"""Unit v5909, CalculateSkirtVertices, against libtpw_unit_CalculateSkirtVertices.dylib (tests/unit_CalculateSkirtVertices.cpp), main app
("SimTheme Park"), src/game/park/SkirtDeform.cpp:
  CalculateSkirtVertices 0x1006f748

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: DecideRideShapeDeformEdges (writes the skirt globals), SetMatrixIdentity (writes a fixed matrix),
DoOrSetupSkirtDeform (records the 13 arguments with the 64-byte matrix, writes the count, the list and the best node from the case's
queues), MReleaseAllocF (size, flags; the first kind may fail), CalcSkirtTextureDirnFlags and ERRORBox (format and ride name).
Pointers are logged as tags (set, shape, height field, node slot) so both sides log the same numbers.

The world of a case: the 20 x 20 skirt flag table, its rows and columns, a set with its mesh (native pointers: the Mac's 4 byte, the host's
8 byte), the mesh header with 7 node slots (flags, mF98 empty), the height field header, the shape, the queues. What is compared: the
globals, the mesh halfword, the mesh block, the root node, every node's flags and list block, the event log.

No fused operations. Not tested: a null height field header while a node has flag 0x40 (the original reads 0x6c of it unconditionally);
a failing second allocation (the original stores through the null block); a count above 8 (the fake writes 8 entries only).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CalculateSkirtVertices"
MASK = 0xFFFFFFFF

FN = 0x1006F748
SIZES = {FN: 1080}
OWN = set(SIZES)

DECIDE, SETID, DO, ALLOC, CALC, ERR = 0x1006D76C, 0x100C0E28, 0x1006DAA4, 0x10064BD4, 0x1006ECA0, 0x1006FD3C

ADDR = dict(flags=0x102DA2EC, rows=0x102DA92C, cols=0x102DA930)
FIXED_LAYOUT = [0x30, 0x18, 0x6C, 0x70, 0x44, 0x78, 0, 0x98, 0xA0]
NAT = {}
MARK = {"set": 0, "shape": 0, "nodes": 0, "hf": 0}
NSLOT = 7
ROOT_SLOT = 3


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def tag(p):
    p &= MASK
    if p == 0:
        return 0
    if p == MARK["set"]:
        return 1
    if p == MARK["shape"]:
        return 2
    if p == MARK["hf"]:
        return 3
    if MARK["nodes"] <= p < MARK["nodes"] + 0x1000:
        return 16 + p - MARK["nodes"]
    return 4


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_decide(e):
    L = e.cl
    w, h = s32(e.r(4)), s32(e.r(5))
    L.rec("Decide", (tag(e.r(3)), w, h))
    e.write(ADDR["flags"] + (1 * 20 + 2) * 4, struct.pack(">I", (w * 31 + h) & MASK))
    e.write(ADDR["rows"], struct.pack(">I", h & MASK))
    e.write(ADDR["cols"], struct.pack(">I", w & MASK))


def stub_setid(e):
    e.cl.rec("SetMatrixIdentity", (1 if e.r(3) else 0,))
    e.write(e.r(3), struct.pack(">16f", *[float(i + 1) for i in range(16)]))


def stub_do(e):
    L = e.cl
    sp = e.r(1)
    a, b, listp = e.r(3), e.r(4), e.r(5)
    mat = [e.r(i) for i in range(6, 11)] + [e.u32(sp + 0x38 + 4 * i) for i in range(11)]
    hf, setup, countp = e.u32(sp + 0x64), s32(e.u32(sp + 0x68)), e.u32(sp + 0x6C)
    bestp, bcp = e.u32(sp + 0x74), e.u32(sp + 0x78)
    flags, only, mode = e.u32(sp + 0x7C), e.u32(sp + 0x80), s32(e.u32(sp + 0x84))
    lst = e.u32(listp) if listp else 0
    ptrs = (1 if lst else 0) | (2 if countp else 0) | (4 if bestp else 0) | (8 if bcp else 0)
    L.rec("Do", (tag(a), tag(b), tag(hf), setup, flags, tag(only), mode, fb(e.f(1)), tag(e.u32(bestp)), s32(e.u32(bcp)), ptrs))
    L.rec("DoMatrix", tuple(mat[:12]))
    L.rec("DoMatrix2", tuple(mat[12:]))
    n = s32(L.ret("count"))
    e.write(countp, struct.pack(">I", n & MASK))
    for i in range(min(max(n, 0), 8)):
        e.write(lst + 2 * i, struct.pack(">H", ((n + 1) * 257 + i * 3) & 0xFFFF))
    if only == 0:
        sel = s32(L.ret("best"))
        e.write(bestp, struct.pack(">I", 0 if sel == 99 else (a + sel * 0xA0) & MASK))


def stub_alloc(e):
    L = e.cl
    size, flags = e.r(3), e.r(4)
    L.rec("MReleaseAllocF", (size, flags))
    if flags == 1 and L.ret("alloc") != 0:
        e.set_r(3, 0)
    else:
        e.set_r(3, e.alloc(max(size, 16)))


def stub_calc(e):
    e.cl.rec("CalcSkirt", (tag(e.r(3)), tag(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


STUBS.internal(DECIDE, stub_decide)
STUBS.internal(SETID, stub_setid)
STUBS.internal(DO, stub_do)
STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(CALC, stub_calc)
STUBS.internal(ERR, CL.recorder("ERRORBox", "ss"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wr16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def shorts(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def r16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    ptrfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.off = dict(heads=8, header=4, f0c=0xC, b14=0x14, root=0x2C, setsz=0x100, meshsz=0x40)

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

    def p32(self, a):
        return a

    def a32(self, w):
        return w

    def mark(self, st, shape, nodes, hf):
        MARK.update(set=st, shape=shape, nodes=nodes, hf=hf)

    def call(self, st, hdr, a, w, h):
        T.call_ppc(self.e, FN, [st, hdr, a, w & MASK, h & MASK])


class Host(Side):
    fmt = "<I"
    ptrfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        n = NAT
        self.off = dict(heads=n[0], header=n[1], f0c=n[2], b14=n[3], root=n[4], setsz=n[5], meshsz=n[6])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_csv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_csv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, a):
        return a - self.lib.tpw_csv_base()

    def a32(self, w):
        return w + self.lib.tpw_csv_base() if w else 0

    def mark(self, st, shape, nodes, hf):
        self.lib.tpw_csv_mark(st, shape, nodes, hf)

    def call(self, st, hdr, a, w, h):
        self.lib.tpw_csv_call(st, hdr, a, s32(w), s32(h))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_csv_"
    lib.tpw_csv_addr.restype, lib.tpw_csv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_csv_alloc.restype, lib.tpw_csv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_csv_base.restype, lib.tpw_csv_base.argtypes = C.c_uint64, []
    lib.tpw_csv_mark.restype, lib.tpw_csv_mark.argtypes = None, [C.c_uint64] * 4
    lib.tpw_csv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_csv_call.restype, lib.tpw_csv_call.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_csv_layout(out)
    assert list(out)[:9] == FIXED_LAYOUT, list(out)[:9]
    NAT.clear()
    NAT.update(enumerate(list(out)[9:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def wptr(s, a, v):
    s.write(a, struct.pack(s.ptrfmt, v))


def rptr(s, a):
    return struct.unpack(s.ptrfmt, s.read(a, 8 if s.ptrfmt == "<Q" else 4))[0]


def build(s, case):
    o = s.off
    s.words(s.g("flags"), case["cells"])
    s.words(s.g("rows"), [case["rows"]])
    s.words(s.g("cols"), [case["cols"]])
    nodes = s.alloc(NSLOT * 0xA0)
    s.write(nodes, bytes(NSLOT * 0xA0))
    for i in range(NSLOT):
        s.words(nodes + i * 0xA0, [case["nflags"][i]])
    s.nodes = nodes
    hfblock = s.alloc(0x40)
    s.words(hfblock, [0x11111111] * 16)
    s.hfblock = hfblock
    mh = s.alloc(0x100)
    s.write(mh, bytes(0x100))
    s.words(mh + 0x30, [case["hflags"]])
    s.write(mh + 0x18, case["name"].encode() + b"\0" * (20 - len(case["name"])))
    s.wr16(mh + 0x44, case["ncount"])
    s.words(mh + 0x70, [s.p32(nodes)])
    s.words(mh + 0x78, [s.p32(nodes + ROOT_SLOT * 0xA0)])
    s.mh = mh
    hdr = 0
    if not case["hdr_null"]:
        hdr = s.alloc(0x100)
        s.write(hdr, bytes(0x100))
        s.words(hdr + 0x6C, [0 if case["hf_null"] else s.p32(hfblock)])
    s.hdr = hdr
    mesh = s.alloc(max(o["meshsz"], 0x40))
    s.write(mesh, bytes(max(o["meshsz"], 0x40)))
    wptr(s, mesh + o["header"], mh)
    s.wr16(mesh + o["f0c"], case["f0c"])
    if case["b14"]:
        marker = s.alloc(16)
        s.write(marker, struct.pack(s.fmt[0] + "8H", 0x0A0B, 0x0C0D, 0x0102, 0x0304, 0, 0, 0, 0))
        wptr(s, mesh + o["b14"], marker)
    if case["root"] != 99:
        wptr(s, mesh + o["root"], nodes + case["root"] * 0xA0)
    s.mesh = mesh
    st = s.alloc(o["setsz"])
    s.write(st, bytes(o["setsz"]))
    wptr(s, st + o["heads"], mesh)
    s.st = st
    shape = 0
    if not case["a_null"]:
        shape = s.alloc(16)
        s.words(shape, [1, 2, 3, 4])
    s.shape = shape
    s.mark(st, shape, nodes, hfblock)


def execute(s, case):
    s.call(s.st, s.hdr, s.shape, case["w"], case["h"])


def blk(s, w):
    return s.a32(w)


def observe(s, case):
    o = s.off
    n = s.r16(s.mesh + o["f0c"])
    n = n - 0x10000 if n >= 0x8000 else n
    b14 = rptr(s, s.mesh + o["b14"])
    nb = n * 2 if n > 0 else 4
    root = rptr(s, s.mesh + o["root"])
    if root == 0:
        rootslot = -1
    elif s.nodes <= root < s.nodes + NSLOT * 0xA0:
        rootslot = (root - s.nodes) // 0xA0
    else:
        rootslot = -2
    nodes = []
    for i in range(NSLOT):
        base = s.nodes + i * 0xA0
        flags = s.rd_word(base)
        f98 = s.a32(s.rd_word(base + 0x98))
        info = None
        if f98:
            cnt = s.rd_word(f98)
            data = s.a32(s.rd_word(f98 + 4))
            info = (cnt, data - f98, s.shorts(data, cnt) if 0 < cnt <= 8 else None)
        nodes.append((flags, info))
    return {"log": s.log(), "cells": s.rd_words(s.g("flags"), 400), "rows": s.rd_word(s.g("rows")), "cols": s.rd_word(s.g("cols")),
            "n": n, "b14": s.shorts(b14, nb // 2) if b14 else None, "root": rootslot, "nodes": nodes}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["Pirate Ship", "Ferris", "", "A" * 19, "Big Wheel"]


def make(r, hdr_null=None, hf_null=None, a_null=None, hflags=None, counts=None, sel=None, root=None, alloc=None, ncount=None):
    c = {}
    c["hdr_null"] = (r.random() < 0.15) if hdr_null is None else hdr_null
    c["hf_null"] = (r.random() < 0.15) if hf_null is None else hf_null
    c["a_null"] = (r.random() < 0.2) if a_null is None else a_null
    base = r.choice([0, 0x80, 0x100, 0x8, 0x88, 0x188, 0x180, 0x108])
    c["hflags"] = (base | (r.getrandbits(32) & 0xFFFFFE77)) if hflags is None else hflags
    if hflags is None and r.random() < 0.6:
        c["hflags"] = base
    c["ncount"] = r.randrange(0, NSLOT + 1) if ncount is None else ncount
    c["nflags"] = [(0x40 if r.random() < 0.6 else 0) | (r.getrandbits(32) & 0x3F00 if r.random() < 0.3 else 0) for _ in range(NSLOT)]
    if c["hdr_null"]:
        c["nflags"] = [f & ~0x40 for f in c["nflags"]]
    c["name"] = r.choice(NAMES)
    c["w"] = r.choice([0, 1, -1, 5, 20, r.randrange(1, 40)])
    c["h"] = r.choice([0, 1, -1, 7, 20, r.randrange(1, 40)])
    c["cells"] = [r.getrandbits(32) for _ in range(400)]
    c["rows"], c["cols"] = r.getrandbits(32), r.getrandbits(32)
    c["f0c"] = r.getrandbits(16)
    c["b14"] = r.random() < 0.5
    c["root"] = r.choice([99, 0, 1, 3, 6]) if root is None else root
    cq = counts if counts is not None else [r.choice([-1, 0, 1, 2, 3, 8]) for _ in range(r.randrange(1, 5))]
    c["queues"] = {"count": cq, "best": [r.choice([99, -3, -1, 0, 1, 2, 3])] if sel is None else [sel],
                   "alloc": [r.choice([0, 0, 1])] if alloc is None else [alloc]}
    return c


def edges():
    r = random.Random(909)
    for hdr_null in (False, True):
        for hf_null in (False, True):
            for a_null in (False, True):
                for hflags in (0, 0x80, 0x100, 0x8, 0x88, 0x188, 0x180):
                    yield make(r, hdr_null, hf_null, a_null, hflags)
    for sel in (99, -3, 0, 3):
        for cnt in ([0], [3], [-1], [2, 3], [3, 0], [8, 1, 8]):
            for alloc in (0, 1):
                yield make(r, False, False, False, 0x8, cnt, sel, alloc=alloc, ncount=NSLOT)
    for root in (99, 0, 3, 6):
        yield make(r, False, False, False, 0x88, [2], 3, root, 0, NSLOT)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CalculateSkirtVertices", ppc, host, edge=edges, rnd=rnd),
]
