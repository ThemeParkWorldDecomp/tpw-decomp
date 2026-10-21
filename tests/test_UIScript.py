"""Differential tests for the window script interpreter and the control setters, against libtpw_unit_UIScript.dylib
(tests/unit_UIScript.cpp):

  src/game/ui/InterfaceWindow.cpp   InterfaceWindow::Build, ProcessScript, SetDrawObject, SetClientDrawObject, GetVisibility,
                                   SetCallbackFunction, CaptureKb
  src/game/ui/CoolRegion.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp        InterfaceKnob::SetMinMaxAngles, InterfaceListBox::SetListRect, InterfaceListControl2::SetListRect,
                                   SetColumns, SetColumnPos, CoolCircle::CoolCircle, CoolEllipse::CoolEllipse, CoolPoly::CoolPoly

Real on both sides: those, the window constructor (src/game/ui/InterfaceWindow.cpp, which builds the test trees and is called by the
fake factory) and the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). Fakes that record the same event on both sides: InterfaceWindow::CreateObject
(makes a window of the asked kind with the real constructor and links it as the parent's last child; the queue "CreateObject"
makes a call answer NULL), WhatAmI (answers the kind the window was made with), GetWindow (the queue "GetWindow": 1 answers the
window itself), DrawObjectLibrary::FindDrawObject (the queue "FindDrawObject": the index of the world's draw object, -1 none),
the draw objects (Show, Change with the text style byte, the destructor), the message senders, UI_UpdatePointerIn, the window's
other virtual members, the region shapes' destructors, the fake regions' destructor, the assertion and operator new (the queue
"NewFails": a nonzero value makes a call answer NULL, on both sides).

A case is a script of operations on a world of eight windows (made by the test with the kind WhatAmI answers: a plain window, a
scroll bar (3), a list box (4), a list control (7), a knob (0xb), a vary box (0xc), a light panel (0xd) and a button), up to 320
more made by the factory, six draw objects, three fonts, two fake regions and six window scripts. After every operation the answer,
every window (the Mac's 0x138 bytes by name, the kind, the region and the control's own words), the draw objects, the globals, the
heap traffic and the length of the call log are compared, and the whole call log at the end. The PowerPC side decides which
operations the state allows (one that would make the original use a window that was not made, or follow a loop of links, is skipped
on both sides).

Two real scripts from the program image are run: the login panel (0x102abbb0, 371 words) and the lobby's window (0x1029e382).

Quirks the tests pin: SetClientDrawObject's release deletes the window's MAIN draw object (and leaves the pointer); a list box
asked twice what it is; a script record for the wrong kind of window asserts and goes on; record 9 on a window that is neither a
list box nor a list control takes no words; SetColumns(0) leaves the column count 0; the draw objects' user count is SET to 1.
"""
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as UP

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIScript"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(Build=0x101DEB78, ProcessScript=0x101DEC14, SetDrawObject=0x101DB474, SetClientDrawObject=0x101DB624,
          GetVisibility=0x101DBD9C, SetCallback=0x101DCBA8, CaptureKb=0x101DCCEC, MinMax=0x101D0260, BoxRect=0x101D11B0,
          ControlRect=0x101D47F0, SetColumns=0x101D2488, SetColumnPos=0x101D2584, Circle=0x101D5BB0, Ellipse=0x101D5CE4,
          Poly=0x101D5E2C)
# what they call that is real too: the window constructor, MakeRectAbsolute, the rectangles and the regions' base
WINDOW_CTOR = 0x101D98D8
REAL_HELPERS = [WINDOW_CTOR, 0x101DAB88, 0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678,
                0x101C7710, 0x101C7788, 0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00,
                0x101D5B74]
# the fakes (in-binary functions)
CREATE_OBJECT, WHAT_AM_I, GET_WINDOW, FIND_DRAW_OBJECT = 0x101DC750, 0x101D9EB0, 0x101DBE40, 0x101CD3B0
POST_MESSAGE, UPDATE_POINTER_IN, CALC_CLIENT_AREA = 0x101C9400, 0x101C8F18, 0x101D987C
CIRCLE_DTOR, ELLIPSE_DTOR, POLY_DTOR = 0x101D6200, 0x101D6180, 0x101D5F04

# ---- the original's data
KEYBOARD, SCRIPT_POSITION = 0x10395C10, 0x10395C5C
CORE_VTABLE, CIRCLE_VTABLE, ELLIPSE_VTABLE, POLY_VTABLE = 0x102AF8FC, 0x102AFCB8, 0x102AFC84, 0x102AFC50
DEFAULT_CALLBACK_TV, THUMB_CALLBACK_TV = 0x10268480, 0x10268390
SIZE_WINDOW = 0x138
BLOCK = 0x300               # the room of a window on the PowerPC side (the control classes' members follow the window)
CONTROL_KINDS = (4, 7, 0xB)

# ---- labels (the host's tpw_uis_labels)
LAB_WINDOW, LAB_DRAW, LAB_REGION, LAB_FONT = 10, 20, 30, 50
SLOTS, CREATED, DRAWS, REGIONS, FONTS, SCRIPTS = 8, 320, 6, 2, 3, 6
SCRIPT_WORDS = 4096
MAX_QUEUE = 64              # the call log's return queues hold this many values; the last repeats
MAX_COLUMNS = 8
FONT_IDS = (0x101, 0x108, 0x1FF)

# the kinds of the world's windows, by slot
WORLD_KINDS = (1, 3, 4, 7, 0xB, 0xC, 0xD, 2)
PLAIN, SCROLLBAR, LISTBOX, LISTCONTROL, KNOB, VARYBOX, LIGHTPANEL, BUTTON = range(8)

# the records of a script
(R_CREATE, R_DRAW, R_CLIENTDRAW, R_CLIENTRECT, R_REGION, R_END, R_UP, R_DOWN, R_THUMB, R_LISTSCROLL, R_LISTRECT, R_COLUMNS,
 R_HEADER, R_KNOB, R_PLUS, R_MINUS, R_LIGHT, R_HELP, R_F90) = range(19)


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def words(values):
    """A script as the PowerPC side keeps it: big-endian shorts."""
    return struct.pack(">%dh" % len(values), *[s16(v) for v in values])


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures and the operations."""

    ADDRESSES = [(KEYBOARD, 4), (SCRIPT_POSITION, 4)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.windows = [e.alloc(BLOCK, b"\xa5" * BLOCK) for _ in range(SLOTS)]
        self.made = [False] * SLOTS
        self.kinds = {}                # window address -> the kind WhatAmI answers (the world's and the factory's)
        self.created = []              # the windows the fake factory made, in order
        self.draws = []
        for _ in range(DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.uis_draw_vtable)
            self.draws.append(block)
        self.regions = []
        for _ in range(REGIONS):
            block = e.alloc(8, bytes(8))
            w32(e, block, e.uis_region_vtable)
            self.regions.append(block)
        self.fonts = []
        for k in range(FONTS):
            block = e.alloc(0x10, bytes(0x10))
            w32(e, block + 0xC, case.get("fonts", FONT_IDS)[k])
            self.fonts.append(block)
        self.scripts = []
        for k in range(SCRIPTS):
            script = case.get("scripts", {}).get(k, [])
            data = words(script) + words([R_END] * (SCRIPT_WORDS - len(script)))   # (see the host's reset)
            self.scripts.append(e.alloc(len(data), data))
        fixed = {}
        fixed.update({a: LAB_DRAW + i for i, a in enumerate(self.draws)})
        fixed.update({a: LAB_REGION + i for i, a in enumerate(self.regions)})
        fixed.update({a: LAB_FONT + i for i, a in enumerate(self.fonts)})
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues)
        # the globals as the Mac's data has them
        w32(e, KEYBOARD, 0)
        w32(e, SCRIPT_POSITION, 0)

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def window(self, slot):
        return self.windows[slot] if 0 <= slot < SLOTS and self.made[slot] else 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def kind_of(self, address):
        return self.kinds.get(address, 1)

    def rect_code(self, address):
        for w in [self.windows[s] for s in range(SLOTS) if self.made[s]] + self.created:
            k = {4: 1, 0x10: 2, 0x1C: 3, 0x28: 4, 0x34: 5}.get(address - w)
            if k:
                return 10 * self.L.lab(w) + k
        return -1

    def acyclic(self):
        """False when the windows' links make a loop (the original would never come back from one)."""
        e = self.e
        known = {w for s, w in enumerate(self.windows) if self.made[s]} | set(self.created)

        def chain(start, offset):
            seen = set()
            w = start
            while w and w in known:
                if w in seen:
                    return False
                seen.add(w)
                w = e.u32(w + offset)
            return True

        return all(chain(w, 0x50) and chain(w, 0x5C) and chain(e.u32(w + 0x58), 0x50) for w in known)

    def u32(self, slot, offset):
        return self.e.u32(self.windows[slot] + offset)

    # ---- the dumps (the orders are tpw_uis_dump_* in the library)
    def dump_window(self, address):
        e, L = self.e, self.L
        d = bytes(e.read(address, SIZE_WINDOW))
        s16_ = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        s32_ = lambda o: struct.unpack(">i", d[o:o + 4])[0]   # noqa: E731
        u32_ = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        lab = lambda o: L.lab(u32_(o))   # noqa: E731
        out = [d[0]]
        for offset in (4, 0x10, 0x1C, 0x28, 0x34):
            out += [s16_(offset + 4), s16_(offset + 6), s16_(offset + 8), s16_(offset + 10)]
        region = u32_(0x40)
        out.append(0 if region == 0 else 1 if region == address + 4 else L.lab(region))
        out += [s32_(0x44), lab(0x48), s16_(0x4C), lab(0x50), lab(0x54), lab(0x58), lab(0x5C)]
        out += [s32_(0x60), s32_(0x64), int(d[0x68:0x88] == b"\xa5" * 0x20)]
        out += [s32_(0x88), s32_(0x8C), s32_(0x90), lab(0x94), lab(0x98), int(d[0x9C:0xDC] == bytes(0x40))]
        out += [s16_(0xDC), s16_(0xDE), s16_(0xE0), s16_(0xE2), lab(0xE4), s16_(0xE8), s32_(0xEC), s32_(0xF0)]
        out += [s16_(0xF4), s16_(0xF6), s16_(0xF8), s16_(0xFA)]
        out += [lab(0xFC)] + [s16_(0x100 + 2 * i) for i in range(4)] + [s16_(0x108), int(d[0x10A:0x110] == b"\xa5" * 6)]
        out += [self.callback_code(u32_(0x110)), 0 if u32_(0x114) == 0 else 1 if u32_(0x114) == DEFAULT_CALLBACK_TV else 2, s32_(0x11C)]
        out += [lab(0x120 + 4 * i) for i in range(4)]
        out += [s32_(0x130), d[0x134], d[0x135], T.s8(d[0x136])]
        # the kind the fakes know, the region and the control's own words
        out.append(self.kind_of(address))
        out += self.region_words(region, address)
        out += self.control_words(address)
        return out

    def callback_code(self, tvector):
        if tvector == 0:
            return 0
        return {DEFAULT_CALLBACK_TV: 1, THUMB_CALLBACK_TV: 2, self.e.uis_callbacks[1]: 3, self.e.uis_callbacks[2]: 4}.get(tvector, 5)

    def region_words(self, region, window):
        e = self.e
        if region == 0 or region == window + 4:
            return [0] * 5
        vptr = e.u32(region)
        coordinates = struct.unpack(">4h", e.read(region + 4, 8))
        if vptr == CORE_VTABLE:
            return [3] + list(coordinates)
        if vptr == CIRCLE_VTABLE:
            return [4, coordinates[0], coordinates[1], coordinates[2], 0]
        if vptr == ELLIPSE_VTABLE:
            return [5] + list(coordinates)
        if vptr == POLY_VTABLE:
            count = struct.unpack(">h", e.read(region + 4, 2))[0]
            points = e.u32(region + 8)
            out = [6, count, int(points != 0), 0, 0]
            if points:
                out += list(struct.unpack(">%dh" % (2 * count), e.read(points, 4 * count))) if count > 0 else []
            return out
        return [2, self.L.lab(region), 0, 0, 0]   # a fake region of the world

    def control_words(self, window):
        e = self.e
        kind = self.kind_of(window)
        if kind == 0xB:
            return list(struct.unpack(">3i", e.read(window + 0x138, 12)))
        if kind == 4:
            return list(struct.unpack(">4h", e.read(window + 0x13C, 8)))
        if kind == 7:
            out = list(struct.unpack(">4h", e.read(window + 0x13C, 8)))
            count = struct.unpack(">h", e.read(window + 0x164, 2))[0]
            columns = e.u32(window + 0x16C)
            out += [count, int(columns != 0)]
            if columns and 0 < count <= MAX_COLUMNS:
                for i in range(count):
                    out += struct.unpack(">4h", e.read(columns + 0xC * i + 4, 8))
            return out
        return []

    def dump_globals(self):
        e, L = self.e, self.L
        out = [L.lab(e.u32(KEYBOARD)), T.s32(e.u32(SCRIPT_POSITION))]
        for block in self.draws:
            out += list(struct.unpack(">5h", e.read(block + 4, 6) + e.read(block + 0xC, 4)))
        return out

    def dump_state(self):
        windows = [self.dump_window(self.windows[s]) if self.made[s] else [0] for s in range(SLOTS)]
        windows += [self.dump_window(w) for w in self.created]
        return dict(windows=windows, globals=self.dump_globals(), heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        needs_window = code in (2, 3, 4, 6, 22, 23, 24, 25, 26) or 27 <= code <= 31
        if needs_window and not self.window(a[0]):
            return None
        if code in (20, 21) and a[0] != -1 and not self.window(a[0]):
            return None
        if code == 1:
            slot, parent = a[0], a[1]
            if self.made[slot] or (parent >= 0 and not self.made[parent]) or parent >= slot:
                return None
            e = self.e
            block = self.windows[slot]
            e.write(block, b"\xa5" * BLOCK)
            self.L.labels[block] = LAB_WINDOW + slot
            self.call(WINDOW_CTOR, block, self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]), s16(a[7]))
            self.made[slot] = True
            self.kinds[block] = a[8]
            if a[8] in CONTROL_KINDS:
                e.write(block + SIZE_WINDOW, bytes(BLOCK - SIZE_WINDOW))
            return [self.L.lab(block), 0, 0, 0, 0]
        if code in (2, 3, 4, 5, 6, 7):
            self.setup(code, a)
            if code == 2 and not self.acyclic():
                raise AssertionError("the script made a loop of links")
            return [0, 0, 0, 0, 0]
        return self.function(code, a)

    def setup(self, code, a):
        e = self.e
        if code == 5:   # a draw object's counts: index, in library, resizable, shown
            block = self.draws[a[0]]
            e.write(block + 0xC, struct.pack(">hh", s16(a[1]), s16(a[2])))
            e.write(block + 4, struct.pack(">h", s16(a[3])))
            return
        if code == 7:   # a global: which, value
            if a[0] == 1:
                w32(e, KEYBOARD, self.window(a[1]))
            elif a[0] == 2:
                w32(e, SCRIPT_POSITION, a[1])
            return
        w = self.windows[a[0]]
        if code == 2:   # links: slot, parent, previous, next, child
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
        elif code == 3:   # fields: slot, attributes, region, font, z, z rotation, pivot x and y
            w32(e, w + 0x44, a[1])
            w32(e, w + 0x40, w + 4 if a[2] == 0 else 0 if a[2] < 0 else self.regions[a[2] - 1])
            w32(e, w + 0x48, self.fonts[a[3]] if a[3] >= 0 else 0)
            e.write(w + 0xDC, struct.pack(">hhhh", s16(a[4]), s16(a[5]), s16(a[6]), s16(a[7])))
        elif code == 4:   # draw objects: slot, the window's, the client's
            w32(e, w + 0x94, self.draws[a[1]] if a[1] >= 0 else 0)
            w32(e, w + 0x98, self.draws[a[2]] if a[2] >= 0 else 0)
        elif code == 6:   # control state: slot, which, values
            if a[1] == 1:
                e.write(w + 0x138, struct.pack(">3i", T.s32(a[2]), T.s32(a[3]), T.s32(a[4])))
            elif a[1] in (2, 3):
                e.write(w + 0x13C, struct.pack(">4h", s16(a[2]), s16(a[3]), s16(a[4]), s16(a[5])))
            elif a[1] == 4:
                e.write(w + 0x164, struct.pack(">h", s16(a[2])))

    def allowed(self, code, a):
        """False for an operation the state does not allow."""
        if code in (22, 23, 24, 25, 26) or 27 <= code <= 31 or code in (20, 21):
            if not self.acyclic():
                return False
        if 27 <= code <= 31:
            want = {27: 0xB, 28: 4, 29: 7, 30: 7, 31: 7}[code]
            if self.kind_of(self.windows[a[0]]) != want:
                return False
            if code == 31 and a[1] < 0:
                return False   # the original writes before the column array
            if code == 30 and not 0 <= a[1] <= MAX_COLUMNS:
                return False
        return True

    def function(self, code, a):
        if not self.allowed(code, a):
            return None
        e, L = self.e, self.L
        w = self.windows[a[0]] if 0 <= a[0] < SLOTS else 0
        answer = [0, 0, 0, 0, 0]
        before = len(self.created)
        if code == 20:
            w32(e, SCRIPT_POSITION, a[2])
            answer[0] = L.lab(self.call(FN["ProcessScript"], w, self.scripts[a[1]]))
            answer[1] = T.s32(e.u32(SCRIPT_POSITION))
        elif code == 21:
            answer[0] = L.lab(self.call(FN["Build"], w, self.scripts[a[1]], self.callback(a[2])))
            answer[1] = T.s32(e.u32(SCRIPT_POSITION))
        elif code == 22:
            self.call(FN["SetDrawObject"], w, self.draws[a[1]] if a[1] >= 0 else 0)
        elif code == 23:
            self.call(FN["SetClientDrawObject"], w, self.draws[a[1]] if a[1] >= 0 else 0)
        elif code == 24:
            answer[0] = T.s32(self.call(FN["GetVisibility"], w))
        elif code == 25:
            self.call(FN["SetCallback"], w, self.callback(a[1]))
        elif code == 26:
            self.call(FN["CaptureKb"], w, a[1])
        elif code == 27:
            pair = e.alloc(8, struct.pack(">ii", T.s32(a[3]), T.s32(a[4])))
            self.call(FN["MinMax"], w, pair if a[1] else 0, pair + 4 if a[2] else 0)
        elif code == 28:
            self.call(FN["BoxRect"], w, s16(a[1]), s16(a[2]), s16(a[3]), s16(a[4]))
        elif code == 29:
            self.call(FN["ControlRect"], w, s16(a[1]), s16(a[2]), s16(a[3]), s16(a[4]))
        elif code == 30:
            self.call(FN["SetColumns"], w, s16(a[1]))
        elif code == 31:
            self.call(FN["SetColumnPos"], w, s16(a[1]), s16(a[2]), s16(a[3]))
        else:
            raise ValueError(code)
        answer[2] = len(self.created) - before
        return answer

    def callback(self, code):
        return {0: 0, 1: self.e.uis_callbacks[1], 2: self.e.uis_callbacks[2], 3: THUMB_CALLBACK_TV}[code]


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects', regions' and callbacks' vtables and TVectors, in persistent memory."""
    if hasattr(e, "uis_draw_vtable"):
        return

    def destroyed(name):
        def dtor(emu):
            L, this, flag = emu.cl, emu.r(3), T.s16(emu.r(4))
            L.rec(name, (L.lab(this),))
            if flag > 0:
                emu.us.heap[2] += 1
                if L.lab(this) > 0:
                    L.rec("delete", (L.lab(this),))
                L.forget(this)
        return dtor

    def show(emu):
        emu.cl.rec("Draw.Show", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4))))

    def change(emu):
        L, this, flags, record = emu.cl, emu.r(3), emu.r(4), emu.r(5)
        frame, depth, rotation = struct.unpack(">3h", emu.read(record + 4, 6))
        pivot_x, pivot_y = struct.unpack(">2h", emu.read(record + 0xC, 4))
        rect = emu.u32(record + 0x14)
        coordinates = struct.unpack(">4h", emu.read(rect + 4, 8))
        L.rec("Change", (L.lab(this), flags, frame, depth, rotation, emu.us.rect_code(rect)) + coordinates + (pivot_x, pivot_y))
        if flags & 0x10000:
            style = emu.u32(record + 0x10)
            L.rec("Change.style", (L.lab(this), emu.read(style + 8, 1)[0]))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x08, struct.pack(">I", T.fake_function(e, destroyed("Draw.dtor"))))
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.write(vt + 0x1C, struct.pack(">I", T.fake_function(e, show)))
    e.uis_draw_vtable = vt

    vt = T.persist_alloc(e, 0x20)
    e.write(vt + 0x18, struct.pack(">I", T.fake_function(e, destroyed("FakeRegion.dtor"))))
    e.uis_region_vtable = vt

    # the world's two message functions (only their addresses matter)
    e.uis_callbacks = [0, T.persist_alloc(e, 8, struct.pack(">II", 0x7C0A0001, e.toc)),
                       T.persist_alloc(e, 8, struct.pack(">II", 0x7C0A0002, e.toc))]


def callback_stub(fn):
    """An import stub that calls PowerPC code back (tpwtest.stub_cb, but for one case's stubs): fn(emu, call)."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(UP.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
    return hook


def create_object(e, call):
    """The fake factory: the real constructor builds the window; it becomes the parent's last child."""
    us, L = e.us, e.cl
    parent, kind, attributes, ident = e.r(3), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))
    left, top, right, bottom = T.s16(e.r(7)), T.s16(e.r(8)), T.s16(e.r(9)), T.s16(e.r(10))
    L.rec("CreateObject", (L.lab(parent), kind, attributes, ident, left, top, right, bottom))
    if L.ret("CreateObject") != 0 or len(us.created) >= CREATED:
        return 0
    block = e.alloc(BLOCK, b"\xa5" * BLOCK)
    L.newlab(block)   # (before the constructor, which records events)
    call(("code", WINDOW_CTOR), block, parent, attributes, ident, left, top, right, bottom)
    if kind in CONTROL_KINDS:
        e.write(block + SIZE_WINDOW, bytes(BLOCK - SIZE_WINDOW))
    us.created.append(block)
    us.kinds[block] = kind
    if parent:
        last = e.u32(parent + 0x58)
        if last == 0:
            w32(e, parent + 0x58, block)
        else:
            while e.u32(last + 0x50):
                last = e.u32(last + 0x50)
            w32(e, last + 0x50, block)
            w32(e, block + 0x54, last)
    return block


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL_HELPERS)
    R = CL.recorder
    us = lambda e: e.us   # noqa: E731

    def op_new(e):
        if e.cl.ret("NewFails") != 0:   # (the queue "NewFails" makes a call answer NULL)
            e.set_r(3, 0)
            return
        us(e).heap[0] += 1
        e.set_r(3, e.alloc(max(e.r(3), 4)))

    def op_new_array(e):
        us(e).heap[1] += 1
        e.set_r(3, e.alloc(max(e.r(3), 4)))

    def op_delete(e):
        block = e.r(3)
        if block:
            us(e).heap[2] += 1
            if e.cl.lab(block) > 0:
                e.cl.rec("delete", (e.cl.lab(block),))
                e.cl.forget(block)

    def op_delete_array(e):
        if e.r(3):
            us(e).heap[3] += 1

    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_array)
    S.imp("__dl__FPv", op_delete)
    S.imp("__dla__FPv", op_delete_array)

    def construct(e, call):
        raw, ctor, dtor, size, n = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        if raw == 0:
            return 0
        w32(e, raw, size)
        w32(e, raw + 4, n)
        base = raw + 8
        for i in range(n):
            call(ctor, base + i * size, 1)
        return base

    S.imp("__construct_new_array", callback_stub(construct))
    S.imp("__assertion_failed", lambda e: e.cl.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))

    def count_delete(e, L):
        if T.s16(e.r(4)) > 0:
            us(e).heap[2] += 1

    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    calc_client = R("CalcClientArea", "L")

    def calc_client_area(e):
        calc_client(e)
        this = e.r(3)
        e.write(this + 0x2C, bytes(e.read(this + 8, 8)))   # (the real one copies the rectangle to the client rectangle)

    S.internal(CALC_CLIENT_AREA, calc_client_area)
    S.internal(WHAT_AM_I, R("WhatAmI", "L", ret=lambda e, L: us(e).kind_of(e.r(3))))
    S.internal(GET_WINDOW, R("GetWindow", "Li", ret=lambda e, L: e.r(3) if L.ret("GetWindow") != 0 else 0))
    S.internal(FIND_DRAW_OBJECT, R("FindDrawObject", "u", ret=lambda e, L: find_draw(e, L)))
    S.internal(CIRCLE_DTOR, R("CoolCircle.dtor", "D", then=count_delete))
    S.internal(ELLIPSE_DTOR, R("CoolEllipse.dtor", "D", then=count_delete))
    S.internal(POLY_DTOR, R("CoolPoly.dtor", "D", then=count_delete))
    S.imp("memset", lambda e: e.write(e.r(3), bytes([e.r(4) & 0xFF]) * e.r(5)) if e.r(5) else None)
    return S


def find_draw(e, L):
    index = L.ret("FindDrawObject", -1)
    return e.us.draws[index] if 0 <= index < DRAWS else 0


class CallbackHook:
    """The fake factory as a callback stub on an in-binary function (the stubs of CL.Stubs cannot call PowerPC code back)."""

    def __init__(self, e):
        self.e = e
        e.internal_stubs = getattr(e, "internal_stubs", {})
        e.internal_stubs.pop(CREATE_OBJECT, None)

        def hook(uc, address, size, user):
            e.pending_cb = create_object
            uc.reg_write(UP.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)

        self.handle = e.uc.hook_add(UC_HOOK_CODE, hook, begin=CREATE_OBJECT, end=CREATE_OBJECT)

    def remove(self):
        self.e.uc.hook_del(self.handle)


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uis_open", False):
        return
    import ctypes as C
    lib.tpw_uis_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_uis_script.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int16)]
    lib.tpw_uis_font_id.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_uis_region.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uis_dump_window.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uis_dump_window.restype = C.c_int32
    lib.tpw_uis_created.restype = C.c_int32
    lib.tpw_uis_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uis_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._uis_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        open_host(lib)
        lib.tpw_uis_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues)
        lib.tpw_uis_labels()
        for k, font in enumerate(case.get("fonts", FONT_IDS)):
            lib.tpw_uis_font_id(k, font)
        for k, script in case.get("scripts", {}).items():
            array = (C.c_int16 * max(1, len(script)))(*[s16(v) for v in script])
            lib.tpw_uis_script(k, len(script), array)

    def op(self, code, a):
        C = self.C
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 5)()
        before = self.lib.tpw_uis_created()
        self.lib.tpw_uis_op(code, args, out)
        if code >= 20:
            out[2] = self.lib.tpw_uis_created() - before   # (the windows the operation made)
        return list(out)

    def dump_state(self, step):
        C, lib = self.C, self.lib
        buf = (C.c_int64 * 4096)()
        windows = []
        for slot in range(SLOTS):
            if step["made"][slot]:
                n = lib.tpw_uis_dump_window(slot, buf)
                windows.append(list(buf[:n]))
            else:
                windows.append([0])
        for k in range(lib.tpw_uis_created()):
            n = lib.tpw_uis_dump_window(SLOTS + k, buf)
            windows.append(list(buf[:n]))
        lib.tpw_uis_dump_globals(buf)
        globals_ = list(buf[:2 + 5 * DRAWS])
        lib.tpw_uis_heap(buf)
        return dict(windows=windows, globals=globals_, heap=list(buf[:4]), log=C.c_int32.in_dll(lib, "tpw_cl_n").value)


# ======================================================================= running a script

RAN = {}   # what the PowerPC side decided to run, for the host side (by case)
STUBS = None


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        hook = CallbackHook(e)
        try:
            return fn(e, case)
        finally:
            hook.remove()
            CL.Stubs.deactivate(e, saved)
    return run


@with_stubs
def ui_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for op in case["ops"]:
            code, a = op
            answer = world.op(code, a)
            if answer is None:
                continue
            ran.append(dict(op=op, made=list(world.made)))
            steps.append(dict(op=code, answer=answer, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def ui_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for step in RAN.pop(id(case)):
            code, a = step["op"]
            answer = host.op(code, a)
            steps.append(dict(op=code, answer=answer, state=host.dump_state(step)))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)
    REAL_SCRIPT[:] = real_script(e)
    LOBBY_SCRIPT[:] = list(struct.unpack(">38h", e.read(0x1029E382, 76)))


# ======================================================================= building blocks

TESTS = []

SMALL = [0, 0, 1, -1, 2, -3, 7, -8, 20, -33, 100, -250, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x12345, -0x12345]


def lng(value):
    """A 32-bit value as the script keeps it: the low half and then the high half."""
    value &= MINUS
    return [value & 0xFFFF, value >> 16]


def link_ops(parents):
    """Operations that link the windows as the parents list says: children in the order of their slots."""
    ops = []
    for slot in range(len(parents)):
        siblings = [t for t in range(len(parents)) if parents[t] == parents[slot]]
        at = siblings.index(slot)
        previous = siblings[at - 1] if at > 0 else -1
        following = siblings[at + 1] if at + 1 < len(siblings) else -1
        children = [t for t in range(len(parents)) if parents[t] == slot]
        ops.append((2, (slot, parents[slot], previous, following, children[0] if children else -1)))
    return ops


WORLD_PARENTS = [-1, 0, 0, 0, 0, 0, 0, 0]
WORLD_RECTS = [(0, 0, 400, 300), (10, 10, 60, 100), (70, 10, 160, 100), (170, 10, 260, 100), (270, 10, 330, 100),
               (10, 110, 60, 200), (70, 110, 160, 200), (170, 110, 260, 200)]


def world(attributes=None, rects=None, regions=None, fonts=None, depth=None, parents=None):
    """The eight windows with their kinds, linked (all but the first are children of the first), with their fields."""
    attributes = attributes or [1] * SLOTS
    rects = rects or WORLD_RECTS
    regions = regions or [0] * SLOTS
    fonts = fonts or [-1] * SLOTS
    parents = parents or WORLD_PARENTS
    ops = [(1, (s, parents[s], attributes[s], 100 + s) + tuple(rects[s]) + (WORLD_KINDS[s],)) for s in range(SLOTS)]
    ops += link_ops(parents)
    for s in range(SLOTS):
        z = depth[s] if depth else 1 + 2 * (s > 0)
        ops.append((3, (s, attributes[s], regions[s], fonts[s], z, 3 * s, 20 + s, 30 + s)))
    return ops


def case_of(ops, q=None, scripts=None, fonts=None):
    case = dict(ops=ops)
    if q:
        case["q"] = q
    if scripts:
        case["scripts"] = scripts
    if fonts:
        case["fonts"] = fonts
    return case


def run_script(slot, k=0, start=0):
    return (20, (slot, k, start))


# ======================================================================= the records


def create(kind, attributes=1, ident=0, rect=(0, 0, 10, 10), body=(), tail=(R_END,)):
    """Record 0: a child and, up to the tail that ends them, the records about it."""
    return [R_CREATE, kind] + lng(attributes) + lng(ident) + list(rect) + list(body) + list(tail)


def draw(hash_value):
    return [R_DRAW] + lng(hash_value)


def client_draw(hash_value):
    return [R_CLIENTDRAW] + lng(hash_value)


def client_rect(left, top, right, bottom):
    return [R_CLIENTRECT, left, top, right, bottom]


def region(shape, *numbers):
    return [R_REGION, shape] + list(numbers)


def polygon(points):
    return region(4, len(points), *[v for point in points for v in point])


def child(record, rect=(0, 0, 10, 10), body=(), tail=(R_END,)):
    """A record that makes a child from a rectangle alone (6, 7, 8, 9, 14, 15) and the records about it."""
    return [record] + list(rect) + list(body) + list(tail)


def header(column, rect=(0, 0, 10, 10), body=(), tail=(R_END,)):
    return [R_HEADER, column] + list(rect) + list(body) + list(tail)


def light(number, rect=(0, 0, 10, 10), body=(), tail=(R_END,)):
    return [R_LIGHT, number] + list(rect) + list(body) + list(tail)


def columns(*pairs):
    return [R_COLUMNS, len(pairs)] + [v for pair in pairs for v in pair]


def list_rect(left, top, right, bottom):
    return [R_LISTRECT, left, top, right, bottom]


def knob(minimum, maximum):
    return [R_KNOB, minimum, maximum]


def help_text(value):
    return [R_HELP] + lng(value)


def f90(value):
    return [R_F90] + lng(value)


def script(*records, tail=(R_END,)):
    """A whole script: the records and what ends the top level."""
    return [v for record in records for v in record] + list(tail)


REAL_SCRIPT = []   # filled by setup
LOBBY_SCRIPT = []  # the lobby's own window description, filled by setup


def real_script(e):
    """The lobby's login panel script (the game's data at 0x102abbb0, 371 words), read from the original's image."""
    return list(struct.unpack(">371h", e.read(0x102ABBB0, 742)))


# ======================================================================= test 1: the interpreter

# the hashes the real script uses, for the draw objects
HASHES = [0x1600292, 0x79490FDE, 0xEE32BE94, 0xEC9FFD8A, 0, MINUS, 0x80000000]


def with_children(parents, ids):
    """A world whose windows are linked as `parents` says and have these ids (the op that makes the window takes the id)."""
    ops = world(parents=parents)
    return [(1, a[:3] + (ids[a[0]],) + a[4:]) if code == 1 else (code, a) for code, a in ops]


def wrong_kind_records(rect):
    """(record words, the kinds of window it is for) for the records that are about one kind of window."""
    return [(child(R_UP, rect), (SCROLLBAR,)), (child(R_DOWN, rect), (SCROLLBAR,)), (child(R_THUMB, rect), (SCROLLBAR,)),
            (child(R_LISTSCROLL, rect), (LISTBOX, LISTCONTROL)), (list_rect(*rect), (LISTBOX, LISTCONTROL)),
            (columns((1, 2), (3, 4)), (LISTCONTROL,)), (header(1, rect), (LISTCONTROL,)), (knob(5, 6), (KNOB,)),
            (child(R_PLUS, rect), (VARYBOX,)), (child(R_MINUS, rect), (VARYBOX,)), (light(3, rect), (LIGHTPANEL,))]


def interpreter_edges():
    rect = (3, -4, 50, 60)
    # every record once on the window it is for (the queue answers: no window has the child yet)
    on = {SCROLLBAR: [child(R_UP, rect), child(R_DOWN, rect), child(R_THUMB, rect)],
          LISTBOX: [child(R_LISTSCROLL, rect), list_rect(*rect)],
          LISTCONTROL: [child(R_LISTSCROLL, rect), list_rect(*rect), columns((1, 2), (3, 4), (5, 6)), header(0, rect), header(2, rect)],
          KNOB: [knob(-90, 270)],
          VARYBOX: [child(R_PLUS, rect), child(R_MINUS, rect)],
          LIGHTPANEL: [light(0, rect), light(31, rect), light(7, rect)],
          PLAIN: [create(1, 1, 5, rect), draw(0x1600292), client_draw(0x79490FDE), client_rect(1, 2, 3, 4), region(1, 1, 2, 3, 4),
                  help_text(0x12345678), f90(-5)]}
    for slot, records in on.items():
        for record in records:
            yield case_of(world() + [run_script(slot)], scripts={0: script(record)}, q={"FindDrawObject": [0, 1, -1]})
        yield case_of(world() + [run_script(slot)], scripts={0: script(*records)}, q={"FindDrawObject": [2, 3]})
    # a record for the wrong kind of window: the assertion and then the record goes on (but not the knob's on a list: it would
    # write over the list's rectangle, which the two sides lay out differently)
    for words_, slots in wrong_kind_records(rect):
        for slot in range(SLOTS):
            if slot in slots or (words_[0] == R_KNOB and WORLD_KINDS[slot] in (4, 7)):
                continue
            yield case_of(world() + [run_script(slot)], scripts={0: script(words_)})
    # the queue says the window has the child already: the assertion, then the record goes on
    for record, kind_slot in ((child(R_UP, rect), SCROLLBAR), (child(R_DOWN, rect), SCROLLBAR), (child(R_THUMB, rect), SCROLLBAR),
                              (child(R_PLUS, rect), VARYBOX), (child(R_MINUS, rect), VARYBOX), (light(5, rect), LIGHTPANEL)):
        for answers in ([1], [0, 1], [1, 0]):
            yield case_of(world() + [run_script(kind_slot)], scripts={0: script(record, record)}, q={"GetWindow": answers})
    # the scroll bar without buttons (attribute 0x40), with and without a child already there
    for attributes in (0x40, 0x41, 0x21, 0x60):
        for record in (R_UP, R_DOWN):
            yield case_of(world(attributes=[1, attributes] + [1] * 6) + [run_script(SCROLLBAR)], scripts={0: script(child(record, rect))})
            yield case_of(world(attributes=[1, attributes] + [1] * 6) + [run_script(SCROLLBAR)], scripts={0: script(child(record, rect))},
                          q={"GetWindow": [1]})
    # the light number: out of range, at the ends, with and without a light already
    for number in (-1, 0, 1, 31, 32, 33, 0x7FFF, -0x8000, 0xFF):
        for answers in ([0], [1]):
            yield case_of(world() + [run_script(LIGHTPANEL)], scripts={0: script(light(number, rect))}, q={"GetWindow": answers})
    # the scroll bar a list gets, when the list has a child of id 1 (first, second or in the middle) and when it has others
    for parent_slot, kind in ((LISTBOX, 4), (LISTCONTROL, 7)):
        for ids in ([1], [5, 1], [1, 5], [5, 6, 1, 7], [5, 6], [0], [-1], [0x10001]):
            parents = WORLD_PARENTS[:]
            for k, child_id in enumerate(ids):
                parents[1 + k] = parent_slot if 1 + k != parent_slot else 0
            all_ids = [100 + s for s in range(SLOTS)]
            slots = [t for t in range(1, SLOTS) if parents[t] == parent_slot]
            for t, child_id in zip(slots, ids):
                all_ids[t] = child_id
            if parent_slot in slots:
                continue
            yield case_of(with_children(parents, all_ids) + [run_script(parent_slot)],
                          scripts={0: script(child(R_LISTSCROLL, rect), child(R_LISTSCROLL, rect))})
    # the end of a level: 5, a word that is not a record (above 0x12, negative), nested and at the top
    for tail in (19, 20, 0x7FFF, -1, -0x8000, 0x13, 0x80, -19, 0x100):
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(create(1, 1, 7, rect, tail=(tail,)), help_text(9), tail=(tail,))})
        yield case_of(world() + [run_script(PLAIN)], scripts={0: [tail, R_HELP, 1, 2]})
        yield case_of(world() + [run_script(PLAIN)], scripts={0: [R_END, R_HELP, 1, 2]})
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(create(1, 1, 7, rect, tail=(R_END,)), create(2, 1, 8, rect, tail=(tail,)),
                                                                       help_text(4), tail=(R_END,))})
    # kinds, attributes, ids and rectangles of the children at their edges
    for kind in (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19, 0x7FFF, -1, -0x8000):
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(create(kind, 1, 3, rect))})
    for attributes in (0, 1, 2, 0x400, 0x401, 0x7FFFFFFF, -1, T.s32(0x80000000), 0x10000, 0x12345678):
        for ident in (0, 1, -1, 0x7FFFFFFF, T.s32(0x80000000), 0x12345678, 0x11100005):
            yield case_of(world() + [run_script(PLAIN)], scripts={0: script(create(2, attributes, ident, (1, 2, 3, 4)))})
    for rect_ in ((0, 0, 0, 0), (-1, -1, -1, -1), (0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF), (-0x8000, -0x8000, -0x8000, -0x8000),
                  (0x8000, 0xFFFF, 0x10000, 0x12345), (5, 5, 1, 1)):
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(create(1, 0x401, 3, rect_), client_rect(*rect_))})
        for record in (child(R_UP, rect_), child(R_DOWN, rect_), child(R_THUMB, rect_)):
            yield case_of(world() + [run_script(SCROLLBAR)], scripts={0: script(record)})
        yield case_of(world() + [run_script(LISTBOX)], scripts={0: script(child(R_LISTSCROLL, rect_), list_rect(*rect_))})
        yield case_of(world() + [run_script(LISTCONTROL)], scripts={0: script(list_rect(*rect_), header(0, rect_), header(-5, rect_),
                                                                              header(0x7FFF, rect_), header(-0x8000, rect_))})
        yield case_of(world() + [run_script(VARYBOX)], scripts={0: script(child(R_PLUS, rect_), child(R_MINUS, rect_))})
        yield case_of(world() + [run_script(LIGHTPANEL)], scripts={0: script(light(0, rect_))})
    # the client rectangle: made absolute by the attribute 0x400, through a chain of parents
    chain = [-1, 0, 1, 2, 3, 4, 5, 6]
    for attributes in (1, 0x401, 0x400, 0x403):
        for rect_ in ((1, 2, 3, 4), (-5, 7, 100, 200), (0x7FFF, 0x7FFF, 1, 1), (-0x8000, -0x8000, 0, 0)):
            ops = world(attributes=[1] * 7 + [attributes], parents=chain)
            yield case_of(ops + [run_script(7), run_script(3)], scripts={0: script(client_rect(*rect_), create(1, 0x400, 4, rect), client_rect(*rect_))})
    # the regions: every shape number, the numbers at their edges, a region replacing each kind of region
    shapes = [region(0), region(1, 1, 2, 3, 4), region(2, 5, 6, 7), region(3, 8, 9, 10, 11), polygon([]), polygon([(1, 2)]),
              polygon([(1, 2), (3, 4), (5, 6)]), polygon([(-1, -2), (0x7FFF, -0x8000)] * 4), region(5), region(6), region(-1),
              region(0x7FFF), region(-0x8000), region(1, -1, -1, -1, -1), region(1, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF),
              region(2, -0x8000, 0x7FFF, 0), region(3, 0, 0, 0, 0)]
    for shape in shapes:
        for existing in (0, 1, 2, -1):   # the window's own rectangle, a fake region, none
            ops = world(regions=[existing] * SLOTS)
            yield case_of(ops + [run_script(PLAIN)], scripts={0: script(shape)})
    # the region that could not be made (new answers NULL): the old region goes, the window's own rectangle is the region
    for shape in (region(1, 1, 2, 3, 4), region(2, 5, 6, 7), region(3, 8, 9, 10, 11), polygon([(1, 2), (3, 4)]), polygon([]), region(9)):
        for existing in (0, 1, -1):
            yield case_of(world(regions=[existing] * SLOTS) + [run_script(PLAIN)], scripts={0: script(shape)}, q={"NewFails": [1]})
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(region(1, 1, 2, 3, 4), shape, region(2, 5, 6, 7))}, q={"NewFails": [0, 1, 0]})
    # a region that replaces the one the script made before (each kind of shape replaced by each kind of shape)
    for first in (region(1, 1, 2, 3, 4), region(2, 5, 6, 7), region(3, 8, 9, 10, 11), polygon([(1, 2), (3, 4)]), polygon([])):
        for second in (region(1, 4, 3, 2, 1), region(2, 7, 6, 5), region(3, 11, 10, 9, 8), polygon([(5, 6), (7, 8), (9, 10)]), region(9)):
            yield case_of(world() + [run_script(PLAIN)], scripts={0: script(first, second, second)})
    yield case_of(world() + [run_script(PLAIN)], scripts={0: script(*shapes)})
    # the knob
    for minimum, maximum in ((0, 0), (-90, 270), (270, -90), (5, 5), (T.s32(0x80000000), 0x7FFFFFFF), (0x7FFF, -0x8000), (-0x8000, 0x7FFF),
                             (1, 0), (0, -1)):
        yield case_of(world() + [run_script(KNOB)], scripts={0: script(knob(minimum, maximum), knob(maximum, minimum))})
    # the columns: counts, positions at their edges, a second call that finds columns already
    for count in (0, 1, 2, 3, 7, 8, 9, 16):
        pairs = [(i * 3 - 5, i * 7 + 1) for i in range(count)]
        yield case_of(world() + [run_script(LISTCONTROL)], scripts={0: script(columns(*pairs))})
        yield case_of(world() + [run_script(LISTCONTROL)], scripts={0: script(columns(*pairs), columns((9, 9), (8, 8)))})
    yield case_of(world() + [run_script(LISTCONTROL)], scripts={0: script(columns((-0x8000, 0x7FFF), (0x7FFF, -0x8000), (0, 0)))})
    # draw objects: the hash is read from two words and the answer of the library is a draw object or none
    for value in HASHES + [0x10000, 0xFFFF, 1, -1]:
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(draw(value), client_draw(value))}, q={"FindDrawObject": [0, -1]})
    yield case_of(world() + [run_script(PLAIN)], scripts={0: script(draw(1), draw(2), draw(3), client_draw(4), client_draw(5), client_draw(6))},
                  q={"FindDrawObject": [0, 1, 2, 3, 4, 5]})
    yield case_of(world() + [run_script(PLAIN)], scripts={0: script(draw(1), draw(1), client_draw(2), client_draw(2))}, q={"FindDrawObject": [3]})
    # help text numbers and the other word
    for value in (0, 1, -1, 0x7FFFFFFF, T.s32(0x80000000), 0x12345678, 0xFFFF, 0x10000):
        yield case_of(world() + [run_script(PLAIN)], scripts={0: script(help_text(value), f90(value))})
    # nested children and many records; a nested level that is a chain; siblings
    chain_words = []
    for depth in range(8, 0, -1):
        chain_words = create(2, 1, depth, rect, body=help_text(depth) + chain_words)
    yield case_of(world() + [run_script(PLAIN)], scripts={0: script(chain_words)})
    yield case_of(world() + [run_script(PLAIN)], scripts={0: script(*[create(1, 1, i, rect, body=help_text(i)) for i in range(12)])})
    # a script of one record and the ends
    yield case_of(world() + [run_script(PLAIN)], scripts={0: [R_END]})
    yield case_of(world() + [run_script(PLAIN)], scripts={0: [R_HELP, 7, 0, R_END]})
    # start positions
    base = script(create(1, 1, 1, rect), help_text(5), create(2, 1, 2, rect))
    for start in (0, 1, 2, 11, len(base) - 1, len(base)):
        yield case_of(world() + [run_script(PLAIN, 0, start)], scripts={0: base + [R_END]})
    # Build: the callback goes to the last window of the top level
    for callback in (0, 1, 2, 3):
        yield case_of(world() + [(21, (PLAIN, 0, callback))], scripts={0: script(create(1, 1, 1, rect), create(2, 1, 2, rect))})
        yield case_of(world() + [(21, (PLAIN, 0, callback))], scripts={0: [R_END]})
        yield case_of(world() + [(21, (PLAIN, 0, callback))], scripts={0: script(help_text(3))})
        yield case_of(world() + [(21, (PLAIN, 0, callback))], scripts={0: script(create(1, 1, 1, rect), create(2, 1, 2, rect))},
                      q={"CreateObject": [1, 0]})
    yield case_of(world() + [(7, (2, 77)), (21, (PLAIN, 0, 1))], scripts={0: script(create(1, 1, 1, rect))})
    # the script's window is none (the records only make windows)
    yield case_of(world() + [(20, (-1, 0, 0))], scripts={0: script(create(1, 1, 1, rect), create(2, 1, 2, rect, body=create(3, 1, 3, rect)))})
    yield case_of(world() + [(21, (-1, 0, 2))], scripts={0: script(create(1, 1, 1, rect))})
    # a window the factory could not make: an assertion, and the records of the new window are about none (they end at once)
    for failing in ([1], [0, 1], [1, 0, 1], [1, 1, 1]):
        yield case_of(world() + [run_script(PLAIN)], q={"CreateObject": failing},
                      scripts={0: script(create(1, 1, 1, rect), create(2, 1, 2, rect), create(3, 1, 3, rect))})
    for record, slot in ((child(R_UP, rect), SCROLLBAR), (child(R_DOWN, rect), SCROLLBAR), (child(R_LISTSCROLL, rect), LISTBOX),
                         (child(R_LISTSCROLL, rect), LISTCONTROL), (header(1, rect), LISTCONTROL), (child(R_PLUS, rect), VARYBOX),
                         (child(R_MINUS, rect), VARYBOX), (light(3, rect), LIGHTPANEL)):
        yield case_of(world() + [run_script(slot)], q={"CreateObject": [1, 0]}, scripts={0: script(record, record)})
    # the real login panel of the lobby
    yield case_of(world() + [run_script(PLAIN)], scripts={0: REAL_SCRIPT}, q={"FindDrawObject": [0, 1, 2, -1, 3, 4, 5]})
    yield case_of(world() + [(21, (PLAIN, 0, 1))], scripts={0: REAL_SCRIPT}, q={"FindDrawObject": [0, 1, 2, 3]})
    yield case_of(world(fonts=[0] * SLOTS) + [(21, (PLAIN, 0, 2))], scripts={0: REAL_SCRIPT}, q={"FindDrawObject": [1, 2, 3, 4, 5, 0]})
    # the lobby's own window (38 words: the panel and the mail icon with its draw object)
    yield case_of(world() + [run_script(PLAIN)], scripts={0: LOBBY_SCRIPT}, q={"FindDrawObject": [3]})
    yield case_of(world() + [(21, (PLAIN, 0, 1))], scripts={0: LOBBY_SCRIPT}, q={"FindDrawObject": [-1]})
    yield case_of(world(fonts=[1] * SLOTS) + [(21, (-1, 0, 2))], scripts={0: LOBBY_SCRIPT}, q={"FindDrawObject": [0]})


# ----------------------------------------------------------------- random scripts

CREATE_KINDS = [1, 1, 1, 2, 2, 3, 4, 7, 0xB, 0xC, 0xD, 0, 5, 6, 8, 9, 10, 0xE, 0xF, 0x12, 0x13, 0x7FFF, -1]
ENDINGS = [19, 0x13, -1, 0x7FFF, -0x8000, 0x100]
ATTRIBUTES = [0, 1, 1, 1, 0x401, 0x400, 2, 0x21, 0x10, 0x40, 0x41, 0x7FFFFFFF, -1]
IDS = [0, 1, 1, 2, 3, 5, 0x10, 0x11, -1, 0x7FFFFFFF, 100, 31251]


class Gen:
    """A random script that the original reads to its end: every record is followed by the words the original takes for it (which
    depends on the kind of window it is about and on whether that window has a child with id 1), a level ends with 5 or with a word
    that is not a record, and the factory's failures (only in the first sixty calls, and never for a window whose records use it)
    come with the answers of the queues, in the order the calls happen."""

    def __init__(self, r, budget):
        self.r = r
        self.budget = budget
        self.creates, self.windows, self.finds = [], [], []

    def rect(self):
        r = self.r
        return tuple(r.choice(SMALL + [r.randint(-300, 300)]) for _ in range(4))

    def make(self, kind, attributes, ident, rect, record, can_fail=True, depth=0, endings=True):
        """The factory call a record makes and the records about the new window. Answers (the body words, the ending)."""
        r = self.r
        fail = can_fail and len(self.creates) < 60 and r.random() < 0.04
        self.creates.append(1 if fail else 0)
        if fail:
            return [], [R_END], None
        body, ids = self.level(kind, depth + 1) if depth < 3 and r.random() < 0.6 else ([], [])
        tail = [r.choice(ENDINGS)] if endings and r.random() < 0.05 else [R_END]
        return body, tail, ids

    def level(self, kind, depth):
        r = self.r
        out, child_ids = [], []
        for _ in range(r.randint(1, 7) if depth else r.randint(4, 40)):
            if self.budget <= 0:
                break
            self.budget -= 1
            out += self.record(kind, depth, child_ids)
        return out, child_ids

    def child_record(self, record, kind_window, expect, ident, child_kind, depth, child_ids, rect_first=()):
        """A record that makes a child from four words: the GetWindow call when the window is the right kind, the child."""
        r = self.r
        if kind_window == expect and record != R_LISTSCROLL:
            self.windows.append(1 if r.random() < 0.1 else 0)
        rect = self.rect()
        body, tail, ids = self.make(child_kind, 1, ident, rect, record, can_fail=record != R_THUMB, depth=depth)
        if ids is not None:
            child_ids.append(ident)
        return [record] + list(rect_first) + list(rect) + body + tail

    def record(self, kind, depth, child_ids):
        r = self.r
        weights = [("create", 5), ("draw", 1), ("cdraw", 1), ("crect", 1), ("region", 2), ("help", 1), ("f90", 1)]
        specific = {3: ["up", "down", "thumb"], 4: ["lscroll", "lrect"], 7: ["lscroll", "lrect", "cols", "header"], 0xB: ["knob"],
                    0xC: ["plus", "minus"], 0xD: ["light"]}
        weights += [(name, 3) for name in specific.get(kind, [])]
        if r.random() < 0.1:   # a record for the wrong kind of window (the knob's not on a list: see the edges)
            every = ["up", "down", "thumb", "lscroll", "lrect", "cols", "header", "plus", "minus", "light"]
            if kind not in (4, 7):
                every.append("knob")
            weights.append((r.choice(every), 30))
        total = sum(w for _, w in weights)
        pick, name = r.uniform(0, total), None
        for name, weight in weights:
            pick -= weight
            if pick <= 0:
                break
        if name == "create":
            child_kind = r.choice(CREATE_KINDS)
            ident = r.choice(IDS)
            rect = self.rect()
            body, tail, ids = self.make(child_kind, r.choice(ATTRIBUTES), ident, rect, R_CREATE, depth=depth)
            if ids is not None:
                child_ids.append(ident)
            return [R_CREATE, child_kind] + lng(r.choice(ATTRIBUTES)) + lng(ident) + list(rect) + body + tail
        if name in ("draw", "cdraw"):
            self.finds.append(r.choice([-1, 0, 1, 2, 3, 4, 5, 0, 1]))
            return (draw if name == "draw" else client_draw)(r.choice(HASHES))
        if name == "crect":
            return client_rect(*self.rect())
        if name == "region":
            shape = r.choice([0, 1, 2, 3, 4, 4, 5, 0x7FFF, -1])
            numbers = {1: 4, 2: 3, 3: 4}.get(shape, 0)
            if shape == 4:
                return polygon([(r.choice(SMALL), r.choice(SMALL)) for _ in range(r.choice([0, 1, 2, 3, 6]))])
            return region(shape, *[r.choice(SMALL) for _ in range(numbers)])
        if name == "help":
            return help_text(r.choice(SMALL + [0x12345678]))
        if name == "f90":
            return f90(r.choice(SMALL + [0x12345678]))
        if name == "up":
            return self.child_record(R_UP, kind, 3, 1, 2, depth, child_ids)
        if name == "down":
            return self.child_record(R_DOWN, kind, 3, 2, 2, depth, child_ids)
        if name == "thumb":
            return self.child_record(R_THUMB, kind, 3, 3, 2, depth, child_ids)
        if name == "plus":
            return self.child_record(R_PLUS, kind, 0xC, 1, 2, depth, child_ids)
        if name == "minus":
            return self.child_record(R_MINUS, kind, 0xC, 2, 2, depth, child_ids)
        if name == "lscroll":
            if kind in (4, 7) and 1 not in child_ids:
                return self.child_record(R_LISTSCROLL, kind, 0, 1, 3, depth, child_ids)
            return [R_LISTSCROLL]
        if name == "lrect":
            return list_rect(*self.rect()) if kind in (4, 7) else [R_LISTRECT]
        if name == "cols":
            count = r.choice([0, 1, 2, 3, 5, 8])
            return columns(*[(r.choice(SMALL), r.choice(SMALL)) for _ in range(count)])
        if name == "header":
            number = r.choice([0, 1, 2, 5, -1, 0x7FFF])
            rect = self.rect()
            ident = number + 0x10
            body, tail, ids = self.make(2, 1, ident, rect, R_HEADER, depth=depth)
            if ids is not None:
                child_ids.append(ident)
            return [R_HEADER, number] + list(rect) + body + tail
        if name == "knob":
            return knob(r.choice(SMALL), r.choice(SMALL))
        if name == "light":
            number = r.choice([0, 1, 5, 31, 32, -1, 0x7FFF, 3, 4])
            rect = self.rect()
            if 0 <= number < 32:
                self.windows.append(1 if r.random() < 0.1 else 0)
            body, tail, ids = self.make(6, 1, number | 0x11100000, rect, R_LIGHT, depth=depth)
            if ids is not None:
                child_ids.append(T.s32(number | 0x11100000))
            return [R_LIGHT, number] + list(rect) + body + tail
        raise ValueError(name)

    def queues(self):
        q = {}
        if self.creates:
            q["CreateObject"] = (self.creates + [0] * MAX_QUEUE)[:MAX_QUEUE]
        if self.windows:
            q["GetWindow"] = self.windows[:MAX_QUEUE]
        if self.finds:
            q["FindDrawObject"] = self.finds[:MAX_QUEUE]
        return q


def random_world(r):
    """A world with random attributes, fonts, depths, regions and draw objects; the ops that make it."""
    attributes = [r.choice([1, 1, 1, 0, 0x401, 0x400, 0x21, 3]) for _ in range(SLOTS)]
    ops = world(attributes=attributes, regions=[r.choice([0, 0, 1, 2, -1]) for _ in range(SLOTS)],
                fonts=[r.choice([-1, -1, 0, 1, 2]) for _ in range(SLOTS)], depth=[r.choice([0, 1, 3, 5, 0x7FFF, -1]) for _ in range(SLOTS)])
    for slot in range(SLOTS):
        if r.random() < 0.4:
            ops.append((4, (slot, r.choice([-1, 0, 1, 2, 3]), r.choice([-1, 0, 1, 2, 3]))))
    for index in range(DRAWS):
        ops.append((5, (index, r.choice([0, 0, 1]), r.choice([-1, 0, 0, 1, 2]), r.choice([0, 1]))))
    return ops


def interpreter_random(r):
    gen = Gen(r, r.choice([6, 20, 60, 200, 200]))
    slot = r.randrange(SLOTS)
    words_, _ = gen.level(WORLD_KINDS[slot], 0)
    if not words_:
        words_ = help_text(1)
    ops = random_world(r)
    ops.append((21, (slot, 0, r.choice([0, 1, 2, 3]))) if r.random() < 0.3 else run_script(slot))
    if r.random() < 0.3:
        ops.append((22 if r.random() < 0.5 else 23, (r.randrange(SLOTS), r.choice([-1, 0, 1, 2, 3, 4, 5]))))
    return case_of(ops, q=gen.queues(), scripts={0: words_ + [R_END]})


def interpreter_cases():
    yield from interpreter_edges()
    r = random.Random(7000)
    for _ in range(6):
        gen = Gen(r, 200)
        slot = r.randrange(SLOTS)
        words_, _ = gen.level(WORLD_KINDS[slot], 0)
        yield case_of(random_world(r) + [run_script(slot)], q=gen.queues(), scripts={0: words_ + [R_END]})


TESTS.append(FnTest("ProcessScript, Build", ui_ppc, ui_host, edge=interpreter_cases, rnd=interpreter_random))


# ======================================================================= test 2: the draw objects, the visibility, the keyboard


def draw_ops(slot, main, client):
    return [(4, (slot, main, client))]


def draw_edges():
    # SetDrawObject and SetClientDrawObject over the old object (none, or one with each user count and library flag), the new one
    # (none, the same, another), the window's font and visibility. Window 1 is the one the object is for; draw 2 is its main
    # object when the client's is set (the quirk: that one is deleted).
    for func in (22, 23):
        for old in (-1, 0):
            for count in (-1, 0, 1, 2):
                for in_library in (0, 1):
                    for new in (-1, 0, 1):
                        for main in ((-1,) if func == 22 else (-1, 2)):
                            for font, attribute in ((-1, 1), (1, 1), (2, 0)):
                                ops = world(fonts=[-1, font] + [-1] * 6, attributes=[1, attribute] + [1] * 6)
                                ops += draw_ops(1, old if func == 22 else main, -1 if func == 22 else old)
                                ops += [(5, (0, in_library, count, 1)), (5, (2, 0, 1, 1)), (func, (1, new))]
                                yield case_of(ops)
    # one object on two windows; each lets go
    for func in (22, 23):
        for in_library in (0, 1):
            for first_count in (0, 1):
                ops = world() + [(5, (0, in_library, 0, 1)), (func, (1, 0)), (func, (2, 0)), (5, (0, in_library, first_count, 1)),
                                 (func, (1, -1)), (func, (2, -1)), (func, (1, 0)), (func, (1, 1)), (func, (1, 0))]
                yield case_of(ops)
    # the quirk, then the window's own object is replaced: the dangling pointer is read and deleted again
    for count in (0, 1):
        for in_library in (0, 1):
            ops = world() + [(4, (1, 2, 1)), (5, (1, in_library, count, 1)), (5, (2, 0, 1, 1)), (23, (1, 3)), (22, (1, 4)), (22, (1, 3)),
                             (23, (1, 1)), (23, (1, -1))]
            yield case_of(ops)
    # the depth: the window's z and the client's z + 1 at the edges, the pivot, the rotation
    for z in (0, 1, -1, 0x7FFF, -0x8000, 0x100):
        for rotation in (0, 5, -7, 0x7FFF):
            ops = world() + [(3, (1, 1, 0, 0, z, rotation, 0x7FFF, -0x8000)), (22, (1, 0)), (23, (1, 1))]
            yield case_of(ops)
    # the font's id: only its low byte goes to the style
    for font_ids in ((0, 0, 0), (0x101, 0x1FF, 0x100), (0x80, 0x7F, 0xFF), (-1, T.s32(0x80000000), 0x7FFFFFFF)):
        for font in (0, 1, 2):
            ops = world(fonts=[-1, font] + [-1] * 6) + [(22, (1, 0)), (23, (1, 1))]
            yield case_of(ops, fonts=font_ids)
    # the window's rectangles are what the objects are told (a window made absolute)
    for attributes in (1, 0x401):
        ops = world(attributes=[1, attributes] + [1] * 6) + [(22, (1, 0)), (23, (1, 1)), (22, (2, 0)), (23, (2, 1))]
        yield case_of(ops)


def visibility_edges():
    chain = [-1, 0, 1, 2, 3, 4, 5, 6]
    for attributes in ([1] * 8, [0] * 8, [1, 1, 1, 0, 1, 1, 1, 1], [1, 1, 1, 1, 1, 1, 1, 0], [0, 1, 1, 1, 1, 1, 1, 1], [3] * 8,
                       [2, 3, 3, 3, 3, 3, 3, 3], [0x10, 0x11, 0x20, 1, 1, 1, 1, 1], [-1] * 8, [T.s32(0x80000000)] + [1] * 7):
        for parents in (chain, WORLD_PARENTS):
            ops = world(attributes=attributes, parents=parents) + [(24, (s,)) for s in range(SLOTS)]
            yield case_of(ops)
    yield case_of(world() + [(24, (3,)), (3, (0, 0, 0, -1, 1, 0, 0, 0)), (24, (3,)), (24, (0,))])


def callback_edges():
    for slot in range(SLOTS):
        for first in (0, 1, 2, 3):
            for second in (0, 1, 2, 3):
                yield case_of(world() + [(25, (slot, first)), (25, (slot, second))])
    yield case_of(world() + [(25, (s, 1 + s % 3)) for s in range(SLOTS)] + [(25, (s, 0)) for s in range(SLOTS)])


def keyboard_edges():
    for held in (-1, 0, 3):
        for slot in (0, 3, 5):
            for on in (0, 1, -1, 0x7FFFFFFF, T.s32(0x80000000), 256):
                yield case_of(world() + [(7, (1, held)), (26, (slot, on)), (26, (slot, on + 1)), (26, (3, 0)), (26, (3, 1)), (26, (slot, 0))])
    yield case_of(world() + [(26, (s, 1)) for s in range(SLOTS)] + [(26, (s, 0)) for s in range(SLOTS)])
    yield case_of(world() + [(26, (2, 0)), (26, (2, 0)), (26, (2, 1)), (26, (4, 1)), (26, (4, 1)), (26, (2, 1))], q={"PostMessage": [5, -1, 0]})


def small_edges():
    yield from draw_edges()
    yield from visibility_edges()
    yield from callback_edges()
    yield from keyboard_edges()


def small_random(r):
    ops = random_world(r)
    for _ in range(r.randint(2, 14)):
        k = r.random()
        slot = r.randrange(SLOTS)
        if k < 0.3:
            ops.append((22, (slot, r.choice([-1, 0, 1, 2, 3, 4, 5]))))
        elif k < 0.55:
            ops.append((23, (slot, r.choice([-1, 0, 1, 2, 3, 4, 5]))))
        elif k < 0.62:
            ops.append((24, (slot,)))
        elif k < 0.7:
            ops.append((25, (slot, r.choice([0, 1, 2, 3]))))
        elif k < 0.78:
            ops.append((26, (slot, r.choice([0, 1, -1, 7]))))
        elif k < 0.82:
            ops.append((7, (1, r.choice([-1, slot, r.randrange(SLOTS)]))))
        elif k < 0.9:
            ops.append((5, (r.randrange(DRAWS), r.choice([0, 0, 1]), r.choice([-1, 0, 0, 1, 2]), r.choice([0, 1]))))
        elif k < 0.95:
            ops.append((4, (slot, r.choice([-1, 0, 1, 2]), r.choice([-1, 0, 1, 2]))))
        else:
            ops.append((3, (slot, r.choice([0, 1, 1, 0x401, 0x21]), r.choice([0, 0, 1, 2]), r.choice([-1, 0, 1, 2]), r.choice(SMALL), r.choice(SMALL),
                            r.choice(SMALL), r.choice(SMALL))))
    return case_of(ops, q={"PostMessage": [r.choice([0, 1, -1])]})


TESTS.append(FnTest("SetDrawObject, SetClientDrawObject, GetVisibility, SetCallbackFunction, CaptureKb", ui_ppc, ui_host, edge=small_edges,
                    rnd=small_random))


# ======================================================================= test 3: the control setters

ANGLES = [0, 1, -1, 5, -90, 270, 0x7FFFFFFF, T.s32(0x80000000)]


def control_edges():
    # the knob: each pointer given or not, from each starting pair
    for has_min, has_max in ((1, 1), (1, 0), (0, 1), (0, 0)):
        for start in ((0, 0, 0), (10, 20, 10), (20, 10, -10), (0x7FFFFFFF, T.s32(0x80000000), 5), (-1, -1, 0)):
            for minimum in ANGLES:
                for maximum in ANGLES:
                    yield case_of(world() + [(6, (KNOB, 1) + start), (27, (KNOB, has_min, has_max, minimum, maximum))])
    yield case_of(world() + [(27, (KNOB, 1, 1, 5, 9)), (27, (KNOB, 1, 1, 9, 5)), (27, (KNOB, 0, 1, 1, 1)), (27, (KNOB, 1, 0, 100, 0))])
    # the rectangles of the lists
    for left, top, right, bottom in ((0, 0, 0, 0), (1, 2, 3, 4), (-1, -2, -3, -4), (0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF),
                                     (-0x8000, -0x8000, -0x8000, -0x8000), (0x8000, 0xFFFF, 0x10000, 0x12345), (5, 6, 1, 2)):
        yield case_of(world() + [(28, (LISTBOX, left, top, right, bottom)), (29, (LISTCONTROL, left, top, right, bottom)),
                                 (28, (LISTBOX, 9, 8, 7, 6)), (29, (LISTCONTROL, 9, 8, 7, 6))])
    # the columns: counts, a second call, the position of a column in and out of range, the column count set without columns
    for count in range(0, 9):
        yield case_of(world() + [(30, (LISTCONTROL, count))] + [(31, (LISTCONTROL, c, 10 * c + 1, 10 * c + 7)) for c in range(0, count + 2)])
        yield case_of(world() + [(30, (LISTCONTROL, count)), (30, (LISTCONTROL, 3)), (31, (LISTCONTROL, 1, -5, 5))])
    for count in (0, 1, 3, -1, 0x7FFF, -0x8000, 5):
        yield case_of(world() + [(6, (LISTCONTROL, 4, count)), (30, (LISTCONTROL, 4)), (31, (LISTCONTROL, 0, 1, 2)), (31, (LISTCONTROL, 2, 3, 4))])
    for left, right in ((0, 0), (-1, 1), (0x7FFF, -0x8000), (0x8000, 0xFFFF)):
        yield case_of(world() + [(30, (LISTCONTROL, 3)), (31, (LISTCONTROL, 0, left, right)), (31, (LISTCONTROL, 2, right, left)),
                                 (31, (LISTCONTROL, 0x7FFF, left, right)), (31, (LISTCONTROL, 3, left, right))])


def control_random(r):
    ops = world()
    if r.random() < 0.2:   # (a column count with no columns; set before any call makes the columns: it would be outside them)
        ops.append((6, (LISTCONTROL, 4, r.choice([0, 1, 3, -1, 0x7FFF]))))
    for _ in range(r.randint(2, 14)):
        k = r.random()
        if k < 0.25:
            ops.append((27, (KNOB, r.choice([0, 1, 1]), r.choice([0, 1, 1]), r.choice(ANGLES + [r.randint(-400, 400)]),
                             r.choice(ANGLES + [r.randint(-400, 400)]))))
        elif k < 0.4:
            ops.append((28, (LISTBOX,) + tuple(r.choice(SMALL) for _ in range(4))))
        elif k < 0.55:
            ops.append((29, (LISTCONTROL,) + tuple(r.choice(SMALL) for _ in range(4))))
        elif k < 0.7:
            ops.append((30, (LISTCONTROL, r.choice([0, 1, 2, 3, 5, 8]))))
        elif k < 0.9:
            ops.append((31, (LISTCONTROL, r.choice([0, 1, 2, 3, 4, 7, 8, 9, 0x7FFF]), r.choice(SMALL), r.choice(SMALL))))
        else:
            ops.append((6, (KNOB, 1, r.choice(ANGLES), r.choice(ANGLES), r.choice(ANGLES))))
    return case_of(ops)


TESTS.append(FnTest("InterfaceKnob::SetMinMaxAngles, SetListRect, SetColumns, SetColumnPos", ui_ppc, ui_host, edge=control_edges,
                    rnd=control_random))


# ======================================================================= test 4: the region constructors


class Counter:
    """What the stubs of the region test count into."""

    def __init__(self):
        self.heap = [0, 0, 0, 0]


@with_stubs
def region_ppc(e, case):
    counter = Counter()
    e.us = counter
    e.cl = CL.PpcLog({})
    kind, values = case["region"]
    points = e.alloc(4 * max(len(case.get("points", [])), 1) + 8, words(case.get("points", [])))
    block = e.alloc(0x40, b"\xa5" * 0x40)
    out = []
    if kind == 1:
        T.call_ppc(e, FN["Circle"], [block, s16(values[0]), s16(values[1]), s16(values[2])])
        out = [e.u32(block) == CIRCLE_VTABLE] + list(struct.unpack(">3h", e.read(block + 4, 6)))
    elif kind == 2:
        T.call_ppc(e, FN["Ellipse"], [block, s16(values[0]), s16(values[1]), s16(values[2]), s16(values[3])])
        out = [e.u32(block) == ELLIPSE_VTABLE] + list(struct.unpack(">4h", e.read(block + 4, 8)))
    else:
        T.call_ppc(e, FN["Poly"], [block, s16(values[0]), points])
        count = struct.unpack(">h", e.read(block + 4, 2))[0]
        pointer = e.u32(block + 8)
        out = [e.u32(block) == POLY_VTABLE, count, pointer != points]
        out += list(struct.unpack(">%dh" % (2 * count), e.read(pointer, 4 * count))) if count > 0 else []
    return dict(fields=[int(v) for v in out], heap=list(counter.heap))


def region_host(lib, case):
    import ctypes as C
    open_host(lib)
    lib.tpw_uis_region.restype = C.c_int32
    lib.tpw_uis_reset()
    CL.host_begin(lib, {}, {})
    script = case.get("points", [])
    array = (C.c_int16 * max(1, len(script)))(*[s16(v) for v in script])
    lib.tpw_uis_script(0, len(script), array)
    kind, values = case["region"]
    args = (C.c_int64 * 4)(*[s16(v) for v in values], *([0] * (4 - len(values))))
    out = (C.c_int64 * 4096)()
    n = lib.tpw_uis_region(kind, args, 0, out)
    CL.host_end(lib)
    buf = (C.c_int64 * 8)()
    lib.tpw_uis_heap(buf)
    return dict(fields=list(out[:n]), heap=list(buf[:4]))


def region_edges():
    for values in ((0, 0, 0), (1, 2, 3), (-1, -2, -3), (0x7FFF, -0x8000, 0x7FFF), (0x8000, 0xFFFF, 0x12345), (100, 200, 0)):
        yield dict(region=(1, values))
    for values in ((0, 0, 0, 0), (1, 2, 3, 4), (-1, -2, -3, -4), (0x7FFF, -0x8000, 0x7FFF, -0x8000), (0x8000, 0xFFFF, 0x12345, 0x10000)):
        yield dict(region=(2, values))
    for count in (0, 1, 2, 3, 8):
        for points in ([0] * (2 * count), [(-1) ** i * (i + 1) * 1000 for i in range(2 * count)], [0x7FFF, -0x8000] * count):
            yield dict(region=(3, (count,)), points=points)


def region_random(r):
    kind = r.choice([1, 2, 3])
    if kind == 3:
        count = r.randint(0, 12)
        return dict(region=(3, (count,)), points=[r.choice(SMALL + [r.randint(-1000, 1000)]) for _ in range(2 * count)])
    return dict(region=(kind, tuple(r.choice(SMALL) for _ in range(3 if kind == 1 else 4))))


TESTS.append(FnTest("CoolCircle, CoolEllipse and CoolPoly constructors", region_ppc, region_host, edge=region_edges, rnd=region_random))
