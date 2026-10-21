"""Unit v5003, IntensifyPix<CTraits<N>> (N = 4, 3, 2, 1), against libtpw_unit_unnamed_IntensifyPix.dylib
(tests/unit_unnamed_IntensifyPix.cpp), main app, src/game/ui/3DFont.cpp:
  CTraits<4> 0x100073b8, CTraits<3> 0x100071f4, CTraits<__vec2x32float__> 0x10007030, CTraits<__int128> 0x10006e6c

A case is a pixel word, a rect, a position, an alpha and five masks. The world is built by one piece of Python on both memories.
Compared: the N bytes written, and that the other bytes of the 4-byte pixel are untouched. No callees, no fused operations.
Not tested: a rect or pixel pointer that is NULL (read unconditionally).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_unnamed_IntensifyPix"
MASK = 0xFFFFFFFF
FN = {4: 0x100073b8, 3: 0x100071f4, 2: 0x10007030, 1: 0x10006e6c}
HOST_LAYOUT = [4, 8, 12, 16]


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e

    def done(self):
        pass

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def call(self, n, pix, rect, c):
        T.call_ppc(self.e, FN[n], [self.alloc(4), pix, T.D(c["alpha"]), c["colour"], c["m1"], c["m2"], c["m3"], c["keep"],
                                   c["x"] & MASK, c["y"] & MASK, rect])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ip_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def call(self, n, pix, rect, c):
        getattr(self.lib, "tpw_ip_p%d" % n)(pix, c["alpha"], c["colour"], c["m1"], c["m2"], c["m3"], c["keep"],
                                             T.s32(c["x"]), T.s32(c["y"]), rect)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ip_alloc.restype, lib.tpw_ip_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ip_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in (1, 2, 3, 4):
        f = getattr(lib, "tpw_ip_p%d" % n)
        f.restype = None
        f.argtypes = [C.c_uint64, C.c_double] + [C.c_uint32] * 5 + [C.c_int32, C.c_int32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ip_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    n = case["n"]
    try:
        pix = s.alloc(8)
        s.words(pix, [case["pixel"]])
        rect = s.alloc(16)
        s.words(rect, case["rect"])
        tail = s.read(pix + n, 4 - n)
        s.call(n, pix, rect, case)
        l, t, rr, b = [T.s32(v) for v in case["rect"]]
        a = abs(case["alpha"])
        hit = l <= T.s32(case["x"]) < rr and t <= T.s32(case["y"]) < b and a <= 1.0
        # The bytes are stored low byte first; an untouched pixel is read back as the word of the side's own byte order.
        if hit:
            res = s.read(pix, n)
        else:
            res = struct.unpack(s.fmt, s.read(pix, 4))[0]
        return {"written": res, "tail_same": hit and s.read(pix + n, 4 - n) == tail or not hit}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, n):
    left = r.randrange(-5, 10)
    top = r.randrange(-5, 10)
    rect = [left, top, left + r.randrange(0, 8), top + r.randrange(0, 8)]
    c = {"n": n, "pixel": r.getrandbits(32), "rect": [v & MASK for v in rect], "x": r.randrange(left - 2, rect[2] + 2),
         "y": r.randrange(top - 2, rect[3] + 2),
         "alpha": r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 1.0000001, -2.0, 0.999, float("nan"), float("inf"), r.uniform(-1.2, 1.2),
                            r.uniform(-1, 1)]),
         "colour": r.getrandbits(32), "keep": r.choice([0, MASK, r.getrandbits(32)])}
    for k in ("m1", "m2", "m3"):
        c[k] = r.choice([0xFF, 0xFF00, 0xFF0000, 0xFF000000, MASK, 0, r.getrandbits(32)])
    return c


def edges(n):
    def gen():
        r = random.Random(100 + n)
        for i in range(70):
            c = mk(r, n)
            l, t = T.s32(c["rect"][0]), T.s32(c["rect"][1])
            c["rect"] = [l & MASK, t & MASK, (l + 4) & MASK, (t + 4) & MASK]
            c["x"], c["y"] = [(l - 1, t), (l, t), (l + 3, t + 3), (l + 4, t), (l, t - 1), (l, t + 3), (l, t + 4)][i % 7]
            c["alpha"] = [0.0, -0.0, 1.0, -1.0, 0.5, 1.0000001, float("nan"), float("-inf"), 0.999, 0.001][i % 10]
            yield c
    return gen


def rnd(n):
    return lambda r: mk(r, n)


TESTS = [
    FnTest("@unnamed@::IntensifyPix<CTraits<4>>", ppc, host, edge=edges(4), rnd=rnd(4)),
    FnTest("@unnamed@::IntensifyPix<CTraits<3>>", ppc, host, edge=edges(3), rnd=rnd(3)),
    FnTest("@unnamed@::IntensifyPix<CTraits<__vec2x32float__>>", ppc, host, edge=edges(2), rnd=rnd(2)),
    FnTest("@unnamed@::IntensifyPix<CTraits<__int128>>", ppc, host, edge=edges(1), rnd=rnd(1)),
]
