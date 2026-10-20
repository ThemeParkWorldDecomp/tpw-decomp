"""The park's second-stage systems (unit w111, main app; PT_SetRange from the engine): LoadLevelAttributeMap, the poly search
table (SetupForPolySearch, SetdownFromPolySearch), SetupHeightmapSystem, ClosedownRideHoardings, DestroyLevelLights, the highlight
colour buffers, InitialiseCoasterTypes and DestroyCoasterTypes, the cell system (LLCC_SystemInit, LLCC_SystemDeInit),
DataErrorHandler, the script system (SCR_InitScriptSystem), the dynamic cell meshes (DC_SetupDynamicCellMeshes,
DC_FreeDynamicCellMeshes) and the engine's PT_SetRange.

Every callee outside the 17 functions is a fake on both sides that records the same event (the allocator and its free, the mesh
and texture freers, the dynamic mesh's constructor and destructor, the file lookup, the map loader, the cell system's callees,
CMapWho::Unlink, rand, the height field's range setter). The functions run as the original on the PowerPC side and as src/ on the
host. Every global the functions touch is filled with random words (or a sentinel pointer) before the call and compared after, so
a missing, extra or wrong-width store shows; the objects the functions free or delete (coaster types, lights, meshes) are built on
both sides with fixed labels and what is left of them is compared. DataErrorHandler is the one deliberate difference: the Mac's
box (MessageBoxA, flags 0x31) is a UDebug line on the host, so the PowerPC fake records the box as that line (a box with another
port or flags would be recorded as an error) and the answer 2 (Cancel) is not tested (the host always answers true).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LevelSystems"
MAIN = "SimTheme Park"
ENGINE = "engine symbols shared"

# Entry points (main app unless noted).
ATTRIB, POLY_UP, POLY_DOWN, HEIGHTMAP, HOARDINGS, LIGHTS, HILITE_UP, HILITE_DOWN = (
    0x1005BE44, 0x1005C7E8, 0x1005C850, 0x1005F6F4, 0x10061448, 0x1006446C, 0x1006D1D0, 0x1006D26C)
COASTER_INIT, COASTER_DESTROY, CELL_INIT, CELL_DEINIT, DATA_ERROR, SCRIPT_INIT, DC_SETUP, DC_FREE = (
    0x10042478, 0x1003ACF0, 0x10090B68, 0x10090CDC, 0x100B9834, 0x100D8D94, 0x100DF8FC, 0x100E01AC)
PT_SET_RANGE = 0x1000BDE4          # engine
OWN = {ATTRIB, POLY_UP, POLY_DOWN, HEIGHTMAP, HOARDINGS, LIGHTS, HILITE_UP, HILITE_DOWN, COASTER_INIT, COASTER_DESTROY,
       CELL_INIT, CELL_DEINIT, DATA_ERROR, SCRIPT_INIT, DC_SETUP, DC_FREE}
OPS = {  # op number in tpw_ls_call -> entry
    0: ATTRIB, 1: POLY_UP, 2: POLY_DOWN, 3: HEIGHTMAP, 4: HOARDINGS, 5: LIGHTS, 6: HILITE_UP, 7: HILITE_DOWN, 8: COASTER_INIT,
    9: COASTER_DESTROY, 10: CELL_INIT, 11: CELL_DEINIT, 12: DATA_ERROR, 13: SCRIPT_INIT, 14: DC_SETUP, 15: DC_FREE,
    16: PT_SET_RANGE}

SENTINEL = 0x6A5A0010
BLOCK_LABEL, SCRIPT_LABEL = 20, 24
MAP_LABEL, HOARD_LABEL, CELL_LABEL, WORLD_MAP_LABEL, HFIELD_LABEL = 1, 2, 3, 4, 5
TABLE_LABELS = (6, 7, 8)
LIGHT_LABELS = (31, 32)
MESH_LABELS = (51, 52, 53)
HFIELD_SLOT = 0x100AF11C           # engine: gei_HField

# Global -> (PowerPC address, words, width in bytes, kind). kind: w words, f floats (bits), p pointers (4 bytes there, 8 on the host).
STATE = {
    "gPolySearchCount": (0x102C1E18, 1, 4, "w"), "gPolySearchTable": (0x102C1E1C, 1, 4, "p"),
    "gHeightmapScaleA": (0x102C1E0C, 1, 4, "f"), "gHeightmapScaleB": (0x102C1E08, 1, 4, "f"),
    "gHighlightFaceColours": (0x102D1F10, 1, 4, "p"), "gHighlightNodeColours": (0x102D1F0C, 1, 4, "p"),
    "gMaxMeshFaces4": (0x102D1F14, 1, 4, "w"), "gMaxMeshF3E": (0x102D1F18, 1, 4, "w"), "gMaxMeshNodes": (0x102D1F1C, 1, 4, "w"),
    "gObjectTableCount": (0x102B730C, 1, 4, "w"), "gObjectTableFirstFree": (0x102B7308, 1, 4, "w"),
    "gObjectTableLowest": (0x102B7304, 1, 4, "w"),
    "gExtrudeFaceCountA": (0x102B6988, 1, 4, "w"), "gExtrudeFaceCountB": (0x102B6984, 1, 4, "w"),
    "gCoasterTypeCount": (0x102B68B8, 1, 4, "w"), "gCoasterTypes": (0x102B68BC, 50, 4, "p"),
    "gCoasterTextureBase": (0x1029CE20, 1, 4, "w"),
    "gScriptInitialised": (0x102F7C28, 1, 4, "w"), "gScriptF7C2C": (0x102F7C2C, 1, 4, "w"),
    "gScriptTableSize": (0x102F7C14, 1, 4, "w"), "gScriptTable": (0x102F7C18, 1, 4, "p"),
    "gDynamicCellF9BD4": (0x102F9BD4, 1, 4, "w"), "gDynamicCellsReady": (0x102F9BD8, 1, 4, "w"),
    "gDynamicCellRandom": (0x102F9BDC, 110, 4, "w"), "gDynamicCellF9D94": (0x102F9D94, 1, 4, "w"),
    "gDynamicCellF9D98": (0x102F9D98, 1, 4, "f"), "gDynamicCellF9D9C": (0x102F9D9C, 1, 4, "f"),
    "gDynamicCellF9DA0": (0x102F9DA0, 1, 4, "f"), "gDynamicCellFBDBC": (0x102FBDBC, 1, 4, "w"),
    "gDynamicCellFlag": (0x102A031C, 1, 1, "w"),
    "gBlueprintMesh": (0x1034C188, 1, 4, "p"), "gWaterMesh": (0x1034C184, 1, 4, "p"), "gFlagsMesh": (0x1034C180, 1, 4, "p"),
    "gLLCC4818": (0x102E4818, 1, 1, "w"), "gLLCC47C4": (0x102E47C4, 1, 1, "w"), "gLLCCWidth": (0x102E47FC, 1, 4, "w"),
    "gLLCCHeight": (0x102E47F8, 1, 4, "w"), "gLLCC47CC": (0x102E47CC, 1, 4, "w"), "gLLCC4814": (0x102E4814, 1, 4, "w"),
    "gLLCC4810": (0x102E4810, 1, 4, "w"), "gLLCC480C": (0x102E480C, 1, 4, "w"), "gLLCC4808": (0x102E4808, 1, 4, "w"),
    "gLLCC4804": (0x102E4804, 1, 4, "w"), "gLLCC4800": (0x102E4800, 1, 4, "w"), "gLLCCDE0E2": (0x102DE0E2, 1, 2, "w"),
    "gLLCCC214": (0x102DC214, 1, 4, "w"), "gLLCCC210": (0x102DC210, 1, 4, "w"), "gLLCCC208": (0x102DC208, 1, 4, "w"),
    "gLLCCC204": (0x102DC204, 1, 4, "w"), "gLLCCC200": (0x102DC200, 1, 4, "w"), "gLLCC47EC": (0x102E47EC, 1, 4, "w"),
    "gLLCC47E8": (0x102E47E8, 1, 4, "w"), "gLLCC47E4": (0x102E47E4, 1, 4, "w"), "gLLCC47E0": (0x102E47E0, 1, 4, "w"),
    "gLLCCDB6C": (0x102DDB6C, 1, 4, "w"), "gLLCCDB68": (0x102DDB68, 1, 2, "w"), "gLLCC47C8": (0x102E47C8, 1, 4, "w"),
    "GlobalDiffuse": (None, 4, 4, "f"),           # an engine data import (its slot is the data itself)
}
CELL_ORDER = ["gLLCC4818", "gLLCC47C4", "gLLCCWidth", "gLLCCHeight", "gLLCC47CC", "gLLCC4814", "gLLCC4810", "gLLCC480C",
              "gLLCC4808", "gLLCC4804", "gLLCC4800", "gLLCCDE0E2", "gLLCCC214", "gLLCCC210", "gLLCCC208", "gLLCCC204",
              "gLLCCC200", "gLLCC47EC", "gLLCC47E8", "gLLCC47E4", "gLLCC47E0", "gLLCCDB6C", "gLLCCDB68", "gLLCC47C8"]
HASH_MASK = 0x7FFFFFFFFFFFFFFF

# Globals the case builders set (not prefilled with random words): the others are all random before every call.
BUILT = {"gCoasterTypes", "gCoasterTypeCount"}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def fold(h, v):
    return ((h ^ (v & 0xFFFFFFFF)) * 0x100000001B3) & HASH_MASK


def cell_hash(e):
    h = 0xCBF29CE484222325 & HASH_MASK
    for name in CELL_ORDER:
        addr, _, width, _ = STATE[name]
        h = fold(h, int.from_bytes(e.read(addr, width), "big"))
    return h


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ state on both sides


def address(e, name):
    addr = STATE[name][0]
    return e.data_import(name) if addr is None else addr


def ppc_get(e, names, lab):
    out = {}
    for name in names:
        _, n, width, kind = STATE[name]
        a = address(e, name)
        values = []
        for i in range(n):
            word = int.from_bytes(e.read(a + i * width, width), "big")
            values.append(lab(word) if kind == "p" else word)
        out[name] = values
    return out


def ppc_set(e, pre, tokens):
    for name, values in pre.items():
        _, n, width, kind = STATE[name]
        a = address(e, name)
        for i, v in enumerate(values):
            if isinstance(v, str):
                v = tokens[v]
            e.write(a + i * width, (v & ((1 << (8 * width)) - 1)).to_bytes(width, "big"))


def host_addr(lib, name):
    fn = lib.tpw_ls_addr
    fn.restype, fn.argtypes = C.c_void_p, [C.c_char_p]
    return fn(name.encode())


def host_set(lib, pre, tokens):
    for name, values in pre.items():
        _, n, width, kind = STATE[name]
        a = host_addr(lib, name)
        hw = 8 if kind == "p" else width
        for i, v in enumerate(values):
            if isinstance(v, str):
                v = tokens[v]
            C.memmove(a + i * hw, (v & ((1 << (8 * hw)) - 1)).to_bytes(hw, "little"), hw)


def host_get(lib, names):
    lab = lib.tpw_cl_lab
    lab.restype, lab.argtypes = C.c_int32, [C.c_void_p]
    out = {}
    for name in names:
        _, n, width, kind = STATE[name]
        a = host_addr(lib, name)
        hw = 8 if kind == "p" else width
        values = []
        for i in range(n):
            word = int.from_bytes(C.string_at(a + i * hw, hw), "little")
            values.append(lab(word or None) if kind == "p" else word)
        out[name] = values
    return out


def prefill(r, **fixed):
    """Random words (or a sentinel pointer) for every global the case does not build or fix."""
    pre = {}
    for name, (_, n, width, kind) in STATE.items():
        if name in fixed:
            pre[name] = list(fixed[name])
        elif name in BUILT:
            continue
        elif kind == "p":
            pre[name] = [SENTINEL + 0x10 * i for i in range(n)]
        elif kind == "f":
            pre[name] = [r.getrandbits(32) for _ in range(n)]
        else:
            pre[name] = [r.getrandbits(8 * width) for _ in range(n)]
    for name, values in fixed.items():
        pre[name] = list(values)
    return pre


# ------------------------------------------------------------------ the case runner


class Ppc:
    """The PowerPC side of one case: the fakes, the objects and the labels."""

    def __init__(self, e, c, program=MAIN):
        self.e = e
        e.reset_scratch()
        self.c = c
        self.L = L = CL.PpcLog({}, c.get("q"))
        e.cl = L
        self.blocks = [e.alloc(64, b"\xa5" * 64) for _ in range(4)]
        for i, b in enumerate(self.blocks):
            L.labels[b] = BLOCK_LABEL + i
        self.script = e.alloc(416, b"\xa5" * 416)
        L.labels[self.script] = SCRIPT_LABEL
        self.tokens = {"blk%d" % i: b for i, b in enumerate(self.blocks)}
        self.next_block = 0
        self.theme = e.alloc(1200)
        e.write(self.theme, c.get("theme", "").encode("latin-1") + b"\0")
        self.found = e.alloc(1200)
        found = c.get("found")
        if found is not None:
            e.write(self.found, found.encode("latin-1") + b"\0")
        else:
            self.found = 0
        self.last_ok = None

    # ---- the fakes
    def install(self):
        e, L = self.e, self.L
        st = CL.Stubs()
        st.real = set(OWN)
        self.st = st

        def alloc(e):
            L.rec("MReleaseAllocF", [e.r(3), e.r(4)])
            e.set_r(3, self.blocks[self.next_block & 3])
            self.next_block += 1

        def free(e):
            L.rec("MReleaseFreeF", [L.lab(e.r(3))])
            e.set_r(3, 0)

        def dm_free_mesh(e):
            L.rec("DMFreeMesh", [L.lab(e.r(3))])
            e.set_r(3, T.u32(L.ret("DMFreeMesh", 1)))

        def dm_load(e):
            L.rec("DMLoadTextures", [L.lab(e.r(3))])
            e.set_r(3, T.u32(L.ret("DMLoadTextures", 1)))

        def object_table(e):
            r3, r4, r5 = e.r(3), e.r(4), e.r(5)
            L.rec("SetupObjectInstanceTable", [L.lab(r3), L.lab(r4), L.lab(r5), e.u32(r3), e.u32(r4), e.u32(r5)])

        def find_file(e):
            L.rec("FindFilePath", [e.r(4), int(e.r(5) != 0)], [e.cstr(e.r(3))[:159]])
            e.set_r(3, self.found)

        def load_attrib(e):
            L.rec("LoadAttribMap", [L.lab(e.r(3))], [e.cstr(e.r(4))[:159] if e.r(4) else "(null)"])
            e.set_r(3, T.u32(L.ret("LoadAttribMap", 0)))

        def cell_args(name, count):
            def stub(e):
                L.rec(name, [T.s32(e.r(3 + i)) for i in range(count)] + [cell_hash(e)])
            return stub

        def rand(e):
            value = L.ret("rand", 4242)
            L.rec("rand", [value])
            e.set_r(3, T.u32(value))

        def op_new(e):
            fail = L.ret("newnull", 0) != 0
            L.rec("new", [e.r(3), int(fail)])
            e.set_r(3, 0 if fail else e.alloc(e.r(3), b"\xa5" * e.r(3)))

        def ctor(e):
            this, ok = e.r(3), e.r(6)
            L.rec("CDynamicMesh", [L.newlab(this), e.r(4) & 0xFFFF, e.r(5) & 0xFFFF, int(ok == self.last_ok), T.s32(e.r(8))] +
                  [CL.fbits(e.f(i)) for i in range(1, 7)], [e.cstr(e.r(7))[:159]])
            self.last_ok = ok
            w32(e, ok, L.ret("ctorOk", 1))

        def dtor(e):
            this = e.r(3)
            if this != 0:
                L.rec("CDynamicMesh::~CDynamicMesh", [L.lab(this)])
                if T.s16(e.r(4)) > 0 and L.lab(this) > 0:
                    L.rec("delete", [L.lab(this)])
                    L.forget(this)

        def unlink(e):
            L.rec("Unlink", [L.lab(e.r(3)), L.lab(e.r(4)), T.s32(e.r(5))])
            e.set_r(3, 1)

        def message_box(e):
            port, text, caption, flags = e.r(3), e.cstr(e.r(4)), e.cstr(e.r(5)), e.r(6)
            if port == 0 and flags == 0x31:
                L.rec("UDebug", [], [("%s: %s" % (caption, text))[:159]])
            else:
                L.rec("MessageBoxA(unexpected)", [port, flags], [text[:159], caption[:159]])
            e.set_r(3, T.u32(L.ret("answer", 1)))

        st.internal(0x10064BD4, alloc)
        st.internal(0x10064C54, free)
        st.internal(0x10030AE8, dm_free_mesh)
        st.internal(0x1003013C, dm_load)
        st.internal(0x10030178, CL.recorder("DMFreeTextures", "L"))
        st.imp("LbMem_Alloc__FUi", lambda e: (L.rec("LbMem_Alloc", [e.r(3)]), e.set_r(3, self.script))[1])
        st.imp("rand", rand)
        st.internal(0x1004E60C, CL.recorder("CreateExtrudeFaceStorage", "uu"))
        st.internal(0x101EF1B0, object_table)
        st.internal(0x1015139C, lambda e: (L.rec("get_theme_path"), e.set_r(3, self.theme))[1])
        st.internal(0x100597F8, find_file)
        st.internal(0x1013E5AC, load_attrib)
        for name, address_, count in (("LLCC_ResetMap", 0x100916A8, 2), ("LLCM_SetIllegalPlacement", 0x10088934, 1),
                                      ("LLCM_LoadQMeshes", 0x10081A98, 0), ("LLCM_LoadKTrackMeshes", 0x10081DD0, 0),
                                      ("LLCM_LoadWTrackMeshes", 0x10082174, 0), ("LLCM_LoadHoardingMeshes", 0x1008255C, 0),
                                      ("LLCC_SystemResetClickStack", 0x10091DD4, 0), ("LLCC_ResetBuyLandList", 0x10096190, 0),
                                      ("LLCC_DrawInitialHoarding", 0x10095420, 0)):
            st.internal(address_, cell_args(name, count))
        for name, address_ in (("LLCM_DestroyQMeshes", 0x10081BC8), ("LLCM_DestroyKTrackMeshes", 0x10081F18),
                               ("LLCM_DestroyWTrackMeshes", 0x100822BC), ("LLCM_DestroyHoardingMeshes", 0x10082688)):
            st.internal(address_, CL.recorder(name))
        st.internal(0x100D8F0C, CL.recorder("SCR_InitCheckTable"))
        st.imp("__nw__FUl", op_new)
        st.internal(0x100E1610, ctor)
        st.internal(0x100E191C, dtor)
        st.imp("Unlink__7CMapWhoFP12CMapLinkBasei", unlink)
        st.imp("MessageBoxA__10NS_MacDozeFP8GrafPortPCcPCcUi", message_box)
        # the labelled objects
        self.map = e.alloc(16, b"\xa5" * 16)
        w32(e, e.data_import("gei_MapWho"), self.map)
        L.labels[self.map] = MAP_LABEL
        L.labels[0x1029CF58] = HOARD_LABEL
        L.labels[0x102A02FC] = CELL_LABEL
        self.world = e.alloc(16, b"\xa5" * 16)
        w32(e, 0x1037EBFC, self.world)
        L.labels[self.world + 0x2D8] = WORLD_MAP_LABEL
        for label, a in zip(TABLE_LABELS, (0x102B730C, 0x102B7308, 0x102B7304)):
            L.labels[a] = label
        self.saved = st.activate(e)

    def finish(self):
        CL.Stubs.deactivate(self.e, self.saved)


# ------------------------------------------------------------------ the objects the functions free or delete


def light_vtable(e):
    """A vtable (in persistent memory) whose third slot (+8) is the deleting destructor of a light."""
    if hasattr(e, "ls_light_vtable"):
        return e.ls_light_vtable

    def destructor(emu):
        L, this, flag = emu.cl, emu.r(3), T.s16(emu.r(4))
        if flag > 0 and L.lab(this) > 0:
            L.rec("delete", (L.lab(this),))
            L.forget(this)

    vtable = T.persist_alloc(e, 0x20)
    e.write(vtable + 8, struct.pack(">I", T.fake_function(e, destructor)))
    e.ls_light_vtable = vtable
    return vtable


def ppc_build_lights(p, c):
    """The record gGlobalLight points at (type at +0, the directional light's holder at +4, the point light's at +0xc) and the
    holders: a holder is 0x74 bytes, its first word points at the light at +0x10 whose first word is the link base at +8."""
    e, L = p.e, p.L
    record = e.alloc(0x14, b"\0" * 0x14)
    w32(e, record, c["type"])
    vt = light_vtable(e)
    for slot, (present, obj_label, link_label) in ((4, (c["dir"], 31, 41)), (0xC, (c["point"], 32, 42))):
        if not present:
            continue
        holder = e.alloc(0x74, b"\xa5" * 0x74)
        w32(e, holder, holder + 0x10)
        w32(e, holder + 4, vt)
        w32(e, holder + 0x10, holder + 8)
        w32(e, record + slot, holder)
        L.labels[holder] = obj_label
        L.labels[holder + 8] = obj_label     # the host's link base is the object's own address (one label for both)
    w32(e, 0x102C6C84, record)
    return record


def host_build_lights(lib, c):
    lib.tpw_ls_make_lights.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ls_make_lights(c["type"], c["dir"], c["point"])


def ppc_build_meshes(p, c):
    e, L = p.e, p.L
    for bit, (slot, label) in enumerate(zip((0x1034C188, 0x1034C184, 0x1034C180), MESH_LABELS)):
        if (c["mask"] >> bit) & 1:
            mesh = e.alloc(0xA4, b"\xa5" * 0xA4)
            L.labels[mesh] = label
            w32(e, slot, mesh)
        else:
            w32(e, slot, 0)


def host_build_meshes(lib, c):
    labels = (C.c_int32 * 3)(*MESH_LABELS)
    lib.tpw_ls_make_meshes.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_ls_make_meshes(c["mask"], labels)


def coaster_label_count(spec):
    return sum(17 + t[3] + t[5] for t in spec)


def ppc_build_coasters(p, c):
    """Coaster types in Mac layout; the labels are handed out in the order the host builder creates the objects."""
    e, L = p.e, p.L
    spec = c["coasters"]
    labels = iter(range(100, 100 + coaster_label_count(spec) + 8))
    built = []

    def make(addr):
        label = next(labels)
        if addr != 0:
            L.labels[addr] = label
        return addr

    def blk(mask, bit):
        return 0 if (mask >> bit) & 1 else e.alloc(16, b"\xa5" * 16)

    for t, s in enumerate(spec):
        a, b, cc, count, index, nrec, mask = s
        rec = make(e.alloc(0x320, b"\xa5" * 0x320))
        sets = [make(e.alloc(0x30, b"\xa5" * 0x30)) for _ in range(3)]
        set_blocks = []
        for k in range(3):
            blk_k = make(blk(mask, k))
            set_blocks.append(blk_k)
            w32(e, sets[k] + 0x2C, blk_k)
        w32(e, rec + 0x2FC, sets[a])
        w32(e, rec + 0x300, sets[b])
        w32(e, rec + 0x304, sets[cc])
        w32(e, rec + 0x308, make(blk(mask, 3)))
        textures = make(e.alloc(0x20, b"\xa5" * 0x20))
        e.write(textures + 4, struct.pack(">H", count))
        entries = make(e.alloc(0x10 * (count + 1), b"\xa5" * (0x10 * (count + 1))))
        w32(e, textures + 0x10, entries)
        entry_blocks = []
        for i in range(count):
            eb = make(e.alloc(16, b"\xa5" * 16))
            entry_blocks.append(eb)
            w32(e, entries + 0x10 * i + 4, eb)
        w32(e, rec + 0x1C, textures)
        w32(e, rec + 0x18, make(e.alloc(16, b"\xa5" * 16)))
        w32(e, rec + 0x14, make(blk(mask, 4)))
        records = make(e.alloc(0x18 * max(nrec, 1), b"\xa5" * (0x18 * max(nrec, 1))))
        rec_blocks = []
        for r in range(nrec):
            rb = make(blk(mask, 9) if r == index else e.alloc(16, b"\xa5" * 16))
            rec_blocks.append(rb)
            w32(e, records + 0x18 * r + 0xC, rb)
        w32(e, rec + 0x88, records)
        w32(e, rec + 0x8C, index)
        w32(e, rec + 0x90, make(blk(mask, 6)))
        w32(e, rec + 0x94, make(blk(mask, 7)))
        w32(e, rec + 0x98, make(blk(mask, 8)))
        w32(e, rec + 0x78, make(blk(mask, 5)))
        w32(e, 0x102B68BC + 4 * t, rec)
        built.append((rec, sets, textures, entries, count, records, nrec))
    w32(e, 0x102B68B8, len(spec))
    p.built = built


def ppc_dump_coasters(p):
    e, L = p.e, p.L
    out = []
    for rec, sets, textures, entries, count, records, nrec in p.built:
        for off in (0x14, 0x18, 0x1C, 0x78, 0x88, 0x90, 0x94, 0x98, 0x2FC, 0x300, 0x304, 0x308):
            out.append(L.lab(e.u32(rec + off)))
        for s in sets:
            out.append(L.lab(e.u32(s + 0x2C)))
        out.append(L.lab(e.u32(textures + 0x10)))
        for i in range(count):
            out.append(L.lab(e.u32(entries + 0x10 * i + 4)))
        for r in range(nrec):
            out.append(L.lab(e.u32(records + 0x18 * r + 0xC)))
    return out


def host_build_coasters(lib, c):
    spec = c["coasters"]
    flat = [v for s in spec for v in s]
    labels = list(range(100, 100 + coaster_label_count(spec) + 8))
    lib.tpw_ls_make_coasters.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    lib.tpw_ls_make_coasters(len(spec), (C.c_int32 * len(flat))(*flat), (C.c_int32 * len(labels))(*labels))


def host_dump_coasters(lib):
    out = (C.c_int64 * 1024)()
    lib.tpw_ls_dump_coasters.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ls_dump_coasters.restype = C.c_int32
    n = lib.tpw_ls_dump_coasters(out)
    return list(out[:n])


# ------------------------------------------------------------------ one case on each side


def ppc_case(e, c):
    program = c.get("program", MAIN)
    p = Ppc(e, c)
    op = c["op"]
    p.install()
    out = {}
    try:
        pre = c["pre"]
        ppc_set(e, pre, p.tokens)
        if op == 5:
            ppc_build_lights(p, c)
        if op == 15:
            ppc_build_meshes(p, c)
        if op == 9:
            ppc_build_coasters(p, c)
        a, b, cc, d = c.get("args", (0, 0, 0, 0))
        text = e.alloc(1200)
        e.write(text, c.get("text", "").encode("latin-1") + b"\0")
        args = {10: [T.u32(a), T.u32(b)], 12: [text]}.get(op, [])
        r3 = T.call_ppc(e, OPS[op], args)[0]
        out["result"] = r3 if op in (0, 12) else 0
        names = [n for n in STATE if n not in BUILT or op == 9]
        out["state"] = ppc_get(e, names, p.L.lab)
        if op == 13:
            out["script"] = list(e.read(p.script, 416))
        if op == 9:
            out["coasters"] = ppc_dump_coasters(p)
        if op == 5:
            record = e.u32(0x102C6C84)
            out["lights"] = [p.L.lab(e.u32(record + 4)), p.L.lab(e.u32(record + 0xC))]
    finally:
        p.finish()
    out["log"] = p.L.log
    return out


def host_case(lib, c):
    op = c["op"]
    CL.host_begin(lib, {}, c.get("q"))
    lib.tpw_ls_begin.argtypes = [C.c_char_p, C.c_char_p]
    lib.tpw_ls_begin(c.get("theme", "").encode("latin-1"), None if c.get("found") is None else c["found"].encode("latin-1"))
    # labels of the fixed objects
    label = lib.tpw_cl_label
    label.argtypes = [C.c_int32, C.c_void_p]
    blocks = host_addr(lib, "gBlocks")
    for i in range(4):
        label(BLOCK_LABEL + i, blocks + 64 * i)
    label(SCRIPT_LABEL, host_addr(lib, "gScriptBlock"))
    label(MAP_LABEL, host_addr(lib, "gMapStorage"))
    label(HOARD_LABEL, host_addr(lib, "gRideHoardingTextures"))
    label(CELL_LABEL, host_addr(lib, "gDynamicCellTextures"))
    label(WORLD_MAP_LABEL, host_addr(lib, "gWorldStorage") + 0x2D8)
    for lab_, name in zip(TABLE_LABELS, ("gObjectTableCount", "gObjectTableFirstFree", "gObjectTableLowest")):
        label(lab_, host_addr(lib, name))
    tokens = {"blk%d" % i: blocks + 64 * i for i in range(4)}
    out = {}
    try:
        host_set(lib, c["pre"], tokens)
        if op == 5:
            host_build_lights(lib, c)
        if op == 15:
            host_build_meshes(lib, c)
        if op == 9:
            host_build_coasters(lib, c)
        a, b, cc, d = c.get("args", (0, 0, 0, 0))
        lib.tpw_ls_call.restype = C.c_int64
        lib.tpw_ls_call.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_char_p]
        r = lib.tpw_ls_call(op, T.s32(a), T.s32(b), T.s32(cc), T.s32(d), c.get("text", "").encode("latin-1"))
        out["result"] = r if op in (0, 12) else 0
        names = [n for n in STATE if n not in BUILT or op == 9]
        out["state"] = host_get(lib, names)
        if op == 13:
            buf = (C.c_uint8 * 416)()
            lib.tpw_ls_dump_script.argtypes = [C.POINTER(C.c_uint8)]
            lib.tpw_ls_dump_script(buf)
            out["script"] = list(buf)
        if op == 9:
            out["coasters"] = host_dump_coasters(lib)
        if op == 5:
            dump = (C.c_int64 * 2)()
            lib.tpw_ls_dump_lights.argtypes = [C.POINTER(C.c_int64)]
            lib.tpw_ls_dump_lights(dump)
            out["lights"] = list(dump)
    finally:
        log = CL.host_end(lib)
        if op == 5:
            deleted = (C.c_int32 * 2)(*[int(any(ev == ("delete", (lab_,), ()) for ev in log)) for lab_ in (31, 32)])
            lib.tpw_ls_free_lights.argtypes = [C.POINTER(C.c_int32)]
            lib.tpw_ls_free_lights(deleted)
        if op == 9:
            lib.tpw_ls_free_coasters()
    out["log"] = log
    return out


def ppc_engine(e, c):
    """PT_SetRange in the engine's program."""
    e.reset_scratch()
    L = CL.PpcLog({}, None)
    e.cl = L
    st = CL.Stubs()
    st.real = {PT_SET_RANGE}
    st.internal(0x1000DB80, lambda emu: L.rec("ProcTextureRange", [L.lab(emu.r(3))] + [T.s32(emu.r(i)) for i in range(4, 8)]))
    saved = st.activate(e)
    try:
        hfield = e.alloc(16, b"\xa5" * 16) if c["present"] else 0
        L.labels[hfield] = HFIELD_LABEL
        w32(e, HFIELD_SLOT, hfield)
        T.call_ppc(e, PT_SET_RANGE, [T.u32(v) for v in c["args"]])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"log": L.log}


def host_engine(lib, c):
    hfield = host_addr(lib, "gHFieldStorage")
    CL.host_begin(lib, {HFIELD_LABEL: hfield}, None)
    slot = C.c_uint64.from_address(host_addr(lib, "gei_HField"))
    slot.value = hfield if c["present"] else 0
    lib.tpw_ls_call.restype = C.c_int64
    lib.tpw_ls_call.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_char_p]
    a = [T.s32(v) for v in c["args"]]
    try:
        lib.tpw_ls_call(16, *a, b"")
    finally:
        log = CL.host_end(lib)
    return {"log": log}


class EngineTest(FnTest):
    """A test of a function of the engine's program: the runner is switched to its emulator for the case and back."""

    def run_case(self, runner, case):
        main = runner.program
        runner.use(ENGINE)
        try:
            return FnTest.run_case(self, runner, case)
        finally:
            runner.use(main)


# ------------------------------------------------------------------ cases

EDGE_WORDS = [0, 1, 2, 3, 4, 5, 0xB, 0xC, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x15555555, 0x2AAAAAAB,
              0x7FFFFFFF, 0x80000000, 0x80000001, 0xAAAAAAAB, 0xFFFFFFFE, 0xFFFFFFFF]
PRINTABLE = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :._-%"


def rnd_word(r):
    return r.choice(EDGE_WORDS) if r.random() < 0.5 else r.getrandbits(32)


def rnd_text(r, limit=60):
    return "".join(r.choice(PRINTABLE) for _ in range(r.randrange(0, limit)))


def case(op, r, args=(0, 0, 0, 0), fixed=None, **extra):
    c = {"op": op, "args": tuple(args), "pre": prefill(r, **(fixed or {})), "q": {}}
    c.update(extra)
    return c


def make_cases(op, edge_fn, rnd_fn, seed):
    def edges():
        r = random.Random(seed)
        return list(edge_fn(r))

    def rnd(r):
        return rnd_fn(r)
    return edges, rnd


def test_for(name, op, edge_fn, rnd_fn, seed, ppc=ppc_case, host=host_case, cls=FnTest):
    edge, rnd = make_cases(op, edge_fn, rnd_fn, seed)
    return cls(name, ppc, host, edge=edge, rnd=rnd)


# --- LoadLevelAttributeMap
THEMES = ["", "data:levels:Jungle", "data:levels:Halloween:x", "a", "%s", "100%", "with space:and:colons", "x" * 100,
          "y" * 1014, "z" * 1013, "q" * 500]
ATTRIB_ANSWERS = [0, 1, -1, 7, -2147483648, 2147483647, 256, -256]


def attrib_case(r, theme=None, found="same", answer=None):
    theme = theme if theme is not None else (r.choice(THEMES) if r.random() < 0.5 else rnd_text(r, 200))
    if found == "same":
        found = None if r.random() < 0.2 else (rnd_text(r, 80) if r.random() < 0.5 else theme + ":Terrain:Base.map")
    answer = answer if answer is not None else r.choice(ATTRIB_ANSWERS)
    return case(0, r, theme=theme, found=found, q={"LoadAttribMap": [answer]})


def attrib_edges(r):
    for theme in THEMES:
        for found in (None, theme + ":Terrain:Base.map", ""):
            for answer in (0, 1, -1):
                yield attrib_case(r, theme, found, answer)
    for answer in ATTRIB_ANSWERS:
        yield attrib_case(r, "data:levels:Jungle", "found", answer)


def attrib_random(r):
    return attrib_case(r)


# --- the poly search table
def poly_up_edges(r):
    for n in EDGE_WORDS:
        yield case(1, r, fixed={"gPolySearchCount": [n]})


def poly_up_random(r):
    return case(1, r, fixed={"gPolySearchCount": [rnd_word(r)]})


def poly_down_edges(r):
    for table in (0, "blk0", "blk1", SENTINEL, 0xA5A5A5A5):
        yield case(2, r, fixed={"gPolySearchTable": [table]})


def poly_down_random(r):
    return case(2, r, fixed={"gPolySearchTable": [r.choice([0, "blk0", "blk3", SENTINEL, r.getrandbits(32)])]})


# --- the plain globals
def plain_edges(op):
    def edges(r):
        for _ in range(4):
            yield case(op, r)
    return edges


def plain_random(op):
    def rnd(r):
        return case(op, r)
    return rnd


# --- DestroyLevelLights
LIGHT_TYPES = [-1, 0, 1, 2, 3, 4, 7, -2147483648, 2147483647, -100, 5, 100, 0x10000]


def lights_case(r, kind=None, directional=None, point=None):
    kind = kind if kind is not None else r.choice(LIGHT_TYPES + [r.getrandbits(32) - (1 << 31)])
    directional = directional if directional is not None else r.randint(0, 1)
    point = point if point is not None else r.randint(0, 1)
    return case(5, r, type=kind, dir=directional, point=point, q={})


def lights_edges(r):
    for kind in LIGHT_TYPES:
        for directional in (0, 1):
            for point in (0, 1):
                yield lights_case(r, kind, directional, point)


def lights_random(r):
    return lights_case(r)


# --- the highlight buffers
def hilite_up_edges(r):
    for a in (0, 1, 5, 0x15555555, 0x15555556, 0x80000000, 0xFFFFFFFF):
        for b in (0, 1, 7, 0x80000000, 0xFFFFFFFF):
            yield case(6, r, fixed={"gMaxMeshFaces4": [a], "gMaxMeshF3E": [b], "gMaxMeshNodes": [rnd_word(r)]})
    for n in EDGE_WORDS:
        yield case(6, r, fixed={"gMaxMeshNodes": [n]})


def hilite_up_random(r):
    return case(6, r, fixed={"gMaxMeshFaces4": [rnd_word(r)], "gMaxMeshF3E": [rnd_word(r)], "gMaxMeshNodes": [rnd_word(r)]})


def hilite_down_edges(r):
    for a in (0, "blk0", "blk1", SENTINEL):
        for b in (0, "blk2", "blk3", SENTINEL):
            yield case(7, r, fixed={"gHighlightFaceColours": [a], "gHighlightNodeColours": [b]})


def hilite_down_random(r):
    pick = lambda: r.choice([0, "blk0", "blk1", "blk2", "blk3", SENTINEL, r.getrandbits(32)])
    return case(7, r, fixed={"gHighlightFaceColours": [pick()], "gHighlightNodeColours": [pick()]})


# --- the coaster types
def coaster_spec(r, shape=None, base=None):
    sets = shape if shape is not None else tuple(r.randint(0, 2) for _ in range(3))
    count = r.choice([3, 3, 4, 5, 8, 6]) if base is None else r.randint(base, 8)
    nrec = r.randint(1, 4)
    mask = 0 if r.random() < 0.5 else r.getrandbits(10)
    return (sets[0], sets[1], sets[2], count, r.randrange(nrec), nrec, mask)


def coaster_case(r, n, shapes=None, base=3, masks=None):
    spec = []
    for t in range(n):
        shape = shapes[t % len(shapes)] if shapes else None
        s = coaster_spec(r, shape, base)
        if masks is not None:
            s = s[:6] + (masks[t % len(masks)],)
        spec.append(s)
    return case(9, r, coasters=spec, fixed={"gCoasterTextureBase": [base]}, q={"DMFreeMesh": [1]})


def coaster_edges(r):
    for n in (0, 1, 2, 3):
        yield coaster_case(r, n)
    for shape in ((0, 1, 2), (1, 1, 1), (0, 1, 0), (0, 1, 1), (2, 1, 0), (0, 0, 1), (1, 0, 0), (0, 0, 0), (2, 2, 2), (1, 0, 2)):
        yield coaster_case(r, 1, [shape])
        yield coaster_case(r, 2, [shape, (2, 0, 1)])
    for base in (0, 1, 2, 3, 4, 7):
        yield coaster_case(r, 2, base=base)
    for mask in (0, 0x3FF, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x7):
        yield coaster_case(r, 2, masks=[mask])
    # a texture count equal to the base (no entries freed) and one above it by one
    for count in (3, 4):
        c = coaster_case(r, 1)
        c["coasters"][0] = c["coasters"][0][:3] + (count,) + c["coasters"][0][4:]
        yield c


def coaster_random(r):
    return coaster_case(r, r.randint(0, 3), base=r.choice([0, 1, 2, 3, 3, 3]))


# --- the cell system
def cell_edges(r):
    for w in (0, 1, 0x80, 0x100, -1, 0x7FFFFFFF, -0x80000000, 64, 1000):
        for h in (0, 0x80, -5, 0x7FFFFFFF, 3):
            yield case(10, r, args=(w, h, 0, 0))


def cell_random(r):
    return case(10, r, args=(T.s32(rnd_word(r)), T.s32(rnd_word(r)), 0, 0))


# --- DataErrorHandler
def error_text(r):
    return r.choice(["", "Invalid lightsource creation attempted", "x", "Data error: mesh 5", "%s %d 100%", "a" * 100]) \
        if r.random() < 0.5 else rnd_text(r, 120)


def error_case(r, text=None, answer=None):
    return case(12, r, text=text if text is not None else error_text(r),
                q={"answer": [answer if answer is not None else r.choice([1, 0, 3, 4, -1, 0x7FFFFFFF, 1, 1])]})


def error_edges(r):
    for answer in (0, 1, 3, 4, -1, 100):
        yield error_case(r, "Invalid lightsource creation attempted", answer)
    yield error_case(r, "", 1)
    yield error_case(r, "x" * 200, 1)


def error_random(r):
    return error_case(r)


# --- the script system
def script_edges(r):
    for guard in (0, 1, 2, 0xFFFFFFFF, 0x80000000, 0x100):
        yield case(13, r, fixed={"gScriptInitialised": [guard]})


def script_random(r):
    return case(13, r, fixed={"gScriptInitialised": [r.choice([0, 0, 1, rnd_word(r)])]})


# --- the dynamic cell meshes
def dc_setup_case(r, load=None, ok=None, newnull=None, rand=None):
    load = load if load is not None else r.choice([0, 1, 5, -1, 0x7FFFFFFF])
    ok = ok if ok is not None else [r.choice([0, 1, 1, 7, 0x80000000]) for _ in range(r.randint(1, 3))]
    newnull = newnull if newnull is not None else [0] + [r.choice([0, 0, 1]) for _ in range(2)]
    rand = rand if rand is not None else [r.choice([0, 1, 2, 3, 4, 5, 0x7FFF, 0x7FFE, 12345, -1, -2, -3, -4, -0x80000000, 0x7FFFFFFF])
                                          if r.random() < 0.6 else r.getrandbits(15) for _ in range(64)]
    return case(14, r, q={"DMLoadTextures": [load], "ctorOk": list(ok), "newnull": list(newnull), "rand": list(rand)})


def dc_setup_edges(r):
    for load in (0, 1, 9):
        yield dc_setup_case(r, load)
    for ok in ([1], [0], [1, 0], [1, 1, 0], [0, 0], [0, 1, 0], [1, 0, 0], [0, 0, 0], [0, 1, 1], [5, 0, 1], [1, 1, 1]):
        yield dc_setup_case(r, 1, ok, [0, 0, 0])
    for newnull in ([0, 1, 0], [0, 0, 1], [0, 1, 1]):
        for ok in ([1], [0], [1, 0], [0, 1], [1, 0, 1], [0, 1, 0]):
            yield dc_setup_case(r, 1, ok, newnull)


def dc_setup_random(r):
    return dc_setup_case(r)


def dc_free_case(r, mask=None, ready=None):
    mask = mask if mask is not None else r.randint(0, 7)
    ready = ready if ready is not None else r.choice([0, 1, 2, 0x80000000, r.getrandbits(32)])
    return case(15, r, fixed={"gDynamicCellsReady": [ready]}, mask=mask)


def dc_free_edges(r):
    for ready in (0, 1, 2, 0xFFFFFFFF):
        for mask in range(8):
            yield dc_free_case(r, mask, ready)


def dc_free_random(r):
    return dc_free_case(r)


# --- PT_SetRange
def range_case(r, present=None):
    args = tuple(T.u32(rnd_word(r)) for _ in range(4))
    return {"present": present if present is not None else r.random() < 0.7, "args": args}


def range_edges():
    r = random.Random(77)
    for present in (False, True):
        for _ in range(4):
            yield range_case(r, present)
    yield {"present": True, "args": (0, 0, 0, 0)}
    yield {"present": True, "args": (1, 2, 3, 4)}
    yield {"present": True, "args": (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 1)}


def range_random(r):
    return range_case(r)


TESTS = [
    test_for("LoadLevelAttributeMap", 0, attrib_edges, attrib_random, 101),
    test_for("SetupForPolySearch", 1, poly_up_edges, poly_up_random, 102),
    test_for("SetdownFromPolySearch", 2, poly_down_edges, poly_down_random, 103),
    test_for("SetupHeightmapSystem", 3, plain_edges(3), plain_random(3), 104),
    test_for("ClosedownRideHoardings", 4, plain_edges(4), plain_random(4), 105),
    test_for("DestroyLevelLights", 5, lights_edges, lights_random, 106),
    test_for("CreateHighlightColourBuffers", 6, hilite_up_edges, hilite_up_random, 107),
    test_for("DestroyHighlightColourBuffers", 7, hilite_down_edges, hilite_down_random, 108),
    test_for("InitialiseCoasterTypes", 8, plain_edges(8), plain_random(8), 109),
    test_for("DestroyCoasterTypes", 9, coaster_edges, coaster_random, 110),
    test_for("LLCC_SystemInit", 10, cell_edges, cell_random, 111),
    test_for("LLCC_SystemDeInit", 11, plain_edges(11), plain_random(11), 112),
    test_for("DataErrorHandler", 12, error_edges, error_random, 113),
    test_for("SCR_InitScriptSystem", 13, script_edges, script_random, 114),
    test_for("DC_SetupDynamicCellMeshes", 14, dc_setup_edges, dc_setup_random, 115),
    test_for("DC_FreeDynamicCellMeshes", 15, dc_free_edges, dc_free_random, 116),
    EngineTest("PT_SetRange", ppc_engine, host_engine, edge=range_edges, rnd=range_random),
]
