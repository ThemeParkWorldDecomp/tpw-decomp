"""Unit v5294, SOUND_SetRect, against libtpw_unit_SOUND_SetRect.dylib (tests/unit_SOUND_SetRect.cpp), main app
("SimTheme Park"), src/SoundRuntime_SOUND_SetRect.cpp:
  SOUND_SetRect 0x100de828

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, host memory). The only callee,
the virtual SystemCommand of the audio object (slot +0x20, reached through __ptr_glue 0x1023baa8), is a fake on both sides that
records the command it is given: whether the call came through the slot and the object, the command's vtable (PowerPC: the
SetBoundingRectangle import vtable 0x103bd2c0; host: the dynamic type), handle, left, top, right, bottom.

The world of a case: the wanted word and the system-on word (0 or nonzero), the audio object (always present). What is compared:
the event log. No fused operations. Not tested: a NULL audio object with both words set (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SOUND_SetRect"
MASK = 0xFFFFFFFF

FN = 0x100DE828
SIZES = {FN: 148}
OWN = set(SIZES)
PTR_GLUE = 0x1023BAA8

ADDR = dict(wanted=0x102F7C38, on=0x102F7C3C, audio=0x102F7C34)
HOST_LAYOUT = [4]
SLOT = 0xABCD0001
VT_SLOT = 0x1026114C     # the TOC slot holding the import vtable's address (resolved by the emulator)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    """The virtual call SystemCommand(command) of the audio object (r4 object, r5 command, r12 the slot)."""
    L = e.cl
    obj = e.r(4)
    cmd = e.r(5)
    w = struct.unpack(">6I", e.read(cmd, 24))
    ok_obj = 1 if obj == e.u32(ADDR["audio"]) else 0
    ok_slot = 1 if e.r(12) == SLOT else 0
    if w[0] == e.u32(VT_SLOT):
        L.rec("SystemCommand", (1, ok_obj & ok_slot, s32(w[1]), s32(w[2]), s32(w[3]), s32(w[4]), s32(w[5])))
    else:
        L.rec("SystemCommand", (0,))


STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


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

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, args):
        vt = self.e.alloc(0x40)
        obj = self.e.alloc(8)
        self.words(vt + 0x20, [SLOT])
        self.words(obj, [vt])
        self.words(ADDR["audio"], [obj])
        T.call_ppc(self.e, FN, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sr_addr(name.encode())

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, args):
        self.lib.tpw_sr_setrect(*[s32(a) for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sr_addr.restype, lib.tpw_sr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sr_alloc.restype, lib.tpw_sr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sr_setrect.restype, lib.tpw_sr_setrect.argtypes = None, [C.c_int32] * 5
    out = (C.c_int64 * 32)()
    n = lib.tpw_sr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("wanted"), [case["wanted"]])
    s.words(s.g("on"), [case["on"]])


def run(s, case):
    try:
        build(s, case)
        s.call(case["args"])
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


VALS = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 100, -100, 0x40000000]


def mk(r, wanted, on, args):
    return {"queues": {}, "wanted": wanted, "on": on, "args": [a & MASK for a in args]}


def edges():
    r = random.Random(94)
    for wanted, on in ((0, 0), (0, 1), (1, 0), (1, 1), (0x80000000, 1), (1, 0xFFFFFFFF)):
        yield mk(r, wanted, on, [7, 10, 20, 30, 40])
    yield mk(r, 1, 1, [0, 0, 0, 0, 0])
    yield mk(r, 1, 1, [-1, -1, -1, -1, -1])
    for a in VALS:
        for b in VALS:
            yield mk(r, 1, 1, [a, b, b, a, b])
            yield mk(r, 1, 1, [b, a, 5, b, a])


def rnd(r):
    return mk(r, r.choice([0, 1, 5]), r.choice([0, 1, 1, 1]), [r.choice([r.randrange(1 << 32), r.choice(VALS)]) for _ in range(5)])


TESTS = [
    FnTest("SOUND_SetRect", ppc, host, edge=edges, rnd=rnd),
]
