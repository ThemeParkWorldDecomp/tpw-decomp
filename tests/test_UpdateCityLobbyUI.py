"""Unit v5189, the city lobby's controller window, against libtpw_unit_UpdateCityLobbyUI.dylib (tests/unit_UpdateCityLobbyUI.cpp), main app
("SimTheme Park"), src/UI_IslandLobby_msg_sinit_ui_cityLobbyController_cpp_UpdateCityLobbyUI.cpp:
  UpdateCityLobbyUI 0x101e0928, CreateCityLobbyController 0x101e05e4, CityLobbyControllerMsg 0x101e0400

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments (objects as labels) and answer from the case's queues: the window members
(GetWindow, Show, Build, SetClientDrawObject, SetFont, SetFontJust, SetFontColour, CallDefaultWindowHandler), the title object and master
save constructors (new labels), GetFontTableEntry, UI_TITLE_COLOUR, CreateOnlineParkListPanel, the advisor's Add, IsSwearFilterOn, the swear
filter (which edits the string), the string constructor, assign and destructor, and every virtual call the Mac makes through __ptr_glue
(a window's SetText, the screens' Show and Hide, the online islands' slots 0x34 to 0x48). swprintf and wcslen are glue on the Mac (a Python
"%d" and a count here) and the real ones from platform/msl.cpp on the host.

The world of a case: the controller window global (NULL or the window), the master save global (NULL or an object), the lobby with its window
(NULL or a window), the screen system with its five screens, an island (NULL, no park, or an order and a park name), the convert buffer
prefilled with a marker. What is compared: the answer (the message function), the event log, the window global and the master save global as
labels, the first 0x40 characters of the convert buffer.

No fused operations. Not tested: a NULL gLobby or gLobbyScreens, a controller window whose GetWindow answers NULL (the original reads them
unconditionally).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateCityLobbyUI"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN_UPDATE, FN_CREATE, FN_MSG = 0x101E0928, 0x101E05E4, 0x101E0400
SIZES = {FN_UPDATE: 552, FN_CREATE: 476, FN_MSG: 416}
OWN = set(SIZES)

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_cl_addr(name)
ADDR = dict(win=0x10395C70, master=0x10380A8C, buf=0x10380C48, lobby=0x102E4888, screens=0x102E49B8, pool=0x1024EDC2,
            script=0x102AFE7A, tv=0x10268498, cancel=0x102684A0, table=0x102B004C)
POOL_CHARS = [ord("%"), ord("d"), 0]      # the pool's first string, the number format

# object labels in the logs
LABELS = dict(W=1, W9=2, Wa=3, P=4, ONL=5, THM=6, GLB=7, ADV=8, MST=9)
TOKEN = 0x7E000000                         # a fake vtable slot's value: TOKEN | the Mac's slot offset


def s32(v):
    return T.s32(v)


def th(chars):
    h = 0
    for u in chars:
        h = (h * 31 + u) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CTX = {}


def rd_chars(e, a, n=None):
    out = []
    while n is None or len(out) < n:
        u = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if n is None and (u == 0 or len(out) >= 0x4000):
            break
        out.append(u)
    return out


def wr_chars(e, a, chars):
    e.write(a, b"".join(struct.pack(">H", u) for u in chars))


def glue_ret(e):
    """The glue saves the TOC at 0x14(r1) and the caller loads it back after the call."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))


def stub_wctor(e):
    rep, data = e.alloc(0x10), e.alloc(0x10)
    e.write(rep, struct.pack(">IIII", 0, 7, 1, data))
    e.write(e.r(3), struct.pack(">I", rep))
    e.cl.rec("WStringCtor", (1,))


def stub_wdtor(e):
    rep = e.u32(e.r(3))
    e.cl.rec("WStringDtor", (T.s16(e.r(4)), e.u32(rep)))


def stub_wassign(e):
    self_, text, n = e.r(3), e.r(4), e.r(5)
    chars = rd_chars(e, text, n)
    e.cl.rec("WStringAssign", (n, th(chars)))
    rep = e.u32(self_)
    data = e.alloc((n + 1) * 2)
    wr_chars(e, data, chars)
    e.write(rep, struct.pack(">I", n))
    e.write(rep + 4, struct.pack(">I", n))
    e.write(rep + 0xC, struct.pack(">I", data))
    e.set_r(3, self_)


def stub_getwindow(e):
    L = e.cl
    i = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), i))
    e.set_r(3, CTX["W9"] if i == 0x49D9 else (CTX["Wa"] if i == 0x49DA else 0))


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), int(e.r(4) == ADDR["script"]), int(e.r(5) == ADDR["tv"])))
    e.set_r(3, CTX["W"] if L.ret("Build") else 0)


def stub_font_entry(e):
    i = e.r(3)
    e.cl.rec("GetFontTableEntry", (i,))
    e.set_r(3, (CTX["fonts"] + 16 * (i & 15)) & MASK)


def stub_setfont(e):
    L = e.cl
    L.rec("SetFont", (L.lab(e.r(3)), ((e.r(4) - CTX["fonts"]) & MASK) // 16))


def stub_filter(e):
    L = e.cl
    rep = e.u32(e.r(4))
    n = e.u32(rep)
    data = e.u32(rep + 0xC)
    chars = rd_chars(e, data, n)
    L.rec("Filter", (L.lab(e.r(3)), n, th(chars), e.r(5) & 0xFF))
    answer = L.ret("Filter") != 0
    if answer:
        if chars:
            chars[0] = ord("*")
        if len(chars) > 1:
            chars[-1] = ord("#")
        wr_chars(e, data, chars)
    e.set_r(3, int(answer))


def stub_ptr_glue(e):
    """A virtual call: r12 holds the slot's value (TOKEN | slot), r3 the object, r4 the text of SetText."""
    L = e.cl
    r12 = e.r(12)
    assert (r12 >> 8) == (TOKEN >> 8), hex(r12)
    slot = r12 & 0xFF
    if slot == 0x2C:
        text = e.r(4)
        if text == 0:
            L.rec("SetText", (L.lab(e.r(3)), 0, 0, 0))
        else:
            chars = rd_chars(e, text)
            L.rec("SetText", (L.lab(e.r(3)), 1 if text == ADDR["buf"] else 2, len(chars), th(chars)))
    else:
        L.rec("vcall", (L.lab(e.r(3)), slot))
    glue_ret(e)


def fmt_apply(fmt, value):
    """swprintf for the formats this module uses: "%d" and plain text."""
    text = "".join(chr(u) for u in fmt)
    assert text.replace("%d", "").count("%") == 0, text
    return [ord(c) for c in text.replace("%d", str(value))]


def stub_swprintf(e):
    buf, size, fmt, value = e.r(3), e.r(4), e.r(5), s32(e.r(6))
    fchars = rd_chars(e, fmt)
    e.cl.rec("swprintf", (size, th(fchars), value))
    out = fmt_apply(fchars, value)
    if size > 0:
        out = out[: size - 1]
        wr_chars(e, buf, out + [0])
    e.set_r(3, len(out))
    glue_ret(e)


def stub_wcslen(e):
    e.set_r(3, len(rd_chars(e, e.r(3))))
    glue_ret(e)


STUBS.internal(0x101E0B84, stub_wctor)
STUBS.internal(0x101468F8, stub_wdtor)
STUBS.internal(0x101B2510, stub_wassign)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x101DC1EC, CL.recorder("Show", "Lh"))
STUBS.internal(0x101DEB78, stub_build)
STUBS.internal(0x101DE318, CL.recorder("Default", "Liii", "i"))
STUBS.internal(0x101DB624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x101DA118, stub_setfont)
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x101DA3AC, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x10180270, stub_font_entry)
STUBS.internal(0x1018BD3C, CL.recorder("UI_TITLE_COLOUR", "L"))
STUBS.internal(0x1018FBE8, CL.recorder("TitleCtor", "Nu"))
STUBS.internal(0x101EC4E0, lambda e: e.cl.rec("CreateOnlineParkListPanel", (int(e.r(3) == ADDR["cancel"]),)))
STUBS.internal(0x100A7074, CL.recorder("Add", "Lii"))
STUBS.internal(0x1016B438, CL.recorder("IsSwearFilterOn", "L", "b"))
STUBS.internal(0x101694DC, CL.recorder("MasterCtor", "N"))
STUBS.internal(0x1020D2CC, stub_filter)
STUBS.internal(0x1023BAA8, stub_ptr_glue)
STUBS.internal(0x10239160, stub_swprintf)
STUBS.internal(0x102389C8, stub_wcslen)


def setup(runner):
    T.install_std_stubs(runner.emu)       # operator new and the rest of the C library


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def chars(self, a, cs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", u) for u in cs))

    def rd_chars16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def make_world(self):
        """The objects every case has (labelled in the log), made by the side's own helpers."""
        self.o = dict(W=self.mk_win(), W9=self.mk_win(), Wa=self.mk_win(), P=self.mk_win(),
                      ONL=self.mk_screen(), THM=self.mk_screen(), GLB=self.mk_screen(), ADV=self.mk_screen(), MST=self.mk_plain())


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_world()
        CTX.update(W=self.o["W"], W9=self.o["W9"], Wa=self.o["Wa"], fonts=e.alloc(0x100))
        self.L = e.cl = CL.PpcLog({self.o[k]: v for k, v in LABELS.items()}, case["queues"])
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

    def lab(self, a):
        return self.L.lab(a)

    def _slots(self, vt):
        for s in range(0x0C, 0x80, 4):
            self.words(vt + s, [TOKEN | s])

    def mk_win(self):
        a, vt = self.alloc(0x400), self.alloc(0x100)
        self._slots(vt)
        self.words(a + 0x118, [vt])
        return a

    def mk_screen(self):
        a, vt = self.alloc(0x400), self.alloc(0x100)
        self._slots(vt)
        self.words(a, [vt])
        return a

    def mk_plain(self):
        return self.alloc(0x400)

    def mk_lobby(self, window):
        a = self.alloc(0x40)
        self.words(a + 4, [window])
        return a

    def mk_screens(self, theme, online, globe, advisor):
        a = self.alloc(0x40)
        self.words(a + 4, [theme, online, globe, 0, advisor])
        return a

    def mk_park(self, cs):
        a, t = self.alloc(0x40), self.alloc(2 * len(cs) + 2)
        self.chars(t, cs)
        self.words(a + 0x14, [t])
        return a

    def mk_island(self, order, park):
        a = self.alloc(0x80)
        self.words(a + 0x24, [order])
        self.words(a + 0x70, [park])
        return a

    def set_globals(self, window, master, lobby, screens):
        self.words(ADDR["win"], [window])
        self.words(ADDR["master"], [master])
        self.words(ADDR["lobby"], [lobby])
        self.words(ADDR["screens"], [screens])

    def get_window(self):
        return self.rd_word(ADDR["win"])

    def get_master(self):
        return self.rd_word(ADDR["master"])

    def check_pool(self):
        got = self.rd_chars16(ADDR["pool"], 3)
        assert got == POOL_CHARS, ("the Mac's pool starts with", got, self.read(ADDR["pool"], 0x60))

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_world()
        CL.host_begin(lib, {v: self.o[k] for k, v in LABELS.items()}, case["queues"])
        lib.tpw_cl_set_ctx(self.o["W"], self.o["W9"], self.o["Wa"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def lab(self, a):
        return self.lib.tpw_cl_labp(a)

    def mk_win(self):
        return self.lib.tpw_cl_mk_win()

    def mk_screen(self):
        return self.lib.tpw_cl_mk_screen()

    def mk_plain(self):
        return self.lib.tpw_cl_mk_plain()

    def mk_lobby(self, window):
        return self.lib.tpw_cl_mk_lobby(window)

    def mk_screens(self, theme, online, globe, advisor):
        return self.lib.tpw_cl_mk_screens(theme, online, globe, advisor)

    def mk_park(self, cs):
        arr = (C.c_uint16 * (len(cs) + 1))(*cs)
        return self.lib.tpw_cl_mk_park(arr, len(cs))

    def mk_island(self, order, park):
        return self.lib.tpw_cl_mk_island(s32(order), park)

    def set_globals(self, window, master, lobby, screens):
        self.lib.tpw_cl_set_globals(window, master, lobby, screens)

    def get_window(self):
        return self.lib.tpw_cl_get_window()

    def get_master(self):
        return self.lib.tpw_cl_get_master()

    def check_pool(self):
        got = list(struct.unpack("<3H", self.read(self.g("pool"), 6)))
        assert got == POOL_CHARS, got

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_UPDATE:
            lib.tpw_cl_update(args[0])
            return 0
        if fn == FN_CREATE:
            lib.tpw_cl_create()
            return 0
        return lib.tpw_cl_msg(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    """The host library's entry points, typed once."""
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    sig = {
        "addr": (u64, [C.c_char_p]), "mk_win": (u64, []), "mk_screen": (u64, []), "mk_plain": (u64, []),
        "mk_lobby": (u64, [u64]), "mk_screens": (u64, [u64, u64, u64, u64]), "mk_park": (u64, [C.POINTER(C.c_uint16), i32]),
        "mk_island": (u64, [i32, u64]), "set_ctx": (None, [u64, u64, u64]), "set_globals": (None, [u64, u64, u64, u64]),
        "get_window": (u64, []), "get_master": (u64, []), "labp": (i32, [u64]), "update": (None, [u64]), "create": (None, []),
        "msg": (i32, [u64, i32, i32, i32]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, "tpw_cl_" + name)
        f.restype, f.argtypes = res, args
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks: the same code for both memories (offsets are the Mac's)."""
    s.check_pool()
    o = s.o
    s.chars(s.g("buf"), [0x5A5A] * 0x40)
    isl = case.get("island")
    s.island = 0
    if isl is not None:
        park = s.mk_park([ord(c) for c in isl["text"]]) if isl["text"] is not None else 0
        s.island = s.mk_island(isl["order"], park)
    lobby = s.mk_lobby(o["P"] if case["lobbyWin"] else 0)
    screens = s.mk_screens(o["THM"], o["ONL"], o["GLB"], o["ADV"])
    s.set_globals(o["W"] if case["win"] else 0, o["MST"] if case["master"] else 0, lobby, screens)


def execute(s, case):
    """The call."""
    op = case["op"]
    if op == "update":
        s.call(FN_UPDATE, [s.island])
        return 0
    if op == "create":
        s.call(FN_CREATE, [])
        return 0
    return s.call(FN_MSG, [s.o[case["window"]], case["msg"], case["a"], case["b"]])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    return {"answer": answer, "log": s.log(), "win": s.lab(s.get_window()), "master": s.lab(s.get_master()),
            "buf": s.rd_chars16(s.g("buf"), 0x40)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TEXTS = ["", "a", "Hello", "xyz", "Zoo Park", "x" * 37, "été", "ab"]
ORDERS = [0, 1, 2, 9, 10, 99, -1, -5, 0x7FFFFFFF, -0x80000000, 123456]
MSGS = [0x100, 0x14, 0, 0xFF, 0x101, -1, 0x7FFFFFFF, -0x80000000, 0x13, 0x15, 0x17, 0x200]
AS = [0x49D4, 0x49D5, 0x49D6, 0x49D7, 0x49D8, 0x49D9, 0x49DA, 0x49DB, 0x49DC, 0x49DD, 0, -1, 0x7FFFFFFF, -0x80000000, 0x49D5 - 0x80000000]


def base(r, op):
    """The world every case starts from: random globals; the fakes' answer queues cover every branch."""
    return {"op": op, "win": r.choice([0, 1, 1]), "master": r.choice([0, 1]), "lobbyWin": r.choice([0, 1, 1]),
            "queues": {"Default": [r.choice([0, 1, -1, r.randrange(-2 ** 31, 2 ** 31)])], "Build": [r.choice([0, 1])],
                       "IsSwearFilterOn": [r.choice([0, 1])], "Filter": [r.choice([0, 1])]}}


def with_queues(c, swear=None, filt=None, build_=None, default=None):
    q = c["queues"]
    if swear is not None:
        q["IsSwearFilterOn"] = [swear]
    if filt is not None:
        q["Filter"] = [filt]
    if build_ is not None:
        q["Build"] = [build_]
    if default is not None:
        q["Default"] = [default]
    return c


# ---- UpdateCityLobbyUI
def update_case(r, island="random"):
    c = base(r, "update")
    if island == "random":
        k = r.random()
        if k < 0.15:
            island = None
        elif k < 0.3:
            island = {"order": r.choice(ORDERS), "text": None}
        else:
            island = {"order": r.choice(ORDERS + [r.randrange(-1000, 1000)]), "text": r.choice(TEXTS)}
    c["island"] = island
    return c


def update_edges():
    r = random.Random(1189)
    for win, master, swear, filt in itertools.product((0, 1), (0, 1), (0, 1), (0, 1)):
        for island in (None, {"order": 3, "text": None}, {"order": 0, "text": ""}, {"order": 1, "text": "Hello"},
                       {"order": 0x7FFFFFFF, "text": "x"}, {"order": -1, "text": "ab"}, {"order": 12, "text": "Zoo Park"}):
            c = update_case(r, island)
            c["win"], c["master"] = win, master
            yield with_queues(c, swear, filt)


def update_random(r):
    return update_case(r)


# ---- CreateCityLobbyController
def create_case(r):
    return base(r, "create")


def create_edges():
    r = random.Random(2189)
    for win, lobby_win, build_ in itertools.product((0, 1), (0, 1), (0, 1)):
        c = create_case(r)
        c["win"], c["lobbyWin"] = win, lobby_win
        yield with_queues(c, build_=build_)


def create_random(r):
    return create_case(r)


# ---- CityLobbyControllerMsg
def msg_case(r, msg=None, a=None, b=None):
    c = base(r, "msg")
    c["window"] = r.choice(["W", "P"])
    c["msg"] = r.choice(MSGS) if msg is None else msg
    c["a"] = (r.choice(AS) if r.random() < 0.8 else r.randrange(-2 ** 31, 2 ** 31)) if a is None else a
    c["b"] = r.choice([0, 1, -1, r.randrange(-2 ** 31, 2 ** 31)]) if b is None else b
    if c["msg"] == 0x100 and a is None and r.random() < 0.8:
        c["a"] = 0x49D5 + r.randrange(0, 8)
    return c


def msg_edges():
    r = random.Random(3189)
    for a in AS:
        yield msg_case(r, 0x100, a)
    for msg in MSGS:
        for win in (0, 1):
            c = msg_case(r, msg, 0x49D5)
            c["win"] = win
            yield c
    for a in range(0x49D5, 0x49DD):
        for win in (0, 1):
            c = msg_case(r, 0x100, a)
            c["win"] = win
            yield c


def msg_random(r):
    return msg_case(r)


TESTS = [
    FnTest("UpdateCityLobbyUI", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("CreateCityLobbyController", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("CityLobbyControllerMsg", ppc, host, edge=msg_edges, rnd=msg_random),
]
