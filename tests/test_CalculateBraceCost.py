"""Unit v5943: the track editor's brace cost, the editor switch and the left click, against libtpw_unit_CalculateBraceCost.dylib
(tests/unit_CalculateBraceCost.cpp), main app ("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  CalculateBraceCost 0x10058924, SetCoasterEdit 0x100585f4 (the tag_sCoasterInstance* overload), CoasterAcceptLeftMouseClick 0x10057f80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4-byte pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers, the host offsets of the structs come from
tpw_cbc_layout) for the translation; the same code reads both back. The callees outside the unit (RecordAction, CloseObject, Reopen,
SetCoasterState, SetCurrentEditCoaster, DisableCurrentPylonEditState, FlagGenerateTrack, GetHeightForInterval, GetPointedAtRide, StartMovePylon,
the Toggle* functions, DoDeleteMultiPylons) are fakes on both sides that record their arguments (blocks as labels) and answer from the case;
SetCurrentEditCoaster stores the edit instance, FlagGenerateTrack may swap it (to see that the original reloads it). sqrt is the C one.

The world of a case: the edit coaster global, the tool word and input flags, the highlight flag, the ride table, the instance, brace, pylon and
control blocks of the case. What is compared: the answer, the globals the functions write (edit instance, drag brace, enabled and down words),
the event log.

Fused operations: CalculateBraceCost has fmadd and fmadds (ROUNDING hooks them). Not tested: a NULL edit coaster or a NULL next brace in
CoasterAcceptLeftMouseClick / CalculateBraceCost (read unconditionally), a NULL coaster object in SetCoasterEdit.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CalculateBraceCost"
MASK = 0xFFFFFFFF

FN_CBC, FN_SCE, FN_CAL = 0x10058924, 0x100585F4, 0x10057F80
SIZES = {FN_CBC: 432, FN_SCE: 296, FN_CAL: 1372}
OWN = set(SIZES)

CALLEES = dict(RecordAction=0x101441FC, CloseObject=0x1010ABCC, Reopen=0x1010ACDC, Disable=0x10053B58, Flag=0x10040F08, Height=0x10040EB8,
               Ride=0x10067254, State=0x10042BB8, SetEdit=0x1003ACA0, Start=0x10057738, Loop=0x10052E5C, Bank=0x100532F4, Loft=0x10052AC4,
               Rotate=0x100530A8, SwapDown=0x10053898, SwapUp=0x10053720, Delete=0x10053F98)

ADDR = dict(edit=0x102B68B0, enabled=0x102C16D4, down=0x102C16D8, w16e0=0x102C16E0, drag=0x102C178C, iflags=0x102C1794, keep=0x102C6CC8,
            rides=0x102C79A4, rec=0x1037EC44 + 0x24)

# the structure fields the module writes, with the Mac offsets (the host's come from tpw_cbc_layout in the same order)
FIELDS = [("i.objid", 0x14), ("i.type", 4), ("i.state", 8), ("i.cobj", 0x28), ("i.b3c", 0x3C), ("i.f40", 0x40), ("i.last", 0x54), ("i.pyl", 0x4C),
          ("b.flags", 8), ("b.next", 0x68), ("b.x", 0x4C), ("b.z", 0x54), ("b.fe8", 0xE8), ("b.data", 0x100), ("b.w70", 0x70),
          ("d.flags", 4), ("p.ctl", 0x18), ("p.other", 0x2C),
          ("c.flags", 0), ("c.f5c", 0x5C), ("c.f60", 0x60), ("c.f64", 0x64), ("t.A", 0xA8),
          ("n.flags", 4), ("n.next", 0x28), ("n.count", 0x30), ("n.braces", 0x34)]
MAC_OFF = dict(FIELDS)
NFIELDS = len(FIELDS)

BLOCKS = ["i0", "i1", "cobj0", "cobj1", "rideA", "rideB", "rideC", "n0", "n1", "n2", "b0", "b1", "b2", "b3", "b4", "d0", "d1", "d2", "d3", "d4",
          "pyl", "pyl2", "ctl", "ctl2", "type", "fb", "fc", "fd", "fe"]
LAB = {n: i + 1 for i, n in enumerate(BLOCKS)}
BSIZE = 0x1000
CUR = [None]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, v):
    return e.cl.lab(v & MASK)


def st_record(e):
    e.cl.rec("RecordAction", (s32(e.r(4)), e.r(5) & 0xFFFF))


def st_close(e):
    e.cl.rec("CloseObject", (lab(e, e.r(3)),))


def st_reopen(e):
    e.cl.rec("Reopen", (lab(e, e.r(3)),))


def st_state(e):
    e.cl.rec("SetCoasterState", (lab(e, e.r(3)), e.r(4) & MASK))


def st_setedit(e):
    e.cl.rec("SetCurrentEditCoaster", (lab(e, e.r(3)),))
    e.write(ADDR["edit"], struct.pack(">I", e.r(3) & MASK))


def st_flag(e):
    e.cl.rec("FlagGenerateTrack", (lab(e, e.r(3)), s32(e.r(4))))
    swap = e.cl.ret("FlagSwap")
    if swap != 0:
        e.write(ADDR["edit"], struct.pack(">I", CUR[0].addr(swap)))


def st_height(e):
    e.cl.rec("GetHeightForInterval", (fb(e.f(1)), lab(e, e.r(4))))
    e.set_f1(bitsf(e.cl.ret("Height") & MASK))


def st_ride(e):
    e.cl.rec("GetPointedAtRide", ())
    e.set_r(3, CUR[0].addr(e.cl.ret("Ride")))


def st_start(e):
    e.cl.rec("StartMovePylon", (lab(e, e.r(3)),))
    e.set_r(3, 1 if e.cl.ret("StartMovePylon") != 0 else 0)


def simple(name):
    def fn(e):
        e.cl.rec(name, ())
        e.set_r(3, 1)
    return fn


STUBS.internal(CALLEES["RecordAction"], st_record)
STUBS.internal(CALLEES["CloseObject"], st_close)
STUBS.internal(CALLEES["Reopen"], st_reopen)
STUBS.internal(CALLEES["State"], st_state)
STUBS.internal(CALLEES["SetEdit"], st_setedit)
STUBS.internal(CALLEES["Disable"], simple("DisableCurrentPylonEditState"))
STUBS.internal(CALLEES["Flag"], st_flag)
STUBS.internal(CALLEES["Height"], st_height)
STUBS.internal(CALLEES["Ride"], st_ride)
STUBS.internal(CALLEES["Start"], st_start)
for key, name in (("Loop", "ToggleLoopTurnPylonState"), ("Bank", "ToggleBankInclinePylonState"), ("Loft", "ToggleLoftPylonState"),
                  ("Rotate", "ToggleRotatePylonState"), ("SwapDown", "ToggleSwapDownPylonState"), ("SwapUp", "ToggleSwapUpPylonState"),
                  ("Delete", "DoDeleteMultiPylons")):
    STUBS.internal(CALLEES[key], simple(name))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def blk_alloc(self):
        self.blk = {n: self.alloc(BSIZE) for n in BLOCKS}
        self.by_addr = {a: LAB[n] for n, a in self.blk.items()}

    def addr(self, label):
        return self.blk[BLOCKS[label - 1]] if label else 0

    def label_of(self, a):
        return 0 if a == 0 else self.by_addr.get(a, -1)

    def wptr(self, a, label):
        self.write(a, struct.pack(self.pfmt, self.addr(label)))

    def rptr(self, a):
        return self.label_of(struct.unpack(self.pfmt, self.read(a, self.pw))[0])

    def b(self, name):
        return self.blk[name]

    def fld(self, name, field):
        return self.blk[name] + self.o[field]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pw = 4
    o = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        CUR[0] = self
        self.blk_alloc()
        self.L = e.cl = CL.PpcLog({a: LAB[n] for n, a in self.blk.items()}, case["queues"])
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

    def publish(self):
        pass

    def call(self, entry, args):
        with ROUNDING(self.e):
            return T.call_ppc(self.e, entry, args)

    def cbc(self, inst, a, brace, b, c, d, e):
        r3, _, f1 = self.call(FN_CBC, [self.addr(inst), a & MASK, self.addr(brace), self.addr(b), self.addr(c), self.addr(d), self.addr(e)])
        return canon_f(fb(f1))

    def sce(self, inst, flag):
        self.call(FN_SCE, [self.addr(inst), flag & MASK])
        return None

    def cal(self, down):
        r3, _, _ = self.call(FN_CAL, [down & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pw = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.offs
        self.blk_alloc()
        CL.host_begin(lib, {LAB[n]: a for n, a in self.blk.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cbc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cbc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def publish(self):
        base = self.g("labels")
        for n, a in self.blk.items():
            self.write(base + 8 * LAB[n], struct.pack("<Q", a))

    def cbc(self, inst, a, brace, b, c, d, e):
        return canon_f(self.lib.tpw_cbc_calc(self.addr(inst), s32(a), self.addr(brace), self.addr(b), self.addr(c), self.addr(d), self.addr(e)))

    def sce(self, inst, flag):
        self.lib.tpw_cbc_sce(self.addr(inst), s32(flag))
        return None

    def cal(self, down):
        return self.lib.tpw_cbc_cal(s32(down)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("calc", C.c_uint32, [C.c_uint64, C.c_int32] + [C.c_uint64] * 5),
                            ("sce", None, [C.c_uint64, C.c_int32]), ("cal", C.c_uint32, [C.c_int32])):
        f = getattr(lib, "tpw_cbc_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_cbc_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 64)()
    n = lib.tpw_cbc_layout(out)
    vals = list(out)[:n]
    assert vals[NFIELDS] == 8, vals
    assert all(0 <= v < BSIZE for v in vals), vals
    lib.offs = {name: vals[i] for i, (name, _) in enumerate(FIELDS)}
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.publish()
    op = case["op"]
    if op == "cbc":
        build_cbc(s, case)
    elif op == "sce":
        build_sce(s, case)
    else:
        build_cal(s, case)


def build_cbc(s, c):
    i0 = s.b("i0")
    s.wptr(i0 + s.o["i.type"], LAB["type"])
    ta = s.b("type") + s.o["t.A"]
    s.words(ta + s.o["c.flags"], [c["typeA"][0]])
    s.words(ta + s.o["c.f5c"], c["typeA"][1:2])
    s.words(ta + s.o["c.f60"], c["typeA"][2:3])
    s.words(ta + s.o["c.f64"], c["typeA"][3:4])
    # brace b0: x, z, loft, next brace b1, the pylon "pyl"
    b0 = s.b("b0")
    s.words(b0 + s.o["b.x"], [c["b0"][0]])
    s.words(b0 + s.o["b.z"], [c["b0"][1]])
    s.words(b0 + s.o["b.fe8"], [c["b0"][2]])
    s.wptr(b0 + s.o["b.next"], LAB["b1"])
    s.wptr(b0 + s.o["b.data"], LAB["pyl"] if c["data"] else 0)
    b1 = s.b("b1")
    s.words(b1 + s.o["b.x"], [c["b1"][0]])
    s.words(b1 + s.o["b.z"], [c["b1"][1]])
    s.words(b1 + s.o["b.flags"], [c["b1"][2]])
    pyl = s.b("pyl")
    s.wptr(pyl + s.o["p.ctl"], LAB["ctl"] if c["ctlp"] else 0)
    s.wptr(pyl + s.o["p.other"], LAB["pyl2"] if c["other"] else 0)
    s.wptr(s.b("pyl2") + s.o["p.ctl"], LAB["ctl2"] if c["other"] == 2 else 0)
    for name, spec in (("ctl", c["ctl"]), ("ctl2", c["ctl2"])):
        a = s.b(name)
        s.words(a + s.o["c.flags"], [spec[0]])
        s.words(a + s.o["c.f5c"], [spec[1]])
        s.words(a + s.o["c.f60"], [spec[2]])
        s.words(a + s.o["c.f64"], [spec[3]])
    for name, v in zip(("fb", "fc", "fd", "fe"), c["pf"]):
        s.words(s.b(name), [v])


def build_inst(s, name, spec, cobj):
    a = s.b(name)
    s.words(a + s.o["i.objid"], [spec["objid"]])
    s.words(a + s.o["i.state"], [spec["state"]])
    s.words(a + s.o["i.b3c"], [spec["b3c"]])
    s.wptr(a + s.o["i.cobj"], LAB[cobj])


def build_sce(s, c):
    build_inst(s, "i0", c["i0"], "cobj0")
    build_inst(s, "i1", c["i1"], "cobj1")
    s.wptr(s.g("edit"), [0, LAB["i0"], LAB["i1"]][c["edit"]])
    s.words(s.g("enabled"), [c["enabled"]])
    s.words(s.g("down"), [c["down"]])


def build_cal(s, c):
    s.words(s.g("rec"), [c["rec"]])
    s.words(s.g("w16e0"), [c["mode"]])
    s.words(s.g("iflags"), [c["iflags"]])
    s.words(s.g("keep"), [c["keep"]])
    s.wptr(s.g("drag"), c["drag"])
    for i, label in enumerate(c["rides"]):
        s.wptr(s.g("rides") + s.pw * i, label)
    s.wptr(s.g("edit"), LAB["i1"])
    i1 = s.b("i1")
    s.wptr(i1 + s.o["i.last"], LAB["b0"] if c["last"] else 0)
    s.words(i1 + s.o["i.f40"], [c["f40"]])
    nodes = c["nodes"]
    s.wptr(i1 + s.o["i.pyl"], LAB["n0"] if nodes else 0)
    for k, node in enumerate(nodes):
        a = s.b("n%d" % k)
        s.words(a + s.o["n.flags"], [node["flags"]])
        s.words(a + s.o["n.count"], [len(node["braces"])])
        s.wptr(a + s.o["n.next"], LAB["n%d" % (k + 1)] if k + 1 < len(nodes) else 0)
        for j, bl in enumerate(node["braces"]):
            s.wptr(a + s.o["n.braces"] + s.pw * j, bl)
    for k, br in enumerate(c["braces"]):
        a = s.b("b%d" % k)
        s.words(a + s.o["b.w70"], [br["w70"]])
        s.words(a + s.o["b.flags"], [br["flags"]])
        s.wptr(a + s.o["b.data"], LAB["d%d" % k])
        s.words(s.b("d%d" % k) + s.o["d.flags"], [br["dflags"]])


def execute(s, c):
    if c["op"] == "cbc":
        L = [LAB["i0"], c["a"], LAB["b0"] if c["brace"] else 0]
        p = c["ptrs"]
        return s.cbc(LAB["i0"], c["a"], LAB["b0"] if c["brace"] else 0, LAB["fb"] if p[0] else 0, LAB["fc"] if p[1] else 0,
                     LAB["fd"] if p[2] else 0, LAB["fe"] if p[3] else 0)
    if c["op"] == "sce":
        return s.sce(LAB["i0"] if c["inst"] else 0, c["flag"])
    s.cal(c["down"])   # void in the headers; the answer is StartMovePylon's, which the call log compares
    return 0


def observe(s, c, answer):
    return {"answer": answer, "log": s.log(), "edit": s.rptr(s.g("edit")), "drag": s.rptr(s.g("drag")),
            "enabled": s.rd_words(s.g("enabled"), 1), "down": s.rd_words(s.g("down"), 1)}


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

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
CTL_FLAGS = [0, 4, 0x4000, 0x8000, 0xC000, 0x4004, 0xFFFFFFFF, 0x1000]


def rf(r):
    k = r.random()
    if k < 0.05:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-100, 100))
    if k < 0.9:
        return fb(r.uniform(-1000, 1000))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-6, 6))


# ---- CalculateBraceCost
def cbc_case(r, **kw):
    c = {"op": "cbc"}
    c["a"] = r.choice([0, 1, -1, 5])
    c["brace"] = r.random() > 0.05
    c["data"] = r.random() > 0.05
    c["ctlp"] = r.random() > 0.05
    c["other"] = r.choice([0, 1, 2, 2])
    c["ctl"] = [r.choice(CTL_FLAGS), rf(r), rf(r), rf(r)]
    c["ctl2"] = [r.choice(CTL_FLAGS), rf(r), rf(r), rf(r)]
    c["typeA"] = [r.choice(CTL_FLAGS), rf(r), rf(r), rf(r)]
    c["b0"] = [rf(r), rf(r), rf(r)]
    c["b1"] = [rf(r), rf(r), r.choice([0, 0x80, 0x40, 0xFFFFFFFF, 0x7F, 0x180])]
    c["ptrs"] = [r.random() < 0.5, r.random() < 0.7, r.random() < 0.5, r.random() < 0.5]
    if c["ptrs"][0]:
        c["ptrs"][1] = True
    c["pf"] = [rf(r), rf(r), rf(r), rf(r)]
    c["queues"] = {"Height": [rf(r)]}
    c.update(kw)
    return c


def cbc_edges():
    r = random.Random(5943)
    for a in (0, 1):
        for cf in (0, 0x4000, 0x8000, 0xC000):
            for other in (0, 1, 2):
                for c2f in (0, 0x4000):
                    for b1f in (0, 0x80):
                        for ptrs in ([False, False, False, False], [True, True, True, True], [True, True, False, False], [False, False, True, True]):
                            c = cbc_case(r, a=a, other=other, ptrs=ptrs, brace=True, data=True, ctlp=True)
                            c["ctl"][0] = cf
                            c["ctl2"][0] = c2f
                            c["typeA"][0] = r.choice([0, 0x4000, 0xC000])
                            c["b1"][2] = b1f
                            yield c
    for kw in (dict(brace=False), dict(data=False), dict(ctlp=False)):
        yield cbc_case(r, **kw)
    # exact values: unit costs
    for h in (0, fb(10.0), fb(-5.0)):
        c = cbc_case(r, a=1, other=0, ptrs=[False, False, False, False])
        c["queues"] = {"Height": [h]}
        c["ctl"] = [0, fb(2.5), fb(3.0), fb(0.5)]
        c["b0"] = [fb(3.0), fb(4.0), fb(1.0)]
        c["b1"] = [fb(0.0), fb(0.0), 0]
        yield c


# ---- SetCoasterEdit
def inst_spec(r):
    return {"objid": r.choice([0, 1, 0x1234, 0xFFFF, 0x10000, 0x12345678, r.randrange(1 << 32)]), "state": r.choice([0, 1, 2, 3, 0xE, 0xFFFFFFFF, r.randrange(1 << 32)]),
            "b3c": r.choice([0, 1, 2, 3, 0xFFFFFFFF, r.randrange(1 << 32)])}


def sce_case(r, **kw):
    c = {"op": "sce"}
    c["inst"] = r.random() < 0.7
    c["flag"] = r.choice([0, 1, -1, 5, 0x10000])
    c["edit"] = r.choice([0, 1, 2])
    c["i0"] = inst_spec(r)
    c["i1"] = inst_spec(r)
    c["enabled"] = r.choice([0, 1, r.randrange(1, 1 << 32)])
    c["down"] = r.choice([0, 1, r.randrange(1, 1 << 32)])
    c["queues"] = {"FlagSwap": [r.choice([0, 0, LAB["i0"], LAB["i1"]])]}
    c.update(kw)
    return c


def sce_edges():
    r = random.Random(5944)
    for inst in (False, True):
        for flag in (0, 1, -1):
            for edit in (0, 1, 2):
                for b3c in (0, 1, 2, 3):
                    for state in (0, 2):
                        for swap in (0, LAB["i0"], LAB["i1"]):
                            c = sce_case(r, inst=inst, flag=flag, edit=edit)
                            c["i0"]["b3c"] = b3c
                            c["i1"]["state"] = state
                            c["i0"]["state"] = state ^ 2
                            c["queues"] = {"FlagSwap": [swap]}
                            yield c


# ---- CoasterAcceptLeftMouseClick
MODES = [0, 1, 2, 3, 4, 5, 8, 0x10, 0x20, 0x40, 0x80, 0xFF, 0x100, 0x1FF]


def cal_case(r, **kw):
    c = {"op": "cal"}
    c["down"] = r.choice([0, 1, 1, -1, 5])
    c["rec"] = r.choice([0, 0, 0, 1, -1])
    c["mode"] = r.choice(MODES) | (r.randrange(1 << 24) << 8 if r.random() < 0.3 else 0)
    c["iflags"] = r.choice([0, 1, 2, 4, 3, 5, 6, 7, r.randrange(1 << 32)])
    c["keep"] = r.choice([0, 0, 1, -1])
    c["last"] = r.random() < 0.85
    c["f40"] = r.choice([0, 1, 0x10])
    c["braces"] = [{"w70": r.randrange(0, 6), "flags": r.choice([0, 0x40, 0x80, 0xC0, r.randrange(1 << 32)]), "dflags": r.choice([0, 1, 2, 3, 0xFFFFFFFD])}
                   for _ in range(5)]
    c["rides"] = [r.choice([0, LAB["rideA"], LAB["rideB"], LAB["rideC"]]) for _ in range(8)]
    n = r.choice([0, 1, 2, 3, 3])
    c["nodes"] = [{"flags": r.choice([0, 0, 1, 2, 0xFFFFFFFE, r.randrange(1 << 32)]),
                   "braces": [LAB["b%d" % r.randrange(5)] for _ in range(r.choice([0, 1, 2, 3, 3]))]} for _ in range(n)]
    c["drag"] = r.choice([0, LAB["b0"], LAB["b4"]])
    c["queues"] = {"Ride": [r.choice([0, LAB["rideA"], LAB["rideB"], LAB["rideC"]])], "StartMovePylon": [r.choice([0, 1, 2])]}
    c.update(kw)
    return c


def cal_edges():
    r = random.Random(5945)
    for mode in MODES:
        for down in (0, 1, -1):
            for iflags in range(8):
                for keep in (0, 1):
                    yield cal_case(r, mode=mode, down=down, iflags=iflags, keep=keep, rec=0)
    for mode in (2, 4, 8):
        yield cal_case(r, mode=mode, down=1, iflags=0, keep=0, rec=1)
    # a hit on every table entry: brace i of one node, ride j
    for w in range(6):
        c = cal_case(r, mode=0x20, down=0, rec=0, last=True)
        c["braces"][0]["w70"] = w
        c["rides"] = [LAB["rideA"]] * 8
        c["nodes"] = [{"flags": 0, "braces": [LAB["b0"]]}]
        yield c
    for mode in (2, 4, 8, 0x10, 0x20, 0x40, 0x80):
        c = cal_case(r, mode=mode, down=1 if mode in (2, 4, 8, 0x10) else 0, rec=0, keep=0, iflags=0 if mode != 0x10 else 2 ^ 2, last=True, f40=1)
        c["rides"] = [LAB["rideB"]] * 8
        c["queues"]["Ride"] = [LAB["rideB"]]
        c["nodes"] = [{"flags": 1, "braces": [LAB["b2"]]}, {"flags": 0, "braces": [LAB["b3"], LAB["b1"]]}]
        c["braces"][1]["flags"] = 0x40 if mode == 4 else 0
        c["braces"][1]["dflags"] = 0
        yield c


TESTS = [
    FnTest("CalculateBraceCost", ppc, host, edge=cbc_edges, rnd=cbc_case),
    FnTest("SetCoasterEdit", ppc, host, edge=sce_edges, rnd=sce_case),
    FnTest("CoasterAcceptLeftMouseClick", ppc, host, edge=cal_edges, rnd=cal_case),
]
