"""Unit v5264, InterfaceKnob::ChangeValue, against libtpw_unit_InterfaceKnob_ChangeValue.dylib (tests/unit_InterfaceKnob_ChangeValue.cpp), main app
("SimTheme Park"), src/InterfaceKnob_InterfaceKnob_ChangeValue.cpp:
  InterfaceKnob::ChangeValue 0x101d0448

A case is a knob with its ten range/value words, an id, a parent and (or not) a client draw object, and a new value. The knob is built
twice by one piece of Python: Mac offsets in emulator memory, and through a host setter (the host object has native pointers).
UI_PostMessage is a fake on both sides recording (window label, message, a, b). The client draw object's Change is reached through
the vtable at +0x38 slot 0x14 and __ptr_glue (faked on the PowerPC side): it records (object label, flags, the short at +8 of the
record, r12 less the expected descriptor); the host fake records the same with 0 for the last.

What is compared: the ten words of the knob and the event log. No fused operations. Not tested: a value range of 0 reaching the
division (the emulator answers the dividend, the hardware is undefined).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceKnob_ChangeValue"
MASK = 0xFFFFFFFF

FN = 0x101d0448
SIZES = {FN: 248}
OWN = set(SIZES)
UI_POST = 0x101c9400
PTR_GLUE = 0x1023baa8
DESC = 0x12345670
HOST_LAYOUT = [8]
OFFS = [0x138, 0x13c, 0x140, 0x144, 0x148, 0x14c, 0x150, 0x154, 0x158, 0x15c]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(UI_POST, CL.recorder("UI_PostMessage", "Liii"))


def stub_glue(e):
    L = e.cl
    off = struct.unpack(">h", e.read(e.r(5) + 8, 2))[0]
    L.rec("Change", (L.lab(e.r(3)), e.r(4) & MASK, off, (e.r(12) - DESC) & MASK))


STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.parent = e.alloc(0x40)
        self.dobj = e.alloc(0x60) if case["hasdo"] else 0
        self.knob = e.alloc(0x170)
        fixed = {self.parent: 1}
        if self.dobj:
            fixed[self.dobj] = 2
        self.L = e.cl = CL.PpcLog(fixed, {})
        self.saved = STUBS.activate(e)

    def words(self, a, ws):
        self.e.write(a, b"".join(struct.pack(">I", w & MASK) for w in ws))

    def set_knob(self, case):
        self.words(self.knob + 0x5c, [self.parent])
        self.words(self.knob + 0x88, [case["id"]])
        self.words(self.knob + 0x98, [self.dobj])
        for o, v in zip(OFFS, case["f"]):
            self.words(self.knob + o, [v])
        if self.dobj:
            vt = self.e.alloc(0x40)
            self.words(vt + 0x14, [DESC])
            self.words(self.dobj + 0x38, [vt])

    def call(self, v):
        T.call_ppc(self.e, FN, [self.knob, v & MASK])

    def fields(self):
        return [T.s32(struct.unpack(">I", self.e.read(self.knob + o, 4))[0]) for o in OFFS]

    def log(self):
        return self.L.log

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.parent = lib.tpw_ik_alloc(0x40)
        self.dobj = lib.tpw_ik_newdo() if case["hasdo"] else 0
        self.knob = lib.tpw_ik_newknob()
        fixed = {1: self.parent}
        if self.dobj:
            fixed[2] = self.dobj
        CL.host_begin(lib, fixed, {})

    def set_knob(self, case):
        arr = (C.c_int32 * 10)(*[T.s32(v) for v in case["f"]])
        self.lib.tpw_ik_set(self.knob, self.parent, T.s32(case["id"]), self.dobj, arr)

    def call(self, v):
        self.lib.tpw_ik_ChangeValue(self.knob, T.s32(v))

    def fields(self):
        arr = (C.c_int32 * 10)()
        self.lib.tpw_ik_get(self.knob, arr)
        return list(arr)

    def log(self):
        return CL.host_end(self.lib)

    def done(self):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ik_alloc.restype, lib.tpw_ik_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ik_newknob.restype, lib.tpw_ik_newknob.argtypes = C.c_uint64, []
    lib.tpw_ik_newdo.restype, lib.tpw_ik_newdo.argtypes = C.c_uint64, []
    lib.tpw_ik_set.restype, lib.tpw_ik_set.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64, C.POINTER(C.c_int32)]
    lib.tpw_ik_get.restype, lib.tpw_ik_get.argtypes = None, [C.c_uint64, C.POINTER(C.c_int32)]
    lib.tpw_ik_ChangeValue.restype, lib.tpw_ik_ChangeValue.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_ik_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ik_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.set_knob(case)
        s.call(case["value"])
        return {"fields": s.fields(), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, minv, maxv, value, new, hasdo, vr=None, minA=None, maxA=None):
    minA = r.choice([0, 0, 100, -50, 7]) if minA is None else minA
    maxA = r.choice([0xfff, 0xfff, 500, 4000, 1000]) if maxA is None else maxA
    vr = (maxv - minv) if vr is None else vr
    return {"id": r.choice([0, 5, 77, -3]), "hasdo": hasdo, "value": new,
            "f": [minA, maxA, maxA - minA, r.randrange(0, 4096), r.randrange(0, 4096), minv, maxv, vr, value, r.randrange(-9, 9)]}


def rnd(r):
    minv = r.choice([0, 0, -10, 5, -1000])
    maxv = minv + r.choice([1, 2, 10, 100, 1000, 4095])
    if r.random() < 0.1:
        maxv = minv - r.choice([1, 5])
    value = r.choice([minv, maxv, r.randrange(min(minv, maxv), max(minv, maxv) + 1)])
    new = r.choice([minv, maxv, minv - 1, maxv + 1, value, r.randrange(minv - 20, maxv + 20), r.randrange(-100000, 100000)])
    vr = None
    if r.random() < 0.15:
        vr = r.choice([1, 3, -7, 50])
    return make(r, minv, maxv, value, new, r.random() < 0.7, vr)


def edges():
    r = random.Random(264)
    for hasdo in (True, False):
        for new in (-5, 0, 1, 5, 9, 10, 11, 20, 3, 7):
            yield make(r, 0, 10, 4, new, hasdo)
        yield make(r, 0, 10, 10, 10, hasdo)
        yield make(r, 0, 10, 0, -1, hasdo)
        yield make(r, 0, 10, 0, 3, hasdo)
        yield make(r, 0, 10, 3, 3, hasdo)
        yield make(r, 5, 5, 5, 5, hasdo)
        yield make(r, 5, 5, 4, 9, hasdo)
        yield make(r, -10, 10, 0, 5, hasdo)
        yield make(r, 0, 3, 1, 2, hasdo, minA=0, maxA=0xfff)
        yield make(r, 0, 3, 1, 2, hasdo, minA=0, maxA=0x8000)
        yield make(r, 10, 0, 5, 3, hasdo)
        yield make(r, 10, 0, 5, 20, hasdo)


TESTS = [
    FnTest("InterfaceKnob::ChangeValue", ppc, host, edge=edges, rnd=rnd),
]
