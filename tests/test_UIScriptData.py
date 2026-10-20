"""Differential tests for unit w36 (the lobby's window scripts as data, the island controller's message handler and the small window
virtuals), against libtpw_unit_UIScriptData.dylib (tests/unit_UIScriptData.cpp):

  src/UI_IslandLobby_msg.cpp   IslandLobbyControllerMsg, RemoveIslandLobbyController, HideIslandLobbyController, UpdateKeyParticles
  src/ui_whatami.cpp           the eleven WhatAmI members of InterfaceWindow, InterfaceBar, InterfaceCheckButtons, InterfaceEditBox,
                               InterfaceImage, InterfaceKnob, InterfaceLightPanel, InterfaceListBox, InterfaceListControl2,
                               InterfaceTextBox and InterfaceVaryBox; InterfaceWindow::CalcClientArea and CallDefaultWindowHandler
  src/ui_helptext.cpp          InterfaceHelpText (constructor, destructor, the 15 other members), UI_SetHelpTextHandler,
                               InterfaceTimer::InterfaceTimer
  src/ui_loginpanel_data.cpp   the login panel's script (371 words); the lobby's (38) and the island controller's (269) are
                               src/lobby.cpp's and src/UI_IslandLobby.cpp's, read from libtpw_unit_Lobby and libtpw_unit_IslandLobby

Three tests.

1. THE TABLES. The three scripts of the host build, word for word against the words at 0x1029e382, 0x102b006e and 0x102abbb0 of the
   program image (one case is the whole table, the others compare random stretches of it, so every word is compared many times).

2. THE WINDOW TREES. InterfaceWindow::Build runs on both sides under a new root window, with the original's and the translation's own
   factory, constructors, interpreter and WhatAmI (all real), on the three real scripts (the PowerPC side reads them from the image,
   the host from its arrays) and on made-up scripts: every kind of window and every record that asks WhatAmI for a kind (the list
   box's, list control's, knob's, vary box's and light panel's own records, and the same records for the wrong kind, which assert and
   go on), the draw objects, regions, client rectangles, absolute rectangles, failing kinds, and 200 random scripts of up to 120
   records. Compared: the window Build answered, the script position, the keyboard window, every window of the tree (kind by WhatAmI,
   id, attributes, rectangles, z, pivot, help text numbers, region, draw objects by the hash they were asked for, the handlers, the
   control's own words), the draw objects' counts, the heap traffic and the whole call log.

3. THE OPERATIONS. Scripts of up to 200 operations on a world of four windows, four help text objects (the default instance, base
   class objects and objects of a class that records HideHelpText), two timers, the fake screens and the controller's globals. The
   operations: the controller's message function (every message that matters and many that do not, every button, with and without
   the screens), Remove, Hide, UpdateKeyParticles on windows at the edges of the rectangle range, CalcClientArea, CallDefaultWindow-
   Handler, WhatAmI, the help text's constructor and destructor (flags of both signs) and members, UI_SetHelpTextHandler and the
   timer's constructor. After EVERY operation the answer, the windows, the globals, the help text objects, the timers, the heap traffic
   and the length of the call log are compared, and the whole call log at the end.

Real on both sides: the unit's functions; the interpreter, the factory, the constructors and the shapes (test 2); the window
constructor and the rectangles. Fakes that record the same event on both sides: the message senders, the pointer, the timer table,
the draw object library and the draw objects, the particle emitters, the key display, the window's GetPosition, the screens' members,
the UI clock, the assertion, and the allocator.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import ppc_const as P

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIScriptData"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(msg=0x101E0D98, remove=0x101E11F8, hide=0x101E12D4, key_particles=0x101E133C, call_default=0x101DE318,
          calc_client=0x101D987C, set_handler=0x101CA978, timer_ctor=0x101D819C, help_ctor=0x101CF8FC, help_dtor=0x101CF954)
WHAT_AM_I = {1: 0x101D9EB0, 2: 0x101CB010, 4: 0x101D0E78, 5: 0x101CD7A8, 6: 0x101CFF50, 7: 0x101D1918, 8: 0x101CC0C8, 9: 0x101C8848,
             10: 0x101D7D74, 11: 0x101D0228, 12: 0x101D8E0C, 13: 0x101D0A14}
# the help text's members in the order of the C++ file's operation 24
HELP_MEMBERS = [0x101CF874, 0x101CF8B8, 0x101CF9D0, 0x101CFA14, 0x101CFA88, 0x101CFABC, 0x101CFAF0, 0x101CFB24, 0x101CFB68,
                0x101CFC04, 0x101CFC8C, 0x101CFD10, 0x101CFDB0, 0x101CFD48, 0x101CFD7C]
# the slots (offsets in the vtable at 0x102afa88) of those members
HELP_SLOTS = [0x08, 0x0C, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44]
HELP_NAMES = ["ShowHelpText", "HideHelpText", "Enable", "Disable", "Show", "Hide", "Update", "OnMouseMoved", "OnMouseMovedIn",
              "OnMouseMovedOut", "OnMouseDown", "OnFrame", "OnHideOrDelete", "Init", "DeInit"]
# what the units run for real: the window's constructor, the interpreter, the factory, the constructors, the setters, the shapes,
# the rectangles and the regions' base
REAL = [0x101D98D8, 0x101DAB88, 0x101DEB78, 0x101DEC14, 0x101DB474, 0x101DB624, 0x101DBD9C, 0x101DCBA8, 0x101DCCEC, 0x101D0260,
        0x101D11B0, 0x101D47F0, 0x101D2488, 0x101D2584, 0x101D5BB0, 0x101D5CE4, 0x101D5E2C, 0x101DC750, 0x101DBE40,
        0x101CADC0, 0x101D0C50, 0x101CD4C4, 0x101CFE3C, 0x101D15CC, 0x101CBF90, 0x101C86C4, 0x101D7BF0, 0x101D008C, 0x101D8C94,
        0x101D08F0, 0x101CD468, 0x101D0B98, 0x101D154C,
        0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788, 0x101C7A1C,
        0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74]
BUILD = 0x101DEB78
WINDOW_CTOR = 0x101D98D8
# the fakes (in-binary functions)
POST_MESSAGE, SEND_MESSAGE, UPDATE_POINTER_IN, TICKS, ADD_TIMER = 0x101C9400, 0x101C9518, 0x101C8F18, 0x101CAC3C, 0x101D878C
RELEASE_STRING, GET_POSITION, INIT_EMITTER, KILL_EMITTER = 0x101D7B10, 0x101DB884, 0x100BEC30, 0x100BE1E8
KEY_DISPLAY, FIND_DRAW, WINDOW_SHOW = 0x101E188C, 0x101CD3B0, 0x101DC1EC

# ---- the original's data
G = dict(ctrl=0x10395CBC, emitter=0x10395CB4, screens=0x102E49B8, handler=0x102AF928, default_help=0x10395B8C, position=0x10395C5C,
         keyboard=0x10395C10, last=0x10395C18, hebrew=0x10395B88, redraw=0x10395C28)
HELP_VTABLE = 0x102AFA88
CORE_VTABLE, CIRCLE_VTABLE, ELLIPSE_VTABLE, POLY_VTABLE = 0x102AF8FC, 0x102AFCB8, 0x102AFC84, 0x102AFC50
# the window handlers (TVectors) by the numbers the C++ file gives: 1 and 2 the operations' two (made here), 3 the thumb's, then
# the window's default, the button's, check buttons, edit box, knob, list box, list control, text box, vary box
HANDLER_CODE = {0x10268390: 3, 0x10268480: 4, 0x10268110: 5, 0x10268128: 6, 0x102681C0: 7, 0x10268298: 8, 0x102682D0: 9,
                0x102682F0: 10, 0x102683F8: 11, 0x10268440: 12}
IMAGE_TABLES = {"lobby": (0x1029E382, 38), "island": (0x102B006E, 269), "login": (0x102ABBB0, 371)}

# ---- labels (the C++ file's)
LAB_WINDOW, LAB_HELP, LAB_TIMER, LAB_SCREEN, LAB_DEFAULT = 10, 20, 30, 40, 50
WINDOWS, HELPS, TIMERS = 4, 4, 2
MAX_QUEUE = 64

# the controller's window ids
ENTER, GLOBE, PREVIOUS, NEXT = 0x1E0E8, 0x1E0E9, 0x1E0F0, 0x1E0F1
BUTTON_IDS = [ENTER, GLOBE, PREVIOUS, NEXT]


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def w16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xFFFF))


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the dumps and the operations."""

    ADDRESSES = [(G["ctrl"], 4), (G["emitter"], 4), (G["screens"], 4), (G["handler"], 4), (G["default_help"], 0x18),
                 (G["position"], 4), (G["keyboard"], 4), (G["last"], 4), (G["hebrew"], 1), (G["redraw"], 4)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.heap = [0, 0, 0, 0]       # blocks handled by operator new, new[], delete, delete[] while a function runs
        self.observe = False           # true while a function under test runs: new blocks are filled and named
        self.windows = [0] * WINDOWS
        self.handler_code = [None] * WINDOWS   # the default handler of each window by number (None: not made)
        self.helps = [0] * HELPS
        self.help_heap = [False] * HELPS
        self.timers = [0] * TIMERS
        self.draw_of = {}              # hash -> block of the fake draw object
        self.hash_of = {}              # block -> hash
        self.draw_order = []
        self.emitter_next = 7
        self.handler_slot = 0          # the slot of the help text object UI_SetHelpTextHandler last installed (0: the default instance)
        fixed = {e.sd_theme: LAB_SCREEN, e.sd_globe: LAB_SCREEN + 1, G["default_help"]: LAB_DEFAULT}
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues)
        # the globals as the Mac's data has them
        for key in ("ctrl", "emitter", "screens", "position", "keyboard", "last", "redraw"):
            w32(e, G[key], 0)
        e.write(G["hebrew"], bytes(1))
        w32(e, G["handler"], G["default_help"])
        e.write(G["default_help"], bytes(0x18))
        w32(e, G["default_help"] + 0x10, HELP_VTABLE)
        T.call_ppc(e, FN["help_ctor"], [G["default_help"]])   # (the Mac makes the default instance at start-up)
        self.L.log.clear()

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def callback_code(self, tvector):
        if tvector == 0:
            return 0
        if tvector == self.e.sd_handlers[1]:
            return 1
        if tvector == self.e.sd_handlers[2]:
            return 2
        return HANDLER_CODE.get(tvector, 99)

    def tvector_of(self, code):
        return {0: 0, 1: self.e.sd_handlers[1], 2: self.e.sd_handlers[2], 3: 0x10268390}[code]

    def rect_code(self, address):
        for offset, code in ((4, 1), (0x10, 2), (0x1C, 3), (0x28, 4), (0x34, 5)):
            base = address - offset
            if self.L.lab(base) > 0:
                return 10 * self.L.lab(base) + code
        return -1

    def what_am_i(self, w):
        e = self.e
        tvector = e.u32(e.u32(w + 0x118) + 0x14)
        return T.s32(self.call(e.u32(tvector), w))

    # ---- the dumps (the orders are the C++ file's)
    def dump_rect(self, d, offset):
        return list(struct.unpack(">4h", d[offset:offset + 8]))

    def region_words(self, region, window):
        e = self.e
        if region == 0:
            return [0] * 5
        if region == window + 4:
            return [1, 0, 0, 0, 0]
        vptr = e.u32(region)
        c = struct.unpack(">4h", e.read(region + 4, 8))
        if vptr == CORE_VTABLE:
            return [3] + list(c)
        if vptr == CIRCLE_VTABLE:
            return [4, c[0], c[1], c[2], 0]
        if vptr == ELLIPSE_VTABLE:
            return [5] + list(c)
        if vptr == POLY_VTABLE:
            count = c[0]
            points = e.u32(region + 8)
            out = [6, count, int(points != 0), 0, 0]
            if points and count > 0:
                out += list(struct.unpack(">%dh" % (2 * count), e.read(points, 4 * count)))
            return out
        return [2, 0, 0, 0, 0]

    def draw_hash(self, address):
        return 0 if address == 0 else self.hash_of.get(address, -1)

    def dump_window(self, w, depth):
        e, L = self.e, self.L
        kind = self.what_am_i(w)
        size = 0x180
        d = bytes(e.read(w, size))
        h = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        i = lambda o: T.s32(struct.unpack(">I", d[o:o + 4])[0])   # noqa: E731
        u = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [depth, L.lab(w), kind, i(0x88), i(0x44)]
        out += self.dump_rect(d, 8) + self.dump_rect(d, 0x2C) + self.dump_rect(d, 0x14)
        out += [h(0xDC), h(0xDE), h(0xE0), h(0xE2), i(0x8C), i(0x90)]
        out += self.region_words(u(0x40), w)
        out += [self.draw_hash(u(0x94)), self.draw_hash(u(0x98)), self.callback_code(u(0x110)), self.callback_code(u(0x114)),
                h(0x4C), L.lab(u(0x5C))]
        if kind == 2:
            out += [d[0x138] - 256 if d[0x138] >= 128 else d[0x138], d[0x139] - 256 if d[0x139] >= 128 else d[0x139], i(0x13C), i(0x140),
                    int(u(0x148) != 0), u(0x14C), u(0x154), T.s8(d[0x158]), T.s8(d[0x159]), d[0x15A]]
        elif kind == 5:
            out += [int(u(0x138) != 0), i(0x13C), int(u(0x164) != 0), d[0x162]]
        elif kind == 4:
            out += self.dump_rect(d, 0x13C)
        elif kind == 7:
            out += self.dump_rect(d, 0x13C)
            count = h(0x164)
            columns = u(0x16C)
            out += [count, int(columns != 0)]
            if columns and 0 < count <= 8:
                for k in range(count):
                    out += struct.unpack(">4h", e.read(columns + 0xC * k + 4, 8))
        elif kind == 11:
            out += [i(0x138), i(0x13C), i(0x140)]
        elif kind == 12:
            out += [i(0x138), i(0x13C), i(0x140), i(0x148), d[0x154]]
        elif kind == 13:
            out += [i(0x138)]
        elif kind == 10:
            out += [int(u(0x138) != 0), i(0x148)]
        elif kind == 8:
            out += [i(0x138)]
        return out

    def dump_tree(self, w, depth):
        out = []
        while w:
            out += self.dump_window(w, depth)
            out += self.dump_tree(self.e.u32(w + 0x58), depth + 1)
            w = self.e.u32(w + 0x50)
        return out

    # ---- the tree test
    def tree(self, case):
        e, L = self.e, self.L
        kind, script, callback = case["script"]
        if kind == "image":
            address, count = IMAGE_TABLES[script]
        else:
            data = struct.pack(">%dh" % len(script), *[s16(v) for v in script])
            address = e.alloc(len(data), data)
        root = e.alloc(0x138, b"\xa5" * 0x138)
        self.observe = True
        self.heap[0] += 1
        L.newlab(root)   # (the host's root comes from operator new and is named)
        self.call(WINDOW_CTOR, root, 0, 1, 0, 0, 0, 2048, 1536)
        made = self.call(BUILD, root, address, self.tvector_of(callback))
        self.observe = False
        tree = self.dump_tree(root, 0)
        draws = []
        for block in self.draw_order:
            draws += [self.hash_of[block], struct.unpack(">h", e.read(block + 0xC, 2))[0], struct.unpack(">h", e.read(block + 0xE, 2))[0],
                      struct.unpack(">h", e.read(block + 4, 2))[0]]
        return dict(answer=[L.lab(made), T.s32(e.u32(G["position"])), L.lab(e.u32(G["keyboard"]))], tree=tree, draws=draws,
                    heap=list(self.heap), globals=[L.lab(e.u32(G["last"])), e.read(G["hebrew"], 1)[0], T.s32(e.u32(G["redraw"]))],
                    log=list(L.log))

    # ---- the operations' world
    def window(self, slot):
        return self.windows[slot] if 0 <= slot < WINDOWS else 0

    def help_object(self, slot):
        if slot == 0:
            return G["default_help"]
        return self.helps[slot] if 0 < slot < HELPS else 0

    def dump_state(self):
        e, L = self.e, self.L
        out = []
        for slot in range(WINDOWS):
            w = self.windows[slot]
            out.append(int(w != 0))
            if w == 0:
                continue
            d = bytes(e.read(w, 0x138))
            for offset in (8, 0x14, 0x20, 0x2C, 0x38):
                out += self.dump_rect(d, offset)
            u = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
            out += [self.callback_code(u(0x110)), self.callback_code(u(0x114)), T.s32(u(0x44)), T.s32(u(0x88)),
                    struct.unpack(">h", d[0xDC:0xDE])[0], L.lab(u(0x5C))]
        out += [L.lab(e.u32(G["ctrl"])), T.s32(e.u32(G["emitter"])), int(e.u32(G["screens"]) != 0), self.handler_slot]
        for slot in range(HELPS):
            h = self.help_object(slot)
            out.append(int(h != 0))
            if h == 0:
                continue
            x, y, z, v, wn = struct.unpack(">hhhhh", e.read(h, 10))
            out += [x, y, z, v, wn]
            ticks = e.u32(h + 0xC)
            out += [ticks, L.lab(e.u32(h + 0x14))]
        for slot in range(TIMERS):
            t = self.timers[slot]
            out.append(int(t != 0))
            if t == 0:
                continue
            out += [L.lab(e.u32(t)), e.u32(t + 4), T.s32(e.u32(t + 8)), T.s32(e.u32(t + 0xC)), T.s32(e.u32(t + 0x10)),
                    T.s32(e.u32(t + 0x14)), L.lab(e.u32(t + 0x18)), L.lab(e.u32(t + 0x1C))]
        out += list(self.heap)
        out.append(len(L.log))
        return out

    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        e, L = self.e, self.L
        zero = [0, 0, 0, 0, 0]
        made = lambda slot: 0 <= slot < WINDOWS and self.windows[slot] != 0   # noqa: E731
        if code == 1:   # a window: slot, parent slot, attributes, id, left, top, right, bottom, default handler
            slot, parent = a[0], a[1]
            if not 0 <= slot < WINDOWS or made(slot) or (parent >= 0 and not made(parent)) or parent >= slot:
                return None
            block = e.alloc(0x300, b"\xa5" * 0x300)
            L.labels[block] = LAB_WINDOW + slot
            self.call(WINDOW_CTOR, block, self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]), s16(a[7]))
            self.windows[slot] = block
            code_ = 4   # (UI_DefaultWindowCallback)
            if a[8] >= 0:
                w32(e, block + 0x114, self.tvector_of(a[8]))
                code_ = a[8]
            self.handler_code[slot] = code_
            return [L.lab(block), 0, 0, 0, 0]
        if code in (2, 3, 14, 15, 16) and not made(a[0]):
            return None
        if code == 2:   # the rectangle: slot, left, top, right, bottom
            e.write(self.windows[a[0]] + 8, struct.pack(">4h", s16(a[1]), s16(a[2]), s16(a[3]), s16(a[4])))
            return zero
        if code == 3:   # the default handler
            w32(e, self.windows[a[0]] + 0x114, self.tvector_of(a[1]))
            self.handler_code[a[0]] = a[1]
            return zero
        if code == 4:   # the controller's global
            if a[0] >= 0 and not made(a[0]):
                return None
            w32(e, G["ctrl"], self.window(a[0]))
            return zero
        if code == 5:
            w32(e, G["emitter"], a[0])
            return zero
        if code == 6:
            w32(e, G["screens"], e.sd_screens if a[0] else 0)
            return zero
        answer = list(zero)
        self.observe = True
        try:
            if code == 10:   # IslandLobbyControllerMsg: slot, message, a, b
                message, x = T.s32(a[1] & MINUS), T.s32(a[2] & MINUS)
                if not made(a[0]) or self.handler_code[a[0]] == 0:
                    return None
                if message == 0x100 and x in BUTTON_IDS and e.u32(G["screens"]) == 0:
                    return None
                answer[0] = T.s32(self.call(FN["msg"], self.window(a[0]), a[1], a[2], a[3]))
            elif code == 11:
                self.call(FN["remove"])
            elif code == 12:
                self.call(FN["hide"])
            elif code == 13:   # UpdateKeyParticles: slot (-1 none), on
                if (a[1] != 0 or a[0] >= 0) and not made(a[0]):
                    return None
                self.call(FN["key_particles"], self.window(a[0]), a[1])
            elif code == 14:
                self.call(FN["calc_client"], self.windows[a[0]])
            elif code == 15:   # CallDefaultWindowHandler: slot, message, a, b
                if self.handler_code[a[0]] == 0:
                    return None
                answer[0] = T.s32(self.call(FN["call_default"], self.windows[a[0]], a[1], a[2], a[3]))
            elif code == 16:
                answer[0] = T.s32(self.call(WHAT_AM_I[1], self.windows[a[0]]))
            elif code in (20, 21):   # a help text object: slot (1 to 3), class (1 base, 2 recording)
                slot = a[0]
                if not 1 <= slot < HELPS or self.helps[slot]:
                    return None
                block = e.alloc(0x40, b"\xa5" * 0x40)
                L.labels[block] = LAB_HELP + slot
                if code == 21:
                    self.heap[0] += 1
                self.call(FN["help_ctor"], block)
                if a[1] == 2:
                    w32(e, block + 0x10, e.sd_recording_vtable)   # (a derived class's constructor puts its own vtable in after the base's)
                self.helps[slot] = block
                self.help_heap[slot] = code == 21
            elif code == 22:   # the destructor: slot, flag
                slot = a[0]
                if not 1 <= slot < HELPS or not self.helps[slot]:
                    return None
                flag = a[1]
                if T.s16(flag & 0xFFFF) > 0 and not self.help_heap[slot]:
                    return None   # (a delete of a block the world did not get from operator new)
                self.call(FN["help_dtor"], self.helps[slot], flag)
                self.helps[slot] = 0
                self.help_heap[slot] = False
            elif code == 23:   # UI_SetHelpTextHandler: slot (-1 none)
                if a[0] >= 0 and not self.help_object(a[0]):
                    return None
                self.call(FN["set_handler"], self.help_object(a[0]) if a[0] >= 0 else 0)
                handler = e.u32(G["handler"])
                self.handler_slot = 0 if handler == G["default_help"] else -1
                for slot in range(1, HELPS):
                    if self.helps[slot] and handler == self.helps[slot]:
                        self.handler_slot = slot
            elif code == 24:   # a member of a help text: slot, which, arguments
                h = self.help_object(a[0])
                if h == 0:
                    return None
                which = a[1]
                # (a virtual call, as the host's: through the vtable the object carries, whose slots are the image's data)
                tvector = e.u32(e.u32(h + 0x10) + HELP_SLOTS[which])
                member = e.u32(tvector)
                if which == 7:
                    self.call(member, h, s16(a[2]), s16(a[3]))
                elif which in (8, 12):
                    self.call(member, h, self.window(a[2]))
                else:
                    self.call(member, h)
            elif code == 25:   # a field of a help text, stored raw: slot, which (0 enabled, 1 wanted, 2 showing), value
                h = self.help_object(a[0])
                if h == 0:
                    return None
                w16(e, h + (0, 2, 8)[a[1]], a[2])
            elif code == 26:   # a timer: slot
                slot = a[0]
                if not 0 <= slot < TIMERS:
                    return None
                block = e.alloc(0x20, b"\xa5" * 0x20)
                L.labels[block] = LAB_TIMER + slot
                self.call(FN["timer_ctor"], block)
                self.timers[slot] = block
            elif code == 30:   # the default help text made again over 0xa5 bytes
                e.write(G["default_help"], b"\xa5" * 0x18)
                w32(e, G["default_help"] + 0x10, HELP_VTABLE)
                self.call(FN["help_ctor"], G["default_help"])
            else:
                raise ValueError(code)
        finally:
            self.observe = False
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects', handlers', screens' and recording help text's tables, in persistent memory."""
    if hasattr(e, "sd_ready"):
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
    e.sd_draw_vtable = vt

    # the two handlers the operations give windows: record and answer from the queue "Default"
    def handler(number, default):
        def fn(emu):
            L = emu.cl
            L.rec("Handler", (number, L.lab(emu.r(3)), T.s32(emu.r(4)), T.s32(emu.r(5)), T.s32(emu.r(6))))
            emu.set_r(3, T.u32(L.ret("Default", default)))
        return fn

    e.sd_handlers = {1: T.fake_function(e, handler(1, 0x55)), 2: T.fake_function(e, handler(2, 0x66))}

    # the screens: a theme island screen (Hide at 0x28, next at 0x40, previous at 0x44, select at 0x48) and a download globe (Show at
    # 0x24), and the screen system pointing at both (+4 and +0xc)
    def rec_this(name):
        return T.fake_function(e, lambda emu: emu.cl.rec(name, (emu.cl.lab(emu.r(3)),)))

    theme_vt = T.persist_alloc(e, 0x60)
    for slot, name in ((0x28, "Theme.Hide"), (0x40, "Theme.GotoNextIsland"), (0x44, "Theme.GotoPreviousIsland"),
                       (0x48, "Theme.SelectCurrentIsland")):
        e.write(theme_vt + slot, struct.pack(">I", rec_this(name)))
    globe_vt = T.persist_alloc(e, 0x60)
    e.write(globe_vt + 0x24, struct.pack(">I", rec_this("Globe.Show")))
    e.sd_theme = T.persist_alloc(e, 0x20, struct.pack(">I", theme_vt) + bytes(0x1C))
    e.sd_globe = T.persist_alloc(e, 0x20, struct.pack(">I", globe_vt) + bytes(0x1C))
    e.sd_screens = T.persist_alloc(e, 0x20, struct.pack(">IIIIII", 0, e.sd_theme, 0, e.sd_globe, 0, 0) + bytes(8))

    # a help text class whose HideHelpText (the slot at 0xc) records and then does the base class's work
    def hide_help(emu):
        this = emu.r(3)
        emu.cl.rec("Help.HideHelpText", (emu.cl.lab(this),))
        emu.write(this + 2, struct.pack(">H", 0))

    base_vt = bytes(e.read(HELP_VTABLE, 0x48))
    e.sd_recording_vtable = T.persist_alloc(e, 0x48, base_vt)
    e.write(e.sd_recording_vtable + 0xC, struct.pack(">I", T.fake_function(e, hide_help)))
    e.sd_ready = True


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN[k] for k in ("msg", "remove", "hide", "key_particles", "call_default", "calc_client", "set_handler", "timer_ctor",
                                  "help_ctor", "help_dtor"))
    S.real.update(WHAT_AM_I.values())
    S.real.update(HELP_MEMBERS)
    S.real.update(REAL)
    R = CL.recorder

    # the heap: blocks made while a function runs are filled with 0xa5 and named (objects), and counted
    def op_new(e):
        us, size = e.us, e.r(3)
        if us.observe and us.L.ret("NewFails") != 0:
            e.set_r(3, 0)
            return
        if us.observe:
            block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
            us.heap[0] += 1
            us.L.newlab(block)
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
    S.imp("__assertion_failed", lambda e: e.cl.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))

    def construct(e, call):
        """__construct_new_array(raw, constructor, destructor, size, count): the array of the list control's columns."""
        raw, ctor, size, n = e.r(3), e.r(4), e.r(6), e.r(7)
        if raw == 0:
            return 0
        w32(e, raw, size)
        w32(e, raw + 4, n)
        base = raw + 8
        for i in range(n):
            call(ctor, base + i * size, 1)
        return base

    def callback_stub(fn):
        def hook(e):
            e.pending_cb = fn
            e.uc.reg_write(P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
        return hook

    S.imp("__construct_new_array", callback_stub(construct))

    # the senders, the pointer, the clock and the timer table
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(SEND_MESSAGE, R("SendMessage", "Liii", ret=lambda e, L: 0))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(TICKS, R("IveGotTicks", "", ret=lambda e, L: L.ret("Ticks", 1000)))
    S.internal(ADD_TIMER, R("AddTimer", "Lii", ret=lambda e, L: 0))
    S.internal(RELEASE_STRING, R("ReleaseString", "L"))
    S.internal(WINDOW_SHOW, R("Window.Show", "Lh", ret=lambda e, L: 1))

    # the controller's callees
    def next_emitter(e, L):
        handle = e.us.emitter_next
        e.us.emitter_next += 1
        return handle

    S.internal(KEY_DISPLAY, R("UpdateIslandLobbyKeyDisplay"))
    S.internal(INIT_EMITTER, R("PAR_InitEmitter", "iiii", ret=next_emitter))
    S.internal(KILL_EMITTER, R("PAR_KillEmitter", "i"))

    def get_position(e, L):
        this, rect = e.r(3), e.r(4)
        e.write(rect + 4, bytes(e.read(this + 8, 8)))
        return 1

    S.internal(GET_POSITION, R("GetPosition", "Lxh", ret=get_position))

    # the draw object library: one fake draw object per hash, named when first asked for
    def find_draw(e, L):
        us, h = e.us, e.r(3)
        block = us.draw_of.get(h)
        if block is None:
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.sd_draw_vtable)
            L.newlab(block)
            us.draw_of[h] = block
            us.hash_of[block] = h
            us.draw_order.append(block)
        return block

    S.internal(FIND_DRAW, R("FindDrawObject", "u", ret=find_draw))
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_sd_open", False):
        return
    lib.tpw_sd_tree.argtypes = [C.c_void_p, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sd_tree.restype = C.c_int32
    lib.tpw_sd_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_sd_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sd_state.restype = C.c_int32
    lib.tpw_sd_login_pointer.restype = C.c_void_p
    lib._sd_open = True


def host_table(name):
    """The host's array of a script: the unit's own library for the login panel, the lobby's and the island controller's for theirs."""
    count = IMAGE_TABLES[name][1]
    if name == "login":
        lib = RUNNER.lib
        return (C.c_int16 * count).from_address(lib.tpw_sd_login_pointer())
    library, symbol = {"lobby": ("unit_Lobby", "gLobbyWindowData"), "island": ("unit_IslandLobby", "gIslandLobbyWindowData")}[name]
    return (C.c_int16 * count).in_dll(RUNNER.shim(library), symbol)


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        self.lib = lib
        open_host(lib)
        lib.tpw_sd_ops_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues)
        lib.tpw_sd_labels()

    def op(self, code, a):
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 5)()
        self.lib.tpw_sd_op(code, args, out)
        return list(out)

    def dump_state(self):
        buf = (C.c_int64 * 4096)()
        count = self.lib.tpw_sd_state(buf)
        return list(buf[:count])


# ======================================================================= test 1: the tables


def tables_ppc(e, case):
    name, start, stop = case
    address, count = IMAGE_TABLES[name]
    return dict(words=list(struct.unpack(">%dh" % (stop - start), e.read(address + 2 * start, 2 * (stop - start)))))


def tables_host(lib, case):
    name, start, stop = case
    return dict(words=list(host_table(name)[start:stop]))


def tables_edges():
    for name, (address, count) in IMAGE_TABLES.items():
        yield (name, 0, count)
        yield (name, 0, 1)
        yield (name, count - 1, count)


def tables_rnd(r):
    name = r.choice(sorted(IMAGE_TABLES))
    count = IMAGE_TABLES[name][1]
    start = r.randrange(count)
    return (name, start, r.randrange(start + 1, min(count, start + 60) + 1))


# ======================================================================= test 2: the window trees

TREE_RAN = {}


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


@with_stubs
def tree_ppc(e, case):
    world = Ppc(e, case)
    try:
        return world.tree(case)
    finally:
        world.restore()


def tree_host(lib, case):
    open_host(lib)
    kind, script, callback = case["script"]
    lib.tpw_sd_reset()
    queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
    CL.host_begin(lib, {}, queues)
    lib.tpw_sd_labels()
    if kind == "image":
        words = host_table(script)
        pointer = C.cast(words, C.c_void_p)
        keep = words
    else:
        keep = (C.c_int16 * len(script))(*[s16(v) for v in script])
        pointer = C.cast(keep, C.c_void_p)
    out = (C.c_int64 * 65536)()
    count = lib.tpw_sd_tree(pointer, callback, out)
    log = CL.host_end(lib)
    words = list(out[:count])
    n = words[3]
    tree = words[4:4 + n]
    rest = words[4 + n:]
    draws, heap, globals_ = rest[:-7], rest[-7:-3], rest[-3:]
    del keep
    return dict(answer=words[:3], tree=tree, draws=draws, heap=heap, globals=globals_, log=log)


# ---- the scripts
def child(kind, attributes=1, ident=0x10, rect=(0, 0, 100, 100)):
    """Record 0: a child window (kind, attributes (2 words), id (2), rectangle); its own records follow up to a record 5."""
    return [0, kind, attributes & 0xFFFF, (attributes >> 16) & 0xFFFF, ident & 0xFFFF, (ident >> 16) & 0xFFFF] + list(rect)


def draw(h):
    return [1, h & 0xFFFF, (h >> 16) & 0xFFFF]


def client_draw(h):
    return [2, h & 0xFFFF, (h >> 16) & 0xFFFF]


def long_record(record, value):
    return [record, value & 0xFFFF, (value >> 16) & 0xFFFF]


END = [5]


def script_case(words, callback=0, **extra):
    case = dict(script=("words", list(words) + [5] * 8, callback))
    case.update(extra)
    return case


def image_case(name, callback=0):
    return dict(script=("image", name, callback))


KINDS_SAFE = [1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
BAD_KINDS = [0, 14, 15, 99, -1, 0x10000, -0x8000]


def tree_edges():
    for name in ("lobby", "island", "login"):
        for callback in (0, 1, 2):
            yield image_case(name, callback)
    yield script_case([5])                       # nothing at all
    yield script_case([], 1)                      # (only padding: ends at once)
    yield script_case([0x13, 7, 7, 7])            # an unknown record ends the level
    yield script_case([-1, 7, 7, 7], 2)
    # every kind of window alone, with the attribute sets that matter
    for kind in KINDS_SAFE + BAD_KINDS:
        for attributes in (1, 0x41, 0x401, 0x100, 0x3, 0):
            # (a kind the factory fails on leaves the level without a window: its records would write to NULL, as the original's do)
            body = long_record(17, 345) + long_record(18, -1) if kind in KINDS_SAFE else []
            yield script_case(child(kind, attributes, 0x1234, (5, 6, 700, 800)) + body + END + END)
    # every kind inside the one before
    nested = []
    for kind in KINDS_SAFE:
        nested += child(kind, 1, 0x100 + kind, (kind, kind, 500 + kind, 600 + kind))
    yield script_case(nested + END * (len(KINDS_SAFE) + 1))
    # every kind side by side, and the callback goes to the last one
    side = []
    for kind in KINDS_SAFE:
        side += child(kind, 1, 0x200 + kind, (10 * kind, 0, 10 * kind + 9, 99)) + END
    yield script_case(side + END, 1)
    # the records that ask WhatAmI, for the right kind
    yield script_case(child(4, 1, 0x20, (0, 0, 300, 200)) + [10, 1, 2, 30, 40] + END + END)
    yield script_case(child(7, 1, 0x21, (0, 0, 300, 200)) + [10, 5, 6, 70, 80] + [11, 3, 0, 10, 10, 20, 20, 30] + END + END)
    yield script_case(child(7, 1, 0x21, (0, 0, 300, 200)) + [11, 2, 1, 2, 3, 4] + [12, 0, 5, 5, 50, 15] + END + [12, 3, 6, 6, 60, 16] + END + END + END)
    yield script_case(child(7, 1, 0x21, (0, 0, 300, 200)) + [11, 0] + [12, -20, 5, 5, 50, 15] + END + END + END)
    yield script_case(child(11, 1, 0x22, (0, 0, 64, 64)) + [13, 100, 3000] + END + END)
    yield script_case(child(11, 1, 0x22, (0, 0, 64, 64)) + [13, 3000, 100] + END + END)
    yield script_case(child(11, 1, 0x22, (0, 0, 64, 64)) + [13, -5, -9] + END + END)
    yield script_case(child(12, 1, 0x23, (0, 0, 64, 64)) + [14, 1, 2, 10, 20] + END + [15, 30, 40, 50, 60] + END + END + END)
    yield script_case(child(12, 1, 0x23, (0, 0, 64, 64)) + [14, 1, 2, 10, 20] + END + [14, 3, 4, 5, 6] + END + END + END)
    yield script_case(child(13, 1, 0x24, (0, 0, 400, 400)) + [16, 0, 1, 2, 3, 4] + draw(0x1234) + END + [16, 31, 5, 6, 7, 8] + END + END + END)
    yield script_case(child(13, 1, 0x24, (0, 0, 400, 400)) + [16, 0, 1, 2, 3, 4] + END + [16, 0, 5, 6, 7, 8] + END + [16, 32, 1, 1, 2, 2] + END
                      + [16, -1, 1, 1, 2, 2] + END + END + END)
    # the same records for the wrong kind: an assertion and on (records 11 and 13 are left out: they would write to a window of
    # another class), and the records that take no words on a window that is not a list
    for kind in (1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13):
        body = [10] if kind not in (4, 7) else []   # (record 10 takes no words on a window that is not a list)
        yield script_case(child(kind, 1, 0x30 + kind) + body + [12, 1, 9, 9, 19, 19] + END + [14, 8, 8, 18, 18] + END + [15, 7, 7, 17, 17] + END
                          + [16, 3, 4, 4, 14, 14] + END + [6, 1, 1, 11, 11] + END + [7, 2, 2, 12, 12] + END + [8, 3, 3, 13, 13] + END
                          + ([9] if kind not in (4, 7) else []) + END + END)
    # draw objects, regions, client rectangles, absolute rectangles
    yield script_case(child(1, 1, 0x40, (10, 20, 110, 220)) + draw(0xAAAA5555) + client_draw(0x12345678) + [3, 1, 2, 3, 4] + END + END)
    yield script_case(child(1, 0x401, 0x40, (10, 20, 110, 220)) + [3, 1, 2, 3, 4] + END
                      + child(1, 0x401, 0x41, (5, 5, 50, 50)) + [3, 1, 2, 3, 4] + END + END, 2)
    yield script_case(child(1, 1, 0x42) + draw(7) + END + child(1, 1, 0x43) + draw(7) + client_draw(7) + END + child(1, 1, 0x44) + client_draw(8) + END + END)
    yield script_case(child(1, 1, 0x45) + [4, 1, 1, 2, 3, 4] + END + child(1, 1, 0x46) + [4, 2, 5, 6, 7] + END + child(1, 1, 0x47) + [4, 3, 5, 6, 7, 8] + END
                      + child(1, 1, 0x48) + [4, 4, 3, 10, 11, 12, 13, 14, 15] + END + child(1, 1, 0x49) + [4, 4, 0] + END
                      + child(1, 1, 0x4A) + [4, 9] + END + child(1, 1, 0x4B) + [4, 0] + END + END)
    yield script_case(child(1, 1, 0x4B) + [4, 4, 1, 5, 6] + END + child(2, 1, 0x4C) + [4, 1, 0, 0, 9, 9] + END + END)
    yield script_case(child(1, 1, 0x4D) + [17, 1, 0, 18, 7, 7] + END + END)
    # failing allocation of a polygon or a region
    yield script_case(child(1, 1, 0x4E) + [4, 4, 2, 1, 2, 3, 4] + END + END, q=dict(NewFails=[0, 0, 0, 1]))
    yield script_case(child(1, 1, 0x4E) + [4, 1, 1, 2, 3, 4] + END + END, q=dict(NewFails=[0, 0, 1]))
    # the message queue's answers to the constructor
    yield script_case(child(1, 1, 0x4F) + END + END, q=dict(PostMessage=[5, 6, 7]))
    # a script that is a long chain of levels
    chain = []
    for k in range(10):
        chain += child(1, 1, 0x50 + k, (k, k, 100 - k, 100 - k))
    yield script_case(chain + END * 11)


def random_script(r):
    """A random script of up to 120 records that the original and the translation can both run safely (see the comments)."""
    words = []
    budget = [r.randrange(3, 120)]
    hashes = [0x1111, 0x2222, 0x3333, 0x44444444, 0x80000001, 0xFFFFFFFF]
    used = set()

    def rect():
        return [r.randrange(-20, 600), r.randrange(-20, 600), r.randrange(-20, 900), r.randrange(-20, 900)]

    def level(kind, depth, ok):
        """The records of one level on a window of this kind; `ok` is False for the level of a window that failed (only the end)."""
        if not ok:
            words.append(5)
            return
        drawn = False
        clientdrawn = False
        while budget[0] > 0:
            budget[0] -= 1
            choice = r.randrange(24)
            if choice < 6 and depth < 5:
                k = r.choice(KINDS_SAFE + [1, 1, 2, 2] + (BAD_KINDS if r.randrange(6) == 0 else []))
                attributes = r.choice([1, 1, 3, 0x41, 0x401, 0x100, 0, 0x10001])
                words.extend(child(k, attributes, r.randrange(1, 0x20000), rect()))
                level(k, depth + 1, k in KINDS_SAFE)
            elif choice == 6 and not drawn:
                drawn = True
                words.extend(draw(r.choice(hashes)))
            elif choice == 7 and not clientdrawn:
                clientdrawn = True
                words.extend(client_draw(r.choice(hashes)))
            elif choice == 8:
                words.extend([3] + rect())
            elif choice == 9:
                shape = r.choice([1, 2, 3, 4, 4, 9])
                if shape == 1:
                    words.extend([4, 1] + rect())
                elif shape == 2:
                    words.extend([4, 2, r.randrange(100), r.randrange(100), r.randrange(50)])
                elif shape == 3:
                    words.extend([4, 3, r.randrange(100), r.randrange(100), r.randrange(50), r.randrange(50)])
                elif shape == 4:
                    n = r.randrange(0, 6)
                    words.extend([4, 4, n] + [r.randrange(-50, 300) for _ in range(2 * n)])
                else:
                    words.extend([4, shape])
            elif choice == 10 and kind in (4, 7):
                words.extend([10] + rect())
            elif choice == 11 and kind == 7:
                n = r.randrange(0, 4)
                words.extend([11, n] + [r.randrange(0, 300) for _ in range(2 * n)])
            elif choice == 12 and kind == 7 and depth < 5:
                words.extend([12, r.randrange(-5, 10)] + rect())
                level(2, depth + 1, True)
            elif choice == 13 and kind == 11:
                words.extend([13, r.randrange(-100, 5000), r.randrange(-100, 5000)])
            elif choice in (14, 15) and depth < 5:
                words.extend([choice] + rect())
                level(2, depth + 1, True)
            elif choice == 16 and depth < 5:
                words.extend([16, r.choice([0, 1, 5, 31, 31, 32, -1, 3])] + rect())
                level(6, depth + 1, True)
            elif choice in (17, 18):
                words.extend(long_record(choice, r.choice([-1, 0, 1, 345, 0x10000, -0x8000])))
            elif choice == 19 and kind not in (4, 7):
                words.append(9)
            elif choice == 20 and kind not in (4, 7):
                words.append(10)
            elif choice in (21, 22) and depth < 5:
                words.extend([6 + (choice - 21)] + rect())
                level(2, depth + 1, True)
            elif choice == 23 and depth < 5:
                words.extend([8] + rect())
                level(2, depth + 1, True)
            else:
                continue
            if r.randrange(14) == 0:
                break
        words.append(5)

    while budget[0] > 0:
        budget[0] -= 1
        k = r.choice(KINDS_SAFE + BAD_KINDS[:2])
        words.extend(child(k, r.choice([1, 3, 0x41, 0x401]), r.randrange(1, 0x20000), rect()))
        level(k, 1, k in KINDS_SAFE)
    words.append(5)
    return words


def tree_rnd(r):
    return script_case(random_script(r), r.choice([0, 1, 2]), q=dict(PostMessage=[r.choice([0, 0, 3])]))


# ======================================================================= test 3: the operations

RAN = {}   # what the PowerPC side decided to run, for the host side (by case)


@with_stubs
def ops_ppc(e, case):
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


def ops_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for code, a in RAN.pop(id(case)):
            answer = host.op(code, a)
            steps.append(dict(op=code, answer=answer, state=host.dump_state()))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


# the operations (the C++ file's numbers)
def make_window(slot, parent=-1, attributes=1, ident=100, rect=(0, 0, 100, 100), handler=-1):
    return (1, (slot, parent, attributes, ident) + tuple(rect) + (handler,))


def set_rect(slot, rect):
    return (2, (slot,) + tuple(rect))


def set_handler(slot, code):
    return (3, (slot, code))


def controller(slot):
    return (4, (slot,))


def emitter(handle):
    return (5, (handle,))


def screens(present):
    return (6, (present,))


def msg(slot, message, a=0, b=0):
    return (10, (slot, message, a, b))


REMOVE, HIDE = (11, ()), (12, ())


def key_particles(slot, on):
    return (13, (slot, on))


def calc_client(slot):
    return (14, (slot,))


def call_default(slot, message, a=0, b=0):
    return (15, (slot, message, a, b))


def what_am_i(slot):
    return (16, (slot,))


def make_help(slot, cls=1, heap=False):
    return (21 if heap else 20, (slot, cls))


def destroy_help(slot, flag):
    return (22, (slot, flag))


def set_help_handler(slot):
    return (23, (slot,))


def help_member(slot, which, x=0, y=0):
    return (24, (slot, which, x, y))


def poke_help(slot, which, value):
    return (25, (slot, which, value))


def make_timer(slot):
    return (26, (slot,))


REMAKE_DEFAULT = (30, ())

MESSAGES = [0x14, 0x11, 0x100, 0x10, 0x12, 0x13, 0x15, 0xFF, 0x101, 0, 1, 4, 6, -1, 0x7FFFFFFF, -0x80000000, 0x1E0E8, 0x10011, 0x114]
ARGS = [0, 1, -1, 2, 0x1E0E7, 0x1E0E8, 0x1E0E9, 0x1E0EA, 0x1E0EF, 0x1E0F0, 0x1E0F1, 0x1E0F2, 0x1E0E6, 0x7FFFFFFF, -0x80000000, 0x100]
RECTS = [(0, 0, 100, 100), (10, 20, 110, 220), (-5, -7, 3, 4), (0, 0, 0, 0), (100, 100, 50, 40), (0x7FF0, 0x7FF0, 0x7FFF, 0x7FFF),
         (-0x8000, -0x8000, -0x7FF0, -0x7FF0), (5, 6, 0x7FFE, 7), (0x7FFC, 0, 0x7FFE, 9), (0, 855, 476, 1521), (0, 1535, 1, 1536),
         (-1, -1, 0, 0), (3, 0x7FFF, 4, 0x7FFF), (7, -0x8000, 8, 0x7FFF), (1, 1535, 2, 1537), (-3, 1, -2, 2)]


def base_world(handler=1, attributes=1):
    """The controller's panel as window 0, a second window, the screens and the controller's global."""
    return [make_window(0, -1, attributes, 0x1E0E7, (28, 855, 476, 1521), handler), make_window(1, 0, 1, 0x1E0EA, (554, 170, 990, 437), 2),
            controller(0), screens(1)]


def ops_case(ops, **extra):
    case = dict(ops=list(ops))
    case.update(extra)
    return case


def controller_edges():
    # every message, with every button id, with the screens and without; the default handler's answers come from the queue
    for message in MESSAGES:
        for a in ARGS:
            if a not in (0, 1, -1, 0x1E0E8, 0x1E0E9, 0x1E0F0, 0x1E0F1, 0x1E0F2, 0x1E0E7) and message not in (0x100, 0x11):
                continue
            yield ops_case(base_world() + [emitter(0), msg(0, message, a, 5), emitter(9), msg(0, message, a, -5), msg(1, message, a, 0)],
                           q=dict(Default=[0x1234, -7, 0, 0x7FFFFFFF, -0x80000000]))
    yield ops_case(base_world() + [screens(0)] + [msg(0, m, a, 3) for m in (0x14, 0x11, 0x10, 0x100) for a in (0, 1, 0x1E0E7, 0x1E0F2)])
    yield ops_case(base_world() + [msg(0, 0x14), msg(0, 0x14), msg(0, 0x11, 0), msg(0, 0x11, 1), msg(0, 0x11, 0)])
    # the handler of the window itself, a handler of the other kind, no controller global
    yield ops_case(base_world(2) + [msg(0, 0x100, GLOBE), msg(0, 0x100, ENTER), msg(0, 0x14, 0, 0), msg(0, 0x14, 0, 0)],
                   q=dict(Default=[1, 2, 3]))
    yield ops_case(base_world() + [controller(-1), msg(0, 0x14), controller(1), msg(0, 0x14), msg(0, 0x100, PREVIOUS)])
    # Remove and Hide: the controller set and not
    yield ops_case(base_world() + [REMOVE, HIDE, controller(-1), REMOVE, HIDE, controller(1), HIDE, REMOVE])
    # UpdateKeyParticles on windows at the edges of the rectangle range, with the emitter running and not
    for rect in RECTS:
        yield ops_case([make_window(0, -1, 1, 1, rect, 1), key_particles(0, 1), key_particles(0, 1), key_particles(0, 0), key_particles(-1, 0),
                        key_particles(0, 1), emitter(0), key_particles(0, 1), key_particles(0, 0), key_particles(0, 0), emitter(77),
                        key_particles(0, 1), key_particles(0, 0)])
    yield ops_case([make_window(0, -1, 1, 1, (0, 0, 5, 5), 1), key_particles(-1, 0), emitter(3), key_particles(-1, 0), key_particles(-1, 0)])


def controller_rnd(r):
    ops = base_world(r.choice([1, 2, 1, 1]))
    for _ in range(r.randrange(20, 200)):
        k = r.randrange(12)
        if k < 6:
            message = r.choice(MESSAGES + [r.getrandbits(32) - (1 << 31)])
            a = r.choice(ARGS + [r.getrandbits(32) - (1 << 31)])
            ops.append(msg(r.choice([0, 0, 1]), message, a, r.choice([0, 1, -1, r.getrandbits(32) - (1 << 31)])))
        elif k == 6:
            ops.append(r.choice([REMOVE, HIDE]))
        elif k == 7:
            ops.append(key_particles(r.choice([-1, 0, 1]), r.randrange(2)))
        elif k == 8:
            ops.append(emitter(r.choice([0, 0, 1, 7, -1, r.getrandbits(32) - (1 << 31)])))
        elif k == 9:
            ops.append(controller(r.choice([-1, 0, 1])))
        elif k == 10:
            ops.append(screens(r.randrange(2)))
        else:
            ops.append(set_rect(r.choice([0, 1]), r.choice(RECTS)))
    return ops_case(ops, q=dict(Default=[r.randrange(-5, 5) for _ in range(4)], Ticks=[r.getrandbits(32)]))


def window_edges():
    # WhatAmI of a plain window, CalcClientArea at the edges (after the rectangle is stored raw), CallDefaultWindowHandler
    for rect in RECTS:
        yield ops_case([make_window(0, -1, 1, 1, rect), what_am_i(0), set_rect(0, RECTS[1]), calc_client(0), set_rect(0, rect), calc_client(0),
                        calc_client(0), what_am_i(0)])
    yield ops_case([make_window(0, -1, 0x401, 1, (3, 4, 50, 60)), make_window(1, 0, 0x401, 2, (3, 4, 50, 60)), calc_client(1), calc_client(0)])
    for message in (0, 1, -1, 0x11, 0x14, 0x100, 0x7FFFFFFF, -0x80000000):
        yield ops_case([make_window(0, -1, 1, 1, (0, 0, 10, 10), 1), make_window(1, 0, 1, 2, (0, 0, 10, 10), 2),
                        call_default(0, message, 5, 6), call_default(1, message, -1, 0x7FFFFFFF), set_handler(0, 2), call_default(0, message, 1, 2),
                        set_handler(1, 1), call_default(1, message, 3, 4), what_am_i(1)],
                       q=dict(Default=[0, 1, -1, 0x7FFFFFFF, -0x80000000]))
    yield ops_case([make_window(0, -1, 1, 1, (0, 0, 10, 10), 0), call_default(0, 4, 5, 6), set_handler(0, 1), call_default(0, 4, 5, 6)])


def window_rnd(r):
    ops = [make_window(0, -1, r.choice([1, 0x401, 3]), 1, r.choice(RECTS), 1), make_window(1, 0, r.choice([1, 0x401]), 2, r.choice(RECTS), 2)]
    for _ in range(r.randrange(10, 120)):
        k = r.randrange(8)
        slot = r.choice([0, 1])
        if k < 2:
            ops.append(set_rect(slot, r.choice(RECTS)))
        elif k < 4:
            ops.append(calc_client(slot))
        elif k < 6:
            ops.append(call_default(slot, r.choice(MESSAGES), r.choice(ARGS), r.choice(ARGS)))
        elif k == 6:
            ops.append(set_handler(slot, r.choice([1, 2, 1])))
        else:
            ops.append(what_am_i(slot))
    return ops_case(ops, q=dict(Default=[r.randrange(-1000, 1000) for _ in range(5)]))


def help_edges():
    # the constructor over 0xa5 bytes, then every member in every combination of the flags
    yield ops_case([REMAKE_DEFAULT])
    yield ops_case([REMAKE_DEFAULT, make_help(1), make_help(2, 2), make_help(3, 1, True)])
    for cls in (1, 2):
        for enabled in (0, 1, -1, 0x100):
            for wanted in (0, 1, 2, -1, 0x10000):
                for showing in (0, 1, 5):
                    yield ops_case([make_window(0, -1, 1, 1), make_window(1, 0, 1, 2), make_help(1, cls), poke_help(1, 0, enabled),
                                    poke_help(1, 1, wanted), poke_help(1, 2, showing), help_member(1, 8, 0), help_member(1, 8, 1),
                                    help_member(1, 8, 0), help_member(1, 9), help_member(1, 8, 1), help_member(1, 10),
                                    help_member(1, 8, 1), help_member(1, 12, 0), help_member(1, 12, 1), help_member(1, 12, 1),
                                    help_member(1, 8, 1), help_member(1, 12, -1), help_member(1, 3), help_member(1, 2), help_member(1, 3)],
                                   q=dict(Ticks=[0, 0xFFFFFFFF, 0x80000000, 1234]))
    for which in range(15):
        yield ops_case([make_window(0, -1, 1, 1), make_help(1), make_help(2, 2), help_member(0, which, 5, -6), help_member(1, which, -32768, 32767),
                        help_member(2, which, 0x7FFF, 0), help_member(1, which, 0, 0)])
    # a help text made first with the base class and then with the recording class, the destructor with flags of both signs
    for flag in (-1, 0, 1, 2, 32767, -32768, 0x10000, 0x10001, 0xFFFF):
        yield ops_case([make_help(1, 1, True), make_help(2, 2, True), make_help(3, 1), destroy_help(1, flag), destroy_help(2, flag),
                        destroy_help(3, flag), make_help(1, 2, True), destroy_help(1, 0)])
    # UI_SetHelpTextHandler: none (the default), the default, the objects, and after an object is destroyed
    yield ops_case([make_help(1), make_help(2, 2, True), set_help_handler(1), set_help_handler(-1), set_help_handler(2), set_help_handler(0),
                    set_help_handler(1), destroy_help(1, 0), set_help_handler(-1), set_help_handler(2), destroy_help(2, 1), set_help_handler(0)])
    # timers: the clock's answers
    yield ops_case([make_timer(0), make_timer(1), make_timer(0)], q=dict(Ticks=[0, 1, 0xFFFFFFFF, 0x80000000, 77]))
    yield ops_case([make_timer(0)], q=dict(Ticks=[0xFFFFFFFE]))


def help_rnd(r):
    ops = [make_window(0, -1, 1, 1), make_window(1, 0, 1, 2)]
    for _ in range(r.randrange(10, 200)):
        k = r.randrange(16)
        slot = r.choice([0, 1, 2, 3])
        if k < 7:
            ops.append(help_member(slot, r.randrange(15), r.choice([-1, 0, 1, 5, 32767, -32768, 0, 1]), r.choice([-1, 0, 1, 6, 32767])))
        elif k == 7:
            ops.append(help_member(slot, r.choice([8, 12]), r.choice([-1, 0, 1])))
        elif k < 10:
            ops.append(make_help(slot, r.choice([1, 2]), r.randrange(2) == 0))
        elif k == 10:
            ops.append(destroy_help(slot, r.choice([-1, 0, 1, 2, 0x10000, -32768])))
        elif k == 11:
            ops.append(set_help_handler(r.choice([-1, 0, 1, 2, 3])))
        elif k == 12:
            ops.append(poke_help(slot, r.randrange(3), r.choice([0, 1, 2, -1, 0x7FFF, 0x10000])))
        elif k == 13:
            ops.append(make_timer(r.randrange(2)))
        elif k == 14:
            ops.append(r.choice([REMAKE_DEFAULT, REMAKE_DEFAULT, help_member(0, 3)]))
        else:
            ops.append(help_member(slot, 9))
    return ops_case(ops, q=dict(Ticks=[r.getrandbits(32) for _ in range(3)]))


def mixed_rnd(r):
    """Operations of all groups on one world, 200 of them."""
    ops = base_world(r.choice([1, 2])) + [make_help(1), make_help(2, 2)]
    for _ in range(200):
        group = r.randrange(4)
        if group == 0:
            ops += controller_rnd(r)["ops"][4:9]
        elif group == 1:
            ops += window_rnd(r)["ops"][2:5]
        elif group == 2:
            ops += help_rnd(r)["ops"][2:6]
        else:
            ops.append(r.choice([msg(0, 0x100, r.choice(BUTTON_IDS)), key_particles(0, r.randrange(2)), calc_client(r.choice([0, 1])),
                                 REMOVE, HIDE, make_timer(r.randrange(2)), REMAKE_DEFAULT]))
        if len(ops) >= 200:
            break
    return ops_case(ops[:200], q=dict(Default=[r.randrange(-9, 9) for _ in range(4)], Ticks=[r.getrandbits(32), 5]))


# ======================================================================= the module

RUNNER = None
STUBS = None


def setup(runner):
    global RUNNER, STUBS
    RUNNER = runner
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)
    open_host(runner.lib)
    runner.shim("unit_Lobby")
    runner.shim("unit_IslandLobby")


def edges_of(gen):
    return lambda: list(gen())


TESTS = [
    FnTest("the lobby's three window scripts, word for word", tables_ppc, tables_host, edge=tables_edges, rnd=tables_rnd),
    FnTest("Build on the real and made-up scripts: the window trees", tree_ppc, tree_host, edge=tree_edges, rnd=tree_rnd),
    FnTest("the controller's message function, Remove, Hide and UpdateKeyParticles", ops_ppc, ops_host, edge=controller_edges, rnd=controller_rnd),
    FnTest("WhatAmI, CalcClientArea and CallDefaultWindowHandler of windows", ops_ppc, ops_host, edge=window_edges, rnd=window_rnd),
    FnTest("the help text handler, UI_SetHelpTextHandler and the timer's constructor", ops_ppc, ops_host, edge=help_edges, rnd=help_rnd),
    FnTest("operations of all groups together", ops_ppc, ops_host, edge=lambda: [], rnd=mixed_rnd),
]
