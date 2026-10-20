"""Unit v5667, the pylon brace functions, against libtpw_unit_RestorePylonDetails.dylib (tests/unit_RestorePylonDetails.cpp), main app
("SimTheme Park"), src/GetNewBraceLoftInterval_RestorePylonDetails.cpp:
  CalculateBraceHeight 0x1003e6e0, RestorePylonDetails 0x1003ec00, SquishPylonBracesIfRequired 0x1003ee10, InsertBraceIntoPylon 0x1003ef78

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's offsets) for the translation; the same code
reads both back. The callees outside the unit are fakes on both sides that record their arguments and answer from the case's queues:
GetHeightForInterval and GetIntervalForHeight (float answers), SetupBraceAnimFrameData and UpdatePylonStackMeshes (events only).

The world of a case: two controls, two pylons, ten braces, two instances (the argument and the editor's, with their type blocks), the
saved pylon, the five saved braces and their heights, gPylonStackSkip and the editor instance. Objects live in a pool of 0x200-byte blocks
(persistent on the PowerPC side), so a pointer is compared as (object, offset) and the log names objects by their pool number. What is
compared: the answer, the event log, every field the functions touch of both pylons and all ten braces, the out float, gLinkSavedFlag.

No fused operations (plain fadds, fsubs, fcmpo). Not tested: a pylon with a NULL control that holds braces (the original reads the control's
fields), a level past the count, a count of 5 for InsertBraceIntoPylon (the shift writes a sixth brace slot), NaN heights as an argument.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RestorePylonDetails"
MASK = 0xFFFFFFFF

FN_RESTORE, FN_SQUISH, FN_INSERT, FN_CBH = 0x1003EC00, 0x1003EE10, 0x1003EF78, 0x1003E6E0
SIZES = {FN_RESTORE: 448, FN_SQUISH: 256, FN_INSERT: 1304, FN_CBH: 192}
OWN = set(SIZES)

CALL_HEIGHT, CALL_INTERVAL, CALL_SETUP, CALL_UPDATE = 0x10040EB8, 0x10040E10, 0x1003BB44, 0x1003D958

ADDR = dict(editinst=0x102B68B0, skip=0x102B68A0, savedflag=0x102B6EFC, savedpylon=0x102B6EB4, savedbraces=0x102B69A0,
            savedheights=0x102B698C)

# ---- the Mac's offsets (and the host's, read from the library, in the same key order)
KB = ["num", "flags", "prev", "next", "f6c", "f74", "f90", "w94", "w98", "w9c", "wa0", "wa4", "wa8", "wac", "fd4", "fd8", "fdc", "e0", "e4",
      "e8", "ec", "f0", "f4", "f8", "fc", "data", "c78", "c84", "raw", "size"]
OB = [4, 8, 0x64, 0x68, 0x6C, 0x74, 0x90, 0x94, 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xD4, 0xD8, 0xDC, 0xE0, 0xE4, 0xE8, 0xEC, 0xF0, 0xF4,
      0xF8, 0xFC, 0x100, 0x78, 0x84, 0xC, 0x104]
KP = ["num", "flags", "f08", "cx", "cz", "inst", "ctl", "x", "y", "z", "next", "prev", "count", "br", "size"]
OP = [0, 4, 8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x48]
KC = ["flags", "f68", "f6c", "fc4", "f30", "f44"]
OC = [0, 0x68, 0x6C, 0xC4, 0x30, 0x44]
KI = ["type"]
OI = [4]
MAC = dict(B=dict(zip(KB, OB)), P=dict(zip(KP, OP)), C=dict(zip(KC, OC)), I=dict(zip(KI, OI)))

BPTR = ["prev", "next", "f74", "f90", "data"]
BFLOAT = ["f6c", "w94", "fd4", "fd8", "fdc", "e0", "e4", "e8", "ec", "f0", "f4", "f8"]
RAW = ["r%02x" % o for o in range(0x0C, 0x64, 4)]
BWK = ["num", "flags"] + RAW + ["f6c", "w94", "w98", "w9c", "wa0", "wa4", "wa8", "wac", "fd4", "fd8", "fdc", "e0", "e4", "e8", "ec", "f0", "f4",
                                  "f8", "fc"]
PWK = ["num", "flags", "f08", "cx", "cz", "x", "y", "z", "count"]
PPTR = ["inst", "ctl", "next", "prev"]

STRIDE = 0x200
PY0, PY1, CT0, CT1, INST, INST2, TYPE, TYPE2, OUT = 0, 1, 2, 3, 4, 5, 6, 7, 18
BR0 = 8
NOBJ = 19

POOL = [0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_height(e):
    L = e.cl
    L.rec("GetHeightForInterval", (canon_f(fb(e.f(1))), L.lab(e.r(4))))
    e.set_f1(bitsf(L.ret("GetHeightForInterval")))


def stub_interval(e):
    L = e.cl
    L.rec("GetIntervalForHeight", (canon_f(fb(e.f(1))), L.lab(e.r(4))))
    e.set_f1(bitsf(L.ret("GetIntervalForHeight")))


def stub_setup(e):
    L = e.cl
    L.rec("Setup", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_update(e):
    L = e.cl
    L.rec("Update", (L.lab(e.r(3)), L.lab(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))


STUBS.internal(CALL_HEIGHT, stub_height)
STUBS.internal(CALL_INTERVAL, stub_interval)
STUBS.internal(CALL_SETUP, stub_setup)
STUBS.internal(CALL_UPDATE, stub_update)


def setup(runner):
    T.install_std_stubs(runner.emu)
    POOL[0] = T.persist_alloc(runner.emu, NOBJ * STRIDE)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def obj(self, k):
        return self.pool + k * STRIDE

    def wo(self, key):
        """The offset of a brace word."""
        if key in RAW:
            return self.B["raw"] + int(key[1:], 16) - 0x0C
        return self.B[key]

    def refaddr(self, spec):
        if spec is None:
            return 0
        k, off = spec
        return self.obj(k) + self.denorm.get(off, off)

    def ref(self, addr):
        """A pointer as [object label, offset]; the host's offsets of the two sBraceInfo counts are made the Mac's."""
        if addr == 0:
            return [0, 0]
        for k in range(NOBJ):
            base = self.obj(k)
            if base <= addr < base + STRIDE:
                off = addr - base
                return [k + 1, self.norm.get(off, off)]
        return [-1, 0]

    def set_layout(self, lay):
        self.B, self.P, self.C, self.I = lay["B"], lay["P"], lay["C"], lay["I"]
        self.denorm = {0x78: self.B["c78"], 0x84: self.B["c84"]}
        self.norm = {self.B["c78"]: 0x78, self.B["c84"]: 0x84}


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.pool = POOL[0]
        self.set_layout(MAC)
        self.bstride = 0x104
        self.L = e.cl = CL.PpcLog({self.pool + k * STRIDE: k + 1 for k in range(NOBJ)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_cbh(self, brace):
        _, _, f1 = T.call_ppc(self.e, FN_CBH, [brace & MASK])
        return canon_f(fb(f1))

    def call_restore(self, inst, pylon):
        T.call_ppc(self.e, FN_RESTORE, [inst, pylon])
        return 0

    def call_squish(self, inst, control, pylon):
        r3, _, _ = T.call_ppc(self.e, FN_SQUISH, [inst, control, pylon])
        return r3 & MASK

    def call_insert(self, inst, brace, pylon, level, hbits, out, a, b, c, d):
        T.call_ppc(self.e, FN_INSERT, [inst, brace, pylon, level, T.F(bitsf(hbits)), out, a & MASK, b & MASK, c & MASK, d & MASK])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.pool = lib.tpw_rpd_addr(b"pool")
        self.set_layout(lib.lay)
        self.bstride = self.B["size"]
        CL.host_begin(lib, {k + 1: self.pool + k * STRIDE for k in range(NOBJ)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rpd_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_cbh(self, brace):
        return canon_f(self.lib.tpw_rpd_cbh(brace))

    def call_restore(self, inst, pylon):
        self.lib.tpw_rpd_restore(inst, pylon)
        return 0

    def call_squish(self, inst, control, pylon):
        return self.lib.tpw_rpd_squish(inst, control, pylon) & MASK

    def call_insert(self, inst, brace, pylon, level, hbits, out, a, b, c, d):
        self.lib.tpw_rpd_insert(inst, brace, pylon, level, hbits, out, T.s32(a), T.s32(b), T.s32(c), T.s32(d))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rpd_addr.restype, lib.tpw_rpd_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rpd_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rpd_cbh.restype, lib.tpw_rpd_cbh.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rpd_restore.restype, lib.tpw_rpd_restore.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_rpd_squish.restype, lib.tpw_rpd_squish.argtypes = C.c_int32, [C.c_uint64] * 3
    lib.tpw_rpd_insert.restype = None
    lib.tpw_rpd_insert.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_int32, C.c_int32, C.c_int32,
                                   C.c_int32]
    out = (C.c_int64 * 64)()
    n = lib.tpw_rpd_layout(out)
    vals = list(out)[:n]
    assert n == len(KB) + len(KP) + len(KC) + len(KI), n
    lay = {}
    i = 0
    for name, keys in (("B", KB), ("P", KP), ("C", KC), ("I", KI)):
        lay[name] = dict(zip(keys, vals[i:i + len(keys)]))
        i += len(keys)
    assert lay["B"]["raw"] == 0x0C and lay["B"]["size"] <= STRIDE and lay["P"]["size"] <= STRIDE
    assert lay["B"]["num"] == 4 and lay["B"]["flags"] == 8
    assert lay["C"]["f68"] == 0x68 and lay["C"]["f30"] == 0x30 and lay["C"]["f44"] == 0x44 and lay["C"]["fc4"] == 0xC4
    lib.lay = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def put_brace(s, a, b):
    for key, v in b["w"].items():
        s.w(a + s.wo(key), v)
    for key, spec in b["p"].items():
        s.ptr(a + s.B[key], s.refaddr(spec))


def put_pylon(s, a, p):
    for key, v in p["w"].items():
        s.w(a + s.P[key], v)
    for key, spec in p["p"].items():
        s.ptr(a + s.P[key], s.refaddr(spec))
    for i, spec in enumerate(p["br"]):
        s.ptr(a + s.P["br"] + i * s.psz, s.refaddr(spec))


def build(s, case):
    s.write(s.pool, bytes(NOBJ * STRIDE))
    for i, c in enumerate(case["ctl"]):
        a = s.obj(CT0 + i)
        for key in ("flags", "f68", "f6c", "fc4"):
            s.w(a + s.C[key], c[key])
    for i, p in enumerate(case["pyl"]):
        put_pylon(s, s.obj(i), p)
    for i, b in enumerate(case["br"]):
        put_brace(s, s.obj(BR0 + i), b)
    s.ptr(s.obj(INST) + s.I["type"], s.obj(TYPE))
    s.ptr(s.obj(INST2) + s.I["type"], s.obj(TYPE2))
    s.ptr(s.g("editinst"), s.obj(INST2))
    s.w(s.g("skip"), case["skip"])
    sv = case["saved"]
    s.write(s.g("savedpylon"), bytes(s.P["size"] if s.psz == 8 else 0x48))
    put_pylon(s, s.g("savedpylon"), sv["pyl"])
    for i, b in enumerate(sv["br"]):
        a = s.g("savedbraces") + i * s.bstride
        s.write(a, bytes(s.bstride))
        put_brace(s, a, b)
    for i, h in enumerate(sv["h"]):
        s.w(s.g("savedheights") + 4 * i, h)
    s.w(s.g("savedflag"), sv["flag"])


def execute(s, case):
    op = case["op"]
    if op == "cbh":
        return s.call_cbh(0 if case["brace"] is None else s.obj(BR0 + case["brace"]))
    if op == "restore":
        return s.call_restore(s.obj(INST), s.obj(PY0))
    if op == "squish":
        return s.call_squish(s.obj(INST), s.obj(case["ctl_arg"]), s.obj(PY0))
    out = s.obj(OUT) if case["out"] else 0
    return s.call_insert(s.obj(INST), s.obj(BR0 + 9), s.obj(PY0), case["level"], case["height"], out, *case["flags"])


def observe(s, case, answer):
    res = {"answer": answer, "log": s.log(), "out": s.r(s.obj(OUT)), "flag": s.r(s.g("savedflag"))}
    for i in range(2):
        a = s.obj(i)
        res["py%d" % i] = ([s.r(a + s.P[k]) for k in PWK] + [s.ref(s.rptr(a + s.P[k])) for k in PPTR] +
                           [s.ref(s.rptr(a + s.P["br"] + j * s.psz)) for j in range(5)])
    for i in range(10):
        a = s.obj(BR0 + i)
        words = [s.r(a + s.wo(k)) for k in BWK]
        words[2 + len(RAW) + BWK[2 + len(RAW):].index("e8")] = canon_f(words[2 + len(RAW) + BWK[2 + len(RAW):].index("e8")])
        res["br%d" % i] = words + [s.ref(s.rptr(a + s.B[k])) for k in BPTR]
    return res


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


def rff(r):
    k = r.random()
    if k < 0.05:
        return r.choice([0, 0x80000000, fb(1.0), fb(-1.0)])
    if k < 0.8:
        return fb(r.uniform(-3, 6))
    return fb(r.uniform(-300, 300))


def rbref(r):
    return None if r.random() < 0.3 else (BR0 + r.randrange(10), 0)


def cgen(r):
    return {"flags": (r.getrandbits(32) & ~8) | (8 if r.random() < 0.5 else 0),
            "f68": fb(r.choice([0.0, r.uniform(0, 2)])), "f6c": fb(r.choice([r.uniform(0.5, 6), r.uniform(-1, 1)])),
            "fc4": fb(r.choice([0.0, r.uniform(0, 1)]))}


def bgen(r):
    w = {}
    for k in BWK:
        w[k] = rff(r) if k in BFLOAT else r.getrandbits(32)
    w["num"] = r.randrange(0, 6)
    if r.random() < 0.3:
        w["flags"] &= ~0x2000
    if r.random() < 0.3:
        w["flags"] |= 0x2000
    p = {"prev": rbref(r), "next": rbref(r), "f74": None if r.random() < 0.3 else (CT0 + r.randrange(2), r.choice([0x30, 0x44])),
         "f90": None if r.random() < 0.3 else (BR0 + r.randrange(10), r.choice([0x78, 0x84])),
         "data": (r.choice([PY0, PY0, PY0, PY1]), 0)}
    return {"w": w, "p": p}


def pygen(r, n, ctl, brs):
    w = {"num": r.randrange(5), "flags": r.getrandbits(32), "f08": r.getrandbits(32), "cx": r.getrandbits(8), "cz": r.getrandbits(8),
         "x": rff(r), "y": rff(r), "z": rff(r), "count": n}
    p = {"inst": (INST, 0) if r.random() < 0.5 else None, "ctl": None if ctl is None else (CT0 + ctl, 0),
         "next": (r.randrange(2), 0) if r.random() < 0.5 else None, "prev": (r.randrange(2), 0) if r.random() < 0.5 else None}
    br = [(BR0 + i, 0) for i in brs] + [None] * (5 - len(brs))
    return {"w": w, "p": p, "br": br}


def qgen(r):
    def special(r, lo, hi):
        k = r.random()
        if k < 0.04:
            return r.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000])
        if k < 0.2:
            return fb(r.choice([0.0, 1.0, -1.0, 0.5]))
        return fb(r.uniform(lo, hi))
    return {"GetHeightForInterval": [special(r, -0.5, 3) for _ in range(12)],
            "GetIntervalForHeight": [special(r, -0.7, 1.7) for _ in range(8)]}


def base(r, op, n=None, pctl=-1):
    if n is None:
        n = r.randrange(0, 5)
    if pctl == -1:
        pctl = r.randrange(2)
    ids = r.sample(range(9), 5)
    brs = ids[:n]
    br = [bgen(r) for _ in range(10)]
    for i, k in enumerate(brs):
        br[k]["w"]["num"] = i if r.random() < 0.85 else r.randrange(6)
    sn = r.randrange(0, 6)
    saved = {"pyl": pygen(r, sn, r.randrange(2), r.sample(range(9), sn)), "br": [bgen(r) for _ in range(5)],
             "h": [rff(r) for _ in range(5)], "flag": r.randrange(1, 4)}
    return {"op": op, "ctl": [cgen(r), cgen(r)], "pyl": [pygen(r, n, pctl, brs), pygen(r, 0, 1, [])], "br": br, "saved": saved,
            "skip": r.choice([0, 0, 1]), "queues": qgen(r)}


def cbh_case(r):
    c = base(r, "cbh")
    c["brace"] = None if r.random() < 0.1 else r.randrange(9)
    return c


def restore_case(r, sn=None):
    c = base(r, "restore")
    if sn is not None:
        c["saved"]["pyl"] = pygen(r, sn, r.randrange(2), r.sample(range(9), sn))
    return c


def squish_case(r, n=None):
    c = base(r, "squish", n=r.randrange(0, 6) if n is None else n, pctl=r.randrange(2))
    c["ctl_arg"] = CT0 + r.randrange(2)
    return c


def insert_case(r, kind=None, flags=None, nullctl=False):
    n = r.randrange(0, 5)
    if nullctl:
        n = 0
    c = base(r, "insert", n=n, pctl=None if nullctl else -1)
    k = r.randrange(3) if kind is None else kind
    c["level"] = 0 if (k == 0 or nullctl) else (n if k == 1 else r.randint(0, n))
    c["height"] = rff(r) if r.random() < 0.7 else fb(r.uniform(0, 4))
    if (c["height"] & 0x7F800000) == 0x7F800000:
        c["height"] = fb(1.5)
    c["out"] = r.random() < 0.7
    c["flags"] = flags if flags is not None else [r.randrange(2) for _ in range(4)]
    return c


def cbh_edges():
    r = random.Random(6701)
    for i in range(30):
        yield cbh_case(r)


def restore_edges():
    r = random.Random(6702)
    for sn in (0, 1, 2, 3, 4, 5, 5, 0, 2):
        yield restore_case(r, sn)


def squish_edges():
    r = random.Random(6703)
    for n in (0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 2, 3, 4, 5, 5):
        yield squish_case(r, n)


def insert_edges():
    r = random.Random(6704)
    for a in (0, 1):
        for b in (0, 1):
            for c in (0, 1):
                for d in (0, 1):
                    for kind in (0, 1, 2):
                        yield insert_case(r, kind, [a, b, c, d])
    for b in (0, 1):
        yield insert_case(r, 0, [1, b, 0, 0], nullctl=True)
        yield insert_case(r, 0, [0, b, 1, 1], nullctl=True)


TESTS = [
    FnTest("CalculateBraceHeight", ppc, host, edge=cbh_edges, rnd=cbh_case),
    FnTest("RestorePylonDetails", ppc, host, edge=restore_edges, rnd=restore_case),
    FnTest("SquishPylonBracesIfRequired", ppc, host, edge=squish_edges, rnd=squish_case),
    FnTest("InsertBraceIntoPylon", ppc, host, edge=insert_edges, rnd=insert_case),
]
