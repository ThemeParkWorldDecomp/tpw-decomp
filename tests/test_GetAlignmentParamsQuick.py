"""Unit v5970, the coaster track's Bezier and alignment helpers, against libtpw_unit_GetAlignmentParamsQuick.dylib
(tests/unit_GetAlignmentParamsQuick.cpp), main app ("SimTheme Park"), src/game/rides/CoasterSpline.cpp:
  BezierPosition 0x1004da98, BezierTangent 0x1004dc00, GetAlignmentParamsQuick 0x1004de98, GetBankParams 0x1004e04c,
  GetAlignmentParamsSlow 0x1004e230

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetVectorRotationAroundX/Y/ZAxisFrom*, SetInitialOrientationParams) are fakes on both sides that record their arguments (the
point's bits) and answer from the case's queues.

The world of a case: the sine table globals (mask, scale, table), Bezier control blocks, the orientation block (0x24 bytes: the original
writes a word at +0x1c), the point blocks. What is compared: the written blocks, and the event log.

Fused operations: ROUNDING hooks the unit's functions (fmadds, fctiwz). Not tested: angles that are NaN or huge on the table path (fctiwz
of those differs between the emulator and the hardware).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetAlignmentParamsQuick"
MASK = 0xFFFFFFFF

F_BPOS, F_BTAN, F_QUICK, F_BANK, F_SLOW = 0x1004DA98, 0x1004DC00, 0x1004DE98, 0x1004E04C, 0x1004E230
SIZES = {F_BPOS: 268, F_BTAN: 260, F_QUICK: 356, F_BANK: 372, F_SLOW: 128}
OWN = set(SIZES)

ROT_X, ROT_Y, ROT_Z = 0x100C1040, 0x100C10B8, 0x100C112C
SET_INIT = 0x1005092C

ADDR = dict(tpg=0x103B74C8)
HOST_LAYOUT = [4, 12, 16, 0x1C, 4, 0xC, 0x40, 0x58]
ORIENT_WORDS = 9


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


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_rot(name):
    def stub(e):
        L = e.cl
        pt = struct.unpack(">3I", e.read(e.r(3), 12))
        L.rec(name, tuple(canon_f(b) for b in pt))
        e.set_f1(bitsf(L.ret(name)))
    return stub


STUBS.internal(ROT_X, make_rot("RotX"))
STUBS.internal(ROT_Y, make_rot("RotY"))
STUBS.internal(ROT_Z, make_rot("RotZ"))


def stub_set_init(e):
    L = e.cl
    L.rec("SetInit", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))


STUBS.internal(SET_INIT, stub_set_init)


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

    def call(self, fn, args):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gap_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gap_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        getattr(self.lib, "tpw_gap_" + fn)(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gap_"
    lib.tpw_gap_addr.restype, lib.tpw_gap_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gap_alloc.restype, lib.tpw_gap_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gap_layout.argtypes = [C.POINTER(C.c_int64)]
    u, f = C.c_uint64, C.c_float
    lib.tpw_gap_bpos.argtypes = [u, u, f, u]
    lib.tpw_gap_btan.argtypes = [u, u, f, u]
    lib.tpw_gap_quick.argtypes = [f, f, f, f, u, u]
    lib.tpw_gap_bank.argtypes = [u, u, u, f, u]
    lib.tpw_gap_slow.argtypes = [u, u, f, u, C.c_int32, u]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gap_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    tpg = s.g("tpg")
    s.words(tpg + 4, [case["mask"], case["scale"]])
    s.words(tpg + 0xC, case["table"])
    s.blk = {}
    for name, ws in case["blocks"].items():
        a = s.alloc(max(4 * len(ws), 0x24) + 8)
        s.words(a, ws)
        s.blk[name] = (a, len(ws))


def ptr(s, name):
    return s.blk[name][0] if name in s.blk else 0


def execute(s, case):
    op = case["op"]
    a = ptr
    if op == "bpos":
        s.call(F_BPOS, [a(s, "a"), a(s, "b"), T.F(bitsf(case["t"])), a(s, "out")]) if isinstance(s, Ppc) else \
            s.call("bpos", [a(s, "a"), a(s, "b"), bitsf(case["t"]), a(s, "out")])
    elif op == "btan":
        s.call(F_BTAN, [a(s, "a"), a(s, "b"), T.F(bitsf(case["t"])), a(s, "out")]) if isinstance(s, Ppc) else \
            s.call("btan", [a(s, "a"), a(s, "b"), bitsf(case["t"]), a(s, "out")])
    elif op == "quick":
        px, py, pz = case["p"]
        if isinstance(s, Ppc):
            s.call(F_QUICK, [px, py, pz, T.F(bitsf(case["t"])), a(s, "orient"), a(s, "out")])
        else:
            s.call("quick", [bitsf(px), bitsf(py), bitsf(pz), bitsf(case["t"]), a(s, "orient"), a(s, "out")])
    elif op == "bank":
        if isinstance(s, Ppc):
            s.call(F_BANK, [a(s, "pt"), a(s, "a"), a(s, "b"), T.F(bitsf(case["t"])), a(s, "orient")])
        else:
            s.call("bank", [a(s, "pt"), a(s, "a"), a(s, "b"), bitsf(case["t"]), a(s, "orient")])
    else:
        if isinstance(s, Ppc):
            s.call(F_SLOW, [a(s, "params"), a(s, "pt"), T.F(bitsf(case["t"])), a(s, "orient"), case["n"] & MASK, a(s, "out")])
        else:
            s.call("slow", [a(s, "params"), a(s, "pt"), bitsf(case["t"]), a(s, "orient"), case["n"], a(s, "out")])


def observe(s, case):
    res = {"log": s.log()}
    for name, (a, n) in s.blk.items():
        res[name] = [canon_f(w) for w in s.rd_words(a, n)]
    return res


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


# ---------------------------------------------------------------------------------------------------------------- cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rfa(r):
    """An angle: no NaN or infinity (they go through fctiwz)."""
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, fb(math.pi), fb(-math.pi), fb(2 * math.pi), fb(math.pi / 2), 0x3F800000])
    if k < 0.9:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def rt(r):
    return r.choice([0, 0x3F800000, fb(r.random()), fb(r.random()), fb(r.uniform(-1, 2))])


def base(r, op):
    mask = r.choice([0xFFF, 0xFFF, 0x3FF, 0xFF])
    scale = fb(r.choice([4096 / (2 * math.pi), 1024 / (2 * math.pi), r.uniform(1, 900)]))
    table = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(0x1000)]
    if r.random() < 0.3:
        for _ in range(40):
            table[r.randrange(0x1000)] = rf(r)
    return {"op": op, "mask": mask, "scale": scale, "table": table, "blocks": {}, "t": rt(r),
            "queues": {"RotX": [rfa(r)], "RotY": [rfa(r)], "RotZ": [rfa(r)]}}


def ctrl(r):
    return [rf(r) for _ in range(0x58 // 4)]


def pt(r):
    return [rf(r) for _ in range(3)]


def orient(r, flag=None):
    ws = [rf(r) for _ in range(ORIENT_WORDS)]
    ws[0] = r.choice([0, 1, 2, 3, 0x80000001, 0xFFFFFFFE]) if flag is None else flag
    if flag is None:
        ws[1] = rfa(r)
        ws[2] = rfa(r)
    return ws


def bez_case(r, op):
    c = base(r, op)
    c["blocks"] = {"a": ctrl(r), "b": ctrl(r), "out": pt(r)}
    return c


def quick_case(r, flag=None, out=None, op="quick"):
    c = base(r, op)
    c["p"] = pt(r)
    c["blocks"] = {"orient": orient(r, flag)}
    if out is None:
        out = r.choice([None, rfa(r)])
    if out is not None:
        c["blocks"]["out"] = [out]
    return c


def bank_case(r):
    c = base(r, "bank")
    c["blocks"] = {"pt": pt(r), "a": ctrl(r), "b": ctrl(r), "orient": orient(r)}
    return c


def slow_case(r):
    c = quick_case(r, op="slow")
    c["blocks"]["pt"] = c.pop("p")
    c["blocks"]["params"] = [r.getrandbits(32) for _ in range(8)]
    c["n"] = r.choice([0, 1, -1, 5])
    return c


def bez_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for t in (0, 0x3F800000, 0x80000000, 0x3F000000, 0xBF800000, 0x7FC00000):
            c = bez_case(r, op)
            c["t"] = t
            yield c
    return gen


def quick_edges():
    r = random.Random(11)
    P = fb(math.pi)
    for flag in (0, 1, 2, 3):
        for out in (None, 0, P, fb(-1.0), fb(1.0)):
            for rx in (0, P, fb(-1.0), fb(2.0), fb(4.0), 0x7FC00000, 0x80000000):
                c = quick_case(r, flag, out)
                c["queues"]["RotX"] = [rx]
                c["queues"]["RotY"] = [fb(r.uniform(-4, 4))]
                yield c


def bank_edges():
    r = random.Random(12)
    for t in (0, 0x3F800000, 0x3F000000):
        for a4 in (0, 0x80000000, fb(math.pi), fb(-math.pi / 2)):
            c = bank_case(r)
            c["t"] = t
            c["blocks"]["orient"][1] = a4
            c["blocks"]["orient"][2] = a4 ^ 0x80000000
            yield c


def slow_edges():
    r = random.Random(13)
    for flag in (0, 1):
        for out in (None, fb(1.0)):
            yield slow_case_with(r, flag, out)


def slow_case_with(r, flag, out):
    c = slow_case(r)
    c["blocks"]["orient"][0] = flag
    if out is None:
        c["blocks"].pop("out", None)
    else:
        c["blocks"]["out"] = [out]
    return c


TESTS = [
    FnTest("BezierPosition", ppc, host, edge=bez_edges("bpos", 1), rnd=lambda r: bez_case(r, "bpos")),
    FnTest("BezierTangent", ppc, host, edge=bez_edges("btan", 2), rnd=lambda r: bez_case(r, "btan")),
    FnTest("GetAlignmentParamsQuick", ppc, host, edge=quick_edges, rnd=quick_case),
    FnTest("GetBankParams", ppc, host, edge=bank_edges, rnd=bank_case),
    FnTest("GetAlignmentParamsSlow", ppc, host, edge=slow_edges, rnd=slow_case),
]
