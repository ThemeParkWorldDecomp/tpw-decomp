"""Unit v5053, the loft of a new brace and the link level of a pylon, against libtpw_unit_GetNewBraceLoftInterval.dylib
(tests/unit_GetNewBraceLoftInterval.cpp), main app ("SimTheme Park"), src/GetNewBraceLoftInterval.cpp:
  GetNewBraceLoftInterval 0x1003e108, DetermineLinkLevel 0x1003e7dc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back.
The callees outside the unit (CTRACE, CalculateBraceHeight, DistanceBetweenPoints, GetIntervalForHeight, SetLoftPylon) are fakes on both
sides that record their arguments and answer from the case's queues.

The world of a case: the edit instance's word 0x118, the input flags, the pylon records with their controls and braces, the ride set chain
for the other brace's height table. What is compared: the answer, the out float and level, the event log.

Fused operations: fmadds in both functions; HardwareRounding is aimed at them. Not tested: GetNewBraceLoftInterval with the other brace NULL
in the branch that reads it (the original then uses its caller's f31, a register the test cannot fix).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetNewBraceLoftInterval"
MASK = 0xFFFFFFFF

FN_G, FN_D = 0x1003E108, 0x1003E7DC
SIZES = {FN_G: 456, FN_D: 1000}
OWN = set(SIZES)

CTRACE = 0x1006FCBC
CALCH = 0x1003E6E0
DIST = 0x100C3198
INTERVAL = 0x10040E10
SETLOFT = 0x10055E0C

ADDR = dict(edit=0x102B68B0, flags=0x102C1794, sets=0x102C79A4)
TEXTS = {0x1023CF10: 0, 0x1023CF4D: 1, 0x1023CF8E: 2, 0x1023CFE5: 3, 0x1023D019: 4, 0x1023D055: 5}

LAYOUT_NAMES = ["pyl_control", "pyl_x", "pyl_y", "pyl_z", "pyl_other", "pyl_count", "pyl_braces", "ctl_id", "ctl_f2c", "ctl_f68",
                "ctl_f6c", "ctl_fc4", "br_flags", "br_data", "br_w70", "br_size", "dat_flags", "dat_control", "dat_size", "set_heads",
                "mesh_f28", "inst_f118", "ctl_size", "set_size", "mesh_size", "inst_size"]
MAC = dict(pyl_control=0x18, pyl_x=0x1C, pyl_y=0x20, pyl_z=0x24, pyl_other=0x2C, pyl_count=0x30, pyl_braces=0x34, ctl_id=0x0C,
           ctl_f2c=0x2C, ctl_f68=0x68, ctl_f6c=0x6C, ctl_fc4=0xC4, br_flags=8, br_data=0x100, br_w70=0x70, br_size=0, dat_flags=4,
           dat_control=0x18, dat_size=0, set_heads=8, mesh_f28=0x28, inst_f118=0x118, ctl_size=0, set_size=0, mesh_size=0, inst_size=0)


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


def stub_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3) & MASK, TEXTS.get(e.r(4) & MASK, -1), s32(e.r(5)), e.r(6) & MASK, e.r(7) & MASK))


def stub_calch(e):
    L = e.cl
    b = e.r(3)
    L.rec("CalcH", (e.u32(b), e.u32(b + 4)))
    e.set_f1(bitsf(L.ret("CalcH") & MASK))


def stub_dist(e):
    L = e.cl
    a = struct.unpack(">3I", e.read(e.r(3), 12))
    b = struct.unpack(">3I", e.read(e.r(4), 12))
    L.rec("Dist", tuple(canon_f(x) for x in a + b))
    e.set_f1(bitsf(L.ret("Dist") & MASK))


def stub_interval(e):
    L = e.cl
    c = e.r(4) & MASK
    L.rec("Interval", (canon_f(fb(e.f(1))), e.u32(c + 0x0C) if c else 0))
    e.set_f1(bitsf(L.ret("Interval") & MASK))


def stub_setloft(e):
    e.cl.rec("SetLoft", (canon_f(fb(e.f(1))), e.r(4) & MASK, s32(e.r(5)), s32(e.r(6))))


STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(CALCH, stub_calch)
STUBS.internal(DIST, stub_dist)
STUBS.internal(INTERVAL, stub_interval)
STUBS.internal(SETLOFT, stub_setloft)


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

    def ptr(self, a, value):
        self.write(a, struct.pack(self.pfmt, value))

    def blk(self, key=None):
        return self.alloc(max(0x200, self.o.get(key, 0) if key else 0))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.o = MAC
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

    def call_g(self, args):
        with ROUNDING(self.e):
            _, _, f1 = T.call_ppc(self.e, FN_G, [x & MASK for x in args])
        return canon_f(fb(f1))

    def call_d(self, h, pylon, level, out, quiet):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_D, [T.F(bitsf(h)), pylon, level, out, quiet & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.offs
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lay_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lay_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_g(self, args):
        a = args
        r = self.lib.tpw_lay_gnbli(a[0], a[1], a[2], a[3], s32(a[4]), s32(a[5]), a[6])
        return canon_f(fb(r))

    def call_d(self, h, pylon, level, out, quiet):
        return self.lib.tpw_lay_dll(bitsf(h), pylon, level, out, s32(quiet)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lay_addr.restype, lib.tpw_lay_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lay_alloc.restype, lib.tpw_lay_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lay_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lay_gnbli.restype = C.c_float
    lib.tpw_lay_gnbli.argtypes = [C.c_uint64] * 4 + [C.c_int32, C.c_int32, C.c_uint64]
    lib.tpw_lay_dll.restype = C.c_uint32
    lib.tpw_lay_dll.argtypes = [C.c_float, C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lay_layout(out)
    assert n == len(LAYOUT_NAMES), n
    lib.offs = dict(zip(LAYOUT_NAMES, list(out)[:n]))
    assert lib.offs["pyl_braces"] == lib.offs["pyl_count"] + 4 or lib.offs["pyl_braces"] == lib.offs["pyl_count"] + 8
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_control(s, ctl):
    o = s.o
    c = s.blk("ctl_size")
    s.words(c, [ctl.get("flags", 0)])
    s.words(c + o["ctl_id"], [ctl["id"]])
    s.words(c + o["ctl_f2c"], [ctl.get("f2c", 0)])
    s.words(c + o["ctl_f68"], [ctl.get("f68", 0)])
    s.words(c + o["ctl_f6c"], [ctl.get("f6c", 0)])
    s.words(c + o["ctl_fc4"], [ctl.get("fc4", 0)])
    return c


def build_brace(s, number, w4, flags, data, w70):
    o = s.o
    b = s.blk("br_size")
    s.words(b, [number, w4])
    s.words(b + o["br_flags"], [flags])
    s.ptr(b + o["br_data"], data)
    s.words(b + o["br_w70"], [w70])
    return b


def build_data(s, flags, control):
    o = s.o
    d = s.blk("dat_size")
    s.words(d + o["dat_flags"], [flags])
    s.ptr(d + o["dat_control"], control)
    return d


def build_g(s, case):
    o = s.o
    inst = s.blk("inst_size")
    s.words(inst + o["inst_f118"], [case["inst118"]])
    s.ptr(s.g("edit"), inst)
    pylon = s.blk()
    other_pylon = s.blk()
    for p, xyz in ((pylon, case["px"]), (other_pylon, case["ox"])):
        s.words(p + o["pyl_x"], [xyz[0]])
        s.words(p + o["pyl_y"], [xyz[1]])
        s.words(p + o["pyl_z"], [xyz[2]])
    s.ptr(pylon + o["pyl_other"], other_pylon)
    s.words(pylon + o["pyl_count"], [case["count"]])
    ctrl = build_control(s, {"id": 7, "flags": case["ctrl"]}) if case["ctrl"] is not None else 0
    ctrl_b = build_control(s, {"id": 8})
    brace = build_brace(s, 0x11, 0x22, 0, build_data(s, 0, ctrl_b), 0)
    other = 0
    ot = case["other"]
    if ot is not None:
        ctrl_o = build_control(s, {"id": 9, "f2c": ot["f2c"]})
        other = build_brace(s, 0x33, 0x44, 0, build_data(s, ot["dflags"], ctrl_o), ot["w70"])
        s.words(other + 0x50, [ot["f50"]])
        entry = s.blk()
        s.words(entry + 0x34, [ot["f34"]])
        table = s.blk()
        if not ot["slot_null"]:
            s.ptr(table + ot["f2c"] * 0x14 + 4, entry)
        holder = s.blk()
        s.ptr(holder + 4, table)
        mesh = s.blk("mesh_size")
        s.ptr(mesh + o["mesh_f28"], holder)
        rset = s.blk("set_size")
        s.ptr(rset + o["set_heads"], mesh)
        s.ptr(s.g("sets") + s.psz * ot["w70"], rset)
    out = s.alloc(16)
    s.words(out, [case["initout"]])
    return [pylon, ctrl, brace, other, case["a"], case["b"], out if case["hasout"] else 0], out


def build_d(s, case):
    o = s.o
    s.words(s.g("flags"), [case["inflags"]])
    ctrl = build_control(s, {"id": 5, "f68": case["f68"], "f6c": case["f6c"], "fc4": case["fc4"]})
    pylon = s.alloc(o["pyl_braces"] + 8 * 8 + 16)
    s.ptr(pylon + o["pyl_control"], ctrl)
    s.words(pylon + o["pyl_count"], [len(case["braces"])])
    for i, (num, w4, flags) in enumerate(case["braces"]):
        b = build_brace(s, num, w4, flags, 0, 0)
        s.ptr(pylon + o["pyl_braces"] + s.psz * i, b)
    level = s.alloc(16)
    s.words(level, [case["initlevel"]])
    out = s.alloc(16)
    s.words(out, [case["initout"]])
    return pylon, level, out


def run_g(s, case):
    try:
        args, out = build_g(s, case)
        ans = s.call_g(args)
        return {"answer": ans, "out": s.rd_word(out), "log": s.log()}
    finally:
        s.done()


def run_d(s, case):
    try:
        pylon, level, out = build_d(s, case)
        ans = s.call_d(case["h"], pylon, level, out, case["quiet"])
        return {"answer": ans, "level": s.rd_word(level), "out": s.rd_word(out), "log": s.log()}
    finally:
        s.done()


def ppc_g(e, case):
    return run_g(Ppc(e, case), case)


def host_g(lib, case):
    return run_g(Host(lib, case), case)


def ppc_d(e, case):
    return run_d(Ppc(e, case), case)


def host_d(lib, case):
    return run_d(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000, 0x00000001]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-3, 3))
    return fb(r.uniform(-300, 300))


def u(r, lo, hi):
    return fb(r.uniform(lo, hi))


def g_case(r, ctrl="rand", a=None, b=None, count=None, kind=None):
    c = {"op": "g"}
    c["ctrl"] = (r.choice([None, 0, 0x4000, 0x4000, 0x4000, 0x4004, 0x3FFF]) if ctrl == "rand" else ctrl)
    c["a"] = (r.choice([0, 0, 0, 1, -1]) if a is None else a)
    c["b"] = (r.choice([0, 1]) if b is None else b)
    c["count"] = (r.choice([0, 1, 2]) if count is None else count)
    c["hasout"] = r.choice([0, 1, 1])
    c["inst118"] = rf(r)
    c["initout"] = rf(r)
    c["px"] = [rf(r), rf(r), rf(r)]
    c["ox"] = [rf(r), rf(r), rf(r)]
    reach = c["ctrl"] is not None and (c["ctrl"] & 0x4000) != 0 and c["a"] == 0
    if kind is None:
        kind = r.choice(["flag", "slot", "null", "nullslot"]) if reach else r.choice(["none", "flag", "slot"])
    if kind == "none":
        c["other"] = None
    else:
        c["other"] = {"dflags": 1 if kind == "flag" else r.choice([0, 2]), "f50": rf(r), "w70": r.randrange(4), "f2c": r.randrange(3),
                      "slot_null": kind == "nullslot", "f34": rf(r)}
    c["queues"] = {"Dist": [rf(r)], "Interval": [rf(r) if r.random() < 0.4 else u(r, -0.5, 1.5), rf(r) if r.random() < 0.3 else u(r, -0.5, 1.5)],
                   "CalcH": [0]}
    return c


def g_edges():
    r = random.Random(5053)
    for ctrl in (None, 0, 0x4000):
        for a in (0, 1):
            for b in (0, 1):
                for count in (0, 1):
                    for kind in ("flag", "slot", "nullslot"):
                        yield g_case(r, ctrl, a, b, count, kind)
    for ctrl in (None, 0x4000):
        yield g_case(r, ctrl, 1, 0, 0, "none")


def g_random(r):
    return g_case(r)


def d_case(r, n=None, inflags=None, f40=None, quiet=None):
    c = {"op": "d"}
    n = r.choice([0, 1, 2, 3, 4]) if n is None else n
    c["inflags"] = r.choice([0, 0, 0x100, 0x1FF, 0x200]) if inflags is None else inflags
    snap = r.random() < 0.5
    q = (lambda lo, hi: fb(round(r.uniform(lo, hi) * 4) / 4)) if snap else (lambda lo, hi: fb(r.uniform(lo, hi)))
    c["f68"] = q(0, 2)
    c["f6c"] = q(5, 40)
    c["fc4"] = q(0, 3)
    flag0 = r.random() < 0.6 if f40 is None else f40
    c["braces"] = [(r.randrange(1, 1000), r.getrandbits(32), (0x40 if (i == 0 and flag0) else r.choice([0, 0x40, 0xC0])) | r.choice([0, 0x100]))
                   for i in range(n)]
    c["h"] = q(0, 45)
    c["quiet"] = r.choice([0, 0, 1]) if quiet is None else quiet
    c["initout"] = rf(r)
    c["initlevel"] = r.getrandbits(32)
    heights, cur = [], r.uniform(0, 6)
    for _ in range(n + 3):
        heights.append(fb(round(cur * 4) / 4 if snap else cur))
        cur += r.uniform(0, 8) if r.random() < 0.8 else 0.0
    c["queues"] = {"CalcH": heights}
    return c


def d_edges():
    r = random.Random(7788)
    for n in range(0, 5):
        for inflags in (0, 0x100):
            for f40 in (False, True):
                for quiet in (0, 1):
                    for _ in range(3):
                        yield d_case(r, n, inflags, f40, quiet)


def d_random(r):
    return d_case(r)


TESTS = [
    FnTest("GetNewBraceLoftInterval", ppc_g, host_g, edge=g_edges, rnd=g_random),
    FnTest("DetermineLinkLevel", ppc_d, host_d, edge=d_edges, rnd=d_random),
]
