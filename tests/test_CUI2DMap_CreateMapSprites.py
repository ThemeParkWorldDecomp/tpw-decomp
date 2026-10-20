"""Unit v5058, the 2D map's sprite maker, against libtpw_unit_CUI2DMap_CreateMapSprites.dylib (tests/unit_CUI2DMap_CreateMapSprites.cpp), main
app ("SimTheme Park"), src/CUI2DMap_CreateMapSprites.cpp:
  CUI2DMap::CreateMapSprites 0x1022fd78

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for
the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside the
unit are fakes on both sides that record their arguments: GetUsedThingHead, GetNextUsedThing (answers from the case's queues), the sprite
constructor (records its image word, id, the two status words and the scale; the object is new memory the fake zeroes), UpdateSpritePosition
(records the map, the sprite's x, y and thing) and GAME_ASSERTMSG (records the condition).

The world of a case: the map (width, height, the five image words), the world (cells' word at +0x2c, the object list head), a table of
things (kind, four position bytes, flags, link), the used list, five balance words, the two end markers. What is compared: the event log, which
holds every sprite made with its final position. fmadds here is exact (a byte times 1/256 plus a byte), so no rounding hooks are needed.
Not tested: a cell index of 0 (needs a wrapped halfword, not reachable with a width below 128).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUI2DMap_CreateMapSprites"
MASK = 0xFFFFFFFF

FN = 0x1022FD78
SIZES = {FN: 2628}
OWN = set(SIZES)

HEAD, NEXT, CTOR, UPDATE, ASSERT = 0x1013ADB0, 0x1013AE08, 0x10230D64, 0x10230A0C, 0x1000DDE8

ADDR = dict(none=0x102B294C, objnone=0x102B294E, world=0x1037EBFC, tarr=0x1034CBFC, bal=0x102B4568)
BAL_OFFS = (0x5D0, 0x600, 0x618, 0x630, 0x648)
IMAGES = (0x20068, 0x2006C, 0x20070, 0x20074, 0x20078, 0x2007C)
# the sprite entry: 48 bytes, mThing at 0; the sprite: mX 4, mY 8, mThing 0x18, size 0x30
HOST_LAYOUT = [48, 0, 4, 8, 0x18, 0x30]
NTHINGS = 12


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_head(e):
    L = e.cl
    L.rec("Head", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("Head") & 0xFFFF))


def stub_next(e):
    L = e.cl
    L.rec("Next", (L.lab(e.r(4)), L.lab(e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("Next") & 0xFFFF))


def stub_update(e):
    L = e.cl
    sp = e.r(4)
    L.rec("Update", (L.lab(e.r(3)), L.lab(sp), canon_f(e.u32(sp + 4)), canon_f(e.u32(sp + 8)), L.lab(e.u32(sp + 0x18))))


def stub_assert(e):
    e.cl.rec("Assert", (e.r(3) & 0xFF,))


STUBS.internal(HEAD, stub_head)
STUBS.internal(NEXT, stub_next)
STUBS.internal(UPDATE, stub_update)
STUBS.internal(ASSERT, stub_assert)
STUBS.internal(CTOR, CL.recorder("Ctor", "Nuiiif"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def objects(self, case):
        """Allocates the world, the map and the things (label 1, 2, 10 + id); returns {label: address}."""
        self.world = self.alloc(0x1DA748)
        self.map = self.alloc(0x20080)
        self.things = [0] + [self.alloc(0x30) for _ in range(NTHINGS)]
        d = {1: self.world, 2: self.map}
        for i in range(1, NTHINGS + 1):
            d[10 + i] = self.things[i]
        return d


class Ppc(Side):
    fmt = ">I"
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        e.cl = None
        objs = self.objects(case)
        self.L = e.cl = CL.PpcLog({a: l for l, a in objs.items()}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call(self):
        with_args = [self.map & MASK]
        T.call_ppc(self.e, FN, with_args)


class Host(Side):
    fmt = "<I"
    stride = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        objs = self.objects(case)
        CL.host_begin(lib, objs, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_cm_run(self.map)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    # the thing table: entries 0 .. NTHINGS + 2 cleared, then the things' pointers
    for i in range(NTHINGS + 3):
        s.ptr(s.g("tarr") + i * s.stride, 0)
    for i in range(1, NTHINGS + 1):
        s.ptr(s.g("tarr") + i * s.stride, s.things[i])
    if case["t0"]:
        s.ptr(s.g("tarr"), s.things[1])
    s.ptr(s.g("world"), s.world)
    s.half(s.g("none"), case["none"])
    s.half(s.g("objnone"), case["objnone"])
    s.words(s.g("bal") + 0, [])
    for off, v in zip(BAL_OFFS, case["bal"]):
        s.words(s.g("bal") + off, [v])
    s.words(s.map + 0x24, [case["w"], case["h"]])
    for k, off in enumerate(IMAGES):
        s.words(s.map + off, [0xA00 + k])
    for (x, y), v in case["cells"].items():
        s.words(s.world + 0x2D8 + (x + (y << 7)) * 0x44 + 0x2C, [v])
    s.half(s.world + 0x1DA746, case["objhead"])
    for i, t in enumerate(case["things"], 1):
        a = s.things[i]
        s.write(a + 2, bytes([t["kind"]]))
        s.write(a + 4, bytes(t["pos"]))
        s.half(a + 0x2E, t["flags"])
        s.half(a + 0xC, t["link"])


def run(s, case):
    try:
        build(s, case)
        s.call()
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def byte(r):
    return r.choice([0, 1, 255, 128, r.randrange(256), r.randrange(256)])


def make(r, kinds=None, w=None, h=None):
    n = r.randrange(0, NTHINGS + 1) if kinds is None else len(kinds)
    things = []
    for i in range(n):
        kind = r.choice([1, 4, 5, 6, 7, 0, 2, 3, 19]) if kinds is None else kinds[i]
        things.append({"kind": kind, "pos": [byte(r) for _ in range(4)], "flags": r.choice([0x10, 0, 0x30, 0x20, r.randrange(0x10000)]), "link": 0})
    none = r.choice([0, 0x3FFF, 0xFFFF])
    objnone = r.choice([0, 0x3FFF, 0xFFFF])
    ids = list(range(1, n + 1))
    r.shuffle(ids)
    used = ids[: r.randrange(0, n + 1)]
    r.shuffle(ids)
    objl = ids[: r.randrange(0, n + 1)]
    for a, b in zip(objl, objl[1:] + [objnone]):
        things[a - 1]["link"] = b
    for t in things:
        if t["link"] == 0:
            t["link"] = objnone
    for a in objl:
        if r.random() < 0.7:
            things[a - 1]["flags"] |= 0x10
    w = r.randrange(0, 5) if w is None else w
    h = r.randrange(0, 4) if h is None else h
    cells = {(x, y): r.choice([0, 0, 7, 8, 1, 3, r.randrange(-3, 12)]) for x in range(w) for y in range(h)}
    head = used[0] if used else none
    nxt = used[1:] + [none]
    return {"things": things, "none": none, "objnone": objnone, "t0": r.random() < 0.1, "w": w, "h": h, "cells": cells,
            "objhead": objl[0] if objl else objnone, "bal": [r.choice([0, 1, 7, -5, 100, r.randrange(-1000, 100000), 16777217]) for _ in range(5)],
            "queues": {"Head": [head], "Next": nxt}}


def edges():
    r = random.Random(5058)
    for k in (1, 4, 5, 6, 7, 2, 0):
        c = make(r, [k])
        c["queues"] = {"Head": [1], "Next": [c["none"]]}
        c["head"] = 1
        # a used list of the one thing and an object list of it (flag 0x10) too
        c["things"][0]["flags"] = 0x10
        c["objhead"] = 1
        c["things"][0]["link"] = c["objnone"]
        yield c
    for kinds in ([1, 6, 7, 4, 5], [7, 7]):
        c = make(r, kinds, 3, 2)
        c["queues"] = {"Head": [1], "Next": list(range(2, len(kinds) + 1)) + [c["none"]]}
        yield c
    c = make(r, [], 4, 3)
    c["cells"] = {(x, y): v for (x, y), v in zip(c["cells"], [7, 8, 5, 0, 1, -1, 0, 7, 8, 2, 3, 8])}
    yield c
    c = make(r, [], 0, 3)
    yield c
    c = make(r, [], 3, 0)
    yield c
    c = make(r, [6], 1, 1)
    c["t0"] = True
    c["queues"] = {"Head": [1], "Next": [c["none"]]}
    yield c


def rnd(r):
    return make(r)


TESTS = [FnTest("CUI2DMap::CreateMapSprites", ppc, host, edge=edges, rnd=rnd)]
