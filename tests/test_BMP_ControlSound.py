"""Unit v5966: the bump sound control and three ride record functions, against libtpw_unit_BMP_ControlSound.dylib
(tests/unit_BMP_ControlSound.cpp), main app, src/game/rides/BumperRide.cpp:
  BMP_ControlSound 0x10028e30, FreeBuoysForRide 0x10027494, BMP_GetCarsOnRide 0x10026e74, BMP_SetRideModelSelection 0x10026be4

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, host library
memory). The callees outside the unit (SingleSoundAtBump, TRACE, MineToSystemCords, TrackRideIdToThingId, GAME_ASSERTMSG,
SND_SendIndirectedEvent) are fakes on both sides that record their arguments and answer from the case.

The world of a case: the list at 0x102b56c0 (magic, ride records, count, random seed), a ride record block, a bump or a buoy list, the thing
array rows, the advisor thing id and the sound banks. Compared: the answer, the bump words, the world words, the ride record, the buoy
list, the event log. No fused operations. Not tested: BMP_GetCarsOnRide with a dead world and the id 0 (the original reads address 0x5c).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_ControlSound"
MASK = 0xFFFFFFFF

FN_SOUND, FN_FREE, FN_CARS, FN_SET = 0x10028E30, 0x10027494, 0x10026E74, 0x10026BE4
SIZES = {FN_SOUND: 712, FN_FREE: 160, FN_CARS: 132, FN_SET: 128}
OWN = set(SIZES)

SINGLE, TRACE, MINE, TRACKID, ASSERTMSG, SENDEV = 0x1002B180, 0x1006FCFC, 0x1002B0DC, 0x10030020, 0x1000DDE8, 0x100DF858

ADDR = dict(world=0x102B56C0, banks=0x102F7C94, thingarr=0x1034CBFC, adv=0x1029C960)
HOST_LAYOUT = [0x40, 2]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_single(e):
    e.cl.rec("SingleSoundAtBump", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_mine(e):
    L = e.cl
    L.rec("MineToSystemCords", (s32(e.r(3)), s32(e.r(4))))
    x, y = L.ret("pt_x") & MASK, L.ret("pt_y") & MASK
    e.write(e.r(5), struct.pack(">II", x, y))


def stub_track(e):
    L = e.cl
    L.rec("TrackRideIdToThingId", (s32(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("tid") & 0xFFFF))


def stub_send(e):
    e.cl.rec("SND_SendIndirectedEvent", tuple(s32(e.r(i)) for i in range(3, 10)))
    e.set_r(3, 0)


STUBS.internal(SINGLE, stub_single)
STUBS.internal(TRACE, CL.recorder("TRACE", "s"))
STUBS.internal(MINE, stub_mine)
STUBS.internal(TRACKID, stub_track)
STUBS.internal(ASSERTMSG, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(SENDEV, stub_send)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.begin({})
        self.saved = STUBS.activate(e)

    def begin(self, labels):
        self.L = self.e.cl = CL.PpcLog(labels, self.case["queues"])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def set_thing(self, idx, thing):
        self.words(ADDR["thingarr"] + idx * 0x14, [thing])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if entry == FN_CARS else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.begin({})

    def begin(self, labels):
        CL.host_begin(self.lib, {v: k for k, v in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_cs_off(a)

    def set_thing(self, idx, thing):
        self.lib.tpw_cs_setthing(idx, thing)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        a = [x & MASK for x in args]
        if entry == FN_SOUND:
            self.lib.tpw_cs_sound(args[0])
            return 0
        if entry == FN_FREE:
            self.lib.tpw_cs_free(args[0], s32(a[1]))
            return 0
        if entry == FN_CARS:
            return self.lib.tpw_cs_cars(s32(a[0])) & MASK
        self.lib.tpw_cs_set(s32(a[0]), s32(a[1]), s32(a[2]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_addr.restype, lib.tpw_cs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_off.restype, lib.tpw_cs_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_cs_setthing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cs_sound.argtypes = [C.c_uint64]
    lib.tpw_cs_free.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_cs_cars.restype, lib.tpw_cs_cars.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_cs_set.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


MAGIC = 0x4A454647


def world_setup(s, case):
    """The list: magic, ride records (256 of 0xd0), count at 0x24, seed at 0x38. Answers the world address, the records."""
    w = s.g("world")
    rides = s.alloc(0xD0 * 256)
    s.words(w, [0] * 16)
    s.words(w, [case["magic"]])
    s.words(w + 8, [s.ptr(rides)])
    s.words(w + 0x24, [case["cnt"]])
    s.words(w + 0x38, [case["seed"]])
    return w, rides


def build(s, case):
    w, rides = world_setup(s, case)
    s.words(s.g("banks") + 0x1C, [case["bank"]])
    idx = case["idx"]
    rec = rides + idx * 0xD0
    s.words(rec, [case["kind"]])
    s.words(rec + 0x14, [0x11111111, 0x22222222])
    s.words(rec + 0x5C, [case["cars"]])
    case["w"], case["rec"] = w, rec
    if case["op"] == "sound":
        bump = s.alloc(0xB0)
        s.begin({bump: 1})
        s.words(bump, [case["flags"]])
        s.words(bump + 0x14, [case["pend"], case["state"]])
        s.words(bump + 0x34, [case["bx"], case["by"]])
        s.words(bump + 0x9C, [s.ptr(rec)])
        s.half(s.g("adv"), case["adv"])
        t = s.alloc(0x40)
        s.words(t + 0x20, [case["tw"]])
        s.set_thing(case["tid"], t)
        s.set_thing(0, t if case["t0"] else 0)
        if case["tid"] == 0:
            s.set_thing(0, t)
        case["bump"] = bump
    elif case["op"] == "free":
        k = len(case["flags_list"])
        block = s.alloc(0x20 * k + 0x20)
        nodes = [block + 0x20 * i for i in range(k)]
        order = case["order"]
        ride = s.alloc(0xD0)
        for pos, i in enumerate(order):
            nxt = s.ptr(nodes[order[pos + 1]]) if pos + 1 < k else 0
            s.words(nodes[i], [case["flags_list"][i]])
            s.words(nodes[i] + 0x14, [nxt])
        s.words(ride + 0xA8, [s.ptr(nodes[order[0]]) if k else 0, case["cnt_ride"]])
        case["nodes"], case["ride"] = nodes, ride


def execute(s, case):
    op = case["op"]
    if op == "sound":
        return s.call(FN_SOUND, [case["bump"]])
    if op == "free":
        return s.call(FN_FREE, [case["ride"], case["mask"]])
    if op == "cars":
        return s.call(FN_CARS, [case["id"]])
    return s.call(FN_SET, [case["id"], case["a"], case["b"]])


def observe(s, case, answer):
    op = case["op"]
    w = s.rd_words(case["w"], 16)
    w[2] = 0
    out = {"answer": answer, "log": s.log(), "world": w}
    if op == "sound":
        b = s.rd_words(case["bump"], 44)
        b[39] = 0
        out["bump"] = b
    elif op == "free":
        nodes = case["nodes"]
        ptrmap = {s.ptr(a): i for i, a in enumerate(nodes)}
        out["nodes"] = [(s.rd_word(a), ptrmap.get(s.rd_word(a + 0x14), -1 if s.rd_word(a + 0x14) == 0 else -2)) for a in nodes]
        h = s.rd_word(case["ride"] + 0xA8)
        out["head"] = ptrmap.get(h, -1 if h == 0 else -2)
        out["rcount"] = s.rd_word(case["ride"] + 0xAC)
    else:
        out["rec"] = s.rd_words(case["rec"], 0xD0 // 4)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def lcg(seed):
    return (seed * 0x343FD + 0x269EC3) & MASK


def hits(seed):
    return ((lcg(seed) >> 16) & 7) == 0


def pick_seed(r, want):
    while True:
        sd = r.getrandbits(32)
        if hits(sd) == want:
            return sd


def base(r, op):
    c = {"op": op, "magic": MAGIC if r.random() < 0.85 else r.choice([0, 1, MAGIC ^ 1]), "cnt": r.randrange(0, 50),
         "seed": r.getrandbits(32), "bank": r.getrandbits(31), "idx": r.choice([0, 1, 7, 255, r.randrange(256)]),
         "kind": r.choice([0, 1, 0x1234, r.getrandbits(24)]), "cars": r.getrandbits(32)}
    c["queues"] = {"pt_x": [r.randrange(-1000, 1000)], "pt_y": [r.randrange(-1000, 1000)], "tid": [1]}
    return c


def sound_case(r, state=None, pend=None):
    c = base(r, "sound")
    c["state"] = r.choice([0, 1, 2, 3, 4, 5, 6, -1, 0x7FFFFFFF]) if state is None else state
    c["pend"] = r.choice([0, 0, 1, 2, 3, 4, 5, 6, 0x80000000]) if pend is None else pend
    eff = max(c["state"] & MASK, c["pend"]) if c["pend"] else c["state"] & MASK
    if c["pend"] and (c["state"] & MASK) >= c["pend"]:
        eff = c["state"] & MASK
    c["seed"] = pick_seed(r, r.random() < 0.5)
    c["adv"] = r.choice([0, 5, 0x2799, r.randrange(0x2800)])
    c["tid"] = r.choice([c["adv"], c["adv"], 1, 0, 0x27FF, r.randrange(0x2800)])
    c["queues"]["tid"] = [c["tid"]]
    c["t0"] = r.random() < 0.5
    c["tw"] = r.getrandbits(32)
    c["bx"], c["by"] = r.randrange(-100000, 100000), r.randrange(-100000, 100000)
    c["flags"] = r.getrandbits(32)
    return c


def sound_edges():
    r = random.Random(11)
    for st in (-1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF):
        for pend in (0, 3, 5):
            yield sound_case(r, st, pend)
    for st, pend in ((2, 1), (2, 2), (2, 3), (1, 1), (0, 0x80000000), (5, 4), (3, 4)):
        for _ in range(3):
            yield sound_case(r, st, pend)


def id_case(r, op):
    c = base(r, op)
    c["id"] = r.choice([c["idx"] | (c["kind"] << 8), c["idx"] | (c["kind"] << 8), r.getrandbits(32), r.randrange(256), c["idx"] | ((c["kind"] ^ 1) << 8)]) & MASK
    c["idx"] = c["id"] & 0xFF
    if r.random() < 0.7:
        c["kind"] = (c["id"] >> 8) & 0xFFFFFF
    if op == "cars":
        if c["magic"] != MAGIC and c["id"] == 0:
            c["id"] = 1
            c["idx"] = 1
    c["a"], c["b"] = r.getrandbits(32), r.getrandbits(32)
    return c


def cars_edges():
    r = random.Random(21)
    for _ in range(12):
        yield id_case(r, "cars")
    c = id_case(r, "cars")
    c["magic"] = MAGIC
    c["id"], c["idx"], c["kind"] = 0, 0, 0
    yield c


def set_edges():
    r = random.Random(31)
    for _ in range(12):
        yield id_case(r, "set")


def free_case(r, mask=None):
    c = base(r, "free")
    k = r.choice([0, 1, 2, 3, 5, 8])
    c["flags_list"] = [r.getrandbits(32) | r.choice([1, 1, 0]) for _ in range(k)]
    c["order"] = list(range(k))
    r.shuffle(c["order"])
    c["mask"] = r.choice([0, 1, 2, 4, 6, 0x80000000, -1, r.getrandbits(32)]) if mask is None else mask
    c["cnt_ride"] = r.randrange(0, 20)
    return c


def free_edges():
    r = random.Random(41)
    for m in (0, 1, 2, 6, -1, 0x10, 0x80000000):
        for _ in range(4):
            yield free_case(r, m)


TESTS = [
    FnTest("BMP_ControlSound", ppc, host, edge=sound_edges, rnd=lambda r: sound_case(r)),
    FnTest("FreeBuoysForRide", ppc, host, edge=free_edges, rnd=lambda r: free_case(r)),
    FnTest("BMP_GetCarsOnRide", ppc, host, edge=cars_edges, rnd=lambda r: id_case(r, "cars")),
    FnTest("BMP_SetRideModelSelection", ppc, host, edge=set_edges, rnd=lambda r: id_case(r, "set")),
]
