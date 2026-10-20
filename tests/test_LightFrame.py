"""The light manager's per-frame lighting and the render memory's small members (engine
library; src/tb_lightmanager_frame.cpp and src/tb_rendermemory_frame.cpp):

  CLightManager::CalcDiffuseColor, PrepareLighting, AddLights, CalcIntensities,
  CalcNodeVertexColors, CalcVertexFogIntensities, SMapLinkType::PrepareLights,
  CRenderMemory::SaveInfo, GetVertexEndPosition and GetIndexEndPosition.

A light case is a state (six lights, the chain, the ambient colour, the display's flags and fog,
the light manager's mask) and a script of operations run on both sides in order. After every
operation the test compares the result, the buffers the operation wrote and the whole state
(every word of every light, the chain, the accumulated colour, the inverse world matrix, the
world pointer, the mask, the display). The only callees outside the unit are CMatrix::Inverse
and CMatrix::TransformVertex, fakes on both sides that record the matrix words and the light
field they are given (tests/unit_LightFrame.cpp), and the library's sqrt, which the PowerPC
side stubs faithfully.

Two things the emulator does not do as the processor does are corrected for the code under
test (see HardwareRounding): the single-precision fused multiply-adds round twice, and fctiwz
of a value above INT32_MAX gives 0x80000000 where the processor saturates to 0x7fffffff.

The one input the translation has to choose is f30 on entry to CalcDiffuseColor (a spot light
with no point light before it in the chain reads it, see the notes): the PowerPC side sets it to
0.0 before every call, and the translation starts that value at 0.0.
"""
import contextlib
import ctypes as C
import math
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_LightFrame"

MASK = 0xFFFFFFFF
LIGHTS = 6
LIGHT_WORDS = 22
STATE_WORDS = 164
UNKNOWN = 0xFFFF

# The functions under test (labels in the engine's function table).
FN = dict(
    Diffuse="CalcDiffuseColor__13CLightManagerFP10_D3DVECTORfff",
    Prepare="PrepareLighting__13CLightManagerFP7CMatrixP11sAlignedBox",
    Add="AddLights__13CLightManagerFPP6CLight",
    Intensities="CalcIntensities__13CLightManagerFPUlP10_D3DVECTORP7SVertexi",
    Node="CalcNodeVertexColors__13CLightManagerFPUlP9tag_sNode",
    Fog="CalcVertexFogIntensities__13CLightManagerFP11SFVF_TLCTEXii",
    Links="PrepareLights__12SMapLinkTypeFv",
    Save="SaveInfo__13CRenderMemoryFPcPUs",
    VertexEnd="GetVertexEndPosition__13CRenderMemoryFv",
    IndexEnd="GetIndexEndPosition__13CRenderMemoryFv",
)
# The two callees that are faked: CMatrix::Inverse and CMatrix::TransformVertex.
INVERSE, TRANSFORM = 0x1001BA90, 0x1001C584

# Engine data the functions touch.
G_INVERSE, G_WORLD, G_CHAIN, G_RESULT, G_DIFFUSE = 0x100AF150, 0x100AF190, 0x100AF194, 0x100AF198, 0x100AF1A8
DISPLAY = 0x10165634
GUARDED = [(G_INVERSE, 0x6C), (DISPLAY, 4), (DISPLAY + 0x8BC, 4), (DISPLAY + 0x8D0, 8)]

# The operations (the numbers of tests/unit_LightFrame.cpp).
ADD, PREPARE, DIFFUSE, INTENSITIES, NODE, FOG, DISPLAY_OP, MASK_OP = range(1, 9)
KEEP_CURRENT = 4


def fb(x):
    """A float as its single-precision bits."""
    return CL.fbits(x)


def bf(word):
    """The float with the given single-precision bits."""
    return struct.unpack(">f", struct.pack(">I", word & MASK))[0]


def canon(word):
    """A NaN as one value."""
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF):
        return 0x7FC00000
    return word


# ----------------------------------------------------------------- the emulator's rounding

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    """The raw 64-bit pattern of floating-point register i."""
    v = e.uc.reg_read(ppcemu.FPR[i])
    if isinstance(v, int):
        return v
    return struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    """Floating-point register i as a Python float."""
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes the single-precision fused multiply-adds (fmadds, fmsubs,
    fnmadds, fnmsubs) in double precision and rounds that to single, which can differ by one
    unit in the last place from the processor's single rounding, and gives 0x80000000 for fctiwz
    of a value above INT32_MAX where the processor gives 0x7fffffff. A code hook over the
    functions that use them looks at each instruction before it runs and corrects its target
    register just before the next one (fmaf rounds once)."""

    changed = {"fma": 0, "fctiwz": 0}    # corrections that altered a result (all cases so far), for the notes

    def __init__(self, e, names):
        self.e = e
        self.pending = None
        self.hooks = []
        self.ranges = []
        for name in names:
            entry = e.by_label[name]["entry"]
            size = e.by_label[name]["size"]
            self.ranges.append((entry, entry + size))

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for hook in self.hooks:
            self.e.uc.hook_del(hook)
        self.hooks = []
        self.pending = None

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fma":
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            else:
                corrected = (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF
            if fpr(e, target) != corrected and not (kind == "fma" and value != value):
                HardwareRounding.changed[kind] += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 59 and ((word >> 1) & 31) in (28, 29, 30, 31):
            form = (word >> 1) & 31
            a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
            if not all(math.isfinite(x) for x in (a, c, b)):
                return
            for x in (a, c, b):
                assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
            subtract = form in (28, 30)
            value = float(LIBM.fmaf(a, c, -b if subtract else b))
            if form in (30, 31):
                value = -value
            self.pending = ("fma", rd, value)
        elif word >> 26 == 63 and (word >> 1) & 0x3FF == 15:
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)


# ----------------------------------------------------------------- random words

# Finite values that hit the edges of the arithmetic, and the ones that do not stay finite.
SPECIAL_FINITE = [fb(v) for v in (0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 0.25, 255.0, 1e-3, 3.0, -2.0, 100.0)] + [
    0x00000001, 0x007FFFFF, 0x00800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x3F800001, 0x3F7FFFFF]
SPECIAL_WILD = [0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00001, 0x7F800001]


class Gen:
    """Random float words with a style: "clean" (ordinary values), "special" (now and then an
    edge value) or "wild" (also infinities and NaNs)."""

    def __init__(self, rnd, style):
        self.r = rnd
        self.style = style

    def f(self, lo, hi):
        r = self.r
        k = r.random()
        if self.style == "special" and k < 0.15:
            return r.choice(SPECIAL_FINITE)
        if self.style == "wild":
            if k < 0.07:
                return r.choice(SPECIAL_WILD)
            if k < 0.20:
                return r.choice(SPECIAL_FINITE)
        return fb(T.f32(r.uniform(lo, hi)))

    def vec(self, lo, hi):
        return [self.f(lo, hi) for _ in range(3)]


def style_of(rnd):
    k = rnd.random()
    if k < 0.6:
        return "clean"
    if k < 0.85:
        return "special"
    return "wild"


# ----------------------------------------------------------------- cases

def make_light(g, kind=None, flags_high=None):
    """The 26 words of a light object, with 'next' (word 22) left for the case builder."""
    r = g.r
    if kind is None:
        kind = r.choice([2, 2, 2, 4, 4, 4, 6, 6, 0, 1, 3, 5, 7, 8, 10, 12, 14, 15])
    if flags_high is None:
        flags_high = r.choice([0, 0, 0, 0, 0x10000000, 0x20000000, 0xFFFFFFF0 & ~0x10000000, 0x00F00000, 0x80000000])
    flags = (flags_high & ~0xF) | kind
    words = [0xA5A5A5A5] * 26
    colour = [g.f(-0.2, 1.5) for _ in range(4)]
    words[1:5] = colour
    words[5:8] = g.vec(-5.0, 5.0)           # position
    words[8:11] = g.vec(-5.0, 5.0)          # mF20, the position in object space
    words[11:14] = g.vec(-1.0, 1.0)         # normal
    words[14:17] = g.vec(-1.0, 1.0)         # mData, the direction in object space
    radius = r.uniform(0.2, 8.0)
    words[17] = fb(T.f32(radius))
    words[18] = fb(T.f32(radius * radius)) if g.style == "clean" else g.f(0.0, 80.0)
    words[19] = fb(T.f32(1.0 / (radius * radius))) if g.style == "clean" else g.f(0.0, 4.0)
    words[20] = flags
    words[21] = r.getrandbits(32)
    words[22] = 0
    return words


def make_case(rnd, ops_for, style=None, chain_size=None, kinds=None):
    """A random state. ops_for(g, case) returns the script. The lights chain together in a
    random order; the others have a made-up next."""
    style = style or style_of(rnd)
    g = Gen(rnd, style)
    lights = []
    for i in range(LIGHTS):
        kind = kinds[i % len(kinds)] if kinds else None
        lights.append(make_light(g, kind))
    size = rnd.randint(0, LIGHTS) if chain_size is None else chain_size
    chain = rnd.sample(range(LIGHTS), size)
    case = dict(
        style=style,
        lights=lights,
        chain=chain,
        gd=[g.f(0.0, 1.0) for _ in range(4)],
        result=[rnd.getrandbits(32) for _ in range(4)],
        inverse=[rnd.getrandbits(32) for _ in range(16)],
        mask=[rnd.getrandbits(32), rnd.choice([MASK, MASK, 0, 0x00FF00FF, 0xFF000000, 0xFFFFFF00, rnd.getrandbits(32)])],
        display=[rnd.choice([0, 0x410100, 0x2000, 0x800, 0xFFFFF7FF, 0x400800, 0x7FF]), rnd.getrandbits(32),
                 fb(T.f32(rnd.uniform(0.0, 100.0))), fb(T.f32(rnd.uniform(0.0, 300.0)))],
    )
    case["ops"] = ops_for(g, case)
    return case


def chain_members(case):
    return list(case["chain"])


def state_words(case):
    """The flat state of tests/unit_LightFrame.cpp's header comment."""
    s = [0] * STATE_WORDS
    chain = case["chain"]
    s[0] = chain[0] + 1 if chain else 0
    nexts = {}
    for position, light in enumerate(chain):
        nexts[light] = chain[position + 1] + 1 if position + 1 < len(chain) else 0
    for i, words in enumerate(case["lights"]):
        for k in range(1, 22):
            s[1 + LIGHT_WORDS * i + k - 1] = words[k]
        s[1 + LIGHT_WORDS * i + 21] = nexts.get(i, (i * 5 + 3) % (LIGHTS + 1))
    s[133:137] = case["gd"]
    s[137:141] = case["result"]
    s[141] = 0
    s[142:158] = case["inverse"]
    s[158], s[159] = case["mask"]
    s[160:164] = case["display"]
    return s


def canon_state(s):
    """A state with every NaN made one value (the float words: lights 1..19, the colours and the
    matrix)."""
    out = list(s)
    for i in range(LIGHTS):
        for k in range(1, 20):
            out[1 + LIGHT_WORDS * i + k - 1] = canon(out[1 + LIGHT_WORDS * i + k - 1])
    for index in list(range(133, 141)) + list(range(142, 158)) + [162, 163]:
        out[index] = canon(out[index])
    return out


# ----------------------------------------------------------------- the operations of a script

def encode(op, g=None):
    """(opcode, input words, number of output words) of a script operation for the host."""
    kind = op[0]
    if kind == "add":
        return ADD, [len(op[1])] + list(op[1]), 1
    if kind == "prep":
        return PREPARE, list(op[1]) + [op[2]], 1
    if kind == "diffuse":
        return DIFFUSE, list(op[1]) + [op[2], op[3], op[4]], 0
    if kind == "intens":
        count, normals, vertices = op[1], op[2], op[3]
        words = [count] + [w for n in normals for w in n] + [w for v in vertices for w in v]
        return INTENSITIES, words, count + 2
    if kind == "node":
        count, sets, normals, positions, indices = op[1], op[2], op[3], op[4], op[5]
        words = [count, sets] + [w for n in normals for w in n] + [w for p in positions for w in p] + list(indices)
        return NODE, words, count + 2
    if kind == "fog":
        count, stride, vertex_words = op[1], op[2], op[3]
        return FOG, [count, stride] + list(vertex_words), 1 + len(vertex_words)
    if kind == "display":
        return DISPLAY_OP, list(op[1:5]), 0
    if kind == "mask":
        return MASK_OP, [op[1], op[2]], 0
    raise ValueError(kind)


def fog_words(count, stride):
    """How many vertex words the fog operation carries (the host's buffer is the same size)."""
    nbytes = (count - 1) * stride + 0x20 if count > 0 else 0x20
    return (nbytes + 3) // 4 + 8


# ----------------------------------------------------------------- the PowerPC side

class PpcWorld:
    """The six lights, the light manager and the engine data of one case on the PowerPC side."""

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.lights = [e.alloc(0x68, b"\xa5" * 0x68) for _ in range(LIGHTS)]
        self.number = {a: i + 1 for i, a in enumerate(self.lights)}
        self.manager = e.alloc(8, b"\0" * 8)
        self.worlds = {}
        L = CL.PpcLog({})
        e.cl = L
        self.log = L
        self.load(state_words(case))

    def address(self, number):
        return self.lights[number - 1] if 1 <= number <= LIGHTS else 0

    def light_number(self, address):
        if address == 0:
            return 0
        return self.number.get(address, UNKNOWN)

    def load(self, s):
        e = self.e
        for i, a in enumerate(self.lights):
            words = [0xA5A5A5A5] * 26
            for k in range(1, 23):
                words[k] = s[1 + LIGHT_WORDS * i + k - 1]
            words[22] = self.address(words[22])
            e.write(a, struct.pack(">26I", *words))
        e.write(G_CHAIN, struct.pack(">I", self.address(s[0])))
        e.write(G_DIFFUSE, struct.pack(">4I", *s[133:137]))
        e.write(G_RESULT, struct.pack(">4I", *s[137:141]))
        e.write(G_WORLD, struct.pack(">I", 0))
        e.write(G_INVERSE, struct.pack(">16I", *s[142:158]))
        e.write(self.manager, struct.pack(">2I", s[158], s[159]))
        e.write(DISPLAY, struct.pack(">I", s[160]))
        e.write(DISPLAY + 0x8BC, struct.pack(">I", s[161]))
        e.write(DISPLAY + 0x8D0, struct.pack(">2I", s[162], s[163]))

    def dump(self):
        e = self.e
        out = [0] * STATE_WORDS
        out[0] = self.light_number(e.u32(G_CHAIN))
        for i, a in enumerate(self.lights):
            words = struct.unpack(">26I", e.read(a, 0x68))
            for k in range(1, 22):
                out[1 + LIGHT_WORDS * i + k - 1] = words[k]
            out[1 + LIGHT_WORDS * i + 21] = self.light_number(words[22])
        out[133:137] = struct.unpack(">4I", e.read(G_DIFFUSE, 16))
        out[137:141] = struct.unpack(">4I", e.read(G_RESULT, 16))
        world = e.u32(G_WORLD)
        out[141] = 0 if world == 0 else self.worlds.get(world, UNKNOWN)
        out[142:158] = struct.unpack(">16I", e.read(G_INVERSE, 64))
        out[158], out[159] = struct.unpack(">2I", e.read(self.manager, 8))
        out[160] = e.u32(DISPLAY)
        out[161] = e.u32(DISPLAY + 0x8BC)
        out[162], out[163] = struct.unpack(">2I", e.read(DISPLAY + 0x8D0, 8))
        return out

    def field_id(self, address):
        """Which light field a pointer is, as unit_LightFrame.cpp's FieldId numbers them."""
        for i, base in enumerate(self.lights):
            codes = {0x38: 1, 0x2C: 2, 0x20: 3, 0x14: 4}
            if address - base in codes:
                return (i + 1) * 16 + codes[address - base]
        return 15

    # -- the fakes
    def fake_inverse(self, e):
        matrix = e.r(3)
        words = list(struct.unpack(">16I", e.read(matrix, 64)))
        self.log.rec("Inverse", [int(matrix == G_INVERSE)])
        self.log.rec("Matrix", words[:8])
        self.log.rec("Matrix", words[8:])
        e.write(matrix, struct.pack(">16I", *[(w + 1) & MASK for w in words]))

    def fake_transform(self, e):
        matrix, out, source, kind = e.r(3), e.r(4), e.r(5), T.s32(e.r(6))
        words = list(struct.unpack(">16I", e.read(matrix, 64)))
        v = list(struct.unpack(">3I", e.read(source, 12)))
        self.log.rec("TransformVertex", [int(matrix == G_INVERSE), kind, self.field_id(out), self.field_id(source)])
        self.log.rec("Matrix", words[:8])
        self.log.rec("Matrix", words[8:])
        self.log.rec("Vector", v)
        if kind == 3:
            result = [v[1], v[0] ^ 0x80000000, v[2]]
        else:
            result = [v[2], v[0], v[1]]
        e.write(out, struct.pack(">3I", *result))

    def stubs(self):
        st = CL.Stubs()
        st.real = set(self.e.func(name) for name in FN.values())
        st.internal(INVERSE, self.fake_inverse)
        st.internal(TRANSFORM, self.fake_transform)
        return st

    # -- the operations
    def call(self, name, args):
        e = self.e
        e.set_f(30, 0.0)
        with HardwareRounding(e, [FN["Diffuse"], FN["Intensities"], FN["Node"]]):
            return T.call_ppc(e, e.func(FN[name]), args)[0]

    def run(self, op):
        e = self.e
        kind = op[0]
        if kind == "add":
            numbers = op[1]
            array = words_buffer(e, [self.address(n) for n in numbers] + [0])
            return [self.call("Add", [self.manager, array]) & MASK]
        if kind == "prep":
            world = words_buffer(e, op[1])
            self.worlds[world] = op[2]
            return [self.call("Prepare", [self.manager, world, 0x1234]) & MASK]
        if kind == "diffuse":
            normal = words_buffer(e, op[1])
            self.call("Diffuse", [self.manager, normal, T.F(bf(op[2])), T.F(bf(op[3])), T.F(bf(op[4]))])
            return []
        if kind == "intens":
            count, normals, vertices = op[1], op[2], op[3]
            colours = words_buffer(e, [0xA5A5A5A5] * (count + 2))
            n = words_buffer(e, [w for v in normals for w in v])
            v = words_buffer(e, [w for x in vertices for w in x])
            self.call("Intensities", [self.manager, colours, n, v, count])
            return list(struct.unpack(">%dI" % (count + 2), e.read(colours, 4 * (count + 2))))
        if kind == "node":
            count, sets, normals, positions, indices = op[1], op[2], op[3], op[4], op[5]
            colours = words_buffer(e, [0xA5A5A5A5] * (count + 2))
            n = words_buffer(e, [w for v in normals for w in v])
            p = words_buffer(e, [w for x in positions for w in x])
            idx = e.alloc(2 * count + 4, struct.pack(">%dH" % count, *indices) + b"\0\0\0\0")
            node = e.alloc(0xA0, b"\xa5" * 0xA0)
            e.write(node + 0x58, struct.pack(">4H", 0xA5A5, 0xA5A5, 0xA5A5, count))
            e.write(node + 0x60, struct.pack(">I", p))
            e.write(node + 0x64, struct.pack(">I", n))
            e.write(node + 0x94, struct.pack(">I", idx))
            self.call("Node", [self.manager, colours, node])
            return list(struct.unpack(">%dI" % (count + 2), e.read(colours, 4 * (count + 2))))
        if kind == "fog":
            count, stride, words = op[1], op[2], op[3]
            buf = words_buffer(e, words)
            result = self.call("Fog", [self.manager, buf, count, stride]) & 0xFF
            return [result] + list(struct.unpack(">%dI" % len(words), e.read(buf, 4 * len(words))))
        if kind == "display":
            e.write(DISPLAY, struct.pack(">I", op[1]))
            e.write(DISPLAY + 0x8BC, struct.pack(">I", op[2]))
            e.write(DISPLAY + 0x8D0, struct.pack(">2I", op[3], op[4]))
            return []
        if kind == "mask":
            e.write(self.manager, struct.pack(">2I", op[2], op[1]))
            return []
        raise ValueError(kind)


def words_buffer(e, words, tail=4):
    """A PowerPC buffer holding big-endian words, with a few spare bytes (never zero-sized)."""
    data = struct.pack(">%dI" % len(words), *[w & MASK for w in words]) + b"\0" * tail
    return e.alloc(len(data), data)


@contextlib.contextmanager
def guarded(e):
    """The engine data a case overwrites, put back afterwards (other modules share it)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


def lights_ppc(e, case):
    with guarded(e):
        world = PpcWorld(e, case)
        stubs = world.stubs()
        saved = stubs.activate(e)
        try:
            results = []
            states = []
            for op in case["ops"]:
                results.append(world.run(op))
                states.append(canon_state(world.dump()))
            return dict(results=results, states=states, log=world.log.log)
        finally:
            CL.Stubs.deactivate(e, saved)


# ----------------------------------------------------------------- the host side

def bind(lib):
    """argtypes of the unit's calls (once per library)."""
    if getattr(lib, "lf_bound", False):
        return
    lib.lf_bound = True
    words = C.POINTER(C.c_uint32)
    lib.tpw_lf_load.argtypes = [words]
    lib.tpw_lf_dump.argtypes = [words]
    lib.tpw_lf_op.argtypes = [C.c_int32, words, words]
    lib.tpw_lf_links.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    lib.tpw_lf_rm_load.argtypes = [words]
    lib.tpw_lf_rm_dump.argtypes = [words]
    lib.tpw_lf_rm_op.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, words]


def u32_array(values, size=None):
    n = max(size if size is not None else len(values), len(values), 1)
    return (C.c_uint32 * n)(*[v & MASK for v in values])


def lights_host(lib, case):
    bind(lib)
    CL.host_begin(lib, {})
    lib.tpw_lf_load(u32_array(state_words(case)))
    results = []
    states = []
    for op in case["ops"]:
        opcode, words, outputs = encode(op)
        out = u32_array([], outputs + 1)
        lib.tpw_lf_op(opcode, u32_array(words), out)
        results.append([out[i] for i in range(outputs)])
        dump = u32_array([], STATE_WORDS)
        lib.tpw_lf_dump(dump)
        states.append(canon_state([dump[i] for i in range(STATE_WORDS)]))
    return dict(results=results, states=states, log=CL.host_end(lib))


# ----------------------------------------------------------------- scripts

def unit_vector(g):
    """A direction or normal: a unit-ish vector, now and then a zero or an axis."""
    r = g.r
    k = r.random()
    if k < 0.08:
        return [fb(0.0)] * 3
    if k < 0.2:
        v = [fb(0.0)] * 3
        v[r.randrange(3)] = r.choice([fb(1.0), fb(-1.0)])
        return v
    return g.vec(-1.0, 1.0)


def position(g):
    return g.vec(-6.0, 6.0)


def diffuse_ops(g, case):
    r = g.r
    ops = []
    for _ in range(r.randint(1, 4)):
        x, y, z = position(g)
        ops.append(("diffuse", unit_vector(g), x, y, z))
    return ops


def vertices_for(g, count):
    return [position(g) + [g.r.getrandbits(32)] for _ in range(count)]


def intensities_ops(g, case):
    r = g.r
    ops = []
    for _ in range(r.randint(1, 3)):
        count = r.choice([0, 1, 2, 3, 5, 7, 8, 9, 15, 16])
        if r.random() < 0.3:
            ops.append(("display", r.choice([0, 0x800, 0x410100, 0x410900]), 0, 0, 0))
        if r.random() < 0.3:
            ops.append(("mask", r.choice([MASK, 0, 0x00FF00FF, r.getrandbits(32)]), 0))
        ops.append(("intens", count, [unit_vector(g) for _ in range(count)], vertices_for(g, count)))
    return ops


def node_ops(g, case):
    r = g.r
    ops = []
    for _ in range(r.randint(1, 3)):
        count = r.choice([0, 1, 2, 3, 5, 8, 9, 16])
        sets = r.randint(1, 4)
        positions = [[g.f(-6.0, 6.0) for _ in range(12)] for _ in range(sets)]
        indices = [r.randrange(4 * sets) for _ in range(count)]
        if r.random() < 0.3:
            ops.append(("display", r.choice([0, 0x800, 0x410100, 0x410900]), 0, 0, 0))
        if r.random() < 0.3:
            ops.append(("mask", r.choice([MASK, 0, 0x00FF00FF, r.getrandbits(32)]), 0))
        ops.append(("node", count, sets, [unit_vector(g) for _ in range(count)], positions, indices))
    return ops


def matrix_words(g):
    return [g.f(-3.0, 3.0) for _ in range(16)]


def prepare_ops(g, case):
    """Add some of the lights that are not on the chain, prepare, repeat."""
    r = g.r
    ops = []
    members = set(case["chain"])
    for _ in range(r.randint(1, 4)):
        outside = [i for i in range(LIGHTS) if i not in members]
        if outside and r.random() < 0.6:
            picked = r.sample(outside, r.randint(0, len(outside)))
            ops.append(("add", [i + 1 for i in picked]))
            for i in picked:
                if case["lights"][i][20] & 0x10000000 == 0:
                    members.add(i)
        ops.append(("prep", matrix_words(g), r.randint(1, 8)))
    return ops


def fog_ops(g, case):
    r = g.r
    ops = []
    for _ in range(r.randint(1, 3)):
        count = r.choice([0, 1, 2, 3, 4, 7])
        stride = r.choice([0x20, 0x20, 0x20, 0x28, 0x30, 0x40, 0x24, 0x10, 0])
        words = [r.getrandbits(32) for _ in range(fog_words(count, stride))]
        near = r.uniform(0.0, 50.0)
        far = near + r.uniform(0.5, 100.0) if r.random() < 0.9 else r.uniform(0.0, 50.0)
        ops.append(("display", r.choice([0, 0x2000]), r.getrandbits(32), fb(T.f32(near)), fb(T.f32(far))))
        for k in range(count):
            slot = k * stride // 4 + 2
            if r.random() < 0.4:
                words[slot] = fb(T.f32(r.uniform(near - 5.0, far + 5.0)))
            elif r.random() < 0.3:
                words[slot] = g.f(-5.0, 200.0)
        ops.append(("fog", count, stride, words))
    return ops


def rnd_case(ops_for, **kw):
    def make(r):
        return make_case(r, ops_for, **kw)
    return make


# ----------------------------------------------------------------- edge cases

def light_words(kind, colour=(1.0, 1.0, 1.0, 1.0), position=(0.0, 0.0, 0.0), f20=(0.0, 0.0, 0.0),
                normal=(0.0, 0.0, 1.0), data=(0.0, 0.0, 1.0), radius_squared=4.0, flags_high=0):
    """A light by its values (floats), for the hand-made cases."""
    words = [0xA5A5A5A5] * 26
    words[1:5] = [fb(c) for c in colour]
    words[5:8] = [fb(p) for p in position]
    words[8:11] = [fb(p) for p in f20]
    words[11:14] = [fb(n) for n in normal]
    words[14:17] = [fb(d) for d in data]
    words[17] = fb(math.sqrt(radius_squared))
    words[18] = fb(radius_squared)
    words[19] = fb(1.0 / radius_squared) if radius_squared else 0
    words[20] = flags_high | kind
    words[21] = 0x11223344
    words[22] = 0
    return words


def hand_case(lights, chain, ops, gd=(0.2, 0.1, 0.1, 0.1), mask=(0, MASK), display=(0, 0, fb(10.0), fb(20.0))):
    filler = [light_words(2) for _ in range(LIGHTS - len(lights))]
    return dict(
        style="hand", lights=list(lights) + filler, chain=chain,
        gd=[fb(v) for v in gd], result=[0x11111111, 0x22222222, 0x33333333, 0x44444444],
        inverse=[fb(1.0) if i % 5 == 0 else 0 for i in range(16)], mask=list(mask), display=list(display), ops=ops)


def diffuse_edges():
    one = [fb(0.0), fb(0.0), fb(1.0)]
    zero = [fb(0.0)] * 3
    cases = []
    # No lights: the result is the ambient colour.
    cases.append(hand_case([], [], [("diffuse", one, fb(1.0), fb(2.0), fb(3.0))]))
    # A directional light facing, opposed to, and across the normal; and the exact zero.
    directional = light_words(2, colour=(0.5, 0.25, 0.75, 0.125), data=(0.0, 0.0, -1.0))
    for normal in ([0.0, 0.0, 1.0], [0.0, 0.0, -1.0], [1.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.6, 0.0, 0.8]):
        cases.append(hand_case([directional], [0], [("diffuse", [fb(c) for c in normal], 0, 0, 0)]))
    # An exact zero share leaves the sign of a zero ambient colour to the fused operation.
    cases.append(hand_case([light_words(2, data=(0.0, 0.0, 1.0))], [0], [("diffuse", zero, 0, 0, 0)],
                           gd=(-0.0, -0.0, -0.0, -0.0)))
    cases.append(hand_case([light_words(2, colour=(-1.0, -1.0, -1.0, -1.0), data=(0.0, 0.0, 1.0))], [0],
                           [("diffuse", zero, 0, 0, 0)], gd=(0.0, 0.0, 0.0, 0.0)))
    # An exact zero share is added (it is not below zero), which shows only in the sign of a zero:
    # a negative colour times a negative zero added to a negative zero ambient colour is a positive
    # zero, and a light left out would leave the negative one. The same for a spot light whose
    # dot product is exactly zero (positive colour, share +0).
    minus_zero = (-0.0, -0.0, -0.0, -0.0)
    cases.append(hand_case([light_words(2, colour=(-1.0, -1.0, -1.0, -1.0), data=(0.0, 0.0, 1.0))], [0],
                           [("diffuse", zero, 0, 0, 0)], gd=minus_zero))
    cases.append(hand_case([light_words(6, colour=(1.0, 1.0, 1.0, 1.0), f20=(0.0, 0.0, 5.0), radius_squared=100.0)], [0],
                           [("diffuse", zero, 0, 0, 0)], gd=minus_zero))
    # A point light: inside, on the radius (not lit), outside, with a zero radius.
    point = light_words(4, colour=(1.0, 0.5, 0.25, 0.125), f20=(1.0, 0.0, 0.0), radius_squared=4.0)
    for where in ((1.0, 0.0, 0.0), (3.0, 0.0, 0.0), (-1.0, 0.0, 0.0), (1.0, 2.0, 0.0), (1.0, 5.0, 0.0)):
        cases.append(hand_case([point], [0], [("diffuse", one, fb(where[0]), fb(where[1]), fb(where[2]))]))
    cases.append(hand_case([light_words(4, f20=(0.0, 0.0, 0.0), radius_squared=0.0)], [0],
                           [("diffuse", one, fb(0.0), fb(0.0), fb(0.0))]))
    # Squared distances exactly on the radius's square, in each axis and in two (a lit point at that
    # distance adds +0, which shows against a negative zero ambient colour).
    for f20, where, radius_squared in (((2.0, 0.0, 0.0), (0.0, 0.0, 0.0), 4.0), ((0.0, 3.0, 0.0), (0.0, 0.0, 0.0), 9.0),
                                       ((0.0, 0.0, -2.0), (0.0, 0.0, 0.0), 4.0), ((3.0, 4.0, 0.0), (0.0, 0.0, 0.0), 25.0),
                                       ((1.0, 1.0, 0.0), (0.0, 0.0, 0.0), 2.0)):
        light = light_words(4, colour=(1.0, 1.0, 1.0, 1.0), f20=f20, radius_squared=radius_squared)
        cases.append(hand_case([light], [0], [("diffuse", one, fb(where[0]), fb(where[1]), fb(where[2]))],
                               gd=(-0.0, -0.0, -0.0, -0.0)))
    # A spot light alone (f30 is 0.0), after a point light (its distance is used, lit or not),
    # and with the point light after it.
    spot = light_words(6, colour=(1.0, 1.0, 1.0, 1.0), f20=(0.0, 0.0, 5.0), radius_squared=100.0)
    near_point = light_words(4, colour=(0.0, 0.0, 0.0, 0.0), f20=(1.0, 1.0, 1.0), radius_squared=9.0)
    far_point = light_words(4, colour=(0.0, 0.0, 0.0, 0.0), f20=(30.0, 0.0, 0.0), radius_squared=9.0)
    for lights, chain in (([spot], [0]), ([spot, near_point], [1, 0]), ([spot, far_point], [1, 0]),
                          ([spot, near_point], [0, 1])):
        for normal in ([0.0, 0.0, 1.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]):
            cases.append(hand_case(lights, chain, [("diffuse", [fb(c) for c in normal], fb(0.0), fb(0.0), fb(0.0))]))
    # Kinds that add nothing, whatever the other bits say.
    for kind in (0, 1, 3, 5, 7, 8, 10, 12, 14, 15):
        cases.append(hand_case([light_words(kind, flags_high=0x30000000)], [0], [("diffuse", one, 0, 0, 0)]))
    # Kind 6 with the other bits of the flags set is still a spot light; the nibble is compared whole.
    cases.append(hand_case([light_words(6, flags_high=0xFFFFFFF0 & ~0x10000000, f20=(0.0, 0.0, 5.0))], [0],
                           [("diffuse", one, 0, 0, 0)]))
    # Several lights of every kind, with colours that overflow and underflow a byte.
    mixed = [light_words(2, colour=(2.0, 3.0, 4.0, 5.0), data=(0.0, 0.0, -1.0)),
             light_words(4, colour=(1.0, -9.0, 100.0, 1e30), f20=(0.5, 0.5, 0.5)),
             light_words(6, colour=(0.0, 1.0, 1.0, 1.0), f20=(0.0, 0.0, 2.0), radius_squared=50.0)]
    cases.append(hand_case(mixed, [2, 1, 0], [("diffuse", one, fb(0.0), fb(0.0), fb(0.0)),
                                              ("diffuse", [fb(0.0), fb(1.0), fb(0.0)], fb(1.0), fb(1.0), fb(1.0))]))
    # Non-finite inputs.
    nan = 0x7FC00000
    inf = 0x7F800000
    cases.append(hand_case([directional], [0], [("diffuse", [nan, fb(0.0), fb(1.0)], 0, 0, 0)]))
    cases.append(hand_case([point], [0], [("diffuse", one, nan, 0, 0), ("diffuse", one, inf, 0, 0)]))
    cases.append(hand_case([spot], [0], [("diffuse", [inf, 0, fb(1.0)], 0, 0, 0)]))
    return cases


def intensities_edges():
    cases = []
    white = light_words(2, colour=(1.0, 1.0, 1.0, 1.0), data=(0.0, 0.0, -1.0))
    normal = [fb(0.0), fb(0.0), fb(1.0)]
    vertex = [fb(1.0), fb(2.0), fb(3.0), 0x12345678]

    def op(count, lit=True):
        return ("intens", count, [normal] * count, [vertex] * count)

    # The lit path: counts around the eight-word blocks of the fill path, mask variants.
    for count in (0, 1, 3, 8, 9, 16):
        cases.append(hand_case([white], [0], [op(count)], mask=(0, MASK)))
    for mask in (0, 0x00FF00FF, 0xFFFFFF00, 0x80808080):
        cases.append(hand_case([white], [0], [op(3)], mask=(0, mask)))
    # The fill path (display flag 0x800 set), with other flag bits, and with it clear.
    for count in (0, 1, 7, 8, 9, 16):
        for flags in (0x800, 0xFFFFFFFF, 0x7FF, 0xFFFFF7FF):
            cases.append(hand_case([white], [0], [op(count)], mask=(0, 0x11223344), display=(flags, 0, fb(10.0), fb(20.0))))
    # Channel values: exact bytes, just under, clamped above and below, huge, NaN, infinities.
    for value in (0.0, 1.0, 0.5, 1.0 / 255.0, 0.999, 254.9 / 255.0, 1.5, 100.0, 1e10, 1e30, -0.001, -1.0, -1e30):
        light = light_words(2, colour=(0.0, value, value, value), data=(0.0, 0.0, -1.0))
        cases.append(hand_case([light], [0], [op(2)], gd=(0, 0, 0, 0)))
    for word in (0x7FC00000, 0x7F800000, 0xFF800000):
        case = hand_case([], [], [op(2)])
        case["gd"] = [fb(0.0), word, word, word]
        cases.append(case)
    return cases


def node_edges():
    cases = []
    white = light_words(2, colour=(0.0, 1.0, 0.5, 0.25), data=(0.0, 0.0, -1.0))
    sets = [[fb(float(k + 12 * s)) for k in range(12)] for s in range(3)]
    normals = [[fb(0.0), fb(0.0), fb(1.0)], [fb(0.0), fb(1.0), fb(0.0)], [fb(0.0), fb(0.0), fb(-1.0)]] * 4
    for count in (0, 1, 2, 3, 8, 9, 12):
        indices = [(i * 5 + 1) % 12 for i in range(count)]
        cases.append(hand_case([white], [0], [("node", count, 3, normals[:count], sets, indices)]))
        cases.append(hand_case([white], [0], [("node", count, 3, normals[:count], sets, indices)],
                               display=(0x800, 0, fb(1.0), fb(2.0)), mask=(0, 0x01020304)))
    return cases


def prepare_edges():
    cases = []
    identity = [fb(1.0) if i % 5 == 0 else 0 for i in range(16)]
    scaled = [fb(2.0) if i % 5 == 0 else 0 for i in range(16)]
    # Direction lengths: zero and exactly one are left alone; anything else is made unit length.
    for data in ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 2.0, 0.0), (3.0, 4.0, 0.0), (1.0, 1.0, 1.0), (1e-30, 0.0, 0.0),
                 (1e30, 1e30, 0.0), (float("inf"), 0.0, 0.0), (float("nan"), 0.0, 0.0), (0.6, 0.8, 0.0)):
        light = light_words(2, normal=data)
        cases.append(hand_case([light], [0], [("prep", identity, 1)]))
    # The two bits on their own and together, and neither; other flag bits.
    for kind in (2, 4, 6, 0, 1, 3, 5, 7, 8, 10, 12):
        light = light_words(kind, normal=(1.0, 2.0, 3.0), position=(4.0, 5.0, 6.0), flags_high=0x30000000)
        cases.append(hand_case([light], [0], [("prep", scaled, 2)]))
    # An empty chain; the world pointer and the inverse are still set.
    cases.append(hand_case([], [], [("prep", identity, 3)]))
    # AddLights: nothing, lights that do not chain, several in order, onto an existing chain.
    plain = [light_words(2), light_words(4), light_words(6), light_words(2, flags_high=0x10000000),
             light_words(4, flags_high=0x10000000), light_words(2, flags_high=0xEFFFFFF0)]
    cases.append(hand_case(plain, [], [("add", [])]))
    cases.append(hand_case(plain, [], [("add", [4, 5])]))
    cases.append(hand_case(plain, [], [("add", [1, 2, 3])]))
    cases.append(hand_case(plain, [0], [("add", [2, 4, 6]), ("prep", identity, 4), ("add", [3]), ("prep", scaled, 5)]))
    cases.append(hand_case(plain, [0, 1, 2], [("add", [4, 5, 6])]))
    return cases


def fog_edges():
    cases = []

    def vertices(depths, stride=0x20):
        count = len(depths)
        words = [0xA5A5A5A5] * fog_words(count, stride)
        for k, depth in enumerate(depths):
            words[k * stride // 4 + 2] = depth
        return words

    # Depths around the fog start and end: below, on, above, between, on the end, beyond.
    depths = [fb(v) for v in (0.0, 9.9, 10.0, 10.0001, 12.5, 15.0, 19.9999, 20.0, 20.0001, 1e30, -5.0)]
    for fog_colour in (0x40008000, 0xFFFFFFFF, 0x12345678, 0):
        words = vertices(depths)
        cases.append(hand_case([], [], [("display", 0x2000, fog_colour, fb(10.0), fb(20.0)),
                                        ("fog", len(depths), 0x20, words)]))
    # None fogged, one vertex, none.
    cases.append(hand_case([], [], [("display", 0x2000, 0x40008000, fb(10.0), fb(20.0)),
                                    ("fog", 3, 0x20, vertices([fb(1.0), fb(2.0), fb(3.0)])),
                                    ("fog", 0, 0x20, vertices([])),
                                    ("fog", 1, 0x20, vertices([fb(10.5)]))]))
    # Strides.
    for stride in (0x20, 0x30, 0x40, 0x24, 0x10, 0x08, 0):
        cases.append(hand_case([], [], [("display", 0x2000, 0x40008000, fb(1.0), fb(5.0)),
                                        ("fog", 3, stride, vertices([fb(0.5), fb(3.0), fb(9.0)], stride))]))
    # Odd fog ranges: near = far, far below near, zero range from zero, huge, infinite, NaN.
    inf = 0x7F800000
    nan = 0x7FC00000
    for near, far in ((fb(10.0), fb(10.0)), (fb(20.0), fb(10.0)), (fb(0.0), fb(0.0)), (fb(1e-30), fb(1e30)),
                      (fb(0.0), inf), (fb(10.0), nan), (nan, fb(10.0)), (0xFF800000, fb(10.0)), (fb(-5.0), fb(5.0)),
                      (fb(0.0), fb(1e-40))):
        cases.append(hand_case([], [], [("display", 0x2000, 0x40008000, near, far),
                                        ("fog", 6, 0x20, vertices([fb(v) for v in (-1.0, 0.0, 4.0, 6.0, 9.99, 1e-41)]))]))
    # NaN and infinite depths.
    cases.append(hand_case([], [], [("display", 0x2000, 0x40008000, fb(10.0), fb(20.0)),
                                    ("fog", 4, 0x20, vertices([nan, inf, 0xFF800000, 0x7F800001]))]))
    # Depths where the fused 1 - (far - depth) / (far - near) truncates to another byte than the
    # same sum with the product rounded first would (found by exact arithmetic).
    for near, far, depth in ((0.0, 255.0, 51.0), (0.0, 100.0, 40.0), (0.0, 300.0, 240.0), (10.0, 300.0, 68.0),
                             (50.0, 100.0, 70.0), (100.0, 255.0, 131.0), (10.0, 255.0, 206.0)):
        cases.append(hand_case([], [], [("display", 0x2000, 0x40008000, fb(near), fb(far)),
                                        ("fog", 1, 0x20, vertices([fb(depth)]))]))
    return cases


# ----------------------------------------------------------------- SMapLinkType::PrepareLights

class LinkWorld:
    """A list of links on the PowerPC side. The links' objects hold the vtable pointer of the
    fakes (setup), whose GetLinkType and Prepare slots are laid out as the original's (+0x10 and
    +0xc) and answer from this object."""

    def __init__(self, e, types, results):
        self.e = e
        self.log = CL.PpcLog({})
        self.types = {}
        self.results = {}
        count = len(types)
        self.links = []
        for i in range(count):
            link = e.alloc(8, struct.pack(">II", e.lf_vtable, 0))
            self.links.append(link)
            self.log.labels[link] = 11 + i
            self.types[link] = types[i]
            self.results[link] = results[i]
        self.nodes = e.alloc(8 * max(count, 1) + 8)
        for i, link in enumerate(self.links):
            following = self.nodes + 8 * (i + 1) if i + 1 < count else 0
            e.write(self.nodes + 8 * i, struct.pack(">II", link, following))

    def get_type(self, e):
        link = e.r(3)
        self.log.rec("GetLinkType", [self.log.lab(link)])
        e.set_r(3, T.u32(self.types[link]))

    def prepare(self, e):
        link = e.r(3)
        self.log.rec("Prepare", [self.log.lab(link)])
        e.set_r(3, T.u32(self.results[link]))


def links_ppc(e, case):
    types, results = case
    world = LinkWorld(e, types, results)
    e.lf_links = world
    stubs = CL.Stubs()
    stubs.real = set(e.func(name) for name in FN.values())
    saved = stubs.activate(e)
    try:
        head = world.nodes if types else 0
        T.call_ppc(e, e.func(FN["Links"]), [head])
        return dict(log=world.log.log)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.lf_links = None


def links_host(lib, case):
    bind(lib)
    types, results = case
    CL.host_begin(lib, {})
    lib.tpw_lf_links(len(types), (C.c_int32 * (len(types) + 1))(*types), (C.c_int32 * (len(types) + 1))(*results))
    return dict(log=CL.host_end(lib))


def links_edges():
    yield ([], [])
    for kind in (1, 0, 2, -1, 3, 0x7FFFFFFF, -0x80000000, 0x10001):
        yield ([kind], [5])
    yield ([1, 1, 1], [0, 1, 2])
    yield ([1, 1, 2, 1], [0, 0, 0, 0])
    yield ([1, 0, 1], [0, 0, 0])
    yield ([1] * 8, [-1] * 8)
    yield ([1, 1, 1, 1, 1, 1, 1, 0], [7] * 8)


def links_random(r):
    count = r.randint(0, 8)
    types = [1 if r.random() < 0.8 else r.choice([0, 2, -1, 3, r.getrandbits(31)]) for _ in range(count)]
    results = [r.choice([0, 1, -1, r.getrandbits(31)]) for _ in range(count)]
    return (types, results)


# ----------------------------------------------------------------- CRenderMemory

RM_WORDS = 25
RM_LOAD_FLAGS, RM_TYPE = 0, 1
RM_STATIC, RM_DYNAMIC = 2, 8     # first word of each pool: data, used, size, index data, index used, index size
RM_NEXT_DYNAMIC, RM_NEXT_STATIC = 20, 21


def rm_ppc(e, case):
    state, ops = case
    obj = words_buffer(e, state)
    stubs = CL.Stubs()
    stubs.real = set(e.func(name) for name in FN.values())
    saved = stubs.activate(e)
    try:
        results = []
        states = []
        for op in ops:
            if op[0] == "save":
                T.call_ppc(e, e.func(FN["Save"]), [obj, op[1], op[2]])
                results.append([])
            elif op[0] == "vend":
                results.append([T.call_ppc(e, e.func(FN["VertexEnd"]), [obj])[0] & MASK])
            elif op[0] == "iend":
                results.append([T.call_ppc(e, e.func(FN["IndexEnd"]), [obj])[0] & MASK])
            else:
                e.write(obj + 4 * op[1], struct.pack(">I", op[2] & MASK))
                results.append([])
            states.append(list(struct.unpack(">%dI" % RM_WORDS, e.read(obj, 4 * RM_WORDS))))
        return dict(results=results, states=states)
    finally:
        CL.Stubs.deactivate(e, saved)


def rm_host(lib, case):
    bind(lib)
    state, ops = case
    lib.tpw_lf_rm_load(u32_array(state))
    results = []
    states = []
    for op in ops:
        out = u32_array([], 2)
        if op[0] == "save":
            lib.tpw_lf_rm_op(1, op[1], op[2], out)
            results.append([])
        elif op[0] == "vend":
            lib.tpw_lf_rm_op(2, 0, 0, out)
            results.append([out[0]])
        elif op[0] == "iend":
            lib.tpw_lf_rm_op(3, 0, 0, out)
            results.append([out[0]])
        else:
            dump = u32_array([], RM_WORDS)
            lib.tpw_lf_rm_dump(dump)
            words = [dump[i] for i in range(RM_WORDS)]
            words[op[1]] = op[2]
            lib.tpw_lf_rm_load(u32_array(words))
            results.append([])
        dump = u32_array([], RM_WORDS)
        lib.tpw_lf_rm_dump(dump)
        states.append([dump[i] for i in range(RM_WORDS)])
    return dict(results=results, states=states)


def rm_state(flags, kind, static=(0x30000000, 0x1000, 0x100000, 0x30200000, 0x800, 0x10000),
             dynamic=(0x31000000, 0x2000, 0x100000, 0x31200000, 0x1000, 0x10000), next_dynamic=3, next_static=0xFFD):
    """A render memory's 25 words; the fields the three functions never touch get distinct values."""
    words = [0xC0DE0000 + i for i in range(RM_WORDS)]
    words[RM_LOAD_FLAGS], words[RM_TYPE] = flags, kind
    words[RM_STATIC:RM_STATIC + 6] = static
    words[RM_DYNAMIC:RM_DYNAMIC + 6] = dynamic
    words[RM_NEXT_DYNAMIC], words[RM_NEXT_STATIC] = next_dynamic, next_static
    return words


def rm_edges():
    save = ("save", 0x30001234, 0x30200640)
    for flags in (0, 1, 2, 3, 4, 5, 6, 7, 0x80000004, 0x7FFFFFFB):
        for kind in (1, 2):
            yield (rm_state(flags, kind), [save, ("vend",), ("iend",)])
    # The pool in use is the one the type names: the other is not touched.
    yield (rm_state(1, 1), [("save", 0x30000000, 0x30200000), ("save", 0x30100000, 0x30210000), ("vend",), ("iend",)])
    yield (rm_state(2, 2), [("save", 0x31000000, 0x31200000), ("save", 0x31100000, 0x31210000), ("vend",), ("iend",)])
    # Ends below the start of the pool, 32-bit wrap-around, a zero or negative size.
    yield (rm_state(1, 1), [("save", 0x10, 0x20), ("save", 0xFFFFFFF0, 0xFFFFFFFE), ("vend",)])
    yield (rm_state(1, 2), [("save", 0x7FFFFFFF, 0x80000000), ("save", 0x80000000, 0x7FFFFFFF)])
    yield (rm_state(0, 1, static=(0xFFFFF000, 0, 0x2000, 0xFFFFF800, 0, 0x1800)), [("vend",), ("iend",)])
    yield (rm_state(0, 2, dynamic=(0x31000000, 5, -0x100000, 0x31200000, 6, -0x10000)), [("vend",), ("iend",)])
    yield (rm_state(0, 1, static=(0x30000000, 0, 0, 0x30200000, 0, 0)), [("vend",), ("iend",)])
    # The record counters: down for the static pool, up for the dynamic, with wrap-around.
    yield (rm_state(0, 1, next_static=0), [("save", 0x30000100, 0x30200100), ("save", 0x30000100, 0x30200100)])
    yield (rm_state(0, 2, next_dynamic=0x7FFFFFFF), [("save", 0x31000100, 0x31200100), ("save", 0x31000100, 0x31200100)])
    yield (rm_state(0, 1, next_dynamic=0xFFFFFFFF, next_static=0x80000000), [("save", 0x30000100, 0x30200100)])
    # The type changes between calls.
    yield (rm_state(0, 1), [("save", 0x30000100, 0x30200100), ("set", RM_TYPE, 2), ("save", 0x31000100, 0x31200100),
                            ("vend",), ("set", RM_TYPE, 1), ("vend",), ("iend",)])


def rm_random(r):
    state = [r.getrandbits(32) for _ in range(RM_WORDS)]
    state[RM_LOAD_FLAGS] = r.choice([0, 1, 2, 3, 4, 5, 6, 7, r.getrandbits(32), r.getrandbits(32) | 4, r.getrandbits(32) & ~4])
    state[RM_TYPE] = r.choice([1, 2])
    if r.random() < 0.5:
        for base in (RM_STATIC, RM_DYNAMIC):
            state[base] = r.choice([0x30000000, 0x31000000, 0x20000000, 0xFFFF0000])
            state[base + 1] = r.randint(0, 0x100000)
            state[base + 2] = r.choice([0x100000, 0, r.randint(-0x1000, 0x200000)])
            state[base + 3] = state[base] + 0x200000 & MASK
            state[base + 4] = r.randint(0, 0x20000)
            state[base + 5] = r.choice([0x10000, 0, r.randint(-0x100, 0x20000)])
    ops = []
    for _ in range(r.randint(1, 6)):
        kind = r.choice(["save", "save", "vend", "iend", "set"])
        if kind == "save":
            ops.append(("save", r.choice([r.getrandbits(32), (state[RM_STATIC] + r.randint(0, 0x100000)) & MASK,
                                          (state[RM_DYNAMIC] + r.randint(0, 0x100000)) & MASK]),
                        r.choice([r.getrandbits(32), (state[RM_STATIC + 3] + r.randint(0, 0x20000)) & MASK,
                                  (state[RM_DYNAMIC + 3] + r.randint(0, 0x20000)) & MASK])))
        elif kind == "set":
            index = r.choice([RM_LOAD_FLAGS, RM_TYPE, RM_STATIC, RM_STATIC + 2, RM_STATIC + 3, RM_STATIC + 5, RM_DYNAMIC,
                              RM_DYNAMIC + 2, RM_DYNAMIC + 3, RM_DYNAMIC + 5, RM_NEXT_DYNAMIC, RM_NEXT_STATIC])
            value = r.choice([1, 2]) if index == RM_TYPE else r.getrandbits(32)
            ops.append(("set", index, value))
        else:
            ops.append((kind,))
    return (state, ops)


def setup(runner):
    """The standard stubs, and the fakes a link's virtual calls reach (laid out as the original's
    vtable: Prepare at +0xc, GetLinkType at +0x10)."""
    e = runner.emu
    T.install_std_stubs(e)
    e.lf_links = None
    prepare = T.fake_function(e, lambda em: em.lf_links.prepare(em))
    get_type = T.fake_function(e, lambda em: em.lf_links.get_type(em))
    e.lf_vtable = T.persist_alloc(e, 0x20)
    e.write(e.lf_vtable + 0x0C, struct.pack(">I", prepare))
    e.write(e.lf_vtable + 0x10, struct.pack(">I", get_type))


# ----------------------------------------------------------------- the tests

TESTS = [
    FnTest("CLightManager::CalcDiffuseColor", lights_ppc, lights_host, edge=diffuse_edges,
           rnd=rnd_case(diffuse_ops)),
    FnTest("CLightManager::CalcIntensities", lights_ppc, lights_host, edge=intensities_edges,
           rnd=rnd_case(intensities_ops)),
    FnTest("CLightManager::CalcNodeVertexColors", lights_ppc, lights_host, edge=node_edges,
           rnd=rnd_case(node_ops)),
    FnTest("CLightManager::PrepareLighting and AddLights", lights_ppc, lights_host, edge=prepare_edges,
           rnd=rnd_case(prepare_ops)),
    FnTest("CLightManager::CalcVertexFogIntensities", lights_ppc, lights_host, edge=fog_edges,
           rnd=rnd_case(fog_ops)),
    FnTest("SMapLinkType::PrepareLights", links_ppc, links_host, edge=links_edges, rnd=links_random),
    FnTest("CRenderMemory::SaveInfo, GetVertexEndPosition, GetIndexEndPosition", rm_ppc, rm_host, edge=rm_edges,
           rnd=rm_random),
]
