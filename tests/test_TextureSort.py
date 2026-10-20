"""The texture cache's sorting and lists, and CQSort (engine library, src/tb_texturecache_sort.cpp
and src/tb_qsort.cpp): CQSort's eight sorts (ulAsc, ulDes, pulAsc, pfDes, each with the three
argument core and the six argument entry point), CTextureCache::Textures_QSort, Textures_QSortFree,
SortTextures, Entry_QSortInit, SortEntries, SortByCriteria, SortByAddress, MergeListsConditional
and CreateNonWrappingTextureList.

CQSort. A case is one call on a buffer of records (the Mac's big-endian words, the host's
little-endian ones, from the same list of words) or on a list of pointers to records. The records,
the list (as record numbers), the scratch buffer and the sorter's three words are compared.

The cache. A case is one call on a small world: the cache with six rows of pages, up to 128
pages (a link and an OpenGL texture each), up to 4,400 texture entries in one array, four
spare place records, an array of entries to sort and a list head. The test builds the world on
both sides from the same description (the host library reads it as a flat list, the PowerPC side
is written into emulator memory), runs the call and compares its answer, the recorded calls and
the state of everything the call could change, read in the order tests/unit_TextureSort.cpp
writes the host's.

Everything outside the functions is a fake on both sides that records its arguments: the entries'
FlushEntryFlags and StoreAsWrapping, the pages' CalcUsage and virtual GetPixelCount, the cache's
AutoMipmap and Entry_Compress and the assertion handler. CalcUsage and the two cache steps can
assign counters the sorts read afterwards (so an order of reads and calls that differs from the
original's shows). CTextureBase::IsPositionLocked and the OpenGL texture's constructor run as the
original.

The original's fused multiply-adds (SortEntries' three limits) and its fctiwz (the sort keys and
the area limit) run under HardwareRounding, as in test_TextureFrame.py.
"""
import copy
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_TextureSort"

QS = dict(ulAsc3=0x1001ffe8, ulDes3=0x10020160, pulAsc3=0x1001fd38, pfDes3=0x1001fe90,
          ulAsc6=0x10020398, ulDes6=0x100203f8, pulAsc6=0x100202d8, pfDes6=0x10020338)
FN = dict(QSort=0x10033098, QSortFree=0x100331f8, SortTextures=0x1003335c, EntryQSortInit=0x10034e08,
          SortEntries=0x10035300, SortByCriteria=0x100358ec, SortByAddress=0x10035ab0, Merge=0x10035be0,
          CreateNW=0x10035e6c)
SIZES = dict(QSort=280, QSortFree=280, SortTextures=760, EntryQSortInit=68, SortEntries=1464, SortByCriteria=384,
             SortByAddress=236, Merge=560, CreateNW=544)
FLUSH, STORE_WRAP, CALC_USAGE, AUTO_MIPMAP, COMPRESS, GET_PIXELS = 0x1004168c, 0x10041848, 0x10038670, 0x10035244, 0x1003514c, 0x10046a94
# The members they reach that run as the original: the OpenGL texture's and CTextureBase's
# constructors and CTextureBase::IsPositionLocked.
INNER = [0x10045d88, 0x10032b58, 0x10032c14]
QSORT_OBJECT = 0x100b161c        # the engine's sorter (BSS)
FREQUENCY = 0x10052d74           # gFrequencyIndex
MASK = 0xFFFFFFFF
NAN, INF = math.nan, math.inf


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def fb(x):
    return T.f32_bits(x)


def canon(word):
    """A single-precision value's bits with every NaN the same."""
    return 0x7FC00000 if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF) else word


def signed64(v):
    v &= 0xFFFFFFFFFFFFFFFF
    return v - (1 << 64) if v >= (1 << 63) else v


# ================================================================= CQSort

QS_OPS = [("ulAsc", 3), ("ulDes", 3), ("pulAsc", 3), ("pfDes", 3), ("ulAsc", 6), ("ulDes", 6), ("pulAsc", 6), ("pfDes", 6)]
QS_ADDR = [QS["ulAsc3"], QS["ulDes3"], QS["pulAsc3"], QS["pfDes3"], QS["ulAsc6"], QS["ulDes6"], QS["pulAsc6"], QS["pfDes6"]]


def qs_ppc(e, case):
    op = case["op"]
    words = case["words"]            # records, each a list of 32-bit words
    n = len(words)
    per = case["per"]
    rec_size = 4 * per
    rec = e.alloc(max(n * rec_size, 4) + 16, b"".join(struct.pack(">%dI" % per, *r) for r in words) + b"\xa5" * 16)
    tmp = e.alloc(rec_size + 16, struct.pack(">%dI" % per, *case["tmp"]) + b"\xa5" * 16)
    qs = e.alloc(12, b"\xa5" * 12)
    pointer_sort = op in (3, 4, 7, 8)
    arr = 0
    if pointer_sort:
        arr = e.alloc(max(n, 1) * 4 + 16, b"".join(struct.pack(">I", rec + i * rec_size) for i in case["order"]))
    data = arr if pointer_sort else rec
    size, key_off, lo, hi = case["size"], case["koff"] * 4, case["lo"], case["hi"]
    if op <= 4:
        e.write(qs, struct.pack(">III", size & MASK, key_off & MASK, tmp))
        T.call_ppc(e, QS_ADDR[op - 1], [qs, data, lo & MASK, hi & MASK])
    else:
        e.write(qs, struct.pack(">III", 0x1111, 0x2222, 0))
        T.call_ppc(e, QS_ADDR[op - 1], [qs, data, tmp, size & MASK, key_off & MASK, lo & MASK, hi & MASK])
    out = {}
    if pointer_sort:
        out["order"] = [(e.u32(arr + 4 * i) - rec) // rec_size for i in range(len(case["order"]))]
    else:
        out["records"] = [list(struct.unpack(">%dI" % per, e.read(rec + i * rec_size, rec_size))) for i in range(n)]
    out["tmp"] = list(struct.unpack(">%dI" % per, e.read(tmp, rec_size)))
    guard = e.read(rec + n * rec_size, 16)
    out["guard"] = guard
    s, k, t = struct.unpack(">iiI", e.read(qs, 12))
    if t == 0:
        code = 0
    elif t == tmp:
        code = -2
    elif rec <= t < rec + n * rec_size:
        code = (t - rec) // rec_size + 1
    else:
        code = -3
    out["sorter"] = [s, k, code]
    return out


def qs_host(lib, case):
    op = case["op"]
    words = case["words"]
    n = len(words)
    per = case["per"]
    rec_size = 4 * per
    rec = C.create_string_buffer(b"".join(struct.pack("<%dI" % per, *r) for r in words) + b"\xa5" * 16, max(n * rec_size, 0) + 16)
    tmp = C.create_string_buffer(struct.pack("<%dI" % per, *case["tmp"]) + b"\xa5" * 16, rec_size + 16)
    pointer_sort = op in (3, 4, 7, 8)
    order = (C.c_int32 * max(len(case["order"]), 1))(*case["order"])
    out = (C.c_int64 * 3)()
    fn = lib.tpw_ts_qsort
    fn.argtypes = [C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_int32, C.c_int32, C.c_int32, C.c_int32,
                   C.c_int32, C.c_void_p]
    fn(op, C.addressof(rec), rec_size, C.addressof(order), C.addressof(tmp), T.s32(case["size"]), case["koff"] * 4,
       T.s32(case["lo"]), T.s32(case["hi"]), len(case["order"]) if pointer_sort else n, C.addressof(out))
    res = {}
    if pointer_sort:
        res["order"] = list(order)[:len(case["order"])]
    else:
        raw = rec.raw
        res["records"] = [list(struct.unpack("<%dI" % per, raw[i * rec_size:(i + 1) * rec_size])) for i in range(n)]
    res["tmp"] = list(struct.unpack("<%dI" % per, tmp.raw[:rec_size]))
    res["guard"] = rec.raw[n * rec_size:n * rec_size + 16]
    res["sorter"] = [int(out[0]), int(out[1]), int(out[2])]
    return res


KEY_PATTERNS = ["random", "few", "ascending", "descending", "equal", "extreme", "pairs"]
UKEYS = [0, 1, 2, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF, 0x12345678, 0x0000FFFF, 0xFFFF0000]
FKEYS = [0.0, -0.0, 1.0, -1.0, 2.5, -2.5, 1e30, -1e30, INF, -INF, 1e-40, -1e-40, 3.4028234663852886e38, 0.5, 100.0]
NAN_BITS = [0x7FC00000, 0xFFC00000, 0x7F800001, 0xFFFFFFFF]


def key_values(r, n, floats):
    """n 32-bit key words in one of several patterns (floats: float bit patterns)."""
    pattern = r.choice(KEY_PATTERNS)

    def one():
        if floats:
            k = r.random()
            if k < 0.08:
                return r.choice(NAN_BITS)
            if k < 0.7:
                return fb(r.choice(FKEYS))
            return fb(T.f32(r.uniform(-50.0, 50.0)))
        k = r.random()
        if k < 0.35:
            return r.choice(UKEYS)
        if k < 0.65:
            return r.randrange(8)
        return r.getrandbits(32)

    if pattern == "equal":
        v = one()
        return [v] * n
    if pattern == "few":
        pool = [one() for _ in range(r.randint(1, 4))]
        return [r.choice(pool) for _ in range(n)]
    if pattern == "pairs":
        pool = [one() for _ in range(max(1, n // 2))]
        return [pool[i // 2 % len(pool)] for i in range(n)]
    values = [one() for _ in range(n)]
    if pattern == "ascending":
        values.sort(key=(lambda x: struct.unpack("<f", struct.pack("<I", x))[0]) if floats else None)
        values = [v for v in values]
    elif pattern == "descending":
        values.sort(key=(lambda x: struct.unpack("<f", struct.pack("<I", x))[0]) if floats else None, reverse=True)
    elif pattern == "extreme":
        values = [r.choice([0, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF] if not floats else [fb(INF), fb(-INF), 0x7FC00000, 0, 0x80000000]) for _ in range(n)]
    return values


def qs_case(r, op, n=None, per=None, floats=None, full=None):
    """A random CQSort case. op 1..8 as the table QS_OPS."""
    pointer_sort = op in (3, 4, 7, 8)
    n = r.choice([1, 2, 3, 4, 5, 8, 13, 21, 40]) if n is None else n
    per = r.randint(1, 6) if per is None else per
    koff = r.randrange(per)
    floats = op in (4, 8) if floats is None else floats
    keys = key_values(r, n, floats)
    words = []
    for i in range(n):
        rec = [r.getrandbits(32) for _ in range(per)]
        rec[koff] = keys[i]
        if per > 1:
            rec[(koff + 1) % per] = i          # a record number in the other words, so a record can be told apart
        words.append(rec)
    order = list(range(n))
    if pointer_sort:
        r.shuffle(order)
    if full or r.random() < 0.6:
        lo, hi = 0, n - 1
    else:
        lo = r.randrange(n)
        hi = r.randrange(lo, n)
    size = r.choice([4 * per, 4 * per, 0x5c, 0, 7, -1])
    if not pointer_sort:
        size = 4 * per
    return dict(op=op, words=words, per=per, koff=koff, order=order, lo=lo, hi=hi, size=size,
                tmp=[r.getrandbits(32) for _ in range(per)])


def qs_edges(op):
    def gen():
        r = random.Random(100 + op)
        floats = op in (4, 8)
        for n in (1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 33):
            for pattern in range(4):
                c = qs_case(r, op, n=n, full=True)
                if n and pattern == 1:
                    c["lo"], c["hi"] = n - 1, n - 1
                if n > 2 and pattern == 2:
                    c["lo"], c["hi"] = 1, n - 2
                if n > 1 and pattern == 3:
                    c["lo"], c["hi"] = 1, 0           # an empty range (low > high) inside the buffer
                yield c
        # one key position, sizes of one to six words, every key position
        for per in range(1, 7):
            for koff in range(per):
                c = qs_case(r, op, n=11, per=per, full=True)
                c["koff"] = koff
                for i, rec in enumerate(c["words"]):
                    rec[koff] = (i * 7919 + 3) % 5 if not floats else fb(float((i * 7919 + 3) % 5) - 2.0)
                yield c
        # sorted, reversed and constant runs of a longer list
        for pattern in ("ascending", "descending", "equal"):
            c = qs_case(r, op, n=40, full=True)
            keys = list(range(40)) if pattern == "ascending" else list(range(39, -1, -1)) if pattern == "descending" else [7] * 40
            for i, rec in enumerate(c["words"]):
                rec[c["koff"]] = keys[i] if not floats else fb(float(keys[i]))
            yield c
        # unsigned order: keys with the top bit set against keys without
        if not floats:
            c = qs_case(r, op, n=9, full=True)
            for i, rec in enumerate(c["words"]):
                rec[c["koff"]] = [0x80000000, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0, 0x80000001, 2, 0xFFFFFFFE, 0x40000000][i]
            yield c
        else:
            c = qs_case(r, op, n=10, full=True)
            for i, rec in enumerate(c["words"]):
                rec[c["koff"]] = fb([NAN, 1.0, -0.0, 0.0, INF, -INF, 2.0, NAN, -1.0, 1.0][i])
            yield c
    return gen


def qs_random(op):
    return lambda r: qs_case(r, op)


def qs_test(op):
    name, args = QS_OPS[op - 1]
    label = f"CQSort::{name}({'void*, int, int' if args == 3 else 'void*, void*, int, int, int, int'})"
    return FnTest(label, qs_ppc_run, qs_host_run, edge=qs_edges(op), rnd=qs_random(op))


def qs_ppc_run(e, case):
    st = CL.Stubs()
    st.real = set(QS.values())
    saved = st.activate(e)
    try:
        return qs_ppc(e, case)
    finally:
        CL.Stubs.deactivate(e, saved)


def qs_host_run(lib, case):
    return qs_host(lib, case)




# ================================================================= the cache's world
# Pointers in the description are numbers: an entry 1..N, a page 1..N (0: null), a spare place 1..4
# (0: the entry's own).

LINKS_MAX, ENTRIES_MAX, PLACES = 128, 4400, 4

# Entry flag bits.
ONPAGE, QUEUED, LOCKED, DROPPED = 0x1000, 0x2000, 0x4000, 0x8000
BLEND, ASKED = 0x80000, 0x40000000
WRAP_U, WRAP_V, WRAP = 0x10000, 0x20000, 0x30000
RESHUFFLE, LIMITED = 1, 4        # cache flags
NOT_LOCKED = 0xFFFF0FCF          # what a reset keeps


def blank_place(rnd=None):
    if rnd is None:
        return dict(bytes=[0] * 16, f10=0, f14=0)
    return dict(bytes=[rnd.choice([0, 1, 2, 0xfe, 0xff, rnd.randrange(256)]) for _ in range(16)],
                f10=rnd.choice([0, 1, -1, 17, 255, rnd.randint(-1000, 1000)]),
                f14=rnd.choice([0, 1, 0x8000, 0x7fff, -1, -2, -0x80000000, rnd.randint(-0x80000, 0x80000)]))


def blank_entry():
    return dict(flags=0, w=32, h=32, cw=32, ch=32, page=0, texture=0, usize=0.0, vsize=0.0, uoff=0.0, voff=0.0,
                prio=1.0, pitch=32, sortkey=0, next=0, wrap=0, place=0, own=blank_place())


def blank_link():
    return dict(entries=0, f08=0, f0c=0, f10=0, f14=-1, f18=0, f1c=0, tflags=0, tw=256, th=256, tpitch=1024,
                pixels=0, usage=0, first=[0, 0, 0], later=[0, 0, 0])


def blank_world(nl=0, ne=0):
    return dict(freq=0, qs=(1111, 2222), mip=(0, 0), comp=(0, 0),
                cache=dict(flags=0, f18e0=0, f18e4=0, f18b8=0, f18bc=0, f18c0=0, f18c4=0, head=0, f1818=[0] * 6,
                           limits=[0.0] * 5),
                rows=[[] for _ in range(6)], links=[blank_link() for _ in range(nl)], places=[blank_place() for _ in range(PLACES)],
                entries=[blank_entry() for _ in range(ne)], array=[], head=0)


def flat(w):
    """The description as the host library reads it (the order of tests/unit_TextureSort.cpp)."""
    c = w["cache"]
    d = [len(w["links"]), len(w["entries"]), len(w["array"])]
    d += [w["freq"], w["qs"][0], w["qs"][1], w["mip"][0], w["mip"][1], w["comp"][0], w["comp"][1]]
    d += [c["flags"], c["f18e0"], c["f18e4"], c["f18b8"], c["f18bc"], c["f18c0"], c["f18c4"], c["head"]]
    d += list(c["f1818"]) + [fb(x) for x in c["limits"]] + [len(row) for row in w["rows"]]
    for row in w["rows"]:
        d += row
    for link in w["links"]:
        d += [link[k] for k in ("entries", "f08", "f0c", "f10", "f14", "f18", "f1c", "tflags", "tw", "th", "tpitch", "pixels", "usage")]
        d += list(link["first"]) + list(link["later"])
    for place in w["places"]:
        d += list(place["bytes"]) + [place["f10"], place["f14"]]
    for en in w["entries"]:
        d += [en[k] for k in ("flags", "w", "h", "cw", "ch", "page", "texture")]
        d += [fb(en[k]) for k in ("usize", "vsize", "uoff", "voff", "prio")]
        d += [en["pitch"], en["sortkey"], en["next"], en["wrap"], en["place"]]
        d += list(en["own"]["bytes"]) + [en["own"]["f10"], en["own"]["f14"]]
    d += list(w["array"]) + [w["head"]]
    return d


def sections(values, w):
    """The state, as the two sides list it, cut into named parts."""
    pos = 0

    def take(n):
        nonlocal pos
        part = list(values[pos:pos + n])
        pos += n
        return part

    nl, ne, na = len(w["links"]), len(w["entries"]), len(w["array"])
    d = {"sorter": take(3), "cache": take(8 + 6 + 5 + 6)}
    d["rows"] = [take(len(row) + 2) for row in w["rows"]]
    d["links"] = [take(11) for _ in range(nl)]
    d["places"] = [take(18) for _ in range(PLACES)]
    d["entries"] = [take(34) for _ in range(ne)]
    d["array"] = take(na)
    d["head"] = take(1)
    d["out"] = take(ne + 3)
    assert pos == len(values), f"state of {len(values)} numbers, expected {pos}"
    return d


def hash_entries(numbers):
    h = 0
    for n in numbers:
        h = (h * 1000003 + n + 1) & ((1 << 60) - 1)
    return h


# ----------------------------------------------------------------- the emulator's fused multiply-adds and fctiwz

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]
# SortEntries (fmadds, fctiwz) and MergeListsConditional (fctiwz).
CODE_RANGES = [(FN["SortEntries"], FN["SortEntries"] + SIZES["SortEntries"]), (FN["Merge"], FN["Merge"] + SIZES["Merge"])]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the
    code under test and nowhere else (as in test_TextureFrame.py and test_NodeGamuts.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to
      single, which can differ by one unit in the last place from the processor's single
      rounding of the exact result; the result is replaced by the C library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor
      saturates to 0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just
    before the next one."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xffffffff00000000) | 0x7fffffff)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3ff == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


# ----------------------------------------------------------------- PowerPC side


class Ppc:
    """The world in emulator memory, the fakes, the call and the state."""

    def __init__(self, e, w):
        self.e, self.w = e, w
        self.nl, self.ne, self.na = len(w["links"]), len(w["entries"]), len(w["array"])
        self.calls = [0] * self.nl

    # -- numbering
    def eno(self, a):
        if a == 0:
            return 0
        off = a - self.entries
        if 0 <= off < self.ne * 0x5c and off % 0x5c == 0:
            return off // 0x5c + 1
        return -1

    def lno(self, a):
        if a == 0:
            return 0
        off = a - self.links
        if 0 <= off < self.nl * 0x20 and off % 0x20 == 0:
            return off // 0x20 + 1
        return -1

    def tno(self, a):
        if a == 0:
            return 0
        try:
            return self.textures.index(a) + 1
        except ValueError:
            return -1

    def entry_at(self, n):
        return self.entries + 0x5c * (n - 1) if n > 0 else 0

    def link_at(self, n):
        return self.links + 0x20 * (n - 1) if n > 0 else 0

    def build(self):
        e, w = self.e, self.w
        c = w["cache"]
        e.write(QSORT_OBJECT, struct.pack(">III", w["qs"][0] & MASK, w["qs"][1] & MASK, 0))
        w32(e, FREQUENCY, w["freq"])
        self.cache = e.alloc(0x1910, b"\xa5" * 0x1910)
        self.links = e.alloc(max(self.nl, 1) * 0x20, b"\xa5" * (max(self.nl, 1) * 0x20))
        self.places = e.alloc(PLACES * 0x18, b"\xa5" * (PLACES * 0x18))
        self.entries = e.alloc(max(self.ne, 1) * 0x5c, b"\xa5" * (max(self.ne, 1) * 0x5c))
        self.arr = e.alloc(max(self.na, 1) * 4 + 4, b"\xa5" * (max(self.na, 1) * 4 + 4))
        self.headvar = e.alloc(4, b"\xa5" * 4)
        self.out = e.alloc((self.ne + 8) * 4, struct.pack(">I", 1) * (self.ne + 8))

        cc = self.cache
        w32(e, cc + 0x18b4, c["flags"])
        for off, key in ((0x18e0, "f18e0"), (0x18e4, "f18e4"), (0x18b8, "f18b8"), (0x18bc, "f18bc"), (0x18c0, "f18c0"),
                         (0x18c4, "f18c4")):
            w32(e, cc + off, c[key])
        w32(e, cc + 0x18ac, self.entry_at(c["head"]))
        for k, v in enumerate(c["f1818"]):
            w32(e, cc + 0x1818 + 4 * k, v)
        for k, v in enumerate(c["limits"]):
            w32(e, cc + 0x18ec + 4 * k, fb(v))
        for i, row in enumerate(w["rows"]):
            w32(e, cc + 0x1800 + 4 * i, len(row))
            e.write(cc + 0x400 * i, b"".join(struct.pack(">I", self.link_at(n)) for n in row) + b"\x00" * 8)

        self.textures, self.bases = [], []
        for i, link in enumerate(w["links"]):
            obj = e.alloc(0x34, b"\xa5" * 0x34)
            T.call_ppc(e, 0x10045d88, [obj, 1])            # the original's constructor makes the vtables
            base = obj + 0x1c
            self.textures.append(obj)
            self.bases.append(base)
            e.write(base, struct.pack(">IIII", link["tflags"] & MASK, link["tw"] & MASK, link["th"] & MASK, link["tpitch"] & MASK))
            e.write(self.links + 0x20 * i, struct.pack(">IIIIIIII", base, self.entry_at(link["entries"]), link["f08"] & MASK,
                                                       link["f0c"] & MASK, link["f10"] & MASK, link["f14"] & MASK,
                                                       link["f18"] & MASK, link["f1c"] & MASK))
        for k, place in enumerate(w["places"]):
            self.write_place(self.places + 0x18 * k, place)
        for i, en in enumerate(w["entries"]):
            a = self.entries + 0x5c * i
            place = self.places + 0x18 * (en["place"] - 1) if en["place"] > 0 else a + 0x44
            texture = self.textures[en["texture"] - 1] if en["texture"] > 0 else 0
            own = en["own"]
            e.write(a, struct.pack(">IIIhhhhIIIIIIIhxxIIII16sII", place, self.entry_at(en["next"]), en["flags"] & MASK,
                                   T.s16(en["w"]), T.s16(en["h"]), T.s16(en["cw"]), T.s16(en["ch"]), self.link_at(en["page"]),
                                   texture, fb(en["usize"]), fb(en["vsize"]), fb(en["uoff"]), fb(en["voff"]), fb(en["prio"]),
                                   T.s16(en["pitch"]), en["sortkey"] & MASK, 0, 0, 0, bytes(own["bytes"]), own["f10"] & MASK,
                                   own["f14"] & MASK))
        e.write(self.arr, b"".join(struct.pack(">I", self.entry_at(n)) for n in w["array"]))
        w32(e, self.headvar, self.entry_at(w["head"]))
        self.wrap = [en["wrap"] for en in w["entries"]]

    def write_place(self, a, p):
        self.e.write(a, bytes(p["bytes"]) + struct.pack(">II", p["f10"] & MASK, p["f14"] & MASK))

    # -- the fakes
    def stubs(self):
        st = CL.Stubs()
        st.real = set(FN.values()) | set(QS.values()) | set(INNER)
        st.internal(FLUSH, self.flush)
        st.internal(STORE_WRAP, self.store_wrap)
        st.internal(CALC_USAGE, self.calc_usage)
        st.internal(AUTO_MIPMAP, lambda e: self.cache_step(e, "AutoMipmap", self.w["mip"]))
        st.internal(COMPRESS, lambda e: self.cache_step(e, "Entry_Compress", self.w["comp"]))
        st.internal(GET_PIXELS, self.get_pixels)
        st.imp("__assertion_failed", lambda e: e.cl.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def flush(self, e):
        this = e.r(3)
        e.cl.rec("FlushEntryFlags", (self.eno(this), e.u32(this + 8)))

    def store_wrap(self, e):
        n = self.eno(e.r(3))
        e.cl.rec("StoreAsWrapping", (n,))
        e.set_r(3, self.wrap[n - 1] if n > 0 else 0)

    def calc_usage(self, e):
        link, flags = e.r(3), T.s32(e.r(4))
        n = self.lno(link)
        e.cl.rec("CalcUsage", (n, flags))
        fake = self.w["links"][n - 1]
        if fake["usage"] != 0:
            triple = fake["first"] if self.calls[n - 1] == 0 else fake["later"]
            e.write(link + 8, struct.pack(">III", *[v & MASK for v in triple]))
        self.calls[n - 1] += 1

    def cache_step(self, e, name, effect):
        this, entries, count = e.r(3), e.r(4), T.s32(e.r(5))
        numbers = [self.eno(e.u32(entries + 4 * i)) for i in range(count)]
        e.cl.rec(name, (count, hash_entries(numbers), numbers[0] if numbers else 0, T.s32(e.u32(this + 0x18c4)),
                        T.s32(e.u32(this + 0x18e0)), e.u32(this + 0x18b4)))
        if effect[0] != 0:
            w32(e, this + 0x18c4, effect[1])

    def get_pixels(self, e):
        n = self.tno(e.r(3))
        e.cl.rec("GetPixelCount", (n,))
        e.set_r(3, T.u32(self.w["links"][n - 1]["pixels"]) if n > 0 else 0)

    # -- the call and the state
    def call(self, op, a):
        e, c = self.e, self.cache
        if op in (1, 2):
            T.call_ppc(e, FN["QSort" if op == 1 else "QSortFree"], [c, c + 0x400 * a[0], a[1] & MASK, a[2] & MASK])
        elif op == 3:
            T.call_ppc(e, FN["SortTextures"], [c])
        elif op == 4:
            T.call_ppc(e, FN["EntryQSortInit"], [c, self.arr, a[0] & MASK, a[1] & MASK])
        elif op == 5:
            T.call_ppc(e, FN["SortEntries"], [c])
        elif op == 6:
            T.call_ppc(e, FN["SortByCriteria"], [c, self.headvar])
        elif op == 7:
            T.call_ppc(e, FN["SortByAddress"], [c, self.headvar])
        elif op == 8:
            T.call_ppc(e, FN["Merge"], [c, self.headvar, self.entry_at(a[0])])
        else:
            return T.s32(T.call_ppc(e, FN["CreateNW"], [c, self.out, self.entry_at(a[0]), self.link_at(a[1])])[0])
        return 0

    def state(self):
        e, u, s = self.e, self.e.u32, T.s32
        c = self.cache
        sz, ko, t = struct.unpack(">iiI", e.read(QSORT_OBJECT, 12))
        out = [sz, ko, 0 if t == 0 else self.eno(t)]
        out += [self.eno(u(c + 0x18ac)), u(c + 0x18b4)] + [s(u(c + off)) for off in (0x18e0, 0x18e4, 0x18b8, 0x18bc, 0x18c0, 0x18c4)]
        out += [s(u(c + 0x1818 + 4 * k)) for k in range(6)]
        out += [canon(u(c + 0x18ec + 4 * k)) for k in range(5)]
        counts = [s(u(c + 0x1800 + 4 * k)) for k in range(6)]
        out += counts
        for i in range(6):
            out += [self.lno(u(c + 0x400 * i + 4 * k)) for k in range(counts[i] + 2)]
        for i in range(self.nl):
            a = self.links + 0x20 * i
            out.append(self.eno(u(a + 4)))
            out += [s(u(a + off)) for off in (8, 0xc, 0x10, 0x14, 0x18, 0x1c)]
            base = self.bases[i]
            out += [u(base), s(u(base + 4)), s(u(base + 8)), s(u(base + 0xc))]
        for k in range(PLACES):
            out += self.place(self.places + 0x18 * k)
        if self.ne:
            block = e.read(self.entries, self.ne * 0x5c)
            for i in range(self.ne):
                a = self.entries + 0x5c * i
                (place, nxt, flags, w_, h_, cw, ch, page, texture, usize, vsize, uoff, voff, prio, pitch, sortkey, _f38, _data,
                 _pad, own, ownf10, ownf14) = struct.unpack_from(">IIIhhhhIIIIIIIhxxIIII16sII", block, 0x5c * i)
                if place == a + 0x44:
                    code = 0
                elif self.places <= place < self.places + 0x18 * PLACES and (place - self.places) % 0x18 == 0:
                    code = (place - self.places) // 0x18 + 1
                else:
                    code = -1
                out += [code, self.eno(nxt), flags, w_, h_, cw, ch, self.lno(page), self.tno(texture), canon(usize), canon(vsize),
                        canon(uoff), canon(voff), canon(prio), pitch, s(sortkey)]
                out += list(own) + [s(ownf10), s(ownf14)]
        out += [self.eno(u(self.arr + 4 * i)) for i in range(self.na)]
        out.append(self.eno(u(self.headvar)))
        out += [self.eno(u(self.out + 4 * i)) for i in range(self.ne + 3)]
        return out

    def place(self, a):
        return list(self.e.read(a, 16)) + [T.s32(self.e.u32(a + 0x10)), T.s32(self.e.u32(a + 0x14))]


def ppc_run(e, case):
    w = case["world"]
    p = Ppc(e, w)
    st = p.stubs()
    saved = st.activate(e)
    try:
        p.build()
        e.cl = CL.PpcLog({}, {})
        with HardwareRounding(e):
            result = p.call(case["op"], case["args"])
        return dict(result=result, state=sections(p.state(), w), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, case):
    w = case["world"]
    CL.host_begin(lib, {}, {})
    description = flat(w)
    d = (C.c_int64 * len(description))(*[signed64(v) for v in description])
    args = (C.c_int64 * 4)(*[signed64(v) for v in (list(case["args"]) + [0, 0, 0, 0])[:4]])
    capacity = 4096 + 40 * len(w["entries"]) + 40 * len(w["links"])
    out = (C.c_int64 * capacity)()
    run = lib.tpw_ts_run
    run.restype = C.c_int64
    run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
    result = run(case["op"], args, d, out, capacity)
    lib.tpw_ts_count.restype = C.c_int32
    n = lib.tpw_ts_count()
    log = CL.host_end(lib)
    assert n <= capacity, "the state buffer is too small"
    return dict(result=result, state=sections(list(out[:n]), w), log=log)


# ----------------------------------------------------------------- random worlds


def rfl(r):
    """A float for a field: mostly moderate, some edge values."""
    k = r.random()
    if k < 0.2:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.0, 4.0, 0.25, 1024.0, 1e-3, 100.0, -100.0, 3.0e9, -3.0e9,
                         1.0e-40, NAN, INF, -INF, 3.4028234663852886e38])
    if k < 0.6:
        return T.f32(r.uniform(-1.0, 3.0))
    return T.f32(r.uniform(-2000.0, 2000.0))


def rsize(r):
    return r.choice([8, 16, 32, 64, 128, 256, r.randint(1, 300), r.randint(1, 40), 1, 7, 9, 0, -3, 0x7fff, -0x8000,
                     r.randint(-0x8000, 0x7fff)])


def rkey(r):
    """A sort key (32 bits): few values (ties), the extremes, or anything."""
    k = r.random()
    if k < 0.35:
        return r.randrange(6)
    if k < 0.55:
        return r.choice([0, 1, 0x7fffffff, 0x80000000, 0x80000001, 0xffffffff, 0x20000000, 0x40000000, 0x0fff0000])
    return r.getrandbits(32)


def rflags(r, asked=0.5):
    """An entry's flags: random bits with the ones the functions test set on purpose."""
    keep = r.getrandbits(32) & r.getrandbits(32) & ~(ONPAGE | QUEUED | LOCKED | DROPPED | BLEND | ASKED | WRAP | 0xf0)
    for bit, p in ((ASKED, asked), (LOCKED, 0.3), (QUEUED, 0.3), (DROPPED, 0.15), (ONPAGE, 0.3), (BLEND, 0.25), (WRAP_U, 0.15),
                   (WRAP_V, 0.15), (0x10, 0.3), (0x20, 0.3), (0x40, 0.3), (0x80, 0.3)):
        if r.random() < p:
            keep |= bit
    return keep & MASK


def random_entry(r, nl, asked=0.5):
    width, height = rsize(r), rsize(r)
    en = blank_entry()
    en.update(flags=rflags(r, asked), w=width, h=height, cw=r.choice([width, (width + 7) & ~7, rsize(r), 8, 16, 32]),
              ch=r.choice([height, (height + 7) & ~7, rsize(r), 8, 16, 32]), page=r.randint(0, nl) if nl else 0,
              texture=r.randint(0, nl) if nl else 0, usize=rfl(r), vsize=rfl(r), uoff=rfl(r), voff=rfl(r), prio=rfl(r),
              pitch=r.choice([0, 32, r.randint(-0x8000, 0x7fff)]), sortkey=T.s32(rkey(r)),
              wrap=r.randrange(2), place=r.choice([0, 0, 0, 1, 2, 3, 4]), own=blank_place(r))
    return en


def random_world(r, ne, nl=0):
    w = blank_world(nl=nl, ne=ne)
    w["entries"] = [random_entry(r, nl) for _ in range(ne)]
    w["freq"] = r.randrange(16)
    w["places"] = [blank_place(r) for _ in range(PLACES)]
    w["qs"] = (r.choice([1111, 5000, 77]), r.choice([2222, 6000, 99]))
    return w


def set_chain(w, order):
    """Chains the entries (numbers) through mNext; entries not in `order` point at nothing or
    into it (a stale pointer). Returns the head."""
    for k, n in enumerate(order):
        w["entries"][n - 1]["next"] = order[k + 1] if k + 1 < len(order) else 0
    return order[0] if order else 0


def stale_pointers(r, w, order):
    for n in range(1, len(w["entries"]) + 1):
        if n not in order:
            w["entries"][n - 1]["next"] = r.choice([0] + list(order))


def case(op, args, w):
    return dict(op=op, args=list(args), world=w)


def pick(r, ne, k=None):
    k = r.randint(0, ne) if k is None else k
    return r.sample(range(1, ne + 1), k)


# Operations.
QSORT, QSORT_FREE, SORT_TEXTURES, ENTRY_QSORT, SORT_ENTRIES, BY_CRITERIA, BY_ADDRESS, MERGE, CREATE_NW = range(1, 10)

LENGTHS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17, 18, 24, 25]


def ikeys(r, n, signed=True):
    """n 32-bit keys in one of several patterns (ties, runs, the extremes)."""
    pattern = r.choice(["random", "few", "ascending", "descending", "equal", "extreme", "pairs"])

    def one():
        k = r.random()
        if k < 0.35:
            return r.randrange(8) - (4 if signed else 0)
        if k < 0.6:
            return r.choice([0, 1, -1, 0x7fffffff, -0x80000000, 0x7ffffffe, -0x7fffffff, 0x10000, -0x10000])
        return r.randint(-0x80000000, 0x7fffffff)

    if pattern == "equal":
        return [one()] * n
    if pattern == "few":
        pool = [one() for _ in range(r.randint(1, 4))]
        return [r.choice(pool) for _ in range(n)]
    if pattern == "pairs":
        pool = [one() for _ in range(max(1, n // 2))]
        return [pool[i // 2 % len(pool)] for i in range(n)]
    values = [one() for _ in range(n)]
    if pattern == "ascending":
        values.sort()
    elif pattern == "descending":
        values.sort(reverse=True)
    elif pattern == "extreme":
        values = [r.choice([0x7fffffff, -0x80000000, 0, -1, 1]) for _ in range(n)]
    return values


def rnd_qsort(free):
    def gen(r):
        n = r.randint(1, 30) if r.random() < 0.92 else r.randint(31, 100)
        w = blank_world(nl=n)
        row = r.randrange(6)
        w["rows"][row] = r.sample(range(1, n + 1), n)
        keys = ikeys(r, n)
        for i, link in enumerate(w["links"]):
            link["f10" if free else "f0c"] = keys[i]
            link["f0c" if free else "f10"] = r.randint(-5, 5)
            link["f08"] = r.randint(-9, 9)
        if r.random() < 0.6:
            lo, hi = 0, n - 1
        else:
            lo = r.randrange(n)
            hi = r.randrange(lo, n)
        return case(QSORT_FREE if free else QSORT, [row, lo, hi], w)
    return gen


def rnd_sort_textures(r):
    nl = r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 14, 20, 30]) if r.random() < 0.95 else r.randint(31, 90)
    w = blank_world(nl=nl)
    flags = r.choice([0, 0, 0, 1, 1])
    if r.random() < 0.3:
        flags |= r.getrandbits(32) & ~1
    w["cache"].update(flags=flags, f18b8=r.randint(-99, 99), f18bc=r.randint(-99, 99), f18c0=r.randint(-99, 99),
                      f1818=[r.randint(-9, 9) for _ in range(6)])
    # Each page goes into a row, rows in any order.
    ids = list(range(1, nl + 1))
    r.shuffle(ids)
    focus = r.choice([None, None, r.randrange(6)])
    for n in ids:
        w["rows"][focus if focus is not None and r.random() < 0.8 else r.randrange(6)].append(n)
    for link in w["links"]:
        used = r.choice([r.randint(0, 20), r.randint(0, 2000), 0, 1])
        link.update(tflags=r.choice([0, 0, 1, 1, 2, 3, r.getrandbits(32)]) & MASK, f08=r.randint(-99, 99), f0c=used,
                    f10=r.choice([r.randint(-20, 20), r.randint(-0x80000000, 0x7fffffff)]), f14=r.randint(-9, 9),
                    f18=r.randint(-9, 9), f1c=r.randint(-9, 9), tw=r.choice([8, 256]), th=r.choice([8, 256]),
                    tpitch=r.randint(0, 99))
        if r.random() < 0.5:
            link["usage"] = 1
            link["first"] = [r.randint(0, 3000), r.choice([r.randint(0, 20), r.randint(0, 2000)]), r.choice([r.randint(-20, 20), r.randint(-0x80000000, 0x7fffffff)])]
            link["later"] = [r.randint(-99, 3000), r.randint(-99, 2000), r.randint(-20, 20)]
        shown = link["first"][1] if link["usage"] else used
        link["pixels"] = r.choice([2 * shown - 1, 2 * shown, 2 * shown + 1, 2 * shown, r.randint(0, 4000), 0, -1, 0x7fffffff])
    return case(SORT_TEXTURES, [], w)


def rnd_entry_qsort(r):
    ne = r.choice(LENGTHS) + r.randint(0, 3)
    w = random_world(r, ne)
    order = pick(r, ne, min(ne, r.choice(LENGTHS)))
    keys = ikeys(r, ne, signed=False)
    for i, en in enumerate(w["entries"]):
        en["sortkey"] = keys[i]
    w["array"] = order
    if len(order) >= 2 and r.random() < 0.65:
        lo, hi = 0, len(order) - 1
    elif len(order) >= 2:
        lo = r.randrange(len(order))
        hi = r.randrange(lo, len(order))
    else:
        lo, hi = 0, len(order) - 1
    return case(ENTRY_QSORT, [lo, hi], w)


def pyfctiwz(x):
    if x != x:
        return -0x80000000
    if x >= 2147483648.0:
        return 0x7fffffff
    if x <= -2147483649.0:
        return -0x80000000
    return int(x)


def area_of(w, order):
    total = 0
    for n in order:
        en = w["entries"][n - 1]
        total += T.s16(en["w"]) * T.s16(en["h"])
    return T.s32(total)


def wanted_of(area):
    return pyfctiwz(T.f32(1.5 * T.f32(float(area))))


def tune_cache(r, w, order):
    """Cache settings for SortEntries that put the area tests near their boundaries."""
    c = w["cache"]
    area = area_of(w, order)
    if r.random() < 0.4:
        w["mip"] = (1, r.choice([area, area + 1, area - 1, 0, r.randint(0, 5000), -1, 0x7fffffff, -0x80000000]))
    shown = w["mip"][1] if w["mip"][0] else area
    wanted = wanted_of(shown)
    available = r.choice([wanted - 1, wanted, wanted + 1, wanted, 0, -1, r.randint(-5000, 5000), 0x7fffffff, -0x80000000, wanted + 0x40000000])
    c["f18bc"] = r.randint(-1000, 1000)
    c["f18c0"] = T.s32(available + c["f18bc"])
    c["flags"] = r.choice([0, LIMITED, LIMITED, RESHUFFLE, LIMITED | RESHUFFLE]) | (r.getrandbits(32) & ~(LIMITED | RESHUFFLE) if r.random() < 0.2 else 0)
    c["f18e4"] = r.choice([shown - 1, shown, shown + 1, shown // 2, 0, -1, r.randint(-100, 5000), 0x7fffffff, -0x80000000])
    c["f18e4"] = T.s32(c["f18e4"])
    c["f18e0"] = r.randint(0, 12)
    if r.random() < 0.5:
        w["comp"] = (1, r.choice([shown, shown // 2, shown // 3, c["f18e4"], c["f18e4"] + 1, c["f18e4"] - 1, 0, r.randint(0, 5000), 0x7fffffff, -1]))
        w["comp"] = (1, T.s32(w["comp"][1]))
    c["f18c4"] = r.randint(-99, 99)
    c["limits"] = [rfl(r) for _ in range(5)]
    c["f1818"] = [r.randint(-9, 9) for _ in range(6)]


def rnd_sort_entries(r):
    ne = r.choice(LENGTHS) + r.randint(0, 3)
    w = random_world(r, ne)
    for en in w["entries"]:
        en["prio"] = T.f32(r.choice([0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 0.25, -1.0, T.f32(r.uniform(-1.0, 3.0))])) if r.random() < 0.7 else rfl(r)
        en["w"], en["h"] = (r.choice([8, 16, 32, 64, 128, 256, 7, 100]), r.choice([8, 16, 32, 64, 128, 256, 9, 100])) if r.random() < 0.8 else (rsize(r), rsize(r))
        en["flags"] &= ~DROPPED if r.random() < 0.7 else MASK
    order = pick(r, ne, min(ne, r.choice(LENGTHS)))
    w["cache"]["head"] = set_chain(w, order)
    stale_pointers(r, w, order)
    tune_cache(r, w, order)
    return case(SORT_ENTRIES, [], w)


def rnd_by_criteria(r):
    ne = r.choice(LENGTHS) + r.randint(0, 3)
    w = random_world(r, ne)
    order = pick(r, ne, min(ne, r.choice(LENGTHS)))
    w["head"] = set_chain(w, order)
    stale_pointers(r, w, order)
    w["cache"]["head"] = r.choice([0, 1]) if ne else 0       # the queue is not the list
    return case(BY_CRITERIA, [], w)


def rnd_by_address(r):
    c = rnd_by_criteria(r)
    c["op"] = BY_ADDRESS
    return c


def rnd_merge(r):
    ne = r.randint(0, 16)
    w = random_world(r, ne, nl=r.choice([0, 2, 3]))
    order = pick(r, ne)
    listed = order[:r.randint(0, min(len(order), 6))]
    fresh = order[len(listed):len(listed) + r.randint(0, 8)]
    if r.random() < 0.8:
        listed.sort(key=lambda n: -w["entries"][n - 1]["sortkey"])
    w["head"] = set_chain(w, listed)
    head2 = set_chain(w, fresh)
    stale_pointers(r, w, listed + fresh)
    w["cache"]["flags"] = r.choice([0, 0, 1, r.getrandbits(32)]) & MASK
    for n in fresh:
        w["entries"][n - 1]["flags"] = rflags(r, 0.6)
    return case(MERGE, [head2], w)


def rnd_create_nw(r):
    ne = r.randint(0, 16)
    nl = r.choice([1, 1, 2])
    w = random_world(r, ne, nl=nl)
    order = pick(r, ne)
    on_page = order[:r.randint(0, min(len(order), 7))]
    others = order[len(on_page):len(order)]
    others = others[:r.randint(0, 8)]
    for n in order:
        en = w["entries"][n - 1]
        en["w"], en["h"] = r.randint(1, 4), r.randint(1, 4)
    if r.random() < 0.7:
        key = lambda n: (-w["entries"][n - 1]["h"], -w["entries"][n - 1]["w"])
        on_page.sort(key=key)
        others.sort(key=key)
    link = r.randrange(nl)
    w["links"][link]["entries"] = set_chain(w, on_page)
    head_b = set_chain(w, others)
    stale_pointers(r, w, on_page + others)
    for n in on_page:
        w["entries"][n - 1]["flags"] = rflags(r, 0.6)
    return case(CREATE_NW, [head_b, link + 1], w)



# ----------------------------------------------------------------- edge cases


def with_entry(w, n, **kw):
    w["entries"][n - 1].update(kw)
    return w


def plain_world(ne, nl=0):
    """Entries that are all 32 x 32, nothing flagged, priorities 1.0, in one array."""
    return blank_world(nl=nl, ne=ne)


def edge_qsort(free):
    def gen():
        r = random.Random(7 + free)
        field = "f10" if free else "f0c"
        for row in range(6):
            for keys in ([5], [5, 3], [3, 5], [4, 4], [3, 2, 1], [1, 2, 3], [2, 1, 3], [2, 3, 1], [3, 1, 2], [1, 3, 2], [7, 7, 7],
                         [9, 1, 9, 1], list(range(12)), list(range(12, 0, -1)), [3] * 12, [0x7fffffff, -0x80000000, 0, -1, 1, 0x7fffffff],
                         [-0x80000000, 0x7fffffff, -0x80000000], [1, -1, 1, -1, 1, -1, 1, -1]):
                if row not in (0, 3, 5) and len(keys) > 5:
                    continue
                n = len(keys)
                w = blank_world(nl=n)
                w["rows"][row] = list(range(1, n + 1))
                for i, link in enumerate(w["links"]):
                    link[field] = keys[i]
                    link["f10" if not free else "f0c"] = 100 - i
                yield case(QSORT_FREE if free else QSORT, [row, 0, n - 1], w)
                if n > 3:
                    yield case(QSORT_FREE if free else QSORT, [row, 1, n - 2], w)
                    yield case(QSORT_FREE if free else QSORT, [row, n - 1, n - 1], w)
                    yield case(QSORT_FREE if free else QSORT, [row, 1, 2], w)
        # a longer list in an order that needs several rounds
        for _ in range(6):
            n = 60
            w = blank_world(nl=n)
            w["rows"][2] = r.sample(range(1, n + 1), n)
            for link in w["links"]:
                link[field] = r.randint(-30, 30)
            yield case(QSORT_FREE if free else QSORT, [2, 0, n - 1], w)
    return gen


def textures_world(rows, flags=0, locked=(), used=None, pixels=None):
    """rows: lists of page numbers per size. used[n]/pixels[n]: mF0C and the pixel count of page n."""
    n = max([max(row) for row in rows if row] + [0])
    w = blank_world(nl=n)
    w["cache"]["flags"] = flags
    for i, row in enumerate(rows):
        w["rows"][i] = list(row)
    for k, link in enumerate(w["links"], start=1):
        link["tflags"] = 1 if k in locked else 0
        link["f0c"] = (used or {}).get(k, 10 * (n - k))
        link["f10"] = 7 * k % 5
        link["pixels"] = (pixels or {}).get(k, 100)
    return w


def edge_sort_textures():
    r = random.Random(33)
    # Nothing at all, and rows of one page, with either cache flag.
    for flags in (0, 1, 5):
        yield case(SORT_TEXTURES, [], blank_world())
        for row in range(6):
            rows = [[] for _ in range(6)]
            rows[row] = [1]
            for lock in (0, 1):
                yield case(SORT_TEXTURES, [], textures_world(rows, flags, locked=(1,) if lock else ()))
    # Every lock pattern of two to five pages in one row.
    for flags in (0, 1):
        for n in (2, 3, 4, 5):
            for bits in range(1 << n):
                locked = tuple(k + 1 for k in range(n) if (bits >> k) & 1)
                rows = [[], [], list(range(1, n + 1)), [], [], []]
                yield case(SORT_TEXTURES, [], textures_world(rows, flags, locked=locked))
                rows = [[], [], list(range(n, 0, -1)), [], [], []]
                yield case(SORT_TEXTURES, [], textures_world(rows, flags, locked=locked, used={k: k for k in range(1, n + 1)}))
    # Half full: pages over half to the end, none, all, and the boundary.
    for n in (3, 4):
        for fulls in range(1 << n):
            rows = [list(range(1, n + 1)), [], [], [], [], []]
            used = {k: k for k in range(1, n + 1)}
            pixels = {k: (2 * k - 1 if (fulls >> (k - 1)) & 1 else 2 * k + 1) for k in range(1, n + 1)}
            yield case(SORT_TEXTURES, [], textures_world(rows, 1, used=used, pixels=pixels))
            pixels = {k: 2 * k for k in range(1, n + 1)}
            yield case(SORT_TEXTURES, [], textures_world(rows, 1, used=used, pixels=pixels))
    # All six rows with pages, the sums over every one, locked ones left out.
    for flags in (0, 1):
        rows = [[1, 2], [3], [4, 5, 6], [7], [8, 9], [10, 11, 12]]
        w = textures_world(rows, flags, locked=(2, 6, 9, 12))
        for k, link in enumerate(w["links"], start=1):
            link.update(f08=1000 * k, f0c=r.randint(0, 50), pixels=r.randint(0, 120))
        yield case(SORT_TEXTURES, [], w)
    # CalcUsage assigning the counters (the first triple on the first call, the second after).
    for flags in (0, 1):
        rows = [[], [1, 2, 3, 4], [], [5, 6], [], []]
        w = textures_world(rows, flags, locked=(3,))
        for k, link in enumerate(w["links"], start=1):
            link.update(usage=1, first=[100 + k, 50 - 7 * k, 3 * k], later=[200 + k, 1000 - k, -k], pixels=200)
        yield case(SORT_TEXTURES, [], w)
    # Long rows (the sort recurses), a negative or wrapping used count.
    for flags in (0, 1):
        n = 40
        w = textures_world([[], [], [], list(range(1, n + 1)), [], []], flags, locked=tuple(range(3, n, 7)))
        for k, link in enumerate(w["links"], start=1):
            link.update(f0c=r.randint(-30, 30), f10=r.randint(-30, 30), pixels=r.randint(-10, 80))
        yield case(SORT_TEXTURES, [], w)
    w = textures_world([[1, 2, 3], [], [], [], [], []], 1, used={1: 0x40000000, 2: -0x40000000, 3: 0x7fffffff}, pixels={1: -0x7fffffff, 2: 5, 3: 0x7fffffff})
    yield case(SORT_TEXTURES, [], w)


def edge_entry_qsort():
    r = random.Random(41)
    for n in (0, 1, 2, 3, 4, 5, 8, 9, 13):
        for keys_kind in ("ascending", "descending", "equal", "high", "random"):
            w = plain_world(max(n, 1))
            keys = {"ascending": list(range(n)), "descending": list(range(n, 0, -1)), "equal": [7] * n,
                    "high": [(0x80000000 + 3 * i) % (1 << 32) if i % 2 else i for i in range(n)],
                    "random": [r.randrange(5) for _ in range(n)]}[keys_kind]
            for i in range(n):
                w["entries"][i]["sortkey"] = T.s32(keys[i])
            w["array"] = list(range(1, n + 1))
            if keys_kind == "random":
                r.shuffle(w["array"])
            yield case(ENTRY_QSORT, [0, n - 1], w)
            if n > 3:
                yield case(ENTRY_QSORT, [1, n - 2], w)
                yield case(ENTRY_QSORT, [2, 2], w)
                yield case(ENTRY_QSORT, [3, 1], w)
                yield case(ENTRY_QSORT, [n - 1, 0], w)


def entry_list_world(r, n, kind, nl=0):
    """n entries chained in an order (the kind names it) with sort keys of a pattern."""
    w = random_world(r, n, nl=nl)
    for en in w["entries"]:
        en["flags"] &= ~DROPPED & MASK
    keys = {"sorted": list(range(n)), "reverse": list(range(n, 0, -1)), "equal": [9] * n,
            "ties": [(i * 3) % 4 for i in range(n)], "random": [r.randrange(1000) for _ in range(n)],
            "high": [0x80000000 if i % 2 else i for i in range(n)]}[kind]
    for i, en in enumerate(w["entries"]):
        en["sortkey"] = T.s32(keys[i])
    order = list(range(1, n + 1))
    if kind in ("random", "ties", "high") or r.random() < 0.3:
        r.shuffle(order)
    return w, order


def edge_by_list(op):
    def gen():
        r = random.Random(50 + op)
        for n in (0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 15, 16, 17, 24, 25, 33):
            for kind in ("sorted", "reverse", "equal", "ties", "random", "high"):
                w, order = entry_list_world(r, n, kind)
                if op == BY_ADDRESS:
                    order = sorted(order) if kind in ("sorted", "equal") else sorted(order, reverse=True) if kind == "reverse" else order
                w["head"] = set_chain(w, order)
                yield case(op, [], w)
    return gen


def big_list_world(r, n):
    w = random_world(r, n)
    for en in w["entries"]:
        en["flags"] &= ~DROPPED & MASK
        en["own"] = blank_place()
    return w


def edge_big_lists(op):
    r = random.Random(60 + op)
    for n in (4095, 4096, 4097, 4100, 4300):
        w = big_list_world(r, n)
        order = list(range(1, n + 1))
        r.shuffle(order)
        w["head"] = set_chain(w, order)
        yield case(op, [], w)
    # Five thousand entries are more than a case holds; the limit is exact at 4096.
    w = big_list_world(r, 4097)
    w["head"] = set_chain(w, list(range(4097, 0, -1)))
    yield case(op, [], w)


def sort_entries_world(r, n, **kw):
    w = random_world(r, n)
    for en in w["entries"]:
        en["flags"] &= ~DROPPED & MASK
        en["prio"] = T.f32(r.choice([0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 0.25, -1.0, T.f32(r.uniform(-1.0, 3.0))]))
        en["w"], en["h"] = r.choice([8, 16, 32, 64, 128, 256, 7, 100]), r.choice([8, 16, 32, 64, 128, 256, 9, 100])
        en["own"] = blank_place()
    order = list(range(1, n + 1))
    r.shuffle(order)
    w["cache"]["head"] = set_chain(w, order)
    return w, order


def edge_sort_entries():
    r = random.Random(77)
    # An empty queue, with the compress tests off and on (a count of 0 gives the loop no entry).
    for flags in (0, LIMITED):
        for available in (0, 5, -5):
            for e4 in (0, 100, -3):
                w = blank_world()
                w["cache"].update(flags=flags, f18c0=available, f18bc=0, f18e4=e4)
                w["mip"] = (1, 50)
                yield case(SORT_ENTRIES, [], w)
    # Lengths up to 25 (the original sums eight at a time) with the area tests either way.
    for n in range(1, 26):
        for mode in range(5):
            w, order = sort_entries_world(r, n)
            c = w["cache"]
            area = area_of(w, order)
            wanted = wanted_of(area)
            c["limits"] = [0.5] * 5
            c["f18e0"] = n
            if mode == 0:        # fits, no limit: nothing runs
                c["f18c0"], c["f18bc"], c["flags"], c["f18e4"] = wanted + 100, 0, 0, 0
            elif mode == 1:      # too big: compress
                c["f18c0"], c["f18bc"], c["flags"], c["f18e4"] = wanted - 1, 0, 0, 0
            elif mode == 2:      # fits, limit under the area: compress and drop from the end
                c["f18c0"], c["f18bc"], c["flags"], c["f18e4"] = wanted + 100, 0, LIMITED, area // 2
            elif mode == 3:      # limit equal to the area
                c["f18c0"], c["f18bc"], c["flags"], c["f18e4"] = wanted + 100, 0, LIMITED, area
            else:                # too big and a limit that needs every entry gone
                c["f18c0"], c["f18bc"], c["flags"], c["f18e4"] = 0, 0, LIMITED, -1
            yield case(SORT_ENTRIES, [], w)
    # The boundary of the area test: exactly the wanted, one less and one more (signed).
    for delta in (-2, -1, 0, 1, 2):
        w, order = sort_entries_world(r, 6)
        area = area_of(w, order)
        c = w["cache"]
        c["f18bc"], c["f18c0"] = 1000, T.s32(wanted_of(area) + delta + 1000)
        yield case(SORT_ENTRIES, [], w)
    # Differences that wrap, and a wanted amount that saturates.
    for c0, bc in ((0x7fffffff, -5), (-0x80000000, 5), (0x7fffffff, 0x7fffffff), (-0x80000000, -0x80000000), (5, 0x7fffffff)):
        w, order = sort_entries_world(r, 4)
        w["cache"]["f18c0"], w["cache"]["f18bc"] = c0, bc
        yield case(SORT_ENTRIES, [], w)
    for size in (0x7fff, -0x8000):
        for n in (1, 2, 3, 4):
            w, order = sort_entries_world(r, n)
            for en in w["entries"]:
                en["w"], en["h"] = size, 0x7fff
            yield case(SORT_ENTRIES, [], w)
    # The effect of AutoMipmap and of Entry_Compress on the area.
    for mip in (None, 0, 1000, -1, 0x7fffffff, -0x80000000):
        for comp in (None, 0, 3, 50, -1):
            for e4 in (0, 10, 60):
                w, order = sort_entries_world(r, 5)
                if mip is not None:
                    w["mip"] = (1, mip)
                if comp is not None:
                    w["comp"] = (1, comp)
                w["cache"].update(flags=LIMITED, f18e4=e4, f18c0=0, f18bc=0)
                yield case(SORT_ENTRIES, [], w)
    # Dropping stops as soon as the area is within the limit: limits equal to the area less the cache
    # areas of the last entries in the priority order (equality), one under and one over.
    for n in (2, 3, 5, 8):
        for blend_at in (None, 0, 1):
            w, order = sort_entries_world(r, n)
            ranked = []
            for k, index in enumerate(order):
                en = w["entries"][index - 1]
                en.update(prio=float(n - k), w=8 + k, h=16, cw=8 * (k + 1), ch=16)
                en["flags"] = BLEND if blend_at is not None and k == n - 1 - blend_at else 0
                ranked.append(en)
            area = area_of(w, order)
            freed = 0
            for m in range(1, n):
                last = ranked[n - m]
                freed += 0 if last["flags"] & BLEND else last["cw"] * last["ch"]
                for delta in (-1, 0, 1):
                    w2 = copy.deepcopy(w)
                    w2["cache"].update(flags=LIMITED, f18c0=0, f18bc=0, f18e4=area - freed + delta)
                    yield case(SORT_ENTRIES, [], w2)
    # The last entries are of the 0x80000 kind (never flagged), or already flagged 0x8000.
    for kinds in ((0, 0, 1), (1, 1, 1), (0, 1, 0), (1, 0, 0)):
        w, order = sort_entries_world(r, 3)
        for k, blend in enumerate(kinds):
            w["entries"][k]["prio"] = float(3 - k)
            w["entries"][k]["flags"] |= BLEND if blend else 0
        w["cache"].update(flags=LIMITED, f18e4=-1, f18c0=0, f18bc=0)
        yield case(SORT_ENTRIES, [], w)
    for dropped in (0, 1, 2, 3, 5, 7):
        w, order = sort_entries_world(r, 3)
        for k in range(3):
            if (dropped >> k) & 1:
                w["entries"][k]["flags"] |= DROPPED
        yield case(SORT_ENTRIES, [], w)
    # Priorities: ties, descending, ascending, not numbers, the limits' arithmetic.
    for prios in ([1.0] * 4, [0.0, 1.0, 2.0, 3.0], [3.0, 2.0, 1.0, 0.0], [NAN, 1.0, 2.0, NAN], [INF, 1.0, -INF, 0.5], [-0.0, 0.0, -0.0, 0.0],
                  [1e30, 1e-30, 3e38, 1.0], [2.0, 2.0, 1.0, 1.0]):
        w, order = sort_entries_world(r, 4)
        for k, p_ in enumerate(prios):
            w["entries"][k]["prio"] = p_
        yield case(SORT_ENTRIES, [], w)
    # Flags that change the key: locked, the 0x80000 kind, the wanted pair; sizes of the key.
    for flags in (LOCKED, BLEND, LOCKED | BLEND, 0xc0, 0x30, 0xf0, 0xffffffff, 0x8000 | 0xc0):
        w, order = sort_entries_world(r, 3)
        for en in w["entries"]:
            en["flags"] = flags
        w["cache"]["flags"] = LIMITED
        w["cache"]["f18e4"] = 1
        yield case(SORT_ENTRIES, [], w)
    # More than 4096 in the queue: the ones past the limit are reset (some of the 0x80000 kind,
    # some locked, some with the wanted pair set).
    for n in (4095, 4096, 4097, 4101, 4200):
        w, order = sort_entries_world(r, n)
        for k, index in enumerate(order[4096:]):
            w["entries"][index - 1]["flags"] = [BLEND, LOCKED | 0xc0, BLEND | LOCKED | ONPAGE, 0x10, DROPPED | 0xf0][k % 5]
        yield case(SORT_ENTRIES, [], w)


def edge_merge():
    r = random.Random(88)
    for freq in (0, 1, 7, 15):
        for flags in (0, 1):
            for entry_flags in (0, ASKED, LOCKED, ASKED | LOCKED, QUEUED, ASKED | QUEUED | ONPAGE | LOCKED | DROPPED):
                w = random_world(r, 4, nl=2)
                for en in w["entries"]:
                    en["texture"], en["page"] = r.choice([1, 2]), r.choice([1, 2])
                w["freq"] = freq
                w["cache"]["flags"] = flags
                for k, en in enumerate(w["entries"]):
                    en["flags"] = entry_flags
                    en["place"] = [0, 1, 1, 2][k]
                    en["next"] = k + 2 if k < 3 else 0
                    en["sortkey"] = 100 - 10 * k
                    en["own"] = dict(bytes=[k + 1] * 16, f10=100 * (k + 1), f14=-(k + 1) * 7 - 1)
                w["places"][0] = dict(bytes=[0xff] * 16, f10=-3, f14=-0x80000000)
                w["places"][1] = dict(bytes=list(range(16)), f10=5, f14=1)
                yield case(MERGE, [1], w)
    # Insertion: before everything, after everything, in the middle, equal keys (signed order).
    for listed in ([], [5], [50, 40, 30], [-5, -10], [0x7fffffff, 0], [-0x80000000]):
        for key in (100, 45, 40, 35, 0, -7, 0x7fffffff, -0x80000000, -1):
            w = random_world(r, 1 + len(listed))
            w["freq"] = 3
            for k, lk in enumerate(listed):
                w["entries"][k + 1]["sortkey"] = lk
            w["head"] = set_chain(w, list(range(2, 2 + len(listed))))
            w["entries"][0].update(flags=ASKED, sortkey=key, next=0)
            yield case(MERGE, [1], w)
    # Several at once, in orders that interleave.
    for n in (2, 3, 5, 8):
        for kind in ("sorted", "reverse", "equal", "random"):
            w = random_world(r, n + 3)
            w["freq"] = r.randrange(16)
            keys = {"sorted": list(range(n)), "reverse": list(range(n, 0, -1)), "equal": [4] * n, "random": [r.randint(-9, 9) for _ in range(n)]}[kind]
            for k in range(n):
                w["entries"][k].update(flags=ASKED | (LOCKED if k % 3 == 2 else 0), sortkey=keys[k])
            set_chain(w, list(range(1, n + 1)))
            w["head"] = set_chain(w, [n + 1, n + 2, n + 3])
            for k, lk in zip((n + 1, n + 2, n + 3), (5, 1, -5)):
                w["entries"][k - 1]["sortkey"] = lk
            w["cache"]["flags"] = r.choice([0, 1])
            yield case(MERGE, [1], w)
    # An empty chain.
    w = random_world(r, 2)
    w["head"] = set_chain(w, [1, 2])
    yield case(MERGE, [0], w)
    # The key of a reset entry: priorities, locked, kind, sizes, the wanted pair.
    for prio in (0.0, 1.5, 2.0, -3.0, NAN, INF, 1.0e9):
        for flags in (0, LOCKED, BLEND, 0xc0, 0xffffffff & ~ASKED):
            w = random_world(r, 1)
            w["entries"][0].update(flags=flags, prio=prio, w=0x7fff, h=-0x8000, next=0)
            yield case(MERGE, [1], w)


def edge_create_nw():
    r = random.Random(99)

    def world(page, others, wraps=(), asked=None, sizes=None):
        n = len(page) + len(others)
        w = random_world(r, n, nl=1)
        ids = list(range(1, n + 1))
        on_page, extra = ids[:len(page)], ids[len(page):]
        for k, n_ in enumerate(ids):
            hw = (sizes or {}).get(n_, ((page + others)[k]))
            w["entries"][n_ - 1].update(h=hw[0], w=hw[1], cw=16 * (k + 1), ch=8 + k, flags=ASKED if (asked is None or n_ in asked) else 0,
                                        wrap=1 if n_ in wraps else 0)
        w["links"][0]["entries"] = set_chain(w, on_page)
        return w, set_chain(w, extra)

    # Both chains empty, one empty.
    w, head = world([], [])
    yield case(CREATE_NW, [0, 1], w)
    for chain in ([(4, 4)], [(4, 4), (3, 3)], [(2, 9), (2, 3), (1, 1)]):
        w, head = world(list(chain), [])
        yield case(CREATE_NW, [0, 1], w)
        w, head = world([], list(chain))
        yield case(CREATE_NW, [head, 1], w)
    # Page first, others first, equal sizes, a taller or wider one in either chain.
    pairs = [((3, 3), (3, 3)), ((3, 3), (4, 3)), ((4, 3), (3, 3)), ((3, 3), (3, 4)), ((3, 4), (3, 3)), ((2, 9), (3, 1)), ((3, 1), (2, 9))]
    for a_, b_ in pairs:
        w, head = world([a_], [b_])
        yield case(CREATE_NW, [head, 1], w)
        w, head = world([a_, a_], [b_, b_])
        yield case(CREATE_NW, [head, 1], w)
    # The first entry on the page stored as wrapping is skipped; entries of the chain that are wrapping are skipped, anywhere.
    for wraps in ((), (1,), (2,), (1, 2), (3,), (4,), (3, 4), (1, 3), (2, 4), (1, 2, 3, 4)):
        w, head = world([(5, 5), (4, 4)], [(5, 4), (3, 3)], wraps=wraps)
        yield case(CREATE_NW, [head, 1], w)
    for wraps in ((), (3,), (4,), (5,), (3, 4, 5), (3, 5)):
        w, head = world([(6, 6)], [(5, 5), (4, 4), (3, 3)], wraps=wraps)
        yield case(CREATE_NW, [head, 1], w)
    # Page entries that were not asked for are skipped (at the start, in the middle, at the end).
    for asked in ((), (1,), (2,), (3,), (1, 3), (2, 3), (1, 2), (1, 2, 3)):
        w, head = world([(5, 5), (4, 4), (3, 3)], [(5, 4), (4, 3)], asked=set(asked) | {4, 5})
        yield case(CREATE_NW, [head, 1], w)
        w, head = world([(5, 5), (4, 4), (3, 3)], [], asked=set(asked))
        yield case(CREATE_NW, [0, 1], w)
    # Tails: one chain much longer.
    w, head = world([(9, 9)], [(8, 8), (7, 7), (6, 6), (5, 5)])
    yield case(CREATE_NW, [head, 1], w)
    w, head = world([(9, 9), (8, 8), (7, 7), (6, 6), (5, 5)], [(2, 2)])
    yield case(CREATE_NW, [head, 1], w)
    # The sum: wide sizes wrap.
    w, head = world([(5, 5)], [(4, 4)])
    for en in w["entries"]:
        en["cw"], en["ch"] = -0x8000, -0x8000
    yield case(CREATE_NW, [head, 1], w)
    w, head = world([(5, 5)], [(4, 4)])
    for en in w["entries"]:
        en["cw"], en["ch"] = 0x7fff, 0x7fff
    yield case(CREATE_NW, [head, 1], w)


def test(name, edges, rnd):
    return FnTest(name, ppc_run, host_run, edge=edges, rnd=rnd)


TESTS = [test("CTextureCache::Textures_QSort", edge_qsort(False), rnd_qsort(False)),
         test("CTextureCache::Textures_QSortFree", edge_qsort(True), rnd_qsort(True)),
         test("CTextureCache::SortTextures", edge_sort_textures, rnd_sort_textures),
         test("CTextureCache::Entry_QSortInit", edge_entry_qsort, rnd_entry_qsort),
         test("CTextureCache::SortEntries", edge_sort_entries, rnd_sort_entries),
         test("CTextureCache::SortByCriteria", lambda: list(edge_by_list(BY_CRITERIA)()) + list(edge_big_lists(BY_CRITERIA)), rnd_by_criteria),
         test("CTextureCache::SortByAddress", lambda: list(edge_by_list(BY_ADDRESS)()) + list(edge_big_lists(BY_ADDRESS)), rnd_by_address),
         test("CTextureCache::MergeListsConditional", edge_merge, rnd_merge),
         test("CTextureCache::CreateNonWrappingTextureList", edge_create_nw, rnd_create_nw)]
TESTS = [qs_test(op) for op in range(1, 9)] + TESTS
