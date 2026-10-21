"""Unit w83, the engine's Bitmap pixel methods, against libtpw_unit_Bitmap.dylib (tests/unit_Bitmap.cpp), in the program
"engine symbols shared":
  src/engine/Bitmap.cpp  colour_fill 0x10046e2c, copy_to 0x10046ee4, swizzle_for_gimex 0x10046f60, alphablt 0x10047910,
                  shadowblend 0x10047afc, colourblt 0x10047bb4, descimate 0x10047d24
(Bitmap::clear, realloc and load of the same source file are tested by test_landscape.py.)

Each function runs as the original in the emulator and as the host's translation over a bitmap (width, height, depth
and a buffer) built on both sides from the same bytes. Every buffer sits between two guard strips (0xa5) and is compared
whole, so a stray write, a skipped byte or an overrun shows; the depth fields are free (the methods treat every depth
other than the ones they name alike) and the buffers are sized to what the original reads and writes (a destination's
extent is worked out by walking the pointer the way the original steps it, and a case whose walk would leave the buffer
at the front is not generated). The destination of alphablt, shadowblend and colourblt is stepped by 4 or 1 byte per pixel
whatever its depth field says and skips depth * (dest width - source width) at the end of each row (a negative skip when
the destination is narrower: walked too).

The only callees are memcpy (a stub on the PowerPC side, the host's own) and the array operators new[] and delete[]
(descimate): both sides record (kind, size) for new[] and (kind, ordinal of the block, hash of its bytes at that moment)
for delete[], fill new blocks with 0xc3, and can make the Nth new[] of a run answer NULL. The hash compares the work
buffer's whole contents at the delete, so the first pass of descimate is checked beyond what reaches the destination.
Divisions by a zero count (a destination larger than the source) are not tested: divwu by zero is undefined on the
PowerPC, the host's division traps or answers whatever the CPU does.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_Bitmap"

GUARD = 16
FILL_GUARD = 0xA5
EXT = (0, 1, 2, 127, 128, 129, 254, 255)

OP_FILL, OP_COPY, OP_SWIZZLE, OP_ALPHA, OP_SHADOW, OP_COLOUR, OP_DESCIMATE = range(7)
ADDR = {OP_FILL: 0x10046E2C, OP_COPY: 0x10046EE4, OP_SWIZZLE: 0x10046F60, OP_ALPHA: 0x10047910,
        OP_SHADOW: 0x10047AFC, OP_COLOUR: 0x10047BB4, OP_DESCIMATE: 0x10047D24}
NAME = {OP_FILL: "Bitmap::colour_fill", OP_COPY: "Bitmap::copy_to", OP_SWIZZLE: "Bitmap::swizzle_for_gimex",
        OP_ALPHA: "Bitmap::alphablt", OP_SHADOW: "Bitmap::shadowblend", OP_COLOUR: "Bitmap::colourblt",
        OP_DESCIMATE: "Bitmap::descimate"}


# ----------------------------------------------------------------- cases


def bm(w, h, bpp, data):
    """A bitmap: dimensions, depth field and the buffer (bytes, or None for no data)."""
    return dict(w=w, h=h, bpp=bpp, data=data)


def case(op, src, dst=None, args=(), fail=0):
    return dict(op=op, src=src, dst=dst, args=tuple(args), fail=fail)


def rbytes(r, n, bias=0.3):
    """Random bytes, a share of them from the extremes (0, 1, 127, 128, 254, 255)."""
    return bytes(r.choice(EXT) if r.random() < bias else r.randrange(256) for _ in range(n))


def walk(start, w, h, advance, skip):
    """The byte range [lo, hi) a destination pointer touches over h rows of w pixels (None if none)."""
    lo, hi, p = None, None, start
    for _ in range(h):
        for _ in range(w):
            lo = p if lo is None else min(lo, p)
            hi = p + advance if hi is None else max(hi, p + advance)
            p += advance
        p += skip
    return lo, hi


def make_dest(r, w, h, advance, dw, dh, dbpp, x=0, y=0, fill=None):
    """A destination bitmap for a w x h source, or None when its walk would start before the buffer."""
    start = dbpp * (x + dw * y)
    lo, hi = walk(start, w, h, advance, dbpp * (dw - w))
    if lo is not None and lo < 0:
        return None
    size = (hi if hi is not None else start) + 8
    data = rbytes(r, size) if fill is None else bytes([fill]) * size
    return bm(dw, dh, dbpp, data)


def random_dest(r, w, h, advance, x=0, y=0):
    """A random destination: usually 4 bytes deep and wide enough, sometimes odd depths and narrower than w."""
    for _ in range(30):
        dbpp = r.choice((4, 4, 4, 4, 1, 2, 3, 5))
        dw = r.randrange(1, w + 6) if r.random() < 0.3 else w + x + r.randrange(0, 4)
        dh = h + y + r.randrange(0, 3)
        dest = make_dest(r, w, h, advance, dw, dh, dbpp, x, y)
        if dest is not None:
            return dest
    return make_dest(r, w, h, advance, w + x, h + y, 4, x, y)


def src_len(op, w, h, bpp):
    """What the method reads from its own buffer."""
    if op == OP_ALPHA:
        return w * h * (1 if bpp == 1 else 2 if bpp == 2 else 4)
    if op in (OP_SHADOW, OP_COLOUR):
        return w * h
    if op == OP_SWIZZLE:
        return w * h * 4
    return w * h * bpp


def sizes(r, small=True):
    w = r.choice((0, 1, 1, 2, 3, 4, 5, 8, 9)) if small else r.randrange(1, 12)
    h = r.choice((0, 1, 1, 2, 3, 4, 5, 8)) if small else r.randrange(1, 12)
    if r.random() < 0.15:
        w = 0 if r.random() < 0.5 else w
        h = 0 if w else h
    return w, h


# ----------------------------------------------------------------- the two sides


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return T.s32(h)


class PpcHeap:
    """The PowerPC side's new[] and delete[] for one case (the events tests/unit_Bitmap.cpp records)."""

    def __init__(self, fail_at):
        self.events, self.blocks, self.calls, self.fail_at = [], [], 0, fail_at

    def new(self, e):
        size = e.r(3)
        self.calls += 1
        self.events.append((1, size, 0))
        if self.calls == self.fail_at:
            e.set_r(3, 0)
            return
        a = e.alloc(max(size, 4), b"\xc3" * max(size, 4))
        self.blocks.append([a, size])
        e.set_r(3, a)

    def delete(self, e):
        p = e.r(3)
        if p == 0:
            return
        for i, block in enumerate(self.blocks):
            if block[0] == p:
                self.events.append((2, i + 1, fnv(e.read(p, block[1]))))
                block[0] = None
                break


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.bm_heap = PpcHeap(0)
    e.stub("__nwa__FUl", lambda em: em.bm_heap.new(em))
    e.stub("__dla__FPv", lambda em: em.bm_heap.delete(em))
    lib = runner.lib
    lib.tpw_bm_run.restype = C.c_int32
    lib.tpw_bm_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_bm_reset.argtypes = [C.c_int32]


def ppc_make(e, b):
    """(object address, region address, region length) of a bitmap in the emulator's memory."""
    if b is None:
        return 0, 0, 0
    if b["data"] is None:
        base, length, data = 0, 0, 0
    else:
        length = len(b["data"]) + 2 * GUARD
        base = e.alloc(length, bytes([FILL_GUARD]) * GUARD + b["data"] + bytes([FILL_GUARD]) * GUARD)
        data = base + GUARD
    obj = e.alloc(16, struct.pack(">IIII", b["w"], b["h"], b["bpp"], data))
    return obj, base, length


def ppc_run(e, c):
    e.bm_heap = heap = PpcHeap(c["fail"])
    s_obj, s_base, s_len = ppc_make(e, c["src"])
    d_obj, d_base, d_len = ppc_make(e, c["dst"])
    args = [s_obj] + ([d_obj] if c["dst"] is not None else []) + [a & 0xFFFFFFFF for a in c["args"]]
    r3 = T.call_ppc(e, ADDR[c["op"]], args)[0]
    return dict(src=e.read(s_base, s_len) if s_len else None, dst=e.read(d_base, d_len) if d_len else None,
                ret=r3 if c["op"] == OP_DESCIMATE else 0, heap=heap.events)


def host_run(lib, c):
    def make(b):
        if b is None:
            return None, None, None
        desc = (C.c_uint32 * 3)(b["w"], b["h"], b["bpp"])
        if b["data"] is None:
            return desc, None, None
        region = bytes([FILL_GUARD]) * GUARD + b["data"] + bytes([FILL_GUARD]) * GUARD
        buf = (C.c_uint8 * len(region)).from_buffer_copy(region)
        return desc, buf, C.addressof(buf) + GUARD

    s_desc, s_buf, s_data = make(c["src"])
    d_desc, d_buf, d_data = make(c["dst"])
    args = (C.c_uint32 * 4)(*[a & 0xFFFFFFFF for a in c["args"]], *([0] * (4 - len(c["args"]))))
    lib.tpw_bm_reset(c["fail"])
    ret = lib.tpw_bm_run(c["op"], C.cast(s_desc, C.c_void_p) if s_desc else None, s_data,
                         C.cast(d_desc, C.c_void_p) if d_desc else None, d_data, C.cast(args, C.c_void_p))
    n = C.c_int32.in_dll(lib, "tpw_bm_nevents").value
    table = (C.c_int32 * 3 * 64).in_dll(lib, "tpw_bm_events")
    heap = [tuple(table[i]) for i in range(n)]
    return dict(src=bytes(s_buf) if s_buf is not None else None, dst=bytes(d_buf) if d_buf is not None else None,
                ret=ret if c["op"] == OP_DESCIMATE else 0, heap=heap)


# ----------------------------------------------------------------- generators per function


def edges_fill():
    out = []
    r = random.Random(8301)
    for bpp in (0, 1, 2, 3, 4, 5, 8):
        for w, h in ((1, 1), (3, 2), (5, 1), (0, 4), (4, 0)):
            n = w * h * bpp
            out.append(case(OP_FILL, bm(w, h, bpp, rbytes(r, n)), args=(1, 2, 3, 4)))
    for colours in ((0, 0, 0, 0), (255, 255, 255, 255), (255, 0, 0, 0), (0, 255, 0, 0), (0, 0, 255, 0), (0, 0, 0, 255),
                    (17, 34, 51, 68), (128, 127, 129, 1)):
        for bpp in (1, 4):
            out.append(case(OP_FILL, bm(2, 3, bpp, rbytes(r, 6 * bpp)), args=colours))
    for bpp in (1, 4):
        out.append(case(OP_FILL, bm(3, 3, bpp, None), args=(9, 8, 7, 6)))
    # a product of 2^31 or more is a negative count (cmpw): nothing is written; 2^32 wraps to 0
    for w, h in ((0x10000, 0x8000), (0x80000000, 1), (1, 0x80000000), (0x10000, 0x10000), (0xFFFFFFFF, 1)):
        out.append(case(OP_FILL, bm(w, h, 4, rbytes(r, 8)), args=(9, 8, 7, 6)))
    return out


def random_fill(r):
    w, h = sizes(r)
    bpp = r.choice((0, 1, 2, 3, 4, 4, 4, 1, 5))
    data = None if r.random() < 0.05 else rbytes(r, w * h * bpp)
    return case(OP_FILL, bm(w, h, bpp, data), args=[r.choice(EXT) if r.random() < 0.4 else r.randrange(256) for _ in range(4)])


def copy_case(r, w, h, bpp, extra=0, nodata=False):
    n = w * h * bpp
    src = bm(w, h, bpp, None if nodata else rbytes(r, n))
    dst_dims = (w, h, bpp) if r.random() < 0.8 else (r.randrange(1, 6), r.randrange(1, 6), r.randrange(1, 5))
    return case(OP_COPY, src, bm(*dst_dims, rbytes(r, max(n, 0) + extra, 0.0)))


def edges_copy():
    r = random.Random(8302)
    out = []
    for bpp in (0, 1, 2, 3, 4, 5):
        for w, h in ((1, 1), (3, 2), (5, 1), (0, 3), (3, 0), (7, 5)):
            out.append(copy_case(r, w, h, bpp, extra=r.choice((0, 4))))
    out.append(copy_case(r, 4, 4, 4, extra=0, nodata=True))
    out.append(copy_case(r, 2, 2, 1, extra=0, nodata=True))
    out.append(copy_case(r, 6, 3, 4, extra=40))      # a larger destination: only the first bytes change
    return out


def random_copy(r):
    w, h = sizes(r)
    return copy_case(r, w, h, r.choice((0, 1, 2, 3, 4, 4, 5)), extra=r.choice((0, 0, 3, 16)), nodata=r.random() < 0.05)


def edges_swizzle():
    r = random.Random(8303)
    out = []
    for w, h in ((1, 1), (2, 1), (1, 2), (3, 3), (5, 2), (0, 5), (5, 0)):
        for bpp in (4, 1, 2):
            out.append(case(OP_SWIZZLE, bm(w, h, bpp, rbytes(r, w * h * 4))))
    out.append(case(OP_SWIZZLE, bm(1, 1, 4, bytes([1, 2, 3, 4]))))
    for w, h in ((0x10000, 0x8000), (0x80000000, 1), (0x10000, 0x10000)):
        out.append(case(OP_SWIZZLE, bm(w, h, 4, rbytes(r, 8))))
    return out


def random_swizzle(r):
    w, h = sizes(r)
    return case(OP_SWIZZLE, bm(w, h, r.choice((1, 2, 3, 4, 4)), rbytes(r, w * h * 4)))


def sweep_alphablt():
    """One pixel, every depth, alpha x destination byte over the extremes."""
    r = random.Random(8304)
    out = []
    for bpp in (1, 2, 3, 4):
        for a in EXT:
            for d in EXT:
                n = src_len(OP_ALPHA, 1, 1, bpp)
                src = bytes([a] + [EXT[(a + 3 * i + 1) % 8] for i in range(1, n)])[:n]
                dst = bm(1, 1, 4, bytes([d, EXT[(d + 2) % 8], EXT[(d + 5) % 8], EXT[(d + 7) % 8]]) + rbytes(r, 8))
                out.append(case(OP_ALPHA, bm(1, 1, bpp, src), dst, (0, 0)))
    return out


def alphablt_case(r, w, h, bpp, x=None, y=None, dest=None):
    x = r.randrange(0, 4) if x is None else x
    y = r.randrange(0, 3) if y is None else y
    src = bm(w, h, bpp, rbytes(r, src_len(OP_ALPHA, w, h, bpp)))
    return case(OP_ALPHA, src, dest or random_dest(r, w, h, 4, x, y), (x, y))


def edges_alphablt():
    r = random.Random(8305)
    out = sweep_alphablt()
    for bpp in (0, 1, 2, 3, 4, 5, 8):
        for w, h in ((1, 1), (3, 2), (4, 1), (0, 3), (3, 0)):
            out.append(alphablt_case(r, w, h, bpp, 0, 0))
            out.append(alphablt_case(r, w, h, bpp, 2, 1))
    # a destination of exactly the source's width, a wider one, a narrower one (negative skip), with offsets
    for bpp in (1, 2, 4):
        for dw in (2, 3, 4, 7):
            dest = make_dest(r, 3, 2, 4, dw, 4, 4, 1, 1)
            if dest is not None:
                out.append(case(OP_ALPHA, bm(3, 2, bpp, rbytes(r, src_len(OP_ALPHA, 3, 2, bpp))), dest, (1, 1)))
    # a destination depth other than 4 changes the row skip and the start, not the 4-byte step
    for dbpp in (1, 2, 3, 5):
        dest = make_dest(r, 3, 2, 4, 5, 3, dbpp, 1, 1)
        out.append(case(OP_ALPHA, bm(3, 2, 4, rbytes(r, 24)), dest, (1, 1)))
    # a width or height of 2^31 or more is negative to the signed loop compares: nothing is drawn
    for w, h in ((0x80000000, 1), (1, 0x80000000), (0xFFFFFFFF, 2)):
        out.append(case(OP_ALPHA, bm(w, h, 4, rbytes(r, 8)), bm(2, 2, 4, rbytes(r, 24)), (0, 0)))
    # the start offset depth * (x + width * y) wraps at 32 bits (mullw): a destination 0x10000 wide and a y of 0x10000 start at x
    for bpp in (1, 2, 4):
        out.append(case(OP_ALPHA, bm(2, 1, bpp, rbytes(r, src_len(OP_ALPHA, 2, 1, bpp))),
                        bm(0x10000, 1, 4, rbytes(r, 4 * 0x10000)), (1, 0x10000)))
    return out


def random_alphablt(r):
    w, h = sizes(r)
    return alphablt_case(r, w, h, r.choice((1, 2, 3, 4, 4, 4, 0, 5)))


def mask_case(r, op, w, h, args=(), bpp=1):
    src = bm(w, h, bpp, rbytes(r, w * h))
    return case(op, src, random_dest(r, w, h, 4 if op == OP_COLOUR else 1), args)


def edges_shadow():
    r = random.Random(8306)
    out = []
    for bpp in (1, 4, 0, 2):
        for w, h in ((1, 1), (3, 2), (5, 1), (0, 3), (3, 0), (6, 4)):
            out.append(mask_case(r, OP_SHADOW, w, h, bpp=bpp))
    for a in EXT:
        for d in EXT:
            out.append(case(OP_SHADOW, bm(1, 1, 1, bytes([a])), bm(1, 1, 1, bytes([d]) + rbytes(r, 8))))
    for dw in (2, 3, 4, 6):
        dest = make_dest(r, 3, 2, 1, dw, 3, 1)
        out.append(case(OP_SHADOW, bm(3, 2, 1, rbytes(r, 6)), dest))
    for dbpp in (2, 3, 4):
        out.append(case(OP_SHADOW, bm(3, 2, 1, rbytes(r, 6)), make_dest(r, 3, 2, 1, 5, 3, dbpp)))
    out.append(case(OP_SHADOW, bm(3, 2, 1, None), bm(3, 2, 1, rbytes(r, 10))))
    for w, h in ((0x80000000, 1), (1, 0x80000000), (0xFFFFFFFF, 2)):
        out.append(case(OP_SHADOW, bm(w, h, 1, rbytes(r, 8)), bm(2, 2, 1, rbytes(r, 8))))
    return out


def random_shadow(r):
    w, h = sizes(r)
    c = mask_case(r, OP_SHADOW, w, h, bpp=r.choice((1, 1, 1, 2, 4)))
    if r.random() < 0.04:
        c["src"]["data"] = None
    return c


def edges_colour():
    r = random.Random(8307)
    out = []
    # one alpha x mask byte x destination at a time, over the extremes
    for alpha in EXT:
        for k, mask in enumerate(EXT):
            for d in (0, 128, 255):
                colours = tuple(EXT[(alpha + 3 * k + 2 * i) % 8] for i in range(3))
                dest = bm(1, 1, 4, bytes([d, EXT[(d + 1) % 8], EXT[(d + 4) % 8], 255 - d]) + rbytes(r, 8))
                out.append(case(OP_COLOUR, bm(1, 1, 1, bytes([mask])), dest, (alpha,) + colours))
    for alpha, colours in ((255, (255, 255, 255)), (255, (0, 0, 0)), (0, (255, 255, 255)), (128, (128, 128, 128)),
                           (1, (255, 255, 255)), (200, (10, 20, 30))):
        for w, h in ((1, 1), (3, 2), (5, 1), (0, 3), (3, 0)):
            out.append(mask_case(r, OP_COLOUR, w, h, (alpha,) + colours))
    for dw in (2, 3, 4, 6):
        dest = make_dest(r, 3, 2, 4, dw, 3, 4)
        out.append(case(OP_COLOUR, bm(3, 2, 1, rbytes(r, 6)), dest, (200, 100, 150, 250)))
    for dbpp in (1, 2, 3, 5):
        out.append(case(OP_COLOUR, bm(3, 2, 1, rbytes(r, 6)), make_dest(r, 3, 2, 4, 5, 3, dbpp), (200, 100, 150, 250)))
    out.append(case(OP_COLOUR, bm(3, 2, 1, None), bm(3, 2, 4, rbytes(r, 30)), (200, 100, 150, 250)))
    for w, h in ((0x80000000, 1), (1, 0x80000000), (0xFFFFFFFF, 2)):
        out.append(case(OP_COLOUR, bm(w, h, 1, rbytes(r, 8)), bm(2, 2, 4, rbytes(r, 24)), (200, 100, 150, 250)))
    return out


def random_colour(r):
    w, h = sizes(r)
    colours = tuple(r.choice(EXT) if r.random() < 0.3 else r.randrange(256) for _ in range(4))
    c = mask_case(r, OP_COLOUR, w, h, colours, bpp=r.choice((1, 1, 1, 2, 4)))
    if r.random() < 0.04:
        c["src"]["data"] = None
    return c


DESCIMATE_FILLS = ("random", "ff", "zero", "ramp", "edges")


def pixels(r, n, kind):
    if kind == "ff":
        return b"\xff" * (4 * n)
    if kind == "zero":
        return bytes(4 * n)
    if kind == "ramp":
        return bytes((i * 7 + (i >> 2)) & 0xFF for i in range(4 * n))
    if kind == "edges":
        return rbytes(r, 4 * n, 0.9)
    return rbytes(r, 4 * n, 0.1)


def descimate_case(r, sw, sh, dw, dh, kind="random", fail=0):
    src = bm(sw, sh, 4, pixels(r, sw * sh, kind))
    dst = bm(dw, dh, 4, rbytes(r, dw * dh * 4, 0.0))
    return case(OP_DESCIMATE, src, dst, (), fail)


def edges_descimate():
    r = random.Random(8309)
    out = []
    ratios = [(4, 4, 2, 2), (8, 8, 1, 1), (16, 16, 2, 2), (2, 2, 1, 1), (6, 4, 3, 2), (4, 6, 2, 3), (3, 3, 3, 3),
              (1, 1, 1, 1), (5, 5, 2, 2), (7, 10, 3, 4), (10, 7, 4, 3), (13, 13, 6, 5), (20, 20, 3, 3), (64, 16, 8, 4),
              (40, 40, 4, 4), (5, 40, 5, 3), (7, 3, 7, 1), (3, 7, 1, 7), (9, 2, 4, 2), (2, 9, 2, 4), (11, 11, 10, 10)]
    for sw, sh, dw, dh in ratios:
        out.append(descimate_case(r, sw, sh, dw, dh))
    # column counts and row counts around the eight-wide loop: 7, 8, 9, 15, 16, 17, 24, 25, 33
    for n in (7, 8, 9, 15, 16, 17, 24, 25, 33):
        out.append(descimate_case(r, n, 1, 1, 1))
        out.append(descimate_case(r, 1, n, 1, 1))
        out.append(descimate_case(r, n, n, 1, 1))
        out.append(descimate_case(r, 2 * n, 2, 2, 1))
        out.append(descimate_case(r, 2, 2 * n, 1, 2))
    for kind in DESCIMATE_FILLS:
        out.append(descimate_case(r, 12, 9, 4, 3, kind))
        out.append(descimate_case(r, 17, 17, 1, 1, kind))
    # counts of 255 (the largest sum of a byte-wide count) and counts that do not fit a byte (300 becomes 44)
    out.append(descimate_case(r, 255, 1, 1, 1, "ff"))
    out.append(descimate_case(r, 1, 255, 1, 1, "ff"))
    out.append(descimate_case(r, 3, 255, 1, 1, "ff"))
    out.append(descimate_case(r, 300, 1, 1, 1))
    out.append(descimate_case(r, 1, 300, 1, 1))
    out.append(descimate_case(r, 257, 1, 1, 1))
    out.append(descimate_case(r, 1, 257, 1, 1))
    # the 16.16 rounding constant (0x8000): the quotient (size * 0x10000 + 0x8000) / dest size changes with a constant
    # of 0x8001 only when dest size * dest size is at least about 180 (the rounding error must reach the step count) and the running position then crosses a whole
    # pixel one step early, so the smallest such sizes (found by search) are tried: 231 to 195, 296 to 221,
    # in height, in width and in both
    for n, d in ((231, 195), (296, 221), (401, 227)):
        out.append(descimate_case(r, 1, n, 1, d))
        out.append(descimate_case(r, n, 1, d, 1))
    out.append(descimate_case(r, 231, 231, 195, 195))
    # allocation failures: the first (work buffer) answers 0 at once; the second or third leaves the others leaked
    for sw, sh, dw, dh in ((4, 4, 2, 2), (7, 5, 3, 2)):
        for fail in (1, 2, 3):
            out.append(descimate_case(r, sw, sh, dw, dh, fail=fail))
    return out


def random_descimate(r):
    sw, sh = r.randrange(1, 49), r.randrange(1, 49)
    if r.random() < 0.3:
        dw, dh = r.randrange(1, min(sw, 6) + 1), r.randrange(1, min(sh, 6) + 1)
    else:
        dw, dh = r.randrange(1, min(sw, 14) + 1), r.randrange(1, min(sh, 14) + 1)
    return descimate_case(r, sw, sh, dw, dh, r.choice(DESCIMATE_FILLS), 0 if r.random() < 0.95 else r.randrange(1, 4))


def make_test(op, edges, rnd):
    return FnTest("%s 0x%08x" % (NAME[op], ADDR[op]), ppc_run, host_run, edge=edges, rnd=rnd)


TESTS = [
    make_test(OP_FILL, edges_fill, random_fill),
    make_test(OP_COPY, edges_copy, random_copy),
    make_test(OP_SWIZZLE, edges_swizzle, random_swizzle),
    make_test(OP_ALPHA, edges_alphablt, random_alphablt),
    make_test(OP_SHADOW, edges_shadow, random_shadow),
    make_test(OP_COLOUR, edges_colour, random_colour),
    make_test(OP_DESCIMATE, edges_descimate, random_descimate),
]
