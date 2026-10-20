"""Unit v5946, LLCC_DrawTrackLine and the static initialiser of lobby.cpp, against libtpw_unit_LLCC_DrawTrackLine.dylib
(tests/unit_LLCC_DrawTrackLine.cpp), main app, src/TrackCell_flags_CTrackCell_IsTrackStartBlock_LLCC_DrawTrackLine.cpp:
  LLCC_DrawTrackLine 0x100a30f8, __sinit_lobby_cpp 0x100a6be0

A case is a world and a call, built by one piece of Python on both sides. Callees outside the unit are fakes recording their arguments on
both sides: abs (real arithmetic), CMap::IsXYValid (x, y; answers from a queue), the track cell's Modify (0x1009dfe4; the cell as an
offset from the world, mode, a, b; answers from a queue), the two string constructors (object label, length and a hash of the text) and
__register_global_object (labels of object, destructor, node).

The world: gGameWorld (any block; only the cell address is formed), the two cell system words, the end point as two words. Compared: the
answer, the log, the two words, the end point. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_DrawTrackLine"
MASK = 0xFFFFFFFF

FN_DRAW, FN_SINIT = 0x100A30F8, 0x100A6BE0
SIZES = {FN_DRAW: 912, FN_SINIT: 148}
OWN = set(SIZES)

ISXY, MODIFY, ABS = 0x101012F4, 0x1009DFE4, 0x10238BC0
CTOR_W, CTOR_C, REG = 0x10238800, 0x102382D8, 0x10239940

ADDR = dict(world=0x1037EBFC, g485C=0x102E485C, g4858=0x102E4858, lw1=0x102E48AC, lw2=0x102E488C, up=0x102E4868,
            n1=0x102E48C0, n2=0x102E48A0, n3=0x102E487C, dtw=0x103BCBB8, dts=0x103BCD38)
HOST_LAYOUT = [0x1102D8, 0x28]
LABELS = ["lw1", "lw2", "up", "n1", "n2", "n3", "dtw", "dts"]


def s32(v):
    return T.s32(v)


def thash(chars):
    h = 0
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_isxy(e):
    L = e.cl
    L.rec("IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("IsXYValid") & MASK)


def stub_modify(e):
    L = e.cl
    off = s32(e.r(3) - e.u32(ADDR["world"]))
    L.rec("Modify", (off, s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Modify") & MASK)


def stub_abs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


def read_str(e, a, width):
    out = []
    while True:
        v = int.from_bytes(e.read(a, width), "big")
        if v == 0:
            return out
        out.append(v)
        a += width


def stub_ctor(width, name):
    def fn(e):
        L = e.cl
        chars = read_str(e, e.r(4), width)
        if width == 2:
            print(name, "".join(chr(c) for c in chars))
        L.rec(name, (L.lab(e.r(3)), len(chars), thash(chars)))
    return fn


def stub_reg(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


STUBS.internal(ISXY, stub_isxy)
STUBS.internal(MODIFY, stub_modify)
STUBS.imp("abs", stub_abs)
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", stub_ctor(2, "ctorW"))
STUBS.imp("__ct__26TbDynamicStringTemplate<c>FPCc", stub_ctor(1, "ctorC"))
STUBS.imp("__register_global_object", stub_reg)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR[n]: i + 1 for i, n in enumerate(LABELS)}, case["queues"])
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

    def draw(self, mode, x, y, px, py):
        r3, _, _ = T.call_ppc(self.e, FN_DRAW, [mode & MASK, x & MASK, y & MASK, px, py])
        return r3 & MASK

    def sinit(self):
        T.call_ppc(self.e, FN_SINIT, [])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i + 1: self.g(n) for i, n in enumerate(LABELS)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dtl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dtl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def draw(self, mode, x, y, px, py):
        return self.lib.tpw_dtl_draw(s32(mode), s32(x), s32(y), px, py) & MASK

    def sinit(self):
        self.lib.tpw_dtl_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_dtl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "draw").restype = C.c_uint32
    getattr(lib, p + "draw").argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_uint64, C.c_uint64]
    getattr(lib, p + "sinit").restype = None
    getattr(lib, p + "sinit").argtypes = []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build_draw(s, case):
    w = s.alloc(64)
    s.words(s.g("world"), [w])
    s.words(s.g("g485C"), [case["g485C"]])
    s.words(s.g("g4858"), [case["g4858"]])
    blk = s.alloc(8)
    s.words(blk, case["end"])
    return blk


def run_draw(s, case):
    try:
        blk = build_draw(s, case)
        ans = s.draw(case["mode"], case["x"], case["y"], blk, blk + 4)
        return {"answer": ans, "log": s.log(), "g485C": s.rd_word(s.g("g485C")), "g4858": s.rd_word(s.g("g4858")),
                "end": s.rd_words(blk, 2)}
    finally:
        s.done()


def run_sinit(s, case):
    try:
        ans = s.sinit()
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc_draw(e, case):
    return run_draw(Ppc(e, case), case)


def host_draw(lib, case):
    return run_draw(Host(lib, case), case)


def ppc_sinit(e, case):
    return run_sinit(Ppc(e, case), case)


def host_sinit(lib, case):
    return run_sinit(Host(lib, case), case)


def draw_case(r, x=None, y=None, ex=None, ey=None, valid=None, mod=None):
    x = r.randrange(0, 30) if x is None else x
    y = r.randrange(0, 30) if y is None else y
    ex = r.choice([x, x + r.randrange(-12, 13), r.randrange(0, 30)]) if ex is None else ex
    ey = r.choice([y, y + r.randrange(-12, 13), r.randrange(0, 30)]) if ey is None else ey
    valid = [r.choice([0, 1, 1, 1]) for _ in range(30)] if valid is None else valid
    mod = [r.choice([0, 1, 1, 1, 1, 1]) for _ in range(30)] if mod is None else mod
    return {"mode": r.choice([0x81, 0x85, 0x86, 0x100, -1, 7]), "x": x, "y": y, "end": [ex & MASK, ey & MASK],
            "g485C": r.choice([0, 1, 5]), "g4858": r.choice([0, 1, 9]),
            "queues": {"IsXYValid": valid, "Modify": mod}}


def draw_edges():
    r = random.Random(946)
    ones = [1] * 30
    zero = [0] * 30
    for x, y, ex, ey in [(5, 5, 11, 5), (11, 5, 5, 5), (5, 5, 12, 6), (12, 6, 5, 5), (5, 5, 5, 11), (5, 11, 5, 5),
                         (5, 5, 6, 12), (6, 12, 5, 5), (5, 5, 5, 5), (5, 5, 8, 8), (5, 5, 6, 5), (5, 5, 5, 6), (5, 5, 7, 6),
                         (0, 0, 0, 0), (3, 3, -4, 3), (3, 3, 3, -4), (0, 0, 127, 0)]:
        yield draw_case(r, x, y, ex, ey, ones, ones)
        yield draw_case(r, x, y, ex, ey, zero, ones)
        yield draw_case(r, x, y, ex, ey, ones, [0] * 30)
        yield draw_case(r, x, y, ex, ey, ones, [1, 1, 0] + [1] * 27)
        yield draw_case(r, x, y, ex, ey, ones, [1, 1, 1, 1, 1, 1, 0])
        yield draw_case(r, x, y, ex, ey)


def draw_random(r):
    return draw_case(r)


def sinit_edges():
    yield {"queues": {}}


TESTS = [
    FnTest("LLCC_DrawTrackLine", ppc_draw, host_draw, edge=draw_edges, rnd=draw_random),
    FnTest("__sinit_lobby_cpp", ppc_sinit, host_sinit, edge=sinit_edges, rnd=lambda r: {"queues": {}}),
]
