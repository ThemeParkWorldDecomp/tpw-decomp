"""Unit v5682, one tick of the bumper cars, against libtpw_unit_BMP_Tick.dylib (tests/unit_BMP_Tick.cpp), main app, src/BMP_Tick.cpp:
  BMP_Tick 0x10028cb0

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
The callees outside the unit (BMP_AI, BMP_Move, BMP_Collision, BMP_InsideArea, BMP_OutsideArea, BMP_ControlSound, GetFinishLine,
SetNextBuoyTarget) are fakes on both sides recording the bump's index, its flags word and the pointers; BMP_AI answers from a queue
and may clear the live bit or set bit 0x200000.

The world of a case: the list at 0x102b56c0 (magic, tick, bumps, count), 0 to 4 bumps of 0xac bytes (plus one spare live slot after them),
each with a collision object block, a buoy word, a ride word and the two counters. What is compared: the list words, every bump's words,
the event log. No fused operations. Not tested: a count that overruns the array (the original walks off the end too).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_Tick"
MASK = 0xFFFFFFFF

FN_TICK = 0x10028cb0
SIZES = {FN_TICK: 352}
OWN = set(SIZES)

CALLEES = {0x10029128: "BMP_AI", 0x10029b9c: "BMP_Move", 0x1002c7ec: "BMP_Collision", 0x1002c260: "BMP_InsideArea",
           0x1002be04: "BMP_OutsideArea", 0x10028e30: "BMP_ControlSound"}
FINISH = 0x1002d340
SETBUOY = 0x100296f4

ADDR = dict(world=0x102b56c0)
HOST_LAYOUT = [0x10, 0x14, 0xac]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def _bump_stub(name):
    def stub(e):
        L = e.cl
        p = e.r(3)
        first = e.u32(ADDR["world"] + 0x10)
        flags = e.u32(p)
        L.rec(name, ((p - first) // 0xac, flags))
        if name == "BMP_AI":
            r = L.ret("BMP_AI")
            if r & 1:
                flags &= ~1
            if r & 2:
                flags |= 0x200000
            e.write(p, struct.pack(">I", flags & MASK))
    return stub


for _a, _n in CALLEES.items():
    STUBS.internal(_a, _bump_stub(_n))


def stub_finish(e):
    e.cl.rec("GetFinishLine", (e.r(3),))
    e.set_r(3, e.cl.ret("GetFinishLine") & MASK)


def stub_setbuoy(e):
    p = e.r(4)
    first = e.u32(ADDR["world"] + 0x10)
    e.cl.rec("SetNextBuoyTarget", (e.r(3), (p - first) // 0xac, e.u32(p)))


STUBS.internal(FINISH, stub_finish)
STUBS.internal(SETBUOY, stub_setbuoy)


def setup(runner):
    T.install_std_stubs(runner.emu)


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

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def tick(self):
        T.call_ppc(self.e, FN_TICK, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bt_addr(name.encode())

    def alloc(self, n):
        a = self.lib.tpw_bt_alloc(n)
        self.base = self.lib.tpw_bt_base()
        return a

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def tick(self):
        self.lib.tpw_bt_tick()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bt_addr.restype, lib.tpw_bt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bt_alloc.restype, lib.tpw_bt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bt_base.restype, lib.tpw_bt_base.argtypes = C.c_uint64, []
    lib.tpw_bt_tick.restype, lib.tpw_bt_tick.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_bt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    slots = case["slots"]
    n = len(slots)
    s.base = 0 if isinstance(s, Ppc) else s.lib.tpw_bt_base()
    arr = s.alloc((n + 5) * 0xac)
    live = 0
    for i, sl in enumerate(slots + [dict(flags=1, c4=0, c8=0, buoy=0, ride=0)] * 4):
        col = s.alloc(0x20)
        s.words(col + 0x1c, [sl["buoy"]])
        a = arr + i * 0xac
        s.words(a, [sl["flags"]])
        s.words(a + 0x98, [col - s.base, sl["ride"], 0, sl["c4"], sl["c8"]])
        if i < n and sl["flags"] & 1:
            live += 1
    s.words(s.g("world"), [case["magic"], case["tick"], 0, 0, arr - s.base, case["count"] if case["count"] is not None else live])
    s.arr = arr
    s.n = n


def observe(s, case):
    w = s.g("world")
    words = s.rd_words(w, 6)
    words[4] = 0     # the array's address differs between the memories
    return {"log": s.log(), "world": words, "bumps": s.rd_words(s.arr, (s.n + 1) * 0xac // 4) if False else
            [[x for j, x in enumerate(s.rd_words(s.arr + i * 0xac, 0xac // 4)) if j not in (0x98 // 4,)] for i in range(s.n + 5)]}


def run(s, case):
    try:
        build(s, case)
        s.tick()
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def slot_case(r, c4=None, c8=None, live=None):
    return dict(flags=(1 if (r.random() < 0.8 if live is None else live) else 0) | r.choice([0, 0, 0x200000, 0x10000, 0x4000]) |
                (r.randrange(0, 1 << 8) << 24 if r.random() < 0.2 else 0),
                c4=r.choice([-1, 0, 1, 5, 0x10, 0x11, 0x30]) if c4 is None else c4,
                c8=r.choice([0, 1, 0x1e, 0x1f, 0x20, 0x21, 0x40, MASK]) if c8 is None else c8,
                buoy=r.choice([0, 0x1234, 0x5000]), ride=r.choice([0, 0x2000, 0x2400]))


def base(r, slots, magic=0x4A454647, count=None):
    return {"slots": slots, "magic": magic, "tick": r.choice([0, 1, 31, MASK]), "count": count,
            "queues": {"BMP_AI": [r.choice([0, 0, 0, 1, 2, 3]) for _ in range(r.randrange(1, 6))],
                       "GetFinishLine": [r.choice([0, 0x6000, 0x6100])]}}


def tick_edges():
    r = random.Random(682)
    yield base(r, [slot_case(r)], magic=0)
    yield base(r, [slot_case(r)], magic=0x4A454648)
    yield base(r, [], count=0)
    yield base(r, [slot_case(r, live=False)], count=0)
    for c4 in (-1, 0, 1, 0x10, 0x11):
        for c8 in (0, 0x1e, 0x1f, 0x20, 0x21, MASK):
            for buoy in (0, 0x1234):
                s = slot_case(r, c4, c8, live=True)
                s["buoy"] = buoy
                s["ride"] = r.choice([0, 0x2000])
                c = base(r, [s])
                c["queues"]["BMP_AI"] = [0]
                yield c
    for k in (1, 2, 3):
        c = base(r, [slot_case(r, 0x11, 0x20, live=True), slot_case(r, 0x20, 0x1f, live=True)])
        c["queues"]["BMP_AI"] = [k, 0]
        yield c
    yield base(r, [slot_case(r, live=False), slot_case(r, live=True), slot_case(r, live=False), slot_case(r, live=True)])


def tick_random(r):
    return base(r, [slot_case(r) for _ in range(r.randrange(0, 5))],
                magic=r.choice([0x4A454647] * 9 + [0, MASK]))


TESTS = [
    FnTest("BMP_Tick", ppc, host, edge=tick_edges, rnd=tick_random),
]
