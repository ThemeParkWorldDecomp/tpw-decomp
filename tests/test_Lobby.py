"""Differential test for src/lobby.cpp (the lobby object and the base class of its systems),
against libtpw_unit_Lobby.dylib (tests/unit_Lobby.cpp).

A case is a script: make the lobby, make and destroy systems (which register with it), Init,
Tick, Render, Close, the list calls, the window message function, destroy it. After every
operation: what it answered, the events of the fakes, and the state (the lobby's fields, its
list of systems in order, the globals).

On the PowerPC side the lobby, the base class of the systems and the list are the original's
code. The four systems the lobby makes are stand-ins there (a constructor that records and
adds the object to the lobby's list as the base class would, a vtable of recording members);
on the host they are fakes over the real base class.

The test's own rules, decided on the PowerPC side: the lobby is the one in gLobby whenever
systems are made or destroyed (they register with it); a system is in the list once; after
the lobby is destroyed the globals of its four systems are cleared (the game closes a lobby
before it destroys it); DeleteSystemList ends the lobby's life (its only caller is the
destructor).
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Lobby"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F
LOBBY = dict(ctor=0x100A57E4, dtor=0x100A58C8, Init=0x100A5B40, Tick=0x100A5E8C, Render=0x100A5F78, Close=0x100A6004,
             DeleteSystemList=0x100A61F8, Register=0x100A634C, UnRegister=0x100A63AC, system_ctor=0x100A56C8,
             system_dtor=0x100A5744, window_fn=0x100A5950, globals_dtor=0x100A6180, list_dtor=0x100A62AC)
G = dict(lobby=0x102E4888, screens=0x102E49B8, flying=0x102E48D4, view=0x102E48CC, meshes=0x102E48D0,
         definition=0x10395C24, return_flag=0x102E4860, user=0x10380B34, online=0x10380AB0)
WINDOW_DATA, WINDOW_FN_TV, MAIL_FN_TV, SYSTEM_DTOR_TV = 0x1029E382, 0x10266408, 0x10266400, 0x10266410
SYSTEMS = 8
SKIPPED = "(skipped)"


def list_add_tail(e, lst, value):
    """TbTList::AddTail on the Mac's layout (head 4, tail 8, count 0xc; a node: value, next, previous)."""
    tail = e.u32(lst + 8)
    node = e.alloc(12, struct.pack(">III", value, 0, tail))
    w32(e, (tail + 4) if tail else (lst + 4), node)
    w32(e, lst + 8, node)
    w32(e, lst + 0xC, e.u32(lst + 0xC) + 1)


def list_remove(e, lst, value):
    node = e.u32(lst + 4)
    while node and e.u32(node) != value:
        node = e.u32(node + 4)
    if not node:
        return
    following, previous = e.u32(node + 4), e.u32(node + 8)
    w32(e, (previous + 4) if previous else (lst + 4), following)
    w32(e, (following + 8) if following else (lst + 8), previous)
    w32(e, lst + 0xC, e.u32(lst + 0xC) - 1)


def list_values(e, lst):
    out, node = [], e.u32(lst + 4)
    while node and len(out) < 40:
        out.append(e.u32(node))
        node = e.u32(node + 4)
    return out


def system_dtor(name):
    """A stand-in system's destructor: records, leaves the lobby's list, frees when asked."""
    def fn(e):
        L, obj = e.cl, e.r(3)
        L.rec(name + ".dtor", (L.lab(obj),))
        lobby = e.u32(G["lobby"])
        if lobby and e.u32(lobby + 0x18):
            list_remove(e, e.u32(lobby + 0x18), obj)
        if T.s16(e.r(4)) > 0 and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)
    return fn


def make_vtable(e, slots):
    vt = T.persist_alloc(e, 0x40)
    for offset, target in slots.items():
        w32(e, vt + offset, T.fake_function(e, target) if callable(target) else target)
    return vt


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(LOBBY.values())
    FS.string_stubs(S)
    emu.lb_vtables = {}
    for name in ("Meshes", "View", "Flying", "Screens"):
        emu.lb_vtables[name] = make_vtable(emu, {8: system_dtor(name), 0xC: R(name + ".Init", "L"), 0x10: R(name + ".Tick", "L"),
                                                 0x14: R(name + ".Render", "L"), 0x18: R(name + ".Close", "L")})
    emu.lb_vtables["System"] = make_vtable(emu, {8: SYSTEM_DTOR_TV, 0xC: R("System.Init", "L"), 0x10: R("System.Tick", "L"),
                                                 0x14: R("System.Render", "L"), 0x18: R("System.Close", "L")})
    emu.lb_vtables["Screen"] = make_vtable(emu, {0x24: lambda e: e.cl.rec("Screen.Activate", (e.u32(e.r(3) + 4),))})

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__nw__FUl", lambda e: e.set_r(3, e.alloc(max(e.r(3), 4), b"\xa5" * max(e.r(3), 4))))
    S.imp("__dl__FPv", op_delete)
    S.imp("time", lambda e: e.set_r(3, 0x3B9ACA00))
    S.internal(0x101472F4, R("Clock.Read", "", lambda e, L: L.ret("Read", 1000)))
    S.internal(0x1014733C, R("Clock.ReadFixed", "", lambda e, L: L.ret("ReadFixed", 1000)))

    def user_ctor(e):
        L, obj = e.cl, e.r(3)
        L.rec("CFSUserInfo.ctor", (L.newlab(obj),))
        w32(e, obj + 0x60, L.ret("GameUser", -1))
        w32(e, obj + 0x8C, L.ret("Outbox", 0))

    def online_ctor(e):
        L, obj = e.cl, e.r(3)
        L.rec("CFSOnlineSystem.ctor", (L.newlab(obj),))
        w32(e, obj + 0x108, L.ret("Online108", 0))

    S.internal(0x1017DE54, user_ctor)
    S.internal(0x1016F7D0, online_ctor)
    side = lambda e: e.lb   # noqa: E731
    S.internal(0x101DEB78, lambda e: (e.cl.rec("Window.Build", (e.cl.lab(e.r(3)), int(e.r(4) == WINDOW_DATA), int(e.r(5) == WINDOW_FN_TV))),
                                      e.set_r(3, side(e).built)))
    S.internal(0x101DCCEC, R("Window.CaptureKb", "Li"))
    S.internal(0x101DBE40, R("Window.GetWindow", "Li", lambda e, L: side(e).mail if L.ret("GetWindow", 1) else 0))
    S.internal(0x101DCBA8, lambda e: e.cl.rec("Window.SetCallbackFunction", (e.cl.lab(e.r(3)), int(e.r(4) == MAIL_FN_TV))))
    S.internal(0x101C9518, R("UI_SendMessage", "Liii", lambda e, L: 0))
    S.internal(0x101874D0, R("UI_CreateVersionNumber", "b"))
    S.internal(0x101AE9E0, R("CreateLoginPanel"))
    S.internal(0x100DEF68, R("SND_RegisterGlobalLobbySounds"))
    S.internal(0x100DF1BC, R("SND_FreeGlobalLobbySounds"))
    S.internal(0x100B55B8, R("ParseLobbyDescriptor", "N"))
    S.internal(0x100A6484, R("Lobby.WindowMessagesFn", "LLiii", lambda e, L: L.ret("WindowMessagesFn", 7)))

    def system_ctor(name):
        def fn(e):
            L, obj = e.cl, e.r(3)
            L.rec(name + ".ctor", (L.newlab(obj),))
            list_add_tail(e, e.u32(e.u32(G["lobby"]) + 0x18), obj)
            w32(e, obj, emu.lb_vtables[name])
            if name == "Screens":
                for offset, which in ((4, 1), (0xC, 3), (0x10, 4), (0x14, 5)):
                    w32(e, obj + offset, e.alloc(8, struct.pack(">II", emu.lb_vtables["Screen"], which)))
                w32(e, obj + 8, 0)
        return fn

    for address, name in ((0x100A7EFC, "Meshes"), (0x100A8B40, "View"), (0x100AB578, "Flying"), (0x100B6D48, "Screens")):
        S.internal(address, system_ctor(name))
    return S


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.lb = self
        e.heap_in_scratch = True
        self.block = e.alloc(0x38)
        self.definition, self.built, self.mail, self.message_window = (e.alloc(8) for _ in range(4))
        self.user_block, self.online_block = e.alloc(0xD4), e.alloc(0x404)
        self.L = e.cl = CL.PpcLog({self.block: 1, self.definition: 2, self.built: 3, self.mail: 4, self.user_block: 5,
                                   self.online_block: 6, self.message_window: 7}, case.get("q"))
        self.live = False
        self.systems = [0] * SYSTEMS
        for name in G:
            w32(e, G[name], 0)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def current(self):
        return self.live and self.e.u32(G["lobby"]) == self.block

    def listed(self):
        e = self.e
        lst = e.u32(self.block + 0x18) if self.live else 0
        return list_values(e, lst) if lst else []

    def forget_lobby(self):
        """After the lobby is gone: the systems it destroyed, the globals of its four."""
        e = self.e
        w32(e, G["lobby"], 0)
        self.live = False
        for name in ("screens", "flying", "view", "meshes"):
            w32(e, G[name], 0)

    def op(self, op, a):
        e, L, block = self.e, self.L, self.block
        slot = a[0] & (SYSTEMS - 1)
        system = self.systems[slot]
        if op == 1:
            if self.live:
                return None
            e.write(block, b"\xa5" * 0x38)
            self.live = True
            w32(e, G["lobby"], block)        # (set first here; the constructor does not read it)
            self.call(LOBBY["ctor"], block)
            return 0
        if op in (2, 3, 6, 7, 8, 9) and not self.current():
            return None
        if op in (4, 5, 10, 11, 15) and not self.live:
            return None
        if op in (2, 7):
            gone = set(self.listed())
            self.call(LOBBY["dtor"], block, 0) if op == 2 else self.call(LOBBY["DeleteSystemList"], block)
            self.systems = [0 if s in gone else s for s in self.systems]
            list_gone = 1 if e.u32(block + 0x18) == 0 else 0
            self.forget_lobby()
            return list_gone
        if op in (3, 4, 5, 6):
            return T.s32(self.call(LOBBY[("Init", "Tick", "Render", "Close")[op - 3]], block))
        if op == 8:
            if system:
                return None
            made = e.alloc(4)
            self.call(LOBBY["system_ctor"], made)
            w32(e, made, e.lb_vtables["System"])
            self.systems[slot] = made
            return L.newlab(made)
        if op == 9:
            if not system:
                return None
            self.call(LOBBY["system_dtor"], system, 1)
            self.systems[slot] = 0
            return 0
        if op in (10, 11):
            if not system or (op == 10 and system in self.listed()):
                return None
            self.call(e.u32(e.u32(e.u32(block) + (8 if op == 10 else 0xC))), block, system)
            return 0
        if op == 12:
            return T.s32(self.call(LOBBY["window_fn"], self.message_window, a[1], a[2], a[3]))
        if op == 13:
            if a[1] and not self.live:
                return None
            w32(e, G["lobby"], block if a[1] else 0)
            return 0
        if op == 14:
            w32(e, G["return_flag"], a[1])
            w32(e, G["definition"], self.definition if a[2] else 0)
            w32(e, G["user"], self.user_block if a[3] & 1 else 0)
            if a[3] & 1:
                w32(e, self.user_block + 0x60, a[4])
                w32(e, self.user_block + 0x8C, a[5])
            w32(e, G["online"], self.online_block if a[3] & 2 else 0)
            if a[3] & 2:
                w32(e, self.online_block + 0x108, a[6])
            return 0
        if op == 15:
            w32(e, block + 0x14, a[1])
            w32(e, block + 0x1C, a[2])
            return 0
        return -99

    def state(self):
        e, L, block = self.e, self.L, self.block
        s = lambda o: T.s32(e.u32(block + o))   # noqa: E731
        out = [1 if self.live else 0]
        if self.live:
            lst = e.u32(block + 0x18)
            out += [L.lab(e.u32(block + 4)), e.u32(block + 8), s(0xC), L.lab(e.u32(block + 0x10)), s(0x14),
                    T.s32(e.u32(lst + 0xC)) if lst else -1]
            if lst:
                out += [L.lab(v) for v in list_values(e, lst)]
            out += [s(0x1C), s(0x20), s(0x24), s(0x28), s(0x2C), e.u32(block + 0x30), 1 if e.u32(block + 0x34) != 0xA5A5A5A5 else 0]
        current = e.u32(G["lobby"])
        out.append(0 if current == 0 else 1 if current == block and self.live else -1)
        out += [L.lab(e.u32(G[name])) for name in ("screens", "flying", "view", "meshes")]
        out += [T.s32(e.u32(G["return_flag"])), L.lab(e.u32(G["user"])), L.lab(e.u32(G["online"]))]
        return out


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    if "c" not in FS.VT:
        FS.make_vtables(e)
    STUBS = build_stubs(e)


def lobby_ppc(e, case):
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


def lobby_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib["tpw_lb_begin"]()
    fixed_at = F(lib, "tpw_lb_fixed", C.c_void_p, C.c_int32)
    CL.host_begin(lib, {k: fixed_at(k) for k in range(1, 8)}, case.get("q"))
    op_fn = F(lib, "tpw_lb_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32))
    state_fn = F(lib, "tpw_lb_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 256)()
    live, listed_gone = False, None
    try:
        for (op, a), skip in zip(case["script"], case["_skipped"]):
            if skip:
                answers.append(SKIPPED)
            else:
                answers.append(op_fn(op, (C.c_int32 * 8)(*[T.s32(v & MINUS) for v in (list(a) + [0] * 8)[:8]])))
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, marks=marks, log=log)


# ---- cases


def op(code, *a):
    return (code, (list(a) + [0] * 8)[:8])


def random_script(r, length):
    script = [op(14, 0, r.choice([0, 0, 1]), r.choice([0, 1]), r.randrange(4), r.choice([-1, -1, 0, 2]), r.choice([0, 0, 3]),
                 r.choice([0, 0, 0, 1]))]
    for _ in range(length):
        k = r.random()
        slot = r.randrange(SYSTEMS)
        if k < 0.12:
            script.append(op(1))
        elif k < 0.17:
            script.append(op(2))
        elif k < 0.27:
            script.append(op(3))
        elif k < 0.42:
            script.append(op(4))
        elif k < 0.50:
            script.append(op(5))
        elif k < 0.58:
            script.append(op(6))
        elif k < 0.60:
            script.append(op(7))
        elif k < 0.76:
            script.append(op(8, slot))
        elif k < 0.82:
            script.append(op(9, slot))
        elif k < 0.85:
            script.append(op(10, slot))
        elif k < 0.90:
            script.append(op(11, slot))
        elif k < 0.93:
            script.append(op(12, 0, r.randrange(-3, 0x20000), r.getrandbits(16), r.getrandbits(16)))
        elif k < 0.95:
            script.append(op(13, 0, r.randrange(2)))
        elif k < 0.98:
            script.append(op(14, 0, r.choice([0, 0, 1]), r.choice([0, 1]), r.randrange(4), r.choice([-1, 0, 2]), r.choice([0, 3]),
                             r.choice([0, 0, 1])))
        else:
            script.append(op(15, 0, r.choice([0, 1, 2, 3]), r.choice([0, 0, 9])))
    return script


def lobby_case(r):
    q = {"Read": [r.getrandbits(20) for _ in range(4)],
         "ReadFixed": [r.choice([0, 5, 100, 499, 500, 501, 1000, 100000, r.getrandbits(18)]) for _ in range(10)]}
    if r.random() < 0.3:
        q["GetWindow"] = [r.randrange(2)]
    if r.random() < 0.5:
        q["GameUser"] = [r.choice([-1, 0, 3])]
    if r.random() < 0.3:
        q["Outbox"] = [r.choice([0, 2])]
    if r.random() < 0.3:
        q["Online108"] = [r.choice([0, 1])]
    q["WindowMessagesFn"] = [r.randrange(-5, 50)]
    return dict(q=q, script=random_script(r, r.randrange(6, 30)))


def lobby_edge():
    r = random.Random(11)
    # A lobby's whole life with three systems: they tick in the order they were made.
    yield dict(q={"ReadFixed": [1000, 1005, 1505, 2006, 2006, 1000]},
               script=[op(1), op(8, 0), op(8, 1), op(8, 2), op(4), op(4), op(4), op(4), op(4), op(4), op(5), op(11, 1), op(4),
                       op(10, 1), op(5), op(9, 0), op(4), op(6), op(2)])
    # Init in every situation: a window definition or none, the mail window found or not, a
    # user logged in or not or to be made, mail waiting, the online system's word, the flag.
    for definition in (0, 1):
        for have in (0, 1, 2, 3):
            for user in (-1, 2):
                for flag in (0, 1):
                    for online in (0, 1):
                        for mail in (0, 1):
                            yield dict(q={"GetWindow": [mail], "GameUser": [user], "Outbox": [1 - mail], "Online108": [online],
                                          "Read": [777]},
                                       script=[op(14, 0, flag, definition, have, user, mail * 3, online), op(1), op(8, 3), op(3),
                                               op(4), op(5), op(6), op(6), op(2)])
    # Init twice, a lobby destroyed without Close, systems that outlive their lobby.
    yield dict(script=[op(1), op(15, 0, 1, 9), op(3), op(15, 0, 1, 9), op(3), op(4), op(6), op(4), op(2)])
    yield dict(script=[op(1), op(8, 0), op(3), op(8, 1), op(4), op(2), op(1), op(4), op(2)])
    yield dict(script=[op(1), op(8, 0), op(8, 1), op(11, 0), op(2), op(1), op(9, 0), op(8, 0), op(10, 0), op(11, 0), op(11, 0),
                       op(10, 0), op(7)])
    # The window message function with and without a lobby.
    yield dict(q={"WindowMessagesFn": [42]}, script=[op(12, 0, 5, 6, 7), op(1), op(12, 0, 0x10006, -1, 2), op(13, 0, 0),
                                                       op(12, 0, 1, 2, 3), op(13, 0, 1), op(15, 0, 3), op(4), op(2)])
    for _ in range(60):
        yield lobby_case(r)


TESTS = [FnTest("CLBLobby and CLBLobbySystem", lobby_ppc, lobby_host, edge=lobby_edge, rnd=lobby_case)]
