"""Unit v5559, the 3DFont.cpp antialiased line for one byte per pixel, against libtpw_unit_Antialiased_2.dylib (tests/unit_Antialiased_2.cpp),
main app ("SimTheme Park"), src/game/ui/3DFont.cpp:
  Antialiased<CTraits<__int128>> 0x10006904

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) and in host memory
(little-endian). The callees outside the unit are fakes on both sides: CColour::GetFormattedValue (logs the format words and the colour
bytes, answers from the queue) and IntensifyPix<1> (logs the pixel as an offset from the area's data, the alpha bits, the colour, the
four masks, x, y and the clip rectangle). sqrt is the C library on both sides.

The world of a case: a render area (masks, data pointer, stride), a colour, a clip rectangle and two points. What is compared: the
whole event log. The function returns nothing and writes no memory itself. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Antialiased_2"
MASK = 0xFFFFFFFF

FN = 0x10006904
GETFMT = 0x1023af18
INTENSIFY = 0x10006e6c
HOST_LAYOUT = [0x18, 0x28, 0x24]
STATE = {"data": 0}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_getfmt(e):
    L = e.cl
    fmt = struct.unpack(">6I", e.read(e.r(4), 24))
    col = tuple(e.read(e.r(3), 4))
    L.rec("GetFormattedValue", fmt + col)
    e.set_r(3, L.ret("GetFormattedValue") & MASK)


def stub_intensify(e):
    L = e.cl
    sp = e.r(1)
    keep = e.u32(sp + 0x38)
    x = e.u32(sp + 0x3C)
    y = e.u32(sp + 0x40)
    rect = e.u32(sp + 0x44)
    bits = T.f64_bits(e.f(1))
    L.rec("IntensifyPix", (s32(e.r(4) - STATE["data"]), s32(bits >> 32), s32(bits), s32(e.r(7)), s32(e.r(8)), s32(e.r(9)),
                           s32(e.r(10)), s32(keep), s32(x), s32(y)))
    L.rec("IntensifyRect", tuple(s32(w) for w in struct.unpack(">4i", e.read(rect, 16))))


STUBS.imp("GetFormattedValue__Q33bfl4draw7CColourCFRC11TbPixFormat", stub_getfmt)
STUBS.internal(INTENSIFY, stub_intensify)


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

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def area(self, a, masks, data, stride):
        self.words(a, masks)
        self.words(a + 0x18, [data])
        self.words(a + 0x24, [stride])

    def call(self, area, clip, col, p0, p1):
        T.call_ppc(self.e, FN, [self.alloc(4), area, clip, col, p0, p1])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_aa_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def area(self, a, masks, data, stride):
        self.words(a, masks)
        self.write(a + 0x18, struct.pack("<Q", data))
        self.words(a + 0x28, [stride])

    def call(self, area, clip, col, p0, p1):
        self.lib.tpw_aa_run(area, clip, col, p0, p1)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_aa_alloc.restype, lib.tpw_aa_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_aa_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_aa_run.restype, lib.tpw_aa_run.argtypes = None, [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = lib.tpw_aa_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        data = s.alloc(16)
        STATE["data"] = data
        a = s.alloc(0x30)
        s.area(a, case["masks"], data, case["stride"])
        col = s.alloc(0x24)
        s.write(col, bytes(case["col"]))
        clip = s.alloc(16)
        s.words(clip, case["clip"])
        p0 = s.alloc(8)
        s.words(p0, case["p0"])
        p1 = s.alloc(8)
        s.words(p1, case["p1"])
        s.call(a, clip, col, p0, p1)
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, p0, p1, stride=None):
    return {"queues": {"GetFormattedValue": [r.getrandbits(32)]},
            "masks": [r.getrandbits(32) for _ in range(4)] + [8, 0],
            "stride": r.randrange(1, 100) if stride is None else stride,
            "col": [r.randrange(256) for _ in range(4)],
            "clip": [r.randrange(-5, 10), r.randrange(-5, 10), r.randrange(10, 40), r.randrange(10, 40)],
            "p0": list(p0), "p1": list(p1)}


def rnd(r):
    x, y = r.randrange(-15, 30), r.randrange(-15, 30)
    return mk(r, (x, y), (r.randrange(-15, 30), r.randrange(-15, 30)), r.choice([None, None, 0, -7, 1]))


def edges():
    r = random.Random(5559)
    for dx in (-9, -5, -1, 0, 1, 3, 5, 9):
        for dy in (-9, -5, -1, 0, 1, 3, 5, 9):
            x, y = r.randrange(-3, 12), r.randrange(-3, 12)
            yield mk(r, (x, y), (x + dx, y + dy))
    yield mk(r, (0, 0), (0, 0))
    yield mk(r, (3, 4), (-20, 4), -5)
    yield mk(r, (3, 4), (3, -20), -5)


TESTS = [
    FnTest("Antialiased<CTraits<__int128>>", ppc, host, edge=edges, rnd=rnd),
]
