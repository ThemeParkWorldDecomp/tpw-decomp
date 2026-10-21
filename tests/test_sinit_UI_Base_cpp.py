"""Unit v5295, UI_ToggleHelpText and the static initialiser of UI_Base.cpp, against libtpw_unit_sinit_UI_Base_cpp.dylib
(tests/unit_sinit_UI_Base_cpp.cpp), main app ("SimTheme Park"), src/game/ui/UI_Base.cpp:
  UI_ToggleHelpText 0x101ca9d0, __sinit_UI_Base_cpp 0x101cac94

A case is a world and a call. The world is built by the same Python code on both sides. Callees outside the unit are fakes that record
events: InterfaceHelpText's constructor, __register_global_object, and the two virtuals (Enable, Disable) reached through __ptr_glue.

The world of a case: for the toggle, the handler object with a random mEnabled (and garbage in the half after it) and its vtable;
for the initialiser, the clock's vtable word cleared. Compared: the event log and, for the initialiser, whether the clock's vtable word
was stored (the Mac's value is checked to be 0x102af94c, the host's to be non-null).

No fused operations. Not tested: the exact vtable values of the host (they are native pointers).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_UI_Base_cpp"
MASK = 0xFFFFFFFF

FN_TOGGLE, FN_SINIT = 0x101CA9D0, 0x101CAC94
SIZES = {FN_TOGGLE: 84, FN_SINIT: 100}
OWN = set(SIZES)

CTOR = 0x101CF8FC
REGISTER = 0x10239940
PTR_GLUE = 0x1023BAA8
TV_ENABLE, TV_DISABLE = 0x10300001, 0x10300002

ADDR = dict(default=0x10395B8C, dtor=0x10268238, node=0x10395BA4, clock=0x10395B74, helptext=0x102AF928)
PPC_OBJ = [0]
PPC_VT = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    e.cl.rec("InterfaceHelpText", (e.cl.lab(e.r(3)),))


def stub_register(e):
    L = e.cl
    L.rec("register_global_object", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


def stub_glue(e):
    name = {TV_ENABLE: "Enable", TV_DISABLE: "Disable"}.get(e.r(12), "Unknown")
    e.cl.rec(name, (e.cl.lab(e.r(3)),))


STUBS.internal(CTOR, stub_ctor)
STUBS.internal(REGISTER, stub_register)
STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)
    PPC_OBJ[0] = T.persist_alloc(runner.emu, 0x40)
    PPC_VT[0] = T.persist_alloc(runner.emu, 0x40)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["default"]: 1, ADDR["dtor"]: 2, ADDR["node"]: 3, PPC_OBJ[0]: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_help(self, enabled, junk):
        obj, vt = PPC_OBJ[0], PPC_VT[0]
        self.words(vt, [0] * 16)
        self.words(vt + 0x14, [TV_ENABLE, TV_DISABLE])
        self.words(obj, [((enabled & 0xFFFF) << 16) | junk, 0, 0, 0, vt, 0])
        self.words(ADDR["helptext"], [obj])

    def clock_clear(self):
        self.words(ADDR["clock"], [0])

    def clock_set(self):
        return int(self.e.u32(ADDR["clock"]) == 0x102AF94C)

    def call(self, op):
        with_fn = FN_TOGGLE if op == "toggle" else FN_SINIT
        T.call_ppc(self.e, with_fn, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        a = lambda n: lib.tpw_tsb_addr(n.encode())
        CL.host_begin(lib, {1: a("default"), 2: a("dtor"), 3: a("node"), 4: a("obj")}, case["queues"])

    def done(self):
        pass

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_help(self, enabled, junk):
        self.lib.tpw_tsb_set_help(T.s32(enabled))

    def clock_clear(self):
        self.lib.tpw_tsb_clock_clear()

    def clock_set(self):
        return self.lib.tpw_tsb_clock_set()

    def call(self, op):
        (self.lib.tpw_tsb_toggle if op == "toggle" else self.lib.tpw_tsb_sinit)()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tsb_addr.restype, lib.tpw_tsb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tsb_alloc.restype, lib.tpw_tsb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_tsb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tsb_set_help.argtypes = [C.c_int32]
    lib.tpw_tsb_clock_set.restype = C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_tsb_layout(out)
    assert list(out)[:n] == [], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        if case["op"] == "toggle":
            s.set_help(case["enabled"], case["junk"])
            s.call("toggle")
            return {"log": s.log()}
        s.clock_clear()
        s.call("sinit")
        return {"clock": s.clock_set(), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def toggle_case(r, enabled=None):
    return {"op": "toggle", "queues": {}, "enabled": r.choice([0, 1, 0xFFFF, 0x8000, r.randrange(0x10000)]) if enabled is None else enabled,
            "junk": r.randrange(0x10000)}


def toggle_edges():
    r = random.Random(11)
    for v in (0, 1, 0xFFFF, 0x8000, 0x7FFF, 2, 0x100):
        yield toggle_case(r, v)


def toggle_random(r):
    return toggle_case(r)


def sinit_case(r):
    return {"op": "sinit", "queues": {}}


def sinit_edges():
    yield sinit_case(None)


TESTS = [
    FnTest("UI_ToggleHelpText", ppc, host, edge=toggle_edges, rnd=toggle_random),
    FnTest("__sinit_UI_Base_cpp", ppc, host, edge=sinit_edges, rnd=sinit_case),
]
