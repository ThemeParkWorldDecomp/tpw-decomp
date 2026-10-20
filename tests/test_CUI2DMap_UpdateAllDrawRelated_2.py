"""Unit v5965, CUI2DMap::UpdateAllDrawRelated 0x1022e2c4 (3688 bytes), against libtpw_unit_CUI2DMap_UpdateAllDrawRelated_2.dylib
(tests/unit_CUI2DMap_UpdateAllDrawRelated_2.cpp), main app ("SimTheme Park"),
src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_UpdateAllDrawRelated_2.cpp:
  CUI2DMap::UpdateAllDrawRelated

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: the CDraw constructor, SetCurrent and destructor, GAME_ASSERTMSG, the three CObject queries, the
colour scaler UI2DMReturnScaleColour (its answer is a hash of the index), DrawCoasterTracks, CUI2DMapSprite::UpdateVisibilityAndColour
and the four render context slots (0x10, 0x14, 0x24, 0x28) the drawing goes through (the original through __ptr_glue and a vtable of
TVectors built by the SetCurrent fake, the host through the CUI2DMapDrawOps interface).

The world of a case: the map (size, filters, a grid of cells with a type and a thing id), the colour table, the thing array and its objects
(a float at +0x40), the sprite list (kind, mode, match, the thing's two halfwords) and the current sprite. What is compared: the log of
every fake with every argument (rectangles, lines and the nine words of every colour) and the head of the map object.

No fused operations; the two fctiwz conversions run under HardwareRounding. Not tested: a map of width or height 0 (480/0 saturates in
fctiwz and Unicorn answers differently), a map larger than the log holds, a cell outside the 128 x 128 grid.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CUI2DMap_UpdateAllDrawRelated_2"
MASK = 0xFFFFFFFF

FN = 0x1022e2c4
SIZES = {FN: 3688}
OWN = set(SIZES)

# callees
ASSERT, EXC, SAT, CLOSED = 0x1000dde8, 0x1010cfb0, 0x1010f9d0, 0x1010cae8
SCALE, TRACKS, UVC = 0x1022bb44, 0x1022f160, 0x10231134
DRAW_NEW, DRAW_SET, DRAW_DEL = 0x1023aed0, 0x1023aee8, 0x1023af30
# the code addresses of the four render context slots (functions of other units, never run here)
SLOT_CODE = {0x10: 0x1022f4d4, 0x14: 0x1022f8a4, 0x24: 0x1022fa1c, 0x28: 0x1022fad0}

ADDR = dict(things=0x1034cbfc, colours=0x103970f0)
# host: offsetof(CObject, mF40), sizeof(CThingArrayEntry), CUI2DMapSprite mKind, mMode, mMatch, mThing, sizeof, SpriteList mHead,
# CNode mNext, sizeof(SpriteList), sizeof(CNode)
HOST_LAYOUT = [0x40, 0x30, 0x30, 0x34, 0x38, 0x18, 0x50, 0x08, 0x08, 0x20, 0x18]

HOST_SP = dict(size=0x50, kind=0x30, mode=0x34, match=0x38, thing=0x18, ptrw=8, lsize=0x20, head=0x08, nsize=0x18, nnext=0x08, estride=0x30)
PPC_SP = dict(size=0x40, kind=0x14, mode=0x1c, match=0x20, thing=0x18, ptrw=4, lsize=0x10, head=0x04, nsize=0x0c, nnext=0x04, estride=0x14)


def s32(v):
    return T.s32(v)


def hw(index, i):
    return (index * 0x9E3779B1 + i * 0x85EBCA6B + 0x1234567) & MASK


def text_sum(t):
    return sum(t) & 0xFFFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def words_at(e, a, n):
    return [e.u32(a + 4 * i) for i in range(n)]


def st_assert(e):
    L = e.cl
    raw = e.cstr(e.r(4))
    text = raw.encode("latin-1") if isinstance(raw, str) else bytes(raw)
    L.rec("assert", (e.r(3) & 0xFF, text_sum(text)))


def objid(e, p):
    return e.u32(p)


def st_exc(e):
    L = e.cl
    L.rec("exc", (objid(e, e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, L.ret("exc") & MASK)


def st_sat(e):
    L = e.cl
    L.rec("sat", (objid(e, e.r(3)),))
    e.set_r(3, L.ret("sat") & MASK)


def st_closed(e):
    L = e.cl
    L.rec("closed", (objid(e, e.r(3)),))
    e.set_r(3, L.ret("closed") & MASK)


def st_scale(e):
    L = e.cl
    out, index = e.r(3), s32(e.r(4))
    L.rec("scale", (index, e.u32(e.r(5)), e.u32(e.r(6)), e.u32(e.r(7))))
    e.write(out, b"".join(struct.pack(">I", hw(index & MASK, i)) for i in range(9)))


def st_tracks(e):
    e.cl.rec("tracks", (e.r(3) - e.cl.mapbase,))


def st_uvc(e):
    e.cl.rec("uvc", (objid(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def st_draw_new(e):
    e.cl.draw = e.r(3)
    e.cl.rec("draw_new", ())


def st_draw_set(e):
    L = e.cl
    L.rec("setcur", (e.r(4) - L.mapbase,))
    ctx = 0
    if L.case["ctx"]:
        ctx = e.alloc(8)
        vt = e.alloc(0x40)
        e.write(ctx, struct.pack(">I", vt))
        for slot, code in SLOT_CODE.items():
            tv = e.alloc(8)
            e.write(tv, struct.pack(">II", code, 0))
            e.write(vt + slot, struct.pack(">I", tv))
        L.ctx = ctx
    e.write(e.r(3), struct.pack(">I", ctx))


def st_draw_del(e):
    e.cl.rec("draw_del", ())


def colour_event(e, p):
    e.cl.rec("col", tuple(words_at(e, p, 9)))


def line_stub(slot):
    def fn(e):
        L = e.cl
        L.rec("s%x" % slot, (int(e.r(3) == L.ctx), int(e.r(4) == L.draw + 0x30), int(e.r(5) == L.draw + 0x58), s32(e.r(6)), s32(e.r(7)),
                             s32(e.r(8)), s32(e.r(9))))
        colour_event(e, e.r(10))
    return fn


def rect_stub(slot):
    def fn(e):
        L = e.cl
        rect = [s32(w) for w in words_at(e, e.r(7), 4)]
        L.rec("s%x" % slot, tuple([int(e.r(3) == L.ctx), int(e.r(4) == L.draw + 0x30), int(e.r(5) == L.draw + 0x58), s32(e.r(6))] + rect))
        colour_event(e, e.r(8))
    return fn


STUBS.internal(ASSERT, st_assert)
STUBS.internal(EXC, st_exc)
STUBS.internal(SAT, st_sat)
STUBS.internal(CLOSED, st_closed)
STUBS.internal(SCALE, st_scale)
STUBS.internal(TRACKS, st_tracks)
STUBS.internal(UVC, st_uvc)
STUBS.imp("__ct__Q33bfl4draw5CDrawFv", st_draw_new)
STUBS.imp("SetCurrent__Q33bfl4draw5CDrawFP12TbRenderArea", st_draw_set)
STUBS.imp("__dt__Q33bfl4draw5CDrawFv", st_draw_del)
STUBS.internal(SLOT_CODE[0x10], line_stub(0x10))
STUBS.internal(SLOT_CODE[0x14], line_stub(0x14))
STUBS.internal(SLOT_CODE[0x24], rect_stub(0x24))
STUBS.internal(SLOT_CODE[0x28], rect_stub(0x28))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.sp["ptrw"] == 4 else "Q"), v))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    sp = PPC_SP

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.case = case
        self.L.ctx = 0
        self.L.draw = 0
        self.L.mapbase = 0
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

    def setmap(self, a):
        self.L.mapbase = a

    def call(self, m):
        with HardwareRounding(self.e):
            T.call_ppc(self.e, FN, [m & MASK])


class Host(Side):
    fmt = "<I"
    sp = HOST_SP

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ui2dm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ui2dm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setmap(self, a):
        pass

    def call(self, m):
        self.lib.tpw_ui2dm_run(m, self.case["ctx"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ui2dm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    sp = s.sp
    m = s.alloc(0x20100)
    s.setmap(m)
    s.words(m + 0x18, [case["mask"], case["m1c"], case["m20"], case["w"], case["h"]])
    for (x, y, t, i) in case["cells"]:
        a = m + 0x60 + (x + y * 128) * 8
        s.words(a, [t])
        s.half(a + 4, i)
    s.words(s.g("colours"), case["colours"])
    ob = s.alloc(16 * 0x80)
    for i in range(1, 16):
        s.words(ob + i * 0x80, [i])
        s.words(ob + i * 0x80 + 0x40, [case["objs"][i]])
    things = s.g("things")
    for i in range(16):
        ptr = ob + i * 0x80 if i else (ob + 0x80 if case["ent0"] else 0)
        s.ptr(things + i * sp["estride"], ptr)
    sprites = []
    for (idx, (kind, mode, match, first, second)) in enumerate(case["sprites"]):
        spr = s.alloc(sp["size"])
        tb = s.alloc(0x220)
        s.half(tb + 0x206, first)
        s.half(tb + 0x208, second)
        s.words(spr, [100 + idx])
        s.words(spr + sp["kind"], [kind])
        s.words(spr + sp["mode"], [mode])
        s.words(spr + sp["match"], [match])
        s.ptr(spr + sp["thing"], tb)
        sprites.append(spr)
    nodes = [s.alloc(sp["nsize"]) for _ in sprites]
    for i, spr in enumerate(sprites):
        s.ptr(nodes[i], spr)
        s.ptr(nodes[i] + sp["nnext"], nodes[i + 1] if i + 1 < len(nodes) else 0)
    lst = s.alloc(sp["lsize"])
    s.ptr(lst + sp["head"], nodes[0] if nodes else 0)
    s.ptr(m + 0x20080, lst)
    cur = case["cur"]
    s.ptr(m + 0x20060, sprites[cur] if 0 <= cur < len(sprites) else 0)
    return m


def run(s, case):
    try:
        m = build(s, case)
        s.call(m)
        return {"log": s.log(), "head": s.rd_words(m, 24)}
    finally:
        s.done()


_printed = []


def ppc(e, case):
    if not _printed:
        _printed.append(1)
        print("jump table", [hex(w) for w in words_at(e, 0x102b2914, 13)])
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

FLOATS = [0.0, 1.0, 12.5, 49.9, 50.0, 99.99, 100.0, 255.0, 256.0, 300.7, -1.0, -0.5, 257.9, 1000.0, -300.0]


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def base(r, w=None, h=None):
    w = r.choice([1, 2, 3, 4, 5, 7, 8]) if w is None else w
    h = r.choice([1, 2, 3, 4, 6]) if h is None else h
    cells = []
    for y in range(h + 1):
        for x in range(w + 1):
            if r.random() < 0.8:
                t = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3, 8, 13, 0xFFFFFFFF, 0x7FFFFFFF])
                cells.append((x, y, t, r.choice([1, 1, 2, 2, 3, 4])))
    qlen = 8
    return {
        "w": w, "h": h, "mask": r.choice([0, MASK, r.randrange(64), r.randrange(64), 0x10, 1, 2, 4, 8]),
        "m1c": r.choice([0, 1, 2, 3, 4]), "m20": r.choice([0, 1, 2, 3, 3, 2, 4, 5]),
        "cells": cells, "colours": [r.getrandbits(32) for _ in range(23 * 9)],
        "objs": [fbits(r.choice(FLOATS)) if r.random() < 0.6 else fbits(r.uniform(-50, 400)) for _ in range(16)],
        "ent0": int(r.random() < 0.1), "ctx": int(r.random() < 0.9),
        "sprites": [], "cur": -1,
        "queues": {"exc": [r.choice([0, 1, 49, 50, 100, 200, 255, 256, 300, r.randrange(512)]) for _ in range(qlen)],
                   "sat": [r.choice([0, 50, 100, -5, 150, r.randrange(-20, 200)]) & MASK for _ in range(qlen)],
                   "closed": [r.choice([0, 1]) for _ in range(qlen)]},
    }


def halves(r):
    return r.choice([0, 1, 2, 3, 127, 128, 129, 130, 255, 256, 16000, 16383, 16384, 0x4100, r.randrange(0x4100), r.randrange(0x4100)])


def add_sprites(r, c):
    n = r.choice([0, 1, 2, 3, 5])
    for _ in range(n):
        mode = r.choice([0, 1, 2, 3, 1, 2, 3, 4])
        c["sprites"].append((r.choice([2, 2, 2, 0, 1, 3]), mode, r.choice([0, 1, 2, 3, 4, 5, 8, 0x10, c["mask"], c["m1c"], c["m20"]]),
                             halves(r), halves(r)))
    c["cur"] = r.choice([-1, 0, 1, 2, 7]) if n else -1
    return c


def rnd(r):
    return add_sprites(r, base(r))


def edges():
    r = random.Random(965)
    # every type under each filter and mask: one map of two cells
    for t in list(range(0, 14)) + [0xFFFFFFFF, 0x80000000]:
        for (m1c, m20, mask) in [(0, 0, 0), (1, 2, MASK), (3, 3, 0x1f), (2, 0, 0x0), (1, 0, 1), (0, 0, 2), (0, 5, 4), (0, 1, 8), (0, 0, 0x10), (2, 3, 0)]:
            c = base(r, 2, 1)
            c["cells"] = [(0, 0, t, 1), (1, 0, t, 1 if (t & 1) else 2), (0, 1, t, 1), (1, 1, 0, 1), (2, 0, t, 1), (2, 1, 5, 3)]
            c["m1c"], c["m20"], c["mask"] = m1c, m20, mask
            c["ent0"] = int(t == 5)
            yield c
    # borders: equal neighbours on every side, one differing thing id
    for variant in range(6):
        c = base(r, 3, 3)
        c["cells"] = []
        for y in range(4):
            for x in range(4):
                c["cells"].append((x, y, 3 if x < 3 and y < 3 else 0, 1 if (x, y) != (1, 1 + (variant % 2)) else 2))
        c["m20"] = variant
        yield c
    # grid extremes
    for (w, h) in [(1, 1), (128, 1), (1, 128), (127, 1), (2, 127)]:
        c = base(r, w, h)
        c["cells"] = [(x, y, r.choice([0, 3, 3, 5, 9]), 1) for y in range(h) for x in range(w)]
        c["queues"]["closed"] = [0, 1, 1, 0]
        yield c
    # sprites: halfword boundaries, every mode and the current sprite
    for first in [0, 1, 2, 3, 128, 129, 130, 131, 255, 256, 257, 16384, 0x4100, 0xFFFF]:
        for second in [0, 1, 2, 126, 127, 128, 129, 130, 255, 16000, 16255, 16256, 16257, 16383, 16384, 0xFFFF]:
            c = base(r, 3, 2)
            c["mask"], c["m1c"], c["m20"] = 5, 2, 3
            c["sprites"] = [(2, 1, 4, first, second), (2, 2, 2, first, second), (2, 3, 3, first, second), (2, 1, 2, first, second)]
            c["cur"] = r.choice([0, 1, 2, 3])
            yield c
    for kind in range(4):
        for mode in range(5):
            for match in [0, 1, 2, 3, 4, 5, 8]:
                c = base(r, 2, 2)
                c["mask"], c["m1c"], c["m20"] = 5, 2, 3
                c["sprites"] = [(kind, mode, match, 130, 300), (2, 2, 2, 300, 300)]
                c["cur"] = 1
                if mode == 1 and match == 8:
                    c["mask"] = 0x10
                yield c
    # the coaster layer, a missing context, the first thing allocated
    for mask in [0x10, 0x0, 0x1f]:
        for ctx in [0, 1]:
            c = base(r, 3, 2)
            c["mask"], c["ctx"] = mask, ctx
            c["sprites"] = [(2, 1, 0x10, 200, 200)]
            yield c


TESTS = [
    FnTest("CUI2DMap::UpdateAllDrawRelated", ppc, host, edge=edges, rnd=rnd),
]
