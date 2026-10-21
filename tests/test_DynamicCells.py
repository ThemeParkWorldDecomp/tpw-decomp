"""Unit w120, the dynamic cell meshes in the frame, against libtpw_unit_DynamicCells.dylib (tests/unit_DynamicCells.cpp), main app
("SimTheme Park"):
  DC_TickDynamicCellMeshes 0x100dfd88 and CDynamicMesh::DrawAll 0x100e1a04, DC_CreatePolyFace 0x100e2778 (src/game/park/DynamicCell.cpp),
  LLCCM_CreateQMesh 0x10081c58, LLCCM_CreateKTrackMesh 0x10081fb8, LLCCM_CreateWTrackMesh 0x10082350, LLCCM_CreateHoardingMesh
  0x10082720 (src/game/park/LLCellMouse.cpp).

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses and layouts
(big-endian, 32-bit pointers) for the original, and in the host library's memory (little-endian, native pointers) for the translation; the
same code reads both back. The seven functions call each other for real on both sides (the tick runs DrawAll on three mesh objects, DrawAll
runs DC_CreatePolyFace). Every callee outside the unit is a fake that records the same event on both sides (the clock's pause flag and
rate, the two mesh fakes, which advance the mesh's counters by the case's amounts, the rotate, animate, draw, reset and sort calls, the
weather lookup, the ride instancers and CTRACE). The fakes that may write do so from the case's queues: the rotate fake rewrites rows 5 to 8
of the texture table and may move the two scroll lags, the first water draw may move the water's start position, sort may replace the
keys. Floats are recorded as bit patterns with every NaN made one value.

What is compared after a call: the events in order, the answer (the instancers), every global the functions touch (the 30 texture floats, the
lags, the counters, the phases, the blink flag, the water's start position, the first keys), and for each mesh object its fields, its 24 face
sets (20 are cleared, the 4 beyond must stay) and its face buffer, half word by half word. Pointers are compared as labels, or as an offset in
a registered region.

The fused multiply-adds and the fctiwz of the tick get the processor's rounding through test_DynamicMesh's HardwareRounding (Unicorn rounds
fused operations twice and saturates fctiwz of large values to the wrong sign), aimed at the tick's range. Not tested: a NULL mesh pointer in
the tick (the Mac writes to low memory), texture numbers of 20 and above in a record (the Mac writes past the 20 face sets), mesh table
indices past the table, instance numbers past the ride array.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_DynamicCells"
MASK = 0xFFFFFFFF
INT_MIN = -(1 << 31)
FACES = 512           # sFaces in each mesh's face buffer
SETS = 24             # face sets the test allocates (the function clears 20)

TICK, DRAWALL, POLY = 0x100DFD88, 0x100E1A04, 0x100E2778
INSTANCERS = [0x10081C58, 0x10081FB8, 0x10082350, 0x10082720]
OWN = {TICK, DRAWALL, POLY, *INSTANCERS}
PAUSE, RATE, SQUARE, VERTEX = 0x101474B8, 0x10168A00, 0x100E210C, 0x100E1F10
ROTATE, ANIM_WATER, DRAW_WATER, ANIM_FLAG, DRAW_FLAG = 0x100E0544, 0x100E10F4, 0x100E060C, 0x100E1350, 0x100E087C
RESET, SORT, WEATHER = 0x100E0260, 0x100DFB68, 0x1013ED74
INSTANCE, RESTORE, SETANIM, CTRACE = 0x10069B24, 0x1006A140, 0x100C6670, 0x1006FCBC

# The Mac's addresses of the globals; the host library's addresses come from tpw_dc_addr(name).
ADDR = dict(uv=0x102F9B48, bc0=0x102F9BC0, bc4=0x102F9BC4, db50=0x102DDB50, db54=0x102DDB54, d94=0x102F9D94, d98=0x102F9D98,
            d9c=0x102F9D9C, da0=0x102F9DA0, fbdbc=0x102FBDBC, flag=0x102A031C, bp=0x1034C188, water=0x1034C184, flags=0x1034C180,
            ws=0x102F9BC8, cam=0x102B5BF4, clock=0x1037F048, world=0x1037EBFC, ridesets=0x102C79A4, qmesh=0x1029DB48,
            kmesh=0x1029DBA8, wmesh=0x1029DC98, hmesh=0x1029DDC4, qsize=0x102DCEA8, tsize=0x102DC220, faces=0x102FBDC0,
            keys=0x102F9DA4, spots=0x102A0320)
TABLES = [("qmesh", 8), ("kmesh", 20), ("wmesh", 25), ("hmesh", 28)]
# The scalar globals: name -> (kind, words). Every case writes all of them and reads all of them back.
GLOBALS = dict(uv=("f", 30), bc0=("f", 1), bc4=("f", 1), db50=("w", 1), db54=("w", 1), d94=("w", 1), d98=("f", 1), d9c=("f", 1),
               da0=("f", 1), fbdbc=("w", 1), flag=("b", 1), ws=("f", 3), cam=("f", 1))
KEY_WORDS = 48        # keys written and read back in every case

# Labels: objects below 200, regions from 201 (a pointer into a region is told as (label << 32) + its offset).
L_CLOCK, L_WORLD, L_WEATHER, L_MESH, L_SET = 2, 3, 40, 10, 60
R_KEYS, R_UV, R_BUFFER, R_QSIZE, R_TSIZE, R_FACES = 201, 202, 203, 204, 205, 210


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def dbits(x):
    """A double's 64-bit pattern as a signed number (the call log's queues hold int64)."""
    return struct.unpack(">q", struct.pack(">d", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def signed64(v):
    v &= (1 << 64) - 1
    return v - (1 << 64) if v >= (1 << 63) else v


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class TickRounding(HardwareRounding):
    """HardwareRounding aimed at the tick: two fmsubs, four fmadds and one fctiwz."""

    def __enter__(self):
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=TICK, end=TICK + 1012 - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fbits_reg(e, n):
    return canon_f(CL.fbits(e.f(n)))


def stack(e, n):
    """The n-th stack argument word (the caller's 0x38(r1) and up; the stub runs at the callee's entry)."""
    return e.u32(e.r(1) + 0x38 + 4 * n)


def be_float(e, address, bits):
    e.write(address, struct.pack(">I", bits & MASK))


def stub_pause(e):
    e.cl.rec("GetPauseFlag", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("pause", 0) & MASK)


def stub_rate(e):
    e.cl.rec("GetTimeRate", (e.cl.lab(e.r(3)),))
    bits = e.cl.ret("rate", 0) & ((1 << 64) - 1)
    e.set_f(1, struct.unpack(">d", struct.pack(">Q", bits))[0])


def stub_square(e):
    L, d = e.cl, e.dc
    obj = e.r(3)
    L.rec("DC_CreateSquareFace", (L.lab(obj), fbits_reg(e, 1), fbits_reg(e, 2), fbits_reg(e, 3), e.r(7), e.r(8), s32(e.r(9)),
                                  fbits_reg(e, 4), s32(stack(e, 0)), stack(e, 1), s32(stack(e, 2))))
    vertices, count, cursor = e.u32(obj + 0x9C), e.u32(obj + 0x98), e.u32(obj + 0x94)
    L.rec("SquareState", (vertices, count, d.tag(cursor)))
    add_vertices = L.ret("sqv", 4)
    add_faces = L.ret("sqf", 2)
    add_cursor = L.ret("sqc", 2)
    e.write(obj + 0x9C, struct.pack(">I", (vertices + add_vertices) & MASK))
    e.write(obj + 0x98, struct.pack(">I", (count + add_faces) & MASK))
    e.write(obj + 0x94, struct.pack(">I", (cursor + 8 * add_cursor) & MASK))


def stub_vertex(e):
    L, d = e.cl, e.dc
    obj = e.r(3)
    vertices = e.u32(obj + 0x9C)
    L.rec("DC_SetVertex", (L.lab(obj), fbits_reg(e, 1), fbits_reg(e, 2), fbits_reg(e, 3), fbits_reg(e, 4), fbits_reg(e, 5), s32(e.r(9)),
                           d.tag(e.r(10)), vertices))
    e.write(obj + 0x9C, struct.pack(">I", (vertices + L.ret("vtx", 1)) & MASK))


def stub_rotate(e):
    L, d = e.cl, e.dc
    points = e.r(3)
    L.rec("HDZ_RotatePointZ", (d.tag(points), fbits_reg(e, 1)))
    for k in range(8):
        bits = L.ret("rot", -1)
        if bits != -1:
            be_float(e, points + 12 * (k // 2) + 4 * (k % 2), bits)
    lag_z = L.ret("rotbc0", -1)
    if lag_z != -1:
        be_float(e, ADDR["bc0"], lag_z)
    lag_x = L.ret("rotbc4", -1)
    if lag_x != -1:
        be_float(e, ADDR["bc4"], lag_x)


def stub_anim(name):
    return lambda e: e.cl.rec(name, (s32(e.r(3)), s32(e.r(4)), fbits_reg(e, 1), fbits_reg(e, 2)))


def stub_draw_water(e):
    L = e.cl
    L.rec("DC_DrawWater", (fbits_reg(e, 1), fbits_reg(e, 2), fbits_reg(e, 3), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), s32(e.r(9)), s32(e.r(10)),
                           s32(stack(e, 0))))
    for k, name in enumerate(("wsx", "wsy", "wsz")):
        bits = L.ret(name, -1)
        if bits != -1:
            be_float(e, ADDR["ws"] + 4 * k, bits)


def stub_draw_flag(e):
    e.cl.rec("DC_DrawFlag", (fbits_reg(e, 1), fbits_reg(e, 2), fbits_reg(e, 3), fbits_reg(e, 4), s32(e.r(7)), s32(e.r(8)), s32(e.r(9)),
                             fbits_reg(e, 5)))


def stub_reset(e):
    e.cl.rec("DC_ResetSquareFaceListPtr", (e.s32(ADDR["fbdbc"]),))


def stub_sort(e):
    L = e.cl
    keys = e.r(3)
    L.rec("sort", (e.dc.tag(keys), e.r(4)))
    n = 1
    while True:
        word = L.ret("sortk", -1)
        if word == -1:
            break
        e.write(keys + 4 * n, struct.pack(">I", word & MASK))
        n += 1


def stub_weather(e):
    e.cl.rec("GetWeather", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.dc.weather[e.cl.ret("weather", 0) & 7])


def record_params(e, address):
    x, z, rotation, size = struct.unpack(">iiiI", e.read(address, 16))
    e.cl.rec("RideParams", (x, z, rotation, e.dc.tag(size)))


def stub_instance(e):
    L, d = e.cl, e.dc
    L.rec("InstanceRide", (e.r(3), e.r(4), e.r(6), s32(e.r(7)), d.tag(e.r(8)), s32(e.r(9)), s32(e.r(10)), stack(e, 0), s32(stack(e, 1))))
    record_params(e, e.r(5))
    e.set_r(3, L.ret("inst", 1) & MASK)


def stub_restore(e):
    L, d = e.cl, e.dc
    L.rec("RestoreRideInstance", (e.r(3), e.r(4), e.r(5), e.r(7), d.tag(e.r(8)), s32(e.r(9)), s32(e.r(10)), s32(stack(e, 0))))
    record_params(e, e.r(6))
    e.set_r(3, L.ret("restore", 0) & MASK)


def stub_set_anim(e):
    L = e.cl
    L.rec("SetCurRideAnim", (L.lab(e.r(3)), e.r(4), e.r(5), e.r(6), fbits_reg(e, 1), e.r(8)))


def stub_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3),), (e.cstr(e.r(4))[:159],))


STUBS.internal(PAUSE, stub_pause)
STUBS.internal(RATE, stub_rate)
STUBS.internal(SQUARE, stub_square)
STUBS.internal(VERTEX, stub_vertex)
STUBS.internal(ROTATE, stub_rotate)
STUBS.internal(ANIM_WATER, stub_anim("DC_AnimateWater"))
STUBS.internal(DRAW_WATER, stub_draw_water)
STUBS.internal(ANIM_FLAG, stub_anim("DC_AnimateFlag"))
STUBS.internal(DRAW_FLAG, stub_draw_flag)
STUBS.internal(RESET, stub_reset)
STUBS.internal(SORT, stub_sort)
STUBS.internal(WEATHER, stub_weather)
STUBS.internal(INSTANCE, stub_instance)
STUBS.internal(RESTORE, stub_restore)
STUBS.internal(SETANIM, stub_set_anim)
STUBS.internal(CTRACE, stub_ctrace)

# ---------------------------------------------------------------------------------------------------------------- the two sides

MAC_OBJ = dict(type=("w", 0x00), nface=("h", 0x28), nvert=("h", 0x2A), nuv=("h", 0x2C), box=("f", 0x34), facesets=("p", 0x50),
               faces=("p", 0x70), cursor=("p", 0x94), nfaces=("w", 0x98), nverts=("w", 0x9C), maxsq=("w", 0xA0))
MAC_OBJ_SIZE = 0xA4
LAYOUT_ORDER = ["type", "nface", "nvert", "nuv", "box", "facesets", "faces", "cursor", "nfaces", "nverts", "maxsq"]
WIDTH = dict(b=1, h=2, w=4, f=4)


class Side:
    """What the shared code needs from either memory."""

    def wr(self, a, kind, v):
        n = self.PTR if kind == "p" else WIDTH[kind]
        self.write(a, (v & ((1 << (8 * n)) - 1)).to_bytes(n, self.order))

    def rd(self, a, kind):
        n = self.PTR if kind == "p" else WIDTH[kind]
        return int.from_bytes(self.read(a, n), self.order)

    def rd_f(self, a):
        return canon_f(self.rd(a, "f"))

    def put(self, obj, name, v, index=0):
        kind, off = self.OBJ[name]
        self.wr(obj + off + index * 4, kind, v)

    def get(self, obj, name, index=0):
        kind, off = self.OBJ[name]
        return self.rd(obj + off + index * 4, kind)

    def tag(self, p):
        if p == 0:
            return 0
        for label, start, size, below in self.regions:
            if start - below <= p < start + size:
                return (label << 32) + (p - (start - below))
        return self.lab(p)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's scratch area and image."""
    ppc = True
    PTR = 4
    order = "big"
    OBJ = MAC_OBJ
    OBJ_SIZE = MAC_OBJ_SIZE
    TABLE_STRIDE, TABLE_TYPE, TABLE_CONTROLS = 12, 0, 8

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.regions = []
        self.weather = [0] * 8
        e.dc = self
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.dc = None

    def g(self, name):
        return ADDR[name]

    def alloc(self, size, fill=0xA5, label=0):
        address = self.e.alloc(size, bytes([fill & 0xFF]) * size)
        if label:
            self.L.labels[address] = label
        return address

    def label(self, address, label):
        self.L.labels[address] = label

    def region(self, label, start, size, below=0):
        self.regions.append((label, start, size, below))

    def lab(self, p):
        return self.L.lab(p)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def fill(self, a, n, v):
        self.e.write(a, bytes([v & 0xFF]) * n)

    def set_weather(self, k, address):
        self.weather[k] = address

    def log(self):
        return self.L.log

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]

    def run_tick(self):
        with TickRounding(self.e):
            self.call(TICK)

    def run_draw(self, obj):
        self.call(DRAWALL, obj)

    def run_poly(self, obj, positions, texture, uvs, points):
        self.call(POLY, obj, positions, texture & MASK, uvs, points)

    def run_instance(self, which, x, y, kind, index, rotation, restore):
        return self.call(INSTANCERS[which], x & MASK, y & MASK, kind, index, rotation, restore & MASK) & MASK


class Host(Side):
    """The same on the host library."""
    ppc = False
    PTR = 8
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_dc_reset()
        CL.host_begin(lib, {}, {k: [signed64(v) for v in vs] for k, vs in case["queues"].items()})
        self.regions = []
        layout = host_layout(lib)
        self.OBJ = {name: (MAC_OBJ[name][0], layout[name]) for name in LAYOUT_ORDER}
        self.OBJ_SIZE = layout["size"]
        self.TABLE_STRIDE, self.TABLE_TYPE, self.TABLE_CONTROLS = layout["mesh_size"], layout["mesh_type"], layout["mesh_controls"]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dc_addr(name.encode())

    def alloc(self, size, fill=0xA5, label=0):
        return self.lib.tpw_dc_alloc(size, fill & 0xFF, label)

    def label(self, address, label):
        self.lib.tpw_cl_label(label, address)

    def region(self, label, start, size, below=0):
        self.regions.append((label, start, size, below))
        self.lib.tpw_dc_region(label, start, size, below)

    def lab(self, p):
        return self.lib.tpw_cl_lab(p)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def fill(self, a, n, v):
        C.memset(a, v & 0xFF, n)

    def set_weather(self, k, address):
        self.lib.tpw_dc_weather(k, address)

    def log(self):
        return CL.host_end(self.lib)

    def run_tick(self):
        self.lib.tpw_dc_tick()

    def run_draw(self, obj):
        self.lib.tpw_dc_drawall(obj)

    def run_poly(self, obj, positions, texture, uvs, points):
        self.lib.tpw_dc_poly(obj, positions, T.s32(texture), uvs, points)

    def run_instance(self, which, x, y, kind, index, rotation, restore):
        return self.lib.tpw_dc_instance(which, x, y, kind, index, rotation, restore) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    lib.tpw_dc_alloc.restype, lib.tpw_dc_alloc.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32, C.c_int32]
    lib.tpw_dc_region.argtypes = [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_dc_weather.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_dc_addr.restype, lib.tpw_dc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dc_drawall.argtypes = [C.c_uint64]
    lib.tpw_dc_poly.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_dc_instance.restype = C.c_uint32
    lib.tpw_dc_instance.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32]
    lib.bound = True


def host_layout(lib):
    cached = getattr(lib, "layout", None)
    if cached is None:
        out = (C.c_int64 * 32)()
        n = lib.tpw_dc_layout(out)
        values = list(out)[:n]
        names = LAYOUT_ORDER + ["size", "mesh_type", "mesh_controls", "mesh_size"]
        assert n == len(names), n
        cached = lib.layout = dict(zip(names, values))
    return cached


# ---------------------------------------------------------------------------------------------------------------- the world


def build_world(s, case):
    """Builds the world of a case on a side: the globals, the key and record arrays, the meshes, the weather, the ride tables."""
    s.label(s.g("clock"), L_CLOCK)
    s.region(R_KEYS, s.g("keys"), 0x2018, 16)
    s.region(R_UV, s.g("uv"), 120)
    s.region(R_QSIZE, s.g("qsize"), 8)
    s.region(R_TSIZE, s.g("tsize"), 8)
    s.region(R_FACES, s.g("faces"), 0x50240)
    for name, (kind, words) in GLOBALS.items():
        values = case["globals"].get(name, [0] * words)
        for k, v in enumerate(values):
            s.wr(s.g(name) + k * WIDTH[kind], kind, v)
    for k, word in enumerate(case["keys"]):
        s.wr(s.g("keys") + 4 * k, "w", word)
    for index, writes in case["records"].items():
        base = s.g("faces") + 0xA0 * index
        s.fill(base, 0xA0, 0xA5)
        for offset, kind, v in writes:
            s.wr(base + offset, kind, v)
    s.meshes = []
    for k, m in enumerate(case.get("meshes", [])):
        s.meshes.append(make_mesh(s, m, k))
    for slot, k in zip(("bp", "water", "flags"), range(len(s.meshes))):
        s.wr(s.g(slot), "p", s.meshes[k][0])
    if "winds" in case:
        world = s.alloc(32, 0xA5, L_WORLD)
        s.wr(s.g("world"), "p", world)
        for k, wind in enumerate(case["winds"]):
            block = s.alloc(0x68, 0x5A, L_WEATHER + k)
            s.wr(block + 0x5C, "f", wind)
            s.wr(block + 0x60, "f", 0x3F000000)
            s.set_weather(k, block)
    if "tables" in case:
        for name, count in TABLES:
            for j, (type_word, controls) in enumerate(case["tables"][name]):
                entry = s.g(name) + j * s.TABLE_STRIDE
                s.wr(entry + s.TABLE_TYPE, "w", type_word)
                s.wr(entry + s.TABLE_CONTROLS, "b", controls)
        for name in ("qsize", "tsize"):
            for k, v in enumerate(case["sizes"][name]):
                s.wr(s.g(name) + 4 * k, "w", v)
        for i, instance in enumerate(case["sets"]):
            block = s.alloc(16, 0xA5, L_SET + i)
            s.wr(s.g("ridesets") + s.PTR * instance, "p", block)


def make_mesh(s, m, k):
    """A raw mesh object (never constructed): 0xa5 everywhere except what the case sets. Returns (object, face sets, face buffer)."""
    obj = s.alloc(s.OBJ_SIZE, 0xA5, L_MESH + k)
    sets = s.alloc(16 * SETS, 0xA5)
    faces = s.alloc(8 * (FACES + 1), 0xA5)       # (one face more, so that a cursor at the end still lies in the region)
    s.region(R_FACES + 1 + k, faces, 8 * (FACES + 1))
    s.put(obj, "type", m["type"])
    s.put(obj, "maxsq", m["maxsq"])
    s.put(obj, "facesets", sets)
    s.put(obj, "faces", faces)
    pre = m["pre"]
    s.put(obj, "nface", pre["nface"])
    s.put(obj, "nvert", pre["nvert"])
    s.put(obj, "nuv", pre["nuv"])
    for i, v in enumerate(pre["box"]):
        s.put(obj, "box", v, i)
    s.put(obj, "nfaces", pre["nfaces"])
    s.put(obj, "nverts", pre["nverts"])
    s.put(obj, "cursor", faces + 8 * m.get("cursor", 0) if "cursor" in m else 0x6A6A6A6A)
    return obj, sets, faces


def dump_mesh(s, mesh):
    obj, sets, faces = mesh
    d = {name: s.get(obj, name) for name in ("type", "nface", "nvert", "nuv", "nfaces", "nverts", "maxsq")}
    d["box"] = [canon_f(s.get(obj, "box", i)) for i in range(6)]
    for name in ("cursor", "faces", "facesets"):
        d[name] = s.tag(s.get(obj, name))
    d["sets"] = [[s.rd(sets + 16 * j, "w"), s.rd(sets + 16 * j + 4, "h"), s.rd(sets + 16 * j + 6, "h"), s.rd(sets + 16 * j + 8, "h"),
                  s.rd(sets + 16 * j + 0xA, "h"), s.rd(sets + 16 * j + 0xC, "w")] for j in range(SETS)]
    d["buffer"] = [[s.rd(faces + 8 * j + 2 * k, "h") for k in range(4)] for j in range(FACES)]
    return d


def observe(s, case, result=None):
    out = {"log": s.log(), "result": result}
    for name, (kind, words) in GLOBALS.items():
        values = [s.rd(s.g(name) + k * WIDTH[kind], kind) for k in range(words)]
        out[name] = [canon_f(v) for v in values] if kind == "f" else values
    out["keys"] = [s.rd(s.g("keys") + 4 * k, "w") for k in range(KEY_WORDS)]
    out["meshes"] = [dump_mesh(s, m) for m in s.meshes]
    out["spots"] = [[s.rd(s.g("spots") + 16 * j + 4 * k, "w") for k in range(4)] for j in range(8)]
    return out


def execute(s, case):
    op = case["op"]
    if op == "tick":
        s.run_tick()
    elif op == "draw":
        s.run_draw(s.meshes[0][0])
    elif op == "poly":
        block = s.alloc(0x90, 0xA5)
        s.region(R_BUFFER, block, 0x90)
        for k, v in enumerate(case["points"]):
            s.wr(block + 4 * k, "f", v)
        s.run_poly(s.meshes[0][0], block, case["texture"], block + 0x30, block + 0x60)
    elif op == "inst":
        i = case["inst"]
        return s.run_instance(i["which"], i["x"], i["y"], i["kind"], i["index"], i["rotation"], i["restore"])
    return None


def ppc(e, case):
    s = Ppc(e, case)
    try:
        build_world(s, case)
        result = execute(s, case)
        return observe(s, case, result)
    finally:
        s.done()


def host(lib, case):
    s = Host(lib, case)
    build_world(s, case)
    result = execute(s, case)
    return observe(s, case, result)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0xFFC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x007FFFFF, 0x80000001, 0x3F800000, 0xBF800000,
           fb(10.0), fb(-10.0), fb(1024.0), fb(-1024.0), fb(1034.0), fb(-1034.0), fb(1014.0), fb(-1014.0), fb(1023.99), fb(1034.01),
           fb(-1033.99), fb(500.0), fb(-500.0), 0x7F7FFFFF, 0xFF7FFFFF]


def rf(r, wide=True):
    """A random float's bits: a random pattern, a special value, or a number near the box limits (about 1000)."""
    c = r.random()
    if c < 0.2 and wide:
        return r.getrandbits(32)
    if c < 0.45:
        return r.choice(SPECIAL)
    if c < 0.7:
        return fb(r.uniform(-1100.0, 1100.0))
    return fb(r.uniform(-4.0, 4.0))


def rword(r):
    return r.choice([0, 1, 2, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(16), r.getrandbits(8)])


def queues(**kw):
    return {k: list(v) for k, v in kw.items()}


def base_globals(r, **fixed):
    g = {"uv": [r.getrandbits(32) for _ in range(30)], "bc0": [rf(r)], "bc4": [rf(r)], "db50": [rword(r)], "db54": [rword(r)],
         "d94": [r.choice([0, 1, 5, 6, 7, -1, INT_MIN, 0x7FFFFFFF, r.getrandbits(32), r.randrange(-3, 12)]) & MASK],
         "d98": [rf(r)], "d9c": [rf(r)], "da0": [rf(r)], "fbdbc": [0], "flag": [r.choice([0, 1, 0x80, 0xFF, r.getrandbits(8)])],
         "ws": [rf(r) for _ in range(3)], "cam": [rf(r)]}
    for k, v in fixed.items():
        g[k] = v if isinstance(v, list) else [v]
    return g


def mesh_spec(r, mtype, maxsq):
    return {"type": mtype, "maxsq": maxsq, "pre": {"nface": r.getrandbits(16), "nvert": r.getrandbits(16), "nuv": r.getrandbits(16),
                                                    "box": [r.getrandbits(32) for _ in range(6)], "nfaces": r.getrandbits(32),
                                                    "nverts": r.getrandbits(32)}}


def record_writes(r, mtype, texture, is_square, positions=None, keep=None):
    """The writes (offset, kind, value) of one dynamic face record, over a 0xa5 fill."""
    pos = positions or [rf(r), rf(r), rf(r)]
    if is_square:
        w = [(0, "f", pos[0]), (4, "f", pos[1]), (8, "f", pos[2]), (0xC, "b", r.choice([0, 1, 4, 0x10, 0x40, r.getrandbits(8)])),
             (0x10, "w", r.choice([0, 4, 0x10, 0x40, 0x104, 0xFFFFFF40, r.getrandbits(32)])), (0x14, "f", rf(r)),
             (0x18, "w", r.choice([0, 1, r.getrandbits(32)])), (0x1C, "b", r.getrandbits(8))]
    else:
        w = [(0, "f", pos[0]), (4, "f", pos[1]), (8, "f", pos[2])] + [(4 * k, "f", rf(r)) for k in range(3, 36)]
    w += [(0x90, "w", mtype), (0x94, "w", is_square), (0x98, "w", r.choice([0, 1, r.getrandbits(32)]) if keep is None else keep),
          (0x9C, "b", texture)]
    return w


def pick_square_flag(r):
    return r.choice([0, 0, 1, 1, 2, 0x100, 0xFFFFFFFF, r.getrandbits(32) | 1])


def list_world(r, mesh_types, nrec, mode="random", texture_mode="random", keys_extra=0):
    """Records and keys for the meshes of a case. mode: how the types are grouped; every list is of nrec records."""
    types = list(mesh_types)
    foreign = 7
    order = []
    if mode == "ours_first":
        order = [types[0]] * nrec
    elif mode == "other_only":
        order = [foreign] * nrec
    elif mode == "other_ours_other":
        third = max(1, nrec // 3)
        order = ([foreign] * third + [types[0]] * (nrec - 2 * third) + [foreign] * third)[:nrec]
    elif mode == "ours_other":
        half = nrec // 2
        order = [types[0]] * half + [foreign] * (nrec - half)
    elif mode == "ours_other_ours":
        third = max(1, nrec // 3)
        order = ([types[0]] * third + [foreign] * third + [types[0]] * nrec)[:nrec]
    else:
        order = [r.choice(types + [foreign]) for _ in range(nrec)]
    textures = []
    t = r.randrange(20)
    for _ in range(nrec):
        if texture_mode == "same":
            pass
        elif texture_mode == "runs":
            t = t if r.random() < 0.6 else r.randrange(20)
        else:
            t = r.choice([0, 19, r.randrange(20)])
        textures.append(t)
    indices = r.sample(range(0, 0x806), nrec) if r.random() < 0.3 else list(range(nrec))
    if nrec and r.random() < 0.15:
        indices[-1] = 0x805
    records, keys = {}, []
    for k in range(nrec):
        records[indices[k]] = record_writes(r, order[k], textures[k], pick_square_flag(r))
        keys.append((indices[k] << 5) | (textures[k] if r.random() < 0.5 else r.getrandbits(5)))
    keys += [r.getrandbits(32) & ~0x3FFFFFF for _ in range(KEY_WORDS - len(keys))]   # (never read: key >> 5 stays far apart)
    return records, keys


def advance_queues(r, kind="random"):
    """The mesh fakes' counter steps, as sequences of per-call values."""
    if kind == "default":
        return {}
    sqv = [r.choice([4, 4, 0, 1, 7, 0x7FFF, 0x8000, 0xFFFE, 0x10000, 0x12345]) for _ in range(8)]
    sqf = [r.choice([2, 2, 0, 1, 6, 0x7FFF, 0xFFFF, 0x10001]) for _ in range(8)]
    sqc = [r.choice([2, 2, 0, 1, 6]) for _ in range(8)]
    vtx = [r.choice([1, 1, 0, 2, 7]) for _ in range(8)]
    return queues(sqv=sqv, sqf=sqf, sqc=sqc, vtx=vtx)


def draw_case(r, mtype=None, maxsq=None, nrec=None, mode="random", texture_mode="random", fb_count=None, advance="random"):
    mtype = r.choice([0, 1, 3, 7]) if mtype is None else mtype
    maxsq = r.choice([0, 1, 2, 5, 0x600, 3, 4, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF]) if maxsq is None else maxsq
    nrec = r.randrange(0, 13) if nrec is None else nrec
    records, keys = list_world(r, [mtype], nrec, mode, texture_mode)
    count = nrec if fb_count is None else fb_count
    return {"op": "draw", "queues": advance_queues(r, advance), "globals": base_globals(r, fbdbc=count & MASK), "keys": keys,
            "records": records, "meshes": [mesh_spec(r, mtype, maxsq)]}


def draw_edges():
    r = random.Random(1201)
    for mtype in (0, 1, 3):
        for maxsq in (0, 1, 2, 5, 0x600, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF):   # (the last three: negative and huge, the compare is signed)
            yield draw_case(r, mtype, maxsq, 4)
    yield draw_case(r, 0, 5, 0)                                    # empty list
    yield draw_case(r, 0, 5, 3, fb_count=-1)                       # negative count: no iteration
    yield draw_case(r, 1, 5, 3, fb_count=0)
    yield draw_case(r, 0, 5, 1)                                    # one record (a square or a poly face)
    yield draw_case(r, 0, 5, 1)
    yield draw_case(r, 3, 5, 1)
    yield draw_case(r, 0, 5, 6, mode="other_only")                 # all of another type: no draw, the loop runs to the end
    for mode in ("ours_first", "other_ours_other", "ours_other", "ours_other_ours"):
        for texture_mode in ("same", "runs", "random"):
            yield draw_case(r, 0, 0x600, 9, mode, texture_mode)
    yield draw_case(r, 0, 2, 9, "ours_first", "runs")              # more records than mMaxSquares: CTRACE for both kinds
    yield draw_case(r, 0, 1, 9, "ours_first", "random")
    yield draw_case(r, 0, 0, 9, "ours_first", "random")            # nothing is ever drawn but the bookkeeping runs
    yield draw_case(r, 1, 0x600, 12, "ours_first", "random", advance="default")
    yield draw_case(r, 0, 0x600, 8, "ours_first", "random", advance="random")


def box_case(r, mtype, is_square, position):
    """One drawn face at a given first corner: the box of the mesh is that corner plus and minus 10, clamped."""
    records = {0: record_writes(r, mtype, r.randrange(20), is_square, list(position))}
    return {"op": "draw", "queues": advance_queues(r, "default"), "globals": base_globals(r, fbdbc=1),
            "keys": [0] + [0] * (KEY_WORDS - 1), "records": records, "meshes": [mesh_spec(r, mtype, 5)]}


def box_edges():
    r = random.Random(1202)
    values = [fb(0.0), fb(-0.0), fb(1014.0), fb(1034.0), fb(1033.99), fb(1034.01), fb(-1014.0), fb(-1034.0), fb(-1033.99), fb(-1034.01),
              fb(1024.0), fb(-1024.0), fb(5000.0), fb(-5000.0), 0x7FC00000, 0xFFC00000, 0x7F800000, 0xFF800000, fb(500.5), fb(-3.25)]
    for v in values:
        for sq in (1, 0):
            yield box_case(r, 0, sq, (v, fb(r.uniform(-900, 900)), fb(r.uniform(-900, 900))))
            yield box_case(r, 0, sq, (fb(r.uniform(-900, 900)), v, fb(r.uniform(-900, 900))))
            yield box_case(r, 0, sq, (fb(r.uniform(-900, 900)), fb(r.uniform(-900, 900)), v))


def draw_all_edges():
    yield from draw_edges()
    yield from box_edges()


def draw_random(r):
    return draw_case(r)


# ---- DC_CreatePolyFace


def poly_case(r, first=None, cursor=None, vtx=None, texture=None):
    first = r.choice([0, 1, 0xFFFD, 0xFFFE, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(8)]) if first is None else first
    m = mesh_spec(r, 0, 0)
    m["pre"]["nverts"] = first
    m["cursor"] = r.randrange(0, 40) if cursor is None else cursor
    vtx = [r.choice([1, 1, 0, 2, 7]) for _ in range(4)] if vtx is None else vtx
    return {"op": "poly", "queues": queues(vtx=vtx), "globals": base_globals(r), "keys": [0] * KEY_WORDS, "records": {},
            "meshes": [m], "points": [rf(r) for _ in range(36)], "texture": r.getrandbits(32) if texture is None else texture}


def poly_edges():
    r = random.Random(1203)
    for first in (0, 1, 0xFFFD, 0xFFFE, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0xFFFFFFFF):
        yield poly_case(r, first)
        yield poly_case(r, first, vtx=[1, 1, 1, 1])
    for vtx in ([0, 0, 0, 0], [2, 2, 2, 2], [7, 0, 7, 0], [1, 0, 0, 5]):
        yield poly_case(r, 3, vtx=vtx)
    for texture in (0, 1, MASK, 0x12345678):
        yield poly_case(r, 10, texture=texture)
    yield poly_case(r, 10, cursor=0)
    yield poly_case(r, 10, cursor=FACES - 2)


def poly_random(r):
    return poly_case(r)


# ---- DC_TickDynamicCellMeshes

RATES = [0.0, 1.0, 0.25, 1.25, 2.0, 3.0, -1.0, 0.9999999, 2.9999999, 1.9999999, 0.99999999, 2.99999999, 1.99999999, 0.99999, 1e10, -1e10, float("nan"), float("inf"),
         -float("inf"), 1.0000000149011612, 0.5, 1.5, 1e-310, 4294967296.5, 2147483647.5, 2147483648.0, -2147483648.5]


def tick_case(r, count=None, g=None, q=None, winds=None):
    """A tick over three meshes (types 0, 1, 3) and nrec records. g: global overrides, q: queue overrides, winds: the eight winds."""
    mtypes = [0, 1, 3]
    nrec = r.choice([0, 0, 1, 2, 3, 3]) if count is None else count
    records, keys = list_world(r, mtypes, nrec, r.choice(["random", "ours_first", "ours_first"]), r.choice(["same", "runs", "random"]))
    glob = base_globals(r, fbdbc=nrec)
    meshes = [mesh_spec(r, t, r.choice([0, 1, 2, 5, 0x600, 0xFFFFFFFF])) for t in mtypes]
    queue = advance_queues(r, r.choice(["random", "default"]))
    queue["weather"] = [r.randrange(8) for _ in range(8)]
    queue["pause"] = [r.choice([0, 0, 0, 1, 2, -1, INT_MIN])]
    queue["rate"] = [dbits(r.choice(RATES)) if r.random() < 0.6 else dbits(r.uniform(-4, 4)) for _ in range(3)]
    if r.random() < 0.5:
        queue["rot"] = [rf(r) if r.random() < 0.8 else -1 for _ in range(8)]
        if r.random() < 0.5:
            queue["rotbc0"] = [rf(r)]
            queue["rotbc4"] = [rf(r)]
    if r.random() < 0.4:
        queue["wsx"], queue["wsy"], queue["wsz"] = [rf(r), -1], [rf(r), -1], [rf(r), -1]
    if records and r.random() < 0.3:
        used = sorted(records)
        queue["sortk"] = [(r.choice(used) << 5) | r.getrandbits(5) for _ in range(nrec)] + [-1]
    if winds is None:
        winds = [rf(r) if r.random() < 0.7 else r.choice(SPECIAL) for _ in range(8)]
    for k, v in (g or {}).items():
        glob[k] = v if isinstance(v, list) else [v]
    queue.update(q or {})
    return {"op": "tick", "queues": queue, "globals": glob, "keys": keys, "records": records, "meshes": meshes, "winds": winds}


def tick_edges():
    r = random.Random(1204)
    running = {"pause": [0], "rate": [dbits(1.0)] * 3}
    paused = {"pause": [1]}
    yield tick_case(r, 0, q=running)
    for db50, db54 in [(0, 0), (1, 1), (-1, -1), (0x7FFFFFFF, INT_MIN), (INT_MIN, 0x7FFFFFFF), (0x1000001, 0x12345679),
                       (0x12345679, 0x1000001), (3, -3)]:
        yield tick_case(r, 0, g={"db50": db50 & MASK, "db54": db54 & MASK, "bc0": fb(1000.0), "bc4": fb(-20.5)}, q=paused)
    yield tick_case(r, 0, g={"db50": 0x12345679, "db54": 0x1000001, "bc0": 0x7F7FFFFF, "bc4": 0xFF7FFFFF}, q=paused)
    for pause in (0, 1, 2, -1, INT_MIN):
        yield tick_case(r, 0, q={"pause": [pause]})
    for rate in RATES:
        yield tick_case(r, 0, q={"pause": [0], "rate": [dbits(rate)] * 3})
    yield tick_case(r, 0, q={"pause": [0], "rate": [dbits(0.25), dbits(1.75), dbits(2.5)]})
    yield tick_case(r, 0, q={"pause": [0], "rate": [dbits(3.0), dbits(-0.5), dbits(1.0000001)]})
    for flag in (0, 1, 0xFF):
        for before in (-3, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0x7FFFFFFF):
            yield tick_case(r, 0, g={"flag": flag, "d94": before & MASK}, q=running)
    for wind in ([0, fb(360.0), fb(-90.0), fb(90.0), 0x7FC00000, 0x7F800000, 0xFF7FFFFF, fb(1e30)], [fb(float(k)) for k in range(8)]):
        yield tick_case(r, 0, q=dict(paused, weather=list(range(8))), winds=wind)
    yield tick_case(r, 0, q=dict(paused, rot=[fb(0.25 * k) for k in range(8)], rotbc0=[fb(7.0)], rotbc4=[fb(-9.0)]))
    yield tick_case(r, 0, q=dict(paused, rot=[fb(0.25 * k) for k in range(8)]))
    yield tick_case(r, 0, q=dict(paused, rot=[-1] * 8))
    yield tick_case(r, 0, q=dict(paused, wsx=[fb(1.5), -1], wsy=[fb(2.5), -1], wsz=[fb(3.5), -1]))
    for count in (1, 2, 3):
        for _ in range(3):
            yield tick_case(r, count)
    big = tick_case(r, 0, q=paused)                                 # every key names a record of another type
    big["globals"]["fbdbc"] = [0x806]
    big["keys"] = [(k % 4) << 5 for k in range(0x806)]
    big["records"] = {k: record_writes(r, 7, 3, r.choice([0, 1])) for k in range(4)}
    yield big
    negative = tick_case(r, 2, q=paused)
    negative["globals"]["fbdbc"] = [MASK]
    yield negative


def tick_random(r):
    return tick_case(r)


# ---- the instancers

SIZE = [("qmesh", 8), ("kmesh", 20), ("wmesh", 25), ("hmesh", 28)]
ROTATIONS = [0, 90, 180, 270, 360, 45, -90 & MASK, 450, 0x7FFFFFFF, 0x80000000, 0x168 - 0xB4, 0x168 - 0x5A, 0x168 - 0x10E, 1, 359, 720,
             0xFFFFFFFF, 0x168, 0x169]


def inst_case(r, which, x=None, y=None, index=None, rotation=None, restore=None, inst=None, rest=None, set_ids=(0, 1, 5, 10239)):
    count = TABLES[which][1]
    tables = {name: [(r.choice([0, 1, r.getrandbits(32)]), r.getrandbits(8)) for _ in range(n)] for name, n in TABLES}
    q = queues(inst=[r.choice([0, 1, 5, 10239, 0xFFFFFFFF, r.getrandbits(32)]) if inst is None else inst],
               restore=[r.choice([0, 1, 5, 10239, 0xFFFFFFFF, r.getrandbits(32)]) if rest is None else rest])
    if which == 2:
        q["inst"] = [r.choice([0, 1, 5, 10239]) if inst is None else inst]
        q["restore"] = [r.choice([0, 1, 5, 10239]) if rest is None else rest]
    return {"op": "inst", "queues": q, "globals": base_globals(r), "keys": [0] * KEY_WORDS, "records": {}, "tables": tables,
            "sizes": {"qsize": [r.getrandbits(32), r.getrandbits(32)], "tsize": [r.getrandbits(32), r.getrandbits(32)]},
            "sets": list(set_ids),
            "inst": {"which": which, "x": r.choice([0, 1, -1, 0x7FFFFFFF, INT_MIN, r.randrange(-50, 300)]) if x is None else x,
                     "y": r.choice([0, 1, -1, 0x7FFFFFFF, INT_MIN, r.randrange(-50, 300)]) if y is None else y,
                     "kind": r.getrandbits(32), "index": r.randrange(count) if index is None else index,
                     "rotation": r.choice(ROTATIONS) if rotation is None else rotation,
                     "restore": r.choice([0, 1, 2, -1]) if restore is None else restore}}


def inst_edges(which):
    def gen():
        r = random.Random(1210 + which)
        for rotation in ROTATIONS:
            for restore in (0, 1):
                yield inst_case(r, which, x=r.randrange(0, 100), y=r.randrange(0, 100), rotation=rotation, restore=restore)
        for x in (0, 1, -1, 0x7FFFFFFF, INT_MIN):
            for y in (0, 1, -1, 0x7FFFFFFF, INT_MIN):
                yield inst_case(r, which, x=x, y=y, rotation=r.choice([0, 90, 180, 270]), restore=r.choice([0, 1]))
        count = TABLES[which][1]
        for index in (0, 1, count - 1):
            yield inst_case(r, which, index=index, restore=0)
            yield inst_case(r, which, index=index, restore=1)
        for restore in (2, -1, INT_MIN):
            yield inst_case(r, which, restore=restore)
        for answer in (0, 1, 5, 10239):
            yield inst_case(r, which, restore=0, inst=answer)
            yield inst_case(r, which, restore=1, rest=answer)
        if which != 2:
            yield inst_case(r, which, restore=0, inst=0xFFFFFFFF)
            yield inst_case(r, which, restore=1, rest=0xFFFFFFFF)
    return gen


def inst_random(which):
    return lambda r: inst_case(r, which)


TESTS = [
    FnTest("DC_TickDynamicCellMeshes", ppc, host, edge=tick_edges, rnd=tick_random),
    FnTest("CDynamicMesh::DrawAll", ppc, host, edge=draw_all_edges, rnd=draw_random),
    FnTest("CDynamicMesh::DC_CreatePolyFace", ppc, host, edge=poly_edges, rnd=poly_random),
    FnTest("LLCCM_CreateQMesh", ppc, host, edge=inst_edges(0), rnd=inst_random(0)),
    FnTest("LLCCM_CreateKTrackMesh", ppc, host, edge=inst_edges(1), rnd=inst_random(1)),
    FnTest("LLCCM_CreateWTrackMesh", ppc, host, edge=inst_edges(2), rnd=inst_random(2)),
    FnTest("LLCCM_CreateHoardingMesh", ppc, host, edge=inst_edges(3), rnd=inst_random(3)),
]
