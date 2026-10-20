"""Unit v51061, the UIInterface stand-ins (thin calls into the UI), against libtpw_unit_UIInterface_PreLevelLoad.dylib
(tests/unit_UIInterface_PreLevelLoad.cpp), main app, src/OnlineParkFindPanelMsg_ContactMenu_ButtonsUnderlayMsg_2_UIInterface_PreLevelLoad.cpp:
  PreLevelLoad, GetShiftKeyState, SetSelectedObject, GetSelectedObject, GetSelectedPeep, Load, MessageTag, Save, RemoveLocators,
  StaffDBChange, ShowGameMainMenu, RideCondemned, Screen_BuyObject, Screen_BuyStaff, RideBrokenDown, Screen_ParkStatus, RideDeleted,
  Screen_AllItems, ResearchCategoryComplete, Screen_AllStaff, ShowMap, Screen_AllPeeps, Screen_Finance, Screen_Loans, Screen_StaffCosts

A case is a call. Every callee is a fake on both sides recording its name and the arguments it reads (the tag parameters block by its id word,
the panels by label); the fakes answer from the case's queues. What is compared: the answer (where the function has one) and the event log.
No fused operations. Not tested: nothing skipped. The tag index of the ride arguments is passed in the high half of r4 (a two byte
aggregate), and the callee's argument is compared as that high half.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIInterface_PreLevelLoad"
MASK = 0xFFFFFFFF

# name: (address, size, host key, arg kind, has answer, callees used)
FUNCS = {
    "UIInterface::PreLevelLoad": (0x101f00c0, 52, "pre", "", False, ["GWTH"]),
    "UIInterface::GetShiftKeyState": (0x101f0068, 36, "shift", "", True, ["SHIFT"]),
    "UIInterface::SetSelectedObject": (0x101f0000, 40, "setsel", "u", False, []),
    "UIInterface::GetSelectedObject": (0x101effa4, 36, "getsel", "", True, ["GETSEL"]),
    "UIInterface::GetSelectedPeep": (0x101eff4c, 36, "getpeep", "", True, ["GETPEEP"]),
    "UIInterface::Load": (0x101f0284, 60, "load", "u", False, []),
    "UIInterface::MessageTag": (0x101efee8, 40, "msgtag", "p", False, []),
    "UIInterface::Save": (0x101f02ec, 40, "save", "u", False, []),
    "UIInterface::RemoveLocators": (0x101f0340, 64, "removeloc", "", True, ["SPU"]),
    "UIInterface::StaffDBChange": (0x101efe04, 40, "staffdb", "i", False, []),
    "UIInterface::ShowGameMainMenu": (0x101f03b4, 40, "mainmenu", "", False, []),
    "UIInterface::RideCondemned": (0x101efd74, 48, "condemned", "h", False, []),
    "UIInterface::Screen_BuyObject": (0x101f0410, 56, "buyobj", "", False, []),
    "UIInterface::Screen_BuyStaff": (0x101f047c, 40, "buystaff", "", False, []),
    "UIInterface::RideBrokenDown": (0x101efce4, 48, "broken", "h", False, []),
    "UIInterface::Screen_ParkStatus": (0x101f04d8, 36, "parkstat", "", False, []),
    "UIInterface::RideDeleted": (0x101efc84, 4, "deleted", "h", False, []),
    "UIInterface::Screen_AllItems": (0x101f0534, 44, "allitems", "", False, []),
    "UIInterface::ResearchCategoryComplete": (0x101efc14, 40, "research", "i", False, []),
    "UIInterface::Screen_AllStaff": (0x101f0594, 44, "allstaff", "", False, []),
    "UIInterface::ShowMap": (0x101efbc0, 40, "showmap", "", False, []),
    "UIInterface::Screen_AllPeeps": (0x101f05f4, 40, "allpeeps", "", False, []),
    "UIInterface::Screen_Finance": (0x101f0650, 36, "finance", "", False, []),
    "UIInterface::Screen_Loans": (0x101f06a8, 36, "loans", "", False, []),
    "UIInterface::Screen_StaffCosts": (0x101f06fc, 36, "staffcosts", "", False, []),
}
OWN = set(v[0] for v in FUNCS.values())

# name: (address, spec per register from r3, answers)   L label, i s32, w high half, u raw word, p the id word behind the pointer
CALLEES = {
    "GameWindowToggle_Hidden": (0x101a8c1c, "", True),
    "ToggleGameWindow": (0x101a8ac0, "", False),
    "UIMain_GetShiftKeyState": (0x10182088, "", True),
    "UI_SetSelectedObject": (0x10194768, "u", False),
    "UI_GetSelectedObject": (0x10194730, "", True),
    "UI_GetSelectedPeep": (0x101946f8, "", True),
    "InstantRemoveAllTags": (0x10192280, "", False),
    "LoadTags": (0x10192704, "u", False),
    "AddTag": (0x101921f4, "p", False),
    "SaveTags": (0x1019231c, "u", False),
    "StaffPanelUp": (0x101c0cac, "", True),
    "RemoveStaffPanel": (0x101c0c5c, "", False),
    "UI_HirePanelStateChange": (0x101a2a14, "i", False),
    "CreateMainMenu": (0x101fc05c, "i", False),
    "RideStatusPanel::Condemned": (0x101bc2f0, "Lw", False),
    "RideStatusPanel::BreakDown": (0x101bc1f4, "Lw", False),
    "CreateRideListPanel": (0x101b8434, "iw", False),
    "CreateHireListPanel": (0x101a1c88, "i", False),
    "CreateParkStatusPanel": (0x101aca78, "", False),
    "CreateAllItemsPanel": (0x101983c4, "iu", False),
    "ResearchPanelCategoryComplete": (0x101b5b4c, "i", False),
    "CreateAllStaffPanel": (0x10199630, "iu", False),
    "CUI2DMap::CreateMapPanel": (0x1022c080, "L", False),
    "CreateAllKidsPanel": (0x101952bc, "u", False),
    "CreateFinancePanel": (0x101a001c, "", False),
    "CreateLoansPanel": (0x101a4de4, "", False),
    "CreateStaffCostsPanel": (0x101be440, "", False),
}
ADDR = dict(rsp=0x103959a4, map=0x1039742c)
HOST_LAYOUT = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_stub(name, spec, answers):
    def stub(e):
        L = e.cl
        vals = []
        for i, c in enumerate(spec):
            r = e.r(3 + i) & MASK
            if c == "L":
                vals.append(L.lab(r))
            elif c == "i":
                vals.append(T.s32(r))
            elif c == "w":
                vals.append((r >> 16) & 0xFFFF)
            elif c == "u":
                vals.append(r)
            elif c == "p":
                vals.append(e.u32(r))
        L.rec(name, tuple(vals))
        if answers:
            e.set_r(3, L.ret(name) & MASK)
    return stub


for _n, (_a, _s, _r) in CALLEES.items():
    STUBS.internal(_a, make_stub(_n, _s, _r))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))


class Ppc(Side):
    order = "big"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["rsp"]: 1, ADDR["map"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, name, arg):
        addr, _, _, kind, answers, _ = FUNCS[name]
        args = [0x1000]
        if kind in "ui":
            args.append(arg & MASK)
        elif kind == "h":
            args.append((arg << 16) & MASK)
        elif kind == "p":
            args.append(arg)
        r3, _, _ = T.call_ppc(self.e, addr, args)
        return r3 & MASK if answers else 0


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_uip_addr(b"rsp"), 2: lib.tpw_uip_addr(b"map")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_uip_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, arg):
        addr, _, key, kind, answers, _ = FUNCS[name]
        f = getattr(self.lib, "tpw_uip_" + key)
        r = f() if kind == "" else f(arg)
        return r & MASK if answers else 0


KINDS = {"u": C.c_uint32, "i": C.c_int32, "h": C.c_uint16, "p": C.c_uint64}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_uip_"
    lib.tpw_uip_addr.restype, lib.tpw_uip_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_uip_alloc.restype, lib.tpw_uip_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_uip_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, (addr, size, key, kind, answers, cs) in FUNCS.items():
        f = getattr(lib, p + key)
        f.restype = C.c_uint32
        f.argtypes = [KINDS[kind]] if kind else []
    out = (C.c_int64 * 32)()
    n = lib.tpw_uip_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        name = case["op"]
        arg = case["arg"]
        if FUNCS[name][3] == "p":
            blk = s.alloc(0x28)
            s.words(blk, [case["arg"], 1, 2, 3, 4, 5, 6, 7, 8, 9])
            arg = blk
        ans = s.call(name, arg)
        return {"answer": ans, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


WORDS = [0, 1, 0xFFFFFFFF, 0x80000000, 0x10000, 0x7FFFFFFF, 0xFFFF, 0x12345678]


def mkcase(r, name, arg):
    queues = {}
    for cn in FUNCS[name][5]:
        queues[{"GWTH": "GameWindowToggle_Hidden", "SHIFT": "UIMain_GetShiftKeyState", "GETSEL": "UI_GetSelectedObject",
                "GETPEEP": "UI_GetSelectedPeep", "SPU": "StaffPanelUp"}[cn]] = [arg]
    return {"op": name, "queues": queues, "arg": arg}


def make_test(name):
    kind = FUNCS[name][3]
    has_q = bool(FUNCS[name][5])

    def val(r):
        if kind == "h":
            return r.choice([0, 1, 0xFFFF, r.randrange(0x10000)])
        return r.choice(WORDS + [r.randrange(1 << 32)])

    def edges():
        r = random.Random(7)
        vals = WORDS if (kind or has_q) else [0]
        for v in vals:
            if kind == "h":
                v &= 0xFFFF
            yield mkcase(r, name, v)

    def rnd(r):
        return mkcase(r, name, val(r) if (kind or has_q) else 0)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(n) for n in FUNCS]
