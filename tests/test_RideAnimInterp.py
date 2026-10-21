"""Differential test for src/game/LMatrix.cpp, src/game/rides/RideAnim.cpp, the node animation's fifteen interpolators:
RAInterpPosnAtFrame, RABezierInterFromShape, RABezierTangentFromShape, RALinearInterFromShape,
RALinearTangentFromShape, RACatmulInterFromShape, SetLMatrixOrientation, SetLMatrixScaleNoTrans,
SetLMatrixXZOrientation, CombineMatricesNoWDest2nd (the s3DAxis form at 0x100c37a4),
RACalcOrientationBetweenControlPoints, RASphericalLinearInterpolate, RAQuaternionToLAxis,
RAGetUVFrameData and RAAnimateTextureCoords, against libtpw_unit_RideAnimInterp.dylib.

The functions call nothing of the game, so there is nothing to fake: the original runs under the
emulator with its C library calls (modf, fmod, sqrt, acos) answered by Python's math module, and the
translation runs on the host with libm. Their inputs are TPGlobals' sine table, mask and scale
(+0, +4, +8, +0xc), the engine flags (+0x400c) and the words +0x401c and +0x4020, and the records of
a mesh animation.

One scenario function per original builds a case on a "side" (PpcSide: big-endian words in the
emulator's memory, absolute addresses; HostSide: little-endian words in arena blocks, Ptr32 fields
holding offsets from the arena's base) and calls the function. Both sides run the same scenario
code from the same case, so a case is described once. What is compared afterwards: every block the
scenario allocated (the data blocks word by word, floats with every NaN as one value and signed
zeros kept; the blocks that hold pointers only for being unchanged), TPGlobals (unchanged), and the
cells that the references of RAGetUVFrameData are made of (unchanged). Blocks are poisoned with
0xa5 before the case, so a stray write or an untouched output shows.

Unicorn computes fmadds in double precision and rounds twice; HardwareRounding (one code hook per
fused instruction, as in test_RideAnimTick.py) gives each the processor's single rounding. The
double-precision fused instructions (fmadd and fmsub of the matrix helpers) are exact in Unicorn.

Not tested: indexes and counts that make a function read outside the case's blocks (the cases have
guard areas around every array, and the tests keep inside them); frames that make an index of
2^31 (NaN, infinity, 1e10); aliasing between an output and an input.
"""
import ctypes as C
import math
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideAnimInterp"

MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000
TPGLOBALS = 0x103B74C8
GLOBALS_BYTES = 0x4024        # TPGlobals up to the first string: the table, flags and the two words

ENTRY = dict(
    posn=0x100C8CA8, bezier=0x100C84EC, bezier_tangent=0x100C86E8, linear=0x100C88E0,
    linear_tangent=0x100C89C8, catmul=0x100C8A8C, orientation=0x100C1EAC, scale=0x100C1AB8,
    xz=0x100C2EE8, combine=0x100C37A4, calc_orientation=0x100C8054, slerp=0x100C7DB0,
    quaternion_axis=0x100C7CA0, texture_coords=0x100C82D0, uv_frame=0x100C81A0)
# The host's entry points: the symbol (after tpw_rai_) and the leading arguments it takes.
HOST = dict(
    posn=("posn", []), bezier=("shape", [("i", 0)]), bezier_tangent=("shape", [("i", 1)]),
    linear=("shape", [("i", 2)]), linear_tangent=("shape", [("i", 3)]), catmul=("shape", [("i", 4)]),
    orientation=("orientation", []), scale=("scale", []), xz=("xz", []), combine=("combine", []),
    calc_orientation=("calc_orientation", []), slerp=("slerp", []), quaternion_axis=("quaternion_axis", []),
    texture_coords=("texture_coords", []), uv_frame=("uv_frame", []))
CTYPES = {"p": C.c_void_p, "r": C.c_void_p, "u": C.c_uint32, "i": C.c_int32, "f": C.c_float}

GUARD = 8                    # floats of poison before and after an array whose index can stray


# ------------------------------------------------------------------ small helpers


def canon_word(word):
    """A 32-bit word with every NaN pattern made one (the processors differ in the NaNs they make)."""
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x7FFFFF):
        return NAN_BITS
    return word


def f32(x):
    """A float rounded to single precision."""
    return T.f32(x)


# ------------------------------------------------------------------ the original's C library calls


def modf(e):
    """modf(double, double*): the fraction in f1, the whole part stored through r5 (a double takes r3 and
    r4)."""
    fraction, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(fraction)


def fmod(e):
    """fmod(double, double): C's answers for an infinite first argument or a zero second (NaN)."""
    x, y = e.f(1), e.f(2)
    if math.isnan(x) or math.isnan(y) or math.isinf(x) or y == 0.0:
        e.set_f1(math.nan)
    else:
        e.set_f1(math.fmod(x, y))


# ------------------------------------------------------------------ the emulator's fused multiply-adds

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    """The raw 64-bit pattern of floating-point register i."""
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    """Floating-point register i as a double."""
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds the result to single, which can differ by one unit in the last place from the
    processor's single rounding of the exact result. A code hook looks at each instruction
    before it runs and rewrites its target register just before the next one, with the C
    library's fmaf (one rounding). ranges: the (begin, end) address ranges of the functions
    that use fused instructions."""

    changed = 0    # results the correction replaced (all cases so far), for the notes
    seen = 0       # fused instructions it looked at

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


def function_ranges(e):
    """The (begin, end) ranges of the fifteen functions in the program's function table."""
    if not hasattr(e, "rai_ranges"):
        sizes = {f["entry"]: f["size"] for f in e.functions}
        e.rai_ranges = [(a, a + sizes[a]) for a in ENTRY.values()]
    return e.rai_ranges


# ------------------------------------------------------------------ TPGlobals

def sine_table(size):
    """The table BuildGeometryTables makes: sin(2 pi i / size) as singles."""
    return [f32(math.sin(2.0 * math.pi * i / size)) for i in range(size)]


def scrambled_table(size):
    """A table whose entries are all different and not sines, so that a wrong index shows."""
    return [f32(0.25 + 0.75 * ((i * 37 + 11) % size) / size) for i in range(size)]


TABLES = {
    "sine": sine_table(0x1000),
    "sine16": sine_table(16),
    "sine4": sine_table(4),
    "scrambled16": scrambled_table(16),
    "scrambled4096": scrambled_table(0x1000),
}
TABLE_SIZES = {"sine": 0x1000, "sine16": 16, "sine4": 4, "scrambled16": 16, "scrambled4096": 0x1000}


def make_globals(table="sine", flags=0, f401c=0, f4020=0, mask=None, scale=None):
    """The words a case puts in TPGlobals: the named table with its size, mask (size - 1 unless
    given) and scale (entries per radian unless given), the engine flags and the words 0x401c, 0x4020."""
    size = TABLE_SIZES[table]
    return dict(table=table, size=size, mask=size - 1 if mask is None else mask,
                scale=f32(size / 6.2831855) if scale is None else scale, flags=flags, f401c=f401c, f4020=f4020)


STANDARD = make_globals()


# ------------------------------------------------------------------ the two sides' memory


class Block:
    """A block a scenario allocated. kind: F floats (32-bit words, NaNs equal), I 32-bit words,
    H 16-bit words, - a record that holds pointers (only checked for being unchanged)."""

    def __init__(self, address, size, kind):
        self.address, self.size, self.kind, self.before = address, size, kind, None


class Side:
    """What the two sides share: the blocks, the dump of them and the comparison with TPGlobals."""

    def __init__(self):
        self.blocks = []
        self.expected_globals = None
        self.cells_ok = True

    def raw(self, block):
        raise NotImplementedError

    def snapshot(self):
        for block in self.blocks:
            block.before = self.raw(block)

    def dump(self):
        out = []
        for block in self.blocks:
            data = self.raw(block)
            if block.kind == "-":
                out.append(data == block.before)
            elif block.kind == "H":
                out.append(list(struct.unpack(self.order + "%dH" % (block.size // 2), data)))
            else:
                words = struct.unpack(self.order + "%dI" % (block.size // 4), data)
                out.append([canon_word(w) if block.kind == "F" else w for w in words])
        return out

    def finish(self):
        """What a scenario returns: whether TPGlobals and the cells are as they were, then the blocks."""
        return [self.globals_unchanged(), self.cells_ok] + self.dump()


class PpcSide(Side):
    """The PowerPC's memory: absolute addresses, big-endian words."""
    order = ">"

    def __init__(self, e):
        super().__init__()
        self.e = e

    def alloc(self, size, kind):
        address = self.e.alloc(size, b"\xa5" * size)
        self.blocks.append(Block(address, size, kind))
        return address

    def put(self, address, fmt, *values):
        self.e.write(address, struct.pack(">" + fmt, *values))

    def p32(self, address):
        return address

    def raw(self, block):
        return self.e.read(block.address, block.size)

    def set_globals(self, g):
        words = struct.pack(">IIf", g["size"], g["mask"], g["scale"])
        table = TABLES[g["table"]]
        self.e.write(TPGLOBALS, words)
        self.e.write(TPGLOBALS + 0xC, struct.pack(">%df" % len(table), *table))
        self.e.write(TPGLOBALS + 0x400C, struct.pack(">I", g["flags"]))
        self.e.write(TPGLOBALS + 0x401C, struct.pack(">II", g["f401c"], g["f4020"]))
        self.expected_globals = self.e.read(TPGLOBALS, GLOBALS_BYTES)

    def globals_unchanged(self):
        return self.e.read(TPGLOBALS, GLOBALS_BYTES) == self.expected_globals

    def run(self, key, args):
        """Calls the original. Arguments are (tag, value): p a pointer, u or i an integer, f a float, r a
        reference to a pointer (the address of a cell that holds it)."""
        e = self.e
        self.snapshot()
        placed = []
        cells = []
        for tag, value in args:
            if tag == "f":
                placed.append(T.F(value))
            elif tag == "r":
                cell = e.alloc(4)
                e.write(cell, struct.pack(">I", value))
                cells.append((cell, value))
                placed.append(cell)
            else:
                placed.append(value & MASK)
        with HardwareRounding(e, function_ranges(e)):
            T.call_ppc(e, ENTRY[key], placed)
        self.cells_ok = all(e.u32(cell) == value for cell, value in cells)


class HostSide(Side):
    """The host's memory: arena blocks, little-endian words, Ptr32 offsets from the arena's base."""
    order = "<"

    def __init__(self, lib):
        super().__init__()
        self.lib = lib
        lib.tpw_rai_base.restype = C.c_uint64
        lib.tpw_rai_alloc.restype = C.c_uint64
        lib.tpw_rai_alloc.argtypes = [C.c_uint32]
        lib.tpw_rai_reset()
        lib.tpw_rai_alloc(16)          # the arena exists, so its base is known; offset 0 is null
        self.base = lib.tpw_rai_base()

    def alloc(self, size, kind):
        address = self.lib.tpw_rai_alloc(size)
        C.memset(address, 0xA5, size)
        self.blocks.append(Block(address, size, kind))
        return address

    def put(self, address, fmt, *values):
        data = struct.pack("<" + fmt, *values)
        C.memmove(address, data, len(data))

    def p32(self, address):
        return address - self.base if address else 0

    def raw(self, block):
        return C.string_at(block.address, block.size)

    def set_globals(self, g):
        table = TABLES[g["table"]]
        bits = [struct.unpack("<I", struct.pack("<f", x))[0] for x in table]
        scale_bits = struct.unpack("<I", struct.pack("<f", g["scale"]))[0]
        self.lib.tpw_rai_set_globals.argtypes = [C.c_uint32] * 6
        self.lib.tpw_rai_set_globals(g["size"], g["mask"], scale_bits, g["flags"], g["f401c"], g["f4020"])
        self.lib.tpw_rai_set_table.argtypes = [C.POINTER(C.c_uint32), C.c_uint32]
        self.lib.tpw_rai_set_table((C.c_uint32 * len(bits))(*bits), len(bits))
        self.table_count = len(bits)
        self.expected_globals = [g["size"], g["mask"], scale_bits, g["flags"], g["f401c"], g["f4020"],
                                 sum(bits) & MASK]

    def globals_unchanged(self):
        out = (C.c_uint32 * 7)()
        self.lib.tpw_rai_get_globals.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
        self.lib.tpw_rai_get_globals(self.table_count, out)
        return list(out) == self.expected_globals

    def run(self, key, args):
        self.snapshot()
        symbol, leading = HOST[key]
        function = getattr(self.lib, "tpw_rai_" + symbol)
        function.restype = None
        every = leading + list(args)
        function.argtypes = [CTYPES[tag] for tag, _ in every]
        function(*[value for _, value in every])


def put_floats(side, address, values):
    """Writes floats at an address of the side's memory."""
    side.put(address, "%df" % len(values), *values)


def new_floats(side, values, guard=0):
    """A block of floats with `guard` floats of poison on each side; returns the first value's address."""
    address = side.alloc(4 * (len(values) + 2 * guard), "F")
    put_floats(side, address + 4 * guard, values)
    return address + 4 * guard


def run_ppc(e, scenario, case):
    """One case on the original: the C library calls stubbed, the unit's own addresses made real
    (other modules leave stubs on some of them), TPGlobals put back afterwards."""
    saved = e.read(TPGLOBALS, GLOBALS_BYTES)
    stubs = CL.Stubs()
    stubs.imp("modf", modf)
    stubs.imp("fmod", fmod)
    stubs.real.update(ENTRY.values())
    previous = stubs.activate(e)
    try:
        return scenario(PpcSide(e), case)
    finally:
        CL.Stubs.deactivate(e, previous)
        e.write(TPGLOBALS, saved)


def run_host(lib, scenario, case):
    """One case on the translation."""
    return scenario(HostSide(lib), case)


def setup(runner):
    """The emulator's standard stubs (sqrt and acos among them)."""
    T.install_std_stubs(runner.emu)


def make_test(name, scenario, edges, random_case):
    """A test of one function: the scenario on both sides, the edge cases and the random ones."""
    return FnTest(name,
                  lambda e, case: run_ppc(e, scenario, case),
                  lambda lib, case: run_host(lib, scenario, case),
                  edge=edges, rnd=random_case)


# ------------------------------------------------------------------ random values


def rval(r, wild=0.08, lo=-100.0, hi=100.0):
    """A random float: mostly moderate, some round numbers, and now and then anything (NaN, infinities,
    denormals)."""
    k = r.random()
    if k < wild:
        return T.rfloat(r, nan=True)
    if k < wild + 0.1:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 2.0, -2.0])
    return f32(r.uniform(lo, hi))


def rpoint(r, wild=0.05):
    """Three random floats."""
    return [rval(r, wild), rval(r, wild), rval(r, wild)]


# ------------------------------------------------------------------ RAInterpPosnAtFrame


def scenario_posn(side, case):
    """RAInterpPosnAtFrame: a track of values, a frame, the wrap flag; the answer is one float."""
    side.set_globals(STANDARD)
    values = new_floats(side, case["values"], GUARD)
    out = side.alloc(4, "F")
    side.run("posn", [("p", values), ("u", case["count"]), ("f", case["frame"]), ("p", out), ("i", case["wrap"])])
    return side.finish()


def posn_case(values, count, frame, wrap):
    """A case of RAInterpPosnAtFrame."""
    return dict(values=list(values), count=count, frame=f32(frame), wrap=wrap)


def posn_edges():
    """Counts 0 to 4, wrap on and off, frames across every index boundary (below the first value,
    on each, between, on the count and past it), and values that do and do not wrap round at 100."""
    frames = [-1.5, -0.5, -0.0, 0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.5]
    value_sets = [
        [10.0, 20.0, 30.0, 40.0, 50.0],
        [90.0, 10.0, 95.0, 5.0, 60.0],
        [10.0, 90.0, 20.0, 80.0, 0.0],
    ]
    for count in range(5):
        for wrap in (0, 1):
            for values in value_sets:
                for frame in frames:
                    if count == 0 and wrap == 1:
                        # The original divides by zero for the next index: undefined on the PowerPC;
                        # Unicorn answers a zero quotient and so does the translation.
                        if frame not in (0.5, 1.5):
                            continue
                    yield posn_case(values + [33.0] * 4, count, frame, wrap)
    # The difference of the two neighbours against 50: equal to it, just over it, both ways round.
    for pair in ([0.0, 50.0], [50.0, 0.0], [0.0, 50.0009], [50.0009, 0.0], [10.0, 60.0], [0.0, 100.0], [-30.0, 30.0]):
        for wrap in (0, 1):
            yield posn_case(pair, 2, 0.5, wrap)
            yield posn_case(pair, 2, 0.0, wrap)
            yield posn_case(pair, 2, 1.0, wrap)
    # Special values: NaN, infinities, huge and tiny, and the fraction's extremes.
    for pair in ([math.inf, 5.0], [5.0, math.inf], [-math.inf, math.inf], [math.nan, 5.0], [5.0, math.nan],
                 [1e30, -1e30], [1e-40, 99.0], [199.0, 1.0], [-5.0, 150.0]):
        for wrap in (0, 1):
            yield posn_case(pair, 2, 0.5, wrap)
            yield posn_case(pair, 2, 1.0, wrap)
    for frame in (1.9999999, 0.0000001, 2.0, 2.0000002, -0.0000001):
        yield posn_case([10.0, 95.0, 5.0], 3, frame, 1)
        yield posn_case([10.0, 95.0, 5.0], 3, frame, 0)
    # Fractions that leave 100 after the blend (the fmod).
    yield posn_case([99.0, 1.0], 2, 0.5, 0)
    yield posn_case([1.0, 99.0], 2, 0.5, 0)
    yield posn_case([1.0, 99.0], 2, 0.9999, 0)
    yield posn_case([60.0, 0.0], 2, 0.1, 1)


def posn_random(r):
    """Random counts, values (mostly percentages), frames across the track and the wrap flag."""
    count = r.choice([0, 1, 2, 2, 3, 3, 4, 5, 6, 8])
    kind = r.random()
    if kind < 0.6:
        values = [f32(r.uniform(0.0, 100.0)) for _ in range(count + 2)]
    elif kind < 0.85:
        values = [f32(r.uniform(-150.0, 250.0)) for _ in range(count + 2)]
    else:
        values = [rval(r, 0.3) for _ in range(count + 2)]
    wrap = r.randrange(2)
    if count == 0 and wrap == 1:
        wrap = 0
    pick = r.random()
    if pick < 0.2:
        frame = float(r.randint(-1, count + 2))
    elif pick < 0.3:
        frame = float(r.randint(-1, count + 2)) + f32(r.choice([-1e-6, 1e-6, 0.999999]))
    else:
        frame = r.uniform(-1.9, count + 2.9)
    return posn_case(values, count, frame, wrap)


# ------------------------------------------------------------------ the shape interpolators


def scenario_shape(key):
    """One of the five shape functions (key: the original's entry in ENTRY): a shape, an optional array of
    points, an index and a fraction; the answer is a point."""
    def scenario(side, case):
        """The scenario of one shape function."""
        side.set_globals(make_globals(f4020=case["n"]))
        flat = [c for point in case["points"] for c in point]
        points = new_floats(side, flat, 3 * 4)
        flat_other = [c for point in case["other"] for c in point]
        other = new_floats(side, flat_other, 3 * 4)
        shape = side.alloc(0x10, "-")
        side.put(shape, "IHHII", case["shape_flags"], case["n"], 0, side.p32(points), 0)
        out = side.alloc(12, "F")
        # With an array given the shape is not looked at (some cases pass none at all), except by
        # RALinearTangentFromShape, which never looks at the array and always reads the shape.
        given = other if case["use_array"] else 0
        no_shape = case["use_array"] and case["null_shape"] and key != "linear_tangent"
        shape_argument = 0 if no_shape else shape
        side.run(key, [("p", shape_argument), ("p", given), ("u", case["index"]), ("f", case["fraction"]),
                       ("p", out)])
        return side.finish()
    return scenario


def other_points(points):
    """The array given as `a`: different points from the shape's, so that taking the wrong array shows."""
    return [[x + 1.5, y * 2.0 - 0.25, z - 3.0] for x, y, z in points]


def shape_case(points, n, index, fraction, use_array=False, null_shape=False, flags=0, other=None):
    """A case of a shape function: the shape's points, the ring size (TPGlobals' 0x4020), the index and
    fraction, whether the array is given (and the shape then null), and the points of that array."""
    points = [[f32(c) for c in p] for p in points]
    other = [[f32(c) for c in p] for p in (other if other is not None else other_points(points))]
    return dict(points=points, other=other, n=n, index=index, fraction=f32(fraction), use_array=use_array,
                null_shape=null_shape, shape_flags=flags)


def sample_points(count, seed=1):
    """Distinct, mixed-sign points, so that a wrong neighbour shows."""
    return [[f32(math.sin(seed + i * 1.7) * 10.0), f32(math.cos(seed * 2 + i * 0.9) * 20.0 + i),
             f32(i * 3.0 - 5.0 + math.sin(i))] for i in range(count)]


def ring_edges(zero_ring):
    """Bezier and Catmull-Rom: rings of 1 to 5 points (and an empty one), indexes across and past the
    ring, fractions whole and fractional, negative and large."""
    cases = []
    for n in range(0 if zero_ring else 1, 6):
        points = sample_points(max(n, 4) + 1)
        pairs = [(0, 0.0), (0, 0.5), (1, 0.25), (max(n - 1, 0), 0.5), (n, 0.0), (n + 1, 1.5), (2, -0.5), (0, 3.75),
                 (3, 1.0), (5, 2.0), (1, -1.25), (2, 0.999)]
        for number, (index, fraction) in enumerate(pairs):
            if n == 0 and (index > 1 or fraction not in (0.0, 0.5, 0.25)):
                continue          # no ring: the point numbers are used as they are, so keep them in the block
            cases.append(shape_case(points, n, index, fraction, use_array=number % 3 == 1, null_shape=number % 6 == 1))
    # Large fractions: whole parts up to 2^31 - 128 are the same on the hardware and in Unicorn; a
    # larger positive one (or +infinity) saturates to 0x7fffffff there and gives 0x80000000 in Unicorn,
    # so those are left out. Negative overflow and NaN give 0x80000000 in both.
    for index, fraction in ((1, 1e9), (1, -1e9), (0, 2147483520.0), (7, -1e30), (2, -3.4e38), (3, 16777216.0),
                            (3, 16777217.0), (0xFFFFFFFF, 0.5), (0x80000000, 1.5), (0x7FFFFFFF, 2.5),
                            (5, -math.inf), (5, math.nan), (5, -2147483648.0), (5, -2147483904.0)):
        cases.append(shape_case(sample_points(5), 5, index, fraction))
    cases.append(shape_case([[math.inf, math.nan, -0.0]] * 5, 5, 1, 0.5))
    cases.append(shape_case([[1e30, -1e30, 1e-40]] * 5, 5, 1, 0.5))
    return cases


def ring_random(r):
    """Random rings, indexes and fractions for the bezier and Catmull-Rom functions."""
    n = r.choice([1, 2, 3, 3, 4, 4, 5, 6, 8])
    points = [rpoint(r) for _ in range(n + r.choice([0, 0, 1, 2]))]
    kind = r.random()
    if kind < 0.5:
        fraction = r.uniform(-2.0, 6.0)
    elif kind < 0.7:
        fraction = float(r.randint(-2, 6))
    elif kind < 0.85:
        fraction = r.uniform(0.0, 1.0)
    else:
        fraction = r.choice([1e9, -1e9, -1e30, math.nan, -math.inf, 2147483520.0, f32(r.uniform(-2e9, 2e9)),
                             f32(r.uniform(-1e5, 1e5))])
    return shape_case(points, n, r.randrange(0, 2 * n + 4), fraction, use_array=r.random() < 0.4,
                      null_shape=r.random() < 0.5, flags=r.getrandbits(8),
                      other=[rpoint(r) for _ in range(len(points))])


def line_edges():
    """Linear shapes: indexes and fractions that keep the two points (index + whole, + 1) inside the
    block's guard."""
    cases = []
    for n in range(1, 6):
        points = sample_points(n)
        pairs = [(0, 0.0), (0, 0.5), (0, 1.0), (0, 0.999), (max(n - 2, 0), 0.5), (max(n - 2, 0), 0.0), (n - 1, 0.0),
                 (n - 1, 0.5), (0, -0.5), (1, -1.0), (0, 1.5), (0, 2.25), (2, 0.75), (1, -0.25), (0, -1.75)]
        for number, (index, fraction) in enumerate(pairs):
            cases.append(shape_case(points, n, index, fraction, use_array=number % 3 == 1, null_shape=number % 6 == 1))
    cases.append(shape_case([[math.inf, math.nan, -0.0], [1.0, -math.inf, 3.0]], 2, 0, 0.5))
    cases.append(shape_case([[1e30, -1e30, 1e-40], [1e30, 1e30, -1e-40]], 2, 0, 0.5))
    return cases


def line_random(r):
    """Random points and a fraction that keeps the two points of a segment inside the block, for the linear
    functions."""
    n = r.choice([2, 3, 3, 4, 5, 6])
    points = [rpoint(r) for _ in range(n)]
    whole = r.randint(-1, 1)
    fraction = whole + r.choice([0.0, r.uniform(0.0, 1.0), r.uniform(0.0, 1.0), 0.5, 0.999999])
    index = r.randrange(0, max(n - 1 - max(whole, 0), 1))
    if r.random() < 0.1:
        fraction = f32(r.uniform(-2.5, 2.5))
    return shape_case(points, n, index, fraction, use_array=r.random() < 0.4, null_shape=r.random() < 0.5,
                      flags=r.getrandbits(8), other=[rpoint(r) for _ in range(len(points))])


def indexed(cases):
    """A test's edge list from a list built once."""
    return lambda: cases


# ------------------------------------------------------------------ the matrix helpers


def rmatrix(r, wild=0.04):
    """Sixteen floats for a matrix: mostly moderate, and now and then anything."""
    return [rval(r, wild, -50.0, 50.0) for _ in range(16)]


def sample_matrix(seed=1):
    """A matrix whose sixteen words are all different, so that a misplaced word shows."""
    return [f32(math.sin(seed + i * 0.7) * 3.0 + 0.1 * i) for i in range(16)]


def scenario_orientation(side, case):
    """SetLMatrixOrientation: a matrix and three axes, each null or not, in one block or three."""
    side.set_globals(STANDARD)
    matrix = side.alloc(0x40, "F")
    put_floats(side, matrix, case["matrix"])
    # The three axes in one block (as AnimateNode's s3DAxis) or in three.
    axes = case["axes"]
    if case["packed"]:
        block = new_floats(side, [c for axis in axes for c in (axis or [0.0, 0.0, 0.0])])
        addresses = [block + 12 * i for i in range(3)]
    else:
        addresses = [new_floats(side, axis or [0.0, 0.0, 0.0]) for axis in axes]
    arguments = [("p", matrix)] + [("p", 0 if axes[i] is None else addresses[i]) for i in range(3)]
    side.run("orientation", arguments)
    return side.finish()


def orientation_case(matrix, axes, packed):
    """A case of SetLMatrixOrientation."""
    return dict(matrix=matrix, axes=[None if a is None else [f32(c) for c in a] for a in axes], packed=packed)


def orientation_edges():
    """Every combination of null axes, in one block and in three, and special values."""
    cases = []
    axes = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]]
    for mask in range(8):
        chosen = [axes[i] if mask >> i & 1 else None for i in range(3)]
        for packed in (False, True):
            cases.append(orientation_case(sample_matrix(mask), chosen, packed))
    cases.append(orientation_case(sample_matrix(), [[math.nan, math.inf, -0.0], [1e-40, -1e30, 0.0], [5.0, 5.0, 5.0]],
                                  True))
    return cases


def orientation_random(r):
    """A random matrix and axes, each null one time in four."""
    axes = [None if r.random() < 0.25 else rpoint(r) for _ in range(3)]
    return orientation_case(rmatrix(r), axes, r.random() < 0.5)


def scenario_scale(side, case):
    """SetLMatrixScaleNoTrans: a matrix and three scales (references to three floats, or to one)."""
    side.set_globals(STANDARD)
    matrix = side.alloc(0x40, "F")
    put_floats(side, matrix, case["matrix"])
    scales = new_floats(side, case["scales"])
    # (Same: all three references are the one float.)
    places = [scales, scales, scales] if case["same"] else [scales, scales + 4, scales + 8]
    side.run("scale", [("p", matrix)] + [("p", place) for place in places])
    return side.finish()


def scale_case(matrix, scales, same=False):
    """A case of SetLMatrixScaleNoTrans."""
    return dict(matrix=matrix, scales=[f32(c) for c in scales], same=same)


def scale_edges():
    """Round and awkward scales and rows: zero, huge, tiny, NaN and infinite."""
    cases = []
    identity = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 5.0, 6.0, 7.0, 1.0]
    for scales in ([1.0, 1.0, 1.0], [2.0, 3.0, 4.0], [-1.0, 0.5, 0.0], [0.0, 0.0, 0.0], [1e30, 1e-30, 3.0],
                   [math.inf, 2.0, math.nan]):
        cases.append(scale_case(identity, scales))
        cases.append(scale_case(sample_matrix(), scales))
        cases.append(scale_case(sample_matrix(2), scales, same=True))
    # Zero rows (length 0: a division by zero), one zero row, rows that overflow or underflow when squared.
    zero = [0.0] * 16
    cases.append(scale_case(zero, [1.0, 2.0, 3.0]))
    cases.append(scale_case(identity[:4] + [0.0] * 4 + identity[8:], [1.0, 2.0, 3.0]))
    big = [1e30, 1e30, 1e30, 0.0, 1e-30, 1e-30, 1e-30, 0.0, 3e19, 4e19, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0]
    cases.append(scale_case(big, [1.0, 1.0, 1.0]))
    tiny = [1e-23, 1e-23, 1e-23, 0.0, 1e-40, 0.0, 0.0, 0.0, 1.0, 1e-23, 1e-23, 0.0, 0.0, 0.0, 0.0, 1.0]
    cases.append(scale_case(tiny, [1.0, 1.0, 1.0]))
    cases.append(scale_case([math.nan] * 16, [1.0, 1.0, 1.0]))
    cases.append(scale_case([math.inf, 0.0, 0.0, 0.0, -math.inf, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0],
                            [1.0, 1.0, 1.0]))
    return cases


def scale_random(r):
    """A random matrix and scales."""
    return scale_case(rmatrix(r), [rval(r, 0.1, -10.0, 10.0) for _ in range(3)], same=r.random() < 0.1)


def scenario_xz(side, case):
    """SetLMatrixXZOrientation: a matrix and a direction."""
    side.set_globals(STANDARD)
    matrix = side.alloc(0x40, "F")
    put_floats(side, matrix, case["matrix"])
    direction = new_floats(side, case["direction"])
    side.run("xz", [("p", matrix), ("p", direction)])
    return side.finish()


def xz_case(matrix, direction):
    """A case of SetLMatrixXZOrientation."""
    return dict(matrix=matrix, direction=[f32(c) for c in direction])


def xz_edges():
    """Directions along and against the axes, parallel to the up row, zero, huge, tiny, NaN and infinite."""
    cases = []
    identity = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 5.0, 6.0, 7.0, 1.0]
    for direction in ([0.0, 0.0, 1.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, -1.0, 0.0], [1.0, 1.0, 1.0],
                      [0.6, 0.0, 0.8], [-0.3, 0.2, 0.9], [0.0, 0.0, 0.0], [3.0, -2.0, 5.0], [1e-30, 1e-30, 1e-30],
                      [1e20, -1e20, 1e20], [math.nan, 0.0, 1.0], [math.inf, 0.0, 1.0], [0.0, 1e-23, 1e-23]):
        cases.append(xz_case(identity, direction))
        cases.append(xz_case(sample_matrix(3), direction))
    # An up that is not (0, 1, 0), parallel to the direction, and zero.
    upright = sample_matrix(5)
    upright[4:7] = [1.0, 2.0, 2.0]
    cases.append(xz_case(upright, [1.0, 2.0, 2.0]))
    cases.append(xz_case(upright, [-2.0, -4.0, -4.0]))
    upright[4:7] = [0.0, 0.0, 0.0]
    cases.append(xz_case(upright, [1.0, 2.0, 2.0]))
    return cases


def xz_random(r):
    """A random matrix and direction."""
    return xz_case(rmatrix(r), [rval(r, 0.05, -5.0, 5.0) for _ in range(3)])


def scenario_combine(side, case):
    """CombineMatricesNoWDest2nd (s3DAxis): nine floats of axes and a matrix."""
    side.set_globals(STANDARD)
    axis = new_floats(side, case["axis"])
    matrix = side.alloc(0x40, "F")
    put_floats(side, matrix, case["matrix"])
    side.run("combine", [("p", axis), ("p", matrix)])
    return side.finish()


def combine_case(axis, matrix):
    """A case of the axis form of CombineMatricesNoWDest2nd."""
    return dict(axis=[f32(c) for c in axis], matrix=matrix)


def combine_edges():
    """The identity, an ordinary axis, zero matrices and special values."""
    cases = []
    identity_axis = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
    sample_axis = [f32(math.cos(i * 0.6) * 2.0 + 0.3) for i in range(9)]
    for seed in range(4):
        cases.append(combine_case(identity_axis, sample_matrix(seed)))
        cases.append(combine_case(sample_axis, sample_matrix(seed)))
    cases.append(combine_case(sample_axis, [0.0] * 16))
    cases.append(combine_case([0.0] * 9, sample_matrix()))
    cases.append(combine_case([math.nan, math.inf, -0.0, 1.0, 2.0, 3.0, 1e30, 1e-30, 5.0], sample_matrix(6)))
    cases.append(combine_case(sample_axis, [1e30] * 16))
    return cases


def combine_random(r):
    """Random axes and matrix."""
    return combine_case([rval(r, 0.05, -5.0, 5.0) for _ in range(9)], rmatrix(r))


# ------------------------------------------------------------------ the rotation helpers


def rquaternion(r, wild=0.04):
    """Four random floats for a quaternion (not necessarily a unit one)."""
    return [rval(r, wild, -2.0, 2.0) for _ in range(4)]


def unit_quaternion(r):
    """A random unit rotation (or near it): four normal numbers over their length."""
    q = [r.gauss(0.0, 1.0) for _ in range(4)]
    length = math.sqrt(sum(c * c for c in q)) or 1.0
    return [f32(c / length) for c in q]


def scenario_quaternion_axis(side, case):
    """RAQuaternionToLAxis: a quaternion in, nine floats out."""
    side.set_globals(STANDARD)
    quaternion = new_floats(side, case["q"])
    out = side.alloc(0x24, "F")
    side.run("quaternion_axis", [("p", quaternion), ("p", out)])
    return side.finish()


def quaternion_axis_case(q):
    """A case of RAQuaternionToLAxis."""
    return dict(q=[f32(c) for c in q])


def quaternion_axis_edges():
    """Unit axes and rotations, zero, scaled, tiny, huge and special quaternions."""
    cases = []
    for q in ([0.0, 0.0, 0.0, 1.0], [1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0],
              [0.5, 0.5, 0.5, 0.5], [0.5, -0.5, 0.5, -0.5], [0.0, 0.0, 0.0, 0.0], [2.0, 0.0, 0.0, 0.0],
              [1.0, 2.0, 3.0, 4.0], [-1.0, 2.0, -3.0, 4.0], [1e-30, 1e-30, 1e-30, 1e-30], [1e20, 1e20, 1e20, 1e20],
              [math.nan, 0.0, 0.0, 1.0], [math.inf, 0.0, 0.0, 1.0], [0.0, 0.0, 0.0, -0.0], [1e-23, 0.0, 0.0, 1e-23]):
        cases.append(quaternion_axis_case(q))
    return cases


def quaternion_axis_random(r):
    """A random unit quaternion or a random one."""
    return quaternion_axis_case(unit_quaternion(r) if r.random() < 0.5 else rquaternion(r))


def scenario_slerp(side, case):
    """RASphericalLinearInterpolate: two quaternions and t, with TPGlobals' table, mask and scale of the
    case."""
    side.set_globals(case["g"])
    a = new_floats(side, case["a"])
    b = new_floats(side, case["b"])
    out = side.alloc(0x10, "F")
    side.run("slerp", [("p", a), ("p", b), ("f", case["t"]), ("p", out)])
    return side.finish()


def slerp_case(a, b, t, g=STANDARD):
    """A case of RASphericalLinearInterpolate."""
    return dict(a=[f32(c) for c in a], b=[f32(c) for c in b], t=f32(t), g=g)


def around(value, steps):
    """The single-precision number `steps` units in the last place from value."""
    bits = struct.unpack(">I", struct.pack(">f", value))[0]
    return struct.unpack(">f", struct.pack(">I", bits + steps))[0]


def slerp_edges():
    """Close, apart and opposite pairs, each table and mask, the two thresholds a unit apart, t outside 0
    to 1 and special values."""
    cases = []
    a = [0.0, 0.0, 0.0, 1.0]
    sixty = [math.sin(math.pi / 6), 0.0, 0.0, math.cos(math.pi / 6)]
    for t in (0.0, 0.25, 0.5, 1.0, 1.5, -0.5):
        cases.append(slerp_case(a, sixty, t))
        cases.append(slerp_case(a, a, t))
        cases.append(slerp_case(a, [0.0, 0.0, 0.0, -1.0], t))
        cases.append(slerp_case(sixty, a, t))
    # Each table, mask and scale that the cases use, at one spherical and one opposite pair.
    for table in TABLES:
        for mask in (None, 0, 3):
            g = make_globals(table, mask=mask)
            for b in (sixty, [0.0, 0.6, 0.0, -0.8], [1.0, 0.0, 0.0, 0.0]):
                cases.append(slerp_case(a, b, 0.3, g))
    # The two thresholds: 1 + dot against 0.001 and 1 - dot against 0.001, a unit in the last place either side.
    threshold = 0.001
    for step in range(-3, 4):
        low = around(f32(threshold - 1.0), step)          # dot just above -1 + 0.001
        cases.append(slerp_case(a, [0.0, math.sqrt(max(1.0 - low * low, 0.0)), 0.0, low], 0.5))
        high = around(f32(1.0 - threshold), step)         # dot just below 1 - 0.001
        cases.append(slerp_case(a, [0.0, math.sqrt(max(1.0 - high * high, 0.0)), 0.0, high], 0.5))
    for dot in (-1.0, -0.999, -0.9989, -0.99899, -0.9990001, 0.999, 0.9989, 0.99899, 0.9990001, 1.0, 0.0, -0.5):
        cases.append(slerp_case(a, [0.0, math.sqrt(max(1.0 - dot * dot, 0.0)), 0.0, dot], 0.7))
    # t far outside 0 to 1 (negative angles wrap round the table's mask), NaN and infinite coordinates.
    for t in (-3.0, 5.0, -1000.0, 2000.5, 1e-30, -1e5):
        cases.append(slerp_case(a, sixty, t))
        cases.append(slerp_case(a, [0.0, 0.6, 0.0, -0.8], t))
    cases.append(slerp_case([math.nan, 0, 0, 1], sixty, 0.5))
    cases.append(slerp_case(a, [0, math.inf, 0, 1], 0.5))
    cases.append(slerp_case([0, 0, 0, 0], [0, 0, 0, 0], 0.5))
    cases.append(slerp_case([2.0, 0, 0, 0], [0, 3.0, 0, 0], 0.5))
    cases.append(slerp_case(a, sixty, math.nan))
    return cases


def slerp_random(r):
    """Random pairs (a half apart, a fifth close, a fifth opposite) with a random t, table and mask."""
    kind = r.random()
    a = unit_quaternion(r)
    if kind < 0.5:
        b = unit_quaternion(r)
    elif kind < 0.7:
        b = [f32(c + r.uniform(-0.02, 0.02)) for c in a]
    elif kind < 0.9:
        b = [f32(-c + r.uniform(-0.02, 0.02)) for c in a]
    else:
        a, b = rquaternion(r, 0.1), rquaternion(r, 0.1)
    t = r.choice([f32(r.uniform(0.0, 1.0)), f32(r.uniform(0.0, 1.0)), f32(r.uniform(-1.5, 2.5)), 0.0, 1.0, 0.5])
    table = r.choice(list(TABLES))
    mask = r.choice([None, None, None, 0, 1, 3, 7, 0x5])
    mask = None if mask is not None and mask >= TABLE_SIZES[table] else mask
    flags = r.getrandbits(32)
    return slerp_case(a, b, t, make_globals(table, flags=flags, mask=mask))


def scenario_calc_orientation(side, case):
    """RACalcOrientationBetweenControlPoints: an array of keys, an index and a fraction, with TPGlobals'
    key count and flags."""
    side.set_globals(case["g"])
    # Keys of 0x14 bytes (a frame, a word, four floats) with two spare keys of poison on each side.
    count = len(case["keys"])
    block = side.alloc(0x14 * (count + 4), "-")
    for i, (frame, word, q) in enumerate(case["keys"]):
        side.put(block + 0x14 * (i + 2), "HHffff", frame, word, *q)
    out = side.alloc(0x24, "F")
    side.run("calc_orientation", [("p", block + 0x28), ("u", case["index"]), ("f", case["fraction"]), ("p", out)])
    return side.finish()


def calc_case(keys, index, fraction, g):
    """A case of RACalcOrientationBetweenControlPoints."""
    return dict(keys=[(frame, word, [f32(c) for c in q]) for frame, word, q in keys], index=index,
                fraction=f32(fraction), g=g)


def calc_edges():
    """Every index of two key counts with the flags that pick each blend, the key count against the index,
    and special fractions."""
    cases = []
    keys = [(0, 0xFFFF, [0.0, 0.0, 0.0, 1.0]), (10, 0xFFFF, [0.5, 0.5, 0.5, 0.5]),
            (20, 3, [0.0, 0.6, 0.0, -0.8]), (30, 0xFFFF, [0.1, 0.2, 0.3, 0.9])]
    for flags in (0, 2, 3, 0xFFFFFFFD, 0xFFFFFFFF):
        table = "sine"
        for count in (4, 3):
            for index in range(count):
                for fraction in (0.0, 0.5, 1.0):
                    cases.append(calc_case(keys, index, fraction, make_globals(table, flags=flags, f401c=count)))
    # The key count against the index: zero keys, one, more than there are, and the last index.
    for f401c in (0, 1, 2, 3, 4, 5, 0xFFFFFFFF):
        for flags in (0, 2):
            for index in (0, 2, 3):
                cases.append(calc_case(keys, index, 0.25, make_globals("sine", flags=flags, f401c=f401c)))
    for table in ("sine16", "scrambled4096"):
        cases.append(calc_case(keys, 1, 0.5, make_globals(table, flags=0, f401c=4)))
        cases.append(calc_case(keys, 1, 0.5, make_globals(table, flags=2, f401c=4)))
    cases.append(calc_case(keys, 0, math.nan, make_globals("sine", flags=2, f401c=4)))
    cases.append(calc_case(keys, 0, -2.5, make_globals("sine", flags=2, f401c=4)))
    cases.append(calc_case(keys, 0, 7.5, make_globals("sine", flags=0, f401c=4)))
    return cases


def calc_random(r):
    """Random keys, index, fraction, table and flags."""
    count = r.randint(1, 5)
    keys = [(r.randrange(0, 100), r.choice([0xFFFF, r.randrange(0, 8)]),
             unit_quaternion(r) if r.random() < 0.8 else rquaternion(r, 0.1)) for _ in range(count)]
    f401c = r.choice([count, count, count, count - 1, count + 1, 0, 1])
    index = r.randrange(0, count) if r.random() < 0.9 else r.randrange(0, count + 1)
    index = min(index, count - 1) if f401c == 0 else index
    fraction = r.choice([f32(r.uniform(0.0, 1.0)), f32(r.uniform(0.0, 1.0)), 0.0, 1.0, f32(r.uniform(-1.5, 2.5))])
    table = r.choice(["sine", "sine", "sine16", "scrambled4096"])
    return calc_case(keys, index, fraction, make_globals(table, flags=r.getrandbits(32), f401c=f401c))


# ------------------------------------------------------------------ the texture coordinate tracks


def scenario_uv_frame(side, case):
    """RAGetUVFrameData: a control at a frame and a list of key frames; the answers are an index and a
    fraction."""
    side.set_globals(STANDARD)
    control = side.alloc(0x38, "-")
    side.put(control + 0x20, "f", case["frame"])
    # The frame numbers with four spare numbers of poison on each side (the first key can be read
    # one before its start).
    frames = side.alloc(2 * (len(case["frames"]) + 8), "H")
    side.put(frames + 8, "%dH" % len(case["frames"]), *case["frames"])
    index = side.alloc(4, "I")
    fraction = side.alloc(4, "F")
    side.run("uv_frame", [("r", control), ("r", frames + 8), ("u", case["count"]), ("p", index), ("p", fraction)])
    return side.finish()


def uv_frame_case(frames, frame, count=None):
    """A case of RAGetUVFrameData."""
    return dict(frames=list(frames), frame=f32(frame), count=len(frames) if count is None else count)


def uv_frame_edges():
    """Key lists of every shape against frames before, on, between and past the keys."""
    cases = []
    sets = [[], [0], [5], [0, 10], [0, 10, 20], [0, 10, 20, 30, 40], [3, 10, 20], [0, 0, 5], [10, 10, 10], [0, 20, 10, 30],
            [65535], [0, 65535]]
    frames = [-1.0, -0.0, 0.0, 0.5, 1.0, 4.9, 5.0, 5.5, 10.0, 10.5, 19.99, 20.0, 25.0, 30.0, 40.0, 41.0, 100.0,
              65534.5, 65535.0, 65536.0, 1e10, 4294967296.0, 3e9]
    for keys in sets:
        for frame in frames:
            cases.append(uv_frame_case(keys, frame))
    cases.append(uv_frame_case([0, 10, 20, 30], 25.0, count=2))
    cases.append(uv_frame_case([0, 10, 20, 30], 25.0, count=3))
    cases.append(uv_frame_case([0, 10, 20, 30], 5.0, count=1))
    cases.append(uv_frame_case([0, 10, 20, 30], 5.0, count=0))
    cases.append(uv_frame_case([0, 10, 20, 30], math.nan))
    cases.append(uv_frame_case([0, 10, 20, 30], math.inf))
    cases.append(uv_frame_case([0, 10, 20, 30], -math.inf))
    return cases


def uv_frame_random(r):
    """Random key lists (mostly sorted) and frames."""
    count = r.choice([0, 1, 2, 3, 3, 4, 5, 6, 8])
    kind = r.random()
    if kind < 0.7:
        frames = sorted(r.randrange(0, 60) for _ in range(count))
    elif kind < 0.9:
        frames = [r.randrange(0, 60) for _ in range(count)]
    else:
        frames = [r.getrandbits(16) for _ in range(count)]
    frame = r.choice([r.uniform(-2.0, 70.0), float(r.randrange(-1, 62)), r.uniform(0.0, 65535.0), r.uniform(-2.0, 70.0)])
    return uv_frame_case(frames, frame)


def scenario_texture_coords(side, case):
    """RAAnimateTextureCoords: a node animation with texture coordinate tracks laid out as the file has
    them, a node with sets of coordinates, a control at a frame and the additive flag."""
    side.set_globals(STANDARD)
    control = side.alloc(0x38, "-")
    side.put(control + 0x20, "f", case["frame"])
    keys = len(case["frames"])
    frames = side.alloc(2 * (keys + 8), "H")
    side.put(frames + 8, "%dH" % keys, *case["frames"])
    flat = [c for pair in case["pairs"] for c in pair]
    pairs = new_floats(side, flat, 8)
    tracks = side.alloc(4 * max(len(case["tracks"]), 1), "H")
    for i, (start, count) in enumerate(case["tracks"]):
        side.put(tracks + 4 * i, "HH", start, count)
    animation = side.alloc(0x14, "-")
    side.put(animation, "IIiII", len(case["tracks"]), side.p32(tracks), keys, side.p32(frames + 8), side.p32(pairs))
    sets = new_floats(side, case["uv"])
    node = side.alloc(0xA0, "-")
    side.put(node + 0x68, "I", side.p32(sets))
    node_anim = side.alloc(0x40, "-")
    side.put(node_anim + 0x2C, "I", side.p32(animation))
    side.put(node_anim + 0x3C, "I", side.p32(node))
    side.run("texture_coords", [("p", control), ("p", node_anim), ("i", case["additive"])])
    return side.finish()


def texture_case(frame, tracks, frames, pairs, uv, additive):
    """A case of RAAnimateTextureCoords: the control's frame, the tracks (first key and number of keys),
    all the tracks' key frames and (u, v) pairs, the node's coordinate sets and the additive flag."""
    return dict(frame=f32(frame), tracks=[tuple(t) for t in tracks], frames=list(frames),
                pairs=[[f32(u), f32(v)] for u, v in pairs], uv=[f32(c) for c in uv], additive=additive)


def sample_texture_case(frame, additive, track_count=4, keys_each=3):
    """Tracks laid end to end, each with keys every 10 frames from 0 and its own (u, v) pairs."""
    tracks, frames, pairs = [], [], []
    for t in range(track_count):
        tracks.append((len(frames), keys_each))
        for k in range(keys_each):
            frames.append(10 * k)
            pairs.append((0.1 * t + 0.25 * k, 0.5 - 0.125 * k + 0.01 * t))
    sets = (track_count + 3) // 4
    uv = [0.5 + 0.0625 * i for i in range(8 * sets)]
    return texture_case(frame, tracks, frames, pairs, uv, additive)


def texture_edges():
    """Frames before, on and past the keys, every additive value, track counts across the sets of four,
    tracks of 0 to 2 keys, tracks that start late or overlap."""
    cases = []
    for additive in (0, 1, 4, -1):
        for frame in (-1.0, 0.0, 3.0, 10.0, 15.5, 20.0, 25.0, 100.0):
            cases.append(sample_texture_case(frame, additive))
    for track_count in (0, 1, 2, 4, 5, 8, 9):
        for additive in (0, 1):
            cases.append(sample_texture_case(7.5, additive, track_count))
    for keys_each in (0, 1, 2):
        cases.append(sample_texture_case(7.5, 1, 4, keys_each))
        cases.append(sample_texture_case(7.5, 0, 4, keys_each))
    # A track that starts after frame 0 (the frame before its first key), overlapping tracks, NaN and infinities.
    cases.append(texture_case(2.0, [(0, 3)], [5, 10, 20], [(0.0, 0.0), (1.0, 2.0), (3.0, 4.0)], [0.25] * 8, 0))
    cases.append(texture_case(2.0, [(0, 3), (1, 2), (0, 1)], [5, 10, 20], [(0.0, 0.0), (1.0, 2.0), (3.0, 4.0)],
                              [0.25] * 8, 1))
    cases.append(texture_case(12.0, [(0, 2)], [0, 20], [(math.nan, 1.0), (2.0, math.inf)], [0.5] * 8, 1))
    cases.append(texture_case(math.nan, [(0, 2)], [0, 20], [(0.5, 1.0), (2.0, 3.0)], [0.5] * 8, 0))
    return cases


def texture_random(r):
    """Random tracks (end to end, sometimes overlapping), keys, pairs, coordinates, frame and additive
    flag."""
    track_count = r.choice([0, 1, 2, 3, 4, 4, 5, 6, 8, 9])
    frames, pairs, tracks = [], [], []
    overlap = r.random() < 0.15
    for t in range(track_count):
        count = r.choice([0, 1, 2, 3, 3, 4, 5])
        if overlap and frames and r.random() < 0.5:
            tracks.append((r.randrange(0, len(frames)), r.randrange(0, 3)))
            continue
        tracks.append((len(frames), count))
        first = r.choice([0, 0, 0, r.randrange(0, 10)])
        keys = sorted([first] + [r.randrange(first, first + 40) for _ in range(max(count - 1, 0))])[:count]
        for key in keys:
            frames.append(key)
            pairs.append((rval(r, 0.04, -2.0, 2.0), rval(r, 0.04, -2.0, 2.0)))
    # The tracks may name keys past the end of what was built: keep them inside the arrays.
    tracks = [(min(start, len(frames)), min(count, len(frames) - min(start, len(frames)))) for start, count in tracks]
    sets = max((track_count + 3) // 4, 1)
    uv = [rval(r, 0.04, -2.0, 2.0) for _ in range(8 * sets)]
    frame = r.choice([r.uniform(-2.0, 45.0), float(r.randrange(-1, 45)), r.uniform(-2.0, 45.0)])
    return texture_case(frame, tracks, frames, pairs, uv, r.choice([0, 0, 1, 4, -7]))


# ------------------------------------------------------------------ the tests

TESTS = [
    make_test("RAInterpPosnAtFrame", scenario_posn, posn_edges, posn_random),
    make_test("RABezierInterFromShape", scenario_shape("bezier"), indexed(ring_edges(True)), ring_random),
    make_test("RABezierTangentFromShape", scenario_shape("bezier_tangent"), indexed(ring_edges(True)), ring_random),
    make_test("RALinearInterFromShape", scenario_shape("linear"), indexed(line_edges()), line_random),
    make_test("RALinearTangentFromShape", scenario_shape("linear_tangent"), indexed(line_edges()), line_random),
    make_test("RACatmulInterFromShape", scenario_shape("catmul"), indexed(ring_edges(True)), ring_random),
    make_test("SetLMatrixOrientation", scenario_orientation, orientation_edges, orientation_random),
    make_test("SetLMatrixScaleNoTrans", scenario_scale, scale_edges, scale_random),
    make_test("SetLMatrixXZOrientation", scenario_xz, xz_edges, xz_random),
    make_test("CombineMatricesNoWDest2nd (s3DAxis)", scenario_combine, combine_edges, combine_random),
    make_test("RAQuaternionToLAxis", scenario_quaternion_axis, quaternion_axis_edges, quaternion_axis_random),
    make_test("RASphericalLinearInterpolate", scenario_slerp, slerp_edges, slerp_random),
    make_test("RACalcOrientationBetweenControlPoints", scenario_calc_orientation, calc_edges, calc_random),
    make_test("RAGetUVFrameData", scenario_uv_frame, uv_frame_edges, uv_frame_random),
    make_test("RAAnimateTextureCoords", scenario_texture_coords, texture_edges, texture_random),
]
