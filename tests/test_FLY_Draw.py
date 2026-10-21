"""Unit v5007, FLY_Draw, against libtpw_unit_FLY_Draw.dylib (tests/unit_FLY_Draw.cpp), main app ("SimTheme Park"),
src/game/rides/FlyingRide.cpp:
  FLY_Draw 0x10077ae4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers where the headers have them) for the translation. The
callees outside the unit are fakes on both sides: SetLMatrixIdentityNoTrans and RotateLMatrixXYZNoTrans (the matrix is told by a marker
word in m[3][3], which the function never writes), FindDataNodeIndexByID (the set is told by its mFD4 word), PAR_SetEmitterPos and the
float-to-angle function at 0x10237cd8 (named FLY_FloatToInt on the host; the answers come from queues).

The world of a case: the step, the fly bases (0x122c-byte blocks, every word the function reads written per slot), three ride sets with their
mesh, header, node (matrix) and embedded records, the three angle/position registers. What is compared: the registers, the vector, every
word of every base, the matrix of every ride node, the event log.

Fused multiply-adds and fctiwz: the PowerPC side runs under HardwareRounding. Not tested: a NULL ride set for a nonzero ride number, an
embedded record index past the records, a NaN step or NaN floats (the original stores them; the two processors make different NaNs),
float-to-int values outside 32 bits.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_FLY_Draw"
MASK = 0xFFFFFFFF

FN = 0x10077ae4
SIZES = {FN: 1776}
OWN = set(SIZES)

SETIDENT = 0x100c0eac
ROTATE = 0x100c174c
FIND = 0x101ffdc8
EMITTER = 0x100be150
F2I = 0x10237cd8

ADDR = dict(gFlyBases=0x102da964, gRideAnimSets=0x102c79a4, gFlyDrawPosX=0x102da960, gFlyDrawPosY=0x102da95c, gFlyDrawPosZ=0x102da958,
            gFlyDrawAngA=0x102da954, gFlyDrawAngB=0x102da950, gFlyDrawAngC=0x102da94c, gFlyDrawOrigin=0x1029d2f8)
REGS = ["gFlyDrawPosX", "gFlyDrawPosY", "gFlyDrawPosZ", "gFlyDrawAngA", "gFlyDrawAngB", "gFlyDrawAngC"]
BASE_SIZE = 0x122c
RIDES = 3
RECORDS = 4

# the Mac's layout of the ride chain; the host's comes from tpw_fd_layout
PPC_LAYOUT = dict(heads=8, fd4=0xd4, header=4, f28=0x28, f78=0x78, matrix=0x10, records=4, rec_size=0x14, rec_block=4, ptr=4)
LAYOUT_KEYS = ["heads", "fd4", "header", "f28", "f78", "matrix", "records", "rec_size", "rec_block", "ptr"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_setident(e):
    e.cl.rec("SetIdentity", (e.u32(e.r(3) + 0x3c),))


def stub_rotate(e):
    a = e.r(5)
    e.cl.rec("Rotate", (e.u32(e.r(3) + 0x3c), s32(e.r(4)), e.u32(a), e.u32(a + 4), e.u32(a + 8)))


def stub_find(e):
    e.cl.rec("Find", (e.s32(e.r(3) + 0xd4), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, e.cl.ret("Find") & MASK)


def stub_emitter(e):
    e.cl.rec("Emitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_f2i(e):
    v = (fb(e.f(1)) * 2654435761) & MASK
    e.cl.rec("F2I", (fb(e.f(1)), v))
    e.set_r(3, v)


STUBS.internal(SETIDENT, stub_setident)
STUBS.internal(ROTATE, stub_rotate)
STUBS.internal(FIND, stub_find)
STUBS.internal(EMITTER, stub_emitter)
STUBS.internal(F2I, stub_f2i)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptrs(self, a, vs):
        """Native-width pointer words."""
        fmt = self.fmt[0] + ("Q" if self.lay["ptr"] == 8 else "I")
        self.write(a, b"".join(struct.pack(fmt, v) for v in vs))


class Ppc(Side):
    fmt = ">I"
    lay = PPC_LAYOUT

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

    def p32(self, a):
        return a

    def log(self):
        return self.L.log

    def call(self, step):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [T.F(step)])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fd_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def p32(self, a):
        return self.lib.tpw_fd_off(a) if a else 0

    def log(self):
        return CL.host_end(self.lib)

    def call(self, step):
        self.lib.tpw_fd_draw(step)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fd_addr.restype, lib.tpw_fd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fd_alloc.restype, lib.tpw_fd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fd_off.restype, lib.tpw_fd_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_fd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fd_draw.restype, lib.tpw_fd_draw.argtypes = None, [C.c_float]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fd_layout(out)
    v = list(out)[:n]
    lib.lay = dict(zip(LAYOUT_KEYS, v[:10]))
    assert lib.lay["f78"] == 0x78 and lib.lay["matrix"] == 0x10, v
    assert max(v[10:n]) < 0x400, v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    lay = s.lay
    s.words(s.g("gFlyDrawOrigin"), case["vec"])
    for name, w in zip(REGS, case["regs"]):
        s.words(s.g(name), [w])
    world = {"bases": {}, "nodes": {}}
    sets = [0] * 32
    for k in range(1, RIDES + 1):
        spec = case["rides"][k]
        st, mesh, hdr, node, eset, recs = [s.alloc(0x400) for _ in range(6)]
        blocks = [s.alloc(0x40) for _ in range(RECORDS)]
        s.ptrs(st + lay["heads"], [mesh])
        s.words(st + lay["fd4"], [1000 + k])
        s.ptrs(mesh + lay["header"], [hdr])
        s.ptrs(mesh + lay["f28"], [eset])
        s.words(hdr + lay["f78"], [s.p32(node)])
        s.words(node + lay["matrix"], spec["matrix"][:15] + [1000 + k])
        s.ptrs(eset + lay["records"], [recs])
        for j in range(RECORDS):
            if spec["blocks"][j] is not None:
                s.ptrs(recs + j * lay["rec_size"] + lay["rec_block"], [blocks[j]])
                s.words(blocks[j], spec["blocks"][j])
        sets[k] = st
        world["nodes"][k] = node + lay["matrix"]
    s.ptrs(s.g("gRideAnimSets"), sets)
    table = [0] * 100
    for idx, slots in case["bases"].items():
        b = s.alloc(BASE_SIZE)
        table[idx] = b
        world["bases"][idx] = b
        for n, d in enumerate(slots):
            for off, w in d.items():
                s.words(b + 0x5c + n * 0xe4 + off, [w])
    s.ptrs(s.g("gFlyBases"), table)
    return world


def observe(s, case, world):
    out = {"log": s.log(), "vec": s.rd_words(s.g("gFlyDrawOrigin"), 3)}
    for name in REGS:
        out[name] = s.rd_word_(s.g(name)) if False else s.rd_words(s.g(name), 1)
    for idx, b in world["bases"].items():
        out["base%d" % idx] = s.rd_words(b, BASE_SIZE // 4)
    for k, m in world["nodes"].items():
        out["mat%d" % k] = s.rd_words(m, 16)
    return out


def run(s, case):
    try:
        world = build(s, case)
        s.call(case["step"])
        return observe(s, case, world)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def sint(r, lim=40000):
    return r.randint(-lim, lim) & MASK


def angle(r):
    return r.choice([r.randrange(0, 4096), r.randint(-9000, 9000), r.randrange(0, 4096), 0, 0x7ff, 0x800, 0xfff, 0x1000]) & MASK


def slot(r, active, flag, ride, emit):
    d = {0x80: active, 0xc8: flag, 0xc0: ride, 0xc4: emit}
    for off in (4, 8, 0xc, 0x1c, 0x20, 0x24):
        d[off] = sint(r)
    for off in (0x10, 0x14, 0x18, 0x28, 0x2c, 0x30):
        d[off] = angle(r)
    for off in (0x34, 0x38, 0x3c):
        d[off] = fb(r.uniform(-50, 50))
    for off in (0x40, 0x44, 0x48):
        d[off] = fb(r.uniform(-1000, 5000))
    return d


def ride_spec(r, nullblock):
    blocks = []
    for j in range(RECORDS):
        if r.random() < nullblock:
            blocks.append(None)
        else:
            blocks.append([r.getrandbits(32) if i not in (12, 13, 14) else fb(r.choice([r.uniform(-500, 500), r.uniform(-1e5, 1e5), 0.0, -0.5, 0.99, -1.5]))
                           for i in range(16)])
    return {"matrix": [fb(r.uniform(-3, 3)) for _ in range(16)], "blocks": blocks}


def mkcase(r, nb=None, step=None, p_active=0.8, p_flag=0.4, p_ride=0.7, p_emit=0.7, nullblock=0.3, idxs=None):
    if step is None:
        step = r.choice([0.0, 0.5, 1.0, 2.0, 2.5, r.uniform(0, 2), r.uniform(0, 3), -1.0, -0.0, 1e30])
    if nb is None:
        nb = r.choice([0, 1, 1, 2, 3])
    if idxs is None:
        idxs = r.sample([0, 1, 2, 5, 50, 98, 99], nb)
    bases = {}
    for idx in idxs:
        slots = []
        for n in range(20):
            slots.append(slot(r, r.choice([1, 2, 0x80000000]) if r.random() < p_active else 0,
                              r.choice([1, 5, 0x80000000]) if r.random() < p_flag else 0,
                              r.randrange(1, RIDES + 1) if r.random() < p_ride else 0,
                              r.randrange(1, 60) if r.random() < p_emit else 0))
        bases[idx] = slots
    return {"queues": {"Find": [r.choice([0xFFFFFFFF, 0, 1, 2, 3, 3]) for _ in range(60)]},
            "step": bitsf(fb(step)), "vec": [r.getrandbits(32) for _ in range(3)], "regs": [fb(r.uniform(-9, 9)) for _ in range(6)],
            "bases": bases, "rides": {k: ride_spec(r, nullblock) for k in range(1, RIDES + 1)}}


def edges():
    r = random.Random(5007)
    yield mkcase(r, nb=0)
    yield mkcase(r, nb=1, idxs=[0])
    yield mkcase(r, nb=1, idxs=[99], p_active=0.0)
    yield mkcase(r, nb=1, idxs=[1], p_active=1.0, p_flag=1.0, p_ride=0.0)
    yield mkcase(r, nb=1, idxs=[1], p_active=1.0, p_flag=1.0, p_ride=1.0, p_emit=1.0)
    yield mkcase(r, nb=1, idxs=[2], p_active=1.0, p_flag=0.0, p_ride=0.0)
    yield mkcase(r, nb=1, idxs=[2], p_active=1.0, p_flag=0.0, p_ride=1.0, p_emit=0.0)
    yield mkcase(r, nb=1, idxs=[3], p_active=1.0, p_flag=0.0, p_ride=1.0, p_emit=1.0, nullblock=0.0)
    yield mkcase(r, nb=1, idxs=[3], p_active=1.0, p_flag=0.0, p_ride=1.0, p_emit=1.0, nullblock=1.0)
    for st in (0.0, -0.0, 1.0, 2.0, 2.0000002, 2.5, 1e30, 0.25):
        yield mkcase(r, nb=1, idxs=[7], step=st, p_active=0.7, p_flag=0.2)
    yield mkcase(r, nb=3, idxs=[1, 50, 99], p_active=0.5)


def rnd(r):
    return mkcase(r)


TESTS = [
    FnTest("FLY_Draw", ppc, host, edge=edges, rnd=rnd),
]
