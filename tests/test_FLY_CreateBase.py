"""Unit v5236: FLY_CreateBase (makes a fly base in the first free slot of gFlyBases and starts its first car) and EmployEric (starts a car in
the first free slot of a base), against libtpw_unit_FLY_CreateBase.dylib (tests/unit_FLY_CreateBase.cpp), main app, src/game/rides/FlyingRide.cpp:
  FLY_CreateBase 0x100753e8, EmployEric 0x10074d9c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (LbMem_Alloc, labs,
CWorld::GetRand, SND_SendIndirectedEvent, GAME_ASSERTMSG, CRideDatabase::GetRideInfo, InstanceRide, SetCurRideAnim, FindDataNodeIndexByID)
are fakes on both sides that record every argument and answer from the case's queues. EmployEric runs as the original inside FLY_CreateBase.

The world of a case: the four fly zone limits, gGameWorld, the sound bank 7, the thing array's first entries (with the ride type at +0xe),
the ride info's +0x7b8 word, gRideAnimSets 0 to 7 (fake constants), gFlyBases, the base block. What is compared: the answer, the event log,
which gFlyBases slots are set (and whether kept), and every word of the base (the halfword at +0x58 as a halfword). The ride info's word is
written at the Mac's +0x7b8 on the PPC side and at the host field of the same name (the first sub ride type) on the host side
(tests/ridelayout.py).

No fused operations. Not tested: a zone span of 0 (divw by 0), a failed LbMem_Alloc (the original then reads address 0x4c), thing number
0 with no thing in entry 0 (the original reads the thing's +0xe at NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ridelayout
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FLY_CreateBase"
MASK = 0xFFFFFFFF

FN_CREATE, FN_EMPLOY = 0x100753E8, 0x10074D9C
SIZES = {FN_CREATE: 352, FN_EMPLOY: 1208}
OWN = set(SIZES)

CALLEE_RAND = 0x1013B1E4
CALLEE_SND = 0x100DF858
CALLEE_ASSERT = 0x1000DDE8
CALLEE_RIDEINFO = 0x10154868
CALLEE_INSTANCE = 0x10069B24
CALLEE_SETANIM = 0x100C6670
CALLEE_FIND = 0x101FFDC8

ADDR = dict(zb=0x1029D2B0, za=0x1029D2B4, zd=0x1029D2AC, zc=0x1029D2B8, world=0x1037EBFC, banks=0x102F7C94,
            things=0x1034CBFC, sets=0x102C79A4, bases=0x102DA964, db=0x1037F6C8)
HOST_LAYOUT = [0x122C]
WORLD = 0x7000
SETS = [0x5000 + 0x100 * i for i in range(8)]
BASE_WORDS = 0x122C // 4


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    size = e.r(3) & MASK
    e.cl.rec("LbMem_Alloc", (size,))
    e.set_r(3, e.alloc(size, b"\xa5" * size))


def stub_labs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


def stub_rideinfo(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, L.ri)


def stub_instance(e):
    L = e.cl
    sp = e.r(1)
    L.rec("InstanceRide", (e.r(3) & MASK, e.r(4) & MASK, L.lab(e.r(5)), e.r(6) & MASK, s32(e.r(7)), L.lab(e.r(8)), s32(e.r(9)),
                           s32(e.r(10)), e.u32(sp + 0x38), s32(e.u32(sp + 0x3C))))
    e.set_r(3, L.ret("InstanceRide") & MASK)


def stub_setanim(e):
    L = e.cl
    L.rec("SetCurRideAnim", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK,
                             struct.unpack(">I", struct.pack(">f", e.f(1)))[0], e.r(8) & MASK))
    e.set_r(3, L.ret("SetCurRideAnim") & MASK)


def stub_find(e):
    L = e.cl
    L.rec("FindDataNodeIndexByID", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("FindDataNodeIndexByID") & MASK)


STUBS.imp("LbMem_Alloc__FUi", stub_alloc)
STUBS.imp("labs", stub_labs)
STUBS.internal(CALLEE_RAND, CL.recorder("GetRand", "L", ret="i"))
STUBS.internal(CALLEE_SND, CL.recorder("SND_SendIndirectedEvent", "iiiiiii", ret="i"))
STUBS.internal(CALLEE_ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(CALLEE_RIDEINFO, stub_rideinfo)
STUBS.internal(CALLEE_INSTANCE, stub_instance)
STUBS.internal(CALLEE_SETANIM, stub_setanim)
STUBS.internal(CALLEE_FIND, stub_find)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_block(self, a):
        """The base's words, with the halfword at +0x58 read as a halfword and the other half as raw bytes."""
        ws = self.rd_words(a, BASE_WORDS)
        h = struct.unpack(self.fmt[0] + "H", self.read(a + 0x58, 2))[0]
        ws[0x58 // 4] = (h, struct.unpack(self.fmt[0] + "H", self.read(a + 0x5A, 2))[0])
        return ws


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {WORLD: 1, ADDR["db"]: 2}
        fixed.update({SETS[i]: 10 + i for i in range(8)})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.L.ri = e.alloc(0x800)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ri(self):
        return self.L.ri

    def rec(self, mac):
        return mac

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setbase(self, i, p):
        self.words(ADDR["bases"] + 4 * i, [p])

    def getbase(self, i):
        return self.rd_words(ADDR["bases"] + 4 * i, 1)[0]

    def setthing(self, i, p):
        self.words(ADDR["things"] + 0x14 * i, [p])

    def setset(self, i, p):
        self.words(ADDR["sets"] + 4 * i, [p])

    def create(self, c):
        r3, _, _ = T.call_ppc(self.e, FN_CREATE, [w & MASK for w in c["args"]])
        return s32(r3)

    def employ(self, c):
        r3, _, _ = T.call_ppc(self.e, FN_EMPLOY, [c["index"]])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {1: WORLD, 2: self.g("db")}
        fixed.update({10 + i: SETS[i] for i in range(8)})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fe_addr(name.encode())

    def ri(self):
        return self.g("ri")

    def rec(self, mac):
        return ridelayout.host_offset(self.lib, mac)

    def alloc(self, n):
        return self.lib.tpw_fe_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setbase(self, i, p):
        self.lib.tpw_fe_setbase(i, p)

    def getbase(self, i):
        return self.lib.tpw_fe_getbase(i)

    def setthing(self, i, p):
        self.lib.tpw_fe_setthing(i, p)

    def setset(self, i, p):
        self.lib.tpw_fe_setset(i, p)

    def create(self, c):
        return self.lib.tpw_fe_create(*[w & MASK if k == 0 else s32(w) for k, w in enumerate(c["args"])])

    def employ(self, c):
        return self.lib.tpw_fe_employ(c["index"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fe_addr.restype, lib.tpw_fe_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fe_alloc.restype, lib.tpw_fe_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fe_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fe_setbase.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fe_getbase.restype, lib.tpw_fe_getbase.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_fe_setthing.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fe_setset.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fe_create.restype = C.c_int32
    lib.tpw_fe_create.argtypes = [C.c_uint32] + [C.c_int32] * 6
    lib.tpw_fe_employ.restype, lib.tpw_fe_employ.argtypes = C.c_int32, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fe_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    ridelayout.bind(lib)
    lib.bound = True


def build(s, c):
    g = s.g
    s.words(g("zb"), [c["zones"][0]])
    s.words(g("za"), [c["zones"][1]])
    s.words(g("zd"), [c["zones"][2]])
    s.words(g("zc"), [c["zones"][3]])
    s.words(g("world"), [WORLD])
    s.words(g("banks") + 0x1C, [c["bank7"]])
    s.words(s.ri() + s.rec(0x7B8), [c["rideno"]])
    for i in range(8):
        s.setset(i, SETS[i])
    for i in range(4):
        if c["things"][i] is None:
            s.setthing(i, 0)
        else:
            t = s.alloc(0x20)
            s.half(t + 0xE, c["things"][i])
            s.setthing(i, t)
    orig = {}
    for i in range(100):
        s.setbase(i, 0)
    for i in c.get("occupied", []):
        orig[i] = s.alloc(16)
        s.setbase(i, orig[i])
    if "block" in c:
        b = s.alloc(0x122C)
        s.words(b, c["block"])
        s.half(b + 0x58, c["thingno"])
        s.half(b + 0x5A, c["block"][0x58 // 4])
        s.setbase(c["index"], b)
        orig[c["index"]] = b
    c["_orig"] = orig


def observe(s, c, answer):
    orig = c["_orig"]
    flags = []
    for i in range(100):
        p = s.getbase(i)
        flags.append(0 if p == 0 else 1 if orig.get(i) == p else 2)
    idx = answer if c["op"] == "create" else c["index"]
    block = s.rd_block(s.getbase(idx)) if 1 <= idx < 100 else None
    return {"answer": answer, "log": s.log(), "flags": flags, "block": block}


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, s.create(c) if c["op"] == "create" else s.employ(c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ------------------------------------------------------------------------------------------------------------ case generators

RANDS = [0, 1, 2, 3, 99, 100, 101, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFF9C]


def rr(r):
    return r.choice(RANDS) if r.random() < 0.5 else r.randrange(0, 1 << 32)


def zones(r):
    k = r.random()
    if k < 0.4:
        return [0x11170, 0x222E0, 0x2E630, 0x7530]      # B, A, D, C as the game has them
    if k < 0.5:
        return [0x11170 + 1, 0x222E0 - 1, 0x2E630 + 1, 0x7530 - 1]
    z = [r.randrange(-300000, 300000) for _ in range(4)]
    if z[0] == z[2]:
        z[2] += 7
    if z[1] == z[3]:
        z[3] += 5
    return [v & MASK for v in z]


def world(r, op):
    nrand = r.randrange(6, 16)
    finds = [r.randrange(0, 1 << 31) for _ in range(r.choice([0, 0, 1, 2, 4]))] + [0xFFFFFFFF]
    things = [r.choice([None, 5, 0x1234, 0xFFFF]) if r.random() < 0.5 else r.randrange(0, 0x10000) for _ in range(4)]
    things[0] = r.choice([None, 7, 0x40])
    return {"op": op, "zones": zones(r), "bank7": r.randrange(0, 1 << 32), "rideno": r.choice([0, 1, 0x85, r.randrange(0, 1 << 32)]),
            "things": things,
            "queues": {"GetRand": [rr(r) for _ in range(nrand)], "SND_SendIndirectedEvent": [r.randrange(0, 1 << 32)],
                       "InstanceRide": [r.choice([0, 1, 2, 7, r.randrange(0, 8)])],
                       "SetCurRideAnim": [r.choice([0, 1, 0x30, r.randrange(0, 1 << 32)])], "FindDataNodeIndexByID": finds}}


def fix_thing(c, no):
    """The thing number must name a present thing (the original reads its +0xe)."""
    if c["things"][no] is None:
        c["things"][no] = r_type(no)
    return no


def r_type(no):
    return 0x100 + no


# ---- EmployEric
def employ_case(r, index=None, free=None, count=None):
    c = world(r, "employ")
    c["index"] = r.choice([1, 2, 17, 50, 99]) if index is None else index
    c["thingno"] = fix_thing(c, r.randrange(0, 4))
    first = r.choice([0, 0, 1, 5, 18, 19, 20]) if free is None else free
    words = [r.randrange(0, 1 << 32) for _ in range(BASE_WORDS)]
    words[0x28 // 4] = r.choice([0, 300, r.randrange(0, 1 << 32)])
    words[0xC // 4] = (r.choice([0, 0xFFFFFFFF, 3, r.randrange(0, 1 << 32)]) if count is None else count)
    for k in range(20):
        v = 0
        if k < first:
            v = r.choice([1, 4, 0xFFFFFFFF, r.randrange(1, 1 << 32)])
        elif k > first:
            v = r.choice([0, 0, r.randrange(0, 1 << 32)])
        words[(0xDC + k * 0xE4) // 4] = v
    c["block"] = words
    return c


def employ_edges():
    r = random.Random(5236)
    for first in (0, 1, 19, 20):
        yield employ_case(r, free=first)
    for count in (0, 0xFFFFFFFF, 1, 2):
        yield employ_case(r, free=0, count=count)
    for z in ([0x11170, 0x222E0, 0x2E630, 0x7530],):
        for i in range(24):
            c = employ_case(r, free=i % 3)
            c["zones"] = z
            c["queues"]["GetRand"] = [(i >> 0) & 1, (i >> 1) & 1, (i >> 2) & 1, i, i + 3] * 3
            yield c
    for i in range(6):
        c = employ_case(r, free=0)
        c["queues"]["InstanceRide"] = [i % 2]
        c["queues"]["FindDataNodeIndexByID"] = [0xFFFFFFFF] if i < 3 else [1, 2, 3, 0xFFFFFFFF]
        c["things"][0] = r.choice([None, 9])
        c["thingno"] = fix_thing(c, i % 4) if not (i % 4 == 0 and c["things"][0] is None) else fix_thing(c, 1)
        yield c


def employ_random(r):
    c = employ_case(r)
    if c["thingno"] == 0 and c["things"][0] is None:
        c["thingno"] = fix_thing(c, 1)
    return c


# ---- FLY_CreateBase
def create_case(r, first=None):
    c = world(r, "create")
    no = r.randrange(0, 4)
    if c["things"][0] is None and no == 0:
        no = 1
    fix_thing(c, no)
    first = r.choice([1, 1, 2, 50, 99, 100]) if first is None else first
    occ = list(range(1, first)) + [i for i in range(first + 1, 100) if r.random() < 0.3]
    if r.random() < 0.5:
        occ.append(0)
    c["occupied"] = occ
    low = r.randrange(0, 1 << 16)
    c["args"] = [(no << 16) | low, r.randrange(0, 1 << 32), r.randrange(0, 1 << 32), r.randrange(0, 1 << 32), r.randrange(0, 1 << 32),
                 r.choice([0, 0x400, 0x401, r.randrange(0, 1 << 32)]), r.choice([0, 1, 0xFFFFFFFF, r.randrange(0, 1 << 32)])]
    return c


def create_edges():
    r = random.Random(1007)
    for first in (1, 2, 99, 100):
        yield create_case(r, first)
    for a in (0, 1):
        c = create_case(r, 3)
        c["args"][6] = a
        yield c
    for i in range(10):
        yield create_case(r)


def create_random(r):
    return create_case(r)


TESTS = [
    FnTest("FLY_CreateBase", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("EmployEric", ppc, host, edge=employ_edges, rnd=employ_random),
]
