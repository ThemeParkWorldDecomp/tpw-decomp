"""Unit v5671, a person has used a ride or shop (CPerson::WinningMods), against libtpw_unit_CPerson_WinningMods.dylib
(tests/unit_CPerson_WinningMods.cpp), main app ("SimTheme Park"),
src/game/people/Person.cpp:
  CPerson::WinningMods 0x1011a570

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (the thing id, the thoughts, the ride record, the ride's takings and quality, the sound, the happiness helpers, the
world's random numbers, the sprite scripts, CTRACE and GAME_ASSERTMSG) are fakes on both sides that record their arguments and answer from
the case. The ride record (GetDBInfo) is one block with the words at +0x144 .. +0x15c and +0x7a8: the Mac's offsets on the PPC side, the
host fields of the same names on the host side (tests/ridelayout.py).

The world of a case: the person (random bytes with the floats, words and halfwords the function touches set), the object (flag halfword
at +0x2e), the ride record, the world pointer, the sound bank word, the person table halfword and the answer queues. What is compared:
the person's fields, the rest of the person's bytes, and the event log (including the CTRACE and assert texts).

No fused operations; the float to int conversions stay below 2^31. Not tested: a price of 0 or a negative price/prize (log of a
non-positive number: the conversion of NaN and infinity differs between the emulator and the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_WinningMods"
MASK = 0xFFFFFFFF

FN = 0x1011A570
SIZES = {FN: 2656}
OWN = set(SIZES)

(C_THING, C_EVENT, C_DBINFO, C_PRIZE, C_COST, C_RECORD, C_EXTRA, C_PLAY, C_TRACE, C_INGR, C_RAND, C_LOWER, C_SEED, C_SPRITES,
 C_ACTION, C_SPAWN, C_QUALITY, C_TOILET, C_PRICE, C_HAPPY, C_ASSERT) = (
    0x1012E1F8, 0x1012F3D4, 0x10107CD4, 0x1010F52C, 0x1010F6EC, 0x1010F34C, 0x10119C0C, 0x10115B80, 0x1006FCBC, 0x1010FC7C,
    0x1013B1E4, 0x1011B384, 0x1013B244, 0x1007C900, 0x1007C9F8, 0x100DA18C, 0x1010FB38, 0x10110070, 0x1010F4F4, 0x1011B198,
    0x1000DDE8)

ADDR = dict(sound=0x102F7C94, ptable=0x102A0D54, gworld=0x1037EC0C)
BLOCK = 0x1029FD48
HOST_LAYOUT = [4, 4]

PERSON_SIZE = 0x240
INFO_SIZE = 0x800
FLOATS = (0x19C, 0x1A4, 0x1A8, 0x1AC, 0x1B0, 0x1B4)
WORDS = (0x20, 0x24, 0x1A0, 0x1D0, 0x210, 0x214)
HALFS = (0xC2, 0xC4)
STATE = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, a):
    return e.cl.lab(a)


def ret(e, key):
    return e.cl.ret(key) & MASK


def st_thing(e):
    L = e.cl
    who = L.lab(e.r(4))
    L.rec("GetThingId", (who,))
    e.write(e.r(3), struct.pack(">H", L.ret("PersonId" if who == 1 else "ObjId") & 0xFFFF))


def st_event(e):
    e.cl.rec("RecordEvent", (lab(e, e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF))


def st_dbinfo(e):
    e.cl.rec("GetDBInfo", (lab(e, e.r(3)),))
    e.set_r(3, STATE["info"])


def getter(name, key):
    def fn(e):
        e.cl.rec(name, (lab(e, e.r(3)),))
        e.set_r(3, ret(e, key))
    return fn


def st_record(e):
    e.cl.rec("RecordACost", (lab(e, e.r(3)), e.r(4) & MASK))


def st_toilet(e):
    e.cl.rec("PersonUsedToilet", (lab(e, e.r(3)), e.r(4) & MASK))


def st_extra(e):
    e.cl.rec("ExtraExcitementEffects", (lab(e, e.r(3)), lab(e, e.r(4))))


def st_play(e):
    e.cl.rec("PlaySound", (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_lower(e):
    e.cl.rec("LowerHappiness", (lab(e, e.r(3)), s32(e.r(4))))


def st_happy(e):
    e.cl.rec("GetHappinessAmount", (lab(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, ret(e, "Happiness"))


def st_rand(e):
    e.cl.rec("GetRand", (lab(e, e.r(3)),))
    e.set_r(3, ret(e, "Rand"))


def st_seed(e):
    e.cl.rec("SeedRand", (lab(e, e.r(3)), e.r(4) & MASK))


def st_sprites(e):
    e.cl.rec("ESP_GetRandomSpritesFrom", (s32(e.r(3)),))
    e.set_r(3, ret(e, "Sprites"))


def st_action(e):
    e.cl.rec("ESP_GetRandomActionFrom", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, ret(e, "Action"))


def st_spawn(e):
    e.cl.rec("ESP_SpawnScript", (1 if e.r(3) == BLOCK else 0, s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3))))
    e.set_r(3, ret(e, "Script"))


REC_T0 = CL.recorder("CTRACE", "is")
REC_T2 = CL.recorder("CTRACE", "isii")


def st_trace(e):
    (REC_T2 if "%d" in e.cstr(e.r(4)) else REC_T0)(e)


for addr, fn in ((C_THING, st_thing), (C_EVENT, st_event), (C_DBINFO, st_dbinfo), (C_PRIZE, getter("GetPrizeValue", "Prize")),
                 (C_COST, getter("GetCostOfGoods", "Cost")), (C_RECORD, st_record), (C_EXTRA, st_extra), (C_PLAY, st_play),
                 (C_TRACE, st_trace), (C_INGR, getter("GetAmountOfSpecialIngredient", "Ingredient")), (C_RAND, st_rand),
                 (C_LOWER, st_lower), (C_SEED, st_seed), (C_SPRITES, st_sprites), (C_ACTION, st_action), (C_SPAWN, st_spawn),
                 (C_QUALITY, getter("GetProductQuality", "Quality")), (C_TOILET, st_toilet),
                 (C_PRICE, getter("GetPricePerUse", "Price")), (C_HAPPY, st_happy),
                 (C_ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))):
    STUBS.internal(addr, fn)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.hfmt, v & 0xFFFF))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def rd_half(self, a):
        return struct.unpack(self.hfmt, self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    hfmt = ">H"

    def __init__(self, e, case):
        self.e = e
        self.person = e.alloc(PERSON_SIZE)
        self.obj = e.alloc(0x40)
        self.world = e.alloc(0x40)
        self.info = e.alloc(INFO_SIZE)
        STATE["info"] = self.info
        self.L = e.cl = CL.PpcLog({self.person: 1, self.obj: 2, self.person + 0x30: 3, self.world: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def rec(self, mac):
        return mac

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [self.person, self.obj])


class Host(Side):
    fmt = "<I"
    hfmt = "<H"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.person = lib.tpw_wm_alloc(PERSON_SIZE)
        self.obj = lib.tpw_wm_alloc(0x40)
        self.world = lib.tpw_wm_alloc(0x40)
        self.info = lib.tpw_wm_alloc(max(INFO_SIZE, ridelayout.host_size(lib)))
        CL.host_begin(lib, {1: self.person, 2: self.obj, 3: self.person + 0x30, 4: self.world}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_wm_addr(name.encode())

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_wm_winning(self.person, self.obj)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wm_addr.restype, lib.tpw_wm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wm_alloc.restype, lib.tpw_wm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wm_winning.restype, lib.tpw_wm_winning.argtypes = None, [C.c_uint64, C.c_uint64]
    ridelayout.bind(lib)
    out = (C.c_int64 * 32)()
    n = lib.tpw_wm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.write(s.person, case["fill"])
    for off, bits in zip(FLOATS, case["floats"]):
        s.words(s.person + off, [bits])
    for off, w in zip(WORDS, case["words"]):
        s.words(s.person + off, [w])
    for off, h in zip(HALFS, case["halfs"]):
        s.half(s.person + off, h)
    s.half(s.obj + 0x2E, case["flag"])
    for i, w in enumerate(case["d"]):         # 0x144 .. 0x15c: seven words
        s.words(s.info + s.rec(0x144 + 4 * i), [w])
    s.words(s.info + s.rec(0x7A8), [case["ui"]])
    s.ptr(s.g("gworld"), s.world)
    s.words(s.g("sound") + 4, [case["snd"]])
    s.half(s.g("ptable") + 2, case["ptab"])
    if isinstance(s, Host):
        s.ptr(s.g("info"), s.info)


def observe(s):
    p = s.person
    rest = bytearray(s.read(p, PERSON_SIZE))
    for off in FLOATS + WORDS:
        rest[off:off + 4] = b"\0\0\0\0"
    for off in HALFS:
        rest[off:off + 2] = b"\0\0"
    return {
        "log": s.log(),
        "floats": [s.rd_word(p + o) for o in FLOATS],
        "words": [s.rd_word(p + o) for o in WORDS],
        "halfs": [s.rd_half(p + o) for o in HALFS],
        "rest": bytes(rest),
    }


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rfl(r):
    k = r.random()
    if k < 0.6:
        return fb(float(r.randint(-10, 115)))
    if k < 0.9:
        return fb(r.uniform(-10, 115))
    return fb(r.choice([0.0, 100.0, -0.0, 99.5, 100.5]))


def rd(r):
    return r.choice([0, 0, 1, -1, 2, -2, 5, 10, r.randint(-60, 60), r.randint(-60, 60), r.randint(-100000, 100000)])


def base(r, **o):
    c = {
        "fill": bytes(r.randrange(256) for _ in range(PERSON_SIZE)),
        "floats": [rfl(r) for _ in FLOATS],
        "words": [r.choice([0, 2, 1, r.randrange(1 << 32)]), r.choice([0, 2, 1, 3]), r.randrange(1 << 32), r.randrange(1 << 32),
                  r.choice([0, 0, 5, r.randrange(1 << 32)]), r.randrange(1 << 32)],
        "halfs": [r.randrange(1 << 16), r.randrange(1 << 16)],
        "flag": r.choice([0, 1, 1, 2, 3, 0x8000, 0x8001]),
        "d": [rd(r) for _ in range(5)] + [r.choice([-1, 0, 1, 2, 3, 4, 5]), r.choice([-1, 0, 1, 2, 3, 4])],
        "ui": r.choice([0, 1, 2, 2, 3, 7]),
        "snd": r.randrange(1 << 32),
        "ptab": r.randrange(1 << 16),
    }
    c["d"][0] = r.choice([c["d"][0], 0, c["d"][1]])
    c["d"][1] = r.choice([c["d"][1], 0, c["d"][0]])
    price = r.choice([1, 2, 3, 100, r.randint(1, 5000)])
    c["queues"] = {
        "PersonId": [r.randint(0, 65535)], "ObjId": [r.randint(0, 65535)],
        "Prize": [r.choice([price, 1, 2 * price, r.randint(1, 5000), 1000])], "Price": [price],
        "Cost": [r.randint(-1000, 100000)], "Ingredient": [r.choice([0, 1, 5, 20, 29, 30, 100, 255, r.randint(0, 40)])],
        "Quality": [r.choice([0, 1, 50, 100, 255, r.randint(0, 255)])],
        "Happiness": [r.choice([0, 1, 100, 255, 256, 0x3FF, r.randint(0, 255)])],
        "Rand": [r.randrange(1 << 32) for _ in range(4)], "Sprites": [r.randint(0, 50)], "Action": [r.randint(0, 50)],
        "Script": [r.randint(-5, 1 << 20)],
    }
    for k, v in o.items():
        if k in ("ui", "flag"):
            c[k] = v
        elif k == "k":
            c["d"][6] = v
        elif k == "m":
            c["d"][5] = v
        elif k == "w24":
            c["words"][1] = v
        elif k == "w210":
            c["words"][4] = v
        else:
            c["queues"][k] = [v]
    return c


def edges():
    r = random.Random(5671)
    for ui in (0, 1, 2, 3):
        for k in (-1, 0, 1, 2, 3, 4):
            for m in (-1, 0, 1, 2, 3, 4, 5):
                yield base(r, ui=ui, k=k, m=m)
    for w24 in (0, 1, 2):
        yield base(r, k=2, w24=w24, flag=0)
    for w210 in (0, 7):
        yield base(r, k=1, w210=w210)
    for q in (0, 9, 10, 11, 100, 255):
        yield base(r, k=1, Quality=q, w210=0)
    for flag in (0, 1, 0x100, 0x101):
        yield base(r, flag=flag)
    for ingr in (0, 1, 14, 15, 16, 28, 29, 30):
        for rnd in (0, 1, 7, 8):
            c = base(r, Ingredient=ingr)
            c["queues"]["Rand"] = [rnd]
            c["d"][0] = c["d"][1] = r.choice([0, 1, 5, 20, 29 - ingr])
            yield c
    for prize, price in ((1, 1), (2, 1), (1, 2), (1000, 3), (3, 1000), (5000, 1)):
        yield base(r, ui=2, Prize=prize, Price=price, Happiness=100)
    for tf in (0.0, 90.0, 90.5, 91.0, 255.5, 256.0, 300.0):
        c = base(r, flag=1)
        c["floats"][4] = fb(tf)
        yield c
    for fv in (100.0, 99.0, 0.0, 1.0, 50.0):
        for dv in (0, 1, -1, 100, -100, 50):
            c = base(r, ui=0)
            c["floats"] = [fb(fv)] * 6
            c["d"] = [dv] * 5 + [r.choice([1, 2, 3]), 0]
            c["queues"]["Ingredient"] = [abs(dv) % 256]
            yield c


def rnd(r):
    return base(r)


TESTS = [
    FnTest("CPerson::WinningMods", ppc, host, edge=edges, rnd=rnd),
]
