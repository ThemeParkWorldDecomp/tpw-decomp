"""Unit v5989, CPeep::SetRandomDestination, against libtpw_unit_CPeep_SetRandomDestination.dylib (tests/unit_CPeep_SetRandomDestination.cpp), main app
("SimTheme Park"), src/game/people/Peep.cpp:
  CPeep::SetRandomDestination 0x10113870

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments and answers from the case's queues: IsStranded, IsLocalArea..., SetStranded,
SetDest (both), ShowThoughtBubble, GetRand, InsidePatrolArea, FindDestInPatrolArea, NumberOfHVNeighbours, GetNeighbours, GetDirection, IsPath,
IsQueue, IsEntrance, IsExit, IsXYValid, CTRACE and GAME_ASSERTMSG (texts included). Cells are named by their byte offset from gWorldMap (the
cells are never read: every cell method is a fake).

The world of a case: the peep block (random filler, then type at +2, x at +4, y at +6), gWorld (a block, gWorldMap = gWorld + 0x2d8), the
nine "no cell" halfwords at 0x102a0d64, the four compass offsets (PPC side only; the host library defines the constants). What is compared:
the answer, the whole peep block, the event log.

No fused operations. Not tested: nothing is skipped; the jump table of the ring offsets is read from the original's binary.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPeep_SetRandomDestination"
MASK = 0xFFFFFFFF

FN = 0x10113870
OWN = {FN: 3456}

ADDR = dict(gWorld=0x1037EC0C, gWorldMap=0x1037EC08, noCell=0x102A0D64)
HOST_LAYOUT = [2]
PEEP_SIZE = 0x1A8
TEXTS_OK = True


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell_stub(name):
    def fn(e):
        L = e.cl
        base = e.u32(0x1037EC08)
        L.rec(name, (s32(e.r(3) - base),))
        e.set_r(3, L.ret(name) & MASK)
    return fn


def get_rand(e):
    L = e.cl
    L.rec("GetRand", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetRand") & MASK)


STUBS.internal(0x1011566C, CL.recorder("IsStranded", "L", ret="i"))
STUBS.internal(0x101156FC, CL.recorder("IsLocal", "L", ret="i"))
STUBS.internal(0x101155BC, CL.recorder("SetStranded", "Li"))
STUBS.internal(0x10115400, CL.recorder("SetDestTag", "Lw", ret="i"))
STUBS.internal(0x101154F0, CL.recorder("SetDestMap", "Lww", ret="i"))
STUBS.internal(0x1012F0D8, CL.recorder("ShowThoughtBubble", "Lib"))
STUBS.internal(0x1013B1E4, get_rand)
STUBS.internal(0x10127AF8, CL.recorder("InsidePatrolArea", "Lw", ret="i"))
STUBS.internal(0x10127C10, CL.recorder("FindDestInPatrolArea", "L", ret="i"))
STUBS.internal(0x101012F4, CL.recorder("IsXYValid", "ii", ret="i"))
STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "us"))
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "is"))
for _name, _addr in (("NumberOfHVNeighbours", 0x10081890), ("GetNeighbours", 0x100816E8), ("GetDirection", 0x10081930),
                     ("IsPath", 0x1009C44C), ("IsQueue", 0x1009C490), ("IsEntrance", 0x1009C4DC), ("IsExit", 0x1009C524)):
    STUBS.internal(_addr, cell_stub(_name))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def halves(self, a, hs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(hs), *[h & 0xFFFF for h in hs]))

    def deltas(self):
        pass


class Ppc(Side):
    fmt = ">I"

    def ptr(self, a, v):
        self.words(a, [v])

    def __init__(self, e, case):
        self.e = e
        self.peep = e.alloc(PEEP_SIZE)
        self.world = e.alloc(0x400)
        self.L = e.cl = CL.PpcLog({self.peep: 1, self.peep + 0x30: 2, self.world: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def deltas(self):
        for addr, (x, y) in ((0x1034C25C, (0, 1)), (0x1034C284, (1, 0)), (0x1034C2AC, (0, -1)), (0x1034C234, (-1, 0))):
            self.words(addr, [x, y])

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN, [self.peep])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.peep = lib.tpw_srd_alloc(PEEP_SIZE)
        self.world = lib.tpw_srd_alloc(0x400)
        CL.host_begin(lib, {1: self.peep, 2: self.peep + 0x30, 3: self.world}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_srd_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        return self.lib.tpw_srd_call(self.peep) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_srd_addr.restype, lib.tpw_srd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_srd_alloc.restype, lib.tpw_srd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_srd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_srd_call.restype, lib.tpw_srd_call.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_srd_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.peep, struct.pack(">%dI" % len(case["filler"]), *case["filler"]))
    s.write(s.peep + 2, bytes([case["type"]]))
    s.write(s.peep + 4, bytes([case["x"]]))
    s.write(s.peep + 6, bytes([case["y"]]))
    s.ptr(s.g("gWorld"), s.world)
    s.ptr(s.g("gWorldMap"), s.world + 0x2D8)
    s.halves(s.g("noCell"), case["noCell"])
    s.deltas()


def observe(s, case, answer):
    raw = s.read(s.peep, PEEP_SIZE)
    orig = struct.pack(">%dI" % len(case["filler"]), *case["filler"])
    orig = orig[:2] + bytes([case["type"], orig[3], case["x"], orig[5], case["y"]]) + orig[7:]
    changed = raw[0xD0:0xD4] != orig[0xD0:0xD4]
    return {"answer": answer, "log": s.log(), "peep": raw[:0xD0] + raw[0xD4:], "flag": s.rd_word(s.peep + 0xD0) if changed else -1}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def bq(r, p, n=24):
    return [1 if r.random() < p else 0 for _ in range(n)]


def base(r, hv_first=None, staff=None, stranded=None, local=None):
    cx = r.choice([0, 1, 2, 5, 6, 126, 127, 128, 200, 255, r.randrange(256)])
    cy = r.choice([0, 1, 2, 5, 6, 126, 127, 128, 200, 255, r.randrange(256)])
    if staff is None:
        staff = r.random() < 0.5
    typ = r.choice([4, 5, 6, 7, 8]) if staff else r.choice([0, 1, 2, 3, 9, 10, 255])
    cell = (cx + cy * 128 + 1) & 0xFFFF
    no_cell = []
    for _ in range(9):
        k = r.random()
        if k < 0.55:
            no_cell.append(0)
        elif k < 0.7:
            no_cell.append(r.randrange(300))
        elif k < 0.9:
            no_cell.append((cell + r.choice([1, -1, 128, -128, 0])) & 0xFFFF)
        else:
            no_cell.append(r.randrange(0x10000))
    p_inside = r.choice([0.2, 0.5, 0.9])
    p_path = r.choice([0.2, 0.7, 1.0])
    p_queue = r.choice([0.0, 0.2, 0.6])
    p_exit = r.choice([0.0, 0.2, 0.6])
    p_valid = r.choice([0.3, 0.8, 1.0])
    p_set = r.choice([0.1, 0.5, 1.0])
    hv = [r.choice([-1, 0, 1, 2, 3, 4, 5, 2, 3, 4]) for _ in range(24)]
    if hv_first is True:
        hv[0] = r.choice([0, -1, -5])
    elif hv_first is False:
        hv[0] = r.randrange(1, 5)
    nbs = [r.choice([0x55, 0x11, 0x44, 0x01, 0x40, 0x10, 0x04, 0, r.randrange(256), 0xFF]) for _ in range(24)]
    dirs = [r.choice([1, 0x10, 4, 0x40, 0, 2, r.randrange(256)]) for _ in range(24)]
    queues = {
        "IsStranded": [int(r.random() < 0.5) if stranded is None else stranded],
        "IsLocal": [int(r.random() < 0.5) if local is None else local],
        "GetRand": [r.getrandbits(32) if r.random() < 0.85 else r.choice([0, 1, 2, 3, 4, 5, 0x7F, 0xFFFFFFFF]) for _ in range(30)],
        "InsidePatrolArea": [0 if r.random() > p_inside else r.choice([1, 2, -1]) for _ in range(24)],
        "FindDestInPatrolArea": [r.choice([0, 0, 1, 7]) for _ in range(6)],
        "NumberOfHVNeighbours": hv,
        "GetNeighbours": nbs,
        "GetDirection": dirs,
        "IsPath": bq(r, p_path),
        "IsQueue": bq(r, p_queue),
        "IsEntrance": bq(r, 0.3),
        "IsExit": bq(r, p_exit),
        "IsXYValid": bq(r, p_valid),
        "SetDestTag": [1 if r.random() < p_set else 0 for _ in range(24)],
        "SetDestMap": [r.choice([0, 1, 5]) for _ in range(4)],
    }
    return {"type": typ, "x": cx, "y": cy, "filler": [r.getrandbits(32) for _ in range(PEEP_SIZE // 4)], "noCell": no_cell, "queues": queues}


def edges():
    for i in range(160):
        r = random.Random(7000 + i)
        yield base(r, hv_first=bool((i >> 3) & 1), staff=bool((i >> 2) & 1), stranded=i & 1, local=(i >> 1) & 1)
    for i in range(60):
        r = random.Random(9000 + i)
        yield base(r, hv_first=False, stranded=0)


def rnd(r):
    return base(r)


TESTS = [
    FnTest("CPeep::SetRandomDestination", ppc, host, edge=edges, rnd=rnd),
]
