"""Differential test for unit w133 (src/game/TPLevel.cpp, Debug_overlays.cpp, Debug_blendflags.cpp, LLCellSystem_hoarding.cpp,
intrface_pointed.cpp, hlsprite_zone.cpp, ActionRecorder.cpp): TickLevelLights, TickFlashObjects, TickRideHighlights, SetBaseBlendFlags,
LLCC_DrawInitialHoarding, CIntrface::DoPointedThingComputations, FLY_SetFlyZone, CActionRecorder::DoTick and ResetActionRecording,
against libtpw_unit_ParkTicks.dylib (tests/unit_ParkTicks.cpp is the host's half).

The nine functions run as the original on the PowerPC side; every callee is a fake on both sides that records its name, arguments and
the answers it was given from the case's queues. The meshes of the flash and highlight ticks, the sun, the thing array, the world's turn
word and the recorder are built from the same description on both sides. Pointers are compared as labels, floats as bit patterns (NaNs
equal), memory as the words the function could have changed.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkTicks"

MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000

TICK_LIGHTS, TICK_FLASH, TICK_HIGHLIGHT, BLEND_FLAGS = 0x100645B0, 0x1006D5FC, 0x1006CFF8, 0x1006EBEC
HOARDING, POINTED, FLY_ZONE, DO_TICK, RESET_RECORDING = 0x10095420, 0x1007DBB8, 0x100755A0, 0x10144F80, 0x10143E08
OWN = [TICK_LIGHTS, TICK_FLASH, TICK_HIGHLIGHT, BLEND_FLAGS, HOARDING, POINTED, FLY_ZONE, DO_TICK, RESET_RECORDING]

G_LIGHT, G_DIRECTION, G_COLOUR, G_AMBIENT, G_SCALE = 0x102C6C84, 0x102C6C88, 0x102C6C94, 0x102C6C98, 0x1029CF78
G_FLASH_PHASE, G_FLASH_LIST, G_TPGLOBALS_F4014, G_HIGHLIGHT = 0x102D1EFC, 0x102D1F00, 0x103B74C8 + 0x4014, 0x102C6CE8
G_DIALOG, G_4818, G_LEFT, G_TOP, G_RIGHT, G_BOTTOM, G_E0CC = 0x103BC664, 0x102E4818, 0x1029E044, 0x1029E048, 0x1029E03C, 0x1029E040, 0x102DE0CC
G_BALANCE, G_THINGS, G_INTRFACE, G_RECORDER, G_WORLD = 0x102B434C, 0x1034CBFC, 0x102DC1AC, 0x1037EC44, 0x1037EBFC
G_FLY_B, G_FLY_C, G_FLY_D, G_FLY_A = 0x1029D2B0, 0x1029D2B8, 0x1029D2AC, 0x1029D2B4
FILE_CLOSE_CODE = 0x1006FC90

STUBS = None


# ------------------------------------------------------------------ helpers


def fb(x):
    if x != x:
        return NAN_BITS
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def canon(bits):
    bits &= MASK
    return NAN_BITS if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def pw(e, address, *words):
    e.write(address, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def pw16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xFFFF))


def rfloat_bits(r):
    choice = r.random()
    if choice < 0.2:
        return fb(r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 255.0, math.inf, -math.inf, math.nan]))
    if choice < 0.3:
        return r.getrandbits(32)
    return fb(T.f32(r.uniform(-3, 3)))


def hlib(lib):
    if not getattr(lib, "_pt_ready", False):
        lib.tpw_pt_addr.restype = C.c_void_p
        lib.tpw_pt_addr.argtypes = [C.c_char_p]
        lib.tpw_pt_build.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
        lib.tpw_pt_dump.argtypes = [C.POINTER(C.c_uint32)]
        lib.tpw_pt_dump.restype = C.c_int32
        lib.tpw_pt_light_dump.argtypes = [C.POINTER(C.c_uint32)]
        lib.tpw_pt_get_interface.argtypes = [C.POINTER(C.c_int32)]
        lib.tpw_pt_call.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_pt_set_turn.argtypes = [C.c_uint32]
        lib.tpw_pt_set_thing.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_pt_set_row.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_pt_set_interface.argtypes = [C.c_int32, C.c_int32, C.c_int32]
        lib._pt_ready = True
    return lib


def haddr(lib, name):
    return hlib(lib).tpw_pt_addr(name.encode())


def hset(lib, name, ctype, value):
    ctype.from_address(haddr(lib, name)).value = value


def hget(lib, name, ctype):
    return ctype.from_address(haddr(lib, name)).value


# ------------------------------------------------------------------ the PowerPC fakes


def make_stubs():
    S = CL.Stubs()
    S.real.update(OWN)

    def ctrace(e):
        text = e.cstr(e.r(4))[:159]
        ints = [e.r(3) & MASK]
        if "%" in text:
            ints.append(T.s32(e.r(5)))
        e.cl.rec("CTRACE", ints, [text])

    def assertmsg(e):
        e.cl.rec("GAME_ASSERTMSG", [T.s32(e.r(3))], [e.cstr(e.r(4))[:159]])

    S.internal(0x1006FCBC, ctrace)
    S.internal(0x1000DDE8, assertmsg)
    S.internal(0x10088934, CL.recorder("LLCM_SetIllegalPlacement", "i"))
    S.internal(0x10090E18, CL.recorder("LLCC_SystemSetMode", "ib"))
    S.internal(0x10088590, CL.recorder("LLCM_SetCurrentObject", "b"))
    S.internal(0x100957A4, CL.recorder("LLCC_DrawHoardingsFromBalanceFile", "L"))
    S.internal(0x100956A8, CL.recorder("LLCC_DrawDefaultHoardings"))
    S.internal(0x10093934, CL.recorder("LLCC_LockHoarding", "b"))
    S.internal(0x10093B0C, CL.recorder("LLCC_FillInsideOfPark"))
    S.internal(0x1008E6E0, CL.recorder("LLCC_BuildHoardingDB", "iiiibb"))
    S.internal(0x1005C59C, CL.recorder("RecalculateParkDarkening"))

    def over_ride(e):
        e.cl.rec("LLCR_OverRide", [e.r(4) & MASK, e.r(5) & MASK])
        pw16(e, e.r(3), e.cl.ret("OverRide"))

    def extents(e):
        e.cl.rec("LLCR_GetHoardingExtents", [e.cl.lab(e.r(i)) for i in (3, 4, 5, 6)])
        for i in (3, 4, 5, 6):
            pw(e, e.r(i), e.cl.ret("Extents"))

    def entrance(e):
        world = e.u32(G_WORLD)
        e.cl.rec("GetParkEntrancePos", [T.s32((e.r(4) - world) & MASK), T.s32(e.r(5))])
        pw16(e, e.r(3), e.cl.ret("Entrance"))

    def cell_index(e):
        map_ = (e.u32(G_WORLD) + 0x2D8) & MASK
        return T.s32((e.r(3) - map_) & MASK) // 0x44

    def unset_flag(e):
        e.cl.rec("UnSetFlag", [cell_index(e), e.r(4) & 0xFFFF])

    def set_hfield(e):
        e.cl.rec("SetHFieldTile", [cell_index(e)])

    S.internal(0x10089210, over_ride)
    S.internal(0x1008F9C8, extents)
    S.internal(0x10101AE0, entrance)
    S.internal(0x1009BC18, unset_flag)
    S.internal(0x1009CDB4, set_hfield)

    S.internal(0x10144CE4, CL.recorder("EndReplay", "L"))
    S.internal(0x10145360, CL.recorder("GetNextCameraFileEntry", "L"))
    S.internal(0x101453A8, CL.recorder("ReadAndProcessNextAction", "L", ret=lambda e, L: L.ret("ReadAction")))
    S.internal(0x10145318, CL.recorder(
        "GetNextTimingFileEntry", "L", then=lambda e, L: pw(e, e.r(3) + 0x30, L.ret("Timing"))))

    def record_action(e):
        data = e.read(e.r(5), 128)
        any_ = 0
        for b in data:
            any_ |= b
        e.cl.rec("RecordAction", [e.cl.lab(e.r(3)), T.s32(e.r(4)), any_])

    S.internal(0x10144BEC, record_action)
    S.internal(FILE_CLOSE_CODE, CL.recorder("CFile::Close", "L"))

    S.imp("UpdateLights__7CSystemFv", CL.recorder("UpdateLights", "L"))

    def get_absolute(e):
        e.cl.rec("GetAbsolute")
        e.set_r(3, 0)
        e.set_r(4, 0)

    S.imp("GetAbsolute__Q213SamsUtilities6UTimerFv", get_absolute)

    def fmod(e):
        x, y = e.f(1), e.f(2)
        if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
            e.set_f1(math.nan)
        else:
            e.set_f1(math.fmod(x, y))

    S.imp("fmod", fmod)

    def mem_open(e):
        e.cl.rec("CMemFile::Open", [e.cl.lab(e.r(4)), e.r(5) & MASK])
        pw(e, e.r(3), 0)

    S.imp("Open__Q33bfl10FileSystem8CMemFileFUi", mem_open)
    return S


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = make_stubs()


# ------------------------------------------------------------------ running a case


def ppc_run(e, case, entry, args, prepare, collect, fixed=None):
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog(fixed(e) if fixed else {}, case.get("queues"))
        prepare(e, case)
        T.call_ppc(e, entry, args(e, case))
        out = collect(e, case)
        out["log"] = e.cl.log
        return out
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, case, op, arg, prepare, collect, fixed=None):
    hlib(lib)
    lib.tpw_pt_prepare()
    CL.host_begin(lib, fixed(lib) if fixed else {}, case.get("queues"))
    try:
        prepare(lib, case)
        lib.tpw_pt_call(op, arg(case))
    finally:
        log = CL.host_end(lib)
    out = collect(lib, case)
    out["log"] = log
    return out


# ------------------------------------------------------------------ the sun (TickLevelLights)


def ppc_light_state(e, case):
    holder = e.alloc(0x80, b"\xa5" * 0x80)
    light = holder + 0x10
    pw(e, holder, light)
    record = e.alloc(0x14, b"\0" * 0x14)
    pw(e, record + 4, holder)
    pw(e, G_LIGHT, record)
    case["_light"] = light
    case["_holder"] = holder
    pw(e, G_DIRECTION, *case["direction"])
    pw(e, G_COLOUR, case["colour"])
    pw(e, G_AMBIENT, case["ambient"])
    pw(e, G_SCALE, case["scale"])
    pw(e, e.data_import("GlobalDiffuse"), *([0xA5A5A5A5] * 4))


def ppc_lights(e, case):
    def prepare(e, c):
        ppc_light_state(e, c)

    def collect(e, c):
        light = c["_light"]
        return {"normal": [canon(e.u32(light + 0x2C + 4 * i)) for i in range(3)], "word": e.u32(light + 0x54),
                "colour": [canon(e.u32(light + 4 + 4 * i)) for i in range(4)],
                "direction": [canon(e.u32(G_DIRECTION + 4 * i)) for i in range(3)],
                "diffuse": [canon(e.u32(e.data_import("GlobalDiffuse") + 4 * i)) for i in range(4)]}

    return ppc_run(e, case, TICK_LIGHTS, lambda e, c: [c["force"]], prepare, collect,
                   fixed=lambda e: {e.data_import("gei_System"): 1})


def host_lights(lib, case):
    def prepare(lib, c):
        lib.tpw_pt_light_prepare()
        for i in range(3):
            C.c_uint32.from_address(haddr(lib, "direction") + 4 * i).value = c["direction"][i]
        hset(lib, "colour", C.c_uint32, c["colour"])
        hset(lib, "ambient", C.c_uint32, c["ambient"])
        hset(lib, "scale", C.c_uint32, c["scale"])
        for i in range(4):
            C.c_uint32.from_address(haddr(lib, "diffuse") + 4 * i).value = 0xA5A5A5A5

    def collect(lib, c):
        out = (C.c_uint32 * 8)()
        lib.tpw_pt_light_dump(out)
        return {"normal": [canon(out[i]) for i in range(3)], "word": out[3], "colour": [canon(out[4 + i]) for i in range(4)],
                "direction": [canon(C.c_uint32.from_address(haddr(lib, "direction") + 4 * i).value) for i in range(3)],
                "diffuse": [canon(C.c_uint32.from_address(haddr(lib, "diffuse") + 4 * i).value) for i in range(4)]}

    return host_run(lib, case, 0, lambda c: c["force"], prepare, collect, fixed=lambda lib: {1: haddr(lib, "system")})


def lights_case(force=1, direction=(1.0, 0.0, 0.0), colour=0xFF8040C0, ambient=0x10203040, scale=1.0):
    return {"force": force, "direction": [fb(x) for x in direction], "colour": colour, "ambient": ambient, "scale": fb(scale)}


def lights_edges():
    return [lights_case(), lights_case(force=0), lights_case(direction=(0, 0, 0)), lights_case(direction=(math.inf, 1, 0)),
            lights_case(direction=(math.nan, 1, 0)), lights_case(direction=(1, 2, 3), scale=2.5), lights_case(colour=0, ambient=0),
            lights_case(colour=MASK, ambient=MASK), lights_case(direction=(-1.5, 0.25, -3.0), scale=0.0)]


def lights_random(r):
    c = lights_case(force=r.choice([0, 1, 1, 1, 2, -1]))
    c["direction"] = [rfloat_bits(r) for _ in range(3)]
    c["colour"], c["ambient"] = r.getrandbits(32), r.getrandbits(32)
    c["scale"] = r.choice([fb(1.0), fb(T.f32(r.uniform(-4, 4))), rfloat_bits(r)])
    return c


# ------------------------------------------------------------------ the meshes (flash, highlights, blend flags)
# A case's "sets": a list of sets, a set a list of nodes, a node {flags, nwords, tex: [halfwords], faces: [texture index]}.


def flatten(sets):
    spec = [len(sets)]
    for nodes in sets:
        spec.append(len(nodes))
        for n in nodes:
            spec += [n["flags"], n["nwords"], len(n["faces"]), len(n["tex"])] + list(n["tex"]) + list(n["faces"])
    return spec


def ppc_build(e, sets):
    """Lays the sets out in the Mac's layout; returns (first set, first node, per node (points, textures))."""
    first_set = first_node = 0
    previous = 0
    infos = []
    for nodes in sets:
        array = e.alloc(0xA0 * max(len(nodes), 1))
        for i, n in enumerate(nodes):
            a = array + 0xA0 * i
            points = e.alloc(4 * (n["nwords"] + 2), b"\xa5" * (4 * (n["nwords"] + 2)))
            textures = e.alloc(8 * max(len(n["tex"]), 1))
            for t, half in enumerate(n["tex"]):
                pw16(e, textures + 8 * t, half)
                pw16(e, textures + 8 * t + 2, 0x5A5A)
            faces = e.alloc(0x10 * max(len(n["faces"]), 1))
            for f, index in enumerate(n["faces"]):
                pw(e, faces + 0x10 * f, textures + 8 * index)
            pw(e, a, n["flags"])
            pw16(e, a + 0x5A, len(n["faces"]))
            pw16(e, a + 0x5E, n["nwords"])
            pw(e, a + 0x64, points)
            pw(e, a + 0x6C, faces)
            infos.append((points, n["nwords"], [textures + 8 * t for t in range(len(n["tex"]))]))
        header = e.alloc(0xC0)
        pw16(e, header + 0x44, len(nodes))
        pw(e, header + 0x9C, header)   # (TickRideHighlights also loads the node pointer of the header this points to, a dead value)
        pw(e, header + 0x70, array)
        mesh = e.alloc(0x10)
        pw(e, mesh + 4, header)
        s = e.alloc(0xE0)
        pw(e, s + 8, mesh)
        if previous:
            pw(e, previous + 0xD0, s)
        else:
            first_set, first_node = s, array
        previous = s
    return first_set, first_node, infos


def ppc_mesh_dump(e, infos):
    out = []
    for points, nwords, textures in infos:
        out += [e.u32(points + 4 * i) for i in range(nwords + 2)]
        for t in textures:
            out += [struct.unpack(">H", e.read(t, 2))[0], struct.unpack(">H", e.read(t + 2, 2))[0]]
    return out


def mesh_case(sets, **extra):
    c = {"sets": sets, "phase": fb(0.25), "frame": fb(0.1), "arg": 1}
    c.update(extra)
    return c


def run_mesh(op, entry):
    """op: the library's call number for the function."""
    def ppc(e, case):
        def prepare(e, c):
            first, node, infos = ppc_build(e, c["sets"])
            c["_infos"], c["_node"] = infos, node
            pw(e, G_FLASH_LIST, first if op == 1 else 0xC0DEC0DE)
            pw(e, G_HIGHLIGHT, first if op == 2 else 0)
            pw(e, G_FLASH_PHASE, c["phase"])
            pw(e, G_TPGLOBALS_F4014, c["frame"])

        def collect(e, c):
            return {"mesh": ppc_mesh_dump(e, c["_infos"]), "phase": canon(e.u32(G_FLASH_PHASE))}

        def args(e, c):
            return [0, c["_node"], c["arg"]] if op == 3 else []

        return ppc_run(e, case, entry, args, prepare, collect)

    def host(lib, case):
        def prepare(lib, c):
            spec = (C.c_uint32 * len(flatten(c["sets"])))(*flatten(c["sets"]))
            lib.tpw_pt_build(spec, 1 if op == 2 else 0)
            hset(lib, "flashPhase", C.c_uint32, c["phase"])
            hset(lib, "frameTime", C.c_uint32, c["frame"])

        def collect(lib, c):
            buffer = (C.c_uint32 * 4096)()
            n = lib.tpw_pt_dump(buffer)
            return {"mesh": [buffer[i] for i in range(n)], "phase": canon(hget(lib, "flashPhase", C.c_uint32))}

        return host_run(lib, case, op, lambda c: c["arg"], prepare, collect)

    return ppc, host


def random_sets(r, min_sets=0, max_sets=3):
    sets = []
    for _ in range(max(min_sets, r.choice([0, 1, 1, 2, max_sets]))):
        nodes = []
        for _ in range(r.randrange(1, 4)):
            ntex = r.randrange(0, 5)
            nodes.append({"flags": r.choice([0, 0x4000, 0x200, 0x4200, r.getrandbits(32), r.getrandbits(32) | 0x4000,
                                             r.getrandbits(32) & ~0x4000]),
                          "nwords": r.randrange(0, 7),
                          "tex": [r.choice([0, 0x800, 0xF7FF, 0xFFFF, r.getrandbits(16)]) for _ in range(ntex)],
                          "faces": [r.randrange(ntex) for _ in range(r.randrange(0, 5))] if ntex else []})
        sets.append(nodes)
    return sets


def mesh_random(r):
    c = mesh_case(random_sets(r))
    c["phase"] = r.choice([fb(0.0), fb(1.9999), fb(T.f32(r.uniform(0, 2))), rfloat_bits(r)])
    c["frame"] = r.choice([fb(0.0), fb(T.f32(r.uniform(0, 1))), fb(T.f32(r.uniform(0, 4))), rfloat_bits(r)])
    return c


def mesh_edges():
    one = [{"flags": 0x4000, "nwords": 3, "tex": [], "faces": []}]
    return [mesh_case([]), mesh_case([one]), mesh_case([[{"flags": 0, "nwords": 3, "tex": [], "faces": []}]]),
            mesh_case([one, one, one]), mesh_case([[{"flags": 0x4200, "nwords": 0, "tex": [], "faces": []}]]),
            mesh_case([one], phase=fb(1.99), frame=fb(1.0)), mesh_case([one], phase=fb(math.nan)),
            mesh_case([one], phase=fb(math.inf)), mesh_case([one], frame=fb(math.inf)), mesh_case([one], phase=fb(-0.5)),
            mesh_case([[{"flags": 0x4000, "nwords": 6, "tex": [], "faces": []},
                        {"flags": 0x4000, "nwords": 1, "tex": [], "faces": []}]], phase=fb(0.7), frame=fb(0.01))]


def blend_random(r):
    c = mesh_case(random_sets(r, min_sets=1, max_sets=1))
    c["arg"] = r.choice([0, 1, 2, -1, r.getrandbits(32) - (1 << 31)])
    return c


def blend_edges():
    node = lambda flags, tex, faces: {"flags": flags, "nwords": 0, "tex": tex, "faces": faces}
    return [mesh_case([[node(0, [0, 0x800, 0xFFFF], [0, 1, 2, 0, 1])]], arg=1),
            mesh_case([[node(0, [0, 0x800, 0xFFFF], [0, 1, 2, 0, 1])]], arg=0),
            mesh_case([[node(0x200, [0, 0x800], [0, 1])]], arg=1), mesh_case([[node(0x200, [0, 0x800], [0, 1])]], arg=0),
            mesh_case([[node(0, [], [])]], arg=1), mesh_case([[node(0, [0x7F7F], [0, 0, 0, 0])]], arg=-1),
            mesh_case([[node(0, [0xF7FF], [0])]], arg=0), mesh_case([[node(0, [0x0800], [0])]], arg=2)]


# ------------------------------------------------------------------ the hoarding


def hoarding_case(entrances=(0x1234, 0x1236), dialog=0, extents=(-1, -1, -1, -1), balance=0xAA, e0cc=0x77):
    return {"queues": {"Entrance": list(entrances)}, "dialog": dialog, "extents": extents, "flag": balance, "e0cc": e0cc,
            "turn": 0}


def ppc_world(e, c):
    block = e.alloc(16)
    world = (block + 12 - 0x1DA70C) & MASK
    pw(e, G_WORLD, world)
    pw(e, block + 12, c.get("turn", 0))


def hoarding_pre_ppc(e, c):
    ppc_world(e, c)
    pw(e, G_DIALOG, c["dialog"])
    e.write(G_4818, bytes([c["flag"]]))
    e.write(G_E0CC, bytes([c["e0cc"] & 0xFF]))
    for address, v in zip((G_LEFT, G_TOP, G_RIGHT, G_BOTTOM), c["extents"]):
        pw(e, address, v)


def ppc_hoarding(e, case):
    def collect(e, c):
        return {"extents": [T.s32(e.u32(a)) for a in (G_LEFT, G_TOP, G_RIGHT, G_BOTTOM)], "flag": e.read(G_4818, 1)[0],
                "e0cc": e.read(G_E0CC, 1)[0]}

    return ppc_run(e, case, HOARDING, lambda e, c: [], hoarding_pre_ppc, collect, fixed=lambda e: {G_BALANCE: 5})


def host_hoarding(lib, case):
    def prepare(lib, c):
        lib.tpw_pt_set_turn(0)
        hset(lib, "dialogOptions", C.c_uint32, c["dialog"])
        hset(lib, "llcc4818", C.c_uint8, c["flag"])
        hset(lib, "llccE0CC", C.c_uint8, c["e0cc"] & 0xFF)
        for name, v in zip(("left", "top", "right", "bottom"), c["extents"]):
            hset(lib, name, C.c_int32, v)

    def collect(lib, c):
        return {"extents": [hget(lib, n, C.c_int32) for n in ("left", "top", "right", "bottom")],
                "flag": hget(lib, "llcc4818", C.c_uint8), "e0cc": hget(lib, "llccE0CC", C.c_uint8)}

    return host_run(lib, case, 4, lambda c: 0, prepare, collect, fixed=lambda lib: {5: haddr(lib, "balance")})


def hoarding_edges():
    return [hoarding_case(), hoarding_case(dialog=0x400), hoarding_case(dialog=0xFFFFFBFF),
            hoarding_case(entrances=(0x1234, 0x1234)), hoarding_case(entrances=(0x0102, 0x0108)),
            hoarding_case(entrances=(0x0102, 0x0102)), hoarding_case(entrances=(0x0301, 0x0001)),
            hoarding_case(entrances=(0, 0)), hoarding_case(entrances=(0xFFFF, 0xFFFF)), hoarding_case(entrances=(0x7FFF, 0xFF80)),
            hoarding_case(entrances=(0x2080, 0x2000)), hoarding_case(entrances=(0x3F81, 0x4000))]


def hoarding_random(r):
    def ident():
        if r.random() < 0.15:
            return r.getrandbits(16)
        return (r.randrange(0, 127) << 7) + r.randrange(2, 125) + 1

    return hoarding_case(entrances=(ident(), ident()), dialog=r.choice([0, 0x400, r.getrandbits(32)]),
                         extents=tuple(r.randrange(-5, 5) for _ in range(4)), balance=r.getrandbits(8), e0cc=r.getrandbits(31))


# ------------------------------------------------------------------ the pointed thing


def pointed_case(pointed=0, staff=0, under=0, kinds=None, rows=None, f24=1, answer=0):
    return {"pointed": pointed, "staff": staff, "under": under, "kinds": kinds or [0] * 8, "rows": rows or [0] * 8,
            "f24": f24, "queues": {"OverRide": [answer]}}


def ppc_pointed(e, case):
    def prepare(e, c):
        things = [e.alloc(0x40) for _ in range(8)]
        c["_things"] = things
        for t, kind in zip(things, c["kinds"]):
            e.write(t + 2, bytes([kind]))
        for row, v in enumerate(c["rows"]):
            pw(e, G_THINGS + 20 * row, things[v - 1] if v else 0)
        pw(e, G_INTRFACE, things[c["pointed"] - 1] if c["pointed"] else 0)
        pw(e, G_INTRFACE + 4, things[c["staff"] - 1] if c["staff"] else 0)
        pw(e, G_INTRFACE + 8, things[c["under"] - 1] if c["under"] else 0)
        pw16(e, G_INTRFACE + 0x24, c["f24"])

    def ident(c, address):
        v = e.u32(address)
        return 0 if v == 0 else (c["_things"].index(v) + 1 if v in c["_things"] else 99)

    def collect(e, c):
        return {"slots": [ident(c, G_INTRFACE + 4 * i) for i in range(3)]}

    return ppc_run(e, case, POINTED, lambda e, c: [G_INTRFACE], prepare, collect)


def host_pointed(lib, case):
    def prepare(lib, c):
        for i, kind in enumerate(c["kinds"]):
            lib.tpw_pt_set_thing(i, kind)
        for row, v in enumerate(c["rows"]):
            lib.tpw_pt_set_row(row, v)
        lib.tpw_pt_set_interface(c["pointed"], c["staff"], c["under"])
        hset(lib, "interfaceF24", C.c_uint16, c["f24"])

    def collect(lib, c):
        out = (C.c_int32 * 3)()
        lib.tpw_pt_get_interface(out)
        return {"slots": list(out)}

    return host_run(lib, case, 5, lambda c: 0, prepare, collect)


def pointed_edges():
    kinds = [1, 4, 5, 6, 7, 8, 0x12, 2]
    out = []
    for i in range(1, 9):
        out.append(pointed_case(pointed=i, kinds=kinds, staff=7, under=7))
    out.append(pointed_case(pointed=0, kinds=kinds, staff=6, under=6, answer=0))
    out.append(pointed_case(pointed=0, kinds=kinds, rows=[0, 3, 4, 0, 0, 0, 0, 0], answer=1))
    out.append(pointed_case(pointed=0, kinds=kinds, rows=[0, 3, 4, 0, 0, 0, 0, 0], answer=2, f24=0x8001))
    out.append(pointed_case(pointed=0, kinds=kinds, rows=[2, 3, 4, 0, 0, 0, 0, 0], answer=2))
    out.append(pointed_case(pointed=5, kinds=kinds, rows=[0, 3, 4, 0, 0, 0, 0, 0], answer=2, f24=0))
    return out


def pointed_random(r):
    kinds = [r.choice([1, 4, 5, 6, 7, 8, 0x12, 0, 2, 3, 9, 0x13, r.getrandbits(8)]) for _ in range(8)]
    rows = [r.choice([0, 0, r.randrange(1, 9)]) for _ in range(8)]
    return pointed_case(pointed=r.choice([0, 0, r.randrange(1, 9)]), staff=r.choice([0, 7]), under=r.choice([0, 7]), kinds=kinds,
                        rows=rows, f24=r.choice([r.getrandbits(16), r.randrange(0, 300)]),
                        answer=r.choice([0, r.randrange(0, 8)]))


# ------------------------------------------------------------------ the fly zone


def fly_case(values):
    return {"queues": {"Extents": list(values)}, "initial": [1, 2, 3, 4]}


def ppc_fly(e, case):
    def prepare(e, c):
        for a, v in zip((G_FLY_B, G_FLY_C, G_FLY_D, G_FLY_A), c["initial"]):
            pw(e, a, v)

    def collect(e, c):
        return {"zone": [T.s32(e.u32(a)) for a in (G_FLY_B, G_FLY_C, G_FLY_D, G_FLY_A)]}

    return ppc_run(e, case, FLY_ZONE, lambda e, c: [], prepare, collect,
                   fixed=lambda e: {G_FLY_B: 1, G_FLY_C: 2, G_FLY_D: 3, G_FLY_A: 4})


def host_fly(lib, case):
    def prepare(lib, c):
        for name, v in zip(("flyB", "flyC", "flyD", "flyA"), c["initial"]):
            hset(lib, name, C.c_int32, v)

    def collect(lib, c):
        return {"zone": [hget(lib, n, C.c_int32) for n in ("flyB", "flyC", "flyD", "flyA")]}

    return host_run(lib, case, 6, lambda c: 0, prepare, collect,
                    fixed=lambda lib: {1: haddr(lib, "flyB"), 2: haddr(lib, "flyC"), 3: haddr(lib, "flyD"), 4: haddr(lib, "flyA")})


def fly_edges():
    return [fly_case([0, 0, 0, 0]), fly_case([1, 2, 3, 4]), fly_case([-1, -1, -1, -1]), fly_case([126, 127, 0, 5]),
            fly_case([0x7FFFFFFF, -0x80000000, 0x10000000, 0x20000000]), fly_case([-35, 35, 3, -3])]


def fly_random(r):
    return fly_case([r.choice([r.randrange(-10, 140), r.getrandbits(32) - (1 << 31), 0, -1]) for _ in range(4)])


# ------------------------------------------------------------------ the action recorder


def recorder_case(mode=0, state=7, recording=1, rtype=0, turns=0, f08=0x55, f2c=0x66, read=(0,), timing=(0,), turn=1234):
    return {"mode": mode, "state": state, "recording": recording, "rtype": rtype, "turns": turns, "f08": f08, "f2c": f2c,
            "turn": turn, "queues": {"ReadAction": list(read), "Timing": list(timing)}}


def ppc_recorder_state(e, c):
    ppc_world(e, c)
    vtable = e.alloc(16)
    descriptor = e.alloc(8)
    pw(e, descriptor, FILE_CLOSE_CODE, e.toc)
    pw(e, vtable + 0xC, descriptor)
    c["_vtable"] = vtable
    e.write(G_RECORDER, b"\0" * 0x34)
    for off in (0xC, 0x14, 0x1C):
        pw(e, G_RECORDER + off, vtable)
    pw(e, G_RECORDER, c["state"])
    pw(e, G_RECORDER + 4, c["mode"])
    pw(e, G_RECORDER + 8, c["f08"])
    pw(e, G_RECORDER + 0x24, c["recording"])
    pw(e, G_RECORDER + 0x28, c["rtype"])
    pw(e, G_RECORDER + 0x2C, c["f2c"])
    pw(e, G_RECORDER + 0x30, c["turns"])


def ppc_recorder(entry):
    def run(e, case):
        def collect(e, c):
            return {"words": [T.s32(e.u32(G_RECORDER + o)) for o in (0, 4, 8, 0x24, 0x28, 0x2C, 0x30)]}

        return ppc_run(e, case, entry, lambda e, c: [G_RECORDER], ppc_recorder_state, collect,
                       fixed=lambda e: {G_RECORDER + 0xC: 1, G_RECORDER + 0x14: 2, G_RECORDER + 0x1C: 3, G_RECORDER: 4})

    return run


def host_recorder(op):
    def run(lib, case):
        def prepare(lib, c):
            lib.tpw_pt_set_turn(c["turn"])
            hset(lib, "recState", C.c_int32, c["state"])
            hset(lib, "recMode", C.c_int32, c["mode"])
            hset(lib, "recReplaying", C.c_int32, c["recording"])
            hset(lib, "recType", C.c_int32, c["rtype"])
            hset(lib, "recTurns", C.c_int32, c["turns"])
            hset(lib, "recF08", C.c_int32, c["f08"])
            hset(lib, "recF2C", C.c_int32, c["f2c"])

        def collect(lib, c):
            return {"words": [hget(lib, n, C.c_int32) for n in ("recState", "recMode", "recF08", "recReplaying", "recType", "recF2C", "recTurns")]}

        def fixed(lib):
            return {1: haddr(lib, "actionFile"), 2: haddr(lib, "timingFile"), 3: haddr(lib, "cameraFile"), 4: haddr(lib, "recorder")}

        return host_run(lib, case, op, lambda c: 0, prepare, collect, fixed=fixed)

    return run


def reset_edges():
    return [recorder_case(mode=0), recorder_case(mode=1), recorder_case(mode=4), recorder_case(mode=-1, state=0),
            recorder_case(mode=0, state=-5)]


def reset_random(r):
    return recorder_case(mode=r.choice([0, 0, 1, 2, 4, r.getrandbits(32) - (1 << 31)]), state=r.getrandbits(32) - (1 << 31))


def tick_edges():
    out = [recorder_case(recording=0), recorder_case(recording=0, rtype=1), recorder_case(rtype=0), recorder_case(rtype=0, read=(1, 1, 0)),
           recorder_case(rtype=1, turns=3), recorder_case(rtype=2, turns=3), recorder_case(rtype=1, turns=0, read=(1, 0), timing=(5,)),
           recorder_case(rtype=1, turns=0, read=(1, 1, 0), timing=(0, 7)), recorder_case(rtype=2, turns=0, read=(1, 1, 0), timing=(0, 7)),
           recorder_case(rtype=1, turns=0, read=(0,)), recorder_case(rtype=1, turns=0, read=(1, 0), timing=(2,)),
           recorder_case(rtype=1, turns=0, read=(1, 0), timing=(3,)), recorder_case(rtype=1, turns=0, read=(1, 0), timing=(-3,)),
           recorder_case(rtype=1, turns=0x7FFFFFFF), recorder_case(rtype=1, turns=-1), recorder_case(rtype=1, turns=-0x80000000),
           recorder_case(rtype=3), recorder_case(rtype=-1), recorder_case(rtype=0x10000), recorder_case(rtype=1, turns=1),
           recorder_case(rtype=2, turns=0, read=(1, 0), timing=(0x40000000,), turn=-1 & MASK),
           recorder_case(rtype=1, turns=0, read=(1, 0), timing=(0x80000000,))]
    return out


def tick_random(r):
    rtype = r.choice([0, 1, 1, 2, 2, 3, -1, r.getrandbits(32) - (1 << 31)])
    n = r.randrange(0, 5)
    return recorder_case(recording=r.choice([0, 1, 1, 1, 2, -1]), rtype=rtype, turns=r.choice([0, 0, 1, 2, 3, 5, r.getrandbits(32) - (1 << 31)]),
                         mode=r.getrandbits(3), read=[r.randrange(1, 100) for _ in range(n)] + [0],
                         timing=[r.choice([0, 0, 1, 2, 3, 5, 100, r.getrandbits(32) - (1 << 31)]) for _ in range(r.randrange(1, 5))],
                         turn=r.getrandbits(32))


# ------------------------------------------------------------------ the tests


flash_ppc, flash_host = run_mesh(1, TICK_FLASH)
highlight_ppc, highlight_host = run_mesh(2, TICK_HIGHLIGHT)
blend_ppc, blend_host = run_mesh(3, BLEND_FLAGS)

TESTS = [
    FnTest("TickLevelLights", ppc_lights, host_lights, edge=lights_edges, rnd=lights_random),
    FnTest("TickFlashObjects", flash_ppc, flash_host, edge=mesh_edges, rnd=mesh_random),
    FnTest("TickRideHighlights", highlight_ppc, highlight_host, edge=mesh_edges, rnd=mesh_random),
    FnTest("SetBaseBlendFlags", blend_ppc, blend_host, edge=blend_edges, rnd=blend_random),
    FnTest("LLCC_DrawInitialHoarding", ppc_hoarding, host_hoarding, edge=hoarding_edges, rnd=hoarding_random),
    FnTest("CIntrface::DoPointedThingComputations", ppc_pointed, host_pointed, edge=pointed_edges, rnd=pointed_random),
    FnTest("FLY_SetFlyZone", ppc_fly, host_fly, edge=fly_edges, rnd=fly_random),
    FnTest("CActionRecorder::DoTick", ppc_recorder(DO_TICK), host_recorder(7), edge=tick_edges, rnd=tick_random),
    FnTest("CActionRecorder::ResetActionRecording", ppc_recorder(RESET_RECORDING), host_recorder(8), edge=reset_edges, rnd=reset_random),
]
