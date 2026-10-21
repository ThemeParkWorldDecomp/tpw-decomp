"""Unit w172, the ride-specific commands of the ride scripts, against libtpw_unit_RideCommands.dylib (tests/unit_RideCommands.cpp), main app
("SimTheme Park"), src/game/rides/rsse.cpp:
  ExecuteTourSpecificCommand 0x100d6fec, ExecuteBumperSpecificCommand 0x100d78ec, ExecuteCoasterSpecificCommand 0x100d8104,
  __sinit_rsse_cpp 0x100d85f0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside the unit (the
FLY_ tour ride, BMP_ bumper ride, Coaster and LLCC_ calls, GetCoasterInstanceID, FindDataNodeIndexByID and the assert) are fakes on both sides that
record their arguments and answer from the case's queues. ReadScript and ExtractData are inlined by the original; the host library runs the unit's
own translation of them (src/game/rides/RideEffects.cpp, src/game/rides/rsse.cpp, tested by unit w150).

The world of a case: a script record (random words, the fields the dispatchers read set), its program (the command, its operand, random words
around them; the program counter and the program length are random so the reads can run off the end), eight variables, the thing table (rows 0 to 7,
the row the script names always populated, row 0 present or absent), four ride sets (mesh angle word, eight embedded records with their 0x40-byte
blocks). What is compared: the script record's words and halfwords (not its pointers), the variables, the program, and the call log.

Fused operations: the floats of the tour ride creation (fctiwz of three block floats) run under HardwareRounding over the unit's own functions.
Not tested: a NULL program or variable table (ReadScript and ExtractData read through them), a variable index above 7 (the original indexes the
table unchecked), a NULL thing for the script's own row (the bumper reads the thing's fields), a ride slot without a set, a node index above 7.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_RideCommands"
MASK = 0xFFFFFFFF

FN = dict(Tour=0x100D6FEC, Bumper=0x100D78EC, Coaster=0x100D8104)
SZ = dict(Tour=2240, Bumper=2008, Coaster=1192)
SINIT = 0x100D85F0
SIZES = {FN[n]: SZ[n] for n in FN}
SIZES[SINIT] = 28
OWN = set(SIZES)
BOUND = dict(Tour=0x12, Bumper=0x11, Coaster=8)

ADDR = dict(RIDESETS=0x102C79A4, THINGS=0x1034CBFC, W00=0x102F7C00, W04=0x102F7C04)
HOST_LAYOUT = [0xF4, 0x18, 0x1C, 0x48, 0xE6, 0xAC, 0xAE, 0xC0, 0xE0, 0x9C, 0x24, 0x28]
KINDS = ("Tour", "Bumper", "Coaster")
NROWS, NSLOTS, NRECS = 8, 4, 8
CODE_WORDS, VAR_WORDS = 24, 8
L_SCRIPT = 1


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def sext16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

# (name, address, argument spec, answers from the queue "name")
CALLEES = [
    ("GAME_ASSERTMSG", 0x1000DDE8, "is", False),
    ("FindDataNodeIndexByID", 0x101FFDC8, "Luu", True),
    ("FLY_CreateBase", 0x100753E8, "uiiiiii", True),
    ("FLY_RemoveBase", 0x100781F4, "i", True),
    ("FLY_AddPeep", 0x100787C8, "ii", True),
    ("FLY_ExtractPeep", 0x1007848C, "i", True),
    ("FLY_GetBrokenStat", 0x10078FB0, "i", True),
    ("FLY_GetCarInBase", 0x10078F54, "i", True),
    ("FLY_LaunchCar", 0x10079024, "i", True),
    ("FLY_RideRunning", 0x100791C0, "i", True),
    ("FLY_SetCarSize", 0x10079114, "ii", True),
    ("FLY_SetMotionScale", 0x10079168, "ii", True),
    ("FLY_SetNumberOfCars", 0x10078EE4, "ii", True),
    ("FLY_SetOpenClosed", 0x10078A58, "ii", True),
    ("FLY_SetRepairState", 0x10078B40, "ii", True),
    ("FLY_SetRunTime", 0x10078958, "ii", True),
    ("FLY_SetRunway", 0x100790C0, "ii", True),
    ("BMP_AddJoiningListToBump", 0x1002CC70, "i", True),
    ("BMP_AddPeepToRideJoiningList", 0x1002E070, "iu", True),
    ("BMP_BeginRide", 0x10026924, "i", True),
    ("BMP_BreakRide", 0x100264C0, "i", True),
    ("BMP_CloseRide", 0x10026118, "i", True),
    ("BMP_FixRide", 0x1002672C, "i", True),
    ("BMP_GetCarsOnRide", 0x10026E74, "i", True),
    ("BMP_HaltRide", 0x100262CC, "i", True),
    ("BMP_LAUNCH", 0x1002CFF4, "ii", True),
    ("BMP_OpenRide", 0x10025EE4, "i", True),
    ("BMP_PollForPeeps", 0x1002E198, "i", True),
    ("BMP_REMOVEBUMP", 0x1002E614, "ii", True),
    ("BMP_RideWorn", 0x100263F4, "i", True),
    ("BMP_SetRideDuration", 0x10026B14, "ii", True),
    ("CoasterAddPeep", 0x10049288, "uu", True),
    ("CoasterGetPeep", 0x100494C0, "u", True),
    ("CoasterGetQueueSpace", 0x1004944C, "u", True),
    ("CoasterSetCapacity", 0x10049758, "ui", True),
    ("CoasterSetOpenClosedState", 0x100496C0, "ui", True),
    ("CoasterSetRepairState", 0x10049588, "uu", True),
    ("CoasterSetSpeed", 0x10049630, "uu", True),
    ("GetCoasterInstanceID", 0x10049120, "i", True),
    ("LLCC_SetTrackAnim", 0x1008C7B0, "Lb", False),
    ("LLCC_SetTrackAnimReverse", 0x1008CAD8, "Lb", False),
]
for _n, _a, _spec, _ret in CALLEES:
    STUBS.internal(_a, CL.recorder(_n, _spec, "i" if _ret else None))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

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

    def label(self, a, n):
        self.L.labels[a] = n

    def p(self, a):
        return a

    def set_thing(self, row, spec):
        if spec is None:
            self.words(ADDR["THINGS"] + 0x14 * row, [0])
            return
        obj = self.alloc(0x40)
        self.words(obj + 0x24, [spec[0], spec[1]])
        self.words(ADDR["THINGS"] + 0x14 * row, [obj])
        self.label(obj, 40 + row)

    def make_ride(self, slot, spec):
        recs = self.alloc(0x14 * NRECS)
        for i, (has, blk) in enumerate(spec["recs"]):
            b = 0
            if has:
                b = self.alloc(0x40)
                self.words(b, blk)
            self.words(recs + 0x14 * i, [0, b, 0, 0, 0])
        emb = self.alloc(0x10)
        self.words(emb, [0, recs])
        mesh = self.alloc(0x40)
        self.words(mesh + 0x10, [spec["f10"]])
        self.words(mesh + 0x28, [emb])
        st = self.alloc(0x10)
        self.words(st + 8, [mesh])
        self.words(ADDR["RIDESETS"] + 4 * slot, [st])
        self.label(st, 10 + slot)

    def call(self, kind, script):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN[kind], [script])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_rcm_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rcm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, a, n):
        self.lib.tpw_cl_label(n, a)

    def p(self, a):
        return a - self.base if a else 0

    def set_thing(self, row, spec):
        if spec is None:
            self.lib.tpw_rcm_thing(row, 0, 0, 0)
            return
        self.label(self.lib.tpw_rcm_thing(row, 1, s32(spec[0]), s32(spec[1])), 40 + row)

    def make_ride(self, slot, spec):
        d = []
        for has, blk in spec["recs"]:
            d.append(1 if has else 0)
            d.extend(blk)
        arr = (C.c_int64 * len(d))(*d)
        self.label(self.lib.tpw_rcm_ride(slot, s32(spec["f10"]), arr), 10 + slot)

    def call(self, kind, script):
        getattr(self.lib, "tpw_rcm_" + kind)(script)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rcm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rcm_thing.restype, lib.tpw_rcm_thing.argtypes = C.c_uint64, [C.c_uint32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_rcm_ride.restype, lib.tpw_rcm_ride.argtypes = C.c_uint64, [C.c_int32, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    for n in KINDS:
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

PTR_WORDS = (6, 7, 8, 44)     # code, vars, stack, objects
HALF_AT = (0xAC, 0xAE, 0xC0, 0xC2, 0xE4, 0xE6)
HALF_WORDS = (43, 48, 57)


def build(s, c):
    for row in range(NROWS):
        s.set_thing(row, c["things"][row])
    for slot in range(NSLOTS):
        s.make_ride(slot, c["rides"][slot])
    sc = c["sc"]
    script = s.alloc(0xF4)
    s.label(script, L_SCRIPT)
    words = list(c["fill"])
    code = s.alloc(4 * CODE_WORDS)
    s.words(code, c["code"])
    vrs = s.alloc(4 * VAR_WORDS)
    s.words(vrs, c["vars"])
    words[6], words[7], words[8], words[44] = s.p(code), s.p(vrs), 0, 0
    for name, off in (("handle", 8), ("state", 0x3C), ("answer", 0x48), ("pw", 0x50), ("fly", 0x9C), ("ride", 0xC8), ("coaster", 0xE0)):
        words[off // 4] = sc[name]
    s.words(script, words)
    for name, off in (("thing", 0xAC), ("f_ae", 0xAE), ("speed", 0xC0), ("f_c2", 0xC2), ("e4", 0xE4), ("e6", 0xE6)):
        s.half(script + off, sc[name])
    return {"script": script, "code": code, "vars": vrs}


def observe(s, c, env):
    w = s.rd_words(env["script"], 0x3D)
    sc = [v for i, v in enumerate(w) if i not in PTR_WORDS and i not in HALF_WORDS]
    sc += [s.rd_half(env["script"] + o) for o in HALF_AT]
    return {"log": s.log(), "sc": sc, "vars": s.rd_words(env["vars"], VAR_WORDS), "code": s.rd_words(env["code"], CODE_WORDS)}


def run(s, c):
    try:
        env = build(s, c)
        s.call(c["kind"], env["script"])
        return observe(s, c, env)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

ANS = [0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, 300, 0x10000]
VALS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x7FFFFFFF, -0x80000000, 1000, 0x1234567]
FLOATS = [0.0, 0.5, -0.5, 0.999, -0.999, 1.5, -1.5, 3.75, -3.75, 123.456, -987.65, 7158278.5, -7158278.5, 1e10, -1e10, 3e9, 1e-8,
          float("inf"), float("-inf"), float("nan"), 6e6, 14316557.0, 2.5e8]


def vword(i):
    return 0x40000000 + i


def lit(r):
    h = r.choice([0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x1234, r.getrandbits(16)])
    hi = r.choice([0, 0, 0xFFFF, 0x4100, r.getrandbits(16)])
    if (hi >> 8) == 0x40:
        hi = 0
    return (hi << 16) | h


def operand(r):
    return vword(r.randrange(VAR_WORDS)) if r.random() < 0.4 else lit(r)


def anyword(r):
    return vword(r.randrange(VAR_WORDS)) if r.random() < 0.3 else lit(r)


def value_of(word, vrs):
    if (word & 0xFF000000) == 0x40000000:
        return vrs[word ^ 0x40000000]
    return sext16(word)


def rf_bits(r):
    k = r.random()
    if k < 0.7:
        return fb(r.choice(FLOATS))
    if k < 0.9:
        return fb(r.uniform(-3e6, 3e6))
    return r.choice([0x7FC00000, 0xFFC00000, 0x7F800001, 0x80000000, 0x00000001, r.getrandbits(32)])


def mk_ride(r):
    recs = []
    for _ in range(NRECS):
        blk = [r.getrandbits(32) for _ in range(16)]
        for i in (12, 13, 14):
            blk[i] = rf_bits(r)
        recs.append((r.random() < 0.8, blk))
    f10 = r.choice([0, 1, 90, 180, 360, -90, 0x7FFFF, 0x80000, 0xFFFFF, 0x100000, -1, r.getrandbits(32), r.randrange(0, 720)])
    return {"f10": f10 & MASK, "recs": recs}


def mk_queues(r):
    q = {}
    for name, _a, _s, has in CALLEES:
        if has:
            q[name] = [r.choice(ANS) if r.random() < 0.7 else r.getrandbits(32) for _ in range(r.randrange(1, 3))]
    q["FindDataNodeIndexByID"] = [r.choice([-1, -1, 0, 1, 2, 3, 4, 5, 6, 7]) for _ in range(2)]
    q["GetCoasterInstanceID"] = [r.choice([0, 0, 1, 77, r.getrandbits(32)])]
    return q


def base(r, kind):
    thing = r.choice([0, 1, 2, 3, 4, 5, 6, 7]) if kind == "Bumper" else r.randrange(0, 8)
    things = []
    for row in range(NROWS):
        if row == 0:
            present = thing == 0 or r.random() < 0.3
        else:
            present = row == thing or r.random() < 0.7
        things.append((r.getrandbits(32), r.getrandbits(32)) if present else None)
    sc = {"handle": r.choice([0, 1, 5, r.getrandbits(32)]), "state": 0, "answer": r.getrandbits(32), "pw": 0,
          "fly": r.choice([0, 1, 9, r.getrandbits(32)]), "ride": r.randrange(NSLOTS), "coaster": r.choice([0, 3, r.getrandbits(32)]),
          "thing": thing, "f_ae": r.getrandbits(16), "speed": r.choice([0, 1, 100, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]),
          "f_c2": r.getrandbits(16), "e4": r.getrandbits(16), "e6": r.choice([0, 1, 0xFFFF, r.getrandbits(16)])}
    return {"kind": kind, "queues": mk_queues(r), "rides": [mk_ride(r) for _ in range(NSLOTS)], "things": things,
            "fill": [r.getrandbits(32) for _ in range(0x3D)], "sc": sc,
            "vars": [r.choice(VALS) if r.random() < 0.7 else r.getrandbits(32) for _ in range(VAR_WORDS)]}


def finish(r, c, prog, cut, pre):
    """Places the program: `pre` random words first (the program counter starts after them), then the program; the length cuts `cut` words."""
    words = [anyword(r) for _ in range(pre)] + prog
    words += [anyword(r) for _ in range(CODE_WORDS - len(words))]
    c["code"] = words
    c["sc"]["state"] = pre
    c["sc"]["pw"] = max(pre + len(prog) - cut, 0)
    return c


def commands(kind, r):
    b = BOUND[kind]
    return list(range(0, b + 3)) + [0xFFFFFFFF, 0x80000000, r.getrandbits(32)]


def mk_case(r, kind, cmd, word, cut=0, pre=0):
    c = base(r, kind)
    prog = [cmd] if word is None else [cmd, word]
    if kind == "Bumper" and cmd == 17 and word is not None:
        a = value_of(word, c["vars"])
        c["sc"]["e6"] = a & 0xFFFF if r.random() < 0.5 else c["sc"]["e6"]
    return finish(r, c, prog, cut, pre)


OPERANDS = [vword(0), vword(7), vword(3), 0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x00010000, 0x1234FFFF, 0xFFFF0001, 0x41000003, 0x3FFFFFFF]


def edges(kind):
    def gen():
        r = random.Random(sum(map(ord, kind)) * 7919)
        out = []
        for cmd in commands(kind, r):
            for word in OPERANDS:
                out.append(mk_case(r, kind, cmd, word))
            out.append(mk_case(r, kind, cmd, None))                     # the program ends after the command
            out.append(mk_case(r, kind, cmd, None, cut=1))              # and before it
            out.append(mk_case(r, kind, cmd, lit(r), cut=0, pre=3))     # a program counter past the start
            c = mk_case(r, kind, cmd, vword(1))
            c["sc"]["state"] = -5                                       # a dead script
            out.append(c)
            c = mk_case(r, kind, cmd, vword(1))
            c["sc"]["state"] = c["sc"]["pw"]                            # at the end
            out.append(c)
        return out
    return gen


def randoms(kind):
    def gen(r):
        b = BOUND[kind]
        cmd = r.choice(list(range(1, b + 1)) * 3 + [0, b + 1, r.getrandbits(32)])
        word = operand(r)
        cut = r.choice([0] * 8 + [1, 2])
        pre = r.choice([0, 0, 0, 1, 5, 17])
        return mk_case(r, kind, cmd, word, cut, pre)
    return gen


# ---------------------------------------------------------------------------------------------------------------- __sinit_rsse_cpp

G_BEFORE, G_AFTER = ADDR["W00"] - 4, ADDR["W04"] + 4


def sinit_ppc(e, case):
    ws = (G_BEFORE, ADDR["W00"], ADDR["W04"], G_AFTER)
    for a in ws:
        e.write(a, b"\x5a\xa5\x5a\xa5")
    T.call_ppc(e, SINIT, [])
    return {"words": [e.u32(a) for a in ws]}


def sinit_host(lib, case):
    bind(lib)
    junk = 0x5AA55AA5
    return {"words": [junk, C.c_uint32.from_address(lib.tpw_rcm_addr(b"W00")).value, C.c_uint32.from_address(lib.tpw_rcm_addr(b"W04")).value, junk]}


def sinit_cases():
    return [{"queues": {}} for _ in range(2)]


TESTS = [
    FnTest("ExecuteTourSpecificCommand", ppc, host, edge=edges("Tour"), rnd=randoms("Tour")),
    FnTest("ExecuteBumperSpecificCommand", ppc, host, edge=edges("Bumper"), rnd=randoms("Bumper")),
    FnTest("ExecuteCoasterSpecificCommand", ppc, host, edge=edges("Coaster"), rnd=randoms("Coaster")),
    FnTest("__sinit_rsse_cpp", sinit_ppc, sinit_host, edge=sinit_cases, rnd=lambda r: {"queues": {}}),
]
