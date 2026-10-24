"""Unit v5992, SetupBraceBezierCtrlParams and GenerateTrack, against libtpw_unit_SetupBraceBezierCtrlParams.dylib
(tests/unit_SetupBraceBezierCtrlParams.cpp), main app ("SimTheme Park"), src/game/rides/CoasterTrack.cpp:
  SetupBraceBezierCtrlParams 0x10050a3c, GenerateTrack 0x10050fcc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The structures of
the Mac (word pointers) and of the host (8 byte pointers, other offsets) are written through a table of field names, the Mac's offsets here
and the host's from the library (tpw_sbbc_layout). The callees outside the unit (DistanceBetweenPoints, RecalcBraceTangents,
GetActualBraceParams, LinearSampleTrack, CalculateTrackGForcesSpeedAndExcitement, RecalcTrackBounds, DoCollisionChecking,
DestroyDynamicMeshSetInstance, GenerateExtrusions, GenerateTrackSubdivisions, ExtrudeTrackSection, GetNewTrackMeshSet) are fakes on both
sides that record their arguments (the distances' points as bits, the objects as labels); the distances and the subdivision counts come
from the case's queues. GenerateTrack runs SetupBraceBezierCtrlParams as the original on both sides.

The world of a case: an instance (type with a texture set, brace chain with splines of controls, mesh set chain, the junk the call
overwrites) and the globals of the call. What is compared: the call log, every control's number, neighbours and weight, the instance's
words, the cache, the texture flag store and the extrusion globals. No fused operations. Not tested: a NULL spline (read at once).

With the port fixes on (unit coaster64), the translation does not store a control's previous control (the Mac's word 0x14): the host
control's mBrace covers that word and keeps the control's brace, and Link14 works the previous control out from it (the port fixes in
SetupBraceBezierCtrlParams and Link14, src/game/rides/CoasterTrack.cpp). The host world then gives every control its brace in mBrace (as
SetupBraceInfo does), and the previous control read back is the one Link14 answers from the host's words after the call: the control
before it in its array, or the last of the same array of its brace's mNext (none without one), once mBrace still holds the brace.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetupBraceBezierCtrlParams"
MASK = 0xFFFFFFFF

FN_SETUP, FN_GEN = 0x10050A3C, 0x10050FCC
SIZES = {FN_SETUP: 880, FN_GEN: 760}
OWN = set(SIZES)

CALLEES = dict(DIST=0x100C3198, TANG=0x1003D080, ACTUAL=0x1003D4CC, LINEAR=0x10051770, CALCG=0x1004BA48, BOUNDS=0x10047818,
               COLL=0x10048F64, DESTROY=0x10063264, EXTRS=0x1004FF18, SUBDIV=0x1004E8C0, EXTRUDE=0x1004F628, MESHSET=0x1004EA30)

ADDR = dict(gCoasterCurrent=0x102B68A8, gCoasterCurrentData=0x102B68A4, gStoreB=0x102B74B4, gCache=0x102B74BC, gTimes=0x102B74C0,
            gParamsTable=0x102B7670, gParamsBuf=0x102B7674, gKind=0x102C1674, gWinding=0x102C1678, gTOff=0x102C167C)
PTR_GLOBALS = ("gCoasterCurrent", "gCoasterCurrentData", "gStoreB", "gCache", "gParamsTable")

# (name, the Mac's offset or size); the order is tpw_sbbc_layout's
NAMES = [("I_FLAGS", 0), ("I_TYPE", 4), ("I_F38", 0x38), ("I_B3C", 0x3C), ("I_LAST", 0x54), ("I_F70", 0x70), ("I_F74", 0x74),
         ("I_F78", 0x78), ("I_F7C", 0x7C), ("I_F80", 0x80), ("I_F84", 0x84), ("I_F88", 0x88), ("I_F8C", 0x8C), ("I_F90", 0x90),
         ("I_F94", 0x94), ("I_SIZE", 0x200),
         ("B_FLAGS", 8), ("B_X4C", 0x4C), ("B_PREV", 0x64), ("B_NEXT", 0x68), ("B_F74", 0x74), ("B_F90", 0x90), ("B_DATA", 0x100),
         ("B_SIZE", 0x110),
         ("D_FLAGS", 4), ("D_SIZE", 0x40),
         ("S_COUNT", 0), ("S_A", 4), ("S_B", 8), ("S_SIZE", 16),
         ("C_F00", 0), ("C_F08", 8), ("C_F0C", 0xC), ("C_PREV", 0x14), ("C_NEXT", 0x18), ("C_F40", 0x40), ("C_SIZE", 0x58),
         ("T_TEX", 0x1C), ("T_SIZE", 0x400),
         ("X_COUNT", 4), ("X_ENTRIES", 0x10), ("X_SIZE", 0x40),
         ("E_FLAGS", 0), ("E_SIZE", 0x10),
         ("M_F38", 0x38), ("M_SIZE", 0x100)]
MAC = dict(NAMES)
IFLOATS = ["I_F70", "I_F74", "I_F78", "I_F7C", "I_F80", "I_F84", "I_F88", "I_F8C", "I_F90"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def pt_bits(e, a):
    return tuple(canon_f(w) for w in struct.unpack(">3I", e.read(a, 12)))


def stub_dist(e):
    L = e.cl
    L.rec("Dist", pt_bits(e, e.r(3)) + pt_bits(e, e.r(4)))
    e.set_f1(bitsf(L.ret("Dist")))


def plain(name, nargs):
    def stub(e):
        L = e.cl
        L.rec(name, tuple(L.lab(e.r(3 + i)) for i in range(nargs)))
    return stub


def stub_destroy(e):
    L = e.cl
    L.rec("Destroy", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))


def stub_subdiv(e):
    L = e.cl
    L.rec("Subdiv", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), canon_f(fb(e.f(1)))))
    e.set_r(3, L.ret("Subdiv") & MASK)


def stub_extrude(e):
    L = e.cl
    L.rec("Extrude", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), L.lab(e.r(6)), s32(e.r(7))))


def stub_meshset(e):
    e.cl.rec("GetNewTrackMeshSet", (s32(e.r(3)),))


STUBS.internal(CALLEES["DIST"], stub_dist)
STUBS.internal(CALLEES["TANG"], plain("RecalcBraceTangents", 0))
STUBS.internal(CALLEES["ACTUAL"], plain("GetActualBraceParams", 0))
STUBS.internal(CALLEES["LINEAR"], plain("LinearSampleTrack", 1))
STUBS.internal(CALLEES["CALCG"], plain("CalcG", 1))
STUBS.internal(CALLEES["BOUNDS"], plain("RecalcTrackBounds", 1))
STUBS.internal(CALLEES["COLL"], plain("DoCollisionChecking", 1))
STUBS.internal(CALLEES["DESTROY"], stub_destroy)
STUBS.internal(CALLEES["EXTRS"], plain("GenerateExtrusions", 0))
STUBS.internal(CALLEES["SUBDIV"], stub_subdiv)
STUBS.internal(CALLEES["EXTRUDE"], stub_extrude)
STUBS.internal(CALLEES["MESHSET"], stub_meshset)


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

    def wh(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.W = plan(self, case)
        fixed = labels(self, self.W)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def o(self, name):
        return MAC[name]

    def port_fix(self):
        return False

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

    def call(self, op, inst):
        T.call_ppc(self.e, FN_SETUP if op == "setup" else FN_GEN, [inst & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.W = plan(self, case)
        fixed = labels(self, self.W)
        CL.host_begin(lib, {lab: addr for addr, lab in fixed.items()}, case["queues"])

    def done(self):
        pass

    def o(self, name):
        return self.lib.offs[name]

    def port_fix(self):
        return self.lib.port_fix

    def g(self, name):
        return self.lib.tpw_sbbc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sbbc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, inst):
        (self.lib.tpw_sbbc_setup if op == "setup" else self.lib.tpw_sbbc_gen)(inst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sbbc_addr.restype, lib.tpw_sbbc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sbbc_alloc.restype, lib.tpw_sbbc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sbbc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sbbc_setup.restype, lib.tpw_sbbc_setup.argtypes = None, [C.c_uint64]
    lib.tpw_sbbc_gen.restype, lib.tpw_sbbc_gen.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_sbbc_layout(out)
    assert n == len(NAMES), n
    lib.offs = {name: out[i] for i, (name, _) in enumerate(NAMES)}
    lib.tpw_sbbc_portfix.restype, lib.tpw_sbbc_portfix.argtypes = C.c_int32, []
    lib.port_fix = lib.tpw_sbbc_portfix() == 1
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def plan(s, case):
    """Allocates every object of the case (addresses only; build fills them)."""
    o = s.o
    W = {"inst": s.alloc(o("I_SIZE")), "type": s.alloc(o("T_SIZE")), "tex": s.alloc(o("X_SIZE")),
         "entries": s.alloc(max(1, len(case["tex"])) * o("E_SIZE")), "store": s.alloc(0x100), "dummy": s.alloc(16)}
    W["sets"] = [s.alloc(o("M_SIZE")) for _ in range(case["nsets"])]
    W["braces"] = []
    for b in case["braces"]:
        n = max(1, b["count"])
        W["braces"].append({"addr": s.alloc(o("B_SIZE")), "data": s.alloc(o("D_SIZE")), "f74": s.alloc(8), "spl": s.alloc(o("S_SIZE")),
                            "A": s.alloc(n * o("C_SIZE")), "B": s.alloc(n * o("C_SIZE"))})
    return W


def labels(s, W):
    d = {W["inst"]: 1, W["type"]: 3, W["dummy"]: 4, W["store"]: 5, s.g("gParamsBuf"): 80, s.g("gTimes"): 81}
    for i, b in enumerate(W["braces"]):
        d[b["addr"]] = 10 + i
    for i, a in enumerate(W["sets"]):
        d[a] = 40 + i
    return d


def lab(s, p):
    if p == 0:
        return 0
    return labels(s, s.W).get(p, -1)


def build(s, case):
    o, W = s.o, s.W
    inst = W["inst"]
    csz = o("C_SIZE")
    s.words(inst + o("I_FLAGS"), [case["iflags"]])
    s.wp(inst + o("I_TYPE"), W["type"])
    s.wp(inst + o("I_F38"), W["sets"][0] if W["sets"] else 0)
    s.wp(inst + o("I_LAST"), W["braces"][0]["addr"] if W["braces"] else 0)
    for name, junk in zip(IFLOATS + ["I_F94"], case["junk"]):
        s.words(inst + o(name), [junk])
    for i, a in enumerate(W["sets"]):
        s.wp(a + o("M_F38"), W["sets"][i + 1] if i + 1 < len(W["sets"]) else 0)
    s.wp(W["type"] + o("T_TEX"), W["tex"])
    s.wh(W["tex"] + o("X_COUNT"), len(case["tex"]))
    s.wp(W["tex"] + o("X_ENTRIES"), W["entries"])
    for i, fl in enumerate(case["tex"]):
        s.words(W["entries"] + i * o("E_SIZE") + o("E_FLAGS"), [fl])
    for i in range(8):
        s.words(W["store"] + i * 0x1C, [0x55555555])
    nb = len(W["braces"])
    for i, (b, br) in enumerate(zip(case["braces"], W["braces"])):
        a = br["addr"]
        s.words(a + o("B_FLAGS"), [b["flags"]])
        s.wp(a + o("B_PREV"), W["braces"][i + 1]["addr"] if i + 1 < nb else 0)
        s.wp(a + o("B_NEXT"), W["braces"][i - 1]["addr"] if i > 0 else 0)
        s.wp(a + o("B_F74"), br["f74"])
        s.words(br["f74"], [b["f74"]])
        s.wp(a + o("B_F90"), br["spl"])
        s.wp(a + o("B_DATA"), br["data"])
        s.words(br["data"] + o("D_FLAGS"), [b["dflags"]])
        s.words(a + o("B_X4C"), b["pt"])
        s.words(br["spl"] + o("S_COUNT"), [b["count"]])
        s.wp(br["spl"] + o("S_A"), br["A"])
        s.wp(br["spl"] + o("S_B"), br["B"])
        for key in ("A", "B"):
            for k, c in enumerate(b[key]):
                ca = br[key] + k * csz
                s.words(ca + o("C_F00"), [0x01234567])
                s.words(ca + o("C_F08"), [c["f08"]])
                s.words(ca + o("C_F0C"), [c["junk"]])
                s.words(ca + o("C_F40"), c["pt"])
                # With the port fixes the host control's mBrace (the Mac's words 0x10 and 0x14) holds its brace (the module's docstring).
                s.wp(ca + o("C_PREV"), a if s.port_fix() else 0x7777)
                s.wp(ca + o("C_NEXT"), 0x7777)
    if case["op"] == "gen":
        s.wp(s.g("gCoasterCurrentData"), inst + o("I_B3C"))
        s.wp(s.g("gCoasterCurrent"), 0x9999)
        s.wp(s.g("gStoreB"), W["store"])
        s.wp(s.g("gCache"), {"same": W["type"], "other": W["dummy"], "null": 0}[case["cache"]])
        s.wp(s.g("gParamsTable"), 0x8888)
        s.words(s.g("gKind"), [7])
        s.words(s.g("gWinding"), [9])
        s.words(s.g("gTOff"), [case["toff"]])


def norm_ctl(s, W, p, csz):
    if p == 0:
        return 0
    k = 0
    for br in W["braces"]:
        for key in ("A", "B"):
            k += 1
            if br[key] <= p < br[key] + max(1, len(W["braces"])) * 0 + 64 * csz:
                pass
    # exact search by the arrays' own sizes
    k = 0
    for br, n in zip(W["braces"], W["counts"]):
        for key in ("A", "B"):
            k += 1
            if br[key] <= p < br[key] + max(1, n) * csz:
                return k * 1000 + (p - br[key]) // csz + (500 if (p - br[key]) % csz else 0)
    return -1


def link14(s, W, br, ca, csz):
    """The previous control Link14 answers for the host control at ca of the brace br (port fixes on; the module's docstring)."""
    o = s.o
    if s.rp(ca + o("C_PREV")) != br["addr"]:
        return "mBrace %#x" % s.rp(ca + o("C_PREV"))
    spl = s.rp(br["addr"] + o("B_F90"))
    n = s.rd_word(spl + o("S_COUNT"))
    a = s.rp(spl + o("S_A"))
    base = a if a <= ca < a + n * csz else s.rp(spl + o("S_B"))
    index = (ca - base) // csz
    if index > 0:
        return norm_ctl(s, W, base + (index - 1) * csz, csz)
    other = s.rp(br["addr"] + o("B_NEXT"))
    if other == 0:
        return 0
    ospl = s.rp(other + o("B_F90"))
    obase = s.rp(ospl + (o("S_A") if base == a else o("S_B")))
    return norm_ctl(s, W, obase + (s.rd_word(ospl + o("S_COUNT")) - 1) * csz, csz)


def observe(s, case):
    o, W = s.o, s.W
    csz = o("C_SIZE")
    W["counts"] = [b["count"] for b in case["braces"]]
    res = {"log": s.log()}
    ctl = []
    for br, b in zip(W["braces"], case["braces"]):
        for key in ("A", "B"):
            for k in range(b["count"]):
                ca = br[key] + k * csz
                prev = link14(s, W, br, ca, csz) if s.port_fix() else norm_ctl(s, W, s.rp(ca + o("C_PREV")), csz)
                ctl.append([s.rd_word(ca + o("C_F00")), prev,
                            norm_ctl(s, W, s.rp(ca + o("C_NEXT")), csz), canon_f(s.rd_word(ca + o("C_F0C"))),
                            canon_f(s.rd_word(ca + o("C_F08"))), [canon_f(w) for w in s.rd_words(ca + o("C_F40"), 3)]])
    res["ctl"] = ctl
    inst = W["inst"]
    res["inst"] = [s.rd_word(inst + o("I_FLAGS")), lab(s, s.rp(inst + o("I_F38")))] + \
                  [s.rd_word(inst + o(n)) for n in IFLOATS + ["I_F94"]]
    if case["op"] == "gen":
        res["g"] = {n: lab(s, s.rp(s.g(n))) for n in PTR_GLOBALS}
        res["gw"] = [s.rd_word(s.g("gKind")), s.rd_word(s.g("gWinding")), canon_f(s.rd_word(s.g("gTOff")))]
        res["store"] = [s.rd_word(W["store"] + i * 0x1C) for i in range(8)]
    return res


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], s.W["inst"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rdist(r):
    k = r.random()
    if k < 0.12:
        return r.choice([0, 0x80000000, 0x7F800000, 0x7FC00000, 0x3F800000, 0xBF800000])
    if k < 0.9:
        return fb(r.uniform(0.001, 20))
    return fb(r.uniform(-5, 5))


def rpt(r):
    return [fb(r.uniform(-300, 300)) if r.random() < 0.9 else r.choice([0, 0x7FC00000, 0x7F800000]) for _ in range(3)]


def rctl(r):
    return {"f08": fb(r.uniform(-100, 100)), "junk": r.getrandbits(32), "pt": rpt(r)}


def rbrace(r, count=None, flags=None, dflags=None, f74=None):
    n = r.choice([0, 1, 1, 2, 2, 3, 4, 5]) if count is None else count
    return {"count": n, "flags": r.choice([0, 0, 0x2000, 0x10000, 0x20000, 0x30000, 0x200000, 0x32000]) if flags is None else flags,
            "dflags": r.choice([0, 0, 1, 2, 3]) if dflags is None else dflags,
            "f74": r.choice([0, 1, 2, 3, n, n + 1, n - 1 if n else 0, r.randrange(8)]) & MASK if f74 is None else f74,
            "pt": rpt(r), "A": [rctl(r) for _ in range(max(1, n))], "B": [rctl(r) for _ in range(max(1, n))]}


def base(r, op, nb=None, **kw):
    nb = r.choice([0, 1, 2, 3, 4]) if nb is None else nb
    return {"op": op, "iflags": r.choice([0, 0x4000, 0x4001, r.getrandbits(32)]), "nsets": r.choice([0, 1, 2, 3]),
            "tex": [r.choice([0, 4, 1, 5, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(r.choice([0, 1, 2, 3, 6]))],
            "cache": r.choice(["same", "other", "null"]), "toff": r.choice([0, fb(1.5), r.getrandbits(32) & 0x3FFFFFFF]),
            "junk": [r.getrandbits(32) for _ in range(10)],
            "braces": [rbrace(r, **kw) for _ in range(nb)],
            "queues": {"Dist": [rdist(r) for _ in range(80)], "Subdiv": [r.choice([0, 1, 2, 5, -1, r.randrange(100)]) for _ in range(20)]}}


def setup_edges():
    r = random.Random(5001)
    yield base(r, "setup", 0)
    for cnt in (0, 1, 2, 3, 4):
        for f74 in (0, 1, 2, 3, 5):
            for nb in (1, 2, 3):
                c = base(r, "setup", nb, count=cnt, f74=f74, dflags=0, flags=0)
                yield c
    for dfl in (0, 1, 2, 3):
        for fl in (0, 0x2000):
            yield base(r, "setup", 3, count=3, f74=2, dflags=dfl, flags=fl)
    c = base(r, "setup", 3, count=4, f74=2, dflags=0, flags=0)
    c["queues"]["Dist"] = [0, fb(1.0), fb(2.0), fb(0.5), fb(-1.0), fb(3.0), fb(1.0), fb(1.0)] * 10
    yield c


def setup_random(r):
    return base(r, "setup")


def gen_edges():
    r = random.Random(5002)
    for nb in (0, 1, 2, 3):
        for fl in (0, 0x10000, 0x20000, 0x30000):
            for cnt in (0, 1, 2, 3):
                yield base(r, "gen", nb, count=cnt, flags=fl)
    for cache in ("same", "other", "null"):
        c = base(r, "gen", 2, count=2)
        c["cache"] = cache
        yield c
    yield base(r, "gen", 2, count=2, flags=0x10000, f74=1)
    c = base(r, "gen", 4)
    for b, fl in zip(c["braces"], (0x10000, 0, 0x20000, 0)):
        b["flags"] = fl
        b["count"] = 3
        b["A"] = b["A"] + [rctl(r)] * 3
        b["B"] = b["B"] + [rctl(r)] * 3
    yield c


def gen_random(r):
    return base(r, "gen")


TESTS = [
    FnTest("SetupBraceBezierCtrlParams", ppc, host, edge=setup_edges, rnd=setup_random),
    FnTest("GenerateTrack", ppc, host, edge=gen_edges, rnd=gen_random),
]
