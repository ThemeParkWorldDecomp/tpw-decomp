"""Unit v5637, CMapCell::Modify (the cell system's entry for changing a map cell: bit 0x200 of the mode asks whether the placement connects,
bit 0x100 draws the placement preview, the low byte otherwise is a command 0x80 to 0x87 or 0x32, or a mode set through ModifyType), against
libtpw_unit_CMapCell_Modify.dylib (tests/unit_CMapCell_Modify.cpp), main app ("SimTheme Park"), src/MapDelta_map_CMapCell_Modify.cpp:
  CMapCell::Modify 0x10097538

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records what the real one reads and answers from the case's queues: the cell getters (GetTrackCellEquiv,
GetMapCellEquiv, GetNeighbouringCell) answer one of the case's cells by index (0 is NULL where the original can get NULL), GetObjectThingId
records its cell as an offset from the world, DC_AddSquareFaceToList records every argument (floats as bits, the three stack words) and
the two cell system words the walls clear, Delete and LLCR_DeleteRide overwrite gLLCC4804 / gLLCC4800 (the original restores them).

The world of a case: five map cells (the first is `this`), three track cells, the world pointer placed so that a cross track resolves to
one of the track cells, the thing array's entry 0 and the entry of the thing, the object's +0x28 word, the cell system's globals and the
function's own. What is compared: the answer, the event log, every global read back at its size and the cells' id, mode and parent id.

No fused operations; fctiwz runs under HardwareRounding. Not tested: NaN or huge ground heights (the fake heights stay in -60..120, the
wall loop runs from one height to the other in steps of 10), a NULL track cell (the original reads its mode unconditionally), a thing id
past the thing array.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CMapCell_Modify"
MASK = 0xFFFFFFFF
MASK64 = 0xFFFFFFFFFFFFFFFF

# ---- the unit's own function
FN_MODIFY = 0x10097538
SIZES = {FN_MODIFY: 8360}
OWN = set(SIZES)

# ---- the Mac's addresses of the globals; the host library's come from tpw_mcm_addr(name)
INT_GLOBALS = ["gLLCCC210", "gLLCCC214", "gLLCCDE0B4", "gLLCCDE0B8", "gLLCC47C8", "gLLCC4800", "gLLCC4804", "gValidTypeChangeLock",
               "gModifyLastMode", "gModifyTypeRefused", "gModifyPylonIndex"]
HALF_GLOBALS = ["gLLCCDB68", "gModifyNoParent", "gModifyNoThing"]
ADDR = dict(gLLCCC210=0x102dc210, gLLCCC214=0x102dc214, gLLCCDB68=0x102ddb68, gLLCCDE0B4=0x102de0b4, gLLCCDE0B8=0x102de0b8,
            gLLCC47C8=0x102e47c8, gLLCC4800=0x102e4800, gLLCC4804=0x102e4804, gValidTypeChangeLock=0x102e4834,
            gModifyLastMode=0x102dc21c, gModifyTypeRefused=0x102e483c, gModifyPylonIndex=0x102ddb70, gModifyNoParent=0x1029e20e,
            gModifyNoThing=0x1029e210, gGameWorld=0x1037ebfc, gThingArray=0x1034cbfc)
# The MapDelta constants (x, y), written on the PowerPC side (the host's are const definitions in the unit library).
DELTAS = {0x1034c284: (1, 0), 0x1034c270: (1, 1), 0x1034c25c: (0, 1), 0x1034c2ac: (0, -1), 0x1034c234: (-1, 0)}
# ---- the host's sizes the module relies on: CMapCell, CTrackCell, CObject::mF28, CThingArrayEntry (and its mThing), MapDelta
HOST_LAYOUT = [0x44, 0x28, 0x28, 48, 0, 12]

NCELLS, NTRACKS = 5, 3


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def text_sum(data):
    """The unit library's TextSum: the length in the high word, a weighted byte sum in the low one."""
    s = 0
    for ch in data:
        s = (s * 31 + ch) & MASK
    return (len(data) << 32) | s


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's function only (Unicorn saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rd32(e, a):
    return struct.unpack(">I", e.read(a & MASK, 4))[0]


def answer(e, name):
    e.set_r(3, e.cl.ret(name) & MASK)


def predicate(name):
    """A `this`-only predicate or getter answering an integer from the queue."""
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
        answer(e, name)
    return stub


def u8_answer(name):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
        e.set_r(3, e.cl.ret(name) & 0xFF)
    return stub


def half_arg(name):
    """A method of `this` and a halfword (flags) answering an integer (or nothing)."""
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), e.r(4) & 0xFFFF))
        if name == "IsFlag":
            answer(e, name)
    return stub


def clobber(e):
    e.write(ADDR["gLLCC4804"], struct.pack(">I", 0x4804))
    e.write(ADDR["gLLCC4800"], struct.pack(">I", 0x4800))


def stub_get_mode(e):
    e.cl.rec("LLCC_SystemGetMode", ())
    answer(e, "LLCC_SystemGetMode")


def stub_set_mode(e):
    e.cl.rec("LLCC_SystemSetModeNR", (s32(e.r(3)), e.r(4) & 0xFF))


def stub_illegal(e):
    e.cl.rec("LLCM_SetIllegalPlacement", (s32(e.r(3)),))


def stub_delete_ride(e):
    e.cl.rec("LLCR_DeleteRide", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    clobber(e)


def fix_bridges(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
        e.set_r(3, L.ret(name) & 0xFF)
    return stub


def stub_height(e):
    e.cl.rec("GetWorldGSquareHeight", (s32(e.r(3)), s32(e.r(4)), 0 if e.r(5) == 0 else 1))
    e.set_f1(bitsf(e.cl.ret("GetWorldGSquareHeight")))


def stub_dc(e):
    """DC_AddSquareFaceToList(float x, float y, float z, uint8 side, int texture, int rotation, float unused1, int large, uint8 unused2,
    int keep, DYN_Type type): the last three are the stack words at sp+0x38, +0x3c, +0x40; also the two words the walls clear."""
    sp = e.r(1)
    e.cl.rec("DC_AddSquareFaceToList", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), e.r(6) & 0xFF, s32(e.r(7)),
                                        s32(e.r(8)), canon_f(fb(e.f(4))), s32(e.r(10)), rd32(e, sp + 0x38) & 0xFF,
                                        s32(rd32(e, sp + 0x3c)), s32(rd32(e, sp + 0x40))))
    e.cl.rec("DC_Words", (s32(rd32(e, ADDR["gLLCCC210"])), s32(rd32(e, ADDR["gLLCCC214"]))))


def stub_assert(e):
    a = e.r(4)
    data = b""
    while len(data) < 400:
        ch = e.read(a + len(data), 1)
        if ch == b"\0":
            break
        data += ch
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), text_sum(data)))


def stub_remove_bridge(e):
    L = e.cl
    L.rec("RemoveBridge", (L.lab(e.r(3)), s32(rd32(e, e.r(4))), s32(rd32(e, e.r(4) + 4)), s32(rd32(e, e.r(5))),
                           s32(rd32(e, e.r(5) + 4))))


def stub_track_equiv(e):
    L = e.cl
    L.rec("GetTrackCellEquiv", (L.lab(e.r(3)),))
    i = L.ret("GetTrackCellEquiv")
    e.set_r(3, L.tcells[i - 1] if 1 <= i <= len(L.tcells) else 0)


def cell_getter(name, with_delta):
    def stub(e):
        L = e.cl
        if with_delta:
            L.rec(name, (L.lab(e.r(3)), s32(rd32(e, e.r(4))), s32(rd32(e, e.r(4) + 4)), s32(e.r(5))))
        else:
            L.rec(name, (L.lab(e.r(3)),))
        i = L.ret(name)
        e.set_r(3, L.cells[i - 1] if 1 <= i <= len(L.cells) else 0)
    return stub


def stub_connection(e):
    e.cl.rec("ConnectionType", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    answer(e, "ConnectionType")


def stub_directions(e):
    e.cl.rec("CalculateDirections", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def stub_neighbours(e):
    e.cl.rec("CalculateNeighbours", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & 0xFF))


def stub_tile(e):
    e.cl.rec("SetHFieldTile", (e.cl.lab(e.r(3)),))


def stub_parent_id(e):
    e.cl.rec("ModifyParentID", (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def stub_pylon(e):
    e.cl.rec("SetPylonIndex", (e.cl.lab(e.r(3)), e.r(4) & 0xFFFF))


def stub_thing_id(e):
    """GetObjectThingId (hidden result): r3 the result's halfword, r4 the cell (recorded as its offset from the world)."""
    world = rd32(e, ADDR["gGameWorld"])
    e.cl.rec("GetObjectThingId", ((e.r(4) - world) & MASK,))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetObjectThingId") & 0xFFFF))


def stub_delete(e):
    e.cl.rec("Delete", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    clobber(e)


def stub_modify_type(e):
    e.cl.rec("ModifyType", (e.cl.lab(e.r(3)), s32(e.r(4))))
    answer(e, "ModifyType")


for _addr, _fn in [
        (0x100914f4, stub_get_mode), (0x100911f0, stub_set_mode), (0x10088934, stub_illegal), (0x100892c4, stub_delete_ride),
        (0x1008fa34, fix_bridges("LLCR_FixBridges")), (0x1009024c, fix_bridges("LLCR_FixBridgesReverse")),
        (0x1005f160, stub_height), (0x100e02a0, stub_dc), (0x1000dde8, stub_assert), (0x1009722c, stub_remove_bridge),
        (0x100f48c4, stub_track_equiv), (0x100f49a0, cell_getter("GetMapCellEquiv", False)),
        (0x101036b0, cell_getter("GetNeighbouringCell", True)), (0x10081930, u8_answer("GetDirection")),
        (0x1009b158, stub_connection), (0x1009af80, stub_directions), (0x10099998, stub_neighbours), (0x1009cdb4, stub_tile),
        (0x10099930, stub_parent_id), (0x1009d898, stub_pylon), (0x10103a04, stub_thing_id), (0x1009d0f4, stub_delete),
        (0x1009c44c, predicate("IsPath")), (0x1009bb70, half_arg("IsFlag")), (0x1009c61c, predicate("IsRide")),
        (0x1009c4dc, predicate("IsEntrance")), (0x1009c524, predicate("IsExit")), (0x10099614, stub_modify_type),
        (0x100a367c, predicate("IsTrackX")), (0x100a3574, predicate("IsTrack")), (0x100a34c4, predicate("IsBridge")),
        (0x100a35dc, predicate("IsHoarding")), (0x100a37a0, u8_answer("IsCorner")), (0x100a3914, u8_answer("IsCrossRoad")),
        (0x100a2964, half_arg("SetFlag")), (0x100a29cc, half_arg("UnSetFlag"))]:
    STUBS.internal(_addr, _fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def blocks(self):
        self.cells = [self.alloc(0x44) for _ in range(NCELLS)]
        self.tcells = [self.alloc(0x28) for _ in range(NTRACKS)]
        self.obj = self.alloc(0x500)
        labels = {a: 10 + i for i, a in enumerate(self.cells)}
        labels.update({a: 20 + i for i, a in enumerate(self.tcells)})
        labels[self.obj] = 40
        return labels

    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt + "I", v & MASK))

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt + "H", v & 0xFFFF))

    def r32(self, a):
        return struct.unpack(self.fmt + "I", self.read(a, 4))[0]

    def r16(self, a):
        return struct.unpack(self.fmt + "H", self.read(a, 2))[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">"
    pmask = MASK
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        labels = self.blocks()
        self.L = e.cl = CL.PpcLog(labels, case["queues"])
        self.L.cells, self.L.tcells = self.cells, self.tcells
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
        self.w32(a, v)

    def platform(self):
        for a, (x, y) in DELTAS.items():
            self.write(a, struct.pack(">iiI", x, y, 0))

    def log(self):
        return self.L.log

    def call(self, mode, a, b):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_MODIFY, [self.cells[0], mode & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<"
    pmask = MASK64
    stride = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        labels = self.blocks()
        CL.host_begin(lib, {v: k for k, v in labels.items()}, case["queues"])
        arr = C.c_uint64 * 8
        lib.tpw_mcm_tables(arr(*self.cells), NCELLS, arr(*self.tcells), NTRACKS)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_mcm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_mcm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v & MASK64))

    def platform(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, mode, a, b):
        return self.lib.tpw_mcm_modify(self.cells[0], s32(mode), s32(a), s32(b)) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_mcm_addr.restype, lib.tpw_mcm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mcm_alloc.restype, lib.tpw_mcm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mcm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mcm_tables.restype = None
    lib.tpw_mcm_tables.argtypes = [C.POINTER(C.c_uint64), C.c_int32, C.POINTER(C.c_uint64), C.c_int32]
    lib.tpw_mcm_modify.restype, lib.tpw_mcm_modify.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mcm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks (offsets are the Mac's) the same way on both sides."""
    s.platform()
    for name in INT_GLOBALS:
        s.w32(s.g(name), case[name])
    for name in HALF_GLOBALS:
        s.w16(s.g(name), case[name])
    for a, c in list(zip(s.cells, case["cells"])) + list(zip(s.tcells, case["tcells"])):
        s.w16(a, c["id"])
        s.w32(a + 8, c["type"])
        s.w16(a + 0x10, c["pid"])
    # The world sits where the first track cell's parent id resolves to the target track cell.
    pid = case["tcells"][case["queues"]["GetTrackCellEquiv"][0] - 1]["pid"]
    world = s.tcells[case["target"]] - 0x1102d8 - (pid - 1) * 0x28
    s.ptr(s.g("gGameWorld"), world & s.pmask)
    things = s.g("gThingArray")
    s.ptr(things, case["entry0"])
    thing = case["queues"]["GetObjectThingId"][0] & 0xFFFF
    if thing < 0x2800:
        s.ptr(things + thing * s.stride, s.obj)
    s.w32(s.obj + 0x28, case["f28"])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log()}
    for name in INT_GLOBALS:
        out[name] = s.r32(s.g(name))
    for name in HALF_GLOBALS:
        out[name] = s.r16(s.g(name))
    out["cells"] = [(s.r16(a), s.r32(a + 8), s.r16(a + 0x10)) for a in s.cells + s.tcells]
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(*case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [0x200, 0x204, 0x2ff, 0x300, 0x104, 0x118, 0x10b, 0x110, 0x107, 0x100, 0x1ff, 0x111,
         0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x32, 0x432, 0x15, 0x415, 0, 1, 3, 0x7f, 0x88, 0xff, 0x400]
DEEP = {0x85: 60, 0x86: 60, 0x32: 80, 0x87: 30, 0x81: 30, 0x15: 30}


def height(r):
    return fb(r.choice([0.0, -0.0, 5.0, 10.0, 9.99, 0.5, -9.5, -15.0, 120.0, 31.0, 1.0, round(r.uniform(-60, 120), 2)]))


def cell(r, types):
    return {"id": r.choice([0, 1, 0x80, 0x81, 0x4000, 0xFFFF, r.randrange(1, 0x4001), r.randrange(1, 0x4001)]),
            "type": r.choice(types), "pid": r.choice([0, 1, 2, 0x4000, 0xFFFF, r.randrange(1, 0x4001)])}


def base(r):
    """The world every case starts from: random globals and cells; the fakes' answer queues cover every branch."""
    pred = [0, 1, 0x100, -1]
    no_thing = r.choice([0, 0, 3])
    thing = r.choice([0, 1, 5, 0x27FF, no_thing, r.randrange(0x2800)])
    q = {
        "LLCC_SystemGetMode": [r.choice([0, 1, 2, 3, 4, 4, -1]) for _ in range(4)],
        "ConnectionType": [r.choice([0, 1, 1, 2, 3, -1]) for _ in range(4)],
        "GetTrackCellEquiv": [r.randrange(1, NTRACKS + 1) for _ in range(12)],
        "IsTrackX": [r.choice([0, 1, 0x100]) for _ in range(4)],
        "GetWorldGSquareHeight": [height(r) for _ in range(24)],
        "GetNeighbouringCell": [r.randrange(0, NCELLS + 1) for _ in range(8)],
        "GetMapCellEquiv": [r.randrange(1, NCELLS + 1) for _ in range(4)],
        "GetDirection": [r.choice([0, 1, 4, 0x10, 0x40, 2, 0x80, 0x11]) for _ in range(4)],
        "IsTrack": [r.choice(pred) for _ in range(4)],
        "IsBridge": [r.choice([0, 1, 0x100, -1, 1]) for _ in range(4)],
        "IsHoarding": [r.choice([0, 0, 1, -1]) for _ in range(4)],
        "IsCorner": [r.choice([0, 0, 1, 0x80]) for _ in range(4)],
        "IsCrossRoad": [r.choice([0, 0, 1, 0x80]) for _ in range(4)],
        "IsPath": [r.choice(pred) for _ in range(4)],
        "IsFlag": [r.choice([0, 0, 1, -1]) for _ in range(4)],
        "IsRide": [r.choice([0, 0, 1]) for _ in range(4)],
        "IsEntrance": [r.choice([0, 0, 1]) for _ in range(4)],
        "IsExit": [r.choice([0, 0, 1]) for _ in range(4)],
        "GetObjectThingId": [thing],
        "LLCR_FixBridges": [r.choice([0, 1, 0x80])],
        "LLCR_FixBridgesReverse": [r.choice([0, 1])],
        "ModifyType": [r.choice([0, 1, -1, 2])],
    }
    small = [0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, r.randrange(-5, 6)]
    c = {"queues": q,
         "gLLCCC210": r.choice([0, 5, -1]), "gLLCCC214": r.choice([0, 7, -1]),
         "gLLCCDE0B4": r.choice(small), "gLLCCDE0B8": r.choice(small),
         "gLLCC47C8": r.choice([0, 0, 0, 1, 7]), "gLLCC4800": r.choice([-1, 0, 12]), "gLLCC4804": r.choice([-1, 0, 34]),
         "gValidTypeChangeLock": r.choice([0, 1, 5]), "gModifyLastMode": r.choice([0, 0x15, 3, 0x104, -1]),
         "gModifyTypeRefused": r.choice([0, 1, 2]), "gModifyPylonIndex": r.choice([0, 1, 0x7FFF, 0xFFFF, -1, 0x12345]),
         "gLLCCDB68": r.choice([0, 1, 0x8001, 0xFFFF]), "gModifyNoParent": r.choice([0, 0, 1, 2]), "gModifyNoThing": no_thing,
         "cells": [cell(r, [0, 1, 2, 3, 4, 7, 9, 10, 11, 0x1e, 0x15, 8, r.randrange(0x20)]) for _ in range(NCELLS)],
         "tcells": [cell(r, [0xb, 0xc, 0x10, 0x11, 0x19, 0, 1, 9, 0xd, 0x12, 0xa, r.randrange(0x20)]) for _ in range(NTRACKS)],
         "target": r.randrange(NTRACKS), "entry0": r.choice([0, 0, 0x1234]), "f28": r.choice([0, 1, -1])}
    return c


def modify_case(r, mode=None):
    c = base(r)
    if mode is None:
        mode = r.choice(MODES + [r.randrange(0x400)])
    c["args"] = [mode, r.choice([5, 5, 0, 1, -1, 0x80, r.randrange(-3, 10)]), r.choice([0, 1, 3, -1, 0x80, r.randrange(-3, 10)])]
    return c


def modify_edges():
    """Every mode branch, the deep ones with many seeds (their paths need several fake answers to line up)."""
    r = random.Random(5637)
    for mode in MODES:
        for _ in range(DEEP.get(mode, 12)):
            yield modify_case(r, mode)


def modify_random(r):
    return modify_case(r)


TESTS = [
    FnTest("CMapCell::Modify", ppc, host, edge=modify_edges, rnd=modify_random),
]
