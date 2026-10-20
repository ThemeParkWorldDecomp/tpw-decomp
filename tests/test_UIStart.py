"""Differential tests for the start of the interface, against libtpw_unit_UIStart.dylib
(tests/unit_UIStart.cpp):

  src/TPAdvisor.cpp      the text file reader (7 functions)
  src/UI_Base.cpp        CoolRegion, CoolRect, UI_hash_cakes
  src/ui_DrawObject.cpp  InterfaceCursor, CursorTable, InterfaceDrawObject, DrawObjectLibrary,
                         the engine mesh objects' constructors, destructors and Show
  src/ui_main_meshes.cpp TPResource_AddFrameMesh/SliderMesh/MeshDrawObject, LoadAllUIMeshes,
                         LoadLocalisedUIMeshes, InitGeneralUI, UIInterface::InitGeneral, the
                         clock tick objects

Three tests. The reader: one call on a text at a position. The rectangles and cursors, and
the draw objects with the interface's meshes: scripts of operations, each followed by a
comparison of what it answered, what the fakes recorded and the whole state. Operations
the state does not allow (an empty slot, an object the library still has) are decided on the
PowerPC side and skipped on both.

On the PowerPC side the bullfrog strings are test_FSSystem.py's, objects are made with the
original constructors and virtual members are called through the objects' own vtables.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIStart"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F

# ---- the unit's functions
PARSE = dict(SkipWhitespace=0x10030C08, IsComment=0x10030D28, IsLetter=0x10030D80, IsNumber=0x10030DEC,
             IsMiscChar=0x10030E44, IsWhiteSpace=0x10030EDC, GetString=0x10031014)
REGION = dict(PointIn=0x101D5A3C, Middle=0x101D5A70, Shift=0x101D5AA0, Adjust=0x101D5AD0, dtor=0x101D5B00, eq=0x101D5B74)
RECT = dict(ctor=0x101C7448, ctor4=0x101C7494, dtor=0x101C74E0, Set=0x101C756C, Get=0x101C75A8, GetDims=0x101C7618,
            PointIn=0x101C7678, Middle=0x101C7710, RectIntersects=0x101C7788, Shift=0x101C7A1C, Adjust=0x101C7A78)
HASH = 0x101C8D40
CURSOR = dict(ctor=0x101CC6A0, dtor=0x101CC6F0, SetData=0x101CC768, SetHot=0x101CC7A0, Show=0x101CC7DC, Hide=0x101CC814,
              GetVisible=0x101CC84C, Draw=0x101CC888, Use=0x101CC8BC, Animate=0x101CCC8C)
TABLE = dict(ctor=0x101CC994, dtor=0x101CC9CC, DeAllocate=0x101CCA38, Allocate=0x101CCAFC, SetEntry=0x101CCBB0,
             GetEntry=0x101CCC34)
OBJECT = dict(ctor=0x101CCCC0, dtor=0x101CCD9C, Draw=0x101CCE44, Draw2=0x101CCE84, Change=0x101CCEC8, Query=0x101CCFAC,
              Show=0x101CCFE8, SetFrameBase=0x101CD020, SetFrame=0x101CD070, Refresh=0x1018A1B0,
              GetWritableDataArea=0x1018A1E8, InstanceMe=0x1018A234)
LIBRARY = dict(Add=0x101CD0BC, RemoveAll=0x101CD214, FindName=0x101CD2D4, FindHash=0x101CD3B0)
ENGINE = dict(ctor=0x10187A28, dtor=0x10187B28, Show=0x10187BFC, Draw=0x101884F4, Draw2=0x10188538,
              frame_ctor=0x1018868C, frame_dtor=0x10188704, slider_ctor=0x10188B8C, slider_dtor=0x10188C04)
MAIN = dict(AddFrameMesh=0x10184ABC, AddSliderMesh=0x10184D34, AddMeshDrawObject=0x10184F74, LoadAllUIMeshes=0x1018515C,
            LoadLocalisedUIMeshes=0x10185414, InitGeneralUI=0x10185B68, InitGeneral=0x101EF5F8, tick=0x10187994,
            lb_tick=0x10185B08)
REAL = [v for group in (PARSE, REGION, RECT, CURSOR, TABLE, OBJECT, LIBRARY, ENGINE, MAIN) for v in group.values()] + [HASH]

# ---- the original's data
VT_REGION, VT_RECT, VT_CURSOR, VT_OBJECT = 0x102AFCD8, 0x102AF8FC, 0x102AFA08, 0x102AFA38
VT_TICK, VT_LB_TICK = 0x102A7AE4, 0x102A7AD8
G = dict(library=0x10395C30, screen_width=0x102B2B54, ui_width=0x10395BBC, ui_height=0x10395BBA, title=0x1038CC50,
         cursor_table=0x1038CC64, ui_heap=0x1038CC60, font=0x1037F1A0, clock=0x10380B7C, game=0x1037F134,
         globals=0x103B74C8, text=0x1037FF18, frame_name=0x10380BF4, slider_name=0x10380BD4, plain_name=0x10380BB4,
         peek=0x102676B0)
GLOBAL_STRINGS = {0x42CC: ".md2", 0x4574: "*.md2", 0x4600: "UI", 0x4678: "Langauge"}

LAB = dict(manager=2, resource0=3, resource1=4, heap=5, text=6, storage=7)
MESH_LABEL, HEADER_LABEL = 20, 40
KIND = {0x3C: 0, 0x84: 1, 0x19C: 2, 0x18: 3, 0x14: 4}
SLOTS = 8
SKIPPED = "(skipped)"


def s16(v):
    return T.s16(v & 0xFFFF)


def separated(path):
    return path if path.endswith(":") else path + ":"


# ======================================================================= PowerPC side


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(REAL)
    FS.string_stubs(S)
    S.imp("__as__26TbDynamicStringTemplate<c>FPCc", lambda e: FS.s_write(e, e.r(3), e.cstr(e.r(4), limit=400), False))
    side = lambda e: e.us   # noqa: E731

    def op_new(e):
        e.set_r(3, side(e).new(e.r(3)))

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__nw__FUl", op_new)
    S.imp("__dl__FPv", op_delete)
    S.imp("__nwa__FUl", lambda e: e.set_r(3, e.alloc(max(e.r(3), 4), b"\xa5" * max(e.r(3), 4))))
    S.imp("__dla__FPv", lambda e: None)
    text = lambda e, a: e.read(a, 400).split(b"\0")[0]   # noqa: E731

    def strncpy(e):
        dest, source, count = e.r(3), text(e, e.r(4)), e.r(5)
        e.write(dest, (source[:count] + bytes(count))[:count])

    def strcmp(e):
        a, b = text(e, e.r(3)), text(e, e.r(4))
        e.set_r(3, T.u32((a > b) - (a < b)))

    S.imp("strncpy", strncpy)
    S.imp("strcmp", strcmp)
    S.imp("LbTime_GetClock__Fv", R("LbTime_GetClock", "", lambda e, L: L.ret("Clock", 0x1234)))

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    def load_mesh(e):
        L, s = e.cl, side(e)
        L.rec("LoadMesh", (1 if e.r(4) == 0 else 0, T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))), (e.cstr(e.r(3))[:159],))
        if not L.ret("Load", 1) or not s.loadable:
            e.set_r(3, 0)
            return
        e.set_r(3, s.loadable[s.next_mesh % len(s.loadable)])
        s.next_mesh += 1

    S.internal(0x100B7D04, load_mesh)
    i(0x100B8EA8, "OverrideTexturePaths", "Lii")
    i(0x10064F40, "CreateEngineMesh", "L")
    i(0x10065004, "DestroyEngineMesh", "L")
    i(0x100631C4, "DestroyMeshInstance", "LLu")
    S.internal(0x10065200, lambda e: e.cl.rec("EnableMeshDraw", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4) - 0x3C), T.s32(e.r(5)))))
    i(0x100653A4, "DisableMeshDraw", "L")

    def new_heap(e):
        e.cl.rec("HNewHeap", (e.r(3), e.r(4), e.r(5), 1 if e.r(6) == G["ui_heap"] else 0))
        w32(e, e.r(6), side(e).heap)
        e.set_r(3, 1)

    S.internal(0x1005B734, new_heap)
    S.internal(0x100053F4, lambda e: e.set_r(3, 1 if FS.icmp(FS.s_read(e, e.r(3), False), FS.s_read(e, e.r(4), False)) == 0 else 0))

    def add_separator(e):
        path = FS.s_read(e, e.r(3), False)
        e.cl.rec("FS_AddDirectorySeparator", (), (path[:159],))
        FS.s_write(e, e.r(3), separated(path), False)

    S.internal(0x10004BC0, add_separator)
    cs = lambda e, a: e.cstr(a)[:159] if a else "(null)"   # noqa: E731
    S.internal(0x100680FC, lambda e: (e.cl.rec("SetupDirectoryForMeshLoad", (), (cs(e, e.r(3)), cs(e, e.r(4)), cs(e, e.r(5)))),
                                      e.set_r(3, 1)))
    S.internal(0x1006FCFC, lambda e: e.cl.rec("TRACE", (T.s32(e.r(4)),), (e.cstr(e.r(3))[:159],)))

    def storage(e):
        e.cl.rec("pGetCurrentSearchStorage", (e.cl.lab(e.r(3)),))
        e.set_r(3, side(e).storage)

    S.internal(0x1015FDF4, storage)

    def find_first(e):
        L, s = e.cl, side(e)
        L.rec("FindFirst", (L.lab(e.r(3)),), (FS.s_read(e, e.r(4), False)[:159],))
        which = s.which(e.r(3))
        s.cursor[which] = 0
        found = s.next_file(which, e.r(5))
        forced = L.ret("FindFirst", -1)
        e.set_r(3, T.u32(found if forced == -1 else forced))

    def find_next(e):
        L, s = e.cl, side(e)
        L.rec("FindNext", (L.lab(e.r(3)),))
        found = s.next_file(s.which(e.r(3)), e.r(4))
        forced = L.ret("FindNext", -1)
        e.set_r(3, T.u32(found if forced == -1 or found == 0 else forced))

    def find_resource(e):
        L, s = e.cl, side(e)
        name = FS.s_read(e, e.r(4), False)
        L.rec("FindResource", (L.lab(e.r(3)),), (name[:159],))
        result = L.ret("FindResource", 1)
        if result != 0:
            w32(e, e.r(5), s.resources[0 if name == "UI" else 1])
        e.set_r(3, T.u32(result))

    S.internal(0x10160830, find_first)
    S.internal(0x10160A2C, find_next)
    S.internal(0x10165448, find_resource)
    S.internal(0x1015E93C, lambda e: (e.cl.rec("GetLanguageName", (e.cl.lab(e.r(3)),)), e.set_r(3, side(e).language)))
    i(0x1015EA40, "IsHebrew", "L", lambda e, L: 1 if L.ret("Hebrew", 0) else 0)
    S.internal(0x1016906C, lambda e: e.cl.rec("Gen8x8DebugFont", (1 if e.r(3) == G["font"] else 0,) + tuple(s16(e.r(k)) for k in range(4, 9))))
    i(0x101C8A5C, "UI_InitSystem", "iib")
    S.internal(0x101CABE0, lambda e: e.cl.rec("UI_SetClockTickFunction", (1 if e.r(3) == G["clock"] else 0,)))
    S.internal(0x101C9F18, lambda e: e.cl.rec("UI_SetPeekCallBack", (1 if e.r(3) == G["peek"] else 0,)))
    i(0x101D7808, "InterfaceStringHeap::Allocate", "iiii")
    i(0x1018575C, "InitialiseCOLOURCursors")
    S.internal(0x101CA170, lambda e: e.cl.rec("UI_UseCursorTable", (1 if e.r(3) == G["cursor_table"] else 0,)))
    i(0x101CA078, "UI_SetStandardCursor", "H")
    i(0x101C9F68, "UI_SetCursor", "h")
    i(0x101CA0B0, "UI_HideCursor")
    i(0x1018C60C, "InitHelpText")

    # The title text object: its base as the original's constructor leaves it (the fields
    # InterfaceDrawObject's sets, two rectangles), the vtable a fake whose Change records.
    def title_ctor(e):
        obj, data = e.r(3), e.r(4)
        for offset in (4, 6, 8, 0xC, 0xE, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32):
            e.write(obj + offset, b"\0\0")
        w32(e, obj, data)
        for rect in (obj + 0x10, obj + 0x1C):
            e.write(rect, struct.pack(">I4h", VT_RECT, 0, 0, 0, 0))
        w32(e, obj + 0x38, emu.us_title_vtable)
        e.cl.rec("TitleText.ctor", (e.cl.lab(obj), 1 if data == 0 else 0))

    S.internal(0x1018FBE8, title_ctor)
    for addr, name in ((0x10188168, "EngineMesh"), (0x101887F0, "Frame"), (0x10188CF0, "Slider")):
        S.internal(addr, (lambda n: lambda e: e.cl.rec(n + ".Change", (e.cl.lab(e.r(3)), e.r(4), 1 if e.r(5) else 0)))(name))
    for addr, name in ((0x10188580, "EngineMesh"), (0x10188A68, "Frame"), (0x10188ED0, "Slider")):
        S.internal(addr, (lambda n: lambda e: e.cl.rec(n + ".InstanceMe", (e.cl.lab(e.r(3)),)))(name))
    return S


def title_change(e):
    change = e.r(5)
    style = e.u32(change + 0x10)
    b = lambda o: e.read(style + o, 1)[0]   # noqa: E731
    e.cl.rec("TitleText.Change", (e.cl.lab(e.r(3)), e.r(4), b(4), b(5), b(9), b(0x10), b(0x11), b(0x12)))


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.us = self
        e.heap_in_scratch = True
        self.kinds = {}
        self.slots = [0] * SLOTS
        # (LoadMesh gives the meshes that have a root node, as every mesh of the game has: the
        # original takes the name of the root node without looking whether there is one.)
        self.meshes, self.headers, self.loadable, self.next_mesh = [], [], [], 0
        self.manager = e.alloc(16)
        self.storage = e.alloc(16)
        self.heap = e.alloc(16)
        self.node_word = e.alloc(4)
        self.language = e.alloc(32, case.get("language", "english").encode() + b"\0")
        self.resources = []
        for path in case.get("paths", ("data:ui", "data:language")):
            resource = e.alloc(0x40)
            FS.s_write(e, resource + 0x14, separated(path), False)
            self.resources.append(resource)
        self.files = [list(names) for names in case.get("files", ([], []))]
        self.prefixes = case.get("prefixes", ("", ""))
        self.cursor = [0, 0]
        fixed = {self.manager: LAB["manager"], self.resources[0]: LAB["resource0"], self.resources[1]: LAB["resource1"],
                 self.heap: LAB["heap"], G["text"]: LAB["text"], self.storage: LAB["storage"]}
        for index, spec in enumerate(case.get("meshes", ())):
            mesh, header = self.make_mesh(*spec)
            fixed[mesh] = MESH_LABEL + index
            fixed[header] = HEADER_LABEL + index
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        # the globals, as the static initialisers and the start-up leave them
        w32(e, G["library"], 0)
        w32(e, G["title"], 0)
        w32(e, G["ui_heap"], 0)
        e.write(G["cursor_table"], bytes(8))
        w32(e, G["screen_width"], 800)
        e.write(G["ui_width"], struct.pack(">h", 800))
        e.write(G["ui_height"], struct.pack(">h", 600))
        w32(e, G["game"] + 0x34, self.manager)
        w32(e, G["clock"], VT_LB_TICK)
        for offset, text in GLOBAL_STRINGS.items():
            FS.s_write(e, G["globals"] + offset, text, False)
        for name, text in (("frame_name", "!frame"), ("slider_name", "!slider"), ("plain_name", "!f_plain")):
            FS.s_write(e, G[name], text, False)

    # ---- fixtures
    def make_mesh(self, count, chain, header_flags, node_flags, names):
        e = self.e
        header = e.alloc(0xB8)
        w32(e, header + 0x30, header_flags)
        e.write(header + 0x44, struct.pack(">H", count & 0xFFFF))
        previous = 0
        for k in range(chain):
            node = e.alloc(0xA0)
            w32(e, node, (node_flags + k * 0x100) & MINUS)
            w32(e, node + 0x54, e.alloc(len(names[k]) + 1, names[k].encode("latin-1") + b"\0"))
            w32(e, (previous + 0xC) if previous else (header + 0x78), node)
            previous = node
        mesh = e.alloc(0x80)
        w32(e, mesh + 4, header)
        self.meshes.append(mesh)
        self.headers.append(header)
        if chain:
            self.loadable.append(mesh)
        return mesh, header

    def which(self, resource):
        return 1 if resource == self.resources[1] else 0

    def next_file(self, which, info):
        e = self.e
        if self.cursor[which] >= len(self.files[which]):
            return 0
        name = self.files[which][self.cursor[which]]
        self.cursor[which] += 1
        prefix = self.prefixes[which]
        e.write(info, struct.pack(">II", 100, 0x81))
        e.write(info + 0x24, (prefix + name).encode("latin-1") + b"\0")
        w32(e, info + 0x20, info + 0x24 + len(prefix))
        return 1

    def new(self, size):
        """operator new: the game's objects get a label and memory full of 0xa5."""
        e = self.e
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        if size in KIND:
            self.kinds[block] = KIND[size]
            self.L.newlab(block)
            self.L.rec("new", (size,))
        return block

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def virtual(self, obj, slot, *args, vptr=0x38):
        e = self.e
        return self.call(e.u32(e.u32(e.u32(obj + vptr) + slot)), obj, *args)

    def in_library(self):
        e, found, node, n = self.e, set(), self.e.u32(G["library"]), 0
        while node and n < 200:
            found.add(e.u32(node))
            node, n = e.u32(node + 0x14), n + 1
        return found

    def info(self, name):
        e = self.e
        info = e.alloc(0x128)
        e.write(info, struct.pack(">II", 100, 0x81))
        e.write(info + 0x24, b"pre:" + name.encode("latin-1") + b"\0")
        w32(e, info + 0x20, info + 0x28)
        return info

    # ---- one operation; None when the state does not allow it
    def op(self, op, a, text, text2):
        e, L = self.e, self.L
        slot = a[0] if 0 <= a[0] < SLOTS else None
        obj = self.slots[slot] if slot is not None else 0
        need_object = op in (2, 3, 4, 5, 6, 7, 22, 23, 24) or (op == 8 and a[0] >= 0)
        if need_object and not obj:
            return None
        if op in (0, 1, 13, 14, 15) and obj:
            return None
        cstr = lambda t: e.alloc(len(t) + 1, t.encode("latin-1") + b"\0")   # noqa: E731
        if op == 0:
            block = self.new(0x3C)
            self.call(OBJECT["ctor"], block, self.meshes[a[1]] if a[1] >= 0 else 0)
            self.slots[slot] = block
            return L.lab(block)
        if op == 1:
            block = self.new(0x84)
            self.call((ENGINE["ctor"], ENGINE["frame_ctor"], ENGINE["slider_ctor"])[a[1]], block,
                      self.meshes[a[2]] if a[2] >= 0 else 0)
            self.slots[slot] = block
            return L.lab(block)
        if op == 2:
            if obj in self.in_library():
                return None
            self.virtual(obj, 8, 1)
            self.slots[slot] = 0
            return 0
        if op == 3:
            change = e.alloc(0x28)
            e.write(change + 4, struct.pack(">6h", *[s16(v) for v in (a[2], a[2] * 3 + 1, a[2] * 5 + 2, a[2] + 1, a[2] * 7 + 3,
                                                                         a[2] * 11 + 4)]))
            self.virtual(obj, 0x14, a[1], 0 if a[3] else change)
            return 0
        if op == 4:
            self.virtual(obj, 0x1C, s16(a[1]))
            return 0
        if op in (5, 6):
            self.call(OBJECT["SetFrame" if op == 5 else "SetFrameBase"], obj, s16(a[1]))
            return 0
        if op == 7:
            x, y = e.alloc(4, struct.pack(">i", 5)), e.alloc(4, struct.pack(">i", 6))
            query = T.s32(self.virtual(obj, 0x18, a[1], a[2]))
            area = T.s32(self.virtual(obj, 0x24, x, y))
            self.virtual(obj, 0x20)
            self.virtual(obj, 0xC, 1, obj + 0x10, 2)
            self.virtual(obj, 0x10, 1, obj + 0x10, 2, 0)
            return query * 100 + area * 10 + (1 if e.u32(x) == 5 and e.u32(y) == 6 else 0)
        if op == 8:
            if a[0] >= 0 and obj in self.in_library():   # twice in the list: destroyed twice when it is emptied
                return None
            return T.s32(self.call(LIBRARY["Add"], obj if a[0] >= 0 else 0, cstr(text)))
        if op == 9:
            return L.lab(self.call(LIBRARY["FindName"], cstr(text)))
        if op == 10:
            return L.lab(self.call(LIBRARY["FindHash"], a[1]))
        if op == 11:
            if a[1] != 0:
                held = self.in_library()
                self.slots = [0 if s in held else s for s in self.slots]
            self.call(LIBRARY["RemoveAll"], s16(a[1]))
            w32(e, G["library"], 0)
            return 0
        if op == 12:
            return self.call(HASH, cstr(text), a[1])
        if op in (13, 14, 15):
            folder = FS.new_string(e, text2, False)
            result = self.call(MAIN[("AddFrameMesh", "AddSliderMesh", "AddMeshDrawObject")[op - 13]], self.info(text),
                               self.storage, folder)
            self.slots[slot] = result
            return L.lab(result)
        if op == 16:
            return T.s32(self.call(MAIN["LoadAllUIMeshes"]))
        if op == 17:
            return T.s32(self.call(MAIN["LoadLocalisedUIMeshes"]))
        if op == 18:
            self.call(MAIN["InitGeneralUI"])
            return 0
        if op == 19:
            self.call(MAIN["InitGeneral"], self.storage)
            return 0
        if op == 20:
            w32(e, G["screen_width"], a[1])
            e.write(G["ui_width"], struct.pack(">h", s16(a[2])))
            e.write(G["ui_height"], struct.pack(">h", s16(a[3])))
            return 0
        if op == 21:
            base = e.alloc(4, struct.pack(">I", VT_TICK))
            return self.call(MAIN["tick"], base) * 0x100000 + self.call(MAIN["lb_tick"], G["clock"])
        if op == 22:
            if self.kinds.get(obj) != 1:
                return None
            w32(e, self.node_word, a[1])
            w32(e, obj + 0x7C, self.node_word)
            return 0
        if op == 23:
            e.write(obj + 0xA, struct.pack(">h", s16(a[1])))
            return 0
        if op == 24:
            return L.lab(self.virtual(obj, 0x28))
        return -99

    # ---- the state
    def dump(self, obj):
        e, L = self.e, self.L
        h = lambda o: s16(struct.unpack(">H", e.read(obj + o, 2))[0])   # noqa: E731
        kind = self.kinds.get(obj, -1)
        out = [kind, L.lab(e.u32(obj))] + [h(o) for o in (4, 6, 8, 0xA, 0xC, 0xE, 0x14, 0x16, 0x18, 0x1A, 0x20, 0x22, 0x24, 0x26,
                                                           0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34)]
        if kind == 1:
            flags = e.u32(obj + 0x7C)
            out += [1 if e.read(obj + 0x3C, 0x40) == b"\xa5" * 0x40 else 0, 0 if flags == 0 else 1 if flags == self.node_word else -1,
                    T.s32(e.u32(obj + 0x80))]
        return out

    def state(self):
        e, L, out = self.e, self.L, []
        for obj in self.slots:
            out.append(L.lab(obj))
            if obj:
                out += self.dump(obj)
        nodes, node = [], e.u32(G["library"])
        while node and len(nodes) < 60:
            nodes.append(node)
            node = e.u32(node + 0x14)
        out.append(len(nodes))
        for node in nodes:
            out += [L.lab(node), e.u32(node + 0x10)] + list(struct.unpack(">3I", e.read(node + 4, 12))) + [L.lab(e.u32(node))]
            if e.u32(node):
                out += self.dump(e.u32(node))
        for header in self.headers:
            out.append(e.u32(header + 0x30))
            node = e.u32(header + 0x78)
            while node:
                out.append(e.u32(node))
                node = e.u32(node + 0xC)
        title = e.u32(G["title"])
        out.append(L.lab(title))
        if title:
            out += self.dump(title)
        count = struct.unpack(">H", e.read(G["cursor_table"] + 4, 2))[0]
        entries = e.u32(G["cursor_table"])
        out += [count, 1 if entries else 0] + [L.lab(e.u32(entries + 4 * k)) for k in range(min(count, 0x20))]
        out += [L.lab(e.u32(G["ui_heap"])), e.u32(self.node_word)]
        return out


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    if "c" not in FS.VT:
        FS.make_vtables(e)
    if not hasattr(e, "us_title_vtable"):
        e.us_title_vtable = T.persist_alloc(e, 0x40)
        other = T.fake_function(e, lambda emu: None)
        for slot in range(8, 0x40, 4):
            e.write(e.us_title_vtable + slot, struct.pack(">I", other))
        e.write(e.us_title_vtable + 0x14, struct.pack(">I", T.fake_function(e, title_change)))
    STUBS = build_stubs(e)


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


def script_ppc(e, case):
    side = PpcSide(e, case)
    answers, states, marks, skipped = [], [], [], []
    # The Mac has memory at address 0: LoadLocalisedUIMeshes marks "the object" as localised
    # there when a file made none (a write to 0x80). The emulator gets that page for the case.
    e.uc.mem_map(0, 0x1000)
    try:
        for op, a, text, text2 in case["script"]:
            answer = side.op(op, list(a), text, text2)
            skipped.append(answer is None)
            answers.append(SKIPPED if answer is None else answer)
            states.append(side.state())
            marks.append(len(side.L.log))
    finally:
        e.uc.mem_unmap(0, 0x1000)
    case["_skipped"] = skipped
    return dict(answers=answers, states=states, marks=marks, log=side.L.log)


def script_host(lib, case):
    begin = F(lib, "tpw_us_begin", None, C.c_char_p, C.c_char_p, C.c_char_p, C.c_char_p, C.c_char_p)
    paths = case.get("paths", ("data:ui", "data:language"))
    prefixes = case.get("prefixes", ("", ""))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    begin(case.get("language", "english").encode(), paths[0].encode(), paths[1].encode(), prefixes[0].encode(),
          prefixes[1].encode())
    add_file = F(lib, "tpw_us_file", None, C.c_int32, C.c_char_p)
    for which, names in enumerate(case.get("files", ([], []))):
        for name in names:
            add_file(which, name.encode("latin-1"))
    fixed_at = F(lib, "tpw_us_fixed", C.c_void_p, C.c_int32)
    fixed = {LAB["manager"]: fixed_at(0), LAB["resource0"]: fixed_at(1), LAB["resource1"]: fixed_at(2), LAB["storage"]: fixed_at(3),
             LAB["heap"]: fixed_at(4), LAB["text"]: fixed_at(5)}
    make_mesh = F(lib, "tpw_us_mesh", C.c_void_p, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32, C.c_char_p, C.POINTER(C.c_void_p))
    for index, (count, chain, header_flags, node_flags, names) in enumerate(case.get("meshes", ())):
        header = C.c_void_p()
        fixed[MESH_LABEL + index] = make_mesh(count, chain, header_flags, node_flags, "\n".join(names).encode("latin-1"),
                                              C.byref(header))
        fixed[HEADER_LABEL + index] = header.value
    CL.host_begin(lib, fixed, case.get("q"))
    op_fn = F(lib, "tpw_us_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32), C.c_char_p, C.c_char_p)
    state_fn = F(lib, "tpw_us_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 8192)()
    try:
        for (op, a, text, text2), skip in zip(case["script"], case["_skipped"]):
            if skip:
                answers.append(SKIPPED)
            else:
                answers.append(op_fn(op, (C.c_int32 * 4)(*[T.s32(v & MINUS) for v in a]), text.encode("latin-1"),
                                     text2.encode("latin-1")))
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, marks=marks, log=log)


# ---- script cases

MESH_NAMES = ["panel", "button_ok", "w_small_shadow", "a_very_long_node_name", "x", "", "Panel", "icon"]
FILE_NAMES = ["!frame.md2", "!FRAME.MD2", "!f_plain.md2", "!cbut.md2", "!slider.md2", "!Slider.md2", "abc.md2", "panel.md2",
              "!other.md2", "w_small_shadow.md2", "W_SMALL_SHADOW.MD2", "x", "ab", "", "!frame", "icons.MD2", "!frame.md2x",
              "long_file_name_here.md2"]
LIBRARY_NAMES = ["a", "b", "ab", "ba", "panel", "Panel", "", "twelve_chars", "more_than_twelve_characters", "!frame", "!slider",
                 "button_ok", "x"]
CHANGE_FLAGS = [0, 1, 4, 0x40, 0x80000, 1 | 4, 0x80045, 2, 0x10, 0xC00, MINUS, 0x80000 | 1]


def mesh_spec(r):
    chain = r.choice([0, 1, 1, 2, 3, 5])
    return (r.choice([chain, chain, chain + 2, 0, 0x7FFF, 0xFFFF]), chain, r.getrandbits(32) & ~0x200 if r.random() < 0.7 else MINUS,
            r.getrandbits(32) & 0xFFFFF0FF, [r.choice(MESH_NAMES) for _ in range(chain)])


def op(code, a0=0, a1=0, a2=0, a3=0, text="", text2=""):
    return (code, (a0, a1, a2, a3), text, text2)


def random_script(r, meshes, length):
    script = []
    for _ in range(length):
        slot = r.randrange(SLOTS)
        mesh = r.randrange(-1, meshes) if meshes else -1
        k = r.random()
        if k < 0.08:
            script.append(op(0, slot, mesh))
        elif k < 0.22:
            script.append(op(1, slot, r.randrange(3), mesh))
        elif k < 0.30:
            script.append(op(2, slot))
        elif k < 0.38:
            script.append(op(3, slot, r.choice(CHANGE_FLAGS), r.randrange(-3, 12), 1 if r.random() < 0.1 else 0))
        elif k < 0.46:
            script.append(op(4, slot, r.choice([0, 0, 1, 1, 2, -1])))
        elif k < 0.50:
            script.append(op(r.choice([5, 6]), slot, r.randrange(-2, 8)))
        elif k < 0.53:
            script.append(op(7, slot, r.getrandbits(8), r.randrange(100)))
        elif k < 0.63:
            script.append(op(8, slot if r.random() < 0.93 else -1, text=r.choice(LIBRARY_NAMES)))
        elif k < 0.68:
            script.append(op(9, text=r.choice(LIBRARY_NAMES + ["missing"])))
        elif k < 0.71:
            script.append(op(10, 0, T.s32(ui_hash(r.choice(LIBRARY_NAMES), 0x2F)) if r.random() < 0.8 else r.getrandbits(31)))
        elif k < 0.74:
            script.append(op(11, 0, r.choice([0, 1, 1, -1])))
        elif k < 0.76:
            script.append(op(12, 0, r.choice([0x2F, 0x2F, 1, 0, 0x101]), text=r.choice(LIBRARY_NAMES + ["\xe9t\xe9", "Zz~"])))
        elif k < 0.84:
            script.append(op(r.choice([13, 14, 15]), slot, text=r.choice(FILE_NAMES), text2=r.choice(["data:ui:", "", "f:"])))
        elif k < 0.87:
            script.append(op(16))
        elif k < 0.90:
            script.append(op(17))
        elif k < 0.92:
            script.append(op(r.choice([18, 19])))
        elif k < 0.94:
            script.append(op(20, 0, r.choice([640, 800, 1024, 0]), r.choice([640, 800, 1024, 1, -5]), r.choice([480, 600, 768, 7])))
        elif k < 0.95:
            script.append(op(21))
        elif k < 0.97:
            script.append(op(22, slot, r.getrandbits(31)))
        elif k < 0.985:
            script.append(op(23, slot, r.randrange(-1, 9)))
        else:
            script.append(op(24, slot))
    return script


def ui_hash(text, multiplier):
    value = 0
    for byte in text.encode("latin-1"):
        value = (multiplier * (((byte - 256 if byte > 127 else byte) & MINUS) ^ value)) & MINUS
    return value


def script_case(r):
    meshes = [mesh_spec(r) for _ in range(r.randrange(0, 5))]
    q = {}
    if r.random() < 0.4:
        q["Load"] = [r.choice([1, 1, 1, 0]) for _ in range(12)] + [1]
    if r.random() < 0.2:
        q["FindResource"] = [r.choice([1, 1, 0, 2]) for _ in range(4)] + [1]
    if r.random() < 0.15:
        q["FindFirst"] = [r.choice([-1, -1, 0, 2]) for _ in range(3)] + [-1]
    if r.random() < 0.15:
        q["FindNext"] = [r.choice([-1, -1, -1, 2]) for _ in range(8)] + [-1]
    if r.random() < 0.3:
        q["Hebrew"] = [r.randrange(2)]
    q["Clock"] = [r.getrandbits(20)]
    return dict(meshes=meshes, language=r.choice(["english", "french", "us", ""]),
                paths=(r.choice(["data:ui", "data:ui:", "ui"]), r.choice(["data:language", "lang:"])),
                prefixes=(r.choice(["", "", "sub:"]), r.choice(["", "english:"])),
                files=([r.choice(FILE_NAMES) for _ in range(r.randrange(0, 9))], [r.choice(FILE_NAMES) for _ in range(r.randrange(0, 5))]),
                q=q, script=random_script(r, len(meshes), r.randrange(6, 30)))


def script_edge():
    r = random.Random(77)
    three = [(3, 3, 0x21, 0x1000, ["panel", "inner", "leaf"]), (1, 1, MINUS, 0, ["button_ok"]), (0, 0, 0, 0, [])]
    # A base object: every member, its changes, the frame limits.
    script = [op(0, 0, 0), op(7, 0, 3, 4), op(24, 0), op(4, 0, 1), op(5, 0, 2), op(23, 0, 4), op(5, 0, 2), op(5, 0, 4),
              op(5, 0, -1), op(6, 0, 3), op(6, 0, 4)]
    for flags in CHANGE_FLAGS:                    # each change from the same start: frame 0, base 0
        script += [op(5, 0, 0), op(6, 0, 0), op(3, 0, flags, 2)]
    # a frame count of nothing lets no frame through, not even 0
    script += [op(3, 0, 1, 9), op(3, 0, 1, 2, 1), op(6, 0, 3), op(5, 0, 2), op(23, 0, 0), op(6, 0, 0), op(5, 0, 0), op(23, 0, -1),
               op(6, 0, -2), op(2, 0), op(0, 1, -1), op(2, 1)]
    yield dict(meshes=three, script=script)
    # The three engine mesh objects, on a 640 screen and a wider one, with and without a mesh.
    for width in (800, 640):
        for kind in (0, 1, 2):
            yield dict(meshes=three, script=[op(20, 0, width, 800, 600), op(1, 0, kind, 0), op(1, 1, kind, 1), op(1, 2, kind, 2),
                                             op(1, 3, kind, -1), op(4, 0, 1), op(4, 0, 1), op(4, 0, 0), op(22, 0, 0x12345678),
                                             op(4, 0, 2), op(4, 0, 0), op(4, 3, 1), op(3, 0, 5, 1), op(24, 0), op(7, 0, 1, 1),
                                             op(2, 0), op(8, 1, text="one"), op(11, 0, 0), op(2, 1), op(2, 2), op(2, 3)])
    # The library: the walk that never looks at the last node, empty names, names of twelve
    # characters and more, both searches, both ways of emptying it.
    names = ["a", "b", "a", "c", "c", "b", "", "twelve_chars", "more_than_twelve_characters", "ab", "ba"]
    script = []
    for slot in range(SLOTS):
        script.append(op(0 if slot % 2 else 1, slot, 0, 0) if slot % 2 else op(1, slot, slot % 3, 0))
    for k, name in enumerate(names):
        script.append(op(8, k % SLOTS, text=name))
    script += [op(8, -1, text="null")] + [op(9, text=name) for name in ("a", "b", "c", "twelve_chars", "missing", "")]
    script += [op(10, 0, T.s32(ui_hash("b", 0x2F))), op(10, 0, 12345), op(2, 0), op(11, 0, 0), op(2, 0), op(8, 1, text="again"),
               op(8, 2, text="again"), op(8, 3, text="again"), op(11, 0, 1), op(9, text="again")]
    yield dict(meshes=three, script=script)
    yield dict(meshes=three, script=[op(12, 0, m, text=t) for m in (0x2F, 1, 0) for t in ("", "a", "ab", "ba", "\xe9", "!frame")])
    # Each kind of file through the three makers, loading and not.
    for load in ([1], [0], [1, 0, 1]):
        for maker in (13, 14, 15):
            script = []
            for k, name in enumerate(FILE_NAMES):
                script += [op(maker, k % SLOTS, text=name, text2="data:ui:"), op(11, 0, 0)]
            yield dict(meshes=three, q={"Load": load}, script=script)
    # The two loaders: every file, no resource, a search that fails or answers oddly.
    for q in ({}, {"FindResource": [0]}, {"FindResource": [2]}, {"FindFirst": [0]}, {"FindFirst": [2]}, {"FindNext": [2, 2, -1]},
              {"Load": [1, 0, 1, 1, 0]}):
        for prefixes in (("", ""), ("sub:", "english:")):
            yield dict(meshes=three, files=(FILE_NAMES, ["panel.md2", "abc.md2", "icons.MD2"]), prefixes=prefixes, q=q,
                       script=[op(16), op(17), op(9, text="!frame"), op(9, text="panel"), op(11, 0, 1)])
    yield dict(meshes=three, files=([], []), script=[op(16), op(17)])
    yield dict(meshes=[], files=(FILE_NAMES, FILE_NAMES), script=[op(16), op(17)])
    # The localised meshes one by one (a file that makes no object, in each place).
    for files in (["panel.md2"], ["!frame.md2"], ["w_small_shadow.md2", "panel.md2"], ["panel.md2", "!x.md2", "abc.md2"]):
        for language in ("english", ""):
            yield dict(meshes=three, files=([], files), language=language, script=[op(17), op(17)])
    # InitGeneralUI, twice, and through UIInterface; the clocks.
    for hebrew in (0, 1):
        yield dict(meshes=three, files=(["!frame.md2", "panel.md2", "!slider.md2"], ["abc.md2"]), q={"Hebrew": [hebrew], "Clock": [77]},
                   script=[op(20, 0, 800, 1024, 768), op(18), op(21), op(20, 0, 640, 640, 480), op(19), op(21), op(11, 0, 1)])
    for _ in range(60):
        yield script_case(r)


# ======================================================================= the text file reader

PARSE_ORDER = ["IsComment", "IsLetter", "IsNumber", "IsMiscChar", "IsWhiteSpace", "SkipWhitespace", "GetString"]


def parse_ppc(e, case):
    text, size, position, which = case["text"], case["size"], case["position"], case["op"]
    base = e.alloc(len(text) + 4, text + b"\0\0\0\0")
    parse = e.alloc(12, struct.pack(">III", base + position, position, size))
    out = e.alloc(0x110, b"\x7e" * 0x110)
    args = [parse]
    if which == 6:
        args += [out] + [v & MINUS for v in case["flags"]]
    result = T.call_ppc(e, PARSE[PARSE_ORDER[which]], args)[0]
    cursor, pos, _ = struct.unpack(">III", e.read(parse, 12))
    return [T.s32(result), pos, cursor - base, e.read(out, 0x110).split(b"\0")[0] if which == 6 else b""]


def parse_host(lib, case):
    fn = F(lib, "tpw_us_parse", None, C.c_char_p, C.c_uint32, C.c_uint32, C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int64),
           C.c_char_p)
    text = C.create_string_buffer(case["text"] + b"\0\0\0\0", len(case["text"]) + 4)
    out = (C.c_int64 * 3)()
    string = C.create_string_buffer(b"\x7e" * 0x110, 0x110)
    fn(text, case["size"], case["position"], case["op"], (C.c_int32 * 5)(*case["flags"]), out, string)
    return [out[0], out[1], out[2], string.raw.split(b"\0")[0] if case["op"] == 6 else b""]


PARSE_ALPHABET = b"abzAZmM09 5\t\r\n#;!/:@[`{~ _-.\x00\x80\xff\x1f\x7f,"


def parse_case(r, text=None):
    if text is None:
        text = bytes(r.choice(PARSE_ALPHABET) for _ in range(r.randrange(1, 40)))
    size = r.choice([len(text) - 1, len(text) - 1, r.randrange(len(text)), 0])
    position = r.choice([0, 0, r.randrange(size + 1), size])
    return dict(text=text, size=size, position=position, op=r.randrange(7),
                flags=[r.choice([0x100, 0x100, 8, 3, 2, 1, 0, -1]), r.randrange(2), r.randrange(2), r.randrange(2), r.randrange(2)])


def parse_edge():
    r = random.Random(5)
    for byte in range(256):                       # every character through the five classes
        for which in range(5):
            yield dict(text=bytes([byte, 0x41]), size=1, position=0, op=which, flags=[0, 0, 0, 0, 0])
    for text in (b"rides.md2\r\ncoaster.md2 # all\n", b"  \t\n", b"abc", b"abc def", b"12ab!#x", b";#", b"a", b""):
        text = text + b"Z"                        # the character past the end is a letter
        for size in range(len(text)):
            for position in range(size + 1):
                yield dict(text=text, size=size, position=position, op=5, flags=[0, 0, 0, 0, 0])
                for flags in ([0x100, 1, 1, 1, 1], [0x100, 1, 0, 0, 0], [0x100, 0, 1, 0, 0], [0x100, 0, 0, 1, 0],
                              [0x100, 0, 0, 0, 1], [4, 1, 1, 1, 1], [1, 1, 1, 1, 1], [0, 1, 1, 1, 1], [0x100, 0, 0, 0, 0]):
                    yield dict(text=text, size=size, position=position, op=6, flags=flags)
    for _ in range(300):
        yield parse_case(r)


# ======================================================================= rectangles and cursors


class GeoSide:
    def __init__(self, e, case):
        self.e = e
        e.us = self
        e.heap_in_scratch = True
        self.kinds = {}
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        self.rects = [0, 0]
        self.region = 0
        self.table = 0
        self.table_state = "none"
        self.cursors = [0] * 6

    new = PpcSide.new
    call = PpcSide.call

    def virtual(self, obj, slot, *args, vptr=0):
        e = self.e
        return self.call(e.u32(e.u32(e.u32(obj + vptr) + slot)), obj, *args)

    def words(self, mask, count):
        e = self.e
        cells = [e.alloc(4, struct.pack(">hh", -7, -7)) for _ in range(count)]
        return cells, [cell if mask >> k & 1 else 0 for k, cell in enumerate(cells)]

    def read(self, cells):
        return [s16(struct.unpack(">H", self.e.read(cell, 2))[0]) for cell in cells] + [-7] * (4 - len(cells))

    def op(self, op, a):
        e, L = self.e, self.L
        rect, other = self.rects[a[0] & 1], self.rects[~a[0] & 1]
        none = [-7777] * 4
        if op in (2, 3, 4, 5, 6, 7, 9, 10) and not rect:
            return None
        if op in (8, 11) and not (rect and other):
            return None
        if op in (13, 14) and not self.region:
            return None
        if op in (21, 22, 23, 24, 25, 26) and not self.table:
            return None
        if op in (0, 1):
            block = e.alloc(0xC, b"\xa5" * 0xC)
            if op == 0:
                self.call(RECT["ctor"], block)
            else:
                self.call(RECT["ctor4"], block, *[s16(v) for v in a[1:5]])
            self.rects[a[0] & 1] = block
            return 0, none
        if op == 2:
            self.virtual(rect, 0x18, 1)
            self.rects[a[0] & 1] = 0
            return 0, none
        if op == 3:
            self.call(RECT["Set"], rect, *[s16(v) for v in a[1:5]])
            return 0, none
        if op == 4:
            cells, args = self.words(a[1], 4)
            self.call(RECT["Get"], rect, *args)
            return 0, self.read(cells)
        if op == 5:
            cells, args = self.words(a[1], 2)
            self.call(RECT["GetDims"], rect, *args)
            return 0, self.read(cells)
        if op == 6:
            return self.virtual(rect, 8, s16(a[1]), s16(a[2])) & 0xFF, none
        if op == 7:
            cells, args = self.words(a[1], 2)
            self.virtual(rect, 0xC, *args)
            return 0, self.read(cells)
        if op == 8:
            return self.call(RECT["RectIntersects"], rect, other), none
        if op == 9:
            self.virtual(rect, 0x10, s16(a[1]), s16(a[2]))
            return 0, none
        if op == 10:
            self.virtual(rect, 0x14, *[s16(v) for v in a[1:5]])
            return 0, none
        if op == 11:
            return T.s32(self.virtual(rect, 0x1C, other)), none
        if op == 12:
            self.region = e.alloc(4, struct.pack(">I", VT_REGION))
            return 0, none
        if op == 13:
            self.virtual(self.region, 0x18, 1)
            self.region = 0
            return 0, none
        if op == 14:
            cells, args = self.words(3, 2)
            inside = self.virtual(self.region, 8, s16(a[1]), s16(a[2])) & 0xFF
            self.virtual(self.region, 0xC, *args)
            self.virtual(self.region, 0x10, s16(a[1]), s16(a[2]))
            self.virtual(self.region, 0x14, *[s16(v) for v in a[1:5]])
            return inside * 2 + T.s32(self.virtual(self.region, 0x1C, self.region)), self.read(cells)
        if op == 20:
            self.table = e.alloc(8, b"\xa5" * 8)
            self.call(TABLE["ctor"], self.table)
            self.table_state = "empty"
            return 0, none
        if op == 21:
            result = T.s32(self.call(TABLE["Allocate"], self.table, a[1] & 0xFFFF))
            self.table_state = "allocated"
            return result, none
        if op == 22:
            if self.table_state == "freed":      # the table keeps the pointer it has freed
                return None
            self.call(TABLE["DeAllocate"], self.table)
            self.table_state = "freed" if self.table_state == "allocated" else self.table_state
            return 0, none
        if op == 23:
            k = a[1] % 6
            if a[3] and not self.cursors[k]:
                self.cursors[k] = self.new(0x14)
                self.call(CURSOR["ctor"], self.cursors[k])
            result = T.s32(self.call(TABLE["SetEntry"], self.table, a[2] & 0xFFFF, self.cursors[k]))
            if result:
                self.cursors[k] = 0
            return result, none
        if op == 24:
            return L.lab(self.call(TABLE["GetEntry"], self.table, a[1] & 0xFFFF)), none
        if op == 25:
            self.call(TABLE["dtor"], self.table, 0)
            self.table = 0
            return 0, none
        if op == 26:
            cursor = self.call(TABLE["GetEntry"], self.table, a[1] & 0xFFFF)
            if not cursor:
                return -1, none
            v = lambda slot, *args: self.virtual(cursor, slot, *args, vptr=0x10)   # noqa: E731
            out = [v(0xC) & 0xFF]
            self.call(CURSOR["SetHot"], cursor, a[2] & 0xFF, a[3] & 0xFF)
            if a[4] & 1:
                v(0x18)
            if a[4] & 2:
                v(0x14)
            v(0x10, 0, ord("x"))
            v(0x1C, 3)
            v(0x20, 1, 2)
            v(0x24, 5)
            out += [v(0xC) & 0xFF] + [T.s8(b) for b in e.read(cursor + 1, 2)]
            return e.u32(cursor + 0xC), out
        return -99, none

    def state(self):
        e, out = self.e, []
        for rect in self.rects:
            out.append(1 if rect else 0)
            if rect:
                out += list(struct.unpack(">4h", e.read(rect + 4, 8)))
        out.append(1 if self.table else 0)
        if self.table:
            count = struct.unpack(">H", e.read(self.table + 4, 2))[0]
            entries = e.u32(self.table)
            out += [count] + [self.L.lab(e.u32(entries + 4 * k)) for k in range(min(count, 16))]
        return out


def geo_ppc(e, case):
    side = GeoSide(e, case)
    answers, states, skipped = [], [], []
    for op_code, a in case["script"]:
        answer = side.op(op_code, list(a))
        skipped.append(answer is None)
        answers.append(SKIPPED if answer is None else [answer[0]] + list(answer[1]))
        states.append(side.state())
    case["_skipped"] = skipped
    return dict(answers=answers, states=states, log=side.L.log)


def geo_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib["tpw_us_geo_begin"]()
    CL.host_begin(lib, {}, case.get("q"))
    op_fn = F(lib, "tpw_us_geo", C.c_int64, C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int64))
    state_fn = F(lib, "tpw_us_geo_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states = [], []
    out = (C.c_int64 * 4)()
    values = (C.c_int64 * 64)()
    try:
        for (op_code, a), skip in zip(case["script"], case["_skipped"]):
            if skip:
                answers.append(SKIPPED)
            else:
                answers.append([op_fn(op_code, (C.c_int32 * 6)(*[T.s32(v & MINUS) for v in a]), out)] + list(out))
            states.append(list(values[:state_fn(values)]))
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, log=log)


def geo_script(r, length):
    script = []
    # mostly a few values, so that edges meet edges
    coordinate = lambda: r.choice([0, 5, 10, 10, 15, 20, 20, 25, 30, 1, 9, 11, 19, 21, -4, 0x7FFF, -0x8000, r.randrange(-30, 60)])   # noqa: E731
    for _ in range(length):
        k = r.random()
        which = r.randrange(2)
        four = [coordinate() for _ in range(4)]
        if k < 0.06:
            script.append((0, [which, 0, 0, 0, 0, 0]))
        elif k < 0.22:
            if r.random() < 0.7:                  # mostly proper rectangles
                four = [min(four[0], four[2]), min(four[1], four[3]), max(four[0], four[2]), max(four[1], four[3])]
            script.append((1, [which] + four + [0]))
        elif k < 0.25:
            script.append((2, [which, 0, 0, 0, 0, 0]))
        elif k < 0.33:
            script.append((3, [which] + four + [0]))
        elif k < 0.39:
            script.append((r.choice([4, 5, 7]), [which, r.randrange(16), 0, 0, 0, 0]))
        elif k < 0.47:
            script.append((6, [which, coordinate(), coordinate(), 0, 0, 0]))
        elif k < 0.62:
            script.append((8, [which, 0, 0, 0, 0, 0]))
        elif k < 0.68:
            script.append((r.choice([9, 10]), [which] + four + [0]))
        elif k < 0.70:
            script.append((11, [which, 0, 0, 0, 0, 0]))
        elif k < 0.73:
            script.append((r.choice([12, 13, 14]), [0] + four + [0]))
        elif k < 0.76:
            script.append((20, [0] * 6))
        elif k < 0.81:
            script.append((21, [0, r.choice([0, 1, 3, 5, 0x16]), 0, 0, 0, 0]))
        elif k < 0.84:
            script.append((22, [0] * 6))
        elif k < 0.93:
            script.append((23, [0, r.randrange(6), r.choice([0, 1, 2, 4, 5, 0x15, 0x16, 0xFFFF]), r.randrange(4) != 0, 0, 0]))
        elif k < 0.96:
            script.append((24, [0, r.choice([0, 1, 2, 5, 0x16, 0xFFFF]), 0, 0, 0, 0]))
        elif k < 0.97:
            script.append((25, [0] * 6))
        else:
            script.append((26, [0, r.randrange(6), r.randrange(-128, 128), r.randrange(-128, 128), r.randrange(4), 0]))
    return script


def geo_edge():
    r = random.Random(9)
    # One rectangle against others in every relation to it.
    base = [10, 10, 20, 20]
    others = [base, [10, 10, 20, 10], [0, 0, 30, 30], [12, 12, 18, 18], [5, 5, 15, 15], [15, 5, 25, 15], [15, 15, 25, 25],
              [5, 15, 15, 25], [0, 12, 30, 18], [12, 0, 18, 30], [0, 0, 5, 5], [11, 11, 19, 19], [9, 9, 21, 21], [10, 0, 20, 12],
              [10, 18, 20, 30], [0, 10, 12, 20], [18, 10, 30, 20], [10, 10, 20, 21], [20, 20, 30, 30], [0, 15, 30, 40],
              [15, 0, 40, 30], [10, 12, 20, 18], [12, 10, 18, 20],
              # an edge of the other on an edge of this one, spanning it the other way
              [0, 10, 30, 18], [0, 12, 30, 20], [0, 10, 30, 20], [0, 5, 30, 10], [0, 20, 30, 25], [10, 0, 18, 30], [12, 0, 20, 30],
              [10, 0, 20, 30], [5, 0, 10, 30], [20, 0, 25, 30], [0, 12, 20, 18], [10, 12, 30, 18], [12, 0, 18, 20], [12, 10, 18, 30]]
    for other in others:
        yield dict(script=[(1, [0] + base + [0]), (1, [1] + other + [0]), (8, [0] * 6), (8, [1, 0, 0, 0, 0, 0]), (11, [0] * 6)])
    yield dict(script=[(0, [0] * 6), (4, [0, 15, 0, 0, 0, 0]), (3, [0, 1, 2, 9, 12, 0]), (4, [0, 5, 0, 0, 0, 0]), (4, [0, 0, 0, 0, 0, 0]),
                       (5, [0, 3, 0, 0, 0, 0]), (5, [0, 1, 0, 0, 0, 0]), (5, [0, 0, 0, 0, 0, 0]), (7, [0, 3, 0, 0, 0, 0]),
                       (7, [0, 2, 0, 0, 0, 0]), (3, [0, 9, 12, 1, 2, 0]), (7, [0, 3, 0, 0, 0, 0]), (3, [0, -0x8000, 0, 0x7FFF, 1, 0]),
                       (7, [0, 3, 0, 0, 0, 0]), (5, [0, 3, 0, 0, 0, 0]), (9, [0, 3, -4, 0, 0, 0]), (10, [0, 1, 2, 3, 4, 0]),
                       (9, [0, 0x7FFF, 0x7FFF, 0, 0, 0]), (2, [0] * 6)] +
                      [(6, [1, x, y, 0, 0, 0]) for x in (9, 10, 19, 20) for y in (9, 10, 19, 20)])
    yield dict(script=[(1, [1, 10, 10, 20, 20, 0])] + [(6, [1, x, y, 0, 0, 0]) for x in (9, 10, 19, 20) for y in (9, 10, 19, 20)])
    yield dict(script=[(12, [0] * 6), (14, [0, 1, 2, 3, 4, 0]), (13, [0] * 6)])
    # The cursor table: before it is allocated, full, out of range, emptied.
    yield dict(script=[(20, [0] * 6), (24, [0, 0, 0, 0, 0, 0]), (23, [0, 0, 0, 1, 0, 0]), (22, [0] * 6), (21, [0, 3, 0, 0, 0, 0]),
                       (23, [0, 0, 0, 1, 0, 0]), (23, [0, 1, 0, 1, 0, 0]), (23, [0, 1, 2, 1, 0, 0]), (23, [0, 2, 3, 1, 0, 0]),
                       (23, [0, 3, 1, 0, 0, 0]), (24, [0, 0, 0, 0, 0, 0]), (24, [0, 1, 0, 0, 0, 0]), (24, [0, 2, 0, 0, 0, 0]),
                       (24, [0, 3, 0, 0, 0, 0]), (26, [0, 0, 5, -3, 0, 0]), (26, [0, 0, -128, 127, 1, 0]), (26, [0, 0, 1, 1, 2, 0]),
                       (26, [0, 2, 0, 0, 3, 0]), (26, [0, 1, 0, 0, 0, 0]), (22, [0] * 6), (22, [0] * 6), (24, [0, 0, 0, 0, 0, 0]),
                       (21, [0, 0x16, 0, 0, 0, 0]), (23, [0, 4, 0x15, 1, 0, 0]), (23, [0, 5, 0x16, 1, 0, 0]), (25, [0] * 6)])
    for _ in range(80):
        yield dict(script=geo_script(r, r.randrange(8, 40)))


TESTS = [
    FnTest("the text file reader", with_stubs(parse_ppc), parse_host, edge=parse_edge, rnd=parse_case),
    FnTest("UI rectangles, cursors and the cursor table", with_stubs(geo_ppc), geo_host, edge=geo_edge,
           rnd=lambda r: dict(script=geo_script(r, r.randrange(8, 40)))),
    FnTest("UI draw objects, the library and the interface's meshes", with_stubs(script_ppc), script_host, edge=script_edge,
           rnd=script_case),
]
