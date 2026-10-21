"""Differential tests for unit w155, the park analyser's staff queries (src/game/park/analyser.cpp), against libtpw_unit_ParkStaff.dylib
(tests/unit_ParkStaff.cpp), main app ("SimTheme Park"):
  HiredSomeone 0x100e4750, GetStaffHappinessLevel 0x100e4ebc, GetDaysSinceLastObjectBuilt 0x100e7098, GetDaysSinceLastStaffHired 0x100e7188,
  NumberOfStaff 0x100e8324, NumberOfStaffTooTiredToWork 0x100e84d4, NumberOfStaffTypeWithPatrolArea 0x100e86a4, AverageQualityOfStaffType 0x100e88a0,
  AverageHappinessOfStaffType 0x100e89fc, AverageHappinessOfStaff 0x100e8b64, CurrentNumberOfStaffTypeForHire 0x100e8d4c,
  CurrentResearcherAbility 0x100e8dd8, MaxResearcherAbility 0x100e8ed8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside the unit are fakes
on both sides that record their arguments and answer from the case's queues: GAME_ASSERTMSG, the world accessor, GetUsedThingHead and GetNextUsedThing,
CStaff::TooTiredToWork and HasPatrolArea, CStaffDB::GetNumStaffOfType, CParkTime::GetFunnyTime and TbTimeStamp::operator- (a bullfrog import). The signed
64 bit divide helper 0x10237e1c runs as the original.

The world of a case: the analyser (0x21c48 bytes between two 0x40 guards, filled with 0xa5, the two stamps set), eight things (0x448 bytes: kind, dead,
the grade at +0x1e4, the happiness float at +0x1f4, the next researcher at +0x20c), the thing table's rows 0 to 15, two worlds (the accessor answers world 1,
the global gWorld is world 2; each has its own researcher head halfword) and the balance slots 160 to 419. What is compared: the answer (the float bit for
bit), the call log, the guards and the analyser's bytes (the two stamps as 64 bit values).

Fused operations: the happiness level's fadds, fdivs and fctiwz (HardwareRounding on the unit's functions). Not tested: a happiness sum above 2^24 in
GetStaffHappinessLevel (at most 0x2800 staff of 255 each is below it, so the float carry is exact), a divide by zero or INT_MIN / -1 (the divisors
are the counts), a researcher chain that loops, a negative researcher grade (the balance slot would leave the table).
"""
import ctypes as C
import random
import struct
import zlib

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkStaff"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
I64_MIN, I64_MAX = -(1 << 63), (1 << 63) - 1

# ---- the unit's own functions (address, size from the MANIFEST)
SIZES = {0x100E4750: 120, 0x100E4EBC: 532, 0x100E7098: 172, 0x100E7188: 172, 0x100E8324: 380, 0x100E84D4: 396, 0x100E86A4: 416,
         0x100E88A0: 264, 0x100E89FC: 272, 0x100E8B64: 424, 0x100E8D4C: 60, 0x100E8DD8: 192, 0x100E8ED8: 20}
OWN = set(SIZES)
FN = {"hired": 0x100E4750, "happiness": 0x100E4EBC, "days_obj": 0x100E7098, "days_staff": 0x100E7188, "nstaff": 0x100E8324,
      "tired": 0x100E84D4, "patrol": 0x100E86A4, "quality": 0x100E88A0, "hap_type": 0x100E89FC, "hap_staff": 0x100E8B64,
      "hire": 0x100E8D4C, "research": 0x100E8DD8, "maxres": 0x100E8ED8}

# ---- the callees faked on both sides
ASSERT, GETWORLD, HEAD, NEXT = 0x1000DDE8, 0x1014205C, 0x1013ADB0, 0x1013AE08
TIRED, PATROL, GETNUM, FUNNY = 0x10126CB4, 0x10127A78, 0x10129C6C, 0x101127D0

# ---- the Mac's addresses of the globals
TABLE, G_WORLD, MAIN = 0x1034CBFC, 0x1037EC0C, 0x102B4568
HOST_LAYOUT = [0x21C2C, 0x21C34, 0x21C48, 0, 0x2A0, 0x1DA742, 2, 3, 1]

ANALYSER_SIZE = 0x21C48
GUARD = 0x40
OBJ_AT, STAFF_AT = 0x21C2C, 0x21C34
THING_SIZE = 0x448
NT = 8                      # things
MAXID = 15                  # rows 0 to 15
TAIL0 = 0x1DA700            # a world's pointer is its tail block minus this
HEAD_AT = 0x1DA742 - TAIL0
BAL_FIRST, BAL_COUNT = 160, 260
L_AN, L_W1, L_W2, L_CLK1, L_CLK2, L_OBJ, L_STAFF, L_THING = 1, 2, 3, 4, 5, 6, 7, 10
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def be64(e, a):
    return int.from_bytes(e.read(a, 8), "big")


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only (Unicorn rounds fused operations twice and saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_stubs(S):
    """The Stubs registrations; S is the Ppc side (it holds the log and world 1)."""
    def log():
        return S.e.cl

    def assertion(emu):
        log().rec("GAME_ASSERTMSG", [s32(emu.r(3))], [emu.cstr(emu.r(4))])

    def getworld(emu):
        log().rec("GetWorld", [log().lab(emu.r(3))])
        emu.set_r(3, S.world1)

    def head(emu):
        L = log()
        L.rec("GetUsedThingHead", [L.lab(emu.r(4))])
        S.e.write(emu.r(3), struct.pack(">H", L.ret("head", 0) & 0xFFFF))

    def nxt(emu):
        L = log()
        L.rec("GetNextUsedThing", [L.lab(emu.r(4)), L.lab(emu.r(5))])
        S.e.write(emu.r(3), struct.pack(">H", L.ret("next", 0) & 0xFFFF))

    def tired(emu):
        L = log()
        L.rec("TooTiredToWork", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("tired") & MASK)

    def patrol(emu):
        L = log()
        L.rec("HasPatrolArea", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("patrol") & MASK)

    def getnum(emu):
        L = log()
        L.rec("GetNumStaffOfType", [L.lab(emu.r(3)), s32(emu.r(4))])
        emu.set_r(3, L.ret("numstaff") & MASK)

    def funny(emu):
        L = log()
        sp = emu.r(1)
        outs = [emu.r(i) for i in range(5, 11)] + [emu.u32(sp + 0x38)]
        h = emu.u32(sp + 0x3C)
        L.rec("GetFunnyTime", [L.lab(emu.r(4))] + [L.lab(o) for o in outs])
        L.rec("GetFunnyTime.8", [L.lab(h)])
        S.e.write(emu.r(3), (L.ret("ft") & M64).to_bytes(8, "big"))

    def minus(emu):
        L = log()
        a, b = emu.r(4), emu.r(5)
        L.rec("Stamp-", [L.lab(a), s64(be64(S.e, a)), L.lab(b), s64(be64(S.e, b))])
        S.e.write(emu.r(3), (L.ret("minus") & M64).to_bytes(8, "big"))

    return ((ASSERT, assertion), (GETWORLD, getworld), (HEAD, head), (NEXT, nxt), (TIRED, tired), (PATROL, patrol), (GETNUM, getnum),
            (FUNNY, funny)), (("__mi__11TbTimeStampCFRC11TbTimeStamp", minus),)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory (self.fmt is the byte order)."""

    def put(self, a, fmt, value):
        self.write(a, struct.pack(self.fmt[0] + fmt, value))

    def get(self, a, fmt):
        return struct.unpack(self.fmt[0] + fmt, self.read(a, struct.calcsize(fmt)))[0]

    def block(self, size, fill=0xA5):
        a = self.alloc(size)
        self.write(a, bytes([fill]) * size)
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world1 = 0
        self.L = e.cl = CL.PpcLog({}, queues_of(case))
        internals, imports = make_stubs(self)
        stubs = CL.Stubs()
        stubs.real.update(OWN)
        for addr, fn in internals:
            stubs.internal(addr, fn)
        for name, fn in imports:
            stubs.imp(name, fn)
        self.saved = stubs.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, label, address):
        self.L.labels[address] = label

    def start(self):
        pass

    def log(self):
        return self.L.log

    def set_row(self, row, thing):
        self.put(TABLE + 0x14 * row, "I", thing)

    def set_worlds(self, w1, w2):
        self.world1 = w1
        self.put(G_WORLD, "I", w2)

    def set_balance(self, values):
        self.write(MAIN + 8 + 4 * BAL_FIRST, b"".join(struct.pack(">I", v & MASK) for v in values))

    def call(self, op, an, args):
        with ROUNDING(self.e):
            r3, _, f1 = T.call_ppc(self.e, FN[op], [an] + [a & MASK for a in args])
        if op == "happiness":
            return canon_f(fb(f1))
        return 0 if op == "hired" else s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.labels = {}
        self.queues = queues_of(case)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pf_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, label, address):
        self.labels[label] = address

    def start(self):
        CL.host_begin(self.lib, self.labels, self.queues)

    def log(self):
        return CL.host_end(self.lib)

    def set_row(self, row, thing):
        self.lib.tpw_pf_row(row, thing)

    def set_worlds(self, w1, w2):
        self.lib.tpw_pf_worlds(w1, w2)

    def set_balance(self, values):
        self.lib.tpw_pf_balance(BAL_FIRST, len(values), (C.c_int32 * len(values))(*[s32(v) for v in values]))

    def call(self, op, an, args):
        f = getattr(self.lib, "tpw_pf_" + op)
        r = f(an, *[s32(a) if op != "happiness" else a & MASK for a in args])
        if op == "hired":
            return 0
        if op == "happiness":
            return canon_f(r & MASK)
        return r


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_pf_alloc.restype, lib.tpw_pf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pf_worlds.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_pf_row.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_pf_balance.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    for op in FN:
        f = getattr(lib, "tpw_pf_" + op)
        n = 1 + {"happiness": 1, "tired": 0, "patrol": 1, "quality": 1, "hap_type": 1, "hire": 1}.get(op, 0)
        f.argtypes = [C.c_uint64] + ([C.c_uint32] if op == "happiness" else [C.c_int32] * (n - 1))
        f.restype = None if op == "hired" else (C.c_uint32 if op == "happiness" else C.c_int32)
    out = (C.c_int64 * 32)()
    n = lib.tpw_pf_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def queues_of(case):
    return {k: [s64(v) for v in vs] for k, vs in case["q"].items()}


def default_balance(slot):
    return s32((slot * 7919 + 13) * 31)


def build(s, case):
    """Writes the case's blocks through Side.put: the same code for both memories (offsets are the Mac's)."""
    s.an_base = s.block(ANALYSER_SIZE + 2 * GUARD)
    an = s.an = s.an_base + GUARD
    s.put(an + OBJ_AT, "Q", case["objst"] & M64)
    s.put(an + STAFF_AT, "Q", case["stst"] & M64)
    things = []
    for kind, dead, grade, hap, nxt in case["things"]:
        t = s.block(THING_SIZE)
        s.write(t + 2, bytes([kind & 0xFF, dead & 0xFF]))
        s.put(t + 0x1E4, "I", grade & MASK)
        s.put(t + 0x1F4, "I", hap & MASK)
        s.put(t + 0x20C, "H", nxt & 0xFFFF)
        things.append(t)
    tails = [s.block(0x80) for _ in range(2)]
    w1, w2 = tails[0] - TAIL0, tails[1] - TAIL0
    s.put(tails[0] + HEAD_AT, "H", case["rhead"] & 0xFFFF)
    s.put(tails[1] + HEAD_AT, "H", case["w2head"] & 0xFFFF)
    s.set_worlds(w1, w2)
    for row in range(MAXID + 1):
        slot = case["rows"].get(row, 0)
        s.set_row(row, things[slot - 1] if slot else 0)
    s.set_balance([case["mb"].get(BAL_FIRST + i, default_balance(BAL_FIRST + i)) for i in range(BAL_COUNT)])
    s.label(L_AN, an)
    s.label(L_W1, w1)
    s.label(L_W2, w2)
    s.label(L_CLK1, w1 + 0x2A0)
    s.label(L_CLK2, w2 + 0x2A0)
    s.label(L_OBJ, an + OBJ_AT)
    s.label(L_STAFF, an + STAFF_AT)
    for i, t in enumerate(things):
        s.label(L_THING + i, t)
    s.start()


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    an = s.an
    whole = s.read(s.an_base, ANALYSER_SIZE + 2 * GUARD)
    cut = GUARD + OBJ_AT
    rest = whole[:cut] + whole[cut + 16:]
    return {"answer": answer, "log": s.log(), "bytes": [len(rest), zlib.crc32(rest)],
            "obj": s.get(an + OBJ_AT, "Q"), "staff": s.get(an + STAFF_AT, "Q")}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], s.an, case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


class Base(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


# ---------------------------------------------------------------------------------------------------------------- case generators

KINDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x13, 0x81, 0xFF]
STAFF_KINDS = [4, 5, 6, 7, 8]
MODELS = [0, 3, 4, 5, 6, 7, 8, 9, -1, 0x104, 0x105, INT_MIN, INT_MAX]
HAPPY = [0.0, 0.5, 1.0, 127.9, 254.9, 255.0, 255.9, 256.0, 257.5, 300.0, -1.0, -0.5, 1e10, -1e10, float("nan"), 1e30, 12345.678]
GRADES = [INT_MIN, INT_MIN + 1, -1, 0, 1, 2, 3, 4, 5, 20, 100, INT_MAX, 0x1999999A, 0x00A3D70B]
RGRADES = [0, 1, 2, 3, 4, 5, 6, 10, 17, 25, 33, 40]
BALVALS = [0, 1, -1, 2, 0x10000, 0x7FFF, 0x8000, INT_MIN, INT_MAX, 100, -100, 0x12345678]
ANSWERS = [0, 1, 2, -1, 0x100, 0x10000, INT_MIN, INT_MAX]
SPANS = [0, 1, -1, 863999999999, 864000000000, 864000000001, -863999999999, -864000000000, -864000000001, I64_MIN, I64_MAX, 1 << 32,
         (1 << 32) - 1, -(1 << 32), 864000000000 * 5, -864000000000 * 5, 864000000000 * 0x7FFFFFFF, 864000000000 * 0x80000000,
         864000000000 * 0x100000001, 864000000000 * 0xFFFFFFFF + 5]
STAMPS = [0, 1, M64, I64_MAX, 1 << 63, 0x123456789ABCDEF0, 0xFEDCBA9876543210, 864000000000, 1 << 32]


def hap_bits(r):
    return fb(r.choice(HAPPY)) if r.random() < 0.7 else fb(r.uniform(-20, 400))


def rand_thing(r, kinds=None):
    kind = r.choice(kinds if kinds and r.random() < 0.75 else KINDS)
    return (kind, r.choice([0, 0, 0, 1, 2, 0xFF]), r.choice(GRADES), hap_bits(r), r.randrange(0, 20))


def blank(r, op, args=(), kinds=None):
    return {"op": op, "args": list(args), "things": [rand_thing(r, kinds) for _ in range(NT)], "rows": {}, "rhead": 0,
            "w2head": r.randrange(0, 16), "mb": {}, "objst": r.choice(STAMPS), "stst": r.choice(STAMPS), "q": {}}


def with_walk(r, c, n=None):
    """A used-list walk of n things: ids, their rows, the head/next queues and the callee answers."""
    n = r.choice([0, 1, 1, 2, 3, 4, 6, 7, 8]) if n is None else n
    ids = r.sample(range(1, MAXID + 1), n)
    slots = r.sample(range(1, NT + 1), n)
    c["rows"] = dict(zip(ids, slots))
    for extra in r.sample(range(1, MAXID + 1), 3):
        c["rows"].setdefault(extra, r.randint(0, NT))
    if r.random() < 0.1:
        c["rows"][0] = r.randint(1, NT)
    c["q"].update({"head": [ids[0] if ids else 0], "next": ids[1:] + [0],
                   "tired": [r.choice(ANSWERS) for _ in range(NT)], "patrol": [r.choice(ANSWERS) for _ in range(NT)]})
    return c


def walk_case(op, kinds=None, arg=None):
    def make(r, model=None):
        args = [] if op in ("nstaff", "tired", "hap_staff") else [r.choice(MODELS) if model is None else model]
        if op == "happiness":
            args = [r.getrandbits(32)]
        return with_walk(r, blank(r, op, args, kinds))
    return make


def walk_edges(op, kinds):
    """Single things of every kind (alive and dead) for every model, a staff thing of every kind among others, the assertion and empty walks."""
    def edges():
        n = 0
        for model in (MODELS if op not in ("nstaff", "tired", "hap_staff", "happiness") else [None]):
            for kind in KINDS:
                for dead in (0, 1):
                    n += 1
                    r = random.Random(n * 977 + 5)
                    args = [] if model is None else [model]
                    if op == "happiness":
                        args = [0]
                    c = blank(r, op, args, kinds)
                    c["things"][0] = (kind, dead, r.choice(GRADES), fb(r.choice(HAPPY)), 0)
                    c["things"][1] = (r.choice(STAFF_KINDS), 0, 3, fb(r.choice(HAPPY)), 0)
                    with_walk(r, c, 2)
                    yield c
        r = random.Random(41)
        c = with_walk(r, blank(r, op, [] if op in ("nstaff", "tired", "hap_staff") else [4] if op != "happiness" else [0], kinds), 0)
        yield c
        c = with_walk(r, blank(r, op, [] if op in ("nstaff", "tired", "hap_staff") else [4] if op != "happiness" else [0], kinds), 3)
        c["rows"][0] = 2
        yield c
    return edges


def hap_edges(op, kinds):
    """The happiness values (each alone on a matching staff thing) and a few sums."""
    def edges():
        for i, h in enumerate(HAPPY + [3.9999, 255.999, -0.0, 2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 4294967296.0 + 7]):
            r = random.Random(100 + i)
            model = 5
            c = blank(r, op, [] if op in ("hap_staff",) else [0] if op == "happiness" else [model], kinds)
            c["things"] = [(model, 0, 0, fb(h), 0)] + c["things"][1:]
            for j in range(1, NT):
                c["things"][j] = (model, 0, 0, fb(r.choice(HAPPY)), 0)
            with_walk(r, c, r.choice([1, 2, 5, 8]))
            c["rows"] = {k + 1: k + 1 for k in range(NT)}
            c["rows"][1] = 1
            c["q"]["head"] = [1]
            c["q"]["next"] = list(range(2, r.choice([2, 3, 5, 9]))) + [0]
            yield c
    return edges


def quality_edges(op):
    def edges():
        for i, g in enumerate(GRADES):
            for model in (5, 4, 8, 0, 9):
                r = random.Random(300 + i * 11 + model)
                c = blank(r, op, [model], None)
                c["things"] = [(model, 0, g, fb(100.5), 0), (model, 1, r.choice(GRADES), fb(1.0), 0), (3, 0, 77, fb(9.0), 0)] + c["things"][3:]
                with_walk(r, c, 3)
                c["rows"] = {1: 1, 2: 2, 3: 3}
                c["q"]["head"], c["q"]["next"] = [1], [2, 3, 0]
                yield c
        for n in (1, 2, 3, 5, 8):
            r = random.Random(900 + n)
            c = blank(r, op, [5], None)
            c["things"] = [(5, 0, g, fb(255.0), 0) for g in r.sample(GRADES, 8)]
            c["rows"] = {k + 1: k + 1 for k in range(NT)}
            c["q"]["head"], c["q"]["next"] = [1], list(range(2, n + 1)) + [0]
            yield c
    return edges


def time_case(op):
    def make(r):
        c = blank(r, op)
        c["q"] = {"ft": [r.choice([0, 1, M64, I64_MAX, 1 << 63, r.getrandbits(64), r.getrandbits(40)])],
                  "minus": [r.choice(SPANS) if r.random() < 0.7 else s64(r.getrandbits(r.choice([20, 40, 50, 64])))]}
        return c
    return make


def time_edges(op):
    def edges():
        i = 0
        for span in SPANS:
            for st in STAMPS[:4]:
                i += 1
                r = random.Random(500 + i)
                c = blank(r, op)
                c["objst"], c["stst"] = st, STAMPS[-1 - (i % 5)]
                c["q"] = {"ft": [r.choice([0, 1, I64_MAX, 1 << 40])], "minus": [span]}
                yield c
    return edges


def hired_case(r):
    c = blank(r, "hired")
    c["q"] = {"ft": [r.choice([0, 1, M64, I64_MAX, 1 << 63, r.getrandbits(64), r.getrandbits(40)])]}
    return c


def hired_edges():
    for i, t in enumerate(STAMPS + [r_ for r_ in SPANS]):
        r = random.Random(700 + i)
        c = blank(r, "hired")
        c["q"] = {"ft": [t]}
        yield c


def hire_case(r):
    c = blank(r, "hire", [r.choice([0, 1, 2, 3, 4, -1, INT_MAX, INT_MIN, 5, 0x100])])
    c["q"] = {"numstaff": [r.choice(ANSWERS + [r.randrange(0, 40)])]}
    return c


def hire_edges():
    for i, t in enumerate([0, 1, 2, 3, 4, -1, INT_MAX, INT_MIN, 5, 0x100, 0x10000]):
        for a in (0, 1, 7, -1, INT_MIN, INT_MAX):
            r = random.Random(800 + i)
            c = blank(r, "hire", [t])
            c["q"] = {"numstaff": [a]}
            yield c


def research_chain(r, c, n):
    """A researcher chain of n links: the head in world 1, each thing's +0x20c the next id, the last 0."""
    ids = r.sample(range(1, MAXID + 1), n)
    slots = r.sample(range(1, NT + 1), n)
    c["rows"] = dict(zip(ids, slots))
    for extra in r.sample(range(1, MAXID + 1), 3):
        c["rows"].setdefault(extra, r.randint(0, NT))
    if r.random() < 0.1:
        c["rows"][0] = r.randint(1, NT)
    c["rhead"] = ids[0] if ids else 0
    things = list(c["things"])
    for k, slot in enumerate(slots):
        kind, dead, _, hap, _ = things[slot - 1]
        things[slot - 1] = (kind, dead, r.choice(RGRADES), hap, ids[k + 1] if k + 1 < n else 0)
    c["things"] = things
    for slot in slots:
        grade = things[slot - 1][2]
        for d in range(1):
            c["mb"][261 + 3 * grade] = r.choice(BALVALS) if r.random() < 0.8 else s32(r.getrandbits(32))
    return c


def research_case(r):
    return research_chain(r, blank(r, "research"), r.choice([0, 1, 1, 2, 3, 5, 6]))


def research_edges():
    for n in (0, 1, 2, 5, 6):
        for v in range(4):
            r = random.Random(1100 + n * 10 + v)
            c = research_chain(r, blank(r, "research"), n)
            if v == 1:
                c["rows"][0] = 1
            if v == 2:
                for key in list(c["mb"]):
                    c["mb"][key] = BALVALS[(key + n) % len(BALVALS)]
            if v == 3:
                for key in list(c["mb"]):
                    c["mb"][key] = INT_MAX
            yield c
    for g in RGRADES:
        r = random.Random(1200 + g)
        c = research_chain(r, blank(r, "research"), 1)
        slot = c["rows"][c["rhead"]]
        kind, dead, _, hap, _ = c["things"][slot - 1]
        c["things"][slot - 1] = (kind, dead, g, hap, 0)
        c["mb"] = {261 + 3 * g: 12345 + g}
        yield c


def maxres_case(r):
    c = blank(r, "maxres")
    for slot in (172, 273):
        c["mb"][slot] = r.choice(BALVALS) if r.random() < 0.8 else s32(r.getrandbits(32))
    return c


def maxres_edges():
    for i, a in enumerate(BALVALS):
        for b in BALVALS:
            c = blank(random.Random(1300 + i), "maxres")
            c["mb"] = {172: a, 273: b}
            yield c


def make(name, ppc_fn, host_fn, edges, rnd):
    return Base(name, ppc_fn, host_fn, edge=edges, rnd=rnd)


ALL = [None]
TESTS = [
    make("CParkAnalyser::HiredSomeone", ppc, host, hired_edges, hired_case),
    make("CParkAnalyser::GetStaffHappinessLevel", ppc, host, lambda: list(hap_edges("happiness", STAFF_KINDS)()) + list(walk_edges("happiness", STAFF_KINDS)()),
         walk_case("happiness", STAFF_KINDS)),
    make("CParkAnalyser::GetDaysSinceLastObjectBuilt", ppc, host, time_edges("days_obj"), time_case("days_obj")),
    make("CParkAnalyser::GetDaysSinceLastStaffHired", ppc, host, time_edges("days_staff"), time_case("days_staff")),
    make("CParkAnalyser::NumberOfStaff", ppc, host, walk_edges("nstaff", STAFF_KINDS), walk_case("nstaff", STAFF_KINDS)),
    make("CParkAnalyser::NumberOfStaffTooTiredToWork", ppc, host, walk_edges("tired", STAFF_KINDS), walk_case("tired", STAFF_KINDS)),
    make("CParkAnalyser::NumberOfStaffTypeWithPatrolArea", ppc, host, walk_edges("patrol", STAFF_KINDS), walk_case("patrol", STAFF_KINDS)),
    make("CParkAnalyser::AverageQualityOfStaffType", ppc, host, lambda: list(quality_edges("quality")()) + list(walk_edges("quality", None)()),
         walk_case("quality", [4, 5, 6, 7, 8, 3])),
    make("CParkAnalyser::AverageHappinessOfStaffType", ppc, host, lambda: list(hap_edges("hap_type", None)()) + list(walk_edges("hap_type", None)()),
         walk_case("hap_type", [4, 5, 6, 7, 8, 3])),
    make("CParkAnalyser::AverageHappinessOfStaff", ppc, host, lambda: list(hap_edges("hap_staff", STAFF_KINDS)()) + list(walk_edges("hap_staff", STAFF_KINDS)()),
         walk_case("hap_staff", STAFF_KINDS)),
    make("CParkAnalyser::CurrentNumberOfStaffTypeForHire", ppc, host, hire_edges, hire_case),
    make("CParkAnalyser::CurrentResearcherAbility", ppc, host, research_edges, research_case),
    make("CParkAnalyser::MaxResearcherAbility", ppc, host, maxres_edges, maxres_case),
]
