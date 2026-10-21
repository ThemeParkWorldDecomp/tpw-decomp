"""Unit v5052: CPerson::CPerson (the constructor of a person) and CPerson::ReceiveMessage, against libtpw_unit_CPerson_CPerson.dylib
(tests/unit_CPerson_CPerson.cpp), main app ("SimTheme Park"), src/game/people/Person.cpp:
  CPerson::CPerson 0x10116634, CPerson::ReceiveMessage 0x10116e24

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (CPeep::CPeep, GetRand, SeedRand, GetThingId, ESP_GetRandomSpritesFrom, the script spawns, IsInsidePark, SetDest,
UpdatePositionInEngine, SpawnParticleEffect, SetState, GetCrossingBSSidePos, GetNewPersonIndex, the message and the message centre's post,
RecordEvent, PlaySound, InQueueingState, DismissFromQueue, LowerHappiness, GAME_ASSERTMSG, the virtual GetType) are fakes on both sides that
record their arguments and answer from the case's queues. __construct_array of the four thing ids is not logged (the loop after it rewrites
them); the one of the 29 subscription headers is logged as 29 events on both sides.

The world of a case: a person block (random bytes), gWorld (only mTurn is read), gWorldMap, gMainBalance (100 slots), the message
centre (null or not), the three halfword tables, gThingArray[0]'s first word and gSoundBanks. What is compared: the person block (decoded
by field, so the byte order of each side does not show), the message centre pointer (null or not), the event log.

No fused operations. Not tested: a balance spread or count of 0 (divw by 0), a divide of INT_MIN by -1.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_CPerson"
MASK = 0xFFFFFFFF

CTOR, RECV = 0x10116634, 0x10116E24
SIZES = {CTOR: 1488, RECV: 604}
OWN = set(SIZES)

ADDR = dict(world=0x1037EC0C, wmap=0x1037EC08, mc=0x1037F6C4, bal=0x102B4568, tbl=0x102A0D54, cella=0x102A0F14, cellb=0x102A0F16,
            tarr=0x1034CBFC, snd=0x102F7C94)
HOST_LAYOUT = [0x1D0, 8, 0x1DA70C, 12, 0x0C]
PERSON = 0x240
SELF = [0]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def off(e, a):
    return s32(a - SELF[0])


def reg(name, addr, fn):
    STUBS.internal(addr, fn)


def st_cpeep(e):
    rec(e, "CPeep", (e.r(4) >> 16) & 0xFFFF, s32(e.r(5)))


def st_thingid(e):
    rec(e, "GetThingId")
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetThingId") & 0xFFFF))


def st_getrand(e):
    rec(e, "GetRand")
    e.set_r(3, e.cl.ret("GetRand") & MASK)


def st_seed(e):
    rec(e, "SeedRand", e.r(4) & MASK)


def st_newperson(e):
    rec(e, "GetNewPersonIndex")
    e.set_r(3, e.cl.ret("GetNewPersonIndex") & MASK)


def st_esp(e):
    rec(e, "ESP_GetRandomSpritesFrom", s32(e.r(3)))
    e.set_r(3, e.cl.ret("ESP_GetRandomSpritesFrom") & MASK)


def st_spawn(e):
    rec(e, "SpawnSpriteScript", off(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))


def st_setscript(e):
    rec(e, "SetSpriteScript", off(e, e.r(3)), s32(e.r(4)))


def st_inside(e):
    rec(e, "IsInsidePark")
    e.set_r(3, e.cl.ret("IsInsidePark") & MASK)


def st_setdest(e):
    r4, r5 = e.r(4), e.r(5)
    rec(e, "SetDest", (r4 >> 24) & 0xFF, (r4 >> 16) & 0xFF, (r5 >> 24) & 0xFF, (r5 >> 16) & 0xFF)
    e.set_r(3, e.cl.ret("SetDest") & MASK)


def st_updatepos(e):
    rec(e, "UpdatePositionInEngine", fb(e.f(1)))


def st_particle(e):
    rec(e, "SpawnParticleEffect", s32(e.r(4)))


def st_playsound(e):
    rec(e, "PlaySound", s32(e.r(4)), s32(e.r(5)))


def st_setstate(e):
    rec(e, "SetState", s32(e.r(4)))


def st_inqueue(e):
    rec(e, "InQueueingState")
    e.set_r(3, e.cl.ret("InQueueingState") & MASK)


def st_dismiss(e):
    rec(e, "DismissFromQueue")


def st_lower(e):
    rec(e, "LowerHappiness", s32(e.r(4)))


def st_crossing(e):
    L = e.cl
    rec(e, "GetCrossingBSSidePos", L.lab(e.r(4)), s32(e.r(5)))
    e.write(e.r(3), struct.pack(">H", L.ret("GetCrossingBSSidePos") & 0xFFFF))


def st_record(e):
    rec(e, "RecordEvent", off(e, e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF)


def st_arrived(e):
    rec(e, "CMsgPeepArrived", (e.r(4) >> 16) & 0xFFFF)


def st_post(e):
    rec(e, "PostMessage", e.cl.lab(e.r(3)))


def st_construct_array(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the TOC where the caller reloads it from
    if e.r(6) == 16:                       # the 29 subscription headers of the message centre
        for _ in range(e.r(7)):
            rec(e, "CSubscriptionHeader")


def st_assert(e):
    text = e.cstr(e.r(4))
    h = 0
    for b in text.encode("latin-1") if isinstance(text, str) else bytes(text):
        h = (h * 31 + b) & MASK
    args = [s32(e.r(i)) for i in (5, 6, 7, 8)] if "%" in (text if isinstance(text, str) else text.decode("latin-1")) else [0, 0, 0, 0]
    rec(e, "GAME_ASSERTMSG", e.r(3) & 0xFF, h, *args)


def st_ptr_glue(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    e.set_r(3, e.u32(e.r(3) + 0x0C))


for a, f in ((0x10112E18, st_cpeep), (0x1012E1F8, st_thingid), (0x1013B1E4, st_getrand), (0x1013B244, st_seed),
             (0x10140E64, st_newperson), (0x1007C900, st_esp), (0x100FAB9C, st_spawn), (0x100FAA74, st_setscript),
             (0x10115A80, st_inside), (0x101154F0, st_setdest), (0x101149CC, st_updatepos), (0x101152A4, st_particle),
             (0x10115B80, st_playsound), (0x10120354, st_setstate), (0x10120B2C, st_inqueue), (0x1011F1B8, st_dismiss),
             (0x1011B384, st_lower), (0x10101C0C, st_crossing), (0x1012F3D4, st_record), (0x101521E4, st_arrived),
             (0x1015256C, st_post), (0x10238758, st_construct_array), (0x1000DDE8, st_assert), (0x1023BAA8, st_ptr_glue)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):                    # a pointer global: 4 bytes on the Mac, 8 on the host
        if self.fmt == ">I":
            self.words(a, [v])
        else:
            self.write(a, struct.pack("<Q", v & 0xFFFFFFFFFFFFFFFF))

    def rd_ptr(self, a):
        return self.rd_word(a) if self.fmt == ">I" else struct.unpack("<Q", self.read(a, 8))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.this = e.alloc(PERSON)
        self.map = e.alloc(16)
        self.centre = e.alloc(16)
        self.blk = e.alloc(16)
        self.msg = e.alloc(32)
        self.vt = e.alloc(16)
        SELF[0] = self.this
        self.L = e.cl = CL.PpcLog({self.map: 1, self.centre: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def balance(self, vals):                # the Mac's slot i is at gMainBalance + 8 + 4 * i
        self.words(ADDR["bal"] + 8, vals)

    def ctor(self, cell):
        T.call_ppc(self.e, CTOR, [self.this, (cell << 16) & MASK])

    def recv(self, mid, mtype):
        self.words(self.vt, [0, 0, 0, 0])
        self.words(self.msg, [self.vt, 0, (mid & 0xFFFF) << 16, mtype])
        T.call_ppc(self.e, RECV, [self.this, self.msg])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.this = lib.tpw_pers_alloc(PERSON)
        self.map = lib.tpw_pers_alloc(16)
        self.centre = lib.tpw_pers_alloc(16)
        self.blk = lib.tpw_pers_alloc(16)
        CL.host_begin(lib, {1: self.map, 2: self.centre}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pers_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def balance(self, vals):                # the host's BalanceValue is a union of 8 bytes
        for i, v in enumerate(vals):
            self.words(self.g("bal") + 8 * i, [v, 0])

    def ctor(self, cell):
        self.lib.tpw_pers_ctor(self.this, cell)

    def recv(self, mid, mtype):
        self.lib.tpw_pers_recv(self.this, mid, mtype)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pers_addr.restype, lib.tpw_pers_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pers_alloc.restype, lib.tpw_pers_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pers_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pers_ctor.restype, lib.tpw_pers_ctor.argtypes = None, [C.c_uint64, C.c_uint32]
    lib.tpw_pers_recv.restype, lib.tpw_pers_recv.argtypes = None, [C.c_uint64, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pers_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

# the person's halfword and byte fields (everything else is a word)
SPANS = {0xC2: 2, 0x1C0: 1, 0x1DC: 2, 0x1DE: 2, 0x1F0: 1, 0x1F1: 1, 0x228: 2, 0x22A: 2}
for _o in range(0x1E0, 0x1F0, 2):
    SPANS[_o] = 2
for _o in (0xC, 0x20, 0x24, 0x28, 0x1BC, 0x1C4, 0x1C8, 0x1CC, 0x1D0, 0x1D4, 0x1D8, 0x1F4, 0x1F8, 0x1FC, 0x200, 0x204, 0x208, 0x20C,
           0x210, 0x214, 0x218, 0x21C, 0x220, 0x224) + tuple(range(0x19C, 0x1BC, 4)):
    SPANS[_o] = 4


def dump(s):
    """The person block decoded field by field (the same values on either byte order); the bytes between fields as they are."""
    raw = s.read(s.this, PERSON)
    order = "big" if s.fmt[0] == ">" else "little"
    out = []
    p = 0
    while p < PERSON:
        n = SPANS.get(p, 0)
        if n:
            out.append(("f", int.from_bytes(raw[p:p + n], order)))
            p += n
        else:
            out.append(("b", raw[p]))
            p += 1
    return out


def build(s, case):
    s.write(s.this, case["fill"])
    for o, n in SPANS.items():              # the fields start from the fill's bytes read as a number, stored in this side's byte order
        v = int.from_bytes(case["fill"][o:o + n], "big")
        {1: s.byte, 2: s.half}.get(n, lambda a, x: s.words(a, [x]))(s.this + o, v)
    for o, (n, v) in case["fields"].items():
        if n == 1:
            s.byte(s.this + o, v)
        elif n == 2:
            s.half(s.this + o, v)
        else:
            s.words(s.this + o, [v])
    s.words(s.blk, [case["turn"]])
    s.ptr(s.g("world"), s.blk - 0x1DA70C)
    s.ptr(s.g("wmap"), s.map)
    s.ptr(s.g("mc"), 0 if case["mc_null"] else s.centre)
    s.balance(case["bal"])
    s.half(s.g("tbl") + 2, case["tbl1"])
    s.half(s.g("cella"), case["cella"])
    s.half(s.g("cellb"), case["cellb"])
    s.words(s.g("tarr"), [case["thing0"]])
    s.words(s.g("snd") + 4, [case["snd1"]])


def execute(s, case):
    if case["op"] == "ctor":
        s.ctor(case["cell"])
    else:
        s.recv(case["id"], case["type"])


def observe(s, case):
    mc = s.rd_ptr(s.g("mc"))
    return {"log": s.log(), "person": dump(s), "mc_set": mc != 0, "mc_same": mc == s.centre}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

RAND_EDGES = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 49, 50, 99, 100, 29, 30]


def rr(r):
    return r.choice(RAND_EDGES) if r.random() < 0.2 else r.getrandbits(32)


def ru16(r):
    return r.choice([0, 1, 2, 3, 0x7F, 0x80, 0x81, 0xFFFF]) if r.random() < 0.3 else r.getrandbits(16)


def base(r, op):
    bal = [r.randrange(-1000, 5000) & MASK for _ in range(100)]
    c = {"op": op, "fill": bytes(r.getrandbits(8) for _ in range(PERSON)), "fields": {}, "turn": r.getrandbits(32), "mc_null": r.random() < 0.5,
         "bal": bal, "tbl1": ru16(r), "cella": ru16(r), "cellb": ru16(r), "thing0": r.choice([0, 0, 1, 0x1234]), "snd1": r.getrandbits(32)}
    return c


def ctor_case(r, **kw):
    c = base(r, "ctor")
    c["cell"] = ru16(r)
    bal = c["bal"]
    bal[1] = r.randrange(-100000, 100000) & MASK
    bal[2] = r.choice([1, 2, 3, 50, r.randrange(1, 60)])
    bal[3] = r.choice([0, 1, 2, 60, r.randrange(0, 60)])
    bal[99] = r.choice([1, 2, 3, 4, r.randrange(1, 5)])
    bal[35] = r.choice([0, 1, 50, 99, 100, 0xFFFFFFFF, r.getrandbits(32)])
    rands = [rr(r) for _ in range(10)]
    c["queues"] = {"GetRand": rands, "GetThingId": [ru16(r) for _ in range(3)], "ESP_GetRandomSpritesFrom": [r.randrange(-5, 50)],
                   "IsInsidePark": [r.choice([0, 1, 5])], "GetNewPersonIndex": [r.randrange(-5, 1000)],
                   "GetCrossingBSSidePos": [ru16(r)], "SetDest": [r.choice([0, 1, 7])]}
    c.update(kw)
    return c


def ctor_edges():
    r = random.Random(5052)
    for inside in (0, 1):
        for sd in (0, 1):
            for mcn in (False, True):
                for flag in (0, 1, 2):
                    c = ctor_case(r)
                    c["queues"]["IsInsidePark"] = [inside]
                    c["queues"]["SetDest"] = [sd]
                    c["mc_null"] = mcn
                    pick = c["queues"]["GetRand"][7] % 100
                    c["bal"][35] = [0, 0xFFFFFFFF, pick][flag] if flag < 2 else pick + 1
                    yield c
    for cell in (0, 1, 0x80, 0x81, 0xFFFF):
        c = ctor_case(r)
        c["cell"] = cell
        c["queues"]["IsInsidePark"] = [0]
        c["queues"]["GetCrossingBSSidePos"] = [cell]
        yield c
    for rands in ([0] * 10, [0xFFFFFFFF] * 10, [0x80000000] * 10, [0x7FFFFFFF] * 10):
        c = ctor_case(r)
        c["queues"]["GetRand"] = rands
        yield c
    for spread, rng, cnt in ((1, 0, 1), (50, 60, 4), (2, 0, 3)):
        c = ctor_case(r)
        c["bal"][2], c["bal"][3], c["bal"][99] = spread, rng, cnt
        yield c


def ctor_random(r):
    return ctor_case(r)


def recv_case(r):
    c = base(r, "recv")
    mid = ru16(r)
    c["id"] = mid
    c["type"] = 10 if r.random() < 0.8 else r.choice([0, 9, 11, -1, 0x7FFFFFFF])

    def pick():
        return mid if r.random() < 0.4 else ru16(r)

    f = c["fields"]
    f[0x1DC] = (2, pick())
    f[0x1DE] = (2, pick())
    for i in range(4):
        f[0x1E0 + 2 * i] = (2, pick())
        f[0x1E8 + 2 * i] = (2, ru16(r))
    f[0x220] = (4, r.choice([0x10, 0x10, 0, 6, 0x11, r.getrandbits(32)]))
    f[0xC] = (4, r.choice([0, 0, 1, 0x1000]))
    f[0x24] = (4, r.getrandbits(32))
    f[0x20] = (4, r.getrandbits(32))
    f[0x28] = (4, r.getrandbits(32))
    f[0x228] = (2, ru16(r))
    f[0x22A] = (2, ru16(r))
    if r.random() < 0.5:
        c["cella"] = f[0x228][1] if r.random() < 0.7 else ru16(r)
        c["cellb"] = f[0x22A][1] if r.random() < 0.7 else ru16(r)
    c["queues"] = {"InQueueingState": [r.choice([0, 1, 3])], "GetThingId": [ru16(r)]}
    return c


def recv_edges():
    r = random.Random(5053)
    for typ in (10, 9, 11, 0):
        for state in (0x10, 0, 6):
            for scr in (0, 1):
                for t0 in (0, 1):
                    for ok in (0, 1):
                        c = recv_case(r)
                        c["type"] = typ
                        f = c["fields"]
                        f[0x1DC] = (2, c["id"])
                        f[0x220] = (4, state)
                        f[0xC] = (4, scr)
                        c["thing0"] = t0
                        f[0x228] = (2, 5)
                        f[0x22A] = (2, 6)
                        c["cella"], c["cellb"] = (5, 6) if ok else (5, 7)
                        yield c
    for mid in (0, 0xFFFF):
        c = recv_case(r)
        c["id"] = mid
        c["type"] = 10
        for o in (0x1DC, 0x1DE, 0x1E0, 0x1E2, 0x1E4, 0x1E6):
            c["fields"][o] = (2, mid)
        yield c


def recv_random(r):
    return recv_case(r)


TESTS = [
    FnTest("CPerson::CPerson", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("CPerson::ReceiveMessage", ppc, host, edge=recv_edges, rnd=recv_random),
]
