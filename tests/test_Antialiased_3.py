"""Unit v5940, the antialiased 3D line for four and three bytes per pixel, against libtpw_unit_Antialiased_3.dylib
(tests/unit_Antialiased_3.cpp), main app ("SimTheme Park"), src/game/ui/3DFont.cpp:
  Antialiased<CTraits<4>> 0x100058c0, Antialiased<CTraits<3>> 0x10005e30

A case is a render area (format words, data pointer, stride), a clip rectangle, a colour and two points. The world is written to emulator
memory for the original and passed as C values to the host entry point. The callees outside the unit are fakes on both sides that record
what they read: CColour::GetFormattedValue (the colour bytes and the six format words, answers from the case's queue) and
IntensifyPix<N> (the pixel offset from the data pointer, the alpha as double bits, the colour, the four area words, x, y, and the clip).
sqrt is the real one on both sides.

What is compared: the whole event log. No fused operations. Not tested: nothing excluded; the pixel memory is never touched
(IntensifyPix is a fake), so the data pointer is a plain number.
"""
import ctypes as C
import math
import random
import struct

import tpwtest as T
import calllog as CL
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Antialiased_3"
MASK = 0xFFFFFFFF

FN = {4: 0x100058C0, 3: 0x10005E30}
PIX = {4: 0x100073B8, 3: 0x100071F4}
GFV = 0x1023AF18
DATA = 0x20000000


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(FN.values())


def stub_gfv(e):
    L = e.cl
    c = e.u32(e.r(3))
    f = [e.u32(e.r(4) + 4 * i) for i in range(6)]
    L.rec("GetFormattedValue", (c, f[0], f[1], f[2], f[3], s32(f[4]), s32(f[5])))
    e.set_r(3, L.ret("GetFormattedValue") & MASK)


def stub_pix(e):
    L = e.cl
    sp = e.r(1)
    bits = T.f64_bits(e.f(1))
    m3, x, y, clip = [e.u32(sp + o) for o in (0x38, 0x3C, 0x40, 0x44)]
    off = s32(e.r(4) - DATA)
    L.rec("Pix", (off, bits & MASK, bits >> 32, e.r(7), e.r(8), e.r(9), e.r(10), m3, s32(x), s32(y)))
    L.rec("PixClip", tuple(s32(e.u32(clip + 4 * i)) for i in range(4)))


def keep_toc(fn):
    """The glue the original calls saves the TOC pointer at 0x14(r1); the original reloads r2 from there after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


def stub_sqrt(e):
    e.set_f1(math.sqrt(e.f(1)))


STUBS.internal(GFV, keep_toc(stub_gfv))
STUBS.internal(0x10239250, keep_toc(stub_sqrt))
for _a in PIX.values():
    STUBS.internal(_a, keep_toc(stub_pix))


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_aa_run.restype = None
    lib.tpw_aa_run.argtypes = [C.c_int32, C.POINTER(C.c_uint32), C.c_uint64, C.c_int32, C.POINTER(C.c_int32), C.c_uint32,
                               C.POINTER(C.c_int32)]
    lib.bound = True


def ppc_run(n):
    def run(e, case):
        e.cl = CL.PpcLog({}, case["queues"])
        saved = STUBS.activate(e)
        try:
            fmt = case["fmt"]
            area = e.alloc(0x28)
            e.write(area, struct.pack(">10I", fmt[0], fmt[1], fmt[2], fmt[3], fmt[4] & MASK, fmt[5] & MASK, DATA, 0, 0,
                                      case["stride"] & MASK))
            clip = e.alloc(16)
            e.write(clip, struct.pack(">4I", *[v & MASK for v in case["clip"]]))
            col = e.alloc(0x30)
            e.write(col, struct.pack(">I", case["colour"]))
            fr = e.alloc(8)
            e.write(fr, struct.pack(">2I", case["pts"][0] & MASK, case["pts"][1] & MASK))
            to = e.alloc(8)
            e.write(to, struct.pack(">2I", case["pts"][2] & MASK, case["pts"][3] & MASK))
            T.call_ppc(e, FN[n], [0, area, clip, col, fr, to])
            return {"log": e.cl.log}
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


def host_run(n):
    def run(lib, case):
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        fmt = (C.c_uint32 * 6)(*[v & MASK for v in case["fmt"]])
        clip = (C.c_int32 * 4)(*case["clip"])
        pts = (C.c_int32 * 4)(*case["pts"])
        lib.tpw_aa_run(n, fmt, DATA, case["stride"], clip, case["colour"], pts)
        return {"log": CL.host_end(lib)}
    return run


def mk(r, pts=None, stride=None):
    if pts is None:
        pts = [r.randrange(-30, 31) for _ in range(4)]
    if stride is None:
        stride = r.choice([r.randrange(1, 300), r.randrange(-300, 300), 0, 64, 1024])
    c = {"pts": pts, "stride": stride,
         "fmt": [r.getrandbits(32) for _ in range(4)] + [r.choice([24, 32, 16]), r.choice([0, 1])],
         "clip": [r.randrange(-20, 10), r.randrange(-20, 10), r.randrange(0, 40), r.randrange(0, 40)],
         "colour": r.getrandbits(32),
         "queues": {"GetFormattedValue": [r.getrandbits(32)]}}
    return c


def edges(seed):
    def gen():
        r = random.Random(seed)
        for dx in (-7, -3, -1, 0, 1, 3, 7):
            for dy in (-7, -3, -1, 0, 1, 3, 7):
                yield mk(r, [2, 3, 2 + dx, 3 + dy])
        yield mk(r, [5, 5, 5, 5])
        yield mk(r, [-4, 9, 12, -3], 0)
    return gen


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("Antialiased<CTraits<4>>", ppc_run(4), host_run(4), edge=edges(4), rnd=rnd),
    FnTest("Antialiased<CTraits<3>>", ppc_run(3), host_run(3), edge=edges(3), rnd=rnd),
]
