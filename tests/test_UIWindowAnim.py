"""Differential tests for the window's animation starters and the check buttons' destructor (unit w91), against
libtpw_unit_UIWindowAnim.dylib (tests/unit_UIWindowAnim.cpp):

  src/InterfaceWindow_anim.cpp         InterfaceWindow::setPivot, startAnim, startAnimTimer
  src/InterfaceCheckButtons_dtor.cpp   InterfaceCheckButtons::~InterfaceCheckButtons

Real on both sides: those, SetPosition (which startAnim calls, with ShiftPosition and HandleScale under it), the window's constructor
(src/InterfaceWindow.cpp, which builds the test trees), InterfaceTimer::Start and the rectangles (src/UI_Base.cpp). Fakes that record the
same event on both sides: the message senders, UI_UpdatePointerIn, UI_SetCursor, the UI clock (IveGotTicks, answers from a queue), the
window's untranslated virtual members (DrawFunction, CalcClientArea, the destructor, GetHelpText) and Show, the draw objects (Draw and
Change record the fields their flags name, the pivot pair of flag 0x40 included), the help text handler, two regions (Shift) and the
timers (plain storage). The check buttons' constructor is the original's on the PowerPC side and a fake on the host (the current
button -1, a dummy callback in both message function members).

A case is a script of operations on a world of eight windows (a slot holds a plain window or a check buttons object; the last is a
spare), five draw objects, two regions, two timers, three animation tables and a help text object. After each operation the answer, every
window, the globals, the timers (running flag and start tick), the heap traffic and the length of the call log are compared, and the whole
call log at the end. The PowerPC side decides which operations the state allows (one that would make the original read past the animation
timers, a missing window or walk a cycle is skipped on both sides).

Quirks the tests pin: startAnim with a NULL table still undoes a running animation (and leaves mAnimating at 1), the rectangle it undoes
is the window's own at +8 less the offsets, setPivot visits the parent, then each child with its subtree in order (the fifth level
and deeper through the recursion), the check buttons' destructor deletes only when its flag is positive.
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIWindowAnim"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(SetPivot=0x101DDD34, StartAnim=0x101DD08C, StartAnimTimer=0x101DD6B8, CheckDtor=0x101CC02C, CheckCtor=0x101CBF90,
          TimerStart=0x101D8460, Shift=0x101DA9D0, HandleScale=0x101DAC2C, SetPosition=0x101DAE98, WindowCtor=0x101D98D8,
          MakeAbs=0x101DAB88)
# what they call that is real too: the rectangles and the regions' base (CoolRect's constructors, Adjust and destructor among them)
REAL_HELPERS = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788,
                0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74]
# the fakes (in-binary functions)
SEND_MESSAGE, POST_MESSAGE, UPDATE_POINTER_IN, SET_CURSOR, IVE_GOT_TICKS = 0x101C9518, 0x101C9400, 0x101C8F18, 0x101C9F68, 0x101CAC3C
DRAW_FUNCTION, CALC_CLIENT_AREA, WINDOW_DTOR, GET_HELP_TEXT, WINDOW_SHOW = 0x1018A270, 0x101D987C, 0x101D9BA4, 0x1018AA0C, 0x101DC1EC

# ---- the original's data
G = dict(focus=0x10395C20, help=0x102AF928, buttons=0x10395BD0, dbl=0x102AF91C, redraw=0x10395C28, result=0x10395C60)
RESULT_ADDRESS = 0x10395C60
CORE_VTABLE, DEFAULT_CALLBACK_TV = 0x102AF8FC, 0x10268480   # CoolRect's vtable; UI_DefaultWindowCallback's TVector
SIZE_WINDOW = 0x138
SIZE_BUTTONS = 0x13C   # the check buttons add the current button's id at 0x138
OPERATOR_DELETE = "__dl__FPv"

# ---- labels (the host's tpw_uiwd_labels)
LAB_WINDOW, LAB_DRAW, LAB_REGION, LAB_TIMER, LAB_HELP, LAB_TABLE = 10, 20, 30, 40, 50, 60
SLOTS, DRAWS, REGIONS, TIMERS, TABLES = 8, 5, 2, 2, 3
SPARE = SLOTS - 1                  # the window that is never part of a tree: the deleting destructor is given this one
TABLE_FRAMES = 64                  # frames past the ones a case sets are zero
MAX_QUEUE = 64                     # the call log's return queues hold this many values; the last repeats

# the values the draw objects' events and the handler's messages use
ANIM_END, ANIM_ZROT, ANIM_GOTO, ANIM_FRAME = -9999, -9998, -9997, -9996


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def pack_position(x, y):
    """A pointer position as the handler's messages carry it: x in the high half, y in the low (a negative y is sign
    extended into the word, as UI_DevicePointerMove does)."""
    return T.s32((((x & 0xFFFF) << 16) | (y & MINUS)) & MINUS)


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures and the operations."""

    ADDRESSES = [(G["focus"], 4), (G["help"], 4), (G["buttons"], 36), (G["dbl"], 4), (G["redraw"], 4), (G["result"], 8)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.windows = [e.alloc(SIZE_BUTTONS, b"\xa5" * SIZE_BUTTONS) for _ in range(SLOTS)]
        self.made = [False] * SLOTS
        self.buttons = [False] * SLOTS   # the slots that hold check buttons
        self.draws = []
        for _ in range(DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.uiwd_draw_vtable)
            self.draws.append(block)
        self.regions = []
        for _ in range(REGIONS):
            block = e.alloc(8, bytes(8))
            w32(e, block, e.uiwd_region_vtable)
            self.regions.append(block)
        self.timers = [e.alloc(0x20, b"\xa5" * 0x10 + struct.pack(">i", 1) + b"\xa5" * 0xC) for _ in range(TIMERS)]
        self.help = e.alloc(0x18, bytes(0x18))
        w32(e, self.help + 0x10, e.uiwd_help_vtable)
        self.tables = []
        for k in range(TABLES):
            words = list(case.get("tables", ())[k]) if k < len(case.get("tables", ())) else []
            data = b"".join(struct.pack(">4h", *[s16(v) for v in frame]) for frame in words)
            data += bytes(8 * TABLE_FRAMES - len(data))
            self.tables.append(e.alloc(len(data), data))
        fixed = {}
        fixed.update({a: LAB_DRAW + i for i, a in enumerate(self.draws)})
        fixed.update({a: LAB_REGION + i for i, a in enumerate(self.regions)})
        fixed.update({a: LAB_TIMER + i for i, a in enumerate(self.timers)})
        fixed[self.help] = LAB_HELP
        fixed.update({a: LAB_TABLE + i for i, a in enumerate(self.tables)})
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues)
        # the globals as the Mac's data has them
        w32(e, G["focus"], 0)
        w32(e, G["help"], self.help)
        e.write(G["buttons"], bytes(36))
        w32(e, G["dbl"], 500)
        w32(e, G["redraw"], 0)
        e.write(G["result"], bytes(8))

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def window(self, slot):
        return self.windows[slot] if 0 <= slot < SLOTS and self.made[slot] else 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def rect_code(self, address):
        for slot, w in enumerate(self.windows):
            if self.made[slot]:
                k = {4: 1, 0x10: 2, 0x1C: 3, 0x28: 4, 0x34: 5}.get(address - w)
                if k:
                    return (slot + 1) * 10 + k
        return -1

    def acyclic(self):
        """False when the windows' links make a loop (the original would never come back from one)."""
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

        return all(visit(w) for w in known) and all(self.parent_chain_ok(w, known) for w in known)

    def parent_chain_ok(self, w, known):
        seen = set()
        while w and w in known:
            if w in seen:
                return False
            seen.add(w)
            w = self.e.u32(w + 0x5C)
        return True

    def u32(self, slot, offset):
        return self.e.u32(self.windows[slot] + offset)

    def s16at(self, slot, offset):
        return struct.unpack(">h", self.e.read(self.windows[slot] + offset, 2))[0]

    # ---- the dumps (the orders are tpw_uiwd_dump_* in the library)
    def dump_window(self, address):
        d = bytes(self.e.read(address, SIZE_WINDOW))
        L = self.L
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
        out += [0 if u32_(0x110) == 0 else 1 if u32_(0x110) == DEFAULT_CALLBACK_TV else 2,
                0 if u32_(0x114) == 0 else 1 if u32_(0x114) == DEFAULT_CALLBACK_TV else 2, s32_(0x11C)]
        out += [lab(0x120 + 4 * i) for i in range(4)]
        out += [s32_(0x130), d[0x134], d[0x135], T.s8(d[0x136])]
        if self.buttons[self.windows.index(address)]:   # the check buttons' current button (0x138)
            out.append(T.s32(self.e.u32(address + 0x138)))
        else:
            out.append(0)
        return out

    def dump_globals(self):
        e, L = self.e, self.L
        out = [L.lab(e.u32(G["focus"])), T.s32(e.u32(G["dbl"])), T.s32(e.u32(G["redraw"])), T.s32(e.u32(G["result"])),
               L.lab(e.u32(self.help + 0x14))]
        for i in range(3):
            base = G["buttons"] + 12 * i
            out += list(struct.unpack(">3h", e.read(base, 6))) + [T.s32(e.u32(base + 8))]
        for t in self.timers:
            out.append(T.s32(e.u32(t + 0x10)))
        for t in self.timers:
            out.append(T.s32(e.u32(t + 4)))
        out.append(L.lab(e.u32(G["help"])))
        return out

    def dump_state(self):
        return dict(windows=[self.dump_window(self.windows[s]) if self.made[s] else [0] for s in range(SLOTS)],
                    globals=self.dump_globals(), heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        needs_window = code in (2, 3, 4, 5, 6, 8, 9, 10, 40, 41, 42) or (code == 43 and a[1] != 2)
        if needs_window and not self.window(a[0]):
            return None
        if code == 1:
            slot, parent = a[0], a[1]
            if self.made[slot] or (parent >= 0 and not self.made[parent]) or parent >= slot:
                return None
            e = self.e
            e.write(self.windows[slot], b"\xa5" * SIZE_BUTTONS)
            self.L.labels[self.windows[slot]] = LAB_WINDOW + slot
            if a[8]:
                self.call(FN["CheckCtor"], self.windows[slot], self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]),
                          s16(a[7]))
            else:
                self.call(FN["WindowCtor"], self.windows[slot], self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]),
                          s16(a[7]))
            self.made[slot] = True
            self.buttons[slot] = bool(a[8])
            return [self.L.lab(self.windows[slot]), 0, 0, 0, 0]
        if code in (2, 3, 4, 5, 6, 8, 9, 10):
            self.setup(code, a)
            if code == 2 and not self.acyclic():
                raise AssertionError("the script made a loop of links")
            return [0, 0, 0, 0, 0]
        if code == 7:
            return self.set_global(a)
        return self.function(code, a)

    def setup(self, code, a):
        e = self.e
        w = self.windows[a[0]]
        if code == 2:   # links: slot, parent, previous, next, child
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
        elif code == 3:   # fields: slot, attributes, destroying, region, cursor
            w32(e, w + 0x44, a[1])
            e.write(w, bytes([a[2] & 0xFF]))
            w32(e, w + 0x40, w + 4 if a[3] == 0 else self.regions[a[3] - 1])
            e.write(w + 0x4C, struct.pack(">h", s16(a[4])))
        elif code == 4:   # draw objects: slot, the window's, the client's
            w32(e, w + 0x94, self.draws[a[1]] if a[1] >= 0 else 0)
            w32(e, w + 0x98, self.draws[a[2]] if a[2] >= 0 else 0)
        elif code == 5:   # animation: slot, table, frame, ticks, start, animating, offsets
            w32(e, w + 0xE4, self.tables[a[1]] if a[1] >= 0 else 0)
            e.write(w + 0xE8, struct.pack(">h", s16(a[2])))
            w32(e, w + 0xEC, a[3])
            w32(e, w + 0xF0, a[4])
            e.write(w + 0x135, bytes([a[5] & 0xFF]))
            e.write(w + 0xF4, struct.pack(">4h", *[s16(v) for v in a[6:10]]))
        elif code == 6:   # slide: slot, timer, steps, steps left, sliding, target rectangle
            w32(e, w + 0xFC, self.timers[a[1]] if a[1] >= 0 else 0)
            e.write(w + 0x100, struct.pack(">4h", *[s16(v) for v in a[2:6]]))
            e.write(w + 0x108, struct.pack(">h", s16(a[6])))
            e.write(w + 0x136, bytes([a[7] & 0xFF]))
            e.write(w + 0x38, struct.pack(">4h", *[s16(v) for v in a[8:12]]))
        elif code == 10:   # the client rectangle: slot, left, top, right, bottom
            e.write(w + 0x2C, struct.pack(">4h", *[s16(v) for v in a[1:5]]))
        elif code == 9:   # animation timers: slot, the four timers
            for i in range(4):
                w32(e, w + 0x120 + 4 * i, self.timers[a[1 + i]] if a[1 + i] >= 0 else 0)
        elif code == 8:   # depth and pivot: slot, z, z rotation, pivot x, pivot y, buttons held
            e.write(w + 0xDC, struct.pack(">h", s16(a[1])))
            e.write(w + 0xDE, struct.pack(">h", s16(a[2])))
            e.write(w + 0xE0, struct.pack(">hh", s16(a[3]), s16(a[4])))
            w32(e, w + 0x11C, a[5])

    def set_global(self, a):
        e = self.e
        which, v = a[0], a[1]
        if which == 1:
            w32(e, G["focus"], self.window(v))
        elif which == 2:
            w32(e, G["dbl"], v)
        elif which == 3:
            w32(e, G["redraw"], v)
        elif which == 4:
            w32(e, self.help + 0x14, self.window(v))
        elif which == 5:   # a button: index, state, x, y, time
            e.write(G["buttons"] + 12 * v, struct.pack(">3h", s16(a[2]), s16(a[3]), s16(a[4])))
            w32(e, G["buttons"] + 12 * v + 8, a[5])
        elif which == 6:
            w32(e, self.timers[v] + 0x10, a[2])
        elif which == 7:
            w32(e, G["result"], v)
        return [0, 0, 0, 0, 0]

    def allowed(self, code, a):
        """False for an operation the state does not allow: the original would follow a cycle, delete a window that is not check
        buttons or read past the animation timers."""
        if code in (40, 41) and not self.acyclic():
            return False
        if code == 42:
            return a[1] in (0, 1, 2, 3, 0x80080, 0x80081, 0x80082, 0x80083)
        if code == 43 and a[1] != 2:
            deleting = s16(a[2]) > 0
            if a[1] == 0:   # a window the deleting form frees must have no links (the parent would keep a dangling child)
                w = self.windows[a[0]]
                if any(self.e.u32(w + o) for o in (0x50, 0x54, 0x58, 0x5C)):
                    return False
            return self.buttons[a[0]] and deleting == (a[1] == 0)
        return True

    def function(self, code, a):
        if not self.allowed(code, a):
            return None
        e, L = self.e, self.L
        w = self.windows[a[0]] if 0 <= a[0] < SLOTS else 0
        answer = [0, 0, 0, 0, 0]
        if code == 40:
            self.call(FN["SetPivot"], w, s16(a[1]), s16(a[2]))
        elif code == 41:   # startAnim: slot, table, ticks
            answer[0] = T.s32(self.call(FN["StartAnim"], w, self.tables[a[1]] if a[1] >= 0 else 0, a[2]))
        elif code == 42:
            answer[0] = T.s32(self.call(FN["StartAnimTimer"], w, a[1]))
        elif code == 43:   # the check buttons' destructor: slot, mode (0 deleting form, 1 plain destructor, 2 NULL), the flag in r4
            if a[1] == 2:
                self.call(FN["CheckDtor"], 0, a[2])
            else:
                self.call(FN["CheckDtor"], w, a[2])
                if a[1] == 0:
                    self.made[a[0]] = False
                    self.buttons[a[0]] = False
        else:
            raise ValueError(code)
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects', help text's and regions' vtables and the fakes the Python side shares, in persistent memory."""
    if hasattr(e, "uiwd_draw_vtable"):
        return

    def draw(emu):
        us = emu.us
        this, a, rect, b = emu.r(3), s16(emu.r(4)), emu.r(5), s16(emu.r(6))
        coordinates = struct.unpack(">4h", emu.read(rect + 4, 8))
        emu.cl.rec("Draw", (emu.cl.lab(this), a, us.rect_code(rect)) + coordinates + (b,))

    def change(emu):
        us = emu.us
        this, flags, record = emu.r(3), emu.r(4), emu.r(5)
        values = [emu.cl.lab(this), flags]
        if flags & 1:
            values.append(struct.unpack(">h", emu.read(record + 4, 2))[0])
        if flags & 4:
            values.append(struct.unpack(">h", emu.read(record + 8, 2))[0])
        if flags & 0x10:
            rect = emu.u32(record + 0x14)
            values.append(us.rect_code(rect))
            values += struct.unpack(">4h", emu.read(rect + 4, 8))
        if flags & 0x40:
            values += struct.unpack(">2h", emu.read(record + 0xC, 4))
        if flags & 0x80:
            values.append(struct.unpack(">i", emu.read(record, 4))[0])
        values += [0] * (9 - len(values))
        emu.cl.rec("Change", tuple(values))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x0C, struct.pack(">I", T.fake_function(e, draw)))
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.uiwd_draw_vtable = vt

    def moved(emu):
        emu.cl.rec("Help.OnMouseMoved", (s16(emu.r(4)), s16(emu.r(5))))

    def moved_in(emu):
        emu.cl.rec("Help.OnMouseMovedIn", (emu.cl.lab(emu.r(4)),))
        w32(emu, emu.r(3) + 0x14, emu.r(4))

    def moved_out(emu):
        emu.cl.rec("Help.OnMouseMovedOut")
        w32(emu, emu.r(3) + 0x14, 0)

    vt = T.persist_alloc(e, 0x48)
    for slot, fn in ((0x28, moved), (0x2C, moved_in), (0x30, moved_out), (0x38, lambda emu: emu.cl.rec("Help.OnFrame"))):
        e.write(vt + slot, struct.pack(">I", T.fake_function(e, fn)))
    e.uiwd_help_vtable = vt

    def shift(emu):
        emu.cl.rec("Region.Shift", (emu.cl.lab(emu.r(3)), s16(emu.r(4)), s16(emu.r(5))))

    vt = T.persist_alloc(e, 0x20)
    e.write(vt + 0x10, struct.pack(">I", T.fake_function(e, shift)))
    e.uiwd_region_vtable = vt


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL_HELPERS)
    R = CL.recorder

    def count_delete(e, L):
        if T.s16(e.r(4)) > 0:
            e.us.heap[2] += 1

    S.internal(SEND_MESSAGE, R("SendMessage", "Liii", ret="i"))
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(SET_CURSOR, R("SetCursor", "h"))
    S.internal(IVE_GOT_TICKS, R("IveGotTicks", "", ret="i"))
    S.internal(DRAW_FUNCTION, R("DrawFunction", "L"))
    calc_client = R("CalcClientArea", "L")

    def calc_client_area(e):
        calc_client(e)
        this = e.r(3)
        e.write(this + 0x2C, bytes(e.read(this + 8, 8)))   # (the real one copies the rectangle to the client rectangle)

    S.internal(CALC_CLIENT_AREA, calc_client_area)
    S.internal(GET_HELP_TEXT, R("GetHelpText", "L", ret="i"))
    S.internal(WINDOW_SHOW, R("Window.Show", "Lh", ret=lambda e, L: 1))
    S.internal(WINDOW_DTOR, R("Window.dtor", "D", then=count_delete))
    def operator_delete(e):
        # the original's deleting destructor frees the block itself (the host's operator delete counts and logs the same)
        block = e.r(3)
        if block:
            e.us.heap[2] += 1
            if e.cl.lab(block) > 0:
                e.cl.rec("delete", (e.cl.lab(block),))
            e.cl.forget(block)

    S.imp(OPERATOR_DELETE, operator_delete)
    S.imp("abs", lambda e: e.set_r(3, T.u32(abs(T.s32(e.r(3))))))
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uiwd_open", False):
        return
    import ctypes as C
    lib.tpw_uiwd_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_uiwd_table.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int16)]
    lib.tpw_uiwd_dump_window.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uiwd_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uiwd_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._uiwd_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        open_host(lib)
        lib.tpw_uiwd_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues)
        lib.tpw_uiwd_labels()
        for k, frames in enumerate(case.get("tables", ())):
            words = (C.c_int16 * max(1, 4 * len(frames)))(*[s16(v) for frame in frames for v in frame])
            lib.tpw_uiwd_table(k, len(frames), words)

    def op(self, code, a):
        C = self.C
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 5)()
        self.lib.tpw_uiwd_op(code, args, out)
        return list(out)

    def dump_state(self, step):
        C, lib = self.C, self.lib
        buf = (C.c_int64 * 256)()
        windows = []
        for slot in range(SLOTS):
            if step["made"][slot]:
                lib.tpw_uiwd_dump_window(slot, buf)
                windows.append(list(buf[:WINDOW_FIELDS]))
            else:
                windows.append([0])
        lib.tpw_uiwd_dump_globals(buf)
        globals_ = list(buf[:GLOBAL_FIELDS])
        lib.tpw_uiwd_heap(buf)
        return dict(windows=windows, globals=globals_, heap=list(buf[:4]), log=C.c_int32.in_dll(lib, "tpw_cl_n").value)


WINDOW_FIELDS = 69
GLOBAL_FIELDS = 22


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


# ======================================================================= scripts: building blocks

TESTS = []

SMALL = [0, 0, 1, -1, 2, -3, 7, -8, 20, -33, 100, -250, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x12345, -0x12345]
ANCHORS = [0, 0x10000, 0x20000, 0x30000, 0x40000, 0x80000, 0xC0000, 0xF0000, 0x50000, 0xA0000, 0x70000, 0xB0000, 0xD0000, 0xE0000,
           0x90000, 0x60000]
KINDS = [0x10000000, 0x20000000, 0x40000000, 0x80000000, 0xF0000000, 0x70000000]


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


def default_rects(n):
    return [(s * 10, s * 8, s * 10 + 60, s * 8 + 50) for s in range(n)]


def world(parents, rects=None, attributes=None, regions=None, draws=None, cursors=None, kinds=None):
    """The windows 0.. made with these parents (-1 for the root, which must be slot 0), linked, with their fields and draw
    objects set. Window 0 is the only root, so every list of siblings has a parent."""
    n = len(parents)
    rects = rects or default_rects(n)
    attributes = attributes or [1] * n
    regions = regions or [0] * n
    draws = draws or [(-1, -1)] * n
    cursors = cursors or [-1] * n
    kinds = kinds or [0] * n
    ops = [(1, (s, parents[s], attributes[s], 100 + s) + tuple(rects[s]) + (kinds[s],)) for s in range(n)]
    ops += link_ops(parents)
    for s in range(n):
        ops.append((3, (s, attributes[s], 0, regions[s], cursors[s])))
        ops.append((4, (s,) + tuple(draws[s])))
    return ops


def spare_ops(rect=(5, 6, 55, 66), attributes=1, kind=1):
    """The spare: alone, made (check buttons by default), the only one the deleting destructor may be given."""
    return [(1, (SPARE, -1, attributes, 77) + rect + (kind,)), (3, (SPARE, attributes, 0, 0, -1)), (4, (SPARE, -1, -1))]


def case_of(ops, q=None, tables=None):
    case = dict(ops=ops)
    if q:
        case["q"] = q
    if tables:
        case["tables"] = tables
    return case


def rnd_parents(r, n):
    chain = r.choice([0.2, 0.5, 0.9])
    return [-1] + [s - 1 if r.random() < chain else r.randrange(s) for s in range(1, n)]


def rnd_rect(r, around=150):
    left, top = r.randint(-30, around), r.randint(-30, around)
    return (left, top, left + r.choice([0, 1, 2, r.randint(-5, 200), r.randint(1, 100)]),
            top + r.choice([0, 1, 2, r.randint(-5, 200), r.randint(1, 100)]))


def rnd_draws(r):
    return (r.choice([-1, -1, 0, 1, 2, 3, 4]), r.choice([-1, -1, 0, 1, 2, 3, 4]))


def draw_attribute(r):
    """Mostly visible; some hidden, some of the kinds that draw nothing themselves."""
    if r.random() < 0.12:
        return r.choice([0, 2, 8, 0x400, 0x10000000])
    value = 1
    if r.random() < 0.18:
        value |= r.choice(KINDS)
    if r.random() < 0.2:
        value |= r.choice([2, 8, 0x400, 0x30000, 0xF0000])
    return value


def anchor_attribute(r):
    return (1 if r.random() < 0.85 else r.choice([0, 2, 8])) | r.choice(ANCHORS) | (r.choice([0x400, 0x8, 0x2]) if r.random() < 0.1 else 0)


def any_attribute(r):
    if r.random() < 0.6:
        return draw_attribute(r) | r.choice(ANCHORS)
    return r.getrandbits(32)


def rnd_world(r, attribute, n=None, regions=True):
    n = n or r.randint(1, SLOTS - 1)
    parents = rnd_parents(r, n)
    ops = world(parents, [rnd_rect(r) for _ in range(n)], [attribute(r) for _ in range(n)],
                [r.choice([0, 0, 0, 1, 2]) if regions else 0 for _ in range(n)], [rnd_draws(r) for _ in range(n)],
                [r.choice([-1, 0, 3, 9]) for _ in range(n)])
    return ops, n


def rnd_ticks(r, length=None, start=None):
    """Clock readings, rising by small and large steps (some start near the wrap); the last repeats."""
    length = length or r.randint(2, 40)
    t = r.choice([0, 100, 5000, 0xFFFFFF00, 0x7FFFFFF0, 0x80000000]) if start is None else start
    out = []
    for _ in range(length):
        out.append(t & MINUS)
        t += r.choice([0, 0, 1, 3, 10, 50, 200, 1000])
    return out


def delta(r):
    return r.choice(SMALL)


# ======================================================================= test 1: setPivot

PIVOTS = [(0, 0), (5, -7), (-1, 1), (0x7FFF, -0x8000), (-0x8000, 0x7FFF), (0x8000, 0x10001), (0xFFFF, 0x12345), (100, 100)]
DRAW_SETS = [(-1, -1), (0, -1), (-1, 1), (2, 2), (3, 4), (0, 1), (1, 0)]


def pivot_draws(n, offset=0):
    """Draw objects for n windows: every combination of none, one and both in turn."""
    return [DRAW_SETS[(s + offset) % len(DRAW_SETS)] for s in range(n)]


def pivot_edges():
    chain8 = [-1, 0, 1, 2, 3, 4, 5, 6]
    wide = [-1, 0, 0, 0, 1, 1, 2, 3]
    # the fifth level with a list of siblings (slots 4 to 6 under 3), a sixth under the first of them
    deep = [-1, 0, 1, 2, 3, 3, 3, 4]
    # siblings at every level of the unrolled part, children below only the middle ones
    bushy = [-1, 0, 0, 1, 1, 2, 3, 3]
    single = [-1]
    for parents in (single, [-1, 0], [-1, 0, 1], [-1, 0, 1, 2], [-1, 0, 1, 2, 3], chain8, wide, deep, bushy):
        n = len(parents)
        for x, y in PIVOTS:
            yield case_of(world(parents, draws=pivot_draws(n)) + [(40, (0, x, y))])
        for offset in (1, 3):
            yield case_of(world(parents, draws=pivot_draws(n, offset)) + [(40, (0, 5, -7))])
        # from the other windows too: the descendants only, not the siblings or the parents
        for slot in range(n):
            yield case_of(world(parents, draws=[(0, 1)] * n) + [(40, (slot, 11 + slot, -12 - slot))])
    # no draw objects at all, then all of them
    yield case_of(world(chain8, draws=[(-1, -1)] * 8) + [(40, (0, 3, 4)), (40, (3, 5, 6))])
    yield case_of(world(chain8, draws=[(0, 1)] * 8) + [(40, (0, 3, 4)), (40, (3, 5, 6)), (40, (7, 1, 1))])
    # the pivot before and after (op 8 sets the pair, the descendants get overwritten, the others keep theirs)
    ops = world(wide, draws=pivot_draws(8)) + [(8, (s, s, 0, 40 + s, 50 + s, 0)) for s in range(8)]
    yield case_of(ops + [(40, (1, -2, -3)), (40, (0, 9, 9)), (40, (2, 0, 0))])
    # the same pair given twice
    yield case_of(world(deep, draws=pivot_draws(8)) + [(40, (0, 4, 5)), (40, (0, 4, 5))])


def pivot_random(r):
    n = r.randint(1, SLOTS)
    parents = rnd_parents(r, n)
    ops = world(parents, [rnd_rect(r) for _ in range(n)], [1] * n, [0] * n, [rnd_draws(r) for _ in range(n)])
    for _ in range(r.randint(1, 5)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.75:
            ops.append((40, (slot, delta(r), delta(r))))
        elif k < 0.9:
            ops.append((8, (slot, delta(r), 0, delta(r), delta(r), 0)))
        else:
            ops.append((4, (slot,) + rnd_draws(r)))
    return case_of(ops)


TESTS.append(FnTest("InterfaceWindow::setPivot", ui_ppc, ui_host, edge=pivot_edges, rnd=pivot_random))


# ======================================================================= test 2: startAnim

TABLE_ONE = [(1, 2, 3, 4), (ANIM_END, 0, 0, 0)]
TABLE_TWO = [(5, 6, 7, 8), (-9, -10, -11, -12), (ANIM_END, 0, 0, 0)]
FRAME_SETS = [TABLE_ONE, TABLE_TWO]   # (startAnim stores the table's address and never reads it; the frames only tell the tables apart)


def rnd_table(r):
    return [tuple(delta(r) for _ in range(4)) for _ in range(r.randint(1, 5))] + [(ANIM_END, 0, 0, 0)]
OFFSETS = [(0, 0, 0, 0), (1, 2, 3, 4), (-5, 6, -7, 8), (0x7FFF, -0x8000, 0x7FFF, -0x8000), (-0x8000, 0x7FFF, -0x8000, 0x7FFF), (0, 0, 0, 1),
           (0, 0, 1, 0), (0, 1, 0, 0), (1, 0, 0, 0), (0x8000, 0x10001, -0x8001, 0xFFFF)]


def anim_state(slot, table, frame, ticks, start, animating, offsets):
    return (5, (slot, table, frame, ticks, start, animating) + tuple(offsets))


def start_edges():
    parents = [-1, 0, 0, 1]
    anchors = [1, 0x10001, 0x30001, 0xF0001]
    draws = [(0, 1), (2, -1), (-1, 3), (4, 4)]
    base = world(parents, attributes=anchors, regions=[0, 1, 2, 0], draws=draws)
    ticks = {"IveGotTicks": [1000, 2000, 3000, 4000, 5000]}
    # not animating: with and without frames; the old state of the animation fields is overwritten
    for animating in (0, 1, 2, 0x80, 0xFF, 0x101):
        for table in (-1, 0, 1):
            for offsets in OFFSETS:
                yield case_of(base + [anim_state(1, 2, 9, 77, 88, animating, offsets), (41, (1, table, 35))], q=ticks, tables=FRAME_SETS + [TABLE_ONE])
    # each offset alone (the rectangle moves by minus that offset at that edge)
    for k in range(4):
        for value in (1, -1, 3, -3, 0x7FFF, -0x8000):
            offsets = [0, 0, 0, 0]
            offsets[k] = value
            for slot in (0, 1, 2, 3):
                yield case_of(base + [anim_state(slot, 0, 0, 5, 5, 1, offsets), (41, (slot, 0, 8))], q=ticks, tables=FRAME_SETS)
    # the argument's extremes and the clock's
    for value in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x23, 0xFFFF):
        for now in (0, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFF00):
            yield case_of(base + [(41, (1, 0, T.s32(value)))], q={"IveGotTicks": [now, now + 1]}, tables=FRAME_SETS)
    # twice in a row (the second finds the first running with no offsets), a NULL table between, a table swapped
    yield case_of(base + [(41, (1, 0, 10)), (41, (1, 1, 20)), (41, (1, -1, 30)), (41, (1, 0, 40)), (41, (3, 1, 50))], q=ticks, tables=FRAME_SETS)
    yield case_of(base + [anim_state(1, 0, 0, 5, 5, 1, (4, 3, 2, 1)), (41, (1, -1, 30)), (41, (1, -1, 30)), (41, (1, 0, 5))], q=ticks, tables=FRAME_SETS)
    # a window that has been shifted by an earlier animation: a deeper tree, children that follow the edges
    deep = world([-1, 0, 1, 2, 3], attributes=[1, 0xF0001, 0x30001, 0xC0001, 0x10001], regions=[0, 1, 2, 0, 1], draws=[(0, 1)] * 5)
    for offsets in OFFSETS:
        yield case_of(deep + [anim_state(0, 0, 0, 5, 5, 1, offsets), (41, (0, 1, 6))], q=ticks, tables=FRAME_SETS)
        yield case_of(deep + [anim_state(2, 0, 0, 5, 5, 1, offsets), (41, (2, 1, 6))], q=ticks, tables=FRAME_SETS)
    # a window with no draw objects, no children, a degenerate and a huge rectangle
    for rect in ((0, 0, 0, 0), (5, 5, 5, 5), (0x7FFF, 0x7FFF, -0x8000, -0x8000), (-0x8000, -0x8000, 0x7FFF, 0x7FFF), (10, 20, 5, 10)):
        yield case_of(world([-1], rects=[rect]) + [anim_state(0, 0, 0, 5, 5, 1, (1, 2, 3, 4)), (41, (0, 0, 9))], q=ticks, tables=FRAME_SETS)
    # the rectangle it undoes is the window's own, not the client rectangle (set to something else here)
    yield case_of(base + [(10, (1, 100, 101, 102, 103)), anim_state(1, 0, 0, 5, 5, 1, (4, 3, 2, 1)), (41, (1, 0, 8))], q=ticks, tables=FRAME_SETS)
    # the animation fields the function must not touch are left as they were (the frame set by op 5 is overwritten; the slide is not)
    yield case_of(base + [(6, (1, 0, 3, 4, -5, 6, 7, 1, 20, 21, 80, 81)), (8, (1, 4, 5, 6, 7, 8)), (41, (1, 1, 12))], q=ticks, tables=FRAME_SETS)


def start_random(r):
    n = r.randint(1, SLOTS)
    ops, _ = rnd_world(r, anchor_attribute, n=n)
    tables = [rnd_table(r) for _ in range(r.randint(1, 3))]
    ticks = rnd_ticks(r)
    for _ in range(r.randint(1, 8)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.35:
            ops.append(anim_state(slot, r.choice([-1] + list(range(len(tables)))), r.choice(SMALL), r.choice(SMALL), r.choice(SMALL),
                                  r.choice([0, 1, 1, 1, 2, 0x80, 0xFF]), [delta(r) for _ in range(4)]))
        else:
            ops.append((41, (slot, r.choice([-1] + list(range(len(tables))) * 2), r.choice(SMALL + [0x7FFFFFFF, -0x80000000]))))
    return case_of(ops, q={"IveGotTicks": ticks}, tables=tables)


TESTS.append(FnTest("InterfaceWindow::startAnim", ui_ppc, ui_host, edge=start_edges, rnd=start_random))


# ======================================================================= test 3: startAnimTimer

WHICH = [0, 1, 2, 3, 0x80080, 0x80081, 0x80082, 0x80083]


def timer_edges():
    base = world([-1, 0])
    ticks = {"IveGotTicks": [100, 200, 300, 400, 500, 600]}
    # every number against every arrangement of the four slots (the two timers, none), with the timers running or stopped
    arrangements = [(0, 1, -1, 0), (-1, -1, -1, -1), (1, 1, 0, 0), (0, -1, 1, -1), (-1, 0, -1, 1), (0, 0, 0, 0), (1, -1, -1, 1)]
    for slots in arrangements:
        for running in ((1, 1), (0, 0), (0, 5), (-1, 0)):
            for which in WHICH:
                yield case_of(base + [(9, (1,) + slots), (7, (6, 0, running[0])), (7, (6, 1, running[1])), (42, (1, which))], q=ticks)
    # a window whose slots are set on another window (the starter reads its own)
    yield case_of(base + [(9, (0, 0, 0, 0, 0)), (9, (1, -1, -1, -1, 1)), (42, (0, 0)), (42, (1, 3)), (42, (1, 0x80083))], q=ticks)
    # started twice, with the clock moving
    yield case_of(base + [(9, (1, 0, 1, -1, -1)), (42, (1, 0)), (42, (1, 0x80080)), (42, (1, 1)), (42, (1, 2)), (42, (1, 0x80082))], q=ticks)


def timer_random(r):
    n = r.randint(1, SLOTS)
    ops, _ = rnd_world(r, anchor_attribute, n=n)
    for _ in range(r.randint(1, 12)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.4:
            ops.append((9, (slot,) + tuple(r.choice([-1, 0, 1]) for _ in range(4))))
        elif k < 0.5:
            ops.append((7, (6, r.randrange(2), r.choice([0, 1, 5, -1]))))
        else:
            ops.append((42, (slot, r.choice(WHICH + [r.randint(-5, 9), r.getrandbits(20)]))))
    return case_of(ops, q={"IveGotTicks": rnd_ticks(r)})


TESTS.append(FnTest("InterfaceWindow::startAnimTimer", ui_ppc, ui_host, edge=timer_edges, rnd=timer_random))


# ======================================================================= test 4: ~InterfaceCheckButtons

IN_PLACE = [0, -1, 0x8000, 0x10000, 0xFFFF, 0x7FFF0000, -0x8000, -0x7FFF]   # flags whose low half is not positive
DELETING = [1, 2, 0x7FFF, 0x10001, 0x17FFF, 0xFFFF0001]                       # flags whose low half is positive


def buttons_edges():
    tree = world([-1, 0, 0, 1], kinds=[0, 1, 1, 1], draws=[(0, 1), (2, -1), (-1, 3), (4, 4)])
    # the deleting form on the alone one: the base destructor runs, then the block is freed
    for flag in DELETING:
        yield case_of(spare_ops() + [(43, (SPARE, 0, flag))])
    yield case_of(spare_ops() + [(43, (SPARE, 0, 1))] + spare_ops() + [(43, (SPARE, 0, 1))])
    # the plain destructor leaves the object in place: in a tree, alone, twice, with every flag that is not positive as a short
    for flag in IN_PLACE:
        for slot in (1, 2, 3):
            yield case_of(tree + [(43, (slot, 1, flag))])
        yield case_of(spare_ops() + [(43, (SPARE, 1, flag))])
    yield case_of(tree + [(43, (1, 1, 0)), (43, (1, 1, 0)), (43, (3, 1, -1)), (43, (2, 1, 0))])
    # a plain destructor and then the deleting one, other objects around them
    yield case_of(tree + spare_ops() + [(43, (SPARE, 1, 0)), (43, (SPARE, 0, 1)), (43, (1, 1, 0))])
    # NULL: nothing happens whatever the flag
    for flag in DELETING + IN_PLACE:
        yield case_of(tree + [(43, (-1, 2, flag))])
    # objects with fields set before they go: the destructor leaves them alone
    yield case_of(tree + [(5, (1, 0, 3, 4, 5, 1, 1, 2, 3, 4)), (9, (1, 0, 1, 1, 0)), (8, (1, 5, 6, 7, 8, 9)), (43, (1, 1, 0))], tables=[TABLE_ONE])


def buttons_random(r):
    n = r.randint(1, SLOTS - 1)
    parents = rnd_parents(r, n)
    ops = world(parents, [rnd_rect(r) for _ in range(n)], [1] * n, [0] * n, [rnd_draws(r) for _ in range(n)],
                kinds=[r.choice([0, 1, 1]) for _ in range(n)])
    for _ in range(r.randint(1, 8)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.5:
            ops.append((43, (slot, 1, r.choice(IN_PLACE))))
        elif k < 0.7:
            ops += spare_ops(kind=r.choice([0, 1, 1]))
            ops.append((43, (SPARE, r.choice([0, 1]), r.choice(DELETING) if r.random() < 0.6 else r.choice(IN_PLACE))))
        elif k < 0.8:
            ops.append((43, (-1, 2, r.choice(DELETING + IN_PLACE))))
        else:
            ops.append((40, (slot, delta(r), delta(r))))
    return case_of(ops)


TESTS.append(FnTest("InterfaceCheckButtons::~InterfaceCheckButtons", ui_ppc, ui_host, edge=buttons_edges, rnd=buttons_random))
