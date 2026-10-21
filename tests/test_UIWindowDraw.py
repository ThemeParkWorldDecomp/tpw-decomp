"""Differential tests for the window drawing and the default window handler, against libtpw_unit_UIWindowDraw.dylib
(tests/unit_UIWindowDraw.cpp):

  src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp   UI_DefaultWindowCallback, InterfaceWindow::DrawMe, DrawReverseOrder, ClientDraw,
                                 ShiftPosition, HandleScale, SetPosition, GetPosition, BringToTop, doAnimFrame, setZRot,
                                 SetValue, InterfaceTimer::Stop

Real on both sides: those, the window's constructor (src/game/ui/InterfaceWindow.cpp, which builds the test trees) and the
rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). Fakes that record the same event on both sides: the message senders (UI_SendMessage and
UI_PostMessage), UI_UpdatePointerIn, UI_SetCursor, the UI clock (IveGotTicks, answers from a queue), the window's untranslated
virtual members (DrawFunction, CalcClientArea, the destructor, GetHelpText) and Show, the draw objects (Draw and Change record
the fields their flags name), the help text handler, two regions (Shift) and the timers (plain storage).

A case is a script of operations on a world of eight windows (the last is a spare that message 4 may delete), five draw
objects, two regions, two timers, three animation tables and a help text object. After each operation the answer, every
window, the globals the handler follows (focus window, the three buttons, the double click time), the timers, the heap
traffic and the length of the call log are compared, and the whole call log at the end. The PowerPC side decides which
operations the state allows (one that would make the original read a missing timer, walk a cycle or follow a window that
was not made is skipped on both sides).

Quirks the tests pin: BringToTop answers the window that WAS last (and tells it 0x19), message 0xc answers the ADDRESS of a
static word (the host's answer is compared as "the address of the static"), the posted position of a button message keeps
y sign-extended into the word, ShiftPosition and SetPosition truncate to 16 bits.
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIWindowDraw"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(Handler=0x101DE3F8, DrawMe=0x101DA564, DrawReverse=0x101DA794, ClientDraw=0x1018A2A8, Shift=0x101DA9D0,
          HandleScale=0x101DAC2C, SetPosition=0x101DAE98, GetPosition=0x101DB884, BringToTop=0x101DC4E0,
          AnimFrame=0x101DD304, SetZRot=0x101DDB04, SetValue=0x101DFE84, TimerStop=0x101D8428, WindowCtor=0x101D98D8,
          MakeAbs=0x101DAB88)
# what they call that is real too: the rectangles and the regions' base
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

# ---- labels (the host's tpw_uiwd_labels)
LAB_WINDOW, LAB_DRAW, LAB_REGION, LAB_TIMER, LAB_HELP, LAB_TABLE = 10, 20, 30, 40, 50, 60
SLOTS, DRAWS, REGIONS, TIMERS, TABLES = 8, 5, 2, 2, 3
# the messages the handler answers without touching the window (a NULL window is passed to the fakes as it is)
NULL_WINDOW_MESSAGES = (0, 4, 5, 0xA, 0xB, 0xD, 0xE, 0xF, 0x11, 0x12, 0x13, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1F, 0x10002, 0x10008, -1)
SPARE = SLOTS - 1                  # the window that is never part of a tree: message 4 deletes it
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
        self.windows = [e.alloc(SIZE_WINDOW, b"\xa5" * SIZE_WINDOW) for _ in range(SLOTS)]
        self.made = [False] * SLOTS
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
        out.append(L.lab(e.u32(G["help"])))
        return out

    def dump_state(self):
        return dict(windows=[self.dump_window(self.windows[s]) if self.made[s] else [0] for s in range(SLOTS)],
                    globals=self.dump_globals(), heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        if code in (2, 3, 4, 5, 6, 8) or 20 <= code <= 30 or (code == 32 and not (a[0] == -1 and a[1] in NULL_WINDOW_MESSAGES)):
            if not self.window(a[0]):
                return None
        if code == 1:
            slot, parent = a[0], a[1]
            if self.made[slot] or (parent >= 0 and not self.made[parent]) or parent >= slot:
                return None
            e = self.e
            e.write(self.windows[slot], b"\xa5" * SIZE_WINDOW)
            self.L.labels[self.windows[slot]] = LAB_WINDOW + slot
            self.call(FN["WindowCtor"], self.windows[slot], self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]),
                      s16(a[7]))
            self.made[slot] = True
            return [self.L.lab(self.windows[slot]), 0, 0, 0, 0]
        if code in (2, 3, 4, 5, 6, 8):
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
        """False for an operation the state does not allow: the original would read a missing timer or window, follow a cycle
        or walk off the animation table."""
        e = self.e
        if code == 31:
            return 0 <= a[0] < TIMERS
        if code in (20, 21, 23, 24, 25, 26, 27, 29) and not self.acyclic():
            return False
        w = self.windows[a[0]] if 0 <= a[0] < SLOTS else 0
        if code == 28:
            return self.anim_ok(a[0])
        if code == 27 or (code == 32 and a[1] == 9):
            nxt, prev = e.u32(w + 0x50), e.u32(w + 0x54)
            if nxt and not prev and not e.u32(nxt + 0x5C):
                return False   # the first of several roots: its next sibling has no parent to take it as first child
        if code == 32:
            message = a[1]
            if message == 4:
                return a[0] == -1 or (a[0] == SPARE and not any(e.u32(w + o) for o in (0x50, 0x54, 0x58, 0x5C)))
            if message in (0x10004, 0x10005, 0x10006, 0x10007) and not 0 <= a[2] < 3:
                return False
            if message == 0x10 and a[2] == 0x80085:
                if e.read(w + 0x136, 1)[0] != 0 and struct.unpack(">h", e.read(w + 0x108, 2))[0] == 0 and e.u32(w + 0xFC) == 0:
                    return False
            if (message == 0x10 and a[2] == 0x80084) or message == 0x1E:
                return self.anim_ok(a[0])
        return True

    def anim_ok(self, slot):
        """False when the animation would read a frame outside its table (only checked while it can run)."""
        if self.u32(slot, 0xE4) == 0 or self.e.read(self.windows[slot] + 0x135, 1)[0] != 1:
            return True
        return 0 <= self.s16at(slot, 0xE8) < TABLE_FRAMES

    def function(self, code, a):
        if not self.allowed(code, a):
            return None
        e, L = self.e, self.L
        w = self.windows[a[0]] if 0 <= a[0] < SLOTS else 0
        answer = [0, 0, 0, 0, 0]
        if code == 20:
            answer[0] = T.s32(self.call(FN["DrawMe"], w))
        elif code == 21:
            answer[0] = T.s32(self.call(FN["DrawReverse"], w))
        elif code == 22:
            self.call(FN["ClientDraw"], w)
        elif code == 23:
            answer[0] = T.s32(self.call(FN["Shift"], w, s16(a[1]), s16(a[2])))
        elif code == 24:
            self.call(FN["HandleScale"], w, s16(a[1]), s16(a[2]), s16(a[3]), s16(a[4]))
        elif code == 25:
            rect = e.alloc(16, struct.pack(">I4h", CORE_VTABLE, s16(a[1]), s16(a[2]), s16(a[3]), s16(a[4])))
            answer[0] = T.s32(self.call(FN["SetPosition"], w, 0 if a[5] else rect))
            answer[1:] = struct.unpack(">4h", e.read(rect + 4, 8))
        elif code == 26:
            rect = e.alloc(16, struct.pack(">I4h", CORE_VTABLE, 7, 8, 9, 10))
            answer[0] = T.s32(self.call(FN["GetPosition"], w, 0 if a[2] else rect, s16(a[1])))
            answer[1:] = struct.unpack(">4h", e.read(rect + 4, 8))
        elif code == 27:
            answer[0] = L.lab(self.call(FN["BringToTop"], w))
        elif code == 28:
            answer[0] = T.s32(self.call(FN["AnimFrame"], w))
        elif code == 29:
            self.call(FN["SetZRot"], w, s16(a[1]))
        elif code == 30:
            answer[0] = T.s32(self.call(FN["SetValue"], w, a[1]))
        elif code == 31:
            self.call(FN["TimerStop"], self.timers[a[0]])
        elif code == 32:
            result = self.call(FN["Handler"], w, a[1], a[2], a[3])
            answer[0] = -0x7001 if a[1] == 0xC and result == RESULT_ADDRESS else T.s32(result)
            if a[1] == 4 and a[0] >= 0:
                self.made[a[0]] = False
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


WINDOW_FIELDS = 68
GLOBAL_FIELDS = 20


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


def world(parents, rects=None, attributes=None, regions=None, draws=None, cursors=None):
    """The windows 0.. made with these parents (-1 for the root, which must be slot 0), linked, with their fields and draw
    objects set. Window 0 is the only root, so every list of siblings has a parent."""
    n = len(parents)
    rects = rects or default_rects(n)
    attributes = attributes or [1] * n
    regions = regions or [0] * n
    draws = draws or [(-1, -1)] * n
    cursors = cursors or [-1] * n
    ops = [(1, (s, parents[s], attributes[s], 100 + s) + tuple(rects[s])) for s in range(n)]
    ops += link_ops(parents)
    for s in range(n):
        ops.append((3, (s, attributes[s], 0, regions[s], cursors[s])))
        ops.append((4, (s,) + tuple(draws[s])))
    return ops


def spare_ops(rect=(5, 6, 55, 66), attributes=1):
    """The spare window: alone, made, the only one message 4 may delete."""
    return [(1, (SPARE, -1, attributes, 77) + rect), (3, (SPARE, attributes, 0, 0, -1)), (4, (SPARE, -1, -1))]


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


# ======================================================================= test 1: DrawMe, DrawReverseOrder, ClientDraw


def draw_edges():
    chain = [-1, 0, 1, 2, 3, 4, 5]
    both = [(0, 1)] * 7
    # a single window: visible, hidden, the kinds, with no, one and both draw objects
    for attribute in (1, 0, 2, 8, 3, 0x10000001, 0xF0000001, 0x80000001, 0x40000000, 0xF0000000, 0x11, 0x401):
        for draws in ((-1, -1), (0, -1), (-1, 1), (2, 2), (3, 4)):
            yield case_of(world([-1], attributes=[attribute], draws=[draws]) + [(20, (0,)), (21, (0,)), (22, (0,))])
    # chains: everything visible, one window hidden, one a kind, from the root and from the middle
    yield case_of(world(chain, draws=both) + [(20, (0,)), (21, (0,)), (20, (3,)), (21, (3,)), (20, (6,)), (21, (6,))])
    for k in range(7):
        for value in (0, 2, 0x10000001, 0xF0000001):
            attributes = [1] * 7
            attributes[k] = value
            yield case_of(world(chain, attributes=attributes, draws=both) + [(20, (0,)), (21, (0,)), (20, (k,)), (21, (k,))])
    # a wide tree: three children with two children each, the middle one hidden, then a kind
    wide = [-1, 0, 0, 0, 1, 1, 2]
    for attributes in ([1] * 7, [1, 1, 0, 1, 1, 1, 1], [1, 0x10000001, 1, 1, 1, 1, 1], [1, 1, 1, 0xF0000001, 1, 0, 1]):
        yield case_of(world(wide, attributes=attributes, draws=[(0, 1)] * 7) + [(20, (0,)), (21, (0,)), (20, (1,)), (21, (2,))])
    # a deep chain with siblings at every level
    deep = [-1, 0, 0, 1, 1, 3, 3]
    yield case_of(world(deep) + [(20, (0,)), (21, (0,)), (20, (3,)), (21, (3,))])
    # the fourth level with three children (the last unrolled level of the Mac's walk has a list of siblings to go through)
    for attributes in ([1] * 7, [1, 1, 1, 1, 1, 1, 0], [1, 1, 1, 1, 1, 0x10000001, 1], [1, 1, 1, 1, 0, 1, 1]):
        yield case_of(world([-1, 0, 1, 2, 3, 3, 3], attributes=attributes, draws=both) + [(20, (0,)), (21, (0,)), (21, (3,)), (20, (3,))])
    # the client rectangle's z + 1 at the edges
    for z in (0, 1, -1, 0x7FFF, -0x8000, 0x100, 0xFFFF):
        yield case_of(world([-1], draws=[(-1, 2)]) + [(8, (0, z, 0, 0, 0, 0)), (22, (0,))])
    # attributes changed between draws
    yield case_of(world(wide, draws=[(0, 1)] * 7) + [(20, (0,)), (3, (1, 0, 0, 0, -1)), (20, (0,)), (21, (0,)),
                                                      (3, (1, 0x10000001, 0, 0, -1)), (20, (0,)), (21, (0,))])


def draw_random(r):
    ops, n = rnd_world(r, draw_attribute)
    for _ in range(r.randint(1, 12)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.35:
            ops.append((20, (slot,)))
        elif k < 0.7:
            ops.append((21, (slot,)))
        elif k < 0.8:
            ops.append((22, (slot,)))
        elif k < 0.92:
            ops.append((3, (slot, draw_attribute(r), 0, 0, -1)))
        else:
            ops.append((8, (slot, delta(r), 0, 0, 0, 0)))
    return case_of(ops)


TESTS.append(FnTest("DrawMe, DrawReverseOrder, ClientDraw", ui_ppc, ui_host, edge=draw_edges, rnd=draw_random))


# ======================================================================= test 2: ShiftPosition, GetPosition


def shift_edges():
    wide = [-1, 0, 0, 1, 1, 2, 3]
    base = world(wide, regions=[0, 1, 2, 0, 1, 0, 2], draws=[(0, 1), (1, -1), (-1, 2), (3, 4), (0, 0), (-1, -1), (2, 3)])
    for dx, dy in ((0, 0), (5, 0), (0, -7), (3, 4), (-3, -4), (0x7FFF, 1), (1, 0x7FFF), (0x8000, 0), (0, 0x8000), (-0x8000, -0x8000),
                   (0xFFFF, 0xFFFF), (0x10001, 0x20002)):
        for slot in (0, 1, 3, 6):
            yield case_of(base + [(23, (slot, dx, dy))])
    yield case_of(base + [(8, (0, 1, 0, 0x7FFF, -0x8000, 0)), (23, (0, 1, 1)), (8, (1, 1, 0, -0x8000, 0x7FFF, 0)), (23, (1, -1, -1))])
    # GetPosition: with and without the animation's offsets taken off, NULL
    for offsets in ((0, 0, 0, 0), (1, 2, 3, 4), (-5, 0x7FFF, -0x8000, 9), (0x8000, 0x8000, 0, 1)):
        ops = world([-1, 0]) + [(5, (0, -1, 0, 0, 0, 0) + offsets)]
        for subtract in (0, 1, -1, 5, 0x10000):
            for null in (0, 1):
                ops.append((26, (0, subtract, null)))
        yield case_of(ops)


def shift_random(r):
    ops, n = rnd_world(r, anchor_attribute)
    for _ in range(r.randint(1, 10)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.45:
            ops.append((23, (slot, delta(r), delta(r))))
        elif k < 0.75:
            ops.append((26, (slot, r.choice([0, 1, 1, -1, 7]), int(r.random() < 0.1))))
        elif k < 0.85:
            ops.append((5, (slot, -1, 0, 0, 0, 0) + tuple(delta(r) for _ in range(4))))
        else:
            ops.append((8, (slot, 1, 0, delta(r), delta(r), 0)))
    return case_of(ops)


TESTS.append(FnTest("ShiftPosition, GetPosition", ui_ppc, ui_host, edge=shift_edges, rnd=shift_random))


# ======================================================================= test 3: SetPosition, HandleScale


def scale_edges():
    parents = [-1, 0, 0, 0, 0]
    rects = [(0, 0, 100, 100), (10, 10, 50, 50), (60, 10, 90, 50), (10, 60, 50, 90), (60, 60, 90, 90)]
    # one child per anchor combination, scaled by every combination of zero and nonzero amounts
    for anchor in ANCHORS:
        attributes = [1, 1 | anchor, 1 | anchor, 1 | anchor, 1 | anchor]
        base = world(parents, rects=rects, attributes=attributes, draws=[(0, 1)] * 5)
        yield case_of(base + [(24, (0, 4, 5, -6, 7)), (24, (0, 0, 0, 0, 0)), (24, (0, 3, 0, 0, 0)), (24, (0, 0, 3, 0, 0)),
                              (24, (0, 0, 0, 3, 0)), (24, (0, 0, 0, 0, 3)), (24, (0, 3, 0, -3, 0)), (24, (0, 0, 3, 0, -3)),
                              (24, (0, 3, 3, 0, 0)), (24, (0, 0, 0, 3, 3))])
    # SetPosition: NULL, the same rectangle, each edge different alone, same size shifted, resized
    base = world(parents, rects=rects, attributes=[1, 0x10001, 0x20001, 0x30001, 0xF0001], draws=[(0, 1)] * 5)
    yield case_of(base + [(25, (0, 0, 0, 100, 100, 1)), (25, (0, 0, 0, 100, 100, 0)), (25, (0, 1, 0, 100, 100, 0)),
                          (25, (0, 1, 1, 100, 100, 0)), (25, (0, 1, 1, 101, 100, 0))])
    for rect in ((1, 0, 100, 100), (0, 1, 100, 100), (0, 0, 101, 100), (0, 0, 100, 101), (5, 6, 105, 106), (5, 6, 5, 6),
                 (-5, -6, 95, 94), (0, 0, 0x7FFF, 0x7FFF), (-0x8000, -0x8000, 0, 0), (0x7FFF, 0x7FFF, -0x8000, -0x8000), (0, 0, 200, 50)):
        yield case_of(base + [(25, (0,) + rect + (0,))])
        yield case_of(base + [(25, (2,) + rect + (0,))])
    yield case_of(base + [(25, (0, 0, 0, 100, 100, 0)), (25, (0, 0, 0, 100, 100, 0))])
    # a deeper tree: the resize reaches the grandchildren through the child's SetPosition
    deep = world([-1, 0, 1, 2, 3], rects=[(0, 0, 100, 100), (5, 5, 95, 95), (10, 10, 90, 90), (15, 15, 85, 85), (20, 20, 80, 80)],
                 attributes=[1, 0xF0001, 0x30001, 0xC0001, 0x10001], draws=[(0, 1)] * 5)
    yield case_of(deep + [(25, (0, 0, 0, 120, 140, 0)), (25, (0, 0, 0, 90, 90, 0)), (23, (0, 5, 5))])
    # an animated child is placed from where it rests
    anim = world(parents, rects=rects, attributes=[1, 0x30001, 0x30001, 0, 1])
    yield case_of(anim + [(5, (1, -1, 0, 0, 0, 0, 3, 4, 5, 6)), (5, (2, -1, 0, 0, 0, 0, -3, 0x7FFF, 0, 1)), (25, (0, 0, 0, 120, 130, 0))])


def scale_random(r):
    ops, n = rnd_world(r, anchor_attribute)
    for _ in range(r.randint(1, 10)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.4:
            ops.append((24, (slot, delta(r), delta(r), delta(r), delta(r))))
        elif k < 0.85:
            rect = rnd_rect(r) if r.random() < 0.5 else (slot * 10, slot * 8, slot * 10 + 60, slot * 8 + 50)
            if r.random() < 0.3:
                rect = (rect[0], rect[1], rect[0] + 60, rect[1] + 50)
            ops.append((25, (slot,) + rect + (int(r.random() < 0.08),)))
        elif k < 0.93:
            ops.append((5, (slot, -1, 0, 0, 0, 0) + tuple(delta(r) for _ in range(4))))
        else:
            ops.append((23, (slot, delta(r), delta(r))))
    return case_of(ops)


TESTS.append(FnTest("SetPosition, HandleScale", ui_ppc, ui_host, edge=scale_edges, rnd=scale_random))


# ======================================================================= test 4: BringToTop


def top_edges():
    parents = [-1, 0, 0, 0, 2, 2]
    base = world(parents)
    # every window, then repeated
    for slot in range(6):
        yield case_of(base + [(27, (slot,))])
        yield case_of(base + [(27, (slot,)), (27, (slot,)), (27, (0 if slot == 0 else 1,))])
    yield case_of(base + [(27, (1,)), (27, (1,)), (27, (2,)), (27, (3,)), (27, (2,)), (27, (4,)), (27, (5,)), (27, (4,))])
    yield case_of(world([-1, 0]) + [(27, (1,)), (27, (0,))])
    # through the message, with and without the redraw switch and a non zero first argument
    for redraw in (0, 1, 7):
        for a in (0, 1, -1):
            yield case_of(base + [(7, (3, redraw)), (32, (1, 9, a, 0)), (32, (1, 9, a, 0)), (32, (3, 9, a, 0)), (32, (0, 9, a, 0))])


def top_random(r):
    ops, n = rnd_world(r, any_attribute)
    ops.append((7, (3, r.choice([0, 1, 5]))))
    for _ in range(r.randint(1, 12)):
        slot = r.randrange(n)
        if r.random() < 0.6:
            ops.append((27, (slot,)))
        else:
            ops.append((32, (slot, 9, r.choice([0, 1, 2, -1]), r.choice(SMALL))))
    return case_of(ops)


TESTS.append(FnTest("BringToTop", ui_ppc, ui_host, edge=top_edges, rnd=top_random))


# ======================================================================= test 5: doAnimFrame, setZRot, SetValue, InterfaceTimer::Stop


def anim_frame_kinds(r, index, last, count):
    """One random frame of an animation table at `index` (the table has `count` frames): amounts, or a command."""
    k = r.random()
    if index == last and r.random() < 0.7:
        return (ANIM_END, r.choice([0, 3, -1]), 0, 0)
    if k < 0.12:
        return (ANIM_ZROT, r.choice(SMALL), r.choice([0, 5]), r.choice([0, -5]))
    if k < 0.24:
        return (ANIM_FRAME, r.choice(SMALL), r.choice([0, 5]), 0)
    if k < 0.34 and index + 1 < count:
        return (ANIM_GOTO, r.randint(index + 1, count - 1), 0, 0)
    if k < 0.38:
        return (r.choice([ANIM_END, ANIM_ZROT, ANIM_GOTO, ANIM_FRAME]) + r.choice([-1, 1, 0]), 1, 2, 3)   # next to a command
    return tuple(delta(r) for _ in range(4))


def rnd_table(r):
    count = r.randint(1, 10)
    return [anim_frame_kinds(r, i, count - 1, count) for i in range(count)]


TABLE_ZERO = [(5, 6, 7, 8), (ANIM_ZROT, 30, 0, 0), (ANIM_FRAME, 4, 0, 0), (-1, -2, -3, -4), (ANIM_GOTO, 5, 0, 0), (9, 9, 9, 9), (ANIM_END, 0, 0, 0)]
TABLE_END = [(ANIM_END, 0, 0, 0)]
TABLE_LOOP = [(ANIM_GOTO, 1, 0, 0), (ANIM_GOTO, 2, 0, 0), (1, 1, 1, 1), (ANIM_GOTO, 0, 0, 0)]
TABLE_WRAP = [(0x7FFF, -0x8000, 0x7FFF, -0x8000), (1, 1, 1, 1), (ANIM_ZROT, -0x8000, 0, 0), (ANIM_END, 0, 0, 0)]
STEADY = list(range(10, 700, 100))   # a clock that always moves on by more than the usual animation interval


def anim_edges():
    parents = [-1, 0, 0, 1]
    draws = [(0, 1), (2, -1), (-1, 3), (4, 4)]
    base = world(parents, draws=draws)
    tables = [TABLE_ZERO, TABLE_END, TABLE_LOOP]
    # the table walked to its end, with the offsets building up and the end command putting the window back
    yield case_of(base + [(5, (0, 0, 0, 5, 0, 1, 0, 0, 0, 0))] + [(28, (0,))] * 10, q={"IveGotTicks": STEADY}, tables=tables)
    yield case_of(base + [(5, (1, 1, 0, 5, 0, 1, 7, 8, 9, 10))] + [(28, (1,))] * 3, q={"IveGotTicks": STEADY}, tables=tables)
    # a loop of gotos: the clock stops moving at the end of the queue, which ends the recursion
    yield case_of(base + [(5, (0, 2, 0, 1, 0, 1, 0, 0, 0, 0)), (28, (0,)), (28, (0,))], q={"IveGotTicks": [100, 100, 200, 200, 300, 300, 300]},
                  tables=tables)
    yield case_of(base + [(5, (2, 2, 3, 0, 0, 1, 0, 0, 0, 0)), (28, (2,))], q={"IveGotTicks": [10, 20, 30, 40, 50, 50, 50]}, tables=tables)
    # not running: not animating (0, 2, 0xff), no table, the interval not yet over (equal, one more), the clock wrapped
    for animating in (0, 2, 0xFF):
        yield case_of(base + [(5, (0, 0, 0, 5, 0, animating, 0, 0, 0, 0)), (28, (0,))], q={"IveGotTicks": STEADY}, tables=tables)
    yield case_of(base + [(5, (0, -1, 0, 5, 0, 1, 0, 0, 0, 0)), (28, (0,))], q={"IveGotTicks": STEADY}, tables=tables)
    for now, start, ticks in ((10, 5, 5), (11, 5, 5), (10, 5, 4), (5, 5, 0), (6, 5, 0), (3, 0xFFFFFFF0, 10), (3, 0xFFFFFFF0, 30),
                              (0x80000005, 5, 5), (0x7FFFFFFF, 0x80000000, 0), (100, 50, -1), (100, 50, 0x7FFFFFFF)):
        yield case_of(base + [(5, (0, 0, 0, T.s32(ticks), T.s32(start), 1, 0, 0, 0, 0)), (28, (0,)), (28, (0,))],
                      q={"IveGotTicks": [now, now, now + 1000, now + 1000]}, tables=tables)
    # a table frame that is a large amount, or the frame number negative or past the table
    yield case_of(base + [(5, (0, 2, 0, 5, 0, 1, 0, 0, 0, 0))] + [(28, (0,))] * 6, q={"IveGotTicks": STEADY},
                  tables=[TABLE_ZERO, TABLE_END, TABLE_WRAP])
    yield case_of(base + [(5, (0, 0, 60, 5, 0, 1, 0, 0, 0, 0))] + [(28, (0,))] * 5, q={"IveGotTicks": STEADY}, tables=tables)
    # setZRot on the root, a middle window and a leaf; with draw objects and without
    for rotation in (0, 5, -1, 0x7FFF, -0x8000, 0x10003, 0xFFFF):
        for slot in (0, 1, 3):
            yield case_of(base + [(29, (slot, rotation))])
    # SetValue
    for value in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x12345678):
        for slot in range(4):
            yield case_of(base + [(30, (slot, value))])
    # InterfaceTimer::Stop: running, stopped, any word
    for running in (1, 0, 5, -1):
        yield case_of(base + [(7, (6, 0, running)), (7, (6, 1, 1)), (31, (0,)), (31, (1,)), (31, (0,))])


def anim_setup_op(r, slot, tables, ticks, animTicks):
    return (5, (slot, r.choice([-1] + list(range(len(tables))) * 3), r.choice([0, 0, 0, 0, 1, 2, 5, 9]), animTicks,
                T.s32(ticks[0] - r.choice([0, 1, 10, 1000, 0x7FFFFFFF])), r.choice([1, 1, 1, 1, 0, 2]),
                delta(r), delta(r), delta(r), delta(r)))


def anim_random(r):
    ops, n = rnd_world(r, any_attribute)
    tables = [rnd_table(r) for _ in range(r.randint(1, 3))]
    ticks = rnd_ticks(r)
    animTicks = r.choice([0, 1, 5, 20, 100, -1])
    for slot in r.sample(range(n), r.randint(1, n)):
        ops.append(anim_setup_op(r, slot, tables, ticks, animTicks))
    for _ in range(r.randint(1, 14)):
        k = r.random()
        slot = r.randrange(n)
        if k < 0.07:
            ops.append(anim_setup_op(r, slot, tables, ticks, animTicks))
        elif k < 0.6:
            ops.append((28, (slot,)))
        elif k < 0.75:
            ops.append((29, (slot, delta(r))))
        elif k < 0.85:
            ops.append((30, (slot, r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]))))
        elif k < 0.9:
            ops.append((31, (r.randrange(TIMERS),)))
        elif k < 0.95:
            ops.append((7, (6, r.randrange(TIMERS), r.choice([0, 1, 7]))))
        else:
            ops.append((4, (slot,) + rnd_draws(r)))
    return case_of(ops, q={"IveGotTicks": ticks}, tables=tables)


TESTS.append(FnTest("doAnimFrame, setZRot, SetValue, InterfaceTimer::Stop", ui_ppc, ui_host, edge=anim_edges, rnd=anim_random))


# ======================================================================= test 6: UI_DefaultWindowCallback

MESSAGES = ([1, 2, 3, 5, 6, 7, 8, 9, 0xC, 0x1E] * 3 + [0x10] * 4 + [0x10001, 0x10002] * 2 + [0x10003] * 4 + [0x10004, 0x10005] * 6 +
            [0x10006, 0x10007] * 3 + [4, 4, 0, 0xA, 0xB, 0xD, 0xE, 0xF, 0x11, 0x12, 0x13, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1F, 0xFFFF, 0x10000,
                                       0x10008, 0x10009, 0x11004, 0x20000, -1, -2, 0x7FFFFFFF, -0x80000000, 0x80000010, 0x80000001])


def rnd_position(r):
    return pack_position(r.randint(-20, 140), r.randint(-20, 140))


def rnd_message(r, n):
    """One handler message to a random window, its arguments chosen for what it reads."""
    slot = r.randrange(n)
    message = r.choice(MESSAGES)
    a, b = r.choice(SMALL), r.choice(SMALL)
    if message == 0x10:
        a = r.choice([0x80084, 0x80085, 0x80085, 0x80083, 0x80086, 0, -1, 0x80184])
    elif message in (0x10004, 0x10005, 0x10006, 0x10007):
        a, b = r.choice([0, 1, 2, 0, 1, 2, 3, -1, 32]), rnd_position(r)
    elif message in (0x10001, 0x10002, 0x10003):
        a = rnd_position(r)
    elif message == 9:
        a = r.choice([0, 1, 5, -1])
    elif message == 8:
        a = T.s32(((r.randint(-20, 100) & 0xFFFF) << 16) | (r.randint(-20, 100) & 0xFFFF))
        b = T.s32(((r.randint(-20, 200) & 0xFFFF) << 16) | (r.randint(-20, 200) & 0xFFFF))
    elif message in (6, 7):
        a, b = r.choice(SMALL + [0x10000, 0x18000, -0x10001]), r.choice(SMALL + [0x10000])
    elif r.random() < 0.3:
        a, b = r.getrandbits(32), r.getrandbits(32)
        a, b = T.s32(a), T.s32(b)
    if message == 4:
        slot = SPARE
    return (32, (slot, message, a, b))


def state_op(r, n, tables):
    """An operation that sets up what the handler reads: the globals, the buttons, a slide, an animation, attributes."""
    k = r.random()
    slot = r.randrange(n)
    if k < 0.15:
        return (7, (1, r.choice([-1, slot, r.randrange(n)])))
    if k < 0.3:
        return (7, (5, r.randrange(3), r.choice([0, 1, 1, 2]), r.randint(-10, 130), r.randint(-10, 130),
                    r.choice([0, 100, 1000, T.s32(0xFFFFFF00), 5000])))
    if k < 0.4:
        return (8, (slot, r.choice([1, 2, 0x100]), r.choice([0, 0, 5]), r.randint(0, 100), r.randint(0, 100), r.choice([0, 1, 2, 4, 7, 3, 0x80000000])))
    if k < 0.5:
        return (6, (slot, r.choice([-1, 0, 1]), delta(r), delta(r), delta(r), delta(r), r.choice([0, 0, 1, 2, 3, -1]), r.choice([0, 1, 1, 0x80, -1])) +
                tuple(r.randint(0, 100) for _ in range(4)))
    if k < 0.58:
        return (7, (4, r.choice([-1, slot])))
    if k < 0.65:
        return (7, (2, r.choice([0, 1, 5, 500, 1000, -1])))
    if k < 0.7:
        return (7, (3, r.choice([0, 1, 9])))
    if k < 0.8:
        return (3, (slot, r.choice([1, 9, 0x401, 0x8001, 0x11, 0, 0x10000001, 0x40001]), 0, r.choice([0, 0, 1, 2]), r.choice([-1, 0, 3, 9, 0x7FFF])))
    if k < 0.9:
        return (8, (slot, r.choice([0, 1, 5, 0x7FFF, -1]), r.choice([0, 3]), r.randint(0, 100), r.randint(0, 100), r.choice([0, 1, 2, 4, 7, 3])))
    return (4, (slot,) + rnd_draws(r))


def message_edges():
    parents = [-1, 0, 0, 1]
    draws = [(0, 1), (2, -1), (-1, 3), (4, 4)]
    rects = [(0, 0, 100, 100), (10, 10, 60, 60), (50, 50, 95, 95), (12, 12, 40, 40)]
    base = world(parents, rects=rects, draws=draws) + spare_ops()
    tables = [TABLE_ZERO, TABLE_END, TABLE_LOOP]
    help_values = {"GetHelpText": [-1, 0, 5, 0x7FFFFFFF, -0x80000000]}
    # the messages that call a member of the window or post another
    for message in (1, 2, 3, 5, 0, 0xA, 0xB, 0xD, 0x11, 0x12, 0x13, 0x15, 0x1A, 0x1B, 0x1C, 0x1D, 0x1F, 0xFFFF, 0x10000, 0x10008, 0x11004,
                    -1, 0x80000001, 0x7FFFFFFF):
        for slot in (0, 1, 3):
            yield case_of(base + [(32, (slot, message, 5, -6))])
    for message in NULL_WINDOW_MESSAGES:
        yield case_of(base + [(32, (-1, message, 3, 4))])
    for a in (0, 1, -1, 0x10000, 0x10001, 0x18000):
        yield case_of(base + [(32, (1, 6, a, 0))])
    for a, b in ((0, 0), (3, 4), (-3, -4), (0, 9), (0x7FFF, 1), (0x10003, 0x20004)):
        yield case_of(base + [(32, (0, 7, a, b)), (32, (1, 7, a, b))])
    for a, b in ((0, 0), (T.s32(5 << 16 | 6), T.s32(105 << 16 | 106)), (T.s32(0xFFFB << 16 | 6), T.s32(100 << 16 | 0xFFF0)),
                 (T.s32(0 << 16 | 0), T.s32(100 << 16 | 100)), (T.s32(1 << 16 | 0), T.s32(100 << 16 | 100)), (T.s32(0x7FFF << 16 | 0x7FFF), T.s32(0x8000 << 16 | 0x8000))):
        yield case_of(base + [(32, (0, 8, a, b)), (32, (1, 8, a, b))])
    # delete the spare, post a delete
    yield case_of(base + [(32, (SPARE, 4, 0, 0))])
    yield case_of(base + [(32, (SPARE, 5, 0, 0)), (32, (SPARE, 4, 1, 1)), (1, (SPARE, -1, 1, 5, 0, 0, 10, 10)), (32, (SPARE, 4, 1, 1))])
    yield case_of(base + [(32, (1, 4, 0, 0))])   # skipped: the window has a parent
    # help text number
    yield case_of(base + [(32, (0, 0xC, 0, 0)), (32, (1, 0xC, 0, 0)), (32, (3, 0xC, 1, 1)), (32, (0, 0xC, 0, 0)), (7, (7, 3)), (32, (2, 0xC, 0, 0))],
                  q=help_values)
    # the timers' messages
    ticks = {"IveGotTicks": [10, 110, 210, 310, 410, 510, 610, 710]}
    yield case_of(base + [(5, (0, 0, 0, 5, 0, 1, 0, 0, 0, 0))] + [(32, (0, 0x10, 0x80084, 0))] * 8, q=ticks, tables=tables)
    slide = (6, (1, 0, 3, 4, -5, 6, 2, 1, 20, 21, 80, 81))
    yield case_of(base + [slide] + [(32, (1, 0x10, 0x80085, 0))] * 4, tables=tables)
    yield case_of(base + [(6, (1, 0, 3, 4, -5, 6, 0, 1, 20, 21, 80, 81)), (32, (1, 0x10, 0x80085, 0)), (32, (1, 0x10, 0x80085, 0))])
    yield case_of(base + [(6, (1, 1, 3, 4, -5, 6, 0, 0, 20, 21, 80, 81)), (32, (1, 0x10, 0x80085, 0))])
    yield case_of(base + [(6, (1, -1, 3, 4, -5, 6, 0, 0x80, 20, 21, 80, 81)), (32, (1, 0x10, 0x80085, 0))])   # skipped: no timer to stop
    yield case_of(base + [(6, (1, 0, 0x7FFF, 0, 0, 0, -1, -1, 20, 21, 80, 81)), (32, (1, 0x10, 0x80085, 0)), (32, (1, 0x10, 0x80085, 0))])
    for other in (0, 0x80083, 0x80086, -1, 0x80184):
        yield case_of(base + [slide, (32, (1, 0x10, other, 0))])
    # on frame: the help text handler tells the window, with and without the animation
    for helper in (0, 1):
        for animating in (0, 1):
            yield case_of(base + [(7, (4, 1 if helper else 3)), (5, (1, 0, 0, 5, 0, animating, 0, 0, 0, 0)), (32, (1, 0x1E, 0, 0)),
                                  (32, (1, 0x1E, 0, 0))], q=ticks, tables=tables)
    # the pointer: entered, left, moved with the buttons held and not
    for cursor in (-1, 0, 5, 0x7FFF, -0x8000):
        yield case_of(base + [(3, (1, 1, 0, 0, cursor)), (32, (1, 0x10001, pack_position(5, 6), 0)), (32, (1, 0x10002, 0, 0)),
                              (32, (2, 0x10001, 0, 0)), (32, (1, 0x1E, 0, 0))])
    moves = [pack_position(x, y) for x, y in ((10, 10), (16, 10), (17, 10), (4, 10), (3, 10), (10, 16), (10, 17), (10, 4), (10, 3), (-5, -5), (200, 200))]
    for held in (0, 1, 6):
        for state in (0, 1, 2):
            ops = [(8, (1, 1, 0, 5, 5, held)), (7, (5, 0, state, 10, 10, 0)), (7, (5, 1, 1, 10, 10, 0)), (7, (5, 2, 1, -5, -5, 0))]
            ops += [(32, (1, 0x10003, m, 0)) for m in moves]
            yield case_of(base + ops)
    # the buttons: press, move, release, press again within the double click time and after it
    gesture = [(32, (1, 0x10005, 0, pack_position(20, 20))), (32, (1, 0x10003, pack_position(21, 21), 0)), (32, (1, 0x10004, 0, pack_position(21, 21))),
               (32, (1, 0x10005, 0, pack_position(22, 22))), (32, (1, 0x10004, 0, pack_position(22, 22))),
               (32, (1, 0x10006, 0, pack_position(22, 22))), (32, (1, 0x10007, 0, pack_position(22, 22)))]
    for step in (0, 1, 100, 499, 500, 501, 100000):
        for attribute in (1, 9):
            ticks_q = {"IveGotTicks": [1000 + step * i for i in range(12)]}
            yield case_of(base + [(3, (1, attribute, 0, 0, -1)), (7, (1, 0))] + gesture * 2, q=ticks_q)
    for button in (0, 1, 2):
        for where in ((20, 20), (5, 5), (500, 500), (-3, -3), (11, 11), (59, 59), (60, 60), (10, 60), (-1, 30)):
            yield case_of(base + [(32, (1, 0x10005, button, pack_position(*where))), (32, (1, 0x10004, button, pack_position(*where))),
                                  (32, (1, 0x10006, button, pack_position(*where))), (32, (1, 0x10007, button, pack_position(*where)))],
                          q={"IveGotTicks": [10, 20, 30, 40, 50, 60, 70]})
    # a position with a negative y (and x): the word the button messages carry has y sign-extended into it
    negative = world([-1, 0], rects=[(-30, -30, 100, 100), (-20, -25, 60, 60)])
    for where in ((-5, -7), (-1, -1), (-20, -25), (10, -3), (-3, 10)):
        position = pack_position(*where)
        yield case_of(negative + [(32, (1, 0x10005, 0, position)), (32, (1, 0x10004, 0, position)), (32, (1, 0x10005, 1, position)),
                                  (32, (1, 0x10006, 1, position)), (32, (1, 0x10007, 1, position)), (7, (5, 2, 1, where[0], where[1], 0)),
                                  (32, (1, 0x10006, 2, position)), (32, (1, 0x10007, 2, position))], q={"IveGotTicks": [10, 20, 30, 40, 50, 60]})
    # the focus: the window that has it is told, a new one too; the first press with none
    yield case_of(base + [(32, (1, 0x10005, 0, pack_position(20, 20))), (32, (1, 0x10005, 1, pack_position(20, 20))), (32, (2, 0x10005, 0, pack_position(60, 60))),
                          (7, (1, 2)), (32, (2, 0x10005, 2, pack_position(60, 60)))], q={"IveGotTicks": [10, 20, 30, 40, 50]})
    # the button record with a time before the clock's wrap and states that are not 0 or 1
    for state in (0, 1, 2, 3, -1):
        for held in (0, 1, 7):
            yield case_of(base + [(8, (1, 1, 0, 5, 5, held)), (7, (5, 0, state, 20, 20, T.s32(0xFFFFFF00))), (7, (5, 1, state, 21, 21, 5)),
                                  (32, (1, 0x10004, 0, pack_position(20, 20))), (32, (1, 0x10004, 1, pack_position(20, 20))),
                                  (32, (1, 0x10005, 0, pack_position(20, 20))), (32, (1, 0x10005, 1, pack_position(20, 20)))],
                          q={"IveGotTicks": [0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80]})


def message_random(r):
    ops, n = rnd_world(r, any_attribute)
    if r.random() < 0.7:
        ops += spare_ops()
    tables = [rnd_table(r) for _ in range(r.randint(1, 3))]
    ticks = rnd_ticks(r)
    for slot in r.sample(range(n), r.randint(0, n)):
        ops.append(anim_setup_op(r, slot, tables, ticks, r.choice([0, 1, 5, 20, 100, -1])))
    ops.append((7, (3, r.choice([0, 1]))))
    for _ in range(r.randint(1, 30)):
        ops.append(rnd_message(r, n) if r.random() < 0.8 else state_op(r, n, tables))
    return case_of(ops, q={"IveGotTicks": ticks, "GetHelpText": [r.choice([-1, 0, 5, 0x7FFFFFFF]) for _ in range(5)]}, tables=tables)


def long_script(r):
    """A script of 200 messages (with the odd setup between) over a tree with gestures in it."""
    ops, n = rnd_world(r, any_attribute, n=r.randint(4, 7))
    ops += spare_ops()
    tables = [rnd_table(r) for _ in range(3)]
    ticks = rnd_ticks(r, length=64)
    for slot in range(n):
        ops.append(anim_setup_op(r, slot, tables, ticks, r.choice([0, 1, 5, 20])))
    ops.append((7, (3, 1)))
    for _ in range(200):
        ops.append(rnd_message(r, n) if r.random() < 0.85 else state_op(r, n, tables))
    return case_of(ops, q={"IveGotTicks": ticks, "GetHelpText": [5, -1, 0]}, tables=tables)


def message_cases():
    yield from message_edges()
    for seed in range(6):
        yield long_script(__import__("random").Random(1000 + seed))


TESTS.append(FnTest("UI_DefaultWindowCallback", ui_ppc, ui_host, edge=message_cases, rnd=message_random))
