"""Unit w136, the game window's message functions, against libtpw_unit_GameWin.dylib (tests/unit_GameWin.cpp), in the main app
("SimTheme Park"):
  src/ui_main_gamewin.cpp  Engine_GameWinMsg 0x10183c84, Engine_GameFirstPersonMsg 0x10184628

Every callee is a recording fake on both sides: the key tables' KeyDown, KeyUp and Search (one scripted answer per call, from the queues of the
same names), CheckShifts, IveGotTicks, CaptureDevice, UnCaptureDevice, setCursor, CallDefaultWindowHandler, CMouse::AssignMouseMode,
ImmediateMouseHandler and ProcessIngameMouse, CMMDefault::CMMDefault, CCheat::CCheat and IsCheating, ContextMouse, ContextMouseClick,
UnhighlightRide, LLCC_SystemSetMode, EndCoasterEditMode, Will_HandleCameraMessages, the two first-person functions and UI_DoPostcard. labs is the
C library (a stub). The nothrow operator new answers from the queue "New" and records the Mac's sizes (1, 8). The current mouse mode is a fake
object whose virtual at +0x2c answers the queue "GetMode". Compared: the answer, the event log, and every global the functions touch (the
windows and the mouse as labels, the statics and their guard bytes, the 12 bytes of the static CCheat).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_GameWin"
MASK = 0xFFFFFFFF

WINMSG, FPMSG = 0x10183C84, 0x10184628
# The callees (in-binary addresses).
CAPTURE, UNCAPTURE, CALL_DEFAULT, SET_CURSOR = 0x101DCC10, 0x101DCC6C, 0x101DE318, 0x101DD7F0
MMDEFAULT_CTOR, ASSIGN_MOUSE_MODE, IMMEDIATE, PROCESS_INGAME = 0x1007EA78, 0x1007E2E0, 0x1007E094, 0x1007DE84
KEY_DOWN, KEY_UP, SEARCH = 0x1014FDEC, 0x1014FEF0, 0x1014FD68
CHEAT_CTOR, IS_CHEATING = 0x10146F24, 0x101470C8
CONTEXT_MOUSE, CONTEXT_CLICK, UNHIGHLIGHT = 0x10182548, 0x10183320, 0x1006D0F8
SET_MODE, END_EDIT, WILL = 0x10090E18, 0x1019CAD0, 0x100331C4
TICKS, SHIFTS = 0x101CAC3C, 0x1022A3B4
STOP_RIDING, TOGGLE_WALK, POSTCARD = 0x10032584, 0x10033B90, 0x101B232C
# The globals.
G_GAMEWIN, G_FPWIN, G_PANEL = 0x103949E4, 0x103949E0, 0x1038CC5C
G_MOUSE, G_MODE, G_W1E0, G_W1E4 = 0x102DC1F4, 0x102DC1F0, 0x102DC1E0, 0x102DC1E4
G_RMB, G_FLAGS = 0x1038071C + 0x39, 0x102B5A40
G_PTRX, G_PTRY, G_SCRW, G_SCRH = 0x10395BC2, 0x10395BC0, 0x10395BBC, 0x10395BBA
G_RD, G_RDG, G_PT, G_PTG = 0x10380C2C, 0x10380C28, 0x10380C24, 0x10380C20
G_CO, G_COG, G_PX, G_PY = 0x10380C1C, 0x10380C18, 0x10380C16, 0x10380C14
G_PIN, G_C34, G_C38 = 0x10380C30, 0x10380C34, 0x10380C38
G_CHEATG, G_CHEAT, G_KEEP = 0x102DA940, 0x102DA934, 0x102C6CC8
# Key tables: camera, cheat, game, shortcuts, debug (labels 4, 5, 6, 7, 11).
G_TABLES = [(0x1037F22C, 4), (0x1037F228, 5), (0x1037F230, 6), (0x1037F220, 7), (0x1037F21C, 11)]

# Test objects: label -> size. 1 to 3 windows, 4 to 7 and 11 the key tables, 8 a mouse, 9 the fake mode; 10 is the static CCheat.
SIZES = {1: 0x200, 2: 0x200, 3: 0x200, 4: 0x20, 5: 0x20, 6: 0x20, 7: 0x20, 11: 0x20, 8: 8, 9: 8}
LABELS = list(SIZES) + [10]
DEFAULT_STATE = dict(gamewin=1, fpwin=3, panel=0, mouse=0, mode=0, w1e0=0x1234, w1e4=0x5678, rmb=0, flags=0, ptrx=0, ptry=0, scrw=640, scrh=480,
                     rd=0, rdg=1, pt=0, ptg=1, co=1, cog=1, px=0, py=0, pin=0, c34=0x77, c38=0x88, cheatg=1, keep=1)
STATE_KEYS = list(DEFAULT_STATE)
NSTATE = len(STATE_KEYS)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def w16(e, a, v):
    e.write(a, struct.pack(">h", T.s16(v)))


def wb(e, a, v):
    e.write(a, bytes([v & 0xFF]))


def F(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


STUBS = None


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder

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

    S.imp("__nw__FUl", new)
    S.imp("labs", lambda e: e.set_r(3, T.u32(abs(T.s32(e.r(3))))))
    S.internal(CAPTURE, R("CaptureDevice", "L", ret=lambda e, L: 1))
    S.internal(UNCAPTURE, R("UnCaptureDevice", "L", ret=lambda e, L: 1))
    S.internal(CALL_DEFAULT, R("CallDefaultWindowHandler", "Liii", ret="i"))
    S.internal(SET_CURSOR, R("setCursor", "Lh"))
    S.internal(MMDEFAULT_CTOR, R("CMMDefault.ctor", "L"))
    S.internal(ASSIGN_MOUSE_MODE, R("AssignMouseMode", "LL"))
    S.internal(IMMEDIATE, R("ImmediateMouseHandler", "Lii"))
    S.internal(PROCESS_INGAME, R("ProcessIngameMouse", "iiiiL"))
    S.internal(KEY_DOWN, R("KeyDown", "Lhh", ret="i"))
    S.internal(KEY_UP, R("KeyUp", "Lhh", ret="i"))
    S.internal(SEARCH, R("Search", "Lhh", ret="i"))
    S.internal(CHEAT_CTOR, R("CCheat.ctor", "L"))
    S.internal(IS_CHEATING, R("IsCheating", "L", ret="b"))
    S.internal(CONTEXT_MOUSE, R("ContextMouse", "i"))
    S.internal(CONTEXT_CLICK, R("ContextMouseClick", "", ret="i"))
    S.internal(UNHIGHLIGHT, R("UnhighlightRide", ""))
    S.internal(SET_MODE, R("LLCC_SystemSetMode", "ib"))
    S.internal(END_EDIT, R("EndCoasterEditMode", ""))
    S.internal(WILL, R("Will_HandleCameraMessages", "iii"))
    S.internal(TICKS, R("IveGotTicks", "", ret="i"))
    S.internal(SHIFTS, R("CheckShifts", "", ret="h"))
    S.internal(STOP_RIDING, R("StopRidingRideIn1stPerson", ""))
    S.internal(TOGGLE_WALK, R("ToggleFirstPersonWalkAround", ""))
    S.internal(POSTCARD, R("UI_DoPostcard", ""))
    return S


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs()
    # The fake mouse mode's vtable: the slot at +0x2c is the GetMode virtual (a descriptor of a fake function).
    get_mode = T.fake_function(e, CL.recorder("GetMode", "L", ret="i"))
    e.gw_vtable = T.persist_alloc(e, 0x40)
    ws(e, e.gw_vtable + 0x2C, [get_mode])


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
        ws(e, self.L.addr[9], [e.gw_vtable, 0xA5A5A5A5])
        self.L.labels[G_CHEAT] = 10
        self.L.addr[10] = G_CHEAT
        self.set_state(case.get("st", {}))

    def a(self, label):
        return self.L.addr.get(label, 0)

    def set_state(self, changes):
        e, st = self.e, dict(DEFAULT_STATE, **changes)
        ws(e, G_GAMEWIN, [self.a(st["gamewin"])])
        ws(e, G_FPWIN, [self.a(st["fpwin"])])
        ws(e, G_PANEL, [self.a(st["panel"])])
        ws(e, G_MOUSE, [self.a(st["mouse"])])
        ws(e, G_MODE, [self.a(st["mode"])])
        ws(e, G_W1E0, [st["w1e0"]])
        ws(e, G_W1E4, [st["w1e4"]])
        wb(e, G_RMB, st["rmb"])
        ws(e, G_FLAGS, [st["flags"]])
        w16(e, G_PTRX, st["ptrx"])
        w16(e, G_PTRY, st["ptry"])
        w16(e, G_SCRW, st["scrw"])
        w16(e, G_SCRH, st["scrh"])
        ws(e, G_RD, [st["rd"]])
        wb(e, G_RDG, st["rdg"])
        ws(e, G_PT, [st["pt"]])
        wb(e, G_PTG, st["ptg"])
        ws(e, G_CO, [st["co"]])
        wb(e, G_COG, st["cog"])
        w16(e, G_PX, st["px"])
        w16(e, G_PY, st["py"])
        ws(e, G_PIN, [st["pin"]])
        ws(e, G_C34, [st["c34"]])
        ws(e, G_C38, [st["c38"]])
        wb(e, G_CHEATG, st["cheatg"])
        ws(e, G_KEEP, [st["keep"]])
        e.write(G_CHEAT, b"\xa5" * 12)
        for address, label in G_TABLES:
            ws(e, address, [self.a(label)])

    def dump(self):
        e, L = self.e, self.L
        u8 = lambda a: e.read(a, 1)[0]
        s8 = lambda a: T.s8(u8(a)) if hasattr(T, "s8") else (u8(a) - 256 if u8(a) > 127 else u8(a))
        s16 = lambda a: struct.unpack(">h", e.read(a, 2))[0]
        s32 = lambda a: T.s32(e.u32(a))
        out = [L.lab(e.u32(G_GAMEWIN)), L.lab(e.u32(G_FPWIN)), L.lab(e.u32(G_PANEL)), L.lab(e.u32(G_MOUSE)), L.lab(e.u32(G_MODE)),
               s32(G_W1E0), s32(G_W1E4), u8(G_RMB), s32(G_FLAGS), s16(G_PTRX), s16(G_PTRY), s16(G_SCRW), s16(G_SCRH),
               s32(G_RD), s8(G_RDG), s32(G_PT), s8(G_PTG), s32(G_CO), s8(G_COG), s16(G_PX), s16(G_PY), s32(G_PIN), s32(G_C34), s32(G_C38),
               s8(G_CHEATG), s32(G_KEEP)]
        return out + list(e.read(G_CHEAT, 12))

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [x & MASK for x in args])[0]


def host_start(lib, case):
    """Labels the host's test objects like the PowerPC side's and sets the state."""
    CL.host_begin(lib, {label: F(lib, "tpw_gw_obj", C.c_void_p, C.c_int32)(label) for label in LABELS}, case.get("q"))
    st = dict(DEFAULT_STATE, **case.get("st", {}))
    words = (C.c_int32 * NSTATE)(*[T.s32(st[key] & MASK) for key in STATE_KEYS])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    F(lib, "tpw_gw_state", None, C.POINTER(C.c_int32))(words)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1


def host_dump(lib):
    out = (C.c_int32 * (NSTATE + 12))()
    F(lib, "tpw_gw_dump", None, C.POINTER(C.c_int32))(out)
    return list(out)


def ppc_case(address):
    def run(e, case):
        p = Ppc(e, case)
        try:
            r3 = p.call(address, p.a(case["window"]), case["message"], case["a"], case["b"])
            return dict(result=T.s32(r3), state=p.dump(), log=p.L.log)
        finally:
            p.done()
    return run


def host_case(name):
    def run(lib, case):
        host_start(lib, case)
        result = F(lib, name, C.c_int32, *[C.c_int32] * 4)(case["window"], T.s32(case["message"]), T.s32(case["a"]), T.s32(case["b"]))
        log = CL.host_end(lib)
        return dict(result=result, state=host_dump(lib), log=log)
    return run


# ---------------------------------------------------------------- generators

def s16v(r):
    return r.choice([0, 1, -1, 8, 9, -8, -9, 100, -100, 640, 1024, 2047, 2048, 2049, -2048, -2049, 32767, -32768, r.randrange(-32768, 32768)])


def pos(hi, lo):
    """A packed position: x in the high half, y in the low half (both taken mod 65536)."""
    return ((hi & 0xFFFF) << 16 | (lo & 0xFFFF)) & MASK


def rand_word(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(32) - 0x80000000, r.randrange(-1000, 1000)])


def answers(r, kind):
    """A queue of answers for a key table call (0 mostly, so that the tables are walked)."""
    if kind == "search":
        pool = [0x10, 0xF, 0x10010, 0x2000F, 0, 5, 0x1000F, -1, 0x8000, 0x10, 0xF]
    else:
        pool = [0, 0, 0, 1, -1, 0x10, 0x100000, 0x7FFFFFFF]
    return [r.choice(pool) for _ in range(r.randrange(1, 7))]


def random_state(r, **fixed):
    st = dict(
        gamewin=r.choice([1, 1, 0]), fpwin=r.choice([3, 3, 0]), panel=r.choice([0, 0, 2]), mouse=r.choice([0, 8]), mode=r.choice([0, 9]),
        w1e0=r.choice([0, -1, 0x1234, r.getrandbits(32)]), w1e4=r.choice([0, -1, 0x5678, r.getrandbits(32)]), rmb=r.choice([0, 1, 1]),
        flags=r.choice([0, 2, 4, 6, 0x10, 0x16, 0x20, 0x400, 0x402, 0x404, 1, 8, 0xFFFFFFE9, MASK, r.getrandbits(32)]),
        ptrx=s16v(r), ptry=s16v(r), scrw=r.choice([640, 1024, 2048, 0, 1, -640, s16v(r)]), scrh=r.choice([480, 768, 1536, 0, 1, -480, s16v(r)]),
        rd=r.choice([0, 0, 1, 1, 5, -1]), rdg=r.choice([0, 1, 1, 0x80, 0xFF, 5]), pt=r.choice([0, 100, -100, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]),
        ptg=r.choice([0, 1, 1, 0x80, 0xFF]), co=r.choice([0, 1, 1, 7, -1]), cog=r.choice([0, 1, 1, 0xFF, 0x80]), px=s16v(r), py=s16v(r),
        pin=r.choice([0, 1, 1, 5]), c34=r.choice([0x77, 0, r.getrandbits(32)]), c38=r.choice([0x88, 0, r.getrandbits(32)]),
        cheatg=r.choice([0, 1, 1, 0x80, 0xFF]), keep=r.choice([0, 1, 5, -1]))
    st.update(fixed)
    return {k: (v & MASK) if k in ("w1e0", "w1e4", "flags", "pt", "rd", "co", "pin", "c34", "c38", "keep") else v for k, v in st.items()}


def queues(r, st=None, ticks=None):
    ptick = (st or {}).get("pt", 0)
    return {
        "New": [r.choice([0, 1, 1, 1]) for _ in range(r.randrange(1, 6))],
        "GetMode": [r.choice([0, 1, 9, -1, r.getrandbits(32)])],
        "KeyDown": answers(r, "key"), "KeyUp": answers(r, "key"), "Search": answers(r, "search"),
        "CheckShifts": [r.choice([0, 1, 2, 0x8000, 0x12345, -1, 0x10000, r.getrandbits(32)])],
        "IveGotTicks": [ticks if ticks is not None else r.choice([ptick + 199, ptick + 200, ptick + 201, ptick, 0, 0x7FFFFFFF, r.getrandbits(32)]) & MASK],
        "IsCheating": [r.choice([0, 1, 0x100, 0xFF, 2])],
        "ContextMouseClick": [r.choice([0, 1, -1, 0x10000, r.getrandbits(32)])],
        "CallDefaultWindowHandler": [r.choice([0, 1, -1, r.getrandbits(32)])],
    }


WIN_MESSAGES = [0x14, 0x15, 0x1E, 0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x10006, 0x1000A, 0x1000B, 0x13, 0x16, 0x10009, 0x1000C,
                0x10000, 0x100, 0, 0x1A, 0x19, 0x11, 0x1F, 0x10007, 0x10008, 0x1000D, 0x20000A, 0x30000B, 0xFFFF000A, MASK, 0x80000000, 0x7FFFFFFF]
BUTTONS = [0, 1, 2, 3, -1, 0x10, 0x100000001]
KEYS = [0x1B, 0x70, 0x41, 0, 1, 0x10, 0x1A, 0x1C, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x1001B, -1, 0x80000000, 0x7FFFFFFF]


def case_of(r, window, message, a, b, st=None, ticks=None):
    st = st if st is not None else random_state(r)
    return dict(window=window, message=message & MASK, a=a & MASK, b=b & MASK, st=st, q=queues(r, st, ticks))


def win_args(r, message, st):
    """a and b suited to the message (random, with the interesting values weighted)."""
    if message == 0x10003:
        a = pos(st["px"] + r.choice([0, 1, -1, 7, 8, 9, -8, -9, 10, r.randrange(-40, 40)]), st["py"] + r.choice([0, 1, -1, 7, 8, 9, -8, -9, 10, r.randrange(-40, 40)]))
        b = r.getrandbits(32)
    elif message in (0x10004, 0x10005, 0x10006):
        a = r.choice(BUTTONS)
        b = r.choice([pos(s16v(r), s16v(r)), r.getrandbits(32), 0, MASK, 0x80008000, 0x7FFF7FFF])
    elif message in (0x1000A, 0x1000B):
        a = r.choice(KEYS)
        b = r.choice([pos(r.randrange(0, 0x200), s16v(r)), r.getrandbits(32), 0, MASK, 0x00FF0000, 0x01000000, 0x00010000])
    else:
        a = r.choice(BUTTONS + KEYS + [r.getrandbits(32)])
        b = r.choice([r.getrandbits(32), 0, MASK, pos(s16v(r), s16v(r))])
    return a, b


def win_random(r):
    message = r.choice(WIN_MESSAGES + [0x1E, 0x10003, 0x10004, 0x10005, 0x1000A, 0x1000B])
    st = random_state(r)
    a, b = win_args(r, message, st)
    return case_of(r, r.choice([1, 1, 2]), message, a, b, st)


def win_edges():
    r = random.Random(136)
    # every message with every button (rmb off and on), a random state each
    for message in WIN_MESSAGES:
        for rmb in (0, 1):
            for a in BUTTONS:
                st = random_state(r, rmb=rmb)
                yield case_of(r, 1, message, a, win_args(r, message, st)[1], st)
    # the right press: statics, mouse and mode patterns; the allocation pattern of the mouse (new for 1 and 8)
    for mouse in (0, 8):
        for mode in (0, 9):
            for new in ([1, 1], [0], [1, 0], [1, 1, 1]):
                st = random_state(r, rmb=1, mouse=mouse, mode=mode)
                c = case_of(r, 1, 0x10005, 1, pos(-5, 700), st)
                c["q"]["New"] = new
                yield c
    # the release: watched or not, mouse present or not, every allocation pattern
    for rd in (0, 1):
        for mouse in (0, 8):
            for new in ([1, 1, 1], [0], [1, 0], [1, 1, 0], [0, 1], [0, 0], [1, 0, 1]):
                st = random_state(r, rmb=1, mouse=mouse, rd=rd)
                c = case_of(r, 1, 0x10004, 1, pos(10, 20), st)
                c["q"]["New"] = new
                yield c
    for a in (0, 2, 3):
        yield case_of(r, 1, 0x10004, a, pos(3, 4), random_state(r, rmb=1, rd=1))
    # the move test around the 9 limit, each half
    for dx in (-10, -9, -8, 0, 8, 9, 10):
        for dy in (-10, -9, -8, 0, 8, 9, 10):
            st = random_state(r, rmb=1, rd=1, px=r.choice([100, -100, 0, 32767, -32768]), py=r.choice([100, 0, 30000]))
            yield case_of(r, 1, 0x10003, pos(st["px"] + dx, st["py"] + dy), 0, st)
    st = random_state(r, rmb=1, rd=0, px=5, py=5)
    yield case_of(r, 1, 0x10003, pos(500, 500), 0, st)
    # the frame: the press longer than 200 ticks (signed difference), the limit itself
    for pt in (0, 1000, -1000, 0x7FFFFFFF, -0x80000000):
        for delta in (-1, 0, 199, 200, 201, 5000, 0x7FFFFFFF, -0x80000000):
            st = random_state(r, rmb=1, rd=1, pt=pt)
            yield case_of(r, 1, 0x1E, 0, 0, st, ticks=(pt + delta) & MASK)
    for mouse in (0, 8):
        for new in ([1, 1], [0], [1, 0]):
            for panel in (0, 2):
                for pin in (0, 1):
                    for keep in (0, 1):
                        st = random_state(r, mouse=mouse, panel=panel, pin=pin, keep=keep, rmb=r.choice([0, 1]))
                        c = case_of(r, 1, 0x1E, 0, 0, st)
                        c["q"]["New"] = new
                        yield c
    # the scaling: x by the width / 2048 and y by the height / 0x600, negative pointers, halves and sizes
    for sx in (0, 1, 2047, 2048, 2049, -2047, -2049, 640, -640, 32767, -32768):
        for width in (640, 1, -1, 1024, 0, 32767, -32768):
            st = random_state(r, ptrx=sx, scrw=width, ptry=sx, scrh=width, mouse=8)
            yield case_of(r, 1, 0x1E, 0, 0, st)
            yield case_of(r, 1, 0x10004, r.choice([0, 1]), pos(sx, sx), dict(st, rmb=0))
    for hi in (0, 1, -1, 3000, -3000, 32767, -32768, 2047, -2047, 2049, -2049):
        for lo in (0, 1, 0x5FF, 0x600, 0x601, 0xFFFF, 0x8000, 0x7FFF, 12345):
            for height in (480, -480, 1536, 32767, -32768, 1):
                st = random_state(r, mouse=8, scrh=height, scrw=r.choice([640, -640, 2047]), panel=0, rmb=0)
                yield case_of(r, 1, 0x10004, r.choice([0, 1]), pos(hi, lo), st)
    # the button down: panel or not, every button, the click answers
    for panel in (0, 2):
        for a in (0, 1, 2, 3, -1):
            for click in (0, 1, -1, 0x10000):
                st = random_state(r, panel=panel, rmb=r.choice([0, 1]), mouse=8)
                c = case_of(r, 1, 0x10005, a, pos(s16v(r), s16v(r)), st)
                c["q"]["ContextMouseClick"] = [click]
                yield c
    # pointer enter and leave
    for panel in (0, 2):
        for message in (0x10001, 0x10002, 0x15, 0x14):
            yield case_of(r, 1, message, 0, 0, random_state(r, panel=panel))
    # the keys: the tables walked in order, the cheat object
    for message in (0x1000A, 0x1000B):
        for first in range(5):
            for cheatg in (0, 1, 0x80):
                for cheating in (0, 1, 0x100):
                    for key in (0x1B, 0x41, -1, 0x12345):
                        st = random_state(r, cheatg=cheatg)
                        c = case_of(r, 1, message, key, pos(r.choice([0, 1, 0xFF, 0x1FF]), 7), st)
                        names = "KeyDown" if message == 0x1000A else "KeyUp"
                        c["q"][names] = [0] * first + [r.choice([1, -1, 0x10000])] if first < 4 else [0]
                        c["q"]["IsCheating"] = [cheating]
                        yield c


def fp_random(r):
    message = r.choice([0x14, 0x10004, 0x10005, 0x1000A, 0x10006, 0x1000B, 0x100, 0x13, 0x15, 0x10003, 0x1E, 0x10002, 0x1000C, 0x10009, r.getrandbits(32)])
    st = random_state(r)
    a = r.choice([0, 1, 2, 0x1B, 0x70, 0x41, -1, 0x10, 0x1001B, r.getrandbits(32)])
    b = r.choice([pos(r.randrange(0, 0x200), s16v(r)), r.getrandbits(32), 0, MASK, 0x00FF0000, 0x01000000])
    return case_of(r, r.choice([3, 3, 1]), message, a, b, st)


def fp_edges():
    r = random.Random(1360)
    for message in (0x14, 0x10003, 0x10004, 0x10005, 0x10006, 0x10007, 0x10009, 0x1000A, 0x1000B, 0x1000C, 0x100, 0x101, 0xFF, 0x13, 0x15, 0x1E, 0, MASK):
        for a in (0, 1, 2, 0x1B, 0x70, -1, 0x10):
            for flags in (0, 2, 4, 6, 0x10, 0x16, 0x14, 0x12, 0x400, 0x402):
                for rmb in (0, 1):
                    st = random_state(r, flags=flags, rmb=rmb)
                    yield case_of(r, 3, message, a, pos(r.randrange(0, 0x200), s16v(r)), st)
    # the key up: the two Search calls, every pair of answers (the answer is taken as a short)
    answers_pool = [0x10, 0xF, 0x10010, 0x2000F, 0, 5, 0x8010, -1, 0x1000F, 0x20010]
    for first in answers_pool:
        for second in answers_pool:
            for a in (0x1B, 0x70, 0x1B + 0x100000000):
                for flags in (0, 0x16, 2, 4):
                    st = random_state(r, flags=flags)
                    c = case_of(r, 3, 0x1000B, a, pos(r.choice([0, 1, 0xFF, 0x1FF]), 3), st)
                    c["q"]["Search"] = [first, second]
                    yield c
    # the key and button messages: the camera table's arguments
    for a in (0, 1, 2, 0x10, 0x11, -1, 0x7FFFFFFF, 0x80000000, 0xFFFF, 0x10000, 0x1000F):
        for message in (0x10004, 0x10005):
            for shifts in (0, 1, 0x8000, 0x12345, -1):
                c = case_of(r, 3, message, a, 0, random_state(r))
                c["q"]["CheckShifts"] = [shifts]
                yield c
    for hi in (0, 1, 0xFF, 0x100, 0x1FF, 0x80):
        yield case_of(r, 3, 0x1000A, 0x70, pos(hi, 0xFFFF) | 0xAB000000, random_state(r))
        yield case_of(r, 3, 0x1000B, 0x70, pos(hi, 0xFFFF) | 0xAB000000, random_state(r))


# ---------------------------------------------------------------- the tests

TESTS = [
    FnTest("Engine_GameWinMsg", ppc_case(WINMSG), host_case("tpw_gw_winmsg"), edge=win_edges, rnd=win_random),
    FnTest("Engine_GameFirstPersonMsg", ppc_case(FPMSG), host_case("tpw_gw_fpmsg"), edge=fp_edges, rnd=fp_random),
]
