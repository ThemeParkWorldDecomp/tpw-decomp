"""Unit v5186, the pylon abort, the ride-movable test and the blend table, against libtpw_unit_LLCC_AbortPylonMove.dylib
(tests/unit_LLCC_AbortPylonMove.cpp), main app ("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCC_AbortPylonMove 0x1008f1a4, LLCC_IsRideMovable 0x1008f330, LLCC_GetRideBlendInfo 0x1008f574

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (LLCC_DrawLine, CObject::GetDBInfo, LLCR_OffsetStartOfTrack, LLCR_OffsetEndOfTrack, CCellBase::NumberOfNeighbours) are fakes on both
sides that record their arguments and answer from the case's queues (the offset and line fakes also move the cell they are given).

The world of a case: the pylon globals, the brace flag, the saved mesh word, the blend table (noise), a world block whose track cells hold
random mesh words, a ride object (x, y bytes, rotation) with a ride record (track type), a shape block. What is compared: the answer,
every global, the probed cells' mesh words, the blend table and the event log.

No fused operations. Not tested: coordinates far outside the world block (the original indexes the cells unchecked), shapes wider than 16.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_AbortPylonMove"
MASK = 0xFFFFFFFF

FN_ABORT, FN_MOVABLE, FN_BLEND = 0x1008F1A4, 0x1008F330, 0x1008F574
SIZES = {FN_ABORT: 292, FN_MOVABLE: 528, FN_BLEND: 732}
OWN = set(SIZES)

CALLEE_DRAWLINE = 0x1009C0D0
CALLEE_GETDBINFO = 0x10107CD4
CALLEE_NN = 0x10081744
CALLEE_OS = 0x1008C620
CALLEE_OE = 0x1008C6D8

ADDR = dict(gameworld=0x1037EBFC, e0b4=0x1029E0B4, e0b8=0x1029E0B8, e08c=0x102DE08C, e090=0x102DE090, df84=0x102DDF84)
CELLS = 0x1102D8
WORLD_SIZE = 0x122000
HOST_LAYOUT = [CELLS, 0x28, 4, 0x108, 0x100, 0x10, 0x9C]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def s32(v):
    return T.s32(v)


def stub_drawline(e):
    L = e.cl
    px, py = e.r(6), e.r(7)
    L.rec("DrawLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.u32(px)), s32(e.u32(py))))
    e.write(px, struct.pack(">I", L.ret("DLx") & MASK))
    e.write(py, struct.pack(">I", L.ret("DLy") & MASK))


def stub_getdbinfo(e):
    e.cl.rec("GetDBInfo", (0,))
    e.set_r(3, CUR["info"])


def make_offset(name, k):
    def stub(e):
        L = e.cl
        px, py = e.r(3), e.r(4)
        L.rec(name, (s32(e.u32(px)), s32(e.u32(py)), s32(e.r(5))))
        e.write(px, struct.pack(">I", L.ret(k + "x") & MASK))
        e.write(py, struct.pack(">I", L.ret(k + "y") & MASK))
        e.set_r(3, L.ret(k) & 0xFF)
    return stub


def stub_nn(e):
    L = e.cl
    idx = (s32(e.r(3) - (CUR["world"] + CELLS))) // 0x28
    L.rec("NumberOfNeighbours", (idx,))
    e.set_r(3, L.ret("NN") & MASK)


STUBS.internal(CALLEE_DRAWLINE, stub_drawline)
STUBS.internal(CALLEE_GETDBINFO, stub_getdbinfo)
STUBS.internal(CALLEE_OS, make_offset("OffsetStart", "OS"))
STUBS.internal(CALLEE_OE, make_offset("OffsetEnd", "OE"))
STUBS.internal(CALLEE_NN, stub_nn)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


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
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self, w):
        self.write(ADDR["gameworld"], struct.pack(">I", w))
        CUR["world"] = w

    def set_info(self, i):
        CUR["info"] = i

    def type_off(self):
        return 0x9C

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_apm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_apm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self, w):
        self.lib.tpw_apm_setworld(w)

    def set_info(self, i):
        self.lib.tpw_apm_setinfo(i)

    def type_off(self):
        return self.lib.type_off

    def call(self, fn, args):
        if fn == FN_ABORT:
            self.lib.tpw_apm_abort()
            return 0
        if fn == FN_MOVABLE:
            return self.lib.tpw_apm_movable(args[0]) & MASK
        return self.lib.tpw_apm_blend(args[0])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_apm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "setworld").argtypes = [C.c_uint64]
    getattr(lib, p + "setinfo").argtypes = [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "movable").restype, getattr(lib, p + "movable").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "blend").restype, getattr(lib, p + "blend").argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    got = list(out)[:n]
    assert got[:6] == HOST_LAYOUT[:6], got
    lib.type_off = got[6]       # BalanceValue is 8 bytes on the host: slot 36 is not at 0x9c there
    lib.bound = True


PROBE = [x + 128 * y for y in range(8) for x in range(8)]


def build(s, case):
    s.words(s.g("e0b4"), [case["a"]])
    s.words(s.g("e0b8"), [case["b"]])
    s.words(s.g("e08c"), [case["saved"]])
    s.write(s.g("e090"), bytes([case["flag"]]))
    s.write(s.g("df84"), b"".join(struct.pack(">I", t) for t in case["table"][:64]))
    s.words(s.g("df84") + 0x100, case["table"][64:])
    w = s.alloc(WORLD_SIZE)
    s.set_world(w)
    for i, idx in enumerate(PROBE):
        s.words(w + CELLS + idx * 0x28 + 4, [case["cellwords"][i]])
    info = s.alloc(0x400)
    s.words(info + s.type_off(), [case["type"]])
    s.set_info(info)
    obj = s.alloc(0x40)
    s.write(obj + 4, bytes([case["bx"]]))
    s.write(obj + 6, bytes([case["by"]]))
    s.words(obj + 0x10, [case["rot"]])
    shape = s.alloc(0xE00)
    s.words(shape, case["shape"])
    case["_w"], case["_obj"], case["_shape"] = w, obj, shape


def execute(s, case):
    if case["op"] == "abort":
        return s.call(FN_ABORT, [])
    if case["op"] == "movable":
        return s.call(FN_MOVABLE, [case["_obj"]])
    return 1 if s.call(FN_BLEND, [case["_shape"]]) == s.g("df84") else 0


def observe(s, case, answer):
    w = case["_w"]
    return {
        "answer": answer,
        "log": s.log(),
        "e0b4": s.rd_words(s.g("e0b4"), 1),
        "e0b8": s.rd_words(s.g("e0b8"), 1),
        "e08c": s.rd_words(s.g("e08c"), 1),
        "e090": s.read(s.g("e090"), 1),
        "df84": s.read(s.g("df84"), 256),
        "df84size": s.rd_words(s.g("df84") + 0x100, 2),
        "cells": [s.rd_words(w + CELLS + idx * 0x28 + 4, 1)[0] for idx in PROBE],
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {
        "op": op,
        "a": -1, "b": -1, "saved": r.getrandbits(32), "flag": r.choice([0, 1]),
        "table": [r.getrandbits(32) for _ in range(66)],
        "cellwords": [r.getrandbits(32) for _ in PROBE],
        "type": 1, "bx": r.randrange(256), "by": r.randrange(256), "rot": r.choice([0, 90, 180, 270, r.randrange(400)]),
        "shape": [0, 0], "queues": {},
    }


def dl_queues(r):
    return {"DLx": [r.randrange(8) for _ in range(3)], "DLy": [r.randrange(8) for _ in range(3)]}


# ---- LLCC_AbortPylonMove
def abort_case(r, a=None, b=None, flag=None):
    c = base(r, "abort")
    c["a"] = r.choice([-1, r.randrange(8), r.randrange(8)]) if a is None else a
    c["b"] = r.choice([-1, r.randrange(8), r.randrange(8)]) if b is None else b
    if flag is not None:
        c["flag"] = flag
    c["queues"] = dl_queues(r)
    return c


def abort_edges():
    r = random.Random(11)
    for a in (-1, 0, 3):
        for b in (-1, 0, 5):
            for flag in (0, 1):
                yield abort_case(r, a, b, flag)
    c = abort_case(r, 0, 0, 1)
    c["queues"] = {"DLx": [0, 0, 0], "DLy": [0, 0, 0]}
    yield c
    c = abort_case(r, 7, 7, 1)
    c["queues"] = {"DLx": [7, 7, 7], "DLy": [7, 7, 7]}
    yield c


def abort_random(r):
    return abort_case(r)


# ---- LLCC_IsRideMovable
def movable_case(r, type_=None, nn=None):
    c = base(r, "movable")
    c["type"] = r.choice([0, 1, 2, 3, -1, 4, 1, 2, 1, 2, 7, 0x80000000]) if type_ is None else type_
    c["queues"] = {
        "OSx": [r.randrange(128)], "OSy": [r.randrange(12)], "OS": [r.randrange(256)],
        "OEx": [r.randrange(128)], "OEy": [r.randrange(12)], "OE": [r.randrange(256)],
        "NN": nn if nn is not None else [r.choice([1, 1, 1, 0, 2, 3]), r.choice([1, 1, 0, 2, 4])],
    }
    return c


def movable_edges():
    r = random.Random(12)
    for t in (-1, 0, 1, 2, 3, 4):
        for nn in ([1, 1], [1, 0], [0, 1], [2, 1], [1, 2]):
            yield movable_case(r, t, nn)
    c = movable_case(r, 1, [1, 1])
    c["bx"], c["by"] = 0, 0
    yield c
    c = movable_case(r, 2, [1, 1])
    c["bx"], c["by"] = 255, 255
    c["queues"]["OSx"], c["queues"]["OSy"] = [0], [0]
    yield c


def movable_random(r):
    return movable_case(r)


# ---- LLCC_GetRideBlendInfo
BLEND_TYPES = [4, 0x17, 9, 10]
OTHER_TYPES = [0, 1, 2, 3, 5, 8, 11, 0x16, 0x18, 0xFFFFFFF7, 0xFFFFFFFC]


def shape_words(r, w, h, p, fixed=None):
    ws = [w, h] + [0] * (20 * 40)
    for z in range(20):
        for x in range(20):
            t = fixed if fixed is not None else (r.choice(BLEND_TYPES) if r.random() < p else r.choice(OTHER_TYPES))
            ws[2 + z * 40 + x * 2] = t
            ws[2 + z * 40 + x * 2 + 1] = r.getrandbits(32)
    return ws


def blend_case(r, w=None, h=None, p=None, fixed=None):
    c = base(r, "blend")
    w = r.randrange(0, 17) if w is None else w
    h = r.randrange(0, 17) if h is None else h
    p = r.choice([0.3, 0.7, 0.9, 1.0]) if p is None else p
    c["shape"] = shape_words(r, w, h, p, fixed)
    return c


def blend_edges():
    r = random.Random(13)
    for w, h in ((0, 0), (1, 1), (0, 5), (5, 0), (2, 2), (3, 3), (16, 16), (16, 1), (1, 16)):
        yield blend_case(r, w, h, 1.0)
        yield blend_case(r, w, h, 0.7)
    for t in BLEND_TYPES + OTHER_TYPES:
        yield blend_case(r, 4, 4, 1.0, t)


def blend_random(r):
    return blend_case(r)


TESTS = [
    FnTest("LLCC_AbortPylonMove", ppc, host, edge=abort_edges, rnd=abort_random),
    FnTest("LLCC_IsRideMovable", ppc, host, edge=movable_edges, rnd=movable_random),
    FnTest("LLCC_GetRideBlendInfo", ppc, host, edge=blend_edges, rnd=blend_random),
]
