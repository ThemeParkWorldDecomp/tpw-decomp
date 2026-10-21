"""Unit v5369, the trains' distances from the centre car, capacity and activation, against libtpw_unit_RecalcDistancesFromCentreCar.dylib
(tests/unit_RecalcDistancesFromCentreCar.cpp), main app ("SimTheme Park"), src/game/rides/CoasterQueue.cpp, src/game/rides/CoasterTrain.cpp:
  RecalcDistancesFromCentreCar 0x1004a4e8, SetTrainCapacity 0x1004a328, ActivateCoasterTrain 0x1004a8a0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's own offsets) for the translation. The callees
outside the unit are fakes on both sides that record their arguments: GetTrainFrontAndRearSamples, GetRideAnimTime, both EnableMeshDraw,
SetCurRideAnim and StartTrainSounds. RecalcDistancesFromCentreCar runs as the original inside SetTrainCapacity.

The world of a case: an instance with its type (train count), one to three trains with their types (car type pointers, spacing floats,
default capacity), eight cars a train, three car types, the table of ride anim sets. What is compared: the instance's words (flags, the
capacity word and its copy), every field of every train and car the functions can write, and the event log.

No fused operations. Not tested: a centre car or a car count past the eight cars allocated, a ride number past the table of eight.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RecalcDistancesFromCentreCar"
MASK = 0xFFFFFFFF

FN_RECALC, FN_SETCAP, FN_ACTIVATE = 0x1004A4E8, 0x1004A328, 0x1004A8A0
SIZES = {FN_RECALC: 260, FN_SETCAP: 380, FN_ACTIVATE: 380}
OWN = set(SIZES)

CALLEE_FRONTREAR = 0x1004A7D0
CALLEE_RIDETIME = 0x100C3268
CALLEE_MESH_RANGE = 0x100650C4
CALLEE_MESH = 0x10065178
CALLEE_SETANIM = 0x100C6670
CALLEE_SOUNDS = 0x10046A18

ADDR = dict(rides=0x102C79A4)
NCARS = 8
NRIDES = 8

KEYS = ["i_flags", "i_type", "i_b3c", "i_fac", "i_ff0", "i_qb8", "i_trains", "i_size",
        "t_flags", "t_type", "t_f08", "t_used", "t_cars", "t_centre", "t_size",
        "c_flags", "c_type", "c_index", "c_ride", "c_f1c", "c_hA", "c_hB", "c_cA", "c_cB", "c_nA", "c_nB", "c_f38", "c_f3c", "c_f44", "c_size",
        "y_carA", "y_carB", "y_carC", "y_f2f8", "y_f2e8", "y_f30c", "y_f310", "y_f314", "y_size",
        "k_flags", "k_size",
        "r_flags", "r_heads", "r_fc0", "r_fc4", "r_size"]
MAC_OFF = dict(zip(KEYS, [0, 4, 0x3C, 0xAC, 0xF0, 0xB8, 0x128, 0x14C,
                          0, 4, 8, 0xC, 0x28, 0x2C, 0x80,
                          0, 4, 8, 0xC, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x44, 0x60,
                          0x2FC, 0x300, 0x304, 0x2F8, 0x2E8, 0x30C, 0x310, 0x314, 0x360,
                          0, 8,
                          4, 8, 0xC0, 0xC4, 0xE8]))
MAC_TYPE_BASE = 0x2F0       # a train's type pointer is the coaster type plus 0x2f0 on the Mac


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_frontrear(e):
    L = e.cl
    L.rec("GetTrainFrontAndRearSamples", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_ridetime(e):
    e.cl.rec("GetRideAnimTime", (s32(e.r(3)),))
    e.set_r(3, 0)


def stub_mesh_range(e):
    L = e.cl
    L.rec("EnableMeshDrawRange", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def stub_mesh(e):
    L = e.cl
    L.rec("EnableMeshDraw", (L.lab(e.r(3)),))


def stub_setanim(e):
    L = e.cl
    L.rec("SetCurRideAnim", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, canon_f(fb(e.f(1))), e.r(8) & MASK))


def stub_sounds(e):
    L = e.cl
    L.rec("StartTrainSounds", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))


STUBS.internal(CALLEE_FRONTREAR, stub_frontrear)
STUBS.internal(CALLEE_RIDETIME, stub_ridetime)
STUBS.internal(CALLEE_MESH_RANGE, stub_mesh_range)
STUBS.internal(CALLEE_MESH, stub_mesh)
STUBS.internal(CALLEE_SETANIM, stub_setanim)
STUBS.internal(CALLEE_SOUNDS, stub_sounds)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def h16(self, a, hi, lo):
        self.write(a, struct.pack(self.fmt[0] + "HH", hi & 0xFFFF, lo & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    tbase = MAC_TYPE_BASE
    o = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.queues = case["queues"]
        self.saved = STUBS.activate(e)

    def begin(self, pairs):
        self.L = self.e.cl = CL.PpcLog({addr: lab for lab, addr in pairs}, self.queues)

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

    def call(self, op, args):
        fn = {"recalc": FN_RECALC, "setcap": FN_SETCAP, "activate": FN_ACTIVATE}[op]
        T.call_ppc(self.e, fn, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    tbase = 0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.hoff
        self.queues = case["queues"]

    def begin(self, pairs):
        CL.host_begin(self.lib, {lab: addr for lab, addr in pairs}, self.queues)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        if op == "recalc":
            self.lib.tpw_rc_recalc(args[0], args[1])
        elif op == "setcap":
            self.lib.tpw_rc_setcap(args[0], args[1] & MASK, args[2] & MASK, args[3] & MASK)
        else:
            self.lib.tpw_rc_activate(args[0], args[1])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rc_addr.restype, lib.tpw_rc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rc_alloc.restype, lib.tpw_rc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rc_recalc.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_rc_setcap.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_rc_activate.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_rc_layout(out)
    assert n == len(KEYS), n
    lib.hoff = dict(zip(KEYS, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    o = s.o
    pairs = []
    inst = s.alloc(o["i_size"])
    pairs.append((1, inst))
    ctypes_ = []
    for k, flags in enumerate(case["cartypes"]):
        p = s.alloc(o["k_size"])
        s.words(p + o["k_flags"], [flags])
        ctypes_.append(p)
    s.ctp = {p: k for k, p in enumerate(ctypes_)}
    # ride anim sets
    sets = []
    for r in range(NRIDES):
        d = case["rides"][r]
        if d is None:
            sets.append(0)
            s.wptr(s.g("rides") + s.psz * r, 0)
            continue
        p = s.alloc(o["r_size"])
        mesh = s.alloc(16)
        pairs.append((20 + r, p))
        pairs.append((40 + r, mesh))
        s.words(p + o["r_flags"], [d["flags"]])
        s.wptr(p + o["r_heads"], mesh)
        s.h16(p + o["r_fc0"], *d["fc0"])
        s.h16(p + o["r_fc4"], *d["fc4"])
        sets.append(p)
        s.wptr(s.g("rides") + s.psz * r, p)
    ntr = len(case["trains"])
    trains = s.alloc(o["t_size"] * ntr)
    ity = s.alloc(o["y_size"])
    s.words(ity + o["y_f2e8"], [ntr])
    s.words(inst + o["i_flags"], [case["flags"]])
    s.wptr(inst + o["i_type"], ity)
    s.words(inst + o["i_b3c"], [case["b3c"]])
    s.words(inst + o["i_fac"], [case["fac"]])
    s.words(inst + o["i_ff0"], [case["ff0"]])
    s.words(inst + o["i_qb8"], [case["qb8"]])
    s.wptr(inst + o["i_trains"], trains)
    s.trains = []
    for ti, t in enumerate(case["trains"]):
        tp = trains + o["t_size"] * ti
        pairs.append((2 + ti, tp))
        s.trains.append(tp)
        ty = s.alloc(o["y_size"])
        s.wptr(ty + o["y_carA"], ctypes_[t["ty"][0]])
        s.wptr(ty + o["y_carB"], ctypes_[t["ty"][1]])
        s.wptr(ty + o["y_carC"], ctypes_[t["ty"][2]])
        s.words(ty + o["y_f2f8"], [t["f2f8"]])
        s.words(ty + o["y_f30c"], [t["f30c"]])
        s.words(ty + o["y_f310"], [t["f310"]])
        s.words(ty + o["y_f314"], [t["f314"]])
        cars = s.alloc(o["c_size"] * NCARS)
        s.words(tp + o["t_flags"], [t["flags"]])
        s.wptr(tp + o["t_type"], ty + s.tbase)
        s.words(tp + o["t_f08"], [t["f08"]])
        s.words(tp + o["t_used"], [t["used"]])
        s.wptr(tp + o["t_cars"], cars)
        s.words(tp + o["t_centre"], [t["centre"]])
        for ci, c in enumerate(t["cars"]):
            cp = cars + o["c_size"] * ci
            s.words(cp + o["c_flags"], [c["flags"]])
            s.wptr(cp + o["c_type"], ctypes_[c["ty"]])
            s.words(cp + o["c_index"], [c["index"]])
            s.words(cp + o["c_ride"], [c["ride"]])
            s.words(cp + o["c_f1c"], [c["f1c"]])
            s.wptr(cp + o["c_hA"], c["hA"])
            s.wptr(cp + o["c_hB"], c["hB"])
            s.wptr(cp + o["c_cA"], c["cA"])
            s.wptr(cp + o["c_cB"], c["cB"])
            s.words(cp + o["c_nA"], [c["nA"]])
            s.words(cp + o["c_nB"], [c["nB"]])
            s.words(cp + o["c_f38"], [c["f38"]])
            s.words(cp + o["c_f3c"], [c["f3c"]])
            s.words(cp + o["c_f44"], [c["f44"]])
    s.inst = inst
    s.begin(pairs)


def execute(s, case):
    op = case["op"]
    if op == "setcap":
        s.call(op, [s.inst] + case["args"])
    else:
        s.call(op, [s.inst, s.trains[case["train"]]])


def observe(s, case):
    o = s.o
    r = {"log": s.log(),
         "inst": [s.rd_word(s.inst + o["i_flags"]), s.rd_word(s.inst + o["i_ff0"]), s.rd_word(s.inst + o["i_qb8"])]}
    for ti, tp in enumerate(s.trains):
        r["t%d" % ti] = [s.rd_word(tp + o[k]) for k in ("t_flags", "t_f08", "t_used", "t_centre")]
        cars = s.rptr(tp + o["t_cars"])
        for ci in range(NCARS):
            cp = cars + o["c_size"] * ci
            ws = [s.rd_word(cp + o[k]) for k in ("c_flags", "c_index", "c_ride", "c_f1c", "c_nA", "c_nB", "c_f44")]
            ws.append(s.ctp.get(s.rptr(cp + o["c_type"]), -1))
            ws += [s.rptr(cp + o[k]) for k in ("c_hA", "c_hB", "c_cA", "c_cB")]
            ws += [canon_f(s.rd_word(cp + o[k])) for k in ("c_f38", "c_f3c")]
            r["t%dc%d" % (ti, ci)] = ws
    return r


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def rcar(r, used, ride=None):
    return {"flags": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]),
            "ty": r.randrange(3),
            "index": r.choice([0, max(used - 1, 0), r.randrange(0, 8), r.randrange(0, 8), r.getrandbits(32)]) & MASK,
            "ride": r.choice([0, 1, 2, 3, 4, 5, 6, 7]) if ride is None else ride,
            "f1c": r.getrandbits(32),
            "hA": r.choice([0, 0x2000 + r.randrange(100)]), "hB": r.choice([0, 0x3000 + r.randrange(100)]),
            "cA": r.choice([0, 0x4000 + r.randrange(100)]), "cB": r.choice([0, 0x5000 + r.randrange(100)]),
            "nA": r.getrandbits(32), "nB": r.getrandbits(32),
            "f38": rf(r), "f3c": rf(r), "f44": r.getrandbits(32)}


def rtrain(r, used=None):
    used = r.randrange(0, 8) if used is None else used
    ty = [r.randrange(3) for _ in range(3)]
    if r.random() < 0.4:
        ty = [ty[1], ty[1], ty[1]]
    elif r.random() < 0.3:
        ty[0] = ty[1]
    return {"flags": r.choice([0, 2, 4, 6, 0x20, 0x44, r.getrandbits(32)]),
            "ty": ty, "f2f8": r.choice([0, 1, 5, 100, 1000, r.getrandbits(32)]),
            "f30c": rf(r), "f310": rf(r), "f314": rf(r),
            "f08": r.choice([0, 1, 3, r.randrange(0, 4)]),
            "used": used, "centre": r.choice([0, used // 2, max(used - 1, 0), r.randrange(0, 8)]) if used else r.choice([0, 0, 1]),
            "cars": [rcar(r, used) for _ in range(NCARS)]}


def rride(r):
    if r.random() < 0.1:
        return None
    return {"flags": r.choice([0, 0x4000, 0x4000, 0x20, 0xFFFFFFFF, r.getrandbits(32)]),
            "fc0": (r.getrandbits(16), r.getrandbits(16)), "fc4": (r.getrandbits(16), r.getrandbits(16))}


def base(r, op, ntr=1):
    rides = [rride(r) for _ in range(NRIDES)]
    rides[0] = None
    c = {"op": op, "queues": {},
         "flags": r.choice([0, 0x800, r.getrandbits(32)]),
         "b3c": r.choice([0, 4, 4, 4, 0xFFFFFFFB, 0xC, r.getrandbits(32)]),
         "fac": r.choice([fb(3.0), fb(3.0), rf(r)]),
         "ff0": r.getrandbits(32), "qb8": r.getrandbits(32),
         "cartypes": [r.choice([0, 1, 2, 3, r.getrandbits(32)]) for _ in range(3)],
         "rides": rides,
         "trains": [rtrain(r) for _ in range(ntr)], "train": 0}
    # a car's ride must have a set (the original dereferences the table entry)
    for t in c["trains"]:
        for car in t["cars"]:
            while rides[car["ride"]] is None:
                car["ride"] = r.randrange(0, NRIDES)
    return c


def recalc_case(r, used=None):
    c = base(r, "recalc", r.randrange(1, 3))
    c["train"] = r.randrange(len(c["trains"]))
    if used is not None:
        t = c["trains"][c["train"]]
        t["used"] = used
        t["centre"] = r.randrange(0, min(used + 2, NCARS))
        for car in t["cars"]:
            car["index"] = r.choice([0, used - 1, r.randrange(0, 8)]) & MASK
    return c


def recalc_edges():
    r = random.Random(1)
    for used in range(0, 8):
        for _ in range(4):
            yield recalc_case(r, used)
    for _ in range(10):
        c = recalc_case(r, 5)
        c["trains"][0]["centre"] = 0
        c["train"] = 0
        yield c
    c = recalc_case(r, 4)
    c["fac"] = 0
    yield c


def recalc_random(r):
    return recalc_case(r)


def setcap_case(r, a=None, b=None, c_=None):
    c = base(r, "setcap", r.randrange(1, 4))
    c["args"] = [r.choice([0, 0, 1, 7, 100, 1000, r.getrandbits(32)]) if a is None else a,
                 r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7]) if b is None else b,
                 r.choice([0, 1, 2, 3, r.getrandbits(32)]) if c_ is None else c_]
    for t in c["trains"]:
        for car in t["cars"]:
            car["f44"] = r.getrandbits(32)
    if r.random() < 0.5:
        for t in c["trains"]:
            t["used"] = c["args"][1] if c["args"][1] else t["used"]
    return c


def setcap_edges():
    r = random.Random(2)
    for b in (0, 1, 2, 3, 4, 5, 6, 7):
        for cc in (0, 1, 2, 3):
            for a in (0, 9, 0xFFFFFFFF):
                yield setcap_case(r, a, b, cc)
    c = setcap_case(r, 5, 0, 1)
    for t in c["trains"]:
        t["flags"] = 0
        t["used"] = 1
        t["ty"] = [0, 1, 1]
        for car in t["cars"]:
            car["ty"] = 1
    yield c                       # divisor 0 (divwu by 0)
    c = setcap_case(r, 5, 0, 1)
    for t in c["trains"]:
        t["flags"] = 4
        t["used"] = 0
    yield c                       # no cars, restored: the centre goes huge, nothing recalculated


def setcap_random(r):
    return setcap_case(r)


def activate_case(r, b3c=None):
    c = base(r, "activate", r.randrange(1, 3))
    c["train"] = r.randrange(len(c["trains"]))
    if b3c is not None:
        c["b3c"] = b3c
    return c


def activate_edges():
    r = random.Random(3)
    for b3c in (0, 4, 0xFFFFFFFB, 0xC, 0xFFFFFFFF):
        for _ in range(3):
            yield activate_case(r, b3c)
    for used in range(0, 8):
        c = activate_case(r, 4)
        t = c["trains"][c["train"]]
        t["used"] = used
        yield c


def activate_random(r):
    return activate_case(r)


TESTS = [
    FnTest("RecalcDistancesFromCentreCar", ppc, host, edge=recalc_edges, rnd=recalc_random),
    FnTest("SetTrainCapacity", ppc, host, edge=setcap_edges, rnd=setcap_random),
    FnTest("ActivateCoasterTrain", ppc, host, edge=activate_edges, rnd=activate_random),
]
