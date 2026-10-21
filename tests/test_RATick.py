"""Unit v5308, the ride animation tick and the quick vertex copy, against libtpw_unit_RATick.dylib (tests/unit_RATick.cpp), main app
("SimTheme Park"), src/game/rides/RideAnim.cpp:
  RATick 0x100c4e20, QuickCopyUnmaskedMeshVertices 0x100c4f18

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. AnimateNode and
RAAnimateTextureFrames are fakes on both sides that record their arguments (objects as labels); memcpy is the std stub on the PowerPC side
and the real one on the host (the copied bytes are compared).

The world of a case: RATick: a control block, a mesh record pointing at a header, an animation header (flags, node animation count, array)
with node animations that point at nodes, TPGlobals' engine flags word. QuickCopy: two meshes whose headers hold a node count and an array
of 0xa0-byte nodes with random words, flags, vertex counts, vertex blocks (or none) and boxes. What is compared: the answer, the event log,
every node's words (vertex pointers as present or not) and every vertex block of both meshes.

No fused operations. Not tested: a NULL mesh or header (read unconditionally), a destination vertex block smaller than the source's sets.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RATick"
MASK = 0xFFFFFFFF

FN_TICK, FN_QUICK = 0x100C4E20, 0x100C4F18
SIZES = {FN_TICK: 172, FN_QUICK: 240}
OWN = set(SIZES)

ANIMATE_NODE = 0x100C45D4
ANIMATE_TEX = 0x100C3678

ADDR = dict(globals=0x103B74C8)
HOST_LAYOUT = [8, 0xA0, 0x40, 0, 0x58, 0x60, 0x78, 0x18, 0x3C, 0x2C, 0x12, 0x70, 0x44]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_animate_node(e):
    L = e.cl
    L.rec("AnimateNode", tuple(L.lab(e.r(i)) for i in (3, 4, 5, 6, 7)))
    e.set_r(3, L.ret("AnimateNode") & MASK)


def stub_animate_tex(e):
    L = e.cl
    L.rec("RAAnimateTextureFrames", tuple(L.lab(e.r(i)) for i in (3, 4, 5)))


STUBS.internal(ANIMATE_NODE, stub_animate_node)
STUBS.internal(ANIMATE_TEX, stub_animate_tex)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def p(self, a):
        """The word a Ptr32 field holds for the block at a."""
        return a

    def mesh(self, header):
        m = self.alloc(0x100)
        if self.fmt == ">I":
            self.words(m + 4, [header])
        else:
            self.write(m + 8, struct.pack("<Q", header))
        return m


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def begin(self, pairs):
        self.L = self.e.cl = CL.PpcLog({a: n for n, a in pairs}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, pairs):
        CL.host_begin(self.lib, {n: a for n, a in pairs}, self.case["queues"])

    def done(self):
        pass

    def p(self, a):
        return self.lib.tpw_ratick_off(a)

    def g(self, name):
        return self.lib.tpw_ratick_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ratick_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_TICK:
            return self.lib.tpw_ratick_tick(*args) & MASK
        self.lib.tpw_ratick_quick(*args)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ratick_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ratick_off.restype, lib.tpw_ratick_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ratick_tick.restype, lib.tpw_ratick_tick.argtypes = C.c_uint32, [C.c_uint64] * 3
    lib.tpw_ratick_quick.restype, lib.tpw_ratick_quick.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build_tick(s, case):
    w = {"pairs": []}
    g = s.g("globals")
    s.words(g + 0x400C, [case["engine"]])
    control = s.alloc(0x40)
    header = s.alloc(0xC0)
    mesh = s.mesh(header)
    k = case["count"]
    anim = 0
    w["pairs"] += [(1, control), (2, header)]
    if not case["nullanim"]:
        anim = s.alloc(0x40)
        arr = s.alloc(0x40 * max(k, 1))
        s.words(anim, [case["flags"]])
        s.half(anim + 0x12, k)
        s.words(anim + 0x2C, [s.p(arr)])
        w["pairs"].append((3, anim))
        for i in range(k):
            node = s.alloc(0x10)
            s.words(arr + 0x40 * i + 0x3C, [s.p(node)])
            w["pairs"] += [(10 + i, arr + 0x40 * i), (30 + i, node)]
    w.update(control=control, mesh=mesh, anim=anim)
    return w


def node_build(s, nd):
    base = s.alloc(0xA0)
    s.words(base, nd["w"])
    s.words(base, [nd["flags"]])
    s.half(base + 0x58, nd["vc"])
    s.half(base + 0x5A, nd["w"][0x16])
    blk = 0
    if nd["hasv"]:
        blk = s.alloc(0x100)
        s.words(blk, nd["blk"])
    s.words(base + 0x60, [s.p(blk)])
    return base, blk


def mesh_build(s, nodes, count):
    arr = s.alloc(0xA0 * len(nodes))
    blks = []
    for i, nd in enumerate(nodes):
        base, blk = node_build(s, nd)
        s.write(arr + 0xA0 * i, s.read(base, 0xA0))
        blks.append(blk)
    header = s.alloc(0xC0)
    s.words(header + 0x70, [s.p(arr)])
    s.half(header + 0x44, count)
    return s.mesh(header), arr, blks


def build_quick(s, case):
    src, sarr, sblk = mesh_build(s, case["src"], len(case["src"]))
    dst, darr, dblk = mesh_build(s, case["dest"], case["n"])
    return {"pairs": [], "src": src, "dst": dst, "sarr": sarr, "darr": darr, "sblk": sblk, "dblk": dblk}


def observe_nodes(s, arr, count, blks):
    out = []
    for i in range(count):
        ws = s.rd_words(arr + 0xA0 * i, 0x28)
        ws[0x18] = 1 if ws[0x18] else 0
        a = arr + 0xA0 * i + 0x58           # a word of two halfwords: the same halves on both sides
        ws[0x16] = struct.unpack(s.fmt[0] + "2H", s.read(a, 4))
        out.append(ws)
    for b in blks:
        out.append(s.rd_words(b, 0x40) if b else None)
    return out


def run(s, case):
    try:
        if case["op"] == "tick":
            w = build_tick(s, case)
            s.begin(w["pairs"])
            ans = s.call(FN_TICK, [w["control"], w["mesh"], w["anim"]])
            return {"answer": ans, "log": s.log()}
        w = build_quick(s, case)
        s.begin(w["pairs"])
        ans = s.call(FN_QUICK, [w["src"], w["dst"], case["force"]])
        n = len(case["src"])
        return {"log": s.log(), "dest": observe_nodes(s, w["darr"], n, w["dblk"]),
                "src": observe_nodes(s, w["sarr"], n, w["sblk"])}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def tick_case(r, k=None, flags=None, engine=None, null=None):
    return {"op": "tick", "queues": {"AnimateNode": [r.choice([0, 1])]},
            "count": r.randrange(0, 5) if k is None else k,
            "flags": r.getrandbits(32) if flags is None else flags,
            "engine": r.getrandbits(32) if engine is None else engine,
            "nullanim": r.random() < 0.1 if null is None else null}


def tick_edges():
    r = random.Random(5308)
    for null in (True, False):
        for k in (0, 1, 3):
            for flags in (0, 2, 3, 0xFFFFFFFD, 0xFFFFFFFF):
                for engine in (0, 8, 0xFFFFFFF7, 0xFFFFFFFF):
                    yield tick_case(r, k, flags, engine, null)


def tick_random(r):
    return tick_case(r)


def mknode(r, vc=None, hasv=None, flag=None):
    return {"w": [r.getrandbits(32) for _ in range(0x28)],
            "flags": (r.getrandbits(32) & ~0x200000) | (0x200000 if (r.random() < 0.5 if flag is None else flag) else 0),
            "vc": r.choice([0, 1, 3, 4, 5, 8, 13, 20, 0xFFFF & r.getrandbits(5)]) if vc is None else vc,
            "hasv": r.random() < 0.8 if hasv is None else hasv,
            "blk": [r.getrandbits(32) for _ in range(0x40)]}


def quick_case(r, n=None, force=None, **kw):
    L = 6
    return {"op": "quick", "queues": {}, "n": r.randrange(0, L + 1) if n is None else n,
            "force": r.choice([0, 1, -1, 7]) if force is None else force,
            "dest": [mknode(r, **kw) for _ in range(L)], "src": [mknode(r, **kw) for _ in range(L)]}


def quick_edges():
    r = random.Random(53081)
    for n in (0, 1, 6):
        for force in (0, 1):
            yield quick_case(r, n, force)
    for vc in (0, 1, 3, 4, 5, 13):
        for flag in (False, True):
            for force in (0, 1):
                yield quick_case(r, 6, force, vc=vc, flag=flag)
    for hasv in (False, True):
        yield quick_case(r, 6, 1, hasv=hasv)
    c = quick_case(r, 6, 1)
    for i in range(6):                       # source vertex count zero, destination non-zero: no vertex copy, box copy
        c["src"][i]["vc"] = 0
        c["dest"][i]["vc"] = 4
        c["dest"][i]["hasv"] = c["src"][i]["hasv"] = True
    yield c
    c = quick_case(r, 6, 1)
    for i in range(6):                       # destination vertex count zero: nothing at all
        c["dest"][i]["vc"] = 0
    yield c


def quick_random(r):
    return quick_case(r)


TESTS = [
    FnTest("RATick", ppc, host, edge=tick_edges, rnd=tick_random),
    FnTest("QuickCopyUnmaskedMeshVertices", ppc, host, edge=quick_edges, rnd=quick_random),
]
