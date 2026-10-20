"""Unit v5288, StaticifyCreateOnlyAnimNodes, against libtpw_unit_StaticifyCreateOnlyAnimNodes.dylib
(tests/unit_StaticifyCreateOnlyAnimNodes.cpp), main app ("SimTheme Park"), src/RideAnimTick_StaticifyCreateOnlyAnimNodes.cpp:
  StaticifyCreateOnlyAnimNodes 0x100c6b54

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. There are no callees.

The world of a case: a ride set (flags), its mesh, the mesh's header (total node count, node count, the node and mini node tables with
random flag words) and the word gei_System.mFlags. What is compared: the set's flags, every word of the node tables, gei_System's flags.

No fused operations. Not tested: a NULL set, mesh or header (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaticifyCreateOnlyAnimNodes"
MASK = 0xFFFFFFFF

FN = 0x100C6B54
SIZES = {FN: 144}
OWN = set(SIZES)

ADDR = dict(sys=0x103BC9EC)
# set mFlags, set mHeads, mesh mHeader, header total, header nodes, header mNodes, header mMiniNodes, sizeof node, sizeof mini node
HOST_LAYOUT = [8, 16, 8, 0x42, 0x44, 0x70, 0x74, 0xA0, 0x58]
NODE, MINI = 0xA0, 0x58

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    set_flags, set_heads, mesh_header = 4, 8, 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        # gei_System is a data import: the TOC slot holds the address the loader gave it (not the Mac's 0x103bc9ec)
        return self.e.u32(0x102606E4)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def native(self, a, target):
        self.words(a, [target])

    def p32(self, a, target):
        self.words(a, [target])

    def call(self, s):
        T.call_ppc(self.e, FN, [s & MASK])


class Host(Side):
    fmt = "<I"
    set_flags, set_heads, mesh_header = 8, 16, 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sacn_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sacn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def native(self, a, target):
        self.write(a, struct.pack("<Q", target))

    def p32(self, a, target):
        self.words(a, [self.lib.tpw_sacn_off(target) if target else 0])

    def call(self, s):
        self.lib.tpw_sacn_run(s)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sacn_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    nodes, minis = case["nodes"], case["minis"]
    s.words(s.g("sys"), [case["sys"]])
    st = s.alloc(0x100)
    mesh = s.alloc(0x40)
    hdr = s.alloc(0xB0)
    nb = s.alloc(max(1, len(nodes)) * NODE)
    mb = s.alloc(max(1, len(minis)) * MINI)
    s.words(st + s.set_flags, [case["flags"]])
    s.native(st + s.set_heads, mesh)
    s.native(mesh + s.mesh_header, hdr)
    s.h16(hdr + 0x42, case["total"])
    s.h16(hdr + 0x44, case["count"])
    s.p32(hdr + 0x70, nb)
    s.p32(hdr + 0x74, mb)
    for i, w in enumerate(nodes):
        s.words(nb + i * NODE, [w])
    for i, w in enumerate(minis):
        s.words(mb + i * MINI, [w])
    case["_blocks"] = (st, nb, mb)
    return st


def observe(s, case):
    st, nb, mb = case["_blocks"]
    return {
        "log": s.log(),
        "set": s.rd_words(st + s.set_flags, 1),
        "sys": s.rd_words(s.g("sys"), 1),
        "nodes": [s.rd_words(nb + i * NODE, 1)[0] for i in range(len(case["nodes"]))],
        "minis": [s.rd_words(mb + i * MINI, 1)[0] for i in range(len(case["minis"]))],
    }


def run(s, case):
    try:
        st = build(s, case)
        s.call(st)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def fw(r):
    w = r.getrandbits(32)
    k = r.random()
    if k < 0.3:
        w |= 0x100
    elif k < 0.5:
        w &= ~0x100
    if r.random() < 0.6:
        w |= 0x800
    return w


def make(r, total, count, fl=None, sysf=None):
    """total: header count of all nodes; count: header count of full nodes; tables sized to cover the loop."""
    nn = max(count, 0)
    nm = max(total - count, 0)
    return {"queues": {}, "total": total, "count": count,
            "nodes": [fw(r) if fl is None else fl for _ in range(nn)],
            "minis": [fw(r) if fl is None else fl for _ in range(nm)],
            "flags": r.getrandbits(32) if r.random() < 0.7 else 0x40000000,
            "sys": r.getrandbits(32) if sysf is None else sysf}


def edges():
    r = random.Random(5288)
    for total, count in ((0, 0), (0, 3), (1, 1), (1, 0), (3, 3), (4, 2), (4, 0), (2, 5), (5, 5)):
        for fl in (None, 0x100, 0x900, 0x800, 0, 0xFFFFFFFF):
            for sysf in (None, 0, 0x80000000):
                yield make(r, total, count, fl, sysf)


def rnd(r):
    total = r.randrange(0, 8)
    return make(r, total, r.randrange(0, 8))


TESTS = [
    FnTest("StaticifyCreateOnlyAnimNodes", ppc, host, edge=edges, rnd=rnd),
]
