"""The height field's node gamuts (SetupHFieldNodeGamuts, DoCalcNodeHFieldGamut) and the two
matrix helpers they call (CombineMatricesNoWDest2nd, VectorThroughLMatrixNoW), original
versus translation. The unit calls nothing outside itself, so there are no fakes and no call
log: a case sets up memory, calls, and everything the function could touch is compared
(floats bit for bit, a NaN equal to any NaN where arithmetic produced it)."""
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

SHIM_LIBRARY = "unit_NodeGamuts"
SETUP, DOCALC, COMBINE, VECTOR, IDENTITY = 0x100b971c, 0x100b9304, 0x100c28fc, 0x100c1f64, 0x100c0e28
LIMITS = (0x1029eb78, 0x1029eb7c, 0x1029eb80, 0x1029eb84)   # lowest x, highest x, lowest z, highest z
MASK = 0xffffffff
NODE_SIZE, HEADER_SIZE, FIELD_SIZE, MAX_NODES, NODE_DESC_WORDS = 0xa0, 0xb8, 0x30, 12, 27
FLT_MAX = 3.4028234663852886e38
NAN = 0x7fc00000


def fb(x):
    """The bit pattern of a single-precision value, as the number a word holds."""
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def bf(w):
    """The single-precision value a bit pattern holds."""
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def ws(e, a, values):
    """Writes 32-bit words, big-endian, to the emulator's memory."""
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    """Reads n big-endian 32-bit words from the emulator's memory."""
    return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))


def canon(w):
    """Any NaN is one value: the sign and payload of a NaN that went through arithmetic differ."""
    return NAN if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w


def step(x, n):
    """x moved n single-precision values toward +infinity."""
    w = fb(x)
    ordered = -(w & 0x7fffffff) if w & 0x80000000 else w
    ordered += n
    return bf(0x80000000 | -ordered if ordered < 0 else ordered)


IDENTITY_WORDS = [fb(1.0) if i % 5 == 0 else 0 for i in range(16)]
SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, 3.0, -2.5, FLT_MAX, -FLT_MAX, 1.1754943508222875e-38, 1.401298464324817e-45,
           1e-40, 1e30, -1e30, math.inf, -math.inf, math.nan, 2.0 ** -24, 1 + 2.0 ** -12, 12345.678, -0.001]


# Two instructions Unicorn (QEMU 5.0) does not execute as the processor does, corrected here for
# the code under test and nowhere else (FINDINGS 2026-10-08 and 2026-10-10):
#  - fmadds is a double-precision fused multiply-add followed by a rounding to single, which can
#    differ by one unit in the last place from the processor's single rounding of the exact
#    result; the result is replaced by the C library's fmaf, which rounds once;
#  - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor saturates
#    to 0x7fffffff.
# Both are patched by looking at the instruction word before it runs and rewriting its target
# register just before the next instruction.
LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]
CODE_RANGES = [(0x100b9304, 0x100b96d4), (0x100c1f64, 0x100c2000), (0x100c28fc, 0x100c2b18)]


def fpr(e, i):
    """The raw 64-bit pattern of floating-point register i."""
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    """Floating-point register i as a Python float."""
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Code hooks over the four functions that make the emulator's fmadds and fctiwz behave as the
    processor's do (see above); entered around every call of the original."""

    def __init__(self, e):
        self.e = e
        self.pending = None
        self.hooks = []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        """Runs before each instruction: applies the pending correction to the instruction before,
        then decodes this one and notes the correction it needs."""
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fmadds":
                uc.reg_write(ppcemu.FPR[target], struct.unpack(">Q", struct.pack(">d", value))[0])
            else:
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xffffffff00000000) | 0x7fffffff)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 59 and (word >> 1) & 31 == 29:            # fmadds frD, frA, frC, frB
            a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
            if all(math.isfinite(x) for x in (a, c, b)):
                for x in (a, c, b):
                    assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "fmadds operand is not a single"
                self.pending = ("fmadds", rd, float(LIBM.fmaf(a, c, b)))
        elif word >> 26 == 63 and (word >> 1) & 0x3ff == 15:       # fctiwz frD, frB
            x = fpr_double(e, rb)
            if x >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)


@contextlib.contextmanager
def original_code(e):
    """The unit's own functions run as the original, whatever stubs other modules left, with the
    two corrections above."""
    stubs = CL.Stubs()
    stubs.real.update([SETUP, DOCALC, COMBINE, VECTOR, IDENTITY])
    saved = stubs.activate(e)
    try:
        with HardwareRounding(e):
            yield
    finally:
        CL.Stubs.deactivate(e, saved)


def setup(runner):
    """The unit calls no imports; the standard stubs are installed as in the other modules."""
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------- VectorThroughLMatrixNoW

def vector_ppc(e, case):
    """The original: case = (16 matrix words, 3 point words); the matrix afterwards and the point."""
    matrix, point = case
    m = e.alloc(64)
    p = e.alloc(12)
    ws(e, m, matrix)
    ws(e, p, point)
    with original_code(e):
        T.call_ppc(e, VECTOR, [m, p])
    return dict(matrix=rs(e, m, 16), point=[canon(w) for w in rs(e, p, 3)])


def vector_host(lib, case):
    """The translation, through tpw_ng_vector."""
    matrix, point = case
    fn = lib.tpw_ng_vector
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * 19)()
    fn((C.c_uint32 * 16)(*matrix), (C.c_uint32 * 3)(*point), out)
    return dict(matrix=list(out[:16]), point=[canon(w) for w in out[16:19]])


def words(values):
    """Float values as their bit patterns."""
    return [fb(v) for v in values]


def random_value(r):
    """A float32 of mixed size, one time in ten a special value."""
    k = r.random()
    if k < 0.55:
        return T.f32(r.uniform(-1000, 1000))
    if k < 0.8:
        return T.f32(r.uniform(-3, 3))
    if k < 0.9:
        return T.f32(r.uniform(-1e6, 1e6))
    return r.choice(SPECIAL)


def random_matrix(r):
    """16 values; the translation row is often small or zero so that residues survive."""
    m = [random_value(r) for _ in range(16)]
    if r.random() < 0.5:
        m[12:15] = [0.0, 0.0, 0.0] if r.random() < 0.5 else [T.f32(r.uniform(-1e-3, 1e-3)) for _ in range(3)]
    return m


def vector_edges():
    """Special values in every place, a few plain cases, then seeded mixtures of both."""
    for s in SPECIAL:
        yield words([s] * 16), words([1.0, 2.0, 3.0])
        yield IDENTITY_WORDS, words([s, s, s])
        yield IDENTITY_WORDS, words([s, 1.0, -1.0])
        yield words([float(i) for i in range(16)]), words([s, 2.0, 0.5])
    yield IDENTITY_WORDS, words([0.0, 0.0, 0.0])
    yield words([float(i) for i in range(16)]), words([1.0, 1.0, 1.0])
    yield words([0.0] * 16), words([-0.0, -0.0, -0.0])
    r = random.Random(11)
    for _ in range(80):
        matrix = [r.choice(SPECIAL) if r.random() < 0.3 else random_value(r) for _ in range(16)]
        point = [r.choice(SPECIAL) if r.random() < 0.3 else random_value(r) for _ in range(3)]
        yield words(matrix), words(point)


def vector_random(r):
    """A random matrix and point; half of them built so that a fused add cancels its product."""
    m = random_matrix(r)
    p = [random_value(r) for _ in range(3)]
    k = r.random()
    if k < 0.25:
        # The first fused add cancels the rounded product of the first term: y = -x, row 1 = row 0.
        m[4:7] = m[0:3]
        p[1] = -p[0]
    elif k < 0.5:
        # The second one: x = 0, z = -y, row 2 = row 1.
        m[8:11] = m[4:7]
        p[0] = 0.0
        p[2] = -p[1]
    return words(m), words(p)


# ---------------------------------------------------------------- CombineMatricesNoWDest2nd

def combine_ppc(e, case):
    """The original: case = (source words, destination words, alias); with alias the destination is
    also the source."""
    source, destination, alias = case
    d = e.alloc(64)
    ws(e, d, destination)
    s = d
    if not alias:
        s = e.alloc(64)
        ws(e, s, source)
    with original_code(e):
        T.call_ppc(e, COMBINE, [s, d])
    return combine_result(rs(e, d, 16), rs(e, s, 16), alias)


def combine_host(lib, case):
    """The translation, through tpw_ng_combine."""
    source, destination, alias = case
    fn = lib.tpw_ng_combine
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.c_int32, C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * 32)()
    fn((C.c_uint32 * 16)(*source), (C.c_uint32 * 16)(*destination), alias, out)
    return combine_result(list(out[:16]), list(out[16:]), alias)


def combine_result(destination, source, alias):
    """The three columns written are computed (NaNs equal); the fourth column is untouched and so
    is a source that is not the destination (exact)."""
    def computed(words_):
        return [canon(w) if i % 4 != 3 else w for i, w in enumerate(words_)]
    return dict(destination=computed(destination), source=computed(source) if alias else source)


def combine_edges():
    """Special values in every place, a few plain cases, then seeded mixtures of both."""
    for s in SPECIAL:
        yield words([s] * 16), words([1.0] * 16), 0
        yield IDENTITY_WORDS, words([s] * 16), 0
        yield words([s] * 16), IDENTITY_WORDS, 0
        yield words([s] * 16), words([s] * 16), 1
    yield IDENTITY_WORDS, IDENTITY_WORDS, 0
    yield IDENTITY_WORDS, IDENTITY_WORDS, 1
    yield words([float(i) for i in range(16)]), words([float(16 - i) for i in range(16)]), 0
    yield words([float(i) for i in range(16)]), words([float(16 - i) for i in range(16)]), 1
    yield words([0.0] * 16), words([-0.0] * 16), 0
    r = random.Random(12)
    for _ in range(80):
        a = [r.choice(SPECIAL) if r.random() < 0.3 else random_value(r) for _ in range(16)]
        t = [r.choice(SPECIAL) if r.random() < 0.3 else random_value(r) for _ in range(16)]
        yield words(a), words(t), int(r.random() < 0.2)


def combine_random(r):
    """Two random matrices; half built so that a fused add cancels its product."""
    a, t = random_matrix(r), random_matrix(r)
    k = r.random()
    if k < 0.25:
        # The first fused add cancels: source column 1 = -column 0, destination row 1 = row 0.
        for row in range(4):
            a[row * 4 + 1] = -a[row * 4]
        t[4:8] = t[0:4]
        if r.random() < 0.5:
            for row in range(4):
                a[row * 4 + 2] = 0.0
    elif k < 0.5:
        # The second one: the first term is zero, source column 2 = -column 1, destination row 2 = row 1.
        for row in range(4):
            a[row * 4] = 0.0
            a[row * 4 + 2] = -a[row * 4 + 1]
        t[8:12] = t[4:8]
    return words(a), words(t), int(r.random() < 0.15)


# ---------------------------------------------------------------- the gamut functions

# A gamut case is a list of 32-bit words (the same list goes to both sides):
#   0 op (0 SetupHFieldNodeGamuts, 1 DoCalcNodeHFieldGamut)   1 node count
#   2..5 the limits at 0x1029eb78..84                        6..11 the height field: origin x z,
#   12..27 the matrix passed to DoCalcNodeHFieldGamut         cell size x z, cell counts x z
#   28.. the nodes, 27 words each: flags, parent, first child, next sibling (node numbers plus one,
#   0 for none), matrix (16), box minimum and maximum (6), the four gamut bytes (one word, memory order).
def gamut_ppc(e, desc):
    """The original: builds the nodes, field and header in emulator memory and returns everything
    afterwards (limits, field, header, all twelve node slots) with the links as node numbers."""
    op, count = desc[0], desc[1]
    nodes = e.alloc(NODE_SIZE * MAX_NODES, b"\xa5" * (NODE_SIZE * MAX_NODES))
    field = e.alloc(FIELD_SIZE, b"\xa5" * FIELD_SIZE)
    header = e.alloc(HEADER_SIZE, b"\xa5" * HEADER_SIZE)
    ws(e, field + 8, desc[6:12])
    ws(e, header + 0x6c, [field])
    ws(e, header + 0x78, [nodes])

    def address(index):
        return nodes + (index - 1) * NODE_SIZE if index else 0

    for i in range(count):
        d = desc[28 + i * NODE_DESC_WORDS: 28 + (i + 1) * NODE_DESC_WORDS]
        base = nodes + i * NODE_SIZE
        ws(e, base, [d[0], address(d[1]), address(d[3]), address(d[2])])   # flags, parent, sibling, first child
        ws(e, base + 0x10, d[4:20])
        ws(e, base + 0x78, d[20:26])
        e.write(base + 0x90, struct.pack("<I", d[26]))                       # four bytes, in memory order
    saved = [e.read(a, 4) for a in LIMITS]
    ws(e, LIMITS[0], desc[2:3])
    ws(e, LIMITS[1], desc[3:4])
    ws(e, LIMITS[2], desc[4:5])
    ws(e, LIMITS[3], desc[5:6])
    try:
        with original_code(e):
            if op == 0:
                T.call_ppc(e, SETUP, [header])
            else:
                T.call_ppc(e, DOCALC, [nodes, field, *desc[12:28]])
        out = [e.u32(a) for a in LIMITS]
        out += rs(e, field, FIELD_SIZE // 4)
        header_words = rs(e, header, HEADER_SIZE // 4)
        header_words[0x6c // 4] = 1 if header_words[0x6c // 4] == field else 0xfffffffe
        header_words[0x78 // 4] = (header_words[0x78 // 4] - nodes) // NODE_SIZE + 1 \
            if nodes <= header_words[0x78 // 4] < nodes + NODE_SIZE * MAX_NODES else 0xfffffffe
        out += header_words
        for i in range(MAX_NODES):
            w = rs(e, nodes + i * NODE_SIZE, NODE_SIZE // 4)
            for k in (1, 2, 3):
                w[k] = 0 if w[k] == 0 else (w[k] - nodes) // NODE_SIZE + 1 \
                    if nodes <= w[k] < nodes + NODE_SIZE * MAX_NODES else 0xfffffffe
            w[0x90 // 4] = struct.unpack("<I", e.read(nodes + i * NODE_SIZE + 0x90, 4))[0]
            out += w
        return out
    finally:
        for a, b in zip(LIMITS, saved):
            e.write(a, bytes(b))


def gamut_host(lib, desc):
    """The translation, through tpw_ng_gamut; the same dump."""
    fn = lib.tpw_ng_gamut
    fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    out = (C.c_uint32 * (4 + FIELD_SIZE // 4 + HEADER_SIZE // 4 + MAX_NODES * NODE_SIZE // 4))()
    fn((C.c_uint32 * len(desc))(*desc), out)
    return list(out)


def node_desc(flags=0, parent=0, child=0, sibling=0, matrix=IDENTITY_WORDS, box=(0.0,) * 6, gamut=0xa5a5a5a5):
    """One node's 27 words (see the layout above); the box is (min x y z, max x y z)."""
    return [flags, parent, child, sibling, *matrix, *words(box), gamut]


def make_desc(op, nodes, limits=(-128, 127, -128, 127), field=(0.0, 0.0, 1.0, 1.0, 100, 100), matrix=IDENTITY_WORDS):
    """nodes: node_desc lists (node 0 is the root). The field is origin x z, cell x z, counts x z."""
    f = [*words(field[:4]), field[4] & MASK, field[5] & MASK]
    padded = [list(n) for n in nodes] + [[0xa5a5a5a5] * NODE_DESC_WORDS] * (MAX_NODES - len(nodes))
    return [op, len(nodes), *[v & MASK for v in limits], *f, *matrix, *[w for n in padded for w in n]]


def links(parents):
    """Child and sibling indices (plus one) from a list of parent indices (-1 for the root)."""
    n = len(parents)
    child = [0] * n
    sibling = [0] * n
    last = {}
    for i in range(1, n):
        p = parents[i]
        if p in last:
            sibling[last[p]] = i + 1
        else:
            child[p] = i + 1
        last[p] = i
    return child, sibling


def tree(parents, flags=None, matrices=None, boxes=None, gamuts=None):
    """Node descriptions for a tree given by its parent list (the root is first, its parent -1)."""
    child, sibling = links(parents)
    out = []
    for i, p in enumerate(parents):
        out.append(node_desc(flags[i] if flags else 0, p + 1 if p >= 0 else 0, child[i], sibling[i],
                             matrices[i] if matrices else IDENTITY_WORDS, boxes[i] if boxes else (0.0,) * 6,
                             gamuts[i] if gamuts else 0xa5a5a5a5))
    return out


def affine(r):
    """A matrix as a node might have: a turn about Y, a scale, a translation, a stray fourth column."""
    angle = r.uniform(0, 6.3)
    sx, sy, sz = [r.choice([1.0, 1.0, 0.5, 2.0, -1.0, r.uniform(0.2, 3)]) for _ in range(3)]
    ca, sa = T.f32(math.cos(angle)), T.f32(math.sin(angle))
    t = [T.f32(r.uniform(-60, 60)) for _ in range(3)]
    w = [0.0, 0.0, 0.0, 1.0] if r.random() < 0.6 else [T.f32(r.uniform(-3, 3)) for _ in range(4)]
    return words([T.f32(ca * sx), 0.0, T.f32(-sa * sx), w[0], 0.0, sy, 0.0, w[1],
                  T.f32(sa * sz), 0.0, T.f32(ca * sz), w[2], t[0], t[1], t[2], w[3]])


def random_node_matrix(r):
    """Identity, a node-like affine matrix, a loose random one, or one with special values."""
    k = r.random()
    if k < 0.25:
        return IDENTITY_WORDS
    if k < 0.75:
        return affine(r)
    if k < 0.92:
        return words([T.f32(r.uniform(-2, 2)) for _ in range(12)] + [T.f32(r.uniform(-50, 50)) for _ in range(3)] + [1.0])
    return words([r.choice(SPECIAL) if r.random() < 0.3 else T.f32(r.uniform(-2, 2)) for _ in range(16)])


def random_box(r):
    """A box (min x y z, max x y z) within about 70 units, sometimes inside out, flat or odd."""
    lo = [T.f32(r.uniform(-30, 30)) for _ in range(3)]
    size = [T.f32(r.uniform(0, 40)) for _ in range(3)]
    k = r.random()
    if k < 0.1:
        size = [-s for s in size]                        # corners the wrong way round
    elif k < 0.15:
        size = [0.0, 0.0, 0.0]                           # a point
    elif k < 0.2:
        lo[r.randrange(3)] = r.choice(SPECIAL)           # a stray special value
    return (*lo, *[T.f32(a + b) for a, b in zip(lo, size)])


def random_flags(r):
    """Node flags: mostly 0, else the bits the function reads, else any."""
    k = r.random()
    if k < 0.5:
        return 0
    if k < 0.62:
        return 0x200
    if k < 0.72:
        return 0x40000000
    if k < 0.8:
        return r.choice([0x4000000, 0x2000, 0x4002000])
    if k < 0.85:
        return 0x40000200
    return r.getrandbits(32)


def random_limits(r):
    """The default limits, no limits, or four arbitrary ones."""
    k = r.random()
    if k < 0.6:
        return (-128, 127, -128, 127)
    if k < 0.7:
        return (-2 ** 31, 2 ** 31 - 1, -2 ** 31, 2 ** 31 - 1)
    return tuple(r.randint(-300, 300) for _ in range(4))


def random_cell_size(r):
    """A cell size, mostly a power of two."""
    return r.choice([1.0, 2.0, 4.0, 8.0, 0.5, 16.0, T.f32(r.uniform(0.3, 12))])


def random_origin(r):
    """A height field origin coordinate, often 0."""
    return r.choice([0.0, 0.0, T.f32(r.uniform(-100, 100)), -64.0, 32.0])


def random_field(r):
    """origin x z, cell size x z, cell counts x z."""
    counts = [r.choice([r.randint(1, 150), 64, 128, 100, r.randint(-2, 3), 2 ** 31 - 1]) for _ in range(2)]
    return (random_origin(r), random_origin(r), random_cell_size(r), random_cell_size(r), counts[0], counts[1])


def random_parents(r, n):
    """The parent list of a chain, a star or a random tree of n nodes."""
    shape = r.random()
    if shape < 0.25:
        return [-1] + list(range(n - 1))                  # a chain
    if shape < 0.4:
        return [-1] + [0] * (n - 1)                       # one parent with many children
    return [-1] + [r.randrange(i) for i in range(1, n)]


def gamut_random(op):
    """The random case generator for one entry point."""
    def make(r):
        n = r.choice([1, 2, 3, 4, 5, 6, 8, 12])
        parents = random_parents(r, n)
        nodes = tree(parents, [random_flags(r) for _ in range(n)], [random_node_matrix(r) for _ in range(n)],
                     [random_box(r) for _ in range(n)], [r.getrandbits(32) for _ in range(n)])
        return make_desc(op, nodes, random_limits(r), random_field(r), random_node_matrix(r))
    return make


def one_node(flags=0, matrix=IDENTITY_WORDS, box=(0.0,) * 6, **kw):
    """A case with one node (keywords: op, limits, field, matrix handed to the function)."""
    return make_desc(kw.pop("op", 1), [node_desc(flags, matrix=matrix, box=box)], **kw)


def gamut_edges(op):
    """The edge case generator for one entry point."""
    def edges():
        # Cells of whole positions on a unit grid, around zero and the field's edges.
        for ox in (0.0, -10.0, 7.5):
            for low in (-3.0, -1.0, -0.5, -0.0, 0.0, 0.25, 1.0, 5.0, 99.0):
                for high in (-1.0, 0.0, 0.5, 1.0, 1.001, 2.0, 99.0, 100.0, 101.0):
                    yield one_node(box=(low, 0.0, low, high, 0.0, high), op=op, field=(ox, ox, 1.0, 1.0, 100, 100))
        # The 0.999 added to the last position's quotient: positions at and around k + 0.001.
        for k in (0, 1, 5, 17, 50, 120):
            for j in range(-3, 4):
                p = step(T.f32(k + 0.001), j)
                yield one_node(box=(0.0, 0.0, 0.0, p, 0.0, p), op=op, field=(0.0, 0.0, 1.0, 1.0, 200, 200))
                yield one_node(box=(0.0, 0.0, 0.0, T.f32(p * 3.0), 0.0, T.f32(p * 3.0)), op=op,
                               field=(0.0, 0.0, 3.0, 3.0, 200, 200))
        # Every flag value the function reads, on a node with a real matrix; then the limits against boxes
        # that reach far outside the field.
        m = affine(random.Random(3))
        for flags in (0, 0x200, 0x40000000, 0x4000000, 0x2000, 0xffffffff):
            yield one_node(flags, matrix=m, box=(-5.0, -5.0, -5.0, 9.0, 9.0, 9.0), op=op,
                           field=(-20.0, -20.0, 2.0, 2.0, 30, 30))
        for limits in ((0, 0, 0, 0), (10, -10, 10, -10), (-2 ** 31, 2 ** 31 - 1, -2 ** 31, 2 ** 31 - 1),
                       (2 ** 31 - 1, 2 ** 31 - 1, 2 ** 31 - 1, 2 ** 31 - 1), (-2 ** 31, -2 ** 31, -2 ** 31, -2 ** 31),
                       (-1000, 1000, -1000, 1000), (3, 3, 5, 5), (-5, 5, -7, 7)):
            yield one_node(matrix=IDENTITY_WORDS, box=(-100.0, 0.0, -100.0, 100.0, 0.0, 100.0), op=op, limits=limits,
                           field=(-100.0, -100.0, 1.0, 1.0, 50, 50))
            yield one_node(matrix=IDENTITY_WORDS, box=(-300.0, 0.0, 12.0, 400.0, 0.0, 30.0), op=op, limits=limits,
                           field=(0.0, 0.0, 1.0, 1.0, 50, 50))
        # Special values everywhere a float is read.
        for s in SPECIAL:
            yield one_node(box=(s, 1.0, 2.0, 3.0, 4.0, 5.0), op=op, field=(0.0, 0.0, 1.0, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, s, 3.0, 4.0, 5.0), op=op, field=(0.0, 0.0, 1.0, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, 2.0, s, 4.0, 5.0), op=op, field=(0.0, 0.0, 1.0, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, 2.0, 3.0, 4.0, s), op=op, field=(0.0, 0.0, 1.0, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, 2.0, 30.0, 4.0, 50.0), op=op, field=(s, 0.0, 1.0, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, 2.0, 30.0, 4.0, 50.0), op=op, field=(0.0, s, 1.0, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, 2.0, 30.0, 4.0, 50.0), op=op, field=(0.0, 0.0, s, 1.0, 100, 100))
            yield one_node(box=(1.0, 1.0, 2.0, 30.0, 4.0, 50.0), op=op, field=(0.0, 0.0, 1.0, s, 100, 100))
            yield one_node(matrix=words([s] * 16), box=(1.0, 1.0, 2.0, 30.0, 4.0, 50.0), op=op)
        for count in (0, 1, 99, 100, 101, 2 ** 31 - 1, -1, -2 ** 31):
            yield one_node(box=(0.0, 0.0, 0.0, 99.5, 0.0, 99.5), op=op, field=(0.0, 0.0, 1.0, 1.0, count, 100))
            yield one_node(box=(0.0, 0.0, 0.0, 99.5, 0.0, 99.5), op=op, field=(0.0, 0.0, 1.0, 1.0, 100, count))
        # Trees: the skip and no-box flags on inner nodes, siblings after a skipped subtree.
        rr = random.Random(21)
        shapes = [[-1, 0, 0, 0], [-1, 0, 1, 2], [-1, 0, 0, 1, 1, 2], [-1, 0, 1, 0, 3, 3, 0]]
        for parents in shapes:
            n = len(parents)
            for flags in ([0] * n, [0x200] + [0] * (n - 1), [0, 0x40000000] + [0] * (n - 2), [0x40000000] * n,
                          [0x200] * n, [0, 0x200, 0x40000200] + [0] * (n - 3), [0, 0, 0x40000000, 0x200] + [0] * (n - 4)):
                mats = [affine(rr) for _ in range(n)]
                boxes = [random_box(rr) for _ in range(n)]
                yield make_desc(op, tree(parents, flags, mats, boxes, [0x01020304 * (i + 1) & MASK for i in range(n)]),
                                field=(-40.0, -40.0, 2.0, 2.0, 60, 60), matrix=affine(rr))
        # A deep chain of translations: each level adds to the matrix.
        n = 12
        yield make_desc(op, tree([-1] + list(range(n - 1)), None, [words([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 0, 5, 1])
                                                                   for _ in range(n)],
                                 [(0.0, 0.0, 0.0, 1.0, 1.0, 1.0)] * n), field=(0.0, 0.0, 1.0, 1.0, 40, 40))
    return edges


TESTS = [
    FnTest("VectorThroughLMatrixNoW (0x100c1f64)", vector_ppc, vector_host, edge=vector_edges, rnd=vector_random),
    FnTest("CombineMatricesNoWDest2nd (0x100c28fc)", combine_ppc, combine_host, edge=combine_edges, rnd=combine_random),
    FnTest("DoCalcNodeHFieldGamut (0x100b9304)", gamut_ppc, gamut_host, edge=gamut_edges(1), rnd=gamut_random(1)),
    FnTest("SetupHFieldNodeGamuts (0x100b971c)", gamut_ppc, gamut_host, edge=gamut_edges(0), rnd=gamut_random(0)),
]
