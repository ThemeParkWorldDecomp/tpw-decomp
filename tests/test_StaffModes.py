"""Unit w143, the park click and the staff mouse modes, against libtpw_unit_StaffModes.dylib (tests/unit_StaffModes.cpp), main app
("SimTheme Park"):
  src/game/ui/ui_main.cpp   ContextMouseClick 0x10183320
  src/game/Mouse.cpp     CMMPlaceStaff (constructor 0x1007eb94, destructor 0x10081320, GetMode 0x100813b0, OnAssign 0x1007ec9c, OnRelease 0x1007eeb8,
                          OnLeftClick 0x1007ef80), CMMPickUpStaff (constructor 0x1007f46c, destructor 0x10081258, GetMode 0x100812e8, OnAssign
                          0x1007f4d8, OnLeftClick 0x1007f58c, OnRightClick 0x1007fa74, OnRelease 0x1007f948), CSpritePlaceMode (OnMouseMove
                          0x1007e528, destructor 0x1007ec14, OnRelease 0x10081550)
  src/game/TPPointer.cpp   GetPickupWorldPosn 0x10067220

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments and answers from the case's queues: the cell predicates, the five staff
constructors, DropStaff, the staff headquarters, the advisor, the bank and the object control getters, the script calls, the square face
drawing, the cursor, CTRACE, GAME_ASSERTMSG, the sound event, the panels, the keyboard, the nothrow operator new, operator delete, CMMOnline
and the CMMPathDraw (constructors and virtuals; unit w144 translates it) and a fake current mode. The landed src/game/Mouse.cpp (AssignMouseMode,
CMMDefault) runs for real on both sides.

The world of a case: gMouse, gMouseMode (none, the fake mode, or the staff mode under test), gKeyboard, the two mouse words, gGameWinWordC38,
gIntrface's cell id, the game type, the sound bank word, the thing array's rows 0 to 4, four things (cell x and y at +4 and +6, the object type
at +0xe, the rotation at +0x10), the staff HQ's halfword at +0x60, twelve map cells and twelve track cells (type at +8, parent at +0x10), the
pickup point and the staff mode's fields. What is compared: the answer, the event log (floats as bits, NaN made one), the globals and the staff
mode's fields (unless it was deleted). The Mac's size of the new staff object (0x214 or 0x218 by model) is checked on the PowerPC side.

No fused operations. Not tested: a cell id of 0 or past twelve (the Mac's 32 bit address arithmetic), a command key press whose object is a
thing array row without a thing (the original reads it), a NULL world for the staff modes (they read it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_StaffModes"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
CONTEXT = 0x10183320
P_LEFT, P_ASSIGN, P_RELEASE, P_DTOR, P_CTOR, P_GETMODE = 0x1007EF80, 0x1007EC9C, 0x1007EEB8, 0x10081320, 0x1007EB94, 0x100813B0
K_LEFT, K_RELEASE, K_ASSIGN, K_DTOR, K_CTOR, K_GETMODE, K_RIGHT = 0x1007F58C, 0x1007F948, 0x1007F4D8, 0x10081258, 0x1007F46C, 0x100812E8, 0x1007FA74
B_MOVE, B_DTOR, B_RELEASE = 0x1007E528, 0x1007EC14, 0x10081550
PICKUP = 0x10067220
SIZES = {CONTEXT: 2364, P_LEFT: 1208, P_ASSIGN: 492, P_RELEASE: 152, P_DTOR: 100, P_CTOR: 60, P_GETMODE: 8,
         K_LEFT: 904, K_RELEASE: 252, K_ASSIGN: 132, K_DTOR: 140, K_CTOR: 48, K_GETMODE: 8, K_RIGHT: 4,
         B_MOVE: 388, B_DTOR: 88, B_RELEASE: 68, PICKUP: 8}
OWN = set(SIZES)
# the landed originals of src/game/Mouse.cpp that run for real: AssignMouseMode, the CMMDefault constructor, destructor, GetMode, OnLeftClick, OnRightClick
LANDED = {0x1007E2E0, 0x1007EA78, 0x100813E8, 0x10081468, 0x1007EAC4, 0x1007EAF8, 0x1007EB2C}

# ---- the callees faked on both sides
ASSERT, CTRACE, NEW = 0x1000DDE8, 0x1006FCBC, 0x102384B8
IS_FLAG, IS_HOARD, IS_TRACK, IS_GRASS, IS_PATH, IS_QUEUE = 0x1009BB70, 0x1009CA9C, 0x1009C96C, 0x1009C660, 0x1009C44C, 0x1009C490
SET_CURSOR, CHECK_CONTEXT = 0x101EFA54, 0x101EFB0C
SPAWN, KILL, SET_POS, ADD_FACE = 0x100DA18C, 0x100D9478, 0x100D932C, 0x100E02A0
FATE, GET_HQ, DROP, GET_ADVISOR, CANCEL = 0x10129D50, 0x1013E99C, 0x1012C168, 0x1013EAB0, 0x1000B048
ONLINE_CTOR, PD_CTOR, PD_CTOR2 = 0x1007FAAC, 0x1007E6E4, 0x1007E744
HANDYMAN, ENTERTAINER, GUARD, MECHANIC, RESEARCHER = 0x100FF0A4, 0x100FB224, 0x100FDB08, 0x10104170, 0x10120EEC
MAIN_MENU_UP, CHECK_SELECTED, GET_PEEP, SET_SELECTED, GET_SELECTED = 0x101FC1EC, 0x101947E4, 0x101946F8, 0x10194768, 0x10194730
GOTO_STATUS, VISITOR_SHOW, STAFF_SHOW, ENTRY_PRICE, COASTER_EDIT = 0x10181E2C, 0x101433F8, 0x101C54F0, 0x1019D660, 0x1019C584
SET_CURRENT, OVERRIDE, SET_ROTATION, SOUND = 0x10088590, 0x10089210, 0x10090D40, 0x100DD9FC
GET_CONTROL, GET_BALANCE, GET_BANK, THING_ID = 0x100FA0B8, 0x100F32B4, 0x1013EDFC, 0x10103A04
KEYBOARD_CTOR = "__ct__Q213SamsUtilities9UKeyboardFv"
KEYBOARD_VARIABLE = "gKeyboard__13SamsUtilities"
STAFF_CTORS = {HANDYMAN: "CHandyman.ctor", ENTERTAINER: "CEntertainer.ctor", GUARD: "CGuard.ctor", MECHANIC: "CMechanic.ctor",
               RESEARCHER: "CResearcher.ctor"}

# ---- the Mac's addresses of the globals (the asm's TOC comments)
G = dict(mouse=0x102DC1F4, mode=0x102DC1F0, w1e0=0x102DC1E0, w1e4=0x102DC1E4, c38=0x10380C38, intrface=0x102DC1AC, world=0x1037EBFC,
         things=0x1034CBFC, pickup=0x102C6CEC, bank3=0x102F7CA0, gtype=0x102B3AA0, guard=0x102B3AA4)
PLACE_VT, PICK_VT = 0x1029D5A8, 0x1029D554
# the host's offsets and sizes the module relies on (checked once against tpw_sm_layout)
HOST_LAYOUT = [0x44, 0x28, 0x2D8, 0x1102D8, 8, 0x10, 0x60, 8]

# labels: 1 the world, 8 the mouse, 9 the fake mode, 10 the staff mode under test, 12 the preset keyboard, 20 to 23 the things (rows 1 to 4),
# 50 the staff HQ, 51 the advisor, 52 the bank account, 53 the object control; map cell k is 300 + k, track cell k is 400 + k
CELLS = 12
THING_LABELS = [20, 21, 22, 23]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def cf(x):
    """A float register's value as canonical bits."""
    return canon_f(CL.fbits(x))


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

R = CL.recorder


def returns(name, spec, key, mask=MASK):
    return R(name, spec, ret=lambda e, L: L.ret(key, 0) & mask)


def halfword(name, key, ints):
    """A hidden halfword result (r3 the destination): the event, then the answer of the queue `key` stored there."""
    def stub(e):
        L = e.cl
        L.rec(name, ints(e, L))
        e.write(e.r(3), struct.pack(">H", L.ret(key, 0) & 0xFFFF))
    return stub


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


def vtable(e, slots):
    vt = T.persist_alloc(e, 0x40)
    for slot, fn in slots.items():
        ws(e, vt + slot, [T.fake_function(e, fn)])
    return vt


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def assign(name):
        return lambda e: e.cl.rec(name, (e.cl.lab(e.r(3)), e.cl.lab(e.u32(G["mode"]))))

    def noop(e):
        pass
    e.sm_mode_vt = vtable(e, {8: destructor("Mode.dtor"), 0x2C: R("Mode.GetMode", "L", ret=lambda e, L: L.ret("ModeGetMode", 0) & MASK),
                              0x30: assign("Mode.OnAssign"), 0x34: assign("Mode.OnRelease")})
    e.sm_online_vt = vtable(e, {8: destructor("Online.dtor"), 0x2C: R("Online.GetMode", "L", ret=lambda e, L: 7),
                                0x30: assign("Online.OnAssign"), 0x34: noop})
    e.sm_pd_vt = vtable(e, {8: destructor("PathDraw.dtor"), 0x2C: lambda e: e.set_r(3, 3), 0x30: assign("PathDraw.OnAssign"), 0x34: noop})
    e.sm_kb_vt = vtable(e, {0x18: R("Keyboard.IsCommandDown", "L", ret=lambda e, L: L.ret("Command", 0) & MASK)})
    # the world: the map cells at +0x2d8 and the track cells at +0x1102d8, one block from the map's start
    e.sm_map = T.persist_alloc(e, 0x110000 + 0x800)
    e.sm_selected = 0
    e.sm = None


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)
    S.real.update(LANDED)

    def size_token(e, size):
        if size in (0x214, 0x218):
            e.sm_sizes.append(size)
            return 0x500
        return size

    def new(e):
        L, size = e.cl, e.r(3)
        token = size_token(e, size)
        if L.ret("New", 1) == 0:
            L.rec("operator_new", (token, 0))
            e.set_r(3, 0)
            return
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        L.newlab(block)
        L.rec("operator_new", (token, L.lab(block)))
        e.set_r(3, block)

    def keyboard_ctor(e):
        e.cl.rec("UKeyboard.ctor", (e.cl.lab(e.r(3)),))
        ws(e, e.r(3), [e.sm_kb_vt])

    def online_ctor(e):
        e.cl.rec("Online.ctor", (e.cl.lab(e.r(3)),))
        ws(e, e.r(3), [e.sm_online_vt, 0])

    def pd_ctor(e):
        e.cl.rec("PathDraw.ctor", (e.cl.lab(e.r(3)), s32(e.r(4))))
        ws(e, e.r(3), [e.sm_pd_vt, 0])

    def pd_ctor2(e):
        e.cl.rec("PathDraw.ctor2", (e.cl.lab(e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF))
        ws(e, e.r(3), [e.sm_pd_vt, 0])

    def ctrace(e):
        text = e.cstr(e.r(4))[:159]
        ints = [e.r(3) & MASK]
        if "%d" in text:
            ints.append(s32(e.r(5)))
        e.cl.rec("CTRACE", ints, (text,))

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def spawn(e):
        L, b = e.cl, e.r(3)
        L.rec("ESP_SpawnScript", [e.u32(b + 4 * i) for i in range(6)] + [s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), cf(e.f(1)), cf(e.f(2)), cf(e.f(3))])
        e.set_r(3, T.u32(L.ret("Spawn", 0)))

    def set_pos(e):
        e.cl.rec("SCR_SetScriptPosition", (s32(e.r(3)), cf(e.f(1)), cf(e.f(2)), cf(e.f(3)), s32(e.r(7))))

    def add_face(e):
        sp = e.r(1)
        e.cl.rec("DC_AddSquareFaceToList", (cf(e.f(1)), cf(e.f(2)), cf(e.f(3)), e.r(6) & 0xFF, s32(e.r(7)), s32(e.r(8)), cf(e.f(4)), s32(e.r(10)),
                                            e.u32(sp + 0x38) & 0xFF, s32(e.u32(sp + 0x3C)), s32(e.u32(sp + 0x40))))

    def override(e):
        e.cl.rec("LLCR_OverRide", (s32(e.r(4)), s32(e.r(5))))
        e.write(e.r(3), struct.pack(">H", e.cl.ret("Override", 0) & 0xFFFF))

    def world_getter(name, label):
        def stub(e):
            e.cl.rec(name, (e.cl.lab(e.r(3)),))
            e.set_r(3, e.sm.a(label))
        return stub

    def get_control(e):
        e.cl.rec("GetObjectControl", ((e.r(4) >> 16) & 0xFFFF, s32(e.r(5))))
        e.set_r(3, e.sm.a(53))

    def selected(e):
        e.cl.rec("UI_GetSelectedObject")
        e.set_r(3, e.sm_selected)

    def peep(e):
        e.cl.rec("UI_GetSelectedPeep")
        e.set_r(3, e.sm_peep)

    S.imp("__nw__FUl", new)

    def new_glue(e):     # the glue saves the caller's TOC at 0x14(r1) before the call, the caller reloads it afterwards
        e.write(e.r(1) + 0x14, struct.pack(">I", e.toc))
        new(e)
    S.internal(NEW, new_glue)
    S.imp("__dl__FPv", lambda e: delete(e, e.r(3)))
    S.imp(KEYBOARD_CTOR, keyboard_ctor)
    S.internal(ONLINE_CTOR, online_ctor)
    S.internal(PD_CTOR, pd_ctor)
    S.internal(PD_CTOR2, pd_ctor2)
    S.internal(CTRACE, ctrace)
    S.internal(ASSERT, assert_)
    S.internal(SPAWN, spawn)
    S.internal(KILL, R("SCR_KillScript", "i"))
    S.internal(SET_POS, set_pos)
    S.internal(ADD_FACE, add_face)
    S.internal(SET_CURSOR, R("SetGameWindowCursor", "xh"))
    S.internal(CHECK_CONTEXT, R("CheckMouseContext", "xi"))
    for address, name in ((IS_TRACK, "IsTrack"), (IS_HOARD, "IsHoarding"), (IS_GRASS, "IsGrass"), (IS_PATH, "IsPath"), (IS_QUEUE, "IsQueue")):
        S.internal(address, returns(name, "L", name))
    S.internal(IS_FLAG, returns("IsFlag", "LH", "IsFlag"))
    S.internal(FATE, R("FateOfPickedUpStaff", "Li"))
    S.internal(GET_HQ, world_getter("GetStaffHQ", 50))
    S.internal(GET_ADVISOR, world_getter("GetAdvisor", 51))
    S.internal(GET_BANK, world_getter("GetBankAccount", 52))
    S.internal(DROP, R("DropStaff", "Lw"))
    S.internal(CANCEL, R("CancelMessage", "Li"))
    for address, name in STAFF_CTORS.items():
        S.internal(address, R(name, "Lwibi"))
    S.internal(MAIN_MENU_UP, returns("MainMenuUp", "", "MainMenuUp"))
    S.internal(CHECK_SELECTED, R("UI_CheckSelected"))
    S.internal(GET_PEEP, peep)
    S.internal(GET_SELECTED, selected)
    S.internal(SET_SELECTED, R("UI_SetSelectedObject", "L"))
    S.internal(GOTO_STATUS, R("UIMain_GotoStatusPanelForThing", "L"))
    S.internal(VISITOR_SHOW, R("VisitorStatusPanel.Show", "xL"))
    S.internal(STAFF_SHOW, R("StaffStatusPanel.Show", "xL"))
    S.internal(ENTRY_PRICE, R("CreateEntryPricePanel"))
    S.internal(COASTER_EDIT, R("CoasterEditMode", "L"))
    S.internal(SET_CURRENT, R("LLCM_SetCurrentObject", "b"))
    S.internal(OVERRIDE, override)
    S.internal(SET_ROTATION, R("LLCC_SetRideRotation", "ib"))
    S.internal(SOUND, R("SOUND_SendEvent", "iiiiii"))
    S.internal(THING_ID, halfword("GetObjectThingId", "ThingId", lambda e, L: (L.lab(e.r(4)),)))
    S.internal(GET_CONTROL, get_control)
    S.internal(GET_BALANCE, returns("GetBalance", "L", "Balance"))
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
        self.map = e.sm_map
        e.sm = self
        e.sm_sizes = []
        e.sm_selected = e.sm_peep = 0

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def make(self, label, size):
        e = self.e
        if label == 9:
            a = e.alloc(8, struct.pack(">I", e.sm_mode_vt) + b"\xa5" * 4)
        elif label == 12:
            a = e.alloc(16, struct.pack(">I", e.sm_kb_vt) + b"\0" * 12)
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
        if key == "kb":
            self.words(self.e.data_import(KEYBOARD_VARIABLE), [value])
        elif key == "c38":
            self.words(G["c38"], [value])
        else:
            self.words(G[key], [value])

    def set_f24(self, v):
        self.half(G["intrface"] + 0x24, v)

    def set_thing(self, i, a):
        self.words(G["things"] + 0x14 * i, [a])

    def clear_things(self):
        self.write(G["things"], b"\0" * (0x14 * 16))

    def set_selected(self, a):
        self.e.sm_selected = a

    def set_peep(self, a):
        self.e.sm_peep = a

    def set_gtype(self, v):
        self.words(G["gtype"], [v])
        self.byte(G["guard"], 1)

    def set_bank(self, v):
        self.words(G["bank3"], [v])

    def set_pickup(self, bits):
        self.words(G["pickup"], bits)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def staff(self, kind, v):
        """The staff mode under test (label 10); `v` the fields: place (script, model, pay, f11, f14, placed), pick (script, sprite, f10)."""
        if kind == "place":
            a = self.make(10, 0x1C)
            self.words(a, [PLACE_VT, 0, v[0], v[1]])
            self.byte(a + 0x10, v[2])
            self.byte(a + 0x11, v[3])
            self.words(a + 0x14, [v[4]])
            self.byte(a + 0x18, v[5])
        else:
            a = self.make(10, 0x14)
            self.words(a, [PICK_VT, 0, v[0], v[1]])
            self.byte(a + 0x10, v[2])
        return a

    def fields(self, kind, a):
        u = self.e.u32
        if kind == "place":
            return [s32(u(a + 8)), s32(u(a + 0xC)), self.e.read(a + 0x10, 1)[0], self.e.read(a + 0x11, 1)[0], s32(u(a + 0x14)),
                    self.e.read(a + 0x18, 1)[0]]
        return [s32(u(a + 8)), s32(u(a + 0xC)), self.e.read(a + 0x10, 1)[0]]

    def dump(self):
        e, L = self.e, self.L
        u32 = e.u32
        return [L.lab(u32(e.data_import(KEYBOARD_VARIABLE))), L.lab(u32(G["mouse"])), L.lab(u32(G["mode"])), s32(u32(G["w1e0"])),
                s32(u32(G["w1e4"])), L.lab(e.sm_selected)]

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]

    def ctor(self, kind, args):
        e = self.e
        size = 0x1C if kind == "place" else 0x14
        a = e.alloc(size, b"\xa5" * size)
        self.call(P_CTOR if kind == "place" else K_CTOR, a, *args)
        vt = e.u32(a)
        assert vt == (PLACE_VT if kind == "place" else PICK_VT), hex(vt)
        return self.fields(kind, a)


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_sm_init()
        CL.host_begin(lib, {}, case["queues"])
        self.addr = {}

    def done(self):
        pass

    def make(self, label, size):
        a = self.lib.tpw_sm_fake_mode() if label == 9 else self.lib.tpw_sm_obj(label, size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.lib.tpw_cl_label(label, C.c_void_p(a))
        self.addr[label] = a

    def a(self, label):
        return self.addr.get(label, 0)

    def world(self):
        return self.lib.tpw_sm_addr(b"world")

    def map_address(self):
        return self.lib.tpw_sm_addr(b"map")

    def set_world(self, present):
        self.lib.tpw_sm_set(16, 1 if present else 0, 0)

    def setg(self, key, value):
        self.lib.tpw_sm_set({"kb": 0, "mouse": 1, "mode": 2, "w1e0": 3, "w1e4": 4, "c38": 5}[key], value, 0)

    def set_f24(self, v):
        self.lib.tpw_sm_set(6, v, 0)

    def set_thing(self, i, a):
        self.lib.tpw_sm_set(7, i, a)

    def clear_things(self):
        self.lib.tpw_sm_set(8, 0, 0)

    def set_selected(self, a):
        self.lib.tpw_sm_set(9, a, 0)

    def set_peep(self, a):
        self.lib.tpw_sm_set(10, a, 0)

    def set_gtype(self, v):
        self.lib.tpw_sm_set(11, v, 0)

    def set_bank(self, v):
        self.lib.tpw_sm_set(12, s32(v), 0)

    def set_pickup(self, bits):
        self.lib.tpw_sm_set(13, bits[0], bits[1])
        self.lib.tpw_sm_set(14, bits[2], 0)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def staff(self, kind, v):
        k = 0 if kind == "place" else 1
        a = self.lib.tpw_sm_new(k)
        self.name(10, a)
        ids = (0, 1, 2, 3, 4, 5) if kind == "place" else (0, 6, 7)
        for field, value in zip(ids, v):
            self.lib.tpw_sm_set_field(a, k, field, value)
        return a

    def fields(self, kind, a):
        k = 0 if kind == "place" else 1
        ids = (0, 1, 2, 3, 4, 5) if kind == "place" else (0, 6, 7)
        return [self.lib.tpw_sm_get_field(a, k, field) for field in ids]

    def dump(self):
        out = (C.c_int64 * 16)()
        self.lib.tpw_sm_dump(out)
        return list(out)[:6]

    def call(self, name, *args):
        return getattr(self.lib, name)(*(list(args[:1]) + [s32(x) for x in args[1:]]))

    def ctor(self, kind, args):
        if kind == "place":
            a = self.lib.tpw_sm_ctor_place(*[s32(x) for x in args])
        else:
            a = self.lib.tpw_sm_ctor_pick(*[s32(x) for x in args])
        return self.fields(kind, a)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, i32, i64, u32 = C.c_uint64, C.c_int32, C.c_int64, C.c_uint32
    lib.tpw_sm_addr.restype, lib.tpw_sm_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_sm_init.argtypes = []
    lib.tpw_sm_obj.restype, lib.tpw_sm_obj.argtypes = u64, [i32, u32]
    lib.tpw_sm_fake_mode.restype, lib.tpw_sm_fake_mode.argtypes = u64, []
    lib.tpw_sm_new.restype, lib.tpw_sm_new.argtypes = u64, [i32]
    lib.tpw_sm_ctor_place.restype, lib.tpw_sm_ctor_place.argtypes = u64, [i32] * 4
    lib.tpw_sm_ctor_pick.restype, lib.tpw_sm_ctor_pick.argtypes = u64, [i32] * 2
    lib.tpw_sm_set_field.argtypes = [u64, i32, i32, i64]
    lib.tpw_sm_get_field.restype, lib.tpw_sm_get_field.argtypes = i64, [u64, i32, i32]
    lib.tpw_sm_set.argtypes = [i32, i64, i64]
    lib.tpw_sm_dump.argtypes = [C.POINTER(i64)]
    lib.tpw_sm_layout.argtypes = [C.POINTER(i64)]
    lib.tpw_sm_context.restype = i32
    lib.tpw_sm_pickup.restype = u64
    for name in ("place_leftclick", "pick_leftclick", "pick_rightclick"):
        getattr(lib, "tpw_sm_" + name).argtypes = [u64, i32, i32]
    for name in ("place_assign", "place_release", "pick_assign", "pick_release"):
        getattr(lib, "tpw_sm_" + name).argtypes = [u64]
    for name in ("place_getmode", "pick_getmode"):
        getattr(lib, "tpw_sm_" + name).restype, getattr(lib, "tpw_sm_" + name).argtypes = i32, [u64]
    for name in ("place_dtor", "pick_dtor"):
        getattr(lib, "tpw_sm_" + name).argtypes = [u64, i32]
    lib.tpw_sm_base_move.argtypes = [u64, i32, i32, i32, i32, i32]
    lib.tpw_sm_base_release.argtypes = [u64, i32]
    lib.tpw_sm_base_dtor.argtypes = [u64, i32, i32]
    lib.tpw_cl_label.argtypes = [i32, C.c_void_p]
    lib.tpw_sm_init()
    out = (i64 * 32)()
    n = lib.tpw_sm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    st = case["st"]
    s.clear_things()
    mouse = s.make(8, 16)
    fake = s.make(9, 8)
    keyboard = s.make(12, 16)
    hq = s.make(50, 0x100)
    s.make(51, 0x40)
    s.make(52, 0x40)
    control = s.make(53, 0x40)
    s.name(1, s.world())
    s.words(control + 4, [st["price"]])
    s.half(hq + 0x60, st["mf60"])
    things = {}
    for i, label in enumerate(THING_LABELS):
        things[label] = s.make(label, 0x40)
        x, y, kind, rot = case["things"][i]
        s.byte(things[label] + 4, x)
        s.byte(things[label] + 6, y)
        s.half(things[label] + 0xE, kind)
        s.words(things[label] + 0x10, [rot])
        s.set_thing(i + 1, things[label])
    if st["row0"]:
        s.set_thing(0, things[st["row0"]])
    obj = 0
    if case.get("kind"):
        obj = s.staff(case["kind"], case["obj"])
    m = s.map_address()
    s.write(m, b"\0" * (0x44 * 16))
    s.write(m + 0x110000, b"\0" * (0x28 * 16))
    for k in range(1, CELLS + 1):
        base_cell, base_track = m + (k - 1) * 0x44, m + 0x110000 + (k - 1) * 0x28
        s.name(300 + k, base_cell)
        s.name(400 + k, base_track)
        ctype, parent = case["cells"][k - 1]
        s.words(base_cell + 8, [ctype])
        s.half(base_cell + 0x10, parent)
        ttype, tparent = case["tracks"][k - 1]
        s.words(base_track + 8, [ttype])
        s.half(base_track + 0x10, tparent)
    s.set_world(st["world"])
    s.setg("kb", keyboard if st["kb"] else 0)
    s.setg("mouse", mouse if st["mouse"] else 0)
    s.setg("mode", {"none": 0, "fake": fake, "self": obj}[st["mode"]])
    for key in ("w1e0", "w1e4", "c38"):
        s.setg(key, st[key])
    s.set_f24(st["f24"])
    s.set_gtype(st["gtype"])
    s.set_bank(st["bank3"])
    s.set_selected(things[st["sel"]] if st["sel"] else 0)
    s.set_peep(things[st["peep"]] if st["peep"] else 0)
    s.set_pickup(case["pickup"])
    return obj


def execute(s, case, obj):
    op, args = case["op"], case["args"]
    ppc = isinstance(s, Ppc)
    if op == "context":
        return s.call(CONTEXT) & MASK if ppc else s.call("tpw_sm_context") & MASK
    if op == "pickup":
        return (s.call(PICKUP) == G["pickup"]) if ppc else (s.lib.tpw_sm_pickup() == s.lib.tpw_sm_addr(b"pickup"))
    if op in ("ctor_place", "ctor_pick"):
        return s.ctor(op[5:], args)
    table = {   # op: (Mac address, host function)
        "place_left": (P_LEFT, "tpw_sm_place_leftclick"), "place_assign": (P_ASSIGN, "tpw_sm_place_assign"),
        "place_release": (P_RELEASE, "tpw_sm_place_release"), "place_dtor": (P_DTOR, "tpw_sm_place_dtor"),
        "place_getmode": (P_GETMODE, "tpw_sm_place_getmode"), "pick_left": (K_LEFT, "tpw_sm_pick_leftclick"),
        "pick_right": (K_RIGHT, "tpw_sm_pick_rightclick"), "pick_assign": (K_ASSIGN, "tpw_sm_pick_assign"),
        "pick_release": (K_RELEASE, "tpw_sm_pick_release"), "pick_dtor": (K_DTOR, "tpw_sm_pick_dtor"),
        "pick_getmode": (K_GETMODE, "tpw_sm_pick_getmode")}
    if op in table:
        address, name = table[op]
        if ppc:
            r = s.call(address, obj, *args)
        else:
            # the destructor tests its flag as a halfword (extsh); the unit's wrapper takes the sign of the whole int
            r = s.call(name, obj, *([T.s16(args[0])] if op.endswith("dtor") else args))
        return (r & MASK) if op.endswith("getmode") else None
    # the base class's members, called on the staff mode of the kind `case["kind"]`
    k = 0 if case["kind"] == "place" else 1
    if op == "base_move":
        s.call(B_MOVE, obj, *args) if ppc else s.call("tpw_sm_base_move", obj, k, *args)
    elif op == "base_release":
        s.call(B_RELEASE, obj) if ppc else s.call("tpw_sm_base_release", obj, k)
    elif op == "base_dtor":
        s.call(B_DTOR, obj, *args) if ppc else s.call("tpw_sm_base_dtor", obj, k, T.s16(args[0]))
    return None


def observe(s, case, obj, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    log = s.log()
    gone = any(name == "delete" and ints and ints[0] == 10 for name, ints, _ in log)
    out = {"answer": answer, "log": log, "state": s.dump()}
    if obj and not gone and case["op"] not in ("place_dtor", "pick_dtor", "base_dtor"):
        out["fields"] = s.fields(case["kind"], obj)
    return out


def run(s, case):
    try:
        obj = build(s, case)
        answer = execute(s, case, obj) if case["op"] not in ("ctor_place", "ctor_pick") else None
        if case["op"] in ("ctor_place", "ctor_pick"):
            # the constructor runs on a fresh block: the fields it stored (0xa5 filled before)
            return {"answer": None, "log": s.log(), "state": s.dump(), "fields": execute(s, case, obj)}
        return observe(s, case, obj, answer)
    finally:
        s.done()


def ppc(e, case):
    s = Ppc(e, case)
    out = run(s, case)
    # the Mac's size of the staff object a click makes: 0x218 for the models 4 and 5, 0x214 for 6, 7 and 8
    if case["op"] == "place_left" and e.sm_sizes:
        want = 0x218 if case["obj"][1] in (4, 5) else 0x214
        assert e.sm_sizes == [want], (e.sm_sizes, case["obj"])
    return out


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
CELL_TYPES = [0, 1, 2, 3, 4, 5, 9, 10, 0x14, 0x15, 0x16, 0x17, 0xFFFFFFFF, 7, 21, 0x7FFFFFFF]
TRACK_TYPES = [0, 0xB, 0xC, 0x10, 0x19, 5, 0xB, 0xC, 0x10, 0x19, 0x15, 0xFFFFFFFF]
PREDICATES = ["IsTrack", "IsFlag", "IsHoarding", "IsGrass", "IsPath", "IsQueue"]


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op, args=(), kind=None, mode=None):
    """The world every case starts from: random globals, things and cells; the answer queues cover every branch."""
    st = dict(kb=r.choice([0, 1, 1]), mouse=r.choice([0, 1, 1]), mode=mode or ("none" if kind is None else "self"),
              w1e0=r.choice([0, 5, -1, 7]), w1e4=r.choice([0, 5, -1, 7]), c38=r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, -1]),
              f24=r.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 5, 6]), gtype=r.choice([0, 0, 0, 1, 2]), bank3=r.choice([0, 7, -1, 0x1234]),
              world=1, mf60=r.choice([0, 0, 1, 2, 3, 4]), sel=r.choice([0, 20, 21, 22, 23]), peep=r.choice([0, 20, 21, 22, 23]),
              row0=r.choice([0, 0, 0, 0, 20]), price=r.choice([0, 100, -5, 5000, 0x7FFFFFFF]))
    things = [(r.randrange(256), r.randrange(256), r.choice([0, 1, 5, 0x1234, 0xFFFF]), r.choice([0, 1, 90, -90, 270])) for _ in THING_LABELS]
    cells = [(r.choice(CELL_TYPES), r.randrange(1, CELLS + 1)) for _ in range(CELLS)]
    tracks = [(r.choice(TRACK_TYPES), r.randrange(1, CELLS + 1)) for _ in range(CELLS)]
    queues = {"New": [r.choice([1, 1, 1, 1, 0]) for _ in range(4)], "MainMenuUp": [r.choice([0, 0, 0, 1])],
              "ModeGetMode": [r.choice([0, 1, 1, 2, 3, 8, 9, 5, -1, 7])], "ThingId": [r.choice([0, 1, 2, 3, 4, 5, 9])],
              "Override": [r.choice([0, 1, 2, 3, 4, 5])], "Command": [r.choice([0, 0, 1, 0x100, 0xFF, 0x101, 0x80, 0xFF00, 0x3])],
              "Balance": [r.choice([0, 100, -5, 5000, 0x7FFFFFFF, 99])], "Spawn": [r.choice([0, 1, 5, 77, -1])]}
    for name in PREDICATES:
        queues[name] = [r.choice([0, 0, 1])]
    pickup = [rf(r), rf(r), rf(r)]
    case = {"op": op, "args": list(args), "st": st, "things": things, "cells": cells, "tracks": tracks, "queues": queues, "pickup": pickup}
    if kind:
        case["kind"] = kind
        script = r.choice([0, 0, 1, 5, 77, -1])
        if kind == "place":
            case["obj"] = [script, r.choice([4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 0, 3, 9, -1]), r.randrange(256), r.randrange(256),
                           r.choice([0, 1, -1, 0x7FFFFFFF, 12345]), r.choice([0, 0, 1, 2, 255])]
        else:
            case["obj"] = [script, r.choice([0, 1, 4, 5, 6, 7, 8, 100, -1]), r.randrange(256)]
    return settle(case)


def settle(c):
    """Keeps a case inside what the original can take: the command key path needs an object (a thing array row with a thing) and the
    staff HQ's held id a thing."""
    if c["op"] == "context" and not c["st"]["kb"]:
        c["queues"]["New"] = [1, 1, 1, 1]     # a failed keyboard allocation is read through a null pointer by the original
    if c["op"] == "context" and c["st"]["c38"] == 4 and (c["queues"]["Command"][0] & 0xFF) != 0:
        if not 1 <= c["queues"]["Override"][0] <= 4:
            c["queues"]["Override"] = [1 + (c["queues"]["Override"][0] % 4)]
    return c


def settled(gen):
    return lambda: (settle(c) for c in gen())


def combos():
    for bits in range(64):
        yield {name: (bits >> i) & 1 for i, name in enumerate(PREDICATES)}


def with_predicates(c, answers):
    for name, v in answers.items():
        c["queues"][name] = [v]
    return c


# ---- ContextMouseClick
def ctx_case(r):
    return base(r, "context")


def ctx_edges():
    r = random.Random(14301)
    for world in (0, 1):
        for menu in (0, 1):
            c = base(r, "context")
            c["st"]["world"] = world
            c["queues"]["MainMenuUp"] = [menu]
            yield c
    for kind in list(range(-1, 17)) + [0x7FFFFFFF, 0x80000000 - 1 - 0x7FFFFFFF]:
        for answer in (0, 1, 2, -1):
            for mode in ("none", "fake"):
                for f24 in (0, 1, 7):
                    c = base(r, "context", mode=mode)
                    c["st"].update(c38=kind, f24=f24)
                    c["queues"]["ModeGetMode"] = [answer]
                    yield c
    for kb in (0, 1):
        for mouse in (0, 1):
            for mode in ("none", "fake"):
                for new in ([1, 1, 1, 1], [0, 1, 1, 1], [1, 0, 1, 1], [1, 1, 0, 1], [1, 1, 1, 0], [0] * 4):
                    for kind in (1, 4, 3):
                        c = base(r, "context", mode=mode)
                        c["st"].update(kb=kb, mouse=mouse, c38=kind, f24=r.randrange(1, 13), gtype=r.choice([0, 1]))
                        c["queues"].update(New=new, ModeGetMode=[r.choice([0, 1])], Command=[r.choice([0, 1])], Override=[2])
                        yield settle(c)
    # kind 3 and 6: the cell, its parent, the object's thing id
    for kind in (3, 6):
        for ctype in CELL_TYPES:
            for thing in (0, 1, 2, 3, 4, 7):
                c = base(r, "context")
                f24 = r.randrange(1, 13)
                c["st"].update(c38=kind, f24=f24, mouse=1, kb=1)
                c["queues"].update(ModeGetMode=[1], ThingId=[thing], MainMenuUp=[0])
                c["cells"][f24 - 1] = (ctype, r.randrange(1, 13))
                c["st"]["mode"] = r.choice(["none", "fake"])
                yield c
    # kind 4: the command key, the object found, the price against the balance
    for command in (0, 1, 0x100, 0xFF, 0x101, 0x80, 0xFF00, 0x3):
        for found in (0, 1, 2, 3, 4, 5):
            for balance, price in ((100, 100), (100, 101), (100, 99), (-5, -5), (-5, -4), (0x7FFFFFFF, -0x80000000), (-0x80000000, 0x7FFFFFFF)):
                c = base(r, "context")
                c["st"].update(c38=4, f24=r.randrange(1, 13), mouse=1, kb=r.choice([0, 1]), price=price, bank3=r.choice([0, 9, -1]),
                               gtype=r.choice([0, 1, 2]))
                c["queues"].update(ModeGetMode=[r.choice([0, 1])], Command=[command], Override=[found], Balance=[balance], MainMenuUp=[0])
                yield settle(c)
    # kind 5: the track cell types, the parents and the cell mode
    for ttype in TRACK_TYPES:
        for ptype in TRACK_TYPES:
            for thing in (0, 2):
                c = base(r, "context")
                f24 = r.randrange(1, 13)
                parent = r.randrange(1, 13)
                c["st"].update(c38=5, f24=f24, mouse=1, kb=1)
                c["queues"].update(ModeGetMode=[1], ThingId=[thing], MainMenuUp=[0])
                c["tracks"][f24 - 1] = (ttype, parent)
                c["tracks"][parent - 1] = (ptype, r.randrange(1, 13))
                yield c
    # kinds 7 to 14: the selected peep
    for kind in range(7, 15):
        for peep in (0, 20, 21):
            for gtype in (0, 1):
                c = base(r, "context")
                c["st"].update(c38=kind, peep=peep, mouse=1, gtype=gtype)
                c["queues"].update(ModeGetMode=[1], MainMenuUp=[0])
                yield c
    # the assertion on the thing array's row 0
    for row0 in (0, 20):
        for kind in (3, 4, 5, 6):
            c = base(r, "context")
            c["st"].update(c38=kind, row0=row0, f24=r.randrange(1, 13), mouse=1)
            c["queues"].update(ModeGetMode=[1], MainMenuUp=[0], Command=[1], Override=[1], ThingId=[1])
            yield settle(c)


# ---- the staff modes
def staff_case(op, kind, args=(), r=None, mode=None):
    return base(r, op, args, kind=kind, mode=mode)


def place_left_edges():
    r = random.Random(14302)
    for answers in combos():
        for model in (4, 5, 6, 7, 8, 3, 9):
            for mode in ("self", "fake", "none"):
                if r.random() < 0.5:
                    continue
                c = with_predicates(staff_case("place_left", "place", [r.randrange(-5, 99), r.randrange(-5, 99)], r, mode), answers)
                c["obj"][1] = model
                c["st"]["f24"] = r.randrange(1, 13)
                yield c
    for model in (4, 5, 6, 7, 8):
        for new in ([1, 1, 1, 1], [0, 1, 1, 1], [1, 0, 1, 1], [1, 1, 0, 1], [1, 1, 1, 0]):
            for gtype in (0, 1, 2):
                for mouse in (0, 1):
                    c = with_predicates(staff_case("place_left", "place", [3, 4], r), {name: 0 for name in PREDICATES})
                    c["queues"]["IsGrass"] = [1]
                    c["obj"][1] = model
                    c["queues"]["New"] = new
                    c["st"].update(gtype=gtype, mouse=mouse, f24=r.randrange(1, 13))
                    yield c
    for ctype in CELL_TYPES:
        c = with_predicates(staff_case("place_left", "place", [0, 0], r), {"IsTrack": 1})
        c["st"]["f24"] = 3
        c["cells"][2] = (ctype, 1)
        yield c


def place_left_random(r):
    return with_predicates(staff_case("place_left", "place", [r.randrange(-5, 99), r.randrange(-5, 99)], r), r.choice(list(combos())))


def place_assign_edges():
    r = random.Random(14303)
    for model in list(range(-1, 11)):
        for script in (0, 1, 77):
            for spawn in (0, 5, -1):
                for flag in (0, 1, 255):
                    c = staff_case("place_assign", "place", [], r)
                    c["obj"][:] = [script, model, r.randrange(256), flag, 7, 0]
                    c["queues"]["Spawn"] = [spawn]
                    yield c


def place_assign_random(r):
    return staff_case("place_assign", "place", [], r)


def place_release_edges():
    r = random.Random(14304)
    for placed in (0, 1, 2, 255):
        for script in (0, 1, -1, 99):
            c = staff_case("place_release", "place", [], r)
            c["obj"][0] = script
            c["obj"][5] = placed
            yield c


def place_release_random(r):
    return staff_case("place_release", "place", [], r)


def getmode_edges(op, kind, seed):
    def gen():
        r = random.Random(seed)
        for _ in range(4):
            yield staff_case(op, kind, [], r)
    return gen


def dtor_edges(op, kind, seed):
    def gen():
        r = random.Random(seed)
        for flag in (-1, 0, 1, 2, 3, 0x7FFFFFFF, 0x10001, 0x8000, 0x7FFF):
            for mode in ("none", "self", "fake"):
                c = staff_case(op, kind, [flag], r, mode)
                yield c
    return gen


def dtor_random(op, kind):
    return lambda r: staff_case(op, kind, [r.choice([-1, 0, 1, 2, 3])], r)


def pick_left_edges():
    r = random.Random(14305)
    for answers in combos():
        for mode in ("self", "fake", "none"):
            if r.random() < 0.5:
                continue
            c = with_predicates(staff_case("pick_left", "pick", [r.randrange(-5, 99), r.randrange(-5, 99)], r, mode), answers)
            c["st"]["f24"] = r.randrange(1, 13)
            yield c
    for new in ([1, 1, 1, 1], [0, 1, 1, 1], [1, 0, 1, 1], [1, 1, 0, 1], [0] * 4):
        for gtype in (0, 1, 2):
            for mouse in (0, 1):
                c = with_predicates(staff_case("pick_left", "pick", [1, 2], r), {name: 0 for name in PREDICATES})
                c["queues"]["IsPath"] = [1]
                c["queues"]["New"] = new
                c["st"].update(gtype=gtype, mouse=mouse, f24=r.randrange(1, 13))
                yield c


def pick_left_random(r):
    return with_predicates(staff_case("pick_left", "pick", [r.randrange(-5, 99), r.randrange(-5, 99)], r), r.choice(list(combos())))


def pick_right_edges():
    r = random.Random(14306)
    for _ in range(6):
        yield staff_case("pick_right", "pick", [r.randrange(-5, 99), r.randrange(-5, 99)], r)


def pick_assign_edges():
    r = random.Random(14307)
    for sprite in (0, 1, 4, 5, 6, 7, 8, 9, -1, 0x7FFFFFFF):
        for script in (0, 1, -1, 99):
            for flag in (0, 1, 255):
                c = staff_case("pick_assign", "pick", [], r)
                c["obj"][:] = [script, sprite, flag]
                c["queues"]["Spawn"] = [r.choice([0, 5, -1])]
                yield c


def pick_release_edges():
    r = random.Random(14308)
    for mf60 in (0, 1, 2, 3, 4):
        for script in (0, 1, -1):
            for row0 in (0, 20):
                c = staff_case("pick_release", "pick", [], r)
                c["st"].update(mf60=mf60, row0=row0)
                c["obj"][0] = script
                yield c


def pick_release_random(r):
    return staff_case("pick_release", "pick", [], r)


def base_move_edges():
    r = random.Random(14309)
    for kind in ("place", "pick"):
        for answers in combos():
            for script in (0, 5):
                c = with_predicates(staff_case("base_move", kind, [r.randrange(-5, 9)] * 4, r), answers)
                c["obj"][0] = script
                c["st"]["f24"] = r.randrange(1, 13)
                yield c
    for bits in SPECIAL + [fb(0.0), fb(-5.0), fb(5.0), fb(1e30), fb(-1e30), fb(3.4e38), fb(1e-40)]:
        for k in range(3):
            c = staff_case("base_move", r.choice(["place", "pick"]), [0, 0, 0, 0], r)
            c["obj"][0] = 3
            c["pickup"] = [bits if k == 0 else rf(r), bits if k == 1 else rf(r), bits if k == 2 else rf(r)]
            c["queues"]["IsGrass"] = [0]
            c["queues"]["IsPath"] = [0]
            c["queues"]["IsQueue"] = [0]
            for name in ("IsTrack", "IsFlag", "IsHoarding"):
                c["queues"][name] = [r.choice([0, 1])]
            yield c


def base_move_random(r):
    return with_predicates(staff_case("base_move", r.choice(["place", "pick"]), [r.randrange(-5, 9) for _ in range(4)], r), r.choice(list(combos())))


def base_release_edges():
    r = random.Random(14310)
    for kind in ("place", "pick"):
        for script in (0, 1, -1, 5):
            yield _with_script(staff_case("base_release", kind, [], r), script)


def _with_script(c, script):
    c["obj"][0] = script
    return c


def base_dtor_edges():
    r = random.Random(14311)
    for kind in ("place", "pick"):
        for flag in (-1, 0, 1, 2, 0x10001, 0x8000):
            yield staff_case("base_dtor", kind, [flag], r)


def ctor_place_edges():
    r = random.Random(14312)
    for model in (0, 4, 5, 8, -1, 0x7FFFFFFF):
        for pay in (0, 1, 255):
            for flag in (0, 7, 255):
                for value in (0, -1, 0x7FFFFFFF, 123):
                    c = base(r, "ctor_place")
                    c["args"] = [model, pay, flag, value]
                    yield c


def ctor_place_random(r):
    c = base(r, "ctor_place")
    c["args"] = [r.choice([0, 4, 5, 6, 7, 8, r.randrange(-100, 100)]), r.randrange(256), r.randrange(256), r.choice([0, -1, r.randrange(-2**31, 2**31)])]
    return c


def ctor_pick_edges():
    r = random.Random(14313)
    for sprite in (0, 1, 4, 5, 6, 7, 8, -1, 0x7FFFFFFF):
        for flag in (0, 1, 255):
            c = base(r, "ctor_pick")
            c["args"] = [sprite, flag]
            yield c


def ctor_pick_random(r):
    c = base(r, "ctor_pick")
    c["args"] = [r.choice([0, 4, 5, 6, r.randrange(-100, 100)]), r.randrange(256)]
    return c


def pickup_edges():
    r = random.Random(14314)
    for _ in range(4):
        yield base(r, "pickup")


def from_edges(edge):
    cache = []

    def rnd(r):
        if not cache:
            cache.extend(edge())
        return r.choice(cache)
    return rnd


TESTS = [
    FnTest("ContextMouseClick", ppc, host, edge=settled(ctx_edges), rnd=ctx_case),
    FnTest("CMMPlaceStaff::OnLeftClick", ppc, host, edge=place_left_edges, rnd=place_left_random),
    FnTest("CMMPlaceStaff::OnAssign", ppc, host, edge=place_assign_edges, rnd=place_assign_random),
    FnTest("CMMPlaceStaff::OnRelease", ppc, host, edge=place_release_edges, rnd=place_release_random),
    FnTest("CMMPlaceStaff::~CMMPlaceStaff", ppc, host, edge=dtor_edges("place_dtor", "place", 14315), rnd=dtor_random("place_dtor", "place")),
    FnTest("CMMPlaceStaff::CMMPlaceStaff", ppc, host, edge=ctor_place_edges, rnd=ctor_place_random),
    FnTest("CMMPlaceStaff::GetMode", ppc, host, edge=getmode_edges("place_getmode", "place", 14316), rnd=from_edges(getmode_edges("place_getmode", "place", 14316))),
    FnTest("CMMPickUpStaff::OnLeftClick", ppc, host, edge=pick_left_edges, rnd=pick_left_random),
    FnTest("CMMPickUpStaff::OnRelease", ppc, host, edge=pick_release_edges, rnd=pick_release_random),
    FnTest("CMMPickUpStaff::OnAssign", ppc, host, edge=pick_assign_edges, rnd=from_edges(pick_assign_edges)),
    FnTest("CMMPickUpStaff::~CMMPickUpStaff", ppc, host, edge=dtor_edges("pick_dtor", "pick", 14317), rnd=dtor_random("pick_dtor", "pick")),
    FnTest("CMMPickUpStaff::CMMPickUpStaff", ppc, host, edge=ctor_pick_edges, rnd=ctor_pick_random),
    FnTest("CMMPickUpStaff::GetMode", ppc, host, edge=getmode_edges("pick_getmode", "pick", 14318), rnd=from_edges(getmode_edges("pick_getmode", "pick", 14318))),
    FnTest("CMMPickUpStaff::OnRightClick", ppc, host, edge=pick_right_edges, rnd=from_edges(pick_right_edges)),
    FnTest("CSpritePlaceMode::OnMouseMove", ppc, host, edge=base_move_edges, rnd=base_move_random),
    FnTest("CSpritePlaceMode::~CSpritePlaceMode", ppc, host, edge=base_dtor_edges, rnd=from_edges(base_dtor_edges)),
    FnTest("CSpritePlaceMode::OnRelease", ppc, host, edge=base_release_edges, rnd=from_edges(base_release_edges)),
    FnTest("GetPickupWorldPosn", ppc, host, edge=pickup_edges, rnd=from_edges(pickup_edges)),
]
