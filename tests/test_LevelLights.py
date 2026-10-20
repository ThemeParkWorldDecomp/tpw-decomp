"""The park's lights and its second initialisation and teardown (unit w110, main app): CreateGlobalLight with the three CLight
setters it calls (SetColor, SetNormal, SetRadius) and the CMapLinkBase constructor, LevelInit2, LevelDeInit, and the four
CWorld height field accessors LevelInit2 reads.

Every callee outside those functions is a fake on both sides that records the same event (the engine's system object and map,
the record allocator, the error handler, the callees of LevelInit2 and LevelDeInit, translated or not). The functions run as
the original on the PowerPC side and as src/ on the host. The objects the code makes are compared field by field: both sides
fill every new block (the light, the record) with 0xa5, so a field nothing writes must still read 0xa5. The light's layout on
the Mac (the pointer at +0, the vtable words, the shared CMapLinkBase) cannot be seen on the host, where the compiler lays out
CLightParallel and CLightPoint; the PowerPC side checks it against the listing and the host answers 1.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LevelLights"

CREATE, LI2, DEINIT = 0x10063D24, 0x100639B0, 0x10063B80
SETCOLOR, SETNORMAL, SETRADIUS, LINKBASE, CLIGHT_CTOR = 0x1000784C, 0x10007964, 0x100641F8, 0x10007B7C, 0x10007A98
ACCESSORS = {"x": 0x1013E89C, "y": 0x1013E8DC, "w": 0x1013E91C, "h": 0x1013E95C}
OWN = {CREATE, LI2, DEINIT, SETCOLOR, SETNORMAL, SETRADIUS, LINKBASE, CLIGHT_CTOR, *ACCESSORS.values()}

# Addresses of the globals.
BALANCE = 0x102B4568 + 0x544            # gMainBalance's four height field words
WORLD_CELL = 0x1037EBFC                 # gGameWorld
OPTIONS = 0x102B2B48                    # gGameOptions
HEADER = 0x102C6C7C                     # gHeightfieldHeader
HF_WORDS = [0x102C6C70, 0x102C6C6C, 0x102C6C68, 0x102C6C64, 0x102C6C60, 0x102C6C5C]   # x, y, width, height, x origin, y origin
EXTRA = [0x102C6C58, 0x102C6C54]
COUNT_A, COUNT_B, SETS = 0x102D19A8, 0x102D19AC, 0x102C79A4
MAX_SETS = 10240

# The vtable words the original writes into the lights it makes.
VTABLE = {2: 0x10278C60, 3: 0x1029CFB4}
FILL = 0xA5A5A5A5
LIGHT_FLOATS = set(range(0, 19))        # indices of the 21 light words that are floats (the rest: flags, colour word)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def canon(bits):
    """A float's bits with every NaN made one value."""
    f = struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]
    return 0x7FC00000 if math.isnan(f) else bits & 0xFFFFFFFF


def canon_light(words):
    return [canon(w) if i in LIGHT_FLOATS else w & 0xFFFFFFFF for i, w in enumerate(words)]


def rec_pointer_state(word):
    """0 the allocator's fill, 1 a pointer, 2 NULL (the host's PointerState)."""
    return 0 if word == FILL else 2 if word == 0 else 1


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ CreateGlobalLight


def ppc_create(e, c):
    e.reset_scratch()
    map_object = e.alloc(16, b"\xa5" * 16)
    w32(e, e.data_import("gei_MapWho"), map_object)
    system = e.data_import("gei_System")
    diffuse = e.data_import("GlobalDiffuse")
    e.write(diffuse, struct.pack(">4I", *c["diffuse"]))
    position = e.alloc(12, struct.pack(">3I", *c["position"])) if c["position"] is not None else 0
    direction = e.alloc(12, struct.pack(">3I", *c["direction"])) if c["direction"] is not None else 0
    block = e.alloc(0x40, b"\xa5" * 0x40)
    L = CL.PpcLog({map_object: 1, system: 2}, c.get("q"))
    e.cl = L
    state = {"link": None, "news": 0}
    st = CL.Stubs()
    st.real = set(OWN)

    def op_new(e):
        size = e.r(3)
        e.set_r(3, e.alloc(size, b"\xa5" * size))
        state["news"] += 1

    def link(e):
        L.rec("Link", [L.lab(e.r(3)), int(e.r(4) != 0), T.s32(e.r(5))])
        state["link"] = e.r(4)
        e.set_r(3, 1)

    st.imp("__nw__FUl", op_new)
    st.imp("UpdateLights__7CSystemFv", lambda e: L.rec("UpdateLights", [L.lab(e.r(3))]))
    st.imp("Link__7CMapWhoFP12CMapLinkBasei", link)
    st.internal(0x10064BD4, lambda e: (L.rec("MReleaseAllocF", [e.r(3), e.r(4)]), e.set_r(3, block))[1])
    st.internal(0x100B9834, lambda e: (L.rec("DataErrorHandler", [], [e.cstr(e.r(3))]), e.set_r(3, 0))[1])
    saved = st.activate(e)
    out = {}
    try:
        r3 = T.call_ppc(e, CREATE, [c["type"], c["colour"], position, direction])[0]
        words = [0] * 40
        layout = 1
        words[7] = state["news"]
        words[6] = 2
        if r3 != 0:
            u = e.u32
            words[0] = 1
            words[1] = u(r3)
            words[2] = u(r3 + 0x10)
            words[3] = rec_pointer_state(u(r3 + 4))
            words[4] = rec_pointer_state(u(r3 + 0xC))
            words[5] = rec_pointer_state(u(r3 + 8))
            holder = u(r3 + 4) if words[3] == 1 else u(r3 + 0xC) if words[4] == 1 else 0
            if holder != 0:
                light = holder + 0x10
                kind = u(r3)
                vtable = VTABLE[kind]
                layout = int(u(holder) == light and u(holder + 4) == vtable and u(holder + 8) == vtable + 0xC and
                             u(holder + 0xC) == 0 and u(light) == holder + 8 and u(light + 0x5C) == vtable + 0x24 and
                             u(light + 0x60) == 0)
                words[8:29] = struct.unpack(">21I", e.read(light + 4, 21 * 4))
                if state["link"] is not None:
                    words[6] = int(state["link"] == holder + 8)
        words[29:33] = struct.unpack(">4I", e.read(diffuse, 16))
        out["words"] = canon_light(words[:29]) + [canon(w) for w in words[29:33]]
        out["layout"] = layout
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def host_create(lib, c):
    lib.tpw_ll_prepare()
    CL.host_begin(lib, {}, c.get("q"))
    lib.tpw_ll_prepare()
    lib.tpw_ll_create.restype = None
    lib.tpw_ll_create.argtypes = [C.c_int32, C.c_uint32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
                                  C.POINTER(C.c_uint32)]
    position = (C.c_uint32 * 3)(*c["position"]) if c["position"] is not None else None
    direction = (C.c_uint32 * 3)(*c["direction"]) if c["direction"] is not None else None
    diffuse = (C.c_uint32 * 4)(*c["diffuse"])
    words = (C.c_uint32 * 40)()
    try:
        lib.tpw_ll_create(c["type"], c["colour"], position, direction, diffuse, words)
    finally:
        log = CL.host_end(lib)
    words = list(words)
    return {"words": canon_light(words[:29]) + [canon(w) for w in words[29:33]], "layout": 1, "log": log}


# ------------------------------------------------------------------ the setters and the constructor


def ppc_setter(e, c):
    e.reset_scratch()
    system = e.data_import("gei_System")
    L = CL.PpcLog({system: 2})
    e.cl = L
    st = CL.Stubs()
    st.real = set(OWN)
    st.imp("UpdateLights__7CSystemFv", lambda e: L.rec("UpdateLights", [L.lab(e.r(3))]))
    saved = st.activate(e)
    block = e.alloc(0x80, b"\xa5" * 0x80)
    try:
        a, b, cc, d = [T.F(T.bits_f32(x)) for x in c["args"]]
        entry = [SETCOLOR, SETNORMAL, SETRADIUS][c["op"]]
        args = [[block, a, b, cc, d], [block, a, b, cc], [block, a]][c["op"]]
        T.call_ppc(e, entry, args)
        words = struct.unpack(">21I", e.read(block + 4, 21 * 4))
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"words": canon_light(words), "log": L.log}


def host_setter(lib, c):
    lib.tpw_ll_prepare()
    CL.host_begin(lib, {})
    lib.tpw_ll_prepare()
    lib.tpw_ll_setter.restype = None
    lib.tpw_ll_setter.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]
    words = (C.c_uint32 * 21)()
    try:
        lib.tpw_ll_setter(c["op"], *c["args"], words)
    finally:
        log = CL.host_end(lib)
    return {"words": canon_light(list(words)), "log": log}


def ppc_linkbase(e, c):
    e.reset_scratch()
    block = e.alloc(64, b"\xa5" * 64)
    T.call_ppc(e, LINKBASE, [block])
    data = e.read(block, 64)
    return {"vtable_only": int(struct.unpack(">I", data[:4])[0] == 0x10278C9C and data[4:] == b"\xa5" * 60), "tail": 1}


def host_linkbase(lib, c):
    lib.tpw_ll_linkbase.restype = None
    out = (C.c_uint32 * 2)()
    lib.tpw_ll_linkbase(out)
    return {"vtable_only": int(out[0] == 1 and out[1] == 1), "tail": 1}


# ------------------------------------------------------------------ LevelInit2


def ppc_li2(e, c):
    e.reset_scratch()
    L = CL.PpcLog({}, c.get("q"))
    e.cl = L
    st = CL.Stubs()
    st.real = set(OWN)
    plain = {"LoadLevelAttributeMap": 0x1005BE44, "DC_SetupDynamicCellMeshes": 0x100DF8FC, "SetupForPolySearch": 0x1005C7E8,
             "DestroyHighlightColourBuffers": 0x1006D26C, "DestroyScratchGeomBuffer": 0x10068D90,
             "CreateHighlightColourBuffers": 0x1006D1D0, "SetupHeightmapSystem": 0x1005F6F4,
             "InitialiseCoasterTypes": 0x10042478, "SCR_InitScriptSystem": 0x100D8D94}
    for name, address in plain.items():
        st.internal(address, CL.recorder(name))
    st.internal(0x10068D14, CL.recorder("CreateScratchGeomBuffer", "i"))
    st.internal(0x10090B68, CL.recorder("LLCC_SystemInit", "ii"))
    st.imp("PT_SetRange__Fiiii", CL.recorder("PT_SetRange", "iiii"))
    saved = st.activate(e)
    world = e.alloc(16, b"\xa5" * 16)
    w32(e, WORLD_CELL, world)
    for i, v in enumerate(c["balance"]):
        w32(e, BALANCE + 4 * i, v)
    w32(e, OPTIONS, c["options"])
    for i, v in enumerate(c["extra"]):
        w32(e, EXTRA[i], v)
    for address, v in zip(HF_WORDS, c["globals"]):
        w32(e, address, v)
    header = 0
    if c["mode"] >= 1:
        header = e.alloc(0x100, b"\0" * 0x100)
        if c["mode"] >= 2:
            field = e.alloc(0x30, b"\0" * 0x30)
            w32(e, field + 0x10, c["scale"][0])
            w32(e, field + 0x14, c["scale"][1])
            w32(e, header + 0x6C, field)
    w32(e, HEADER, header)
    out = {}
    try:
        out["result"] = T.s32(T.call_ppc(e, LI2, [])[0])
        words = [e.u32(a) for a in HF_WORDS] + [e.u32(a) for a in EXTRA]
        out["words"] = [w if i < 4 or i >= 6 else canon(w) for i, w in enumerate(words)]
        out["options"] = e.u32(OPTIONS)
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def host_li2(lib, c):
    CL.host_begin(lib, {}, c.get("q"))
    lib.tpw_ll_li2_set.restype = None
    lib.tpw_ll_li2_set.argtypes = [C.POINTER(C.c_int32), C.c_uint32, C.POINTER(C.c_int32), C.POINTER(C.c_uint32), C.c_int32,
                                   C.c_uint32, C.c_uint32]
    lib.tpw_ll_li2_run.restype = None
    lib.tpw_ll_li2_run.argtypes = [C.POINTER(C.c_uint32)]
    balance = (C.c_int32 * 4)(*[T.s32(v) for v in c["balance"]])
    extra = (C.c_int32 * 2)(*[T.s32(v) for v in c["extra"]])
    globals_ = (C.c_uint32 * 6)(*c["globals"])
    lib.tpw_ll_li2_set(balance, c["options"], extra, globals_, c["mode"], c["scale"][0], c["scale"][1])
    words = (C.c_uint32 * 10)()
    try:
        lib.tpw_ll_li2_run(words)
    finally:
        log = CL.host_end(lib)
    words = list(words)
    # host order: result, x, y, width, height, x origin, y origin, extra x, extra y, options
    return {"result": T.s32(words[0]), "words": [w if i < 4 or i >= 6 else canon(w) for i, w in enumerate(words[1:9])],
            "options": words[9], "log": log}


# ------------------------------------------------------------------ LevelDeInit


def ppc_deinit(e, c):
    e.reset_scratch()
    L = CL.PpcLog({}, c.get("q"))
    e.cl = L
    st = CL.Stubs()
    st.real = set(OWN)
    plain = {"DC_FreeDynamicCellMeshes": 0x100E01AC, "ADV_Close": 0x10007FC4, "ClosedownRideHoardings": 0x10061448,
             "DestroyCoasterInstances": 0x10043994, "DestroyCoasterTypes": 0x1003ACF0, "DestroyLevelLights": 0x1006446C,
             "SetdownFromPolySearch": 0x1005C850, "LLCC_SystemDeInit": 0x10090CDC, "ESP_RemoveSprites": 0x1007C790,
             "DestroyLevelHeightfield": 0x1005C0FC, "DestroyAllRideTypes": 0x1006A9F0, "DestroyPathAllocs": 0x100B8DF4,
             "DestroyScratchGeomBuffer": 0x10068D90, "DestroyHighlightColourBuffers": 0x1006D26C}
    for name, address in plain.items():
        st.internal(address, CL.recorder(name))
    st.internal(0x10031F38, CL.recorder("DestroyAdvisor", "u"))

    def destroy_ride(e):
        L.rec("DestroyRideInstance", [e.r(3)])
        w32(e, COUNT_A, max(0, e.u32(COUNT_A) + L.ret("RideCountDelta", 0)))

    st.internal(0x1006A3EC, destroy_ride)
    saved = st.activate(e)
    w32(e, COUNT_A, c["count_a"])
    w32(e, COUNT_B, c["count_b"])
    e.write(SETS, b"\0" * (4 * MAX_SETS))
    for i, present in enumerate(c["sets"]):
        if present:
            w32(e, SETS + 4 * i, 0x6A5A0000 + 16 * i)
    out = {}
    try:
        T.call_ppc(e, DEINIT, [c["keep"]])
        out["counts"] = [e.u32(COUNT_A), e.u32(COUNT_B)]
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def host_deinit(lib, c):
    CL.host_begin(lib, {}, c.get("q"))
    lib.tpw_ll_di_set.restype = None
    lib.tpw_ll_di_set.argtypes = [C.c_uint32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint8)]
    lib.tpw_ll_di_run.restype = None
    lib.tpw_ll_di_run.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
    present = (C.c_uint8 * len(c["sets"]))(*[1 if p else 0 for p in c["sets"]])
    lib.tpw_ll_di_set(c["count_a"], c["count_b"], len(c["sets"]), present)
    counts = (C.c_uint32 * 2)()
    try:
        lib.tpw_ll_di_run(c["keep"], counts)
    finally:
        log = CL.host_end(lib)
    return {"counts": list(counts), "log": log}


# ------------------------------------------------------------------ the accessors


def ppc_accessor(e, c):
    e.reset_scratch()
    world = e.alloc(16, b"\xa5" * 16)
    for i, v in enumerate(c["balance"]):
        w32(e, BALANCE + 4 * i, v)
    return {"value": T.s32(T.call_ppc(e, ACCESSORS[c["which"]], [world])[0])}


def host_accessor(lib, c):
    lib.tpw_ll_accessors.restype = None
    lib.tpw_ll_accessors.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    balance = (C.c_int32 * 4)(*[T.s32(v) for v in c["balance"]])
    out = (C.c_int32 * 4)()
    lib.tpw_ll_accessors(balance, out)
    return {"value": out["xywh".index(c["which"])]}


# ------------------------------------------------------------------ cases

COLOURS = [0, 0xFFFFFFFF, 0x80402010, 0x01020304, 0xFF000000, 0x000000FF, 0x00FF0000, 0x0000FF00, 0x7F7F7F7F, 0x80808080,
           0xFEFEFEFE, 0x12345678, 0xDEADBEEF, 0xFF00FF00, 0x00FF00FF]
TYPES = [-1, 0, 1, 2, 3, 4, 7, -2147483648, 2147483647, -100, 5]
FLOAT_BITS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x42C80000, 0x437F0000, 0x3F7FBE77, 0x3FC00000, 0xBF000000,
              0x40000000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF, 0x00800000, 0x00000001, 0x1E3CE508, 0x3F000000,
              0x3B808081, 0x3C008081, 0x3F333333, 0x3F32F1AA, 0x4F000000, 0xCF000000, 0x5F000000, 0x42FE0000, 0x437E0000,
              0x7FA00000]


def fbits(x):
    return T.f32_bits(T.f32(x))


def rnd_float_bits(r, snan=True):
    k = r.random()
    if k < 0.4:
        bits = r.choice(FLOAT_BITS)
    elif k < 0.7:
        bits = fbits(r.uniform(-1.5, 2.0))
    elif k < 0.85:
        bits = fbits(r.uniform(-1000.0, 1000.0))
    else:
        bits = r.getrandbits(32)
    return bits


def rnd_vector(r):
    return tuple(rnd_float_bits(r) for _ in range(3))


def rnd_colour(r):
    return r.choice(COLOURS) if r.random() < 0.5 else r.getrandbits(32)


def create_case(r, kind, colour, vectors=True):
    c = {"type": kind, "colour": colour, "diffuse": tuple(r.getrandbits(32) for _ in range(4)),
         "position": rnd_vector(r) if vectors or r.random() < 0.5 else None,
         "direction": rnd_vector(r) if vectors or r.random() < 0.5 else None}
    if kind in (0, 1, 4, 5, 7, -1) and r.random() < 0.5:
        c["position"] = None
    if kind in (0, 1, 4, 5, 7, -1) and r.random() < 0.5:
        c["direction"] = None
    if kind == 2:
        c["direction"] = rnd_vector(r)
    if kind == 3:
        c["position"] = rnd_vector(r)
    return c


def create_edges():
    r = __import__("random").Random(11)
    for kind in TYPES:
        for colour in COLOURS[:6]:
            yield create_case(r, kind, colour)
    for colour in COLOURS:
        yield create_case(r, 2, colour)
        yield create_case(r, 3, colour)
        yield create_case(r, 1, colour)
    for bits in FLOAT_BITS:
        c = create_case(r, 2, 0x80402010)
        c["direction"] = (bits, bits ^ 0x80000000, bits)
        yield c
        c = create_case(r, 3, 0x01020304)
        c["position"] = (bits, bits ^ 0x80000000, bits)
        yield c


def create_random(r):
    kind = r.choice([2, 2, 3, 3, 1, 0, -1, 4, r.randint(-3, 9), r.getrandbits(32) - (1 << 31)])
    return create_case(r, kind, rnd_colour(r), vectors=False)


# QEMU's fctiwz answers 0x80000000 for a product above INT32_MAX where the hardware (and ppc_fctiwz) answers 0x7fffffff, so SetColor
# is not given a component whose product with 255 passes 2^31: such a component becomes the largest one whose product stays in range.
NEAR_LIMIT = 0x4B000000      # 8388608.0, times 255 = 2139095040


def in_fctiwz_range(bits):
    f = struct.unpack("<f", struct.pack("<I", bits))[0]
    return not (f > 0 and f * 255.0 >= 2147483648.0)


def setter_case(r, op, args=None):
    if args is None:
        args = [rnd_float_bits(r) for _ in range(4)]
    else:
        args = list(args) + [0] * (4 - len(args))
    if op == 0:
        args = [bits if in_fctiwz_range(bits) else NEAR_LIMIT for bits in args]
    return {"op": op, "args": tuple(args)}


COLOUR_VALUES = [0.0, 1.0, 0.999, 1.5, -0.5, 2.0, 0.5, 0.7, 0.002, 0.004, 1.0 / 255, 254.5 / 255, 255.0 / 256, 0.3, 0.1,
                 -0.001, 1e10, -1e10, 128.5 / 255, 129.0 / 255, 0.9999999, 256.0 / 255, 2.0 / 255 + 1e-7]


def setcolor_edges():
    r = __import__("random").Random(12)
    for a in COLOUR_VALUES:
        yield setter_case(r, 0, [fbits(a), fbits(1.0 - a), fbits(a * 0.5), fbits(a * 2.0)])
        yield setter_case(r, 0, [fbits(a)] * 4)
    for bits in FLOAT_BITS:
        yield setter_case(r, 0, [bits, 0x3F800000, bits, 0x3F000000])
        yield setter_case(r, 0, [0x3F800000, bits, 0x3F000000, bits])


def setcolor_random(r):
    if r.random() < 0.5:
        return setter_case(r, 0, [fbits(r.choice(COLOUR_VALUES) + r.uniform(-0.01, 0.01)) for _ in range(4)])
    return setter_case(r, 0)


def setnormal_edges():
    r = __import__("random").Random(13)
    for bits in FLOAT_BITS:
        yield setter_case(r, 1, [bits, 0x3F800000, 0xBF800000])
        yield setter_case(r, 1, [0, bits, 0x80000000])
        yield setter_case(r, 1, [0x3F800000, 0, bits])


def setnormal_random(r):
    return setter_case(r, 1)


def setradius_edges():
    r = __import__("random").Random(14)
    for bits in FLOAT_BITS + [fbits(1e-20), fbits(1e-10), fbits(1e20), fbits(3e19), fbits(0.1), fbits(100.0), fbits(-100.0),
                              fbits(1e-23), fbits(1.5e-23), 0x20000000, 0x1F800000, 0x5F800000, 0x5F7FFFFF, 0x1F7FFFFF]:
        yield setter_case(r, 2, [bits])


def setradius_random(r):
    k = r.random()
    if k < 0.3:
        return setter_case(r, 2, [fbits(r.uniform(-200.0, 200.0))])
    if k < 0.6:
        return setter_case(r, 2, [fbits(10.0 ** r.uniform(-30, 30))])
    return setter_case(r, 2)


INTS = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 5, 100, 128, 256, 0x1000, 0x10000, -50, 1000]


def rnd_int(r):
    return T.u32(r.choice(INTS) if r.random() < 0.5 else r.randint(-100000, 100000) if r.random() < 0.5 else r.getrandbits(32))


def li2_case(r, mode=None, options=None, balance=None):
    return {"balance": tuple(balance if balance is not None else [rnd_int(r) for _ in range(4)]),
            "options": options if options is not None else r.choice([0, 0x2000, 0xFFFFFFFF, 0x2000 | r.getrandbits(32),
                                                                      r.getrandbits(32) & ~0x2000, 0x4000, 0x1FFF]),
            "extra": tuple(rnd_int(r) for _ in range(2)) if r.random() < 0.7 else (0, 0),
            "globals": tuple(r.getrandbits(32) for _ in range(6)),
            "mode": mode if mode is not None else r.choice([0, 1, 2, 2, 2]),
            "scale": (rnd_float_bits(r), rnd_float_bits(r)),
            "q": {}}


def li2_edges():
    r = __import__("random").Random(15)
    for mode in (0, 1, 2):
        for options in (0, 0x2000, 0x1FFF, 0x2001, 0xFFFFFFFF, 0xFFFFDFFF):
            yield li2_case(r, mode, options)
    for balance in ([0, 0, 0, 0], [1, 2, 3, 4], [-1, -1, -1, -1], [0x7FFFFFFF, 0x7FFFFFFF, 1, 1], [100, 200, 300, 400],
                    [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x80000001], [-16777217, 16777217, 33554435, -3]):
        yield li2_case(r, 2, 0x2000, balance)
        yield li2_case(r, 2, 0, balance)


def li2_random(r):
    return li2_case(r)


def deinit_case(r, keep=None, n=None):
    n = n if n is not None else r.choice([0, 1, 2, 3, 5, 8, 12])
    sets = [r.random() < 0.6 for _ in range(n + 1)]
    sets[0] = r.random() < 0.3                 # index 0 is never looked at
    count_a = r.randint(0, 3)
    count_b = max(0, n - count_a)
    if r.random() < 0.3:
        count_b = r.randint(0, 6)
    deltas = [r.choice([0, 0, 0, -1, -2, 1]) for _ in range(6)]
    return {"keep": keep if keep is not None else r.randint(0, 1), "count_a": count_a, "count_b": count_b,
            "sets": sets + [False] * 20, "q": {"RideCountDelta": deltas}}


def deinit_edges():
    r = __import__("random").Random(16)
    for keep in (0, 1):
        for n in (0, 1, 2, 5):
            yield deinit_case(r, keep, n)
    yield {"keep": 0, "count_a": 0, "count_b": 0, "sets": [True] * 30, "q": {}}
    yield {"keep": 1, "count_a": 2, "count_b": 3, "sets": [True] * 30, "q": {"RideCountDelta": [-1, -1, -1]}}
    yield {"keep": 0, "count_a": 3, "count_b": 2, "sets": [False, True, False, True, True] + [False] * 25,
           "q": {"RideCountDelta": [-1, -1]}}
    yield {"keep": 0, "count_a": 1, "count_b": 4, "sets": [False] + [True] * 29, "q": {"RideCountDelta": [-1, 2]}}


def deinit_random(r):
    return deinit_case(r)


def accessor_case(which):
    def edge():
        r = __import__("random").Random(17)
        for balance in ([0, 0, 0, 0], [1, 2, 3, 4], [-1, -2, -3, -4], [0x7FFFFFFF, 0x80000000, 5, 6]):
            yield {"which": which, "balance": tuple(T.u32(v) for v in balance)}

    def rnd(r):
        return {"which": which, "balance": tuple(rnd_int(r) for _ in range(4))}
    return edge, rnd


def accessor_test(name, which):
    edge, rnd = accessor_case(which)
    return FnTest(name, ppc_accessor, host_accessor, edge=edge, rnd=rnd)


TESTS = [
    FnTest("CreateGlobalLight", ppc_create, host_create, edge=create_edges, rnd=create_random),
    FnTest("CLight::SetColor", ppc_setter, host_setter, edge=setcolor_edges, rnd=setcolor_random),
    FnTest("CLight::SetNormal", ppc_setter, host_setter, edge=setnormal_edges, rnd=setnormal_random),
    FnTest("CLight::SetRadius", ppc_setter, host_setter, edge=setradius_edges, rnd=setradius_random),
    FnTest("CMapLinkBase::CMapLinkBase", ppc_linkbase, host_linkbase, edge=lambda: [{}], rnd=lambda r: {}),
    FnTest("LevelInit2", ppc_li2, host_li2, edge=li2_edges, rnd=li2_random),
    FnTest("LevelDeInit", ppc_deinit, host_deinit, edge=deinit_edges, rnd=deinit_random),
    accessor_test("CWorld::GetHeightfieldXStart", "x"),
    accessor_test("CWorld::GetHeightfieldYStart", "y"),
    accessor_test("CWorld::GetHeightfieldWidth", "w"),
    accessor_test("CWorld::GetHeightfieldHeight", "h"),
]
