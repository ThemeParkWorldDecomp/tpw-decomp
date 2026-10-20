"""Unit v51060, the small predicates and accessors of CMapCell, CTrackCell, CLBLobby and the cell system, against
libtpw_unit_CMapCell_IsQueue.dylib (tests/unit_CMapCell_IsQueue.cpp), main app ("SimTheme Park"),
src/MapDelta_map_CMapCell_IsQueue.cpp: thirty functions, listed in OPS with their addresses.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back (fields are written
and read by their size, so a halfword sits where the Mac's halfword does). The callees outside the unit (GetNeighbours,
NumberOfHVNeighbours, GetTrackCellEquiv, CTrackCell::IsTrackX, LLCCM_DestroyQMesh, LLCCM_DestroyKTrackMesh) are fakes on both sides that
record their arguments and answer from the case's queues.

The world of a case: a cell block with every field of CMapCell / CTrackCell random (the mode and the flag word biased to the compared
values), a lobby block, the six globals and four int32 slots. What is compared: the answer (masked to the width the function answers),
every field of the cell, the lobby word, the globals, the slots and the event log. No fused operations. Not tested: nothing is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMapCell_IsQueue"
MASK = 0xFFFFFFFF

# (name, address, size, answer mask (0: void), kind)
OPS = [
    ("CMapCell::IsQueue", 0x1009c490, 36, MASK, "cell"),
    ("CMapCell::IsPath", 0x1009c44c, 28, MASK, "cell"),
    ("CMapCell::IsEntrance", 0x1009c4dc, 28, MASK, "cell"),
    ("CMapCell::IsExit", 0x1009c524, 28, MASK, "cell"),
    ("CMapCell::IsBridge", 0x1009c568, 8, MASK, "cell"),
    ("CMapCell::IsLake", 0x1009c5ec, 8, MASK, "cell"),
    ("CMapCell::IsRide", 0x1009c61c, 28, MASK, "cell"),
    ("CMapCell::IsGrass", 0x1009c660, 36, MASK, "cell"),
    ("CMapCell::IsWalkable", 0x1009c6ac, 28, MASK, "cell"),
    ("CMapCell::IsWalkableInFirstPerson", 0x1009c6f4, 48, MASK, "cell"),
    ("CMapCell::IsNoGo", 0x1009c75c, 44, MASK, "cell"),
    ("CMapCell::IsCoasterPylon", 0x1009c7b0, 28, MASK, "cell"),
    ("CMapCell::IsStraight", 0x1009c8c4, 124, 0xFF, "cell"),
    ("CMapCell::IsTrackX", 0x1009ca44, 44, MASK, "cell"),
    ("CMapCell::GetFlag", 0x1009bbe8, 8, 0xFFFF, "cell"),
    ("CMapCell::SetFlag", 0x1009bbb8, 8, 0, "cell"),
    ("CMapCell::RemoveInstanceMesh", 0x1009d07c, 68, 0, "cell"),
    ("CMapCell::GetPylonIndex", 0x1009d860, 8, MASK, "cell"),
    ("CMapCell::SetPylonIndex", 0x1009d898, 8, 0, "cell"),
    ("CMapCell::ModifyParentID", 0x10099930, 16, 0, "cell"),
    ("CTrackCell::ModifyParentID", 0x100a0b68, 16, 0, "cell"),
    ("CTrackCell::IsTrack", 0x100a3574, 60, MASK, "cell"),
    ("CTrackCell::IsJustTrack", 0x100a3628, 36, MASK, "cell"),
    ("CTrackCell::SetSegmentNumber", 0x100a3a38, 8, 0, "cell"),
    ("CTrackCell::RemoveInstanceMesh", 0x100a4048, 68, 0, "cell"),
    ("CTrackCell::DeleteMesh", 0x100a5098, 68, 0, "cell"),
    ("CLBLobby::IsKeyDown", 0x100a66b0, 12, MASK, "lobby"),
    ("__sinit_LLCellRide_cpp", 0x10090b2c, 16, 0, "global"),
    ("LLCR_GetHoardingExtents", 0x1008f9c8, 52, 0, "ext"),
    ("LLCC_ResetPylonMove", 0x1008f2f4, 16, 0, "global"),
]
OWN = {a for _, a, _, _, _ in OPS}

CALLEES = dict(GetNeighbours=0x100816e8, NumberOfHVNeighbours=0x10081890, GetTrackCellEquiv=0x100f4844,
               TrackIsTrackX=0x100a367c, DestroyQMesh=0x10081d80, DestroyKTrackMesh=0x10082120)

ADDR = dict(gLLCCE090=0x102de090, gLLCCE092=0x102de092, gLLCCHoardMaxY=0x102de0bc, gLLCCHoardMaxX=0x102de0c0,
            gLLCCHoardMinY=0x102de0c4, gLLCCHoardMinX=0x102de0c8)
GSIZE = dict(gLLCCE090=1, gLLCCE092=2, gLLCCHoardMaxY=4, gLLCCHoardMaxX=4, gLLCCHoardMinY=4, gLLCCHoardMinX=4)
# host: mMeshInstance, mType, mFlags, mParentID, mPylonIndex, CTrackCell::mSegmentNumber (the lobby's flag word follows)
HOST_LAYOUT = [4, 8, 0xE, 0x10, 0x40, 0x24]

# the cell's fields (offset, size)
FIELDS = [(0, 2), (2, 1), (3, 1), (4, 4), (8, 4), (0xC, 1), (0xD, 1), (0xE, 2), (0x10, 2), (0x12, 2), (0x14, 4), (0x18, 4), (0x1C, 4),
          (0x20, 2), (0x22, 1), (0x23, 1), (0x24, 2), (0x26, 2), (0x28, 4), (0x2C, 4), (0x30, 2), (0x32, 2), (0x34, 4), (0x38, 4),
          (0x3C, 4), (0x40, 2), (0x42, 2), (0x44, 4)]
CELL_SIZE = 0x48
LOBBY_PPC_OFF = 0x1C
CHAR = {1: "B", 2: "H", 4: "I"}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, addr, nargs=1):
    def stub(e):
        L = e.cl
        L.rec(name, tuple(L.lab(e.r(3)) if i == 0 else e.r(3 + i) & MASK for i in range(nargs)))
        if name in ("GetNeighbours", "NumberOfHVNeighbours", "GetTrackCellEquiv", "TrackIsTrackX"):
            e.set_r(3, L.ret(name) & MASK)
    STUBS.internal(addr, stub)


for _n in ("GetNeighbours", "NumberOfHVNeighbours", "GetTrackCellEquiv", "TrackIsTrackX"):
    mk(_n, CALLEES[_n])


def _destroy(name):
    def stub(e):
        e.cl.rec(name, (e.r(3) & MASK,))
    return stub


STUBS.internal(CALLEES["DestroyQMesh"], _destroy("DestroyQMesh"))
STUBS.internal(CALLEES["DestroyKTrackMesh"], _destroy("DestroyKTrackMesh"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(self.end + CHAR[size], v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(self.end + CHAR[size], self.read(a, size))[0]


class Ppc(Side):
    end = ">"
    lobby_off = LOBBY_PPC_OFF

    def __init__(self, e, case):
        self.e = e
        self.cell, self.lobby, self.slots = e.alloc(CELL_SIZE), e.alloc(0x40), e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.cell: 1, self.lobby: 2, self.slots: 3}, case["queues"])
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

    def call(self, idx, case):
        _, addr, _, _, kind = OPS[idx]
        arg = case["arg"] & MASK
        if kind == "cell":
            obj = [self.cell, arg]
        elif kind == "lobby":
            obj = [self.lobby, arg]
        elif kind == "ext":
            obj = [self.slots, self.slots + 4, self.slots + 8, self.slots + 12]
        else:
            obj = []
        r3, _, _ = T.call_ppc(self.e, addr, obj)
        return r3 & MASK


class Host(Side):
    end = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lobby_off = lib.lobby_off
        self.cell, self.lobby, self.slots = lib.tpw_cq_alloc(CELL_SIZE), lib.tpw_cq_alloc(0x400), lib.tpw_cq_alloc(16)
        CL.host_begin(lib, {1: self.cell, 2: self.lobby, 3: self.slots}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cq_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, idx, case):
        kind = OPS[idx][4]
        obj = {"cell": self.cell, "lobby": self.lobby, "ext": self.slots}.get(kind, 0)
        return self.lib.tpw_cq_call(idx, obj, case["arg"] & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cq_addr.restype, lib.tpw_cq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cq_alloc.restype, lib.tpw_cq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cq_call.restype, lib.tpw_cq_call.argtypes = C.c_uint32, [C.c_int32, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cq_layout(out)
    assert list(out)[:6] == HOST_LAYOUT, list(out)[:n]
    lib.lobby_off = out[6]
    lib.bound = True


def build(s, case):
    for off, size in FIELDS:
        s.put(s.cell + off, size, case["cell"][off])
    s.put(s.lobby + s.lobby_off, 4, case["lobby"])
    for name, v in case["g"].items():
        s.put(s.g(name), GSIZE[name], v)
    for i, v in enumerate(case["slots"]):
        s.put(s.slots + 4 * i, 4, v)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "cell": [s.get(s.cell + off, size) for off, size in FIELDS],
            "lobby": s.get(s.lobby + s.lobby_off, 4),
            "g": [s.get(s.g(n), GSIZE[n]) for n in sorted(ADDR)],
            "slots": [s.get(s.slots + 4 * i, 4) for i in range(4)]}


def run(s, case):
    try:
        build(s, case)
        idx = case["op"]
        ans = s.call(idx, case) & OPS[idx][3]
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MT = list(range(0, 0x24)) + [0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100, 0x1E + 0x100]
BYTES = [0, 1, 2, 4, 8, 0x10, 0x40, 0x11, 0x44, 0x15, 0x51, 0x55, 0xFF, 0x80, 0x41, 0x14, 0x05, 0x50]


def rnd32(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x8000, 0xFFFF, 0xFFFF0000, r.getrandbits(32), r.getrandbits(32)])


def base(r, op, **kw):
    cell = {}
    for off, size in FIELDS:
        cell[off] = r.getrandbits(8 * size)
    cell[8] = r.choice(MT) if r.random() < 0.8 else r.getrandbits(32)
    cell[4] = r.choice([0, 0, 1, r.getrandbits(32), 0xFFFFFFFF])
    cell[0xE] = r.choice([0x400, 0, 0xFBFF, 0xFFFF, 0x7FFF, r.getrandbits(16)])
    cell[0x40] = r.choice([0, 0xFFFF, 0x8000, 0x7FFF, r.getrandbits(16)])
    cell.update(kw.get("cell", {}))
    c = {"op": op, "cell": cell, "lobby": rnd32(r), "arg": rnd32(r),
         "g": {n: r.getrandbits(8 * GSIZE[n]) for n in ADDR}, "slots": [rnd32(r) for _ in range(4)],
         "queues": {"GetNeighbours": [kw.get("nb", r.choice(BYTES + [r.getrandbits(8)]))],
                    "NumberOfHVNeighbours": [kw.get("hv", r.choice([0, 1, 2, 2, 2, 3, 4]))],
                    "GetTrackCellEquiv": [kw.get("eq", r.choice([0, 0x1234]))],
                    "TrackIsTrackX": [kw.get("tx", r.choice([0, 1, 7, 0xFFFFFFFF]))]}}
    for k in ("arg", "lobby"):
        if k in kw:
            c[k] = kw[k]
    return c


def make_test(idx):
    name = OPS[idx][0]

    def edges():
        r = random.Random(1000 + idx)
        for t in MT:
            yield base(r, idx, cell={8: t})
        for f in (0, 0x400, 0xFBFF, 0xFFFF, 0x7FFF, 0x0400 | 1, 0x200, 0x800):
            yield base(r, idx, cell={0xE: f})
        for m in (0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678):
            yield base(r, idx, cell={4: m})
        for p in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x1234):
            yield base(r, idx, cell={0x40: p})
        for a in (0, 1, 0xFFFF, 0x10000, 0x80000000, 0xFFFF0000, 0x7FFF, 0x8000, 0x12345678):
            yield base(r, idx, arg=a)
        if name == "CLBLobby::IsKeyDown":
            for lw in (0, 1, 2, 4, 7, 0xFFFFFFFF):
                for a in (0, 1, 2, 4, 3, 0xFFFFFFFF):
                    yield base(r, idx, lobby=lw, arg=a)
        if name == "CMapCell::IsStraight":
            for nb in range(256):
                yield base(r, idx, nb=nb, hv=2)
            for nb in BYTES:
                for hv in (0, 1, 3, 4):
                    yield base(r, idx, nb=nb, hv=hv)
        if name == "CMapCell::IsTrackX":
            for eq in (0, 0x1234):
                for tx in (0, 1, 7):
                    yield base(r, idx, eq=eq, tx=tx)

    def rnd(r):
        return base(r, idx)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(i) for i in range(len(OPS))]
