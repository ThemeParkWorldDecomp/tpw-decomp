"""The coaster's track statistics (unit w212), against libtpw_unit_CoasterStats.dylib (tests/unit_CoasterStats.cpp), main app
("SimTheme Park"), src/TPConfig_stats.cpp:
  CalculateTrackGForcesSpeedAndExcitement 0x1004ba48, UpdateExcitementAtSample 0x1004d424, SpreadTrainsOnTrack 0x1004b6c4,
  UpdateCoasterTrainPosns 0x1004b4ac, UpdateCoasterTrainsActive 0x1004b320, CollateCoasterStats 0x1004d7c8,
  SetInitialExcitementValues 0x1004d338, SetCoasterCapacity 0x1004a248, ModF 0x1004a75c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; the same code reads both back. The
callees outside the unit are fakes on both sides that record their arguments and answer from the case's queues: UnloadTrain 0x100499e4,
ActivateCoasterTrain 0x1004a8a0 and StopTrainSounds 0x100471bc (each may change the instance or the train, because the unit reads them again
after the call), the two DisableMeshDraw 0x100653a4 and 0x1006548c, and GetVectorRotationAroundZAxisFromY 0x100c1290 (the angle comes from the
queue "angle"). The MSL imports modf and fmod are Python functions on the PowerPC side and libm on the host.

The world of a case (case["seed"] and case["p"] build it, case["o"] overrides single fields): the 22 words of TPConfig.cpp, the engine's sine
table (gTPGlobals), the ride animation sets' table (gRideAnimSets), one coaster instance with two coaster types, twelve trains of ten cars
with four car types, the track's samples with their pieces and eight braces (a chain from mLastBrace), four ride animation sets. Pointers
are written from names, so both sides hold the same graph. What is compared: the answer (ModF), every field of every object, the 22 words
and the event log. Pointers are compared as names ("trains[3]", "null"); floats with every NaN made one.

"Clean" worlds hold finite sane numbers: the spread step (SpreadTrainsOnTrack, and its call from the g-force function when the instance's
flag 0x1000 is set and 0x4000 clear) loops until a time passes and indexes the samples, so it needs positive speeds, a track long enough
for the trains and at least one train. "Wild" worlds (NaN, infinities, zeros of both signs, any scale) run where no such step can run, and
for the functions that have none.

Fused operations: the original's fmadds / fmsubs are rounded once by the hardware and Unicorn rounds them twice, and its fctiwz saturates
wrongly, so the unit's functions run under HardwareRounding (tests/test_DynamicMesh.py). Not tested: zero trains or zero samples (divwu by
zero is undefined), a sample index or position outside the track (the Mac's 0x38 sample stride and the host's 0x40 read other places), a
speed of zero, negative or NaN in the spread step (it never ends), mCarsInUse = 0 in the placement of cars, a negative train count in
SetCoasterCapacity's loop (it would run four billion times).
"""
import ctypes as C
import math
import random
import re
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CoasterStats"
MASK = 0xFFFFFFFF
KNONE = 0x7FFFFFFF          # what the fakes' queues answer for "leave it as it is" (kNone in tests/unit_CoasterStats.cpp)

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
CALC, EXCITE, SPREAD, POSNS, ACTIVE = 0x1004BA48, 0x1004D424, 0x1004B6C4, 0x1004B4AC, 0x1004B320
COLLATE, INITIAL, CAPACITY, MODF = 0x1004D7C8, 0x1004D338, 0x1004A248, 0x1004A75C
SIZES = {CALC: 2340, EXCITE: 844, SPREAD: 580, POSNS: 468, ACTIVE: 324, COLLATE: 276, INITIAL: 164, CAPACITY: 156, MODF: 84}
OWN = set(SIZES)
ENTRY = {"calc": CALC, "excite": EXCITE, "spread": SPREAD, "posns": POSNS, "active": ACTIVE, "collate": COLLATE, "initial": INITIAL,
         "capacity": CAPACITY, "modf": MODF}

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
UNLOAD, ACTIVATE, STOPSOUNDS, DMD1, DMD3, ROTATION = 0x100499E4, 0x1004A8A0, 0x100471BC, 0x100653A4, 0x1006548C, 0x100C1290

# ---- the 22 words of TPConfig.cpp: (name, the Mac's address, kind, word count); the host's come from tpw_cst_addr(name)
GLOBALS = [
    ("gTrackGravity", 0x1029CE30, "f", 1), ("gCoasterMinTrains", 0x1029CE34, "w", 1), ("gBankFadeStartMin", 0x1029CE38, "f", 1),
    ("gBankFadeMax", 0x1029CE3C, "f", 1), ("gBankFadeMin", 0x1029CE40, "f", 1), ("gExciteBankFloorA", 0x1029CE44, "f", 1),
    ("gExciteBankFloorB", 0x1029CE48, "f", 1), ("gExciteFloor", 0x1029CE4C, "f", 1), ("gRatingBMax", 0x1029CE50, "f", 1),
    ("gRatingBMin", 0x1029CE54, "f", 1), ("gRatingAMax", 0x1029CE58, "f", 1), ("gRatingAMin", 0x1029CE5C, "f", 1),
    ("gExciteRiseA", 0x102B7474, "w", 1), ("gExciteRiseB", 0x102B7478, "w", 1), ("gExcitePrevB", 0x102B747C, "f", 1),
    ("gExciteMaxB", 0x102B7480, "f", 1), ("gExciteMaxA", 0x102B7484, "f", 1), ("gExcitePrevBank", 0x102B7488, "f", 1),
    ("gExcitePrevG", 0x102B748C, "f", 3), ("gExciteWord", 0x102B7498, "w", 1), ("gExciteB", 0x102B749C, "f", 1),
    ("gExciteA", 0x102B74A0, "f", 1),
]
ADDR = {name: addr for name, addr, _, _ in GLOBALS}
ADDR["gTPGlobals"] = 0x103B74C8             # mSinTableMask +4, mSinTableScale +8, mSinTable +0xC (4096 floats)
ADDR["gRideAnimSets"] = 0x102C79A4          # Mac entries are 4 bytes
SIN_MAC = (4, 8, 12)
START = {"gTrackGravity": 0x3F800000, "gCoasterMinTrains": 1, "gBankFadeMax": 0x3F800000, "gRatingBMax": 0x42C80000,
         "gRatingAMax": 0x42C80000}         # the Mac's starting values of the initialised words (the others start at zero)

# ---- struct -> ([(field, Mac offset, kind)], Mac size); kinds: w word, f float, p pointer (4 bytes on the Mac, 8 on the host), h2 two
# halfwords (one word in the log). The order is the one tpw_cst_layout exports.
FIELDS = {
    "inst": ([("mFlags", 0x00, "w"), ("mType", 0x04, "p"), ("mState", 0x08, "w"), ("mSpeedLimit", 0x34, "f"), ("mLastBrace", 0x54, "p"),
              ("mBraces", 0x58, "p"), ("mSamples", 0x9C, "p"), ("mSampleCount", 0xA4, "w"), ("mLength", 0xA8, "f"), ("mFAC", 0xAC, "f"),
              ("mTrainCount", 0xF4, "w"), ("mRatingA", 0x100, "f"), ("mRatingB", 0x104, "f"), ("mMarkedBraces", 0x108, "w"),
              ("mTrackLength", 0x10C, "f"), ("mTopHeight", 0x110, "f"), ("mTopSpeed", 0x114, "f"), ("mLeadTrain", 0x124, "p"),
              ("mTrains", 0x128, "p"), ("mBankFadeStart", 0x134, "f")], 0x150),
    "train": ([("mFlags", 0x00, "w"), ("mF08", 0x08, "w"), ("mCarsInUse", 0x0C, "w"), ("mFrontSample", 0x20, "f"),
               ("mRearSample", 0x24, "f"), ("mCars", 0x28, "p"), ("mCentreCar", 0x2C, "w"), ("mF30", 0x30, "f"),
               ("mPosition", 0x74, "f"), ("mF78", 0x78, "f")], 0x80),
    "car": ([("mFlags", 0x00, "w"), ("mType", 0x04, "p"), ("mRide", 0x0C, "w"), ("mF38", 0x38, "f"), ("mF40", 0x40, "f")], 0x60),
    "ctype": ([("mF24", 0x24, "f"), ("mF28", 0x28, "f")], 0x40),
    "type": ([("mFlags", 0x00, "w"), ("mF24", 0x24, "f"), ("mF28", 0x28, "f"), ("mF2C", 0x2C, "f"), ("mF44", 0x44, "f"),
              ("mF54", 0x54, "f"), ("mF60", 0x60, "f"), ("mF64", 0x64, "f"), ("mF68", 0x68, "f"), ("mF6C", 0x6C, "f"),
              ("mF2E8", 0x2E8, "w"), ("mF2EC", 0x2EC, "w"), ("mF320", 0x320, "f"), ("mF324", 0x324, "f"), ("mF32C", 0x32C, "f"),
              ("mF330", 0x330, "f"), ("mF334", 0x334, "f"), ("mF338", 0x338, "f"), ("mF340", 0x340, "f"), ("mF344", 0x344, "f"),
              ("mF348", 0x348, "f"), ("mF34C", 0x34C, "f"), ("mF350", 0x350, "f"), ("mF354", 0x354, "f"), ("mF358", 0x358, "f")], 0x360),
    "sample": ([("mFlags", 0x00, "w"), ("mPos.x", 0x04, "f"), ("mPos.y", 0x08, "f"), ("mPos.z", 0x0C, "f"), ("mYaw", 0x10, "f"),
                ("mPitch", 0x14, "f"), ("mBank", 0x18, "f"), ("mSpeed", 0x1C, "f"), ("mG.x", 0x20, "f"), ("mG.y", 0x24, "f"),
                ("mG.z", 0x28, "f"), ("mPiece", 0x2C, "p")], 0x38),
    "piece": ([("mBrace", 0x10, "p")], 0x14),
    "brace": ([("mFlags", 0x08, "w"), ("mPrev", 0x64, "p"), ("mSample", 0xFC, "w")], 0x104),
    "set": ([("mFlags", 0x04, "w"), ("mHeads", 0x08, "p"), ("mFC0", 0xC0, "h2"), ("mFC4", 0xC4, "h2")], 0xD0),
}
ORDER = ["inst", "train", "car", "ctype", "type", "sample", "piece", "brace", "set"]
KIND = {st: {f: k for f, _, k in FIELDS[st][0]} for st in FIELDS}
OFFM = {st: {f: o for f, o, _ in FIELDS[st][0]} for st in FIELDS}
SZM = {st: FIELDS[st][1] for st in FIELDS}

# ---- the host's offsets and sizes the module relies on, checked once against tpw_cst_layout (catches a header that moved): every struct's
# fields in FIELDS order then its size, then the sine table's mask, scale and table in gTPGlobals
HOST_LAYOUT = [
    0, 8, 16, 72, 112, 120, 192, 204, 208, 212, 300, 312, 316, 320, 324, 328, 332, 352, 360, 384, 496,        # inst
    0, 16, 20, 40, 44, 48, 56, 60, 132, 136, 144,                                                              # train
    0, 8, 20, 80, 88, 120,                                                                                     # car
    48, 52, 64,                                                                                                # ctype
    0, 44, 48, 52, 72, 88, 100, 104, 108, 112, 816, 820, 856, 860, 868, 872, 876, 880, 888, 892, 896, 900, 904,
    908, 912, 944,                                                                                             # type
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 72,                                                           # sample
    16, 24,                                                                                                    # piece
    8, 104, 260, 496,                                                                                          # brace
    8, 16, 288, 292, 392,                                                                                      # set
    4, 8, 12,                                                                                                  # gTPGlobals
]
HOST_OFF, HOST_SZ, HOST_SIN = {}, {}, ()

NT, NCARS, NB, NCT = 12, 10, 8, 4        # trains, cars per train, braces, car types a world holds
LABELS = [(1, "inst", 0)] + [(10 + i, "trains", i) for i in range(NT)] + [(30 + k, "heads%d" % k, 0) for k in range(1, 5)]


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def fbs(x):
    """The canonical bits of a float answer (f1 of the Mac, the C float of the host)."""
    try:
        return canon_f(fb(x))
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only (Unicorn rounds fused operations twice and saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def be(v):
    return struct.pack(">I", v & MASK)


def stub_unload(e):
    """UnloadTrain(instance, train, flag): the queue "utc" sets the instance's train count, "utb" moves its train base on by that many."""
    L = e.cl
    inst = e.r(3)
    L.rec("UnloadTrain", (L.lab(inst), L.lab(e.r(4)), s32(e.r(5))))
    k = L.ret("utc", KNONE)
    if k != KNONE:
        e.write(inst + 0xF4, be(k))
    adv = L.ret("utb", 0)
    e.write(inst + 0x128, be(e.u32(inst + 0x128) + adv * 0x80))


def stub_activate(e):
    """ActivateCoasterTrain(instance, train): the queues "atype" (0 or 1: the instance's type), "atc" (its train count), "aflags" (its flags)."""
    L = e.cl
    inst = e.r(3)
    L.rec("ActivateCoasterTrain", (L.lab(inst), L.lab(e.r(4))))
    k = L.ret("atype", KNONE)
    if k in (0, 1):
        e.write(inst + 4, be(L.types[k]))
    k = L.ret("atc", KNONE)
    if k != KNONE:
        e.write(inst + 0xF4, be(k))
    k = L.ret("aflags", KNONE)
    if k != KNONE:
        e.write(inst, be(k))


def stub_stopsounds(e):
    """StopTrainSounds(instance, train): the queue "sflags" sets the train's flags."""
    L = e.cl
    train = e.r(4)
    L.rec("StopTrainSounds", (L.lab(e.r(3)), L.lab(train)))
    k = L.ret("sflags", KNONE)
    if k != KNONE:
        e.write(train, be(k))


def stub_dmd1(e):
    e.cl.rec("DisableMeshDraw1", (e.cl.lab(e.r(3)),))


def stub_dmd3(e):
    """DisableMeshDraw(mesh, from, to): the two HFieldXZ arrive as one word each."""
    e.cl.rec("DisableMeshDraw3", (e.cl.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def stub_rotation(e):
    """GetVectorRotationAroundZAxisFromY(point): records the vector, answers the float whose bits the queue "angle" holds."""
    L = e.cl
    p = e.r(3)
    L.rec("GetVectorRotationAroundZAxisFromY", tuple(canon_f(e.u32(p + 4 * i)) for i in range(3)))
    e.set_f1(bitsf(L.ret("angle", 0)))


def imp_modf(e):
    """double modf(double x, double* whole): x in f1, the pointer in r5 (the double takes r3 and r4)."""
    frac, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)


def imp_fmod(e):
    """double fmod(double x, double y) with C semantics."""
    x, y = e.f(1), e.f(2)
    if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
        e.set_f1(math.nan)
    else:
        e.set_f1(math.fmod(x, y))


STUBS.internal(UNLOAD, stub_unload)
STUBS.internal(ACTIVATE, stub_activate)
STUBS.internal(STOPSOUNDS, stub_stopsounds)
STUBS.internal(DMD1, stub_dmd1)
STUBS.internal(DMD3, stub_dmd3)
STUBS.internal(ROTATION, stub_rotation)
STUBS.imp("modf", imp_modf)
STUBS.imp("fmod", imp_fmod)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy ... and sqrt (tpwtest.py 491 on)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def addr(self, name, i=0):
        a, esz = self.names[name]
        return a + i * esz

    def resolve(self, v):
        """A pointer field's value from its name: None, "name" or ("name", index)."""
        if v is None:
            return 0
        if isinstance(v, str):
            return self.names[v][0]
        return self.addr(v[0], v[1])

    def label(self, v):
        if v == 0:
            return "null"
        for lo, hi, name, esz in self.regions:
            if lo <= v < hi:
                i, off = divmod(v - lo, esz)
                return "%s[%d]" % (name, i) + ("+%d" % off if off else "")
        return "?"


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt, pf, f2, pw = ">I", ">I", ">HH", 4
    off, sz, sin = OFFM, SZM, SIN_MAC

    def __init__(self, e, case):
        self.e = e
        self.L = None
        self.saved = STUBS.activate(e)

    def begin(self, fixed, queues, types):
        self.L = self.e.cl = CL.PpcLog({addr: label for label, addr in fixed}, queues)
        self.L.types = list(types)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.write(a, bytes(n))
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args, f0=None):
        """args: ("i", word), ("p", address) or ("f", float bits); f0 is set first (the caller's last float, UpdateExcitementAtSample)."""
        e = self.e
        if f0 is not None:
            e.set_f(0, bitsf(f0))
        conv = [T.F(bitsf(v)) if k == "f" else v & MASK for k, v in args]
        with ROUNDING(e):
            r3, _, f1 = T.call_ppc(e, ENTRY[op], conv)
        return f1


class Host(Side):
    """The same on the host library."""
    fmt, pf, f2, pw = "<I", "<Q", "<HH", 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off, self.sz, self.sin = HOST_OFF, HOST_SZ, HOST_SIN

    def begin(self, fixed, queues, types):
        CL.host_begin(self.lib, {label: addr for label, addr in fixed}, queues)
        self.lib.tpw_cst_set_world(types[0], types[1])

    def done(self):
        pass

    def g(self, name):
        a = self.lib.tpw_cst_addr(name.encode())
        assert a, name
        return a

    def alloc(self, n):
        return self.lib.tpw_cst_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args, f0=None):
        return getattr(self.lib, "tpw_cst_" + op)(*[bitsf(v) if k == "f" else v & (MASK if k == "i" else (1 << 64) - 1) for k, v in args])


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    global HOST_SIN
    if getattr(lib, "bound", False):
        return
    p = "tpw_cst_"

    def fn(name, restype, argtypes):
        f = getattr(lib, p + name)
        f.restype, f.argtypes = restype, argtypes
        return f

    fn("addr", C.c_uint64, [C.c_char_p])
    fn("alloc", C.c_uint64, [C.c_uint32])
    layout = fn("layout", C.c_int32, [C.POINTER(C.c_int64)])
    fn("set_world", None, [C.c_uint64, C.c_uint64])
    fn("calc", None, [C.c_uint64])
    fn("excite", None, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_float, C.c_float, C.c_uint64, C.c_float])
    fn("spread", None, [C.c_uint64, C.c_float])
    fn("posns", None, [C.c_uint64])
    fn("active", None, [C.c_uint64])
    fn("collate", None, [C.c_uint64, C.c_float, C.c_float, C.c_float])
    fn("initial", None, [C.c_uint64])
    fn("capacity", None, [C.c_uint64, C.c_uint32, C.c_uint32])
    fn("modf", C.c_float, [C.c_float, C.c_uint64])
    out = (C.c_int64 * 256)()
    n = layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    pos = 0
    for st in ORDER:
        names = [f for f, _, _ in FIELDS[st][0]]
        HOST_OFF[st] = {f: out[pos + i] for i, f in enumerate(names)}
        HOST_SZ[st] = out[pos + len(names)]
        pos += len(names) + 1
    HOST_SIN = tuple(out[pos:pos + 3])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

_TRUE_SIN = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(4096)]
TYPE_FLOATS = {"mF24": (0, 1.5), "mF28": (-0.5, 1.5), "mF2C": (-0.5, 1.5), "mF44": (0.25, 30), "mF54": (0.3, 3), "mF60": (-2, 2),
               "mF64": (-2, 2), "mF68": (-2, 2), "mF6C": (0, 2), "mF320": (0, 5), "mF324": (0, 5), "mF32C": (-2, 2), "mF330": (-2, 2),
               "mF334": (-2, 2), "mF338": (-2, 2), "mF340": (-2, 2), "mF344": (-2, 2), "mF348": (-2, 2), "mF34C": (-1, 3),
               "mF350": (-1, 3), "mF354": (-1, 5), "mF358": (-1, 5)}
OVERRIDE = re.compile(r"^([A-Za-z]+[0-9]*)(?:\[([0-9]+)\])?\.(.+)$")
_MEMO = {}

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


def cf(r, lo, hi):
    """A float's bits in [lo, hi], now and then a boundary or a round value."""
    if hi < lo:
        lo, hi = hi, lo
    if r.random() < 0.12:
        x = min(max(r.choice([lo, hi, 0.0, 1.0, -1.0, 0.5, 0.1, (lo + hi) / 2]), lo), hi)
    else:
        x = r.uniform(lo, hi)
    return fb(x)


def fgen(r, clean):
    if clean:
        return lambda lo, hi: cf(r, lo, hi)
    return lambda lo, hi: rf(r)


def gen_globals(r, clean):
    F = fgen(r, clean)

    def pick(name, lo, hi):
        return [START.get(name, 0) if r.random() < 0.4 else F(lo, hi)]

    g = {"gTrackGravity": pick("gTrackGravity", 0.2, 3.0),
         "gCoasterMinTrains": [r.choice([1, 1, 2, 3, 0, MASK]) if clean else r.choice([1, 2, 0, MASK, 100, 0x7FFFFFFF, 0x80000000])],
         "gBankFadeStartMin": pick("gBankFadeStartMin", -2, 5), "gBankFadeMax": pick("gBankFadeMax", 0.2, 1.5),
         "gBankFadeMin": pick("gBankFadeMin", -0.3, 0.3), "gExciteBankFloorA": pick("gExciteBankFloorA", -1, 2),
         "gExciteBankFloorB": pick("gExciteBankFloorB", -1, 2), "gExciteFloor": pick("gExciteFloor", -1, 2),
         "gRatingBMax": pick("gRatingBMax", 0, 200), "gRatingBMin": pick("gRatingBMin", -5, 50),
         "gRatingAMax": pick("gRatingAMax", 0, 200), "gRatingAMin": pick("gRatingAMin", -5, 50),
         "gExciteRiseA": [r.choice([0, 1, 2, MASK, r.getrandbits(32)])], "gExciteRiseB": [r.choice([0, 1, 2, MASK, r.getrandbits(32)])],
         "gExcitePrevB": [F(-5, 20)], "gExciteMaxB": [F(0, 50)], "gExciteMaxA": [F(0, 50)], "gExcitePrevBank": [F(-3, 3)],
         "gExcitePrevG": [F(-5, 5), F(-5, 5), F(-5, 5)], "gExciteWord": [r.getrandbits(32)], "gExciteB": [F(0, 20)],
         "gExciteA": [F(0, 20)]}
    return g


def gen_sin(r, clean):
    mask = r.choice([0xFFF, 0xFFF, 0xFFF, 0x7FF, 0xFF, 0, 0x555])
    if r.random() < 0.65:
        return (_TRUE_SIN, mask, fb(4096 / (2 * math.pi)))
    if clean:
        return ([fb(r.uniform(-1, 1)) for _ in range(4096)], mask, fb(r.uniform(100, 1000)))
    return ([rf(r) for _ in range(4096)], mask, rf(r))


def world(case):
    key = (case["op"], case["seed"], repr(sorted(case["p"].items())), repr(sorted(case["o"].items())))
    if key not in _MEMO:
        if len(_MEMO) > 6:
            _MEMO.clear()
        _MEMO[key] = _world(case)
    return _MEMO[key]


def _world(case):
    op, p = case["op"], case["p"]
    r = random.Random(case["seed"])
    clean = p.get("clean", False)
    F = fgen(r, clean)
    w = {"objs": [], "raw": [], "g": {}, "sin": None, "by": {}}
    if op == "modf":
        w["raw"].append(("whole", 4, [0xA5A5A5A5]))
        return w
    step = p.get("step", False)
    need_l = step or op == "spread"

    # the car types, the sample spacing and from them the span a train covers (the track must have room for it)
    ct = [{"mF24": F(0, 0.8), "mF28": F(0, 0.8)} for _ in range(NCT)]
    fac = F(p.get("facmin", 0.8), 4.0)
    span = 0.0
    if clean:
        span = 5 * (max(bitsf(c["mF24"]) for c in ct) + max(bitsf(c["mF28"]) for c in ct)) / bitsf(fac) * 1.02 + 0.05

    cu_lo, cu_hi = p.get("cu", (1, 6))
    trains = [{"mFlags": r.getrandbits(3) | (r.getrandbits(8) << 8), "mF08": r.randrange(3), "mCarsInUse": r.randint(cu_lo, cu_hi),
               "mFrontSample": rf(r), "mRearSample": rf(r), "mCars": "cars%d" % i, "mCentreCar": r.randrange(NCARS),
               "mF30": cf(r, -0.9, 12) if op == "posns" else F(0, 12), "mPosition": rf(r), "mF78": rf(r)} for i in range(NT)]

    n = p.get("n", 6)
    if op == "posns":
        n = max(n, int(max(bitsf(t["mF30"]) for t in trains) + span) + 4)
    elif need_l:
        n = max(n, int(span) + 6)
    length = cf(r, 0.5, n - 3 - span) if need_l else F(0.5, 30)
    lf = bitsf(length) if clean else 1.0

    cars = []
    for i in range(NT):
        cars.append([{"mFlags": r.getrandbits(32), "mType": "ctype%d" % r.randrange(NCT), "mRide": r.randrange(5), "mF38": F(-3, 3),
                      "mF40": cf(r, -lf, lf) if need_l else F(-3, 20)} for _ in range(NCARS)])

    ns = n + 4
    samples, pieces = [], []
    for i in range(ns):
        if op == "spread":
            speed = fb(r.uniform(0.25, 10)) if r.random() < 0.5 else fb(r.uniform(0.25, 400))
        else:
            speed = F(0.25, 400)
        if p.get("bad") and r.random() < p["bad"]:      # NaN (the spread ends at it), and for the positions' average other bad speeds
            speed = r.choice([NAN, NAN, 0xFFC00000] if op == "spread" else [NAN, PINF, NINF, 0, 0x80000000, fb(-3.0), fb(1e30)])
        samples.append({"mFlags": r.getrandbits(32), "mPos.x": F(-20, 20), "mPos.y": F(-20, 20), "mPos.z": F(-20, 20), "mYaw": F(-4, 4),
                        "mPitch": F(-4, 4), "mBank": F(-4, 4), "mSpeed": speed, "mG.x": F(-5, 5), "mG.y": F(-5, 5), "mG.z": F(-5, 5),
                        "mPiece": ("pieces", i) if (1 <= i <= n - 2 or r.random() < 0.7) else None})
        pieces.append({"mBrace": "brace%d" % r.randrange(NB)})

    types = []
    for _ in range(2):
        t = {"mFlags": (r.getrandbits(32) & ~0x108 & MASK) | (0x100 if r.random() < 0.6 else 0) | (8 if r.random() < 0.6 else 0)}
        for f, (lo, hi) in TYPE_FLOATS.items():
            t[f] = F(lo, hi)
        t["mF2E8"] = r.randint(*p.get("maxtr", (0, 12)))
        t["mF2EC"] = r.randrange(0, 20000)
        types.append(t)
    m0 = bitsf(types[0]["mF44"])
    if clean:
        limit = fb(m0 + r.uniform(0, 5)) if r.random() < 0.4 else cf(r, m0, 400.0)
    else:
        limit = rf(r)
    if need_l:                         # the interval of the spread: at most about sixty steps of the fastest sample
        good = [bitsf(smp["mSpeed"]) for smp in samples if 0.0 < bitsf(smp["mSpeed"]) < math.inf]
        top = bitsf(limit) if op == "calc" else max(good or [1.0])
        for t in types:
            t["mF2EC"] = 0 if r.random() < 0.08 else int(r.uniform(0, 60) * bitsf(fac) / top * 1000)

    order = r.sample(range(NB), r.randint(0, NB))              # the brace chain from mLastBrace
    braces = []
    for b in range(NB):
        nxt = None
        if b in order and order.index(b) + 1 < len(order):
            nxt = "brace%d" % order[order.index(b) + 1]
        braces.append({"mFlags": r.getrandbits(32), "mPrev": nxt, "mSample": r.randint(1, n) if b == 0 else r.randrange(0, 2 * n)})
    sets = []
    for k in range(1, 5):
        sets.append({"mFlags": (r.getrandbits(32) & ~0x4000 & MASK) | (0x4000 if r.random() < 0.5 else 0), "mHeads": "heads%d" % k,
                     "mFC0": r.getrandbits(32), "mFC4": r.getrandbits(32)})

    flags = ((r.getrandbits(32) | p.get("fon", 0)) & ~p.get("foff", 0)) & MASK
    state = r.getrandbits(32)
    if p.get("state") == "on":
        state |= 0xC
    elif p.get("state") == "off":
        state &= ~0xC & MASK
    inst = {"mFlags": flags, "mType": "type0", "mState": state, "mSpeedLimit": limit,
            "mLastBrace": ("brace%d" % order[0]) if order else None, "mBraces": "brace0", "mSamples": "samples", "mSampleCount": n,
            "mLength": length, "mFAC": fac, "mTrainCount": r.randint(*p.get("tc", (0, 8))), "mRatingA": rf(r), "mRatingB": rf(r),
            "mMarkedBraces": r.randrange(12), "mTrackLength": rf(r), "mTopHeight": rf(r), "mTopSpeed": rf(r),
            "mLeadTrain": ("trains", r.randrange(NT)) if r.random() < 0.7 else None, "mTrains": ("trains", 0), "mBankFadeStart": rf(r)}

    objs = [("inst", "inst", [inst]), ("type0", "type", [types[0]]), ("type1", "type", [types[1]])]
    objs += [("ctype%d" % k, "ctype", [ct[k]]) for k in range(NCT)]
    objs += [("trains", "train", trains)] + [("cars%d" % i, "car", cars[i]) for i in range(NT)]
    objs += [("samples", "sample", samples), ("pieces", "piece", pieces)]
    objs += [("brace%d" % b, "brace", [braces[b]]) for b in range(NB)] + [("set%d" % k, "set", [sets[k - 1]]) for k in range(1, 5)]
    w["objs"] = objs
    w["by"] = {name: elems for name, _, elems in objs}
    w["raw"] = [("heads%d" % k, 16, []) for k in range(1, 5)]
    if op == "excite":
        w["gblock"] = [F(-5, 5), F(-5, 5), F(-5, 5)]
        w["raw"].append(("gblock", 12, w["gblock"]))
    w["g"] = gen_globals(r, clean)
    if op == "calc":
        w["sin"] = gen_sin(r, clean)
    for key, val in case["o"].items():
        if key.startswith("gblock."):
            w["gblock"][int(key[7:])] = val
            continue
        if key.startswith("g."):
            name, idx = key[2:].rsplit(".", 1)
            w["g"][name][int(idx)] = val
            continue
        m = OVERRIDE.match(key)
        w["by"][m.group(1)][int(m.group(2) or 0)][m.group(3)] = val
    return w


def pack_block(s, st, elems):
    size, off, kind = s.sz[st], s.off[st], KIND[st]
    buf = bytearray(size * len(elems))
    for i, el in enumerate(elems):
        for f, v in el.items():
            at = i * size + off[f]
            k = kind[f]
            if k == "p":
                struct.pack_into(s.pf, buf, at, s.resolve(v))
            elif k == "h2":
                struct.pack_into(s.f2, buf, at, (v >> 16) & 0xFFFF, v & 0xFFFF)
            else:
                struct.pack_into(s.fmt, buf, at, v & MASK)
    return bytes(buf)


def unpack_block(s, st, data, count):
    size, off, kind = s.sz[st], s.off[st], KIND[st]
    out = []
    for i in range(count):
        vals = {}
        for f, o in off.items():
            at = i * size + o
            k = kind[f]
            if k == "p":
                vals[f] = s.label(struct.unpack_from(s.pf, data, at)[0])
            elif k == "h2":
                hi, lo = struct.unpack_from(s.f2, data, at)
                vals[f] = (hi << 16) | lo
            else:
                v = struct.unpack_from(s.fmt, data, at)[0]
                vals[f] = canon_f(v) if k == "f" else v
        out.append(vals)
    return out


def build(s, case, w):
    """Writes the case's blocks and words through Side: the same code for both memories."""
    s.names, s.regions = {}, []
    for name, st, elems in w["objs"]:
        esz = s.sz[st]
        a = s.alloc(esz * len(elems))
        s.names[name] = (a, esz)
        s.regions.append((a, a + esz * len(elems), name, esz))
    for name, nbytes, words in w["raw"]:
        a = s.alloc(nbytes)
        s.names[name] = (a, nbytes)
        s.regions.append((a, a + nbytes, name, nbytes))
    for name, st, elems in w["objs"]:
        s.write(s.addr(name), pack_block(s, st, elems))
    for name, nbytes, words in w["raw"]:
        if words:
            s.words(s.addr(name), words)
    if case["op"] != "modf":
        for name, _, kind, count in GLOBALS:
            s.words(s.g(name), w["g"][name])
        table = s.g("gRideAnimSets")
        s.write(table, struct.pack(s.pf, 0))
        for k in range(1, 5):
            s.write(table + k * s.pw, struct.pack(s.pf, s.addr("set%d" % k)))
        if w["sin"]:
            tbl, mask, scale = w["sin"]
            g = s.g("gTPGlobals")
            s.words(g + s.sin[0], [mask])
            s.words(g + s.sin[1], [scale])
            s.write(g + s.sin[2], struct.pack(s.fmt[0] + "4096I", *tbl))
    fixed = [(label, s.addr(name, i)) for label, name, i in LABELS if name in s.names]
    types = [s.addr("type0"), s.addr("type1")] if "type0" in s.names else [0, 0]
    s.begin(fixed, {k: list(v) for k, v in case["queues"].items()}, types)


def execute(s, case, w):
    """The call."""
    op, a = case["op"], case["a"]
    if op == "modf":
        return fbs(s.call("modf", [("f", a["v"]), ("p", s.addr("whole") if a["w"] else 0)]))
    inst = ("p", s.addr("inst"))
    if op == "excite":
        smp = a["s"]
        if a["gsep"]:
            gaddr, gz = s.addr("gblock"), w["gblock"][2]
        else:
            gaddr, gz = s.addr("samples", smp) + s.off["sample"]["mG.x"], w["by"]["samples"][smp]["mG.z"]
        s.call("excite", [inst, ("i", smp), ("i", a["w"]), ("f", a["sp"]), ("f", a["bk"]), ("p", gaddr), ("f", a["t"])], f0=gz)
    elif op == "spread":
        s.call("spread", [inst, ("f", a["t"])])
    elif op == "collate":
        s.call("collate", [inst, ("f", a["h"]), ("f", a["sp"]), ("f", a["t"])])
    elif op == "capacity":
        s.call("capacity", [inst, ("i", a["trains"]), ("i", a["unused"])])
    else:                                   # calc, posns, active, initial
        s.call(op, [inst])
    return None


def observe(s, case, w, answer):
    """Everything the call may change, read back the same way on both sides; pointers as names, floats canon_f'd."""
    obs = {"answer": answer, "log": s.log()}
    if case["op"] == "modf":
        obs["whole"] = s.rd_word(s.addr("whole"))
        return obs
    for name, st, elems in w["objs"]:
        data = s.read(s.addr(name), len(elems) * s.sz[st])
        for i, vals in enumerate(unpack_block(s, st, data, len(elems))):
            for f, v in vals.items():
                obs["%s.%d.%s" % (name, i, f)] = v
    for name, _, kind, count in GLOBALS:
        for i, v in enumerate(s.rd_words(s.g(name), count)):
            obs["g.%s.%d" % (name, i)] = canon_f(v) if kind == "f" else v
    return obs


def run(s, case):
    try:
        w = world(case)
        build(s, case, w)
        return observe(s, case, w, execute(s, case, w))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


NAN, PINF, NINF = 0x7FC00000, 0x7F800000, 0xFF800000
BAD = [NAN, NAN, 0xFFC00000, PINF, NINF]
FLOAT_GLOBALS = ["g.%s.%d" % (name, i) for name, _, kind, count in GLOBALS if kind == "f" for i in range(count)]
SAMPLE_FLOATS = ["mPos.x", "mPos.y", "mPos.z", "mYaw", "mPitch", "mBank", "mSpeed", "mG.x", "mG.y", "mG.z"]


def seed_of(r):
    return r.randrange(1 << 30)


def poison(r, a, o, groups):
    """One NaN or infinity into a float the function reads: a group (a list of keys) at random, then a key of it; "a.x" is a call argument."""
    key = r.choice(r.choice(groups))
    if key.startswith("a."):
        a[key[2:]] = r.choice(BAD)
    else:
        o[key] = r.choice(BAD)


def mk(op, seed, p=None, o=None, a=None, q=None):
    return {"op": op, "seed": seed, "p": p or {}, "o": o or {}, "a": a or {}, "queues": q or {}}


def q_angle(sd, wild):
    """The angles GetVectorRotationAroundZAxisFromY answers: around [-pi - 0.5, pi + 0.5], the exact turns now and then."""
    rr = random.Random(sd ^ 0x55AA55)
    out = []
    for _ in range(30):
        k = rr.random()
        if wild and k < 0.06:
            out.append(rr.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000]))
        elif k < 0.18:
            out.append(fb(rr.choice([0.0, math.pi, -math.pi, math.pi / 2, -math.pi / 2, 3.1415925, 4.0, -4.0])))
        else:
            out.append(fb(rr.uniform(-3.7, 3.7)))
    return out


def lst(r, choices, hi=3):
    return [r.choice(choices) for _ in range(r.randint(1, hi))]


# ---- ModF
MODF_VALUES = [0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 2.5, -2.5, 1e9, 2147483647.0, 2147483648.0, 3e9, -3e9, -2147483648.0, -2147483904.0,
               math.inf, -math.inf, math.nan, 8388607.5, 16777216.0, 1e30, -1e30, 123456.789, 0.99999994, -0.99999994, 1.0000001]


def modf_edges():
    r = random.Random(2121)
    for x in MODF_VALUES + [bitsf(0x00000001), bitsf(0x80000001), bitsf(0x7FC00001)]:
        for whole in (1, 0):
            yield mk("modf", seed_of(r), {}, {}, {"v": fb(x), "w": whole})


def modf_random(r):
    k = r.random()
    if k < 0.3:
        v = rf(r)
    elif k < 0.6:
        v = fb(r.uniform(-1e10, 1e10))
    elif k < 0.8:
        v = fb(r.randint(-5000, 5000) + r.choice([0.0, 0.25, 0.5, 0.75, r.random()]))
    else:
        v = fb(r.uniform(-3, 3))
    return mk("modf", seed_of(r), {}, {}, {"v": v, "w": 0 if r.random() < 0.2 else 1})


# ---- SetCoasterCapacity
def q_capacity(r):
    q = {}
    if r.random() < 0.5:
        q["utc"] = lst(r, [KNONE, KNONE, 0, 1, 2, 3, 5, 8])
    if r.random() < 0.4:
        q["utb"] = lst(r, [0, 1]) + [0]
    return q


def capacity_case(r, trains=None, o=None, q=None):
    sd = seed_of(r)
    if trains is None:
        trains = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF])
    p = {"n": 4, "tc": (0, 8), "maxtr": (0, 12)}
    if r.random() < 0.5:
        p["state"] = "on"
    return mk("capacity", sd, p, o, {"trains": trains, "unused": r.getrandbits(32)}, q_capacity(r) if q is None else q)


def capacity_edges():
    r = random.Random(2122)
    for tr, tc, st in [(0, 5, 0xC), (3, 5, 0xC), (3, 5, 4), (3, 5, 8), (3, 5, 0x10), (3, 5, 0), (3, 5, 0xFFFFFFF3), (5, 5, 0xC), (6, 5, 0xC),
                       (9, 5, 0xC), (1, 2, 0xC), (1, 1, 0xC), (2, 0, 0xC), (0xFFFFFFFF, 5, 0xC), (0x80000000, 5, 0xC),
                       (0x7FFFFFFF, 5, 0xC), (1, 8, 0xC), (1, 8, 0xFFFFFFFF)]:
        yield capacity_case(r, tr, {"inst.mTrainCount": tc, "inst.mState": st}, {})
    # the train count and the train base are read again after each unload
    yield capacity_case(r, 1, {"inst.mTrainCount": MASK, "inst.mState": 0xC}, {"utc": [3]})
    yield capacity_case(r, 1, {"inst.mTrainCount": 6, "inst.mState": 0xC}, {"utc": [2, 4, KNONE]})
    yield capacity_case(r, 2, {"inst.mTrainCount": 6, "inst.mState": 0xC}, {"utc": [KNONE], "utb": [1, 1, 0]})
    yield capacity_case(r, 0, {"inst.mTrainCount": 6, "inst.mState": 0xC}, {"utc": [2], "utb": [1, 0]})


# ---- UpdateCoasterTrainsActive
def q_active(r):
    q = {}
    if r.random() < 0.3:
        q["atype"] = lst(r, [KNONE, 0, 1])
    if r.random() < 0.3:
        q["atc"] = lst(r, [KNONE, 0, 1, 2, 3, 6, 10])
    if r.random() < 0.3:
        q["aflags"] = lst(r, [KNONE, 0, 0x800, 0x1800, 0x7FF])
    if r.random() < 0.3:
        q["sflags"] = lst(r, [KNONE, 0, 1, 2, 3, 5, 7])
    return q


def active_case(r, o=None, q=None):
    sd = seed_of(r)
    o = dict(o or {})
    if not o and r.random() < 0.08:
        o["inst.mTrainCount"] = MASK
    p = {"n": 4, "tc": (0, 10), "maxtr": (0, 12), "cu": (0, 6)}
    if r.random() < 0.7:
        p["fon"] = 0x800
    return mk("active", sd, p, o, {}, q_active(r) if q is None else q)


def active_edges():
    r = random.Random(2123)
    for f2e8, tc, fl in [(0, 3, 0x800), (4, 4, 0x800), (4, 2, 0x800), (4, 2, 0), (4, 0, 0x800), (4, MASK, 0x800), (6, 3, 0x800), (10, 1, 0x800),
                         (1, 0, 0x800), (1, 1, 0x800), (12, 6, 0x800)]:
        o = {"type0.mF2E8": f2e8, "inst.mTrainCount": tc, "inst.mFlags": fl}
        for i in range(min(f2e8, NT)):          # trains alternate between on and off
            o["trains[%d].mFlags" % i] = 1 if i % 2 == 0 else r.choice([0, 2, 4, 6, 7])
        yield active_case(r, o, {})
    # one train outside the count with cars of every ride, the two ways a ride's meshes are switched off
    for ride in range(5):
        for setflag in (0x4000, 0):
            o = {"type0.mF2E8": 2, "inst.mTrainCount": 1, "inst.mFlags": 0x800, "trains[1].mFlags": 3, "trains[1].mF08": 1,
                 "trains[1].mCarsInUse": 3, "set%d.mFlags" % max(ride, 1): setflag}
            for j in range(1, 4):
                o["cars1[%d].mRide" % j] = ride
            yield active_case(r, o, {})
    yield active_case(r, {"type0.mF2E8": 4, "inst.mTrainCount": 2, "inst.mFlags": 0x800, "trains[2].mFlags": 3, "trains[3].mFlags": 3},
                      {"atype": [1], "atc": [KNONE], "aflags": [0x800], "sflags": [0]})
    yield active_case(r, {"type0.mF2E8": 4, "type1.mF2E8": 1, "inst.mTrainCount": 3, "inst.mFlags": 0x800, "trains[0].mFlags": 0,
                          "trains[1].mFlags": 0, "trains[2].mFlags": 0}, {"atype": [1]})
    yield active_case(r, {"type0.mF2E8": 4, "type1.mF2E8": 12, "inst.mTrainCount": 2, "inst.mFlags": 0x800, "trains[0].mFlags": 0},
                      {"atype": [1], "atc": [0], "aflags": [0]})


# ---- UpdateCoasterTrainPosns
def posns_case(r, o=None, p2=None, inject=True):
    sd = seed_of(r)
    p = {"clean": True, "tc": (0, 8), "n": r.randint(6, 16), "facmin": 0.5}
    if inject and r.random() < 0.25:                # speeds that are not numbers, infinite, zero or negative
        p["bad"] = 0.3
    if p2:
        p.update(p2)
    return mk("posns", sd, p, o, {}, {})


def posns_edges():
    r = random.Random(2124)
    case = lambda *args: posns_case(*args, inject=False)
    flat = {"ctype%d.mF24" % k: 0 for k in range(NCT)}
    flat.update({"ctype%d.mF28" % k: 0 for k in range(NCT)})
    yield case(r, {"inst.mTrainCount": 0})
    yield case(r, {"inst.mTrainCount": 1, "trains[0].mCarsInUse": 1})
    yield case(r, {"inst.mTrainCount": 1, "trains[0].mCarsInUse": 2})
    yield case(r, {"inst.mTrainCount": 3})
    for f30 in (0.0, -0.0, 3.0, -0.5, -0.9, 2.25, 7.999):               # whole, signed zero and negative fractions, all cars on one place
        o = dict(flat)
        o.update({"inst.mTrainCount": 2, "trains[0].mF30": fb(f30), "trains[1].mF30": fb(f30)})
        yield case(r, o)
    for f08 in (0, 1, 2):
        for cu in (1, 4, 6):
            yield case(r, {"inst.mTrainCount": 2, "trains[0].mF08": f08, "trains[0].mCarsInUse": cu, "trains[0].mCentreCar": cu - 1,
                                 "trains[1].mF08": f08, "trains[1].mCarsInUse": cu, "trains[1].mCentreCar": 0})


# ---- SpreadTrainsOnTrack
def spread_case(r, o=None, p2=None, inject=True):
    sd = seed_of(r)
    p = {"clean": True, "tc": (1, 6), "n": r.randint(8, 20), "maxtr": (1, 8)}
    if inject and r.random() < 0.15:                # a NaN speed ends the stepping at once; the next train would start at a NaN place
        p["bad"] = 0.25
        o = dict(o or {}, **{"inst.mTrainCount": 2})
    if p2:
        p.update(p2)
    return mk("spread", sd, p, o, {"t": rf(r)}, {})


def spread_edges():
    r = random.Random(2125)
    case = lambda *args: spread_case(*args, inject=False)
    for tc in (1, 2, 3, 6):
        yield case(r, {"inst.mTrainCount": tc})
    for ms in (0, 1, 7, 400):
        yield case(r, {"type0.mF2EC": ms, "inst.mTrainCount": 3})
    for f40 in (0.0, -0.0, 0.5, -0.5, 1.0, -1.0):                       # the last car's place: zero of both signs, either side, whole
        o = {"inst.mTrainCount": 2, "inst.mLength": fb(5.5)}
        for i in range(2):
            for j in range(NCARS):
                o["cars%d[%d].mF40" % (i, j)] = fb(f40)
        yield case(r, o, {"n": 12})
    for k in range(3):                                                   # all speeds equal, the interval a whole number of steps
        o = {"inst.mTrainCount": 4, "inst.mFAC": fb(1.0), "type0.mF2EC": 3000}
        for i in range(24):
            o["samples[%d].mSpeed" % i] = fb(1.0 + k)
        yield case(r, o, {"n": 20})
    for v in (NAN, PINF, NINF):                                          # the last car's place that is not a number: one train, no place to find from it
        yield case(r, dict({"inst.mTrainCount": 1}, **{"cars0[%d].mF40" % j: v for j in range(NCARS)}))
    for bad in (1.0, 0.5, 0.2, 0.2):                                     # a speed that is not a number ends the stepping (two trains: one place to find)
        yield case(r, {"inst.mTrainCount": 2}, {"bad": bad})


# ---- CollateCoasterStats
def collate_case(r, o=None, a=None, inject=True):
    sd = seed_of(r)
    o, args = dict(o or {}), {"h": rf(r), "sp": rf(r), "t": rf(r)}
    args.update(a or {})
    if inject and r.random() < 0.3:
        poison(r, args, o, [FLOAT_GLOBALS, ["inst.mLength", "inst.mFAC"], ["a.h", "a.sp"]])
    return mk("collate", sd, {"n": 4, "clean": r.random() < 0.5}, o, args, {})


def collate_edges():
    r = random.Random(2126)
    for ba, bb in ((0x40, 0x80), (0xC0, 0xC0), (0x40, 0x40), (0x80, 0x80), (0, 0), (0xFFFFFFFF, 0xC0)):
        yield collate_case(r, {"inst.mLastBrace": "brace0", "brace0.mFlags": ba, "brace0.mPrev": "brace1", "brace1.mFlags": bb,
                               "brace1.mPrev": None}, inject=False)
    yield collate_case(r, {"inst.mLastBrace": None}, inject=False)
    for a, b, lo_, hi_ in ((10.0, 20.0, 0.0, 15.0), (10.0, 20.0, 12.0, 100.0), (-5.0, 3.0, 0.0, 100.0), (0.0, 0.0, 0.0, 0.0),
                           (math.nan, 1.0, 0.0, 5.0), (1.0, math.nan, 0.0, 5.0), (math.inf, 1.0, 0.0, 5.0), (1.0, -math.inf, 0.0, 5.0),
                           (3.0, 4.0, 7.0, 7.0), (50.0, 50.0, 100.0, 100.0)):
        o = {"g.gExciteA.0": fb(a), "g.gExciteMaxA.0": fb(b), "g.gExciteB.0": fb(a), "g.gExciteMaxB.0": fb(b), "g.gRatingAMin.0": fb(lo_),
             "g.gRatingAMax.0": fb(hi_), "g.gRatingBMin.0": fb(lo_), "g.gRatingBMax.0": fb(hi_)}
        yield collate_case(r, o, inject=False)
    for name in ("gRatingAMax", "gRatingAMin", "gRatingBMax", "gRatingBMin", "gExciteA", "gExciteB", "gExciteMaxA", "gExciteMaxB"):
        yield collate_case(r, {"g.%s.0" % name: NAN}, inject=False)     # a rating or a sum that is not a number: the clamps' unordered side
    yield collate_case(r, {"inst.mLength": NAN}, inject=False)
    yield collate_case(r, {"inst.mFAC": NAN}, inject=False)
    yield collate_case(r, {}, {"h": NAN}, inject=False)
    yield collate_case(r, {}, {"sp": NAN}, inject=False)
    yield collate_case(r, {}, {"h": PINF, "sp": NINF}, inject=False)


# ---- SetInitialExcitementValues
def initial_case(r, o=None, inject=True):
    o = dict(o or {})
    if inject and r.random() < 0.3:
        poison(r, {}, o, [["type0.mF320", "type0.mF324"]])
    return mk("initial", seed_of(r), {"n": 4, "clean": r.random() < 0.5}, o, {}, {})


def initial_edges():
    r = random.Random(2127)
    yield initial_case(r, inject=False)
    yield initial_case(r, {"type0.mF320": fb(-0.0), "type0.mF324": fb(-0.0)}, inject=False)
    yield initial_case(r, {"type0.mF320": 0x7FC00000, "type0.mF324": 0x7F800000}, inject=False)
    yield initial_case(r, {"type0.mF320": fb(3.5), "type0.mF324": fb(-2.5)}, inject=False)


# ---- UpdateExcitementAtSample
def excite_case(r, a=None, o=None, inject=True):
    sd = seed_of(r)
    o = dict(o or {})
    clean = r.random() < 0.6
    F = fgen(r, clean)
    bank = r.choice([F(-4, 4), F(-4, 4), fb(math.pi / 2), fb(-math.pi / 2), 0, 0x80000000, fb(math.pi), fb(1.6), fb(-3.2)])
    args = {"s": r.choice([0, 1, 2, 3, 4, 5, 6, r.randrange(7)]), "w": r.getrandbits(32), "sp": F(0, 40), "bk": bank, "t": rf(r),
            "gsep": r.random() < 0.5}
    args.update(a or {})
    if inject and r.random() < 0.35:
        g = ["gblock.%d" % i for i in range(3)] if args["gsep"] else ["samples[%d].mG.%s" % (args["s"], c) for c in "xyz"]
        poison(r, args, o, [FLOAT_GLOBALS, g, ["a.sp", "a.bk"]])
    return mk("excite", sd, {"n": 8, "clean": clean}, o, args, {})


def excite_edges():
    r = random.Random(2128)
    for s in (0, 1, 2, 3, 4, 6):                                          # the rise tests start at the fourth sample
        yield excite_case(r, {"s": s}, inject=False)
    for gy in (0.0, -0.0, 0.1, -0.1, 0.2, bitsf(fb(0.1) - 1), bitsf(fb(0.1) + 1), -5.0, math.nan):
        for bank in (0.0, math.pi, -math.pi / 2, 2.0):
            for gsep in (1, 0):
                o = {"g.gExcitePrevBank.0": fb(0.0), ("gblock.1" if gsep else "samples[3].mG.y"): fb(gy)}
                yield excite_case(r, {"gsep": gsep, "bk": fb(bank), "s": 3}, o, inject=False)
    for f in (0, 1):
        for fl in (0.0, 0.5, -1.0):                                       # the floors of the two banking terms against |bank| - pi / 2
            yield excite_case(r, {"bk": fb(2.5), "gsep": f}, {"g.gExciteBankFloorA.0": fb(fl), "g.gExciteBankFloorB.0": fb(fl)}, inject=False)
    for rise_a, rise_b in ((0, 0), (1, 0), (0, 1), (1, 1), (2, MASK)):    # the state of the two rise flags
        yield excite_case(r, {"s": 5}, {"g.gExciteRiseA.0": rise_a, "g.gExciteRiseB.0": rise_b}, inject=False)
    # not a number: the bank (the two banking terms and the pull come from it), the speed, the time and the word, the three g components
    for bank in (NAN, PINF, NINF, 0x80000000):
        for gsep in (1, 0):
            yield excite_case(r, {"bk": bank, "gsep": gsep, "s": 3}, inject=False)
    for arg, val in (("sp", NAN), ("sp", PINF), ("t", NAN), ("t", NINF), ("w", MASK)):
        yield excite_case(r, {arg: val, "bk": fb(2.5), "s": 4}, inject=False)
    for i in range(3):
        for val in (NAN, PINF):
            yield excite_case(r, {"bk": fb(2.5), "gsep": 1, "s": 4}, {"gblock.%d" % i: val}, inject=False)
    for key in FLOAT_GLOBALS:                                             # each float the function reads, not a number
        for gsep in (1, 0):
            yield excite_case(r, {"bk": fb(2.5), "s": 4, "gsep": gsep}, {key: NAN}, inject=False)


# ---- CalculateTrackGForcesSpeedAndExcitement
def calc_mode(sd, mode, r, n=None, o=None, p2=None, inject=False):
    p = {"clean": mode != "wild"}
    o = dict(o or {})
    if mode == "step":
        p.update(step=True, n=n or r.randint(16, 22), fon=0x1000, foff=0x4000, tc=(1, 6), maxtr=(1, 8))
    else:
        p.update(n=n or r.randint(3, 14), tc=(0, 8), maxtr=(0, 12))
        if r.random() < 0.5:
            p["foff"] = 0x1000
        else:
            p["fon"] = 0x5000
    if p2:
        p.update(p2)
    if inject and mode != "step" and r.random() < 0.6:       # NaN or infinity into one float or two (a step world must stay sane)
        groups = [["inst.mSpeedLimit", "inst.mLength", "inst.mFAC"], ["type0." + f for f in TYPE_FLOATS], FLOAT_GLOBALS,
                  ["samples[%d].%s" % (i, f) for i in range(p["n"]) for f in SAMPLE_FLOATS]]
        for _ in range(r.randint(1, 2)):
            poison(r, {}, o, groups)
    return mk("calc", sd, p, o, {}, {"angle": q_angle(sd, mode == "wild")})


def calc_random(r):
    k = r.random()
    return calc_mode(seed_of(r), "step" if k < 0.4 else ("clean" if k < 0.7 else "wild"), r, inject=True)


def calc_edges():
    r = random.Random(2129)
    for n in (2, 3):                                                      # no inner sample, one
        for mode in ("clean", "wild"):
            yield calc_mode(seed_of(r), mode, r, n=n)
    for fl, mode in ((0x1000, "step"), (0, "clean"), (0x5000, "clean"), (0x4000, "clean"), (0x1800, "step"), (0x9000, "step")):
        yield calc_mode(seed_of(r), mode, r, o={"inst.mFlags": fl})
    for tf in (0, 0x100, 8, 0x108):                                       # the type's climb flag and banking flag
        for mode in ("clean", "step"):
            yield calc_mode(seed_of(r), mode, r, o={"type0.mFlags": tf})
    # the bank fade starts where the clamp puts it: the sample at the start is not faded (a tie), the next one is
    yield calc_mode(seed_of(r), "clean", r, o={"type0.mFlags": 8, "type0.mF44": fb(100.0), "inst.mSpeedLimit": fb(200.0),
                                               "g.gBankFadeStartMin.0": fb(2.0), "inst.mFAC": fb(1.0)})
    yield calc_mode(seed_of(r), "clean", r, o={"type0.mFlags": 8, "g.gBankFadeStartMin.0": fb(2.0), "g.gBankFadeMax.0": fb(0.5),
                                               "g.gBankFadeMin.0": fb(0.25)})
    # the fade clamps with a value that is not a number (a length that is, or a bound) and with the bounds the wrong way round
    fade = {"type0.mFlags": 8, "type0.mF44": fb(1.0), "inst.mSpeedLimit": fb(2.0), "inst.mFAC": fb(1.0), "g.gBankFadeStartMin.0": fb(-9.0)}
    for extra in ({"inst.mLength": NAN}, {"g.gBankFadeMax.0": NAN}, {"g.gBankFadeMin.0": NAN},
                  {"inst.mLength": NAN, "g.gBankFadeMax.0": fb(0.5), "g.gBankFadeMin.0": fb(0.25)},
                  {"g.gBankFadeMax.0": fb(0.2), "g.gBankFadeMin.0": fb(0.6)}, {"inst.mLength": PINF}, {"inst.mLength": fb(-4.0)}):
        yield calc_mode(seed_of(r), "clean", r, n=10, o=dict(fade, **extra))
    # the trains: the interval zero (the wanted count saturates), tiny; a huge one wants no train, so the minimum decides (and with one train
    # the spread has nothing to space: a huge interval with more would never end)
    for ms in (0, 1):
        yield calc_mode(seed_of(r), "step", r, o={"type0.mF2EC": ms})
    huge = {"type0.mF2EC": 100000000, "type0.mF2E8": 3}
    yield calc_mode(seed_of(r), "step", r, o=dict(huge, **{"g.gCoasterMinTrains.0": 1}))
    yield calc_mode(seed_of(r), "step", r, o=dict(huge, **{"g.gCoasterMinTrains.0": 0, "inst.mTrainCount": 1}))
    yield calc_mode(seed_of(r), "step", r, o=dict(huge, **{"g.gCoasterMinTrains.0": MASK, "inst.mTrainCount": 1}))
    yield calc_mode(seed_of(r), "step", r, o=dict(huge, **{"g.gCoasterMinTrains.0": 1, "inst.mState": 0xC, "inst.mTrainCount": 6}))
    # the minimum above the type's maximum (the maximum wins), a maximum of zero (the capacity is left alone)
    yield calc_mode(seed_of(r), "step", r, o={"g.gCoasterMinTrains.0": 3, "type0.mF2E8": 2})
    yield calc_mode(seed_of(r), "step", r, o={"g.gCoasterMinTrains.0": 3, "type0.mF2E8": 0, "inst.mTrainCount": 2})
    # the speed limit below the minimum, equal to it, and a minimum of zero (no spread step may run)
    yield calc_mode(seed_of(r), "clean", r, o={"inst.mSpeedLimit": fb(0.1), "type0.mF44": fb(5.0)})
    yield calc_mode(seed_of(r), "clean", r, o={"inst.mSpeedLimit": fb(5.0), "type0.mF44": fb(5.0)})
    yield calc_mode(seed_of(r), "clean", r, o={"type0.mF44": fb(0.0), "inst.mSpeedLimit": fb(0.0)})
    yield calc_mode(seed_of(r), "clean", r, o={"samples[2].mPos.x": 0, "samples[2].mPos.y": 0, "samples[2].mPos.z": 0, "samples[1].mPos.x": 0,
                                               "samples[1].mPos.y": 0, "samples[1].mPos.z": 0, "type0.mFlags": 0x108})


TESTS = [
    FnTest("CalculateTrackGForcesSpeedAndExcitement", ppc, host, edge=calc_edges, rnd=calc_random),
    FnTest("UpdateExcitementAtSample", ppc, host, edge=excite_edges, rnd=excite_case),
    FnTest("SpreadTrainsOnTrack", ppc, host, edge=spread_edges, rnd=spread_case),
    FnTest("UpdateCoasterTrainPosns", ppc, host, edge=posns_edges, rnd=posns_case),
    FnTest("UpdateCoasterTrainsActive", ppc, host, edge=active_edges, rnd=active_case),
    FnTest("CollateCoasterStats", ppc, host, edge=collate_edges, rnd=collate_case),
    FnTest("SetInitialExcitementValues", ppc, host, edge=initial_edges, rnd=initial_case),
    FnTest("SetCoasterCapacity", ppc, host, edge=capacity_edges, rnd=capacity_case),
    FnTest("ModF", ppc, host, edge=modf_edges, rnd=modf_random),
]
