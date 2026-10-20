"""Unit v5954, the person's walk through the ticket booth, against libtpw_unit_CPerson_LeaveQueue.dylib (tests/unit_CPerson_LeaveQueue.cpp),
main app ("SimTheme Park"), src/CPerson_..._CPerson_LeaveQueue.cpp:
  CPerson::LeaveQueue 0x1011dbf4, ProcessStatePayingAtBooth 0x1011cce4, ProcessStateWaitingForParkToOpen 0x1011c9f8,
  ProcessStateWalkingToTicketBooths 0x1011c88c, FeelingAboutEntryFee 0x1011c63c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the world, the map, the
park analyser, the bank account, the thoughts, CPeep::SetDest and PeepWalks, SetState, DismissFromQueue, the happiness changes, CTRACE and
GAME_ASSERTMSG) are fakes on both sides that record their arguments and answer from the case's queues. FeelingAboutEntryFee runs as the original
inside ProcessStatePayingAtBooth.

The world of a case: a person block (id, cell bytes, ride, and the words the functions touch), a world block (the bank account at +0x100 with its
balance word at +0x118, the park analyser at +0x300), a map block (the who halfword of the person's cell), the thing array entries, gMainBalance
values 13 to 17 and 33 to 36, the person sprite table. What is compared: the answer, the touched person fields and the event log.

No fused operations (fsubs and fmuls only). Not tested: a person cell past x 7 / y 7 (the map block is small), divisor 0 on hardware (it is
tested against the emulator, which answers the dividend; the translation does the same).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_LeaveQueue"
MASK = 0xFFFFFFFF

LEAVE, PAY, WAIT, WALK, FEEL = 0x1011dbf4, 0x1011cce4, 0x1011c9f8, 0x1011c88c, 0x1011c63c
SIZES = {LEAVE: 160, PAY: 508, WAIT: 684, WALK: 300, FEEL: 540}
OWN = set(SIZES)

ADDR = dict(world=0x1037ec0c, map=0x1037ec08, things=0x1034cbfc, bal=0x102b4568, table=0x102a0d54)
HOST_LAYOUT = [2, 0]
SZ = dict(person=0x240, world=0x400, map=0x10000, ride=0x40)
LABELS = dict(person=1, world=2, map=3, ride=4)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

_ct_plain = CL.recorder("CTRACE", "is")
_ct_gp = CL.recorder("CTRACE", "isii")


def stub_ctrace(e):
    (_ct_gp if "%d" in e.cstr(e.r(4)) else _ct_plain)(e)


STUBS.internal(0x1006fcbc, stub_ctrace)
STUBS.internal(0x1000dde8, CL.recorder("GAME_ASSERTMSG", "is"))


def stub_getid(e):
    e.write(e.r(3), e.read(e.r(4), 2))


STUBS.internal(0x1012e1f8, stub_getid)


def this_stub(name, addr, extra=None, back=0, ret=None):
    def f(e):
        L = e.cl
        args = (L.lab(e.r(3) - back),) + (extra(e) if extra else ())
        L.rec(name, args)
        if ret:
            ret(e, L)
    STUBS.internal(addr, f)


this_stub("RemovePersonFromQueue", 0x10108c20, lambda e: (e.r(4) >> 16,))
this_stub("DismissFromQueue", 0x1011f1b8)
this_stub("SetState", 0x10120354, lambda e: (s32(e.r(4)),))
this_stub("LowerHappiness", 0x1011b384, lambda e: (s32(e.r(4)),))
this_stub("RaiseHappiness", 0x1011b258, lambda e: (s32(e.r(4)),))
this_stub("SetDest", 0x10115400, lambda e: (e.r(4) >> 16,), ret=lambda e, L: e.set_r(3, 1))
this_stub("PeepWalks", 0x10114fa8, ret=lambda e, L: e.set_r(3, L.ret("PeepWalks") & MASK))
this_stub("ShowThoughtBubble", 0x1012f0d8, lambda e: (s32(e.r(4)), e.r(5) & 0xFF), back=0x30)
this_stub("PayAdmission", 0x100f3ac8, back=0x100)
this_stub("IsParkClosed", 0x1013ff7c, ret=lambda e, L: e.set_r(3, L.ret("IsParkClosed") & MASK))
this_stub("ParkGates_GetStatus", 0x1013ffb4, ret=lambda e, L: e.set_r(3, L.ret("ParkGates_GetStatus") & MASK))
this_stub("ArrivalVehicle_IsSeagoing", 0x10140de0, ret=lambda e, L: e.set_r(3, L.ret("ArrivalVehicle_IsSeagoing") & MASK))
this_stub("ArrivalVehicle_GetStatus", 0x101406cc, ret=lambda e, L: e.set_r(3, L.ret("ArrivalVehicle_GetStatus") & MASK))
this_stub("GetRand", 0x1013b1e4, ret=lambda e, L: e.set_r(3, L.ret("GetRand") & MASK))
this_stub("GetBankAccount", 0x1013edfc, ret=lambda e, L: e.set_r(3, (e.r(3) + 0x100) & MASK))
this_stub("GetParkAnalyser", 0x1013ebc8, ret=lambda e, L: e.set_r(3, (e.r(3) + 0x300) & MASK))
this_stub("GetTotalParkAttractionPoints", 0x100e5b1c, back=0x300,
          ret=lambda e, L: e.set_r(3, L.ret("GetTotalParkAttractionPoints") & MASK))
this_stub("OfferOpinionOn", 0x100e4360, lambda e: (s32(e.r(4)), e.r(5) & 0xFF), back=0x300)


def map_stub(name, addr):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(4)), s32(e.r(5))))
        e.write(e.r(3), struct.pack(">H", L.ret(name) & 0xFFFF))
    STUBS.internal(addr, f)


map_stub("GetTicketBoothPos", 0x101019bc)
map_stub("GetParkEntrancePos", 0x10101ae0)
map_stub("GetCrossingParkSidePos", 0x10101b74)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def h(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def b(self, a, v):
        self.write(a, bytes([v & 0xFF]))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blk = {k: e.alloc(n) for k, n in SZ.items()}
        self.L = e.cl = CL.PpcLog({self.blk[k]: n for k, n in LABELS.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self, world, map_):
        self.words(ADDR["world"], [world])
        self.words(ADDR["map"], [map_])

    def set_thing(self, i, p):
        self.words(ADDR["things"] + 0x14 * i, [p])

    def set_bal(self, i, v):
        self.words(ADDR["bal"] + 8 + 4 * i, [v])

    def set_table(self, i, v):
        self.h(ADDR["table"] + 2 * i, v)

    def call(self, op):
        entry = {"leave": LEAVE, "pay": PAY, "wait": WAIT, "walk": WALK, "feel": FEEL}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [self.blk["person"]])
        return r3 & MASK if op == "feel" else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blk = {k: lib.tpw_pq_alloc(n) for k, n in SZ.items()}
        CL.host_begin(lib, {n: self.blk[k] for k, n in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self, world, map_):
        self.lib.tpw_pq_setworld(world, map_)

    def set_thing(self, i, p):
        self.lib.tpw_pq_setthing(i, p)

    def set_bal(self, i, v):
        self.lib.tpw_pq_setbal(i, v & MASK)

    def set_table(self, i, v):
        self.lib.tpw_pq_settable(i, v & 0xFFFF)

    def call(self, op):
        f = getattr(self.lib, "tpw_pq_" + {"leave": "leave", "pay": "paying", "wait": "waiting", "walk": "walking", "feel": "feeling"}[op])
        r = f(self.blk["person"])
        return (r & MASK) if op == "feel" else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pq_alloc.restype, lib.tpw_pq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pq_setworld.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_pq_setthing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_pq_setbal.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_pq_settable.argtypes = [C.c_uint32, C.c_uint32]
    for n in ("leave", "paying", "waiting", "walking"):
        f = getattr(lib, "tpw_pq_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    lib.tpw_pq_feeling.restype, lib.tpw_pq_feeling.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    p, w, m, ride = (s.blk[k] for k in ("person", "world", "map", "ride"))
    s.set_world(w, m)
    for i in range(16):
        s.set_thing(i, 0)
    s.set_thing(case["ride"], ride)
    if case["thing0"]:
        s.set_thing(0, ride)
    s.h(p, case["id"])
    s.b(p + 4, case["x"])
    s.b(p + 6, case["y"])
    s.h(p + 0xC2, case["h0c2"])
    s.h(p + 0x1DC, case["ride"])
    s.words(p + 0x188, [case["w188"]])
    s.words(p + 0x19C, [case["f19c"]])
    s.words(p + 0x1BC, [case["w1bc"]])
    s.words(p + 0x200, [case["w200"]])
    s.words(p + 0x204, [case["w204"]])
    s.words(w + 0x100 + 0x118, [case["balance"]])
    cell = (case["x"] + (case["y"] << 7) + 1) & 0xFFFF
    s.h(m + (cell - 1) * 0x44 + 0x24, case["who"])
    for i, v in case["bal"].items():
        s.set_bal(i, v)
    for i, v in enumerate(case["tbl"]):
        s.set_table(i, v)


def observe(s, case, answer):
    p = s.blk["person"]
    return {"answer": answer, "log": s.log(),
            "words": [s.rd_word(p + o) for o in (0x188, 0x19C, 0x1BC, 0x200, 0x204)],
            "halves": [s.rd_half(p + o) for o in (0, 0xC0, 0xC2, 0xC4, 0x1DC)]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def fee_scen(points, b40, balance, b3c=1, c=(0.5, 1.0, 2.0)):
    return dict(points=points & MASK, b40=b40 & MASK, balance=balance & MASK, b3c=b3c & MASK,
                c44=fb(c[0]), c48=fb(c[1]), c4c=fb(c[2]))


def fee_edges():
    out = []
    for b40 in (0, 7):
        for points in (1, 40):
            F = points + b40
            for bal in (2 * F, 2 * F + 1, 2 * F - 1, F, F - 1, F + 1, F // 2, F // 2 + 1, 0, b40, b40 + 1, 0xFFFFFFFF):
                out.append(fee_scen(points, b40, bal))
        for bal in (b40, b40 + 1, 0):
            out.append(fee_scen(0, b40, bal))
    out.append(fee_scen(5, 3, 10, b3c=0))
    return out


FEE_EDGES = fee_edges()
SPECIAL_F = [0, 0x80000000, 0x7FC00000, 0x7F800000]
F19C = [0.0, 256.0, 255.0, 255.9, 1.5, -1.0, 300.7, 512.0, 0.5, -256.0, 1e6, -0.0, 65536.0, 1000000000.0]


def rfee(r):
    if r.random() < 0.6:
        return r.choice(FEE_EDGES)
    b40 = r.choice([0, 3, 100, r.getrandbits(32)])
    cf = lambda: r.choice(SPECIAL_F) if r.random() < 0.1 else fb(r.uniform(0, 4))
    s = fee_scen(r.choice([0, 1, r.randrange(0x100000), r.getrandbits(32)]), b40,
                 r.choice([0, r.randrange(1000), r.getrandbits(32), b40 + 1]), b3c=r.choice([1, 2, 3, 50, r.getrandbits(32)]))
    s.update(c44=cf(), c48=cf(), c4c=cf())
    return s


def mk(r, op, fee=None, q=None, **ov):
    x, y = r.randrange(8), r.randrange(8)
    pid = r.randrange(1, 0x10000)
    cell = x + (y << 7) + 1
    sc = fee if fee is not None else rfee(r)
    tb = lambda: r.choice([cell, cell, cell + 1, r.randrange(0x10000)])
    c = dict(op=op, x=x, y=y, id=pid, ride=r.randrange(16), thing0=r.random() < 0.5, h0c2=r.randrange(0x10000),
             w188=r.getrandbits(32), w1bc=r.getrandbits(32), w200=r.choice([0, 0, 1, r.randrange(1, 100)]),
             w204=r.choice([0, 0, 1, 7]), f19c=fb(r.choice(F19C)) if r.random() < 0.8 else fb(r.uniform(-1000, 1000)),
             who=r.choice([pid, pid, pid ^ 1, r.randrange(0x10000)]), tbl=[r.randrange(0x10000) for _ in range(3)],
             balance=sc["balance"],
             bal={13: sc["b3c"], 14: sc["b40"], 15: sc["c44"], 16: sc["c48"], 17: sc["c4c"],
                  33: r.getrandbits(32), 34: r.getrandbits(32), 35: r.getrandbits(32), 36: r.getrandbits(32)})
    qs = {"IsParkClosed": [r.choice([0, 0, 1])], "ParkGates_GetStatus": [r.choice([1, 1, 0, 2])],
          "ArrivalVehicle_IsSeagoing": [r.choice([0, 0, 1, 2])], "ArrivalVehicle_GetStatus": [r.choice([3, 3, 0, 1, 2])],
          "GetRand": [r.getrandbits(32) for _ in range(3)], "PeepWalks": [r.choice([0, 1, 2, 3])],
          "GetTicketBoothPos": [tb(), tb(), tb()], "GetParkEntrancePos": [r.randrange(0x10000)],
          "GetCrossingParkSidePos": [r.randrange(0x10000)], "GetTotalParkAttractionPoints": [sc["points"]]}
    if q:
        qs.update(q)
    c["queues"] = qs
    c.update(ov)
    return c


def edges_for(op):
    r = random.Random({"leave": 11, "pay": 12, "wait": 13, "walk": 14, "feel": 15}[op])
    if op == "leave":
        for ride in (0, 3, 15):
            for t0 in (False, True):
                yield mk(r, op, ride=ride, thing0=t0)
    elif op == "feel":
        for sc in FEE_EDGES:
            yield mk(r, op, fee=sc)
    elif op == "pay":
        for sc in FEE_EDGES:
            yield mk(r, op, fee=sc, w200=0)
        F = 41
        for f in F19C:
            yield mk(r, op, fee=fee_scen(40, 1, F), w200=0, f19c=fb(f))
        for w in (1, 2, 0xFFFFFFFF):
            yield mk(r, op, w200=w)
        for rnd in (0, 49, 50, 99, 0xFFFFFFFF):
            yield mk(r, op, fee=fee_scen(40, 1, 41 // 2 + 1), w200=0, q={"GetRand": [0, rnd, rnd]})
    elif op == "wait":
        for closed in (0, 1):
            for st in (1, 0):
                for w204 in (0, 1):
                    for w200 in (0, 2):
                        for tbm in (0, 1, 2):
                            for who in (0, 1):
                                c = mk(r, op, w204=w204, w200=w200)
                                cell = c["x"] + (c["y"] << 7) + 1
                                other = (cell + 5) & 0xFFFF
                                c["queues"].update({"IsParkClosed": [closed], "ParkGates_GetStatus": [st]})
                                c["queues"]["GetTicketBoothPos"] = [[cell, other, other], [other, cell, other], [other, other, other]][tbm]
                                c["who"] = c["id"] if who else c["id"] ^ 1
                                yield c
    elif op == "walk":
        for sea in (0, 1):
            for st in (3, 1):
                for low in (0, 1):
                    for walk in (0, 1, 2, 3):
                        for closed in (0, 1):
                            c = mk(r, op, id=(r.randrange(1, 0x4000) << 2) | (0 if low == 0 else r.choice([1, 2, 3])))
                            c["queues"].update({"ArrivalVehicle_IsSeagoing": [sea], "ArrivalVehicle_GetStatus": [st],
                                                "PeepWalks": [walk], "IsParkClosed": [closed]})
                            yield c


def rnd_for(op):
    def g(r):
        return mk(r, op)
    return g


TESTS = [
    FnTest("CPerson::LeaveQueue", ppc, host, edge=lambda: edges_for("leave"), rnd=rnd_for("leave")),
    FnTest("CPerson::ProcessStatePayingAtBooth", ppc, host, edge=lambda: edges_for("pay"), rnd=rnd_for("pay")),
    FnTest("CPerson::ProcessStateWaitingForParkToOpen", ppc, host, edge=lambda: edges_for("wait"), rnd=rnd_for("wait")),
    FnTest("CPerson::ProcessStateWalkingToTicketBooths", ppc, host, edge=lambda: edges_for("walk"), rnd=rnd_for("walk")),
    FnTest("CPerson::FeelingAboutEntryFee", ppc, host, edge=lambda: edges_for("feel"), rnd=rnd_for("feel")),
]
