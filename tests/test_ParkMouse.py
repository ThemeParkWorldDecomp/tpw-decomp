"""Unit w142, the park pointer, against libtpw_unit_ParkMouse.dylib (tests/unit_ParkMouse.cpp), main app ("SimTheme Park"):
  src/game/ui/ui_main.cpp   ContextMouse 0x10182548
  src/game/Mouse.cpp    CMouse::ImmediateMouseHandler 0x1007e094, CMouse::ProcessIngameMouse 0x1007de84, CMouse::DoubleClick 0x1007e248

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments and answers from the case's queues: the current mouse mode (a fake object whose virtuals
record), the keyboard (GetModifiers), the nothrow operator new (sizes 1, 8 and 0xc), AssignMouseMode, the CMMDefault constructor, the cell
predicates, GetObjectThingId, LLCR_OverRide, the park entrance and ticket booth positions, the object and staff members, the text table
(GetConstString, FormatString with its tag), UIMain_ConvertMulti, SetHelpTextString, the cursor, StaffPanelUp, UI_GetSelectedObject,
HighlightRide, LLCC_SystemGetMode and GAME_ASSERTMSG.

The world of a case: gKeyboard, gMouse and the mouse mode, the mouse words and button flags, the ride highlight keep word, the help
thing and kind words (0x10380c34, 0x10380c38), gIntrface's pointed thing and cell id, four test things (kind byte at +2, words at +0x28
and +0x1d8), the thing array's entries 1 to 4, twelve map cells and twelve track cells (type at +8, parent at +0x10), the ride record
(+0x7a8 and +0x288) and the queues. What is compared: the event log and every global the functions touch (pointers as labels), and the
first 0x40 characters of the conversion buffer.

No fused operations. Not tested: a NULL thing where the original reads it (a thing array entry of an id whose thing is read), a cell id of 0
for a parent (the 32 bit wrap of the Mac's address arithmetic), a cell past the twelve.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkMouse"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
CONTEXT, IMMEDIATE, INGAME, DOUBLE = 0x10182548, 0x1007E094, 0x1007DE84, 0x1007E248
SIZES = {CONTEXT: 3508, IMMEDIATE: 216, INGAME: 248, DOUBLE: 52}
OWN = set(SIZES)

# ---- the callees faked on both sides
HIGHLIGHT, ASSIGN_MODE, MMDEFAULT_CTOR = 0x1006CE28, 0x1007E2E0, 0x1007EA78
OVERRIDE, LLCC_MODE = 0x10089210, 0x100914F4
IS_ENTRANCE, IS_EXIT, IS_RIDE, IS_GRASS = 0x1009C4DC, 0x1009C524, 0x1009C61C, 0x1009C660
BOOTH_POS, ENTRANCE_POS, THING_ID = 0x101019BC, 0x10101AE0, 0x10103A04
GET_DB_INFO, UPGRADES_WHAT, GET_UI_NAME = 0x10107CD4, 0x1011050C, 0x10110690
BEING_POINTED, GET_NAME = 0x10125F30, 0x10126F98
CONVERT_MULTI, SET_HELP, GET_SELECTED, SET_CURSOR, STAFF_PANEL_UP = 0x10180334, 0x1018C694, 0x10194730, 0x101A8C58, 0x101C0CAC
ASSERT = 0x1000DDE8
KEYBOARD_CTOR = "__ct__Q213SamsUtilities9UKeyboardFv"
KEYBOARD_VARIABLE = "gKeyboard__13SamsUtilities"

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host's come from the library
G = dict(mouse=0x102DC1F4, mode=0x102DC1F0, w1e0=0x102DC1E0, w1e4=0x102DC1E4, left=0x102DC1EC, right=0x102DC1E8, keep=0x102C6CC8,
         c34=0x10380C34, c38=0x10380C38, intrface=0x102DC1AC, world=0x1037EBFC, things=0x1034CBFC, buffer=0x10380C48, text=0x1037FF18)
GKEYS = ["kb", "mouse", "mode", "w1e0", "w1e4", "left", "right", "keep", "c34", "c38"]
# the host's offsets and sizes the module relies on (checked once against tpw_pm_layout)
HOST_LAYOUT = [0x44, 0x28, 0x2D8, 0x1102D8, 8, 0x10, 0xA98, 0x510]

# test objects: label -> size. 8 the mouse, 9 the fake mode, 12 the preset keyboard, 20 to 23 things, 30 the text table, 31 the buffer, 40 the map;
# map cell k (the 1 based id) is 300 + k, track cell k is 400 + k (the blocks of the nothrow new take 100 up).
CELLS = 12
THING_LABELS = [20, 21, 22, 23]


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def wide(text):
    return b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0"


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
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


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # the fake mode's vtable: the slots at +0x0c to +0x2c record
    vt = T.persist_alloc(e, 0x40)
    for slot, name, count in ((0x0C, "OnLeftClick", 2), (0x10, "OnLeftRelease", 2), (0x14, "OnRightClick", 2), (0x18, "OnRightRelease", 2),
                              (0x1C, "OnDoubleClick", 2), (0x20, "OnMouseMove", 4), (0x24, "WhileLeftDown", 2), (0x28, "WhileRightDown", 2)):
        def fn(e, name=name, count=count):
            L = e.cl
            L.rec(name, (L.lab(e.r(3)),) + tuple(s32(e.r(4 + i)) for i in range(count)))
        ws(e, vt + slot, [T.fake_function(e, fn)])

    def get_mode(e):
        L = e.cl
        L.rec("GetMode", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("GetMode", 0)))
    ws(e, vt + 0x2C, [T.fake_function(e, get_mode)])
    e.pm_mode_vt = vt
    # the keyboard's vtable: GetModifiers at +0x1c
    kvt = T.persist_alloc(e, 0x40)

    def modifiers(e):
        L = e.cl
        L.rec("Keyboard.GetModifiers", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("Modifiers", 0)))
    ws(e, kvt + 0x1C, [T.fake_function(e, modifiers)])
    e.pm_keyboard_vt = kvt
    e.pm_staff_name = T.persist_alloc(e, 32, wide("StaffName"))
    # the world: the map cells at +0x2d8 and the track cells at +0x1102d8, one block from the map's start
    e.pm_map = T.persist_alloc(e, 0x110000 + 0x800)
    e.pm_ride = T.persist_alloc(e, 0x800)
    e.pm_selected = 0


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)

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

    def keyboard_ctor(e):
        L = e.cl
        this = e.r(3)
        L.rec("UKeyboard.ctor", (L.lab(this),))
        ws(e, this, [e.pm_keyboard_vt])

    def get_const(e):
        L = e.cl
        L.rec("GetConstString", (L.lab(e.r(3)), e.r(4)))
        e.set_r(3, CL.new_text(e, "cs%x" % e.r(4)))

    def format_string(e):
        L = e.cl
        table, out, size, ident, tag = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        head = (L.lab(table), L.lab(out), size, ident)
        shown = ""
        if ident in (0x226, 0x22B, 0x244):
            shown = CL.wide_text(e, e.u32(tag + 12))
            L.rec("FormatString", head + (e.u32(tag), e.u32(tag + 8)), (shown,))
        elif ident == 0x229:
            L.rec("FormatString", head + (e.u32(tag), e.u32(tag + 8), s32(e.u32(tag + 12))))
        else:
            L.rec("FormatString", head)
        text = "F%x" % ident + (":" + shown if shown else "")
        n = min(len(text), max(0, size - 1))
        e.write(out, wide(text[:n]))
        e.set_r(3, n)

    def convert(e):
        L = e.cl
        text = e.cstr(e.r(3))
        L.rec("UIMain_ConvertMulti", (), (text[:159],))
        e.write(G["buffer"], wide(text[:0x1FF]))
        e.set_r(3, G["buffer"])

    def override(e):
        L = e.cl
        L.rec("LLCR_OverRide", (s32(e.r(4)), s32(e.r(5))))
        e.write(e.r(3), struct.pack(">H", L.ret("Override", 0) & 0xFFFF))

    def ui_name(e):
        L = e.cl
        L.rec("GetUIName", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5)))
        e.write(e.r(4), wide("UIName"))

    def get_name(e):
        e.cl.rec("StaffGetName", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.pm_staff_name)

    def db_info(e):
        e.cl.rec("GetDBInfo", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.pm_ride)

    def help_text(e):
        L = e.cl
        L.rec("SetHelpTextString", (L.lab(e.r(3)),), (CL.wide_text(e, e.r(3)),))

    def selected(e):
        e.cl.rec("UI_GetSelectedObject")
        e.set_r(3, e.pm_selected)

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    S.imp("__nw__FUl", new)
    S.imp(KEYBOARD_CTOR, keyboard_ctor)
    S.imp("GetConstString__8TbITableCFUl", get_const)
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)
    S.internal(HIGHLIGHT, R("HighlightRide", ""))
    S.internal(ASSIGN_MODE, R("AssignMouseMode", "LL"))
    S.internal(MMDEFAULT_CTOR, R("CMMDefault.ctor", "L"))
    S.internal(OVERRIDE, override)
    S.internal(LLCC_MODE, returns("LLCC_SystemGetMode", "", "LLCC_SystemGetMode"))
    for address, name in ((IS_ENTRANCE, "IsEntrance"), (IS_EXIT, "IsExit"), (IS_RIDE, "IsRide"), (IS_GRASS, "IsGrass")):
        S.internal(address, returns(name, "L", name))
    S.internal(THING_ID, halfword("GetObjectThingId", "ThingId", lambda e, L: (L.lab(e.r(4)),)))
    S.internal(ENTRANCE_POS, halfword("GetParkEntrancePos", "Entrance", lambda e, L: (L.lab(e.r(4)), s32(e.r(5)))))
    S.internal(BOOTH_POS, halfword("GetTicketBoothPos", "Booth", lambda e, L: (L.lab(e.r(4)), s32(e.r(5)))))
    S.internal(GET_DB_INFO, db_info)
    S.internal(UPGRADES_WHAT, halfword("UpgradesWhat", "Upgrades", lambda e, L: (L.lab(e.r(4)),)))
    S.internal(GET_UI_NAME, ui_name)
    S.internal(BEING_POINTED, R("BeingPointedAt", "L"))
    S.internal(GET_NAME, get_name)
    S.internal(CONVERT_MULTI, convert)
    S.internal(SET_HELP, help_text)
    S.internal(GET_SELECTED, selected)
    S.internal(SET_CURSOR, R("UI_SetGameWindowCursor", "h"))
    S.internal(STAFF_PANEL_UP, returns("StaffPanelUp", "", "StaffPanelUp"))
    S.internal(ASSERT, assert_)
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

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.addr = {}
        self.map = e.pm_map

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def make(self, label, size):
        e = self.e
        if label == 9:
            a = e.alloc(8, struct.pack(">I", e.pm_mode_vt) + b"\xa5" * 4)
        elif label == 12:
            a = e.alloc(16, struct.pack(">I", e.pm_keyboard_vt) + b"\0" * 12)
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

    def table(self):
        self.make(30, 64)

    def world(self):
        return self.map - 0x2D8

    def reg(self, name):
        return G[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setg(self, key, value):
        self.words(G[key] if key != "kb" else self.e.data_import(KEYBOARD_VARIABLE), [value])

    def set_intrface(self, pointed, f24):
        self.words(G["intrface"] + 4, [pointed])
        self.half(G["intrface"] + 0x24, f24)

    def set_thing(self, i, a):
        self.words(G["things"] + 0x14 * i, [a])

    def clear_things(self):
        self.write(G["things"], b"\0" * (0x14 * 16))

    def set_selected(self, a):
        self.e.pm_selected = a

    def set_ride(self, ui, up):
        self.words(self.e.pm_ride + 0x7A8, [ui])
        self.words(self.e.pm_ride + 0x288, [up])

    def clear_buffer(self):
        self.write(G["buffer"], b"\x77" * 0x100)

    def place_world(self, table_label):
        self.words(G["world"], [self.world()])
        self.words(G["text"] + 0x568, [self.a(table_label)])

    def buffer_address(self):
        return G["buffer"]

    def map_address(self):
        return self.map

    def log(self):
        return self.L.log

    def word_label(self, w):
        return self.L.labels.get(w & MASK, w & MASK) + (1000 if (w & MASK) in self.L.labels else 0)

    def dump(self):
        e, L = self.e, self.L
        u32 = e.u32
        kb = u32(e.data_import(KEYBOARD_VARIABLE))
        return [L.lab(kb), L.lab(u32(G["mouse"])), L.lab(u32(G["mode"])), s32(u32(G["w1e0"])), s32(u32(G["w1e4"])), s32(u32(G["left"])),
                s32(u32(G["right"])), s32(u32(G["keep"])), self.word_label(u32(G["c34"])), s32(u32(G["c38"]))]

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.addr = {}
        self.rev = {}
        lib.tpw_pm_init()

    def done(self):
        pass

    def make(self, label, size):
        a = self.lib.tpw_pm_obj(label, size)
        self.name(label, a)
        return a

    def name(self, label, a):
        self.lib.tpw_cl_label(label, C.c_void_p(a))
        self.addr[label] = a
        self.rev[a & MASK] = label

    def a(self, label):
        return self.addr.get(label, 0)

    def table(self):
        self.name(30, self.lib.tpw_pm_addr(b"table"))

    def world(self):
        return self.lib.tpw_pm_addr(b"world")

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setg(self, key, value):
        self.lib.tpw_pm_set(GKEYS.index(key), value, 0)

    def set_intrface(self, pointed, f24):
        self.lib.tpw_pm_set(10, pointed, f24)

    def set_thing(self, i, a):
        self.lib.tpw_pm_set(11, i, a)

    def clear_things(self):
        self.lib.tpw_pm_set(15, 0, 0)

    def set_selected(self, a):
        self.lib.tpw_pm_set(12, a, 0)

    def set_ride(self, ui, up):
        self.lib.tpw_pm_set(13, ui, up)

    def clear_buffer(self):
        self.lib.tpw_pm_set(14, 0, 0)

    def place_world(self, table_label):
        pass

    def buffer_address(self):
        return self.lib.tpw_pm_addr(b"buffer")

    def map_address(self):
        return self.lib.tpw_pm_addr(b"map")

    def log(self):
        return CL.host_end(self.lib)

    def word_label(self, w):
        return self.rev.get(w & MASK, w & MASK) + (1000 if (w & MASK) in self.rev else 0)

    def dump(self):
        out = (C.c_int64 * 16)()
        self.lib.tpw_pm_dump(out)
        v = list(out)[:10]
        v[8] = self.word_label(v[8])
        return v

    def call(self, name, *args):
        return getattr(self.lib, name)(*[s32(x) for x in args])


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_pm_addr.restype, lib.tpw_pm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pm_init.argtypes = []
    lib.tpw_pm_obj.restype, lib.tpw_pm_obj.argtypes = C.c_uint64, [C.c_int32, C.c_uint32]
    lib.tpw_pm_set.argtypes = [C.c_int32, C.c_int64, C.c_int64]
    lib.tpw_pm_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pm_context.argtypes = [C.c_int32]
    lib.tpw_pm_immediate.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_pm_ingame.argtypes = [C.c_int32] * 4
    lib.tpw_pm_double.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_pm_init()
    out = (C.c_int64 * 32)()
    n = lib.tpw_pm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(v) for v in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    st = case["st"]
    s.clear_things()
    s.clear_buffer()
    mouse = s.make(8, 16)
    mode = s.make(9, 8)
    keyboard = s.make(12, 16)
    s.table()
    s.name(31, s.buffer_address())
    things = {}
    for i, label in enumerate(THING_LABELS):
        things[label] = s.make(label, 0x200)
        kind, w28, w1d8 = case["things"][i]
        s.byte(things[label] + 2, kind)
        s.words(things[label] + 0x28, [w28])
        s.words(things[label] + 0x1D8, [w1d8])
        s.set_thing(i + 1, things[label])
    s.place_world(30)
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
    s.setg("kb", {0: 0, 1: keyboard}[st["kb"]])
    s.setg("mouse", mouse if st["mouse"] else 0)
    s.setg("mode", mode if st["mode"] else 0)
    for key in ("w1e0", "w1e4", "left", "right", "keep", "c38"):
        s.setg(key, st[key])
    c34 = st["c34"]
    s.setg("c34", things[c34] if c34 in things else c34)
    pointed = st["pointed"]
    s.set_intrface(things[pointed] if pointed else 0, st["f24"])
    s.set_selected(things[st["selected"]] if st["selected"] else 0)
    s.set_ride(*st["ride"])


def execute(s, case):
    op, args = case["op"], case["args"]
    if isinstance(s, Ppc):
        address = {"context": CONTEXT, "immediate": IMMEDIATE, "ingame": INGAME, "double": DOUBLE}[op]
        if op in ("immediate", "double"):
            s.call(address, s.a(8), *args)
        elif op == "ingame":
            s.call(address, *args, 0)
        else:
            s.call(address, *args)
    else:
        s.call({"context": "tpw_pm_context", "immediate": "tpw_pm_immediate", "ingame": "tpw_pm_ingame", "double": "tpw_pm_double"}[op], *args)


def observe(s, case):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    return {"log": s.log(), "state": s.dump(), "text": s.rd_halves(s.buffer_address(), 0x40)}


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


# ---------------------------------------------------------------------------------------------------------------- case generators

CELL_TYPES = [0, 1, 2, 3, 4, 5, 9, 10, 0x14, 0x15, 0x16, 0x17, 0xFFFFFFFF, 7, 21, 21]
TRACK_TYPES = [0, 0xB, 0xC, 0x10, 0x19, 5, 0xB, 0xC, 0x10, 0x19, 0x15, 0xFFFFFFFF]
KINDS = [0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x12, 0xFF]


def pick_id(r):
    return r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3])


def base(r, op, args=()):
    """The world every case starts from: random globals, things and cells; the answer queues cover every branch."""
    st = dict(kb=r.choice([0, 1, 1]), mouse=r.choice([0, 1, 1]), mode=r.choice([0, 1, 1]),
              w1e0=r.choice([0, 5, -1, 7]), w1e4=r.choice([0, 5, -1, 7]), left=r.choice([0, 0, 1, -1, 5]), right=r.choice([0, 0, 1, -1, 3]),
              keep=r.choice([0, 0, 1, 7]), c38=r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1]),
              c34=r.choice([0, 0, 20, 21, 22, 23, 0x77, -1]), pointed=r.choice([0, 0, 20, 21, 22, 23]), f24=pick_id(r),
              selected=r.choice([0, 20, 21, 22, 23]), ride=(r.choice([0, 0, 1, 5, -1]), r.choice([0, 2, 2, 5, -1])))
    things = [(r.choice(KINDS), r.choice([0, 1, 7]), r.choice([0, 0, 5, 0x1234, -1])) for _ in THING_LABELS]
    cells = [(r.choice(CELL_TYPES), r.randrange(1, CELLS + 1)) for _ in range(CELLS)]
    tracks = [(r.choice(TRACK_TYPES), r.randrange(1, CELLS + 1)) for _ in range(CELLS)]
    f24 = st["f24"]
    entrance = [r.choice([f24, f24, f24 + 1, f24 - 1, f24 + 2, 0, 0xFFFF, r.randrange(0, 14)]) & 0xFFFF for _ in range(2)]
    booth = [r.choice([f24, f24 + 1, 0, r.randrange(0, 14)]) & 0xFFFF for _ in range(2)]
    queues = {"New": [1, 1, 1], "StaffPanelUp": [r.choice([0, 0, 1, 2, 2, 3, -1])],
              "Modifiers": [r.choice([0, 0, 0x200, 0x1100, 0x100, 0x1000, 0x800, 0x200 | 0x800, 0x1300, 0xFFFF, 0x5, 0x400, 0x200 | 0x1100])],
              "GetMode": [r.choice([0, 1, 1, 2, 3, 8, 9, 9, 3, 8, 5, -1, 7])], "LLCC_SystemGetMode": [r.choice([0x14, 0x15, 0x16, 0x17, 0, 3, 0x15])],
              "ThingId": [r.choice([0, 1, 2, 3, 4])], "Upgrades": [r.choice([0, 0, 1, 7])],
              "Entrance": entrance, "Booth": booth, "Override": [r.choice([0, 1, 2, 3, 4, 5])],
              "IsEntrance": [r.choice([0, 1])], "IsExit": [r.choice([0, 1])], "IsRide": [r.choice([0, 1])], "IsGrass": [r.choice([0, 1])]}
    return {"op": op, "args": list(args), "st": st, "things": things, "cells": cells, "tracks": tracks, "queues": queues}


def settle(c, r):
    """Keeps a case inside what the original can take: the things it reads must exist (mode 9 reads the thing it was given)."""
    q = c["queues"]
    if q["GetMode"][0] == 9 and c["args"][0] == 0 and c["st"]["mode"]:
        q["Override"] = [r.choice([0, 1, 2, 3, 4])]
        q["Override"] += [r.choice([1, 2, 3, 4])] * 2
    if c["st"]["mode"] == 0 and c["st"]["mouse"] == 0 and c["args"] and c["args"][0] == 0:
        pass
    return c


def ctx_case(r, context=None):
    c = base(r, "context", [r.choice([0, 0, 0, 1]) if context is None else context])
    return settle(c, r)


def ctx_edges():
    r = random.Random(14201)
    for context in (0, 1, -1, 2):
        for mode in (0, 1, 2, 3, 8, 9, 5, -1):
            for llcc in (0x14, 0x15, 0x16, 0x17):
                for keep in (0, 1):
                    c = ctx_case(r, context)
                    c["st"].update(keep=keep, mode=1, mouse=1)
                    c["queues"]["GetMode"] = [mode]
                    c["queues"]["LLCC_SystemGetMode"] = [llcc]
                    yield settle(c, r)
    for mods in (0, 0x200, 0x1100, 0x100, 0x1000, 0x800, 0x201, 0x900, 0x1900, 0x300):
        for staff in (0, 1, 2, 3, -1):
            for kind in (0, 1, 2, 4, 5, 6, 7, 8, 9):
                for context in (0, 1):
                    c = ctx_case(r, context)
                    c["queues"].update(Modifiers=[mods], StaffPanelUp=[staff], GetMode=[1])
                    c["st"].update(mode=1, mouse=1, pointed=20, c38=r.choice([0, 7, 8, 9, 10, 11, 12]), c34=r.choice([0, 20, 21]))
                    c["things"][0] = (kind, r.choice([0, 1]), r.choice([0, 9]))
                    yield settle(c, r)
    for kb in (0, 1):
        for new in (0, 1):
            for mouse in (0, 1):
                for mode in (0, 1):
                    for newmode in (0, 1):
                        c = ctx_case(r, 0)
                        c["st"].update(kb=kb, mouse=mouse, mode=mode)
                        c["queues"]["New"] = [1, 1, 1]
                        yield settle(c, r)
    # the cell analysis: the type of the cell and of the track cell under the pointer
    for ctype in CELL_TYPES:
        for ttype in TRACK_TYPES:
            for staff_free in (0,):
                c = ctx_case(r, 0)
                c["queues"].update(Modifiers=[0], StaffPanelUp=[0], GetMode=[1])
                c["st"].update(mode=1, mouse=1, pointed=0, f24=5, c38=r.choice([0, 1, 3, 4, 5, 2, 6]))
                c["cells"][4] = (ctype, c["cells"][4][1])
                c["tracks"][4] = (ttype, r.randrange(1, CELLS + 1))
                c["queues"]["Entrance"] = [r.choice([0, 9]), r.choice([0, 9])]
                c["queues"]["Booth"] = [r.choice([0, 9]), r.choice([0, 9])]
                yield settle(c, r)
    for e0 in (0, 4, 5, 6, 7):
        for e1 in (0, 3, 4, 5, 6):
            for b0 in (0, 5, 4):
                for b1 in (0, 5, 3):
                    c = ctx_case(r, 0)
                    c["queues"].update(Modifiers=[0], StaffPanelUp=[0], GetMode=[1], Entrance=[e0, e1], Booth=[b0, b1])
                    c["st"].update(mode=1, mouse=1, pointed=0, f24=5, c38=r.choice([0, 13, 14]))
                    yield settle(c, r)
    # the predicates of the track cell branch, the thing ids, the upgrade answers
    for rid in (0, 1):
        for exit_ in (0, 1):
            for ent in (0, 1):
                for grass in (0, 1):
                    for up in (0, 1):
                        for override in (0, 1, 5):
                            for ttype in (0xB, 0x10, 0xC):
                                c = ctx_case(r, 0)
                                c["queues"].update(Modifiers=[0], StaffPanelUp=[0], GetMode=[1], IsRide=[rid], IsExit=[exit_], IsEntrance=[ent],
                                                   IsGrass=[grass], Upgrades=[up, up], Override=[override], Entrance=[0, 0], Booth=[0, 0],
                                                   ThingId=[r.choice([0, 1, 2])])
                                c["st"].update(mode=1, mouse=1, pointed=0, f24=3, c38=r.choice([0, 5, 2]))
                                c["tracks"][2] = (ttype, r.randrange(1, CELLS + 1))
                                if ttype == 0xC:
                                    c["tracks"][c["tracks"][2][1] - 1] = (r.choice([0x19, 0xB, 0]), 1)
                                c["cells"][2] = (r.choice([0, 1, 3, 4, 9, 10, 0x15, 5]), c["cells"][2][1])
                                yield settle(c, r)
    # the upgrade case: type 4, 9 and 10, the ride record's upgrade type, the object already shown
    for ctype in (4, 9, 10):
        for up in (0, 3):
            for kind in (0, 2, 3, -1):
                for c34 in (0, 20, 21):
                    for override in (0, 1, 5):
                        c = ctx_case(r, 0)
                        c["queues"].update(Modifiers=[0], StaffPanelUp=[0], GetMode=[1], Upgrades=[up], Override=[override],
                                           Entrance=[0, 0], Booth=[0, 0], IsRide=[0], IsExit=[0], IsEntrance=[0], IsGrass=[0])
                        c["st"].update(mode=1, mouse=1, pointed=0, f24=3, c38=r.choice([0, 4]), c34=c34, ride=(0, kind))
                        c["cells"][2] = (ctype, 1)
                        c["tracks"][2] = (0, 1)
                        yield settle(c, r)
    # mode 9 and the ride modes: the thing found under the pointer
    for found in (0, 1, 2, 3, 4):
        for kind in (2, 3):
            for up in (0, 1):
                for ui in (0, 1):
                    for c34 in (0, 20, 21):
                        for c38 in (0, 4):
                            c = ctx_case(r, 0)
                            c["queues"].update(GetMode=[9], Override=[found], Upgrades=[up])
                            c["st"].update(mode=1, mouse=1, f24=r.choice([0, 1, 5]), c34=c34, c38=c38, ride=(ui, 0))
                            c["things"][max(found, 1) - 1] = (kind, 0, 0)
                            yield settle(c, r)
    for ctype in (0x15, 0, 0xFFFFFFFF, 1):
        for thing_id in (0, 1, 2, 3):
            for selected in (0, 20, 21, 22):
                for mode, llcc in ((8, 0), (3, 0x15), (3, 0x16), (3, 0x14)):
                    c = ctx_case(r, 0)
                    c["queues"].update(GetMode=[mode], LLCC_SystemGetMode=[llcc], ThingId=[thing_id])
                    c["st"].update(mode=1, mouse=1, keep=0, f24=r.choice([0, 3, 4]), selected=selected)
                    c["cells"][2] = (ctype, r.randrange(1, CELLS + 1))
                    c["cells"][3] = (ctype, r.randrange(1, CELLS + 1))
                    yield settle(c, r)


def ctx_random(r):
    return ctx_case(r)


def imm_case(r):
    c = base(r, "immediate", [r.choice([0, 1, -1, 5, 7, 640, -32768, 0x7FFFFFFF, 2000]), r.choice([0, 1, -1, 5, 7, 480, -32768, 0x7FFFFFFF])])
    c["st"].update(mode=1, mouse=1)
    if r.random() < 0.4:
        c["st"]["w1e4"], c["st"]["w1e0"] = c["args"][0], c["args"][1]
    elif r.random() < 0.3:
        c["st"]["w1e4"] = c["args"][0]
    elif r.random() < 0.3:
        c["st"]["w1e0"] = c["args"][1]
    return c


def imm_edges():
    r = random.Random(14202)
    for left in (0, 1, -1, 5):
        for right in (0, 1, -1):
            for same in range(4):
                c = imm_case(r)
                x, y = c["args"]
                c["st"].update(left=left, right=right, w1e4=x if same & 1 else x + 1, w1e0=y if same & 2 else y - 1)
                yield c
    yield from (imm_case(r) for _ in range(20))


def ingame_case(r):
    c = base(r, "ingame", [r.choice([0xF0, 0xF1, 0xF0, 0xF1, 0xF2, 0, -1, 0xEF, 0x1F0]), r.choice([0, 1, 2, -1, 1, 0x100000001 & MASK]),
                           r.choice([0, 5, -3, 640]), r.choice([0, 7, -9, 480])])
    c["st"].update(mode=1, mouse=1)
    return c


def ingame_edges():
    r = random.Random(14203)
    for key in (0xF0, 0xF1, 0xF2, 0xEF, 0, -1, 0x1F0):
        for down in (0, 1, 2, -1, 0x100000001 & MASK):
            for left in (0, 1, 5):
                for right in (0, 1, 5):
                    c = ingame_case(r)
                    c["args"][0], c["args"][1] = key, down
                    c["st"].update(left=left, right=right)
                    yield c


def double_case(r):
    c = base(r, "double", [r.choice([0, 1, -1, 640, 0x7FFFFFFF, -32768]), r.choice([0, 1, -1, 480, 0x7FFFFFFF, -32768])])
    c["st"].update(mode=1, mouse=1)
    return c


def double_edges():
    r = random.Random(14204)
    for _ in range(12):
        c = double_case(r)
        c["st"]["mode"] = r.choice([1, 1, 1])
        yield c


TESTS = [
    FnTest("ContextMouse", ppc, host, edge=ctx_edges, rnd=ctx_random),
    FnTest("CMouse::ImmediateMouseHandler", ppc, host, edge=imm_edges, rnd=imm_case),
    FnTest("CMouse::ProcessIngameMouse", ppc, host, edge=ingame_edges, rnd=ingame_case),
    FnTest("CMouse::DoubleClick", ppc, host, edge=double_edges, rnd=double_case),
]

