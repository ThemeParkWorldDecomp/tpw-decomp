"""Unit v5343, the staff and kid locator slide-out panels, against libtpw_unit_ShowKidLocatorPanel.dylib (tests/unit_ShowKidLocatorPanel.cpp),
main app ("SimTheme Park"), src/ui_staffPanel_ShowKidLocatorPanel.cpp:
  ShowStaffPanel 0x101c0690, ShowKidLocatorPanel 0x101c0a38

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory for the original, host memory for the
translation). The callees outside the unit (LinkSlideoutPanel, GetWindow, Show, link, SetCurrent, UpdateLocator, UIMain_CheckPanel,
UIMain_AdvisorMessage, CMMDefault::CMMDefault, CMouse::AssignMouseMode, the virtual GetMode through __ptr_glue) are fakes on both sides that
record their arguments; GetWindow answers its own object. operator new is real on both sides (not logged: the sizes differ).

The world of a case: the four windows, the thing (kind byte), the location, state, first-person flags, the mouse (NULL or preset), the mode
object (NULL or fake) and its answer. Compared: the globals written and the event log.

No fused operations. Not tested: NULL window globals (the original reads through them unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowKidLocatorPanel"
MASK = 0xFFFFFFFF

FN_STAFF, FN_KID = 0x101C0690, 0x101C0A38
SIZES = {FN_STAFF: 892, FN_KID: 504}
OWN = set(SIZES)

LINK, GETWIN, SETCUR, SHOW, LINKFN = 0x101A82EC, 0x101DBE40, 0x101CC148, 0x101DC1EC, 0x101DDF20
UPDATE, ADVISOR, CHECK = 0x101BF798, 0x101821CC, 0x10180514
CTOR, ASSIGN, GLUE = 0x1007EA78, 0x1007E2E0, 0x1023BAA8
TABLE = 0x102AE3C4

ADDR = dict(flags=0x102B5A40, w1e0=0x102DC1E0, w1e4=0x102DC1E4, mode=0x102DC1F0, mouse=0x102DC1F4, main=0x103949D8,
            loc=0x10395A34, state=0x10395A38, thing=0x10395A3C, C=0x10395A40, B=0x10395A44, A=0x10395A48)
HOST_LAYOUT = [2]
# labels
L_MAIN, L_A, L_B, L_C, L_THING, L_MODE, L_MOUSE = 1, 2, 3, 4, 5, 6, 7

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    L = e.cl
    L.rec("GetMode", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetMode") & MASK)


STUBS.internal(LINK, CL.recorder("LinkSlideoutPanel", "Li"))
STUBS.internal(GETWIN, CL.recorder("GetWindow", "Li", lambda e, log: e.r(3)))
STUBS.internal(SETCUR, CL.recorder("SetCurrent", "Li"))
STUBS.internal(SHOW, CL.recorder("Show", "Lh"))
STUBS.internal(LINKFN, CL.recorder("link", "LLbb"))
STUBS.internal(UPDATE, CL.recorder("UpdateLocator", ""))
STUBS.internal(CHECK, CL.recorder("CheckPanel", ""))
STUBS.internal(ADVISOR, CL.recorder("Advisor", "i"))
STUBS.internal(CTOR, CL.recorder("CMMDefault", "N"))
STUBS.internal(ASSIGN, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def setup_objects(self, case):
        self.objs = {k: self.alloc(0x40) for k in ("main", "A", "B", "C", "thing", "mouse")}
        self.write(self.objs["thing"] + 2, bytes([(case.get("kind") or 0) & 0xFF]))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.setup_objects(case)
        o = self.objs
        self.mode_obj = self.alloc(0x40)
        vt = self.alloc(0x80)
        self.words(self.mode_obj, [vt])
        self.words(vt + 0x2C, [0x1000])
        self.L = e.cl = CL.PpcLog({o["main"]: L_MAIN, o["A"]: L_A, o["B"]: L_B, o["C"]: L_C, o["thing"]: L_THING,
                                   self.mode_obj: L_MODE, o["mouse"]: L_MOUSE}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def mode_addr(self):
        return self.mode_obj

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def staff(self, thing):
        T.call_ppc(self.e, FN_STAFF, [thing])

    def kid(self):
        T.call_ppc(self.e, FN_KID, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.setup_objects(case)
        o = self.objs
        self.fake = lib.tpw_sp_addr(b"fakemode")
        CL.host_begin(lib, {L_MAIN: o["main"], L_A: o["A"], L_B: o["B"], L_C: o["C"], L_THING: o["thing"],
                            L_MODE: self.fake, L_MOUSE: o["mouse"]}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sp_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def mode_addr(self):
        return self.fake

    def alloc(self, n):
        return self.lib.tpw_sp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def staff(self, thing):
        self.lib.tpw_sp_staff(thing)

    def kid(self):
        self.lib.tpw_sp_kid()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sp_addr.restype, lib.tpw_sp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sp_alloc.restype, lib.tpw_sp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sp_staff.restype, lib.tpw_sp_staff.argtypes = None, [C.c_uint64]
    lib.tpw_sp_kid.restype, lib.tpw_sp_kid.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.objs
    s.ptr(s.g("main"), o["main"])
    s.ptr(s.g("A"), o["A"])
    s.ptr(s.g("B"), o["B"])
    s.ptr(s.g("C"), o["C"])
    s.ptr(s.g("thing"), 0)
    s.ptr(s.g("mouse"), o["mouse"] if case["mouse"] else 0)
    s.ptr(s.g("mode"), s.mode_addr() if case["mode"] else 0)
    s.words(s.g("loc"), [case["loc"]])
    s.words(s.g("state"), [case["state"]])
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("w1e0"), [case["w0"]])
    s.words(s.g("w1e4"), [case["w4"]])


def execute(s, case):
    if case["op"] == "staff":
        s.staff(0 if case["kind"] is None else s.objs["thing"])
    else:
        s.kid()


def observe(s, case):
    o = s.objs
    m = s.rd_word(s.g("mouse"))
    return {"log": s.log(),
            "loc": s.rd_word(s.g("loc")), "state": s.rd_word(s.g("state")), "flags": s.rd_word(s.g("flags")),
            "w0": s.rd_word(s.g("w1e0")), "w4": s.rd_word(s.g("w1e4")),
            "thing": 0 if s.rd_word(s.g("thing")) == 0 else (1 if s.rd_word(s.g("thing")) == o["thing"] & MASK else 2),
            "mouse": 0 if m == 0 else (1 if m == o["mouse"] & MASK else 2)}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "kind": None, "mouse": r.choice([0, 1]), "mode": r.choice([0, 1]),
            "loc": r.choice([0, 6, 3, r.randrange(0, 8)]), "state": r.randrange(0, 5), "flags": r.choice([0, 0x1000, r.getrandbits(32)]),
            "w0": r.getrandbits(32), "w4": r.getrandbits(32),
            "queues": {"GetMode": [r.choice([9, 9, 1, 0, 8, 10])]}}


def staff_case(r, kind="r"):
    c = base(r, "staff")
    c["kind"] = r.choice([None, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0x13, 255]) if kind == "r" else kind
    c["show_table"] = True
    return c


def staff_edges():
    r = random.Random(4301)
    for k in [None, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 255]:
        for mouse in (0, 1):
            for mode, ans in ((0, 9), (1, 9), (1, 1)):
                c = staff_case(r, k)
                c["mouse"], c["mode"] = mouse, mode
                c["queues"] = {"GetMode": [ans]}
                yield c


def kid_case(r):
    return base(r, "kid")


def kid_edges():
    r = random.Random(4302)
    for mouse in (0, 1):
        for mode, ans in ((0, 9), (1, 9), (1, 1)):
            c = kid_case(r)
            c["mouse"], c["mode"] = mouse, mode
            c["queues"] = {"GetMode": [ans]}
            yield c


TESTS = [
    FnTest("ShowStaffPanel", ppc, host, edge=staff_edges, rnd=staff_case),
    FnTest("ShowKidLocatorPanel", ppc, host, edge=kid_edges, rnd=kid_case),
]
