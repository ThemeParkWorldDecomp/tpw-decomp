"""Unit v5922, SetControlType, against libtpw_unit_SetControlType_2.dylib (tests/unit_SetControlType_2.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  SetControlType 0x1002d404

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (CWorld::GetRand, FindDataNodeIndexByID, CRideDatabase::GetRideInfo, InstanceRide, TrackRideIdToThingId,
GAME_ASSERTMSG, SND_SendIndirectedEvent, SOUND_MoveSoundEffect, FindTargetBump) are fakes on both sides that record their arguments and
answer from the case's queues; labs and sqrt are the C library on both sides.

The world of a case: the bump list at 0x102b56c0 (magic, rides, bumps, table), a ride record, four bump slots, four ride nodes, a
table, the sine table, the sound banks, the anim set pointers, the thing array rows 0 to 3 and the advisor's thing id. Pointer words
inside the list, rides and bumps are Ptr32 (arena offsets on the host, addresses on the Mac). What is compared: the event log and the
four bump slots after the call (the pointer word 0x9c as an offset from the rides block).

No fused operations (the float divide is exact in the emulator's double-then-single rounding). Not tested: a bump or ride kind-1 walk
over nodes without the flag (an endless loop in the original), a NULL thing for a non-advisor id (a NULL read).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetControlType_2"
MASK = 0xFFFFFFFF

FN = 0x1002D404

ADDR = dict(list=0x102B56C0, world=0x1037EBFC, sine=0x102B4EC0, banks=0x102F7C94, sets=0x102C79A4, things=0x1034CBFC,
            advisor=0x1029C960)
STRIDE_PPC = 0x14
HOST_LAYOUT_TAIL = [8, 0xA8, 0xD0]   # the host's tagBUMP is 0xa8 (the slot is 0xac apart; read by offset)
WORLD = 0x7000
SETBASE = 0x1000


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)
INFO = {}


def u(v):
    return v & MASK


def stub_getrand(e):
    L = e.cl
    L.rec("GetRand", (u(e.r(3)),))
    e.set_r(3, L.ret("GetRand") & MASK)


def stub_find(e):
    L = e.cl
    L.rec("FindDataNodeIndexByID", (u(e.r(3)), u(e.r(4)), u(e.r(5))))
    e.set_r(3, L.ret("FindDataNodeIndexByID") & MASK)


def stub_rideinfo(e):
    L = e.cl
    L.rec("GetRideInfo", ((e.r(4) >> 16) & 0xFFFF,))
    e.write(INFO["blk"] + 0x7B8, struct.pack(">I", L.ret("GetRideInfo") & MASK))
    e.set_r(3, INFO["blk"])


def stub_instance(e):
    L = e.cl
    sp = e.r(1)
    L.rec("InstanceRide", tuple(u(e.r(i)) for i in range(3, 11)) + (e.u32(sp + 0x38), e.u32(sp + 0x3C)))
    e.set_r(3, L.ret("InstanceRide") & MASK)


def stub_track(e):
    L = e.cl
    L.rec("TrackRideIdToThingId", (u(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("TrackRideIdToThingId") & 0xFFFF))


def stub_snd(e):
    L = e.cl
    L.rec("SND_SendIndirectedEvent", tuple(u(e.r(i)) for i in range(3, 10)))
    e.set_r(3, L.ret("SND_SendIndirectedEvent") & MASK)


def stub_move(e):
    L = e.cl
    L.rec("SOUND_MoveSoundEffect", tuple(u(e.r(i)) for i in range(3, 7)))
    e.set_r(3, L.ret("SOUND_MoveSoundEffect") & MASK)


def stub_target(e):
    L = e.cl
    lst = ADDR["list"]
    L.rec("FindTargetBump", (u(e.r(3) - e.u32(lst + 0x10)), u(e.r(4) - e.u32(lst + 8))))
    if L.ret("FindTargetBump") == 0:
        e.set_r(3, 0)
    else:
        e.write(e.r(5), struct.pack(">I", L.ret("FindTargetBumpIndex") & MASK))


def labs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v) & MASK if v < 0 else v & MASK)


STUBS.internal(0x1013B1E4, stub_getrand)
STUBS.internal(0x101FFDC8, stub_find)
STUBS.internal(0x10154868, stub_rideinfo)
STUBS.internal(0x10069B24, stub_instance)
STUBS.internal(0x10030020, stub_track)
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "bs"))
STUBS.internal(0x100DF858, stub_snd)
STUBS.internal(0x100DDF60, stub_move)
STUBS.internal(0x1002DF10, stub_target)
STUBS.imp("labs", labs)


def setup(runner):
    T.install_std_stubs(runner.emu)
    INFO["blk"] = T.persist_alloc(runner.emu, 0x1000)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


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

    def off(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def put_ptrs(self, name, vals):
        self.words(self.g(name), vals)

    def put_thing(self, i, p):
        self.words(self.g("things") + STRIDE_PPC * i, [p])

    def put_u16(self, name, v):
        self.write(self.g(name), struct.pack(">H", v & 0xFFFF))

    def call(self, ride, bump):
        T.call_ppc(self.e, FN, [ride, bump])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_sct_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sct_addr(name.encode())

    def off(self, a):
        return (a - self.lib.tpw_sct_base()) & MASK     # the base is only fixed once something is allocated

    def alloc(self, n):
        return self.lib.tpw_sct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptrs(self, name, vals):
        self.write(self.g(name), b"".join(struct.pack("<Q", v) for v in vals))

    def put_thing(self, i, p):
        self.write(self.g("things") + self.lib.stride * i, struct.pack("<Q", p))

    def put_u16(self, name, v):
        self.write(self.g(name), struct.pack("<H", v & 0xFFFF))

    def call(self, ride, bump):
        self.lib.tpw_sct_run(ride, bump)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_sct_addr").restype, getattr(lib, "tpw_sct_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_sct_alloc").restype, getattr(lib, "tpw_sct_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sct_base.restype = C.c_uint64
    lib.tpw_sct_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sct_run.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sct_layout(out)
    assert list(out)[1:n] == HOST_LAYOUT_TAIL, list(out)[:n]
    lib.stride = out[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

SINE = [int(256 * math.sin(math.pi * 2 * i / 512)) & MASK for i in range(512)]


def build(s, case):
    # the sine table, banks, sets, world, advisor id
    s.words(s.g("sine"), SINE)
    s.words(s.g("banks"), case["banks"])
    s.put_ptrs("sets", [SETBASE + 0x10 * i for i in range(8)])
    s.words(s.g("world"), [WORLD]) if isinstance(s, Ppc) else s.write(s.g("world"), struct.pack("<Q", WORLD))
    s.put_u16("advisor", case["advisor"])
    for i in range(4):
        if i == 0 and case["thing0_null"]:
            s.put_thing(0, 0)
            continue
        t = s.alloc(0x40)
        s.words(t + 0x20, [case["thingval"][i]])
        s.put_thing(i, t)

    rides = s.alloc(0xD0 * 3)
    bumps = s.alloc(0xAC * 4)
    nodes = s.alloc(0x20 * 4)
    table = s.alloc(0x1C * 8)
    cnt = s.alloc(0x20)
    s.words(cnt + 0xC, [case["cnt"]])
    for k in range(4):
        n = nodes + 0x20 * k
        nx = case["next"][k]
        s.words(n, [case["nflags"][k]])
        s.words(n + 4, [case["nval"][k]])
        s.words(n + 0x14, [s.off(nodes + 0x20 * nx) if nx >= 0 else 0])
    for j in range(8):
        t = table + 0x1C * j
        s.words(t + 4, [case["tval"][j]])
        nx = case["tnext"][j]
        s.words(t + 0x14, [s.off(nodes + 0x20 * nx) if nx >= 0 else 0])

    ride = rides + 0xD0 * case["ridx"]
    for off, v in case["ride"].items():
        s.words(ride + off, [v])
    s.words(ride + 0xA4, [s.off(nodes + 0x20 * case["n_a4"])])
    s.words(ride + 0xA8, [s.off(nodes + 0x20 * case["n_a8"])])
    s.words(ride + 0xC0, [s.off(cnt)])
    for r in range(3):
        if r != case["ridx"]:
            s.words(rides + 0xD0 * r, [r + 5])
    for b in range(4):
        for off, v in case["bumps"][b].items():
            s.words(bumps + 0xAC * b + off, [v])
        owner = rides + 0xD0 * (case["ridx"] if b == case["bidx"] else case["owner"][b])
        s.words(bumps + 0xAC * b + 0x9C, [s.off(owner)])

    lst = s.g("list")
    s.words(lst, [case["magic"]])
    s.words(lst + 8, [s.off(rides)])
    s.words(lst + 0x10, [s.off(bumps)])
    s.words(lst + 0x14, [case["nbump"]])
    s.words(lst + 0x20, [s.off(table)])
    s.words(lst + 0x24, [case["ntable"]])
    return rides, bumps, ride


def run(s, case):
    try:
        rides, bumps, ride = build(s, case)
        bump = bumps + 0xAC * case["bidx"]
        s.call(ride, bump)
        out = []
        for b in range(4):
            w = s.rd_words(bumps + 0xAC * b, 0xAC // 4)
            w[0x9C // 4] = (w[0x9C // 4] - s.off(rides)) & MASK
            out.append(w)
        return {"log": s.log(), "bumps": out}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

FLAG_BITS = [1, 2, 4, 8, 0x20, 0x40, 0x2000, 0x4000, 0x8000, 0x10000, 0x40000, 0x100000, 0x400000, 0x3000000]


def pos(r):
    k = r.random()
    if k < 0.7:
        return r.randrange(-40, 41)
    if k < 0.95:
        return r.randrange(-100000, 100000)
    return r.choice([0x7FFFFFFF, -0x80000000, 0x3FFFFFFF, -0x40000000])


def base(r, kind=None):
    if kind is None:
        kind = r.choice([-14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, -15, 0, 1, r.randrange(-20, 5)])
    ridx = r.randrange(3)
    flags = 0
    for b in FLAG_BITS:
        if r.random() < 0.5:
            flags |= b
    if r.random() < 0.2:
        flags |= r.getrandbits(32)
    bumps = []
    for b in range(4):
        bw = {0: flags if b == 0 else (r.getrandbits(32) | (1 if r.random() < 0.8 else 0)),
              8: r.randrange(4), 0x20: r.choice([0, 0, r.randrange(1, 1000)]), 0x34: r.randrange(-40, 41) if b else pos(r),
              0x38: r.randrange(-40, 41) if b else pos(r), 0x64: r.randrange(0, 60), 0x7C: r.randrange(-2, 11)}
        bumps.append(bw)
    state = r.choice([0, 1, 2, 3])
    ride = {0: kind & MASK, 0x10: r.getrandbits(32), 0x20: r.choice([r.randrange(-100, 100), r.randrange(-2**31, 2**31)]) & MASK,
            0x50: state, 0x5C: r.randrange(0, 5), 0x74: r.randrange(-30, 31), 0x78: r.randrange(-30, 31),
            0xAC: r.choice([1, 2, 3, 5, 9])}   # 0 would loop 2^32 times (divwu by 0)
    nflags = [r.choice([0, 2, 3, 1]) for _ in range(4)]
    nxt = [r.choice([-1, 0, 1, 2, 3]) for _ in range(4)]
    if state == 1:
        nflags = [2 | r.randrange(2) for _ in range(4)]
        nxt = [(k + 1) % 4 for k in range(4)]
    tid = r.choice([1, 2, 3])
    q = {"GetRand": [r.getrandbits(32) for _ in range(60)],   # the host side keeps 64 values of a queue
         "FindDataNodeIndexByID": [r.choice([0, 1, 5, MASK]) for _ in range(3)],
         "GetRideInfo": [r.getrandbits(32)], "InstanceRide": [r.randrange(0, 20)],
         "TrackRideIdToThingId": [tid], "SND_SendIndirectedEvent": [r.choice([0, r.randrange(1, 5000)])],
         "SOUND_MoveSoundEffect": [r.choice([0, r.randrange(1, 5000)])], "FindTargetBump": [r.choice([0, 1])],
         "FindTargetBumpIndex": [r.randrange(-3, 9)]}
    ntable = r.randrange(0, 9)
    for b in bumps:
        if kind in (-14, -13) or True:
            pass
    return {"queues": q, "magic": r.choice([0x4A454647, 0x4A454647, 0x4A454647, r.getrandbits(32)]), "ridx": ridx,
            "bidx": r.randrange(4), "owner": [r.randrange(3) for _ in range(4)], "ride": ride, "bumps": bumps,
            "cnt": r.choice([0, 1, 5, 20, 40]), "next": nxt, "nflags": nflags, "nval": [r.randrange(-5, 12) for _ in range(4)],
            "tval": [r.randrange(-5, 12) for _ in range(8)], "tnext": [r.choice([-1, 0, 1, 2, 3]) for _ in range(8)],
            "n_a4": r.randrange(4), "n_a8": r.randrange(4), "nbump": r.randrange(0, 5), "ntable": ntable,
            "banks": [r.getrandbits(32) for _ in range(8)], "advisor": r.randrange(4), "thing0_null": r.random() < 0.3,
            "thingval": [r.getrandbits(32) for _ in range(4)]}


def fix(c):
    """Keep the reads inside the blocks: a bump 0x7c used as a table index (flag 8) is 0 to 7."""
    for b in c["bumps"]:
        if b[0] & 8:
            b[0x7C] = b[0x7C] & 7
    # the walk counts only live slots: the count must not pass the live ones (it would read past the four slots)
    c["nbump"] = min(c["nbump"], sum(1 for b in c["bumps"] if b[0] & 1))
    return c


def case(r, kind=None):
    c = base(r, kind)
    if c["queues"]["TrackRideIdToThingId"][0] == 0:
        c["queues"]["TrackRideIdToThingId"] = [1]
    return fix(c)


def edges():
    r = random.Random(4242)
    for kind in range(-16, 3):
        for _ in range(8):
            yield case(r, kind)
    # bump 20 with collisions, sound off/on, advisor id hit, no magic
    for _ in range(40):
        c = case(r, r.choice([-14, -13, -12, -11, -10]))
        c["bumps"][c["bidx"]][0] |= 0x100000
        c["magic"] = 0x4A454647
        c["nbump"] = 4
        c["cnt"] = r.choice([20, 40])
        for b in range(4):
            c["bumps"][b][0] |= 1
            c["bumps"][b][0x64] = r.randrange(0, 80)
        yield fix(c)
    for _ in range(40):
        c = case(r, r.choice([-14, -13, -12, -11, -10, -9, -8]))
        c["bumps"][c["bidx"]][0] |= 0x100000
        c["bumps"][c["bidx"]][0x20] = 0
        c["advisor"] = c["queues"]["TrackRideIdToThingId"][0] if r.random() < 0.3 else c["advisor"]
        yield c


def rnd(r):
    return case(r)


TESTS = [
    FnTest("SetControlType", ppc, host, edge=edges, rnd=rnd),
]
