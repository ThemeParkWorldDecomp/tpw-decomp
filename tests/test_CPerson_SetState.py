"""Unit v5370, CPerson::SetState, against libtpw_unit_CPerson_SetState.dylib (tests/unit_CPerson_SetState.cpp), main app
("SimTheme Park"), src/game/people/PersonQueue.cpp:
  CPerson::SetState 0x10120354

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the script, thought,
map, world, challenge and analyser calls, the ride's WillIWin and GetDBInfo, the sprite helpers, the assert, the float to unsigned glue) is a
fake on both sides that records its arguments (objects as offsets from the case's base block) and answers from the case's queues.

The world of a case: one base block holding the person (fields by Mac offset), a map, a challenge HQ, an analyser, eight rides each with a
ride record; the thing array rows, the person table, the world words, gWorld and gWorldMap. What is compared: the event log and every
field the function can write (state word, +0x1c, +0x28, +0x1f1 byte, +0x1f4, +0x1fc, +0x200, +0x208, +0x210, +0xc2, +0x1dc).
The ride records' words are written at the Mac's offsets on the PPC side and at the host fields of the same names on the host side
(tests/ridelayout.py); the rides are 0x600 apart so that the host's words stay inside their ride's block.

No fused operations. Not tested: nothing excluded; the state past 0x15 does nothing in both.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_SetState"
MASK = 0xFFFFFFFF

FN = 0x10120354
SIZES = {FN: 1944}
OWN = set(SIZES)

ADDR = dict(gThingArray=0x1034CBFC, gWorldMap=0x1037EC08, gWorld=0x1037EC0C, gPersonTable=0x102A0D54, gBlock=0x1029FD48)

WORLD_BYTES = 0x1E0100
BASE_BYTES = 0x4000
MAP_OFF, HQ_OFF, ANA_OFF = 0x400, 0x300, 0x340
RIDE0, RIDE_STEP = 0x800, 0x600
INFO_DELTA = 0x200
NRIDES = 8
HOST_LAYOUT = [1]

WORLD_PPC = [0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- PowerPC fakes
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(e, p):
    return (p - e.base) & MASK


def stub(addr, name, fn):
    def run(e):
        fn(e, e.cl)
    STUBS.internal(addr, run)


def rec_ret(name, args, key=None):
    def fn(e, L):
        L.rec(name, tuple(a(e) for a in args))
        if key:
            e.set_r(3, L.ret(key) & MASK)
    return fn


R3REL = lambda e: rel(e, e.r(3))
R3W = lambda e: 1 if e.r(3) == WORLD_PPC[0] else 0


def R(n):
    return lambda e: e.r(n) & MASK


def RH(n):
    return lambda e: (e.r(n) >> 16) & 0xFFFF


stub(0x100FAA74, "SetSpriteScript", rec_ret("SetSpriteScript", [R3REL, R(4)]))
stub(0x100FAB9C, "SpawnSpriteScript", rec_ret("SpawnSpriteScript", [R3REL, R(4), R(5), R(6)]))
stub(0x100FAC4C, "KillSpriteScript", rec_ret("KillSpriteScript", [R3REL]))
stub(0x1013B1E4, "GetRand", rec_ret("GetRand", [R3W], "GetRand"))
stub(0x1013B244, "SeedRand", rec_ret("SeedRand", [R3W, R(4)]))
stub(0x1000DDE8, "GAME_ASSERTMSG", rec_ret("GAME_ASSERTMSG", [R(3), lambda e: len(e.cstr(e.r(4)))]))
stub(0x101103E0, "WillIWin", rec_ret("WillIWin", [R3REL, lambda e: rel(e, e.r(4))], "WillIWin"))
stub(0x101149CC, "UpdatePositionInEngine", rec_ret("UpdatePositionInEngine", [R3REL, lambda e: fb(e.f(1))]))
stub(0x101152A4, "SpawnParticleEffect", rec_ret("SpawnParticleEffect", [R3REL, R(4)]))
stub(0x1007C900, "ESP_GetRandomSpritesFrom", rec_ret("ESP_GetRandomSpritesFrom", [R(3)], "sprites"))
stub(0x1007C9F8, "ESP_GetRandomActionFrom", rec_ret("ESP_GetRandomActionFrom", [R(3), R(4)], "action"))
stub(0x100DA18C, "ESP_SpawnScript", rec_ret("ESP_SpawnScript", [lambda e: 1 if e.r(3) == ADDR["gBlock"] else 0, R(4), R(5), R(6),
                                                              lambda e: fb(e.f(1)), lambda e: fb(e.f(2)), lambda e: fb(e.f(3))], "script"))
stub(0x1012F35C, "RemoveThoughtBubble", rec_ret("RemoveThoughtBubble", [R3REL]))
stub(0x100D9478, "SCR_KillScript", rec_ret("SCR_KillScript", [R(3)]))
stub(0x1012F3D4, "RecordEvent", rec_ret("RecordEvent", [R3REL, R(4), RH(5)]))
stub(0x100F7324, "AddToCounter", rec_ret("AddToCounter", [R3REL, R(4), R(5), RH(6), RH(7)]))
stub(0x1011B11C, "MakeBalloonPop", rec_ret("MakeBalloonPop", [R3REL]))
stub(0x1011962C, "GetTimeInPark", rec_ret("GetTimeInPark", [R3REL], "timeinpark"))
stub(0x101154F0, "SetDest", rec_ret("SetDest", [R3REL, RH(4), RH(5)], "setdest"))


def f_get_hq(e, L):
    L.rec("GetChallengeHQ", (R3W(e),))
    e.set_r(3, (e.base + HQ_OFF) & MASK)


def f_get_ana(e, L):
    L.rec("GetParkAnalyser", (R3W(e),))
    e.set_r(3, (e.base + ANA_OFF) & MASK)


def f_person_left(e, L):
    L.rec("PersonLeftPark", (rel(e, e.r(3)), e.r(4) & MASK))


def f_booth(e, L):
    which = e.r(5) & MASK
    L.rec("GetTicketBoothPos", (rel(e, e.r(4)), which))
    e.write(e.r(3), struct.pack(">H", L.ret("booth%d" % which) & 0xFFFF))


def f_dbinfo(e, L):
    L.rec("GetDBInfo", (rel(e, e.r(3)),))
    e.set_r(3, (e.r(3) + INFO_DELTA) & MASK)


def f_thingid(e, L):
    L.rec("GetThingId", (rel(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("thingid") & 0xFFFF))


def f_glue(e, L):
    e.set_r(3, int(e.f(1)) & MASK)


stub(0x1013EE88, "GetChallengeHQ", f_get_hq)
stub(0x1013EBC8, "GetParkAnalyser", f_get_ana)
stub(0x100E4600, "PersonLeftPark", f_person_left)
stub(0x101019BC, "GetTicketBoothPos", f_booth)
stub(0x10107CD4, "GetDBInfo", f_dbinfo)
stub(0x1012E1F8, "GetThingId", f_thingid)
stub(0x10237CD8, "fp2unsigned", f_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD_PPC[0] = T.persist_alloc(runner.emu, WORLD_BYTES)


# ---------------------------------------------------------------------------------------------------------------- the two sides
class Side:
    walk = False

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd(self, a, code):
        n = struct.calcsize(self.fmt[0] + code)
        return struct.unpack(self.fmt[0] + code, self.read(a, n))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self):
        return WORLD_PPC[0]

    def setup(self, base):
        self.e.base = base
        self.words(ADDR["gWorld"], [WORLD_PPC[0]])
        self.words(ADDR["gWorldMap"], [base + MAP_OFF])

    def set_thing(self, idx, p):
        self.words(ADDR["gThingArray"] + 0x14 * idx, [p])

    def rec(self, mac):
        return mac

    def call(self, person, state):
        T.call_ppc(self.e, FN, [person & MASK, state & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.walk = lib.tpw_ss_walkfix() == 1
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ss_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ss_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self):
        if not getattr(self.lib, "world_block", 0):
            self.lib.world_block = self.lib.tpw_ss_alloc(WORLD_BYTES)
        return self.lib.world_block

    def setup(self, base):
        self.lib.tpw_ss_setup(self.world(), base, base + MAP_OFF)

    def set_thing(self, idx, p):
        self.lib.tpw_ss_setthing(idx, p)

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def call(self, person, state):
        self.lib.tpw_ss_setstate(person, state & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ss_addr.restype, lib.tpw_ss_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ss_alloc.restype, lib.tpw_ss_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ss_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ss_setup.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_ss_setthing.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_ss_setstate.argtypes = [C.c_uint64, C.c_uint32]
    ridelayout.bind(lib)
    out = (C.c_int64 * 32)()
    n = lib.tpw_ss_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world
PW = [0xC, 0x1C, 0x20, 0x24, 0x28, 0xD0, 0x1FC, 0x1F4, 0x200, 0x204, 0x208, 0x210, 0x214]   # person words
PH = [0xC2, 0x1DC]                                                                           # person halfwords
PB = [4, 6, 0x18, 0x19, 0x1A, 0x1B, 0x1F1]                                                   # person bytes


def build(s, case):
    base = s.alloc(BASE_BYTES)
    s.setup(base)
    p = base
    for o in PW:
        s.words(p + o, [case["pw"][o]])
    for o in PH:
        s.halves(p + o, [case["ph"][o]])
    for o in PB:
        s.write(p + o, bytes([case["pb"][o]]))
    if s.walk:
        pb = case["pb"]
        s.halves(p + 0x18, [pb[0x18] << 8 | pb[0x19], pb[0x1A] << 8 | pb[0x1B]])
    for k in range(NRIDES):
        ride = base + RIDE0 + k * RIDE_STEP
        s.halves(ride + 0xE, [case["ride_type"][k]])
        s.halves(ride + 0x2E, [case["ride_flags"][k]])
        s.words(ride + INFO_DELTA + s.rec(0x100), [case["info100"][k]])
        s.words(ride + INFO_DELTA + s.rec(0x15C), [case["info15c"][k]])
    for i in range(16):
        k = case["things"][i]
        s.set_thing(i, 0 if k is None else base + RIDE0 + k * RIDE_STEP)
    s.halves(s.g("gPersonTable"), case["table"])
    s.words(s.world() + 0x1DA70C, [case["world_word"]])
    s.base = base
    s.person = p


def observe(s, case):
    p = s.person
    out = {"log": s.log()}
    for o in PW + [0x220]:
        out["w%x" % o] = s.rd(p + o, "I")
    for o in PH:
        out["h%x" % o] = s.rd(p + o, "H")
    for o in PB:
        out["b%x" % o] = s.rd(p + o, "B")
    if s.walk:
        for o in (0x18, 0x1A):
            h = s.rd(p + o, "H")
            out["b%x" % o], out["b%x" % (o + 1)] = h >> 8, h & 0xFF
    return out


def run(s, case):
    try:
        build(s, case)
        s.call(s.person, case["state"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases
def r32(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, 150, 149, 151, r.randrange(1 << 32), r.randrange(1 << 32), r.randrange(1000)])


def nz(r):
    return r.choice([0, 0, r.randrange(1, 1 << 32), 1])


def base_case(r, state):
    c = {"state": state}
    pw = {o: r.choice([0, r32(r), r.randrange(1 << 32)]) for o in PW}
    for o in (0xC, 0x28, 0xD0, 0x204, 0x210, 0x214):
        pw[o] = nz(r)
    c["pw"] = pw
    ident = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3])
    c["ph"] = {0xC2: r.randrange(65536), 0x1DC: ident}
    pb = {o: r.randrange(256) for o in PB}
    pb[0x1F1] = r.choice([0, 1, 5, 25, 83, 84, 255, r.randrange(256), r.randrange(256)])
    c["pb"] = pb
    cell = (pb[4] + pb[6] * 128 + 1) & 0xFFFF
    c["ride_type"] = [r.randrange(65536) for _ in range(NRIDES)]
    c["ride_flags"] = [r.choice([0, 0x20, 0xFFFF, r.randrange(65536)]) for _ in range(NRIDES)]
    c["info100"] = [nz(r) for _ in range(NRIDES)]
    c["info15c"] = [r.choice([1, 0, 2, 0xFFFFFFFF, r.randrange(1 << 32)]) for _ in range(NRIDES)]
    c["things"] = [(0 if (ident == 0 or r.random() < 0.2) else None)] + list(range(1, NRIDES)) + [None] * 8
    c["table"] = [r.randrange(65536), r.randrange(65536)]
    c["world_word"] = r.randrange(1 << 32)
    c["queues"] = {
        "booth0": [cell if r.random() < 0.4 else r.randrange(65536)],
        "booth1": [cell if r.random() < 0.5 else r.randrange(65536)],
        "GetRand": [r32(r)],
        "WillIWin": [r.choice([0, 1])],
        "thingid": [r.randrange(65536)],
        "sprites": [r.randrange(1 << 32)],
        "action": [r.randrange(1 << 32)],
        "script": [r.randrange(1 << 32)],
        "timeinpark": [r.choice([0, 1, 0x40000000, r.randrange(1 << 32)])],
        "setdest": [r.randrange(2)],
    }
    return c


def edges():
    r = random.Random(5370)
    for state in list(range(22)) + [22, 23, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]:
        for _ in range(10 if state < 22 else 2):
            yield base_case(r, state)
    for b in (0, 1, 4, 5, 10, 25, 83, 84, 100, 200, 254, 255):
        c = base_case(r, 8)
        c["pb"][0x1F1] = b
        yield c
    for rv in (0, 149, 150, 151, 299, 300, 0xFFFFFFFF, 0x80000000):
        c = base_case(r, 5)
        c["queues"]["GetRand"] = [rv]
        yield c


def rnd(r):
    return base_case(r, r.choice(list(range(22)) * 3 + [22, 40, 0xFFFFFFFF]))


TESTS = [
    FnTest("CPerson::SetState", ppc, host, edge=edges, rnd=rnd),
]
