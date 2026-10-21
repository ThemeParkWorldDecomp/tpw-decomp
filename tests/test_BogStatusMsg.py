"""Unit v5699: the bog status panel's window handler, its static constructor, the coaster place mouse mode and the coaster panel's check
buttons, against libtpw_unit_BogStatusMsg.dylib (tests/unit_BogStatusMsg.cpp), main app ("SimTheme Park"),
src/game/ui/ui_bogStatus.cpp, src/game/ui/ui_coasterPanel.cpp:
  BogStatusMsg 0x1019a9b0, __sinit_ui_bogStatus_cpp 0x1019ad70, CoasterPlaceMouseMode 0x1019b018, WhichCheckButtonsAreDown 0x1019b120

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the panel's members,
the timers, the mouse, the allocation, the check buttons and the cursor) is a fake on both sides that records its arguments and answers
from the case's queues.

The world of a case: the panel block (0x24 bytes) filled with random words, the staff room thing global (NULL or an object), the mouse
global (NULL or an object) and its two words, the coaster panel global. What is compared: the answer, the log, the panel words, the
mouse words and what the mouse global holds (nothing, the old object, a new one).

No fused operations. Not tested: nothing is left out; a NULL buttons window is a case (the fakes ignore it, as the original passes it on).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BogStatusMsg"
MASK = 0xFFFFFFFF

FN_MSG, FN_SINIT, FN_PLACE, FN_WHICH = 0x1019A9B0, 0x1019AD70, 0x1019B018, 0x1019B120
SIZES = {FN_MSG: 628, FN_SINIT: 76, FN_PLACE: 220, FN_WHICH: 360}
OWN = set(SIZES)

ADDR = dict(panel=0x10394800, thing=0x103947A4, coasterpanel=0x10394834, mouse=0x102DC1F4, w1e0=0x102DC1E0, w1e4=0x102DC1E4)
HOST_LAYOUT = [0, 0x20]
NWORDS = 10          # the panel words compared (0x28 bytes)

STUBS = CL.Stubs()
STUBS.real.update(OWN)

BUTTONS = [0]        # the PowerPC address of this case's buttons window (the GetWindow fake answers it)


def rec(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret))


rec(0x10181570, "TickRidePreview", "")
rec(0x1018227C, "CancelAdvisorMessage", "i")
rec(0x101DD5E0, "setAnimTimer", "Lii", ret="h")
rec(0x101DD6B8, "startAnimTimer", "Li")
rec(0x1019A504, "UpdateStatusWindow", "L")
rec(0x1019AD1C, "Remove", "L")
rec(0x10192C28, "GetNextObject", "L")
rec(0x10192DE0, "GetPreviousObject", "L")
rec(0x10193010, "CloseObject", "Li")
rec(0x101932A0, "RequestMaintenance", "Lb")
rec(0x10193138, "EraseObject", "L")
rec(0x101938D8, "Move", "L")
rec(0x101983C4, "CreateAllItemsPanel", "iL")
rec(0x10193C78, "DefaultStatusPanelMsg", "Liii", ret="i")
rec(0x101929E8, "UIStatusPanel", "L")
rec(0x10088590, "LLCM_SetCurrentObject", "b")
rec(0x1007E6E4, "CMMPathDraw", "Ni")
rec(0x1007EA78, "CMMDefault", "N")
rec(0x1007E2E0, "AssignMouseMode", "LL")
rec(0x10091A5C, "LLCC_SystemSetRideTrack", "i")
rec(0x101DBE40, "GetWindow", "Li", ret=lambda e, log: BUTTONS[0] if log.ret("GetWindow") else 0)
rec(0x101CC148, "SetCurrent", "Li")
rec(0x101A8C58, "UI_SetGameWindowCursor", "h")
rec(0x102384B8, "new", "u", ret=lambda e, log: e.alloc(max(e.r(3), 64)))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def put_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def get_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def objects(self):
        self.win, self.thing, self.cpanel, self.buttons, self.mouse = [self.alloc(0x400) for _ in range(5)]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objects()
        BUTTONS[0] = self.buttons
        fixed = {ADDR["panel"]: 1, self.win: 2, self.thing: 3, self.cpanel: 4, self.buttons: 5, self.mouse: 6}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def sinit_table(self, word):
        return word

    def call(self, op, args):
        entry = {"msg": FN_MSG, "sinit": FN_SINIT, "place": FN_PLACE, "which": FN_WHICH}[op]
        if op == "msg":
            args = [self.win] + list(args)
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if op == "msg" else 0      # the others answer nothing


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objects()
        lib.tpw_bog_set_buttons(self.buttons)
        CL.host_begin(lib, {1: self.g("panel"), 2: self.win, 3: self.thing, 4: self.cpanel, 5: self.buttons, 6: self.mouse},
                      case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bog_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bog_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def sinit_table(self, word):
        # the static initialiser's table is the arena offset of BogStatusPanelTable() (unit v5L31): the Mac's address when it is that
        return 0x102A9384 if word == self.lib.tpw_bog_sinit_table() else word

    def call(self, op, args):
        if op == "msg":
            return self.lib.tpw_bog_msg(self.win, *[s32(a) for a in args]) & MASK
        if op == "sinit":
            self.lib.tpw_bog_sinit()
        elif op == "place":
            self.lib.tpw_bog_place()
        else:
            self.lib.tpw_bog_which(args[0] & MASK)
        return 0


def s32(v):
    return T.s32(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bog_addr.restype, lib.tpw_bog_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bog_alloc.restype, lib.tpw_bog_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bog_set_buttons.restype, lib.tpw_bog_set_buttons.argtypes = None, [C.c_uint64]
    lib.tpw_bog_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bog_msg.restype, lib.tpw_bog_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_bog_sinit.restype, lib.tpw_bog_sinit.argtypes = None, []
    lib.tpw_bog_sinit_table.restype, lib.tpw_bog_sinit_table.argtypes = C.c_uint32, []
    lib.tpw_bog_place.restype, lib.tpw_bog_place.argtypes = None, []
    lib.tpw_bog_which.restype, lib.tpw_bog_which.argtypes = None, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bog_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("panel"), case["panel"])
    s.put_ptr(s.g("thing"), s.thing if case["thing"] else 0)
    s.put_ptr(s.g("coasterpanel"), s.cpanel)
    s.put_ptr(s.g("mouse"), s.mouse if case["mouse"] else 0)
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])


def observe(s, case, answer):
    m = s.get_ptr(s.g("mouse"))
    kind = 0 if m == 0 else (1 if m == s.mouse else 2)
    panel = s.rd_words(s.g("panel"), NWORDS)
    panel[8] = s.sinit_table(panel[8])
    return {"answer": answer, "log": s.log(), "panel": panel, "mouse": kind,
            "w1e0": s.rd_word(s.g("w1e0")), "w1e4": s.rd_word(s.g("w1e4"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

MSGS = [0, 1, 4, 5, 6, 0x0F, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x1D, 0x1E, 0x1F, 0xFF, 0x100, 0x101, -1, 0x7FFFFFFF, -0x80000000]
SUBS = [-1, 0, 1, -2, 0x7FFFFFFF, -0x80000000, 0x80080, 0x80081, 0x8007F, 0x180080, 0x100080] + list(range(0x15BB2, 0x15BC8))


def base(r, op, args):
    return {"op": op, "args": args,
            "queues": {"setAnimTimer": [r.choice([0, 1, 0x10000, -1, 5])],
                       "DefaultStatusPanelMsg": [r.choice([0, 1, -1, 77, r.randrange(-2**31, 2**31)])],
                       "GetWindow": [r.choice([0, 1])]},
            "panel": [r.getrandbits(32) for _ in range(NWORDS)], "thing": r.choice([0, 1]), "mouse": r.choice([0, 1]),
            "w1e0": r.getrandbits(32), "w1e4": r.getrandbits(32)}


def msg_edges():
    r = random.Random(5699)
    for m in MSGS:
        yield base(r, "msg", [m, r.choice([0, 0x80080]), r.choice([0, 1])])
    for a in (0x80080, 0x80081, 0x8007F, 0, -1, 0x180080, 0x100080):
        for b in (0, 1):
            yield base(r, "msg", [0x10, a, b])
    for m in (0x14, 0x15):
        for timer in (0, 1, 0x10000, -1):
            c = base(r, "msg", [m, 0, 0])
            c["queues"]["setAnimTimer"] = [timer]
            yield c
    for a in SUBS:
        for b in (0, 1, 2):
            for th in (0, 1):
                c = base(r, "msg", [0x100, a, b])
                c["thing"] = th
                yield c


def msg_random(r):
    m = r.choice(MSGS + [r.randrange(-300, 600)])
    a = r.choice(SUBS + [r.randrange(-2**31, 2**31), r.randrange(0x15B00, 0x15C00)])
    return base(r, "msg", [m, a, r.choice([0, 1, 2, r.randrange(-5, 5)])])


def sinit_edges():
    r = random.Random(5700)
    for _ in range(4):
        yield base(r, "sinit", [])


def sinit_random(r):
    return base(r, "sinit", [])


def place_edges():
    r = random.Random(5701)
    for pre in (0, 1):
        c = base(r, "place", [])
        c["mouse"] = pre
        yield c


def place_random(r):
    return base(r, "place", [])


def which_edges():
    r = random.Random(5702)
    for mask in [0] + [1 << i for i in range(10)] + [0x1FF, 0xFFFFFFFF, 0x200, 0x80000000, 0x3F, 0x1F8, 0x1FC, 0x1FE, 0x180, 0x100, 0xC0]:
        for g in (0, 1):
            c = base(r, "which", [mask])
            c["queues"]["GetWindow"] = [g]
            yield c


def which_random(r):
    return base(r, "which", [r.getrandbits(10) if r.random() < 0.8 else r.getrandbits(32)])


TESTS = [
    FnTest("BogStatusMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("__sinit_ui_bogStatus_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("CoasterPlaceMouseMode", ppc, host, edge=place_edges, rnd=place_random),
    FnTest("WhichCheckButtonsAreDown", ppc, host, edge=which_edges, rnd=which_random),
]
