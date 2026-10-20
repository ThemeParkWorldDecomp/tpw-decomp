"""Unit v5211, CMapCell::Delete, against libtpw_unit_CMapCell_Delete.dylib (tests/unit_CMapCell_Delete.cpp), main app
("SimTheme Park"), src/MapDelta_map_CMapCell_Delete.cpp:
  CMapCell::Delete 0x1009d0f4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the cell base's neighbour functions, CTRACE, GAME_ASSERTMSG, ModifyType, SetHFieldTile, GetObjectThingId, the object's queue and
value functions, the bank account, OPPOSITE, ADD_DIRECTION) is a fake on both sides that records its arguments (texts as a hash) and
answers from the case's queues.

The world of a case: twelve map cells of random content (the cell under test is the first; its mode, flag word, parent id and overlap
counter are chosen), the thing table rows 0 to 9 (row 0 is free unless the case says), the globals gGameWorld, gLLCC47E4 and the two
halfwords at 0x1029e226 / 0x1029e228, a track cell. What is compared: every field of the twelve cells, the track cell, the event log.

No fused operations. Not tested: a thing table row that is free but used (the original reads through NULL), a thing id past row 9.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMapCell_Delete"
MASK = 0xFFFFFFFF

FN = 0x1009D0F4
SIZES = {FN: 1860}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, things=0x1034CBFC, llcc=0x102E47E4, noparent=0x1029E226, nothing=0x1029E228)
HOST_LAYOUT = [0x44, 48, 0x2D8]
MAC_STRIDE = 0x14
NC = 12   # cells in the world block

# (offset, size) of every field of a cell that the module writes and reads back; the last eight are the words from 0x24 on
FIELDS = [(0, 2), (2, 1), (3, 1), (4, 4), (8, 4), (0xC, 1), (0xD, 1), (0xE, 2), (0x10, 2), (0x12, 2), (0x14, 4), (0x18, 4), (0x1C, 4),
          (0x20, 2), (0x22, 1), (0x23, 1)] + [(0x24 + 4 * i, 4) for i in range(8)]
I_ID, I_TYPE, I_NB, I_DIR, I_FLAGS, I_PARENT, I_COUNT = 0, 4, 5, 6, 7, 8, 13
TRACK_WORDS = 10

ADDRS = dict(NumberOfNeighbours=0x10081744, CTRACE=0x1006FCBC, GetNeighbours=0x100816E8, RemoveNeighbour=0x10081678,
             GetNeighbouringCell=0x10103814, OPPOSITE=0x10102500, SetHFieldTile=0x1009CDB4, GetObjectThingId=0x10103A04,
             GAME_ASSERTMSG=0x1000DDE8, RecalculateBackOfQueue=0x1010952C, ADD_DIRECTION=0x1010254C, ModifyDirection=0x10081810,
             ModifyType=0x10099614, NumberOfHVNeighbours=0x10081890, GetResidualValuePercentage=0x1010FD98,
             LLCC_SystemGetQueueCost=0x10091BD8, GetBankAccount=0x1013EDFC, MakeDeposit=0x100F32EC, GetTrackCellEquiv=0x100F48C4)

CTX = {}
host_stride = [48]


def s32(v):
    return T.s32(v)


def hash_text(text):
    if isinstance(text, str):
        text = text.encode("latin-1")
    h = 0
    for c in text:
        h = (h * 31 + c) & MASK
    return h


def rot(v, by):
    v &= 0xFF
    return ((v << by) | (v >> (8 - by))) & 0xFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lb(e, reg=3):
    return e.cl.lab(e.r(reg))


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def ret(e, name):
    return e.cl.ret(name)


def st_numnb(e):
    rec(e, "NumberOfNeighbours", lb(e))
    e.set_r(3, ret(e, "NumberOfNeighbours") & MASK)


def st_ctrace(e):
    rec(e, "CTRACE", e.r(3) & MASK, hash_text(e.cstr(e.r(4))))


def st_getnb(e):
    rec(e, "GetNeighbours", lb(e))
    e.set_r(3, ret(e, "GetNeighbours") & 0xFF)


def st_removenb(e):
    rec(e, "RemoveNeighbour", lb(e), e.r(4) & 0xFF)


def st_getnc(e):
    rec(e, "GetNeighbouringCell", lb(e), e.r(4) & 0xFF)
    k = ret(e, "GetNeighbouringCell")
    e.set_r(3, 0 if k == 0 else CTX["cells"] + k * 0x44)


def st_opposite(e):
    d = e.r(3) & 0xFF
    rec(e, "OPPOSITE", d)
    e.set_r(3, rot(d, 4))


def st_sethf(e):
    rec(e, "SetHFieldTile", lb(e))


def st_getthing(e):
    rec(e, "GetObjectThingId", lb(e, 4))
    e.write(e.r(3), struct.pack(">H", ret(e, "GetObjectThingId") & 0xFFFF))


def st_assert(e):
    rec(e, "GAME_ASSERTMSG", e.r(3) & MASK, hash_text(e.cstr(e.r(4))))


def st_recalc(e):
    rec(e, "RecalculateBackOfQueue", lb(e))


def st_adddir(e):
    a, b = e.r(3) & 0xFF, e.r(4) & 0xFF
    rec(e, "ADD_DIRECTION", a, b)
    e.set_r(3, rot(a, b))


def st_moddir(e):
    rec(e, "ModifyDirection", lb(e), e.r(4) & 0xFF)


def st_modtype(e):
    rec(e, "ModifyType", lb(e), e.r(4) & MASK)


def st_numhv(e):
    rec(e, "NumberOfHVNeighbours", lb(e))
    e.set_r(3, ret(e, "NumberOfHVNeighbours") & MASK)


def st_resid(e):
    rec(e, "GetResidualValuePercentage", lb(e))
    e.set_r(3, ret(e, "GetResidualValuePercentage") & MASK)


def st_cost(e):
    rec(e, "LLCC_SystemGetQueueCost")
    e.set_r(3, ret(e, "LLCC_SystemGetQueueCost") & MASK)


def st_getacc(e):
    rec(e, "GetBankAccount", lb(e))
    e.set_r(3, CTX["account"])


def st_deposit(e):
    rec(e, "MakeDeposit", lb(e), e.r(4) & MASK)


def st_gettrack(e):
    rec(e, "GetTrackCellEquiv", lb(e))
    e.set_r(3, CTX["track"])


for _name, _fn in (("NumberOfNeighbours", st_numnb), ("CTRACE", st_ctrace), ("GetNeighbours", st_getnb), ("RemoveNeighbour", st_removenb),
                   ("GetNeighbouringCell", st_getnc), ("OPPOSITE", st_opposite), ("SetHFieldTile", st_sethf),
                   ("GetObjectThingId", st_getthing), ("GAME_ASSERTMSG", st_assert), ("RecalculateBackOfQueue", st_recalc),
                   ("ADD_DIRECTION", st_adddir), ("ModifyDirection", st_moddir), ("ModifyType", st_modtype),
                   ("NumberOfHVNeighbours", st_numhv), ("GetResidualValuePercentage", st_resid), ("LLCC_SystemGetQueueCost", st_cost),
                   ("GetBankAccount", st_getacc), ("MakeDeposit", st_deposit), ("GetTrackCellEquiv", st_gettrack)):
    STUBS.internal(ADDRS[_name], _fn)

_printed = []


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(self.e_ + {1: "B", 2: "H", 4: "I"}[size], v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(self.e_ + {1: "B", 2: "H", 4: "I"}[size], self.read(a, size))[0]

    def cell(self, k):
        return self.cells + k * 0x44


class Ppc(Side):
    e_ = ">"
    stride = MAC_STRIDE

    def __init__(self, e, case):
        self.e = e
        self.world = e.alloc(0x2D8 + NC * 0x44 + 0x40)
        self.cells = self.world + 0x2D8
        self.account = e.alloc(16)
        self.track = e.alloc(0x28)
        self.objs = [e.alloc(16) for _ in range(10)]
        fixed = {self.cell(k): 1 + k for k in range(NC)}
        fixed[self.world] = 20
        fixed[self.account] = 21
        fixed[self.track] = 22
        for t in range(10):
            fixed[self.objs[t]] = 30 + t
        CTX.update(cells=self.cells, account=self.account, track=self.track)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.put(a, 4, v)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, a, b):
        T.call_ppc(self.e, FN, [self.cells, a & MASK, b & MASK])


class Host(Side):
    e_ = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = host_stride[0]
        self.world = lib.tpw_cd_alloc(0x2D8 + NC * 0x44 + 0x40)
        self.cells = self.world + 0x2D8
        self.account = lib.tpw_cd_alloc(16)
        self.track = lib.tpw_cd_alloc(0x28)
        self.objs = [lib.tpw_cd_alloc(16) for _ in range(10)]
        fixed = {1 + k: self.cell(k) for k in range(NC)}
        fixed[20] = self.world
        fixed[21] = self.account
        fixed[22] = self.track
        for t in range(10):
            fixed[30 + t] = self.objs[t]
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cd_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, a, b):
        self.lib.tpw_cd_delete(self.cells, s32(a), s32(b))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cd_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "delete").restype, getattr(lib, p + "delete").argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for k in range(NC):
        for (off, size), v in zip(FIELDS, case["cells"][k]):
            s.put(s.cell(k) + off, size, v)
    for i, v in enumerate(case["track"]):
        s.put(s.track + 4 * i, 4, v)
    s.ptr(s.g("world"), s.world)
    if "set_ctx" in dir(s):
        pass
    if isinstance(s, Host):
        s.ptr(s.g("account"), s.account)
        s.ptr(s.g("track"), s.track)
    s.put(s.g("llcc"), 4, case["llcc"])
    s.put(s.g("noparent"), 2, case["noparent"])
    s.put(s.g("nothing"), 2, case["nothing"])
    for t in range(10):
        s.put(s.objs[t] + 2, 1, case["kinds"][t])
        s.ptr(s.g("things") + t * s.stride, 0 if (t == 0 and not case["thing0"]) else s.objs[t])


def observe(s, case):
    cells = [[s.get(s.cell(k) + off, size) for off, size in FIELDS] for k in range(NC)]
    track = [s.get(s.track + 4 * i, 4) for i in range(TRACK_WORDS)]
    return {"cells": cells, "track": track, "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        s.call(*case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NBPAT = [0x05, 0x41, 0x14, 0x50, 0x11, 0x44, 0x01, 0x04, 0x10, 0x40, 0x00, 0xFF, 0x15, 0x55, 0x82, 0x28]
PCT = [0, 1, 50, 100, 150, 1000, 0x10000, 0x7FFFFFFF, -1, -100]
COST = [0, 1, 100, 250, 4096, 100000, 0x7FFFFFFF, -7, 0x20000, 0x40000000]


def mk(r, typ, f20=None, ab=None):
    cells = [[r.getrandbits(8 * size) for _, size in FIELDS] for _ in range(NC)]
    for k in range(NC):
        cells[k][I_ID] = k + 1
        if k > 0:
            cells[k][I_TYPE] = r.choice([0, 9, 9, 1, 1, 2, 5, 3, 0x1E])
        cells[k][I_PARENT] = r.randrange(0, 5)
    c0 = cells[0]
    c0[I_TYPE] = typ
    if f20 is None:
        f20 = r.random() < 0.5
    c0[I_FLAGS] = (r.getrandbits(16) & ~0x20) | (0x20 if f20 else 0)
    c0[I_COUNT] = r.choice([0xFFFF, 0, 1, 2, 0x7FFF, 0x8000, r.getrandbits(16)])
    if ab is None:
        ab = r.choice([(0, 0), (1, 0), (0, 1), (-1, 5), (r.getrandbits(32), r.getrandbits(32))])
    thing0 = r.random() < 0.3
    tids = [1, 2, 3, 4] + ([0] if thing0 else [])
    q = {
        "NumberOfNeighbours": [r.choice([0, 0, 1, 2, 3, 4])],
        "GetNeighbours": [r.choice(NBPAT) for _ in range(6)],
        "GetNeighbouringCell": [r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]) for _ in range(6)],
        "NumberOfHVNeighbours": [r.choice([2, 2, 2, 0, 1, 3, 4])],
        "GetObjectThingId": [r.choice(tids) for _ in range(6)],
        "GetResidualValuePercentage": [r.choice(PCT + [r.randrange(0, 300)])],
        "LLCC_SystemGetQueueCost": [r.choice(COST + [r.randrange(0, 5000)])],
    }
    return {"args": list(ab), "cells": cells, "track": [r.getrandbits(32) for _ in range(TRACK_WORDS)], "queues": q,
            "llcc": r.choice([0, 0, 1, r.getrandbits(32)]), "noparent": r.randrange(0, 5), "nothing": r.choice([0, 1, 2, 3]),
            "thing0": thing0, "kinds": [r.choice([3, 3, 3, 1, 4, 0]) for _ in range(10)]}


def edges():
    r = random.Random(5211)
    for typ in list(range(0, 0x17)) + [0x40, 0xFFFFFFFF, 0x80000000]:
        for f20 in (False, True):
            for ab in ((0, 0), (1, 0), (0, 1)):
                for _ in range(2):
                    yield mk(r, typ, f20, ab)


def rnd(r):
    return mk(r, r.choice(list(range(0, 0x17)) + [r.getrandbits(32)]))


TESTS = [
    FnTest("CMapCell::Delete", ppc, host, edge=edges, rnd=rnd),
]
