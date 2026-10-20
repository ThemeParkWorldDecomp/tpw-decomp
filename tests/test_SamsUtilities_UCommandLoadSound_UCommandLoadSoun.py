"""Unit v5758, the UCommandLoadSound constructor and destructor, against libtpw_unit_SamsUtilities_UCommandLoadSound_UCommandLoadSoun.dylib
(tests/unit_SamsUtilities_UCommandLoadSound_UCommandLoadSoun.cpp), src/SamsUtilities_UCommandLoadSound_UCommandLoadSoun.cpp:
  UCommandLoadSound 0x1002ac30, ~UCommandLoadSound 0x1002ab8c

The callees (UCommand ctor, counter copy ctor, counter dtor, operator delete glue) are fakes on both sides that record their arguments and
leave a marker word at the object. Compared: the log, words at +0, +4, +8 of the object, the answer (this != NULL for the destructor).
The destructor gets a NULL object, flags <= 0 and > 0, with garbage in the high half of r4 on the PowerPC side. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SamsUtilities_UCommandLoadSound_UCommandLoadSoun"
MASK = 0xFFFFFFFF

FN_CTOR, FN_DTOR = 0x1002ac30, 0x1002ab8c
SIZES = {FN_CTOR: 80, FN_DTOR: 100}
OWN = set(SIZES)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s16(v):
    return T.s16(v)


def stub_ucommand(e):
    L = e.cl
    L.rec("UCommand", (L.lab(e.r(3)),))
    e.write(e.r(3), struct.pack(">I", 0x11111111))


def stub_copy(e):
    L = e.cl
    L.rec("CounterCopy", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.write(e.r(3), e.read(e.r(4), 4))


def stub_cdtor(e):
    L = e.cl
    L.rec("CounterDestruct", (L.lab(e.r(3)), s16(e.r(4))))
    e.write(e.r(3), struct.pack(">I", 0xDEAD0000 | (e.r(4) & 0xFFFF)))


def stub_free(e):
    L = e.cl
    L.rec("Free", (L.lab(e.r(3)),))


STUBS.internal(0x1002a7e8, stub_ucommand)
STUBS.internal(0x1002ad08, stub_copy)
STUBS.internal(0x1002ae58, stub_cdtor)
STUBS.internal(0x1002ea94, stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
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

    def ctor(self, obj, src):
        T.call_ppc(self.e, FN_CTOR, [obj, src])
        return 1

    def dtor(self, obj, flag, junk):
        r3, _, _ = T.call_ppc(self.e, FN_DTOR, [obj, ((junk << 16) | (flag & 0xFFFF)) & MASK])
        return int(r3 != 0)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lsc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self, obj, src):
        self.lib.tpw_lsc_ctor(obj, src)
        return 1

    def dtor(self, obj, flag, junk):
        return self.lib.tpw_lsc_dtor(obj, s16(flag)) & 1


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_lsc_"
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "ctor").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "dtor").restype, getattr(lib, p + "dtor").argtypes = C.c_uint32, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == [4, 8], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(64)
        src = s.alloc(64)
        s.words(obj, case["init"])
        s.words(src, case["src"])
        if case["op"] == "ctor":
            ans = s.ctor(obj, src)
            tgt = obj
        else:
            tgt = 0 if case["null"] else obj
            ans = s.dtor(tgt, case["flag"], case["junk"])
        return {"answer": ans, "log": s.log(), "words": s.rd_words(obj, 4), "src": s.rd_words(src, 2)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, **kw):
    c = {"op": op, "queues": {}, "init": [r.getrandbits(32) for _ in range(4)], "src": [r.getrandbits(32) for _ in range(2)],
         "null": False, "flag": 0, "junk": 0}
    c.update(kw)
    return c


def ctor_edges():
    r = random.Random(11)
    for _ in range(3):
        yield mk(r, "ctor")


def ctor_random(r):
    return mk(r, "ctor")


def dtor_edges():
    r = random.Random(12)
    for null in (False, True):
        for flag in (0, 1, -1, 0x7FFF, 0x8000, 0xFFFF, 2):
            yield mk(r, "dtor", null=null, flag=flag & 0xFFFF, junk=r.choice([0, 0xFFFF, 1]))


def dtor_random(r):
    return mk(r, "dtor", null=r.random() < 0.2, flag=r.getrandbits(16), junk=r.getrandbits(16))


TESTS = [
    FnTest("SamsUtilities::UCommandLoadSound::UCommandLoadSound", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("SamsUtilities::UCommandLoadSound::~UCommandLoadSound", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
