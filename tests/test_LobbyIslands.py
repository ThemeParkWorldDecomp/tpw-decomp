"""Differential test for src/lobby_islands.cpp (the lobby's island screen: set-up, the list of
islands, show and hide; CLBIsland's constructor), against libtpw_unit_LobbyIslands.dylib
(tests/unit_LobbyIslands.cpp).

A case is a world and a script. The world: the lobby's view (position, target, field of view), the
lobby description's field of view, the online system (there or not), the sound table, the game's
themes, and the queues the fakes answer from (the lobby's generator, the C library's rand, the
sound registrations, the sound events, "which island does ResetCurrentIsland pick", IsVisible).
The script: make a theme-island screen (the Mac's way: a block of 0xa5, the base constructor, the
derived vtable), make islands that register with it, Register, UnRegister, SetCurrentIsland, Init,
Render, Close, Show and Hide (the theme islands' and the base class's), DeleteIslandListEntries,
the panels, and destroy the screen with the base destructor. After every operation: the calls
recorded by the fakes, the view's words, the online system, every live screen (its list, current
island and every camera word) and every live island, and how many blocks operator new and delete
have handled.

On the PowerPC side the screen base class, the island destructor, the view, the mesh system,
sounds, panels, the parser and the rest are recorders; the screens, the islands and the list
(TbTList<CLBIsland*>) run as the original's code.

The generator keeps a model of the screens (visible word, listed islands in order, the current
island) so that a script never lists an island twice (the screen would delete it twice) or leaves a
destroyed island as the current one.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyIslands"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F
U = dict(island_ctor=0x100AFE88, ctor=0x100B023C, dtor=0x100B08E4, Init=0x100B0A54, Render=0x100B1D94, Close=0x100B1DD0,
         Show=0x100B1FE0, Hide=0x100B2174, DeleteIslandListEntries=0x100B2374, Register=0x100B2420, UnRegister=0x100B2534,
         SetCurrentIsland=0x100B25D0, ThemeShow=0x100B2CC4, ThemeHide=0x100B2FE4, ShowPanels=0x100B3148, HidePanels=0x100B31B0)
# The list instances the screens use (TbTList<CLBIsland*, CLBIsland*>) and TbPtrList<CLBIsland>'s destructor.
LIST = dict(ct=0x100B4DF8, dt=0x100B4E54, find=0x100B4EF8, removeall=0x100B4F68, removeat=0x100B5010, removehead=0x100B50E8,
            insertbefore=0x100B51B0, addtail=0x100B5350, addhead=0x100B5424, ptrlist_dtor=0x100B09B8)
G = dict(lobby=0x102E4888, view=0x102E48CC, meshes=0x102E48D0, flying=0x102E48D4, online=0x10380AB0, sounds=0x102F7CC0,
         game=0x1037F134)
THEME_VTABLE = 0x1029E9C4
SCREENS, ISLANDS = 2, 8
L_SCREEN, L_LOBBY, L_GLOBALS, L_VIEW, L_MESHES, L_FLYING, L_RAIN, L_ONLINE, L_ISLAND, L_GAME, L_SIGN, L_PARK = (
    1, 3, 4, 5, 6, 7, 8, 9, 10, 18, 70, 80)
WORLD_WORDS = 17
NAMES = ["Alpha", "bravo", "Charlie Isle", "d", "Echo5"]
FOLDERS = ["data:lobby:", "DATA:lobby:rock", "f"]
MESHES = ["rock", "Rock_B", "m1", "island_a"]
TITLES = ["", "Spooky", "Sunny Park", "zz"]
THEMES = ["halloween", "space", "Hollywood", "jungle"]


def hash_text(text):
    h = 2166136261
    for ch in text.encode("latin-1"):
        h = ((h ^ ch) * 16777619) & MINUS
    return h


def hash16(text):
    h = 2166136261
    for ch in text:
        h = ((h ^ (ord(ch) & 0xFF)) * 16777619) & MINUS
    return h


def bf(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MINUS))[0]


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(U.values())
    S.real.update(LIST.values())
    FS.string_stubs(S)
    S.imp("rand", R("rand", "", lambda e, L: L.ret("rand", 0) & MINUS))
    S.imp("GameLoad__5CRainFv", R("CRain.GameLoad", "L"))
    S.imp("Close__5CRainFv", R("CRain.Close", "L"))

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        e.alloc_log.append(("delete", obj, 0))
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__dl__FPv", op_delete)

    # The screen base class: no screen system; the "visible" word starts at 0. IsVisible answers the word
    # unless the case queued answers.
    def base_ctor(e):
        L, obj = e.cl, e.r(3)
        L.rec("ScreenBase.ctor", (L.lab(obj),))
        w32(e, obj + 4, 0)

    S.internal(0x100B6BA4, base_ctor)
    S.internal(0x100B6C2C, R("ScreenBase.dtor", "L"))
    S.internal(0x100A6FB8, R("IsVisible", "L", lambda e, L: L.ret("IsVisible", T.s32(e.u32(e.r(3) + 4))) & MINUS))

    # The theme islands' members that are not in the unit.
    def reset_current(e):
        L, screen = e.cl, e.r(3)
        L.rec("ResetCurrentIsland", (L.lab(screen), T.s32(e.r(4))))
        k = L.ret("ResetCurrent", -1)
        node, n = e.u32(e.u32(screen + 8) + 4), 0
        while node:
            if n == k:
                w32(e, screen + 0xC, e.u32(node))
                w32(e, screen + 0x14, 0)
                break
            node, n = e.u32(node + 4), n + 1

    S.internal(0x100B3760, reset_current)
    S.internal(0x100B3854, R("ResetAdvisorTimeout", "L"))
    S.internal(0x100B38D4, R("UpdateSigns", "L"))
    S.internal(0x100B5D54, R("ParseThemeDescriptor", "LsLiSS"))

    def island_dtor(e):
        L, obj, flag = e.cl, e.r(3), T.s16(e.r(4))
        label = L.lab(obj)
        L.rec("CLBIsland.dtor", (label,))
        if L_ISLAND <= label < L_ISLAND + ISLANDS:
            e.li.island_alive[label - L_ISLAND] = False
        if flag > 0:
            e.alloc_log.append(("delete", obj, 0))
            if label > 0:
                L.rec("delete", (label,))
                L.forget(obj)

    S.internal(0x100B0108, island_dtor)

    # The lobby: the generator, the view, the mesh system and its instances, the flying meshes.
    S.internal(0x100AA98C, R("GetRand", "L", lambda e, L: L.ret("Rand", 0) & MINUS))
    S.internal(0x100A9428, R("SetGlobalAmbientLight", "Liu"))
    S.internal(0x100A94F8, lambda e: e.cl.rec(
        "SetGlobalDirectionalLight", (e.cl.lab(e.r(3)), T.s32(e.r(4)), e.u32(e.r(5)) if e.r(5) else -1, e.cl.lab(e.r(6)), T.s32(e.r(7)))))
    S.internal(0x100A992C, R("SetFogOn", "L"))
    S.internal(0x100A99B4, R("SetFogSettings", "Luff"))
    S.internal(0x100A82B0, R("ShowHeightfield", "L"))
    S.internal(0x100A835C, R("HideHeightfield", "L"))
    S.internal(0x100A88A4, R("DeleteMeshInstanceListEntries", "L"))
    S.internal(0x100AB8F0, R("DeleteFlyingMeshGroupListEntries", "L"))

    def instance_mesh(e):
        L = e.cl
        L.rec("InstanceMeshType", (L.lab(e.r(3)), T.s32(e.r(6)), e.r(7), L.lab(e.r(8)), L.lab(e.r(9))),
              (e.cstr(e.r(4), limit=400)[:159], e.cstr(e.r(5), limit=400)[:159]))
        obj = e.alloc(16, b"\xa5" * 16)
        L.newlab(obj)
        e.set_r(3, obj)

    S.internal(0x100A8408, instance_mesh)
    S.internal(0x100A7C0C, R("RotateY", "Lf"))
    S.internal(0x100A790C, lambda e: e.cl.rec("SetPosition", (e.cl.lab(e.r(3)),) + struct.unpack(">3I", e.read(e.r(4), 12))))

    # Sounds, the island lobby's panels, the online system.
    S.internal(0x100DF02C, R("SND_RegisterLocalLobbySounds", "s", lambda e, L: L.ret("SND_RegisterLocalLobbySounds", 0) & MINUS))
    S.internal(0x100DF234, R("SND_FreeLocalLobbySounds"))
    S.internal(0x100DE0FC, R("SOUND_FadeSoundEffect", "i"))
    S.internal(0x100DD9FC, R("SOUND_SendEvent", "iiiiii", lambda e, L: L.ret("SOUND_SendEvent", 0) & MINUS))
    S.internal(0x100DD668, R("SOUND_Tick"))
    S.internal(0x101E0F68, R("CreateIslandLobbyController"))
    S.internal(0x101E11F8, R("RemoveIslandLobbyController"))
    S.internal(0x101E1264, R("ShowIslandLobbyController"))
    S.internal(0x101E12D4, R("HideIslandLobbyController"))
    S.internal(0x101E15B4, R("UpdateIslandLobbyUI"))
    S.internal(0x1016F7D0, R("CFSOnlineSystem.ctor", "N"))
    S.internal(0x10171404, R("CFSOnlineSystem.StopProcessing", "L"))

    # The game's themes: the unicode name is the folder name widened, the gate names are made from it.
    def to_unicode(e):
        text = FS.s_read(e, e.r(4), False)
        e.cl.rec("StringToUniString", (), (text[:159],))
        FS.s_write(e, e.r(3), text, True)
        e.set_r(3, 1)

    def gate_names(e):
        text = FS.s_read(e, e.r(4), True)
        e.cl.rec("GetLocalisedThemeGateNames", (e.cl.lab(e.r(3)),), (CL.narrow(text),))
        FS.s_write(e, e.r(5), "g1:" + text, True)
        FS.s_write(e, e.r(6), "g2:" + text, True)

    S.internal(0x100057A0, to_unicode)
    S.internal(0x1014A768, gate_names)
    return S


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        e.li = self
        w, themes = case["world"], case["themes"]
        self.lobby = e.alloc(0x38, b"\xa5" * 0x38)
        self.globals = e.alloc(0x6C, b"\xa5" * 0x6C)
        self.view = e.alloc(0x64, b"\xa5" * 0x64)
        self.meshes, self.flying = e.alloc(16), e.alloc(16)
        self.rain = e.alloc(16)
        self.online = e.alloc(0x404, b"\xa5" * 0x404)
        self.signs = [CL.new_wide(e, "AB"), CL.new_wide(e, "cde")]
        self.parks = [e.alloc(16), e.alloc(16)]
        w32(e, self.lobby + 0x10, self.globals)
        w32(e, self.globals + 4, w[7])
        e.write(self.view + 4, struct.pack(">I", w[6]))
        e.write(self.view + 8, struct.pack(">6I", *w[0:6]))
        w32(e, G["lobby"], self.lobby)
        w32(e, G["view"], self.view)
        w32(e, G["meshes"], self.meshes)
        w32(e, G["flying"], self.flying)
        w32(e, e.data_import("gei_Rain"), self.rain)
        w32(e, G["online"], self.online if w[8] else 0)
        table = e.alloc(32, struct.pack(">8I", *w[9:17]))
        w32(e, G["sounds"], table)
        nodes = []
        for text in themes:
            string = FS.new_string(e, text, False)
            nodes.append((e.alloc(12), string))
        for i, (node, string) in enumerate(nodes):
            nxt = nodes[i + 1][0] if i + 1 < len(nodes) else 0
            prv = nodes[i - 1][0] if i else 0
            e.write(node, struct.pack(">III", string, nxt, prv))
        w32(e, G["game"] + 0x28, nodes[0][0] if nodes else 0)
        w32(e, G["game"] + 0x2C, nodes[-1][0] if nodes else 0)
        w32(e, G["game"] + 0x30, len(nodes))
        fixed = {self.lobby: L_LOBBY, self.globals: L_GLOBALS, self.view: L_VIEW, self.meshes: L_MESHES, self.flying: L_FLYING,
                 self.rain: L_RAIN, self.online: L_ONLINE, G["game"]: L_GAME,
                 self.signs[0]: L_SIGN, self.signs[1]: L_SIGN + 1, self.parks[0]: L_PARK, self.parks[1]: L_PARK + 1}
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        self.screens, self.islands = [0] * SCREENS, [0] * ISLANDS
        self.screen_alive, self.island_alive = [False] * SCREENS, [False] * ISLANDS
        del e.alloc_log[:]

    def finish(self):
        """Leaves the globals this case set as it found them (zero), so other modules see none of it."""
        e = self.e
        for name in ("lobby", "view", "meshes", "flying", "online", "sounds"):
            w32(e, G[name], 0)
        w32(e, e.data_import("gei_Rain"), 0)
        for off in (0x28, 0x2C, 0x30):
            w32(e, G["game"] + off, 0)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a if isinstance(a, T.F) else a & MINUS for a in args])[0]

    def virtual(self, obj, slot, *args):
        e = self.e
        return self.call(e.u32(e.u32(e.u32(obj) + slot)), obj, *args)

    def text(self, value):
        return self.e.alloc(len(value) + 1, value.encode("latin-1") + b"\0")

    def island_slot(self, address):
        if address == 0:
            return -1
        return self.islands.index(address) if address in self.islands else -9

    def screen_slot(self, address):
        if address == 0:
            return -1
        return self.screens.index(address) if address in self.screens else -9

    def op(self, op, a, texts):
        e, L = self.e, self.L
        s = a[0] & 1
        screen = self.screens[s]
        if op == 1:
            block = e.alloc(0xE8, b"\xa5" * 0xE8)
            e.alloc_log.append(("new", block, 0xE8))
            L.labels[block] = L_SCREEN + s
            self.call(U["ctor"], block)
            w32(e, block, THEME_VTABLE)
            self.screens[s] = block
            self.screen_alive[s] = True
        elif op == 2:
            self.call(U["dtor"], screen, a[1])
            self.screen_alive[s] = False
        elif op == 3:
            name, folder, mesh_a, mesh_b, title = (texts.split("\n") + [""] * 5)[:5]
            slot = a[0] & 7
            block = e.alloc(0x74, b"\xa5" * 0x74)
            e.alloc_log.append(("new", block, 0x74))
            L.labels[block] = L_ISLAND + slot
            position = e.alloc(12, struct.pack(">3I", *[v & MINUS for v in a[4:7]]))
            sign1 = self.signs[(a[7] - 1) & 1] if a[7] else 0
            sign2 = self.signs[(a[8] - 1) & 1] if a[8] else 0
            park = self.parks[(a[9] - 1) & 1] if a[9] else 0
            self.islands[slot] = block
            self.island_alive[slot] = True
            self.call(U["island_ctor"], block, self.screens[a[1] & 1], a[2], self.text(name), self.text(folder), self.text(mesh_a),
                      self.text(mesh_b), T.F(bf(a[3])), position, CL.new_wide(e, title), park, sign1, sign2)
        elif op == 4:
            self.call(U["Register"], screen, self.islands[a[1] & 7])
        elif op == 5:
            self.call(U["UnRegister"], screen, self.islands[a[1] & 7])
        elif op == 6:
            self.call(U["SetCurrentIsland"], screen, 0 if a[1] < 0 else self.islands[a[1] & 7])
        elif op == 7:
            self.virtual(screen, 0xC)
        elif op == 8:
            self.virtual(screen, 0x14)
        elif op == 9:
            self.virtual(screen, 0x18)
        elif op == 10:
            self.virtual(screen, 0x24)
        elif op == 11:
            self.virtual(screen, 0x28)
        elif op == 12:
            self.call(U["Show"], screen)
        elif op == 13:
            self.call(U["Hide"], screen)
        elif op == 14:
            self.call(U["DeleteIslandListEntries"], screen)
        elif op == 15:
            self.call(U["ShowPanels"], screen)
        elif op == 16:
            self.call(U["HidePanels"], screen)
        elif op == 17:
            w32(e, screen + 4, a[1])
        elif op == 18:
            w32(e, screen + 0xC, 0 if a[1] < 0 else self.islands[a[1] & 7])
        elif op == 19:
            w32(e, self.islands[a[0] & 7] + 0x1C, a[1])
            w32(e, self.islands[a[0] & 7] + 0x20, a[2])
        elif op == 20:
            w32(e, G["online"], self.online if a[0] else 0)
        elif op == 21:
            self.call(U["Register"], screen, 0)
        else:
            return -99
        return 0

    def state(self):
        e, L = self.e, self.L
        s32 = T.s32
        out = [e.u32(self.view + o) for o in (8, 0xC, 0x10, 0x14, 0x18, 0x1C, 4, 0x28, 0x2C)] + [s32(e.u32(self.view + 0x30))]
        out.append(L.lab(e.u32(G["online"])))
        for i in range(SCREENS):
            if not self.screen_alive[i]:
                continue
            block = self.screens[i]
            lst = e.u32(block + 8)
            out += [i, s32(e.u32(block + 4)), s32(e.u32(lst + 0xC))]
            node, n = e.u32(lst + 4), 0
            while node and n < 40:
                out.append(self.island_slot(e.u32(node)))
                node, n = e.u32(node + 4), n + 1
            out += [self.island_slot(e.u32(block + 0xC)), self.island_slot(e.u32(block + 0x10)), s32(e.u32(block + 0x14))]
            out += [e.u32(block + o) for o in range(0x18, 0xE8, 4)]
        for i in range(ISLANDS):
            if not self.islands[i] or not self.island_alive[i]:
                continue
            b = self.islands[i]
            out += [i, L.lab(e.u32(b + 4)), L.lab(e.u32(b + 8)), self.screen_slot(e.u32(b + 0xC)), s32(e.u32(b + 0x10)),
                    e.u32(b + 0x14), s32(e.u32(b + 0x18)), s32(e.u32(b + 0x1C)), s32(e.u32(b + 0x20)), s32(e.u32(b + 0x24)),
                    hash_text(FS.s_read(e, b + 0x28, False)), e.u32(b + 0x3C), e.u32(b + 0x40), e.u32(b + 0x44),
                    hash16(FS.s_read(e, b + 0x48, True)), s32(e.u32(b + 0x5C)), s32(e.u32(b + 0x60)), e.u32(b + 0x64),
                    e.u32(b + 0x68), e.u32(b + 0x6C), L.lab(e.u32(b + 0x70))]
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "new"))
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "delete" and address))
        return out


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    if not FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


def islands_ppc(e, case):
    saved = STUBS.activate(e)
    side = None
    try:
        side = PpcSide(e, case)
        answers, states, marks = [], [], []
        for op, a, texts in case["script"]:
            answers.append(side.op(op, list(a), texts))
            states.append(side.state())
            marks.append(len(side.L.log))
        return dict(answers=answers, states=states, marks=marks, log=side.L.log)
    finally:
        if side is not None:
            side.finish()
        CL.Stubs.deactivate(e, saved)


def islands_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    CL.host_begin(lib, {}, case.get("q"))
    F(lib, "tpw_li_begin", None, C.POINTER(C.c_uint32), C.c_char_p)(
        (C.c_uint32 * WORLD_WORDS)(*case["world"]), "\n".join(case["themes"]).encode("latin-1"))
    op_fn = F(lib, "tpw_li_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32), C.c_char_p)
    state_fn = F(lib, "tpw_li_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 2048)()
    try:
        for op, a, texts in case["script"]:
            answers.append(op_fn(op, (C.c_int32 * 12)(*[T.s32(v & MINUS) for v in a]), texts.encode("latin-1")))
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, marks=marks, log=log)


# ---- the scripts


class Script:
    """Builds a script and keeps its model: per screen the visible word, the listed islands in
    order and the current island; per island whether it is alive, its order and where it is listed.
    `rc` is the ResetCurrent queue the case will use (a Show consumes one entry)."""

    def __init__(self, rc=()):
        self.steps = []
        self.alive_screen = [False, False]
        self.visible = [0, 0]
        self.listed = [[], []]
        self.current = [None, None]
        self.made = [False] * ISLANDS
        self.alive = [False] * ISLANDS
        self.order = [0] * ISLANDS
        self.where = [None] * ISLANDS
        self.rc = list(rc)
        self.rc_pos = 0

    def add(self, op, *a, texts=""):
        self.steps.append((op, (list(a) + [0] * 12)[:12], texts))

    def next_rc(self):
        if not self.rc:
            return -1
        value = self.rc[min(self.rc_pos, len(self.rc) - 1)]
        self.rc_pos += 1
        return value

    # screens
    def make_screen(self, s):
        self.add(1, s)
        self.alive_screen[s] = True
        self.visible[s] = 0
        self.listed[s] = []
        self.current[s] = None

    def kill_islands(self, s):
        """The islands listed in screen s are destroyed; the current island is cleared when one of them."""
        dead = self.listed[s]
        self.listed[s] = []
        for i in dead:
            self.alive[i] = False
            self.where[i] = None
        if self.current[s] in dead:
            self.current[s] = None
            if self.alive_screen[s]:
                self.add(18, s, -1)

    def destroy(self, s, flag):
        self.add(2, s, flag)
        self.alive_screen[s] = False
        for i in self.listed[s]:
            self.alive[i] = False
            self.where[i] = None
        self.listed[s] = []

    # islands
    def insert_sorted(self, s, i):
        position = 0
        while position < len(self.listed[s]) and signed(self.order[self.listed[s][position]]) < signed(self.order[i]):
            position += 1
        self.listed[s].insert(position, i)
        self.where[i] = s

    def make_island(self, i, s, order, angle, position, sign1, sign2, park, name, folder, mesh_a, mesh_b, title):
        self.add(3, i, s, order, angle, position[0], position[1], position[2], sign1, sign2, park,
                 texts="\n".join([name, folder, mesh_a, mesh_b, title]))
        self.made[i] = self.alive[i] = True
        self.order[i] = order & MINUS
        self.insert_sorted(s, i)

    def register(self, s, i):
        self.add(4, s, i)
        self.insert_sorted(s, i)

    def unregister(self, s, i):
        self.add(5, s, i)
        if self.where[i] == s:
            self.listed[s].remove(i)
            self.where[i] = None

    def set_current(self, s, i):
        self.add(6, s, -1 if i is None else i)
        if i is not None:
            self.current[s] = i

    def force_current(self, s, i):
        self.add(18, s, -1 if i is None else i)
        self.current[s] = i

    def show(self, s):
        self.add(10, s)
        if self.visible[s] == 0:
            self.visible[s] = 1
            self.current[s] = None
            k = self.next_rc()
            if 0 <= k < len(self.listed[s]):
                self.current[s] = self.listed[s][k]

    def hide(self, s):
        self.add(11, s)
        if self.visible[s] != 0:
            self.visible[s] = 0
            self.kill_islands(s)

    def base_show(self, s):
        self.add(12, s)
        if self.visible[s] == 0:
            self.visible[s] = 1
            self.current[s] = None

    def base_hide(self, s):
        self.add(13, s)
        if self.visible[s] != 0:
            self.visible[s] = 0

    def delete_entries(self, s):
        self.add(14, s)
        self.kill_islands(s)

    def set_visible(self, s, v):
        self.add(17, s, v)
        self.visible[s] = v


def signed(v):
    v &= MINUS
    return v - (1 << 32) if v & 0x80000000 else v


EDGE_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 90.0, 180.0, 360.0, 1e-30, -1e-30, 1e30, -3.4e38, 12345.678, 500.0, 75.0]


def rbits(r):
    """Random float bits for a value that is copied, not computed on: any pattern, NaNs included."""
    k = r.random()
    if k < 0.2:
        return CL.fbits(r.choice(EDGE_FLOATS))
    if k < 0.7:
        return CL.fbits(T.rfloat_mod(r, -1000.0, 1000.0))
    return r.getrandbits(32)


def rangle(r):
    """Random degrees: any float but a NaN."""
    k = r.random()
    if k < 0.2:
        return CL.fbits(r.choice(EDGE_FLOATS))
    return CL.fbits(T.rfloat(r))


def rorder(r):
    return r.choice([0, 1, 2, 3, 3, 5, 5, 10, 100, -1, -5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.randrange(8)]) & MINUS


def rworld(r):
    return [rbits(r) for _ in range(6)] + [rbits(r), rbits(r), r.randrange(2)] + [r.getrandbits(32) for _ in range(8)]


EDGE_RANDS = [0, 1, 2, 0x3FFFFFFF, 0x40000000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x00FFFFFF, 0x01000001, 0x12345678]


def rrand_queue(r):
    k = r.random()
    if k < 0.12:
        return [r.choice(EDGE_RANDS)] * 24
    if k < 0.25:
        return [r.choice(EDGE_RANDS) if r.random() < 0.5 else r.getrandbits(32) for _ in range(24)]
    return [r.getrandbits(32) for _ in range(24)]


def make_world(r, themes=None, queues=None):
    return dict(world=rworld(r), themes=THEMES[:r.randrange(len(THEMES) + 1)] if themes is None else themes,
                q=queues if queues is not None else {
                    "Rand": rrand_queue(r), "rand": [r.choice([r.getrandbits(32), r.randrange(0x8000), 0xFF, 0x100, 0xFFFFFFFF, 0]) for _ in range(12)],
                    "SND_RegisterLocalLobbySounds": [r.randrange(4) for _ in range(6)],
                    "SOUND_SendEvent": [r.choice([r.randrange(-3, 50), r.randrange(-2 ** 31, 2 ** 31)]) for _ in range(8)],
                    "ResetCurrent": [r.choice([-1, 0, 0, 1, 2, 9]) for _ in range(4)]})


def random_script(r, rc):
    s = Script(rc)
    for slot in range(SCREENS):
        if r.random() < (1.0 if slot == 0 else 0.4):
            s.make_screen(slot)
    next_island = 0
    for _ in range(r.randrange(8, 40)):
        live = [x for x in range(SCREENS) if s.alive_screen[x]]
        if not live:
            break
        sc = r.choice(live)
        k = r.random()
        if k < 0.20 and next_island < ISLANDS:
            s.make_island(next_island, sc, rorder(r), rangle(r), [rbits(r), rbits(r), rbits(r)], r.randrange(3), r.randrange(3),
                          r.randrange(3), r.choice(NAMES), r.choice(FOLDERS), r.choice(MESHES), r.choice(MESHES), r.choice(TITLES))
            next_island += 1
        elif k < 0.27:
            free = [i for i in range(ISLANDS) if s.alive[i] and s.where[i] is None]
            if free:
                s.register(sc, r.choice(free))
        elif k < 0.33:
            alive = [i for i in range(ISLANDS) if s.alive[i]]
            if alive:
                s.unregister(sc, r.choice(alive))
        elif k < 0.40:
            alive = [i for i in range(ISLANDS) if s.alive[i]]
            s.set_current(sc, r.choice(alive) if alive and r.random() < 0.8 else None)
        elif k < 0.44:
            s.add(r.choice([7, 8, 9]), sc)
        elif k < 0.54:
            s.show(sc)
        elif k < 0.62:
            s.hide(sc)
        elif k < 0.67:
            s.base_show(sc)
        elif k < 0.71:
            s.base_hide(sc)
        elif k < 0.75:
            s.delete_entries(sc)
        elif k < 0.79:
            s.add(r.choice([15, 16]), sc)
        elif k < 0.83:
            s.set_visible(sc, r.choice([0, 1, 1, 2, -1]))
        elif k < 0.89:
            alive = [i for i in range(ISLANDS) if s.alive[i]]
            if alive:
                s.add(19, r.choice(alive), r.choice([0, 0, 1, 7, -3]), r.choice([0, 1, 9, 0x7FFFFFFF]))
        elif k < 0.93:
            s.add(20, r.randrange(2))
        elif k < 0.96:
            s.destroy(sc, r.choice([0, 1]))
    if r.random() < 0.5:
        for sc in range(SCREENS):
            if s.alive_screen[sc]:
                s.destroy(sc, r.choice([0, 1]))
    return s


def islands_case(r):
    case = make_world(r)
    case["script"] = random_script(r, case["q"]["ResetCurrent"]).steps
    return case


def edge_cases():
    r = random.Random(41)
    base = lambda **q: make_world(r, queues=q)   # noqa: E731
    pos = lambda *v: [CL.fbits(x) for x in v]    # noqa: E731

    def island(s, i, sc, order, **kw):
        s.make_island(i, sc, order, kw.get("angle", CL.fbits(90.0)), kw.get("pos", pos(10.0, 5.0, -20.0)), kw.get("s1", 0),
                      kw.get("s2", 0), kw.get("park", 0), kw.get("name", "Isle"), "data:lobby:", "rock", "rock_b", kw.get("title", "T"))

    # The constructor and the destructor: the camera starts at the view, the random flights, the zero direction.
    for rands in ([0] * 24, [5, 6, 7, 5, 9, 10, 1, 2, 3, 1, 4, 5], [5, 6, 7, 5, 6, 10, 1, 2, 3, 1, 2, 3], [5, 6, 7, 5, 6, 7, 1, 2, 3, 4, 5, 6],
                  [0x3FFFFFFF, 0x40000000, 0xFFFFFFFF, 0x7FFFFFFF, 1, 2] * 4, [0x00FFFFFF, 0x01000001, 0x01000000] * 8):
        s = Script()
        s.make_screen(0)
        s.destroy(0, 0)
        s.make_screen(1)
        s.destroy(1, 1)
        c = base(Rand=rands)
        c["script"] = s.steps
        yield c
    # Init, Render, Close; the panels.
    s = Script()
    s.make_screen(0)
    for op in (7, 8, 9, 15, 16, 7):
        s.add(op, 0)
    c = make_world(r)
    c["script"] = s.steps
    yield c
    # The list is kept in order of the islands' order (signed), ties go in front of the one already there.
    s = Script()
    s.make_screen(0)
    for i, order in enumerate([5, 3, 9, 3, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 5]):
        island(s, i, 0, order, name="n%d" % i, s1=i % 3, s2=(i + 1) % 3, park=i % 3)
    s.destroy(0, 1)
    c = make_world(r)
    c["script"] = s.steps
    yield c
    # UnRegister (listed here, listed elsewhere, not listed), Register again, SetCurrentIsland (NULL changes nothing).
    s = Script()
    s.make_screen(0)
    s.make_screen(1)
    for i, order in enumerate([4, 2, 8]):
        island(s, i, 0, order)
    island(s, 3, 1, 1)
    s.set_current(0, None)
    s.set_current(0, 1)
    s.set_current(0, None)
    s.unregister(0, 3)
    s.unregister(1, 2)
    s.unregister(0, 1)
    s.unregister(0, 1)
    s.register(1, 1)
    s.register(0, 3)
    s.set_current(1, 3)
    s.unregister(0, 0)
    s.unregister(0, 2)
    s.unregister(0, 3)
    s.register(0, 2)
    s.delete_entries(0)
    s.delete_entries(1)
    s.destroy(0, 0)
    s.destroy(1, 1)
    c = make_world(r)
    c["script"] = s.steps
    yield c
    # A NULL island in the list (only into an empty list): the list can still be emptied.
    s = Script()
    s.make_screen(0)
    s.add(21, 0)
    s.add(14, 0)
    s.add(21, 0)
    s.add(2, 0, 0)
    c = make_world(r)
    c["script"] = s.steps
    yield c
    # The theme islands' Hide meets a NULL entry in the list: nothing to destroy, and the loop goes on.
    s = Script()
    s.make_screen(0)
    s.add(21, 0)
    s.set_visible(0, 1)
    s.hide(0)
    s.add(21, 0)
    s.show(0)
    s.hide(0)
    s.destroy(0, 1)
    c = make_world(r)
    c["script"] = s.steps
    yield c
    # The base class's Show and Hide, with and without the online system, with a current island.
    for online in (0, 1):
        for visible_before in (0, 1):
            s = Script()
            s.make_screen(0)
            island(s, 0, 0, 1)
            island(s, 1, 0, 2)
            s.force_current(0, 1)
            s.add(19, 1, 0, 5)
            s.set_visible(0, visible_before)
            s.base_show(0)
            s.add(19, 1, 3, 4)
            s.force_current(0, 1)
            s.base_hide(0)
            s.base_hide(0)
            s.force_current(0, None)
            s.set_visible(0, 1)
            s.base_hide(0)
            s.base_show(0)
            s.add(20, online)
            s.set_visible(0, 0)
            s.base_show(0)
            c = make_world(r)
            c["world"][8] = online
            c["script"] = s.steps
            yield c
    # The theme islands' Show and Hide: no themes, themes, a current island picked by ResetCurrentIsland or not.
    for themes in ([], ["jungle"], THEMES[:3]):
        for rc in ([-1], [0], [1], [5], [2, 0, -1]):
            s = Script(rc)
            s.make_screen(0)
            island(s, 0, 0, 1)
            island(s, 1, 0, 2)
            island(s, 2, 0, 3)
            s.show(0)
            s.show(0)
            s.hide(0)
            s.hide(0)
            island(s, 3, 0, 7)
            s.show(0)
            if s.current[0] == 3:
                s.add(19, 3, 11, 12)
            s.show(0)
            s.hide(0)
            s.show(0)
            s.destroy(0, 0)
            c = make_world(r, themes=themes)
            c["q"]["ResetCurrent"] = rc
            c["script"] = s.steps
            yield c
    # Hide with the current island holding sounds, or none, listed or not.
    s = Script()
    s.make_screen(0)
    island(s, 0, 0, 1)
    island(s, 1, 0, 2)
    s.unregister(0, 1)
    s.add(19, 0, 5, 6)
    s.add(19, 1, 7, 8)
    s.force_current(0, 1)
    s.set_visible(0, 1)
    s.hide(0)
    s.force_current(0, 0)
    s.set_visible(0, 1)
    s.add(19, 0, 0, 0)
    s.base_hide(0)
    s.set_visible(0, 1)
    s.hide(0)
    c = make_world(r)
    c["script"] = s.steps
    yield c
    # IsVisible answering for itself: the screen shown when the first answer says so, or the second.
    for answers in ([1], [0, 1], [0, 0], [1, 0, 1], [0, 1, 0, 1, 1], [2], [-1, 0]):
        for op in (10, 11, 12, 13):
            s = Script()
            s.make_screen(0)
            island(s, 0, 0, 1)
            island(s, 1, 0, 2)
            s.force_current(0, 0)
            s.add(op, 0)
            s.steps.append((18, [0, -1] + [0] * 10, ""))     # (the current island may be gone)
            s.add(op, 0)
            s.add(20, 0)
            s.add(op, 0)
            c = make_world(r, themes=THEMES[:2])
            c["q"]["IsVisible"] = answers
            c["q"]["ResetCurrent"] = [0]
            c["script"] = s.steps
            yield c
    # The island's constructor on its own: arguments, the random sky colour, the angle.
    for angle in (0.0, -0.0, 1.0, 45.0, 90.0, 180.0, 360.0, -90.0, 1e30, 1e-45, 3.0e38):
        s = Script()
        s.make_screen(0)
        island(s, 0, 0, 0, angle=CL.fbits(angle), pos=[0x7FC00000, 0x80000000, 0xFFFFFFFF], s1=1, s2=2, park=2, title="Wide ti", name="x")
        s.make_island(1, 0, 0, CL.fbits(angle), pos(1.5, 2.5, 3.5), 2, 1, 1, "", "", "", "", "")
        c = base(Rand=[1] * 24, rand=[0, 0xFF, 0x100, 0xFFFFFFFF, 0x12345678, 0x7FFF, 0x80000000, 1],
                 SND_RegisterLocalLobbySounds=[0, 3, 1], SOUND_SendEvent=[0], ResetCurrent=[-1])
        c["script"] = s.steps
        yield c
    for _ in range(40):
        yield islands_case(r)


TESTS = [FnTest("CLBIsland and CLBScreenComponentIslands (set-up, list, show, hide)", islands_ppc, islands_host,
                edge=edge_cases, rnd=islands_case)]
