"""Unit v5364, the staff status panel's message functions, against libtpw_unit_StaffStatusMsg.dylib (tests/unit_StaffStatusMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_staffStatus.cpp:
  StaffStatusMsg 0x101c4f8c, OneEditMsg 0x101c56bc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the panel's Remove,
SetSelected, UpdateStatusWindow, FireMe, the status panel base's walkers, the window's timers, GetWindow, getExtraParam and default handler,
the staff's queries, the mouse and its modes, operator new, UI_SendMessage and UI_PostMessage, ...) is a fake on both sides that records
its arguments and answers from the case. The three virtual calls (the panel's slot 0x34, the edit panel's slot 0xc and the button window's
Enable through the table at +0x118) go through __ptr_glue on the Mac and through fake function blocks on the host; the window and the
extra-parameter object are identified by whether they are the case's own block.

The world of a case: the panel object at 0x10395b1c (window word, table), the staff pointer 0x103947a4, gMouse and its two words, the
extra parameters of the edit window, the answers of the fakes. What is compared: the answer, the log, the panel's words, the staff
pointer, the mouse words and whether gMouse stayed, was filled or stayed null.

No fused operations. Not tested: a NULL window object from GetWindow or a NULL second extra parameter (the original reads through them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffStatusMsg"
MASK = 0xFFFFFFFF

FN_SS, FN_OE = 0x101C4F8C, 0x101C56BC
ADDR = dict(panel=0x10395B1C, thing=0x103947A4, mouse=0x102DC1F4, w1e0=0x102DC1E0, w1e4=0x102DC1E4, editname=0x10395AD4)
HOST_LAYOUT = [0xC, 0x20]
MOUSE_PRESET = 0x7777
PANEL_LABEL, NAME_LABEL = 1, 10

# per-case state the stubs read (module-level: each module runs in its own process)
NAMES = {}
EXTRA = [0] * 16
WINRET = [0]
NEWS = []     # the sizes the original asked operator new for (the host's operator new is the shim's and records nothing)


def expected_news(case):
    """The sizes the translation must ask for (checked against the original's by the compare): message 0x100, a 0x510, b 1."""
    if case["op"] != "ss" or case["args"][1:] != [0x100, 0x510, 1]:
        return []
    return [0x14] if case["mouse"] != 0 else [0x14, 1, 8]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update({FN_SS, FN_OE})


def mk(name, kinds="", ret=None, result=None):
    """A fake recording `name` with one value per kind: L label, u raw word, i s32, h s16. ret: queue key for r3."""

    def fn(e):
        L = e.cl
        vals = []
        for k, kind in enumerate(kinds):
            r = e.r(3 + k)
            vals.append({"L": lambda: L.lab(r), "u": lambda: r & MASK, "i": lambda: s32(r), "h": lambda: T.s16(r)}[kind]())
        L.rec(name, tuple(vals))
        if result is not None:
            e.set_r(3, result() & MASK)
        elif ret is not None:
            e.set_r(3, L.ret(ret) & MASK)

    return fn


def stub_new(e):
    size = e.r(3)
    NEWS.append(size)
    e.set_r(3, e.alloc(max(size, 64)))


def stub_ptr_glue(e):
    L = e.cl
    name = NAMES.get(e.r(12))
    if name == "slot34":
        L.rec("Slot34", (L.lab(e.r(3)),))
    elif name == "slot0c":
        L.rec("Slot0c", (1 if e.r(3) == EXTRA[1] else 0,))
    elif name == "enable":
        L.rec("Enable", (1 if e.r(3) == WINRET[0] else 0, T.s16(e.r(4))))
    else:
        L.rec("BadGlue", (e.r(12),))


def stub_get_extra(e):
    L = e.cl
    idx = T.s16(e.r(4))
    L.rec("getExtraParam", (e.r(3) & MASK, idx))
    e.set_r(3, EXTRA[idx] if 0 <= idx < 16 else 0)


for addr, fn in [
    (0x101C5668, mk("Remove", "L")),
    (0x101C5358, mk("SetSelected", "L")),
    (0x101C4990, mk("UpdateStatusWindow", "L")),
    (0x101C5400, mk("FireMe", "L")),
    (0x10192C28, mk("GetNextObject", "L")),
    (0x10192DE0, mk("GetPreviousObject", "L")),
    (0x101945C4, lambda e: e.cl.rec("EditNamePanelGone", (1 if e.r(3) == EXTRA[1] else 0,))),
    (0x1018227C, mk("CancelAdvisorMessage", "i")),
    (0x101821CC, mk("AdvisorMessage", "i")),
    (0x101817E4, mk("UpdateSpritePreview", "")),
    (0x10182304, mk("EnableAllKeyTables", "")),
    (0x1018116C, mk("DecideKbCapture", "")),
    (0x101DD5E0, mk("setAnimTimer", "uii", ret="setAnimTimer")),
    (0x101DD6B8, mk("startAnimTimer", "ui")),
    (0x101DBE40, mk("GetWindow", "ui", result=lambda: WINRET[0])),
    (0x101DD030, stub_get_extra),
    (0x101DE318, mk("CallDefaultWindowHandler", "uiii", ret="CallDefault")),
    (0x101C9518, mk("UI_SendMessage", "uiii")),
    (0x101C9400, mk("UI_PostMessage", "uiii")),
    (0x10193C78, mk("DefaultStatusPanelMsg", "uiii", ret="Default")),
    (0x10125AEC, mk("CanBePickedUp", "u", ret="CanBePickedUp")),
    (0x10126718, mk("GetStaffType", "u", ret="GetStaffType")),
    (0x10125B98, mk("PickMeUp", "u")),
    (0x10127A24, mk("CancelPatrolArea", "u")),
    (0x10126F30, mk("SetName", "uL")),
    (0x10199630, mk("CreateAllStaffPanel", "iu")),
    (0x101C0690, mk("ShowStaffPanel", "u")),
    (0x1007E2E0, mk("AssignMouseMode", "LL")),
    (0x1007E6E4, CL.recorder("CMMPathDraw", "Ni")),
    (0x1007EA78, CL.recorder("CMMDefault", "N")),
    (0x102384B8, stub_new),
    (0x1023BAA8, stub_ptr_glue),
]:
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["panel"]: PANEL_LABEL, ADDR["editname"]: NAME_LABEL}, case["queues"])
        NAMES.clear()
        del NEWS[:]
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

    def off(self, a):
        return a

    def ptrw(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def fnptr(self, a, name):
        self.words(a, [0x10000000, 0])
        NAMES[a] = name

    def make_window(self, win):
        table, vec = self.alloc(0x40), self.alloc(16)
        self.fnptr(vec, "enable")
        self.words(table + 0x1C, [vec])
        self.words(win + 0x118, [table])
        WINRET[0] = win

    def set_extras(self, values):
        for i in range(16):
            EXTRA[i] = values.get(i, 0)

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {PANEL_LABEL: self.g("panel"), NAME_LABEL: self.g("editname")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def off(self, a):
        return self.lib.tpw_sm_offset(a)

    def ptrw(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0] & MASK

    def fnptr(self, a, name):
        self.write(a, struct.pack("<Q", self.g("fn_" + name)))

    def make_window(self, win):
        self.write(win, struct.pack("<Q", self.g("vt")))
        self.ptrw(self.g("winret"), win)

    def set_extras(self, values):
        base = self.g("extra")
        for i in range(16):
            self.ptrw(base + 8 * i, values.get(i, 0))

    def call(self, entry, args):
        fn = self.lib.tpw_sm_StaffStatusMsg if entry == FN_SS else self.lib.tpw_sm_OneEditMsg
        return fn(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sm_addr.restype, lib.tpw_sm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sm_alloc.restype, lib.tpw_sm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sm_offset.restype, lib.tpw_sm_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sm_layout.argtypes = [C.POINTER(C.c_int64)]
    for name in ("tpw_sm_StaffStatusMsg", "tpw_sm_OneEditMsg"):
        f = getattr(lib, name)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    panel = s.g("panel")
    table, vec34 = s.alloc(0x40), s.alloc(16)
    s.fnptr(vec34, "slot34")
    s.words(table + 0x34, [s.off(vec34)])
    s.words(panel, [0, 0, 0, case["mwin"], 0, 0, 0, 0, s.off(table)])
    s.ptrw(s.g("thing"), case["thing"])
    s.ptrw(s.g("mouse"), case["mouse"])
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])
    s.make_window(s.alloc(0x200))
    obj, table2, vec0c = s.alloc(0x40), s.alloc(0x40), s.alloc(16)
    s.fnptr(vec0c, "slot0c")
    s.words(table2 + 0xC, [s.off(vec0c)])
    s.words(obj + 0x20, [s.off(table2)])
    s.set_extras({0: case["staff"], 1: obj})


def execute(s, case):
    return s.call(FN_SS if case["op"] == "ss" else FN_OE, case["args"])


def observe(s, case, answer):
    mouse = s.rd_ptr(s.g("mouse"))
    return {
        "answer": answer,
        "log": s.log(),
        "news": list(NEWS) if isinstance(s, Ppc) else expected_news(case),
        "panel": s.rd_words(s.g("panel"), 8),
        "thing": s.rd_ptr(s.g("thing")),
        "w1e0": s.rd_word(s.g("w1e0")),
        "w1e4": s.rd_word(s.g("w1e4")),
        "mouse": 0 if mouse == 0 else (1 if mouse == MOUSE_PRESET else 2),
    }


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

WORDS = [0, 1, -1, 5, 0x7FFFFFFF, -0x80000000, 0x80080]


def base(r, op, msg, a, b, variant=None):
    v = r.randrange(2) if variant is None else variant
    return {
        "op": op,
        "args": [0x6000, msg, a, b],
        "mwin": 0x5000,
        "thing": [0, 0x4321][v],
        "mouse": [MOUSE_PRESET, 0][v],
        "staff": [0x2222, 0][r.randrange(2)] if op == "oe" else 0,
        "w1e0": r.choice(WORDS) & MASK,
        "w1e4": r.choice(WORDS) & MASK,
        "queues": {
            "setAnimTimer": [[1, 0][v] if r.random() < 0.8 else r.choice([-1, 5, 0x10000])],
            "CanBePickedUp": [[1, 0][v]],
            "GetStaffType": [r.randrange(8)],
            "Default": [r.randrange(-5, 1000)],
            "CallDefault": [r.randrange(-5, 1000)],
        },
    }


SS_MSGS = [0, 5, 0xF, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x1D, 0x1E, 0x1F, 0x80, 0xFF, 0x100, 0x101, 0x802, -1]
A100 = [-2, -1, 0, 0x50A, 0x50B, 0x50C, 0x50D, 0x50E, 0x50F, 0x510, 0x511, 0x520, 0x521, 0x522, 0x7FFFFFFF]
A10 = [0x80080, 0x80000, 0x80081, 0x7FF80, 0x80100, 0]


def ss_triples():
    for msg in SS_MSGS:
        if msg == 0x100:
            for a in A100:
                for b in (0, 1, 2):
                    yield msg, a, b
        elif msg == 0x10:
            for a in A10:
                yield msg, a, 0
        else:
            yield msg, 0, 0
            yield msg, 0x50E, 1


def ss_edges():
    r = random.Random(5364)
    for msg, a, b in ss_triples():
        for v in (0, 1):
            yield base(r, "ss", msg, a, b, v)


def ss_random(r):
    msg = r.choice(SS_MSGS + [0x100, 0x100, 0x1E, 0x15, 0x10, r.randrange(-3, 0x900)])
    a = r.choice(A100 + A10 + [r.randrange(-0x600, 0x600)])
    b = r.choice([0, 1, 2, -1, r.randrange(-100, 100)])
    return base(r, "ss", msg, a, b)


OE_MSGS = [0x802, 0x100, 0x14, 0x804, 0x801, 0x803, 0x805, 0, 0x13, 0x15, 0xFF, 0x101, 1, -1]


def oe_edges():
    r = random.Random(5365)
    for msg in OE_MSGS:
        for a in ((-1, -2, 0, 1, -3, 0x100) if msg == 0x100 else (0, -1)):
            for v in (0, 1):
                yield base(r, "oe", msg, a, r.choice([0, 7]), v)


def oe_random(r):
    msg = r.choice(OE_MSGS + [0x100, 0x100, 0x14, r.randrange(-3, 0x900)])
    return base(r, "oe", msg, r.choice([-1, -2, 0, 1, -3, r.randrange(-50, 50)]), r.choice([0, 1, -1, 99]))


TESTS = [
    FnTest("StaffStatusMsg", ppc, host, edge=ss_edges, rnd=ss_random),
    FnTest("OneEditMsg", ppc, host, edge=oe_edges, rnd=oe_random),
]
