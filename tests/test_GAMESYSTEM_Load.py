"""Unit v5151, the park file's clock words, against libtpw_unit_GAMESYSTEM_Load.dylib (tests/unit_GAMESYSTEM_Load.cpp), main app
("SimTheme Park"), src/GameSys_GAMESYSTEM_Load.cpp:
  GAMESYSTEM_Save 0x1023678c, GAMESYSTEM_Load 0x10236b30

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. LbFile_Read, LbFile_Write and UDebug are fakes
on both sides that record their arguments; reads answer data words and transferred counts from queues, writes answer counts.

The world of a case: the six clock globals random. What is compared: the answer, the six globals, the event log (every word written,
and the strings of UDebug). The sixth write of Save is an uninitialised stack word on the Mac: both fakes record 0 for it.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GAMESYSTEM_Load"
MASK = 0xFFFFFFFF

FN_SAVE, FN_LOAD = 0x1023678C, 0x10236B30
SIZES = {FN_SAVE: 892, FN_LOAD: 788}
OWN = set(SIZES)

LBREAD, LBWRITE, UDEBUG = 0x10238848, 0x10238878, 0x10000E40

ADDR = dict(gGameSys_103bc178=0x103BC178, gTickCount=0x103BC17C, gTickNow=0x103BC180,
            gLastTurnTime=0x103BC184, gLastFlyTime=0x103BC188, gTickTime=0x103BC18C)
GLOBALS = list(ADDR)
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    L = e.cl
    L.rec("Read", (L.lab(e.r(4)), e.r(6) & MASK))
    v = L.ret("RdV") & MASK
    n = L.ret("RdN") & MASK
    e.write(e.r(5), struct.pack(">I", v))
    e.write(e.r(7), struct.pack(">I", n))


def stub_write(e):
    L = e.cl
    v = struct.unpack(">I", e.read(e.r(5), 4))[0]
    k = getattr(L, "nwrites", 0)
    L.nwrites = k + 1
    if k == 5:
        v = 0
    L.rec("Write", (L.lab(e.r(4)), e.r(6) & MASK, v))
    e.write(e.r(7), struct.pack(">I", L.ret("WrN") & MASK))


def stub_udebug(e):
    e.cl.rec("UDebug", (), (e.cstr(e.r(3)),))


STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.internal(UDEBUG, stub_udebug)


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

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, f):
        r3, _, _ = T.call_ppc(self.e, fn, [f])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_gsl_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gsl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gsl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, f):
        return (self.lib.tpw_gsl_save if fn == FN_SAVE else self.lib.tpw_gsl_load)(f) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gsl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "reset").restype, getattr(lib, p + "reset").argtypes = None, []
    for n in ("save", "load"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for name in GLOBALS:
        s.words(s.g(name), [case[name]])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for name in GLOBALS:
        out[name] = s.rd_words(s.g(name), 1)
    return out


def run(s, case):
    try:
        build(s, case)
        f = s.alloc(16)
        return observe(s, case, s.call(case["fn"], f))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rword(r):
    return r.choice([0, 1, MASK, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(16)])


def base(r, fn, fail=None):
    if fail is None:
        fail = r.choice(list(range(9)) + [9, 9, 9])
    good = [4, 4, 4, 5, 7]
    rdn = [r.choice(good) if r.random() < 0.5 else 4 for _ in range(9)]
    if fail < 9:
        rdn[fail] = r.choice([0, 1, 3, 8, 12, 0x104])
    c = {"fn": fn, "queues": {"RdN": rdn, "WrN": list(rdn), "RdV": [rword(r) for _ in range(9)]}}
    for name in GLOBALS:
        c[name] = rword(r)
    return c


def save_edges():
    r = random.Random(5151)
    for fail in list(range(10)):
        yield base(r, FN_SAVE, fail)


def load_edges():
    r = random.Random(5152)
    for fail in list(range(10)):
        yield base(r, FN_LOAD, fail)


TESTS = [
    FnTest("GAMESYSTEM_Save", ppc, host, edge=save_edges, rnd=lambda r: base(r, FN_SAVE)),
    FnTest("GAMESYSTEM_Load", ppc, host, edge=load_edges, rnd=lambda r: base(r, FN_LOAD)),
]
