"""Differential tests for unit w39, the login panel, against libtpw_unit_LoginPanel.dylib (tests/unit_LoginPanel.cpp):

  src/ui_loginpanel.cpp        CreateLoginPanel, FillLoginNames, UI_TITLE_COLOUR, UIMain_GetString
  src/InterfaceWindow_font.cpp InterfaceWindow::SetFont, SetFontJust, SetFontColour, setExtraParam, SetClientDrawText and the virtual
                               SetText
  src/keytables_disable.cpp    DisableAllKeytables

Real on both sides, so that CreateLoginPanel builds the panel the way the game does: the login panel's script (the PowerPC side reads
the image's words, the host its array), the script interpreter, the control factory and constructors, the window constructor, WhatAmI,
CalcClientArea, GetWindow, CaptureKb, SetClientDrawObject and the rectangles. Fakes that record the same event on both sides: Show, the
message senders, the pointer, the draw object library and the draw objects (Change records the part of the change its flags select,
and Show), the title text object's constructor, the font table, the user information (its constructor and the three members the panel
asks), the advisor's Add, the islands' ResetIslandLoopingSounds, the string table's GetConstString, the converter's ConvertString, the
timer table, the string heap, the assertion and the allocator.

A case is a script of operations on a world: a root window (the interface's), the lobby's window under it, four windows of the tests
with no parent, the lobby and its screens, 13 fonts, eight fake draw objects, the string table, seven key tables of 16 entries, and the
panel's globals. After EVERY operation the answer and the whole state are compared: the panel's globals, every window of the root's tree
and the four test windows (kind by WhatAmI, id, attributes, the three rectangles, z, pivot, help text numbers, region, draw objects, the
handlers, the font, the 16 extra parameters, the button's own words), every draw object's state (what the setters told it: font,
justification, colour, text), the draw object library, all 16 entries of every key table, the converted-text buffer, the heap traffic
and the length of the call log; the whole call log at the end. The PowerPC side decides which operations the state allows (one that
needs a window that was not made is skipped, on both sides).

Five tests: (1) CreateLoginPanel and FillLoginNames on worlds of 0 to 4 saved players, strings present and missing, converter loaded
and not, names of every length, easy-mode answers that do not fit a short, the user information absent, given and vanishing while the
panel is filled, the script cut to nothing; (2) the window setters on windows with and without a draw object and a client draw object;
(3) UIMain_GetString; (4) DisableAllKeytables on key tables of every count; (5) 200 operations of all kinds at random.
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
SHIM_LIBRARY = "unit_LoginPanel"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(create=0x101AE9E0, fill=0x101AE39C, disable=0x1015108C, set_font=0x101DA118, font_colour=0x101DA3AC, font_just=0x101DA204,
          extra=0x101DCFD0, get_string=0x10180468, title_colour=0x1018BD3C, client_text=0x101DE130, set_text=0x101DE248)
OWN = list(FN.values())
# what they call that runs for real: the window constructor, the interpreter, the factory, the constructors, the setters, the shapes,
# GetWindow, the rectangles and the regions' base, and the WhatAmI members
REAL = [0x101D98D8, 0x101DAB88, 0x101DEB78, 0x101DEC14, 0x101DB474, 0x101DB624, 0x101DBD9C, 0x101DCBA8, 0x101DCCEC, 0x101D0260,
        0x101D11B0, 0x101D47F0, 0x101D2488, 0x101D2584, 0x101D5BB0, 0x101D5CE4, 0x101D5E2C, 0x101DC750, 0x101DBE40,
        0x101CADC0, 0x101D0C50, 0x101CD4C4, 0x101CFE3C, 0x101D15CC, 0x101CBF90, 0x101C86C4, 0x101D7BF0, 0x101D008C, 0x101D8C94,
        0x101D08F0, 0x101CD468, 0x101D0B98, 0x101D154C,
        0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788, 0x101C7A1C,
        0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74,
        0x101D9EB0, 0x101CB010, 0x101D0E78, 0x101CD7A8, 0x101CFF50, 0x101D1918, 0x101CC0C8, 0x101C8848, 0x101D7D74, 0x101D0228,
        0x101D8E0C, 0x101D0A14, 0x101D987C]
WINDOW_CTOR = 0x101D98D8
# the fakes (in-binary functions)
POST_MESSAGE, UPDATE_POINTER_IN, ADD_TIMER, RELEASE_STRING = 0x101C9400, 0x101C8F18, 0x101D878C, 0x101D7B10
WINDOW_SHOW, FIND_DRAW = 0x101DC1EC, 0x101CD3B0
GET_FONT, TITLE_CTOR = 0x10180270, 0x1018FBE8
USER_CTOR, USER_NUM, USER_NAME, USER_EASY = 0x1017DE54, 0x1017E788, 0x1017E9D8, 0x1017EA74
ADVISOR_ADD, RESET_LOOPING = 0x100A7074, 0x100B2228

# ---- the original's data
G = dict(user=0x10380B34, lobby=0x102E4888, screens=0x102E49B8, root=0x10395C24, keyboard=0x10395C10, position=0x10395C5C,
         last=0x10395C18, panel=0x10394ABC, first=0x10394AB8, text=0x1037FF18, buffer=0x10380C48, script=0x102ABBB0, hebrew=0x10395B88,
         redraw=0x10395C28)
KEY_TABLE_GLOBALS = [0x1037F234, 0x1037F230, 0x1037F22C, 0x1037F228, 0x1037F224, 0x1037F220, 0x1037F21C]
# the window handlers (TVectors) by the numbers the C++ file gives
HANDLER_CODE = {0x10268390: 3, 0x10268480: 4, 0x10268110: 5, 0x10268128: 6, 0x102681C0: 7, 0x10268298: 8, 0x102682D0: 9,
                0x102682F0: 10, 0x102683F8: 11, 0x10268440: 12, 0x10267CE0: 20, 0x10267CD0: 21, 0x10267CC0: 22}

# ---- labels (the C++ file's)
LAB_ROOT, LAB_LOBBY_WINDOW, LAB_TEST = 10, 11, 12
LAB_LOBBY, LAB_SCREENS, LAB_THEME, LAB_ADVISOR, LAB_USER = 16, 17, 18, 19, 20
LAB_TABLE, LAB_CONVERTER, LAB_BUFFER = 21, 22, 23
LAB_FONT, LAB_POOL = 30, 50
TEST_WINDOWS, FONTS, POOL_DRAWS, TABLES, ENTRIES, NAMES = 4, 13, 8, 7, 16, 4
BUFFER_WORDS = 64
MAX_QUEUE = 64
# the panel's window ids, in the C++ file's table (window numbers 100 + n)
PANEL_IDS = [0x7A13, 0x7A14, 0x7A15, 0x7A16, 0x7A17, 0x7A18, 0x7A19, 0x7A1A, 0x7A1B, 0x7A1C]
STATES = [0, 1, -1, 0x7FFF, 0x1234]

# the texts the operations hand out (wide) and the string table's (multibyte)
TEXTS = ["", "Anna", "Bob", "Fifteen chars!!", "N" * 40, "Zo\u00eb", "x", "Theodora Pinkus", "a" * 100, "\u0416\u0443", "  ", "0",
         "Player 1", "Sixteen chars!!!", "Q", "Quit now"]
MULTIBYTE = [b"", b"Quit", b"Empty slot", b"M" * 63, b"\xe9\xe8x", b"Click here", b"1", b"Another one"]


def s16(v):
    return T.s16(v & 0xFFFF)


def s32(v):
    return T.s32(v & MINUS)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def w16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xFFFF))


def pack(words):
    """A state as bytes (eight a word, so that a difference is reported as byte offset / 8): one comparison instead of thousands."""
    return struct.pack("<%dq" % len(words), *words)


def wide(text):
    return b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0"


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the dumps and the operations."""

    ADDRESSES = [(G["user"], 4), (G["lobby"], 4), (G["screens"], 4), (G["root"], 4), (G["keyboard"], 4), (G["position"], 4),
                 (G["last"], 4), (G["panel"], 4), (G["first"], 4), (G["text"] + 0x54C, 0x20), (G["buffer"], 2 * BUFFER_WORDS),
                 (G["script"], 2), (G["hebrew"], 1), (G["redraw"], 4)] + [(a, 4) for a in KEY_TABLE_GLOBALS]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.heap = [0, 0, 0, 0]          # blocks handled by operator new, new[], delete, delete[] while an operation runs
        self.observe = False              # true while a function under test runs: new blocks are filled and named
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog({}, queues)
        L = self.L
        self.draw_of = {}                 # hash -> block of the library's fake draw object
        self.hash_of = {}                 # block -> hash
        self.library = []                 # the library's blocks in the order they were made
        self.kinds = {}                   # window address -> what its WhatAmI answered
        self.state = {}                   # label -> the draw state; self.order: the labels in the order they were named
        self.order = []
        self.texts = {}                   # index -> address of the wide text
        for k, text in enumerate(TEXTS):
            self.texts[k] = e.alloc(len(wide(text)), wide(text), alignment=4)
        self.strings = {}                 # string number -> address of its multibyte text
        self.names = [0] * NAMES          # the address of the name object the user information answers for a slot
        self.easy = [0] * NAMES
        self.table_block = e.alloc(0x40, bytes(0x40))
        self.converter_block = e.alloc(0x40, bytes(0x40))
        for address, label in ((self.table_block, LAB_TABLE), (self.converter_block, LAB_CONVERTER), (G["buffer"], LAB_BUFFER)):
            L.labels[address] = label
        # the globals as the Mac's data has them
        w32(e, G["user"], 0)
        for key in ("keyboard", "position", "last", "panel", "first", "redraw"):
            w32(e, G[key], 0)
        e.write(G["hebrew"], bytes(1))
        for k in range(BUFFER_WORDS):
            w16(e, G["buffer"] + 2 * k, ord("#"))
        base = G["text"] + 0x54C
        for i in range(0x20 // 4):
            w32(e, base + 4 * i, 0)
        w32(e, G["text"] + 0x54C, self.converter_block)
        w32(e, G["text"] + 0x550, self.converter_block)
        w32(e, G["text"] + 0x564, self.table_block)
        self.script_word = e.read(G["script"], 2)
        # the fonts, the pool of draw objects and the key tables
        self.fonts = []
        for i in range(FONTS):
            block = e.alloc(0x10, bytes(0x10))
            w32(e, block + 0xC, i)
            L.labels[block] = LAB_FONT + i
            self.fonts.append(block)
        self.pool = []
        for i in range(POOL_DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.lp_draw_vtable)
            L.labels[block] = LAB_POOL + i
            self.name_draw(block)
            self.pool.append(block)
        self.entry_blocks, self.table_objects = [], []
        for t in range(TABLES):
            entries = e.alloc(ENTRIES * 0x14, bytes(ENTRIES * 0x14))
            table = e.alloc(0x18, bytes(0x18))
            self.entry_blocks.append(entries)
            self.table_objects.append(table)
            w32(e, KEY_TABLE_GLOBALS[t], table)
            self.fill_table(t, 3, 1, 10 * t + 1)
        # the world's windows (the same order as the host's: the root, the lobby's window, the four test windows)
        self.windows = []
        for slot in range(2 + TEST_WINDOWS):
            self.windows.append(e.alloc(0x300, b"\xa5" * 0x300))
        L.labels[self.windows[0]] = LAB_ROOT
        L.labels[self.windows[1]] = LAB_LOBBY_WINDOW
        self.call(WINDOW_CTOR, self.windows[0], 0, 1, 0x1000, 0, 0, 2048, 1536)
        self.call(WINDOW_CTOR, self.windows[1], self.windows[0], 1, 0xBF431, 0, 0, 2048, 1536)
        w32(e, self.windows[0] + 0x58, self.windows[1])   # (the constructor does not link the window to its parent)
        for i in range(TEST_WINDOWS):
            L.labels[self.windows[2 + i]] = LAB_TEST + i
            self.call(WINDOW_CTOR, self.windows[2 + i], 0, 1, 0x2001 + i, 10 * i, 20 * i, 100 + 10 * i, 200 + 20 * i)
        w32(e, G["root"], self.windows[0])
        lobby = e.alloc(0x38, bytes(0x38))
        w32(e, lobby + 4, self.windows[1])
        w32(e, G["lobby"], lobby)
        theme = e.alloc(0x20, bytes(0x20))
        advisor = e.alloc(0x30, bytes(0x30))
        screens = e.alloc(0x20, bytes(0x20))
        w32(e, screens + 4, theme)
        w32(e, screens + 0x14, advisor)
        w32(e, G["screens"], screens)
        self.user_block = e.alloc(0xD4, bytes(0xD4))
        for address, label in ((lobby, LAB_LOBBY), (screens, LAB_SCREENS), (theme, LAB_THEME), (advisor, LAB_ADVISOR),
                               (self.user_block, LAB_USER)):
            L.labels[address] = label

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def name_draw(self, block):
        """Gives a draw object a state (the host's StateOf): it is dumped in the order the objects were named."""
        label = self.L.lab(block)
        if label > 0 and label not in self.state:
            self.state[label] = dict(shown=-1, changes=0, font=0, just=[0, 0], colour=[0, 0, 0, 0], has_text=0, text="")
            self.order.append(label)

    def rect_code(self, address):
        for offset, code in ((4, 1), (0x10, 2), (0x1C, 3), (0x28, 4), (0x34, 5)):
            base = address - offset
            if self.L.lab(base) > 0:
                return 10 * self.L.lab(base) + code
        return -1

    def what_am_i(self, w):
        """What the window's WhatAmI answers (a window never changes its class, so the answer is kept)."""
        kind = self.kinds.get(w)
        if kind is None:
            e = self.e
            tvector = e.u32(e.u32(w + 0x118) + 0x14)
            kind = self.kinds[w] = T.s32(self.call(e.u32(tvector), w))
        return kind

    def callback_code(self, tvector):
        return 0 if tvector == 0 else HANDLER_CODE.get(tvector, 99)

    def find(self, window, ident):
        """The first window with that id, depth first from `window` and across its siblings (the C++ file's Find)."""
        e = self.e
        while window:
            if T.s32(e.u32(window + 0x88)) == ident:
                return window
            found = self.find(e.u32(window + 0x58), ident)
            if found:
                return found
            window = e.u32(window + 0x50)
        return 0

    def window(self, spec):
        if 0 <= spec < TEST_WINDOWS:
            return self.windows[2 + spec]
        if spec == 4:
            return self.windows[1]
        if spec == 5:
            return self.windows[0]
        if 100 <= spec < 100 + len(PANEL_IDS):
            return self.find(self.windows[0], PANEL_IDS[spec - 100])
        return 0

    def text(self, index):
        return self.texts[index] if 0 <= index < len(TEXTS) else 0

    def fill_table(self, t, count, enabled, seed):
        e = self.e
        entries, table = self.entry_blocks[t], self.table_objects[t]
        for i in range(ENTRIES):
            at = entries + 0x14 * i
            e.write(at, struct.pack(">hhhh", s16(seed + i), s16(seed * 3 + i), s16(seed & 0xFF), STATES[(seed + i) % 5]))
            e.write(at + 8, bytes((seed + i + k) & 0xFF for k in range(8)))
            w32(e, at + 0x10, 0x1000 + i if i % 3 == 0 else 0)
        w32(e, table, entries)
        e.write(table + 4, struct.pack(">hh", s16(count), s16(enabled)))

    # ---- the dumps (the orders are the C++ file's)
    def dump_rect(self, d, offset):
        return list(struct.unpack(">4h", d[offset:offset + 8]))

    def dump_window(self, w, depth):
        e, L = self.e, self.L
        kind = self.what_am_i(w)
        d = bytes(e.read(w, 0x180))
        h = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        i = lambda o: T.s32(struct.unpack(">I", d[o:o + 4])[0])   # noqa: E731
        u = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [depth, L.lab(w), kind, i(0x88), i(0x44)]
        out += self.dump_rect(d, 8) + self.dump_rect(d, 0x2C) + self.dump_rect(d, 0x14)
        out += [h(0xDC), h(0xDE), h(0xE0), h(0xE2), i(0x8C), i(0x90)]
        region = u(0x40)
        if region == 0:
            out += [0] * 5
        elif region == w + 4:
            out += [1, 0, 0, 0, 0]
        else:
            out += [2, 0, 0, 0, 0]
        out += [L.lab(u(0x94)), L.lab(u(0x98)), self.callback_code(u(0x110)), self.callback_code(u(0x114)), h(0x4C), L.lab(u(0x5C)),
                L.lab(u(0x48))]
        out += [i(0x9C + 4 * k) for k in range(16)]
        if kind == 2:
            out += [T.s8(d[0x138]), T.s8(d[0x139]), i(0x13C), i(0x140), int(u(0x148) != 0), u(0x14C), u(0x154), T.s8(d[0x158]),
                    T.s8(d[0x159]), d[0x15A]]
        return out

    def dump_tree(self, w, depth):
        out = []
        while w:
            out += self.dump_window(w, depth)
            out += self.dump_tree(self.e.u32(w + 0x58), depth + 1)
            w = self.e.u32(w + 0x50)
        return out

    def dump_state(self):
        e, L = self.e, self.L
        out = [L.lab(e.u32(G["panel"])), s32(e.u32(G["first"])), L.lab(e.u32(G["user"])), L.lab(e.u32(G["keyboard"])),
               s32(e.u32(G["position"])), L.lab(e.u32(G["last"]))]
        tree = self.dump_tree(self.windows[0], 0)
        out.append(len(tree))
        out += tree
        for i in range(TEST_WINDOWS):
            out += self.dump_window(self.windows[2 + i], 0)
        out.append(len(self.order))
        for label in self.order:
            s = self.state[label]
            text = s["text"]
            out += [label, s["shown"], s["changes"], s["font"], s["just"][0], s["just"][1]] + s["colour"] + [s["has_text"], len(text)]
            out += [ord(c) for c in text]
        out.append(len(self.library))
        for block in self.library:
            out += [self.hash_of[block], struct.unpack(">h", e.read(block + 0xC, 2))[0], struct.unpack(">h", e.read(block + 0xE, 2))[0],
                    struct.unpack(">h", e.read(block + 4, 2))[0]]
        for block in self.pool:
            out += [struct.unpack(">h", e.read(block + 0xC, 2))[0], struct.unpack(">h", e.read(block + 0xE, 2))[0],
                    struct.unpack(">h", e.read(block + 4, 2))[0]]
        for t in range(TABLES):
            table = e.u32(KEY_TABLE_GLOBALS[t])
            out.append(int(table == self.table_objects[t]))
            out += [struct.unpack(">h", e.read(table + 6, 2))[0], struct.unpack(">h", e.read(table + 4, 2))[0]]
            entries = e.u32(table)
            for k in range(ENTRIES):
                at = entries + 0x14 * k
                out += list(struct.unpack(">hhhh", e.read(at, 8))) + list(e.read(at + 8, 8)) + [int(e.u32(at + 0x10) != 0)]
        out += list(struct.unpack(">%dH" % BUFFER_WORDS, e.read(G["buffer"], 2 * BUFFER_WORDS)))
        out += list(self.heap)
        out.append(len(L.log))
        return out

    def op(self, code, a):
        """Runs one operation. Returns the answer (four words) or None when the state does not allow it."""
        e, L = self.e, self.L
        answer = [0, 0, 0, 0]
        # the operations that only set the world up
        if code == 20:   # a player's name: slot, text (-1 none)
            slot = a[0]
            if not 0 <= slot < NAMES:
                return None
            if 0 <= a[1] < len(TEXTS):
                length = len(TEXTS[a[1]])
                obj = e.alloc(0x20, bytes(0x20))
                w32(e, obj + 4, length)
                w32(e, obj + 8, self.texts[a[1]])
                self.names[slot] = obj
            else:
                self.names[slot] = 0
            return answer
        if code == 21:
            if not 0 <= a[0] < NAMES:
                return None
            self.easy[a[0]] = a[1]
            return answer
        if code == 22:
            w32(e, G["user"], self.user_block if a[0] else 0)
            return answer
        if code == 23:   # a string of the table: number, multibyte text (-1 none)
            ident = a[0] & MINUS
            if 0 <= a[1] < len(MULTIBYTE):
                data = MULTIBYTE[a[1]]
                self.strings[ident] = e.alloc(len(data) + 1, data + b"\0")
            else:
                self.strings.pop(ident, None)
            return answer
        if code == 24:
            w32(e, G["text"] + 0x550, self.converter_block if a[0] else 0)
            return answer
        if code == 25:
            if not 0 <= a[0] < TABLES:
                return None
            self.fill_table(a[0], a[1], a[2], a[3])
            return answer
        if code == 26:
            w32(e, self.fonts[a[0]] + 0xC, a[1])
            return answer
        if code == 28:
            w16(e, G["script"], a[0])
            return answer
        if code == 30:
            for k in range(BUFFER_WORDS):
                w16(e, G["buffer"] + 2 * k, a[0] + k)
            return answer
        if code == 29:   # the panel's global: -1 none, else a window number
            if a[0] == -1:
                w32(e, G["panel"], 0)
            else:
                target = self.window(a[0])
                if target == 0:
                    return None
                w32(e, G["panel"], target)
            return answer
        w = self.window(a[0]) if code in (3, 4, 5, 6, 7, 8, 9, 27) else 0
        if code in (3, 4, 5, 6, 7, 8, 9, 27) and w == 0:
            return None
        if code == 9 and self.what_am_i(w) != 1:
            return None
        if code == 27:   # a window's draw objects (pool numbers, -1 none): a raw store
            w32(e, w + 0x94, self.pool[a[1]] if 0 <= a[1] < POOL_DRAWS else 0)
            w32(e, w + 0x98, self.pool[a[2]] if 0 <= a[2] < POOL_DRAWS else 0)
            return answer
        self.observe = True
        try:
            if code == 1:
                self.call(FN["create"])
            elif code == 2:
                self.call(FN["fill"])
            elif code == 3:
                self.call(FN["title_colour"], w)
            elif code == 4:
                self.call(FN["set_font"], w, self.fonts[a[1]] if 0 <= a[1] < FONTS else 0)
            elif code == 5:
                self.call(FN["font_just"], w, a[1], a[2])
            elif code == 6:
                self.call(FN["font_colour"], w, a[1], a[2], a[3], a[4])
            elif code == 7:   # setExtraParam: the draw objects are put back after the call (a negative index overwrites them)
                before = (e.u32(w + 0x94), e.u32(w + 0x98))
                answer[0] = s32(self.call(FN["extra"], w, a[1] & 0xFFFF, a[2]))
                after = (e.u32(w + 0x94), e.u32(w + 0x98))
                answer[1] = 0 if after[0] == before[0] else s32(after[0])
                answer[2] = 0 if after[1] == before[1] else s32(after[1])
                w32(e, w + 0x94, before[0])
                w32(e, w + 0x98, before[1])
            elif code == 8:
                self.call(FN["client_text"], w, self.text(a[1]))
            elif code == 9:
                tvector = e.u32(e.u32(w + 0x118) + 0x2C)
                answer[0] = s32(self.call(e.u32(tvector), w, self.text(a[1])))
            elif code == 10:
                result = self.call(FN["get_string"], a[0])
                answer[0] = 0 if result == 0 else 1 if result == G["buffer"] else 2
            elif code == 11:
                self.call(FN["disable"])
            else:
                raise ValueError(code)
        finally:
            self.observe = False
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects' vtable, in persistent memory."""
    if hasattr(e, "lp_ready"):
        return

    def destroyed(emu):
        L, this, flag = emu.cl, emu.r(3), T.s16(emu.r(4))
        L.rec("Draw.dtor", (L.lab(this),))
        if flag > 0:
            emu.us.heap[2] += 1
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
            L.forget(this)

    def show(emu):
        L, this, shown = emu.cl, emu.r(3), T.s16(emu.r(4))
        L.rec("Draw.Show", (L.lab(this), shown))
        us = emu.us
        label = L.lab(this)
        if label in us.state:
            us.state[label]["shown"] = shown

    def change(emu):
        L, us, this, flags, record = emu.cl, emu.us, emu.r(3), emu.r(4), emu.r(5)
        label = L.lab(this)
        frame = depth = rotation = pivot_x = pivot_y = 0
        rect_code = left = top = right = bottom = 0
        if flags & 1:
            frame = struct.unpack(">h", emu.read(record + 4, 2))[0]
        if flags & 2:
            depth = struct.unpack(">h", emu.read(record + 6, 2))[0]
        if flags & 4:
            rotation = struct.unpack(">h", emu.read(record + 8, 2))[0]
        if flags & 0x40:
            pivot_x, pivot_y = struct.unpack(">2h", emu.read(record + 0xC, 4))
        if flags & 0x10:
            rect = emu.u32(record + 0x14)
            left, top, right, bottom = struct.unpack(">4h", emu.read(rect + 4, 8))
            rect_code = us.rect_code(rect)
        L.rec("Change", (label, flags, frame, depth, rotation, rect_code, left, top, right, bottom, pivot_x, pivot_y))
        state = us.state.get(label)
        if state is not None:
            state["changes"] += 1
        if not flags & (0x10000 | 0xC00 | 0x40000 | 0x100):
            return
        style = emu.u32(record + 0x10)
        font = emu.read(style + 8, 1)[0] if flags & 0x10000 else 0
        just = list(emu.read(style + 4, 2)) if flags & 0xC00 else [0, 0]
        colour = list(emu.read(style + 0x10, 4)) if flags & 0x40000 else [0, 0, 0, 0]
        text_pointer = emu.u32(style) if flags & 0x100 else 0
        text = ""
        if flags & 0x100:
            text = "(null)" if text_pointer == 0 else CL.wide_text(emu, text_pointer)
        L.rec("Change.style", (label, font, just[0], just[1], colour[0], colour[1], colour[2], colour[3]), (text,))
        if state is None:
            return
        if flags & 0x10000:
            state["font"] = font
        if flags & 0xC00:
            state["just"] = just
        if flags & 0x40000:
            state["colour"] = colour
        if flags & 0x100:
            state["has_text"] = 2 if text_pointer == 0 else 1
            state["text"] = text[:95]

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x08, struct.pack(">I", T.fake_function(e, destroyed)))
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.write(vt + 0x1C, struct.pack(">I", T.fake_function(e, show)))
    e.lp_draw_vtable = vt
    e.lp_ready = True


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(OWN)
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

    # the senders, the pointer, the timer table and the string heap
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(ADD_TIMER, R("AddTimer", "Lii", ret=lambda e, L: 0))
    S.internal(RELEASE_STRING, R("ReleaseString", "L"))

    # Show records, and (the queue "ShowDropsUserInfo") may make the user information vanish at once
    def show(e):
        L = e.cl
        L.rec("Window.Show", (L.lab(e.r(3)), T.s16(e.r(4))))
        if L.ret("ShowDropsUserInfo") != 0:
            w32(e, G["user"], 0)
        e.set_r(3, 1)

    S.internal(WINDOW_SHOW, show)

    # the draw object library: one fake draw object per hash, named when first asked for
    def find_draw(e, L):
        us, h = e.us, e.r(3)
        block = us.draw_of.get(h)
        if block is None:
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.lp_draw_vtable)
            L.newlab(block)
            us.draw_of[h] = block
            us.hash_of[block] = h
            us.library.append(block)
            us.name_draw(block)
        return block

    S.internal(FIND_DRAW, R("FindDrawObject", "u", ret=find_draw))

    # the title text object's constructor: the vtable and the record (the object came from operator new and has its label)
    def title_ctor(e):
        L, us, this, data = e.cl, e.us, e.r(3), e.r(4)
        L.rec("TitleText.ctor", (L.lab(this), int(data != 0)))
        w32(e, this + 0x38, e.lp_draw_vtable)
        us.name_draw(this)
        e.set_r(3, this)

    S.internal(TITLE_CTOR, title_ctor)

    # the font table: a pool of 13 fonts, NULL past it
    def get_font(e, L):
        index = e.r(3)
        return e.us.fonts[index] if index < FONTS else 0

    S.internal(GET_FONT, R("GetFontTableEntry", "u", ret=get_font))

    # the user information: the constructor gives the object a label; the names and the easy-mode answers are the world's
    def user_ctor(e):
        L, this = e.cl, e.r(3)
        L.rec("UserInfo.ctor", (L.lab(this),))
        e.set_r(3, this)

    def num_valid(e, L):
        return sum(1 for name in e.us.names if name)

    def user_name(e, L):
        return e.us.names[e.r(4)] if e.r(4) < NAMES else 0

    def user_easy(e, L):
        return e.us.easy[e.r(4)] if e.r(4) < NAMES else 0

    S.internal(USER_CTOR, user_ctor)
    S.internal(USER_NUM, R("UserInfo.GetNumValidGameLoginSlots", "L", ret=num_valid))
    S.internal(USER_NAME, R("UserInfo.GetGameLoginSlotUserName", "Li", ret=user_name))
    S.internal(USER_EASY, R("UserInfo.IsGameLoginSlotUserEasyMode", "Li", ret=user_easy))

    # the advisor and the islands
    S.internal(ADVISOR_ADD, R("Advisor.Add", "Lii"))
    S.internal(RESET_LOOPING, R("Islands.ResetIslandLoopingSounds", "L"))

    # the string table and the converter
    def const_string(e):
        L = e.cl
        L.rec("GetConstString", (L.lab(e.r(3)), e.r(4)))
        e.set_r(3, e.us.strings.get(e.r(4), 0))

    def convert(e):                     # hidden result in r3, then this, out, source, size
        L, out, source, size = e.cl, e.r(5), e.r(6), e.r(7) & 0xFFFF
        L.rec("ConvertString", (L.lab(e.r(4)), L.lab(out), size), (e.cstr(source),))
        for i in range(size):
            char = e.read(source + i, 1)[0]
            w16(e, out + 2 * i, char)
            if char == 0:
                break

    S.imp("GetConstString__8TbITableCFUl", const_string)
    S.imp("ConvertString__14TbIMBToUnicodeFPwPCUcUs", convert)
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_lp_open", False):
        return
    lib.tpw_lp_reset.argtypes = []
    lib.tpw_lp_world.argtypes = []
    lib.tpw_lp_text.argtypes = [C.c_int32, C.c_void_p, C.c_int32]
    lib.tpw_lp_multibyte.argtypes = [C.c_int32, C.c_void_p, C.c_int32]
    lib.tpw_lp_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_lp_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lp_state.restype = C.c_int32
    lib._lp_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        self.lib = lib
        open_host(lib)
        lib.tpw_lp_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues)
        self.keep = []
        for k, text in enumerate(TEXTS):
            buffer = (C.c_uint16 * (len(text) + 1))(*[ord(c) for c in text], 0)
            self.keep.append(buffer)
            lib.tpw_lp_text(k, C.cast(buffer, C.c_void_p), len(text))
        for k, data in enumerate(MULTIBYTE):
            buffer = C.create_string_buffer(data, len(data) + 1)
            self.keep.append(buffer)
            lib.tpw_lp_multibyte(k, C.cast(buffer, C.c_void_p), len(data))
        lib.tpw_lp_world()

    def op(self, code, a):
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 4)()
        self.lib.tpw_lp_op(code, args, out)
        return list(out)

    def dump_state(self):
        buf = (C.c_int64 * 65536)()
        count = self.lib.tpw_lp_state(buf)
        return list(buf[:count])



# ======================================================================= the cases

def op(code, *args):
    return (code, tuple(args))


CREATE, FILL, DISABLE = op(1), op(2), op(11)


def title_colour(w):
    return op(3, w)


def set_font(w, font):
    return op(4, w, font)


def font_just(w, h, v):
    return op(5, w, h, v)


def font_colour(w, r, g, b, a):
    return op(6, w, r, g, b, a)


def extra_param(w, index, value):
    return op(7, w, index, value)


def client_text(w, text):
    return op(8, w, text)


def set_text(w, text):
    return op(9, w, text)


def get_string(ident):
    return op(10, ident)


def name(slot, text):
    return op(20, slot, text)


def easy(slot, value):
    return op(21, slot, value)


def user_info(present):
    return op(22, present)


def string(ident, text):
    return op(23, ident, text)


def converter(loaded):
    return op(24, loaded)


def table(index, count, enabled, seed):
    return op(25, index, count, enabled, seed)


def font_id(font, ident):
    return op(26, font, ident)


def draws(w, main, client):
    return op(27, w, main, client)


def script_word(value):
    return op(28, value)


def panel(w):
    return op(29, w)


def fill_buffer(seed):
    return op(30, seed)


NO_PANEL, PANEL = -1, 100
STANDARD = [string(8, 1), string(0x1A5, 2)]   # the quit row's text and the empty row's


def case(ops, **extra):
    c = dict(ops=list(ops))
    c.update(extra)
    return c


# ---- the PowerPC side runs the script; the host replays what it ran
RAN = {}
RUNNER = None
STUBS = None


def with_stubs(fn):
    def run(e, c):
        saved = STUBS.activate(e)
        try:
            return fn(e, c)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


@with_stubs
def ops_ppc(e, c):
    world = Ppc(e, c)
    steps, ran = [], []
    try:
        for step in c["ops"]:
            code, args = step
            answer = world.op(code, args)
            if answer is None:
                continue
            ran.append(step)
            steps.append(dict(op=code, answer=answer, state=pack(world.dump_state())))
        RAN[id(c)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def ops_host(lib, c):
    host = Host(lib, c)
    steps = []
    try:
        for code, args in RAN.pop(id(c)):
            answer = host.op(code, args)
            steps.append(dict(op=code, answer=answer, state=pack(host.dump_state())))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


NAMES_SETS = [
    [],
    [(0, 1)],
    [(0, 3)],
    [(1, 2), (3, 7)],
    [(0, 1), (1, 2), (2, 3), (3, 4)],
    [(0, 5), (1, 9), (2, 10), (3, 14)],
    [(2, 12)],
    [(0, 0), (1, 6)],
]


def panel_edges():
    # the panel on worlds of 0 to 4 players, with each strings/converter combination, the user information absent or given
    for names in NAMES_SETS:
        for strings in ([], STANDARD, [string(8, 1)], [string(0x1A5, 2)], [string(8, -1), string(0x1A5, 3)]):
            for loaded in (1, 0):
                for given in (0, 1):
                    ops = [user_info(given), converter(loaded)] + list(strings)
                    ops += [name(s, t) for s, t in names]
                    ops += [easy(s, v) for s, v in ((0, 1), (1, 0), (2, 0x10000), (3, 0x10001))]
                    ops += [CREATE]
                    yield case(ops)
    # easy-mode answers that do not fit a short, and negative ones
    for value in (0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, -0x80000000, 0x7FFFFFFF):
        yield case(STANDARD + [name(0, 1), name(1, 2), name(2, 3), name(3, 4), easy(0, value), easy(1, value), easy(2, value), easy(3, value),
                               CREATE])
    # the panel twice, names changed between, filling alone, the panel dropped and put back
    yield case(STANDARD + [CREATE, name(0, 1), FILL, name(0, -1), name(1, 2), FILL, CREATE, FILL, panel(NO_PANEL), FILL, panel(PANEL), FILL])
    yield case(STANDARD + [FILL, panel(NO_PANEL), FILL, user_info(0), FILL, CREATE])
    # the strings changed after the panel is built
    yield case([CREATE, string(0x1A5, 4), FILL, string(0x1A5, -1), FILL, string(0x1A5, 3), converter(0), FILL, converter(1), FILL])
    # the script cut to nothing: no panel; then restored
    yield case(STANDARD + [script_word(5), CREATE, FILL, script_word(0), CREATE, FILL])
    yield case(STANDARD + [script_word(5), name(1, 2), CREATE, user_info(0), CREATE])
    # the user information vanishing under Show: in the lobby window's Show, then in a row's
    for pattern in ([1], [0, 1], [0, 0, 1], [0, 0, 0, 1], [0, 0, 0, 0, 1], [0, 0, 0, 0, 0, 1], [0, 0, 0, 0, 0, 0, 1], [1, 1, 1], [0, 1, 0, 1, 0, 1, 0, 1]):
        for names in ([], [(0, 1), (1, 2), (2, 3), (3, 4)], [(1, 2)]):
            yield case(STANDARD + [user_info(1)] + [name(s, t) for s, t in names] + [CREATE, FILL, CREATE], q=dict(ShowDropsUserInfo=pattern))
    # the message queue's answers to the windows' constructors and the key press the panel takes
    yield case(STANDARD + [name(0, 1), CREATE], q=dict(PostMessage=[5, 6, 7]))
    # the key tables are disabled by the panel
    yield case(STANDARD + [table(0, 5, 1, 3), table(2, 16, 1, 4), table(4, 7, 1, 5), table(6, 2, 1, 6), CREATE])
    # a window of the tests as the panel's global: the Fill goes to nobody but the panel
    yield case(STANDARD + [CREATE, panel(NO_PANEL), CREATE, panel(PANEL), FILL])


def window_edges():
    # the setters on test windows with no draw object, one of them, both
    combos = [(-1, -1), (0, -1), (-1, 1), (2, 3)]
    for main, client in combos:
        base = [draws(0, main, client)]
        for font in (-1, 0, 5, 12, 13, 5):
            yield case(base + [set_font(0, font), set_font(0, font), set_font(0, -1), set_font(0, font)])
        for ident in (0, 0x7F, 0x80, 0xFF, 0x100, 0x1FF, -1, -0x80, 0x7FFFFFFF, -0x80000000, 0x12345678):
            yield case(base + [font_id(5, ident), set_font(0, 5), set_font(0, 3), set_font(0, 5)])
        for h, v in ((0, 0), (1, 1), (2, 1), (1, 2), (-1, -1), (255, 256), (0x7FFFFFFF, -0x80000000), (3, 0x100), (0x80, 0x7F)):
            yield case(base + [font_just(0, h, v), font_just(0, v, h)])
        for colour in ((0, 0, 0, 0), (255, 255, 255, 255), (0xEA, 0xEF, 0x66, 0xFF), (-1, -1, -1, -1), (256, 257, 258, 259),
                       (0x7FFFFFFF, -0x80000000, 0x80, 0x7F), (-20, -94, 4, 255)):
            yield case(base + [font_colour(0, *colour), title_colour(0)])
        for text in (-1, 0, 1, 3, 4, 5, 8, 9):
            yield case(base + [client_text(0, text), set_text(0, text), set_text(0, -1), client_text(0, -1)])
        for index in (0, 1, 7, 15, 16, 17, 100, 32767, -1, -2, 0x7FFF, 0x10000, 0x10005):
            for value in (0, 1, 0x7FFFFFFF, 0xFFFFFFFF, 0x12345678):
                yield case(base + [extra_param(0, index, value), extra_param(0, 0, 9)])
    # every setter on every test window and the lobby's window, in turn, with the draw objects different on each
    yield case([draws(0, 0, -1), draws(1, -1, 1), draws(2, 2, 3), draws(3, -1, -1)]
               + [op for w in range(4) for op in (set_font(w, 4), font_just(w, 1, 1), font_colour(w, 1, 2, 3, 4), client_text(w, 1), set_text(w, 2))])
    # the draw objects shared by two windows, and set again
    yield case([draws(0, 1, 1), draws(1, 1, 1), set_font(0, 2), set_font(1, 2), font_just(0, 2, 2), draws(0, -1, 4), set_text(0, 6), set_text(1, 7)])
    # SetFont with the font the window has already, after the font table's ids changed
    yield case([draws(0, 0, 1), set_font(0, 4), font_id(4, 99), set_font(0, 4), set_font(0, 5), font_id(5, 99), set_font(0, 5)])
    # the title colour: no theme number, the same colour for every window
    yield case([draws(0, 0, 1), draws(1, -1, 1), draws(2, 0, -1), title_colour(0), title_colour(1), title_colour(2), title_colour(3), title_colour(4),
                title_colour(5)])
    # the lobby's window and the root, with no draw objects of the world's own (they have none)
    yield case([set_font(4, 3), font_just(5, 1, 1), font_colour(4, 1, 2, 3, 4), set_text(4, 1), client_text(5, 2), extra_param(4, 3, 8)])
    # the panel's own windows: the name areas have title text objects, the rows have draw objects
    yield case(STANDARD + [CREATE] + [op for w in range(100, 110) for op in (set_font(w, 7), font_just(w, 0, 2), font_colour(w, 9, 8, 7, 6),
                                                                              client_text(w, 3), extra_param(w, 2, 5))])
    yield case(STANDARD + [CREATE] + [set_text(w, 5) for w in (100, 101, 102, 105, 106, 107, 108, 109)] + [title_colour(102), title_colour(103)])


def string_edges():
    for converter_on in (1, 0):
        for ident in (0, 1, 8, 9, 0x1A5, 0x193, 0x3FF, -1, 0x7FFFFFFF, -0x80000000, 12345):
            yield case([converter(converter_on), string(ident, 1), get_string(ident), string(ident, 3), get_string(ident), string(ident, 0),
                        get_string(ident), string(ident, 4), get_string(ident), string(ident, -1), get_string(ident), get_string(ident + 1)])
    yield case([fill_buffer(0x40), string(8, 1), converter(0), get_string(8), converter(1), get_string(8), get_string(9)])
    yield case([string(1, 1), string(2, 2), string(3, 3), string(4, 4), string(5, 5), string(6, 6), string(7, 7), string(8, 1), string(9, 2),
                string(10, 3), string(11, 4), string(12, 5), string(13, 6)] + [get_string(i) for i in range(0, 15)])


def key_edges():
    counts = (-32768, -1, 0, 1, 2, 5, 15, 16)
    for count in counts:
        for enabled in (0, 1, -1, 0x7FFF):
            yield case([table(t, count, enabled, 3 + t) for t in range(7)] + [DISABLE, DISABLE])
    for t in range(7):
        yield case([table(t, 16, 1, 100 + t), DISABLE])
        yield case([table(u, 0, 1, 7) for u in range(7)] + [table(t, 16, 1, 100 + t), DISABLE])
    for seed in range(0, 60, 7):
        yield case([table(t, (seed + t) % 17, 1 + t % 2, seed * 5 + t) for t in range(7)] + [DISABLE])
    yield case([DISABLE])


def edges():
    for generator in (panel_edges, window_edges, string_edges, key_edges):
        for c in generator():
            yield c


# ---- the random cases


def random_world(r):
    """The strings, the converter and the key tables of a random world."""
    ops = []
    if r.randrange(5):
        ops += [string(8, r.choice([1, 1, 5, -1])), string(0x1A5, r.choice([2, 2, 3, 4, -1]))]
    if r.randrange(4) == 0:
        ops.append(converter(0))
    for t in range(7):
        if r.randrange(2):
            ops.append(table(t, r.choice([-1, 0, 1, 2, 5, 16]), r.choice([0, 1, 1]), r.randrange(100)))
    return ops


def window_spec(r):
    return r.choice([0, 1, 2, 3, 4, 5, 100, 101, 102, 102, 103, 104, 105, 106, 107, 108, 109, 3])


def op_create(r):
    return CREATE


def op_fill(r):
    return FILL


def op_name(r):
    return name(r.randrange(4), r.choice([-1, r.randrange(16)]))


def op_easy(r):
    return easy(r.randrange(4), r.choice([0, 1, 2, 0x10000, 0x10001, -1, r.getrandbits(32) - (1 << 31)]))


def op_user_info(r):
    return user_info(r.randrange(2))


def op_string(r):
    return string(r.choice([8, 0x1A5, 9, r.randrange(20)]), r.choice([-1, r.randrange(8)]))


def op_converter(r):
    return converter(r.randrange(2))


def op_panel(r):
    return panel(r.choice([NO_PANEL, PANEL, NO_PANEL]))


def op_script(r):
    return script_word(r.choice([5, 0, 5]))


def op_buffer(r):
    return fill_buffer(r.randrange(0x4000))


def op_title_colour(r):
    return title_colour(window_spec(r))


def op_set_font(r):
    return set_font(window_spec(r), r.choice([-1, 0, 5, 5, 12, 13, r.randrange(13)]))


def op_font_just(r):
    value = lambda: r.choice([0, 1, 2, -1, 255, 256, r.getrandbits(32) - (1 << 31)])   # noqa: E731
    return font_just(window_spec(r), value(), value())


def op_font_colour(r):
    return font_colour(window_spec(r), *[r.choice([0, 255, 256, -1, r.getrandbits(32) - (1 << 31), r.randrange(256)]) for _ in range(4)])


def op_extra(r):
    return extra_param(window_spec(r), r.choice([0, 1, 2, 15, 16, 17, -1, -2, 100, 32767, r.randrange(16)]),
                       r.choice([0, 1, 2, 3, 0x7FFFFFFF, r.getrandbits(32)]))


def op_client_text(r):
    return client_text(window_spec(r), r.choice([-1, r.randrange(16)]))


def op_set_text(r):
    return set_text(window_spec(r), r.choice([-1, r.randrange(16)]))


def op_get_string(r):
    return get_string(r.choice([8, 0x1A5, 0, 9, -1, r.randrange(20)]))


def op_disable(r):
    return DISABLE


def op_table(r):
    return table(r.randrange(7), r.choice([-32768, -1, 0, 1, 2, 5, 16]), r.choice([0, 1, -1]), r.randrange(1000))


def op_font_id(r):
    return font_id(r.randrange(13), r.choice([0, 1, 5, 0x7F, 0x80, 0xFF, 0x100, -1, r.getrandbits(32) - (1 << 31)]))


def op_draws(r):
    return draws(r.randrange(4), r.choice([-1, -1, r.randrange(8)]), r.choice([-1, -1, r.randrange(8)]))


GROUP_PANEL = [op_create, op_create, op_fill, op_fill, op_name, op_name, op_easy, op_user_info, op_string, op_converter, op_panel, op_script,
               op_buffer, op_set_text, op_client_text, op_disable]
GROUP_WINDOW = [op_title_colour, op_set_font, op_set_font, op_font_just, op_font_colour, op_extra, op_client_text, op_set_text, op_font_id,
                op_draws, op_draws, op_create]
GROUP_STRING = [op_get_string, op_get_string, op_get_string, op_string, op_string, op_converter, op_buffer]
GROUP_KEY = [op_disable, op_table, op_table]
GROUP_ALL = GROUP_PANEL + GROUP_WINDOW + GROUP_STRING + GROUP_KEY


def random_case_of(group, count):
    def make(r):
        ops = random_world(r)
        if group is GROUP_WINDOW and r.randrange(2):
            ops.append(CREATE)
        for _ in range(r.randrange(count // 3, count)):
            ops.append(r.choice(group)(r))
        ops = ops[:200]
        q = dict(ShowDropsUserInfo=[r.choice([0, 0, 0, 0, 1]) for _ in range(8)], PostMessage=[r.choice([0, 0, 3])])
        return case(ops, q=q)
    return make


# ======================================================================= the module


def setup(runner):
    global RUNNER, STUBS
    RUNNER = runner
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)
    open_host(runner.lib)


TESTS = [
    FnTest("CreateLoginPanel and FillLoginNames: the panel on worlds of 0 to 4 players", ops_ppc, ops_host,
           edge=lambda: list(panel_edges()), rnd=random_case_of(GROUP_PANEL, 40)),
    FnTest("the window's font and text setters, setExtraParam, UI_TITLE_COLOUR", ops_ppc, ops_host, edge=lambda: list(window_edges()),
           rnd=random_case_of(GROUP_WINDOW, 40)),
    FnTest("UIMain_GetString", ops_ppc, ops_host, edge=lambda: list(string_edges()), rnd=random_case_of(GROUP_STRING, 40)),
    FnTest("DisableAllKeytables", ops_ppc, ops_host, edge=lambda: list(key_edges()), rnd=random_case_of(GROUP_KEY, 30)),
    FnTest("operations of all groups together", ops_ppc, ops_host, edge=lambda: [], rnd=random_case_of(GROUP_ALL, 200)),
]
