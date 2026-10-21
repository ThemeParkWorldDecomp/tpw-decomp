"""Differential test for src/game/TPCamera.cpp and src/game/park/TPHField.cpp (main app, unit w140): GetNextGridsquare 0x10033e14,
DetermineHFieldHeight 0x1005ea70, DetermineHFieldHeightQuick 0x1005eda8, MatrixDifferenceTiny 0x10034784, ApplyHeadsOnRidesBumpage
0x100336f4, RoundRotationToNearest90Degrees 0x100346a0, SetCameraPOI 0x10033650, RideRideIfRequired 0x100345f8 and
AllowFullCameraZoomout 0x10032c08, against libtpw_unit_CameraCallees.dylib.

Real on both sides: the nine functions and __cvt_fp2unsigned 0x10237cd8. Fakes on both sides that record every argument:
MAP_wall_in_way 0x10101ca4 (answers the "Wall" queue), GetThingAtPosnIfEntrance 0x1003277c (the case's object or null),
CObject::GoFirstPerson 0x1010ed8c and CRideDatabase::GetRideInfo 0x10154868 (the Mac passes the 2-byte type id in the top half of r4;
answers null). The C library calls (modf, fmod) are Python's math, which is the host's libm.

A case is a dict of words. Compared: the answer, every global or output word written, and the call logs.

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (a code hook per fused instruction) gives each the
processor's single rounding.

Not tested: NaN and infinite inputs; SetCameraPOI with a non-null x and a null z (the original dereferences null, as the translation).
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
SHIM_LIBRARY = "unit_CameraCallees"
GRID, HEIGHT, QUICK, TINY, BUMP, ROUND, POI, RIDE, ZOOM = (0x10033E14, 0x1005EA70, 0x1005EDA8, 0x10034784, 0x100336F4, 0x100346A0,
                                                          0x10033650, 0x100345F8, 0x10032C08)
MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000
FUSED = [(GRID, GRID + 1948), (HEIGHT, HEIGHT + 776), (QUICK, QUICK + 676), (BUMP, BUMP + 268)]
A = dict(header=0x102C6C7C, rdb=0x1037F6C8, modeA=0x102B5AA4, pos=0x1029CC78, f68=0x1029CC68, f5bf4=0x102B5BF4, vel=0x102B5BB4,
         poi=0x1029CC88)


def f32(x):
    return T.f32(x)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(w):
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def canon(word):
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x7FFFFF):
        return NAN_BITS
    return word


def s32(v):
    v &= MASK
    return v - (1 << 32) if v & 0x80000000 else v


def fr(r, lo, hi):
    return fb(f32(r.uniform(lo, hi)))


def fmod(e):
    x, y = e.f(1), e.f(2)
    if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
        e.set_f1(math.nan)
    else:
        e.set_f1(math.fmod(x, y))


def modf(e):
    """modf(double, double*): the fraction in f1, the whole part stored through r5."""
    fraction, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(fraction)


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


def words(e, addr, ws):
    e.write(addr, struct.pack(">" + "I" * len(ws), *[v & MASK for v in ws]))


def run(e, st, entry, args):
    old = st.activate(e)
    try:
        with HardwareRounding(e, FUSED):
            return T.call_ppc(e, entry, args)
    finally:
        CL.Stubs.deactivate(e, old)


def arr(ws):
    return (C.c_uint32 * (len(ws) + 1))(*[v & MASK for v in ws])


def prep_host(lib, queues=None):
    CL.host_begin(lib, {}, queues or {})
    lib.tpw_cc_labels()


# ------------------------------------------------------------------ the height lookups

POOL_FLAGS = [0, 0, 0x800, 0x800, 4, 4, 0x804, 0x804, 1, 0x801, 5, 0x805, 0x2, 0x10, 0x1000, 0x8004]


def make_field(r, w=None, d=None):
    w = w or r.randint(1, 8)
    d = d or r.randint(1, 8)
    sx = 10.0 if r.random() < 0.6 else f32(r.uniform(2.0, 14.0))
    sz = sx if r.random() < 0.6 else f32(r.uniform(2.0, 14.0))
    ox = 0.0 if r.random() < 0.6 else f32(r.uniform(-10.0, 15.0))
    oz = 0.0 if r.random() < 0.6 else f32(r.uniform(-10.0, 15.0))
    heights = [fr(r, -5.0, 40.0) for _ in range((w + 1) * (d + 1))]
    quads = [(r.choice(POOL_FLAGS) << 16) | r.getrandbits(16) for _ in range(w * d)]
    return dict(p=[fb(ox), fb(oz), fb(sx), fb(sz), w, d], heights=heights, quads=quads)


def point(r, fld, quick):
    p = fld["p"]
    ox, oz, sx, sz, w, d = bf(p[0]), bf(p[1]), bf(p[2]), bf(p[3]), p[4], p[5]
    if quick:
        ox = oz = 0.0
    fracs = [0.0, 0.0, 0.25, 0.5, 0.75, 0.999, 0.9999, 1.0, 0.1, 0.4999, 0.5001]
    if r.random() < 0.15:
        return [fr(r, -15.0, 10.0 * w + 15.0), fr(r, -15.0, 10.0 * d + 15.0)]
    ix = r.randint(-1, w) if r.random() < 0.15 else r.randint(0, w - 1)
    iz = r.randint(-1, d) if r.random() < 0.15 else r.randint(0, d - 1)
    fx = r.choice(fracs) if r.random() < 0.6 else r.random()
    fz = r.choice(fracs) if r.random() < 0.6 else r.random()
    if r.random() < 0.3:
        fz = fx
    if r.random() < 0.15:
        fz = 1.0 - fx
    return [fb(f32((ix + fx) * sx + ox)), fb(f32((iz + fz) * sz + oz))]


def height_case(r, quick, **kw):
    fld = make_field(r)
    c = dict(fld=fld, xz=point(r, fld, quick), flag=r.choice([0, 0, 1, 5, -1]), quick=quick)
    c.update(kw)
    return c


def height_edges(quick):
    def gen():
        r = random.Random(77 + quick)
        for flags in POOL_FLAGS:
            for flag in (0, 1):
                for _ in range(3):
                    fld = make_field(r)
                    fld["quads"] = [(flags << 16) | r.getrandbits(16) for _ in fld["quads"]]
                    yield dict(fld=fld, xz=point(r, fld, quick), flag=flag, quick=quick)
        for w, d in ((1, 1), (2, 3), (8, 8)):
            fld = make_field(r, w, d)
            fld["p"][:4] = [fb(0.0), fb(0.0), fb(10.0), fb(10.0)]
            for x in (0.0, 10.0 * w - 0.001, 10.0 * w, 10.0 * w + 0.01, -0.001, 5.0, 9.999, 10.0):
                for z in (0.0, 10.0 * d - 0.001, 10.0 * d, 10.0 * d + 0.01, -0.001, 5.0, 9.999, 10.0):
                    yield dict(fld=fld, xz=[fb(f32(x)), fb(f32(z))], flag=r.choice([0, 1]), quick=quick)
    return gen


def ppc_height(e, case):
    fld = case["fld"]
    p, w, d = fld["p"], fld["p"][4], fld["p"][5]
    header, field = e.alloc(0x100), e.alloc(0x30)
    heights = e.alloc(4 * len(fld["heights"]) + 512)
    quads = e.alloc(4 * len(fld["quads"]) + 512)
    words(e, heights, fld["heights"])
    words(e, quads, fld["quads"])
    words(e, A["header"], [header])
    words(e, header + 0x6C, [field])
    words(e, field + 8, p[:4])
    words(e, field + 0x18, p[4:])
    words(e, field + 0x28, [heights, quads])
    e.cl = CL.PpcLog({})
    st = CL.Stubs()
    st.imp("modf", modf)
    x, z = bf(case["xz"][0]), bf(case["xz"][1])
    if case["quick"]:
        r3, r4, f1 = run(e, st, QUICK, [T.F(x), T.F(z)])
    else:
        r3, r4, f1 = run(e, st, HEIGHT, [T.F(x), T.F(z), case["flag"] & MASK])
    return [canon(fb(f1))], e.cl.log


def host_height(lib, case):
    fld = case["fld"]
    prep_host(lib)
    lib.tpw_cc_field.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_void_p, C.c_int32]
    quads = b"".join(struct.pack("<HH", q >> 16, q & 0xFFFF) for q in fld["quads"])
    lib.tpw_cc_field(1, arr(fld["p"]), arr(fld["heights"]), len(fld["heights"]), C.c_char_p(quads), len(fld["quads"]))
    out = (C.c_uint32 * 4)()
    lib.tpw_cc_height.argtypes = [C.c_void_p, C.c_int32, C.c_void_p]
    lib.tpw_cc_height(arr(case["xz"] + [case["flag"]]), case["quick"], out)
    lib.tpw_cc_field(0, None, None, 0, None, 0)
    return [canon(out[0])], CL.host_end(lib)


# ------------------------------------------------------------------ GetNextGridsquare


def grid_case(r, **kw):
    def pos():
        k = r.random()
        if k < 0.25:
            return r.choice([0.0, 10.0, 20.0, 9.999, 10.001, 19.999, 5.0, 100.0, 95.0, 0.0005])
        if k < 0.3:
            return r.uniform(-8.0, 0.0)
        return r.uniform(0.0, 150.0)

    def step():
        k = r.random()
        if k < 0.15:
            return 0.0
        if k < 0.35:
            return r.choice([1.0, -1.0, 10.0, -10.0, 0.5, -0.5, 3.0, -3.0, 100.0, -100.0, 1e-5, -1e-5])
        return r.uniform(-30.0, 30.0) * r.choice([1.0, 0.01, 1.0, 10.0])
    frm = [fb(f32(pos())), fb(f32(r.choice([0.0, 0.0, r.uniform(-3, 8)]))), fb(f32(pos()))]
    stp = [fb(f32(step())), fb(f32(r.choice([0.0, 0.0, r.uniform(-3, 3)]))), fb(f32(step()))]
    if r.random() < 0.15:
        stp[2] = stp[0] if r.random() < 0.5 else fb(-bf(stp[0]))
    c = dict(frm=frm, stp=stp, wall=[r.choice([0, 0, 1]) for _ in range(4)])
    c.update(kw)
    return c


def grid_edges():
    r = random.Random(555)
    for _ in range(40):
        yield grid_case(r)
    for sx in (0.0, 1.0, -1.0, 7.0, -7.0, 40.0, -40.0):
        for sz in (0.0, 1.0, -1.0, 7.0, -7.0, 40.0, -40.0):
            for start in (0.0, 10.0, 9.999, 3.0):
                for wall in ([0] * 4, [1] * 4, [1, 0, 0, 0]):
                    yield dict(frm=[fb(start), 0, fb(start + 5.0)], stp=[fb(sx), 0, fb(sz)], wall=wall)
    for t in (1.0, 2.0, 0.5):
        yield dict(frm=[fb(5.0), 0, fb(5.0)], stp=[fb(t), 0, fb(t)], wall=[0, 0, 0, 0])
        yield dict(frm=[fb(5.0), 0, fb(5.0)], stp=[fb(-t), 0, fb(-t)], wall=[0, 0, 0, 0])
        yield dict(frm=[fb(5.0), 0, fb(5.0)], stp=[fb(t), 0, fb(-t)], wall=[1, 0, 0, 0])


def ppc_grid(e, case):
    a = e.alloc(12)
    b = e.alloc(12)
    o = e.alloc(12, b"\xa5" * 12)
    words(e, a, case["frm"])
    words(e, b, case["stp"])
    e.cl = CL.PpcLog({}, {"Wall": case["wall"]})
    st = CL.Stubs()

    def wall(e):
        args = [e.r(3) & 0xFF if False else e.r(3), e.r(4), e.r(5), e.r(6)]
        e.cl.rec("MAP_wall_in_way", [args[0], args[1], args[2], s32(args[3])])
        e.set_r(3, e.cl.ret("Wall") & MASK)
    st.internal(0x10101CA4, wall)
    st.imp("modf", modf)
    r3, r4, f1 = run(e, st, GRID, [a, b, o])
    return [r3 & MASK] + [canon(e.u32(o + 4 * i)) for i in range(3)], e.cl.log


def host_grid(lib, case):
    prep_host(lib, {"Wall": case["wall"]})
    out = (C.c_uint32 * 4)()
    lib.tpw_cc_grid.argtypes = [C.c_void_p, C.c_void_p]
    lib.tpw_cc_grid(arr(case["frm"] + case["stp"]), out)
    return [out[0]] + [canon(out[i]) for i in range(1, 4)], CL.host_end(lib)


# ------------------------------------------------------------------ the matrix callees


def matrix_case(r):
    a = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    b = list(a)
    mode = r.random()
    if mode < 0.5:
        for i in r.sample(range(16), r.randint(0, 4)):
            b[i] = fb(f32(bf(a[i]) + r.choice([0.0, 1e-6, -1e-6, 3e-6, -3e-6, 1e-5, -1e-5, 1e-4, r.uniform(-1e-4, 1e-4)])))
    elif mode < 0.7:
        i = r.randrange(16)
        b[i] = fb(f32(bf(a[i]) + r.choice([1e-6, 2e-6, 5e-6, 9.99e-6, 1.0001e-5, 1.2e-5, 1e-3])))
    elif mode < 0.8:
        b = [fb(f32(r.uniform(-2, 2))) for _ in range(16)]
    return dict(a=a, b=b)


def tiny_edges():
    r = random.Random(31)
    base = [fb(f32(r.uniform(-1, 1))) for _ in range(16)]
    yield dict(a=base, b=list(base))
    for i in range(16):
        for delta in (1e-6, 9e-6, 1.1e-5, 1e-4, -1e-4):
            b = list(base)
            b[i] = fb(f32(bf(base[i]) + delta))
            yield dict(a=base, b=b)
    for n in (3, 6, 12):
        for delta in (4e-7, 8.3e-7, 8.4e-7, 1e-6):
            b = list(base)
            for i in [0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14][:n]:
                b[i] = fb(f32(bf(base[i]) + delta))
            yield dict(a=base, b=b)
    yield dict(a=base, b=[0] * 16)


def ppc_tiny(e, case):
    a, b = e.alloc(64), e.alloc(64)
    words(e, a, case["a"])
    words(e, b, case["b"])
    e.cl = CL.PpcLog({a: 3, b: 4})
    r3, r4, f1 = run(e, CL.Stubs(), TINY, [a, b])
    return [r3 & MASK], e.cl.log


def host_tiny(lib, case):
    prep_host(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_cc_tiny.argtypes = [C.c_void_p, C.c_void_p]
    lib.tpw_cc_tiny(arr(case["a"] + case["b"]), out)
    return [out[0]], CL.host_end(lib)


def bump_case(r, dt=None):
    ride = [fb(f32(r.uniform(-1.5, 1.5))) for _ in range(16)]
    prev = [fb(f32(r.uniform(-1.5, 1.5))) for _ in range(16)]
    if dt is None:
        dt = r.choice([0.0, 0.5, 1.0, 1.0001, 2.0, 100.0, 500.0, 999.0, 1000.0, 1000.5, 5000.0, -3.0, r.uniform(0, 1100), r.uniform(0, 1100)])
    return dict(ride=ride, prev=prev, dt=fb(f32(dt)))


def bump_edges():
    r = random.Random(8)
    for dt in (0.0, 0.5, 1.0, 1.5, 999.0, 1000.0, 1001.0, 1e9, -1.0, 0.99999, 500.0):
        yield bump_case(r, dt)


def ppc_bump(e, case):
    ride, prev, out = e.alloc(64), e.alloc(64), e.alloc(64, b"\xa5" * 64)
    words(e, ride, case["ride"])
    words(e, prev, case["prev"])
    e.cl = CL.PpcLog({})
    run(e, CL.Stubs(), BUMP, [ride, prev, T.F(bf(case["dt"])), out])
    return [canon(e.u32(out + 4 * i)) for i in range(16)], e.cl.log


def host_bump(lib, case):
    prep_host(lib)
    out = (C.c_uint32 * 16)()
    lib.tpw_cc_bump.argtypes = [C.c_void_p, C.c_void_p]
    lib.tpw_cc_bump(arr(case["ride"] + case["prev"] + [case["dt"]]), out)
    return [canon(w) for w in out], CL.host_end(lib)


# ------------------------------------------------------------------ the small ones


def round_case(r):
    k = r.random()
    if k < 0.4:
        y = (r.randint(-9, 9) + r.choice([0.0, 0.0, 0.25, -0.25, 0.5, -0.5, 0.2499, 0.2501, 0.7499, 0.75, 0.001, 0.4999, 0.5001])) * 1.5707964
    elif k < 0.5:
        y = r.choice([0.0, 0.7853982, -0.7853982, 1.5707964, -1.5707964, 3.1415927, -3.1415927, 6.2831855, 0.7853981, 0.7853983])
    else:
        y = r.uniform(-30, 30)
    return dict(yaw=fb(f32(y)))


def round_edges():
    r = random.Random(12)
    for y in (0.0, 0.7853982, -0.7853982, 1.5707964, -1.5707964, 3.1415927, -3.1415927, 6.2831855, 0.7853981, 0.7853983, 2.3561945,
              -2.3561945, 0.3, -0.3, 1.0, -1.0, 100.0, -100.0):
        yield dict(yaw=fb(f32(y)))
    for _ in range(20):
        yield round_case(r)


def ppc_round(e, case):
    words(e, A["f5bf4"], [case["yaw"]])
    e.cl = CL.PpcLog({})
    st = CL.Stubs()
    st.imp("fmod", fmod)
    run(e, st, ROUND, [])
    return [canon(e.u32(A["f5bf4"]))], e.cl.log


def host_round(lib, case):
    prep_host(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_cc_round.argtypes = [C.c_uint32, C.c_void_p]
    lib.tpw_cc_round(case["yaw"], out)
    return [canon(out[0])], CL.host_end(lib)


def poi_case(r):
    mask = r.choice([0, 1 | 2, 4, 8, 16, 1 | 2 | 4 | 8 | 16, 4 | 8, 1 | 2 | 16, 31, 31, 0, 4 | 16, 8 | 16])
    return dict(mask=mask, vals=[fr(r, -50, 800) for _ in range(5)], start=[fr(r, -5, 5) for _ in range(9)])


def poi_edges():
    r = random.Random(3)
    for mask in (0, 3, 4, 8, 16, 31, 7, 12, 19, 24, 28, 12 | 16):
        c = poi_case(r)
        c["mask"] = mask
        yield c


def ppc_poi(e, case):
    ptrs = []
    for i in range(5):
        p = e.alloc(4)
        words(e, p, [case["vals"][i]])
        ptrs.append(p if (case["mask"] >> i) & 1 else 0)
    s = case["start"]
    words(e, A["pos"], s[0:3])
    words(e, A["f68"], [s[3]])
    words(e, A["f5bf4"], [s[4]])
    words(e, A["vel"], s[5:8])
    words(e, A["poi"], [s[8]])
    e.cl = CL.PpcLog({})
    run(e, CL.Stubs(), POI, ptrs)
    u = lambda a, n=1: [canon(e.u32(a + 4 * i)) for i in range(n)]
    return u(A["pos"], 3) + u(A["f68"]) + u(A["f5bf4"]) + u(A["vel"], 3) + u(A["poi"]), e.cl.log


def host_poi(lib, case):
    prep_host(lib)
    out = (C.c_uint32 * 9)()
    lib.tpw_cc_poi.argtypes = [C.c_void_p, C.c_void_p]
    lib.tpw_cc_poi(arr([case["mask"]] + case["vals"] + case["start"]), out)
    return [canon(w) for w in out], CL.host_end(lib)


def zoom_case(r):
    return dict(mode=r.choice([0, 0x200, 0xFFFFFFFF, 0x80, 0x280, r.getrandbits(32), r.getrandbits(32)]))


def zoom_edges():
    for m in (0, 0x200, 0x100, 0x400, 0xFFFFFDFF, 0xFFFFFFFF, 0x80, 0x280):
        yield dict(mode=m)


def ppc_zoom(e, case):
    words(e, A["modeA"], [case["mode"]])
    e.cl = CL.PpcLog({})
    r3, r4, f1 = run(e, CL.Stubs(), ZOOM, [])
    return [r3 & MASK, e.u32(A["modeA"])], e.cl.log


def host_zoom(lib, case):
    prep_host(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_cc_zoom.argtypes = [C.c_uint32, C.c_void_p]
    lib.tpw_cc_zoom(case["mode"], out)
    return [out[0], out[1]], CL.host_end(lib)


def ride_case(r):
    return dict(x=fr(r, -5, 300), z=fr(r, -5, 300), y=fr(r, -5, 20), present=r.choice([0, 1, 1]), kind=r.choice([0, 1, 7, 0x123, 0xFFFF, r.getrandbits(16)]))


def ride_edges():
    r = random.Random(5)
    for present in (0, 1):
        for kind in (0, 1, 0x8000, 0xFFFF, 0x1234):
            c = ride_case(r)
            c.update(present=present, kind=kind)
            yield c


def ppc_ride(e, case):
    point = e.alloc(12)
    words(e, point, [case["x"], case["y"], case["z"]])
    obj = e.alloc(0x448)
    e.write(obj + 0x0E, struct.pack(">H", case["kind"]))
    e.cl = CL.PpcLog({A["rdb"]: 1, obj: 2})
    st = CL.Stubs()

    def thing(e):
        e.cl.rec("GetThingAtPosnIfEntrance", [fb(e.f(1)), fb(e.f(2))])
        e.set_r(3, obj if case["present"] else 0)

    def info(e):
        e.cl.rec("GetRideInfo", [e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF])
        e.set_r(3, 0)

    def first(e):
        e.cl.rec("GoFirstPerson", [e.cl.lab(e.r(3)), s32(e.r(4))])
    st.internal(0x1003277C, thing)
    st.internal(0x10154868, info)
    st.internal(0x1010ED8C, first)
    r3, r4, f1 = run(e, st, RIDE, [point])
    return [r3 & MASK], e.cl.log


def host_ride(lib, case):
    prep_host(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_cc_ride.argtypes = [C.c_void_p, C.c_void_p]
    lib.tpw_cc_ride(arr([case["x"], case["z"], case["y"], case["present"], case["kind"]]), out)
    return [out[0]], CL.host_end(lib)


def rnd_height(quick):
    return lambda r: height_case(r, quick)


TESTS = [
    FnTest("GetNextGridsquare", ppc_grid, host_grid, edge=grid_edges, rnd=grid_case),
    FnTest("DetermineHFieldHeight", ppc_height, host_height, edge=height_edges(0), rnd=rnd_height(0)),
    FnTest("DetermineHFieldHeightQuick", ppc_height, host_height, edge=height_edges(1), rnd=rnd_height(1)),
    FnTest("MatrixDifferenceTiny", ppc_tiny, host_tiny, edge=tiny_edges, rnd=matrix_case),
    FnTest("ApplyHeadsOnRidesBumpage", ppc_bump, host_bump, edge=bump_edges, rnd=bump_case),
    FnTest("RoundRotationToNearest90Degrees", ppc_round, host_round, edge=round_edges, rnd=round_case),
    FnTest("SetCameraPOI", ppc_poi, host_poi, edge=poi_edges, rnd=poi_case),
    FnTest("RideRideIfRequired", ppc_ride, host_ride, edge=ride_edges, rnd=ride_case),
    FnTest("AllowFullCameraZoomout", ppc_zoom, host_zoom, edge=zoom_edges, rnd=zoom_case),
]
