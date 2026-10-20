"""Unit v51045, against libtpw_unit_TbStaticFileBank_RegisterBank.dylib (tests/unit_TbStaticFileBank_RegisterBank.cpp), sound library
("sound symbols shared"), src/TbSysCommand_TpBody_Load_CSprocketsChannel_SetDescription_2_TbStaticFileBank_RegisterBank.cpp and
src/TbSoundSampleStore.cpp:
  TbStaticFileBank::RegisterBank 0x100130e8, TbSoundSampleStore::operator[] 0x100100e8, __sinit_TbSoundSamplePipe_cpp 0x1000f8d4,
  __sinit_TbMMFileBank_cpp 0x10009934, __sinit_StreamableMpeg2_cpp 0x10008760

A case is a world and a call, built on both sides. The file's GetSize / Read / Close, the buffer allocation, the channel's FindPipe, the
global-object registration and the queue constructor are fakes recording their arguments. Compared: the answer, mData's bytes, mCount,
mCache's bytes, the guard bytes and the event log. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_TbStaticFileBank_RegisterBank"
MASK = 0xFFFFFFFF
TOC = 0x1003abf0

FN_REG, FN_IDX, FN_SP, FN_MM, FN_MP2 = 0x100130e8, 0x100100e8, 0x1000f8d4, 0x10009934, 0x10008760
SIZES = {FN_REG: 252, FN_IDX: 136, FN_SP: 68, FN_MM: 120, FN_MP2: 140}
OWN = set(SIZES)

ALLOC, REGGLOBAL, QUEUECTOR = 0x1002ebfc, 0x1002eefc, 0x100040ac
F_CLOSE, F_READ, F_GETSIZE, B_DEREG, C_FINDPIPE = 0x10000100, 0x10000104, 0x10000108, 0x1000010c, 0x10000110

NAMES = {0x1003f2a8: "1003f2a8", 0x10033d3c: "10033d3c", 0x1003f2ac: "1003f2ac", 0x1003f284: "1003f284",
         0x10033d34: "10033d34", 0x1003f288: "1003f288", 0x1003e33c: "1003e33c", 0x1003e32a: "1003e32a",
         0x1003352c: "1003352c", 0x1003ebf8: "1003ebf8", 0x1003ebe8: "1003ebe8", 0x100336dc: "100336dc",
         0x1003ebec: "1003ebec", 0x1003ebc8: "1003ebc8"}
LABELS = {a: i + 1 for i, a in enumerate(NAMES)}
ADDR = dict(g329=0x1003e329, g328=0x1003e328)
HOST_LAYOUT = [144]   # sizeof(TbSoundPipeChannel) on the host (native pointers); the Mac stride 0x60 is used on the PPC side

CUR = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_alloc(e):
    L = e.cl
    n = e.r(3) & MASK
    L.rec("Alloc", (n,))
    if L.ret("AllocFail"):
        e.set_r(3, 0)
    else:
        e.set_r(3, e.alloc(max(n, 4)))


def st_regglobal(e):
    L = e.cl
    # the glue call saves the TOC at 0x14(r1) and the caller reloads r2 from there
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
    L.rec("RegisterGlobal", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


def st_queue(e):
    L = e.cl
    L.rec("QueueCtor", (L.lab(e.r(3)),))


def st_close(e):
    e.cl.rec("Close", ())


def st_read(e):
    n = e.r(5) & MASK
    e.cl.rec("Read", (n,))
    d = CUR["data"]
    e.write(e.r(4), d[:n])
    e.set_r(3, n)


def st_getsize(e):
    e.cl.rec("GetSize", ())
    e.set_r(3, e.cl.ret("GetSize") & MASK)


def st_dereg(e):
    e.cl.rec("Deregister", ())


def st_findpipe(e):
    L = e.cl
    ch, h = e.r(3), e.r(4)
    hv = struct.unpack(">I", e.read(h, 4))[0]
    L.rec("FindPipe", ((ch - CUR["array"]) // 0x60, hv))
    e.set_r(3, 1 if L.ret("FindPipe") else 0)


STUBS.internal(ALLOC, st_alloc)
STUBS.internal(REGGLOBAL, st_regglobal)
STUBS.internal(QUEUECTOR, st_queue)
STUBS.internal(F_CLOSE, st_close)
STUBS.internal(F_READ, st_read)
STUBS.internal(F_GETSIZE, st_getsize)
STUBS.internal(B_DEREG, st_dereg)
STUBS.internal(C_FINDPIPE, st_findpipe)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog(dict(LABELS), case["queues"])
        self.saved = STUBS.activate(e)
        CUR["data"] = case.get("data", b"")

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def desc(self, code):
        a = self.e.alloc(8)
        self.words(a, [code, TOC])
        return a

    def reg(self, case):
        e = self.e
        bank = e.alloc(0x20)
        fv = e.alloc(0x40)
        self.words(fv + 0xc, [self.desc(F_CLOSE)])
        self.words(fv + 0x10, [self.desc(F_READ)])
        self.words(fv + 0x1c, [self.desc(F_GETSIZE)])
        bv = e.alloc(0x50)
        self.words(bv + 0x3c, [self.desc(B_DEREG)])
        self.words(bank, [bv])
        self.words(bank + 0x14, [fv, 0])
        sret = e.alloc(4)
        T.call_ppc(e, FN_REG, [sret, bank, 0, 0])
        return (s32(self.rd_word(sret)), self.rd_word(bank + 8), self.rd_word(bank + 0xc), self.rd_word(bank + 0x1c))

    def idx(self, case):
        e = self.e
        n = case["n"]
        arr = e.alloc(0x60 * n)
        CUR["array"] = arr
        vt = e.alloc(0x50)
        self.words(vt + 0x3c, [self.desc(C_FINDPIPE)])
        for i in range(n):
            self.words(arr + 0x60 * i, [vt])
        store = e.alloc(0x10)
        self.words(store + 0xc, [arr])
        h = e.alloc(4)
        self.words(h, [case["handle"]])
        r3, _, _ = T.call_ppc(e, FN_IDX, [store, h])
        r3 &= MASK
        return -1 if r3 == 0 else (r3 - arr) // 0x60

    def sinit(self, fn):
        T.call_ppc(self.e, fn, [])

    def alloc(self, n):
        return self.e.alloc(n)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {LABELS[a]: lib.tpw_rb_addr(NAMES[a].encode()) for a in NAMES}
        CL.host_begin(lib, fixed, case["queues"])
        self.data = case.get("data", b"")

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rb_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def reg(self, case):
        out = (C.c_uint64 * 3)()
        buf = C.create_string_buffer(self.data, max(len(self.data), 1))
        r = self.lib.tpw_rb_register(C.cast(buf, C.c_void_p), len(self.data), out)
        return (r, out[0], out[1], out[2])

    def idx(self, case):
        return self.lib.tpw_rb_index(case["handle"], case["n"])

    def sinit(self, fn):
        getattr(self.lib, {FN_SP: "tpw_rb_sinit_pipe", FN_MM: "tpw_rb_sinit_mm", FN_MP2: "tpw_rb_sinit_mp2"}[fn])()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rb_addr.restype, lib.tpw_rb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rb_alloc.restype, lib.tpw_rb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rb_register.restype = C.c_int32
    lib.tpw_rb_register.argtypes = [C.c_void_p, C.c_uint32, C.POINTER(C.c_uint64)]
    lib.tpw_rb_index.restype, lib.tpw_rb_index.argtypes = C.c_int32, [C.c_uint32, C.c_uint32]
    for n in ("pipe", "mm", "mp2"):
        getattr(lib, "tpw_rb_sinit_" + n).restype = None
    out = (C.c_int64 * 32)()
    n = lib.tpw_rb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        op = case["op"]
        res = {}
        if op in ("sp", "mm", "mp2"):
            s.write(s.g("g329"), bytes([case["g329"]]))
            s.write(s.g("g328"), bytes([case["g328"]]))
            s.sinit({"sp": FN_SP, "mm": FN_MM, "mp2": FN_MP2}[op])
            res["g329"] = s.read(s.g("g329"), 1)
            res["g328"] = s.read(s.g("g328"), 1)
        elif op == "idx":
            res["answer"] = s.idx(case)
        else:
            r, data, count, cache = s.reg(case)
            res["answer"] = r
            res["count"] = count & MASK
            res["data"] = s.read(data, case["size"]) if data else None
            res["cache"] = s.read(cache, (count & MASK) * 4) if cache else None
            res["hasdata"] = data != 0
            res["hascache"] = cache != 0
        res["log"] = s.log()
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def reg_case(r, size=None, count=None, fail=None):
    size = r.choice([4, 8, 40, 100, 500]) if size is None else size
    count = r.choice([0, 1, 2, 5, 17, 100]) if count is None else count
    fail = r.choice([0, 0, 1]) if fail is None else fail
    body = bytes(r.randrange(256) for _ in range(size - 4))
    data = struct.pack("<I", count) + body
    return {"op": "reg", "size": size, "data": data, "queues": {"GetSize": [size], "AllocFail": [fail]}}


def reg_edges():
    r = random.Random(11)
    yield reg_case(r, 4, 0, 0)
    yield reg_case(r, 4, 1, 0)
    yield reg_case(r, 8, 1, 0)
    yield reg_case(r, 64, 0x100, 0)
    yield reg_case(r, 16, 3, 1)
    yield reg_case(r, 4, 0, 1)


def reg_random(r):
    return reg_case(r)


def idx_case(r, index=None, found=None, low=None, n=None):
    n = r.randrange(1, 9) if n is None else n
    index = r.randrange(n) if index is None else index
    low = r.choice([0, 1, 0x3ffffff, r.randrange(1 << 26)]) if low is None else low
    found = r.choice([0, 1]) if found is None else found
    return {"op": "idx", "n": n, "handle": ((index << 26) | low) & MASK, "queues": {"FindPipe": [found]}}


def idx_edges():
    r = random.Random(12)
    for found in (0, 1):
        yield idx_case(r, 0, found, 0, 1)
        yield idx_case(r, 3, found, 0x3ffffff, 4)
        yield idx_case(r, 7, found, 5, 8)


def idx_random(r):
    return idx_case(r)


def sinit_case(op):
    def make(r, g329=None, g328=None):
        return {"op": op,
                "g329": r.choice([0, 1, 0x80, 0xff]) if g329 is None else g329,
                "g328": r.choice([0, 1, 0x80, 0xff]) if g328 is None else g328,
                "queues": {}}

    def edges():
        r = random.Random(13)
        for a in (0, 1, 0x80, 0xff):
            for b in (0, 1, 0x80, 0xff):
                yield make(r, a, b)

    return edges, make


sp_e, sp_r = sinit_case("sp")
mm_e, mm_r = sinit_case("mm")
mp_e, mp_r = sinit_case("mp2")

TESTS = [
    FnTest("TbStaticFileBank::RegisterBank", ppc, host, edge=reg_edges, rnd=reg_random),
    FnTest("TbSoundSampleStore::operator[]", ppc, host, edge=idx_edges, rnd=idx_random),
    FnTest("__sinit_TbSoundSamplePipe_cpp", ppc, host, edge=sp_e, rnd=sp_r),
    FnTest("__sinit_TbMMFileBank_cpp", ppc, host, edge=mm_e, rnd=mm_r),
    FnTest("__sinit_StreamableMpeg2_cpp", ppc, host, edge=mp_e, rnd=mp_r),
]
