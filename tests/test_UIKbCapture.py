"""Unit w49: the decision of which window holds the keyboard after a dialog closes, against libtpw_unit_UIKbCapture.dylib
(tests/unit_UIKbCapture.cpp).

Five functions run for real on both sides: UIMain_DecideKbCapture (0x1018116c), MainMenuUp (0x101fc1ec), UIMenu::IsShown (0x1018b298),
CoasterEditPanelUp (0x1019cfdc) and CoasterEditPanelGetKb (0x1019d014), and with them three small functions of other units that are real
on the PowerPC side: CKeyTable::Enable (0x101500b8), InterfaceWindow::GetAttr (0x101dbb70) and InterfaceWindow::CaptureKb (0x101dccec); on
the host, Enable and CaptureKb are the translated sources and GetAttr is its one line in the unit file (its file is the message pump).
The only fake is the message poster CaptureKb calls (UI_PostMessage, 0x101c9400), which records its arguments on both sides. The order
of the three Enable calls is not visible (Enable writes one word and records nothing); the mutation pass can only see which tables were
enabled.

The world is ten windows (never constructed: only the attribute word at 0x44 is read), the main menu object, the lobby (only the window
at +4), the three key tables the decision enables (camera, game, cheat) and the window globals. After every case the log, the window that
has the keyboard (gUIKeyboardWindow) and the three tables' enabled words are compared.

One case is left out on purpose: a game window with no bankrupt window and no first-person window. The original reads the attributes of
the first-person window with no test for NULL there (an address-0 read on the Mac), which the host cannot do.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIKbCapture"

MASK = 0xFFFFFFFF

# ---- the unit's functions (the ops of the host driver)
DECIDE, MENU_UP, COASTER_UP, COASTER_GETKB, IS_SHOWN = range(5)
ENTRIES = [0x1018116C, 0x101FC1EC, 0x1019CFDC, 0x1019D014, 0x1018B298]
# real on both sides: Enable, GetAttr, CaptureKb; the fake: UI_PostMessage
ENABLE, GET_ATTR, CAPTURE_KB, POST = 0x101500B8, 0x101DBB70, 0x101DCCEC, 0x101C9400

# ---- the original's data: the eight window globals (in the order of the host's tpw_kb_windows), the menu, the coaster flag, the tables
WINDOW_GLOBALS = [0x103949E4, 0x103949E0, 0x103949DC, 0x10395E98, 0x10395E94, 0x1038CC58, 0x1038CC5C, 0x10394834]
GAME, FIRST_PERSON, BUMHOLE, ONLINE_PANEL, ONLINE_WINDOW, BANKRUPT, PANEL, COASTER_PANEL = range(8)
MENU, COASTER_FLAG, LOBBY, KEYBOARD = 0x10396A58, 0x10394830, 0x102E4888, 0x10395C10
TABLE_GLOBALS = [0x1037F22C, 0x1037F230, 0x1037F228]   # camera, game, cheat

WINDOWS = 10
LAB_WIN = 10
ATTR = 0x44            # the window's attribute word
ENABLED = 6            # the key table's enabled word (16 bits)
SAVED = WINDOW_GLOBALS + [MENU, COASTER_FLAG, LOBBY, KEYBOARD] + TABLE_GLOBALS


def s32(v):
    return T.s32(v & MASK)


def case(op=DECIDE, windows=None, menu=None, flag=0, lobby=(0, 0), keyboard=-1, attrs=None, enabled=(0, 0, 0)):
    """One case. windows: the eight window globals as window indexes ({GAME: 3, ...}; absent means NULL). menu: None for no menu, else
    (the word IsShown answers, the menu's window index). lobby: (0 none | 1 without window | 2 with window, the window index). keyboard:
    the window that has it before the call (-1 none). attrs: the ten windows' attribute words. enabled: the three tables' words."""
    w = [-1] * 8
    for slot, index in (windows or {}).items():
        w[slot] = index
    return dict(op=op, w=w, menu=menu, flag=flag, lobby=list(lobby), keyboard=keyboard,
                attrs=list(attrs) if attrs is not None else [0] * WINDOWS, enabled=list(enabled))


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the world in the emulator's memory, the recording poster, the dump."""

    def __init__(self, e, c):
        self.e = e
        self.case = c
        self.win = [e.alloc(0x140, bytes(0x140)) for _ in range(WINDOWS)]
        self.table = [e.alloc(0x18, bytes(0x18)) for _ in range(3)]
        self.menu = e.alloc(0x14, bytes(0x14))
        self.lobby = e.alloc(0x40, bytes(0x40))
        labels = {self.win[i]: LAB_WIN + i for i in range(WINDOWS)}
        self.L = e.cl = CL.PpcLog(labels, {})
        self.saved = {a: bytes(e.read(a, 4)) for a in SAVED}
        for i, word in enumerate(c["attrs"]):
            self.w32(self.win[i] + ATTR, word)
        for slot, address in enumerate(WINDOW_GLOBALS):
            self.w32(address, self.at(c["w"][slot]))
        self.w32(COASTER_FLAG, c["flag"])
        if c["menu"] is not None:
            shown, window = c["menu"]
            self.w32(self.menu + 0xC, shown)
            self.w32(self.menu + 0x10, self.at(window))
        self.w32(MENU, self.menu if c["menu"] is not None else 0)
        kind, window = c["lobby"]
        self.w32(self.lobby + 4, self.at(window) if kind == 2 else 0)
        self.w32(LOBBY, self.lobby if kind != 0 else 0)
        self.w32(KEYBOARD, self.at(c["keyboard"]))
        for i, address in enumerate(TABLE_GLOBALS):
            self.w32(address, self.table[i])
            e.write(self.table[i] + ENABLED, struct.pack(">H", c["enabled"][i] & 0xFFFF))

    def w32(self, address, value):
        self.e.write(address, struct.pack(">I", value & MASK))

    def at(self, index):
        return 0 if index < 0 else self.win[index]

    def index_of(self, address):
        """A window pointer as the index (-1 for NULL, -2 for any other pointer)."""
        if address == 0:
            return -1
        return self.win.index(address) if address in self.win else -2

    def restore(self):
        for address, data in self.saved.items():
            self.e.write(address, data)

    def dump(self):
        """The state in the order of the host's tpw_kb_state."""
        e = self.e
        return [self.index_of(e.u32(KEYBOARD))] + [T.s16(struct.unpack(">H", e.read(t + ENABLED, 2))[0]) for t in self.table]


STUBS = None


def stubs():
    """The one PowerPC fake: the message poster, recording what the host's records. The eight functions of the world are real."""
    st = CL.Stubs()
    st.real = set(ENTRIES) | {ENABLE, GET_ATTR, CAPTURE_KB}
    st.internal(POST, CL.recorder("UI_PostMessage", "Liii", ret=lambda e, L: 0))
    return st


def run_ppc(e, c):
    global STUBS
    if STUBS is None:
        STUBS = stubs()
    f = Ppc(e, c)
    saved = STUBS.activate(e)
    try:
        r3 = T.call_ppc(e, ENTRIES[c["op"]], [f.menu] if c["op"] == IS_SHOWN else [])[0]
        ret = s32(r3) if c["op"] in (MENU_UP, COASTER_UP, IS_SHOWN) else 0
        if c["op"] == MENU_UP:
            ret &= 0xFF   # the listing answers the byte (clrlwi r3, r31, 24)
        return dict(ret=ret, log=e.cl.log, state=f.dump())
    finally:
        CL.Stubs.deactivate(e, saved)
        f.restore()


# ======================================================================= the host side


def run_host(lib, c):
    lib.tpw_kb_window.restype = lib.tpw_kb_table.restype = lib.tpw_kb_menu.restype = lib.tpw_kb_lobby.restype = C.c_void_p
    lib.tpw_kb_window.argtypes = [C.c_int]
    lib.tpw_kb_table.argtypes = [C.c_int]
    lib.tpw_kb_setattr.argtypes = [C.c_int, C.c_uint32]
    lib.tpw_kb_windows.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_kb_menu_lobby.argtypes = [C.c_int, C.c_int32, C.c_int, C.c_int, C.c_int, C.c_int]
    lib.tpw_kb_enabled.argtypes = [C.POINTER(C.c_int16)]
    lib.tpw_kb_state.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_kb_run.argtypes = [C.c_int, C.POINTER(C.c_int32)]
    lib.tpw_kb_reset()
    fixed = {LAB_WIN + i: lib.tpw_kb_window(i) for i in range(WINDOWS)}
    for i, word in enumerate(c["attrs"]):
        lib.tpw_kb_setattr(i, word & MASK)
    lib.tpw_kb_windows((C.c_int32 * 8)(*c["w"]), T.s32(c["flag"] & MASK))
    shown, menu_window = c["menu"] if c["menu"] is not None else (0, -1)
    kind, lobby_window = c["lobby"]
    lib.tpw_kb_menu_lobby(int(c["menu"] is not None), T.s32(shown & MASK), menu_window, kind, lobby_window, c["keyboard"])
    lib.tpw_kb_enabled((C.c_int16 * 3)(*[T.s16(v & 0xFFFF) for v in c["enabled"]]))
    CL.host_begin(lib, fixed)
    out = (C.c_int32 * 1)()
    lib.tpw_kb_run(c["op"], out)
    log = CL.host_end(lib)
    state = (C.c_int32 * 4)()
    lib.tpw_kb_state(state)
    ret = out[0] & 0xFF if c["op"] == MENU_UP else out[0]
    return dict(ret=ret, log=log, state=list(state))


def ppc(e, c):
    return run_ppc(e, c)


def host(lib, c):
    return run_host(lib, c)


# ======================================================================= the cases

ATTR_WORDS = [0, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF, 0x80000001, 0x80000000]


def unsafe(c):
    """The one case the host cannot run: a game window, no bankrupt window, no first-person window, no menu up."""
    if c["op"] != DECIDE:
        return False
    menu_up = c["menu"] is not None and c["menu"][0] != 0
    w = c["w"]
    return (not menu_up) and w[GAME] >= 0 and w[BANKRUPT] < 0 and w[FIRST_PERSON] < 0


def attrs_for(**bits):
    """Attribute words: each window index's word given by name; the rest 0."""
    out = [0] * WINDOWS
    for index, word in bits.items():
        out[int(index[1:])] = word
    return out


def decide_edges():
    # no game window: first-person x online panel x online window x attribute bit 0 x the lobby; the other globals set and ignored
    for fp in (-1, 0):
        for panel in (-1, 2):
            for window in (-1, 4):
                for attr in ((0, 0, 0), (1, 1, 1), (1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 0), (0, 1, 1), (1, 0, 1)):
                    for lobby in ((0, 0), (1, 0), (2, 6)):
                        attrs = [0] * WINDOWS
                        attrs[0], attrs[2], attrs[4] = attr
                        windows = {}
                        if fp >= 0:
                            windows[FIRST_PERSON] = 0
                        if panel >= 0:
                            windows[ONLINE_PANEL] = 2
                        if window >= 0:
                            windows[ONLINE_WINDOW] = 4
                        windows.update({BUMHOLE: 7, BANKRUPT: 8, PANEL: 9})
                        yield case(windows=windows, lobby=lobby, attrs=attrs, enabled=(0, 0, 0))
    # the lobby alone, with the keyboard elsewhere or already there
    for kind in (0, 1, 2):
        for keyboard in (-1, 6, 7):
            yield case(lobby=(kind, 6), keyboard=keyboard)
    # a game window: bankrupt x first-person x game window attribute x panel x coaster flag x bumhole
    for bankrupt in (-1, 8):
        for fp in (-1, 1):
            for fp_attr in (0, 1, 2, 3):
                for game_attr in (0, 1, 2, 3):
                    for panel in (-1, 9):
                        for flag in (0, 1, -1, 5):
                            for bumhole in (-1, 7):
                                for bumhole_attr in (0, 1, 2):
                                    windows = {GAME: 0, BANKRUPT: bankrupt, FIRST_PERSON: fp, PANEL: panel, BUMHOLE: bumhole,
                                               COASTER_PANEL: 5, ONLINE_PANEL: 2}
                                    windows = {k: v for k, v in windows.items() if v >= 0}
                                    attrs = [0] * WINDOWS
                                    attrs[0], attrs[1], attrs[7] = game_attr, fp_attr, bumhole_attr
                                    c = case(windows=windows, flag=flag, attrs=attrs, enabled=(7, -1, 0x1234), keyboard=3,
                                             lobby=(2, 6))
                                    if not unsafe(c):
                                        yield c
    # the keyboard already on the window that is chosen, and on another
    for keyboard in (-1, 0, 1, 5, 7, 8, 9, 4):
        for flag in (0, 1):
            yield case(windows={GAME: 0, FIRST_PERSON: 1, COASTER_PANEL: 5}, flag=flag, keyboard=keyboard,
                       attrs=attrs_for(w0=1), enabled=(0, 0, 0))
            yield case(windows={GAME: 0, FIRST_PERSON: 1, COASTER_PANEL: 5, PANEL: 9}, flag=flag, keyboard=keyboard,
                       attrs=attrs_for(w0=1), enabled=(0, 0, 0))
    # the tables' words before the call: Enable writes 1 to the enabled word of a table, whatever it held
    for enabled in ((0, 0, 0), (1, 1, 1), (5, 6, 7), (-1, -2, -3), (0x7FFF, -0x8000, 0x100), (2, 0, 0xFF)):
        yield case(windows={GAME: 0, FIRST_PERSON: 1}, attrs=attrs_for(w0=1, w1=0), enabled=enabled)
        yield case(windows={GAME: 0, FIRST_PERSON: 1}, attrs=attrs_for(w0=0, w1=1), enabled=enabled)
        yield case(windows={GAME: 0, FIRST_PERSON: 1, BUMHOLE: 2}, attrs=attrs_for(w2=1), enabled=enabled)
        yield case(windows={GAME: 0, FIRST_PERSON: 1, COASTER_PANEL: 5}, flag=1, attrs=attrs_for(w0=1), enabled=enabled)
        yield case(windows={GAME: 0, FIRST_PERSON: 1}, flag=0, attrs=attrs_for(w0=1), enabled=enabled)
        yield case(windows={FIRST_PERSON: 1}, attrs=attrs_for(w1=1), enabled=enabled)
    # the main menu: absent, shown 0 / 1 / 7 / -1 / 0x10000 / 0x80000000, its window present or NULL, over a busy world
    for menu in (None, (0, 3), (1, 3), (7, 3), (-1, 3), (0x10000, 3), (0x80000000, 3), (1, -1), (0, -1)):
        for keyboard in (-1, 3, 6):
            for game in (-1, 0):
                windows = {FIRST_PERSON: 1, ONLINE_PANEL: 2, BANKRUPT: 8, PANEL: 9, BUMHOLE: 7}
                if game >= 0:
                    windows[GAME] = 0
                yield case(windows=windows, menu=menu, keyboard=keyboard, lobby=(2, 6), attrs=[1] * WINDOWS, enabled=(0, 0, 0))
    # two globals on one window
    yield case(windows={GAME: 4, FIRST_PERSON: 4, BANKRUPT: 4}, attrs=attrs_for(w4=1), keyboard=4)
    yield case(windows={GAME: 4, FIRST_PERSON: 4, PANEL: 4, COASTER_PANEL: 4}, attrs=attrs_for(w4=1), keyboard=4)
    yield case(windows={ONLINE_PANEL: 4, ONLINE_WINDOW: 4, FIRST_PERSON: 4}, attrs=attrs_for(w4=1), keyboard=4)
    yield case(windows={GAME: 4, FIRST_PERSON: 4, BUMHOLE: 4}, attrs=attrs_for(w4=2), keyboard=4)
    yield case(windows={ONLINE_PANEL: 4}, lobby=(2, 4), attrs=attrs_for(w4=0), keyboard=4)


def decide_rnd(r):
    while True:
        windows = {}
        for slot in range(8):
            if r.random() < 0.6:
                windows[slot] = r.randrange(WINDOWS)
        menu = None
        if r.random() < 0.25:
            menu = (r.choice([0, 1, 7, -1, r.randrange(-2 ** 31, 2 ** 31)]), r.randrange(-1, WINDOWS))
        attrs = [r.choice(ATTR_WORDS + [r.getrandbits(32)]) for _ in range(WINDOWS)]
        c = case(windows=windows, menu=menu, flag=r.choice([0, 1, -1, 5, r.randrange(-2 ** 31, 2 ** 31)]),
                 lobby=(r.randrange(3), r.randrange(WINDOWS)), keyboard=r.randrange(-1, WINDOWS), attrs=attrs,
                 enabled=tuple(r.choice([0, 1, -1, r.randrange(-2 ** 15, 2 ** 15)]) for _ in range(3)))
        if not unsafe(c):
            return c


# ---- MainMenuUp, UIMenu::IsShown


def menu_edges():
    for shown in (0, 1, 2, 7, -1, 0xFF, 0x100, 0x10000, 0x7FFFFFFF, 0x80000000):
        for present in (True, False):
            yield case(op=MENU_UP, menu=(shown, 3) if present else None)


def menu_rnd(r):
    return case(op=MENU_UP, menu=(r.choice([0, 1, 7, r.randrange(-2 ** 31, 2 ** 31)]), r.randrange(-1, WINDOWS)) if r.random() < 0.7 else None)


def shown_edges():
    for shown in (0, 1, 2, 7, -1, 0xFF, 0x100, 0x10000, 0x7FFFFFFF, 0x80000000, 0x12345678):
        yield case(op=IS_SHOWN, menu=(shown, 3))


def shown_rnd(r):
    return case(op=IS_SHOWN, menu=(r.randrange(-2 ** 31, 2 ** 31), r.randrange(-1, WINDOWS)))


# ---- CoasterEditPanelUp, CoasterEditPanelGetKb


def flag_edges():
    for flag in (0, 1, 2, -1, 0xFF, 0x100, 0x10000, 0x7FFFFFFF, -0x80000000, 0x12345678):
        yield case(op=COASTER_UP, flag=flag)


def flag_rnd(r):
    return case(op=COASTER_UP, flag=r.choice([0, 1, r.randrange(-2 ** 31, 2 ** 31)]))


def getkb_edges():
    # the coaster panel present or NULL, the keyboard on it, on another window, or nowhere
    for panel in (-1, 5, 6):
        for keyboard in (-1, 5, 6, 0):
            yield case(op=COASTER_GETKB, windows={COASTER_PANEL: panel} if panel >= 0 else {}, keyboard=keyboard)


def getkb_rnd(r):
    return case(op=COASTER_GETKB, windows={COASTER_PANEL: r.randrange(WINDOWS)} if r.random() < 0.85 else {},
                keyboard=r.randrange(-1, WINDOWS), attrs=[r.getrandbits(32) for _ in range(WINDOWS)])


def setup(runner):
    T.install_std_stubs(runner.emu)


TESTS = [FnTest("UIMain_DecideKbCapture", ppc, host, edge=decide_edges, rnd=decide_rnd),
         FnTest("MainMenuUp", ppc, host, edge=menu_edges, rnd=menu_rnd),
         FnTest("UIMenu::IsShown", ppc, host, edge=shown_edges, rnd=shown_rnd),
         FnTest("CoasterEditPanelUp", ppc, host, edge=flag_edges, rnd=flag_rnd),
         FnTest("CoasterEditPanelGetKb", ppc, host, edge=getkb_edges, rnd=getkb_rnd)]
