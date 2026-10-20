"""Unit v5228, the ride height by polygon search, against libtpw_unit_GetRideHeightAtPoint.dylib (tests/unit_GetRideHeightAtPoint.cpp), main app
("SimTheme Park"), src/CalculateMaxVertexHeights_GetRideHeightAtPoint.cpp:
  GetRideHeightAtPoint 0x1005d198, GetHeightFromPolySearch 0x1005d000, DoNodePolySearch 0x1005cce4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit, __fpclassifyf and
DetermineHFieldHeightQuick, are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the ride context (rotation, world block with height offset, box, nodes and per-node flags, embedded data with params, a
node and a point array), the flag global, the vertex table. What is compared: the answer, the height offset word, the out parameter, the
event log.

Fused operations: fmadds / fnmadds in DoNodePolySearch, run under HardwareRounding. Not tested: a params array pointer of 0 inside
GetHeightFromPolySearch (the original reads through it).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetRideHeightAtPoint"
MASK = 0xFFFFFFFF

FN_RIDE, FN_HEIGHT, FN_NODE = 0x1005D198, 0x1005D000, 0x1005CCE4
SIZES = {FN_RIDE: 316, FN_HEIGHT: 356, FN_NODE: 708}
OWN = set(SIZES)

CALLEE_HQ = 0x1005EDA8

ADDR = dict(ctx=0x102C1E10, flags=0x102C1E14, table=0x102C1E1C)
HOST_LAYOUT = [4, 4, 8]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_hq(e):
    L = e.cl
    L.rec("DetermineHFieldHeightQuick", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2)))))
    e.set_f1(bitsf(L.ret("DetermineHFieldHeightQuick")))


def stub_fpc(e):
    L = e.cl
    L.rec("fpclassifyf", (canon_f(fb(e.f(1))),))
    e.set_r(3, L.ret("fpclassifyf") & MASK)


STUBS.internal(CALLEE_HQ, stub_hq)
STUBS.imp("__fpclassifyf", stub_fpc)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, v):
        """A Ptr32 field: the Mac address, or the host's offset from the arena base."""
        return self.off(v)

    def half(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", w & 0xFFFF) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, v):
        return v

    def native(self, a, v):
        self.words(a, [v])

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(max(n, 8))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_ride(self, x, z, out):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_RIDE, [T.F(bitsf(x)), T.F(bitsf(z)), out & MASK])
        return 0

    def call_height(self, x, z, a, b):
        with ROUNDING(self.e):
            _, _, f1 = T.call_ppc(self.e, FN_HEIGHT, [T.F(bitsf(x)), T.F(bitsf(z)), a, b])
        return canon_f(fb(f1))

    def call_node(self, node, params, x, z, pt, flag):
        with ROUNDING(self.e):
            _, _, f1 = T.call_ppc(self.e, FN_NODE, [node & MASK, params & MASK, T.F(bitsf(x)), T.F(bitsf(z)), pt & MASK, flag])
        return canon_f(fb(f1))


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def off(self, v):
        return v - self.lib.tpw_rh_base() if v else 0

    def native(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def g(self, name):
        return self.lib.tpw_rh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rh_alloc(max(n, 8))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_ride(self, x, z, out):
        self.lib.tpw_rh_ride(bitsf(x), bitsf(z), out)
        return 0

    def call_height(self, x, z, a, b):
        return canon_f(fb(self.lib.tpw_rh_height(bitsf(x), bitsf(z), a, b)))

    def call_node(self, node, params, x, z, pt, flag):
        return canon_f(fb(self.lib.tpw_rh_node(node, params, bitsf(x), bitsf(z), pt, flag)))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rh_addr.restype, lib.tpw_rh_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rh_alloc.restype, lib.tpw_rh_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rh_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rh_base.restype, lib.tpw_rh_base.argtypes = C.c_uint64, []
    lib.tpw_rh_node.restype, lib.tpw_rh_node.argtypes = C.c_float, [C.c_uint64, C.c_uint64, C.c_float, C.c_float, C.c_uint64, C.c_int32]
    lib.tpw_rh_height.restype, lib.tpw_rh_height.argtypes = C.c_float, [C.c_float, C.c_float, C.c_int32, C.c_int32]
    lib.tpw_rh_ride.restype, lib.tpw_rh_ride.argtypes = None, [C.c_float, C.c_float, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rh_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_node(s, a, nd):
    """A tag_sNode at a: +0x5a set count, +0x64 points, +0x6c face sets, +0x70 faces, +0x94 index table."""
    idx = s.alloc(2 * len(nd["idx"]))
    s.half(idx, nd["idx"])
    pts = s.alloc(len(nd["pts"]) * 4)
    s.words(pts, nd["pts"])
    nfaces = sum(len(f) for f in nd["sets"])
    faces = s.alloc(8 * nfaces)
    sets = s.alloc(16 * len(nd["sets"]))
    k = 0
    for i, fl in enumerate(nd["sets"]):
        s.half(sets + 16 * i + 4, [k, len(fl)])
        for (h, i0, i1, i2) in fl:
            s.half(faces + 8 * k, [h, i0, i1, i2])
            k += 1
    s.half(a + 0x5A, [len(nd["sets"])])
    s.words(a + 0x64, [s.ptr(pts)])
    s.words(a + 0x6C, [s.ptr(sets)])
    s.words(a + 0x70, [s.ptr(faces)])
    s.words(a + 0x94, [s.ptr(idx)])


def build(s, case):
    w = {}
    table = s.alloc(len(case["table"]) * 4)
    s.words(table, case["table"])
    s.native(s.g("table"), table)
    s.words(s.g("flags"), [case["flags"]])
    P = s.alloc(0x40)
    Q = s.alloc(0xB0)
    s.words(s.g("ctx"), [s.ptr(P)])
    s.words(P + 4, [s.ptr(Q)])
    s.words(P + 0x10, [case["rot"]])
    n = len(case["qnodes"])
    skip = case["skip"]
    block = s.alloc((skip + n) * 0xA0)
    for i, nd in enumerate(case["qnodes"]):
        build_node(s, block + (skip + i) * 0xA0, nd)
    box = s.alloc(0x50)
    s.words(box + 0x44, [case["boxy"]])
    fl = s.alloc(n * 0x14 + 4)
    for i, f in enumerate(case["qflags"]):
        s.words(fl + i * 0x14, [f])
    s.words(Q + 0xA0, [case["qy"]])
    s.half(Q + 0x46, [skip, n])
    s.words(Q + 0x70, [s.ptr(block)])
    s.words(Q + 0x78, [s.ptr(box)])
    s.words(Q + 0x7C, [s.ptr(fl)])
    params = s.alloc(n * 0x14 + 0x28)
    for i in range(n + 1):
        s.half(params + i * 0x14 + 0x10, [case["params"][i]])
    R = s.alloc(8)
    s.words(R, [case["rflags"]])
    s.words(R + 4, [s.ptr(params)])
    if not case["rnull"]:
        s.words(P + 0x28, [s.ptr(R)])
    node = s.alloc(0xA0)
    build_node(s, node, case["fnode"])
    pts = s.alloc(12 * 64)
    s.words(pts, case["ppts"])
    if case["usenode"]:
        s.words(P + 0x2C, [s.ptr(node)])
    s.words(P + 0x30, [s.ptr(pts)])
    out = s.alloc(4)
    w.update(Q=Q, node=node, params=params, pts=pts, out=out)
    return w


def execute(s, case, w):
    op = case["op"]
    if op == "ride":
        return s.call_ride(case["x"], case["z"], w["out"])
    if op == "height":
        return s.call_height(case["x"], case["z"], case["a"], case["b"])
    return s.call_node(w["node"], w["params"] if case["useparams"] else 0, case["x"], case["z"],
                       w["pts"] if case["usept"] else 0, case["flag"])


def observe(s, case, w, answer):
    return {"answer": answer, "log": s.log(), "qy": s.rd_words(w["Q"] + 0xA0, 1), "out": s.rd_words(w["out"], 1)}


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, case, w, execute(s, case, w))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def coord(r):
    k = r.random()
    if k < 0.04:
        return r.choice(SPECIAL)
    if k < 0.5:
        return fb(r.randrange(-6, 7) * 0.5)
    return fb(r.uniform(-3, 3))


def gen_node(r):
    nidx = r.randint(3, 8)
    npts = r.randint(1, 4)
    pts = []
    for _ in range(npts):
        ny = r.choice([1.0, -1.0, 0.5, 2.0, 0.0, r.uniform(-2, 2)])
        pts += [fb(r.uniform(-2, 2)), fb(ny), fb(r.uniform(-2, 2))]
    sets = []
    for _ in range(r.choice([0, 1, 1, 2, 3])):
        fl = []
        for _ in range(r.randint(0, 6)):
            h = (r.randrange(npts) << 1) | (1 if r.random() < 0.85 else 0)
            fl.append((h, r.randrange(nidx), r.randrange(nidx), r.randrange(nidx)))
        sets.append(fl)
    return {"idx": [r.randrange(8) for _ in range(nidx)], "pts": pts, "sets": sets}


def base(r, op):
    n = r.choice([0, 1, 2, 3, 4])
    return {
        "op": op,
        "queues": {"fpclassifyf": [r.choice([0, 1, 1, 2])],
                   "DetermineHFieldHeightQuick": [r.choice([fb(r.uniform(-5, 5)), 0x7FC00000, 0])]},
        "x": coord(r), "z": coord(r),
        "rot": r.choice([0, 0x5A, 0xB4, 0x10E, 0x2D, 0x10F, 0xB3]),
        "flags": r.choice([0, 1, 2, 4, 8, 3, 6, 12, 5, 10, 14, 15]),
        "qy": r.choice([fb(r.uniform(-9, 9)), 0x7FC00000, 0, 0x7F800000]),
        "boxy": fb(r.uniform(-9, 9)),
        "table": [fb(r.randrange(-6, 7) * 0.5) if r.random() < 0.5 else fb(r.uniform(-3, 3)) for _ in range(16 * 3)],
        "fnode": gen_node(r),
        "usenode": r.random() < 0.8,
        "ppts": [fb(r.choice([1.0, -1.0, 0.5, 0.0, r.uniform(-2, 2)])) for _ in range(12 * 16)],
        "skip": r.choice([0, 1, 2]),
        "qnodes": [gen_node(r) for _ in range(n)],
        "qflags": [r.choice([0, 2, 6, 2, 4, 3, 7, 6, 2]) for _ in range(n)],
        "params": [r.randrange(0, 8) for _ in range(n + 1)],
        "rflags": r.choice([8, 8, 8, 0, 9, 1]),
        "rnull": r.random() < 0.15,
        "a": r.choice([0, 0, 1]), "b": r.choice([0, 0, 1]),
        "useparams": r.random() < 0.6, "usept": r.random() < 0.4, "flag": r.choice([0, 1, 4]),
    }


def mk(op):
    def rnd(r):
        return base(r, op)

    def edges():
        r = random.Random({"ride": 11, "height": 12, "node": 13}[op])
        for _ in range(40):
            c = base(r, op)
            if op == "ride":
                c["flags"] = r.choice([0, 1, 2, 4, 8, 3, 6, 12, 5, 10, 14, 15, 0, 1])
            yield c
        for rot in (0, 0x5A, 0xB4, 0x10E):
            for flag in (0, 1):
                for fl in (0, 1, 2, 4, 8):
                    c = base(r, op)
                    c.update(rot=rot, flag=flag, flags=fl, usenode=True, rnull=False, rflags=8, a=0, b=0)
                    yield c
    return edges, rnd


TESTS = []
for _name, _op in (("GetRideHeightAtPoint", "ride"), ("GetHeightFromPolySearch", "height"), ("DoNodePolySearch", "node")):
    _e, _r = mk(_op)
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
