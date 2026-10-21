"""Unit w185, the cell system's modes, ride words, costs, cursor, click stack and first block fix up, against libtpw_unit_LLCellModes.dylib
(tests/unit_LLCellModes.cpp), main app ("SimTheme Park"): src/game/park/LLCellSystem.cpp, the 23 functions of LLCellSystem.cpp's range
0x10090d40 to 0x10092254 (SetRideRotation .. FixFirstBlock).

A case is the cell system's words (every global the 23 functions touch, random with the edge values), the click stack and its copy (filled
with a pattern), the mouse (gMouse, gMouseMode, the two mouse words), the world (a raw block: the track cells of ids 0 to 40 and the large id
0x3fff at world + 0x1102d8, parent at +0x10) and a call. The world is built twice by ONE piece of Python: in emulator memory at the Mac's
addresses (big-endian) and in the host library's memory (little-endian). Every callee outside the unit is a fake on both sides that records
its arguments and answers from the case's queues: the action recorder (both overloads), the advisor (GetAdvisor, CancelMessage, the request
constructor, QueueRequest), the cursor, AbortMovePylon, the illegal placement pair, SetCoasterEdit, FlagItemAsSelected, IsXYValid, the track
cell and cell base members, CTRACE, GAME_ASSERTMSG, the nothrow operator new and a fake current mouse mode. The landed src/game/Mouse.cpp
(AssignMouseMode, CMMDefault) runs for real on both sides, as do OPPOSITE and ADD_DIRECTION (the host copies their bodies).

What is compared: the answer, the event log, every word the functions can store, both click arrays and the mouse state.

Not tested: negative click counts, a click count above 0x400 for the copy, the three bytes after gLLCC47C4 (the overflow of AddToClickStack
writes a whole word there on the Mac), a NULL world, a track cell id past 0x3fff, SetCoasterEdit's word beyond 32 bits.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LLCellModes"
MASK = 0xFFFFFFFF

# ---- the 23 functions: name -> (address, host wrapper)
FN = dict(
    SetRideRotation=(0x10090D40, "SetRideRotation"), GetRideRotation=(0x10090DE0, "GetRideRotation"),
    SystemSetMode=(0x10090E18, "SystemSetMode"), SystemSetModeNR=(0x100911F0, "SystemSetModeNR"),
    SystemGetLastMode=(0x1009152C, "SystemGetLastMode"), SystemIsMode=(0x10091568, "SystemIsMode"),
    SystemSetRide=(0x100915BC, "SystemSetRide"), SystemSetCursor=(0x1009179C, "SystemSetCursor"),
    SystemSetRideQueue=(0x10091994, "SystemSetRideQueue"), SystemGetRideQueue=(0x10091A08, "SystemGetRideQueue"),
    SystemSetRideTrack=(0x10091A5C, "SystemSetRideTrack"), SystemGetRideTrack=(0x10091AD0, "SystemGetRideTrack"),
    SystemGetPathCost=(0x10091B60, "GetPathCost"), SystemGetQueueCost=(0x10091BD8, "GetQueueCost"),
    SystemGetKartTrackCost=(0x10091C54, "GetKartTrackCost"), SystemGetWaterTrackCost=(0x10091CD4, "GetWaterTrackCost"),
    SystemSetSessionCost=(0x10091D14, "SetSessionCost"), SystemAddToSessionCost=(0x10091D50, "AddToSessionCost"),
    SystemGetSessionCost=(0x10091D98, "GetSessionCost"), SystemGetClickStackEntries=(0x10091E18, "GetClickStackEntries"),
    SystemAddToClickStack=(0x10091E5C, "AddToClickStack"), SystemMakeClickStackCopy=(0x10091ED0, "MakeClickStackCopy"),
    FixFirstBlock=(0x10091F5C, "FixFirstBlock"),
)
OWN = {a for a, _ in FN.values()}
# the landed originals of src/game/Mouse.cpp that run for real, and OPPOSITE / ADD_DIRECTION
LANDED = {0x1007E2E0, 0x1007EA78, 0x100813E8, 0x10081468, 0x1007EAC4, 0x1007EAF8, 0x1007EB2C}
REAL_OTHER = {0x10102500, 0x1010254C}

# ---- the callees faked on both sides
ASSERT, CTRACE, NEW, CHECK_CONTEXT = 0x1000DDE8, 0x1006FCBC, 0x102384B8, 0x101EFB0C
REC2, REC3, GET_ADVISOR, CANCEL, QUEUE, REQ_CTOR = 0x101441FC, 0x101445D0, 0x1013EAB0, 0x1000B048, 0x1000AE48, 0x1000DEB4
ABORT, SET_ILL, GET_ILL, CURSOR, COASTER_EDIT, FLAG_ITEM, IS_XY = (0x10057DF4, 0x10088934, 0x10088970, 0x101A8C58, 0x10058758, 0x10140EE4,
                                                                   0x101012F4)
IS_TRACK, IS_START, IS_CORNER, SET_HFIELD, GET_NEIGH = 0x100A3574, 0x100A36CC, 0x100A37A0, 0x100A3BAC, 0x100816E8
MOD_DIR, MOD_NEIGH, REM_NEIGH, REM_DIR, GET_NB_CELL = 0x10081810, 0x10081608, 0x10081678, 0x1008184C, 0x1012FDDC

# ---- the Mac's addresses of the globals: name -> (address, width, signed)
WORDS = dict(
    gLLCC4800=(0x102E4800, 4), gLLCC4804=(0x102E4804, 4), gLLCC4808=(0x102E4808, 4), gLLCC480C=(0x102E480C, 4),
    gLLCC4810=(0x102E4810, 4), gLLCC4814=(0x102E4814, 4), gLLCC47C4=(0x102E47C4, 1), gLLCC47C8=(0x102E47C8, 4),
    gLLCC47CC=(0x102E47CC, 4), gLLCC47D0=(0x102E47D0, 4), gLLCC47D4=(0x102E47D4, 4), gLLCC47D8=(0x102E47D8, 4),
    gLLCC47DC=(0x102E47DC, 4), gLLCC47E8=(0x102E47E8, 4), gLLCC47EC=(0x102E47EC, 4), gLLCCC200=(0x102DC200, 4),
    gLLCCC204=(0x102DC204, 4), gLLCCC208=(0x102DC208, 4), gLLCCDB6C=(0x102DDB6C, 4), gLLCCDE0E2=(0x102DE0E2, 2),
    gLLCCE0DC=(0x102DE0DC, 4), gLLCCE0E0=(0x102DE0E0, 2), gLLCC9F14=(0x1029DF14, 4), gLLCC9F24=(0x1029DF24, 2),
    gLLCC9F26=(0x1029DF26, 2), gLLCCE288=(0x1029E288, 4), gLLCCE28C=(0x1029E28C, 4), gLLCCDB58=(0x102DDB58, 2),
    gLLCCDB5A=(0x102DDB5A, 2), gLLCCFirstBlockX=(0x1029E02C, 4), gLLCCFirstBlockY=(0x1029E030, 4),
    gLLCCClickStackCount=(0x102E47C0, 4), gLLCCClickCopyIndex=(0x102E27B8, 4), gLLCCClickCopyCount=(0x102E27BC, 4),
)
ARRAYS = dict(gLLCCClickStack=0x102E27C0, gLLCCClickStackCopy=0x102E07B8)
G = dict(mouse=0x102DC1F4, mode=0x102DC1F0, w1e0=0x102DC1E0, w1e4=0x102DC1E4, world=0x1037EBFC, gtype=0x102B3AA0, guard=0x102B3AA4)
HOST_LAYOUT = [0x28, 0x1102D8, 0x10, 8]
UNSIGNED = {"gLLCC47C4", "gLLCCDE0E2", "gLLCCE0E0", "gLLCC9F24", "gLLCC9F26", "gLLCCDB58", "gLLCCDB5A"}
FMT = {1: "B", 2: "H", 4: "I"}
LARGE = 499          # label of the cell of id 0x3fff
NCELL = 40           # cells 0 to 40 are zeroed and labelled 400 + k


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

R = CL.recorder


def returns(name, spec, key, mask=MASK):
    return R(name, spec, ret=lambda e, L: L.ret(key, 0) & mask)


def delete(e, a):
    L = e.cl
    if L.lab(a) > 0:
        L.rec("delete", (L.lab(a),))
    L.forget(a)


def destructor(name):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
        if T.s16(e.r(4)) > 0:
            delete(e, e.r(3))
    return stub


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def assign(name):
        return lambda e: e.cl.rec(name, (e.cl.lab(e.r(3)), e.cl.lab(e.u32(G["mode"]))))
    vt = T.persist_alloc(e, 0x40)
    for slot, fn in {8: destructor("Mode.dtor"), 0x2C: R("Mode.GetMode", "L", ret=lambda e, L: L.ret("ModeGetMode", 0) & MASK),
                     0x30: assign("Mode.OnAssign"), 0x34: assign("Mode.OnRelease")}.items():
        ws(e, vt + slot, [T.fake_function(e, fn)])
    e.llm_mode_vt = vt
    e.llm_world = T.persist_alloc(e, 0x1B1000)
    e.llm_adv = T.persist_alloc(e, 0x40)
    e.llm = None
    e.llm_req = 0


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)
    S.real.update(LANDED)
    S.real.update(REAL_OTHER)

    def new(e):
        L, size = e.cl, e.r(3)
        if L.ret("New", 1) == 0:
            L.rec("operator_new", (size, 0))
            e.set_r(3, 0)
            return
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        L.newlab(block)
        L.rec("operator_new", (size, L.lab(block)))
        e.set_r(3, block)

    def new_glue(e):     # the glue saves the caller's TOC at 0x14(r1) before the call, the caller reloads it afterwards
        e.write(e.r(1) + 0x14, struct.pack(">I", e.toc))
        new(e)

    def ctrace(e):
        text = e.cstr(e.r(4))[:159]
        ints = [e.r(3) & MASK]
        if "%d" in text:
            ints.append(s32(e.r(5)))
        e.cl.rec("CTRACE", ints, (text,))

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def get_advisor(e):
        e.cl.rec("GetAdvisor")
        e.set_r(3, e.llm.a(30))

    def req_ctor(e):
        e.llm_req = e.r(3)
        e.cl.rec("CAdvisorRequest", (s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))))

    def queue(e):
        e.cl.rec("QueueRequest", (e.cl.lab(e.r(3)), 1 if e.r(4) == e.llm_req else 0))

    def nb_cell(e):
        L = e.cl
        L.rec("GetNeighbouringCell", (L.lab(e.r(3)), e.r(4) & 0xFF, s32(e.r(5))))
        k = L.ret("Cell", 0)
        e.set_r(3, e.llm.a(400 + k) if k else 0)

    S.imp("__nw__FUl", new)
    S.internal(NEW, new_glue)
    S.imp("__dl__FPv", lambda e: delete(e, e.r(3)))
    S.internal(CTRACE, ctrace)
    S.internal(ASSERT, assert_)
    S.internal(CHECK_CONTEXT, R("CheckMouseContext", "xi"))
    S.internal(REC2, R("RecordAction", "xiH"))
    S.internal(REC3, R("RecordAction3", "xiHb"))
    S.internal(GET_ADVISOR, get_advisor)
    S.internal(CANCEL, R("CancelMessage", "Li"))
    S.internal(REQ_CTOR, req_ctor)
    S.internal(QUEUE, queue)
    S.internal(ABORT, R("AbortMovePylon"))
    S.internal(SET_ILL, R("LLCM_SetIllegalPlacement", "i"))
    S.internal(GET_ILL, R("LLCM_GetIllegalPlacement", "", ret=lambda e, L: L.ret("GetIllegal", 0) & MASK))
    S.internal(CURSOR, R("UI_SetGameWindowCursor", "h"))
    S.internal(COASTER_EDIT, R("SetCoasterEdit", "ui"))
    S.internal(FLAG_ITEM, R("FlagItemAsSelected", "xw"))
    S.internal(IS_XY, R("IsXYValid", "ii", ret=lambda e, L: L.ret("IsXYValid", 0) & MASK))
    S.internal(IS_TRACK, returns("IsTrack", "L", "IsTrack"))
    S.internal(IS_START, returns("IsTrackStartBlock", "L", "IsStart"))
    S.internal(IS_CORNER, returns("IsCorner", "L", "IsCorner"))
    S.internal(SET_HFIELD, R("SetHFieldTile", "L"))
    S.internal(GET_NEIGH, returns("GetNeighbours", "L", "Neighbours"))
    S.internal(MOD_DIR, R("ModifyDirection", "Lb"))
    S.internal(MOD_NEIGH, R("ModifyNeighbours", "Lb"))
    S.internal(REM_NEIGH, R("RemoveNeighbour", "Lb"))
    S.internal(REM_DIR, R("RemoveDirection", "Lb"))
    S.internal(GET_NB_CELL, nb_cell)
    return S


STUBS = build_stubs()

# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def setw(self, name, v):
        a, width = WORDS[name][0], WORDS[name][1]
        self.write(self.gaddr(name, a), struct.pack(self.fmt[0] + FMT[width], v & ((1 << (8 * width)) - 1)))

    def getw(self, name):
        a, width = WORDS[name][0], WORDS[name][1]
        v = struct.unpack(self.fmt[0] + FMT[width], self.read(self.gaddr(name, a), width))[0]
        if name not in UNSIGNED and v >= 1 << (8 * width - 1):
            v -= 1 << (8 * width)
        return v

    def put_array(self, name, words):
        self.write(self.gaddr(name, ARRAYS[name]), struct.pack(self.fmt[0] + "I" * len(words), *words))

    def get_array(self, name):
        return list(struct.unpack(self.fmt[0] + "I" * 0x800, self.read(self.gaddr(name, ARRAYS[name]), 0x2000)))


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.addr = {}
        e.llm = self

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def gaddr(self, name, mac):
        return mac

    def make(self, label, size):
        e = self.e
        if label == 9:
            a = e.alloc(8, struct.pack(">I", e.llm_mode_vt) + b"\xa5" * 4)
        elif label == 8:
            a = e.alloc(16, b"\xa5" * 16)
        elif label == 30:
            a = e.llm_adv
        else:
            a = e.alloc(size, b"\0" * size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.L.labels[a] = label
        self.addr[label] = a

    def a(self, label):
        return self.addr.get(label, 0)

    def world(self):
        return self.e.llm_world

    def set_world(self):
        ws(self.e, G["world"], [self.world()])
        ws(self.e, G["gtype"], [0])      # the game type (normal), its guard set: the Mac's constructor is not run
        self.e.write(G["guard"], b"\x01")

    def setg(self, key, value):
        ws(self.e, G[key], [value])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def dump(self):
        e, L = self.e, self.L
        u32 = e.u32
        return [L.lab(u32(G["mouse"])), L.lab(u32(G["mode"])), s32(u32(G["w1e0"])), s32(u32(G["w1e4"]))]

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]

    def id_block(self, v):
        return self.e.alloc(4, struct.pack(">HH", v & 0xFFFF, 0))


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_llm_init()
        CL.host_begin(lib, {}, case["queues"])
        self.addr = {}
        self.cache = {}

    def done(self):
        pass

    def gaddr(self, name, mac):
        if name not in self.cache:
            self.cache[name] = self.lib.tpw_llm_gaddr(name.encode())
            assert self.cache[name], name
        return self.cache[name]

    def make(self, label, size):
        a = self.lib.tpw_llm_fake_mode() if label == 9 else self.lib.tpw_llm_obj(label, size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.lib.tpw_cl_label(label, C.c_void_p(a))
        self.addr[label] = a

    def a(self, label):
        return self.addr.get(label, 0)

    def world(self):
        return self.lib.tpw_llm_gaddr(b"world")

    def set_world(self):
        pass

    def setg(self, key, value):
        self.lib.tpw_llm_set({"mouse": 1, "mode": 2, "w1e0": 3, "w1e4": 4}[key], value)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def dump(self):
        out = (C.c_int64 * 8)()
        self.lib.tpw_llm_dump(out)
        return list(out)[:4]

    def id_block(self, v):
        return v & 0xFFFF


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, i64 = C.c_uint64, C.c_int32, C.c_int64
    lib.tpw_llm_gaddr.restype, lib.tpw_llm_gaddr.argtypes = u64, [C.c_char_p]
    lib.tpw_llm_init.argtypes = []
    lib.tpw_llm_obj.restype, lib.tpw_llm_obj.argtypes = u64, [i32, C.c_uint32]
    lib.tpw_llm_fake_mode.restype, lib.tpw_llm_fake_mode.argtypes = u64, []
    lib.tpw_llm_set.argtypes = [i32, i64]
    lib.tpw_llm_dump.argtypes = [C.POINTER(i64)]
    lib.tpw_llm_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_cl_label.argtypes = [i32, C.c_void_p]
    for name, (_, wrapper) in FN.items():
        f = getattr(lib, "tpw_llm_" + wrapper)
        f.restype = i32
        f.argtypes = [i32] * ARGC[name]
    lib.tpw_llm_init()
    out = (i64 * 32)()
    n = lib.tpw_llm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


ARGC = dict(SetRideRotation=2, GetRideRotation=0, SystemSetMode=2, SystemSetModeNR=2, SystemGetLastMode=0, SystemIsMode=1, SystemSetRide=2,
            SystemSetCursor=0, SystemSetRideQueue=1, SystemGetRideQueue=0, SystemSetRideTrack=1, SystemGetRideTrack=0, SystemGetPathCost=0,
            SystemGetQueueCost=0, SystemGetKartTrackCost=0, SystemGetWaterTrackCost=0, SystemSetSessionCost=1, SystemAddToSessionCost=1,
            SystemGetSessionCost=0, SystemGetClickStackEntries=0, SystemAddToClickStack=2, SystemMakeClickStackCopy=0, FixFirstBlock=1)

# ---------------------------------------------------------------------------------------------------------------- the world


def cell_address(s, k):
    return s.world() + 0x1102D8 + (k - 1) * 0x28


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    s.set_world()
    s.name(1, s.world())
    s.make(30, 0x40)
    mouse = s.make(8, 16)
    fake = s.make(9, 8)
    # the track cells: ids 0 to NCELL and the large one, zeroed, labelled 400 + k, parent at +0x10
    s.write(cell_address(s, 0), b"\0" * (0x28 * (NCELL + 1)))
    s.write(cell_address(s, 0x3FFF), b"\0" * 0x28)
    for k in range(0, NCELL + 1):
        s.name(400 + k, cell_address(s, k))
        s.half(cell_address(s, k) + 0x10, case["parents"][k])
    s.name(LARGE, cell_address(s, 0x3FFF))
    s.half(cell_address(s, 0x3FFF) + 0x10, case["parents"][NCELL + 1])
    for name, v in case["w"].items():
        s.setw(name, v)
    salt = case["salt"]
    s.put_array("gLLCCClickStack", [((i * 0x9E3779B1) + salt) & MASK for i in range(0x800)])
    s.put_array("gLLCCClickStackCopy", [((i * 0x85EBCA6B) ^ salt ^ 0xA5A5A5A5) & MASK for i in range(0x800)])
    s.setg("mouse", mouse if case["mouse"] else 0)
    s.setg("mode", fake if case["mode"] else 0)
    s.setg("w1e0", case["w1e0"])
    s.setg("w1e4", case["w1e4"])


def execute(s, case):
    name, args = case["fn"], list(case["args"])
    address, wrapper = FN[name]
    if name == "SystemSetRide":
        args[0] = s.id_block(args[0])
    if isinstance(s, Ppc):
        r = s.call(address, *args)
    else:
        r = getattr(s.lib, "tpw_llm_" + wrapper)(*[s32(a) for a in args])
    return (r & MASK) if case["ret"] else None


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    out = {"answer": answer, "log": s.log(), "state": s.dump(), "words": {n: s.getw(n) for n in WORDS}}
    out["stack"] = s.get_array("gLLCCClickStack")
    out["copy"] = s.get_array("gLLCCClickStackCopy")
    # the three bytes after gLLCC47C4 are not modelled; the overflow writes a whole word there on the Mac
    return out


def run(s, case):
    try:
        build(s, case)
        answer = execute(s, case)
        return observe(s, case, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [-1, 0, 1, 2, 3, 4, 11, 16, 21, 22, 57, 0x15, 0x16, 0x35, 0x3B, 0x3C, 0x7FFFFFFF, 100, -0x80000000]
CURSOR_MODES = list(range(0, 0x3D)) + [-1, 0x7FFFFFFF, -0x80000000, 0x100]
FLAGS = [0, 1]
WORDV = [0, 1, -1, 5, 0x7FFFFFFF, -0x80000000, 0x12345678, 0x400, 0x3FF, 255, 256]
SAFE_XY = [(0, 0), (1, 0), (7, 0), (31, 0), (-1, 0), (0x10003, 0), (5, 0x1000000), (0x3FFE, 0), (12, 0), (2, 0)]
IDS = [0, 1, 2, 5, 0x7F, 0x80, 0xFFFF, 0x1234, 0x8000]


def id_of(x, y):
    return ((x & MASK) + ((y << 7) & MASK) + 1) & 0xFFFF


def base(r, fn, args=(), ret=True, **over):
    """The world every case starts from: random words and answer queues that cover every branch."""
    w = {}
    for name, (_, width) in WORDS.items():
        if width == 1:
            w[name] = r.choice([0, 1, 0x80, 0xFF, r.randrange(256)])
        elif width == 2:
            w[name] = r.choice([0, 1, 0xFFFF, 0x8000, r.randrange(0x10000)])
        else:
            w[name] = r.choice(WORDV + [r.randrange(1 << 32) - (1 << 31)])
    for name in ("gLLCC4808", "gLLCC480C", "gLLCC4810", "gLLCC4814"):
        w[name] = r.choice(MODES)
    for name in ("gLLCCC200", "gLLCCC204", "gLLCCC208", "gLLCCE0DC"):
        w[name] = r.choice([0, 0, 1, -1, 7])
    w["gLLCCClickStackCount"] = r.choice([0, 1, 2, 5, 0x3FF])
    w["gLLCCClickCopyIndex"] = r.choice([0, 7, -1])
    x, y = r.choice(SAFE_XY)
    w["gLLCCFirstBlockX"], w["gLLCCFirstBlockY"] = x, y
    w.update(over.pop("w", {}))
    ncell = 12

    def seq(*vals):
        return [r.choice(vals) for _ in range(ncell)]
    queues = {"New": [r.choice([1, 1, 1, 0]) for _ in range(4)], "ModeGetMode": [r.choice([0, 1, 2, 8])],
              "GetIllegal": [r.choice([0, 0, 1, 2, -1])], "IsXYValid": [r.choice([0, 1, 1, 1])],
              "IsTrack": seq(0, 1, 1, 2, -1), "IsStart": seq(0, 1, 1, 5), "IsCorner": seq(0, 1, 1, 0x7F),
              "Neighbours": seq(0, 1, 4, 16, 64, 5, 0x55, 0xAA, 0xFF, r.randrange(256)),
              "Cell": seq(0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9)}
    queues.update(over.pop("queues", {}))
    parents = [r.choice([0, 1, 1, 2]) for _ in range(NCELL + 1)] + [r.choice([0, 1, 2])]
    case = dict(fn=fn, args=list(args), ret=ret, w=w, queues=queues, parents=parents, salt=r.randrange(1 << 32),
                mouse=r.choice([0, 1, 1]), mode=r.choice([0, 1]), w1e0=r.choice([0, 5, -1, 7]), w1e4=r.choice([0, 5, -1, 7]))
    case.update(over)
    return case


def rnd_args(r, fn):
    if fn in ("SetRideRotation", "SystemSetMode", "SystemSetModeNR"):
        return [r.choice(MODES + [r.randrange(1 << 32) - (1 << 31)]), r.choice(FLAGS)]
    if fn in ("SystemIsMode", "SystemSetRideQueue", "SystemSetRideTrack", "SystemSetSessionCost", "SystemAddToSessionCost"):
        return [r.choice(MODES + WORDV + [r.randrange(1 << 32) - (1 << 31)])]
    if fn == "SystemSetRide":
        return [r.choice(IDS), r.choice(WORDV)]
    if fn == "SystemAddToClickStack":
        return [r.choice(WORDV + [r.randrange(1 << 32) - (1 << 31)]), r.choice(WORDV + [r.randrange(1 << 32) - (1 << 31)])]
    if fn == "FixFirstBlock":
        return [r.choice(FLAGS)]
    return []


def returns_value(fn):
    return fn.startswith("SystemGet") or fn.startswith("GetRide") or fn in ("SystemIsMode", "SystemAddToClickStack")


def simple(fn):
    def rnd(r):
        return base(r, fn, rnd_args(r, fn), ret=returns_value(fn))
    return rnd


def simple_edges(fn):
    def edges():
        r = random.Random(hash(fn) & 0xFFFF)
        out = []
        for v in (WORDV if rnd_args(r, fn) else [0]):
            for _ in range(2):
                args = rnd_args(r, fn)
                if args:
                    args[0] = v
                out.append(base(r, fn, args, ret=returns_value(fn)))
        return out
    return edges


def mode_edges(fn):
    def edges():
        r = random.Random(11)
        out = []
        for mode in MODES:
            for flag in FLAGS:
                for same in (False, True):
                    for last in (0x15, 0x16, 0, 3):
                        w = {"gLLCC480C": mode if same else r.choice([m for m in MODES if m != mode]), "gLLCC4808": r.choice(MODES),
                             "gLLCC4810": r.choice([0, 5]), "gLLCC4814": r.choice(MODES)}
                        out.append(base(r, fn, [mode, flag], ret=False, w=w))
                        if mode == 0:
                            # the last mode after the call is the old current mode: make it 0x15 / 0x16 as well
                            w2 = dict(w, gLLCC480C=last)
                            out.append(base(r, fn, [mode, flag], ret=False, w=w2))
        return out
    return edges


def rotation_edges():
    r = random.Random(12)
    out = []
    for rot in (0, 1, -1, 0x10000, 0x12345, 5):
        for flag in FLAGS:
            for eq in (True, False):
                out.append(base(r, "SetRideRotation", [rot, flag], ret=False, w={"gLLCC4810": rot if eq else rot + 1}))
    return out


def cursor_edges():
    r = random.Random(13)
    out = []
    for mode in CURSOR_MODES:
        for c200 in (0, 1):
            for c208 in (0, 1):
                out.append(base(r, "SystemSetCursor", [], ret=False, w={"gLLCC480C": mode, "gLLCCC200": c200, "gLLCCC208": c208}))
    return out


def click_edges(fn):
    def edges():
        r = random.Random(14)
        out = []
        counts = [0, 1, 2, 0x3FE, 0x3FF, 0x400, 0x401, 0x7FFFFFFF, 0x1000]
        for count in counts:
            for _ in range(3):
                args = rnd_args(r, fn)
                if fn == "SystemAddToClickStack":
                    args = [r.choice(WORDV + [r.randrange(1 << 32) - (1 << 31)]), r.choice(WORDV + [r.randrange(1 << 32) - (1 << 31)])]
                if fn == "SystemMakeClickStackCopy" and count > 0x400:
                    continue
                out.append(base(r, fn, args, ret=(fn == "SystemAddToClickStack"), w={"gLLCCClickStackCount": count}))
        return out
    return edges


def click_random(fn):
    def rnd(r):
        count = r.choice([0, 1, 2, 3, r.randrange(0x401), 0x3FF, 0x400, 0x401])
        if fn == "SystemMakeClickStackCopy" and count > 0x400:
            count = 0x400
        args = rnd_args(r, fn)
        return base(r, fn, args, ret=(fn == "SystemAddToClickStack"), w={"gLLCCClickStackCount": count})
    return rnd


def set_ride_edges():
    r = random.Random(15)
    return [base(r, "SystemSetRide", [i, t], ret=False) for i in IDS for t in (0, 1, -1, 300)]


def first_block(r, flag, valid, xy, queues):
    x, y = xy
    q = dict(queues)
    q["IsXYValid"] = [valid]
    return base(r, "FixFirstBlock", [flag], ret=False, w={"gLLCCFirstBlockX": x, "gLLCCFirstBlockY": y}, queues=q)


def ffb_random(r):
    pick = r.choice
    xy = pick(SAFE_XY) if r.random() < 0.9 else (pick([-1, 5, 0x7FFFFFFF]), pick([-1, 3, 0x7FFFFFFF]))
    valid = pick([0, 1, 1, 1, 1])
    if not valid:
        pass
    elif xy not in SAFE_XY:
        xy = pick(SAFE_XY)
    n = 14
    q = {"IsTrack": [pick([0, 1, 1, 1, 2]) for _ in range(n)], "IsStart": [pick([0, 1, 1]) for _ in range(n)],
         "IsCorner": [pick([0, 1, 1, 0x7F])], "Neighbours": [pick([0, 1, 4, 5, 16, 64, 0x55, 0xAA, 0xFF, r.randrange(256)]) for _ in range(n)],
         "Cell": [pick([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) for _ in range(n)]}
    return first_block(r, pick(FLAGS), valid, xy, q)


def ffb_edges():
    r = random.Random(16)
    out = []
    for flag in FLAGS:
        for valid in (0, 1):
            for xy in SAFE_XY[:4]:
                for track in (0, 1):
                    for corner in (0, 1):
                        for neigh in (0, 0xFF, 0x55, 1, 4, 16, 64):
                            for cellk in (0, 1, 2):
                                for isstart in (0, 1):
                                    q = {"IsTrack": [track, 1, 1, 0], "IsStart": [isstart], "IsCorner": [corner], "Neighbours": [neigh],
                                         "Cell": [cellk, 3, 0, 4]}
                                    out.append(first_block(r, flag, valid, xy, q))
    r.shuffle(out)
    return out[:260]


def make_tests():
    tests = []

    def add(fn, edge=None, rnd=None):
        tests.append(FnTest("LLCC_" + fn, ppc, host, edge=edge or simple_edges(fn), rnd=rnd or simple(fn)))
    add("SetRideRotation", edge=rotation_edges)
    add("GetRideRotation")
    add("SystemSetMode", edge=mode_edges("SystemSetMode"), rnd=lambda r: base(r, "SystemSetMode", rnd_args(r, "SystemSetMode"), ret=False))
    add("SystemSetModeNR", edge=mode_edges("SystemSetModeNR"), rnd=lambda r: base(r, "SystemSetModeNR", rnd_args(r, "SystemSetModeNR"), ret=False))
    add("SystemGetLastMode")
    add("SystemIsMode")
    add("SystemSetRide", edge=set_ride_edges)
    add("SystemSetCursor", edge=cursor_edges,
        rnd=lambda r: base(r, "SystemSetCursor", [], ret=False, w={"gLLCC480C": r.choice(CURSOR_MODES)}))
    for fn in ("SystemSetRideQueue", "SystemSetRideTrack", "SystemSetSessionCost", "SystemAddToSessionCost"):
        add(fn)
    for fn in ("SystemGetRideQueue", "SystemGetRideTrack", "SystemGetPathCost", "SystemGetQueueCost", "SystemGetKartTrackCost",
               "SystemGetWaterTrackCost", "SystemGetSessionCost"):
        add(fn)
    add("SystemGetClickStackEntries")
    add("SystemAddToClickStack", edge=click_edges("SystemAddToClickStack"), rnd=click_random("SystemAddToClickStack"))
    add("SystemMakeClickStackCopy", edge=click_edges("SystemMakeClickStackCopy"), rnd=click_random("SystemMakeClickStackCopy"))
    add("FixFirstBlock", edge=ffb_edges, rnd=ffb_random)
    return tests


TESTS = make_tests()
