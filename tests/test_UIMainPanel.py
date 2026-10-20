"""Unit w101, the UI main panel helpers, against libtpw_unit_UIMainPanel.dylib (tests/unit_UIMainPanel.cpp), in the main app
("SimTheme Park"):
  src/ui_main_panel.cpp  UIMain_UISound 0x101802cc, UIMain_CheckPanel 0x10180514, UIMain_SetPanel 0x101805a0, UIMain_SetTitleText 0x10180794,
                         UIMain_AdvisorMessage 0x101821cc, UIMain_CancelAdvisorMessage 0x1018227c, UIMain_DefaultPanelMsg 0x1018493c,
                         UIMain_GetFullyOutOfFirstPerson 0x10187434

Real on both sides: CoolRect (the constructor, Set and the destructor run from the original on the PowerPC side). Every other callee is a
recording fake on both sides: SOUND_SendEvent, UI_SendMessage, UIMain_DecideKbCapture (both of these record the label of gUIPanelWindow at the
time of the call), UI_TITLE_COLOUR, RemoveSlideoutPanel, UI_SetGameWindowCursor, CKeyTable::Disable/KeyDown/KeyUp, CMouse::AssignMouseMode,
CMMDefault::CMMDefault, the title text object's constructor and SetPreDrawCallback, the InterfaceWindow members the title code calls (the
window it asks for comes from the queue "Child", the rectangle GetPosition writes from the queues RectL/RectT/RectR/RectB), CWorld::GetAdvisor,
CAdvisorRequest's constructor, CAdvisor::QueueRequest/CancelMessage/PlayLastTutorialMessage, and the two first-person functions. The nothrow
operator new answers from the queue "New" (0: NULL) and records the size as the Mac's (1 for the mouse, an empty class there, 8, 0x19c). The
current mouse mode is a fake object whose virtual at +0x2c answers the queue "GetMode" (a PowerPC-side vtable of the test's).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIMainPanel"
MASK = 0xFFFFFFFF

UISOUND, CHECKPANEL, SETPANEL, SETTITLE = 0x101802CC, 0x10180514, 0x101805A0, 0x10180794
ADVISOR, CANCEL, DEFAULTMSG, FIRSTPERSON = 0x101821CC, 0x1018227C, 0x1018493C, 0x10187434
# The callees (in-binary addresses).
SOUND_SEND_EVENT, UI_SEND_MESSAGE, DECIDE_KB = 0x100DD9FC, 0x101C9518, 0x1018116C
DISABLE, KEY_DOWN, KEY_UP = 0x101500EC, 0x1014FDEC, 0x1014FEF0
ASSIGN_MOUSE_MODE, MMDEFAULT_CTOR = 0x1007E2E0, 0x1007EA78
REMOVE_SLIDEOUT, SET_GAME_CURSOR = 0x101A83D8, 0x101A8C58
TITLE_CTOR, SET_PRE_DRAW = 0x1018FBE8, 0x1018D0BC
GET_WINDOW, GET_POSITION, SET_POSITION, SET_DRAW_OBJECT = 0x101DBE40, 0x101DB884, 0x101DAE98, 0x101DB474
SET_FONT, SET_FONT_JUST, TITLE_COLOUR, SET_WINDOW_DRAW_TEXT = 0x101DA118, 0x101DA204, 0x1018BD3C, 0x101DE1BC
CALL_DEFAULT = 0x101DE318
GET_ADVISOR, REQUEST_CTOR, QUEUE_REQUEST = 0x1013EAB0, 0x1000DEB4, 0x1000AE48
CANCEL_MESSAGE, PLAY_LAST = 0x1000B048, 0x1000B1C4
STOP_RIDING, TOGGLE_WALK = 0x10032584, 0x10033B90
# The globals.
G_PANEL, G_MOUSE, G_W1E0, G_W1E4, G_MODE = 0x1038CC5C, 0x102DC1F4, 0x102DC1E0, 0x102DC1E4, 0x102DC1F0
G_TITLE, G_TEXT, G_FLAGS, G_WORLD, G_SOUND = 0x1038CC50, 0x1037FF18, 0x102B5A40, 0x1037EBFC, 0x102F7CA0
G_KEYS = [0x1037F22C, 0x1037F228, 0x1037F230, 0x1037F220]   # camera, cheat, game, shortcuts
FONT_BASE = G_TEXT + 0x1C

# Test objects: label -> size. 1 the panel window, 2 another, 3 the child, 14 the previous panel; 4 to 7 the key tables; 8 a mouse; 9 the
# fake mode; 10 a title text object; 11 the advisor; 12 the world.
SIZES = {1: 0x200, 2: 0x200, 3: 0x200, 14: 0x200, 4: 0x20, 5: 0x20, 6: 0x20, 7: 0x20, 8: 8, 9: 8, 10: 0x20, 11: 0x20, 12: 0x40}
TEXTS = [None, "", "Title", "A longer title 12?"]
DEFAULT_STATE = dict(panel=0, mouse=0, mode=0, w1e0=0x1234, w1e4=0x5678, title=0, group=1, flags=0, world=12, sound=0x2468, draw=0)
STATE_KEYS = ["panel", "mouse", "mode", "w1e0", "w1e4", "title", "group", "flags", "world", "sound", "draw"]


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def w16(e, a, v):
    e.write(a, struct.pack(">h", T.s16(v)))


def s16(e, a):
    return struct.unpack(">h", e.read(a, 2))[0]


def F(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


STUBS = None


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder

    def panel(e):
        return e.cl.lab(e.u32(G_PANEL))

    def send_message(e):
        e.cl.rec("UI_SendMessage", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), panel(e)))
        e.set_r(3, 0)

    def decide(e):
        e.cl.rec("DecideKbCapture", (panel(e),))

    def new(e):
        L, size = e.cl, e.r(3)
        if L.ret("New", 1) == 0:
            L.rec("operator_new", (size, 0))
            e.set_r(3, 0)
            return
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        L.newlab(block)
        L.rec("operator_new", (size, L.lab(block)))
        e.set_r(3, block)

    def get_window(e):
        L = e.cl
        L.rec("GetWindow", (L.lab(e.r(3)), T.s32(e.r(4))))
        e.set_r(3, L.addr.get(L.ret("Child", 0), 0))

    def get_position(e):
        L, rect = e.cl, e.r(4)
        L.rec("GetPosition", (L.lab(e.r(3)), T.s16(e.r(5))))
        for offset, key in ((4, "RectL"), (6, "RectT"), (8, "RectR"), (0xA, "RectB")):
            w16(e, rect + offset, L.ret(key, 0))
        e.set_r(3, 1)

    def set_position(e):
        L, rect = e.cl, e.r(4)
        L.rec("SetPosition", (L.lab(e.r(3)),) + tuple(s16(e, rect + o) for o in (4, 6, 8, 0xA)))
        e.set_r(3, 1)

    def set_font(e):
        L = e.cl
        index, leftover = divmod(T.s32(e.r(4) - FONT_BASE), 0x18)
        L.rec("SetFont", (L.lab(e.r(3)), index, leftover))

    def get_advisor(e):
        L = e.cl
        L.rec("GetAdvisor", (L.lab(e.r(3)),))
        e.set_r(3, L.addr.get(L.ret("Advisor", 0), 0))

    S.imp("__nw__FUl", new)
    S.internal(SOUND_SEND_EVENT, R("SOUND_SendEvent", "iiiiii"))
    S.internal(UI_SEND_MESSAGE, send_message)
    S.internal(DECIDE_KB, decide)
    S.internal(DISABLE, R("Disable", "L"))
    S.internal(KEY_DOWN, R("KeyDown", "Lhh"))
    S.internal(KEY_UP, R("KeyUp", "Lhh"))
    S.internal(ASSIGN_MOUSE_MODE, R("AssignMouseMode", "LL"))
    S.internal(MMDEFAULT_CTOR, R("CMMDefault.ctor", "L"))
    S.internal(REMOVE_SLIDEOUT, R("RemoveSlideoutPanel", "h"))
    S.internal(SET_GAME_CURSOR, R("UI_SetGameWindowCursor", "h"))
    S.internal(TITLE_CTOR, R("TitleText.ctor", "LL"))
    S.internal(SET_PRE_DRAW, R("SetPreDrawCallback", "Lu"))
    S.internal(GET_WINDOW, get_window)
    S.internal(GET_POSITION, get_position)
    S.internal(SET_POSITION, set_position)
    S.internal(SET_DRAW_OBJECT, R("SetDrawObject", "LL"))
    S.internal(SET_FONT, set_font)
    S.internal(SET_FONT_JUST, R("SetFontJust", "Lii"))
    S.internal(TITLE_COLOUR, R("TitleColour", "L"))
    S.internal(SET_WINDOW_DRAW_TEXT, R("SetWindowDrawText", "LS"))
    S.internal(CALL_DEFAULT, R("CallDefaultWindowHandler", "Liii", ret="i"))
    S.internal(GET_ADVISOR, get_advisor)
    S.internal(REQUEST_CTOR, R("Request.ctor", "Niwuiii"))
    S.internal(QUEUE_REQUEST, R("QueueRequest", "LL"))
    S.internal(CANCEL_MESSAGE, R("CancelMessage", "Li"))
    S.internal(PLAY_LAST, R("PlayLastTutorialMessage", "L"))
    S.internal(STOP_RIDING, R("StopRidingRideIn1stPerson", ""))
    S.internal(TOGGLE_WALK, R("ToggleFirstPersonWalkAround", ""))
    return S


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs()
    # The fake mouse mode's vtable: the slot at +0x2c is the GetMode virtual (a descriptor of a fake function).
    get_mode = T.fake_function(e, CL.recorder("GetMode", "L", ret="i"))
    e.up_vtable = T.persist_alloc(e, 0x40)
    ws(e, e.up_vtable + 0x2C, [get_mode])


class Ppc:
    """One case on the PowerPC side: the test objects built and labelled, the globals set, the fakes active, a fresh log."""

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        e.cl = self.L = CL.PpcLog({}, case.get("q"))
        self.L.addr = {}
        self.saved = STUBS.activate(e)
        for label, size in SIZES.items():
            address = e.alloc(size, b"\xa5" * size)
            self.L.labels[address] = label
            self.L.addr[label] = address
        ws(e, self.L.addr[9], [e.up_vtable, 0xA5A5A5A5])
        self.set_state(case.get("st", {}))

    def a(self, label):
        return self.L.addr.get(label, 0)

    def set_state(self, changes):
        e, st = self.e, dict(DEFAULT_STATE, **changes)
        ws(e, G_PANEL, [self.a(st["panel"])])
        ws(e, G_MOUSE, [self.a(st["mouse"])])
        ws(e, G_MODE, [self.a(st["mode"])])
        ws(e, G_W1E0, [st["w1e0"]])
        ws(e, G_W1E4, [st["w1e4"]])
        ws(e, G_TITLE, [self.a(st["title"])])
        ws(e, G_TEXT + 4, [st["group"]])
        ws(e, G_FLAGS, [st["flags"]])
        ws(e, G_WORLD, [self.a(st["world"])])
        ws(e, G_SOUND, [st["sound"]])
        ws(e, self.a(3) + 0x94, [0x7C0DD000 if st["draw"] else 0])
        for address, label in zip(G_KEYS, (4, 5, 6, 7)):
            ws(e, address, [self.a(label)])

    def dump(self):
        e, L = self.e, self.L
        return [L.lab(e.u32(G_PANEL)), L.lab(e.u32(G_MOUSE)), L.lab(e.u32(G_MODE)), T.s32(e.u32(G_W1E0)), T.s32(e.u32(G_W1E4)),
                L.lab(e.u32(G_TITLE)), e.u32(G_TEXT + 4), T.s32(e.u32(G_FLAGS)), L.lab(e.u32(G_WORLD)), T.s32(e.u32(G_SOUND)),
                int(e.u32(self.a(3) + 0x94) != 0)]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]


def host_start(lib, case):
    """Labels the host's test objects like the PowerPC side's and sets the state."""
    CL.host_begin(lib, {label: F(lib, "tpw_up_obj", C.c_void_p, C.c_int32)(label) for label in SIZES}, case.get("q"))
    st = dict(DEFAULT_STATE, **case.get("st", {}))
    words = (C.c_int32 * 11)(*[T.s32(st[key] & MASK) for key in STATE_KEYS])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    F(lib, "tpw_up_state", None, C.POINTER(C.c_int32))(words)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1


def host_dump(lib):
    out = (C.c_int32 * 11)()
    F(lib, "tpw_up_dump", None, C.POINTER(C.c_int32))(out)
    return list(out)


def ppc_case(address, args_of, result_of=None):
    def run(e, case):
        p = Ppc(e, case)
        try:
            text = TEXTS[case["text"]] if "text" in case else None
            if "text" in case:
                text_address = 0 if text is None else CL.new_wide(e, text)
                args = args_of(p, case, text_address)
            else:
                args = args_of(p, case, 0)
            r3 = p.call(address, *args)
            return dict(result=result_of(r3) if result_of else 0, state=p.dump(), log=p.L.log)
        finally:
            p.done()
    return run


def host_case(name, restype, args_of):
    def run(lib, case):
        host_start(lib, case)
        result = F(lib, name, restype, *[C.c_int32] * len(args_of(case)))(*args_of(case))
        log = CL.host_end(lib)
        return dict(result=0 if result is None else result, state=host_dump(lib), log=log)
    return run


def s32_result(r3):
    return T.s32(r3)


def bool_result(r3):
    return r3 & 0xFF


# ---------------------------------------------------------------- state generators

def random_state(r, **fixed):
    st = dict(
        panel=r.choice([0, 1, 14]), mouse=r.choice([0, 8]), mode=r.choice([0, 9]), w1e0=r.choice([0, -1, 0x1234, r.getrandbits(32)]),
        w1e4=r.choice([0, -1, 0x5678, r.getrandbits(32)]), title=r.choice([0, 10]), group=r.choice([0, 1, 2, 3]),
        flags=r.choice([0, 2, 4, 0x400, r.getrandbits(32)]), world=r.choice([12, 12, 0]), sound=r.choice([0, 1, -1, r.getrandbits(32)]),
        draw=r.choice([0, 1]))
    st.update(fixed)
    return {k: (v & MASK) if k in ("w1e0", "w1e4", "flags", "sound") else v for k, v in st.items()}


# ---------------------------------------------------------------- UIMain_UISound

def sound_case(code, sound):
    return dict(code=code, st=dict(sound=sound & MASK))


def sound_edges():
    for code in (0, 1, 5, 0x7F, 0x100, 0x7FFFFFFF, -1, -0x80000000):
        for sound in (0, 1, -1, 0x2468, 0x7C0000):
            yield sound_case(code & MASK if code < 0 else code, sound)


def sound_random(r):
    return sound_case(r.choice([r.getrandbits(32), r.randrange(0, 600)]), r.choice([r.getrandbits(32), 0, 7]))


# ---------------------------------------------------------------- UIMain_CheckPanel

def check_edges():
    for panel in (0, 1, 14):
        for world in (12, 0):
            yield dict(st=dict(panel=panel, world=world))


def check_random(r):
    return dict(st=random_state(r))


# ---------------------------------------------------------------- UIMain_SetPanel

def panel_case(window, mouse, mode, answer, new, **more):
    return dict(window=window, st=dict(mouse=mouse, mode=mode, **more), q={"GetMode": [answer], "New": new})


def panel_edges():
    r = random.Random(101)
    # every success or failure pattern of the five allocations the function can make, with a window
    for pattern in range(32):
        new = [(pattern >> i) & 1 for i in range(5)]
        for mouse in (0, 8):
            for mode, answer in ((0, 0), (9, 9), (9, 1), (9, 8), (9, 10), (9, -1), (9, 0x10000009)):
                yield panel_case(1, mouse, mode, answer, new, panel=r.choice([0, 14]))
    # no window: only the last two lines run, whatever the rest is
    for mouse in (0, 8):
        for mode in (0, 9):
            for panel in (0, 1, 14):
                yield panel_case(0, mouse, mode, 9, [0, 1], panel=panel)
    yield panel_case(2, 8, 9, 9, [1], panel=1)


def panel_random(r):
    return panel_case(r.choice([0, 1, 1, 1, 2]), r.choice([0, 8]), r.choice([0, 9]), r.choice([9, 9, 1, 0, 8, 10, r.getrandbits(32)]),
                      [r.choice([0, 1, 1]) for _ in range(r.randrange(1, 6))], panel=r.choice([0, 1, 14]), w1e0=r.getrandbits(32),
                      w1e4=r.getrandbits(32))


# ---------------------------------------------------------------- UIMain_SetTitleText

RECTS = [(100, 10, 200, 50), (100, 10, 201, 50), (-100, 0, -1, 7), (0, 0, 0, 0), (5, 5, 6, 6), (-5, 2, 0, 3), (-3, -3, -8, 0), (-3, 4, -6, 4),
         (-2, 4, -3, 4), (7, 1, 4, 9), (1, 1, 0, 1), (-32768, 0, 32767, 100), (32767, 0, -32768, 100), (-32768, -32768, 32767, 32767),
         (30000, 1, 32767, 2), (-32768, 1, -30000, 2), (1000, 0, -1000, 0), (0x7FFF, 5, 0x7FFF, 6), (-1, -1, 0, 0), (-1, 0, -2, 0)]


def title_case(window, child, title, new, rect, text, group, draw):
    return dict(window=window, text=text, st=dict(title=title, group=group, draw=draw),
                q={"Child": [child], "New": new, "RectL": [rect[0]], "RectT": [rect[1]], "RectR": [rect[2]], "RectB": [rect[3]]})


def title_edges():
    for rect in RECTS:
        for draw in (0, 1):
            yield title_case(1, 3, 10, [1], rect, 2, 1, draw)
    for child in (0, 3):
        for title in (0, 10):
            for new in ([1], [0]):
                for draw in (0, 1):
                    for text in range(4):
                        for group in (0, 1, 2, 3):
                            yield title_case(1, child, title, new, RECTS[(text + group) % len(RECTS)], text, group, draw)
    for group in (4, 5, 8):
        yield title_case(1, 3, 10, [1], RECTS[0], 2, group, 0)


def title_random(r):
    rect = (r.choice([r.randrange(-32768, 32768), r.randrange(-500, 500)]), r.randrange(-32768, 32768),
            r.choice([r.randrange(-32768, 32768), r.randrange(-500, 500)]), r.randrange(-32768, 32768))
    return title_case(r.choice([1, 1, 2]), r.choice([0, 3, 3, 3]), r.choice([0, 10]), [r.choice([0, 1, 1])], rect, r.randrange(4),
                      r.randrange(4), r.choice([0, 0, 1]))


# ---------------------------------------------------------------- the advisor hooks

MESSAGES = [0, 1, 2, 0x15E, 0x15F, 0x160, 0x161, 0x7FFFFFFF, -1 & MASK, 0x80000000, 0x12345678, 100, 200, 350]


def advisor_case(message, advisor, world):
    return dict(message=message, st=dict(world=world), q={"Advisor": [advisor]})


def advisor_edges():
    for message in MESSAGES:
        for advisor in (11, 0):
            for world in (12, 0):
                yield advisor_case(message, advisor, world)


def advisor_random(r):
    return advisor_case(r.choice([r.getrandbits(32), r.randrange(0, 352)]), r.choice([11, 11, 0]), r.choice([12, 12, 0]))


# ---------------------------------------------------------------- UIMain_DefaultPanelMsg

KEY_MESSAGES = [0x1000A, 0x1000B, 0x1000C, 0x1000D, 0x14, 0x13, 0x15, 0x10009, 0x1, 0, MASK, 0x7FFFFFFF, 0x80000000, 0x10000, 0x20000A,
                0x30000B, 0x1000A + 0x100000000 & MASK, 0xFFFF000A, 0x4, 0x5, 0x16, 0x1F, 0x100, 0x10, 0x11]
KEYS = [0x1B, 0x70, 0x1A, 0x1C, 0x6F, 0x71, 0, MASK, 0x1001B, 0x10070, 0x8000001B, 0xFFFF8070, 0x41, 0x7FFF, 0x8000, 0xFFFF, 0x10000]
MODIFIERS = [0, 1, 0xFF, 0x100, 0x2, 0x80, 0xFE]


def default_case(message, a, hi, low, top, panel, answer):
    b = (top << 24) | (hi << 16) | low
    return dict(args=[1, message, a, b], st=dict(panel=panel), q={"CallDefaultWindowHandler": [answer]})


def default_edges():
    for message in KEY_MESSAGES:
        for a in KEYS:
            for hi in MODIFIERS:
                if a in (0x1B, 0x70) or hi in (0, 1, 0x100) or message in (0x14, 0x1000C):
                    yield default_case(message, a, hi, 0x1234 if a & 1 else 0, 0xAB if hi == 0x100 else 0, 14, 77)
    for panel in (0, 1, 14):
        yield default_case(0x14, 3, 0, 0, 0, panel, 5)
    # bits 24 to 31 of the last argument are not part of the modifiers; bit 16 to 23 are
    for top in (0, 0x80, 0xFF):
        for hi in (0, 1):
            yield default_case(0x1000B, 0x1B, hi, 0xFFFF, top, 14, 3)
            yield default_case(0x1000B, 0x70, hi, 0xFFFF, top, 14, 3)


def default_random(r):
    return default_case(r.choice([r.choice(KEY_MESSAGES), r.getrandbits(32), 0x1000A, 0x1000B, 0x14]), r.choice([r.choice(KEYS), r.getrandbits(32)]),
                        r.choice([0, 0, 1, r.getrandbits(8), 0x100 | r.getrandbits(8)]) & 0x1FF, r.getrandbits(16), r.getrandbits(8),
                        r.choice([0, 1, 14]), r.choice([0, 1, -1, r.getrandbits(32)]) & MASK)


# ---------------------------------------------------------------- UIMain_GetFullyOutOfFirstPerson

def firstperson_edges():
    for bits in range(8):
        flags = (2 if bits & 1 else 0) | (4 if bits & 2 else 0) | (0x400 if bits & 4 else 0)
        for other in (0, 1, 0x1F8, 0xFFFFFBF9 & ~0x400, 0x80000000, 0x2000, MASK & ~0x406):
            yield dict(st=dict(flags=flags | other))
    for flags in (MASK, 0, 1, 0x400, 0x402, 0x404, 0x406, 8, 0x10, 0x80):
        yield dict(st=dict(flags=flags))


def firstperson_random(r):
    return dict(st=dict(flags=r.choice([r.getrandbits(32), r.getrandbits(32) & 0x406, r.getrandbits(32) & ~0x406 & MASK])))


# ---------------------------------------------------------------- the tests

TESTS = [
    FnTest("UIMain_UISound", ppc_case(UISOUND, lambda p, c, t: [c["code"]]), host_case("tpw_up_sound", None, lambda c: [T.s32(c["code"])]),
           edge=sound_edges, rnd=sound_random),
    FnTest("UIMain_CheckPanel", ppc_case(CHECKPANEL, lambda p, c, t: [], bool_result), host_case("tpw_up_checkpanel", C.c_int32, lambda c: []),
           edge=check_edges, rnd=check_random),
    FnTest("UIMain_SetPanel", ppc_case(SETPANEL, lambda p, c, t: [p.a(c["window"])]), host_case("tpw_up_setpanel", None, lambda c: [c["window"]]),
           edge=panel_edges, rnd=panel_random),
    FnTest("UIMain_SetTitleText", ppc_case(SETTITLE, lambda p, c, t: [p.a(c["window"]), 0x4D2, t]),
           host_case("tpw_up_settitle", None, lambda c: [c["window"], 0x4D2, c["text"]]), edge=title_edges, rnd=title_random),
    FnTest("UIMain_AdvisorMessage", ppc_case(ADVISOR, lambda p, c, t: [c["message"]]),
           host_case("tpw_up_advisor", None, lambda c: [T.s32(c["message"])]), edge=advisor_edges, rnd=advisor_random),
    FnTest("UIMain_CancelAdvisorMessage", ppc_case(CANCEL, lambda p, c, t: [c["message"]]),
           host_case("tpw_up_cancel", None, lambda c: [T.s32(c["message"])]), edge=advisor_edges, rnd=advisor_random),
    FnTest("UIMain_DefaultPanelMsg", ppc_case(DEFAULTMSG, lambda p, c, t: [p.a(c["args"][0])] + c["args"][1:], s32_result),
           host_case("tpw_up_defaultmsg", C.c_int32, lambda c: [c["args"][0]] + [T.s32(x & MASK) for x in c["args"][1:]]),
           edge=default_edges, rnd=default_random),
    FnTest("UIMain_GetFullyOutOfFirstPerson", ppc_case(FIRSTPERSON, lambda p, c, t: []), host_case("tpw_up_firstperson", None, lambda c: []),
           edge=firstperson_edges, rnd=firstperson_random),
]
