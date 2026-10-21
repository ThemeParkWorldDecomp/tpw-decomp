"""Unit v51021, the ride pointer pick, against libtpw_unit_RayPolyCheckOnRide.dylib (tests/unit_RayPolyCheckOnRide.cpp), main app
("SimTheme Park"), src/game/TPPointer.cpp:
  RayPolyCheckOnRide 0x10065648, FindPointedRide 0x10065b24

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides that record their arguments and answer from the case: CombineMatricesNoWDest2nd (logs hashes of
both matrices, changes nothing), GetNodeWorldVertPosnsIntoGeomBuffer (logs the node's tag and a hash of the matrix; the case writes the
vertex buffer itself), VectorThroughLMatrixNoWNoTrans (logs, then rotates the point (x, y, z) -> (y, z, x)), GetWorldGSquareHeight,
GetPylonFromBraceMesh, GetNthBraceMesh, RecalculateInvalidFaceNormals, SetMatrixIdentity (fills a recognisable pattern). sqrt and acos
are the C library on both sides; modf and fmod are the module's own fakes on the PowerPC side.

The world of a case: a node tree (flags, matrices, face sets, faces, normals, index words), the vertex buffer, the ray origin and
direction, and for FindPointedRide the height field size, three ride sets with meshes, headers and trees. What is compared: the answer,
the call log, the hit point and normal, the pointed ride position and normal, and what the ray pointers were set to.

Fused operations: the PowerPC side runs under HardwareRounding. Not tested: a ride id of 0 from GetNthBraceMesh or an id above 3 (the
original reads a null ride set), a face index outside the case's arrays.
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

SHIM_LIBRARY = "unit_RayPolyCheckOnRide"
MASK = 0xFFFFFFFF

FN_RAY, FN_FIND = 0x10065648, 0x10065B24
SIZES = {FN_RAY: 1184, FN_FIND: 1000}
OWN = set(SIZES)

CALLEES = dict(combine=0x100C28FC, nodeverts=0x101ED17C, vecthrough=0x100C2040, gsq=0x1005F160, pylon=0x100587FC,
               nth=0x10058888, recalc=0x100C7418, setident=0x100C0E28)

ADDR = dict(hitnormal=0x102C6CA8, hitpoint=0x102C6CB4, origin=0x102C6CC0, direction=0x102C6CC4, pointednormal=0x102C6CD8,
            pointedpos=0x102C6CCC, geom=0x102C7990, width=0x102C6C68, height=0x102C6C64, ridesets=0x102C79A4)
# host: sizeof(void*), offsetof(tag_sRideAnimSet, mHeads), offsetof(tag_sMesh, mHeader), then the Mac offsets the module writes
HOST_LAYOUT = [8, 16, 8, 0x10, 0x54, 0x5A, 0x6C, 0x70, 0x94, 16, 8, 0x2C, 0x30, 0x78, 0x84]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def hw(words):
    h = 0
    for w in words:
        h = (h * 31 + w) & MASK
    return h


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mwords(e, a, n=16):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


def stub_combine(e):
    e.cl.rec("Combine", (hw(mwords(e, e.r(3))), hw(mwords(e, e.r(4)))))


def stub_nodeverts(e):
    e.cl.rec("NodeVerts", (e.u32(e.r(3) + 0x54), hw(mwords(e, e.r(4)))))


def stub_vecthrough(e):
    p = e.r(4)
    x, y, z = mwords(e, p, 3)
    e.cl.rec("VecThrough", (hw(mwords(e, e.r(3))), x, y, z))
    e.write(p, struct.pack(">3I", y, z, x))


def stub_gsq(e):
    L = e.cl
    L.rec("GSQHeight", (s32(e.r(3)), s32(e.r(4)), e.u32(e.r(5))))
    e.write(e.r(5), struct.pack(">I", L.ret("GSQExtra") & MASK))
    e.set_f1(bitsf(L.ret("GSQHeight")))


def stub_pylon(e):
    L = e.cl
    L.rec("GetPylon", (e.r(3) & MASK,))
    p = L.ret("Pylon") & MASK
    e.write(e.r(4), struct.pack(">I", L.ret("PylonCount") & MASK))
    e.set_r(3, p)


def stub_nth(e):
    e.cl.rec("GetNth", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, e.cl.ret("NthId") & MASK)


def stub_recalc(e):
    e.cl.rec("Recalc", (e.u32(e.r(3) + 0x2C),))


def stub_setident(e):
    e.cl.rec("SetIdent", ())
    e.write(e.r(3), struct.pack(">16I", *[fb(1.0 + 0.25 * i) for i in range(16)]))


for name, fn in (("combine", stub_combine), ("nodeverts", stub_nodeverts), ("vecthrough", stub_vecthrough), ("gsq", stub_gsq),
                 ("pylon", stub_pylon), ("nth", stub_nth), ("recalc", stub_recalc), ("setident", stub_setident)):
    STUBS.internal(CALLEES[name], fn)


def stub_fmod(e):
    x, y = e.f(1), e.f(2)
    try:
        r = math.fmod(x, y)
    except ValueError:
        r = float("nan")
    e.set_f1(r)


def stub_modf(e):
    frac, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)


STUBS.imp("fmod", stub_fmod)
STUBS.imp("modf", stub_modf)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt16, h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt, fmt16, psz = ">I", ">H", 4
    off_heads, off_header = 8, 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def p32(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(max(n, 16))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_words(a, 1)[0]

    def log(self):
        return self.L.log

    def call_ray(self, node, m):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_RAY, [node] + list(m))
        return r3 & MASK

    def call_find(self, a):
        cx, cz, axis, x, y, z, ys, d, c, step = a
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_FIND, [cx & MASK, cz & MASK, axis & MASK, T.F(x), T.F(y), T.F(z), T.F(ys), d, c, T.F(step)])
        return r3 & MASK


class Host(Side):
    fmt, fmt16, psz = "<I", "<H", 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_rpc_base()
        self.off_heads, self.off_header = lib.offs
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rpc_addr(name.encode())

    def p32(self, a):
        return 0 if a == 0 else a - self.base

    def alloc(self, n):
        return self.lib.tpw_rpc_alloc(max(n, 16))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call_ray(self, node, m):
        arr = (C.c_uint32 * 16)(*m)
        return self.lib.tpw_rpc_ray(node, arr) & MASK

    def call_find(self, a):
        cx, cz, axis, x, y, z, ys, d, c, step = a
        return self.lib.tpw_rpc_find(cx & MASK, cz & MASK, s32(axis), x, y, z, ys, d, c, step)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rpc_addr.restype, lib.tpw_rpc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rpc_alloc.restype, lib.tpw_rpc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rpc_base.restype, lib.tpw_rpc_base.argtypes = C.c_uint64, []
    lib.tpw_rpc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rpc_ray.restype, lib.tpw_rpc_ray.argtypes = C.c_uint32, [C.c_uint64, C.POINTER(C.c_uint32)]
    lib.tpw_rpc_find.restype = C.c_uint64
    lib.tpw_rpc_find.argtypes = [C.c_uint32, C.c_uint32, C.c_int32, C.c_float, C.c_float, C.c_float, C.c_float, C.c_uint64,
                                 C.c_uint64, C.c_float]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rpc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.offs = (out[1], out[2])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mk_node(s, spec):
    a = s.alloc(0xA0)
    s.words(a, [spec["flags"]])
    s.words(a + 0x10, spec["matrix"])
    s.words(a + 0x54, [spec["tag"]])
    pts = s.alloc(12 * len(spec["points"]))
    for i, p in enumerate(spec["points"]):
        s.words(pts + 12 * i, p)
    sets = s.alloc(16 * len(spec["sets"]))
    for i, (first, count) in enumerate(spec["sets"]):
        s.halves(sets + 16 * i + 4, [first, count])
    faces = s.alloc(8 * len(spec["faces"]))
    for i, f in enumerate(spec["faces"]):
        s.halves(faces + 8 * i, f)
    idx = s.alloc(2 * len(spec["idx"]))
    s.halves(idx, spec["idx"])
    s.halves(a + 0x5A, [len(spec["sets"])])
    s.words(a + 0x64, [s.p32(pts)])
    s.words(a + 0x6C, [s.p32(sets)])
    s.words(a + 0x70, [s.p32(faces)])
    s.words(a + 0x94, [s.p32(idx)])
    prev = 0
    for kid in reversed(spec["kids"]):
        k = mk_node(s, kid)
        s.words(k + 8, [s.p32(prev)])
        prev = k
    s.words(a + 0x0C, [s.p32(prev)])
    return a


def build_common(s, case):
    for n in ("hitnormal", "hitpoint", "pointednormal", "pointedpos"):
        s.words(s.g(n), [0, 0, 0])
    s.setptr(s.g("origin"), 0)
    s.setptr(s.g("direction"), 0)
    geom = s.alloc(12 * len(case["geom"]))
    for i, v in enumerate(case["geom"]):
        s.words(geom + 12 * i, v)
    s.setptr(s.g("geom"), geom)
    o = s.alloc(12)
    s.words(o, case["origin"])
    d = s.alloc(12)
    s.words(d, case["dir"])
    s.addr = {"o": o, "d": d}


def build(s, case):
    build_common(s, case)
    if case["op"] == "ray":
        s.setptr(s.g("origin"), s.addr["o"])
        s.setptr(s.g("direction"), s.addr["d"])
        s.addr["tree"] = mk_node(s, case["tree"])
    else:
        s.words(s.g("width"), [case["w"]])
        s.words(s.g("height"), [case["h"]])
        s.addr["rides"] = {}
        for i, ride in enumerate(case["rides"], 1):
            rs = s.alloc(0x40)
            mesh = s.alloc(0x40)
            hdr = s.alloc(0xC0)
            s.setptr(rs + s.off_heads, mesh)
            s.setptr(mesh + s.off_header, hdr)
            root = mk_node(s, ride["tree"])
            s.words(hdr + 0x2C, [i])
            s.words(hdr + 0x30, [ride["hflags"]])
            s.words(hdr + 0x84, [ride["miny"]])
            s.words(hdr + 0x78, [s.p32(root)])
            s.setptr(s.g("ridesets") + s.psz * i, rs)
            s.addr["rides"][rs] = i
        s.setptr(s.g("ridesets"), 0)


def execute(s, case):
    if case["op"] == "ray":
        return s.call_ray(s.addr["tree"], case["matrix"])
    a = case["args"]
    r = s.call_find(a[:7] + [s.addr["d"], s.addr["o"], a[7]])
    return s.addr["rides"].get(r, 0 if r == 0 else -1)


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for n in ("hitnormal", "hitpoint", "pointednormal", "pointedpos"):
        out[n] = [canon_f(w) for w in s.rd_words(s.g(n), 3)]
    for n, k in (("origin", "o"), ("direction", "d")):
        v = s.getptr(s.g(n))
        out[n] = 0 if v == 0 else (1 if v == s.addr[k] else 2)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def fv(r, lo, hi):
    return r.uniform(lo, hi)


def vsub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def unit(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return [c / n for c in v]


def rvec(r, lo, hi):
    return [r.uniform(lo, hi) for _ in range(3)]


def bits3(v):
    return [fb(c) for c in v]


def hit_face(r, O, D):
    """A triangle in a plane through a point of the ray, the point inside it (mostly); returns (stored normal, vertices)."""
    while True:
        n = unit(rvec(r, -1, 1))
        if abs(sum(n[i] * D[i] for i in range(3))) > 0.2 * math.sqrt(sum(c * c for c in D)):
            break
    t = r.uniform(0.5, 30)
    H = [O[i] + t * D[i] for i in range(3)]
    u = unit(cross(n, rvec(r, -1, 1)))
    w = cross(n, u)
    a0 = r.uniform(0, 6.28)
    g1, g2 = r.uniform(1.5, 2.6), r.uniform(1.5, 2.6)
    verts = []
    for a in (a0, a0 + g1, a0 + g1 + g2):
        R = r.uniform(0.5, 4)
        verts.append([H[i] + R * (math.cos(a) * u[i] + math.sin(a) * w[i]) for i in range(3)])
    k = r.uniform(0.5, 3)
    return [n[2] * k, n[0] * k, n[1] * k], verts


def gen_node(r, G, O, D, tags, depth):
    spec = {"flags": r.choice([0, 0, 0, 0, 0, 0x10, 0x200, 0x8000, 0x20, 0x30, 0x1, 0x8201, 0x40]),
            "matrix": [fb(r.uniform(-2, 2)) for _ in range(16)], "tag": tags[0], "points": [], "idx": [], "faces": [], "sets": [],
            "kids": []}
    tags[0] += 1
    for _ in range(r.randrange(0, 5)):
        if r.random() < 0.45:
            nrm, verts = hit_face(r, O, D)
        else:
            nrm, verts = rvec(r, -1, 1), [rvec(r, -6, 6) for _ in range(3)]
        pt = len(spec["points"])
        spec["points"].append(bits3(nrm))
        g = len(G)
        G.extend(bits3(v) for v in verts)
        pos = len(spec["idx"])
        spec["idx"].extend([g, g + 1, g + 2])
        spec["faces"].append(((pt << 1) | r.randrange(2), pos, pos + 1, pos + 2))
    nf = len(spec["faces"])
    for _ in range(r.randrange(0, 4)):
        first = r.randrange(0, nf + 1)
        spec["sets"].append((first, r.randrange(0, nf - first + 1)))
    if depth < 2:
        for _ in range(r.randrange(0, 3)):
            spec["kids"].append(gen_node(r, G, O, D, tags, depth + 1))
    return spec


def ray_case(r):
    O = rvec(r, -20, 20)
    D = unit(rvec(r, -1, 1))
    D = [c * r.uniform(0.5, 2) for c in D]
    G = [[0, 0, 0]]
    tree = gen_node(r, G, O, D, [1], 0)
    return {"op": "ray", "queues": {}, "origin": bits3(O), "dir": bits3(D), "geom": G, "tree": tree,
            "matrix": [fb(r.uniform(-3, 3)) for _ in range(16)]}


def find_case(r):
    O = rvec(r, -20, 20)
    D = [c * r.uniform(0.5, 2) for c in unit(rvec(r, -1, 1))]
    G = [[0, 0, 0]]
    tags = [1]
    rides = []
    for _ in range(3):
        tree = gen_node(r, G, O, D, tags, 1)
        tree["flags"] = r.choice([0, 0, 0, 0x20])
        rides.append({"tree": tree, "hflags": r.choice([0, 0x40000, 0x20000, 0x60000, 0x7FFFF]), "miny": fb(r.uniform(-10, 30))})
    W, H = r.randrange(1, 20), r.randrange(1, 20)
    first = r.choice([0, 1, 1, 2, 3, 3])
    q = {"GSQExtra": [first, 1, 2, 3], "GSQHeight": [fb(r.uniform(-5, 40)) for _ in range(3)],
         "Pylon": [r.choice([0, 0, 0x7000])], "PylonCount": [r.randrange(0, 4)],
         "NthId": [r.randrange(1, 4) for _ in range(3)]}
    if r.random() < 0.3:
        q["GSQHeight"][0] = r.choice([fb(1e9), fb(-1e9), 0x7FC00000])
    x = r.choice([r.uniform(-200, 200), r.uniform(-30, 30), 0.0, 10.0, -10.0, 20.0, r.uniform(0, 50)])
    z = r.choice([r.uniform(-200, 200), r.uniform(-30, 30), 0.0, 10.0, -10.0, 20.0, r.uniform(0, 50)])
    step = r.choice([r.uniform(0.1, 5), r.uniform(-5, -0.1), 0.0, -0.0, float("nan"), 1.0])
    if r.random() < 0.05:
        x = r.choice([float("inf"), float("nan")])
    args = [r.randrange(-3, W + 3), r.randrange(-3, H + 3), r.choice([0, 1, 1, -1]), x, r.uniform(-5, 40), z,
            r.choice([r.uniform(-5, 5), 0.0, 3.0]), step]
    args = [T.f32(a) if isinstance(a, float) else a for a in args]
    return {"op": "find", "queues": q, "origin": bits3(O), "dir": bits3(D), "geom": G, "w": W, "h": H, "rides": rides, "args": args}


def mk_find(r, **kw):
    c = find_case(r)
    c.update(kw)
    return c


def ray_edges():
    r = random.Random(11)
    for _ in range(60):
        yield ray_case(r)


def ray_random(r):
    return ray_case(r)


def find_edges():
    r = random.Random(12)
    for first in (0, 1, 2, 3):
        for step in (1.0, -1.0, 0.0, float("nan")):
            for axis in (0, 1):
                c = find_case(r)
                c["queues"]["GSQExtra"][0] = first
                a = c["args"]
                a[7], a[2] = step, axis
                yield c
    for cx, cz in ((-1, -1), (0, 0), (100, 100), (-5, 100), (100, -5)):
        c = find_case(r)
        c["queues"]["GSQExtra"][0] = 1
        c["args"][0], c["args"][1] = cx, cz
        yield c
    for _ in range(40):
        c = find_case(r)
        c["queues"]["Pylon"] = [0x7000]
        c["queues"]["PylonCount"] = [r.randrange(1, 4)]
        yield c


def find_random(r):
    return find_case(r)


TESTS = [
    FnTest("RayPolyCheckOnRide", ppc, host, edge=ray_edges, rnd=ray_random),
    FnTest("FindPointedRide", ppc, host, edge=find_edges, rnd=find_random),
]
