"""Unit v51023, four coaster helpers, against libtpw_unit_HACKRestoreCoasterQueues.dylib (tests/unit_HACKRestoreCoasterQueues.cpp), main app
("SimTheme Park"), src/LoadCoasters_HACKRestoreCoasterQueues.cpp:
  HACKRestoreCoasterQueues 0x10044b10, NormaliseVectorXZ 0x10045a38, DisableCoasterCarDrawSafe 0x1004a648, IncCoasterConst1 0x10052564

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, host struct offsets from tpw_hrcq_layout) for the
translation. Callees outside the unit (CThing::GetThingId, RSSE_GetScriptPointer, RSSE_Getvar, UI_IterateThingNext,
HACKCoasterReturnLostPeep, DisableMeshDraw x2, FindNthDataNodeIndex, DisableRideHead, FlagGenerateTrack, CTRACE) are fakes on both sides
that record their arguments (objects by a marker word or id they hold) and answer from the case. sqrt is the std stub on the PowerPC side.

The world of a case: the coaster list, trains, cars and queue words, the visitors list, the script variables; the ride anim set table, its
mesh and embedded records; the edit coaster and its type floats; the vector. What is compared: the event log, the answer and the
floats written back.

No fused operations. Not tested: the registers r28 and r29 are the caller's when the coaster's script is NULL (the emulator side
is given 0 and the translation starts them 0); an index out of the embedded records.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HACKRestoreCoasterQueues"
MASK = 0xFFFFFFFF

FN_HACK, FN_NORM, FN_DISABLE, FN_INC = 0x10044B10, 0x10045A38, 0x1004A648, 0x10052564
SIZES = {FN_HACK: 552, FN_NORM: 108, FN_DISABLE: 224, FN_INC: 156}
OWN = set(SIZES)

ADDR = dict(gCoasterInstances=0x102B68B4, gCoasterEditInstance=0x102B68B0, gRideAnimSets=0x102C79A4)

LAYOUT_NAMES = ["inst_ent", "inst_next", "inst_script", "inst_cobj", "inst_ntrains", "inst_trains", "inst_type", "inst_size",
                "train_inuse", "train_cars", "train_size",
                "car_curA", "car_countA", "car_curB", "car_countB", "car_size",
                "set_flags", "set_heads", "set_fc0", "set_fc4", "set_size",
                "mesh_f28", "eset_records", "rec_flags", "rec_size"]
MAC = dict(inst_ent=0x10, inst_next=0x18, inst_script=0x24, inst_cobj=0x28, inst_ntrains=0xF4, inst_trains=0x128, inst_type=4,
           inst_size=0x14C, train_inuse=0xC, train_cars=0x28, train_size=0x80,
           car_curA=0x28, car_countA=0x30, car_curB=0x2C, car_countB=0x34, car_size=0x60,
           set_flags=4, set_heads=8, set_fc0=0xC0, set_fc4=0xC4, set_size=0x100,
           mesh_f28=0x28, eset_records=4, rec_flags=0, rec_size=0x14)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u32at(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def u16at(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def st_thingid(e):
    out, this = e.r(3), e.r(4)
    i = u16at(e, this)
    e.cl.rec("GetThingId", (i,))
    e.write(out, struct.pack(">H", i))


def st_script(e):
    e.cl.rec("GetScript", (e.r(3) & MASK,))
    e.set_r(3, e.cl.ret("Script") & MASK)


def st_getvar(e):
    e.cl.rec("Getvar", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, e.cl.ret("Var0" if (e.r(4) & MASK) == 0 else "Var1") & MASK)


def st_iter(e):
    thing = e.r(4) & MASK
    nxt = e.cl.head if thing == 0 else u32at(e, thing + 0x2F0)
    e.cl.rec("Iter", (e.r(3) & MASK, 0xFFFFFFFF if thing == 0 else u16at(e, thing)))
    e.set_r(3, nxt & MASK)


def st_return(e):
    e.cl.rec("ReturnLostPeep", (u32at(e, e.r(3) + 0x10), e.r(4) & MASK))


def st_dmd1(e):
    e.cl.rec("DisableMeshDraw", (u32at(e, e.r(3) + 0x80),))


def st_dmd2(e):
    e.cl.rec("DisableMeshDraw2", (u32at(e, e.r(3) + 0x80), e.r(4) & MASK, e.r(5) & MASK))


def st_find(e):
    e.cl.rec("FindNth", (u32at(e, e.r(3) + 0x300), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, e.cl.ret("Find") & MASK)


def st_head(e):
    e.cl.rec("DisableRideHead", (u32at(e, e.r(3) + 0x80), e.r(4) & MASK))


def st_flag(e):
    e.cl.rec("FlagGenerateTrack", (u32at(e, e.r(3) + 0x10), e.r(4) & MASK))


def st_ctrace(e):
    s = e.cstr(e.r(4) & MASK)
    b = s.encode("latin1") if isinstance(s, str) else bytes(s)
    # the doubles travel in r5/r6 and r7/r8 (copied through the stack); f1 and f2 are not loaded by the original
    e.cl.rec("CTRACE", (e.r(3) & MASK, sum(b) & MASK, len(b), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK, e.r(8) & MASK))


STUBS.internal(0x1012E1F8, st_thingid)
STUBS.internal(0x100D668C, st_script)
STUBS.internal(0x100D6C58, st_getvar)
STUBS.internal(0x101F9268, st_iter)
STUBS.internal(0x10049324, st_return)
STUBS.internal(0x100653A4, st_dmd1)
STUBS.internal(0x1006548C, st_dmd2)
STUBS.internal(0x101FFE74, st_find)
STUBS.internal(0x1020039C, st_head)
STUBS.internal(0x10040F08, st_flag)
STUBS.internal(0x1006FCBC, st_ctrace)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "I", v & MASK))

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.psize == 4 else "Q"), v))

    def rd32(self, a):
        return struct.unpack(self.fmt[0] + "I", self.read(a, 4))[0]

    def blk(self, n):
        a = self.alloc(n)
        self.write(a, bytes(n))
        return a


class Ppc(Side):
    fmt = ">I"
    psize = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.head = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, n):
        return MAC[n]

    def g(self, name):
        return ADDR[name]

    def set_head(self, h):
        self.L.head = h

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        self.e.set_r(28, 0)
        self.e.set_r(29, 0)
        r3 = T.call_ppc(self.e, entry, args)[0] & MASK
        return r3 if entry == FN_INC else 0


class Host(Side):
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.lay = dict(zip(LAYOUT_NAMES, lib.layout))

    def done(self):
        pass

    def off(self, n):
        return self.lay[n]

    def g(self, name):
        return self.lib.tpw_hrcq_addr(name.encode())

    def set_head(self, h):
        self.ptr(self.g("gIterHead"), h)

    def alloc(self, n):
        return self.lib.tpw_hrcq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        lib = self.lib
        if entry == FN_HACK:
            lib.tpw_hrcq_hack(args[0])
            return 0
        if entry == FN_NORM:
            lib.tpw_hrcq_norm(args[0])
            return 0
        if entry == FN_DISABLE:
            lib.tpw_hrcq_disable(args[0])
            return 0
        return lib.tpw_hrcq_inc() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hrcq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hrcq_hack.argtypes = [C.c_uint32]
    lib.tpw_hrcq_norm.argtypes = [C.c_uint64]
    lib.tpw_hrcq_disable.argtypes = [C.c_uint32]
    lib.tpw_hrcq_inc.restype = C.c_int32
    out = (C.c_int64 * 32)()
    n = lib.tpw_hrcq_layout(out)
    assert n == len(LAYOUT_NAMES), n
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_hack(s, c):
    o = s.off
    insts = c["insts"]
    addrs = [s.blk(o("inst_size") + 0x20) for _ in insts]
    for i, d in enumerate(insts):
        a = addrs[i]
        s.w32(a + o("inst_ent"), d["ent"])
        s.ptr(a + o("inst_next"), addrs[i + 1] if i + 1 < len(insts) else 0)
        s.w32(a + o("inst_script"), d["script"])
        cobj = s.blk(0x10)
        s.w16(cobj, d["cid"])
        s.ptr(a + o("inst_cobj"), cobj)
        s.w32(a + o("inst_ntrains"), d["ntrains"])
        trains = s.blk(max(1, len(d["trains"])) * o("train_size"))
        s.ptr(a + o("inst_trains"), trains)
        for ti, t in enumerate(d["trains"]):
            ta = trains + ti * o("train_size")
            s.w32(ta + o("train_inuse"), t["inuse"])
            cars = s.blk(max(1, len(t["cars"])) * o("car_size"))
            s.ptr(ta + o("train_cars"), cars)
            for ci, (A, B) in enumerate(t["cars"]):
                ca = cars + ci * o("car_size")
                for arr, nm in ((A, "A"), (B, "B")):
                    blk = s.blk(4 * len(arr) + 4)
                    for k, w in enumerate(arr):
                        s.w32(blk + 4 * k, w)
                    s.ptr(ca + o("car_cur" + nm), blk)
                    s.w32(ca + o("car_count" + nm), len(arr) if not c["trap"] else len(arr))
    s.ptr(s.g("gCoasterInstances"), addrs[0] if addrs else 0)
    prev = 0
    blocks = []
    for p in c["persons"]:
        b = s.blk(0x310)
        s.w16(b, p["id"])
        s.w16(b + 0x1DC, p["ride"])
        s.w32(b + 0x220, p["state"])
        blocks.append(b)
    for i, b in enumerate(blocks[:-1]):
        s.ptr(b + 0x2F0, blocks[i + 1])
    s.set_head(blocks[0] if blocks else 0)


def build_disable(s, c):
    o = s.off
    for i in range(8):
        s.ptr(s.g("gRideAnimSets") + i * s.psize, 0)
    d = c["set"]
    if d is None:
        return
    st = s.blk(0x400)
    mesh = s.blk(0x100)
    es = s.blk(0x40)
    recs = s.blk(8 * o("rec_size"))
    s.ptr(s.g("gRideAnimSets") + c["ride"] * s.psize, st)
    s.w32(st + o("set_flags"), d["flags"])
    s.ptr(st + o("set_heads"), mesh)
    s.w16(st + o("set_fc0"), d["fc0"][0])
    s.w16(st + o("set_fc0") + 2, d["fc0"][1])
    s.w16(st + o("set_fc4"), d["fc4"][0])
    s.w16(st + o("set_fc4") + 2, d["fc4"][1])
    s.w32(st + 0x300, d["marker"])
    s.w32(mesh, d["f00"])
    s.w32(mesh + 0x80, d["mmark"])
    s.ptr(mesh + o("mesh_f28"), es)
    s.ptr(es + o("eset_records"), recs)
    for i, f in enumerate(d["recs"]):
        s.w32(recs + i * o("rec_size") + o("rec_flags"), f)


def build_inc(s, c):
    o = s.off
    s.st = {}
    inst = c["inc"]
    if inst is None:
        s.ptr(s.g("gCoasterEditInstance"), 0)
        return
    a = s.blk(o("inst_size") + 0x20)
    s.w32(a + o("inst_ent"), inst["ent"])
    s.ptr(s.g("gCoasterEditInstance"), a)
    if inst["f8"] is not None:
        t = s.blk(0x40)
        s.w32(t + 8, inst["f8"])
        s.w32(t + 0xC, inst["fc"])
        s.ptr(a + o("inst_type"), t)
        s.st["type"] = t


def build_norm(s, c):
    s.st = {"p": s.blk(0x10)}
    for i, w in enumerate(c["vec"]):
        s.w32(s.st["p"] + 4 * i, w)


def build(s, c):
    {"hack": build_hack, "disable": build_disable, "inc": build_inc, "norm": build_norm}[c["op"]](s, c)


def execute(s, c):
    op = c["op"]
    if op == "hack":
        return s.call(FN_HACK, [c["target"]])
    if op == "disable":
        return s.call(FN_DISABLE, [c["ride"]])
    if op == "inc":
        return s.call(FN_INC, [])
    return s.call(FN_NORM, [s.st["p"]])


def observe(s, c, answer):
    r = {"answer": answer, "log": s.log()}
    if c["op"] == "inc" and "type" in s.st:
        r["f8"] = canon_f(s.rd32(s.st["type"] + 8))
        r["fc"] = canon_f(s.rd32(s.st["type"] + 0xC))
    if c["op"] == "norm":
        r["vec"] = [canon_f(s.rd32(s.st["p"] + 4 * i)) for i in range(3)]
    return r


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def hack_case(r, ninst=None, found=True, script=None, npers=None, ntr=None):
    npers = r.randrange(0, 6) if npers is None else npers
    ids = r.sample(range(1, 0x8000), npers + 4)
    cid = ids[0]
    pids = ids[1:1 + npers]
    pool = pids + [cid, 0, ids[-1]]

    def word():
        return r.choice([r.getrandbits(32), (r.getrandbits(16) << 16) | r.choice(pool), r.choice(pool), r.choice(pool)])

    ninst = r.randrange(1, 4) if ninst is None else ninst
    insts = []
    for i in range(ninst):
        trains = []
        n = r.randrange(0, 4) if ntr is None else ntr
        for _ in range(n + 1):
            cars = []
            for _ in range(r.randrange(1, 4)):
                cars.append(([word() for _ in range(r.randrange(0, 4))], [word() for _ in range(r.randrange(0, 4))]))
            trains.append({"inuse": r.randrange(0, len(cars) + 1), "cars": cars})
        insts.append({"ent": 100 + 7 * i, "script": r.randrange(0, 1000), "cid": cid if i == 0 else ids[-1], "ntrains": n, "trains": trains})
    if ninst > 1 and r.random() < 0.15:
        insts[1]["ent"] = insts[0]["ent"]
    target = insts[r.randrange(ninst)]["ent"] if found else 999
    sc = r.choice([0, 0x4000]) if script is None else script
    var = lambda: r.choice(pool + [0x10000 | r.choice(pool), r.getrandbits(32)])
    persons = [{"id": pid, "ride": cid if r.random() < 0.75 else r.getrandbits(16),
                "state": 0x10 if r.random() < 0.75 else r.choice([0xD, 0xE, 0x11, 0x100010, -16, 0])} for pid in pids]
    return {"op": "hack", "insts": insts, "target": target, "persons": persons, "trap": False,
            "queues": {"Script": [sc], "Var0": [var()], "Var1": [var()]}}


def hack_edges():
    r = random.Random(11)
    yield hack_case(r, found=False)
    yield hack_case(r, ninst=1, script=0, npers=3)
    yield hack_case(r, ninst=1, script=0x4000, npers=0)
    yield hack_case(r, ninst=1, script=0x4000, npers=4, ntr=0)
    for _ in range(12):
        yield hack_case(r, ninst=1, script=0x4000, npers=r.randrange(2, 6), ntr=r.randrange(1, 4))
    c = hack_case(r, ninst=1, npers=2)
    c["insts"] = []
    c["persons"] = []
    c["target"] = 100
    yield c


def hack_random(r):
    return hack_case(r)


def disable_case(r, ride=None, none=False):
    ride = r.randrange(1, 6) if ride is None else ride
    c = {"op": "disable", "ride": ride, "queues": {"Find": [r.choice([MASK, MASK, 0, 1, 2, 3]) for _ in range(16)]}}
    if none:
        c["set"] = None
        return c
    c["set"] = {"flags": r.choice([0, 0x4000, 0x3000, 0x7FFF, r.getrandbits(32)]), "f00": r.choice([0, 1, 2, 3, r.getrandbits(32)]),
                "fc0": (r.getrandbits(16), r.getrandbits(16)), "fc4": (r.getrandbits(16), r.getrandbits(16)),
                "marker": r.getrandbits(32), "mmark": r.getrandbits(32), "recs": [r.randrange(0, 8) for _ in range(8)]}
    return c


def disable_edges():
    r = random.Random(12)
    yield disable_case(r, none=True)
    for flags in (0, 0x4000, 0xBFFF, 0x3000):
        for f00 in (0, 1, 2, 3):
            c = disable_case(r)
            c["set"]["flags"], c["set"]["f00"] = flags, f00
            yield c
    c = disable_case(r)
    c["queues"]["Find"] = [MASK]
    yield c
    c = disable_case(r)
    c["queues"]["Find"] = [0]
    c["set"]["recs"] = [2] * 8
    yield c
    c = disable_case(r)
    c["queues"]["Find"] = [0]
    c["set"]["recs"] = [5] * 8
    yield c


def disable_random(r):
    return disable_case(r, none=r.random() < 0.1)


def inc_case(r, kind=None):
    kind = r.choice([0, 1, 2, 2, 2]) if kind is None else kind
    c = {"op": "inc", "queues": {}}
    if kind == 0:
        c["inc"] = None
    elif kind == 1:
        c["inc"] = {"ent": r.getrandbits(32), "f8": None, "fc": 0}
    else:
        c["inc"] = {"ent": r.getrandbits(32), "f8": rf(r), "fc": rf(r)}
    return c


def inc_edges():
    r = random.Random(13)
    for k in (0, 1, 2):
        yield inc_case(r, k)
    for b in SPECIAL + [fb(1.05), fb(1.0 / 1.05), fb(100.0)]:
        c = inc_case(r, 2)
        c["inc"]["f8"] = b
        yield c


def inc_random(r):
    return inc_case(r)


def norm_case(r, vec=None):
    return {"op": "norm", "queues": {}, "vec": [rf(r), rf(r), rf(r)] if vec is None else vec}


def norm_edges():
    r = random.Random(14)
    one = fb(1.0)
    for v in ([0, 0, 0], [one, 0, 0], [0, one, one], [one, one, one], [fb(3.0), fb(5.0), fb(4.0)], [fb(1e30), 0, fb(1e30)],
              [fb(1e-30), 0, fb(1e-30)], [0x7F800000, 0, 0], [0x7FC00000, one, one], [0x80000000, one, 0x80000000]):
        yield norm_case(r, v)


def norm_random(r):
    return norm_case(r)


TESTS = [
    FnTest("HACKRestoreCoasterQueues", ppc, host, edge=hack_edges, rnd=hack_random),
    FnTest("NormaliseVectorXZ", ppc, host, edge=norm_edges, rnd=norm_random),
    FnTest("DisableCoasterCarDrawSafe", ppc, host, edge=disable_edges, rnd=disable_random),
    FnTest("IncCoasterConst1", ppc, host, edge=inc_edges, rnd=inc_random),
]
