"""Unit v51020, CSaveLoad::Save, against libtpw_unit_CSaveLoad_Save.dylib (tests/unit_CSaveLoad_Save.cpp), main app
("SimTheme Park"), src/SaveLoad_CSaveLoad_SaveHeader_2_CSaveLoad_Save.cpp:
  CSaveLoad::Save 0x10157868

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee is a fake on both sides that records
its arguments: GAME_ASSERTMSG, CTRACE, LbFile_Open/Write/Close/Delete, SaveHeader, PreSaveOperations, SaveModules, the CFile / CMemFile /
CDiscFile members (Close, Seek, GetSize, Open, constructors, destructors), the temporary allocator (the constructor, Allocate and Free
reached through the pointer glue), the access objects' Read and Write, operator new and LbZLib_MemCompress. Buffers are named in the log
by their order of allocation; files by their kind (1 memory, 2 disc).

The world of a case: the type, the path, the header (given or not), whether the allocator exists, and the queues of answers (open, write,
SaveHeader, memory file open and access object, SaveModules, size, allocations, compress, disc open and access object, access write).
What is compared: the answer, the event log, the CSaveLoad's first words (mError) and whether gMemoryTemporary is set.

No fused operations. Not tested: a negative GetSize (the original allocates a 4 GB buffer).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSaveLoad_Save"
MASK = 0xFFFFFFFF

FN_SAVE = 0x10157868
SIZES = {FN_SAVE: 1844}
OWN = set(SIZES)

ADDR = dict(gMemTemp=0x103BCF14)
HOST_LAYOUT = [4]

MEMVT = 0x102A4D0C
DISCVT = 0xC0DE0002
FILEVT = 0xC0DE0001
M_ALLOC, M_FREE, M_READ, M_WRITE = 0x7F000001, 0x7F000002, 0x7F000003, 0x7F000004


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
W = {}      # the persistent objects' addresses
BUFS = []   # the case's allocations, in order


def bufid(p):
    p &= MASK
    if p == 0:
        return 0
    return BUFS.index(p) + 1 if p in BUFS else -1


def kind(e, a):
    w = e.u32(a)
    return 1 if w == MEMVT else 2 if w == DISCVT else 0


def pw(e, a, w):
    e.write(a, struct.pack(">I", w & MASK))


def res_word(e, v):
    pw(e, e.r(3), v)


# ---- recorders (text arguments)
def ret_open(e, L):
    res_word(e, L.ret("LbOpen"))
    pw(e, e.r(4), L.ret("LbHandle"))
    return e.r(3)


def ret_zero_res(e, L):
    res_word(e, 0)
    return e.r(3)


def ret_disc_open(e, L):
    res = s32(L.ret("DiscOpen"))
    res_word(e, res)
    if res >= 0 and L.ret("discImpl") != 0:
        pw(e, e.r(4) + 4, W["discitem"])
    return e.r(3)


rec_open = CL.recorder("LbFile_Open", "xxsu", ret=ret_open)
rec_delete = CL.recorder("LbFile_Delete", "xs", ret=ret_zero_res)
rec_disc_open = CL.recorder("DiscOpen", "xxsu", ret=ret_disc_open)
rec_a1 = CL.recorder("assert", "is")
rec_a2 = CL.recorder("assert", "iss")
rec_t1 = CL.recorder("trace", "us")
rec_t2 = CL.recorder("trace", "uss")


def st_assert(e):
    (rec_a2 if "%s" in e.cstr(e.r(4)) else rec_a1)(e)


def st_trace(e):
    (rec_t2 if "%s" in e.cstr(e.r(4)) else rec_t1)(e)


# ---- plain stubs
def st_write(e):
    L = e.cl
    h, n = e.r(4) & MASK, e.r(6) & MASK
    L.rec("LbFile_Write", (h, n))
    res_word(e, L.ret("LbWrite"))
    if e.r(7):
        pw(e, e.r(7), n)


def st_close(e):
    e.cl.rec("LbFile_Close", (e.r(4) & MASK,))
    res_word(e, 0)


def st_save_header(e):
    e.cl.rec("SaveHeader", (e.r(4) & MASK, int(e.r(5) != 0)))
    e.set_r(3, e.cl.ret("SaveHeader") & MASK)


def st_pre(e):
    e.cl.rec("PreSave", ())


def st_modules(e):
    e.cl.rec("SaveModules", (kind(e, e.r(4)), s32(e.r(5))))
    e.set_r(3, e.cl.ret("SaveModules") & MASK)


def st_cfile_ctor(e):
    pw(e, e.r(3), FILEVT)
    pw(e, e.r(3) + 4, 0)


def st_mem_open(e):
    L = e.cl
    L.rec("MemOpen", (e.r(5) & MASK,))
    res = s32(L.ret("MemOpen"))
    res_word(e, res)
    if res >= 0 and L.ret("memImpl") != 0:
        pw(e, e.r(4) + 4, W["memitem"])


def st_disc_ctor(e):
    pw(e, e.r(3), DISCVT)
    pw(e, e.r(3) + 4, 0)
    e.cl.rec("DiscCtor", ())


def st_disc_dtor(e):
    e.cl.rec("DiscDtor", ())


def st_file_dtor(e):
    e.cl.rec("FileDtor", ())


def st_file_close(e):
    e.cl.rec("FileClose", (kind(e, e.r(3)),))


def st_get_size(e):
    e.cl.rec("GetSize", (kind(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetSize") & MASK)


def st_seek(e):
    e.cl.rec("Seek", (kind(e, e.r(3)), s32(e.r(4)), e.r(5) & MASK))
    e.set_r(3, 0)


def st_compress(e):
    e.cl.rec("Compress", (bufid(e.r(3)), bufid(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))
    e.set_r(3, e.cl.ret("Compress") & MASK)


def st_new(e):
    e.set_r(3, e.alloc(max(e.r(3) & MASK, 4)))


def st_alloc_ctor(e):
    e.cl.rec("AllocCtor", ())
    pw(e, e.r(3) + 4, W["allocvt"])


def st_ptr_glue(e):
    L = e.cl
    code = e.u32(e.r(12))
    if code == M_ALLOC:
        size = e.r(4) & MASK
        L.rec("Alloc", (size,))
        if L.ret("Alloc") == 0:
            e.set_r(3, 0)
        else:
            p = e.alloc(max(size, 1))
            BUFS.append(p)
            e.set_r(3, p)
    elif code == M_FREE:
        ref = e.r(4)
        L.rec("Free", (bufid(e.u32(ref)),))
        pw(e, ref, 0)
    elif code == M_READ:
        item, buf, size = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
        L.rec("Read", (1 if item == W["memitem"] else 2 if item == W["discitem"] else -1, bufid(buf), size))
        e.write(buf, bytes(i % 251 for i in range(size)))
        e.set_r(3, size)
    elif code == M_WRITE:
        item, buf, size = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
        L.rec("Write", (1 if item == W["memitem"] else 2 if item == W["discitem"] else -1, bufid(buf), size))
        e.set_r(3, L.ret("AccWrite") & MASK)
    else:
        raise AssertionError("unknown pointer glue target %x" % code)


for _a, _f in (
    (0x1000DDE8, st_assert), (0x1006FCBC, st_trace),
    (0x10238830, rec_open), (0x10238878, st_write), (0x10238860, st_close), (0x10239880, rec_delete),
    (0x10159A08, st_save_header), (0x1015B428, st_pre), (0x1015A190, st_modules),
    (0x10238F38, st_cfile_ctor), (0x102398C8, st_mem_open), (0x10239688, st_disc_ctor), (0x102396A0, rec_disc_open),
    (0x102396D0, st_disc_dtor), (0x10239868, st_file_dtor), (0x102396B8, st_file_close), (0x10239BC8, st_get_size),
    (0x10239BE0, st_seek), (0x10239BF8, st_compress), (0x102384B8, st_new), (0x102390A0, st_alloc_ctor),
    (0x1023BAA8, st_ptr_glue),
):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    W["base"] = T.persist_alloc(runner.emu, 0x100)
    fill(runner.emu)


def fill(e):
    b = W["base"]
    for i, m in enumerate((M_ALLOC, M_FREE, M_READ, M_WRITE)):
        e.write(b + 8 * i, struct.pack(">II", m, 0))
    W["allocvt"], W["accvt"], W["existing"], W["memitem"], W["discitem"] = b + 0x20, b + 0x40, b + 0x60, b + 0x70, b + 0x80
    e.write(b + 0x20 + 0xC, struct.pack(">I", b + 0))
    e.write(b + 0x20 + 0x14, struct.pack(">I", b + 8))
    e.write(b + 0x40 + 0xC, struct.pack(">I", b + 0x10))
    e.write(b + 0x40 + 0x10, struct.pack(">I", b + 0x18))
    e.write(b + 0x60, struct.pack(">II", 0, W["allocvt"]))
    e.write(b + 0x70, struct.pack(">I", W["accvt"]))
    e.write(b + 0x80, struct.pack(">I", W["accvt"]))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        del BUFS[:]
        fill(e)
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def gaddr(self):
        return self.e.u32(0x102610F4)   # the TOC slot of the import (the harness puts the data elsewhere)

    def set_exists(self, ex):
        self.words(self.gaddr(), [W["existing"] if ex else 0])

    def has_alloc(self):
        return int(self.rd_words(self.gaddr(), 1)[0] != 0)

    def call(self, this, path, header, typ):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [this, path, header, typ & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_exists(self, ex):
        self.lib.tpw_sl_reset(1 if ex else 0)

    def has_alloc(self):
        return self.lib.tpw_sl_has_allocator()

    def call(self, this, path, header, typ):
        return self.lib.tpw_sl_Save(this, path, header, s32(typ)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sl_alloc.restype, lib.tpw_sl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sl_reset.argtypes = [C.c_int32]
    lib.tpw_sl_has_allocator.restype = C.c_int32
    lib.tpw_sl_Save.restype = C.c_uint32
    lib.tpw_sl_Save.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        this = s.alloc(0x20)
        path = s.alloc(64)
        s.write(path, case["path"].encode() + b"\0")
        header = s.alloc(16) if case["hdr"] else 0
        s.set_exists(case["exists"])
        answer = s.call(this, path, header, case["type"])
        lg = s.log()
        return {"answer": answer, "log": lg, "obj": s.rd_words(this, 8), "alloc": s.has_alloc()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


PATHS = ["Park1.sav", "a", "My Park/with space.sav", ""]


def make(r, **kw):
    """A case: a random world, then the given overrides of the queue answers."""
    size = r.choice([0, 1, 0x40, 0x200, r.randrange(2, 0x800)])
    comp = r.choice([0, 1, max(size - 1, 0), size, size + 1, r.randrange(0, 0x400)])
    q = {
        "LbOpen": [-1 if r.random() < 0.1 else 0],
        "LbHandle": [r.randrange(1, 1000)],
        "LbWrite": [-1 if r.random() < 0.1 else 0],
        "SaveHeader": [0 if r.random() < 0.1 else 1],
        "MemOpen": [-1 if r.random() < 0.1 else 0],
        "memImpl": [0 if r.random() < 0.1 else 1],
        "SaveModules": [0 if r.random() < 0.1 else 1],
        "GetSize": [size],
        "Alloc": [0 if r.random() < 0.1 else 1, 0 if r.random() < 0.1 else 1],
        "Compress": [comp],
        "DiscOpen": [-1 if r.random() < 0.1 else 0],
        "discImpl": [0 if r.random() < 0.1 else 1],
        "AccWrite": [comp if r.random() < 0.7 else r.choice([comp + 1, -1, 0])],
    }
    q.update(kw.pop("q", {}))
    c = {"queues": q, "exists": r.choice([0, 1]), "type": r.choice([0, 1, 2, 2, -1]), "hdr": r.choice([0, 1]),
         "path": r.choice(PATHS)}
    c.update(kw)
    return c


OK = {"LbOpen": [0], "LbWrite": [0], "SaveHeader": [1], "MemOpen": [0], "memImpl": [1], "SaveModules": [1], "Alloc": [1],
      "GetSize": [0x100], "Compress": [0x80], "AccWrite": [0x80], "DiscOpen": [0], "discImpl": [1]}


def edges():
    r = random.Random(51020)
    yield make(r, q=dict(OK), type=2, exists=0)
    yield make(r, q=dict(OK), type=1, exists=1)
    yield make(r, q=dict(OK), type=0)
    for key, val in (("LbOpen", [-1]), ("LbWrite", [-1]), ("SaveHeader", [0]), ("MemOpen", [-1]), ("SaveModules", [0]),
                     ("Alloc", [0]), ("Alloc", [1, 0]), ("DiscOpen", [-1]), ("discImpl", [0]), ("memImpl", [0]),
                     ("AccWrite", [0x7F]), ("AccWrite", [-1]), ("Compress", [0x101]), ("Compress", [0]),
                     ("GetSize", [0]), ("GetSize", [1])):
        for ex in (0, 1):
            q = dict(OK)
            q[key] = val
            yield make(r, q=q, exists=ex, type=2)
    q = dict(OK)
    q.update({"Compress": [0x100], "AccWrite": [0x100], "GetSize": [0x100]})
    yield make(r, q=q)
    q.update({"Compress": [0x101], "AccWrite": [0x101]})
    yield make(r, q=q)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CSaveLoad::Save", ppc, host, edge=edges, rnd=rnd),
]
