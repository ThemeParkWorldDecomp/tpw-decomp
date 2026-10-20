"""Unit v5900, the coaster train loading, against libtpw_unit_UnloadTrain_2.dylib (tests/unit_UnloadTrain_2.cpp), main app
("SimTheme Park"), src/AllocateQueue_UnloadTrain_2.cpp:
  UnloadTrain 0x100499e4, KickPeepsFromEntranceQueue 0x10049858, LoadTrain 0x10049bf4, CoasterSetCapacity 0x10049758,
  SetEntranceLinearSamples 0x10049ddc, GetCarsInStation 0x10049e8c, HandleTrainLoading 0x10049fbc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the structures keep the Mac's fields but the host's
offsets and 8-byte pointers (an offset table per side, the host's exported by the library). The callees outside the unit are fakes on both
sides: EnableRideHead(mesh, index, peep) and DisableRideHead(mesh, index) (a mesh is logged as the id in its first word) and
SetTrainCapacity(instance, a, b, c).

The world of a case: gObjectTable, gRideAnimSets, the three globals (the two floors and the random word), one coaster instance with its
two wraparound queues (entrance queue +0xb0, exit queue +0xd0) and ring blocks, its type, one train with up to 4 cars (+ an offset), two car
types with head index arrays, the cars' head slot arrays and a chain of three braces. What is compared: the log, both queues (words, cursors
relative to the block, ring contents), the instance's three float/int words, the train's counters, every car's counts, slot array identity
and contents, the random word and GetCarsInStation's two outputs.

SetEntranceLinearSamples has fused multiply-adds: the ROUNDING hook gives them the hardware's single rounding. Not tested:
CoasterSetCapacity's answer (the original leaves r3 as the instance or SetTrainCapacity's leftover; nothing reads it); a queue of capacity 0
(the emulator's divwu answers 0); HandleTrainLoading with no car at all (GetCarsInStation then answers the null's distance, and the loop that
follows runs off the car array); GetCarsInStation with no car in the station is compared as the original's address-dependent formula.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_UnloadTrain_2"
MASK = 0xFFFFFFFF

FN = dict(unload=0x100499e4, kick=0x10049858, load=0x10049bf4, setcap=0x10049758, entr=0x10049ddc, cars=0x10049e8c, handle=0x10049fbc)
SIZES = {0x100499e4: 452, 0x10049858: 324, 0x10049bf4: 412, 0x10049758: 212, 0x10049ddc: 104, 0x10049e8c: 220, 0x10049fbc: 572}
OWN = set(SIZES)

DISABLE, ENABLE, SETCAP = 0x1020039c, 0x10200230, 0x1004a328

ADDR = dict(table=0x102b6f04, sets=0x102c79a4, capfloor=0x1029ce28, kickfloor=0x1029ce2c, seed=0x102f7bd0)

# The Mac's offsets and sizes, in the order the library exports the host's.
MAC = {
    "inst.mType": 4, "inst.mState": 8, "inst.mFAC": 0xac, "inst.mQB0": 0xb0, "inst.mQD0": 0xd0, "inst.mFF0": 0xf0, "inst.mFF8": 0xf8,
    "inst.mFFC": 0xfc, "inst.mTrains": 0x128, "inst.mLastBrace": 0x54, "inst.size": 0x150,
    "q.cap": 0, "q.block": 4, "q.words": 8,
    "tr.flags": 0, "tr.type": 4, "tr.f08": 8, "tr.n": 0xc, "tr.f10": 0x10, "tr.f14": 0x14, "tr.f18": 0x18, "tr.f1c": 0x1c,
    "tr.front": 0x20, "tr.rear": 0x24, "tr.cars": 0x28, "tr.size": 0x80,
    "car.type": 4, "car.ride": 0xc, "car.curA": 0x28, "car.curB": 0x2c, "car.cntA": 0x30, "car.cntB": 0x34, "car.f40": 0x40, "car.f44": 0x44,
    "car.size": 0x60,
    "ct.f24": 0x24, "ct.f28": 0x28, "ct.block": 0x2c, "ct.size": 0x40,
    "ty.f2f8": 0x2f8, "ty.f318": 0x318, "ty.f31c": 0x31c, "ty.base": 0x2f0, "ty.size": 0x340,
    "set.heads": 8, "set.size": 0x100,
    "br.prev": 0x64, "br.f90": 0x90, "br.size": 0x110,
    "sp.ctrl": 4, "sp.size": 0x10,
    "bc.f08": 8, "bc.size": 0x60,
}
HOSTO = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_disable(e):
    e.cl.rec("DisableRideHead", (e.u32(e.r(3)), e.r(4) & MASK))


def stub_enable(e):
    e.cl.rec("EnableRideHead", (e.u32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def stub_setcap(e):
    L = e.cl
    L.rec("SetTrainCapacity", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK))


STUBS.internal(DISABLE, stub_disable)
STUBS.internal(ENABLE, stub_enable)
STUBS.internal(SETCAP, stub_setcap)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.inst = e.alloc(MAC["inst.size"])
        self.L = e.cl = CL.PpcLog({self.inst: 1}, case["queues"])
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

    def call(self, name, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN[name], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = HOSTO
        self.inst = self.alloc(HOSTO["inst.size"])
        CL.host_begin(lib, {1: self.inst}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_u_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_u_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, args):
        return getattr(self.lib, "tpw_u_" + name)(*[a & 0xFFFFFFFFFFFFFFFF for a in args]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_u_addr.restype, lib.tpw_u_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_u_alloc.restype, lib.tpw_u_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_u_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, n in (("kick", 1), ("unload", 3), ("load", 4), ("setcap", 2), ("entr", 1), ("cars", 4), ("handle", 2)):
        f = getattr(lib, "tpw_u_" + name)
        f.restype = C.c_uint32
        f.argtypes = [C.c_uint64] * n
    out = (C.c_int64 * 128)()
    n = lib.tpw_u_layout(out)
    assert n == len(MAC), (n, len(MAC))
    HOSTO.update(zip(MAC, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_queue(s, base, q):
    O = s.O
    blk = s.alloc(4 * q["cap"] + 16)
    s.words(blk, q["items"])
    s.words(base + O["q.cap"], [q["cap"]])
    s.wp(base + O["q.block"], blk)
    s.words(base + O["q.words"], [0, q["count"], q["count2"], blk + 4 * q["rd"], blk + 4 * q["wr"], 0])
    return blk


def build(s, case):
    w = case["w"]
    O = s.O
    inst = s.inst
    s.words(s.g("capfloor"), [w["capfloor"]])
    s.words(s.g("kickfloor"), [w["kickfloor"]])
    s.words(s.g("seed"), [w["seed"]])
    s.wp(s.g("table") + w["k"] * s.psz, inst)
    s.words(inst + O["inst.mState"], [w["state"]])
    s.words(inst + O["inst.mFAC"], [w["fac"]])
    s.words(inst + O["inst.mFF0"], [w["ff0"]])
    s.words(inst + O["inst.mFF8"], [w["lo"]])
    s.words(inst + O["inst.mFFC"], [w["hi"]])
    s.qblk = [build_queue(s, inst + O["inst.mQB0"], w["qb"]), build_queue(s, inst + O["inst.mQD0"], w["qd"])]
    # the type
    ty = s.alloc(O["ty.size"])
    s.words(ty + O["ty.f2f8"], [w["heads"]])
    s.words(ty + O["ty.f318"], [w["v318"]])
    s.words(ty + O["ty.f31c"], [w["f31c"]])
    s.wp(inst + O["inst.mType"], ty)
    # the ride anim sets and their meshes
    for ride in (1, 2, 3):
        mesh = s.alloc(0x40)
        s.words(mesh, [0x100 + ride])
        st = s.alloc(O["set.size"])
        s.wp(st + O["set.heads"], mesh)
        s.wp(s.g("sets") + ride * s.psz, st)
    # the car types
    cts = []
    for c in w["cts"]:
        ct = s.alloc(O["ct.size"])
        s.words(ct + O["ct.f24"], [c["f24"]])
        s.words(ct + O["ct.f28"], [c["f28"]])
        idx = s.alloc(32)
        s.words(idx, c["idx"])
        s.wp(ct + O["ct.block"], idx)
        cts.append(ct)
    # the train and its cars
    tr = s.train = s.alloc(O["tr.size"])
    s.wp(inst + O["inst.mTrains"], tr)
    for key, name in (("flags", "tr.flags"), ("f08", "tr.f08"), ("n", "tr.n"), ("f10", "tr.f10"), ("f14", "tr.f14"), ("f18", "tr.f18"),
                      ("f1c", "tr.f1c"), ("front", "tr.front"), ("rear", "tr.rear")):
        s.words(tr + O[name], [w["tr"][key]])
    s.wp(tr + O["tr.type"], ty + O["ty.base"])
    cars = s.cars = s.alloc(8 * O["car.size"] + 16)
    s.wp(tr + O["tr.cars"], cars)
    s.slots = {}
    s.slotlist = []
    for j, c in enumerate(w["cars"]):
        car = cars + j * O["car.size"]
        s.wp(car + O["car.type"], cts[c["ct"]])
        s.words(car + O["car.ride"], [c["ride"]])
        ha, hb = s.alloc(32), s.alloc(32)
        s.words(ha, c["hA"])
        s.words(hb, c["hB"])
        s.slots[ha], s.slots[hb] = "A%d" % j, "B%d" % j
        s.slotlist.append((ha, hb))
        s.wp(car + O["car.curA"], ha)
        s.wp(car + O["car.curB"], hb)
        s.words(car + O["car.cntA"], [c["cntA"]])
        s.words(car + O["car.cntB"], [c["cntB"]])
        s.words(car + O["car.f40"], [c["f40"]])
        s.words(car + O["car.f44"], [c["f44"]])
    # the braces: last -> prev -> prev; the floats sit in the first control of the first and third braces' splines
    braces = []
    for i in range(3):
        br = s.alloc(O["br.size"])
        sp = s.alloc(O["sp.size"])
        ctl = s.alloc(O["bc.size"])
        s.words(ctl + O["bc.f08"], [w["bf"][i]])
        s.wp(sp + O["sp.ctrl"], ctl)
        s.wp(br + O["br.f90"], sp)
        braces.append(br)
    s.wp(braces[0] + O["br.prev"], braces[1])
    s.wp(braces[1] + O["br.prev"], braces[2])
    s.wp(inst + O["inst.mLastBrace"], braces[0])
    s.outs = s.alloc(16)
    s.words(s.outs, [0xDEAD0001, 0xDEAD0002])


def div96_model(base):
    d = s32((0 - base) & MASK)
    hi = (0x2AAAAAAB * d) >> 32
    hi >>= 4
    return (hi + (1 if hi < 0 else 0)) & MASK


def execute(s, case):
    s.call_raw = s.call
    s.call = lambda name, args: (s.call_raw(name, args), 0)[1]    # every function here is void (CoasterSetCapacity's answer is not compared)
    op, a = case["op"], case["args"]
    if op == "kick":
        return s.call("kick", [s.inst])
    if op == "unload":
        return s.call("unload", [s.inst, s.train, a[0]])
    if op == "load":
        return s.call("load", [s.inst, s.train, a[0], a[1]])
    if op == "setcap":
        s.call("setcap", [a[0], a[1]])
        return 0
    if op == "entr":
        return s.call("entr", [s.inst])
    if op == "cars":
        return s.call("cars", [s.inst, s.train, s.outs, s.outs + 4])
    return s.call("handle", [s.inst, s.train])


def observe(s, case, answer):
    w = case["w"]
    O = s.O
    inst = s.inst
    o = {"log": s.log(), "answer": answer, "seed": s.rd_words(s.g("seed"), 1)}
    o["ff0"] = s.rd_words(inst + O["inst.mFF0"], 1)
    o["lo"] = [canon_f(x) for x in s.rd_words(inst + O["inst.mFF8"], 1)]
    o["hi"] = [canon_f(x) for x in s.rd_words(inst + O["inst.mFFC"], 1)]
    for name, base, blk, q in (("qb", inst + O["inst.mQB0"], s.qblk[0], w["qb"]), ("qd", inst + O["inst.mQD0"], s.qblk[1], w["qd"])):
        ws = s.rd_words(base + O["q.words"], 6)
        ws[3] = (ws[3] - blk) & MASK
        ws[4] = (ws[4] - blk) & MASK
        o[name] = ws
        o[name + "ring"] = s.rd_words(blk, q["cap"])
        o[name + "cap"] = s.rd_words(base + O["q.cap"], 1)
    o["train"] = [s.rd_word(s.train + O[k]) for k in ("tr.flags", "tr.f08", "tr.n", "tr.f10", "tr.f14", "tr.f18", "tr.f1c")]
    carsout = []
    for j in range(8):
        car = s.cars + j * O["car.size"]
        ha, hb = s.slotlist[j]
        carsout.append([s.slots.get(s.rp(car + O["car.curA"]), "?"), s.slots.get(s.rp(car + O["car.curB"]), "?"),
                        s.rd_word(car + O["car.cntA"]), s.rd_word(car + O["car.cntB"]), s.rd_words(ha, 8), s.rd_words(hb, 8)])
    o["carsout"] = carsout
    if case["op"] == "cars":
        none = div96_model(s.cars & MASK)
        o["outs"] = ["none" if x == none else x for x in s.rd_words(s.outs, 2)]
    return o


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def fl(r, lo=-5.0, hi=5.0):
    return fb(r.uniform(lo, hi))


def queue_spec(r, cap=None):
    cap = r.randint(1, 6) if cap is None else cap
    cnt = r.randint(0, cap)
    rd = r.randrange(cap)
    return {"cap": cap, "count": cnt, "count2": r.choice([0, 0, 0, 1, 2, 3]), "rd": rd,
            "wr": (rd + cnt) % cap if r.random() < 0.8 else r.randrange(cap),
            "items": [r.choice([0, r.randrange(1, 5000), r.randrange(1, 5000)]) for _ in range(cap)]}


def world(r, op, over=None):
    over = over or {}
    ncars = r.randint(1, 4) if op == "handle" else r.randint(0, 4)
    lo = r.uniform(-5, 5)
    hi = lo + r.uniform(0, 6)
    if op != "handle" and r.random() < 0.1:
        hi = lo - r.uniform(0, 3)
    front = lo + r.choice([r.uniform(-2, 4), 0.0])
    rear = hi + r.choice([r.uniform(-4, 2), 0.0])
    w = {
        "k": r.randrange(256), "capfloor": r.randint(-2, 8), "kickfloor": r.randint(-2, 6), "seed": r.getrandbits(32),
        "state": r.choice([0, 4, 4, 8, 0xc, 0xe]), "fac": fb(r.choice([1.0, 2.0, 3.0, 0.5, 1.5])), "ff0": r.randint(0, 10),
        "lo": fb(lo), "hi": fb(hi), "qb": queue_spec(r), "qd": queue_spec(r),
        "heads": r.randint(0, 10), "v318": r.randint(0, 10), "f31c": r.choice([fb(0.0), fb(1.0), fl(r, 0, 1), fl(r, 0, 1)]),
        "cts": [{"f24": fl(r, 0, 3), "f28": fl(r, 0, 3), "idx": [r.randrange(30) for _ in range(8)]} for _ in range(2)],
        "tr": {"flags": r.choice([0, 0x20, 0x2, 0x22, 0x40, 0x62]), "f08": r.randint(0, 2), "n": ncars, "f10": r.randint(0, 20),
               "f14": r.randint(0, 6), "f18": r.randint(0, ncars), "f1c": r.randint(0, 5), "front": fb(front), "rear": fb(rear)},
        "cars": [{"ct": r.randrange(2), "ride": r.randint(1, 3), "cntA": r.randint(0, 5), "cntB": r.randint(0, 5),
                  "f40": fl(r, -6, 6), "f44": r.randint(0, 5),
                  "hA": [r.randrange(1, 9000) for _ in range(8)], "hB": [r.randrange(1, 9000) for _ in range(8)]} for _ in range(8)],
        "bf": [fl(r, -10, 10) for _ in range(3)],
    }
    if op == "handle":
        # at least one car in the station (otherwise GetCarsInStation answers an address-dependent value)
        j = r.randint(w["tr"]["f08"], w["tr"]["f08"] + ncars - 1)
        w["hi"] = max(w["hi"], w["lo"]) if False else w["hi"]
        w["fac"] = fb(r.choice([1.0, 2.0, 3.0, 0.5]))
        flo = struct.unpack(">f", struct.pack(">I", w["lo"]))[0]
        fhi = struct.unpack(">f", struct.pack(">I", w["hi"]))[0]
        w["cars"][j]["f40"] = fb((flo + fhi) / 2)
        for c in w["cts"]:
            c["f24"], c["f28"] = fl(r, 0, 3), fl(r, 0, 3)
    for k, v in over.items():
        if k in w:
            w[k] = v
        else:
            w["tr"][k] = v
    return w


def mk(r, op, args=(), **over):
    return {"op": op, "queues": {}, "w": world(r, op, over), "args": list(args)}


def edges(op, make):
    def gen():
        r = random.Random(sum(map(ord, op)) * 7 + 1)
        for c in make(r):
            yield c
    return gen


def rnd(op, args):
    def gen(r):
        return mk(r, op, args(r))
    return gen


def e_kick(r):
    for kf in (-1, 0, 2, 9):
        for _ in range(5):
            yield mk(r, "kick", kickfloor=kf)
    for cap in (1, 2):
        for _ in range(4):
            yield mk(r, "kick", qd=queue_spec(r, cap), kickfloor=1)


def e_unload(r):
    for flag in (0, 1, -1, 2):
        for _ in range(8):
            yield mk(r, "unload", [flag])


def e_load(r):
    for f14 in (0, 1, 3, 6):
        for count in (0, 1, 2, 4):
            yield mk(r, "load", [r.randint(0, 3), count], f14=f14)


def e_setcap(r):
    for _ in range(40):
        w = mk(r, "setcap", [0, 0])
        ww = w["w"]
        # make the clamped capacity equal the current one now and then
        cap = r.randint(-3, 12)
        v = max(ww["capfloor"], cap)
        v = min(v, ww["v318"], ww["heads"])
        if r.random() < 0.5:
            ww["ff0"] = v
        w["args"] = [ww["k"], cap]
        yield w


def rnd_setcap(r):
    c = mk(r, "setcap")
    c["args"] = [c["w"]["k"], r.randint(-3, 12)]
    return c


def e_entr(r):
    for t in (0.0, 1.0, 0.5, 0.25):
        for _ in range(4):
            yield mk(r, "entr", f31c=fb(t))


def e_cars(r):
    for _ in range(12):
        yield mk(r, "cars")
    # the compares meet exactly: a car whose edge sits on the window's ends
    for _ in range(8):
        c = mk(r, "cars")
        w = c["w"]
        w["hi"] = w["lo"]
        w["fac"] = fb(1.0)
        j = w["tr"]["f08"]
        for ct in w["cts"]:
            ct["f24"] = ct["f28"] = fb(0.0)
        for car in w["cars"]:
            car["f40"] = w["lo"]
        yield c
    for _ in range(4):
        c = mk(r, "cars")
        c["w"]["hi"] = fb(-100.0)
        c["w"]["lo"] = fb(100.0)
        yield c


def e_handle(r):
    for flags in (0, 0x20):
        for state in (0, 4):
            for inst in (True, False):
                for _ in range(3):
                    c = mk(r, "handle", flags=flags, state=state)
                    w = c["w"]
                    flo = struct.unpack(">f", struct.pack(">I", w["lo"]))[0]
                    fhi = struct.unpack(">f", struct.pack(">I", w["hi"]))[0]
                    if inst:
                        w["tr"]["front"], w["tr"]["rear"] = fb(flo + 1), fb(fhi - 1)
                    else:
                        w["tr"]["front"], w["tr"]["rear"] = fb(flo - 1), fb(fhi + 1)
                    yield c
    for _ in range(4):
        c = mk(r, "handle")
        w = c["w"]
        w["tr"]["front"], w["tr"]["rear"] = w["lo"], w["hi"]
        yield c


TESTS = [
    FnTest("KickPeepsFromEntranceQueue", ppc, host, edge=edges("kick", e_kick), rnd=rnd("kick", lambda r: [])),
    FnTest("UnloadTrain", ppc, host, edge=edges("unload", e_unload), rnd=rnd("unload", lambda r: [r.choice([0, 1, 1, -1, 2])])),
    FnTest("LoadTrain", ppc, host, edge=edges("load", e_load), rnd=rnd("load", lambda r: [r.randint(0, 3), r.randint(0, 4)])),
    FnTest("CoasterSetCapacity", ppc, host, edge=edges("setcap", e_setcap),
           rnd=lambda r: rnd_setcap(r)),
    FnTest("SetEntranceLinearSamples", ppc, host, edge=edges("entr", e_entr), rnd=rnd("entr", lambda r: [])),
    FnTest("GetCarsInStation", ppc, host, edge=edges("cars", e_cars), rnd=rnd("cars", lambda r: [])),
    FnTest("HandleTrainLoading", ppc, host, edge=edges("handle", e_handle), rnd=rnd("handle", lambda r: [])),
]
