"""Unit v5694, UIMain_GetObjectState, against libtpw_unit_UIMain_GetObjectState.dylib (tests/unit_UIMain_GetObjectState.cpp), main app,
src/ui_main_panel_UIMain_GetObjectState.cpp:
  UIMain_GetObjectState 0x10180b84

A case is a world and a call. The world (an object block, a ride record block, a few world halfwords, seven id globals) is built twice by
one piece of Python code. Every callee outside the unit is a fake on both sides that records its arguments and answers from the case's
queues: GetRideInfo (answers the case's ride block), IsCondemned, IsTrackClosed, IsTrackValid, RequiresUpgrade, GetStaffHQ, IsStriking,
GetAssignedStaffMember, IsBeingWorkedUpon, IsFlaggedForMaintenance, IsBrokenDown, IsBOQConnected, IsClosed, UpgradesWhat,
IsExitCellConnected.

What is compared: the answer, the event log, the id globals and the world halfwords read back. No fused operations. Not tested: nothing
skipped (the ride record is always present).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMain_GetObjectState"
MASK = 0xFFFFFFFF

FN = 0x10180B84
OWN = {FN}

ADDR = dict(world=0x1037EBFC, id7858=0x102A7858, id785A=0x102A785A, id785C=0x102A785C, id785E=0x102A785E,
            id7860=0x102A7860, id7862=0x102A7862, id7864=0x102A7864)
IDS = ["id7858", "id785A", "id785C", "id785E", "id7860", "id7862", "id7864"]
HOST_LAYOUT = [0x0E, 0x28, 0x2E, 0x1DA73C, 0x1DA73E]
WORLD_OFF = 0x1DA73C

CUR = {"ride": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def plain(name, addr, bool_ret=False):
    def f(e):
        e.cl.rec(name, ())
        e.set_r(3, e.cl.ret(name) & MASK)
    STUBS.internal(addr, f)


for _n, _a in (("IsCondemned", 0x1010C8BC), ("IsTrackClosed", 0x10052850), ("IsTrackValid", 0x100528D8),
               ("RequiresUpgrade", 0x1010C9C0), ("IsBeingWorkedUpon", 0x1010C6D8), ("IsFlaggedForMaintenance", 0x1010C980),
               ("IsBrokenDown", 0x1010C878), ("IsBOQConnected", 0x10109764), ("IsClosed", 0x1010CAE8),
               ("IsExitCellConnected", 0x10109960)):
    plain(_n, _a)


def stub_ride(e):
    e.cl.rec("GetRideInfo", ((e.r(4) >> 16) & 0xFFFF,))
    e.set_r(3, CUR["ride"])


def stub_hq(e):
    e.cl.rec("GetStaffHQ", ())
    e.set_r(3, 0x1000)


def stub_striking(e):
    e.cl.rec("IsStriking", (T.s32(e.r(4)),))
    e.set_r(3, e.cl.ret("IsStriking") & MASK)


def hidden(name):
    def f(e):
        e.cl.rec(name, ())
        e.write(e.r(3), struct.pack(">H", e.cl.ret(name) & 0xFFFF))
    return f


STUBS.internal(0x10154868, stub_ride)
STUBS.internal(0x1013E99C, stub_hq)
STUBS.internal(0x1012BDE0, stub_striking)
STUBS.internal(0x1010C5A0, hidden("GetAssignedStaffMember"))
STUBS.internal(0x1011050C, hidden("UpgradesWhat"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("Q" if self.fmt == "<I" else "I"), v))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


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

    def setride(self, a):
        CUR["ride"] = a

    def call(self, obj, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK, flag & MASK])
        return T.s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gos_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gos_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setride(self, a):
        self.lib.tpw_gos_setride(a)

    def call(self, obj, flag):
        return self.lib.tpw_gos_state(obj, flag)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gos_addr.restype, lib.tpw_gos_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gos_alloc.restype, lib.tpw_gos_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gos_setride.restype, lib.tpw_gos_setride.argtypes = None, [C.c_uint64]
    lib.tpw_gos_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gos_state.restype, lib.tpw_gos_state.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gos_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for name in IDS:
        s.half(s.g(name), case[name])
    obj = s.alloc(0x40)
    s.half(obj + 0x0E, case["type"])
    s.words(obj + 0x28, [case["f28"]])
    s.half(obj + 0x2E, case["flags"])
    ride = s.alloc(0x7C0)
    s.words(ride + 0x9C, [case["kind"]])
    s.words(ride + 0x288, [case["r288"]])
    s.words(ride + 0x7A8, [case["r7a8"]])
    s.setride(ride)
    wbase = s.alloc(0x1DA748)
    s.half(wbase + WORLD_OFF, case["w3c"])
    s.half(wbase + WORLD_OFF + 2, case["w3e"])
    s.ptr(s.g("world"), wbase)
    return obj


def run(s, case):
    try:
        obj = build(s, case)
        ans = s.call(obj, case["flag"])
        d = {"answer": ans, "log": s.log()}
        for name in IDS:
            d[name] = s.rd_half(s.g(name))
        return d
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


QKEYS = ["IsCondemned", "IsTrackClosed", "IsTrackValid", "RequiresUpgrade", "IsStriking", "IsBeingWorkedUpon",
         "IsFlaggedForMaintenance", "IsBrokenDown", "IsBOQConnected", "IsClosed", "IsExitCellConnected"]


def case_(r, bias):
    c = {}
    for name in IDS:
        c[name] = r.choice([0, 1, 2])
    c["w3c"] = r.choice([0, 1, 2])
    c["w3e"] = r.choice([0, 1, 2])
    c["type"] = r.randrange(0, 0x10000)
    c["f28"] = r.choice([0, 1, 5])
    c["flags"] = r.choice([0, 1, 8, 9, 0xFFFF, 0x10, 0x7FF6])
    c["kind"] = r.choice([-1, 0, 1, 2, 3, 4, 3, 1])
    c["r288"] = r.choice([0, 1, 2])
    c["r7a8"] = r.choice([0, 0, 1])
    c["flag"] = r.choice([0, 1, 2, 3, -1])
    q = {}
    for k in QKEYS:
        q[k] = [1 if r.random() < bias.get(k, 0.5) else 0]
    q["GetAssignedStaffMember"] = [r.choice([0, 1, 2])]
    q["UpgradesWhat"] = [r.choice([0, 1, 2])]
    c["queues"] = q
    return c


def edges():
    r = random.Random(694)
    for i in range(600):
        yield case_(r, {"IsCondemned": 0.1, "IsTrackClosed": 0.8, "IsTrackValid": 0.8, "RequiresUpgrade": 0.3,
                        "IsFlaggedForMaintenance": 0.4, "IsBrokenDown": 0.4, "IsBOQConnected": 0.4, "IsStriking": 0.3})


def rnd(r):
    return case_(r, {})


TESTS = [
    FnTest("UIMain_GetObjectState", ppc, host, edge=edges, rnd=rnd),
]
