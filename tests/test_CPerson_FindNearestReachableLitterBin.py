"""Unit v5245, the CPerson queue states and the nearest-thing searches, against libtpw_unit_CPerson_FindNearestReachableLitterBin.dylib
(tests/unit_CPerson_FindNearestReachableLitterBin.cpp), main app ("SimTheme Park"),
src/game/people/PersonQueue.cpp:
  CPerson::FindNearestReachableLitterBin 0x1011e9ec, CPerson::FindNearestEntertainer 0x1011ebc4,
  CPerson::ProcessStateBeingUnloaded 0x1011e0e4, CPerson::ProcessStateBeingLoaded 0x1011dcc0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (assert, CTRACE,
PeepWalks, SetDest, GetBackOfQueue, GetThingId, TooPricey, AdmitPerson, FindQueueDestination, the object and thought calls, SetState) are
fakes on both sides that record their arguments and answer from the case's queues; labs is a glue import answered by abs.

The world of a case: gWorld with the two list heads, gThingArray rows 1 to 6 pointing at thing blocks (cell bytes, flags, next links,
kind), the four sentinel halfwords, the person block. What is compared: the answer, the person block read back, the event log.

No fused operations. Not tested: a ride index of 0 or an unlinked row (the original dereferences a NULL thing), a list that never reaches
its sentinel.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_FindNearestReachableLitterBin"
MASK = 0xFFFFFFFF

F_LITTER, F_ENT, F_UNLOADED, F_LOADED = 0x1011E9EC, 0x1011EBC4, 0x1011E0E4, 0x1011DCC0
SIZES = {F_LITTER: 408, F_ENT: 344, F_UNLOADED: 460, F_LOADED: 632}
OWN = set(SIZES)

ADDR = dict(nominor=0x102A1000, noback=0x102A1002, litter_end=0x102A1004, ent_end=0x102A1006, array=0x1034CBFC, world=0x1037EC0C)
MAC_STRIDE = 0x14
NTHINGS = 6
PERSON_SIZE = 0x240
THING_SIZE = 0x220

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def rec(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret))


rec(0x1000DDE8, "GAME_ASSERTMSG", "is")
_ctrace = {0: CL.recorder("CTRACE", "is"), 2: CL.recorder("CTRACE", "isuu")}


def stub_ctrace(e):
    text = e.cstr(e.r(4))
    n = text.count("%d")
    _ctrace[2 if n >= 2 else 0](e)


STUBS.internal(0x1006FCBC, stub_ctrace)
rec(0x10114FA8, "PeepWalks", "L", ret="i")
rec(0x10115400, "SetDest", "Lw", ret="i")


def back_ret(e, log):
    v = log.ret("GetBackOfQueue") if hasattr(log, "ret") else 0
    e.write(e.r(3), struct.pack(">H", v & 0xFFFF))
    return e.r(3)


rec(0x101092F4, "GetBackOfQueue", "xL", ret=back_ret)
rec(0x1010EEC4, "WalkedAway", "L")
rec(0x1010D464, "PersonLeftDuringAdmittance", "Lw")
rec(0x10108C20, "RemovePersonFromQueue", "Lw")
rec(0x1010D0E0, "AdmitPerson", "Lw", ret="i")
rec(0x1011F1B8, "DismissFromQueue", "L")
rec(0x1011EF14, "FindQueueDestination", "L", ret="i")
rec(0x1011B384, "LowerHappiness", "Li")
rec(0x10120354, "SetState", "Li")
rec(0x10119EEC, "TooPricey", "LL", ret="i")
rec(0x1012F0D8, "ShowThoughtBubble", "Lib")
rec(0x1012F3D4, "RecordEvent", "Liw")


def thing_id_ret(e, log):
    e.write(e.r(3), e.read(e.r(4), 2))
    return e.r(3)


rec(0x1012E1F8, "GetThingId", "xL", ret=thing_id_ret)
STUBS.imp("labs", lambda e: e.set_r(3, abs(s32(e.r(3))) & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def h(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def b(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def blocks(self, case):
        self.person = self.alloc(PERSON_SIZE)
        self.things = [0] + [self.alloc(THING_SIZE) for _ in range(NTHINGS)]
        self.world = self.alloc(0x1DA800)
        self.res = self.alloc(8)

    def fixed(self):
        d = {self.person: 1, self.person + 0x30: 2}
        for k in range(1, NTHINGS + 1):
            d[self.things[k]] = 10 + k
        return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks(case)
        self.L = e.cl = CL.PpcLog(self.fixed(), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def entry(self, idx):
        return ADDR["array"] + idx * MAC_STRIDE

    def set_ptr(self, a, v):
        self.words(a, [v])

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, case):
        with_args = {"litter": [self.res, self.person, case["range"] & MASK], "ent": [self.res, self.person],
                     "unloaded": [self.person], "loaded": [self.person]}[op]
        entry = {"litter": F_LITTER, "ent": F_ENT, "unloaded": F_UNLOADED, "loaded": F_LOADED}[op]
        T.call_ppc(self.e, entry, with_args)
        if op in ("litter", "ent"):
            return struct.unpack(">H", self.read(self.res, 2))[0]
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks(case)
        CL.host_begin(lib, {label: addr for addr, label in self.fixed().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pq_addr(name.encode())

    def entry(self, idx):
        return self.g("array") + idx * self.lib.stride

    def set_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def alloc(self, n):
        return self.lib.tpw_pq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, case):
        L = self.lib
        if op == "litter":
            return L.tpw_pq_litter(self.person, case["range"] & MASK) & 0xFFFF
        if op == "ent":
            return L.tpw_pq_entertainer(self.person) & 0xFFFF
        if op == "unloaded":
            L.tpw_pq_unloaded(self.person)
        else:
            L.tpw_pq_loaded(self.person)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pq_addr.restype, lib.tpw_pq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pq_alloc.restype, lib.tpw_pq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pq_litter.restype, lib.tpw_pq_litter.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_pq_entertainer.restype, lib.tpw_pq_entertainer.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pq_unloaded.restype, lib.tpw_pq_unloaded.argtypes = None, [C.c_uint64]
    lib.tpw_pq_loaded.restype, lib.tpw_pq_loaded.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pq_layout(out)
    assert n == 3 and out[1] == 0 and out[2] >= 8, list(out)[:n]
    lib.stride = out[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.h(s.g("nominor"), case["nominor"])
    s.h(s.g("noback"), case["noback"])
    s.h(s.g("litter_end"), case["litter_end"])
    s.h(s.g("ent_end"), case["ent_end"])
    s.set_ptr(s.g("world"), s.world)
    s.h(s.world + 0x1DA746, case["head_l"])
    s.h(s.world + 0x1DA740, case["head_e"])
    for idx in range(NTHINGS + 1):
        s.set_ptr(s.entry(idx), 0)
    for k in range(1, NTHINGS + 1):
        s.set_ptr(s.entry(k), s.things[k])
        t = case["things"][k - 1]
        a = s.things[k]
        s.h(a, t["tid"])
        s.b(a + 2, t["kind"])
        s.b(a + 4, t["x"])
        s.b(a + 6, t["y"])
        s.h(a + 0xC, t["next"])
        s.h(a + 0x2E, t["flags"])
        s.h(a + 0x32, t["mapid"])
        s.h(a + 0x20C, t["next_e"])
    if case["row0"]:
        s.set_ptr(s.entry(0), s.things[1])
    p = s.person
    s.h(p, case["tid"])
    s.b(p + 4, case["px"])
    s.b(p + 6, case["py"])
    s.h(p + 0x1DC, case["ride"])
    s.h(p + 0x1DE, case["minor"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "person": person_view(s)}


def person_view(s):
    raw = bytearray(s.read(s.person, PERSON_SIZE))
    hs = [struct.unpack(s.fmt[0] + "H", bytes(raw[o:o + 2]))[0] for o in (0, 0x1DC, 0x1DE)]
    for o in (0, 0x1DC, 0x1DE):
        raw[o:o + 2] = b"\0\0"
    return {"halves": hs, "rest": bytes(raw)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def chain(r, ids_pool, n, sentinel):
    """Returns (head, next map) for a random walk through n of the ids ending at the sentinel."""
    order = r.sample(ids_pool, n)
    nxt = {}
    for i, k in enumerate(order):
        nxt[k] = order[i + 1] if i + 1 < n else sentinel
    return (order[0] if order else sentinel), nxt


def mk(r, op, n=None, small=True, **kw):
    ids = list(range(1, NTHINGS + 1))
    sl = r.choice([0, 0, 9, 0x1234])
    se = r.choice([0, 0, 9, 0x1234])
    nl = r.randrange(0, NTHINGS + 1) if n is None else n
    ne = r.randrange(0, NTHINGS + 1) if n is None else n
    head_l, nxt_l = chain(r, ids, nl, sl)
    head_e, nxt_e = chain(r, ids, ne, se)
    xr, yr = (r.choice([3, 6, 12]), r.choice([2, 5])) if small else (256, 256)
    things = []
    for k in ids:
        things.append(dict(tid=r.randrange(0x10000), kind=r.choice([3, 3, 3, 1, 0]), x=r.randrange(xr), y=r.randrange(yr),
                           next=nxt_l.get(k, r.randrange(0x10000)), next_e=nxt_e.get(k, r.randrange(0x10000)),
                           flags=r.choice([0x40, 0x40, 0, 0x80, 0xFFFF, 0x40 | r.randrange(0x10000)]), mapid=r.randrange(0x10000)))
    g1000 = r.choice([0, 5])
    ride = r.randrange(1, NTHINGS + 1)
    minor = r.choice([g1000, g1000] + ids)
    c = {"op": op, "things": things, "head_l": head_l, "head_e": head_e, "litter_end": sl, "ent_end": se,
         "row0": r.random() < 0.15, "tid": r.randrange(0x10000), "px": r.randrange(xr), "py": r.randrange(yr),
         "ride": ride, "minor": minor, "nominor": g1000, "noback": r.choice([0, 0x77]),
         "range": r.choice([0, 1, 2, 3, 5, 8, 20, 0x10001, MASK, r.randrange(30)]),
         "queues": {"PeepWalks": [r.choice([0, 0, 1, 2])], "SetDest": [r.choice([0, 1, 1])],
                    "GetBackOfQueue": [r.choice([0, 0x77, 0x3, r.randrange(0x10000)])], "TooPricey": [r.choice([0, 1])],
                    "AdmitPerson": [r.choice([0, 1])], "FindQueueDestination": [r.choice([0, 1])]}}
    c["queues"].update(kw.pop("queues", {}))
    c.update(kw)
    return c


def cases_for(op, seed):
    def edges():
        r = random.Random(seed)
        for n in (0, 1, 2, 3, 6):
            for _ in range(4):
                yield mk(r, op, n=n)
        for _ in range(6):
            yield mk(r, op, small=False)
        for sd in (0, 1):
            for pw in (0, 1, 2):
                for tp in (0, 1):
                    for ad in (0, 1):
                        for fq in (0, 1):
                            yield mk(r, op, queues={"SetDest": [sd], "PeepWalks": [pw], "TooPricey": [tp], "AdmitPerson": [ad],
                                                    "FindQueueDestination": [fq]})

    def rnd(r):
        return mk(r, op)

    return edges, rnd


def test(name, op, seed):
    edges, rnd = cases_for(op, seed)
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [
    test("CPerson::FindNearestReachableLitterBin", "litter", 11),
    test("CPerson::FindNearestEntertainer", "ent", 12),
    test("CPerson::ProcessStateBeingUnloaded", "unloaded", 13),
    test("CPerson::ProcessStateBeingLoaded", "loaded", 14),
]
