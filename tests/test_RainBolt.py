"""w67: the real midpoint, recursive subdivision and quad builder against the engine original.

RainRender supplies the checked float correction and endian helpers. Its rain builder's Mac to host
pointer expansion is copied here. Every word of the rain object is compared after each scripted call,
including all sixteen lightning lists. Random, the math virtual, TransformVertex and AddPoly are
recording fakes. sqrt is real. TransformVertex does direct matrix arithmetic in the real translation,
so this module fakes it with the same affine map as the math virtual. All vertex words are recorded.

The Mac does not initialise specular. A hook clears those four undefined stack words for each
AddLightningSection invocation, including recursive invocations; the host clears its quad array.
HardwareRounding corrects fused single instructions in precisely the three real code ranges.
"""
import contextlib
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from test_RainRender import HardwareRounding, fb, bf, canon, ws, rs, i64, little, stack_cleared

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_RainBolt"
GENERATE, SECTION, NEW = 0x10021F48, 0x1002163C, 0x100223D8
SIZES = {GENERATE: 1092, SECTION: 2240, NEW: 1324}
MATH_VAR, MATRIX_VAR, DRAW_VAR = 0x100AFE94, 0x100AFF70, 0x10052B3C
TRANSFORM_VERTEX, ADD_POLY, RANDOM = 0x1001C584, 0x100078C0, 0x1004C5E8
RAIN_BYTES, RAIN_WORDS = 0xA1CC, 0xA1CC // 4
WORDS_PER_STEP = 6 + RAIN_WORDS


def float_word(offset):
    return (0x9DE4 <= offset < 0xA164 or
            0x3008 <= offset < 0x3308 and (offset - 0x3008) % 0x30 < 0x24)


def action(a=(1, 2, 3), b=(5, 7, 11), scale=0.6, null=False, to_null=False):
    return [int(null), int(to_null)] + [fb(x) for x in a + b] + [fb(scale)]


def case(op, actions=None, depth=0, count=0, **kw):
    rain = {0x3310: fb(0.6), 0x9DA0: 2, 0xA188: depth, 0xA1A0: 0, 0xA16C: fb(1.3),
            0xA170: fb(0.125), 0xA174: fb(0.25), 0xA178: fb(0.75), 0xA17C: fb(0.875),
            0xA180: fb(1e9), 0xA184: fb(2.0), 0xA19C: 0xA1B2C3D4}
    for i in range(16):
        rain[0x9DA4 + 4 * i] = count if i == depth else 17 + i
        for j in range(4):
            rain[0x9DE4 + 16 * i + 4 * j] = fb(j + i * 0.25)
        for j in range(8):
            rain[0x9EE4 + 32 * i + 4 * j] = fb(j + i * 0.5)
        for j in range(2):
            rain[0xA0E4 + 8 * i + 4 * j] = fb((j + 1) * 0.3)
    for i, x in enumerate((0, -1, 0)):
        rain[0x3008 + 2 * 0x30 + 0x18 + 4 * i] = fb(x)
    c = dict(op=op, actions=actions or [action()], rain=rain, q={})
    c.update(kw)
    return c


def flat(c):
    return ([c["op"]] + [c["rain"].get(4 * i, 0) for i in range(RAIN_WORDS)] +
            [len(c["actions"])] + [w for a in c["actions"] for w in a])


def states(words, count):
    # Named short lists give useful field indices on a mismatch; other rain fields are exact bytes.
    out = []
    for i in range(count):
        w = words[i * WORDS_PER_STEP:(i + 1) * WORDS_PER_STEP]
        rain = w[6:]
        out.append(dict(result=w[0], point=w[1:4], random_calls=w[4], recursion_calls=w[5],
                        counts=rain[0x9DA4 // 4:0x9DE4 // 4],
                        points=rain[0x9DE4 // 4:0x9EE4 // 4],
                        edges=rain[0x9EE4 // 4:0xA0E4 // 4],
                        normals=rain[0xA0E4 // 4:0xA164 // 4],
                        depth=rain[0xA188 // 4], rain=little(rain)))
    return out


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    transform = T.fake_function(e, lambda em: em.rb_side.transform(em))
    vt = T.persist_alloc(e, 0x40)
    ws(e, vt + 0x10, [transform])
    e.rb_math = T.persist_alloc(e, 0x10)
    ws(e, e.rb_math, [0, vt])


@contextlib.contextmanager
def globals_saved(e):
    saved = [(a, e.read(a, 4)) for a in (MATH_VAR, MATRIX_VAR, DRAW_VAR)]
    try:
        yield
    finally:
        for a, b in saved:
            e.write(a, b)


class PpcSide:
    def __init__(self, e, c):
        self.e = e
        self.rain = e.alloc(RAIN_BYTES)
        self.matrix = e.alloc(64)
        self.draw = e.alloc(64)
        self.a, self.b, self.point = [e.alloc(12) for _ in range(3)]
        ws(e, self.rain, [c["rain"].get(4 * i, 0) for i in range(RAIN_WORDS)])
        ws(e, MATH_VAR, [e.rb_math])
        ws(e, MATRIX_VAR, [self.matrix])
        ws(e, DRAW_VAR, [self.draw])
        self.L = e.cl = CL.PpcLog({self.rain: 1, e.rb_math: 2, self.draw: 3, self.matrix: 4}, c["q"])
        self.random_calls = 0
        self.new_calls = 0
        e.rb_side = self

    def project(self, out, src):
        e = self.e
        w = rs(e, src, 3)
        self.L.rec("Input", tuple(canon(x) for x in w))
        x, y, z = map(bf, w)
        ws(e, out, [fb(T.f32(T.f32(x * 2.0) + 1.0)), fb(T.f32(T.f32(y * -0.5) + 2.0)),
                    fb(T.f32(z + 3.0)), fb(1.0)])

    def transform(self, e):
        matrix, out, src = e.r(4), e.r(5), e.r(6)
        so, si, count, kind = [T.s32(e.r(i)) for i in range(7, 11)]
        self.L.rec("Transform", (self.L.lab(e.r(3)), self.L.lab(matrix), int(out == src), so, si, count, kind))
        for i in range(count):
            self.project(out + i * so, src + i * si)

    def vertex(self, e):
        out, src, kind = e.r(4), e.r(5), T.s32(e.r(6))
        self.L.rec("TransformVertex", (self.L.lab(e.r(3)), int(out == src), kind))
        self.project(out, src)

    def poly(self, e):
        self.L.rec("AddPoly", (self.L.lab(e.r(3)), T.s32(e.r(5)), T.s32(e.r(6)), self.L.lab(e.r(7)), T.s32(e.r(8))))
        for i in range(T.s32(e.r(5))):
            w = rs(e, e.r(4) + i * e.r(6), 8)
            self.L.rec("Vertex", tuple(canon(x) if j in (0, 1, 2, 3, 6, 7) else x for j, x in enumerate(w)))
        e.set_r(3, T.u32(self.L.ret("AddPoly", 1)))

    def rand(self, e):
        value = T.u32(self.L.ret("Random", 511))
        self.random_calls += 1
        self.L.rec("Random", (value,))
        e.set_r(3, value)

    def assertion(self, e):
        self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4))))

    def snapshot(self, result):
        words = rs(self.e, self.rain, RAIN_WORDS)
        for i, w in enumerate(words):
            if float_word(4 * i):
                words[i] = canon(w)
        return ([result & 0xFFFFFFFF] + [canon(x) for x in rs(self.e, self.point, 3)] +
                [self.random_calls, self.new_calls] + words)


def run_ppc(e, c):
    with globals_saved(e):
        side = PpcSide(e, c)
        st = CL.Stubs()
        st.real = set(SIZES) | {0x1004C858}
        st.internal(TRANSFORM_VERTEX, side.vertex)
        st.internal(ADD_POLY, side.poly)
        # Let the glue stub save the caller's TOC before the import fake answers.
        st.imp("Random", side.rand)
        st.imp("__assertion_failed", side.assertion)
        saved = st.activate(e)
        stack_cleared(e)
        words = []

        def entry_hook(uc, address, size, user):
            if address == NEW:
                side.new_calls += 1
            else:
                # Only these undefined specular words are cleared, in each invocation's own frame.
                sp = e.r(1) - 0x3F0
                for i in range(4):
                    ws(e, sp + 0x118 + 0x20 * i + 0x14, [0])
        hooks = [e.uc.hook_add(UC_HOOK_CODE, entry_hook, begin=a, end=a) for a in (NEW, SECTION)]
        try:
            with HardwareRounding(e, [(a, a + n) for a, n in SIZES.items()]):
                for a in c["actions"]:
                    ws(e, side.a, a[2:5])
                    ws(e, side.b, a[5:8])
                    ws(e, side.point, [0xA5A5A5A5] * 3)
                    if c["op"] == 1:
                        T.call_ppc(e, GENERATE, [side.point, side.a, side.b, T.F(bf(a[8]))])
                        result = 0
                    else:
                        result = T.call_ppc(e, SECTION if c["op"] == 2 else NEW,
                                            [side.rain, 0 if a[0] else side.a, 0 if a[1] else side.b])[0]
                    words += side.snapshot(result)
        finally:
            for h in hooks:
                e.uc.hook_del(h)
            CL.Stubs.deactivate(e, saved)
            e.rb_side = None
        # Host instrumentation and the entry hook count recursion independently; Random logs pin order.
        if c["op"] == 3:
            assert side.random_calls >= 5 * side.new_calls
        return dict(state=states(words, len(c["actions"])), log=side.L.log)


def run_host(lib, c):
    lib.tpw_rb_run.restype = C.c_int32
    lib.tpw_rb_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_uint32)]
    size = WORDS_PER_STEP * len(c["actions"])
    out = (C.c_uint32 * size)()
    CL.host_begin(lib, {}, c["q"])
    n = lib.tpw_rb_run(i64(flat(c)), out)
    log = CL.host_end(lib)
    assert n == size
    return dict(state=states(list(out), len(c["actions"])), log=log)


def random_point(r):
    values = [0.0, -0.0, 1e-40, 1e30, math.nan, math.inf, -math.inf]
    return tuple(r.choice(values) if r.random() < 0.08 else T.f32(r.uniform(-50, 50)) for _ in range(3))


def random_generate(r):
    return case(1, [action(random_point(r), random_point(r), r.choice([0.0, -1.0, 0.6, 20.0]))],
                q={"Random": [r.getrandbits(32) for _ in range(5)]})


def edge_generate():
    # All choices and all zero-component fallbacks, signed remainders and degenerate cross products.
    for k in (0, 1, 2, -1, -2, -3, 32767, -32768):
        for d in ((1, 2, 3), (0, 2, 3), (0, 0, 3), (1, 0, 3), (1, 0, 0),
                  (1, 2, 0), (0, 2, 0), (0, 0, 0)):
            yield case(1, [action((2, 3, 4), tuple(x + y for x, y in zip((2, 3, 4), d)))],
                       q={"Random": [8190, -32768, 1, 0x12345678, k]})
    for value in (0, 8190, 8191, 8192, 16382, 32767, 32768, 65535, -1, -8191):
        yield case(1, q={"Random": [value, value, value, value, value]})
    for value in (math.nan, math.inf, -math.inf, 1e30, 1e-40, -0.0):
        for j in range(3):
            a = [1.0, 2.0, 3.0]
            a[j] = value
            yield case(1, [action(tuple(a), (4, 5, 6))], q={"Random": [1000, 2000, 0, 0, j]})
    r = random.Random(6701)
    for _ in range(100):
        yield random_generate(r)


def edge_section():
    for depth in range(4):
        for count in (0, 1, 9):
            for null in (False, True):
                for result in (0, 1):
                    yield case(2, [action(null=null)], depth=depth, count=count, q={"AddPoly": [result]})
        # Equal world positions never increment the count. A vertical projected segment does.
        yield case(2, [action((1, 2, 3), (1, 2, 3))], depth=depth)
        yield case(2, [action((1, 2, 3), (1, 2, 7))], depth=depth)
        # Exactly unchanged screen x/y: n > 0 increments, leaves all the other state alone.
        for null in (False, True):
            c = case(2, [action(b=(0, 0, 8), null=null)], depth=depth, count=3)
            c["rain"][0x9DE4 + depth * 16] = fb(1)
            c["rain"][0x9DE8 + depth * 16] = fb(2)
            yield c
        # Reversing the normal cancels the join, after the new segment is normalised.
        c = case(2, [action(b=(1, -4, 0))], depth=depth, count=1)
        c["rain"].update({0xA16C: fb(depth + 1), 0x9DE4 + depth * 16: fb(3),
                          0x9DE8 + depth * 16: fb(3), 0xA0E4 + depth * 8: fb(0),
                          0xA0E8 + depth * 8: fb(-1)})
        yield c
        # A whole polyline, including its NULL-from closing segment.
        yield case(2, [action((1, 1, 1), (2, 3, 4)), action((5, 8, 2), (3, 7, 9)),
                       action(b=(1, -2, 3), null=True)], depth=depth)
    yield case(2, [action(null=True, to_null=True)])
    for count in (-1, 0x7FFFFFFF, -0x80000000):
        yield case(2, count=count)
    yield case(2, [action((0.0, -0.0, 3), (-0.0, 0.0, 3))])
    yield case(2, [action((math.nan, 2, 3), (math.nan, 2, 3))])
    # A single-precision squared sum underflows to zero before the double comparison.
    for null in (False, True):
        c = case(2, [action(b=(-0.5, 4, 3), null=null)], count=1)
        c["rain"].update({0x9DE4: fb(1e-40), 0x9DE8: fb(-1e-40)})
        yield c
    for value in (math.nan, math.inf, -math.inf, 1e30, 1e-40):
        for count in (0, 1):
            for null in (False, True):
                yield case(2, [action(b=(value, value, 3), null=null)], count=count)


def random_section(r):
    depth = r.randrange(4)
    c = case(2, [action(random_point(r), random_point(r), null=r.random() < 0.3)
                 for _ in range(r.randrange(1, 5))], depth=depth, count=r.randrange(4),
             q={"AddPoly": [r.randrange(2) for _ in range(4)]})
    c["rain"][0xA16C] = fb(r.choice([0, -1.3, 1.3, 10, math.nan]))
    c["rain"][0xA19C] = r.getrandbits(32)
    for i in range(0x9DE4, 0xA164, 4):
        c["rain"][i] = fb(r.uniform(-10, 10))
    return c


def random_new(r):
    # Large thresholds keep these scripts finite, with forced side branches up to depth 3.
    c = case(3, [action(tuple(r.uniform(-10, 10) for _ in range(3)),
                       tuple(r.uniform(-10, 10) for _ in range(3))) for _ in range(r.randrange(1, 4))],
             count=r.randrange(3), q={"Random": [r.randrange(32768) for _ in range(2000)],
                                    "AddPoly": [r.randrange(2) for _ in range(16)]})
    c["rain"][0xA1A0] = r.randrange(4)
    c["rain"][0xA184] = fb(r.uniform(-10, 10))
    return c


def edge_new():
    for count in (0, 1):
        for q in ([0], [1, 0], [1]):
            yield case(3, count=count, q={"Random": [1000, 2000, 0, 0, 2, 511, 2048], "AddPoly": q})
    # Forced branch, both dot signs, the depth limit and each downstream failure order.
    for direction in ((0, -1, 0), (0, 1, 0), (0, 0, 0)):
        for q in ([0], [1, 0], [1, 1, 0], [1, 1, 1, 0], [1]):
            c = case(3, [action((2, 3, 4), (2, 13, 4))],
                     q={"Random": [1000, 2000, 0, 0, 2, 0, 0, 0] + [511] * 30, "AddPoly": q})
            c["rain"][0xA1A0] = 1
            for i, x in enumerate(direction):
                c["rain"][0x3020 + 2 * 0x30 + 4 * i] = fb(x)
            yield c
    for depth in (0, 1, 2, 3):
        c = case(3, depth=depth, q={"Random": [1000, 2000, 0, 0, 2, 0] + [511] * 100})
        c["rain"][0xA1A0] = depth
        yield c
    # The displacement favours one half, so each of the two recursive call sites can fail
    # independently. The first segment is already started, and the child draws use t = 0.
    for sign in (0, 1):
        for q in ([0], [1, 0], [1, 1, 0], [1]):
            c = case(3, [action((2, 3, 4), (2, 13, 4))], count=1,
                     q={"Random": [8190, 8190, sign, 0, 1, 511, 2048] +
                                    [0, 0, 0, 0, 1, 511, 2048] * 50, "AddPoly": q})
            c["rain"].update({0xA180: fb(30), 0xA184: fb(0)})
            yield c
    # Threshold equality and its neighbours with t=0: midpoint is exactly (1, 2, 3).
    for threshold in (T.f32(1.0 - 2**-24), 1.0, T.f32(1.0 + 2**-23)):
        for q in ([0], [1, 0], [1]):
            c = case(3, [action((0, 2, 3), (2, 2, 3))], count=1,
                     q={"Random": [0, 1, 0, 0, 2, 511, 2048] + [511] * 100, "AddPoly": q})
            c["rain"][0xA180] = fb(threshold)
            c["rain"][0xA184] = fb(0)
            yield c
    # Values set by CRain's constructor (Init does not overwrite them); __RenderLightning sets depth.
    for length in (20, 50, 500):
        for depth in (0, 3):
            r = random.Random(6700 + length + depth)
            c = case(3, [action((2, length + 3, 4), (2, 3, 4))],
                     q={"Random": [r.randrange(32768) for _ in range(2000)]})
            c["rain"].update({0xA180: fb(6), 0xA184: fb(2), 0xA16C: fb(1.3), 0x3310: fb(0.6), 0xA1A0: depth})
            yield c
    yield from edge_branch_rounding()


# Triage w67: the forced side branch's rounding. The branch is found by a Random of 0 (the sixth), and the next two Randoms pick the
# length factors f and g; the bolt in slot 2 supplies the direction. Each tuple below is (to.x, to.z, first Random, second Random,
# direction). The first three are found by a search (done offline with the C library's fmaf) where the direction is chosen so that
# direction.z * branch.z + A is a rounding error away from zero, with A = round(direction.x * branch.x): the fused outer add of the
# dot product leaves that error (negative, so the branch is turned), the separately rounded one gives exactly 0 (not negative, the
# branch is kept). The last two are denormal directions: branch.y is an odd number of the smallest denormal and the turned branch
# adds direction.y * g/2 (also an odd multiple), where 0.5 * branch.y is not exact, so the fused and the unfused sums round apart.
DOT_CANCELS = [
    ((-8.489745140075684, 6.043771743774414), 886, 4, (1.4878838062286377, 0.0, 1.1749972105026245)),
    ((4.0255022048950195, -3.664973020553589), 681, 864, (0.03982903063297272, 0.0, 0.04630350321531296)),
    ((4.864556789398193, -3.2269327640533447), 832, 149, (-0.7215055227279663, 0.0, -0.7186185717582703)),
]
SMALLEST_DENORMAL = 1.401298464324817e-45
DENORMAL_BRANCHES = [
    ((3.0, 1.0), 0, 0, (-1.0, 1 * SMALLEST_DENORMAL, 0.0), 0.0),
    ((3.0, 1.0), 0, 511, (-1.0, 1 * SMALLEST_DENORMAL, 0.0), 0.0),
]


# (The child call after the branch draws five Randoms for its midpoint: component 2, since a denormal d.y would pick y and divide by it.)
def branch_case(to, randoms, direction, to_y=0.0):
    c = case(3, [action((0.0, 0.0, 0.0), (to[0], to_y, to[1]))],
             q={"Random": [1000, 2000, 0, 0, 2, 0] + list(randoms) + [1000, 2000, 0, 0, 2] + [511] * 60, "AddPoly": [1]})
    c["rain"][0xA1A0] = 1
    for i, x in enumerate(direction):
        c["rain"][0x3020 + 2 * 0x30 + 4 * i] = fb(x)
    return c


def edge_branch_rounding():
    for to, first, second, direction in DOT_CANCELS:
        yield branch_case(to, (first, second), direction)
        # The same bolt with the result of the AddPoly calls failing at each level, so the branch's own draws show.
        c = branch_case(to, (first, second), direction)
        c["q"]["AddPoly"] = [1, 0]
        yield c
    for to, first, second, direction, to_y in DENORMAL_BRANCHES:
        yield branch_case(to, (first, second), direction, to_y)


TESTS = [
    FnTest("GeneratePoint", run_ppc, run_host, edge=edge_generate, rnd=random_generate),
    FnTest("CRain::AddLightningSection", run_ppc, run_host, edge=edge_section, rnd=random_section),
    FnTest("CRain::NewRenderLightning", run_ppc, run_host, edge=edge_new, rnd=random_new),
]
