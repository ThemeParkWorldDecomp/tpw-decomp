"""Unit w147: the coaster cross-section builders, against libtpw_unit_CrossSections.dylib (tests/unit_CrossSections.cpp), main app
("SimTheme Park"), src/game/rides/CoasterType.cpp:
  BuildCrossSectionPoints 0x10036d5c, BuildCrossSectionEdges 0x10037aa8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back.
MReleaseAllocF and MReleaseFreeF are fakes on both sides that log their arguments (the allocator hands out zeroed blocks followed by
512 guard bytes of 0xa5; the host's pointer table is twice the Mac's, so the host log's second allocation is halved).

The world of a case: a balance (the Mac's 0x3b80 bytes, random words everywhere, then the 20 point rows, 20 edge rows, the cross-section
count and entries of the case), a type (a 0x5a pattern, +0x40 set), a table of 21 records and the out word, all pattern filled so
that untouched words show. What is compared: the answer, *out, the allocation and free log, the type's +0x90 and +0x94 (as the block
or the pattern), every word of the 21 records (counts as numbers, pointers as (block, index in the block)), and the points and edges
blocks with their guard bytes (floats as bits, a NaN as one value in the sum word; edges as one word and four halfwords).

No fused operations (fadds is one rounding). Not tested: a section row above the number of rows (the Mac reads past the table), more
than 20 sections (the Mac reads the count word as an entry), a total whose block size is huge (the sum of negative counts), a
signalling NaN in the summed word or in the type's +0x40 (the processors make different NaNs).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CrossSections"
MASK = 0xFFFFFFFF

FN_P, FN_E = 0x10036D5C, 0x10037AA8
SIZES = {FN_P: 3308, FN_E: 2580}
OWN = set(SIZES)

ALLOC, FREE = 0x10064BD4, 0x10064C54          # MReleaseAllocF, MReleaseFreeF
GUARD = 512
PAT = 0x5A5A5A5A

# The Mac's layout: record (size, 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14), type (size, +0x40, +0x90, +0x94), balance (size, first slot, stride).
MAC = {"rec": (0x18, 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14), "type": (0x35C, 0x40, 0x90, 0x94), "bal": (0x3B80, 8, 4), "ptr": 4}
HOST_LAYOUT = [40, 0, 4, 8, 16, 24, 32, 944, 68, 144, 152, 30464, 16, 20, 12]

SLOTS = 3806
POINT_COUNT, POINT_STRIDE, POINT_DATA = 0x290, 0x194, 0x100
EDGE_COUNT, EDGE_STRIDE, EDGE_DATA = 0x2180, 0xF4, 0x2090
SECTIONS, SECTION_COUNT = 0x33A0, 0x3530


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    """void* MReleaseAllocF(uint32_t size, uint32_t flags): a zeroed block and 512 guard bytes; the block gets the next label."""
    size = e.r(3)
    e.cl.rec("ReleaseAlloc", (size, e.r(4)))
    a = e.alloc(size + GUARD, bytes(size) + b"\xa5" * GUARD, alignment=8)
    e.cl.newlab(a)
    e.set_r(3, a)


def stub_free(e):
    """void* MReleaseFreeF(void* block): logs the block's label."""
    e.cl.rec("ReleaseFree", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(FREE, stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_ptr(self, a):
        return struct.unpack(self.fmt[0] + ("I" if self.lay["ptr"] == 4 else "Q"), self.read(a, self.lay["ptr"]))[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    lay = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n, alignment=8)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, FN_P if op == "points" else FN_E, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        h = HOST_LAYOUT
        self.lay = {"rec": tuple(h[0:7]), "type": tuple(h[7:11]), "bal": (h[11], h[12], 8), "ptr": 8}
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_xs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        log = list(CL.host_end(self.lib))
        allocs = [i for i, ev in enumerate(log) if ev[0] == "ReleaseAlloc"]
        if len(allocs) >= 2:
            name, ints, texts = log[allocs[1]]
            log[allocs[1]] = (name, (ints[0] // 2,) + tuple(ints[1:]), texts)
        return log

    def call(self, op, args):
        if op == "points":
            return self.lib.tpw_xs_points(*args) & MASK
        return self.lib.tpw_xs_edges(*args[:3]) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_xs_alloc.restype, lib.tpw_xs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_xs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_xs_points.restype, lib.tpw_xs_points.argtypes = C.c_int32, [C.c_uint64] * 4
    lib.tpw_xs_edges.restype, lib.tpw_xs_edges.argtypes = C.c_int32, [C.c_uint64] * 3
    out = (C.c_int64 * 32)()
    n = lib.tpw_xs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the balance, the type, the records and the out word (offsets are the Mac's); returns their addresses."""
    L = s.lay
    bal_size, bal_off, stride = L["bal"]
    rng = random.Random(case["seed"])
    slot = [rng.getrandbits(32) for _ in range(SLOTS)]

    def put(off, v):
        slot[(off - 8) // 4] = v & MASK

    for k in range(20):
        put(POINT_COUNT + POINT_STRIDE * k, case["pc"][k])
        put(EDGE_COUNT + EDGE_STRIDE * k, case["ec"][k])
        for j in range(20):
            for w in range(5):
                put(POINT_DATA + POINT_STRIDE * k + 0x14 * j + 4 * w, case["pdata"][k][j][w])
            for w in range(3):
                put(EDGE_DATA + EDGE_STRIDE * k + 0xC * j + 4 * w, case["edata"][k][j][w])
    for i, (row, erow, row2, bits) in enumerate(case["secs"]):
        put(SECTIONS + 0x14 * i, row)
        put(SECTIONS + 0x14 * i + 4, erow)
        put(SECTIONS + 0x14 * i + 0xC, row2)
        put(SECTIONS + 0x14 * i + 0x10, bits)
    put(SECTION_COUNT, case["n"])
    pad = b"\x5a" * (stride - 4)
    balance = s.alloc(bal_size)
    s.write(balance, b"\x5a" * bal_size)
    s.write(balance + bal_off, b"".join(struct.pack(s.fmt, v) + pad for v in slot))
    type_size, off40, _, _ = L["type"]
    ty = s.alloc(type_size)
    s.write(ty, b"\x5a" * type_size)
    s.words(ty + off40, [case["mf40"]])
    rec_size = L["rec"][0]
    records = s.alloc(rec_size * 21)
    s.write(records, b"\x5a" * (rec_size * 21))
    out = s.alloc(16)
    s.write(out, b"\x5a" * 16)
    return ty, records, balance, out


def decode(s, p, base, size, unit, name):
    """A pointer as (name, index, remainder) in the block at `base`, or 0, or 'pat' for the pattern, or the raw value."""
    if p == 0:
        return 0
    if p in (PAT, 0x5A5A5A5A5A5A5A5A):
        return "pat"
    if base and base <= p < base + size + GUARD:
        d = p - base
        return (name, d // unit, d % unit)
    return ("?", p)


def observe(s, case, op, ty, records, out, answer):
    """Everything the call may change, read back the same way on both sides."""
    L = s.lay
    _, _, o90, o94 = L["type"]
    rec_size, r0, r4, r8, rc, r10, r14 = L["rec"]
    res = {"answer": answer, "log": s.log(), "out": s.rd_word(out)}
    total = sum(case["pc"]) & MASK if op == "points" else sum(case["ec"]) & MASK
    psize = (sum(case["pc"]) * 0x14) & MASK
    esize = (sum(case["ec"]) * 0xC) & MASK
    bp = s.rd_ptr(ty + o90)
    be = s.rd_ptr(ty + o94)
    res["t90"] = "pat" if bp in (PAT, 0x5A5A5A5A5A5A5A5A) else (0 if bp == 0 else "block")
    res["t94"] = "pat" if be in (PAT, 0x5A5A5A5A5A5A5A5A) else (0 if be == 0 else "block")
    rows = []
    for i in range(21):
        a = records + rec_size * i
        rows.append((s.rd_word(a + r0), s.rd_word(a + r4), decode(s, s.rd_ptr(a + r8), bp, psize, 0x14, "P"),
                     decode(s, s.rd_ptr(a + rc), be, esize, 0xC, "E"), decode(s, s.rd_ptr(a + r10), bp, psize, 0x14, "P"),
                     s.rd_word(a + r14)))
    res["records"] = rows
    if res["t90"] == "block":
        w = s.rd_words(bp, (psize + GUARD) // 4)
        res["points"] = [canon_f(x) if i % 5 == 1 else x for i, x in enumerate(w)]
    if res["t94"] == "block":
        data = s.read(be, esize + GUARD)
        n = (len(data) // 12) * 12
        e = s.fmt[0]
        res["edges"] = [list(x) for x in struct.iter_unpack(e + "I4H", data[:n])] + [data[n:]]
    return res


def run(s, case):
    try:
        ty, records, balance, out = build(s, case)
        op = case["op"]
        args = [ty, records, balance, out] if op == "points" else [ty, records, balance]
        answer = s.call(op, args)
        return observe(s, case, op, ty, records, out, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x80000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF,
           0x00800000, 0x007FFFFF, 0xFF7FFFFF]


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value (no signalling NaN)."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rcount(r):
    """A row count: empty a third of the time, else 1 to 20."""
    x = r.random()
    return 0 if x < 0.35 else r.randint(1, 20) if x < 0.7 else r.randint(1, 4)


def world(op, seed, pc=None, ec=None, secs=None, n=None, mf40=None, edata=None, pdata=None):
    """A case: the counts, the data, the cross sections and the type's +0x40 (random where not given)."""
    r = random.Random(seed)
    pc = [rcount(r) for _ in range(20)] if pc is None else pc
    ec = [rcount(r) for _ in range(20)] if ec is None else ec
    rows_p = max([k + 1 for k in range(20) if pc[k] != 0] or [0])
    rows_e = max([k + 1 for k in range(20) if ec[k] != 0] or [0])
    if n is None:
        n = r.choice([0, 1, 2, 3, 5, 10, 20, r.randint(1, 20)])
    if secs is None:
        secs = []
        for _ in range(20):
            row = r.randint(1, max(rows_p, 1))
            erow = r.randint(1, max(rows_e, 1))
            row2 = 0 if r.random() < 0.5 else (row if r.random() < 0.6 else r.randint(1, max(rows_p, 1)))
            secs.append((row, erow, row2, rf(r)))
    secs = list(secs) + [(0, 0, 0, 0)] * (20 - len(secs))
    return {"op": op, "queues": {}, "seed": seed, "pc": pc, "ec": ec, "n": n, "secs": secs,
            "mf40": rf(r) if mf40 is None else mf40,
            "pdata": pdata or [[[rf(r) for _ in range(5)] for _ in range(20)] for _ in range(20)],
            "edata": edata or [[[r.choice([r.getrandbits(32), r.getrandbits(16), r.randrange(0x10000, 1 << 32), 0, 0xFFFFFFFF,
                                           0x8000, 0xFFFF8000]) for _ in range(3)] for _ in range(20)] for _ in range(20)]}


def edges(op):
    """Every branch of the asm at least once, each boundary on both sides."""
    def gen():
        seed = 1000 if op == "points" else 2000
        counts = lambda **kw: [kw.get(str(k), 0) for k in range(20)]
        one = [(1, 1, 0, 0)]
        # no rows: nothing allocated
        yield world(op, seed, pc=[0] * 20, ec=[0] * 20)
        # a total of 0 with nonzero rows
        yield world(op, seed + 1, pc=counts(**{"0": 3, "1": -3}), ec=counts(**{"0": 3, "1": -3}), secs=one, n=1)
        # each row alone (the offsets of every row), the section naming row 1
        for k in range(20):
            c = [0] * 20
            c[k] = 1 + k % 4
            yield world(op, seed + 10 + k, pc=c, ec=c, secs=one, n=1)
        # only row 20, sections naming row 1 (its data) and row 20 (a zero entry)
        c = [0] * 19 + [3]
        yield world(op, seed + 40, pc=c, ec=c, secs=[(1, 1, 0, 5), (20, 20, 0, 6)], n=2)
        # an empty row between used ones: the shift (rows 1 and 3 used, a section on row 2 gets row 3's points)
        c = [2, 0, 3] + [0] * 17
        yield world(op, seed + 41, pc=c, ec=c, secs=[(1, 1, 0, 0), (2, 2, 0, 0), (3, 3, 0, 0)], n=3)
        # all 20 rows full, 20 sections
        full = [20] * 20
        yield world(op, seed + 42, pc=full, ec=full, secs=[(k + 1, 20 - k, 0, 1) for k in range(20)], n=20)
        # negative counts: stored, nothing copied, the total wraps (positive rows overrun the block into the guard)
        yield world(op, seed + 43, pc=counts(**{"0": 5, "1": -3}), ec=counts(**{"0": 5, "1": -3}), secs=[(1, 1, 0, 0), (2, 2, 0, 0)], n=2)
        yield world(op, seed + 44, pc=counts(**{"0": 4, "2": -1, "3": 2}), ec=counts(**{"0": 4, "2": -1, "3": 2}),
                    secs=[(1, 1, 0, 0), (2, 2, 0, 0), (3, 3, 0, 0)], n=3)
        yield world(op, seed + 45, pc=counts(**{"0": -2, "1": 6}), ec=counts(**{"0": -2, "1": 6}), secs=[(1, 1, 0, 0), (2, 2, 0, 0)], n=2)
        # section counts: 0, negative, 1, 20
        for n in (0, -1, -100, 1, 20):
            yield world(op, seed + 50 + (n % 7), n=n, pc=[3] * 20, ec=[2] * 20,
                        secs=[(1 + k % 20, 1 + k % 20, 0, 3) for k in range(20)])
        # a section row 0 at the first and the last section (return 0, the leak)
        for bad in (0, 1, 2):
            secs = [(1, 1, 0, 0), (1, 1, 0, 0), (1, 1, 0, 0)]
            row, erow, row2, bits = secs[bad]
            secs[bad] = (0, erow, row2, bits)
            yield world(op, seed + 60 + bad, pc=[2] * 20, ec=[2] * 20, secs=secs, n=3)
            secs = [(1, 1, 0, 0), (1, 1, 0, 0), (1, 1, 0, 0)]
            secs[bad] = (row, 0, row2, bits)
            yield world(op, seed + 63 + bad, pc=[2] * 20, ec=[2] * 20, secs=secs, n=3)
        # the second row: absent, equal counts, unequal counts; and a second row on the last section
        yield world(op, seed + 70, pc=[2, 2, 3] + [0] * 17, ec=[2] * 20, secs=[(1, 1, 2, 0x3F800000), (3, 1, 3, 0x40000000)], n=2)
        yield world(op, seed + 71, pc=[2, 2, 3] + [0] * 17, ec=[2] * 20, secs=[(1, 1, 3, 7), (2, 1, 0, 8)], n=2)
        yield world(op, seed + 72, pc=[2, 2, 3] + [0] * 17, ec=[2] * 20, secs=[(1, 1, 0, 7), (2, 1, 1, 8), (3, 1, 3, 9)], n=3)
        yield world(op, seed + 73, pc=[2, 0, 2] + [0] * 17, ec=[2] * 20, secs=[(1, 1, 2, 7)], n=1)
        # NaNs, infinities, denormals, -0.0 in the summed word and the type's +0x40
        specials = [0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x80000001, 0x80000000, 0, 0x007FFFFF, 0x7F7FFFFF, 0xFF7FFFFF,
                    0x3F800000, 0xBF800000]
        for i, a in enumerate(specials):
            for j, b in enumerate(specials):
                if (i + j) % 3 == 0 or i == j:
                    pdata = [[[0x3F800000, a, 0x80000000, 0x7FC00000, 0xFF800000] for _ in range(20)] for _ in range(20)]
                    yield world(op, seed + 100 + i * 12 + j, pc=[1] * 20, ec=[1] * 20, secs=one, n=1, mf40=b, pdata=pdata)
        # edge words above 0xffff and negative
        words = [0x12345678, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0xFFFF0000, 0x0001FFFF, 1, 0x8000, 0xFFFF8000, 0x10000]
        for i, w in enumerate(words):
            ed = [[[w, words[(i + 1) % len(words)], words[(i + 2) % len(words)]] for _ in range(20)] for _ in range(20)]
            yield world(op, seed + 400 + i, pc=[2] * 20, ec=[2] * 20, secs=one, n=1, edata=ed)
    return gen


def rnd(op):
    return lambda r: world(op, r.getrandbits(48))


TESTS = [
    FnTest("BuildCrossSectionPoints", ppc, host, edge=edges("points"), rnd=rnd("points")),
    FnTest("BuildCrossSectionEdges", ppc, host, edge=edges("edges"), rnd=rnd("edges")),
]
