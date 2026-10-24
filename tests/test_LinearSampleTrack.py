"""Unit v5664, LinearSampleTrack, against libtpw_unit_LinearSampleTrack.dylib (tests/unit_LinearSampleTrack.cpp), main app
("SimTheme Park"), src/game/rides/CoasterTrack.cpp:
  LinearSampleTrack 0x10051770

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, its own struct offsets and 8-byte pointers) for the translation. The
callees outside the unit (GetTrackPosition, GetWrappedTrackPosition, CalcNumTrackSamples, the position control's Increment and
Decrement, BezierTangent, GetAlignmentParamsQuick, GetBankParams, MReleaseFreeF, MReleaseAllocF, FloatToCell, SetEntranceLinearSamples) are
fakes on both sides that record their arguments and answer from the case's queues; sqrt, fmod, modf and __fpclassifyf are pure fakes.

The world of a case: 2 or 3 braces linked by their previous pointer (the last one is the instance's), each with a spline of 1 to 3
controls and two arrays of 8 controls, the sample buffer and its capacity, the track length (+0xac), gCoasterCurrentData pointing at the
instance's +0x3c block. The fake positions are a random walk with steps of at least 0.2 per axis so the sampling terminates; the fake
Increment says (queue "inc") whether the second brace ends and whether the control moves to the previous brace. What is compared: the
instance's capacity, sample count, widest span and length, whether the buffer moved, every sample written (and the copy after the last),
every control's place, every brace's sample, and the event log.

Fused operations (fmadd, fmadds) and fctiwz: HardwareRounding on the function. Not tested: a first control with no second brace (the
original then runs on registers nothing sets); a negative track length (the sampling may never end); the Decrement branch is reached only
when the t falls below 0, which needs such a length.

With the port fixes on (unit coaster64), the translation keeps no previous control (the Mac's word 0x14 of a control): Link14 works it
out from the control's brace (mBrace) and its place, as SetupBraceBezierCtrlParams links them (the port fixes in
SetupBraceBezierCtrlParams and Link14, src/game/rides/CoasterTrack.cpp). A case's world then gives every control the previous control
SetupBraceBezierCtrlParams links (the one before it in its array, or for the first the last of the same array of the next brace, none for
the last brace) and every brace its next, on both sides (linked); and a control whose previous control the original reads (a sample's t
below 0) has its own brace, found by running the original first (LinearSampleTest). The other controls keep the case's brace, and the
comparison stays exact. The fake Increment walks a brace's controls past its spline's count (the real one moves to the next brace), and
Link14 takes a control past the count in the first array for one of the second: the host world holds a brace's two arrays in one block,
the second first, so that Link14 answers the control before it there too, as the linked case has it.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_LinearSampleTrack"
MASK = 0xFFFFFFFF

FN = 0x10051770
SIZES = {FN: 2472}
OWN = set(SIZES)

GTP, CNTS, FREE, ALLOC, GWTP = 0x100514F0, 0x1005162C, 0x10064C54, 0x10064BD4, 0x1005158C
BT, GAQ, GBP, INC, DEC, SEL, FTC = 0x1004DC00, 0x1004DE98, 0x1004E04C, 0x10051300, 0x100513FC, 0x10049DDC, 0x10237CD8
ADDR = dict(data=0x102B68A4)

# The Mac's offsets, in the order tpw_lst_layout answers the host's.
MAC = dict([
    ("inst.blk", 0x3C), ("inst.last", 0x54), ("inst.cap", 0x98), ("inst.samples", 0x9C), ("inst.a0", 0xA0), ("inst.count", 0xA4),
    ("inst.len", 0xA8), ("inst.fac", 0xAC), ("inst.size", 0x150),
    ("brace.flags", 0x08), ("brace.prev", 0x64), ("brace.f90", 0x90), ("brace.f74", 0x74), ("brace.sample", 0xFC), ("brace.next", 0x68),
    ("brace.size", 0x108),
    ("spl.count", 0), ("spl.A", 4), ("spl.B", 8), ("spl.size", 0xC),
    ("ctl.f08", 0x08), ("ctl.brace", 0x10), ("ctl.p14", 0x14), ("ctl.next", 0x18), ("ctl.size", 0x58), ("ctl.f40", 0x40),
    ("smp.pos", 4), ("smp.yaw", 0x10), ("smp.pitch", 0x14), ("smp.bank", 0x18), ("smp.ca", 0x2C), ("smp.cb", 0x30), ("smp.t", 0x34),
    ("smp.size", 0x38),
])
KEYS = list(MAC)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def state(e, ctl):
    L = e.cl
    return (e.u32(ctl), L.lab(e.u32(ctl + 4)), e.u32(ctl + 8), L.lab(e.u32(ctl + 12)))


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


def put_point(e, a, t, index):
    """The smooth track of the host fake: the direction (3, 1, -2) times the case's scale times (t + index), in single precision."""
    scale = bitsf(e.cl.ret("scale"))
    tt = f32(t + index)
    e.write(a, struct.pack(">3f", f32(tt * f32(scale * 3.0)), f32(tt * f32(scale * 1.0)), f32(tt * f32(scale * -2.0))))


def put_tangent(e, a):
    L = e.cl
    e.write(a, struct.pack(">3I", L.ret("tan") & MASK, L.ret("tan") & MASK, L.ret("tan") & MASK))


def stub_gtp(e):
    t = e.f(1)
    e.cl.rec("GTP", state(e, e.r(3)) + (fb(t),))
    put_point(e, e.r(5), t, e.u32(e.r(3)))


def stub_gwtp(e):
    t = e.f(1)
    e.cl.rec("GWTP", state(e, e.r(3)) + (e.cl.lab(e.r(4)), fb(t)))
    put_point(e, e.r(6), t, e.u32(e.r(3)))


def stub_cnts(e):
    L = e.cl
    L.rec("CNTS", (L.lab(e.r(3)), fb(e.f(1)), e.u32(e.r(5))))
    e.set_r(3, L.ret("cnts") & MASK)


def stub_inc(e):
    L = e.cl
    c = e.r(3)
    L.rec("INC", state(e, c))
    v = L.ret("inc")
    brace = e.u32(c + 4)
    braceB = e.u32(c + 12)
    if v & 1:
        braceB = 0
    if v & 2 and e.u32(brace + 0x64):
        brace = e.u32(brace + 0x64)
    e.write(c, struct.pack(">I", (e.u32(c) + 1) & MASK))
    e.write(c + 4, struct.pack(">I", brace))
    e.write(c + 12, struct.pack(">I", braceB))


def stub_dec(e):
    c = e.r(3)
    e.cl.rec("DEC", state(e, c))
    if e.u32(c) > 0:
        e.write(c, struct.pack(">I", e.u32(c) - 1))


def stub_bt(e):
    L = e.cl
    L.rec("BT", (L.lab(e.r(3)), L.lab(e.r(4)), fb(e.f(1))))
    put_tangent(e, e.r(6))


def stub_gaq(e):
    L = e.cl
    orient = e.r(7)
    L.rec("GAQ", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, fb(e.f(1)), 1 if e.r(8) else 0))
    L.rec("GAQo", struct.unpack(">8I", e.read(orient, 32)))
    e.write(orient + 4, struct.pack(">3I", L.ret("al") & MASK, L.ret("al") & MASK, L.ret("al") & MASK))


def stub_gbp(e):
    L = e.cl
    orient = e.r(7)
    L.rec("GBP", struct.unpack(">3I", e.read(e.r(3), 12)) + (L.lab(e.r(4)), L.lab(e.r(5)), fb(e.f(1))))
    L.rec("GBPo", struct.unpack(">8I", e.read(orient, 32)))
    e.write(orient + 12, struct.pack(">I", L.ret("bank") & MASK))


def stub_free(e):
    e.cl.rec("FREE", (e.cl.lab(e.r(3)),))


def stub_alloc(e):
    size = e.r(3) & MASK
    e.cl.rec("ALLOC", (size // 0x38, e.r(4) & MASK, size % 0x38))
    e.set_r(3, e.alloc(size))


def stub_sel(e):
    e.cl.rec("SEL", (e.cl.lab(e.r(3)),))


def stub_ftc(e):
    bits = struct.unpack(">Q", struct.pack(">d", e.f(1)))[0]
    e.cl.rec("FTC", (bits >> 32, bits & MASK))
    x = e.f(1)
    e.set_r(3, (int(x) if -2147483649.0 < x < 2147483648.0 else -2147483648) & MASK)


def fmod(e):
    x, y = e.f(1), e.f(2)
    if y == 0 or math.isinf(x) or math.isnan(x) or math.isnan(y):
        r = float("nan")
    elif math.isinf(y):
        r = x
    else:
        r = math.fmod(x, y)
    e.set_f1(r)


def modf(e):
    fr, ip = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", ip))
    e.set_f1(fr)


def fpclassifyf(e):
    x = e.f(1)
    e.set_r(3, 1 if math.isnan(x) else 2 if math.isinf(x) else 3 if x == 0 else 4)


for _addr, _fn in ((GTP, stub_gtp), (GWTP, stub_gwtp), (CNTS, stub_cnts), (INC, stub_inc), (DEC, stub_dec), (BT, stub_bt), (GAQ, stub_gaq),
                   (GBP, stub_gbp), (FREE, stub_free), (ALLOC, stub_alloc), (SEL, stub_sel), (FTC, stub_ftc)):
    STUBS.internal(_addr, _fn)
STUBS.imp("fmod", fmod)
STUBS.imp("modf", modf)
STUBS.imp("__fpclassifyf", fpclassifyf)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.pw))[0]


class Ppc(Side):
    fmt, pfmt, pw = ">I", ">I", 4
    O = MAC

    def __init__(self, e, case, watch=None):
        self.e = e
        self.case = case
        self.saved = None
        self.watch = watch

    def start(self, labels):
        self.L = self.e.cl = CL.PpcLog({a: l for l, a in labels.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
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

    def call(self, inst):
        hooks = []
        if self.watch is not None:
            for i, arrays in enumerate(self.ctls):
                for arr, base in enumerate(arrays):
                    end = base + NCTL * 0x58 - 1
                    hooks.append(self.e.uc.hook_add(UC_HOOK_MEM_READ, self.read_link, (i, arr, base), begin=base, end=end))
        try:
            with ROUNDING(self.e):
                T.call_ppc(self.e, FN, [inst & MASK])
        finally:
            for h in hooks:
                self.e.uc.hook_del(h)

    def read_link(self, uc, access, addr, size, value, where):
        """Notes (brace, array, control) when the original reads a control's previous control (its word 0x14)."""
        i, arr, base = where
        j, offset = divmod(addr - base, 0x58)
        if 0x14 <= offset < 0x18:
            self.watch.add((i, arr, j))


class Host(Side):
    fmt, pfmt, pw = "<I", "<Q", 8
    O = None

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        Host.O = lib.O

    def start(self, labels):
        CL.host_begin(self.lib, labels, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lst_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lst_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, inst):
        self.lib.tpw_lst_run(inst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lst_addr.restype, lib.tpw_lst_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lst_alloc.restype, lib.tpw_lst_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lst_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lst_run.restype, lib.tpw_lst_run.argtypes = None, [C.c_uint64]
    lib.tpw_lst_portfix.restype, lib.tpw_lst_portfix.argtypes = C.c_int32, []
    lib.port_fix = lib.tpw_lst_portfix() == 1
    out = (C.c_int64 * 64)()
    n = lib.tpw_lst_layout(out)
    assert n == len(KEYS), n
    lib.O = dict(zip(KEYS, list(out)[:n]))
    assert lib.O["ctl.size"] == 0x58
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

NCTL = 8


def build(s, case):
    """Writes the world; answers {label: address} for every object the log names."""
    O = s.O
    labels = {}
    inst = s.inst = s.alloc(O["inst.size"])
    labels[1] = inst
    smp = s.alloc(max(case["cap0"], 1) * O["smp.size"])
    labels[2] = smp
    s.smp0 = smp
    nb = case["nb"]
    braces, ctls = [], []
    for i in range(nb):
        braces.append(s.alloc(O["brace.size"]))
        labels[10 + i] = braces[i]
        if O["ctl.p14"] < 0:
            # The port fixes: the two arrays in one block, the second first (the module's docstring).
            both = s.alloc(2 * NCTL * 0x58)
            ctls.append([both + NCTL * 0x58, both])
        else:
            ctls.append([s.alloc(NCTL * 0x58), s.alloc(NCTL * 0x58)])
        for arr in range(2):
            for j in range(NCTL):
                labels[100 + i * 20 + arr * 8 + j] = ctls[i][arr] + j * 0x58
    s.braces, s.ctls = braces, ctls
    for i in range(nb):
        b = braces[i]
        sp = s.alloc(O["spl.size"])
        s.w32(sp + O["spl.count"], case["counts"][i])
        s.wptr(sp + O["spl.A"], ctls[i][0])
        s.wptr(sp + O["spl.B"], ctls[i][1])
        s.wptr(b + O["brace.f90"], sp)
        s.wptr(b + O["brace.prev"], braces[i - 1] if i else 0)
        if "links" in case:
            s.wptr(b + O["brace.next"], braces[i + 1] if i + 1 < nb else 0)
        s.w32(b + O["brace.flags"], case["flags"][i])
        s.w32(b + O["brace.sample"], case["sample0"][i])
        if case["f74"][i] is not None:
            w = s.alloc(4)
            s.w32(w, case["f74"][i])
            s.wptr(b + O["brace.f74"], w)
        for arr in range(2):
            for j in range(NCTL):
                c = ctls[i][arr] + j * 0x58
                f08, brn, p14, f40 = case["ctl"][i][arr][j]
                s.w32(c + O["ctl.f08"], f08)
                s.wptr(c + O["ctl.brace"], braces[brn])
                if "links" in case:
                    # The port fixes keep no previous control on the host (ctl.p14 is -1 there; the module's docstring).
                    link = case["links"][i][arr][j]
                    if O["ctl.p14"] >= 0:
                        s.wptr(c + O["ctl.p14"], ctls[link[0]][arr] + link[1] * 0x58 if link else 0)
                else:
                    s.wptr(c + O["ctl.p14"], ctls[i][arr] + p14 * 0x58 if p14 >= 0 else 0)
                s.wptr(c + O["ctl.next"], c + 0x58 if j + 1 < NCTL else 0)
                for k in range(3):
                    s.w32(c + O["ctl.f40"] + 4 * k, f40[k])
    s.w32(inst + O["inst.cap"], case["cap0"])
    s.wptr(inst + O["inst.samples"], smp)
    s.w32(inst + O["inst.a0"], case["a00"])
    s.w32(inst + O["inst.count"], case["count0"])
    s.w32(inst + O["inst.len"], case["len0"])
    s.w32(inst + O["inst.fac"], case["len"])
    s.wptr(inst + O["inst.last"], braces[nb - 1])
    s.wptr(s.g("data"), inst + O["inst.blk"])
    return labels


def observe(s, case):
    O = s.O
    inst = s.inst
    inv = {}
    for i in range(case["nb"]):
        for arr in range(2):
            for j in range(NCTL):
                inv[s.ctls[i][arr] + j * 0x58] = (i, arr, j)
    sp = s.rptr(inst + O["inst.samples"])
    cnt = s.r32(inst + O["inst.count"])
    out = {
        "cap": s.r32(inst + O["inst.cap"]), "moved": sp != s.smp0, "a0": s.r32(inst + O["inst.a0"]), "count": cnt,
        "len": canon_f(s.r32(inst + O["inst.len"])), "log": s.log(),
    }
    samples = []
    for k in range(min(cnt + 1, 400)):
        b = sp + k * O["smp.size"]
        row = [canon_f(s.r32(b + O["smp.pos"] + 4 * i)) for i in range(3)]
        row += [canon_f(s.r32(b + O["smp.yaw"])), canon_f(s.r32(b + O["smp.pitch"])), canon_f(s.r32(b + O["smp.bank"]))]
        row += [inv.get(s.rptr(b + O["smp.ca"]), -1), inv.get(s.rptr(b + O["smp.cb"]), -1), canon_f(s.r32(b + O["smp.t"]))]
        samples.append(row)
    out["samples"] = samples
    out["f08"] = [[[canon_f(s.r32(s.ctls[i][arr] + j * 0x58 + O["ctl.f08"])) for j in range(NCTL)] for arr in range(2)]
                  for i in range(case["nb"])]
    out["bsample"] = [s.r32(b + O["brace.sample"]) for b in s.braces]
    return out


def run(s, case):
    try:
        labels = build(s, case)
        s.start(labels)
        s.call(s.inst)
        return observe(s, case)
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


def make_case(r, **kw):
    nb = r.randrange(2, 4)
    n = r.randrange(150, 260)
    nb = kw.get("nb", nb)
    n = kw.get("n", n)
    c = {"nb": nb, "n": n}
    c["counts"] = [r.choice([1, 1, 2, 3]) for _ in range(nb)]
    c["len"] = fb(r.choice([0.8, 1.0, 2.0, 3.0, 3.0, 4.5, 6.0]) if r.random() < 0.7 else r.uniform(0.8, 6.0))
    c["cap0"] = r.randrange(0, n) if r.random() < 0.5 else r.randrange(n, 300)
    c["a00"] = r.randrange(0, 9)
    c["count0"] = r.randrange(0, 9)
    c["len0"] = rf(r)
    c["flags"] = [r.choice([0, 0x2000, 0x1000, 0x3000]) for _ in range(nb)]
    c["sample0"] = [r.randrange(0, 40) for _ in range(nb)]
    c["f74"] = [None if r.random() < 0.4 else r.randrange(0, 5) for _ in range(nb)]
    ctl = []
    for i in range(nb):
        per = []
        for arr in range(2):
            row = []
            for j in range(NCTL):
                if r.random() < 0.5:
                    f08 = fb(j * 20 + r.randrange(0, 20) + r.choice([0.0, 0.0, 0.3]))
                else:
                    f08 = fb(r.choice([0.0, 0.5, 1.0, r.uniform(0, 100)]))
                row.append((f08, r.randrange(nb), r.choice([-1, -1, r.randrange(NCTL)]),
                            [fb(r.uniform(-5, 5)) for _ in range(3)]))
            per.append(row)
        ctl.append(per)
    c["ctl"] = ctl
    inc = [r.choice([0, 2]) for _ in range(r.randrange(0, 4))] + [r.choice([1, 3])]
    c["queues"] = {
        "scale": [fb(r.uniform(0.3, 1.0))], "tan": [rf(r) for _ in range(3)], "al": [rf(r) for _ in range(12)],
        "bank": [rf(r) for _ in range(5)], "inc": inc, "cnts": [n],
    }
    for k, v in kw.items():
        if k == "inc":
            c["queues"]["inc"] = v
        else:
            c[k] = v
    if "n" in kw:
        c["queues"]["cnts"] = [kw["n"]]
        c["cap0"] = kw.get("cap0", c["cap0"])
    return c


def edges():
    r = random.Random(5664)
    yield make_case(r)
    yield make_case(r, len=fb(0.8))
    yield make_case(r, len=fb(6.0), inc=[1])
    yield make_case(r, inc=[3])
    yield make_case(r, inc=[2, 2, 3])
    yield make_case(r, inc=[0, 0, 0, 1])
    yield make_case(r, counts=[1, 1, 1], nb=3)
    yield make_case(r, counts=[3, 3, 3], nb=3)
    yield make_case(r, counts=[1, 1], nb=2)
    yield make_case(r, n=150, cap0=150)
    yield make_case(r, n=150, cap0=149)
    yield make_case(r, n=200, cap0=0)
    yield make_case(r, n=200, cap0=300)
    yield make_case(r, flags=[0, 0, 0], f74=[None, None, None], nb=3, counts=[2, 2, 2])
    yield make_case(r, flags=[0x2000] * 3, f74=[1, 1, 1], nb=3, counts=[2, 2, 2])
    yield make_case(r, flags=[0] * 3, f74=[0, 1, 2], nb=3, counts=[2, 2, 2], inc=[0, 0, 0, 1])
    yield make_case(r, len0=fb(0.0))
    yield make_case(r, len0=fb(7.0))


def rnd(r):
    return make_case(r)


def linked(case):
    """The case with every control's previous control as SetupBraceBezierCtrlParams links it (brace, index; None for none) and every
    brace's next (port fixes on; the module's docstring)."""
    nb, counts = case["nb"], case["counts"]
    links = [[[(i, j - 1) if j else ((i + 1, counts[i + 1] - 1) if i + 1 < nb else None) for j in range(NCTL)] for arr in range(2)]
             for i in range(nb)]
    return dict(case, links=links)


def read_links(e, case):
    """The controls (brace, array, index) whose previous control the original reads in the case."""
    e.reset_scratch()
    s = Ppc(e, case, set())
    run(s, case)
    return s.watch


class LinearSampleTest(FnTest):
    """FnTest, with the case linked when the unit is built with the port fixes (unit coaster64): the original runs first to find the
    controls whose previous control it reads, and those get their own brace, until no other is read. The comparison stays exact."""

    def run_case(self, runner, case):
        bind(runner.lib)
        if not runner.lib.port_fix:
            return FnTest.run_case(self, runner, case)
        case = linked(case)
        for _ in range(8):
            read = read_links(runner.emu, case)
            foreign = sorted(x for x in read if case["ctl"][x[0]][x[1]][x[2]][1] != x[0])
            if not foreign:
                return FnTest.run_case(self, runner, case)
            ctl = [[list(row) for row in per] for per in case["ctl"]]
            for i, arr, j in foreign:
                f08, _, p14, f40 = ctl[i][arr][j]
                ctl[i][arr][j] = (f08, i, p14, f40)
            case = dict(case, ctl=ctl)
        return ["the controls whose previous control the original reads did not settle"]


TESTS = [
    LinearSampleTest("LinearSampleTrack", ppc, host, edge=edges, rnd=rnd),
]
