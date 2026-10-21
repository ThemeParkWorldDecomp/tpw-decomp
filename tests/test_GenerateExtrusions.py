"""Unit v5557, GenerateExtrusions, against libtpw_unit_GenerateExtrusions.dylib (tests/unit_GenerateExtrusions.cpp), main app
("SimTheme Park"), src/game/rides/CoasterExtrude.cpp:
  GenerateExtrusions 0x1004ff18

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (SetInitialCoasterInterpParams, IsEndOfSampleBuffer, InterpolateCoasterSpline, GetCrossSectionFromParams, fmod, modf) are fakes on
both sides that record their arguments and answer from the case.

The world of a case: the current instance (type, orientation, sampled track with a control list per sample), the type (flags, the table of
transitions, the two section words), the transition records (each a length float), the two clamp globals, and the answer queues (how many
steps, the orientation the spline gives, the cross section each step has). What is compared: the first 60 records of the table, the two
static records, the orientation and the event log. The records and words that hold addresses are compared as record numbers.

One fused operation (fnmsubs) and fctiwz: the function's range runs under HardwareRounding. Not tested: NaN positions (fctiwz of NaN).
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

SHIM_LIBRARY = "unit_GenerateExtrusions"
MASK = 0xFFFFFFFF

FN = 0x1004FF18
SIZES = {FN: 2536}
OWN = set(SIZES)

SIP, IEOS, ICS, GCS = 0x100509BC, 0x1004B050, 0x1004B0B0, 0x1004FD94

ADDR = dict(cur=0x102B68A8, curdata=0x102B68A4, g0=0x1029CE70, g1=0x1029CE74, static=0x102B7650, buf=0x102B7674)

MAC = dict(P=4, itype=4, f78=0x78, samples=0x9C, block=0x3C, isize=0x200, ssize=0x38, spiece=0x2C, cf08=8, cnext=0x18, csize=0x58,
           rf14=0x14, rsize=0x20, esize=0x10, esec=0xC, ttable=0x78, tcount=0x7C, tsize=0x120, secB=0x80, secA=0x84)
HOST = {}          # filled by bind from tpw_ge_layout
NSAMPLES = 24
FIRST = 8          # the sample number of the array's first element is -8
NBUF = 60


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def split64(x):
    b = T.f64_bits(x)
    return (b >> 32) & MASK, b & MASK


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_sip(e):
    L = e.cl
    L.rec("SIP", (L.lab(e.r(3)),))


def stub_ieos(e):
    L = e.cl
    L.rec("IEOS", (L.lab(e.r(3)), canon_f(fb(e.f(1)))))
    e.set_r(3, L.ret("IEOS") & MASK)


def stub_ics(e):
    L = e.cl
    L.rec("ICS", (L.lab(e.r(3)), canon_f(fb(e.f(1))), L.lab(e.r(6))))
    o = e.r(6)
    a, b, c = L.ret("ICS_a") & MASK, L.ret("ICS_b") & MASK, L.ret("ICS_c") & MASK
    e.write(o + 4, struct.pack(">III", a, b, c))
    p = e.r(5)
    e.write(p, bytes(12))


def stub_gcs(e):
    L = e.cl
    L.rec("GCS", (L.lab(e.r(3)), canon_f(fb(e.f(1))), L.lab(e.r(5)), canon_f(fb(e.f(2)))))
    e.set_r(3, L.recs[L.ret("GCS") & 7] & MASK)


def stub_fmod(e):
    x, y = e.f(1), e.f(2)
    e.cl.rec("fmod", split64(x) + split64(y))
    e.set_f1(math.fmod(x, y))


def stub_modf(e):
    x = e.f(1)
    e.cl.rec("modf", split64(x))
    frac, whole = math.modf(x)
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)


STUBS.internal(SIP, stub_sip)
STUBS.internal(IEOS, stub_ieos)
STUBS.internal(ICS, stub_ics)
STUBS.internal(GCS, stub_gcs)
STUBS.imp("fmod", stub_fmod)
STUBS.imp("modf", stub_modf)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def norm(self, v):
        return ("r", self.recs.index(v)) if v in self.recs else ("raw", v)


class Ppc(Side):
    fmt = ">I"
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.inst = e.alloc(self.O["isize"])
        self.type = e.alloc(self.O["tsize"])
        self.L = e.cl = CL.PpcLog({self.inst: 1, self.type: 2, self.inst + 0x78: 3, self.inst + 0x3C: 4}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def word(self, a):
        return a

    def set_recs(self, recs):
        self.L.recs = recs

    def log(self):
        return self.L.log

    def call(self):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = HOST
        self.inst = self.alloc(HOST["isize"])
        self.type = self.alloc(HOST["tsize"])
        self.base = lib.tpw_ge_base()      # reserved by the first allocation
        CL.host_begin(lib, {1: self.inst, 2: self.type, 3: self.inst + HOST["f78"], 4: self.inst + HOST["block"]}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ge_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ge_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def word(self, a):
        """A record address as the type table's word keeps it: the arena offset."""
        return a - self.base if a else 0

    def set_recs(self, recs):
        self.write(self.g("recs"), struct.pack("<8Q", *recs))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_ge_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ge_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ge_base.restype, lib.tpw_ge_base.argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    v = list(out)[:n]
    names = ["itype", "f78", "samples", "block", "isize", "ssize", "spiece", "cf08", "cnext", "csize", "rf14", "rsize", "esize", "esec",
             "ttable", "tcount", "tsize", "secB", "secA"]
    assert len(v) == len(names), v
    HOST.update(zip(names, v))
    HOST["P"] = 8
    assert HOST["csize"] == 0x58 and HOST["esize"] == 24 and HOST["ttable"] == 0x78 and HOST["tcount"] == 0x80, v
    assert HOST["secB"] == 0x88 and HOST["secA"] == 0x90, v
    assert HOST["isize"] >= HOST["samples"] + 8 and HOST["tsize"] >= 0x98, v
    HOST["tsize"] = max(HOST["tsize"], 0x120)
    lib.bound = True


def build(s, case):
    O = s.O
    recs = [0]
    for ln in case["lens"]:
        a = s.alloc(O["rsize"])
        s.words(a + O["rf14"], [ln])
        recs.append(a)
    recs += [0] * (8 - len(recs))
    s.recs = recs
    s.set_recs(recs)
    arr = s.alloc(NSAMPLES * O["ssize"])
    for j, nodes in enumerate(case["samples"]):
        addrs = [s.alloc(O["csize"]) for _ in nodes]
        for k, (f08, w0) in enumerate(nodes):
            s.words(addrs[k], [w0])
            s.words(addrs[k] + O["cf08"], [f08])
            if k + 1 < len(nodes):
                s.ptr(addrs[k] + O["cnext"], addrs[k + 1])
        s.ptr(arr + j * O["ssize"] + O["spiece"], addrs[0])
    inst, typ = s.inst, s.type
    s.ptr(inst + O["itype"], typ)
    s.ptr(inst + O["samples"], arr + FIRST * O["ssize"])
    s.words(inst + O["f78"], case["orient"])
    s.words(typ + 0x74, [case["flags"]])
    entries = case["entries"]
    table = s.alloc(0x2C * len(entries))
    for k, (w20, fr, to) in enumerate(entries):
        s.words(table + 0x2C * k + 0x20, [s.word(recs[w20]), s.word(recs[fr]), s.word(recs[to])])
    s.ptr(typ + O["ttable"], table)
    s.words(typ + O["tcount"], [len(entries)])
    s.ptr(typ + O["secB"], recs[case["secB"]])
    s.ptr(typ + O["secA"], recs[case["secA"]])
    s.ptr(s.g("cur"), inst)
    s.ptr(s.g("curdata"), inst + O["block"])
    s.words(s.g("g0"), [case["g0"]])
    s.words(s.g("g1"), [case["g1"]])
    s.write(s.g("static"), bytes(2 * O["esize"]))
    s.write(s.g("buf"), bytes(NBUF * O["esize"]))


def observe(s):
    O = s.O
    E, sec = O["esize"], O["esec"]

    def rec(a):
        return [canon_f(x) for x in s.rd_words(a, 3)] + [s.norm(s.rd_ptr(a + sec))]

    lg = s.log()
    return {
        "log": lg,
        "buf": [rec(s.g("buf") + k * E) for k in range(NBUF)],
        "static": [rec(s.g("static") + k * E) for k in range(2)],
        "orient": [canon_f(x) for x in s.rd_words(s.inst + O["f78"], 7)],
    }


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

LENS = [0.5, 1.0, 2.0, -0.5, -1.0, -0.25, 0.0, 0.75, -2.0, 1.5, -1.5, 0.25]


def base(r, n=None, flags=None, lens=None, clamps=None):
    if n is None:
        n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14])
    if lens is None:
        lens = [fb(r.choice(LENS)) if r.random() < 0.9 else fb(r.uniform(-3, 3)) for _ in range(4)]
    else:
        lens = [fb(x) for x in lens]
    if flags is None:
        flags = r.choice([1, 1, 1, 1, 0, 3, 2, 0x11])
    a0 = r.randrange(0, 5)
    seq = []
    cur = a0 if r.random() < 0.7 else r.randrange(0, 5)
    for _ in range(max(n, 1)):
        if r.random() > 0.55:
            cur = r.randrange(0, 5)
        seq.append(cur)
    entries = [(a0, r.randrange(0, 5), r.randrange(0, 5))]
    extra = []
    prev = a0
    for v in seq:
        if v != prev and r.random() < 0.8:
            extra.append((0 if r.random() < 0.08 else r.randrange(1, 5), prev, v))
        prev = v
    for _ in range(r.randrange(0, 3)):
        extra.append((r.randrange(0, 5), r.randrange(0, 5), r.randrange(0, 5)))
    if r.random() < 0.3:
        r.shuffle(extra)
    entries += extra[:12]
    if clamps is None:
        clamps = (fb(0.0), fb(1.0)) if r.random() < 0.75 else (fb(r.uniform(-0.5, 1.0)), fb(r.uniform(0.0, 1.5)))
    samples = []
    for j in range(NSAMPLES):
        i = j - FIRST
        f = i + r.uniform(-0.2, 0.3)
        nodes = []
        for _ in range(r.randrange(1, 4)):
            nodes.append((fb(f), r.choice([i & MASK, r.randrange(0, 1000), r.randrange(0, 2 ** 32)])))
            f += r.choice([0.0, r.uniform(0, 0.7), r.uniform(0, 0.7), r.uniform(-0.1, 0.0)])
        samples.append(nodes)
    c_vals = [fb(r.uniform(-10, 10)) if r.random() < 0.8 else fb(r.uniform(-1e4, 1e4)) for _ in range(n + 2)]
    return {
        "n": n,
        "queues": {
            "IEOS": [0] * n + [1],
            "GCS": seq,
            "ICS_a": [fb(r.uniform(-4, 4)) for _ in range(n + 2)],
            "ICS_b": [fb(r.uniform(-4, 4)) for _ in range(n + 2)],
            "ICS_c": c_vals,
        },
        "lens": lens, "flags": flags, "entries": entries, "secA": r.randrange(0, 5), "secB": r.randrange(0, 5),
        "g0": clamps[0], "g1": clamps[1], "orient": [fb(r.uniform(-5, 5)) for _ in range(7)], "samples": samples,
    }


def edges():
    r = random.Random(5557)
    yield base(r, n=0)
    yield base(r, n=1)
    yield base(r, n=8, flags=0)
    for lens in ([1.0, 2.0, 0.5, 1.5], [-1.0, -0.5, -0.25, -2.0], [0.0, 0.0, 0.0, 0.0], [0.5, -0.5, 1.0, -1.0], [-0.5, 0.5, -1.0, 1.0]):
        for _ in range(4):
            yield base(r, n=r.choice([6, 10, 14]), flags=1, lens=lens)
    yield base(r, n=12, flags=1, clamps=(fb(0.25), fb(0.75)))
    yield base(r, n=12, flags=1, clamps=(fb(0.9), fb(0.1)))
    for _ in range(20):
        yield base(r, n=14, flags=1)


def rnd(r):
    return base(r)


TESTS = [
    FnTest("GenerateExtrusions", ppc, host, edge=edges, rnd=rnd),
]
