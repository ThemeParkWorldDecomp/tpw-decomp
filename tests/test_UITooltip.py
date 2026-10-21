"""Differential tests for unit w56, the tooltip window, against libtpw_unit_UITooltip.dylib (tests/unit_UITooltip.cpp):

  src/game/ui/ui_tooltip.cpp           CUIHelpWindow (the static constructor, the destructor, ShowHelpText, HideHelpText, OnFrame,
                               OnMouseMovedIn, Init, DeInit, Enable, Disable, Update), HelpTextMsg, InitHelpText,
                               SetHelpTextString, MoveHelpText
  src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp InterfaceWindow::GetOriginalPosition (and the real GetPosition, SetPosition, ShiftPosition under
                               MoveHelpText)

Real on both sides: those, the help text handler the class derives from (src/game/ui/InterfaceHelpText.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp: the base members the tooltip calls,
UI_SetHelpTextHandler), the window constructor and the rectangles. Fakes that record the same event on both sides: every other window
member (CreateObject, the font, text and draw object setters, the extra parameters, the callback, Show, Enable, SetText, the default
handler), the message senders, the pointer, the UI clock, the font table entry and its height, the draw object library, the text
sprite's constructor, the interface's string table (GetConstString) and the multibyte converter.

A case is a world and a script of operations. The world: four real windows (the first is the root window; their rectangles, original
rectangles and animation offsets are the case's), the tooltip object (the static object itself, its seven words are the case's), the
screen's real height, the help text pair message 0xc answers, the handler the pump has, and the queues the fakes answer from. After EVERY
operation the answer, the tooltip object's words, the pair, the handler, the four windows' rectangles, pivots, depths and animation
offsets, and the heap traffic are compared, and the whole call log at the end. The world never leaves the object initialised without a
text window (the original dereferences it), CreateObject never answers NULL and the extra parameter never answers NULL (both crash).

Quirks the tests pin: Init cuts the font height to 16 bits, divides by the screen's height as a signed 32-bit number, subtracts the
32-bit height from 0x5f0 and passes the 16-bit cuts as the top and bottom; ShowHelpText prefers the pair's string, then the table's
string for the number, with -1 meaning the empty text, and hides again when the text is empty; MoveHelpText resizes the window when the
original rectangle is not the same size, and a coordinate of -1 keeps the window's own less its animation offsets.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UITooltip"

MINUS = 0xFFFFFFFF

# ---- the unit's functions (all run as the original on the PowerPC side)
OBJ = 0x1038CD34
FN = dict(sinit=0x1018C8EC, dtor=0x1018BF00, init=0x1018C280, deinit=0x1018C470, msg=0x1018BE38, show=0x1018BF94, hide=0x1018C124,
          moved_in=0x1018C1E4, frame=0x1018C1B0, enable=0x1018C4AC, disable=0x1018C528, update=0x1018C5A8, init_text=0x1018C60C,
          set_text=0x1018C694, move=0x1018C7A4, original=0x101DB9D0)
# what they call that runs for real: the window constructor and its rectangles, the position members, the base class's members
REAL = [0x101D98D8, 0x101DAB88, 0x101D987C, 0x101DB884, 0x101DAE98, 0x101DA9D0, 0x101DAC2C, 0x101CA978,
        0x101CF874, 0x101CF8B8, 0x101CF8FC, 0x101CF954, 0x101CF9D0, 0x101CFA14, 0x101CFA88, 0x101CFABC, 0x101CFAF0, 0x101CFB24,
        0x101CFB68, 0x101CFC04, 0x101CFC8C, 0x101CFD10, 0x101CFD48, 0x101CFD7C, 0x101CFDB0,
        0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788, 0x101C7A1C,
        0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74]
# the fakes (in-binary functions)
SEND_MESSAGE, POST_MESSAGE, UPDATE_POINTER_IN, IVE_GOT_TICKS = 0x101C9518, 0x101C9400, 0x101C8F18, 0x101CAC3C
WINDOW_ENABLE, WINDOW_SET_TEXT, WINDOW_SHOW, CREATE_OBJECT = 0x101DC320, 0x101DE248, 0x101DC1EC, 0x101DC750
SET_CLIENT_DRAW_OBJECT, SET_DRAW_OBJECT, SET_FONT_JUST, SET_FONT_WRAP = 0x101DB624, 0x101DB474, 0x101DA204, 0x101DA2D8
SET_FONT, SET_FONT_COLOUR, SET_CLIENT_DRAW_TEXT, SET_EXTRA, GET_EXTRA = 0x101DA118, 0x101DA3AC, 0x101DE130, 0x101DCFD0, 0x101DD030
SET_Z, SET_CALLBACK, CALL_DEFAULT = 0x101DD8BC, 0x101DCBA8, 0x101DE318
GET_FONT, FONT_HEIGHT, FIND_DRAW, SPRITE_CTOR, CONVERT_MULTI = 0x10180270, 0x10168C04, 0x101CD2D4, 0x1018D11C, 0x101803CC
HELP_MSG_TVECTOR = 0x10267830
FONT_VTABLE = 0x102A71C0   # InterfaceFontWrapper's vtable (slot 0xc is GetHeight)

# ---- the original's data
G = dict(height=0x10395BBA, root=0x10395C24, result=0x10395C60, string=0x10395C64, help=0x102AF928, default=0x10395B8C,
         text=0x1037FF18)
TABLE_SLOT = G["text"] + 0x568   # gText.mResourcePointers[7], the string table ShowHelpText reads
TOOLTIP_VTABLE = 0x102A7DE8

# ---- labels (the C++ file's)
LAB_OBJECT, LAB_DEFAULT, LAB_WINDOW, LAB_FONT, LAB_TOKEN, LAB_TABLE = 5, 6, 10, 20, 30, 40
WINDOWS, TEXTS_N, STRINGS_N, TOKENS = 4, 8, 6, 3
MAX_QUEUE = 60

# the texts the operations hand out (wide) and the string table's (multibyte; all but one ASCII, see CONVERT)
TEXTS = ["", "Tip", "Two\nlines", "\n", "café", "x" * 40, "Ж", "a\nb\nc"]
STRINGS = [b"", b"Table text", b"Multi\nline", b"\xe9 bytes", b"S" * 30, b"\n"]
assert len(TEXTS) == TEXTS_N and len(STRINGS) == STRINGS_N


def s16(v):
    return T.s16(v & 0xFFFF)


def s32(v):
    return T.s32(v & MINUS)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def w16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xFFFF))


def wide(text):
    return b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0"


def wide_at(e, address):
    out = []
    while True:
        v = struct.unpack(">H", e.read(address + 2 * len(out), 2))[0]
        if v == 0:
            return out
        out.append(v)


def narrow_bytes(data):
    return "".join(chr(b) if b < 0x80 else "?" for b in data)[:159]


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the operations and the dumps."""

    ADDRESSES = [(OBJ, 0x1C), (G["height"], 2), (G["root"], 4), (G["result"], 8), (G["help"], 4), (G["default"], 0x18), (TABLE_SLOT, 4)]

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        self.heap = [0, 0, 0, 0]
        self.observe = False
        queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
        self.L = e.cl = CL.PpcLog({}, queues)
        L = self.L
        self.windows = []
        rects = case["rects"]
        for i in range(WINDOWS):
            self.windows.append(e.alloc(0x140, b"\xa5" * 0x140))
        # the Mac's constructor needs the stubs below (UI_PostMessage, UI_UpdatePointerIn): installed by the caller before this runs
        for i, w in enumerate(self.windows):
            L.labels[w] = LAB_WINDOW + i
            self.call(0x101D98D8, w, 0, 1, 0x100 + i, *rects[i])
            for k, v in enumerate((0x1111 + i, 0x2222 + i, 0x3333 + i, 0x4444 + i)):   # markers in the previous rectangle
                w16(e, w + 0x20 + 2 * k, v)
            for k in range(4):
                w16(e, w + 0xF4 + 2 * k, case["anim"][i][k])
            if case["orig"][i] is not None:
                for k, v in enumerate(case["orig"][i]):
                    w16(e, w + 0x14 + 2 * k, v)
        w32(e, G["root"], self.windows[0])
        w16(e, G["height"], case["height"])
        self.font = e.alloc(0x20, bytes(0x20))
        w32(e, self.font + 0x10, FONT_VTABLE)
        L.labels[self.font] = LAB_FONT
        self.tokens = []
        for i in range(TOKENS):
            block = e.alloc(0x40, bytes(0x40))
            L.labels[block] = LAB_TOKEN + i
            self.tokens.append(block)
        self.table = e.alloc(0x20, bytes(0x20))
        L.labels[self.table] = LAB_TABLE
        w32(e, TABLE_SLOT, self.table)
        self.texts = [e.alloc(len(wide(t)), wide(t), alignment=4) for t in TEXTS]
        self.strings = [e.alloc(len(s) + 1, s + b"\0") for s in STRINGS]
        self.convert_buffer = e.alloc(0x400, bytes(0x400))
        L.labels[OBJ] = LAB_OBJECT
        self.default = G["default"]
        L.labels[self.default] = LAB_DEFAULT
        self.call(0x101CF8FC, self.default)
        w32(e, G["help"], self.default if case["handler"] == 0 else OBJ)
        self.put_object(case["obj"])
        number, text = case["pair"]
        w32(e, G["result"], number)
        w32(e, G["string"], self.texts[text] if text >= 0 else 0)
        L.log.clear()

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def window(self, slot):
        return self.windows[slot] if slot >= 0 else 0

    def put_object(self, o):
        e = self.e
        e.write(OBJ, struct.pack(">hhhhh", s16(o["enabled"]), s16(o["wanted"]), s16(o["mx"]), s16(o["my"]), s16(o["showing"])))
        w16(e, OBJ + 0xA, 0xA5A5)
        w32(e, OBJ + 0xC, o["ticks"])
        w32(e, OBJ + 0x10, TOOLTIP_VTABLE)
        w32(e, OBJ + 0x14, self.window(o["window"]))
        w32(e, OBJ + 0x18, self.window(o["help"]))

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    def dump_window(self, w):
        e = self.e
        d = bytes(e.read(w, 0x100))
        h = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        out = []
        for base in (8, 0x14, 0x20, 0x2C):
            out += [h(base), h(base + 2), h(base + 4), h(base + 6)]
        out += [h(0xE0), h(0xE2), h(0xDC)] + [h(0xF4 + 2 * k) for k in range(4)]
        return out

    def dump_state(self):
        e, L = self.e, self.L
        d = bytes(e.read(OBJ, 0x1C))
        h = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        u = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [h(0), h(2), h(4), h(6), h(8), u(0xC), int(u(0x10) == TOOLTIP_VTABLE), L.lab(u(0x14)), L.lab(u(0x18)),
               s32(e.u32(G["result"])), L.lab(e.u32(G["help"]))]
        for w in self.windows:
            out += self.dump_window(w)
        out += list(self.heap)
        return out

    def op(self, code, a):
        """Runs one operation and answers its five answer words."""
        e = self.e
        answer = [0] * 5
        self.observe = True
        try:
            if code == 1:
                self.call(FN["init"], OBJ)
            elif code == 2:
                self.call(FN["deinit"], OBJ)
            elif code == 3:
                answer[0] = s32(self.call(FN["msg"], self.windows[a[0]], a[1], a[2], a[3]))
            elif code == 4:
                self.call(FN["show"], OBJ)
            elif code == 5:
                self.call(FN["hide"], OBJ)
            elif code == 6:
                self.call(FN["moved_in"], OBJ, self.window(a[0]))
            elif code == 7:
                self.call(FN["frame"], OBJ)
            elif code == 8:
                self.call(FN["enable"], OBJ)
            elif code == 9:
                self.call(FN["disable"], OBJ)
            elif code == 10:
                self.call(FN["update"], OBJ)
            elif code == 11:
                self.call(FN["init_text"])
            elif code == 12:
                self.call(FN["set_text"], self.texts[a[0]])
            elif code == 13:
                self.call(FN["move"], s16(a[0]), s16(a[1]))
            elif code == 14:
                self.call(FN["dtor"], OBJ, 0)
                w32(e, OBJ + 0x10, TOOLTIP_VTABLE)   # (the object is used again)
            elif code == 15:
                rect = e.alloc(12, bytes(12))
                e.write(rect + 4, struct.pack(">hhhh", 0x5151, 0x5252, 0x5353, 0x5454))
                answer[0] = s32(self.call(FN["original"], self.windows[a[0]], 0 if a[1] else rect))
                answer[1:5] = struct.unpack(">hhhh", e.read(rect + 4, 8))
            elif code == 16:
                e.write(OBJ, b"\xa5" * 0x1C)
                self.call(FN["sinit"])
            else:
                raise ValueError(code)
        finally:
            self.observe = False
        return answer


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL)
    R = CL.recorder

    # the heap: blocks made while a function runs are filled with 0xa5 and counted
    def op_new(e):
        us, size = e.us, e.r(3)
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        if us.observe:
            us.heap[0] += 1
        e.set_r(3, block)

    S.imp("__nw__FUl", op_new)
    S.imp("__register_global_object", lambda e: None)

    # the Mac's wcschr and wcslen on 16-bit characters
    def wcschr(e):
        text, wanted = e.r(3), e.r(4) & 0xFFFF
        chars = wide_at(e, text)
        e.set_r(3, text + 2 * chars.index(wanted) if wanted in chars else (text + 2 * len(chars) if wanted == 0 else 0))

    S.imp("wcschr", wcschr)
    S.imp("wcslen", lambda e: e.set_r(3, len(wide_at(e, e.r(3)))))

    # the senders, the pointer and the clock
    def send_answer(e, L):
        code = L.ret("SendMessage")
        return 0 if code == 0 else 0xFFFFFFFF if code == 1 else G["result"]

    S.internal(SEND_MESSAGE, R("SendMessage", "Liii", ret=send_answer))
    S.internal(POST_MESSAGE, R("PostMessage", "Liii", ret="i"))
    S.internal(UPDATE_POINTER_IN, R("UpdatePointerIn", "", ret=lambda e, L: 1))
    S.internal(IVE_GOT_TICKS, R("IveGotTicks", "", ret=lambda e, L: L.ret("IveGotTicks", 1000)))

    # the window's virtual members and the other members the tooltip calls
    S.internal(WINDOW_ENABLE, R("Window.Enable", "Lh", ret=lambda e, L: 0))
    S.internal(WINDOW_SET_TEXT, R("Window.SetText", "LS", ret=lambda e, L: 1))
    S.internal(WINDOW_SHOW, R("Window.Show", "Lh", ret=lambda e, L: 1))

    def create_object(e, L):
        index = L.ret("CreateObject")
        return e.us.windows[index] if 0 < index < WINDOWS else 0

    S.internal(CREATE_OBJECT, R("CreateObject", "Liiihhhh", ret=create_object))
    S.internal(SET_CLIENT_DRAW_OBJECT, R("SetClientDrawObject", "LL"))
    S.internal(SET_DRAW_OBJECT, R("SetDrawObject", "LL"))
    S.internal(SET_FONT_JUST, R("SetFontJust", "Lii"))
    S.internal(SET_FONT_WRAP, R("SetFontWrap", "Lii"))
    S.internal(SET_FONT, R("Window.SetFont", "LL"))
    S.internal(SET_FONT_COLOUR, R("SetFontColour", "Liiii"))
    S.internal(SET_CLIENT_DRAW_TEXT, R("SetClientDrawText", "LS"))
    S.internal(SET_EXTRA, R("setExtraParam", "LhL", ret=lambda e, L: 1))

    def get_extra(e, L):
        which = L.ret("ExtraParam")
        return OBJ if which == 1 else e.us.default if which == 2 else 0

    S.internal(GET_EXTRA, R("getExtraParam", "Lh", ret=get_extra))
    S.internal(SET_Z, R("setZ", "Lh"))

    def set_callback(e):
        L, callback = e.cl, e.r(4)
        L.rec("SetCallbackFunction", (L.lab(e.r(3)), 1 if callback == HELP_MSG_TVECTOR else 0 if callback == 0 else 2))

    S.internal(SET_CALLBACK, set_callback)
    S.internal(CALL_DEFAULT, R("CallDefaultWindowHandler", "Liii", ret="i"))

    # the font table entry, its height, the draw object library, the text sprite's constructor
    S.internal(GET_FONT, R("GetFontTableEntry", "u", ret=lambda e, L: e.us.font))
    S.internal(FONT_HEIGHT, R("Font.GetHeight", "L", ret=lambda e, L: L.ret("Height")))

    def find_draw(e, L):
        index = L.ret("FindDraw")
        return e.us.tokens[index - 1] if 0 < index <= TOKENS else 0

    S.internal(FIND_DRAW, R("FindDrawObject", "s", ret=find_draw))
    S.internal(SPRITE_CTOR, R("TextSprite.ctor", "NL", ret=lambda e, L: e.r(3)))

    # the string table and the converter
    def const_string(e):
        L = e.cl
        L.rec("GetConstString", (L.lab(e.r(3)), e.r(4)))
        index = L.ret("String", -1)
        e.set_r(3, e.us.strings[index] if 0 <= index < STRINGS_N else 0)

    S.imp("GetConstString__8TbITableCFUl", const_string)

    def convert(e):
        L, text = e.cl, e.r(3)
        data = bytes(read_bytes(e, text)) if text else b""
        L.rec("ConvertMulti", (), (narrow_bytes(data) if text else "(null)",))
        buffer = e.us.convert_buffer
        for i, b in enumerate(data[:0x1FF]):
            w16(e, buffer + 2 * i, b)
        w16(e, buffer + 2 * min(len(data), 0x1FF), 0)
        e.set_r(3, buffer)

    S.internal(CONVERT_MULTI, convert)
    return S


def read_bytes(e, address):
    out = bytearray()
    while True:
        b = e.read(address + len(out), 1)[0]
        if b == 0:
            return out
        out.append(b)


def setup(runner):
    T.install_std_stubs(runner.emu)   # (memset, memcpy and the like, which the window constructor calls)


def operations(case):
    """The case's operations as (code, arguments)."""
    return [(op[0], op[1] if len(op) > 1 else ()) for op in case["ops"]]


def ppc(e, case):
    if not hasattr(e, "tt_stubs"):
        e.tt_stubs = build_stubs(e)
    saved = e.tt_stubs.activate(e)
    us = None
    try:
        e.cl = CL.PpcLog({})
        us = Ppc(e, case)
        answers, states = [], []
        for code, args in operations(case):
            answers.append(us.op(code, args))
            states.append(us.dump_state())
        return {"answers": answers, "states": states, "log": us.L.log}
    finally:
        if us is not None:
            us.restore()
        CL.Stubs.deactivate(e, saved)


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_tt_open", False):
        return
    lib.tpw_tt_world.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_tt_anim.argtypes = [C.c_int32, C.POINTER(C.c_int16)]
    lib.tpw_tt_orig.argtypes = [C.c_int32, C.POINTER(C.c_int16)]
    lib.tpw_tt_object.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_tt_pair.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_tt_screen.argtypes = [C.c_int32]
    lib.tpw_tt_handler.argtypes = [C.c_int32]
    lib.tpw_tt_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_tt_state.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tt_state.restype = C.c_int32
    lib._tt_open = True


def host(lib, case):
    open_host(lib)
    queues = {k: v[:MAX_QUEUE] for k, v in case.get("q", {}).items()}
    CL.host_begin(lib, {}, queues)
    keep = []
    rects = (C.c_int16 * 16)(*[v for r in case["rects"] for v in r])
    texts = (C.c_void_p * TEXTS_N)()
    for k, text in enumerate(TEXTS):
        buffer = (C.c_uint16 * (len(text) + 1))(*[ord(c) for c in text], 0)
        keep.append(buffer)
        texts[k] = C.cast(buffer, C.c_void_p)
    strings = (C.c_void_p * STRINGS_N)()
    for k, data in enumerate(STRINGS):
        buffer = C.create_string_buffer(data, len(data) + 1)
        keep.append(buffer)
        strings[k] = C.cast(buffer, C.c_void_p)
    lib.tpw_tt_world(C.cast(rects, C.c_void_p), C.cast(texts, C.c_void_p), C.cast(strings, C.c_void_p))
    for i in range(WINDOWS):
        lib.tpw_tt_anim(i, (C.c_int16 * 4)(*case["anim"][i]))
        if case["orig"][i] is not None:
            lib.tpw_tt_orig(i, (C.c_int16 * 4)(*case["orig"][i]))
    lib.tpw_tt_screen(case["height"])
    lib.tpw_tt_handler(case["handler"])
    o = case["obj"]
    lib.tpw_tt_object((C.c_int32 * 8)(o["enabled"], o["wanted"], o["mx"], o["my"], o["showing"], o["ticks"], o["window"], o["help"]))
    lib.tpw_tt_pair(case["pair"][0], case["pair"][1])
    C.c_int32.in_dll(lib, "tpw_cl_n").value = 0   # (the constructors' events are not compared)
    answers, states = [], []
    for code, args in operations(case):
        a = (C.c_int64 * 4)(*args, *([0] * (4 - len(args))))
        out = (C.c_int64 * 5)()
        lib.tpw_tt_op(code, a, out)
        answers.append(list(out))
        buf = (C.c_int64 * 256)()
        count = lib.tpw_tt_state(buf)
        states.append(list(buf[:count]))
    return {"answers": answers, "states": states, "log": CL.host_end(lib)}


# ======================================================================= the cases

ROOT_RECT = (0, 0, 2048, 1536)
DEFAULT_RECTS = [ROOT_RECT, (570, 1400, 1474, 1520), (100, 200, 400, 300), (-50, -60, 70, 80)]
DEFAULT_ANIM = [(0, 0, 0, 0)] * 4


def obj(enabled=1, wanted=0, mx=0x11, my=0x22, showing=1, ticks=77, window=-1, help=1):
    return dict(enabled=enabled, wanted=wanted, mx=mx, my=my, showing=showing, ticks=ticks, window=window, help=help)


def case(ops, o=None, rects=None, anim=None, height=0x300, pair=(0, -1), handler=0, q=None, orig=None):
    return dict(ops=ops, obj=o or obj(), rects=rects or DEFAULT_RECTS, anim=anim or DEFAULT_ANIM, height=height, pair=pair,
                handler=handler, q=q or {}, orig=orig or [None] * 4)


HEIGHTS = [0x300, 0x400, 0x600, 1, 0x7FFF, -5, 0x1000]
FONT_HEIGHTS = [0, 8, 12, 40, -1, 0x10008, 0x7FFF, -0x8000, 100, 0x1FFFF]
SHOWINGS = [0, 1, 2, -1]


def tests():
    out = []

    def add(name, op_code, edge, rnd):
        out.append(FnTest(name, ppc, lambda lib, c: host(lib, c), edge=edge, rnd=rnd))

    def rand_obj(r, showing=None):
        showing = r.choice([0, 1, 1, 1, 2, -1, 0x100]) if showing is None else showing
        return obj(enabled=r.choice([0, 1, 1, 2, -1, 0x100]), wanted=r.choice([0, 1, 1, 7]), mx=r.randint(-0x8000, 0x7FFF),
                   my=r.randint(-0x8000, 0x7FFF), showing=showing, ticks=r.getrandbits(32), window=r.choice([-1, 0, 1, 2, 3]),
                   help=r.choice([1, 2, 3]))

    def rand_rects(r):
        rects = [ROOT_RECT]
        for _ in range(3):
            if r.random() < 0.2:
                left, top = r.randint(-0x8000, 0x7000), r.randint(-0x8000, 0x7000)
                rects.append((left, top, left + r.randint(1, 0x700), top + r.randint(1, 0x700)))
            else:
                left, top = r.randint(-300, 2000), r.randint(-300, 1500)
                rects.append((left, top, left + r.randint(0, 600), top + r.randint(0, 400)))
        return rects

    def rand_anim(r):
        return [tuple(r.choice([0, 0, r.randint(-40, 40)]) for _ in range(4)) for _ in range(4)]

    def rand_queues(r, extra=None):
        q = {"SendMessage": [r.choice([0, 1, 2, 2, 2]) for _ in range(6)], "Height": [r.choice(FONT_HEIGHTS) for _ in range(6)],
             "CreateObject": [r.randint(1, 3) for _ in range(4)], "FindDraw": [r.randint(0, 3) for _ in range(4)],
             "String": [r.randint(-1, 5) for _ in range(6)], "IveGotTicks": [r.getrandbits(32) for _ in range(4)],
             "ExtraParam": [r.choice([1, 1, 2]) for _ in range(4)], "CallDefaultWindowHandler": [r.randint(-5, 5) for _ in range(4)]}
        q.update(extra or {})
        return q

    def rand_pair(r):
        return (r.choice([-1, 0, 3, 0x1234, -2, 5, 0x7FFFFFFF, -0x80000000]), r.choice([-1, -1, 0, 1, 2, 3, 4, 5, 6, 7]))

    def rand_orig(r, rects):
        out = [None]
        for i in range(1, 4):
            if r.random() < 0.5:
                out.append(None)
            elif r.random() < 0.5:   # the same size, somewhere else
                left, top = r.randint(-300, 2000), r.randint(-300, 1500)
                out.append((left, top, left + rects[i][2] - rects[i][0], top + rects[i][3] - rects[i][1]))
            else:
                left, top = r.randint(-300, 2000), r.randint(-300, 1500)
                out.append((left, top, left + r.randint(0, 600), top + r.randint(0, 400)))
        return out

    def rand_world(r, ops, showing=None):
        rects = rand_rects(r)
        return case(ops, rand_obj(r, showing), rects, rand_anim(r), r.choice(HEIGHTS), rand_pair(r), r.randint(0, 1),
                    rand_queues(r), rand_orig(r, rects))

    # ---- the static constructor
    add("__sinit_ui_tooltip_cpp", 16, lambda: [case([(16,)]), case([(16,)], obj(0, 1, 5, 6, 7, 8, 1, 2)),
                                                 case([(16,), (4,), (16,)], q={"SendMessage": [0]})],
        lambda r: rand_world(r, [(16,)]))
    # ---- the destructor (the non-deleting form)
    add("CUIHelpWindow::~CUIHelpWindow", 14, lambda: [case([(14,)], obj(e, w, showing=s, window=win, help=h)) for e in (0, 1) for w in (0, 1)
                                                      for s in (0, 1) for win in (-1, 2) for h in (-1, 1)],
        lambda r: rand_world(r, [(14,)]))

    # ---- Init: font heights x screen heights x what CreateObject and FindDrawObject answer
    def init_edge():
        for fh in FONT_HEIGHTS:
            for sh in HEIGHTS:
                yield case([(1,)], height=sh, q={"Height": [fh], "CreateObject": [1], "FindDraw": [1]})
        for create in (1, 2, 3):
            for find in (0, 1, 2, 3):
                for showing in (0, 1):
                    yield case([(1,), (1,)], obj(showing=showing, help=create), q={"CreateObject": [create, 4 - create], "FindDraw": [find],
                                                                                    "Height": [12, 40]})
        yield case([(1,)], height=0x300, q={"Height": [0x7FFF], "CreateObject": [1]})
        yield case([(1,)], height=0x7FFF, q={"Height": [-0x8000], "CreateObject": [2]})
        yield case([(1,)], height=-1, q={"Height": [0x7FFF], "CreateObject": [3]})
        yield case([(1,)], height=0x300, q={"Height": [3000], "CreateObject": [1]})   # taller than the screen: a negative top

    add("CUIHelpWindow::Init", 1, lambda: list(init_edge()), lambda r: rand_world(r, [(1,)]))

    add("CUIHelpWindow::DeInit", 2, lambda: [case([(2,)], obj(showing=s, window=win, help=h)) for s in (0, 1, 5) for win in (-1, 1) for h in (-1, 2)],
        lambda r: rand_world(r, [(2,)]))

    # ---- HelpTextMsg
    def msg_edge():
        for w in range(4):
            for m in (0x14, 0, 1, 0xC, 0x13, 0x15, 0x10005, -1, 0x7FFFFFFF, 0x13, 0x1E):
                for extra in (1, 2):
                    for answer in (0, -1, 0x55):
                        yield case([(3, (w, m, 7 + w, -9))], obj(showing=1), q={"ExtraParam": [extra], "CallDefaultWindowHandler": [answer]})
        yield case([(3, (1, 0x14, 0, 0)), (3, (1, 0x14, 0, 0))], obj(showing=1), q={"ExtraParam": [1, 2]})
        yield case([(3, (1, 0x14, 0, 0)), (4,), (1,)], obj(showing=1), q={"ExtraParam": [1], "SendMessage": [2], "CreateObject": [2]})

    def msg_rnd(r):
        ops = [(3, (r.randint(0, 3), r.choice([0x14, 0x14, r.randint(-5, 0x20), r.getrandbits(31)]), r.getrandbits(31) - 0x40000000,
                    r.randint(-100, 100)))]
        return rand_world(r, ops)

    add("HelpTextMsg", 3, msg_edge, msg_rnd)

    # ---- ShowHelpText: the grid of the flags, the message's answer, the pair, the table and the text
    def show_edge():
        for showing in (0, 1):
            for enabled in (0, 1, 2):
                for answer in (0, 1, 2):
                    yield case([(4,)], obj(enabled=enabled, showing=showing, window=2), q={"SendMessage": [answer]}, pair=(3, 1))
        for answer_text in (-1, 0, 1, 2, 3, 4, 5, 6, 7):
            for number in (-1, 0, 3, 0x7FFFFFFF):
                yield case([(4,)], obj(window=2, wanted=0), pair=(number, answer_text), q={"SendMessage": [2]})
        for number in (-1, 0, 3, 0x1234, -2):
            for string in range(-1, 6):
                for wanted in (0, 1):
                    yield case([(4,)], obj(window=2, wanted=wanted), pair=(number, -1), q={"SendMessage": [2], "String": [string]})
        yield case([(4,), (4,)], obj(window=2), pair=(5, -1), q={"SendMessage": [2, 0], "String": [1]})
        yield case([(4,)], obj(window=-1, help=1), q={"SendMessage": [2]}, pair=(4, 1))   # no window: SendMessage answers per the queue

    def show_rnd(r):
        return rand_world(r, [(4,)])

    add("CUIHelpWindow::ShowHelpText", 4, show_edge, show_rnd)

    # ---- HideHelpText
    add("CUIHelpWindow::HideHelpText", 5,
        lambda: [case([(5,)], obj(showing=s, wanted=w, help=h)) for s in (0, 1, 3) for w in (0, 1, 5) for h in (-1, 1, 3)]
        + [case([(5,), (5,)], obj(showing=1, wanted=1, help=2))],
        lambda r: rand_world(r, [(5,)]))

    # ---- OnMouseMovedIn
    def moved_edge():
        for showing in (0, 1):
            for enabled in (0, 1):
                for win in (-1, 0, 1, 2, 3):
                    for answer in (0, 2):
                        yield case([(6, (win,))], obj(enabled=enabled, showing=showing, ticks=5), pair=(3, 2 if answer else -1),
                                   q={"SendMessage": [answer], "IveGotTicks": [1234, 99]})
        yield case([(6, (1,)), (6, (2,)), (6, (-1,))], q={"SendMessage": [1, 0, 2], "IveGotTicks": [10, 20, 30], "String": [2]}, pair=(1, -1))

    add("CUIHelpWindow::OnMouseMovedIn", 6, moved_edge, lambda r: rand_world(r, [(6, (r.choice([-1, 0, 1, 2, 3]),))]))

    add("CUIHelpWindow::OnFrame", 7, lambda: [case([(7,)]), case([(7,)], obj(0, 1, 2, 3, 0, 5, 1, -1))], lambda r: rand_world(r, [(7,)]))

    # ---- Enable, Disable, Update
    def flag_edge(code):
        def edge():
            for showing in (0, 1, 4):
                for enabled in (0, 1, 3):
                    for wanted in (0, 1):
                        for answer in (0, 2):
                            yield case([(code,)], obj(enabled=enabled, wanted=wanted, showing=showing), pair=(3, 2 if answer else -1),
                                       q={"SendMessage": [answer], "String": [1]})
            yield case([(code,), (code,), (code,)], obj(wanted=1), q={"SendMessage": [2, 2, 0]}, pair=(1, 1))
        return edge

    add("CUIHelpWindow::Enable", 8, flag_edge(8), lambda r: rand_world(r, [(8,)]))
    add("CUIHelpWindow::Disable", 9, flag_edge(9), lambda r: rand_world(r, [(9,)]))
    add("CUIHelpWindow::Update", 10, flag_edge(10), lambda r: rand_world(r, [(10,)]))

    # ---- InitHelpText: the handler the pump had, the flags Init leaves
    def init_text_edge():
        for handler in (0, 1):
            for fh in (0, 12, 40):
                for showing in (0, 1):
                    for enabled in (0, 1):
                        yield case([(11,)], obj(enabled=enabled, showing=showing, wanted=1), handler=handler,
                                   q={"Height": [fh], "CreateObject": [1], "FindDraw": [2]})
        yield case([(11,), (11,)], q={"Height": [12], "CreateObject": [2, 3]})
        yield case([(11,), (4,), (8,), (4,)], q={"Height": [12], "CreateObject": [2], "SendMessage": [2], "String": [1]}, pair=(1, -1))

    add("InitHelpText", 11, init_text_edge, lambda r: rand_world(r, [(11,)]))

    # ---- SetHelpTextString
    def set_text_edge():
        for showing in (0, 1):
            for enabled in (0, 1, 2):
                for text in range(TEXTS_N):
                    yield case([(12, (text,))], obj(enabled=enabled, showing=showing, wanted=1))
        yield case([(12, (1,)), (12, (0,)), (12, (2,)), (12, (3,))])

    add("SetHelpTextString", 12, set_text_edge, lambda r: rand_world(r, [(12, (r.randint(0, 7),))]))

    # ---- MoveHelpText: (-1, -1) back to the original rectangle, the others a shift
    COORDS = [-1, 0, 1, 5, -2, 0x7FFF, -0x8000, 0x1234, 570, 1400, 2047]

    def move_edge():
        sets = [
            (DEFAULT_RECTS, DEFAULT_ANIM),
            ([ROOT_RECT, (570, 1400, 1474, 1520), (100, 200, 400, 300), (-50, -60, 70, 80)], [(0, 0, 0, 0), (3, -4, 5, -6), (0, 0, 0, 0), (1, 2, 3, 4)]),
        ]
        for rects, anim in sets:
            for x in COORDS:
                for y in COORDS:
                    for help_slot in (1, 2, -1):
                        yield case([(13, (x, y))], obj(help=help_slot), rects=rects, anim=anim)
        yield case([(13, (-1, -1)), (13, (50, 60)), (13, (-1, -1)), (13, (-1, 7)), (13, (9, -1))])
        # the original rectangle of another size (a resize), of the same size elsewhere, and the same as the window's
        for orig in ((0, 0, 100, 100), (570, 1400, 1474, 1520), (100, 100, 1004, 220), (570, 1400, 1474, 1521), (1, 2, 3, 4)):
            for anim in (DEFAULT_ANIM, [(0, 0, 0, 0), (3, -4, 5, -6), (0, 0, 0, 0), (0, 0, 0, 0)]):
                yield case([(13, (-1, -1)), (15, (1, 0)), (13, (-1, -1))], orig=[None, orig, None, None], anim=anim)
                yield case([(13, (-1, -1)), (13, (10, 20)), (13, (-1, -1))], orig=[None, orig, None, None], anim=anim,
                           q={"SendMessage": [0]})

    def move_rnd(r):
        ops = [(13, (r.choice([-1, -1, r.randint(-0x8000, 0x7FFF), r.randint(-100, 2100)]),
                     r.choice([-1, -1, r.randint(-0x8000, 0x7FFF), r.randint(-100, 1600)]))) for _ in range(r.randint(1, 4))]
        return rand_world(r, ops)

    add("MoveHelpText", 13, move_edge, move_rnd)

    # ---- GetOriginalPosition (the rectangle the window was made with, here also with the window moved first)
    add("InterfaceWindow::GetOriginalPosition", 15,
        lambda: [case([(15, (w, null))], rects=rects) for w in range(4) for null in (0, 1)
                 for rects in (DEFAULT_RECTS, [ROOT_RECT, (-0x8000, -0x8000, 0x7FFF, 0x7FFF), (1, 2, 3, 4), (0, 0, 0, 0)])]
        + [case([(15, (1, 0)), (13, (5, 6)), (15, (1, 0)), (13, (-1, -1)), (15, (1, 0))])],
        lambda r: rand_world(r, [(15, (r.randint(0, 3), r.choice([0, 0, 0, 1])))]))

    # ---- scripts of operations of all kinds
    def script(r):
        ops = []
        for _ in range(r.randint(2, 8)):
            code = r.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
            if code == 3:
                args = (r.randint(0, 3), r.choice([0x14, 0x14, 0xC, 5]), 1, 2)
            elif code == 6:
                args = (r.choice([-1, 0, 1, 2, 3]),)
            elif code == 12:
                args = (r.randint(0, 7),)
            elif code == 13:
                args = (r.choice([-1, r.randint(0, 2000)]), r.choice([-1, r.randint(0, 1500)]))
            elif code == 15:
                args = (r.randint(0, 3), r.choice([0, 0, 1]))
            else:
                args = ()
            ops.append((code, args))
        return rand_world(r, ops, showing=1)

    add("scripts of operations", 0, lambda: [], script)
    return out


TESTS = tests()
