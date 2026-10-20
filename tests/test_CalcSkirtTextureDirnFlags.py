"""Unit v5042, CalcSkirtTextureDirnFlags, against libtpw_unit_CalcSkirtTextureDirnFlags.dylib (tests/unit_CalcSkirtTextureDirnFlags.cpp), main app
("SimTheme Park"), src/Debug_blendflags_CalcSkirtTextureDirnFlags.cpp:
  CalcSkirtTextureDirnFlags 0x1006eca0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: LLCC_GetRideBlendInfo (answers the case's blend table), MReleaseAllocF (a
zeroed block), GetNodeWorldVertPosnsIntoGeomBuffer, StripRideWrapping, ERRORBox, CTRACE. The float to integer glue at 0x10237cd8 is a
truncation on both sides (the host calls an inline cast).

The world of a case: a ride set and mesh (native pointers: 4 bytes on the Mac, 8 on the host), a header, a node with face sets, faces, UVs and
a vertex index table, a geometry buffer with the triangles' points, a height field header, a shape, an optional blend table, and the
cleared 20 x 20 skirt table. What is compared: the mesh flag word, the skirt output halfwords, the texture flag halfwords, the whole skirt
table, the current cell global and the event log.

No fused operations. Not tested: a width or height of 0 (a zero size allocation), a width or height above 20, cell indexes outside the
table (the original writes past it), NaN or infinite coordinates (the processors make different NaNs and the glue's saturation is unknown).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CalcSkirtTextureDirnFlags"
MASK = 0xFFFFFFFF

FN = 0x1006eca0
CALLEES = dict(blend=0x1008f574, alloc=0x10064bd4, nodepos=0x10068dec, strip=0x101ee150, errbox=0x1006fd3c, ctrace=0x1006fcbc)
CVT = 0x10237cd8

ADDR = dict(cur=0x102d1fa8, cells=0x102d1fac, geom=0x102c7990, hfh=0x102c6c7c)
HOST_SIZES = [0x54, 0x8340]
CELLS_BYTES = 0x8340
# the Mac's offsets (set, mesh); the host's come from the layout
MAC = dict(heads=8, mf00=0, header=4, root=0x2c, skirt=0x18)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.internal(CALLEES["blend"], CL.recorder("LLCC_GetRideBlendInfo", "L", ret=lambda e, log: log.blend))
STUBS.internal(CALLEES["alloc"], CL.recorder("MReleaseAllocF", "uu", ret=lambda e, log: e.alloc(e.r(3))))
STUBS.internal(CALLEES["nodepos"], CL.recorder("GetNodeWorldVertPosnsIntoGeomBuffer", "L"))
STUBS.internal(CALLEES["strip"], CL.recorder("StripRideWrapping", "LLfLL"))
STUBS.internal(CALLEES["errbox"], CL.recorder("ERRORBox", "ssuuuuuu"))
STUBS.internal(CALLEES["ctrace"], CL.recorder("CTRACE", "usii"))


def stub_cvt(e):
    e.set_r(3, int(math.trunc(e.f(1))) & MASK)


STUBS.internal(CVT, stub_cvt)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def pk(self, buf, off, v):
        struct.pack_into(self.fmt, buf, off, v & MASK)

    def pk16(self, buf, off, v):
        struct.pack_into(self.fmt[0] + "H", buf, off, v & 0xFFFF)

    def pkf(self, buf, off, x):
        self.pk(buf, off, fb(x))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    ptrfmt = ">I"
    off = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = None
        self.saved = None

    def start(self, fixed):
        self.L = self.e.cl = CL.PpcLog({a: n for n, a in fixed.items()}, {})
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def p32(self, a):
        return a & MASK

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_blend(self, a):
        self.L.blend = a

    def call(self, case, s, shape):
        T.call_ppc(self.e, FN, [s, shape, case["w"] & MASK, case["h"] & MASK, case["force"] & MASK])


class Host(Side):
    fmt = "<I"
    ptrfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.offs

    def start(self, fixed):
        CL.host_begin(self.lib, {n: a for n, a in fixed.items()}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_skirt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_skirt_alloc(n)

    def p32(self, a):
        return self.lib.tpw_skirt_off(a) if a else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_blend(self, a):
        C.memmove(self.g("blend"), struct.pack("<Q", a), 8)

    def call(self, case, s, shape):
        self.lib.tpw_skirt_call(s, shape, case["w"], case["h"], case["force"])


def put_ptr(s, a, v):
    s.write(a, struct.pack(s.ptrfmt, v))


def get_ptr(s, a):
    return struct.unpack(s.ptrfmt, s.read(a, struct.calcsize(s.ptrfmt)))[0]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_skirt_addr.restype, lib.tpw_skirt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_skirt_alloc.restype, lib.tpw_skirt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_skirt_off.restype, lib.tpw_skirt_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_skirt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_skirt_call.restype, lib.tpw_skirt_call.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_skirt_layout(out)
    v = list(out)[:n]
    assert v[5:] == HOST_SIZES, v
    lib.offs = dict(heads=v[0], mf00=v[1], header=v[2], root=v[3], skirt=v[4])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    """Blocks written with the Mac's offsets for the file images; the set and mesh with the side's own."""
    nslots = 3 * len(c["faces"])
    nf = len(c["faces"])
    ntex = len(c["tex"])
    nsets = len(c["sets"])
    set_a = s.alloc(0x200)
    mesh = s.alloc(0x100)
    header = s.alloc(0xc0)
    hfh = s.alloc(0xc0)
    hfield = s.alloc(0x40)
    nodeN = s.alloc(0xa0)
    nodeB = s.alloc(0xa0) if not c["root_null"] else nodeN
    tex = s.alloc(8 * ntex + 8)
    fsets = s.alloc(0x10 * nsets + 0x10)
    faces = s.alloc(8 * nf + 8)
    nuv = (nslots + 3) // 4
    uvs = s.alloc(0x20 * nuv + 0x20)
    f94 = s.alloc(2 * nslots + 8)
    geom = s.alloc(12 * nslots + 12)
    shape = s.alloc(0xc90)
    blend = s.alloc(512) if c["blend"] is not None else 0
    fixed = {1: set_a, 2: shape, 3: nodeN, 5: geom}
    if blend:
        fixed[4] = blend
    s.start(fixed)
    off = s.off

    # clear the globals
    s.write(s.g("cells"), bytes(CELLS_BYTES))
    put_ptr(s, s.g("cur"), 0)
    put_ptr(s, s.g("geom"), geom)
    put_ptr(s, s.g("hfh"), hfh)
    s.set_blend(blend)
    if blend:
        s.write(blend, bytes(c["blend"]))

    # set and mesh (native pointers)
    put_ptr(s, set_a + off["heads"], mesh)
    b = bytearray(0x100)
    s.pk(b, off["mf00"], c["mesh_flags"])
    s.write(mesh, bytes(b))
    put_ptr(s, mesh + off["header"], header)
    put_ptr(s, mesh + off["root"], 0 if c["root_null"] else nodeN)

    # header: name 0x18, textures 0x50, hfield 0x6c, root 0x78
    b = bytearray(0xc0)
    b[0x18:0x18 + len(c["name"])] = c["name"].encode()
    s.pk(b, 0x50, (s.p32(tex) + c["tex_shift"] * 8))
    s.pk(b, 0x78, s.p32(nodeB))
    s.write(header, bytes(b))
    b = bytearray(0xc0)
    s.pk(b, 0x6c, s.p32(hfield))
    s.write(hfh, bytes(b))
    b = bytearray(0x40)
    s.pkf(b, 0x10, c["csx"])
    s.pkf(b, 0x14, c["csz"])
    s.write(hfield, bytes(b))

    # nodes
    b = bytearray(0xa0)
    s.pk16(b, 0x5a, nsets)
    s.pk(b, 0x68, s.p32(uvs))
    s.pk(b, 0x6c, s.p32(fsets))
    s.pk(b, 0x70, s.p32(faces))
    s.pk(b, 0x94, s.p32(f94))
    for i in range(3):
        s.pkf(b, 0x40 + 4 * i, c["rootpos"][i])
    s.write(nodeN, bytes(b))
    if nodeB != nodeN:
        b = bytearray(0xa0)
        for i in range(3):
            s.pkf(b, 0x40 + 4 * i, c["rootpos"][i])
        s.write(nodeB, bytes(b))

    # textures, face sets, faces
    b = bytearray(8 * ntex + 8)
    for i, fl in enumerate(c["tex"]):
        s.pk16(b, 8 * i, fl)
    s.write(tex, bytes(b))
    b = bytearray(0x10 * nsets + 0x10)
    for i, (t, first, cnt) in enumerate(c["sets"]):
        s.pk(b, 0x10 * i, s.p32(tex + 8 * t))
        s.pk16(b, 0x10 * i + 4, first)
        s.pk16(b, 0x10 * i + 6, cnt)
    s.write(fsets, bytes(b))
    b = bytearray(8 * nf + 8)
    pts = bytearray(12 * nslots + 12)
    t94 = bytearray(2 * nslots + 8)
    for i, f in enumerate(c["faces"]):
        for k in range(3):
            slot = 3 * i + k
            s.pk16(b, 8 * i + 2 + 2 * k, slot)
            g = c["perm"][slot]
            s.pk16(t94, 2 * slot, g)
            for j in range(3):
                s.pkf(pts, 12 * g + 4 * j, f[k][j])
    s.write(faces, bytes(b))
    s.write(f94, bytes(t94))
    s.write(geom, bytes(pts))
    b = bytearray(0x20 * nuv + 0x20)
    for slot in range(nslots):
        u, v = c["uv"][slot]
        s.pkf(b, 0x20 * (slot // 4) + 4 * (slot % 4), u)
        s.pkf(b, 0x20 * (slot // 4) + 0x10 + 4 * (slot % 4), v)
    s.write(uvs, bytes(b))
    # shape: mCells[20][20] of 8 bytes at 8
    b = bytearray(0xc90)
    for r in range(c["h"]):
        for q in range(c["w"]):
            s.pk(b, 8 + 0xa0 * r + 8 * q, c["cellvals"][r][q])
    s.write(shape, bytes(b))
    return set_a, shape, mesh, tex


def observe(s, c, mesh, tex):
    off = s.off
    skirt = get_ptr(s, mesh + off["skirt"])
    cur = get_ptr(s, s.g("cur"))
    cells = s.g("cells")
    n = c["w"] * c["h"]
    return {
        "log": s.log(),
        "flags": s.rd_words(mesh + off["mf00"], 1),
        "skirt": s.rd_halves(skirt, 2 * n) if skirt else None,
        "tex": [s.rd_halves(tex + 8 * i, 1)[0] for i in range(len(c["tex"]))],
        "cells": s.rd_words(cells, CELLS_BYTES // 4),
        "cur": (cur - cells) if cur else -1,
    }


def run(s, c):
    try:
        set_a, shape, mesh, tex = build(s, c)
        s.call(c, set_a, shape)
        return observe(s, c, mesh, tex)
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------------------- case generators


def quant(r):
    k = r.random()
    if k < 0.75:
        return r.randrange(0, 9) / 8.0
    if k < 0.9:
        return f32(r.uniform(0, 1))
    return f32(r.uniform(-1000, 1000))


def make_tri(r, cx, cz, csx, csz, skirt):
    if skirt:
        dx = 0.25 * csx * r.choice([-1, 1])
        dz = 0.25 * csz * r.choice([-1, 1])
        ny = r.choice([0.0, 0.0, 0.05, -0.05, 0.4]) * abs(dx)
        ny2 = r.choice([0.0, 0.0, 0.05, -0.05, 0.4]) * abs(dz)
        a = ((cx + 0.3) * csx, r.uniform(-2, 2), (cz + 0.3) * csz)
        b = (a[0] + dx, a[1] + ny, a[2] + r.choice([0, 0.01 * csz]))
        cc = (b[0] + r.choice([0, 0.01 * csx]), b[1] + ny2, b[2] + dz)
        pts = [a, b, cc]
        k = r.randrange(3)
        pts = pts[k:] + pts[:k]
        if r.random() < 0.5:
            pts.reverse()
    else:
        pts = [((cx + r.uniform(0.05, 0.95)) * csx, r.uniform(-3, 3), (cz + r.uniform(0.05, 0.95)) * csz) for _ in range(3)]
    return [tuple(f32(v) for v in p) for p in pts]


def make_case(r, force=None, use_blend=None, root_null=None, w=None, h=None, hole=None):
    w = r.randrange(1, 6) if w is None else w
    h = r.randrange(1, 6) if h is None else h
    c = {"w": w, "h": h}
    c["force"] = r.choice([0, 0, 1, 7]) if force is None else force
    c["root_null"] = r.random() < 0.4 if root_null is None else root_null
    c["mesh_flags"] = r.choice([0, 0x40, r.getrandbits(32)])
    c["name"] = "ride%d" % r.randrange(1000)
    c["rootpos"] = [f32(r.uniform(-50, 50)) for _ in range(3)]
    c["csx"] = r.choice([1.0, 2.0, 2.5, 0.5, 4.0, 3.0])
    c["csz"] = r.choice([1.0, 2.0, 2.5, 0.5, 4.0, 3.0])
    c["tex_shift"] = r.randrange(-2, 4)
    has_blend = r.random() < 0.6 if use_blend is None else use_blend
    c["blend"] = [(0 if r.random() < 0.4 else r.randrange(1, 256)) for _ in range(400)] if has_blend else None
    c["cellvals"] = [[(0 if r.random() < 0.3 else r.randrange(1, 9)) for _ in range(w)] for _ in range(h)]
    ntex = r.randrange(1, 5)
    c["tex"] = [r.getrandbits(16) for _ in range(ntex)]
    nfaces = r.randrange(0, 25) if hole is None else hole
    faces = []
    for i in range(nfaces):
        if faces and r.random() < 0.15:
            faces.append(faces[-1])
            continue
        if r.random() < 0.7:
            cx = r.randrange(0, w + 1) if r.random() < 0.9 else r.randrange(0, 20)
            cz = r.randrange(0, h + 1) if r.random() < 0.9 else r.randrange(0, 20)
        else:
            cx, cz = r.randrange(0, min(w, 3)), r.randrange(0, min(h, 3))
        faces.append(make_tri(r, cx, cz, c["csx"], c["csz"], r.random() < 0.8))
    c["faces"] = faces
    nsets = r.randrange(1, 4)
    sets = []
    pos = 0
    for i in range(nsets):
        cnt = (len(faces) - pos) if i == nsets - 1 else r.randrange(0, len(faces) - pos + 1)
        sets.append((r.randrange(ntex), pos, cnt))
        pos += cnt
    c["sets"] = sets
    nslots = 3 * len(faces)
    perm = list(range(nslots))
    r.shuffle(perm)
    c["perm"] = perm
    c["uv"] = [(quant(r), quant(r)) for _ in range(nslots)]
    return c


def edges():
    seed = 4242
    for force in (0, 1):
        for blend in (False, True):
            for root_null in (False, True):
                seed += 1
                yield make_case(random.Random(seed), force=force, use_blend=blend, root_null=root_null, w=3, h=3, hole=12)
    yield make_case(random.Random(7001), force=0, use_blend=False, w=1, h=1, hole=6)
    yield make_case(random.Random(7002), force=1, use_blend=False, w=20, h=20, hole=40)
    yield make_case(random.Random(7003), force=0, use_blend=True, w=5, h=4, hole=0)
    for i in range(20):
        yield make_case(random.Random(8000 + i), force=1, use_blend=False, w=2, h=2, hole=16)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CalcSkirtTextureDirnFlags", ppc, host, edge=edges, rnd=rnd),
]
