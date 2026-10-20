"""Unit w214, a coaster car bent to the track and a running train's sounds and screams, against libtpw_unit_CarBend.dylib
(tests/unit_CarBend.cpp), main app ("SimTheme Park"), src/TPConfig_bend.cpp:
  InterpAngles 0x1004dd5c, AccumulateBendAngles 0x10045ad8, RotateNodeVertexPosns 0x10046620, BendCarToTrack 0x1004688c,
  HandleScreams 0x10046d78, TickTrainSounds 0x1004702c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the M3D2 pieces as Ptr32 offsets into the arena) for
the translation; the same code reads both back. The callees outside the unit are fakes on both sides that record their arguments (floats as
bits, the mesh as a label) and answer from the case's queues: GetFirstPersonRideID, GetVectorRotationAroundYAxisFromZ (the angle from the
queue "Rot"; the log shows the point's three floats), SOUND_SetParamValue, SOUND_MoveSoundEffect, SCREAM_Start, SCREAM_Single, SCREAM_Move,
SCREAM_SetLevel, SCREAM_Stop and TPProcessEmbeddedData. sqrt and acos are the emulator's standard stubs, modf and fmod this module's own
(the C library's on the host); __cvt_fp2unsigned (0x10237cd8) runs as the original on the PowerPC side.

The world of a case: the engine's sine table with its mask and scale (the bend angles go through it), gActionRecorder.mRecording, the eight
clamps (gBendAngleClamps), gRideAnimSets; a coaster instance (state, spacing, samples, a type holding the rail heights and the sound scale),
a car with its type, a train, and for BendCarToTrack the ride sets (a mesh whose header holds the live nodes and mini nodes, and the master
copy apart from it or the header itself). What is compared: InterpAngles' answer, the three arrays (all 128 words each) and *centre, every
vertex block (live, master, source) whole, the train's words, and the event log of every fake.

Fused operations: fmadds and fmsubs in InterpAngles, AccumulateBendAngles and RotateNodeVertexPosns (BendCarToTrack runs the last two as
the original); HardwareRounding hooks all six ranges (fused operations rounded once, fctiwz saturating).

Not tested (inputs the original cannot take, or that read what no case can pin down): a negative, NaN or huge car length (the loops would
run billions of times), a position outside [0, count) other than the open range (-1, 0), BendCarToTrack with a null ride set (unguarded
read) and with a mini node that lacks flag 0x200 (a Mac address difference across two arrays), TickTrainSounds with the recorder running and
mF10 set while the train's flag 0x100 is set (HandleScreams then moves the single scream to x, y, z, which the original takes from
registers its caller left), and a vertex z that sends RotateNodeVertexPosns beyond the 128-entry arrays.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CarBend"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original
ACCUMULATE, ROTATE, BEND, SCREAMS, SOUNDS, INTERP = 0x10045AD8, 0x10046620, 0x1004688C, 0x10046D78, 0x1004702C, 0x1004DD5C
SIZES = {ACCUMULATE: 2796, ROTATE: 520, BEND: 320, SCREAMS: 612, SOUNDS: 320, INTERP: 188}
OWN = set(SIZES)

# ---- the callees faked on both sides, and the runtime routine that runs as the original
RIDE_ID, SET_PARAM, MOVE_SOUND = 0x10032B64, 0x100DD380, 0x100DDF60
SCREAM_START, SCREAM_SINGLE, SCREAM_MOVE, SCREAM_LEVEL, SCREAM_STOP = 0x100CCB30, 0x100CCDE8, 0x100CCD3C, 0x100CCCC0, 0x100CCD98
ROTATION, EMBEDDED, CVT_FP2UNSIGNED = 0x100C10B8, 0x101FFB0C, 0x10237CD8

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_cbd_addr(name)
ADDR = dict(table=0x103B74D4, mask=0x103B74CC, scale=0x103B74D0, recording=0x1037EC68, clamps=0x1029CD8C, ridesets=0x102C79A4)

# struct -> ([(field, Mac offset, kind)], Mac size). Kinds: w word, p native pointer (4 bytes on the Mac, 8 on the host), h halfword, a Ptr32 of
# the M3D2 file layout (the Mac's address, on the host the offset from the arena's base). The host's offsets come from tpw_cbd_layout in the
# same order, each struct's size last.
FIELDS = {
    "car": ([("mType", 0x04, "p"), ("mRide", 0x0c, "w"), ("mF10[0]", 0x10, "w"), ("mF10[1]", 0x14, "w"), ("mF10[2]", 0x18, "w"), ("mF40", 0x40, "w")], 0x60),
    "train": ([("mFlags", 0x00, "w"), ("mF10", 0x10, "w"), ("mFirstPersonCar", 0x34, "p"), ("mF3C[0]", 0x3c, "w"), ("mF3C[1]", 0x40, "w"), ("mF3C[2]", 0x44, "w"), ("mF48[0]", 0x48, "w"), ("mF48[1]", 0x4c, "w"), ("mF48[2]", 0x50, "w"), ("mF54[1]", 0x58, "w"), ("mF64", 0x64, "w"), ("mF68", 0x68, "w"), ("mF6C", 0x6c, "w"), ("mF70", 0x70, "w"), ("mPosition", 0x74, "w"), ("mF78", 0x78, "w")], 0x80),
    "inst": ([("mType", 0x04, "p"), ("mState", 0x08, "w"), ("mSamples", 0x9c, "p"), ("mSampleCount", 0xa4, "w"), ("mFAC", 0xac, "w")], 0x150),
    "type": ([("mF50", 0x50, "w"), ("mF58", 0x58, "w"), ("mF5C", 0x5c, "w")], 0x31c),
    "ctype": ([("mNodeIndex", 0x20, "w"), ("mF24", 0x24, "w"), ("mF28", 0x28, "w")], 0x34),
    "sample": ([("mFlags", 0x0, "w"), ("mPos.x", 0x4, "w"), ("mPos.y", 0x8, "w"), ("mPos.z", 0xc, "w"), ("mYaw", 0x10, "w")], 0x38),
    "set": ([("mHeads", 0x08, "p")], 0xd0),
    "mesh": ([("mHeader", 0x04, "p")], 0x10),
    "header": ([("mNodeCount", 0x44, "h"), ("mNodes", 0x70, "a"), ("mMiniNodes", 0x74, "a"), ("mSelf", 0x9c, "a")], 0xb8),
    "node": ([("mFlags", 0x0, "w"), ("mVertexCount", 0x58, "h"), ("mVertices", 0x60, "a")], 0xa0),
    "mini": ([("mFlags", 0x0, "w")], 0x58),
    "vset": ([], 0x30),
}
ORDER = ["car", "train", "inst", "type", "ctype", "sample", "set", "mesh", "header", "node", "mini", "vset"]
MAC = {name: {f: o for f, o, _ in fields} for name, (fields, _) in FIELDS.items()}
MAC_SIZE = {name: size for name, (_, size) in FIELDS.items()}
KINDS = {name: {f: k for f, _, k in fields} for name, (fields, _) in FIELDS.items()}
HOST = {}          # struct -> {field: offset}, filled by bind
HOST_SIZE = {}
# The host's offsets and sizes (native pointers: a car is 120 bytes and a train 144 where the Mac's are 0x60 and 0x80, a sample 64 where the
# Mac's is 0x38; the M3D2 pieces keep the file layout: header 184, node 160, mini node 88, vertex set 48).
HOST_LAYOUT = [8, 20, 24, 28, 32, 88, 120, 0, 24, 64, 76, 80, 84, 88, 92, 96, 104, 116, 120, 124, 128, 132, 136, 144, 8, 16, 192, 204, 212, 496, 84, 92, 96, 944, 44, 48, 52, 64, 0, 4, 8, 12, 16, 72, 16, 392, 8, 112, 68, 112, 116, 156, 184, 0, 88, 96, 160, 0, 88, 48]
SAMPLE_FIELDS = ("mFlags", "mPos.x", "mPos.y", "mPos.z", "mYaw")
L_MESH = 20        # a ride set's mesh is label 20 + the ride's number


def f32(x):
    """x rounded to single precision (an overflow gives the infinity)."""
    try:
        return struct.unpack(">f", struct.pack(">f", x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", f32(x)))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


PI32 = f32(math.pi)
SIN = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(4096)]
SIN_SCALE = fb(4096 / (2 * math.pi))
TABLE = {fmt: struct.pack(fmt[0] + "4096I", *SIN) for fmt in (">I", "<I")}
DEFAULT_CLAMPS = [0, 0x3F800000] * 4

# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's six functions (Unicorn rounds fused operations twice and saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(CVT_FP2UNSIGNED)


def stub_rotation(e):
    """float GetVectorRotationAroundYAxisFromZ(s3DPoint&): logs the point's three floats; the angle is the queue "Rot"'s."""
    L = e.cl
    p = e.r(3)
    L.rec("GetVectorRotationAroundYAxisFromZ", [canon_f(e.u32(p)), canon_f(e.u32(p + 4)), canon_f(e.u32(p + 8))])
    e.set_f1(bitsf(L.ret("Rot", 0)))


def modf(e):
    """modf(double, double*): the fraction in f1, the whole part stored through r5."""
    fraction, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(fraction)


def fmod(e):
    x, y = e.f(1), e.f(2)
    if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
        e.set_f1(math.nan)
    else:
        e.set_f1(math.fmod(x, y))


STUBS.internal(RIDE_ID, CL.recorder("GetFirstPersonRideID", "", ret="i"))
STUBS.internal(SET_PARAM, CL.recorder("SOUND_SetParamValue", "iuu", ret="i"))
STUBS.internal(MOVE_SOUND, CL.recorder("SOUND_MoveSoundEffect", "iiii", ret="i"))
STUBS.internal(SCREAM_START, CL.recorder("SCREAM_Start", "iiiiiii", ret="i"))
STUBS.internal(SCREAM_SINGLE, CL.recorder("SCREAM_Single", "iiiiii", ret="i"))
STUBS.internal(SCREAM_MOVE, CL.recorder("SCREAM_Move", "iiii", ret="i"))
STUBS.internal(SCREAM_LEVEL, CL.recorder("SCREAM_SetLevel", "iii"))
STUBS.internal(SCREAM_STOP, CL.recorder("SCREAM_Stop", "i"))
STUBS.internal(ROTATION, stub_rotation)
STUBS.internal(EMBEDDED, CL.recorder("TPProcessEmbeddedData", "L"))
STUBS.imp("modf", modf)
STUBS.imp("fmod", fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)       # sqrt and acos (MATH1), memset, memcpy ...


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory. Struct offsets and sizes (offs, sizes) are the Mac's or the host's."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def new(self, name, count=1):
        return self.alloc(self.sizes[name] * count)

    def at(self, name, base, index):
        return base + self.sizes[name] * index

    def put(self, name, addr, field, value):
        a = addr + self.offs[name][field]
        kind = KINDS[name][field]
        if kind == "p":
            self.write(a, struct.pack(self.pfmt, value))
        elif kind == "h":
            self.write(a, struct.pack(self.fmt[0] + "H", value & 0xFFFF))
        elif kind == "a":
            self.words(a, [self.p32(value)])
        else:
            self.words(a, [value])

    def get(self, name, addr, field):
        return self.rd_word(addr + self.offs[name][field])

    def put_all(self, name, addr, values):
        for field, value in values.items():
            self.put(name, addr, field, value)

    def sample_block(self, samples):
        """The track samples (flags, x, y, z, yaw bits) in one block."""
        stride, offs = self.sizes["sample"], self.offs["sample"]
        buf = bytearray(stride * len(samples))
        for i, values in enumerate(samples):
            for field, value in zip(SAMPLE_FIELDS, values):
                struct.pack_into(self.fmt, buf, i * stride + offs[field], value & MASK)
        a = self.alloc(len(buf))
        self.write(a, bytes(buf))
        return a

    def vertex_block(self, w, verts, fill, room=0):
        """Vertex sets of four (x[4], y[4], z[4]) holding verts (room: sets for at least that many), the rest a finite fill; remembered to be read back whole."""
        size = self.sizes["vset"]
        sets = max(1, (max(len(verts), room) + 3) // 4)
        a = self.alloc(sets * size)
        self.words(a, [fb(fill + i) for i in range(sets * size // 4)])
        for i, (x, y, z) in enumerate(verts):
            b = a + (i // 4) * size + (i % 4) * 4
            self.words(b, [x])
            self.words(b + 16, [y])
            self.words(b + 32, [z])
        w.vblocks.append((a, sets * size // 4))
        return a

    def ride_sets(self, table):
        base = self.g("ridesets")
        for i in range(16):
            self.write(base + i * self.psize, struct.pack(self.pfmt, table.get(i, 0)))


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt, pfmt, psize = ">I", ">I", 4
    offs, sizes = MAC, MAC_SIZE

    def __init__(self, e, case):
        self.e = e
        self.saved = None

    def begin(self, case, labels):
        self.L = self.e.cl = CL.PpcLog({a: n for a, n in labels}, case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def p32(self, address):
        return address

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        with ROUNDING(self.e):
            return T.call_ppc(self.e, entry, args)

    def interp(self, a, b, t):
        _, _, f1 = self.call(INTERP, [T.F(bitsf(a)), T.F(bitsf(b)), T.F(bitsf(t))])
        return fb(f1)

    def accumulate(self, inst, car, yaws, pitches, heights, capacity, position, centre):
        self.call(ACCUMULATE, [inst, car, yaws, pitches, heights, capacity & MASK, T.F(bitsf(position)), centre])

    def rotate(self, node, source, dest, count, yaws, pitches, heights, centre, spacing):
        self.call(ROTATE, [node, source, dest, count, yaws, pitches, heights, centre, T.F(bitsf(spacing))])

    def bend(self, inst, car):
        self.call(BEND, [inst, car])

    def screams(self, inst, train, x, y, z):
        self.call(SCREAMS, [inst, train, x & MASK, y & MASK, z & MASK])

    def sounds(self, inst, train):
        self.call(SOUNDS, [inst, train])


class Host(Side):
    """The same on the host library."""
    fmt, pfmt, psize = "<I", "<Q", 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.offs, self.sizes = HOST, HOST_SIZE
        self.base = lib.tpw_cbd_base()
        self.blocks = []

    def begin(self, case, labels):
        CL.host_begin(self.lib, {n: a for a, n in labels}, case["queues"])

    def done(self):
        for block in self.blocks:
            self.lib.tpw_cbd_free(block)
        self.blocks = []

    def g(self, name):
        return self.lib.tpw_cbd_addr(name.encode())

    def p32(self, address):
        return address - self.base if address else 0

    def alloc(self, n):
        block = self.lib.tpw_cbd_alloc(max(n, 16))          # zeroed, 32-bit addressable (platform/arena.cpp)
        self.blocks.append(block)
        return block

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def interp(self, a, b, t):
        return self.lib.tpw_cbd_InterpAngles(a, b, t)

    def accumulate(self, inst, car, yaws, pitches, heights, capacity, position, centre):
        self.lib.tpw_cbd_AccumulateBendAngles(inst, car, yaws, pitches, heights, capacity & MASK, position, centre)

    def rotate(self, node, source, dest, count, yaws, pitches, heights, centre, spacing):
        self.lib.tpw_cbd_RotateNodeVertexPosns(node, source, dest, count, yaws, pitches, heights, centre, spacing)

    def bend(self, inst, car):
        self.lib.tpw_cbd_BendCarToTrack(inst, car)

    def screams(self, inst, train, x, y, z):
        self.lib.tpw_cbd_HandleScreams(inst, train, s32(x), s32(y), s32(z))

    def sounds(self, inst, train):
        self.lib.tpw_cbd_TickTrainSounds(inst, train)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_cbd_addr.restype, lib.tpw_cbd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cbd_alloc.restype, lib.tpw_cbd_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cbd_free.argtypes = [C.c_uint64]
    lib.tpw_cbd_base.restype = C.c_uint64
    lib.tpw_cbd_layout.restype, lib.tpw_cbd_layout.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_cbd_InterpAngles.restype, lib.tpw_cbd_InterpAngles.argtypes = C.c_uint32, [C.c_uint32] * 3
    lib.tpw_cbd_AccumulateBendAngles.argtypes = [C.c_uint64] * 5 + [C.c_uint32, C.c_uint32, C.c_uint64]
    lib.tpw_cbd_RotateNodeVertexPosns.argtypes = [C.c_uint64] * 3 + [C.c_uint32] + [C.c_uint64] * 3 + [C.c_uint32, C.c_uint32]
    lib.tpw_cbd_BendCarToTrack.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_cbd_HandleScreams.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_cbd_TickTrainSounds.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    out = (C.c_int64 * 128)()
    n = lib.tpw_cbd_layout(out)
    got, i = list(out)[:n], 0
    for s in ORDER:
        HOST[s] = {}
        for name, _, _ in FIELDS[s][0]:
            HOST[s][name] = got[i]
            i += 1
        HOST_SIZE[s] = got[i]
        i += 1
    assert i == n and got == HOST_LAYOUT, got
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


class W:
    """What build made: the addresses the call and the read-back need."""

    def __init__(self):
        self.labels = []          # (address, label) of the objects the log names (the meshes)
        self.vblocks = []         # (address, words) of every vertex block


def common_globals(s, case):
    s.words(s.g("recording"), [case.get("recording", 0)])


def sine_globals(s, case):
    """The engine's sine table, its mask and scale, and the eight clamps."""
    s.write(s.g("table"), TABLE[s.fmt])
    s.words(s.g("mask"), [0xFFF])
    s.words(s.g("scale"), [SIN_SCALE])
    s.words(s.g("clamps"), case.get("clamps", DEFAULT_CLAMPS))


def build_track(s, w, spec):
    """The coaster instance: its type (rail heights, sound scale), the samples, the state and the spacing."""
    w.type = s.new("type")
    s.put_all("type", w.type, {"mF50": spec.get("scale", 0x42C80000), "mF58": spec.get("flat", 0), "mF5C": spec.get("climb", 0)})
    w.samples = s.sample_block(spec["samples"])
    w.inst = s.new("inst")
    s.put("inst", w.inst, "mType", w.type)
    s.put("inst", w.inst, "mState", spec.get("state", 0))
    s.put("inst", w.inst, "mSamples", w.samples)
    s.put("inst", w.inst, "mSampleCount", spec["count"])
    s.put("inst", w.inst, "mFAC", spec.get("spacing", 0x3F800000))


def build_car(s, w, spec):
    """The car and its type (the node index and the car's length behind and ahead)."""
    ctype = s.new("ctype")
    s.put_all("ctype", ctype, {"mNodeIndex": spec.get("node", 0), "mF24": spec.get("ahead", 0), "mF28": spec.get("back", 0)})
    w.car = s.new("car")
    s.put("car", w.car, "mType", ctype)
    s.put("car", w.car, "mRide", spec.get("ride", 1))
    for i, v in enumerate(spec.get("dir", (0, 0, 0))):
        s.put("car", w.car, "mF10[%d]" % i, v)
    s.put("car", w.car, "mF40", spec.get("position", 0))


def build_arrays(s, w, spec=None):
    """The three 128-float arrays (the case's, or sentinels) and the centre word."""
    w.yaws, w.pitches, w.heights = (s.alloc(0x200) for _ in range(3))
    for i, a in enumerate((w.yaws, w.pitches, w.heights)):
        s.words(a, spec[("yaws", "pitches", "heights")[i]] if spec else [fb(1000 * (i + 1) + k) for k in range(128)])
    w.centre = s.alloc(16)
    s.words(w.centre, [0xDEADBEEF])


def build_set(s, w, ride, st):
    """A ride set: its mesh's header holds the live nodes and the mini nodes; mSelf is a master copy or the header itself."""
    live = s.new("node", len(st["nodes"]))
    for i, node in enumerate(st["nodes"]):
        a = s.at("node", live, i)
        s.put("node", a, "mFlags", node["flags"])
        s.put("node", a, "mVertexCount", len(node["verts"]))
        s.put("node", a, "mVertices", s.vertex_block(w, node["verts"], 4000))
    mini = s.new("mini", max(len(st["mini"]), 1))
    for i, flags in enumerate(st["mini"]):
        s.put("mini", s.at("mini", mini, i), "mFlags", flags)
    header = s.new("header")
    s.put("header", header, "mNodeCount", len(st["nodes"]))
    s.put("header", header, "mNodes", live)
    s.put("header", header, "mMiniNodes", mini)
    if st["master"] is None:
        s.put("header", header, "mSelf", header)
    else:
        nodes = s.new("node", len(st["master"]))
        for i, verts in enumerate(st["master"]):
            a = s.at("node", nodes, i)
            s.put("node", a, "mVertexCount", len(verts))
            s.put("node", a, "mVertices", s.vertex_block(w, verts, 6000))
        master = s.new("header")
        s.put("header", master, "mNodes", nodes)
        s.put("header", header, "mSelf", master)
    mesh = s.new("mesh")
    s.put("mesh", mesh, "mHeader", header)
    ride_set = s.new("set")
    s.put("set", ride_set, "mHeads", mesh)
    w.labels.append((mesh, L_MESH + ride))
    return ride_set


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    w = W()
    op, spec = case["op"], case.get("spec")
    if op == "interp":
        return w
    common_globals(s, case)
    if op in ("accumulate", "bend"):
        sine_globals(s, case)
        build_track(s, w, spec)
        build_car(s, w, spec)
        if op == "accumulate":
            build_arrays(s, w)
        else:
            s.ride_sets({ride: build_set(s, w, ride, st) for ride, st in spec["sets"].items()})
    elif op == "rotate":
        sine_globals(s, case)
        build_arrays(s, w, spec)
        w.node = s.new("node")
        w.source = s.vertex_block(w, spec["verts"], 4000)
        w.dest = w.source if spec["inplace"] else s.vertex_block(w, [], 6000, len(spec["verts"]))
    else:
        build_track(s, w, spec)
        build_car(s, w, spec)
        w.train = s.new("train")
        s.put_all("train", w.train, spec["train"])
        s.put("train", w.train, "mFirstPersonCar", w.car)
    return w


def execute(s, w, case):
    """The call."""
    op, spec = case["op"], case.get("spec")
    if op == "interp":
        return s.interp(*case["args"])
    if op == "accumulate":
        s.accumulate(w.inst, w.car, w.yaws, w.pitches, w.heights, case["capacity"], spec["position"], w.centre)
    elif op == "rotate":
        s.rotate(w.node, w.source, w.dest, len(spec["verts"]), w.yaws, w.pitches, w.heights, spec["centre"], spec["spacing"])
    elif op == "bend":
        s.bend(w.inst, w.car)
    elif op == "screams":
        s.screams(w.inst, w.train, *case["xyz"])
    else:
        s.sounds(w.inst, w.train)
    return None


TRAIN_WORDS = ["mFlags", "mF6C", "mF70", "mF64", "mF68"]


def observe(s, w, case, answer):
    """Everything the call may change, read back the same way on both sides; floats canon_f'd."""
    op = case["op"]
    if op == "interp":
        return {"answer": canon_f(answer)}
    out = {"log": s.log()}
    if op in ("accumulate", "rotate"):
        for name in ("yaws", "pitches", "heights"):
            out[name] = [canon_f(x) for x in s.rd_words(getattr(w, name), 128)]
    if op == "accumulate":
        out["centre"] = s.rd_word(w.centre)
    if op in ("rotate", "bend"):
        for i, (a, n) in enumerate(w.vblocks):
            out["vertices%d" % i] = [canon_f(x) for x in s.rd_words(a, n)]
    if op in ("screams", "sounds"):
        for name in TRAIN_WORDS:
            out[name] = s.get("train", w.train, name)
    return out


def run(s, case):
    try:
        w = build(s, case)
        s.begin(case, w.labels)
        return observe(s, w, case, execute(s, w, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


class Case(dict):
    """A case; its repr is short (the samples and arrays are long): dict.__repr__(case) shows everything."""

    def __repr__(self):
        spec = self.get("spec") or {}
        brief = {k: spec[k] for k in ("count", "base", "ba", "aa", "ride", "node", "inplace", "centre") if k in spec}
        rest = {k: v for k, v in self.items() if k not in ("op", "spec", "queues", "clamps")}
        return "Case(%s %s %s %s)" % (self["op"], brief, rest, {k: v for k, v in self["queues"].items() if k != "Rot"})


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def angle(r, special=0.0):
    """An angle's bits: inside (-pi, pi) mostly, now and then a bigger one, +-pi or a special value."""
    k = r.random()
    if k < special:
        return r.choice(SPECIAL)
    if k < special + 0.05:
        return fb(r.uniform(-100, 100))
    if k < special + 0.1:
        return r.choice([fb(PI32), fb(-PI32), 0, 0x80000000])
    return fb(r.uniform(-math.pi, math.pi))


def count_asked(m, spacing):
    """cw_cvt_fp2unsigned(m / spacing) + 1 in single precision."""
    return int(f32(m / spacing)) + 1


def split_k(centre, spacing, z):
    """The index RotateNodeVertexPosns takes for a vertex at z, or None when it would leave 0 to 126 (the arrays hold 128, k + 1 is read)."""
    q = f32(f32(f32(centre) * spacing) + z)
    q = f32(q / spacing)
    return int(q) if math.isfinite(q) and -1.0 < q < 127.0 else None


def vertex_z(r, centre, spacing, kmode=None):
    """A z that keeps k in 0 to 126; kmode picks the end of the range: negative (k 0 from a fraction below zero), first, last."""
    for _ in range(50):
        if kmode == "negative":
            q = -r.uniform(0.01, 0.99)
        elif kmode == "first":
            q = r.choice([0.0, r.random()])
        elif kmode == "last":
            q = 126 + r.choice([0.0, r.random(), 0.9999])
        else:
            q = r.randrange(0, 127) + r.choice([0.0, r.random(), r.random()])
        z = f32(q * spacing - f32(f32(centre) * spacing))
        if split_k(centre, spacing, z) is not None:
            return z
    return 0.0


# ---- InterpAngles
def interp_case(a, b, t):
    return Case(op="interp", args=[a, b, t], queues={})


def interp_edges():
    r = random.Random(1401)
    values = [0.0, -0.0, 1.0, -1.0, PI32, -PI32, 3.0, 2.5, -2.5, 1e-30, 1e30, math.inf, -math.inf, math.nan]
    for a in values:
        for b in values:
            for t in (0.0, 0.5, 1.0, r.random()):
                yield interp_case(fb(a), fb(b), fb(t))
    # the distance |a - b| below, at and just above pi_f, both ways round
    for a in (0.0, -0.0, 1.0, -1.0, 2.5, -2.5, 3.1, 0.1):
        for sign in (1, -1):
            for delta in (-2, -1, 0, 1, 2):
                b = (fb(a + sign * PI32) + delta) & MASK
                for t in (0.0, 0.5, 1.0, -0.5, 1.5, r.random()):
                    yield interp_case(fb(a), b, fb(t))
                    yield interp_case(b, fb(a), fb(t))
    for _ in range(40):
        yield interp_case(fb(r.uniform(-math.pi, math.pi)), fb(r.uniform(-math.pi, math.pi)), fb(r.uniform(-2, 3)))


def interp_random(r):
    k = r.random()
    if k < 0.7:
        a, b = angle(r), angle(r)
    elif k < 0.9:
        a, b = angle(r, 0.2), angle(r, 0.2)
    else:
        a, b = rf(r), rf(r)
    t = fb(r.uniform(0, 1)) if r.random() < 0.7 else rf(r)
    return interp_case(a, b, t)


# ---- RotateNodeVertexPosns
def rotate_case(r, count=None, inplace=None, centre=None, spacing=None, kmode=None, special=None):
    count = r.randint(0, 13) if count is None else count
    inplace = r.random() < 0.4 if inplace is None else inplace
    special = r.random() < 0.15 if special is None else special
    spacing = f32(spacing if spacing is not None else r.choice([1.0, 0.5, 2.0, r.uniform(0.05, 20)]))
    centre = r.randrange(0, 127) if centre is None else centre
    verts = []
    for _ in range(count):
        coords = [fb(r.uniform(-20, 20)) if not special or r.random() > 0.1 else rf(r) for _ in range(2)]
        verts.append((coords[0], coords[1], fb(vertex_z(r, centre, spacing, kmode))))
    spec = dict(verts=verts, inplace=inplace, centre=centre, spacing=fb(spacing))
    for name in ("yaws", "pitches"):
        spec[name] = [angle(r, 0.02 if special else 0.0) for _ in range(128)]
    spec["heights"] = [fb(r.uniform(-10, 10)) for _ in range(128)]
    return Case(op="rotate", spec=spec, queues={}, clamps=DEFAULT_CLAMPS)


def rotate_edges():
    r = random.Random(1402)
    for count in range(0, 14):
        for inplace in (False, True):
            yield rotate_case(r, count=count, inplace=inplace)
    for centre in (0, 1, 63, 126):
        for spacing in (1.0, 0.5, 2.0, 7.3, 0.07):
            yield rotate_case(r, count=r.randint(1, 13), centre=centre, spacing=spacing)
    for kmode in ("negative", "first", "last"):
        for centre in (0, 5, 126):
            yield rotate_case(r, count=r.randint(2, 9), centre=centre, kmode=kmode)
    for _ in range(12):
        yield rotate_case(r, count=r.randint(1, 13), special=True)


def rotate_random(r):
    return rotate_case(r, kmode=r.choice([None, None, None, None, "negative", "first", "last"]))


# ---- the track around a car (AccumulateBendAngles, BendCarToTrack)
def make_samples(r, n, spacing, walk, yawmode, flags):
    """n samples (flags, x, y, z, yaw) as bits: the track's place by `walk`, its yaw by `yawmode`."""
    p = [r.uniform(-30, 30) for _ in range(3)]
    yaw = r.uniform(-math.pi, math.pi)
    samples = []
    for i in range(n):
        if walk == "walk" or walk == "flat":
            d = [r.uniform(-1, 1), r.uniform(-1, 1) if walk == "walk" else 0.0, r.uniform(-1, 1)]
            norm = math.sqrt(sum(c * c for c in d)) or 1.0
            step = spacing * r.uniform(0.5, 1.5)
            p = [p[k] + d[k] / norm * step for k in range(3)]
        elif walk == "random":
            p = [r.uniform(-60, 60) for _ in range(3)]
        elif walk == "line":
            p[0] += spacing
        elif walk == "vertical":
            p[1] += spacing
        if yawmode == "uniform":
            yaw = r.uniform(-math.pi, math.pi)
        elif yawmode == "smooth":
            yaw = (yaw + r.uniform(-0.3, 0.3) + math.pi) % (2 * math.pi) - math.pi
        elif yawmode == "wrap":
            yaw = 0.9 * math.pi * (1 if i % 2 == 0 else -1)
        elif yawmode == "zero":
            yaw = 0.0
        fl = flags if flags is not None else ((8 if r.random() < 0.35 else 0) | (r.getrandbits(8) & 0xF7 if r.random() < 0.15 else 0))
        samples.append((fl, fb(p[0]), fb(p[1]), fb(p[2]), fb(yaw)))
    return samples


def direction(r):
    k = r.random()
    if k < 0.7:
        return [fb(r.uniform(-1, 1)) for _ in range(3)]
    if k < 0.92:
        return [fb(c) for c in r.choice([(1, 0, 0), (0, 0, 1), (0, 1, 0), (1, 1, 1), (-1, 0.5, -2), (0, 1, 1), (1, 1, 0), (0, -1, 0), (-1, 0, 0)])]
    return [0, 0, 0] if k < 0.96 else [fb(1e-30), fb(1e-30), fb(1e-30)]


def rot_queue(r):
    """The angles GetVectorRotationAroundYAxisFromZ answers: mostly -4 to 4, now and then a special or a huge one."""
    out = []
    for _ in range(130):
        k = r.random()
        if k < 0.03:
            out.append(r.choice(SPECIAL))
        elif k < 0.05:
            out.append(fb(r.uniform(-1e10, 1e10)))
        else:
            out.append(fb(r.uniform(-4, 4)))
    return out


def clamp_set(r, kind):
    if kind == "default":
        return DEFAULT_CLAMPS
    values = [fb(r.uniform(-1, 1)) for _ in range(8)]
    if kind == "inverted":
        values = [fb(0.5), fb(-0.5)] * 4          # lo above hi: the value ends at hi
    return values


def track_spec(r, count=None, base=None, frac=None, position=None, ba=None, aa=None, spacing=None, walk=None, yawmode=None, dirs=None,
               clamps=None, flags=None, back=None, ahead=None):
    """One track around a car: samples, spacing, the car's length behind and ahead (and the samples that asks for), its direction, the
    rail heights, the clamps and the position. count, base, frac, ba and aa pin the shape; the rest is random."""
    spacing = f32(spacing if spacing is not None else r.choice([1.0, 0.5, 2.0, r.uniform(0.05, 20), r.uniform(0.5, 3)]))
    count = r.randint(2, 70) if count is None else count
    if base is None:
        base = r.choice([0, count - 1, count - 2, r.randrange(count), r.randrange(count), r.randrange(count)])
    base = max(0, min(base, count - 1))
    frac = r.choice([0.0, 0.5, r.random(), r.random(), 0.999]) if frac is None else frac
    pos = f32(position) if position is not None else f32(base + frac)
    ba = (r.randint(1, 12) if r.random() < 0.8 else r.randint(1, 62)) if ba is None else ba
    aa = (r.randint(1, 12) if r.random() < 0.8 else r.randint(1, 62)) if aa is None else aa
    # the car's length: (n - 1 + u) spacings asks for n samples; the check below keeps what the function will really ask for
    back = f32((ba - 1 + r.uniform(0.05, 0.95)) * spacing) if back is None else back
    ahead = f32((aa - 1 + r.uniform(0.05, 0.95)) * spacing) if ahead is None else ahead
    asked_back, asked_ahead = count_asked(back, spacing), count_asked(ahead, spacing)
    assert asked_back + 1 + asked_ahead <= 128, (asked_back, asked_ahead)
    walk = r.choice(["walk", "walk", "walk", "flat", "random", "line", "vertical", "same"]) if walk is None else walk
    yawmode = r.choice(["uniform", "smooth", "smooth", "wrap", "zero"]) if yawmode is None else yawmode
    return dict(count=count, base=base, spacing=fb(spacing), samples=make_samples(r, count + 2, spacing, walk, yawmode, flags),
                flat=fb(r.uniform(0.2, 2)) if r.random() < 0.9 else rf(r), climb=fb(r.uniform(-2, 3)) if r.random() < 0.9 else rf(r),
                dir=dirs if dirs is not None else direction(r), back=fb(back), ahead=fb(ahead), position=fb(pos), ba=asked_back, aa=asked_ahead,
                node=0, ride=1)


def clamps_for(r, clamps):
    return clamp_set(r, clamps if clamps is not None else r.choice(["default", "default", "distinct", "inverted"]))


# ---- AccumulateBendAngles
def accumulate_case(r, clamps=None, capacity=None, **kw):
    spec = track_spec(r, **kw)
    cap = r.choice([0x80, 0x80, 0x80, 0, 1, r.getrandbits(32)]) if capacity is None else capacity
    return Case(op="accumulate", spec=spec, capacity=cap, clamps=clamps_for(r, clamps), queues={"Rot": rot_queue(r)})


ACCUMULATE_EDGES = [
    dict(base=0, frac=0.0), dict(base=0, frac=0.5), dict(count=2, base=0, frac=0.5), dict(count=2, base=1, frac=0.5), dict(count=2, base=1, frac=0.0),
    dict(count=3, base=2, frac=0.25), dict(count=3, base=1, frac=0.75), dict(position=-0.5), dict(position=-0.0), dict(position=0.0),
    dict(count=70, base=35, ba=5, aa=5), dict(count=70, base=35, ba=1, aa=1), dict(count=70, base=35, ba=2, aa=1), dict(count=70, base=35, ba=1, aa=2),
    dict(count=40, base=2, ba=10, aa=4), dict(count=40, base=37, ba=4, aa=10), dict(count=6, base=2, ba=20, aa=20), dict(count=70, base=35, ba=62, aa=62),
    dict(count=70, base=0, ba=62, aa=62), dict(count=70, base=69, ba=62, aa=62), dict(count=20, base=10, ba=10, aa=9), dict(count=20, base=10, ba=11, aa=10),
    dict(count=20, base=10, ba=12, aa=11), dict(count=20, base=9, ba=10, aa=10),
    dict(back=0.0), dict(ahead=0.0), dict(back=0.0, ahead=0.0), dict(back=-0.25, ahead=-0.25), dict(count=30, base=15, spacing=1.0, back=3.0, ahead=3.0),
    dict(spacing=0.05), dict(spacing=20.0), dict(spacing=0.5), dict(spacing=2.0), dict(spacing=1.0),
    dict(walk="walk"), dict(walk="flat"), dict(walk="random"), dict(walk="line"), dict(walk="vertical"), dict(walk="same"),
    dict(yawmode="uniform"), dict(yawmode="smooth"), dict(yawmode="wrap"), dict(yawmode="zero"),
    dict(flags=0), dict(flags=8), dict(flags=0xFF), dict(flags=0xF7),
    dict(dirs=[fb(1), 0, 0]), dict(dirs=[0, 0, fb(1)]), dict(dirs=[0, fb(1), 0]), dict(dirs=[0, 0, 0]), dict(dirs=[fb(1), fb(1), fb(1)]),
    dict(dirs=[fb(-1), fb(0.5), fb(-2)]), dict(dirs=[0, fb(1), fb(1)]), dict(dirs=[fb(1), fb(1), 0]), dict(dirs=[0x80000000, fb(1), 0x80000000]),
    dict(clamps="distinct"), dict(clamps="distinct"), dict(clamps="inverted"), dict(clamps="default"),
    dict(capacity=0), dict(capacity=0xFFFFFFFF),
]


WALKS = ["walk", "flat", "random", "line", "vertical", "same"]
YAWS = ["uniform", "smooth", "wrap", "zero"]
AXES = [(1, 0, 0), (0, 0, 1), (0, 1, 0), (1, 1, 1), (-1, 0.5, -2), (0, 1, 1), (1, 1, 0), (0, -1, 0), (-1, 0, 0), (0, 0, -1), (-1, -1, -1),
        (1e-30, 1e-30, 1e-30)]


def accumulate_edges():
    r = random.Random(1403)
    for kw in ACCUMULATE_EDGES:
        yield accumulate_case(r, **kw)
    for walk in WALKS:
        for yawmode in YAWS:
            for clamps in ("default", "distinct", "inverted"):
                yield accumulate_case(r, walk=walk, yawmode=yawmode, clamps=clamps)
    for axis in AXES:
        for clamps in ("default", "distinct"):
            yield accumulate_case(r, dirs=[fb(c) for c in axis], clamps=clamps)


def accumulate_random(r):
    return accumulate_case(r)


# ---- BendCarToTrack
def vertex(r, spacing, ba, aa):
    """A vertex whose z keeps RotateNodeVertexPosns inside the written entries (centre = ba, ba + 1 + aa written)."""
    for _ in range(50):
        z = f32(spacing * r.uniform(-ba + 0.1, aa - 0.1))
        k = split_k(ba, spacing, z)
        if k is not None and 0 <= k and k + 1 <= ba + aa:
            return (fb(r.uniform(-20, 20)), fb(r.uniform(-20, 20)), fb(z))
    return (fb(r.uniform(-20, 20)), fb(r.uniform(-20, 20)), 0)


def random_set(r, spacing, ba, aa, kind, inplace, nverts):
    """A ride set whose node under test is by `kind`: rotate (flag 0x200 clear), skip (set) or mini (a mini node, flag set)."""
    n_nodes = r.randint(1, 5)
    n_mini = r.randint(1, 3) if kind == "mini" else r.randint(0, 3)
    target = n_nodes + r.randrange(n_mini) if kind == "mini" else r.randrange(n_nodes)
    nodes, master = [], []
    for i in range(n_nodes):
        count = nverts if i == target and nverts is not None else r.randint(0, 13)
        pristine = [vertex(r, spacing, ba, aa) for _ in range(count)]
        flags = r.getrandbits(32) & ~0x200 & MASK
        if (i == target and kind == "skip") or (i != target and r.random() < 0.3):
            flags |= 0x200
        junk = [(fb(r.uniform(-50, 50)), fb(r.uniform(-50, 50)), fb(r.uniform(-50, 50))) for _ in range(count)]
        nodes.append(dict(flags=flags, verts=pristine if inplace else junk))
        master.append(pristine)
    mini = [r.getrandbits(32) | 0x200 for _ in range(n_mini)]
    return dict(nodes=nodes, mini=mini, master=None if inplace else master, target=target)


def bend_case(r, ride=None, kind=None, inplace=None, nverts=None, clamps=None, **kw):
    spec = track_spec(r, **kw)
    spacing = bitsf(spec["spacing"])
    ride = r.choice([0, 1, 2, 3, 1, 2, 3]) if ride is None else ride
    kind = kind or r.choice(["rotate", "rotate", "rotate", "skip", "mini"])
    inplace = r.random() < 0.4 if inplace is None else inplace
    spec["sets"] = {k: random_set(r, spacing, spec["ba"], spec["aa"], kind if k == ride else "rotate", inplace, nverts if k == ride else None)
                    for k in (1, 2, 3)}
    spec["ride"] = ride
    spec["node"] = spec["sets"][ride]["target"] if ride else 0
    spec["inplace"] = inplace
    return Case(op="bend", spec=spec, clamps=clamps_for(r, clamps), queues={"Rot": rot_queue(r)})


def bend_edges():
    r = random.Random(1404)
    yield bend_case(r, ride=0)
    yield bend_case(r, ride=0, kind="mini")
    for kind in ("rotate", "skip", "mini"):
        for inplace in (False, True):
            for ride in (1, 2, 3):
                yield bend_case(r, ride=ride, kind=kind, inplace=inplace)
    for nverts in range(0, 14):
        yield bend_case(r, ride=r.choice([1, 2, 3]), kind="rotate", nverts=nverts, inplace=nverts % 2 == 0)
    for kw in (dict(base=0, frac=0.0), dict(count=2, base=1, frac=0.5), dict(count=70, base=35, ba=3, aa=3), dict(count=70, base=35, ba=62, aa=62),
               dict(count=8, base=3, ba=20, aa=20), dict(spacing=0.05), dict(spacing=20.0), dict(clamps="distinct"), dict(clamps="inverted"),
               dict(dirs=[0, 0, 0]), dict(walk="same"), dict(walk="vertical")):
        yield bend_case(r, ride=r.choice([1, 2, 3]), kind="rotate", **kw)
    for walk in WALKS:
        for yawmode in YAWS:
            yield bend_case(r, ride=r.choice([1, 2, 3]), kind="rotate", walk=walk, yawmode=yawmode)
    for axis in AXES:
        yield bend_case(r, ride=r.choice([1, 2, 3]), kind="rotate", dirs=[fb(c) for c in axis])


def bend_random(r):
    return bend_case(r)


# ---- HandleScreams
def handles(r):
    """Three answers for a scream fake: distinct handles, now and then zero or negative."""
    return [r.choice([1000 + r.randrange(100), 1000 + r.randrange(100), 0, -3, 0x7FFFFFFF, 0x80000000]) for _ in range(3)]


def scream_case(r, combo=None, xyz=None):
    """combo: the bits 0 to 7 are train flag 0x100, train flag 0x10, sample flag 2, sample flag 4, state 0x40, recording, mF6C, mF70."""
    combo = r.getrandbits(8) if combo is None else combo
    f100, f10, s2, s4, st40, rec, h6c, h70 = ((combo >> i) & 1 for i in range(8))
    other = lambda keep: r.getrandbits(32) & ~keep & MASK if r.random() < 0.4 else 0
    count = r.randint(2, 6)
    index = r.randrange(count)
    flags = other(0x110) | (0x100 if f100 else 0) | (0x10 if f10 else 0)
    samples = [(r.getrandbits(32), 0, 0, 0, 0) for _ in range(count + 2)]
    samples[index] = (other(6) | (2 if s2 else 0) | (4 if s4 else 0), 0, 0, 0, 0)
    position = f32(index + r.choice([0.0, 0.25, 0.75, 0.999]))
    if index == 0 and r.random() < 0.15:
        position = -r.choice([0.0, 0.5, 0.999])
    train = {"mFlags": flags, "mF10": r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(1, 40)]),
             "mF6C": r.choice([1, 0x1234, 0xFFFFFFFF, r.getrandbits(32) | 1]) if h6c else 0,
             "mF70": r.choice([1, 0x4321, 0xFFFFFFFF, r.getrandbits(32) | 1]) if h70 else 0, "mPosition": fb(position)}
    spec = dict(count=count, samples=samples, state=other(0x40) | (0x40 if st40 else 0), train=train)
    xyz = xyz or [r.choice([0, 1, -1, 100, -100, 0x7FFFFFFF, -0x80000000, r.randrange(-100000, 100000)]) for _ in range(3)]
    queues = {"SCREAM_Single": handles(r), "SCREAM_Start": handles(r), "SCREAM_Move": handles(r)}
    return Case(op="screams", spec=spec, xyz=xyz, recording=r.choice([1, 2, r.getrandbits(32) | 1]) if rec else 0, queues=queues)


def screams_edges():
    r = random.Random(1405)
    for combo in range(256):
        yield scream_case(r, combo, [11, -22, 33])
    for combo in range(256):
        yield scream_case(r, combo)


def screams_random(r):
    return scream_case(r)


# ---- TickTrainSounds
def place(r):
    """A place word's bits: ordinary coordinates, now and then negative, huge or special."""
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL + [fb(3e9), fb(-3e9), fb(2147483648.0), fb(-2147483648.0), fb(2147483520.0)])
    if k < 0.2:
        return fb(r.uniform(-1e5, 1e5))
    return fb(r.uniform(-500, 500))


def sounds_case(r, same=None, recording=None, f10=None, scale=None, speed=None):
    same = r.random() < 0.5 if same is None else same
    mride = r.randrange(1, 5)
    answer = mride if same else r.choice([mride + 1, 0, 0xFFFFFFFF, mride + 2])
    recording = r.choice([0, 0, 0, 1, 2, r.getrandbits(32) | 1]) if recording is None else recording
    f10 = r.choice([0, 0, 1, 2, 5, 0xFFFFFFFF, r.getrandbits(32)]) if f10 is None else f10
    scale = r.choice([100.0, 50.0, 1.0, 250.0, 1e-3, 0.0, -80.0, math.nan, math.inf, r.uniform(1, 500)]) if scale is None else scale
    scale = f32(scale)
    speed = (r.choice([0.0, scale, scale * 0.5, scale * 1.3, scale * 2.0, -scale * 0.5, r.uniform(0, 1.5) * scale, 1e30, -1e30, 5.0, math.nan])
             if math.isfinite(scale) and r.random() < 0.9 else r.choice([0.0, 1.0, -1.0, math.inf, math.nan, r.uniform(-200, 200)])) if speed is None else speed
    count = r.randint(2, 5)
    index = r.randrange(count)
    flags = r.getrandbits(32) & ~0x110 & MASK
    if r.random() < 0.5:
        flags |= 0x10
    if r.random() < 0.5 and not (recording and f10):
        flags |= 0x100
    samples = [(r.getrandbits(32), 0, 0, 0, 0) for _ in range(count + 2)]
    train = {"mFlags": flags, "mF10": f10, "mF3C[0]": place(r), "mF3C[1]": place(r), "mF3C[2]": place(r), "mF48[0]": place(r), "mF48[1]": place(r),
             "mF48[2]": place(r), "mF54[1]": r.choice([0, 1, 7, 0xFFFFFFFF, r.getrandbits(32)]), "mF64": r.choice([0, r.getrandbits(32)]),
             "mF68": r.choice([0, r.getrandbits(32)]), "mF6C": r.choice([0, 0, r.getrandbits(32) | 1]), "mF70": r.choice([0, 0, r.getrandbits(32) | 1]),
             "mPosition": fb(f32(index + r.choice([0.0, 0.5, 0.999]))), "mF78": fb(speed)}
    spec = dict(count=count, samples=samples, state=r.choice([0, 0x40, r.getrandbits(32)]), scale=fb(scale), train=train, ride=mride)
    queues = {"GetFirstPersonRideID": [answer], "SOUND_SetParamValue": handles(r), "SOUND_MoveSoundEffect": handles(r),
              "SCREAM_Single": handles(r), "SCREAM_Start": handles(r), "SCREAM_Move": handles(r)}
    return Case(op="sounds", spec=spec, recording=recording, queues=queues)


def sounds_edges():
    r = random.Random(1406)
    for same in (True, False):
        for recording in (0, 1, 0xFFFF0000):
            for f10 in (0, 1, 7):
                for speed in (0.0, 50.0, 100.0, 150.0, -40.0, 1e30):
                    yield sounds_case(r, same=same, recording=recording, f10=f10, scale=100.0, speed=speed)
    for scale in (0.0, math.nan, math.inf, -100.0, 1e-3):
        for speed in (0.0, 1.0, -1.0, math.nan):
            yield sounds_case(r, scale=scale, speed=speed, recording=0, f10=r.choice([0, 3]))


def sounds_random(r):
    return sounds_case(r)


TESTS = [
    FnTest("InterpAngles", ppc, host, edge=interp_edges, rnd=interp_random),
    FnTest("AccumulateBendAngles", ppc, host, edge=accumulate_edges, rnd=accumulate_random),
    FnTest("RotateNodeVertexPosns", ppc, host, edge=rotate_edges, rnd=rotate_random),
    FnTest("BendCarToTrack", ppc, host, edge=bend_edges, rnd=bend_random),
    FnTest("HandleScreams", ppc, host, edge=screams_edges, rnd=screams_random),
    FnTest("TickTrainSounds", ppc, host, edge=sounds_edges, rnd=sounds_random),
]
