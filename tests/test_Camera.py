"""Differential test for src/game/TPCamera.cpp and src/game/LMatrix.cpp (main app, unit w135): HandleCamera 0x1003491c (the park
camera's frame) and SetLMatrixOrientationHi 0x100c1db4, against libtpw_unit_Camera.dylib.

Real on both sides: the external-camera branch of HandleCamera (a 0x80 in gFirstPersonFlags), SetMatrixIdentity 0x100c0e28,
RotateLMatrixXYZNoTrans 0x100c174c, RotateLMatrixXYZNoTransHi 0x100c1944, YRotateLMatrixNoTrans 0x100c1698, InvertMatrix 0x100c249c and
CombineMatricesNoWDest2nd 0x100c28fc. Fakes on both sides that record every argument: CClock::ReadFixed 0x1014733c, CKeyTable::GetState
0x1014fff0 (held when the id's bit is in the case's key mask), DetermineHFieldHeight 0x1005ea70, GetNextGridsquare 0x10033e14 (its out point is not recorded, the original leaves it unset on the first call; writes the
scripted point and answers the scripted more flag), RideRideIfRequired 0x100345f8, MatrixDifferenceTiny 0x10034784,
ApplyHeadsOnRidesBumpage 0x100336f4 (writes the scripted matrix) and the engine's CMatrix::Load (an import). The C library calls (sin, cos,
sqrt, fmod) are Python's math, which is the host's libm.

A case is a dict of words: the clock, the camera globals, the settings and window numbers, the key mask, the height field, the ride matrix
and the scripted answers. Compared: every camera global, the matrices (camera, the previous two, the inverse, the engine's view matrix and
the ride matrix), which matrix is current, and the call logs.

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (a code hook per fused instruction) gives each the
processor's single rounding.

Not tested: NaN and infinite inputs; a height field missing its header (the original reads the header unconditionally); the height-field
heights or quads missing while walking (the original leaves two registers unset there; the translation gives them 0.0f).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Camera"
ENTRY = 0x1003491C
SIZE = 5644
ORIENT = 0x100C1DB4
MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000
FUSED = [(ENTRY, ENTRY + SIZE), (ORIENT, ORIENT + 192), (0x100C0E28, 0x100C0E28 + 84), (0x100C174C, 0x100C174C + 432),
         (0x100C1944, 0x100C1944 + 296), (0x100C1698, 0x100C1698 + 124), (0x100C249C, 0x100C249C + 460), (0x100C28FC, 0x100C28FC + 540)]
A = dict(
    stamp=0x102B5A90, modeA=0x102B5AA4, modeB=0x102B5AA8, f5aac=0x102B5AAC, f5bf4=0x102B5BF4, f68=0x1029CC68, start=0x1029CC6C,
    pos=0x1029CC78, vel=0x102B5BB4, yaw=0x102B5BF0, pitch=0x102B5A4C, tilt=0x102B5A44, ground=0x102B5A24, turn=0x102B5BE4,
    fpitch=0x102B5BE8, anchor=0x102B5A9C, flags=0x102B5A40, settings=0x1038071C, tpw=0x102B2B4C, ptr=0x102B5A94, cx=0x102B2B5C,
    cy=0x102B2B58, hw=0x10278A28, hh=0x10278A2C, wideOnce=0x102B5A08, wideMin=0x102B5A0C, wideMaxOnce=0x102B5A00, wideMax=0x102B5A04,
    walk=0x102B59F8, f59=0x102B59FC, keys=0x1037F22C, clock=0x1037F048, target=0x102B5BD8, vp=0x102B5BC0, ride=0x102B5A2C,
    cam=0x102B5B74, ma=0x102B5AF4, mb=0x102B5B34, ext=0x102B5A50, inv=0x102B5AB0, current=0x102B5AF0, header=0x102C6C7C,
    globals=0x103B74C8)


def f32(x):
    return T.f32(x)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon(word):
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x7FFFFF):
        return NAN_BITS
    return word


def s32(v):
    v &= MASK
    return v - (1 << 32) if v & 0x80000000 else v


# ------------------------------------------------------------------ the original's C library calls


def fmod(e):
    x, y = e.f(1), e.f(2)
    if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
        e.set_f1(math.nan)
    else:
        e.set_f1(math.fmod(x, y))


# ------------------------------------------------------------------ the emulator's fused multiply-adds

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """fmadds, fmsubs, fnmadds and fnmsubs with the processor's single rounding (see test_RideAnimInterp.py)."""

    changed = 0
    seen = 0

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ the cases

# Edge values of the pointer's offset (in half windows) that the listing compares with: the edge band 0.93, the first person dead zones.
EDGE_U = [0.0, 0.93, -0.93, 0.9301, -0.9301, 0.9299, -0.9299, 0.4, -0.4, 0.2, -0.2, 0.2001, -0.2001, 0.3, -0.3, 0.45, -0.45, 1.0, -1.0]
FLAG_POOL = [0, 0, 0, 0, 0x2, 0x2, 0x2, 0x4, 0x10, 0x14, 0x8, 0x20, 0x800, 0x1000, 0x100, 0x802, 0x1002, 0x102, 0x2, 0x1004, 0x804]
MODE_BITS = [4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200]


def fr(r, lo, hi):
    return fb(f32(r.uniform(lo, hi)))


def make(r, **kw):
    c = {}
    c["mask"] = r.choice([15, 255, 4095])
    c["scale"] = fr(r, 1.0, 800.0)
    c["table"] = [fb(f32(r.uniform(-1.0, 1.0))) for _ in range(c["mask"] + 1)]
    now = r.getrandbits(32) if r.random() < 0.15 else r.randint(1000, 5_000_000)
    delta = r.choice([1, 2, 5, 17, 50, 150, 198, 199, 200, 201, 202, 300, 5000, 0, -1, -7, r.randint(0, 400)])
    c["now"] = now
    c["stamp"] = (now - delta) & MASK
    modeA = 0
    for b in MODE_BITS:
        if r.random() < (0.5 if b in (0x80, 0x200) else 0.25):
            modeA |= b
    modeB = modeA if r.random() < 0.3 else (modeA ^ r.choice([0, 4, 8, 0x10, 0x18, 0x20, 0x40])) | (r.choice([0, 0x100]) if r.random() < 0.1 else 0)
    c["modeA"], c["modeB"] = modeA, modeB
    c["f5aac"] = r.choice([0, 0, 0, 1, 1, 2])
    y = r.choice([0.0, r.uniform(-7, 7), r.uniform(-7, 7), r.uniform(-1, 1), 0.7853982, -0.7853982, 1.5707964, -1.5707964, 3.0, -3.0])
    c["f5bf4"] = fb(f32(y))
    c["f68"] = fr(r, 20.0, 380.0) if r.random() < 0.8 else fb(f32(r.choice([40.0, 300.0, 69.0, 150.0, 110.0])))
    c["pos"] = [fr(r, -30.0, 800.0), fr(r, -5.0, 20.0), fr(r, -30.0, 800.0)]
    if r.random() < 0.1:
        c["pos"][0] = fb(f32(r.choice([-3.0, 0.0, 1000.0])))
    if r.random() < 0.1:
        c["pos"][2] = fb(f32(r.choice([-3.0, 0.0, 1000.0])))
    scale = r.choice([0.00001, 0.0005, 0.5, 3.0, 8.0])
    c["vel"] = [fr(r, -scale, scale), fr(r, -scale, scale) if r.random() < 0.3 else 0, fr(r, -scale, scale)]
    if r.random() < 0.1:
        c["vel"] = [fb(f32(r.choice([0.0, 0.00005, -0.00005, 0.0001, -0.0001]))) for _ in range(3)]
    c["start"] = [fr(r, 0, 600), fr(r, 0, 5), fr(r, 0, 600)]
    c["yaw"] = fr(r, -7, 7)
    if r.random() < 0.15:
        c["yaw"] = c["f5bf4"]
    if r.random() < 0.1:
        c["yaw"] = fb(f32(struct.unpack(">f", struct.pack(">I", c["f5bf4"]))[0] + r.choice([0.0004, -0.0004, 0.0006, -0.0006])))
    c["pitch"] = fr(r, -2, 2)
    c["tilt"] = [fr(r, -0.5, 0.5), fr(r, -0.5, 0.5)]
    c["ground"] = fr(r, -2, 12)
    c["turn"] = fr(r, -2, 2)
    c["fpitch"] = fr(r, -1, 1)
    c["anchor"] = [fr(r, 0, 1000), fr(r, 0, 800)]
    flags = r.choice(FLAG_POOL)
    if r.random() < 0.04:
        flags |= 0x80
    c["flags"] = flags
    c["iso"] = r.choice([0, 1])
    c["rmb"] = r.choice([0, 1, 1])
    c["tpw"] = r.choice([0, 8, 8, 8, 0xFFFFFFFF, 0xF7, r.getrandbits(32)])
    cx, hw = f32(r.uniform(300, 800)), f32(r.uniform(300, 800))
    cy, hh = f32(r.uniform(250, 600)), f32(r.uniform(250, 600))
    c["cx"], c["cy"], c["hw"], c["hh"] = fb(cx), fb(cy), fb(hw), fb(hh)

    def offset():
        return r.choice(EDGE_U) if r.random() < 0.45 else r.uniform(-1.15, 1.15)
    c["ptr"] = [fb(f32(cx + hw * offset())), fb(f32(cy - hh * offset()))]
    c["wideOnce"] = r.choice([0, 1, 1])
    c["wideMin"] = fr(r, 20, 80)
    c["wideMaxOnce"] = r.choice([0, 1, 1])
    c["wideMax"] = fr(r, 200, 400)
    c["walk"] = r.choice([0, 1])
    c["f59"] = fr(r, -1, 1)
    keys = 0
    for k in (0, 1, 6, 7, 0xD, 0xE, 0xF, 0x10, 0x12, 0x13, 0x15, 0x16):
        if r.random() < 0.18:
            keys |= 1 << k
    c["keys"] = keys
    c["target"] = [fr(r, -9, 9) for _ in range(3)]
    c["vp"] = [fr(r, -9, 9) for _ in range(3)]
    c["rideSet"] = 1 if (flags & 0x3C) or r.random() < 0.3 else 0
    c["ride"] = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    c["cam"] = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    c["ma"] = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    c["mb"] = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    c["ext"] = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    w, d = r.randint(20, 60), r.randint(20, 60)
    cell = f32(r.uniform(4, 14))
    c["field"] = [fb(cell), fb(f32(cell if r.random() < 0.5 else r.uniform(4, 14))), w, d]
    full = not (flags & 0x3C) or r.random() < 0.5
    c["hasHeights"] = 1 if full or r.random() < 0.5 else 0
    c["hasQuads"] = 1 if full or r.random() < 0.5 else 0
    c["height"] = [fb(f32(r.uniform(-3, 15))) for _ in range(r.choice([1, 2, 4]))]
    k = r.choice([0, 0, 1, 2, 5])
    c["grid"] = [1] * k + [0]
    c["gridout"] = [fb(f32(r.uniform(-20, 800))) for _ in range(3 * max(k, 1))]
    c["riderride"] = [r.choice([0, 0, 0, 1])]
    c["tiny"] = [r.choice([0, 1])]
    c["bump"] = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    c.update(kw)
    return c


def edges():
    r = random.Random(4321)
    # the dt cases
    for delta in (1, 2, 199, 200, 201, 0, -1, 5000):
        yield make(r, f5aac=0, stamp=(1000000 - delta) & MASK, now=1000000, flags=0)
    yield make(r, f5aac=0, now=0xFFFFFFFF, stamp=5, flags=0)
    yield make(r, f5aac=0, now=3, stamp=0xFFFFFFF0, flags=0)
    # the isometric snap, four cases
    for yaw in (0.3, 0.7, 0.8, 1.5, -0.3, -0.7, -0.8, -1.5, 0.0, 3.2, -3.2, 0.785398, 0.7853982, -0.7853982):
        for iso in (1, 0):
            yield make(r, f5aac=1, iso=iso, f5bf4=fb(f32(yaw)), flags=0, modeA=0, modeB=0)
    # the key mask on its own, parked
    for keys in (0, 1 << 6, 1 << 0x12, 1 << 7, 1 << 0x13, 1, 1 << 0x15, 2, 1 << 0x16, 0xFFFFFFFF, 1 << 0xD, 1 << 0xE, 1 << 0xF, 1 << 0x10):
        yield make(r, keys=keys, flags=0, f5aac=0)
        yield make(r, keys=keys, flags=2, f5aac=0)
    # edge scroll on the bands
    for ux in (0.93, -0.93, 0.9301, -0.9301, 0.9299, 0.0):
        for uy in (0.93, -0.93, 0.9301, -0.9301, 0.9299, 0.0):
            cx, hw, cy, hh = 500.0, 400.0, 300.0, 300.0
            yield make(r, cx=fb(cx), hw=fb(hw), cy=fb(cy), hh=fb(hh), ptr=[fb(f32(cx + hw * ux)), fb(f32(cy - hh * uy))], flags=0,
                       tpw=8, modeA=0, modeB=0, keys=0, f5aac=0)
    # the reset, the modes, the zoom clamps
    for mode in (0x100, 0x180, 0x380, 0x80, 0x280, 0x84, 0x48, 0x90, 0xA0, 0xC0, 0xC4):
        for once in (0, 1):
            yield make(r, modeA=mode, modeB=mode & ~0x100, wideOnce=once, wideMaxOnce=once, flags=0)
    # the external camera
    yield make(r, flags=0x80)
    yield make(r, flags=0x82)
    # first person: walking and riding, with every band of the pointer
    for flags in (2, 0x802, 4, 0x14, 0x8, 0x10, 0x20, 0x1002, 0x102):
        for ux in (0.0, 0.4, -0.4, 0.2, -0.2, 0.3, -0.3, 0.9):
            for uy in (0.0, 0.4, -0.4, 0.2, -0.2, 0.3):
                cx, hw, cy, hh = 500.0, 400.0, 300.0, 300.0
                yield make(r, flags=flags, cx=fb(cx), hw=fb(hw), cy=fb(cy), hh=fb(hh), rideSet=1,
                           ptr=[fb(f32(cx + hw * ux)), fb(f32(cy - hh * uy))], hasHeights=1, hasQuads=1, f5aac=0)
    for flags in (2, 4, 0x14):
        for tiny in (0, 1):
            yield make(r, flags=flags, tiny=[tiny], hasHeights=1, hasQuads=1, rideSet=1)
    for grid in ([0], [1, 0], [1, 1, 1, 0], [1, 1, 1, 1, 1, 1, 1, 0]):
        for ride in (0, 1):
            yield make(r, flags=2, grid=grid, riderride=[ride], vel=[fb(1.5), 0, fb(-2.5)], modeA=0, modeB=0, hasHeights=1, hasQuads=1,
                       gridout=[fb(f32(r.uniform(-20, 800))) for _ in range(3 * len(grid))], keys=0, f5aac=0)


def rnd(r):
    return make(r)


def orient_edges():
    r = random.Random(99)
    for pitch, yaw in ((0.0, 0.0), (1.5707964, 0.0), (0.0, 1.5707964), (-1.0, 3.0), (4.0, -4.0), (0.3, -0.3), (3.1415927, 3.1415927)):
        yield orient_case(r, pitch, yaw)


def orient_case(r, pitch=None, yaw=None):
    return dict(m=[fb(f32(r.uniform(-3, 3))) for _ in range(16)], pitch=fb(f32(r.uniform(-4, 4) if pitch is None else pitch)),
                yaw=fb(f32(r.uniform(-4, 4) if yaw is None else yaw)))


def orient_rnd(r):
    return orient_case(r)


# ------------------------------------------------------------------ the original (PowerPC side)


def ppc(e, case):
    ride = e.alloc(64)
    view = e.alloc(64, b"\xa5" * 64)
    keys = e.alloc(16)
    labels = {A["cam"]: 1, A["ma"]: 2, A["mb"]: 3, ride: 4, keys: 5, A["clock"]: 6, A["ext"]: 8}
    e.cl = CL.PpcLog(labels, {"ReadFixed": [s32(case["now"])], "Height": case["height"], "GridOut": case["gridout"], "Grid": case["grid"],
                              "RideRide": case["riderride"], "Tiny": case["tiny"], "Bump": case["bump"]})
    st = CL.Stubs()

    def read_fixed(e):
        value = s32(e.cl.ret("ReadFixed"))
        e.cl.rec("ReadFixed", [e.cl.lab(e.r(3)), value])
        e.set_r(3, value & MASK)

    def get_state(e):
        ident = e.r(4) & 0xFFFF
        ident = ident - 0x10000 if ident & 0x8000 else ident
        value = 1 if (0 <= ident < 32 and (case["keys"] >> ident) & 1) else 0
        e.cl.rec("GetState", [e.cl.lab(e.r(3)), ident, value])
        e.set_r(3, value)

    def height(e):
        x, z = fb(e.f(1)), fb(e.f(2))
        e.cl.rec("DetermineHFieldHeight", [x, z, s32(e.r(5))])
        e.set_f1(struct.unpack(">f", struct.pack(">I", e.cl.ret("Height") & MASK))[0])

    def grid(e):
        a, b, o = e.r(3), e.r(4), e.r(5)
        e.cl.rec("GetNextGridsquare", [e.u32(a + 4 * i) for i in range(3)] + [e.u32(b + 4 * i) for i in range(3)])
        e.write(o, struct.pack(">III", e.cl.ret("GridOut") & MASK, e.cl.ret("GridOut") & MASK, e.cl.ret("GridOut") & MASK))
        e.set_r(3, e.cl.ret("Grid") & MASK)

    def riderride(e):
        p = e.r(3)
        e.cl.rec("RideRideIfRequired", [e.u32(p + 4 * i) for i in range(3)])
        e.set_r(3, e.cl.ret("RideRide") & MASK)

    def tiny(e):
        e.cl.rec("MatrixDifferenceTiny", [e.cl.lab(e.r(3)), e.cl.lab(e.r(4))])
        e.set_r(3, e.cl.ret("Tiny") & MASK)

    def bump(e):
        e.cl.rec("ApplyHeadsOnRidesBumpage", [e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), fb(e.f(1)), e.cl.lab(e.r(6))])
        e.write(e.r(6), struct.pack(">" + "I" * 16, *[e.cl.ret("Bump") & MASK for _ in range(16)]))

    def load(e):
        e.cl.rec("Load", [e.cl.lab(e.r(4))])
        e.write(e.r(3), e.read(e.r(4), 64))

    st.internal(0x1014733C, read_fixed)
    st.internal(0x1014FFF0, get_state)
    st.internal(0x1005EA70, height)
    st.internal(0x10033E14, grid)
    st.internal(0x100345F8, riderride)
    st.internal(0x10034784, tiny)
    st.internal(0x100336F4, bump)
    st.imp("Load__7CMatrixFR7sMatrix", load)
    st.imp("fmod", fmod)
    st.real.update([0x100C0E28, 0x100C174C, 0x100C1944, 0x100C1DB4, 0x100C1698, 0x100C249C, 0x100C28FC])

    def words(addr, ws):
        e.write(addr, struct.pack(">" + "I" * len(ws), *[v & MASK for v in ws]))

    words(A["globals"] + 4, [case["mask"], case["scale"]])
    words(A["globals"] + 0xC, case["table"])
    words(A["stamp"], [case["stamp"]])
    words(A["modeA"], [case["modeA"], case["modeB"], case["f5aac"]])
    words(A["f5bf4"], [case["f5bf4"]])
    words(A["f68"], [case["f68"]])
    words(A["start"], case["start"])
    words(A["pos"], case["pos"])
    words(A["vel"], case["vel"])
    words(A["yaw"], [case["yaw"]])
    words(A["pitch"], [case["pitch"]])
    words(A["tilt"], case["tilt"])
    words(A["ground"], [case["ground"]])
    words(A["turn"], [case["turn"]])
    words(A["fpitch"], [case["fpitch"]])
    words(A["anchor"], case["anchor"])
    words(A["flags"], [case["flags"]])
    e.write(A["settings"] + 0x3A, bytes([case["iso"]]))
    e.write(A["settings"] + 0x38, bytes([case["rmb"]]))
    words(A["tpw"], [case["tpw"]])
    words(A["ptr"], case["ptr"])
    words(A["cx"], [case["cx"]])
    words(A["cy"], [case["cy"]])
    words(A["hw"], [case["hw"]])
    words(A["hh"], [case["hh"]])
    e.write(A["wideOnce"], bytes([case["wideOnce"]]))
    words(A["wideMin"], [case["wideMin"]])
    e.write(A["wideMaxOnce"], bytes([case["wideMaxOnce"]]))
    words(A["wideMax"], [case["wideMax"]])
    e.write(A["walk"], bytes([case["walk"]]))
    words(A["f59"], [case["f59"]])
    words(A["keys"], [keys])
    words(A["target"], case["target"])
    words(A["vp"], case["vp"])
    words(A["ride"], [ride if case["rideSet"] else 0])
    words(ride, case["ride"])
    words(A["cam"], case["cam"])
    words(A["ma"], case["ma"])
    words(A["mb"], case["mb"])
    words(A["ext"], case["ext"])
    e.write(A["inv"], b"\xa5" * 64)
    words(A["current"], [0])
    vp = e.data_import("gei_ViewMatrix")
    words(vp, [view])
    header = e.alloc(0x100)
    field = e.alloc(0x30)
    words(A["header"], [header])
    words(header + 0x6C, [field])
    words(field + 0x08 + 8, case["field"][0:2])
    words(field + 0x18, case["field"][2:4])
    heights = e.alloc(64) if case["hasHeights"] else 0
    quads = e.alloc(64) if case["hasQuads"] else 0
    words(field + 0x28, [heights, quads])
    old = st.activate(e)
    try:
        with HardwareRounding(e, FUSED):
            T.call_ppc(e, ENTRY, [])
    finally:
        CL.Stubs.deactivate(e, old)

    def u(addr, n=1):
        return [e.u32(addr + 4 * i) for i in range(n)]
    current = e.u32(A["current"])
    out = u(A["stamp"]) + u(A["modeA"], 3) + u(A["f5bf4"]) + u(A["f68"]) + u(A["pos"], 3) + u(A["vel"], 3) + u(A["yaw"]) + u(A["pitch"])
    out += u(A["tilt"], 2) + u(A["ground"]) + u(A["turn"]) + u(A["fpitch"]) + u(A["anchor"], 2) + u(A["flags"])
    out += [e.read(A["wideOnce"], 1)[0]] + u(A["wideMin"]) + [e.read(A["wideMaxOnce"], 1)[0]] + u(A["wideMax"]) + [e.read(A["walk"], 1)[0]]
    out += u(A["f59"]) + u(A["target"], 3) + u(A["vp"], 3)
    out += [1 if current == A["cam"] else 8 if current == A["ext"] else 9 if current else 0]
    out += u(A["cam"], 16) + u(A["ma"], 16) + u(A["mb"], 16) + u(A["inv"], 16) + u(view, 16) + u(ride, 16)
    return [canon(w) for w in out], e.cl.log


# ------------------------------------------------------------------ the translation (host side)


def host(lib, case):
    CL.host_begin(lib, {}, {"ReadFixed": [s32(case["now"])], "Height": case["height"], "GridOut": case["gridout"], "Grid": case["grid"],
                            "RideRide": case["riderride"], "Tiny": case["tiny"], "Bump": case["bump"]})
    lib.tpw_cam_labels()

    def arr(ws):
        return (C.c_uint32 * (len(ws) + 1))(*[v & MASK for v in ws])

    lib.tpw_cam_table.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_cam_table(arr(case["table"]), len(case["table"]))
    lib.tpw_cam_field.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_cam_field(1, case["field"][0], case["field"][1], case["field"][2], case["field"][3], case["hasHeights"], case["hasQuads"])
    g = [case["mask"], case["scale"], case["stamp"], case["modeA"], case["modeB"], case["f5aac"], case["f5bf4"], case["f68"]]
    g += case["pos"] + case["vel"] + case["start"]
    g += [case["yaw"], case["pitch"]] + case["tilt"] + [case["ground"], case["turn"], case["fpitch"]] + case["anchor"]
    g += [case["flags"], case["iso"], case["rmb"], case["tpw"]] + case["ptr"] + [case["cx"], case["cy"], case["hw"], case["hh"]]
    g += [case["wideOnce"], case["wideMin"], case["wideMaxOnce"], case["wideMax"], case["walk"], case["f59"], case["keys"]]
    g += case["target"] + case["vp"] + [case["rideSet"]] + case["ride"] + case["cam"] + case["ma"] + case["mb"] + case["ext"]
    out = (C.c_uint32 * 400)()
    lib.tpw_cam_run.argtypes = [C.c_void_p, C.c_void_p]
    lib.tpw_cam_run(arr(g), out)
    lib.tpw_cam_field(0, 0, 0, 0, 0, 0, 0)
    return [canon(w) for w in list(out)[:LENGTH]], CL.host_end(lib)


# stamp, modes (3), f5bf4, f68, pos (3), vel (3), yaw, pitch, tilt (2), ground, turn, fpitch, anchor (2), flags, wide once, min, max once,
# max, walk, f59, target (3), view point (3), current, six matrices
LENGTH = 131


def ppc_orient(e, case):
    matrix = e.alloc(64)
    e.write(matrix, struct.pack(">" + "I" * 16, *case["m"]))
    e.cl = CL.PpcLog({})
    st = CL.Stubs()
    st.real.update([0x100C1698])
    old = st.activate(e)
    try:
        with HardwareRounding(e, FUSED):
            T.call_ppc(e, ORIENT, [matrix, T.F(struct.unpack(">f", struct.pack(">I", case["pitch"]))[0]),
                                   T.F(struct.unpack(">f", struct.pack(">I", case["yaw"]))[0])])
    finally:
        CL.Stubs.deactivate(e, old)
    return [canon(e.u32(matrix + 4 * i)) for i in range(16)], e.cl.log


def host_orient(lib, case):
    CL.host_begin(lib, {})
    out = (C.c_uint32 * 16)()
    lib.tpw_cam_orient.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p]
    lib.tpw_cam_orient((C.c_uint32 * 16)(*case["m"]), case["pitch"], case["yaw"], out)
    return [canon(w) for w in list(out)], CL.host_end(lib)


TESTS = [FnTest("HandleCamera", ppc, host, edge=edges, rnd=rnd), FnTest("SetLMatrixOrientationHi", ppc_orient, host_orient,
                                                                         edge=orient_edges, rnd=orient_rnd)]
