"""Unit v51122: the keyboard action wrappers (context_help, show_version, ToggleHelpText, CamcorderMode, Screen_*), the Remove*Panel
functions, MainPanel_EnableResearchButton, GameWindowToggle_Hidden and __sinit_ui_mainpanel2_cpp, against
libtpw_unit_context_help.dylib (tests/unit_context_help.cpp), main app ("SimTheme Park"),
src/game/keyboard.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_mainpanel2.cpp, src/game/ui/ui_optionsPanel.cpp, src/game/ui/ui_parkStatus.cpp.

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory for the original, the host library for
the translation). The callees outside the unit (UIInterface members, GetAdvisor, PlayLastTutorialMessage, UI_SendMessage, the wide string
constructor, __register_global_object) are fakes on both sides that record their arguments; GetAdvisor answers from the case.

The world of a case: gGameWorld (NULL or an object), the advisor GetAdvisor answers (NULL or an object), the four panel pointers
(NULL or a window), gGameWindowToggled. What is compared: the answer, the event log, the globals read back. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_context_help"
MASK = 0xFFFFFFFF

NAMES = ["context_help", "show_version", "ToggleHelpText", "CamcorderMode", "Screen_Research", "Screen_PeepLoc", "Screen_StaffLoc",
         "Screen_Map", "Screen_EntryPrice", "Screen_StaffCosts", "Screen_Loans", "Screen_Finance", "Screen_AllPeeps",
         "Screen_AllStaff", "Screen_AllItems", "Screen_ParkStatus", "Screen_BuyStaff", "RemoveLoansPanel",
         "MainPanel_EnableResearchButton", "GameWindowToggle_Hidden", "__sinit_ui_mainpanel2_cpp", "RemoveOptionsPanel",
         "RemoveParkStatusPanel", "RemoveAddressBook"]
FN = [0x1014f340, 0x1014f39c, 0x1014ebf4, 0x1014eba4, 0x1014eb50, 0x1014eafc, 0x1014eaa8, 0x1014ea58, 0x1014ea04, 0x1014e9b0,
      0x1014e960, 0x1014e90c, 0x1014e8b8, 0x1014e864, 0x1014e810, 0x1014e7bc, 0x1014e768, 0x101a571c, 0x101a7830, 0x101a8c1c,
      0x101a8df4, 0x101ab904, 0x101ada00, 0x101b0a84]
SIZES = dict(zip(FN, [56, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 56, 16, 12, 64, 56, 56, 56]))

# the UIInterface members the wrappers call (op -> callee address, event name)
UI_CALLEES = {1: (0x101f1270, "ShowVersionTag"), 2: (0x101f0908, "ToggleHelpText"), 3: (0x101f0ffc, "CamcorderMode"),
              4: (0x101f08b0, "Screen_Research"), 5: (0x101f0834, "Screen_PeepLoc"), 6: (0x101f07b4, "Screen_StaffLoc"),
              7: (0x101efbc0, "ShowMap"), 8: (0x101f0758, "Screen_EntryPrice"), 9: (0x101f06fc, "Screen_StaffCosts"),
              10: (0x101f06a8, "Screen_Loans"), 11: (0x101f0650, "Screen_Finance"), 12: (0x101f05f4, "Screen_AllPeeps"),
              13: (0x101f0594, "Screen_AllStaff"), 14: (0x101f0534, "Screen_AllItems"), 15: (0x101f04d8, "Screen_ParkStatus"),
              16: (0x101f047c, "Screen_BuyStaff")}

ADDR = dict(gUIInterface=0x10396440, gGameWorld=0x1037ebfc, gGameWindowToggled=0x10394984, gSetOnOffRoot=0x10394a14,
            gParkStatusPanel=0x10394a8c, gAddressBookPanel=0x103956f8, gLoansPanel=0x10394980, gMainPanel2Text=0x103949e8,
            gMainPanel2TextNode=0x103949fc)
PANELS = {17: "gLoansPanel", 21: "gSetOnOffRoot", 22: "gParkStatusPanel", 23: "gAddressBookPanel"}
PTRS = ["gGameWorld", "gSetOnOffRoot", "gParkStatusPanel", "gAddressBookPanel", "gLoansPanel"]
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(FN)
for _op, (_addr, _name) in UI_CALLEES.items():
    STUBS.internal(_addr, CL.recorder("UIInterface::" + _name, "L"))
STUBS.internal(0x1013eab0, CL.recorder("GetAdvisor", "L", ret="i"))
STUBS.internal(0x1000b1c4, CL.recorder("PlayLastTutorialMessage", "L"))
STUBS.internal(0x101c9518, CL.recorder("UI_SendMessage", "Liii"))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", CL.recorder("WideStringCtor", "LS"))
STUBS.imp("__register_global_object", CL.recorder("__register_global_object", "LxL"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world, self.adv, self.win = e.alloc(16), e.alloc(16), e.alloc(16)
        q = dict(case["queues"])
        q["GetAdvisor"] = [self.adv if case["adv"] else 0]
        self.L = e.cl = CL.PpcLog({ADDR["gUIInterface"]: 1, self.world: 2, self.adv: 3, self.win: 4, ADDR["gMainPanel2Text"]: 5,
                                   ADDR["gMainPanel2TextNode"]: 6}, q)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, name, v):
        self.words(self.g(name), [v])

    def getptr(self, name):
        return self.rd_word(self.g(name))

    def log(self):
        return self.L.log

    def call(self, op, arg):
        r3, _, _ = T.call_ppc(self.e, FN[op], [arg] if op == 18 else [])
        return r3 & MASK if op not in (17, 18, 20, 21, 22, 23) else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world, self.adv, self.win = (lib.tpw_ch_alloc(16) for _ in range(3))
        q = dict(case["queues"])
        q["GetAdvisor"] = [self.adv if case["adv"] else 0]
        CL.host_begin(lib, {1: self.g("gUIInterface"), 2: self.world, 3: self.adv, 4: self.win, 5: self.g("gMainPanel2Text"),
                            6: self.g("gMainPanel2TextNode")}, q)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ch_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def getptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, arg):
        r = self.lib.tpw_ch_call(op, arg)
        return r if op not in (17, 18, 20, 21, 22, 23) else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ch_addr.restype, lib.tpw_ch_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ch_alloc.restype, lib.tpw_ch_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ch_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ch_call.restype, lib.tpw_ch_call.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ch_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.setptr("gGameWorld", s.world if case["world"] else 0)
    for name in PTRS[1:]:
        s.setptr(name, s.win if case[name] else 0)
    s.words(s.g("gGameWindowToggled"), [case["toggled"]])


def observe(s, case, answer):
    d = {"answer": answer, "log": s.log(), "toggled": s.rd_word(s.g("gGameWindowToggled"))}
    for name in PTRS:
        v = s.getptr(name)
        d[name] = 0 if v == 0 else 1 if v in (s.world, s.win) else 2
    return d


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["arg"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, **kw):
    c = {"op": op, "queues": {}, "adv": r.choice([0, 1]), "world": r.choice([0, 1]), "toggled": r.choice([0, 1, 2, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]),
         "arg": r.choice([0, 1, 2, 0x100]), "gSetOnOffRoot": r.choice([0, 1]), "gParkStatusPanel": r.choice([0, 1]),
         "gAddressBookPanel": r.choice([0, 1]), "gLoansPanel": r.choice([0, 1])}
    c.update(kw)
    return c


def edges_for(op):
    def gen():
        r = random.Random(1000 + op)
        for adv in (0, 1):
            for world in (0, 1):
                for flag in (0, 1):
                    yield mk(r, op, adv=adv, world=world, gSetOnOffRoot=flag, gParkStatusPanel=flag, gAddressBookPanel=flag,
                             gLoansPanel=flag, toggled=flag)
        yield mk(r, op, toggled=0x80000000, arg=0)
        yield mk(r, op, toggled=0xFFFFFFFF, arg=1)
    return gen


def rnd_for(op):
    return lambda r: mk(r, op)


TESTS = [FnTest(name, ppc, host, edge=edges_for(i), rnd=rnd_for(i)) for i, name in enumerate(NAMES)]
