"""Unit w43: login handlers, removal, game login, dialog construction and their leaves.

The panel and dialog are built by the real script interpreter, factory and window constructors.
Real callees include the font table, user and master-save constructors, font setters, window
visibility and message dispatch, draw-object lookup, game type, pause, and island reset/sign
updates. The disk, name-entry, dialog shell, advisor and update-UI boundaries are recording
fakes. String-library import fakes preserve capacity and allocation/free counts.

Each case is an operation script. Every step compares its answer, window tree, draw states,
key tables, globals, user/master fields, island selection, heap traffic and call-log length;
the entire call log is compared afterward. Edge cases and 200 random cases cover each group,
including 200-operation handler scripts. Invalid pointer dereferences are skipped on both sides.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import ppc_const as P
from unicorn import UC_HOOK_CODE

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LoginMsg"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(create=0x101AE9E0, fill=0x101AE39C, disable=0x1015108C, set_font=0x101DA118, font_colour=0x101DA3AC, font_just=0x101DA204,
          extra=0x101DCFD0, get_string=0x10180468, title_colour=0x1018BD3C, client_text=0x101DE130, set_text=0x101DE248)
FN.update(enable_all=0x10151010, enable=0x101500b8, disable_one=0x101500ec, get_extra=0x101dd030, wrap=0x101da2d8,
          give_key=0x1016a4b4, save=0x1017fa50, is_easy=0x1016a4fc, login=0x1017f330, dialog=0x101f1800,
          panel_msg=0x101add48, entry_msg=0x101ade94, quit_msg=0x101ae218, remove=0x101af02c,
          disabled=0x101823c8, ui_disable=0x10182364, pause_timer=0x101d8b64)
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
REAL += [0x101dbafc, 0x1016f5d8, 0x1016f6a0, 0x10149904]
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
         redraw=0x10395C28, master=0x10380a8c, game_type=0x102b3aa0, type_guard=0x102b3aa4, game_options=0x102b2b48,
         key_disabled=0x1038cc54, timer_paused=0x10395c4c, game=0x1037f134, settings=0x1038071c)
KEY_TABLE_GLOBALS = [0x1037F234, 0x1037F230, 0x1037F22C, 0x1037F228, 0x1037F224, 0x1037F220, 0x1037F21C]
# the window handlers (TVectors) by the numbers the C++ file gives
HANDLER_CODE = {0x10268390: 3, 0x10268480: 4, 0x10268110: 5, 0x10268128: 6, 0x102681C0: 7, 0x10268298: 8, 0x102682D0: 9,
                0x102682F0: 10, 0x102683F8: 11, 0x10268440: 12, 0x10267CE0: 20, 0x10267CD0: 21, 0x10267CC0: 22, 0x102685d0: 23}

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
        addresses = self.ADDRESSES + [(0x10395c30, 4), (0x102e48f8, 20), (0x10395c04, 4), (0x10395c08, 4), (0x10395bf8, 4), (0x102af928, 4), (G["text"] + 4, 4), (G["text"] + 0x1c, 52 * 0x18), (G["master"], 4), (G["game_type"], 8), (G["game_options"], 4),
            (G["key_disabled"], 4), (G["timer_paused"], 4), (G["game"], 0x40), (G["settings"] + 0x36, 1),
            (0x10380b38, 20), (0x103b74c8 + 0x44c0, 20)]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in addresses}
        self.heap = [0, 0, 0, 0]          # blocks handled by operator new, new[], delete, delete[] while an operation runs
        self.observe = False              # true while a function under test runs: new blocks are filled and named
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog({}, queues)
        L = self.L
        self.draw_of = {}                 # hash -> block of the library's fake draw object
        self.hash_of = {}                 # block -> hash
        self.library = []                 # the library's blocks in the order they were made
        self.cached_windows = {}
        self.cached_tables = {}
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
        w32(e, G["text"] + 4, 0)
        # the fonts, the pool of draw objects and the key tables
        self.fonts = []
        for i in range(FONTS):
            block = G["text"] + 0x1c + 0x18 * i
            e.write(block, bytes(0x18))
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
        self.seed_hash = {}
        hashes = set()
        for address, count in ((0x102abbb0, 371), (0x102b1380, 77)):
            values = struct.unpack(">%dH" % count, e.read(address, count * 2))
            for i in range(count - 1):
                hashes.add(values[i] | values[i+1] << 16)
        for text in ("b_okay", "b_exit"):
            h = 0
            for char in text:
                h = ((h ^ ord(char)) * 47) & MINUS
            hashes.add(h)
        head = 0
        for h in sorted(hashes):
            draw = e.alloc(0x3c, bytes(0x3c))
            w32(e, draw + 0x38, e.lp_draw_vtable)
            w16(e, draw + 0xc, 1)
            self.seed_hash[draw] = h
            node = e.alloc(24, bytes(24))
            w32(e, node, draw)
            w32(e, node + 0x10, h)
            w32(e, node + 0x14, head)
            head = node
        w32(e, 0x10395c30, head)
        self.set_string(0x102e48f8, "selected", False, fresh=True, observe=False)
        # the world's windows (the same order as the host's: the root, the lobby's window, the four test windows)
        queue = e.alloc(24, bytes(24))
        L.labels[queue] = 26
        w32(e, 0x10395c04, queue)
        w32(e, 0x10395c08, queue)
        w32(e, 0x10395bf8, 0)
        help_obj = e.alloc(24, bytes(24))
        w32(e, help_obj + 0x10, 0x102afa88)
        w32(e, 0x102af928, help_obj)
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
        self.lobby_block = lobby
        theme = e.alloc(0x20, bytes(0x20))
        advisor = e.alloc(0x30, bytes(0x30))
        screens = e.alloc(0x20, bytes(0x20))
        w32(e, screens + 4, theme)
        w32(e, screens + 0x14, advisor)
        w32(e, G["screens"], screens)
        self.theme_block = theme
        self.island_block = e.alloc(0x74, bytes(0x74))
        self.set_string(self.island_block + 0x28, "theme0", False, fresh=True, observe=False)
        mesh_a = e.alloc(64, bytes(64))
        mesh_b = e.alloc(64, bytes(64))
        w32(e, self.island_block + 4, mesh_a)
        w32(e, self.island_block + 8, mesh_b)
        node = e.alloc(12, struct.pack(">3I", self.island_block, 0, 0))
        island_list = e.alloc(16, struct.pack(">4I", 0, node, node, 1))
        w32(e, theme + 8, island_list)
        w32(e, theme + 12, 0)
        self.park_block = e.alloc(0xbc, bytes(0xbc))
        for address, label in ((self.island_block, 27), (mesh_a, 28), (mesh_b, 29), (self.park_block, 25)):
            L.labels[address] = label
        self.user_block = e.alloc(0xD4, bytes(0xD4))
        w32(e, self.user_block + 0x60, -1)
        self.master_block = e.alloc(0x4c, bytes(0x4c))
        L.labels[self.master_block] = 24
        self.saved_slot = 0
        self.master_deleted = False
        self.dialog_window = 0
        for key in ("master", "game_options", "key_disabled", "timer_paused"):
            w32(e, G[key], 0)
        w32(e, G["game_type"], 0)
        e.write(G["type_guard"], b"\x01")
        w32(e, G["game"] + 0x3c, 0)
        w32(e, G["game"] + 0x1c, 0)
        e.write(G["game"] + 0x20, b"\0")
        w32(e, G["game"] + 0x28, 0)
        e.write(G["settings"] + 0x36, b"\x01")
        # The separator and gms filename are the genuine initialized strings of the image.
        self.set_string(0x10380b38, "gms.dat", True, fresh=True, observe=False)
        self.set_string(0x103b74c8 + 0x44c0, ":", True, fresh=True, observe=False)
        for address, label in ((lobby, LAB_LOBBY), (screens, LAB_SCREENS), (theme, LAB_THEME), (advisor, LAB_ADVISOR),
                               (self.user_block, LAB_USER)):
            L.labels[address] = label

    def string_text(self, obj, is_wide):
        data = self.e.u32(obj + 8)
        if data == 0:
            return ""
        return CL.wide_text(self.e, data) if is_wide else self.e.cstr(data)

    def set_string(self, obj, text, is_wide, fresh=False, observe=True):
        e = self.e
        capacity = 0 if fresh else e.u32(obj + 12)
        if fresh or len(text) > capacity:
            capacity = 8 * (len(text) // 8 + 1)
            data = e.alloc((capacity + 1) * (2 if is_wide else 1), b"\xa5" * ((capacity + 1) * (2 if is_wide else 1)))
            if observe and self.observe:
                self.heap[1] += 1
                if not fresh:
                    self.heap[3] += 1
            w32(e, obj + 8, data)
            w32(e, obj + 12, capacity)
        data = e.u32(obj + 8)
        e.write(data, wide(text) if is_wide else text.encode("latin-1") + b"\0")
        w32(e, obj + 4, len(text))
        w32(e, obj + 16, 8)

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
        previous = self.cached_windows.get(w)
        if previous is not None and previous[0] == (d, depth):
            return previous[1]
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
        for k in range(16):
            value = u(0x9c + 4 * k)
            if self.callback_code(u(0x110)) == 23 and k < 3:
                out.append({0: 0, 0x10267cd8: 1, 0x10267cc8: 2}.get(value, 3))
            else:
                out.append(s32(value))
        if kind == 2:
            out += [T.s8(d[0x138]), T.s8(d[0x139]), i(0x13C), i(0x140), int(u(0x148) != 0), u(0x14C), u(0x154), T.s8(d[0x158]),
                    T.s8(d[0x159]), d[0x15A]]
        self.cached_windows[w] = ((d, depth), out)
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
            header = bytes(e.read(table, 8))
            entries, count, enabled = struct.unpack(">Ihh", header)
            data = bytes(e.read(entries, ENTRIES * 0x14))
            previous = self.cached_tables.get(t)
            key = (table, header, data)
            if previous is not None and previous[0] == key:
                out += previous[1]
                continue
            start = len(out)
            out += [int(table == self.table_objects[t]), enabled, count]
            for k in range(ENTRIES):
                at = 0x14 * k
                out += list(struct.unpack_from(">hhhh", data, at)) + list(data[at + 8:at + 16])
                out.append(struct.unpack_from(">I", data, at + 0x10)[0])
            self.cached_tables[t] = (key, out[start:])
        out += list(struct.unpack(">%dH" % BUFFER_WORDS, e.read(G["buffer"], 2 * BUFFER_WORDS)))
        out += list(self.heap)
        out.append(len(L.log))
        user = e.u32(G["user"])
        master = e.u32(G["master"])
        out += [s32(e.u32(self.user_block + 0x60)), s32(e.u32(user + 0x60)) if user else -2, L.lab(master),
                0 if self.master_deleted else s32(e.u32(self.master_block + 0x20)),
                0 if self.master_deleted else int(e.read(self.master_block + 0x24, 1)[0] != 0),
                s32(e.u32(master + 0x20)) if master else 0, int(e.read(master + 0x24, 1)[0] != 0) if master else 0,
                s32(e.u32(G["game_type"])), e.u32(G["game_options"]), e.read(G["key_disabled"], 1)[0],
                s32(e.u32(G["timer_paused"])), s32(e.u32(G["game"] + 0x3c)), s32(e.u32(G["game"] + 0x1c)),
                e.read(G["game"] + 0x20, 1)[0], self.saved_slot, L.lab(self.dialog_window), L.lab(e.u32(self.theme_block + 12)), e.u32(0x102e48f8 + 4)]
        return out

    def op(self, code, a):
        """Runs one operation. Returns the answer (four words) or None when the state does not allow it."""
        e, L = self.e, self.L
        answer = [0, 0, 0, 0]
        # the operations that only set the world up
        if code == 20:   # a player's name: slot, text (-1 none)
            slot = a[0]
            length = 0
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
            user = e.u32(G["user"]) or self.user_block
            w32(e, user + 24 * slot + 4, length if self.names[slot] else 0)
            w32(e, user + 24 * slot + 8, self.texts[a[1]] if self.names[slot] else 0)
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
        if code == 60:
            kind, value = a
            if kind == 0:
                w32(e, self.user_block + 0x60, value)
            elif kind == 1:
                w32(e, self.master_block + 0x20, value)
            elif kind == 2:
                e.write(self.master_block + 0x24, bytes([value & 255]))
            elif kind == 3:
                w32(e, G["master"], self.master_block if value else 0)
            elif kind == 4:
                w32(e, G["first"], value)
            elif kind == 5:
                w32(e, G["game_type"], value)
            elif kind == 6:
                w32(e, G["game_options"], value)
            elif kind == 7:
                e.write(G["key_disabled"], bytes([value & 255]))
            elif kind == 8:
                w32(e, G["timer_paused"], value)
            elif kind == 9:
                w32(e, G["game"] + 0x3c, value)
            elif kind == 10:
                w32(e, G["lobby"], self.lobby_block if value else 0)
            elif kind == 11:
                e.write(G["settings"] + 0x36, bytes([int(value != 0)]))
            elif kind == 12:
                nodes = []
                for i in range(value):
                    text = e.alloc(20, bytes(20))
                    self.set_string(text, "theme%d" % i, False, fresh=True, observe=False)
                    node = e.alloc(12, bytes(12))
                    w32(e, node, text)
                    nodes.append(node)
                for i in range(len(nodes) - 1):
                    w32(e, nodes[i] + 4, nodes[i + 1])
                w32(e, G["game"] + 0x28, nodes[0] if nodes else 0)
            return answer
        if code in (34, 35, 41, 42, 43):
            w = self.window(a[0])
            if w == 0:
                return None
        if code in (37, 39) and e.u32(G["user"]) == 0:
            return None
        if code == 44 or (code == 41 and a[1] == 5):
            panel_window = e.u32(G["panel"])
            if panel_window == 0 or e.u32(panel_window + 0x5c) == 0:
                return None
        if code == 42 and a[1] in (0x100, 0x10006):
            slot = e.u32(w + 0x9c)
            if slot > 3:
                return None
            if a[1] == 0x100 and not self.names[slot]:
                return None
            user = e.u32(G["user"])
            if a[1] == 0x10006 and user and e.u32(user + 24 * slot + 4):
                panel_window = e.u32(G["panel"])
                if panel_window == 0 or e.u32(panel_window + 0x5c) == 0:
                    return None
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
            elif code == 31:
                self.call(FN["enable_all"])
            elif code in (32, 33):
                self.call(FN["enable"] if code == 32 else FN["disable_one"], self.table_objects[a[0]])
            elif code == 34:
                value = self.call(FN["get_extra"], w, a[1])
                answer[0] = L.lab(value) if s16(a[1]) < 0 else s32(value)
            elif code == 35:
                self.call(FN["wrap"], w, a[1], a[2])
            elif code == 36:
                self.call(FN["give_key"], self.master_block)
            elif code == 37:
                self.call(FN["save"], e.u32(G["user"]))
            elif code == 38:
                answer[0] = int(self.call(FN["is_easy"], self.master_block) != 0)
            elif code == 39:
                self.call(FN["login"], e.u32(G["user"]), a[0])
            elif code == 40:
                params = e.alloc(16, bytes(16))
                w32(e, params, self.text(a[0]))
                e.write(params + 4, bytes(x & 255 for x in a[1:4]))
                w32(e, params + 8, a[4])
                w32(e, params + 12, a[5])
                callbacks = [0, 0x10267cd8, 0x10267cc8]
                answer[0] = L.lab(self.call(FN["dialog"], params, a[6], callbacks[a[7]], a[8], callbacks[a[9]], a[10], callbacks[a[11]], a[12]))
            elif code in (41, 42, 43):
                fn = {41: "panel_msg", 42: "entry_msg", 43: "quit_msg"}[code]
                answer[0] = s32(self.call(FN[fn], w, *a[1:4]))
            elif code == 44:
                self.call(FN["remove"], a[0])
            elif code == 45:
                answer[0] = self.call(FN["disabled"])
            elif code == 46:
                self.call(FN["ui_disable"])
            elif code == 47:
                self.call(FN["pause_timer"])
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
        if flags & 0x3000:
            style = emu.u32(record + 0x10)
            L.rec("Change.wrap", (label, *emu.read(style + 6, 2)))
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
    def instance(e):
        us, obj = e.us, e.r(3)
        h = us.seed_hash[obj]
        if e.cl.lab(obj) < 0:
            e.cl.newlab(obj)
            us.name_draw(obj)
            us.library.append(obj)
            us.hash_of[obj] = h
        e.cl.rec("Draw.InstanceMe", (e.cl.lab(obj), h))
        e.set_r(3, obj)
    e.write(vt + 0x28, struct.pack(">I", T.fake_function(e, instance)))
    e.lp_draw_vtable = vt
    e.lp_change = change
    e.lp_show = show
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
    S.real.add(POST_MESSAGE)
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(ADD_TIMER, R("AddTimer", "Lii", ret=lambda e, L: 0))
    S.internal(RELEASE_STRING, R("ReleaseString", "L"))

    # Show records, and (the queue "ShowDropsUserInfo") may make the user information vanish at once

    S.real.update([WINDOW_SHOW, 0x101dbf88, 0x101dbafc, 0x101cfdb0])

    # the draw object library: one fake draw object per hash, named when first asked for

    S.real.update([FIND_DRAW, 0x101cd2d4, 0x101c8d40])

    # the title text object's constructor: the vtable and the record (the object came from operator new and has its label)
    def title_ctor(e):
        L, us, this, data = e.cl, e.us, e.r(3), e.r(4)
        L.rec("TitleText.ctor", (L.lab(this), int(data != 0)))
        w32(e, this + 0x38, e.lp_draw_vtable)
        us.name_draw(this)
        e.set_r(3, this)

    S.internal(TITLE_CTOR, title_ctor)

    # the font table: a pool of 13 fonts, NULL past it

    S.real.add(GET_FONT)

    # the user information: the constructor gives the object a label; the names and the easy-mode answers are the world's

    def num_valid(e, L):
        return sum(1 for name in e.us.names if name)

    def user_name(e, L):
        return e.us.names[e.r(4)] if e.r(4) < NAMES else 0

    def user_easy(e, L):
        return e.us.easy[e.r(4)] if e.r(4) < NAMES else 0

    S.real.add(USER_CTOR)
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
    extend_stubs(S, emu)
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
        handles = []
        for address, callback in e.lm_deferred.items():
            def hook(uc, addr, size, user, target=callback):
                e.pending_cb = target
                uc.reg_write(P.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)
            handles.append(e.uc.hook_add(UC_HOOK_CODE, hook, begin=address, end=address))
        try:
            return fn(e, c)
        finally:
            for handle in handles:
                e.uc.hook_del(handle)
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


# ======================================================================= the module


def setup(runner):
    global RUNNER, STUBS
    RUNNER = runner
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)
    open_host(runner.lib)



# These imports are the string library boundary. They keep the original dynamic string capacity and heap counts.
def extend_stubs(S, emu):
    R = CL.recorder
    def default_handler(e):
        window, message = e.r(3), s32(e.r(4))
        e.cl.rec("DefaultHandler", (e.cl.lab(window), message, s32(e.r(5)), s32(e.r(6))))
        if message == 4 and window == e.u32(G["panel"]):
            w32(e, G["panel"], 0)
        e.set_r(3, e.cl.ret("DefaultHandler"))
    S.internal(0x101de3f8, default_handler)
    S.internal(0x101c7c98, R("Queue.Add", "LLiii", ret=lambda e, L: 1))
    S.internal(0x101f1520, R("DialogHandler", "Liii", ret=lambda e, L: 0))
    S.internal(0x101af60c, R("NewName", "i"))
    S.internal(0x101caaac, R("EnableHelpText"))
    S.internal(0x101caa4c, R("DisableHelpText"))
    S.internal(0x100a7128, R("Advisor.ClearAll", "L"))
    S.real.add(0x100b3760)
    S.real.add(0x100b38d4)
    S.internal(0x101e15b4, R("UpdateIslandLobbyUI"))
    S.internal(0x1014738c, R("Clock.Pause"))
    S.internal(0x101ef8a0, R("UIInterface.Pause"))
    S.internal(0x10007d54, R("ADV_Pause"))
    S.internal(0x100dd548, R("SOUND_Pause"))
    S.internal(0x100dde38, R("SOUND_SetPauseOn3DSoundsOnly", "i"))
    S.internal(0x1000dde8, R("GAME_ASSERTMSG", "is"))


    def master_dtor(e):
        us, L, obj = e.us, e.cl, e.r(3)
        L.rec("Master.dtor", (L.lab(obj),))
        if obj == us.master_block:
            us.master_deleted = True
        if T.s16(e.r(4)) > 0:
            us.heap[2] += 1
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    def load(e):
        obj = e.r(3)
        e.cl.rec("Master.Load", (e.cl.lab(obj),), (e.cstr(e.r(4)),))
        e.write(obj + 0x24, bytes([e.cl.ret("LoadEasy") & 255]))
        w32(e, obj + 0x20, e.cl.ret("LoadKeys"))
        if e.cl.ret("LoadDropsMaster"):
            w32(e, G["master"], 0)
        e.set_r(3, e.cl.ret("Load", 1))

    def info(e):
        e.cl.rec("Master.GetInfoOn", (e.cl.lab(e.r(3)),), (e.us.string_text(e.r(4), False),))
        if e.cl.ret("InfoDropsMaster"):
            w32(e, G["master"], 0)
        e.set_r(3, e.us.park_block)

    def save(e):
        master = e.u32(G["master"])
        slot = s32(e.r(4))
        e.us.saved_slot = slot
        e.cl.rec("UserInfo.InternalSave", (e.cl.lab(e.r(3)), slot, e.cl.lab(master), s32(e.u32(master + 0x20)) if master else 0))

    S.real.update([0x101694dc, 0x10169a88])
    S.internal(0x1016997c, master_dtor)
    S.internal(0x1016a6c8, load)
    S.internal(0x1016b47c, info)
    S.internal(0x1017fab0, save)

    def path(e):
        out, user, slot = e.r(3), e.r(4), s32(e.r(5))
        e.cl.rec("UserInfo.ReturnPath", (e.cl.lab(user), slot, s32(e.u32(user + 0x60))))
        e.us.set_string(out, "users:%splayer" % chr(ord("1") + slot), True, fresh=True)

    def convert(e):
        out, source = e.r(3), e.r(4)
        text = e.us.string_text(source, True)
        e.cl.rec("UniStringToString", (), (text,))
        e.us.set_string(out, text, False)
        e.set_r(3, len(text) + 1)

    def mail(name, offset):
        def hook(e):
            e.cl.rec(name, (e.cl.lab(e.r(3) - offset),), (e.cstr(e.r(4)),))
            e.set_r(3, e.cl.ret(name))
        return hook

    S.internal(0x1017ebf0, path)
    S.internal(0x10005670, convert)
    S.internal(0x10203ce0, mail("Outbox.Load", 0x64))
    S.internal(0x10206b2c, mail("AddressBook.Load", 0xc4))

    for kind, is_wide in (("c", False), ("w", True)):
        def default(e, iw=is_wide):
            e.us.set_string(e.r(3), "", iw, fresh=True)

        def copy(e, iw=is_wide):
            e.us.set_string(e.r(3), e.us.string_text(e.r(4), iw), iw, fresh=True)

        def append(e, iw=is_wide):
            e.us.set_string(e.r(3), e.us.string_text(e.r(3), iw) + e.us.string_text(e.r(4), iw), iw)

        def destroyed(e):
            if e.us.observe:
                e.us.heap[3] += 1

        def text_ctor(e, iw=is_wide):
            text = CL.wide_text(e, e.r(4)) if iw else e.cstr(e.r(4))
            e.us.set_string(e.r(3), text, iw, fresh=True)

        def assign_text(e, iw=is_wide):
            text = CL.wide_text(e, e.r(4)) if iw else e.cstr(e.r(4))
            e.us.set_string(e.r(3), text, iw)

        S.imp(f"__ct__26TbDynamicStringTemplate<{kind}>FPC{kind}", text_ctor)
        S.imp(f"__as__26TbDynamicStringTemplate<{kind}>FPC{kind}", assign_text)
        S.imp(f"__ct__26TbDynamicStringTemplate<{kind}>Fv", default)
        S.imp(f"__ct__26TbDynamicStringTemplate<{kind}>FRC26TbDynamicStringTemplate<{kind}>", copy)
        S.imp(f"__dt__26TbDynamicStringTemplate<{kind}>Fv", destroyed)
        S.imp(f"__apl__26TbDynamicStringTemplate<{kind}>FRC15TbStringBase<{kind}>", append)
        S.imp(f"Length__15TbStringBase<{kind}>CFv", lambda e: e.set_r(3, e.u32(e.r(3) + 4)))

    def sign_name(which):
        def hook(e):
            obj, out, size = e.r(3), e.r(4), e.r(5)
            e.cl.rec("Park.GetSignName" + which, (e.cl.lab(obj), e.cl.lab(out), size))
            if size >= 2:
                e.write(out, wide(which))
        return hook
    S.internal(0x1016c7e8, sign_name("A"))
    S.internal(0x1016ca34, sign_name("B"))

    def convert_to_wide(e):
        out, source = e.r(3), e.r(4)
        text = e.us.string_text(source, False)
        e.cl.rec("StringToUniString", (), (text,))
        e.us.set_string(out, text, True)
        e.set_r(3, len(text) + 1)
    S.internal(0x100057a0, convert_to_wide)

    def gate_names(e):
        text = e.us.string_text(e.r(4), True)
        e.cl.rec("Game.GetGateNames", (), (text,))
        e.us.set_string(e.r(5), "GateA", True)
        e.us.set_string(e.r(6), "GateB", True)
    S.internal(0x1014a768, gate_names)
    S.internal(0x100a7e84, R("Mesh.SetRideSigns", "LSS"))


    def format_string(e):
        table, out, size, ident, tag = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        e.cl.rec("FormatString", (e.cl.lab(table), e.cl.lab(out), size, ident, e.u32(tag), e.u32(tag + 8)),
                 (CL.wide_text(e, e.u32(tag + 12)),))
        text = "Delete player?"[:max(0, size - 1)]
        e.write(out, wide(text))
        e.set_r(3, len(text))

    S.real.update([0x101c9518, 0x101de384])
    for address in (0x101cb5f0, 0x101cc27c, 0x101cd8fc, 0x101d07ac, 0x101d1438, 0x101d2080, 0x101d8134, 0x101d9674):
        S.internal(address, lambda e: e.set_r(3, 0))
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)


    def dialog(e, call):
        script, callback, modal = e.r(3), e.r(4), s32(e.r(5))
        e.cl.rec("Dialog", (int(script == 0x102b1380), e.us.callback_code(callback), modal))
        values = list(struct.unpack(">77h", e.read(script, 154)))
        for i in range(0, 77, 10):
            e.cl.rec("Dialog.script", (i, *(values[i:i + 10] + [0] * max(0, i + 10 - 77))))
        if e.cl.ret("DialogFails"):
            return 0
        window = call(("code", 0x101deb78), e.us.windows[0], script, callback)
        e.us.dialog_window = window
        return window



    S.real.update([0x101cd2d4, 0x101f12c8])
    emu.lm_deferred = {0x101f12c8: dialog}
    S.real.add(0x1018d11c)
    S.real.add(0x1017df80)
    S.internal(0x10203940, lambda e: e.set_r(3, e.r(3)))
    S.internal(0x102064cc, lambda e: e.set_r(3, e.r(3)))
    S.internal(0x1016d63c, lambda e: e.write(e.r(3), bytes(12)))
    S.internal(0x1016eaf0, lambda e: e.write(e.r(3), bytes(12)))
    S.internal(0x1016e37c, lambda e: e.write(e.r(3), bytes(16)))
    S.imp("__ct__15TbTextFormatterFv", lambda e: None)

    def sprite_base(e):
        obj, data = e.r(3), e.r(4)
        e.cl.rec("TextSprite.base_ctor", (e.cl.lab(obj), int(data != 0)))
        e.write(obj, bytes(0x3c))
        w32(e, obj + 0x38, e.lp_draw_vtable)
        e.us.name_draw(obj)
        e.set_r(3, obj)

    S.internal(0x1018c978, sprite_base)
    S.internal(0x1018d798, emu.lp_change)
    S.internal(0x1018cf10, emu.lp_show)


# Every case begins with the same genuine login panel built by CreateLoginPanel.
def login_world(names=True):
    operations = STANDARD + [string(9, 1), user_info(1)]
    if names:
        operations += [name(i, i + 1) for i in range(4)]
    return operations + [CREATE]


def config(kind, value):
    return op(60, kind, value)


def handler(code, window, message, a=0, b=0):
    return op(code, window, message, a, b)


def key_cases():
    for count in (-32768, -1, 0, 1, 15, 16):
        for enabled in (-32768, -1, 0, 1, 32767):
            yield case([table(i, count, enabled, i + 10) for i in range(7)] +
                       [op(32, 0), op(33, 1), op(31), op(33, 4), config(7, 255), op(45), op(46), op(45), config(8, -1), op(47)])


def leaf_cases():
    for index in (-2, -1, 0, 1, 15, 16, 17, 32767, 65536, 65551, 65552):
        yield case([draws(0, 0, 1), extra_param(0, 0, 0x12345678), extra_param(0, 15, 0x87654321), op(34, 0, index)])
    for main, client in ((-1, -1), (0, -1), (-1, 0), (0, 1), (0, 0)):
        for h, v in ((0, 0), (1, 1), (-1, 256), (255, 128), (-2147483648, 2147483647)):
            yield case([draws(0, main, client), op(35, 0, h, v)])
    for keys in (-2147483648, -1, 0, 1, 2147483646, 2147483647):
        for easy_value in (0, 1, 2, 127, 128, 255, 256, -1):
            yield case([user_info(1), config(0, keys), config(1, keys), config(2, easy_value), op(36), op(36), op(38), op(37)])


def login_cases():
    for slot in range(4):
        for existing in (0, 1):
            for game_type in (-1, 0, 1, 2, 3):
                for easy_value in (0, 1, 255):
                    for themes in (0, 1, 4, 16):
                        yield case([user_info(1), config(3, existing), config(5, game_type), config(12, themes), op(39, slot)],
                                   q=dict(LoadEasy=[easy_value], LoadKeys=[2147483647], Load=[0]))
    for drop in (0, 1):
        yield case([user_info(1), config(12, 3), op(39, 2)], q=dict(LoadDropsMaster=[drop], InfoDropsMaster=[0, 1, 0]))


def dialog_args(r):
    return (r.choice([-1, 0, 1, 3]), *[r.randrange(256) for _ in range(3)], r.choice([-1, 0, 4, 12, 13]),
            r.choice([-2147483648, -1, 0, 3, 2147483647]), *[v for _ in range(3) for v in (r.choice([-1, 0, 1, 2, 3]), r.randrange(3))], r.randrange(2))


def dialog_cases():
    import itertools
    for buttons in itertools.product((-1, 0, 1, 2, 3), repeat=3):
        for flag in (0, 1):
            yield case([config(7, 255), op(40, 1, 128, 255, 0, 4, -1, buttons[0], 1, buttons[1], 2, buttons[2], 0, flag)])
    # triage: button kinds far from 0, 1, 2 (the ladder compares them signed)
    for kind in (4, 255, 256, 2147483647, -2, -2147483648):
        for others in ((0, 1), (2, 3), (kind, 1)):
            yield case([config(7, 255), op(40, 1, 128, 255, 0, 4, -1, kind, 1, others[0], 2, others[1], 0, 0)])
            yield case([config(7, 255), op(40, 1, 128, 255, 0, 4, -1, others[0], 1, kind, 2, others[1], 0, 1)])
    for active in (-1, 0, 1, 2):
        for lobby in (0, 1):
            for flag in (0, 1):
                yield case([config(9, active), config(10, lobby), op(40, -1, 0, 1, 255, 13, 0, 2, 0, 1, 1, 0, 2, flag)])
    yield case([config(9, 1), config(10, 0), op(40, 0, 0, 0, 0, 0, 123, 1, 2, 2, 1, 1, 0, 0)], q=dict(DialogFails=[1]))


def handler_cases():
    messages = (-2147483648, -1, 0, 4, 5, 0x13, 0x14, 0x15, 0x100, 0x101, 0x10000, 0x10001, 0x10002, 0x10003,
                0x10004, 0x10005, 0x10006, 0x10007, 0x11006, 2147483647)
    for message in messages:
        for slot in range(4):
            yield case(login_world() + [handler(41, PANEL, message, -1, 2147483647)], q=dict(DefaultHandler=[-123]))
            yield case(login_world() + [extra_param(0, 0, slot), draws(0, 0, 1), handler(42, 0, message, -1, 2147483647)],
                       q=dict(DefaultHandler=[456], LoadEasy=[slot & 1]))
            yield case(login_world() + [handler(43, 109, message, 2, -1)], q=dict(DefaultHandler=[789]))
    for slot in range(4):
        for present in (0, 1):
            yield case(login_world(False) + [user_info(present), extra_param(0, 0, slot), handler(42, 0, 0x10006)])
    for cancel in (-1, 0, 1, 2):
        for user in (-1, 0, 3, 4):
            for first in (0, 1, -1):
                for game_type in (-1, 0, 1, 2, 3):
                    yield case(login_world() + [config(0, user), config(4, first), config(5, game_type), op(44, cancel)])
    for slot in range(4):
        for tooltips in (0, 1):
            yield case(login_world() + [extra_param(0, 0, slot), config(11, tooltips), handler(42, 0, 0x10006)], q=dict(LoadEasy=[slot & 1]))
    yield case(login_world() + [handler(41, PANEL, 0x14), op(44, 0)])
    # triage: the lazily made user information (NULL gUserInfo) in the delete prompt and in RemoveLoginPanel, and the
    # master save already present in RemoveLoginPanel (0x101ae05c, 0x101af0dc, 0x101af174)
    for slot in range(4):
        yield case(login_world() + [user_info(0), extra_param(0, 0, slot), handler(42, 0, 0x100)])
    for game_type in (0, 1, 2):
        yield case(login_world() + [user_info(0), config(5, game_type), op(44, 0)])
        yield case(login_world() + [config(0, 2), config(4, 1), config(3, 1), config(5, game_type), op(44, 0)])


def random_keys(r):
    operations = []
    for i in range(7):
        operations.append(table(i, r.choice([-32768, -1, 0, 1, 16, r.randrange(17)]), r.randrange(-32768, 32768), r.randrange(1000)))
    operations += [op(32, r.randrange(7)), op(33, r.randrange(7)), op(31), config(7, r.randrange(256)), op(45), op(46), op(45), op(47)]
    return case(operations)


def random_leaves(r):
    return case([draws(0, r.randrange(-1, 8), r.randrange(-1, 8)), extra_param(0, r.randrange(16), r.getrandbits(32)),
                 op(34, 0, r.choice([-2, -1, r.randrange(18)])), op(35, 0, r.getrandbits(32), r.getrandbits(32)),
                 user_info(1), config(0, r.getrandbits(32)), config(1, r.getrandbits(32)), config(2, r.randrange(256)),
                 op(36), op(38), op(37)])


def random_login(r):
    return case([user_info(1), config(3, r.randrange(2)), config(5, r.choice([-1, 0, 1, 2, 3])), config(12, r.randrange(17)),
                 op(39, r.randrange(4))], q=dict(LoadEasy=[r.randrange(256)], LoadKeys=[r.getrandbits(32) - 2147483648],
                                                LoadDropsMaster=[r.randrange(2)], InfoDropsMaster=[0, 1, 0]))


def random_dialog(r):
    return case([config(9, r.choice([-1, 0, 1, 2])), config(10, r.randrange(2)), config(7, r.randrange(256)), op(40, *dialog_args(r))],
                q=dict(DialogFails=[r.choice([0, 0, 0, 1])]))


def random_handlers(r):
    operations = login_world(r.randrange(2))
    for _ in range(200):
        code = r.choice([31, 35, 41, 42, 43, 44, 45, 46, 47])
        if code == 35:
            operations.append(op(35, 0, r.randrange(256), r.randrange(256)))
        elif code in (41, 42, 43):
            window = PANEL if code == 41 else 0 if code == 42 else 109
            operations.append(handler(code, window, r.choice([0x14, 0x10001, 0x10002, 0x10003, 0x10006, 0x11006, 5, -1])))
            if code == 42:
                operations.insert(len(operations)-1, extra_param(0, 0, r.randrange(4)))
        elif code == 44:
            operations.append(op(44, r.randrange(2)))
        else:
            operations.append(op(code))
    return case(operations, q=dict(LoadEasy=[r.randrange(256)], LoadKeys=[r.randrange(5)], DefaultHandler=[r.randrange(-999, 1000)]))


TESTS = [
    FnTest("key tables: Enable, Disable, EnableAllKeytables and the UI leaves", ops_ppc, ops_host, edge=lambda: list(key_cases()), rnd=random_keys),
    FnTest("getExtraParam, SetFontWrap, GiveExtraKey, IsEasyModeUser and SaveGlobalMasterSave", ops_ppc, ops_host,
           edge=lambda: list(leaf_cases()), rnd=random_leaves),
    FnTest("CFSUserInfo::GameLogin", ops_ppc, ops_host, edge=lambda: list(login_cases()), rnd=random_login),
    FnTest("UI_DialogBox: controls, callbacks, pause and key bookkeeping", ops_ppc, ops_host,
           edge=lambda: list(dialog_cases()), rnd=random_dialog),
    FnTest("LoginPanelMsg, EntryMsg, QuitMsg and RemoveLoginPanel", ops_ppc, ops_host,
           edge=lambda: list(handler_cases()), rnd=random_handlers),
]
