"""Differential tests for the control factory, the control constructors and the button, against
libtpw_unit_UIControls.dylib (tests/unit_UIControls.cpp):

  src/ui_controls_create.cpp   InterfaceWindow::CreateObject and the constructors of InterfaceBar, InterfaceCheckButtons,
                               InterfaceEditBox, InterfaceImage, InterfaceKnob, InterfaceLightPanel, InterfaceListBox,
                               InterfaceListControl2, InterfaceTextBox and InterfaceVaryBox
  src/InterfaceButton.cpp      InterfaceButton's constructor, destructor, CalcFrame, Enable, WhatAmI, GetState, DoTextColour,
                               SetState, HiLight, SetText, GetText, StopHeldTimer, SetAutoRepeat, SetTextUpColour,
                               SetTextDownColour and GetHelpText

Real on both sides: those, the window's constructor (src/InterfaceWindow.cpp) and the rectangles (src/UI_Base.cpp). Fakes that
record the same event on both sides: UI_PostMessage, UI_UpdatePointerIn, UI_AddTimer (answers a timer of the world from a queue),
the window's untranslated virtual members (DrawFunction, CalcClientArea and the three overrides of it the constructors reach,
the destructor, Enable, SetText), SetCallbackFunction (which also sets the handler), CaptureKb, Show, the string heap's
ReleaseString, the scroll bar (constructor, SetMinMax, Enable) and the draw objects (Change records the fields its flags name,
with the text style for the button).

A case is a script of operations on a world of eight window slots (any class), three draw objects, two timers and three strings.
After each operation the answer, every window reachable from the slots (each as a table of its fields by Mac offset: the whole
object, the untouched parts as a flag), the UI globals, the heap traffic and the length of the call log are compared, and the
whole call log at the end. The PowerPC side decides which operations the state allows (a slot already in use, a missing window,
a destructor of a window that is still linked) and the host side replays exactly those.

Quirks the tests pin: CreateObject answers NULL and makes nothing for kind 0, 14 and up and every negative number; only a plain
window (kind 1) with attribute 0x100 keeps its parent in the word at +0x60; the list box adds 0x80 to the scroll bar's
attributes and the list control does not; the edit box makes its 256 character buffer only with attribute 0x40; the button's
DoTextColour tells Change(0) with an unwritten style for the disabled frame; Enable answers 1 whatever non-zero thing the base
class answered.
"""
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIControls"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (kind numbers are CreateObject's)
FN = dict(CreateObject=0x101DC750, WindowCtor=0x101D98D8, MakeAbs=0x101DAB88, Button=0x101CADC0, ButtonDtor=0x101CAEA0,
          CalcFrame=0x101CAD20, Enable=0x101CAF48, WhatAmI=0x101CB010, GetState=0x101CB048, DoTextColour=0x101CB084,
          SetState=0x101CB1D0, HiLight=0x101CB2B8, SetText=0x101CB3C4, GetText=0x101CB440, StopHeld=0x101CB478,
          SetAutoRepeat=0x101CB4D4, SetUp=0x101CB518, SetDown=0x101CB584, GetHelp=0x101CBF40)
CTOR = {1: 0x101D98D8, 2: 0x101CADC0, 3: 0x101D636C, 4: 0x101D0C50, 5: 0x101CD4C4, 6: 0x101CFE3C, 7: 0x101D15CC, 8: 0x101CBF90,
        9: 0x101C86C4, 10: 0x101D7BF0, 11: 0x101D008C, 12: 0x101D8C94, 13: 0x101D08F0}
# what they call that is real too: the rectangles and the regions' base
REAL_HELPERS = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788,
                0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74]
# the fakes (in-binary functions)
POST_MESSAGE, UPDATE_POINTER_IN, ADD_TIMER = 0x101C9400, 0x101C8F18, 0x101D878C
WINDOW_CALC, EDIT_CALC, LISTBOX_CALC, LISTCTL_CALC = 0x101D987C, 0x101CD468, 0x101D0B98, 0x101D154C
WINDOW_DTOR, WINDOW_ENABLE, SCROLL_ENABLE, WINDOW_SETTEXT = 0x101D9BA4, 0x101DC320, 0x101D658C, 0x101DE248
SET_CALLBACK, CAPTURE_KB, WINDOW_SHOW, SCROLL_SETMINMAX, SCROLL_CTOR = 0x101DCBA8, 0x101DCCEC, 0x101DC1EC, 0x101D6B44, 0x101D636C
RELEASE_STRING = 0x101D7B10

# ---- the original's data
G = dict(redraw=0x10395C28, hebrew=0x10395B88, last=0x10395C18)
CORE_VTABLE = 0x102AF8FC   # CoolRect's vtable
# the vtables by kind
VTABLE = {1: 0x102A7D94, 2: 0x102AF974, 3: 0x102A7D5C, 4: 0x102A7D10, 5: 0x102A7CC4, 6: 0x102AFAEC, 7: 0x102AFBFC, 8: 0x102AF9C8,
          9: 0x102A7C78, 10: 0x102AFD14, 11: 0x102AFB40, 12: 0x102AFD68, 13: 0x102AFB94}
KIND_OF_VTABLE = {v: k for k, v in VTABLE.items()}
# the sizes of the objects: the operator new arguments in CreateObject
MAC_SIZE = {1: 0x138, 2: 0x15C, 3: 0x15C, 4: 0x158, 5: 0x168, 6: 0x140, 7: 0x178, 8: 0x13C, 9: 0x154, 10: 0x150, 11: 0x168,
            12: 0x158, 13: 0x13C}
# the window handlers (TVectors) by the numbers the host's CallbackCode gives
HANDLER_CODE = {0x10268480: 1, 0x10268110: 2, 0x10268128: 3, 0x102681C0: 4, 0x10268298: 5, 0x102682D0: 6, 0x102682F0: 7,
                0x102683F8: 8, 0x10268440: 9}
KINDS = range(1, 14)

# ---- labels (the host's tpw_ctl_labels)
LAB_WINDOW, LAB_DRAW, LAB_TIMER, LAB_STRING = 10, 20, 40, 50
SLOTS, DRAWS, TIMERS, STRINGS = 8, 3, 2, 3
MAX_WINDOWS = 40
MAX_QUEUE = 64
STRING_TEXTS = ("abc", "x", "")


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


# ======================================================================= the tables of fields (Mac offset, type)
# Types: u8 s8 s16 s32 u32 (numbers), lab (a pointer as its label), cb (a handler as its number), kind (the class from the
# vtable), region (0 none, 1 the window's own rectangle, else a label), vptr (a CoolRect's vtable is right), ptr (not NULL),
# rawN (N bytes all untouched: 0xa5), zero64, const1.


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

EXTRA = {
    2: [(0x138, "s8"), (0x139, "s8"), (0x13A, "raw2"), (0x13C, "s32"), (0x140, "s32"), (0x144, "raw4"), (0x148, "lab"),
        (0x14C, "u32"), (0x150, "raw4"), (0x154, "u32"), (0x158, "s8"), (0x159, "s8"), (0x15A, "u8"), (0x15B, "raw1")],
    3: [(0x138, "raw36")],
    4: core(0x138) + [(0x144, "s32"), (0x148, "s32"), (0x14C, "s32"), (0x150, "s32"), (0x154, "s16"), (0x156, "s16")],
    5: [(0x138, "ptr"), (0x13C, "s32"), (0x140, "s32"), (0x144, "s32"), (0x148, "s16"), (0x14A, "s16"), (0x14C, "s16"),
        (0x14E, "s16"), (0x150, "s16"), (0x152, "s16"), (0x154, "s16"), (0x156, "s16"), (0x158, "s16"), (0x15A, "raw2"),
        (0x15C, "s32"), (0x160, "u8"), (0x161, "u8"), (0x162, "u8"), (0x163, "raw1"), (0x164, "lab"), (0x2138, "buf0"),
        (0x2139, "bufraw")],
    6: [(0x138, "raw4"), (0x13C, "raw4")],
    7: core(0x138) + [(0x144, "s32"), (0x148, "s32"), (0x14C, "s32"), (0x150, "s32"), (0x154, "s32"), (0x158, "s32"), (0x15C, "raw4"),
                      (0x160, "s32"), (0x164, "s16"), (0x166, "s16"), (0x168, "s16"), (0x16A, "s16"), (0x16C, "s32"),
                      (0x170, "s16"), (0x172, "s16"), (0x174, "s32")],
    8: [(0x138, "s32")],
    9: [(0x138, "s32"), (0x13C, "s32"), (0x140, "s32"), (0x144, "raw4")] + core(0x148),
    10: [(0x138, "ptr"), (0x13C, "s32"), (0x140, "s32"), (0x144, "s32"), (0x148, "s32"), (0x14C, "s16"), (0x14E, "s16")],
    11: [(0x138 + 4 * i, "s32") for i in range(10)] + [(0x160, "raw4"), (0x164, "s16"), (0x166, "raw2")],
    12: [(0x138, "s32"), (0x13C, "s32"), (0x140, "s32"), (0x144, "s32"), (0x148, "s32"), (0x14C, "raw4"), (0x150, "raw4"),
         (0x154, "u8"), (0x155, "raw3")],
    13: [(0x138, "s32")],
}




def check_tables():
    """The tables cover every byte of the object, once: the window's 0x138 bytes and each class's own up to its size."""
    sizes = {"u8": 1, "s8": 1, "s16": 2, "s32": 4, "u32": 4, "lab": 4, "cb": 4, "kind": 4, "region": 4, "vptr": 4, "ptr": 4,
             "const1": 1, "zero64": 0x40}

    def width(how):
        return int(how[3:]) if how.startswith("raw") else sizes[how]

    def tiles(fields, start, end):
        at = start
        for offset, how in sorted(f for f in fields if f[0] < 0x2000):
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

    ADDRESSES = [(G["redraw"], 4), (G["hebrew"], 1), (G["last"], 4)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.slots = [0] * SLOTS            # the window in each slot (0: none)
        self.heap = [0, 0, 0, 0]            # blocks handled by operator new, new[], delete, delete[] while a function runs
        self.observe = False                # true while a function under test runs: new blocks are filled and named
        self.sizes = {}                     # the size of every object operator new made
        self.draws = []
        for _ in range(DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.uic_draw_vtable)
            self.draws.append(block)
        self.timers = [e.alloc(0x20, b"\xa5" * 0x20) for _ in range(TIMERS)]
        self.strings = [CL.new_wide(e, text) for text in STRING_TEXTS]
        fixed = {a: LAB_DRAW + i for i, a in enumerate(self.draws)}
        fixed.update({a: LAB_TIMER + i for i, a in enumerate(self.timers)})
        fixed.update({a: LAB_STRING + i for i, a in enumerate(self.strings)})
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues)
        # the globals as the Mac's data has them
        w32(e, G["redraw"], 0)
        e.write(G["hebrew"], bytes(1))
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

    def kind_at(self, slot):
        return KIND_OF_VTABLE.get(self.e.u32(self.slots[slot] + 0x118), 0) if self.made(slot) else 0

    def begin(self):
        self.observe = True

    def end(self):
        self.observe = False

    def referenced(self, w):
        """True when another window points at this one (as parent, child or sibling): a destructor must not be run on it, the
        others would be left with a dangling link (and the host would reuse the address)."""
        e = self.e
        return any(e.u32(x + offset) == w for x in self.discover() if x != w for offset in (0x50, 0x54, 0x58, 0x5C))

    # ---- the dumps (the tables are BASE and EXTRA; the host's are tpw_ctl_state)
    def field(self, w, offset, kind):
        e, L = self.e, self.L
        raw = lambda n: int(bytes(e.read(w + offset, n)) == b"\xa5" * n)   # noqa: E731
        if kind == "u8":
            return e.read(w + offset, 1)[0]
        if kind == "s8":
            return T.s8(e.read(w + offset, 1)[0])
        if kind == "s16":
            return struct.unpack(">h", e.read(w + offset, 2))[0]
        if kind == "s32":
            return T.s32(e.u32(w + offset))
        if kind == "u32":
            return e.u32(w + offset)
        if kind == "lab":
            return L.lab(e.u32(w + offset))
        if kind == "cb":
            value = e.u32(w + offset)
            return 0 if value == 0 else HANDLER_CODE.get(value, 99)
        if kind == "kind":
            return KIND_OF_VTABLE.get(e.u32(w + offset), 0)
        if kind == "region":
            value = e.u32(w + offset)
            return 0 if value == 0 else 1 if value == w + 4 else L.lab(value)
        if kind == "vptr":
            return int(e.u32(w + offset) == CORE_VTABLE)
        if kind == "ptr":
            return int(e.u32(w + offset) != 0)
        if kind == "zero64":
            return int(bytes(e.read(w + offset, 0x40)) == bytes(0x40))
        if kind == "const1":
            return 1
        if kind == "buf0":
            buffer = e.u32(w + 0x138)
            return struct.unpack(">H", e.read(buffer, 2))[0] if buffer else 0
        if kind == "bufraw":
            buffer = e.u32(w + 0x138)
            return int(bytes(e.read(buffer + 2, 0x1FE)) == b"\xa5" * 0x1FE) if buffer else 1
        if kind.startswith("raw"):
            return raw(int(kind[3:]))
        raise ValueError(kind)

    def dump_window(self, w):
        kind = KIND_OF_VTABLE.get(self.e.u32(w + 0x118), 0)
        return self.L.lab(w), {offset: self.field(w, offset, how) for offset, how in BASE + EXTRA.get(kind, [])}

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

    def dump_state(self):
        dumps = [self.dump_window(w) for w in self.discover()]
        e, L = self.e, self.L
        return dict(order=[label for label, _ in dumps], windows={label: fields for label, fields in dumps},
                    globals=[L.lab(e.u32(G["last"])), T.s32(e.u32(G["redraw"])), e.read(G["hebrew"], 1)[0]], heap=list(self.heap),
                    log=len(L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        e = self.e
        zero = [0, 0, 0, 0, 0]
        if code == 1:   # a plain window: slot, parent slot, attributes, id, left, top, right, bottom
            slot, parent = a[0], a[1]
            if self.made(slot) or (parent >= 0 and not self.made(parent)) or parent >= slot:
                return None
            block = e.alloc(MAC_SIZE[1], b"\xa5" * MAC_SIZE[1])
            self.L.labels[block] = LAB_WINDOW + slot
            self.call(FN["WindowCtor"], block, self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]), s16(a[7]))
            self.slots[slot] = block
            return [self.L.lab(block), 0, 0, 0, 0]
        if code in (2, 3, 4, 7) or code >= 20:
            if not self.made(a[0]):
                return None
        if code == 2:   # links: slot, parent, previous, next, child
            w = self.slots[a[0]]
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
            return zero
        if code == 3:   # fields: slot, attributes, help text, second help text
            w = self.slots[a[0]]
            w32(e, w + 0x44, a[1])
            w32(e, w + 0x8C, a[2])
            w32(e, w + 0x90, a[3])
            return zero
        if code == 4:   # draw objects: slot, the window's, the client's
            w = self.slots[a[0]]
            w32(e, w + 0x94, self.draws[a[1]] if a[1] >= 0 else 0)
            w32(e, w + 0x98, self.draws[a[2]] if a[2] >= 0 else 0)
            return zero
        if code == 5:   # a control through its constructor: slot, kind, parent slot, attributes, id, left, top, right, bottom
            slot, kind, parent = a[0], a[1], a[2]
            if self.made(slot) or not 1 <= kind <= 13 or (parent >= 0 and not self.made(parent)):
                return None
            size = MAC_SIZE[kind]
            block = e.alloc(size, b"\xa5" * size)
            self.L.labels[block] = LAB_WINDOW + slot
            self.begin()
            self.call(CTOR[kind], block, self.window(parent), a[3], a[4], s16(a[5]), s16(a[6]), s16(a[7]), s16(a[8]))
            self.end()
            self.slots[slot] = block
            return [self.L.lab(block), 0, 0, 0, 0]
        if code == 6:   # CreateObject: result slot, parent slot, kind, attributes, id, left, top, right, bottom
            slot, parent, kind = a[0], a[1], T.s32(a[2] & MINUS)
            if self.made(slot) or not self.made(parent):
                return None
            self.begin()
            made = self.call(FN["CreateObject"], self.window(parent), kind, a[3], a[4], s16(a[5]), s16(a[6]), s16(a[7]), s16(a[8]))
            self.end()
            if made != 0:
                assert 1 <= kind <= 13, f"kind {kind} made an object"
                assert self.sizes[made] == MAC_SIZE[kind], f"kind {kind}: operator new asked for {self.sizes[made]:#x}"
                self.slots[slot] = made
            else:
                assert not 1 <= kind <= 13, f"kind {kind} made nothing"
            return [self.L.lab(made), 0, 0, 0, 0]
        if code == 7:   # a button's data: slot, state, highlight, held, delay, interval, text, up word, down word, shift x, shift y
            if self.kind_at(a[0]) != 2:
                return None
            w = self.slots[a[0]]
            e.write(w + 0x138, struct.pack(">bb", T.s8(a[1] & 0xFF), T.s8(a[2] & 0xFF)))
            e.write(w + 0x15A, bytes([a[3] & 0xFF]))
            w32(e, w + 0x13C, a[4])
            w32(e, w + 0x140, a[5])
            w32(e, w + 0x148, self.strings[a[6]] if a[6] >= 0 else 0)
            w32(e, w + 0x14C, a[7])
            w32(e, w + 0x154, a[8])
            e.write(w + 0x158, struct.pack(">bb", T.s8(a[9] & 0xFF), T.s8(a[10] & 0xFF)))
            return zero
        if code == 8:   # globals: the UI's auto redraw, whether the UI is right to left
            w32(e, G["redraw"], a[0])
            e.write(G["hebrew"], bytes([1 if a[1] else 0]))
            return zero
        return self.button(code, a)

    def button(self, code, a):
        """The button's members: the slot in a[0] must hold a button."""
        if self.kind_at(a[0]) != 2:
            return None
        w = self.slots[a[0]]
        if code == 34 and self.referenced(w):
            return None
        answer = [0, 0, 0, 0, 0]
        self.begin()
        if code == 20:
            answer[0] = T.s32(self.call(FN["CalcFrame"], w))
        elif code == 21:
            answer[0] = T.s32(self.call(FN["Enable"], w, s16(a[1])))
        elif code == 22:
            self.call(FN["DoTextColour"], w)
        elif code == 23:
            answer[0] = T.s32(self.call(FN["SetState"], w, T.s8(a[1] & 0xFF), T.s8(a[2] & 0xFF)))
        elif code == 24:
            answer[0] = T.s32(self.call(FN["HiLight"], w, T.s8(a[1] & 0xFF), T.s8(a[2] & 0xFF)))
        elif code == 25:
            answer[0] = T.s32(self.call(FN["SetText"], w, self.strings[a[1]] if a[1] >= 0 else 0))
        elif code == 26:
            answer[0] = self.L.lab(self.call(FN["GetText"], w))
        elif code == 27:
            answer[0] = T.s32(self.call(FN["StopHeld"], w))
        elif code == 28:
            self.call(FN["SetAutoRepeat"], w, a[1], a[2])
        elif code == 29:
            self.call(FN["SetUp"], w, a[1])
        elif code == 30:
            self.call(FN["SetDown"], w, a[1])
        elif code == 31:
            answer[0] = T.s32(self.call(FN["GetHelp"], w))
        elif code == 32:
            answer[0] = T.s32(self.call(FN["GetState"], w))
        elif code == 33:
            answer[0] = T.s32(self.call(FN["WhatAmI"], w))
        elif code == 34:   # the destructor: a positive flag also deletes the object; the slot is empty after
            self.call(FN["ButtonDtor"], w, s16(a[1]))
            self.slots[a[0]] = 0
        else:
            raise ValueError(code)
        self.end()
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects' vtable, in persistent memory."""
    if hasattr(e, "uic_draw_vtable"):
        return

    def change(emu):
        # Change(flags, record): the frame for flag 1; for the text flags (0x4c000) the style the record points at
        this, flags, record = emu.r(3), emu.r(4), emu.r(5)
        frame, shift_x, shift_y, colour = 0, 0, 0, 0
        if flags & 1:
            frame = struct.unpack(">h", emu.read(record + 4, 2))[0]
        if flags & 0x4C000:
            style = emu.u32(record + 0x10)
            shift_x, shift_y = struct.unpack(">2h", emu.read(style + 0x0A, 4))
            colour = emu.u32(style + 0x10)
        emu.cl.rec("Change", (emu.cl.lab(this), flags, frame, shift_x, shift_y, colour))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.uic_draw_vtable = vt


def install_nested_hook(emu, address):
    """Like T.stub_internal, for a stub that makes PowerPC calls of its own: fn(emu, call) runs outside the Unicorn hook (the
    way T.stub_cb services imports), and the function then returns to its caller. Stubs.activate puts the fn in
    emu.internal_stubs for the case."""
    if not hasattr(emu, "internal_stubs"):
        emu.internal_stubs, emu.internal_hooks = {}, {}
    if address in emu.internal_hooks:
        return

    def hook(uc, where, size, user):
        fn = emu.internal_stubs.get(where)
        if fn is None:
            return
        emu.pending_cb = fn
        uc.reg_write(P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)

    emu.internal_hooks[address] = emu.uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)


def copy_rect(e, this, to):
    """The window's rectangle (the coordinates at +8) copied to the rectangle whose coordinates start at `to`."""
    e.write(this + to, bytes(e.read(this + 8, 8)))


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL_HELPERS)
    S.real.update(CTOR[k] for k in KINDS if k != 3)
    R = CL.recorder

    # the heap: blocks made while a function runs are filled with 0xa5 and named (objects), and counted
    def op_new(e):
        us, size = e.us, e.r(3)
        if us.observe:
            block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
            us.heap[0] += 1
            us.L.newlab(block)
            us.sizes[block] = size
        else:
            block = e.alloc(max(size, 4))
        e.set_r(3, block)

    def op_new_array(e):
        us, size = e.us, e.r(3)
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4)) if us.observe else e.alloc(max(size, 4))
        if us.observe:
            us.heap[1] += 1
        e.set_r(3, block)

    def op_delete(e):
        us, block = e.us, e.r(3)
        if block != 0 and us.observe:
            us.heap[2] += 1
        if block != 0 and us.L.lab(block) > 0:
            us.L.rec("delete", (us.L.lab(block),))
            us.L.forget(block)

    def op_delete_array(e):
        if e.r(3) != 0 and e.us.observe:
            e.us.heap[3] += 1

    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_array)
    S.imp("__dl__FPv", op_delete)
    S.imp("__dla__FPv", op_delete_array)

    # the senders and the timers
    def timer(e, L):
        k = L.ret("AddTimer")
        return e.us.timers[k - 1] if 1 <= k <= TIMERS else 0

    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(ADD_TIMER, R("AddTimer", "Lii", ret=timer))

    # CalcClientArea of the window and of the three classes whose constructors reach their own
    def window_calc(e, L):
        copy_rect(e, e.r(3), 0x2C)

    def list_box_calc(e, L):
        this = e.r(3)
        copy_rect(e, this, 0x2C)
        left, top, right, bottom = struct.unpack(">4h", e.read(this + 0x2C, 8))
        right = s16(right - 0x30)
        e.write(this + 0x2C, struct.pack(">4h", left, top, right, bottom))
        e.write(this + 0x13C, struct.pack(">4h", s16(left + 4), s16(top + 4), s16(right - 4), s16(bottom - 4)))

    def list_control_calc(e, L):
        this = e.r(3)
        copy_rect(e, this, 0x2C)
        copy_rect(e, this, 0x13C)

    S.internal(WINDOW_CALC, R("CalcClientArea", "L", then=window_calc))
    S.internal(EDIT_CALC, R("EditBox.CalcClientArea", "L", then=window_calc))
    S.internal(LISTBOX_CALC, R("ListBox.CalcClientArea", "L", then=list_box_calc))
    S.internal(LISTCTL_CALC, R("ListControl2.CalcClientArea", "L", then=list_control_calc))

    # the window's other untranslated members
    S.internal(WINDOW_DTOR, R("Window.dtor", "L"))
    enable = R("Window.Enable", "Lh", ret="h")
    S.internal(WINDOW_ENABLE, enable)
    S.internal(SCROLL_ENABLE, enable)
    S.internal(WINDOW_SETTEXT, R("Window.SetText", "LL", ret=lambda e, L: 1))
    S.internal(CAPTURE_KB, R("CaptureKb", "Li", ret=lambda e, L: 1))
    S.internal(WINDOW_SHOW, R("Window.Show", "Lh", ret=lambda e, L: 1))
    S.internal(RELEASE_STRING, R("ReleaseString", "L"))

    # SetCallbackFunction records the handler's number and, like the original, sets the handler when it is not NULL
    def set_callback(e):
        this, handler = e.r(3), e.r(4)
        e.cl.rec("SetCallbackFunction", (e.cl.lab(this), 0 if handler == 0 else HANDLER_CODE.get(handler, 99)))
        if handler != 0:
            w32(e, this + 0x110, handler)
        e.set_r(3, 1)

    S.internal(SET_CALLBACK, set_callback)

    # the scroll bar: a window of the scroll bar's class, then the event (the host's fake constructs the base class first too)
    S.internal(SCROLL_SETMINMAX, R("ScrollBar.SetMinMax", "Lii"))

    def scroll_ctor(e, call):
        L = e.cl
        this, parent, attributes, ident = e.r(3), e.r(4), T.s32(e.r(5)), T.s32(e.r(6))
        left, top, right, bottom = (T.s16(e.r(r)) for r in (7, 8, 9, 10))
        call(("code", FN["WindowCtor"]), this, parent, attributes, ident, left, top, right, bottom)
        w32(e, this + 0x118, VTABLE[3])
        L.rec("ScrollBar.ctor", (L.lab(this), L.lab(parent), attributes, ident, left, top, right, bottom))
        return this

    install_nested_hook(emu, SCROLL_CTOR)
    S.internal(SCROLL_CTOR, scroll_ctor)
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uic_open", False):
        return
    import ctypes as C
    lib.tpw_ctl_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_ctl_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ctl_state.restype = C.c_int32
    lib.tpw_ctl_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ctl_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._uic_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        open_host(lib)
        lib.tpw_ctl_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues)
        lib.tpw_ctl_labels()

    def op(self, code, a):
        C = self.C
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 5)()
        self.lib.tpw_ctl_op(code, args, out)
        return list(out)

    def dump_state(self):
        C, lib = self.C, self.lib
        buf = (C.c_int64 * 65536)()
        count = lib.tpw_ctl_state(buf)
        words = list(buf[:count])
        order, windows, i = [], {}, 0
        while i < count:
            label, pairs = words[i], words[i + 1]
            fields = {words[i + 2 + 2 * k]: words[i + 3 + 2 * k] for k in range(pairs)}
            assert len(fields) == pairs, f"window {label}: a field was written twice"
            order.append(label)
            windows[label] = fields
            i += 2 + 2 * pairs
        lib.tpw_ctl_globals(buf)
        globals_ = list(buf[:3])
        lib.tpw_ctl_heap(buf)
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

COLOURS = [0xFFFFFFFF, 0x000000FF, 0, 0x01020304, 0x80FF7F00, 0xFF0000FF, 0x00FF00FF, 0x0000FFFF]
SHIFTS = [0, 1, -1, 2, 126, 127, -128, -127]
STATES = [0, 1, 2, 3, -1, 127, -128, 4, 0x55]
RECTS = [(0, 0, 0, 0), (10, 20, 110, 220), (-5, -7, 3, 4), (0, 0, 1, 1), (100, 100, 50, 40), (0x7FF0, 0x7FF0, 0x7FFF, 0x7FFF),
         (-0x8000, -0x8000, -0x7FF0, -0x7FF0), (5, 6, 0x7FFE, 7), (0x7FFC, 0, 0x7FFE, 9)]


def window_op(slot, parent, attributes=1, ident=100, rect=(0, 0, 100, 100)):
    """A plain window."""
    return (1, (slot, parent, attributes, ident) + tuple(rect))


def construct(slot, kind, parent, attributes, ident, rect):
    """A control through its constructor."""
    return (5, (slot, kind, parent, attributes, ident) + tuple(rect))


def create(slot, parent, kind, attributes, ident, rect):
    """A control through CreateObject, as a child of `parent`."""
    return (6, (slot, parent, kind, attributes, ident) + tuple(rect))


def button_data(slot, state=0, hilight=0, held=0, delay=500, interval=125, text=-1, up=0xFFFFFFFF, down=0x000000FF, shift_x=1, shift_y=1):
    return (7, (slot, state, hilight, held, delay, interval, text, up, down, shift_x, shift_y))


def draws_op(slot, window_draw, client_draw):
    return (4, (slot, window_draw, client_draw))


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


def world(parents, rects=None, attributes=None):
    """Plain windows 0.. with these parents (-1 for the root, slot 0 only), linked into lists of children."""
    n = len(parents)
    rects = rects or [(s * 10, s * 8, s * 10 + 60, s * 8 + 50) for s in range(n)]
    attributes = attributes or [1] * n
    ops = [window_op(s, parents[s], attributes[s], 100 + s, rects[s]) for s in range(n)]
    return ops + link_ops(parents)


def case_of(ops, q=None):
    case = dict(ops=ops)
    if q:
        case["q"] = q
    return case


def rnd_rect(r):
    if r.random() < 0.3:
        return r.choice(RECTS)
    left, top = r.randint(-30, 300), r.randint(-30, 300)
    return (left, top, left + r.choice([0, 1, r.randint(-5, 400), r.randint(1, 100)]), top + r.choice([0, 1, r.randint(-5, 400), r.randint(1, 100)]))


def rnd_attributes(r):
    """Mostly small sets of the bits the constructors read (1, 2, 0x40, 0x80, 0x100, 0x400, 0x1000), some any 32 bits."""
    if r.random() < 0.15:
        return r.getrandbits(32)
    value = 0
    for bit in (1, 2, 0x10, 0x40, 0x80, 0x100, 0x400, 0x1000, 0x10000000):
        if r.random() < 0.35:
            value |= bit
    return value


def rnd_id(r):
    return r.choice([0, 1, -1, 100, 0x7FFFFFFF, -0x80000000, r.randint(-1000, 1000)])


def rnd_queues(r):
    return {"AddTimer": [r.choice([0, 1, 2, 3]) for _ in range(4)], "Window.Enable": [r.choice([0, 1, -1, 0x10000, 5]) for _ in range(4)],
            "PostMessage": [1, 0]}


# ======================================================================= test 1: the constructors

CTOR_ATTRIBUTES = [0, 1, 2, 0x40, 0x41, 0x400, 0x401, 0x1000, 0x1040, 0x1001, 0x1400, 0x100, 0xFFFFFFFF, 0x80000000, 0x10, 0x90]


def constructor_edges():
    root = [window_op(0, -1, 1, 7, (10, 20, 400, 300))]
    for kind in KINDS:
        for attributes in CTOR_ATTRIBUTES:
            for rect_ in (RECTS[1], RECTS[0], RECTS[5]):
                yield case_of(root + [construct(1, kind, 0, attributes, 55, rect_)], q={"AddTimer": [1], "Window.Enable": [1]})
        # no parent, and a parent whose own rectangle is shifted (attribute 0x400 makes the rectangle absolute)
        yield case_of([construct(0, kind, -1, 0, -1, (1, 2, 3, 4))])
        yield case_of(root + [construct(1, kind, 0, 0x400, 9, (3, 4, 30, 40)), construct(2, kind, 1, 0x401, 9, (3, 4, 30, 40))])
    # the edit box's timer and the right to left flag
    for timer in (0, 1, 2, 3):
        for hebrew in (0, 1):
            yield case_of([(8, (0, hebrew)), construct(0, 5, -1, 0x40, 1, RECTS[1])], q={"AddTimer": [timer]})
    # the constructors one after the other, as the globals show the last window made
    yield case_of(root + [construct(i, kind, 0, 1, i, RECTS[1]) for i, kind in enumerate(KINDS, start=1) if i < 8])


def constructor_random(r):
    ops = [window_op(0, -1, r.choice([1, 0x400, 0x401]), 7, rnd_rect(r))]
    slots = [0]
    for _ in range(r.randint(1, 6)):
        free = [s for s in range(SLOTS) if s not in slots]
        if not free:
            break
        slot = r.choice(free)
        ops.append(construct(slot, r.choice(list(KINDS)), r.choice(slots + [-1]), rnd_attributes(r), rnd_id(r), rnd_rect(r)))
        slots.append(slot)
    ops.insert(1, (8, (r.choice([0, 1, 5]), r.choice([0, 1]))))
    return case_of(ops, q=rnd_queues(r))


TESTS.append(FnTest("the eleven constructors", ui_ppc, ui_host, edge=constructor_edges, rnd=constructor_random))


# ======================================================================= test 2: CreateObject

CREATE_KINDS = list(range(-3, 18)) + [0x7FFFFFFF, -0x80000000, 0xFFFF, 0x100, 0x10000000D, 0xD]
CREATE_ATTRIBUTES = [0, 1, 0x100, 0x101, 0x40, 0x1000, 0x1040, 0x1100, 0xFFFFFFFF, 0x400, 0x401, 0x80, 0x10]


def create_edges():
    parents = {
        "none": [window_op(0, -1, 1, 7, (10, 20, 400, 300))],
        "one": world([-1, 0]),
        "three": world([-1, 0, 0, 0]),
        "chain": world([-1, 0, 1]),
    }
    for kind in CREATE_KINDS:
        for name, base in parents.items():
            yield case_of(base + [create(SLOTS - 1, 0, kind, 0x101, 5, RECTS[1])], q={"AddTimer": [1], "Window.Enable": [1]})
        for attributes in CREATE_ATTRIBUTES:
            yield case_of(parents["one"] + [create(SLOTS - 1, 0, kind, attributes, -7, RECTS[2])])
    # the lists make their scroll bar: every combination of the two attributes, the places it takes from the rectangle
    for kind in (4, 7):
        for attributes in (0, 0x1000, 0x1040, 0x40, 0x1001, 0x1400):
            for rect_ in RECTS:
                yield case_of(parents["three"] + [create(SLOTS - 1, 0, kind, attributes, 3, rect_)])
    # controls made inside controls, and several under one parent (the list of children grows at its end)
    yield case_of(world([-1]) + [create(1, 0, 1, 0x100, 1, RECTS[1]), create(2, 1, 2, 0, 2, RECTS[2]), create(3, 1, 5, 0x40, 3, RECTS[1]),
                                 create(4, 1, 4, 0x1000, 4, RECTS[1]), create(5, 4, 9, 0, 5, RECTS[2]), create(6, 0, 13, 0, 6, RECTS[3]),
                                 create(7, 0, 8, 0, 7, RECTS[4])], q={"AddTimer": [2]})
    # a panel with a button created into it (the pointer update and the posted message of the plain window constructor)
    yield case_of([window_op(0, -1, 0x401, 1, (0, 0, 640, 480)), create(1, 0, 1, 0x401, 2, (10, 10, 200, 100)), create(2, 1, 2, 0x401, 3, (5, 5, 50, 20))])


def create_random(r):
    base = r.choice([world([-1]), world([-1, 0]), world([-1, 0, 0]), world([-1, 0, 1, 1])])
    n = len([op for op in base if op[0] == 1])
    ops = list(base)
    made = set(range(n))
    for _ in range(r.randint(1, 8)):
        free = [s for s in range(SLOTS) if s not in made]
        if not free:
            break
        slot = r.choice(free)
        kind = r.choice([r.randint(-2, 15), r.randint(1, 13), r.randint(1, 13)])
        ops.append(create(slot, r.choice(sorted(made)), kind, rnd_attributes(r), rnd_id(r), rnd_rect(r)))
        if 1 <= kind <= 13:
            made.add(slot)
    return case_of(ops, q=rnd_queues(r))


TESTS.append(FnTest("InterfaceWindow::CreateObject", ui_ppc, ui_host, edge=create_edges, rnd=create_random))


# ======================================================================= test 3: the button's members


def button_world(state=0, hilight=0, attributes=1, draws=(-1, -1), text=-1, **keywords):
    """A root window and a button (slot 1) made with the constructor, then set up."""
    return [window_op(0, -1, 1, 7, (0, 0, 300, 200)), construct(1, 2, 0, attributes, 5, (10, 10, 90, 40)), draws_op(1, *draws),
            button_data(1, state=state, hilight=hilight, text=text, **keywords)]


DRAW_PAIRS = [(-1, -1), (0, -1), (-1, 1), (0, 1), (2, 2)]


def button_edges():
    # the frame: every state, highlight and the disabled bit
    for state in STATES:
        for hilight in (0, 1, -1, 5):
            for attributes in (1, 3, 2, 0x401):
                yield case_of(button_world(state, hilight, attributes) + [(20, (1,)), (32, (1,)), (33, (1,)), (31, (1,))])
    # the text style: every frame, with the colours and the shifts, with and without a client draw object
    for state in (0, 1, 2, 3):
        for hilight in (0, 1):
            for attributes in (1, 2):
                for draws in DRAW_PAIRS:
                    for up, down in ((0xFFFFFFFF, 0x000000FF), (0x01020304, 0x05060708)):
                        for shifts in ((1, 1), (-3, 4), (127, -128)):
                            yield case_of(button_world(state, hilight, attributes, draws, up=up, down=down, shift_x=shifts[0], shift_y=shifts[1])
                                          + [(22, (1,))])
    # SetState and HiLight: same value and new, redraw 0 and 1 and -1, the auto redraw on and off
    for redraw_flag in (0, 1):
        for state in (0, 1, 3, -128):
            for new in (0, 1, 2, 3, -128, 0x100):
                for redraw in (0, 1, -1):
                    for draws in ((-1, -1), (0, 1)):
                        yield case_of(button_world(state, 0, 1, draws) + [(8, (redraw_flag, 0)), (23, (1, new, redraw)), (23, (1, new, redraw)),
                                                                          (24, (1, new, redraw)), (24, (1, 0, redraw))])
    # Enable: the base class answers 0, 1, -1 or a number with the high bits set
    for answer in (0, 1, -1, 0x10000, 2):
        for draws in DRAW_PAIRS:
            for state in (0, 1, 2):
                yield case_of(button_world(state, 1, 1, draws) + [(21, (1, 0)), (21, (1, 1)), (21, (1, -1))], q={"Window.Enable": [answer, 1 - answer]})
    # the text: set with a draw object or none, read back, set to NULL; the setters and the help text
    for draws in DRAW_PAIRS:
        yield case_of(button_world(0, 0, 1, draws) + [(25, (1, 0)), (26, (1,)), (25, (1, 2)), (26, (1,)), (25, (1, -1)), (26, (1,)), (25, (1, 1))])
    for held in (0, 1, 2, 3, 255):
        yield case_of(button_world() + [button_data(1, held=held), (27, (1,)), (27, (1,))])
    for first, second in ((0, 0), (500, 125), (-1, 0x7FFFFFFF), (0x7FFFFFFF, -0x80000000)):
        yield case_of(button_world() + [(28, (1, first, second))])
    for colour in COLOURS:
        for draws in DRAW_PAIRS:
            yield case_of(button_world(0, 0, 1, draws) + [(29, (1, colour)), (30, (1, colour ^ 0xFFFFFFFF)), (23, (1, 1, 0)), (29, (1, colour)),
                                                          (30, (1, colour))])
    for state in STATES:
        yield case_of(button_world(state) + [(3, (1, 1, 11, 22)), (31, (1,)), (3, (1, 1, -1, 0x7FFFFFFF)), (31, (1,))])
    # the destructor: with and without text, the flag, and the string heap's release
    for text in (-1, 0, 2):
        for flag in (1, 0, -1, 2, 0x7FFF, -0x8000):
            yield case_of(button_world(text=text) + [(34, (1, flag))])


def button_random(r):
    ops = [window_op(0, -1, 1, 7, (0, 0, 300, 200))]
    buttons = []
    for slot in range(1, r.randint(2, 4)):
        ops.append(construct(slot, 2, 0, r.choice([1, 3, 2, 0x401]), slot, rnd_rect(r)))
        ops.append(draws_op(slot, *r.choice(DRAW_PAIRS)))
        ops.append(button_data(slot, r.choice(STATES), r.choice([0, 1, -1]), r.choice([0, 1, 2, 3]), r.choice([0, 500, -1]), r.choice([0, 125]),
                               r.choice([-1, 0, 1, 2]), r.choice(COLOURS), r.choice(COLOURS), r.choice(SHIFTS), r.choice(SHIFTS)))
        buttons.append(slot)
    ops.append((8, (r.choice([0, 1]), 0)))
    for _ in range(r.randint(3, 30)):
        slot = r.choice(buttons)
        code = r.choice([20, 21, 22, 23, 23, 24, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 3, 4, 34])
        if code == 21:
            ops.append((21, (slot, r.choice([0, 1, -1]))))
        elif code in (23, 24):
            ops.append((code, (slot, r.choice(STATES), r.choice([0, 1, -1]))))
        elif code == 25:
            ops.append((25, (slot, r.choice([-1, 0, 1, 2]))))
        elif code == 28:
            ops.append((28, (slot, r.choice([0, 500, -1, 0x7FFFFFFF]), r.choice([0, 125, -1]))))
        elif code in (29, 30):
            ops.append((code, (slot, r.choice(COLOURS))))
        elif code == 3:
            ops.append((3, (slot, r.choice([1, 3, 2, 0x401]), r.choice([-1, 0, 9]), r.choice([-1, 0, 7]))))
        elif code == 4:
            ops.append(draws_op(slot, *r.choice(DRAW_PAIRS)))
        elif code == 34:
            ops.append((34, (slot, r.choice([1, 0, -1, 2]))))
        else:
            ops.append((code, (slot,)))
    return case_of(ops, q={"Window.Enable": [r.choice([0, 1, -1, 0x10000]) for _ in range(6)], "PostMessage": [1]})


TESTS.append(FnTest("InterfaceButton: constructor and members", ui_ppc, ui_host, edge=button_edges, rnd=button_random))


# ======================================================================= test 4: scripts of 200 operations


def long_script(r, length=200):
    """A script of `length` operations over the whole world: controls made by CreateObject and by the constructors, the
    button's members, setters and the odd global."""
    ops = world([-1, 0, 0]) + [(8, (r.choice([0, 1]), r.choice([0, 1])))]
    made = {0, 1, 2}
    buttons = set()
    for _ in range(length):
        k = r.random()
        free = [s for s in range(SLOTS) if s not in made]
        if k < 0.25 and free:
            slot = r.choice(free)
            kind = r.choice([r.randint(1, 13), 2, 2, 2, 5, 4, 7, r.randint(-1, 15)])
            ops.append(create(slot, r.choice(sorted(made)), kind, rnd_attributes(r), rnd_id(r), rnd_rect(r)))
            if 1 <= kind <= 13:
                made.add(slot)
                if kind == 2:
                    buttons.add(slot)
        elif k < 0.35 and free:
            slot = r.choice(free)
            kind = r.randint(1, 13)
            ops.append(construct(slot, kind, r.choice(sorted(made) + [-1]), rnd_attributes(r), rnd_id(r), rnd_rect(r)))
            made.add(slot)
            if kind == 2:
                buttons.add(slot)
        elif buttons:
            slot = r.choice(sorted(buttons))
            code = r.choice([20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 7, 4, 3, 34, 8])
            if code == 7:
                ops.append(button_data(slot, r.choice(STATES), r.choice([0, 1]), r.choice([0, 2]), r.choice([0, 500]), 125, r.choice([-1, 0, 1]),
                                       r.choice(COLOURS), r.choice(COLOURS), r.choice(SHIFTS), r.choice(SHIFTS)))
            elif code == 4:
                ops.append(draws_op(slot, *r.choice(DRAW_PAIRS)))
            elif code == 3:
                ops.append((3, (slot, r.choice([1, 3, 2]), r.choice([-1, 5]), r.choice([-1, 6]))))
            elif code == 8:
                ops.append((8, (r.choice([0, 1]), r.choice([0, 1]))))
            elif code == 34:
                ops.append((34, (slot, r.choice([1, 0]))))
                buttons.discard(slot)
                made.discard(slot)
            elif code == 21:
                ops.append((21, (slot, r.choice([0, 1]))))
            elif code in (23, 24):
                ops.append((code, (slot, r.choice(STATES), r.choice([0, 1]))))
            elif code == 25:
                ops.append((25, (slot, r.choice([-1, 0, 1]))))
            elif code == 28:
                ops.append((28, (slot, r.choice([0, 500]), r.choice([0, 125]))))
            elif code in (29, 30):
                ops.append((code, (slot, r.choice(COLOURS))))
            else:
                ops.append((code, (slot,)))
    return case_of(ops, q=rnd_queues(r))


def long_cases():
    import random
    for seed in range(6):
        yield long_script(random.Random(2000 + seed))


def script_random(r):
    return long_script(r, r.randint(10, 60))


TESTS.append(FnTest("scripts of controls and button calls", ui_ppc, ui_host, edge=long_cases, rnd=script_random))
