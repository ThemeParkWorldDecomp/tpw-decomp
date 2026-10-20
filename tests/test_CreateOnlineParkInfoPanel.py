"""Unit v5537, the online park info panel, against libtpw_unit_CreateOnlineParkInfoPanel.dylib (tests/unit_CreateOnlineParkInfoPanel.cpp),
main app ("SimTheme Park"), src/CreateOnlineParkInfoPanel.cpp:
  CreateOnlineParkInfoPanel 0x101eabb4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit is a fake on both sides that records its arguments in the shared log: the park copy, the window calls (a window is
named by a token: 1 the lobby's, 2 the panel, 3 a given parent, otherwise the id it was asked for), the strings (their text is named
by length and hash; the format fake understands %ls and %d), the sprite and title objects (a kind word), the swear filter (it turns
every 'a' into '*', which makes the filtered texts visible in the SetText calls), the numbers table and the advisor.

The world of a case: the park block (Mac layout, with its four texts), whether a panel is open already, the parent, the words of the
city (the short form flag), the swear filter flag, the online system and master save globals (null or made), the login name, the answer
of UI_GetHighestZ, the three callback words and the three globals that keep them. What is compared: the event log, the panel global
(as a window token), the three callback globals, whether the online system and master save globals are set, and the park copy.

No fused operations. Not tested: a NULL gLobby, gLobbyScreens or park (the original reads them unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateOnlineParkInfoPanel"
MASK = 0xFFFFFFFF

FN = 0x101EABB4
SIZES = {FN: 3660}
OWN = set(SIZES)

# ---- the Mac's addresses of the globals (the asm's TOC comments)
ADDR = dict(
    gOnlineParkInfoPanel=0x103961E4,
    gOnlineParkInfoPark=0x10396118,
    gOnlineParkInfoCancel=0x10396114,
    gOnlineParkInfoDownload=0x10396110,
    gOnlineParkInfoContext=0x1039610C,
    gOnlineSystem=0x10380AB0,
    gMasterSave=0x10380A8C,
    gLobby=0x102E4888,
    gLobbyScreens=0x102E49B8,
    gText=0x1037FF18,
    gGame=0x1037F134,
)
HOST_LAYOUT = [2]

SCRIPT = 0x102B0E58
MSGFN = 0x10268588
OPEN_PANEL = 0x5000     # a panel that is open already (never dereferenced)


def s32(v):
    return T.s32(v)


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


# ---------------------------------------------------------------------------------------------------------------- memory helpers

S = {}      # the PowerPC side's world of the running case


def rdw(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def wrw(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def rd_w(e, a):
    """The characters of the wide string at a (without the NUL)."""
    out = []
    while len(out) < 0x4000:
        chunk = e.read(a + 2 * len(out), 64)
        for i in range(0, 64, 2):
            c = (chunk[i] << 8) | chunk[i + 1]
            if c == 0:
                return out
            out.append(c)
    return out


def rd_c(e, a):
    out = []
    while len(out) < 0x1000:
        chunk = e.read(a + len(out), 32)
        for b in chunk:
            if b == 0:
                return out
            out.append(b)
    return out


def sumw(chars):
    """(length, FNV-1a hash) of the characters, the way the host fakes name a text."""
    if chars is None:
        return (-1, 0)
    h = 2166136261
    for c in chars:
        h = ((h ^ c) * 16777619) & MASK
    return (len(chars), h)


def sum_ptr(e, a):
    return (-1, 0) if a == 0 else sumw(rd_w(e, a))


def put_w(e, chars):
    buf = e.alloc(2 * len(chars) + 2)
    e.write(buf, b"".join(struct.pack(">H", c) for c in chars) + b"\0\0")
    return buf


def set16(e, obj, chars):
    """A TbDynamicStringTemplate<wchar_t> (length 4, data 8, capacity c) holding the characters."""
    buf = put_w(e, chars)
    e.write(obj + 4, struct.pack(">III", len(chars), buf, len(chars)))


def cur16(e, obj):
    d = rdw(e, obj + 8)
    return rd_w(e, d) if d else []


def bs_set(e, obj, chars):
    """An MSL basic_string<wchar_t>: the word at 0 points at the rep whose word at 0xc points at the characters."""
    rep = e.alloc(0x20)
    wrw(e, rep + 0xC, put_w(e, chars))
    wrw(e, obj, rep)


def bs_get(e, obj):
    return rd_w(e, rdw(e, rdw(e, obj) + 0xC))


def win(e, token):
    """The window block of a token (made on first use): the token at 4, the fake vtable at 0x118."""
    w = S["wins"].get(token)
    if w is None:
        w = e.alloc(0x130)
        wrw(e, w + 4, token)
        wrw(e, w + 0x118, S["vt"])
        S["wins"][token] = w
    return w


def tok(e, p):
    return rdw(e, p + 4)


def text16(s):
    return [ord(ch) for ch in s]


def do_format(e, fmt, args):
    """%ls (a wide string pointer) and %d (an int) over the fmt characters, the way the host fake does."""
    out = []
    i = 0
    ai = 0
    n = len(fmt)
    while i < n:
        c = fmt[i]
        if c != 0x25:
            out.append(c)
            i += 1
            continue
        i += 1
        if i >= n:
            break
        if fmt[i] == 0x25:
            out.append(0x25)
            i += 1
            continue
        if fmt[i] == 0x6C:
            i += 1
        if i >= n:
            break
        k = fmt[i]
        i += 1
        if k == 0x73:
            a = args[ai]
            ai += 1
            out += rd_w(e, a) if a else text16("(null)")
        elif k == 0x64:
            v = s32(args[ai])
            ai += 1
            out += text16(str(v))
        else:
            out += [0x25, k]
    return out


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        def wrapped(e):
            wrw(e, e.r(1) + 0x14, e.r(2))      # an import glue saves the TOC where the caller reloads it from
            return fn(e)
        STUBS.internal(addr, wrapped)
        return fn
    return deco


@stub(0x1017C2E4)
def st_park_assign(e):
    dst, src = e.r(3), e.r(4)
    e.cl.rec("ParkAssign", (s32(rdw(e, src + 0x48)), s32(rdw(e, src + 0x4C))))
    e.write(dst, e.read(src, 0xC0))
    e.set_r(3, dst)


@stub(0x101CA880)
def st_highest_z(e):
    e.cl.rec("HighestZ", (int(e.r(3) != 0), int(e.r(4) != 0)))
    e.set_r(3, S["z"] & MASK)


@stub(0x101DEB78)
def st_build(e):
    e.cl.rec("Build", (tok(e, e.r(3)), int(e.r(4) == SCRIPT), int(e.r(5) == MSGFN)))
    e.set_r(3, win(e, 2))


@stub(0x101DD8BC)
def st_set_z(e):
    e.cl.rec("SetZ", (tok(e, e.r(3)), s16(e.r(4))))


@stub(0x101DA9D0)
def st_shift(e):
    e.cl.rec("Shift", (tok(e, e.r(3)), s16(e.r(4)), s16(e.r(5))))
    e.set_r(3, 0)


@stub(0x101DBE40)
def st_get_window(e):
    t = s32(e.r(4))
    e.cl.rec("GetWindow", (tok(e, e.r(3)), t))
    e.set_r(3, win(e, t & MASK))


@stub(0x10238740)
def st_str_ctor16(e):
    e.cl.rec("StrCtor16", (0,))
    set16(e, e.r(3), [])


@stub(0x102384B8)
def st_new(e):
    e.set_r(3, e.alloc(e.r(3)))


@stub(0x1016F7D0)
def st_online_ctor(e):
    e.cl.rec("OnlineSystemCtor", (0,))


@stub(0x10178B3C)
def st_city_info(e):
    e.cl.rec("ReturnCityInfo", (s32(e.r(4)),))
    e.set_r(3, S["city"])


@stub(0x1017A588)
def st_theme_name(e):
    n = s32(e.r(4))
    e.cl.rec("ThemeName", (n,))
    b = ("theme%d" % n).encode() + b"\0"
    a = e.alloc(len(b))
    e.write(a, b)
    e.set_r(3, a)


@stub(0x102382D8)
def st_str_ctor8(e):
    obj, src = e.r(3), e.r(4)
    data = rd_c(e, src)
    e.cl.rec("StrCtor8", sumw(data))
    b = bytes(data) + b"\0"
    buf = e.alloc(len(b))
    e.write(buf, b)
    e.write(obj + 4, struct.pack(">III", len(data), buf, len(data)))


@stub(0x1014A478)
def st_localised(e):
    game, theme, out = e.r(3), e.r(4), e.r(5)
    data = rd_c(e, rdw(e, theme + 8))
    ln, h = sumw(data)
    e.cl.rec("LocalisedThemeName", (int(game == ADDR["gGame"]), ln, h))
    set16(e, out, text16("L:") + list(data))


@stub(0x102382F0)
def st_str_dtor8(e):
    e.cl.rec("StrDtor8", (0,))


@stub(0x10238710)
def st_str_dtor16(e):
    e.cl.rec("StrDtor16", (0,))


@stub(0x10180468)
def st_get_string(e):
    i = s32(e.r(3))
    e.cl.rec("GetString", (i,))
    e.set_r(3, put_w(e, text16("S%d" % i)))


@stub(0x1023A918)
def st_format16(e):
    obj, fmt = e.r(3), rd_w(e, e.r(4))
    out = do_format(e, fmt, [e.r(n) for n in range(5, 11)])
    f, r = sumw(fmt), sumw(out)
    e.cl.rec("StrFormat", (f[0], f[1], r[0], r[1]))
    set16(e, obj, out)


@stub(0x1018D11C)
def st_sprite_ctor(e):
    wrw(e, e.r(3), 1)
    e.cl.rec("SpriteCtor", (e.r(4),))


@stub(0x1018FBE8)
def st_title_ctor(e):
    wrw(e, e.r(3), 2)
    e.cl.rec("TitleCtor", (e.r(4),))


@stub(0x101DB624)
def st_set_draw_object(e):
    e.cl.rec("SetClientDrawObject", (tok(e, e.r(3)), rdw(e, e.r(4))))


@stub(0x10180270)
def st_get_font(e):
    i = e.r(3)
    e.cl.rec("GetFont", (i,))
    e.set_r(3, S["fonts"] + 16 * i)


@stub(0x101DA118)
def st_set_font(e):
    e.cl.rec("SetFont", (tok(e, e.r(3)), (e.r(4) - S["fonts"]) // 16))


@stub(0x101DE130)
def st_set_draw_text(e):
    ln, h = sum_ptr(e, e.r(4))
    e.cl.rec("SetClientDrawText", (tok(e, e.r(3)), ln, h))


@stub(0x101DC1EC)
def st_show(e):
    e.cl.rec("Show", (tok(e, e.r(3)), s16(e.r(4))))
    e.set_r(3, 0)


@stub(0x10171834)
def st_login_name(e):
    e.cl.rec("LoginName", (int(e.r(3) != 0),))
    e.set_r(3, S["login"])


@stub(0x102399B8)
def st_wcsicmp(e):
    a, b = rd_w(e, e.r(3)), rd_w(e, e.r(4))

    def low(cs):
        return [c + 32 if 0x41 <= c <= 0x5A else c for c in cs]
    la, lb = low(a), low(b)
    e.set_r(3, 0 if la == lb else (MASK if la < lb else 1))


@stub(0x101694DC)
def st_master_ctor(e):
    e.cl.rec("MasterSaveCtor", (0,))


@stub(0x1016B438)
def st_swear_on(e):
    e.cl.rec("IsSwearFilterOn", (int(e.r(3) != 0),))
    e.set_r(3, 1 if S["swear"] else 0)


@stub(0x10146A14)
def st_bs_ctor_copy(e):
    bs_set(e, e.r(3), rd_w(e, e.r(4)))


@stub(0x101E0B84)
def st_bs_ctor_empty(e):
    bs_set(e, e.r(3), [])


@stub(0x1020D2CC)
def st_filter(e):
    ms, obj, replace = e.r(3), e.r(4), e.r(5) & 0xFF
    chars = bs_get(e, obj)
    ln, h = sumw(chars)
    e.cl.rec("Filter", (int(ms != 0), ln, h, replace))
    out = [0x2A if c == 0x61 else c for c in chars]
    bs_set(e, obj, out)
    e.set_r(3, 1 if out != chars else 0)


@stub(0x101468F8)
def st_bs_dtor(e):
    pass


@stub(0x10239328)
def st_str_assign(e):
    src = rd_w(e, e.r(4))
    e.cl.rec("StrAssign", sumw(src))
    set16(e, e.r(3), src)


@stub(0x1023A8A0)
def st_str_append(e):
    src = rd_w(e, e.r(4))
    e.cl.rec("StrAppend", sumw(src))
    set16(e, e.r(3), cur16(e, e.r(3)) + src)


@stub(0x10239730)
def st_format_string(e):
    this, out, size, i = e.r(3), e.r(4), e.r(5), e.r(6)
    tags = [e.r(7), e.r(8), e.r(9)]
    info = []
    for t in tags:
        info += [rdw(e, t), rdw(e, t + 8), s32(rdw(e, t + 0xC))]
    e.cl.rec("FormatString", tuple([int(this != 0), size, i] + info))
    s = "FS%d" % i
    for k in range(3):
        s += ":%d=%d" % (info[3 * k + 1], info[3 * k + 2])
    chars = text16(s)
    e.write(out, b"".join(struct.pack(">H", c) for c in chars) + b"\0\0")
    e.set_r(3, len(chars))


@stub(0x102389C8)
def st_wcslen(e):
    e.set_r(3, len(rd_w(e, e.r(3))))


@stub(0x101B2510)
def st_bs_assign(e):
    obj, src, n = e.r(3), e.r(4), e.r(5)
    chars = []
    while len(chars) < n:
        chars.append(struct.unpack(">H", e.read(src + 2 * len(chars), 2))[0])
    bs_set(e, obj, chars)


@stub(0x101DA2D8)
def st_set_font_wrap(e):
    e.cl.rec("SetFontWrap", (tok(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(0x101DA204)
def st_set_font_just(e):
    e.cl.rec("SetFontJust", (tok(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(0x1023BAA8)
def st_ptr_glue(e):
    """The call through the window's vtable slot 0x2c: SetText(window, text)."""
    assert e.r(12) == S["desc"], hex(e.r(12))
    wrw(e, e.r(1) + 0x14, e.r(2))      # the glue saves the TOC where the caller reloads it from
    ln, h = sum_ptr(e, e.r(4))
    e.cl.rec("SetText", (tok(e, e.r(3)), ln, h))
    e.set_r(3, 1)


@stub(0x1018BD3C)
def st_title_colour(e):
    e.cl.rec("UI_TITLE_COLOUR", (tok(e, e.r(3)),))


@stub(0x101DCCEC)
def st_capture_kb(e):
    e.cl.rec("CaptureKb", (tok(e, e.r(3)), s32(e.r(4))))


@stub(0x100A7074)
def st_advisor_add(e):
    e.cl.rec("AdvisorAdd", (s32(e.r(4)), s32(e.r(5))))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wide(self, chars):
        a = self.alloc(2 * len(chars) + 2)
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", c) for c in chars) + b"\0\0")
        return a

    def wptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.fmt[0] + self.pfmt, self.read(a, self.psz))[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    pfmt = "I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def prepare(self, case, login):
        e = self.e
        S.clear()
        S.update(f60=case["f60"], swear=case["swear"], login=login, z=case["z"], wins={})
        S["vt"] = e.alloc(0x80)
        S["desc"] = e.alloc(8)
        wrw(e, S["vt"] + 0x2C, S["desc"])
        S["fonts"] = e.alloc(0x200)
        S["city"] = e.alloc(0x70)
        wrw(e, S["city"] + 0x60, case["f60"])
        lobby = e.alloc(0x40)
        wrw(e, lobby + 4, win(e, 1))
        wrw(e, ADDR["gLobby"], lobby)
        screens = e.alloc(0x40)
        wrw(e, screens + 0x14, e.alloc(0x40))
        wrw(e, ADDR["gLobbyScreens"], screens)
        wrw(e, ADDR["gText"] + 0x564, 0x1234)
        e.write(ADDR["gOnlineParkInfoPark"], bytes(0xC4))

    def token(self, p):
        return rdw(self.e, p + 4)

    def park_dump(self):
        e, g = self.e, ADDR["gOnlineParkInfoPark"]
        w = lambda off: rdw(e, g + off)
        out = [w(0x08), w(0x48), w(0x4C), w(0x50), w(0x54)]
        out += list(struct.unpack(">3H", e.read(g + 0xA8, 6)))
        out += [w(0xB8), w(0xBC)]
        for off in (0x14, 0x28, 0x3C, 0x9C):
            out += list(sum_ptr(e, w(off)))
        return out

    def call(self, blk, parent, cb, dl, ctx):
        pw = win(self.e, 3) if parent else 0
        T.call_ppc(self.e, FN, [blk, pw, cb, dl, ctx])


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    pfmt = "Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pi_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def prepare(self, case, login):
        self.lib.tpw_pi_prepare(s32(case["f60"]), case["swear"], login, s32(case["z"]))

    def token(self, p):
        return self.lib.tpw_pi_token(p)

    def park_dump(self):
        out = (C.c_int64 * 32)()
        n = self.lib.tpw_pi_park_dump(out)
        return list(out)[:n]

    def call(self, blk, parent, cb, dl, ctx):
        self.lib.tpw_pi_call(blk, 1 if parent else 0, cb, dl, ctx)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_pi_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pi_prepare.restype, lib.tpw_pi_prepare.argtypes = None, [C.c_int32, C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_pi_token.restype, lib.tpw_pi_token.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pi_park_dump.restype, lib.tpw_pi_park_dump.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_pi_call.restype, lib.tpw_pi_call.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    p = case["park"]
    blk = s.alloc(0xE0)
    s.words(blk + 0x08, [p["id"]])
    for off, key in ((0x0C, "t0c"), (0x20, "t20"), (0x34, "t34"), (0x94, "t94")):
        a = s.wide(p[key])
        s.words(blk + off + 4, [len(p[key]), a, len(p[key])])
    s.words(blk + 0x48, [p["f48"], p["f4c"], p["f50"], p["f54"]])
    s.halves(blk + 0xA8, [p["fa8"], p["faa"], p["fac"]])
    s.words(blk + 0xB8, [p["fb8"], p["fbc"]])
    login = s.wide(case["login"])
    s.prepare(case, login)
    s.wptr(s.g("gOnlineParkInfoPanel"), OPEN_PANEL if case["panel"] else 0)
    cb, dl, ctx = case["cb"]
    # the globals that keep the callbacks hold other words before the call (an open panel leaves them alone)
    s.wptr(s.g("gOnlineParkInfoCancel"), case["old"][0])
    s.wptr(s.g("gOnlineParkInfoDownload"), case["old"][1])
    s.wptr(s.g("gOnlineParkInfoContext"), case["old"][2])
    s.wptr(s.g("gOnlineSystem"), s.alloc(0x2000) if case["sys"] else 0)
    s.wptr(s.g("gMasterSave"), s.alloc(0x2000) if case["ms"] else 0)
    return blk


def execute(s, case, blk):
    try:
        s.call(blk, case["parent"], *case["cb"])
    except RuntimeError:
        if isinstance(s, Ppc):
            print("log so far", s.L.log[-6:], "r2", hex(s.e.r(2)), "r1", hex(s.e.r(1)))
        raise


def observe(s, case):
    """Everything the call may change, read back the same way on both sides."""
    panel = s.rptr(s.g("gOnlineParkInfoPanel"))
    if panel not in (0, OPEN_PANEL):
        panel = s.token(panel)
    return {
        "log": s.log(),
        "panel": panel,
        "cancel": s.rptr(s.g("gOnlineParkInfoCancel")),
        "download": s.rptr(s.g("gOnlineParkInfoDownload")),
        "context": s.rptr(s.g("gOnlineParkInfoContext")),
        "sys": int(s.rptr(s.g("gOnlineSystem")) != 0),
        "ms": int(s.rptr(s.g("gMasterSave")) != 0),
        "park": s.park_dump(),
    }


def run(s, case):
    try:
        blk = build(s, case)
        execute(s, case, blk)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    if not getattr(ppc, "shown", False):
        ppc.shown = True
        print("pool", e.read(0x1024F08A, 0x60).hex())
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

ALPHABET = "abcdeABC xyz019_*!?"
ASCII_NAME = "abcXYZ019 _"


def rtext(r, lo=0, hi=8, alphabet=ALPHABET):
    n = r.randint(lo, hi) if r.random() < 0.9 else r.randint(20, 60)
    out = []
    for _ in range(n):
        if r.random() < 0.05:
            out.append(r.choice([0x00E9, 0x263A, 0x4E2D, 0xFFFD]))
        else:
            out.append(ord(r.choice(alphabet)))
    return out


def rword(r):
    k = r.random()
    if k < 0.3:
        return r.randrange(0, 20)
    if k < 0.5:
        return r.choice([0, 1, 0x7FFFFFFF, 0x80000000, MASK, 0x7FFFFFFE])
    return r.randrange(0, 1 << 32)


def base(r):
    login = rtext(r, 1, 8, ASCII_NAME)
    owner_kind = r.choice(["same", "case", "diff"])
    if owner_kind == "same":
        owner = list(login)
    elif owner_kind == "case":
        owner = [c ^ 0x20 if 0x41 <= (c | 0x20) <= 0x5A else c for c in login]
    else:
        owner = rtext(r, 0, 8, ASCII_NAME)
    park = {
        "id": rword(r),
        "t0c": rtext(r), "t20": rtext(r), "t34": owner, "t94": rtext(r, 0, 10),
        "f48": r.choice([0, 1, 5, r.randrange(0, 1 << 32)]), "f4c": rword(r), "f50": rword(r), "f54": rword(r),
        "fa8": r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0, 65536)]),
        "faa": r.randrange(0, 65536), "fac": r.randrange(0, 65536),
        "fb8": r.choice([0x7FFFFFFF, 0, 5, rword(r)]), "fbc": r.choice([0, 1, rword(r)]),
    }
    return {
        "op": "c", "queues": {}, "park": park, "login": login,
        "panel": 0, "parent": r.choice([0, 1]),
        "f60": r.choice([0, 0, 1, rword(r)]), "swear": r.choice([0, 1]), "sys": r.choice([0, 1]), "ms": r.choice([0, 1]),
        "z": r.choice([0, 1, -5, 100, 32765, 32766, 32767, 65532, r.randrange(-70000, 70000)]),
        "cb": [r.randrange(0x1000, 0x2000) * 4 for _ in range(3)],
        "old": [r.randrange(0x100, 0x200) * 4 for _ in range(3)],
    }


def edges():
    """Every branch of the asm at least once: the panel open already, the short and long forms, each flag both ways."""
    r = random.Random(5370)
    for parent in (0, 1):
        c = base(r)
        c["panel"] = 1
        c["parent"] = parent
        yield c
    for f60 in (1, -1, 0x10000):
        for parent in (0, 1):
            for sys in (0, 1):
                for ms in (0, 1):
                    for swear in (0, 1):
                        c = base(r)
                        c.update(f60=f60 & MASK, parent=parent, sys=sys, ms=ms, swear=swear)
                        yield c
    for parent in (0, 1):
        for sys in (0, 1):
            for ms in (0, 1):
                for swear in (0, 1):
                    for empty in (0, 1):
                        for sentinel in (0, 1):
                            for fbc in (0, 1):
                                for owner in ("same", "case", "diff"):
                                    c = base(r)
                                    c.update(f60=0, parent=parent, sys=sys, ms=ms, swear=swear)
                                    p = c["park"]
                                    p["t94"] = [] if empty else text16("note a b")
                                    p["fb8"] = 0x7FFFFFFF if sentinel else r.choice([0, 5, 0x80000000, MASK])
                                    p["fbc"] = fbc
                                    if owner == "same":
                                        p["t34"] = list(c["login"])
                                    elif owner == "case":
                                        p["t34"] = [ch ^ 0x20 if 0x41 <= (ch | 0x20) <= 0x5A else ch for ch in c["login"]]
                                    else:
                                        p["t34"] = c["login"] + text16("x")
                                    p["t0c"] = text16("abc") if swear else p["t0c"]
                                    p["t20"] = text16("aaa b") if swear else p["t20"]
                                    yield c


def rnd(r):
    c = base(r)
    if r.random() < 0.05:
        c["panel"] = 1
    return c


TESTS = [
    FnTest("CreateOnlineParkInfoPanel", ppc, host, edge=edges, rnd=rnd),
]
