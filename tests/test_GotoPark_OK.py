"""Unit v5276, the park-goto callbacks, against libtpw_unit_GotoPark_OK.dylib (tests/unit_GotoPark_OK.cpp), main app
("SimTheme Park"), src/game/online/online.cpp:
  GotoPark_OK 0x10213728, GotoCallbackFn 0x102137bc

A case is a world and a call. The callees outside the unit (operator new, the COnlineControl constructor, GotoFailed, GotoStoredPerson,
UDebug, GS_SetCurrentGameStatus) are fakes on both sides that record their arguments.

The world of a case: gOnlineControl either NULL or an existing object. What is compared: the answer, the global (NULL, unchanged or newly
created) and the event log. No fused operations. Not tested: none.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GotoPark_OK"
MASK = 0xFFFFFFFF

FN_PARK, FN_CB = 0x10213728, 0x102137BC
SIZES = {FN_PARK: 112, FN_CB: 140}
OWN = set(SIZES)

CALLEE_NEW = "__nw__FUl"
CALLEE_CTOR = 0x10213870
CALLEE_FAILED = 0x1021C254
CALLEE_STORED = 0x1021C330
CALLEE_DEBUG = 0x10000E40
CALLEE_GS = 0x10236E6C

ADDR = dict(gOnlineControl=0x10396D28)
HOST_LAYOUT = [8]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("new", (size,))
    e.set_r(3, e.alloc(size))


STUBS.imp(CALLEE_NEW, stub_new)
STUBS.internal(CALLEE_CTOR, CL.recorder("COnlineControl", "N"))
STUBS.internal(CALLEE_FAILED, CL.recorder("GotoFailed", "L"))
STUBS.internal(CALLEE_STORED, CL.recorder("GotoStoredPerson", "L"))
STUBS.internal(CALLEE_DEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(CALLEE_GS, CL.recorder("GS_SetCurrentGameStatus", "i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def rd_word(self, a):
        return int.from_bytes(self.read(a, 4), self.order)


class Ppc(Side):
    order = "big"

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

    def park(self, ctx):
        r3, _, _ = T.call_ppc(self.e, FN_PARK, [ctx & MASK])
        return r3 & MASK

    def callback(self, status, data):
        T.call_ppc(self.e, FN_CB, [status & MASK, data & MASK])
        return 0


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gpo_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gpo_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def park(self, ctx):
        return self.lib.tpw_gpo_park(ctx & MASK) & MASK

    def callback(self, status, data):
        self.lib.tpw_gpo_callback(T.s32(status), data & MASK)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gpo_addr.restype, lib.tpw_gpo_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gpo_alloc.restype, lib.tpw_gpo_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gpo_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gpo_park.restype, lib.tpw_gpo_park.argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_gpo_callback.restype, lib.tpw_gpo_callback.argtypes = None, [C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gpo_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    pre = s.alloc(16) if case["exists"] else 0
    s.words(s.g("gOnlineControl"), [pre])
    if s.order == "little":
        s.words(s.g("gOnlineControl") + 4, [0])     # the host's pointer is 8 bytes
    return pre


def observe(s, case, pre, answer):
    now = s.rd_word(s.g("gOnlineControl"))
    state = "null" if now == 0 else ("same" if now == (pre & MASK) else "new")
    return {"answer": answer, "log": s.log(), "global": state}


def run(s, case):
    try:
        pre = build(s, case)
        if case["op"] == "park":
            ans = s.park(case["ctx"])
        else:
            ans = s.callback(case["status"], case["ctx"])
        return observe(s, case, pre, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(op, exists, status=0, ctx=0):
    return {"op": op, "queues": {}, "exists": exists, "status": status, "ctx": ctx}


def park_edges():
    for exists in (0, 1):
        for ctx in (0, 1, 0x12345678):
            yield mk("park", exists, ctx=ctx)


def park_random(r):
    return mk("park", r.choice([0, 1]), ctx=r.getrandbits(32))


def cb_edges():
    for exists in (0, 1):
        for status in (0, 1, -1, 2, 0x7FFFFFFF, -0x80000000):
            yield mk("cb", exists, status=status, ctx=5)


def cb_random(r):
    return mk("cb", r.choice([0, 1]), status=r.choice([0, 0, 1, -1, r.getrandbits(32)]), ctx=r.getrandbits(32))


TESTS = [
    FnTest("GotoPark_OK", ppc, host, edge=park_edges, rnd=park_random),
    FnTest("GotoCallbackFn", ppc, host, edge=cb_edges, rnd=cb_random),
]
