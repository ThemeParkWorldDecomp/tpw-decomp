"""Unit w71b, the MPEG audio decoder, part 2 (synthesis and layer II), against libtpw_unit_MpegSynth.dylib
(tests/unit_MpegSynth.cpp), in the sound library ("sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0):
  src/TbMpegSynth.cpp  PolySynth 0x10006c38, dct64 0x10005adc, WRITE_SAMPLE 0x10006f88, Decode_Layer2 0x10005698,
                       II_step_two 0x1000483c, II_step_twoMMX 0x10004ee0, and the tables they read: decwin (0x1003b488, 544
                       floats), the dct64 cosines (0x1003318c .. 0x10033204), alloc_0 .. alloc_4 (0x1003a2c8 ...), the group
                       tables (0x100394c0, 0x10039520, 0x100396a0), the translate table (0x1003b21c) and the sblimits (0x1003b27c)

There are no fakes: part 1 (src/TbMpegBase.cpp, unit w71a: GetBits, the header code, Decode, Read) is linked into the unit
library and the PowerPC side runs the original code of both parts, so a stream can be decoded end to end through Read and the
PCM shorts compared. A case is a "Mac image" of the object (tests/test_MpegBase.py's: the 0x24f0 bytes in the Mac's byte order,
the synthesis buffers as big-endian floats), a stream, the test's own areas (a fraction area of 0x180 floats, an integer
multiplier table, allocation and scale factor arrays, a table of the test's own, dct64's in and out areas), the scratch that
Decode_Layer2 and dct64 keep at 0x1003e688 .. 0x1003ebc8 (filled with one byte), and a list of operations. After EVERY
operation the object image, the output buffer, the areas and the scratch are compared (floats as raw words, every NaN as one
value: the PowerPC leaves a NaN's sign and payload to its operand order).
"""
import ctypes as C
import pathlib
import random
import struct

import tpwtest as T
from tpwtest import FnTest

import test_MpegBase as MB

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_MpegSynth"
MASK = 0xFFFFFFFF
IMG = MB.IMG
OUT_SIZE = MB.OUT_SIZE

A_DCT64, A_POLY, A_WRITE, A_LAYER2, A_STEP2, A_STEP2MMX = 0x10005ADC, 0x10006C38, 0x10006F88, 0x10005698, 0x1000483C, 0x10004EE0
A_OPEN_PTR, A_READ, A_DECODE = MB.A_OPEN_PTR, MB.A_READ, MB.A_DECODE
SCRATCH = 0x1003E688            # fraction 0x300, scale 0xc0, alloc 0x40, select 0x40, dct buffer 0x100
SCRATCH_SIZE = 0x540
USING_MMX = MB.USING_MMX
ALLOC_ADDRESS = [0x1003A2C8, 0x1003A748, 0x1003ABF8, 0x1003AD38, 0x1003AEF8]
ALLOC_END = [0x1003A748, 0x1003ABF8, 0x1003AD38, 0x1003AEF8, 0x1003B208]
# the bits of the allocation code of each subband of the five tables (27, 30, 8, 12 and 30 subbands)
BANDS = [[4] * 11 + [3] * 12 + [2] * 4, [4] * 11 + [3] * 12 + [2] * 7, [4, 4] + [3] * 6, [4, 4] + [3] * 10, [4] * 4 + [3] * 7 + [2] * 19]
FRAC_WORDS = 0x180
DCT_WORDS = 0x260

OP_POLY, OP_LAYER2, OP_STEP2, OP_STEP2MMX, OP_OPEN_PTR, OP_READ, OP_DECODE, OP_DCT64, OP_WRITE, OP_OPEN_SYNTH = range(10)


def s32(v):
    return T.s32(v)


def nz(word):
    """A float word with every NaN as one value."""
    word &= MASK
    return 0x7FC00000 if (word & 0x7F800000) == 0x7F800000 and (word & 0x7FFFFF) else word


def fw(x):
    """The float word of a Python number (rounded to single; overflow gives the infinity)."""
    try:
        return struct.unpack(">I", struct.pack(">f", x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


# ---------------------------------------------------------------- the images and snapshots

def synth_bytes(words):
    return struct.pack(">%dI" % len(words), *words)


def norm_obj(d):
    """The parsed image with the synthesis buffers as normalised float words."""
    d = dict(d)
    d["synth"] = [nz(w) for w in struct.unpack(">%dI" % (len(d["synth"]) // 4), d["synth"])]
    return d


def parse_scratch(raw, big):
    fmt = ">" if big else "<"
    return dict(frac=[nz(w) for w in struct.unpack(fmt + "192I", raw[0:0x300])], scale=bytes(raw[0x300:0x3C0]),
                alloc=bytes(raw[0x3C0:0x400]), select=bytes(raw[0x400:0x440]),
                dct=[nz(w) for w in struct.unpack(fmt + "64I", raw[0x440:0x540])])


def case_words(case, key, count):
    v = case.get(key)
    return list(v) if v is not None else [0] * count


def alloc_bytes(values, count):
    v = list(values) if values is not None else []
    return struct.pack("%db" % count, *(T.s8(x) for x in (v + [0] * count)[:count]))


# ---------------------------------------------------------------- the PowerPC side

def table_address(ctx, which):
    return ctx["custom"] if which == 5 else ALLOC_ADDRESS[which]


def ppc_op(e, ctx, op):
    kind, a, b, c, d = (tuple(op) + (0, 0, 0, 0))[:5]
    obj = ctx["obj"]
    call = lambda addr, args: T.call_ppc(e, addr, args)[0]
    if kind == OP_POLY:
        call(A_POLY, [obj, 0 if a < 0 else ctx["out"] + a, ctx["frac"] + 256 * b])
        return 0
    if kind == OP_LAYER2:
        return s32(call(A_LAYER2, [obj, 0 if a < 0 else ctx["out"] + a]))
    if kind == OP_STEP2:
        call(A_STEP2, [obj, ctx["balloc"], ctx["frac"], ctx["scale"], b & MASK, table_address(ctx, a) + 4 * c])
        return 0
    if kind == OP_STEP2MMX:
        call(A_STEP2MMX, [obj, ctx["mul"], ctx["balloc"], ctx["shortfrac"], ctx["scale"], table_address(ctx, a) + 4 * c])
        return 0
    if kind == OP_OPEN_PTR:
        return s32(call(A_OPEN_PTR, [obj, 0 if a < 0 else ctx["stream"] + a, b & MASK]))
    if kind == OP_READ:
        return s32(call(A_READ, [obj, 0 if a < 0 else ctx["out"] + a, b & MASK]))
    if kind == OP_DECODE:
        return s32(call(A_DECODE, [obj, 0 if a < 0 else ctx["out"] + a]))
    if kind == OP_DCT64:
        call(A_DCT64, [ctx["dct"], ctx["dct"] + 4 * 0x120, ctx["dct"] + 4 * 0x220])
        return 0
    if kind == OP_WRITE:
        T.call_ppc(e, A_WRITE, [ctx["out"] + b, T.F(T.bits_f32(a))])
        return 0
    raise ValueError(kind)


def ppc_snapshot(e, ctx):
    raw = bytearray(e.read(ctx["obj"], IMG))
    vptr = struct.unpack_from(">I", raw, 0x2480)[0]
    raw[0x2480:0x2484] = struct.pack(">I", 1 if vptr == MB.REAL_VTABLE else 3)
    raw[0x2484:0x2488] = b"\0\0\0\0"
    data = struct.unpack_from(">I", raw, 0x24A8)[0]
    raw[0x24A8:0x24AC] = struct.pack(">I", MASK if data == 0 else (data - ctx["stream"]) & MASK)
    return norm_obj(MB.parse(raw))


def ppc_case(e, case):
    obj = e.alloc(IMG + 16)
    stream = e.alloc(0x10040)
    out = e.alloc(OUT_SIZE, bytes([case["outfill"]]) * OUT_SIZE)
    e.write(stream, case["stream"])
    frac = e.alloc(FRAC_WORDS * 4, struct.pack(">%dI" % FRAC_WORDS, *case_words(case, "frac", FRAC_WORDS)))
    shortfrac = e.alloc(FRAC_WORDS * 2, struct.pack(">%dh" % FRAC_WORDS, *case_words(case, "shortfrac", FRAC_WORDS)))
    mul = e.alloc(2048 * 4, struct.pack(">2048i", *case_words(case, "mul", 2048)))
    balloc = e.alloc(0x200, alloc_bytes(case.get("balloc"), 0x200))
    scale = e.alloc(0x300, alloc_bytes(case.get("scale"), 0x300))
    custom = e.alloc(2048)
    pairs = case.get("custom") or []
    e.write(custom, b"".join(struct.pack(">hh", bits, d) for bits, d in pairs))
    dct = e.alloc(DCT_WORDS * 4, struct.pack(">%dI" % DCT_WORDS, *case_words(case, "dct", DCT_WORDS)))
    ctx = dict(obj=obj, stream=stream, out=out, frac=frac, shortfrac=shortfrac, mul=mul, balloc=balloc, scale=scale, custom=custom,
               dct=dct)
    e.write(obj, bytes(MB.image(case, stream)))
    e.write(SCRATCH, bytes([case["sfill"]]) * SCRATCH_SIZE)
    e.write(USING_MMX, struct.pack(">I", 0))
    steps = []
    for op in case["ops"]:
        result = ppc_op(e, ctx, op)
        steps.append(dict(result=result, obj=ppc_snapshot(e, ctx), out=e.read(out, OUT_SIZE),
                          frac=[nz(w) for w in struct.unpack(">%dI" % FRAC_WORDS, e.read(frac, FRAC_WORDS * 4))],
                          shortfrac=list(struct.unpack(">%dh" % FRAC_WORDS, e.read(shortfrac, FRAC_WORDS * 2))),
                          dct=[nz(w) for w in struct.unpack(">%dI" % DCT_WORDS, e.read(dct, DCT_WORDS * 4))],
                          scratch=parse_scratch(e.read(SCRATCH, SCRATCH_SIZE), True)))
    return dict(steps=steps)


def run_ppc(emu, case):
    return ppc_case(emu, case)


def setup(runner):
    # Open calls OpenSynth, which calls memset (the library's import); the standard stubs answer it
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------- the host side

def bind(lib):
    lib.tpw_ms_begin.argtypes = [C.c_int, C.c_int, C.c_int]
    lib.tpw_ms_set_stream.argtypes = [C.c_char_p, C.c_int]
    lib.tpw_ms_load.argtypes = [C.c_char_p]
    lib.tpw_ms_snapshot.argtypes = [C.c_void_p, C.c_int]
    lib.tpw_ms_get_out.argtypes = [C.c_void_p]
    lib.tpw_ms_set_frac.argtypes = [C.c_void_p]
    lib.tpw_ms_get_frac.argtypes = [C.c_void_p]
    lib.tpw_ms_set_short_frac.argtypes = [C.c_void_p]
    lib.tpw_ms_get_short_frac.argtypes = [C.c_void_p]
    lib.tpw_ms_set_mul.argtypes = [C.c_void_p]
    lib.tpw_ms_set_balloc.argtypes = [C.c_void_p]
    lib.tpw_ms_set_scale.argtypes = [C.c_void_p]
    lib.tpw_ms_set_custom.argtypes = [C.c_void_p, C.c_int]
    lib.tpw_ms_set_dct.argtypes = [C.c_void_p]
    lib.tpw_ms_get_dct.argtypes = [C.c_void_p]
    lib.tpw_ms_scratch.argtypes = [C.c_void_p]
    lib.tpw_ms_op.restype = C.c_int64
    lib.tpw_ms_op.argtypes = [C.c_int, C.c_int64, C.c_int64, C.c_int64, C.c_int64]
    lib.tpw_ms_decwin_bits.restype = C.c_uint32
    lib.tpw_ms_decwin_bits.argtypes = [C.c_int]
    lib.tpw_ms_cosine_bits.restype = C.c_uint32
    lib.tpw_ms_cosine_bits.argtypes = [C.c_int]
    lib.tpw_ms_alloc_entries.restype = C.c_int32
    lib.tpw_ms_alloc_entries.argtypes = [C.c_int]
    lib.tpw_ms_alloc_word.restype = C.c_int32
    lib.tpw_ms_alloc_word.argtypes = [C.c_int, C.c_int]
    lib.tpw_ms_group_byte.restype = C.c_int32
    lib.tpw_ms_group_byte.argtypes = [C.c_int, C.c_int]
    lib.tpw_ms_translate.restype = C.c_int32
    lib.tpw_ms_translate.argtypes = [C.c_int]
    lib.tpw_ms_sblimit.restype = C.c_int32
    lib.tpw_ms_sblimit.argtypes = [C.c_int]


def host_result(kind, value):
    if kind in (OP_LAYER2, OP_OPEN_PTR, OP_READ, OP_DECODE):
        return s32(value & MASK)
    return 0


def host_case(lib, case):
    bind(lib)
    lib.tpw_ms_begin(case["fill"], case["outfill"], case["sfill"])
    stream = case["stream"]
    lib.tpw_ms_set_stream(stream, len(stream))
    lib.tpw_ms_load(bytes(MB.image(case)))
    lib.tpw_ms_set_frac(struct.pack("<%dI" % FRAC_WORDS, *case_words(case, "frac", FRAC_WORDS)))
    lib.tpw_ms_set_short_frac(struct.pack("<%dh" % FRAC_WORDS, *case_words(case, "shortfrac", FRAC_WORDS)))
    lib.tpw_ms_set_mul(struct.pack("<2048i", *case_words(case, "mul", 2048)))
    lib.tpw_ms_set_balloc(alloc_bytes(case.get("balloc"), 0x200))
    lib.tpw_ms_set_scale(alloc_bytes(case.get("scale"), 0x300))
    pairs = case.get("custom") or []
    flat = [v for pair in pairs for v in pair]
    lib.tpw_ms_set_custom(struct.pack("<%dh" % len(flat), *flat), len(pairs))
    lib.tpw_ms_set_dct(struct.pack("<%dI" % DCT_WORDS, *case_words(case, "dct", DCT_WORDS)))
    steps = []
    image_buffer = (C.c_uint8 * IMG)()
    out_buffer = (C.c_uint8 * OUT_SIZE)()
    frac_buffer = (C.c_uint32 * FRAC_WORDS)()
    short_buffer = (C.c_int16 * FRAC_WORDS)()
    dct_buffer = (C.c_uint32 * DCT_WORDS)()
    scratch = (C.c_uint8 * SCRATCH_SIZE)()
    try:
        for op in case["ops"]:
            kind, a, b, c, d = (tuple(op) + (0, 0, 0, 0))[:5]
            value = lib.tpw_ms_op(kind, a, b, c, d)
            lib.tpw_ms_snapshot(image_buffer, case["fill"])
            lib.tpw_ms_get_out(out_buffer)
            lib.tpw_ms_get_frac(frac_buffer)
            lib.tpw_ms_get_short_frac(short_buffer)
            lib.tpw_ms_get_dct(dct_buffer)
            lib.tpw_ms_scratch(scratch)
            steps.append(dict(result=host_result(kind, value), obj=norm_obj(MB.parse(bytes(image_buffer))), out=bytes(out_buffer),
                              frac=[nz(w) for w in frac_buffer], shortfrac=list(short_buffer), dct=[nz(w) for w in dct_buffer],
                              scratch=parse_scratch(bytes(scratch), False)))
    finally:
        lib.tpw_ms_end()
    return dict(steps=steps)


# ---------------------------------------------------------------- case building

FILLS = [0x00, 0xFF, 0xA5, 0x5A]


def sane(layer=2, channels=2, **kw):
    return MB.sane(layer=layer, channels=channels, **kw)


def mk(ops, state=None, **kw):
    c = dict(mode=0, fill=0xA5, outfill=0x3C, sfill=0x00, state=sane() if state is None else state, ops=list(ops), stream=b"",
             synth=None, frame=None, frac=None, shortfrac=None, mul=None, balloc=None, scale=None, custom=None, dct=None)
    c.update(kw)
    return c


def rfill(r):
    return r.choice(FILLS)


def special_word(r):
    return r.choice([0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF, 0xFF7FFFFF, 0x00000001,
                     0x007FFFFF, 0x80000001, 0x7FA00000, 0xFFC00001])


def audio_word(r, amplitude=32768.0):
    return fw(r.uniform(-amplitude, amplitude))


def wild_word(r):
    k = r.random()
    if k < 0.35:
        return audio_word(r)
    if k < 0.5:
        return fw(r.uniform(-1.0, 1.0))
    if k < 0.6:
        return fw(r.uniform(-4e5, 4e5))
    if k < 0.68:
        return fw(r.uniform(-1.0, 1.0) * 10.0 ** r.randrange(-40, 38))
    if k < 0.76:
        return special_word(r)
    if k < 0.84:
        return fw(float(r.randrange(-40000, 40000)))
    return r.getrandbits(32)


def words(r, count, kind):
    """Float words: 'small' (+-1, the size of layer II fractions), 'audio' (+-4), 'loud' (+-1000: most samples clip, but the sums
    stay below 2^31: the emulator's fctiwz answers INT_MIN above that where the processor saturates, see WRITE_SAMPLE's tests),
    'big' (+-20000, dct64 only) and 'wild' (anything, NaNs and infinities too; only where nothing is converted)."""
    if kind == "small":
        return [fw(r.uniform(-1.0, 1.0)) for _ in range(count)]
    if kind == "audio":
        return [audio_word(r, 4.0) for _ in range(count)]
    if kind == "loud":
        return [audio_word(r, 1000.0) for _ in range(count)]
    if kind == "big":
        return [audio_word(r, 20000.0) for _ in range(count)]
    return [wild_word(r) for _ in range(count)]


def rbytes(r, n):
    return MB.rbytes(r, n)


def sparse_bytes(r, n):
    """Bytes with many zeros, so that allocations come out mixed (some bands unallocated)."""
    return bytes(r.choice([0, 0, 0, r.randrange(256), r.randrange(256), 0xFF, 0x80, 0x01]) if r.random() < 0.7 else 0 for _ in range(n))


# ---- dct64

def dct_case(samples, r, fills=None):
    area = [r.getrandbits(32) for _ in range(DCT_WORDS)] if fills == "random" else [0] * DCT_WORDS
    area[0x220:0x240] = samples
    return mk([(OP_DCT64,)], dct=area, sfill=rfill(r) if fills != "zero" else 0)


def dct_edges():
    r = random.Random(101)
    one, zero, negzero = 0x3F800000, 0, 0x80000000
    yield dct_case([zero] * 32, r, "zero")
    yield dct_case([zero] * 32, r, "random")
    yield dct_case([negzero] * 32, r, "random")
    yield dct_case([one] * 32, r, "random")
    yield dct_case([fw(1000.0 * (1 if i % 2 else -1)) for i in range(32)], r, "random")
    yield dct_case([fw(float(i) * 100.0) for i in range(32)], r, "random")
    for i in range(32):
        s = [zero] * 32
        s[i] = fw(1000.0)
        yield dct_case(s, r, "random")
    for i in (0, 7, 15, 16, 31):
        s = [zero] * 32
        s[i] = fw(-3.25)
        yield dct_case(s, r, "zero")
    yield dct_case([0x7F7FFFFF] * 32, r, "random")                      # overflow to infinity
    yield dct_case([0x00000001] * 32, r, "random")                      # denormals
    yield dct_case([0x007FFFFF if i % 2 else 0x00800000 for i in range(32)], r, "random")
    for position in (0, 5, 16, 31):
        s = [fw(100.0)] * 32
        s[position] = 0x7FC00000
        yield dct_case(s, r, "random")
        s[position] = 0x7F800000
        yield dct_case(s, r, "random")
        s[position] = 0xFF800000
        yield dct_case(s, r, "random")


def dct_random(r):
    kind = r.choice(["audio", "big", "loud", "small", "wild"])
    return dct_case(words(r, 32, kind), r, "random")


# ---- WRITE_SAMPLE

def write_case(bits, r, offset=0x10):
    return mk([(OP_WRITE, bits, offset)], outfill=r.choice([0x00, 0xFF, 0x5A]))


def write_edges():
    r = random.Random(103)
    values = [0.0, -0.0, 0.25, -0.25, 0.5, -0.5, 0.999, -0.999, 1.0, -1.0, 1.5, -1.5, 2.5, -2.5, 100.7, -100.7, 32766.0, 32766.9,
              32767.0, 32767.5, 32767.999, 32768.0, 32768.5, 32769.0, -32767.0, -32767.5, -32768.0, -32768.5, -32768.999, -32769.0,
              65535.0, 65536.0, -65536.0, 1e9, -1e9, 2147483520.0, -2147483648.0, -2147483904.0, -3e9, -1e30, 8388608.0,
              16777216.0, -8388609.0]
    # (the values from 2^31 up are not run: the emulator's fctiwz answers INT_MIN for them where the processor saturates at
    # INT_MAX, and the host follows the processor; the NaNs and the negative infinity answer INT_MIN on both)
    for v in values:
        yield write_case(fw(v), r)
    for bits in (0xFF800000, 0x7FC00000, 0xFFC00000, 0x7F800001, 0x00000001, 0x80000001, 0x007FFFFF):
        yield write_case(bits, r)
    yield write_case(fw(5.0), r, offset=0)
    yield write_case(fw(-5.0), r, offset=OUT_SIZE - 2)


def write_random(r):
    if r.random() < 0.4:
        word = wild_word(r)
        value = struct.unpack(">f", struct.pack(">I", word))[0]
        if value >= 2147483648.0 and value == value:
            word = fw(value % 2000000000.0)
    else:
        word = fw(r.uniform(-40000.0, 40000.0))
    return write_case(word, r, offset=2 * r.randrange(0, 100))


# ---- PolySynth

def synth_state(r, kind="small"):
    return synth_bytes(words(r, 0x480, kind))


def poly_case(r, channels, bo, outs, blocks, kind="small", fkind="small", synth=None):
    ops = [(OP_POLY, out, blocks[i % len(blocks)]) for i, out in enumerate(outs)]
    return mk(ops, state=sane(layer=1, channels=channels, bo=bo, channels32=channels << 5), synth=synth_state(r, kind) if synth is None else synth,
              frac=words(r, FRAC_WORDS, fkind), fill=rfill(r), outfill=r.choice([0x00, 0xFF, 0x3C]), sfill=rfill(r))


def poly_edges():
    r = random.Random(107)
    for channels in (1, 2):
        for bo in list(range(16)) + [16, 17, 31, 255]:
            yield poly_case(r, channels, bo, [0], [0])
    for channels in (1, 2):
        for bo in (0, 1, 2, 15):
            yield poly_case(r, channels, bo, [-1], [0])
            yield poly_case(r, channels, bo, [0x100, 0x100, 0x100], [2, 1, 0])
            yield poly_case(r, channels, bo, [0] * 17, [0, 1, 2], "small", "loud")
    for channels in (0, -1):
        yield poly_case(r, channels, 5, [0], [0])
    for fkind in ("loud", "small", "audio"):
        for channels in (1, 2):
            yield poly_case(r, channels, 7, [0, 0x80], [0, 1], "small", fkind)
    for channels in (1, 2):          # nothing is converted (no output): wild values, NaNs and infinities in the buffers
        yield poly_case(r, channels, 7, [-1, -1], [0, 1], "wild", "wild")
        yield poly_case(r, channels, 2, [-1], [2], "small", "wild")
        yield poly_case(r, channels, 2, [-1], [2], "wild", "small")
    # all zero, all ones, constant
    for channels in (1, 2):
        yield poly_case(r, channels, 3, [0, 0], [0], synth=bytes(0x1200))
        yield poly_case(r, channels, 3, [0], [0], synth=synth_bytes([0x3F800000] * 0x480))
        yield poly_case(r, channels, 4, [0], [0], synth=synth_bytes([0xBF800000] * 0x480))
        yield poly_case(r, channels, 4, [0, 0], [0], synth=synth_bytes([0x80000000] * 0x480))
    # the window sums in range: small synthesis words so that every sample is a small number
    for channels in (1, 2):
        for bo in (0, 1, 6, 9):
            yield poly_case(r, channels, bo, [0, 0, 0, 0], [0, 1, 2], "small", "small")


def poly_random(r):
    channels = r.choice([1, 2, 2, 1, 2])
    bo = r.randrange(16) if r.random() < 0.9 else r.getrandbits(8)
    outs = [r.choice([0, 0x40, 0x200, -1]) for _ in range(r.randrange(1, 6))]
    if r.random() < 0.8:
        outs = [outs[0] if outs[0] >= 0 else 0] * len(outs) if r.random() < 0.5 else outs
    kind = r.choice(["small", "audio", "audio", "loud", "small"])
    fkind = r.choice(["small", "audio", "audio", "loud", "small"])
    if r.random() < 0.1:
        outs, kind, fkind = [-1] * len(outs), r.choice(["wild", kind]), r.choice(["wild", fkind])
    return poly_case(r, channels, bo, outs, [r.randrange(3) for _ in range(3)], kind, fkind)


# ---- II_step_two and II_step_twoMMX

def rand_codes(r, table, sblimit, channels, zero=0.3):
    """Valid allocation codes (below 1 << the subband's bits), the channels of a subband one after the other (at least one:
    the integer form reads the first channel's code whatever the channel count)."""
    codes = []
    for sb in range(sblimit):
        n = 1 << BANDS[table][sb]
        for _ in range(max(channels, 1)):
            codes.append(0 if r.random() < zero else r.randrange(n))
    return codes


def custom_table(r, bands):
    """A table of the test's own: every band a run of 1 << nbal entries, the first {nbal, 0}; the others random {bits, d}."""
    pairs, nbals = [], []
    for _ in range(bands):
        nbal = r.randrange(1, 4)
        nbals.append(nbal)
        pairs.append((nbal, 0))
        for _ in range((1 << nbal) - 1):
            k = r.random()
            if k < 0.45:
                bits = r.randrange(1, 17)
                pairs.append((bits, -((1 << (bits - 1)) - 1) if bits > 1 and r.random() < 0.7 else -r.randrange(1, 32768)))
            elif k < 0.6:
                pairs.append((r.randrange(1, 6), 3))
            elif k < 0.75:
                pairs.append((r.randrange(1, 8), 5))
            elif k < 0.9:
                pairs.append((r.randrange(1, 11), 9))
            else:
                pairs.append((r.randrange(1, 17), -r.randrange(1, 32768)))
    return pairs, nbals


def scale_bytes(r, mode="random"):
    if mode == "zero":
        return [0] * 0x300
    if mode == "max":
        return [63] * 0x300
    return [r.randrange(64) for _ in range(0x300)]


def mul_words(r, kind="random"):
    if kind == "small":
        return [r.randrange(-3000, 3000) for _ in range(2048)]
    if kind == "unit":
        return [4096 * (1 if i % 3 else -1) for i in range(2048)]
    return [s32(r.choice([r.getrandbits(32), r.randrange(-100000, 100000), 0, 4096, -4096, 0x7FFFFFFF, -0x80000000]) & MASK) for _ in range(2048)]


def step_case(r, table, sblimit, channels, ops, codes=None, custom=None, stream=None, mmx=False, scale="random", zero=0.3,
              mulkind="random", frac_fill="random"):
    if codes is None:
        codes = rand_codes(r, table, sblimit, 1 if mmx and channels != 2 else channels, zero)
    return mk(ops, state=sane(layer=2, channels=channels, sblimit=sblimit), balloc=codes, scale=scale_bytes(r, scale), custom=custom,
              stream=rbytes(r, 3000) if stream is None else stream,
              frac=[r.getrandbits(32) for _ in range(FRAC_WORDS)] if frac_fill == "random" else [0] * FRAC_WORDS,
              shortfrac=[r.randrange(-32768, 32768) for _ in range(FRAC_WORDS)], mul=mul_words(r, mulkind), fill=rfill(r),
              sfill=rfill(r))


def step_edges(mmx):
    op = OP_STEP2MMX if mmx else OP_STEP2
    r = random.Random(113 + mmx)
    for table in range(5):
        bands = len(BANDS[table])
        for channels in (1, 2):
            for sblimit in range(0, bands + 1):
                yield step_case(r, table, sblimit, channels, [(op, table, 0)], mmx=mmx)
        # every code of every band of one table: stereo, the first band, all codes
    for channels in (1, 2):
        for x1 in (0, 1, 2):
            yield step_case(r, 0, 27, channels, [(op, 0, x1), (op, 0, x1)], mmx=mmx)
    # channel counts other than 1 and 2: the float form loops over mChannels (3 channels, none, a negative count), the integer
    # form does the second channel only for exactly 2
    for channels in (0, 3, -1, 4):
        for table in (0, 2):
            yield step_case(r, table, min(5, len(BANDS[table])), channels, [(op, table, 0)], mmx=mmx)
            yield step_case(r, table, len(BANDS[table]), channels, [(op, table, 1)], mmx=mmx, zero=0.0)
    # the scale factor offset x1: three per coded band, the offset picks one of them
    for channels in (1, 2):
        yield step_case(r, 1, 30, channels, [(op, 1, 0), (op, 1, 1), (op, 1, 2), (op, 1, 3), (op, 1, 7)], mmx=mmx, zero=0.0)
    # every code of the first bands, stream bytes of ones and zeros
    for table in range(5):
        for code in range(16):
            for channels in (1, 2):
                n = 1 << BANDS[table][0]
                if code >= n:
                    continue
                codes = [code, code] if channels == 2 else [code]
                for data in (bytes(100), bytes([0xFF]) * 100, rbytes(r, 100)):
                    yield step_case(r, table, 1, channels, [(op, table, 0)], codes=codes, stream=data, mmx=mmx, scale="max" if code % 2 else "zero")
    # past the table's subbands: a table of the test's own with one bit-less entry per band and no allocation
    pairs = [(0, 0)] * 80
    for sblimit in (31, 32, 33, 40, 64, 79, -1, -100, -0x80000000):
        for channels in (1, 2):
            yield step_case(r, 0, sblimit, channels, [(op, 5, 0)], codes=[0] * 80, custom=pairs, mmx=mmx)
    # random tables of the test's own
    for _ in range(12):
        bands = r.randrange(1, 12)
        pairs, nbals = custom_table(r, bands)
        channels = r.choice([1, 2])
        codes = []
        for nbal in nbals:
            for _ in range(channels):
                codes.append(0 if r.random() < 0.25 else r.randrange(1 << nbal))
        yield step_case(r, 0, bands, channels, [(op, 5, 0)], codes=codes, custom=pairs, mmx=mmx)
    # the table pointer starts inside the table: the entries before it are not read
    yield step_case(r, 2, 4, 2, [(op, 2, 0)], mmx=mmx, stream=bytes([0xFF]) * 600)
    # multiplier extremes for the MMX form
    if mmx:
        for kind in ("small", "unit"):
            yield step_case(r, 0, 27, 2, [(op, 0, 0)], mmx=mmx, mulkind=kind, zero=0.0)
            yield step_case(r, 4, 30, 1, [(op, 4, 0)], mmx=mmx, mulkind=kind, zero=0.0)
    # a mono object with the second channel's slots preset (they must stay), and a stream of ones
    yield step_case(r, 0, 27, 1, [(op, 0, 0)], mmx=mmx, stream=bytes([0xFF]) * 3000, zero=0.0)
    yield step_case(r, 0, 27, 2, [(op, 0, 0)], mmx=mmx, stream=bytes([0xFF]) * 3000, zero=0.0)
    yield step_case(r, 0, 27, 2, [(op, 0, 0)], mmx=mmx, stream=bytes(3000), zero=0.0)


def step_random(mmx):
    op = OP_STEP2MMX if mmx else OP_STEP2

    def gen(r):
        if r.random() < 0.2:
            bands = r.randrange(1, 12)
            pairs, nbals = custom_table(r, bands)
            channels = r.choice([1, 2])
            codes = [0 if r.random() < 0.25 else r.randrange(1 << nbal) for nbal in nbals for _ in range(channels)]
            return step_case(r, 0, bands, channels, [(op, 5, 0)], codes=codes, custom=pairs, mmx=mmx, scale=r.choice(["random", "max"]))
        table = r.randrange(5)
        sblimit = r.randrange(0, len(BANDS[table]) + 1)
        channels = r.choice([1, 2])
        ops = [(op, table, r.randrange(3))] * r.randrange(1, 4)
        return step_case(r, table, sblimit, channels, ops, mmx=mmx, zero=r.choice([0.0, 0.3, 0.7]), scale=r.choice(["random", "random", "max", "zero"]),
                         stream=rbytes(r, 3000) if r.random() < 0.7 else sparse_bytes(r, 3000), mulkind=r.choice(["random", "small", "unit"]))
    return gen


# ---- Decode_Layer2

def l2_state(lsf, sri, bri, channels, bo, **kw):
    return sane(layer=2, channels=channels, lsf=lsf, sri=sri, bri=bri, bo=bo, id=0 if lsf else 1, channels32=channels << 5, **kw)


def l2_case(r, lsf, sri, bri, channels, stream, ops=None, bo=None, synth="small", out=0, bitpos=0):
    ops = ops or [(OP_LAYER2, out)]
    return mk(ops, state=l2_state(lsf, sri, bri, channels, r.randrange(16) if bo is None else bo, bitpos=bitpos), stream=stream,
              synth=synth_state(r, synth), fill=rfill(r), sfill=rfill(r), outfill=r.choice([0x00, 0x3C]))


def layer2_edges():
    r = random.Random(127)
    for lsf, sris in ((1, [3]), (1, [4]), (0, [0, 1, 2])):
        for sri in sris:
            for channels in (1, 2):
                for bri in range(16):
                    stream = rbytes(r, 4000) if bri % 3 else sparse_bytes(r, 4000)
                    yield l2_case(r, lsf, sri, bri, channels, stream)
    for lsf, sri in ((1, 3), (0, 0)):          # no channels: the mono path with no scale factors, nothing synthesised
        for bri in (5, 11):
            yield l2_case(r, lsf, sri, bri, 0, rbytes(r, 4000))
    for channels in (1, 2):
        for lsf, sri in ((1, 3), (0, 0)):
            for data in (bytes(4000), bytes([0xFF]) * 4000, bytes([0x55]) * 4000, bytes([0xAA]) * 4000, bytes([0x10, 0x01]) * 2000,
                         bytes([0xF0, 0x0F]) * 2000, sparse_bytes(r, 4000)):
                yield l2_case(r, lsf, sri, 5, channels, data)
                yield l2_case(r, lsf, sri, 11, channels, data, ops=[(OP_LAYER2, -1)])
    # bit positions that are not on a byte, several frames in a row, the output past the first bytes
    for bitpos in (0, 1, 7, 8, 13, 32, 100):
        yield l2_case(r, 1, 3, 5, 2, rbytes(r, 4000), bitpos=bitpos)
    yield l2_case(r, 1, 3, 5, 2, rbytes(r, 6000), ops=[(OP_LAYER2, 0), (OP_LAYER2, 0x1200)])
    yield l2_case(r, 1, 3, 5, 1, rbytes(r, 6000), ops=[(OP_LAYER2, 0), (OP_LAYER2, 0x1200), (OP_LAYER2, 0x2400)])
    # the synthesis buffer states: zeros, constant, and every bo
    for bo in range(16):
        yield l2_case(r, 1, 3, 5, 2, rbytes(r, 4000), bo=bo)
    # a non-lsf stream with the reserved sample rate index 3 (stereo): the table index is the byte at 0x1003b21c + 0x60 + bri, past the
    # translate table, where the sblimit halfwords (00 1b 00 1e 00 08 00 0c 00 1e) follow; for the bit rate indexes 0, 2, 4, 6 and 8 that
    # byte is 0, the same table 0 the host takes (index 0 is at == 0x60 exactly: the `at < 0x60` bound)
    rr = random.Random(131)
    for bri in (0, 2, 4, 6, 8):
        yield l2_case(rr, 0, 3, bri, 2, rbytes(rr, 4000))
        yield l2_case(rr, 0, 3, bri, 2, sparse_bytes(rr, 4000), ops=[(OP_LAYER2, 0), (OP_LAYER2, 0x1200)])
    # real frames: layer II stereo in the game's banks (decoded at the first bit after the header)
    for tag, streams in MB.sdt().items():
        if tag != "l2":
            continue
        for data in streams[:3]:
            h = struct.unpack(">I", data[:4])[0]
            channels = 1 if ((h >> 6) & 3) == 3 else 2
            lsf = 1 if ((h >> 19) & 1) == 0 else 0
            sri = ((h >> 10) & 3) + 3 * lsf
            bri = (h >> 12) & 15
            yield l2_case(r, lsf, sri, bri, channels, data, bitpos=32, ops=[(OP_LAYER2, 0)])


def layer2_random(r):
    lsf = r.getrandbits(1)
    sri = r.choice([3, 4, 5, 3]) if lsf else r.randrange(3)
    channels = r.choice([1, 2, 2])
    data = rbytes(r, 4000) if r.random() < 0.6 else sparse_bytes(r, 4000)
    ops = [(OP_LAYER2, r.choice([0, 0, -1, 0x200])) for _ in range(r.randrange(1, 3))]
    return l2_case(r, lsf, sri, r.randrange(16), channels, data, ops=ops, bitpos=r.choice([0, 0, 32, r.randrange(0, 200)]),
                   synth=r.choice(["small", "audio", "audio", "loud"]))


# ---- end to end: Open and Read over layer II streams (Decode, Decode_Layer2, PolySynth, II_step_two together)

def read_ops(total, reads, out=0):
    return [(OP_OPEN_PTR, 0, total)] + [(OP_READ, out, n) for n in reads]


def read_edges():
    r = random.Random(131)
    for kw in (dict(layer=2, id_=0, bri=11, mode=0), dict(layer=2, id_=0, bri=5, mode=3), dict(layer=2, id_=1, bri=9, mode=3),
               dict(layer=2, id_=1, bri=9, mode=1, sri=0), dict(layer=2, id_=0, bri=12, mode=2, pad=1), dict(layer=2, id_=1, bri=4, sri=1, mode=0),
               dict(layer=2, id_=1, bri=14, sri=2, mode=0)):
        stream = MB.stream_of(r, 5, **kw)
        for reads in ([0x1200] * 3, [0x900] * 3, [0x2400], [0x300, 0x300, 0x600, 0x900], [2] * 4, [0x500, 0x1000, 0x1800]):
            yield mk(read_ops(len(stream), reads), state=sane(open=0), stream=stream)
    for kw in (dict(layer=2, id_=0, bri=11, mode=0), dict(layer=2, id_=0, bri=5, mode=3)):
        stream = MB.stream_of(r, 4, **kw)
        for out in (0, 2, 0x40, 0x102):
            yield mk(read_ops(len(stream), [0x1200, 0x300], out), state=sane(open=0), stream=stream)
    # decode alone, at an output offset or into nothing
    stream = MB.stream_of(r, 4, layer=2, id_=0, bri=11, mode=0)
    for out in (0, 0x40, -1):
        yield mk([(OP_OPEN_PTR, 0, len(stream))] + [(OP_DECODE, out)] * 3, state=sane(open=0), stream=stream)
    # the game's frames (layer II stereo, MPEG-2 22050 Hz), read from the extracted banks when they are there
    for tag, streams in MB.sdt().items():
        if tag != "l2":
            continue
        for data in streams[:3]:
            for reads in ([0x1200] * 6, [0x900] * 8, [0x2400, 0x2400, 0x2400]):
                yield mk(read_ops(len(data), reads), state=sane(open=0), stream=data)


def read_random(r):
    kw = dict(layer=2, id_=r.getrandbits(1), bri=r.randrange(1, 15), sri=r.randrange(3), mode=r.randrange(4), pad=r.getrandbits(1))
    if kw["id_"] == 0:
        kw["sri"] = 0
    stream = MB.stream_of(r, r.randrange(1, 5), **kw) + (bytes(8) if r.random() < 0.3 else b"")
    reads = [r.choice([0x300, 0x600, 0x900, 0x1200, 0x2400, r.randrange(1, 0x1000) & ~1]) for _ in range(r.randrange(1, 5))]
    return mk(read_ops(len(stream), reads, r.choice([0, 0, 2, 0x40])), state=sane(open=0), stream=stream, fill=rfill(r), sfill=rfill(r))


# ---- the tables

def tables_ppc(e, case):
    kind, i = case["table"]
    if kind == "decwin":
        return dict(words=[nz(w) for w in struct.unpack(">544I", e.read(0x1003B488, 544 * 4))])
    if kind == "cosine":
        addrs = [0x10033204 - 4 * k for k in range(16)] + [0x100331C4 - 4 * k for k in range(8)] + [0x100331A4 - 4 * k for k in range(4)] + \
                [0x10033194 - 4 * k for k in range(2)] + [0x1003318C]
        return dict(words=[e.u32(a) for a in addrs])
    if kind == "alloc":
        start = ALLOC_ADDRESS[i]
        n = (ALLOC_END[i] - start) // 4
        values = struct.unpack(">%dh" % (2 * n), e.read(start, 4 * n))
        return dict(words=[s32((values[2 * k] << 16) | (values[2 * k + 1] & 0xFFFF)) for k in range(n)])
    if kind == "group":
        d = i
        start, size = {3: (0x100394C0, 96), 5: (0x10039520, 384), 9: (0x100396A0, 3072)}[d]
        return dict(words=list(e.read(start, size)))
    if kind == "translate":
        return dict(words=list(e.read(0x1003B21C, 0x60)))
    return dict(words=list(struct.unpack(">5h", e.read(0x1003B27C, 10))))


def tables_host(lib, case):
    bind(lib)
    kind, i = case["table"]
    if kind == "decwin":
        return dict(words=[nz(lib.tpw_ms_decwin_bits(k)) for k in range(544)])
    if kind == "cosine":
        return dict(words=[lib.tpw_ms_cosine_bits(k) for k in range(31)])
    if kind == "alloc":
        return dict(words=[lib.tpw_ms_alloc_word(i, k) for k in range(lib.tpw_ms_alloc_entries(i))])
    if kind == "group":
        size = {3: 96, 5: 384, 9: 3072}[i]
        return dict(words=[lib.tpw_ms_group_byte(i, k) for k in range(size)])
    if kind == "translate":
        return dict(words=[lib.tpw_ms_translate(k) for k in range(0x60)])
    return dict(words=[lib.tpw_ms_sblimit(k) for k in range(5)])


def tables_edges():
    yield dict(table=("decwin", 0))
    yield dict(table=("cosine", 0))
    for i in range(5):
        yield dict(table=("alloc", i))
    for d in (3, 5, 9):
        yield dict(table=("group", d))
    yield dict(table=("translate", 0))
    yield dict(table=("sblimit", 0))


def tables_random(r):
    return dict(table=r.choice([("decwin", 0), ("cosine", 0), ("alloc", r.randrange(5)), ("group", r.choice([3, 5, 9])),
                                ("translate", 0), ("sblimit", 0)]))


TESTS = [
    FnTest("dct64(float*, float*, float*) 0x10005adc", run_ppc, host_case, edge=dct_edges, rnd=dct_random),
    FnTest("WRITE_SAMPLE(short*, float) 0x10006f88", run_ppc, host_case, edge=write_edges, rnd=write_random),
    FnTest("CMpegBase::PolySynth(short*, float*) 0x10006c38", run_ppc, host_case, edge=poly_edges, rnd=poly_random),
    FnTest("CMpegBase::II_step_two(char*, float(*)[2][32], char*, int, const al_table*) 0x1000483c", run_ppc, host_case,
           edge=lambda: step_edges(False), rnd=step_random(False)),
    FnTest("CMpegBase::II_step_twoMMX(int*, char*, short(*)[2][32], char*, const al_table*) 0x10004ee0", run_ppc, host_case,
           edge=lambda: step_edges(True), rnd=step_random(True)),
    FnTest("CMpegBase::Decode_Layer2(short*) 0x10005698", run_ppc, host_case, edge=layer2_edges, rnd=layer2_random),
    FnTest("layer II streams end to end: Open and Read over Decode, Decode_Layer2, PolySynth, II_step_two", run_ppc, host_case,
           edge=read_edges, rnd=read_random),
    FnTest("the tables: decwin (0x1003b488), dct64 cosines, alloc_0 .. alloc_4, group tables, translate table, sblimits",
           tables_ppc, tables_host, edge=tables_edges, rnd=tables_random),
]
