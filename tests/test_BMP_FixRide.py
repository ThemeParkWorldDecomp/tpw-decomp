"""Unit v5220, the ride commands of the bump world, against libtpw_unit_BMP_FixRide.dylib (tests/unit_BMP_FixRide.cpp), main app
("SimTheme Park"), src/BMP_FixRide.cpp:
  BMP_HaltRide 0x100262cc, BMP_BreakRide 0x100264c0, BMP_FixRide 0x1002672c, BMP_BeginRide 0x10026924

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (TRACE, PAR_KillEmitter, PAR_InitEmitter, FlushDeferredAnim, SetCurRideAnim, SetControlType, BMP_DOCK, BMP_CloseRide) are fakes on
both sides that record their arguments (objects by index) and answer from the case's queues.

The world of a case: the list global (magic, ride block, bump block, bump count), four ride records, a block of bump slots, four ride anim
sets with their chain to the position nodes. What is compared: the ride records and bump slots after the call (owner pointers as ride
indices) and the event log. The functions are void on the Mac; the translation's answer is not compared.

No fused operations; the position floats stay far below 2^31 after the scale, so fctiwz needs no hooks. Not tested: a dead world (bad
magic) with ride 0 (the original then reads a null record), a bump count above the live slots (the walk runs off the block).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_FixRide"
MASK = 0xFFFFFFFF
MAGIC = 0x4A454647

HALT, BREAK, FIX, BEGIN = 0x100262CC, 0x100264C0, 0x1002672C, 0x10026924
SIZES = {HALT: 260, BREAK: 584, FIX: 468, BEGIN: 460}
OWN = set(SIZES)

TRACE, KILL, FLUSH, SETANIM = 0x1006FCFC, 0x100BE1E8, 0x100C6604, 0x100C6670
SETCTL, INITEM, DOCK, CLOSE = 0x1002D404, 0x100BEC30, 0x1002E740, 0x10026118

WORLD, SETS, TABLE = 0x102B56C0, 0x102C79A4, 0x1029C9EC
NRIDE, NSET, NENT = 4, 4, 4
RIDEW, SLOTW = 0xD0 // 4, 0xAC // 4


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ride_index(e, addr):
    return s32(addr - e.u32(WORLD + 8)) // 0xD0


def bump_index(e, addr):
    return s32(addr - e.u32(WORLD + 0x10)) // 0xAC


def set_index(e, addr):
    for i in range(8):
        if e.u32(SETS + 4 * i) == addr:
            return i
    return -1


STUBS.internal(TRACE, CL.recorder("TRACE", "s"))


def st_kill(e):
    e.cl.rec("PAR_KillEmitter", (s32(e.r(3)),))


def st_init(e):
    e.cl.rec("PAR_InitEmitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("PAR_InitEmitter") & MASK)


def st_flush(e):
    e.cl.rec("FlushDeferredAnim", (set_index(e, e.r(3)), e.r(4) & MASK))


def st_setanim(e):
    e.cl.rec("SetCurRideAnim", (set_index(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, fb(e.f(1)), e.r(8) & MASK))


def st_setctl(e):
    b = e.r(4)
    e.cl.rec("SetControlType", (ride_index(e, e.r(3)), bump_index(e, b), e.u32(b), s32(e.u32(b + 0x88))))


def st_dock(e):
    e.cl.rec("BMP_DOCK", (bump_index(e, e.r(3)),))
    e.set_r(3, e.cl.ret("BMP_DOCK") & MASK)


def st_close(e):
    e.cl.rec("BMP_CloseRide", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("BMP_CloseRide") & MASK)


for _a, _f in ((KILL, st_kill), (INITEM, st_init), (FLUSH, st_flush), (SETANIM, st_setanim), (SETCTL, st_setctl), (DOCK, st_dock),
               (CLOSE, st_close)):
    STUBS.internal(_a, _f)

PROBED = []


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

    def world(self):
        return WORLD

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def p32(self, addr):
        return addr

    def a32(self, word):
        return word

    def mkset(self, i, entries):
        sset, mesh, table, ents = self.alloc(0x20), self.alloc(0x30), self.alloc(8), self.alloc(entries * 0x14)
        self.words(sset + 8, [mesh])
        self.words(mesh + 0x28, [table])
        self.words(table + 4, [ents])
        self.words(SETS + 4 * i, [sset])
        self.ents = getattr(self, "ents", {})
        self.ents[i] = ents

    def mknode(self, i, k, x, y, z):
        node = self.alloc(0x40)
        self.words(node + 0x30, [x, y, z])
        self.words(self.ents[i] + 0x14 * k + 4, [node])

    def call(self, op, ride):
        entry = {"halt": HALT, "break": BREAK, "fix": FIX, "begin": BEGIN}[op]
        T.call_ppc(self.e, entry, [ride & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def world(self):
        return self.lib.tpw_bump_addr(b"world")

    def alloc(self, n):
        return self.lib.tpw_bump_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, addr):
        return self.lib.tpw_bump_off(addr)

    def a32(self, word):
        return self.lib.tpw_bump_from_off(word)

    def mkset(self, i, entries):
        self.lib.tpw_bump_mkset(i, entries)

    def mknode(self, i, k, x, y, z):
        self.lib.tpw_bump_mknode(i, k, x, y, z)

    def call(self, op, ride):
        getattr(self.lib, "tpw_bump_" + op)(s32(ride))


HOST_LAYOUT = [0xD0, 0xAC, 0x9C, 0x14, 0x38]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bump_addr.restype, lib.tpw_bump_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bump_alloc.restype, lib.tpw_bump_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bump_off.restype, lib.tpw_bump_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_bump_from_off.restype, lib.tpw_bump_from_off.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bump_mkset.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_bump_mknode.argtypes = [C.c_uint32] * 5
    lib.tpw_bump_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("halt", "break", "fix", "begin"):
        getattr(lib, "tpw_bump_" + n).argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bump_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    w = s.world()
    rides = s.alloc(NRIDE * 0xD0)
    for i, rw in enumerate(case["rides"]):
        s.words(rides + 0xD0 * i, rw)
    m = len(case["bumps"])
    bumps = s.alloc(max(1, m) * 0xAC)
    for i, bw in enumerate(case["bumps"]):
        bw = list(bw)
        bw[0x27] = s.p32(rides + 0xD0 * case["owners"][i]) if case["owners"][i] >= 0 else 0
        s.words(bumps + 0xAC * i, bw)
    for i in range(NSET):
        s.mkset(i, NENT)
        for k, node in enumerate(case["nodes"][i]):
            if node is not None:
                s.mknode(i, k, *node)
    s.words(w, [case["magic"], 0, s.p32(rides), 0, s.p32(bumps) if case["bumps_ok"] else 0, case["count"]])
    s.rides, s.bumps, s.m = rides, bumps, m


def observe(s, case):
    out = {"log": s.log(), "rides": s.rd_words(s.rides, NRIDE * RIDEW)}
    for i in range(s.m):
        ws = s.rd_words(s.bumps + 0xAC * i, SLOTW)
        o = s.a32(ws[0x27])
        ws[0x27] = (o - s.rides) // 0xD0 if o else -1
        out["bump%d" % i] = ws
    return out


def run(s, case):
    try:
        if not PROBED and isinstance(s, Ppc):
            PROBED.append(1)
            print("table", [hex(s.e.u32(TABLE + 4 * i)) for i in range(14)])
        build(s, case)
        s.call(case["op"], case["ride"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

KINDS = [-1, -2, -5, -14, -13, -3, 0, 3]


def rfloat(r):
    k = r.random()
    if k < 0.12:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.0009765625, 0.0009, 0.5, -0.0009])
    return r.uniform(-300, 300) if k < 0.7 else r.uniform(-3, 3)


def make(r, op, kind=None, match=True, magic_ok=True, f50=None, f54=None):
    rides = []
    for i in range(NRIDE):
        rw = [r.getrandbits(32) for _ in range(RIDEW)]
        rw[0] = r.choice(KINDS) & MASK
        rw[0x50 // 4] = r.choice([0, 1, 1, 2, 3]) & MASK
        rw[0x54 // 4] = r.choice([0, 2, 2, 1]) & MASK
        rides.append(rw)
    n = r.randrange(NRIDE)
    if kind is not None:
        rides[n][0] = kind & MASK
    if f50 is not None:
        rides[n][0x50 // 4] = f50 & MASK
    if f54 is not None:
        rides[n][0x54 // 4] = f54 & MASK
    m = r.choice([0, 1, 2, 3, 4, 6])
    bumps, owners, live = [], [], 0
    for i in range(m):
        bw = [r.getrandbits(32) for _ in range(SLOTW)]
        flags = r.getrandbits(32)
        if r.random() < 0.75:
            flags |= 1
        if r.random() < 0.7:
            flags |= 0x4000
        bw[0] = flags
        bw[2] = r.choice([0, 1, 2, 3, 3])
        bw[0x24 // 4] = r.choice([-1, 0, 1, 2, 3, 3]) & MASK
        bw[0x2C // 4] = r.choice([MASK, MASK, 5, 77]) & MASK
        owners.append(n if r.random() < 0.7 else r.randrange(NRIDE))
        bumps.append(bw)
        live += flags & 1
    count = live
    k = r.random()
    if k < 0.15:
        count = 0
    elif k < 0.3 and live:
        count = r.randrange(1, live + 1)
    nodes = [[None if r.random() < 0.25 else tuple(fb(rfloat(r)) for _ in range(3)) for _ in range(NENT)] for _ in range(NSET)]
    rid = (rides[n][0] << 8 | n) & MASK
    if not match:
        rid ^= 1 << r.randrange(8, 20)
    queues = {"PAR_InitEmitter": [r.randrange(1, 90)], "BMP_DOCK": [r.randrange(0, 3)], "BMP_CloseRide": [r.randrange(0, 3)]}
    return {"op": op, "ride": rid, "rides": rides, "bumps": bumps, "owners": owners, "nodes": nodes, "count": count,
            "magic": MAGIC if magic_ok else r.choice([0, MAGIC + 1]), "bumps_ok": not (op == "halt" and r.random() < 0.1),
            "queues": queues}


def fixed_ride(c):
    return c


def edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        for kind in range(-16, 3):
            for _ in range(3):
                yield make(r, op, kind=kind, f50=1, f54=2)
        for _ in range(6):
            yield make(r, op, match=False)
        for _ in range(4):
            c = make(r, op, magic_ok=False, match=False)
            c["ride"] = c["ride"] | 0x100
            yield c
        for v in (0, 1, 2, 3, -1):
            yield make(r, op, kind=-1, f50=v, f54=v)
        for _ in range(4):
            yield make(r, op, kind=-1, f50=1, f54=2)
    return gen


def rnd(op):
    return lambda r: make(r, op, match=r.random() < 0.85)


TESTS = [FnTest(name, ppc, host, edge=edges(op), rnd=rnd(op))
         for name, op in (("BMP_HaltRide", "halt"), ("BMP_BreakRide", "break"), ("BMP_FixRide", "fix"), ("BMP_BeginRide", "begin"))]
