"""Unit v5962, the embedded-entry counters, EnableRideHead, bthread_cond and CalculateOrientFromTrackMorph, against
libtpw_unit_CountDataNodesByType.dylib (tests/unit_CountDataNodesByType.cpp), main app ("SimTheme Park"),
src/game/TPEmbed.cpp, src/game/online/bthread.cpp:
  CountDataNodesByType 0x101fff0c, FindNthDataNodeIndex 0x101ffe74, EnableRideHead 0x10200230, bthread_cond::bthread_cond 0x10200e34,
  bthread_cond::~bthread_cond 0x10200ea8, bthread_cond::wait 0x10200f30, CalculateOrientFromTrackMorph 0x101fec54

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Where the host's
structures hold native pointers (the ride set, the mesh, the embedded set and its records, the thing table, the bthread_cond handle) the
side supplies the pointer width, the offsets and the strides; the file structures (the mesh header and its embedded array) are the same on
both. The callees outside the unit are fakes on both sides that record their arguments: GAME_ASSERTMSG (condition, text, two words),
ESP_SpawnScript, CPerson::GetHeadSpriteID (the marker word of the person), CreateEvent, WaitForSingleObject, CloseHandle, operator delete
(whether it is the object), SetMatrixIdentity, Z/Y/XRotateLMatrix (they leave their two arguments in distinct cells of the matrix) and
CombineMatricesNoWNoTransDest2nd (it logs the 3 x 3 part of both matrices). sqrt and __fpclassifyf are plain C on both sides.

The world of a case: the embedded flags of the header, the embedded set and its records, the thing table, the sine table of gTPGlobals,
the points, the morph record. What is compared: the answers, the sets and records, the handle, the output matrix, the event log.

CalculateOrientFromTrackMorph runs under HardwareRounding (fmadd, fmsubs and fctiwz). Not tested: a thing id past the table (the original
reads past it), a NULL person for a nonzero peep, a sine table index outside the table (mask above 0xfff), angles whose scaled value is
beyond 2^31 (fctiwz saturates differently in the emulator).
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

SHIM_LIBRARY = "unit_CountDataNodesByType"
MASK = 0xFFFFFFFF

FN_COUNT, FN_FIND, FN_ENABLE = 0x101FFF0C, 0x101FFE74, 0x10200230
FN_CTOR, FN_DTOR, FN_WAIT = 0x10200E34, 0x10200EA8, 0x10200F30
FN_ORIENT = 0x101FEC54
SIZES = {FN_ORIENT: 604}
OWN = {FN_COUNT, FN_FIND, FN_ENABLE, FN_CTOR, FN_DTOR, FN_WAIT, FN_ORIENT}

ASSERT, SPAWN, HEAD = 0x1000DDE8, 0x100DA18C, 0x1011836C
CREATE, WAITOBJ, CLOSE, DELETE = 0x1023A8E8, 0x1023A6F0, 0x1023A8D0, 0x10238218
FPCLASS = 0x10239268
IDENT, ZROT, YROT, XROT, COMB = 0x100C0E28, 0x100C152C, 0x100C1458, 0x100C1308, 0x100C2B5C

ADDR = dict(things=0x1034CBFC, block=0x1029FDA8, tpg=0x103B74C8)

CUR = [0]   # the object the bthread_cond calls work on (operator delete tells whether it got it)


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

STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "isuu"))
STUBS.internal(SPAWN, CL.recorder("ESP_SpawnScript", "Liiifff", ret="i"))
STUBS.internal(CREATE, CL.recorder("CreateEvent", "uuuu", ret="i"))
STUBS.internal(WAITOBJ, CL.recorder("WaitForSingleObject", "uu", ret="i"))
STUBS.internal(CLOSE, CL.recorder("CloseHandle", "u"))


def stub_head(e):
    """CPerson::GetHeadSpriteID(): the person's first word names it in the log."""
    L = e.cl
    L.rec("GetHeadSpriteID", (e.u32(e.r(3)),))
    e.set_r(3, L.ret("GetHeadSpriteID") & MASK)


def stub_delete(e):
    e.cl.rec("delete", (1 if e.r(3) == CUR[0] else 0,))


def stub_fpclass(e):
    x = e.f(1)
    e.set_r(3, 1 if x != x else 4)


def stub_ident(e):
    e.cl.rec("SetMatrixIdentity", ())
    a = e.r(3)
    e.write(a, b"".join(struct.pack(">I", 0x3F800000 if i % 5 == 0 else 0) for i in range(16)))


def rotate_stub(name, k1, k2):
    def stub(e):
        c, s = fb(e.f(1)), fb(e.f(2))
        e.cl.rec(name, (c, s))
        a = e.r(3)
        e.write(a + 4 * k1, struct.pack(">I", c))
        e.write(a + 4 * k2, struct.pack(">I", s))
    return stub


def stub_comb(e):
    for name, a in (("CombineFirst", e.r(3)), ("CombineSecond", e.r(4))):
        e.cl.rec(name, tuple(e.u32(a + 16 * i + 4 * j) for i in range(3) for j in range(3)))


STUBS.internal(HEAD, stub_head)
STUBS.internal(DELETE, stub_delete)
STUBS.internal(FPCLASS, stub_fpclass)
STUBS.internal(IDENT, stub_ident)
STUBS.internal(ZROT, rotate_stub("ZRotateLMatrix", 0, 1))
STUBS.internal(YROT, rotate_stub("YRotateLMatrix", 5, 6))
STUBS.internal(XROT, rotate_stub("XRotateLMatrix", 10, 8))
STUBS.internal(COMB, stub_comb)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.E + "I", w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.E + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def h16(self, a, v):
        self.write(a, struct.pack(self.E + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.E + ("I" if self.PW == 4 else "Q"), v))

    def rd_ptr32(self, a):
        """The low word of a pointer-sized field."""
        w = self.rd_words(a, self.PW // 4)
        return w[0] if self.E == "<" else w[-1]


class Ppc(Side):
    E, PW = ">", 4
    HEADS, MHEADER, MF28, SRECORDS, SF08, RF0C, RSIZE, TSIZE = 8, 4, 0x28, 4, 8, 0xC, 0x14, 0x14
    TBL, SCALE, MSK = 0xC, 8, 4
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["block"]: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def p32(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args, rounding=False):
        if rounding:
            with ROUNDING(self.e):
                r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        else:
            r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    E, PW = "<", 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        L = lib.layout
        (self.HEADS, self.MHEADER, self.MF28, self.SRECORDS, self.SF08, self.RF0C, self.RSIZE, self.TSIZE, _, _, self.base,
         self.TBL, self.SCALE, self.MSK) = L
        CL.host_begin(lib, {1: self.g("block")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cdn_addr(name.encode())

    def p32(self, a):
        return a - self.base

    def alloc(self, n):
        return self.lib.tpw_cdn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args, rounding=False):
        lib = self.lib
        if fn == FN_COUNT:
            return lib.tpw_cdn_count(args[0], args[1] & MASK) & MASK
        if fn == FN_FIND:
            return lib.tpw_cdn_find(args[0], args[1] & MASK, args[2] & MASK) & MASK
        if fn == FN_ENABLE:
            lib.tpw_cdn_enable(args[0], args[1] & MASK, args[2] & MASK)
            return 0
        if fn == FN_CTOR:
            return lib.tpw_cdn_ctor(args[0])
        if fn == FN_DTOR:
            return lib.tpw_cdn_dtor(args[0], s32(args[1] << 16) >> 16)
        if fn == FN_WAIT:
            return lib.tpw_cdn_wait(args[0], s32(args[1])) & 0xFF
        lib.tpw_cdn_orient(*args)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cdn_addr.restype, lib.tpw_cdn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cdn_alloc.restype, lib.tpw_cdn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cdn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cdn_count.restype, lib.tpw_cdn_count.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_cdn_find.restype, lib.tpw_cdn_find.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_cdn_enable.restype, lib.tpw_cdn_enable.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_cdn_ctor.restype, lib.tpw_cdn_ctor.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_cdn_dtor.restype, lib.tpw_cdn_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_cdn_wait.restype, lib.tpw_cdn_wait.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_cdn_orient.restype, lib.tpw_cdn_orient.argtypes = None, [C.c_uint64] * 5
    lib.tpw_cdn_alloc(16)   # reserves the arena: its base is nullptr before the first allocation
    out = (C.c_int64 * 32)()
    n = lib.tpw_cdn_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout[8] == 8 and lib.layout[9] == 64, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_set(s, case):
    """A ride set -> mesh -> header with the case's embedded flags (stride 0x14)."""
    flags = case["flags"]
    n = len(flags)
    E = s.alloc(max(n, 1) * 0x14)
    for i, f in enumerate(flags):
        s.words(E + 0x14 * i, [f, 0x11111111 * (i + 1), 0x22222222, 0x33333333, 0x44444444])
    H = s.alloc(0x100)
    s.h16(H + 0x48, n)
    s.words(H + 0x7C, [s.p32(E)])
    M = s.alloc(0x100)
    s.ptr(M + s.MHEADER, H)
    S = s.alloc(0x100)
    s.ptr(S + s.HEADS, M)
    return S


def build_enable(s, case):
    n = len(case["rflags"])
    R = s.alloc(n * s.RSIZE)
    for i in range(n):
        s.words(R + s.RSIZE * i, [case["rflags"][i]])
        s.words(R + s.RSIZE * i + s.RF0C, [0xA5A5A5A5 ^ i])
    X = s.alloc(0x40)
    s.words(X, [case["sflags"]])
    s.ptr(X + s.SRECORDS, R)
    s.words(X + s.SF08, [case["sf08"]])
    H = s.alloc(0x100)
    s.h16(H + 0x44, case["nodes"])
    s.h16(H + 0x46, case["f46"])
    M = s.alloc(0x100)
    s.ptr(M + s.MHEADER, H)
    s.ptr(M + s.MF28, X)
    things = s.g("things")
    idx = case["peep"] & 0xFFFF
    P0 = s.alloc(16)
    s.words(P0, [case["marker0"]])
    if idx != 0 and case["peep"] != 0:
        P = s.alloc(16)
        s.words(P, [case["marker"]])
        s.ptr(things + idx * s.TSIZE, P)
    s.ptr(things, P0 if case["has0"] else 0)
    return M, X, R, n


def build_orient(s, case):
    tpg = s.g("tpg")
    s.words(tpg + s.MSK, [case["mask"]])
    s.words(tpg + s.SCALE, [case["scale"]])
    s.words(tpg + s.TBL, TABLE)
    p0 = s.alloc(16)
    s.words(p0, case["p0"])
    p1 = s.alloc(16)
    s.words(p1, case["p1"])
    nm = s.alloc(16)
    s.words(nm, case["normal"])
    B = s.alloc(0x80)
    ang = B + 0x10
    s.words(ang, case["angles"])
    morph = ang + case["back"]
    s.h16(morph + 2, case["back"])
    m = s.alloc(64)
    s.words(m, case["matrix"])
    return p0, p1, nm, morph, m


def build_cond(s, case):
    obj = s.alloc(16)
    CUR[0] = obj
    if case["obj"]:
        s.ptr(obj, case["handle"])
        return obj
    return 0


def run(s, case):
    try:
        op = case["op"]
        if op in ("count", "find"):
            S = build_set(s, case)
            if op == "count":
                ans = s.call(FN_COUNT, [S, case["kind"]])
            else:
                ans = s.call(FN_FIND, [S, case["kind"], case["n"]])
            return {"answer": ans, "log": s.log()}
        if op == "enable":
            M, X, R, n = build_enable(s, case)
            s.call(FN_ENABLE, [M, case["index"], case["peep"]])
            return {"set": s.rd_words(X, 1) + s.rd_words(X + s.SF08, 1),
                    "records": [(s.rd_word(R + s.RSIZE * i), s.rd_word(R + s.RSIZE * i + s.RF0C)) for i in range(n)],
                    "log": s.log()}
        if op == "orient":
            p0, p1, nm, morph, m = build_orient(s, case)
            s.call(FN_ORIENT, [p0, p1, nm, morph, m], rounding=True)
            return {"matrix": [canon_f(w) for w in s.rd_words(m, 16)], "log": s.log()}
        obj = build_cond(s, case)
        if op == "ctor":
            r = s.call(FN_CTOR, [obj])
            return {"answer": int(r == obj), "handle": s.rd_ptr32(obj), "log": s.log()}
        if op == "dtor":
            o = obj if case["obj"] else 0
            r = s.call(FN_DTOR, [o, case["flag"]])
            return {"answer": int(r == o), "handle": s.rd_ptr32(obj) if o else 0, "log": s.log()}
        r = s.call(FN_WAIT, [obj, case["ms"]])
        return {"answer": r & 0xFF, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

KINDS = [0x80, 0x1000000, 0x800000, 0x2000000, 0x100000, 0x80000, 0x400000]
ALLBITS = 0
for _k in KINDS:
    ALLBITS |= _k
TABLE = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(0x1000)]


def rfl(r):
    k = r.random()
    if k < 0.08:
        return r.choice([0x3F800000, 0xC2C80000, 0x42C80000, 0x00000000, 0x80000000, 0x7F800000, 0x7FC00000])
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def set_case(r, op, n=None, kind=None, nth=None):
    n = r.choice([0, 1, 2, 3, 5, 8, 12]) if n is None else n
    flags = [r.choice([0, ALLBITS & r.getrandbits(32), r.choice(KINDS), r.getrandbits(32), r.choice(KINDS) | r.choice(KINDS)]) for _ in range(n)]
    kind = r.choice(KINDS + [0, MASK, 3, 0x80 | 0x800000]) if kind is None else kind
    matches = sum(1 for f in flags if f & kind)
    if nth is None:
        nth = r.choice([0, 1, 2, matches, matches + 1, r.randrange(0, 15)])
    return {"op": op, "queues": {}, "flags": flags, "kind": kind, "n": nth}


def set_edges(op):
    r = random.Random(0x5EC0 + len(op))
    yield set_case(r, op, n=0, kind=0x80, nth=1)
    yield set_case(r, op, n=1, kind=0, nth=1)
    yield set_case(r, op, n=1, kind=MASK, nth=1)
    yield set_case(r, op, n=1, kind=MASK, nth=0)
    yield set_case(r, op, n=1, kind=MASK, nth=2)
    for kind in KINDS:
        for nth in (0, 1, 2, 3):
            yield set_case(r, op, n=6, kind=kind, nth=nth)
    c = set_case(r, op, n=4, kind=0x80, nth=1)
    c["flags"] = [0x80, 0x80, 0x80, 0x80]
    for nth in (1, 2, 3, 4, 5):
        c = dict(c, n=nth)
        yield c
    c = dict(c, flags=[0, 0, 0x80000000, 0x80], kind=0x80000000)
    yield c


def count_random(r):
    return set_case(r, "count")


def find_random(r):
    return set_case(r, "find")


HEADS = [0, 1, 7, 0x1234, 0x7FFFFFFF, 0x80000000, 0x80000005, 0xFFFFFFFF, 0x80000000 | 0x1234]


def enable_case(r, n=None, peep=None, has0=None, head=None):
    n = r.choice([1, 2, 3, 4]) if n is None else n
    peep = r.choice([0, 1, 2, 0x27FF, 0x10005, 0x20000 | 0x33, r.randrange(1, 0x2800), r.randrange(0, 0x2800) | (r.getrandbits(16) << 16)]) if peep is None else peep
    if (peep & 0xFFFF) >= 0x2800:
        peep &= 0x27FF | 0xFFFF0000
        peep = (peep & 0xFFFF0000) | (r.randrange(1, 0x2800))
    has0 = r.choice([False, True]) if has0 is None else has0
    if peep != 0 and (peep & 0xFFFF) == 0:
        has0 = True
    head = r.choice(HEADS + [r.getrandbits(32)]) if head is None else head
    return {"op": "enable", "queues": {"GetHeadSpriteID": [head], "ESP_SpawnScript": [r.choice([0, 1, 5, -1 & MASK, r.getrandbits(32)])]},
            "rflags": [r.choice([0, 1, 2, 0x22, r.getrandbits(32)]) for _ in range(n)], "index": r.randrange(n),
            "sflags": r.choice([0, 4, 1, r.getrandbits(32)]), "sf08": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]),
            "nodes": r.choice([0, 1, 0x7FFF, 0xFFFF, r.getrandbits(16)]), "f46": r.getrandbits(16), "peep": peep, "has0": has0,
            "marker": r.getrandbits(32), "marker0": r.getrandbits(32)}


def enable_edges():
    r = random.Random(0xE17AB)
    for peep in (0, 1, 0x27FF, 0x10005, 0xFFFF0007):
        for has0 in (False, True):
            for head in (0, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                yield enable_case(r, peep=peep, has0=has0, head=head)
    c = enable_case(r, n=3, peep=0, head=0)
    c["index"] = 2
    yield c
    yield enable_case(r, n=1, peep=0x10000, has0=True)


def enable_random(r):
    return enable_case(r)


def orient_case(r, **kw):
    scale = kw.get("scale", r.choice([fb(4096 / (2 * math.pi)), fb(r.uniform(10, 1000)), fb(651.8986)]))
    mask = kw.get("mask", r.choice([0xFFF, 0xFFF, 0x7FF, 0x3FF, 0xFF, 0]))
    p0 = kw.get("p0", [fb(r.uniform(-100, 100)) for _ in range(3)] + [0])
    if "p1" in kw:
        p1 = kw["p1"]
    elif r.random() < 0.1:
        p1 = list(p0)
    else:
        p1 = [fb(r.uniform(-100, 100)) for _ in range(3)] + [0]
    normal = kw.get("normal", [fb(r.uniform(-1, 1)) for _ in range(3)] + [0])
    angles = kw.get("angles", [fb(r.uniform(-20, 20)) for _ in range(3)])
    return {"op": "orient", "queues": {}, "mask": mask, "scale": scale, "p0": p0, "p1": p1, "normal": normal, "angles": angles,
            "back": r.choice([0x10, 0x20, 0x24, 0x30]), "matrix": [rfl(r) for _ in range(16)]}


def orient_edges():
    r = random.Random(0x0123)
    z = fb(0.0)
    yield orient_case(r, p1=[fb(1.0), z, z, 0], p0=[z, z, z, 0])
    yield orient_case(r, p1=[z, z, z, 0], p0=[z, z, z, 0])
    yield orient_case(r, p1=[z, fb(2.0), z, 0], p0=[z, z, z, 0], angles=[z, z, z])
    yield orient_case(r, p1=[z, z, fb(-3.0), 0], p0=[z, z, z, 0], angles=[fb(-1.0), fb(-0.001), fb(3.0)])
    yield orient_case(r, p0=[fb(1e30), z, z, 0], p1=[fb(-1e30), z, z, 0])
    yield orient_case(r, p0=[fb(1e-30), z, z, 0], p1=[fb(2e-30), fb(1e-30), z, 0])
    yield orient_case(r, mask=0)
    yield orient_case(r, mask=0xFFF, angles=[fb(0.5), fb(1.0), fb(2.0)])
    yield orient_case(r, angles=[fb(-5.5), fb(-7.25), fb(19.99)])
    yield orient_case(r, normal=[z, z, z, 0])
    for _ in range(6):
        yield orient_case(r)


def orient_random(r):
    return orient_case(r)


def cond_case(r, op, **kw):
    c = {"op": op, "queues": {}, "obj": True, "handle": r.choice([0, 1, 0x1234, 0x7FFFFFFF, 0xFFFFFFFF, r.getrandbits(32)])}
    c["queues"]["CreateEvent"] = [r.choice([0, 1, 0x12345678, 0xFFFFFFFF, r.getrandbits(32)])]
    c["queues"]["WaitForSingleObject"] = [r.choice([0, 0, 1, 0x102, 0xFFFFFFFF, r.getrandbits(32)])]
    c["flag"] = r.choice([-1, 0, 1, 5, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)])
    c["ms"] = r.choice([-1, -5, -0x80000000, 0, 1, 1000, 0x7FFFFFFF, r.randrange(-0x80000000, 0x80000000)])
    c.update(kw)
    return c


def ctor_edges():
    r = random.Random(11)
    for v in (0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678):
        c = cond_case(r, "ctor")
        c["queues"]["CreateEvent"] = [v]
        yield c


def dtor_edges():
    r = random.Random(12)
    for flag in (-1, 0, 1, 2, 0x7FFF, -0x8000):
        yield cond_case(r, "dtor", flag=flag)
    yield cond_case(r, "dtor", flag=1, obj=False)
    yield cond_case(r, "dtor", flag=0, obj=False)
    yield cond_case(r, "dtor", flag=-3, obj=False)


def wait_edges():
    r = random.Random(13)
    for ms in (-1, -0x80000000, -2, 0, 1, 0x7FFFFFFF):
        for res in (0, 1, 0x102):
            c = cond_case(r, "wait", ms=ms)
            c["queues"]["WaitForSingleObject"] = [res]
            yield c
    c = cond_case(r, "wait", ms=5)
    c["queues"]["WaitForSingleObject"] = [0x100000000 - 1]
    yield c


def rnd_op(op):
    def gen(r):
        return cond_case(r, op, obj=(r.random() < 0.9) if op == "dtor" else True)
    return gen


TESTS = [
    FnTest("CountDataNodesByType", ppc, host, edge=lambda: set_edges("count"), rnd=count_random),
    FnTest("FindNthDataNodeIndex", ppc, host, edge=lambda: set_edges("find"), rnd=find_random),
    FnTest("EnableRideHead", ppc, host, edge=enable_edges, rnd=enable_random),
    FnTest("bthread_cond::bthread_cond", ppc, host, edge=ctor_edges, rnd=rnd_op("ctor")),
    FnTest("bthread_cond::~bthread_cond", ppc, host, edge=dtor_edges, rnd=rnd_op("dtor")),
    FnTest("bthread_cond::wait", ppc, host, edge=wait_edges, rnd=rnd_op("wait")),
    FnTest("CalculateOrientFromTrackMorph", ppc, host, edge=orient_edges, rnd=orient_random),
]
