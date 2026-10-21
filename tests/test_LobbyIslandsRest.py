"""Differential test for src/game/online/LBFlyingMesh.cpp, src/game/online/lobby_island.cpp, src/game/online/lobby_parser.cpp (the lobby's island screen, part 3: the theme
descriptor parser and the rest of the island and theme island members), against
libtpw_unit_LobbyIslandsRest.dylib (tests/unit_LobbyIslandsRest.cpp).

Functions: CLBScreenComponentIslands::ParseThemeDescriptor and ResetIslandLoopingSounds; CLBIsland's
destructor and Show; CLBLobby::GetRand; the theme islands' WindowMessagesFn, ProcessCancel,
SelectCurrentIsland, StartCurrentIslandZoom, EnterCurrentIsland, SetCurrentIsland, GotoNextIsland,
GotoPreviousIsland, ResetCurrentIsland, ResetAdvisorTimeout, UpdateSigns and destructor; and
LB_ReadTbUniStringUntilNext.

A case is a world and a script. The world: the lobby (its generator's seed and state word, the
description's camera places), the lobby view, the sound table, the game's themes and the text of each
theme's description file, the game type, the user info and the master save (each there or not), the
theme the player last entered, and the queues the fakes answer from. The script: make a theme-island
screen (the Mac's way: a block of 0xa5, the base constructor, the derived vtable), make islands with the
real constructor, read theme descriptions, show and hide the screen, and call the functions of the unit.
Islands are named by their place in the screen's list. After every operation: the calls the fakes
recorded, the view, the lobby's words, the user info, the master save and the remembered theme, every
live screen (its list, every island in it, current and last island, every camera word) and how many
blocks operator new and delete have handled.

On the PowerPC side the unit's functions, the screen's set-up, list, show and hide, the island's
constructor, the lobby description's string reader and the list instances are the original's code; the
screen base class, the view, the meshes, the flying mesh groups, the sounds, the panels, the master
save, the user info, the game, the files and the C library are recorders (or, for the strings, functional
fakes). The scripts keep to what the original survives: they skip an operation (on both sides, with the
same answer) that would read through a NULL pointer or a freed island.
"""
import ctypes as C
import random
import re
import struct

import calllog as CL
import test_FSSystem as FS
import test_LobbyIslands as LI
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyIslandsRest"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F
hash_text = LI.hash_text
bf = LI.bf

# The unit's functions.
M = dict(parse=0x100B5D54, island_show=0x100B01C0, island_dtor=0x100B0108, get_rand=0x100AA98C, reset_sounds=0x100B2228,
         set_current=0x100B3540, enter=0x100B346C, select=0x100B3218, zoom=0x100B33E8, reset_current=0x100B3760,
         reset_timeout=0x100B3854, update_signs=0x100B38D4, theme_dtor=0x100B4CEC, goto_next=0x100B35AC,
         goto_previous=0x100B3684, window=0x100B2AF8, cancel=0x100B2C00, read_uni=0x100B69F8)
READ_STRING = 0x100B5C78       # LB_ReadTbStringUntilNext (src/game/online/lobby_parser.cpp), the original's code here too
U = LI.U
G = dict(LI.G, game_type=0x102B3AA0, game_guard=0x102B3AA4, user=0x10380B34, master=0x10380A8C, theme=0x102E48F8,
         folder=0x102E4978)
FILE = 0x7C0FD400
THEME_VTABLE = LI.THEME_VTABLE
SCREENS = 2
L_SCREEN, L_LOBBY, L_GLOBALS, L_VIEW, L_MESHES, L_FLYING, L_RAIN, L_ONLINE, L_GAME, L_FILE, L_USER, L_MASTER = (
    1, 3, 4, 5, 6, 7, 8, 9, 18, 20, 21, 22)
L_SIGN, L_ONLINE_PARK, L_PARK_DATA = 70, 80, 90

# The words of a world.
W_VIEW, W_VIEW_FOV, W_DESCRIPTION_FOV, W_ONLINE, W_SOUNDS, W_CAMERA_X, W_CAMERA_Y = 0, 6, 7, 8, 9, 17, 27
W_SEED, W_LOBBY_STATE, W_GAME_TYPE, W_USER, W_USER_EXISTS, W_MASTER_EXISTS, W_GUARD = 37, 38, 39, 40, 41, 42, 43
WORLD_WORDS = 44

NAMES = ["Alpha", "bravo", "Charlie Isle", "d", "Echo5", "ClearMaster", "alpha"]
FOLDERS = ["rock", "Rock/b", "data\\x\\y", "a/b\\c/d", "f", "", "dir name", "UPPER"]
MESHES = ["rock", "Rock_B", "m1", "island_a", "", "x y"]
TITLES = ["", "Spooky", "Sunny Park", "zz", "caf\xe9 \xff\x80", "a,b", "A very long title for a very long island"]
THEMES = ["halloween", "space", "Hollywood", "jungle", "Fantasy", "ClearMaster"]


def s32(v):
    return T.s32(v & MINUS)


# ---- the C library's number readers, as the parser test has them


NUMBER = re.compile(r"\s*[+-]?(\d+\.?\d*([eE][+-]?\d+)?|\.\d+([eE][+-]?\d+)?)")
INTEGER = re.compile(r"\s*[+-]?\d+")


def c_atof(text):
    found = NUMBER.match(text)
    return float(found.group(0)) if found else 0.0


def c_atoi(text):
    found = INTEGER.match(text)
    return int(found.group(0)) if found else 0


def text_at(e, address):
    return e.read(address, 600).split(b"\0")[0]


# ---- the PowerPC side's fakes


def build_stubs(emu):
    S = LI.build_stubs(emu)
    R = CL.recorder
    # The unit's functions run as the original's code; so do the parser's string reader and everything
    # src/game/online/lobby_island.cpp has.
    for address in list(M.values()) + [READ_STRING]:
        S.internals.pop(address, None)
        S.real.add(address)
    # The compare, the number readers and the string pieces the parser uses.
    text = lambda e, a: e.read(a, 600).split(b"\0")[0]   # noqa: E731

    def strncmp(e):
        a, b = text(e, e.r(3))[:e.r(5)], text(e, e.r(4))[:e.r(5)]
        e.set_r(3, T.u32((a > b) - (a < b)))

    S.imp("strncmp", strncmp)
    S.imp("atof", lambda e: e.set_f1(c_atof(text(e, e.r(3)).decode("latin-1"))))
    S.imp("atoi", lambda e: e.set_r(3, T.u32(max(-0x80000000, min(0x7FFFFFFF, c_atoi(text(e, e.r(3)).decode("latin-1")))))))
    S.imp("__apl__26TbDynamicStringTemplate<c>FRCc",
          lambda e: FS.s_write(e, e.r(3), FS.s_read(e, e.r(3), False) + e.read(e.r(4), 1).decode("latin-1"), False))
    S.imp("__apl__26TbDynamicStringTemplate<w>FRCw",
          lambda e: FS.s_write(e, e.r(3), FS.s_read(e, e.r(3), True) + chr(struct.unpack(">H", e.read(e.r(4), 2))[0]), True))
    S.imp("__as__26TbDynamicStringTemplate<c>FPCc", lambda e: FS.s_write(e, e.r(3), e.cstr(e.r(4), limit=400), False))
    S.imp("__as__26TbDynamicStringTemplate<w>FPCw", lambda e: FS.s_write(e, e.r(3), FS.wide_read(e, e.r(4)), True))
    S.imp("LockBuffer__15TbStringBase<c>FUi", lambda e: e.set_r(3, e.u32(e.r(3) + 8)))
    S.imp("ReleaseBuffer__15TbStringBase<c>FUi", lambda e: None)
    S.imp("_stricmp__10NS_MacDozeFPCcPCc",
          lambda e: e.set_r(3, T.u32(FS.icmp(e.cstr(e.r(3), limit=400), e.cstr(e.r(4), limit=400)))))
    S.imp("LbTime_GetClock__Fv", R("LbTime_GetClock", "", lambda e, L: L.ret("Clock", 0) & MINUS))

    def alloc(e):
        size = e.r(3)
        block = e.alloc(size + 1)
        e.cl.rec("LbMem_Alloc", (size, e.cl.newlab(block)))
        e.set_r(3, block)

    def free(e):
        e.cl.rec("LbMem_Free", (e.cl.lab(e.r(3)),))
        e.cl.forget(e.r(3))

    S.imp("LbMem_Alloc__FUi", alloc)
    S.imp("LbMem_Free__FPv", free)

    # The files: one text per theme, found by the name TPFile_Open is given.
    def file_open(e):
        name = e.cstr(e.r(3))[:159]
        e.cl.rec("TPFile_Open", (), (name, e.cstr(e.r(4))[:159]))
        e.li.file_text = b""
        for theme, content in e.li.files:
            if name == "data:lobby:" + theme + ".txt":
                e.li.file_text = content
                break
        e.set_r(3, FILE)

    def file_read(e):
        e.cl.rec("TPFile_Read", (e.cl.lab(e.r(3)), e.r(4), e.r(5), e.cl.lab(e.r(6))))
        if e.li.file_text:
            e.write(e.r(3), e.li.file_text)
        e.set_r(3, e.r(5))

    S.internal(0x10003FD4, file_open)
    S.internal(0x100043EC, R("TPFile_Seek", "Luu", lambda e, L: 0))
    S.internal(0x100044E8, R("TPFile_Size", "L", lambda e, L: len(e.li.file_text)))
    S.internal(0x10004260, file_read)
    S.internal(0x10004354, R("TPFile_Close", "L", lambda e, L: 0))

    # The mesh instances, the flying mesh groups, the base class's moves, the sparkle.
    S.internal(0x100A7860, R("CLBMeshInstance.Show", "L"))
    S.internal(0x100A7D28, R("CLBMeshInstance.PlayAnim", "Lii"))
    S.internal(0x100A7E84, R("CLBMeshInstance.SetRideSigns", "LSS"))
    S.internal(0x100B2638, R("Islands.GotoNextIsland", "L"))
    S.internal(0x100B2808, R("Islands.GotoPreviousIsland", "L"))
    S.internal(0x101E1460, R("KeyParticlePOUFFE"))

    def group_ctor(e):
        L = e.cl
        position, direction = struct.unpack(">3I", e.read(e.r(7), 12)), struct.unpack(">3I", e.read(e.r(8), 12))
        L.rec("CLBFlyingMeshGroup.ctor", (L.newlab(e.r(3)), T.s32(e.r(4))) + position + direction + (CL.fbits(e.f(1)), CL.fbits(e.f(2))),
              (e.cstr(e.r(5))[:159], e.cstr(e.r(6))[:159]))

    S.internal(0x100AAE1C, group_ctor)
    S.internal(0x100AB2FC, R("CLBFlyingMeshGroup.Show", "L"))

    # The game, the master save, the user info, the game type.
    S.internal(0x10149C7C, R("CGame.SetCurrentTheme", "LT"))

    def get_info(e):
        L = e.cl
        name = FS.s_read(e, e.r(4), False)
        L.rec("CFSGlobalMasterSave.GetInfoOn", (L.lab(e.r(3)),), (name[:159],))
        if name == "ClearMaster":    # the test's hook: the master save is gone when GetInfoOn returns
            w32(e, G["master"], 0)
        if name == e.li.missing:
            e.set_r(3, 0)
        else:
            e.set_r(3, e.li.park_data[hash_text(name) % 3])

    S.internal(0x1016B47C, get_info)
    S.internal(0x101694DC, R("CFSGlobalMasterSave.ctor", "N"))
    S.internal(0x10169EA0, R("CFSGlobalMasterSave.HowManyKeys", "L", lambda e, L: L.ret("Keys", 0) & MINUS))
    S.internal(0x1016C3BC, R("CFSGlobalParkData.KeyCostToEnter", "L",
                             lambda e, L: L.ret("Cost:%d" % L.lab(e.r(3)), 0) & MINUS))

    def sign(letter):
        def fn(e):
            L = e.cl
            label, size = L.lab(e.r(3)), e.r(5)
            L.rec("CFSGlobalParkData.GetSignName" + letter, (label, size))
            chars = ("S%s%d" % (letter, label))[:max(size - 1, 0)]
            e.write(e.r(4), b"".join(struct.pack(">H", ord(c)) for c in chars) + b"\0\0")
        return fn

    S.internal(0x1016C7E8, sign("A"))
    S.internal(0x1016CA34, sign("B"))

    def user_ctor(e):
        e.cl.rec("CFSUserInfo.ctor", (e.cl.newlab(e.r(3)),))
        w32(e, e.r(3) + 0x60, e.li.user)

    S.internal(0x1017DE54, user_ctor)
    S.internal(0x1016F5D8, lambda e: w32(e, e.r(3), e.li.game_type))
    return S


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        e.li = self
        w = case["world"]
        self.game_type, self.user, self.missing = w[W_GAME_TYPE] & MINUS, w[W_USER] & MINUS, case["missing"]
        self.files = [(theme, text) for theme, text in case["files"]]
        self.themes = list(case["themes"])
        self.file_text = b""
        self.lobby = e.alloc(0x38, b"\xa5" * 0x38)
        self.globals = e.alloc(0x6C, b"\xa5" * 0x6C)
        self.view = e.alloc(0x64, b"\xa5" * 0x64)
        self.meshes, self.flying = e.alloc(16), e.alloc(16)
        self.rain = e.alloc(16)
        self.online = e.alloc(0x404, b"\xa5" * 0x404)
        self.signs = [CL.new_wide(e, "AB"), CL.new_wide(e, "cde")]
        self.parks = [e.alloc(16), e.alloc(16)]
        self.park_data = [e.alloc(16) for _ in range(3)]
        w32(e, self.lobby + 0x34, w[W_SEED])
        w32(e, self.lobby + 0x14, w[W_LOBBY_STATE])
        w32(e, self.lobby + 0x10, self.globals)
        w32(e, self.globals + 4, w[W_DESCRIPTION_FOV])
        e.write(self.globals + 0x1C, struct.pack(">10I", *w[W_CAMERA_X:W_CAMERA_X + 10]))
        e.write(self.globals + 0x44, struct.pack(">10I", *w[W_CAMERA_Y:W_CAMERA_Y + 10]))
        e.write(self.view + 4, struct.pack(">I", w[W_VIEW_FOV]))
        e.write(self.view + 8, struct.pack(">6I", *w[W_VIEW:W_VIEW + 6]))
        w32(e, G["lobby"], self.lobby)
        w32(e, G["view"], self.view)
        w32(e, G["meshes"], self.meshes)
        w32(e, G["flying"], self.flying)
        w32(e, e.data_import("gei_Rain"), self.rain)
        w32(e, G["online"], self.online if w[W_ONLINE] else 0)
        table = e.alloc(32, struct.pack(">8I", *w[W_SOUNDS:W_SOUNDS + 8]))
        w32(e, G["sounds"], table)
        nodes = []
        for text in case["themes"]:
            string = FS.new_string(e, text, False)
            nodes.append((e.alloc(12), string))
        for i, (node, string) in enumerate(nodes):
            nxt = nodes[i + 1][0] if i + 1 < len(nodes) else 0
            prv = nodes[i - 1][0] if i else 0
            e.write(node, struct.pack(">III", string, nxt, prv))
        w32(e, G["game"] + 0x28, nodes[0][0] if nodes else 0)
        w32(e, G["game"] + 0x2C, nodes[-1][0] if nodes else 0)
        w32(e, G["game"] + 0x30, len(nodes))
        FS.s_write(e, G["folder"], "data:lobby:", False)
        FS.s_write(e, G["theme"], case["theme"], False)
        e.write(G["game_guard"], bytes([w[W_GUARD] & 0xFF]))
        w32(e, G["game_type"], self.game_type)
        fixed = {self.lobby: L_LOBBY, self.globals: L_GLOBALS, self.view: L_VIEW, self.meshes: L_MESHES, self.flying: L_FLYING,
                 self.rain: L_RAIN, self.online: L_ONLINE, G["game"]: L_GAME, FILE: L_FILE,
                 self.signs[0]: L_SIGN, self.signs[1]: L_SIGN + 1, self.parks[0]: L_ONLINE_PARK, self.parks[1]: L_ONLINE_PARK + 1}
        for i, block in enumerate(self.park_data):
            fixed[block] = L_PARK_DATA + i
        if w[W_USER_EXISTS]:
            self.make_user(self.user)
            fixed[u32_of(e, G["user"])] = L_USER
        else:
            w32(e, G["user"], 0)
        if w[W_MASTER_EXISTS]:
            block = e.alloc(0x4C, b"\xa5" * 0x4C)
            w32(e, G["master"], block)
            fixed[block] = L_MASTER
        else:
            w32(e, G["master"], 0)
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        self.screens = [0] * SCREENS
        self.screen_alive = [False] * SCREENS
        del e.alloc_log[:]

    def make_user(self, user):
        block = self.e.alloc(0xD4, b"\xa5" * 0xD4)
        w32(self.e, block + 0x60, user)
        w32(self.e, G["user"], block)
        return block

    def finish(self):
        """Leaves the globals this case set as it found them (zero), so other modules see none of it."""
        e = self.e
        for name in ("lobby", "view", "meshes", "flying", "online", "sounds", "user", "master", "game_type"):
            w32(e, G[name], 0)
        e.write(G["game_guard"], b"\0")
        e.write(G["theme"], b"\0" * 0x14)
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

    def screen_slot(self, address):
        if address == 0:
            return -1
        return self.screens.index(address) if address in self.screens else -9

    def list_nodes(self, screen):
        e = self.e
        nodes, node = [], e.u32(e.u32(screen + 8) + 4)
        while node and len(nodes) < 40:
            nodes.append(node)
            node = e.u32(node + 4)
        return nodes

    def island_at(self, screen, k):
        e = self.e
        count = s32(e.u32(e.u32(screen + 8) + 0xC))
        if count <= 0:
            return 0
        node = e.u32(e.u32(screen + 8) + 4)
        for _ in range((k & MINUS) % count):
            if not node:
                break
            node = e.u32(node + 4)
        return e.u32(node) if node else 0

    def signs_would_fail(self, screen, themes):
        """Whether UpdateSigns (or, with `themes`, a Show, which makes the islands of the game's themes first)
        would ask the master save for a theme it misses while a player is logged in: the original then calls
        through NULL. The user info is made by the first call if it is not there: the world's user then."""
        e = self.e
        user = e.u32(G["user"])
        if (s32(e.u32(user + 0x60)) if user else s32(self.user)) == -1:
            return False
        if any(FS.s_read(e, e.u32(node) + 0x28, False) == self.missing for node in self.list_nodes(screen)):
            return True
        return themes and self.missing in self.themes

    def list_index(self, screen, island):
        if island == 0:
            return -1
        for n, node in enumerate(self.list_nodes(screen)):
            if self.e.u32(node) == island:
                return n
        return -9

    def op(self, op, a, texts):
        e, L = self.e, self.L
        s = a[0] & 1
        screen = self.screens[s]
        if op == 1:
            block = e.alloc(0xE8, b"\xa5" * 0xE8)
            L.labels[block] = L_SCREEN + s
            self.call(U["ctor"], block)
            w32(e, block, THEME_VTABLE)
            self.screens[s] = block
            self.screen_alive[s] = True
            return 0
        if op == 25:
            self.game_type = a[1] & MINUS
            w32(e, G["game_type"], a[1])
            return 0
        if op == 26:
            self.user = a[1] & MINUS
            if a[0]:
                if not e.u32(G["user"]):
                    L.labels[self.make_user(a[1])] = L_USER
                else:
                    w32(e, e.u32(G["user"]) + 0x60, a[1])
            else:
                w32(e, G["user"], 0)
            return 0
        if op == 27:
            if a[0]:
                block = e.alloc(0x4C, b"\xa5" * 0x4C)
                w32(e, G["master"], block)
                L.labels[block] = L_MASTER
            else:
                w32(e, G["master"], 0)
            return 0
        if op == 28:
            self.missing = texts
            return 0
        if op == 29:
            FS.s_write(e, G["theme"], texts, False)
            return 0
        if op == 18:
            return self.call(M["get_rand"], self.lobby)
        if not self.screen_alive[s]:
            return -5
        has_null = any(e.u32(node) == 0 for node in self.list_nodes(screen))
        if op == 2:
            self.call(M["theme_dtor"], screen, a[1])
            self.screen_alive[s] = False
        elif op == 3:
            if has_null:
                return -5
            name, folder, mesh_a, mesh_b, title = (texts.split("\n") + [""] * 5)[:5]
            block = e.alloc(0x74, b"\xa5" * 0x74)
            position = e.alloc(12, struct.pack(">3I", *[v & MINUS for v in a[3:6]]))
            sign1 = self.signs[(a[6] - 1) & 1] if a[6] else 0
            sign2 = self.signs[(a[7] - 1) & 1] if a[7] else 0
            park = self.parks[(a[8] - 1) & 1] if a[8] else 0
            self.call(U["island_ctor"], block, screen, a[1], self.text(name), self.text(folder), self.text(mesh_a),
                      self.text(mesh_b), T.F(bf(a[2])), position, CL.new_wide(e, title), park, sign1, sign2)
        elif op == 4:
            island = self.island_at(screen, a[1])
            if not island:
                return -5
            self.call(M["island_show"], island)
        elif op == 5:
            island = self.island_at(screen, a[1])
            if not island:
                return -5
            if e.u32(screen + 0xC) == island:
                w32(e, screen + 0xC, 0)
            if e.u32(screen + 0x10) == island:
                w32(e, screen + 0x10, 0)
            self.call(M["island_dtor"], island, a[2])
        elif op == 6:
            self.call(M["reset_sounds"], screen)
        elif op == 7:
            island = 0 if a[1] < 0 else self.island_at(screen, a[1])
            self.call(M["set_current"], screen, island)
        elif op == 8:
            self.virtual(screen, 0x50)
        elif op == 9:
            self.call(M["select"], screen)
        elif op == 10:
            self.call(M["zoom"], screen)
        elif op == 11:
            if s32(e.u32(e.u32(screen + 8) + 0xC)) <= 0 or not e.u32(e.u32(screen + 8) + 4) or has_null:
                return -5
            self.call(M["reset_current"], screen, a[1])
        elif op == 12:
            self.call(M["reset_timeout"], screen)
        elif op == 13:
            if has_null or self.signs_would_fail(screen, False):
                return -5
            self.call(M["update_signs"], screen)
        elif op == 14:
            self.call(M["goto_next"], screen)
        elif op == 15:
            self.call(M["goto_previous"], screen)
        elif op == 16:
            self.call(M["window"], screen, a[1], a[2], a[3])
        elif op == 17:
            if s32(e.u32(screen + 0x14)) == 2 and e.u32(screen + 0xC) == 0:
                return -5
            return self.call(M["cancel"], screen)
        elif op == 19:
            w32(e, screen + 0x14, a[1])
        elif op == 20:
            w32(e, screen + 0xC, 0 if a[1] < 0 else self.island_at(screen, a[1]))
        elif op == 21:
            if (a[5] and e.u32(screen + 0x10) == 0) or has_null:
                return -5
            park = self.parks[(a[1] - 1) & 1] if a[1] else 0
            gate1 = self.signs[(a[3] - 1) & 1] if a[3] else 0
            gate2 = self.signs[(a[4] - 1) & 1] if a[4] else 0
            self.call(M["parse"], screen, self.text(texts), park, a[2], gate1, gate2)
        elif op == 22:      # Show reads every theme's islands, then updates the signs
            if has_null or self.signs_would_fail(screen, True):
                return -5
            self.virtual(screen, 0x24)
        elif op == 23:
            was_visible = e.u32(screen + 4) != 0
            self.virtual(screen, 0x28)
            if was_visible:
                w32(e, screen + 0xC, 0)
                w32(e, screen + 0x10, 0)
        elif op == 24:
            island = self.island_at(screen, a[1])
            if not island:
                return -5
            w32(e, island + 0x1C, a[2])
            w32(e, island + 0x20, a[3])
        elif op == 30:
            w32(e, screen + 0x10, 0 if a[1] < 0 else self.island_at(screen, a[1]))
        elif op == 31:
            w32(e, screen + 4, a[1])
        elif op == 32:                # a NULL entry into an empty list
            if e.u32(e.u32(screen + 8) + 0xC):
                return -5
            self.call(U["Register"], screen, 0)
        elif op == 34:                # ResetIslandLoopingSounds with no list
            lst = e.u32(screen + 8)
            w32(e, screen + 8, 0)
            self.call(M["reset_sounds"], screen)
            w32(e, screen + 8, lst)
        elif op == 33:                # an island's "shown" word
            island = self.island_at(screen, a[1])
            if not island:
                return -5
            w32(e, island + 0x10, a[2])
        else:
            return -99
        return 0

    def state(self):
        e, L = self.e, self.L
        for i in range(SCREENS):
            if self.screen_alive[i]:
                for node in self.list_nodes(self.screens[i]):
                    if e.u32(node) and L.lab(e.u32(node)) == -1:
                        L.newlab(e.u32(node))
        out = [e.u32(self.view + o) for o in (8, 0xC, 0x10, 0x14, 0x18, 0x1C, 4, 0x28, 0x2C)] + [s32(e.u32(self.view + 0x30))]
        out.append(L.lab(e.u32(G["online"])))
        out += [e.u32(self.lobby + 0x34), s32(e.u32(self.lobby + 0x14)), hash_text(FS.s_read(e, G["theme"], False)),
                s32(e.u32(G["game_type"]))]
        user = e.u32(G["user"])
        out.append(1 + s32(e.u32(user + 0x60)) if user else 0)
        out += [L.lab(user), L.lab(e.u32(G["master"]))]
        for i in range(SCREENS):
            if not self.screen_alive[i]:
                continue
            block = self.screens[i]
            lst = e.u32(block + 8)
            out += [i, s32(e.u32(block + 4)), s32(e.u32(lst + 0xC))]
            for n, node in enumerate(self.list_nodes(block)):
                b = e.u32(node)
                if b == 0:      # (a NULL entry)
                    out += [n, 0]
                    continue
                out += [n, L.lab(b), L.lab(e.u32(b + 4)), L.lab(e.u32(b + 8)), self.screen_slot(e.u32(b + 0xC)), s32(e.u32(b + 0x10)),
                        e.u32(b + 0x14), s32(e.u32(b + 0x18)), s32(e.u32(b + 0x1C)), s32(e.u32(b + 0x20)), s32(e.u32(b + 0x24)),
                        hash_text(FS.s_read(e, b + 0x28, False)), e.u32(b + 0x3C), e.u32(b + 0x40), e.u32(b + 0x44),
                        hash16_full(FS.s_read(e, b + 0x48, True)), s32(e.u32(b + 0x5C)), s32(e.u32(b + 0x60)), e.u32(b + 0x64),
                        e.u32(b + 0x68), e.u32(b + 0x6C), L.lab(e.u32(b + 0x70))]
            out += [self.list_index(block, e.u32(block + 0xC)), self.list_index(block, e.u32(block + 0x10)), s32(e.u32(block + 0x14))]
            out += [e.u32(block + o) for o in range(0x18, 0xE8, 4)]
        return out


def u32_of(e, address):
    return e.u32(address)


def hash16_full(text):
    h = 2166136261
    for ch in text:
        h = ((h ^ ord(ch)) * 16777619) & MINUS
    return h


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    if "c" not in FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


# ---- running a case


def blob_of(case):
    files = "\x02".join("%s\x03%s" % (theme, text.decode("latin-1")) for theme, text in case["files"])
    return "\x01".join(["\n".join(case["themes"]), files, case["missing"], case["theme"]]).encode("latin-1")


def rest_ppc(e, case):
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


def rest_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    CL.host_begin(lib, {}, case.get("q"))
    F(lib, "tpw_lr_begin", None, C.POINTER(C.c_uint32), C.c_char_p)((C.c_uint32 * WORLD_WORDS)(*case["world"]), blob_of(case))
    op_fn = F(lib, "tpw_lr_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32), C.c_char_p)
    state_fn = F(lib, "tpw_lr_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 4096)()
    try:
        for op, a, texts in case["script"]:
            answers.append(op_fn(op, (C.c_int32 * 12)(*[T.s32(v & MINUS) for v in a]), texts.encode("latin-1")))
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    answers = [x & MINUS if x >= 0 else x for x in answers]
    return dict(answers=answers, states=states, marks=marks, log=log)


def read_uni_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        e.li = type("Li", (), {})()
        e.cl = CL.PpcLog({})
        text = case["text"].encode("latin-1")
        base = e.alloc(len(text) + 1, text + b"\0")
        cursor = e.alloc(4, struct.pack(">I", base + case["start"]))
        result = e.alloc(0x14)
        T.call_ppc(e, M["read_uni"], [result, cursor, ord(case["delimiter"])])
        return [FS.s_read(e, result, True), e.u32(cursor) - base]
    finally:
        CL.Stubs.deactivate(e, saved)


def read_uni_host(lib, case):
    text = C.create_string_buffer(case["text"].encode("latin-1"))
    out = (C.c_uint16 * 700)()
    end = F(lib, "tpw_lr_read_uni", C.c_int32, C.c_char_p, C.c_int32, C.c_int32, C.POINTER(C.c_uint16))(
        text, case["start"], ord(case["delimiter"]), out)
    chars = []
    for value in out:
        if value == 0:
            break
        chars.append(chr(value))
    return ["".join(chars), end]


# ---- the worlds and the scripts


def rbits(r):
    """Random float bits for a value that is copied, not computed on: any pattern, NaNs included."""
    k = r.random()
    if k < 0.2:
        return CL.fbits(r.choice([0.0, -0.0, 1.0, -1.0, 90.0, 500.0, 75.0, 1e-30, 1e30]))
    if k < 0.7:
        return CL.fbits(T.rfloat_mod(r, -1000.0, 1000.0))
    return r.getrandbits(32)


def rworld(r):
    w = [0] * WORLD_WORDS
    for i in range(6):
        w[W_VIEW + i] = rbits(r)
    w[W_VIEW_FOV] = rbits(r)
    w[W_DESCRIPTION_FOV] = rbits(r)
    w[W_ONLINE] = r.randrange(2)
    for i in range(8):
        w[W_SOUNDS + i] = r.getrandbits(32)
    for i in range(20):
        w[W_CAMERA_X + i] = rbits(r)
    w[W_SEED] = r.choice([0, 1, 0xFFFFFFFF, 0x3C6EF35F, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32)])
    w[W_LOBBY_STATE] = r.choice([0, 1, 2, 3, -1, 0x7FFFFFFF]) & MINUS
    w[W_GAME_TYPE] = r.choice([0, 1, 2, 2, 3, -1, 0x80000000]) & MINUS
    w[W_USER] = r.choice([-1, -1, 0, 1, 2, 3, 7]) & MINUS
    w[W_USER_EXISTS] = r.randrange(2)
    w[W_MASTER_EXISTS] = r.randrange(2)
    w[W_GUARD] = r.randrange(2)
    return w


def rqueues(r):
    q = {
        "rand": [r.choice([r.getrandbits(32), r.randrange(0x8000), 0xFF, 0x100, 0xFFFFFFFF, 0]) for _ in range(12)],
        "SND_RegisterLocalLobbySounds": [r.randrange(4) for _ in range(12)],
        "SOUND_SendEvent": [r.choice([r.randrange(-3, 50), r.randrange(-2 ** 31, 2 ** 31)]) for _ in range(12)],
        "Keys": [r.choice([-1, 0, 1, 2, 3, 5, 10]) for _ in range(12)],
        "Clock": [r.getrandbits(32) for _ in range(6)],
    }
    for i in range(3):
        q["Cost:%d" % (L_PARK_DATA + i)] = [r.choice([-1, 0, 1, 2, 3, 5, 10, 11]) for _ in range(12)]
    return q


# The lines of a description file.
INT_TEXTS = ["0", "1", "2", "3", "5", "9", " 4", "-1", "7abc", "x", "", "12", "100", "255", "256", "65535", "-3", "+2"]
REAL_TEXTS = ["0", "1", "90", "-45.5", "180", "360", "1e2", ".5", "12.25", "-0", "x", "", "3.0e-2", "-120.75", "7", " 8"]


def folder_text(r):
    return r.choice(FOLDERS)


def island_line(r, number=None, **kw):
    number = str(r.randrange(10)) if number is None else number
    sep = kw.get("sep", r.choice(["", " ", ""]))
    return 'ISLAND(%s,%s"%s"%s"%s"%s"%s"%s"%s"%s,%s%s,%s%s)%s' % (
        number, sep, kw.get("folder", folder_text(r)), sep, kw.get("a", r.choice(MESHES)), sep, kw.get("b", r.choice(MESHES)),
        sep, kw.get("title", r.choice(TITLES)), sep, sep, kw.get("angle", r.choice(REAL_TEXTS)), sep,
        kw.get("y", r.choice(REAL_TEXTS)), r.choice(["", ";", " // note"]))


def flying_line(r):
    # (Anything may stand between the bracket and the first quote, between the quotes, and between the second
    # quote and its comma: the parser skips to the next quote or comma.)
    return 'FLYINGMESH(%s"%s"%s"%s"%s,%s,%s,%s,%s)%s' % (
        r.choice(["", "", " ", "x "]), folder_text(r), r.choice(["", " ", ""]), r.choice(MESHES), r.choice(["", "", " ", " x"]),
        r.choice(INT_TEXTS), r.choice(REAL_TEXTS), r.choice(REAL_TEXTS), r.choice(REAL_TEXTS) + "," + r.choice(REAL_TEXTS),
        r.choice(["", ";"]))


def weather_line(r):
    k = r.randrange(3)
    if k == 0:
        return "LIGHTNING(%s)%s" % (r.choice(INT_TEXTS), r.choice(["", ";"]))
    if k == 1:
        return "RAINY(%s)%s" % (r.choice(INT_TEXTS), r.choice(["", ";"]))
    return "SKYCOLOUR(%s,%s,%s)%s" % (r.choice(INT_TEXTS), r.choice(INT_TEXTS), r.choice(INT_TEXTS), r.choice(["", ";"]))


BAD_LINES = ["", "", "// a comment", "# another", "ISLAND", "ISLAN(1)", "island(1)", " ISLAND(1)", "FLYINGMESH", "FLYINGMESH (1)",
             "LIGHTNING", "RAINY", "SKYCOLOUR", "skycolour(1,2,3)", "ISLANDFOV(3)", "SPINSPEED(4)", "RAINYDAY(2)", "X", "(", ")", "\r",
             "ISLANDS(1)", "ISLAND)", "SKY"]


def file_lines(r, count, first_island, bare=False):
    """Lines of a description. With `first_island` the first matched line is an island (a world where
    the screen has no island yet survives it); else any line may come first. Answers (lines, needs_last):
    whether a flying mesh or a weather line comes before the first island."""
    lines, seen_island, needs_last = [], False, False
    for _ in range(count):
        k = r.random()
        if k < 0.34 or (first_island and not seen_island and k < 0.7):
            lines.append(island_line(r))
            seen_island = True
            continue
        if first_island and not seen_island:
            lines.append(r.choice(BAD_LINES))
            continue
        if k < 0.5:
            line = flying_line(r)
        elif k < 0.78:
            line = weather_line(r)
        else:
            line = r.choice(BAD_LINES)
            lines.append(line)
            continue
        if not seen_island:
            needs_last = True
        lines.append(line)
    return lines, needs_last


def file_text(r, lines):
    ending = r.choice(["\n", "\n", "\r\n"])
    text = ending.join(lines)
    if lines and r.random() < 0.9:
        text += ending
    elif lines and not lines[-1].strip():
        text += ending
    return text.encode("latin-1")


def needs_last_of(text):
    """Whether the description has a flying mesh or weather line before its first island line."""
    for line in text.decode("latin-1").replace("\r", "").split("\n"):
        if line.startswith("ISLAND("):
            return False
        if line.startswith(("FLYINGMESH(", "LIGHTNING(", "RAINY(", "SKYCOLOUR(")):
            return True
    return False


def make_files(r, ok):
    """The theme descriptions of a world: for each theme one file. With `ok` every file starts with an island
    and has at least one."""
    files = []
    for theme in THEMES:
        if ok:
            lines, _ = file_lines(r, r.randrange(2, 9), True)
            if not any(line.startswith("ISLAND(") for line in lines):
                lines.insert(0, island_line(r))
        else:
            lines, _ = file_lines(r, r.randrange(0, 10), False)
        files.append((theme, file_text(r, lines)))
    return files


class Script:
    def __init__(self):
        self.steps = []

    def add(self, op, *a, texts=""):
        self.steps.append((op, (list(a) + [0] * 12)[:12], texts))

    def island(self, s, order, angle, position, sign1, sign2, park, name, folder, mesh_a, mesh_b, title):
        self.add(3, s, order, angle, position[0], position[1], position[2], sign1, sign2, park,
                 texts="\n".join([name, folder, mesh_a, mesh_b, title]))

    def parse(self, s, theme, park, order, gate1, gate2, needs_last):
        self.add(21, s, park, order, gate1, gate2, 1 if needs_last else 0, texts=theme)


def rangle(r):
    return CL.fbits(r.choice([0.0, 90.0, 180.0, -45.0, 360.0, 1e30, T.rfloat_mod(r, -400.0, 400.0)]))


def random_island(r, s, script):
    script.island(s, r.choice([0, 1, 2, 3, 5, 10, -1, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)]), rangle(r),
                  [rbits(r), rbits(r), rbits(r)], r.randrange(3), r.randrange(3), r.randrange(3), r.choice(NAMES),
                  r.choice(FOLDERS), r.choice(MESHES), r.choice(MESHES), r.choice(TITLES))


def make_case(r, script_fn, ok=True, files=None):
    world = rworld(r)
    themes = THEMES[:r.randrange(1, len(THEMES) + 1)] if ok else []
    case = dict(world=world, themes=themes, files=files if files is not None else make_files(r, ok), q=rqueues(r),
                missing=r.choice(["", "", "zz", "ClearMaster", r.choice(NAMES)]), theme=r.choice(["", "alpha", "Bravo", "d", "Echo5", "x"]))
    case["script"] = script_fn(r, case)
    return case


def parse_script(r, case):
    """Descriptions read onto a screen that may already have islands: any lines, in any order."""
    s = Script()
    s.add(1, 0)
    for _ in range(r.randrange(0, 3)):
        random_island(r, 0, s)
    for _ in range(r.randrange(1, 4)):
        theme, text = r.choice(case["files"])
        s.parse(0, theme, r.randrange(3), r.choice([-1, -1, 0, 1, 5, 9]), r.randrange(3), r.randrange(3), needs_last_of(text))
        if r.random() < 0.3:
            s.add(r.choice([6, 11, 13, 4, 20, 30]), 0, r.randrange(5))
    return s.steps


def members_script(r, case):
    s = Script()
    s.add(1, 0)
    if r.random() < 0.4:
        s.add(1, 1)
    for _ in range(r.randrange(0, 5)):
        random_island(r, r.randrange(2) if r.random() < 0.3 else 0, s)
    for _ in range(r.randrange(6, 30)):
        sc = 0 if r.random() < 0.75 else 1
        k = r.random()
        if k < 0.05:
            random_island(r, sc, s)
        elif k < 0.08:
            theme, text = r.choice(case["files"])
            s.parse(sc, theme, r.randrange(3), r.choice([-1, -1, 0, 5]), r.randrange(3), r.randrange(3), needs_last_of(text))
        elif k < 0.12:
            s.add(r.choice([22, 22, 23]), sc)
        elif k < 0.16:
            s.add(4, sc, r.randrange(6))
        elif k < 0.20:
            s.add(5, sc, r.randrange(6), r.choice([0, 1, 1]))
        elif k < 0.25:
            s.add(6, sc)
        elif k < 0.30:
            s.add(7, sc, r.choice([-1, 0, 1, 2, 3, 4]))
        elif k < 0.35:
            s.add(8, sc)
        elif k < 0.41:
            s.add(9, sc)
        elif k < 0.44:
            s.add(10, sc)
        elif k < 0.49:
            s.add(11, sc, r.choice([0, 0, 1, -1, 7]))
        elif k < 0.52:
            s.add(12, sc)
        elif k < 0.58:
            s.add(13, sc)
        elif k < 0.62:
            s.add(r.choice([14, 15]), sc)
        elif k < 0.70:
            s.add(16, sc, r.choice(WINDOW_MESSAGES), r.choice(WINDOW_KEYS), r.getrandbits(32))
        elif k < 0.74:
            s.add(17, sc)
        elif k < 0.76:
            s.add(18, 0)
        elif k < 0.80:
            s.add(19, sc, r.choice([0, 1, 2, 3, -1, 0x7FFFFFFF]))
        elif k < 0.84:
            s.add(20, sc, r.choice([-1, 0, 1, 2, 3, 4, 5]))
        elif k < 0.86:
            s.add(24, sc, r.randrange(6), r.choice([0, 5, -1, 77]), r.choice([0, 6, 99]))
        elif k < 0.875:
            s.add(34, sc)
        elif k < 0.88:
            s.add(25, 0, r.choice([0, 1, 2, 2, 3, -1]))
        elif k < 0.90:
            s.add(26, r.randrange(2), r.choice([-1, -1, 0, 2, 5]))
        elif k < 0.92:
            s.add(27, r.randrange(2))
        elif k < 0.93:
            s.add(28, 0, texts=r.choice(["", "zz", "ClearMaster", r.choice(NAMES)]))
        elif k < 0.95:
            s.add(29, 0, texts=r.choice(["", "alpha", "BRAVO", "Charlie Isle", "d", "zz"]))
        elif k < 0.97:
            s.add(30, sc, r.choice([-1, 0, 1, 2]))
        elif k < 0.98:
            s.add(33, sc, r.randrange(4), r.choice([0, 1, 2, -1]))
        else:
            s.add(31, sc, r.choice([0, 1, 1, 2]))
    if r.random() < 0.5:
        for sc in range(2):
            s.add(2, sc, r.choice([0, 1]))
    return s.steps


WINDOW_MESSAGES = [0x1000B, 0x1000B, 0x10005, 0x10005, 0x1000A, 0x1000C, 0x10004, 0x10006, 0, -1, 0x7FFFFFFF, -0x80000000, 5, 0x20005]
WINDOW_KEYS = [0x2500, 0x2700, 0x0D, 0, 1, 0x2501, 0x24FF, 0x2600, 0x2701, 0x2800, -1, 0x7FFFFFFF, -0x80000000, 0x0C, 0x0E, 0x25, 0x27]


def parse_case(r):
    return make_case(r, parse_script, ok=False)


def members_case(r):
    return make_case(r, members_script, ok=True)


def edge_world(r, **kw):
    world = rworld(r)
    for key, value in kw.items():
        world[globals()["W_" + key]] = value & MINUS
    return world


_EDGES = None


def edges():
    """The edge cases (built once): (kind, case), kind "parse" for the cases of the parser's test."""
    global _EDGES
    if _EDGES is None:
        _EDGES = list(edge_cases())
    return _EDGES


def edge_cases():
    r = random.Random(97)
    base = lambda script, **kw: dict(world=edge_world(r, **kw.pop("w", {})), themes=kw.pop("themes", []),   # noqa: E731
                                     files=kw.pop("files", []), q=kw.pop("q", rqueues(r)), missing=kw.pop("missing", ""),
                                     theme=kw.pop("theme", ""), script=script.steps)

    def island(s, sc, order, name, **kw):
        s.island(sc, order, kw.get("angle", CL.fbits(90.0)), kw.get("pos", [CL.fbits(1.0), CL.fbits(2.0), CL.fbits(3.0)]),
                 kw.get("s1", 0), kw.get("s2", 0), kw.get("park", 0), name, "data:lobby:", "rock", "rock_b", kw.get("title", "T"))

    fb = CL.fbits
    real_file = (
        'ISLAND(0,"Halloween/Rock","isl_hal","isl_hal_lo","Haunted Isle",90,5)\r\nLIGHTNING(4)\r\nRAINY(1)\r\n'
        'SKYCOLOUR(40,40,60)\r\nFLYINGMESH("Halloween\\Bats","bat",6,1.5,0,-0.5,30)\r\n'
        'ISLAND(1,"Space\\Pad","isl_sp","isl_sp_lo","Moon Base",-45.5,10)\r\nFLYINGMESH("Space/Ships","ship",2,0,0,1,12.5)\r\n'
        'ISLAND(2,"Fairy/Wood","isl_fw","isl_fw_lo","Fairy Wood",180,0)\r\nSKYCOLOUR(255,255,255)\r\n').encode("latin-1")
    # The parser over a real-looking description, each ending, with and without a forced order.
    for name, text in (("real", real_file), ("lf", real_file.replace(b"\r\n", b"\n")), ("empty", b""), ("one newline", b"\n"),
                       ("no final newline", real_file.rstrip(b"\r\n")), ("cr alone", real_file.replace(b"\r\n", b"\r"))):
        for order in (-1, 0, 4):
            s = Script()
            s.add(1, 0)
            s.parse(0, "Halloween", 1, order, 1, 2, False)
            s.parse(0, "Halloween", 0, -1, 0, 0, False)
            yield base(s, files=[("Halloween", text)], q=rqueues(r))
    # Each keyword with each value; the keyword matched by its first characters only.
    for value in INT_TEXTS:
        for keyword in ("LIGHTNING(%s)", "RAINY(%s)", "SKYCOLOUR(%s,%s,%s)"):
            text = ("ISLAND(3,\"f\",\"a\",\"b\",\"t\",1,2)\n" + keyword.replace("%s", value) + "\n").encode("latin-1")
            s = Script()
            s.add(1, 0)
            s.parse(0, "T", 0, -1, 0, 0, False)
            yield base(s, files=[("T", text)])
    for value in REAL_TEXTS:
        text = ('ISLAND(3,"f/g\\h","a","b","t",%s,%s)\nFLYINGMESH("x/y","m",3,%s,%s,%s,%s)\n' % ((value,) * 6)).encode("latin-1")
        s = Script()
        s.add(1, 0)
        s.parse(0, "T", 2, -1, 1, 0, False)
        yield base(s, files=[("T", text)])
    for index in (0, 9, 5, 1):
        text = ('ISLAND(%d,"f","a","b","t",1,2)\n' % index).encode("latin-1")
        s = Script()
        s.add(1, 0)
        s.parse(0, "T", 0, -1, 0, 0, False)
        yield base(s, files=[("T", text)])
    for order in (0, 3, 9):
        text = b'ISLAND(7,"f","a","b","t",1,2)\n'
        s = Script()
        s.add(1, 0)
        s.parse(0, "T", 0, order, 0, 0, False)
        yield base(s, files=[("T", text)])
    # Lines that are not matched, or matched at their first characters only.
    for line in BAD_LINES + ["ISLAND(1,\"f\",\"a\",\"b\",\"t\",1,2) trailing ISLAND(2", "FLYINGMESH(\"a\",\"b\",1,0,0,0,1)"]:
        text = ("ISLAND(1,\"f\",\"a\",\"b\",\"t\",1,2)\n%s\nRAINY(2)\n" % line).encode("latin-1")
        s = Script()
        s.add(1, 0)
        s.parse(0, "T", 0, -1, 0, 0, False)
        yield base(s, files=[("T", text)])
    # The same island numbers twice, ties in the list, then the weather of the last island read.
    text = ('ISLAND(2,"f","a","b","t",1,2)\nISLAND(2,"g","c","d","u",3,4)\nISLAND(1,"h","e","f","v",5,6)\nRAINY(3)\n'
            'LIGHTNING(1)\nSKYCOLOUR(1,2,3)\nISLAND(2,"i","g","h","w",7,8)\nRAINY(9)\n').encode("latin-1")
    s = Script()
    s.add(1, 0)
    s.parse(0, "T", 1, -1, 1, 2, False)
    s.parse(0, "T", 2, -1, 2, 1, False)
    yield base(s, files=[("T", text)])
    # A flying mesh and weather onto a screen whose last island was made by hand; and a missing file.
    text = b'FLYINGMESH("a","b",2,1,2,3,4)\nRAINY(5)\n'
    s = Script()
    s.add(1, 0)
    island(s, 0, 4, "Isle", pos=[fb(10.0), fb(20.0), fb(30.0)])
    s.parse(0, "T", 0, -1, 0, 0, True)
    s.parse(0, "Absent", 0, -1, 0, 0, True)
    s.parse(0, "T", 0, -1, 0, 0, True)
    yield base(s, files=[("T", text)])
    s = Script()
    s.add(1, 0)
    island(s, 0, 4, "Isle", pos=[0x7FC00000, fb(float("inf")), fb(-0.0)])
    s.parse(0, "T", 0, -1, 0, 0, True)
    yield base(s, files=[("T", text)])
    # A second parse needing a last island when there is none (skipped), and parse onto two screens.
    s = Script()
    s.add(1, 0)
    s.add(1, 1)
    s.parse(0, "T", 0, -1, 0, 0, True)
    s.parse(1, "U", 1, 3, 1, 1, False)
    s.parse(0, "U", 2, -1, 2, 2, False)
    s.parse(1, "T", 0, -1, 0, 0, True)
    yield base(s, files=[("T", text), ("U", b'ISLAND(6,"f","a","b","t",1,2)\n')])

    # GetRand.
    for seed in (0, 1, 0xFFFFFFFF, 0x3C6EF35F, 0x80000000, 0x7FFFFFFF, 0x0019660D, 0x12345678):
        s = Script()
        for _ in range(4):
            s.add(18)
        yield base(s, w={"SEED": seed})
    # The island's Show and destructor: shown 0, nonzero, negative; flags; the owner's list.
    for shown in (0, 1, 2, -1, 0x80000000):
        for flag in (-1, 0, 1):
            s = Script()
            s.add(1, 0)
            island(s, 0, 3, "a")
            island(s, 0, 1, "b")
            island(s, 0, 2, "c")
            s.add(33, 0, 1, shown)
            s.add(4, 0, 1)
            s.add(4, 0, 1)
            s.add(5, 0, 1, flag)
            s.add(5, 0, 5, flag)
            s.add(5, 0, 0, flag)
            s.add(5, 0, 0, flag)
            s.add(4, 0, 0)
            s.add(2, 0, 1)
            yield base(s)
    # ResetIslandLoopingSounds: no current island, a current island, an empty list, a NULL first entry.
    for current in (-1, 0, 2):
        for sounds in ((0, 0), (5, 6), (-1, 77)):
            s = Script()
            s.add(1, 0)
            s.add(6, 0)
            island(s, 0, 3, "a")
            island(s, 0, 1, "b")
            island(s, 0, 2, "c")
            s.add(20, 0, current)
            s.add(24, 0, 0, sounds[0], sounds[1])
            s.add(24, 0, 2, sounds[1], sounds[0])
            s.add(6, 0)
            s.add(6, 0)
            yield base(s, q=dict(rqueues(r), SND_RegisterLocalLobbySounds=[0, 1, 2, 3, 3, 1], SOUND_SendEvent=[11, 22, 33, 44, 55, 66]))
    s = Script()
    s.add(1, 0)
    s.add(32, 0)
    s.add(6, 0)
    s.add(2, 0, 0)
    yield base(s)
    s = Script()
    s.add(1, 0)
    s.add(32, 0)
    s.add(2, 0, 1)
    yield base(s)
    s = Script()
    s.add(1, 0)
    s.add(34, 0)
    island(s, 0, 1, "a")
    s.add(34, 0)
    s.add(20, 0, 0)
    s.add(34, 0)
    yield base(s)
    # SetCurrentIsland and ResetCurrentIsland.
    for theme in ("", "alpha", "ALPHA", "Bravo", "d", "x", "Alph"):
        for value in (0, 0, 1, -1):
            s = Script()
            s.add(1, 0)
            s.add(11, 0, value)
            for name in ("Alpha", "bravo", "alpha", "D"):
                island(s, 0, len(s.steps), name)
            s.add(7, 0, -1)
            s.add(7, 0, 2)
            s.add(11, 0, value)
            s.add(11, 0, 0)
            s.add(19, 0, 2)
            s.add(7, 0, 1)
            s.add(11, 0, 1)
            s.add(11, 0, 0)
            yield base(s, theme=theme)
    # EnterCurrentIsland: no current island, islands of other names, the lobby's state word.
    for name in ("Alpha", "", "a b", "ClearMaster"):
        for lobby_state in (0, 2, -1):
            s = Script()
            s.add(1, 0)
            s.add(8, 0)
            island(s, 0, 1, name)
            island(s, 0, 2, "other")
            s.add(8, 0)
            s.add(20, 0, 1)
            s.add(8, 0)
            s.add(20, 0, 0)
            s.add(8, 0)
            yield base(s, w={"LOBBY_STATE": lobby_state}, theme="old")
    # SelectCurrentIsland: every branch, with the master save there, absent, or gone after GetInfoOn.
    for game_type in (0, 1, 2, 3, -1):
        for master in (0, 1):
            for missing in ("", "A", "ClearMaster"):
                for guard in (0, 1):
                    for keys, cost in ((5, 3), (3, 3), (3, 5), (-2, -1), (0, 0), (-1, -2)):
                        s = Script()
                        s.add(1, 0)
                        s.add(9, 0)
                        island(s, 0, 1, "A")
                        island(s, 0, 2, "ClearMaster")
                        s.add(9, 0)
                        s.add(20, 0, 0)
                        s.add(9, 0)
                        s.add(19, 0, 1)
                        s.add(9, 0)
                        s.add(19, 0, 0)
                        s.add(20, 0, 1)
                        s.add(9, 0)
                        s.add(19, 0, 0)
                        s.add(9, 0)
                        q = dict(rqueues(r), Keys=[keys] * 4)
                        for i in range(3):
                            q["Cost:%d" % (L_PARK_DATA + i)] = [cost] * 4
                        yield base(s, w={"GAME_TYPE": game_type, "MASTER_EXISTS": master, "GUARD": guard}, missing=missing, q=q)
    # StartCurrentIslandZoom, ProcessCancel, the Goto pair, the advisor's clock.
    for state in (0, 1, 2, 3, -1):
        for game_type in (0, 2):
            s = Script()
            s.add(1, 0)
            s.add(17, 0)
            island(s, 0, 1, "A")
            s.add(20, 0, 0)
            s.add(19, 0, state)
            s.add(17, 0)
            s.add(19, 0, state)
            s.add(14, 0)
            s.add(19, 0, state)
            s.add(15, 0)
            s.add(19, 0, state)
            s.add(10, 0)
            s.add(19, 0, state)
            s.add(12, 0)
            s.add(20, 0, -1)
            s.add(19, 0, state)
            s.add(17, 0)
            yield base(s, w={"GAME_TYPE": game_type, "GUARD": state & 1}, q=dict(rqueues(r), Clock=[0, 1, 0xFFFFFFFF, 12345]))
    # WindowMessagesFn: every message and key.
    for message in WINDOW_MESSAGES:
        for key in WINDOW_KEYS:
            s = Script()
            s.add(1, 0)
            island(s, 0, 1, "A")
            s.add(20, 0, 0)
            s.add(16, 0, message, key, 5)
            yield base(s, w={"GAME_TYPE": 2 if key & 1 else 0}, q=dict(rqueues(r), Keys=[9] * 4))
    # UpdateSigns: no user info, a user, none logged in; the master save there or not; a theme without a record.
    for user_exists in (0, 1):
        for user in (-1, 0, 3):
            for master in (0, 1):
                for missing in ("", "b"):
                    s = Script()
                    s.add(1, 0)
                    s.add(13, 0)
                    for name in ("a", "b", "c", "a"):
                        island(s, 0, len(s.steps), name)
                    s.add(13, 0)
                    s.add(26, 1, user)
                    s.add(13, 0)
                    s.add(27, 0)
                    s.add(13, 0)
                    s.add(26, 0, 0)
                    s.add(13, 0)
                    yield base(s, w={"USER_EXISTS": user_exists, "USER": user, "MASTER_EXISTS": master}, missing=missing)
    # The theme islands' Show and Hide with every theme read; the destructor with flags.
    for themes in (["jungle"], THEMES[:3], THEMES):
        for flag in (0, 1):
            s = Script()
            s.add(1, 0)
            s.add(22, 0)
            s.add(22, 0)
            s.add(13, 0)
            s.add(6, 0)
            s.add(9, 0)
            s.add(23, 0)
            s.add(22, 0)
            s.add(2, 0, flag)
            yield base(s, themes=themes, files=make_files(r, True), theme=r.choice(THEMES))
    for _ in range(30):
        yield members_case(r)
    for _ in range(30):
        yield parse_case(r)
    # (The cases above that read descriptions with a script made only of parses are the parser's.)


def is_parse_case(case):
    """The parser's cases: scripts that read descriptions and do little else."""
    ops = [step[0] for step in case["script"]]
    return 21 in ops and all(op in (1, 3, 21, 6, 11, 13, 4, 20, 30) for op in ops)


def read_uni_case(r):
    alphabet = ["a", "b", ",", ")", "(", " ", "1", "\xe9", "\xff", "\x80", "\x7f", "Z", "-", ";"]
    text = "".join(r.choice(alphabet) for _ in range(r.randrange(1, 30)))
    delimiter = r.choice([",", ")", "(", ";", '"'])
    text += delimiter
    return dict(text=text, start=r.randrange(len(text)), delimiter=delimiter)


def read_uni_edge():
    for text, start, delimiter in ((")", 0, ")"), ("abc)", 0, ")"), ("abc)", 2, ")"), ("a,b,c)", 0, ","), ("a,b,c)", 2, ","),
                                   ("\xe9\xff\x80)", 0, ")"), ("\x7f\x80\x81)", 0, ")"), ("x" * 300 + ")", 0, ")"), (",,", 0, ",")):
        yield dict(text=text, start=start, delimiter=delimiter)


TESTS = [
    FnTest("CLBScreenComponentIslands::ParseThemeDescriptor (and the screen it makes islands on)", rest_ppc, rest_host,
           edge=lambda: [c for c in edges() if is_parse_case(c)], rnd=parse_case),
    FnTest("the island and theme island members (WindowMessagesFn, SelectCurrentIsland, UpdateSigns, ...)", rest_ppc, rest_host,
           edge=lambda: [c for c in edges() if not is_parse_case(c)], rnd=members_case),
    FnTest("LB_ReadTbUniStringUntilNext", read_uni_ppc, read_uni_host, edge=read_uni_edge, rnd=read_uni_case),
]
