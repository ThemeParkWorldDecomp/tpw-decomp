"""Unit v5213, the ride preview, against libtpw_unit_CreateRidePreview.dylib (tests/unit_CreateRidePreview.cpp), main app
("SimTheme Park"), src/game/rides/RidePreview.cpp:
  CreateRidePreview 0x1006c3bc, DetermineRidePreviewScale 0x1006c198

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (InstanceRide, _stricmp, ScaleMesh, SetMatrixIdentity, SetRideMobile, SetMeshInUISpace, EnableMeshDraw, SetCurRideAnim,
DistanceBetweenPoints) are fakes on both sides that record their arguments (meshes and animation sets by a marker word the case writes;
strings by their length and a weighted sum of their bytes); fmod and sqrt are the C library's on both sides.

The world of a case: gRideTypes and gRideAnimSets (native pointers: 4 bytes on the Mac, 8 on the host, with the host's offsets), the
type's and the instance's mesh headers (the file's layout on both sides: texture infos with flags and names, textures, nodes with face
sets, the root node's matrix words), the ride's box, the preview block with random words. What is compared: the answer, the whole
preview block (texture pointers as the textures' markers), every face set's texture marker, the event log.

DetermineRidePreviewScale's fused multiply-add is hooked with ROUNDING. Not tested: the set's mMatrix (a pointer to the callee's
stack), a NULL ride set (read unconditionally), the type set's mesh when mFD0 is NULL (not read).
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

SHIM_LIBRARY = "unit_CreateRidePreview"
MASK = 0xFFFFFFFF

FN_CREATE, FN_DET = 0x1006C3BC, 0x1006C198
SIZES = {FN_CREATE: 1412, FN_DET: 476}
OWN = set(SIZES)

STRICMP = 0x10238368
INSTANCE_RIDE, SCALE_MESH, SET_MATRIX_IDENTITY, SET_RIDE_MOBILE = 0x10069B24, 0x100655D8, 0x100C0E28, 0x10068280
SET_MESH_UI, ENABLE_MESH_DRAW, SET_CUR_RIDE_ANIM, DISTANCE = 0x1006553C, 0x10065200, 0x100C6670, 0x100C3198

ADDR = dict(sets=0x102C79A4, types=0x102D19B0, preview=0x102D1F20)
# the layout export: [0..6] the host's native offsets, [7:] sizes and file-layout offsets that must be the Mac's
HOST_FIXED = [0x88, 0x10, 0x10, 0xA0, 8, 0x36, 0x44, 0x50, 0x54, 0x70, 0x78, 0x5A, 0x6C, 0x10, 0xC, 0x74]
MAC = dict(flags=4, heads=8, fd0=0xD0, hmap=0xCC, setsize=0x100, mhdr=4, meshsize=0x40)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stack_u32(e, off):
    return e.u32((e.r(1) + off) & MASK)


def bits_at(e, a):
    return canon_f(e.u32(a))


def marker(e, a, off=0):
    return e.u32((a + off) & MASK) if a else 0


def stub_stricmp(e):
    L = e.cl
    a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
    a = a if isinstance(a, bytes) else a.encode("latin1")
    b = b if isinstance(b, bytes) else b.encode("latin1")
    wa = sum((i + 1) * c for i, c in enumerate(a))
    wb = sum((i + 1) * c for i, c in enumerate(b))
    L.rec("stricmp", (len(a), wa, len(b), wb))
    la, lb = a.lower(), b.lower()
    e.set_r(3, 0 if la == lb else (1 if la > lb else MASK))
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))      # the glue saved the TOC pointer here


def stub_instance_ride(e):
    L = e.cl
    L.rec("InstanceRide", (e.r(3) & MASK, e.r(4) & MASK, L.lab(e.r(5)), e.r(6) & MASK, e.r(7) & MASK, L.lab(e.r(8)), e.r(9) & MASK,
                           e.r(10) & MASK, stack_u32(e, 0x38), stack_u32(e, 0x3C)))
    e.set_r(3, L.ret("InstanceRide") & MASK)


def stub_scale_mesh(e):
    p = e.r(4)
    e.cl.rec("ScaleMesh", (marker(e, e.r(3)), bits_at(e, p), bits_at(e, p + 4), bits_at(e, p + 8)))


def stub_identity(e):
    e.cl.rec("SetMatrixIdentity", ())
    e.write(e.r(3), b"".join(struct.pack(">f", 1.0 if i % 5 == 0 else 0.0) for i in range(16)))


def stub_set_mobile(e):
    e.cl.rec("SetRideMobile", (e.u32(e.r(3)),))


def stub_set_mesh_ui(e):
    e.cl.rec("SetMeshInUISpace", (marker(e, e.u32(e.r(3))),))


def stub_enable(e):
    m = e.r(4)
    w = [bits_at(e, m + 4 * i) for i in range(16)]
    e.cl.rec("EnableMeshDraw", (marker(e, e.r(3)), e.r(5) & MASK) + tuple(w[:8]))
    e.cl.rec("EnableMeshDraw2", tuple(w[8:]))


def stub_set_anim(e):
    e.cl.rec("SetCurRideAnim", (marker(e, e.r(3), 4), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, canon_f(fb(e.f(1))), e.r(8) & MASK))


def stub_distance(e):
    a, b = e.r(3), e.r(4)
    e.cl.rec("DistanceBetweenPoints", tuple(bits_at(e, a + 4 * i) for i in range(3)) + tuple(bits_at(e, b + 4 * i) for i in range(3)))
    e.set_f1(bitsf(e.cl.ret("Distance")))


def fmod(e):
    x, y = e.f(1), e.f(2)
    try:
        v = math.fmod(x, y)
    except ValueError:
        v = float("nan")
    e.set_f1(v)


STUBS.internal(STRICMP, stub_stricmp)
STUBS.internal(INSTANCE_RIDE, stub_instance_ride)
STUBS.internal(SCALE_MESH, stub_scale_mesh)
STUBS.internal(SET_MATRIX_IDENTITY, stub_identity)
STUBS.internal(SET_RIDE_MOBILE, stub_set_mobile)
STUBS.internal(SET_MESH_UI, stub_set_mesh_ui)
STUBS.internal(ENABLE_MESH_DRAW, stub_enable)
STUBS.internal(SET_CUR_RIDE_ANIM, stub_set_anim)
STUBS.internal(DISTANCE, stub_distance)
STUBS.imp("fmod", fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def wp(self, a, v):
        """A native pointer: 4 bytes on the Mac, 8 on the host."""
        self.write(a, struct.pack(self.pfmt, v))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PS = 4
    O = MAC

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

    def to32(self, a):
        return a

    def from32(self, w):
        return w

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_create(self, x, y, z, w, h, ident):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_CREATE, [T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), T.F(bitsf(w)), T.F(bitsf(h)), ident])
        return r3 & MASK

    def call_det(self, ride, w, h, out):
        a = [out + 4 * i for i in range(8)]
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_DET, [ride, T.F(bitsf(w)), T.F(bitsf(h)), a[0], 0, a[1], a[2], a[3], a[4], a[5], a[6], a[7]])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PS = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.hostO
        self.base = lib.tpw_cr_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cr_alloc(n)

    def to32(self, a):
        return a - self.base if a else 0

    def from32(self, w):
        return w + self.base if w else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_create(self, x, y, z, w, h, ident):
        r = self.lib.tpw_cr_create(bitsf(x), bitsf(y), bitsf(z), bitsf(w), bitsf(h), ident & MASK)
        return r

    def call_det(self, ride, w, h, out):
        self.lib.tpw_cr_determine(ride & MASK, bitsf(w), bitsf(h), out)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cr_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype, getattr(lib, p + "base").argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cr_create.restype = C.c_uint64
    lib.tpw_cr_create.argtypes = [C.c_float] * 5 + [C.c_uint32]
    lib.tpw_cr_determine.restype = None
    lib.tpw_cr_determine.argtypes = [C.c_uint32, C.c_float, C.c_float, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cr_layout(out)
    lay = list(out)[:n]
    assert lay[7:] == HOST_FIXED + [] or lay[7:7 + len(HOST_FIXED)] == HOST_FIXED, lay
    lib.hostO = dict(flags=lay[0], heads=lay[1], fd0=lay[2], hmap=lay[3], setsize=lay[4], mhdr=lay[5], meshsize=lay[6])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

NAMES = ["sign1.tga", "SIGN1.TGA", "Sign2.TGA", "sign2.tga", "other.tga", "", "sign3.tga"]
FLAGS = [0, 1, 2, 8, 0xA, 0x10, 0x12, 0x1A, 3, 0xB]


def build_header(s, spec, tagbase):
    """A mesh header (the file's layout): texture infos, textures, nodes with face sets, the root node. Returns its address and its textures."""
    n = len(spec["tex"])
    hdr = s.alloc(0xC0)
    s.half(hdr + 0x36, n)
    texs = s.alloc(max(8 * n, 8))
    infos = s.alloc(max(16 * n, 16))
    textures = []
    for i, (flags, name) in enumerate(spec["tex"]):
        s.half(texs + 8 * i, tagbase + i)
        textures.append(texs + 8 * i)
        nm = s.alloc(len(name) + 1)
        s.write(nm, name.encode() + b"\0")
        s.words(infos + 16 * i, [flags])
        s.words(infos + 16 * i + 12, [s.to32(nm)])
    s.words(hdr + 0x50, [s.to32(texs), s.to32(infos)])
    nodes_spec = spec.get("nodes", [])
    s.half(hdr + 0x44, len(nodes_spec))
    nodes = s.alloc(max(0xA0 * len(nodes_spec), 0xA0))
    ext = spec.get("ext")
    fsets = []
    for k, refs in enumerate(nodes_spec):
        na = nodes + 0xA0 * k
        fa = s.alloc(max(16 * len(refs), 16))
        s.half(na + 0x5A, len(refs))
        s.words(na + 0x6C, [s.to32(fa)])
        for j, ref in enumerate(refs):
            t = 0 if ref == -1 else (ext if ref == -2 else textures[ref])
            s.words(fa + 16 * j, [s.to32(t)])
        fsets.append((fa, len(refs)))
    s.words(hdr + 0x70, [s.to32(nodes)])
    if "mf78" in spec:
        root = s.alloc(0xA0)
        s.words(root + 0x10, spec["mf78"])
        s.words(hdr + 0x78, [s.to32(root)])
    return hdr, textures, fsets


def build_mesh(s, hdr, ident):
    m = s.alloc(s.O["meshsize"])
    s.words(m, [ident])
    s.wp(m + s.O["mhdr"], hdr)
    return m


def build_set(s, mesh, ident, fd0, box):
    a = s.alloc(s.O["setsize"])
    s.words(a + s.O["flags"], [ident])
    s.wp(a + s.O["heads"], mesh)
    s.wp(a + s.O["fd0"], fd0 if fd0 == 0 else a)
    if box is not None:
        b = s.alloc(0x40)
        s.words(b + 0x18, box)
        s.wp(a + s.O["hmap"], b)
    return a


def build(s, case):
    sets, types = s.g("sets"), s.g("types")
    for i in range(8):
        s.wp(sets + s.PS * i, 0)
        s.wp(types + s.PS * i, 0)
    ext = s.alloc(8)
    s.half(ext, 0x300)
    case_ext = ext
    prev = list(case["preview"])
    for k in range(4):
        prev[29 + k] = s.to32(ext) if case["prev_ext"][k] else 0
    s.words(s.g("preview"), prev)
    s.state = {"fsets": [], "ext": case_ext}
    if case["op"] == "create":
        thdr, _, _ = build_header(s, case["type"], 0x100)
        tmesh = build_mesh(s, thdr, 0x10) if case["fd0"] else 0
        tset = build_set(s, tmesh, 0x1, case["fd0"], None)
        s.wp(types + s.PS * case["id"], tset)
        spec = dict(case["inst"], ext=ext)
        hdr, _, fsets = build_header(s, spec, 0x200)
        s.state["fsets"] = fsets
        mesh = build_mesh(s, hdr, 0x20)
        iset = build_set(s, mesh, 0x2, 1, case["box"])
        s.wp(sets + s.PS * case["inst_no"], iset)
    else:
        iset = build_set(s, 0, 0x2, 1, case["box"])
        s.wp(sets + s.PS * case["ride"], iset)
        s.state["out"] = s.alloc(32)
        s.words(s.state["out"], [0xDEADBEEF] * 8)


def tex_id(s, w):
    a = s.from32(w)
    return s.rd_half(a) if a else 0


def observe(s, case, answer):
    log = s.log()
    if case["op"] == "create":
        words = s.rd_words(s.g("preview"), 34)
        shown = [canon_f(w) for w in words]
        for k in range(4):
            shown[29 + k] = tex_id(s, words[29 + k])
        fs = []
        for fa, cnt in s.state["fsets"]:
            fs.append([tex_id(s, s.rd_word_at(fa + 16 * j)) for j in range(cnt)])
        return {"answer": answer == s.g("preview"), "log": log, "preview": shown, "fsets": fs}
    out = [canon_f(w) for w in s.rd_words(s.state["out"], 8)]
    return {"log": log, "out": out}


Side.rd_word_at = lambda self, a: self.rd_words(a, 1)[0]


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "create":
            ans = s.call_create(*case["args"])
        else:
            ans = s.call_det(case["ride"], case["w"], case["h"], s.state["out"])
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.05:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rdist(r):
    k = r.random()
    if k < 0.06:
        return r.choice([0, 0x7F800000, 0x7FC00000, 0x80000000])
    return fb(r.uniform(0.01, 20))


def gen_tex(r, good):
    n = r.randrange(2, 6) if good else r.randrange(0, 5)
    tex = [(r.choice(FLAGS), r.choice(NAMES)) for _ in range(n)]
    if good:
        i, j = r.sample(range(n), 2)
        tex[i] = (r.choice([0, 1, 4, 0xA, 0x1A, 8]), r.choice(["sign1.tga", "SIGN1.TGA", "other.tga"]))
        if tex[i][1] == "other.tga":
            tex[i] = (0xA, "other.tga")
        tex[j] = (r.choice([0, 1, 0x12, 0x1A]), r.choice(["sign2.tga", "Sign2.TGA", "other.tga"]))
        if tex[j][1] == "other.tga":
            tex[j] = (0x12, "other.tga")
    return tex


def gen_nodes(r, n):
    nodes = []
    for _ in range(r.randrange(0, 3)):
        nodes.append([r.choice([-1, -2] + list(range(n)) * 3) for _ in range(r.randrange(0, 5))])
    return nodes


def create_case(r, good=None, fd0=None, inst_no=None):
    good = r.random() < 0.6 if good is None else good
    c = {"op": "create"}
    c["id"] = r.choice([1, 2, 3])
    c["inst_no"] = r.choice([0, 1, 2, 3, 3]) if inst_no is None else inst_no
    c["fd0"] = r.random() < 0.8 if fd0 is None else fd0
    c["type"] = {"tex": gen_tex(r, good)}
    ti = gen_tex(r, good)
    c["inst"] = {"tex": ti, "nodes": gen_nodes(r, len(ti)), "mf78": [r.getrandbits(32) for _ in range(16)]}
    c["box"] = [rf(r) for _ in range(6)] + [r.getrandbits(32) for _ in range(6)]
    c["preview"] = [r.getrandbits(32) for _ in range(34)]
    c["prev_ext"] = [r.random() < 0.5 for _ in range(4)]
    c["args"] = [rf(r) for _ in range(5)] + [c["id"]]
    if r.random() < 0.3:
        c["args"][3] = fb(r.uniform(0.1, 10))
        c["args"][4] = fb(r.uniform(0.1, 10))
    if r.random() < 0.1:
        c["preview"][18] = fb(r.uniform(-30, 30))
    c["queues"] = {"InstanceRide": [c["inst_no"]], "Distance": [rdist(r), rdist(r)]}
    c["preview"][18] = fb(r.uniform(-30, 30)) if r.random() < 0.8 else c["preview"][18]
    return c


def create_edges():
    r = random.Random(5213)
    for good in (True, False):
        for fd0 in (True, False):
            for inst_no in (0, 1, 2):
                for _ in range(2):
                    yield create_case(r, good, fd0, inst_no)


def create_random(r):
    return create_case(r)


def det_case(r):
    c = {"op": "det", "ride": r.choice([1, 2, 3])}
    c["box"] = [rf(r) if r.random() < 0.2 else fb(r.uniform(-20, 20)) for _ in range(6)] + [r.getrandbits(32) for _ in range(6)]
    c["w"] = rf(r) if r.random() < 0.2 else fb(r.uniform(0.1, 10))
    c["h"] = rf(r) if r.random() < 0.2 else fb(r.uniform(0.1, 10))
    c["preview"] = [r.getrandbits(32) for _ in range(34)]
    c["prev_ext"] = [False] * 4
    c["queues"] = {"Distance": [rdist(r), rdist(r)]}
    return c


def det_edges():
    r = random.Random(1006)
    for _ in range(12):
        c = det_case(r)
        yield c
    c = det_case(r)
    c["box"][:6] = [fb(1.0), fb(5.0), fb(-2.0), fb(3.0), fb(7.0), fb(4.0)]
    c["w"], c["h"] = fb(1.5), fb(0.75)
    c["queues"] = {"Distance": [fb(2.0), fb(3.0)]}
    yield c
    for w, h in ((fb(1.0), fb(100.0)), (fb(100.0), fb(1.0)), (0x7FC00000, fb(1.0)), (fb(1.0), 0x7FC00000), (fb(1.0), fb(1.0))):
        c = det_case(r)
        c["w"], c["h"] = w, h
        c["queues"] = {"Distance": [fb(2.0), fb(2.0)]}
        yield c


def det_random(r):
    return det_case(r)


TESTS = [
    FnTest("CreateRidePreview", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("DetermineRidePreviewScale", ppc, host, edge=det_edges, rnd=det_random),
]
