"""Unit v5693, DoMeshSkirtDeform, against libtpw_unit_DoMeshSkirtDeform.dylib (tests/unit_DoMeshSkirtDeform.cpp), main app
("SimTheme Park"), src/Debug_blendflags_DoMeshSkirtDeform.cpp:
  DoMeshSkirtDeform 0x1006e930

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. DoOrSetupSkirtDeform (0x1006daa4) is a fake on both
sides that records every argument (the pointers as labels, the list through its pointer, the by-value matrix word by word); SetMatrixIdentity
runs as the real code on both sides.

The world of a case: one block holding the mesh, its parent mesh, the mesh's header (flags, node list, first node, self header), the self
header, the height-field header, the nodes (some with a texture record whose word at +4 is the list), all at fixed offsets. What is compared:
the log, and the flag words of the header and the first nodes after the call.

No fused operations. Not tested: a NULL mesh, header or parent mesh (the original reads them unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DoMeshSkirtDeform"
MASK = 0xFFFFFFFF

FN = 0x1006E930
CALLEE = 0x1006DAA4

# offsets in the world block
MESH, MESH2, HDR, SELF, PARAM, HF, NODE_A, NODE_B, NODES, TEX, LISTS, BLOCK = (
    0x000, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700, 0x800, 0xD00, 0xE00, 0xF80)
HOST_LAYOUT = [4, 8, 0x14, 0x30, 0x44, 0x6C, 0x70, 0x78, 0x9C, 0x98, 0xA0, 4]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_skirt(e):
    L = e.cl
    sp = e.r(1)
    st = lambda off: e.u32(sp + off)
    L.rec("Skirt", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.u32(e.r(5))), L.lab(st(0x64)), s32(st(0x68)), L.lab(st(0x6C)),
                    canon_f(fb(e.f(1))), L.lab(st(0x74)), L.lab(st(0x78)), st(0x7C), L.lab(st(0x80)), s32(st(0x84))))
    w = [e.r(6), e.r(7), e.r(8), e.r(9), e.r(10)] + [st(0x24 + 4 * k) for k in range(5, 16)]
    L.rec("SkirtM1", tuple(x & MASK for x in w[:8]))
    L.rec("SkirtM2", tuple(x & MASK for x in w[8:]))


def s32(v):
    return T.s32(v)


STUBS.internal(CALLEE, stub_skirt)


def setup(runner):
    T.install_std_stubs(runner.emu)


def fixed_labels(w, n):
    d = {w + NODE_A: 1, w + NODE_B: 2, w + HF: 3, w + BLOCK: 4}
    for i in range(8):
        d[w + NODES + 0xA0 * i] = 10 + i
        d[w + LISTS + 16 * i] = 20 + i
    return d


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.w = e.alloc(0x1000)
        self.L = e.cl = CL.PpcLog(fixed_labels(self.w, 0), {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def p(self, a):
        return a

    def mesh(self, w, hdr, m2, blk):
        self.words(w + MESH + 4, [hdr])
        self.words(w + MESH + 8, [m2])
        self.words(w + MESH2 + 0x14, [blk])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, mesh, header, y, a):
        T.call_ppc(self.e, FN, [mesh & MASK, header & MASK, T.F(bitsf(y)), a & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.w = lib.tpw_dms_alloc(0x1000)
        CL.host_begin(lib, {v: k for k, v in fixed_labels(self.w, 0).items()}, {})

    def done(self):
        pass

    def p(self, a):
        return self.lib.tpw_dms_off(a)

    def mesh(self, w, hdr, m2, blk):
        self.write(w + MESH + HOST_LAYOUT_HOST[0], struct.pack("<Q", hdr))
        self.write(w + MESH + HOST_LAYOUT_HOST[1], struct.pack("<Q", m2))
        self.write(w + MESH2 + HOST_LAYOUT_HOST[2], struct.pack("<Q", blk))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, mesh, header, y, a):
        self.lib.tpw_dms_run(mesh, header, bitsf(y), a & MASK)


HOST_LAYOUT_HOST = [0, 0, 0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_dms_"
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64, C.c_uint64, C.c_float, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    got = list(out)[:n]
    HOST_LAYOUT_HOST[:] = got[:3]
    assert got[3:12] == HOST_LAYOUT[3:11] + [4], got
    lib.bound = True


def build(s, c):
    w = s.w
    p = s.p
    s.mesh(w, w + HDR, w + MESH2, w + BLOCK)
    # the mesh's header
    s.words(w + HDR + 0x30, [c["hflags"]])
    s.write(w + HDR + 0x44, struct.pack(s.fmt[0] + "H", c["count"]))           # mNodeCount is the halfword at 0x44
    s.words(w + HDR + 0x70, [p(w + NODES)])
    s.words(w + HDR + 0x78, [p(w + NODE_B)])
    s.words(w + HDR + 0x9C, [p(w + SELF)])
    s.words(w + SELF + 0x78, [p(w + NODE_A)])
    s.words(w + NODE_A, [c["flags_a"]])
    s.words(w + NODE_B, [c["flags_b"]])
    s.words(w + PARAM + 0x6C, [p(w + HF) if c["hf"] else 0])
    for i in range(c["count"]):
        n = w + NODES + 0xA0 * i
        if c["tex"][i]:
            s.words(n + 0x98, [p(w + TEX + 16 * i)])
            s.words(w + TEX + 16 * i + 4, [p(w + LISTS + 16 * i)])


def execute(s, c):
    header = s.w + PARAM if c["param"] else 0
    s.call(s.w + MESH, header, c["y"], c["a"])


def observe(s, c, _):
    w = s.w
    return {"log": s.log(), "hflags": s.rd_words(w + HDR + 0x30, 1), "fa": s.rd_words(w + NODE_A, 1),
            "fb": s.rd_words(w + NODE_B, 1)}


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    return fb(r.uniform(-300, 300))


def case(r, param=None, hf=None, count=None, hflags=None):
    count = r.choice([0, 1, 2, 3, 5, 8]) if count is None else count
    return {"param": r.random() < 0.8 if param is None else param, "hf": r.random() < 0.8 if hf is None else hf,
            "count": count, "tex": [r.random() < 0.6 for _ in range(8)],
            "hflags": r.getrandbits(32) if hflags is None else hflags, "flags_a": r.getrandbits(32), "flags_b": r.getrandbits(32),
            "y": rf(r), "a": r.choice([0, 1, 0x100, r.getrandbits(32)])}


def edges():
    r = random.Random(693)
    yield case(r, param=False)
    yield case(r, param=True, hf=False)
    for count in (0, 1, 2, 5, 8):
        for hflags in (0, 0x100, 0xFFFFFFFF, 0xFFFFFEFF):
            yield case(r, True, True, count, hflags)


def rnd(r):
    return case(r)


TESTS = [
    FnTest("DoMeshSkirtDeform", ppc, host, edge=edges, rnd=rnd),
]
