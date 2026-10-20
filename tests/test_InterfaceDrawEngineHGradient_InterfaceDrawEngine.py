"""Unit v5080: the horizontal gradient's constructor, against libtpw_unit_InterfaceDrawEngineHGradient_InterfaceDrawEngine.dylib
(tests/unit_InterfaceDrawEngineHGradient_InterfaceDrawEngine.cpp), main app
("SimTheme Park"), src/ui_DrawObjectEngineMesh_InterfaceDrawEngineHGradient_InterfaceDrawEngine.cpp:
  InterfaceDrawEngineHGradient::InterfaceDrawEngineHGradient 0x101898c8

A case is a world and a call. The world is built twice by one piece of Python code: in emulator memory (big-endian) for the original, and in
the host library's memory for the translation. The callees outside the unit (the base constructor, DMLoadTextures, DMCreateMesh,
DMFreeTextures, SetMatrixIdentity, CalcVerts, CalcUVs) are fakes on both sides that record their arguments; DMLoadTextures and DMCreateMesh
answer from the case; DMCreateMesh, when it answers nonzero, hands out the case's blocks (vertex sets, points, UV sets, faces, mesh, matrix,
face sets, textures), as the real one makes them.

The world of a case: a zeroed object, its texture set filled with junk (the constructor must clear it), a name and the blocks. What is
compared: the answer (this), the words the constructor writes (a dump of the object in a layout both sides share: pointers as labels), the
content of every block, and the event log.

No fused operations. Not tested: the host's object is not the Mac's size, so the words the constructor does not write are not compared.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceDrawEngineHGradient_InterfaceDrawEngine"
MASK = 0xFFFFFFFF

CTOR = 0x101898C8
SIZES = {CTOR: 776}
OWN = set(SIZES)

BASE_CTOR = 0x101CCCC0
DM_LOAD = 0x1003013C
DM_CREATE = 0x100301C0
DM_FREE = 0x10030178
SET_IDENTITY = 0x100C0E28
CALC_VERTS = 0x10189CD4
CALC_UVS = 0x10189D70

FOLDER = b"data:ui:textures\0"
BLOCK_SIZES = [0x60, 0x60, 0x40, 0x20, 0x20, 0x40, 0x20, 0x20]
NAMES = ["vsets", "points", "uvs", "faces", "mesh", "matrix", "fsets", "textures"]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_base(e):
    L = e.cl
    L.rec("InterfaceDrawObject", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_load(e):
    L = e.cl
    L.rec("DMLoadTextures", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("DMLoadTextures") & MASK)


def stub_create(e):
    L = e.cl
    st, tex = e.r(3), e.r(4)
    L.rec("DMCreateMesh", (L.lab(st), L.lab(tex), L.lab(e.r(5)), e.r(6)))
    r = L.ret("DMCreateMesh")
    if r:
        B = e.hg_blocks
        entry = e.u32(st + 8)
        for off, v in ((0x10, B[0]), (0x14, B[1]), (0x18, B[2]), (0x1C, B[3]), (0x24, B[4])):
            e.write(st + off, struct.pack(">I", v))
        e.write(entry + 0x28, struct.pack(">I", B[5]))
        e.write(entry + 0x2C, struct.pack(">I", B[6]))
        e.write(tex + 0x18, struct.pack(">I", B[7]))
    e.set_r(3, r & MASK)


def stub_free(e):
    L = e.cl
    L.rec("DMFreeTextures", (L.lab(e.r(3)),))


def stub_identity(e):
    L = e.cl
    L.rec("SetMatrixIdentity", (L.lab(e.r(3)),))


def stub_verts(e):
    L = e.cl
    L.rec("CalcVerts", (L.lab(e.r(3)), fb(e.f32(e.r(3) + 0x120))))


def stub_uvs(e):
    L = e.cl
    L.rec("CalcUVs", (L.lab(e.r(3)), fb(e.f32(e.r(3) + 0x120))))


STUBS.internal(BASE_CTOR, stub_base)
STUBS.internal(DM_LOAD, stub_load)
STUBS.internal(DM_CREATE, stub_create)
STUBS.internal(DM_FREE, stub_free)
STUBS.internal(SET_IDENTITY, stub_identity)
STUBS.internal(CALC_VERTS, stub_verts)
STUBS.internal(CALC_UVS, stub_uvs)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def make_world(self, case):
        self.name = self.alloc(16)
        self.write(self.name, b"meter.wct\0")
        self.blocks = [self.alloc(n) for n in BLOCK_SIZES]
        # label of a pointer: 0 is NULL, the object's own blocks by number
        self.labels = {self.name: 4}
        for i, b in enumerate(self.blocks):
            self.labels[b] = 10 + i

    def lab(self, p):
        p &= (1 << 64) - 1
        if p == 0:
            return 0
        if p in self.labels:
            return self.labels[p]
        return p & MASK


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x130)
        self.make_world(case)
        e.hg_blocks = self.blocks
        e.write(self.obj + 0xBC, b"\xA5" * 0x20)
        fixed = {self.obj: 1, self.obj + 0xBC: 2, self.obj + 0x6C: 3, self.name: 4, self.blocks[5]: 5}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr32(self, v):
        return v

    def log(self):
        return self.L.log

    def construct(self):
        r3, _, _ = T.call_ppc(self.e, CTOR, [self.obj, self.name])
        return int((r3 & MASK) == self.obj)

    def dump(self):
        o = self.obj
        w = lambda off: self.rd_word(o + off)
        h = lambda off: self.rd_halves(o + off, 1)[0]
        d = [w(0), w(0x3C), h(0x40), h(0x42), h(0x44), h(0x46), h(0x48)]
        d += self.rd_words(o + 0x4C, 6) * 2
        d += [w(0x6C), h(0x70), int(w(0x74) == o + 0x3C), int(w(0x78) == o + 0xDC)]
        d += [w(0xBC), h(0xC0), w(0xC4), w(0xC8), int(w(0xCC) == o + 0xAC), w(0xD0), w(0xD4), w(0xAC), w(0xB0), w(0x120)]
        d += [w(0x7C), w(0x80), w(0x84), w(0x88), w(0x90), w(0x64), w(0x68)]
        return d


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_hg_alloc(0x400)
        self.make_world(case)
        arr = (C.c_uint64 * 8)(*self.blocks)
        lib.tpw_hg_setblocks(arr)
        a = (C.c_uint64 * 4)()
        lib.tpw_hg_addrs(self.obj, a)
        CL.host_begin(lib, {1: a[0], 2: a[1], 3: a[2], 4: self.name, 5: self.blocks[5]}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_hg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def readable(self, p):
        return True

    def log(self):
        return CL.host_end(self.lib)

    def ptr32(self, v):
        """A Ptr32 word (an offset from the arena base) as an address."""
        return self.lib.tpw_hg_base() + v if v else 0

    def construct(self):
        return int(self.lib.tpw_hg_ctor(self.obj, self.name) == self.obj)

    def dump(self):
        out = (C.c_int64 * 64)()
        n = self.lib.tpw_hg_dump(self.obj, out)
        assert n == 40, n
        return [x & ((1 << 64) - 1) for x in out[:n]]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hg_alloc.restype, lib.tpw_hg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hg_base.restype, lib.tpw_hg_base.argtypes = C.c_uint64, []
    lib.tpw_hg_setblocks.restype, lib.tpw_hg_setblocks.argtypes = None, [C.POINTER(C.c_uint64)]
    lib.tpw_hg_addrs.restype, lib.tpw_hg_addrs.argtypes = None, [C.c_uint64, C.POINTER(C.c_uint64)]
    lib.tpw_hg_ctor.restype, lib.tpw_hg_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_hg_dump.restype, lib.tpw_hg_dump.argtypes = C.c_int32, [C.c_uint64, C.POINTER(C.c_int64)]
    lib.bound = True


POINTER_FIELDS = {26, 28, 29, 31, 33, 34, 35, 36, 37, 38, 39}


def normalise(s, d):
    """The dump with the pointers made labels (the folder's string is label 99, a pointer to nothing known its low word)."""
    out = list(d)
    for i in POINTER_FIELDS:
        out[i] = s.lab(d[i])
    if d[25] != 0 and s.read(d[25], len(FOLDER)) == FOLDER:
        out[25] = 99
    else:
        out[25] = d[25] & MASK
    out[0] = d[0] & MASK
    return out


def observe(s, answer):
    d = normalise(s, s.dump())
    b = s.blocks
    fsets_tex = s.rd_words(b[6], 1)[0]
    return {
        "answer": answer,
        "log": s.log(),
        "dump": d,
        "vsets": s.rd_words(b[0], 24),
        "points": s.rd_words(b[1], 24),
        "uvs": s.rd_words(b[2], 16),
        "faces": s.rd_halves(b[3], 16),
        "mesh": s.rd_words(b[4], 8),
        "matrix": s.rd_words(b[5], 16),
        "fsets_tex": s.lab(s.ptr32(fsets_tex)),
        "fsets": s.rd_halves(b[6] + 4, 4),
        "textures": s.rd_words(b[7], 8),
    }


def run(s, case):
    try:
        return observe(s, s.construct())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make_case(load, create):
    return {"queues": {"DMLoadTextures": [load], "DMCreateMesh": [create]}}


def edges():
    for load in (0, 1):
        for create in (0, 1):
            yield make_case(load, create)


def rnd(r):
    return make_case(r.choice([0, 1, 5]), r.choice([0, 1, 7]))


TESTS = [
    FnTest("InterfaceDrawEngineHGradient::InterfaceDrawEngineHGradient", ppc, host, edge=edges, rnd=rnd),
]
