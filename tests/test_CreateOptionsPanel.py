"""Unit v5634, the options panel's opening (the settings stored, the game paused when it ran, the map's render options saved and the
panel built, its texts, scroll bars and buttons set from the player's settings), against libtpw_unit_CreateOptionsPanel.dylib
(tests/unit_CreateOptionsPanel.cpp), main app ("SimTheme Park"), src/CreateOptionsPanel.cpp:
  CreateOptionsPanel 0x101a9fb8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit is a fake on both sides that records its arguments and answers from the case: StoreSettings, the four volume getters,
CGame::Pause, UI_PauseTimerSystem, ADV_Kill, the map's Get/SetRenderOptions, the texture cache's Flush, UI_SendMessage, the window
members (Build, GetWindow, setZ, SetClientDrawObject, SetFontColour, SetFont, SetClientDrawText), SetMinMax, SetState, UIMain_GetString
(writes the case's stem and the id in hex to gUIMainConvertBuffer and answers it, as the real one answers that buffer),
UIMain_SetTitleText, GetFontTableEntry, operator new and the text sprite's constructor, the panel's DoScroll and DoButton, SetOnOffText,
and the virtual Enable, SetValue and SetText (the PowerPC side's __ptr_glue stub, the host's fake vtable). wcsncpy and wcscat are real
on both sides (a Python model of MSL's on the PowerPC, platform/msl.cpp on the host). Strings are recorded as "text" events of eight
characters.

The world of a case: gSetOnOffRoot, gGame.mF3C, the player's settings (renderer, resolution, detail, video card, the four volume
switches, sound quality, the seven switches), gLobby (a lobby whose window is one of the case's), gGameWindow, gUIRootWindow, the map,
the texture cache and the display (its device count) through the gei cells, the font index at 0x102ab668, six windows (each with its
depth) and a font the fakes answer, and gUIMainConvertBuffer's first 0x100 characters. What is compared: the event log, gSetOnOffRoot's
label, the three panel globals and the font index, gGame.mF3C, the settings block and the convert buffer's first 0x100 characters.

No fused operations. Not tested: strings of 63 characters or more (the original's wcsncpy into its 64-character local leaves the copy
unterminated and wcscat reads past it; the stem stays at 59 characters at most); a NULL window from GetWindow or Build, a NULL map,
texture cache or display (the original dereferences them all unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateOptionsPanel"
MASK = 0xFFFFFFFF

# ---- the unit's own function (address, size from the MANIFEST)
FN = 0x101a9fb8
SIZES = {FN: 6432}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN); GLUE ones are cross-fragment glue (r2 saved at 0x14(r1))
STORE_SETTINGS = 0x10166244
PAUSE = 0x10149904
PAUSE_TIMERS = 0x101d8b64
ADV_KILL = 0x10008f00
SEND_MESSAGE = 0x101c9518
BUILD = 0x101deb78
GET_STRING = 0x10180468
SET_TITLE = 0x10180794
GET_WINDOW = 0x101dbe40
SET_Z = 0x101dd8bc
SPRITE_CTOR = 0x1018d11c
SET_DRAW_OBJECT = 0x101db624
SET_FONT_COLOUR = 0x101da3ac
GET_FONT = 0x10180270
SET_FONT = 0x101da118
SET_DRAW_TEXT = 0x101de130
SET_MIN_MAX = 0x101d6b44
GET_SFX = 0x10165ddc
GET_MUSIC = 0x10165d9c
GET_SPEECH = 0x10165e1c
GET_MOVIE = 0x10165e5c
DO_SCROLL = 0x101a8fb8
SET_STATE = 0x101cb1d0
DO_BUTTON = 0x101a9808
SET_ON_OFF = 0x101a8e60
GLUE_GET_RENDER = 0x10239808
GLUE_SET_RENDER = 0x10239820
GLUE_FLUSH = 0x10239838
GLUE_NEW = 0x102384b8
GLUE_WCSNCPY = 0x10238cb0
GLUE_WCSCAT = 0x10239370
PTR_GLUE = 0x1023baa8

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_cop_addr(name)
ADDR = dict(gGame=0x1037f134, game_f3c=0x1037f134 + 0x3c, settings=0x1038071c, gLobby=0x102e4888, gGameWindow=0x103949e4,
            gUIRootWindow=0x10395c24, gSetOnOffRoot=0x10394a14, convbuf=0x10380c48, gei_MapWho=0x103bc908, gei_TCache=0x103bc9c0,
            gei_DDraw=0x103bc98c, running=0x10394a08, f0c=0x10394a0c, render=0x10394a10, font_index=0x102ab668, script=0x102aade4,
            callback=0x10267c90)
# The TOC slots of the imported variables (the listing's TOC comments; the cells above are the Mac's, the emulator binds its own)
IMPORT_TOC = dict(gei_MapWho=0x102607e4, gei_TCache=0x10260674, gei_DDraw=0x10260b28)
# ---- the host's offsets the module relies on (CPlayerSettings: mCurrent, then Settings' fields, Volume's mVolume, a word)
HOST_LAYOUT = [0, 4, 8, 0xc, 0x10, 0x18, 0x20, 0x28, 0x30, 0x34, 0x3a, 4, 4]

# The labels of the log: fixed objects, then the case's windows (20 on) and the font (11).
FIXED_LABELS = dict(script=2, callback=3, convbuf=4, gGame=5, settings=6)
LAB_MAPWHO, LAB_TCACHE, LAB_DDRAW, LAB_LOBBY, LAB_FONT, LAB_WINDOW = 7, 8, 9, 10, 11, 20
N_WINDOWS = 6
FONT_OBJECT = N_WINDOWS          # the font's index in the objects the fakes answer

CONV_CHARS = 0x100


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}                         # the running case's objects (Mac addresses by index) and the vtable's TVector markers


def save_r2(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def obj(index):
    return 0 if index < 0 else CUR["objs"][index]


def rec_text(e, a):
    """A UTF-16 string as "text" events of eight characters, the last one holding the terminator."""
    L = e.cl
    while True:
        c = list(struct.unpack(">8H", e.read(a, 16)))
        if 0 in c:
            k = c.index(0)
            c = c[:k] + [0] * (8 - k)
            L.rec("text", tuple(c))
            return
        L.rec("text", tuple(c))
        a += 16


def simple(name, spec, answer=False, glue=False):
    """A fake that records r3.. by spec (L label, i s32, u u32, h s16, b byte as 0/1) and answers the queue `name` when `answer`."""
    def stub(e):
        if glue:
            save_r2(e)
        L = e.cl
        vals = []
        for k, code in enumerate(spec):
            v = e.r(3 + k)
            if code == "L":
                vals.append(L.lab(v & MASK))
            elif code == "i":
                vals.append(s32(v))
            elif code == "u":
                vals.append(v & MASK)
            elif code == "h":
                vals.append(T.s16(v))
            elif code == "c":
                vals.append(T.s8(v))
            elif code == "b":
                vals.append(1 if (v & 0xFF) else 0)
        L.rec(name, tuple(vals))
        if answer:
            e.set_r(3, L.ret(name) & MASK)
    return stub


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3) & MASK), L.lab(e.r(4) & MASK), L.lab(e.r(5) & MASK)))
    e.set_r(3, obj(L.ret("Build")))


def stub_get_string(e):
    L = e.cl
    ident = s32(e.r(3))
    L.rec("GetString", (ident,))
    text = CUR["tag"] + "%x" % (ident & MASK)
    a = ADDR["convbuf"]
    e.write(a, b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0")
    e.set_r(3, a)


def stub_set_title(e):
    L = e.cl
    L.rec("SetTitleText", (L.lab(e.r(3) & MASK), s32(e.r(4))))
    rec_text(e, e.r(5) & MASK)


def stub_get_window(e):
    """Answers the case's GetWindow table in call order (longer than a queue may be)."""
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3) & MASK), s32(e.r(4))))
    k = CUR["gw_next"]
    CUR["gw_next"] = k + 1
    e.set_r(3, obj(CUR["gw"][k & 255]))


def stub_new(e):
    save_r2(e)
    L = e.cl
    size = e.r(3) & MASK
    L.rec("new", (size,))
    e.set_r(3, 0 if L.ret("new") == 0 else e.alloc(size))


stub_sprite_ctor = CL.recorder("SpriteCtor", "NL")       # the new sprite gets a new label; r4 (NULL) as a label


def stub_get_font(e):
    L = e.cl
    L.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, obj(L.ret("Font")))


def stub_set_state(e):
    L = e.cl
    L.rec("SetState", (L.lab(e.r(3) & MASK), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def stub_wcsncpy(e):
    """MSL's wcsncpy: copies up to n characters and pads with zeros to n."""
    save_r2(e)
    d, s, n = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    i = 0
    while i < n:
        ch = e.read(s + 2 * i, 2)
        e.write(d + 2 * i, ch)
        i += 1
        if ch == b"\0\0":
            break
    while i < n:
        e.write(d + 2 * i, b"\0\0")
        i += 1


def stub_wcscat(e):
    save_r2(e)
    d, s = e.r(3) & MASK, e.r(4) & MASK
    while e.read(d, 2) != b"\0\0":
        d += 2
    while True:
        ch = e.read(s, 2)
        e.write(d, ch)
        if ch == b"\0\0":
            break
        d += 2
        s += 2


def stub_ptr_glue(e):
    """The virtual calls: the TVector in r12 is one of the case's vtable markers."""
    save_r2(e)
    L = e.cl
    which = CUR["tvec"].get(e.r(12) & MASK)
    if which == "Enable":
        L.rec("Enable", (L.lab(e.r(3) & MASK), T.s16(e.r(4))))
    elif which == "SetValue":
        L.rec("SetValue", (L.lab(e.r(3) & MASK), s32(e.r(4))))
    elif which == "SetText":
        L.rec("SetText", (L.lab(e.r(3) & MASK),))
        rec_text(e, e.r(4) & MASK)
    else:
        raise RuntimeError("unexpected virtual call through %08x" % (e.r(12) & MASK))
    e.set_r(3, 0)


STUBS.internal(STORE_SETTINGS, simple("StoreSettings", "L"))
STUBS.internal(PAUSE, simple("Pause", "Lib"))
STUBS.internal(PAUSE_TIMERS, simple("UI_PauseTimerSystem", ""))
STUBS.internal(ADV_KILL, simple("ADV_Kill", "i"))
STUBS.internal(SEND_MESSAGE, simple("UI_SendMessage", "Liii", answer=True))
STUBS.internal(BUILD, stub_build)
STUBS.internal(GET_STRING, stub_get_string)
STUBS.internal(SET_TITLE, stub_set_title)
STUBS.internal(GET_WINDOW, stub_get_window)
STUBS.internal(SET_Z, simple("setZ", "Lh"))
STUBS.internal(SPRITE_CTOR, stub_sprite_ctor)
STUBS.internal(SET_DRAW_OBJECT, simple("SetClientDrawObject", "LL"))
STUBS.internal(SET_FONT_COLOUR, simple("SetFontColour", "Liiii"))
STUBS.internal(GET_FONT, stub_get_font)
STUBS.internal(SET_FONT, simple("SetFont", "LL"))
STUBS.internal(SET_DRAW_TEXT, simple("SetClientDrawText", "LL"))
STUBS.internal(SET_MIN_MAX, simple("SetMinMax", "Lii"))
STUBS.internal(GET_SFX, simple("GetSFXVolume", "L", answer=True))
STUBS.internal(GET_MUSIC, simple("GetMusicVolume", "L", answer=True))
STUBS.internal(GET_SPEECH, simple("GetSpeechVolume", "L", answer=True))
STUBS.internal(GET_MOVIE, simple("GetMovieVolume", "L", answer=True))
STUBS.internal(DO_SCROLL, simple("DoScroll", "ii"))
STUBS.internal(SET_STATE, stub_set_state)
STUBS.internal(DO_BUTTON, simple("DoButton", "ii"))
STUBS.internal(SET_ON_OFF, simple("SetOnOffText", "iibbbi"))
STUBS.internal(GLUE_GET_RENDER, simple("GetRenderOptions", "L", answer=True, glue=True))
STUBS.internal(GLUE_SET_RENDER, simple("SetRenderOptions", "Li", glue=True))
STUBS.internal(GLUE_FLUSH, simple("Flush", "Li", glue=True))
STUBS.internal(GLUE_NEW, stub_new)
STUBS.internal(GLUE_WCSNCPY, stub_wcsncpy)
STUBS.internal(GLUE_WCSCAT, stub_wcscat)
STUBS.internal(PTR_GLUE, stub_ptr_glue)


def setup(runner):
    pass                                  # every C library call of the function is faked above


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt + "I", v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt + "I", self.read(a, 4))[0]

    def w8(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def r8(self, a):
        return self.read(a, 1)[0]

    def w16s(self, a, vs):
        self.write(a, b"".join(struct.pack(self.fmt + "H", v & 0xFFFF) for v in vs))

    def r16s(self, a, n):
        return list(struct.unpack(self.fmt + "%dH" % n, self.read(a, 2 * n)))

    def wptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt, pfmt, psize = ">", ">I", 4

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def start(self, labels):
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name in IMPORT_TOC:            # an imported variable: the emulator's cell is the one its TOC slot holds
            return self.rptr(IMPORT_TOC[name])
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return bytes(self.e.read(a, n))

    def write(self, a, data):
        self.e.write(a, data)

    def window(self, z):
        w = self.alloc(0x140)
        self.write(w + 0xdc, struct.pack(">h", z))
        self.wptr(w + 0x118, self.vtable)
        return w

    def make_vtable(self):
        vt = self.alloc(0x40)
        tv = self.alloc(0x30)
        markers = {tv: "Enable", tv + 8: "SetValue", tv + 16: "SetText"}
        self.wptr(vt + 0x1c, tv)
        self.wptr(vt + 0x24, tv + 8)
        self.wptr(vt + 0x2c, tv + 16)
        self.vtable = vt
        CUR["tvec"] = markers

    def lobby(self, w):
        b = self.alloc(0x40)
        self.wptr(b + 4, w)
        return b

    def ddraw(self, count):
        b = self.alloc(0x790)
        self.w32(b + 0x784, count)
        return b

    def set_objects(self, objs, tag, gw):
        CUR["objs"] = list(objs)
        CUR["tag"] = tag
        CUR["gw"] = list(gw)
        CUR["gw_next"] = 0

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    """The same on the host library."""
    fmt, pfmt, psize = "<", "<Q", 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def start(self, labels):
        CL.host_begin(self.lib, {lab: a for a, lab in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cop_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cop_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def make_vtable(self):
        pass

    def window(self, z):
        return self.lib.tpw_cop_window(z)

    def lobby(self, w):
        return self.lib.tpw_cop_lobby(w)

    def ddraw(self, count):
        return self.lib.tpw_cop_ddraw(s32(count))

    def set_objects(self, objs, tag, gw):
        base = self.g("objects")
        for i, p in enumerate(objs):
            self.wptr(base + 8 * i, p)
        self.write(self.g("gw_answers"), struct.pack("<%dq" % len(gw), *gw))
        self.w32(self.g("gw_next"), 0)
        self.lib.tpw_cop_set_tag(tag.encode())

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_cop_run()


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_cop_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "window").restype, getattr(lib, p + "window").argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "lobby").restype, getattr(lib, p + "lobby").argtypes = C.c_uint64, [C.c_uint64]
    getattr(lib, p + "ddraw").restype, getattr(lib, p + "ddraw").argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "set_tag").restype, getattr(lib, p + "set_tag").argtypes = None, [C.c_char_p]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

# The settings block by Mac offset: words, then bytes.
SETTING_WORDS = [("renderer", 0x0), ("resolution", 0x4), ("detail", 0x8), ("card", 0xc), ("sfx", 0x14), ("music", 0x1c),
                 ("speech", 0x24), ("movie", 0x2c), ("quality", 0x30)]
SETTING_BYTES = [("sfx_on", 0x10), ("music_on", 0x18), ("speech_on", 0x20), ("movie_on", 0x28), ("advisor", 0x34),
                 ("tutorial", 0x35), ("tooltips", 0x36), ("confirm", 0x37), ("rmb_scroll", 0x38), ("rmb_cancel", 0x39),
                 ("isometric", 0x3a)]


def build(s, case):
    """Writes the case's globals and blocks (the same code for both memories; offsets are the Mac's)."""
    s.make_vtable()
    windows = [s.window(z) for z in case["z"]]
    font = s.alloc(0x20)
    mapwho, tcache = s.alloc(0x20), s.alloc(0x20)
    ddraw = s.ddraw(case["devices"])
    labels = {s.g(name): lab for name, lab in FIXED_LABELS.items()}
    labels.update({mapwho: LAB_MAPWHO, tcache: LAB_TCACHE, ddraw: LAB_DDRAW, font: LAB_FONT})
    for i, w in enumerate(windows):
        labels[w] = LAB_WINDOW + i
    lobby = 0
    if case["lobby"] >= 0:
        lobby = s.lobby(windows[case["lobby"]])
        labels[lobby] = LAB_LOBBY
    s.labels = labels
    s.set_objects(windows + [font], case["tag"], case["gw"])
    s.start(labels)                       # before the globals: activating the PowerPC fakes rewrites the import cells

    s.wptr(s.g("gSetOnOffRoot"), windows[case["root"]] if case["root"] >= 0 else 0)
    s.wptr(s.g("gLobby"), lobby)
    s.wptr(s.g("gGameWindow"), windows[case["game_window"]] if case["game_window"] >= 0 else 0)
    s.wptr(s.g("gUIRootWindow"), windows[case["ui_root"]])
    s.wptr(s.g("gei_MapWho"), mapwho)
    s.wptr(s.g("gei_TCache"), tcache)
    s.wptr(s.g("gei_DDraw"), ddraw)
    s.w32(s.g("game_f3c"), case["f3c"])
    s.w8(s.g("running"), case["running"])
    s.w32(s.g("f0c"), case["f0c"])
    s.w32(s.g("render"), case["render"])
    s.w32(s.g("font_index"), case["font_index"])
    st = s.g("settings")
    for name, off in SETTING_WORDS:
        s.w32(st + off, case["settings"][name])
    for name, off in SETTING_BYTES:
        s.w8(st + off, case["settings"][name])
    s.w16s(s.g("convbuf"), [0x4100 + i for i in range(CONV_CHARS)])


def observe(s, case):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    st = s.g("settings")
    root = s.rptr(s.g("gSetOnOffRoot"))
    return {
        "log": s.log(),
        "root": 0 if root == 0 else s.labels.get(root, -1),
        "running": s.r8(s.g("running")),
        "f0c": s.r32(s.g("f0c")),
        "render": s.r32(s.g("render")),
        "font_index": s.r32(s.g("font_index")),
        "f3c": s.r32(s.g("game_f3c")),
        "settings_words": [s.r32(st + off) for _, off in SETTING_WORDS],
        "settings_bytes": [s.r8(st + off) for _, off in SETTING_BYTES],
        "convbuf": s.r16s(s.g("convbuf"), CONV_CHARS),
    }


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

STEM = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :-"


def rint(r):
    return r.choice([0, 1, 2, 3, 4, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-100, 200), r.getrandbits(32)]) & MASK


def make_case(r, **over):
    """The world: random settings and globals; the fakes' answer queues cover every branch."""
    settings = {name: rint(r) for name, _ in SETTING_WORDS}
    for name in ("renderer", "card"):
        settings[name] = r.choice([0, 1, 2, 0xFFFFFFFF])
    for name in ("resolution", "detail"):
        settings[name] = r.choice([0, 1, 2, 3, 4, 0xFFFFFFFF, r.getrandbits(32)])
    settings["quality"] = r.choice([0, 50, 100, r.getrandbits(32)])
    for name, _ in SETTING_BYTES:
        settings[name] = r.choice([0, 1])
    settings.update(over.pop("settings", {}))
    c = {
        "z": [r.choice([0, 1, -1, 0x7FFE, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)]) for _ in range(N_WINDOWS)],
        "devices": r.choice([0, 1, 2, 3, -1, -0x80000000, r.getrandbits(32)]),
        "lobby": r.choice([-1, -1, r.randrange(N_WINDOWS)]),
        "root": -1,
        "game_window": r.choice([-1, r.randrange(N_WINDOWS)]),
        "ui_root": r.randrange(N_WINDOWS),
        "f3c": r.choice([0, 1, 1, 2, -1]) & MASK,
        "running": r.choice([0, 1, 0x5a]),
        "f0c": r.getrandbits(32),
        "render": r.getrandbits(32),
        "font_index": r.choice([6, 0, 1, r.getrandbits(32)]),
        "tag": "".join(r.choice(STEM) for _ in range(r.choice([0, 1, 5, 20, r.randrange(60)]))),
        "settings": settings,
        "gw": [r.randrange(N_WINDOWS) for _ in range(256)],      # GetWindow's answers (about 111 calls)
        "queues": {
            "Build": [r.randrange(N_WINDOWS)],
            "new": [r.choice([1, 1, 1, 0]) for _ in range(20)],
            "Font": [r.choice([FONT_OBJECT, FONT_OBJECT, -1]) for _ in range(20)],
            "GetRenderOptions": [s32(r.getrandbits(32))],
            "UI_SendMessage": [s32(r.getrandbits(32))],
            "GetSFXVolume": [r.choice([0, 50, 100, s32(r.getrandbits(32))])],
            "GetMusicVolume": [r.choice([0, 50, 100, s32(r.getrandbits(32))])],
            "GetSpeechVolume": [r.choice([0, 50, 100, s32(r.getrandbits(32))])],
            "GetMovieVolume": [r.choice([0, 50, 100, s32(r.getrandbits(32))])],
        },
    }
    queues = over.pop("queues", {})
    c.update(over)
    c["queues"].update(queues)
    return c


def edges():
    """Every branch of the asm at least once, each boundary on both sides."""
    r = random.Random(5634)
    yield make_case(r, root=0)                                    # already open: nothing happens
    yield make_case(r, root=3, lobby=2)
    for f3c in (1, 0, 2, 0xFFFFFFFF):                             # the game paused or not
        yield make_case(r, f3c=f3c)
    yield make_case(r, lobby=-1, game_window=-1)                  # the message to a NULL game window
    yield make_case(r, lobby=-1, game_window=4)
    yield make_case(r, lobby=1, game_window=4)
    for renderer in (0, 1, 0xFFFFFFFF):
        yield make_case(r, settings=dict(renderer=renderer))
    for card, devices in ((0, 2), (0, 1), (1, 1), (1, 2), (1, 0), (1, -1), (1, -0x80000000), (5, 0x7FFFFFFF), (0xFFFFFFFF, 3)):
        yield make_case(r, devices=devices, settings=dict(card=card))
    for res in (0xFFFFFFFF, 0, 1, 2, 3, 4, 0x7FFFFFFF, 0x80000000, 0x80000001, 0x147AE14, 0x147AE15):
        yield make_case(r, settings=dict(resolution=res))
    for detail in (0xFFFFFFFF, 0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0x147AE14):
        yield make_case(r, settings=dict(detail=detail))
    for on in (0, 1):
        names = [name for name, _ in SETTING_BYTES]
        yield make_case(r, settings={name: on for name in names})
    yield make_case(r, settings=dict(isometric=1, rmb_scroll=0))
    yield make_case(r, settings=dict(isometric=0, rmb_scroll=1))
    for z in (0x7FFE, 0x7FFF, -1, -0x8000, 0):
        yield make_case(r, z=[z] * N_WINDOWS)
    yield make_case(r, queues={"new": [0] * 20})                  # every allocation NULL: no constructor, a NULL draw object
    yield make_case(r, queues={"new": [1] * 20, "Font": [-1] * 20})
    yield make_case(r, tag="")
    yield make_case(r, tag="".join(r.choice(STEM) for _ in range(59)))   # the longest stem: 62 characters with three hex digits
    yield make_case(r, font_index=0xFFFFFFFF)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CreateOptionsPanel", ppc, host, edge=edges, rnd=rnd),
]
