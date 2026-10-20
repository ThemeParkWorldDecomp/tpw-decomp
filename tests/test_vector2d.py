"""Differential tests for vector2d.cpp (pilot). See difftest.py for the protocol."""
import ctypes
import math
import struct

INT_EDGES = [0, 1, -1, 2, -2, 0x10000, -0x10000, 0x8000, -0x8000, 0x7FFF, 0x12345678, -0x12345678,
             0x7FFFFFFF, -0x80000000, 0x3FFFFFFF, -0x40000000, 0x00010001, 0x00FFFFFF, -0x00FFFFFF, 0x40000, 0xC0000]


def s32(v):
    return ((v + 0x80000000) & 0xFFFFFFFF) - 0x80000000


def u32(v):
    return v & 0xFFFFFFFF


def fixmul(a, b):
    p = a * b
    p += 0x8000 if p >= 0 else -0x8000
    q = p >> 16                     # Python >> is arithmetic (floor) on negatives: matches C++ >> on int64 (arithmetic)
    return max(-0x80000000, min(0x7FFFFFFF, q))


def fixdiv(a, b):
    if b == 0:
        return 0x7FFFFFFF if a >= 0 else -0x80000000
    n = a << 16
    q = int(n / b) if abs(n) < 2**52 else (abs(n) // abs(b)) * (1 if (n < 0) == (b < 0) else -1)
    q = (abs(n) // abs(b)) * (1 if (n < 0) == (b < 0) else -1)   # truncation toward zero, as C++
    r = n - q * b                                                 # remainder with the sign of n
    if abs(r) * 2 >= abs(b):
        q += -1 if (n < 0) != (b < 0) else 1
    return max(-0x80000000, min(0x7FFFFFFF, q))


def install_stubs(emu):
    emu.stub("FixMul", lambda e: e.set_r(3, fixmul(s32(e.r(3)), s32(e.r(4)))))
    emu.stub("FixDiv", lambda e: e.set_r(3, fixdiv(s32(e.r(3)), s32(e.r(4)))))
    emu.stub("sqrt", lambda e: e.set_f1(math.sqrt(e.f(1))))
    emu.deleted = 0
    def dl(e):
        e.deleted = 1
    emu.stub("__dl__FPv", dl)


class Test:
    """Generic test: argspec items are ("buf", name, fmt, values) or ("int", value)
    (values are python ints; fmt without endianness).  ret: "u32", "i32", "none".
    compare_bufs: buffer names to compare after the call."""
    returns_float = False

    def __init__(self, name, ppc_name, host_name, argspec_fn, ret="u32", compare_bufs=(), edge=None, rnd=None,
                 host_argtypes=None, side_effect=None):
        self.name, self.ppc_name, self.host_name = name, ppc_name, host_name
        self.argspec_fn, self.ret, self.compare_bufs = argspec_fn, ret, compare_bufs
        self._edge, self._rnd = edge, rnd
        self.host_argtypes = host_argtypes
        self.side_effect = side_effect

    def edge_cases(self):
        return self._edge()

    def random_case(self, r):
        return self._rnd(r)

    def build(self, side, case):
        args, bufs = [], {}
        for item in self.argspec_fn(case):
            if item[0] == "buf":
                _, name, fmt, vals = item
                bufs[name] = side.pack(fmt, *vals)
                args.append(name)
            else:
                args.append(item[1])
        return args, bufs

    def call_host(self, lib, args, objs):
        fn = getattr(lib, self.host_name)
        fn.restype = {"u32": ctypes.c_uint32, "i32": ctypes.c_int32, "none": None}[self.ret]
        cargs = []
        for i, a in enumerate(args):
            at = self.host_argtypes[i] if self.host_argtypes else None
            if isinstance(a, int):
                if at is ctypes.c_uint32:
                    cargs.append(ctypes.c_uint32(u32(a)))
                elif at is ctypes.c_int16:
                    cargs.append(ctypes.c_int16(((a & 0xFFFF) ^ 0x8000) - 0x8000))
                else:
                    cargs.append(ctypes.c_int32(s32(a)))
            else:
                cargs.append(ctypes.cast(a, ctypes.c_void_p) if at is ctypes.c_void_p else a)
        return fn(*cargs)

    def compare(self, p_res, h_res, p_after, h_after, ppc, host, case):
        mism = []
        if self.ret != "none":
            pv = u32(p_res) if self.ret == "u32" else s32(p_res)
            hv = u32(h_res) if self.ret == "u32" else s32(h_res)
            if pv != hv:
                mism.append(f"return ppc={pv:#x} host={hv:#x}")
        for name in self.compare_bufs:
            fmt = self.fmt_of(case, name)
            pv = ppc.unpack(fmt, p_after[name])
            hv = host.unpack(fmt, h_after[name])
            if pv != hv:
                mism.append(f"{name}: ppc={[hex(u32(v)) for v in pv]} host={[hex(u32(v)) for v in hv]}")
        return mism

    def fmt_of(self, case, name):
        for item in self.argspec_fn(case):
            if item[0] == "buf" and item[1] == name:
                return item[2]
        raise KeyError(name)


def rfixed(r):
    k = r.random()
    if k < 0.4:
        return r.randint(-0x80000000, 0x7FFFFFFF)
    if k < 0.8:
        return r.randint(-0x400000, 0x400000)      # +-64.0
    return r.choice(INT_EDGES)


def rvec(r):
    return (rfixed(r), rfixed(r))


V2 = "ii"

TESTS = [
    Test("xaxis_circle_intersect", "xaxis_circle_intersect__FR8Vector2dl", "tpw_xaxis_circle_intersect",
         lambda c: [("buf", "p", V2, c["p"]), ("int", c["r"])], ret="u32", compare_bufs=("p",),
         edge=lambda: [dict(p=(x, y), r=rr) for x in INT_EDGES[:10] for y in (0, 0x10000, -0x30000) for rr in (0, 0x10000, 0x7FFFFFFF)],
         rnd=lambda r: dict(p=rvec(r), r=rfixed(r))),
    Test("line_circle_intersect", "line_circle_intersect__FR8Vector2dR8Vector2dR8Vector2dl", "tpw_line_circle_intersect",
         lambda c: [("buf", "p1", V2, c["p1"]), ("buf", "d", V2, c["d"]), ("buf", "c", V2, c["c"]), ("int", c["r"])],
         ret="i32", compare_bufs=("p1", "d", "c"),
         edge=lambda: [dict(p1=(x, 0), d=(0x10000, 0), c=(0, 0), r=rr) for x in INT_EDGES for rr in (0, 0x10000, 0x40000)]
              + [dict(p1=(0x20000, 0x20000), d=(-0x10000, -0x10000), c=(0, 0), r=0x10000),
                 dict(p1=(0, 0), d=(0, 0), c=(0, 0), r=0), dict(p1=(0x7FFFFFFF, 0x7FFFFFFF), d=(0x7FFFFFFF, 1), c=(-0x80000000, 5), r=0x7FFFFFFF)],
         rnd=lambda r: dict(p1=rvec(r), d=rvec(r), c=rvec(r), r=rfixed(r))),
    Test("Vector2d::operator-(v)", "__mi__8Vector2dCFRC8Vector2d", "tpw_v2_sub",
         lambda c: [("buf", "out", V2, (0xDEAD, 0xBEEF)), ("buf", "a", V2, c["a"]), ("buf", "b", V2, c["b"])],
         ret="none", compare_bufs=("out", "a", "b"),
         edge=lambda: [dict(a=(x, y), b=(y, x)) for x in INT_EDGES for y in (0, 1, -0x80000000, 0x7FFFFFFF)],
         rnd=lambda r: dict(a=rvec(r), b=rvec(r))),
    Test("Vector2d::operator+", "__pl__8Vector2dCFRC8Vector2d", "tpw_v2_add",
         lambda c: [("buf", "out", V2, (0, 0)), ("buf", "a", V2, c["a"]), ("buf", "b", V2, c["b"])],
         ret="none", compare_bufs=("out", "a", "b"),
         edge=lambda: [dict(a=(x, y), b=(y, x)) for x in INT_EDGES for y in (0, 1, -0x80000000, 0x7FFFFFFF)],
         rnd=lambda r: dict(a=rvec(r), b=rvec(r))),
    Test("Vector2d::operator-()", "__mi__8Vector2dCFv", "tpw_v2_neg",
         lambda c: [("buf", "out", V2, (0, 0)), ("buf", "a", V2, c["a"])],
         ret="none", compare_bufs=("out", "a"),
         edge=lambda: [dict(a=(x, y)) for x in INT_EDGES for y in (0, -0x80000000)],
         rnd=lambda r: dict(a=rvec(r))),
    Test("Vector2d::operator*(s)", "__ml__8Vector2dCFl", "tpw_v2_mul",
         lambda c: [("buf", "out", V2, (0, 0)), ("buf", "a", V2, c["a"]), ("int", c["s"])],
         ret="none", compare_bufs=("out", "a"),
         edge=lambda: [dict(a=(x, y), s=s) for x in INT_EDGES for y in (0x10000, -0x18000) for s in (0, 0x10000, -0x8000, 0x7FFFFFFF, -0x80000000)],
         rnd=lambda r: dict(a=rvec(r), s=rfixed(r))),
    Test("Vector2d::operator/(s)", "__dv__8Vector2dCFl", "tpw_v2_div",
         lambda c: [("buf", "out", V2, (0, 0)), ("buf", "a", V2, c["a"]), ("int", c["s"])],
         ret="none", compare_bufs=("out", "a"),
         edge=lambda: [dict(a=(x, y), s=s) for x in INT_EDGES for y in (0x10000, -0x18000) for s in (0, 1, -1, 0x10000, -0x8000, 0x7FFFFFFF, -0x80000000, 3)],
         rnd=lambda r: dict(a=rvec(r), s=rfixed(r))),
    Test("Vector2d::Vector2d()", "__ct__8Vector2dFv", "tpw_v2_ctor0",
         lambda c: [("buf", "out", V2, (c["g"], c["g"]))], ret="none", compare_bufs=("out",),
         edge=lambda: [dict(g=g) for g in (0, -1, 0x12345678)], rnd=lambda r: dict(g=rfixed(r))),
    Test("Vector2d::Vector2d(x,y)", "__ct__8Vector2dFll", "tpw_v2_ctor2",
         lambda c: [("buf", "out", V2, (0, 0)), ("int", c["x"]), ("int", c["y"])], ret="none", compare_bufs=("out",),
         edge=lambda: [dict(x=x, y=-x) for x in INT_EDGES], rnd=lambda r: dict(x=rfixed(r), y=rfixed(r))),
    Test("isqrt", "isqrt__FUl", "tpw_isqrt",
         lambda c: [("int", c["x"])], ret="u32", compare_bufs=(),
         edge=lambda: [dict(x=x) for x in (0, 1, 2, 3, 4, 15, 16, 17, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFE0001, 0xFFFF0000)],
         rnd=lambda r: dict(x=r.randint(0, 0xFFFFFFFF)),
         host_argtypes=[ctypes.c_uint32]),
    Test("Vector2d::~Vector2d(flag)", "__dt__8Vector2dFv", "tpw_v2_dtor",
         lambda c: [("buf", "obj", V2, (1, 2)), ("int", c["flag"])], ret="i32", compare_bufs=("obj",),
         edge=lambda: [dict(flag=f) for f in (0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x1FFFF)],
         rnd=lambda r: dict(flag=r.randint(-0x80000000, 0x7FFFFFFF)),
         host_argtypes=[ctypes.c_void_p, ctypes.c_int16]),
]


class DtorTest(Test):
    """The PPC destructor returns `this` and may call operator delete; compare the
    'deleted' side effect rather than the return value."""


# patch: the PPC side of the destructor reports deletion through the stub; wrap compare
_d = TESTS[-1]
_orig_compare = _d.compare


def _dtor_compare(p_res, h_res, p_after, h_after, ppc, host, case, _emu_holder=[]):
    emu = _emu_holder[0]
    mism = []
    if emu.deleted != s32(h_res):
        mism.append(f"deleted: ppc={emu.deleted} host={s32(h_res)}")
    emu.deleted = 0
    return mism


_d.compare = _dtor_compare
_d.ppc_flag_fix = True


def setup(runner):
    install_stubs(runner.emu)
    _dtor_compare.__defaults__[0].append(runner.emu)
    # the destructor takes the flag as a short in r4: pass it as an int (low 16 bits used)
