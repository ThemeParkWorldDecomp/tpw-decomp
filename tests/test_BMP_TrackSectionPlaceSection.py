"""Unit v5566, BMP_TrackSectionPlaceSection against libtpw_unit_BMP_TrackSectionPlaceSection.dylib
(tests/unit_BMP_TrackSectionPlaceSection.cpp), main app ("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_TrackSectionPlaceSection 0x1002ef98

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, pointer words as arena offsets) for the translation. The only callee
outside the unit is TRACE (a fake on both sides that records its text); sqrt is the C library's.

The world of a case: the list at 0x102b56c0 (magic, ride records, the buoy, collision object and section pools with their counts), the
ride record of the id (kind, extent words, existing sections, existing collision objects), pools with a given number of used entries.
What is compared: the answer, the TRACE log, the list words, and every word of every block (pointer words as block name + offset).

The sqrt is run under HardwareRounding (fctiwz). Not tested: a full or missing buoy pool (the original then reads address 0), a type
with no jump table entry and no 0x10000 bit (the original then writes through a NULL buoy), a negative square root argument.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_BMP_TrackSectionPlaceSection"
MASK = 0xFFFFFFFF

FN_A = 0x1002ef98
SIZES = {FN_A: 4080}
OWN = set(SIZES)

CALLEE_TRACE = 0x1006fcfc

ADDR = dict(list=0x102b56c0)
HOST_LAYOUT = [0x100]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(CALLEE_TRACE, CL.recorder("TRACE", "s"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def canon(self, w):
        """A pointer word into one of the case's blocks as (name, offset); anything else as it is."""
        real = self.real(w)
        if w:
            for name, a, size in self.blocks:
                if a <= real < a + size:
                    return (name, real - a)
        return w

    def dump(self, a, n, spec=None):
        """The words; those at the pointer positions (stride in words, indexes) canonicalised."""
        ws = self.rd_words(a, n)
        if spec is None:
            return ws
        stride, idxs = spec
        return [self.canon(w) if i % stride in idxs else w for i, w in enumerate(ws)]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks = []
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a):
        return a

    def real(self, w):
        return w

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_a(self, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_A, [x & MASK for x in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks = []
        self.base = lib.tpw_tsp_base()
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tsp_addr(name.encode())

    def ptr(self, a):
        return a - self.base if a else 0

    def real(self, w):
        return w + self.base if w else 0

    def alloc(self, n):
        a = self.lib.tpw_tsp_alloc(n)
        self.base = self.lib.tpw_tsp_base()       # the arena is reserved by the first allocation
        return a

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_a(self, args):
        return self.lib.tpw_tsp_a(*[s32(x) for x in args[:6]], args[6] & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_tsp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype, getattr(lib, p + "base").argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "a").restype = C.c_uint32
    getattr(lib, p + "a").argtypes = [C.c_int32] * 6 + [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

BUOY, COL, SEC, RIDE = 0x20, 0x1c, 0x28, 0xd0


def build(s, case):
    r = random.Random(case["seed"])
    rnd = lambda: 0x7A000000 | r.getrandbits(20)      # data words that are never a pointer into a block

    def blk(name, size):
        a = s.alloc(size)
        s.blocks.append((name, a, size))
        return a

    def pool(name, count, stride, used):
        a = blk(name, count * stride)
        ws = [0] * (count * stride // 4)
        for i in range(min(used + 3, count)):
            for j in range(stride // 4):
                ws[i * stride // 4 + j] = rnd()
            if i < used:
                ws[i * stride // 4] |= 1
            else:
                ws[i * stride // 4] &= ~1
        s.words(a, ws)
        return a

    rides = blk("rides", 256 * RIDE)
    buoys = pool("buoys", 0x800, BUOY, case["buoys_used"])
    cols = pool("cols", 0x400, COL, case["cols_used"])
    secs = pool("secs", 0x400, SEC, case["secs_used"])
    idx = case["idx"]
    rw = [rnd() for _ in range(RIDE // 4)]
    rw[0] = case["kind"]
    rw[8 // 4] = case["w0"]
    rw[0xc // 4] = case["w1"]
    chain = blk("chain", max(1, len(case["chain"])) * SEC)
    cw = []
    for i, (cx, cy) in enumerate(case["chain"]):
        e = [rnd() for _ in range(SEC // 4)]
        e[0x14 // 4], e[0x18 // 4] = cx, cy
        e[0x24 // 4] = s.ptr(chain + (i + 1) * SEC) if i + 1 < len(case["chain"]) else 0
        cw += e
    s.words(chain, cw)
    rw[0xbc // 4] = s.ptr(chain) if case["chain"] else 0
    colch = blk("colch", max(1, case["colchain"]) * COL)
    cw = []
    for i in range(case["colchain"]):
        e = [rnd() for _ in range(COL // 4)]
        e[0x14 // 4] = s.ptr(colch + (i + 1) * COL) if i + 1 < case["colchain"] else 0
        cw += e
    s.words(colch, cw)
    rw[0xa8 // 4] = s.ptr(colch) if case["colchain"] else 0
    rw[0xac // 4] = case["colcount"]
    s.words(rides + idx * RIDE, rw)
    lw = [case["magic"], rnd(), s.ptr(rides), rnd(), rnd(), rnd(),
          s.ptr(buoys) if case["buoys_used"] >= 0 else 0, case["nbuoys"],
          s.ptr(cols) if case["cols_used"] >= 0 else 0, case["ncols"],
          s.ptr(secs) if case["secs_used"] >= 0 else 0, case["nsecs"], rnd(), rnd(), rnd(), rnd()]
    s.words(s.g("list"), lw)


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "list": s.dump(s.g("list"), 16, (16, {2, 6, 8, 10}))}
    spec = {"rides": (0x34, {0xa8 // 4, 0xbc // 4}), "buoys": (8, {5, 6, 7}), "cols": (7, {4, 5, 6}), "secs": (10, {7, 8, 9}),
            "chain": (10, {7, 8, 9}), "colch": (7, {4, 5, 6})}
    for name, a, size in s.blocks:
        out[name] = s.dump(a, size // 4, spec[name])
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call_a(case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

SAFE_KINDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13]
BUOYED = tuple(range(1, 14))      # the jump table's entries 1..13 place buoys; 0 and 14.. place none


def mk(r, **kw):
    kind = kw.get("kind", r.choice([-5, -8, -12, -13, -1, 0, 3, r.randrange(-20, 20)]))
    idx = kw.get("idx", r.randrange(256))
    magic = kw.get("magic", 0x4A454647)
    typ_lo = kw.get("lo", r.choice(SAFE_KINDS + [r.randrange(0, 20)]))
    typ = typ_lo | (r.choice([0, 0x20000]))
    if "skip" in kw:
        typ |= 0x10000 if kw["skip"] else 0
    elif typ_lo not in BUOYED or r.random() < 0.2:
        typ |= 0x10000
    chain = kw.get("chain", [(r.randrange(-50000, 50000), r.randrange(-50000, 50000)) for _ in range(r.choice([0, 0, 1, 2, 3]))])
    x = kw.get("x", r.choice([0, -1, 5, r.randrange(-60000, 60000)]))
    y = kw.get("y", r.choice([0, -1, 7, r.randrange(-60000, 60000)]))
    id_ = (((kind << 8) | idx) & MASK)
    if "bad_id" in kw:
        id_ = (id_ + kw["bad_id"]) & MASK
    c = {"seed": r.getrandbits(32), "idx": idx, "kind": kind & MASK, "magic": magic,
         "w0": kw.get("w0", r.randrange(-2000, 2000)) & MASK, "w1": kw.get("w1", r.randrange(-2000, 2000)) & MASK,
         "chain": [(a & MASK, b & MASK) for a, b in chain], "colchain": kw.get("colchain", r.choice([0, 0, 1, 2])),
         "colcount": r.randrange(0, 50), "nbuoys": r.randrange(0, 100), "ncols": r.randrange(0, 100), "nsecs": r.randrange(0, 100),
         "buoys_used": kw.get("buoys_used", r.choice([0, 1, 5, r.randrange(0, 40), r.randrange(0, 0x7f0)])),
        "garbage": 0,
         "cols_used": kw.get("cols_used", r.choice([0, 1, r.randrange(0, 40), r.randrange(0, 0x3f0)])),
         "secs_used": kw.get("secs_used", r.choice([0, 1, r.randrange(0, 40), r.randrange(0, 0x3f0)]))}
    c["args"] = [id_, x & MASK, y & MASK, typ, r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]
    if "dup" in kw and chain:
        c["args"][1], c["args"][2] = chain[kw["dup"] % len(chain)][0] & MASK, chain[kw["dup"] % len(chain)][1] & MASK
    return c


def a_edges():
    r = random.Random(5566)
    for lo in list(range(14)) + [14, 15, 0xffff]:
        for skip in (False, True):
            if not skip and lo not in BUOYED:
                continue
            yield mk(r, lo=lo, skip=skip)
    for kind in (-5, -8, -12, -13, -1, 0):
        yield mk(r, lo=11, skip=False, kind=kind, w0=100, w1=100)
    yield mk(r, lo=11, skip=True, w0=0, w1=0)
    yield mk(r, lo=11, skip=False, w0=-2000, w1=2000)
    yield mk(r, lo=11, skip=False, w0=2000, w1=-2000)
    yield mk(r, lo=12, skip=False, idx=0)
    yield mk(r, lo=12, skip=False, idx=255)
    yield mk(r, lo=1, magic=0)
    yield mk(r, lo=1, magic=0x4A454648)
    yield mk(r, lo=1, bad_id=1)
    yield mk(r, lo=1, bad_id=0x100)
    yield mk(r, lo=1, bad_id=-256)
    for d in range(4):
        yield mk(r, lo=1, chain=[(1, 2), (3, 4), (5, 6)], dup=d)
    yield mk(r, lo=1, chain=[(1, 2)], x=1, y=3)
    yield mk(r, lo=1, chain=[(1, 2)], x=2, y=2)
    yield mk(r, lo=2, secs_used=0x400)
    yield mk(r, lo=2, secs_used=-1)
    yield mk(r, lo=2, secs_used=0x3ff)
    yield mk(r, lo=2, cols_used=0x400)
    yield mk(r, lo=2, cols_used=-1)
    yield mk(r, lo=2, cols_used=0x3ff)
    yield mk(r, lo=11, skip=False, buoys_used=0x800 - 5)
    yield mk(r, lo=3, skip=False, buoys_used=0x800 - 2)
    yield mk(r, lo=12, skip=False, buoys_used=0x7ff)
    yield mk(r, lo=0, skip=True, buoys_used=0x800)
    yield mk(r, lo=14, skip=True, buoys_used=-1)
    yield mk(r, lo=2, colchain=0)
    yield mk(r, lo=2, colchain=3)
    yield mk(r, lo=4, skip=False, colchain=1)


def a_random(r):
    return mk(r)


TESTS = [
    FnTest("BMP_TrackSectionPlaceSection", ppc, host, edge=a_edges, rnd=a_random),
]
