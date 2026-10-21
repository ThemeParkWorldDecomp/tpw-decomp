"""Unit v5752, the ride base members of CMesh, against libtpw_unit_CMesh_GenerateRideBase.dylib (tests/unit_CMesh_GenerateRideBase.cpp), engine,
src/engine/Mesh.cpp:
  CMesh::GenerateRideBase 0x1001e644 (one overlay for a texture entry), CMesh::GenerateRideBase 0x1001e72c (the cells of a rectangle of the
  height field get generated textures), CMesh::CenterMatrix 0x1001d97c.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back.
The callees outside the unit are fakes on both sides: CTextureEntry::GetCorrectedData, CProcTextureHandler::CreateProcTexture,
CTextureCache::Remove, CsTexture::Load and Close, CErrorHandler::Push, CMesh::DeleteRideBase, CMesh::SetLocalMatrix, the array new and
delete (the log says kind, element count and whether the allocation worked), the assertion. Entries are named in the log by a marker
word (their flags), elements of the new CsTexture array by their index.

The world of a case: a height field of W x H cells (flags and texture index), a texture record, CsTexture and entry for each cell, the mesh
(flags, possibly set arrays), the globals gei_HField, gei_TCache and the texture format. What is compared: the answer, the log, the
assertions, the mesh's flags and count and arrays, the texture records after the call.

Fused operations: CenterMatrix (fnmsubs), run with HardwareRounding. Not tested: a texture record used by two qualifying cells (the
second visit reads the entries of a fresh CsTexture, which has none: a NULL read), one of the two arrays failing alone (a NULL
write in the original), more than 12 cells (the host's old array and the entry array are told apart by size).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_CMesh_GenerateRideBase"
MASK = 0xFFFFFFFF

FN_A, FN_B, FN_C = 0x1001E644, 0x1001E72C, 0x1001D97C
SIZES = {FN_A: 152, FN_B: 1080, FN_C: 372}
OWN = set(SIZES)

# callees
GETCORR = 0x10041334
CREATEPROC = 0x1001310C
REMOVE = 0x10034610
LOAD = 0x100168D4
CLOSE = 0x10016FC0
PUSH = 0x10008904
DELETERIDEBASE = 0x1001EBD0
SETLOCALMATRIX = 0x1001D3D0
G_NEWARR = 0x1004C000
G_CONSTRUCT = 0x1004C018
G_DESTROY = 0x1004BF40
KINDS = {0x100504C8: 1, 0x10050358: 2, 0x100508B0: 3}
DTOR_CSTEXTURE = 0x10050350

ADDR = dict(hfield=0x100AF11C, tcache=0x10173184, settings=0x10165FC8, errh=0x10052B44)
TABLE = 0x10051CE0

L_NAMES = ["mesh.flags", "mesh.header", "mesh.old", "mesh.tex", "mesh.count", "cs.size", "cs.entries", "hf.f00", "hf.proc", "te.flags",
           "te.width", "te.height", "old.size", "old.mold", "old.mwhere"]
MAC = dict(zip(L_NAMES, [4, 8, 0x124, 0x128, 0x12C, 0x10, 8, 0, 0x230, 8, 0xC, 0xE, 8, 0, 4]))


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
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
CUR = {}


def marker(e, a):
    return e.u32(a + 8) if a else MASK


def idx(e, a):
    base = e.u32(CUR["mesh"] + MAC["mesh.tex"])
    d = (a - base) & MASK
    return s32(d) // 16


def st_getcorr(e):
    L = e.cl
    L.rec("GetCorrectedData", (marker(e, e.r(3)),))
    e.set_r(3, L.ret("GetCorrectedData") & MASK)


def st_createproc(e):
    L = e.cl
    o = e.r(7)
    L.rec("CreateProcTexture", (L.lab(e.r(3)), marker(e, e.r(4)), s32(e.r(5)), s32(e.r(6)), e.u32(o), s32(e.u32(o + 4)), s32(e.u32(o + 8)),
                                s32(e.u32(o + 12)), e.u32(o + 16)))
    e.set_r(3, L.ret("CreateProcTexture") & MASK)


def st_remove(e):
    L = e.cl
    L.rec("Remove", (L.lab(e.r(3)), marker(e, e.r(4))))
    e.set_r(3, 0)


def st_load(e):
    L = e.cl
    L.rec("Load", (idx(e, e.r(3)), marker(e, e.r(4))))
    e.set_r(3, 1)


def st_close(e):
    L = e.cl
    L.rec("Close", (idx(e, e.r(3)),))
    e.set_r(3, 1)


def st_push(e):
    L = e.cl
    L.rec("Push", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_delete(e):
    e.cl.rec("DeleteRideBase", (0,))
    e.set_r(3, 1)


def st_setlocal(e):
    L = e.cl
    ws = [canon_f(e.u32(e.r(5) + 4 * i)) for i in range(16)]
    L.rec("SetLocalMatrix", tuple([s32(e.r(4))] + ws[:8]))
    L.rec("SetLocalMatrix2", tuple(ws[8:]))


def st_newarr(e):
    ok = e.cl.ret("alloc") != 0
    size = e.r(3)
    e.cl.pending = ok
    if ok:
        a = e.alloc(size + 8)
        e.write(a, bytes(size + 8))
        e.set_r(3, a)
    else:
        e.set_r(3, 0)


def st_construct(e):
    L = e.cl
    mem, kind, size, count = e.r(3), KINDS[e.r(4)], e.r(6), e.r(7)
    L.rec("new[]", (kind, s32(count), 1 if mem else 0))
    if mem:
        e.write(mem, struct.pack(">II", size, count))
        e.set_r(3, mem + 8)
    else:
        e.set_r(3, 0)


def st_destroy(e):
    L = e.cl
    ptr, dtor = e.r(3), e.r(4)
    if ptr:
        if dtor == DTOR_CSTEXTURE:
            count = e.u32(ptr - 4)
            for i in range(count - 1, -1, -1):
                L.rec("Close", (idx(e, ptr + 16 * i),))
            L.rec("delete[]", ())      # the host's delete of the old array is the arena's own, which records nothing


def fnv(text):
    h = 2166136261
    if isinstance(text, str):
        text = text.encode("latin-1")
    for b in text:
        h = ((h ^ b) * 16777619) & MASK
    return h


def st_assert(e):
    e.cl.rec("assert", (s32(e.r(5)), fnv(e.cstr(e.r(3))), fnv(e.cstr(e.r(4)))))


def glue(fn):
    """A fake of a glue function: the glue saves the caller's TOC at 0x14(r1), which the caller reloads after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(0x1004BF10, glue(st_assert))
STUBS.internal(GETCORR, st_getcorr)
STUBS.internal(CREATEPROC, st_createproc)
STUBS.internal(REMOVE, st_remove)
STUBS.internal(LOAD, st_load)
STUBS.internal(CLOSE, st_close)
STUBS.internal(PUSH, st_push)
STUBS.internal(DELETERIDEBASE, st_delete)
STUBS.internal(SETLOCALMATRIX, st_setlocal)
STUBS.internal(G_NEWARR, glue(st_newarr))
STUBS.internal(G_CONSTRUCT, glue(st_construct))
STUBS.internal(G_DESTROY, glue(st_destroy))


def setup(runner):
    T.install_std_stubs(runner.emu)
    print("format table", [T.s32(runner.emu.u32(TABLE + 4 * i)) for i in range(16)])


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def o(self, name):
        return self.lay[name]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.lay = MAC
        self.psize = 4
        self.tok = {"handler": e.alloc(16), "cache": e.alloc(16)}
        self.L = e.cl = CL.PpcLog({self.tok["handler"]: 1, self.tok["cache"]: 2, ADDR["errh"]: 3}, case["queues"])
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

    def setptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_word(a)

    setp32 = setptr
    getp32 = getptr

    def log(self):
        return self.L.log

    def call_a(self, w, case):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_A, [w["mesh"], w["ents"][0], w["ents"][1]] + [v & MASK for v in case["args"]])
        return r3 & MASK

    def call_b(self, w, case):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_B, [w["mesh"]] + [v & MASK for v in case["rng"]])
        return r3 & MASK

    def call_c(self, w, case):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_C, [w["mesh"], case["cflags"] & MASK, T.F(bitsf(case["scale"]))])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        self.psize = 8
        self.tok = {"handler": self.alloc(16), "cache": self.alloc(16)}
        CL.host_begin(lib, {1: self.tok["handler"], 2: self.tok["cache"], 3: self.g("errh")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def setp32(self, a, v):
        self.words(a, [(v - self.lib.tpw_gr_base()) if v else 0])

    def getp32(self, a):
        v = self.rd_word(a)
        return v + self.lib.tpw_gr_base() if v else 0

    def log(self):
        return CL.host_end(self.lib)

    def call_a(self, w, case):
        return self.lib.tpw_gr_a(w["mesh"], w["ents"][0], w["ents"][1], *[s32(v) for v in case["args"]]) & MASK

    def call_b(self, w, case):
        return self.lib.tpw_gr_b(w["mesh"], *[s32(v) for v in case["rng"]]) & MASK

    def call_c(self, w, case):
        self.lib.tpw_gr_c(w["mesh"], s32(case["cflags"]), bitsf(case["scale"]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gr_addr.restype, lib.tpw_gr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gr_alloc.restype, lib.tpw_gr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gr_base.restype, lib.tpw_gr_base.argtypes = C.c_uint64, []
    lib.tpw_gr_layout.argtypes = [C.POINTER(C.c_int64)]
    P = C.c_uint64
    lib.tpw_gr_a.restype, lib.tpw_gr_a.argtypes = C.c_int32, [P, P, P, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_gr_b.restype, lib.tpw_gr_b.argtypes = C.c_int32, [P, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_gr_c.restype, lib.tpw_gr_c.argtypes = None, [P, C.c_int32, C.c_float]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gr_layout(out)
    assert n == len(L_NAMES), n
    lib.lay = dict(zip(L_NAMES, list(out)[:n]))
    assert lib.lay["cs.size"] > 0
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    W, H = case["W"], case["H"]
    n = W * H
    w = {}
    ents, css = [], []
    for i in range(n):
        ent = s.alloc(0x100)
        s.words(ent + s.o("te.flags"), [0x1000 + i])
        s.halves(ent + s.o("te.width"), [case["tw"][i], case["th"][i]])
        arr = s.alloc(16)
        s.setptr(arr, ent)
        cs = s.alloc(0x40)
        s.setptr(cs + s.o("cs.entries"), arr)
        ents.append(ent)
        css.append(cs)
    texarr = s.alloc(8 * n + 8)
    for i in range(n):
        s.setp32(texarr + 8 * i + 4, css[i])
    hdr = s.alloc(0x100)
    s.setp32(hdr + 0x50, texarr)
    s.words(hdr + 0x80, case["box"])
    quads = s.alloc(4 * n + 4)
    for i in range(n):
        s.halves(quads + 4 * i, [case["cells"][i], i])
    shf = s.alloc(0x40)
    s.words(shf + 0x18, [W])
    s.setp32(shf + 0x2C, quads)
    hf = s.alloc(0x400)
    s.setptr(hf + s.o("hf.f00"), shf)
    s.setptr(hf + s.o("hf.proc"), s.tok["handler"])
    mesh = s.alloc(0x400)
    s.words(mesh + s.o("mesh.flags"), [case["mflags"]])
    s.setptr(mesh + s.o("mesh.header"), hdr)
    if case["old_pre"]:
        s.setptr(mesh + s.o("mesh.old"), s.alloc(32))
    if case["tex_pre"]:
        s.setptr(mesh + s.o("mesh.tex"), s.alloc(32))
    s.setptr(s.g("hfield"), hf)
    s.setptr(s.g("tcache"), s.tok["cache"])
    s.words(s.g("settings") + 4, [case["fmt"]])
    w.update(mesh=mesh, ents=ents, css=css, texarr=texarr)
    CUR["mesh"] = mesh
    return w


def execute(s, case, w):
    return {"a": s.call_a, "b": s.call_b, "c": s.call_c}[case["op"]](w, case)


def observe(s, case, w, answer):
    mesh = w["mesh"]
    n = case["W"] * case["H"]
    oldp = s.getptr(mesh + s.o("mesh.old"))
    texp = s.getptr(mesh + s.o("mesh.tex"))
    count = s32(s.rd_word(mesh + s.o("mesh.count")))
    stride = 16 if s.psize == 4 else s.o("cs.size")
    css = w["css"]

    def cls(v):
        if v == 0:
            return 0
        if v in css:
            return ("o", css.index(v))
        if texp and 0 <= v - texp < 12 * stride + 64:
            return ("n", (v - texp) // stride)
        return ("?", v)

    d = {"answer": answer, "log": s.log(), "flags": s.rd_word(mesh + s.o("mesh.flags")), "count": count,
         "old_set": oldp != 0, "tex_set": texp != 0,
         "slots": [cls(s.getp32(w["texarr"] + 8 * i + 4)) for i in range(n)]}
    olds = []
    freed = 0 in case["queues"]["alloc"][2:]       # an entry that could not be made: the old array was deleted (the host's block is gone)
    if oldp and 0 < count <= 12 and case["op"] == "b" and not freed:
        for k in range(count):
            o = oldp + k * s.o("old.size")
            mold = s.getptr(o + s.o("old.mold"))
            where = s.getptr(o + s.o("old.mwhere"))
            olds.append((cls(mold), (where - w["texarr"] - 4) // 8 if where else None))
    d["olds"] = olds
    return d


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, case, w, execute(s, case, w))
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
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op, W=None, H=None, cell=None):
    if W is None:
        W = r.randint(1, 4)
    if H is None:
        H = r.randint(1, 3 if W == 4 else 4)
    n = W * H
    cells = []
    for _ in range(n):
        if cell is not None:
            cells.append(cell)
        else:
            cells.append(r.choice([0x280, 0x280, 0x280, 0x80, 0x200, 0, 0x8280, 0x3FF, 0x7D, r.getrandbits(16)]))
    return {"op": op, "W": W, "H": H, "cells": cells, "tw": [r.choice([0, 1, 8, 64, 255, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]) for _ in range(n)],
            "th": [r.getrandbits(16) for _ in range(n)], "box": [rf(r) for _ in range(6)], "fmt": r.randint(0, 5),
            "mflags": r.choice([0x10000000, 0x10000000, 0x10000000, 0x30000000, 0, 0xFFFFFFFF]), "old_pre": False, "tex_pre": False,
            "queues": {"alloc": [1], "GetCorrectedData": [r.getrandbits(32) for _ in range(14)],
                       "CreateProcTexture": [r.choice([1, 1, 1, 0, 3, 2]) for _ in range(14)]}}


def rng_for(r, c):
    x0 = r.randint(0, c["W"] - 1)
    x1 = r.randint(x0 - 1, c["W"] - 1)
    y0 = r.randint(0, c["H"] - 1)
    y1 = r.randint(y0 - 1, c["H"] - 1)
    return [x0, y0, x1, y1]


# ---- 0x1001e644
def a_case(r):
    c = base(r, "a", 2, 1)
    c["args"] = [r.choice([0, 1, -1, 7, 255]), r.choice([0, 1, -1, 9]), r.choice([0, 1, 0x280, MASK, r.getrandbits(32)])]
    return c


def a_edges():
    r = random.Random(11)
    for fmt in range(6):
        c = a_case(r)
        c["fmt"] = fmt
        yield c
    for q in ([0], [1], [2], [3], [MASK]):
        c = a_case(r)
        c["queues"]["CreateProcTexture"] = q
        yield c
    for tw, th in ((0x7FFF, 0x8000), (0x8000, 0x7FFF), (0xFFFF, 0)):
        c = a_case(r)
        c["tw"], c["th"] = [tw, tw], [th, th]
        yield c


# ---- 0x1001e72c
def b_case(r):
    c = base(r, "b")
    c["rng"] = rng_for(r, c)
    k = r.random()
    if k < 0.1:
        c["queues"]["alloc"] = [0, 0, 1]
    elif k < 0.25:
        c["queues"]["alloc"] = [1, 1] + [1] * r.randint(0, 6) + [0]
    if r.random() < 0.1:
        c["old_pre"] = r.random() < 0.7
        c["tex_pre"] = not c["old_pre"] or r.random() < 0.5
    return c


def b_edges():
    r = random.Random(22)
    for W, H in ((1, 1), (2, 2), (3, 3), (4, 3), (3, 4), (1, 4), (4, 1)):
        c = base(r, "b", W, H, 0x280)
        c["rng"] = [0, 0, W - 1, H - 1]
        yield c
    for cell in (0x80, 0x200, 0, 0x8280, 0x280 | 0x1000):
        c = base(r, "b", 3, 2, cell)
        c["rng"] = [0, 0, 2, 1]
        yield c
    # sub-rectangles, empty ranges
    for rng in ([1, 0, 2, 1], [0, 1, 2, 1], [2, 1, 2, 1], [1, 1, 0, 1], [0, 1, 2, 0], [2, 1, 0, 0]):
        c = base(r, "b", 3, 2, 0x280)
        c["rng"] = rng
        yield c
    # mixed cells
    c = base(r, "b", 3, 2)
    c["cells"] = [0x280, 0x80, 0x200, 0x280, 0, 0x8280]
    c["rng"] = [0, 0, 2, 1]
    yield c
    # allocation failures
    for q in ([0, 0, 1], [0, 0, 0], [1, 1, 0], [1, 1, 1, 0], [1, 1, 1, 1, 0], [1, 1, 1, 1, 1, 1, 1, 0]):
        c = base(r, "b", 3, 2, 0x280)
        c["rng"] = [0, 0, 2, 1]
        c["queues"]["alloc"] = q
        yield c
    # no qualifying cell and a failure (both fail with count 0)
    c = base(r, "b", 2, 2, 0)
    c["rng"] = [0, 0, 1, 1]
    c["queues"]["alloc"] = [0, 0, 1]
    yield c
    # the assertion: not initialised, an array set
    for mf, op, tp in ((0, False, False), (0x10000000, True, False), (0x10000000, False, True), (0x10000000, True, True), (0x20000000, False, False)):
        c = base(r, "b", 2, 2, 0x280)
        c["rng"] = [0, 0, 1, 1]
        c["mflags"], c["old_pre"], c["tex_pre"] = mf, op, tp
        yield c
    # the flags: 0x10000000 kept, 0x20000000 added only when every overlay answered with bit 0
    for q in ([1], [0], [1, 0], [3], [2, 1], [1, 1, 3]):
        c = base(r, "b", 2, 2, 0x280)
        c["rng"] = [0, 0, 1, 1]
        c["queues"]["CreateProcTexture"] = q
        yield c
    for fmt in range(6):
        c = base(r, "b", 2, 1, 0x280)
        c["rng"] = [0, 0, 1, 0]
        c["fmt"] = fmt
        yield c


# ---- 0x1001d97c
def c_case(r):
    c = base(r, "c", 1, 1)
    c["cflags"] = r.choice([0, 1, 2, 3, MASK, r.getrandbits(32)])
    c["scale"] = rf(r)
    return c


def c_edges():
    r = random.Random(33)
    for flags in (0, 1, 2, 3, MASK, 0x80000000):
        c = c_case(r)
        c["cflags"] = flags
        c["box"] = [fb(-1.0), fb(-2.0), fb(-4.0), fb(1.0), fb(2.0), fb(4.0)]
        c["scale"] = fb(1.0)
        yield c
    for box in ([0, 0, 0, 0, 0, 0], [fb(1.0)] * 3 + [fb(1.0)] * 3, [fb(0.0), fb(0.0), fb(0.0), fb(10.0), fb(0.1), fb(300.0)],
                [fb(3.3), fb(-7.7), fb(0.01), fb(100.5), fb(-6.1), fb(0.02)], [0x7F800000, 0, 0, 0, 0, 0], [0x7FC00000] * 6):
        c = c_case(r)
        c["box"] = box
        yield c
    for scale in (0, 0x80000000, 0x7F800000, fb(0.5), fb(1.0), fb(-1.0), fb(1e-30), fb(1e30)):
        c = c_case(r)
        c["scale"] = scale
        yield c


TESTS = [
    FnTest("CMesh::GenerateRideBase@0x1001e644", ppc, host, edge=a_edges, rnd=a_case),
    FnTest("CMesh::GenerateRideBase@0x1001e72c", ppc, host, edge=b_edges, rnd=b_case),
    FnTest("CMesh::CenterMatrix", ppc, host, edge=c_edges, rnd=c_case),
]
