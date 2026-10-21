"""Differential tests for the sprite draw object, the scroll bar and the window's show and position leaves, against
libtpw_unit_UIScrollBar.dylib (tests/unit_UIScrollBar.cpp):

  src/game/ui/InterfaceDrawObjectSprite.cpp    InterfaceDrawObjectSprite: constructor, destructor, UpdateTextures, CreateTextures, Refresh, Change,
                                 Show, Draw, GetWritableDataArea, SetPreDrawCallback
  src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceScrollBar.cpp     InterfaceScrollBar: constructor, destructor, CalcClientArea, WhatAmI, Enable, ReCalcValue,
                                 ReCalcThumbPos, GetValue, SetMinMax, SetValue, SetPageValue, AddIncButton, AddDecButton,
                                 AddThumbTrack, ClientClick, DrawFunction, ClientDraw; UI_DefaultScrollBarCallback
  src/game/ui/InterfaceWindow.cpp   InterfaceWindow: Show, ShowAllDRAWOBJS, GetWindow and the two other SetPosition overloads

Real on both sides: those, the window's constructor (src/game/ui/InterfaceWindow.cpp, which builds the test trees) and the rectangles
(src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). Fakes that record the same event on both sides: the message senders and UI_UpdatePointerIn, the default window
handler and the thumb's, the window's other members (destructor, Enable, DrawMe, GetPosition, SetPosition(CoolRect*), SetValue, ClientDraw,
SetCallbackFunction, CreateObject, CalcClientArea, DrawFunction), the draw object base class and two kinds of fake draw object, the help
text handler, the engine's sprite surfaces (SSpriteGameData, SSpriteEngineData, CSpriteList) and CMapWho::Link and Unlink.

A case is a script of operations on a world of ten windows (plain windows or scroll bars), four fake draw objects, four sprite draw
objects, a fake map, a pixel buffer and a help text handler. After each operation the answer, every window, every sprite, the globals, the
heap traffic and the length of the call log are compared, and the whole call log at the end. The PowerPC side decides which operations the
state allows (one that would make the original walk a loop of links, divide by zero, use a thumb that is not there or follow a freed
object is skipped on both sides).

Quirks the tests pin: the sprite object's Change ignores the frame and the pivot, places the sprite from the BOTTOM of the rectangle, and
does nothing before the first rectangle; its Show does nothing (and does not remember the state) until the surface exists; the scroll bar
answers the draw message (3) without the default handler; Enable on a window already in the state asked for answers 0 and does nothing;
ReCalcThumbPos with mode -2 puts the thumb in place and talks to the draw objects even when the value did not change.
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIScrollBar"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (all run as the original on the PowerPC side)
FN = dict(
    Show=0x101DC1EC, ShowAll=0x101DBF88, GetWindow=0x101DBE40, SetPosShort=0x101DB1B0, SetPosInt=0x101DB360,
    BarCtor=0x101D636C, BarDtor=0x101D64B8, CalcClient=0x101D6280, WhatAmI=0x101D6550, Enable=0x101D658C, ReCalcValue=0x101D66A0,
    ReCalcThumbPos=0x101D67DC, GetValue=0x101D6B08, SetMinMax=0x101D6B44, SetValue=0x101D6BD4, SetPage=0x101D6C34, AddInc=0x101D6C74,
    AddDec=0x101D6D20, AddThumb=0x101D6DCC, ClientClick=0x101D6FC0, BarDrawFunction=0x1018A328, BarClientDraw=0x1018A364,
    Handler=0x101D70EC,
    SpriteCtor=0x1018C978, SpriteDtor=0x1018CA88, UpdateTextures=0x1018CB48, CreateTextures=0x1018CBB4, Refresh=0x1018CC5C,
    Change=0x1018CCC0, SpriteShow=0x1018CF10, Draw=0x1018CFF0, GetWritable=0x1018D038, SetPreDraw=0x1018D0BC)
# what they call that is real too: the window's constructor and its helpers, GetNext, and the rectangles and the regions' base
WINDOW_CTOR, MAKE_ABSOLUTE, GET_NEXT = 0x101D98D8, 0x101DAB88, 0x101DBB38
REAL_HELPERS = [WINDOW_CTOR, MAKE_ABSOLUTE, GET_NEXT, 0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618,
                0x101C7678, 0x101C7710, 0x101C7788, 0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00,
                0x101D5B74]
# the fakes (in-binary functions): the senders, the handlers, the window's other members, the draw object base class
SEND_MESSAGE, POST_MESSAGE, UPDATE_POINTER_IN = 0x101C9518, 0x101C9400, 0x101C8F18
DEFAULT_HANDLER, THUMB_HANDLER = 0x101DE3F8, 0x101D7348
W_DRAW_FUNCTION, W_CALC_CLIENT, W_DTOR, W_CLIENT_DRAW, W_ENABLE, W_SET_VALUE = 0x1018A270, 0x101D987C, 0x101D9BA4, 0x1018A2A8, 0x101DC320, 0x101DFE84
W_DRAW_ME, W_GET_POSITION, W_SET_POSITION, W_SET_CALLBACK, W_CREATE_OBJECT = 0x101DA564, 0x101DB884, 0x101DAE98, 0x101DCBA8, 0x101DC750
DRAW_CTOR, DRAW_DTOR = 0x101CCCC0, 0x101CCD9C

# ---- the original's data
G = dict(redraw=0x10395C28, width=0x10395BBC, height=0x10395BBA, help=0x102AF928)
CORE_VTABLE = 0x102AF8FC   # CoolRect's vtable
DEFAULT_TV, BAR_TV, THUMB_TV = 0x10268480, 0x102683C0, 0x10268390   # the handlers' TVectors
SIZE_WINDOW, SIZE_BAR, SIZE_SPRITE = 0x138, 0x15C, 0xDC

# ---- labels (the host's tpw_uisb_labels)
LAB_WINDOW, LAB_DRAW, LAB_SPRITE, LAB_GAME, LAB_SURFACE, LAB_LIST, LAB_INSTANCES = 10, 20, 30, 40, 50, 60, 80
LAB_PIXELS, LAB_MAP, LAB_HELP, LAB_TOKEN = 90, 91, 92, 95
FIRST_NEW = 300
SLOTS, DRAWS, SPRITES, TOKENS = 10, 4, 4, 3
MAX_QUEUE = 64
WINDOW_FIELDS, BAR_FIELDS, SPRITE_FIELDS, GLOBAL_FIELDS = 68, 74, 42, 5

KIND_NONE, KIND_WINDOW, KIND_BAR = 0, 1, 2


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def w16(e, address, value):
    e.write(address, struct.pack(">h", s16(value)))


def f32_bits(e, address):
    return struct.unpack(">I", e.read(address, 4))[0]


def callback_code(tv):
    return 0 if tv == 0 else 1 if tv == DEFAULT_TV else 2 if tv == BAR_TV else 3 if tv == THUMB_TV else 4


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures and the operations."""

    ADDRESSES = [(G["redraw"], 4), (G["width"], 2), (G["height"], 2), (G["help"], 4)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.map_slot = e.data_import("gei_MapWho")
        self.saved[(self.map_slot, 4)] = bytes(e.read(self.map_slot, 4))
        self.windows = [0] * SLOTS
        self.kinds = [KIND_NONE] * SLOTS
        self.sprites = [0] * SPRITES
        self.draws = []
        for _ in range(DRAWS):
            block = e.alloc(0x3C, bytes(0x3C))
            w32(e, block + 0x38, e.uisb_draw_vtable)
            self.draws.append(block)
        self.pixels = e.alloc(16, b"\xa5" * 16)
        self.map = e.alloc(16, bytes(16))
        self.help = e.alloc(0x18, bytes(0x18))
        w32(e, self.help + 0x10, e.uisb_help_vtable)
        self.instances = [e.alloc(16, bytes(16)) for _ in range(SPRITES)]
        self.tokens = [e.alloc(4, bytes(4)) for _ in range(TOKENS)]
        fixed = {}
        fixed.update({a: LAB_DRAW + i for i, a in enumerate(self.draws)})
        fixed.update({a: LAB_INSTANCES + i for i, a in enumerate(self.instances)})
        fixed.update({a: LAB_TOKEN + i for i, a in enumerate(self.tokens)})
        fixed[self.pixels] = LAB_PIXELS
        fixed[self.map] = LAB_MAP
        fixed[self.help] = LAB_HELP
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog(fixed, queues, first_new=FIRST_NEW)
        # the globals as the Mac's data has them (zero), the help text handler and the map the fakes
        w32(e, G["redraw"], 0)
        w16(e, G["width"], 0)
        w16(e, G["height"], 0)
        w32(e, G["help"], self.help)
        w32(e, self.map_slot, self.map)

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def window(self, slot):
        return self.windows[slot] if 0 <= slot < SLOTS and self.kinds[slot] != KIND_NONE else 0

    def sprite(self, slot):
        return self.sprites[slot] if 0 <= slot < SPRITES else 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def made_windows(self):
        return [w for s, w in enumerate(self.windows) if self.kinds[s] != KIND_NONE]

    def acyclic(self):
        """False when the windows' links make a loop (the original would never come back from one)."""
        e = self.e
        known = set(self.made_windows())
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

    def rect_at(self, address, offset):
        return struct.unpack(">4h", self.e.read(address + offset + 4, 8))

    def thumb_of(self, slot):
        """The address of the child of the window with the id 3, as GetWindow finds it (the first), or 0."""
        e = self.e
        child = e.u32(self.windows[slot] + 0x58)
        seen = 0
        while child and seen < 64:
            if T.s32(e.u32(child + 0x88)) == 3:
                return child
            child = e.u32(child + 0x50)
            seen += 1
        return 0

    # ---- the dumps (the orders are tpw_uisb_dump_* in the library)
    def dump_window(self, slot):
        address = self.windows[slot]
        kind = self.kinds[slot]
        d = bytes(self.e.read(address, SIZE_BAR if kind == KIND_BAR else SIZE_WINDOW))
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
        out += [callback_code(u32_(0x110)), callback_code(u32_(0x114)), s32_(0x11C)]
        out += [lab(0x120 + 4 * i) for i in range(4)]
        out += [s32_(0x130), d[0x134], d[0x135], T.s8(d[0x136])]
        if kind == KIND_BAR:
            out += [s32_(0x138), s32_(0x13C), s32_(0x140), s32_(0x144), s32_(0x148), s16_(0x14C)]
        return out

    def dump_sprite(self, slot):
        e, L = self.e, self.L
        a = self.sprites[slot]
        d = bytes(e.read(a, SIZE_SPRITE))
        s16_ = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        u16_ = lambda o: struct.unpack(">H", d[o:o + 2])[0]   # noqa: E731
        s32_ = lambda o: struct.unpack(">i", d[o:o + 4])[0]   # noqa: E731
        u32_ = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [L.lab(u32_(0)), s16_(4), s16_(6), s16_(8), s16_(0xA), s16_(0xC), s16_(0xE)]
        out += [s16_(0x14), s16_(0x16), s16_(0x18), s16_(0x1A), s16_(0x20), s16_(0x22), s16_(0x24), s16_(0x26)]
        out += [s16_(0x28), s16_(0x2A), s16_(0x2C), s16_(0x2E), s16_(0x30), s16_(0x32), s16_(0x34)]
        out += [s16_(0x3C), s16_(0x3E), int(d[0x40:0x44] == b"\xa5" * 4), s16_(0x44), int(d[0x46:0x4C] == b"\xa5" * 6),
                int(u32_(0x4C) != 0), s32_(0x50)]
        out += [s32_(0x64), u32_(0x68), u32_(0x6C), u32_(0x70), L.lab(u32_(0x7C)), u32_(0x80)]
        array = u32_(0x98)
        out += [L.lab(array), L.lab(e.u32(array)) if array else 0, s32_(0xB8)]
        out += [L.lab(u32_(0x9C)), L.lab(u32_(0xA0)), u16_(0xA8), u16_(0xAA)]
        return out

    def dump_globals(self):
        e, L = self.e, self.L
        return [T.s32(e.u32(G["redraw"])), T.s16(struct.unpack(">H", e.read(G["width"], 2))[0]),
                T.s16(struct.unpack(">H", e.read(G["height"], 2))[0]), L.lab(e.u32(self.map_slot)), L.lab(e.u32(G["help"]))]

    def dump_state(self):
        return dict(windows=[self.dump_window(s) if self.kinds[s] != KIND_NONE else [0] for s in range(SLOTS)],
                    sprites=[self.dump_sprite(s) if self.sprites[s] else [0] for s in range(SPRITES)],
                    globals=self.dump_globals(), heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation. Returns the answer (five words) or None when the state does not allow it."""
        e = self.e
        if code in (1, 6):
            slot, parent = a[0], a[1]
            if not 0 <= slot < SLOTS or self.kinds[slot] != KIND_NONE or (parent >= 0 and not self.window(parent)):
                return None
            size = SIZE_WINDOW if code == 1 else SIZE_BAR
            self.windows[slot] = e.alloc(size, b"\xa5" * size)
            self.L.labels[self.windows[slot]] = LAB_WINDOW + slot
            ctor = WINDOW_CTOR if code == 1 else FN["BarCtor"]
            self.call(ctor, self.windows[slot], self.window(parent), a[2], a[3], s16(a[4]), s16(a[5]), s16(a[6]), s16(a[7]))
            self.kinds[slot] = KIND_WINDOW if code == 1 else KIND_BAR
            return [self.L.lab(self.windows[slot]), 0, 0, 0, 0]
        if code == 9:
            slot = a[0]
            if not 0 <= slot < SPRITES or self.sprites[slot] or not -1 <= a[1] < TOKENS:
                return None
            block = e.alloc(0x100, b"\xa5" * 0x100)
            self.sprites[slot] = block
            self.L.labels[block] = LAB_SPRITE + slot
            self.L.labels[block + 0x54] = LAB_GAME + slot
            self.L.labels[block + 0x9C] = LAB_SURFACE + slot
            self.L.labels[block + 0xB4] = LAB_LIST + slot
            self.L.labels[block + 0xB4 + 0x20] = LAB_LIST + slot   # (the list's link base, 0x20 into it, is the list for the log)
            self.call(FN["SpriteCtor"], block, self.tokens[a[1]] if a[1] >= 0 else 0)
            return [self.L.lab(block), 0, 0, 0, 0]
        if code in (2, 3, 4, 5, 11) or 20 <= code <= 24:
            if not self.window(a[0]):
                return None
        if code in (8,) or 30 <= code <= 46:
            if not self.window(a[0]) or self.kinds[a[0]] != KIND_BAR:
                return None
        if code == 10 or 50 <= code <= 58:
            if not self.sprite(a[0]):
                return None
        if code in (2, 3, 4, 5, 7, 8, 10, 11):
            return self.setup(code, a)
        return self.function(code, a)

    def setup(self, code, a):
        e = self.e
        if code == 7:
            if a[0] == 1:
                w32(e, G["redraw"], a[1])
            elif a[0] == 2:
                w16(e, G["width"], a[1])
            elif a[0] == 3:
                w16(e, G["height"], a[1])
            return [0, 0, 0, 0, 0]
        if code == 10:   # a sprite's words
            s = self.sprites[a[0]]
            w16(e, s + 0x44, a[1])
            w16(e, s + 0x3C, a[2])
            w16(e, s + 0x3E, a[3])
            w16(e, s + 4, a[4])
            e.write(s + 0x14, struct.pack(">4h", *[s16(v) for v in a[5:9]]))
            w16(e, s + 0x34, a[9])
            w16(e, s + 0x2C, a[10])
            w16(e, s + 0xA8, a[11])
            w16(e, s + 0xAA, a[12])
            w32(e, s + 0xA0, 0 if a[13] == 0 else self.pixels if a[13] == 1 else 0x12345678)
            return [0, 0, 0, 0, 0]
        w = self.windows[a[0]]
        if code == 2:   # links: slot, parent, previous, next, child
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
            if not self.acyclic():
                raise AssertionError("the script made a loop of links")
        elif code == 3:   # fields: slot, attributes, destroying
            w32(e, w + 0x44, a[1])
            e.write(w, bytes([a[2] & 0xFF]))
        elif code == 4:   # draw objects: slot, the window's, the client's (0.. fake, DRAWS.. a sprite, -1 none)
            for offset, k in ((0x94, a[1]), (0x98, a[2])):
                if 0 <= k < DRAWS:
                    w32(e, w + offset, self.draws[k])
                elif DRAWS <= k < DRAWS + SPRITES:
                    w32(e, w + offset, self.sprites[k - DRAWS])
                else:
                    w32(e, w + offset, 0)
        elif code == 5:   # handler: slot, which
            w32(e, w + 0x110, (0, DEFAULT_TV, BAR_TV, THUMB_TV)[a[1]])
        elif code == 8:   # the bar's words: slot, max, min, value, step, page, style
            e.write(w + 0x138, struct.pack(">5i", *[T.s32(v & MINUS) for v in a[1:6]]))
            w16(e, w + 0x14C, a[6])
        elif code == 11:   # a window's rectangles: slot, which, four
            if a[1] == 2:
                e.write(w + 0xF4, struct.pack(">4h", *[s16(v) for v in a[2:6]]))
            else:
                e.write(w + (0x2C if a[1] == 1 else 8), struct.pack(">4h", *[s16(v) for v in a[2:6]]))
        return [0, 0, 0, 0, 0]

    def allowed(self, code, a):
        """False for an operation the state does not allow: the original would walk a loop, use a thumb that is not there, divide by
        zero, read a register nothing set or follow an object that is gone."""
        e = self.e
        if (20 <= code <= 24 or 30 <= code <= 46) and not self.acyclic():
            return False
        if code == 33 and a[1] not in (-2, -1, 0, 1, 2, 3):
            return False   # (the original leaves the new value as the caller's register had it)
        thumb = self.thumb_of(a[0]) if 30 <= code <= 46 else 0
        if code in (32, 33, 35) and not thumb:
            return False
        if code == 32 and not self.track_ok(a[0]):
            return False
        if code == 42:
            message = a[1]
            # the messages that move the thumb need it (the click tolerates its absence)
            if message in (0x100, 0x10008, 0x10009) and a[2] in (1, 2) and not thumb:
                return False
            if message == 0x1000D and not thumb:
                return False
        if code == 46:
            w = self.windows[a[0]]
            return not any(e.u32(w + o) for o in (0x50, 0x54, 0x58, 0x5C)) and not self.referenced(w)
        if code == 58:
            return not self.referenced(self.sprites[a[0]])
        return True

    def referenced(self, address):
        e = self.e
        return any(e.u32(w + o) == address for w in self.made_windows() for o in (0x94, 0x98))

    def track_ok(self, slot):
        """ReCalcValue divides by the length of the track: False when that is 0."""
        e = self.e
        w = self.windows[slot]
        thumb = self.thumb_of(slot)
        if not thumb:
            return False
        left, top, right, bottom = struct.unpack(">4h", e.read(thumb + 8, 8))
        offsets = struct.unpack(">4h", e.read(thumb + 0xF4, 8))
        left, top, right, bottom = [s16(v - o) for v, o in zip((left, top, right, bottom), offsets)]
        style = struct.unpack(">h", e.read(w + 0x14C, 2))[0]
        client = struct.unpack(">4h", e.read(w + 0x2C, 8))
        if style & 2:
            track = (client[2] - (right - left)) - client[0]
        else:
            track = (client[3] - (bottom - top)) - client[1]
        return track != 0

    def function(self, code, a):
        if not self.allowed(code, a):
            return None
        e, L = self.e, self.L
        w = self.windows[a[0]] if 0 <= a[0] < SLOTS else 0
        s = self.sprites[a[0]] if 0 <= a[0] < SPRITES else 0
        answer = [0, 0, 0, 0, 0]
        if code == 20:
            answer[0] = T.s32(self.call(FN["Show"], w, s16(a[1])))
        elif code == 21:
            self.call(FN["ShowAll"], w, s16(a[1]))
        elif code == 22:
            answer[0] = L.lab(self.call(FN["GetWindow"], w, a[1]))
        elif code == 23:   # SetPosition(short*, short*): slot, x, y, x null, y null
            cells = e.alloc(4, struct.pack(">2h", s16(a[1]), s16(a[2])))
            answer[0] = T.s32(self.call(FN["SetPosShort"], w, 0 if a[3] else cells, 0 if a[4] else cells + 2))
            answer[1], answer[2] = struct.unpack(">2h", e.read(cells, 4))
        elif code == 24:
            answer[0] = T.s32(self.call(FN["SetPosInt"], w, a[1], a[2], a[3], a[4]))
        elif code == 30:
            self.call(FN["CalcClient"], w)
        elif code == 31:
            answer[0] = T.s32(self.call(FN["Enable"], w, s16(a[1])))
        elif code == 32:
            self.call(FN["ReCalcValue"], w)
        elif code == 33:
            answer[0] = T.s16(self.call(FN["ReCalcThumbPos"], w, a[1], a[2]) & 0xFFFF)
        elif code == 34:
            self.call(FN["SetMinMax"], w, a[1], a[2])
        elif code == 35:
            answer[0] = T.s32(self.call(FN["SetValue"], w, a[1]))
        elif code == 36:
            self.call(FN["SetPage"], w, a[1])
        elif code == 37:
            answer[0] = T.s32(self.call(FN["GetValue"], w))
        elif code == 38:
            self.call(FN["AddInc"], w)
        elif code == 39:
            self.call(FN["AddDec"], w)
        elif code == 40:
            self.call(FN["AddThumb"], w)
        elif code == 41:
            self.call(FN["ClientClick"], w, s16(a[1]), s16(a[2]))
        elif code == 42:
            answer[0] = T.s32(self.call(FN["Handler"], w, a[1], a[2], a[3]))
        elif code == 43:
            answer[0] = T.s32(self.call(FN["WhatAmI"], w))
        elif code == 44:
            self.call(FN["BarDrawFunction"], w)
        elif code == 45:
            self.call(FN["BarClientDraw"], w)
        elif code == 46:   # the destructor: slot, flag
            self.call(FN["BarDtor"], w, a[1])
            self.kinds[a[0]] = KIND_NONE   # (gone, deleted or not: a destroyed window is not used again)
        elif code == 50:   # Change: slot, flags, depth, word 0x2c, the rectangle, record null, rectangle null
            rect = e.alloc(16, struct.pack(">I4h", CORE_VTABLE, *[s16(v) for v in a[4:8]]))
            record = e.alloc(0x28, b"\xa5" * 0x28)
            w16(e, record + 6, a[2])
            w16(e, record + 8, a[3])
            w32(e, record + 0x14, 0 if a[9] else rect)
            self.call(FN["Change"], s, a[1], 0 if a[8] else record)
        elif code == 51:
            self.call(FN["SpriteShow"], s, s16(a[1]))
        elif code == 52:
            rect = e.alloc(16, struct.pack(">I4h", CORE_VTABLE, 1, 2, 3, 4))
            self.call(FN["Draw"], s, s16(a[1]), rect, s16(a[2]))
        elif code == 53:
            self.call(FN["Refresh"], s)
        elif code == 54:
            self.call(FN["UpdateTextures"], s, 0)
        elif code == 55:
            self.call(FN["CreateTextures"], s, 0)
        elif code == 56:   # GetWritableDataArea: slot, first null, second null
            cells = e.alloc(8, struct.pack(">2i", 0x5A5A, 0x5A5B))
            result = self.call(FN["GetWritable"], s, 0 if a[1] else cells, 0 if a[2] else cells + 4)
            answer[0] = LAB_PIXELS if result == self.pixels and result != 0 else T.s32(result)
            answer[1], answer[2] = struct.unpack(">2i", e.read(cells, 8))
        elif code == 57:
            self.call(FN["SetPreDraw"], s, a[1])
        elif code == 58:
            self.call(FN["SpriteDtor"], s, a[1])
            if s16(a[1]) > 0:
                for offset in (0x54, 0x9C, 0xB4, 0xD4):
                    self.L.labels.pop(s + offset, None)   # (the parts' labels go with a deleted object)
            self.sprites[a[0]] = 0   # (gone, deleted or not)
        else:
            raise ValueError(code)
        return answer


# ======================================================================= the fakes (PowerPC side)


def make_fakes(e):
    """The fake draw objects' and the help text's vtables, in persistent memory."""
    if hasattr(e, "uisb_draw_vtable"):
        return

    def show(emu):
        emu.cl.rec("Draw.Show", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4))))

    def change(emu):
        this, flags, record = emu.r(3), emu.r(4), emu.r(5)
        first = struct.unpack(">i", emu.read(record, 4))[0] if record and flags & 8 else 0
        emu.cl.rec("Draw.Change", (emu.cl.lab(this), flags, first, 1 if record == 0 else 0))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.write(vt + 0x1C, struct.pack(">I", T.fake_function(e, show)))
    e.uisb_draw_vtable = vt

    vt = T.persist_alloc(e, 0x48)
    e.write(vt + 0x3C, struct.pack(">I", T.fake_function(e, lambda emu: emu.cl.rec("Help.OnHideOrDelete", (emu.cl.lab(emu.r(4)),)))))
    e.uisb_help_vtable = vt


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL_HELPERS)
    R = CL.recorder

    S.internal(SEND_MESSAGE, R("SendMessage", "Liii", ret="i"))
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(DEFAULT_HANDLER, R("DefaultHandler", "Liii", ret="i"))
    S.internal(THUMB_HANDLER, R("ThumbHandler", "Liii", ret=lambda e, L: 0))
    S.internal(W_DRAW_FUNCTION, R("DrawFunction", "L"))
    calc_client = R("CalcClientArea", "L")

    def calc_client_area(e):
        calc_client(e)
        this = e.r(3)
        e.write(this + 0x2C, bytes(e.read(this + 8, 8)))   # (the real one copies the rectangle to the client rectangle)

    S.internal(W_CALC_CLIENT, calc_client_area)
    S.internal(W_DTOR, R("Window.dtor", "D"))
    S.internal(W_CLIENT_DRAW, R("Window.ClientDraw", "L"))
    S.internal(W_SET_VALUE, R("Window.SetValue", "Li", ret=lambda e, L: 1))
    S.internal(W_DRAW_ME, R("DrawMe", "L", ret=lambda e, L: 1))

    def window_enable(e):
        this, on = e.r(3), T.s16(e.r(4))
        e.cl.rec("Window.Enable", (e.cl.lab(this), on))
        attributes, changed = e.u32(this + 0x44), 0
        if on == 0:
            if not attributes & 2:
                changed, attributes = 1, attributes | 2
        elif attributes & 2:
            changed, attributes = 1, attributes & ~2
        w32(e, this + 0x44, attributes)
        e.set_r(3, changed)

    S.internal(W_ENABLE, window_enable)

    def get_position(e):
        this, rect, subtract = e.r(3), e.r(4), T.s16(e.r(5))
        e.cl.rec("GetPosition", (e.cl.lab(this), subtract))
        if rect == 0:
            e.set_r(3, 0)
            return
        values = list(struct.unpack(">4h", e.read(this + 8, 8)))
        if subtract:
            offsets = struct.unpack(">4h", e.read(this + 0xF4, 8))
            values = [s16(v - o) for v, o in zip(values, offsets)]
        e.write(rect + 4, struct.pack(">4h", *values))
        e.set_r(3, 1)

    S.internal(W_GET_POSITION, get_position)

    def set_position(e):
        this, rect = e.r(3), e.r(4)
        if rect == 0:
            e.cl.rec("SetPosition", (e.cl.lab(this), 0, 0, 0, 0, 0))
        else:
            values = struct.unpack(">4h", e.read(rect + 4, 8))
            e.cl.rec("SetPosition", (e.cl.lab(this), 1) + values)
            e.write(this + 8, struct.pack(">4h", *values))
        e.set_r(3, 1)

    S.internal(W_SET_POSITION, set_position)

    def set_callback(e):
        this, callback = e.r(3), e.r(4)
        e.cl.rec("SetCallbackFunction", (e.cl.lab(this), callback_code(callback)))
        if callback:
            w32(e, this + 0x110, callback)
        e.set_r(3, 1)

    S.internal(W_SET_CALLBACK, set_callback)

    def create_object(e):
        this = e.r(3)
        kind, attributes, ident, left, top, right, bottom = T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)), T.s16(e.r(9)), T.s16(e.r(10))
        e.cl.rec("CreateObject", (e.cl.lab(this), kind, attributes, ident, left, top, right, bottom))
        child = e.us.window(e.cl.ret("CreateObject", -1))
        if child == 0 or e.u32(child + 0x5C) != 0:
            e.set_r(3, 0)
            return
        last = e.u32(this + 0x58)
        while last and e.u32(last + 0x50):
            last = e.u32(last + 0x50)
        w32(e, child + 0x5C, this)
        w32(e, child + 0x50, 0)
        w32(e, child + 0x54, last)
        w32(e, last + 0x50 if last else this + 0x58, child)
        w32(e, child + 0x88, ident)
        for offset in (8, 0x14, 0x2C):
            e.write(child + offset, struct.pack(">4h", left, top, right, bottom))
        e.set_r(3, child)

    S.internal(W_CREATE_OBJECT, create_object)

    # the draw object base class
    def draw_ctor(e):
        this, data = e.r(3), e.r(4)
        e.cl.rec("DrawObject.ctor", (e.cl.lab(this), e.cl.lab(data)))
        e.write(this + 4, bytes(0x34))
        w32(e, this, data)

    S.internal(DRAW_CTOR, draw_ctor)
    S.internal(DRAW_DTOR, R("DrawObject.dtor", "L"))

    # the engine's sprite surfaces and the map (imports)
    S.imp("__ct__15SSpriteGameDataFv", R("GameData.ctor", "L"))

    def surface_ctor(e):
        e.cl.rec("Surface.ctor", (e.cl.lab(e.r(3)),))
        e.write(e.r(3), bytes(0x18))

    S.imp("__ct__17SSpriteEngineDataFv", surface_ctor)
    S.imp("__dt__17SSpriteEngineDataFv", R("Surface.dtor", "L"))

    def list_ctor(e):
        this = e.r(3)
        e.cl.rec("List.ctor", (e.cl.lab(this),))
        w32(e, this, this + 0x20)   # the pointer to the link base the original stores at +0

    S.imp("__ct__11CSpriteListFv", list_ctor)
    S.imp("__dt__11CSpriteListFv", R("List.dtor", "L"))

    def list_init(e):
        this = e.r(3)
        e.cl.rec("List.Init", (e.cl.lab(this), T.s32(e.r(4))))
        slot = e.cl.lab(this) - LAB_LIST
        e.set_r(3, e.us.instances[slot] if 0 <= slot < SPRITES else 0)

    S.imp("Init__11CSpriteListFi", list_init)
    S.imp("SetUsedPosition__15SSpriteGameDataFii", R("SetUsedPosition", "Lii"))
    S.imp("SetUsedSize__15SSpriteGameDataFii", R("SetUsedSize", "Lii"))

    def surface_init(e):
        this, width, height = e.r(3), T.s32(e.r(4)), T.s32(e.r(5))
        e.cl.rec("Surface.Init", (e.cl.lab(this), width, height))
        made = e.cl.ret("Surface.Init", 1) != 0
        w16(e, this + 0xC, width)
        w16(e, this + 0xE, height)
        w32(e, this + 4, e.us.pixels if made else 0)
        e.set_r(3, 1 if made else 0)

    S.imp("Init__17SSpriteEngineDataFii", surface_init)
    S.imp("RefreshPartialTexture__17SSpriteEngineDataFv", R("RefreshPartialTexture", "L"))
    S.imp("Link__7CMapWhoFP12CMapLinkBasei", R("MapWho.Link", "LLi", ret=lambda e, L: 1))
    S.imp("Unlink__7CMapWhoFP12CMapLinkBasei", R("MapWho.Unlink", "LLi", ret=lambda e, L: 1))

    def operator_delete(e):
        block = e.r(3)
        if block:
            e.us.heap[2] += 1
            label = e.cl.lab(block)
            if label > 0:
                e.cl.rec("delete", (label,))
                e.cl.forget(block)

    S.imp("__dl__FPv", operator_delete)
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uisb_open", False):
        return
    import ctypes as C
    lib.tpw_uisb_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_uisb_dump_window.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uisb_dump_sprite.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uisb_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uisb_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._uisb_open = True


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        open_host(lib)
        lib.tpw_uisb_reset()
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        CL.host_begin(lib, {}, queues, first_new=FIRST_NEW)
        lib.tpw_uisb_labels()

    def op(self, code, a):
        C = self.C
        args = (C.c_int64 * 16)(*a, *([0] * (16 - len(a))))
        out = (C.c_int64 * 5)()
        self.lib.tpw_uisb_op(code, args, out)
        return list(out)

    def dump_state(self, step):
        C, lib = self.C, self.lib
        buf = (C.c_int64 * 256)()
        windows, sprites = [], []
        for slot in range(SLOTS):
            kind = step["kinds"][slot]
            if kind != KIND_NONE:
                lib.tpw_uisb_dump_window(slot, buf)
                windows.append(list(buf[:BAR_FIELDS if kind == KIND_BAR else WINDOW_FIELDS]))
            else:
                windows.append([0])
        for slot in range(SPRITES):
            if step["sprites"][slot]:
                lib.tpw_uisb_dump_sprite(slot, buf)
                sprites.append(list(buf[:SPRITE_FIELDS]))
            else:
                sprites.append([0])
        lib.tpw_uisb_dump_globals(buf)
        globals_ = list(buf[:GLOBAL_FIELDS])
        lib.tpw_uisb_heap(buf)
        return dict(windows=windows, sprites=sprites, globals=globals_, heap=list(buf[:4]), log=C.c_int32.in_dll(lib, "tpw_cl_n").value)


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
            ran.append(dict(op=op, kinds=list(world.kinds), sprites=[int(bool(s)) for s in world.sprites]))
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

import random as _random

TESTS = []

SMALL = [0, 0, 1, -1, 2, -3, 7, -8, 20, -33, 100, -250, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x12345, -0x12345]
SPARES = (7, 8, 9)   # windows the fake CreateObject hands out to a scroll bar's constructor (made first, alone)


def case_of(ops, q=None):
    case = dict(ops=ops)
    if q:
        case["q"] = q
    return case


def plain(slot, parent, attributes, ident, rect):
    return (1, (slot, parent, attributes, ident) + tuple(rect))


def bar(slot, parent, attributes, ident, rect):
    return (6, (slot, parent, attributes, ident) + tuple(rect))


def tree_ops(parents):
    """Op 2 for each slot of the dict {slot: parent slot or -1}: the links of a tree whose children are in the order the dict lists them
    (the order of their slots when it is built that way). A window not in the dict keeps the links it has."""
    ops = []
    for slot in parents:
        siblings = [t for t in parents if parents[t] == parents[slot]]
        at = siblings.index(slot)
        previous = siblings[at - 1] if at > 0 else -1
        following = siblings[at + 1] if at + 1 < len(siblings) else -1
        children = [t for t in parents if parents[t] == slot]
        ops.append((2, (slot, parents[slot], previous, following, children[0] if children else -1)))
    return ops


def world(parents, rects=None, attributes=None, ids=None, draws=None, handlers=None):
    """Windows 0.. made with these parents (-1 for a root), linked, with their attributes, draw objects and handlers set."""
    n = len(parents)
    rects = rects or [(s * 10, s * 8, s * 10 + 60, s * 8 + 50) for s in range(n)]
    attributes = attributes or [1] * n
    ids = ids or [100 + s for s in range(n)]
    draws = draws or [(-1, -1)] * n
    handlers = handlers or [1] * n
    ops = [plain(s, parents[s], attributes[s], ids[s], rects[s]) for s in range(n)]
    ops += tree_ops({s: parents[s] for s in range(n)})
    for s in range(n):
        ops.append((3, (s, attributes[s], 0)))
        ops.append((4, (s,) + tuple(draws[s])))
        ops.append((5, (s, handlers[s])))
    return ops


def bar_requests(attributes):
    """How many windows a scroll bar's constructor asks CreateObject for."""
    if attributes & 0x80 == 0:
        return 0
    return 1 if attributes & 0x40 else 3


def bar_world(attributes, rect=(10, 20, 30, 220), ident=55, spares=SPARES, extra=None, queue=None):
    """A root window (slot 0), a scroll bar (slot 1) under it, and the spares the bar's constructor takes for its buttons and thumb
    (linked as the bar's children, in the order CreateObject was asked for them). Returns the operations and the case's queues."""
    ops = [plain(s, -1, 1, 0, (0, 0, 10, 10)) for s in spares]
    ops.append(plain(0, -1, 1, 77, (0, 0, 300, 300)))
    ops.append(bar(1, 0, attributes, ident, rect))
    taken = list(spares)[:bar_requests(attributes)]
    ops += tree_ops(dict([(0, -1), (1, 0)] + [(t, 1) for t in taken]))
    q = {"CreateObject": list(spares[:bar_requests(attributes)]) + [-1]}
    if queue:
        q.update(queue)
    return ops + (extra or []), q


def thumb_slot(attributes, spares=SPARES):
    count = bar_requests(attributes)
    return spares[count - 1] if count else None


def rnd_rect(r, around=150):
    left, top = r.randint(-30, around), r.randint(-30, around)
    return (left, top, left + r.choice([0, 1, 2, r.randint(-5, 200), r.randint(1, 100)]),
            top + r.choice([0, 1, 2, r.randint(-5, 200), r.randint(1, 100)]))


# ======================================================================= test 1: GetWindow, SetPosition (two overloads)


def lookup_edges():
    # GetWindow: no children, one, several; the first of equal ids; ids beside the one asked for; negative and extreme ids
    flat = [-1, 0, 0, 0, 0, 0]
    for ids in ([5, 1, 2, 3, 4, 5], [5, 7, 7, 7, 7, 7], [5, 1, 2, 3, 4, 6], [0, 0, 0, 0, 0, 0], [0, -1, -1, 0x7FFFFFFF, -0x80000000, 3]):
        ops = world(flat, ids=ids)
        for asked in (5, 7, 1, 2, 3, 4, 6, 0, -1, 0x7FFFFFFF, -0x80000000, 8):
            ops.append((22, (0, asked)))
        yield case_of(ops)
    # a window with no child; a child's own children are not looked at
    deep = [-1, 0, 1, 2, 3, 4]
    yield case_of(world(deep, ids=[9, 8, 7, 6, 5, 4]) + [(22, (s, i)) for s in range(6) for i in (9, 8, 7, 6, 5, 4)])


def lookup_random(r):
    n = r.randint(1, 7)
    parents = [-1] + [r.randrange(s) for s in range(1, n)]
    ids = [r.choice([0, 1, 2, 3, -1, r.randint(-4, 4)]) for _ in range(n)]
    ops = world(parents, ids=ids)
    for _ in range(r.randint(1, 10)):
        ops.append((22, (r.randrange(n), r.choice([0, 1, 2, 3, -1, r.randint(-5, 5)]))))
    return case_of(ops)


def position_edges():
    base = world([-1, 0], rects=[(0, 0, 400, 400), (10, 20, 50, 80)])
    # the short pointers: both, one, none; at the edges of 16 bits; a window whose size does not fit in 16 bits when added
    for x, y in ((0, 0), (5, 6), (-7, 9), (0x7FFF, 0x7FFF), (-0x8000, -0x8000), (0x7FF0, -0x7FF0), (0x1234, 0x4321)):
        for x_null, y_null in ((0, 0), (1, 0), (0, 1), (1, 1)):
            yield case_of(base + [(23, (1, x, y, x_null, y_null))])
    for rect in ((0, 0, 0, 0), (0, 0, 0x7FFF, 0x7FFF), (-0x8000, -0x8000, 0x7FFF, 0x7FFF), (0x7FFF, 0x7FFF, -0x8000, -0x8000), (5, 5, 4, 4)):
        yield case_of(world([-1, 0], rects=[(0, 0, 100, 100), rect]) + [(23, (1, 0x7FFF, 0x7FFF, 0, 0)), (23, (1, -0x8000, 3, 0, 0)),
                                                                    (23, (1, 1, 1, 0, 1)), (23, (1, 1, 1, 1, 0))])
    # the four ints: cut to 16 bits each
    for values in ((0, 0, 0, 0), (1, 2, 3, 4), (-1, -2, -3, -4), (0x7FFF, 0x8000, 0xFFFF, 0x10000), (0x12345, -0x12345, 0x7FFFFFFF, -0x80000000),
                   (0x1FFFF, 0x20000, -0x18000, 0x18000)):
        yield case_of(base + [(24, (1,) + values), (24, (0,) + values)])


def position_random(r):
    n = r.randint(1, 5)
    ops = world([-1] + [r.randrange(s) for s in range(1, n)], rects=[rnd_rect(r) for _ in range(n)])
    for _ in range(r.randint(1, 8)):
        slot = r.randrange(n)
        if r.random() < 0.5:
            ops.append((23, (slot, r.choice(SMALL), r.choice(SMALL), r.choice([0, 0, 1]), r.choice([0, 0, 1]))))
        else:
            ops.append((24, (slot,) + tuple(r.choice(SMALL + [r.getrandbits(32) - 0x80000000]) for _ in range(4))))
    return case_of(ops)


TESTS.append(FnTest("InterfaceWindow::GetWindow", ui_ppc, ui_host, edge=lookup_edges, rnd=lookup_random))
TESTS.append(FnTest("InterfaceWindow::SetPosition (short*, short*) and (int, int, int, int)", ui_ppc, ui_host, edge=position_edges,
                    rnd=position_random))


# ======================================================================= test 2: Show, ShowAllDRAWOBJS

SPRITE = DRAWS   # the draw object number of sprite 0 (op 4); sprite k is SPRITE + k


def sprite_ops(slot, token=-1, created=1, shown=0, pixels=1, size=(8, 4), pixel_size=(16, 8)):
    """A sprite draw object made by its constructor, then given a state: created or not, shown or not."""
    return [(9, (slot, token)), (10, (slot, created, pixel_size[0], pixel_size[1], shown, 1, 2, 30, 40, 5, 3, size[0], size[1], pixels))]


def show_edges():
    chain = [-1, 0, 1, 2, 3, 4, 5]
    both = [(0, 1)] * 7
    # one window: each state, each value, no, one, two draw objects (fake) and handlers
    for attribute in (1, 0, 3, 2, 0x401, 0x10000001, 0x10000000):
        for draws in ((-1, -1), (0, -1), (-1, 1), (2, 3)):
            for handler in (0, 1, 2, 3):
                ops = world([-1], attributes=[attribute], draws=[draws], handlers=[handler])
                for value in (1, 0, 0, 1, -1, 0x7FFF, -0x8000, 5):
                    ops.append((20, (0, value)))
                yield case_of(ops, q={"DefaultHandler": [4, 5, 6]})
    # the draw message goes to the handler directly, with the value as a signed short
    for value in (1, -1, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x10001):
        yield case_of(world([-1, 0], handlers=[1, 3]) + [(21, (0, value)), (20, (0, value)), (20, (1, value))])
    # the whole chain: from the root, from the middle, a window hidden in the middle (its subtree is not visited when shown)
    yield case_of(world(chain, draws=both) + [(20, (0, 0)), (20, (0, 1)), (20, (3, 0)), (20, (3, 1)), (20, (6, 0)), (20, (6, 1))])
    for k in range(7):
        attributes = [1] * 7
        attributes[k] = 0
        yield case_of(world(chain, attributes=attributes, draws=both) + [(21, (0, 1)), (21, (0, 0)), (20, (0, 1)), (20, (k, 1)), (20, (k, 0)), (20, (0, 0))])
    # a parent that is hidden: a child shown under it changes only its own bit; hiding it later still tells the draw objects
    yield case_of(world([-1, 0, 1], attributes=[1, 0, 0], draws=[(0, 1)] * 3) + [(20, (2, 1)), (20, (1, 1)), (20, (2, 0)), (20, (1, 0)), (20, (0, 0)), (20, (2, 1))])
    # wide trees with hidden branches, the visit order of the draw objects and the handler
    wide = [-1, 0, 0, 0, 1, 1, 2, 2, 4]
    for attributes in ([1] * 9, [1, 1, 0, 1, 1, 1, 1, 1, 1], [1, 1, 1, 1, 0, 1, 1, 1, 1], [1, 0, 1, 1, 1, 1, 1, 1, 1]):
        yield case_of(world(wide, attributes=attributes, draws=[(i % 4, (i + 1) % 4) for i in range(9)], handlers=[1, 3, 1, 0, 1, 3, 1, 1, 1]) +
                      [(21, (0, 1)), (21, (0, 0)), (21, (1, 1)), (21, (0, -1))], q={"DefaultHandler": [1]})
    # sprites as draw objects: created and not, shown and not, with and without the surface's pixels
    for created in (0, 1):
        for shown in (0, 1, 3):
            ops = sprite_ops(0, 0, created, shown) + sprite_ops(1, 1, created, 1 - (shown != 0)) + world([-1, 0, 0], draws=[(SPRITE, -1), (-1, SPRITE + 1), (SPRITE, SPRITE + 1)])
            ops += [(21, (0, 1)), (21, (0, 0)), (20, (1, 0)), (20, (1, 1)), (20, (2, 1)), (20, (2, 0))]
            yield case_of(ops)
    # a window under a hidden parent whose draw object is a sprite
    yield case_of(sprite_ops(0, -1, 1, 0) + world([-1, 0], attributes=[0, 0], draws=[(-1, -1), (SPRITE, SPRITE)]) + [(20, (1, 1)), (20, (0, 0)), (20, (0, 1))])
    # a visit for a value that is nonzero but not 1
    yield case_of(sprite_ops(0, -1, 1, 0) + world([-1, 0], draws=[(SPRITE, 0), (SPRITE, 1)]) + [(21, (0, 2)), (21, (0, -5)), (21, (0, 0)), (21, (0, 0x4000))])
    # the attributes changed between visits
    yield case_of(world(wide, draws=[(0, 1)] * 9) + [(21, (0, 1)), (3, (1, 0, 0)), (21, (0, 1)), (21, (0, 0)), (3, (1, 1, 0)), (3, (0, 0x10000001, 0)), (21, (0, 1))])


def show_random(r):
    n = r.randint(1, 8)
    parents = [-1] + [s - 1 if r.random() < 0.4 else r.randrange(s) for s in range(1, n)]
    ops = []
    used = []
    for k in range(SPRITES):
        if r.random() < 0.6:
            ops += sprite_ops(k, r.choice([-1, 0, 1, 2]), r.choice([0, 1, 1]), r.choice([0, 1, 1, 7]), r.choice([0, 1, 2]))
            used.append(k)

    def draw_choice():
        if used and r.random() < 0.4:
            return SPRITE + r.choice(used)
        return r.choice([-1, -1, 0, 1, 2, 3])

    attributes = [(1 if r.random() < 0.75 else r.choice([0, 2, 8, 0x400])) | (r.choice([0x10000000, 0xF0000000, 0x30000]) if r.random() < 0.15 else 0)
                  for _ in range(n)]
    ops += world(parents, rects=[rnd_rect(r) for _ in range(n)], attributes=attributes, draws=[(draw_choice(), draw_choice()) for _ in range(n)],
                 handlers=[r.choice([0, 1, 1, 2, 3]) for _ in range(n)])
    for _ in range(r.randint(1, 12)):
        k = r.random()
        slot = r.randrange(n)
        value = r.choice([0, 1, 1, 0, -1, 2, 0x7FFF, -0x8000])
        if k < 0.45:
            ops.append((20, (slot, value)))
        elif k < 0.75:
            ops.append((21, (slot, value)))
        elif k < 0.9:
            ops.append((3, (slot, r.choice([0, 1, 1, 3, 0x10000001]), 0)))
        else:
            ops.append((4, (slot, draw_choice(), draw_choice())))
    return case_of(ops, q={"DefaultHandler": [r.choice([0, 1, -1]) for _ in range(4)]})


TESTS.append(FnTest("InterfaceWindow::Show, ShowAllDRAWOBJS", ui_ppc, ui_host, edge=show_edges, rnd=show_random))


# ======================================================================= test 3: the sprite draw object

SCREENS = [(0, 0), (1, 1), (1600, 1200), (2048, 1536), (1024, 768), (-1, -1), (32767, 32767), (-32768, -32768), (3, 7), (2047, 1535)]
FLAG_SETS = [0, 1, 2, 4, 8, 0x10, 0x12, 0x14, 0x16, 0x40, 0x80, 0x100, 0x80000, 0x10000, 0x1F, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 0x3, 0x5, 0x6,
             0x11, 0x13, 0x15, 0xC0, 0x80002, 0x80010]
RECTS = [(0, 0, 0, 0), (0, 0, 2048, 1536), (10, 20, 110, 70), (-50, -60, 50, 60), (100, 100, 50, 50), (1, 1, 2, 2), (0, 0, 1, 1),
         (-32768, -32768, 32767, 32767), (32767, 32767, -32768, -32768), (7, 11, 1033, 777), (2047, 1535, 2048, 1536), (-1, -1, 0, 0),
         (5, 6, 5 + 2047, 6 + 1535), (3, 1535, 1000, 1536)]
DEPTHS = [0, 1, 2, 5, 39, 40, 41, 200, -1, -5, 0x7FFF, -0x8000]


def change_op(slot, flags, depth=3, word=0x55, rect=(10, 20, 110, 70), record_null=0, rect_null=0):
    return (50, (slot, flags, depth, word) + tuple(rect) + (record_null, rect_null))


def sprite_ctor_edges():
    # every data token on every slot, made twice (the second replaces the first, which was never destroyed: the scripts never reuse a slot
    # without a destroy, so each slot is made once here)
    for token in (-1, 0, 1, 2):
        for slot in range(SPRITES):
            yield case_of([(9, (slot, token))])
    yield case_of([(9, (k, k % 3)) for k in range(SPRITES)])
    # the destructor: flag > 0 deletes (the label is forgotten), 0 and below do not; the parts are destroyed in either case
    for flag in (1, 2, 0x7FFF, 0, -1, -0x8000, 0x10000, 0x10001, -0x7FFF):
        yield case_of([(9, (0, 0)), (58, (0, flag))])
    yield case_of([(9, (0, -1)), (9, (1, 1)), (58, (1, 1)), (58, (0, 0)), (9, (1, 2)), (58, (1, 1))])
    # the constructor leaves the surface empty, one instance in the list, the game data's flags, and the surface not created
    yield case_of([(9, (0, -1)), (50, (0, 0x10, 3, 4, 10, 10, 20, 20, 0, 0)), (56, (0, 0, 0)), (51, (0, 1))])


def sprite_change_edges():
    # each flag set on a fresh object and on one whose surface exists, with the screens and a rectangle
    for flags in FLAG_SETS:
        for created in (0, 1):
            for screen in ((1600, 1200), (2048, 1536)):
                ops = [(7, (2, screen[0])), (7, (3, screen[1]))] + sprite_ops(0, -1, created, 0, pixels=0, size=(0, 0))
                ops += [change_op(0, flags), change_op(0, flags, depth=-3, word=-9, rect=(-4, 5, 600, 700))]
                yield case_of(ops)
    # the screen's size and the rectangle's against each other
    for screen in SCREENS:
        for rect in RECTS:
            ops = [(7, (2, screen[0])), (7, (3, screen[1])), (9, (0, -1)), change_op(0, 0x10, rect=rect), change_op(0, 0x10, rect=rect)]
            yield case_of(ops)
    # the depth: at least 1, and the position's z from it
    for depth in DEPTHS:
        for flags in (2, 0x12, 0x10, 6):
            yield case_of(sprite_ops(0, -1, 1, 0) + [change_op(0, flags, depth=depth)])
    yield case_of(sprite_ops(0, -1, 0, 0) + [change_op(0, 2, depth=5), change_op(0, 4, word=7), change_op(0, 0x12, depth=9), change_op(0, 0x10, rect=(1, 2, 3, 4))])
    # a record or rectangle that is NULL: the record is checked first; the rectangle pointer is read only when flag 0x10 is set
    for flags in (0, 2, 4, 0x10, 0x12):
        for record_null, rect_null in ((1, 0), (0, 1)):
            if rect_null and flags & 0x10:
                continue   # (the original reads through the NULL pointer: skipped)
            yield case_of(sprite_ops(0, -1, 1, 0) + [change_op(0, flags, record_null=record_null, rect_null=rect_null)])
            yield case_of([(9, (0, -1)), change_op(0, flags, record_null=record_null, rect_null=rect_null)])
    # the surface cannot be made (the engine's allocator failed): the object still counts as created
    yield case_of([(7, (2, 1600)), (7, (3, 1200)), (9, (0, -1)), change_op(0, 0x10), change_op(0, 0x10, rect=(1, 1, 5, 5)), (56, (0, 0, 0))], q={"Surface.Init": [0]})
    # a sequence on one object: the first rectangle makes the surface, later ones only move the sprite
    yield case_of([(7, (2, 1600)), (7, (3, 1200)), (9, (0, 1)), change_op(0, 4, word=3), change_op(0, 1), change_op(0, 0x10), change_op(0, 0x10, rect=(300, 400, 500, 600)),
                   change_op(0, 2, depth=10), change_op(0, 0x40), change_op(0, 0x14, depth=2), change_op(0, 0x10, rect=(0, 0, 0, 0)), (58, (0, 1))])


def sprite_change_random(r):
    ops = [(7, (2, r.choice([0, 1, 1600, 2048, r.randint(-40000, 40000), r.randint(1, 3000)]))), (7, (3, r.choice([0, 1, 1200, 1536, r.randint(1, 3000)])))]
    ops += sprite_ops(0, r.choice([-1, 0, 1]), r.choice([0, 0, 1]), r.choice([0, 1]), size=(r.randint(0, 5), r.randint(0, 5)))
    for _ in range(r.randint(1, 8)):
        rect = r.choice(RECTS) if r.random() < 0.5 else (r.randint(-3000, 3000), r.randint(-3000, 3000), r.randint(-3000, 3000), r.randint(-3000, 3000))
        flags = r.choice(FLAG_SETS) if r.random() < 0.6 else r.getrandbits(32) & r.choice([0xFFFFFFFF, 0x1F, 0xFF, 0x80012])
        ops.append(change_op(0, flags, depth=r.choice(DEPTHS), word=r.choice(SMALL), rect=rect, record_null=int(r.random() < 0.05)))
    return case_of(ops, q={"Surface.Init": [r.choice([0, 1, 1])]})


def sprite_small_edges():
    # Show: before the surface exists nothing happens (the state is not kept), then link when shown, unlink when hidden, nothing when the same
    for created in (0, 1):
        for shown in (0, 1, 2, -1, 0x4000):
            for value in (0, 1, -1, 2, 0x4000, 0x10000, 0x10001, -0x8000):
                yield case_of(sprite_ops(0, -1, created, shown) + [(51, (0, value)), (51, (0, value)), (51, (0, 0)), (51, (0, 1))])
    # GetWritableDataArea: the answer is the pixels, the sizes are the surface's (unsigned 16 bits), NULL arguments, before the surface
    for created in (0, 1):
        for pixels in (0, 1, 2):
            for size in ((0, 0), (1, 1), (640, 480), (65535, 65535), (32768, 40000)):
                for nulls in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    yield case_of(sprite_ops(0, -1, created, 0, pixels=pixels, size=size) + [(56, (0,) + nulls)])
    # the small members: Draw, Refresh, UpdateTextures, CreateTextures (once; again later), SetPreDrawCallback
    yield case_of([(7, (2, 640)), (7, (3, 480)), (9, (0, -1)), (52, (0, 0, 0)), (52, (0, -3, 7)), (53, (0,)), (54, (0,)), (55, (0,)), (55, (0,)), (53, (0,)), (56, (0, 0, 0)), (57, (0, 0x1234)),
                   (57, (0, 0)), (57, (0, -5)), (58, (0, 1))])
    for created in (0, 1):
        for pixel_size in ((0, 0), (16, 8), (-5, 200), (32767, -32768), (1, 1)):
            for failing in (0, 1):
                yield case_of(sprite_ops(0, -1, created, 0, pixel_size=pixel_size) + [(55, (0,)), (56, (0, 0, 0)), (54, (0,)), (53, (0,))], q={"Surface.Init": [0 if failing else 1]})
    # the slots do not disturb each other
    yield case_of(sprite_ops(0, 0, 1, 0, size=(3, 3)) + sprite_ops(1, 1, 0, 1, size=(5, 5)) + sprite_ops(2, 2, 1, 1, size=(7, 7)) +
                  [(51, (0, 1)), (51, (1, 1)), (51, (2, 0)), (56, (0, 0, 0)), (56, (1, 0, 0)), (56, (2, 0, 0)), (58, (0, 1)), (51, (2, 1)), (58, (2, 1)), (58, (1, 0))])


def sprite_small_random(r):
    ops = [(7, (2, r.choice([640, 1600, 2048]))), (7, (3, r.choice([480, 1200, 1536])))]
    live = []
    for _ in range(r.randint(2, 14)):
        k = r.random()
        if not live or k < 0.2:
            free = [s for s in range(SPRITES) if s not in live]
            if free:
                slot = r.choice(free)
                ops += sprite_ops(slot, r.choice([-1, 0, 1, 2]), r.choice([0, 1]), r.choice([0, 1]), pixels=r.choice([0, 1, 2]), size=(r.randint(0, 99), r.randint(0, 99)),
                                  pixel_size=(r.randint(-3, 99), r.randint(-3, 99)))
                live.append(slot)
            continue
        slot = r.choice(live)
        if k < 0.35:
            ops.append((51, (slot, r.choice([0, 1, 1, -1, 2]))))
        elif k < 0.5:
            ops.append((56, (slot, r.choice([0, 0, 0, 1]), r.choice([0, 0, 0, 1]))))
        elif k < 0.6:
            ops.append((55, (slot,)))
        elif k < 0.7:
            ops.append((r.choice([52, 53, 54]), (slot, r.choice(SMALL), r.choice(SMALL))))
        elif k < 0.8:
            ops.append((57, (slot, r.choice([0, 0x10, 0x7FFFFFFF]))))
        elif k < 0.9:
            ops.append(change_op(slot, r.choice(FLAG_SETS), depth=r.choice(DEPTHS), word=r.choice(SMALL), rect=r.choice(RECTS)))
        else:
            ops.append((58, (slot, r.choice([1, 1, 0, -1]))))
            live.remove(slot)
    return case_of(ops, q={"Surface.Init": [r.choice([0, 1, 1, 1]) for _ in range(3)]})


def sprite_ctor_random(r):
    """A sprite made with any data token and destroyed with any flag; a second one made and sometimes destroyed too."""
    ops = [(9, (0, r.choice([-1, 0, 1, 2]))), (9, (1, r.choice([-1, 0, 1, 2]))), (58, (0, r.choice([1, 0, -1, 7])))]
    if r.random() < 0.5:
        ops.append((58, (1, r.choice([1, 0, -1, 0x7FFF]))))
    return case_of(ops)


TESTS.append(FnTest("InterfaceDrawObjectSprite constructor and destructor", ui_ppc, ui_host, edge=sprite_ctor_edges, rnd=sprite_ctor_random))
TESTS.append(FnTest("InterfaceDrawObjectSprite::Change", ui_ppc, ui_host, edge=sprite_change_edges, rnd=sprite_change_random))
TESTS.append(FnTest("InterfaceDrawObjectSprite::Show, GetWritableDataArea, Draw, Refresh, UpdateTextures, CreateTextures, SetPreDrawCallback",
                    ui_ppc, ui_host, edge=sprite_small_edges, rnd=sprite_small_random))


# ======================================================================= test 4: the scroll bar

def style_of(attributes):
    return (1 if attributes & 0x10 else 2) | (4 if attributes & 0x40 else 0)


def words_op(slot, attributes, maximum=0, minimum=0, value=0, step=1, page=1):
    """The bar's words set directly (the style stays what the attributes made it)."""
    return (8, (slot, maximum, minimum, value, step, page, style_of(attributes)))


def manual_thumb(attributes, rect=(10, 40, 30, 80), parent_slot=1, extra_siblings=()):
    """A bar made without the attribute that makes its children, and a plain window with id 3 as its only child."""
    ops = [bar(1, 0, attributes, 55, (10, 20, 30, 220) if style_of(attributes) & 2 == 0 else (10, 20, 210, 40)),
           plain(2, 1, 1, 3, rect)]
    parents = {0: -1, 1: 0, 2: 1}
    for slot, ident in extra_siblings:
        ops.append(plain(slot, 1, 1, ident, (0, 0, 5, 5)))
        parents[slot] = 1
    return [plain(0, -1, 1, 77, (0, 0, 300, 300))] + ops + tree_ops(parents)


VERTICAL = (10, 20, 30, 220)      # 20 wide: the client area is 160 tall (from 40 to 200)
HORIZONTAL = (10, 20, 210, 40)    # 20 tall: the client area is 160 wide (from 30 to 190)
RANGES = [(0, 100), (0, 0), (5, 5), (100, 0), (-50, 50), (50, -50), (-100, -1), (0, 1), (1, 0), (0, 1000), (-1000, 1000), (7, 4000), (0, 0x3FFFF)]


def thumb_rect_for(attributes, length, bar_rect=None):
    """The thumb's rectangle: `length` long along the bar, at the start of the client area."""
    if style_of(attributes) & 2:
        return (30, 20, 30 + length, 40)
    return (10, 40, 30, 40 + length)


def thumb_script(attributes, length, minimum, maximum, value, step_override=None, page=10, calls=()):
    """A bar with its children made by its constructor (the thumb is the last spare), the thumb resized, the range and value set."""
    ops, q = bar_world(attributes, HORIZONTAL if style_of(attributes) & 2 else VERTICAL, spares=SPARES)
    thumb = thumb_slot(attributes)
    ops.append((11, (thumb, 0) + thumb_rect_for(attributes, length)))
    ops.append((34, (1, minimum, maximum)))
    ops.append((36, (1, page)))
    ops.append(words_op(1, attributes, maximum, minimum, value, 1 if maximum >= minimum else -1, page) if step_override is None else
               words_op(1, attributes, maximum, minimum, value, step_override, page))
    return ops + list(calls), q


def recalc_edges():
    modes = [(33, (1, -2, 0)), (33, (1, 1, 0)), (33, (1, 0, 0)), (33, (1, 2, 0)), (33, (1, 3, 0)), (33, (1, -1, 0)), (33, (1, -1, 50)),
             (33, (1, -1, -7)), (33, (1, -1, 100000)), (33, (1, -2, 0))]
    for attributes in (0x90, 0x80):
        for length in (40, 160, 200, 0, 1, 159):
            for minimum, maximum in RANGES:
                middle = (minimum + maximum) // 2
                for value in (minimum, maximum, middle):
                    ops, q = thumb_script(attributes, length, minimum, maximum, value, calls=modes)
                    yield case_of(ops, q)
    # the steps and pages the constructor does not make: 0, large, negative against the order of the range
    for step in (0, 5, -5, -1, 1, 1000):
        for page in (0, 1, 7, -7, 1000):
            ops, q = thumb_script(0x90, 40, 0, 100, 50, step_override=step, page=page, calls=[(33, (1, 1, 0)), (33, (1, 0, 0)), (33, (1, 2, 0)), (33, (1, 3, 0)), (33, (1, -1, 500)), (33, (1, -1, -500))])
            yield case_of(ops, q)
    # the value outside the range, as if the range had been changed after it was set
    for value in (-500, 500, -1, 101):
        ops, q = thumb_script(0x90, 40, 0, 100, value, calls=[(33, (1, -2, 0)), (33, (1, 1, 0)), (33, (1, -2, 0)), (33, (1, 3, 0))])
        yield case_of(ops, q)
    # the draw objects and the parent: both, one, none, sprites; a bar without a parent
    for draws in ((-1, -1), (0, -1), (-1, 1), (0, 1), (SPRITE, -1), (SPRITE, 2)):
        for with_parent in (1, 0):
            ops, q = thumb_script(0x90, 40, 0, 100, 30, calls=[(33, (1, 1, 0)), (33, (1, -2, 0)), (33, (1, 2, 0)), (33, (1, -1, 100))])
            ops = sprite_ops(0, -1, 1, 0) + ops + [(4, (1,) + draws)]
            if not with_parent:
                ops.append((2, (1, -1, -1, -1, SPARES[0])))
            ops += [(33, (1, 1, 0)), (33, (1, 3, 0)), (35, (1, 77)), (35, (1, 77))]
            yield case_of(ops, q)
    # the thumb's animation offsets are taken off its rectangle first (GetPosition with subtract)
    for offsets in ((0, 0, 0, 0), (3, 4, 5, 6), (-8, 9, 0x7FFF, -0x8000)):
        ops, q = thumb_script(0x90, 40, 0, 100, 30)
        ops.append((11, (thumb_slot(0x90), 2) + offsets))
        yield case_of(ops + [(33, (1, 1, 0)), (33, (1, 3, 0)), (32, (1,))], q)
    # a bar that is not the first child, and a thumb that is not the first child with the id
    ops = manual_thumb(0x90, extra_siblings=((3, 3), (4, 3)))
    yield case_of(ops + [words_op(1, 0x90, 100, 0, 50), (33, (1, 1, 0)), (33, (1, -2, 0))])
    ops = manual_thumb(0x90, rect=(10, 40, 30, 80), extra_siblings=((3, 4),))
    yield case_of(ops + [words_op(1, 0x90, 100, 0, 50), (34, (1, 0, 0)), (33, (1, 1, 0)), (34, (1, 0, 100)), (33, (1, 1, 0))])


def recalc_random(r):
    attributes = r.choice([0x90, 0x80, 0x90 | 0x40, 0x80 | 0x40, 0xB0, 0x91, 0x81])
    length = r.choice([0, 1, 40, 160, 200, r.randint(0, 250)])
    minimum, maximum = r.choice(RANGES) if r.random() < 0.6 else (r.randint(-2000, 2000), r.randint(-2000, 2000))
    value = r.choice([minimum, maximum, (minimum + maximum) // 2, r.randint(-3000, 3000)])
    calls = []
    for _ in range(r.randint(1, 10)):
        mode = r.choice([-2, -1, 0, 1, 2, 3])
        calls.append((33, (1, mode, r.choice([minimum, maximum, r.randint(-3000, 3000), 0]))))
        if r.random() < 0.2:
            calls.append((34, (1, r.randint(-100, 100), r.randint(-100, 100))))
        if r.random() < 0.15:
            calls.append((36, (1, r.randint(-30, 30))))
        if r.random() < 0.15:
            calls.append((35, (1, r.randint(-300, 300))))
    ops, q = thumb_script(attributes, length, minimum, maximum, value, step_override=r.choice([None, None, None, 1, -1, 2, 0, -4]), page=r.choice([1, 10, 33, -2, 0]), calls=calls)
    thumb = thumb_slot(attributes) or SPARES[0]
    if r.random() < 0.3:
        ops.append((4, (1, r.choice([-1, 0, 1, 2, 3]), r.choice([-1, -1, 0, 1]))))
    if r.random() < 0.2:
        ops.append((11, (thumb, 2) + tuple(r.choice(SMALL) for _ in range(4))))
    return case_of(ops + calls, q)


TESTS.append(FnTest("InterfaceScrollBar::ReCalcThumbPos, SetValue, SetMinMax, SetPageValue", ui_ppc, ui_host, edge=recalc_edges, rnd=recalc_random))


# ======================================================================= test 5: the constructor, CalcClientArea, the destructor, Add*

BAR_RECTS = [VERTICAL, HORIZONTAL, (0, 0, 0, 0), (5, 5, 6, 6), (-30, -40, -10, 70), (0x7000, 0x7000, 0x7FFF, 0x7FFF), (10, 20, 30, 20),
             (100, 100, 50, 50), (-0x8000, -0x8000, 0x7FFF, 0x7FFF), (0, 0, 21, 9), (0, 0, 9, 21), (3, 3, 4, 100)]


def spare_ops(attributes=1, draws=None, spares=SPARES):
    """The windows CreateObject hands out, made alone, in the state asked for, with draw objects when asked."""
    ops = [plain(s, -1, attributes, 0, (0, 0, 10, 10)) for s in spares]
    if draws:
        ops += [(4, (s,) + tuple(draws)) for s in spares]
    return ops


def ctor_script(attributes, rect, queue, parent=0, spare_attributes=1, draws=None):
    ops = spare_ops(spare_attributes, draws) + [plain(0, -1, 1, 77, (0, 0, 300, 300))]
    ops.append(bar(1, parent, attributes, 55, rect))
    taken = [t for t in queue[:bar_requests(attributes)] if t >= 0]
    parents = {0: -1, 1: 0 if parent == 0 else -1}
    parents.update({t: 1 for t in taken})
    ops += tree_ops(parents)
    return ops, {"CreateObject": queue + [-1]}


def bar_ctor_edges():
    for base in (0, 1, 3, 0x401):
        for bits in range(8):
            attributes = base | (0x10 if bits & 1 else 0) | (0x40 if bits & 2 else 0) | (0x80 if bits & 4 else 0)
            for rect in BAR_RECTS:
                for queue in (list(SPARES), [-1, -1, -1], [SPARES[0], -1, SPARES[2]]):
                    ops, q = ctor_script(attributes, rect, queue, spare_attributes=0 if rect == VERTICAL else 1)
                    yield case_of(ops + [(37, (1,)), (43, (1,))], q)
    # the children appear hidden and are shown by the constructor, under a hidden parent too; they have draw objects
    for parent_attribute in (1, 0):
        for draws in ((0, 1), (SPRITE, -1)):
            ops = sprite_ops(0, -1, 1, 0) + spare_ops(0, draws) + [plain(0, -1, parent_attribute, 77, (0, 0, 300, 300)), bar(1, 0, 0x90, 55, VERTICAL)]
            ops += tree_ops({0: -1, 1: 0, 7: 1, 8: 1, 9: 1})
            yield case_of(ops, {"CreateObject": list(SPARES) + [-1]})
    # a bar without a parent
    ops, q = ctor_script(0x90, VERTICAL, list(SPARES), parent=-1)
    yield case_of(ops, q)


def bar_ctor_random(r):
    attributes = r.choice([0, 1, 3, 0x401]) | r.choice([0, 0x10]) | r.choice([0, 0, 0x40]) | r.choice([0, 0x80, 0x80])
    rect = r.choice(BAR_RECTS) if r.random() < 0.5 else rnd_rect(r)
    queue = [r.choice(SPARES + (-1,)) for _ in range(3)]
    if r.random() < 0.7:
        queue = list(SPARES)
    ops, q = ctor_script(attributes, rect, queue, parent=r.choice([0, 0, -1]), spare_attributes=r.choice([0, 1]),
                         draws=r.choice([None, (0, 1), (-1, 2)]))
    return case_of(ops + [(8, (1, r.choice(SMALL), r.choice(SMALL), r.choice(SMALL), r.choice([1, -1, 0, 3]), r.choice([1, 5, -2]), r.choice([1, 2, 5, 6, 3, 0, 7])))] +
                   [(30, (1,)), (37, (1,))], q)


def client_edges():
    # every style value (the bits 1, 2 and 4; 4 wins, then 2, else the vertical layout) against every rectangle
    for style in (0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 9, -1, 0x7FFF, -0x8000):
        ops, q = ctor_script(0x10, VERTICAL, [-1, -1, -1])
        for rect in BAR_RECTS:
            ops.append((11, (1, 0) + rect))
            ops.append((8, (1, 0, 0, 0, 1, 1, style)))
            ops.append((30, (1,)))
        yield case_of(ops, q)


def small_bar_edges():
    # the accessors and the empty members, the page and the value
    for value in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        ops, q = ctor_script(0x10, VERTICAL, [-1, -1, -1])
        yield case_of(ops + [(36, (1, value)), (8, (1, 5, 0, value, 1, 2, 1)), (37, (1,)), (43, (1,)), (44, (1,)), (45, (1,)), (36, (1, 3)), (37, (1,))], q)
    # the destructor: deleted, not deleted. A bar with children or links cannot be destroyed in the test (what points at it would keep the
    # pointer); a bar alone can, with or without the buttons and thumb its constructor makes (they stay behind, as windows of their own)
    for flag in (1, 0, -1, 2, 0x7FFF, 0x10000, -0x8000, 0x10001):
        for attributes in (0x10, 0x90, 0x40, 0xD0):
            ops = spare_ops(1) + [bar(1, -1, attributes, 55, VERTICAL), (46, (1, flag))]
            yield case_of(ops, {"CreateObject": list(SPARES) + [-1]})
    ops = spare_ops(1) + [bar(1, -1, 0x10, 55, VERTICAL), (46, (1, 1)), bar(1, -1, 0x10, 56, HORIZONTAL), (46, (1, 0)), bar(1, -1, 0x90, 57, VERTICAL), (37, (1,)), (46, (1, 1))]
    yield case_of(ops, {"CreateObject": list(SPARES) + [-1]})
    # a bar with a parent link or children is not destroyed (skipped), the bar is still there afterwards
    ops, q = ctor_script(0x90, VERTICAL, list(SPARES))
    yield case_of(ops + [(46, (1, 1)), (37, (1,))], q)


def add_edges():
    # the three creators on each style, with the children there or not; called again (CreateObject gives a window with a parent: NULL)
    for style_bits in (0x10, 0, 0x50, 0x40):
        for rect in (VERTICAL, HORIZONTAL, (0, 0, 0x7FE0, 0x7FFF), (-0x8000, -0x8000, -0x7FF0, -0x7FF0), (5, 5, 5, 5)):
            for spare_attributes in (1, 0):
                for queue in (list(SPARES), [-1, 8, -1], [9, 9, 9]):
                    ops = spare_ops(spare_attributes, (0, 1)) + [plain(0, -1, 1, 77, (0, 0, 300, 300)), bar(1, 0, style_bits, 55, rect)]
                    ops += tree_ops({0: -1, 1: 0})
                    ops += [(38, (1,)), (39, (1,)), (40, (1,)), (38, (1,)), (40, (1,))]
                    yield case_of(ops, {"CreateObject": queue + [-1]})
    # the thumb gets the thumb handler and the bar keeps its own; a bar hidden or under a hidden parent
    for parent_attribute, bar_attribute in ((0, 0x91), (1, 0x90), (0, 0x90)):
        ops = spare_ops(0, (0, 1)) + [plain(0, -1, parent_attribute, 77, (0, 0, 300, 300)), bar(1, 0, bar_attribute, 55, VERTICAL)]
        ops += tree_ops({0: -1, 1: 0})
        yield case_of(ops + [(40, (1,)), (38, (1,)), (39, (1,))], {"CreateObject": list(SPARES) + [-1]})


def add_random(r):
    attributes = r.choice([0x10, 0, 0x50, 0x40]) | r.choice([0, 1])
    parent = r.choice([0, -1])
    ops = spare_ops(r.choice([0, 1]), r.choice([None, (0, 1)]))
    ops += [plain(0, -1, r.choice([0, 1]), 77, (0, 0, 300, 300)), bar(1, parent, attributes, 55, rnd_rect(r))]
    ops += tree_ops({0: -1, 1: parent})
    queue = [r.choice(list(SPARES) + [-1]) for _ in range(4)]
    for _ in range(r.randint(1, 6)):
        ops.append((r.choice([38, 39, 40, 30]), (1,)))
    return case_of(ops, {"CreateObject": queue + [-1]})


def enable_edges():
    # a bar with children (its buttons and thumb, hidden or not, enabled or disabled), and a bar inside a bar
    for child_attributes in (1, 0, 3, 2):
        for auto_redraw in (0, 1, 5):
            for value in (0, 1, -1, 7):
                ops = spare_ops(child_attributes) + [plain(0, -1, 1, 77, (0, 0, 300, 300)), bar(1, 0, 0x90, 55, VERTICAL), (7, (1, auto_redraw))]
                ops += tree_ops({0: -1, 1: 0, 7: 1, 8: 1, 9: 1})
                ops += [(31, (1, value)), (31, (1, value)), (31, (1, 1 if value == 0 else 0)), (31, (1, value)), (31, (1, 0)), (31, (1, 0)), (31, (1, 1)), (31, (1, 1))]
                yield case_of(ops, {"CreateObject": list(SPARES) + [-1]})
    # the bar's own attribute bit 2 decides whether a request does anything; a bar inside a bar enables its own children
    for outer in (1, 3):
        for inner in (1, 3):
            ops = [plain(0, -1, 1, 77, (0, 0, 300, 300)), bar(1, 0, 0x10 | outer, 55, VERTICAL), bar(2, 1, 0x10 | inner, 56, (0, 0, 20, 100)), plain(3, 2, 1, 9, (0, 0, 5, 5)),
                   plain(4, 1, 3, 9, (0, 0, 5, 5))]
            ops += tree_ops({0: -1, 1: 0, 2: 1, 4: 1, 3: 2})
            ops += [(3, (1, outer, 0)), (3, (2, inner, 0)), (31, (1, 0)), (31, (1, 1)), (31, (1, 1)), (31, (2, 0)), (31, (1, 0)), (31, (1, 1))]
            yield case_of(ops)
    # the shorts: the value tested is the low 16 bits
    for value in (0x10000, 0x10001, -0x8000, 0x7FFF, 0xFFFF):
        ops = [plain(0, -1, 1, 77, (0, 0, 300, 300)), bar(1, 0, 0x10, 55, VERTICAL), plain(2, 1, 3, 5, (0, 0, 5, 5))]
        ops += tree_ops({0: -1, 1: 0, 2: 1})
        yield case_of(ops + [(31, (1, value)), (31, (1, value)), (31, (1, 0)), (31, (1, value))])


def enable_random(r):
    n = r.randint(2, 7)
    ops = [plain(0, -1, 1, 77, (0, 0, 300, 300))]
    parents = {0: -1}
    kinds = {}
    for slot in range(1, n):
        parent = r.randrange(slot) if r.random() < 0.6 else (slot - 1)
        attributes = r.choice([1, 1, 3, 0, 2]) | (0x10 if r.random() < 0.5 else 0)
        if r.random() < 0.55:
            ops.append(bar(slot, parent, attributes, 50 + slot, rnd_rect(r)))
            kinds[slot] = KIND_BAR
        else:
            ops.append(plain(slot, parent, attributes, 50 + slot, rnd_rect(r)))
        parents[slot] = parent
    ops += tree_ops(parents)
    for _ in range(r.randint(1, 10)):
        bars = [s for s in kinds]
        if not bars:
            break
        k = r.random()
        slot = r.choice(bars)
        if k < 0.6:
            ops.append((31, (slot, r.choice([0, 1, 1, -1]))))
        elif k < 0.75:
            ops.append((7, (1, r.choice([0, 1]))))
        else:
            ops.append((3, (r.choice(list(parents)), r.choice([1, 3, 0, 2]), 0)))
    return case_of(ops)


TESTS.append(FnTest("InterfaceScrollBar constructor, AddIncButton, AddDecButton, AddThumbTrack", ui_ppc, ui_host, edge=lambda: list(bar_ctor_edges()) + list(add_edges()),
                    rnd=lambda r: bar_ctor_random(r) if r.random() < 0.5 else add_random(r)))
TESTS.append(FnTest("InterfaceScrollBar::CalcClientArea, destructor, accessors", ui_ppc, ui_host, edge=lambda: list(client_edges()) + list(small_bar_edges()),
                    rnd=bar_ctor_random))
TESTS.append(FnTest("InterfaceScrollBar::Enable", ui_ppc, ui_host, edge=enable_edges, rnd=enable_random))


# ======================================================================= test 6: ClientClick, ReCalcValue

def click_edges():
    for attributes in (0x90, 0x80):
        for length in (40, 160, 0, 200):
            for minimum, maximum in ((0, 100), (100, 0), (0, 0), (-50, 50)):
                for value in (minimum, maximum, (minimum + maximum) // 2):
                    ops, q = thumb_script(attributes, length, minimum, maximum, value, page=10)
                    if style_of(attributes) & 2:
                        thumb = thumb_rect_for(attributes, length)
                        points = [(thumb[2] - 1, 25), (thumb[2], 25), (thumb[2] + 1, 25), (thumb[0], 25), (0, 0), (-1, 0), (0x7FFF, 0), (-0x8000, 5), (thumb[2] + 40, -7), (31, 0x7FFF)]
                    else:
                        thumb = thumb_rect_for(attributes, length)
                        points = [(15, thumb[3] - 1), (15, thumb[3]), (15, thumb[3] + 1), (15, thumb[1]), (0, 0), (0, -1), (0, 0x7FFF), (5, -0x8000), (-7, thumb[3] + 40), (0x7FFF, 41)]
                    ops += [(41, (1,) + point) for point in points]
                    yield case_of(ops, q)
    # the 16-bit difference: a thumb at the end of the range of shorts against points at the other end
    for bottom in (0x7FFF, -0x8000, 0, 5):
        for y in (0x7FFF, -0x8000, 0, -1, 1):
            ops = manual_thumb(0x90, rect=(10, bottom - 40 if bottom > -0x7FF0 else bottom, 30, bottom)) + [words_op(1, 0x90, 100, 0, 50), (41, (1, 0, y))]
            yield case_of(ops)
    for right in (0x7FFF, -0x8000, 0, 5):
        for x in (0x7FFF, -0x8000, 0, -1, 1):
            ops = manual_thumb(0x80, rect=(right - 40 if right > -0x7FF0 else right, 20, right, 40)) + [words_op(1, 0x80, 100, 0, 50), (41, (1, x, 0))]
            yield case_of(ops)
    # no thumb: nothing happens
    for attributes in (0x90, 0x80, 0x10, 0):
        ops, q = ctor_script(attributes, VERTICAL, [-1, -1, -1])
        yield case_of(ops + [words_op(1, attributes, 100, 0, 50), (41, (1, 5, 5)), (22, (1, 3))], q)


def click_random(r):
    attributes = r.choice([0x90, 0x80, 0x90 | 0x40, 0x80 | 0x40])
    minimum, maximum = r.choice(RANGES)
    ops, q = thumb_script(attributes, r.choice([0, 1, 40, 160, 200, r.randint(0, 250)]), minimum, maximum, r.choice([minimum, maximum, (minimum + maximum) // 2]),
                          step_override=r.choice([None, None, 1, -1, 3, 0]), page=r.choice([1, 10, -3, 0, 40]))
    for _ in range(r.randint(1, 8)):
        ops.append((41, (1, r.choice(SMALL + [r.randint(0, 220)]), r.choice(SMALL + [r.randint(0, 220)]))))
        if r.random() < 0.3:
            ops.append((34, (1, r.randint(-100, 100), r.randint(-100, 100))))
    return case_of(ops, q)


def value_edges():
    for attributes in (0x90, 0x80):
        for length in (40, 160, 0, 200, 1, 159):
            for minimum, maximum in ((0, 100), (100, 0), (0, 0), (-50, 50), (5, 5), (0, 0x3FFFF), (-1000, 1000)):
                for value in (minimum, (minimum + maximum) // 2):
                    ops, q = thumb_script(attributes, length, minimum, maximum, value)
                    thumb = thumb_slot(attributes)
                    style = style_of(attributes)
                    start = 30 if style & 2 else 40
                    for offset in (0, 1, 7, 60, 100, 159, 160, 161, -5):
                        if style & 2:
                            ops.append((11, (thumb, 0, start + offset, 20, start + offset + length, 40)))
                        else:
                            ops.append((11, (thumb, 0, 10, start + offset, 30, start + offset + length)))
                        ops.append((32, (1,)))
                    yield case_of(ops, q)
    # the animation offsets are taken off the thumb's rectangle, and the parent is told when the value changed
    for offsets in ((0, 0, 0, 0), (2, 3, 4, 5), (-7, 20, 0x7FFF, 1)):
        ops, q = thumb_script(0x90, 40, 0, 100, 30)
        ops += [(11, (thumb_slot(0x90), 2) + offsets), (32, (1,)), (32, (1,)), (11, (thumb_slot(0x90), 0, 10, 100, 30, 140)), (32, (1,)), (32, (1,))]
        yield case_of(ops, q)
    # a bar without a parent, and with a track that is empty (skipped)
    ops, q = thumb_script(0x90, 160, 0, 100, 30)
    ops += [(32, (1,)), (2, (1, -1, -1, -1, SPARES[0]))]
    yield case_of(ops, q)


def value_random(r):
    attributes = r.choice([0x90, 0x80])
    minimum, maximum = r.choice(RANGES) if r.random() < 0.7 else (r.randint(-500, 500), r.randint(-500, 500))
    ops, q = thumb_script(attributes, r.choice([0, 1, 40, 100, 160, 200]), minimum, maximum, r.randint(-100, 300), step_override=r.choice([None, 1, -1]))
    thumb = thumb_slot(attributes)
    for _ in range(r.randint(1, 8)):
        offset = r.randint(-20, 200)
        if style_of(attributes) & 2:
            ops.append((11, (thumb, 0, 30 + offset, 20, 30 + offset + r.randint(0, 200), 40)))
        else:
            ops.append((11, (thumb, 0, 10, 40 + offset, 30, 40 + offset + r.randint(0, 200))))
        ops.append((32, (1,)))
        if r.random() < 0.3:
            ops.append((33, (1, r.choice([-2, -1, 0, 1, 2, 3]), r.randint(-200, 300))))
    return case_of(ops, q)


TESTS.append(FnTest("InterfaceScrollBar::ClientClick", ui_ppc, ui_host, edge=click_edges, rnd=click_random))
TESTS.append(FnTest("InterfaceScrollBar::ReCalcValue", ui_ppc, ui_host, edge=value_edges, rnd=value_random))


# ======================================================================= test 7: UI_DefaultScrollBarCallback

MESSAGES = [3, -1, 0x100, 0x10008, 0x10009, 0x1000A, 0x1000B, 0x1000C, 0x1000D, 0x11006, 0, 1, 2, 4, 5, 0x10, 0x11, 0x1E, 0x10000, 0x10001, 0x10007,
            0x1000E, 0x11005, 0x11007, 0x7FFFFFFF, -0x80000000, -2, 0x1001E, 0x101, 0xFF, 0x10002, 0x10003, 0x10004, 0x10005]
ARGS = [0, 1, 2, 3, -1, 5, 100, -100, 0x7FFFFFFF, -0x80000000]


def pack(x, y):
    """A click's position as the message carries it: x in the high half, y in the low."""
    return T.s32(((x & 0xFFFF) << 16) | (y & 0xFFFF))


def handler_edges():
    for attributes in (0x90, 0x80):
        for auto_redraw in (0, 1):
            for message in MESSAGES:
                ops, q = thumb_script(attributes, 40, 0, 100, 30, page=10)
                ops.append((7, (1, auto_redraw)))
                q = dict(q)
                q["DefaultHandler"] = [3, -4, 9]
                bs = [0, 1, pack(15, 50), pack(25, 100), pack(-5, -7), pack(0x7FFF, -0x8000), -1, 0x80000000 - 0x100000000, 0x12345678]
                for a in ARGS:
                    ops.append((42, (1, message, a, bs[a % len(bs)] if a in (0, 1, 2, 3) else pack(a & 0x7FFF, 150))))
                yield case_of(ops, q)
    # the click against the thumb, as the message 0x11006 carries it, in the places the track and the thumb are
    for attributes in (0x90, 0x80):
        for length in (40, 160):
            for minimum, maximum in ((0, 100), (100, 0)):
                ops, q = thumb_script(attributes, length, minimum, maximum, 50, page=10)
                thumb = thumb_rect_for(attributes, length)
                for x, y in ((15, 45), (15, 100), (15, 199), (35, 25), (100, 25), (189, 30), (0, 0), (-1, -1), (30000, 30000), (-30000, -30000), (thumb[2], thumb[3]), (thumb[0], thumb[1])):
                    ops.append((42, (1, 0x11006, 0, pack(x, y))))
                yield case_of(ops, q)
    # the messages that move the thumb with no thumb to move: skipped except for the click, the draw messages and the rest
    for attributes in (0x90, 0x10):
        ops, q = ctor_script(attributes, VERTICAL, [-1, -1, -1])
        ops.append(words_op(1, attributes, 100, 0, 50))
        for message in (3, -1, 0x11006, 0x100, 0x1000D, 0x10008):
            for auto_redraw in (0, 1):
                ops += [(7, (1, auto_redraw)), (42, (1, message, 1, pack(5, 5)))]
        yield case_of(ops, q)
    # the wheel: the value less the first argument, cut to the ends; the draw message with the thumb's draw objects
    ops, q = thumb_script(0x90, 40, 0, 100, 50, page=10)
    ops += [(4, (1, 0, 1))] + [(42, (1, 0x1000D, a, 0)) for a in (1, -1, 5, 100, -100, 0, 0x7FFFFFFF, -0x7FFFFFFF)] + [(42, (1, 3, 0, 0)), (42, (1, 3, 1, 1))]
    yield case_of(ops, q)
    # every message to a bar whose handler is other than the bar's own does not reach this function: here the thumb's window gets message 3 through DrawMe
    ops, q = thumb_script(0x90, 40, 0, 100, 50)
    ops += [(4, (thumb_slot(0x90), 0, 1)), (42, (1, 3, 0, 0)), (42, (1, -1, 0, 0)), (7, (1, 1)), (42, (1, -1, 0, 0)), (42, (1, 3, 0, 0))]
    yield case_of(ops, q)


def handler_random(r):
    attributes = r.choice([0x90, 0x80, 0x90 | 0x40])
    minimum, maximum = r.choice(RANGES)
    ops, q = thumb_script(attributes, r.choice([0, 40, 160, 200]), minimum, maximum, r.choice([minimum, maximum, (minimum + maximum) // 2]), step_override=r.choice([None, None, 1, -1, 3]),
                          page=r.choice([1, 10, -3]))
    ops.append((7, (1, r.choice([0, 1]))))
    if r.random() < 0.4:
        ops.append((4, (1, r.choice([-1, 0, 1]), r.choice([-1, 0, 1]))))
    for _ in range(r.randint(1, 12)):
        message = r.choice(MESSAGES)
        a = r.choice(ARGS)
        b = pack(r.randint(0, 220), r.randint(0, 220)) if r.random() < 0.7 else r.choice([0, 1, -1, 0x7FFFFFFF, r.getrandbits(32) - 0x80000000])
        ops.append((42, (1, message, a, b)))
        if r.random() < 0.2:
            ops.append((34, (1, r.randint(-100, 100), r.randint(-100, 100))))
    q = dict(q)
    q["DefaultHandler"] = [r.choice([0, 1, -1, 77]) for _ in range(4)]
    return case_of(ops, q)


TESTS.append(FnTest("UI_DefaultScrollBarCallback", ui_ppc, ui_host, edge=handler_edges, rnd=handler_random))


# ======================================================================= test 8: scripts of 200 operations over the whole world

def long_world(r):
    """Four sprites, a root, a bar with its children made by the constructor (the spares), more windows, and a second bar whose thumb is
    a window of the script's own."""
    ops = []
    for k in range(SPRITES):
        ops += sprite_ops(k, r.choice([-1, 0, 1, 2]), r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([0, 1, 2]), size=(r.randint(0, 99), r.randint(0, 99)),
                          pixel_size=(r.randint(0, 99), r.randint(0, 99)))
    ops += spare_ops(r.choice([0, 1]), r.choice([None, (0, 1)]))
    ops.append(plain(0, -1, r.choice([1, 1, 1, 0]), 77, (0, 0, 300, 300)))
    attributes = r.choice([0x90, 0x80, 0x90 | 0x40, 0x80 | 0x40, 0x91])
    ops.append(bar(1, 0, attributes, 55, HORIZONTAL if style_of(attributes) & 2 else VERTICAL))
    ops.append(plain(2, 0, r.choice([1, 3, 0]), 12, rnd_rect(r)))
    ops.append(plain(3, 1, r.choice([1, 3]), 13, rnd_rect(r)))
    ops.append(plain(4, 2, 1, 14, rnd_rect(r)))
    attributes2 = r.choice([0x10, 0x00, 0x50])
    ops.append(bar(6, 0, attributes2, 56, rnd_rect(r) if r.random() < 0.3 else (VERTICAL if style_of(attributes2) & 2 == 0 else HORIZONTAL)))
    ops.append(plain(5, 6, 1, 3, thumb_rect_for(attributes2, r.choice([0, 40, 160, 200]))))
    taken = list(SPARES)[:bar_requests(attributes)]
    parents = dict([(0, -1), (1, 0)] + [(t, 1) for t in taken] + [(2, 0), (6, 0), (3, 1), (4, 2), (5, 6)])
    ops += tree_ops(parents)
    minimum, maximum = r.choice(RANGES)
    ops += [(34, (1, minimum, maximum)), (34, (6, r.randint(-50, 50), r.randint(-50, 50))), (36, (1, r.choice([1, 10, 33]))), (36, (6, r.choice([1, 5, -2])))]
    ops += [(4, (s, r.choice([-1, -1, 0, 1, 2, 3, SPRITE, SPRITE + 1]), r.choice([-1, -1, 0, 1, SPRITE + 2]))) for s in (0, 1, 2, 3, 4, 5, 6)]
    return ops, {"CreateObject": list(SPARES[:bar_requests(attributes)]) + [-1]}, [1, 6]


def long_op(r):
    bars = [1, 6]
    windows = [0, 1, 2, 3, 4, 5, 6] + list(SPARES)
    k = r.random()
    if k < 0.08:
        return (20, (r.choice(windows), r.choice([0, 1, 1, -1, 2])))
    if k < 0.14:
        return (21, (r.choice(windows), r.choice([0, 1, 1, -1, 2])))
    if k < 0.17:
        return (22, (r.choice(windows), r.choice([0, 1, 2, 3, 12, -1])))
    if k < 0.2:
        return (23, (r.choice(windows), r.choice(SMALL), r.choice(SMALL), r.choice([0, 0, 1]), r.choice([0, 0, 1])))
    if k < 0.23:
        return (24, (r.choice(windows),) + tuple(r.choice(SMALL) for _ in range(4)))
    if k < 0.4:
        bar_slot = r.choice(bars)
        return (33, (bar_slot, r.choice([-2, -1, 0, 1, 2, 3]), r.choice([0, 50, 100, -7, 1000])))
    if k < 0.46:
        return (r.choice([30, 32, 36, 37, 43, 44, 45]), (r.choice(bars), r.choice(SMALL)))
    if k < 0.52:
        return (31, (r.choice(bars), r.choice([0, 1, 1, -1])))
    if k < 0.56:
        return (r.choice([34, 35]), (r.choice(bars), r.choice([0, 10, 100, -50]), r.choice([0, 100, 20, -50])))
    if k < 0.6:
        return (r.choice([38, 39, 40]), (r.choice(bars),))
    if k < 0.66:
        return (41, (r.choice(bars), r.choice(SMALL + [30, 100]), r.choice(SMALL + [30, 100])))
    if k < 0.74:
        return (42, (r.choice(bars), r.choice(MESSAGES), r.choice(ARGS), pack(r.randint(0, 220), r.randint(0, 220))))
    if k < 0.77:
        return (r.choice([32, 32, 33]), (r.choice(bars), 3, 0))
    if k < 0.83:
        sprite = r.randrange(SPRITES)
        return r.choice([(51, (sprite, r.choice([0, 1, -1]))), (56, (sprite, 0, 0)), (55, (sprite,)), (53, (sprite,)), (54, (sprite,)),
                         change_op(sprite, r.choice(FLAG_SETS), depth=r.choice(DEPTHS), word=r.choice(SMALL), rect=r.choice(RECTS)), (57, (sprite, r.choice([0, 5])))])
    if k < 0.87:
        return (3, (r.choice(windows), r.choice([1, 1, 0, 3, 2, 0x10000001]), 0))
    if k < 0.91:
        return (4, (r.choice(windows), r.choice([-1, 0, 1, 2, 3, SPRITE, SPRITE + 1, SPRITE + 2, SPRITE + 3]), r.choice([-1, -1, 0, 1, SPRITE + 3])))
    if k < 0.94:
        return (7, (r.choice([1, 1, 2, 3]), r.choice([0, 1, 1600, 1200, 2048, 1536, 480])))
    if k < 0.96:
        return (11, (r.choice(windows), r.choice([0, 1, 2]), r.choice(SMALL + [20, 40]), r.choice(SMALL + [20, 40]), r.choice(SMALL + [60, 100]), r.choice(SMALL + [60, 100])))
    if k < 0.98:
        return (8, (r.choice(bars), r.choice(SMALL), r.choice(SMALL), r.choice(SMALL), r.choice([1, -1, 2, 0]), r.choice([1, 10, -3]), r.choice([1, 2, 5, 6])))
    return (5, (r.choice(windows), r.choice([0, 1, 2, 3])))


def long_script(r):
    ops, q, _ = long_world(r)
    for _ in range(200):
        ops.append(long_op(r))
    q = dict(q)
    q["DefaultHandler"] = [r.choice([0, 1, -1, 5]) for _ in range(5)]
    return case_of(ops, q)


def long_edges():
    for seed in range(8):
        yield long_script(_random.Random(2000 + seed))


TESTS.append(FnTest("scripts of 200 operations over windows, scroll bars and sprite draw objects", ui_ppc, ui_host, edge=long_edges, rnd=long_script))
