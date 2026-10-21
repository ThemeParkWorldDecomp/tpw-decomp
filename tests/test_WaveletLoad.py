"""Unit w86, against libtpw_unit_WaveletLoad.dylib (tests/unit_WaveletLoad.cpp), in the program
"engine symbols shared":
  src/engine/Bitmap.cpp          Bitmap::load_wavelet 0x10047294
  src/engine/EngineUtil.cpp, src/engine/Wavelet.cpp  CWaveletImp::Decompress 0x10039694 (the fast-decoder dispatcher)

load_wavelet runs as the original in the emulator and as the host's translation over a bitmap (a size, a depth and an
old pixel buffer, or none) and a scripted file: three little-endian header words, then the planes' floats and sizes and
payloads. Both sides fake the same callees and record the same events (tests/unit_WaveletLoad.cpp lists the kinds):
new[] and delete[] (with the ordinal of the block and a hash of its bytes at the delete, so the interleaved pixels, the
payload buffers and the planes' contents are all compared), the file reader (the size and count of every read; a
read past the end gives zeros), and the wavelet decompressor's slot 0 (every argument, the floats as bits, which block
the source and destination are, and a hash of the packed bytes it was given). The fake decompressor overwrites its whole
destination block with a scripted pattern, so the depth-4 interleave moves known bytes. The Nth new[] of a case can
answer NULL, and so does any request over 1 MiB (a size that wrapped). The bitmap's three fields, its pixel buffer and
the result are compared after the call.

The dispatcher runs with the two fast decoders faked on both sides (every argument recorded, the floats as bits). What
the original leaves in r3 on return is the callee's (the decompile says void); the translation answers 0, so r3 is not
compared.

Not generated, because the original crashes there too: a depth-4 file with a pixel count above 0 and a NULL answer for
the 3-byte or 1-byte plane (the interleave reads it).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_WaveletLoad"

LOAD_ADDR = 0x10047294
DISPATCH_ADDR = 0x10039694
COLOR_FAST, MONO_FAST = 0x10039894, 0x1003A67C
FILE_READ_SLOT = 0x101735D8
K_ADDR = 0x10050168      # the float the original reads at r2 - 0x7308 (0.0f)
ALLOC_LIMIT = 1 << 20
FILE = T.FAKE_FILE_HANDLE
MASK = 0xFFFFFFFF
OUT_CAP = 1 << 20

# bit patterns of floats: ordinary, zeros, a denormal, the largest, infinities, quiet and signalling NaNs
FLOATS = [0x3F800000, 0x40490FDB, 0x00000000, 0x80000000, 0x00000001, 0x007FFFFF, 0x7F7FFFFF, 0x7F800000, 0xFF800000,
          0x7FC00000, 0xFFC12345, 0x7FA00000, 0xFF800001, 0xC2F6E979, 0x3DCCCCCD, 0x12345678]


# ----------------------------------------------------------------- the file script


def words(*values):
    return struct.pack("<%dI" % len(values), *[v & MASK for v in values])


def payload(r, n):
    """n bytes, a share of them from the extremes."""
    return bytes(r.choice((0, 1, 127, 128, 254, 255)) if r.random() < 0.3 else r.randrange(256) for _ in range(n))


def stream(width, height, depth, floats=(), sizes=(), payloads=(), tail=b""):
    """The file: width, height, depth, then the floats (as bits), the payload sizes and the payloads."""
    return words(width, height, depth, *floats, *sizes) + b"".join(payloads) + tail


def plane_file(r, width, height, depth, nbytes=None, floats=None):
    """A well-formed file of the depth (the floats and payload sizes chosen here)."""
    nfloats = {1: 1, 3: 3, 4: 4}.get(depth, 0)
    if floats is None:
        floats = [r.choice(FLOATS) for _ in range(nfloats)]
    nplanes = {1: 1, 3: 1, 4: 2}.get(depth, 0)
    sizes = [nbytes if nbytes is not None else r.choice((0, 1, 5, 16, 100)) for _ in range(nplanes)]
    return stream(width, height, depth, floats, sizes, [payload(r, n) for n in sizes])


def case(data, init=None, init_data=None, fail=0):
    """init: the bitmap's (width, height, depth) before the call (None: all zero); init_data: its buffer (None: none)."""
    return dict(stream=data, init=init, init_data=init_data, fail=fail)


# ----------------------------------------------------------------- the two sides


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def fbits(e, i):
    """FPR i as the single's bit pattern it came from (a NaN keeps its payload bits)."""
    v = e.uc.reg_read(__import__("unicorn").ppc_const.UC_PPC_REG_FPR0 + i)
    raw = struct.unpack(">Q", struct.pack(">d", v))[0] if isinstance(v, float) else v & 0xFFFFFFFFFFFFFFFF
    exp, mant = (raw >> 52) & 0x7FF, raw & ((1 << 52) - 1)
    if exp == 0x7FF and mant:
        return ((raw >> 63) << 31) | 0x7F800000 | (mant >> 29)
    return struct.unpack(">I", struct.pack(">f", struct.unpack(">d", struct.pack(">Q", raw))[0]))[0]


class PpcWorld:
    """The PowerPC side's state for one case: events, allocation record, the file stream."""

    def __init__(self, stream_bytes, fail_at):
        self.events, self.blocks, self.calls, self.fail_at = [], [], 0, fail_at
        self.stream, self.pos, self.dec_calls, self.obj = stream_bytes, 0, 0, 0

    def event(self, kind, *values):
        self.events.append(tuple([kind, *[int(v) for v in values]] + [0] * (11 - len(values))))

    def ordinal(self, addr):
        if addr == 0:
            return 0
        for i, block in enumerate(self.blocks):
            if block[0] == addr:
                return i + 1
        return 0

    def block(self, addr):
        for block in self.blocks:
            if block[0] == addr:
                return block
        return None


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    assert e.toc - 0x7308 == K_ADDR, "the TOC of the fake TVectors is not the engine's"
    assert e.read(K_ADDR, 4) == b"\0\0\0\0", "the engine float k is no longer 0.0f"
    e.wl = PpcWorld(b"", 0)

    def new_array(em):
        w = em.wl
        size = em.r(3)
        w.calls += 1
        w.event(1, size)
        if w.calls == w.fail_at or size > ALLOC_LIMIT:
            em.set_r(3, 0)
            return
        a = em.alloc(max(size, 4), b"\xc3" * max(size, 4))
        w.blocks.append([a, size])
        em.set_r(3, a)

    def delete_array(em):
        w = em.wl
        p = em.r(3)
        if p == 0:
            return
        for i, block in enumerate(w.blocks):
            if block[0] == p:
                w.event(2, i + 1, fnv(em.read(p, block[1])))
                block[0] = None
                break

    e.stub("__nwa__FUl", new_array)
    e.stub("__dla__FPv", delete_array)

    def file_read(em):
        w = em.wl
        dest, size, count = em.r(3), em.r(4), em.r(5)
        w.event(3, size, count, em.r(6) == FILE)
        n = (size * count) & MASK
        chunk = w.stream[w.pos:w.pos + n] if w.pos < len(w.stream) else b""
        w.pos += n
        if n:
            em.write(dest, chunk + b"\0" * (n - len(chunk)))
        em.set_r(3, count)

    e.write(FILE_READ_SLOT, struct.pack(">I", T.fake_function(e, file_read)))

    def decompress(em):
        w = em.wl
        w.dec_calls += 1
        source, dest = em.r(7), em.r(8)
        src, dst = w.block(source), w.block(dest)
        w.event(4, em.r(3) == w.obj, em.r(4), em.r(5), em.r(6) & 0xFF, w.ordinal(source),
                fnv(em.read(source, src[1])) if src else 0, w.ordinal(dest), fbits(em, 1), fbits(em, 2), fbits(em, 3))
        if dst and dst[1]:
            em.write(dest, bytes((j * 37 + w.dec_calls * 101 + 5) & 0xFF for j in range(dst[1])))

    vtable = T.persist_alloc(e, 16, struct.pack(">IIII", 0, 0, T.fake_function(e, decompress), 0))
    e.wl_vtable = vtable

    def color_fast(em):
        em.wl.event(5, em.r(3) == em.wl.obj, em.r(4), em.r(5), em.r(6), em.r(7), fbits(em, 1), fbits(em, 2),
                    fbits(em, 3))

    def mono_fast(em):
        em.wl.event(6, em.r(3) == em.wl.obj, em.r(4), em.r(5), em.r(6), em.r(7), fbits(em, 1))

    T.stub_internal(e, COLOR_FAST, color_fast)
    T.stub_internal(e, MONO_FAST, mono_fast)
    lib = runner.lib
    lib.tpw_wl_reset.argtypes = [C.c_int32]
    lib.tpw_wl_load.restype = C.c_int32
    lib.tpw_wl_load.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p, C.c_uint32, C.c_int32, C.c_void_p,
                                C.c_void_p, C.c_uint32]
    lib.tpw_wl_dispatch.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_float, C.c_float,
                                    C.c_float]


def host_events(lib):
    n = C.c_int32.in_dll(lib, "tpw_wl_nevents").value
    table = (C.c_int64 * 12 * 64).in_dll(lib, "tpw_wl_events")
    return [tuple(table[i][k] for k in range(12)) for i in range(n)]


# ----------------------------------------------------------------- load_wavelet


def load_ppc(e, c):
    w = e.wl = PpcWorld(c["stream"], 0)
    init = c["init"] or (0, 0, 0)
    data = 0
    if c["init_data"] is not None:
        data = e.alloc(max(len(c["init_data"]), 4), c["init_data"])
        w.blocks.append([data, len(c["init_data"])])
    bitmap = e.alloc(16, struct.pack(">IIII", *init, data))
    w.obj = e.alloc(16, struct.pack(">I", e.wl_vtable))   # the fake CWavelet: its vptr, slot 0 at +8
    w.fail_at = c["fail"]
    ret = T.call_ppc(e, LOAD_ADDR, [bitmap, FILE, w.obj], max_insns=20_000_000)[0]
    width, height, depth, mdata = struct.unpack(">IIII", e.read(bitmap, 16))
    block = w.block(mdata) if mdata else None
    return dict(ret=T.s32(ret), fields=(width, height, depth), length=(block[1] if block else MASK) if mdata else MASK,
                data=e.read(mdata, block[1]) if block else b"", events=w.events)


def load_host(lib, c):
    init = (C.c_uint32 * 3)(*c["init"]) if c["init"] else None
    init_data = c["init_data"]
    buf = C.create_string_buffer(init_data, max(len(init_data), 1)) if init_data is not None else None
    s = C.create_string_buffer(c["stream"], max(len(c["stream"]), 1))
    out_desc = (C.c_uint32 * 4)()
    out_data = (C.c_uint8 * OUT_CAP)()
    ret = lib.tpw_wl_load(C.cast(init, C.c_void_p) if init else None, C.cast(buf, C.c_void_p) if buf else None,
                          len(init_data) if init_data is not None else MASK, C.cast(s, C.c_void_p), len(c["stream"]),
                          c["fail"], C.cast(out_desc, C.c_void_p), C.cast(out_data, C.c_void_p), OUT_CAP)
    length = out_desc[3]
    return dict(ret=ret, fields=tuple(out_desc[:3]), length=length,
                data=bytes(out_data[:length]) if length != MASK else b"", events=host_events(lib))


def sizes_for(depth, w, h):
    return depth * w * h


def edges_load():
    r = random.Random(8601)
    out = []
    n = 0
    shapes = ((1, 1), (2, 3), (3, 2), (7, 5), (16, 16))
    for depth in (1, 3, 4):
        for w, h in shapes:
            size = depth * w * h
            floats = [FLOATS[(n + i) % len(FLOATS)] for i in range(4)][:{1: 1, 3: 3, 4: 4}[depth]]
            n += 3
            data = plane_file(r, w, h, depth, nbytes=r.choice((0, 3, 40)), floats=floats)
            out.append(case(data))                                              # an empty object
            out.append(case(data, (w, h, depth), payload(r, size)))             # the same size and depth
            out.append(case(data, (w + 1, h, depth), payload(r, (w + 1) * h * depth)))   # a different size
            out.append(case(data, (w, h, 2), payload(r, 9)))                    # a different depth
            out.append(case(data, (w, h, depth), None))                         # sizes set, no buffer
    # depths that read nothing more, and the product wrap
    for depth in (0, 2, 5, 6, 8, 0xFFFFFFFF, 0x80000000, 0x80000001, 0x40000001, 0x10000001):
        for w, h in ((1, 1), (2, 1), (3, 2), (4, 1)):
            out.append(case(stream(w, h, depth, tail=payload(r, 16))))
            out.append(case(stream(w, h, depth), (w, h, depth), payload(r, 4)))
    # a width or height of 0 (new[0], memset 0, the decompressor called with 0)
    for depth in (1, 3, 4):
        for w, h in ((0, 0), (0, 5), (5, 0)):
            out.append(case(plane_file(r, w, h, depth, nbytes=7)))
            out.append(case(plane_file(r, w, h, depth, nbytes=7), (3, 3, depth), payload(r, 9 * depth)))
    # the floats in every bit pattern at every position
    for depth in (1, 3, 4):
        k = {1: 1, 3: 3, 4: 4}[depth]
        for i, pattern in enumerate(FLOATS):
            floats = [FLOATS[(i + j * 5 + 1) % len(FLOATS)] for j in range(k)]
            floats[i % k] = pattern
            out.append(case(plane_file(r, 2, 2, depth, nbytes=9, floats=floats)))
    # unequal colour and alpha sizes at depth 4, and payloads longer than what is left of the file
    out.append(case(stream(3, 2, 4, [1, 2, 3, 4], [10, 3], [payload(r, 10), payload(r, 3)])))
    out.append(case(stream(3, 2, 4, [1, 2, 3, 4], [3, 10], [payload(r, 3), payload(r, 10)])))
    out.append(case(stream(3, 2, 4, [1, 2, 3, 4], [64, 64], [payload(r, 10)])))
    out.append(case(stream(3, 2, 1, [0x3F800000], [64], [payload(r, 10)])))
    out.append(case(stream(3, 2, 3, [1, 2, 3], [64], [])))
    # a truncated header and an empty file
    out.append(case(b""))
    out.append(case(words(5, 5)))
    out.append(case(words(2, 2, 4, 1)))
    out.append(case(words(2, 2, 1), (2, 2, 1), payload(r, 4)))
    # a pixel count of 2^31 or more is negative to the interleave's compare (the product is also a wrapped size)
    for depth in (4, 3, 1):
        out.append(case(plane_file(r, 0x10000, 0x8001, depth, nbytes=5)))
    out.append(case(plane_file(r, 0x10000, 0x10000, 4, nbytes=5)))
    out.append(case(plane_file(r, 0x80000000, 1, 4, nbytes=5)))
    out.append(case(plane_file(r, 1, 0x80000000, 4, nbytes=5)))
    out.append(case(plane_file(r, 0xFFFFFFFF, 1, 4, nbytes=5)))
    out.append(case(plane_file(r, 0xFFFFFFFF, 0xFFFFFFFF, 3, nbytes=5)))
    # a huge payload size is refused by the allocator (the answer is NULL)
    out.append(case(stream(2, 2, 1, [0x3F800000], [0xFFFFFFFF], [payload(r, 8)])))
    out.append(case(stream(2, 2, 3, [1, 2, 3], [0x7FFFFFFF], [payload(r, 8)])))
    out.append(case(stream(2, 2, 4, [1, 2, 3, 4], [4, 0xFFFFFFFF], [payload(r, 8)])))
    out.append(case(stream(2, 2, 4, [1, 2, 3, 4], [0xFFFFFFFF, 4], [payload(r, 8)])))
    # allocation failures at every site: 1 the pixel buffer, 2 the (first) payload buffer, 3 the alpha payload,
    # 4 the 3-byte plane, 5 the 1-byte plane (the last two only with no pixels: the original reads them otherwise)
    for depth in (1, 3, 4):
        for w, h in ((3, 2), (1, 1), (0, 3), (3, 0)):
            data = plane_file(r, w, h, depth, nbytes=6)
            for fail in (1, 2, 3, 4, 5, 6):
                if depth == 4 and w * h > 0 and fail in (4, 5):
                    continue
                out.append(case(data, fail=fail))
                out.append(case(data, (w, h, depth), payload(r, w * h * depth), fail=fail))
    return out


def random_load(r):
    depth = r.choice((1, 3, 4, 4, 3, 1, 1, 3, 4, 0, 2, 5, r.getrandbits(32)))
    w, h = r.randrange(0, 65), r.randrange(0, 65)
    if r.random() < 0.15:
        w, h = r.choice((1, 2, 3, 7, 16)), r.choice((1, 2, 3, 7, 16))
    nfloats = {1: 1, 3: 3, 4: 4}.get(depth, 0)
    nplanes = {1: 1, 3: 1, 4: 2}.get(depth, 0)
    floats = [r.choice(FLOATS) if r.random() < 0.6 else r.getrandbits(32) for _ in range(nfloats)]
    sizes = [r.randrange(0, 4097) if r.random() < 0.8 else r.choice((0, 1, 4096)) for _ in range(nplanes)]
    data = stream(w, h, depth, floats, sizes, [payload(r, n) for n in sizes])
    if r.random() < 0.1:
        data = data[:r.randrange(0, len(data) + 1)]
    init, init_data = None, None
    roll = r.random()
    if roll < 0.35:
        iw, ih, id_ = (w, h, depth) if r.random() < 0.5 else (r.randrange(0, 40), r.randrange(0, 40), r.choice((1, 2, 3, 4)))
        init = (iw, ih, id_)
        if r.random() < 0.9:
            init_data = payload(r, min(iw * ih * id_, 4096))
    fail = 0
    if r.random() < 0.12:
        fail = r.randrange(1, 7)
        if depth == 4 and w * h > 0 and fail in (4, 5):
            fail = 0
    return case(data, init, init_data, fail)


# ----------------------------------------------------------------- the dispatcher

THIS_PPC = 0x00012340
FLOAT_VALUES = [0.0, 1.0, -1.0, 0.5, 1.5e-45, 3.4028235e38, float("inf"), float("-inf"), 123.456, -0.0, float("nan"),
                1.0e-39]


def dispatch_case(colour, width, height, source, dest, a, b, c):
    return dict(colour=colour, w=width, h=height, src=source, dst=dest, f=(a, b, c))


def edges_dispatch():
    out = []
    for colour in (0, 1):
        out.append(dispatch_case(colour, 17, 33, 0x1000, 0x2000, 1.0, 2.0, 3.0))
        out.append(dispatch_case(colour, 0, 0, 0, 0, 0.0, 0.0, 0.0))
        out.append(dispatch_case(colour, MASK, 0x80000000, MASK, 0x80000000, -1.0, -2.0, -3.0))
        for i, value in enumerate(FLOAT_VALUES):
            out.append(dispatch_case(colour, 100 + i, 200 + i, 0x3000 + i, 0x4000 + i, value,
                                     FLOAT_VALUES[(i + 3) % len(FLOAT_VALUES)], FLOAT_VALUES[(i + 7) % len(FLOAT_VALUES)]))
    return out


def random_dispatch(r):
    def f():
        return r.choice(FLOAT_VALUES) if r.random() < 0.3 else T.f32(r.uniform(-1000, 1000))
    return dispatch_case(r.randrange(2), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), f(),
                         f(), f())


def dispatch_ppc(e, c):
    w = e.wl = PpcWorld(b"", 0)
    w.obj = THIS_PPC
    args = [THIS_PPC, c["w"], c["h"], c["colour"], c["src"], c["dst"]] + [T.F(x) for x in c["f"]]
    T.call_ppc(e, DISPATCH_ADDR, args)
    return dict(events=w.events)


def dispatch_host(lib, c):
    lib.tpw_wl_dispatch(c["colour"], c["w"], c["h"], c["src"], c["dst"], *c["f"])
    return dict(events=host_events(lib))


TESTS = [
    FnTest("Bitmap::load_wavelet 0x10047294", load_ppc, load_host, edge=edges_load, rnd=random_load, exact_nan=True),
    FnTest("CWaveletImp::Decompress 0x10039694", dispatch_ppc, dispatch_host, edge=edges_dispatch,
           rnd=random_dispatch, exact_nan=True),
]
