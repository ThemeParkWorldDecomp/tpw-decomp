"""Unit v5539, NameWindow and TankButton of the research panel, against libtpw_unit_NameWindow.dylib (tests/unit_NameWindow.cpp), main app
("SimTheme Park"), src/DoScroll_NameWindow.cpp and src/CreateResearchPanel_RideMsg.cpp:
  NameWindow 0x101b3da8, TankButton 0x101b4224

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (the world's research lab, the lab's
project calls, GetWindow, the ride database, FormatString, SetClientDrawText, UIMain_GetString, ResearchSpeed, PercentComplete) are fakes on
both sides that record their arguments.

The world of a case: gGameWorld, gResearchPanel, the text table slot; answers of the fakes from the queues (project id, ride found or not,
cost) and whether the child window exists. What is compared: the event log (every call with its arguments, FormatString's tags and the
text handed to SetClientDrawText).

No fused operations. Not tested: the parent window missing (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_NameWindow"
MASK = 0xFFFFFFFF

FN_NAME, FN_TANK = 0x101B3DA8, 0x101B4224
SIZES = {FN_NAME: 564, FN_TANK: 120}
OWN = set(SIZES)

A_WORLD, A_PANEL, A_TABLE, A_RIDEDB = 0x1037EBFC, 0x10395980, 0x1037FF18 + 0x564, 0x1037F6C8
ADDR = dict(world=A_WORLD, panel=A_PANEL, table=A_TABLE, ridedb=A_RIDEDB)
HOST_LAYOUT = [2]

CUR = {}
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_current(e, log):
    """Hidden result pointer r3 receives the project id (a halfword, big-endian)."""
    v = log.ret("GetCurrentProject") & 0xFFFF
    e.write(e.r(3), struct.pack(">H", v))
    return e.r(3)


def stub_typename(e, log):
    ident = (e.r(4) >> 16) & 0xFFFF
    e.write(e.r(5), struct.pack(">3H", 0x4E + (ident & 7), 0x58, 0))
    return e.r(3)


def stub_getwindow(e, log):
    o = CUR["o"]
    return o["parent"] if e.r(3) == o["panel"] else o["child"]


def stub_getstring(e, log):
    return CUR["o"]["unk"]


def stub_world(e, log):
    return CUR["o"]["lab"]


def stub_format(e):
    L = e.cl
    tbl, out, size, ident, ta, tb = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7), e.r(8)
    a = struct.unpack(">3I", e.read(ta, 12))
    b = struct.unpack(">4I", e.read(tb, 16))
    txt = struct.unpack(">3H", e.read(e.u32(ta + 12), 6))
    L.rec("FormatString", (L.lab(tbl), size & MASK, ident & MASK, a[0], a[2], b[0], b[2], s32(b[3]), txt[0], txt[1], txt[2]))
    e.write(out, struct.pack(">3H", 0x46, 0x30 + (s32(b[3]) & 7), 0))
    e.set_r(3, 0)


STUBS.internal(0x1013ECE8, CL.recorder("GetResearchLab", "L", ret=stub_world))
STUBS.internal(0x10122AF4, CL.recorder("GetCurrentProject", "xLi", ret=stub_current))
STUBS.internal(0x10122B7C, CL.recorder("GetEstimatedCostForCurrentProject", "Li", ret="i"))
STUBS.internal(0x101223E8, CL.recorder("NextProject", "Li"))
STUBS.internal(0x101225F0, CL.recorder("PreviousProject", "Li"))
STUBS.internal(0x10154868, CL.recorder("GetRideInfo", "Lw", ret="i"))
STUBS.internal(0x10154AF0, CL.recorder("GetTypeName", "Lwxu", ret=stub_typename))
STUBS.internal(0x101DBE40, CL.recorder("GetWindow", "Li", ret=stub_getwindow))
STUBS.internal(0x101DE130, CL.recorder("SetClientDrawText", "LS"))
STUBS.internal(0x10180468, CL.recorder("UIMain_GetString", "i", ret=stub_getstring))
STUBS.internal(0x101B381C, CL.recorder("ResearchSpeed", "i"))
STUBS.internal(0x101B3398, CL.recorder("PercentComplete", "i"))
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def ptr(self, a, v):
        """A pointer cell: 4 bytes on the Mac, 8 on the host (the host arena is not 32-bit addressable here)."""
        self.write(a, struct.pack(">I", v & MASK) if not self.host else struct.pack("<Q", v))


class Ppc(Side):
    fmt = ">I"
    host = False

    def __init__(self, e, case):
        self.e = e
        o = {k: e.alloc(32) for k in ("world", "lab", "panel", "parent", "child", "tbl")}
        o["unk"] = e.alloc(16)
        e.write(o["unk"], struct.pack(">4H", 0x55, 0x6E, 0x6B, 0))
        CUR["o"] = self.o = o
        fixed = {o["world"]: 1, o["lab"]: 2, o["panel"]: 3, o["parent"]: 4, o["child"]: 5, A_RIDEDB: 6, o["tbl"]: 7}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def name(self, group):
        with_ = T.call_ppc(self.e, FN_NAME, [group & MASK])
        return 0

    def tank(self, group, window, flag):
        T.call_ppc(self.e, FN_TANK, [group & MASK, window & MASK, flag & MASK])
        return 0


class Host(Side):
    fmt = "<I"
    host = True

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        o = {k: lib.tpw_nw_alloc(32) for k in ("world", "lab", "panel", "parent", "child", "tbl")}
        self.o = o
        fixed = {1: o["world"], 2: o["lab"], 3: o["panel"], 4: o["parent"], 5: o["child"], 6: lib.tpw_nw_addr(b"ridedb"),
                 7: o["tbl"]}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_nw_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def name(self, group):
        self.lib.tpw_nw_name(s32(group))
        return 0

    def tank(self, group, window, flag):
        self.lib.tpw_nw_tank(s32(group), window, s32(flag))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_nw_addr.restype, lib.tpw_nw_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_nw_alloc.restype, lib.tpw_nw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_nw_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_nw_name.restype, lib.tpw_nw_name.argtypes = None, [C.c_int32]
    lib.tpw_nw_tank.restype, lib.tpw_nw_tank.argtypes = None, [C.c_int32, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_nw_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.o
    s.ptr(s.g("world"), o["world"])
    s.ptr(s.g("panel"), o["panel"])
    s.ptr(s.g("table"), o["tbl"])
    if s.host:
        s.ptr(s.g("labptr"), o["lab"])
        s.ptr(s.g("parentptr"), o["parent"])
        s.ptr(s.g("childptr"), o["child"] if case["child"] else 0)
    else:
        if not case["child"]:
            o["child"] = 0


def execute(s, case):
    if case["op"] == "name":
        return s.name(case["group"])
    return s.tank(case["group"], 0x1234, case["flag"])


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, group, ride=None, child=None):
    return {"op": op, "group": group, "flag": r.choice([0, 1, 5, -1]), "child": r.choice([0, 1]) if child is None else child,
            "queues": {"GetRideInfo": [r.choice([0, 1]) if ride is None else ride],
                       "GetCurrentProject": [r.choice([0, 1, 0xFFFF, r.randrange(0x10000)])],
                       "GetEstimatedCostForCurrentProject": [r.choice([0, -1, 1000000, r.randrange(-100000, 100000)])]}}


GROUPS = [-2, -1, 0, 1, 2, 3, 4, 5, 6, 99]


def edges(op):
    def gen():
        r = random.Random(7 if op == "name" else 8)
        for g in GROUPS:
            for ride in (0, 1):
                for child in (0, 1):
                    yield base(r, op, g, ride, child)
    return gen


def rnd(op):
    return lambda r: base(r, op, r.choice(GROUPS))


TESTS = [
    FnTest("NameWindow", ppc, host, edge=edges("name"), rnd=rnd("name")),
    FnTest("TankButton", ppc, host, edge=edges("tank"), rnd=rnd("tank")),
]
