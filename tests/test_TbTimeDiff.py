"""Differential tests for bullfrog TbTimeDiff.cpp. See difftest.py and tpwtest.py.

A TbTimeDiff is 8 bytes on both sides (big-endian high word first on the PowerPC, a
native int64_t on the host); cases give values. Functions returning a TbTimeDiff, a
TbTimeStamp or a TbError take the result buffer in r3 on the PowerPC.

Not tested, as C++ leaves them undefined: dividing by 0, and INT64_MIN / -1. The float
forms go through CodeWarrior's runtime helpers on the PowerPC (int64 -> float rounds to
double first; float -> int64 saturates), which ppc_math.h reproduces; edge values sit on
both of those.
"""
import ctypes
import math
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

VP, I32, I64 = ctypes.c_void_p, ctypes.c_int32, ctypes.c_int64
P64 = ctypes.POINTER(I64)
MIN64, MAX64 = -(1 << 63), (1 << 63) - 1


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def s64(v):
    return ((v + (1 << 63)) & ((1 << 64) - 1)) - (1 << 63)


EDGES64 = [0, 1, -1, 2, -2, 9999, 10000, -10000, 0x7FFFFFFF, 0x80000000, -0x80000000, 0xFFFFFFFF, 0x100000000,
           -0x100000000, MAX64, MIN64, MAX64 - 1, MIN64 + 1, 1 << 53, (1 << 53) + 1, -(1 << 53) - 1,
           (1 << 60) + (1 << 36) + 1, -((1 << 60) + (1 << 36) + 1), (1 << 62) + (1 << 38) + 1, (1 << 24) + 1,
           864000000000, -864000000000, 36000000000, 600000000, 10000000, 863999999999, 922337203685477 * 10000]


def r64(r):
    k = r.random()
    if k < 0.2:
        return r.choice(EDGES64)
    if k < 0.45:
        return s64(r.getrandbits(64))
    if k < 0.7:
        return r.randint(-(1 << 40), 1 << 40)
    if k < 0.85:
        return r.randint(-100000, 100000)
    # values with long mantissas near the float rounding points
    e = r.randint(24, 62)
    return r.choice([1, -1]) * ((1 << e) + r.getrandbits(e - 23) * r.choice([1, 0]) + r.getrandbits(8))


def ppc_val(e, v):
    return e.alloc(8, struct.pack(">q", v))


def ppc_get(e, a):
    return struct.unpack(">q", e.read(a, 8))[0]

# ----------------------------------------------------------------- binary value operators


def value_op(mangled, sym):
    def ppc(e, c):
        a, b, out = ppc_val(e, c["a"]), ppc_val(e, c["b"]), e.alloc(8, b"\xA5" * 8)
        T.call_ppc(e, e.func(mangled), [T.I(out), T.I(a), T.I(b)])
        return dict(out=ppc_get(e, out), a=ppc_get(e, a), b=ppc_get(e, b))

    def host(lib, c):
        a, b, out = I64(c["a"]), I64(c["b"]), I64(0x5A5A)
        F(lib, sym, None, P64, P64, P64)(ctypes.byref(a), ctypes.byref(b), ctypes.byref(out))
        return dict(out=out.value, a=a.value, b=b.value)
    return ppc, host


def assign_op(mangled, sym, arg):
    """In-place operators: this in r3 (returned), the operand by reference, int or float."""
    def ppc(e, c):
        a = ppc_val(e, c["a"])
        if arg == "ref":
            extra = [T.I(ppc_val(e, c["b"]))]
        elif arg == "int":
            extra = [T.I(c["n"])]
        else:
            extra = [T.F(c["f"])]
        r3, _, _ = T.call_ppc(e, e.func(mangled), [T.I(a)] + extra)
        return dict(a=ppc_get(e, a), this_ret=r3 == a)

    def host(lib, c):
        a = I64(c["a"])
        if arg == "ref":
            b = I64(c["b"])
            ok = F(lib, sym, I32, P64, P64)(ctypes.byref(a), ctypes.byref(b))
        elif arg == "int":
            ok = F(lib, sym, I32, P64, I32)(ctypes.byref(a), c["n"])
        else:
            ok = F(lib, sym, I32, P64, ctypes.c_float)(ctypes.byref(a), c["f"])
        return dict(a=a.value, this_ret=bool(ok))
    return ppc, host


def compare_op(mangled, sym):
    def ppc(e, c):
        a, b = ppc_val(e, c["a"]), ppc_val(e, c["b"])
        r3, _, _ = T.call_ppc(e, e.func(mangled), [T.I(a), T.I(b)])
        return dict(ret=r3 & 0xFF)

    def host(lib, c):
        a, b = I64(c["a"]), I64(c["b"])
        return dict(ret=F(lib, sym, I32, P64, P64)(ctypes.byref(a), ctypes.byref(b)))
    return ppc, host

# ----------------------------------------------------------------- Set and Get


def set_ppc(e, c):
    res, a = e.alloc(4, b"\xA5" * 4), ppc_val(e, c["a"])
    T.call_ppc(e, e.func("Set__10TbTimeDiffFiiiii"), [T.I(res), T.I(a)] + [T.I(x) for x in c["parts"]])
    return dict(ret=T.s32(e.u32(res)), a=ppc_get(e, a))


def set_host(lib, c):
    a = I64(c["a"])
    r = F(lib, "tpw_TbTimeDiff_Set", I32, P64, I32, I32, I32, I32, I32)(ctypes.byref(a), *c["parts"])
    return dict(ret=r, a=a.value)


def get_ppc(e, c):
    res, a = e.alloc(4, b"\xA5" * 4), ppc_val(e, c["a"])
    outs = [e.alloc(4, struct.pack(">i", g)) if (c["mask"] >> i) & 1 else 0 for i, g in enumerate(c["garbage"])]
    T.call_ppc(e, e.func("Get__10TbTimeDiffCFPiPiPiPiPi"), [T.I(res), T.I(a)] + [T.I(o) for o in outs])
    return dict(ret=T.s32(e.u32(res)), parts=[T.s32(e.u32(o)) if o else None for o in outs], a=ppc_get(e, a))


def get_host(lib, c):
    a = I64(c["a"])
    outs = [I32(g) if (c["mask"] >> i) & 1 else None for i, g in enumerate(c["garbage"])]
    r = F(lib, "tpw_TbTimeDiff_Get", I32, P64, *[VP] * 5)(ctypes.byref(a), *[ctypes.byref(o) if o is not None else None
                                                                            for o in outs])
    return dict(ret=r, parts=[o.value if o is not None else None for o in outs], a=a.value)

# ----------------------------------------------------------------- cases


def pair(r):
    a = r64(r)
    b = r64(r) if r.random() < 0.8 else r.choice([a, a + 1, a - 1, -a, s64(a ^ (1 << 32))])
    return dict(a=a, b=s64(b))


def pair_edges():
    vals = [0, 1, -1, 0xFFFFFFFF, 0x100000000, -0x100000000, MAX64, MIN64, 0x80000000, 0x7FFFFFFF]
    return [dict(a=a, b=b) for a in vals for b in vals]


INT_EDGES = [0, 1, -1, 2, 10, -10, 1000, 0x7FFFFFFF, -0x80000000, 0x10000, -0x10000, 3, 7]


def mul_int_case(r):
    return dict(a=r64(r), n=r.choice(INT_EDGES) if r.random() < 0.4 else T.s32(r.getrandbits(32)))


def div_int_case(r):
    while True:
        c = mul_int_case(r)
        if c["n"] != 0 and not (c["n"] == -1 and c["a"] == MIN64):
            return c


FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 1.5, 0.1, 3.0, 1e-3, 1e3, 1e10, 1e30, -1e30, 3.4028234663852886e38,
          1.401298464324817e-45, 1.1754943508222875e-38, math.inf, -math.inf, math.nan, 0.99999994, 1.0000001,
          9.223372e18, -9.223372e18, 9.2233715e18]


def float_case(r, divide):
    f = r.choice(FLOATS) if r.random() < 0.4 else T.rfloat(r, nan=True)
    return dict(a=r64(r), f=f)


def float_edges():
    return [dict(a=a, f=f) for a in (0, 1, -1, 10000, MAX64, MIN64, (1 << 60) + (1 << 36) + 1, 1 << 53,
                                     (1 << 53) + 1, -(1 << 62))
            for f in FLOATS]


def set_case(r):
    def part():
        k = r.random()
        if k < 0.3:
            return r.randint(-100, 100)
        if k < 0.5:
            return r.choice([0, 1, -1, 23, 24, 59, 60, 999, 1000, 0x7FFFFFFF, -0x80000000, 24855, 24856])
        return T.s32(r.getrandbits(32))
    return dict(a=r64(r), parts=[part() for _ in range(5)])


def set_edges():
    return [dict(a=7, parts=p) for p in ([0] * 5, [1] * 5, [-1] * 5, [24855, 3, 14, 7, 647], [24855, 3, 14, 7, 648],
                                         [-24855, -3, -14, -7, -648], [0x7FFFFFFF] * 5, [-0x80000000] * 5,
                                         [0, 0, 0, 0, 0x7FFFFFFF], [0, 0, 0, 0, -0x80000000])]


def get_case(r):
    return dict(a=r64(r), mask=r.getrandbits(5) if r.random() < 0.3 else 31, garbage=[T.s32(r.getrandbits(32)) for _ in range(5)])


def get_edges():
    return [dict(a=a, mask=m, garbage=[0x11111111] * 5) for a in EDGES64 for m in (31, 0, 1, 2, 4, 8, 16)]


def make(title, sides, edge, rnd):
    ppc, host = sides
    return FnTest(title, ppc, host, edge=edge, rnd=rnd)


TESTS = [
    make("TbTimeDiff::operator+ (TbTimeDiff)", value_op("__pl__10TbTimeDiffCFRC10TbTimeDiff", "tpw_TbTimeDiff_add"),
         pair_edges, pair),
    make("TbTimeDiff::operator+ (TbTimeStamp)", value_op("__pl__10TbTimeDiffCFRC11TbTimeStamp",
                                                         "tpw_TbTimeDiff_add_stamp"), pair_edges, pair),
    make("TbTimeDiff::operator-", value_op("__mi__10TbTimeDiffCFRC10TbTimeDiff", "tpw_TbTimeDiff_sub"),
         pair_edges, pair),
    make("TbTimeDiff::operator+=", assign_op("__apl__10TbTimeDiffFRC10TbTimeDiff", "tpw_TbTimeDiff_add_assign",
                                             "ref"), pair_edges, pair),
    make("TbTimeDiff::operator-=", assign_op("__ami__10TbTimeDiffFRC10TbTimeDiff", "tpw_TbTimeDiff_sub_assign",
                                             "ref"), pair_edges, pair),
    make("TbTimeDiff::operator*= (int)", assign_op("__amu__10TbTimeDiffFi", "tpw_TbTimeDiff_mul_int", "int"),
         lambda: [dict(a=a, n=n) for a in (0, 1, -1, MAX64, MIN64, 0x100000000) for n in INT_EDGES], mul_int_case),
    make("TbTimeDiff::operator*= (float)", assign_op("__amu__10TbTimeDiffFf", "tpw_TbTimeDiff_mul_float", "float"),
         float_edges, lambda r: float_case(r, False)),
    make("TbTimeDiff::operator/= (int)", assign_op("__adv__10TbTimeDiffFi", "tpw_TbTimeDiff_div_int", "int"),
         lambda: [dict(a=a, n=n) for a in (0, 1, -1, 7, -7, MAX64, MIN64, 0x100000000) for n in INT_EDGES
                  if n != 0 and not (a == MIN64 and n == -1)], div_int_case),
    make("TbTimeDiff::operator/= (float)", assign_op("__adv__10TbTimeDiffFf", "tpw_TbTimeDiff_div_float", "float"),
         float_edges, lambda r: float_case(r, True)),
] + [make("TbTimeDiff::operator" + op, compare_op("__%s__10TbTimeDiffCFRC10TbTimeDiff" % code, "tpw_TbTimeDiff_" + code),
          pair_edges, pair)
     for op, code in (("<", "lt"), ("<=", "le"), (">", "gt"), (">=", "ge"), ("!=", "ne"), ("==", "eq"))] + [
    FnTest("TbTimeDiff::Set", set_ppc, set_host, edge=set_edges, rnd=set_case),
    FnTest("TbTimeDiff::Get", get_ppc, get_host, edge=get_edges, rnd=get_case),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
