"""Unit v5366, the track editor's preview brace, against libtpw_unit_LinkPreviewBrace.dylib (tests/unit_LinkPreviewBrace.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp:
  LinkPreviewBrace 0x1003f9fc, LinkPreviewBraceToPylon 0x1003f4fc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native 8-byte pointers, so the offsets come from the library's layout
export) for the translation; the same code reads both back. The callees outside the unit are fakes on both sides that record their
arguments and answer from the case's queues: IsValidPylonPosition, LinkBrace, LinkPylon, InsertBraceIntoPylon, GetNewBraceLoftInterval,
DetermineLinkLevel, GetRideRequiredYOffset, FlagCollisionUpdateTrackAroundBrace, CorrectBraceNumbering, CalculateBraceCost and
LLCC_SystemAddToSessionCost (memcpy is the harness's real one).

The world of a case: the pointer's world position, the height field's row stride, the input flags, gCoasterWord1790, the recorder's
recording word, a list of instances with their pylon lists, a pool of braces and of pylons (each with its braces, control, cell and
position), the main instance's brace roles and block flags. What is compared: the log, the main instance's flags, the block flags, every
pylon and brace of the pool, the found pylon, the saved copies of the pylon, its braces and their heights.

Fused operations and fctiwz run under HardwareRounding on the original's code. Not tested: a pylon with more than five braces (the saved
arrays hold five), a NULL brace where the original reads one.

With the port fixes on (unit coaster64), the port fix in LinkPreviewBraceToPylon takes the type's pylon controls as
tag_sCoasterType's mPylonsB and mPylonsA instead of the words at 0x1c8 and 0xa8, so the type is laid out and its two controls labelled
at the offsets the unit reports for those fields.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_LinkPreviewBrace"
MASK = 0xFFFFFFFF

FN_LPB, FN_LPBP = 0x1003F9FC, 0x1003F4FC
SIZES = {FN_LPB: 1184, FN_LPBP: 740}
OWN = set(SIZES)

C_VALID, C_FLAG, C_LINKBRACE, C_NUMBER, C_LINKPYLON = 0x100411E4, 0x10040FA4, 0x1003E48C, 0x100564AC, 0x1003E344
C_COST, C_ADDCOST, C_LOFT, C_LEVEL, C_INSERT, C_YOFF = 0x10058924, 0x10091D50, 0x1003E108, 0x1003E7DC, 0x1003EF78, 0x101ED29C

ADDR = dict(instances=0x102B68B4, word1790=0x102C1790, input=0x102C1794, header=0x102C6C7C, pointer=0x102C6CF8,
            recording=0x1037EC44 + 0x24, saved_flag=0x102B6EFC, saved_pylon=0x102B6EB4, saved_braces=0x102B69A0,
            heights=0x102B698C, found=0x102B6F00)

LAYOUT_NAMES = ["i_type", "i_blk", "i_next", "i_pylons", "i_last", "i_braces", "i_b5c", "i_prev", "i_b64", "i_ppylon", "i_b6c", "i_f118",
                "i_size", "b_flags", "b_next", "b_loft", "b_data", "b_spline", "b_w74", "b_size", "p_flags", "p_index", "p_cx", "p_cz",
                "p_control", "p_x", "p_z", "p_count", "p_braces", "p_next", "p_size", "s_ctrl", "t_pa", "t_pb",
                "t_size"]
MAC_LAYOUT = [0x04, 0x3C, 0x18, 0x4C, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x118, 0x150,
              0x08, 0x68, 0xE8, 0x100, 0x90, 0x74, 0x110,
              0x04, 0x08, 0x0C, 0x10, 0x18, 0x1C, 0x24, 0x30, 0x34, 0x28, 0x48, 0x04, 0xA8, 0x1C8, 0x400]
MAC_O = dict(zip(LAYOUT_NAMES, MAC_LAYOUT))


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


def st_valid(e):
    L = e.cl
    L.rec("IsValid", tuple(e.r(i) & MASK for i in range(3, 10)))
    e.set_r(3, L.ret("IsValid") & MASK)


def st_linkbrace(e):
    e.cl.rec("LinkBrace", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), s32(e.r(5))))


def st_linkpylon(e):
    L = e.cl
    L.rec("LinkPylon", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))


def st_insert(e):
    L = e.cl
    sp = e.r(1)
    L.rec("Insert", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), e.r(6) & MASK, canon_f(fb(e.f(1))), s32(e.r(9)), s32(e.r(10)),
                     s32(e.u32(sp + 0x38)), s32(e.u32(sp + 0x3C))))
    e.write(e.r(8), struct.pack(">I", L.ret("InsertOut") & MASK))


def st_loft(e):
    L = e.cl
    L.rec("Loft", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), s32(e.r(7)), s32(e.r(8)), e.r(9) & MASK))
    e.set_f1(bitsf(L.ret("Loft")))


def st_level(e):
    L = e.cl
    L.rec("Level", (canon_f(fb(e.f(1))), L.lab(e.r(4)), s32(e.r(7))))
    e.write(e.r(5), struct.pack(">I", L.ret("LevelWord") & MASK))
    e.write(e.r(6), struct.pack(">I", L.ret("LevelOut") & MASK))
    e.set_r(3, 1)


def st_yoff(e):
    L = e.cl
    L.rec("YOffset", (s32(e.r(3)), s32(e.r(4)), e.r(5) & MASK, s32(e.r(6)), s32(e.r(7)), e.r(8) & MASK, s32(e.r(9))))
    e.set_f1(bitsf(L.ret("YOffset")))


def st_flag(e):
    e.cl.rec("Flag", (e.cl.lab(e.r(3)),))


def st_number(e):
    e.cl.rec("Number", (e.cl.lab(e.r(3)),))


def st_cost(e):
    L = e.cl
    L.rec("Cost", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), s32(e.r(6) - e.r(5)), s32(e.r(7) - e.r(5)), e.r(8) & MASK, e.r(9) & MASK))
    e.set_f1(bitsf(L.ret("Cost")))


def st_addcost(e):
    e.cl.rec("AddCost", (s32(e.r(3)),))


for _a, _f in ((C_VALID, st_valid), (C_FLAG, st_flag), (C_LINKBRACE, st_linkbrace), (C_NUMBER, st_number), (C_LINKPYLON, st_linkpylon),
               (C_COST, st_cost), (C_ADDCOST, st_addcost), (C_LOFT, st_loft), (C_LEVEL, st_level), (C_INSERT, st_insert),
               (C_YOFF, st_yoff)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def __init__(self):
        self.labels = {}
        self.active = False

    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def zero(self, a, n):
        self.write(a, bytes(n))

    def lab(self, a):
        if a == 0:
            return 0
        return self.labels.setdefault(a, len(self.labels) + 1)

    def lbl(self, a):
        return 0 if a == 0 else self.labels.get(a, -1)

    def obj(self, n):
        a = self.alloc(n)
        self.lab(a)
        return a

    def g(self, name):
        return ADDR[name]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    O = MAC_O
    bstride = 0x104

    def __init__(self, e, case):
        super().__init__()
        self.e = e
        self.case = case

    def begin(self):
        self.L = self.e.cl = CL.PpcLog(dict(self.labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)
        self.active = True

    def done(self):
        if self.active:
            CL.Stubs.deactivate(self.e, self.saved)
            self.active = False

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def p32(self, a):
        return a

    def log(self):
        return self.L.log

    def call_lpb(self, inst):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_LPB, [inst])

    def call_lpbp(self, inst, brace, pylon, f, flag):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_LPBP, [inst, brace, pylon, T.F(bitsf(f)), flag])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        super().__init__()
        self.lib = lib
        self.case = case
        bind(lib)
        self.O = lib.O
        self.bstride = lib.O["b_size"]

    def begin(self):
        CL.host_begin(self.lib, {v: k for k, v in self.labels.items()}, self.case["queues"])
        self.active = True

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lpb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lpb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def p32(self, a):
        return self.lib.tpw_lpb_off(a)

    def log(self):
        return CL.host_end(self.lib)

    def call_lpb(self, inst):
        self.lib.tpw_lpb_lpb(inst)

    def call_lpbp(self, inst, brace, pylon, f, flag):
        self.lib.tpw_lpb_lpbp(inst, brace, pylon, bitsf(f), s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_lpb_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "lpb").restype, getattr(lib, p + "lpb").argtypes = None, [C.c_uint64]
    getattr(lib, p + "lpbp").restype = None
    getattr(lib, p + "lpbp").argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_float, C.c_int32]
    out = (C.c_int64 * 64)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(LAYOUT_NAMES), n
    lib.O = dict(zip(LAYOUT_NAMES, list(out)[:n]))
    # the Mac-only layout facts the test relies on: the raw words of the braces and the control blocks
    assert lib.O["p_braces"] - lib.O["p_count"] >= 4
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    O, c = s.O, case
    s.zero(s.g("saved_pylon"), O["p_size"])
    s.zero(s.g("saved_braces"), 5 * s.bstride)
    s.zero(s.g("heights"), 0x14)
    s.zero(s.g("saved_flag"), 4)
    s.zero(s.g("found"), s.psz)

    typ = s.obj(max(O["t_size"], 0x400))
    s.lab(typ + O["t_pa"])
    s.lab(typ + O["t_pb"])
    ctrls = []
    for cd in c["ctrls"]:
        a = s.obj(0x100)
        s.w32(a, cd["flags"])
        s.w32(a + 0xCC, cd["limit"])
        ctrls.append(a)
    bsz = max(O["b_size"], 0x110) + 0x10
    braces = [s.obj(bsz) for _ in c["braces"]]
    pylons = [s.obj(O["p_size"] + 0x10) for _ in c["pylons"]]
    insts = [s.obj(O["i_size"] + 0x10) for _ in range(c["ninst"])]
    main = insts[c["main"]]
    s.lab(main + O["i_blk"])

    for bd, a in zip(c["braces"], braces):
        s.w32(a + O["b_flags"], bd["flags"])
        s.w32(a + 0x4C, bd["x"])
        s.w32(a + 0x50, bd["y"])
        s.w32(a + 0x54, bd["z"])
        s.w32(a + O["b_loft"], bd["loft"])
        if bd["data"] >= 0:
            s.wp(a + O["b_data"], pylons[bd["data"]])
        if bd["next"] >= 0:
            s.wp(a + O["b_next"], braces[bd["next"]])
        word = s.alloc(16)
        s.w32(word, bd["w74"])
        s.wp(a + O["b_w74"], word)
        spline = s.alloc(0x20)
        table = s.alloc(4 * 0x58 + 8)
        for i, v in enumerate(bd["tbl"]):
            s.w32(table + i * 0x58 + 0x44, v)
        s.wp(spline + O["s_ctrl"], table)
        s.wp(a + O["b_spline"], spline)

    for pd, a in zip(c["pylons"], pylons):
        s.w32(a + O["p_flags"], pd["flags"])
        s.w32(a + O["p_index"], pd["index"])
        s.w32(a + O["p_cx"], pd["cx"])
        s.w32(a + O["p_cz"], pd["cz"])
        s.w32(a + O["p_x"], pd["x"])
        s.w32(a + O["p_z"], pd["z"])
        s.w32(a + O["p_count"], len(pd["braces"]))
        for i, bi in enumerate(pd["braces"]):
            s.wp(a + O["p_braces"] + i * s.psz, braces[bi])
        s.wp(a + O["p_control"], ctrls[pd["ctrl"]])

    for k, inst in enumerate(insts):
        s.wp(inst + O["i_type"], typ)
        if k + 1 < len(insts):
            s.wp(inst + O["i_next"], insts[k + 1])
        lst = c["lists"][k]
        if lst:
            s.wp(inst + O["i_pylons"], pylons[lst[0]])
            for j in range(len(lst) - 1):
                s.wp(pylons[lst[j]] + O["p_next"], pylons[lst[j + 1]])
    s.w32(main, c["iflags"])
    s.w32(main + O["i_blk"], c["blk"])
    s.w32(main + O["i_f118"], c["f118"])
    rl = c["roles"]
    for key, name in (("prev", "i_prev"), ("b64", "i_b64"), ("b6c", "i_b6c"), ("b5c", "i_b5c"), ("last", "i_last"), ("braces", "i_braces")):
        s.wp(main + O[name], braces[rl[key]])
    s.wp(main + O["i_ppylon"], pylons[c["ppylon"]])

    s.wp(s.g("instances"), insts[0] if c["linked"] else 0)
    s.w32(s.g("word1790"), c["w1790"])
    s.w32(s.g("input"), c["inp"])
    s.w32(s.g("recording"), c["rec"])
    px, pz = c["ptr"]
    s.w32(s.g("pointer"), px)
    s.w32(s.g("pointer") + 4, 0)
    s.w32(s.g("pointer") + 8, pz)
    hdr = s.alloc(0x100)
    hf = s.alloc(0x40)
    s.w32(hf + 0x18, c["stride"])
    s.w32(hdr + 0x6C, s.p32(hf))
    s.wp(s.g("header"), hdr)
    s.world = dict(main=main, braces=braces, pylons=pylons)


def begin_and_call(s, case):
    w = s.world
    s.begin()
    if case["op"] == "lpb":
        s.call_lpb(w["main"])
    else:
        s.call_lpbp(w["main"], w["braces"][case["roles"]["b6c"]], w["pylons"][case["apylon"]], case["af"], case["acopy"])


def pylon_fields(s, a):
    O = s.O
    return [s.r32(a + O["p_flags"]), s.r32(a + O["p_index"]), s.r32(a + O["p_cx"]), s.r32(a + O["p_cz"]),
            s.lbl(s.rp(a + O["p_control"])), canon_f(s.r32(a + O["p_x"])), canon_f(s.r32(a + O["p_z"])), s.r32(a + O["p_count"]),
            [s.lbl(s.rp(a + O["p_braces"] + i * s.psz)) for i in range(5)]]


def brace_fields(s, a):
    O = s.O
    return [s.r32(a + O["b_flags"]), canon_f(s.r32(a + 0x4C)), canon_f(s.r32(a + 0x50)), canon_f(s.r32(a + 0x54)),
            canon_f(s.r32(a + O["b_loft"])), s.lbl(s.rp(a + O["b_data"])), s.lbl(s.rp(a + O["b_next"]))]


def observe(s, case):
    O, w = s.O, s.world
    main = w["main"]
    sb = s.g("saved_braces")
    return {"log": s.log(), "iflags": s.r32(main), "blk": s.r32(main + O["i_blk"]), "found": s.lbl(s.rp(s.g("found"))),
            "saved_flag": s.r32(s.g("saved_flag")), "saved_pylon": pylon_fields(s, s.g("saved_pylon")),
            "saved_braces": [brace_fields(s, sb + i * s.bstride) for i in range(5)],
            "heights": [canon_f(s.r32(s.g("heights") + 4 * i)) for i in range(5)],
            "pylons": [pylon_fields(s, a) for a in w["pylons"]], "braces": [brace_fields(s, a) for a in w["braces"]]}


def run(s, case):
    try:
        build(s, case)
        begin_and_call(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]


def fl(r, special=True):
    k = r.random()
    if special and k < 0.05:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-500, 500))
    return fb(r.uniform(-3000, 3000))


def cell_of(v):
    return int(T.f32(T.f32(v) / 10.0))


COSTS = [0.0, 0.5, -0.5, -1.5, 7.0, 123456.75, -2147483648.0, 2147483520.0, -77.9, 1.0]


def gen(r, op, found=None, hit=None, blk=None, inp=None, valid=None, rec=None, dist=None, cnt_ok=None, cflags=None, data6c=None, cost=None,
        copy=None, w1790=None, ref_data=None):
    c = {"op": op}
    nb, npy = 9, r.randint(1, 5)
    c["ctrls"] = [{"flags": r.choice([0, 0x4000, 4, 0x4004]), "limit": r.choice([0, 1, 2, 3, 4, 5, 6, 9])} for _ in range(npy + 1)]
    c["inp"] = r.choice([0, 0x40, 0x100, 0x140, r.getrandbits(32)]) if inp is None else inp
    c["w1790"] = r.choice([0, 1, 2, 2, 3]) if w1790 is None else w1790
    c["rec"] = r.choice([0, 0, 1, 0xFF]) if rec is None else rec
    px = r.choice([0.0, 10.0, -10.0, 9.99, -9.99, 15.0, -0.5, r.uniform(-300, 300), r.uniform(-300, 300)])
    pz = r.choice([0.0, 10.0, -10.0, 9.99, -9.99, 15.0, -0.5, r.uniform(-300, 300), r.uniform(-300, 300)])
    c["ptr"] = (fb(px), fb(pz))
    cx, cz = cell_of(px), cell_of(pz)
    c["stride"] = r.choice([1, 16, 64, 200, r.randrange(1, 1000)])
    index = (cx + cz * c["stride"]) & MASK
    c["blk"] = r.choice([0, 2, 3, 0x102, r.getrandbits(32)]) if blk is None else blk
    c["iflags"] = r.getrandbits(32)
    c["f118"] = fl(r)
    c["braces"] = []
    for _ in range(nb):
        c["braces"].append({"flags": r.choice([0, 0x40, 0x80, 0x800000, r.getrandbits(32)]), "x": fl(r), "y": fl(r), "z": fl(r),
                            "loft": fl(r), "data": r.choice([-1, r.randrange(npy), r.randrange(npy)]),
                            "next": -1 if r.random() < 0.15 else r.randrange(nb), "w74": r.randrange(4), "tbl": [fl(r) for _ in range(4)]})
    roles = list(range(nb))
    r.shuffle(roles)
    names = ["prev", "b64", "b6c", "b5c", "last", "braces"]
    c["roles"] = {n: roles[i] for i, n in enumerate(names)}
    if r.random() < 0.15:
        c["roles"][r.choice(names)] = r.randrange(nb)
    # the preview brace's place decides the distance test (the limit is 300 cells... 90000 on the squared distance)
    pb = c["braces"][c["roles"]["prev"]]
    if dist is None:
        dx, dz = (r.uniform(-120, 120), r.uniform(-120, 120)) if r.random() < 0.8 else (r.uniform(-400, 400), r.uniform(-400, 400))
    elif dist == "exact":
        dx, dz = 300.0, 0.0
    elif dist == "over":
        dx, dz = 300.05, 0.0
    elif dist == "far":
        dx, dz = 1000.0, -1000.0
    else:
        dx, dz = 3.0, 4.0
    pb["x"], pb["z"] = fb(10.0 * cx + dx), fb(10.0 * cz + dz)
    c["pylons"] = []
    for i in range(npy):
        match = (found is True and i == 0) or (found is None and r.random() < 0.4)
        pf = r.choice([0, 0, 1, 4, 5, 2, 8, 0x10])
        if match:
            pf = r.choice([0, 2, 8, 0x10])
        idx = index if match else (index + r.choice([1, -1, 7, 1000])) & MASK
        count = r.randint(0, 5)
        c["pylons"].append({"flags": pf, "index": idx, "cx": r.choice([0, 1, 5, 100, r.randrange(2000), r.getrandbits(32)]),
                            "cz": r.choice([0, 1, 5, 100, r.randrange(2000), r.getrandbits(32)]),
                            "x": fb(r.choice([0.0, -5.0, 5.0, 10.0, r.uniform(-2000, 2000)])), "z": fb(r.choice([0.0, -5.0, 5.0, -10.0, r.uniform(-2000, 2000)])),
                            "braces": [r.randrange(nb) for _ in range(count)], "ctrl": r.randrange(npy + 1)})
    p0 = c["pylons"][0]
    rl = c["roles"]
    others = [b for b in range(nb) if b not in (rl["prev"], rl["b64"], rl["braces"])]
    if hit is not None:
        p0["braces"] = [r.choice(others) for _ in range(r.randint(0, 3))]
        if hit != "none":
            slot = {"b64": rl["b64"], "prev": rl["prev"], "braces": rl["braces"]}[hit]
            p0["braces"].insert(r.randint(0, len(p0["braces"])), slot)
    if found is False:
        for p in c["pylons"]:
            if p["index"] == index and (p["flags"] & 5) == 0:
                p["index"] = (index + 1) & MASK
    if cnt_ok is not None:
        cnt = len(p0["braces"])
        c["ctrls"][p0["ctrl"]]["limit"] = cnt + 1 if cnt_ok else cnt
    if cflags is not None:
        c["ctrls"][p0["ctrl"]]["flags"] = cflags
    # the lists: the first pylon sits in a random list, the others anywhere or nowhere
    c["ninst"] = r.randint(1, 3)
    c["main"] = r.randrange(c["ninst"])
    c["linked"] = r.random() < 0.9
    c["lists"] = [[] for _ in range(c["ninst"])]
    for i in range(npy):
        if i == 0 and found is not False:
            c["lists"][r.randrange(c["ninst"])].append(0)
        elif r.random() < 0.6:
            c["lists"][r.randrange(c["ninst"])].append(i)
    c["ppylon"] = r.randrange(npy)
    if data6c is not None:
        c["braces"][rl["b6c"]]["data"] = r.randrange(npy) if data6c else -1
    if ref_data is not None:
        drag = c["braces"][rl["b64"] if (c["blk"] & 2) else rl["last"]]
        drag["data"] = c["apylon"] if False else -1
    q = {"IsValid": [r.choice([0, 1, 5]) if valid is None else valid], "Loft": [fl(r)], "LevelWord": [r.randrange(8)], "LevelOut": [fl(r)],
         "InsertOut": [fl(r)], "YOffset": [fl(r)], "Cost": [fb(r.choice(COSTS) if cost is None else cost)]}
    c["queues"] = q
    c["apylon"] = r.randrange(npy)
    c["af"] = fl(r)
    c["acopy"] = r.choice([0, 1]) if copy is None else copy
    if op == "lpbp" and ref_data is not None:
        drag = c["braces"][rl["b64"] if (c["blk"] & 2) else rl["last"]]
        drag["data"] = c["apylon"] if ref_data else -1
    return c


def lpb_case(r):
    return gen(r, "lpb")


def lpbp_case(r):
    return gen(r, "lpbp", ref_data=r.choice([True, False, None]))


def lpb_edges():
    r = random.Random(5366)
    for dist in ("exact", "over", "far", "near"):
        yield gen(r, "lpb", dist=dist, found=False)
    for valid in (0, 1):
        for rec in (0, 1):
            for cost in (-1.5, 0.5, 7.0, 0.0, -2147483648.0, 2147483520.0):
                yield gen(r, "lpb", found=False, valid=valid, rec=rec, cost=cost, dist="near", data6c=True)
    for d6 in (True, False):
        yield gen(r, "lpb", found=False, data6c=d6, dist="near", rec=0)
    for valid in (0, 1):
        for hit in ("b64", "prev", "braces", "none"):
            for blk in (0, 2, 3):
                yield gen(r, "lpb", found=True, hit=hit, blk=blk, valid=valid, dist="near", cnt_ok=True)
    for blk in (0, 2):
        for inp in (0, 0x40):
            for cf in (0, 0x4000):
                for cnt_ok in (True, False):
                    for rec in (0, 1):
                        yield gen(r, "lpb", found=True, hit="none", blk=blk, inp=inp, cflags=cf, cnt_ok=cnt_ok, dist="near", rec=rec,
                                  data6c=True)
    for w in (1, 2):
        yield gen(r, "lpb", found=True, hit="none", blk=2, inp=0x40, cflags=0x4000, cnt_ok=True, dist="near", w1790=w)


def lpbp_edges():
    r = random.Random(7742)
    for copy in (0, 1):
        for inp in (0, 0x40):
            for w in (1, 2):
                for blk in (0, 2):
                    for rd in (True, False):
                        yield gen(r, "lpbp", copy=copy, inp=inp, w1790=w, blk=blk, ref_data=rd)


TESTS = [
    FnTest("LinkPreviewBrace", ppc, host, edge=lpb_edges, rnd=lpb_case),
    FnTest("LinkPreviewBraceToPylon", ppc, host, edge=lpbp_edges, rnd=lpbp_case),
]
