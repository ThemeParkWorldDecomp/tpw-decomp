"""The clipper's edge functions (engine library, tb_clipping_edges.cpp): the six VertexClip_*
functions ClipPoly calls through its table, and InterpolateColour.

A case is two vertices (the one outside the plane and the one inside), a stride, and a buffer
for the vertex the clipper makes, with 16 bytes of guard words after it. The test compares the
new vertex word by word (NaNs in float places as one value) and the five working globals the six
functions share (two integers and three doubles). Nothing outside the unit is called, so there
are no fakes: the original's own InterpolateColour and double-to-unsigned helper run in the
emulator.
"""
import ctypes as C
import math
import random
import struct

import tpwtest as T
from tpwtest import FnTest
from test_Clipping import SPECIAL, NAN_WORDS, fb, moderate, negate, framed_vertex, special_word

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_ClippingEdges"

# The six clippers in table order (planes 0 to 5), and InterpolateColour.
CLIPPERS = [0x1003F8B0, 0x1003FB68, 0x1003FD94, 0x1003FFC8, 0x100401F4, 0x10040424]
NAMES = ["Bottom", "Top", "Right", "Left", "Back", "Front"]
COLOUR = 0x1003FAE4
# The five globals the clippers share: two integers, then three doubles.
STEP, BYTES_LEFT = 0x102B3618, 0x102B361C
FRACTION, INSIDE_DISTANCE, OUTSIDE_DISTANCE = 0x102B3620, 0x102B3628, 0x102B3630
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5
GUARD_WORDS = 4


def setup(runner):
    T.install_std_stubs(runner.emu)


def canon(word):
    """A NaN (as float bits) as one value."""
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF):
        return 0x7FC00000
    return word


def vertex_words_canon(words, stride):
    """The new vertex's words, NaNs in float places (x y z w and the words from 0x18 on) as one value."""
    out = []
    for i, word in enumerate(words):
        is_float = i < 4 or 6 <= i < stride // 4
        out.append(canon(word) if is_float else word)
    return out


def bytes_of(words, big):
    return struct.pack((">" if big else "<") + "%dI" % len(words), *words)


def result_of(words, stride, ints, doubles):
    return dict(vertex=vertex_words_canon(words, stride), step=ints[0], bytes_left=ints[1],
                fraction=doubles[0], inside_distance=doubles[1], outside_distance=doubles[2])


# ----------------------------------------------------------------- the six clippers


def clip_ppc(e, case):
    stride = case["stride"]
    out = e.alloc(stride + 4 * GUARD_WORDS, bytes_of([FILL] * (stride // 4 + GUARD_WORDS), True))
    outside = e.alloc(stride, bytes_of(case["outside"], True))
    inside = e.alloc(stride, bytes_of(case["inside"], True))
    T.call_ppc(e, CLIPPERS[case["which"]], [out, outside, inside, stride])
    words = list(struct.unpack(">%dI" % (stride // 4 + GUARD_WORDS), e.read(out, stride + 4 * GUARD_WORDS)))
    ints = struct.unpack(">ii", e.read(STEP, 8))
    doubles = [struct.unpack(">d", e.read(address, 8))[0] for address in (FRACTION, INSIDE_DISTANCE, OUTSIDE_DISTANCE)]
    result = result_of(words, stride, ints, doubles)
    # the inputs are only read
    result["outside"] = list(struct.unpack(">%dI" % (stride // 4), e.read(outside, stride)))
    result["inside"] = list(struct.unpack(">%dI" % (stride // 4), e.read(inside, stride)))
    return result


def clip_host(lib, case):
    stride = case["stride"]
    count = stride // 4
    out = (C.c_uint32 * (count + GUARD_WORDS))(*([FILL] * (count + GUARD_WORDS)))
    outside = (C.c_uint32 * count)(*case["outside"])
    inside = (C.c_uint32 * count)(*case["inside"])
    fn = lib["tpw_ce_clip"]
    fn.restype = None
    fn.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
    fn(case["which"], C.addressof(out), C.addressof(outside), C.addressof(inside), stride)
    statics = lib["tpw_ce_statics"]
    statics.restype = None
    statics.argtypes = [C.c_void_p, C.c_void_p]
    ints = (C.c_int32 * 2)()
    doubles = (C.c_double * 3)()
    statics(C.addressof(ints), C.addressof(doubles))
    result = result_of(list(out), stride, list(ints), list(doubles))
    result["outside"] = list(outside)
    result["inside"] = list(inside)
    return result


def distance(which, vertex):
    """The plane distance of a vertex the way the clippers compute it (a single-precision sum)."""
    x, y, z, w = [T.bits_f32(word) for word in vertex[:4]]
    if which == 0:
        return T.f32(-y + w)
    if which == 1:
        return T.f32(y + w)
    if which == 2:
        return T.f32(-x + w)
    if which == 3:
        return T.f32(x + w)
    if which == 4:
        return T.f32(-z + w)
    return z


def make_vertex(r, stride, four=None, colour=None, specular=None):
    """A vertex of `stride` bytes: four coordinates, the colours, texture coordinates or other words."""
    four = four if four is not None else framed_vertex(r)
    colour = colour if colour is not None else r.getrandbits(32)
    specular = specular if specular is not None else r.getrandbits(32)
    tail = [fb(T.f32(r.uniform(-4.0, 4.0))) if r.random() < 0.9 else special_word(r) for _ in range(stride // 4 - 6)]
    return list(four) + [colour, specular] + tail


def real_pair(r, which, stride, tries=200):
    """The outside and the inside vertex as ClipPoly would pass them: distances below zero and not."""
    for _ in range(tries):
        outside = framed_vertex(r)
        inside = framed_vertex(r)
        if distance(which, outside) < 0.0 <= distance(which, inside):
            return outside, inside
    return outside, inside


def clip_case(r, which, stride=0x20, kind="real", colours="differ", speculars="differ"):
    if kind == "real":
        four_out, four_in = real_pair(r, which, stride)
    elif kind == "any":
        four_out, four_in = framed_vertex(r), framed_vertex(r)
    elif kind == "special":
        four_out = [special_word(r) for _ in range(4)]
        four_in = [special_word(r) for _ in range(4)]
    else:
        four_out = [r.getrandbits(32) for _ in range(4)]
        four_in = [r.getrandbits(32) for _ in range(4)]
    outside = make_vertex(r, stride, four_out)
    inside = make_vertex(r, stride, four_in)
    if colours == "same":
        inside[4] = outside[4]
    if speculars == "same":
        inside[5] = outside[5]
    elif speculars == "low":
        # only the low byte differs; the inside one is the larger or the smaller
        inside[5] = (outside[5] & 0xFFFFFF00) | r.choice([0, 1, 0x7F, 0x80, 0xFF, r.getrandbits(8)])
    return dict(which=which, stride=stride, outside=outside, inside=inside)


# Distances and coordinates for which the blend of x (or y) rounds differently fused and unfused
# (found by a search over random floats, 1 in about 10^8): inside distance, outside distance, the
# blended coordinate of the inside vertex and of the outside vertex, as float bits.
FUSED_BLENDS = [(0x3F115790, 0xC077436F, 0x40315D7A, 0xC07AAC52), (0x3F4FF3B0, 0xBFCC7D4D, 0x3EA30F70, 0xBF7B3428),
                (0x3FAA0ECC, 0xBFC1932F, 0xC036395A, 0x4062DF5C), (0x3F966E8A, 0xBF856435, 0x4025C078, 0xC019E06A)]


def fused_blend_case(r, which, blend):
    """A case whose plane distances are the first two words (the plane's coordinate carries them, w is
    zero) and whose blended coordinate takes the last two."""
    inside_distance, outside_distance, inside_value, outside_value = blend
    case = clip_case(r, which, 0x20)
    plane_index = (1, 1, 0, 0, 2, 2)[which]
    blended_index = 1 if which in (2, 3) else 0
    negative = which in (0, 2, 4)
    for vertex, distance_bits, value in ((case["inside"], inside_distance, inside_value),
                                         (case["outside"], outside_distance, outside_value)):
        vertex[3] = 0
        vertex[plane_index] = negate(distance_bits) if negative else distance_bits
        vertex[blended_index] = value
    return case


def clip_edges(which):
    r = random.Random(401 + which)
    # every stride the engine uses, and a longer one
    for stride in (0x18, 0x20, 0x28, 0x30, 0x38):
        yield clip_case(r, which, stride)
    for colours in ("same", "differ"):
        for speculars in ("same", "differ", "low"):
            yield clip_case(r, which, 0x20, colours=colours, speculars=speculars)
    for kind in ("any", "special", "random"):
        for _ in range(4):
            yield clip_case(r, which, 0x20, kind)
    # The fraction at its extremes: the plane's coordinate carries the distance (w is zero).
    index = (1, 1, 0, 0, 2, 2)[which]
    pairs = [(-1.0, 0.0), (-1e-30, 1.0), (-1.0, -1.0), (1.0, 1.0), (-1.0, 1e30), (-3e38, 3e38), (0.0, 0.0), (-0.0, 0.0),
             (-math.inf, 1.0), (-1.0, math.inf), (math.inf, math.inf), (1.0, -1.0), (-2.0, 1.0), (-1.0, 2.0)]
    for distance_out, distance_in in pairs + [(None, 1.0), (-1.0, None)]:
        case = clip_case(r, which, 0x20)
        for vertex, value in ((case["outside"], distance_out), (case["inside"], distance_in)):
            vertex[3] = fb(0.0)
            if value is None:
                vertex[index] = 0x7FC00000
            elif which in (0, 2, 4):
                vertex[index] = negate(fb(value))
            else:
                vertex[index] = fb(value)
        yield case
    for blend in FUSED_BLENDS:
        yield fused_blend_case(r, which, blend)
    # colours and specular colours: extreme values, swapped
    for a, b in ((0, 0xFFFFFFFF), (0xFFFFFFFF, 0), (0x00FF00FF, 0xFF00FF00), (0x12345678, 0x9ABCDEF0), (5, 5)):
        case = clip_case(r, which, 0x20)
        case["outside"][4], case["inside"][4] = a, b
        case["outside"][5], case["inside"][5] = b, a
        yield case


def clip_random(r, which):
    stride = r.choice([0x20, 0x20, 0x20, 0x18, 0x28, 0x30, 0x38])
    kind = r.choice(["real", "real", "real", "any", "special", "random"])
    return clip_case(r, which, stride, kind, colours=r.choice(["same", "differ", "differ"]),
                     speculars=r.choice(["same", "differ", "low", "low"]))


# ----------------------------------------------------------------- InterpolateColour


def colour_ppc(e, case):
    r3 = T.call_ppc(e, COLOUR, [case["from"], case["to"], case["t"]])[0]
    return dict(result=r3)


def colour_host(lib, case):
    fn = lib["tpw_ce_colour"]
    fn.restype = C.c_uint32
    fn.argtypes = [C.c_uint32, C.c_uint32, C.c_uint32]
    return dict(result=fn(case["from"], case["to"], case["t"]))


def colour_edges():
    values = (0, 1, 0xFF, 0x100, 0x00FF00FF, 0xFF00FF00, 0xFFFFFFFF, 0x80808080, 0x7F7F7F7F, 0x12345678)
    for a in values:
        for b in values:
            for t in (0, 1, 0x7F, 0x80, 0xFF, 0x100, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                yield {"from": a, "to": b, "t": t}


def colour_random(r):
    t = r.choice([r.getrandbits(8), r.randrange(256), r.getrandbits(32), 255, 254])
    return {"from": r.getrandbits(32), "to": r.getrandbits(32), "t": t}


def clipper_test(which):
    return FnTest("VertexClip_" + NAMES[which], clip_ppc, clip_host, edge=lambda: clip_edges(which),
                  rnd=lambda r: clip_random(r, which))


TESTS = [clipper_test(which) for which in range(6)]
TESTS.append(FnTest("InterpolateColour", colour_ppc, colour_host, edge=colour_edges, rnd=colour_random))
