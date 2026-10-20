"""Differential tests for the park file reader CSaveLoad::Load (0x10157fec) and the most-recent-save loader
CFSLoadSave::LoadMostRecent (0x101fcdcc), src/SaveLoad.cpp and src/FS_LoadSave.cpp (unit w108), against
libtpw_unit_SaveLoad.dylib (tests/unit_SaveLoad.cpp's fakes; see calllog.py).

Both functions run as the original on the PowerPC side and as the translation on the host; LoadMostRecent calls the real Load on both
sides, so a case of it also runs a read of the file it picks. A case is one call. Everything the two functions call is a fake on both
sides that records the same event (name, integers, texts) and answers from the case's queues:

  - the assertion, CTRACE (its text is formatted) and DEBUG_MESSAGE;
  - the file classes: the disc file's and the memory file's constructors, Open, Close, Seek, GetSize and destructors, and the access
    objects behind them (Read and Write record their arguments, fill what they are given, and answer the queue or all the bytes
    asked for); a file whose Open succeeds gets an access object unless the case says "disc impl" or "mem impl" is 0;
  - the temporary allocator (its constructor, Allocate and Free; the object exists before the call or is made by it), the zlib
    size and unpack calls, and CSaveLoad::LoadHeader, LoadModules, PreLoadOperations and PostLoadOperations (stand-ins in the game);
  - for LoadMostRecent: the user info's constructor and ReturnUsersPathWithTheme, UniStringToString, FS_IsSubDirectory,
    LbFile_CompareFileStamps (answers the queue "CompareFileStamps") and the disc storage with its listing (Open, FindFirst and
    FindNext over the entries of the case, each with the text before the name in its buffer, so that the name pointer is not at the
    buffer's start).

The bullfrog strings are functional and unrecorded on both sides. What the cases compare: the answer, the error word, the event log,
the heap traffic (blocks handled by operator new, new[], delete and delete[]), and the allocator and user info pointers afterwards
(by label). The file's first word is placed as the file has it (little endian bytes): the PowerPC side reads it as a big endian word
and swaps it, the host reads it as it is.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_SaveLoad"

LOAD, LOAD_MOST_RECENT = 0x10157FEC, 0x101FCDCC
A = dict(gSaveLoad=0x1037FEFC, gUserInfo=0x10380B34, gMemoryTemporary=None, gOpenTestWord=0x102A6CB8,
         gTPGlobals=0x103B74C8)
# (gMemoryTemporary is a data import of the main app: the emulator gives it a cell of its own, found in setup)
GLOBAL_STRINGS = dict(mSeparator=0x404C, mAllFiles=0x40D8, mSaveFilePattern=0x445C)   # gTPGlobals offsets
LABEL_SAVELOAD, LABEL_HEADER, LABEL_USERINFO, LABEL_ALLOCATOR = 1, 2, 3, 4
CAP = 4096          # the most bytes a fake buffer holds (and a fake read fills), as in the unit file
PRIOR_ERROR = 0x5A5A

VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64

# ======================================================================= PowerPC strings


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def wide_read(e, a, limit=1000):
    out = []
    while a and len(out) < limit:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return "".join(out)


def s_read(e, obj, wide):
    data = e.u32(obj + 8)
    if not data:
        return ""
    return wide_read(e, data) if wide else e.cstr(data, limit=1000)


def s_write(e, obj, text, wide):
    """A TbDynamicStringTemplate in the original's layout {vtable, capacity, data, length, grow-by}."""
    if wide:
        blob = b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0"
    else:
        blob = text.encode("latin-1") + b"\0"
    data = e.alloc(len(blob), blob, alignment=4)
    e.write(obj, struct.pack(">IIIII", CL.FAKE_VT, 0, data, len(text), 0))


def string_stubs(S):
    """The bullfrog string imports, narrow (c) and wide (w): functional, unrecorded."""
    for t, wide in (("c", False), ("w", True)):
        def cstr(e, a, wide=wide):
            return wide_read(e, a) if wide else e.cstr(a, limit=1000)

        def ct0(e, wide=wide):
            s_write(e, e.r(3), "", wide)

        def ct_text(e, wide=wide, cstr=cstr):
            s_write(e, e.r(3), cstr(e, e.r(4)), wide)

        def ct_copy(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(4), wide), wide)

        def assign(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(4), wide), wide)

        def append(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(3), wide) + s_read(e, e.r(4), wide), wide)

        def append_text(e, wide=wide, cstr=cstr):
            s_write(e, e.r(3), s_read(e, e.r(3), wide) + cstr(e, e.r(4)), wide)

        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>Fv", ct0)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FPC{t}", ct_text)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FRC26TbDynamicStringTemplate<{t}>", ct_copy)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", ct_copy)
        S.imp(f"__dt__26TbDynamicStringTemplate<{t}>Fv", lambda e: None)
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FRC26TbDynamicStringTemplate<{t}>", assign)
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", assign)
        S.imp(f"__apl__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", append)
        S.imp(f"__apl__26TbDynamicStringTemplate<{t}>FPC{t}", append_text)


# ======================================================================= PowerPC side

VT = {}


class SlState:
    """Per-case state of the PowerPC fakes."""

    def __init__(self, c):
        self.version = struct.pack("<I", c["version"])
        self.reads = 0
        self.heap = [0, 0, 0, 0]       # new, new[], delete, delete[]
        self.entries = c.get("entries", [])
        self.next_entry = 0
        self.users_path = c.get("users_path", "")


def first_word(e, a):
    if not a:
        return -1
    b = e.read(a, 4)
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)


def make_vtables(emu):
    """Persistent fake vtables: the access objects (Read at +0xc, Write at +0x10) and the temporary allocator's table
    (Allocate at +0xc, Free at +0x14)."""
    def vtable(slots):
        vt = T.persist_alloc(emu, 0x40)
        for off, fn in slots.items():
            w32(emu, vt + off, T.fake_function(emu, fn))
        return vt

    def read(e):
        L, sl = e.cl, e.sl
        this, dest, size = e.r(3), e.r(4), e.r(5)
        L.rec("Access::Read", (L.lab(this), L.lab(dest), size))
        if dest:
            if sl.reads == 0:
                data = sl.version[:min(size, 4)]
            else:
                data = bytes((i * 7 + 3) & 0xFF for i in range(min(size, CAP)))
            if data:
                e.write(dest, data)
        sl.reads += 1
        e.set_r(3, T.u32(L.ret("Access::Read", T.s32(size))))

    def write(e):
        L = e.cl
        this, source, size = e.r(3), e.r(4), e.r(5)
        L.rec("Access::Write", (L.lab(this), L.lab(source), size, first_word(e, source)))
        e.set_r(3, T.u32(L.ret("Access::Write", T.s32(size))))

    def allocate(e):
        L = e.cl
        this, size = e.r(3), e.r(4)
        L.rec("Allocate", (L.lab(this), size))
        if L.ret("Allocate", 1) == 0:
            e.set_r(3, 0)
            return
        block = e.alloc(min(size, CAP) + 16)
        L.newlab(block)
        e.set_r(3, block)

    def free(e):
        L = e.cl
        this, ref = e.r(3), e.r(4)
        block = e.u32(ref)
        L.rec("Free", (L.lab(this), L.lab(block)))
        L.forget(block)
        w32(e, ref, 0)

    VT["access"] = vtable({0xC: read, 0x10: write})
    VT["allocator"] = vtable({0xC: allocate, 0x14: free})


def new_access(e):
    """An access object (the fake vtable), with a new label."""
    obj = e.alloc(16)
    w32(e, obj, VT["access"])
    e.cl.newlab(obj)
    return obj


def new_allocator(e):
    """The temporary allocator: {word, table pointer}."""
    obj = e.alloc(8)
    w32(e, obj + 4, VT["allocator"])
    return obj


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update({LOAD, LOAD_MOST_RECENT})
    string_stubs(S)

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    def m(sym, name, spec="", ret=None, then=None):
        S.imp(sym, R(name, spec, ret, then))

    # ---- debug output
    S.internal(0x1000DDE8, lambda e: e.cl.rec("GAME_ASSERTMSG", (e.r(3) & 0xFF,), (e.cstr(e.r(4))[:159],)))
    S.internal(0x1006FCBC, lambda e: e.cl.rec("CTRACE", (e.r(3),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],)))
    S.internal(0x1006FC90, lambda e: e.cl.rec("DEBUG_MESSAGE", (e.r(4),), (e.cstr(e.r(3))[:159],)))

    # ---- the files
    def file_ct(e):
        obj = e.r(3)
        e.cl.rec("CFile::CFile", (e.cl.newlab(obj),))
        e.write(obj, struct.pack(">II", CL.FAKE_VT, 0))

    def file_dt(e):
        e.cl.rec("CFile::~CFile", (e.cl.lab(e.r(3)),))

    S.imp("__ct__Q33bfl10FileSystem5CFileFv", file_ct)
    S.imp("__ct__Q33bfl10FileSystem9CDiscFileFv", file_ct)        # (the bullfrog constructor runs the CFile's)
    S.imp("__dt__Q33bfl10FileSystem5CFileFv", file_dt)
    S.imp("__dt__Q33bfl10FileSystem9CDiscFileFv", file_dt)
    m("Close__Q33bfl10FileSystem5CFileFv", "CFile::Close", "L")
    m("Seek__Q33bfl10FileSystem5CFileFl14TbFileSeekMode", "CFile::Seek", "Luu", "i")
    m("GetSize__Q33bfl10FileSystem5CFileFv", "CFile::GetSize", "L", "i")

    def opener(name, key):
        def stub(e):
            L = e.cl
            result_buffer, this = e.r(3), e.r(4)
            if name == "CDiscFile::Open":
                L.rec(name, (L.lab(this), e.r(6)), (e.cstr(e.r(5))[:159],))
            else:
                L.rec(name, (L.lab(this), e.r(5)))
            result = L.ret(name)
            w32(e, result_buffer, result)
            if T.s32(T.u32(result)) >= 0 and L.ret(key, 1) != 0:
                w32(e, this + 4, new_access(e))
        return stub

    S.imp("Open__Q33bfl10FileSystem9CDiscFileFPCcUi", opener("CDiscFile::Open", "disc impl"))
    S.imp("Open__Q33bfl10FileSystem8CMemFileFUi", opener("CMemFile::Open", "mem impl"))

    # ---- the zlib calls
    def zlib_size(e):
        L = e.cl
        L.rec("LbZLib_MemDecompressedSize", (L.lab(e.r(3)), first_word(e, e.r(3))))
        e.set_r(3, T.u32(L.ret("LbZLib_MemDecompressedSize", 64)))

    def zlib_unpack(e):
        L = e.cl
        L.rec("LbZLib_MemDecompress", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
        if e.r(4):
            e.write(e.r(4), bytes((i * 13 + 0x21) & 0xFF for i in range(16)))
        e.set_r(3, 0)

    S.imp("LbZLib_MemDecompressedSize__FPv", zlib_size)
    S.imp("LbZLib_MemDecompress__FPvPvPv", zlib_unpack)

    # ---- the allocator's constructor and the heap
    def allocator_ct(e):
        obj = e.r(3)
        e.cl.rec("UAllocatorTemporary", (e.cl.newlab(obj),))
        w32(e, obj, 0)
        w32(e, obj + 4, VT["allocator"])

    S.imp("__ct__Q213SamsUtilities19UAllocatorTemporaryFv", allocator_ct)

    def op_new(e):
        e.sl.heap[0] += 1
        e.set_r(3, e.alloc(max(e.r(3), 4)))

    def op_delete(kind):
        def stub(e):
            L = e.cl
            e.sl.heap[kind] += 1
            obj = e.r(3)
            if obj and L.lab(obj) > 0:
                L.rec("delete", (L.lab(obj),))
                L.forget(obj)
        return stub

    S.imp("__nw__FUl", op_new)
    S.imp("__dl__FPv", op_delete(2))
    S.imp("__dla__FPv", op_delete(3))

    # ---- the rest of CSaveLoad
    S.internal(0x10159E50, lambda e: (e.cl.rec("LoadHeader", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), T.s32(e.r(5)), e.cl.lab(e.r(6)))),
                                      e.set_r(3, T.u32(e.cl.ret("LoadHeader", 1)))))
    S.internal(0x101589F0, lambda e: (e.cl.rec("LoadModules", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), T.s32(e.r(5)))),
                                      e.set_r(3, T.u32(e.cl.ret("LoadModules", 1)))))
    i(0x101587E4, "PreLoadOperations", "L")
    i(0x101588C4, "PostLoadOperations", "L")

    # ---- what LoadMostRecent calls
    i(0x1017DE54, "CFSUserInfo", "N")

    def users_path(e):
        e.cl.rec("ReturnUsersPathWithTheme", (e.cl.lab(e.r(4)),))
        s_write(e, e.r(3), e.sl.users_path, True)

    S.internal(0x1017F858, users_path)

    def uni_to_string(e):
        text = wide_read(e, e.u32(e.r(4) + 8))
        text = "".join(ch if ord(ch) <= 0xFF else "?" for ch in text)
        e.cl.rec("UniStringToString", (), (text[:159],))
        s_write(e, e.r(3), text, False)
        e.set_r(3, len(text) + 1)

    S.internal(0x10005670, uni_to_string)

    def is_sub_directory(e):
        info = e.r(3)
        attributes = e.u32(info + 4)
        name = e.cstr(e.u32(info + 0x20), limit=400)
        e.cl.rec("FS_IsSubDirectory", (attributes,), (name[:159],))
        e.set_r(3, 1 if (attributes & 0x10) != 0 and name[:1] != "." else 0)

    S.internal(0x10004764, is_sub_directory)

    def stamps(e):
        a, b = e.r(3), e.r(4)
        e.cl.rec("LbFile_CompareFileStamps", (e.u32(a), e.u32(a + 4), e.u32(b), e.u32(b + 4)))
        e.set_r(3, T.u32(e.cl.ret("CompareFileStamps")))

    S.imp("LbFile_CompareFileStamps__FPC11TbFileStampPC11TbFileStamp", stamps)

    S.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", lambda e: e.cl.rec("CFileStorage", (e.cl.newlab(e.r(3)),)))
    S.imp("__dt__Q33bfl10FileSystem12CFileStorageFv", lambda e: e.cl.rec("~CFileStorage", (e.cl.lab(e.r(3)),)))

    def disc_open(e):
        L = e.cl
        L.rec("CDiscFileStorage::Open", (L.lab(e.r(4)),), (e.cstr(e.r(5), limit=1000)[:159],))
        e.sl.next_entry = 0
        w32(e, e.r(3), L.ret("CDiscFileStorage::Open"))

    def fill_info(e, info, entry):
        e.write(info, struct.pack(">II", entry["size"], entry["attributes"]))
        e.write(info + 8, struct.pack(">6I", *entry["words"]))
        e.write(info + 0x24, (entry["prefix"] + entry["name"]).encode("latin-1") + b"\0")
        w32(e, info + 0x20, info + 0x24 + len(entry["prefix"]))

    def find_first(e):
        L = e.cl
        L.rec("CFileStorage::FindFirst", (L.lab(e.r(4)),), (e.cstr(e.r(5), limit=1000)[:159],))
        result = T.s32(T.u32(L.ret("FindFirst")))
        if result < 0 or not e.sl.entries:
            w32(e, e.r(3), result if result < 0 else -1)
            return
        fill_info(e, e.r(6), e.sl.entries[0])
        e.sl.next_entry = 1
        w32(e, e.r(3), result)

    def find_next(e):
        L = e.cl
        L.rec("CFileStorage::FindNext", (L.lab(e.r(4)),))
        result = T.s32(T.u32(L.ret("FindNext")))
        if result < 0:
            w32(e, e.r(3), result)
            return
        if e.sl.next_entry >= len(e.sl.entries):
            w32(e, e.r(3), -1)
            return
        fill_info(e, e.r(5), e.sl.entries[e.sl.next_entry])
        e.sl.next_entry += 1
        w32(e, e.r(3), result)

    S.imp("Open__Q33bfl10FileSystem16CDiscFileStorageFPCc", disc_open)
    S.imp("FindFirst__Q33bfl10FileSystem12CFileStorageFPCcRQ33bfl10FileSystem9CFileInfo", find_first)
    S.imp("FindNext__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem9CFileInfo", find_next)
    return S


def ppc_begin(e, c):
    """The world of a case on the PowerPC side; returns the log and the state."""
    e.sl = SlState(c)
    fixed = {A["gSaveLoad"]: LABEL_SAVELOAD}
    header = 0
    if c.get("header"):
        header = e.alloc(0x3D4)
        fixed[header] = LABEL_HEADER
    L = e.cl = CL.PpcLog(fixed, c["q"])
    w32(e, A["gSaveLoad"] + 4, c["error0"])
    w32(e, A["gOpenTestWord"], c["open_word"])
    if c["allocator"]:
        obj = new_allocator(e)
        L.labels[obj] = LABEL_ALLOCATOR
        w32(e, A["gMemoryTemporary"], obj)
    else:
        w32(e, A["gMemoryTemporary"], 0)
    if c.get("user_info"):
        obj = e.alloc(0x20)
        L.labels[obj] = LABEL_USERINFO
        w32(e, A["gUserInfo"], obj)
    else:
        w32(e, A["gUserInfo"], 0)
    for name, off in GLOBAL_STRINGS.items():
        s_write(e, A["gTPGlobals"] + off, c.get("globals", {}).get(name, ""), False)
    return header


def ppc_observe(e, res):
    L = e.cl
    return dict(result=res, error=T.s32(e.u32(A["gSaveLoad"] + 4)), allocator=L.lab(e.u32(A["gMemoryTemporary"])),
                user_info=L.lab(e.u32(A["gUserInfo"])), heap=list(e.sl.heap), log=L.log)


STUBS = None


def setup(runner):
    global STUBS
    A["gMemoryTemporary"] = runner.emu.data_imports["gMemoryTemporary__13SamsUtilities"]
    T.install_std_stubs(runner.emu)
    make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


def ppc_run(call):
    def run(e, c):
        saved = STUBS.activate(e)
        try:
            header = ppc_begin(e, c)
            res = call(e, c, header)
            out = ppc_observe(e, res)
        finally:
            w32(e, A["gOpenTestWord"], 0)       # the debug word and the globals go back as the program has them
            w32(e, A["gMemoryTemporary"], 0)
            w32(e, A["gUserInfo"], 0)
            CL.Stubs.deactivate(e, saved)
        return out
    return run


def ppc_load(e, c, header):
    path = CL.new_text(e, c["path"])
    return T.s32(T.call_ppc(e, LOAD, [A["gSaveLoad"], path, header, T.u32(c["type"])])[0])


def ppc_most_recent(e, c, header):
    T.call_ppc(e, LOAD_MOST_RECENT, [0])
    return None

# ======================================================================= host side


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def host_run(call):
    def run(lib, c):
        addr = lambda n: F(lib, "tpw_sl_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
        version = (C.c_uint8 * 4)(*struct.pack("<I", c["version"]))
        F(lib, "tpw_sl_reset", None, VP)(version)
        entry = F(lib, "tpw_sl_entry", None, U32, U32, VP, C.c_char_p, C.c_char_p)
        for item in c.get("entries", []):
            entry(item["attributes"], item["size"], (U32 * 6)(*item["words"]), item["prefix"].encode("latin-1"),
                  item["name"].encode("latin-1"))
        units = [ord(ch) for ch in c.get("users_path", "")] + [0]
        F(lib, "tpw_sl_set_users_path", None, VP)((C.c_uint16 * len(units))(*units))
        for name in GLOBAL_STRINGS:
            F(lib, "tpw_sl_set_text", None, C.c_char_p, C.c_char_p)(name.encode(), c.get("globals", {}).get(name, "").encode("latin-1"))
        F(lib, "tpw_sl_set_error", None, I32)(c["error0"])
        C.c_uint32.from_address(addr("gOpenTestWord")).value = c["open_word"]
        fixed = {LABEL_SAVELOAD: addr("gSaveLoad")}
        header = 0
        if c.get("header"):
            header = addr("gHeader")
            fixed[LABEL_HEADER] = header
        if c["allocator"]:
            fixed[LABEL_ALLOCATOR] = F(lib, "tpw_sl_make_allocator", VP)()
        if c.get("user_info"):
            fixed[LABEL_USERINFO] = F(lib, "tpw_sl_make_user_info", VP)()
        path = C.create_string_buffer(c["path"].encode("latin-1"))
        CL.host_begin(lib, fixed, c["q"])
        heap = (I32 * 4).in_dll(lib, "tpw_cl_heap")
        for k in range(4):
            heap[k] = 0
        res = call(lib, c, header, path)
        log = CL.host_end(lib)
        lab = F(lib, "tpw_cl_lab", I32, VP)
        out = dict(result=res, error=F(lib, "tpw_sl_error", I32)(),
                   allocator=lab(C.c_void_p.from_address(addr("gMemoryTemporary")).value),
                   user_info=lab(C.c_void_p.from_address(addr("gUserInfo")).value), heap=list(heap), log=log)
        C.c_uint32.from_address(addr("gOpenTestWord")).value = 0
        return out
    return run


def host_load(lib, c, header, path):
    return F(lib, "tpw_sl_load", I32, VP, VP, I32)(C.addressof(path), header, c["type"])


def host_most_recent(lib, c, header, path):
    F(lib, "tpw_sl_load_most_recent", None)()
    return None

# ======================================================================= running a case


class SlTest(FnTest):
    def run_case(self, runner, case):
        mism = super().run_case(runner, case)
        if mism and any(m.startswith("log") for m in mism) and getattr(self, "_last", None):
            p, h = self._last
            return [f"log: {CL.first_difference(p['log'], h['log'])}"] + [m for m in mism if not m.startswith("log")][:3]
        return mism


def sl_test(name, ppc_call, host_call, edge, rnd):
    t = SlTest(name, None, None, edge=edge, rnd=rnd)
    ppc, host = ppc_run(ppc_call), host_run(host_call)

    def p(e, c):
        t._p = ppc(e, c)
        return t._p

    def h(lib, c):
        out = host(lib, c)
        t._last = (t._p, out)
        return out

    t.ppc, t.host = p, h
    return t

# ======================================================================= cases

PATHS = ["HD:Games:TPW:save:users:1Tess:Land:park.TPW", "a", "x.TPWS", "/Users/p/park.TPWSAVE", "", "dir:file name.TPW",
         "C:\\Games\\TPW\\Park 1.TPW"]
VERSIONS = [0, 1, 100, 499, 500, 501, 502, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x1F4, 0xF4010000, 0x000001F4, 0x01000000]
OPEN_FAIL = [-1, -1, -2, -0x80000000]


def pick_path(r):
    if r.random() < 0.8:
        return r.choice(PATHS)
    return "".join(r.choice("abcXYZ:./ 09") for _ in range(r.choice([0, 3, 40, 200, 300])))


def base_load(r, **extra):
    """A case that reads the whole file and loads (type 2), before the deviations of a stage."""
    # the file's size and the position after the header: what is left is their difference (it wraps, and may be negative)
    size = r.choice([(100, 4), (4097, 4), (100000, 4), (9, 4), (5, 4), (100, 4), (100, 4), (3, 4), (0, 4), (0x7FFFFFFF, 0),
                     (4, 0x7FFFFFFF), (0, -1), (0x7FFFFFFF, -0x80000000), (-1, 0), (-0x80000000, 4)])
    c = dict(path=pick_path(r), type=2, header=r.random() < 0.7, open_word=0, version=r.choice([1, 100, 499, 500]),
             allocator=r.random() < 0.5, error0=r.choice([0, 0, PRIOR_ERROR, 5]), q={})
    q = c["q"]
    q["CFile::GetSize"] = [size[0]]
    q["CFile::Seek"] = [size[1], r.choice([0, -1, 7])]
    q["LbZLib_MemDecompressedSize"] = [r.choice([1, 16, 64, 5000, 100000])]
    c.update(extra)
    return c


def stage_case(r, stage, type_=None):
    c = base_load(r)
    q = c["q"]
    if type_ is not None:
        c["type"] = type_
    elif r.random() < 0.2:
        c["type"] = r.choice([0, 1, 1, 3, -1])
    if c["type"] == 0:
        c["header"] = r.random() < 0.8       # type 0 asserts when there is no header
    if stage == 0:                            # the assertion: header-only without a header
        c["type"], c["header"] = 0, False
    elif stage == 1:                          # the file does not open
        q["CDiscFile::Open"] = [r.choice(OPEN_FAIL)]
        c["open_word"] = 0
    elif stage == 2:                          # the file opens but the debug word turns the test round
        c["open_word"] = r.choice([1, 2, 0x80000000])
    elif stage == 3:                          # the file does not open and the debug word lets it go on
        q["CDiscFile::Open"] = [r.choice(OPEN_FAIL)]
        c["open_word"] = r.choice([1, 0xFFFFFFFF])
    elif stage == 4:                          # the first word cannot be read
        if r.random() < 0.4:
            q["disc impl"] = [0]
        else:
            q["Access::Read"] = [r.choice([0, 1, 3, 5, -1, 0x80000004])]
    elif stage == 5:                          # a future version
        c["type"] = r.choice([2, 2, 2, 1, 0]) if type_ is None else type_
        c["version"] = r.choice([501, 502, 0x7FFFFFFF, 1000, 0x10000])
        if c["type"] == 0:
            c["header"] = True
    elif stage == 6:                          # the version on the line
        c["version"] = r.choice([499, 500, 501, 0x80000000, 0xFFFFFFFF, 0x00000000, 0xF4010000])
    elif stage == 7:                          # the header cannot be read
        q["LoadHeader"] = [r.choice([0, 0, 0])]
    elif stage == 8:                          # a header-only load that works
        c["type"], c["header"] = 0, True
        q["LoadHeader"] = [r.choice([1, 2, -1, 0x100])]
    elif stage == 9:                          # nothing left in the file
        q["CFile::GetSize"] = [r.choice([0, 4, 100])]
        q["CFile::Seek"] = [q["CFile::GetSize"][0], 0]
    elif stage == 10:                         # the packed bytes are not all read
        q["Access::Read"] = [4, r.choice([0, 1, -1, 99, 100000, 0x7FFFFFFF])]
    elif stage == 11:                         # not a zlib block
        q["LbZLib_MemDecompressedSize"] = [0]
    elif stage == 12:                         # the allocator refuses the unpacked buffer, or the packed one
        q["Allocate"] = r.choice([[1, 0], [1, 0], [0, 1], [0, 0]])
    elif stage == 13:                         # the memory file does not open
        q["CMemFile::Open"] = [r.choice([-1, -2, -0x80000000])]
    elif stage == 14:                         # the memory file refuses the bytes
        if r.random() < 0.4:
            q["mem impl"] = [0]
        else:
            q["Access::Write"] = [r.choice([-1, -7, -0x80000000])]
    elif stage == 15:                         # LoadModules fails
        q["LoadModules"] = [r.choice([0])]
    else:                                     # it loads
        q["LoadModules"] = [r.choice([1, 2, -1, 0x100, 0x80000000])]
        q["Access::Write"] = [r.choice([0, 1, 5, 64])] if r.random() < 0.3 else []
        q["CMemFile::Open"] = [r.choice([0, 1, 5])] if r.random() < 0.3 else []
    if r.random() < 0.15:
        q["CDiscFile::Open"] = q.get("CDiscFile::Open", [r.choice([0, 1, 7])])
    if r.random() < 0.1 and stage >= 7:
        q["LoadHeader"] = q.get("LoadHeader", [r.choice([1, 3])])
    c["q"] = {k: v for k, v in q.items() if v}
    return c


STAGES = 17


def edge_load():
    r = random.Random(11)
    cases = []
    for stage in range(STAGES):
        for type_ in (0, 1, 2):
            cases.append(stage_case(r, stage, type_))
    # version boundaries under every type
    for version in (499, 500, 501, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0):
        for type_ in (0, 1, 2):
            c = stage_case(r, 16, type_)
            c["version"] = version
            c["header"] = True
            cases.append(c)
    # the open test word against opening and failing, and a missing access object under each type
    for word in (0, 1):
        for fail in (False, True):
            c = stage_case(r, 16, 2)
            c["open_word"] = word
            c["q"]["CDiscFile::Open"] = [-1 if fail else 0]
            cases.append(c)
    return cases


def rnd_load(r):
    return stage_case(r, r.randrange(STAGES))


TESTS = []
TESTS.append(sl_test("CSaveLoad::Load", ppc_load, host_load, edge_load, rnd_load))

# ---- LoadMostRecent

NAMES = ["a.TPW", "park1.TPW", "Zoo.TPWS", "x", "park.TPWSAVE", ".hidden.TPW", "sub", "A long name with spaces.TPW", "b.TPW"]
PREFIXES = ["", "", "", "", "x:", "folder:sub:", "*."]
GLOBALS_CHOICES = dict(mAllFiles=["*", "*", "*.*", ""], mSaveFilePattern=[".TPW*", ".TPW*", ".TPWS", ""],
                       mSeparator=[":", ":", "/", "::", ""])
USER_PATHS = ["HD:TPW:save:users:1Tess:Land", "x", "", "HD:Games:Theme Park World:save:users:2Zoe:Jungle",
              "a" * 150 + ":b"]


def entry(r, **extra):
    name = r.choice(NAMES)
    if r.random() < 0.1:
        name = "n" * r.choice([100, 200, 255])
    prefix = r.choice(PREFIXES)
    name = name[:259 - len(prefix)]
    stamp = r.choice([(0, 0), (1, 0), (0, 1), (0xFFFFFFFF, 0), (0, 0xFFFFFFFF), (r.getrandbits(32), r.getrandbits(32))])
    e = dict(attributes=r.choice([0, 0x20, 0x20, 0x10, 0x11, 0x30, 0xFFFFFFEF, 0xFFFFFFFF]), size=r.getrandbits(32),
             words=list(stamp) + [r.getrandbits(32) for _ in range(4)], prefix=prefix, name=name)
    e.update(extra)
    return e


def most_recent_case(r, scenario=None):
    scenario = r.randrange(7) if scenario is None else scenario
    load = stage_case(r, r.choice([1, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 16, 16]), 2)
    c = dict(load)
    c["globals"] = {k: r.choice(v) for k, v in GLOBALS_CHOICES.items()}
    c["users_path"] = r.choice(USER_PATHS)
    c["user_info"] = r.random() < 0.5
    q = c["q"]
    count = r.choice([0, 1, 1, 2, 3, 4, 6, 10])
    c["entries"] = [entry(r) for _ in range(count)]
    q["CompareFileStamps"] = [r.choice([1, 1, 0, -1, 5, -7, 0x7FFFFFFF, -0x80000000]) for _ in range(max(count, 1))]
    if scenario == 0:                          # the folder does not open
        q["CDiscFileStorage::Open"] = [r.choice([-1, -2])]
    elif scenario == 1:                        # nothing is found
        q["FindFirst"] = [r.choice([-1, -5, -0x80000000])]
    elif scenario == 2:                        # only folders
        c["entries"] = [entry(r, attributes=r.choice([0x10, 0x11, 0x30, 0xFFFFFFFF]), name=r.choice(["sub", "a", "folder"]))
                        for _ in range(r.choice([1, 2, 3]))]
    elif scenario == 3:                        # names and prefixes
        c["entries"] = [entry(r, attributes=0x20, prefix=r.choice(PREFIXES)) for _ in range(r.choice([1, 2, 3, 5]))]
    elif scenario == 4:                        # equal stamps, the first stays
        stamp = [r.getrandbits(32), r.getrandbits(32)]
        c["entries"] = [entry(r, attributes=0x20, words=stamp + [r.getrandbits(32) for _ in range(4)])
                        for _ in range(r.choice([2, 3, 4]))]
        q["CompareFileStamps"] = [0]
    elif scenario == 5:                        # a folder in between files
        c["entries"] = [entry(r, attributes=0x20), entry(r, attributes=0x10, name="dir"), entry(r, attributes=0x20),
                        entry(r, attributes=0x10, name=".git"), entry(r, attributes=0x20, name=".dot")]
    if r.random() < 0.15:
        q["FindFirst"] = q.get("FindFirst", [r.choice([0, 1, 7])])
    if r.random() < 0.4:                       # FindNext answers: successes that are not 0, and the listing cut short
        q["FindNext"] = [r.choice([0, 0, 1, 5, 0x7FFFFFFF, -1, -9]) for _ in range(r.choice([1, 2, 3, 6]))]
    q["CompareFileStamps"] = q["CompareFileStamps"][:] or [1]
    return c


def edge_most_recent():
    r = random.Random(23)
    return [most_recent_case(r, k % 7) for k in range(42)]


def rnd_most_recent(r):
    return most_recent_case(r)


TESTS.append(sl_test("CFSLoadSave::LoadMostRecent", ppc_most_recent, host_most_recent, edge_most_recent, rnd_most_recent))
