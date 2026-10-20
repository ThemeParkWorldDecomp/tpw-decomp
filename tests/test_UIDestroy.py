"""Differential tests for the window's destructor chain, against libtpw_unit_UIDestroy.dylib (tests/unit_UIDestroy.cpp, unit w51):

  src/InterfaceWindow_dtor.cpp   InterfaceWindow::~InterfaceWindow, PropagateDestruction, UnCaptureKeys
  src/UI_timers_remove.cpp       UI_RemoveTimer, InterfaceTimer::FindTimer
  src/UI_invalidate.cpp          ClearAllPointers, UI_InvalidateMessages, UI_InvalidateDrawMessages, UI_InvalidateDeferredMessages,
                                 InterfaceMessageQueue::InvalidateMsgs, InterfaceTimedMessages::InvalidateMsgs

Real on both sides: those, and under them the window constructor (src/InterfaceWindow.cpp), UI_SendMessage and CallWindowHandler
(src/UI_send.cpp) and the rectangles (src/UI_Base.cpp). Fakes that record the same event on both sides: the two window handlers (and
the default one), UI_PostMessage, UI_UpdatePointerIn, the help text handler's OnHideOrDelete, the draw objects' Show and destructor,
the regions' destructor, the assertion, operator delete (the windows', the draw objects' and the regions' storage is kept).

The world: fourteen windows in a forest (made with the original constructor, then linked), six draw objects, three fake regions, a
table of twelve timers, a general, a draw and a timed queue, a key list and the help text handler. A script of operations runs over
it; after each operation the answer, the 17 globals, every window (63 values), both queues, the timed queue, the timers, the draw
objects, the heap traffic and the length of the call log are compared (the whole log at the end).

Scripted behaviour (the case's return queue "act", the same on both sides): each handler call takes an action (1 cut the window's own
next link, 2 detach its children, 3 make it the focus window, 4 make it the keyboard window).

Unit w54 extends the module with the leftovers on the destroy path (the same world, ops 31 to 43): InterfaceWindow::Enable (through the
vtable, so a button runs its own and then this one), operator- (the base's, with a bar, a text box and a button as either side),
GetHelpText, GetText, GetValue and DrawFunction (through the vtable), the bar's and the text box's destructors (windows of the world
may be bars, text boxes and buttons; a text box may own a block of the heap that its destructor frees), InterfaceTimer's destructor and
InterfaceStringHeap::ReleaseString. The destructor ops use the kind's own destructor on the PowerPC side (the vtable's slot +0x10).

The destructor is run in both forms: the plain body (flag 0 in r4 on the PowerPC side, an explicit destructor call on the host) and
the deleting form (flag 1; the host's `delete`). The children's deleting destructors run through the vtable on both sides.
An operation the state does not allow (a loop of links, or a child that would never be unlinked, which makes the original loop for
ever) is decided on the PowerPC side and skipped on both.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIDestroy"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (all run as the original on the PowerPC side)
FN = dict(Dtor=0x101D9BA4, Propagate=0x101D9EE8, UnCapture=0x101DCEBC, RemoveTimer=0x101D8A00, FindTimer=0x101D8500,
          ClearAll=0x101CA254, InvMsgs=0x101C9CA4, InvDraw=0x101C9D2C, InvDeferred=0x101C9DB8, QueueInv=0x101C8038,
          TimedInv=0x101C84D8)
# unit w54: the leftovers (all run as the original on the PowerPC side)
W54 = dict(Enable=0x101DC320, Minus=0x101DCF24, GetHelp=0x1018AA0C, GetText=0x1018A9D4, GetValue=0x101DFF2C, Draw=0x1018A270,
           TimerDtor=0x101D830C, Release=0x101D7B10, BarDtor=0x101C87A4, TextDtor=0x101D7CC8, ButtonDtor=0x101CAEA0)
# what they run that is real: the bar's, the text box's and the button's constructors, the button's members, SetCallbackFunction
W54_REAL = [0x101C86C4, 0x101D7BF0, 0x101CADC0, 0x101CAF48, 0x101CB440, 0x101CBF40, 0x101CAD20, 0x101CB010, 0x101DCBA8]
# what they call that is real too: the window constructor and the rectangles under it, UI_SendMessage and CallWindowHandler
KIND_CTOR = [0x101D98D8, 0x101C86C4, 0x101D7BF0, 0x101CADC0]   # window, bar, text box, button
KIND_DTOR = [0x101D9BA4, 0x101C87A4, 0x101D7CC8, 0x101CAEA0]
WHAT_AM_I = [0x101D9EB0, 0x101C8848, 0x101D7D74]   # recorded fakes: window, bar, text box
REAL = dict(WindowCtor=0x101D98D8, SendMessage=0x101C9518, CallHandler=0x101DE384)
REAL_HELPERS = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788,
                0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74, 0x101DAB88]
# what they call that is a fake
CALC_CLIENT_AREA, DEFAULT_CALLBACK, POST_MESSAGE, UPDATE_POINTER_IN = 0x101D987C, 0x101DE3F8, 0x101C9400, 0x101C8F18
DEFAULT_CALLBACK_TV = 0x10268480

# ---- the original's data
G = dict(pointer_in=0x10395C1C, capture=0x10395C14, focus=0x10395C20, keyboard=0x10395C10, key_capture=0x10395C08,
         msg_queue=0x10395C04, draw_queue=0x10395C00, timed=0x10395BFC, timers=0x10395C54, timer_count=0x10395C50,
         timer_list=0x10395C58, help=0x102AF928, last_window=0x10395C18, peek=0x10395BF8)
BUTTONS = 0x10395BC4
SIZE_WINDOW = 0x138
STORE_WINDOW = 0x180   # the storage of a window of the world (the button is 0x15c)
SAVE = [(a, 4) for a in G.values()] + [(G["key_capture"] + 4, 4), (BUTTONS, 12)]

# ---- labels (the host's tpw_uid_labels)
SLOTS, DRAWS, REGIONS, TIMERS = 14, 6, 3, 12
LAB_WINDOW, LAB_QUEUE, LAB_TIMED, LAB_DRAW, LAB_REGION, LAB_TIMER, LAB_HELP, LAB_KEYS = 10, 30, 32, 40, 50, 60, 80, 90
STRINGS, STRING_CHARS, LAB_STRING, LAB_TEXT = 3, 6, 100, 110
WINDOW_FIELDS = 63
GLOBAL_FIELDS = 17
BIG_ENTRIES = 0x8010   # the slots of a queue made big
SMALL_ENTRIES = 16
TIMED_ENTRIES = 16
BIG_QUEUE = 16   # a queue of more slots than this is dumped as a checksum


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def link_table(parents):
    """(parent, previous, next, child) slots of each window of a forest given by its parents."""
    out = []
    for slot in range(len(parents)):
        siblings = [t for t in range(len(parents)) if parents[t] == parents[slot]]
        at = siblings.index(slot)
        children = [t for t in range(len(parents)) if parents[t] == slot]
        out.append((parents[slot], siblings[at - 1] if at > 0 else -1, siblings[at + 1] if at + 1 < len(siblings) else -1,
                    children[0] if children else -1))
    return out


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the operations and the dumps."""

    def __init__(self, e, case):
        self.e = e
        e.uid = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in SAVE}
        self.windows = [e.alloc(STORE_WINDOW, b"\xa5" * STORE_WINDOW) for _ in range(SLOTS)]
        self.made = [False] * SLOTS
        self.kinds = [0] * SLOTS
        self.freed = set()   # blocks of the heap the case has freed (a text block must not be freed twice on the host)
        self.strings = [e.alloc(2 * STRING_CHARS, bytes(2 * STRING_CHARS)) for _ in range(STRINGS)]
        self.capacity = BIG_ENTRIES if case.get("big") else SMALL_ENTRIES
        self.queues = [e.alloc(0x14, bytes(0x14)) for _ in range(2)]
        self.entries = [e.alloc(16 * self.capacity, bytes(16 * self.capacity)) for _ in range(2)]
        self.timed_block = e.alloc(8, bytes(8))
        self.timed_entries = e.alloc(0x14 * TIMED_ENTRIES, bytes(0x14 * TIMED_ENTRIES))
        self.timers = e.alloc(TIMERS * 0x20, bytes(TIMERS * 0x20))
        for i in range(TIMERS):
            w32(e, self.timers + 0x20 * i + 0x14, 1)
        self.draws = []
        for _ in range(DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.uid_draw_vtable)
            self.draws.append(block)
        self.regions = []
        for _ in range(REGIONS):
            block = e.alloc(8, bytes(8))
            w32(e, block, e.uid_region_vtable)
            self.regions.append(block)
        self.help = e.alloc(0x18, bytes(0x18))
        w32(e, self.help + 0x10, e.uid_help_vtable)
        self.keys = e.alloc(8, struct.pack(">hhhh", 0x41, 0, 0, -1))
        fixed = {}
        fixed.update({w: LAB_WINDOW + i for i, w in enumerate(self.windows)})
        fixed.update({q: LAB_QUEUE + i for i, q in enumerate(self.queues)})
        fixed[self.timed_block] = LAB_TIMED
        fixed.update({d: LAB_DRAW + i for i, d in enumerate(self.draws)})
        fixed.update({r: LAB_REGION + i for i, r in enumerate(self.regions)})
        fixed.update({self.timers + 0x20 * i: LAB_TIMER + i for i in range(TIMERS)})
        fixed[self.help] = LAB_HELP
        fixed.update({a: LAB_STRING + i for i, a in enumerate(self.strings)})
        fixed[self.keys] = LAB_KEYS
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        # the UI's globals as the start-up leaves them (all NULL), and the help text handler
        for name in G:
            w32(e, G[name], 0)
        w32(e, G["key_capture"] + 4, 0)
        e.write(BUTTONS, bytes(12))
        w32(e, G["help"], self.help)
        # the windows, then their links, handlers and flags
        specs = case.get("windows", [])
        for slot, spec in enumerate(specs):
            parent, attributes, handler, destroyed = spec[:4]
            self.kinds[slot] = spec[4] if len(spec) > 4 else 0
            self.call(KIND_CTOR[self.kinds[slot]], self.windows[slot], self.windows[parent] if parent >= 0 else 0, attributes, slot * 7 - 3,
                      s16(slot), s16(slot), s16(slot + 50), s16(slot + 50))
            self.made[slot] = True
        for slot, (parent, previous, following, child) in enumerate(link_table([s[0] for s in specs])):
            self.run(2, (slot, parent, previous, following, child))
        for slot, spec in enumerate(specs):
            parent, attributes, handler, destroyed = spec[:4]
            self.run(1, (slot, destroyed, handler, 0, -1, -1))
        # the queues: general and draw (slots all free), and the timed one
        general, draw, timed = case.get("queues", (8, 8, 8))
        for k, count in enumerate((general, draw)):
            w32(self.e, self.queues[k], self.entries[k] if count > 0 else 0)
            for offset, value in ((4, count), (8, count - 1), (0xC, count - 1), (0x10, 0)):
                w32(self.e, self.queues[k] + offset, value)
        w32(e, self.timed_block, timed)
        w32(e, self.timed_block + 4, self.timed_entries if timed > 0 else 0)
        # the log starts here
        self.L.log.clear()
        self.L.next = CL.FIRST_NEW
        self.heap = [0, 0, 0, 0]

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def window(self, slot):
        return self.windows[slot] if 0 <= slot < SLOTS and self.made[slot] else 0

    def timer(self, index):
        return self.timers + 0x20 * index if 0 <= index < TIMERS else 0

    def draw(self, index):
        return self.draws[index] if 0 <= index < DRAWS else 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def handler_tv(self, handler):
        e = self.e
        return 0 if handler == 0 else e.uid_handlers[handler] if handler <= 2 else DEFAULT_CALLBACK_TV

    def timers_ok(self, start=None):
        """False when the list of timers (from the head, or from `start`) loops."""
        e = self.e
        seen, t = set(), e.u32(G["timer_list"]) if start is None else start
        while t:
            if t in seen or len(seen) > TIMERS + 2:
                return False
            seen.add(t)
            t = e.u32(t + 0x18)
        return True

    def acyclic(self, root):
        """False when the child and next links under `root` make a loop, or reach so many windows that the log would run out."""
        e = self.e
        known = {w for s, w in enumerate(self.windows) if self.made[s]}
        state = {}

        def visit(w):
            if w == 0 or w not in known:
                return True
            if state.get(w) == 1:
                return False
            if state.get(w) == 2:
                return True
            state[w] = 1
            ok = visit(e.u32(w + 0x58)) and visit(e.u32(w + 0x50))
            state[w] = 2
            return ok

        if not visit(root):
            return False
        visits = {}

        def count(w):
            if w not in visits:
                total, child = 1, e.u32(w + 0x58)
                while child:
                    total += count(child)
                    child = e.u32(child + 0x50)
                visits[w] = total
            return visits[w]

        return count(root) <= 300

    def children_ok(self, root):
        """True when every window under `root` has children that point back at it, the first with no previous: the destructor
        unlinks each child from its parent's list, so a child that does not leaves the original deleting it for ever."""
        e = self.e
        stack = [root]
        while stack:
            w = stack.pop()
            child, previous = e.u32(w + 0x58), 0
            while child:
                if e.u32(child + 0x5C) != w or e.u32(child + 0x54) != previous:
                    return False
                stack.append(child)
                previous, child = child, e.u32(child + 0x50)
        return True

    # ---- the dumps (the orders are tpw_uid_dump_* in the library)
    def callback_code(self, value):
        e = self.e
        return 0 if value == 0 else {e.uid_handlers[i]: i for i in range(1, 3)}.get(value, 5 if value == DEFAULT_CALLBACK_TV else 6)

    def dump_globals(self):
        e, L = self.e, self.L
        u = e.u32
        keys = u(G["key_capture"])
        out = [L.lab(u(G["pointer_in"])), L.lab(u(G["capture"])), L.lab(u(G["focus"])), L.lab(u(G["keyboard"])),
               0 if keys == 0 else 1 if keys == self.keys else 9, L.lab(u(G["key_capture"] + 4))]
        out += [L.lab(u(BUTTONS + 4 * i)) for i in range(3)]
        out += [L.lab(u(G["msg_queue"])), L.lab(u(G["draw_queue"])), L.lab(u(G["timed"])), int(u(G["timers"]) != 0),
                T.s32(u(G["timer_count"])), L.lab(u(G["timer_list"])), L.lab(u(G["help"])), L.lab(u(G["last_window"]))]
        assert len(out) == GLOBAL_FIELDS
        return out

    def dump_window(self, address):
        d = bytes(self.e.read(address, SIZE_WINDOW))
        L = self.L
        s16_ = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        s32_ = lambda o: struct.unpack(">i", d[o:o + 4])[0]   # noqa: E731
        u32_ = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [1, d[0]]
        for offset in (4, 0x10, 0x1C, 0x28, 0x34):
            out += [s16_(offset + 4), s16_(offset + 6), s16_(offset + 8), s16_(offset + 10)]
        region = u32_(0x40)
        out.append(0 if region == 0 else 1 if region == address + 4 else L.lab(region))
        out += [s32_(0x44), L.lab(u32_(0x48)), s16_(0x4C)]
        out += [L.lab(u32_(0x50)), L.lab(u32_(0x54)), L.lab(u32_(0x58)), L.lab(u32_(0x5C))]
        out += [s32_(0x60), s32_(0x64), int(d[0x68:0x88] == b"\xa5" * 0x20)]
        out += [s32_(0x88), s32_(0x8C), s32_(0x90), L.lab(u32_(0x94)), L.lab(u32_(0x98)), int(d[0x9C:0xDC] == bytes(0x40))]
        out += [s16_(0xDC), s16_(0xDE), s16_(0xE0), s16_(0xE2), L.lab(u32_(0xE4)), s16_(0xE8), s32_(0xEC), s32_(0xF0)]
        out += [s16_(0xF4), s16_(0xF6), s16_(0xF8), s16_(0xFA), s32_(0xFC), int(d[0x100:0x110] == b"\xa5" * 0x10)]
        out += [self.callback_code(u32_(0x110)), self.callback_code(u32_(0x114)), s32_(0x11C)]
        out += [L.lab(u32_(0x120 + 4 * i)) for i in range(4)]
        out += [s32_(0x130), d[0x134], d[0x135]]
        assert len(out) == WINDOW_FIELDS
        return out

    def dump_queue(self, k):
        e, L = self.e, self.L
        address = self.queues[k]
        entries, count, head, tail, queued = struct.unpack(">IiiiI", e.read(address, 0x14))
        out = [L.lab(address), int(entries != 0), count, head, tail, T.s32(queued)]
        if entries == 0 or count <= 0:
            return out
        words = struct.unpack(f">{4 * count}I", e.read(entries, 16 * count))
        if count > BIG_QUEUE:   # a long ring: a checksum of its entries stands for them
            total = 0x811C9DC5
            for i in range(count):
                for word in (L.lab(words[4 * i]) & MINUS, words[4 * i + 1], words[4 * i + 2], words[4 * i + 3]):
                    total = ((total ^ word) * 0x01000193) & MINUS
            return out + [total]
        for i in range(count):
            out += [L.lab(words[4 * i]), T.s32(words[4 * i + 1]), T.s32(words[4 * i + 2]), T.s32(words[4 * i + 3])]
        return out

    def dump_timed(self):
        e, L = self.e, self.L
        count, entries = struct.unpack(">iI", e.read(self.timed_block, 8))
        out = [int(entries != 0), count]
        if entries == 0:
            return out
        for i in range(max(0, count)):
            w, m, a, b, t = struct.unpack(">IiiiI", e.read(entries + 0x14 * i, 0x14))
            out += [L.lab(w), m, a, b, T.s32(t)]
        return out

    def dump_timers(self):
        e, L = self.e, self.L
        out = []
        for i in range(TIMERS):
            window, start, slice_, ident, running, free, nxt, prev = struct.unpack(">IIiiiiII", e.read(self.timers + 0x20 * i, 0x20))
            out += [L.lab(window), T.s32(start), slice_, ident, running, free, L.lab(nxt), L.lab(prev)]
        return out

    def dump_draws(self):
        out = []
        for block in self.draws:
            shown = struct.unpack(">h", self.e.read(block + 4, 2))[0]
            in_library, users = struct.unpack(">hh", self.e.read(block + 0xC, 4))
            out += [in_library, users, shown]
        return out

    def dump_extras(self, slot):
        """What a bar, a text box or a button adds to a window (8 words; the library's tpw_uid_dump_extras)."""
        if not self.made[slot]:
            return [0] * 8
        a, L, kind = self.windows[slot], self.L, self.kinds[slot]
        d = bytes(self.e.read(a, STORE_WINDOW))
        s32_ = lambda o: struct.unpack(">i", d[o:o + 4])[0]   # noqa: E731
        s16_ = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        s8_ = lambda o: struct.unpack(">b", d[o:o + 1])[0]   # noqa: E731
        if kind == 1:
            return [1, s32_(0x138), s32_(0x13C), s32_(0x140), s16_(0x14C), s16_(0x14E), s16_(0x150), s16_(0x152)]
        if kind == 2:
            return [2, L.lab(struct.unpack(">I", d[0x138:0x13C])[0]), s32_(0x13C), s32_(0x140), s32_(0x144), s32_(0x148), s16_(0x14C), s16_(0x14E)]
        if kind == 3:
            return [3, s8_(0x138), s8_(0x139), s32_(0x13C), s32_(0x140), L.lab(struct.unpack(">I", d[0x148:0x14C])[0]), s8_(0x158), d[0x15A]]
        return [0] * 8

    def dump_strings(self):
        out = []
        for a in self.strings:
            out += list(struct.unpack(f">{STRING_CHARS}H", self.e.read(a, 2 * STRING_CHARS)))
        return out

    def dump_state(self):
        return dict(globals=self.dump_globals(),
                    windows=[self.dump_window(self.windows[s]) if self.made[s] else [0] for s in range(SLOTS)],
                    queues=[self.dump_queue(0), self.dump_queue(1)], timed=self.dump_timed(), timers=self.dump_timers(),
                    draws=self.dump_draws(), heap=list(self.heap), log=len(self.L.log),
                    extras=[self.dump_extras(s) for s in range(SLOTS)], strings=self.dump_strings())

    # ---- the operations
    def run(self, code, a):
        """Runs one operation; returns its answer, or None when the state does not allow it."""
        e, L = self.e, self.L
        u = e.u32
        if code in (1, 2, 4) and not self.window(a[0]):
            return None
        if code == 1:   # a window's fields: slot, destroying byte, handler, region, draw, client draw
            w = self.windows[a[0]]
            e.write(w, bytes([a[1] & 0xFF]))
            w32(e, w + 0x110, self.handler_tv(a[2]))
            w32(e, w + 0x40, w + 4 if a[3] == 0 else 0 if a[3] < 0 else self.regions[a[3] - 1])
            w32(e, w + 0x94, self.draw(a[4]))
            w32(e, w + 0x98, self.draw(a[5]))
            return 0
        if code == 2:   # links: slot, parent, previous, next, child
            w = self.windows[a[0]]
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
            return 0
        if code == 3:
            return self.set_global(a[0], a[1])
        if code == 4:   # a window's animation timers: slot, four timer indexes
            for i in range(4):
                w32(e, self.windows[a[0]] + 0x120 + 4 * i, self.timer(a[1 + i]))
            return 0
        if code == 5:   # a draw object: index, in library, users, shown
            block = self.draws[a[0]]
            e.write(block + 0xC, struct.pack(">hh", s16(a[1]), s16(a[2])))
            e.write(block + 4, struct.pack(">h", s16(a[3])))
            return 0
        if code == 6:   # a timer: index, window, id, free, next, previous, running, slice, start
            t = self.timers + 0x20 * a[0]
            for offset, value in ((0, self.window(a[1])), (4, a[8]), (8, a[7]), (0xC, a[2]), (0x10, a[6]), (0x14, a[3]),
                                  (0x18, self.timer(a[4])), (0x1C, self.timer(a[5]))):
                w32(e, t + offset, value)
            return 0
        if code == 7:   # a queue's entry: queue, index, window, message, a, b
            address = self.entries[a[0]] + 16 * a[1]
            w32(e, address, self.window(a[2]))
            for k in range(3):
                w32(e, address + 4 + 4 * k, a[3 + k])
            return 0
        if code == 8:   # a timed message: index, window, message, a, b, time
            address = self.timed_entries + 0x14 * a[0]
            w32(e, address, self.window(a[1]))
            for k in range(4):
                w32(e, address + 4 + 4 * k, a[2 + k])
            return 0
        if code == 9:   # a queue's shape: queue (2 the timed one), entries present, count
            if a[0] == 2:
                w32(e, self.timed_block, a[2])
                w32(e, self.timed_block + 4, self.timed_entries if a[1] else 0)
            else:
                w32(e, self.queues[a[0]], self.entries[a[0]] if a[1] else 0)
                w32(e, self.queues[a[0]] + 4, a[2])
            return 0
        if code == 10:   # many entries of one window in a queue: queue, count, window, message
            for i in range(a[1]):
                address = self.entries[a[0]] + 16 * i
                for k, value in enumerate((self.window(a[2]), a[3], i, -i)):
                    w32(e, address + 4 * k, value)
            return 0
        # ---- the functions under test
        if code == 20:   # the destructor: slot, flag
            w = self.window(a[0])
            if not w or not self.acyclic(w) or not self.children_ok(w) or not self.timers_ok():
                return None
            if self.holds_freed_text(a[0]):
                return None
            self.call(KIND_DTOR[self.kinds[a[0]]], w, a[1])
            return 0
        if code == 21:   # UI_RemoveTimer: window (-1 for none), id
            if not self.timers_ok():
                return None
            return T.s32(self.call(FN["RemoveTimer"], self.window(a[0]), a[1]))
        if code == 22:   # FindTimer: the timer to start from, window, id
            if not 0 <= a[0] < TIMERS or not self.timers_ok(self.timer(a[0])):
                return None
            return L.lab(self.call(FN["FindTimer"], self.timer(a[0]), self.window(a[1]), a[2]))
        if code == 23:
            self.call(FN["ClearAll"], self.window(a[0]))
            return 0
        if code in (24, 25, 26):
            return T.s32(self.call(FN[{24: "InvMsgs", 25: "InvDraw", 26: "InvDeferred"}[code]], self.window(a[0])))
        if code == 27:   # a queue's InvalidateMsgs: queue, window
            return T.s32(self.call(FN["QueueInv"], self.queues[a[0]], self.window(a[1])))
        if code == 28:
            return T.s32(self.call(FN["TimedInv"], self.timed_block, self.window(a[0])))
        if code == 29:
            w = self.window(a[0])
            if not w or not self.acyclic(w):
                return None
            self.call(FN["Propagate"], w)
            return 0
        if code == 30:
            w = self.window(a[0])
            if not w:
                return None
            return T.s32(self.call(FN["UnCapture"], w))
        if 31 <= code <= 43:
            return self.run54(code, a)
        raise ValueError(code)

    def holds_freed_text(self, slot):
        """True when a text box in the subtree of `slot` owns a block the case has freed already (it would be freed twice)."""
        e, stack = self.e, [self.windows[slot]]
        while stack:
            w = stack.pop()
            index = self.windows.index(w) if w in self.windows else -1
            if index >= 0 and self.kinds[index] == 2 and e.u32(w + 0x138) in self.freed:
                return True
            child = e.u32(w + 0x58)
            while child:
                stack.append(child)
                child = e.u32(child + 0x50)
        return False

    def virtual(self, w, offset):
        """The code address of the window's virtual member at the vtable offset (through its own vtable, as the Mac's calls do)."""
        e = self.e
        return e.u32(e.u32(e.u32(w + 0x118) + offset))

    def run54(self, code, a):
        """The unit w54 operations (31 to 43)."""
        e, L = self.e, self.L
        if code == 37:   # a timer's destructor: index, flag
            if not 0 <= a[0] < TIMERS:
                return None
            self.call(W54["TimerDtor"], self.timer(a[0]), a[1])
            return 0
        if code == 38:   # ReleaseString: string index (-1 none)
            self.call(W54["Release"], self.strings[a[0]] if 0 <= a[0] < STRINGS else 0)
            return 0
        if code == 39:   # a string's first characters: index, c0, c1, c2
            chars = [a[1], a[2], a[3]] + [0x1000 + i for i in range(3, STRING_CHARS)]
            e.write(self.strings[a[0]], struct.pack(f">{STRING_CHARS}H", *[c & 0xFFFF for c in chars]))
            return 0
        w = self.window(a[0])
        if not w:
            return None
        if code == 31:   # Enable through the vtable (+0x1c): slot, on (the raw word: the library passes it as a short)
            return T.s16(self.call(self.virtual(w, 0x1C), w, a[1]))
        if code == 32:   # operator- of the base class: slot, other slot
            other = self.window(a[1])
            if not other:
                return None
            return T.s32(self.call(W54["Minus"], w, other))
        if code == 33:   # GetHelpText (+0x34)
            return T.s32(self.call(self.virtual(w, 0x34), w))
        if code == 34:   # GetText (+0x30): the label of the text
            return L.lab(self.call(self.virtual(w, 0x30), w))
        if code == 35:   # GetValue (+0x28)
            return T.s32(self.call(self.virtual(w, 0x28), w))
        if code == 36:   # DrawFunction (+0x08)
            self.call(self.virtual(w, 0x08), w)
            return 0
        if code == 40:   # a text: slot, mode (0 none, 1 a block of the heap, 2 + n the string n); text boxes at +0x138, buttons at +0x148
            kind = self.kinds[a[0]]
            if kind not in (2, 3):
                return None
            if a[1] == 1:
                text = e.alloc(8, bytes(8))
                L.labels[text] = LAB_TEXT + a[0]
            else:
                text = self.strings[a[1] - 2] if a[1] >= 2 else 0
            w32(e, w + (0x138 if kind == 2 else 0x148), text)
            return 0
        if code == 41:   # mHelpText: slot, value
            w32(e, w + 0x8C, a[1])
            return 0
        if code == 42:   # the second help text number (+0x90)
            w32(e, w + 0x90, a[1])
            return 0
        if code == 43:   # a button's state byte (+0x138)
            if self.kinds[a[0]] != 3:
                return None
            e.write(w + 0x138, bytes([a[1] & 0xFF]))
            return 0
        raise ValueError(code)

    def set_global(self, which, v):
        e = self.e
        if which == 1:
            w32(e, G["pointer_in"], self.window(v))
        elif which == 2:
            w32(e, G["capture"], self.window(v))
        elif which == 3:
            w32(e, G["focus"], self.window(v))
        elif which == 4:
            w32(e, G["keyboard"], self.window(v))
        elif which == 5:
            w32(e, G["key_capture"] + 4, self.window(v))
        elif which == 6:
            w32(e, G["key_capture"], self.keys if v else 0)
        elif which in (7, 8, 9):
            w32(e, BUTTONS + 4 * (which - 7), self.window(v))
        elif which == 10:
            w32(e, G["timer_list"], self.timer(v))
        elif which == 11:
            w32(e, G["timers"], self.timers if v >= 0 else 0)
            w32(e, G["timer_count"], abs(v))
        elif which == 12:
            w32(e, G["msg_queue"], self.queues[0] if v else 0)
        elif which == 13:
            w32(e, G["draw_queue"], self.queues[1] if v else 0)
        elif which == 14:
            w32(e, G["timed"], self.timed_block if v else 0)
        elif which == 15:   # the table starts at a timer of the world (the count stays)
            w32(e, G["timers"], self.timer(v))
        return 0


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The window handlers' and the draw objects', regions' and help text's vtables and TVectors, in persistent memory."""
    if hasattr(e, "uid_draw_vtable"):
        return

    def handler(hid):
        def fn(emu):
            world, L = emu.uid, emu.cl
            window, message, a, b = emu.r(3), T.s32(emu.r(4)), T.s32(emu.r(5)), T.s32(emu.r(6))
            L.rec("Handler", (hid, L.lab(window), message, a, b))
            action = L.ret("act", 0)
            if action == 1:
                w32(emu, window + 0x50, 0)
            elif action == 2:
                w32(emu, window + 0x58, 0)
            elif action == 3:
                w32(emu, G["focus"], window)
            elif action == 4:
                w32(emu, G["keyboard"], window)
            elif action == 5:
                w32(emu, window + 0x44, emu.u32(window + 0x44) ^ 2)
            elif action == 6:
                w32(emu, window + 0x110, 0)
            emu.set_r(3, 0)
        return fn

    e.uid_handlers = [0] + [T.fake_function(e, handler(i)) for i in range(1, 3)]

    def destroyed(name):
        def dtor(emu):
            L, this, flag = emu.cl, emu.r(3), T.s16(emu.r(4))
            L.rec(name, (L.lab(this),))
            if flag > 0:
                emu.uid.heap[2] += 1
                if L.lab(this) > 0:
                    L.rec("delete", (L.lab(this),))
                L.forget(this)
        return dtor

    def show(emu):
        emu.cl.rec("Draw.Show", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4))))

    def change(emu):
        emu.cl.rec("Draw.Change", (emu.cl.lab(emu.r(3)), emu.r(4), struct.unpack(">h", emu.read(emu.r(5) + 4, 2))[0]))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x08, struct.pack(">I", T.fake_function(e, destroyed("Draw.dtor"))))
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.write(vt + 0x1C, struct.pack(">I", T.fake_function(e, show)))
    e.uid_draw_vtable = vt

    vt = T.persist_alloc(e, 0x20)
    e.write(vt + 0x18, struct.pack(">I", T.fake_function(e, destroyed("FakeRegion.dtor"))))
    e.uid_region_vtable = vt

    def on_hide_or_delete(emu):
        emu.cl.rec("Help.OnHideOrDelete", (emu.cl.lab(emu.r(3)), emu.cl.lab(emu.r(4))))

    vt = T.persist_alloc(e, 0x50)
    e.write(vt + 0x3C, struct.pack(">I", T.fake_function(e, on_hide_or_delete)))
    e.uid_help_vtable = vt


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL.values())
    S.real.update(REAL_HELPERS)
    S.real.update(W54.values())
    S.real.update(W54_REAL)
    S.real.update(KIND_CTOR)
    S.real.update(KIND_DTOR)
    R = CL.recorder

    # the heap: operator delete records the windows' deletions (and counts every block); nothing here makes a block
    def op_delete(e):
        block = e.r(3)
        if block:
            e.uid.heap[2] += 1
            e.uid.freed.add(block)
            if e.cl.lab(block) > 0:
                e.cl.rec("delete", (e.cl.lab(block),))
                e.cl.forget(block)

    S.imp("__dl__FPv", op_delete)
    S.imp("__assertion_failed", lambda e: e.cl.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
    S.internal(CALC_CLIENT_AREA, lambda e: None)
    # WhatAmI of a window, a bar and a text box record (operator- calls it on both windows); the button's runs as the original
    S.internal(WHAT_AM_I[0], R("WhatAmI", "L", ret=lambda e, L: 1))
    S.internal(WHAT_AM_I[1], R("Bar.WhatAmI", "L", ret=lambda e, L: 9))
    S.internal(WHAT_AM_I[2], R("TextBox.WhatAmI", "L", ret=lambda e, L: 10))
    S.internal(DEFAULT_CALLBACK, R("Default", "Liii", ret="i"))
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret=lambda e, L: 1))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uid_open", False):
        return
    lib.tpw_uid_op.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uid_op.restype = C.c_int64
    lib.tpw_uid_make_window.argtypes = [C.c_int32] * 9
    lib.tpw_uid_make_window.restype = C.c_int64
    lib.tpw_uid_make_queues.argtypes = [C.c_int32] * 3
    lib.tpw_uid_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_globals.restype = C.c_int32
    lib.tpw_uid_dump_window.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_queue.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_queue.restype = C.c_int32
    lib.tpw_uid_dump_timed.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_timed.restype = C.c_int32
    lib.tpw_uid_dump_timers.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_timers.restype = C.c_int32
    lib.tpw_uid_dump_draws.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_draws.restype = C.c_int32
    lib.tpw_uid_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_extras.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_strings.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uid_dump_strings.restype = C.c_int32
    lib._uid_open = True


def ints(*values):
    return (C.c_int64 * 12)(*values, *([0] * (12 - len(values))))


class Host:
    """One case on the host side: the same fixtures and operations through the library."""

    def __init__(self, lib, case):
        self.lib = lib
        open_host(lib)
        lib.tpw_uid_reset()
        CL.host_begin(lib, {}, case.get("q"))
        lib.tpw_uid_labels()
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        specs = case.get("windows", [])
        for slot, spec in enumerate(specs):
            parent, attributes, handler, destroyed = spec[:4]
            lib.tpw_uid_make_window(slot, parent, T.s32(attributes), slot * 7 - 3, slot, slot, slot + 50, slot + 50,
                                    spec[4] if len(spec) > 4 else 0)
        for slot, (parent, previous, following, child) in enumerate(link_table([s[0] for s in specs])):
            lib.tpw_uid_op(2, ints(slot, parent, previous, following, child))
        for slot, spec in enumerate(specs):
            parent, attributes, handler, destroyed = spec[:4]
            lib.tpw_uid_op(1, ints(slot, destroyed, handler, 0, -1, -1))
        lib.tpw_uid_make_queues(*case.get("queues", (8, 8, 8)))
        lib.tpw_uid_heap_clear()
        C.c_int32.in_dll(lib, "tpw_cl_n").value = 0
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 1

    def op(self, code, a):
        return self.lib.tpw_uid_op(code, ints(*a))

    def dump_state(self, made):
        lib = self.lib
        buf = (C.c_int64 * 8192)()
        n = lib.tpw_uid_dump_globals(buf)
        state = dict(globals=list(buf[:n]))
        windows = []
        for slot in range(SLOTS):
            if made[slot]:
                lib.tpw_uid_dump_window(slot, buf)
                windows.append(list(buf[:WINDOW_FIELDS]))
            else:
                windows.append([0])
        state["windows"] = windows
        state["queues"] = [list(buf[:lib.tpw_uid_dump_queue(k, buf)]) for k in range(2)]
        state["timed"] = list(buf[:lib.tpw_uid_dump_timed(buf)])
        state["timers"] = list(buf[:lib.tpw_uid_dump_timers(buf)])
        state["draws"] = list(buf[:lib.tpw_uid_dump_draws(buf)])
        heap = (C.c_int64 * 4)()
        lib.tpw_uid_heap(heap)
        state["heap"] = list(heap)
        state["log"] = C.c_int32.in_dll(lib, "tpw_cl_n").value
        extras = []
        for slot in range(SLOTS):
            lib.tpw_uid_dump_extras(slot, buf)
            extras.append(list(buf[:8]) if made[slot] else [0] * 8)
        state["extras"] = extras
        state["strings"] = list(buf[:lib.tpw_uid_dump_strings(buf)])
        return state


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
def destroy_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for op in case["ops"]:
            answer = world.run(op[0], op[1])
            if answer is None:
                continue
            ran.append(dict(op=op, made=list(world.made)))
            steps.append(dict(op=op[0], answer=answer, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def destroy_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for step in RAN.pop(id(case)):
            op = step["op"]
            answer = host.op(op[0], op[1])
            steps.append(dict(op=op[0], answer=answer, state=host.dump_state(step["made"])))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)


TESTS = []


# ======================================================================= scripts: building blocks

INVAL = -0x7F7F7F80   # 0x80808080, what InvalidateMsgs writes over a message's words
MESSAGES = [0x15, 0x14, 1, 2, 3, 0x1000A, 0x10001, 0, -1, 0x7FFFFFFF, INVAL, 0x1E]
USERS = [0, 1, 2, 3, -1, -2, 0x7FFF, -0x8000, -0x7FFF, 0x8000 - 2]
LIBRARY = [0, 1, -1, 2, 0x7FFF, -0x8000, -2]
TIMER_IDS = [0x80080, 0x80081, 0x80082, 0x80083, 0x80084, 0x8007F, 0, -1, 0x7FFFFFFF, 0x75E88]


def forest(parents, handlers=None, destroyed=None):
    """The windows of a case: (parent, attributes, handler, destroyed byte) of each of the forest's windows."""
    n = len(parents)
    return [(parents[i], 1, handlers[i] if handlers else 1 + i % 2, destroyed[i] if destroyed else 0) for i in range(n)]


TREE = [-1, 0, 0, 1, 3, 4]   # a root, two children, and a chain of three under the first


def chain(n):
    return [-1] + list(range(n - 1))


def case_of(ops, parents=None, windows=None, queues=(8, 8, 8), q=None, big=False, **extra):
    case = dict(windows=windows if windows is not None else forest(parents if parents is not None else TREE), queues=queues, ops=ops)
    if q:
        case["q"] = q
    if big:
        case["big"] = True
    case.update(extra)
    return case


def wire(general=1, draw=1, timed=1, timers=TIMERS):
    """The globals the UI start-up sets: the three queues and the timer table."""
    return [(3, (12, general)), (3, (13, draw)), (3, (14, timed)), (3, (11, timers))]


def timer_op(index, window, ident, nxt=-1, prev=-1, free=0, running=1, slice_=100, start=5):
    return (6, (index, window, ident, free, nxt, prev, running, slice_, start))


def timer_list(entries, free=0):
    """Timers linked in the order given: (index, window, id) each; the list's head is the first."""
    ops = []
    for k, (index, window, ident) in enumerate(entries):
        nxt = entries[k + 1][0] if k + 1 < len(entries) else -1
        prev = entries[k - 1][0] if k > 0 else -1
        ops.append(timer_op(index, window, ident, nxt, prev, free))
    if entries:
        ops.append((3, (10, entries[0][0])))
    return ops


def anim_timers(slot, indexes):
    return (4, (slot, *indexes))


def own_timers(slot, mask, first=0):
    """Timers for the animation slots of `mask`, in the list, each with the window and the id the destructor looks for."""
    entries = [(first + i, slot, 0x80080 + i) for i in range(4) if mask & (1 << i)]
    ops = timer_list(entries)
    ops.append(anim_timers(slot, [first + i if mask & (1 << i) else -1 for i in range(4)]))
    return ops


def entry(queue, index, window, message=0x15, a=1, b=2):
    return (7, (queue, index, window, message, a, b))


def timed_entry(index, window, message=0x15, a=1, b=2, time=500):
    return (8, (index, window, message, a, b, time))


def destroy(slot, flag=0):
    return (20, (slot, flag))


# ======================================================================= test 1: the destructor


def dtor_edges():
    # a window alone, in both forms; the help text handler is told and the pointer worked out again
    for flag in (0, 1):
        yield case_of([destroy(0, flag)], parents=[-1])
        yield case_of(wire() + [destroy(0, flag)], parents=[-1])
    # the flag is the SHORT in r4: only a positive one deletes (the host's explicit destructor call is the plain form)
    # the destroying byte: set already (no PropagateDestruction, so no 0x14 message), and every value of the byte
    for byte in (0, 1, 2, 0x7F, 0x80, 0xFF):
        for flag in (0, 1):
            yield case_of([destroy(0, flag)], windows=forest(TREE, destroyed=[byte] + [0] * 5))
            yield case_of([destroy(0, flag)], windows=forest(TREE, destroyed=[byte, 0, byte, 0, 1, byte]))
    # triage 2026-10-13a: the flag is the SHORT in r4 (extsh, ble 0x101d9e64): 2, -1, 0x7fff, 0x10000, 0x10001, 0x18000, -0x8000 as well
    for flag in (2, -1, 0x7FFF, 0x10000, 0x10001, 0x18000, -0x8000):
        yield case_of([destroy(0, flag)], parents=[-1])
        yield case_of([destroy(0, flag)])
        yield case_of([destroy(1, flag), destroy(0, flag)], parents=chain(4))
    # trees of every depth: the children are deleted through the vtable (the deleting form), the window itself by the flag
    for depth in range(1, 8):
        for flag in (0, 1):
            yield case_of([destroy(0, flag)], parents=chain(depth))
            yield case_of([destroy(depth // 2, flag)], parents=chain(depth))
    for parents in ([-1, 0, 0, 0, 0], [-1, 0, 1, 1, 0, 4, 4, 4, 2], [-1, -1, 0, 1, 0, 1, 2, 3], [-1, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5]):
        for slot in (0, 1, 2, len(parents) - 1):
            for flag in (0, 1):
                yield case_of([destroy(slot, flag)], parents=parents)
    # the draw objects: every count against every library flag, the main one and the client one, then both the same object
    for users in USERS:
        for library in LIBRARY:
            ops = [(5, (0, library, users, 3)), (5, (1, library, users, 4)), (1, (0, 0, 1, 0, 0, 1))]
            yield case_of(ops + [destroy(0)], parents=[-1])
            ops = [(5, (0, library, users, 3)), (1, (0, 0, 1, 0, 0, -1))]
            yield case_of(ops + [destroy(0, 1)], parents=[-1])
            ops = [(5, (1, library, users, 3)), (1, (0, 0, 1, 0, -1, 1))]
            yield case_of(ops + [destroy(0, 1)], parents=[-1])
            ops = [(5, (0, library, users, 3)), (1, (0, 0, 1, 0, 0, 0))]
            yield case_of(ops + [destroy(0)], parents=[-1])
    # draw objects shared by windows of a tree (users counts down as each lets go)
    for users in (1, 2, 3, 4):
        for library in (0, 1, -2):
            ops = [(5, (0, library, users, 0)), (5, (1, library, users, 0))]
            for slot in range(6):
                ops.append((1, (slot, 0, 1 + slot % 2, 0, slot % 2, -1 if slot % 3 else 1)))
            yield case_of(ops + [destroy(0, 1)])
    # the region: none, the window's own rectangle, a fake region (deleting form), the same fake region on two windows
    for region in (-1, 0, 1, 2, 3):
        for flag in (0, 1):
            yield case_of([(1, (0, 0, 1, region, -1, -1)), destroy(0, flag)], parents=[-1])
    ops = [(1, (slot, 0, 1, 1 + slot % 3, -1, -1)) for slot in range(6)]
    yield case_of(ops + [destroy(0, 1)])
    yield case_of([(1, (1, 0, 1, 1, -1, -1)), (1, (2, 0, 1, 1, -1, -1)), destroy(0)])
    # the animation timers: every subset, all in the list, then only some in the list, then none
    for mask in range(16):
        yield case_of(wire() + own_timers(0, mask) + [destroy(0)], parents=[-1])
        yield case_of(wire() + own_timers(0, mask) + [destroy(0, 1)])
        yield case_of(wire() + own_timers(0, mask) + [anim_timers(0, [0, 1, 2, 3]), destroy(0)], parents=[-1])
    # the timers of a tree's windows (each window removes its own; ids of other windows are left)
    ops = wire()
    ops += timer_list([(slot * 4 + i, slot, 0x80080 + i) for slot in range(3) for i in range(4)])
    for slot in range(3):
        ops.append(anim_timers(slot, [slot * 4 + i for i in range(4)]))
    yield case_of(ops + [destroy(0, 1)])
    yield case_of(ops + [destroy(2, 0)])
    # the table of timers (the assertion): none, empty, a table smaller than the timer's place
    for table in (-3, 0, 1, 2, 3, TIMERS):
        for mask in (1, 8, 15):
            yield case_of(wire(timers=table) + own_timers(0, mask, first=2) + [destroy(0)], parents=[-1])
    # the queues: messages for the window, for a child, for another window, in each of the three queues
    for general, draw, timed in ((1, 1, 1), (1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, 0)):
        ops = wire(general, draw, timed)
        for slot in (0, 1, 2, 4):
            ops += [entry(0, slot, slot), entry(1, slot, slot, 1 + slot % 3), timed_entry(slot, slot, 0x30 + slot, slot, 2, 77 + slot)]
        ops += [entry(0, 5, 0), entry(1, 6, 1), timed_entry(6, 0, time=0), timed_entry(7, 3)]
        yield case_of(ops + [destroy(0), destroy(1, 1)])
    # the globals: this window, a child, another window, a button
    for which in range(1, 10):
        for target in (0, 1, 4, 5, -1):
            yield case_of([(3, (which, target)), (3, (6, 1)), destroy(0, 1)])
            yield case_of([(3, (which, target)), (3, (6, 1)), destroy(1)])
    ops = [(3, (w, 0)) for w in range(1, 10)]
    yield case_of(ops + [destroy(0)])
    ops = [(3, (w, 1)) for w in range(1, 10)]
    yield case_of(ops + [destroy(0, 1)])
    # the unlink: a first child, a middle one, the last, an only child, a root with siblings, a window whose previous link is cut
    parents = [-1, 0, 0, 0, 0, 2, 2]
    for slot in range(len(parents)):
        for flag in (0, 1):
            yield case_of([destroy(slot, flag)], parents=parents)
    for slot in range(5):
        yield case_of([destroy(slot)], parents=[-1, -1, -1, -1, 0])
    yield case_of([(2, (1, 0, 3, 2, -1)), destroy(1)], parents=[-1, 0, 0, 0])
    yield case_of([(2, (2, 0, -1, 3, -1)), destroy(2)], parents=[-1, 0, 0, 0])
    yield case_of([(2, (2, -1, 1, 3, -1)), destroy(2)], parents=[-1, 0, 0, 0])
    yield case_of([(2, (2, 0, 1, -1, -1)), destroy(2)], parents=[-1, 0, 0, 0])
    # destroyed twice and a window destroyed after its parent
    yield case_of([destroy(1, 1), destroy(0, 1)], parents=[-1, 0, 0, 1])
    yield case_of([destroy(0), destroy(0)], parents=TREE)
    yield case_of([destroy(0, 1), destroy(0, 1)], parents=[-1, 0])
    # handlers (none, the two, the default) and what they do while the 0x14 message goes round
    for handlers in ([0] * 6, [1] * 6, [2] * 6, [5] * 6, [1, 0, 2, 5, 1, 2]):
        yield case_of([destroy(0, 1)], windows=forest(TREE, handlers=handlers))
    for action in range(1, 5):
        for first in range(4):
            yield case_of([destroy(0, 1), destroy(1)], q={"act": [0] * first + [action, 0]})
            yield case_of([destroy(1)], parents=[-1, 0, 1, 1, 2], q={"act": [0] * first + [action, 0]})


def dtor_random(r):
    return rnd_case(r, "dtor")


TESTS.append(FnTest("InterfaceWindow::~InterfaceWindow", destroy_ppc, destroy_host, edge=dtor_edges, rnd=dtor_random))


# ======================================================================= random worlds


def rnd_parents(r):
    n = r.randint(1, SLOTS)
    parents = [-1]
    for i in range(1, n):
        mode = r.random()
        if mode < 0.3:
            parents.append(i - 1)
        elif mode < 0.4:
            parents.append(-1)
        else:
            parents.append(r.randrange(0, i))
    return parents


def rnd_world(r, focus):
    """The ops that fill a random world: window fields, draw objects, timers, queues and globals."""
    parents = rnd_parents(r)
    n = len(parents)
    windows = [(parents[i], r.choice([0, 1, 3]), r.choice([0, 1, 1, 2, 5]), r.choice([0, 0, 0, 0, 1, 2, 0x80, 0xFF])) for i in range(n)]
    ops = []
    ops += wire(r.choice([0, 1, 1]), r.choice([0, 1, 1]), r.choice([0, 1, 1]), r.choice([TIMERS, TIMERS, 6, 0, -4]))
    for d in range(DRAWS):
        ops.append((5, (d, r.choice(LIBRARY[:4]), r.choice(USERS[:6]), r.choice([0, 1, 2]))))
    slot_pick = lambda: r.choice([-1] + list(range(n)))   # noqa: E731
    for slot in range(n):
        ops.append((1, (slot, windows[slot][3], windows[slot][2], r.choice([0, 0, -1, 1, 2, 3]), r.choice([-1, -1] + list(range(DRAWS))),
                        r.choice([-1, -1] + list(range(DRAWS))))))
    # timers: a list in a random order, some for the windows' animation ids
    order = list(range(TIMERS))
    r.shuffle(order)
    entries = []
    for index in order[:r.randint(0, TIMERS)]:
        window = slot_pick()
        ident = r.choice(TIMER_IDS)
        entries.append((index, window, ident))
    ops += timer_list(entries, free=r.choice([0, 1]))
    for index in order[len(entries):]:
        if r.random() < 0.3:
            ops.append(timer_op(index, slot_pick(), r.choice(TIMER_IDS), free=r.choice([0, 1])))
    for slot in range(n):
        if r.random() < 0.4:
            indexes = [r.choice([-1, -1] + list(range(TIMERS))) for _ in range(4)]
            ops.append(anim_timers(slot, indexes))
            for i in range(4):
                if indexes[i] >= 0 and r.random() < 0.6:
                    ops.append(timer_op(indexes[i], slot, 0x80080 + i, free=0))
    # the globals
    for which in range(1, 10):
        if r.random() < 0.6:
            ops.append((3, (which, slot_pick())))
    ops.append((3, (6, r.choice([0, 1]))))
    # the queues
    for k in (0, 1):
        count = r.randint(0, 8)
        ops.append((9, (k, r.choice([1, 1, 1, 0]), count)))
        for i in range(count):
            if r.random() < 0.7:
                ops.append(entry(k, i, slot_pick(), r.choice(MESSAGES), r.choice(MESSAGES), r.choice(MESSAGES)))
    count = r.randint(0, TIMED_ENTRIES)
    ops.append((9, (2, r.choice([1, 1, 1, 0]), count)))
    for i in range(count):
        if r.random() < 0.7:
            ops.append(timed_entry(i, slot_pick(), r.choice(MESSAGES), r.choice(MESSAGES), r.choice(MESSAGES), r.choice([0, 1, 500, -1])))
    return windows, ops, n, slot_pick


def rnd_case(r, focus):
    windows, ops, n, slot_pick = rnd_world(r, focus)
    for _ in range(r.randint(1, 4)):
        slot = r.randrange(n)
        window = slot_pick()
        if focus == "dtor":
            ops.append(destroy(slot, r.choice([0, 1])))
        elif focus == "propagate":
            ops.append((29, (slot,)))
            if r.random() < 0.3:
                ops.append((30, (slot,)))
        elif focus == "uncapture":
            ops += [(3, (5, slot_pick())), (3, (6, r.choice([0, 1]))), (30, (slot,)), (30, (r.randrange(n),))]
        elif focus == "remove":
            ops.append((21, (window, r.choice(TIMER_IDS))))
            ops.append((21, (r.randrange(n), 0x80080 + r.randrange(4))))
        elif focus == "find":
            ops.append((22, (r.randrange(TIMERS), window, r.choice(TIMER_IDS))))
            ops.append((22, (r.randrange(TIMERS), r.randrange(n), 0x80080 + r.randrange(4))))
        elif focus == "clear":
            ops.append((23, (window,)))
        elif focus == "inval":
            ops.append((r.choice([24, 25, 26]), (window,)))
        elif focus == "queue":
            ops.append((27, (r.choice([0, 1]), window)))
        elif focus == "timedq":
            ops.append((28, (window,)))
    return dict(windows=windows, queues=(8, 8, 8), ops=ops, q={"act": [r.choice([0, 0, 0, 0, 1, 2, 3, 4]) for _ in range(6)]})


# ======================================================================= test 2: PropagateDestruction, UnCaptureKeys


def propagate_edges():
    # trees of every depth from 1 to 7 (the Mac unrolls four levels and calls itself from the fifth), from the root and from a middle
    for depth in range(1, 8):
        for slot in (0, depth // 2, depth - 1):
            ops = [(3, (4, depth - 1)), (3, (3, slot)), (3, (5, slot)), (3, (6, 1))]
            yield case_of(ops + [(29, (slot,))], parents=chain(depth))
            yield case_of([(29, (slot,))], parents=chain(depth))
    # the same with branches at every level, so the order of the sends and of the clears shows
    for parents in ([-1, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5], [-1, 0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0], [-1, 0, 0, 0, 1, 1, 4, 4, 6, 6, 8, 8, 10, 10],
                    [-1, -1, 0, 1, 2, 3, 4, 5], TREE):
        for slot in range(len(parents)):
            yield case_of([(29, (slot,))], parents=parents)
    # a deep tree with a key capture, the keyboard window and the focus window on different levels (each cleared when its window is done)
    for depth in (6, 7):
        for key in range(depth):
            for board in (0, depth - 1, key):
                ops = [(3, (5, key)), (3, (6, 1)), (3, (4, board)), (3, (3, (key + 2) % depth)), (29, (0,))]
                yield case_of(ops, parents=chain(depth))
    # windows already marked destroyed: still walked, but no message goes to them
    for byte in (1, 0x80, 0xFF, 2):
        for marked in range(6):
            destroyed = [byte if s == marked else 0 for s in range(6)]
            yield case_of([(3, (4, 4)), (3, (3, 5)), (29, (0,))], windows=forest(TREE, destroyed=destroyed))
    yield case_of([(29, (0,)), (29, (0,))], parents=TREE)
    yield case_of([(29, (3,)), (29, (1,)), (29, (0,))], parents=TREE)
    # handlers (none, the two, the default) and what they do while the message goes round
    for handlers in ([0] * 6, [1] * 6, [5] * 6, [1, 0, 2, 5, 1, 2]):
        yield case_of([(29, (0,))], windows=forest(TREE, handlers=handlers))
    for action in range(1, 5):
        for first in range(6):
            yield case_of([(29, (0,)), (29, (0,))], q={"act": [0] * first + [action, 0]})
            yield case_of([(29, (0,))], parents=[-1, 0, 0, 1, 1, 2, 2, 3, 3], q={"act": [0] * first + [action, 0, action]})
    # a peek callback is not set; a window with no handler answers -1 and nothing is recorded
    yield case_of([(29, (0,))], windows=forest(TREE, handlers=[0] * 6))
    # UnCaptureKeys: the window that has the keys, another, none; with and without a key list
    for holder in (-1, 0, 1, 2, 5):
        for keys in (0, 1):
            for slot in (0, 1, 2, 5):
                yield case_of([(3, (5, holder)), (3, (6, keys)), (30, (slot,)), (30, (slot,)), (30, (holder if holder >= 0 else 0,))])


def propagate_random(r):
    return rnd_case(r, "propagate")


TESTS.append(FnTest("InterfaceWindow::PropagateDestruction", destroy_ppc, destroy_host, edge=propagate_edges, rnd=propagate_random))


def uncapture_edges():
    for holder in (-1, 0, 1, 2, 3, 5):
        for keys in (0, 1):
            for slot in range(6):
                yield case_of([(3, (5, holder)), (3, (6, keys)), (30, (slot,)), (30, (slot,))])
    yield case_of([(30, (0,)), (30, (1,))])
    yield case_of([(3, (5, 2)), (30, (2,)), (3, (5, 2)), (3, (6, 1)), (30, (3,)), (30, (2,))])
    yield case_of([(3, (4, 1)), (3, (3, 1)), (3, (5, 1)), (3, (6, 1)), (30, (1,))])


def uncapture_random(r):
    return rnd_case(r, "uncapture")


TESTS.append(FnTest("InterfaceWindow::UnCaptureKeys", destroy_ppc, destroy_host, edge=uncapture_edges, rnd=uncapture_random))


# ======================================================================= test 3: FindTimer, UI_RemoveTimer


def find_edges():
    # lists of every length from 1 to 12 (the Mac unrolls five levels and calls itself from the sixth): the match at every place
    for length in range(1, TIMERS + 1):
        for place in range(length):
            entries = [(k, 1, 0x80080 + 1) if k != place else (k, 0, 0x80080) for k in range(length)]
            ops = timer_list(entries) + [(22, (0, 0, 0x80080)), (22, (place, 0, 0x80080)), (22, (min(place + 1, length - 1), 0, 0x80080)),
                                         (22, (0, 1, 0x80080)), (22, (0, 0, 0x80081))]
            yield case_of(ops)
    # two matches (the deeper one answers), three, and every timer a match; several lists lengths
    for length in range(2, TIMERS + 1):
        for first in range(0, length - 1, 1 if length < 8 else 3):
            for second in range(first + 1, length, 1 if length < 8 else 4):
                entries = [(k, 0, 0x80080) if k in (first, second) else (k, 2, 7) for k in range(length)]
                ops = timer_list(entries) + [(22, (0, 0, 0x80080)), (22, (first, 0, 0x80080)), (22, (second, 0, 0x80080))]
                yield case_of(ops)
    for length in range(1, TIMERS + 1):
        yield case_of(timer_list([(k, 0, 5) for k in range(length)]) + [(22, (0, 0, 5)), (22, (length // 2, 0, 5)), (22, (length - 1, 0, 5))])
        yield case_of(timer_list([(k, k % 3, 5 + k % 2) for k in range(length)]) +
                      [(22, (0, w, i)) for w in range(3) for i in (5, 6)])
    # the window and the id apart: the right id and a wrong window, the right window and a wrong id (signed compare, NULL window)
    for window in (-1, 0, 1):
        for ident in TIMER_IDS:
            for want_window in (-1, 0, 1):
                for want_id in (ident, ident ^ 1, -ident, 0x80080):
                    entries = [(0, 2, 99), (1, window, ident), (2, 3, -5)]
                    yield case_of(timer_list(entries) + [(22, (0, want_window, want_id)), (22, (1, want_window, want_id)),
                                                          (22, (2, want_window, want_id))])
    # a list that does not start at the head, and a timer on its own
    yield case_of(timer_list([(3, 0, 1), (5, 1, 2), (7, 0, 1)]) + [(22, (3, 0, 1)), (22, (5, 0, 1)), (22, (7, 0, 1)), (22, (4, 0, 1))])
    yield case_of([timer_op(2, 0, 9)] + [(22, (2, 0, 9)), (22, (2, 1, 9)), (22, (2, 0, 8))])


def find_random(r):
    return rnd_case(r, "find")


TESTS.append(FnTest("InterfaceTimer::FindTimer", destroy_ppc, destroy_host, edge=find_edges, rnd=find_random))


def remove_edges():
    # no list at all, an empty table, no match
    yield case_of(wire() + [(21, (0, 0x80080))])
    yield case_of(wire() + timer_list([(0, 1, 5)]) + [(21, (0, 5)), (21, (1, 6)), (21, (1, 5)), (21, (1, 5))])
    # the timer removed from the head, the middle, the end, the only one: the links, the head and the free flag
    for length in range(1, 6):
        for place in range(length):
            entries = [(k, 0, 10 + k) for k in range(length)]
            yield case_of(wire() + timer_list(entries) + [(21, (0, 10 + place)), (21, (0, 10 + place))])
            yield case_of(wire() + timer_list(entries) + [(21, (0, 10 + place)), (21, (0, 10 + (place + 1) % length)),
                                                          (21, (0, 10 + (place + 2) % length))])
    # the last match goes (two timers alike), the one before it next time
    yield case_of(wire() + timer_list([(0, 0, 7), (1, 1, 7), (2, 0, 7), (3, 0, 8), (4, 0, 7)]) + [(21, (0, 7))] * 4)
    # a timer that has links of its own that do not agree with the list (the previous or next link NULL on the head, set on a middle one)
    ops = [timer_op(0, 0, 3, nxt=1, prev=-1), timer_op(1, 0, 4, nxt=2, prev=0), timer_op(2, 0, 5, nxt=-1, prev=1), (3, (10, 0))]
    for ident in (3, 4, 5):
        yield case_of(wire() + ops + [(21, (0, ident))])
    ops = [timer_op(0, 0, 3, nxt=1, prev=5), timer_op(1, 0, 4, nxt=2, prev=-1), (3, (10, 0))]
    for ident in (3, 4):
        yield case_of(wire() + ops + [(21, (0, ident))])
    # the table assertion: the timer in the table, outside it (the table is smaller), no table, a table of none, more timers than count
    for table in (-5, 0, 1, 2, 3, 4, 5, 6, TIMERS):
        for index in range(0, 8):
            yield case_of(wire(timers=table) + timer_list([(index, 0, 9)]) + [(21, (0, 9))])
    for table in (-1, 0, 4, TIMERS):
        yield case_of(wire(timers=table) + timer_list([(0, 0, 9), (3, 0, 9), (4, 0, 9), (9, 0, 9)]) + [(21, (0, 9))] * 4)
    # triage 2026-10-13a: a table that starts at a later timer of the world (the count stays), so timers before its start (the unsigned
    # compare against gUITimers, 0x101d8abc) and after its end are outside it, those inside are not
    for base in range(0, 8, 2):
        for count in (0, 1, 2, 4, 6, -1, -3):
            for index in range(0, 10, 3):
                yield case_of(wire(timers=count) + [(3, (15, base))] + timer_list([(index, 0, 5)]) + [(21, (0, 5))])
    # the slot freed whatever it was (free flag already 1, running, start and slice are left alone)
    for free in (0, 1, 2, -1):
        yield case_of(wire() + [timer_op(0, 0, 9, free=free, running=3, slice_=-4, start=0x12345678), (3, (10, 0)), (21, (0, 9))])
    # a NULL window matches a timer with no window; ids across the signed range
    for ident in TIMER_IDS:
        yield case_of(wire() + timer_list([(0, -1, ident), (1, 0, ident)]) + [(21, (-1, ident)), (21, (0, ident))])


def remove_random(r):
    return rnd_case(r, "remove")


TESTS.append(FnTest("UI_RemoveTimer", destroy_ppc, destroy_host, edge=remove_edges, rnd=remove_random))


# ======================================================================= test 4: ClearAllPointers


def clear_edges():
    # every global against this window, another, none: each alone, then all together
    for which in range(1, 10):
        if which in (5, 6):
            continue
        for held in (0, 1, -1):
            for target in (0, 1, -1):
                yield case_of([(3, (which, held)), (23, (target,))])
    ops = [(3, (w, 0)) for w in (1, 2, 3, 4, 7, 8, 9)]
    yield case_of(ops + [(23, (0,))])
    yield case_of(ops + [(23, (1,))])
    yield case_of(ops + [(23, (-1,))])
    ops = [(3, (1, 0)), (3, (2, 1)), (3, (3, 0)), (3, (4, 0)), (3, (7, 1)), (3, (8, 0)), (3, (9, 1))]
    yield case_of(ops + [(23, (0,)), (23, (1,))])
    # the key capture is not touched by it, and neither is the keyboard window
    yield case_of([(3, (5, 0)), (3, (6, 1)), (3, (4, 0)), (23, (0,))])
    # the three buttons, each alone
    for button in range(3):
        for other in range(3):
            ops = [(3, (7 + button, 2)), (3, (7 + other, 3)), (23, (2,)), (23, (3,))]
            yield case_of(ops)


def clear_random(r):
    return rnd_case(r, "clear")


TESTS.append(FnTest("ClearAllPointers", destroy_ppc, destroy_host, edge=clear_edges, rnd=clear_random))


# ======================================================================= test 5: the three UI_Invalidate*Messages


def inval_edges():
    # each queue present or not, a window or NULL: -1 or the queue's answer
    for general in (0, 1):
        for draw in (0, 1):
            for timed in (0, 1):
                for window in (-1, 0, 1):
                    ops = wire(general, draw, timed) + [entry(0, 0, 0), entry(0, 3, 1), entry(1, 1, 0, 1), entry(1, 2, 1), timed_entry(0, 0),
                                                        timed_entry(2, 1, time=9)]
                    yield case_of(ops + [(24, (window,)), (25, (window,)), (26, (window,))])
    # an empty queue, a queue with no slots, a queue with a count and no entries pointer
    for count in (0, 1, 8, -1):
        for present in (0, 1):
            ops = wire() + [(9, (0, present, count)), (9, (1, present, count)), (9, (2, present, count))]
            yield case_of(ops + [(24, (0,)), (25, (0,)), (26, (0,))])
    # the draw queue is not the general queue's: the messages of the window in each
    ops = wire() + [entry(0, 0, 0, 0x15), entry(0, 1, 0, 0x16), entry(1, 0, 0, 1), entry(1, 1, 1, 2), timed_entry(1, 0), timed_entry(2, 0)]
    yield case_of(ops + [(24, (0,)), (25, (0,)), (26, (0,)), (24, (0,)), (25, (1,)), (26, (1,))])


def inval_random(r):
    return rnd_case(r, "inval")


TESTS.append(FnTest("UI_InvalidateMessages, UI_InvalidateDrawMessages, UI_InvalidateDeferredMessages", destroy_ppc, destroy_host,
                    edge=inval_edges, rnd=inval_random))


# ======================================================================= test 6: InterfaceMessageQueue::InvalidateMsgs


def queue_edges():
    # no entries pointer (any count), no slots, every slot the window's, none, mixed; the poison replaces the three words
    for present in (0, 1):
        for count in (0, 1, 2, 7, 8, 16, -1, -5):
            ops = [(9, (0, present, count))]
            for i in range(max(0, min(count, SMALL_ENTRIES))):
                ops.append(entry(0, i, i % 3, 0x15 + i, 100 + i, -i))
            yield case_of(ops + [(27, (0, 0)), (27, (0, 1)), (27, (0, 0)), (27, (0, -1))])
    for count in range(1, 9):
        for pattern in range(1 << min(count, 6)):
            ops = [(9, (1, 1, count))]
            for i in range(count):
                ops.append(entry(1, i, 0 if (pattern >> (i % 6)) & 1 else 1 if i % 2 else -1, 1 + i, 2 + i, 3 + i))
            yield case_of(ops + [(27, (1, 0)), (27, (1, 0)), (27, (1, 1)), (27, (0, 0))])
    # the poison is not a window: a slot whose window is NULL is never counted, and a NULL window asked for counts the free slots
    yield case_of([entry(0, 0, -1), entry(0, 1, 0), entry(0, 2, -1)] + [(27, (0, -1)), (27, (0, 0))])
    # a slot already poisoned still counts, and the arguments are poisoned
    yield case_of([entry(0, 0, 0, INVAL, INVAL, INVAL), entry(0, 1, 0, 5, INVAL, 7)] + [(27, (0, 0))])
    # a queue longer than 32767 slots with all of them the window's: the answer is a short
    for count in (0x7FFF, 0x8000, 0x8001, 0x8005):
        yield case_of([(9, (0, 1, count)), (10, (0, count, 0, 0x15)), (27, (0, 0)), (27, (0, 1))], big=True)
    yield case_of([(9, (1, 1, 0x8003)), (10, (1, 0x8003, 2, 3)), (27, (1, 2)), (27, (1, 2))], big=True)


def queue_random(r):
    return rnd_case(r, "queue")


TESTS.append(FnTest("InterfaceMessageQueue::InvalidateMsgs", destroy_ppc, destroy_host, edge=queue_edges, rnd=queue_random))


# ======================================================================= test 7: InterfaceTimedMessages::InvalidateMsgs


def timedq_edges():
    for present in (0, 1):
        for count in (0, 1, 2, 7, 16, -1, -3):
            ops = [(9, (2, present, count))]
            for i in range(max(0, min(count, TIMED_ENTRIES))):
                ops.append(timed_entry(i, i % 3 - 1 if i % 4 else 0, 0x15 + i, 100 + i, -i, [0, 1, 500, -1][i % 4]))
            yield case_of(ops + [(28, (0,)), (28, (1,)), (28, (0,)), (28, (-1,))])
    for count in range(1, 9):
        for pattern in range(1 << min(count, 6)):
            ops = [(9, (2, 1, count))]
            for i in range(count):
                ops.append(timed_entry(i, 0 if (pattern >> (i % 6)) & 1 else 1 if i % 2 else -1, 1 + i, 2 + i, 3 + i, 77 + i))
            yield case_of(ops + [(28, (0,)), (28, (0,)), (28, (1,))])
    # the time is the only word cleared: the window and the arguments stay
    yield case_of([timed_entry(0, 0, 0x30, 4, 5, 123456), timed_entry(1, 0, 0x31, 6, 7, 0)] + [(28, (0,))])
    # a slot with no window is counted for a NULL window
    yield case_of([timed_entry(0, -1), timed_entry(1, 0), timed_entry(2, -1, time=0)] + [(28, (-1,)), (28, (0,))])


def timedq_random(r):
    return rnd_case(r, "timedq")


TESTS.append(FnTest("InterfaceTimedMessages::InvalidateMsgs", destroy_ppc, destroy_host, edge=timedq_edges, rnd=timedq_random))


# ======================================================================= unit w54: the leftovers on the destroy path
#
# Ten functions: the bar's and the text box's destructors, the base window's GetHelpText, GetText, GetValue, DrawFunction, Enable and
# operator-, InterfaceTimer's destructor and InterfaceStringHeap::ReleaseString. The windows of a case may be of four kinds
# (the fifth word of a window's spec: 0 window, 1 bar, 2 text box, 3 button). The virtual members are called through the window's
# own vtable on both sides.

import itertools

FLAGS = [0, 1, 2, -1, 0x7FFF, 0x10000, 0x10001, 0x18000, -0x8000]   # the deleting flag in r4 is the SHORT: only a positive one deletes
ONS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x10001, 0x18000, 0xFFFF]   # Enable's argument, the raw word (a short on the host)
HELPS = [-1, 0, 7, 0x7FFFFFFF, -0x80000000, 1, -2, 0x12345678]
CHARS = [0, 1, 0x41, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF]


def kforest(parents, kinds, attributes=None, handlers=None, destroyed=None):
    """The windows of a case with their kinds: (parent, attributes, handler, destroyed byte, kind) of each."""
    return [(parents[i], attributes[i] if attributes else 1, handlers[i] if handlers else 1 + i % 2, destroyed[i] if destroyed else 0, kinds[i])
            for i in range(len(parents))]


def text_ops(kinds, text=1):
    """A block of the heap for each text box (text 1) and a string for each button, so that the destructors have something to free."""
    ops = []
    for slot, kind in enumerate(kinds):
        if kind == 2 and text:
            ops.append((40, (slot, 1)))
        elif kind == 3 and text:
            ops.append((40, (slot, 2 + slot % STRINGS)))
    return ops


def string_ops():
    """The three strings of the heap start as ordinary text."""
    return [(39, (i, 0x41 + i, 0x61 + i, 0)) for i in range(STRINGS)]


def tdtor(index, flag):
    return (37, (index, flag))


def enable(slot, on):
    return (31, (slot, on))


def minus(slot, other):
    return (32, (slot, other))


# ---- the text box's and the bar's destructors


def tdtor_edges():
    # one text box, with and without a block, every flag; then as the child of a window that is destroyed (the deleting form of the
    # parent runs `while (mChild) delete mChild`, which a stand-in destructor would never leave)
    for flag in FLAGS:
        for text in (0, 1):
            yield case_of(text_ops([2], text) + [destroy(0, flag)], windows=kforest([-1], [2]))
            yield case_of(text_ops([0, 2], text) + [destroy(1, flag), destroy(0, 1)], windows=kforest([-1, 0], [0, 2]))
            yield case_of(text_ops([0, 2], text) + [destroy(0, flag)], windows=kforest([-1, 0], [0, 2]))
    # a window with a bar, a text box, a button and a plain window among its children, in every order, with and without text
    for order in itertools.permutations([0, 1, 2, 3]):
        kinds = [0] + list(order)
        for text in (0, 1):
            for flag in (0, 1):
                yield case_of(text_ops(kinds, text) + string_ops() + [destroy(0, flag)], windows=kforest([-1, 0, 0, 0, 0], kinds))
    # the children of every kind, four of them (4 ** 4 combinations): the parent's loop must end
    for combo in itertools.product(range(4), repeat=4):
        kinds = [0] + list(combo)
        yield case_of(text_ops(kinds, sum(combo) % 2) + [destroy(0, 1)], windows=kforest([-1, 0, 0, 0, 0], kinds))
    # a text box with children of its own, and text boxes nested
    for kinds in ([2, 1, 2, 3], [2, 2, 2, 2], [0, 2, 0, 2], [3, 2, 1, 0]):
        for slot in range(4):
            yield case_of(text_ops(kinds) + [destroy(slot, 1)], windows=kforest([-1, 0, 1, 1], kinds))
            yield case_of(text_ops(kinds) + [destroy(slot, 0), destroy(0, 1)], windows=kforest([-1, 0, 1, 1], kinds))
    # destroyed twice: the second time is skipped when the block was freed (the host would free it twice)
    yield case_of([destroy(0, 1), destroy(0, 1)], windows=kforest([-1, 0], [2, 2]))
    yield case_of(text_ops([2, 2]) + [destroy(1, 1), destroy(0, 1)], windows=kforest([-1, 0], [2, 2]))
    # the handlers hear message 0x14 before the text is freed? (the text goes first: the log shows the order); actions during the walk
    for action in range(1, 7):
        for first in range(3):
            yield case_of(text_ops([0, 2, 1]) + [destroy(0, 1)], windows=kforest([-1, 0, 0], [0, 2, 1]), q={"act": [0] * first + [action, 0]})
    # the other things the window owns go on being done: timers, queued messages, globals
    ops = wire() + [entry(0, 0, 1), entry(1, 1, 1, 1), timed_entry(0, 1)] + [(3, (w, 1)) for w in range(1, 10)]
    yield case_of(text_ops([0, 2]) + ops + [destroy(0, 1)], windows=kforest([-1, 0], [0, 2]))


def bdtor_edges():
    for flag in FLAGS:
        yield case_of([destroy(0, flag)], windows=kforest([-1], [1]))
        yield case_of([destroy(1, flag), destroy(0, 1)], windows=kforest([-1, 0], [0, 1]))
        yield case_of([destroy(0, flag)], windows=kforest([-1, 0, 1], [1, 1, 1]))
    # chains of five windows of every combination of kinds (4 ** 4 under a bar root), destroyed from the root and from the middle
    for combo in itertools.product(range(4), repeat=4):
        kinds = [1] + list(combo)
        yield case_of(text_ops(kinds) + [destroy(0, (sum(combo) % 2))], windows=kforest(chain(5), kinds))
    for combo in itertools.product(range(4), repeat=3):
        kinds = [0] + list(combo) + [1]
        yield case_of(text_ops(kinds) + [destroy(2, 1)], windows=kforest(chain(5), kinds))
    # bars among siblings, in every position
    for position in range(4):
        kinds = [0, 0, 0, 0]
        kinds[position] = 1
        yield case_of([destroy(0, 1)], windows=kforest([-1, 0, 0, 0], kinds))
        yield case_of([destroy(position, 0), destroy(0, 1)], windows=kforest([-1, 0, 0, 0], kinds))
    # a bar's value and rectangle are left as they were; the draw objects and the region the window has are let go
    yield case_of([(5, (0, 0, 1, 3)), (1, (1, 0, 1, 1, 0, 0)), destroy(0, 1)], windows=kforest([-1, 0], [0, 1]))
    yield case_of([(1, (0, 0, 1, 2, 1, 1)), (5, (1, 0, 1, 3)), destroy(0, 1)], windows=kforest([-1], [1]))
    for action in range(1, 7):
        yield case_of([destroy(0, 1)], windows=kforest([-1, 0, 0], [0, 1, 1]), q={"act": [action, 0]})


def rnd_case54(r, focus):
    """A random world with the four kinds of window, texts, strings and help numbers; then the operations of the function."""
    windows, ops, n, slot_pick = rnd_world(r, focus)
    kinds = [r.choice([0, 0, 1, 2, 2, 3, 3]) for _ in range(n)]
    windows = [w + (k,) for w, k in zip(windows, kinds)]
    ops = list(ops)
    ops += text_ops(kinds, r.choice([0, 1, 1]))
    ops += [(39, (i, r.choice(CHARS), r.choice(CHARS), r.choice(CHARS))) for i in range(STRINGS)]
    for slot in range(n):
        if r.random() < 0.4:
            ops.append((41, (slot, r.choice(HELPS))))
        if r.random() < 0.3:
            ops.append((42, (slot, r.choice(HELPS))))
        if kinds[slot] == 3 and r.random() < 0.6:
            ops.append((43, (slot, r.choice([0, 1, 2, 3, 0x7F, 0x81, -1]))))
    pick = lambda want: [s for s in range(n) if kinds[s] in want] or list(range(n))   # noqa: E731
    for _ in range(r.randint(1, 4)):
        if focus == "tdtor":
            ops.append(destroy(r.choice(pick([2])), r.choice(FLAGS)))
        elif focus == "bdtor":
            ops.append(destroy(r.choice(pick([1])), r.choice(FLAGS)))
        elif focus == "enable":
            ops.append(enable(r.randrange(n), r.choice(ONS)))
        elif focus == "minus":
            ops.append(minus(r.randrange(n), r.randrange(n)))
        elif focus == "help":
            ops.append((33, (r.randrange(n),)))
        elif focus == "text":
            ops.append((34, (r.randrange(n),)))
        elif focus == "value":
            ops.append((35, (r.choice(pick([0, 2, 3])),)))
        elif focus == "draw":
            ops.append((36, (r.randrange(n),)))
        elif focus == "timer":
            ops.append(tdtor(r.randrange(TIMERS), r.choice(FLAGS)))
            if r.random() < 0.3:
                ops.append((3, (11, r.choice([TIMERS, 6, 3, 0, -4]))))
            if r.random() < 0.3:
                ops.append((3, (15, r.randrange(TIMERS))))
        elif focus == "release":
            ops.append((38, (r.randrange(-1, STRINGS),)))
    return dict(windows=windows, queues=(8, 8, 8), ops=ops, q={"act": [r.choice([0, 0, 0, 0, 1, 2, 3, 4, 5, 6]) for _ in range(6)]})


def tdtor_random(r):
    return rnd_case54(r, "tdtor")


def bdtor_random(r):
    return rnd_case54(r, "bdtor")


TESTS.append(FnTest("InterfaceTextBox::~InterfaceTextBox", destroy_ppc, destroy_host, edge=tdtor_edges, rnd=tdtor_random))
TESTS.append(FnTest("InterfaceBar::~InterfaceBar", destroy_ppc, destroy_host, edge=bdtor_edges, rnd=bdtor_random))


# ---- the base window's virtual members: GetHelpText, GetText, GetValue, DrawFunction


def help_edges():
    kinds = [0, 1, 2, 3]
    # the constructor's -1, then every value, for every kind (a button answers its own: the second number while held down)
    for kind in kinds:
        yield case_of([(33, (0,))], windows=kforest([-1], [kind]))
        for value in HELPS:
            for second in (None, -1, 0, 5, 0x7FFFFFFF, -3):
                for state in ((None,) if kind != 3 else (None, 0, 1, 2, 3, 0x7F, 0x81, -1)):
                    ops = [(41, (0, value))]
                    if second is not None:
                        ops.append((42, (0, second)))
                    if state is not None:
                        ops.append((43, (0, state)))
                    yield case_of(ops + [(33, (0,))], windows=kforest([-1], [kind]))
    # a tree of the four kinds, each answering in turn
    for h in range(4):
        ops = [(41, (s, HELPS[(s + h) % len(HELPS)])) for s in range(4)] + [(43, (3, 1))] + [(33, (s,)) for s in range(4)]
        yield case_of(ops, windows=kforest([-1, 0, 0, 0], [0, 1, 2, 3]))
    yield case_of([(33, (s,)) for s in range(4)] + [(33, (s,)) for s in range(4)], windows=kforest([-1, 0, 0, 0], [3, 2, 1, 0]))


def text_edges():
    kinds = [0, 1, 2, 3]
    # NULL for the base (and the bar and the text box, which do not override it); a button answers its text (a string of the heap)
    for kind in kinds:
        yield case_of([(34, (0,))], windows=kforest([-1], [kind]))
        for string in range(STRINGS):
            yield case_of(string_ops() + [(40, (0, 2 + string)), (34, (0,))], windows=kforest([-1], [kind]))
        yield case_of([(40, (0, 0)), (34, (0,))], windows=kforest([-1], [kind]))
    yield case_of(string_ops() + [(40, (3, 2)), (40, (2, 1))] + [(34, (s,)) for s in range(4)], windows=kforest([-1, 0, 0, 0], [0, 1, 2, 3]))
    yield case_of(string_ops() + [(40, (1, 3)), (40, (2, 3))] + [(34, (s,)) for s in range(4)], windows=kforest([-1, 0, 0, 0], [0, 3, 3, 3]))


def value_edges():
    # 0 for the base, the text box and the button (the bar answers its own value: not run here)
    for kind in (0, 2, 3):
        for text in (0, 1):
            yield case_of(text_ops([kind], text) + [(35, (0,)), (35, (0,))], windows=kforest([-1], [kind]))
    for attributes in (0, 1, 2, 3, 0xF0000000, -1):
        yield case_of([(35, (s,)) for s in range(3)], windows=kforest([-1, 0, 0], [0, 2, 3], attributes=[attributes] * 3))
    yield case_of([(41, (0, 5)), (42, (0, 6)), (35, (0,))], windows=kforest([-1], [0]))


def draw_edges():
    # nothing happens, whatever the window has (the log and every field are compared)
    for kind in range(4):
        yield case_of([(36, (0,))], windows=kforest([-1], [kind]))
        yield case_of([(5, (0, 0, 1, 3)), (1, (0, 0, 1, 0, 0, 1)), (3, (3, 0)), (3, (4, 0)), (36, (0,)), (36, (0,))], windows=kforest([-1], [kind]))
    yield case_of([(36, (s,)) for s in range(4)], windows=kforest([-1, 0, 0, 0], [0, 1, 2, 3]))


TESTS.append(FnTest("InterfaceWindow::GetHelpText", destroy_ppc, destroy_host, edge=help_edges, rnd=lambda r: rnd_case54(r, "help")))
TESTS.append(FnTest("InterfaceWindow::GetText", destroy_ppc, destroy_host, edge=text_edges, rnd=lambda r: rnd_case54(r, "text")))
TESTS.append(FnTest("InterfaceWindow::GetValue", destroy_ppc, destroy_host, edge=value_edges, rnd=lambda r: rnd_case54(r, "value")))
TESTS.append(FnTest("InterfaceWindow::DrawFunction", destroy_ppc, destroy_host, edge=draw_edges, rnd=lambda r: rnd_case54(r, "draw")))


# ---- Enable and operator-


def enable_edges():
    attributes_list = [0, 1, 2, 3, 6, 0x12, 0xF0000002, 0xF0000000, -3]
    # every kind (a button runs its own Enable, which calls this one and then tells the draw objects the frame), the bit set and
    # clear, every handler (none, the two, the default), every argument; twice in a row
    for kind in range(4):
        for attributes in attributes_list:
            for handler in (0, 1, 2, 5):
                for on in ONS:
                    ops = [(5, (0, 0, 1, 3)), (5, (1, 0, 1, 3)), (1, (0, 0, handler, 0, 0, 1)), (43, (0, 2)), enable(0, on), enable(0, on)]
                    yield case_of(ops, windows=kforest([-1], [kind], attributes=[attributes], handlers=[handler]))
    # the handler may flip the bit or drop itself while it runs (the answer was worked out before)
    for kind in (0, 3):
        for action in range(1, 7):
            for attributes in (1, 3):
                for on in (0, 1):
                    yield case_of([enable(0, on), enable(0, 1 - on), enable(0, on)], windows=kforest([-1], [kind], attributes=[attributes]),
                                  q={"act": [action, 0, action]})
    # the sequence on the one window: off, off, on, on, off (the bit changes only on the first of each pair)
    for kind in range(4):
        yield case_of([enable(0, v) for v in (0, 0, 1, 1, 0, 2, 0, -1)], windows=kforest([-1], [kind]))
    # a tree: the children and the parent in turn (the handler of each is called on its own window)
    for kinds in ([0, 1, 2, 3], [3, 3, 3, 3], [3, 0, 3, 0]):
        for attributes in (1, 3):
            ops = [enable(s, 0) for s in range(4)] + [enable(s, 1) for s in (3, 2, 1, 0)] + [enable(s, 0) for s in (1, 1)]
            yield case_of(ops, windows=kforest([-1, 0, 0, 1], kinds, attributes=[attributes] * 4))
    # the pointer is worked out again only when the bit changed
    yield case_of([(3, (1, 0)), enable(0, 1), enable(0, 1)], windows=kforest([-1], [0], attributes=[1]))


def minus_edges():
    # the base operator- on every pair of kinds (this one's WhatAmI first, then the other's), also a window with itself
    for a, b in itertools.product(range(4), repeat=2):
        yield case_of([minus(0, 1)], windows=kforest([-1, 0], [a, b]))
        yield case_of([minus(1, 0), minus(0, 1)], windows=kforest([-1, 0], [a, b]))
    for kind in range(4):
        yield case_of([minus(0, 0)], windows=kforest([-1], [kind]))
    kinds = [0, 1, 2, 3]
    yield case_of([minus(i, j) for i in range(4) for j in range(4)], windows=kforest([-1, 0, 0, 0], kinds))
    yield case_of([minus(i, j) for i in range(4) for j in range(4)], windows=kforest([-1, 0, 0, 0], kinds[::-1]))
    # nothing else changes: attributes, children, links
    yield case_of([(3, (3, 1)), (3, (4, 2)), minus(0, 2)], windows=kforest([-1, 0, 0], [0, 2, 2], attributes=[3, 3, 0]))


TESTS.append(FnTest("InterfaceWindow::Enable", destroy_ppc, destroy_host, edge=enable_edges, rnd=lambda r: rnd_case54(r, "enable")))
TESTS.append(FnTest("InterfaceWindow::operator-", destroy_ppc, destroy_host, edge=minus_edges, rnd=lambda r: rnd_case54(r, "minus")))


# ---- the timer's destructor, ReleaseString


def timer_dtor_edges():
    tables = [TIMERS, 5, 3, 1, 0, -4]
    # lists of one to six timers linked in order, the destructor at every place, every flag (plain or deleting), every table
    for length in range(1, 7):
        entries = [(k, k % 3, 10 + k) for k in range(length)]
        for place in range(length):
            for flag in FLAGS:
                for table in tables:
                    yield case_of(wire(timers=table) + timer_list(entries) + [tdtor(place, flag)])
    # twice: the second unlink goes through the links the first left (the timer's own are not cleared)
    for length in range(2, 6):
        entries = [(k, 0, 10 + k) for k in range(length)]
        for place in range(length):
            yield case_of(wire() + timer_list(entries) + [tdtor(place, 1), tdtor(place, 1)])
            yield case_of(wire() + timer_list(entries) + [tdtor(place, 0), tdtor((place + 1) % length, 1)])
    # the whole list taken apart from the head, from the tail, from the middle outwards
    for order in ([0, 1, 2, 3], [3, 2, 1, 0], [1, 2, 0, 3], [2, 1, 3, 0]):
        yield case_of(wire() + timer_list([(k, 0, k) for k in range(4)]) + [tdtor(k, 1) for k in order])
    # a table that starts elsewhere: the timers before it and after its end are outside (the assertion), those inside are not
    for base in range(0, 8, 2):
        for count in (0, 1, 2, 4, 6):
            for index in range(0, 10, 3):
                yield case_of(wire(timers=count) + [(3, (15, base))] + timer_list([(index, 0, 5)]) + [tdtor(index, 1)])
    # triage 2026-10-13a (doubtful 9a): a table that starts at a timer of the world with a NEGATIVE count (the end is below the start)
    for base in (0, 2, 5):
        for count in (-1, -3):
            for index in (0, 3, 6):
                for flag in (0, 1):
                    yield case_of(wire(timers=count) + [(3, (15, base))] + timer_list([(index, 0, 5)]) + [tdtor(index, flag)])
    # no table at all (a count with no table, none), and a table of one timer: the assertion, then the slot is freed anyway
    for table in (-3, 0, 1):
        for free in (0, 1):
            yield case_of(wire(timers=table) + [timer_op(2, 0, 4, free=free), tdtor(2, 1)])
    # a timer whose links are not in the list, one that links to itself, one whose neighbours point elsewhere
    yield case_of(wire() + [timer_op(0, 0, 1, nxt=0, prev=0), tdtor(0, 1)])
    yield case_of(wire() + [timer_op(0, 0, 1, nxt=1, prev=-1), timer_op(1, 0, 2, nxt=-1, prev=5), tdtor(0, 1)])
    yield case_of(wire() + [timer_op(0, 0, 1, nxt=-1, prev=1), timer_op(1, 0, 2, nxt=4, prev=-1), tdtor(0, 0)])
    # running, slice and start are left alone; the free flag becomes 1 only in the deleting form
    for running in (0, 1, 3):
        for free in (0, 1, 2, -1):
            for flag in (0, 1):
                yield case_of(wire() + [timer_op(3, 1, 9, free=free, running=running, slice_=-4, start=0x12345678), (3, (10, 3)), tdtor(3, flag)])
    yield case_of([tdtor(5, 1)])
    yield case_of(wire(timers=0) + [tdtor(5, 0)])


def release_edges():
    patterns = [(0, 0, 0), (0xFFFF, 0xFFFF, 0), (1, 0xFFFF, 5), (0xFFFF, 0, 1), (0x1234, 0x5678, 0x9ABC), (0x8000, 0x7FFF, 0xFFFF), (0xFFFE, 0xFFFE, 0xFFFE)]
    for pattern in patterns:
        for index in range(-1, STRINGS):
            yield case_of([(39, (i, *pattern)) for i in range(STRINGS)] + [(38, (index,))])
            yield case_of([(39, (i, *pattern)) for i in range(STRINGS)] + [(38, (index,)), (38, (index,))])
    # one after the other: only the first two characters of each change, the neighbours stay
    yield case_of(string_ops() + [(38, (0,)), (38, (2,)), (38, (1,))])
    yield case_of(string_ops() + [(38, (-1,)), (38, (1,))])
    # the button's destructor gives its text back through it
    for text in range(STRINGS):
        yield case_of(string_ops() + [(40, (1, 2 + text)), destroy(1, 1)], windows=kforest([-1, 0, 0], [0, 3, 3]))
        yield case_of(string_ops() + [(40, (1, 2 + text)), (40, (2, 2 + (text + 1) % STRINGS)), destroy(0, 1)], windows=kforest([-1, 0, 0], [0, 3, 3]))
    yield case_of(string_ops() + [(40, (1, 2)), (40, (2, 2)), destroy(0, 1)], windows=kforest([-1, 0, 0], [0, 3, 3]))
    yield case_of([destroy(0, 1)], windows=kforest([-1, 0], [0, 3]))


TESTS.append(FnTest("InterfaceTimer::~InterfaceTimer", destroy_ppc, destroy_host, edge=timer_dtor_edges, rnd=lambda r: rnd_case54(r, "timer")))
TESTS.append(FnTest("InterfaceStringHeap::ReleaseString", destroy_ppc, destroy_host, edge=release_edges, rnd=lambda r: rnd_case54(r, "release")))
