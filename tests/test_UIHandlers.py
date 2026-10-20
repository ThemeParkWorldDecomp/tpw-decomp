"""Differential tests for the default window handlers of five controls and their leaves, against libtpw_unit_UIHandlers.dylib
(tests/unit_UIHandlers.cpp):

  src/ui_handlers_small.cpp     UI_DefaultButtonCallback, UI_DefaultCheckButtonsCallback, UI_DefaultKnobCallback,
                                UI_DefaultTextBoxCallback, UI_DefaultVaryBoxCallback
  src/InterfaceWindow_focus.cpp InterfaceWindow: CheckFocus, CaptureDevice, UnCaptureDevice, cursorChange, GetID, GetParent
  src/InterfaceKnob.cpp         InterfaceKnob: DoKnobMove, ChangeAngle
  src/InterfaceVaryBox.cpp      InterfaceVaryBox: DoVaryBoxButton

Real on both sides: those, the button (src/InterfaceButton.cpp: the handler reaches CalcFrame, DoTextColour, SetState, HiLight,
StopHeldTimer and WhatAmI), the controls' constructors (src/ui_controls_create.cpp, which build the test windows), the window's
constructor (src/InterfaceWindow.cpp), GetWindow (src/InterfaceWindow_show.cpp) and the rectangles (src/UI_Base.cpp). Fakes that
record the same event on both sides: UI_PostMessage, UI_UpdatePointerIn, UI_SetCursor, the UI clock (IveGotTicks answers a clock the
script sets and moves on by a step at every call, so a missing or extra call changes every later answer), the window's default handler,
GetAttr and GetChild, the help text handler's Update, OnMouseMoved, OnMouseMovedIn and OnMouseMovedOut, the draw objects (Change records
the fields its flags name), the window's own SetValue and the vary box's virtual SetValue and GetValue.

A case is a script of operations on a world of twelve window slots (a plain window, a button, check buttons, a text box, a knob or a
vary box, all made by their constructors), three draw objects, the UI globals the handlers use (the auto redraw, the focus window, the
capture, the clock) and the pump's three mouse button states. After each operation the answer, every window reachable from the slots
(as a table of its fields by Mac offset: the whole object), the globals, the heap traffic and the length of the call log are compared,
and the whole call log at the end. The PowerPC side decides which operations the state allows (a handler called on a window of another
kind, a division by zero in the knob's angle arithmetic, a cast of a window that is not a button) and the host side replays exactly
those.

Quirks the tests pin: the button hands its own pointer messages and the left button's press and release to nobody (it answers 0 without
the default handler), a press with the pump's table already holding a press does nothing at all, the release calls StopHeldTimer only
when the table says 1; the toggle release tells the help text handler's Update; the pointer-in message with a button held posts the
button ITSELF 0x10008; the knob's press takes the focus even for the right button, its release answers 0 for any button; the knob's
angle arithmetic uses the angle, not the angle above the smallest; the vary box's hold on the decrement button only sets the counters,
on the increment button it also takes one step.
"""
import random as _random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIHandlers"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (all run as the original on the PowerPC side)
FN = dict(Button=0x101CB5F0, Check=0x101CC27C, Knob=0x101D07AC, TextBox=0x101D8134, VaryBox=0x101D9674,
          GetID=0x101DBE08, GetParent=0x101DBAFC, CheckFocus=0x101DC6AC, CursorChange=0x101DD860, CaptureDevice=0x101DCC10,
          UnCaptureDevice=0x101DCC6C, DoKnobMove=0x101D0718, ChangeAngle=0x101D0574, DoVaryBoxButton=0x101D915C)
# what they call that is real too: the button's members, GetWindow, the window's constructor and the rectangles
BUTTON_CTOR, CALC_FRAME, BUTTON_WHATAMI, DO_TEXT_COLOUR, SET_STATE, HILIGHT, STOP_HELD = (
    0x101CADC0, 0x101CAD20, 0x101CB010, 0x101CB084, 0x101CB1D0, 0x101CB2B8, 0x101CB478)
WINDOW_CTOR, MAKE_ABSOLUTE, GET_WINDOW = 0x101D98D8, 0x101DAB88, 0x101DBE40
REAL_HELPERS = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788,
                0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74]
# the controls' constructors (kind numbers are CreateObject's: 1 plain, 2 button, 8 check buttons, 10 text box, 11 knob, 12 vary box)
CTOR = {1: WINDOW_CTOR, 2: BUTTON_CTOR, 8: 0x101CBF90, 10: 0x101D7BF0, 11: 0x101D008C, 12: 0x101D8C94}
KINDS = (1, 2, 8, 10, 11, 12)
# the fakes (in-binary functions)
POST_MESSAGE, UPDATE_POINTER_IN, SET_CURSOR, IVE_GOT_TICKS = 0x101C9400, 0x101C8F18, 0x101C9F68, 0x101CAC3C
DEFAULT_HANDLER, GET_ATTR, GET_CHILD, WINDOW_SET_VALUE = 0x101DE3F8, 0x101DBB70, 0x101DBAC4, 0x101DFE84
VARY_SET_VALUE, VARY_GET_VALUE = 0x101D8F2C, 0x101D8F84
WINDOW_CALC, SET_CALLBACK, CAPTURE_KB = 0x101D987C, 0x101DCBA8, 0x101DCCEC

# ---- the original's data
G = dict(redraw=0x10395C28, focus=0x10395C20, capture=0x10395C14, buttons=0x10395BD0, help=0x102AF928, last=0x10395C18)
CORE_VTABLE = 0x102AF8FC   # CoolRect's vtable
VTABLE = {1: 0x102A7D94, 2: 0x102AF974, 8: 0x102AF9C8, 10: 0x102AFD14, 11: 0x102AFB40, 12: 0x102AFD68}
KIND_OF_VTABLE = {v: k for k, v in VTABLE.items()}
# the sizes of the objects: the operator new arguments in CreateObject
MAC_SIZE = {1: 0x138, 2: 0x15C, 8: 0x13C, 10: 0x150, 11: 0x168, 12: 0x158}
# the window handlers (TVectors) by the numbers the host's CallbackCode gives
HANDLER_CODE = {0x10268480: 1, 0x10268110: 2, 0x10268128: 3, 0x102681C0: 4, 0x10268298: 5, 0x102682D0: 6, 0x102682F0: 7,
                0x102683F8: 8, 0x10268440: 9}

# ---- labels (the host's tpw_uih_labels)
LAB_WINDOW, LAB_DRAW, LAB_HELP = 10, 40, 50   # (the windows take 10 to 21)
SLOTS, DRAWS = 12, 3
MAX_WINDOWS = 40
MAX_QUEUE = 64


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def w16(e, address, value):
    e.write(address, struct.pack(">h", s16(value)))


def w8(e, address, value):
    e.write(address, bytes([value & 0xFF]))


# ======================================================================= the tables of fields (Mac offset, type)
# Types: u8 s8 s16 s32 u32 (numbers), lab (a pointer as its label), cb (a handler as its number), kind (the class from the vtable),
# region (0 none, 1 the window's own rectangle, else a label), vptr (a CoolRect's vtable is right), ptr (not NULL), rawN (N bytes all
# untouched: 0xa5), zero64, const1.


def rect(base):
    return [(base + 4, "s16"), (base + 6, "s16"), (base + 8, "s16"), (base + 10, "s16")]


def core(base):
    """A CoolRect member: its vtable and the four coordinates."""
    return [(base, "vptr")] + rect(base)


BASE = [(0x00, "u8"), (0x01, "raw3")] + core(0x04) + core(0x10) + core(0x1C) + core(0x28) + core(0x34)
BASE += [(0x40, "region"), (0x44, "s32"), (0x48, "lab"), (0x4C, "s16"), (0x4E, "raw2"), (0x50, "lab"), (0x54, "lab"), (0x58, "lab"),
         (0x5C, "lab"), (0x60, "lab"), (0x64, "s32"), (0x68, "raw32"), (0x88, "s32"), (0x8C, "s32"), (0x90, "s32"), (0x94, "lab"),
         (0x98, "lab"), (0x9C, "zero64"), (0xDC, "s16"), (0xDE, "s16"), (0xE0, "s16"), (0xE2, "s16"), (0xE4, "lab"), (0xE8, "s16"),
         (0xEA, "raw2"), (0xEC, "s32"), (0xF0, "s32")]
BASE += [(0xF4 + 2 * i, "s16") for i in range(4)] + [(0xFC, "lab")] + [(0x100 + 2 * i, "s16") for i in range(4)]
BASE += [(0x108, "s16"), (0x10A, "raw6"), (0x110, "cb"), (0x114, "cb"), (0x118, "kind"), (0x11C, "s32")]
BASE += [(0x120 + 4 * i, "lab") for i in range(4)]
BASE += [(0x130, "s32"), (0x134, "u8"), (0x135, "u8"), (0x136, "s8"), (0x137, "const1")]

# The words the operations set or the handlers write are numbers whatever they hold (the 0xa5 fill included).
EXTRA = {
    2: [(0x138, "s8"), (0x139, "s8"), (0x13A, "raw2"), (0x13C, "s32"), (0x140, "s32"), (0x144, "u32"), (0x148, "lab"),
        (0x14C, "u32"), (0x150, "raw4"), (0x154, "u32"), (0x158, "s8"), (0x159, "s8"), (0x15A, "u8"), (0x15B, "raw1")],
    8: [(0x138, "s32")],
    10: [(0x138, "ptr"), (0x13C, "s32"), (0x140, "s32"), (0x144, "s32"), (0x148, "s32"), (0x14C, "s16"), (0x14E, "s16")],
    11: [(0x138 + 4 * i, "s32") for i in range(10)] + [(0x160, "s16"), (0x162, "s16"), (0x164, "s16"), (0x166, "raw2")],
    12: [(0x138, "s32"), (0x13C, "s32"), (0x140, "s32"), (0x144, "s32"), (0x148, "s32"), (0x14C, "s32"), (0x150, "s32"),
         (0x154, "u8"), (0x155, "raw3")],
}


def check_tables():
    """The tables cover every byte of the object, once: the window's 0x138 bytes and each class's own up to its size."""
    sizes = {"u8": 1, "s8": 1, "s16": 2, "s32": 4, "u32": 4, "lab": 4, "cb": 4, "kind": 4, "region": 4, "vptr": 4, "ptr": 4,
             "const1": 1, "zero64": 0x40}

    def width(how):
        return int(how[3:]) if how.startswith("raw") else sizes[how]

    def tiles(fields, start, end):
        at = start
        for offset, how in sorted(fields):
            assert offset == at, f"table gap or overlap at {offset:#x} (expected {at:#x})"
            at += width(how)
        assert at == end, f"table ends at {at:#x}, not {end:#x}"

    tiles(BASE, 0, 0x138)
    for kind, fields in EXTRA.items():
        tiles(fields, 0x138, MAC_SIZE[kind])


check_tables()

# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures and the operations."""

    ADDRESSES = [(G["redraw"], 4), (G["focus"], 4), (G["capture"], 4), (G["buttons"], 36), (G["help"], 4), (G["last"], 4)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.slots = [0] * SLOTS            # the window in each slot (0: none)
        self.heap = [0, 0, 0, 0]            # blocks handled by operator new, new[], delete, delete[] while a function runs
        self.observe = False                # true while a function under test runs
        self.clock = 0                      # what IveGotTicks answers next, and the step it moves on by
        self.step = 0
        self.draws = []
        for _ in range(DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.uih_draw_vtable)
            self.draws.append(block)
        self.help = e.alloc(0x18, bytes(0x18))
        w32(e, self.help + 0x10, e.uih_help_vtable)
        fixed = {a: LAB_DRAW + i for i, a in enumerate(self.draws)}
        fixed[self.help] = LAB_HELP
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues)
        # the globals as the Mac's data has them (zero), and the help text handler the fake
        w32(e, G["redraw"], 0)
        w32(e, G["focus"], 0)
        w32(e, G["capture"], 0)
        e.write(G["buttons"], bytes(36))
        w32(e, G["help"], self.help)
        w32(e, G["last"], 0)

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def made(self, slot):
        return 0 <= slot < SLOTS and self.slots[slot] != 0

    def window(self, slot):
        return self.slots[slot] if 0 <= slot < SLOTS else 0

    def kind_of(self, address):
        return KIND_OF_VTABLE.get(self.e.u32(address + 0x118), 0) if address else 0

    def kind_at(self, slot):
        return self.kind_of(self.slots[slot]) if self.made(slot) else 0

    def children(self, address):
        """The children of the window, as GetWindow walks them (the first child, then each one's next sibling)."""
        e = self.e
        out = []
        child = e.u32(address + 0x58)
        while child and len(out) < 64:
            out.append(child)
            child = e.u32(child + 0x50)
        return out

    def acyclic(self):
        """False when the windows' links make a loop (the original would never come back from one)."""
        e = self.e
        known = {w for w in self.slots if w}

        def visit(w, state):
            if w == 0 or w not in known:
                return True
            if state.get(w) == 1:
                return False
            if state.get(w) == 2:
                return True
            state[w] = 1
            ok = visit(e.u32(w + 0x58), state) and visit(e.u32(w + 0x50), state)
            state[w] = 2
            return ok

        state = {}
        return all(visit(w, state) for w in known) and all(self.parent_chain_ok(w, known) for w in known)

    def parent_chain_ok(self, w, known):
        seen = set()
        while w and w in known:
            if w in seen:
                return False
            seen.add(w)
            w = self.e.u32(w + 0x5C)
        return True

    # ---- the dumps (the tables are BASE and EXTRA; the host's are tpw_uih_state)
    def field(self, d, w, offset, kind):
        """The field of the type `kind` at `offset` of the object whose bytes are `d` (at address w)."""
        L = self.L
        if kind == "u8":
            return d[offset]
        if kind == "s8":
            return T.s8(d[offset])
        if kind == "s16":
            return struct.unpack_from(">h", d, offset)[0]
        if kind == "s32":
            return struct.unpack_from(">i", d, offset)[0]
        if kind == "u32":
            return struct.unpack_from(">I", d, offset)[0]
        if kind == "lab":
            return L.lab(struct.unpack_from(">I", d, offset)[0])
        if kind == "cb":
            value = struct.unpack_from(">I", d, offset)[0]
            return 0 if value == 0 else HANDLER_CODE.get(value, 99)
        if kind == "kind":
            return KIND_OF_VTABLE.get(struct.unpack_from(">I", d, offset)[0], 0)
        if kind == "region":
            value = struct.unpack_from(">I", d, offset)[0]
            return 0 if value == 0 else 1 if value == w + 4 else L.lab(value)
        if kind == "vptr":
            return int(struct.unpack_from(">I", d, offset)[0] == CORE_VTABLE)
        if kind == "ptr":
            return int(struct.unpack_from(">I", d, offset)[0] != 0)
        if kind == "zero64":
            return int(d[offset:offset + 0x40] == bytes(0x40))
        if kind == "const1":
            return 1
        if kind.startswith("raw"):
            n = int(kind[3:])
            return int(d[offset:offset + n] == b"\xa5" * n)
        raise ValueError(kind)

    def dump_window(self, w):
        kind = self.kind_of(w)
        d = bytes(self.e.read(w, MAC_SIZE.get(kind, 0x138)))
        return self.L.lab(w), {offset: self.field(d, w, offset, how) for offset, how in BASE + EXTRA.get(kind, [])}

    def discover(self):
        """The windows reachable from the slots, as the host walks them: a window, then its first child, then its next sibling."""
        seen = []

        def visit(w):
            if w == 0 or w in seen or len(seen) >= MAX_WINDOWS:
                return
            seen.append(w)
            visit(self.e.u32(w + 0x58))
            visit(self.e.u32(w + 0x50))

        for w in self.slots:
            visit(w)
        return seen

    def dump_globals(self):
        e, L = self.e, self.L
        out = [T.s32(e.u32(G["redraw"])), L.lab(e.u32(G["focus"])), L.lab(e.u32(G["capture"])), self.clock, self.step]
        for i in range(3):
            base = G["buttons"] + 12 * i
            state, x, y = struct.unpack(">3h", e.read(base, 6))
            out += [state, x, y, T.s32(e.u32(base + 8))]
        return out

    def dump_state(self):
        dumps = [self.dump_window(w) for w in self.discover()]
        return dict(order=[label for label, _ in dumps], windows={label: fields for label, fields in dumps},
                    globals=self.dump_globals(), heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        e = self.e
        zero = [0, 0, 0, 0, 0]
        if code == 1:   # a control through its constructor: slot, kind, parent slot, attributes, id, left, top, right, bottom
            slot, kind, parent = a[0], a[1], a[2]
            if not 0 <= slot < SLOTS or self.made(slot) or kind not in KINDS or (parent >= 0 and not self.made(parent)) or parent >= slot:
                return None
            size = MAC_SIZE[kind]
            block = e.alloc(size, b"\xa5" * size)
            self.L.labels[block] = LAB_WINDOW + slot
            self.call(CTOR[kind], block, self.window(parent), a[3], a[4], s16(a[5]), s16(a[6]), s16(a[7]), s16(a[8]))
            self.slots[slot] = block
            return [self.L.lab(block), 0, 0, 0, 0]
        if code == 9:   # a global: which, value
            which, value = a[0], a[1]
            if which == 1:
                w32(e, G["redraw"], value)
            elif which == 2 or which == 3:
                if value >= 0 and not self.made(value):
                    return None
                w32(e, G["focus"] if which == 2 else G["capture"], self.window(value))
            elif which == 4:
                self.clock = value & MINUS
            elif which == 5:
                self.step = value & MINUS
            return zero
        if code == 10:   # one of the pump's button states: index, state, x, y, tick
            base = G["buttons"] + 12 * a[0]
            e.write(base, struct.pack(">3h", s16(a[1]), s16(a[2]), s16(a[3])))
            w32(e, base + 8, a[4])
            return zero
        if not self.made(a[0]):
            return None
        w = self.slots[a[0]]
        kind = self.kind_of(w)
        if code == 2:   # links: slot, parent, previous, next, child
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
            if not self.acyclic():
                raise AssertionError("the script made a loop of links")
            return zero
        if code == 3:   # fields: slot, attributes, cursor, buttons held, id, help text
            w32(e, w + 0x44, a[1])
            w16(e, w + 0x4C, a[2])
            w32(e, w + 0x11C, a[3])
            w32(e, w + 0x88, a[4])
            w32(e, w + 0x8C, a[5])
            return zero
        if code == 4:   # draw objects: slot, the window's, the client's
            w32(e, w + 0x94, self.draws[a[1]] if 0 <= a[1] < DRAWS else 0)
            w32(e, w + 0x98, self.draws[a[2]] if 0 <= a[2] < DRAWS else 0)
            return zero
        if code == 5:   # a button's data: slot, state, highlight, held, delay, interval, held tick, up word, down word, shift x, shift y
            if kind != 2:
                return None
            w8(e, w + 0x138, a[1])
            w8(e, w + 0x139, a[2])
            w8(e, w + 0x15A, a[3])
            w32(e, w + 0x13C, a[4])
            w32(e, w + 0x140, a[5])
            w32(e, w + 0x144, a[6])
            w32(e, w + 0x14C, a[7])
            w32(e, w + 0x154, a[8])
            w8(e, w + 0x158, a[9])
            w8(e, w + 0x159, a[10])
            return zero
        if code == 6:   # a knob's data: slot, min angle, max angle, angle range, drag angle, angle, min value, max value, value range,
            #            value, old value, last y, previous y, dragging
            if kind != 11:
                return None
            for i in range(10):
                w32(e, w + 0x138 + 4 * i, a[1 + i])
            w16(e, w + 0x160, a[11])
            w16(e, w + 0x162, a[12])
            w16(e, w + 0x164, a[13])
            return zero
        if code == 7:   # a vary box's data: slot, minimum, maximum, value, old value, increment, countdown, step, forced update
            if kind != 12:
                return None
            for i in range(7):
                w32(e, w + 0x138 + 4 * i, a[1 + i])
            w8(e, w + 0x154, a[8])
            return zero
        if code == 8:   # check buttons: slot, the selection
            if kind != 8:
                return None
            w32(e, w + 0x138, a[1])
            return zero
        return self.function(code, a, w, kind)

    # ---- what the state must allow for a function to run (the original would divide by zero, write into a window that is not a button,
    # or use a window that is not there)
    def knob_words(self, w):
        e = self.e
        return [T.s32(e.u32(w + 0x138 + 4 * i)) for i in range(10)]

    def angle_divides_by_zero(self, w, angle):
        """True when ChangeAngle(angle) would divide by zero or overflow a division: mirrors the angle and value arithmetic."""
        min_a, max_a, range_a, drag, _, min_v, max_v, range_v, value, _ = self.knob_words(w)
        if angle < min_a:
            angle = min_a
        elif angle > max_a:
            angle = max_a
        if angle == drag:
            return False

        def bad(numerator, denominator):
            numerator = T.s32(numerator)
            return denominator == 0 or (numerator == -0x80000000 and denominator == -1)

        if angle == max_a:
            new = max_v
        elif angle == min_a:
            new = min_v
        else:
            if bad(range_v * angle, range_a):
                return True
            new = T.s32(min_v + int(T.s32(range_v * angle) / range_a))
        if new > max_v:
            new = max_v
        elif new < min_v:
            new = min_v
        if new == value or new == max_v or new == min_v:
            return False
        return bad(range_a * (new - min_v), range_v)

    def move_divides_by_zero(self, w, y):
        """The same for DoKnobMove(y)."""
        e = self.e
        dragging, last = struct.unpack(">h", e.read(w + 0x164, 2))[0], struct.unpack(">h", e.read(w + 0x160, 2))[0]
        if dragging == 0 or (y & 0xFFFF) == last:
            return False
        new = s16(y)
        return self.angle_divides_by_zero(w, T.s32(T.s32(e.u32(w + 0x144)) + (last - new) * 8))

    def all_buttons(self, w):
        return all(self.kind_of(c) == 2 for c in self.children(w))

    def child_with_id(self, w, ident):
        for c in self.children(w):
            if T.s32(self.e.u32(c + 0x88)) == ident:
                return c
        return 0

    def allowed(self, code, a, w, kind):
        """False for an operation the state does not allow."""
        e = self.e
        wanted = {30: 2, 31: 8, 32: 11, 33: 10, 34: 12, 40: 11, 41: 11, 42: 12}
        if code in wanted and kind != wanted[code]:
            return False
        if code >= 20 and not self.acyclic():
            return False
        if code == 31:
            message = T.s32(a[1] & MINUS)
            if message == 0x100:
                return self.all_buttons(w)
            if message == 0x15:
                first = e.u32(w + 0x58)
                return first == 0 or self.kind_of(first) != 2 or self.all_buttons(w)
        if code == 32:
            message = T.s32(a[1] & MINUS)
            if message == 0x10003 and self.move_divides_by_zero(w, T.s32(a[2] & MINUS)):
                return False
        if code == 34:
            message = T.s32(a[1] & MINUS)
            if message == 0x15:
                return all(self.child_with_id(w, i) == 0 or self.kind_of(self.child_with_id(w, i)) == 2 for i in (1, 2))
        if code == 40 and self.move_divides_by_zero(w, T.s32(a[1] & MINUS)):
            return False
        if code == 41 and self.angle_divides_by_zero(w, T.s32(a[1] & MINUS)):
            return False
        return True

    def function(self, code, a, w, kind):
        if not self.allowed(code, a, w, kind):
            return None
        e, L = self.e, self.L
        answer = [0, 0, 0, 0, 0]
        self.observe = True
        if code == 20:
            answer[0] = T.s32(self.call(FN["GetID"], w))
        elif code == 21:
            answer[0] = L.lab(self.call(FN["GetParent"], w))
        elif code == 22:
            self.call(FN["CheckFocus"], w)
        elif code == 23:
            self.call(FN["CursorChange"], w)
        elif code == 24:
            answer[0] = T.s32(self.call(FN["CaptureDevice"], w))
        elif code == 25:
            answer[0] = T.s32(self.call(FN["UnCaptureDevice"], w))
        elif code in (30, 31, 32, 33, 34):
            name = {30: "Button", 31: "Check", 32: "Knob", 33: "TextBox", 34: "VaryBox"}[code]
            answer[0] = T.s32(self.call(FN[name], w, a[1], a[2], a[3]))
        elif code == 40:
            self.call(FN["DoKnobMove"], w, a[1])
        elif code == 41:
            self.call(FN["ChangeAngle"], w, a[1])
        elif code == 42:
            self.call(FN["DoVaryBoxButton"], w, s16(a[1]))
        else:
            raise ValueError(code)
        self.observe = False
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects' and the help text's vtables, in persistent memory."""
    if hasattr(e, "uih_draw_vtable"):
        return

    def change(emu):
        # Change(flags, record): the frame for flag 1, the angle for flag 4, and for the text flags (0x4c000) the style the record points at
        this, flags, record = emu.r(3), emu.r(4), emu.r(5)
        frame, angle, shift_x, shift_y, colour = 0, 0, 0, 0, 0
        if flags & 1:
            frame = struct.unpack(">h", emu.read(record + 4, 2))[0]
        if flags & 4:
            angle = struct.unpack(">h", emu.read(record + 8, 2))[0]
        if flags & 0x4C000:
            style = emu.u32(record + 0x10)
            shift_x, shift_y = struct.unpack(">2h", emu.read(style + 0x0A, 4))
            colour = emu.u32(style + 0x10)
        emu.cl.rec("Change", (emu.cl.lab(this), flags, frame, angle, shift_x, shift_y, colour))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.uih_draw_vtable = vt

    # the help text handler: the vtable pointer is at +0x10 of the object, the slots 7 to 10 are Update, OnMouseMoved, OnMouseMovedIn and
    # OnMouseMovedOut
    def update(emu):
        emu.cl.rec("Help.Update", (emu.cl.lab(emu.r(3)),))

    def moved(emu):
        emu.cl.rec("Help.OnMouseMoved", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4)), T.s16(emu.r(5))))

    def moved_in(emu):
        emu.cl.rec("Help.OnMouseMovedIn", (emu.cl.lab(emu.r(3)), emu.cl.lab(emu.r(4))))

    def moved_out(emu):
        emu.cl.rec("Help.OnMouseMovedOut", (emu.cl.lab(emu.r(3)),))

    vt = T.persist_alloc(e, 0x48)
    for offset, fn in ((0x24, update), (0x28, moved), (0x2C, moved_in), (0x30, moved_out)):
        e.write(vt + offset, struct.pack(">I", T.fake_function(e, fn)))
    e.uih_help_vtable = vt


def copy_rect(e, this, to):
    """The window's rectangle (the coordinates at +8) copied to the rectangle whose coordinates start at `to`."""
    e.write(this + to, bytes(e.read(this + 8, 8)))


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update([BUTTON_CTOR, CALC_FRAME, BUTTON_WHATAMI, DO_TEXT_COLOUR, SET_STATE, HILIGHT, STOP_HELD])
    S.real.update([WINDOW_CTOR, MAKE_ABSOLUTE, GET_WINDOW])
    S.real.update(REAL_HELPERS)
    S.real.update(CTOR.values())
    R = CL.recorder

    # the heap: nothing under test makes or frees a block, so any block is a mistake (counted while a function runs)
    def op_new(e):
        size = max(e.r(3), 4)
        if e.us.observe:
            e.us.heap[0] += 1
        e.set_r(3, e.alloc(size, b"\xa5" * size))

    def op_new_array(e):
        size = max(e.r(3), 4)
        if e.us.observe:
            e.us.heap[1] += 1
        e.set_r(3, e.alloc(size, b"\xa5" * size))

    def op_delete(e):
        if e.r(3) != 0 and e.us.observe:
            e.us.heap[2] += 1

    def op_delete_array(e):
        if e.r(3) != 0 and e.us.observe:
            e.us.heap[3] += 1

    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_array)
    S.imp("__dl__FPv", op_delete)
    S.imp("__dla__FPv", op_delete_array)

    # the senders, the pointer, the cursor and the handler every handler ends in
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(SET_CURSOR, R("SetCursor", "h"))
    S.internal(DEFAULT_HANDLER, R("DefaultHandler", "Liii", ret="i"))

    # the UI clock: the script's clock, moved on by its step at every call
    def ticks(e):
        us = e.us
        now = us.clock
        us.clock = (us.clock + us.step) & MINUS
        e.cl.rec("IveGotTicks", (now,))
        e.set_r(3, now)

    S.internal(IVE_GOT_TICKS, ticks)

    # the window's accessors the handlers call, answering what the originals answer
    S.internal(GET_ATTR, R("GetAttr", "L", ret=lambda e, L: e.u32(e.r(3) + 0x44)))
    S.internal(GET_CHILD, R("GetChild", "Li", ret=lambda e, L: e.u32(e.r(3) + 0x58)))
    S.internal(WINDOW_SET_VALUE, R("Window.SetValue", "Li", ret=lambda e, L: 1))
    S.internal(VARY_SET_VALUE, R("VaryBox.SetValue", "Li", ret="i"))
    S.internal(VARY_GET_VALUE, R("VaryBox.GetValue", "L", ret="i"))

    # what the constructors reach: the base CalcClientArea (which copies the rectangle) and SetCallbackFunction (which sets the handler
    # when it is not NULL, and records its number)
    def window_calc(e, L):
        copy_rect(e, e.r(3), 0x2C)

    S.internal(WINDOW_CALC, R("CalcClientArea", "L", then=window_calc))

    def set_callback(e):
        this, handler = e.r(3), e.r(4)
        e.cl.rec("SetCallbackFunction", (e.cl.lab(this), 0 if handler == 0 else HANDLER_CODE.get(handler, 99)))
        if handler != 0:
            w32(e, this + 0x110, handler)
        e.set_r(3, 1)

    S.internal(SET_CALLBACK, set_callback)
    S.internal(CAPTURE_KB, R("CaptureKb", "Li", ret=lambda e, L: 1))
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uih_open", False):
        return
    import ctypes as C
    lib.tpw_uih_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_uih_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uih_state.restype = C.c_int32
    lib.tpw_uih_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uih_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._uih_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        open_host(lib)
        lib.tpw_uih_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues)
        lib.tpw_uih_labels()

    def op(self, code, a):
        C = self.C
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 5)()
        self.lib.tpw_uih_op(code, args, out)
        return list(out)

    def dump_state(self):
        C, lib = self.C, self.lib
        buf = (C.c_int64 * 65536)()
        count = lib.tpw_uih_state(buf)
        words = list(buf[:count])
        order, windows, i = [], {}, 0
        while i < count:
            label, pairs = words[i], words[i + 1]
            fields = {words[i + 2 + 2 * k]: words[i + 3 + 2 * k] for k in range(pairs)}
            assert len(fields) == pairs, f"window {label}: a field was written twice"
            order.append(label)
            windows[label] = fields
            i += 2 + 2 * pairs
        lib.tpw_uih_globals(buf)
        globals_ = list(buf[:17])
        lib.tpw_uih_heap(buf)
        return dict(order=order, windows=windows, globals=globals_, heap=list(buf[:4]),
                    log=C.c_int32.in_dll(lib, "tpw_cl_n").value)


# ======================================================================= running a script

RAN = {}   # what the PowerPC side decided to run, for the host side (by case)


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
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
            ran.append(op)
            steps.append(dict(op=code, answer=answer, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def ui_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for code, a in RAN.pop(id(case)):
            answer = host.op(code, a)
            steps.append(dict(op=code, answer=answer, state=host.dump_state()))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)


# ======================================================================= scripts: building blocks

TESTS = []

# the values the scripts draw from
STATES = [0, 1, 2, 3, -1, 127, -128, 4, 0x55, -2]
HILIGHTS = [0, 1, -1, 2, 0x7F]
HELD_STATES = [0, 1, 2, 3, 0x80, 0xFF]
COLOURS = [0xFFFFFFFF, 0x000000FF, 0, 0x01020304, 0x80FF7F00, 0xFF0000FF, 0x00FF00FF, 0x0000FFFF]
SHIFTS = [0, 1, -1, 2, 126, 127, -128, -127]
DRAW_PAIRS = [(-1, -1), (0, -1), (-1, 1), (0, 1), (2, 2)]
RECT = (10, 20, 110, 70)


def pos(x, y):
    """A position as the pump packs it: x in the high half, y in the low half."""
    return T.s32(((x & 0xFFFF) << 16) | (y & 0xFFFF))


POSITIONS = [0, 1, -1, pos(10, 20), pos(-5, -7), pos(0x7FFF, 0x7FFF), pos(-0x8000, -0x8000), pos(1, -1), pos(-1, 1), pos(300, 4),
             0x00100020, T.s32(0x80008000), 0x7FFFFFFF, -0x80000000, pos(0, 0x8000), pos(0x1234, 0xFFFF)]


def case_of(ops, q=None):
    case = dict(ops=ops)
    if q:
        case["q"] = q
    return case


def make(slot, kind, parent, attributes=1, ident=100, rect_=RECT):
    """A control through its constructor."""
    return (1, (slot, kind, parent, attributes, ident) + tuple(rect_))


def fields(slot, attributes=1, cursor=-1, held=0, ident=100, help_text=-1):
    return (3, (slot, attributes, cursor, held, ident, help_text))


def draws_op(slot, window_draw, client_draw):
    return (4, (slot, window_draw, client_draw))


def button_data(slot, state=0, hilight=0, held=0, delay=500, interval=125, tick=0, up=0xFFFFFFFF, down=0x000000FF, shift_x=1, shift_y=1):
    return (5, (slot, state, hilight, held, delay, interval, tick, up, down, shift_x, shift_y))


def knob_data(slot, min_a=0, max_a=0xFFF, range_a=None, drag=0, angle=0, min_v=0, max_v=1, range_v=None, value=0, old=0, last_y=0, previous_y=0,
              dragging=0):
    range_a = max_a - min_a if range_a is None else range_a
    range_v = max_v - min_v if range_v is None else range_v
    return (6, (slot, min_a, max_a, range_a, drag, angle, min_v, max_v, range_v, value, old, last_y, previous_y, dragging))


def vary_data(slot, min_v=0, max_v=1, value=0, old=0, increment=1, countdown=0, step=0, force=1):
    return (7, (slot, min_v, max_v, value, old, increment, countdown, step, force))


def glob(which, value):
    """A global: 1 the auto redraw, 2 the focus window's slot (-1 none), 3 the capture's slot, 4 the clock, 5 its step."""
    return (9, (which, value))


def table(index, state=0, x=0, y=0, tick=0):
    """One of the pump's three button states."""
    return (10, (index, state, x, y, tick))


def tree_ops(parents):
    """Op 2 for each slot of the dict {slot: parent slot or -1}: the links of a tree whose children are in the order of their slots."""
    ops = []
    for slot in parents:
        siblings = [t for t in parents if parents[t] == parents[slot]]
        at = siblings.index(slot)
        previous = siblings[at - 1] if at > 0 else -1
        following = siblings[at + 1] if at + 1 < len(siblings) else -1
        children = [t for t in parents if parents[t] == slot]
        ops.append((2, (slot, parents[slot], previous, following, children[0] if children else -1)))
    return ops


def root_op():
    """Slot 0: a plain window that receives what the controls post their parent."""
    return make(0, 1, -1, 1, 77, (0, 0, 400, 300))


def call(code, slot, message, a=0, b=0):
    return (code, (slot, message, a, b))


BUTTON, CHECK, KNOB, TEXTBOX, VARY = 30, 31, 32, 33, 34


# ======================================================================= test 1: the leaves

IDS = [0, 1, -1, 77, 0x7FFFFFFF, -0x80000000, 0x12345, -2]
CURSORS = [-1, 0, 1, 5, 0x7FFF, -0x8000, 100, -2]


def leaf_world(kind=1, ident=9, cursor=-1):
    """A root and a window of the kind under it (slot 1), with a sibling of another kind (slot 2)."""
    return [root_op(), make(1, kind, 0, 1, ident), make(2, 2, 0, 1, 11)] + tree_ops({0: -1, 1: 0, 2: 0}) + [fields(1, 1, cursor, 0, ident)]


def leaf_edges():
    # GetID and GetParent on every kind, in the middle of the tree and on the root
    for kind in KINDS:
        for ident in IDS:
            yield case_of(leaf_world(kind, ident) + [(20, (1,)), (21, (1,)), (20, (0,)), (21, (0,)), (20, (2,)), (21, (2,))])
    # cursorChange: every cursor number (a signed short)
    for cursor in CURSORS:
        yield case_of(leaf_world(1, 3, cursor) + [(23, (1,)), (23, (2,)), fields(2, 1, cursor), (23, (2,))])
    # CheckFocus: nobody has the focus, the window has it, another has it, repeated, the other way round
    for focus in (-1, 0, 1, 2):
        for slot in (0, 1, 2):
            yield case_of(leaf_world() + [glob(2, focus), (22, (slot,)), (22, (slot,)), (22, ((slot + 1) % 3,)), (22, (slot,))])
    # CaptureDevice and UnCaptureDevice: nobody has the pointer, the window, another window; the pump's update after a release
    for capture in (-1, 0, 1, 2):
        yield case_of(leaf_world() + [glob(3, capture), (24, (1,)), (24, (1,)), (25, (1,)), (25, (1,)), (24, (2,)), (25, (2,)), (24, (2,)), (25, (1,)),
                                      (25, (2,)), (24, (1,)), (24, (0,)), (25, (0,)), (25, (1,))])
    # the focus and the pointer together, on the window kinds the handlers belong to
    for kind in KINDS:
        yield case_of(leaf_world(kind) + [(22, (1,)), (24, (1,)), (22, (2,)), (24, (2,)), (25, (1,)), (25, (2,)), (22, (1,)), (25, (1,))])


def leaf_random(r):
    ops = [root_op()]
    n = r.randint(2, 6)
    parents = {0: -1}
    for slot in range(1, n):
        parents[slot] = r.randrange(slot)
        ops.append(make(slot, r.choice(KINDS), parents[slot], r.choice([1, 3, 0x401]), r.choice(IDS), (0, 0, 50, 50)))
    ops += tree_ops(parents)
    ops += [fields(slot, 1, r.choice(CURSORS), 0, r.choice(IDS)) for slot in range(n)]
    for _ in range(r.randint(3, 25)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.2:
            ops.append(glob(r.choice([2, 3]), r.choice([-1] + list(range(n)))))
        elif k < 0.4:
            ops.append(fields(slot, 1, r.choice(CURSORS), 0, r.choice(IDS)))
        else:
            ops.append((r.choice([20, 21, 22, 23, 24, 25, 22, 24, 25]), (slot,)))
    return case_of(ops, q={"PostMessage": [r.choice([0, 1]) for _ in range(3)]})


TESTS.append(FnTest("InterfaceWindow: GetID, GetParent, CheckFocus, cursorChange, CaptureDevice, UnCaptureDevice", ui_ppc, ui_host,
                    edge=leaf_edges, rnd=leaf_random))


# ======================================================================= test 2: the text box's handler

MESSAGES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xC, 0x10, 0x15, 0x1E, 0x100, 0x101, 0x800, 0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x10006,
            0x10007, 0x10008, 0x10009, 0x1000A, 0x1000D, -1, -0x80000000, 0x7FFFFFFF, 0x11006]


def textbox_edges():
    base = [root_op(), make(1, 10, 0, 1, 12)] + tree_ops({0: -1, 1: 0})
    for message in MESSAGES:
        for a, b in ((0, 0), (1, 2), (-1, 0x7FFFFFFF), (pos(10, 20), pos(30, 40))):
            yield case_of(base + [call(TEXTBOX, 1, message, a, b)], q={"DefaultHandler": [0, 5, -1]})
    for answer in (0, 1, -1, 0x12345678, -0x80000000):
        yield case_of(base + [call(TEXTBOX, 1, 0xC, 0, 0), call(TEXTBOX, 1, 3, 0, 0)], q={"DefaultHandler": [answer, 7]})


def textbox_random(r):
    base = [root_op(), make(1, 10, 0, r.choice([1, 3, 0x401]), 12)] + tree_ops({0: -1, 1: 0})
    ops = []
    for _ in range(r.randint(1, 8)):
        ops.append(call(TEXTBOX, 1, r.choice(MESSAGES + [r.getrandbits(32) - 0x80000000]), r.choice(POSITIONS), r.choice(POSITIONS)))
    return case_of(base + ops, q={"DefaultHandler": [r.choice([0, 1, -1, 99]) for _ in range(4)]})


TESTS.append(FnTest("UI_DefaultTextBoxCallback", ui_ppc, ui_host, edge=textbox_edges, rnd=textbox_random))


# ======================================================================= test 3: the knob (ChangeAngle, DoKnobMove, the handler)

# the knob's ranges: (smallest angle, largest angle, smallest value, largest value)
KNOB_RANGES = [(0, 0xFFF, 0, 1), (0, 100, 0, 100), (-100, 100, -50, 50), (5, 5, 0, 10), (0, 100, 7, 7), (100, 0, 0, 100), (0, 100, 100, 0),
               (-0x80000000, 0x7FFFFFFF, 0, 1), (0, 0x7FFFFFFF, -0x80000000, 0x7FFFFFFF), (0, 360, 0, 3), (-360, 0, -4, 4), (1, 7, 1, 2),
               (0, 1000, -1000, 1000)]


def knob_world(ranges=(0, 0xFFF, 0, 1), drag=0, value=0, angle=0, draws=(-1, 0), dragging=0, last_y=0, previous_y=0, range_a=None, range_v=None,
               attributes=1, held=0):
    """A root and a knob under it (slot 1, id 21) with the ranges, the drag angle and value, and the draw objects."""
    min_a, max_a, min_v, max_v = ranges
    return ([root_op(), make(1, 11, 0, attributes, 21, RECT)] + tree_ops({0: -1, 1: 0}) + [draws_op(1, *draws)] +
            [knob_data(1, min_a, max_a, range_a, drag, angle, min_v, max_v, range_v, value, 0x1234, last_y, previous_y, dragging), fields(1, attributes, 4, held, 21)])


def angles_for(ranges):
    min_a, max_a = ranges[0], ranges[1]
    mid = min_a // 2 + max_a // 2
    out = [min_a - 1, min_a, min_a + 1, mid, mid + 1, max_a - 1, max_a, max_a + 1, 0, 1, -1, 7, 0x7FFFFFFF, -0x80000000, 360, 1000]
    return [T.s32(v) for v in out]


def knob_edges():
    # ChangeAngle over the ranges, from a few starting points: the angles at and around the ends, the middle and extremes. Each case turns
    # the knob to several angles in a row (the same angle twice: nothing the second time).
    for ranges in KNOB_RANGES:
        for drag, value in ((0, 0), (ranges[0], ranges[2]), (ranges[1], ranges[3]), (3, 1), (-77, -77)):
            for draws in ((-1, -1), (0, 1)):
                angles = angles_for(ranges)
                for first in range(0, len(angles), 4):
                    ops = []
                    for angle in angles[first:first + 4]:
                        ops += [(41, (1, angle)), (41, (1, angle))]
                    yield case_of(knob_world(ranges, drag, value, draws=draws) + ops, q={"PostMessage": [1, 0]})
    # the ranges the setters would never leave: words that disagree with the ends (a division by zero is skipped)
    for range_a, range_v in ((0, 0), (1, 1), (-1, -1), (7, 0), (0, 7), (0x7FFFFFFF, 3), (3, 0x7FFFFFFF), (-0x80000000, 5), (5, -0x80000000)):
        for angle in (-3, 0, 1, 2, 50, 99, 100, 101):
            yield case_of(knob_world((0, 100, 0, 100), 0, 0, range_a=range_a, range_v=range_v) + [(41, (1, angle)), (41, (1, angle + 1))])
    # the displayed angle and the value follow each other
    yield case_of(knob_world((0, 100, 0, 10), 0, 0, draws=(0, 1)) + [(41, (1, angle)) for angle in range(0, 101, 9)] + [(41, (1, angle)) for angle in range(100, -1, -13)])
    # DoKnobMove: not dragging, dragging; the same y, another y, a y with high bits; negative last y (never equal), 32767, -32768
    for dragging in (0, 1, -1, 0x100):
        for last_y in (0, 5, -5, 0x7FFF, -0x8000, 1):
            for y in (0, 5, 6, -5, 0x10005, 0x7FFF, 0x8000, 0xFFFF, -0x8000, -1, 0x12345, 4):
                yield case_of(knob_world((0, 100, 0, 100), 50, 50, dragging=dragging, last_y=last_y, previous_y=9) + [(40, (1, y)), (40, (1, y))])
    # a drag: the knob turns by 8 angle units for each unit up, the end stops it
    yield case_of(knob_world((0, 100, 0, 100), 50, 50, dragging=1, last_y=100, draws=(0, 1)) + [(40, (1, y)) for y in (99, 90, 90, 80, 70, 60, 61, 65, 200, 0)])
    yield case_of(knob_world((0, 4095, 0, 1), 0, 0, dragging=1, last_y=1000, draws=(0, 1)) + [(40, (1, y)) for y in range(1000, 400, -37)])
    # the handler: 0x10005 (a press) with every button, 0x10004, 0x10003 and the messages it passes on
    for a in (0, 1, 2, -1, 3):
        for b in POSITIONS:
            for focus in (-1, 0, 1):
                for capture in (-1, 0, 1):
                    yield case_of(knob_world((0, 100, 0, 100), 50, 50, dragging=1) + [glob(2, focus), glob(3, capture), call(KNOB, 1, 0x10005, a, b),
                                                                                     call(KNOB, 1, 0x10004, a, b)], q={"DefaultHandler": [4, 5]})
    for a in POSITIONS:
        for dragging in (0, 1):
            for last_y in (0, 20, -1):
                yield case_of(knob_world((0, 100, 0, 100), 50, 50, dragging=dragging, last_y=last_y, draws=(-1, 1)) + [call(KNOB, 1, 0x10003, a, 0)],
                              q={"DefaultHandler": [9]})
    for message in MESSAGES:
        yield case_of(knob_world() + [call(KNOB, 1, message, 0, 0), call(KNOB, 1, message, 1, 0x00100010)], q={"DefaultHandler": [0, 3, -4]})
    # a whole drag through the handler
    yield case_of(knob_world((0, 100, 0, 100), 0, 0, draws=(0, 1)) + [glob(2, 0), call(KNOB, 1, 0x10005, 0, pos(30, 300)), call(KNOB, 1, 0x10003, pos(30, 290)),
                                                                     call(KNOB, 1, 0x10003, pos(30, 280)), call(KNOB, 1, 0x10003, pos(31, 280)),
                                                                     call(KNOB, 1, 0x10003, pos(30, 250)), call(KNOB, 1, 0x10004, 0, pos(30, 250)),
                                                                     call(KNOB, 1, 0x10003, pos(30, 200)), call(KNOB, 1, 0x10005, 1, pos(30, 200)),
                                                                     call(KNOB, 1, 0x10004, 1, pos(30, 200))])


def knob_random(r):
    ranges = r.choice(KNOB_RANGES)
    ops = knob_world(ranges, r.choice([0, ranges[0], ranges[1], r.randint(-120, 120)]), r.choice([0, ranges[2], ranges[3], r.randint(-60, 60)]),
                     draws=r.choice(DRAW_PAIRS), dragging=r.choice([0, 1]), last_y=r.choice([0, 100, -1, 0x7FFF]),
                     range_a=r.choice([None, None, None, 0, 5, r.randint(-5, 200)]), range_v=r.choice([None, None, None, 0, 5, r.randint(-5, 200)]))
    for _ in range(r.randint(1, 14)):
        k = r.random()
        if k < 0.25:
            ops.append((41, (1, r.choice(angles_for(ranges) + [r.randint(-200, 400)]))))
        elif k < 0.5:
            ops.append((40, (1, r.choice([r.randint(-50, 300), r.getrandbits(32) - 0x80000000, 0, 100, 0x10064]))))
        elif k < 0.85:
            ops.append(call(KNOB, 1, r.choice([0x10003, 0x10003, 0x10004, 0x10005, 0x10005, 1, 0x1E]), r.choice([0, 0, 1, 2, -1] + POSITIONS),
                            r.choice(POSITIONS)))
        elif k < 0.92:
            ops.append(glob(r.choice([2, 3]), r.choice([-1, 0, 1])))
        else:
            ops.append(knob_data(1, *ranges[:2], None, r.randint(-5, 105), 0, ranges[2], ranges[3], None, r.randint(-5, 105), 0, r.randint(-5, 200),
                                 r.randint(-5, 200), r.choice([0, 1])))
    return case_of(ops, q={"DefaultHandler": [r.choice([0, 1, -1]) for _ in range(3)]})


TESTS.append(FnTest("InterfaceKnob::ChangeAngle, DoKnobMove, UI_DefaultKnobCallback", ui_ppc, ui_host, edge=knob_edges, rnd=knob_random))


# ======================================================================= test 4: the vary box (DoVaryBoxButton, the handler)

# the box's ranges and increments: (smallest, largest)
VARY_RANGES = [(0, 1), (0, 100), (-5, 5), (10, 0), (3, 3), (-0x80000000, 0x7FFFFFFF), (0, 0x7FFFFFFF), (-100000, 100000), (0, 9999)]
INCREMENTS = [1, 2, 5, -1, 0, 100, 0x7FFFFFFF, -0x80000000]


def vary_world(ranges=(0, 100), value=50, increment=1, countdown=0, step=0, force=0, draws=(-1, -1), repeat_values=((500, 125), (500, 125))):
    """A root, a vary box under it (slot 1, id 31) and its two buttons (slots 2 and 3, ids 1 and 2)."""
    ops = [root_op(), make(1, 12, 0, 1, 31, RECT), make(2, 2, 1, 1, 1, (0, 0, 10, 10)), make(3, 2, 1, 1, 2, (0, 20, 10, 30))]
    ops += tree_ops({0: -1, 1: 0, 2: 1, 3: 1})
    ops += [vary_data(1, ranges[0], ranges[1], value, 0x4321, increment, countdown, step, force), draws_op(1, *draws)]
    ops += [button_data(2, delay=repeat_values[0][0], interval=repeat_values[0][1]), button_data(3, delay=repeat_values[1][0], interval=repeat_values[1][1])]
    return ops


def vary_edges():
    # DoVaryBoxButton: every number (and the shorts around them), the value below, at and above the ends, the forced update
    for ranges in VARY_RANGES:
        values = [ranges[0] - 1, ranges[0], ranges[0] + 1, (ranges[0] + ranges[1]) // 2, ranges[1] - 1, ranges[1], ranges[1] + 1, 0]
        for value in values:
            for increment in (1, 2, -1, 0, 0x7FFFFFFF):
                for force in (0, 1):
                    ops = []
                    for which in (0, 1, 2, 3, 4, 5, 6, -1, 0x10003):
                        ops.append((42, (1, which)))
                    yield case_of(vary_world(ranges, T.s32(value), increment, 3, 7, force) + ops)
    # each number alone, on a fresh box: the value far below, just below, at, in and above the ends; the forced update with the value
    # unchanged; the countdown about to run out (an increment other than 1) and not
    for ranges in ((0, 100), (-5, 5), (10, 0)):
        for value in (ranges[0] - 50, ranges[0] - 1, ranges[0], 3, ranges[1], ranges[1] + 1, ranges[1] + 50):
            for increment in (1, 2):
                for which in range(6):
                    for force in (0, 1):
                        for countdown, step in ((1, 1), (2, 7), (0, 0)):
                            yield case_of(vary_world(ranges, value, increment, countdown, step, force) + [(42, (1, which)), (42, (1, which))])
    # a hold: the start, then the repeats (the step grows tenfold every 20 repeats with an increment of 1; the other increments
    # move every fifth repeat)
    for increment in (1, 2, 5, -3, 0):
        for start, repeat in ((3, 5), (2, 4)):
            for count in (1, 5, 19, 20, 21, 41, 42, 63):
                ops = [(42, (1, start))] + [(42, (1, repeat))] * count
                yield case_of(vary_world((0, 100000), 50000, increment) + ops)
                yield case_of(vary_world((-100000, 0), -50000, increment, force=1) + ops)
    # the step grows until the ends stop it, and the counters reset at a new hold
    yield case_of(vary_world((0, 1000000), 0, 1) + [(42, (1, 3))] + [(42, (1, 5))] * 100 + [(42, (1, 2))] + [(42, (1, 4))] * 100)
    # the countdown and the step as the boxes can have them (the original does not look): zero, negative, huge
    for countdown in (0, 1, 2, -1, 0x14, 0x7FFFFFFF, -0x80000000):
        for step in (0, 1, 10, -10, 0x7FFFFFFF, 0x10000000, -0x80000000):
            for increment in (1, 2):
                yield case_of(vary_world((-0x80000000, 0x7FFFFFFF), 5, increment, countdown, step) + [(42, (1, 4)), (42, (1, 5)), (42, (1, 4)), (42, (1, 5))])
    # the handler: the buttons' messages with every id (1 the increment, 2 the decrement, 0 and others nothing)
    for message in (0x100, 0x10008, 0x10009, 0x10006, 0x1000A, 0x1000B, 0x1000C, 0x10007, 0x100, 0x101, 0x1000E, 0x15, 3):
        for a in (1, 2, 0, 3, -1, 0x7FFFFFFF):
            for increment in (1, 3):
                yield case_of(vary_world((0, 50), 25, increment, 5, 1, 1) + [call(VARY, 1, message, a, 0), call(VARY, 1, message, a, pos(3, 4))],
                              q={"DefaultHandler": [0, 8]})
    # the wheel: the box's value plus the amount, through the virtual SetValue and GetValue
    for get in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 50):
        for a in (0, 1, -1, 120, -120, 0x7FFFFFFF, -0x80000000):
            yield case_of(vary_world() + [call(VARY, 1, 0x1000D, a, 0)], q={"VaryBox.GetValue": [get], "VaryBox.SetValue": [1], "DefaultHandler": [3]})
    # the creation message: the two buttons' repeat words are set (200 and 50), a missing button is not
    for repeats in (((500, 125), (500, 125)), ((0, 0), (-1, -1)), ((200, 50), (200, 50))):
        yield case_of(vary_world(repeat_values=repeats) + [call(VARY, 1, 0x15, 0, 0), call(VARY, 1, 0x15, 0, 0)])
    yield case_of([root_op(), make(1, 12, 0, 1, 31, RECT)] + tree_ops({0: -1, 1: 0}) + [call(VARY, 1, 0x15, 0, 0)])
    yield case_of([root_op(), make(1, 12, 0, 1, 31, RECT), make(2, 2, 1, 1, 2, (0, 0, 10, 10))] + tree_ops({0: -1, 1: 0, 2: 1}) + [call(VARY, 1, 0x15)])
    yield case_of([root_op(), make(1, 12, 0, 1, 31, RECT), make(2, 2, 1, 1, 1, (0, 0, 10, 10)), make(3, 1, 1, 1, 2, (0, 0, 10, 10))]
                  + tree_ops({0: -1, 1: 0, 2: 1, 3: 1}) + [call(VARY, 1, 0x15)])
    # the first child with an id is the one: two buttons with the id 1, and the id 2 under a grandchild is not a child
    yield case_of([root_op(), make(1, 12, 0, 1, 31, RECT), make(2, 2, 1, 1, 1, (0, 0, 10, 10)), make(3, 2, 1, 1, 1, (0, 0, 10, 10)),
                   make(4, 2, 3, 1, 2, (0, 0, 10, 10))] + tree_ops({0: -1, 1: 0, 2: 1, 3: 1, 4: 3}) + [call(VARY, 1, 0x15)])


def vary_random(r):
    ranges = r.choice(VARY_RANGES)
    increment = r.choice(INCREMENTS)
    ops = vary_world(ranges, r.choice([ranges[0], ranges[1], r.randint(-10, 110), 0]), increment, r.choice([0, 1, 3, 20]), r.choice([0, 1, 10, 100]),
                     r.choice([0, 1]), r.choice(DRAW_PAIRS))
    for _ in range(r.randint(1, 20)):
        k = r.random()
        if k < 0.4:
            ops.append((42, (1, r.choice([0, 1, 2, 3, 4, 5, 4, 5, 4, 5, 6, -1]))))
        elif k < 0.8:
            ops.append(call(VARY, 1, r.choice([0x100, 0x10008, 0x10009, 0x10008, 0x1000D, 0x15, 3, 0x10001]), r.choice([0, 1, 2, 1, 2, 3, -1, 40]), r.choice(POSITIONS)))
        elif k < 0.9:
            ops.append(vary_data(1, *ranges, r.randint(-10, 110), 0, r.choice(INCREMENTS), r.choice([0, 1, 5, 20]), r.choice([0, 1, 10]), r.choice([0, 1])))
        else:
            ops.append(button_data(r.choice([2, 3]), delay=r.choice([0, 500]), interval=r.choice([0, 125])))
    return case_of(ops, q={"DefaultHandler": [r.choice([0, 1, -1]) for _ in range(3)], "VaryBox.GetValue": [r.randint(-50, 150), 0],
                           "VaryBox.SetValue": [1]})


TESTS.append(FnTest("InterfaceVaryBox::DoVaryBoxButton, UI_DefaultVaryBoxCallback", ui_ppc, ui_host, edge=vary_edges, rnd=vary_random))


# ======================================================================= test 5: the check buttons' handler


def check_world(count=3, attributes=1, current=-1, ids=None, states=None, first_plain=False, draws=(0, 1), button_ids_gap=False):
    """A root, check buttons under it (slot 1) and `count` buttons under those (slots 2..), with the ids, states and the group's selection.
    With first_plain the first child is a plain window instead of a button."""
    ids = ids or [i + 1 for i in range(count)]
    states = states or [0] * count
    ops = [root_op(), make(1, 8, 0, attributes, 40, RECT)]
    parents = {0: -1, 1: 0}
    for i in range(count):
        kind = 1 if first_plain and i == 0 else 2
        ops.append(make(2 + i, kind, 1, 1, ids[i], (0, 10 * i, 30, 10 * i + 8)))
        parents[2 + i] = 1
    ops += tree_ops(parents)
    for i in range(count):
        if not (first_plain and i == 0):
            ops += [button_data(2 + i, state=states[i]), draws_op(2 + i, *draws)]
    ops.append((8, (1, current)))
    return ops


def check_edges():
    # the click message (0x100): a button of the group goes down (b = 1) and up (b = 0), in a group with and without attribute 0x10,
    # with every current selection; the id asked for is a child's, absent or -1
    for count in (0, 1, 2, 5):
        ids = [i + 1 for i in range(count)]
        for attributes in (1, 0x11, 0, 0x10):
            for current in [-1] + ids + [99]:
                for a in ids + [99, -1, 0]:
                    for b in (1, 0, 2, -1):
                        for redraw in (0, 1):
                            yield case_of(check_world(count, attributes, current, states=[i % 2 for i in range(count)]) +
                                          [glob(1, redraw), call(CHECK, 1, 0x100, a, b), call(CHECK, 1, 0x100, a, b)], q={"DefaultHandler": [6, 0]})
    # ids that repeat and are negative: the first child with the id is the one found
    for ids in ([1, 1, 2], [-1, 2, -1], [3, 3, 3], [0, 0, 1]):
        for attributes in (1, 0x11):
            for current in (-1, 1, 3, 2, 0):
                for a in (1, 2, 3, -1, 0):
                    yield case_of(check_world(3, attributes, current, ids) + [call(CHECK, 1, 0x100, a, 1), call(CHECK, 1, 0x100, a, 0)])
    # a click on a child with the id -1 when the selection is a button that is not there (nothing to put up)
    for attributes in (1, 0x11):
        for current in (99, -1, 1, 2):
            for b in (0, 1):
                yield case_of(check_world(3, attributes, current, [-1, 1, 2]) + [call(CHECK, 1, 0x100, -1, b), call(CHECK, 1, 0x100, -1, b)])
    # the creation message (0x15): attribute 0x10 and no selection selects the first child when it is a button
    for count in (0, 1, 2, 3):
        for attributes in (0x10, 0x11, 0, 1, 0x30):
            for current in (-1, 1, 2, 99):
                for ids in (None, [5, 6, 7], [-1, 2, 3], [0, 0, 0]):
                    if ids is not None and len(ids) < count:
                        continue
                    for first_plain in (False, True):
                        yield case_of(check_world(count, attributes, current, ids[:count] if ids else None, first_plain=first_plain) +
                                      [call(CHECK, 1, 0x15, 0, 0), call(CHECK, 1, 0x15, 0, 0)], q={"DefaultHandler": [2]})
    # every other message goes to the default handler untouched
    for message in MESSAGES:
        for a in (1, 2, 0, -1):
            yield case_of(check_world(2, 0x10, 1) + [call(CHECK, 1, message, a, 1), call(CHECK, 1, message, a, 0)], q={"DefaultHandler": [0, 5, -1]})
    # a group of buttons clicked in turn, as a user would (messages the buttons send)
    yield case_of(check_world(5, 0x10, -1) + [call(CHECK, 1, 0x15, 0, 0)] + [call(CHECK, 1, 0x100, a, b) for a, b in
                                                                            ((2, 1), (3, 1), (3, 0), (2, 0), (5, 1), (5, 1), (1, 1), (4, 0), (-1, 1), (-1, 0))])
    yield case_of(check_world(5, 1, -1) + [call(CHECK, 1, 0x100, a, b) for a, b in ((2, 1), (3, 1), (3, 0), (2, 0), (5, 1), (5, 1), (1, 1), (4, 0), (-1, 1), (-1, 0))])


def check_random(r):
    count = r.choice([1, 2, 3, 5, 8])
    attributes = r.choice([1, 0x11, 0, 0x10, 0x31])
    ids = r.choice([None, None, [r.choice([0, 1, 2, -1, 3]) for _ in range(count)]])
    ids_ = ids or [i + 1 for i in range(count)]
    ops = check_world(count, attributes, r.choice([-1] + ids_ + [77]), ids, states=[r.choice([0, 1, 2, 3, -1]) for _ in range(count)],
                      first_plain=r.random() < 0.15, draws=r.choice(DRAW_PAIRS))
    for _ in range(r.randint(1, 14)):
        k = r.random()
        if k < 0.5:
            ops.append(call(CHECK, 1, 0x100, r.choice(ids_ + [77, -1, 0]), r.choice([0, 1, 1, 2, -1])))
        elif k < 0.7:
            ops.append(call(CHECK, 1, 0x15, 0, 0))
        elif k < 0.8:
            ops.append(call(CHECK, 1, r.choice(MESSAGES), r.choice(ids_), r.choice([0, 1])))
        elif k < 0.9:
            ops.append((8, (1, r.choice([-1] + ids_ + [77]))))
        else:
            ops.append(fields(1, r.choice([1, 0x11, 0, 0x10]), -1, 0, 40))
    return case_of(ops, q={"DefaultHandler": [r.choice([0, 1, -1]) for _ in range(3)], "PostMessage": [1]})


TESTS.append(FnTest("UI_DefaultCheckButtonsCallback", ui_ppc, ui_host, edge=check_edges, rnd=check_random))


# ======================================================================= test 6: the button's handler


BUTTON_MESSAGES = [0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x1E, 0x10006, 0x10007, 0x15, 3, 1, 0xC, 0x10, -1, 0, 0x100, 0x10008, 0x10009,
                   0x11006, 0x11004, 0x11005, 0x1D, 0x1F, 0x10000, 0x7FFFFFFF, -0x80000000]
BUTTON_MIX = [0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x1E, 0x1E, 0x10004, 0x10005, 0x10001, 0x10002, 0x1E, 0x10006, 0x10008, 3]
BUTTON_ATTRIBUTES = [1, 3, 0x11, 0x21, 0x31, 0x20, 0x10, 0x401, 0x23, 0, 0x33]
TABLE_STATES = [0, 1, 2, 3, -1, 5]


def button_world(attributes=1, state=0, hilight=0, held=0, delay=500, interval=125, tick=0, draws=(0, 1), buttons_held=0, redraw=0, clock=1000,
                 step=0, tables=((0, 0, 0, 0), (0, 0, 0, 0), (0, 0, 0, 0)), ident=5, up=0xFFFFFFFF, down=0x000000FF, shifts=(1, 1), focus=-1,
                 capture=-1):
    """A root and a button (slot 1) under it, set up: the state, the draw objects, the repeat words, the UI's globals and the pump's
    three button states (state, x, y, tick)."""
    ops = [root_op(), make(1, 2, 0, attributes, ident, RECT)] + tree_ops({0: -1, 1: 0})
    ops += [draws_op(1, *draws), button_data(1, state, hilight, held, delay, interval, tick, up, down, shifts[0], shifts[1]),
            fields(1, attributes, 3, buttons_held, ident, -1), glob(1, redraw), glob(4, clock), glob(5, step), glob(2, focus), glob(3, capture)]
    for index, row in enumerate(tables):
        ops.append(table(index, *row))
    return ops


def sampled(dims, per_value, seed):
    """Samples of the dimensions in which every value of every dimension occurs `per_value` times, with the others drawn at random."""
    r = _random.Random(seed)
    for name, values in dims.items():
        for v in values:
            for _ in range(per_value):
                sample = {k: r.choice(vs) for k, vs in dims.items()}
                sample[name] = v
                yield sample


BUTTON_DIMS = dict(
    attributes=BUTTON_ATTRIBUTES, state=STATES, hilight=HILIGHTS, held=HELD_STATES, delay=[500, 0, 1, -1, 0x7FFFFFFF], interval=[125, 0, 1, -1],
    buttons_held=[0, 1, 2, 7, 0x80000000, 4], redraw=[0, 1], draws=DRAW_PAIRS, table0=TABLE_STATES, table1=TABLE_STATES, table2=TABLE_STATES,
    elapsed=[0, 1, -1, 5, 124, 125, 126, 499, 500, 501, 0x7FFFFFFF, -0x80000000], step=[0, 1, 50, -3, 1000], message=BUTTON_MESSAGES,
    a=[0, 1, 2, -1, 3], b=POSITIONS, tick=[0, 100, 0x7FFFFFFF, -5], capture=[-1, 0, 1], focus=[-1, 0, 1], extra=[1, 2, 3, 4])


def button_case_of(sample, r):
    """The case a sample describes: the world, then the sample's message and a few more (drawn from the mix, ending in the same message)."""
    tables = ((sample["table0"], 3, 4, 100), (sample["table1"], -5, 6, 200), (sample["table2"], 7, -8, 0))
    clock = T.s32(sample["tick"] + sample["elapsed"])
    ops = button_world(sample["attributes"], sample["state"], sample["hilight"], sample["held"], sample["delay"], sample["interval"], sample["tick"],
                       sample["draws"], sample["buttons_held"], sample["redraw"], clock, sample["step"], tables, 5, capture=sample["capture"],
                       focus=sample["focus"])
    ops.append(call(BUTTON, 1, sample["message"], sample["a"], sample["b"]))
    for _ in range(sample["extra"] - 1):
        ops.append(call(BUTTON, 1, r.choice(BUTTON_MIX), r.choice([0, 0, 0, 1, 2]), r.choice(POSITIONS)))
    ops.append(call(BUTTON, 1, sample["message"], sample["a"], sample["b"]))
    return case_of(ops, q={"DefaultHandler": [0, 11, -3], "PostMessage": [1, 0]})


def button_sampled_edges():
    r = _random.Random(31)
    for sample in sampled(BUTTON_DIMS, 6, 77):
        yield button_case_of(sample, r)


def flow(*steps):
    """The handler called for a button with these (message, a, b) steps."""
    return [call(BUTTON, 1, m, a, b) for m, a, b in steps]


def button_flow_edges():
    inside = pos(20, 30)
    # a click, with every attribute set that matters (toggle 0x10, auto repeat 0x20, disabled 2), with the auto redraw on and off and the
    # draw objects there or not
    for attributes in (1, 0x11, 0x21, 0x31, 3, 0x13, 0x23, 0x10, 0x20):
        for redraw in (0, 1):
            for draws in ((-1, -1), (0, 1), (0, -1)):
                yield case_of(button_world(attributes, redraw=redraw, draws=draws, step=40) +
                              flow((0x10001, 0, inside), (0x10003, inside, 0), (0x10005, 0, inside), (0x10004, 0, inside), (0x10005, 0, inside),
                                   (0x10004, 0, inside), (0x10002, 0, 0)), q={"DefaultHandler": [4]})
    # a click and a drag away: out while held (bit 2 off), in again (bit 2 on), release away from the button and over it
    for attributes in (1, 0x11, 0x21):
        yield case_of(button_world(attributes, step=30) + flow((0x10001, 0, inside), (0x10005, 0, inside), (0x10002, 0, 0), (0x10004, 0, 0),
                                                                    (0x10001, 0, inside), (0x10005, 0, inside), (0x10002, 0, 0), (0x10001, 0, inside),
                                                                    (0x10004, 0, inside)))
        yield case_of(button_world(attributes, step=30) + flow((0x10005, 0, inside), (0x10002, 0, 0), (0x10002, 0, 0), (0x10001, 0, inside),
                                                                    (0x10001, 0, inside), (0x10004, 0, inside), (0x10004, 0, inside)))
    # the auto repeat: the frames after a press (the clock moves 60 every call: the delay passes, then the interval each frame or two)
    for step in (1, 30, 60, 130, 500, 1000):
        for attributes in (0x21, 0x31, 0x20, 0x23, 1):
            for delay, interval in ((500, 125), (0, 0), (100, 10), (-1, -1), (1000, 2000)):
                yield case_of(button_world(attributes, delay=delay, interval=interval, step=step, clock=7000) +
                              flow((0x10001, 0, inside), (0x10005, 0, inside)) + flow(*([(0x1E, 0, 0)] * 8)) +
                              flow((0x10002, 0, 0)) + flow(*([(0x1E, 0, 0)] * 3)) + flow((0x10001, 0, inside)) + flow(*([(0x1E, 0, 0)] * 3)) +
                              flow((0x10004, 0, inside)) + flow((0x1E, 0, 0)), q={"DefaultHandler": [1]})
    # a release after the hold fired (held state 2: StopHeldTimer answers 0, so no click is sent), and before it
    for held in (0, 1, 2):
        for attributes in (0x21, 0x31, 1):
            yield case_of(button_world(attributes, state=3, held=held, step=10) + [table(0, 1, 5, 6, 100)] + flow((0x10004, 0, inside)))
    # the right and middle buttons are the default handler's (and so is a second press while the first is held)
    for a in (1, 2, 3, -1):
        for message in (0x10004, 0x10005):
            yield case_of(button_world(0x21, step=5) + flow((message, a, inside), (message, a, inside)), q={"DefaultHandler": [8, 9]})
    yield case_of(button_world(0x21, step=5) + flow((0x10005, 0, inside), (0x10005, 0, inside), (0x10005, 0, inside), (0x10004, 0, inside)))
    # two buttons: the press on one while the pump's table is held by the other
    yield case_of(button_world(0x21, step=5) + [make(2, 2, 0, 0x21, 6, (200, 20, 300, 70)), button_data(2), draws_op(2, 1, 2)] +
                  tree_ops({0: -1, 1: 0, 2: 0}) +
                  flow((0x10005, 0, inside)) + [call(BUTTON, 2, 0x10001, 0, inside), call(BUTTON, 2, 0x10005, 0, inside), call(BUTTON, 1, 0x10002, 0, 0),
                                                call(BUTTON, 2, 0x10004, 0, inside), call(BUTTON, 1, 0x10004, 0, inside)])
    # the position the press records (x in the high half, y in the low, both signed)
    for b in POSITIONS:
        yield case_of(button_world(1, step=3) + flow((0x10005, 0, b)))
    # the focus: the press takes it from another window and from nobody
    for focus in (-1, 0, 1):
        yield case_of(button_world(1, focus=focus) + flow((0x10005, 0, inside), (0x10004, 0, inside), (0x10005, 0, inside)))
    # each message alone with the stock button
    for message in BUTTON_MESSAGES:
        for a in (0, 1, 2, -1):
            yield case_of(button_world(0x21, state=1, hilight=1, buttons_held=1) + flow((message, a, inside)), q={"DefaultHandler": [0, 4]})
            yield case_of(button_world(1) + flow((message, a, inside)), q={"DefaultHandler": [5]})


def button_random(r):
    sample = {name: r.choice(values) for name, values in BUTTON_DIMS.items()}
    return button_case_of(sample, r)


def button_edges():
    yield from button_sampled_edges()
    yield from button_flow_edges()


TESTS.append(FnTest("UI_DefaultButtonCallback", ui_ppc, ui_host, edge=button_edges, rnd=button_random))


# ======================================================================= test 7: scripts of 200 operations

# the handler and its messages by the kind of the window (the pump's: pointer, press, release, frame, the control's own)
HANDLER_OF = {2: BUTTON, 8: CHECK, 10: TEXTBOX, 11: KNOB, 12: VARY}
MESSAGES_OF = {
    2: [0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x1E, 0x10004, 0x10005, 0x1E, 0x10006, 0x15, 3],
    8: [0x100, 0x100, 0x100, 0x15, 3, 0x10005],
    10: [0x10001, 0x10005, 3, 0xC, 0x15],
    11: [0x10003, 0x10003, 0x10004, 0x10005, 0x10005, 3, 0x1E],
    12: [0x100, 0x10008, 0x10009, 0x1000D, 0x15, 3, 0x100, 0x10008, 0x10009],
    1: [3],
}


def long_world(r):
    """Twelve windows: a root (0), a button (1), check buttons (2) with three buttons (3 to 5), a knob (6), a vary box (7) with its two
    buttons (8 and 9), a text box (10) and a plain window (11); all set up at random."""
    ops = [root_op(), make(1, 2, 0, r.choice([1, 0x21, 0x11]), 5, RECT), make(2, 8, 0, r.choice([0x11, 1, 0x10]), 40, RECT)]
    ops += [make(3 + i, 2, 2, 1, i + 1, (0, 10 * i, 30, 10 * i + 8)) for i in range(3)]
    ops += [make(6, 11, 0, 1, 21, RECT), make(7, 12, 0, 1, 31, RECT), make(8, 2, 7, 1, 1, (0, 0, 10, 10)), make(9, 2, 7, 1, 2, (0, 20, 10, 30)),
            make(10, 10, 0, 1, 12, RECT), make(11, 1, 0, 1, 100, RECT)]
    ops += tree_ops({0: -1, 1: 0, 2: 0, 3: 2, 4: 2, 5: 2, 6: 0, 7: 0, 8: 7, 9: 7, 10: 0, 11: 0})
    for slot in (1, 3, 4, 5, 8, 9):
        ops += [button_data(slot, r.choice(STATES), r.choice([0, 1]), r.choice([0, 1, 2]), r.choice([500, 100]), r.choice([125, 10]),
                            r.choice([0, 100, 900]), r.choice(COLOURS), r.choice(COLOURS), r.choice(SHIFTS), r.choice(SHIFTS)), draws_op(slot, *r.choice(DRAW_PAIRS))]
    ranges = r.choice(KNOB_RANGES)
    ops.append(knob_data(6, ranges[0], ranges[1], None, r.choice([0, ranges[0], 3]), 0, ranges[2], ranges[3], None, r.choice([0, ranges[2]]), 0, 50, 50,
                         r.choice([0, 1])))
    ops.append(draws_op(6, -1, r.choice([-1, 0, 1])))
    vr = r.choice(VARY_RANGES)
    ops.append(vary_data(7, vr[0], vr[1], r.choice([vr[0], vr[1], 3, 50]), 0, r.choice([1, 1, 2]), r.choice([0, 1, 3]), r.choice([0, 1, 10]), r.choice([0, 1])))
    ops.append((8, (2, r.choice([-1, 1, 2, 3, 99]))))
    ops += [glob(1, r.choice([0, 1])), glob(4, 1000), glob(5, r.choice([0, 1, 40, 200]))]
    return ops


def long_op(r):
    """One operation of the script."""
    k = r.random()
    slot = r.randrange(12)
    kinds = {0: 1, 1: 2, 2: 8, 3: 2, 4: 2, 5: 2, 6: 11, 7: 12, 8: 2, 9: 2, 10: 10, 11: 1}
    kind = kinds[slot]
    if k < 0.40 and kind in HANDLER_OF:
        message = r.choice(MESSAGES_OF[kind] + [r.choice(MESSAGES)] * 1)
        a = r.choice([0, 0, 1, 2, 3, -1] + POSITIONS) if r.random() < 0.7 else r.choice([1, 2, 3, 4, 5, 0])
        if kind in (2, 11) and message in (0x10004, 0x10005):
            a = r.choice([0, 0, 0, 1, 2])
        if kind == 8 and message == 0x100:
            a = r.choice([1, 2, 3, 4, -1, 0])
        if kind == 12 and message in (0x100, 0x10008, 0x10009):
            a = r.choice([1, 2, 1, 2, 0, 3])
        return call(HANDLER_OF[kind], slot, message, a, r.choice(POSITIONS))
    if k < 0.48 and kind == 11:
        return (r.choice([40, 41]), (slot, r.choice([r.randint(-50, 150), r.getrandbits(32) - 0x80000000, 0, 100, 0x10064])))
    if k < 0.56 and kind == 12:
        return (42, (slot, r.choice([0, 1, 2, 3, 4, 5, 4, 5, 6, -1])))
    if k < 0.66:
        return (r.choice([20, 21, 22, 23, 24, 25, 22, 24, 25]), (slot,))
    if k < 0.74 and kind == 2:
        return button_data(slot, r.choice(STATES), r.choice([0, 1, -1]), r.choice([0, 1, 2]), r.choice([500, 0, 100]), r.choice([125, 0, 10]),
                           r.choice([0, 100, 900, 5000]), r.choice(COLOURS), r.choice(COLOURS), r.choice(SHIFTS), r.choice(SHIFTS))
    if k < 0.78 and kind == 11:
        ranges = r.choice(KNOB_RANGES)
        return knob_data(slot, ranges[0], ranges[1], None, r.randint(-5, 105), 0, ranges[2], ranges[3], None, r.randint(-5, 105), 0, r.randint(-5, 200),
                         r.randint(-5, 200), r.choice([0, 1]))
    if k < 0.82 and kind == 12:
        vr = r.choice(VARY_RANGES)
        return vary_data(slot, vr[0], vr[1], r.randint(-10, 110), 0, r.choice(INCREMENTS), r.choice([0, 1, 5, 20]), r.choice([0, 1, 10]), r.choice([0, 1]))
    if k < 0.85 and kind == 8:
        return (8, (slot, r.choice([-1, 1, 2, 3, 99])))
    if k < 0.88:
        return glob(r.choice([2, 3]), r.choice([-1] + list(range(12))))
    if k < 0.91:
        return glob(r.choice([1, 4, 5, 5]), r.choice([0, 1, 100, 1000, 40, -3, 0x7FFFFFFF]))
    if k < 0.94:
        return table(r.randrange(3), r.choice(TABLE_STATES), r.randint(-50, 50), r.randint(-50, 50), r.choice([0, 100, 5000]))
    if k < 0.97:
        return draws_op(slot, *r.choice(DRAW_PAIRS))
    return fields(slot, r.choice([1, 3, 0x11, 0x21, 0x31, 0x10, 0x20, 0x401]), r.choice(CURSORS), r.choice([0, 1, 2, 7, 0x80000000]), r.choice(IDS) if kind not in (3, 4, 5, 8, 9) else r.choice([1, 2, 3, 9]))


def long_script(r, length=200):
    ops = long_world(r)
    for _ in range(length):
        ops.append(long_op(r))
    return case_of(ops, q={"DefaultHandler": [r.choice([0, 1, -1, 99]) for _ in range(5)], "PostMessage": [r.choice([0, 1]) for _ in range(3)],
                           "VaryBox.GetValue": [r.randint(-50, 150), 0, 7], "VaryBox.SetValue": [1, 0]})


def long_edges():
    for seed in range(4):
        yield long_script(_random.Random(2000 + seed), 200)


TESTS.append(FnTest("scripts of 200 operations over every handler", ui_ppc, ui_host, edge=long_edges, rnd=lambda r: long_script(r, 200)))
