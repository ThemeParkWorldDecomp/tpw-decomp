"""Unit v5706, the distance and bump sound helpers, against libtpw_unit_BMP_Distance.dylib (tests/unit_BMP_Distance.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_Distance 0x1002b318, PossitionSound1 0x1002b248, SingleSoundAtBump 0x1002b180, MineToSystemCords 0x1002b0dc,
  NormaliseVector 0x1002ad34

A case is a world and a call, built by one piece of Python code in both memories. The callees SOUND_MoveSoundEffect and SOUND_SendEvent
are fakes on both sides recording every argument; labs and sqrt are faked on the PowerPC side with C semantics.

The world: a bump block (handle at 0x20, x at 0x34, y at 0x38), the sound bank array at 0x102f7c94, a point block and a vector block.
Compared: the answer, the event log, the bump block, the point, the vector (NaNs made one value), the bank array.

No fused operations; fctiwz only sees values below 2^31. Not tested: nothing skipped.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_Distance"
MASK = 0xFFFFFFFF

F_DIST = "BMP_Distance__Fll"
F_POS = "PossitionSound1__FP7tagBUMP"
F_SINGLE = "SingleSoundAtBump__FP7tagBUMPl"
F_MINE = "MineToSystemCords__FllPQ210NS_MacDoze8tagPOINT"
F_NORM = "NormaliseVector__FP8s3DPoint"

MOVE = 0x100DDF60
SEND = 0x100DD9FC
ADDR = dict(banks=0x102F7C94)
HOST_LAYOUT = [0x34, 0x38, 0xA8, 12]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()


def stub_move(e):
    L = e.cl
    L.rec("SOUND_MoveSoundEffect", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("SOUND_MoveSoundEffect") & MASK)


def stub_send(e):
    L = e.cl
    L.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, L.ret("SOUND_SendEvent") & MASK)


def stub_labs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v if v < 0 else v) & MASK)


def stub_sqrt(e):
    x = e.f(1)
    e.set_f1(math.sqrt(x) if x >= 0 else float("nan"))


STUBS.internal(MOVE, stub_move)
STUBS.internal(SEND, stub_send)
STUBS.imp("labs", stub_labs)
STUBS.imp("sqrt", stub_sqrt)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


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

    def call(self, label, args):
        r3, _, _ = T.call_ppc(self.e, self.e.func(label), [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, label, args):
        l = self.lib
        a = list(args)
        if label == F_DIST:
            return l.tpw_bd_dist(s32(a[0]), s32(a[1])) & MASK
        if label == F_POS:
            l.tpw_bd_pos(a[0])
        elif label == F_SINGLE:
            l.tpw_bd_single(a[0], s32(a[1]))
        elif label == F_MINE:
            l.tpw_bd_mine(s32(a[0]), s32(a[1]), a[2])
        else:
            l.tpw_bd_norm(a[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bd_addr.restype, lib.tpw_bd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bd_alloc.restype, lib.tpw_bd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bd_dist.restype, lib.tpw_bd_dist.argtypes = C.c_int32, [C.c_int32, C.c_int32]
    lib.tpw_bd_pos.restype, lib.tpw_bd_pos.argtypes = None, [C.c_uint64]
    lib.tpw_bd_single.restype, lib.tpw_bd_single.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_bd_mine.restype, lib.tpw_bd_mine.argtypes = None, [C.c_int32, C.c_int32, C.c_uint64]
    lib.tpw_bd_norm.restype, lib.tpw_bd_norm.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bd_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("banks"), case["banks"])
    bump = s.alloc(0xA8)
    s.words(bump + 0x20, [case["handle"]])
    s.words(bump + 0x34, [case["x"], case["y"]])
    pt = s.alloc(8)
    vec = s.alloc(12)
    s.words(vec, case["vec"])
    return bump, pt, vec


def execute(s, case, blocks):
    bump, pt, vec = blocks
    op = case["op"]
    if op == "dist":
        return s.call(F_DIST, case["args"])
    if op == "pos":
        s.call(F_POS, [bump])
    elif op == "single":
        s.call(F_SINGLE, [bump, case["event"]])
    elif op == "mine":
        s.call(F_MINE, [case["x"], case["y"], pt])
    else:
        s.call(F_NORM, [vec])
    return 0      # the others answer void


def observe(s, case, blocks, answer):
    bump, pt, vec = blocks
    return {"answer": answer, "log": s.log(), "bump": s.rd_words(bump, 0x2A), "pt": s.rd_words(pt, 2),
            "vec": [canon_f(w) for w in s.rd_words(vec, 3)], "banks": s.rd_words(s.g("banks"), 8)}


def run(s, case):
    try:
        blocks = build(s, case)
        return observe(s, case, blocks, execute(s, case, blocks))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


INTS = [0, 1, -1, 2, 0x7FFF, 0x8000, 0x8001, -0x7FFF, -0x8000, -0x8001, 0xFFFF, 0x10000, 302, -302, 303, 604, 605,
        0x7FFFFFFF, -0x7FFFFFFF, -0x80000000, 0x40000000, 100000, -100000, 123456789]


def ri(r):
    k = r.random()
    if k < 0.3:
        return r.choice(INTS)
    if k < 0.6:
        return r.randrange(-40000, 40000)
    if k < 0.85:
        return r.randrange(-(1 << 22), 1 << 22)
    return r.randrange(-(1 << 31), 1 << 31)


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000])
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op, x=None, y=None):
    return {"op": op, "x": ri(r) if x is None else x, "y": ri(r) if y is None else y,
            "handle": r.choice([0, 1, 7, 0x12345678, 0xFFFFFFFF, r.randrange(1, 1 << 32)]),
            "event": ri(r), "banks": [r.randrange(0, 1 << 32) for _ in range(8)],
            "vec": [rf(r), rf(r), rf(r)], "args": [0, 0],
            "queues": {"SOUND_MoveSoundEffect": [r.randrange(0, 1 << 32)], "SOUND_SendEvent": [r.randrange(0, 1 << 32)]}}


def make(op):
    def one(r, x=None, y=None):
        c = base(r, op, x, y)
        c["args"] = [c["x"], c["y"]]
        return c

    def edges():
        r = random.Random(sum(map(ord, op)))
        for x in INTS:
            yield one(r, x, r.choice(INTS))
            yield one(r, r.choice(INTS), x)
        for x in (0, 1, -1, 302, 303, -302, -303, 605, 0x8000, 0x7FFF):
            for y in (0, 1, 0x7FFF, 0x8000, -0x8000):
                yield one(r, x, y)

    def rnd(r):
        return one(r)

    return edges, rnd


def norm_edges():
    r = random.Random(77)
    vals = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0x7FC00000, 0x40000000, 0x3F000000, 0x00000001]
    for a in vals:
        for b in (0, 0x3F800000, 0xC0400000):
            c = base(r, "norm")
            c["vec"] = [a, b, r.choice(vals)]
            yield c


def norm_rnd(r):
    return base(r, "norm")


def mk(name, op):
    e, rn = make(op)
    return FnTest(name, ppc, host, edge=e, rnd=rn)


TESTS = [
    mk("BMP_Distance", "dist"),
    mk("PossitionSound1", "pos"),
    mk("SingleSoundAtBump", "single"),
    mk("MineToSystemCords", "mine"),
    FnTest("NormaliseVector", ppc, host, edge=norm_edges, rnd=norm_rnd),
]
