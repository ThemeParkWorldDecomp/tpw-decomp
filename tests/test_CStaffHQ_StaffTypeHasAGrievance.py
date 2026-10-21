"""Unit v5947, CStaffHQ::StaffTypeHasAGrievance, against libtpw_unit_CStaffHQ_StaffTypeHasAGrievance.dylib
(tests/unit_CStaffHQ_StaffTypeHasAGrievance.cpp), main app ("SimTheme Park"),
src/game/buildings/StaffHQ.cpp:
  CStaffHQ::StaffTypeHasAGrievance 0x1012b56c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (CWorld::GetParkAnalyser, CParkAnalyser::GetNumWalkableCells / GetNumLitteredCells, GAME_ASSERTMSG, CTRACE) are fakes on both sides
that record their arguments (texts as ids) and answer from the case's queues.

The world of a case: the world block with the five staff list heads, the ten list-end halfwords, the thing table rows (row 0 sometimes
occupied, so the assertion condition varies), five chains of staff blocks with happiness (+0x1f4), energy (+0x1f8) and the next id (+0x20c),
the headquarters with its forced strike word. What is compared: the answer and the event log (nothing else is written).

fctiwz is aimed by ROUNDING (Unicorn saturates wrongly); values stay below 2^31. Not tested: a list end that the chain never reaches (the
original would read row 0's NULL thing), NaN energies.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CStaffHQ_StaffTypeHasAGrievance"
MASK = 0xFFFFFFFF

FN = 0x1012B56C
SIZES = {FN: 2100}
OWN = set(SIZES)

ASSERT_FN, TRACE_FN, GETANALYSER, WALKABLE, LITTERED = 0x1000DDE8, 0x1006FCBC, 0x1013EBC8, 0x100E571C, 0x100E57D4

ADDR = dict(ends=0x102A11C8, gWorld=0x1037EC0C, thingarr=0x1034CBFC)
HOST_LAYOUT = [0x64, 0x1DA73C, 0x1DA73E, 0x1DA740, 0x1DA742, 0x1DA744, 0x1DA748]
HEADS = [0x1DA73C, 0x1DA73E, 0x1DA740, 0x1DA744, 0x1DA742]   # by staff type 0..4

TEXTS = [
    "Thing has been allocated in ThingArray index 0 - shouldn't happen\n",
    "%ss are striking through fatigue\n",
    "##Type",
    "%ss are striking through unhappiness\n",
    "Unknown staff type in StaffTypeHasAGrievance\n",
]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


def text_id(e, addr):
    if addr == 0:
        return -2
    s = e.cstr(addr)
    if isinstance(s, bytes):
        s = s.decode("latin-1")
    return TEXTS.index(s) if s in TEXTS else -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_assert(e):
    e.cl.rec("assert", (s32(e.r(3)), text_id(e, e.r(4))))


def stub_trace(e):
    e.cl.rec("trace", (s32(e.r(3)), text_id(e, e.r(4)), text_id(e, e.r(5))))


def stub_getanalyser(e):
    e.cl.rec("GetParkAnalyser", (1 if e.r(3) == e.u32(ADDR["gWorld"]) else 0,))
    e.set_r(3, 0x1000)


def stub_walk(e):
    e.cl.rec("GetNumWalkableCells", (e.r(4) & MASK,))
    e.set_r(3, e.cl.ret("walk") & MASK)


def stub_litter(e):
    e.cl.rec("GetNumLitteredCells", (e.r(4) & MASK,))
    e.set_r(3, e.cl.ret("litter") & MASK)


STUBS.internal(ASSERT_FN, stub_assert)
STUBS.internal(TRACE_FN, stub_trace)
STUBS.internal(GETANALYSER, stub_getanalyser)
STUBS.internal(WALKABLE, stub_walk)
STUBS.internal(LITTERED, stub_litter)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


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

    def set_thing(self, idx, addr):
        self.e.write(ADDR["thingarr"] + idx * 0x14, struct.pack(">I", addr & MASK))

    def set_world(self, addr):
        self.e.write(ADDR["gWorld"], struct.pack(">I", addr & MASK))

    def run(self, hq, t):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [hq, t & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sg_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_thing(self, idx, addr):
        self.lib.tpw_sg_set_thing(idx, addr)

    def set_world(self, addr):
        self.lib.tpw_sg_set_world(addr)

    def run(self, hq, t):
        return self.lib.tpw_sg_run(hq, s32(t)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sg_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "set_thing").argtypes = [C.c_uint32, C.c_uint64]
    getattr(lib, p + "set_world").argtypes = [C.c_uint64]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for i in list(range(64)) + [0x27FF]:
        s.set_thing(i, 0)
    world = s.alloc(0x1DA748)
    s.set_world(world)
    if case["row0"]:
        s.set_thing(0, s.alloc(16))
    ends = s.g("ends")
    for k, v in enumerate(case["ends"]):
        s.half(ends + 2 * k, v)
    for t, chain in enumerate(case["chains"]):
        s.half(world + HEADS[t], chain[0][0] if chain else 0)
        for i, (ident, energy, happy) in enumerate(chain):
            thing = s.alloc(0x210)
            s.words(thing + 0x1F4, [happy, energy])
            s.half(thing + 0x20C, chain[i + 1][0] if i + 1 < len(chain) else 0)
            s.set_thing(ident, thing)
    hq = s.alloc(0x68)
    s.words(hq + 0x64, [case["mf64"]])
    case["hq"] = hq


def observe(s, case, answer):
    return {"answer": answer, "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.run(case["hq"], case["type"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def value(r, mode):
    if mode == 0:
        return float(r.randrange(0, 15))
    if mode == 1:
        return float(r.randrange(15, 60))
    if mode == 2:
        return r.uniform(0, 30)
    if mode == 3:
        return r.choice([14.99, 15.0, 15.5, 255.9, 256.0, 300.0, -1.5, -20.0, 0.5, 1e6, -1e6, 14.0, 16.0])
    return float(r.randrange(0, 31))


def make_chain(r, ids, n, mode, fixed=None):
    chain = []
    for i in range(n):
        if fixed is not None:
            e, h = fixed[i]
        else:
            e, h = value(r, mode), value(r, mode)
        chain.append((ids[i], fb(e), fb(h)))
    return chain


def make_case(r, t=None, lens=None, mode=None, fixedE=None, fixedH=None, walk=None, litter=None):
    t = r.choice([-1, 0, 1, 2, 3, 4, 5, 9, -7, 0, 1, 2, 3, 4]) if t is None else t
    pool = r.sample(range(1, 60), 40)
    if r.random() < 0.2:
        pool[0] = 0x27FF
    chains, pos = [], 0
    for k in range(5):
        n = r.choice([0, 1, 2, 3, 4, 4, 5, 6, 8]) if lens is None else lens[k]
        m = r.randrange(5) if mode is None else mode
        ids = pool[pos:pos + n]
        pos += n
        if fixedE is not None and k == t:
            chains.append([(ids[i], fb(fixedE[i]), fb(fixedH[i])) for i in range(n)])
        else:
            chains.append(make_chain(r, ids, n, m))
    ends = []
    for k in range(10):
        chain = chains[[0, 1, 2, 3, 4][k // 2]]
        ends.append(0 if (not chain or r.random() < 0.5) else r.choice(chain)[0])
    q = {"walk": [r.choice([0, 1, 5, 10, 100, 1000, -3]) if walk is None else walk],
         "litter": [r.choice([0, 1, 2, 3, 20, 100, -1]) if litter is None else litter]}
    return {"queues": q, "type": t, "mf64": r.choice([0, 0, 0, 0, 1, 0x100, 0x80000000]), "row0": r.random() < 0.15,
            "chains": chains, "ends": ends}


def edges():
    r = random.Random(947)
    for t in range(-1, 7):
        yield make_case(r, t)
    for t in range(0, 5):
        yield make_case(r, t, walk=5, litter=1)           # exactly 0.2f: not above
        yield make_case(r, t, walk=5, litter=2)
        yield make_case(r, t, walk=0, litter=0)           # NaN
        yield make_case(r, t, walk=0, litter=3)           # +inf
        yield make_case(r, t, walk=100, litter=0)
    base_len = [0, 0, 0, 0, 0]
    for t in range(0, 5):
        for fe, fh in (([15, 15, 15, 15], [20, 20, 20, 20]), ([14, 14, 14, 14], [20, 20, 20, 20]), ([15, 15, 15, 14], [20, 20, 20, 20]),
                       ([0, 0, 0], [0, 0, 0]), ([20, 20, 20, 20], [15, 15, 15, 15]), ([20, 20, 20, 20], [14, 14, 14, 14]),
                       ([20, 20, 20, 20], [15, 15, 15, 14]), ([20, 20, 20, 20, 20], [0, 0, 0, 0, 100]), ([255.5, 256, 1, 1], [30, 30, 30, 30])):
            lens = list(base_len)
            lens[t] = len(fe)
            c = make_case(r, t, lens=lens, fixedE=fe, fixedH=fh, walk=0 if t == 0 else None, litter=0 if t == 0 else None)
            c["ends"] = [0] * 10
            c["mf64"] = 0
            yield c


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CStaffHQ::StaffTypeHasAGrievance", ppc, host, edge=edges, rnd=rnd),
]
