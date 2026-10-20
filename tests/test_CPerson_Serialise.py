"""Unit v5972, CPerson::Serialise, against libtpw_unit_CPerson_Serialise.dylib (tests/unit_CPerson_Serialise.cpp), main app
("SimTheme Park"), src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_Serialise.cpp:
  CPerson::Serialise 0x101170b4

A case is a person block (0x240 bytes, gaps filled with a pattern) and a call. The block is built twice by ONE piece of Python code: in
emulator memory (big-endian) and in the host library's memory (little-endian), field by field with the field's own width. The callees
outside the unit are fakes on both sides: CPeep::Serialise and each SAVEVAR / LOADVAR instance. A save fake records (file, name index,
noLog, the value read through the pointer); a load fake records (file, name index, noLog) and stores the next value of the LOAD queue.
All answer from the RET queue (CPEEP for CPeep::Serialise), so a failure can be injected at any call.

What is compared: the answer, the event log, every field of the block and the bytes outside the fields. Fused operations: none; the
fctiwz of the small floats runs under the HardwareRounding hook. Not tested: a NULL file (never dereferenced by the unit).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CPerson_Serialise"
MASK = 0xFFFFFFFF

FN = 0x101170b4
SIZES = {FN: 3456}
OWN = set(SIZES)

CPEEP = 0x10113138
POOL = 0x102442be
FILE = 0x00007000
SIZE = 0x240

NAMES = [
    "mArrivalDate", "mArrivalIndex", "mBalloonScript", "mBeenAdmitted", "mCash", "mExitLevel", "pv", "mLastPosX", "mLastPosY",
    "mMajorDest", "mNumRides", "mNumShops", "mNumSideshows", "mNumSideshowsWon", "mPaidAdmission", "mParkOpeningWaitingTime",
    "mPersonType", "mPrankeryIndex", "mPreviousRides[i]", "mPreviousTemporaryRides[i]", "mQNext", "mQPrev", "mQueueMoveDelay",
    "mQueuePos", "mRemainingBalloonLife", "mSavedMajorDest", "mSavedState", "mState", "mTimeOfLastSpotAnim", "mTimeStartedIdling",
]

# (save address, load address, width in bytes)
VARS = [
    (0x1000f5dc, 0x1000f02c, 4),   # unsigned long
    (0x1000f384, 0x1000ee04, 4),   # long
    (0x1000f4b0, 0x1000ef18, 4),   # int
    (0x100f45bc, 0x100f44a4, 4),   # unsigned int
    (0x100f1a04, 0x100f17c8, 1),   # unsigned char
    (0x100f18d8, 0x100f16b4, 4),   # float
    (0x1000e9d4, 0x1000eb4c, 2),   # tag_IDX_TYPE (SAVEVARSTRUCT / LOADVARSTRUCT)
    (0x1011b90c, 0x1011b7cc, 4),   # CPerson::PersonState
]

# the fields of the block: (offset, width, is float)
FIELDS = []
for _o in (0x19c, 0x1a0, 0x1a4, 0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c4, 0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1f4, 0x1f8,
           0x1fc, 0x200, 0x204, 0x208, 0x210, 0x214, 0x218, 0x21c, 0x220, 0x224):
    FIELDS.append((_o, 4))
for _o in (0x1dc, 0x1de, 0x1e0, 0x1e2, 0x1e4, 0x1e6, 0x1e8, 0x1ea, 0x1ec, 0x1ee, 0x228, 0x22a):
    FIELDS.append((_o, 2))
for _o in (0x1c0, 0x1f0, 0x1f1):
    FIELDS.append((_o, 1))
FLOATS = (0x19c, 0x1a4, 0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x218, 0x21c)
HOST_LAYOUT = [2, 4]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_cpeep(e):
    L = e.cl
    L.rec("CPEEP", (e.r(4) & MASK, T.s32(e.r(5)), T.s32(e.r(6)), e.r(7) & MASK))
    e.set_r(3, L.ret("CPEEP") & MASK)


STUBS.internal(CPEEP, stub_cpeep)


def make_save(width):
    def stub(e):
        L = e.cl
        name = e.cstr(e.r(5))
        data = e.read(e.r(4), width)
        val = int.from_bytes(data, "big")
        if width == 4 and False:
            val = canon_f(val)
        L.rec("SAVE", (e.r(3) & MASK, NAMES.index(name) if name in NAMES else -1, T.s32(e.r(6)), SAVE_CANON(e.r(4), val, width)))
        e.set_r(3, L.ret("RET") & MASK)
    return stub


FLOAT_SAVE = 0x100f18d8


def SAVE_CANON(ptr, val, width):
    return val


def make_save_float():
    def stub(e):
        L = e.cl
        name = e.cstr(e.r(5))
        val = canon_f(int.from_bytes(e.read(e.r(4), 4), "big"))
        L.rec("SAVE", (e.r(3) & MASK, NAMES.index(name) if name in NAMES else -1, T.s32(e.r(6)), val))
        e.set_r(3, L.ret("RET") & MASK)
    return stub


def make_load(width):
    def stub(e):
        L = e.cl
        name = e.cstr(e.r(5))
        L.rec("LOAD", (e.r(3) & MASK, NAMES.index(name) if name in NAMES else -1, T.s32(e.r(6))))
        v = L.ret("LOAD") & MASK
        e.write(e.r(4), (v & ((1 << (8 * width)) - 1)).to_bytes(width, "big"))
        e.set_r(3, L.ret("RET") & MASK)
    return stub


for _s, _l, _w in VARS:
    STUBS.internal(_s, make_save_float() if _s == FLOAT_SAVE else make_save(_w))
    STUBS.internal(_l, make_load(_w))


def setup(runner):
    T.install_std_stubs(runner.emu)
    # the name pool: every text at its offset
    offs = [0x93, 0xa0, 0xae, 0xbd, 0xcb, 0xd1, 0xdc, 0xdf, 0xe9, 0xf3, 0xfe, 0x108, 0x112, 0x120, 0x131, 0x140, 0x158, 0x164, 0x173,
            0x185, 0x1a0, 0x1a7, 0x1ae, 0x1be, 0x1c8, 0x1de, 0x1ee, 0x1fa, 0x201, 0x215]
    pool = bytearray(0x230)
    for off, name in zip(offs, NAMES):
        pool[off:off + len(name) + 1] = name.encode() + b"\0"
    a = runner.emu.persist_alloc if hasattr(runner.emu, "persist_alloc") else None
    T.persist_write(runner.emu, POOL, bytes(pool)) if hasattr(T, "persist_write") else runner.emu.write(POOL, bytes(pool))


class Side:
    def put(self, a, width, v):
        self.write(a, (v & ((1 << (8 * width)) - 1)).to_bytes(width, self.order))

    def get(self, a, width):
        return int.from_bytes(self.read(a, width), self.order)


class Ppc(Side):
    order = "big"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, obj, save, version, unused):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [obj, FILE, save & MASK, version & MASK, unused & MASK])
        return r3 & MASK


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ser_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, save, version, unused):
        return self.lib.tpw_ser_call(obj, FILE, T.s32(save), T.s32(version), unused & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ser_alloc.restype, lib.tpw_ser_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ser_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ser_call.restype = C.c_uint32
    lib.tpw_ser_call.argtypes = [C.c_uint64, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ser_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    obj = s.alloc(SIZE)
    s.write(obj, bytes([0xAA]) * SIZE)
    for (off, width), v in zip(FIELDS, case["fields"]):
        s.put(obj + off, width, v)
    case["_obj"] = obj
    return obj


def observe(s, case, answer):
    obj = case["_obj"]
    fields = []
    covered = bytearray(SIZE)
    for off, width in FIELDS:
        v = s.get(obj + off, width)
        if off in FLOATS:
            v = canon_f(v)
        fields.append(v)
        for i in range(width):
            covered[off + i] = 1
    raw = s.read(obj, SIZE)
    other = bytes(b if not covered[i] else 0 for i, b in enumerate(raw))
    return {"answer": answer, "log": s.log(), "fields": fields, "other": other}


def run(s, case):
    try:
        obj = build(s, case)
        return observe(s, case, s.call(obj, case["save"], case["version"], case["unused"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------------ cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x3F800000, 0xBF800000, 0x00000001, 0x437F0000, 0x43800000, 0x43808000, 0xC3800000, 0xCF000000,
           0x4F000000]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-600, 600))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rw(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32), r.getrandbits(8), r.getrandbits(16)])


def make_case(r, save=None, version=None, fail=None, cpeep_fail=None):
    fields = []
    for off, width in FIELDS:
        fields.append(rf(r) if off in FLOATS else r.getrandbits(8 * width))
    if save is None:
        save = r.choice([0, 1])
    if version is None:
        version = r.choice([0, 1, 0, 1, -1, 2, 3, 100, -100])
    if fail is None:
        fail = r.choice([None, None, None, r.randrange(0, 46)])
    ret = [0] * fail + [1] if fail is not None else [0]
    cp = [1] if cpeep_fail else [0]
    return {
        "save": save, "version": version, "unused": r.choice([0, 1, rw(r)]), "fields": fields,
        "queues": {"RET": ret, "CPEEP": cp, "LOAD": [rf(r) if r.random() < 0.5 else rw(r) for _ in range(46)]},
    }


def edges():
    r = random.Random(97201)
    for save in (0, 1, 7):
        for version in (-1, 0, 1, 2, 3):
            yield make_case(r, save, version, None)
            yield make_case(r, save, version, None, cpeep_fail=True)
    for save in (0, 1):
        for version in (0, 1):
            for k in range(0, 44):
                yield make_case(r, save, version, k)
    c = make_case(r, 1, 0, None)
    f = dict(zip([o for o, _ in FIELDS], c["fields"]))
    for v in (0x437F0000, 0x43800000, 0x43808000, 0x3F7FFFFF, 0xBF800000, 0xC37F0000, 0x80000000, 0x7FC00000, 0x42FE0000):
        c = make_case(r, 1, 0, None)
        c["fields"] = [v if off in (0x19c, 0x1a4, 0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8) else x for (off, _), x in zip(FIELDS, c["fields"])]
        yield c


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CPerson::Serialise", ppc, host, edge=edges, rnd=rnd),
]
