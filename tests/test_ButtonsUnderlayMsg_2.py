"""Unit v51037, nine small functions of the online panels and the UIInterface, against libtpw_unit_ButtonsUnderlayMsg_2.dylib
(tests/unit_ButtonsUnderlayMsg_2.cpp), main app ("SimTheme Park"), src/OnlineParkFindPanelMsg_ContactMenu_ButtonsUnderlayMsg_2.cpp:
  ButtonsUnderlayMsg 0x101e86c4, HideButtonsMenu 0x101e8b18, __sinit_ui_onlineParkInfoPanel_cpp 0x101ebae0,
  RemoveOnlineParkListPanel 0x101ec840, __sinit_ui_onlineParkListPanel_cpp 0x101ec90c, UIInterface::DateChange 0x101efe60,
  UIInterface::Screen_StaffLoc 0x101f07b4, UIInterface::Screen_PeepLoc 0x101f0834, UIInterface::ThingDeath 0x101f0c2c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the window methods, UI_SendMessage, the panel functions, GAME_ASSERTMSG, the constructors of the string, the park and the online
system, StopProcessing, ClearAll, __register_global_object) are fakes on both sides that record their arguments; window pointers are plain
tokens that nothing dereferences.

The world of a case: the menu windows, the online panel, the hidden word, the online system, the lobby screens, the status panels' window
words, the thing array entries and a thing. What is compared: the answer, the pointer globals, the system's state, the event log.

No fused operations. Not tested: ThingDeath with a NULL thing in the slot it reads (the original reads the kind byte at address 2); the
size 0x404 of the online system the original allocates (the host object has its own size); the text of the host's window methods.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ButtonsUnderlayMsg_2"
MASK = 0xFFFFFFFF

F_BUTTONS, F_HIDE, F_SINIT_INFO, F_REMOVE, F_SINIT_LIST = 0x101E86C4, 0x101E8B18, 0x101EBAE0, 0x101EC840, 0x101EC90C
F_DATE, F_STAFF, F_PEEP, F_DEATH = 0x101EFE60, 0x101F07B4, 0x101F0834, 0x101F0C2C
SIZES = {F_BUTTONS: 180, F_HIDE: 88, F_SINIT_INFO: 144, F_REMOVE: 156, F_SINIT_LIST: 116,
         F_DATE: 80, F_STAFF: 76, F_PEEP: 72, F_DEATH: 140}
OWN = set(SIZES)

ADDR = dict(e90=0x10395E90, e8c=0x10395E8C, word88=0x10395E88, panel=0x10395E98, sys=0x10380AB0, lobby=0x102E49B8,
            listpanel=0x10396234, spanel=0x10395B1C, vpanel=0x1037EC14, sthing=0x103947A4, thingarr=0x1034CBFC,
            infoStrA=0x10396208, infoNodeA=0x1039621C, infoStrB=0x103961E8, infoNodeB=0x103961FC, infoNodePark=0x103961D8,
            infoPark=0x10396118, infoParkDtor=0x10267550, listStrA=0x10396258, listNodeA=0x1039626C, listStrB=0x10396238,
            listNodeB=0x1039624C, dtorglue=0x103BCBB8)
# CThingArrayEntry's size, CLBScreenSystem::mAdvisor's offset and the class size on the host
HOST_LAYOUT = [48, 40, 56]
MAC_STRIDE = 0x14
MAC_ADVISOR = 0x14

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def rec(name, spec, addr, ret=None, glue=False):
    fake = CL.recorder(name, spec, ret)
    if glue:
        def fake_glue(e, fake=fake):
            # the glue code saves the caller's TOC pointer at 0x14(r1) and the caller reloads it after the call
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
            return fake(e)
        STUBS.internal(addr, fake_glue)
    else:
        STUBS.internal(addr, fake)


rec("Show", "uh", 0x101DC1EC)
rec("unlink", "u", 0x101DE084)
rec("CaptureKb", "ui", 0x101DCCEC)
rec("CDWH", "uiii", 0x101DE318, "i")
rec("UI_SendMessage", "uiii", 0x101C9518)
rec("ShowStaffPanel", "u", 0x101C0690)
rec("ShowKidLocatorPanel", "", 0x101C0A38)
rec("AllKidsPanel_KidGone", "L", 0x10195708)
rec("LocatorByeByeKid", "L", 0x101BF98C)
rec("KidGone", "LL", 0x101434A0)
rec("UI_MainPanelDateChange", "iii", 0x101A6234)
rec("GAME_ASSERTMSG", "is", 0x1000DDE8)
rec("CFSOnlineSystem", "N", 0x1016F7D0)
rec("StopProcessing", "L", 0x10171404)
rec("ClearAll", "u", 0x100A7128)
rec("__register_global_object", "LLL", 0x10239940, glue=True)
rec("TbDynamicString", "LS", 0x10238800, glue=True)
rec("CFSOnlinePark", "L", 0x1017BF30)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def labels(self):
        """address (Mac) or name -> label number."""
        n = {"vpanel": 1, "infoStrA": 2, "infoNodeA": 3, "infoStrB": 4, "infoNodeB": 5, "infoPark": 6, "infoNodePark": 7,
             "infoParkDtor": 8, "dtorglue": 9, "listStrA": 11, "listNodeA": 12, "listStrB": 13, "listNodeB": 14}
        return n


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    stride = MAC_STRIDE

    def __init__(self, e, case):
        self.e = e
        self.sysobj = e.alloc(0x40)
        self.thing = e.alloc(16)
        fixed = {ADDR[k]: v for k, v in self.labels().items()}
        fixed[self.sysobj] = 10
        fixed[self.thing] = 20
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

    def adv_off(self):
        return MAC_ADVISOR

    def lobby_size(self):
        return 0x20

    def call(self, addr, args):
        r3 = T.call_ppc(self.e, addr, [a & MASK for a in args])[0]
        return s32(r3) if addr == F_BUTTONS else 0     # the others answer nothing


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = lib.stride
        self.sysobj = self.alloc(0x40)
        self.thing = self.alloc(16)
        fixed = {v: self.g(k) for k, v in self.labels().items()}
        fixed[10] = self.sysobj
        fixed[20] = self.thing
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bum_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bum_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def adv_off(self):
        return self.lib.adv_off

    def lobby_size(self):
        return self.lib.lobby_size

    def call(self, addr, args):
        lib = self.lib
        if addr == F_BUTTONS:
            return lib.tpw_bum_buttons(args[0] & MASK, s32(args[1]), s32(args[2]), s32(args[3]))
        if addr == F_HIDE:
            lib.tpw_bum_hide()
        elif addr == F_SINIT_INFO:
            lib.tpw_bum_sinit_info()
        elif addr == F_REMOVE:
            lib.tpw_bum_remove()
        elif addr == F_SINIT_LIST:
            lib.tpw_bum_sinit_list()
        elif addr == F_DATE:
            lib.tpw_bum_date((C.c_int32 * 9)(*[s32(w) for w in args[1:10]]))
        elif addr == F_STAFF:
            lib.tpw_bum_staff()
        elif addr == F_PEEP:
            lib.tpw_bum_peep()
        elif addr == F_DEATH:
            lib.tpw_bum_death((args[1] >> 16) & 0xFFFF)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bum_"
    lib.tpw_bum_addr.restype, lib.tpw_bum_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bum_alloc.restype, lib.tpw_bum_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bum_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bum_buttons.restype, lib.tpw_bum_buttons.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    for n in ("hide", "sinit_info", "remove", "sinit_list", "staff", "peep"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = None, []
    lib.tpw_bum_date.restype, lib.tpw_bum_date.argtypes = None, [C.POINTER(C.c_int32)]
    lib.tpw_bum_death.restype, lib.tpw_bum_death.argtypes = None, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bum_layout(out)
    lay = list(out)[:n]
    assert lay == HOST_LAYOUT, lay
    lib.stride, lib.adv_off, lib.lobby_size = lay
    lib.bound = True


# ------------------------------------------------------------------------------------------------------------------ the world

def build(s, case):
    s.wptr(s.g("e90"), case["e90"])
    s.wptr(s.g("e8c"), case["e8c"])
    s.words(s.g("word88"), [case["word88"]])
    s.wptr(s.g("panel"), case["panel"])
    s.wptr(s.g("sthing"), case["sthing"])
    s.wptr(s.g("listpanel"), case["listpanel"])
    s.words(s.g("spanel") + 0xC, [case["spanel"]])
    s.words(s.g("vpanel") + 0xC, [case["vpanel"]])
    s.wptr(s.g("sys"), s.sysobj if case["has_sys"] else 0)
    lobby = s.alloc(s.lobby_size())
    s.wptr(lobby + s.adv_off(), case["adv"])
    s.wptr(s.g("lobby"), lobby)
    # the thing array: entry 0 and entry idx
    s.write(s.thing + 2, bytes([case["kind"]]))
    base = s.g("thingarr")
    s.wptr(base, s.thing if case["entry0"] else 0)
    s.wptr(base + case["idx"] * s.stride, s.thing)


def execute(s, case):
    return s.call(case["fn"], case["args"])


def sys_state(s):
    v = s.rptr(s.g("sys"))
    return 0 if v == 0 else (1 if v == s.sysobj else 2)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "e90": s.rptr(s.g("e90")), "e8c": s.rptr(s.g("e8c")), "word88": s.rd_word(s.g("word88")),
            "panel": s.rptr(s.g("panel")), "sthing": s.rptr(s.g("sthing")), "listpanel": s.rptr(s.g("listpanel")),
            "spanel": s.rd_word(s.g("spanel") + 0xC), "vpanel": s.rd_word(s.g("vpanel") + 0xC), "sys": sys_state(s)}


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


# ------------------------------------------------------------------------------------------------------------------ the cases

TOKENS = [0, 0x11000, 0x22000, 0x33000, 0x44000]


def base(r, fn, args):
    return {"fn": fn, "args": args, "queues": {"CDWH": [r.choice([0, 1, -1, 12345, r.randrange(-2 ** 31, 2 ** 31)])]},
            "e90": r.choice(TOKENS), "e8c": r.choice(TOKENS), "word88": r.choice(TOKENS), "panel": r.choice(TOKENS),
            "sthing": r.choice(TOKENS), "listpanel": r.choice(TOKENS), "spanel": r.choice([0, 1, 0x80000000, r.randrange(1, 2 ** 32)]),
            "vpanel": r.choice([0, 1, 0x80000000, r.randrange(1, 2 ** 32)]), "has_sys": r.choice([0, 1]),
            "adv": r.choice(TOKENS[1:]), "kind": 1, "entry0": 0, "idx": 1}


MESSAGES = [0x10006, 0x14, 0x13, 0x15, 0x10005, 0x10007, 0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x20006, 0x10000, 0x6]


def buttons_case(r, window=None, message=None):
    c = base(r, F_BUTTONS, [0, 0, 0, 0])
    c["e90"] = r.choice(TOKENS)
    if window is None:
        window = r.choice([c["e90"], c["e8c"], r.choice(TOKENS)])
    c["args"] = [window, r.choice(MESSAGES) if message is None else message, r.choice([0, 1, -1, r.randrange(-2 ** 31, 2 ** 31)]),
                 r.choice([0, 7, r.randrange(-2 ** 31, 2 ** 31)])]
    return c


def buttons_edges():
    r = random.Random(101)
    for m in MESSAGES:
        for mode in range(5):
            c = base(r, F_BUTTONS, [0, 0, 0, 0])
            c["e90"], c["e8c"] = 0x11000, 0x22000
            w = {0: 0x11000, 1: 0x22000, 2: 0x33000, 3: 0, 4: 0x11000}[mode]
            if mode == 4:
                c["e8c"] = 0x11000
            if mode == 3:
                c["e90"] = 0
            c["args"] = [w, m, 5, -6]
            yield c


def buttons_random(r):
    return buttons_case(r)


def plain_case(fn, args=None):
    def gen(r):
        return base(r, fn, args(r) if args else [0])
    return gen


def hide_edges():
    r = random.Random(102)
    for w in TOKENS:
        for p in TOKENS[:3]:
            c = base(r, F_HIDE, [0])
            c["word88"], c["panel"] = w, p
            yield c


def one_edge(fn, seed, n=2):
    def gen():
        r = random.Random(seed)
        for _ in range(n):
            yield base(r, fn, [0])
    return gen


def remove_edges():
    r = random.Random(103)
    for has in (0, 1, 0, 1):
        c = base(r, F_REMOVE, [0])
        c["has_sys"] = has
        yield c


def date_args(r):
    return [0x1000] + [r.choice([0, 1, -1, 11, 12, 0x7FFFFFFF, -0x80000000, r.randrange(-2 ** 31, 2 ** 31)]) & MASK for _ in range(9)]


def date_edges():
    r = random.Random(104)
    for mon in (0, 11, 0x7FFFFFFF, -1):
        c = base(r, F_DATE, [0x1000, 1, 2, 3, 4, mon & MASK, 6, 7, 8, 9])
        yield c
    for i in range(9):
        a = [0x1000] + [0x100 * (j + 1) + 1 for j in range(9)]
        a[1 + i] = 0xDEADBEEF
        yield base(r, F_DATE, a)


def date_random(r):
    return base(r, F_DATE, date_args(r))


def staff_edges():
    r = random.Random(105)
    for sp in (0, 1, 0x80000000, 0xFFFFFFFF):
        for st in (0, 0x11000):
            c = base(r, F_STAFF, [0])
            c["spanel"], c["sthing"] = sp, st
            yield c


def peep_edges():
    r = random.Random(106)
    for vp in (0, 1, 0x80000000, 0xFFFFFFFF):
        for st in (0, 0x11000):
            c = base(r, F_PEEP, [0])
            c["vpanel"], c["sthing"] = vp, st
            yield c


def death_case(r, idx=None, kind=None, entry0=None):
    c = base(r, F_DEATH, [0, 0])
    idx = r.choice([0, 1, 2, 5, 0x100, 0x27FF, r.randrange(0, 0x2800)]) if idx is None else idx
    c["idx"] = idx
    c["entry0"] = (1 if idx == 0 else r.choice([0, 1])) if entry0 is None else entry0
    c["kind"] = r.choice([0, 1, 1, 2, 3, 4, 0x13, 0xFF]) if kind is None else kind
    c["args"] = [0x1000, idx << 16]
    return c


def death_edges():
    r = random.Random(107)
    for idx in (0, 1, 0x27FF):
        for kind in (1, 0, 2, 255):
            for e0 in (0, 1):
                if idx == 0 and e0 == 0:
                    continue
                yield death_case(r, idx, kind, e0)


def probe(e, case):
    for name, a, n in (("info A", 0x1024F08A + 0x83, 16), ("info B", 0x1024F08A + 0x91, 16), ("list A", 0x1024F170 + 0x37, 16),
                       ("list B", 0x1024F170 + 0x45, 16)):
        print(name, e.read(a, n).decode("utf-16-be", "replace"))
    return {}


TESTS = [
    FnTest("ButtonsUnderlayMsg", ppc, host, edge=buttons_edges, rnd=buttons_random),
    FnTest("HideButtonsMenu", ppc, host, edge=hide_edges, rnd=plain_case(F_HIDE)),
    FnTest("__sinit_ui_onlineParkInfoPanel_cpp", ppc, host, edge=one_edge(F_SINIT_INFO, 108), rnd=plain_case(F_SINIT_INFO)),
    FnTest("RemoveOnlineParkListPanel", ppc, host, edge=remove_edges, rnd=plain_case(F_REMOVE)),
    FnTest("__sinit_ui_onlineParkListPanel_cpp", ppc, host, edge=one_edge(F_SINIT_LIST, 109), rnd=plain_case(F_SINIT_LIST)),
    FnTest("UIInterface::DateChange", ppc, host, edge=date_edges, rnd=date_random),
    FnTest("UIInterface::Screen_StaffLoc", ppc, host, edge=staff_edges, rnd=plain_case(F_STAFF)),
    FnTest("UIInterface::Screen_PeepLoc", ppc, host, edge=peep_edges, rnd=plain_case(F_PEEP)),
    FnTest("UIInterface::ThingDeath", ppc, host, edge=death_edges, rnd=death_case),
]
