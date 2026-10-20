"""Unit v5005, CTrackCell::SetHFieldTile, RefreshHFieldTile and QuickDelete, against libtpw_unit_CTrackCell_SetHFieldTile.dylib
(tests/unit_CTrackCell_SetHFieldTile.cpp), main app ("SimTheme Park"), src/TrackCell_flags_CTrackCell_SetHFieldTile.cpp:
  SetHFieldTile 0x100a3bac, RefreshHFieldTile 0x100a3ec0, QuickDelete 0x100a4e88

A case is a world and a call. The world is built twice by ONE piece of Python code (cells field by field, because the host is little
endian and the fields are narrower than words). Every callee outside the unit (GetMapCellEquiv, GetTile, the mesh creators and
destroyers, SetCurRideAnim, RemoveNeighbour, ModifyDirection, ModifyType, GetNeighbouringCell) is a fake on both sides that records its
arguments (pointers relative to the world block) and answers from the case's queues.

The world: four track cells (cell 0 is `this`, the others are the neighbours GetNeighbouringCell can answer), the map cell equivalent's
type, the flag byte gLLCC4818, the scratch tile, the ride anim set table. Compared: the log, the four cells, the scratch tile.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrackCell_SetHFieldTile"
MASK = 0xFFFFFFFF

FN_SET, FN_REF, FN_QUICK = 0x100A3BAC, 0x100A3EC0, 0x100A4E88
SIZES = {FN_SET: 740, FN_REF: 340, FN_QUICK: 480}
OWN = set(SIZES)

CALLEES = dict(
    equiv=0x100F49A0, gettile=0x100A2A0C, destk=0x10082120, destw=0x10082508, desth=0x1008288C,
    creak=0x10081FB8, creaw=0x10082350, creah=0x10082720, anim=0x100C6670, remnb=0x10081678, moddir=0x10081810,
    modtype=0x100A0828, gnc=0x1012FCB8,
)

ADDR = dict(flag=0x102E4818, tmp=0x102E4840, anims=0x102C79A4, dplusx=0x1034C284, dplusxy=0x1034C270, dplusy=0x1034C25C)
HOST_LAYOUT = [8, 0xE, 0x14, 0x28, 4]
CELL_FMT = "HBBIIBBHHH3IhBBhH"
PW = {"w": 0}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(a):
    return s32((a & MASK) - PW["w"])


def mk(name, fn):
    return fn


def st_equiv(e):
    e.cl.rec("GetMapCellEquiv", (rel(e.r(3)),))
    e.set_r(3, PW["w"] + 0x100)


def st_gettile(e):
    L = e.cl
    L.rec("GetTile", (rel(e.r(3)),))
    rot = L.ret("Rot") & MASK
    tile = L.ret("GetTile") & MASK
    e.write(e.r(4), struct.pack(">I", rot))
    e.set_r(3, tile)


def st_dest(name):
    def f(e):
        e.cl.rec(name, (e.r(3) & MASK,))
    return f


def st_create(name):
    def f(e):
        e.cl.rec(name, tuple(s32(e.r(i)) for i in range(3, 9)))
        e.set_r(3, e.cl.ret(name) & MASK)
    return f


def st_anim(e):
    fl = struct.unpack(">I", struct.pack(">f", e.f(1)))[0]
    e.cl.rec("SetCurRideAnim", (rel(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), fl, s32(e.r(8))))


def st_remnb(e):
    e.cl.rec("RemoveNeighbour", (rel(e.r(3)), e.r(4) & 0xFF))


def st_moddir(e):
    e.cl.rec("ModifyDirection", (rel(e.r(3)), e.r(4) & 0xFF))


def st_modtype(e):
    e.cl.rec("ModifyType", (rel(e.r(3)), s32(e.r(4))))


def st_gnc(e):
    dx, dy = e.s32(e.r(4)), e.s32(e.r(4) + 4)
    e.cl.rec("GetNeighbouringCell", (rel(e.r(3)), dx, dy, s32(e.r(5))))
    v = e.cl.ret("GNC:%d" % (dx * 2 + dy)) & MASK
    e.set_r(3, 0 if v == 0 else PW["w"] + (v - 1) * 0x28)


STUBS.internal(CALLEES["equiv"], st_equiv)
STUBS.internal(CALLEES["gettile"], st_gettile)
STUBS.internal(CALLEES["destk"], st_dest("DestroyK"))
STUBS.internal(CALLEES["destw"], st_dest("DestroyW"))
STUBS.internal(CALLEES["desth"], st_dest("DestroyH"))
STUBS.internal(CALLEES["creak"], st_create("CreateK"))
STUBS.internal(CALLEES["creaw"], st_create("CreateW"))
STUBS.internal(CALLEES["creah"], st_create("CreateH"))
STUBS.internal(CALLEES["anim"], st_anim)
STUBS.internal(CALLEES["remnb"], st_remnb)
STUBS.internal(CALLEES["moddir"], st_moddir)
STUBS.internal(CALLEES["modtype"], st_modtype)
STUBS.internal(CALLEES["gnc"], st_gnc)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put_cell(self, a, c):
        self.write(a, struct.pack(self.fmt[0] + CELL_FMT, *c))

    def get_cell(self, a):
        return list(struct.unpack(self.fmt[0] + CELL_FMT, self.read(a, 0x28)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        PW["w"] = a
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def init_fixed(self, w):
        for i in range(16):
            self.words(ADDR["anims"] + 4 * i, [w + 0x200 + 16 * i])
        self.words(ADDR["dplusx"], [1, 0, 0])
        self.words(ADDR["dplusxy"], [1, 1, 0])
        self.words(ADDR["dplusy"], [0, 1, 0])

    def call(self, op, w):
        entry = {"set": FN_SET, "refresh": FN_REF, "quick": FN_QUICK}[op]
        T.call_ppc(self.e, entry, [w & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hft_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hft_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init_fixed(self, w):
        pass

    def call(self, op, w):
        getattr(self.lib, "tpw_hft_" + {"set": "set", "refresh": "refresh", "quick": "quick"}[op])(w)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hft_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("set", "refresh", "quick"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    w = s.alloc(0x400)
    s.w = w
    s.init_fixed(w)
    for i, c in enumerate(case["cells"]):
        s.put_cell(w + 0x28 * i, c)
    s.words(w + 0x100 + 8, [case["equiv"]])
    s.write(s.g("flag"), bytes([case["flag"]]))
    s.words(s.g("tmp"), [0, 0, 0])


def execute(s, case):
    s.call(case["op"], s.w)


def observe(s, case):
    return {"log": s.log(), "cells": [s.get_cell(s.w + 0x28 * i) for i in range(4)], "tmp": s.rd_words(s.g("tmp"), 3)}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


TYPES = [0, 1, 5, 7, 0xB, 0xC, 0x10, 0x11, 0x15, 0x19, 2, 3, -1, 0x14, 0x16]
KINDS = [0, 3, 4, 5, 2, 1, 3, 4]


def rcell(r, kind=None, typ=None, flags=None):
    return [
        r.choice([0, 1, 2, 0x80, 0x81, 0x423F, 0xFFFF, r.randrange(0x10000)]),
        r.choice([1, 2]), r.randrange(256),
        r.choice([0, 0, 1, 2, 5, 0xFFFFFFFF, r.getrandbits(32)]),
        (r.choice(TYPES) if typ is None else typ) & MASK,
        r.randrange(256), r.randrange(256),
        r.choice([0, 0x10, 0x20, 0xFFFF, 0xFFEF, r.randrange(0x10000)]) if flags is None else flags,
        r.randrange(0x10000), r.randrange(0x10000),
        (r.choice(KINDS) if kind is None else kind) & MASK,
        r.choice([0, 1, 0x37, 5, r.getrandbits(32)]),
        r.getrandbits(32),
        r.randrange(-32768, 32768), r.randrange(256), r.randrange(256),
        r.randrange(-32768, 32768), r.randrange(0x10000),
    ]


def base(r, op, typ=None, kind=None, flags=None, flag=None, equiv=None, tidx=None):
    cells = [rcell(r, kind=kind, typ=typ, flags=flags)] + [rcell(r) for _ in range(3)]
    if tidx is not None:
        cells[0][11] = tidx
    kt = r.choice([3, 4, 5, 0, 2, 1])
    tile = (kt << 16) | r.choice([0, 1, 0x37, r.randrange(0x10000)])
    if r.random() < 0.1:
        tile = r.getrandbits(32)
    q = {
        "GetTile": [tile],
        "Rot": [r.choice([0, 90, 180, 270, -90, r.getrandbits(32)])],
        "CreateK": [r.choice([0, 1, 5, r.getrandbits(32)])],
        "CreateW": [r.choice([0, 1, 2, 7, 15])],
        "CreateH": [r.choice([0, 1, r.getrandbits(32)])],
        "GNC:1": [r.choice([0, 1, 2, 3, 4])],
        "GNC:2": [r.choice([0, 1, 2, 3, 4])],
        "GNC:3": [r.choice([0, 1, 2, 3, 4])],
    }
    return {"op": op, "queues": q, "cells": cells,
            "equiv": r.choice([0x15, 0, 1, 0x14, 0x16]) if equiv is None else equiv,
            "flag": r.choice([0, 1, 0xFF]) if flag is None else flag}


def set_edges():
    r = random.Random(5005)
    for flag in (0, 1):
        for typ in TYPES:
            for equiv in (0x15, 0):
                for kt in (0, 3, 4, 5, 2):
                    for idx in (0, 0x37, 9):
                        c = base(r, "set", typ=typ, flag=flag, equiv=equiv)
                        c["queues"]["GetTile"] = [(kt << 16) | idx]
                        c["queues"]["Rot"] = [r.choice([0, 90, -1])]
                        c["cells"][0][10] = r.choice([0, 3, 4, 5])
                        c["cells"][0][7] = r.choice([0, 0x10])
                        yield c


def set_random(r):
    return base(r, "set")


def ref_edges():
    r = random.Random(5006)
    for equiv in (0x15, 0):
        for kind in (0, 3, 4, 5, 2):
            for fl in (0, 0x10, 0xFFFF):
                for idx in (0, 3):
                    c = base(r, "refresh", kind=kind, flags=fl, equiv=equiv)
                    c["cells"][0][0] = [0, 1, 0x81, 0xFFFF][idx]
                    yield c


def ref_random(r):
    return base(r, "refresh")


def quick_edges():
    r = random.Random(5007)
    for a in range(5):
        for b in range(5):
            for d in range(5):
                c = base(r, "quick")
                c["queues"]["GNC:1"], c["queues"]["GNC:2"], c["queues"]["GNC:3"] = [a], [b], [d]
                yield c


def quick_random(r):
    return base(r, "quick")


TESTS = [
    FnTest("CTrackCell::SetHFieldTile", ppc, host, edge=set_edges, rnd=set_random),
    FnTest("CTrackCell::RefreshHFieldTile", ppc, host, edge=ref_edges, rnd=ref_random),
    FnTest("CTrackCell::QuickDelete", ppc, host, edge=quick_edges, rnd=quick_random),
]
