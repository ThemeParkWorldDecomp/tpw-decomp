"""Unit v5902, the park darkening and the poly search vertices, against libtpw_unit_RecalculateParkDarkening.dylib
(tests/unit_RecalculateParkDarkening.cpp), main app ("SimTheme Park"), src/LevelSystems_RecalculateParkDarkening.cpp:
  RecalculateParkDarkening 0x1005c59c, UpdateHFieldDarkeningNormals 0x1005c2b0, SetupMeshForPolySearchSimple 0x1005c988

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments (nodes by the id halfword at +0x50, matrices by a checksum, cells by their index in the world) and
answer from the case's queues: CMapCell::IsFlag, SetMatrixIdentity (writes a pattern), CombineMatricesNoWDest2nd (rewrites its second
matrix from both), GetNodeWorldVertPosnsIntoGeomBuffer, GetNodePositionSimple (writes the queue's point), FloatToCell (the cell answers
come from the queue), CSystem::UpdateLights. sqrt is the C library's on both sides.

The world of a case: the height field header, its quads, the nodes (trees with points and index maps), the scratch vertices, the world
block, the mesh with its header, nodes, embedded records and the poly search table. What is compared: the log, the normals, the quads, the
flag word, the table, the records' halfwords.

No fused operations. Not tested: the mesh's native pointers are 64 bits on the host, so the mesh, its embedded set and records are
written at the host's offsets (tpw_rpd_layout); a NULL header or origin node (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RecalculateParkDarkening"
MASK = 0xFFFFFFFF

FN_RECALC, FN_UPDATE, FN_SETUP = 0x1005C59C, 0x1005C2B0, 0x1005C988
SIZES = {FN_RECALC: 540, FN_UPDATE: 676, FN_SETUP: 584}
OWN = set(SIZES)

CALLEE_ISFLAG, CALLEE_IDENT, CALLEE_COMBINE = 0x1009BB70, 0x100C0E28, 0x100C28FC
CALLEE_WORLDVERTS, CALLEE_NODEPOS = 0x101ED17C, 0x1005C8B8
CALLEE_F2C, CALLEE_LIGHTS = 0x10237CD8, 0x10239AA8

ADDR = dict(gHeightfieldHeader=0x102C6C7C, gHeightfieldWidth=0x102C6C68, gHeightfieldHeight=0x102C6C64,
            gHeightfieldQuads=0x102C6C78, gScratchGeomBuffer=0x102C7990, gPolySearchTable=0x102C1E1C,
            gGameWorld=0x1037EBFC, gGameWorldRef=0x102A460C, gParkDarkeningFlag=0x102C1E04)
HL = []   # the host's layout, filled by bind: mesh header, rotation, set pointer, root node; set records; record size, record mF10


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def chk(ws):
    h = 0
    for w in ws:
        h = (h * 31 + w) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def half_at(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def mat_at(e, a):
    return list(struct.unpack(">16I", e.read(a, 64)))


def stub_isflag(e):
    L = e.cl
    world = e.u32(ADDR["gGameWorld"])
    L.rec("IsFlag", ((e.r(3) - world - 0x2D8) // 0x44, e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("IsFlag") & MASK)


def stub_ident(e):
    e.cl.rec("SetMatrixIdentity", ())
    e.write(e.r(3), struct.pack(">16I", *[0x3F000000 + i * 0x10000 for i in range(16)]))


def stub_combine(e):
    src, dst = e.r(3), e.r(4)
    s, d = mat_at(e, src), mat_at(e, dst)
    e.cl.rec("CombineMatricesNoWDest2nd", (half_at(e, src + 0x40), chk(s), chk(d)))
    e.write(dst, struct.pack(">16I", *[(d[i] * 3 + s[i] + 1) & MASK for i in range(16)]))


def stub_worldverts(e):
    e.cl.rec("GetNodeWorldVertPosnsIntoGeomBuffer", (half_at(e, e.r(3) + 0x50), chk(mat_at(e, e.r(4)))))


def stub_nodepos(e):
    L = e.cl
    L.rec("GetNodePositionSimple", (half_at(e, e.r(3) + 0x50), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">3I", L.ret("px") & MASK, L.ret("py") & MASK, L.ret("pz") & MASK))


def stub_f2c(e):
    L = e.cl
    L.rec("FloatToCell", (struct.unpack(">q", struct.pack(">d", e.f(1)))[0],))
    e.set_r(3, L.ret("FloatToCell") & MASK)


def stub_lights(e):
    e.cl.rec("UpdateLights", ())


STUBS.internal(CALLEE_ISFLAG, stub_isflag)
STUBS.internal(CALLEE_IDENT, stub_ident)
STUBS.internal(CALLEE_COMBINE, stub_combine)
STUBS.internal(CALLEE_WORLDVERTS, stub_worldverts)
STUBS.internal(CALLEE_NODEPOS, stub_nodepos)
STUBS.internal(CALLEE_F2C, stub_f2c)
STUBS.internal(CALLEE_LIGHTS, stub_lights)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def p(self, a):
        """An address as a Ptr32 word: the Mac's own address, the host's offset from the arena base."""
        return a - self.base if a else 0

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    base = 0
    rec_stride, rec_half = 0x14, 0x10

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def gptr(self, name, v):
        self.words(ADDR[name], [v])

    def set_world(self, w):
        self.words(ADDR["gGameWorld"], [w])
        self.words(ADDR["gGameWorldRef"], [ADDR["gGameWorld"]])

    def mesh(self, a, hdr, rot, setp, root):
        self.words(a + 4, [hdr])
        self.words(a + 0x10, [rot])
        self.words(a + 0x28, [setp])
        self.words(a + 0x2C, [root])

    def set_recs(self, a, recs):
        self.words(a + 4, [recs])

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_rpd_alloc(16)   # the arena is reserved by the first allocation
        self.base = lib.tpw_rpd_base()
        CL.host_begin(lib, {}, case["queues"])

    @property
    def rec_stride(self):
        return HL[5]

    @property
    def rec_half(self):
        return HL[6]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rpd_addr(name.encode())

    def q(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def gptr(self, name, v):
        self.q(self.g(name), v)

    def set_world(self, w):
        self.gptr("gGameWorld", w)

    def mesh(self, a, hdr, rot, setp, root):
        self.q(a + HL[0], hdr)
        self.words(a + HL[1], [rot])
        self.q(a + HL[2], setp)
        self.q(a + HL[3], root)

    def set_recs(self, a, recs):
        self.q(a + HL[4], recs)

    def alloc(self, n):
        return self.lib.tpw_rpd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_RECALC:
            self.lib.tpw_rpd_recalc()
        elif entry == FN_UPDATE:
            m = self.lib.tpw_rpd_alloc(64)
            self.words(m, args[1:])
            self.lib.tpw_rpd_update(args[0], m)
        else:
            self.lib.tpw_rpd_setup(args[0], s32(args[1]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rpd_addr.restype, lib.tpw_rpd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rpd_alloc.restype, lib.tpw_rpd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rpd_base.restype, lib.tpw_rpd_base.argtypes = C.c_uint64, []
    lib.tpw_rpd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rpd_recalc.argtypes = []
    lib.tpw_rpd_update.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_rpd_setup.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rpd_layout(out)
    HL[:] = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_tree(s, sp, ctx):
    a = s.alloc(0xA0)
    s.words(a, [sp["flags"]])
    s.words(a + 0x10, sp["mat"])
    s.half(a + 0x50, sp["id"])
    n = len(sp["map"])
    s.half(a + 0x5E, n)
    pts = s.alloc(max(12 * n, 12))
    s.words(pts, sp["pts"])
    s.words(a + 0x64, [s.p(pts)])
    ctx["pts"].append((pts, 3 * n))
    mp = s.alloc(max(2 * n, 2))
    for i, v in enumerate(sp["map"]):
        s.half(mp + 2 * i, v)
    s.words(a + 0x94, [s.p(mp)])
    prev = None
    kids = [build_tree(s, k, ctx) for k in sp["kids"]]
    for i, k in enumerate(kids):
        if i == 0:
            s.words(a + 0x0C, [s.p(k)])
        else:
            s.words(kids[i - 1] + 0x08, [s.p(k)])
    return a


def build_hf(s, c, ctx):
    gw, gh = c["gw"], c["gh"]
    qa = s.alloc(4 * gw * gh)
    for i, (h0, h1) in enumerate(c["quads"]):
        s.half(qa + 4 * i, h0)
        s.half(qa + 4 * i + 2, h1)
    ctx["quads"] = (qa, 2 * gw * gh)
    hf = s.alloc(0x30)
    s.words(hf + 0x18, [gw, gh])
    s.words(hf + 0x2C, [s.p(qa)])
    root = build_tree(s, c["root"], ctx)
    hdr = s.alloc(0xB0)
    s.words(hdr + 0x6C, [s.p(hf)])
    s.words(hdr + 0x78, [s.p(root)])
    s.gptr("gHeightfieldHeader", hdr)
    s.gptr("gHeightfieldQuads", qa)
    scr = s.alloc(12 * len(c["scratch"]) // 3 + 12)
    s.words(scr, c["scratch"])
    s.gptr("gScratchGeomBuffer", scr)
    s.words(s.g("gHeightfieldWidth"), [c["gW"]])
    s.words(s.g("gHeightfieldHeight"), [c["gH"]])
    s.words(s.g("gParkDarkeningFlag"), [0x5A5A])
    s.set_world(s.alloc(0x2D8 + 0x44 * 800))
    return root


def build_setup(s, c, ctx):
    E, nc, M = len(c["emb"]), len(c["nodes"]), len(c["minis"])

    def vtx(sp):
        v = s.alloc(0x60)
        s.words(v, sp["verts"])
        return v

    def fill(a, sp):
        s.half(a + 0x50, sp["id"])
        s.half(a + 0x58, sp["vc"])
        s.words(a + 0x60, [s.p(vtx(sp))])

    hdr = s.alloc(0xB0)
    s.half(hdr + 0x44, nc)
    s.half(hdr + 0x46, c["f46"])
    s.half(hdr + 0x48, E)
    nodes = s.alloc(0xA0 * max(nc, 1))
    for k, sp in enumerate(c["nodes"]):
        fill(nodes + 0xA0 * k, sp)
    s.words(hdr + 0x70, [s.p(nodes)])
    minis = s.alloc((M + 2) * 0x58)
    for m, sp in enumerate(c["minis"]):
        s.half(minis + m * 0x58 + 0x50, sp["id"])
        nb = minis + (m + 1) * 0x58
        s.half(nb, sp["vc"])
        s.words(nb + 8, [s.p(vtx(sp))])
    s.words(hdr + 0x74, [s.p(minis)])
    origin = s.alloc(0xA0)
    s.words(origin + 0x40, c["origin"])
    s.words(hdr + 0x78, [s.p(origin)])
    emb = s.alloc(0x14 * max(E, 1))
    for i, f in enumerate(c["emb"]):
        s.words(emb + 0x14 * i, [f])
    s.words(hdr + 0x7C, [s.p(emb)])
    stride = s.rec_stride
    recs = s.alloc(stride * max(E, 1))
    for i in range(E):
        s.half(recs + stride * i + s.rec_half, 0xABCD)
    ctx["recs"] = [recs + stride * i + s.rec_half for i in range(E)]
    setp = s.alloc(0x20)
    s.set_recs(setp, recs)
    root = 0
    if c["root"] is not None:
        root = s.alloc(0xA0)
        fill(root, c["root"])
    mesh = s.alloc(0x80)
    s.mesh(mesh, hdr, c["rot"], setp, root)
    size = 12 * 8 * (1 + E) + 24
    table = s.alloc(size)
    s.words(table, [0x12345678] * (size // 4))
    ctx["table"] = (table, size // 4)
    s.gptr("gPolySearchTable", table)
    return mesh


def build(s, c):
    ctx = {"pts": []}
    if c["op"] == "setup":
        ctx["entry"] = build_setup(s, c, ctx)
    else:
        ctx["entry"] = build_hf(s, c, ctx)
    return ctx


def execute(s, c, ctx):
    if c["op"] == "recalc":
        return s.call(FN_RECALC, [])
    if c["op"] == "update":
        return s.call(FN_UPDATE, [ctx["entry"]] + c["mat"])
    return s.call(FN_SETUP, [ctx["entry"], c["flag"] & MASK])


def observe(s, c, ctx, answer):
    out = {"log": s.log(), "flag": s.rd_words(s.g("gParkDarkeningFlag"), 1)}
    if c["op"] == "setup":
        out["table"] = [canon_f(w) for w in s.rd_words(*ctx["table"])]
        out["recs"] = [s.rd_halves(a, 1)[0] for a in ctx["recs"]]
    else:
        out["pts"] = [[canon_f(w) for w in s.rd_words(a, n)] for a, n in ctx["pts"]]
        out["quads"] = s.rd_halves(*ctx["quads"])
    return out


def run(s, c):
    try:
        ctx = build(s, c)
        return observe(s, c, ctx, execute(s, c, ctx))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]
IDC = [0]


def rf(r):
    k = r.random()
    if k < 0.05:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def nid():
    IDC[0] = IDC[0] % 60000 + 1
    return IDC[0]


def tree(r, S, depth=0, kids=None, flags=None, n=None):
    n = r.randrange(0, 4) if n is None else n
    nk = r.randrange(0, 3) if kids is None else kids
    return {"id": nid(), "flags": r.choice([0, 0, 0, 0x200, 0x400, 0x201, 0xFFFFFDFF]) if flags is None else flags,
            "mat": [rf(r) for _ in range(16)], "map": [r.randrange(S) for _ in range(n)],
            "pts": [rf(r) for _ in range(3 * n)],
            "kids": [tree(r, S, depth + 1) for _ in range(nk)] if depth < 2 else []}


def hf_case(r, op, **kw):
    gw, gh = kw.get("gw", r.randrange(1, 5)), kw.get("gh", r.randrange(1, 4))
    S = r.randrange(1, 7)
    lim = max(gw, gh) + 2
    c = {"op": op, "gw": gw, "gh": gh, "gW": r.randrange(1, 7), "gH": r.randrange(1, 6),
         "quads": [(r.choice([0, 0x400, 0x401, 0xFFFF, 0xFBFF, r.randrange(65536)]), r.randrange(65536)) for _ in range(gw * gh)],
         "scratch": [fb(r.uniform(-40, 40)) if r.random() < 0.9 else rf(r) for _ in range(3 * S)],
         "root": tree(r, S, kids=kw.get("kids"), flags=kw.get("flags"), n=kw.get("n")),
         "mat": [rf(r) for _ in range(16)],
         # three answers a vertex: the first cell must be in the row (the original indexes with it unchecked)
         "queues": {"FloatToCell": [v for _ in range(14) for v in (r.randrange(0, gw), r.randrange(-1, gh + 2), r.randrange(-1, gw + 2))],
                    "IsFlag": [r.choice([0, 1, 2, 5]) for _ in range(40)]}}
    c.update({k: v for k, v in kw.items() if k in ("gW", "gH")})
    if "f2c" in kw:
        c["queues"]["FloatToCell"] = kw["f2c"]
    if "isflag" in kw:
        c["queues"]["IsFlag"] = kw["isflag"]
    return c


def vsp(r, vc=None):
    vc = r.randrange(0, 9) if vc is None else vc
    return {"id": nid(), "vc": vc, "verts": [fb(r.uniform(-20, 20)) if r.random() < 0.9 else rf(r) for _ in range(24)]}


def setup_case(r, **kw):
    E = kw.get("E", r.randrange(0, 5))
    nc = r.randrange(0, 6)
    f46 = r.randrange(0, 4)
    M = max(0, f46 + E - nc)
    return {"op": "setup", "flag": kw.get("flag", r.choice([0, 0, 1, 5, -1])),
            "rot": kw.get("rot", r.choice([0, 0x5A, 0xB4, 0x10E, 0x2D, 0x168])),
            "f46": f46, "origin": [fb(r.uniform(-50, 50)) for _ in range(3)],
            "emb": [r.choice([0, 1, 2, 3, 0xFF]) for _ in range(E)],
            "nodes": [vsp(r) for _ in range(nc)], "minis": [vsp(r) for _ in range(M)],
            "root": None if r.random() < 0.2 else vsp(r),
            "queues": {"px": [fb(r.uniform(-9, 9)) for _ in range(6)], "py": [fb(r.uniform(-9, 9)) for _ in range(6)],
                       "pz": [fb(r.uniform(-9, 9)) for _ in range(6)]}}


def recalc_edges():
    r = random.Random(11)
    for kw in ({}, {"gW": 1, "gH": 1}, {"gw": 1, "gh": 1, "isflag": [1, 0]}, {"isflag": [1, 1, 0, 0]}, {"flags": 0x200}, {"kids": 0}):
        yield hf_case(r, "recalc", **kw)


def update_edges():
    r = random.Random(12)
    for kw in ({"flags": 0x200}, {"kids": 0, "flags": 0}, {"n": 0}, {"gw": 1, "gh": 1, "n": 3, "flags": 0},
               {"f2c": [0], "n": 3, "flags": 0}, {"f2c": [0, 0, -1], "n": 2, "flags": 0}, {"f2c": [0, 5, 0], "n": 2, "flags": 0}):
        yield hf_case(r, "update", **kw)


def setup_edges():
    r = random.Random(13)
    for kw in ({"flag": 0, "rot": 0}, {"flag": 1, "rot": 0x5A}, {"flag": 0, "rot": 0xB4, "E": 3}, {"flag": 0, "rot": 0x10E, "E": 4},
               {"flag": 0, "E": 0}, {"flag": 7, "rot": 0xB4}):
        yield setup_case(r, **kw)


TESTS = [
    FnTest("RecalculateParkDarkening", ppc, host, edge=recalc_edges, rnd=lambda r: hf_case(r, "recalc")),
    FnTest("UpdateHFieldDarkeningNormals", ppc, host, edge=update_edges, rnd=lambda r: hf_case(r, "update")),
    FnTest("SetupMeshForPolySearchSimple", ppc, host, edge=setup_edges, rnd=lambda r: setup_case(r)),
]
