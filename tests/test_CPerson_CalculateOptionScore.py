"""Unit v5686, CPerson::CalculateOptionScore, against libtpw_unit_CPerson_CalculateOptionScore.dylib (tests/unit_CPerson_CalculateOptionScore.cpp),
main app ("SimTheme Park"), src/CPerson_..._CPerson_CalculateOptionScore.cpp:
  CPerson::CalculateOptionScore 0x101185f8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetBackOfQueue, GAME_ASSERTMSG, GetThingId, GetDBInfo, CTRACE, GetRegionalEffectCell, GetNumPeopleInQueue, GetPreferredExcitement,
GetExcitementLevel, GetObjectAge, GetWeather) are fakes on both sides that record their arguments (CTRACE and the assert: the level, a hash of
the format text and every argument; the ride name pointer as 1 when it is the record's name) and answer from the case's queues.

The world of a case: the person block (cell bytes, four float needs, the eight recent-ride halfwords), the ride object (type, flags, capacity),
the ride record, the weather, the region effect cell, the balance words, the two match tables, the two "no" halfwords, gThingArray entries 0 and
the person's ride, gWorld and gWorldMap. What is compared: the answer, the event log and the person block after the call.

No fused operations (the float paths use fsubs, fdivs, fadds, fmuls; ROUNDING is kept for fctiwz). Not tested: needs and amounts outside 0 to 100
and record bytes above 109 (the table index would run past the two tables, which sit side by side on the Mac but are separate arrays on the
host); a ride capacity whose times four wraps to 0 (divwu by 0: the emulator answers the dividend, the hardware is undefined); a person ride index of 0x2800 or more (past gThingArray).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CPerson_CalculateOptionScore"
MASK = 0xFFFFFFFF

FN = 0x101185F8
SIZES = {FN: 2612}
OWN = set(SIZES)

GET_BOQ, ASSERT, GET_ID, GET_DB, CTRACE, GET_CELL = 0x101092F4, 0x1000DDE8, 0x1012E1F8, 0x10107CD4, 0x1006FCBC, 0x10101640
GET_NUMQ, GET_PREF, GET_EXC, GET_AGE, GET_WEATHER = 0x10108EF4, 0x1011B4B4, 0x1010CFB0, 0x10107F50, 0x1013ED74

ADDR = dict(noBoq=0x102A0FAC, noRide=0x102A0FAE, tabA=0x102A0F1E, tabB=0x102A0F97, balvals=0x102B4568 + 8, thingarr=0x1034CBFC,
            gWorldMap=0x1037EC08, gWorld=0x1037EC0C)
# the host's sizes the module relies on, checked once: BalanceValue 8 bytes, the thing entry's pointer first, the effect at +8
HOST_CHECK = {9: 8, 11: 0, 12: 8}

PERSON_SIZE = 0x240
SLOTS = (18, 19, 20, 26, 27, 28, 29, 30, 31, 32)


def s32(v):
    return T.s32(v)


def fhash(text):
    h = 0
    for ch in text:
        h = (h * 31 + (ord(ch) if isinstance(ch, str) else ch)) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def which(e, p):
    return e.fk["which"].get(p & MASK, 0)


def text_of(e, addr):
    s = e.cstr(addr)
    return s.decode("latin1") if isinstance(s, (bytes, bytearray)) else s


def stub_boq(e):
    L = e.cl
    L.rec("GetBackOfQueue", (which(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetBackOfQueue") & 0xFFFF))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (e.r(3) & MASK, fhash(text_of(e, e.r(4)))))


def stub_id(e):
    L = e.cl
    w = which(e, e.r(4))
    L.rec("GetThingId", (w,))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId:%d" % w) & 0xFFFF))


def stub_db(e):
    e.cl.rec("GetDBInfo", (which(e, e.r(3)),))
    e.set_r(3, e.fk["db"])


def stub_ctrace(e):
    fmt = text_of(e, e.r(4))
    vals = [e.r(3) & MASK, fhash(fmt)]
    reg = 5
    i = 0
    while i < len(fmt):
        if fmt[i] == "%":
            i += 1
            v = e.r(reg) & MASK
            reg += 1
            if fmt[i] == "l":
                v = 1 if v == e.fk["db"] + 0x68C else (v - e.fk["db"]) & MASK
            vals.append(v)
        i += 1
    e.cl.rec("CTRACE", tuple(vals))


def stub_cell(e):
    e.cl.rec("GetRegionalEffectCell", (which(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.fk["cell"])


def stub_numq(e):
    e.cl.rec("GetNumPeopleInQueue", (which(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetNumPeopleInQueue") & MASK)


def stub_pref(e):
    e.cl.rec("GetPreferredExcitement", (which(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetPreferredExcitement") & 0xFF)


def stub_exc(e):
    flag = e.r(4) & 0xFF
    e.cl.rec("GetExcitementLevel", (which(e, e.r(3)), flag))
    e.set_r(3, e.cl.ret("GetExcitementLevel:%d" % flag) & 0xFF)


def stub_age(e):
    e.cl.rec("GetObjectAge", (which(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetObjectAge") & MASK)


def stub_weather(e):
    e.cl.rec("GetWeather", (which(e, e.r(3)),))
    e.set_r(3, e.fk["weather"])


for _a, _f in ((GET_BOQ, stub_boq), (ASSERT, stub_assert), (GET_ID, stub_id), (GET_DB, stub_db), (CTRACE, stub_ctrace), (GET_CELL, stub_cell),
               (GET_NUMQ, stub_numq), (GET_PREF, stub_pref), (GET_EXC, stub_exc), (GET_AGE, stub_age), (GET_WEATHER, stub_weather)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, fmtc, v):
        self.write(a, struct.pack(self.fmt[0] + fmtc, v))

    def half(self, a, v):
        self.put(a, "H", v & 0xFFFF)

    def word(self, a, v):
        self.put(a, "I", v & MASK)

    def flt(self, a, v):
        self.put(a, "f", v)

    def ptr(self, a, v):
        self.put(a, self.ptrfmt, v)


class Ppc(Side):
    fmt = ">I"
    ptrfmt = "I"
    thing_stride = 0x14
    db_off = {144: 0x144, 148: 0x148, 114: 0x114, 0xC4: 0xC4, 0x1B8: 0x1B8}
    weather_off = 0x30
    bal_stride = 4
    cell_off = 8

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

    def setup(self, person, obj, world, mp, db, weather, cell):
        self.e.fk = {"which": {person: 1, obj: 2, world: 3, mp: 4}, "db": db, "weather": weather, "cell": cell}

    def call(self, person, obj):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [person, obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ptrfmt = "Q"
    bal_stride = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lay = lib.lay
        self.thing_stride = lay[10]
        self.db_off = {144: lay[1], 148: lay[2], 114: lay[3], 0xC4: lay[4], 0x1B8: lay[5]}
        self.weather_off = lay[7]
        self.cell_off = lay[12]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cos_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cos_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup(self, person, obj, world, mp, db, weather, cell):
        self.lib.tpw_cos_setup(person, obj, world, mp, db, weather, cell)

    def call(self, person, obj):
        return self.lib.tpw_cos_call(person, obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cos_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setup").argtypes = [C.c_uint64] * 7
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lay = list(out)[:n]
    for k, v in HOST_CHECK.items():
        assert lay[k] == v, (k, lay)
    lib.lay = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    person = s.alloc(PERSON_SIZE)
    obj = s.alloc(0x60)
    world = s.alloc(0x40)
    mp = s.alloc(0x40)
    db = s.alloc(0x2000)
    weather = s.alloc(0x200)
    cell = s.alloc(0x20)
    s.setup(person, obj, world, mp, db, weather, cell)
    s.half(s.g("noBoq"), c["noBoq"])
    s.half(s.g("noRide"), c["noRide"])
    s.write(s.g("tabA"), bytes(c["tabA"]))
    s.write(s.g("tabB"), bytes(c["tabB"]))
    for slot in SLOTS:
        s.word(s.g("balvals") + s.bal_stride * slot, c["bal"][slot])
    # the person
    s.write(person + 4, bytes([c["cx"]]))
    s.write(person + 6, bytes([c["cy"]]))
    for i, off in enumerate((0x1A4, 0x1A8, 0x1AC, 0x1B0)):
        s.flt(person + off, c["amt"][i])
    for i in range(8):
        s.half(person + 0x1E0 + 2 * i, c["ids"][i])
    # the ride object
    s.half(obj + 0x0E, c["otype"])
    s.half(obj + 0x2E, c["oflags"])
    s.word(obj + 0x3C, c["ocap"])
    # the ride record
    for key, name in ((144, "d144"), (148, "d148"), (114, "d114"), (0xC4, "dc4"), (0x1B8, "d1b8")):
        s.word(db + s.db_off[key], c[name])
    s.word(weather + s.weather_off, c["drops"])
    s.half(cell + s.cell_off, c["effect"])
    # gThingArray: entry 0 and the person's ride
    base = s.g("thingarr")
    thing = s.alloc(0x20)
    s.half(thing + 0x0E, c["ttype"])
    s.ptr(base, thing if c["th0"] else 0)
    idx = c["ids"][0]
    if idx < 0x2800:
        s.ptr(base + idx * s.thing_stride, thing if c["tmode"] else 0)
    s.ptr(s.g("gWorld"), world) if s.fmt == "<I" else s.word(s.g("gWorld"), world)
    s.ptr(s.g("gWorldMap"), mp) if s.fmt == "<I" else s.word(s.g("gWorldMap"), mp)
    return person, obj


def run(s, c):
    try:
        person, obj = build(s, c)
        answer = s.call(person, obj)
        return {"answer": answer, "log": s.log(), "cell": list(s.read(person + 4, 3)),
                "floats": [struct.unpack(s.fmt[0] + "I", s.read(person + o, 4))[0] for o in (0x1A4, 0x1A8, 0x1AC, 0x1B0)],
                "ids": [struct.unpack(s.fmt[0] + "H", s.read(person + 0x1E0 + 2 * i, 2))[0] for i in range(8)]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def pick(r, *vals):
    return r.choice(vals)


def make(r):
    cx, cy = r.randrange(0, 128), r.randrange(0, 128)
    noBoq = pick(r, 0, 5, 100, 0xFFFF)
    b1 = noBoq if r.random() < 0.15 else r.randrange(0, 0x4001)
    if r.random() < 0.6:
        b2 = ((cx + r.randrange(-3, 4)) & 0x7F) + 1
        b3 = (cy + r.randrange(-2, 3)) * 128 + 1 + r.randrange(0, 128)
        b3 = max(1, b3)
    else:
        b2, b3 = r.randrange(0, 0x4001), r.randrange(0, 0x4001)
    oid, pid = r.randrange(1, 0x2800), r.randrange(1, 0x2800)
    ids = [r.randrange(0, 0x2800) for _ in range(8)]
    for i in range(8):
        if r.random() < 0.18:
            ids[i] = oid
    noRide = pick(r, 0, 7, 1000)
    if r.random() < 0.3:
        ids[0] = noRide
    ids[0] = min(ids[0], 0x27FF)
    bal = {18: pick(r, 0, 5, 100, 0xFFFFFFFF, r.randrange(0, 1000)), 19: pick(r, 0, 1, 2, 3, 100, 0x7FFFFFFF), 20: pick(r, 0, 1, 2, 5, -3),
           26: pick(r, 0, 1, 10, 100, 0x10005), 27: pick(r, 0, 1, 10, 100), 28: pick(r, 0, 1, 10, 100, 0x20003),
           29: pick(r, 0, 1, 10, 100), 30: pick(r, 0, 1, 10, 100), 31: pick(r, 0, 1, 10, 100), 32: pick(r, 0, 1, 10, 100)}
    if r.random() < 0.5:
        for k in (26, 27, 28, 29, 30, 31, 32):
            bal[k] = r.randrange(1, 60)

    def amount():
        return pick(r, 0.0, 100.0, 99.9, 10.0, 9.99, 0.5, r.uniform(0, 100), r.uniform(0, 100), float(r.randrange(0, 101)))

    def need():
        return (r.randrange(0, 1 << 24) << 8) | r.randrange(0, 110)

    return {
        "noBoq": noBoq, "noRide": noRide, "cx": cx, "cy": cy, "amt": [amount() for _ in range(4)], "ids": ids,
        "otype": r.randrange(0, 0x10000), "oflags": r.randrange(0, 0x10000), "ocap": pick(r, 0, 1, 5, 20, 0x3FFFFFFF, r.randrange(0, 200)),
        "ttype": r.randrange(0, 0x10000), "tmode": r.choice([0, 1, 1]), "th0": 1 if r.random() < 0.1 else 0,
        "tabA": [r.randrange(256) for _ in range(121)], "tabB": [r.randrange(256) for _ in range(21)], "bal": bal,
        "d144": need(), "d148": need(), "d114": pick(r, 0, 1, 5), "dc4": pick(r, 0, 1, 5, -2, 0x7FFFFFFF),
        "d1b8": pick(r, 0, 1000, 3000, 3001, 6000, 30000, -500, 90000), "drops": pick(r, 0, 1, 50, 0x80000000), "effect": pick(r, 0, 1, 2, 3, -1, -5),
        "queues": {
            "GetBackOfQueue": [b1, b2, b3], "GetThingId:1": [oid], "GetThingId:2": [pid],
            "GetNumPeopleInQueue": [pick(r, 0, 1, 3, 10, 40, 1000)], "GetObjectAge": [pick(r, 0, 5, 100, 1000, -1, 0x7FFFFFFF)],
            "GetPreferredExcitement": [r.randrange(256)], "GetExcitementLevel:0": [pick(r, 0, 50, 255, r.randrange(256))],
            "GetExcitementLevel:1": [pick(r, 0, 1, 77)],
        },
    }


def fix_ids(c):
    """Make the person's ride slots hit the object's id when asked for (the ids are taken from the queue)."""
    return c


def edges():
    r = random.Random(686)
    seed = 0
    # each override list gives one extra case per entry on top of a random world
    knobs = []
    knobs += [{"queues_boq": [None, None, None]}]
    for b in ({"noBoqEq": 1},):
        knobs.append(b)
    for k in range(120):
        seed += 1
        rr = random.Random(seed * 7919)
        c = make(rr)
        sel = k % 30
        q = c["queues"]
        oid = q["GetThingId:1"][0]
        if sel == 0:
            q["GetBackOfQueue"][0] = c["noBoq"]
        elif sel == 1:
            c["ids"][0] = c["noRide"]
        elif sel == 2:
            c["ids"][0] = 77
            c["tmode"] = 1
            c["ttype"] = c["otype"]
            c["th0"] = 0
        elif sel == 3:
            c["ids"][0] = 77
            c["tmode"] = 0
        elif sel == 4:
            c["ids"][0] = 77
            c["tmode"] = 1
            c["ttype"] = c["otype"] ^ 1
            c["th0"] = 1
        elif sel == 5:
            c["effect"] = 0
            c["ocap"] = 0x3FFFFFFF
            q["GetBackOfQueue"][1:] = [c["cx"] + 1, c["cy"] * 128 + 1]
        elif sel == 6:
            c["ocap"] = 0
            q["GetBackOfQueue"][1:] = [c["cx"] + 1, c["cy"] * 128 + 1]
        elif sel == 7:
            for kk in (26, 27, 28, 29, 30, 31, 32):
                c["bal"][kk] = 0
        elif sel == 8:
            q["GetObjectAge"] = [c["bal"][18]]
        elif sel == 9:
            q["GetObjectAge"] = [(c["bal"][18] + 1) & MASK]
        elif sel == 10:
            c["drops"] = 1
            c["d114"] = 1
        elif sel == 11:
            c["drops"] = 0
            c["d114"] = 1
        elif sel == 12:
            c["dc4"] = 0
            c["d1b8"] = 3000
        elif sel == 13:
            c["dc4"] = 0
            c["d1b8"] = 3001
        elif sel == 14:
            c["dc4"] = 0
            c["d1b8"] = 30000
        elif sel == 15:
            c["dc4"] = 3
        elif sel in (16, 17, 18, 19):
            c["ids"][sel - 16 + 0] = c["noRide"] if False else oid
            if sel == 16:
                c["ids"][0] = oid
        elif sel in (20, 21, 22, 23):
            c["ids"][4 + sel - 20] = oid
        elif sel == 24:
            c["ids"][4:8] = [oid] * 4
            c["ids"][0:4] = [oid] * 4
        elif sel == 25:
            q["GetPreferredExcitement"] = [0]
            q["GetExcitementLevel:0"] = [255]
        elif sel == 26:
            q["GetPreferredExcitement"] = [255]
            q["GetExcitementLevel:0"] = [0]
        elif sel == 27:
            c["oflags"] = 1
        elif sel == 28:
            c["oflags"] = 0xFFFE
        elif sel == 29:
            c["amt"] = [100.0, 100.0, 100.0, 100.0]
        if sel in (16, 17, 18, 19):
            # the 1e0 slot must differ from the "no ride" word so the check at the top runs; the chain tests need the id in the slot
            if c["ids"][0] == c["noRide"]:
                c["noRide"] = c["noRide"] + 1
        yield c


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CPerson::CalculateOptionScore", ppc, host, edge=edges, rnd=rnd),
]
