"""Unit v5980, the fly bases' peep handling and repair state, against libtpw_unit_FLY_ForceExtractPeep.dylib
(tests/unit_FLY_ForceExtractPeep.cpp), main app ("SimTheme Park"), src/game/rides/FlyingRide.cpp:
  FLY_ExtractPeep 0x1007848c, FLY_ForceExtractPeep 0x1007861c, FLY_AddPeep 0x100787c8, FLY_SetRepairState 0x10078b40

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (FindDataNodeIndexByID,
DisableRideHead, EnableRideHead, SetCurRideAnim, GAME_ASSERTMSG with its text, SOUND_SendEvent, CWorld::GetRand, labs, PAR_KillEmitter,
PAR_InitEmitter) are fakes on both sides that record their arguments (ride sets and meshes as the index of gRideAnimSets that owns them) and
answer from the case's queues. FLY_ForceExtractPeep runs as the original in FLY_SetRepairState's PowerPC run.

The world of a case: gFlyBases[fly] (a 0x122c block of random words: slots with active, count, peeps, ride, offset, divisor), three ride
sets with meshes, the sound bank word, gGameWorld. What is compared: the answer (not for FLY_SetRepairState, which answers nothing), the whole
base block read back and the event log.

No fused operations. Not tested: nothing skipped (the divisor -1 with the INT_MIN sum is not generated: the hardware result is undefined).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FLY_ForceExtractPeep"
MASK = 0xFFFFFFFF

FN_EXTRACT, FN_FORCE, FN_ADD, FN_REPAIR = 0x1007848C, 0x1007861C, 0x100787C8, 0x10078B40
SIZES = {FN_EXTRACT: 360, FN_FORCE: 384, FN_ADD: 364, FN_REPAIR: 888}
OWN = set(SIZES)

ADDR = dict(rides=0x102C79A4, bases=0x102DA964, banks=0x102F7C94, world=0x1037EBFC)
HOST_LAYOUT = [0x122C, 1, 4]
BLOCK_WORDS = 0x122C // 4


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def set_index(e, p):
    p &= MASK
    if p == 0:
        return 0
    for i in range(1, 4):
        if e.u32(ADDR["rides"] + 4 * i) == p:
            return i
    return MASK


def mesh_index(e, p):
    p &= MASK
    if p == 0:
        return 0
    for i in range(1, 4):
        st = e.u32(ADDR["rides"] + 4 * i)
        if st and e.u32(st + 8) == p:
            return i
    return MASK


def stub_find(e):
    e.cl.rec("Find", (set_index(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, e.cl.ret("Find") & MASK)


def stub_disable(e):
    e.cl.rec("Disable", (mesh_index(e, e.r(3)), e.r(4) & MASK))


def stub_enable(e):
    e.cl.rec("Enable", (mesh_index(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def stub_anim(e):
    e.cl.rec("Anim", (set_index(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, fb(e.f(1)), e.r(8) & MASK))      # the float takes r7's slot
    e.set_r(3, e.cl.ret("Anim") & MASK)


def stub_assert(e):
    e.cl.rec("Assert", (e.r(3) & MASK, e.r(5) & MASK), (e.cstr(e.r(4)),))


def stub_sound(e):
    e.cl.rec("Sound", tuple(e.r(i) & MASK for i in range(3, 9)))
    e.set_r(3, 0)


def stub_rand(e):
    e.cl.rec("Rand", ())
    e.set_r(3, e.cl.ret("Rand") & MASK)


def stub_labs(e):
    v = e.r(3) & MASK
    e.cl.rec("Labs", (v,))
    e.set_r(3, (-s32(v)) & MASK if s32(v) < 0 else v)


def stub_kill(e):
    e.cl.rec("Kill", (e.r(3) & MASK,))


def stub_init(e):
    e.cl.rec("Init", tuple(e.r(i) & MASK for i in range(3, 7)))
    e.set_r(3, e.cl.ret("Init") & MASK)


STUBS.internal(0x101FFDC8, stub_find)
STUBS.internal(0x1020039C, stub_disable)
STUBS.internal(0x10200230, stub_enable)
STUBS.internal(0x100C6670, stub_anim)
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x100DD9FC, stub_sound)
STUBS.internal(0x1013B1E4, stub_rand)
STUBS.internal(0x100BE1E8, stub_kill)
STUBS.internal(0x100BEC30, stub_init)
STUBS.imp("labs", stub_labs)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

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

    def set_mesh(self, st, mesh):
        self.words(st + 8, [mesh])

    def log(self):
        return self.L.log

    def call(self, fn, args):
        with_args = [a & MASK for a in args]
        r3, _, _ = T.call_ppc(self.e, fn, with_args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fly_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fly_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_mesh(self, st, mesh):
        self.lib.tpw_fly_setmesh(st, mesh)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = {FN_FORCE: self.lib.tpw_fly_force, FN_EXTRACT: self.lib.tpw_fly_extract, FN_ADD: self.lib.tpw_fly_add,
             FN_REPAIR: self.lib.tpw_fly_repair}[fn]
        return f(*[s32(a) for a in args]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fly_addr.restype, lib.tpw_fly_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fly_alloc.restype, lib.tpw_fly_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fly_setmesh.restype, lib.tpw_fly_setmesh.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_fly_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, n in (("force", 1), ("extract", 1), ("add", 2), ("repair", 2)):
        f = getattr(lib, "tpw_fly_" + name)
        f.restype, f.argtypes = C.c_uint32, [C.c_int32] * n
    out = (C.c_int64 * 32)()
    n = lib.tpw_fly_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for i in range(1, 4):
        st, mesh = s.alloc(0x400), s.alloc(0x40)
        s.ptr(s.g("rides") + i * s.psz, st)
        s.set_mesh(st, mesh)
    s.blk = None
    s.write(s.g("bases"), bytes(100 * s.psz))      # the host's globals outlive a case
    fly = case["fly"]
    if case["has_base"] and 1 <= fly < 100:
        s.blk = s.alloc(0x122C)
        s.words(s.blk, case["words"])
        s.ptr(s.g("bases") + fly * s.psz, s.blk)
    s.words(s.g("banks") + 8, [case["bank"]])
    s.ptr(s.g("world"), s.alloc(16))


def execute(s, case):
    fn = {"force": FN_FORCE, "extract": FN_EXTRACT, "add": FN_ADD, "repair": FN_REPAIR}[case["op"]]
    r = s.call(fn, case["args"])
    return 0 if case["op"] == "repair" else r


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "block": s.rd_words(s.blk, BLOCK_WORDS) if s.blk else []}


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


# ------------------------------------------------------------------------------------------------------------------ cases

SLOT = 0xE4


class Case(dict):
    def __repr__(self):
        return "<%s %s q=%s>" % (self["op"], self["args"], self["queues"]["Rand"])


class Words(list):
    def __repr__(self):         # keeps the failure line short
        return "<words>"


def slot_off(i, off):
    """The word index of a slot field: slot i starts at 0xdc; off is the field's offset from the slot start."""
    return (0xDC + i * SLOT + off) // 4


def rpos(r):
    return r.choice([0, 1, -1, 299, 300, 301, -300, -299, r.randrange(-5000, 5000), r.randrange(-(1 << 22), 1 << 22),
                     r.randrange(-(1 << 31), 1 << 31)]) & MASK


def rand_slot(w, r, i, sparse=False):
    w[slot_off(i, 0)] = r.choice([0, 1, 1, 2])
    w[slot_off(i, 0xC)] = (r.choice([0, 0, 1, 2, 3, 5, 11, -1, -2, -3, -5, -11])) & MASK
    for k in range(12):
        w[slot_off(i, 0x10) + k] = r.choice([0, r.randrange(1, 1000), r.randrange(1, 1 << 31)])
    w[slot_off(i, 0x40)] = r.choice([0, 1, 2, 3])
    w[slot_off(i, 0x44)] = r.choice([0, 0, 7, 99])
    w[slot_off(i, 0x58)] = r.choice([0, 1, 2, -1, -3, 5, r.randrange(-100, 100)]) & MASK
    w[slot_off(i, 0x5C)] = r.choice([0, 1, 2, 3, 4, 7, -1, -2]) & MASK


def rand_words(r):
    w = [0] * BLOCK_WORDS
    w[0], w[1] = r.choice([0, 1, 2, 5]), r.choice([0, 1, 2, 7])
    w[0x34 // 4] = r.choice([0, 1, 2, 3, 4, 5])
    w[0x48 // 4] = r.randrange(0, 30)
    w[0x28 // 4] = r.randrange(0, 600)
    for o in (0x60, 0x64, 0x68):
        w[o // 4] = rpos(r)
    w[0xD8 // 4] = r.choice([0, 0, 5, 300])
    for i in range(20):
        if r.random() < 0.45 or i < 2:
            rand_slot(w, r, i)
    return w


def repair_overrides(w, r):
    w[0x114 // 4] = r.choice([0, 1, 7])
    w[0x11C // 4] = r.choice([0, 1, 2, 3])
    w[0x120 // 4] = r.choice([0, 0, 7, 99])
    w[0x128 // 4] = r.choice([10, 10, 3])
    w[0x12C // 4] = r.choice([0, 0, 4])
    w[0x130 // 4] = r.choice([0, 0, 2])


def queues(r):
    return {"Find": [r.choice([0xFFFFFFFF, 0xFFFFFFFF, r.randrange(0, 40)]) for _ in range(r.randrange(1, 6))],
            "Anim": [r.choice([0, 1, 1000])], "Rand": [r.choice([0, 1, 99, 100, 12345, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)])],
            "Init": [r.choice([0, 5, 77])]}


def make(r, op, fly=None, words=None, state=None, has_base=True):
    fly = r.choice([1, 2, 50, 99]) if fly is None else fly
    w = rand_words(r) if words is None else words
    if op == "repair" and words is None:
        repair_overrides(w, r)
    args = [fly]
    if op == "add":
        args.append(r.choice([0, 1, 5, r.randrange(1, 1 << 31)]))
    if op == "repair":
        args.append(r.choice([0, 1, 2, 3, 4, 5, -1, 6]) if state is None else state)
    return Case({"op": op, "fly": fly, "args": args, "words": Words(w), "has_base": has_base, "queues": queues(r), "bank": r.getrandbits(32)})


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        for fly in (0, -1, 100, 101):
            yield make(r, op, fly=fly)
        yield make(r, op, fly=5, has_base=False)
        yield make(r, op, fly=1)
        if op == "repair":
            for state in (-1, 0, 1, 2, 3, 4, 5, 6):
                for flag in (0, 1):
                    for ride in (0, 1):
                        for a128 in (10, 3):
                            for rnd in (0, 5):
                                for em in (0, 7):
                                    w = [0] * BLOCK_WORDS
                                    w[0], w[1], w[0x34 // 4] = 1, 2, r.choice([0, state, 9])
                                    w[0x28 // 4], w[0x48 // 4] = 17, 4
                                    for o in (0x60, 0x64, 0x68):
                                        w[o // 4] = rpos(r)
                                    w[0xD8 // 4] = rnd
                                    # a slot the state 4 loop can drain: slot 1 with two peeps and a divisor
                                    rand_slot(w, r, 1)
                                    w[slot_off(1, 0xC)] = 2
                                    w[0x114 // 4], w[0x11C // 4], w[0x120 // 4] = flag, ride, em
                                    w[0x128 // 4], w[0x12C // 4], w[0x130 // 4] = a128, 0, 0
                                    yield make(r, op, words=w, state=state)
                                    w2 = list(w)
                                    w2[0x12C // 4] = 4
                                    yield make(r, op, words=w2, state=state)
        else:
            for count in (-2, -1, 0, 1, 2, 11):
                for div in (0, 1, 3, -1):
                    for ride in (0, 1):
                        for active in (0, 1, 2):
                            for fnd in (0xFFFFFFFF, 3):
                                w = [0] * BLOCK_WORDS
                                w[0x48 // 4] = 9
                                rand_slot(w, r, 0)
                                w[slot_off(0, 0)] = active
                                w[slot_off(0, 0xC)] = count & MASK
                                w[slot_off(0, 0x40)] = ride
                                w[slot_off(0, 0x5C)] = div & MASK
                                c = make(r, op, words=w)
                                c["queues"]["Find"] = [fnd]
                                yield c
                                # the same in a later slot, with an earlier slot not matching
                                w2 = [0] * BLOCK_WORDS
                                rand_slot(w2, r, 3)
                                w2[slot_off(3, 0)] = active
                                w2[slot_off(3, 0xC)] = count & MASK
                                w2[slot_off(3, 0x5C)] = div & MASK
                                w2[slot_off(3, 0x40)] = ride
                                w2[slot_off(0, 0)] = 0
                                w2[slot_off(0, 0xC)] = 0
                                c = make(r, op, words=w2)
                                c["queues"]["Find"] = [fnd]
                                yield c
    return gen


def rnd(op):
    return lambda r: make(r, op)


TESTS = [
    FnTest("FLY_ForceExtractPeep", ppc, host, edge=edges("force", 11), rnd=rnd("force")),
    FnTest("FLY_ExtractPeep", ppc, host, edge=edges("extract", 12), rnd=rnd("extract")),
    FnTest("FLY_AddPeep", ppc, host, edge=edges("add", 13), rnd=rnd("add")),
    FnTest("FLY_SetRepairState", ppc, host, edge=edges("repair", 14), rnd=rnd("repair")),
]
