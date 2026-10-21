"""Unit v5221, DoOrSetupSkirtDeform against libtpw_unit_DoOrSetupSkirtDeform.dylib (tests/unit_DoOrSetupSkirtDeform.cpp), main app
("SimTheme Park"), src/game/park/SkirtDeform.cpp:
  DoOrSetupSkirtDeform 0x1006daa4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record what they read: CombineMatricesNoWDest2nd (both matrices; changes nothing), InvertMatrix (the
source; copies it to the destination), VectorThroughLMatrixNoW (the matrix and the point; replaces the point by the queued x, y, z). modf
is a PowerPC fake (C semantics) against std::modf on the host; the float to unsigned helper 0x10237cd8 runs as the original.

The world of a case: a height field (origin, scale, cell counts, heights with a pad before them, quad flags), the three skirt globals (the
20 x 20 cell flag table and the two limits), a tree of node pairs (the source tree a and the deformed tree b, which may differ in shape),
the list buffer with its cursor cell, the counters, the best-node cell. What is compared: the log of the fakes, every b and a node's flags,
box and vertex words, the count, the best count and node, the list cursor and the list buffer.

Fused multiply-adds: HardwareRounding over the function (Unicorn rounds twice). Not tested: field coordinates far outside the field for
a node with the 0x20000 flag (the original reads the quad table with no bounds check), a zero scale (division by zero), NaN and infinity.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_DoOrSetupSkirtDeform"
MASK = 0xFFFFFFFF

FN = 0x1006daa4
SIZES = {FN: 3604}
OWN = set(SIZES)

CALLEE_COMBINE = 0x100c28fc
CALLEE_INVERT = 0x100c249c
CALLEE_VECTOR = 0x100c1f64

ADDR = dict(table=0x102da2ec, rows=0x102da92c, cols=0x102da930)
HOST_LAYOUT = [0, 8, 0xC, 0x10, 0x58, 0x60, 0x78, 0xA0, 8, 0x18, 0x28, 0x2C, 64]

PAD = 24


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


def stub_combine(e):
    L = e.cl
    src = struct.unpack(">16I", e.read(e.r(3), 64))
    dst = struct.unpack(">16I", e.read(e.r(4), 64))
    L.rec("CombineSrc0", src[:12])
    L.rec("CombineSrc3", src[12:])
    L.rec("CombineDst0", dst[:12])
    L.rec("CombineDst3", dst[12:])


def stub_invert(e):
    L = e.cl
    dst, src = e.r(3), e.r(4)
    data = e.read(src, 64)
    m = struct.unpack(">16I", data)
    L.rec("InvertSrc0", m[:12])
    L.rec("InvertSrc3", m[12:])
    e.write(dst, data)


def stub_vector(e):
    L = e.cl
    m = struct.unpack(">16I", e.read(e.r(3), 64))
    p = e.r(4)
    pt = struct.unpack(">3I", e.read(p, 12))
    L.rec("VecM0", m[:12])
    L.rec("VecM3", m[12:])
    L.rec("VecP", pt)
    e.write(p, struct.pack(">3I", L.ret("vx") & MASK, L.ret("vy") & MASK, L.ret("vz") & MASK))


def stub_modf(e):
    fr, ip = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", ip))
    e.set_f1(fr)


STUBS.internal(CALLEE_COMBINE, stub_combine)
STUBS.internal(CALLEE_INVERT, stub_invert)
STUBS.internal(CALLEE_VECTOR, stub_vector)
STUBS.imp("modf", stub_modf)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    # Ptr32 fields hold an arena offset on the host (the Mac address itself on the PowerPC side); plain pointer cells are 8 bytes on the host
    def p32(self, addr):
        return addr

    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_word(a)


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

    def call(self, w, case):
        args = [w["A"][0], w["B"][0], w["cell"]] + list(case["mat"]) + [
            w["hf"], case["setup"], w["count"], T.F(bitsf(case["lift"])), w["bestnode"], w["bestcount"], case["flags"],
            w["only"], case["mode"]]
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [a & MASK if isinstance(a, int) else a for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def p32(self, addr):
        return self.lib.tpw_skirt_offset(addr)

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def g(self, name):
        return self.lib.tpw_skirt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_skirt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, w, case):
        mat = self.alloc(64)
        self.words(mat, case["mat"])
        self.lib.tpw_skirt_run(w["A"][0], w["B"][0], w["cell"], mat, w["hf"], s32(case["setup"]), w["count"], bitsf(case["lift"]),
                               w["bestnode"], w["bestcount"], case["flags"] & MASK, w["only"], s32(case["mode"]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_skirt_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "offset").restype, getattr(lib, p + "offset").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    run = getattr(lib, p + "run")
    run.restype = None
    u64 = C.c_uint64
    run.argtypes = [u64, u64, u64, u64, u64, C.c_int32, u64, C.c_float, u64, u64, C.c_uint32, u64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("table"), case["table"])
    s.words(s.g("rows"), [case["rows"]])
    s.words(s.g("cols"), [case["cols"]])
    hfc = case["hf"]
    hb = s.alloc(0x30)
    heights = s.alloc(4 * len(hfc["heights"]))
    s.words(heights, hfc["heights"])
    quads = s.alloc(4 * len(hfc["quads"]))
    for i, q in enumerate(hfc["quads"]):
        s.half(quads + 4 * i, q)
    s.words(hb + 8, hfc["org"])
    s.words(hb + 0x18, [hfc["W"], hfc["H"]])
    s.words(hb + 0x28, [s.p32(heights + 4 * PAD), s.p32(quads)])
    nodes = case["nodes"]
    n = len(nodes)
    A = [0] * n
    B = [0] * n
    blocks = {}
    for i, nd in enumerate(nodes):
        if nd["hasA"]:
            A[i] = s.alloc(0xA0)
        if nd["hasB"]:
            B[i] = s.alloc(0xA0)
    for i, nd in enumerate(nodes):
        cnt = nd["n"]
        nblocks = cnt // 4 + 1
        if A[i]:
            v = s.alloc(0x30 * nblocks)
            s.words(v, nd["averts"])
            s.words(A[i], [nd["aflags"]])
            s.words(A[i] + 0x10, nd["amat"])
            s.half(A[i] + 0x58, cnt)
            s.words(A[i] + 0x60, [s.p32(v)])
            s.words(A[i] + 0x78, nd["abox"])
            blocks[("a", i)] = (v, nblocks)
        if B[i]:
            v = s.alloc(0x30 * nblocks)
            s.words(v, nd["bverts"])
            s.words(B[i], [nd["bflags"]])
            s.words(B[i] + 0x10, nd["bmat"])
            s.half(B[i] + 0x58, cnt)
            s.words(B[i] + 0x60, [s.p32(v)])
            s.words(B[i] + 0x78, nd["bbox"])
            blocks[("b", i)] = (v, nblocks)
    for tree in (A, B):
        kids = {}
        for i in range(1, n):
            if tree[i]:
                kids.setdefault(nodes[i]["parent"], []).append(i)
        for p, lst in kids.items():
            if not tree[p]:
                continue
            s.words(tree[p] + 0xC, [s.p32(tree[lst[0]])])
            for x, y in zip(lst, lst[1:]):
                s.words(tree[x] + 8, [s.p32(tree[y])])
    listbuf = s.alloc(2 * len(case["list"]) + 8)
    for i, v in enumerate(case["list"]):
        s.half(listbuf + 2 * i, v)
    cell = s.alloc(8)
    s.put_ptr(cell, listbuf)
    count = s.alloc(4)
    s.words(count, [case["countinit"]])
    bestnode = s.alloc(8)
    bestcount = 0
    if case["bestinit"] is not None:
        bestcount = s.alloc(4)
        s.words(bestcount, [case["bestinit"]])
    only = 0
    if case["only"] is not None:
        kind, i = case["only"]
        only = (A if kind == "a" else B)[i]
    return dict(hb=hb, hf=hb, A=A, B=B, blocks=blocks, listbuf=listbuf, cell=cell, count=count, bestnode=bestnode,
                bestcount=bestcount, only=only)


def label_of(w, addr):
    if addr == 0:
        return 0
    for i, a in enumerate(w["A"]):
        if a == addr:
            return 1 + i
    for i, b in enumerate(w["B"]):
        if b == addr:
            return 100 + i
    return -1


def observe(s, case, w):
    out = {"log": s.log()}
    for (kind, i), (v, nb) in sorted(w["blocks"].items()):
        node = (w["A"] if kind == "a" else w["B"])[i]
        out["%s%d" % (kind, i)] = s.rd_words(node, 1) + s.rd_words(node + 0x78, 6) + s.rd_words(v, 12 * nb)
    out["count"] = s.rd_word(w["count"])
    out["bestc"] = s.rd_word(w["bestcount"]) if w["bestcount"] else None
    out["bestn"] = label_of(w, s.get_ptr(w["bestnode"]))
    out["cursor"] = s.get_ptr(w["cell"]) - w["listbuf"]
    out["list"] = s.rd_halves(w["listbuf"], len(case["list"]))
    return out


def run(s, case):
    try:
        w = build(s, case)
        s.call(w, case)
        return observe(s, case, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

DELTAS = [0.0, 0.0005, 0.005, 0.0099, 0.011, 0.05, 0.0999, 0.101, 0.25, 0.4999, 0.5, 0.5001, 0.9, 0.9001, 0.99, 0.9901, 0.9999]
YS = [0.0, 0.005, -0.005, 0.0099, 0.011, 0.02, 50.0, 99.0, 101.0, -150.0, 1.5, -2.25, 0.09]


def rv(r, lo=-5.0, hi=5.0):
    return fb(r.uniform(lo, hi))


def coord(r, org, scale, ncell, wide, plain_only):
    k = r.randrange(0, ncell + (3 if wide else 1))
    d = r.choice(DELTAS) if r.random() < 0.8 else r.random()
    v = T.f32(org + scale * (k + d))
    if wide and r.random() < 0.1:
        v = T.f32(org - scale * r.random())
    return fb(v)


def make_case(r, setup=None, mode=None, plain=None, flags=None, wide=None, only="rand", nn=None, bestinit="rand"):
    setup = r.choice([0, 1]) if setup is None else setup
    mode = r.choice([0, 1]) if mode is None else mode
    flags = r.choice([0, 0, 0, 1, 2, 4, 5, 8, 1 | 2]) if flags is None else flags
    W, H = r.randrange(1, 7), r.randrange(1, 7)
    org = [r.choice([0.0, -3.25, 5.5, 100.0, 0.5]), r.choice([0.0, 2.0, -7.5, 33.25])]
    scale = [r.choice([1.0, 2.5, 0.5, 10.0, 0.25]), r.choice([1.0, 4.0, 0.5, 3.0])]
    nheights = (W + 5) * (H + 5) + 8
    hf = dict(org=[fb(org[0]), fb(org[1]), fb(scale[0]), fb(scale[1])], W=W, H=H,
              heights=[rv(r, -3, 3) for _ in range(2 * PAD + nheights)],
              quads=[r.choice([0, 4, 4, 1, 5, 2]) for _ in range((W + 3) * (H + 3))])
    n = nn if nn is not None else r.randrange(1, 6)
    if plain is None:
        plain_mode = r.choice([True, False, None])
    else:
        plain_mode = plain
    if wide is None:
        wide = plain_mode is True and r.random() < 0.3
    nodes = []
    total = 0
    for i in range(n):
        cnt = r.choice([0, 1, 2, 3, 4, 5, 6, 6])
        total += cnt
        nb = cnt // 4 + 1
        isplain = r.random() < 0.5 if plain_mode is None else plain_mode
        bflags = r.randrange(0, 1 << 20) & ~0x21200
        if not isplain:
            bflags |= 0x20000
        if r.random() < 0.75:
            bflags |= 0x1000
        if r.random() < 0.08:
            bflags |= 0x200
        nodes.append(dict(
            parent=-1 if i == 0 else r.randrange(0, i), n=cnt,
            hasA=(i == 0) or r.random() < 0.9, hasB=(i == 0) or r.random() < 0.9,
            bflags=bflags, aflags=r.randrange(0, 1 << 20),
            bmat=[rv(r, -2, 2) for _ in range(16)], amat=[rv(r, -2, 2) for _ in range(16)],
            bbox=[rv(r, -3, 0)] * 0 + [rv(r, -6, 0), rv(r, -6, 0), rv(r, -6, 0), rv(r, 0, 6), rv(r, 0, 6), rv(r, 0, 6)],
            abox=[rv(r) for _ in range(6)],
            bverts=[rv(r) for _ in range(12 * nb)], averts=[rv(r) for _ in range(12 * nb)]))
    nq = 2 * (total + 2) + 8
    qx = [coord(r, org[0], scale[0], W, wide, False) for _ in range(nq)]
    qz = [coord(r, org[1], scale[1], H, wide, False) for _ in range(nq)]
    qy = [fb(r.choice(YS)) if r.random() < 0.8 else rv(r, -2, 2) for _ in range(nq)]
    if r.random() < 0.5:
        listv = [i % 6 for i in range(60)]
    else:
        listv = [r.randrange(0, 6) for _ in range(60)]
    if only == "rand":
        o = r.choice([None, None, None, ("b", 0), ("b", r.randrange(0, n)), ("a", 0)])
    else:
        o = only
    if bestinit == "rand":
        bestinit = r.choice([None, 0, 1, 3, 0x7FFFFFFF])
    return {
        "op": "run", "queues": {"vx": qx, "vy": qy, "vz": qz},
        "mat": [rv(r, -2, 2) for _ in range(16)],
        "hf": hf, "table": [r.choice([0, 15, r.randrange(0, 16)]) for _ in range(400)],
        "rows": r.randrange(1, 9), "cols": r.randrange(1, 9),
        "nodes": nodes, "setup": setup, "mode": mode, "flags": flags,
        "lift": fb(r.choice([0.0, 0.5, -1.25, r.uniform(-2, 2)])),
        "list": listv, "countinit": r.randrange(0, 4), "bestinit": bestinit, "only": o,
    }


def edges():
    r = random.Random(5221)
    for setup in (0, 1):
        for mode in (0, 1):
            for plain in (True, False):
                for flags in (0, 1, 2, 4, 5):
                    yield make_case(r, setup=setup, mode=mode, plain=plain, flags=flags, wide=False)
    for setup in (0, 1):
        for mode in (0, 1):
            for _ in range(4):
                yield make_case(r, setup=setup, mode=mode, plain=True, flags=0, wide=True)
    for only in (None, ("b", 0), ("a", 0), ("b", 1)):
        for setup in (0, 1):
            yield make_case(r, setup=setup, plain=None, only=only, nn=3, flags=0)
    for bi in (None, 0, 5):
        yield make_case(r, setup=0, plain=True, flags=0, bestinit=bi, nn=2)
        yield make_case(r, setup=1, plain=False, flags=0, bestinit=bi, nn=2)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("DoOrSetupSkirtDeform", ppc, host, edge=edges, rnd=rnd),
]
