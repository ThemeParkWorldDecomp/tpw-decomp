"""Differential tests for FS_Registry.cpp, FS_System.cpp and the string helpers
(src/game/save/FS_Registry.cpp, src/game/save/FS_System.cpp, src/game/ui/3DFont.cpp; Milestone 1 unit 5, part 1),
against libtpw_unit_FSSystem.dylib (tests/unit_FSSystem.cpp's fakes; see calllog.py).

The bullfrog strings are functional and unrecorded on both sides: the PowerPC side's
string imports and the vtable slots the game calls them through (0x18 and 0x1c assign a
string, 0x20 a C string, 0x28 appends) are Python fakes over the original's layout
{vtable, capacity, data, length, grow-by}. Files, storages, resources and the Toolbox and
macdoze calls record events; CFile's vtable points its Read and Write slots at the main
app's own copies (0x100042dc, 0x10004550), as the library's vtable does.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_FSSystem"

F_ = {
    "TPConvertTPLangDefs": 0x100038B0, "FS_ReadRegistry": 0x10003A2C, "FS_ReadLanguageRegistry": 0x10003B64,
    "TPFile_Open": 0x10003FD4, "TPFile_Read": 0x10004260, "CFile::Read": 0x100042DC, "TPFile_Close": 0x10004354,
    "TPFile_Seek": 0x100043EC, "TPFile_Size": 0x100044E8, "CFile::Write": 0x10004550,
    "FS_FindFilePath": 0x100045C8, "FS_IsSubDirectory": 0x10004764, "FS_DelTree": 0x100047D8,
    "CFileInfo::~CFileInfo": 0x10004994, "CDiscFileStorage::~CDiscFileStorage": 0x10004A10,
    "FS_IsWadFile": 0x10004AB8, "FS_AddDirectorySeparator": 0x10004BC0,
    "CSYS_TPFileInfo::SetLocalPath": 0x10004CCC, "CSYS_TPFileInfo::GetLocalPath": 0x10004D4C,
    "CSYS_TPFileInfo::SetRemotePath": 0x10004D88, "CSYS_TPFileInfo::GetRemotePath": 0x10004E08,
    "CSYS_TPFileInfo::SetDataPath": 0x10004E48, "CSYS_TPFileInfo::GetDataPath": 0x10004EC4,
    "CSYS_TPFileInfo::SetLevelPath": 0x10004F00, "CSYS_TPFileInfo::GetLevelPath": 0x10005050,
    "CSYS_TPFileInfo::SetLanguagePath": 0x10005090,
    "TPStrGtr": 0x10005354, "TPStrEq": 0x100053F4, "TPUniStrEq": 0x1000548C, "RemoveExtension": 0x10005524,
    "UniStringToString": 0x10005670, "StringToUniString": 0x100057A0}
SINIT_REGISTRY, SINIT_SYSTEM = 0x10003D28, 0x100051E0
A = dict(gTPFileInfo=0x102B3458, gGame=0x1037F134, gTPGlobals=0x103B74C8, gText=0x1037FF18,
         gLanguageName=0x102B2E18, gTPDialogLanguageBits=0x103BC660, gFSRegistryLanguages=0x102B3258,
         gFSRegistryTitle=0x102B3238, gFSRegistryLanguage=0x102B3218)
PATHS = ["mLocalPath", "mRemotePath", "mDataPath", "mLevelPath", "mLanguagePath"]   # gTPFileInfo + 0x14 * k
GLOBAL_STRINGS = dict(mSeparator=0x404C, mAllFiles=0x40D8, mWadExtension=0x4394, mDataFolder=0x4830,
                      mLanguageFolder=0x4934, mDefaultLanguagePath=0x4B14)          # gTPGlobals offsets
LANGUAGES = ["English", "French", "German", "Spanish", "Swedish", "Italian", "Dutch", "Polish", "Danish",
             "Japanese", "TraditionalChinese", "SimplifiedChinese", "Thai", "Korean", "Hebrew", "American"]
LABELS = dict(gTPFileInfo=10, gGame=2, manager=3, resource=4, storage=5, resource_path=6, gText=7, file=20,
              implementation=21, info=30, disc=31, arg0=40, arg1=41, dest=50, source=51)
CDISC_VT = 0x10278BF0     # the main app's CDiscFileStorage vtable
VOID = {"FS_DelTree", "FS_AddDirectorySeparator", "CSYS_TPFileInfo::SetLocalPath", "CSYS_TPFileInfo::SetRemotePath",
        "CSYS_TPFileInfo::SetDataPath", "CSYS_TPFileInfo::SetLevelPath", "CSYS_TPFileInfo::SetLanguagePath"}
# results that are pointers (compared by label) and TbErrors (returned in a hidden buffer)
LABEL_RESULT = {"TPFile_Open", "CSYS_TPFileInfo::GetLocalPath", "CSYS_TPFileInfo::GetRemotePath",
                "CSYS_TPFileInfo::GetDataPath", "CSYS_TPFileInfo::GetLevelPath", "RemoveExtension",
                "CFileInfo::~CFileInfo", "CDiscFileStorage::~CDiscFileStorage"}
TBERROR_RESULT = {"FS_ReadRegistry", "FS_ReadLanguageRegistry"}
BYTE_RESULT = {"FS_FindFilePath", "FS_IsSubDirectory", "FS_IsWadFile", "TPStrGtr", "TPStrEq", "TPUniStrEq"}

VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f

# ======================================================================= PowerPC strings


VT = {}


def wide_read(e, a, limit=400):
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
    return wide_read(e, data) if wide else e.cstr(data, limit=400)


def s_write(e, obj, text, wide, full=True):
    if wide:
        blob = b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0"
    else:
        blob = text.encode("latin-1") + b"\0"
    data = e.alloc(len(blob), blob, alignment=4)
    fields = struct.pack(">IIII", VT["w" if wide else "c"], 0, data, len(text))
    e.write(obj, fields + (b"\0\0\0\0" if full else b""))


def new_string(e, text, wide, label=None):
    obj = e.alloc(0x14)
    s_write(e, obj, text, wide)
    if label:
        e.cl.labels[obj] = label
    return obj


def right(text, n):
    return text if n >= len(text) else text[len(text) - n:]


def left(text, n):
    return text if n >= len(text) else text[:n]


def lower(c):
    return c + 32 if 0x41 <= c <= 0x5A else c


def icmp(a, b):
    for x, y in zip(a + "\0", b + "\0"):
        x, y = lower(ord(x)), lower(ord(y))
        if x != y:
            return -1 if x < y else 1
        if x == 0:
            return 0
    return 0


def narrow(text):
    return "".join(c if ord(c) < 0x80 else "?" for c in text)[:159]


def string_stubs(S):
    """The bullfrog string imports, narrow (c) and wide (w)."""
    for k, wide in (("c", False), ("w", True)):
        def cstr(e, a, wide=wide):
            return wide_read(e, a) if wide else e.cstr(a, limit=400)

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

        def length(e):
            e.set_r(3, e.u32(e.r(3) + 0xC))

        def right_(e, wide=wide):
            s_write(e, e.r(3), right(s_read(e, e.r(4), wide), e.r(5)), wide, full=False)

        def left_(e, wide=wide):
            s_write(e, e.r(3), left(s_read(e, e.r(4), wide), e.r(5)), wide, full=False)

        t = "w" if wide else "c"
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>Fv", ct0)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FPC{t}", ct_text)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FRC26TbDynamicStringTemplate<{t}>", ct_copy)
        S.imp(f"__ct__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", ct_copy)
        S.imp(f"__dt__26TbDynamicStringTemplate<{t}>Fv", lambda e: None)
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FRC26TbDynamicStringTemplate<{t}>", assign)
        S.imp(f"__as__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", assign)
        S.imp(f"__apl__26TbDynamicStringTemplate<{t}>FRC15TbStringBase<{t}>", append)
        S.imp(f"__apl__26TbDynamicStringTemplate<{t}>FPC{t}", append_text)
        S.imp(f"Length__15TbStringBase<{t}>CFv", length)
        S.imp(f"Right__15TbStringBase<{t}>CFUi", right_)
        S.imp(f"Left__15TbStringBase<{t}>CFUi", left_)


def make_vtables(emu):
    """Persistent fake vtables: strings (narrow, wide), files, file implementations,
    storages and resources. Each slot is a TVector."""
    def vtable(slots):
        vt = T.persist_alloc(emu, 0x40)
        for off, target in slots.items():
            tv = T.fake_function(emu, target) if callable(target) else T.persist_alloc(
                emu, 8, struct.pack(">II", target, emu.toc), alignment=8)
            w32(emu, vt + off, tv)
        return vt

    for k, wide in (("c", False), ("w", True)):
        def assign(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(4), wide), wide)

        def assign_text(e, wide=wide):
            s_write(e, e.r(3), wide_read(e, e.r(4)) if wide else e.cstr(e.r(4), limit=400), wide)

        def append(e, wide=wide):
            s_write(e, e.r(3), s_read(e, e.r(3), wide) + s_read(e, e.r(4), wide), wide)

        def delete(e):
            # the deleting destructor: as the host's operator delete, labelled strings only
            L, obj = e.cl, e.r(3)
            if T.s16(e.r(4)) > 0 and obj and L.lab(obj) > 0:
                L.rec("delete", (L.lab(obj),))
                L.forget(obj)

        VT[k] = vtable({0x14: delete, 0x18: assign, 0x1C: assign, 0x20: assign_text, 0x28: append})

    def file_dtor(e):
        L, obj, flag = e.cl, e.r(3), T.s16(e.r(4))
        L.rec("CFile::~CFile", (L.lab(obj),))
        if flag > 0 and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    def rec(name, spec, result=None):
        return CL.recorder(name, spec, result)

    VT["file"] = vtable({8: file_dtor, 0xC: rec("CFile::Close", "L"), 0x10: 0x100042DC, 0x14: 0x10004550,
                         0x18: rec("CFile::Seek", "Luu", "i"), 0x1C: rec("CFile::Size", "L", "i")})
    VT["impl"] = vtable({0xC: rec("CFileImplementation::Read", "LLu", "i"),
                         0x10: rec("CFileImplementation::Write", "LLu", "i")})

    def storage_open(e):
        L = e.cl
        L.rec("CFileStorage::OpenFile", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(7), L.lab(e.r(8))),
              (e.cstr(e.r(6))[:159],))
        w32(e, e.r(3), L.ret("CFileStorage::OpenFile"))

    def storage_full(e):
        L = e.cl
        name = s_read(e, e.r(6), False)
        L.rec("CFileStorage::FullPath", (L.lab(e.r(4)), L.lab(e.r(5))), (name[:159],))
        s_write(e, e.r(5), "full:" + name, False)
        w32(e, e.r(3), L.ret("CFileStorage::FullPath"))

    VT["storage"] = vtable({0x14: storage_open, 0x1C: storage_full})

    def resource_open(e):
        L = e.cl
        L.rec("CTPBaseResource::OpenFile", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(6)), (s_read(e, e.r(5), False)[:159],))
        e.set_r(3, T.u32(L.ret("CTPBaseResource::OpenFile")))

    VT["resource"] = vtable({0xC: resource_open})

# ======================================================================= PowerPC side


class FsState:
    pass


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(F_.values())
    S.real.update({SINIT_REGISTRY, SINIT_SYSTEM})
    string_stubs(S)

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    def m(sym, name, spec="", ret=None, then=None):
        S.imp(sym, R(name, spec, ret, then))

    st = lambda e: e.fs   # noqa: E731

    i(0x1015EA0C, "CText::SetLanguageId", "Lu")
    i(0x10165538, "CTPResourceManager::pGetCurrentResource", "L", ret=lambda e, L: st(e).current)
    i(0x1015FE40, "CTPBaseResource::pGetLastFoundFileStorage", "L", ret=lambda e, L: st(e).last_storage)
    S.internal(0x10165344, lambda e: (e.cl.rec("CTPResourceManager::OpenFile", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))),
                                               (s_read(e, e.r(5), False)[:159],)),
                                      e.set_r(3, T.u32(e.cl.ret("CTPResourceManager::OpenFile")))))

    def qualify(e):
        L = e.cl
        name, base = s_read(e, e.r(3), False), s_read(e, e.r(4), False)
        L.rec("QualifyFilename", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))), (name[:159], base[:159]))
        s_write(e, e.r(5), base + "|" + name, False)
        e.set_r(3, T.u32(L.ret("QualifyFilename", 1)))

    S.internal(0x10160078, qualify)

    def project_dir(e):
        e.cl.rec("LbFile_GetProjectDir")
        e.set_r(3, st(e).project_dir)

    S.imp("LbFile_GetProjectDir__Fv", project_dir)

    def tb_result(name, spec_ints, texts=()):
        """A TbError-returning import: r3 is the result buffer."""
        def stub(e):
            L = e.cl
            L.rec(name, spec_ints(e), texts(e) if texts else ())
            w32(e, e.r(3), L.ret(name))
        return stub

    S.imp("LbFile_DeleteDirectory__FPCc", tb_result("LbFile_DeleteDirectory", lambda e: (),
                                                     lambda e: (e.cstr(e.r(4))[:159],)))
    S.imp("LbFile_CreateDirectory__FPCc", tb_result("LbFile_CreateDirectory", lambda e: (),
                                                     lambda e: (e.cstr(e.r(4))[:159],)))

    def mem_alloc(e):
        L = e.cl
        size = e.r(3)
        L.rec("LbMem_Alloc", (size,))
        if L.ret("LbMem_Alloc", 1) == 0:
            e.set_r(3, 0)
            return
        block = e.alloc(size + 2)
        L.newlab(block)
        e.set_r(3, block)

    S.imp("LbMem_Alloc__FUi", mem_alloc)
    m("LbMem_Free__FPv", "LbMem_Free", "L")

    def c2pcpy(e):
        text = e.cstr(e.r(4), limit=1000).encode("latin-1")[:255]
        e.write(e.r(3), bytes([len(text)]) + text)

    S.imp("c2pcpy__Q213SamsUtilities7UStringFPUcPCc", c2pcpy)

    def pascal(e, a):
        n = e.read(a, 1)[0]
        return e.read(a + 1, n).decode("latin-1")

    def equal_string(e):
        a, b = pascal(e, e.r(3)), pascal(e, e.r(4))
        cs, ds = e.r(5) & 0xFF, e.r(6) & 0xFF
        e.cl.rec("EqualString", (1 if cs else 0, 1 if ds else 0), (a[:159], b[:159]))
        same = a == b if cs else (len(a) == len(b) and all(lower(ord(x)) == lower(ord(y)) for x, y in zip(a, b)))
        e.set_r(3, 1 if same else 0)

    S.imp("EqualString", equal_string)

    def find_first(e):
        L = e.cl
        L.rec("FindFirstFile", (), (e.cstr(e.r(3))[:159],))
        handle = L.ret("FindFirstFile", 0x5000)
        if handle != -1:
            name = st(e).found_name.encode("latin-1")[:259]
            e.write(e.r(4) + 0x2C, name + b"\0")
        e.set_r(3, T.u32(handle))

    S.imp("FindFirstFile__10NS_MacDozeFPCcPQ210NS_MacDoze17_WIN32_FIND_DATAA", find_first)
    S.imp("FindClose__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle",
          lambda e: (e.cl.rec("FindClose", (T.s32(e.r(3)),)), e.set_r(3, 1)))

    def stricmp(e):
        a, b = e.cstr(e.r(3), limit=400), e.cstr(e.r(4), limit=400)
        e.cl.rec("_stricmp", (), (a[:159], b[:159]))
        e.set_r(3, T.u32(icmp(a, b)))

    def wcsicmp(e):
        a, b = wide_read(e, e.r(3)), wide_read(e, e.r(4))
        e.cl.rec("_wcsicmp", (), (narrow(a), narrow(b)))
        e.set_r(3, T.u32(icmp(a, b)))

    S.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)
    S.imp("_wcsicmp__10NS_MacDozeFPCwPCw", wcsicmp)

    def wide_to_multi(e):
        L = e.cl
        text, out, size = wide_read(e, e.r(5)), e.r(7), T.s32(e.r(8))
        L.rec("WideCharToMultiByte", (e.r(3), e.r(4), T.s32(e.r(6)), L.lab(out), size, L.lab(e.r(9)), L.lab(e.r(10))),
              (narrow(text),))
        data = bytes(ord(c) if ord(c) <= 0xFF else 0x3F for c in text) + b"\0"
        if out:
            e.write(out, data[:max(size, 0)])
        written = len(data) if not out else min(len(data), size)
        e.set_r(3, T.u32(L.ret("WideCharToMultiByte", written)))

    def multi_to_wide(e):
        L = e.cl
        text, out, size = e.cstr(e.r(5), limit=400), e.r(7), T.s32(e.r(8))
        L.rec("MultiByteToWideChar", (e.r(3), e.r(4), T.s32(e.r(6)), L.lab(out), size), (text[:159],))
        units = [ord(c) for c in text] + [0]
        if out:
            n = max(min(len(units), size), 0)
            e.write(out, b"".join(struct.pack(">H", u) for u in units[:n]))
        written = len(units) if not out else min(len(units), size)
        e.set_r(3, T.u32(L.ret("MultiByteToWideChar", written)))

    S.imp("WideCharToMultiByte__10NS_MacDozeFUiUlPCwiPciPCcPi", wide_to_multi)
    S.imp("MultiByteToWideChar__10NS_MacDozeFUiUlPCciPwi", multi_to_wide)

    # ---- bullfrog files and storages
    def file_ct(e):
        obj = e.r(3)
        e.cl.rec("CFile::CFile", (e.cl.newlab(obj),))
        e.write(obj, struct.pack(">II", VT["file"], 0))

    S.imp("__ct__Q33bfl10FileSystem5CFileFv", file_ct)
    S.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", lambda e: e.cl.rec("CFileStorage", (e.cl.newlab(e.r(3)),)))
    S.imp("__dt__Q33bfl10FileSystem12CFileStorageFv", lambda e: e.cl.rec("~CFileStorage", (e.cl.lab(e.r(3)),)))

    def disc_open(e):
        L, obj, path = e.cl, e.r(4), e.cstr(e.r(5), limit=400)
        L.rec("CDiscFileStorage::Open", (L.lab(obj),), (path[:159],))
        st(e).listings[obj] = [path, 0]
        w32(e, e.r(3), 0 if path in st(e).tree else -1)

    def fill_info(e, info, entry):
        name, attributes = entry
        w32(e, info + 4, attributes)
        e.write(e.u32(info + 0x20), name.encode("latin-1") + b"\0")

    def find_first_entry(e):
        L, obj, info = e.cl, e.r(4), e.r(6)
        L.rec("CFileStorage::FindFirst", (L.lab(obj),), (e.cstr(e.r(5))[:159],))
        listing = st(e).listings.setdefault(obj, ["", 0])
        entries = st(e).tree.get(listing[0], [])
        if not entries:
            w32(e, e.r(3), -1)
            return
        fill_info(e, info, entries[0])
        listing[1] = 1
        w32(e, e.r(3), 0)

    def find_next_entry(e):
        L, obj, info = e.cl, e.r(4), e.r(5)
        L.rec("CFileStorage::FindNext", (L.lab(obj),))
        listing = st(e).listings.setdefault(obj, ["", 0])
        entries = st(e).tree.get(listing[0], [])
        if listing[1] >= len(entries):
            w32(e, e.r(3), -1)
            return
        fill_info(e, info, entries[listing[1]])
        listing[1] += 1
        w32(e, e.r(3), 0)

    S.imp("Open__Q33bfl10FileSystem16CDiscFileStorageFPCc", disc_open)
    S.imp("FindFirst__Q33bfl10FileSystem12CFileStorageFPCcRQ33bfl10FileSystem9CFileInfo", find_first_entry)
    S.imp("FindNext__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem9CFileInfo", find_next_entry)

    def storage_delete(e):
        e.cl.rec("CFileStorage::Delete", (e.cl.lab(e.r(4)),), (e.cstr(e.r(5))[:159],))
        w32(e, e.r(3), 0)

    S.imp("Delete__Q33bfl10FileSystem12CFileStorageFPCc", storage_delete)

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__dl__FPv", op_delete)
    return S


def ppc_begin(e, c):
    s = e.fs = FsState()
    e.heap_in_scratch = True
    fixed = {A["gGame"]: LABELS["gGame"], A["gText"]: LABELS["gText"]}
    for k, name in enumerate(PATHS):
        fixed[A["gTPFileInfo"] + 0x14 * k] = LABELS["gTPFileInfo"] + k
    e.cl = CL.PpcLog(fixed, c.get("q", {}))
    L = e.cl
    for k, name in enumerate(PATHS):
        s_write(e, A["gTPFileInfo"] + 0x14 * k, c.get("paths", {}).get(name, ""), False)
    for name, off in GLOBAL_STRINGS.items():
        s_write(e, A["gTPGlobals"] + off, c["globals"][name], False)
    for k, text in enumerate(LANGUAGES):
        s_write(e, A["gFSRegistryLanguages"] + 0x14 * k, text, False)
    e.write(A["gLanguageName"], c.get("language_name", "").encode("latin-1") + b"\0")
    w32(e, A["gTPDialogLanguageBits"], c.get("language_bits", 0))
    s.project_dir = CL.new_text(e, c.get("project_dir", "HD:Games:TPW"))
    s.found_name = c.get("found_name", "")
    s.tree = {path: list(entries) for path, entries in c.get("tree", {}).items()}
    s.listings = {}
    # resources
    s.manager = e.alloc(0x40)
    L.labels[s.manager] = LABELS["manager"]
    resource = e.alloc(0x40)
    L.labels[resource] = LABELS["resource"]
    s_write(e, resource + 0x14, c.get("resource_path", "Data"), False)
    L.labels[resource + 0x14] = LABELS["resource_path"]
    w32(e, resource + 0x28, VT["resource"])
    storage = e.alloc(0x10)
    w32(e, storage, VT["storage"])
    L.labels[storage] = LABELS["storage"]
    s.current = resource if c.get("resource", True) else 0
    s.last_storage = storage if c.get("storage", True) else 0
    w32(e, A["gGame"] + 0x34, s.manager)


def ppc_observe(e, c):
    out = {"paths": [s_read(e, A["gTPFileInfo"] + 0x14 * k, False) for k in range(5)],
           "language_name": e.cstr(A["gLanguageName"], limit=0x400),
           "language_bits": e.u32(A["gTPDialogLanguageBits"])}
    return out

# ======================================================================= host side


class HostState:
    pass


def host_string(lib, h, text, wide, label=None):
    obj = F(lib, "tpw_fs_new_string", VP, I32)(1 if wide else 0)
    if wide:
        units = (C.c_uint16 * (len(text) + 1))(*([ord(ch) for ch in text] + [0]))
        F(lib, "tpw_fs_set_wide", None, VP, VP)(obj, units)
    else:
        F(lib, "tpw_fs_set_string", None, VP, I32, C.c_char_p)(obj, 0, text.encode("latin-1"))
    if label:
        h.fixed[label] = obj
    return obj


def host_read(lib, obj, wide):
    buf = C.create_string_buffer(1024)
    F(lib, "tpw_fs_get_string", I32, VP, I32, VP, I32)(obj, 1 if wide else 0, buf, 1024)
    return buf.value.decode("latin-1")


def host_begin(lib, c):
    h = HostState()
    h.keep = []
    addr = lambda n: F(lib, "tpw_fs_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    sset = F(lib, "tpw_fs_set_string", None, VP, I32, C.c_char_p)
    h.fixed = {LABELS["gGame"]: addr("gGame"), LABELS["gText"]: addr("gText")}
    for k, name in enumerate(PATHS):
        h.fixed[LABELS["gTPFileInfo"] + k] = addr(name)
        sset(addr(name), 0, c.get("paths", {}).get(name, "").encode("latin-1"))
    for name in GLOBAL_STRINGS:
        sset(addr(name), 0, c["globals"][name].encode("latin-1"))
    name = c.get("language_name", "").encode("latin-1") + b"\0"
    C.memmove(addr("gLanguageName"), name, len(name))
    C.c_uint32.from_address(addr("gTPDialogLanguageBits")).value = c.get("language_bits", 0)
    F(lib, "tpw_fs_set_project_dir", None, C.c_char_p)(c.get("project_dir", "HD:Games:TPW").encode("latin-1"))
    F(lib, "tpw_fs_set_found_name", None, C.c_char_p)(c.get("found_name", "").encode("latin-1"))
    F(lib, "tpw_fs_tree_clear", None)()
    add = F(lib, "tpw_fs_tree_add", None, C.c_char_p, C.c_char_p, U32)
    for path, entries in c.get("tree", {}).items():
        add(path.encode("latin-1"), None, 0)
        for name_, attributes in entries:
            add(path.encode("latin-1"), name_.encode("latin-1"), attributes)
    manager = F(lib, "tpw_fs_manager", VP)()
    resource = F(lib, "tpw_fs_resource", VP)()
    storage = F(lib, "tpw_fs_storage", VP)()
    sset(F(lib, "tpw_fs_resource_path", VP)(), 0, c.get("resource_path", "Data").encode("latin-1"))
    h.fixed.update({LABELS["manager"]: manager, LABELS["resource"]: resource, LABELS["storage"]: storage,
                    LABELS["resource_path"]: F(lib, "tpw_fs_resource_path", VP)()})
    F(lib, "tpw_fs_set_resources", None, VP, VP, VP)(manager, resource if c.get("resource", True) else None,
                                                    storage if c.get("storage", True) else None)
    h.lib = lib
    return h


def host_observe(lib, c):
    addr = lambda n: F(lib, "tpw_fs_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    return {"paths": [host_read(lib, addr(name), False) for name in PATHS],
            "language_name": C.string_at(addr("gLanguageName")).decode("latin-1")[:0x400],
            "language_bits": C.c_uint32.from_address(addr("gTPDialogLanguageBits")).value}

# ======================================================================= running a case


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    if "c" not in VT:
        make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


class FsTest(FnTest):
    def run_case(self, runner, case):
        mism = super().run_case(runner, case)
        if mism and any(m.startswith("log") for m in mism) and getattr(self, "_last", None):
            p, h = self._last
            return [f"log: {CL.first_difference(p['log'], h['log'])}"] + [m for m in mism if not m.startswith("log")][:3]
        return mism


def fn_test(name, args=None, edge=None, rnd=None, post=None):
    """args(c, side) -> argument values; side is ("ppc", e) or ("host", lib, h).
    post(c, side) -> an extra observation after the call."""
    t = FsTest(name, None, None, edge=edge, rnd=rnd)

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            a = args(c, ("ppc", e)) if args else []
            if name in TBERROR_RESULT:
                buf = e.alloc(4, b"\x5a\x5a\x5a\x5a")
                T.call_ppc(e, F_[name], [buf] + [T.u32(v) for v in a])
                res = T.s32(e.u32(buf))
            else:
                r3 = T.call_ppc(e, F_[name], [T.u32(v) for v in a])[0]
                res = (None if name in VOID else e.cl.lab(r3) if name in LABEL_RESULT
                       else r3 & 0xFF if name in BYTE_RESULT else T.s32(r3))
            out = ppc_observe(e, c)
            out["log"] = e.cl.log
            out["post"] = post(c, ("ppc", e)) if post else None
            out["result"] = res
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        t._p = out
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        a = args(c, ("host", lib, h)) if args else []
        CL.host_begin(lib, h.fixed, c.get("q", {}))
        vals = (I64 * 8)(*([int(v) for v in a] + [0] * (8 - len(a))))
        res = F(lib, "tpw_fs_call", I32, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, c)
        out["log"] = log
        out["post"] = post(c, ("host", lib, h)) if post else None
        out["result"] = None if name in VOID else res
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t

# ======================================================================= arguments


def arg_string(c, side, key, label, wide=False):
    text = c[key]
    if side[0] == "ppc":
        e = side[1]
        obj = new_string(e, text, wide, label)
        e.fs.args = getattr(e.fs, "args", {})
        e.fs.args[key] = (obj, wide)
        return obj
    lib, h = side[1], side[2]
    obj = host_string(lib, h, text, wide, label)
    h.args = getattr(h, "args", {})
    h.args[key] = (obj, wide)
    return obj


def latin(text):
    return "".join(ch if ord(ch) <= 0xFF else "?" for ch in text)


def arg_texts(c, side):
    """The texts of the string arguments after the call (UTF-16 code units above 0xff as '?')."""
    if side[0] == "ppc":
        e = side[1]
        return {k: latin(s_read(e, obj, wide)) for k, (obj, wide) in sorted(getattr(e.fs, "args", {}).items())}
    lib, h = side[1], side[2]
    return {k: host_read(lib, obj, wide) for k, (obj, wide) in sorted(getattr(h, "args", {}).items())}


def arg_file(c, side, implementation=True):
    """A CFile (label file) with a fake implementation (label implementation) or none."""
    if side[0] == "ppc":
        e = side[1]
        impl = 0
        if implementation:
            impl = e.alloc(8)
            w32(e, impl, VT["impl"])
            e.cl.labels[impl] = LABELS["implementation"]
        obj = e.alloc(8)
        e.write(obj, struct.pack(">II", VT["file"], impl))
        e.cl.labels[obj] = LABELS["file"]
        return obj
    lib, h = side[1], side[2]
    obj = F(lib, "tpw_fs_new_file", VP, I32)(1 if implementation else 0)
    h.fixed[LABELS["file"]] = obj
    if implementation:
        h.fixed[LABELS["implementation"]] = F(lib, "tpw_fs_file_implementation", VP, VP)(obj)
    return obj


def arg_buffer(c, side, label):
    if side[0] == "ppc":
        e = side[1]
        a = e.alloc(16)
        e.cl.labels[a] = label
        return a
    lib, h = side[1], side[2]
    buf = C.create_string_buffer(16)
    h.keep.append(buf)
    h.fixed[label] = C.addressof(buf)
    return C.addressof(buf)


def arg_info(c, side):
    if c.get("null"):
        return 0
    attributes, name = c["info"]
    if side[0] == "ppc":
        e = side[1]
        info = e.alloc(0x128)
        w32(e, info + 4, attributes)
        w32(e, info + 0x20, info + 0x24)
        e.write(info + 0x24, name.encode("latin-1") + b"\0")
        e.cl.labels[info] = LABELS["info"]
        return info
    lib, h = side[1], side[2]
    info = F(lib, "tpw_fs_new_info", VP, U32, C.c_char_p)(attributes, name.encode("latin-1"))
    h.fixed[LABELS["info"]] = info
    return info


def arg_disc(c, side):
    if c.get("null"):
        return 0
    if side[0] == "ppc":
        e = side[1]
        obj = e.alloc(0x10)
        w32(e, obj, CDISC_VT)
        e.cl.labels[obj] = LABELS["disc"]
        return obj
    lib, h = side[1], side[2]
    obj = F(lib, "tpw_fs_new_disc_storage", VP)()
    h.fixed[LABELS["disc"]] = obj
    return obj

# ======================================================================= case generators


SEPARATORS = [":", ":", ":", "/", "::"]
NAMES = ["", "a", "Coaster.wad", "coaster.WAD", "x.wad.wad", "dir:", "dir::", "HD:Games:", "file.tga", "wad",
         ".wad", "ride:", "Ext.Wad", "e\xe9t\xe9.wad", "a.b", "A.B"]


def rnd_text(r):
    if r.random() < 0.7:
        return r.choice(NAMES)
    return "".join(r.choice("ab:.wWAD/x") for _ in range(r.randint(0, 9)))


def base(r, **extra):
    c = dict(globals=dict(mSeparator=r.choice(SEPARATORS), mAllFiles=r.choice(["*", "*.*"]),
                          mWadExtension=r.choice([".wad", ".WAD", "wad", ""]), mDataFolder=r.choice(["data", "Data:", ""]),
                          mLanguageFolder=r.choice(["data:language", "lang"]),
                          mDefaultLanguagePath=r.choice(["language:default", ""])),
             paths={name: rnd_text(r) for name in PATHS}, language_name=r.choice(["", "German"]),
             language_bits=r.getrandbits(16), project_dir=r.choice(["HD:Games:TPW", "HD:Games:TPW:", "", "x"]),
             q={})
    c.update(extra)
    return c


def gen(make, n=40, seed=5):
    def edge():
        r = random.Random(seed)
        return [make(r) for _ in range(n)]
    return edge, make


TESTS = []


def add(name, make, args=None, post=None):
    edge, rnd = gen(make)
    TESTS.append(fn_test(name, args, edge, rnd, post))


# ---- FS_Registry.cpp
def g_language_bit(r):
    # the bits, their neighbours (every node of the original's compare tree) and others
    bit = 1 << r.randint(0, 15)
    return base(r, language=r.choice([bit, bit, bit + 1, bit - 1, 0, -1, 0x10000, 0x80000000, r.randint(0, 0x10000)]))


add("TPConvertTPLangDefs", g_language_bit, lambda c, s: [c["language"]])
add("FS_ReadRegistry", base)


def g_language(r):
    c = base(r)
    c["found_name"] = r.choice(LANGUAGES + ["german", "FRENCH", "Klingon", "", "English.txt", "x" * 300])
    c["q"] = {"FindFirstFile": [r.choice([0x5000, 0x5000, -1, 7])]}
    return c


add("FS_ReadLanguageRegistry", g_language)


# ---- files
def g_open(r):
    c = base(r, name=r.choice(["data:ui:a.tga", "x", "", "levels:1:park.sam"]), mode=r.choice(["rb", "r"]),
             resource=r.random() < 0.8, storage=r.random() < 0.7)
    c["q"] = {"QualifyFilename": [r.choice([1, 1, 0, 2])], "CFileStorage::OpenFile": [r.choice([0, 0, -1, 5])],
              "CTPBaseResource::OpenFile": [r.choice([1, 0, 2])], "CTPResourceManager::OpenFile": [r.choice([1, 0, -1])]}
    c["null_name"] = r.random() < 0.05
    return c


def open_args(c, side):
    if c["null_name"]:
        return [0, 0]
    texts = [c["name"], c["mode"]]
    if side[0] == "ppc":
        return [CL.new_text(side[1], t) for t in texts]
    keep = [C.create_string_buffer(t.encode("latin-1")) for t in texts]
    side[2].keep += keep
    return [C.addressof(k) for k in keep]


add("TPFile_Open", g_open, open_args)


def g_file(r):
    c = base(r, size=r.choice([0, 1, 16, 0x1000, 0xFFFFFFFF]), count=r.choice([0, 1, 3, 0x10001]),
             offset=r.getrandbits(32), whence=r.choice([0, 1, 2, 3, 0xFFFFFFFF]), impl=r.random() < 0.85)
    c["q"] = {"CFileImplementation::Read": [r.choice([0, 16, -1])], "CFileImplementation::Write": [r.choice([0, 4, -1])],
              "CFile::Seek": [r.choice([0, -1, 5, -100])], "CFile::Size": [r.choice([0, 1234, -1])]}
    return c


add("TPFile_Read", g_file, lambda c, s: [arg_buffer(c, s, LABELS["dest"]), c["size"], c["count"], arg_file(c, s, c["impl"])])
add("CFile::Read", g_file, lambda c, s: [arg_file(c, s, c["impl"]), arg_buffer(c, s, LABELS["dest"]), c["size"]])
add("CFile::Write", g_file, lambda c, s: [arg_file(c, s, c["impl"]), arg_buffer(c, s, LABELS["source"]), c["size"]])
add("TPFile_Close", g_file, lambda c, s: [arg_file(c, s)])
add("TPFile_Seek", g_file, lambda c, s: [arg_file(c, s), c["offset"], c["whence"]])
add("TPFile_Size", g_file, lambda c, s: [arg_file(c, s)])


def g_find(r):
    c = g_open(r)
    c["null_name"] = False
    c["resource"] = c["storage"] = True     # FS_FindFilePath does not check them
    c["out"] = rnd_text(r)
    return c


add("FS_FindFilePath", g_find,
    lambda c, s: [arg_string(c, s, "name", LABELS["arg0"]), arg_string(c, s, "out", LABELS["arg1"])], post=arg_texts)


def g_info(r):
    return base(r, info=(r.choice([0, 0x10, 0x11, 0x20, 0xFFFFFFEF, 0xFFFFFFFF]), r.choice(NAMES + [".", "..", ".x"])))


add("FS_IsSubDirectory", g_info, lambda c, s: [arg_info(c, s)])
add("FS_IsWadFile", g_info, lambda c, s: [arg_info(c, s)])


def g_tree(r):
    """A directory tree under "HD:Del" (the separator is ':')."""
    c = base(r)
    c["globals"]["mSeparator"] = ":"
    tree = {}

    def make(path, depth):
        entries = []
        for k in range(r.randint(0, 4)):
            name = r.choice(["a", "b.wad", ".hidden", "..", "sub%d" % k, "x"])
            attributes = r.choice([0, 0x10, 0x10, 0x20])
            entries.append((name, attributes))
            if attributes & 0x10 and not name.startswith(".") and depth < 2 and r.random() < 0.7:
                make(path + ":" + name, depth + 1)
        tree[path] = entries

    make("HD:Del", 0)
    if r.random() < 0.15:
        tree.pop("HD:Del")                  # the directory does not open
    c["tree"] = tree
    c["path"] = "HD:Del"
    return c


add("FS_DelTree", g_tree, lambda c, s: [arg_string(c, s, "path", LABELS["arg0"])])
add("CFileInfo::~CFileInfo", lambda r: base(r, info=(0, "x"), flag=r.choice([0, 1, -1, 3]), null=r.random() < 0.1),
    lambda c, s: [arg_info(c, s), c["flag"]])
add("CDiscFileStorage::~CDiscFileStorage", lambda r: base(r, flag=r.choice([0, 1, -1, 3]), null=r.random() < 0.1),
    lambda c, s: [arg_disc(c, s), c["flag"]])


def g_path(r):
    return base(r, path=rnd_text(r))


add("FS_AddDirectorySeparator", g_path, lambda c, s: [arg_string(c, s, "path", LABELS["arg0"])], post=arg_texts)
for setter in ("SetLocalPath", "SetRemotePath", "SetDataPath", "SetLevelPath", "SetLanguagePath"):
    add("CSYS_TPFileInfo::" + setter, g_path,
        (lambda c, s: [A["gTPFileInfo"] if s[0] == "ppc" else 0, arg_string(c, s, "path", LABELS["arg0"])]))
for getter in ("GetLocalPath", "GetRemotePath", "GetDataPath", "GetLevelPath"):
    add("CSYS_TPFileInfo::" + getter, g_path, lambda c, s: [A["gTPFileInfo"] if s[0] == "ppc" else 0])


# ---- string helpers
def g_pair(r):
    a = rnd_text(r)
    b = r.choice([a, a.upper(), a.lower(), rnd_text(r), a + "x", a[:-1]])
    return base(r, a=a, b=b)


def pair_args(wide=False):
    return lambda c, s: [arg_string(c, s, "a", LABELS["arg0"], wide), arg_string(c, s, "b", LABELS["arg1"], wide)]


add("TPStrGtr", g_pair, pair_args())
add("TPStrEq", g_pair, pair_args())
add("TPUniStrEq", lambda r: (lambda c: (c.update(a=c["a"] + r.choice(["", "ā", "€"])), c)[1])(g_pair(r)),
    pair_args(True))


def g_extension(r):
    ext = r.choice([".wad", ".WAD", ".tga", "", "wad", "x" * 20])
    name = r.choice([n + e for n in ("Coaster", "a", "") for e in (".wad", ".Wad", ".tga", "")] + [rnd_text(r)])
    return base(r, a=ext, b=name)


add("RemoveExtension", g_extension, pair_args(), post=arg_texts)


def g_convert(r):
    c = base(r, a=r.choice(["", "abc", "Ride\xe9", "x" * 40]), b=r.choice(["", "old"]))
    c["q"] = {"LbMem_Alloc": [r.choice([1, 1, 1, 0])]}
    if r.random() < 0.15:
        c["q"]["WideCharToMultiByte"] = [r.choice([0, -1, 3])]
        c["q"]["MultiByteToWideChar"] = [r.choice([0, -1, 3])]
    return c


add("UniStringToString", lambda r: (lambda c: (c.update(a=c["a"] + r.choice(["", "€"])), c)[1])(g_convert(r)),
    lambda c, s: [arg_string(c, s, "b", LABELS["arg0"]), arg_string(c, s, "a", LABELS["arg1"], True)], post=arg_texts)
add("StringToUniString", g_convert,
    lambda c, s: [arg_string(c, s, "b", LABELS["arg0"], True), arg_string(c, s, "a", LABELS["arg1"])], post=arg_texts)


# ---- the static initialisers
def sinit_ppc(e, c):
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({})
        regs = []
        e.stubs["__register_global_object"] = lambda em: regs.append((em.r(3), em.r(4)))
        texts = [A["gFSRegistryLanguages"] + 0x14 * k for k in range(16)] + [A["gFSRegistryTitle"], A["gFSRegistryLanguage"]]
        paths = [A["gTPFileInfo"] + 0x14 * k for k in range(5)]
        for a in texts + paths:
            e.write(a, b"\0" * 0x14)
        T.call_ppc(e, SINIT_REGISTRY, [])
        T.call_ppc(e, SINIT_SYSTEM, [])
        out = [s_read(e, a, False) for a in texts + paths]
        tds_dtor = e.u32(e.toc - 0x7C1C)      # the TVector of TbDynamicStringTemplate<char>'s destructor
        info_dtor = e.u32(e.toc - 0x75E4)     # CSYS_TPFileInfo's destructor (0x10265720)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    ok = regs == [(a, tds_dtor) for a in texts] + [(A["gTPFileInfo"], info_dtor)]
    return dict(texts=out, registrations_ok=ok)


def sinit_host(lib, c):
    out = (C.c_char * (96 * 23))()
    F(lib, "tpw_fs_sinit_texts", None, VP)(out)
    return dict(texts=[out.raw[96 * i:96 * (i + 1)].split(b"\0")[0].decode("latin-1") for i in range(23)],
                registrations_ok=True)


TESTS.append(FnTest("__sinit_FS_Registry_cpp + __sinit_FS_System_cpp", sinit_ppc, sinit_host,
                    edge=lambda: [{}], rnd=lambda r: {}))
