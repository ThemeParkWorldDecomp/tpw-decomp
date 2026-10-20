"""Unit w47: the "Create new player" bubble's message function and OK, the quit and delete-player confirmations, the dialog box callback,
and the small functions they use, against libtpw_unit_PlayerDialogs.dylib (tests/unit_PlayerDialogs.cpp).

Nine functions run for real on both sides: CreatePlayerMsg, NewNameOK, DefaultDialogCallback, DialogBoxCallback, UIMain_EnableAllKeyTables,
UI_UnPauseTimerSystem, GameConfirmQuit_OK, GameConfirmDelPlayer_OK and InterfaceCheckButtons::GetCurrent (and __ptr_glue, through which
DialogBoxCallback calls the buttons' callbacks). Everything they call is a fake that records the same event on both sides: GetWindow (the
queue "GetWindow:<id>" names the window it answers), Show, getExtraParam (over a per-window table), GetParent, the default handler, the
message senders, the key tables, the clock, the game's Unpause, the panel's removal and name filling, the user information (constructor,
create, login, delete), the lobby's SendQuit, the keyboard decision and the two dialog callbacks (whose answers are queues). The string
class's constructor and destructor and operator new are counted instead of recorded (the host's are the real class).

After every case the answer, the whole log, the state (the dialog global, the first player flag, the user information's label, the key
byte, the pause word, the game's word, the four timers' start ticks, the name buffer) and the heap traffic are compared.

Nine tests, one per function. The message functions are swept over the messages around every comparison of their ladders, the button ids
around theirs and the argument b; NewNameOK over names of 0, 1 and 17 characters, spaces, and the check buttons' current id; the dialog
box callback over every combination of callbacks, their answers, the key tables' state, the pause conditions and the timers.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_PlayerDialogs"

MASK = 0xFFFFFFFF

# ---- the unit's functions, in the order of the host driver's ops
FUNCTIONS = [0x101AF3FC, 0x101AF264, 0x101F1448, 0x101F1520, 0x10182304, 0x101D8BA0, 0x101AE1B0, 0x101ADDDC, 0x101CC108]
CREATE_MSG, NAME_OK, DEFAULT_CB, DIALOG_CB, ENABLE_KEYS, UNPAUSE, QUIT_OK, DELETE_OK, GET_CURRENT = range(9)
PTR_GLUE = 0x1023BAA8
# the fakes (in-binary functions)
GET_WINDOW, SHOW, GET_EXTRA, GET_PARENT, DEFAULT_HANDLER = 0x101DBE40, 0x101DC1EC, 0x101DD030, 0x101DBAFC, 0x101DE318
SEND, POST, DECIDE_KB = 0x101C9518, 0x101C9400, 0x1018116C
ENABLE_TABLES, DISABLE_KEYS = 0x10151010, 0x10182364
TICKS, UNPAUSE_GAME = 0x101CAC3C, 0x101499D0
REMOVE_PANEL, FILL_NAMES = 0x101AF02C, 0x101AE39C
USER_CTOR, CREATE_USER, LOGIN, DELETE_USER, SEND_QUIT = 0x1017DE54, 0x1017EE38, 0x1017F330, 0x1017ED10, 0x100A643C
# the dialog's two buttons' callbacks: two functions of the program that nothing here calls, standing in (their TVectors are made per case)
CALLBACK_FIRST, CALLBACK_SECOND = 0x101F1800, 0x101F12C8

# ---- the original's data
G = dict(dialog=0x10394A90, text=0x10394A94, first=0x10394AB8, user=0x10380B34, game=0x1037F134, lobby=0x102E4888, paused=0x10395C4C,
         timers=0x10395C58, key=0x1038CC54)
GAME_WORD = G["game"] + 0x3C
TEXT_UNITS = 18

# ---- the world: labels
WINDOWS, EXTRAS, TIMERS = 6, 6, 4
LAB_WIN, LAB_USER, LAB_LOBBY, LAB_GAME = 10, 20, 21, 22
# the window ids the functions ask for
PICTURE, CHOICES, CHOICE_EASY, CHOICE_HARD = 0x70B, 0x70C, 0x70D, 0x70E
BUTTON_IDS = [0x9873C7, 0x9873C8, 0x9873C9, 0x9873CA, 0x9873CB]
MESSAGES = [0, 4, 0x13, 0x14, 0x15, 0xFF, 0x100, 0x101, 0x102, 0x801, 0x802, 0x803, 0x804, 0x805]
ARGUMENTS = [-3, -2, -1, 0, CHOICES, CHOICE_EASY, CHOICE_HARD] + BUTTON_IDS
SECOND_ARGUMENTS = [0, 1, 2, -1]
NAMES = ["Anna", "", "A", " ", "     ", "Anna  ", "A B", "  A", " A ", "x" * 17, "x" * 16 + " ", " " * 17, "Zoë", "ab" * 8 + " ",
         "Theodora Pinkus", "QЖ"]


def s32(v):
    return T.s32(v & MASK)


def blank_window():
    """A window with no parent, no extra parameters (NULL) and no current button."""
    return dict(current=-1, parent=-1, extra=[(0, 0)] * EXTRAS)


def text_units(text):
    """A name as the 18 characters of the buffer (a NUL at the end is kept)."""
    units = [ord(c) for c in text][:TEXT_UNITS - 1]
    return units + [0] * (TEXT_UNITS - len(units))


def base_queues():
    """The answers every case has: GetWindow finds the picture (window 1) and the check buttons (window 2) under their ids."""
    return {"GetWindow:%d" % PICTURE: [2], "GetWindow:%d" % CHOICES: [3]}


def case(op, args=(0, 0, 0, 0), wins=None, name="Anna", dialog=0, user=0, lobby=0, word=0, paused=0, key=0, first=0, starts=(0, 0, 0, 0),
         timers=0, q=None):
    """One case: the op (see FUNCTIONS), its four arguments (a window index or a number), the windows ({index: changes}), the name buffer,
    the globals, the timers (how many are in the list, and the four start ticks) and the queues."""
    windows = [blank_window() for _ in range(WINDOWS)]
    for index, changes in (wins or {}).items():
        windows[index].update(changes)
    queues = base_queues()
    queues.update(q or {})
    return dict(op=op, args=list(args), wins=windows, name=name if isinstance(name, list) else text_units(name),
                g=dict(dialog=dialog, user=user, lobby=lobby, word=word, paused=paused, key=key, first=first),
                starts=list(starts), timers=timers, q=queues)


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the stubs' state and the dump."""

    def __init__(self, e, case):
        self.e = e
        e.pd = self
        self.case = case
        self.win = [e.alloc(0x13C, bytes(0x13C)) for _ in range(WINDOWS)]
        self.user = e.alloc(0xD4, bytes(0xD4))
        self.lobby = e.alloc(0x40, bytes(0x40))
        self.timer = [e.alloc(0x20, bytes(0x20)) for _ in range(TIMERS)]
        self.callback = [e.alloc(8, struct.pack(">II", CALLBACK_FIRST, 0)), e.alloc(8, struct.pack(">II", CALLBACK_SECOND, 0))]
        self.heap = [0, 0, 0, 0]   # blocks handled by operator new, new[], delete, delete[]
        labels = {self.win[i]: LAB_WIN + i for i in range(WINDOWS)}
        labels.update({self.user: LAB_USER, self.lobby: LAB_LOBBY, G["game"]: LAB_GAME})
        self.L = e.cl = CL.PpcLog(labels, case["q"])
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in [(G["dialog"], 4), (G["text"], 2 * TEXT_UNITS), (G["first"], 4),
                                                              (G["user"], 4), (GAME_WORD, 4), (G["lobby"], 4), (G["paused"], 4),
                                                              (G["timers"], 4), (G["key"], 1)]}
        for i, w in enumerate(case["wins"]):
            self.w32(self.win[i] + 0x138, w["current"])
        g = case["g"]
        self.w32(G["dialog"], self.at(g["dialog"]))
        e.write(G["text"], struct.pack(">%dH" % TEXT_UNITS, *case["name"]))
        self.w32(G["first"], g["first"])
        self.w32(G["user"], self.user if g["user"] else 0)
        self.w32(GAME_WORD, g["word"])
        self.w32(G["lobby"], self.lobby if g["lobby"] else 0)
        self.w32(G["paused"], g["paused"])
        e.write(G["key"], bytes([g["key"] & 0xFF]))
        for i in range(TIMERS):
            self.w32(self.timer[i] + 4, case["starts"][i])
            self.w32(self.timer[i] + 0x18, self.timer[i + 1] if i + 1 < case["timers"] else 0)
        self.w32(G["timers"], self.timer[0] if case["timers"] > 0 else 0)

    def w32(self, address, value):
        self.e.write(address, struct.pack(">I", value & MASK))

    def at(self, index):
        return 0 if index < 0 else self.win[index]

    def index_of(self, address):
        """A window pointer as the index (-1 for NULL, -2 for any other pointer)."""
        if address == 0:
            return -1
        return self.win.index(address) if address in self.win else -2

    def extra_of(self, window, slot):
        """The value of an extra parameter: a number, or the TVector of one of the two callbacks."""
        kind, value = self.case["wins"][self.win.index(window)]["extra"][slot]
        return self.callback[kind - 1] if kind in (1, 2) else value & MASK

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    def dump(self):
        """The state in the order of the host's tpw_pd_state."""
        e = self.e
        out = [self.index_of(e.u32(G["dialog"])), s32(e.u32(G["first"])), self.L.lab(e.u32(G["user"])), e.read(G["key"], 1)[0],
               s32(e.u32(G["paused"])), s32(e.u32(GAME_WORD))]
        out.extend(s32(e.u32(t + 4)) for t in self.timer)
        out.extend(struct.unpack(">%dH" % TEXT_UNITS, e.read(G["text"], 2 * TEXT_UNITS)))
        return out


def wide_length(e, address):
    count = 0
    while struct.unpack(">H", e.read(address + 2 * count, 2))[0] != 0:
        count += 1
    return count


def stubs():
    """The PowerPC fakes: every callee of the nine functions, each recording what the host's fake records."""
    st = CL.Stubs()
    R = CL.recorder
    st.real = set(FUNCTIONS) | {PTR_GLUE}

    def get_window(e):
        L = e.cl
        this, ident = e.r(3), s32(e.r(4))
        L.rec("GetWindow", [L.lab(this), ident])
        n = L.ret("GetWindow:%d" % ident)
        e.set_r(3, e.pd.win[n - 1] if 1 <= n <= WINDOWS else 0)

    def get_extra(e):
        L = e.cl
        this, index = e.r(3), T.s16(e.r(4))
        L.rec("getExtraParam", [L.lab(this), index])
        e.set_r(3, e.pd.extra_of(this, index) if this in e.pd.win and 0 <= index < EXTRAS else 0)

    def get_parent(e):
        L = e.cl
        this = e.r(3)
        L.rec("GetParent", [L.lab(this)])
        parent = e.pd.case["wins"][e.pd.win.index(this)]["parent"] if this in e.pd.win else -1
        e.set_r(3, e.pd.at(parent))

    def callback(name):
        """A dialog callback's fake: the argument's 32 bits recorded, the queue's answer."""
        def stub(e):
            e.cl.rec(name, [s32(e.r(3))])
            e.set_r(3, T.u32(e.cl.ret(name)))
        return stub

    def disable_keys(e):
        e.cl.rec("UIMain_DisableAllKeyTables")
        e.write(G["key"], bytes([1]))

    def user_ctor(e):
        e.cl.rec("UserInfo.ctor", [e.cl.newlab(e.r(3))])

    def create_user(e):
        L = e.cl
        L.rec("UserInfo.CreateNew", [L.lab(e.r(3)), s32(e.r(4)), s32(e.r(6))], [CL.wds_text(e, e.r(5))])
        if L.ret("ClearUser") != 0:
            e.pd.w32(G["user"], 0)

    def op_new(e):
        size = e.r(3)
        e.pd.heap[0] += 1
        e.set_r(3, e.alloc(max(size, 4), b"\xa5" * max(size, 4)))

    def string_ctor(e):
        e.pd.heap[1] += 1
        CL.wds_set(e, e.r(3), CL.wide_text(e, e.r(4)))

    def string_dtor(e):
        e.pd.heap[3] += 1

    for addr, fn in [(GET_WINDOW, get_window), (SHOW, R("Show", "Lh", ret=lambda e, L: 1)), (GET_EXTRA, get_extra),
                     (GET_PARENT, get_parent), (DEFAULT_HANDLER, R("CallDefaultWindowHandler", "Liii", ret="i")),
                     (SEND, R("UI_SendMessage", "Liii", ret="i")), (POST, R("UI_PostMessage", "Liii", ret="i")),
                     (DECIDE_KB, R("UIMain_DecideKbCapture")), (ENABLE_TABLES, R("EnableAllKeytables")), (DISABLE_KEYS, disable_keys),
                     (TICKS, R("IveGotTicks", ret="i")), (UNPAUSE_GAME, R("CGame.Unpause", "L")),
                     (REMOVE_PANEL, R("RemoveLoginPanel", "i")), (FILL_NAMES, R("FillLoginNames")), (USER_CTOR, user_ctor),
                     (CREATE_USER, create_user), (LOGIN, R("UserInfo.GameLogin", "Li")), (DELETE_USER, R("UserInfo.Delete", "Li")),
                     (SEND_QUIT, R("SendQuit", "L")), (CALLBACK_FIRST, callback("CbFirst")), (CALLBACK_SECOND, callback("CbSecond"))]:
        st.internal(addr, fn)
    st.imp("__nw__FUl", op_new)
    st.imp("wcslen", lambda e: e.set_r(3, wide_length(e, e.r(3))))
    st.imp("__ct__26TbDynamicStringTemplate<w>FPCw", string_ctor)
    st.imp("__dt__26TbDynamicStringTemplate<w>Fv", string_dtor)
    return st


STUBS = None


def run_ppc(e, case):
    """Builds the case's world, calls the function, and answers the comparison record with the world restored."""
    global STUBS
    if STUBS is None:
        STUBS = stubs()
    f = Ppc(e, case)
    saved = STUBS.activate(e)
    try:
        op, given = case["op"], case["args"]
        if op in (CREATE_MSG, DEFAULT_CB, DIALOG_CB):
            args = [f.at(given[0])] + [v & MASK for v in given[1:]]
        elif op == GET_CURRENT:
            args = [f.at(given[0])]
        else:
            args = [given[1] & MASK]   # NewNameOK, the confirmations: the slot or data pointer is the host driver's p1
        r3 = T.call_ppc(e, FUNCTIONS[case["op"]], args)[0]
        ret = s32(r3) if case["op"] not in (ENABLE_KEYS, UNPAUSE) else 0
        return dict(ret=ret, log=e.cl.log, state=f.dump(), heap=list(f.heap))
    finally:
        CL.Stubs.deactivate(e, saved)
        f.restore()


# ======================================================================= the host side


def load_host(lib):
    lib.tpw_pd_window.restype = lib.tpw_pd_user.restype = lib.tpw_pd_lobby.restype = lib.tpw_pd_game.restype = C.c_void_p
    lib.tpw_pd_window.argtypes = [C.c_int]
    lib.tpw_pd_setwin.argtypes = [C.c_int, C.c_int32, C.c_int]
    lib.tpw_pd_setextra.argtypes = [C.c_int, C.c_int, C.c_int, C.c_int32]
    lib.tpw_pd_setname.argtypes = [C.POINTER(C.c_uint16)]
    lib.tpw_pd_globals.argtypes = [C.c_int, C.c_int, C.c_int, C.c_int32, C.c_int32, C.c_int, C.c_int32]
    lib.tpw_pd_timers.argtypes = [C.c_int, C.POINTER(C.c_uint32)]
    lib.tpw_pd_state.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_pd_run.argtypes = [C.c_int, C.c_int, C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_int32)]


def run_host(lib, case):
    """The same world on the host, one call of the function, and the comparison record."""
    load_host(lib)
    lib.tpw_pd_reset()
    fixed = {LAB_WIN + i: lib.tpw_pd_window(i) for i in range(WINDOWS)}
    fixed.update({LAB_USER: lib.tpw_pd_user(), LAB_LOBBY: lib.tpw_pd_lobby(), LAB_GAME: lib.tpw_pd_game()})
    for i, w in enumerate(case["wins"]):
        lib.tpw_pd_setwin(i, w["current"], w["parent"])
        for slot, (kind, value) in enumerate(w["extra"]):
            lib.tpw_pd_setextra(i, slot, kind, T.s32(value & MASK))
    lib.tpw_pd_setname((C.c_uint16 * TEXT_UNITS)(*case["name"]))
    g = case["g"]
    lib.tpw_pd_globals(g["dialog"], g["user"], g["lobby"], g["word"], g["paused"], g["key"], g["first"])
    lib.tpw_pd_timers(case["timers"], (C.c_uint32 * TIMERS)(*[v & MASK for v in case["starts"]]))
    CL.host_begin(lib, fixed, case["q"])
    out = (C.c_int32 * 4)()
    a = case["args"]
    lib.tpw_pd_run(case["op"], T.s32(a[0] & MASK), T.s32(a[1] & MASK), T.s32(a[2] & MASK), T.s32(a[3] & MASK), out)
    log = CL.host_end(lib)
    state = (C.c_int32 * (6 + TIMERS + TEXT_UNITS))()
    lib.tpw_pd_state(state)
    heap = list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))
    return dict(ret=out[0], log=log, state=list(state), heap=heap)


def ppc(e, c):
    return run_ppc(e, c)


def host(lib, c):
    return run_host(lib, c)


# ======================================================================= the cases


def dialog_window(first=0, second=0, third=0, data=0, tables=-1, paused=0, parent=0):
    """A dialog box: its extra parameters (the three callbacks, 0 NULL and 1 and 2 the fakes, their argument, the key table byte, the pause)
    and its parent."""
    return dict(extra=[(first, 0), (second, 0), (third, 0), (0, data), (0, tables), (0, paused)], parent=parent)


def answers(first=0, second=0, default=0, send=0, post=0):
    return {"CbFirst": [first], "CbSecond": [second], "CallDefaultWindowHandler": [default], "UI_SendMessage": [send],
            "UI_PostMessage": [post]}


# ---- CreatePlayerMsg


def create_edges():
    # every message x argument x b, with the bubble (window 0, the dialog global too) and a good name
    for message in MESSAGES:
        for a in ARGUMENTS:
            for b in SECOND_ARGUMENTS:
                yield case(CREATE_MSG, (0, message, a, b), wins={0: dict(extra=[(0, 5)] + [(0, 0)] * 5, parent=4)},
                           q={"CallDefaultWindowHandler": [0x1234 + message]})
    # triage: messages around the ends of the ladder 0x102..0x805 and far from it
    for message in (0x103, 0x400, 0x7FE, 0x7FF, 0x800, 0x806, 0x807, 0xFFF, 0xFFFF, 0x10000, -1, -0x80000000, 0x7FFFFFFF):
        for a in (-3, -2, -1, 0, CHOICES, CHOICE_EASY, CHOICE_HARD):
            for b in (0, 1):
                yield case(CREATE_MSG, (0, message, a, b), wins={0: dict(extra=[(0, 5)] + [(0, 0)] * 5, parent=4)},
                           q={"CallDefaultWindowHandler": [0x1234 + (message & 0xFFFF)]})
    # OK in every state of the name, the check buttons, the user information and the slot
    for name in NAMES:
        for current in (CHOICE_EASY, CHOICE_HARD, -1):
            for user in (0, 1):
                yield case(CREATE_MSG, (0, 0x100, -1, 0), name=name, user=user, wins={2: dict(current=current),
                           0: dict(extra=[(0, 2)] + [(0, 0)] * 5)}, first=7)
    # the user information destroyed by the creation, then made again
    yield case(CREATE_MSG, (0, 0x100, -1, 0), user=1, q={"ClearUser": [1]})
    yield case(CREATE_MSG, (0, 0x100, -1, 0), user=0, q={"ClearUser": [1]})
    # the picture shown or hidden by the choice
    for a in (CHOICE_EASY, CHOICE_HARD, 0, -1):
        for b in (1, 0, 2, -1):
            yield case(CREATE_MSG, (0, 0x101, a, b))
    # the default handler's answers go through
    for answer in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        for message in (0x14, 0x101, 0x803, 0x100):
            yield case(CREATE_MSG, (0, message, 0, 0), wins={0: dict(parent=1)}, q={"CallDefaultWindowHandler": [answer]})
    yield case(CREATE_MSG, (0, 0x14, 0, 0), dialog=-1)
    yield case(CREATE_MSG, (0, 0x14, 0, 0), dialog=2)


def create_rnd(r):
    message = r.choice(MESSAGES + [r.randrange(-2**31, 2**31), 0x80000000 - 1, -0x80000000, r.randrange(0, 0x900)])
    a = r.choice(ARGUMENTS + [r.randrange(-2**31, 2**31)])
    b = r.choice(SECOND_ARGUMENTS + [r.randrange(-2**31, 2**31)])
    return case(CREATE_MSG, (0, message, a, b),
                wins={0: dict(extra=[(0, r.choice([0, 1, 2, 3, -1, r.randrange(-2**31, 2**31)]))] + [(0, 0)] * 5,
                              parent=r.randrange(-1, WINDOWS)),
                      2: dict(current=r.choice([-1, CHOICE_EASY, CHOICE_HARD, 0, r.randrange(-2**31, 2**31)]))},
                name=r.choice(NAMES), user=r.randrange(2), first=r.choice([0, 1, 5]),
                q=dict({"ClearUser": [r.choice([0, 0, 1])], "CallDefaultWindowHandler": [r.randrange(-2**31, 2**31)],
                        "UI_SendMessage": [r.randrange(-2**31, 2**31)]}))


# ---- NewNameOK


def name_edges():
    for name in NAMES:
        for current in (CHOICE_EASY, CHOICE_HARD, -1, 0, 0x7FFFFFFF):
            for user in (0, 1):
                yield case(NAME_OK, (0, 5, 0, 0), name=name, user=user, wins={2: dict(current=current)})
    for slot in (0, 1, 2, 3, 4, -1, 0x7FFFFFFF, -0x80000000, 0x12345678):
        yield case(NAME_OK, (0, slot, 0, 0), name="Bob", user=1)
    yield case(NAME_OK, (0, 1, 0, 0), name="Bob", user=1, q={"ClearUser": [1]})
    yield case(NAME_OK, (0, 1, 0, 0), name="Bob", user=0, q={"ClearUser": [1]})
    # the text after the first NUL (and a buffer with garbage after the name)
    yield case(NAME_OK, (0, 1, 0, 0), name=[ord(c) for c in "Joe  "] + [0] + [ord("z")] * 12)
    yield case(NAME_OK, (0, 1, 0, 0), name=[0x20, 0x20, 0] + [0x41] * 15)
    yield case(NAME_OK, (0, 1, 0, 0), name=[0x20] * 17 + [0])
    yield case(NAME_OK, (0, 1, 0, 0), name=[0x41] * 17 + [0])
    # the characters next to the space (below and above) at the end of the name are kept; a space after them is cut
    for edge in (0x01, 0x09, 0x1F, 0x21, 0x7F, 0xA0, 0x120, 0x10020):
        yield case(NAME_OK, (0, 1, 0, 0), name=[0x41, edge & 0xFFFF, 0x20, 0x20] + [0] * 14)
        yield case(NAME_OK, (0, 1, 0, 0), name=[edge & 0xFFFF] + [0] * 17)
    yield case(NAME_OK, (0, 1, 0, 0), name=[0x3000, 0x20, 0x2000, 0x20] + [0] * 14)    # characters that look like spaces are not spaces
    yield case(NAME_OK, (0, 1, 0, 0), name=[0x20, 0x41, 0x20, 0x20, 0x42, 0x20, 0x20] + [0] * 11)


def name_rnd(r):
    chars = [0x20, 0x20, 0x41, 0x7A, 0xE9, 0x416, 0x3000]
    length = r.randrange(0, TEXT_UNITS)
    name = [r.choice(chars) for _ in range(length)] + [0] * (TEXT_UNITS - length)
    return case(NAME_OK, (0, r.choice([0, 1, 2, 3, r.randrange(-2**31, 2**31)]), 0, 0), name=name, user=r.randrange(2),
                wins={2: dict(current=r.choice([-1, CHOICE_EASY, CHOICE_HARD, r.randrange(-2**31, 2**31)]))},
                q={"ClearUser": [r.choice([0, 0, 1])]})


# ---- DefaultDialogCallback


def default_edges():
    for message in MESSAGES + [-1, 0x7FFFFFFF, -0x80000000]:
        for a in (0, -1, 0x9873C8):
            for b in SECOND_ARGUMENTS:
                yield case(DEFAULT_CB, (3, message, a, b), wins={3: dict(parent=1)}, q={"CallDefaultWindowHandler": [0x4321]})
    for parent in range(-1, WINDOWS):
        yield case(DEFAULT_CB, (3, 0x14, 0, 0), wins={3: dict(parent=parent)})
    for answer in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        yield case(DEFAULT_CB, (0, 0x14, 7, 8), q={"CallDefaultWindowHandler": [answer]})
        yield case(DEFAULT_CB, (0, 0x15, 7, 8), q={"CallDefaultWindowHandler": [answer]})


def default_rnd(r):
    return case(DEFAULT_CB, (r.randrange(WINDOWS), r.choice(MESSAGES + [r.randrange(-2**31, 2**31)]),
                             r.choice([0, -1, r.randrange(-2**31, 2**31)]), r.choice([0, 1, r.randrange(-2**31, 2**31)])),
                wins={i: dict(parent=r.randrange(-1, WINDOWS)) for i in range(WINDOWS)},
                q={"CallDefaultWindowHandler": [r.randrange(-2**31, 2**31)], "UI_PostMessage": [r.randrange(-2**31, 2**31)]})


# ---- DialogBoxCallback


def dialog_edges():
    data = [0, 1, -1, 0x1234, 0x7FFFFFFF]
    # a button message: every button id x every combination of callbacks x their answers
    for a in ARGUMENTS:
        for first in (0, 1, 2):
            for second in (0, 1, 2):
                for third in (0, 1, 2):
                    for answer in (0, 1):
                        yield case(DIALOG_CB, (2, 0x100, a, 0), wins={2: dialog_window(first, second, third, data=data[first + second])},
                                   q=answers(answer, 1 - answer, default=0x55))
    # triage: button ids next to the three buttons and at the extremes (compared signed), and the key table word 0x80000000
    for a in (0x9873C6, 0x9873CC, 0, 1, -1, -0x80000000, 0x7FFFFFFF, 0x98730C8, 0x1873C8):
        for answer in (0, 1):
            yield case(DIALOG_CB, (2, 0x100, a, 0), wins={2: dialog_window(1, 2, 1, data=5)}, q=answers(answer, 1 - answer, default=0x55))
    yield case(DIALOG_CB, (2, 0x14, 0, 0), wins={2: dialog_window(tables=-0x80000000, paused=1)}, word=0, lobby=0, paused=1, timers=2, key=3,
               starts=(11, 22, 33, 44), q={"IveGotTicks": [100, 200, 300, 400]})
    # a few answers a callback can give (any nonzero closes)
    for answer in (0, 1, -1, 2, 0x80000000, 0x7FFFFFFF):
        for a in (0x9873C8, 0x9873C9, 0x9873CA):
            yield case(DIALOG_CB, (2, 0x100, a, 0), wins={2: dialog_window(1, 1, 1, data=9)}, q=answers(answer, answer, default=5, send=6))
    # every message x button id x b with a fixed dialog
    for message in MESSAGES:
        for a in ARGUMENTS:
            for b in SECOND_ARGUMENTS:
                yield case(DIALOG_CB, (2, message, a, b), wins={2: dialog_window(1, 2, 0, data=0x77)},
                           q=answers(1, 1, default=0x31))
    # the destroy message: the key table byte x the pause flag x the game's word x the lobby x the timers' pause x the timers in the list
    for tables in (-1, 0, 1, 2, -2, 0x7FFFFFFF):
        for paused_flag in (0, 1, -1, 7):
            for word in (0, 1, 2, -1):
                for lobby in (0, 1):
                    for paused in (0, 1):
                        for timers in (0, 2):
                            yield case(DIALOG_CB, (2, 0x14, 0, 0), wins={2: dialog_window(tables=tables, paused=paused_flag)}, word=word, lobby=lobby, paused=paused, timers=timers, key=3,
                                       starts=(11, 22, 33, 44), q={"IveGotTicks": [100, 200, 300, 400]})
    for timers in range(5):
        yield case(DIALOG_CB, (2, 0x14, 0, 0), wins={2: dialog_window(paused=1)}, paused=1, timers=min(timers, 4),
                   starts=(11, 22, 33, 44), q={"IveGotTicks": [100, 200, 300, 400]})


def dialog_rnd(r):
    wins = {2: dialog_window(r.choice([0, 1, 2]), r.choice([0, 1, 2]), r.choice([0, 1, 2]), data=r.choice([0, 1, -1, r.randrange(-2**31, 2**31)]),
                             tables=r.choice([-1, 0, 1, 2, r.randrange(-2**31, 2**31)]),
                             paused=r.choice([0, 1, r.randrange(-2**31, 2**31)]), parent=r.randrange(-1, WINDOWS))}
    return case(DIALOG_CB, (2, r.choice(MESSAGES + [r.randrange(-2**31, 2**31)]), r.choice(ARGUMENTS + [r.randrange(-2**31, 2**31)]),
                            r.choice(SECOND_ARGUMENTS)), wins=wins, word=r.choice([0, 1, 2, -1]), lobby=r.randrange(2),
                paused=r.choice([0, 1, 5]), timers=r.randrange(5), key=r.choice([0, 1, 2]),
                starts=tuple(r.randrange(0, 2**32) for _ in range(TIMERS)),
                q=dict(answers(r.choice([0, 1, -1]), r.choice([0, 1, 2]), default=r.randrange(-2**31, 2**31), send=r.randrange(100)),
                       IveGotTicks=[r.randrange(0, 2**32) for _ in range(TIMERS)]))


# ---- UIMain_EnableAllKeyTables


def enable_edges():
    for key in (0, 1, 2, 255):
        yield case(ENABLE_KEYS, key=key)


def enable_rnd(r):
    return case(ENABLE_KEYS, key=r.randrange(256), word=r.randrange(2), paused=r.randrange(3))


# ---- UI_UnPauseTimerSystem


def unpause_edges():
    for paused in (0, 1, 2, -1, 0x7FFFFFFF, -0x80000000, 256):
        for timers in range(TIMERS + 1):
            yield case(UNPAUSE, paused=paused, timers=min(timers, TIMERS), starts=(1, 2, 3, 4),
                       q={"IveGotTicks": [0xFFFFFFFF, 0x80000000, 0, 7]})


def unpause_rnd(r):
    return case(UNPAUSE, paused=r.choice([0, 1, r.randrange(-2**31, 2**31)]), timers=r.randrange(TIMERS + 1),
                starts=tuple(r.randrange(0, 2**32) for _ in range(TIMERS)),
                q={"IveGotTicks": [r.randrange(0, 2**32) for _ in range(r.randrange(1, 5))]})


# ---- GameConfirmQuit_OK


def quit_edges():
    for lobby in (1, 0):
        for data in (0, 1, -1, 0x1234):
            yield case(QUIT_OK, (0, data, 0, 0), lobby=lobby)


def quit_rnd(r):
    return case(QUIT_OK, (0, r.choice([0, r.randrange(-2**31, 2**31)]), 0, 0), lobby=r.randrange(2), first=r.randrange(3),
                user=r.randrange(2))


# ---- GameConfirmDelPlayer_OK


def delete_edges():
    for user in (0, 1):
        for slot in (0, 1, 2, 3, 4, -1, 0x7FFFFFFF, -0x80000000, 0x12345678):
            yield case(DELETE_OK, (0, slot, 0, 0), user=user)


def delete_rnd(r):
    return case(DELETE_OK, (0, r.choice([0, 1, 2, 3, r.randrange(-2**31, 2**31)]), 0, 0), user=r.randrange(2), first=r.randrange(3))


# ---- InterfaceCheckButtons::GetCurrent


def current_edges():
    for current in (-1, 0, 1, CHOICE_EASY, CHOICE_HARD, 0x7A17, 0x7FFFFFFF, -0x80000000, 0x12345678):
        for window in (0, 3, WINDOWS - 1):
            yield case(GET_CURRENT, (window, 0, 0, 0), wins={window: dict(current=current)})


def current_rnd(r):
    window = r.randrange(WINDOWS)
    return case(GET_CURRENT, (window, 0, 0, 0), wins={i: dict(current=r.randrange(-2**31, 2**31)) for i in range(WINDOWS)})


def setup(runner):
    T.install_std_stubs(runner.emu)


TESTS = [FnTest("CreatePlayerMsg", ppc, host, edge=create_edges, rnd=create_rnd),
         FnTest("NewNameOK", ppc, host, edge=name_edges, rnd=name_rnd),
         FnTest("DefaultDialogCallback", ppc, host, edge=default_edges, rnd=default_rnd),
         FnTest("DialogBoxCallback", ppc, host, edge=dialog_edges, rnd=dialog_rnd),
         FnTest("UIMain_EnableAllKeyTables", ppc, host, edge=enable_edges, rnd=enable_rnd),
         FnTest("UI_UnPauseTimerSystem", ppc, host, edge=unpause_edges, rnd=unpause_rnd),
         FnTest("GameConfirmQuit_OK", ppc, host, edge=quit_edges, rnd=quit_rnd),
         FnTest("GameConfirmDelPlayer_OK", ppc, host, edge=delete_edges, rnd=delete_rnd),
         FnTest("InterfaceCheckButtons::GetCurrent", ppc, host, edge=current_edges, rnd=current_rnd)]
