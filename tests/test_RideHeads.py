"""Differential tests of the ride heads (unit w63), against libtpw_unit_RideHeads.dylib (tests/unit_RideHeads.cpp):

  src/TPEmbed_heads.cpp  TickRideHeads 0x10200460
  src/rsse_matrix.cpp    GetLMatrixRotationAroundZAxisYOnly 0x100c0fc8

TickRideHeads runs as the original on the PowerPC side, with InvertMatrix, CombineMatricesNoWDest2nd, SetMatrixIdentity and
SetLMatrixOrientation real on both sides. The C library's sqrt and atan2 and the class function __fpclassifyf are stubs
whose host side is the host's libm (Python's math is the same library). The script setters SCR_SetScriptPosition,
SCR_SetVariable and SCR_SetFloatVariable are fakes on both sides that record every argument (floats as bits). The sine
table is made by BuildGeometryTables on the host and its words are put in the original's TPGlobals.

A case is a mesh whose header has full nodes then mini nodes, an embedded set and records with a 16-word head block
each. What is compared: the recorded calls in order, the two statics (the float and the byte flag), and the sixteen words of
every record's block after the call (the mirror flips them in place). The local matrices are not observable.

Two things the original leaves undefined, pinned so that both sides agree: the direction search keeps the previous head's
row, column and winning direction when no sample beats 0 (and the NaN path keeps the winning direction), and the first
head sees whatever the caller's registers r30 and r31 and the stack word hold; the PowerPC side is given zeros there.

Unicorn computes the single-precision fused instructions in double precision and rounds twice; HardwareRounding (a code
hook per fused instruction, as in test_LobbyLeaves2.py) gives each the processor's single rounding.
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

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideHeads"

MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000

TICK_HEADS = 0x10200460
ZROT = 0x100C0FC8
INVERT, LU_DECOMP, LU_BACKSUBST, COMBINE, SET_ORIENTATION = 0x100C249C, 0x100C2230, 0x100C210C, 0x100C28FC, 0x100C1EAC
SCR_POSITION, SCR_VARIABLE, SCR_FLOAT = 0x100D932C, 0x100D9514, 0x100D95A0

TPGLOBALS = 0x103B74C8
CAMERA_POINTER = 0x102B5AF0
VARIABLE8, VARIABLE8_SET = 0x10396A9C, 0x10396A98
FRAME_BOTTOM = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400   # call_ppc's r1; TickRideHeads takes 0x200 more
BEST_DIRECTION = FRAME_BOTTOM - 0x200 + 0x84                             # its winning direction's three words

INF, NAN = math.inf, math.nan


# ------------------------------------------------------------------ small helpers


def fb(x):
    """The bit pattern of a single-precision value (a NaN as one pattern)."""
    if x != x:
        return NAN_BITS
    try:
        return struct.unpack(">I", struct.pack(">f", x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


def bf(bits):
    """The single-precision value of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def canon(bits):
    """A word with every NaN pattern made one (the processors differ in the NaNs they make)."""
    bits &= MASK
    return NAN_BITS if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def f32(x):
    return T.f32(x)


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the code under test and nowhere
    else (as in test_LobbyLeaves2.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to single, which can differ by one
      unit in the last place from the processor's single rounding of the exact result; the result is replaced by the C
      library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor saturates to 0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just before the next one."""

    changed = 0    # results the correction replaced (all cases so far)
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
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
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


def function_range(e, address):
    """(begin, end) of the function at `address` in the program's function table."""
    for f in e.functions:
        if f["entry"] == address:
            return address, address + f["size"]
    raise KeyError(hex(address))


# ------------------------------------------------------------------ matrices (rows of four floats, row 3 the position)


def identity():
    return [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0]


def translation(x, y, z):
    m = identity()
    m[12], m[13], m[14] = x, y, z
    return m


def rotation(axis, angle):
    """A rotation about the x, y or z axis (row vectors), from the host's exact cosine and sine."""
    c, s = f32(math.cos(angle)), f32(math.sin(angle))
    m = identity()
    if axis == 0:
        m[5], m[6], m[9], m[10] = c, s, -s, c
    elif axis == 1:
        m[0], m[2], m[8], m[10] = c, -s, s, c
    else:
        m[0], m[1], m[4], m[5] = c, s, -s, c
    return m


def product(a, b):
    """a * b for row-vector matrices (rounded to single)."""
    return [f32(sum(a[4 * r + k] * b[4 * k + c] for k in range(4))) for r in range(4) for c in range(4)]


def scaling(x, y, z):
    m = identity()
    m[0], m[5], m[10] = x, y, z
    return m


def words(matrix):
    """The sixteen words of a matrix given as floats; a word given as an int stays as it is."""
    return [x & MASK if isinstance(x, int) else fb(x) for x in matrix]


# ------------------------------------------------------------------ the case


def head(matrix, flags=2, handle=7, block=True):
    """A record: its flags, the script handle (word +0xc) and the head's 16-word block (None: no block)."""
    return {"flags": flags & MASK, "handle": handle & MASK, "block": words(matrix) if block else None}


def case(heads, cam=None, size=0x1000, flag=0, static=0.0, set_flags=4, node_flags=None, full=None, mini=0, first=0):
    """heads: the records. node_flags: the flags of each node (full nodes first); the default gives every node 0. full:
    the number of full nodes (default: all the nodes are full); first: the embedded entry 0's node."""
    count = len(heads)
    nodes = first + count if node_flags is None else max(len(node_flags), first + count)
    if node_flags is None:
        node_flags = [0] * nodes
    node_flags = list(node_flags) + [0] * (nodes - len(node_flags))
    if full is None:
        full = nodes - mini
    mini = nodes - full
    return {"size": size, "flag": flag, "static": static if isinstance(static, int) else fb(static), "cam": words(cam or identity()),
            "set_flags": set_flags & MASK, "count": count, "full": full, "mini": mini, "first": first,
            "node_flags": [f & MASK for f in node_flags], "heads": heads}


def host_numbers(c):
    n = [c["size"], c["flag"], c["static"]] + c["cam"] + [c["set_flags"], c["count"], c["full"], c["mini"], c["first"]]
    n += c["node_flags"]
    for h in c["heads"]:
        n += [h["flags"], h["handle"], 1 if h["block"] is not None else 0] + (h["block"] or [0xA5A5A5A5] * 16)
    return n


# ------------------------------------------------------------------ the two sides


STUBS = None
TABLES = {}          # table size -> words (size, mask, scale bits, entries), made by the host's BuildGeometryTables
LAST_TABLE = [None]  # the size the original's TPGlobals holds


def table(lib, size):
    if size not in TABLES:
        out = (C.c_uint32 * (3 + size))()
        lib.tpw_rh_table(size, out)
        TABLES[size] = list(out)
    return TABLES[size]


def norm_log(log):
    """The log with every float bit pattern that is a NaN made one."""
    out = []
    for name, ints, texts in log:
        ints = list(ints)
        if name == "SCR_SetScriptPosition":
            for k in (1, 2, 3):
                ints[k] = canon(ints[k])
        elif name == "SCR_SetFloatVariable":
            ints[2] = canon(ints[2])
        out.append((name, tuple(ints), texts))
    return out


def ws(e, address, *values):
    e.write(address, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def make_stubs():
    """The C library's functions (sqrt is installed by install_std_stubs) and the three script setters, which record."""
    S = CL.Stubs()
    S.real.update([TICK_HEADS, ZROT, INVERT, LU_DECOMP, LU_BACKSUBST, COMBINE, SET_ORIENTATION, 0x100C0E28])
    S.internal(SCR_POSITION, CL.recorder("SCR_SetScriptPosition", "ifffi"))
    S.internal(SCR_VARIABLE, CL.recorder("SCR_SetVariable", "iui"))
    S.internal(SCR_FLOAT, CL.recorder("SCR_SetFloatVariable", "iuf"))
    return S


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    e.stub("atan2", lambda emu: emu.set_f1(math.atan2(emu.f(1), emu.f(2))))

    def classify(emu):
        x = emu.f(1)
        emu.set_r(3, 1 if x != x else (2 if math.isinf(x) else (3 if x == 0.0 else 4)))

    e.stub("__fpclassifyf", classify)
    STUBS = make_stubs()
    lib = runner.lib
    RUNNER_LIB[0] = lib
    lib.tpw_rh_table.argtypes = [C.c_uint32, C.c_void_p]
    lib.tpw_rh_zrot.argtypes = [C.c_void_p]
    lib.tpw_rh_zrot.restype = C.c_float


def ppc_tick(e, c):
    saved = STUBS.activate(e)
    kept = [(a, e.read(a, n)) for a, n in ((CAMERA_POINTER, 4), (VARIABLE8, 4), (VARIABLE8_SET, 4))]
    try:
        e.cl = CL.PpcLog({})
        words_of_table = table(RUNNER_LIB[0], c["size"])
        ws(e, TPGLOBALS, *words_of_table)
        block = b"\xa5"
        mesh = e.alloc(0x40, block * 0x40)
        header = e.alloc(0xB8, block * 0xB8)
        nodes = e.alloc(0xA0 * (c["full"] + 1), block * (0xA0 * (c["full"] + 1)))
        minis = e.alloc(0x58 * (c["mini"] + 1), block * (0x58 * (c["mini"] + 1)))
        for i, f in enumerate(c["node_flags"]):
            ws(e, nodes + 0xA0 * i if i < c["full"] else minis + 0x58 * (i - c["full"]), f)
        e.write(header + 0x44, struct.pack(">HHH", c["full"], c["first"], c["count"]))
        ws(e, header + 0x70, nodes, minis)
        records = e.alloc(0x14 * max(c["count"], 1), block * (0x14 * max(c["count"], 1)))
        blocks = []
        for i, h in enumerate(c["heads"]):
            b = e.alloc(0x40, block * 0x40)
            if h["block"] is not None:
                ws(e, b, *h["block"])
            blocks.append(b)
            ws(e, records + 0x14 * i, h["flags"], b if h["block"] is not None else 0, 0, h["handle"], 0)
        record_set = e.alloc(0x14, block * 0x14)
        ws(e, record_set, c["set_flags"], records)
        ws(e, mesh + 4, header)
        ws(e, mesh + 0x28, record_set)
        cell = e.alloc(4, b"\0\0\0\0")
        ws(e, cell, mesh)
        camera = e.alloc(0x40, block * 0x40)
        ws(e, camera, *c["cam"])
        ws(e, CAMERA_POINTER, camera)
        ws(e, VARIABLE8, c["static"])
        e.write(VARIABLE8_SET, struct.pack(">B", c["flag"]))
        # What the caller leaves: zeros in the two registers and the stack words that hold the winning direction.
        ws(e, BEST_DIRECTION, 0, 0, 0)
        e.set_r(30, 0)
        e.set_r(31, 0)
        ranges = [function_range(e, a) for a in (TICK_HEADS, INVERT, LU_DECOMP, LU_BACKSUBST, COMBINE, SET_ORIENTATION)]
        with HardwareRounding(e, ranges):
            T.call_ppc(e, TICK_HEADS, [cell])
        out = [canon(e.u32(VARIABLE8)), e.read(VARIABLE8_SET, 1)[0]]
        for i, h in enumerate(c["heads"]):
            out += [canon(w) for w in struct.unpack(">16I", e.read(blocks[i], 0x40))]
        return {"out": out, "log": norm_log(e.cl.log)}
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, data in reversed(kept):
            e.write(a, data)


RUNNER_LIB = [None]


def host_tick(lib, c):
    RUNNER_LIB[0] = lib
    lib.tpw_rh_reset()
    CL.host_begin(lib, {})
    numbers = host_numbers(c)
    data = (C.c_int64 * len(numbers))(*numbers)
    out = (C.c_int64 * (2 + 16 * 4))()
    lib.tpw_rh_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    n = lib.tpw_rh_run(data, out)
    result = [canon(out[0]), out[1] & 0xFF] + [canon(w) for w in list(out)[2:n]]
    return {"out": result, "log": norm_log(CL.host_end(lib))}


# ------------------------------------------------------------------ the cases

DIRECTIONS = [(x, y, z) for x in (-1.0, 0.0, 1.0) for y in (-1.0, 0.0, 1.0) for z in (-1.0, 0.0, 1.0) if (x, y, z) != (0.0, 0.0, 0.0)]
CAMERAS = {
    "identity": identity(),
    "translated": translation(3.0, -2.0, 5.0),
    "rotx": rotation(0, 0.6),
    "roty": rotation(1, -1.1),
    "rotz": rotation(2, 2.5),
    "combined": product(product(rotation(0, 0.4), rotation(1, 0.9)), translation(1.5, 2.0, -3.0)),
}


def at(x, y, z, **kw):
    """One head whose position is (x, y, z), turned like the world (so with an identity camera the direction to the
    camera is minus the position)."""
    return case([head(translation(x, y, z))], **kw)


def edge_flags():
    cases = []
    for set_flags in (0, 4, 1, 3, 8, 0xC, 0xFFFFFFFB, 0xFFFFFFFF, 5, 0x80000004):
        cases.append(case([head(translation(1.0, 2.0, 3.0)), head(translation(-2.0, 1.0, 0.5), handle=9)], set_flags=set_flags))
    flags = (0, 2, 1, 3, 0xFFFFFFFD, 0xFFFFFFFF, 0x10, 0x12)
    for a in flags:
        for b in flags:
            cases.append(case([head(translation(1.0, 2.0, 3.0), flags=a, handle=3, block=bool(a & 2)),
                               head(translation(-4.0, 0.5, 2.0), flags=b, handle=4, block=bool(b & 2))]))
    for count in range(5):
        cases.append(case([head(translation(float(i + 1), 2.0, -1.0 - i), handle=10 + i) for i in range(count)]))
    for pattern in range(16):
        cases.append(case([head(translation(float(i + 1), 2.0, -1.0 - i), flags=2 if pattern >> i & 1 else 1, handle=20 + i,
                                block=bool(pattern >> i & 1)) for i in range(4)]))
    return cases


NODE_FLAG_VALUES = (0, 0x400, 0x8000, 0x8400)
LAYOUTS = ((3, 0, 0), (0, 3, 0), (2, 2, 1), (1, 4, 0), (3, 3, 2), (5, 0, 2))   # (full nodes, mini nodes, first entry's node)


def edge_nodes():
    """Every combination of node flags for three heads, for entries inside and across the full/mini boundary."""
    cases = []
    for full, mini, first in LAYOUTS:
        for a in NODE_FLAG_VALUES:
            for b in NODE_FLAG_VALUES:
                for d in NODE_FLAG_VALUES:
                    flags = [0x55555555] * (first + 3 + mini)
                    flags[first], flags[first + 1], flags[first + 2] = a, b, d
                    cases.append(case([head(rotation(1, 0.3 * (k + 1)) if k else translation(1.0, 2.0, 3.0), handle=30 + k)
                                       for k in range(3)], node_flags=flags, full=full, first=first))
    # Whole-node flags (the mirrored bit and the hidden bit among others) and the bit just below and above each.
    for value in (0xFFFFFFFF, 0x7FFFFBFF, 0xFFFF7FFF, 0xFFFF73FF, 0x200, 0x4000, 0x10000, 0x800, 0x8000 | 0x400 | 0x1):
        cases.append(case([head(translation(1.0, -2.0, 3.0))], node_flags=[value]))
    return cases


def edge_positions():
    cases = []
    for name, cam in CAMERAS.items():
        for scale in (1.0, 1e6, 1e-6):
            for d in DIRECTIONS:
                cases.append(at(-d[0] * scale, -d[1] * scale, -d[2] * scale, cam=cam))
    return cases


def edge_heads():
    cases = []
    positions = ((1.0, 2.0, 3.0), (-2.0, 0.5, 4.0), (0.0, -3.0, 1.0))
    for axis in range(3):
        for angle in (0.5235988, 0.7853982, 1.5707964, 3.1415927, 0.3):
            for p in positions:
                m = product(rotation(axis, angle), translation(*p))
                cases.append(case([head(m)]))
                cases.append(case([head(m)], node_flags=[0x400]))
    cases.append(case([head(product(rotation(0, 0.4), product(rotation(1, 0.9), translation(2.0, 3.0, -1.0))))], cam=CAMERAS["combined"]))
    for s in ((2.0, 2.0, 2.0), (0.5, 0.5, 0.5), (1e-3, 1.0, 1e3), (-1.0, 1.0, 1.0), (1.0, -1.0, -1.0), (3.0, 0.25, 7.0)):
        cases.append(case([head(product(scaling(*s), translation(1.0, -2.0, 3.0)))]))
        cases.append(case([head(product(scaling(*s), translation(1.0, -2.0, 3.0)))], node_flags=[0x400]))
    # Singular, zero and special matrices.
    cases.append(case([head([0.0] * 16)]))
    cases.append(case([head(scaling(1.0, 0.0, 1.0))]))
    cases.append(case([head(scaling(0.0, 0.0, 0.0))]))
    cases.append(case([head(translation(0.0, 0.0, 0.0))]))
    cases.append(case([head(translation(0.0, 0.0, 0.0))], cam=translation(1.0, 0.0, 0.0)))
    cases.append(case([head(translation(1.0, 2.0, 3.0))], cam=translation(1.0, 2.0, 3.0)))
    cases.append(case([head(translation(NAN, 1.0, 1.0))]))
    cases.append(case([head(translation(1.0, NAN, 1.0))]))
    cases.append(case([head(translation(1.0, 1.0, NAN))]))
    cases.append(case([head(identity()[:5] + [NAN] + identity()[6:])]))
    cases.append(case([head(translation(1.0, 1.0, 1.0))], cam=translation(NAN, 0.0, 0.0)))
    cases.append(case([head([0x7FA00000 | 1] + words(identity())[1:])]))   # a signalling NaN
    cases.append(case([head(translation(INF, 0.0, 0.0))]))
    cases.append(case([head(translation(0.0, 1e20, 0.0))]))   # the squares overflow in single: the sum is infinity
    cases.append(case([head(translation(1e20, 1.0, 1.0))]))
    cases.append(case([head(translation(0.0, 0.0, -1e20))]))
    cases.append(case([head(translation(1e-30, 0.0, 0.0))]))
    cases.append(case([head(translation(0.0, 1e-30, 0.0))]))
    cases.append(case([head(translation(1e-30, 1e-30, 1e-30))]))
    cases.append(case([head(translation(1e-40, 0.0, 0.0))]))
    cases.append(case([head(translation(3e38, 0.0, 0.0))]))
    cases.append(case([head(translation(3e38, 3e38, 3e38))]))
    return cases


def edge_directions():
    """Positions exactly on the sample directions' poles and axes, exactly between two samples (a tie keeps the first),
    and on the other side of each sample."""
    cases = []
    s2 = f32(math.sqrt(0.5))
    s3 = f32(math.sqrt(3.0) / 2.0)
    c30 = f32(0.5)
    targets = [(0.0, 1.0, 0.0), (0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 0.0, -1.0),
               (s2, 0.0, s2), (-s2, 0.0, s2), (s2, 0.0, -s2), (-s2, 0.0, -s2), (0.0, s2, s2), (0.0, -s2, s2),
               (s3, c30, 0.0), (0.0, c30, s3), (s3 * s2, c30, s3 * s2), (-s3 * s2, c30, -s3 * s2), (s3, -c30, 0.0),
               (0.0, s3, c30), (0.0, -s3, -c30), (0.5, 0.5, 0.5), (-0.5, 0.5, -0.5), (1.0, 1.0, 0.0), (1.0, 0.0, 1.0),
               (0.0, 1.0, 1.0), (1.0, 1.0, 1.0), (-1.0, -1.0, -1.0)]
    for d in targets:
        for k in (1.0, 2.0, 100.0):
            cases.append(at(-d[0] * k, -d[1] * k, -d[2] * k))
    # Nearly the same as a tie: one unit in the last place off the pole and off the x axis.
    for off in (1e-7, -1e-7, 1e-6, -1e-6, 1e-4, 3e-8):
        cases.append(at(off, -1.0, 0.0))
        cases.append(at(0.0, -1.0, off))
        cases.append(at(-1.0, off, 0.0))
        cases.append(at(0.0, 1.0, -off))
        cases.append(at(off, 0.0, 1.0))
    return cases


def sample_directions():
    """The 56 directions the search samples, as the ideal (double precision) turns give them: row r is the start (0, 1, 0)
    turned r times by pi / 6 about the x axis, column c that turned c times by pi / 4 about the y axis."""
    out = []
    vx, vy, vz = 0.0, 1.0, 0.0
    for _ in range(7):
        for _ in range(8):
            out.append((vx, vy, vz))
            a, b = math.sin(-math.pi / 4), math.sin(math.pi / 4)
            vx, vz = vx * b + vz * a, -vx * a + vz * b
        p, q = math.sin(math.pi / 6), math.sin(2 * math.pi / 3)
        vy, vz = vy * q - vz * p, vy * p + vz * q
    return out


def edge_samples():
    """Directions exactly on a sample (its neighbours then tie in exact arithmetic, and the rounding of each dot product
    decides), a little off it, and half way between samples, at a few distances. The camera's position is the direction (the
    head is the identity), so the direction to the camera is the one given."""
    cases = []
    samples = sample_directions()
    for k in (1.0, 3.7, 1000.0):
        for (x, y, z) in samples:
            cases.append(case([head(identity())], cam=translation(f32(x * k), f32(y * k), f32(z * k))))
    for i, (x, y, z) in enumerate(samples):
        nx, ny, nz = samples[(i + 1) % len(samples)]
        mid = (x + nx, y + ny, z + nz)
        cases.append(case([head(identity())], cam=translation(f32(mid[0] * 2.0), f32(mid[1] * 2.0), f32(mid[2] * 2.0))))
        mx, my, mz = samples[(i + 8) % len(samples)]
        cases.append(case([head(identity())], cam=translation(f32((x + mx) * 1.5), f32((y + my) * 1.5), f32((z + mz) * 1.5))))
        for scale in (1.0000001, 0.9999999):
            cases.append(case([head(identity())], cam=translation(f32(x * scale), f32(y * scale), f32(z * scale))))
    return cases


def edge_statics():
    cases = []
    for flag in (0, 1, 0xFF, 0x80, 0x7F, 2):
        for static in (0.0, 0.685, 1.0, -3.5, NAN, INF, 0x3F2F5C29):
            for count in (0, 1, 3):
                cases.append(case([head(translation(1.0 + i, -2.0, 3.0), handle=40 + i) for i in range(count)], flag=flag,
                                  static=static if not isinstance(static, int) else static))
    # A hidden head does not set the static; the first visible head after it does.
    cases.append(case([head(translation(1.0, 2.0, 3.0)), head(translation(2.0, 2.0, 3.0))], node_flags=[0x8000, 0]))
    cases.append(case([head(translation(1.0, 2.0, 3.0)), head(translation(2.0, 2.0, 3.0))], node_flags=[0, 0x8000], static=2.5))
    return cases


def edge_tables():
    cases = []
    positions = ((-1.0, -2.0, -3.0), (3.0, -1.0, 2.0), (0.0, -1.0, 0.0), (-1.0, 0.0, 1.0), (0.7, 0.2, -0.5), (1.0, 1.0, 1.0))
    for size in (16, 8, 4, 0x100, 0x800, 0x1000, 32):
        for p in positions:
            cases.append(at(*p, size=size))
        cases.append(at(*positions[0], size=size, cam=CAMERAS["combined"]))
        cases.append(case([head(product(rotation(1, 0.7), translation(*positions[1])))], size=size))
    return cases


def multi_heads(r):
    """Several heads in one call: their winning samples are kept between them."""
    cases = []
    for k in range(40):
        count = r.randrange(2, 5)
        heads = [head(product(rotation(r.randrange(3), f32(r.uniform(-3, 3))),
                              translation(f32(r.uniform(-5, 5)), f32(r.uniform(-5, 5)), f32(r.uniform(-5, 5)))),
                      handle=50 + i) for i in range(count)]
        cases.append(case(heads, cam=r.choice(list(CAMERAS.values())), flag=r.choice([0, 1])))
    # The same position again, then the NaN path, then a normal head.
    cases.append(case([head(translation(1.0, 2.0, 3.0)), head(translation(0.0, 0.0, 0.0)), head(translation(-3.0, 1.0, 2.0))]))
    cases.append(case([head(translation(0.0, 0.0, 0.0)), head(translation(-3.0, 1.0, 2.0))]))
    cases.append(case([head(translation(1.0, 2.0, 3.0)), head(translation(0.0, 0.0, 0.0)), head(translation(0.0, 0.0, 0.0))]))
    cases.append(case([head(translation(1.0, 2.0, 3.0)), head(translation(0.0, 1e20, 0.0)), head(translation(-3.0, 1.0, 2.0))]))
    cases.append(case([head(translation(0.0, 1e20, 0.0)), head(translation(1.0, 2.0, 3.0))]))
    return cases


def edge_overflow_axes():
    """Triage 2026-10-13b (doubtful 3: the NaN test looks at x / length only). A head whose direction to the camera has ONE
    component that overflows single precision (the scale of its matrix times a camera position near the largest float): the
    length is infinity, x / length is 0 and y / length (or z / length) is NaN, so the search runs with a NaN dot and keeps
    the previous head's sample, where a test of the wrong component would take the NaN path (row 0, column 0). A first
    head with a plain direction gives the second something to keep."""
    cases = []
    for big in (3e38, -3e38):
        for scale in ((1.0, 0.25, 1.0), (1.0, 1.0, 0.25), (0.25, 1.0, 1.0), (1.0, 0.25, 0.25), (0.25, 0.25, 1.0)):
            for first in (translation(0.0, 0.0, 0.0), rotation(0, 0.9), rotation(1, 2.0)):
                for cam in (translation(big, big, 0.0), translation(0.0, big, big), translation(big, 0.0, big),
                            translation(big, 1.0, 2.0), translation(1.0, big, 2.0), translation(1.0, 2.0, big)):
                    cases.append(case([head(first, handle=1), head(scaling(*scale), handle=2)], cam=cam))
                    cases.append(case([head(scaling(*scale), handle=2), head(first, handle=1)], cam=cam))
    return cases


def edges():
    r = random.Random(63)
    return (edge_flags() + edge_nodes() + edge_positions() + edge_heads() + edge_directions() + edge_samples() + edge_statics() + edge_tables()
            + multi_heads(r) + edge_overflow_axes())


def rpos(r, reach=50.0):
    return f32(r.uniform(-reach, reach)) if r.random() < 0.9 else r.choice([0.0, -0.0, 1.0, -1.0, 1e-6, 1e6, -1e6, 1e-3])


def rmatrix(r):
    """A random head matrix: a rotation about each axis in turn, a scale, a position; sometimes a plain random one."""
    if r.random() < 0.15:
        return [f32(r.uniform(-3, 3)) if r.random() > 0.1 else r.choice([0.0, 1.0, -1.0, 1e-6]) for _ in range(16)]
    m = product(product(rotation(0, f32(r.uniform(-4, 4))), rotation(1, f32(r.uniform(-4, 4)))), rotation(2, f32(r.uniform(-4, 4))))
    if r.random() < 0.3:
        m = product(scaling(f32(r.uniform(0.2, 3)), f32(r.uniform(0.2, 3)), f32(r.uniform(0.2, 3))), m)
    m[12], m[13], m[14] = rpos(r), rpos(r), rpos(r)
    return m


def random_case(r):
    count = r.choice([0, 1, 1, 2, 3, 4])
    heads = []
    for i in range(count):
        flags = r.choice([2, 2, 2, 3, 0, 1, 0x12, r.getrandbits(32)])
        heads.append(head(rmatrix(r), flags=flags, handle=r.choice([1, 2, 77, 0xFFFFFFFF, r.getrandbits(32)]),
                          block=bool(flags & 2) or r.random() < 0.5))
    first = r.randrange(0, 3)
    nodes = first + count
    full = r.randrange(0, nodes + 1) if nodes else 0
    cam = r.choice(list(CAMERAS.values()) + [rmatrix(r)])
    return case(heads, cam=cam, size=r.choice([0x1000, 0x1000, 0x1000, 16, 0x100]), flag=r.choice([0, 0, 1, 0xFF]),
                static=r.choice([0.0, 0.685, f32(r.uniform(-5, 5))]),
                set_flags=r.choice([4, 4, 4, 4, 0, 5, r.getrandbits(32)]),
                node_flags=[r.choice([0, 0, 0x400, 0x8000, 0x8400, r.getrandbits(32)]) for _ in range(nodes)], full=full, first=first)


# ------------------------------------------------------------------ GetLMatrixRotationAroundZAxisYOnly


def ppc_zrot(e, w):
    saved = STUBS.activate(e)
    try:
        matrix = e.alloc(0x40, b"\xa5" * 0x40)
        ws(e, matrix, *w)
        return {"angle": canon(fb(T.call_ppc(e, ZROT, [matrix])[2]))}
    finally:
        CL.Stubs.deactivate(e, saved)


def host_zrot(lib, w):
    data = (C.c_uint32 * 16)(*w)
    return {"angle": canon(fb(lib.tpw_rh_zrot(data)))}


def zrot_edges():
    cases = []
    values = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 1e-6, 1e6, -1e6, 1e-40, 3e38, -3e38, INF, -INF, NAN]
    for a in values:
        for b in values:
            m = [0.0] * 16
            m[4], m[5] = a, b
            cases.append(words(m))
    cases.append(words(identity()))
    cases.append(words(rotation(2, 0.7)))
    cases.append(words(rotation(2, -2.9)))
    return cases


def zrot_random(r):
    m = [f32(r.uniform(-10, 10)) for _ in range(16)]
    if r.random() < 0.2:
        m[4], m[5] = T.rfloat(r), T.rfloat(r)
    return words(m)


TESTS = [
    FnTest("TickRideHeads", ppc_tick, host_tick, edge=edges, rnd=random_case),
    FnTest("GetLMatrixRotationAroundZAxisYOnly", ppc_zrot, host_zrot, edge=zrot_edges, rnd=zrot_random),
]
