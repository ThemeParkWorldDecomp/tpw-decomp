"""Unit v5061, the ride height map rasteriser, against libtpw_unit_CalculateMaxVertexHeights.dylib (tests/unit_CalculateMaxVertexHeights.cpp), main app
("SimTheme Park"), src/game/park/TPHField.cpp:
  CalculateMaxVertexHeights 0x1005d304

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. CombineMatricesNoWDest2nd (changes two floats of the
destination matrix) and VectorThroughLMatrixNoW (replaces the point from a queue) are fakes on both sides that record what they read.

The world of a case: a tree of 1 to 6 nodes (flags, matrix, vertex blocks, face sets, faces, the face index table), the height map (bounds,
cells), the stride and the two limits, the scan table zeroed, the error slot and the flag. What is compared: the answer, the call log, the
scan table, the map's bounds and cells, every node's vertex blocks, the error slot (as a node number) and the flag.

No fused operations; fctiwz only sees small values. Not tested: more than 50 rows in one face (the table is 50 rows), negative or zero limits
(rows would go negative), NaN coordinates (the processors make different NaNs).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CalculateMaxVertexHeights"
MASK = 0xFFFFFFFF

FN = 0x1005D304
SIZES = {FN: 1984}
OWN = set(SIZES)

COMBINE = 0x100C28FC
VECTOR = 0x100C1F64

ADDR = dict(scan=0x102C1E20, stride=0x102C2144, limz=0x102C2148, limx=0x102C214C)
HOST_LAYOUT = [0, 8, 0xC, 0x10, 0x58, 0x5A, 0x60, 0x6C, 0x70, 0x94, 8, 0x10, 0xC, 0x18, 0x24]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_combine(e):
    """void CombineMatricesNoWDest2nd(s3DAxis* source, sMatrix* destination)"""
    L = e.cl
    src, dst = e.r(3), e.r(4)
    s0, s12 = e.u32(src), e.u32(src + 48)
    L.rec("Combine", (s0, s12, e.u32(dst), e.u32(dst + 48), e.u32(dst + 52)))
    e.write(dst, struct.pack(">I", L.ret("cmb") & MASK))
    e.write(dst + 52, struct.pack(">I", s0))


def stub_vector(e):
    """void VectorThroughLMatrixNoW(sMatrix* matrix, s3DPoint* point)"""
    L = e.cl
    m, p = e.r(3), e.r(4)
    L.rec("Vector", (e.u32(p), e.u32(p + 4), e.u32(p + 8), e.u32(m), e.u32(m + 4), e.u32(m + 16), e.u32(m + 20), e.u32(m + 40),
                     e.u32(m + 48), e.u32(m + 52), e.u32(m + 56)))
    e.write(p, b"".join(struct.pack(">I", L.ret("vec") & MASK) for _ in range(3)))


STUBS.internal(COMBINE, stub_combine)
STUBS.internal(VECTOR, stub_vector)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

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

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, w, case):
        args = [w["nodes"][0], 0] + [x & MASK for x in case["matrix"]] + [w["map"], w["bad"], 0, case["p7"] & MASK, w["flag"]]
        r3, _, _ = T.call_ppc(self.e, FN, args)
        return T.s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cmvh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cmvh_alloc(n)

    def ptr(self, a):
        """A Ptr32 field's word: the offset from the arena base."""
        return (a - self.lib.tpw_cmvh_base() & MASK) if a else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, w, case):
        arr = (C.c_float * 16)(*[bitsf(x) for x in case["matrix"]])
        return self.lib.tpw_cmvh_run(w["nodes"][0], arr, w["map"], w["bad"], T.s32(case["p7"]), w["flag"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cmvh_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cmvh_base.restype, lib.tpw_cmvh_base.argtypes = C.c_uint64, []
    f = getattr(lib, p + "run")
    f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.POINTER(C.c_float), C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    w = {}
    s.words(s.g("limx"), [fb(case["limx"])])
    s.words(s.g("limz"), [fb(case["limz"])])
    s.words(s.g("stride"), [case["stride"]])
    s.words(s.g("scan"), [0] * 200)
    nds = case["nodes"]
    addrs = [s.alloc(0xA0) for _ in nds]
    w["nodes"] = addrs
    w["verts"] = []
    for i, nd in enumerate(nds):
        a = addrs[i]
        blocks = len(nd["verts"]) // 12
        vb = s.alloc(48 * blocks)
        s.words(vb, nd["verts"])
        w["verts"].append((vb, 12 * blocks))
        fs = s.alloc(16 * max(1, len(nd["sets"])))
        for j, (first, cnt) in enumerate(nd["sets"]):
            s.halves(fs + 16 * j + 4, [first, cnt])
        fc = s.alloc(8 * max(1, len(nd["faces"])))
        for j, face in enumerate(nd["faces"]):
            s.halves(fc + 8 * j, face)
        f94 = s.alloc(2 * len(nd["f94"]))
        s.halves(f94, nd["f94"])
        kids = [addrs[k] for k in nd["kids"]]
        s.words(a, [nd["flags"]])
        s.words(a + 0xC, [s.ptr(kids[0]) if kids else 0])
        s.words(a + 0x10, nd["matrix"])
        s.halves(a + 0x58, [nd["vcount"], len(nd["sets"])])
        s.words(a + 0x60, [s.ptr(vb)])
        s.words(a + 0x6C, [s.ptr(fs)])
        s.words(a + 0x70, [s.ptr(fc)])
        s.words(a + 0x94, [s.ptr(f94)])
        for k in range(len(kids) - 1):
            s.words(kids[k] + 8, [s.ptr(kids[k + 1])])
    size = case["stride"] * 24 + 64
    data = s.alloc(size)
    s.write(data, bytes(case["cells"][:size]))
    m = s.alloc(0x30)
    s.words(m + 0xC, [s.ptr(data)])
    s.words(m + 0x18, case["mmin"] + case["mmax"])
    s.words(m + 0x24, case["mmax"])
    w["map"], w["data"], w["size"] = m, data, size
    w["bad"] = s.alloc(8) if case["has_bad"] else 0
    if w["bad"]:
        s.words(w["bad"], [case["bad0"]])
    w["flag"] = s.alloc(8)
    s.words(w["flag"], [case["flag0"]])
    return w


def observe(s, case, w, answer):
    index = {a & MASK: i + 1 for i, a in enumerate(w["nodes"])}
    out = {"answer": answer, "log": s.log(), "scan": s.rd_words(s.g("scan"), 200), "map": s.rd_words(w["map"], 3) + s.rd_words(w["map"] + 0x10, 8),
           "cells": s.read(w["data"], w["size"]), "flag": s.rd_word(w["flag"])}
    if w["bad"]:
        v = s.rd_word(w["bad"])
        out["bad"] = index.get(v, ("raw", v))
    for i, (vb, n) in enumerate(w["verts"]):
        out["v%d" % i] = s.rd_words(vb, n)
    return out


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, case, w, s.call(w, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def xs(lim):
    return [-2.0, -1.0001, -1.0, -0.5, -0.0, 0.0, lim - 0.5, lim, lim + 0.5, lim + 1.0, lim + 1.0001, lim + 3.0]


YS = [-5.0, -0.001, 0.0, 0.5, 50.0, 99.99, 99.999, 100.0, 100.5, 200.0]


def make_case(r, forced=None, edgep=0.2, nnodes=None, flags=None):
    limx = T.f32(r.choice([float(r.randint(1, 40)), r.uniform(1, 40)]))
    limz = T.f32(r.choice([float(r.randint(1, 40)), r.uniform(1, 40)]))
    n = nnodes if nnodes is not None else r.randint(1, 4)
    kids = [[] for _ in range(n)]
    for i in range(1, n):
        kids[r.randrange(i)].append(i)
    nodes = []
    total_v = 0
    for i in range(n):
        vc = r.choice([0, 1, 2, 3, 4, 4, 5])   # the fakes' queues hold at most 64 answers
        blocks = max(1, (vc + 3) // 4)
        verts = []
        for _ in range(blocks):
            verts += [fb(r.uniform(0, limx * 0.99)) for _ in range(4)]
            verts += [fb(r.uniform(0, 99)) for _ in range(4)]
            verts += [fb(r.uniform(0, limz * 0.99)) for _ in range(4)]
        nfaces = r.randint(0, 5)
        sets = []
        for _ in range(r.randint(0, 3)):
            first = r.randint(0, nfaces)
            sets.append((first, r.randint(0, nfaces - first)))
        faces = [[r.randrange(0x10000)] + [r.randrange(8) for _ in range(3)] for _ in range(nfaces)]
        f = flags if flags is not None else r.choice([0] * 8 + [0x200, 0x10, 0x8000, 0x8210, 0x1, 0x7FEF])
        nodes.append(dict(flags=f, matrix=[fb(r.uniform(-3, 3)) for _ in range(16)], vcount=vc, verts=verts, sets=sets,
                          faces=faces, f94=[r.randrange(4 * blocks) for _ in range(8)], kids=kids[i]))
        if f & 0x8210 == 0:
            total_v += vc
    vec = []
    k = 0
    for _ in range(total_v):
        for comp in range(3):
            if forced is not None:
                v = forced[k % len(forced)][comp]
                k += 1 if comp == 2 else 0
            elif r.random() < edgep:
                v = r.choice(xs(limx) if comp == 0 else YS if comp == 1 else xs(limz))
            else:
                v = r.uniform(0, (limx, 99, limz)[comp])
            vec.append(fb(T.f32(v)))
    stride = r.choice([24, 32, 40])
    return {"limx": limx, "limz": limz, "stride": stride, "nodes": nodes,
            "queues": {"vec": vec, "cmb": [fb(r.uniform(-3, 3)) for _ in range(n)]},
            "matrix": [fb(r.uniform(-3, 3)) for _ in range(16)], "p7": r.choice([0, 1, -5, 77]),
            "cells": [r.randrange(256) if r.random() < 0.9 else 0 for _ in range(40 * 24 + 64)],
            "mmin": [fb(r.uniform(-10, 60)) for _ in range(3)], "mmax": [fb(r.uniform(-10, 60)) for _ in range(3)],
            "has_bad": r.random() < 0.8, "bad0": r.choice([0, 0x55]), "flag0": r.choice([0, 0, 1, 5])}


def edges():
    r = random.Random(61)
    for f in (0x200, 0x10, 0x8000, 0x8210, 0):
        yield make_case(r, nnodes=1, flags=f)
    for _ in range(6):
        yield make_case(r, nnodes=r.randint(2, 4), flags=0, edgep=0)
    # every clamp edge on its own, on three vertices at a time
    for i in range(14):
        lim = 10.0
        c = make_case(r, nnodes=1, flags=0, edgep=0)
        c["limx"] = c["limz"] = lim
        c["nodes"][0]["vcount"] = 3
        e = xs(lim)
        forced = [(e[(i + j) % 12], YS[(i + j) % 10], e[(i * 5 + j) % 12]) for j in range(3)]
        cc = make_case(random.Random(i), forced=forced, nnodes=1, flags=0)
        cc["limx"] = cc["limz"] = lim
        cc["nodes"][0]["vcount"] = 3
        cc["nodes"][0]["sets"] = [(0, 3)]
        cc["nodes"][0]["faces"] = [[0, 0, 1, 2], [0, 2, 1, 0], [0, 1, 1, 2]]
        yield cc


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CalculateMaxVertexHeights", ppc, host, edge=edges, rnd=rnd),
]
