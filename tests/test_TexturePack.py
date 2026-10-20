"""The texture cache's packing (engine library, tb_texturecache_pack.cpp):
CTextureCache::ArrangeNonwrappingTextures and PackTextures, original versus translation.

A case is one call on a world: a few globals, the cache's scalars, 24 pages (six sizes with
four slots each: a link and an OpenGL texture), up to 140 texture entries, the queue's head
and, for ArrangeNonwrappingTextures, the array of entries to lay out. The test builds the
world on both sides from the same description (the host library reads it as a flat list, the
PowerPC side is written into emulator memory), runs the call and compares its answer, the
recorded calls and the state of everything the call could change, read in the order
tests/unit_TexturePack.cpp writes the host's.

Everything outside the two functions is a fake on both sides that records its arguments: the
entry's StoreAsWrapping and GetBestSize (answered from the entry's flags), CTextureBase::SetFlags,
the page's LockSurface, UnlockSurface and GetPixelCount, UploadToVRAM, CTextureLink::PageUpdated,
the list helpers SortByCriteria (puts the chain in order of the sort key, largest first),
SortByAddress (in order of address), MergeListsConditional (appends the page's chain) and
CreateNonWrappingTextureList (lists the entries a queue says), the quick sorter CQSort::ulDes
(records the call, the first records and a hash of them all, then sorts the range by key, the
largest first, equal keys in order) and the assertion handler. The inline getters of the page
(GetFlags, IsPositionLocked, PositionLock) run as the original.

The original's fctiwz (the sort key of a cut-down entry) runs under HardwareRounding, as in
test_NodeGamuts.py: Unicorn saturates a value above INT32_MAX to 0x80000000 where the processor
gives 0x7fffffff.
"""
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
SHIM_LIBRARY = "unit_TexturePack"

ARRANGE, PACK = 0x10036100, 0x1003678c
ARRANGE_SIZE, PACK_SIZE = 1560, 3268
STORE_WRAP, BEST_SIZE = 0x10041848, 0x10041204
SET_FLAGS = 0x10032bd0
LOCK, UNLOCK, PIXELS = 0x10046480, 0x100464d0, 0x10046a94
UPLOAD, PAGE_UPDATED = 0x1003820c, 0x100385f4
SORT_CRITERIA, SORT_ADDRESS, MERGE, CREATE = 0x100358ec, 0x10035ab0, 0x10035be0, 0x10035e6c
QSORT = 0x100203f8
# The members they reach that run as the original: CTextureBase's inline getters and lock, and the
# constructors that make the OpenGL texture's vtable.
INNER = [0x10032b98, 0x10032c14, 0x10032cdc, 0x10045d88, 0x10032b58]

SYSTEM = 0x10165f80
SORTER = 0x100b161c
# Engine BSS: cut-downs of a whole-page entry, quads pending, quads made, steps to a smaller page size.
COUNTERS = (0x10173188, 0x1017318c, 0x10173190, 0x10173198)
MASK = 0xFFFFFFFF
SIZES = (256, 128, 64, 32, 16, 8)
SLOTS, PAGES, MAX_ENTRIES, QUADS = 4, 24, 140, 0x80
CAPACITY = 4096

# Labels (tests/unit_TexturePack.cpp has the same numbers).
LABEL_CACHE, LABEL_SORTER, LABEL_HEAD, LABEL_TEXTURE, LABEL_LINK, LABEL_ENTRY = 1, 2, 3, 10, 40, 100

# Operations.
OP_ARRANGE, OP_PACK = 1, 2

# Entry flag bits.
LOCKED, WRAP_U, WRAP_V, WRAP, BLEND_KIND = 0x4000, 0x10000, 0x20000, 0x30000, 0x80000
LEVEL = 0x30
# Cache flag bits.
RESHUFFLE, CENTRE = 1, 2


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def fb(x):
    return T.f32_bits(x)


def canon(word):
    """A single-precision value's bits with every NaN the same."""
    return 0x7FC00000 if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF) else word


def s16(v):
    return T.s16(v)


def sort_key(prio, cw, ch, flags):
    """The sort key the cut-down gives an entry (the original's float steps in single precision)."""
    scaled = T.f32(2047.0 * T.f32(1.5 - prio))
    key = ((int(scaled) << 16) & 0x0fff0000)
    key = (cw * 256 + (ch + key)) & MASK
    if flags & LOCKED:
        key |= 0x20000000
    if flags & BLEND_KIND:
        key |= 0x40000000
    return key - (1 << 32) if key >= (1 << 31) else key


def s32(v):
    return T.s32(v)


# ----------------------------------------------------------------- the world


def blank_page(size_index):
    n = SIZES[size_index]
    return dict(entries=0, f08=0, f0c=0, f10=0, f14=-1, f18=0, f1c=0, tflags=0, tw=n, th=n, tpitch=n * 4)


def blank_entry():
    return dict(flags=0, w=32, h=32, cw=32, ch=32, prio=0.5, sortkey=0, next=0)


def blank_world():
    return dict(renderer=3, counters=[0, 0, 0, 0], cache_flags=0, mask=7, mipmaps=0, widest=0, tallest=0, reordered=0,
                pagecount=[4] * 6, usable=[2] * 6, used=[0] * 6,
                pages=[blank_page(p // SLOTS) for p in range(PAGES)], head=0, entries=[], listing=[], queues={})


def page_of(size, slot=0):
    return size * SLOTS + slot


def flat(w):
    """The description as the host library reads it (the order of tests/unit_TexturePack.cpp)."""
    d = [w["renderer"]] + list(w["counters"])
    d += [w["cache_flags"], w["mask"], w["mipmaps"], w["widest"], w["tallest"], w["reordered"]]
    d += list(w["pagecount"]) + list(w["usable"]) + list(w["used"])
    for page in w["pages"]:
        d += [page[k] for k in ("entries", "f08", "f0c", "f10", "f14", "f18", "f1c", "tflags", "tw", "th", "tpitch")]
    d += [w["head"], len(w["entries"])]
    for en in w["entries"]:
        d += [en["flags"], en["w"], en["h"], en["cw"], en["ch"], fb(en["prio"]), en["sortkey"], en["next"]]
    d += [len(w["listing"])] + list(w["listing"])
    return [v & 0xFFFFFFFFFFFFFFFF for v in d]


def signed64(v):
    return v - (1 << 64) if v >= (1 << 63) else v


# ----------------------------------------------------------------- the emulator's fctiwz

LIBM = C.CDLL(None)
CODE_RANGES = [(ARRANGE, ARRANGE + ARRANGE_SIZE), (PACK, PACK + PACK_SIZE)]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000 under Unicorn (QEMU
    5.0); the processor saturates to 0x7fffffff. A code hook looks at each instruction before it
    runs and rewrites the target register just before the next one (as in test_NodeGamuts.py)."""

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
            target = self.pending
            self.pending = None
            HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xffffffff00000000) | 0x7fffffff)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, rb = (word >> 21) & 31, (word >> 11) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3ff == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = rd


# ----------------------------------------------------------------- PowerPC side

_TEMPLATE = {}


class Ppc:
    """The world in emulator memory, the fakes, the call and the state."""

    def __init__(self, e, w):
        self.e, self.w = e, w

    def texture_template(self):
        """The bytes the original's constructor makes of an OpenGL texture (built once)."""
        e = self.e
        if id(e) not in _TEMPLATE:
            obj = e.alloc(0x34, b"\xa5" * 0x34)
            T.call_ppc(e, 0x10045d88, [obj, 1])
            _TEMPLATE[id(e)] = e.read(obj, 0x34)
        return _TEMPLATE[id(e)]

    def build(self):
        e, w = self.e, self.w
        entries = w["entries"]
        template = self.texture_template()
        w32(e, SYSTEM + 0x1c, w["renderer"])
        for addr, v in zip(COUNTERS, w["counters"]):
            w32(e, addr, v)

        self.cache = e.alloc(0x1910, b"\xa5" * 0x1910)
        c = self.cache
        e.write(c, b"\0" * 0x1800)
        self.links = e.alloc(PAGES * 0x20, b"\xa5" * (PAGES * 0x20))
        self.textures = [e.alloc(0x34, template) for _ in range(PAGES)]
        self.entries = e.alloc(max(len(entries), 1) * 0x5c, b"\0" * (max(len(entries), 1) * 0x5c))
        self.headvar = e.alloc(4, b"\0\0\0\0")
        self.listarr = e.alloc((MAX_ENTRIES + 2) * 4, b"\0" * ((MAX_ENTRIES + 2) * 4))
        self.quads = e.alloc((QUADS + 1) * 0x14, b"\x5a" * ((QUADS + 1) * 0x14))
        self.count = e.alloc(4, b"\x5a\x5a\x5a\x5a")

        def entry(index):
            return self.entries + 0x5c * (index - 1) if 0 < index <= len(entries) else 0

        self.entry_at = entry
        w32(e, c + 0x18b4, w["cache_flags"])
        w32(e, c + 0x18cc, w["mask"])
        w32(e, c + 0x18d0, w["mipmaps"])
        w32(e, c + 0x1900, w["widest"])
        w32(e, c + 0x1904, w["tallest"])
        w32(e, c + 0x190c, w["reordered"])
        if w.get("pad1848") is not None:
            # The word after the used-page counts (index 6 of the counters, which the host never reads).
            w32(e, c + 0x1848, w["pad1848"])
        for k in range(6):
            w32(e, c + 0x1800 + 4 * k, w["pagecount"][k])
            w32(e, c + 0x1818 + 4 * k, w["usable"][k])
            w32(e, c + 0x1830 + 4 * k, w["used"][k])
            for slot in range(SLOTS):
                w32(e, c + 0x400 * k + 4 * slot, self.links + 0x20 * page_of(k, slot))
        for p, page in enumerate(w["pages"]):
            obj = self.textures[p]
            base = obj + 0x1c
            e.write(base, struct.pack(">IIII", page["tflags"] & MASK, page["tw"] & MASK, page["th"] & MASK, page["tpitch"] & MASK))
            e.write(self.links + 0x20 * p, struct.pack(">IIIIIIII", base, entry(page["entries"]), page["f08"] & MASK, page["f0c"] & MASK,
                                                       page["f10"] & MASK, page["f14"] & MASK, page["f18"] & MASK, page["f1c"] & MASK))
        for i, en in enumerate(entries):
            a = self.entries + 0x5c * i
            e.write(a, struct.pack(">IIIhhhhIIIIIII", 0, entry(en["next"]), en["flags"] & MASK, s16(en["w"]), s16(en["h"]),
                                   s16(en["cw"]), s16(en["ch"]), 0, 0, 0, 0, 0, 0, fb(en["prio"])))
            w32(e, a + 0x34, en["sortkey"])
        w32(e, self.headvar, entry(w["head"]))
        for i, index in enumerate(w["listing"]):
            w32(e, self.listarr + 4 * i, entry(index))

        fixed = {c: LABEL_CACHE, SORTER: LABEL_SORTER, self.headvar: LABEL_HEAD}
        for p in range(PAGES):
            fixed[self.textures[p]] = LABEL_TEXTURE + p
            fixed[self.textures[p] + 0x1c] = LABEL_TEXTURE + p
            fixed[self.links + 0x20 * p] = LABEL_LINK + p
        for i in range(len(entries)):
            fixed[self.entries + 0x5c * i] = LABEL_ENTRY + 1 + i
        self.fixed = fixed

    # -- the fakes
    def chain(self, head):
        out, e = [], self.e
        while head and len(out) < 64:
            out.append(head)
            head = e.u32(head + 4)
        return out

    def relink(self, listp, order):
        e = self.e
        w32(e, listp, order[0] if order else 0)
        for i, a in enumerate(order):
            w32(e, a + 4, order[i + 1] if i + 1 < len(order) else 0)

    def record_list(self, name, e):
        L = e.cl
        this, listp = e.r(3), e.r(4)
        chain = self.chain(e.u32(listp))
        labels = [L.lab(x) for x in chain[:9]] + [0] * 9
        L.rec(name, (L.lab(this), L.lab(listp), len(chain)) + tuple(labels[:9]))
        return listp, chain

    def sort_criteria(self, e):
        listp, chain = self.record_list("SortByCriteria", e)
        self.relink(listp, sorted(chain, key=lambda a: -s32(e.u32(a + 0x34))))

    def sort_address(self, e):
        listp, chain = self.record_list("SortByAddress", e)
        self.relink(listp, sorted(chain))

    def merge(self, e):
        L = e.cl
        this, listp, other = e.r(3), e.r(4), e.r(5)
        chain = self.chain(e.u32(listp))
        extra = self.chain(other)
        labels = [L.lab(x) for x in extra[:6]] + [0] * 6
        L.rec("MergeListsConditional", (L.lab(this), L.lab(listp), L.lab(other), len(chain), len(extra)) + tuple(labels[:6]))
        self.relink(listp, chain + extra)

    def create(self, e):
        L = e.cl
        this, out, first, link = e.r(3), e.r(4), e.r(5), e.r(6)
        L.rec("CreateNonWrappingTextureList", (L.lab(this), L.lab(first), L.lab(link)))
        packed = L.ret("CreateList", 0)
        n = 0
        for shift in range(0, 60, 4):
            number = (packed >> shift) & 0xf
            if number == 0:
                break
            w32(e, out + 4 * n, self.entry_at(number))
            n += 1
        w32(e, out + 4 * n, 0)
        e.set_r(3, T.u32(L.ret("CreateCount", n)))

    def qsort(self, e):
        L = e.cl
        this, array, size, key_offset, lo, hi = e.r(3), e.r(4), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), s32(e.r(9))
        L.rec("QSort", (L.lab(this), size, key_offset, lo, hi))
        words = size // 4
        records = [list(struct.unpack(">%dI" % words, e.read(array + size * i, size))) for i in range(lo, hi + 1)]
        h = 0
        for k, record in enumerate(records):
            if k < 4 and k % 2 == 0:
                nxt = records[k + 1] if k + 1 < len(records) else [0] * words
                L.rec("QSortRecords", tuple((record + [0] * 5)[:5] + (nxt + [0] * 5)[:5]))
            h = (h * 1000003 + sum(v * (j + 1) for j, v in enumerate(record))) & 0xFFFFFFFFFFFFFFFF
        L.rec("QSortHash", (len(records), signed64(h)))
        key = key_offset // 4
        records.sort(key=lambda r: -r[key])
        for i, record in zip(range(lo, hi + 1), records):
            e.write(array + size * i, struct.pack(">%dI" % words, *record))

    def stubs(self):
        st = CL.Stubs()
        st.real = {ARRANGE, PACK} | set(INNER)

        def store_as_wrapping(e):
            L = e.cl
            entry = e.r(3)
            L.rec("StoreAsWrapping", (L.lab(entry),))
            wrapping = (e.u32(entry + 8) & WRAP) != 0 and s32(e.u32(SYSTEM + 0x1c)) != 4
            e.set_r(3, 1 if wrapping else 0)

        def best_size(e):
            L = e.cl
            entry = e.r(3)
            L.rec("GetBestSize", (L.lab(entry),))
            flags = e.u32(entry + 8)
            if (flags & LEVEL) == LEVEL:
                e.set_r(3, flags & 7)
                return
            width = struct.unpack(">h", e.read(entry + 0x10, 2))[0]
            for limit, value in ((0x80, 0), (0x40, 1), (0x20, 2), (0x10, 3), (8, 4)):
                if width > limit:
                    e.set_r(3, value)
                    return
            e.set_r(3, 5)

        def set_flags(e):
            L = e.cl
            base, flags = e.r(3), e.r(4)
            L.rec("SetFlags", (L.lab(base), s32(flags)))
            w32(e, base, flags | (e.u32(base) & 0x10000000))

        def answer(name, default):
            return lambda e, L: L.ret(name, default)

        def upload(e):
            L = e.cl
            this, link, quad, entry = e.r(3), e.r(4), e.r(5), e.r(6)
            x, y, w, h = (s32(e.u32(quad + 4 * k)) for k in range(4))
            L.rec("UploadToVRAM", (L.lab(this), L.lab(link), x, y, w, h, L.lab(e.u32(quad + 0x10)), L.lab(entry)))
            e.set_r(3, T.u32(L.ret("Upload", 1)))

        st.internal(STORE_WRAP, store_as_wrapping)
        st.internal(BEST_SIZE, best_size)
        st.internal(SET_FLAGS, set_flags)
        st.internal(LOCK, CL.recorder("LockSurface", "L", answer("Lock", 1)))
        st.internal(UNLOCK, CL.recorder("UnlockSurface", "L", answer("Unlock", 1)))
        st.internal(PIXELS, CL.recorder("GetPixelCount", "L", answer("Pixels", 0)))
        st.internal(UPLOAD, upload)
        st.internal(PAGE_UPDATED, CL.recorder("PageUpdated", "L", answer("PageUpdated", 1)))
        st.internal(SORT_CRITERIA, self.sort_criteria)
        st.internal(SORT_ADDRESS, self.sort_address)
        st.internal(MERGE, self.merge)
        st.internal(CREATE, self.create)
        st.internal(QSORT, self.qsort)
        st.imp("__assertion_failed", lambda emu: emu.cl.rec("assert", (T.s32(emu.r(5)),), (emu.cstr(emu.r(3)), emu.cstr(emu.r(4)))))
        return st

    # -- the call and the state
    def call(self, op, a):
        e = self.e
        if op == OP_ARRANGE:
            base = self.textures[a[0] * SLOTS + a[1]] + 0x1c
            return T.call_ppc(e, ARRANGE, [self.cache, self.listarr, self.quads, self.count, base, a[2] & MASK])[0] & 0xff
        return s32(T.call_ppc(e, PACK, [self.cache, self.headvar])[0])

    def lab(self, address):
        return self.e.cl.lab(address)

    def state(self):
        e, u = self.e, self.e.u32
        c = self.cache
        out = [s32(u(addr)) for addr in COUNTERS]
        out += [u(c + 0x18b4)] + [s32(u(c + off)) for off in (0x18cc, 0x18d0, 0x1900, 0x1904, 0x190c)]
        out += [s32(u(c + 0x1800 + 4 * k)) for k in range(6)]
        out += [s32(u(c + 0x1818 + 4 * k)) for k in range(6)]
        out += [s32(u(c + 0x1830 + 4 * k)) for k in range(6)]
        out.append(self.lab(u(self.headvar)))
        for p in range(PAGES):
            a = self.links + 0x20 * p
            out.append(self.lab(u(a + 4)))
            out += [s32(u(a + off)) for off in (8, 0xc, 0x10, 0x14, 0x18, 0x1c)]
            base = self.textures[p] + 0x1c
            out += [u(base), s32(u(base + 4)), s32(u(base + 8)), s32(u(base + 0xc))]
        for i in range(len(self.w["entries"])):
            a = self.entries + 0x5c * i
            out += [self.lab(u(a + 4)), u(a + 8)]
            out += [struct.unpack(">h", e.read(a + 0x10, 2))[0], struct.unpack(">h", e.read(a + 0x12, 2))[0]]
            out += [s32(u(a + 0x34)), canon(u(a + 0x2c))]
            out += [struct.unpack(">h", e.read(a + 0xc, 2))[0], struct.unpack(">h", e.read(a + 0xe, 2))[0]]
        out.append(s32(u(self.count)))
        for i in range(len(self.w["listing"]) + 1):
            out.append(self.lab(u(self.listarr + 4 * i)))
        for i in range(QUADS + 1):
            q = self.quads + 0x14 * i
            out += [s32(u(q + 4 * k)) for k in range(4)] + [self.lab(u(q + 0x10))]
        return out


def ppc_run(e, case):
    w = case["world"]
    p = Ppc(e, w)
    st = p.stubs()
    saved = st.activate(e)
    try:
        p.build()
        e.cl = CL.PpcLog(p.fixed, w["queues"])
        with HardwareRounding(e):
            result = p.call(case["op"], case["args"])
        return dict(result=result, state=p.state(), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, case):
    w = case["world"]
    CL.host_begin(lib, {}, w["queues"])
    description = flat(w)
    d = (C.c_int64 * len(description))(*[signed64(v) for v in description])
    args = (C.c_int64 * 4)(*[signed64(v & 0xFFFFFFFFFFFFFFFF) for v in (case["args"] + [0, 0, 0, 0])[:4]])
    out = (C.c_int64 * CAPACITY)()
    run = lib.tpw_tp_run
    run.restype = C.c_int64
    run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
    result = run(case["op"], args, d, out, CAPACITY)
    lib.tpw_tp_count.restype = C.c_int32
    n = lib.tpw_tp_count()
    log = CL.host_end(lib)
    assert n <= CAPACITY, "the state buffer is too small"
    return dict(result=result, state=list(out[:n]), log=log)


# ----------------------------------------------------------------- cases

NAN, INF = math.nan, math.inf


def case(op, args, w):
    return dict(op=op, args=list(args), world=w)


def entry_of(w, index, **kw):
    w["entries"][index - 1].update(kw)
    return w


def add_entries(w, n, **kw):
    for _ in range(n):
        en = blank_entry()
        en.update(kw)
        w["entries"].append(en)
    return w


def set_page(w, size, slot=0, **kw):
    w["pages"][page_of(size, slot)].update(kw)
    return w


def link_chain(w, order):
    """Makes the entries (1-based, in order) a chain through next; returns the head (or 0)."""
    for k, index in enumerate(order):
        w["entries"][index - 1]["next"] = order[k + 1] if k + 1 < len(order) else 0
    return order[0] if order else 0


def pack_create(*numbers):
    """The value of a CreateList queue entry: the entries (1..15) as 4-bit numbers."""
    value = 0
    for k, n in enumerate(numbers):
        value |= n << (4 * k)
    return value


def rsize(r):
    """A cache size: mostly the page sizes, some others and some that break the arithmetic."""
    return r.choice([8, 16, 32, 64, 128, 256, 8, 16, 32, 64, 512, r.randint(1, 300), r.randint(1, 40), 1, 7, 9, 0, -3, 0x7fff, -0x8000])


def rprio(r):
    k = r.random()
    if k < 0.25:
        return r.choice([0.0, -0.0, 0.5, 1.0, 1.5, 2.0, -1.0, 3.0e9, -3.0e9, 1.0e-40, NAN, INF, -INF, 1.0e6, -1.0e6])
    return T.f32(r.uniform(-1.0, 3.0))


def rflags(r, wrap=None):
    """An entry's flags: a store level (and with level 3 a size), wrapping, locked, the blend kind."""
    f = r.choice([0, 0, 0x10, 0x20, 0x30, 0x30])
    if f == 0x30:
        f |= r.choice([0, 1, 2, 3, 4, 5, 5, 6])
    if wrap is None:
        wrap = r.choice([0, 0, 0, WRAP_U, WRAP_V, WRAP, WRAP])
    f |= wrap
    if r.random() < 0.3:
        f |= LOCKED
    if r.random() < 0.2:
        f |= BLEND_KIND
    if r.random() < 0.15:
        f |= r.getrandbits(32) & 0xfff00f00 & ~(BLEND_KIND | 0x30000)
    return f & MASK


def rkey(r):
    return r.choice([0, 1, 100, 100, 5000, 0x3000, -1, 0x7fffffff, -0x80000000, r.randint(-100000, 100000), r.getrandbits(32) - (1 << 31)])


def rcache_flags(r):
    f = r.choice([0, 1, 2, 3, 3, 0, 1])
    if r.random() < 0.15:
        f |= r.getrandbits(32) & 0xfffffffc
    return f & MASK


def rmask(r):
    return r.choice([7, 7, 7, 7, 3, 15, 0, 31, 1, r.randint(0, 63), -1, -8])


def rcounters(r):
    return [r.choice([0, 5, -1, 0x7fffffff, r.randint(-1000, 1000)]) for _ in range(4)]


def garbage(r):
    return r.choice([0, 1, -1, 77, r.randint(-500, 500)])


def random_entry(r, wrap=None, sizes=None):
    en = blank_entry()
    cw, ch = (sizes() if sizes else (rsize(r), rsize(r)))
    en.update(flags=rflags(r, wrap), w=r.choice([cw, rsize(r)]), h=r.choice([ch, rsize(r)]), cw=cw, ch=ch, prio=rprio(r), sortkey=rkey(r))
    return en


# ---- ArrangeNonwrappingTextures


def arrange_world(r, count=None, small=False):
    w = blank_world()
    w["renderer"] = r.choice([3, 3, 3, 3, 4, 0])
    w["counters"] = rcounters(r)
    w["cache_flags"] = rcache_flags(r)
    w["mask"], w["mipmaps"] = rmask(r), r.choice([0, 0, 4, 1, r.randint(-2, 6)])
    w["widest"], w["tallest"], w["reordered"] = garbage(r), garbage(r), garbage(r)
    size = r.randrange(6)
    slot = r.randrange(SLOTS)
    tw = r.choice([SIZES[size], SIZES[size], SIZES[size], r.choice(SIZES), r.randint(1, 300), 0, 0x7fff])
    th = r.choice([tw, tw, tw, r.choice(SIZES), r.randint(1, 300), 0, 0x7fff])
    set_page(w, size, slot, tw=tw, th=th)
    n = count if count is not None else r.choice([0, 1, 2, 3, 4, 5, 6, 8, 10, 14, 20, 40])
    if small:
        sizes = lambda: (r.choice([8, 8, 16, 16, 32]), r.choice([8, 8, 16, 16, 32]))
    else:
        sizes = None
    for _ in range(n):
        w["entries"].append(random_entry(r, sizes=sizes))
    order = list(range(1, n + 1))
    r.shuffle(order)
    if r.random() < 0.5:
        order.sort(key=lambda i: -(w["entries"][i - 1]["cw"] * 256 + w["entries"][i - 1]["ch"]))
    w["listing"] = order
    return w, [size, slot, r.choice([0, 1, r.randint(-5, 5)])]


def rnd_arrange(r):
    w, args = arrange_world(r)
    return case(OP_ARRANGE, args, w)


def rnd_arrange_long(r):
    w, args = arrange_world(r, count=r.choice([100, 126, 127, 128, 129, 130, 139, 140]), small=True)
    set_page(w, 0, args[1], tw=256, th=256)
    args[0] = 0
    w["cache_flags"] &= ~RESHUFFLE
    return case(OP_ARRANGE, args, w)


def edge_arrange():
    r = random.Random(5)
    # An empty list, a page-sized entry, one that fits nowhere.
    w = blank_world()
    yield case(OP_ARRANGE, [0, 0, 1], w)
    for cw, ch in ((256, 256), (257, 256), (256, 257), (255, 255), (8, 8)):
        w = blank_world()
        add_entries(w, 1, cw=cw, ch=ch)
        w["listing"] = [1]
        yield case(OP_ARRANGE, [0, 0, 0], w)
    # The leftover shapes: width 9 over, height 9 over (strips), both, and 8 or less (each side).
    for rest_w, rest_h in ((0, 0), (8, 8), (9, 8), (8, 9), (9, 9), (200, 0), (0, 200), (-0, 8), (100, 100)):
        for mipmaps, flags in ((0, 0), (4, CENTRE), (4, 0), (0, CENTRE)):
            w = blank_world()
            w["mipmaps"], w["cache_flags"] = mipmaps, flags
            add_entries(w, 2, cw=256 - rest_w, ch=256 - rest_h, sortkey=5)
            w["entries"][1].update(cw=8, ch=8)
            w["listing"] = [1, 2]
            yield case(OP_ARRANGE, [0, 0, 0], w)
    # The mask and odd sizes with the centring flag.
    for mask in (7, 3, 15, 0, 1, -1, 31):
        for cw, ch in ((13, 9), (8, 8), (1, 1), (17, 33), (100, 28)):
            w = blank_world()
            w["mask"], w["mipmaps"], w["cache_flags"] = mask, 4, CENTRE
            add_entries(w, 3, cw=cw, ch=ch)
            w["entries"][1].update(cw=ch, ch=cw)
            w["listing"] = [1, 2, 3]
            yield case(OP_ARRANGE, [0, 0, 0], w)
    # Four 128 squares fill a 256 page; a fifth finds nothing left.
    w = blank_world()
    add_entries(w, 5, cw=128, ch=128)
    w["listing"] = [1, 2, 3, 4, 5]
    yield case(OP_ARRANGE, [0, 0, 0], w)
    # A middle entry that fits nowhere while later ones fit.
    w = blank_world()
    add_entries(w, 4, cw=100, ch=100)
    w["entries"][1].update(cw=400, ch=400)
    w["listing"] = [1, 2, 3, 4]
    yield case(OP_ARRANGE, [0, 0, 0], w)
    # The first entry cut down: the levels, the flags that matter, where it lands in the list.
    for renderer in (3, 4):
        for flags in (0x10, 0x20, 0x30, 0, 0x10 | WRAP, 0x30 | WRAP, 0x30 | LOCKED, 0x20 | BLEND_KIND, 0x30 | LOCKED | BLEND_KIND):
            for cw in (512, 300, 1000, 8, 9, 0x7fff, 1):
                for cache_flags in (RESHUFFLE, 0):
                    w = blank_world()
                    w["renderer"], w["cache_flags"] = renderer, cache_flags
                    add_entries(w, 4, cw=64, ch=64, flags=0)
                    for k, key in enumerate((0x40000, 0x30000, 0x20000, 0x10000)):
                        w["entries"][k + 0]["sortkey"] = key
                    w["entries"][0].update(flags=flags, cw=cw, ch=cw, prio=0.25)
                    w["listing"] = [1, 2, 3, 4]
                    yield case(OP_ARRANGE, [0, 0, 0], w)
    # Where the cut entry goes in the list: ahead of the lower keys, ties, past the last.
    for key in (0x7fffffff, 0x100000, 0x10000, 0x8000, 0, -5, 0x30000):
        for follow in range(4):
            w = blank_world()
            w["cache_flags"] = RESHUFFLE
            add_entries(w, 4, cw=40, ch=40)
            for k, k_key in enumerate((0x40000, 0x30000, 0x30000, 0x10000)):
                w["entries"][k].update(sortkey=k_key, cw=40, ch=40)
            w["entries"][0].update(flags=0x10, cw=600, ch=600, prio=0.5, sortkey=key)
            w["entries"][follow].update(sortkey=0x30000)
            w["listing"] = [1, 2, 3, 4]
            yield case(OP_ARRANGE, [0, 0, 0], w)
    # Keys equal to the cut entry's new key: it goes in front of equal ones or behind them (a strict test).
    for prio in (0.25, 0.5, 1.0, -0.75):
        for cut_from in (600, 1000, 17):
            new_size = (cut_from + 1) >> 1
            key = sort_key(prio, new_size, new_size, 0x10)
            for deltas in ((1, 0, -1), (0, 0, 0), (1, 1, 0), (0, -1, -1), (2, 1, 0), (-1, -2, -3), (0, 0, -1)):
                w = blank_world()
                w["cache_flags"] = RESHUFFLE
                add_entries(w, 4, cw=30, ch=30)
                w["entries"][0].update(flags=0x10, cw=cut_from, ch=cut_from, prio=prio, sortkey=key + 3)
                for k, delta in enumerate(deltas):
                    w["entries"][k + 1].update(sortkey=key + delta)
                w["listing"] = [1, 2, 3, 4]
                yield case(OP_ARRANGE, [0, 0, 0], w)
    # An entry cut down more than once, and cut down to a size that fits.
    w = blank_world()
    w["cache_flags"] = RESHUFFLE
    add_entries(w, 3, cw=32, ch=32)
    w["entries"][0].update(flags=0x30, cw=1000, ch=900, prio=0.5)
    w["listing"] = [1, 2, 3]
    yield case(OP_ARRANGE, [0, 0, 0], w)
    # The list limit: 127 quads, with 140 tiny entries.
    w = blank_world()
    add_entries(w, 140, cw=8, ch=8)
    w["listing"] = list(range(1, 141))
    yield case(OP_ARRANGE, [0, 0, 0], w)
    w = blank_world()
    add_entries(w, 128, cw=16, ch=16)
    w["listing"] = list(range(1, 129))
    yield case(OP_ARRANGE, [0, 0, 0], w)
    w = blank_world()
    add_entries(w, 127, cw=8, ch=8)
    w["listing"] = list(range(1, 128))
    yield case(OP_ARRANGE, [0, 0, 0], w)


# ---- PackTextures


def pack_sizes(r, mode):
    """The cache sizes of an entry: for a whole-page entry around the page sizes, for the others small."""
    if mode == "wrap":
        return lambda: (lambda n: (n, n))(r.choice([256, 512, 1024, 300, 130, 128, 64, 32, 16, 9, 8, 7, 2000, 0x7fff]))
    if mode == "rich":
        return lambda: (r.choice([8, 8, 16, 16, 32, 64]), r.choice([8, 8, 16, 16, 32, 64]))
    return lambda: (r.choice([8, 16, 32, 64, 128, 256, 512, 20, 100]), r.choice([8, 16, 32, 64, 128, 256, 512, 20, 100]))


def pack_world(r, mode):
    """A cache with six sizes of four pages, a queue of entries and, on some pages, entries already placed.
    mode: "wrap" (the queue starts with a whole-page entry), "shared" (it starts with an ordinary entry)."""
    w = blank_world()
    w["renderer"] = r.choice([3, 3, 3, 3, 3, 4])
    w["counters"] = rcounters(r)
    w["cache_flags"] = rcache_flags(r)
    w["mask"], w["mipmaps"] = rmask(r), r.choice([0, 0, 4, 1])
    w["widest"], w["tallest"], w["reordered"] = garbage(r), garbage(r), garbage(r)
    w["pagecount"] = [r.choice([0, 1, 2, 4, 4, 8, 256, r.randint(0, 40)]) for _ in range(6)]
    w["usable"] = [r.choice([0, 1, 2, 3, 4, 4, 12, 40]) for _ in range(6)]
    w["used"] = [r.choice([0, 0, 1, 2, 3]) for _ in range(6)]
    if r.random() < 0.9:
        s = r.randrange(6)
        w["used"][s] = r.randint(0, 2)
        w["usable"][s] = w["used"][s] + r.randint(1, 2)
    if mode == "rich":
        # Every size has pages left, with counts that make the smaller ones look emptier.
        w["pagecount"] = [r.choice([1, 4, 10, 40, r.randint(1, 60)]) for _ in range(6)]
        w["usable"] = [r.choice([1, 2, 4, 12, 40, 41]) for _ in range(6)]
        w["used"] = [r.randint(0, min(3, u - 1)) if r.random() < 0.9 else 3 for u in w["usable"]]
    if r.random() < 0.12:
        w["pad1848"] = r.choice([0x7fffffff, 0x7fffffff, 0, -1, r.randint(-5, 50)])
    for p in range(PAGES):
        n = SIZES[p // SLOTS]
        w["pages"][p].update(tw=r.choice([n, n, n, r.choice(SIZES), r.randint(1, 300)]), th=r.choice([n, n, n, r.choice(SIZES)]),
                             tflags=r.choice([0, 0, 1, 0x10000000, 5]), tpitch=r.choice([0, n * 4]), f08=garbage(r), f0c=garbage(r),
                             f10=garbage(r), f14=garbage(r), f18=garbage(r), f1c=garbage(r))
    count = r.randint(1, 12)
    sizes = pack_sizes(r, mode)
    for _ in range(count):
        w["entries"].append(random_entry(r, sizes=sizes if r.random() < 0.7 else None))
    order = list(range(1, count + 1))
    r.shuffle(order)
    queue_length = r.randint(1, count)
    queue, rest = order[:queue_length], order[queue_length:]
    head = queue[0]
    if mode == "wrap":
        wrap = r.choice([WRAP, WRAP, WRAP_U, WRAP_V])
        entry_of(w, head, flags=(rflags(r, wrap) & ~LOCKED) | (LOCKED if r.random() < 0.25 else 0))
        w["entries"][head - 1]["sortkey"] = rkey(r)
    else:
        entry_of(w, head, flags=rflags(r, 0))
        if mode == "rich":
            for en in w["entries"]:
                en["flags"] &= ~WRAP
    w["head"] = link_chain(w, queue)
    on_pages = {}
    for index in rest:
        if r.random() < 0.7:
            on_pages.setdefault(r.randrange(PAGES), []).append(index)
    for p, members in on_pages.items():
        w["pages"][p]["entries"] = link_chain(w, members)
    for index in rest:
        if not any(index in members for members in on_pages.values()):
            w["entries"][index - 1]["next"] = 0
    pool = queue + [index for members in on_pages.values() for index in members]
    lists = []
    for _ in range(r.randint(1, 4)):
        others = [index for index in pool if index != head]
        r.shuffle(others)
        picked = others[:r.randint(0, min(len(others), 8))]
        listing = [head] + picked if r.random() < 0.8 else picked + [head]
        if r.random() < 0.1:
            listing = r.sample(range(1, count + 1), r.randint(0, min(count, 6)))
        lists.append(pack_create(*listing[:12]))
    q = w["queues"]
    q["CreateList"] = lists
    if r.random() < 0.4:
        q["CreateCount"] = [r.choice([0, 1, 5, 100, 20000, -3, 100000]) for _ in range(r.randint(1, 3))]
    q["Lock"] = [r.choice([1, 1, 1, 1, 1, 0, 5])]
    q["Pixels"] = [r.choice([0, 100, 65536, 16384, 4096, 1000000, r.randint(0, 100000), -5])]
    q["Upload"] = [r.choice([1, 1, 1, 1, 1, 1, 0])] if r.random() < 0.7 else [1, 1, 0]
    q["PageUpdated"] = [r.choice([1, 0, -1, 5])]
    q["Unlock"] = [r.choice([1, 0])]
    return w


def rnd_pack_wrap(r):
    return case(OP_PACK, [], pack_world(r, "wrap"))


def rnd_pack_shared(r):
    return case(OP_PACK, [], pack_world(r, "shared"))


def rnd_pack_rich(r):
    w = pack_world(r, "rich")
    w["renderer"] = 3
    w["cache_flags"] |= RESHUFFLE if r.random() < 0.6 else 0
    # Counts the packing of smaller pages looks at: how many entries it listed, how big the pages are.
    w["queues"]["CreateCount"] = [r.choice([1, 8, 100, 400, 5000, 20000])]
    w["queues"]["Pixels"] = [r.choice([0, 100, 1024, 4096, 65536, 1000000])]
    return case(OP_PACK, [], w)


def pack_base(count=3, renderer=3, cache_flags=RESHUFFLE):
    """Entries 1 and 2 are the queue (entry 1 first), entry 3 and any others are on no list. Every size has two
    pages free."""
    w = blank_world()
    w["renderer"], w["cache_flags"] = renderer, cache_flags
    add_entries(w, count, cw=100, ch=100)
    for k, en in enumerate(w["entries"]):
        en["sortkey"] = 1000 - k
    w["usable"] = [2] * 6
    w["pagecount"] = [4] * 6
    w["head"] = link_chain(w, [1, 2])
    return w


def edge_pack_wrap():
    # A whole-page entry: sizes against the page (256), store levels, locked or not, with a page already holding another entry.
    for cw in (256, 512, 2000, 300, 129, 8, 9, 16, 128, 0x7fff, 257):
        for level in (0, 0x10, 0x20, 0x30):
            for locked in (0, LOCKED):
                for wrap in (WRAP, WRAP_U, WRAP_V):
                    w = pack_base()
                    entry_of(w, 1, flags=wrap | level | locked, cw=cw, ch=cw, prio=0.25)
                    w["pages"][0]["entries"] = link_chain(w, [3])
                    yield case(OP_PACK, [], w)
    # Not wrapping after all: the renderer that stores nothing as wrapping.
    w = pack_base(renderer=4)
    entry_of(w, 1, flags=WRAP | 0x10, cw=512, ch=512)
    w["queues"]["CreateList"] = [pack_create(1, 2)]
    yield case(OP_PACK, [], w)
    # The size to start at: the best size in the flags (level 3), pages used up, and none left.
    for start in range(0, 7):
        for exhausted in ((), (0,), (0, 1, 2), (0, 1, 2, 3, 4), (0, 1, 2, 3, 4, 5), (start,), (5,)):
            for pad in (None, 0x7fffffff, 0):
                w = pack_base()
                entry_of(w, 1, flags=WRAP | 0x30 | start, cw=64, ch=64)
                for size in exhausted:
                    if size < 6:
                        w["used"][size] = 2
                if pad is not None:
                    w["pad1848"] = pad
                yield case(OP_PACK, [], w)
    # The whole queue's best sizes from cw when the level is below 3 (the size of the width).
    for cw in (300, 200, 100, 50, 20, 10, 5):
        w = pack_base()
        entry_of(w, 1, flags=WRAP | 0x10, cw=cw, ch=cw)
        yield case(OP_PACK, [], w)
    # One size smaller when that size has more of its pages free (in proportion) than this one.
    for used0, usable0, count0 in ((0, 2, 4), (1, 2, 4), (2, 2, 4), (0, 1, 1), (0, 0, 0), (1, 4, 0), (3, 4, 8)):
        for used1, usable1, count1 in ((0, 2, 4), (1, 2, 4), (2, 2, 4), (0, 1, 1), (0, 0, 0), (1, 4, 0), (3, 4, 8), (0, 4, 8)):
            for level in (0, 0x10, 0x30):
                w = pack_base()
                entry_of(w, 1, flags=WRAP | level, cw=64, ch=64)
                w["used"][0], w["usable"][0], w["pagecount"][0] = used0, usable0, count0
                w["used"][1], w["usable"][1], w["pagecount"][1] = used1, usable1, count1
                yield case(OP_PACK, [], w)
    # The last size has no smaller one to move to (a page of that size that is wider than 8 lets the entry stay above 8).
    for page_width in (8, 64):
        for cw in (8, 16):
            w = pack_base()
            set_page(w, 5, 0, tw=page_width, th=page_width)
            entry_of(w, 1, flags=WRAP | 0x30 | 5, cw=cw, ch=8)
            yield case(OP_PACK, [], w)
    # The lock and the upload fail; the page's update answers.
    for lock, upload, updated in ((0, 1, 1), (1, 0, 1), (1, 1, 0), (1, 1, -1), (0, 0, 5), (1, 1, 7)):
        w = pack_base()
        entry_of(w, 1, flags=WRAP | 0x10, cw=256, ch=256)
        w["queues"].update(Lock=[lock], Upload=[upload], PageUpdated=[updated])
        yield case(OP_PACK, [], w)


def edge_pack_shared():
    # The simplest: one entry fits; the page's own entries come along; nothing fits (-1); two lists.
    for listing in ((1,), (1, 2), (1, 3), (1, 2, 3), (3, 1), (2,), ()):
        for cw in (100, 300, 256, 8):
            w = pack_base()
            w["pages"][0]["entries"] = link_chain(w, [3])
            entry_of(w, 1, cw=cw, ch=cw)
            w["queues"]["CreateList"] = [pack_create(*listing)]
            yield case(OP_PACK, [], w)
    # The page is locked in place without the reshuffle flag (an assertion), with it (fine).
    for cache_flags in (0, RESHUFFLE, 1 | CENTRE):
        for tflags in (0, 1, 4, 0x10000001):
            w = pack_base(cache_flags=cache_flags)
            w["pages"][0]["tflags"] = tflags
            w["queues"]["CreateList"] = [pack_create(1, 2)]
            yield case(OP_PACK, [], w)
    # Locked entries lock their page; entries that can still be cut keep the maximum-compression bit off.
    for flags1 in (0, LOCKED, 0x10, 0x30, LOCKED | 0x20):
        for flags2 in (0, LOCKED, 0x10, 0x30):
            w = pack_base()
            entry_of(w, 1, flags=flags1)
            entry_of(w, 2, flags=flags2)
            w["queues"]["CreateList"] = [pack_create(1, 2)]
            yield case(OP_PACK, [], w)
    # A failed upload (the first or the second entry), a failed lock, the page's update.
    for upload in ([0], [1, 0], [1, 1, 0]):
        for lock in (0, 1):
            w = pack_base(count=4)
            w["queues"]["CreateList"] = [pack_create(1, 2, 3)]
            w["queues"].update(Upload=upload, Lock=[lock], Unlock=[0, 1])
            yield case(OP_PACK, [], w)
    for updated in (0, 1, -1, 99):
        w = pack_base()
        w["queues"]["CreateList"] = [pack_create(1, 2)]
        w["queues"]["PageUpdated"] = [updated]
        yield case(OP_PACK, [], w)
    # Entries that stay in the queue (not in the quads) and ones in the queue behind the placed ones.
    w = pack_base(count=5)
    w["head"] = link_chain(w, [1, 2, 3, 4, 5])
    w["queues"]["CreateList"] = [pack_create(1, 3, 5)]
    yield case(OP_PACK, [], w)
    w["queues"]["CreateList"] = [pack_create(2, 4)]
    yield case(OP_PACK, [], w)
    w["queues"]["CreateList"] = [pack_create(5, 4, 3, 2, 1)]
    yield case(OP_PACK, [], w)
    # Small entries try every smaller page in turn; sizes with no page left are passed over, up to the end of the table
    # (the word after the last counter is read there: PowerPC side only).
    for used_up in ((), (2,), (2, 3), (3, 4, 5), (2, 3, 4, 5), (1,), (1, 2, 3, 4, 5), (5,), (4,)):
        for pad in (None, 0x7fffffff, 0, -1):
            for cw in (20, 8, 40):
                w = pack_base()
                for size in used_up:
                    w["used"][size] = 2
                if pad is not None:
                    w["pad1848"] = pad
                for size in range(6):
                    w["pagecount"][size] = 4 << size if size < 3 else 8
                entry_of(w, 1, cw=cw, ch=cw)
                entry_of(w, 2, cw=cw, ch=cw)
                w["queues"]["CreateList"] = [pack_create(1, 2)]
                yield case(OP_PACK, [], w)
    # The first entry does not fit the page (no quad at all: -1).
    for reshuffle in (0, RESHUFFLE):
        for level in (0, 0x10, 0x30):
            w = pack_base(cache_flags=reshuffle)
            entry_of(w, 1, cw=600, ch=600, flags=level)
            w["queues"]["CreateList"] = [pack_create(1, 2)]
            yield case(OP_PACK, [], w)


def edge_pack_prefer():
    # The arrangement fits the 256 page and then the 128 page (entries of 100): the preferred size 0 or the best size 1.
    # Free pages in proportion (twice as many), the lead in usable pages (over 10) and the page's pixels against the
    # entries listed (3 * pixels under 4 * count), each at the edge.
    cases = []
    for avail0, count0 in ((1, 10), (1, 4), (2, 4), (1, 1)):
        for avail1, count1 in ((10, 50), (10, 49), (10, 51), (10, 20), (11, 44), (9, 36), (12, 12), (3, 4), (1, 1)):
            cases.append((avail0, count0, avail1, count1, 11, 300, 399))
    for lead in (8, 9, 10, 11, 12):
        cases.append((1, 10, 10, 49, lead, 300, 399))
    for pixels in (398, 399, 400, 401, 402, 0, -1):
        for listed in (299, 300, 301):
            cases.append((1, 10, 10, 49, 11, listed, pixels))
    for number, (avail0, count0, avail1, count1, lead, listed, pixels) in enumerate(cases):
        w = pack_base()
        if number % 2:
            # The word after the used-page counts (the loop over the sizes reads it as the size after the last).
            w["pad1848"] = 0x7fffffff
        w["renderer"] = 3
        used0 = 0
        usable0 = avail0
        w["used"][0], w["usable"][0], w["pagecount"][0] = used0, usable0, count0
        usable1 = usable0 + lead
        used1 = max(0, usable1 - avail1)
        if used1 > 3:
            # the used count indexes four pages: keep it in range and let the usable count carry the difference
            used1 = 3
            usable1 = avail1 + used1
        w["used"][1], w["usable"][1], w["pagecount"][1] = used1, usable1, count1
        # the sizes below are used up so the tries stop at the 128 page
        for size in range(2, 6):
            w["used"][size] = 2
        # one entry of 100: it fits the 256 page and the 128 page, not the 64 page
        w["queues"]["CreateList"] = [pack_create(1)]
        w["queues"]["CreateCount"] = [listed]
        w["queues"]["Pixels"] = [pixels]
        yield case(OP_PACK, [], w)


def test(name, edges, rnd):
    return FnTest(name, ppc_run, host_run, edge=edges, rnd=rnd)


TESTS = [
    test("CTextureCache::ArrangeNonwrappingTextures", edge_arrange, rnd_arrange),
    test("CTextureCache::ArrangeNonwrappingTextures (long lists)", lambda: [], rnd_arrange_long),
    test("CTextureCache::PackTextures (a whole-page entry)", edge_pack_wrap, rnd_pack_wrap),
    test("CTextureCache::PackTextures (shared pages)", edge_pack_shared, rnd_pack_shared),
    test("CTextureCache::PackTextures (smaller pages)", edge_pack_prefer, rnd_pack_rich),
]
