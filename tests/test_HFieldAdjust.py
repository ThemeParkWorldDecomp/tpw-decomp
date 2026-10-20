"""CHField::AdjustHField (engine 0x1000c0e8): 27 fixed rectangles of the height copy that
CHField::Init makes (mF12970) are raised by 4.0, 1.0 or 1.4, each grid point tested against
the record's counts (sHField mF18, unsigned). The buffer holds four words before the grid,
17 rows (every row the function can reach) and 72 words after, so a write off the grid
shows; the object and the record must not change. The function calls nothing, so both call
logs stay empty. A column count of 0xffffffff makes the row length wrap to 0: every row
lands on row 0, which also exposes the order of the rectangles."""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_HFieldAdjust"
ENTRY = 0x1000c0e8
OBJECT_SIZE = 0x12974   # CHField on the Mac
RECORD_SIZE = 0x30      # sHField
BEFORE, ROWS, AFTER = 4, 17, 72
WIDE = 0xffffffff       # a column count whose row length wraps to 0
# Height words that test rounding, signed zeros, NaNs, infinities and denormals.
SPECIALS = [0x00000000, 0x80000000, 0xc0800000, 0xbf800000, 0xbfb33333, 0x7fc00000, 0x7f800001,
            0xffc00001, 0x7f800000, 0xff800000, 0x00000001, 0x807fffff, 0x00800000, 0x7f7fffff,
            0xff7fffff, 0x4b800000, 0x4b7fffff, 0x3f7fffff, 0x33800000, 0x4c000000, 0x4b000001]
# Big-endian words into and out of emulator memory.
def ws(e, a, v): e.write(a, struct.pack(">%dI" % len(v), *v))
def rs(e, a, n): return list(struct.unpack(">%dI" % n, e.read(a, n * 4)))
# NaNs compare as one value (the two sides may quiet a signalling NaN differently).
def canon(v): return 0x7fc00000 if v & 0x7f800000 == 0x7f800000 and v & 0x007fffff else v
# Words in the buffer for a column count: the grid's 17 rows (69 words when rows wrap to 0).
def size(count_x):
    row = (count_x + 1) & 0xffffffff
    return BEFORE + (69 if row == 0 else ROWS * row) + AFTER
# One random height word: moderate, any magnitude, any pattern, special, or near a raise.
def word(r):
    k = r.random()
    if k < 0.35: return T.f32_bits(r.uniform(-100.0, 100.0))
    if k < 0.55: return T.f32_bits(r.uniform(-1.0, 1.0) * 10.0 ** r.uniform(-44.0, 38.0))
    if k < 0.70: return r.getrandbits(32)
    if k < 0.85: return r.choice(SPECIALS)
    return T.f32_bits(-r.choice([4.0, 1.0, 1.4, 5.0, 2.4, 6.4]) * (1.0 + r.uniform(-1e-6, 1e-6)))
# A case: the two counts and the whole buffer (random heights, or one value everywhere).
def case(count_x, count_z, r=None, fill=None):
    n = size(count_x)
    return [count_x, count_z, [fill] * n if fill is not None else [word(r) for _ in range(n)]]
# The original on a 0xa5-filled object and record; returns the buffer, both checks and the log.
def ppc(e, c):
    count_x, count_z, words = c
    e.cl = CL.PpcLog({})
    field = e.alloc(OBJECT_SIZE, b"\xa5" * OBJECT_SIZE)
    record = e.alloc(RECORD_SIZE, b"\xa5" * RECORD_SIZE)
    ws(e, record + 0x18, [count_x, count_z])
    buffer = e.alloc(4 * len(words))
    ws(e, buffer, words)
    ws(e, field, [record])
    ws(e, field + 0x12970, [buffer + 4 * BEFORE])
    field_before, record_before = e.read(field, OBJECT_SIZE), e.read(record, RECORD_SIZE)
    st = CL.Stubs()
    st.real.add(ENTRY)   # lift the stand-in test_HeightfieldInit puts on it
    old = st.activate(e)
    try:
        T.call_ppc(e, ENTRY, [field])
    finally:
        CL.Stubs.deactivate(e, old)
    return {"heights": [canon(v) for v in rs(e, buffer, len(words))],
            "object unchanged": int(e.read(field, OBJECT_SIZE) == field_before),
            "record unchanged": int(e.read(record, RECORD_SIZE) == record_before),
            "log": e.cl.log}
# The translation through tpw_ha_run (tests/unit_HFieldAdjust.cpp), same keys.
def host(lib, c):
    count_x, count_z, words = c
    CL.host_begin(lib, {})
    fn = lib.tpw_ha_run
    fn.argtypes = [C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32), C.c_int32]
    fn.restype = C.c_int32
    buffer = (C.c_uint32 * len(words))(*words)
    same = fn(count_x, count_z, buffer, BEFORE)
    return {"heights": [canon(v) for v in buffer],
            "object unchanged": same & 1,
            "record unchanged": same >> 1 & 1,
            "log": CL.host_end(lib)}
# Edge cases: every bound the rectangles test, wrapping rows, and one value everywhere.
def edges():
    r = random.Random(4)
    # Every column edge the rectangles have, on a grid with all 17 rows.
    for count_x in [0, 1, 26] + list(range(27, 71)) + [100]:
        yield case(count_x, 16, r)
    # Every row edge, on grids wide enough for every column.
    for count_z in range(0, 19):
        yield case(68, count_z, r)
        yield case(69, count_z, r)
    # Row counts that are negative when read signed.
    for count_z in (0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff):
        yield case(70, count_z, r)
    # Rows that wrap onto row 0.
    for count_z in (0, 4, 6, 8, 9, 10, 12, 16, 0x80000000, 0xffffffff):
        yield case(WIDE, count_z, r)
    # One value everywhere.
    for fill in SPECIALS:
        yield case(70, 16, fill=fill)
        yield case(WIDE, 16, fill=fill)
# Random cases: mostly column counts around the rectangles, sometimes huge counts.
def rnd(r):
    k = r.random()
    count_x = WIDE if k < 0.08 else r.randrange(26, 71) if k < 0.7 else r.randrange(0, 101)
    k = r.random()
    count_z = r.choice([0x7fffffff, 0x80000000, 0xffffffff, r.getrandbits(32)]) if k < 0.08 else r.randrange(0, 21)
    return case(count_x, count_z, r)
TESTS = [FnTest("Height field adjustment", ppc, host, edge=edges, rnd=rnd)]
