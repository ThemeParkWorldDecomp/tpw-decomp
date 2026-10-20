"""Unit v5625, the dynamic mesh of a ride's hoardings, against libtpw_unit_GenerateHoardingMeshes.dylib
(tests/unit_GenerateHoardingMeshes.cpp), main app ("SimTheme Park"), src/LevelSystems_GenerateHoardingMeshes.cpp:
  GenerateHoardingMeshes 0x1005fa70

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back, by the Mac's
offsets on the PowerPC side and by the host's (tpw_ghm_layout) on the host. The callees outside the unit are fakes on both sides:
MReleaseAllocF (records size and flags, hands out a zeroed block), HoardingCentreAngle (records the cell, side and the two halves, answers
a float from its queue), FloatSortByArray (records count, the two tables, depth, offset and every source entry as its place relative to
the first and the float it points at; writes a permutation of the sources, reversed or rotated, chosen by its queue), DMCreateMesh
(records the set and entry as allocation numbers, the textures, the flags, the point count, the name and the textures' folder text; hands
out the vertex sets, points, UV sets, faces (filled with a pattern, so the faces' kept bit 0 varies), the matrix and the face set),
SetMatrixIdentity (records whether it got DMCreateMesh's matrix) and DetermineInnerBaseInsets (records the count, the cell's corners and
every transformed point; answers the eight inset floats from its queue). sprintf is the standard stub on the PowerPC side, snprintf on
the host.

The world of a case: the ride set (flags, its mesh with the head count at +0xc and the root node at +0x2c, two old words at +0xb0 and
+0xb4), the root node (matrix, vertex count, vertex sets), the shape (width, height, 20 x 20 cells of random bytes), the theme text and
gRideHoardingTextures' textures word. What is compared: the event log; the ride set's flags, +0xb0 and +0xb4 (as labels); every block the
fakes handed out as the function fills it (the set's header words, the entry's counts and box, every vertex set, point, UV set and face,
the face set, the corner block, the geometry block); the first count entries of the three sort tables (the pointers relative to the
first source); whether the textures' folder was set.

The fused operations: the node transform (fmadds), hooked by HardwareRounding over its range; the box's two fmadds are exact for the
shapes the table holds. Not tested: shapes wider or taller than 20 cells (they overrun the original's stack tables), a FloatSortByArray
that does not answer a permutation (the original then reads its index table unset), more than 1024 node vertices (the point table).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GenerateHoardingMeshes"
MASK = 0xFFFFFFFF

FN = 0x1005fa70
SIZES = {FN: 4100}
OWN = set(SIZES)
TRANSFORM = (0x1005ffb8, 0x10060094)   # the node transform's fmadds

HOARDING_CENTRE_ANGLE = 0x1005f8c4
DETERMINE_INNER_BASE_INSETS = 0x1005f738
FLOAT_SORT_BY_ARRAY = 0x100c2d50
MRELEASE_ALLOCF = 0x10064bd4
DM_CREATE_MESH = 0x100301c0
SET_MATRIX_IDENTITY = 0x100c0e28

ADDR = dict(gRideHoardingTextures=0x1029cf58, gHoardingSortScratch=0x102c2150, gHoardingSortSource=0x102c3a50,
            gHoardingSortOrder=0x102c5350)

LAYOUT_KEYS = ["anim.flags", "anim.heads", "anim.geom", "anim.dset", "anim.size", "mesh.f0c", "mesh.root", "mesh.size",
               "set.entries", "set.vs", "set.pts", "set.uv", "set.faces", "entry.matrix", "entry.facesets", "geom.block",
               "tex.folder", "tex.textures", "node.matrix", "node.vc", "node.vs", "shape.cells", "entry.box", "entry.pointcount"]
MAC_OFF = {"anim.flags": 4, "anim.heads": 8, "anim.geom": 0xb0, "anim.dset": 0xb4, "anim.size": 0xe0, "mesh.f0c": 0xc,
           "mesh.root": 0x2c, "mesh.size": 0x40, "set.entries": 8, "set.vs": 0x10, "set.pts": 0x14, "set.uv": 0x18, "set.faces": 0x1c,
           "entry.matrix": 0x28, "entry.facesets": 0x2c, "geom.block": 0xc, "tex.folder": 8, "tex.textures": 0x18,
           "node.matrix": 0x10, "node.vc": 0x58, "node.vs": 0x60, "shape.cells": 8, "entry.box": 0x10, "entry.pointcount": 0xc}
SAME_ON_HOST = ["node.matrix", "node.vc", "node.vs", "shape.cells", "entry.box", "entry.pointcount"]
HOST_OFF = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    """HardwareRounding over the node transform only (Unicorn rounds fused operations twice)."""

    def __enter__(self):
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=TRANSFORM[0], end=TRANSFORM[1] - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    L = e.cl
    size, flags = e.r(3) & MASK, e.r(4) & MASK
    L.rec("MReleaseAllocF", (size, flags))
    a = e.alloc(size + 16)
    e.write(a, bytes(size + 16))
    L.allocs.append(a)
    e.set_r(3, a)


def stub_angle(e):
    L = e.cl
    L.rec("HoardingCentreAngle", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2)))))
    e.set_f1(bitsf(L.ret("HoardingCentreAngle")))


def stub_sort(e):
    L = e.cl
    count, src, scr = s32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK
    L.rec("FloatSortByArray", (count, int(src == ADDR["gHoardingSortSource"]), int(scr == ADDR["gHoardingSortScratch"]),
                               s32(e.r(6)), s32(e.r(7))))
    if count <= 0:
        return
    ptrs = [e.u32(src + 4 * i) for i in range(count)]
    for i, p in enumerate(ptrs):
        L.rec("FloatSortValue", (i, (p - ptrs[0]) // 4, canon_f(e.u32(p))))
    mode = s32(L.ret("FloatSortByArray"))
    for i in range(count):
        frm = count - 1 - i if mode < 0 else (i + mode) % count
        e.write(scr + 4 * i, struct.pack(">I", ptrs[frm]))


def stub_dmcreate(e):
    L = e.cl
    st, tex, name, flags = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    entry = e.u32(st + 8)
    n = struct.unpack(">H", e.read(entry + 0xc, 2))[0]
    folder = e.u32(tex + 8)
    idx = lambda p: L.allocs.index(p) if p in L.allocs else -1
    L.rec("DMCreateMesh", (idx(st), int(tex == ADDR["gRideHoardingTextures"]), flags, idx(entry), n),
          (e.cstr(name), e.cstr(folder) if folder else ""))
    seed = L.ret("FacePattern") & MASK
    blocks = []
    for size in (n * 0x30, n * 60, n * 0x20, n * 0x20, 0x40, 0x10):
        a = e.alloc(size + 16)
        e.write(a, bytes(size + 16))
        blocks.append(a)
    e.write(blocks[3], b"".join(struct.pack(">H", (k * 0x9E37 + seed) & 0xFFFF) for k in range(n * 16)))
    e.write(blocks[5] + 4, b"\x77" * 12)
    for off, a in zip((0x10, 0x14, 0x18, 0x1c), blocks[:4]):
        e.write(st + off, struct.pack(">I", a))
    e.write(entry + 0x28, struct.pack(">II", blocks[4], blocks[5]))
    L.dm = blocks
    e.set_r(3, L.ret("DMCreateMesh") & MASK)


def stub_identity(e):
    L = e.cl
    m = e.r(3) & MASK
    L.rec("SetMatrixIdentity", (1 if L.dm and m == L.dm[4] else (0 if m == 0 else -1),))


def stub_insets(e):
    L = e.cl
    corners, pts, count, out = e.r(3) & MASK, e.r(4) & MASK, s32(e.r(5)), e.r(6) & MASK
    cw = struct.unpack(">8I", e.read(corners, 32))
    L.rec("DetermineInnerBaseInsets", (count,) + tuple(canon_f(w) for w in cw))
    for i in range(count):
        p = struct.unpack(">3I", e.read(pts + 12 * i, 12))
        L.rec("DIBIPoint", (i,) + tuple(canon_f(w) for w in p))
    for k in range(8):
        e.write(out + 4 * k, struct.pack(">I", L.ret("Insets") & MASK))


STUBS.internal(MRELEASE_ALLOCF, stub_alloc)
STUBS.internal(HOARDING_CENTRE_ANGLE, stub_angle)
STUBS.internal(FLOAT_SORT_BY_ARRAY, stub_sort)
STUBS.internal(DM_CREATE_MESH, stub_dmcreate)
STUBS.internal(SET_MATRIX_IDENTITY, stub_identity)
STUBS.internal(DETERMINE_INNER_BASE_INSETS, stub_insets)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def u32(self, a):
        return self.rd_words(a, 1)[0]

    def u16s(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def floats(self, a, n):
        return [canon_f(w) for w in self.rd_words(a, n)]

    def ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    OFF = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.allocs = []
        self.L.dm = []
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.write(a, bytes(n))
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ptr32(self, target):
        return target

    def ptr32_get(self, field):
        return self.u32(field)

    def allocs(self):
        return list(self.L.allocs)

    def dm(self):
        return list(self.L.dm)

    def call(self, theme, anim, shape):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [theme, anim, shape])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.OFF = HOST_OFF
        lib.tpw_ghm_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ghm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ghm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr32(self, target):
        return self.lib.tpw_ghm_ptr32(target)

    def ptr32_get(self, field):
        return self.lib.tpw_ghm_ptr32_get(field)

    def allocs(self):
        return [self.lib.tpw_ghm_block(i) for i in range(self.lib.tpw_ghm_block(-1))]

    def dm(self):
        d = [self.lib.tpw_ghm_block(16 + j) for j in range(6)]
        return d if d[0] else []

    def call(self, theme, anim, shape):
        self.lib.tpw_ghm_run(theme, anim, shape)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ghm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "reset").restype, getattr(lib, p + "reset").argtypes = None, []
    getattr(lib, p + "block").restype, getattr(lib, p + "block").argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "ptr32").restype, getattr(lib, p + "ptr32").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "ptr32_get").restype, getattr(lib, p + "ptr32_get").argtypes = C.c_uint64, [C.c_uint64]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(LAYOUT_KEYS), n
    HOST_OFF.update(zip(LAYOUT_KEYS, list(out)[:n]))
    for k in SAME_ON_HOST:   # the file-image layouts the module writes by the Mac's offsets
        assert HOST_OFF[k] == MAC_OFF[k], (k, HOST_OFF[k])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    O = s.OFF
    shape = s.alloc(0xc88)
    s.words(shape, [case["w"], case["h"]])
    s.write(shape + O["shape.cells"], bytes(case["cells"]))
    node = 0
    nd = case["node"]
    if nd is not None:
        vs = s.alloc(max(1, len(nd["verts"]) // 12) * 0x30)
        s.words(vs, nd["verts"])
        node = s.alloc(0xa0)
        s.words(node + O["node.matrix"], nd["matrix"])
        s.w16(node + O["node.vc"], nd["vc"])
        s.words(node + O["node.vs"], [s.ptr32(vs)])
    mesh = s.alloc(O["mesh.size"])
    s.w16(mesh + O["mesh.f0c"], case["heads"])
    s.wptr(mesh + O["mesh.root"], node)
    anim = s.alloc(O["anim.size"])
    old_geom, old_set = s.alloc(16), s.alloc(16)
    s.wptr(anim + O["anim.heads"], mesh)
    s.words(anim + O["anim.flags"], [case["flags"]])
    s.wptr(anim + O["anim.geom"], old_geom)
    s.wptr(anim + O["anim.dset"], old_set)
    s.textures = s.alloc(16)
    tex = s.g("gRideHoardingTextures")
    s.wptr(tex + O["tex.textures"], s.textures)
    s.wptr(tex + O["tex.folder"], 0)
    theme = s.alloc(len(case["theme"]) + 1)
    s.write(theme, case["theme"].encode() + b"\0")
    s.anim = anim
    s.sent = {old_geom: -2, old_set: -3, s.textures: -4}
    return theme, anim, shape


def count_sides(case):
    n = 0
    for y in range(max(0, case["h"])):
        for x in range(max(0, case["w"])):
            n += bin(case["cells"][y * 0xa0 + x * 8 + 4] & 0x55).count("1")
    return n


def observe(s, case):
    O = s.OFF
    allocs = s.allocs()

    def lab(p):
        if p == 0:
            return 0
        if p in s.sent:
            return s.sent[p]
        if p in allocs:
            return 100 + allocs.index(p)
        return -1

    A = s.anim
    out = {"log": s.log(), "flags": s.u32(A + O["anim.flags"]), "geom": lab(s.ptr(A + O["anim.geom"])),
           "dset": lab(s.ptr(A + O["anim.dset"])), "nalloc": len(allocs)}
    tex = s.g("gRideHoardingTextures")
    out["folder_set"] = int(s.ptr(tex + O["tex.folder"]) != 0)
    n = count_sides(case)
    if n:
        src0 = s.ptr(s.g("gHoardingSortSource"))
        out["order"] = [s32(w) for w in s.rd_words(s.g("gHoardingSortOrder"), n)]
        out["source"] = [(s.ptr(s.g("gHoardingSortSource") + s.psize * i) - src0) // 4 for i in range(n)]
        out["sorted"] = [(s.ptr(s.g("gHoardingSortScratch") + s.psize * i) - src0) // 4 for i in range(n)]
    if len(allocs) >= 3:
        st, en, blk = allocs[:3]
        out["set"] = [s.u32(st), s.u16s(st + 4, 1)[0], lab(s.ptr(st + O["set.entries"]))]
        out["entry"] = [s.u32(en)] + s.u16s(en + 4, 6) + s.floats(en + O["entry.box"], 6)
        out["corners"] = s.u16s(blk, 4 * n)
        dm = s.dm()
        if dm:
            out["dm_fields"] = [s.ptr(st + O[k]) == d for k, d in zip(("set.vs", "set.pts", "set.uv", "set.faces"), dm)]
            out["vs"] = s.floats(dm[0], 12 * n)
            out["pts"] = s.floats(dm[1], 15 * n)
            out["uv"] = s.floats(dm[2], 8 * n)
            out["faces"] = s.u16s(dm[3], 16 * n)
            fs = dm[5]
            out["faceset"] = [lab(s.ptr32_get(fs))] + s.u16s(fs + 4, 4) + list(s.read(fs + 0xc, 4))
    if len(allocs) >= 4:
        g = allocs[3]
        out["geometry"] = s.rd_words(g, 3) + [lab(s.ptr(g + O["geom.block"]))]
    return out


def run(s, case):
    try:
        args = build(s, case)
        s.call(*args)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r, special=True):
    k = r.random()
    if special and k < 0.05:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def make_case(r, w=None, h=None, heads=None, node=True, vc=None, density=None, side_bits=None, perm=None, theme=None):
    w = r.choice([1, 2, 3, 4, 5, r.randint(-2, 6)]) if w is None else w
    h = r.choice([1, 2, 3, 4, 5, r.randint(-2, 6)]) if h is None else h
    cells = bytearray(r.getrandbits(8) for _ in range(0xc80))
    density = r.choice([0.0, 0.3, 0.6, 1.0, r.random()]) if density is None else density
    for y in range(max(0, h)):
        for x in range(max(0, w)):
            o = y * 0xa0 + x * 8 + 4
            if side_bits is not None:
                cells[o] = (cells[o] & 0xAA) | side_bits
            else:
                bits = sum(b for b in (1, 4, 0x10, 0x40) if r.random() < density)
                cells[o] = (cells[o] & 0xAA) | bits
    nd = None
    if node:
        vc = r.choice([0, 1, 3, 4, 5, 8, r.randint(0, 12)]) if vc is None else vc
        nsets = max(1, (vc + 3) // 4)
        nd = {"vc": vc, "verts": [rf(r) for _ in range(12 * nsets)], "matrix": [rf(r) for _ in range(16)]}
    if theme is None:
        theme = r.choice(["", "Jungle", "Space", "LostKingdom", "x" * r.randint(1, 129)])   # the log keeps 159 characters of a text
    if perm is None:
        perm = r.choice([-1, 0, 1, r.randrange(0, 60)])
    queues = {"HoardingCentreAngle": [rf(r) for _ in range(r.randint(1, 12))], "FloatSortByArray": [perm],
              "Insets": [rf(r) for _ in range(r.randint(8, 24))], "FacePattern": [r.getrandbits(16)],
              "DMCreateMesh": [r.choice([0, 1, -1])]}
    return {"w": w, "h": h, "cells": list(cells), "node": nd,
            "heads": r.choice([2, 3, 7, 0xFFFF, 0x8000]) if heads is None else heads,
            "flags": r.getrandbits(32), "theme": theme, "queues": queues}


def edges():
    r = random.Random(5625)
    yield make_case(r, w=2, h=2, heads=0, density=1.0)
    yield make_case(r, w=2, h=2, heads=1, density=1.0)
    yield make_case(r, w=2, h=2, heads=2, density=1.0)
    yield make_case(r, w=2, h=2, heads=0xFFFF, density=1.0)
    yield make_case(r, w=3, h=2, node=False, density=1.0)
    yield make_case(r, w=3, h=2, node=False, density=0.0)
    yield make_case(r, w=1, h=1, side_bits=0x55, vc=5)
    yield make_case(r, w=1, h=1, side_bits=0x55, vc=0)
    yield make_case(r, w=1, h=1, side_bits=0x14, vc=3)
    yield make_case(r, w=1, h=2, side_bits=0x55)
    yield make_case(r, w=2, h=1, side_bits=0x55)
    yield make_case(r, w=0, h=3, density=1.0)
    yield make_case(r, w=3, h=0, density=1.0)
    yield make_case(r, w=-1, h=4, density=1.0)
    yield make_case(r, w=4, h=-5, density=1.0)
    yield make_case(r, w=3, h=3, density=0.0, vc=6)
    for bits in (1, 4, 0x10, 0x40, 0x05, 0x14, 0x50, 0x41, 0x11, 0x44, 0x55, 0x00):
        for perm in (-1, 0, 3):
            yield make_case(r, w=3, h=2, side_bits=bits, perm=perm)
    for vc in (0, 1, 2, 3, 4, 5, 8, 12):
        yield make_case(r, w=2, h=3, density=0.5, vc=vc)
    yield make_case(r, w=20, h=1, density=1.0, vc=1)
    yield make_case(r, w=1, h=20, density=1.0, vc=1)
    yield make_case(r, w=20, h=20, density=0.25, vc=1, perm=-1)
    yield make_case(r, w=20, h=20, density=0.25, vc=0, perm=7)
    yield make_case(r, w=2, h=2, density=1.0, theme="")
    yield make_case(r, w=2, h=2, density=1.0, theme="T" * 129)


def random_case(r):
    return make_case(r)


TESTS = [
    FnTest("GenerateHoardingMeshes", ppc, host, edge=edges, rnd=random_case),
]
