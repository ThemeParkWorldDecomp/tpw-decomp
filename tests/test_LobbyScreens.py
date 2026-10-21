"""Differential test for src/game/online/LBScreenComponentAdvisor.cpp, src/game/online/lobby_screen.cpp (CLBScreenSystem and CLBScreenComponent),
against libtpw_unit_LobbyScreens.dylib (tests/unit_LobbyScreens.cpp).

A case is a script: make the screen system, Init (it makes its five screens), test screens
that register with it, Tick, Render, Close, window messages, "cancel", the list calls,
destroy. After every operation: the answer, the events of the fakes, and the state (the five
pointers, the list of screens in order with each screen's "visible" word).

On the PowerPC side the system, the screen base class and the list are the original's code.
The five concrete screens are stand-ins there: the two base constructors (islands, globe)
record and add the object to the system's list as the screen base class would, the real
derived vtables are left in place, and every virtual member the system reaches is replaced
at its own address by a recorder (so a member shared by two classes records under the class
that defines it, like the host's fake). The advisor is made in place by Init on both sides.

The test's own rules, decided on the PowerPC side: screens are made and destroyed while
their system is the one in gLobbyScreens; a screen is in the list once; Close only after
Init (the five pointers are not set before); DeleteScreenSystemList ends the system's life.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import test_Lobby as LB
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyScreens"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F
SYSTEM = dict(ctor=0x100B6D48, dtor=0x100B6DEC, Init=0x100B6E8C, Tick=0x100B702C, Render=0x100B70D0, Close=0x100B7178,
              WindowMessagesFn=0x100B72D0, ProcessCancel=0x100B73C4, DeleteScreenSystemList=0x100B7498, list_dtor=0x100B755C,
              Register=0x100B7600, UnRegister=0x100B766C, list_base_dtor=0x100B7778)
SCREEN = dict(ctor=0x100B6BA4, dtor=0x100B6C2C, Show=0x100B6CD0, Hide=0x100B6D0C, IsVisible=0x100A6FB8)
G_SCREENS = 0x102E49B8
BASE_DTOR_TV, SHOW_TV, HIDE_TV, VISIBLE_TV = 0x102668E0, 0x102668D8, 0x102668D0, 0x10266438
# The concrete screens' members the system reaches, by the class that defines them.
MEMBERS = {"Islands.Init": 0x100B0A54, "Islands.Render": 0x100B1D94, "Islands.Close": 0x100B1DD0,
           "ThemeIslands.Tick": 0x100B29DC, "OnlineIslands.Tick": 0x100B3B28,
           "Globe.Render": 0x100AD4CC, "Globe.Close": 0x100AD50C,
           "DownloadGlobe.Init": 0x100AE2D0, "DownloadGlobe.Tick": 0x100AE318,
           "PublishGlobe.Init": 0x100AEAC8, "PublishGlobe.Tick": 0x100AEB10,
           "Advisor.Init": 0x100A6C98, "Advisor.Tick": 0x100A6CE8, "Advisor.Render": 0x100A6E10, "Advisor.Close": 0x100A6E4C}
INPUTS = {"ThemeIslands": 0x100B2AF8, "OnlineIslands": 0x100B3BC8, "DownloadGlobe": 0x100AE374, "PublishGlobe": 0x100AEB6C,
          "Advisor": 0x100A6EA8}
CANCELS = {"ThemeIslands": 0x100B2C00, "OnlineIslands": 0x100B3CD0, "Globe": 0x100AD690, "DownloadGlobe": 0x100AE46C,
           "Advisor": 0x100A6EF0}
DTORS = {0x100B4CEC: ("ThemeIslands", "Islands"), 0x100B4BE0: ("OnlineIslands", "Islands"),
         0x100AF330: ("DownloadGlobe", "Globe"), 0x100AF1C8: ("PublishGlobe", "Globe"), 0x100A7234: ("Advisor",)}
BASE_CTORS = {0x100B023C: "Islands", 0x100AC90C: "Globe"}
SCREENS = 8
SKIPPED = "(skipped)"


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(SYSTEM.values())
    S.real.update(SCREEN.values())
    S.imp("__nw__FUl", lambda e: e.set_r(3, e.alloc(max(e.r(3), 4), b"\xa5" * max(e.r(3), 4))))

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__dl__FPv", op_delete)
    # The lobby system base class does nothing here (it needs a lobby).
    S.internal(0x100A56C8, lambda e: None)
    S.internal(0x100A5744, lambda e: None)
    S.internal(0x101FC05C, R("CreateMainMenu", "i"))
    for name, address in MEMBERS.items():
        S.internal(address, R(name, "L"))
    for name, address in INPUTS.items():
        S.internal(address, R(name + ".WindowMessagesFn", "Liii"))
    for name, address in CANCELS.items():
        S.internal(address, R(name + ".ProcessCancel", "L", lambda e, L: L.ret("Cancel", 0)))

    def leave(e, obj):
        system = e.u32(G_SCREENS)
        if system and e.u32(system + 0x18):
            LB.list_remove(e, e.u32(system + 0x18), obj)

    def base_ctor(name):
        def fn(e):
            L, obj = e.cl, e.r(3)
            L.rec(name + ".ctor", (L.newlab(obj),))
            w32(e, obj + 4, 0)
            LB.list_add_tail(e, e.u32(e.u32(G_SCREENS) + 0x18), obj)
        return fn

    def dtor(names):
        def fn(e):
            L, obj = e.cl, e.r(3)
            for name in names:
                L.rec(name + ".dtor", (L.lab(obj),))
            leave(e, obj)
            if T.s16(e.r(4)) > 0 and L.lab(obj) > 0:
                L.rec("delete", (L.lab(obj),))
                L.forget(obj)
        return fn

    for address, name in BASE_CTORS.items():
        S.internal(address, base_ctor(name))
    for address, names in DTORS.items():
        S.internal(address, dtor(names))
    emu.ls_test_vtable = LB.make_vtable(emu, {
        8: BASE_DTOR_TV, 0xC: R("Test.Init", "L"), 0x10: R("Test.Tick", "L"), 0x14: R("Test.Render", "L"), 0x18: R("Test.Close", "L"),
        0x1C: R("Test.WindowMessagesFn", "Liii"), 0x20: R("Test.ProcessCancel", "L", lambda e, L: L.ret("Cancel", 0)),
        0x24: SHOW_TV, 0x28: HIDE_TV, 0x2C: VISIBLE_TV})
    return S


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.block = e.alloc(0x1C)
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        self.live = self.inited = False
        self.screens = [0] * SCREENS
        w32(e, G_SCREENS, 0)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def listed(self):
        e = self.e
        lst = e.u32(self.block + 0x18) if self.live else 0
        return LB.list_values(e, lst) if lst else []

    def op(self, op, a):
        e, L, block = self.e, self.L, self.block
        slot = a[0] & (SCREENS - 1)
        screen = self.screens[slot]
        if op == 1:
            if self.live:
                return None
            e.write(block, b"\xa5" * 0x1C)
            self.call(SYSTEM["ctor"], block)
            w32(e, G_SCREENS, block)
            self.live, self.inited = True, False
            return 0
        if op in (2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14) and not self.live:
            return None
        if op in (2, 9):
            gone = set(self.listed())
            self.call(SYSTEM["dtor"], block, 0) if op == 2 else self.call(SYSTEM["DeleteScreenSystemList"], block)
            self.screens = [0 if s in gone else s for s in self.screens]
            list_gone = 1 if e.u32(block + 0x18) == 0 else 0
            w32(e, G_SCREENS, 0)
            self.live = False
            return list_gone
        if op == 3:
            self.call(SYSTEM["Init"], block)
            self.inited = True
            return 0
        if op in (4, 5):
            self.call(SYSTEM["Tick" if op == 4 else "Render"], block)
            return 0
        if op == 6:
            if not self.inited:
                return None
            self.call(SYSTEM["Close"], block)
            return 0
        if op == 7:
            self.call(SYSTEM["WindowMessagesFn"], block, a[1], a[2], a[3])
            return 0
        if op == 8:
            self.call(SYSTEM["ProcessCancel"], block)
            return 0
        if op == 10:
            if screen:
                return None
            made = e.alloc(8, b"\xa5" * 8)
            self.call(SCREEN["ctor"], made)
            w32(e, made, e.ls_test_vtable)
            self.screens[slot] = made
            return L.newlab(made)
        if op == 11:
            if not screen:
                return None
            self.call(SCREEN["dtor"], screen, 1)
            self.screens[slot] = 0
            return 0
        if op in (12, 13):
            if not screen or (op == 12 and screen in self.listed()):
                return None
            self.call(e.u32(e.u32(e.u32(block) + (0x20 if op == 12 else 0x24))), block, screen)
            return 0
        if op == 14:
            members = self.listed()
            if a[1] < len(members):
                w32(e, members[a[1]] + 4, a[2])
            return 0
        if op == 15:
            if not screen:
                return None
            if a[1] in (0, 1):
                self.call(e.u32(e.u32(e.u32(screen) + (0x24 if a[1] == 0 else 0x28))), screen)
            return T.s32(self.call(e.u32(e.u32(e.u32(screen) + 0x2C)), screen))
        return -99

    def state(self):
        e, L, block = self.e, self.L, self.block
        out = [1 if self.live else 0]
        if self.live:
            lst = e.u32(block + 0x18)
            out += [L.lab(e.u32(block + o)) for o in (4, 8, 0xC, 0x10)] + [0 if e.u32(block + 0x14) == 0 else 1]
            out.append(T.s32(e.u32(lst + 0xC)) if lst else -1)
            if lst:
                for member in LB.list_values(e, lst):
                    out += [L.lab(member), T.s32(e.u32(member + 4))]
        out += [T.s32(e.u32(s + 4)) if s else -7 for s in self.screens]
        return out


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs(e)


def screens_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        side = PpcSide(e, case)
        answers, states, marks, skipped = [], [], [], []
        for op, a in case["script"]:
            answer = side.op(op, list(a))
            skipped.append(answer is None)
            answers.append(SKIPPED if answer is None else answer)
            states.append(side.state())
            marks.append(len(side.L.log))
        case["_skipped"] = skipped
        return dict(answers=answers, states=states, marks=marks, log=side.L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def screens_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib["tpw_ls_begin"]()
    CL.host_begin(lib, {}, case.get("q"))
    op_fn = F(lib, "tpw_ls_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32))
    state_fn = F(lib, "tpw_ls_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 256)()
    try:
        for (op, a), skip in zip(case["script"], case["_skipped"]):
            if skip:
                answers.append(SKIPPED)
            else:
                answers.append(op_fn(op, (C.c_int32 * 8)(*[T.s32(v & MINUS) for v in a])))
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, marks=marks, log=log)


# ---- cases


def op(code, *a):
    return (code, (list(a) + [0] * 8)[:8])


def random_script(r, length):
    script = []
    for _ in range(length):
        k = r.random()
        slot = r.randrange(SCREENS)
        if k < 0.12:
            script.append(op(1))
        elif k < 0.16:
            script.append(op(2))
        elif k < 0.26:
            script.append(op(3))
        elif k < 0.36:
            script.append(op(4))
        elif k < 0.44:
            script.append(op(5))
        elif k < 0.50:
            script.append(op(6))
        elif k < 0.60:
            script.append(op(7, 0, r.choice([0x1000B, 0x1000B, 5, 0x10006]), r.choice([0x1B, 0x1B, 0x20, 0]), r.getrandbits(8)))
        elif k < 0.66:
            script.append(op(8))
        elif k < 0.68:
            script.append(op(9))
        elif k < 0.78:
            script.append(op(10, slot))
        elif k < 0.82:
            script.append(op(11, slot))
        elif k < 0.84:
            script.append(op(12, slot))
        elif k < 0.87:
            script.append(op(13, slot))
        elif k < 0.96:
            script.append(op(14, 0, r.randrange(8), r.choice([0, 1, 1, 2, -1])))
        else:
            script.append(op(15, slot, r.randrange(3)))
    return script


def screens_case(r):
    return dict(q={"Cancel": [r.choice([0, 0, 1, 2, 0x80000000]) for _ in range(r.randrange(1, 12))] + [0]},
                script=random_script(r, r.randrange(6, 30)))


def screens_edge():
    r = random.Random(13)
    # The five screens and two of the test's through a whole life; each made visible in turn.
    script = [op(1), op(10, 0), op(3), op(10, 1), op(4), op(5), op(7, 0, 5, 6, 7), op(8)]
    for index in range(7):
        script += [op(14, 0, index, 1), op(4), op(5), op(7, 0, 0x1000B, 0x1B, 0), op(7, 0, 0x1000B, 0x1C, 0), op(7, 0, 0x1000A, 0x1B, 0),
                   op(8), op(14, 0, index, 0)]
    script += [op(6), op(4), op(6), op(2)]
    yield dict(q={"Cancel": [0]}, script=script)
    # "Cancel": the main menu only when no visible screen takes it.
    for answers in ([0, 0], [1, 0], [0, 2], [0x80000000, 0]):
        yield dict(q={"Cancel": answers + [0]},
                   script=[op(1), op(10, 0), op(10, 1), op(8), op(14, 0, 0, 1), op(14, 0, 1, 2), op(8), op(7, 0, 0x1000B, 0x1B, 9), op(2)])
    # The base class's own members, the list calls, Init twice, a system destroyed without Close.
    yield dict(script=[op(1), op(10, 0), op(15, 0, 2), op(15, 0, 0), op(15, 0, 2), op(4), op(15, 0, 1), op(4),
                       op(14, 0, 0, 2), op(15, 0, 2), op(14, 0, 0, -1), op(15, 0, 2), op(4), op(13, 0), op(13, 0),
                       op(12, 0), op(11, 0), op(10, 1), op(10, 2), op(11, 1), op(9)])
    yield dict(script=[op(1), op(3), op(3), op(14, 0, 0, 1), op(14, 0, 7, 1), op(4), op(6), op(4), op(2)])
    yield dict(script=[op(1), op(10, 0), op(3), op(10, 1), op(2), op(1), op(4), op(3), op(6), op(6), op(2)])
    for _ in range(60):
        yield screens_case(r)


TESTS = [FnTest("CLBScreenSystem and CLBScreenComponent", screens_ppc, screens_host, edge=screens_edge, rnd=screens_case)]
