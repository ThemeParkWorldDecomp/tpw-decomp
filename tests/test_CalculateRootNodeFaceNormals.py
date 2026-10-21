"""Unit v5338, ride mesh helpers, against libtpw_unit_CalculateRootNodeFaceNormals.dylib (tests/unit_CalculateRootNodeFaceNormals.cpp), main app
("SimTheme Park"), src/game/rides/TPRide.cpp:
  CalculateRootNodeFaceNormals 0x100694c8, GetBlendedBaseTextures 0x10069770, GetNodeWorldVertPosnsIntoGeomBuffer 0x10068dec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Blocks of the file
layout (nodes, headers, the height field) hold Ptr32 words (the Mac's address, the host's arena offset); the native objects (the mesh, the ride
set, the globals) use each side's pointer width and offsets (the host's come from tpw_rg_layout). The callees outside the unit are fakes on both
sides: the float to int glue 0x10237cd8 (answers from the queue Ftoi), CMesh::GenerateRideBase (queue), ERRORBox (texts), and
CombineMatricesNoWDest1st (adds the source into the destination word by word, logs a hash of both); sqrt is the C one on both sides.

What is compared: the answers, the scratch buffer (and a guard past it), the faces' index-and-flag words (each side's byte order, as the loader leaves them), the height field's cells, the event log.

Fused operations (fmadds, fmsubs, fmadd) run under HardwareRounding. Not tested: a rotation other than 0, 90, 180, 270 in GetBlendedBaseTextures
(the original then uses stale registers for the cell position).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CalculateRootNodeFaceNormals"
MASK = 0xFFFFFFFF

FN_N, FN_B, FN_W = 0x100694C8, 0x10069770, 0x10068DEC
SIZES = {FN_N: 616, FN_B: 744, FN_W: 416}
OWN = set(SIZES)

FTOI, GENBASE, ERRBOX, COMBINE = 0x10237CD8, 0x10239A90, 0x1006FD3C, 0x100C2694

ADDR = dict(scratch=0x102C7990, hfheader=0x102C6C7C)
HOST_FIXED = [4, 0x10, 0x58, 0x5A, 0x60, 0x6C, 0x70, 0x94, 0x18, 0x6C, 0x78, 0x10, 0x14, 0x18, 0x2C]
NAT_NAMES = ["m_hdr", "m_f10", "m_eng", "m_skirt", "s_self", "s_heads", "s_fc0", "s_fc4", "s_hm", "mesh_size", "set_size"]
MAC_NAT = dict(m_hdr=4, m_f10=0x10, m_eng=0x1C, m_skirt=0x18, s_self=0, s_heads=8, s_fc0=0xC0, s_fc4=0xC4, s_hm=0xCC)
HOST_NAT = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def hashw(ws):
    h = 0
    for w in ws:
        h = (h * 31 + canon_f(w)) & MASK
    return h


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ftoi(e):
    L = e.cl
    L.rec("Ftoi", (fb(e.f(1)),))
    e.set_r(3, L.ret("Ftoi") & MASK)


def stub_combine(e):
    L = e.cl
    d, s = e.r(3), e.r(4)
    dw = struct.unpack(">16I", e.read(d, 64))
    sw = struct.unpack(">16I", e.read(s, 64))
    L.rec("Combine", (hashw(sw), hashw(dw)))
    out = [fb(bitsf(a) + bitsf(b)) for a, b in zip(dw, sw)]
    e.write(d, struct.pack(">16I", *out))


STUBS.internal(FTOI, stub_ftoi)
STUBS.internal(COMBINE, stub_combine)
_genbase = CL.recorder("GenerateRideBase", "Liiii", ret="i")


def stub_genbase(e):
    """A glue import: the glue saves the caller's TOC pointer at 0x14(r1) and the caller reloads it afterwards."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    _genbase(e)


STUBS.internal(GENBASE, stub_genbase)
STUBS.internal(ERRBOX, CL.recorder("ERRORBox", "ss"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def h16(self, a, vs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vs), *[v & 0xFFFF for v in vs]))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def off(self, name):
        return self.nat_off[name]


class Ppc(Side):
    fmt = ">I"
    nat_off = MAC_NAT

    def __init__(self, e, case):
        self.e = e
        self.eng = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.eng: 1}, case["queues"])
        self.saved = STUBS.activate(e)

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

    def ptr(self, v):
        return v

    def nat(self, a, v):
        self.words(a, [v])

    def call(self, op, args):
        entry = {"n": FN_N, "b": FN_B, "w": FN_W}[op]
        with ROUNDING(self.e):
            T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.nat_off = HOST_NAT
        self.eng = self.alloc(16)
        CL.host_begin(lib, {1: self.eng}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rg_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, v):
        return v - self.lib.tpw_rg_base()

    def nat(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def call(self, op, args):
        lib = self.lib
        if op == "n":
            lib.tpw_rg_normals(args[0], s32(args[1]), args[2])
        elif op == "b":
            lib.tpw_rg_blended(args[0])
        else:
            lib.tpw_rg_worldverts(args[0])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rg_addr.restype, lib.tpw_rg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rg_alloc.restype, lib.tpw_rg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rg_base.restype, lib.tpw_rg_base.argtypes = C.c_uint64, []
    lib.tpw_rg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rg_normals.restype, lib.tpw_rg_normals.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_rg_blended.restype, lib.tpw_rg_blended.argtypes = None, [C.c_uint64]
    lib.tpw_rg_worldverts.restype, lib.tpw_rg_worldverts.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_rg_layout(out)
    got = list(out)[:n]
    assert got[:len(HOST_FIXED)] == HOST_FIXED, got
    for name, v in zip(NAT_NAMES, got[len(HOST_FIXED):]):
        HOST_NAT[name] = v
    lib.base = lib.tpw_rg_base()
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

GUARD = 4


def build_normals(s, c):
    nsets = len(c["vbits"]) // 12
    node = s.alloc(0xA0)
    verts = s.alloc(4 * len(c["vbits"]) + 4)
    s.words(verts, c["vbits"])
    tbl = s.alloc(2 * len(c["table"]) + 4)
    s.h16(tbl, c["table"])
    nf = len(c["faces"])
    faces = s.alloc(8 * nf + 4)
    for k, (b0, b1, i0, i1, i2) in enumerate(c["faces"]):
        s.write(faces + 8 * k, struct.pack(s.fmt[0] + "4H", (b0 << 8) | b1, i0, i1, i2))
    fsets = s.alloc(16 * len(c["fsets"]) + 4)
    for k, (st, cnt) in enumerate(c["fsets"]):
        s.write(fsets + 16 * k + 4, struct.pack(s.fmt[0] + "2H", st, cnt))
    s.h16(node + 0x58, [nsets * 4])
    s.h16(node + 0x5A, [len(c["fsets"])])
    s.words(node + 0x60, [s.ptr(verts)])
    s.words(node + 0x6C, [s.ptr(fsets)])
    s.words(node + 0x70, [s.ptr(faces)])
    s.words(node + 0x94, [s.ptr(tbl)])
    buf = s.alloc(12 * nf + 12 * GUARD + 16)
    s.nat(s.g("scratch"), buf)
    res = s.alloc(4)
    s.words(res, [0x55555555])
    return dict(node=node, faces=faces, nf=nf, buf=buf, res=res)


def build_world(s, c):
    nv = c["nverts"]
    node = s.alloc(0xA0)
    nsets = (nv + 3) // 4
    verts = s.alloc(0x30 * nsets + 4)
    s.words(verts, c["vbits"])
    s.h16(node + 0x58, [nv])
    s.words(node + 0x60, [s.ptr(verts)])
    s.words(node + 0x10, c["mats"][0])
    link = node
    for k, m in enumerate(c["mats"][1:]):
        ch = s.alloc(0x50)
        s.words(ch + 0x10, m)
        s.words(link + 4, [s.ptr(ch)])
        link = ch
    buf = s.alloc(12 * nv + 12 * GUARD + 16)
    s.nat(s.g("scratch"), buf)
    return dict(node=node, buf=buf, nv=nv)


def build_blended(s, c):
    w, h = c["w"], c["h"]
    stride = c["stride"]
    total = 32 * stride + 32
    center = 16 * stride + 16
    q = s.alloc(4 * total)
    s.h16(q, c["quads"])
    hf = s.alloc(0x40)
    s.words(hf + 0x10, [c["fa"], c["fb"]])
    s.words(hf + 0x18, [stride])
    s.words(hf + 0x2C, [s.ptr(q + 4 * center)])
    hdr1 = s.alloc(0x100)
    s.words(hdr1 + 0x6C, [s.ptr(hf)])
    s.nat(s.g("hfheader"), hdr1)
    node = s.alloc(0xA0)
    s.words(node + 0x40, [c["m30"]])
    s.words(node + 0x48, [c["m32"]])
    hdr2 = s.alloc(0x100)
    s.write(hdr2 + 0x18, c["name"].encode() + b"\0")
    s.words(hdr2 + 0x78, [s.ptr(node)])
    skirt = s.alloc(4 * len(c["skirt"]) + 4)
    for k, (fl, val) in enumerate(c["skirt"]):
        s.write(skirt + 4 * k, struct.pack(s.fmt[0] + "2H", fl, val))
    hm = s.alloc(0x20)
    s.h16(hm + 8, [w, h])
    decoy = s.alloc(0x20)
    s.h16(decoy + 8, [9, 7])
    selfblk = s.alloc(0x200)
    s.nat(selfblk + s.off("s_hm"), hm)
    mesh = s.alloc(0x100)
    s.nat(mesh + s.off("m_hdr"), hdr2)
    s.words(mesh + s.off("m_f10"), [c["rot"]])
    s.nat(mesh + s.off("m_eng"), s.eng)
    s.nat(mesh + s.off("m_skirt"), skirt)
    st = s.alloc(0x200)
    s.nat(st + s.off("s_self"), selfblk)
    s.nat(st + s.off("s_heads"), mesh)
    s.nat(st + s.off("s_hm"), decoy)
    s.h16(st + s.off("s_fc0"), c["fc0"])
    s.h16(st + s.off("s_fc4"), c["fc4"])
    return dict(set=st, q=q, total=total)


def run(s, c):
    try:
        op = c["op"]
        if op == "n":
            w = build_normals(s, c)
            s.call("n", [w["node"], c["rot"], w["res"]])
            return {"res": s.rd_word(w["res"]), "faces": [struct.unpack(s.fmt[0] + "H", s.read(w["faces"] + 8 * k, 2))[0] for k in range(w["nf"])],
                    "buf": [canon_f(x) for x in s.rd_words(w["buf"], 3 * w["nf"] + 3 * GUARD)], "log": s.log()}
        if op == "w":
            w = build_world(s, c)
            s.call("w", [w["node"]])
            return {"buf": [canon_f(x) for x in s.rd_words(w["buf"], 3 * w["nv"] + 3 * GUARD)], "log": s.log()}
        w = build_blended(s, c)
        s.call("b", [w["set"]])
        return {"q": list(struct.unpack(s.fmt[0] + "%dH" % (2 * w["total"]), s.read(w["q"], 4 * w["total"]))), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x00800000]
ROTS = [0, 0x5A, 0xB4, 0x10E]
ROTS_N = ROTS + [7, 0x2D, 0x168, -90]


def rf(r):
    k = r.random()
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rfm(r):
    k = r.random()
    if k < 0.05:
        return r.choice([0, 0x80000000, 0x3F800000])
    if k < 0.85:
        return fb(r.uniform(-4, 4))
    return fb(r.uniform(-300, 300))


def rcoord(r, small):
    if small:
        return fb(float(r.randint(-2, 2)))
    return rf(r)


# ---- normals
def n_case(r, rot=None, small=None, sets=None):
    small = r.random() < 0.5 if small is None else small
    nsets = r.randint(1, 3)
    vbits = [rcoord(r, small) for _ in range(12 * nsets)]
    nv = 4 * nsets
    tlen = r.randint(1, 12)
    table = [r.randrange(nv) for _ in range(tlen)]
    nf = r.randint(0, 9)
    faces = []
    for _ in range(nf):
        if r.random() < 0.15:
            i = r.randrange(tlen)
            idx = (i, i, i)
        else:
            idx = tuple(r.randrange(tlen) for _ in range(3))
        faces.append((r.getrandbits(8), r.getrandbits(8)) + idx)
    nfs = r.randint(0, 3) if sets is None else sets
    fsets = []
    for _ in range(nfs):
        st = r.randint(0, nf)
        fsets.append((st, r.randint(0, nf - st)))
    return {"op": "n", "queues": {}, "rot": r.choice(ROTS_N) if rot is None else rot, "vbits": vbits, "table": table,
            "faces": faces, "fsets": fsets}


def n_edges():
    r = random.Random(11)
    for rot in ROTS_N:
        for small in (True, False):
            for _ in range(3):
                yield n_case(r, rot, small)
    yield n_case(r, 0, True, 0)
    # a fixed flat face: y normal exactly 1 (the 0.999 compare) with every rotation
    for rot in ROTS:
        c = n_case(r, rot, True, 1)
        c["vbits"] = [fb(v) for v in [0, 0, 1, 1] + [0, 0, 0, 0] + [0, 1, 0, 0]]
        c["table"] = [0, 1, 2]
        c["faces"] = [(0, 0, 0, 1, 2), (0, 0xFF, 0, 2, 1)]
        c["fsets"] = [(0, 2)]
        yield c


def n_random(r):
    return n_case(r)


# ---- world vertices
def w_case(r, nv=None, chain=None):
    nv = r.randint(0, 9) if nv is None else nv
    chain = r.randint(0, 3) if chain is None else chain
    nsets = (nv + 3) // 4
    return {"op": "w", "queues": {}, "nverts": nv, "vbits": [rf(r) for _ in range(12 * nsets)],
            "mats": [[rfm(r) for _ in range(16)] for _ in range(chain + 1)]}


def w_edges():
    r = random.Random(12)
    yield w_case(r, 0, 0)
    yield w_case(r, 0, 2)
    yield w_case(r, 1, 0)
    yield w_case(r, 4, 1)
    yield w_case(r, 5, 3)
    yield w_case(r, 8, 0)


def w_random(r):
    return w_case(r)


# ---- blended base
NAMES = ["", "Ride", "Big Coaster 7", "abcdefghijklmnopqrs"]


def b_case(r, rot=None, ret=None, w=None, h=None):
    rot = r.choice(ROTS) if rot is None else rot
    w = r.randint(0, 4) if w is None else w
    h = r.randint(0, 4) if h is None else h
    stride = r.randint(4, 12)
    total = 32 * stride + 32
    skirt = []
    for _ in range(w * h):
        fl = r.getrandbits(16)
        if r.random() < 0.75:
            fl |= 0x80
        else:
            fl &= ~0x80
        skirt.append((fl, r.getrandbits(16)))
    ret = r.choice([0, 1, 5]) if ret is None else ret
    return {"op": "b", "queues": {"Ftoi": [r.randint(-3, 6), r.randint(-3, 6)], "GenerateRideBase": [ret]},
            "rot": rot, "w": w, "h": h, "stride": stride, "skirt": skirt, "quads": [r.getrandbits(16) for _ in range(2 * total)],
            "fa": rf(r), "fb": rf(r), "m30": rf(r), "m32": rf(r), "name": r.choice(NAMES),
            "fc0": [r.getrandbits(16), r.getrandbits(16)], "fc4": [r.getrandbits(16), r.getrandbits(16)]}


def b_edges():
    r = random.Random(13)
    for rot in ROTS:
        for ret in (0, 1):
            yield b_case(r, rot, ret, 3, 2)
    yield b_case(r, 0, 1, 0, 0)
    yield b_case(r, 0x5A, 0, 0, 3)
    yield b_case(r, 0xB4, 1, 4, 4)
    yield b_case(r, 0x10E, 1, 4, 4)


def b_random(r):
    return b_case(r)


TESTS = [
    FnTest("CalculateRootNodeFaceNormals", ppc, host, edge=n_edges, rnd=n_random),
    FnTest("GetBlendedBaseTextures", ppc, host, edge=b_edges, rnd=b_random),
    FnTest("GetNodeWorldVertPosnsIntoGeomBuffer", ppc, host, edge=w_edges, rnd=w_random),
]
