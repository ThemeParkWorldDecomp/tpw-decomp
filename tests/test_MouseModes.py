"""Unit w144, the rest of Mouse.cpp, against libtpw_unit_MouseModes.dylib (tests/unit_MouseModes.cpp), main app ("SimTheme Park"):
  src/game/Mouse.cpp   CMouseMode base bodies (0x1007dfbc to 0x1007e4f8, inline in include/Mouse.h), CMMPathDraw (0x1007e6e4 to 0x1008151c),
                        CMMOnline (0x1007faac to 0x10081228), CMMCoaster (0x1007fd14 to 0x10081174), CMMFirstPersonTarget (0x1007fe70 to 0x100810b8),
                        __sinit_Mouse_cpp 0x100815c8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for
the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside the
unit is a fake on both sides that records its arguments and answers from the case's queues: the cell predicates, the cell system, the cursor,
the shift key, the ride database and the ride record, the object calls, the coaster click, the online control, CTRACE, GAME_ASSERTMSG, the
nothrow operator new, operator delete and a fake current mode. The landed src/game/Mouse.cpp (AssignMouseMode, CMMDefault) runs for real on both
sides, as do the unit's own modes made by the hand over (CMMOnline's constructor included).

The world of a case: gMouse, gMouseMode (none, the fake mode, or the mode under test), the two mouse words, gIntrface's cell id, the game
type, gOnlineControl, the thing array's rows 0 to 4, four objects, the one ride record (upgrade type, track type, queue flag, UI type), twelve
map cells and twelve track cells (parent at +0x10). What is compared: the answer, the event log, the globals and the mode's fields (unless it
was deleted). The Mac's sizes of the new objects (1, 8, 0xe8c) are in the log.

Not tested: a cell id past twelve for the first person target (the Mac's address arithmetic on the world), a null cell (the sum is never 0 for
the ids the pointer can hold), a NULL world.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MouseModes"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address) by kind: 0 CMMPathDraw, 1 CMMOnline, 2 CMMCoaster, 3 CMMFirstPersonTarget, 4 the CMouseMode base
ADDR = {
    4: dict(OnRightRelease=0x1007DFBC, OnRightClick=0x1007DFF4, OnLeftRelease=0x1007E028, OnLeftClick=0x1007E060, WhileRightDown=0x1007E1A0,
            WhileLeftDown=0x1007E1D8, OnMouseMove=0x1007E210, OnDoubleClick=0x1007E2A8, OnAssign=0x1007E454, dtor=0x1007E484,
            OnRelease=0x1007E4F8),
    0: dict(ctor1=0x1007E6E4, ctor2=0x1007E744, OnLeftClick=0x1007E7E0, OnLeftRelease=0x1007E890, OnRightClick=0x1007E944,
            WhileLeftDown=0x1007E97C, WhileRightDown=0x1007E9B4, OnMouseMove=0x1007E9EC, OnAssign=0x10080EE4, dtor=0x1008149C,
            GetMode=0x1008151C),
    1: dict(ctor=0x1007FAAC, OnLeftClick=0x1007FB2C, OnRightClick=0x1007FBD8, OnDoubleClick=0x1007FC0C, OnAssign=0x1007FCB8, dtor=0x100811A8,
            GetMode=0x10081228),
    2: dict(ctor=0x1007FD14, OnLeftClick=0x1007FD58, OnLeftRelease=0x1007FDB0, OnRightClick=0x1007FE0C, OnAssign=0x1007FE40, dtor=0x100810F4,
            GetMode=0x10081174),
    3: dict(ctor=0x1007FE70, OnMouseMove=0x1007FEC0, OnLeftClick=0x1008009C, OnRightClick=0x10080E40, OnAssign=0x10080E80, dtor=0x1008102C,
            GetMode=0x100810B8),
}
SINIT = 0x100815C8
OWN = {a for table in ADDR.values() for a in table.values()} | {SINIT}
# the landed originals of src/game/Mouse.cpp that run for real: AssignMouseMode, the CMMDefault constructor, destructor, GetMode, OnLeftClick, OnRightClick, OnAssign
LANDED = {0x1007E2E0, 0x1007EA78, 0x100813E8, 0x10081468, 0x1007EAC4, 0x1007EAF8, 0x1007EB2C}
OPID = dict(OnLeftClick=0, OnLeftRelease=1, OnRightClick=2, OnRightRelease=3, OnDoubleClick=4, OnMouseMove=5, WhileLeftDown=6,
            WhileRightDown=7, GetMode=8, OnAssign=9, OnRelease=10)
VTABLE = {0: 0x1029D640, 1: 0x1029D4EC, 2: 0x1029D4A0, 3: 0x1029D454}
OBJ_SIZE = {0: 0x14, 1: 8, 2: 8, 3: 8, 4: 8}

# ---- the callees faked on both sides
ASSERT, CTRACE, NEW = 0x1000DDE8, 0x1006FCBC, 0x102384B8
IS_FLAG, IS_WALKABLE, EQUIV, IS_TRACKX, IS_HOARD = 0x1009BB70, 0x1009C6AC, 0x100F48C4, 0x100A367C, 0x100A35DC
SET_CURSOR, CHECK_CONTEXT, SHIFT = 0x101EFA54, 0x101EFB0C, 0x101F0068
MOUSE_MOVED, LEFT_DOWN, LEFT_UP, SET_MODE = 0x10083180, 0x10084A00, 0x10084CA4, 0x10090E18
SET_RIDE, SET_QUEUE, SET_TRACK, OVERRIDE = 0x100915BC, 0x10091994, 0x10091A5C, 0x10089210
GET_RIDE_INFO, GET_DB_INFO, GO_FIRST, TOGGLE = 0x10154868, 0x10107CD4, 0x1010ED8C, 0x10033B90
COASTER, ONLINE_CTOR, SET_DEST, TELEPORT = 0x10057F80, 0x10213870, 0x10218140, 0x1021802C

# ---- the Mac's addresses of the globals (the asm's TOC comments)
G = dict(mouse=0x102DC1F4, mode=0x102DC1F0, w1e0=0x102DC1E0, w1e4=0x102DC1E4, intrface=0x102DC1AC, world=0x1037EBFC, things=0x1034CBFC,
         gtype=0x102B3AA0, guard=0x102B3AA4, online=0x10396D28, w1f8=0x102DC1F8, w1fc=0x102DC1FC)
HOST_LAYOUT = [0x44, 0x28, 0x2D8, 0x1102D8, 0x10]

# labels: 1 the world, 8 the mouse, 9 the fake mode, 10 the mode under test, 20 to 23 the objects (rows 1 to 4), 60 the ride record, 70 the
# online control; map cell k is 300 + k, track cell k is 400 + k
CELLS = 12
THING_LABELS = [20, 21, 22, 23]


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


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


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def assign(name):
        return lambda e: e.cl.rec(name, (e.cl.lab(e.r(3)), e.cl.lab(e.u32(G["mode"]))))
    vt = T.persist_alloc(e, 0x40)
    for slot, fn in {8: destructor("Mode.dtor"), 0x2C: R("Mode.GetMode", "L", ret=lambda e, L: L.ret("ModeGetMode", 0) & MASK),
                     0x30: assign("Mode.OnAssign"), 0x34: assign("Mode.OnRelease")}.items():
        ws(e, vt + slot, [T.fake_function(e, fn)])
    e.mm_mode_vt = vt
    e.mm_map = T.persist_alloc(e, 0x110000 + 0x800)
    e.mm = None


def u16(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)
    S.real.update(LANDED)

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

    def override(e):
        e.cl.rec("LLCR_OverRide", (s32(e.r(4)), s32(e.r(5))))
        e.write(e.r(3), struct.pack(">H", e.cl.ret("Override", 0) & 0xFFFF))

    def equiv(e):
        e.cl.rec("GetTrackCellEquiv", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.mm.a(400 + e.cl.ret("Equiv", 1)))

    def set_ride(e):     # r3 is the address of the halfword id inside the mode (mode + 0x10)
        e.cl.rec("LLCC_SystemSetRide", (u16(e, e.r(3)), s32(e.r(4))))

    def get_ride_info(e):
        e.cl.rec("GetRideInfo", ((e.r(4) >> 16) & 0xFFFF,))
        e.set_r(3, e.mm.a(60))

    def get_db_info(e):
        e.cl.rec("GetDBInfo", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.mm.a(60))

    S.imp("__nw__FUl", new)
    S.internal(NEW, new_glue)
    S.imp("__dl__FPv", lambda e: delete(e, e.r(3)))
    S.internal(CTRACE, ctrace)
    S.internal(ASSERT, assert_)
    S.internal(SET_CURSOR, R("SetGameWindowCursor", "xh"))
    S.internal(CHECK_CONTEXT, R("CheckMouseContext", "xi"))
    S.internal(SHIFT, R("GetShiftKeyState", "x", ret=lambda e, L: L.ret("Shift", 0) & MASK))
    S.internal(IS_FLAG, returns("IsFlag", "LH", "IsFlag"))
    S.internal(IS_WALKABLE, returns("IsWalkable", "L", "IsWalkable"))
    S.internal(EQUIV, equiv)
    S.internal(IS_TRACKX, returns("IsTrackX", "L", "IsTrackX"))
    S.internal(IS_HOARD, returns("IsHoarding", "L", "IsHoarding"))
    S.internal(MOUSE_MOVED, R("LLCM_MouseMoved", "iiH"))
    S.internal(LEFT_DOWN, R("LLCM_LeftMouseButtonDown", "iiHi"))
    S.internal(LEFT_UP, R("LLCM_LeftMouseButtonUp", "iiHi"))
    S.internal(SET_MODE, R("LLCC_SystemSetMode", "ib"))
    S.internal(SET_RIDE, set_ride)
    S.internal(SET_QUEUE, R("LLCC_SystemSetRideQueue", "i"))
    S.internal(SET_TRACK, R("LLCC_SystemSetRideTrack", "i"))
    S.internal(OVERRIDE, override)
    S.internal(GET_RIDE_INFO, get_ride_info)
    S.internal(GET_DB_INFO, get_db_info)
    S.internal(GO_FIRST, R("GoFirstPerson", "Li"))
    S.internal(TOGGLE, R("ToggleFirstPersonWalkAround"))
    S.internal(COASTER, R("CoasterAcceptLeftMouseClick", "i"))
    S.internal(ONLINE_CTOR, R("Online.ctor", "L"))
    S.internal(SET_DEST, R("SetAvatarDestination", "Lw"))
    S.internal(TELEPORT, R("TeleportAvatar", "Lw"))
    return S


STUBS = build_stubs()

# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws_):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws_))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.addr = {}
        self.map = e.mm_map
        e.mm = self

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def make(self, label, size):
        e = self.e
        if label == 9:
            a = e.alloc(8, struct.pack(">I", e.mm_mode_vt) + b"\xa5" * 4)
        elif label == 8:
            a = e.alloc(16, b"\xa5" * 16)
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
        return self.map - 0x2D8

    def map_address(self):
        return self.map

    def set_world(self, present):
        self.words(G["world"], [self.world() if present else 0])

    def setg(self, key, value):
        self.words(G[key], [value])

    def set_f24(self, v):
        self.half(G["intrface"] + 0x24, v)

    def set_thing(self, i, a):
        self.words(G["things"] + 0x14 * i, [a])

    def clear_things(self):
        self.write(G["things"], b"\0" * (0x14 * 16))

    def set_gtype(self, v):
        self.words(G["gtype"], [v])
        self.byte(G["guard"], 1)

    def ride(self, a, upgrade, track, queue, uitype):
        self.words(a + 0x288, [upgrade])
        self.words(a + 0x9C, [track])
        self.words(a + 0x40, [queue])
        self.words(a + 0x7A8, [uitype])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def mode(self, kind, v):
        """The mode under test (label 10): v = word at +4, then for the PathDraw the cell mode, the flag word and the object id."""
        a = self.make(10, OBJ_SIZE[kind])
        self.words(a, [VTABLE.get(kind, 0), v[0]])
        if kind == 0:
            self.words(a + 8, [v[1], v[2]])
            self.half(a + 0x10, v[3])
        return a

    def fields(self, kind, a):
        w = [s32(self.e.u32(a + 4))]
        if kind == 0:
            w += [s32(self.e.u32(a + 8)), s32(self.e.u32(a + 0xC)), u16(self.e, a + 0x10)]
        return w

    def dump(self):
        e, L = self.e, self.L
        u32 = e.u32
        return [L.lab(u32(G["mouse"])), L.lab(u32(G["mode"])), s32(u32(G["w1e0"])), s32(u32(G["w1e4"])), L.lab(u32(G["online"]))]

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]

    def ctor(self, kind, address, args):
        e = self.e
        a = e.alloc(256, b"\xa5" * 256)
        self.call(address, a, *args)
        if kind != 4:
            assert e.u32(a) == VTABLE[kind], hex(e.u32(a))
        return self.fields(kind, a)


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_mm_init()
        CL.host_begin(lib, {}, case["queues"])
        self.addr = {}

    def done(self):
        pass

    def make(self, label, size):
        a = self.lib.tpw_mm_fake_mode() if label == 9 else self.lib.tpw_mm_obj(label, size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.lib.tpw_cl_label(label, C.c_void_p(a))
        self.addr[label] = a

    def a(self, label):
        return self.addr.get(label, 0)

    def world(self):
        return self.lib.tpw_mm_addr(b"world")

    def map_address(self):
        return self.lib.tpw_mm_addr(b"map")

    def set_world(self, present):
        self.lib.tpw_mm_set(16, 1 if present else 0, 0)

    def setg(self, key, value):
        self.lib.tpw_mm_set({"mouse": 1, "mode": 2, "w1e0": 3, "w1e4": 4, "online": 17}[key], value, 0)

    def set_f24(self, v):
        self.lib.tpw_mm_set(6, v, 0)

    def set_thing(self, i, a):
        self.lib.tpw_mm_set(7, i, a)

    def clear_things(self):
        self.lib.tpw_mm_set(8, 0, 0)

    def set_gtype(self, v):
        self.lib.tpw_mm_set(11, v, 0)

    def ride(self, a, upgrade, track, queue, uitype):
        self.lib.tpw_mm_ride(s32(upgrade), s32(track), s32(queue), s32(uitype))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def mode(self, kind, v):
        a = self.lib.tpw_mm_new(kind)
        self.name(10, a)
        for field, value in zip((0, 1, 2, 3), v):
            self.lib.tpw_mm_set_field(a, kind, field, value)
        return a

    def fields(self, kind, a):
        return [self.lib.tpw_mm_get_field(a, kind, field) for field in ((0, 1, 2, 3) if kind == 0 else (0,))]

    def dump(self):
        out = (C.c_int64 * 16)()
        self.lib.tpw_mm_dump(out)
        return list(out)[:5]

    def ctor(self, kind, variant, args):
        a = self.lib.tpw_mm_ctor(kind, variant, *([s32(x) for x in args] + [0, 0])[:2])
        return self.fields(kind, a)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, i64 = C.c_uint64, C.c_int32, C.c_int64
    lib.tpw_mm_addr.restype, lib.tpw_mm_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_mm_init.argtypes = []
    lib.tpw_mm_obj.restype, lib.tpw_mm_obj.argtypes = u64, [i32, C.c_uint32]
    lib.tpw_mm_ride.argtypes = [i32] * 4
    lib.tpw_mm_fake_mode.restype, lib.tpw_mm_fake_mode.argtypes = u64, []
    lib.tpw_mm_new.restype, lib.tpw_mm_new.argtypes = u64, [i32]
    lib.tpw_mm_ctor.restype, lib.tpw_mm_ctor.argtypes = u64, [i32] * 4
    lib.tpw_mm_set_field.argtypes = [u64, i32, i32, i64]
    lib.tpw_mm_get_field.restype, lib.tpw_mm_get_field.argtypes = i64, [u64, i32, i32]
    lib.tpw_mm_set.argtypes = [i32, i64, i64]
    lib.tpw_mm_dump.argtypes = [C.POINTER(i64)]
    lib.tpw_mm_sinit.argtypes = [C.POINTER(i64)]
    lib.tpw_mm_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_mm_call.restype, lib.tpw_mm_call.argtypes = i32, [u64] + [i32] * 6
    lib.tpw_mm_dtor.argtypes = [u64, i32, i32]
    lib.tpw_cl_label.argtypes = [i32, C.c_void_p]
    lib.tpw_mm_init()
    out = (i64 * 32)()
    n = lib.tpw_mm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    st = case["st"]
    s.clear_things()
    mouse = s.make(8, 16)
    fake = s.make(9, 8)
    ride = s.make(60, 0x800)
    online = s.make(70, 0xE8C)
    s.ride(ride, *st["ride"])
    s.name(1, s.world())
    things = {}
    for i, label in enumerate(THING_LABELS):
        things[label] = s.make(label, 0x40)
        s.set_thing(i + 1, things[label])
    if st["row0"]:
        s.set_thing(0, things[st["row0"]])
    obj = 0
    if case.get("kind") is not None and case["op"] not in ("ctor1", "ctor2", "ctor"):
        obj = s.mode(case["kind"], case["obj"])
    m = s.map_address()
    s.write(m, b"\0" * (0x44 * 16))
    s.write(m + 0x110000, b"\0" * (0x28 * 16))
    for k in range(1, CELLS + 1):
        base_cell, base_track = m + (k - 1) * 0x44, m + 0x110000 + (k - 1) * 0x28
        s.name(300 + k, base_cell)
        s.name(400 + k, base_track)
        s.half(base_track + 0x10, case["tracks"][k - 1])
    s.set_world(True)
    s.setg("mouse", mouse if st["mouse"] else 0)
    s.setg("mode", {"none": 0, "fake": fake, "self": obj}[st["mode"]])
    s.setg("online", online if st["online"] else 0)
    for key in ("w1e0", "w1e4"):
        s.setg(key, st[key])
    s.set_f24(st["f24"])
    s.set_gtype(st["gtype"])
    return obj


def execute(s, case, obj):
    op, kind, args = case["op"], case["kind"], case["args"]
    ppc = isinstance(s, Ppc)
    address = ADDR[kind].get(op) if op != "sinit" else SINIT
    if op in ("ctor1", "ctor2", "ctor"):
        if ppc:
            return s.ctor(kind, address, [] if op == "ctor" else [args[0]] if op == "ctor1" else [args[0], args[1] << 16])
        return s.ctor(kind, 1 if op == "ctor2" else 0, args)
    if op == "dtor":
        if ppc:
            s.call(address, obj, *args)
        else:
            s.lib.tpw_mm_dtor(obj, kind, T.s16(args[0]))
        return None
    if ppc:
        r = s.call(address, obj, *args)
    else:
        r = s.lib.tpw_mm_call(obj, kind, OPID[op], *([s32(x) for x in args] + [0, 0, 0, 0])[:4])
    return (r & MASK) if op == "GetMode" else None


def observe(s, case, obj, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    log = s.log()
    gone = any(name == "delete" and ints and ints[0] == 10 for name, ints, _ in log)
    out = {"answer": answer, "log": log, "state": s.dump()}
    if obj and not gone and case["op"] != "dtor":
        out["fields"] = s.fields(case["kind"], obj)
    return out


def run(s, case):
    try:
        obj = build(s, case)
        answer = execute(s, case, obj)
        if case["op"] in ("ctor1", "ctor2", "ctor"):
            return {"answer": None, "log": s.log(), "state": s.dump(), "fields": answer}
        return observe(s, case, obj, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

CELL_MODES = [4, 0x3B, 4, 0x3B, 0, 1, 2, 3, 5, 0x10, 0x3A, 0x3C, -1, 100]
SHIFTS = [0, 1, 0x100, 0x1FF, 0x12345, 0xFFFF, 0xFFFF0000, 0x80000001, 0xABCD]
IDS16 = [0, 1, 2, 5, 12, 0x7F, 0x80, 0x81, 0xFFFF, 0x1234, 0x4000, 0x8000]


def base(r, op, kind, args=(), fmax=None, **over):
    """The world every case starts from: random globals and answer queues that cover every branch."""
    ids = [i for i in IDS16 if fmax is None or i <= fmax] if fmax is None else list(range(0, fmax + 1))
    st = dict(mouse=r.choice([0, 1, 1]), mode=r.choice(["none", "fake", "self", "self"]), w1e0=r.choice([0, 5, -1, 7]),
              w1e4=r.choice([0, 5, -1, 7]), f24=r.choice(ids), gtype=r.choice([0, 0, 1, 2]), online=r.choice([0, 1]),
              row0=r.choice([0, 0, 0, 20]),
              ride=(r.choice([0, 1, 1, 2, -1, 5]), r.choice([0, 1, 2, 3, -1, 7]), r.choice([0, 1, 2, -1, 256]), r.choice([0, 0, 1, 2, -1])))
    st.update({k: v for k, v in over.items() if k in st})
    tracks = [r.randrange(1, CELLS + 1) for _ in range(CELLS)]
    flag = lambda: r.choice([0, 0, 1, 1, 2, 0x100])
    queues = {"New": [r.choice([1, 1, 1, 1, 0]) for _ in range(4)], "ModeGetMode": [r.choice([0, 1, 2, 8, 9, -1])],
              "Override": [r.choice([0, 1, 2, 3, 4])], "Shift": [r.choice(SHIFTS)], "IsFlag": [flag(), flag()],
              "IsWalkable": [flag()], "IsTrackX": [flag()], "IsHoarding": [flag()], "Equiv": [r.randrange(1, CELLS + 1)]}
    case = {"op": op, "kind": kind, "args": list(args), "st": st, "tracks": tracks, "queues": queues}
    obj = [r.choice([0, 1, 5, -1])]
    if kind == 0:
        obj += [r.choice(CELL_MODES), r.choice([0, 1, 1, 0, 2, -1]), r.choice(IDS16)]
    case["obj"] = obj
    return case


def gen_random(op, kind, nargs=2, fmax=None, ids=False):
    def rnd(r):
        args = [r.choice([0, 1, 5, -1, 640, 0x7FFFFFFF, r.randrange(-1000, 1000)]) for _ in range(nargs)]
        return base(r, op, kind, args, fmax)
    return rnd


def gen_edge(op, kind, nargs=2, seed=0, fmax=None):
    def edge():
        r = random.Random(seed)
        for _ in range(12):
            yield gen_random(op, kind, nargs, fmax)(r)
    return edge


def dtor_edges(kind, seed):
    def gen():
        r = random.Random(seed)
        for flag in (-1, 0, 1, 2, 3, 0x7FFFFFFF, 0x10001, 0x8000, 0x7FFF):
            for mode in ("none", "self", "fake"):
                yield base(r, "dtor", kind, [flag], mode=mode)
    return gen


def dtor_random(kind):
    return lambda r: base(r, "dtor", kind, [r.choice([-1, 0, 1, 2, 3, 0x10001, 0x8000])])


def ctor_cases(kind, op, seed):
    def mk(r):
        c = base(r, op, kind, [r.choice(CELL_MODES), r.choice(IDS16)])
        return c

    def edge():
        r = random.Random(seed)
        for _ in range(10):
            yield mk(r)
    return edge, mk


def from_edges(edge):
    cache = []

    def rnd(r):
        if not cache:
            cache.extend(edge())
        return r.choice(cache)
    return rnd


def pd_assign_edges():
    r = random.Random(14401)
    for mode in CELL_MODES[:8] + [0x3A, 0x3C]:
        for f0c in (0, 1):
            for upgrade in (1, 0):
                for queue in (0, 1):
                    c = base(r, "OnAssign", 0)
                    c["obj"] = [r.choice([0, 1]), mode, f0c, r.choice(IDS16)]
                    c["st"]["ride"] = (upgrade, r.choice([0, 3, -1]), queue, 0)
                    yield c


def pd_assign_random(r):
    c = base(r, "OnAssign", 0)
    c["st"]["mode"] = r.choice(["none", "fake", "self"])
    return c


def pd_click_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for f24 in IDS16:
            for shift in SHIFTS:
                c = base(r, op, 0, [r.randrange(-50, 50), r.randrange(-50, 50)])
                c["st"]["f24"] = f24
                c["queues"]["Shift"] = [shift]
                yield c
    return gen


def pd_move_edges():
    r = random.Random(14402)
    for f24 in IDS16:
        c = base(r, "OnMouseMove", 0, [1, 2, 3, 4])
        c["st"]["f24"] = f24
        yield c


def online_click_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for online in (0, 1):
            for new in (0, 1):
                for f24 in (0, 1, 7, 0xFFFF, 0x1234):
                    c = base(r, op, 1, [3, 4])
                    c["st"].update(online=online, f24=f24)
                    c["queues"]["New"] = [new]
                    yield c
    return gen


def fp_move_edges():
    r = random.Random(14403)
    for f24 in (0, 3):
        for f1 in (0, 1, 2):
            for f2 in (0, 1):
                for walk in (0, 1):
                    for tx in (0, 1):
                        for hoard in (0, 1):
                            c = base(r, "OnMouseMove", 3, [r.randrange(0, 100)] * 4, fmax=12)
                            c["st"]["f24"] = f24
                            c["queues"].update(IsFlag=[f1, f2 if f1 else 1], IsWalkable=[walk], IsTrackX=[tx], IsHoarding=[hoard])
                            yield c


def fp_move_random(r):
    return base(r, "OnMouseMove", 3, [r.randrange(0, 100)] * 4, fmax=12)


def fp_click_edges():
    r = random.Random(14404)
    for f24 in (0, 3):
        for override in (0, 2):
            for ui in (0, 1):
                for f1 in (0, 1):
                    for f2 in (0, 1):
                        for walk in (0, 1):
                            for tx in (0, 1):
                                for hoard in (0, 1):
                                    c = base(r, "OnLeftClick", 3, [r.randrange(0, 100)] * 2, fmax=12)
                                    c["st"]["f24"] = f24
                                    c["st"]["ride"] = (0, 0, 0, ui)
                                    c["queues"].update(Override=[override], IsFlag=[f1, f2], IsWalkable=[walk], IsTrackX=[tx], IsHoarding=[hoard])
                                    yield c


def fp_click_random(r):
    c = base(r, "OnLeftClick", 3, [r.randrange(0, 100)] * 2, fmax=12)
    c["st"]["ride"] = (0, 0, 0, r.choice([0, 0, 1, 2, -1]))
    return c


def fp_click_handover_edges():
    """The hand over in all its shapes: the current mode, the mouse, the new's answers, the game type, the row 0 thing."""
    r = random.Random(14405)
    for mode in ("self", "fake", "none"):
        for mouse in (0, 1):
            for gtype in (0, 1, 2):
                for news in ([1, 1, 1, 1], [0, 1, 1, 1], [1, 0, 1, 1], [1, 1, 0, 1], [1, 1, 1, 0], [0, 0, 0, 0]):
                    for path in (0, 1):
                        c = base(r, "OnLeftClick", 3, [1, 2], fmax=12)
                        c["st"].update(mode=mode, mouse=mouse, gtype=gtype, f24=r.randrange(1, 13), row0=r.choice([0, 20]))
                        c["st"]["ride"] = (0, 0, 0, 0 if path else 1)
                        c["queues"].update(New=news, Override=[r.randrange(1, 5) if path else 0],
                                           IsFlag=[0, 0], IsTrackX=[0], IsHoarding=[0])
                        yield c


def fp_click_edges_all():
    yield from fp_click_edges()
    yield from fp_click_handover_edges()


def sinit_ppc(e, c):
    e.write(G["w1f8"], struct.pack(">II", *c["words"]))
    T.call_ppc(e, SINIT, [])
    return dict(words=list(struct.unpack(">II", e.read(G["w1f8"], 8))))


def sinit_host(lib, c):
    bind(lib)
    out = (C.c_int64 * 2)()
    lib.tpw_mm_sinit(out)
    return dict(words=[v & MASK for v in out])


def sinit_edges():
    for words in ([0, 0], [0xFFFFFFFF, 0xFFFFFFFF], [0x10000, 0], [1, 2]):
        yield dict(words=words)


def sinit_case(r):
    return dict(words=[r.randrange(1 << 32), r.randrange(1 << 32)])


def make_tests():
    tests = []

    def add(name, kind, op, nargs=2, seed=0, fmax=None, edge=None, rnd=None):
        tests.append(FnTest(name, ppc, host, edge=edge or gen_edge(op, kind, nargs, seed, fmax), rnd=rnd or gen_random(op, kind, nargs, fmax)))

    seed = 14410
    for op, nargs in (("OnRightRelease", 2), ("OnRightClick", 2), ("OnLeftRelease", 2), ("OnLeftClick", 2), ("WhileRightDown", 2),
                      ("WhileLeftDown", 2), ("OnMouseMove", 4), ("OnDoubleClick", 2), ("OnAssign", 0), ("OnRelease", 0)):
        seed += 1
        add("CMouseMode::" + op, 4, op, nargs, seed)
    tests.append(FnTest("CMouseMode::~CMouseMode", ppc, host, edge=dtor_edges(4, 14430), rnd=dtor_random(4)))

    # CMMPathDraw
    edge1, rnd1 = ctor_cases(0, "ctor1", 14431)
    edge2, rnd2 = ctor_cases(0, "ctor2", 14432)
    tests.append(FnTest("CMMPathDraw::CMMPathDraw", ppc, host, edge=edge1, rnd=rnd1))
    tests.append(FnTest("CMMPathDraw::CMMPathDraw", ppc, host, edge=edge2, rnd=rnd2))
    tests.append(FnTest("CMMPathDraw::OnLeftClick", ppc, host, edge=pd_click_edges("OnLeftClick", 14433), rnd=gen_random("OnLeftClick", 0)))
    tests.append(FnTest("CMMPathDraw::OnLeftRelease", ppc, host, edge=pd_click_edges("OnLeftRelease", 14434), rnd=gen_random("OnLeftRelease", 0)))
    add("CMMPathDraw::OnRightClick", 0, "OnRightClick", 2, 14435)
    add("CMMPathDraw::WhileLeftDown", 0, "WhileLeftDown", 2, 14436)
    add("CMMPathDraw::WhileRightDown", 0, "WhileRightDown", 2, 14437)
    tests.append(FnTest("CMMPathDraw::OnMouseMove", ppc, host, edge=pd_move_edges, rnd=gen_random("OnMouseMove", 0, 4)))
    tests.append(FnTest("CMMPathDraw::OnAssign", ppc, host, edge=pd_assign_edges, rnd=pd_assign_random))
    tests.append(FnTest("CMMPathDraw::~CMMPathDraw", ppc, host, edge=dtor_edges(0, 14438), rnd=dtor_random(0)))
    add("CMMPathDraw::GetMode", 0, "GetMode", 0, 14439)

    # CMMOnline
    edge, rnd = ctor_cases(1, "ctor", 14440)
    tests.append(FnTest("CMMOnline::CMMOnline", ppc, host, edge=edge, rnd=rnd))
    tests.append(FnTest("CMMOnline::OnLeftClick", ppc, host, edge=online_click_edges("OnLeftClick", 14441), rnd=gen_random("OnLeftClick", 1)))
    add("CMMOnline::OnRightClick", 1, "OnRightClick", 2, 14442)
    tests.append(FnTest("CMMOnline::OnDoubleClick", ppc, host, edge=online_click_edges("OnDoubleClick", 14443), rnd=gen_random("OnDoubleClick", 1)))
    add("CMMOnline::OnAssign", 1, "OnAssign", 0, 14444)
    tests.append(FnTest("CMMOnline::~CMMOnline", ppc, host, edge=dtor_edges(1, 14445), rnd=dtor_random(1)))
    add("CMMOnline::GetMode", 1, "GetMode", 0, 14446)

    # CMMCoaster
    edge, rnd = ctor_cases(2, "ctor", 14447)
    tests.append(FnTest("CMMCoaster::CMMCoaster", ppc, host, edge=edge, rnd=rnd))
    add("CMMCoaster::OnLeftClick", 2, "OnLeftClick", 2, 14448)
    add("CMMCoaster::OnLeftRelease", 2, "OnLeftRelease", 2, 14449)
    add("CMMCoaster::OnRightClick", 2, "OnRightClick", 2, 14450)
    add("CMMCoaster::OnAssign", 2, "OnAssign", 0, 14451)
    tests.append(FnTest("CMMCoaster::~CMMCoaster", ppc, host, edge=dtor_edges(2, 14452), rnd=dtor_random(2)))
    add("CMMCoaster::GetMode", 2, "GetMode", 0, 14453)

    # CMMFirstPersonTarget
    edge, rnd = ctor_cases(3, "ctor", 14454)
    tests.append(FnTest("CMMFirstPersonTarget::CMMFirstPersonTarget", ppc, host, edge=edge, rnd=rnd))
    tests.append(FnTest("CMMFirstPersonTarget::OnMouseMove", ppc, host, edge=fp_move_edges, rnd=fp_move_random))
    tests.append(FnTest("CMMFirstPersonTarget::OnLeftClick", ppc, host, edge=fp_click_edges_all, rnd=fp_click_random))
    add("CMMFirstPersonTarget::OnRightClick", 3, "OnRightClick", 2, 14455)
    add("CMMFirstPersonTarget::OnAssign", 3, "OnAssign", 0, 14456)
    tests.append(FnTest("CMMFirstPersonTarget::~CMMFirstPersonTarget", ppc, host, edge=dtor_edges(3, 14457), rnd=dtor_random(3)))
    add("CMMFirstPersonTarget::GetMode", 3, "GetMode", 0, 14458)

    tests.append(FnTest("__sinit_Mouse_cpp", sinit_ppc, sinit_host, edge=sinit_edges, rnd=sinit_case))
    return tests


TESTS = make_tests()
