"""Unit v5111: TestAdvisor2/3/4 and delete_ride, against libtpw_unit_TestAdvisor3.dylib (tests/unit_TestAdvisor3.cpp), main app,
src/game/keyboard.cpp:
  TestAdvisor2 0x1014c2b0, TestAdvisor3 0x1014c33c, TestAdvisor4 0x1014c3c8, delete_ride 0x1014bf74

The callees ADV_Stat, ADV_Call, LLCC_SystemSetMode and LLCM_LeftMouseButtonUp are fakes on both sides recording their arguments.
The world: the advisor index global, the halfword at gIntrface+0x24, the ADV_Stat answer. Compared: the answer, the index, the log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TestAdvisor3"
MASK = 0xFFFFFFFF

FNS = {"a2": 0x1014C2B0, "a3": 0x1014C33C, "a4": 0x1014C3C8, "del": 0x1014BF74}
SIZES = {0x1014C2B0: 104, 0x1014C33C: 104, 0x1014C3C8: 104, 0x1014BF74: 120}
OWN = set(SIZES)

ADV_STAT = 0x100090DC
ADV_CALL = 0x1000887C
SETMODE = 0x10090E18
LMBU = 0x10084CA4

ADDR = dict(index=0x102A4E54, intrface=0x102DC1AC)
HOST_LAYOUT = [48]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_stat(e):
    L = e.cl
    L.rec("ADV_Stat", (0,))
    e.set_r(3, L.ret("ADV_Stat") & MASK)


def stub_call(e):
    L = e.cl
    L.rec("ADV_Call", (T.s32(e.r(3)),))
    e.set_r(3, 0)


def stub_setmode(e):
    L = e.cl
    L.rec("LLCC_SystemSetMode", (T.s32(e.r(3)), e.r(4) & 0xFF and 1 or 0))


def stub_lmbu(e):
    L = e.cl
    L.rec("LLCM_LeftMouseButtonUp", (T.s32(e.r(3)), T.s32(e.r(4)), e.r(5) & 0xFFFF, T.s32(e.r(6))))


STUBS.internal(ADV_STAT, stub_stat)
STUBS.internal(ADV_CALL, stub_call)
STUBS.internal(SETMODE, stub_setmode)
STUBS.internal(LMBU, stub_lmbu)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    f24 = 0x24

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

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

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FNS[op], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    f24 = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ta_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_ta_" + {"a2": "ta2", "a3": "ta3", "a4": "ta4", "del": "del"}[op])() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ta_addr.restype, lib.tpw_ta_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ta_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("ta2", "ta3", "ta4", "del"):
        getattr(lib, "tpw_ta_" + n).restype = C.c_uint32
        getattr(lib, "tpw_ta_" + n).argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ta_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.words(s.g("index"), [case["index"]])
        s.half(s.g("intrface") + s.f24, case["f24"])
        ans = s.call(case["op"])
        return {"answer": ans, "log": s.log(), "index": s.rd_words(s.g("index"), 1),
                "f24": s.rd_words(s.g("intrface") + s.f24, 1)[0] >> (16 if s.fmt[0] == ">" else 0) & 0xFFFF}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(op, index, stat, f24=0):
    return {"op": op, "index": index & MASK, "f24": f24, "queues": {"ADV_Stat": [stat]}}


def adv_edges(op, step):
    idx = [0, 1, 2, 9, 10, 11, 12, 13, 600, 601, 602, 603, 604, 612, 613, 614, 700, -1, -50, -1000, 0x7FFFFFF0]
    if op == "a4":
        idx += [561, 562, 563, 564, 565]
    for i in idx:
        for st in (0, 1, -1, 5):
            yield mk(op, i, st)


def adv_random(op):
    def f(r):
        return mk(op, r.randrange(-30, 720), r.choice([0, 0, 1, -3, r.randrange(-100, 100)]))
    return f


def del_edges():
    for v in (0, 1, 2, 127, 128, 129, 255, 256, 257, 0x7FFF, 0x8000, 0xFFFF, 100, 1000):
        yield mk("del", 0, 0, v)


def del_random(r):
    return mk("del", 0, 0, r.randrange(0, 65536))


def edges(op):
    return lambda: adv_edges(op, 0)


TESTS = [
    FnTest("TestAdvisor2", ppc, host, edge=edges("a2"), rnd=adv_random("a2")),
    FnTest("TestAdvisor3", ppc, host, edge=edges("a3"), rnd=adv_random("a3")),
    FnTest("TestAdvisor4", ppc, host, edge=edges("a4"), rnd=adv_random("a4")),
    FnTest("delete_ride", ppc, host, edge=del_edges, rnd=del_random),
]
