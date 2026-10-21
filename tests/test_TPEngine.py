"""Differential tests for the engine start-up glue (src/game/TPEngine.cpp, src/game/TPInstance.cpp, src/game/TPMesh.cpp, src/game/park/TPHField.cpp, src/game/rides/TPRide.cpp, Milestone 1 unit 3),
against libtpw_unit_TPEngine.dylib (tests/unit_TPEngine.cpp's fakes; see calllog.py).

Every translated function is called directly. A case sets the same globals, engine
fields, option values, ride sets and return queues on both sides, runs the function, and
compares the call logs, the result and the state the unit writes: the option words, the
engine's texture, display and system flags, the options table, the view matrix, the world
box, the hooks installed, the ride flags and the unit's own globals (pointers by label).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_TPEngine"

F_ = dict(
    MacLoadDebugSettings=0x10059C70, MacIsFirstPerson=0x10059E40, MacReadClock=0x10059E78,
    MacUpdateMouse=0x10059EC4, MacOpen=0x10059EF0, MacRead=0x10059F38, MacClose=0x10059F84, MacSeek=0x10059FC8,
    MacSize=0x1005A010, MacMalloc=0x1005A054, MacFree=0x1005A0A0, EngineStartup_SetOptions=0x1005A0FC,
    EngineStartup_OptionsFile=0x1005A6A8, EngineStartup_Dialog=0x1005A7E0, EngineStartup_SettingsFile=0x1005A9B4,
    EngineStartup_Final=0x1005AB18, FindFilePath=0x1005AD58, EngineStartup=0x1005ADBC, AnimateRides=0x1005B36C,
    EngineTick1=0x1005B4D4, EngineTick2=0x1005B540, EngineTick3=0x1005B590, EngineCloseDown=0x1005B5D8,
    ResetWorldBoundingArea=0x1005B634, TPForce3DRefresh=0x1005B68C, ResetCursorOverSprites=0x1005B6C8,
    HNewHeap=0x1005B734, HFreeHeap=0x1005B774, InitialiseInstancing=0x10061AEC,
    EnableProceduralTexturing=0x100635CC, DisableProceduralTexturing=0x1006367C,
    IsProceduralTexturingActive=0x10063700, MReleaseAllocF=0x10064BD4, MReleaseFreeF=0x10064C54,
    InitialiseMemorySystem=0x10064CB0, UpdateScratchGeomBufferSize=0x100682FC, CreateScratchGeomBuffer=0x10068D14,
    DestroyScratchGeomBuffer=0x10068D90)
A = dict(
    gGameOptions=0x102B2B48, gTPDialogOptions=0x103BC664, gTPDialogTextureMegabytes=0x103BC65C,
    gMainWindow=0x102B2BB4, gScreenWidth=0x102B2B54, gScreenHeight=0x102B2B50, gNoSavedSettings=0x102B2B44,
    gFirstPersonFlags=0x102B5A40, gFirstPersonRideSet=0x102B5A28, gHeightfieldRideSet=0x102C6C74,
    gRideAnimSets=0x102C79A4, gRideCountA=0x102D19A8, gRideCountB=0x102D19AC, gMaxMeshFaces4=0x102D1F14,
    gMaxMeshF3E=0x102D1F18, gMaxMeshNodes=0x102D1F1C, gClock=0x1037F048, gGame=0x1037F134, gTPFileInfo=0x102B3458,
    gTPGlobals=0x103B74C8, gExitError=0x1037F1C0, gPlayerSettings=0x1038071C, gOptionsBalance=0x102B4DA8,
    gSmallTexturesOnly=0x102C1DE4, gOpenGL=0x102C1DE8, gIdxDirectDraw=0x102C1DEC, gCursorOverSprite=0x102C1DF0,
    gCursorSpritePosition=0x102C1DF4, g3DRefreshFlags=0x102C1E00, gMemorySystemInitialised=0x102C6C9C,
    gScratchGeomVertices=0x102C798C, gScratchGeomBuffer=0x102C7990)
SCALARS = [("gGameOptions", 4), ("gTPDialogOptions", 4), ("gTPDialogTextureMegabytes", 4), ("gScreenWidth", 4),
           ("gScreenHeight", 4), ("gNoSavedSettings", 1), ("gFirstPersonFlags", 4), ("gRideCountA", 4),
           ("gRideCountB", 4), ("gMaxMeshFaces4", 4), ("gMaxMeshF3E", 4), ("gMaxMeshNodes", 4),
           ("gSmallTexturesOnly", 1), ("gCursorOverSprite", 4), ("g3DRefreshFlags", 4),
           ("gMemorySystemInitialised", 4), ("gScratchGeomVertices", 4)]
POINTERS = ["gMainWindow", "gOpenGL", "gIdxDirectDraw", "gScratchGeomBuffer"]
# (base, offset, size) of the fields tpw_te_set_fields / get_fields handle, in order.
FIELDS = [("gPlayerSettings", 0, 4), ("gPlayerSettings", 8, 4), ("gPlayerSettings", 0xC, 4),
          ("gPlayerSettings", 0x90, 4), ("gei_TextureSettings", 0, 4), ("gei_TextureSettings", 0x10, 4),
          ("gei_TextureSettings", 0x1C, 4), ("gei_Display", 0, 4), ("gei_System", 0, 4), ("gei_System", 0x1C, 4),
          ("gExitError", 0, 4), ("context", 4, 1), ("context", 6, 1), ("context", 0x4C, 4), ("context", 0x1D8, 4)]
LABELS = dict(gClock=1, gGame=2, gTPFileInfo=3, gExitError=4, gPlayerSettings=5, gOptionsBalance=6, gei_Sky=7,
              gei_Display=8, gei_ProjectionMatrix=9, window=10, idx=11, context=12, tcache=13, mapwho=14, ddraw=15,
              view=16, particle=17)
# TVector -> hook id (unit_TPEngine.cpp tpw_te_hook_id)
HOOKS = {0x102660B0: 1, 0x102660A0: 2, 0x102660A8: 3, 0x10266098: 4, 0x10266090: 5, 0x10266078: 6, 0x102660C8: 7,
         0x10268A18: 8, 0x102660C0: 9, 0x102660B8: 10, 0x10266088: 11, 0x10266080: 12, 0x10266918: 13,
         0x10266910: 14, 0x102668E8: 15, 0x10266908: 16, 0x102668F0: 17, 0x10266900: 18, 0x102668F8: 19}
ENGINE_DATA = ("gei_TextureSettings", "gei_Display", "gei_System", "gei_Sky", "gei_ProjectionMatrix",
               "gs_WorldBoundingArea")
ENGINE_PTRS = ("gei_TCache", "gei_DDraw", "gei_ViewMatrix", "gei_MapWho", "gei_Particle")

VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def w8(e, a, v):
    e.write(a, bytes([v & 0xFF]))


def rd(e, a, size):
    return {1: e.read(a, 1)[0], 2: struct.unpack(">H", e.read(a, 2))[0], 4: e.u32(a)}[size]


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]

# ======================================================================= PowerPC side


class TeState:
    pass


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(F_.values())

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    def m(sym, name, spec="", ret=None, then=None):
        S.imp(sym, R(name, spec, ret, then))

    for sym, fn in CL.string_stubs().items():
        S.imp(sym, fn)
    gs = lambda e: e.te   # noqa: E731

    def balance_load(e):
        e.cl.rec("CBalanceTable::Load", (e.cl.lab(e.r(3)), T.s32(e.r(5))), (e.cstr(e.r(4))[:159],))
        e.set_r(3, T.u32(e.cl.ret("CBalanceTable::Load", 1)))

    def data_path(e, L):
        obj = e.alloc(0x14)
        CL.tds_set(e, obj, gs(e).data_path)
        return obj

    i(0x101472F4, "CClock::Read", "L", "i")
    i(0x10149BA8, "CGame::SetCurrentTheme(long)", "Li")
    i(0x10004EC4, "CSYS_TPFileInfo::GetDataPath", "L", ret=data_path)
    i(0x10004BC0, "FS_AddDirectorySeparator", "T",
      then=lambda e, L: CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + ":"))
    S.internal(0x10020134, balance_load)
    i(0x1014B62C, "CExitError::SetError", "Lius", then=lambda e, L: w32(e, e.r(3), 1))
    i(0x101664FC, "CPlayerSettings::SetRendererType", "Li")
    i(0x101665C0, "CPlayerSettings::SetVideoCard", "Li")
    i(0x10166628, "CPlayerSettings::SetGraphicsDetail", "Li")
    i(0x10003FD4, "TPFile_Open", "ss", "i")
    i(0x10004260, "TPFile_Read", "LuuL", "i")
    i(0x10004354, "TPFile_Close", "L", "i")
    i(0x100043EC, "TPFile_Seek", "Luu", "i")
    i(0x100044E8, "TPFile_Size", "L", "i")
    i(0x100597F8, "FindFilePath", "suL", ret=lambda e, L: CL.new_text(e, "Found:" + e.cstr(e.r(3))))
    i(0x100C74D4, "AnimateRideSet", "Liu")
    S.internal(0x10200460, lambda e: e.cl.rec("TickRideHeads", (e.cl.lab(e.r(3)), e.cl.lab(e.u32(e.r(3))))))
    i(0x100619D4, "HandleActiveHoardings", "Lf")
    i(0x100B7FA0, "CorrectOrientationFlagsForDiffProceduralMode", "i")
    m("PT_Enable__Fv", "PT_Enable")
    m("PT_Disable__Fv", "PT_Disable")

    def global_alloc(e):
        flags, size = e.r(3), e.r(4)
        e.cl.rec("GlobalAlloc", (flags, size))
        block = e.alloc(max(size, 4) if size < 0x100000 else 4)
        e.cl.newlab(block)
        e.set_r(3, block)

    S.imp("GlobalAlloc__10NS_MacDozeFUiUl", global_alloc)
    m("GlobalFree__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle", "GlobalFree", "L", "i")

    def set_rect(e):
        r = e.r(3)
        left, top, right, bottom = (T.s16(e.r(k)) for k in (4, 5, 6, 7))
        e.cl.rec("SetRect", (left, top, right, bottom))
        e.write(r, struct.pack(">hhhh", top, left, bottom, right))

    def pascal(e, a):
        if a == 0:
            return "(null)"
        n = e.read(a, 1)[0]
        return e.read(a + 1, n).decode("latin-1")

    def new_window(e):
        top, left, bottom, right = struct.unpack(">hhhh", e.read(e.r(4), 8))
        e.cl.rec("NewCWindow", (e.cl.lab(e.r(3)), top, left, bottom, right, e.r(6) & 0xFF, T.s16(e.r(7)),
                                T.s32(e.r(8)), e.r(9) & 0xFF, T.s32(e.r(10))), (pascal(e, e.r(5)),))
        e.set_r(3, gs(e).window if e.cl.ret("NewCWindow", 1) else 0)

    def make_spec(e):
        e.cl.rec("FSMakeFSSpec", (T.s16(e.r(3)), T.s32(e.r(4))), (pascal(e, e.r(5)),))
        e.set_r(3, T.u32(e.cl.ret("FSMakeFSSpec")))

    def get_named(e):
        e.cl.rec("GetNamedResource", (e.r(3),), (pascal(e, e.r(4)),))
        e.set_r(3, gs(e).handle if e.cl.ret("GetNamedResource", 1) else 0)

    S.imp("SetRect", set_rect)
    S.imp("NewCWindow", new_window)
    S.imp("FSMakeFSSpec", make_spec)
    def open_res(e):
        permission = e.r(4) & 0xFF
        e.cl.rec("FSpOpenResFile", (permission - 256 if permission & 0x80 else permission,))
        e.set_r(3, T.u32(e.cl.ret("FSpOpenResFile", 7)))

    S.imp("FSpOpenResFile", open_res)
    S.imp("CurResFile", lambda e: (e.cl.rec("CurResFile"), e.set_r(3, T.u32(e.cl.ret("CurResFile", 3)))))
    m("UseResFile", "UseResFile", "h")
    S.imp("GetNamedResource", get_named)
    m("CloseResFile", "CloseResFile", "h")
    m("__ct__Q213SamsUtilities7UStringFPCc", "UString", "xs")
    m("__dt__Q213SamsUtilities7UStringFv", "~UString")
    m("Projection__7CMatrixFfff", "CMatrix::Projection", "Lfff")
    m("Render__7CMapWhoFv", "CMapWho::Render", "L")
    m("RenderSystem_FlipScreen__Fv", "RenderSystem_FlipScreen", "", ret=lambda e, L: 1)
    m("EngineAdd_Close__Fv", "EngineAdd_Close")
    m("RenderSystem_Close__Fv", "RenderSystem_Close", "", ret=lambda e, L: 1)
    m("DDraw_Close__Fv", "DDraw_Close")
    m("SetActiveLayers__4CSkyFi", "CSky::SetActiveLayers", "Li")
    m("SetFogSpace__8CDisplayFff", "CDisplay::SetFogSpace", "Lff")

    def idx_make(e):
        e.cl.rec("IdxMakeOpenGLImplementation", (T.s32(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.r(5))))
        e.set_r(3, gs(e).idx if e.cl.ret("IdxMakeOpenGLImplementation", 1) else 0)

    S.imp("IdxMakeOpenGLImplementation__FQ213SamsUtilities10DeviceKindP8GrafPortPP7GDevice", idx_make)
    m("AddAttribute__Q213SamsUtilities7UOpenGLFll", "UOpenGL::AddAttribute", "Lii")

    def hooks(e, regs):
        return tuple(HOOKS.get(e.r(k), -1 if e.r(k) else 0) for k in regs)

    S.imp("SetFileCallbacks__FPFPCcPCc_PvPFPv_UlPFPvUlUlPv_UiPFPvUlUl_UlPFPv_lPFPCci_Pc",
          lambda e: e.cl.rec("SetFileCallbacks", hooks(e, range(3, 9))))
    S.imp("SetEngineCallbacks__FPFv_iPFv_bPFv_UlPFv_vPFUl_PvPFRPv_v",
          lambda e: e.cl.rec("SetEngineCallbacks", hooks(e, range(3, 9))))
    m("DDraw_Init__FP8GrafPortiiiii", "DDraw_Init", "Liiiii", ret=lambda e, L: L.ret("DDraw_Init", 1))
    m("RenderSystem_Init__Fi", "RenderSystem_Init", "i", ret=lambda e, L: L.ret("RenderSystem_Init", 1))
    m("EngineAdd_Init__FiiPc", "EngineAdd_Init", "iiL", ret=lambda e, L: L.ret("EngineAdd_Init", 1))
    return S


def make_vtable(emu, slots):
    vt = T.persist_alloc(emu, 0x60)
    for off, fn in slots.items():
        w32(emu, vt + off, T.fake_function(emu, fn))
    return vt


def ppc_begin(e, c):
    s = e.te = TeState()
    e.heap_in_scratch = True
    fixed = {}
    for name, label in LABELS.items():
        if name in A:
            fixed[A[name]] = label
        elif name in ENGINE_DATA:
            fixed[e.data_import(name)] = label
    s.window = e.alloc(0x20)
    fixed[s.window] = LABELS["window"]
    s.context = e.alloc(0x1E0)
    fixed[s.context] = LABELS["context"]
    s.idx = e.alloc(0x40)
    fixed[s.idx] = LABELS["idx"]
    if not hasattr(e, "te_idx_vt"):
        def get_context(em):
            em.cl.rec("IdxDirectDraw::DisplayGetContext", (em.cl.lab(em.r(3)),))
            em.set_r(3, em.te.context)
        e.te_idx_vt = make_vtable(e, {0x28: get_context})
    w32(e, s.idx, e.te_idx_vt)
    s.engine = {}
    for name, size, label in (("gei_TCache", 0x1910, "tcache"), ("gei_DDraw", 0x788, "ddraw"),
                              ("gei_ViewMatrix", 0x40, "view"), ("gei_MapWho", 0x40, "mapwho"),
                              ("gei_Particle", 0xA0, "particle")):
        obj = e.alloc(size)
        s.engine[name] = obj
        fixed[obj] = LABELS[label]
        w32(e, e.data_import(name), obj)
    # resource handle
    data = e.alloc(4, bytes(c.get("resource", [0xFF] * 4)))
    s.handle = e.alloc(4)
    w32(e, s.handle, data)
    s.data_path = c.get("data_path", "HD:TPW:Data")
    # rides: sets 20.., meshes 30..
    s.sets, s.meshes = [], []
    for k, ride in enumerate(c.get("rides", [])):
        if ride is None:
            s.sets.append(0)
            continue
        flags, has_heads, heads_f28, f14 = ride
        mesh = 0
        if has_heads:
            mesh = e.alloc(0x30)
            w32(e, mesh + 0x28, 1 if heads_f28 else 0)
            fixed[mesh] = 30 + k
        st = e.alloc(0x20)
        w32(e, st + 4, flags)
        w32(e, st + 8, mesh)
        w32(e, st + 0x14, 1 if f14 else 0)
        fixed[st] = 20 + k
        s.sets.append(st)
    for k in range(16):
        w32(e, A["gRideAnimSets"] + 4 * k, s.sets[k - 1] if 1 <= k <= len(s.sets) else 0)
    w32(e, A["gFirstPersonRideSet"], s.sets[0] if s.sets else 0)
    w32(e, A["gHeightfieldRideSet"], s.sets[c.get("heightfield", -1)] if c.get("heightfield", -1) >= 0 else 0)
    # mesh header for UpdateScratchGeomBufferSize
    if "mesh" in c:
        counts, nodes = c["mesh"]
        node_arr = e.alloc(0xA0 * max(len(nodes), 1))
        for n, (f58, f5c, f5e) in enumerate(nodes):
            e.write(node_arr + 0xA0 * n + 0x58, struct.pack(">H", f58))
            e.write(node_arr + 0xA0 * n + 0x5C, struct.pack(">HH", f5c, f5e))
        header = e.alloc(0x80)
        e.write(header + 0x3A, struct.pack(">H", counts[0]))
        e.write(header + 0x3E, struct.pack(">H", counts[1]))
        e.write(header + 0x42, struct.pack(">HH", counts[2], counts[3]))
        w32(e, header + 0x70, node_arr)
        s.mesh = e.alloc(0x30)
        w32(e, s.mesh + 4, header)
    # scalars, pointers, fields, options
    g = c.get("g", {})
    for name, size in SCALARS:
        (w8 if size == 1 else w32)(e, A[name], g.get(name, 0))
    w32(e, A["gMainWindow"], s.window if g.get("window", 1) else 0)
    w32(e, A["gOpenGL"], s.context)
    w32(e, A["gIdxDirectDraw"], 0)
    w32(e, A["gScratchGeomBuffer"], 0)
    e.write(A["gCursorSpritePosition"], struct.pack(">III", *g.get("cursor", (0, 0, 0))))
    for (base, off, size), v in zip(FIELDS, c.get("fields", [0] * len(FIELDS))):
        addr = s.context if base == "context" else (A[base] if base in A else e.data_import(base))
        (w8 if size == 1 else w32)(e, addr + off, v)
    for k, v in enumerate(c.get("options", [0] * 19)):
        w32(e, A["gOptionsBalance"] + 8 + 4 * k, v)
    e.write(e.data_import("gei_Display") + 0x900, struct.pack(">f", c.get("viewdist", 1.0)))
    w32(e, s.engine["gei_TCache"] + 0x18DC, c.get("tcache", (7, 7))[0])
    w32(e, s.engine["gei_TCache"] + 0x1908, c.get("tcache", (7, 7))[1])
    w32(e, s.engine["gei_DDraw"] + 0x784, c.get("devices", 2))
    e.write(s.engine["gei_ViewMatrix"], struct.pack(">16f", *c.get("view", [2.0] * 16)))
    e.write(e.data_import("gs_WorldBoundingArea"), struct.pack(">6f", *c.get("box", [3.0] * 6)))
    e.write(A["gTPGlobals"] + 0x4014, struct.pack(">f", c.get("step", 0.5)))
    for off, text in ((0x41A0, "low.sam"), (0x41B4, "med.sam"), (0x41C8, "high.sam")):
        CL.tds_set(e, A["gTPGlobals"] + off, text)
    e.cl = CL.PpcLog(fixed, c.get("q", {}))


def field_addr(e, base):
    return e.te.context if base == "context" else (A[base] if base in A else e.data_import(base))


def ppc_observe(e):
    L, s = e.cl, e.te
    out = {name: rd(e, A[name], size) for name, size in SCALARS}
    out["ptrs"] = {name: L.lab(e.u32(A[name])) for name in POINTERS}
    out["cursor"] = list(struct.unpack(">III", e.read(A["gCursorSpritePosition"], 12)))
    out["fields"] = [T.s32(rd(e, field_addr(e, b) + o, sz)) if sz == 4 else rd(e, field_addr(e, b) + o, sz)
                     for b, o, sz in FIELDS]
    out["options"] = [T.s32(e.u32(A["gOptionsBalance"] + 8 + 4 * k)) for k in range(19)]
    out["viewdist"] = e.u32(e.data_import("gei_Display") + 0x900)
    out["tcache"] = [T.s32(e.u32(s.engine["gei_TCache"] + o)) for o in (0x18DC, 0x1908)]
    out["view"] = [e.u32(s.engine["gei_ViewMatrix"] + 4 * k) for k in range(16)]
    out["box"] = [e.u32(e.data_import("gs_WorldBoundingArea") + 4 * k) for k in range(6)]
    out["particle"] = [HOOKS.get(e.u32(s.engine["gei_Particle"] + o), -1 if e.u32(s.engine["gei_Particle"] + o) else 0)
                       for o in (0x84, 0x88, 0x8C, 0x90, 0x94, 0x98, 0x9C)]
    out["rides"] = [e.u32(st + 4) if st else None for st in s.sets]
    return out

# ======================================================================= host side


class HostState:
    pass


def host_begin(lib, c):
    h = HostState()
    addr = lambda n: F(lib, "tpw_te_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    new = F(lib, "tpw_te_new", VP, I32)
    fixed = {}
    for name, label in LABELS.items():
        a = addr(name) if name not in ENGINE_PTRS else None
        if a:
            fixed[label] = a
    fixed[LABELS["window"]] = F(lib, "tpw_te_window", VP)()
    h.context = F(lib, "tpw_te_context", VP)()
    fixed[LABELS["context"]] = h.context
    fixed[LABELS["idx"]] = F(lib, "tpw_te_idx", VP)()
    h.engine = {}
    for name, size, label in (("gei_TCache", 0x4000, "tcache"), ("gei_DDraw", 0x1000, "ddraw"),
                              ("gei_ViewMatrix", 0x40, "view"), ("gei_MapWho", 0x40, "mapwho"),
                              ("gei_Particle", 0x200, "particle")):
        obj = new(size)
        h.engine[name] = obj
        fixed[LABELS[label]] = obj
        C.c_void_p.from_address(addr(name)).value = obj
    F(lib, "tpw_te_clear_particle_hooks", None, VP)(h.engine["gei_Particle"])
    res = (C.c_uint8 * 4).in_dll(lib, "tpw_te_resource")
    res[:] = c.get("resource", [0xFF] * 4)
    F(lib, "tpw_te_set_data_path", None, C.c_char_p)(c.get("data_path", "HD:TPW:Data").encode())
    h.sets = []
    set_ride = F(lib, "tpw_te_set_ride", None, VP, U32, VP, VP)
    set_mesh = F(lib, "tpw_te_set_mesh", None, VP, VP, VP)
    for k, ride in enumerate(c.get("rides", [])):
        if ride is None:
            h.sets.append(None)
            continue
        flags, has_heads, heads_f28, f14 = ride
        mesh = None
        if has_heads:
            mesh = new(0x80)      # the host's mesh record: larger than the Mac's 0x40 (pointers)
            set_mesh(mesh, None, 1 if heads_f28 else None)
            fixed[30 + k] = mesh
        st = new(0x40)
        set_ride(st, flags, mesh, 1 if f14 else None)
        fixed[20 + k] = st
        h.sets.append(st)
    arr = (C.c_void_p * 16).from_address(addr("gRideAnimSets"))
    for k in range(16):
        arr[k] = h.sets[k - 1] if 1 <= k <= len(h.sets) else None
    C.c_void_p.from_address(addr("gFirstPersonRideSet")).value = h.sets[0] if h.sets else None
    hf = c.get("heightfield", -1)
    C.c_void_p.from_address(addr("gHeightfieldRideSet")).value = h.sets[hf] if hf >= 0 else None
    if "mesh" in c:
        counts, nodes = c["mesh"]
        arena = F(lib, "tpw_te_arena", VP, C.c_uint32)
        node_arr = arena(0xA0 * max(len(nodes), 1) + 0x100)
        size = C.sizeof(C.c_void_p)
        for n, (f58, f5c, f5e) in enumerate(nodes):
            F(lib, "tpw_te_set_node", None, VP, C.c_uint16, C.c_uint16, C.c_uint16)(node_arr + 0xA0 * n, f58, f5c, f5e)
        header = arena(0x100)
        F(lib, "tpw_te_set_header", None, VP, VP, VP)(header, (C.c_uint16 * 4)(*counts), node_arr)
        h.mesh = new(0x80)
        set_mesh(h.mesh, header, None)
    g = c.get("g", {})
    for name, size in SCALARS:
        a = addr(name)
        if size == 1:
            C.c_uint8.from_address(a).value = g.get(name, 0) & 0xFF
        else:
            C.c_uint32.from_address(a).value = g.get(name, 0) & 0xFFFFFFFF
    C.c_void_p.from_address(addr("gMainWindow")).value = fixed[LABELS["window"]] if g.get("window", 1) else None
    C.c_void_p.from_address(addr("gOpenGL")).value = h.context
    C.c_void_p.from_address(addr("gIdxDirectDraw")).value = None
    C.c_void_p.from_address(addr("gScratchGeomBuffer")).value = None
    (C.c_uint32 * 3).from_address(addr("gCursorSpritePosition"))[:] = list(g.get("cursor", (0, 0, 0)))
    F(lib, "tpw_te_set_fields", None, VP)((I32 * len(FIELDS))(*[T.s32(v) for v in c.get("fields", [0] * len(FIELDS))]))
    opts = c.get("options", [0] * 19)
    F(lib, "tpw_te_set_options", None, VP, I32)((I32 * len(opts))(*[T.s32(v) for v in opts]), len(opts))
    C.c_float.from_address(F(lib, "tpw_te_view_distance", VP)()).value = c.get("viewdist", 1.0)
    tc = F(lib, "tpw_te_tcache_fields", VP, VP, I32)
    C.c_int32.from_address(tc(h.engine["gei_TCache"], 0)).value = c.get("tcache", (7, 7))[0]
    C.c_int32.from_address(tc(h.engine["gei_TCache"], 1)).value = c.get("tcache", (7, 7))[1]
    C.c_int32.from_address(F(lib, "tpw_te_ddraw_devices", VP, VP)(h.engine["gei_DDraw"])).value = c.get("devices", 2)
    (C.c_float * 16).from_address(h.engine["gei_ViewMatrix"])[:] = c.get("view", [2.0] * 16)
    (C.c_float * 6).from_address(addr("gs_WorldBoundingArea"))[:] = c.get("box", [3.0] * 6)
    F(lib, "tpw_te_set_strings", None, C.c_float)(c.get("step", 0.5))
    CL.host_begin(lib, fixed, c.get("q", {}))
    h.lib = lib
    return h


def host_observe(lib, h):
    addr = lambda n: F(lib, "tpw_te_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    lab = F(lib, "tpw_cl_lab", I32, VP)
    out = {}
    for name, size in SCALARS:
        a = addr(name)
        out[name] = C.c_uint8.from_address(a).value if size == 1 else C.c_uint32.from_address(a).value
    out["ptrs"] = {name: lab(C.c_void_p.from_address(addr(name)).value) for name in POINTERS}
    out["cursor"] = list((C.c_uint32 * 3).from_address(addr("gCursorSpritePosition")))
    vals = (I32 * len(FIELDS))()
    F(lib, "tpw_te_get_fields", None, VP)(vals)
    out["fields"] = list(vals)
    opts = (I32 * 19)()
    F(lib, "tpw_te_get_options", None, VP, I32)(opts, 19)
    out["options"] = list(opts)
    out["viewdist"] = C.c_uint32.from_address(F(lib, "tpw_te_view_distance", VP)()).value
    tc = F(lib, "tpw_te_tcache_fields", VP, VP, I32)
    out["tcache"] = [C.c_int32.from_address(tc(h.engine["gei_TCache"], w)).value for w in (0, 1)]
    out["view"] = list((C.c_uint32 * 16).from_address(h.engine["gei_ViewMatrix"]))
    out["box"] = list((C.c_uint32 * 6).from_address(addr("gs_WorldBoundingArea")))
    hooks = (I32 * 7)()
    F(lib, "tpw_te_particle_hooks", None, VP, VP)(h.engine["gei_Particle"], hooks)
    out["particle"] = list(hooks)
    out["rides"] = [F(lib, "tpw_te_ride_flags", U32, VP)(st) if st else None for st in h.sets]
    return out

# ======================================================================= running a case


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs(runner.emu)


class TeTest(FnTest):
    def run_case(self, runner, case):
        mism = super().run_case(runner, case)
        if mism and any(m.startswith("log") for m in mism) and getattr(self, "_last", None):
            p, h = self._last
            return [f"log: {CL.first_difference(p['log'], h['log'])}"] + [m for m in mism if not m.startswith("log")][:3]
        return mism


def fn_test(name, args=None, result=True, edge=None, rnd=None, post=None):
    """args(c, side) -> list of argument values; side is ("ppc", e) or ("host", lib, h);
    post(side) -> an extra observation after the call."""
    t = TeTest(name, None, None, edge=edge, rnd=rnd)

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            a = args(c, ("ppc", e)) if args else []
            r3 = T.call_ppc(e, F_[name], [T.u32(v) for v in a])[0]
            res = T.s32(r3)
            out = ppc_observe(e)
            out["log"] = e.cl.log
            out["post"] = post(("ppc", e)) if post else None
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        out["result"] = res if result else None
        t._p = out
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        a = args(c, ("host", lib, h)) if args else []
        vals = (I64 * 8)(*([int(v) for v in a] + [0] * (8 - len(a))))
        res = F(lib, "tpw_te_call", I32, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, h)
        out["log"] = log
        out["post"] = post(("host", lib, h)) if post else None
        out["result"] = res if result else None
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t

# ======================================================================= case generators


def rnd_options(r):
    return [r.choice([0, 1, 2, 3, 4, -1, 7]) if k != 17 else r.choice([0, 1, 2, 3, 4, -1]) for k in range(19)]


def base(r, **extra):
    c = dict(g=dict(gGameOptions=r.getrandbits(32), gTPDialogOptions=r.getrandbits(32),
                    gTPDialogTextureMegabytes=r.choice([0, 0, 8, 16, 0xFFF]), gScreenWidth=r.choice([640, 800, 1024, 0x12345]),
                    gScreenHeight=r.choice([480, 600, 768, -1]), gNoSavedSettings=r.choice([0, 0, 1]),
                    gFirstPersonFlags=r.choice([0, 4, 0x16, 2, r.getrandbits(32)]), gRideCountA=r.randint(0, 3),
                    gRideCountB=r.randint(0, 3), gMaxMeshFaces4=r.randint(0, 100), gMaxMeshF3E=r.randint(0, 100),
                    gMaxMeshNodes=r.randint(0, 100), gSmallTexturesOnly=r.choice([0, 1]),
                    gCursorOverSprite=r.getrandbits(32), g3DRefreshFlags=r.getrandbits(32),
                    gMemorySystemInitialised=r.choice([0, 7]), gScratchGeomVertices=r.randint(0, 3000),
                    cursor=(r.getrandbits(32), r.getrandbits(32), r.getrandbits(32))),
             fields=[r.choice([0, 1, 2]), r.choice([0, 1, 2, 3]), r.choice([0, 1, 2]), r.choice([0, 1, 2]),
                     r.getrandbits(32), r.getrandbits(31), r.getrandbits(16), r.getrandbits(32), r.getrandbits(32),
                     r.choice([0, 1, 2, 3, 4, 5, -1]), 0, r.getrandbits(8), r.getrandbits(8), r.getrandbits(16),
                     r.choice([0x800000, 0x800001, 0x7FFFFF, 0x1000000, 0, 0xFFFFFFFF])],
             options=rnd_options(r), viewdist=r.uniform(-5, 5), tcache=(r.randint(-5, 5), r.randint(-5, 5)),
             devices=r.choice([0, 1, 2, 3, -1]), view=[r.uniform(-9, 9) for _ in range(16)],
             box=[r.uniform(-9, 9) for _ in range(6)], step=r.uniform(0, 2), rides=[], resource=[0xFF] * 4,
             q={}, data_path=r.choice(["HD:TPW:Data", "", "x" * 50]))
    c.update(extra)
    return c


def rnd_rides(r):
    rides = [(r.choice([0, 0x20, 0x200000, 0x3000, 0x1000, 0x220020, r.getrandbits(32)]), r.random() < 0.8,
              r.random() < 0.5, r.random() < 0.7) for _ in range(r.randint(1, 6))]
    rides[0] = (rides[0][0], True, rides[0][2], rides[0][3])   # the first-person ride has its heads
    for k in range(1, len(rides)):
        if r.random() < 0.2:
            rides[k] = None                                         # an empty slot
    return rides


def gen(make):
    def edge():
        r = random.Random(5)
        return [make(r) for _ in range(40)]
    return edge, make


def g_simple(r):
    return base(r, q={"CClock::Read": [r.getrandbits(32)], "TPFile_Open": [r.getrandbits(31)],
                      "TPFile_Read": [r.getrandbits(31)], "TPFile_Close": [r.getrandbits(31)],
                      "TPFile_Seek": [r.getrandbits(31)], "TPFile_Size": [r.getrandbits(31)],
                      "GlobalFree": [r.choice([0, 0x1234])]})


def g_debug(r):
    c = base(r)
    c["resource"] = [r.choice([0, 1, 2, 0xFF, 0xFE]) for _ in range(4)]
    c["q"] = {"FSMakeFSSpec": [r.choice([0, 0, 0, -43, 1])], "FSpOpenResFile": [r.choice([7, 7, -1, 0, 12])],
              "CurResFile": [r.choice([3, 1, -2])], "GetNamedResource": [r.choice([1, 1, 0])]}
    return c


def g_startup(r):
    c = g_debug(r)
    c["q"].update({"CBalanceTable::Load": [r.choice([1, 1, 1, 0])], "NewCWindow": [r.choice([1, 1, 1, 1, 0])],
                   "IdxMakeOpenGLImplementation": [r.choice([1, 1, 1, 1, 0])],
                   "DDraw_Init": [r.choice([1, 1, 1, 0])], "RenderSystem_Init": [r.choice([1, 1, 1, 0])],
                   "EngineAdd_Init": [r.choice([1, 1, 1, 0])]})
    c["g"]["window"] = 1
    return c


def g_rides(r):
    c = base(r, rides=rnd_rides(r))
    c["heightfield"] = r.choice([-1, 0])
    c["g"]["gRideCountA"] = r.randint(0, len(c["rides"]))
    c["g"]["gRideCountB"] = r.randint(0, len(c["rides"]) - c["g"]["gRideCountA"])
    c["mode"] = r.choice([2, 2, 3, 0, 1, 7])
    return c


def g_mesh(r):
    nodes = [(r.randint(0, 0xFFFF), r.randint(0, 0xFFFF), r.randint(0, 0xFFFF)) for _ in range(r.randint(0, 5))]
    counts = (r.randint(0, 0xFFFF), r.randint(0, 0xFFFF), r.randint(0, 0xFFFF), len(nodes))
    c = base(r, mesh=(counts, nodes))
    c["g"]["gScratchGeomVertices"] = r.choice([0, 100, 0xFFFF, 0x10000])
    for name in ("gMaxMeshFaces4", "gMaxMeshF3E", "gMaxMeshNodes"):
        c["g"][name] = r.choice([0, 50, 0xFFFF, 0x3FFFC, 0x40000])
    return c


def g_alloc(r):
    c = base(r)
    c["size"] = r.choice([0, 1, 12, 0x400, 0xFFFF])
    c["flags"] = r.getrandbits(32)
    return c


def text_args(c, side, *texts):
    if side[0] == "ppc":
        return [CL.new_text(side[1], t) for t in texts]
    keep = [C.create_string_buffer(t.encode("latin-1")) for t in texts]
    side[2].keep = keep
    return [C.addressof(k) for k in keep]


def labelled_block(side, label, content=0):
    """A 4-byte slot holding a new 16-byte block that gets `label` (0: an empty slot); returns
    the slot's address."""
    if side[0] == "ppc":
        e = side[1]
        slot = e.alloc(4)
        if label:
            block = e.alloc(16)
            e.cl.labels[block] = label
            w32(e, slot, block)
        e.te.slot = slot
        return slot
    lib, h = side[1], side[2]
    slot = C.c_void_p(None)
    h.keep_slot = slot
    if label:
        block = F(lib, "tpw_te_new", VP, I32)(16)
        lib.tpw_cl_label(label, block)
        slot.value = block
    return C.addressof(slot)


def slot_label(side):
    if side[0] == "ppc":
        e = side[1]
        return e.cl.lab(e.u32(e.te.slot))
    return side[1].tpw_cl_lab(side[2].keep_slot.value)


TESTS = []


# Functions without a result (r3 is whatever the original leaves there).
VOID = {"MacLoadDebugSettings", "MacUpdateMouse", "EngineStartup_SetOptions", "EngineStartup_Dialog", "EngineTick1",
        "EngineTick2", "EngineTick3", "EngineCloseDown", "ResetWorldBoundingArea", "TPForce3DRefresh",
        "ResetCursorOverSprites", "InitialiseInstancing", "EnableProceduralTexturing", "DisableProceduralTexturing",
        "InitialiseMemorySystem", "UpdateScratchGeomBufferSize", "CreateScratchGeomBuffer",
        "DestroyScratchGeomBuffer", "AnimateRides", "MacFree"}


def add(name, gen_fn, args=None, result=True, post=None):
    edge, rnd = gen(gen_fn)
    TESTS.append(fn_test(name, args, result and name not in VOID, edge, rnd, post))


def block_ptr(side):
    """The block in the slot labelled_block made."""
    if side[0] == "ppc":
        return side[1].u32(side[1].te.slot)
    return side[2].keep_slot.value or 0


def slot_raw(side):
    if side[0] == "ppc":
        return side[1].u32(side[1].te.slot)
    return side[2].keep_slot.value or 0


def point_arg(c, side):
    if not c["point"]:
        return [0]
    words = c["point"]
    if side[0] == "ppc":
        a = side[1].alloc(12)
        side[1].write(a, struct.pack(">III", *words))
        return [a]
    buf = (C.c_uint32 * 3)(*words)
    side[2].keep_point = buf
    return [C.addressof(buf)]


def mesh_arg(c, side):
    return [side[1].te.mesh if side[0] == "ppc" else side[2].mesh]


for nm in ("MacIsFirstPerson", "MacReadClock", "MacUpdateMouse", "EngineStartup_SetOptions",
           "EngineStartup_Dialog", "EngineTick2", "EngineTick3", "EngineCloseDown", "ResetWorldBoundingArea",
           "EnableProceduralTexturing", "DisableProceduralTexturing", "IsProceduralTexturingActive",
           "InitialiseMemorySystem", "DestroyScratchGeomBuffer"):
    add(nm, g_simple)
add("MacLoadDebugSettings", g_debug)
add("EngineStartup_OptionsFile", g_startup, lambda c, s: [c["g"]["gTPDialogOptions"] & 0x3800 or 0x800])
add("EngineStartup_SettingsFile", g_startup)
add("EngineStartup_Final", g_startup, lambda c, s: [c["g"]["gScreenWidth"], 1, 0, 5])
add("EngineStartup", g_startup)
add("TPForce3DRefresh", g_simple, lambda c, s: [c["g"]["gMaxMeshNodes"] * 0x10001])
add("InitialiseInstancing", g_simple, lambda c, s: [7])
add("CreateScratchGeomBuffer", g_simple, lambda c, s: [c["g"]["gMaxMeshF3E"] % 3])
add("EngineTick1", g_rides)
add("AnimateRides", g_rides, lambda c, s: [c["mode"]])
add("FindFilePath", g_simple, lambda c, s: text_args(c, s, "Data:file.tga") + [c["g"]["gMaxMeshF3E"] % 3], result=False)
add("MacOpen", g_simple, lambda c, s: text_args(c, s, "Data:x.wad", "rb"))
add("MacClose", g_simple, lambda c, s: [0x55])
add("MacSize", g_simple, lambda c, s: [0x56])
add("MacSeek", g_simple, lambda c, s: [0x57, c["g"]["gMaxMeshNodes"], 2])
add("MacRead", g_simple, lambda c, s: [0x58, 16, c["g"]["gMaxMeshF3E"], 0x59])
add("MReleaseAllocF", g_alloc, lambda c, s: [c["size"], c["flags"]], result=False)
add("MacMalloc", g_alloc, lambda c, s: [c["size"]], result=False)
add("MacFree", g_simple, lambda c, s: [labelled_block(s, c["g"]["gRideCountA"] and 50)], result=False, post=slot_label)
add("MReleaseFreeF", g_simple, lambda c, s: [(labelled_block(s, c["g"]["gRideCountA"] and 50), block_ptr(s))[1]],
    result=False)
add("HNewHeap", g_simple, lambda c, s: [11, 22, 33, labelled_block(s, 0)], post=slot_raw)
add("HFreeHeap", g_simple, lambda c, s: [0x77])
add("ResetCursorOverSprites", lambda r: base(r, point=r.choice([None, (r.getrandbits(32), r.getrandbits(32),
                                                                    r.getrandbits(32))])), point_arg)
add("UpdateScratchGeomBufferSize", g_mesh, mesh_arg)
