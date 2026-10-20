"""Unit w215, a coaster car's ride along the track, against libtpw_unit_CarRideTick.dylib (tests/unit_CarRideTick.cpp), main app
("SimTheme Park"), src/TPConfig_ridetick.cpp:
  CoasterCarRideTrackTick 0x1004C4F0, SetCarOrientation 0x1004AB8C, SetTrainSoundState 0x100473E4, HandleCollisions 0x1004C3C0,
  InterpolateCoasterSpline (the instance overload) 0x1004B0B0, PlayCollisionSound 0x10047510, GetLinearSamplePosn 0x1004AFA4,
  DistanceBetweenPoints (the reference overload) 0x1004F374

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian
words, 32-bit pointers) for the original, and in the host library's memory (little-endian, 64-bit pointers, the host structs' own offsets
from tpw_crt_layout) for the translation. A train's type word is the coaster type PLUS 0x2f0 on the Mac (the embedded train type) and
the type itself on the host; the order list of the tick (mTrainWords) holds the trains' addresses on the Mac and Ptr32 offsets from the
arena base on the host (unit w192's slots). The world of a case is made of optional pieces: the globals (the engine's sine table with
its mask and scale, the action recorder's flag, the sound bank, the tick's static byte and float), the ride anim sets 1 to 3 (set,
mesh, header, node with a 4x4 matrix of junk), two coaster types and three car types, a track (K = 4 samples before mSamples[0],
S + 2 after; two chains of Bezier controls, the second one twice as long, the first ended by a +inf knot; braces), an instance with its
trains (8 car slots each) and its order list (junk words and a guard after them), and buffers with guard words around them.

Every callee outside the unit is a fake on both sides that records its arguments (objects as labels, floats as canonical bits) and
answers from the case's queues: GetRideAnimTime ("time", the frame's clock), UpdateRideAnimSpeedImmediate, BendCarToTrack (also the
car's mF40 at the call), the sample overload of InterpolateCoasterSpline 0x1004E318 (it logs its nine arguments, the ninth on the stack,
and writes the point and the orientation from the controls it is given: point = (a.mF40.x, t, b.mF40.z), orientation = (t, c.mF0C,
d.mF0C, a.mF0C, b.mF0C, c.mF08, d.mF08), so the chain walk shows), SOUND_SetParamValue ("handle") and SOUND_SendEvent ("send"). modf and
fmod are the glue imports (C semantics). The matrix helpers (SetMatrixIdentity and the three rotates) and __cvt_fp2unsigned run as the
original on the PowerPC side and are the project's real ones on the host.

Compared: the answer (DistanceBetweenPoints), the call log, and a snapshot of the world after: every field of every object the case
built (a pointer as a label or as an index into its array, floats canonical, the Ptr32 words decoded), the order list, the nodes'
matrices, the buffers with their guards, the car's mF10 and the instance's orientation block, the globals the code writes.

Fused operations (fmadds and relatives, fctiwz saturation) run under HardwareRounding over the eight functions and the helpers.
Not tested (the original crashes, loops for ever or reads out of range there): a NaN, infinite or out-of-range place into the index
math, an infinite or negative nose where the collision sound can play, a centre car without flag 1, a negative or NaN train speed or a
zero distance in the tick (the half-sample step never ends), a divisor mF2F4 of 0 (divwu), mBankFadeStart equal to mLength (a zero
stretch), mLength below 1 or past the samples, ride 0 in SetCarOrientation (gRideAnimSets[0] is NULL), a NULL point for the sample
overload's output.
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
SHIM_LIBRARY = "unit_CarRideTick"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); the helpers run as the original too
TICK, ORIENT, SOUND, COLLIDE, INTERP, PLAY, LINEAR, DISTANCE = 0x1004C4F0, 0x1004AB8C, 0x100473E4, 0x1004C3C0, 0x1004B0B0, 0x10047510, 0x1004AFA4, 0x1004F374
SIZES = {TICK: 3512, ORIENT: 760, SOUND: 236, COLLIDE: 224, INTERP: 224, PLAY: 212, LINEAR: 96, DISTANCE: 92}
OWN = set(SIZES)
HELPERS = {0x100C0E28: 84, 0x100C13BC: 100, 0x100C1698: 124, 0x100C15F0: 112, 0x10237CD8: 76}   # SetMatrixIdentity, the three rotates, __cvt_fp2unsigned
RANGES = dict(SIZES)
RANGES.update(HELPERS)

# ---- the callees faked on both sides
GET_TIME, ANIM_SPEED, BEND, SAMPLE_INTERP, SET_PARAM, SEND_EVENT = 0x100C3268, 0x100C7B58, 0x1004688C, 0x1004E318, 0x100DD380, 0x100DD9FC

# ---- the Mac's addresses of the globals; the host library's come from tpw_crt_addr(name)
ADDR = dict(sin_mask=0x103B74CC, sin_scale=0x103B74D0, sin_table=0x103B74D4, recording=0x1037EC68, rideanim=0x102C79A4,
            bank2=0x102F7C9C, init=0x102B746C, max=0x102B7470)
TYPE_OFF = 0x2F0          # a Mac train's type word: the coaster type plus this
K = 4                     # samples before mSamples[0] (negative sample numbers read valid memory)
SLOTS = 8                 # car slots per train
GUARD_A, GUARD_B = 0xA5A5A5A5, 0x5A5A5A5A
JUNK = 0x3E000000

# struct -> ([(field, Mac offset, kind)], Mac size). Kinds: w word, f float (compared as canonical bits), p pointer, t the train's type word,
# q a Ptr32 word (the Mac's plain address, the host's offset from the arena base). The host's offsets come from tpw_crt_layout in the
# same order, each struct's size last.
FIELDS = {
    "car": ([("mFlags", 0x0, "w"), ("mType", 0x4, "p"), ("mIndex", 0x8, "w"), ("mRide", 0xc, "w"), ("mF10[0]", 0x10, "f"), ("mF10[1]", 0x14, "f"), ("mF10[2]", 0x18, "f"), ("mF1C", 0x1c, "w"), ("mHeadsA", 0x20, "p"), ("mHeadsB", 0x24, "p"), ("mCurrentA", 0x28, "p"), ("mCurrentB", 0x2c, "p"), ("mCountA", 0x30, "w"), ("mCountB", 0x34, "w"), ("mF38", 0x38, "f"), ("mF3C", 0x3c, "w"), ("mF40", 0x40, "f"), ("mF44", 0x44, "w"), ("mF48[0]", 0x48, "f"), ("mF48[1]", 0x4c, "f"), ("mF48[2]", 0x50, "f"), ("mF54[0]", 0x54, "f"), ("mF54[1]", 0x58, "f"), ("mF54[2]", 0x5c, "f")], 0x60),
    "train": ([("mFlags", 0x0, "w"), ("mType", 0x4, "t"), ("mF08", 0x8, "w"), ("mCarsInUse", 0xc, "w"), ("mF10", 0x10, "w"), ("mF14", 0x14, "w"), ("mF18", 0x18, "w"), ("mF1C", 0x1c, "w"), ("mFrontSample", 0x20, "f"), ("mRearSample", 0x24, "f"), ("mCars", 0x28, "p"), ("mCentreCar", 0x2c, "w"), ("mF30", 0x30, "w"), ("mFirstPersonCar", 0x34, "p"), ("mF38", 0x38, "w"), ("mF3C[0]", 0x3c, "f"), ("mF3C[1]", 0x40, "f"), ("mF3C[2]", 0x44, "f"), ("mF48[0]", 0x48, "f"), ("mF48[1]", 0x4c, "f"), ("mF48[2]", 0x50, "f"), ("mF54[0]", 0x54, "w"), ("mF54[1]", 0x58, "w"), ("mF54[2]", 0x5c, "w"), ("mF54[3]", 0x60, "w"), ("mF64", 0x64, "w"), ("mF68", 0x68, "w"), ("mF6C", 0x6c, "w"), ("mF70", 0x70, "w"), ("mPosition", 0x74, "f"), ("mF78", 0x78, "f"), ("mF7C", 0x7c, "f")], 0x80),
    "inst": ([("mFlags", 0x0, "w"), ("mType", 0x4, "p"), ("mF2C", 0x2c, "f"), ("mSpeedLimit", 0x34, "f"), ("mBlock3C", 0x3c, "w"), ("mF78", 0x78, "f"), ("mF7C", 0x7c, "f"), ("mF80", 0x80, "f"), ("mF84", 0x84, "f"), ("mF88", 0x88, "f"), ("mF8C", 0x8c, "f"), ("mF90", 0x90, "f"), ("mSamples", 0x9c, "p"), ("mSampleCount", 0xa4, "w"), ("mLength", 0xa8, "f"), ("mFAC", 0xac, "f"), ("mTrainCount", 0xf4, "w"), ("mFFC", 0xfc, "f"), ("mF120", 0x120, "w"), ("mLeadTrain", 0x124, "p"), ("mTrains", 0x128, "p"), ("mTrainWords", 0x12c, "p"), ("mCars", 0x130, "p"), ("mBankFadeStart", 0x134, "f")], 0x150),
    "type": ([("mFlags", 0x0, "w"), ("mF24", 0x24, "f"), ("mF28", 0x28, "f"), ("mF2C", 0x2c, "f"), ("mF44", 0x44, "f"), ("mF48", 0x48, "f"), ("mF54", 0x54, "f"), ("mF2EC", 0x2ec, "w"), ("mF2F4", 0x2f4, "w")], 0x31c),
    "ctype": ([("mFlags", 0x0, "w"), ("mF18", 0x18, "f"), ("mF1C", 0x1c, "f"), ("mF24", 0x24, "f"), ("mF28", 0x28, "f")], 0x34),
    "set": ([("mHeads", 0x8, "p")], 0xd0),
    "mesh": ([("mHeader", 0x4, "p")], 0x40),
    "hdr": ([("mF78", 0x78, "q")], 0x100),
    "node": ([("mMatrix.m[%d][%d]" % (i // 4, i % 4), 0x10 + 4 * i, "f") for i in range(16)], 0x100),
    "brace": ([("mFlags", 0x8, "w")], 0x110),
    "ctl": ([("mF08", 0x8, "f"), ("mF0C", 0xc, "f"), ("mBrace", 0x10, "p"), ("mNext", 0x18, "p"), ("mF40.x", 0x40, "f"), ("mF40.y", 0x44, "f"), ("mF40.z", 0x48, "f")], 0x58),
    "sample": ([("mFlags", 0x0, "w"), ("mPos.x", 0x4, "f"), ("mPos.y", 0x8, "f"), ("mPos.z", 0xc, "f"), ("mSpeed", 0x1c, "f"), ("mPiece", 0x2c, "p"), ("mPieceB", 0x30, "p")], 0x38),
}
ORDER = ["car", "train", "inst", "type", "ctype", "set", "mesh", "hdr", "node", "brace", "ctl", "sample"]
KINDS = {st: {n: k for n, _, k in FIELDS[st][0]} for st in FIELDS}
MACOFF = {st: {n: o for n, o, _ in FIELDS[st][0]} for st in FIELDS}
HOST = {}          # struct -> {field: offset}, filled by bind
HOST_SIZE = {}
# The host's offsets and sizes (native pointers) per struct of ORDER: each field's offset in table order, then the size. bind checks them
# once against tpw_crt_layout, so a change of a host struct shows as a failed bind, not as a silent shift.
HOST_LAYOUT = [
    0, 8, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120,   # car
    0, 8, 16, 20, 24, 28, 32, 36, 40, 44, 48, 56, 60, 64, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,   # train
    0, 8, 64, 72, 88, 156, 160, 164, 168, 172, 176, 180, 192, 204, 208, 212, 300, 308, 344, 352, 360, 368, 376, 384, 416,   # inst
    0, 44, 48, 52, 72, 76, 88, 820, 828, 944,   # type
    0, 24, 28, 48, 52, 64,   # ctype
    16, 384,   # set
    8, 112,   # mesh
    120, 184,   # hdr
    16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 160,   # node
    8, 288,   # brace
    8, 12, 16, 24, 64, 68, 72, 88,   # ctl
    0, 4, 8, 12, 28, 48, 56, 72,   # sample
]


def fb(x):
    """The single-precision bits of a Python number (a value past the single range is the infinity)."""
    try:
        return struct.unpack(">I", struct.pack(">f", x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions and the helpers they call (Unicorn rounds fused operations twice and saturates
    fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in RANGES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(HELPERS)


def stub_time(e):
    """uint32_t GetRideAnimTime(int32_t which): the case's clock."""
    e.cl.rec("GetRideAnimTime", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("time") & MASK)


def stub_anim_speed(e):
    """void UpdateRideAnimSpeedImmediate(set, float speed, int32_t flag): the flag is in r5 (the float shadows r4)."""
    L = e.cl
    L.rec("UpdateRideAnimSpeedImmediate", (L.lab(e.r(3)), canon_f(fb(e.f(1))), s32(e.r(5))))


def stub_bend(e):
    """void BendCarToTrack(instance, car): also the car's mF40 at the call (the order of the tick's stores)."""
    L = e.cl
    L.rec("BendCarToTrack", (L.lab(e.r(3)), L.lab(e.r(4)), canon_f(e.u32(e.r(4) + 0x40))))


def stub_sample_interp(e):
    """The sample overload of InterpolateCoasterSpline: r3 samples, r4 params, r5..r8 the controls a b c d, f1 t, r10 the point, the
    orientation on the stack (0x38(r1))."""
    L = e.cl
    a, b, c, d = e.r(5), e.r(6), e.r(7), e.r(8)
    t = fb(e.f(1))
    point, orient = e.r(10), e.u32(e.r(1) + 0x38)
    L.rec("InterpolateCoasterSpline", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(a), L.lab(b), L.lab(c), L.lab(d), canon_f(t), L.lab(point), L.lab(orient)))
    e.write(point, struct.pack(">3I", e.u32(a + 0x40), t, e.u32(b + 0x48)))
    if orient:
        e.write(orient, struct.pack(">7I", t, e.u32(c + 0xC), e.u32(d + 0xC), e.u32(a + 0xC), e.u32(b + 0xC), e.u32(c + 0x8), e.u32(d + 0x8)))


def stub_set_param(e):
    """int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value)."""
    e.cl.rec("SOUND_SetParamValue", (s32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, e.cl.ret("handle") & MASK)


def stub_send(e):
    """int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t x, int32_t y, int32_t z)."""
    e.cl.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, e.cl.ret("send") & MASK)


def stub_modf(e):
    """double modf(double x, double* whole): the pointer is in r5 (the double shadows r3 and r4)."""
    frac, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)


def stub_fmod(e):
    """double fmod(double x, double y) with C semantics."""
    x, y = e.f(1), e.f(2)
    e.set_f1(math.nan if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0 else math.fmod(x, y))


STUBS.internal(GET_TIME, stub_time)
STUBS.internal(ANIM_SPEED, stub_anim_speed)
STUBS.internal(BEND, stub_bend)
STUBS.internal(SAMPLE_INTERP, stub_sample_interp)
STUBS.internal(SET_PARAM, stub_set_param)
STUBS.internal(SEND_EVENT, stub_send)
STUBS.imp("modf", stub_modf)
STUBS.imp("fmod", stub_fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)       # sqrt, fabs, memset ...


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, struct.pack(self.fmt[0] + "%dI" % len(ws), *[x & MASK for x in ws]))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def put(self, st, addr, name, v):
        kind, off = KINDS[st][name], self.off(st, name)
        if kind == "t" and self.mac and v:
            v += TYPE_OFF
        if kind in "pt":
            self.write(addr + off, self.pack_ptr(v))
        elif kind == "q":
            self.write(addr + off, struct.pack(self.fmt, self.q_encode(v) & MASK))
        else:
            self.write(addr + off, struct.pack(self.fmt, v & MASK))

    def get(self, st, addr, name):
        kind, off = KINDS[st][name], self.off(st, name)
        if kind in "pt":
            v = self.unpack_ptr(self.read(addr + off, self.psize))
            return v - TYPE_OFF if kind == "t" and self.mac and v else v
        word = struct.unpack(self.fmt, self.read(addr + off, 4))[0]
        return self.q_decode(word) if kind == "q" else word

    def new(self, st, label=None, count=1):
        a = self.alloc(self.size(st) * count)
        if label is not None:
            self.label(label, a)
        return a

    def describe(self, w, v):
        """A pointer as 0, a label, or (array, index, offset) when it points into one of the case's arrays."""
        if v == 0:
            return 0
        lab = self.lab(v)
        if lab > 0:
            return ("L", lab)
        for name, base, stride, count in w.regions:
            if base <= v < base + stride * count:
                return (name, (v - base) // stride, (v - base) % stride)
        return ("?", lab)

    def order_word(self, x):
        """A word of the order list: a train as its label, anything else (junk, the guard, 0) as it is."""
        a = self.q_decode(x)
        lab = self.lab(a) if a else 0
        return ("L", lab) if lab > 0 else x


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    mac, psize, fmt = True, 4, ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, st, name):
        return MACOFF[st][name]

    def size(self, st):
        return FIELDS[st][1]

    def alloc(self, n):
        return self.e.alloc(max(n, 4), bytes(max(n, 4)))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, l, a):
        self.L.labels[a] = l

    def lab(self, a):
        return self.L.lab(a)

    def log(self):
        return self.L.log

    def pack_ptr(self, v):
        return struct.pack(">I", v & MASK)

    def unpack_ptr(self, raw):
        return struct.unpack(">I", raw)[0]

    def q_encode(self, a):
        return a

    def q_decode(self, w):
        return w

    def set_ride(self, i, a):
        self.write(ADDR["rideanim"] + 4 * i, self.pack_ptr(a))

    def call(self, entry, args):
        with ROUNDING(self.e):
            return T.call_ppc(self.e, entry, args)

    def distance(self, a, b):
        _, _, f1 = self.call(DISTANCE, [a, b])
        return canon_f(fb(f1))

    def linear(self, inst, sample, t, out):
        self.call(LINEAR, [inst, sample & MASK, T.F(bitsf(t)), out])

    def collide(self, inst, train):
        self.call(COLLIDE, [inst, train])

    def play(self, inst, train):
        self.call(PLAY, [inst, train])

    def sound(self, train, state, value):
        self.call(SOUND, [train, state & MASK, value & MASK])

    def interp(self, inst, position, point, orient):
        self.call(INTERP, [inst, T.F(bitsf(position)), point, orient])

    def orient(self, car, orient, point):
        self.call(ORIENT, [car, orient, point])

    def tick(self, inst):
        self.call(TICK, [inst])


class Host(Side):
    """The same on the host library."""
    mac, psize, fmt = False, 8, "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_crt_arena_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_crt_addr(name.encode())

    def off(self, st, name):
        return HOST[st][name]

    def size(self, st):
        return HOST_SIZE[st]

    def alloc(self, n):
        return self.lib.tpw_crt_alloc(max(n, 16))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, l, a):
        self.lib.tpw_cl_label(l, a)

    def lab(self, a):
        return self.lib.tpw_cl_lab(a) if a else 0

    def log(self):
        return CL.host_end(self.lib)

    def pack_ptr(self, v):
        return struct.pack("<Q", v)

    def unpack_ptr(self, raw):
        return struct.unpack("<Q", raw)[0]

    def q_encode(self, a):
        return a - self.base if a else 0

    def q_decode(self, w):
        return self.base + w if w else 0

    def set_ride(self, i, a):
        self.write(self.g("rideanim") + 8 * i, self.pack_ptr(a))

    def distance(self, a, b):
        return canon_f(self.lib.tpw_crt_Distance(a, b))

    def linear(self, inst, sample, t, out):
        self.lib.tpw_crt_Linear(inst, s32(sample), t & MASK, out)

    def collide(self, inst, train):
        self.lib.tpw_crt_Collide(inst, train)

    def play(self, inst, train):
        self.lib.tpw_crt_Play(inst, train)

    def sound(self, train, state, value):
        self.lib.tpw_crt_SoundState(train, state & MASK, value & MASK)

    def interp(self, inst, position, point, orient):
        self.lib.tpw_crt_Interp(inst, position & MASK, point, orient)

    def orient(self, car, orient, point):
        self.lib.tpw_crt_Orient(car, orient, point)

    def tick(self, inst):
        self.lib.tpw_crt_Tick(inst)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_crt_addr.restype, lib.tpw_crt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_crt_alloc.restype, lib.tpw_crt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_crt_arena_base.restype = C.c_uint64
    lib.tpw_crt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crt_Distance.restype, lib.tpw_crt_Distance.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_crt_Linear.argtypes = [C.c_uint64, C.c_int32, C.c_uint32, C.c_uint64]
    lib.tpw_crt_Collide.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_crt_Play.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_crt_SoundState.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_crt_Interp.argtypes = [C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64]
    lib.tpw_crt_Orient.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_crt_Tick.argtypes = [C.c_uint64]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    out = (C.c_int64 * 512)()
    n = lib.tpw_crt_layout(out)
    got, i = list(out)[:n], 0
    for st in ORDER:
        HOST[st] = {}
        for name, _, _ in FIELDS[st][0]:
            HOST[st][name] = got[i]
            i += 1
        HOST_SIZE[st] = got[i]
        i += 1
    assert i == n and got == HOST_LAYOUT, got
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


(L_INST, L_PARAMS, L_ORIENT, L_SAMPLES, L_ORDER) = 1, 2, 3, 4, 5
L_TRAIN, L_TYPE, L_CTYPE, L_CAR = 10, 16, 20, 50
L_BRACE, L_CTLA, L_CTLB = 100, 200, 300
L_NODE, L_HDR, L_MESH, L_SET = 400, 410, 420, 430
L_BUF = 500


class W:
    """The objects of a case: what the snapshot reads and the arrays the pointers are described by."""

    def __init__(self):
        self.objs, self.regions, self.bufs, self.bufaddr = [], [], [], []
        self.order, self.order_n = 0, 0
        self.inst = self.trains = self.car = self.train = self.samples = 0


_SINE = {}


def sine_words(seed):
    if seed not in _SINE:
        r = random.Random(seed)
        _SINE[seed] = [fb(r.uniform(-1.0, 1.0)) for _ in range(0x1000)]
    return _SINE[seed]


def put_fields(s, st, addr, values):
    """The listed fields from `values`, junk in every other word field (an unwanted store shows), pointers 0."""
    for name in values:
        assert name in KINDS[st], (st, name)
    for name, off, kind in FIELDS[st][0]:
        if name in values:
            s.put(st, addr, name, values[name])
        elif kind in "wf":
            s.put(st, addr, name, JUNK + (ORDER.index(st) << 12) + off)


def reg_array(s, w, st, base, count, tag, label=None):
    stride = s.size(st)
    w.regions.append((tag, base, stride, count))
    for i in range(count):
        a = base + i * stride
        w.objs.append(("%s%d" % (tag, i), st, a))
        if label is not None:
            s.label(label + i, a)


def build_globals(s, g):
    s.words(s.g("sin_mask"), [g["mask"]])
    s.words(s.g("sin_scale"), [g["scale"]])
    s.words(s.g("sin_table"), sine_words(g["sin"]))
    s.words(s.g("recording"), [g["recording"]])
    s.words(s.g("bank2"), [g["bank2"]])
    s.write(s.g("init"), bytes([g["init"]]))
    s.words(s.g("max"), [g["max"]])


def build_rides(s, w, mats):
    """Ride anim sets 1..len(mats): set -> mesh -> header -> node, the node's matrix junk."""
    for r, mat in enumerate(mats, 1):
        node = s.new("node", L_NODE + r)
        put_fields(s, "node", node, {"mMatrix.m[%d][%d]" % (i // 4, i % 4): mat[i] for i in range(16)})
        hdr = s.new("hdr", L_HDR + r)
        put_fields(s, "hdr", hdr, {"mF78": node})
        mesh = s.new("mesh", L_MESH + r)
        put_fields(s, "mesh", mesh, {"mHeader": hdr})
        st = s.new("set", L_SET + r)
        put_fields(s, "set", st, {"mHeads": mesh})
        s.set_ride(r, st)
        for tag, kind, a in (("set", "set", st), ("mesh", "mesh", mesh), ("hdr", "hdr", hdr), ("node", "node", node)):
            w.objs.append(("%s%d" % (tag, r), kind, a))


def build_types(s, w, sp):
    w.ctypes, w.types = [], []
    for i, c in enumerate(sp["ctypes"]):
        a = s.new("ctype", L_CTYPE + i)
        put_fields(s, "ctype", a, c)
        w.ctypes.append(a)
        w.objs.append(("ctype%d" % i, "ctype", a))
    for i, t in enumerate(sp["types"]):
        a = s.new("type", L_TYPE + i)
        put_fields(s, "type", a, t)
        w.types.append(a)
        w.objs.append(("type%d" % i, "type", a))


def build_track(s, w, tr):
    """Braces, the two chains of controls (a chain's last next is NULL) and the samples (K before mSamples[0])."""
    bs, cs, ss = s.size("brace"), s.size("ctl"), s.size("sample")
    nb, na, nbb, n = len(tr["braces"]), len(tr["ctla"]), len(tr["ctlb"]), len(tr["samples"])
    braces = s.alloc(bs * nb)
    reg_array(s, w, "brace", braces, nb, "brace", L_BRACE)
    for i, flags in enumerate(tr["braces"]):
        put_fields(s, "brace", braces + i * bs, {"mFlags": flags})
    ctla, ctlb = s.alloc(cs * na), s.alloc(cs * nbb)
    reg_array(s, w, "ctl", ctla, na, "ctla", L_CTLA)
    reg_array(s, w, "ctl", ctlb, nbb, "ctlb", L_CTLB)
    for base, chain in ((ctla, tr["ctla"]), (ctlb, tr["ctlb"])):
        for i, c in enumerate(chain):
            vals = dict(c)
            vals["mBrace"] = braces + vals.pop("brace") * bs
            vals["mNext"] = base + (i + 1) * cs if i + 1 < len(chain) else 0
            put_fields(s, "ctl", base + i * cs, vals)
    sbase = s.alloc(ss * n)
    reg_array(s, w, "sample", sbase, n, "samples")
    for i, sm in enumerate(tr["samples"]):
        vals = dict(sm)
        vals["mPiece"] = ctla + vals.pop("a") * cs
        vals["mPieceB"] = ctlb + vals.pop("b") * cs
        put_fields(s, "sample", sbase + i * ss, vals)
    w.samples = sbase + K * ss
    s.label(L_SAMPLES, w.samples)


def build_trains(s, w, sp):
    """The instance's trains (one block), their cars (SLOTS each, one block) and the order list (junk words, then a guard)."""
    nT = len(sp["trains"])
    ts, cs = s.size("train"), s.size("car")
    w.trains = s.alloc(ts * nT)
    reg_array(s, w, "train", w.trains, nT, "train", L_TRAIN)
    cars = s.alloc(cs * nT * SLOTS) if "cars" in sp else 0
    if cars:
        reg_array(s, w, "car", cars, nT * SLOTS, "car", L_CAR)
    for t, tr in enumerate(sp["trains"]):
        vals = dict(tr)
        ty = vals.pop("ty", None)
        vals["mType"] = w.types[ty] if ty is not None else 0
        vals["mCars"] = cars + t * SLOTS * cs if cars else 0
        put_fields(s, "train", w.trains + t * ts, vals)
        if cars:
            for j, car in enumerate(sp["cars"][t]):
                cv = dict(car)
                cv["mType"] = w.ctypes[cv.pop("ct")]
                put_fields(s, "car", cars + (t * SLOTS + j) * cs, cv)
    order = s.alloc(4 * (nT + 1))
    s.label(L_ORDER, order)
    s.words(order, [0xCAFE0000 + i for i in range(nT)] + [0xDEADBEEF])
    w.order, w.order_n = order, nT + 1
    return cars


def build_buf(s, w, i, words):
    """A buffer of words with four guard words on each side; the label and the address are of the first payload word."""
    n = len(words)
    base = s.alloc(4 * (n + 8))
    s.words(base, [GUARD_A] * 4 + list(words) + [GUARD_B] * 4)
    s.label(L_BUF + i, base + 16)
    w.bufs.append(("buf%d" % i, base, n + 8))
    w.bufaddr.append(base + 16)


def build(s, case):
    sp = case["spec"]
    w = W()
    build_globals(s, sp["g"])
    if "rides" in sp:
        build_rides(s, w, sp["rides"])
    if "ctypes" in sp:
        build_types(s, w, sp)
    if "car" in sp:
        w.car = s.new("car", L_CAR)
        put_fields(s, "car", w.car, sp["car"])
        w.objs.append(("car", "car", w.car))
    if "train" in sp:
        w.train = s.new("train", L_TRAIN)
        put_fields(s, "train", w.train, sp["train"])
        w.objs.append(("train", "train", w.train))
    if "inst" in sp:
        w.inst = s.new("inst", L_INST)
        s.label(L_PARAMS, w.inst + s.off("inst", "mBlock3C"))
        s.label(L_ORIENT, w.inst + s.off("inst", "mF78"))
        vals = dict(sp["inst"])
        if "track" in sp:
            build_track(s, w, sp["track"])
            vals["mSamples"] = w.samples
        if "trains" in sp:
            cars = build_trains(s, w, sp)
            ts = s.size("train")
            vals["mTrains"] = w.trains if sp.get("mTrains", True) else 0
            vals["mLeadTrain"] = w.trains + (len(sp["trains"]) - 1) * ts if sp["trains"] else 0
            vals["mTrainWords"] = w.order
            vals["mCars"] = cars
        if "types" in sp:
            vals["mType"] = w.types[0]
        put_fields(s, "inst", w.inst, vals)
        w.objs.append(("inst", "inst", w.inst))
    for i, words in enumerate(sp.get("bufs", [])):
        build_buf(s, w, i, words)
    return w


def execute(s, w, case):
    op, a = case["op"], case["args"]
    ts = s.size("train") if w.trains else 0
    if op == "distance":
        return s.distance(w.bufaddr[0], w.bufaddr[1])
    if op == "linear":
        s.linear(w.inst, a["sample"], a["t"], w.bufaddr[0])
    elif op == "collide":
        s.collide(w.inst, w.trains + a["train"] * ts)
    elif op == "play":
        s.play(w.inst, w.trains + a["train"] * ts)
    elif op == "sound":
        s.sound(w.train, a["state"], a["value"])
    elif op == "interp":
        point = w.bufaddr[0]
        orient = {"none": 0, "inst": w.inst + s.off("inst", "mF78"), "buf": w.bufaddr[1] if len(w.bufaddr) > 1 else 0}[a["orient"]]
        s.interp(w.inst, a["pos"], point, orient)
    elif op == "orient":
        s.orient(w.car, w.bufaddr[0], w.bufaddr[1] if a["point"] else 0)
    else:
        s.tick(w.inst)
    return 0


def fields_of(s, w, st, addr):
    out = {}
    for name, _, kind in FIELDS[st][0]:
        v = s.get(st, addr, name)
        out[name] = s.describe(w, v) if kind in "ptq" else (canon_f(v) if kind == "f" else v)
    return out


def observe(s, w, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    snap = {tag: fields_of(s, w, st, a) for tag, st, a in w.objs}
    return {
        "answer": answer,
        "snap": snap,
        "bufs": {tag: [canon_f(x) if 4 <= i < n - 4 else x for i, x in enumerate(s.rd_words(a, n))] for tag, a, n in w.bufs},
        "order": [s.order_word(x) for x in s.rd_words(w.order, w.order_n)] if w.order else [],
        "globals": {"init": s.read(s.g("init"), 1)[0], "max": canon_f(s.rd_word(s.g("max"))), "recording": s.rd_word(s.g("recording")),
                    "bank2": s.rd_word(s.g("bank2"))},
        "log": s.log(),
    }


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, w, case, execute(s, w, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


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


def rangle(r):
    """An angle in radians (bits): the usual range, every quadrant, huge values for the table index's saturation, now and then NaN or inf."""
    k = r.random()
    if k < 0.02:
        return r.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 0])
    if k < 0.6:
        return fb(r.uniform(-7.0, 7.0))
    if k < 0.75:
        return fb(r.choice([-1, 1]) * r.uniform(7.0, 100.0))
    if k < 0.85:
        return fb(r.choice([-1, 1]) * 10 ** r.uniform(8.0, 12.0))
    return fb(r.choice([-3, -2, -1, 0, 1, 2, 3, 4]) * math.pi / 2)


def gen_globals(r, rec=None):
    return {"mask": r.choice([0xFFF] * 7 + [0x3FF, 0x1FF, 0x7FF, 0xFF]),
            "scale": r.choice([fb(4096.0 / (2.0 * math.pi))] * 7 + [fb(1000.0), fb(100.0), fb(651.0), fb(1.0), 0, fb(-300.0)]),
            "sin": r.randrange(6), "recording": r.choice([0, 0, 0, 1, -1]) if rec is None else rec,
            "bank2": r.randrange(1, 9), "init": r.choice([0, 1]), "max": r.choice([0, fb(1.0), 0x7F7FFFFF, 0x7F800000, 0x7FC00000])}


def gen_queues(r, now=0):
    return {"time": [now & MASK], "handle": [r.randrange(1, 1000) for _ in range(12)], "send": [r.randrange(0, 50) for _ in range(6)]}


def gen_mats(r):
    return [[fb(r.uniform(-3.0, 3.0)) for _ in range(16)] for _ in range(3)]


def gen_track(r, S, nA=None, lo=0.5, hi=2.5, braces=None, sflags=None, wild=False):
    """K + S + 2 samples with x strictly increasing (wild: every coordinate any float), two chains of controls (A: nA with the +inf knot
    last, B: 2 nA + 2), 3 braces. A sample's pieces are controls of the chains (A: not the last, B: up to the nA-th)."""
    nA = nA or r.randint(2, 8)
    x, y, z = r.uniform(-5.0, 5.0), r.uniform(-5.0, 5.0), r.uniform(-5.0, 5.0)
    sflags = sflags or [0, 0, 0, 8, 8, 1, 9, 0x10, 0x18]
    samples = []
    for i in range(K + S + 2):
        if i:
            x, y, z = x + r.uniform(lo, hi), y + r.uniform(-2.0, 2.0), z + r.uniform(-1.5, 1.5)
        slow = r.random()
        px, py, pz = (rf(r), rf(r), rf(r)) if wild else (fb(x), fb(y), fb(z))
        samples.append({"mFlags": r.choice(sflags), "mPos.x": px, "mPos.y": py, "mPos.z": pz,
                        "mSpeed": fb(r.uniform(1.0, 30.0)) if slow > 0.06 else r.choice([0, 0x7FC00000, fb(0.001), 0x80000000]),
                        "a": r.randrange(0, nA - 1), "b": r.randrange(0, nA + 1)})
    brace_flags = braces if braces is not None else [r.choice([0, 0x800, 0x1, 0x801, 0x7FF, 0x10, 0xFFFFFFFF]) for _ in range(3)]
    knot = r.uniform(-1.0, 2.0)
    ctla = []
    for i in range(nA):
        if i:
            knot += 0.0 if r.random() < 0.05 else r.uniform(0.3, 6.0)
        ctla.append({"mF08": fb(knot) if i < nA - 1 else 0x7F800000, "mF0C": rangle(r), "brace": r.randrange(3),
                     "mF40.x": rf(r) if r.random() < 0.1 else fb(r.uniform(-20.0, 20.0)), "mF40.y": rf(r), "mF40.z": fb(r.uniform(-20.0, 20.0))})
    ctlb = [{"mF08": rangle(r), "mF0C": rangle(r), "brace": r.randrange(3), "mF40.x": rf(r), "mF40.y": rf(r), "mF40.z": rf(r)}
            for _ in range(2 * nA + 2)]
    return {"samples": samples, "ctla": ctla, "ctlb": ctlb, "braces": brace_flags}


# ---- helpers of the generators


def opt(o, key, make):
    """An option of an edge case, else the random choice."""
    return o[key] if key in o else make()


def fbx(v):
    """Bits of a float, the string "nan" for a NaN."""
    return 0x7FC00000 if v == "nan" else fb(v)


def mk(op, r, spec, args=None, g=None, now=0):
    spec["g"] = g if g is not None else gen_globals(r)
    return {"op": op, "spec": spec, "args": args or {}, "queues": gen_queues(r, now)}


# ---- DistanceBetweenPoints
def distance_case(r, a=None, b=None):
    p = [rf(r) for _ in range(6)] if a is None else list(a) + list(b)
    return mk("distance", r, {"bufs": [p[:3], p[3:]]})


def distance_edges():
    r = random.Random(215001)
    P = lambda *xs: [fb(x) for x in xs]
    pairs = [(P(0, 0, 0), P(0, 0, 0)), (P(1, 2, 3), P(1, 2, 3)), (P(0, 0, 0), P(3, 4, 0)), (P(0, 0, 0), P(0, 3, 4)), (P(1, 1, 1), P(2, 2, 2)),
             (P(0, 0, 0), P(1e20, 0, 0)), (P(0, 0, 0), P(0, 1e20, 0)), (P(0, 0, 0), P(0, 0, 1e20)), (P(0, 0, 0), P(1e20, 1e20, 1e20)),
             (P(1e30, 0, 0), P(-1e30, 0, 0)), (P(0, 0, 0), P(1e-23, 0, 0)), (P(0, 0, 0), P(1e-30, 1e-30, 1e-30)), (P(0.1, 0.2, 0.3), P(-0.7, 0.5, 0.9)),
             (P(1.0000001, 0, 0), P(0, 0, 0)), (P(3e38, 3e38, 3e38), P(-3e38, -3e38, -3e38)), (P(5, 0, 0), P(5, 0, 0))]
    for a, b in pairs:
        yield distance_case(r, a, b)
    for k in range(len(SPECIAL)):
        yield distance_case(r, [SPECIAL[k], fb(1.0), fb(2.0)], [fb(0.5), SPECIAL[(k + 3) % 10], SPECIAL[(k + 5) % 10]])
    yield distance_case(r, [0x7F800000, 0, 0], [0x7F800000, 0, 0])
    yield distance_case(r, [0, 0, 0x7FC00000], [0, 0, 0])


def distance_random(r):
    return distance_case(r)


# ---- GetLinearSamplePosn
def linear_case(r, S=None, sample=None, t=None, wild=None):
    S = S or r.randint(3, 10)
    wild = (r.random() < 0.3) if wild is None else wild
    sample = r.randint(-K, S) if sample is None else sample
    t = (rf(r) if r.random() < 0.5 else fb(r.uniform(-0.2, 1.2))) if t is None else t
    spec = {"inst": {"mFlags": 0, "mSampleCount": S}, "track": gen_track(r, S, wild=wild), "bufs": [[rf(r) for _ in range(3)]]}
    return mk("linear", r, spec, {"sample": sample, "t": t})


def linear_edges():
    r = random.Random(215002)
    ts = [0, 0x80000000, fb(1.0), fb(0.5), fb(-1.0), fb(2.0), 0x7FC00000, 0x7F800000, 0xFF800000, fb(1e-30), fb(1e30), fb(0.3), fb(1.0000001), fb(0.9999999)]
    for tv in ts:
        yield linear_case(r, 5, r.randint(-K, 5), tv, False)
    for sm in (-K, -1, 0, 4, 5):
        yield linear_case(r, 5, sm, fb(0.25), False)
        yield linear_case(r, 5, sm, fb(0.75), True)
    for _ in range(6):
        yield linear_case(r, 6, r.randint(0, 6), fb(0.5), True)


def linear_random(r):
    return linear_case(r)


# ---- HandleCollisions, PlayCollisionSound
def coll_train(r, L, **over):
    k = r.random()
    front = fb(r.uniform(0.0, 2.0 * L)) if k < 0.9 else r.choice([0x7FC00000, fb(r.uniform(-3.4, 0.0))])
    rear = fb(r.uniform(-3.0, 2.0 * L)) if r.random() < 0.95 else 0x7FC00000
    speed = fb(r.uniform(0.0, 15.0)) if r.random() < 0.9 else r.choice([0, 0x80000000, fb(3.0), fb(-2.0), 0x7FC00000])
    t = {"mFlags": r.choice([0, 0x40, 0x10, 0x80, 0x1C0]), "mPosition": fb(r.uniform(0.0, L)), "mFrontSample": front, "mRearSample": rear, "mF78": speed}
    t.update(over)
    return t


def mk_coll(r, op, trains, idx, L, S, rec=None, count=None):
    inst = {"mFlags": 0, "mTrainCount": len(trains) if count is None else count, "mLength": fb(L)}
    return mk(op, r, {"inst": inst, "track": gen_track(r, S), "trains": trains}, {"train": idx}, gen_globals(r, rec))


def coll_length(r, S):
    return r.choice([1.0, 4.5, float(S - 1), r.uniform(2.0, S - 1.0)])


def collide_random(r):
    S = r.randint(5, 12)
    L = coll_length(r, S)
    nT = r.choice([1, 2, 2, 3, 3, 4])
    return mk_coll(r, "collide", [coll_train(r, L) for _ in range(nT)], r.randrange(nT), L, S)


def Tr(pos=1.0, front=3.0, rear=0.0, speed=1.0, flags=0):
    return {"mFlags": flags, "mPosition": fb(pos), "mFrontSample": fbx(front), "mRearSample": fbx(rear), "mF78": fbx(speed)}


def collide_edges():
    r = random.Random(215003)
    S, L = 8, 4.5
    nan, inf = "nan", float("inf")
    cases = [
        ([Tr()], 0, {}), ([Tr(), Tr()], 0, {"count": 0}), ([Tr(), Tr()], 1, {"count": 1}),
        ([Tr(front=3, speed=3), Tr(rear=2.0, speed=0.0)], 0, {}),                       # the speeds end exactly 1 apart: no sound
        ([Tr(front=3, speed=4.1), Tr(rear=2.0, speed=0.0)], 0, {}),                      # 1.37 apart: sound
        ([Tr(front=3, speed=3.0000002), Tr(rear=2.0, speed=0.0)], 0, {}),
        ([Tr(front=3, speed=13.0), Tr(rear=2.0, speed=10.0)], 0, {}),
        ([Tr(front=3, speed=6.0, flags=0), Tr(rear=2.0, speed=0.0, flags=0x40)], 0, {}),
        ([Tr(front=3, speed=6.0), Tr(rear=2.0, speed=0.0, flags=0xFFFFFFFF)], 0, {}),
        ([Tr(front=3, speed=6.0), Tr(rear=2.0, speed=0.0), Tr(rear=1.0, speed=1.0)], 1, {}),
        ([Tr(front=3, speed=6.0), Tr(rear=2.0, speed=0.0), Tr(front=3.0, rear=1.0, speed=9.0, pos=2.0)], 2, {}),   # the last train's next is the first
        ([Tr(pos=3.0, front=3.0, speed=6.0), Tr(pos=1.0, rear=0.0, speed=0.0)], 0, {}),                          # the other is behind: its rear + length
        ([Tr(pos=3.0, front=5.0, speed=6.0), Tr(pos=1.0, rear=0.0, speed=0.0)], 0, {}),
        ([Tr(front=3.0, speed=6.0), Tr(rear=3.0, speed=0.0)], 0, {}),                    # front == rear
        ([Tr(front=nan, speed=6.0), Tr(rear=2.0, speed=0.0)], 0, {}),
        ([Tr(front=3.0, speed=6.0), Tr(rear=nan, speed=0.0)], 0, {}),
        ([Tr(front=3.0, speed=nan), Tr(rear=2.0, speed=0.0)], 0, {}),
        ([Tr(front=3.0, speed=6.0), Tr(rear=2.0, speed=nan)], 0, {}),
        ([Tr(front=3.0, speed=2.0), Tr(rear=2.0, speed=2.0)], 0, {}),                    # equal speeds
        ([Tr(front=3.0, speed=0.0), Tr(rear=2.0, speed=6.0)], 0, {}),                    # the other is faster
        ([Tr(front=inf, speed=2.0), Tr(rear=2.0, speed=2.0)], 0, {}),
        ([Tr(front=-2.3, speed=6.0), Tr(rear=-3.0, speed=0.0)], 0, {}),                  # the sound's place is before the first sample
        ([Tr(front=4.5, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {}),
        ([Tr(front=9.0, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {}),
        ([Tr(front=4.4999995, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {}),
        ([Tr(front=2.5, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {"L": 1.0}),
        ([Tr(front=3.0, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {"rec": 1}),
        ([Tr(front=3.0, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {"rec": -1}),
        ([Tr(front=3.0, speed=6.0), Tr(rear=1.0, speed=0.0)], 0, {"S": 5, "L": 4.0}),
        ([Tr(front=7.5, speed=1.5, pos=0.2), Tr(rear=-1.0, speed=0.25, pos=0.4), Tr(rear=0.0, speed=2.0), Tr(rear=3.0, speed=5.0)], 0, {}),
    ]
    for trains, idx, o in cases:
        yield mk_coll(r, "collide", trains, idx, o.get("L", L), o.get("S", S), o.get("rec"), o.get("count"))
    for _ in range(4):
        yield collide_random(r)


def play_random(r):
    S = r.randint(5, 12)
    L = coll_length(r, S)
    nT = r.randint(1, 3)
    return mk_coll(r, "play", [coll_train(r, L, mFrontSample=fb(r.uniform(0.0, 3.0 * L))) for _ in range(nT)], r.randrange(nT), L, S)


def play_edges():
    r = random.Random(215004)
    S = 8
    for front, L, rec in [(0.0, 4.5, 0), (4.5, 4.5, 0), (9.0, 4.5, 0), (4.4999995, 4.5, 0), (2.5, 4.5, 0), (3.0, 4.5, 0), (-2.3, 4.5, 0),
                          (-0.5, 4.5, 0), (-3.4, 4.5, 0), (-0.0, 4.5, 0), (1e-30, 4.5, 0), (4.5000005, 4.5, 0), (7.0, 7.0, 0), (7.5, 7.0, 0),
                          (2.5, 1.0, 0), (0.999, 1.0, 0), (3.0, 4.5, 1), (3.0, 4.5, -1), (3.7, 4.5, 0), (123456.0, 6.5, 0)]:
        yield mk_coll(r, "play", [Tr(front=front)], 0, L, S, rec)
    for _ in range(4):
        yield play_random(r)
    # coordinates past the int range and the point's every coordinate negative
    c = mk_coll(r, "play", [Tr(front=2.5)], 0, 4.5, S, 0)
    c["spec"]["track"] = gen_track(r, S, wild=True)
    yield c


# ---- SetTrainSoundState
STATES = list(range(0x13)) + [0x20, 0x7FFFFFFF, 0xFFFFFFFF]


def sound_case(r, state=None, cur=None, rec=None, value=None):
    state = r.choice(STATES) if state is None else state
    cur = (state if r.random() < 0.25 else r.choice(STATES)) if cur is None else cur
    value = (r.getrandbits(32) if r.random() < 0.5 else r.randint(0, 600)) if value is None else value
    train = {"mF38": cur, "mF54[2]": r.getrandbits(32), "mF54[3]": r.getrandbits(32), "mF68": r.choice([0, r.randrange(1, 1000), r.getrandbits(32)])}
    return mk("sound", r, {"train": train}, {"state": state, "value": value}, gen_globals(r, rec))


def sound_edges():
    r = random.Random(215005)
    for st in STATES:
        yield sound_case(r, st, 0x3F, 0, 100)
        yield sound_case(r, st, st, 0, 7)
    for st in (1, 2, 4, 8, 0x10, 3, 0x11):
        yield sound_case(r, st, 0, 1, 5)
        yield sound_case(r, st, 0, -1, 5)
        yield sound_case(r, st, 0xFFFFFFFF, 0, 0xFFFFFFFF)


def sound_random(r):
    return sound_case(r)


# ---- InterpolateCoasterSpline (the instance overload)
def interp_case(r, S=None, pos=None, orient=None, nA=None, track=None):
    S = S or r.randint(4, 10)
    track = track or gen_track(r, S, nA=nA)
    pos = fb(r.uniform(-0.99, S + 1.99)) if pos is None else pos
    orient = r.choice(["none", "inst", "buf", "buf"]) if orient is None else orient
    spec = {"inst": {"mFlags": 0, "mSampleCount": S}, "track": track, "bufs": [[rf(r) for _ in range(3)], [rf(r) for _ in range(7)]]}
    return mk("interp", r, spec, {"pos": pos, "orient": orient})


def interp_edges():
    r = random.Random(215006)
    S = 6
    for nA in (2, 3, 5, 8):
        track = gen_track(r, S, nA=nA)
        knots = [c["mF08"] for c in track["ctla"] if c["mF08"] != 0x7F800000]
        for kb in knots[:3]:                                                     # a place exactly on a control's knot
            yield interp_case(r, S, kb, "buf", nA, track) if -1.0 < bitsf(kb) < S + 2 else interp_case(r, S, fb(0.5), "buf", nA, track)
        yield interp_case(r, S, 0, "inst", nA, track)
        yield interp_case(r, S, 0x80000000, "none", nA, track)
        yield interp_case(r, S, fb(S + 1.5), "buf", nA, track)                   # the last sample
        yield interp_case(r, S, fb(-0.5), "buf", nA, track)
        yield interp_case(r, S, fb(0.9999999), "inst", nA, track)
        yield interp_case(r, S, fb(3.0), "none", nA, track)
    track = gen_track(r, S, nA=4)
    for c in track["ctla"][:3]:
        c["mF08"] = fb(1.0)                                                      # equal knots: the fraction divides by 0
    for pos in (0.5, 1.0, 1.5, 3.0):
        yield interp_case(r, S, fb(pos), "buf", 4, track)


def interp_random(r):
    return interp_case(r)


# ---- SetCarOrientation
def orient_case(r, ride=None, point=None, angles=None, g=None):
    ride = r.randint(1, 3) if ride is None else ride
    point = r.random() < 0.6 if point is None else point
    a = angles or [rangle(r), rangle(r), rangle(r)]
    orient = [rf(r), a[0], a[1], a[2], rf(r), rf(r), rf(r)]
    pt = [fb(r.uniform(-5.0, 5.0)) if r.random() < 0.9 else rf(r) for _ in range(3)]
    spec = {"rides": gen_mats(r), "car": {"mFlags": 1, "mRide": ride}, "bufs": [orient, pt]}
    return mk("orient", r, spec, {"point": point}, g)


def orient_edges():
    r = random.Random(215007)
    pi2 = math.pi / 2.0
    angle_sets = [[0.0, 0.0, 0.0], [pi2, pi2, pi2], [-pi2, 1.0, -1.0], [math.pi, math.pi, math.pi], [3.0, -3.0, 0.5], [7.0, -7.0, 100.0],
                  [1e9, -1e9, 1e12], [0.0001, -0.0001, 6.2831855]]
    for a in angle_sets:
        for point in (False, True):
            yield orient_case(r, r.randint(1, 3), point, [fb(x) for x in a])
    for bits in (0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 1, 0x7F7FFFFF):
        yield orient_case(r, 1, True, [bits, fb(0.5), fb(1.0)])
        yield orient_case(r, 2, False, [fb(0.5), bits, fb(1.0)])
        yield orient_case(r, 3, True, [fb(0.5), fb(1.0), bits])
    for ride in (1, 2, 3):
        g = gen_globals(r, 0)
        g.update({"mask": 0x3FF, "scale": fb(100.0)})
        yield orient_case(r, ride, True, None, g)
        g2 = gen_globals(r, 0)
        g2.update({"mask": 0xFFF, "scale": 0})
        yield orient_case(r, ride, True, None, g2)
        g3 = gen_globals(r, 0)
        g3.update({"mask": 0xFFF, "scale": fb(-300.0)})
        yield orient_case(r, ride, True, None, g3)


def orient_random(r):
    return orient_case(r)


# ---- CoasterCarRideTrackTick
def tick_type(r, o, nan_limit):
    # With no speed limit (NaN: the clamp never takes) a multiplier over 1 on a flag 8 sample doubles the speed every half sample for ever
    # (the original never leaves its loop either), so the multiplier stays at 1 or less there.
    mult = r.choice([0.0, 0.5, 1.0] if nan_limit else [0.0, 0.5, 1.0, 1.5, 2.0])
    return {"mF24": fb(r.choice([0.0, 0.02, 0.05, 0.1])), "mF28": fb(r.choice([0.0, 0.3, 1.0, 2.0])), "mF2C": fb(r.choice([0.0, 0.3, 1.0, 2.0])),
            "mF44": fb(r.choice([0.0, 1.0, 4.0, 8.0])), "mF48": fb(r.choice([0.0, 0.5, 1.0, 3.0])), "mF54": fb(mult),
            "mF2EC": opt(o, "f2ec", lambda: r.randint(0, 5000) if r.random() < 0.9 else r.choice([100000, 0xFFFFFFF0])),
            "mF2F4": opt(o, "f2f4", lambda: r.choice([1, 1, 2, 3, 4, 5, 10, 100]))}


def tick_ctype(r, o):
    return {"mFlags": opt(o, "cfl", lambda: r.choice([0, 0, 1, 2, 3])), "mF18": fb(opt(o, "f18", lambda: r.choice([0.0, 0.0, 0.3, 1.5, -0.5]))),
            "mF1C": fb(r.uniform(0.0, 2.0)), "mF24": fb(r.uniform(0.0, 1.5)), "mF28": fb(r.uniform(0.0, 1.5))}


def tick_case(r, **o):
    S = o.get("S") or r.randint(12, 24)
    nT = opt(o, "nT", lambda: r.choice([1, 1, 2, 2, 3, 3, 4]))
    fac = o.get("fac") or r.choice([3.0] * 5 + [2.0, 4.0, 1.0])
    L = o.get("L") or (r.uniform(1.5, 4.0) if r.random() < 0.1 else r.uniform(4.0, S - 1.0))
    fk = opt(o, "fade", lambda: r.choice(["nan", "far", "mid", "mid", "mid", "neg"]))
    if fk == "nan":
        fade0 = 0x7FC00000
    elif fk == "far":
        fade0 = fb(L + 1.0 + r.uniform(0.01, 5.0))
    elif fk == "mid":
        hi = min(0.9 * L, L - 0.6)
        fade0 = fb(r.uniform(min(0.2 * L, hi - 0.01), hi))
    else:
        fade0 = fb(r.uniform(-2.0, 0.0))
    ffc = opt(o, "ffc", lambda: r.choice([fb(0.0), fb(r.uniform(0.0, L)), fb(-1.0), 0x7FC00000]))
    limit = opt(o, "limit", lambda: r.choice([5.0, 10.0, 20.0, 100.0, 100.0, "nan", 0.0]))
    types = [tick_type(r, o, limit == "nan") for _ in range(r.choice([1, 2]))]
    nct = r.randint(2, 4)
    ctypes = [tick_ctype(r, o) for _ in range(nct)]
    base = r.uniform(0.0, L)
    cluster = opt(o, "cluster", lambda: nT > 1 and r.random() < 0.6)
    trains, cars = [], []
    for t in range(nT):
        nC = opt(o, "nC", lambda: r.randint(1, 5))
        head, centre = r.randint(0, SLOTS - nC), r.randrange(nC)
        pos = ((base + r.uniform(0.0, 3.0)) % L) if cluster else r.uniform(0.0, L)
        if "pos" in o:
            pos = o["pos"][t]
        speed = opt(o, "speed", lambda: r.choice([0.0, 0.005, 1.9, 2.0]) if r.random() < 0.3 else r.uniform(0.0, 15.0))
        tflags = opt(o, "tflags", lambda: sum(b for b in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100) if r.random() < 0.25))
        trains.append({"mFlags": tflags, "mCarsInUse": nC, "mF08": head, "mCentreCar": centre, "mPosition": fb(pos), "mF78": fb(speed),
                       "mFrontSample": fb(pos + r.uniform(0.0, 2.0)), "mRearSample": fb(pos - r.uniform(0.0, 3.0)),
                       "mF38": opt(o, "state", lambda: r.choice([0, 2, 4, 8, 0x10, 0x20])), "mF54[2]": r.getrandbits(32), "mF54[3]": r.getrandbits(32),
                       "mF68": r.randrange(0, 1000), "ty": 0 if len(types) == 1 or r.random() > 0.3 else 1})
        row = []
        for j in range(SLOTS):
            fl = opt(o, "cflags", lambda: r.choice([1, 1, 1, 1, 3, 0, 2]))
            if j == centre:
                fl |= 1
            off = r.uniform(0.0, fac) if j == head else r.uniform(-3.5 * fac, fac)
            row.append({"mFlags": fl, "mRide": opt(o, "ride", lambda: r.choice([0, 1, 2, 3])), "mF38": fb(off), "mF40": fb(r.uniform(-1.5, S)),
                        "ct": r.randrange(nct)})
        cars.append(row)
    lift = o.get("lift")
    sfl = o.get("sflag8")
    track = gen_track(r, S, lo=0.5, hi=4.0, braces=None if lift is None else ([0x800, 0x801, 0x800] if lift else [0, 1, 0x10]),
                      sflags=None if sfl is None else ([8, 9, 0x18] if sfl else [0, 1, 0x10]))
    first = opt(o, "first", lambda: r.random() < 0.15)
    f120 = o["f120"] if "f120" in o else r.getrandbits(32)
    k = r.random()
    dt = opt(o, "dt", lambda: 0 if k < 0.08 else r.randint(1, 60) if k < 0.68 else r.randint(61, 300) if k < 0.93 else r.randint(301, 2500))
    now = (f120 + dt) & MASK
    inst = {"mFlags": (0 if first else 0x10) | (4 if r.random() < 0.1 else 0),
            "mF2C": fbx(opt(o, "f2c", lambda: r.choice([1.0] * 6 + [0.0, 0.0, 0.5, 2.0]))),
            "mSpeedLimit": fbx(limit),
            "mSampleCount": S, "mLength": fb(L), "mFAC": fb(fac), "mTrainCount": nT, "mFFC": ffc, "mF120": f120, "mBankFadeStart": fade0}
    g = gen_globals(r, o.get("rec"))
    if "init" in o:
        g["init"] = o["init"]
    spec = {"rides": gen_mats(r), "ctypes": ctypes, "types": types, "inst": inst, "track": track, "trains": trains, "cars": cars}
    if o.get("nullTrains"):
        spec["mTrains"] = False
    return mk("tick", r, spec, {}, g, now)


TICK_EDGES = [
    dict(nT=1, first=True), dict(nT=1, first=False, dt=0), dict(nT=3, first=True, dt=500), dict(nT=0), dict(nT=0, nullTrains=True),
    dict(nT=1, init=0, dt=30), dict(nT=1, init=1, dt=30),
    dict(nT=1, dt=1), dict(nT=1, dt=2500), dict(nT=2, dt=2500, cluster=True), dict(nT=2, dt=40, f120=0xFFFFFF00), dict(nT=2, dt=300, f120=0xFFFFFFF0),
    dict(nT=1, fade="nan"), dict(nT=1, fade="far"), dict(nT=1, fade="mid"), dict(nT=1, fade="neg"),
    dict(nT=2, fade="mid", tflags=0x40), dict(nT=2, fade="mid", tflags=0), dict(nT=2, fade="neg", dt=300), dict(nT=2, fade="mid", tflags=0x80, dt=200),
    dict(nT=1, fade="neg", tflags=0xA0, speed=6.0, dt=100, ffc=fb(0.0)),
    dict(nT=1, lift=True), dict(nT=1, lift=False), dict(nT=2, lift=True, f2c=0.0), dict(nT=1, lift=True, f2c=1.0, limit=100.0),
    dict(nT=1, lift=True, limit=5.0, speed=8.0), dict(nT=1, lift=True, limit=100.0, speed=12.0), dict(nT=2, lift=True, tflags=0xA0),
    dict(nT=1, sflag8=True), dict(nT=1, sflag8=False), dict(nT=2, sflag8=True, lift=False, fade="far"),
    dict(nT=1, rec=1), dict(nT=2, rec=-1),
    dict(nT=1, speed=0.0), dict(nT=1, speed=0.005), dict(nT=1, speed=1.9), dict(nT=1, speed=2.0), dict(nT=1, speed=0.0099), dict(nT=1, speed=0.0101),
    dict(nT=2, limit="nan"), dict(nT=2, limit=0.0), dict(nT=1, f2c=0.0), dict(nT=1, f2c=2.0, lift=True),
    dict(nT=3, tflags=0x80, f2ec=0, dt=100), dict(nT=3, tflags=0xA0, f2ec=100000, dt=100), dict(nT=2, tflags=0x80, f2ec=2000),
    dict(nT=2, tflags=0x80, f2ec=0xFFFFFFF0), dict(nT=1, tflags=0x80, f2ec=0, dt=40),
    dict(nT=1, ride=0), dict(nT=1, ride=1, f18=0.0, cfl=0), dict(nT=1, ride=2, f18=1.5, cfl=0), dict(nT=1, ride=3, f18=0.3, cfl=1),
    dict(nT=1, ride=1, cfl=3), dict(nT=2, ride=2, f18=-0.5, cfl=2),
    dict(nT=1, f2f4=1, lift=True), dict(nT=1, f2f4=3, lift=True), dict(nT=1, f2f4=0x80000000, lift=True),
    dict(nT=4, cluster=True, dt=200), dict(nT=3, cluster=True, dt=500, speed=8.0), dict(nT=2, cluster=True, dt=60, speed=12.0, tflags=0),
    dict(nT=2, L=1.5), dict(nT=1, L=2.5, dt=100), dict(nT=2, fac=1.0), dict(nT=1, fac=4.0), dict(nT=2, fac=2.0, cluster=True),
    dict(nT=1, nC=1), dict(nT=1, nC=5), dict(nT=2, nC=5, cflags=1), dict(nT=2, nC=3, cflags=0),
    dict(nT=2, state=8), dict(nT=2, state=0x10), dict(nT=1, state=4), dict(nT=1, state=2), dict(nT=1, state=0xFFFFFFFF),
    dict(nT=2, pos=[0.01, 0.4], L=6.0, S=16, dt=100), dict(nT=2, pos=[5.9, 5.99], L=6.0, S=16, dt=200),
    dict(nT=1, pos=[5.3], L=5.0, S=16, dt=100), dict(nT=2, pos=[5.3, 1.0], L=5.0, S=16, dt=300, cluster=False),   # a place past the lap's end
    dict(nT=2, pos=[5.0, 4.9], L=5.0, S=16, dt=100, cluster=False),                                              # exactly at the end
]


def tick_edges():
    r = random.Random(215008)
    for o in TICK_EDGES:
        yield tick_case(r, **o)


def tick_random(r):
    return tick_case(r)


TESTS = [
    FnTest("DistanceBetweenPoints", ppc, host, edge=distance_edges, rnd=distance_random),
    FnTest("GetLinearSamplePosn", ppc, host, edge=linear_edges, rnd=linear_random),
    FnTest("HandleCollisions", ppc, host, edge=collide_edges, rnd=collide_random),
    FnTest("InterpolateCoasterSpline", ppc, host, edge=interp_edges, rnd=interp_random),
    FnTest("PlayCollisionSound", ppc, host, edge=play_edges, rnd=play_random),
    FnTest("SetTrainSoundState", ppc, host, edge=sound_edges, rnd=sound_random),
    FnTest("SetCarOrientation", ppc, host, edge=orient_edges, rnd=orient_random),
    FnTest("CoasterCarRideTrackTick", ppc, host, edge=tick_edges, rnd=tick_random),
]
