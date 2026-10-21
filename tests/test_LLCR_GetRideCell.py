"""Unit v51063, small accessors, static initialisers and the track animation walk, against libtpw_unit_LLCR_GetRideCell.dylib
(tests/unit_LLCR_GetRideCell.cpp), main app ("SimTheme Park"), src/LLCR_OverRide_LLCR_OverRide_3_LLCR_GetRideCell.cpp:
  LLCR_GetRideCell 0x1008bccc, LLCR_GetRideCellDirection 0x1008bd14, LLCR_SetTopLeft 0x1008bd68, LLCR_GetWidthHeight 0x1008bdcc,
  LLCC_SetTrackAnim 0x1008c7b0, __sinit_LLCellMouse_cpp 0x10088ff4, LLCM_GetIllegalPlacement 0x10088970, LLCM_GetCurrentObject 0x100888fc,
  LLCCM_DestroyHoardingMesh 0x1008288c, LLCCM_DestroyWTrackMesh 0x10082508, LLCCM_DestroyKTrackMesh 0x10082120, LLCCM_DestroyQMesh 0x10081d80,
  __sinit_intrface_cpp 0x1007de28, ESP_GetNumberOfCustomAnims 0x1007c990, ESP_GetNumberOfSprites 0x1007c8b0, DrawHLSprite 0x1007b114,
  ESP_DisableSpriteDraw 0x1007aca4, ESP_EnableSpriteDraw 0x1007ac68, FLY_SetMotionScale 0x10079168, FLY_SetCarSize 0x10079114

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees of LLCC_SetTrackAnim
(LLCR_OffsetStartOfTrack, GetMapCellEquiv, IsTrack, IsCorner, OPPOSITE, GetNeighbours, SetCurRideAnim, GetNeighbouringCell, IsTrackStartBlock) and
DestroyRideInstance are fakes on both sides that record their arguments (cells as offsets from the world) and answer from the case's queues.

The world of a case: the globals each function reads (random words), the tables (ride record, ESP groups and sprites, fly bases, ride anim
sets), a world block of track cells with mesh and parent words. What is compared: the answer (when the original defines one), the globals and
blocks written, and the event log. Not tested: the return value of FLY_SetMotionScale / FLY_SetCarSize / DrawHLSprite etc. (the original leaves
r3 as it was); no fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_GetRideCell"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF

FN = {
    "LLCR_GetRideCell": 0x1008bccc, "LLCR_GetRideCellDirection": 0x1008bd14, "LLCR_SetTopLeft": 0x1008bd68,
    "LLCR_GetWidthHeight": 0x1008bdcc, "LLCC_SetTrackAnim": 0x1008c7b0, "__sinit_LLCellMouse_cpp": 0x10088ff4,
    "LLCM_GetIllegalPlacement": 0x10088970, "LLCM_GetCurrentObject": 0x100888fc, "LLCCM_DestroyHoardingMesh": 0x1008288c,
    "LLCCM_DestroyWTrackMesh": 0x10082508, "LLCCM_DestroyKTrackMesh": 0x10082120, "LLCCM_DestroyQMesh": 0x10081d80,
    "__sinit_intrface_cpp": 0x1007de28, "ESP_GetNumberOfCustomAnims": 0x1007c990, "ESP_GetNumberOfSprites": 0x1007c8b0,
    "DrawHLSprite": 0x1007b114, "ESP_DisableSpriteDraw": 0x1007aca4, "ESP_EnableSpriteDraw": 0x1007ac68,
    "FLY_SetMotionScale": 0x10079168, "FLY_SetCarSize": 0x10079114,
}
ARGC = {"LLCR_GetRideCell": 2, "LLCR_GetRideCellDirection": 2, "LLCR_SetTopLeft": 1, "LLCR_GetWidthHeight": 2, "LLCC_SetTrackAnim": 2,
        "__sinit_LLCellMouse_cpp": 0, "LLCM_GetIllegalPlacement": 0, "LLCM_GetCurrentObject": 0, "LLCCM_DestroyHoardingMesh": 1,
        "LLCCM_DestroyWTrackMesh": 1, "LLCCM_DestroyKTrackMesh": 1, "LLCCM_DestroyQMesh": 1, "__sinit_intrface_cpp": 0,
        "ESP_GetNumberOfCustomAnims": 2, "ESP_GetNumberOfSprites": 1, "DrawHLSprite": 1, "ESP_DisableSpriteDraw": 0,
        "ESP_EnableSpriteDraw": 0, "FLY_SetMotionScale": 2, "FLY_SetCarSize": 2}
OWN = set(FN.values())
# functions whose answer the original defines
ANSWERS = {"LLCR_GetRideCell", "LLCR_GetRideCellDirection", "LLCM_GetIllegalPlacement", "LLCM_GetCurrentObject",
           "ESP_GetNumberOfCustomAnims", "ESP_GetNumberOfSprites"}

ADDR = dict(illegal=0x102dc218, curobj=0x102ddb6c, ce092=0x102de092, ce094=0x102de094, db58=0x102ddb58, db5a=0x102ddb5a, db5c=0x102ddb5c,
            db68=0x102ddb68, db74=0x102ddb74, db78=0x102ddb78, dc1d8=0x102dc1d8, dc1dc=0x102dc1dc, intrface=0x102dc1ac,
            espdraw=0x1029d364, groups=0x1029d368, sprites=0x102db520, flybases=0x102da964, anims=0x102c79a4, world=0x1037ebfc)

NCELLS = 10752
OPP = {1: 0x10, 0x10: 1, 4: 0x40, 0x40: 4}
HL = {}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
WORLD = [0]


def rel(a):
    return s32((a - WORLD[0]) & MASK)


def st_offset(e):
    L = e.cl
    px, py = e.r(3), e.r(4)
    L.rec("OffsetStart", (s32(e.s32(px)), s32(e.s32(py)), s32(e.r(5))))
    e.write(px, struct.pack(">i", s32(L.ret("OffsetX"))))
    e.write(py, struct.pack(">i", s32(L.ret("OffsetY"))))
    e.set_r(3, L.ret("OffsetDir") & 0xFF)


def st_mapcell(e):
    e.cl.rec("MapCellEquiv", (rel(e.r(3)),))
    e.set_r(3, 0)


def st_istrack(e):
    e.cl.rec("IsTrack", (rel(e.r(3)),))
    e.set_r(3, e.cl.ret("IsTrack") & MASK)


def st_iscorner(e):
    e.cl.rec("IsCorner", (rel(e.r(3)),))
    e.set_r(3, e.cl.ret("IsCorner") & 0xFF)


def st_istsb(e):
    e.cl.rec("IsTrackStartBlock", (rel(e.r(3)),))
    e.set_r(3, e.cl.ret("IsTrackStartBlock") & MASK)


def st_neighbours(e):
    e.cl.rec("GetNeighbours", (rel(e.r(3)),))
    e.set_r(3, e.cl.ret("GetNeighbours") & 0xFF)


def st_neighcell(e):
    L = e.cl
    L.rec("NeighbouringCell", (rel(e.r(3)), e.r(4) & 0xFF, s32(e.r(5))))
    v = L.ret("NeighbouringCell")
    e.set_r(3, 0 if v == 0 else (WORLD[0] + 0x1102D8 + (v - 1) * 0x28) & MASK)


def st_opposite(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, OPP.get(d, 0))


def st_anim(e):
    e.cl.rec("SetCurRideAnim", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, fb(e.f(1)), e.r(8) & MASK))


def st_destroy(e):
    e.cl.rec("DestroyRideInstance", (e.r(3) & MASK,))


STUBS.internal(0x1008C620, st_offset)
STUBS.internal(0x100F49A0, st_mapcell)
STUBS.internal(0x100A3574, st_istrack)
STUBS.internal(0x100A37A0, st_iscorner)
STUBS.internal(0x100A36CC, st_istsb)
STUBS.internal(0x100816E8, st_neighbours)
STUBS.internal(0x1012FDDC, st_neighcell)
STUBS.internal(0x10102500, st_opposite)
STUBS.internal(0x100C6670, st_anim)
STUBS.internal(0x1006A3EC, st_destroy)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def ptrs(self, a, vals):
        """A table of pointers (4 bytes each on the Mac, 8 on the host; the arena is 32-bit addressable)."""
        if self.psz == 4:
            self.words(a, vals)
        else:
            self.write(a, b"".join(struct.pack("<Q", v & M64) for v in vals))


class Ppc(Side):
    fmt = ">I"
    psz = 4
    gsz, gfirst, gcount, spr_cc, intr24, intr28 = 12, 4, 8, 0x20C, 0x24, 0x28

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

    def set_shape(self, blk):
        self.words(self.g("ce094"), [blk])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, FN[op], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.gsz, self.gfirst, self.gcount, self.spr_cc, self.intr24, self.intr28 = HL["gsz"], HL["gfirst"], HL["gcount"], HL["cc"], HL["i24"], HL["i28"]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_llcr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_llcr_alloc(n)

    def set_shape(self, blk):
        # the host reads the shape through the whole pointer LLCR_SetRide keeps beside the word (unit v5L28)
        self.words(self.g("ce094"), [blk & MASK])
        self.write(self.g("shape"), struct.pack("<Q", blk))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        return getattr(self.lib, "tpw_llcr_" + op)(*[a & M64 for a in args]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_llcr_addr.restype, lib.tpw_llcr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_llcr_alloc.restype, lib.tpw_llcr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_llcr_offset.restype, lib.tpw_llcr_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_llcr_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, n in ARGC.items():
        f = getattr(lib, "tpw_llcr_" + name)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64] * n
    out = (C.c_int64 * 32)()
    n = lib.tpw_llcr_layout(out)
    v = list(out)[:n]
    HL.update(gsz=v[0], gfirst=v[1], gcount=v[2], cc=v[3], i24=v[6], i28=v[7])
    assert v[2] - v[1] == 4 and v[4] == 0x114 and v[5] == 0x118, v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

_BLOBS = {}


def cells_blob(seed, fmt):
    """NCELLS track cells of 0x28 bytes: the mesh word at +4 (0 to 7) and the parent half at +0x10 (0 to 2)."""
    key = (seed, fmt)
    if key not in _BLOBS:
        b = bytearray(NCELLS * 0x28)
        for i in range(NCELLS):
            h = (i * 2654435761 + seed * 40503) & MASK
            struct.pack_into(fmt[0] + "I", b, i * 0x28 + 4, (h >> 7) % 8)
            struct.pack_into(fmt[0] + "H", b, i * 0x28 + 0x10, (h >> 11) % 3)
        _BLOBS[key] = bytes(b)
    return _BLOBS[key]


def build(s, case):
    ctx = {}
    for name, (kind, v) in case.get("g", {}).items():
        a = s.g(name)
        if kind == "w":
            s.words(a, [v])
        elif kind == "h":
            s.half(a, v)
        else:
            s.write(a, bytes([v & 0xFF]))
    op = case["op"]
    if op in ("LLCR_GetRideCell", "LLCR_GetRideCellDirection", "LLCR_GetWidthHeight"):
        blk = s.alloc(0x2C0)
        if op == "LLCR_GetRideCellDirection":      # a byte read: the same bytes on both sides
            s.write(blk, b"".join(struct.pack(">I", w) for w in case["blk"]))
        else:
            s.words(blk, case["blk"])
        s.set_shape(blk)
        if op == "LLCR_GetWidthHeight":
            ctx["out"] = s.alloc(8)
            s.words(ctx["out"], [0xDEADBEEF, 0x12345678])
    elif op == "__sinit_intrface_cpp":
        h, b0, b1, w = case["intr"]
        a = s.g("intrface")
        s.half(a + s.intr24, h)
        s.write(a + s.intr24 + 2, bytes([b0, b1]))
        s.words(a + s.intr28, [w])
    elif op in ("ESP_GetNumberOfCustomAnims", "ESP_GetNumberOfSprites"):
        ga = s.g("groups")
        for i, (first, count) in enumerate(case["groups"]):
            s.words(ga + i * s.gsz + s.gfirst, [first])
            s.words(ga + i * s.gsz + s.gfirst + 4, [count])
        if op == "ESP_GetNumberOfCustomAnims":
            blocks = []
            for v in case["sprites"]:
                b = s.alloc(0x240)
                s.half(b + s.spr_cc, v)
                blocks.append(b)
            s.ptrs(s.g("sprites"), blocks)
    elif op == "DrawHLSprite":
        ctx["script"] = s.alloc(0x118)
        s.words(ctx["script"], case["script"])
    elif op in ("FLY_SetMotionScale", "FLY_SetCarSize"):
        vals = [0] * 100
        ctx["slots"] = sorted(case["slots"])
        for slot in ctx["slots"]:
            b = s.alloc(0x40)
            s.words(b, case["slots"][slot])
            vals[slot] = b
            ctx[slot] = b
        s.ptrs(s.g("flybases"), vals)
    elif op == "LLCC_SetTrackAnim":
        w = s.alloc(0x1102D8 + 0x28 * NCELLS + 0x40)
        s.write(w + 0x1102D8, cells_blob(case["seed"], s.fmt))
        s.ptrs(s.g("world"), [w])
        WORLD[0] = w
        s.ptrs(s.g("anims"), [0] + [0x2000 + 0x10 * i for i in range(1, 8)] + [0] * 8)
        obj = s.alloc(0x20)
        s.words(obj, case["obj"])
        s.write(obj + 4, bytes([case["b4"]]))
        s.write(obj + 6, bytes([case["b6"]]))
        ctx["obj"] = obj
    return ctx


def execute(s, case, ctx):
    op = case["op"]
    if op == "LLCR_GetWidthHeight":
        args = [ctx["out"], ctx["out"] + 4]
    elif op == "LLCC_SetTrackAnim":
        args = [ctx["obj"], case["flag"]]
    elif op == "DrawHLSprite":
        args = [ctx["script"]]
    else:
        args = case.get("args", [])
    r = s.call(op, args)
    return r if op in ANSWERS else None


def observe(s, case, ctx, answer):
    op = case["op"]
    o = {"answer": answer, "log": s.log()}
    if op == "LLCR_GetWidthHeight":
        o["out"] = s.rd_words(ctx["out"], 2)
    elif op == "LLCR_SetTopLeft":
        o["ce092"] = s.rd_half(s.g("ce092"))
    elif op == "__sinit_LLCellMouse_cpp":
        for n in ("db58", "db5a", "db5c", "db68"):
            o[n] = s.rd_half(s.g(n))
        for n in ("db74", "db78"):
            o[n] = s.rd_word(s.g(n))
    elif op == "__sinit_intrface_cpp":
        a = s.g("intrface")
        o["f24"] = s.rd_half(a + s.intr24)
        o["f26"] = list(s.read(a + s.intr24 + 2, 2))
        o["f28"] = s.rd_word(a + s.intr28)
        o["dc1d8"] = s.rd_word(s.g("dc1d8"))
        o["dc1dc"] = s.rd_word(s.g("dc1dc"))
    elif op == "DrawHLSprite":
        o["script"] = s.rd_words(ctx["script"], 0x118 // 4)
    elif op in ("ESP_DisableSpriteDraw", "ESP_EnableSpriteDraw"):
        o["draw"] = s.read(s.g("espdraw"), 1)[0]
    elif op in ("FLY_SetMotionScale", "FLY_SetCarSize"):
        o["blocks"] = [s.rd_words(ctx[slot], 16) for slot in ctx["slots"]]
    return o


def run(s, case):
    try:
        ctx = build(s, case)
        return observe(s, case, ctx, execute(s, case, ctx))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rw(r):
    return r.choice([0, 1, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)])


def words(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def base(op):
    return {"op": op, "queues": {}, "g": {}}


TESTS = []


def reg(name, gen, edges):
    def rnd(r):
        return gen(r)

    def edge():
        r = random.Random(sum(map(ord, name)))
        for kw in edges:
            yield gen(r, **kw)

    TESTS.append(FnTest(name, ppc, host, edge=edge, rnd=rnd))


# ---- ride cell accessors
def mk_cell(op):
    def gen(r, col=None, row=None):
        c = base(op)
        c["blk"] = words(r, 176)
        c["args"] = [r.randrange(20) if col is None else col, r.randrange(4) if row is None else row]
        return c
    return gen


CELL_EDGES = [dict(col=0, row=0), dict(col=1, row=0), dict(col=0, row=1), dict(col=19, row=3), dict(col=7, row=2), dict(col=19, row=0)]
reg("LLCR_GetRideCell", mk_cell("LLCR_GetRideCell"), CELL_EDGES)
reg("LLCR_GetRideCellDirection", mk_cell("LLCR_GetRideCellDirection"), CELL_EDGES)


def gen_wh(r):
    c = base("LLCR_GetWidthHeight")
    c["blk"] = words(r, 176)
    return c


reg("LLCR_GetWidthHeight", gen_wh, [{}])


def gen_topleft(r, idx=None, low=None):
    c = base("LLCR_SetTopLeft")
    c["g"]["ce092"] = ("h", r.getrandbits(16))
    c["args"] = [((r.getrandbits(16) if idx is None else idx) << 16) | (r.getrandbits(16) if low is None else low)]
    return c


reg("LLCR_SetTopLeft", gen_topleft, [dict(idx=0, low=0), dict(idx=1, low=0xFFFF), dict(idx=0xFFFF, low=0), dict(idx=0x8000, low=0x1234)])


# ---- globals
def gen_mouse_init(r):
    c = base("__sinit_LLCellMouse_cpp")
    for n in ("db58", "db5a", "db5c", "db68"):
        c["g"][n] = ("h", r.getrandbits(16))
    for n in ("db74", "db78"):
        c["g"][n] = ("w", rw(r))
    return c


reg("__sinit_LLCellMouse_cpp", gen_mouse_init, [{}])


def gen_intr_init(r):
    c = base("__sinit_intrface_cpp")
    c["g"]["dc1d8"] = ("w", rw(r))
    c["g"]["dc1dc"] = ("w", rw(r))
    c["intr"] = (r.getrandbits(16), r.getrandbits(8), r.getrandbits(8), rw(r))
    return c


reg("__sinit_intrface_cpp", gen_intr_init, [{}])


def gen_illegal(r, v=None):
    c = base("LLCM_GetIllegalPlacement")
    c["g"]["illegal"] = ("w", rw(r) if v is None else v)
    return c


reg("LLCM_GetIllegalPlacement", gen_illegal, [dict(v=0), dict(v=1), dict(v=MASK), dict(v=0x80000000)])


def gen_curobj(r, v=None):
    c = base("LLCM_GetCurrentObject")
    c["g"]["curobj"] = ("w", rw(r) if v is None else v)
    return c


reg("LLCM_GetCurrentObject", gen_curobj, [dict(v=0), dict(v=1), dict(v=MASK), dict(v=0x80000000)])


def mk_destroy(name):
    def gen(r, v=None):
        c = base(name)
        c["args"] = [rw(r) if v is None else v]
        return c
    return gen


for _n in ("LLCCM_DestroyHoardingMesh", "LLCCM_DestroyWTrackMesh", "LLCCM_DestroyKTrackMesh", "LLCCM_DestroyQMesh"):
    reg(_n, mk_destroy(_n), [dict(v=0), dict(v=1), dict(v=MASK), dict(v=0x1234)])


# ---- ESP
def mk_esp(op, two):
    def gen(r, s_=None, idx=None):
        c = base(op)
        c["groups"] = [(r.randrange(8), rw(r)) for _ in range(14)]
        c["sprites"] = [r.getrandbits(16) for _ in range(16)]
        c["args"] = [r.randrange(14) if s_ is None else s_] + ([r.randrange(8) if idx is None else idx] if two else [])
        return c
    return gen


reg("ESP_GetNumberOfCustomAnims", mk_esp("ESP_GetNumberOfCustomAnims", True),
    [dict(s_=0, idx=0), dict(s_=13, idx=7), dict(s_=1, idx=0), dict(s_=5, idx=3)])
reg("ESP_GetNumberOfSprites", mk_esp("ESP_GetNumberOfSprites", False), [dict(s_=0), dict(s_=13), dict(s_=1), dict(s_=7)])


def gen_script(r):
    c = base("DrawHLSprite")
    c["script"] = words(r, 0x118 // 4)
    return c


reg("DrawHLSprite", gen_script, [{}])


def mk_draw(op):
    def gen(r):
        c = base(op)
        c["g"]["espdraw"] = ("b", r.choice([0, 1, 2, 0xFF, r.getrandbits(8)]))
        return c
    return gen


reg("ESP_DisableSpriteDraw", mk_draw("ESP_DisableSpriteDraw"), [{}, {}])
reg("ESP_EnableSpriteDraw", mk_draw("ESP_EnableSpriteDraw"), [{}, {}])


# ---- FLY
def mk_fly(op):
    def gen(r, fly=None, filled=None):
        c = base(op)
        fly = r.choice([-1, 0, 1, 2, 50, 98, 99, 100, 101, r.randrange(-3, 105)]) if fly is None else fly
        slots = set(r.sample([1, 2, 50, 98, 99], 3))
        if 0 < fly < 100 and (filled if filled is not None else r.random() < 0.7):
            slots.add(fly)
        elif filled is False:
            slots.discard(fly)
        c["slots"] = {s_: words(r, 16) for s_ in slots}
        c["args"] = [fly, rw(r)]
        return c
    return gen


FLY_EDGES = [dict(fly=-1), dict(fly=0), dict(fly=1, filled=True), dict(fly=1, filled=False), dict(fly=99, filled=True), dict(fly=99, filled=False),
             dict(fly=100), dict(fly=101), dict(fly=2, filled=True), dict(fly=50, filled=True)]
reg("FLY_SetMotionScale", mk_fly("FLY_SetMotionScale"), FLY_EDGES)
reg("FLY_SetCarSize", mk_fly("FLY_SetCarSize"), FLY_EDGES)


# ---- LLCC_SetTrackAnim
DIRS = [0, 1, 4, 0x10, 0x40, 5, 0x11, 0x80, 2, 8, 0x41]
NEIGH = [0, 1, 4, 5, 0x10, 0x11, 0x14, 0x40, 0x41, 0x44, 0x15, 0x50, 0x55]


def gen_track(r, flag=None, istrack=None, corner=None, neigh=None, direction=None, cell=None, tsb=None):
    c = base("LLCC_SetTrackAnim")
    c["seed"] = r.randrange(6)
    c["obj"] = words(r, 8)
    c["b4"] = r.randrange(256)
    c["b6"] = r.randrange(256)
    c["flag"] = r.randrange(2) if flag is None else flag
    q = c["queues"]
    q["OffsetX"] = [r.randrange(40, 46)]
    q["OffsetY"] = [r.randrange(40, 46)]
    q["OffsetDir"] = [r.choice(DIRS) if direction is None else direction]
    q["IsTrack"] = [r.choice([0, 1, 1, 5, 0x100]) for _ in range(6)] if istrack is None else istrack
    q["IsCorner"] = ([r.choice([0, 0, 4, 5, 0x10]) for _ in range(r.randrange(1, 8))] if corner is None else corner) + [0]
    q["GetNeighbours"] = ([r.choice(NEIGH) for _ in range(r.randrange(1, 10))] if neigh is None else neigh) + [0]
    q["NeighbouringCell"] = [r.choice([0, 0, r.randrange(1, NCELLS - 200)]) for _ in range(6)] if cell is None else cell
    q["IsTrackStartBlock"] = [r.choice([0, 1, 1, 7]) for _ in range(6)] if tsb is None else tsb
    return c


TRACK_EDGES = [
    dict(flag=1, istrack=[0], corner=[0], neigh=[0]),
    dict(flag=0, istrack=[0], corner=[4], neigh=[5, 5]),
    dict(flag=1, istrack=[1], corner=[0, 0, 0], neigh=[1, 1, 0], direction=1, cell=[0]),
    dict(flag=0, istrack=[1], corner=[0, 0, 0], neigh=[4, 4, 0], direction=4, cell=[0]),
    dict(flag=1, istrack=[1], corner=[0, 0, 0], neigh=[0x10, 0x10, 0], direction=0x10, cell=[0]),
    dict(flag=0, istrack=[1], corner=[0, 0, 0], neigh=[0x40, 0x40, 0], direction=0x40, cell=[0]),
    dict(flag=1, istrack=[1], corner=[4, 0], neigh=[5, 5, 1, 0], direction=1, cell=[0]),
    dict(flag=0, istrack=[1], corner=[4, 0], neigh=[0x14, 0x14, 0x10, 0], direction=0x10, cell=[0]),
    dict(flag=1, istrack=[1], corner=[0, 0, 0], neigh=[1, 1, 1, 0], direction=1, cell=[5], tsb=[1]),
    dict(flag=1, istrack=[1], corner=[0, 0, 0], neigh=[1, 1, 0, 0], direction=1, cell=[5], tsb=[0]),
    dict(flag=0, istrack=[1], corner=[0, 0, 0], neigh=[4, 4, 0x40, 0], direction=4, cell=[1], tsb=[1]),
    dict(flag=0, istrack=[1], corner=[0, 0, 0], neigh=[4, 4, 0, 0], direction=4, cell=[1], tsb=[1]),
    dict(flag=1, istrack=[1], corner=[0, 0, 0], neigh=[0x80, 0x80, 0], direction=0x80, cell=[0]),
    dict(flag=1, istrack=[1], corner=[0, 0, 0], neigh=[0, 0], direction=0, cell=[0]),
]
reg("LLCC_SetTrackAnim", gen_track, TRACK_EDGES)
