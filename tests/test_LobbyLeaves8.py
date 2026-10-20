"""Unit w88, the lobby's small leaves part 8, against libtpw_unit_LobbyLeaves8.dylib (tests/unit_LobbyLeaves8.cpp), in the main app
("SimTheme Park"):
  src/RideDB_effects.cpp         CRideDatabase::DeleteCustomEffects 0x10156e34
  src/ui_main_pause.cpp          PauseUI 0x1018679c, TogglePauseUI 0x10186854
  src/ui_mainpanel2_window.cpp   ShowPauseWindow 0x101a86d0, TogglePauseWindow 0x101a87c4, PauseWindowMsg 0x101a8504
  src/ui_pause_data.cpp          the pause window's script (0x102aacfc, 21 words) and animation (0x102aadbc, 5 frames)

A case is a world (the ride database in emulator memory at the Mac's layout, the windows, the key table and the globals) and an operation, built
twice from one description: for the original in emulator memory and for the translation by the unit's entry points (tpw_l8_*). Every callee
outside the unit is a fake that records the same event on both sides (the emitter and sound frees, the timers, the message send, Dialog, the
default dialog callback, GetPosition, setPivot, startAnim, the key table's Search, the game's TogglePause). Dialog also logs the 21 words of the
script it is given and startAnim the 20 words of the frames it is given: on the PowerPC side they are read from the original's data, on the host
side from the translation's arrays, which is what shows that the data was copied right. The fakes can be told by the case's queue "Hook" to
change the state behind the function's back (gPauseWindow, the ride count, the walk's position), which shows that the Mac reads it again.

What only the PowerPC side can see is compared as a number the host reports as 1: Dialog's callback argument is the descriptor 0x10267c38 (the
host reports that the callback is PauseWindowMsg), and its script argument is 0x102aacfc.

Not tested: the answers of void functions (r3 is not part of the contract), DeleteCustomEffects past the 0x14a slots (the host array ends there),
a NULL key table pointer (both sides would fault), a NULL window (GetPosition is a fake).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves8"
MASK = 0xFFFFFFFF

DELETE_EFFECTS, PAUSE_UI, TOGGLE_PAUSE_UI = 0x10156E34, 0x1018679C, 0x10186854
SHOW_PAUSE, TOGGLE_PAUSE_WINDOW, PAUSE_MSG = 0x101A86D0, 0x101A87C4, 0x101A8504
KILL_EMITTER, FREE_CATEGORY = 0x100BBF78, 0x100DC89C
PAUSE_TIMERS, UNPAUSE_TIMERS, SEND_MESSAGE, DECIDE_KB = 0x101D8B64, 0x101D8BA0, 0x101C9518, 0x1018116C
DIALOG, DEFAULT_CALLBACK = 0x101F12C8, 0x101F1448
GET_POSITION, SET_PIVOT, START_ANIM = 0x101DB884, 0x101DDD34, 0x101DD08C
KEY_SEARCH, GAME_TOGGLE = 0x1014FD68, 0x10149A74
G_ROOT, G_PAUSE, G_KEYTABLE, G_GAME = 0x10395C24, 0x103949CC, 0x1037F234, 0x1037F134
SCRIPT, ANIM, DESCRIPTOR = 0x102AACFC, 0x102AADBC, 0x10267C38
SCRIPT_WORDS, ANIM_WORDS = 21, 20
RIDE_SLOTS = 8
MESSAGE = "message"


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def pattern(n, fill):
    return bytes([fill & 0xFF]) * n


def shorts(e, address, count):
    return list(struct.unpack(">" + "h" * count, e.read(address, 2 * count)))


def log_words(L, name, words, count):
    """The events the unit's LogWords makes: ten words to an event, the index first."""
    for i in range(0, count, 10):
        L.rec(name, (i,) + tuple(words[i + k] if i + k < count else 0 for k in range(10)))


# ---------------------------------------------------------------------------------------------------------------- the fakes


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real = {DELETE_EFFECTS, PAUSE_UI, TOGGLE_PAUSE_UI, SHOW_PAUSE, TOGGLE_PAUSE_WINDOW, PAUSE_MSG}

    def hook(e, L):
        action = L.ret("Hook", 0)
        w = e.l8
        if action == 1:
            ws(e, G_PAUSE, [w.windows[2]])
        elif action == 2:
            ws(e, G_PAUSE, [0])
        elif action == 3 and w.db:
            ws(e, w.db + 0x528, [L.ret("HookCount", 0)])
        elif action == 4 and w.db:
            ws(e, w.db + 0x72C, [L.ret("HookPos", 0)])

    def dialog(e):
        L = e.cl
        script, callback, modal = e.r(3), e.r(4), T.s32(e.r(5))
        L.rec("Dialog", (int(script == SCRIPT), int(callback == DESCRIPTOR), modal))
        if script == SCRIPT:
            log_words(L, "Dialog.script", shorts(e, SCRIPT, SCRIPT_WORDS), SCRIPT_WORDS)
        hook(e, L)
        e.set_r(3, e.l8.windows[1] if L.ret("Dialog", 0) != 0 else 0)

    def default_callback(e):
        L = e.cl
        L.rec("DefaultDialogCallback", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        e.set_r(3, T.u32(L.ret("DefaultAnswer", 0)))

    def get_position(e):
        L, rect = e.cl, e.r(4)
        L.rec("GetPosition", (L.lab(e.r(3)), T.s16(e.r(5))))
        values = [L.ret("Rect", 0) for _ in range(4)]
        e.write(rect + 4, struct.pack(">hhhh", *[T.s16(v & 0xFFFF) for v in values]))
        e.set_r(3, 1)

    def start_anim(e):
        L, anim = e.cl, e.r(4)
        L.rec("startAnim", (L.lab(e.r(3)), int(anim == ANIM), T.s32(e.r(5))))
        words = shorts(e, ANIM, ANIM_WORDS) if anim == ANIM else [0] * ANIM_WORDS
        log_words(L, "startAnim.frames", words, ANIM_WORDS)

    def send_message(e):
        L = e.cl
        L.rec("UI_SendMessage", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        hook(e, L)
        e.set_r(3, 0)

    S.internal(KILL_EMITTER, R("PAR_KillLibEmitter", "u", then=hook))
    S.internal(FREE_CATEGORY, R("SOUND_FreeCatagory", "i", then=hook))
    S.internal(PAUSE_TIMERS, R("UI_PauseTimerSystem"))
    S.internal(UNPAUSE_TIMERS, R("UI_UnPauseTimerSystem"))
    S.internal(SEND_MESSAGE, send_message)
    S.internal(DECIDE_KB, R("UIMain_DecideKbCapture"))
    S.internal(DIALOG, dialog)
    S.internal(DEFAULT_CALLBACK, default_callback)
    S.internal(GET_POSITION, get_position)
    S.internal(SET_PIVOT, R("setPivot", "Lhh"))
    S.internal(START_ANIM, start_anim)
    S.internal(KEY_SEARCH, R("Search", "Lhh", ret=lambda e, L: L.ret("Search", 0)))
    S.internal(GAME_TOGGLE, R("TogglePause", "Lb"))
    return S


STUBS = build_stubs()


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    """One case on the PowerPC side: the fakes active, a fresh log, the case's state for the fakes."""

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        e.l8 = self
        self.db = 0
        self.windows = [e.alloc(0x140, pattern(0x140, 0x30 + k)) for k in range(4)]
        for k, address in enumerate(self.windows):
            self.L.labels[address] = k + 1
        self.L.labels[G_GAME] = 5
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.l8 = None

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]


def host_run(lib, entry, description, queues=None):
    fn = getattr(lib, entry)
    fn.restype = C.c_int32
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    values = (C.c_int64 * len(description))(*description)
    out = (C.c_int64 * 2048)()
    CL.host_begin(lib, {}, queues)
    n = fn(values, out)
    log = CL.host_end(lib)
    return [int(out[i]) for i in range(n)], log


def host_only(entry):
    def run(lib, case):
        state, log = host_run(lib, entry, case["d"], case.get("q"))
        return dict(state=state, log=log)
    return run


# ------------------------------------------------------------------------------------------------ CRideDatabase::DeleteCustomEffects
# description: the ride count, the fill byte, the slots filled (at most 8), then per slot: kind (0 NULL, 1 a ride), four emitter words, the
#   sound category
# queues: Hook (after each emitter or sound free: 3 sets the ride count to HookCount, 4 the walk's position to HookPos)


def effects_ppc(e, case):
    p = Ppc(e, case)
    try:
        L = p.L
        count, fill, slots = case["d"][:3]
        cells = [case["d"][3 + 6 * i:9 + 6 * i] for i in range(slots)]
        db = p.db = e.alloc(0x740, pattern(0x740, fill))
        rides = [e.alloc(0x800, pattern(0x800, 0x5A)) if cells[i][0] else 0 for i in range(slots)]
        ws(e, db, [rides[i] if i < slots else 0 for i in range(0x14A)])
        for i in range(slots):
            if rides[i]:
                ws(e, rides[i] + 0x7DC, cells[i][1:5])
                ws(e, rides[i] + 0x7EC, [cells[i][5]])
        ws(e, db + 0x528, [count])
        ws(e, db + 0x72C, [0x12345])
        L.labels[db] = 7
        middle = e.read(db + 0x52C, 0x200)
        p.call(DELETE_EFFECTS, db)
        state = [T.s32(e.u32(db + 0x72C)), T.s32(e.u32(db + 0x528))]
        for i in range(slots):
            if rides[i]:
                state += [e.u32(rides[i] + 0x7DC + 4 * k) for k in range(4)] + [T.s32(e.u32(rides[i] + 0x7EC))]
            else:
                state += [0] * 5
        state.append(int(e.read(db + 0x52C, 0x200) == middle))
        return dict(state=state, log=L.log)
    finally:
        p.done()


effects_host = host_only("tpw_l8_effects")


def word(r):
    return r.choice([0, 0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(8), r.getrandbits(16)])


def category(r):
    return r.choice([0, 0, 1, -1, 7, 0x7FFFFFFF, -0x80000000, r.randint(-1000, 1000), 0x10000])


def effects_case(r, count=None, slots=None, kinds=None, emitters=None, categories=None, hook=None):
    slots = r.randint(0, RIDE_SLOTS) if slots is None else slots
    count = r.choice([0, 1, 2, slots, slots, slots + 1, slots - 1, RIDE_SLOTS + 2, 100, -1, -5, -0x80000000, 0x7FFFFFFF]) if count is None else count
    kinds = [1 if r.random() < 0.85 else 0 for _ in range(slots)] if kinds is None else kinds
    d = [count, r.randint(0, 255), slots]
    for i in range(slots):
        words = emitters[i] if emitters is not None else [word(r) for _ in range(4)]
        d += [kinds[i]] + list(words) + [categories[i] if categories is not None else category(r)]
    q = {}
    if hook is None and r.random() < 0.25:
        hook = [r.choice([0, 0, 3, 4]) for _ in range(6)] + [0]   # (the last value repeats: a hook that sent the walk back every time would never end)
    if hook is not None:
        q = {"Hook": hook, "HookCount": [r.choice([0, 1, 3, 5, RIDE_SLOTS, -1, 100]) for _ in range(4)],
             "HookPos": [r.choice([0, 1, 2, 5, RIDE_SLOTS - 1, RIDE_SLOTS, 0x149]) for _ in range(4)]}
    return dict(d=d, q=q)


def edge_effects():
    r = random.Random(8801)
    none = [[0, 0, 0, 0]] * RIDE_SLOTS
    some = [[1, 0, 2, 0], [0, 0, 0, 7], [0, 5, 0, 0], [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 1]] * 2
    full = [[1, 2, 3, 4]] * RIDE_SLOTS
    for words in (none, some, full):
        for cat in (0, 5, -1):
            for slots in (0, 1, 2, 5, RIDE_SLOTS):
                for count in (0, 1, 2, slots, slots + 1, slots - 1, 6, RIDE_SLOTS + 5, -1, -2, -0x80000000, 0x7FFFFFFF):
                    yield effects_case(r, count, slots, [1] * slots, words[:slots], [cat] * slots, hook=[])
    # a hole in the slots ends the walk; a ride after it is not touched
    for hole in range(4):
        kinds = [0 if i == hole else 1 for i in range(5)]
        yield effects_case(r, 5, 5, kinds, full[:5], [3] * 5, hook=[])
    # emitters and categories one at a time
    for k in range(4):
        words = [[0, 0, 0, 0] for _ in range(2)]
        words[0][k] = 9
        words[1][3 - k] = 0x100
        yield effects_case(r, 2, 2, [1, 1], words, [0, 0], hook=[])
    yield effects_case(r, 2, 2, [1, 1], [[0] * 4] * 2, [-7, 0x10000], hook=[])
    # the count and the position changed behind the walk's back (the Mac reads them again every time round)
    for action in (3, 4):
        for position in range(0, 4):
            for value_index in range(2):
                hook = [action if i == position else 0 for i in range(8)]
                case = effects_case(r, 6, 6, [1] * 6, [[1, 0, 0, 2]] * 6, [4] * 6, hook=hook)
                case["q"]["HookCount"] = [[0, 1, 3, -1][position], 2][value_index:] + [2]
                case["q"]["HookPos"] = [[0, 2, 4, 7][position], 1][value_index:] + [1]
                yield case


def random_effects(r):
    return effects_case(r)


# ------------------------------------------------------------------------------------------------ PauseUI, TogglePauseUI, ShowPauseWindow, TogglePauseWindow
# description: the operation (0 PauseUI, 1 TogglePauseUI, 2 ShowPauseWindow, 3 TogglePauseWindow), the root window (0 none, 1 one), gPauseWindow
#   (0 none, 1 the A window)
# queues: Dialog (0 NULL, 1 the B window), Hook (1 gPauseWindow becomes the C window, 2 it is cleared; after Dialog's record and after UI_SendMessage)

OPS = [PAUSE_UI, TOGGLE_PAUSE_UI, SHOW_PAUSE, TOGGLE_PAUSE_WINDOW]


def pause_ppc(e, case):
    p = Ppc(e, case)
    try:
        op, root, pause = case["d"]
        w = p.windows
        ws(e, G_ROOT, [w[3] if root else 0])
        ws(e, G_PAUSE, [w[0] if pause else 0])
        p.call(OPS[op])
        return dict(state=[p.L.lab(e.u32(G_ROOT)), p.L.lab(e.u32(G_PAUSE))], log=p.L.log)
    finally:
        p.done()


pause_host = host_only("tpw_l8_pause")


def pause_case(r, op=None, root=None, pause=None, dialog=None, hook=None):
    op = r.randint(0, 3) if op is None else op
    root = r.randint(0, 1) if root is None else root
    pause = r.randint(0, 1) if pause is None else pause
    dialog = r.choice([0, 1]) if dialog is None else dialog
    hook = r.choice([0, 0, 0, 1, 2]) if hook is None else hook
    return dict(d=[op, root, pause], q={"Dialog": [dialog], "Hook": [hook]})


def edge_pause():
    r = random.Random(8802)
    for op in range(4):
        for root in (0, 1):
            for pause in (0, 1):
                for dialog in (0, 1):
                    for hook in (0, 1, 2):
                        yield pause_case(r, op, root, pause, dialog, hook)


def random_pause(r):
    return pause_case(r)


def op_tests(op):
    return lambda: [c for c in edge_pause() if c["d"][0] == op]


def op_random(op):
    return lambda r: pause_case(r, op)


# ------------------------------------------------------------------------------------------------ PauseWindowMsg
# description: the message, a, b, the key table's enabled word, gPauseWindow (0 none, 1 the A window), the table's fill byte
# queues: Rect (left, top, right, bottom of GetPosition, four per call), Search (the key table's answer), DefaultAnswer (the default callback's)


def message_ppc(e, case):
    p = Ppc(e, case)
    try:
        message, a, b, enabled, pause, fill = case["d"]
        w = p.windows
        table = e.alloc(0x40, pattern(0x40, fill))
        e.write(table + 6, struct.pack(">h", T.s16(enabled & 0xFFFF)))
        p.L.labels[table] = 6
        ws(e, G_KEYTABLE, [table])
        ws(e, G_PAUSE, [w[0] if pause else 0])
        before = e.read(table, 0x40)
        answer = p.call(PAUSE_MSG, w[3], message, a, b)
        after = e.read(table, 0x40)
        state = [T.s32(answer), p.L.lab(e.u32(G_PAUSE)), struct.unpack(">h", after[6:8])[0], int(before == after)]
        return dict(state=state, log=p.L.log)
    finally:
        p.done()


message_host = host_only("tpw_l8_message")

MESSAGES = [0, 1, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x1D, 0x1E, 0x1F, 0x20, 0x100, 0x101, 0x10001, 0x10005, 0x10006, 0x10007,
            0x10008, 0x10009, 0x1000A, 0x1000B, 0x1000C, 0x1000D, 0x20000, 0x2000B, 0x1001B, 0x7FFFFFFF, -1, -0x80000000, -0x1000B,
            0x8000000B, 0x1000000B, 0x1A, 0x0B, 0x15 + 0x10000, 0x14 + 0x10000]
SEARCH_ANSWERS = [0, 1, -1, 0x10000, -0x10000, 0x8000, 0xFFFF, 0x7FFFFFFF, -0x80000000, 0x12340000, 0x12340001, 2]
ENABLED = [0, 0, 0, 1, -1, 0x7FFF, -0x8000, 0x100]


def message_case(r, message=None, a=None, b=None, enabled=None, pause=None, search=None, rect=None, answer=None):
    message = r.choice(MESSAGES) if message is None else message
    a = r.choice([0, 1, -1, 0x41, 0x7FFF, -0x8000, 0x10000, 0x1FFFF, 0x12345678, r.randint(-0x80000000, 0x7FFFFFFF)]) if a is None else a
    # b: the byte at bits 16..23 is the one the key event tests (zero or not)
    b = r.choice([0, 1, -1, 0x10000, 0xFF0000, 0xFF000000 - 0x100000000, 0x1000000, 0x7FFFFFFF, 0x00FF00FF, 0x00800000, 0x00010000, 0x00100000, 0x00F00000, 0x000F0000, 0x00F0FFFF,
                  r.randint(-0x80000000, 0x7FFFFFFF), r.randint(0, 0xFFFF)]) if b is None else b
    enabled = r.choice(ENABLED) if enabled is None else enabled
    pause = r.randint(0, 1) if pause is None else pause
    search = r.choice(SEARCH_ANSWERS) if search is None else search
    rect = [r.choice([0, 1, -1, 3, 5, -3, -5, 0x7FFF, -0x8000, 1000, 1024, 2047, -2047, r.randint(-0x8000, 0x7FFF)]) for _ in range(8)] if rect is None else rect
    answer = r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randint(-0x80000000, 0x7FFFFFFF)]) if answer is None else answer
    return dict(d=[message, a, b, enabled, pause, r.randint(0, 255)], q={"Rect": rect, "Search": [search], "DefaultAnswer": [answer]})


def edge_message():
    r = random.Random(8803)
    for message in MESSAGES:
        for pause in (0, 1):
            yield message_case(r, message, pause=pause)
    # the key event: every combination of the table's state, the byte and the answer
    for enabled in (0, 1, -1, 0x100, -0x8000):
        for b in (0, 0x10000, 0xFF0000, 0x1000000, 0xFF00FFFF - 0x100000000, 0x00FF00FF, 0x7FFFFFFF, -1, 0x1230000 - 0x1000000):
            for search in (0, 1, -1, 0x10000, 0x8000, 0xFFFF):
                yield message_case(r, 0x1000B, a=r.choice([0x41, -1, 0x1FFFF, 0x8000]), b=b, enabled=enabled, search=search)
    # every bit of the byte at 16..23 alone, and the bits around it
    for bit in list(range(14, 27)):
        yield message_case(r, 0x1000B, b=1 << bit, enabled=0, search=1)
        yield message_case(r, 0x1000B, b=(0x7FFFFFFF & ~(1 << bit)) & ~0xFF0000 if bit < 16 or bit > 23 else 1 << bit, enabled=0, search=1)
        yield message_case(r, 0x1000B, b=(0xFFFFFFFF & ~(1 << bit)) - 0x100000000, enabled=0, search=1)
    # the keys: only the low 16 bits of a, as a signed short
    for a in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x12345, -1, -0x8000, -0x80000000, 0x7FFFFFFF):
        yield message_case(r, 0x1000B, a=a, b=0, enabled=0, search=1)
        yield message_case(r, 0x1000B, a=a, b=0x10000, enabled=0, search=1)
    # the animation: the rectangle's right and bottom, halved by an arithmetic shift of a short
    for right, bottom in ((0, 0), (1, 1), (2, 2), (3, 5), (-1, -1), (-2, -3), (-3, -2), (0x7FFF, -0x8000), (-0x8000, 0x7FFF), (1024, 768), (2047, 1535), (-1, 1)):
        yield message_case(r, 0x15, rect=[7, 9, right, bottom, 1, 1, 1, 1])
        yield message_case(r, 0x15, rect=[-7, -9, right, bottom, 1, 1, 1, 1])
    for answer in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x12345678):
        yield message_case(r, 0x15, answer=answer)
        yield message_case(r, 0x14, answer=answer)
        yield message_case(r, 0x1000B, answer=answer)
        yield message_case(r, 3, answer=answer)


def random_message(r):
    return message_case(r)


TESTS = [
    FnTest("CRideDatabase::DeleteCustomEffects", effects_ppc, effects_host, edge=edge_effects, rnd=random_effects),
    FnTest("PauseUI", pause_ppc, pause_host, edge=op_tests(0), rnd=op_random(0)),
    FnTest("TogglePauseUI", pause_ppc, pause_host, edge=op_tests(1), rnd=op_random(1)),
    FnTest("ShowPauseWindow", pause_ppc, pause_host, edge=op_tests(2), rnd=op_random(2)),
    FnTest("TogglePauseWindow", pause_ppc, pause_host, edge=op_tests(3), rnd=op_random(3)),
    FnTest("PauseWindowMsg", message_ppc, message_host, edge=edge_message, rnd=random_message),
]
