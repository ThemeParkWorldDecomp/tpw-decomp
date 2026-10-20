"""Unit w35b, the Bullfrog half: the rectangle, viewport, clip and palette leaves, against libtpw_unit_LbDrawLeaves.dylib
(tests/unit_LbDrawLeaves.cpp), all in the bullfrog library ("bullfrog symbols shared"):
  src/bfl_lbdraw_leaves.cpp   LbRect_Intersection 0x10029ad0, LbRect_Empty 0x10029cd4, LbRect_Normalise 0x10029d14,
                              LbDraw_SetViewPort 0x1002b1c0, LbDraw_SetClipRect 0x1002b2c4, LbDraw_SetCurrent 0x1002b588 (the
                              TbRenderArea overload), LbPalette_FindColour 0x1001a5c8

Three groups of tests:

1. The three rectangle helpers on rectangles of every shape (empty, inverted, disjoint, touching, nested, the extremes of the
   32-bit range): the rectangle(s) afterwards and the answer.
2. LbPalette_FindColour over palettes of 256 four-byte entries: random ones, ones made of few colours (so that equally near
   entries are common and the Manhattan and the weighted tie-breaks decide), flat and empty ones, grey ramps, the colour map of the
   game's one colour-mapped picture (Cbus.tga, read from ROOT/extracted when the test runs, never copied) and the colours of a real
   24-bit cursor; exact hits at every position and with duplicates.
3. The drawing state as a script. A world of six render areas (every depth the library tells apart and several it does not, odd
   sizes, strides and pixel pointers near the top of the 32-bit range) and a list of operations: set the current area (or none),
   set the viewport (a rectangle or none), set the clip rectangle (a rectangle or none). After every operation the state is dumped:
   the current area, the viewport and clip rectangles, the origin pointer, the drawing context's depth, the pixel capabilities
   object, and every area's words (no function may write an area). The call log (TbPixelCaps::Set, with the labels of the capabilities
   and of the area it is given as the format) is compared at the end. The original's other callees are in the unit.
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbDrawLeaves"
MASK = 0xFFFFFFFF
INTERSECT, EMPTY, NORMALISE = 0x10029AD0, 0x10029CD4, 0x10029D14
SET_VIEWPORT, SET_CLIP, SET_CURRENT, FIND_COLOUR = 0x1002B1C0, 0x1002B2C4, 0x1002B588, 0x1001A5C8
PIXEL_CAPS_SET = 0x100162E8      # TbPixelCaps::Set, the one callee outside the unit
UNIT = [INTERSECT, EMPTY, NORMALISE, SET_VIEWPORT, SET_CLIP, SET_CURRENT, FIND_COLOUR]
# The library's state words (include/LbDraw.h): the dump reads them in one block from the capabilities object.
CAPS, BASE, ORIGIN, CLIP, VIEW, CURRENT = 0x10074838, 0x10074858, 0x1007485C, 0x10074860, 0x10074870, 0x10074880
STATE_END = 0x10074884
CONTEXTS = {0x10074824: 8, 0x10074820: 16, 0x1007481C: 24, 0x10074818: 32}
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
ROOT_DIR = pathlib.Path(__file__).resolve().parents[2]
CURSOR_DIR = ROOT_DIR / "extracted/disc-hfs/Theme Park Data/data/ui/cursors"


def s32(v):
    return T.s32(v)


def heap_ppc(e):
    """The heap traffic of the case on the PowerPC side: operator new, new[], delete and delete[] (none of these functions allocates)."""
    kinds = [k for k, _, _ in e.alloc_log]
    return [kinds.count("new"), kinds.count("new[]"), kinds.count("delete"), kinds.count("delete[]")]


def heap_host(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.stub("labs", lambda e: e.set_r(3, T.u32(abs(s32(e.r(3))))))


# ---------------------------------------------------------------- 1. the rectangle helpers

def pack_rect(r):
    return struct.pack(">4i", *r)


def rect_dict(e, a):
    return list(struct.unpack(">4i", e.read(a, 16)))


def ppc_intersection(e, case):
    ra, rb = e.alloc(16, pack_rect(case[0])), e.alloc(16, pack_rect(case[1]))
    r3, _, _ = T.call_ppc(e, INTERSECT, [ra, rb])
    return {"answer": r3 & 0xFF, "rect": rect_dict(e, ra), "other": rect_dict(e, rb)}


def host_intersection(lib, case):
    ra, rb = (C.c_int32 * 4)(*case[0]), (C.c_int32 * 4)(*case[1])
    lib.tpw_lb_intersection.restype = C.c_int32
    answer = lib.tpw_lb_intersection(ra, rb)
    return {"answer": answer & 0xFF, "rect": list(ra), "other": list(rb)}


def ppc_one(addr):
    def run(e, case):
        ra = e.alloc(16, pack_rect(case))
        T.call_ppc(e, addr, [ra])
        return {"rect": rect_dict(e, ra)}
    return run


def host_one(name):
    def run(lib, case):
        ra = (C.c_int32 * 4)(*case)
        getattr(lib, name)(ra)
        return {"rect": list(ra)}
    return run


# Intervals along one axis (low, high): empty, inverted, nested, touching, single points and the ends of the 32-bit range.
INTERVALS = [(0, 4), (2, 6), (4, 8), (5, 7), (0, 0), (3, 3), (6, 2), (-2, 1), (INT_MIN, INT_MAX), (INT_MAX, INT_MIN), (-1, 0),
             (0, 1), (INT_MIN, INT_MIN), (INT_MAX, INT_MAX), (1, 0)]


def rect_edges_pair():
    fixed = [(INTERVALS[0], INTERVALS[1]), (INTERVALS[0], INTERVALS[0]), (INTERVALS[6], INTERVALS[3]), (INTERVALS[9], INTERVALS[8]),
             (INTERVALS[1], INTERVALS[2])]
    for x in INTERVALS:
        for x2 in INTERVALS:
            for y, y2 in fixed:
                yield [[x[0], y[0], x[1], y[1]], [x2[0], y2[0], x2[1], y2[1]]]
    # the same on the other axis, so that left and top, right and bottom are told apart
    for y in INTERVALS:
        for y2 in INTERVALS:
            for x, x2 in fixed[:3]:
                yield [[x[0], y[0], x[1], y[1]], [x2[0], y2[0], x2[1], y2[1]]]


def rect_edges_one():
    for x in INTERVALS:
        for y in INTERVALS:
            yield [x[0], y[0], x[1], y[1]]


def rnd_coord(r, span):
    k = r.random()
    if k < 0.6:
        return r.randint(-span, span)
    if k < 0.8:
        return r.randint(-1000, 1000)
    return r.choice([INT_MIN, INT_MAX, -1, 0, 1, INT_MIN + 1, INT_MAX - 1])


def rnd_rect(r, span=4):
    return [rnd_coord(r, span) for _ in range(4)]


# ---------------------------------------------------------------- 2. LbPalette_FindColour

def entry(r, g, b, pad=0):
    return bytes([r & 255, g & 255, b & 255, pad & 255])


def palette_of(colours, fill=(0, 0, 0)):
    """256 entries from a list of (r, g, b) triples (the rest are `fill`)."""
    colours = list(colours) + [fill] * (256 - len(colours))
    return b"".join(entry(*colours[i]) for i in range(256))


def ppc_find(e, case):
    palette, r, g, b = case
    e.alloc_log.clear()
    pa = e.alloc(1024, palette)
    r3, _, _ = T.call_ppc(e, FIND_COLOUR, [pa, T.U8(r), T.U8(g), T.U8(b)])
    return {"index": r3 & 0xFF, "heap": heap_ppc(e)}


def host_find(lib, case):
    palette, r, g, b = case
    buf = C.create_string_buffer(palette, 1024)
    lib.tpw_lb_find.restype = C.c_int32
    for i in range(4):
        (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")[i] = 0
    return {"index": lib.tpw_lb_find(buf, r, g, b) & 0xFF, "heap": heap_host(lib)}


def real_palettes():
    """The colour map of the game's one colour-mapped picture and the colours of a 24-bit cursor, as palettes."""
    out = []
    path = CURSOR_DIR / "Cbus.tga"
    if path.exists():
        data = path.read_bytes()
        idlen, maptype, imgtype = data[0], data[1], data[2]
        start, length, bits = struct.unpack("<HHB", data[3:8])
        assert maptype == 1 and bits == 24 and length == 256
        base = 18 + idlen
        colours = [(data[base + 3 * i + 2], data[base + 3 * i + 1], data[base + 3 * i]) for i in range(256)]   # file order: blue, green, red
        out.append(palette_of(colours))
    for path in sorted(CURSOR_DIR.glob("*.tga"))[:12]:
        data = path.read_bytes()
        if data[1] != 0 or data[16] != 24:
            continue
        pixels = data[18 + data[0]:]
        seen = []
        for i in range(0, min(len(pixels), 3 * 128 * 32) - 2, 3):
            triple = (pixels[i + 2], pixels[i + 1], pixels[i])
            if triple not in seen:
                seen.append(triple)
            if len(seen) == 256:
                break
        out.append(palette_of(seen))
    return out


def find_edges():
    cases = []
    grey = palette_of([(i, i, i) for i in range(256)])
    flat = palette_of([(77, 78, 79)] * 256)
    empty = bytes(1024)
    ones = bytes([255] * 1024)
    for p in (grey, flat, empty, ones):
        for c in ((0, 0, 0), (255, 255, 255), (5, 5, 5), (77, 78, 79), (128, 1, 250), (1, 2, 3)):
            cases.append((p, *c))
    # an exact hit at every one of the four positions of the unrolled loop and at the ends, with a duplicate after it
    for position in (0, 1, 2, 3, 4, 5, 6, 7, 100, 253, 254, 255):
        colours = [(i, 255 - i, (i * 7) & 255) for i in range(256)]
        colours[position] = (200, 100, 50)
        colours[(position + 9) % 256] = (200, 100, 50)
        cases.append((palette_of(colours), 200, 100, 50))
    # an exact hit that is nearer than an earlier near miss
    cases.append((palette_of([(10, 10, 10), (10, 10, 11), (11, 10, 10), (50, 50, 50)]), 50, 50, 50))
    cases.append((palette_of([(50, 50, 51), (50, 50, 50)], fill=(0, 0, 0)), 50, 50, 50))
    # one nearest entry, no exact one
    cases.append((palette_of([(10, 10, 10), (20, 20, 20), (30, 30, 30)], fill=(255, 255, 255)), 22, 19, 21))
    # equal squared distance, told apart by the sum of the differences: (5,0,0) 25/5, (3,4,0) 25/7, (0,0,5) 25/5, (4,3,0) 25/7
    cases.append((palette_of([(8, 5, 5), (6, 9, 5), (3, 5, 10), (9, 8, 5)], fill=(250, 250, 250)), 3, 5, 5))
    cases.append((palette_of([(6, 9, 5), (9, 8, 5), (3, 5, 10), (8, 5, 5)], fill=(250, 250, 250)), 3, 5, 5))
    # equal in both, told apart by the weighted sum of the entry's own squares (the entry nearer to black wins)
    cases.append((palette_of([(5, 3, 4), (3, 5, 4), (4, 5, 3), (4, 3, 5)], fill=(250, 250, 250)), 4, 4, 4))
    cases.append((palette_of([(4, 3, 5), (4, 5, 3), (3, 5, 4), (5, 3, 4)], fill=(250, 250, 250)), 4, 4, 4))
    # equal in everything: the first
    cases.append((palette_of([(5, 3, 4), (5, 3, 4), (5, 3, 4)], fill=(250, 250, 250)), 4, 4, 4))
    cases.append((palette_of([(3, 5, 4), (5, 3, 4)], fill=(250, 250, 250)), 4, 4, 4))     # same weights, first wins
    # the fourth byte of an entry is never looked at
    cases.append((bytes([5, 5, 5, 255] * 256), 5, 5, 4))
    cases.append((bytes([1, 2, 3, 0xAA, 9, 9, 9, 0x55] * 128), 9, 9, 8))
    for p in real_palettes():
        for c in ((0, 0, 0), (255, 255, 255), (255, 0, 255), (128, 128, 128), (12, 200, 99), (1, 1, 1), (254, 254, 254), (90, 40, 10)):
            cases.append((p, *c))
    return cases


def find_rnd(r):
    kind = r.random()
    if kind < 0.25:                                    # random bytes
        palette = bytes(r.getrandbits(8) for _ in range(1024))
        if r.random() < 0.5:                           # a query that is in the palette
            i = r.randrange(256)
            return [palette, palette[4 * i], palette[4 * i + 1], palette[4 * i + 2]]
        return [palette, r.getrandbits(8), r.getrandbits(8), r.getrandbits(8)]
    if kind < 0.75:                                    # few distinct colours in a small cube: many ties
        top = r.choice([2, 3, 4, 7, 15])
        colours = [(r.randint(0, top), r.randint(0, top), r.randint(0, top)) for _ in range(r.choice([8, 32, 256]))]
        query = (r.randint(0, top + 2), r.randint(0, top + 2), r.randint(0, top + 2))
        palette = palette_of(colours, fill=(r.randint(0, 255), r.randint(0, 255), r.randint(0, 255)))
        palette = bytes(b if i % 4 != 3 else r.getrandbits(8) for i, b in enumerate(palette))
        return [palette, *query]
    if kind < 0.85:                                    # a cube at the top of the range
        colours = [(255 - r.randint(0, 3), 255 - r.randint(0, 3), 255 - r.randint(0, 3)) for _ in range(256)]
        return [palette_of(colours), 255 - r.randint(0, 4), 255 - r.randint(0, 4), 255 - r.randint(0, 4)]
    colours = [(r.choice([0, 255]), r.choice([0, 255]), r.choice([0, 255])) for _ in range(256)]   # the corners
    return [palette_of(colours), r.getrandbits(8), r.getrandbits(8), r.getrandbits(8)]


# ---------------------------------------------------------------- 3. the drawing state

K_AREAS = 6
WORDS = 10
STD_MASKS = (0xFF0000, 0xFF00, 0xFF, 0xFF000000)


def area(depth=32, width=64, height=48, stride=None, pixels=0x40000000, masks=STD_MASKS, palettised=0):
    stride = width * ((depth + 7) // 8) if stride is None else stride
    return [m & MASK for m in masks] + [depth & MASK, palettised & MASK, pixels & MASK, width & MASK, height & MASK, stride & MASK]


def std_world():
    return [area(8, 64, 48, palettised=1, masks=(0, 0, 0, 0)), area(16, 100, 30, masks=(0x7C00, 0x3E0, 0x1F, 0)),
            area(24, 33, 77, masks=(0xFF0000, 0xFF00, 0xFF, 0)), area(32, 256, 192),
            area(12, 50, 50, stride=77, pixels=0xFFFFF000), area(40, 20, 20, pixels=0x10000000)]


def S(k):
    return (0, k, 0, 0, 0, 0)


def V(rect=None):
    return (1, 0, 0, 0, 0, 0) if rect is None else (1, 1, *rect)


def C_(rect=None):
    return (2, 0, 0, 0, 0, 0) if rect is None else (2, 1, *rect)


def ppc_script(e, case):
    world, ops = case["world"], case["ops"]
    n = len(world)
    e.alloc_log.clear()
    areas = e.alloc(0x28 * n)
    addrs = [areas + 0x28 * i for i in range(n)]
    e.cl = CL.PpcLog({CAPS: 1, **{a: 10 + i for i, a in enumerate(addrs)}})
    for i, w in enumerate(world):
        e.write(addrs[i], struct.pack(">IIIIIIIIII", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9]))
    e.write(CAPS, bytes(STATE_END - CAPS))      # the static initialiser's state (the contexts' vtable words are not read)
    rects = e.alloc(16 * max(len(ops), 1))

    def caps_set(em):
        L = em.cl
        fmt = em.r(4)
        words = struct.unpack(">6I", em.read(fmt, 24))
        L.rec("TbPixelCaps::Set", [L.lab(em.r(3)), L.lab(fmt), words[0], words[1], words[2], words[3], s32(words[4]), s32(words[5])])
        em.write(em.r(3), struct.pack(">8I", words[0], words[1], words[2], words[3], words[4], words[5], 0xCA95, 0))
    st = CL.Stubs()
    st.internal(PIXEL_CAPS_SET, caps_set)
    st.real = set(UNIT)
    old = st.activate(e)
    out = []
    try:
        for i, op in enumerate(ops):
            kind = op[0]
            if kind == 0:
                T.call_ppc(e, SET_CURRENT, [0 if op[1] < 0 else addrs[op[1]]])
            else:
                rect = rects + 16 * i
                e.write(rect, struct.pack(">4i", *op[2:6]))
                T.call_ppc(e, SET_VIEWPORT if kind == 1 else SET_CLIP, [rect if op[1] else 0])
            out += dump_ppc(e, addrs, n)
    finally:
        CL.Stubs.deactivate(e, old)
    return {"state": out, "log": e.cl.log, "heap": heap_ppc(e)}


def dump_ppc(e, addrs, n):
    block = e.read(CAPS, STATE_END - CAPS)
    caps = struct.unpack(">8I", block[0:32])
    base, origin = struct.unpack(">II", block[BASE - CAPS:BASE - CAPS + 8])
    clip = struct.unpack(">4i", block[CLIP - CAPS:CLIP - CAPS + 16])
    view = struct.unpack(">4i", block[VIEW - CAPS:VIEW - CAPS + 16])
    current = struct.unpack(">I", block[CURRENT - CAPS:CURRENT - CAPS + 4])[0]
    number = -1 if current == 0 else (addrs.index(current) if current in addrs else -2)
    out = [number, *view, *clip, origin, 0 if base == 0 else CONTEXTS.get(base, -2), *caps]
    for a in addrs:
        w = struct.unpack(">10I", e.read(a, 0x28))      # masks, depth, palettised, pixels, width, height, stride
        out += list(w)
    return out


def host_script(lib, case):
    world, ops = case["world"], case["ops"]
    CL.host_begin(lib, {})
    for i in range(4):
        (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")[i] = 0
    flat = [w for a in world for w in a]
    warr = (C.c_int32 * len(flat))(*[T.s32(w) for w in flat])
    oflat = [T.s32(v) for op in ops for v in op]
    oarr = (C.c_int32 * len(oflat))(*oflat)
    out = (C.c_int64 * (len(ops) * (1 + 4 + 4 + 1 + 1 + 8 + WORDS * len(world))))()
    lib.tpw_lb_run.restype = C.c_int32
    n = lib.tpw_lb_run(warr, len(world), oarr, len(ops), out)
    return {"state": list(out[:n]), "log": CL.host_end(lib), "heap": heap_host(lib)}


def script(ops, world=None):
    return {"world": world or std_world(), "ops": ops}


def edge_scripts():
    world = std_world()
    # every area made current, and made current again, with NULL between
    yield script([S(i) for i in range(K_AREAS)] + [S(-1), S(3), S(-1), S(-1), S(0), S(5), S(2)])
    # the viewport on the 32-bit area (64 x 48 is area 0, 256 x 192 is area 3)
    rects = [None, (0, 0, 256, 192), (10, 20, 100, 150), (-10, -20, 50, 60), (200, 150, 300, 250), (100, 100, 100, 100), (100, 100, 50, 50),
             (300, 300, 400, 400), (-500, -500, -400, -300), (INT_MIN, INT_MIN, INT_MAX, INT_MAX), (INT_MAX, INT_MAX, INT_MIN, INT_MIN),
             (0, 0, 0, 0), (5, 5, 6, 6), (255, 191, 256, 192), (256, 192, 257, 193)]
    for rect in rects:
        yield script([S(3), V(rect)])
        yield script([S(3), V((20, 30, 200, 160)), C_(rect)])
        yield script([S(3), V(rect), C_(None), C_(rect), V(None)])
    # the clip rectangle against viewports of every kind, on a 24-bit area
    for view in (None, (5, 5, 30, 60), (-5, -5, 10, 10), (30, 70, 33, 77), (0, 0, 0, 0), (40, 80, 10, 10)):
        for clip in (None, (0, 0, 10, 10), (-10, -10, 5, 5), (3, 3, 100, 100), (20, 20, 20, 20), (30, 30, 10, 10), (INT_MIN, INT_MIN, INT_MAX, INT_MAX),
                     (INT_MAX, 0, INT_MIN, 5), (-3, -3, -1, -1), (1000, 1000, 2000, 2000)):
            yield script([S(2), V(view), C_(clip)])
    # every depth, as the first and as a later area
    for d in (8, 9, 12, 15, 16, 17, 23, 24, 25, 31, 32, 33, 40, 7, 1, 0, -1, -8, 0x7FFFFFFF, -0x80000000, 0x80000008, 0x108):
        w = std_world()
        w[5] = area(d, 10, 10, stride=10)
        yield script([S(5), V((1, 1, 9, 9)), S(-1), S(0), S(5)], world=w)
    # areas with no pixels, no size, negative size and an enormous stride
    odd = [area(32, 0, 0), area(8, -4, 7, stride=-4, pixels=0), area(16, 5, -3, stride=10), area(24, 100000, 100000, stride=0x7FFFFFFF, pixels=0xFFFFFFF0),
           area(32, 3, 3, stride=0x80000000, pixels=0x80000000), area(32, 1, 1, stride=-1, pixels=1)]
    for k in range(6):
        yield script([S(k), V((1, 1, 2, 2)), V((-1, -1, 3, 3)), C_((0, 0, 1, 1)), V(None)], world=odd)
    # four long scripts of 200 operations
    for seed in (7, 8, 9, 10):
        yield random_script(random.Random(seed), 200)


def rand_rect(r, w):
    width, height = w[7], w[8]
    width = width if width < 0x80000000 else width - (1 << 32)
    height = height if height < 0x80000000 else height - (1 << 32)
    k = r.random()
    if k < 0.65:
        return (r.randint(-10, max(width, 0) + 10), r.randint(-10, max(height, 0) + 10), r.randint(-10, max(width, 0) + 10),
                r.randint(-10, max(height, 0) + 10))
    if k < 0.8:
        return tuple(r.randint(-300, 300) for _ in range(4))
    return tuple(r.choice([INT_MIN, INT_MAX, -1, 0, 1, INT_MIN + 1, INT_MAX - 1, 100000, -100000]) for _ in range(4))


def rand_area(r):
    depth = r.choice([8, 16, 24, 32, 32, 16, 24, 8, r.randint(-5, 50), 12, 40])
    width = r.choice([r.randint(0, 300), r.randint(-3, 8), 1, 0, 1000])
    height = r.choice([r.randint(0, 300), r.randint(-3, 8), 1, 0, 700])
    stride = r.choice([None, None, r.getrandbits(32), r.randint(-100, 1000)])
    return area(depth, width, height, stride=stride, pixels=r.choice([0x40000000, r.getrandbits(32), 0xFFFFFF00, 0x7FFFFFF0, 0x1000]),
                masks=tuple(r.getrandbits(32) for _ in range(4)), palettised=r.choice([0, 1, r.getrandbits(32)]))


def random_script(r, count):
    world = [rand_area(r) for _ in range(r.randint(1, K_AREAS))]
    ops = [S(r.randrange(len(world)))]
    current = ops[0][1]
    for _ in range(count - 1):
        k = r.random()
        if current < 0 or k < 0.2:
            current = r.choice([-1, -1] + list(range(len(world)))) if current >= 0 else r.randrange(len(world))
            ops.append(S(current))
        else:
            w = world[current]
            rect = None if r.random() < 0.25 else rand_rect(r, w)
            ops.append(V(rect) if k < 0.55 else C_(rect))
    return script(ops, world)


def rnd_script(r):
    return random_script(r, r.randint(4, 40))


TESTS = [
    FnTest("LbRect_Intersection", ppc_intersection, host_intersection, edge=lambda: list(rect_edges_pair()),
           rnd=lambda r: [rnd_rect(r), rnd_rect(r)]),
    FnTest("LbRect_Empty", ppc_one(EMPTY), host_one("tpw_lb_empty"), edge=lambda: list(rect_edges_one()), rnd=rnd_rect),
    FnTest("LbRect_Normalise", ppc_one(NORMALISE), host_one("tpw_lb_normalise"), edge=lambda: list(rect_edges_one()), rnd=rnd_rect),
    FnTest("LbPalette_FindColour", ppc_find, host_find, edge=find_edges, rnd=find_rnd),
    FnTest("LbDraw_SetCurrent, SetViewPort and SetClipRect scripts", ppc_script, host_script, edge=lambda: list(edge_scripts()),
           rnd=rnd_script),
]
