"""Unit v5258, three small ride functions, against libtpw_unit_GetNodePositionSimple.dylib (tests/unit_GetNodePositionSimple.cpp), main app
("SimTheme Park"), src/LevelSystems_GetNodePositionSimple.cpp:
  GetNodePositionSimple 0x1005c8b8, SetRideSpritePosnFocus 0x1005cc10, GetMeshPtrAtHFieldCell 0x1005c1b0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. SetupMeshForPolySearchSimple and GetRideIDAtCell
are fakes on both sides recording their arguments; the ride id comes from the case's queue.

The world of a case: a chain of nodes (links at +4, positions at +0x40..+0x48), or a mesh with its header and optional flag word, or the
height field header with its two sizes and the ride set table. What is compared: the answer, the out point, the two ride globals (the
context pointer as "is it the mesh"), the event log.

No fused operations. Not tested: a NULL node, mesh or height field (read unconditionally by the original).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetNodePositionSimple"
MASK = 0xFFFFFFFF

FN_NODE, FN_FOCUS, FN_CELL = 0x1005C8B8, 0x1005CC10, 0x1005C1B0
SIZES = {FN_NODE: 140, FN_FOCUS: 156, FN_CELL: 112}
OWN = set(SIZES)

CALLEE_SETUP = 0x1005C988
CALLEE_ID = 0x1005F080

ADDR = dict(hfhdr=0x102C6C7C, sets=0x102C79A4, ctx=0x102C1E10, flags=0x102C1E14)
SET_BASE = 0x70000000


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_setup(e):
    L = e.cl
    L.rec("SetupMesh", (L.lab(e.r(3)), s32(e.r(4))))


def stub_id(e):
    L = e.cl
    L.rec("GetRideIDAtCell", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, L.ret("GetRideIDAtCell") & MASK)


STUBS.internal(CALLEE_SETUP, stub_setup)
STUBS.internal(CALLEE_ID, stub_id)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    OFF_HDR, OFF_F28 = 4, 0x28

    def __init__(self, e, case):
        self.e = e
        self.mesh = e.alloc(0x100)
        self.L = e.cl = CL.PpcLog({self.mesh: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def off(self, a):
        return a

    def ptr(self, a, v):
        self.words(a, [v])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ctx_is(self, p):
        return int(self.rd_word(ADDR["ctx"]) == p)

    def node(self, n, o, unused):
        T.call_ppc(self.e, FN_NODE, [n, o, unused & MASK])

    def focus(self, m):
        T.call_ppc(self.e, FN_FOCUS, [m])

    def cell(self, x, y):
        r3, _, _ = T.call_ppc(self.e, FN_CELL, [x & MASK, y & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.OFF_HDR, self.OFF_F28 = lib.host_layout[0], lib.host_layout[1]
        self.mesh = self.alloc(0x100)
        CL.host_begin(lib, {1: self.mesh}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gn_alloc(n)

    def off(self, a):
        return self.lib.tpw_gn_off(a)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctx_is(self, p):
        return self.lib.tpw_gn_ctx_is(p)

    def node(self, n, o, unused):
        self.lib.tpw_gn_node(n, o, s32(unused))

    def focus(self, m):
        self.lib.tpw_gn_focus(m)

    def cell(self, x, y):
        return self.lib.tpw_gn_cell(x & MASK, y & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gn_addr.restype, lib.tpw_gn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gn_alloc.restype, lib.tpw_gn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gn_off.restype, lib.tpw_gn_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_gn_ctx_is.restype, lib.tpw_gn_ctx_is.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_gn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gn_node.restype, lib.tpw_gn_node.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_gn_focus.restype, lib.tpw_gn_focus.argtypes = None, [C.c_uint64]
    lib.tpw_gn_cell.restype, lib.tpw_gn_cell.argtypes = C.c_uint64, [C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gn_layout(out)
    lay = list(out)[:n]
    assert lay[3:6] == [0x30, 0xA0, 0x6C] and lay[6:] == [4, 0x10, 0x18, 12], lay
    lib.host_layout = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build_nodes(s, case):
    addrs = [s.alloc(0x60) for _ in case["nodes"]]
    for i, (a, nd) in enumerate(zip(addrs, case["nodes"])):
        nxt = s.off(addrs[i + 1]) if i + 1 < len(addrs) else 0
        s.words(a + 4, [nxt])
        s.words(a + 0x40, nd)
    return addrs


def run_node(s, case):
    addrs = build_nodes(s, case)
    out = s.alloc(16)
    s.words(out, [0x11111111] * 4)
    s.node(addrs[0], out, case["unused"])
    return {"out": [canon_f(w) for w in s.rd_words(out, 3)], "pad": s.rd_word(out + 12), "log": s.log()}


def run_focus(s, case):
    m = s.mesh
    hdr = s.alloc(0xC0)
    s.words(hdr + 0x30, [case["hflags"]])
    s.words(hdr + 0xA0, [case["fa0"]])
    s.ptr(m + s.OFF_HDR, hdr)
    if case["has28"]:
        blk = s.alloc(8)
        s.words(blk, [case["w28"]])
        s.ptr(m + s.OFF_F28, blk)
    s.words(s.g("flags"), [case["flags0"]])
    s.focus(m)
    return {"flags": s.rd_word(s.g("flags")), "ctx": s.ctx_is(m), "log": s.log()}


def run_cell(s, case):
    hdr = s.alloc(0xC0)
    hf = s.alloc(0x40)
    s.words(hf + 0x18, [case["w"], case["h"]])
    s.words(hdr + 0x6C, [s.off(hf)])
    s.ptr(s.g("hfhdr"), hdr)
    ps = s.g("sets")
    for i in range(64):
        if s.fmt == ">I":
            s.words(ps + 4 * i, [SET_BASE + 0x10 * i])
        else:
            s.write(ps + 8 * i, struct.pack("<Q", SET_BASE + 0x10 * i))
    ans = s.cell(*case["args"])
    return {"answer": ans, "log": s.log()}


def ppc(e, case):
    s = Ppc(e, case)
    try:
        return RUN[case["op"]](s, case)
    finally:
        s.done()


def host(lib, case):
    s = Host(lib, case)
    try:
        return RUN[case["op"]](s, case)
    finally:
        s.done()


RUN = {"node": run_node, "focus": run_focus, "cell": run_cell}

# ---------------------------------------------------------------------------------------------------------------- cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def node_case(r, n=None):
    n = r.randrange(1, 6) if n is None else n
    return {"op": "node", "queues": {}, "unused": r.choice([0, 1, -1, r.getrandbits(32)]),
            "nodes": [[rf(r), rf(r), rf(r)] for _ in range(n)]}


def node_edges():
    r = random.Random(11)
    for n in (1, 2, 3, 4, 5):
        yield node_case(r, n)
    c = node_case(r, 3)
    c["nodes"] = [[0x80000000] * 3, [0x80000000] * 3, [0x80000000] * 3]
    yield c
    c = node_case(r, 3)
    c["nodes"] = [[1, 2, 3], [0x7F800000, 0xFF800000, 0x7FC00000], [0x3F800000] * 3]
    yield c


def node_random(r):
    return node_case(r)


def focus_case(r, has28=None, w28=None, hflags=None, fa0=None):
    return {"op": "focus", "queues": {}, "flags0": r.getrandbits(32),
            "has28": r.choice([0, 1]) if has28 is None else has28,
            "w28": r.getrandbits(32) if w28 is None else w28,
            "hflags": r.getrandbits(32) if hflags is None else hflags,
            "fa0": r.choice([0, 0x80000000, 0x7FC00000, 0x3F800000, 1, rf(r)]) if fa0 is None else fa0}


def focus_edges():
    r = random.Random(12)
    for has28 in (0, 1):
        for w28 in (8, 0, 7, 0xFFFFFFF7, 0x88):
            for hflags in (0, 0x80, 0x7F, 0xFFFFFF7F):
                for fa0 in (0, 0x80000000, 0x3F800000, 0x7FC00000, 1):
                    yield focus_case(r, has28, w28, hflags, fa0)


def focus_random(r):
    return focus_case(r)


def cell_case(r, x=None, y=None, w=None, h=None, rid=None):
    w = r.choice([0, 1, 2, 16, r.randrange(1, 100)]) if w is None else w
    h = r.choice([0, 1, 2, 16, r.randrange(1, 100)]) if h is None else h

    def pick(d):
        return r.choice([0, d - 1, d, d + 1, 0xFFFFFFFF, 0x80000000, r.randrange(0, 120)]) & MASK
    x = pick(w) if x is None else x
    y = pick(h) if y is None else y
    rid = r.choice([0, 1, 63, r.randrange(0, 64)]) if rid is None else rid
    return {"op": "cell", "queues": {"GetRideIDAtCell": [rid]}, "w": w, "h": h, "args": [x, y]}


def cell_edges():
    r = random.Random(13)
    for w, h in ((10, 20), (1, 1), (0, 5), (5, 0)):
        for x in (0, w - 1, w, w + 1, 0xFFFFFFFF):
            for y in (0, h - 1, h, h + 1, 0x80000000):
                yield cell_case(r, x & MASK, y & MASK, w, h, r.choice([0, 5]))


def cell_random(r):
    return cell_case(r)


TESTS = [
    FnTest("GetNodePositionSimple", ppc, host, edge=node_edges, rnd=node_random),
    FnTest("SetRideSpritePosnFocus", ppc, host, edge=focus_edges, rnd=focus_random),
    FnTest("GetMeshPtrAtHFieldCell", ppc, host, edge=cell_edges, rnd=cell_random),
]
