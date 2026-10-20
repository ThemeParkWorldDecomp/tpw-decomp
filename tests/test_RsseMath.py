"""Unit w170: sixteen matrix and vector helpers of rsse.cpp, against libtpw_unit_RsseMath.dylib (tests/unit_RsseMath.cpp), main app
("SimTheme Park"), src/rsse_math.cpp:
  SetLMatrixIdentityNoTrans 0x100c0eac, GetLMatrixRotationAroundYAxis 0x100c0f24, GetVectorRotationAroundXAxisFromZ 0x100c1040 (reference)
  and 0x100c11a4 (pointer), GetVectorRotationAroundYAxisFromZ 0x100c10b8 and 0x100c121c, GetVectorRotationAroundZAxisFromY 0x100c112c and
  0x100c1290, XRotateLMatrix 0x100c1308, YRotateLMatrix 0x100c1458, ZRotateLMatrix 0x100c152c, ScaleLMatrixNoTrans 0x100c1d0c (three floats),
  VectorThroughLMatrixNoWNoTrans 0x100c2040, CombineMatricesNoWNoTransDest2nd 0x100c2b5c, DistanceBetweenPoints 0x100c3198 and
  FloatSortByArray 0x100c2d50.

A case is the arguments of one call. The matrices and points are built twice from the same words (big-endian in emulator memory for the
original, host memory for the translation); the compared values are every word of each matrix and point the call receives (as floats, bit
exact, every NaN one value) and the float returned. FloatSortByArray gets records of four random words in both memories (the key is the float
at the case's byte offset of each) and two arrays of pointers (32-bit words in the emulator, 64-bit on the host); both arrays are compared
after the call as record labels (the record's index, 0 for NULL), so the order of a stable merge and which array holds the result show.
No callee is real outside the unit: atan2 (an import) is Python's math.atan2 on the PowerPC side and the host's libm on the other; sqrt comes
from install_std_stubs (Python's math.sqrt, the same correctly rounded root as the host's).

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (tests/test_Camera.py) gives each fused single instruction in
the unit's functions the processor's single rounding. fmadd (double, DistanceBetweenPoints) is exact in Unicorn.

Not tested: NaN payloads beyond the canonical one (the processors make different NaNs); a matrix that shares memory with a point; a matrix
shared with the point or the second matrix of CombineMatricesNoWNoTransDest2nd except the same-matrix case; FloatSortByArray with pointers
above 32 bits (the original's are 32 bits, the host's native) and keys read past a record (a record is 16 bytes, the offset at most 12).
"""
import ctypes as C
import math
import random
import struct

import tpwtest as T
from tpwtest import FnTest
from test_Camera import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RsseMath"
MASK = 0xFFFFFFFF
INF, NAN = float("inf"), float("nan")

# (test name, entry, size, kind); the kind names the arguments (m, m2 matrices, p, q points, floats; "sort" is FloatSortByArray).
FUNCS = [
    ("SetLMatrixIdentityNoTrans", 0x100C0EAC, 64, "m"),
    ("GetLMatrixRotationAroundYAxis", 0x100C0F24, 104, "m_ret"),
    ("GetVectorRotationAroundXAxisFromZ(s3DPoint&)", 0x100C1040, 52, "p_ret"),
    ("GetVectorRotationAroundYAxisFromZ(s3DPoint&)", 0x100C10B8, 48, "p_ret"),
    ("GetVectorRotationAroundZAxisFromY(s3DPoint&)", 0x100C112C, 52, "p_ret"),
    ("GetVectorRotationAroundXAxisFromZ(s3DPoint*)", 0x100C11A4, 52, "p_ret"),
    ("GetVectorRotationAroundYAxisFromZ(s3DPoint*)", 0x100C121C, 48, "p_ret"),
    ("GetVectorRotationAroundZAxisFromY(s3DPoint*)", 0x100C1290, 52, "p_ret"),
    ("XRotateLMatrix", 0x100C1308, 132, "m_cs"),
    ("YRotateLMatrix", 0x100C1458, 164, "m_cs"),
    ("ZRotateLMatrix", 0x100C152C, 148, "m_cs"),
    ("ScaleLMatrixNoTrans", 0x100C1D0C, 112, "m_xyz"),
    ("VectorThroughLMatrixNoWNoTrans", 0x100C2040, 132, "m_p"),
    ("CombineMatricesNoWNoTransDest2nd", 0x100C2B5C, 428, "m_m"),
    ("DistanceBetweenPoints", 0x100C3198, 92, "p_q_ret"),
    ("FloatSortByArray", 0x100C2D50, 360, "sort"),
]
FUSED = [(entry, entry + size) for _, entry, size, _ in FUNCS]
# The host library's sizes: sMatrix 0x40 with m[3][3] at 0x3c, s3DPoint 0xc with z at 8.
HOST_LAYOUT = [0x40, 0x3C, 0xC, 8]


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


def bits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def from_bits(w):
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def setup(runner):
    T.install_std_stubs(runner.emu)
    runner.emu.stub("atan2", lambda emu: emu.set_f1(math.atan2(emu.f(1), emu.f(2))))


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    U, P, I = C.c_uint32, C.POINTER(C.c_uint32), C.c_int32
    t = lambda name, res, args: (setattr(getattr(lib, "tpw_rm_" + name), "restype", res),
                                 setattr(getattr(lib, "tpw_rm_" + name), "argtypes", args))
    t("SetLMatrixIdentityNoTrans", None, [P])
    t("GetLMatrixRotationAroundYAxis", U, [P])
    t("GetVectorRotation", U, [I, I, P])
    t("RotateLMatrix", None, [I, P, U, U])
    t("ScaleLMatrixNoTrans", None, [P, U, U, U])
    t("VectorThroughLMatrixNoWNoTrans", None, [P, P])
    t("CombineMatricesNoWNoTransDest2nd", None, [P, P, I])
    t("DistanceBetweenPoints", U, [P, P])
    t("FloatSortByArray", None, [I, C.POINTER(C.c_uint64), C.POINTER(C.c_uint64), I, I])
    lib.tpw_rm_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 8)()
    n = lib.tpw_rm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ------------------------------------------------------------------ the two sides


def observed(kind, ret, m, m2, p, q):
    """The compared values: each word as a float, named with its offset."""
    out = {}
    if "m" in kind:
        out.update({f"m[{i // 4}][{i % 4}] at +0x{4 * i:x}": v for i, v in enumerate(m)})
    if kind == "m_m":
        out.update({f"second m[{i // 4}][{i % 4}] at +0x{4 * i:x}": v for i, v in enumerate(m2)})
    if "p" in kind:
        out.update({f"{'first ' if kind == 'p_q_ret' else ''}point.{c}": v for c, v in zip("xyz", p)})
    if kind == "p_q_ret":
        out.update({f"second point.{c}": v for c, v in zip("xyz", q)})
    if kind.endswith("_ret"):
        out["return value"] = ret
    return out


def inputs(case):
    return (case.get("m", [0.0] * 16), case.get("m2", [0.0] * 16), case.get("p", [0.0] * 3),
            case.get("q", [0.0] * 3), case.get("args", []), bool(case.get("same_matrix")))


def sort_labels(words, records):
    """An array of record addresses as labels: the record's index plus one, 0 for NULL, 999 for anything else."""
    return [0 if w == 0 else (records.index(w) + 1 if w in records else 999) for w in words]


def sort_record_words(case):
    """The words of each record of a FloatSortByArray case: four words, the key at the case's offset (words[offset / 4])."""
    out = []
    for i, key in enumerate(case["keys"]):
        words = list(case["junk"][i])
        words[case["offset"] // 4] = key
        out.append(words)
    return out


def make_test(which):
    name, entry, size, kind = FUNCS[which]

    def ppc_sort(e, case):
        words = sort_record_words(case)
        records = [e.alloc(16, struct.pack(">4I", *w)) for w in words]
        n = max(case["count"], 1)
        src = e.alloc(4 * n + 4, struct.pack(f">{len(records)}I", *[records[i] for i in case["order"]]) + bytes(4 * (n + 1 - len(records))))
        scr = e.alloc(4 * n + 4)
        with HardwareRounding(e, FUSED):
            T.call_ppc(e, entry, [case["count"] & MASK, src, scr, case["depth"], case["offset"]])
        return {"source": sort_labels(struct.unpack(f">{n + 1}I", e.read(src, 4 * n + 4)), records),
                "scratch": sort_labels(struct.unpack(f">{n + 1}I", e.read(scr, 4 * n + 4)), records)}

    def host_sort(lib, case):
        bind(lib)
        words = sort_record_words(case)
        bufs = [(C.c_uint32 * 4)(*[from_bits_w for from_bits_w in w]) for w in words]
        records = [C.addressof(b) for b in bufs]
        n = max(case["count"], 1)
        src = (C.c_uint64 * (n + 1))(*[records[i] for i in case["order"]])
        scr = (C.c_uint64 * (n + 1))()
        lib.tpw_rm_FloatSortByArray(case["count"], src, scr, case["depth"], case["offset"])
        return {"source": sort_labels(list(src), records), "scratch": sort_labels(list(scr), records)}

    def ppc(e, case):
        if kind == "sort":
            return ppc_sort(e, case)
        m, m2, p, q, args, same = inputs(case)
        am = e.alloc(64, struct.pack(">16f", *m))
        am2 = e.alloc(64, struct.pack(">16f", *m2))
        ap = e.alloc(12, struct.pack(">3f", *p))
        aq = e.alloc(12, struct.pack(">3f", *q))
        regs = {"m": [am], "m_ret": [am], "p_ret": [ap], "m_cs": [am] + [T.F(x) for x in args],
                "m_xyz": [am] + [T.F(x) for x in args], "m_p": [am, ap], "m_m": [am, am if same else am2],
                "p_q_ret": [ap, aq]}[kind]
        with HardwareRounding(e, FUSED):
            T.call_ppc(e, entry, regs)
        ret = e.f(1)
        return observed(kind, ret, struct.unpack(">16f", e.read(am, 64)), struct.unpack(">16f", e.read(am2, 64)),
                        struct.unpack(">3f", e.read(ap, 12)), struct.unpack(">3f", e.read(aq, 12)))

    def host(lib, case):
        if kind == "sort":
            return host_sort(lib, case)
        bind(lib)
        m, m2, p, q, args, same = inputs(case)
        U = C.c_uint32
        hm, hm2 = (U * 16)(*map(bits, m)), (U * 16)(*map(bits, m2))
        hp, hq = (U * 3)(*map(bits, p)), (U * 3)(*map(bits, q))
        a = [bits(x) for x in args] + [0, 0, 0]
        ret = 0.0
        if which == 0:
            lib.tpw_rm_SetLMatrixIdentityNoTrans(hm)
        elif which == 1:
            ret = from_bits(lib.tpw_rm_GetLMatrixRotationAroundYAxis(hm))
        elif 2 <= which <= 7:
            ret = from_bits(lib.tpw_rm_GetVectorRotation((which - 2) % 3, 1 if which < 5 else 0, hp))
        elif 8 <= which <= 10:
            lib.tpw_rm_RotateLMatrix(which - 8, hm, a[0], a[1])
        elif which == 11:
            lib.tpw_rm_ScaleLMatrixNoTrans(hm, a[0], a[1], a[2])
        elif which == 12:
            lib.tpw_rm_VectorThroughLMatrixNoWNoTrans(hm, hp)
        elif which == 13:
            lib.tpw_rm_CombineMatricesNoWNoTransDest2nd(hm, hm2, int(same))
        elif which == 14:
            ret = from_bits(lib.tpw_rm_DistanceBetweenPoints(hp, hq))
        return observed(kind, ret, [from_bits(w) for w in hm], [from_bits(w) for w in hm2],
                        [from_bits(w) for w in hp], [from_bits(w) for w in hq])

    return FnTest(name, ppc, host, edge=lambda: EDGES[kind](which), rnd=lambda r: RANDOM[kind](r))


# ------------------------------------------------------------------ the cases

IDENTITY = [float(i % 5 == 0) for i in range(16)]
C30, S30 = f32(math.cos(math.pi / 6)), f32(math.sin(math.pi / 6))
MATRICES = [
    IDENTITY,
    [0.0] * 16,
    [-0.0] * 16,
    [float(i) for i in range(16)],
    [1.0, 2.0, 3.0, 0.0, 2.0, -1.0, 1.0, 0.0, 0.0, 3.0, 2.0, 0.0, 500.0, 100.0, 500.0, 1.0],
    [f32(x) for x in (1e-8, 2.0, 7.0, 2.0, 3.0, 1e8, 2.0, 7.0, 9.0, 3.0, 1e-9, 4.0, 5.0, 3.0, 2.0, 1.0)],
    [C30, 0.0, -S30, 0.0, 0.0, 1.0, 0.0, 0.0, S30, 0.0, C30, 0.0, 10.0, -20.0, 30.0, 1.0],
    [f32(x) for x in (0.1, -0.2, 0.3, 0.4, 0.5, -0.6, 0.7, 0.8, -0.9, 1.1, 1.3, -1.7, 1.9, 2.3, -2.9, 3.1)],
    [1.0, NAN, -0.0, 2.0, INF, 3.0, 0.0, -INF, 4.0, 5.0, NAN, 6.0, 7.0, 8.0, 9.0, NAN],
]
POINTS = [(x, y, z) for x in (0.0, -0.0, 1.0, -1.0) for y in (0.0, -0.0, 1.0, -1.0) for z in (0.0, -0.0, 1.0, -1.0)]
POINTS += [(1.0, 2.0, 3.0), (-5.5, 0.25, 100.0), (f32(1e-30), f32(-1e30), 7.0), (INF, 1.0, -INF), (NAN, 2.0, 3.0),
           (3.0, 4.0, 0.0), (f32(1e20), 0.0, 0.0), (0.0, f32(1e20), 0.0), (0.0, 0.0, f32(1e20)), (f32(3e38), 0.0, f32(-3e38))]
ANGLES = [(1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0), (-0.0, 0.0), (C30, S30), (0.5, 2.0), (f32(0.1), f32(-0.7))]
NEAR = [f32(0.9), f32(-0.9), 0.0, -0.0, 1.0, -1.0, INF, -INF, NAN, f32(0.5)]
NEAR += [math.nextafter(f32(0.9), 1), math.nextafter(f32(0.9), 0), math.nextafter(f32(-0.9), -1), math.nextafter(f32(-0.9), 0)]
NEAR = [f32(x) for x in NEAR]


def edges_m(which):
    return [{"m": list(m)} for m in MATRICES]


def edges_m_ret(which):
    out = edges_m(which)
    for base in (IDENTITY, MATRICES[4], MATRICES[6], MATRICES[7]):
        for v in NEAR:
            m = list(base)
            m[9] = v
            out.append({"m": m})
            for zeros in ((0.0, 1.0, 0.0, 1.0), (-0.0, -1.0, -0.0, -1.0), (0.0, -1.0, -0.0, 1.0)):
                m = list(base)
                m[9], m[0], m[2], m[8], m[10] = v, zeros[1], zeros[0], zeros[2], zeros[3]
                out.append({"m": m})
    return out


def edges_p_ret(which):
    return [{"p": list(p)} for p in POINTS]


def edges_m_cs(which):
    return [{"m": list(m), "args": list(a)} for m in MATRICES for a in ANGLES]


def edges_m_xyz(which):
    scales = [(1.0, 1.0, 1.0), (0.0, 0.0, 0.0), (-1.0, 2.0, 0.5), (-0.0, 1.0, 1.0), (f32(1e30), f32(1e-30), 3.0), (NAN, INF, -1.0)]
    return [{"m": list(m), "args": list(s)} for m in MATRICES for s in scales]


def edges_m_p(which):
    return [{"m": list(m), "p": list(p)} for m in MATRICES for p in POINTS[::4] + POINTS[64:]]


def edges_m_m(which):
    out = [{"m": list(a), "m2": list(b)} for a in MATRICES for b in MATRICES]
    out += [{"m": list(a), "m2": [0.0] * 16, "same_matrix": True} for a in MATRICES]
    return out


def edges_p_q_ret(which):
    out = [{"p": list(p), "q": [0.0, 0.0, 0.0]} for p in POINTS]
    out += [{"p": list(p), "q": list(q)} for p in POINTS[64:] for q in POINTS[64:]]
    return out


def rnd_value(r, lo=-1000.0, hi=1000.0):
    k = r.random()
    if k < 0.1:
        return float(r.randint(-4, 4))
    if k < 0.3:
        return f32(r.uniform(-2.0, 2.0))
    return f32(r.uniform(lo, hi))


def rnd_matrix(r):
    return [rnd_value(r) for _ in range(16)]


def rnd_point(r):
    return [rnd_value(r) for _ in range(3)]


def rnd_angle(r):
    if r.random() < 0.6:
        t = r.uniform(-math.pi, math.pi)
        return [f32(math.cos(t)), f32(math.sin(t))]
    return [f32(r.uniform(-2.0, 2.0)), f32(r.uniform(-2.0, 2.0))]


def rnd_m_ret(r):
    m = rnd_matrix(r)
    if r.random() < 0.5:
        m[9] = f32(r.uniform(-1.2, 1.2))
    return {"m": m}


# ---- FloatSortByArray: counts around the halving, every depth parity, the four key offsets, ties, NaN, signed zeros and infinities
SORT_KEYS = [0.0, -0.0, 1.0, -1.0, 2.0, 3.0, INF, -INF, NAN, 0.5, -2.5, 7.0]


def sort_case(r, count, depth, offset, pool=None, shape="random"):
    n = max(count, 1)
    pool = pool or SORT_KEYS
    if shape == "equal":
        keys = [bits(r.choice(pool))] * n
    elif shape == "up":
        keys = [bits(float(i)) for i in range(n)]
    elif shape == "down":
        keys = [bits(float(n - i)) for i in range(n)]
    else:
        keys = [bits(r.choice(pool)) if r.random() < 0.8 else bits(f32(r.uniform(-50, 50))) for _ in range(n)]
    order = list(range(n))
    if shape == "random":
        r.shuffle(order)
    return {"count": count, "depth": depth, "offset": offset, "keys": keys, "order": order,
            "junk": [[r.getrandbits(32) for _ in range(4)] for _ in range(n)]}


def edges_sort(which):
    r = random.Random(170)
    for count in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33, 40, -1, -7):
        for depth in range(4):
            for offset in (0, 4, 8, 12):
                for shape in ("equal", "up", "down", "random"):
                    yield sort_case(r, count, depth, offset, shape=shape)
    for count in (2, 3, 5, 8, 13):
        for depth in (0, 1):
            for pool in ([NAN, 1.0], [NAN], [0.0, -0.0], [INF, -INF, NAN], [1.0, 2.0]):
                yield sort_case(r, count, depth, 4, pool)


def rnd_sort(r):
    return sort_case(r, r.choice([r.randint(0, 40), r.randint(0, 12)]), r.randint(0, 3), r.choice([0, 4, 8, 12]),
                     r.choice([None, None, [0.0, 1.0, 2.0], [NAN, 0.0, 1.0], [0.0, -0.0]]))


EDGES = {"m": edges_m, "m_ret": edges_m_ret, "p_ret": edges_p_ret, "m_cs": edges_m_cs, "m_xyz": edges_m_xyz,
         "m_p": edges_m_p, "m_m": edges_m_m, "p_q_ret": edges_p_q_ret, "sort": edges_sort}
RANDOM = {
    "m": lambda r: {"m": rnd_matrix(r)},
    "m_ret": rnd_m_ret,
    "p_ret": lambda r: {"p": rnd_point(r)},
    "m_cs": lambda r: {"m": rnd_matrix(r), "args": rnd_angle(r)},
    "m_xyz": lambda r: {"m": rnd_matrix(r), "args": [f32(r.uniform(-4.0, 4.0)) for _ in range(3)]},
    "m_p": lambda r: {"m": rnd_matrix(r), "p": rnd_point(r)},
    "m_m": lambda r: {"m": rnd_matrix(r), "m2": rnd_matrix(r), "same_matrix": r.random() < 0.2},
    "p_q_ret": lambda r: {"p": rnd_point(r), "q": rnd_point(r)},
    "sort": rnd_sort,
}

TESTS = [make_test(i) for i in range(len(FUNCS))]
