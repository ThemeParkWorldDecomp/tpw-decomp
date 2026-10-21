"""Unit v5641, the coaster's end pylons and texture scrolling, against libtpw_unit_PlaceEndPylons.dylib (tests/unit_PlaceEndPylons.cpp), main app
("SimTheme Park"), src/game/rides/TPCoaster.cpp:
  PlaceEndPylons 0x100424f0, CoasterUVAnimTick 0x100428d4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers where the host structs have them, Ptr32 offsets from the
arena base where the headers use Ptr32) for the translation; the same code reads both back. The callees outside the unit (the node search,
NewPylon, NewTrackBrace, GetRideAnimTime, the float-to-cell runtime function at 0x10237cd8) are fakes on both sides that record what the real
ones read and answer from the case's queues. modf is the real library function on the host and Python's math.modf on the PowerPC side.

The world of a case: gRideAnimSets[1..3] (each a set -> mesh -> embedded set -> records -> 0x40-byte blocks), the instance with its type,
pylon, brace and data blocks (PlaceEndPylons); the instance, its type, texture set, the chain of mesh sets with header, node, face sets, faces
and UV sets (CoasterUVAnimTick). What is compared: the event log, the UV offsets, the stored time and every UV set after the call.

No fused operations. Not tested: a node search answer of -1 on the first round (the Mac leaves its stack slot uninitialised), a NULL
pylon list, a face index above 255 (the Mac's 256-word table is overrun), a texture slot outside the entries.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PlaceEndPylons"
MASK = 0xFFFFFFFF

FN_PEP, FN_UV = 0x100424F0, 0x100428D4
SIZES = {FN_PEP: 508, FN_UV: 676}
OWN = set(SIZES)

CALLEE_FLOATCELL = 0x10237CD8
CALLEE_FIND = 0x101FFDC8
CALLEE_NEWPYLON = 0x1003D610
CALLEE_NEWBRACE = 0x10040304
CALLEE_ANIMTIME = 0x100C3268

ADDR = dict(gRideAnimSets=0x102C79A4)

# the Mac's offsets and sizes; the host's come from tpw_pep_layout in the same order
NAMES = [
    ("inst.type", 0x04), ("inst.entrance", 0x10), ("inst.pylons", 0x4C), ("inst.lastbrace", 0x54), ("inst.f2c", 0x2C),
    ("inst.f138", 0x138), ("inst.uvoff", 0x13C), ("inst.f38", 0x38), ("inst.size", 0x150),
    ("brace.data", 0x100), ("brace.word70", 0x70), ("brace.size", 0x108),
    ("data.f18", 0x18), ("data.size", 0x40),
    ("pylon.x", 0x1C), ("pylon.y", 0x20), ("pylon.z", 0x24), ("pylon.size", 0x48),
    ("set.heads", 0x08), ("set.size", 0x100),
    ("mesh.f28", 0x28), ("mesh.header", 0x04), ("mesh.size", 0x40),
    ("es.records", 0x04),
    ("rec.block", 0x04), ("rec.size", 0x14),
    ("type.textures", 0x1C), ("type.count", 0x20), ("type.size", 0x100),
    ("texset.entries", 0x10), ("texset.textures", 0x18), ("texset.size", 0x20),
    ("entry.flags", 0x00), ("entry.size", 0x10),
    ("dset.mesh", 0x24), ("dset.f38", 0x38), ("dset.size", 0x40),
]
MAC = dict(NAMES)
KEYS = [k for k, _ in NAMES]
NR = 3          # records per embedded set
NSETS = 3       # gRideAnimSets[1..3]
NTEX = 8        # textures / texture entries
NFACES = 6      # faces per mesh set
NUV = 64        # sUVSet blocks (indices up to 255)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_floatcell(e):
    e.cl.rec("FloatToCell", (canon_f(fb(e.f(1))),))
    e.set_r(3, 0)


def stub_find(e):
    L = e.cl
    L.rec("Find", (e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("Find") & MASK)


def stub_newpylon(e):
    L = e.cl
    pt = e.r(8)
    px, py, pz = struct.unpack(">3I", e.read(pt, 12))
    L.rec("NewPylon", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, L.lab(e.r(7)), canon_f(px), canon_f(py), canon_f(pz)))
    a = e.alloc(0x48)
    e.write(a + MAC["pylon.x"], struct.pack(">I", L.ret("py.x") & MASK))
    e.write(a + MAC["pylon.y"], struct.pack(">I", L.ret("py.y") & MASK))
    e.write(a + MAC["pylon.z"], struct.pack(">I", L.ret("py.z") & MASK))
    e.set_r(3, a)


def stub_newbrace(e):
    L = e.cl
    nine = e.u32((e.r(1) + 0x38) & MASK)
    L.rec("NewTrackBrace", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), e.r(7) & MASK, L.lab(e.r(8)), e.r(9) & MASK,
                            e.r(10) & MASK, nine))
    e.set_r(3, 0)


def stub_animtime(e):
    L = e.cl
    L.rec("GetRideAnimTime", (e.r(3) & MASK,))
    e.set_r(3, L.ret("GetRideAnimTime") & MASK)


def stub_modf(e):
    frac, whole = math.modf(e.f(1))
    e.write(e.r(5) & MASK, struct.pack(">d", whole))
    e.set_f1(frac)


STUBS.internal(CALLEE_FLOATCELL, stub_floatcell)
STUBS.internal(CALLEE_FIND, stub_find)
STUBS.internal(CALLEE_NEWPYLON, stub_newpylon)
STUBS.internal(CALLEE_NEWBRACE, stub_newbrace)
STUBS.internal(CALLEE_ANIMTIME, stub_animtime)
STUBS.imp("modf", stub_modf)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def zero(self, n):
        a = self.alloc(n + 8)
        self.write(a, bytes(n + 8))
        return a

    def blk(self, key):
        return self.zero(max(MAC[key], self.off[key]))

    def wptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def ptr(self, a, key, p):
        self.wptr(a + self.off[key], p)

    def u32(self, a, key, v):
        self.words(a + self.off[key], [v])

    def p32(self, a, off, p):
        self.words(a + off, [(p - self.base) & MASK if p else 0])

    def u16(self, a, off, v):
        self.write(a + off, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    base = 0
    off = MAC

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

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_pep(self, inst, h):
        T.call_ppc(self.e, FN_PEP, [inst, T.F(bitsf(h))])

    def call_uv(self, inst):
        T.call_ppc(self.e, FN_UV, [inst])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.H
        self.base = lib.tpw_pep_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pep_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pep_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_pep(self, inst, h):
        self.lib.tpw_pep_place(inst, bitsf(h))

    def call_uv(self, inst):
        self.lib.tpw_pep_uv(inst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("base", C.c_uint64, []),
                            ("place", None, [C.c_uint64, C.c_float]), ("uv", None, [C.c_uint64])):
        f = getattr(lib, "tpw_pep_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_pep_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 64)()
    n = lib.tpw_pep_layout(out)
    assert n == len(KEYS), n
    lib.H = dict(zip(KEYS, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the worlds


def build_pep(s, c):
    O = s.off
    sets_base = s.g("gRideAnimSets")
    for k in range(1, NSETS + 1):
        recs_spec = c["sets"][k]
        st = s.blk("set.size")
        mesh = s.blk("mesh.size")
        es = s.zero(0x20)
        recs = s.zero(NR * O["rec.size"])
        for j, rec in enumerate(recs_spec):
            if rec is not None:
                b = s.zero(0x40)
                s.words(b + 0x30, rec)
                s.wptr(recs + j * O["rec.size"] + O["rec.block"], b)
        s.ptr(es, "es.records", recs)
        s.ptr(mesh, "mesh.f28", es)
        s.ptr(st, "set.heads", mesh)
        s.wptr(sets_base + k * s.psz, st)
    typ = s.zero(0x40)
    s.words(typ, [c["tflags"]])
    flat = s.zero(0x40)
    s.words(flat + 0x2C, [c["node"]])
    data = s.blk("data.size")
    s.ptr(data, "data.f18", flat)
    brace = s.blk("brace.size")
    s.ptr(brace, "brace.data", data)
    s.u32(brace, "brace.word70", c["word70"])
    pyl = s.blk("pylon.size")
    s.u32(pyl, "pylon.x", c["pylon"][0])
    s.u32(pyl, "pylon.y", c["pylon"][1])
    s.u32(pyl, "pylon.z", c["pylon"][2])
    inst = s.blk("inst.size")
    s.ptr(inst, "inst.type", typ)
    s.u32(inst, "inst.entrance", c["entrance"])
    s.ptr(inst, "inst.pylons", pyl)
    s.ptr(inst, "inst.lastbrace", brace)
    return inst


def build_uv(s, c):
    O = s.off
    tex = s.zero(NTEX * 8)
    entries = s.zero(NTEX * O["entry.size"])
    for j, fl in enumerate(c["entries"]):
        s.words(entries + j * O["entry.size"] + O["entry.flags"], [fl])
    texset = s.blk("texset.size")
    s.ptr(texset, "texset.entries", entries)
    s.ptr(texset, "texset.textures", tex)
    typ = s.blk("type.size")
    s.ptr(typ, "type.textures", texset)
    s.u32(typ, "type.count", len(c["offs"]))
    offs = s.zero(4 * max(len(c["offs"]), 1))
    s.words(offs, c["offs"])
    nxt = 0
    uvs_list = []
    for ds in reversed(c["chain"]):
        dset = s.blk("dset.size")
        s.ptr(dset, "dset.f38", nxt)
        mesh = s.blk("mesh.size")
        header = s.zero(0xB8)
        node = s.zero(0xA8)
        fsets = s.zero(16 * max(len(ds["fsets"]), 1))
        faces = s.zero(8 * NFACES)
        uvs = s.zero(NUV * 0x20)
        s.words(uvs, ds["uvs"])
        for j, (slot, first, cnt) in enumerate(ds["fsets"]):
            s.p32(fsets + 16 * j, 0, tex + 8 * slot)
            s.u16(fsets + 16 * j, 4, first)
            s.u16(fsets + 16 * j, 6, cnt)
        for j, tri in enumerate(ds["faces"]):
            for q in range(3):
                s.u16(faces + 8 * j, 2 + 2 * q, tri[q])
        s.u16(node, 0x5A, len(ds["fsets"]))
        s.p32(node, 0x68, uvs)
        s.p32(node, 0x6C, fsets)
        s.p32(node, 0x70, faces)
        s.p32(header, 0x78, node)
        s.ptr(mesh, "mesh.header", header)
        s.ptr(dset, "dset.mesh", mesh)
        nxt = dset
        uvs_list.insert(0, uvs)
    s.world = {"offs": offs, "n": len(c["offs"]), "uvs": uvs_list}
    inst = s.blk("inst.size")
    s.ptr(inst, "inst.type", typ)
    s.u32(inst, "inst.f2c", c["f2c"])
    s.u32(inst, "inst.f138", c["last"])
    s.ptr(inst, "inst.uvoff", offs)
    s.ptr(inst, "inst.f38", nxt)
    s.world["inst"] = inst
    return inst


def run(s, c):
    try:
        if c["op"] == "pep":
            inst = build_pep(s, c)
            s.call_pep(inst, c["height"])
            return {"log": s.log()}
        inst = build_uv(s, c)
        s.call_uv(inst)
        w = s.world
        return {
            "log": s.log(),
            "offs": [canon_f(x) for x in s.rd_words(w["offs"], w["n"])],
            "f138": s.rd_words(inst + s.off["inst.f138"], 1),
            "uvs": [[canon_f(x) for x in s.rd_words(u, NUV * 8)] for u in w["uvs"]],
        }
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rb(r):
    return fb(r.uniform(-100, 100))


def pep_case(r, flag=None):
    flag = r.random() < 0.5 if flag is None else flag
    c = {"op": "pep", "height": rb(r)}
    c["tflags"] = (r.getrandbits(32) & ~0x20 & MASK) | (0x20 if flag else 0)
    c["node"] = r.randrange(0, NR)
    c["word70"] = r.randrange(1, NSETS + 1)
    c["entrance"] = r.randrange(1, NSETS + 1)
    c["pylon"] = [rb(r), rb(r), rb(r)]
    c["sets"] = {k: [None if r.random() < 0.25 else [rb(r), rb(r), rb(r)] for _ in range(NR)] for k in range(1, NSETS + 1)}
    a1 = r.choice([0, 1, 2, MASK])
    c["queues"] = {"Find": [r.randrange(0, NR), a1], "py.x": [rb(r), rb(r)], "py.y": [rb(r), rb(r)], "py.z": [rb(r), rb(r)]}
    return c


def pep_edges():
    r = random.Random(641)
    for flag in (True, False):
        for _ in range(8):
            yield pep_case(r, flag)
    for flag in (True, False):                        # a record without a block (zeros), on each set
        c = pep_case(r, flag)
        c["sets"] = {k: [None] * NR for k in c["sets"]}
        yield c
    c = pep_case(r, False)                            # the second round finds nothing: the point stays
    c["queues"]["Find"] = [1, MASK]
    yield c
    c = pep_case(r, False)
    c["queues"]["Find"] = [MASK - 0, 2]               # -1 on the first round is not tested: use valid ones
    c["queues"]["Find"] = [0, 0]
    yield c


def pep_random(r):
    return pep_case(r)


def uv_case(r, **kw):
    c = {"op": "uv"}
    n = kw.get("count", r.randrange(0, 4))
    c["offs"] = kw.get("offs") or [fb(r.choice([r.uniform(0, 1.5), r.uniform(-2, 3), 1.0, 0.0])) for _ in range(n)]
    c["f2c"] = kw.get("f2c", fb(r.choice([0.0, 1.0, r.uniform(0, 3)])))
    last = r.getrandbits(32)
    delta = kw.get("delta", r.choice([0, 1, 16, 50, 1000, 2500, 7777, r.randrange(0, 100000), r.getrandbits(32)]))
    c["last"] = last
    now = (last + delta) & MASK
    c["entries"] = [r.choice([0x20, 0, 0x21, MASK, MASK - 0x20]) for _ in range(NTEX)]
    chain = []
    for _ in range(kw.get("chain", r.randrange(0, 4))):
        nf = r.randrange(0, 4)
        fsets = []
        for _ in range(nf):
            cnt = r.randrange(0, 4)
            fsets.append((r.randrange(0, NTEX), r.randrange(0, NFACES - cnt + 1), cnt))
        faces = [[r.choice([r.randrange(0, 12), r.randrange(0, 256)]) for _ in range(3)] for _ in range(NFACES)]
        chain.append({"fsets": fsets, "faces": faces, "uvs": [rb(r) for _ in range(NUV * 8)]})
    c["chain"] = chain
    c["queues"] = {"GetRideAnimTime": [now]}
    return c


def uv_edges():
    r = random.Random(642)
    one = fb(1.0)
    just = 0x3F800001
    for offs in ([one], [just], [fb(2.5), fb(0.5)], [fb(-1.5)], [0x7FC00000], [0x7F800000], [fb(1e10)], [0]):
        for f2c, delta in ((fb(0.0), 0), (fb(1.0), 1000), (fb(2.0), 333), (fb(0.5), 123456789)):
            yield uv_case(r, offs=offs, f2c=f2c, delta=delta, chain=2)
    yield uv_case(r, count=0, chain=0)
    yield uv_case(r, count=3, chain=3, delta=1000, f2c=fb(1.0))
    for _ in range(6):
        c = uv_case(r, chain=2)
        c["entries"] = [0x20] * NTEX
        yield c


def uv_random(r):
    return uv_case(r)


TESTS = [
    FnTest("PlaceEndPylons", ppc, host, edge=pep_edges, rnd=pep_random),
    FnTest("CoasterUVAnimTick", ppc, host, edge=uv_edges, rnd=uv_random),
]
