"""Unit v5112, the point tests over the collision list, against libtpw_unit_PointOutAllInverse.dylib (tests/unit_PointOutAllInverse.cpp),
main app ("SimTheme Park"), src/BMP_InsideArea_PointOutAllInverse.cpp:
  PointOutAllInverse 0x1002bb64, PointInAnyColObject 0x1002ba94, NavPointInArea 0x1002b408

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
PointInColObject (0x1002bc34) is a fake on both sides that records (x, y, index of the object in the list) and answers from the case's queue.

The world of a case: the list record at 0x102b56c0 (first at +0x18, live count at +0x1c) and an array of 0x20-byte objects (flags, x, y);
for NavPointInArea an object and an output point filled with garbage. What is compared: the answer (an object index for PointInAnyColObject,
whether the answer is the output pointer for NavPointInArea), the output point, the call log.

No fused operations. Not tested: a live count above the number of live objects (the original reads past the array).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PointOutAllInverse"
MASK = 0xFFFFFFFF

FN_OUT, FN_IN, FN_NAV = 0x1002bb64, 0x1002ba94, 0x1002b408
SIZES = {FN_OUT: 164, FN_IN: 164, FN_NAV: 120}
OWN = set(SIZES)
CALLEE = 0x1002bc34

ADDR = dict(col=0x102b56c0)
HOST_LAYOUT = [0x18, 0x1c, 0x20]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_callee(e):
    """int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* o)"""
    L = e.cl
    first = e.u32(ADDR["col"] + 0x18)
    L.rec("PointInColObject", (s32(e.r(3)), s32(e.r(4)), (e.r(5) - first) // 0x20))
    e.set_r(3, L.ret("PointInColObject") & MASK)


STUBS.internal(CALLEE, stub_callee)


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

    def ptr(self, a):
        return a

    def log(self):
        return self.L.log

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pioai_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pioai_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a):
        return self.lib.tpw_pioai_off(a)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        l = self.lib
        if fn == FN_OUT:
            return l.tpw_pioai_out_all(s32(args[0]), s32(args[1])) & MASK
        if fn == FN_IN:
            return l.tpw_pioai_in_any(s32(args[0]), s32(args[1]))
        return l.tpw_pioai_nav(args[0], args[1])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_pioai_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pioai_off.restype, lib.tpw_pioai_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pioai_out_all.restype, lib.tpw_pioai_out_all.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    lib.tpw_pioai_in_any.restype, lib.tpw_pioai_in_any.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    lib.tpw_pioai_nav.restype, lib.tpw_pioai_nav.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    if case["op"] == "nav":
        s.obj = s.alloc(0x20)
        s.out = s.alloc(8)
        s.words(s.obj, [case["flags"], case["x"], case["y"]])
        s.words(s.out, [0xDEADBEEF, 0xCAFEF00D])
        return
    objs = case["objs"]
    s.base = s.alloc(0x20 * (len(objs) + 1))
    for i, (f, x, y) in enumerate(objs):
        s.words(s.base + 0x20 * i, [f, x, y, 5, 6, 7, 8, 9])
    s.words(s.g("col") + 0x18, [s.ptr(s.base) if case["has_first"] else 0, case["count"]])


def execute(s, case):
    if case["op"] == "nav":
        return s.call(FN_NAV, [s.obj, s.out])
    fn = FN_OUT if case["op"] == "out" else FN_IN
    return s.call(fn, case["args"])


def observe(s, case, answer):
    if case["op"] == "nav":
        return {"same": answer == s.out, "out": s.rd_words(s.out, 2), "obj": s.rd_words(s.obj, 3), "log": s.log()}
    if case["op"] == "in":
        answer = -1 if answer == 0 else (answer - s.base) // 0x20
    return {"answer": answer, "log": s.log(), "col": s.rd_words(s.g("col") + 0x1c, 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mkobjs(r, n, live_p=0.7):
    objs = []
    for _ in range(n):
        f = r.choice([0, 1, 1, 1, 0x40, 0x41, 0x41, 0x21, 0x61, 0x20, 0x60, 0x3F, 0xFFFFFFFF, 0xFFFFFFBF])
        if r.random() > live_p:
            f &= ~1
        objs.append((f, r.randrange(-5000, 5000) & MASK, r.randrange(-5000, 5000) & MASK))
    return objs


def list_case(r, op, objs=None, count=None, has_first=True, queue=None):
    if objs is None:
        objs = mkobjs(r, r.randrange(0, 7))
    live = sum(1 for o in objs if o[0] & 1)
    if count is None:
        count = r.randrange(0, live + 1)
    if queue is None:
        queue = [r.choice([0, 0, 1, 5]) for _ in range(r.randrange(1, 8))]
    return {"op": op, "objs": objs, "count": count, "has_first": has_first, "queues": {"PointInColObject": queue},
            "args": [r.choice([0, 1, -1, r.randrange(-3000, 3000)]) & MASK, r.choice([0, -1, r.randrange(-3000, 3000)]) & MASK]}


def list_edges(op):
    def gen():
        r = random.Random(7 if op == "out" else 8)
        yield list_case(r, op, objs=[], count=0)
        yield list_case(r, op, objs=[(0x41, 1, 2)], count=1, has_first=False)
        for f in (0, 1, 0x40, 0x41, 0x20, 0x21, 0x60, 0x61):
            for q in (0, 1):
                yield list_case(r, op, objs=[(f, 10, 20)], count=(f & 1), queue=[q])
                yield list_case(r, op, objs=[(f, 10, 20)], count=0, queue=[q])
        for q in ([0, 0, 1], [0, 1], [1], [0, 0, 0]):
            yield list_case(r, op, objs=[(0x41, 1, 2), (0, 3, 4), (0x01, 5, 6), (0x41, 7, 8), (0x40, 9, 9)], count=3, queue=q)
            yield list_case(r, op, objs=[(0x41, 1, 2), (0, 3, 4), (0x01, 5, 6), (0x41, 7, 8), (0x40, 9, 9)], count=2, queue=q)
    return gen


def list_random(op):
    return lambda r: list_case(r, op)


def nav_case(r, flags=None):
    return {"op": "nav", "queues": {}, "flags": r.randrange(0, 1 << 32) if flags is None else flags,
            "x": r.choice([0, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(0, 1 << 32), r.randrange(0, 5000)]),
            "y": r.choice([0, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(0, 1 << 32), r.randrange(0, 5000)])}


def nav_edges():
    r = random.Random(9)
    for f in range(32):
        yield nav_case(r, f)
    for f in (0xFFFFFFFF, 0xFFFFFFE1, 0x1E):
        yield nav_case(r, f)


TESTS = [
    FnTest("PointOutAllInverse", ppc, host, edge=list_edges("out"), rnd=list_random("out")),
    FnTest("PointInAnyColObject", ppc, host, edge=list_edges("in"), rnd=list_random("in")),
    FnTest("NavPointInArea", ppc, host, edge=nav_edges, rnd=nav_case),
]
