"""Unit v5083, the 2D map's MoveCentre, InitSprites and UpdateMapZoom, against libtpw_unit_CUI2DMap_MoveCentre.dylib
(tests/unit_CUI2DMap_MoveCentre.cpp), main app ("SimTheme Park"), src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre.cpp:
  CUI2DMap::MoveCentre 0x1022f8a4, CUI2DMap::InitSprites 0x1022fad0, CUI2DMap::UpdateMapZoom 0x1022f4d4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments: CWorld::GetUsedThingHead / GetNextUsedThing (answers from the queues "head" and "next"),
GAME_ASSERTMSG, CEZSprite::Init / Enable / LoadSprite (queue "LoadSprite"), CMapZoom::Use / Window, CUI2DMap::CreateMapSprites and
UpdateAllSpritePositions, and the TbTList constructor (zeroes the list on the PowerPC side; the host runs the real one).

The world of a case: the map object (width, height, picture size, centre, zoom, sprite images), the screen size, the world block with
its cell table and object head, the thing table with the things of the case (id, kind, flags, next object), the six image names and the
two list end markers. What is compared: the log, the map's fields and image words, the new sprite list (its pointer set, empty, count).

Fused operations: UpdateMapZoom (fmadds, fnmsubs, fctiwz) runs under ROUNDING. Not tested: the list's first word (the original stores
the address of a data object over its vtable; the host keeps the TbTList vtable), a map of width or height 0 into fctiwz (inf), the
cell index 0 wrap (needs a 128 by 512 map, 4 MB of world).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CUI2DMap_MoveCentre"
MASK = 0xFFFFFFFF

FN_MOVE, FN_INIT, FN_ZOOM = 0x1022F8A4, 0x1022FAD0, 0x1022F4D4
SIZES = {FN_MOVE: 308, FN_INIT: 636, FN_ZOOM: 928}
OWN = set(SIZES)

C_USE, C_WINDOW, C_UPDATEALL = 0x1023ADF8, 0x1023AE10, 0x10230974
C_HEAD, C_NEXT, C_ASSERT = 0x1013ADB0, 0x1013AE08, 0x1000DDE8
C_EZINIT, C_EZENABLE, C_EZLOAD = 0x1023AF60, 0x1023AF78, 0x1023AF90
C_LISTCTOR, C_CREATE = 0x102314E8, 0x1022FD78

ADDR = dict(ezsprite=0x10052D48, mapzoom=0x100AFE20,    # the engine's data (the TOC slots 0x103bca60 and 0x103bc948 are import slots)
 gGameWorld=0x1037EBFC, thingarr=0x1034CBFC, images=0x10397010,
            thingend=0x102B2948, objend=0x102B294A, screenw=0x102B2B54, screenh=0x102B2B50)
HOST_LAYOUT = [48, 24, 8]      # sizeof(CThingArrayEntry), sizeof(CUI2DMapImageEntry), offsetof(mName)
MAPSZ, WORLDSZ = 0x20100, 0x1E0000
WORLD_BASE = 0x30000000
NTHINGS = 41
ST = {}                         # the persistent PowerPC blocks


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def fbc(x):
    return canon_f(fb(x))


def thash(text):
    data = text.encode("latin1") if isinstance(text, str) else bytes(text)
    return sum((i + 1) * c for i, c in enumerate(data))


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def _w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def st_head(e):
    L = e.cl
    L.rec("GetUsedThingHead", (L.lab(e.r(4)),))
    _w16(e, e.r(3), L.ret("head"))


def st_next(e):
    L = e.cl
    tid = struct.unpack(">H", e.read(e.r(5), 2))[0]
    L.rec("GetNextUsedThing", (L.lab(e.r(4)), tid))
    _w16(e, e.r(3), L.ret("next"))


def st_assert(e):
    L = e.cl
    L.rec("GAME_ASSERTMSG", (e.r(3) & MASK, thash(e.cstr(e.r(4)))))


def st_ezinit(e):
    L = e.cl
    L.rec("Init", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


def st_ezenable(e):
    L = e.cl
    L.rec("Enable", (L.lab(e.r(3)),))
    e.set_r(3, 0)


def st_ezload(e):
    L = e.cl
    L.rec("LoadSprite", (L.lab(e.r(3)), thash(e.cstr(e.r(4)))))
    e.set_r(3, L.ret("LoadSprite") & MASK)


def st_use(e):
    L = e.cl
    L.rec("Use", (L.lab(e.r(3)), fbc(e.f(1)), fbc(e.f(2)), fbc(e.f(3)), fbc(e.f(4))))


def st_window(e):
    L = e.cl
    L.rec("Window", (L.lab(e.r(3)), fbc(e.f(1)), fbc(e.f(2)), fbc(e.f(3)), fbc(e.f(4))))


def st_updateall(e):
    L = e.cl
    L.rec("UpdateAllSpritePositions", (L.lab(e.r(3)),))


def st_create(e):
    L = e.cl
    L.rec("CreateMapSprites", (L.lab(e.r(3)),))


def st_listctor(e):
    e.write(e.r(3) + 4, bytes(12))


def glue(fn):
    """A fake at a glue address: the glue would have saved the TOC pointer at 0x14(r1) for the caller's restore."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


for _a, _f in ((C_UPDATEALL, st_updateall), (C_HEAD, st_head), (C_NEXT, st_next), (C_ASSERT, st_assert),
               (C_LISTCTOR, st_listctor), (C_CREATE, st_create), (C_USE, glue(st_use)), (C_WINDOW, glue(st_window)),
               (C_EZINIT, glue(st_ezinit)), (C_EZENABLE, glue(st_ezenable)), (C_EZLOAD, glue(st_ezload))):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    ST["map"] = T.persist_alloc(runner.emu, MAPSZ)
    # the world block is 2 MB (the objects' head sits at +0x1da746): a region of its own, zeroed by the cases where they read it
    runner.emu.uc.mem_map(WORLD_BASE, (WORLDSZ + 0xFFF) & ~0xFFF)
    ST["world"] = WORLD_BASE


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def entry(self, i):
        return self.g("thingarr") + i * self.thing_stride

    def image_entry(self, i):
        return self.g("images") + i * self.image_stride


class Ppc(Side):
    fmt = ">I"
    thing_stride = 0x14
    image_stride = 0x14
    image_name_off = 8

    def __init__(self, e, case):
        self.e = e
        self.map = ST["map"]
        self.world = ST["world"]
        # the engine objects are bound at load: the program's TOC slots hold their addresses
        self.L = e.cl = CL.PpcLog({e.u32(0x10261E68): 1, e.u32(0x10261E6C): 2, self.world: 3, self.map: 4}, case["queues"])
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

    def put_ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def list_state(self):
        p = self.rd_word(self.map + 0x20080)
        if p == 0:
            return (0, 0, 0, 0)
        return (1, int(self.rd_word(p + 4) == 0), int(self.rd_word(p + 8) == 0), self.rd_word(p + 12))

    def call(self, op, args):
        entry = {"move": FN_MOVE, "init": FN_INIT, "zoom": FN_ZOOM}[op]
        if op == "move":
            call = [self.map, args[0] & MASK, T.F(bitsf(args[1]))]
        else:
            call = [self.map]
        with ROUNDING(self.e):
            T.call_ppc(self.e, entry, call)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if not hasattr(lib, "map_addr"):
            lib.map_addr = lib.tpw_u5083_alloc(MAPSZ)
            lib.world_addr = lib.tpw_u5083_alloc(WORLDSZ)
        self.map = lib.map_addr
        self.world = lib.world_addr
        self.thing_stride = HOST_LAYOUT[0]
        self.image_stride = HOST_LAYOUT[1]
        self.image_name_off = HOST_LAYOUT[2]
        CL.host_begin(lib, {1: self.g("ezsprite"), 2: self.g("mapzoom"), 3: self.world, 4: self.map}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_u5083_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_u5083_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def list_state(self):
        p = self.rd_ptr(self.map + 0x20080)
        if p == 0:
            return (0, 0, 0, 0)
        return (1, int(self.rd_ptr(p + 8) == 0), int(self.rd_ptr(p + 16) == 0), self.rd_word(p + 24))

    def call(self, op, args):
        lib = self.lib
        if op == "move":
            lib.tpw_u5083_movecentre(self.map, s32(args[0]), bitsf(args[1]))
        elif op == "init":
            lib.tpw_u5083_initsprites(self.map)
        else:
            lib.tpw_u5083_updatemapzoom(self.map)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_u5083_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_u5083_movecentre.restype, lib.tpw_u5083_movecentre.argtypes = None, [C.c_uint64, C.c_int32, C.c_float]
    lib.tpw_u5083_initsprites.restype, lib.tpw_u5083_initsprites.argtypes = None, [C.c_uint64]
    lib.tpw_u5083_updatemapzoom.restype, lib.tpw_u5083_updatemapzoom.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    m, w = s.map, s.world
    s.write(m, bytes(MAPSZ))
    s.write(w + 0x290, bytes(0x44 * 1200 + 0x80))
    s.write(w + 0x1DA740, bytes(16))
    s.words(m + 0x24, [case["mw"], case["mh"]])
    s.words(m + 0x48, [case["pw"], case["ph"]])
    s.words(m + 0x54, [case["cx"], case["cy"], case["zoom"]])
    s.words(s.g("screenw"), [case["sw"]])
    s.words(s.g("screenh"), [case["sh"]])
    s.put_ptr(s.g("gGameWorld"), w)
    s.h16(s.g("thingend"), case["thingend"])
    s.h16(s.g("objend"), case["objend"])
    # the thing table and the things
    s.write(s.g("thingarr"), bytes(NTHINGS * s.thing_stride))
    blocks = {}
    for tid, kind, flags, nxt in case["things"]:
        a = s.alloc(0x40)
        s.h16(a, tid)
        s.write(a + 2, bytes([kind]))
        s.h16(a + 0xC, nxt)
        s.h16(a + 0x2E, flags)
        blocks[tid] = a
        s.put_ptr(s.entry(tid), a)
    if case["e0"]:
        s.put_ptr(s.entry(0), blocks[case["things"][0][0]] if case["things"] else 0x1234)
    s.h16(w + 0x1DA746, case["objhead"])
    for idx, val in case["cells"]:
        s.words(w + 0x2D8 + idx * 0x44 + 0x2C, [val])
    for i, name in enumerate(case["names"]):
        a = s.alloc(len(name) + 1)
        s.write(a, name.encode() + b"\0")
        s.put_ptr(s.image_entry(i) + s.image_name_off, a)


def observe(s, case):
    m = s.map
    return {"log": s.log(), "ints": s.rd_words(m + 0x24, 2) + s.rd_words(m + 0x48, 2),
            "floats": [canon_f(x) for x in s.rd_words(m + 0x54, 3)], "images": s.rd_words(m + 0x20068, 6),
            "list": s.list_state()}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x3F800000, 0xBF800000]   # no NaN or infinity: they reach fctiwz, which Unicorn does not saturate
NAMES = ["map_a.spr", "map_b.spr", "m_c", "d", "Sprites\\e.spr", "f_long_name_here.bmp"]


def base(r, op):
    c = {"op": op, "args": [0, 0], "mw": r.randrange(1, 300), "mh": r.randrange(1, 300), "pw": r.randrange(1, 600),
         "ph": r.randrange(1, 600), "zoom": fb(r.choice([1.0, 2.0, 0.5, r.uniform(0.2, 8), r.uniform(0.01, 1)])),
         "sw": r.choice([640, 800, 1024, 1280, r.randrange(100, 2000)]), "sh": r.choice([480, 600, 768, 1024, r.randrange(100, 1500)]),
         "thingend": r.choice([0, 0xFFFF, 40]), "objend": r.choice([0, 0xFFFF, 40]), "things": [], "e0": 0, "objhead": 0,
         "cells": [], "names": list(NAMES)}
    c["cx"] = fb(r.uniform(-10, c["mw"] + 10))
    c["cy"] = fb(r.uniform(-10, c["mh"] + 10))
    c["objhead"] = c["objend"]
    c["queues"] = {"head": [c["thingend"]], "next": [c["thingend"]], "LoadSprite": [r.randrange(1, 1 << 30) for _ in range(6)]}
    return c


def f_in(r, c):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    return fb(r.choice([r.uniform(-20, 20), r.uniform(-1, c["mw"] + 5), float(r.randrange(-2, c["mw"] + 3))]))


def zoom_fix(c):
    # a map picture size of 0 would reach fctiwz with an infinity; keep the sizes sane
    return c


def move_case(r, d=None, amount=None, cx=None, cy=None):
    c = base(r, "move")
    if cx is not None:
        c["cx"], c["cy"] = cx, cy
    c["args"] = [r.choice([0, 1, 2, 3, 0, 1, 2, 3, -1, 4, 7]) if d is None else d, f_in(r, c) if amount is None else amount]
    return c


def move_edges():
    r = random.Random(5083)
    for d in (-1, 0, 1, 2, 3, 4, 100, -100):
        for amount in (0.0, 1.0, -1.0, 5.5, 1000.0, -1000.0, 0.25):
            yield move_case(r, d, fb(amount))
    for d in (0, 1, 2, 3):
        for cx in (0.0, -0.0, 3.0, 99.0, 100.0, 1e6, -5.0):
            c = move_case(r, d, 0, fb(cx), fb(cx))
            c["mw"], c["mh"] = 100, 100
            yield c
            c = move_case(r, d, fb(0.5), fb(cx), fb(99.5))
            c["mw"], c["mh"] = 100, 100
            yield c


def move_random(r):
    return move_case(r)


def zoom_case(r):
    c = base(r, "zoom")
    c["cx"] = fb(r.uniform(0, c["mw"]))
    c["cy"] = fb(r.uniform(0, c["mh"]))
    return c


def zoom_edges():
    r = random.Random(5084)
    for zoom in (0.5, 1.0, 2.0, 4.0, 0.0, 0.01, 1000.0, -1.0):
        for cx in (0.0, 50.0, 99.0, -1.0, 105.0):
            for cy in (0.0, 49.5, 99.0, 120.0):
                c = zoom_case(r)
                c["mw"] = c["mh"] = 100
                c["pw"] = c["ph"] = 100 if cx != 105.0 else 480
                c["zoom"], c["cx"], c["cy"] = fb(zoom), fb(cx), fb(cy)
                yield c
    for sw, sh in ((0, 0), (1, 1), (640, 480), (1, 1000)):
        c = zoom_case(r)
        c["sw"], c["sh"] = sw, sh
        yield c
    for pw, ph in ((1, 1), (2, 2), (480, 425)):
        c = zoom_case(r)
        c["pw"], c["ph"] = pw, ph
        yield c


def zoom_random(r):
    return zoom_case(r)


def init_case(r, w=None, h=None, nthings=None, nobjs=None):
    c = base(r, "init")
    c["mw"] = r.randrange(0, 12) if w is None else w
    c["mh"] = r.randrange(0, 6) if h is None else h
    ids = r.sample(range(1, 16), r.randrange(0, 9) if nthings is None else nthings)
    c["chain"] = ids
    things = []
    for i in ids:
        things.append((i, r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 4, 5, 6, 7]), 0, 0))
    oids = r.sample(range(16, 31), r.randrange(0, 7) if nobjs is None else nobjs)
    for k, i in enumerate(oids):
        nxt = oids[k + 1] if k + 1 < len(oids) else c["objend"]
        things.append((i, r.randrange(0, 9), r.choice([0, 0x10, 0x10, 0x20, 0xFFFF, 0xFFEF, 0x1F]), nxt))
    c["things"] = things
    c["objhead"] = oids[0] if oids else c["objend"]
    c["queues"]["head"] = [ids[0] if ids else c["thingend"]]
    c["queues"]["next"] = ids[1:] + [c["thingend"]]
    cells = []
    for y in range(c["mh"]):
        for x in range(c["mw"]):
            if r.random() < 0.4:
                cells.append((y * 128 + x, r.choice([1, 2, 7, 8, 0xFFFFFFFF, r.randrange(1, 1 << 32)])))
    c["cells"] = cells
    c["e0"] = 1 if r.random() < 0.15 and things else 0
    return c


def init_edges():
    r = random.Random(5085)
    yield init_case(r, 0, 0, 0, 0)
    yield init_case(r, 1, 1, 1, 1)
    for kind in range(9):
        c = init_case(r, 0, 0, 1, 0)
        t = c["things"][0]
        c["things"][0] = (t[0], kind, 0, 0)
        yield c
    for flags in (0, 0x10, 0x20, 0x30, 0xFFEF, 0xFFFF):
        c = init_case(r, 0, 0, 0, 1)
        t = c["things"][0]
        c["things"][0] = (t[0], t[1], flags, t[3])
        yield c
    yield init_case(r, 130, 2, 2, 2)
    yield init_case(r, 128, 4, 3, 3)
    yield init_case(r, 3, 3, 15, 15)
    for _ in range(4):
        c = init_case(r, 5, 2, 4, 3)
        c["e0"] = 1
        yield c


def init_random(r):
    return init_case(r)


TESTS = [
    FnTest("CUI2DMap::MoveCentre", ppc, host, edge=move_edges, rnd=move_random),
    FnTest("CUI2DMap::InitSprites", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("CUI2DMap::UpdateMapZoom", ppc, host, edge=zoom_edges, rnd=zoom_random),
]
