"""Unit w141, the camera modes and MAP_wall_in_way, against libtpw_unit_CameraModes.dylib (tests/unit_CameraModes.cpp), main app
("SimTheme Park"), src/game/TPCamera.cpp and src/game/park/Map.cpp:
  __sinit_TPCamera_cpp 0x10036518, SetCameraMode 0x1003384c, LoadCamera 0x10036218, SaveCamera 0x10035f4c,
  ToggleFirstPersonWalkAround 0x10033b90, Setup1stPersonRideMeshes 0x10032c64, ToggleRideRideIn1stPerson 0x10032ff4,
  SetupRideRideMatrixAndSuch 0x10032e40, MAP_wall_in_way 0x10101ca4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments and answer from the case's queues: the cell predicates (IsPath ... IsBridge,
GetNeighbours, GetDirection, GetTrackCellEquiv; each records the cell's index in the map, and answers from the queue named after it, the last
value repeating), IsRideRideable, CObject::GetDBInfo (the record's +0x118 word is the case's), DisableRideHighlight, IsCoaster,
GetCoaster1stPersonCar, ShowFirstPersonWindow, the two sprite switches, the procedural texturing trio, both EnableMeshDraw overloads,
FindDataNodeIndexByID (queue "Find", 0xFFFFFFFF for none), TPProcessEmbeddedData (FAKED here: the real one is another unit's), LbFile_Read
(the case's bytes from the queue "RdB", the count from "RdN") and LbFile_Write (records the bytes as hex, count from "WrN"), UDebug,
GAME_ASSERTMSG and fmod (the host's libm, as math.fmod). The own functions are real on both sides (SetCameraMode runs inside the others).

The world of a case: the camera globals, the ride sets (mesh, header, nodes, mini nodes, embedded data and records built from a spec),
gRideCountA/B, the ride pointers (gFirstPersonRideSet, matrix, flag word, node, the heads set), the map block with its world pointer, the
cells' words the function reads (+0, +0x10, +0x24), the track cells' +0x10, the thing rows. What is compared: the answer, the event log, the
globals and every ride set's flags, the entry nodes' flag words and the records' flag words; pointers as names (set, node, record, block).

No fused operations in the translated functions (the HardwareRounding hook only fixes fctiwz's saturation). Not tested: MAP_wall_in_way
with dir 4 or more (it reads an uninitialised stack halfword: the cell id would point before the map), a null ride set where the original
dereferences it, a found node with no embedded set, a width or height of 0.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CameraModes"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST)
SETMODE, LOADC, SAVEC, WALK, MESHES, TOGGLE, MATRIX, SINIT, WALL = (0x1003384C, 0x10036218, 0x10035F4C, 0x10033B90, 0x10032C64,
                                                                   0x10032FF4, 0x10032E40, 0x10036518, 0x10101CA4)
SIZES = {SETMODE: 792, LOADC: 732, SAVEC: 680, WALK: 508, MESHES: 428, TOGGLE: 408, MATRIX: 384, SINIT: 36, WALL: 1724}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
ASSERT, UDEBUG = 0x1000DDE8, 0x10000E40
SHOWFP, DISHIGH, ISCOASTER, COASTERCAR, ESP1, ESP3 = 0x101EF994, 0x100672E4, 0x10043330, 0x100433F4, 0x1007CFC8, 0x1007D280
PROCACTIVE, PROCON, PROCOFF, MESHDRAW, MESHRANGE, PROCESSEMB, FINDNODE = (0x10063700, 0x100635CC, 0x1006367C, 0x10065178, 0x100650C4,
                                                                         0x101FFB0C, 0x101FFDC8)
RIDEABLE, GETDBINFO, TRACKEQUIV, ISBRIDGE, GETNEIGH, GETDIR = 0x10032A44, 0x10107CD4, 0x100F4844, 0x100A34C4, 0x100816E8, 0x10081930
PREDICATES = {"CMapCell::IsPath": 0x1009C44C, "CMapCell::IsQueue": 0x1009C490, "CMapCell::IsRide": 0x1009C61C,
              "CMapCell::IsGrass": 0x1009C660, "CMapCell::IsEntrance": 0x1009C4DC, "CMapCell::IsExit": 0x1009C524,
              "CMapCell::IsTrack": 0x1009C96C, "CMapCell::IsNoGo": 0x1009C75C, "CMapCell::IsWalkable": 0x1009C6AC,
              "CMapCell::IsWalkableInFirstPerson": 0x1009C6F4, "CMapCell::IsWater": 0x1009C408, "CMapCell::IsTrackX": 0x1009CA44}

# ---- the Mac's addresses of the globals: name -> (address, width in bytes, count); the host library's come from tpw_cmo_addr(name)
GL = {
    "flags": (0x102B5A40, 4, 1), "instance": (0x102B5A3C, 4, 1), "f68": (0x1029CC68, 4, 1), "start": (0x1029CC6C, 4, 3),
    "pos": (0x1029CC78, 4, 3), "f5bf4": (0x102B5BF4, 4, 1), "yaw": (0x102B5BF0, 4, 1), "turn": (0x102B5BE4, 4, 1),
    "pitch": (0x102B5BE8, 4, 1), "spos": (0x102B5BCC, 4, 3), "syaw": (0x102B5BEC, 4, 1), "once0": (0x102B5A20, 1, 1),
    "once1": (0x102B5A21, 1, 1), "sentinel": (0x102A0AF0, 2, 1), "width": (0x102C6C68, 4, 1), "height": (0x102C6C64, 4, 1),
    "pointer": (0x102C6CF8, 4, 3), "countA": (0x102D19A8, 4, 1), "countB": (0x102D19AC, 4, 1), "iso": (0x1038071C + 0x3A, 1, 1),
}
FLOATS = {"f68", "start", "pos", "f5bf4", "yaw", "turn", "pitch", "spos", "syaw", "pointer"}
PTR = {"rideSet": 0x102B5A28, "matrix": 0x102B5A2C, "flagword": 0x102B5A30, "node": 0x102B5A34, "headsSet": 0x102B5A38}
RIDESETS = 0x102C79A4
G_WORLD, G_THINGS, UI = 0x1037EBFC, 0x1034CBFC, 0x10396440
BUFLABEL = {"f68": 61, "f5bf4": 62, "flags": 63, "pos": 64, "spos": 65, "syaw": 66}
FILE_LABEL, UI_LABEL = 9, 10
NSETS = 5            # ride numbers 1 .. NSETS
BLK = 0x1DB000
# ---- the host's offsets and sizes the module relies on, checked once against tpw_cmo_layout
# entries 0..2 and 21 hold pointers (host layout wider), so only 3..20 are asserted
HOST_LAYOUT = [4, 0xC0, 0xC4, 0xA0, 0x58, 0x14, 0x44, 0x46, 0x48, 0x70, 0x74, 0x7C, 0x24, 0, 0x10, 2, 0xA, 0x1C, 0x44, 0x28, 0x3A, 0x118]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only (Unicorn saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell_index(e, p):
    return (p - e.cm_blk) // 0x44


def track_index(e, p):
    return (p - e.cm_blk - 0x110000) // 0x28


def predicate(name, mask=MASK):
    def stub(e):
        e.cl.rec(name, [cell_index(e, e.r(3))])
        e.set_r(3, T.u32(e.cl.ret(name)) & mask)
    return stub


def stub_trackequiv(e):
    e.cl.rec("CCellBase::GetTrackCellEquiv", [cell_index(e, e.r(3))])
    e.set_r(3, e.cm_blk + 0x110000 + e.cl.ret("TrackEquiv") * 0x28)


def stub_isbridge(e):
    e.cl.rec("CTrackCell::IsBridge", [track_index(e, e.r(3))])
    e.set_r(3, T.u32(e.cl.ret("CTrackCell::IsBridge")))


def stub_getdbinfo(e):
    e.cl.rec("CObject::GetDBInfo", [e.cl.lab(e.r(3))])
    e.set_r(3, e.cm_info)


def stub_read(e):
    L = e.cl
    result, file, buffer, length, transferred = (e.r(i) for i in range(3, 8))
    L.rec("LbFile_Read", (L.lab(file), L.lab(buffer), length))
    e.write(buffer, bytes(L.ret("RdB") & 0xFF for _ in range(length)))
    n = L.ret("RdN", -1)
    e.write(transferred, struct.pack(">I", length if n < 0 else n & MASK))
    e.write(result, bytes(4))


def stub_write(e):
    L = e.cl
    result, file, buffer, length, transferred = (e.r(i) for i in range(3, 8))
    L.rec("LbFile_Write", (L.lab(file), L.lab(buffer), length), (e.read(buffer, min(length, 24)).hex(),))
    n = L.ret("WrN", -1)
    e.write(transferred, struct.pack(">I", length if n < 0 else n & MASK))
    e.write(result, bytes(4))


def fmod(e):
    x, y = e.f(1), e.f(2)
    if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
        e.set_f1(math.nan)
    else:
        e.set_f1(math.fmod(x, y))


for _name, _addr in PREDICATES.items():
    STUBS.internal(_addr, predicate(_name))
STUBS.internal(GETNEIGH, predicate("CCellBase::GetNeighbours", 0xFF))
STUBS.internal(GETDIR, predicate("CCellBase::GetDirection", 0xFF))
STUBS.internal(TRACKEQUIV, stub_trackequiv)
STUBS.internal(ISBRIDGE, stub_isbridge)
STUBS.internal(GETDBINFO, stub_getdbinfo)
STUBS.internal(RIDEABLE, CL.recorder("IsRideRideable", "Lu", ret="i"))
STUBS.internal(DISHIGH, CL.recorder("DisableRideHighlight", "i"))
STUBS.internal(ISCOASTER, CL.recorder("IsCoaster", "u", ret="i"))
STUBS.internal(COASTERCAR, CL.recorder("GetCoaster1stPersonCar", "u", ret="i"))
STUBS.internal(SHOWFP, CL.recorder("ShowFirstPersonWindow", "Li"))
STUBS.internal(ESP1, CL.recorder("ESP_SwitchToFirstPersonSprites"))
STUBS.internal(ESP3, CL.recorder("ESP_SwitchToThirdPersonSprites"))
STUBS.internal(PROCACTIVE, CL.recorder("IsProceduralTexturingActive", "", ret="i"))
STUBS.internal(PROCON, CL.recorder("EnableProceduralTexturing"))
STUBS.internal(PROCOFF, CL.recorder("DisableProceduralTexturing"))
STUBS.internal(MESHDRAW, CL.recorder("EnableMeshDraw", "L"))
STUBS.internal(MESHRANGE, CL.recorder("EnableMeshDrawRange", "Luu"))
STUBS.internal(PROCESSEMB, CL.recorder("TPProcessEmbeddedData", "L"))
STUBS.internal(FINDNODE, CL.recorder("FindDataNodeIndexByID", "Luu", ret=lambda e, L: L.ret("Find", MASK)))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(UDEBUG, lambda e: e.cl.rec("UDebug", (), (e.cstr(e.r(3))[:159],)))
STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("fmod", fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    byte = "big"
    setflags = 4

    def put(self, a, width, v):
        self.write(a, (v & ((1 << (8 * width)) - 1)).to_bytes(width, self.byte))

    def get(self, a, width):
        return int.from_bytes(self.read(a, width), self.byte)

    def glob_put(self, name, values):
        a, width, count = GL[name]
        base = self.g(name)
        for i in range(count):
            self.put(base + i * width, width, values[i])

    def glob_get(self, name, raw=None):
        a, width, count = GL[name]
        base = self.g(name)
        out = [self.get(base + i * width, width) for i in range(count)]
        if raw == name and self.byte == "big":
            # a word the original read and did not swap yet (failed read): its bytes are the file's, which the host holds little-endian
            out = [int.from_bytes(v.to_bytes(4, "big"), "little") for v in out]
        return [canon_f(v) for v in out] if name in FLOATS else out

    def ident(self, a):
        return "null" if a == 0 else self.names.get(a, "other")


class Ppc(Side):
    byte = "big"
    setflags = 4

    def __init__(self, e, case):
        self.e = e
        fixed = {GL[n][0]: lab for n, lab in BUFLABEL.items()}
        fixed[UI] = UI_LABEL
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)
        self.names = {}

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return GL[name][0]

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def label(self, a, number):
        self.L.labels[a] = number

    def ptr_put(self, name, a):
        self.put(PTR[name], 4, a)

    def ptr_get(self, name):
        return self.get(PTR[name], 4)

    def rideset_put(self, i, a):
        self.put(RIDESETS + 4 * i, 4, a)

    def rideset_get(self, i):
        return self.get(RIDESETS + 4 * i, 4)

    def block(self):
        blk = self.e.cm_blk = self.alloc(BLK)
        return blk

    def setworld(self, blk):
        self.put(G_WORLD, 4, blk - 0x2D8)

    def set_row(self, row, a):
        self.put(G_THINGS + 0x14 * row, 4, a)

    def ride_info(self, value):
        a = self.e.cm_info = self.alloc(0x200)
        self.put(a + 0x118, 4, value)

    def build_set(self, spec):
        """spec: see tpw_cmo_build_set. Returns the set and the names of its parts."""
        N, M, F46, E = spec[4], spec[5], spec[6], spec[7]
        p = spec[8:]
        node_flags, emb_flags, rec_flags, blk_flags, eflags = p[:N + M], p[N + M:N + M + E], p[N + M + E:N + M + 2 * E], \
            p[N + M + 2 * E:N + M + 3 * E], p[N + M + 3 * E]
        sa, mesh, header = self.alloc(0x100), self.alloc(0x40), self.alloc(0x100)
        nodes, minis = self.alloc(0xA0 * (N + 1)), self.alloc(0x58 * (M + 1))
        embedded, eset, records = self.alloc(0x14 * (E + 1)), self.alloc(0x20), self.alloc(0x14 * (E + 1))
        self.put(sa + 4, 4, spec[0])
        self.put(sa + 8, 4, mesh)
        self.put(sa + 0xC0, 4, spec[1])
        self.put(sa + 0xC4, 4, spec[2])
        self.put(mesh + 4, 4, header)
        self.put(header + 0x44, 2, N)
        self.put(header + 0x46, 2, F46)
        self.put(header + 0x48, 2, E)
        self.put(header + 0x70, 4, nodes)
        self.put(header + 0x74, 4, minis)
        self.put(header + 0x7C, 4, embedded)
        for i in range(N):
            self.put(nodes + 0xA0 * i, 4, node_flags[i])
        for i in range(M):
            self.put(minis + 0x58 * i, 4, node_flags[N + i])
        blocks = []
        for i in range(E):
            self.put(embedded + 0x14 * i, 4, emb_flags[i])
            self.put(records + 0x14 * i, 4, rec_flags[i])
            b = self.alloc(0x40) if blk_flags[i] else 0
            blocks.append(b)
            self.put(records + 0x14 * i + 4, 4, b)
        self.put(eset, 4, eflags)
        self.put(eset + 4, 4, records)
        if spec[3]:
            self.put(mesh + 0x28, 4, eset)
        parts = {"mesh": mesh, "header": header, "eset": eset if spec[3] else 0}
        for i in range(E):
            idx = i + F46
            parts["node%d" % i] = nodes + 0xA0 * idx if idx < N else minis + 0x58 * (idx - N)
            parts["rec%d" % i] = records + 0x14 * i
            parts["blk%d" % i] = blocks[i]
        return sa, parts

    def invoke(self, op, args):
        addr, kinds, result = OPS[op]
        words = []
        for kind, a in zip(kinds, args):
            words.append(a & MASK)
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, addr, words, max_insns=50_000_000)
        r3 &= MASK
        return s32(r3) if result == "i" else None


class Host(Side):
    byte = "little"
    setflags = 4

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {lab: lib.tpw_cmo_addr(n.encode()) for n, lab in BUFLABEL.items()}
        fixed[UI_LABEL] = lib.tpw_cmo_addr(b"ui")
        CL.host_begin(lib, fixed, case["queues"])
        self.names = {}

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cmo_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cmo_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, a, number):
        self.lib.tpw_cl_label(number, a)

    def ptr_put(self, name, a):
        self.lib.tpw_cmo_ptr_set(name.encode(), a)

    def ptr_get(self, name):
        return self.lib.tpw_cmo_ptr_get(name.encode())

    def rideset_put(self, i, a):
        self.lib.tpw_cmo_ride_set(i, a)

    def rideset_get(self, i):
        return self.lib.tpw_cmo_ride_set_get(i)

    def block(self):
        return self.lib.tpw_cmo_block()

    def setworld(self, blk):
        self.lib.tpw_cmo_world(blk)

    def set_row(self, row, a):
        self.lib.tpw_cmo_row(row, a)

    def ride_info(self, value):
        self.lib.tpw_cmo_ride_info(s32(value))

    def build_set(self, spec):
        E = spec[7]
        arr = (C.c_uint32 * len(spec))(*[v & MASK for v in spec])
        out = (C.c_uint64 * (3 + 3 * E + 1))()
        sa = self.lib.tpw_cmo_build_set(arr, out)
        parts = {"mesh": out[0], "header": out[1], "eset": out[2]}
        for i in range(E):
            parts["node%d" % i] = out[3 + i]
            parts["rec%d" % i] = out[3 + E + i]
            parts["blk%d" % i] = out[3 + 2 * E + i]
        return sa, parts

    def invoke(self, op, args):
        addr, kinds, result = OPS[op]
        values = []
        for kind, a in zip(kinds, args):
            values.append(a if kind == "p" else a & MASK if kind == "u" else s32(a))
        r = getattr(self.lib, "tpw_cmo_" + HOSTNAME[op])(*values)
        return s32(r) if result == "i" else None


OPS = {
    "mode": (SETMODE, "wwwwpw", "i"), "load": (LOADC, "p", "i"), "save": (SAVEC, "p", "i"), "meshes": (MESHES, "w", "v"),
    "matrix": (MATRIX, "ww", "i"), "toggle": (TOGGLE, "wwwpww", "v"), "walk": (WALK, "", "v"), "wall": (WALL, "uuuw", "i"),
}
HOSTNAME = {"mode": "setcameramode", "load": "load", "save": "save", "meshes": "setupmeshes", "matrix": "ridematrix",
            "toggle": "toggleride", "walk": "togglewalk", "wall": "wall"}


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_cmo_"
    f = getattr(lib, p + "addr")
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = getattr(lib, p + "alloc")
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    f = getattr(lib, p + "block")
    f.restype, f.argtypes = C.c_uint64, []
    getattr(lib, p + "world").argtypes = [C.c_uint64]
    getattr(lib, p + "row").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "ride_info").argtypes = [C.c_int32]
    getattr(lib, p + "ptr_set").argtypes = [C.c_char_p, C.c_uint64]
    f = getattr(lib, p + "ptr_get")
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "ride_set").argtypes = [C.c_int32, C.c_uint64]
    f = getattr(lib, p + "ride_set_get")
    f.restype, f.argtypes = C.c_uint64, [C.c_int32]
    f = getattr(lib, p + "build_set")
    f.restype, f.argtypes = C.c_uint64, [C.POINTER(C.c_uint32), C.POINTER(C.c_uint64)]
    f = getattr(lib, p + "rd32")
    f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "sinit").argtypes = [C.POINTER(C.c_uint32)]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    types = dict(w=C.c_int32, p=C.c_uint64, u=C.c_uint32)
    for op, (_, kinds, result) in OPS.items():
        f = getattr(lib, p + HOSTNAME[op])
        f.argtypes = [types[k] for k in kinds]
        f.restype = C.c_int32 if result == "i" else None
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    got = list(out)[:n]
    assert got[3:21] == HOST_LAYOUT[3:21] and n == 22, [hex(v) for v in got]
    lib.bound = True
    Host.setflags = got[0]


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals, ride sets, map and things: the same code for both memories."""
    for name, values in case["g"].items():
        s.glob_put(name, values)
    s.glob_put("start", [fb(475.0), fb(0.0), fb(175.0)])
    # ride sets
    for i in range(16):
        s.rideset_put(i, 0)
    parts = {}
    s.names = {}
    objs = {}
    for k, spec in sorted(case["sets"].items()):
        sa, pt = s.build_set(spec)
        parts[k] = pt
        objs[("set", k)] = sa
        s.names[sa] = "set%d" % k
        s.label(sa, 20 + k)
        s.label(pt["mesh"], 30 + k)
        for key, a in pt.items():
            if a:
                s.names[a] = "%s:%d" % (key, k)
        if k in case["defined"]:
            s.rideset_put(k, sa)
    s.parts, s.objs = parts, objs
    opaque = s.alloc(0x40)
    s.names[opaque] = "opaque"

    def resolve(ref):
        if ref is None:
            return 0
        if ref == "opaque":
            return opaque
        k, key = ref
        return objs[("set", k)] if key == "set" else parts[k][key]
    for name, ref in case["ptrs"].items():
        s.ptr_put(name, resolve(ref))
    # file handle, the out word
    f = s.alloc(16)
    s.label(f, FILE_LABEL)
    s.file = f
    s.out = s.alloc(4)
    s.put(s.out, 4, 0x5A5A5A5A)
    if "map" in case:
        build_map(s, case["map"], case["g"]["sentinel"][0])


def build_map(s, m, sentinel):
    blk = s.block()
    s.blk = blk
    s.setworld(blk)
    for idx, f in m["cells"].items():
        a = blk + idx * 0x44
        s.put(a, 2, f.get("id", 0))
        s.put(a + 0x10, 2, f.get("parent", 0))
        s.put(a + 0x24, 2, f.get("who", 0))
    for idx, parent in m["tracks"].items():
        s.put(blk + 0x110000 + idx * 0x28 + 0x10, 2, parent)
    for row in range(10):
        s.set_row(row, 0)
    for i, t in enumerate(m["things"]):
        a = s.alloc(0x40)
        s.put(a + 2, 1, t[0])
        s.put(a + 0xA, 2, t[1] or sentinel)   # a chain ends at the sentinel the walk stops on (0 when it is 0)
        s.put(a + 0x1C, 4, t[2])
        s.set_row(i + 1, a)
        s.label(a, 1 + i)
    if m["row0"]:
        a0 = s.alloc(0x40)
        s.put(a0 + 0xA, 2, sentinel)
        s.set_row(0, a0)
    s.ride_info(m["info"])


def execute(s, case):
    op, args = case["op"], list(case["args"])
    if op in ("load", "save"):
        return s.invoke(op, [s.file])
    if op in ("mode", "toggle"):
        i = 4 if op == "mode" else 3
        args[i] = s.out if args[i] else 0
    return s.invoke(op, args)


def observe(s, case, answer):
    r = {"answer": answer, "log": s.log()}
    for name in ("flags", "instance", "f68", "f5bf4", "yaw", "turn", "pitch", "pos", "spos", "syaw", "once0", "once1"):
        r[name] = s.glob_get(name, case.get("raw"))
    for name in PTR:
        r["p_" + name] = s.ident(s.ptr_get(name))
    r["out"] = s.get(s.out, 4)
    for k, pt in sorted(s.parts.items()):
        spec = case["sets"][k]
        sa = s.objs[("set", k)]
        r["set%d" % k] = [s.get(sa + s.setflags, 4)]
        for key, a in sorted(pt.items()):
            if key.startswith("node") or key.startswith("rec"):
                r["set%d" % k].append(s.get(a, 4))
        r["sets_slot%d" % k] = s.ident(s.rideset_get(k))
    return r


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


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
FLAGBITS = [2, 4, 0x10, 0x80, 0x84, 0x92, 0x400, 0x200, 0x800, 0x1000, 0x100, 0x20, 0x40, 0x8, 0x1]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rflags(r):
    v = 0
    for b in FLAGBITS:
        if r.random() < 0.25:
            v |= b
    if r.random() < 0.1:
        v = r.choice([0, 2, 4, 0x10, 6, 0x14, 0x16])
    return v


def rword(r):
    return r.randrange(0, 1 << 32)


def make_set(r):
    N, M = r.randrange(0, 4), r.randrange(0, 4)
    E = r.randrange(0, 5)
    E = min(E, N + M)
    F46 = r.randrange(0, N + M - E + 1)
    node_flags = [rword(r) if r.random() < 0.5 else rword(r) | 0x8000 if r.random() < 0.5 else rword(r) & ~0x8000 for _ in range(N + M)]
    emb_flags = [sum(b for b in (0x4000000, 0x8000000, 0x10000000, 0x20000000) if r.random() < 0.4) | (rword(r) & 0x30FF) for _ in range(E)]
    rec_flags = [rword(r) & 0xFFFFFFFF if r.random() < 0.5 else r.choice([0, 4, 1, 5]) for _ in range(E)]
    blk_flags = [r.randrange(2) for _ in range(E)]
    set_flags = r.choice([0, 4, 0x2000, 0x4000, 0x6000, rword(r), rword(r) & ~0x4000, rword(r) | 0x4000])
    return [set_flags, rword(r), rword(r), 1 if r.random() < 0.85 else 0, N, M, F46, E] + node_flags + emb_flags + rec_flags + blk_flags + [rword(r)]


def pick_ref(r, case, kinds=("set", "rec", "blk", "node")):
    """A pointer value naming a part of one of the case's sets (or None)."""
    options = [None, "opaque"]
    for k, spec in case["sets"].items():
        E = spec[7]
        options.append((k, "set"))
        options.append((k, "mesh"))
        for i in range(E):
            options += [(k, "rec%d" % i), (k, "node%d" % i), (k, "blk%d" % i)]
    ref = r.choice(options)
    if isinstance(ref, tuple) and ref[1] != "set" and case["parts_check"].get(ref) == 0:
        return None
    return ref


def base(r, op):
    """The world every case starts from: random globals, up to NSETS ride sets, the answer queues of the fakes."""
    c = {"op": op}
    g = {}
    g["flags"] = [rflags(r)]
    g["instance"] = [r.choice([0, 1, 2, rword(r)])]
    for name in ("f68", "f5bf4", "yaw", "turn", "pitch", "syaw"):
        g[name] = [rf(r)]
    for name in ("pos", "spos", "pointer"):
        g[name] = [rf(r) for _ in range(3)]
    g["once0"] = [r.choice([0, 1, 5])]
    g["once1"] = [r.choice([0, 1, 7])]
    g["sentinel"] = [0]
    g["width"] = [r.randrange(1, 129)]
    g["height"] = [r.randrange(1, 129)]
    g["countA"] = [r.randrange(0, 4)]
    g["countB"] = [r.randrange(0, 4)]
    g["iso"] = [r.randrange(2)]
    c["g"] = g
    n = r.randrange(0, NSETS + 1)
    c["sets"] = {k: make_set(r) for k in range(1, n + 1)}
    c["defined"] = {k for k in c["sets"] if r.random() < 0.9}
    c["parts_check"] = {}
    for k, spec in c["sets"].items():
        for i in range(spec[7]):
            c["parts_check"][(k, "blk%d" % i)] = spec[8 + spec[4] + spec[5] + 3 * spec[7] - spec[7] + i] if False else 1
        c["parts_check"][(k, "mesh")] = 1
    c["ptrs"] = {}
    for name in PTR:
        c["ptrs"][name] = pick_ref(r, c) if r.random() < 0.6 else None
    c["queues"] = {}
    return c


def fix_ptrs(c):
    """The pointers the functions write through must name something writable: a record flag word or a set."""
    for name in ("matrix", "node"):
        if isinstance(c["ptrs"][name], tuple) and c["ptrs"][name][1] in ("set", "mesh"):
            c["ptrs"][name] = "opaque"
    return c


def good_sets(c):
    return sorted(k for k in c["sets"] if k in c["defined"] and c["sets"][k][3] and c["sets"][k][0] is not None)


def real_flagword(r, c):
    """A flag word the functions may clear a bit in: a record's (or none)."""
    options = [None]
    for k, spec in c["sets"].items():
        if spec[3]:
            options += [(k, "rec%d" % i) for i in range(spec[7])]
    return r.choice(options)


def real_rideset(r, c):
    options = [None] + [(k, "set") for k in c["sets"]]
    return r.choice(options)


def heads_set(r, c):
    options = [None] + [(k, "set") for k in c["sets"]]
    return r.choice(options)


def pointers(r, c):
    c["ptrs"]["flagword"] = real_flagword(r, c)
    c["ptrs"]["rideSet"] = real_rideset(r, c)
    c["ptrs"]["headsSet"] = heads_set(r, c)
    fix_ptrs(c)


# ---- SetCameraMode
def mode_case(r, mode=None, flags=None, toggle=None, force=None, out=None, reset=None, proc=None):
    c = base(r, "mode")
    pointers(r, c)
    c["g"]["flags"] = [rflags(r) if flags is None else flags]
    ride = r.choice([0, 0] + list(range(1, NSETS + 1)))
    c["args"] = [r.choice([0, 2, 4, 0x40, 2, 4, 0x40, 1, 0x20, 6]) if mode is None else mode,
                 r.randrange(2) if toggle is None else toggle, ride, r.randrange(2) if force is None else force,
                 r.randrange(2) if out is None else out, r.randrange(2) if reset is None else reset]
    c["queues"] = {"IsProceduralTexturingActive": [r.randrange(2) if proc is None else proc]}
    return c


def mode_edges():
    r = random.Random(1401)
    for mode in (0, 2, 4, 0x40, 1, 6):
        for prev in (0, 2, 4, 0x10, 0x14, 0x12, 0x16):
            for extra in (0, 0x84, 0x92, 0x400, 0x200, 0x80):
                for toggle in (0, 1):
                    for force in (0, 1):
                        if r.random() < 0.35:
                            yield mode_case(r, mode, prev | extra | r.choice([0, 0x800, 0x1000]), toggle, force)
    for out in (0, 1):
        for reset in (0, 1):
            for proc in (0, 1):
                yield mode_case(r, 4, 0, 1, 0, out, reset, proc)
                yield mode_case(r, 4, 4 | 0x400, 1, 0, out, reset, proc)
                yield mode_case(r, 2, 2 | 0x200, 1, 0, out, reset, proc)


# ---- Setup1stPersonRideMeshes
def meshes_case(r, ride=None):
    c = base(r, "meshes")
    pointers(r, c)
    c["args"] = [r.choice([0] + list(range(1, NSETS + 1))) if ride is None else ride]
    return c


def meshes_edges():
    r = random.Random(1402)
    for flags in (0, 2, 4, 0x10, 6, 0x16, 0x1, 0x20):
        for ride in (0, 1, 2, 3, 5):
            for _ in range(2):
                c = meshes_case(r, ride)
                c["g"]["flags"] = [flags]
                yield c
    for a, b in ((0, 0), (1, 0), (0, 1), (2, 1), (3, 3)):
        c = meshes_case(r, 1)
        c["g"]["countA"], c["g"]["countB"] = [a], [b]
        yield c


# ---- SetupRideRideMatrixAndSuch
def matrix_case(r, ride=None, node=None):
    c = base(r, "matrix")
    pointers(r, c)
    ride = r.choice([0] + list(range(1, NSETS + 1))) if ride is None else ride
    c["args"] = [ride, r.choice([1, 2, 3, 0x10, 0]) if node is None else node]
    spec = c["sets"].get(ride)
    finds = []
    for _ in range(2):
        if spec and spec[3] and spec[7] and r.random() < 0.7:
            finds.append(r.randrange(spec[7]))
        else:
            finds.append(MASK)
    if not (spec and spec[3] and spec[7]):
        finds = [MASK, MASK]
    c["queues"] = {"Find": finds}
    return c


def matrix_edges():
    r = random.Random(1403)
    for ride in (0, 1, 2, 3, 6):
        for node in (1, 2, 5):
            for flags in (0, 4, 2):
                for _ in range(3):
                    c = matrix_case(r, ride, node)
                    c["g"]["flags"] = [flags]
                    yield c


# ---- ToggleRideRideIn1stPerson
def toggle_case(r):
    c = base(r, "toggle")
    pointers(r, c)
    defined = sorted(c["defined"])
    ride = r.choice([0] + defined * 2) if defined else 0
    car = r.choice([0] + defined * 2) if defined else 0
    c["args"] = [ride, r.choice([0, 1, 2]), r.randrange(2), r.randrange(2), r.choice([1, 2, 3]), r.randrange(2)]
    flags = rflags(r)
    c["g"]["flags"] = [flags]
    spec_ok = lambda k: k in c["sets"] and k in c["defined"]
    # the ride the original ends up using must have a set when it dereferences it
    c["queues"] = {"IsCoaster": [r.randrange(2)], "GetCoaster1stPersonCar": [car],
                   "IsProceduralTexturingActive": [r.randrange(2)]}
    chosen = car if False else None
    # fix the set pointers the function writes through
    usable = [k for k in c["defined"]]
    if c["queues"]["IsCoaster"][0] and car == 0:
        c["queues"]["IsCoaster"] = [0]
    spec = None
    final = car if c["queues"]["IsCoaster"][0] else ride
    if final and final not in c["defined"]:
        c["args"][0] = 0
        c["queues"]["IsCoaster"] = [0]
        final = 0
    if final:
        spec = c["sets"][final]
        idx = r.randrange(spec[7]) if spec[7] and spec[3] and r.random() < 0.8 else MASK
        c["queues"]["Find"] = [idx, idx]
        if not spec[3] and not (flags & 4):
            c["queues"]["Find"] = [MASK, MASK]
    else:
        c["queues"]["Find"] = [MASK]
    if final and (flags & 4) == 0 and c["queues"]["Find"][0] != MASK and not spec[3]:
        c["queues"]["Find"] = [MASK]
    return c


def toggle_edges():
    r = random.Random(1404)
    for _ in range(120):
        yield toggle_case(r)


# ---- SaveCamera / LoadCamera
FILE_COUNTS = [0, 1, 3, 4, 5, 7, 8, 11, 12, 13, 23, 24, 25, 100, MASK]


def file_case(r, op, fail=None, heading=None, iso=None, short=None):
    c = base(r, op)
    pointers(r, c)
    items = [rword(r), heading if heading is not None else rf(r), rword(r), rword(r), rword(r), rword(r), rword(r), rword(r), rword(r), rword(r)]
    for i in range(10):
        if r.random() < 0.4:
            items[i] = rf(r)
    bytes_ = b"".join(struct.pack("<I", w & MASK) for w in items)
    c["g"]["pos"] = [rf(r) for _ in range(3)]
    if iso is not None:
        c["g"]["iso"] = [iso]
    counts = [-1] * 6
    if fail is not None:
        counts[fail] = r.choice([v for v in FILE_COUNTS if v != ((4 if fail in (0, 1, 2, 5) else 12))])
    key = "RdN" if op == "load" else "WrN"
    c["queues"] = {"RdB": list(bytes_) + [0], key: counts}
    if op == "load" and fail in (0, 1, 2, 5) and (counts[fail] & MASK) >> 2 != 1:
        c["raw"] = ["f68", "f5bf4", "flags", None, None, "syaw"][fail]
    c["args"] = []
    return c


def file_edges(op):
    def gen():
        r = random.Random(1405 if op == "load" else 1406)
        yield file_case(r, op)
        for step in range(6):
            for _ in range(3):
                yield file_case(r, op, step)
        if op == "load":
            half = 1.5707963705062866
            seeds = []
            for k in range(-6, 7):
                for d in (0.0, 1e-4, -1e-4, 1e-7, -1e-7, 0.7853982, -0.7853982, 0.7853983, -0.7853981, 0.7853981, 2.3561945, -2.3561945):
                    seeds.append(k * half + d)
            seeds += [0.0, -0.0, 3.0e38, -3.0e38, 1e-30, 100.0, -100.0, 1e6]
            for v in seeds:
                for iso in (1,):
                    yield file_case(r, op, None, fb(v), iso)
            for h in SPECIAL:
                yield file_case(r, op, None, h, 1)
                yield file_case(r, op, None, h, 0)
    return gen


# ---- ToggleFirstPersonWalkAround
def walk_case(r):
    c = base(r, "walk")
    c["args"] = []
    pointers(r, c)
    c["map"] = dict(cells={}, tracks={}, things=[], row0=False, info=0)
    c["g"]["flags"] = [rflags(r) if r.random() < 0.7 else r.choice([0, 2, 4, 0x10])]
    if r.random() < 0.7:
        c["g"]["pointer"] = [fb(r.uniform(-30, 1400)), rf(r), fb(r.uniform(-30, 1400))]
    c["queues"] = {n: [r.randrange(2) for _ in range(r.randrange(1, 4))] for n in
                   ("CMapCell::IsWalkable", "CMapCell::IsPath", "CMapCell::IsQueue", "CMapCell::IsGrass")}
    c["queues"]["IsProceduralTexturingActive"] = [r.randrange(2)]
    return c


def walk_edges():
    r = random.Random(1407)
    for flags in (0, 2, 4, 0x10, 0x84):
        for answers in ((0, 0, 0, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1), (1, 1, 1, 1)):
            for px, pz in ((50.0, 60.0), (-5.0, 10.0), (10.0, -5.0), (5000.0, 10.0), (10.0, 5000.0), (1270.0, 1270.0), (0.0, 0.0),
                           (float("nan"), 3.0), (4.0, float("inf"))):
                c = walk_case(r)
                c["g"]["flags"] = [flags]
                c["g"]["pointer"] = [fb(px), fb(0.0 if r.random() < 0.5 else 7.5), fb(pz)]
                c["queues"].update({n: [a] for n, a in zip(("CMapCell::IsWalkable", "CMapCell::IsPath", "CMapCell::IsQueue", "CMapCell::IsGrass"), answers)})
                if r.random() < 0.5:
                    yield c


# ---- MAP_wall_in_way
PRED_NAMES = list(PREDICATES) + ["CTrackCell::IsBridge", "CCellBase::GetNeighbours", "CCellBase::GetDirection"]


def wall_case(r, x=None, z=None, dir_=None, mode=None, uniform=None, entrance=None):
    c = base(r, "wall")
    pointers(r, c)
    x = r.choice([0, 1, 0x7F, r.randrange(128), r.randrange(128)]) if x is None else x
    z = r.choice([0, 1, 0x7F, r.randrange(128), r.randrange(128)]) if z is None else z
    d = r.randrange(4) if dir_ is None else dir_
    mode = r.choice([0, 1, 2, 2]) if mode is None else mode
    c["args"] = [x, z, d, mode]
    thisid = x + z * 128 + 1
    thatid = {0: (z - 1) * 128 + 1 + x, 1: x + z * 128 + 2, 2: (z + 1) * 128 + 1 + x, 3: x + z * 128}[d]
    edge = (x == 0 and d == 3) or (z == 0 and d == 0) or (x == 0x7F and d == 1) or (z == 0x7F and d == 2)
    n = r.randrange(0, 6)
    things = []
    for i in range(n):
        things.append((3 if r.random() < 0.4 else r.choice([1, 4, 0x13, 0]), r.choice([0] + list(range(1, n + 1))), r.randrange(0, 40)))
    # make the chains end: each thing's next is a later thing or 0
    things = [(k, nx if nx > i + 1 else 0, f) for i, (k, nx, f) in enumerate(things)]
    first = r.randrange(0, n + 1) if n else 0
    parent = r.randrange(1, 60)
    cells = {}
    if not edge:
        cells[thatid - 1] = dict(id=r.choice([thatid, r.randrange(0, 40)]), parent=parent)
        cells.setdefault(parent - 1, {})["who"] = first
    c["map"] = dict(cells=cells, tracks={}, things=things, row0=True, info=r.choice([0, 0, 1, 5, -1]))
    tp = {}
    for t in range(3):
        tp[t] = r.randrange(0, 40)
    c["map"]["tracks"] = tp
    if n and r.random() < 0.2:
        c["g"]["sentinel"] = [r.randrange(1, n + 1)]
    q = {}
    p1 = r.choice([0.2, 0.5, 0.8])
    for name in PRED_NAMES:
        if uniform is not None:
            v = uniform.get(name, 0)
            q[name] = [v]
        elif name == "CCellBase::GetNeighbours":
            q[name] = [r.randrange(256) for _ in range(2)]
        elif name == "CCellBase::GetDirection":
            q[name] = [r.choice([0, 1, 4, 0x10, 0x40, 2, 0x80, r.randrange(256)]) for _ in range(2)]
        else:
            q[name] = [1 if r.random() < p1 else 0 for _ in range(r.randrange(1, 4))]
    q["IsRideRideable"] = [r.randrange(2) for _ in range(2)]
    q["TrackEquiv"] = [r.randrange(0, 3)]
    c["queues"] = q
    return c


def wall_edges():
    r = random.Random(1408)
    for x, z, d in ((0, 5, 3), (5, 0, 0), (0x7F, 5, 1), (5, 0x7F, 2), (0, 0, 0), (0x7F, 0x7F, 2), (0, 0x7F, 3), (1, 1, 3), (5, 5, 0),
                    (5, 5, 1), (5, 5, 2), (5, 5, 3), (0x7E, 5, 1), (5, 0x7E, 2), (1, 5, 3), (5, 1, 0)):
        for mode in (0, 1, 2):
            yield wall_case(r, x, z, d, mode)
    for mode in (0, 1, 2):
        for d in range(4):
            for _ in range(60):
                yield wall_case(r, None, None, d, mode)
    # the entrance walk, scripted: IsWalkableInFirstPerson 0, IsEntrance 1, no earlier exit
    for chain in (0, 1, 2, 3):
        for rideable in (0, 1):
            for info in (0, 1):
                for sentinel in (0, 2):
                    c = wall_case(r, 5, 5, 2, 2, uniform={"CMapCell::IsEntrance": 1})
                    n = [[], [(1, 0, 0)], [(1, 2, 0), (3, 0, 9)], [(4, 2, 0), (1, 3, 0), (3, 0, 7)]][chain]
                    c["map"]["things"] = n
                    c["queues"]["IsRideRideable"] = [rideable]
                    c["map"]["info"] = info
                    c["g"]["sentinel"] = [sentinel]
                    first = 1 if n else 0
                    thatidx = 6 * 128 + 5
                    pr = c["map"]["cells"].get(thatidx, {}).get("parent", 7)
                    c["map"]["cells"][thatidx] = dict(id=thatidx + 1, parent=pr)
                    c["map"]["cells"].setdefault(pr - 1, {})["who"] = first
                    yield c


def wall_random(r):
    return wall_case(r)


# ---- __sinit_TPCamera_cpp: the load-time copy of the start position (the Mac: the function at 0x10036518)
def sinit_ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    old = STUBS.activate(e)
    try:
        e.write(0x1029CC6C, struct.pack(">3I", fb(475.0), fb(0.0), fb(175.0)))
        e.write(0x1029CC78, struct.pack(">3I", fb(1.5), fb(2.5), fb(3.5)))
        with ROUNDING(e):
            T.call_ppc(e, SINIT, [])
        return {"pos": list(struct.unpack(">3I", e.read(0x1029CC78, 12)))}
    finally:
        CL.Stubs.deactivate(e, old)


def sinit_host(lib, case):
    bind(lib)
    out = (C.c_uint32 * 3)()
    lib.tpw_cmo_sinit(out)
    return {"pos": list(out)}


def sinit_edges():
    yield {"queues": {}}


def sinit_random(r):
    return {"queues": {}}


def mode_random(r):
    return mode_case(r)


def meshes_random(r):
    return meshes_case(r)


def matrix_random(r):
    return matrix_case(r)


def toggle_random(r):
    return toggle_case(r)


def load_random(r):
    return file_case(r, "load", r.choice([None, None, 0, 1, 2, 3, 4, 5]), None, r.randrange(2))


def save_random(r):
    return file_case(r, "save", r.choice([None, None, 0, 1, 2, 3, 4, 5]))


def walk_random(r):
    return walk_case(r)


TESTS = [
    FnTest("__sinit_TPCamera_cpp", sinit_ppc, sinit_host, edge=sinit_edges, rnd=sinit_random),
    FnTest("SetCameraMode", ppc, host, edge=mode_edges, rnd=mode_random),
    FnTest("Setup1stPersonRideMeshes", ppc, host, edge=meshes_edges, rnd=meshes_random),
    FnTest("SetupRideRideMatrixAndSuch", ppc, host, edge=matrix_edges, rnd=matrix_random),
    FnTest("ToggleRideRideIn1stPerson", ppc, host, edge=toggle_edges, rnd=toggle_random),
    FnTest("LoadCamera", ppc, host, edge=file_edges("load"), rnd=load_random),
    FnTest("SaveCamera", ppc, host, edge=file_edges("save"), rnd=save_random),
    FnTest("ToggleFirstPersonWalkAround", ppc, host, edge=walk_edges, rnd=walk_random),
    FnTest("MAP_wall_in_way", ppc, host, edge=wall_edges, rnd=wall_random),
]
