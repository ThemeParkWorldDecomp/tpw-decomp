"""Unit w71a, the MPEG audio decoder object CMpegBase, part 1, against libtpw_unit_MpegBase.dylib (tests/unit_MpegBase.cpp), in the
sound library ("sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0):
  src/sound/FileAccess.cpp, src/sound/LbMpeg.cpp, src/sound/MpegDecode.cpp  the pool (CMpegBase::operator new 0x10002398, operator delete 0x1000245c, TopUp 0x100024f4, SetLevel
                      0x100025cc, ClearCache 0x10002310), the constructor 0x10002604 and destructor 0x10002640, the two static
                      objects (_CS_CMpegBase's destructor 0x10004110, CMpegBase_DecodeMethod's constructor 0x1000422c and
                      destructor 0x10004190), OpenSynth 0x100026d8, CloseSynth 0x100027f0, OpenLayer 0x10002bb4, CloseLayer
                      0x10002c90, Open (two forms) 0x10002d9c and 0x10002ea4, Close 0x1000301c, Reset 0x10002fa8, SeekBits
                      0x10002754, GetBits 0x1000278c, ProcessHeader 0x10002820, DecodeHeader 0x10002a44, GetHeader (two forms)
                      0x10002aec and 0x10002b54, Decode 0x10002cc4, Read 0x1000309c, Decode_Layer1 0x100043b0, get_sb_stereo
                      0x10004270, Init_Tables 0x10005a58, Close_Tables 0x10005aa8, and muldiv 0x100022e4

The functions run as the original, one script of operations per case. After every operation the whole object is compared as a
"Mac image" (the 0x24f0 bytes in the Mac's byte order: synthesis buffers, frame, every field; the pool link as a label, the
stream pointer as an offset in the stream, the vptr as 1 for the base class and 2 for a derived one), with the result, the output
buffer (16 K, shorts in the Mac's byte order), Decode_Layer1's three scratch arrays and the pool (its six statics and the chain
of idle objects as labels). The whole call log is compared in order too. Every callee outside the unit is a fake on both sides
that records the same event: the heap (new with the size, delete of a labelled block), PolySynth (the output offset, channels,
bit position, the fractions; it also writes samples that are a function of what it was given), Decode_Layer2 (the output offset,
bit position and frame size; the queue "L2" is its answer; it writes a pattern), and, in the fake-derived mode, the three
virtuals a derived class overrides (SeekBits, DecodeHeader, GetHeader; the queues "DH" and "GH" are their answers). On the
PowerPC side the object lives in the emulator's scratch with the REAL vtable word 0x10039424 (so the real virtuals run) or a
fake vtable of three fake_function slots (+0x0c, +0x10, +0x14), the runtime's __nw__FUl and __dl__FPv and the two library
functions of unit w71b are Python stubs, and memset and memcpy are the standard stubs.
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_MpegBase"
MASK = 0xFFFFFFFF
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

# the functions
A_CTOR, A_DTOR, A_NEW, A_DELETE, A_TOPUP, A_SETLEVEL, A_CLEARCACHE = 0x10002604, 0x10002640, 0x10002398, 0x1000245C, 0x100024F4, 0x100025CC, 0x10002310
A_CS_DTOR, A_INIT_TABLES, A_CLOSE_TABLES, A_DM_CTOR, A_DM_DTOR = 0x10004110, 0x10005A58, 0x10005AA8, 0x1000422C, 0x10004190
A_OPEN_SYNTH, A_CLOSE_SYNTH, A_OPEN_LAYER, A_CLOSE_LAYER = 0x100026D8, 0x100027F0, 0x10002BB4, 0x10002C90
A_OPEN_PTR, A_OPEN_HDR, A_CLOSE, A_RESET, A_SEEKBITS, A_GETBITS = 0x10002D9C, 0x10002EA4, 0x1000301C, 0x10002FA8, 0x10002754, 0x1000278C
A_PROCESS, A_DECODE_HEADER, A_GET_HEADER, A_GET_HEADER_H, A_DECODE, A_READ = 0x10002820, 0x10002A44, 0x10002AEC, 0x10002B54, 0x10002CC4, 0x1000309C
A_LAYER1, A_SB_STEREO, A_MULDIV = 0x100043B0, 0x10004270, 0x100022E4
A_POLYSYNTH, A_LAYER2 = 0x10006C38, 0x10005698
# the data
S_FIRST, S_CURRENT, S_LOW, S_HIGH, S_TOTAL, S_LEVEL = 0x1003E35C, 0x1003E350, 0x1003E354, 0x1003E358, 0x1003E34C, 0x10035D8C
USING_MMX = 0x1003E380
D_SAMPLES, D_SCALE, D_ALLOC = 0x1003E388, 0x1003E488, 0x1003E588
D_MULS = 0x10035DF0
REAL_VTABLE = 0x10039424
IMG = 0x24F0
OUT_SIZE = 0x4000

# the fields of the object after the frame: name -> (offset, width); the order is the Mac's
FIELDS = {
    "rate": (0x2488, 4), "rate2": (0x248C, 4), "fbytes": (0x2490, 4), "layer": (0x2494, 1), "bitrate": (0x2498, 4),
    "fsize": (0x249C, 4), "fbytes2": (0x24A0, 4), "frames": (0x24A4, 4), "data": (0x24A8, 4), "bitpos": (0x24AC, 4),
    "fstart": (0x24B0, 4), "total": (0x24B4, 4), "pos": (0x24B8, 4), "sblimit": (0x24BC, 4), "open": (0x24C0, 1),
    "bo": (0x24C1, 1), "mpeg25": (0x24C2, 1), "lsf": (0x24C3, 1), "sri2": (0x24C4, 1), "u24c5": (0x24C5, 1),
    "id": (0x24C6, 1), "nocrc": (0x24C7, 1), "bri": (0x24C8, 1), "sri": (0x24C9, 1), "padding": (0x24CA, 1),
    "mode": (0x24CB, 1), "modeext": (0x24CC, 1), "copyright": (0x24CD, 1), "original": (0x24CE, 1), "u24cf": (0x24CF, 1),
    "channels": (0x24D0, 4), "channels32": (0x24D4, 4), "t1": (0x24D8, 4), "t2": (0x24DC, 4), "t3": (0x24E0, 4),
    "t4": (0x24E4, 4), "fdecoded": (0x24E8, 4), "fconsumed": (0x24EC, 4),
}

(OP_CTOR, OP_DTOR, OP_DTOR_NULL, OP_NEW, OP_DELETE, OP_TOPUP, OP_SETLEVEL, OP_CLEARCACHE, OP_CS_NEW, OP_CS_DTOR, OP_DM_NEW,
 OP_DM_CTOR, OP_DM_DTOR, OP_DM_DTOR_NULL, OP_INIT_TABLES, OP_CLOSE_TABLES, OP_OPEN_SYNTH, OP_CLOSE_SYNTH, OP_OPEN_LAYER,
 OP_CLOSE_LAYER, OP_OPEN_PTR, OP_OPEN_HDR, OP_CLOSE, OP_RESET, OP_GETBITS, OP_SEEKBITS, OP_PROCESS_HEADER, OP_DECODE_HEADER,
 OP_GET_HEADER, OP_GET_HEADER_H, OP_DECODE, OP_READ, OP_LAYER1, OP_SB_STEREO, OP_MULDIV, OP_SET_MMX) = range(36)

STUBS = None
FAKE_VTABLE = None


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------- the images

def image(case, stream_base=None):
    """The Mac image of the case's initial object. stream_base: the PowerPC address of the stream (the data field then holds
    an address), None for the host (the data field holds the offset)."""
    img = bytearray([case["fill"]]) * IMG
    if case.get("synth") is not None:
        img[0:0x1200] = case["synth"]
    if case.get("frame") is not None:
        img[0x1240:0x2480] = case["frame"]
    vptr = case.get("vptr")
    if vptr is None:
        vptr = FAKE_VTABLE if case["mode"] == 1 else REAL_VTABLE
    img[0x2480:0x2484] = struct.pack(">I", vptr & MASK)
    img[0x2484:0x2488] = b"\0\0\0\0"
    state = dict(case["state"])
    state.setdefault("data", None)
    for name, value in state.items():
        off, width = FIELDS[name]
        if name == "data":
            if value is None:
                value = 0 if stream_base is not None else MASK
            elif stream_base is not None:
                value = stream_base + value
        img[off:off + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, "big")
    return img


def parse(raw):
    """A Mac image as a dict of its parts and named fields."""
    d = {"synth": bytes(raw[0:0x1200]), "spare": bytes(raw[0x1200:0x1240]), "frame": bytes(raw[0x1240:0x2480]),
         "vptr": struct.unpack_from(">I", raw, 0x2480)[0], "next": struct.unpack_from(">I", raw, 0x2484)[0]}
    for name, (off, width) in FIELDS.items():
        d[name] = int.from_bytes(raw[off:off + width], "big", signed=width == 4)
    return d


# ---------------------------------------------------------------- the PowerPC side

def make_fake_vtable(e):
    """A vtable whose slots +0x0c, +0x10 and +0x14 are fakes (SeekBits, DecodeHeader, GetHeader), +0x08 the real destructor."""
    def seek(e):
        L = e.cl
        L.rec("SeekBits", (L.lab(e.r(3)), s32(e.r(4))))

    def decode_header(e):
        L = e.cl
        L.rec("DecodeHeader", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("DH", 0)))

    def get_header(e):
        L = e.cl
        L.rec("GetHeader", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("GH", 0)))

    real_destructor = e.u32(REAL_VTABLE + 8)
    words = [0, 0, real_destructor, T.fake_function(e, seek), T.fake_function(e, decode_header), T.fake_function(e, get_header)]
    return T.persist_alloc(e, 0x20, struct.pack(">6I", *words))


def where(e, p):
    """Where an output pointer points, as unit_MpegBase.cpp's Where does."""
    if e.out_base <= p < e.out_base + OUT_SIZE:
        return (p - e.out_base) // 2
    frame = e.obj_base + 0x1240
    if frame <= p < frame + 0x1240:
        return 0x10000 + (p - frame) // 2
    return -1


def put16(e, p, value):
    if where(e, p) >= 0:
        e.write(p, struct.pack(">H", value & 0xFFFF))


def build_stubs():
    S = CL.Stubs()

    def new(e):
        L = e.cl
        size = e.r(3)
        L.rec("new", (size,))
        block = e.alloc(max(size, 16))
        L.newlab(block)
        e.set_r(3, block)

    def delete(e):
        L = e.cl
        block = e.r(3)
        if block != 0 and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)

    def polysynth(e):
        # r3 this, r4 the output, r5 the fractions
        L = e.cl
        this, out, fraction = e.r(3), e.r(4), e.r(5)
        channels = s32(e.u32(this + 0x24D0))
        nch = 2 if channels == 2 else 1
        L.rec("PolySynth", (L.lab(this), where(e, out), channels, s32(e.u32(this + 0x24AC)), s32(e.u32(this + 0x24D4))))
        words = struct.unpack(">%dI" % (32 * nch), e.read(fraction, 128 * nch))
        for i in range(0, 32 * nch, 8):
            L.rec("Fraction", (i,) + words[i:i + 8])
        call = L.poly_calls
        L.poly_calls += 1
        if out != 0:
            for i in range(32 * nch):
                put16(e, out + 2 * i, ((words[i] >> 7) ^ ((i * 0x9E3 + call * 0x35) & MASK)))

    def layer2(e):
        L = e.cl
        this, out = e.r(3), e.r(4)
        fb, fb2 = s32(e.u32(this + 0x2490)), s32(e.u32(this + 0x24A0))
        bitpos = s32(e.u32(this + 0x24AC))
        L.rec("Decode_Layer2", (L.lab(this), where(e, out), bitpos, fb))
        call = L.l2_calls
        L.l2_calls += 1
        shorts = min(fb, fb2)
        if shorts > 0x1200:
            shorts = 0x1200
        shorts = int(shorts / 2)
        if out != 0:
            for i in range(shorts):
                put16(e, out + 2 * i, i * 0x57 + call * 0x1B3 + (bitpos & 0xFFFF))
        e.set_r(3, T.u32(L.ret("L2", 0)))

    S.imp("__nw__FUl", new)
    S.imp("__dl__FPv", delete)
    S.internal(A_POLYSYNTH, polysynth)
    S.internal(A_LAYER2, layer2)
    return S


def setup(runner):
    global STUBS, FAKE_VTABLE
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()
    FAKE_VTABLE = make_fake_vtable(runner.emu)


def ppc_snapshot(e, L, ctx):
    raw = bytearray(e.read(ctx["obj"], IMG))
    vptr = struct.unpack_from(">I", raw, 0x2480)[0]
    raw[0x2480:0x2484] = struct.pack(">I", 1 if vptr == REAL_VTABLE else 2 if vptr == FAKE_VTABLE else 3)
    link = struct.unpack_from(">I", raw, 0x2484)[0]
    raw[0x2484:0x2488] = struct.pack(">I", L.lab(link) & MASK)
    data = struct.unpack_from(">I", raw, 0x24A8)[0]
    raw[0x24A8:0x24AC] = struct.pack(">I", MASK if data == 0 else (data - ctx["stream"]) & MASK)
    return parse(raw)


def ppc_pool(e, L):
    first = e.u32(S_FIRST)
    state = [L.lab(first), s32(e.u32(S_CURRENT)), s32(e.u32(S_LOW)), s32(e.u32(S_HIGH)), s32(e.u32(S_TOTAL)), s32(e.u32(S_LEVEL))]
    node, n = first, 0
    while node != 0 and n < 40:
        state.append(L.lab(node))
        node = e.u32(node + 0x2484)
        n += 1
    state.append(-9)
    return state


def ppc_scratch(e):
    return (list(struct.unpack(">64I", e.read(D_ALLOC, 256))) + [s32(v) for v in struct.unpack(">64I", e.read(D_SCALE, 256))] +
            [s32(v) for v in struct.unpack(">64I", e.read(D_SAMPLES, 256))])


def ppc_op(e, L, ctx, op):
    """One operation on the PowerPC side; returns the result as the comparison wants it."""
    kind, a, b, c = (tuple(op) + (0, 0, 0))[:4]
    obj = ctx["obj"]
    call = lambda addr, args: T.call_ppc(e, addr, args)[0]
    if kind == OP_CTOR:
        call(A_CTOR, [obj])
        return 0
    if kind == OP_DTOR:
        call(A_DTOR, [obj, a & MASK])
        return 0
    if kind == OP_DTOR_NULL:
        call(A_DTOR, [0, 1])
        return 0
    if kind == OP_NEW:
        block = call(A_NEW, [b & MASK])
        ctx["blocks"][a] = block
        return L.lab(block)
    if kind == OP_DELETE:
        call(A_DELETE, [0 if a < 0 else ctx["blocks"][a], b & MASK])
        return 0
    if kind == OP_TOPUP:
        call(A_TOPUP, [])
        return 0
    if kind == OP_SETLEVEL:
        call(A_SETLEVEL, [a & MASK])
        return 0
    if kind == OP_CLEARCACHE:
        call(A_CLEARCACHE, [])
        return 0
    if kind == OP_CS_NEW:
        block = e.alloc(16)
        L.rec("new", (1,))
        L.newlab(block)
        ctx["cs"] = block
        return L.lab(block)
    if kind == OP_CS_DTOR:
        call(A_CS_DTOR, [0 if a == -100 else ctx["cs"], 1 if a == -100 else a & MASK])
        return 0
    if kind == OP_DM_NEW:
        block = e.alloc(16)
        L.rec("new", (1,))
        L.newlab(block)
        ctx["dm"] = block
        call(A_DM_CTOR, [block])
        return L.lab(block)
    if kind == OP_DM_CTOR:
        call(A_DM_CTOR, [ctx["dm"]])
        return 0
    if kind == OP_DM_DTOR:
        call(A_DM_DTOR, [ctx["dm"], a & MASK])
        return 0
    if kind == OP_DM_DTOR_NULL:
        call(A_DM_DTOR, [0, 1])
        return 0
    if kind == OP_INIT_TABLES:
        call(A_INIT_TABLES, [obj])
        return 0
    if kind == OP_CLOSE_TABLES:
        call(A_CLOSE_TABLES, [obj])
        return 0
    if kind == OP_OPEN_SYNTH:
        call(A_OPEN_SYNTH, [obj])
        return 0
    if kind == OP_CLOSE_SYNTH:
        call(A_CLOSE_SYNTH, [obj])
        return 0
    if kind == OP_OPEN_LAYER:
        return s32(call(A_OPEN_LAYER, [obj]))
    if kind == OP_CLOSE_LAYER:
        return s32(call(A_CLOSE_LAYER, [obj]))
    if kind == OP_OPEN_PTR:
        return s32(call(A_OPEN_PTR, [obj, 0 if a < 0 else ctx["stream"] + a, b & MASK]))
    if kind == OP_OPEN_HDR:
        return s32(call(A_OPEN_HDR, [obj, a & MASK, b & MASK]))
    if kind == OP_CLOSE:
        return s32(call(A_CLOSE, [obj]))
    if kind == OP_RESET:
        call(A_RESET, [obj])
        return 0
    if kind == OP_GETBITS:
        return call(A_GETBITS, [obj, a & MASK]) & MASK
    if kind == OP_SEEKBITS:
        call(A_SEEKBITS, [obj, a & MASK])
        return 0
    if kind == OP_PROCESS_HEADER:
        return s32(call(A_PROCESS, [obj, a & MASK]))
    if kind == OP_DECODE_HEADER:
        return s32(call(A_DECODE_HEADER, [obj]))
    if kind == OP_GET_HEADER:
        return s32(call(A_GET_HEADER, [obj]))
    if kind == OP_GET_HEADER_H:
        return s32(call(A_GET_HEADER_H, [obj, a & MASK]))
    if kind == OP_DECODE:
        return s32(call(A_DECODE, [obj, 0 if a < 0 else e.out_base + a]))
    if kind == OP_READ:
        return s32(call(A_READ, [obj, 0 if a < 0 else e.out_base + a, b & MASK]))
    if kind == OP_LAYER1:
        return s32(call(A_LAYER1, [obj, 0 if a < 0 else e.out_base + a]))
    if kind == OP_SB_STEREO:
        sb = ctx["sb"]
        call(A_SB_STEREO, [obj, a & MASK, sb[0], sb[1], e.out_base + b, sb[2]])
        return 0
    if kind == OP_MULDIV:
        return s32(call(A_MULDIV, [a & MASK, b & MASK, c & MASK]))
    if kind == OP_SET_MMX:
        e.write(USING_MMX, struct.pack(">I", a & MASK))
        return 0
    raise ValueError(kind)


def ppc_case(e, case):
    e.heap_in_scratch = True
    obj = e.alloc(IMG + 16)
    stream_bytes = case.get("stream", b"")
    stream = e.alloc(0x10040)
    out = e.alloc(OUT_SIZE, bytes([case.get("outfill", 0)]) * OUT_SIZE)
    e.write(stream, stream_bytes)
    e.out_base, e.obj_base = out, obj
    L = CL.PpcLog({obj: 1}, case.get("q"))
    L.poly_calls = 0
    L.l2_calls = 0
    e.cl = L
    ctx = dict(obj=obj, stream=stream, blocks=[0] * 16, cs=0, dm=0)
    sb = case.get("sb")
    if sb is not None:
        alloc = e.alloc(64, struct.pack(">64b", *sb["alloc"]))
        scale = e.alloc(256, struct.pack(">64I", *sb["scale"]))
        mul = e.alloc(0x2000, struct.pack(">2048i", *sb["mul"]))
        ctx["sb"] = (alloc, scale, mul)
    saved = STUBS.activate(e)
    try:
        e.write(obj, bytes(image(case, stream)))
        pool = case.get("pool", {})
        for address, key in ((S_CURRENT, "cur"), (S_LOW, "low"), (S_HIGH, "high"), (S_TOTAL, "total")):
            e.write(address, struct.pack(">I", pool.get(key, 0) & MASK))
        e.write(S_FIRST, b"\0\0\0\0")
        e.write(S_LEVEL, struct.pack(">I", pool.get("level", 10) & MASK))
        e.write(USING_MMX, struct.pack(">I", case.get("mmx", 0)))
        e.write(D_SAMPLES, b"\0" * 0x300)
        steps = []
        for op in case["ops"]:
            result = ppc_op(e, L, ctx, op)
            steps.append(dict(result=result, obj=ppc_snapshot(e, L, ctx), out=e.read(out, OUT_SIZE), scratch=ppc_scratch(e),
                              pool=ppc_pool(e, L), mmx=s32(e.u32(USING_MMX))))
        return dict(steps=steps, log=L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


# ---------------------------------------------------------------- the host side

def bind(lib):
    lib.tpw_mp_begin.argtypes = [C.c_int, C.c_int, C.c_int]
    lib.tpw_mp_object.restype = C.c_void_p
    lib.tpw_mp_set_stream.argtypes = [C.c_char_p, C.c_int]
    lib.tpw_mp_load.argtypes = [C.c_char_p]
    lib.tpw_mp_snapshot.argtypes = [C.c_void_p, C.c_int]
    lib.tpw_mp_get_out.argtypes = [C.c_void_p]
    lib.tpw_mp_scratch.argtypes = [C.c_void_p]
    lib.tpw_mp_set_sb.argtypes = [C.c_char_p, C.c_char_p, C.c_char_p]
    lib.tpw_mp_pool_init.argtypes = [C.c_int32] * 5
    lib.tpw_mp_pool_state.argtypes = [C.c_void_p]
    lib.tpw_mp_set_mmx.argtypes = [C.c_int32]
    lib.tpw_mp_get_mmx.restype = C.c_int32
    lib.tpw_mp_op.restype = C.c_int64
    lib.tpw_mp_op.argtypes = [C.c_int, C.c_int64, C.c_int64, C.c_int64]
    lib.tpw_mp_muls_bits.restype = C.c_uint32
    lib.tpw_mp_muls_bits.argtypes = [C.c_int, C.c_int]
    lib.tpw_mp_int_alloc.restype = C.c_int32
    lib.tpw_mp_int_alloc.argtypes = [C.c_int]


RESULT_KIND = {OP_GETBITS: "u32", OP_NEW: "none", OP_CS_NEW: "none", OP_DM_NEW: "none"}


def host_result(op, value):
    kind = op[0]
    if kind in (OP_NEW, OP_CS_NEW, OP_DM_NEW):
        return value
    if kind == OP_GETBITS:
        return value & MASK
    if kind in (OP_OPEN_LAYER, OP_CLOSE_LAYER, OP_OPEN_PTR, OP_OPEN_HDR, OP_CLOSE, OP_PROCESS_HEADER, OP_DECODE_HEADER, OP_GET_HEADER,
                OP_GET_HEADER_H, OP_DECODE, OP_READ, OP_LAYER1, OP_MULDIV):
        return s32(value & MASK)
    return 0


def host_case(lib, case):
    bind(lib)
    lib.tpw_mp_begin(case["mode"], case["fill"], case.get("outfill", 0))
    stream = case.get("stream", b"")
    lib.tpw_mp_set_stream(stream, len(stream))
    lib.tpw_mp_load(bytes(image(case)))
    pool = case.get("pool", {})
    lib.tpw_mp_pool_init(pool.get("cur", 0), pool.get("low", 0), pool.get("high", 0), pool.get("total", 0), s32(pool.get("level", 10)))
    lib.tpw_mp_set_mmx(case.get("mmx", 0))
    sb = case.get("sb")
    if sb is not None:
        lib.tpw_mp_set_sb(struct.pack("<64b", *sb["alloc"]), struct.pack("<64I", *sb["scale"]), struct.pack("<2048i", *sb["mul"]))
    CL.host_begin(lib, {1: lib.tpw_mp_object()}, case.get("q"))
    steps = []
    image_buffer = (C.c_uint8 * IMG)()
    out_buffer = (C.c_uint8 * OUT_SIZE)()
    scratch = (C.c_uint32 * 192)()
    pool_state = (C.c_int64 * 48)()
    try:
        for op in case["ops"]:
            kind, a, b, c = (tuple(op) + (0, 0, 0))[:4]
            value = lib.tpw_mp_op(kind, a, b, c)
            lib.tpw_mp_snapshot(image_buffer, case["fill"])
            lib.tpw_mp_get_out(out_buffer)
            lib.tpw_mp_scratch(scratch)
            lib.tpw_mp_pool_state(pool_state)
            chain = []
            for v in pool_state[6:]:
                chain.append(v)
                if v == -9:
                    break
            steps.append(dict(result=host_result(op, value), obj=parse(bytes(image_buffer)), out=bytes(out_buffer),
                              scratch=[scratch[i] if i < 64 else s32(scratch[i]) for i in range(192)],
                              pool=list(pool_state[0:6]) + chain, mmx=lib.tpw_mp_get_mmx()))
    finally:
        log = CL.host_end(lib)
        lib.tpw_mp_end()
    return dict(steps=steps, log=log)


def run_ppc(emu, case):
    return ppc_case(emu, case)


# ---------------------------------------------------------------- the cases

RATES = [44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000]
BITRATES = [
    [[0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0],
     [0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0],
     [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0]],
    [[0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0],
     [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0],
     [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0]]]
FILLS = [0x00, 0xFF, 0xA5, 0x5A, 0x17]


def hdr(id_=0, layer=1, crc=1, bri=4, sri=0, pad=0, priv=0, mode=3, ext=0, copy=0, orig=0, emph=0, sync=0xFFF):
    """A frame header word (layer 4 is the reserved bit pattern 00)."""
    return ((sync << 20) | (id_ << 19) | (((4 - layer) & 3) << 17) | (crc << 16) | (bri << 12) | (sri << 10) | (pad << 9) |
            (priv << 8) | (mode << 6) | (ext << 4) | (copy << 3) | (orig << 2) | emph) & MASK


def frame_size(h):
    """Bytes after the header of a frame (the same arithmetic as ProcessHeader, for laying out streams), None for no frame."""
    if (h & 0xFFF00000) != 0xFFF00000:
        return None
    id_, layer, bri, sri, pad = (h >> 19) & 1, 4 - ((h >> 17) & 3), (h >> 12) & 15, (h >> 10) & 3, (h >> 9) & 1
    lsf = 1 if id_ == 0 else 0
    if bri == 0 or layer == 4:
        return None
    rate = RATES[sri + 3 * lsf]
    kbps = BITRATES[lsf][layer - 1][bri]
    if layer == 1:
        return (kbps * 12000 // rate + pad) * 4 - 4
    return kbps * 144000 // rate + pad - 4


def rbytes(r, n):
    return r.getrandbits(8 * n).to_bytes(n, "big") if n > 0 else b""


def l1bytes(r, n):
    """Bytes whose nibbles favour 0 and small numbers, so that layer I allocations come out mixed."""
    out = bytearray()
    for _ in range(n):
        hi = r.choice([0, 0, 0, 1, 2, 3, 5, 15, r.randrange(16)])
        lo = r.choice([0, 0, 1, 2, 4, 8, r.randrange(16)])
        out.append((hi << 4) | lo)
    return bytes(out)


def build_stream(r, headers, payload=None):
    """The frames for the header words: header, then frame_size random bytes; a header that is no frame is just its word."""
    out = bytearray()
    for h in headers:
        out += struct.pack(">I", h)
        size = frame_size(h)
        if size is not None and size > 0:
            out += payload(r, size) if payload else rbytes(r, size)
    return bytes(out)


def sane(layer=1, channels=1, bitrate=64, rate=22050, **kw):
    fb = channels * (0x300 if layer == 1 else 0x900)
    d = dict(rate=rate, rate2=rate, fbytes=fb, layer=layer, bitrate=bitrate, fsize=100, fbytes2=fb, frames=7, data=0, bitpos=0,
             fstart=0, total=0, pos=0, sblimit=0, open=1, bo=1, mpeg25=0, lsf=1, sri2=0, id=0, nocrc=1, bri=4, sri=3, padding=0,
             mode=3 if channels == 1 else 0, modeext=0, copyright=0, original=0, channels=channels, channels32=channels << 5,
             t1=1, t2=0, t3=0, t4=0, fdecoded=0, fconsumed=fb, u24c5=0x11, u24cf=0x22)
    d.update(kw)
    return d


def mk(ops, state=None, **kw):
    c = dict(mode=0, fill=0xA5, outfill=0x3C, state=sane() if state is None else state, ops=list(ops), stream=b"", q={}, pool={},
             mmx=0)
    c.update(kw)
    return c


def rfill(r):
    return r.choice(FILLS)


def rand_state(r, names=None):
    """Random values for the named fields (all of them by default), full width."""
    state = {n: r.getrandbits(8 * FIELDS[n][1]) for n in (names or FIELDS)}
    if "data" in state:
        state["data"] = r.choice([0, 4, 16])
    return state


def short(v):
    return ((v & 0xFFFF) ^ 0x8000) - 0x8000


def sdt_streams():
    """Streams from the game's sample banks (read at test time from ROOT/extracted, never copied): a layer I mono stream, a
    layer I stream of the other kind (MPEG-1) and a layer II stereo one; an empty list when the folder is absent."""
    base = ROOT / "extracted/disc-hfs/Theme Park Data/data"
    found, wanted = {}, [("global/sound/UIHD.sdt", "l1"), ("global/sound/RideHD.sdt", "l1b"), ("global/sound/MusicHD.sdt", "l2"),
                         ("global/sound/KidsHD.sdt", "l1c")]
    for name, tag in wanted:
        path = base / name
        if not path.exists():
            continue
        data = path.read_bytes()
        count = struct.unpack_from("<I", data, 0)[0]
        if count > 100000:
            continue
        for index in range(min(count, 60)):
            start = struct.unpack_from("<I", data, 4 + 4 * index)[0] + 40
            word = struct.unpack_from(">I", data, start)[0] if start + 4 <= len(data) else 0
            if (word & 0xFFF00000) == 0xFFF00000 and (tag in ("l2", "l1") or ((word >> 19) & 1) == 1 or tag == "l1c"):
                found.setdefault(tag, []).append(data[start:start + 0x3000])
                if len(found[tag]) >= 3:
                    break
    return found


_SDT = None


def sdt():
    global _SDT
    if _SDT is None:
        _SDT = sdt_streams()
    return _SDT


# ---- the constructor, the destructor, the two static objects

def ctor_edges():
    for fill in FILLS:
        yield mk([(OP_CTOR,)], state={}, fill=fill, vptr=0x12345678)
        yield mk([(OP_CTOR,)], state={"open": 1}, fill=fill, vptr=REAL_VTABLE)
        yield mk([(OP_CTOR,)], state={"open": 0xFF}, fill=fill, vptr=0)


def ctor_random(r):
    return mk([(OP_CTOR,)], state=rand_state(r, [n for n in FIELDS if r.random() < 0.3] + ["open"]), fill=rfill(r),
              vptr=r.getrandbits(32), synth=rbytes(r, 0x1200), frame=rbytes(r, 0x1240))


def dtor_edges():
    pools = [{}, dict(cur=5, low=1, high=5, total=2), dict(cur=0, low=0, high=9, total=0), dict(cur=-1, low=3, high=-4, total=0)]
    for open_ in (0, 1, 2, 0xFF):
        for flag in (1, 0, -1, 2, 0x7FFF, -0x8000):
            yield mk([(OP_DTOR, flag)], state=sane(open=open_), pool=pools[(open_ + flag) % 4])
    yield mk([(OP_TOPUP,), (OP_DTOR, 1)], state=sane(open=1))
    yield mk([(OP_SETLEVEL, 3), (OP_TOPUP,), (OP_DTOR, 1)], state=sane(open=0))
    yield mk([(OP_DTOR_NULL,)])
    yield mk([(OP_DTOR_NULL,)], pool=dict(cur=4, low=1, high=2, total=3))


def dtor_random(r):
    ops = []
    if r.random() < 0.4:
        ops += [(OP_SETLEVEL, r.randrange(0, 5)), (OP_TOPUP,)]
    ops.append((OP_DTOR, short(r.choice([1, 0, -1, 2, r.getrandbits(16)]))))
    pool = dict(cur=r.randrange(-3, 8), low=r.randrange(-3, 8), high=r.randrange(-3, 8), total=r.randrange(-3, 8))
    return mk(ops, state=sane(open=r.choice([0, 1, 2, 0xFF, r.getrandbits(8)])), pool=pool, fill=rfill(r))


def cs_edges():
    pre = [(OP_SETLEVEL, 4), (OP_TOPUP,)]
    for flag in (1, 0, -1, 2):
        yield mk([(OP_CS_NEW,), (OP_CS_DTOR, flag)])
        yield mk([(OP_CS_NEW,)] + pre + [(OP_CS_DTOR, flag)], pool=dict(cur=2, low=1, high=2, total=3))
    yield mk(pre + [(OP_CS_DTOR, -100)])
    yield mk([(OP_CS_DTOR, -100)])
    yield mk([(OP_CS_NEW,), (OP_NEW, 0, 0x24F0), (OP_DELETE, 0, 0x24F0, 1), (OP_CS_DTOR, 1)])


def cs_random(r):
    ops = [(OP_CS_NEW,)] if r.random() < 0.8 else []
    ops += [(OP_SETLEVEL, r.randrange(0, 6)), (OP_TOPUP,)]
    if r.random() < 0.4:
        ops.append((OP_NEW, 0, 0x24F0))
    ops.append((OP_CS_DTOR, r.choice([1, 0, -1, 2]) if ops[0][0] == OP_CS_NEW else -100))
    return mk(ops, pool=dict(cur=r.randrange(0, 6), low=r.randrange(0, 4), high=r.randrange(0, 6), total=r.randrange(0, 6)))


def dm_edges():
    for mmx in (0, 1, 2, -1, 0x100):
        for flag in (1, 0, -1, 2):
            yield mk([(OP_DM_NEW,), (OP_SET_MMX, mmx), (OP_DM_DTOR, flag)])
        yield mk([(OP_DM_NEW,), (OP_SET_MMX, mmx), (OP_DM_CTOR,)])
    yield mk([(OP_SET_MMX, 7), (OP_DM_DTOR_NULL,)])
    yield mk([(OP_DM_NEW,), (OP_DM_DTOR, 1)], mmx=5)


def dm_random(r):
    ops = [(OP_DM_NEW,)]
    for _ in range(r.randrange(0, 3)):
        ops.append((OP_SET_MMX, r.choice([0, 1, r.getrandbits(32)])))
        ops.append((OP_DM_CTOR,))
    ops.append((OP_SET_MMX, r.choice([0, 1, r.getrandbits(32)])))
    ops.append((OP_DM_DTOR, short(r.choice([1, 0, -1, 2, r.getrandbits(16)]))))
    return mk(ops, mmx=r.choice([0, 1]))


# ---- the pool

def pool_edges():
    N = (OP_NEW, 0, 0x24F0)
    yield mk([N])
    yield mk([N, (OP_DELETE, 0, 0x24F0, 1)])
    yield mk([N, (OP_DELETE, 0, 0x24F0, 1), N])
    yield mk([(OP_TOPUP,)])
    yield mk([(OP_TOPUP,), (OP_TOPUP,)])
    yield mk([(OP_SETLEVEL, 3), (OP_TOPUP,), N, (OP_NEW, 1, 0x24F0), (OP_NEW, 2, 0x24F0), (OP_NEW, 3, 0x24F0), (OP_NEW, 4, 0x24F0)])
    yield mk([(OP_SETLEVEL, 3), (OP_TOPUP,), (OP_SETLEVEL, 6), (OP_TOPUP,), (OP_SETLEVEL, 2), (OP_TOPUP,)])
    yield mk([(OP_SETLEVEL, 0), (OP_TOPUP,)])
    yield mk([(OP_SETLEVEL, -5), (OP_TOPUP,)])
    yield mk([(OP_SETLEVEL, 5), (OP_TOPUP,), (OP_CLEARCACHE,), (OP_TOPUP,)])
    yield mk([(OP_SETLEVEL, 5), (OP_TOPUP,), (OP_CLEARCACHE,), N])
    yield mk([(OP_CLEARCACHE,)])
    yield mk([(OP_CLEARCACHE,)], pool=dict(cur=4, low=2, high=6, total=9))
    for size in (0, 1, 0x100, 0x24EF, 0x24F1, 0x5000, 0x24F0):
        yield mk([(OP_NEW, 0, size), (OP_DELETE, 0, size if size else 0, 1)])
        yield mk([(OP_SETLEVEL, 2), (OP_TOPUP,), (OP_NEW, 0, size), (OP_DELETE, 0, 0x24F0 if size == 0x24F0 else size, 1)])
    yield mk([(OP_DELETE, -1, 0x24F0, 0), (OP_DELETE, -1, 5, 0)])
    yield mk([(OP_NEW, 0, 0x24F0), (OP_DELETE, 0, 0x100, 1)])
    yield mk([(OP_NEW, 0, 0x24F0), (OP_NEW, 1, 0x24F0), (OP_DELETE, 0, 0x24F0, 1), (OP_DELETE, 1, 0x24F0, 1), (OP_NEW, 2, 0x24F0), (OP_NEW, 3, 0x24F0)])
    for cur, low, high, total in ((0, 0, 0, 0), (5, 5, 5, 5), (5, 9, 2, 0), (-1, 0, -3, 7), (0x7FFFFFFF, 0, 0, 0), (3, -1, 8, 1)):
        yield mk([(OP_SETLEVEL, 2), (OP_TOPUP,), N, (OP_DELETE, 0, 0x24F0, 1), (OP_TOPUP,)], pool=dict(cur=cur, low=low, high=high, total=total))
    yield mk([(OP_SETLEVEL, 4), (OP_TOPUP,)], pool=dict(cur=0, low=0, high=0, total=0, level=1))


def pool_random(r):
    sizes = [0x24F0] * 5 + [0, 1, 0x100, 0x24EF, 0x24F1, 0x3000]
    ops, live = [], {}
    for _ in range(r.randrange(1, 16)):
        k = r.choice(["new", "new", "delete", "delete", "topup", "level", "clear"])
        if k == "new":
            slot = r.randrange(6)
            if slot in live:
                continue
            size = r.choice(sizes)
            ops.append((OP_NEW, slot, size))
            live[slot] = 1 if size == 0 else size
        elif k == "delete":
            if not live and r.random() < 0.7:
                continue
            if live and r.random() < 0.9:
                slot = r.choice(sorted(live))
                real = live.pop(slot)
                claims = [real, real, 0, 1, 0x100] + ([0x24F0, 0x24F0] if real == 0x24F0 else [])
                ops.append((OP_DELETE, slot, r.choice([c for c in claims if c != 0x24F0 or real == 0x24F0]), 1))
            else:
                ops.append((OP_DELETE, -1, r.choice([0x24F0, 5, 0]), 0))
        elif k == "topup":
            ops.append((OP_TOPUP,))
        elif k == "level":
            ops.append((OP_SETLEVEL, r.choice([0, 1, 2, 3, 5, 8, -2, r.randrange(0, 10)])))
        else:
            ops.append((OP_CLEARCACHE,))
    pool = dict(cur=r.randrange(-2, 8), low=r.randrange(-2, 8), high=r.randrange(-2, 8), total=r.randrange(-2, 8),
                level=r.choice([10, 0, 3, 7, -1]))
    return mk(ops, pool=pool)


# ---- the small leaves: OpenSynth, CloseSynth, CloseLayer, Init_Tables, Close_Tables

def synth_case(r, mmx):
    return mk([(OP_OPEN_SYNTH,)], state=sane(open=r.choice([0, 1])), synth=rbytes(r, 0x1200) if r.random() < 0.7 else bytes([r.choice(FILLS)]) * 0x1200,
              frame=rbytes(r, 0x1240), fill=rfill(r), mmx=mmx)


def open_synth_edges():
    r = random.Random(7)
    for mmx in (0, 1, 2, -1, 0x80000000, 0x100):
        yield synth_case(r, mmx & MASK)
    yield mk([(OP_OPEN_SYNTH,), (OP_OPEN_SYNTH,)], synth=bytes([0xFF]) * 0x1200, fill=0xFF, mmx=1)


def open_synth_random(r):
    return synth_case(r, r.choice([0, 0, 1, r.getrandbits(32)]))


def leaf_edges(op):
    def edges():
        r = random.Random(11)
        for fill in FILLS:
            yield mk([(op,)], state=rand_state(r), fill=fill, synth=rbytes(r, 0x1200), frame=rbytes(r, 0x1240))
        yield mk([(op,), (op,)])
    return edges


def leaf_random(op):
    def rnd(r):
        return mk([(op,)] * r.randrange(1, 3), state=rand_state(r), fill=rfill(r), synth=rbytes(r, 0x1200), frame=rbytes(r, 0x1240))
    return rnd


# ---- OpenLayer

MIN_BITRATE = ((1 << 24) * pow(1125, -1, 1 << 25)) % (1 << 25)   # a bit rate whose product with 144000 is 0x80000000


def open_layer_state(layer, channels, bitrate, rate, **kw):
    return sane(layer=layer, channels=channels, bitrate=bitrate, rate=rate, **kw)


def open_layer_edges():
    for layer in (1, 2, 3, 4, 0, 255):
        for channels in (1, 2):
            yield mk([(OP_OPEN_LAYER,)], state=open_layer_state(layer, channels, 64, 22050, fbytes=0x1234, bo=9))
    for rate in RATES:
        for kbps in (8, 32, 64, 128, 448):
            yield mk([(OP_OPEN_LAYER,)], state=open_layer_state(1, 1, kbps, rate))
    for kbps, rate in ((0x7FFFFFFF, 22050), (-1, 22050), (-0x80000000, 22050), (64, -22050), (64, 1), (64, 0x7FFFFFFF), (0, 22050),
                       (MIN_BITRATE, -1), (MIN_BITRATE, 1), (MIN_BITRATE, 22050), (64, 0), (0, 0), (100000, 3), (64, 99), (64, 100),
                       (64, 101), (-64, 22050), (64, -99), (64, -100), (64, -101), (64, 32), (1, 22050), (1000, 100)):
        yield mk([(OP_OPEN_LAYER,)], state=open_layer_state(1, 2, kbps, rate))
    yield mk([(OP_OPEN_LAYER,), (OP_OPEN_LAYER,)])


def open_layer_random(r):
    kbps = r.choice([r.randrange(1, 460), r.randrange(-1000, 100000), r.getrandbits(32), 64])
    rate = r.choice([r.choice(RATES), r.randrange(1, 100000), -r.randrange(1, 100000), r.getrandbits(32) | 1])
    return mk([(OP_OPEN_LAYER,)], state=open_layer_state(r.choice([1, 2, 3, 4, 0, r.getrandbits(8)]), r.choice([1, 2, 0, 3, -1, r.getrandbits(32)]),
                                                        kbps, rate, bo=r.getrandbits(8)), fill=rfill(r))


# ---- SeekBits and GetBits

def seek_edges():
    for byte in (0, 1, 7, 8, 100, 0x1FFFFFFF, 0x20000000, 0x40000000, 0x7FFFFFFF, -1, -8, -0x80000000, 0x10000000):
        yield mk([(OP_SEEKBITS, byte)])
    yield mk([(OP_SEEKBITS, 3), (OP_SEEKBITS, 9)], state=sane(bitpos=77))


def seek_random(r):
    return mk([(OP_SEEKBITS, r.choice([r.randrange(0, 5000), r.getrandbits(32), r.randrange(-100, 0)]) )], state=rand_state(r, ["bitpos"]), fill=rfill(r))


def getbits_case(r, ops, bitpos, length=64):
    return mk(ops, state=sane(bitpos=bitpos), stream=rbytes(r, length), fill=rfill(r))


def getbits_edges():
    r = random.Random(21)
    for offset in range(8):
        for n in list(range(0, 33)) + [33, 40, 63, 64, 65, 100, -1, -5, -32]:
            yield getbits_case(r, [(OP_GETBITS, n)], 8 * (offset // 8 + 3) + offset)
    yield mk([(OP_GETBITS, 8)], state=sane(bitpos=0), stream=b"\xff\xff\xff\xff\xff\xff\xff\xff")
    yield mk([(OP_GETBITS, 32)], state=sane(bitpos=0), stream=bytes([0x80, 0, 0, 1, 2, 3, 4, 5]))
    yield mk([(OP_GETBITS, 32), (OP_GETBITS, 32), (OP_GETBITS, 16)], state=sane(bitpos=0), stream=bytes(range(1, 20)))
    yield mk([(OP_GETBITS, 4)], state=sane(bitpos=-8), stream=bytes(32))
    yield mk([(OP_GETBITS, 4)], state=sane(bitpos=-16), stream=bytes(32))


def getbits_random(r):
    ops = [(OP_GETBITS, r.choice([r.randrange(0, 33), r.randrange(1, 17), r.randrange(-40, 80)])) for _ in range(r.randrange(1, 13))]
    return getbits_case(r, ops, r.randrange(0, 8 * 24), 64)


# ---- ProcessHeader

def header_levels():
    return [10, 0, 1, 5, 0xFFFF, 0x8000, 0x12345, -5, 0x7FFFFFFF, -0x80000000, 0x10000, 0x1FFFF]


def process_edges():
    n = 0
    chunk = []
    for id_ in (0, 1):
        for layer in (1, 2, 3, 4):
            for bri in range(16):
                for sri in range(4):
                    for pad in (0, 1):
                        chunk.append((OP_PROCESS_HEADER, hdr(id_=id_, layer=layer, bri=bri, sri=sri, pad=pad, mode=n % 4,
                                                             ext=(n >> 2) % 4, copy=n & 1, orig=(n >> 1) & 1, crc=(n >> 2) & 1,
                                                             priv=(n >> 3) & 1, emph=(n >> 4) & 3)))
                        n += 1
                        if len(chunk) == 32:
                            yield mk(chunk, state=sane(layer=3, lsf=0, bitrate=77))
                            chunk = []
    if chunk:
        yield mk(chunk)
    # the modes, one by one
    for mode in range(4):
        yield mk([(OP_PROCESS_HEADER, hdr(mode=mode)), (OP_PROCESS_HEADER, hdr(mode=mode, id_=1, layer=2, bri=9))])
    # sync failures: each of the twelve sync bits cleared, and others
    wrong = [hdr() & ~(1 << b) & MASK for b in range(20, 32)] + [0, MASK & ~0xFFF00000, 0xFFE00000, 0xFFF00000 & ~1, 0xFFFFFFFF]
    yield mk([(OP_PROCESS_HEADER, h) for h in wrong])
    yield mk([(OP_PROCESS_HEADER, hdr()), (OP_PROCESS_HEADER, 0x12345678), (OP_PROCESS_HEADER, hdr(id_=1, layer=2, bri=3))], state=sane())
    # the bit rate index 0 (free format) fails after the fields are stored
    yield mk([(OP_PROCESS_HEADER, hdr(bri=0, sri=2, mode=1, pad=1)), (OP_PROCESS_HEADER, hdr(bri=0, id_=1))])
    # the reserved layer with and without lsf reads the words above the table: its second halfword is the pool's level
    for level in header_levels():
        for id_ in (0, 1):
            yield mk([(OP_PROCESS_HEADER, hdr(id_=id_, layer=4, bri=b, sri=1)) for b in range(1, 16)], pool=dict(level=level))
    for level in header_levels():
        yield mk([(OP_SETLEVEL, 77), (OP_PROCESS_HEADER, hdr(id_=0, layer=4, bri=1)), (OP_SETLEVEL, level),
                  (OP_PROCESS_HEADER, hdr(id_=0, layer=4, bri=1)), (OP_PROCESS_HEADER, hdr(id_=0, layer=4, bri=1, pad=1, sri=3))])


def process_random(r):
    ops = []
    for _ in range(r.randrange(1, 9)):
        if r.random() < 0.15:
            h = r.getrandbits(32)
        elif r.random() < 0.1:
            h = hdr(sync=r.getrandbits(12), id_=r.getrandbits(1), layer=r.randrange(1, 5))
        else:
            h = hdr(id_=r.getrandbits(1), layer=r.randrange(1, 5), crc=r.getrandbits(1), bri=r.randrange(16), sri=r.randrange(4),
                    pad=r.getrandbits(1), priv=r.getrandbits(1), mode=r.randrange(4), ext=r.randrange(4), copy=r.getrandbits(1),
                    orig=r.getrandbits(1), emph=r.randrange(4))
        ops.append((OP_PROCESS_HEADER, h))
    return mk(ops, state=sane(layer=r.randrange(0, 5)) if r.random() < 0.5 else rand_state(r), fill=rfill(r),
              pool=dict(level=r.choice([10, 10, 10, r.getrandbits(32)])))


# ---- DecodeHeader, GetHeader (both forms)

def stream_of(r, n, **kw):
    heads = [hdr(**kw)] * n
    return build_stream(r, heads)


def decode_header_edges():
    r = random.Random(31)
    for kw in (dict(layer=1, id_=0, bri=4), dict(layer=2, id_=0, bri=11, mode=0), dict(layer=1, id_=1, bri=2), dict(layer=3, id_=0, bri=4),
               dict(layer=1, id_=0, bri=4, pad=1), dict(layer=2, id_=1, bri=9, sri=2, pad=1)):
        stream = stream_of(r, 4, **kw)
        yield mk([(OP_DECODE_HEADER,)] * 5, state=sane(), stream=stream)
        yield mk([(OP_DECODE_HEADER,)] * 2, state=sane(fstart=8 * 3 + 5), stream=stream)
        yield mk([(OP_DECODE_HEADER,)] * 3, state=sane(fstart=8 * 4), stream=b"\0\0\0\0" + stream)
    # no frame there
    yield mk([(OP_DECODE_HEADER,), (OP_DECODE_HEADER,)], state=sane(), stream=bytes(16))
    yield mk([(OP_DECODE_HEADER,)], state=sane(fstart=8 * 1000), stream=bytes(1200))
    yield mk([(OP_DECODE_HEADER,)], state=sane(fstart=8 * 4, fsize=0x10000000), stream=stream_of(r, 1, layer=1) + stream_of(r, 1, layer=2, bri=11))


def decode_header_random(r):
    heads = [hdr(id_=r.getrandbits(1), layer=r.randrange(1, 5), bri=r.randrange(16), sri=r.randrange(4), pad=r.getrandbits(1),
                 mode=r.randrange(4)) if r.random() < 0.85 else r.getrandbits(32) for _ in range(r.randrange(1, 6))]
    stream = build_stream(r, heads)
    return mk([(OP_DECODE_HEADER,)] * r.randrange(1, 7), state=sane(fstart=r.choice([0, 0, 0, 8 * r.randrange(0, 20), r.randrange(0, 200)]),
                                                                      fsize=r.choice([0, 10, 100, -4])), stream=stream + rbytes(r, 16), fill=rfill(r))


def get_header_edges():
    r = random.Random(41)
    for h in (hdr(), hdr(id_=1, layer=2, bri=3), hdr(bri=0), hdr(layer=4), hdr(sync=0xFFE), 0, MASK, 0x12345678):
        data = struct.pack(">I", h) + rbytes(r, 12)
        yield mk([(OP_GET_HEADER,)], state=sane(), stream=data)
        yield mk([(OP_GET_HEADER_H, h)], state=sane(), stream=data)
    yield mk([(OP_GET_HEADER_H, hdr()), (OP_GET_HEADER_H, 0), (OP_GET_HEADER_H, hdr(id_=1, bri=15, sri=1))])


def get_header_random(r):
    h = hdr(id_=r.getrandbits(1), layer=r.randrange(1, 5), bri=r.randrange(16), sri=r.randrange(4), pad=r.getrandbits(1), mode=r.randrange(4)) if r.random() < 0.8 else r.getrandbits(32)
    if r.random() < 0.3:
        h &= ~(1 << r.randrange(20, 32)) & MASK
    return mk([(OP_GET_HEADER,), (OP_GET_HEADER_H, h)], state=rand_state(r) if r.random() < 0.3 else sane(), stream=struct.pack(">I", h) + rbytes(r, 12), fill=rfill(r))


# ---- Open (both forms), Close, Reset

def open_edges():
    r = random.Random(51)
    sizes = [0, 1, 1000, 123456, 0x7FFFFFFF, -5, -0x80000000, 22050]
    kinds = [dict(layer=1, id_=0, bri=4), dict(layer=2, id_=0, bri=11, mode=0), dict(layer=3, id_=0, bri=5), dict(layer=1, id_=1, bri=3, sri=1),
             dict(layer=2, id_=1, bri=14, sri=2, pad=1), dict(layer=4, id_=0, bri=3), dict(layer=1, id_=0, bri=0), dict(sync=0x7FF)]
    for kw in kinds:
        stream = stream_of(r, 3, **kw)
        for size in sizes[:4]:
            yield mk([(OP_OPEN_PTR, 0, size)], state=sane(open=r.choice([0, 1])), stream=stream)
        yield mk([(OP_OPEN_HDR, hdr(**kw), 5000)], state=sane(open=1))
        yield mk([(OP_OPEN_HDR, hdr(**kw), 0)], state=sane(open=0))
    for size in sizes[4:]:
        yield mk([(OP_OPEN_PTR, 0, size)], stream=stream_of(r, 2, layer=1))
        yield mk([(OP_OPEN_HDR, hdr(), size)])
    yield mk([(OP_OPEN_PTR, 4, 1000)], stream=bytes(4) + stream_of(r, 2, layer=1))
    # the fake-derived class: its GetHeader answers the queue "GH", SeekBits is recorded
    for answer in ([0], [1], [2], [-1], [0x100], [MASK]):
        yield mk([(OP_OPEN_PTR, -1, 5000)], mode=1, q=dict(GH=answer), state=sane(layer=2, channels=2, bitrate=128, rate=24000))
        yield mk([(OP_OPEN_PTR, 0, 5000), (OP_OPEN_PTR, 0, 100)], mode=1, q=dict(GH=answer + [0]))
    yield mk([(OP_OPEN_PTR, -1, 7)], mode=1, q=dict(GH=[0]), state=sane(open=0))
    # a second Open while open, and an Open that fails leaves the object open
    yield mk([(OP_OPEN_HDR, hdr(), 100), (OP_OPEN_HDR, 0, 100), (OP_CLOSE,)])
    yield mk([(OP_OPEN_PTR, 0, 100), (OP_OPEN_PTR, 0, 200)], stream=stream_of(r, 2, layer=2, id_=0, bri=11))


def open_random(r):
    kw = dict(layer=r.randrange(1, 5), id_=r.getrandbits(1), bri=r.randrange(16), sri=r.randrange(4), pad=r.getrandbits(1), mode=r.randrange(4))
    size = r.choice([r.randrange(0, 100000), r.getrandbits(32), 0, 1])
    pool = dict(level=r.choice([10, 10, r.getrandbits(32)]))
    if r.random() < 0.3:
        answers = [r.choice([0, 0, 1, 2, -1]) for _ in range(3)]
        return mk([(OP_OPEN_PTR, r.choice([-1, 0]), size)] + ([(OP_OPEN_PTR, 0, 7)] if r.random() < 0.5 else []), mode=1, q=dict(GH=answers),
                  state=sane(layer=r.randrange(0, 5), channels=r.choice([1, 2]), bitrate=r.randrange(1, 460), rate=r.choice(RATES)), fill=rfill(r))
    stream = stream_of(r, 2, **kw)
    if r.random() < 0.5:
        return mk([(OP_OPEN_PTR, 0, size)], state=sane(open=r.choice([0, 1, 2])) if r.random() < 0.6 else rand_state(r), stream=stream, fill=rfill(r), pool=pool)
    return mk([(OP_OPEN_HDR, hdr(**kw) if r.random() < 0.85 else r.getrandbits(32), size)], state=sane(open=r.choice([0, 1])) if r.random() < 0.6 else rand_state(r),
              fill=rfill(r), pool=pool)


def close_edges():
    for open_ in (0, 1, 2, 0xFF, 0x80):
        yield mk([(OP_CLOSE,)], state=sane(open=open_))
        yield mk([(OP_CLOSE,), (OP_CLOSE,)], state=sane(open=open_), fill=0xFF)


def close_random(r):
    return mk([(OP_CLOSE,)] * r.randrange(1, 3), state=rand_state(r), fill=rfill(r))


def reset_edges():
    for mode in (0, 1):
        for fb2 in (0, 0x300, 0x600, 0x900, 0x1200, -5, 0x7FFFFFFF):
            yield mk([(OP_RESET,)], mode=mode, state=sane(fbytes2=fb2, bitpos=99, fstart=77, fdecoded=5, pos=9, fconsumed=1))
    yield mk([(OP_RESET,), (OP_RESET,)], mode=1)


def reset_random(r):
    return mk([(OP_RESET,)] * r.randrange(1, 3), mode=r.getrandbits(1), state=rand_state(r), fill=rfill(r))


# ---- Decode

def decode_stream_cases(r, kw, outs=(0, 0x40, 0x1000), n=3):
    stream = stream_of(r, n, **kw)
    for out in outs:
        yield mk([(OP_DECODE, out)] * (n + 1), state=sane(layer=r.choice([0, 1, 2, 3]), channels=r.choice([1, 2])), stream=stream, q=dict(L2=[0, 0, -1]))


def decode_edges():
    r = random.Random(61)
    for kw in (dict(layer=1, id_=0, bri=4, mode=3), dict(layer=1, id_=0, bri=8, mode=0), dict(layer=1, id_=1, bri=2, mode=1, sri=1),
               dict(layer=2, id_=0, bri=11, mode=0), dict(layer=2, id_=1, bri=9, mode=3, pad=1), dict(layer=3, id_=0, bri=5),
               dict(layer=4, id_=0, bri=5), dict(layer=1, id_=0, bri=0)):
        for c in decode_stream_cases(r, kw):
            yield c
    yield mk([(OP_DECODE, -1)] * 3, stream=stream_of(r, 3, layer=1, id_=0, bri=4), state=sane())
    yield mk([(OP_DECODE, 0)] * 3, stream=stream_of(r, 3, layer=2, id_=0, bri=11, mode=0), state=sane(), q=dict(L2=[7, 0, -3]))
    # the fake-derived class: DecodeHeader answers the queue "DH"; the layer is whatever the object holds
    for layer in (0, 1, 2, 3, 4, 255):
        for answer in ([0], [1], [-1], [2], [0x100], [MASK]):
            yield mk([(OP_DECODE, 0), (OP_DECODE, 0x100)], mode=1, q=dict(DH=answer, L2=[3]), stream=l1bytes(r, 4000),
                     state=sane(layer=layer, channels=r.choice([1, 2])))


def decode_random(r):
    if r.random() < 0.5:
        kw = dict(layer=r.randrange(1, 5), id_=r.getrandbits(1), bri=r.randrange(1, 16), sri=r.randrange(4), pad=r.getrandbits(1), mode=r.randrange(4))
        stream = stream_of(r, 3, **kw)
        return mk([(OP_DECODE, r.choice([0, 0x80, 0x1000, -1]))] * r.randrange(1, 5), state=sane(layer=r.randrange(0, 5)), stream=stream,
                  q=dict(L2=[r.choice([0, 0, 1, -1])]), fill=rfill(r))
    return mk([(OP_DECODE, r.choice([0, 0x200, -1]))] * r.randrange(1, 4), mode=1, q=dict(DH=[r.choice([0, 0, 0, 1, -1])], L2=[r.choice([0, 5])]),
              stream=l1bytes(r, 6000), state=sane(layer=r.randrange(0, 5), channels=r.choice([1, 2]), bitpos=r.choice([0, 0, 13, 8 * 100])), fill=rfill(r))


# ---- Read

def read_script(r, total, reads, out=0):
    ops, offset = [(OP_OPEN_PTR, 0, total)], out
    for n in reads:
        ops.append((OP_READ, offset, n))
    return ops


def read_edges():
    r = random.Random(71)
    l1 = dict(layer=1, id_=0, bri=4, mode=3)
    l1s = dict(layer=1, id_=0, bri=8, mode=0)
    l2 = dict(layer=2, id_=0, bri=11, mode=0)
    l2m = dict(layer=2, id_=1, bri=9, mode=3)
    for kw in (l1, l1s, l2, l2m):
        stream = stream_of(r, 6, **kw)
        for reads in ([0x100], [0x300], [0x300, 0x300, 0x300], [0x900], [0x1200], [2], [2, 2, 2], [0x2FE, 0x4], [0x500, 0x500, 0x500],
                      [0], [-5], [0x7FE, 0x802], [0x2000, 0x2000], [0x300, 0, 0x300], [0x1800]):
            yield mk(read_script(r, len(stream), reads), state=sane(open=0), stream=stream, q=dict(L2=[0]))
    # a stream that ends (the next header is no frame): Read answers what it got
    for n in (1, 2, 3):
        stream = stream_of(r, n, layer=1, id_=0, bri=4, mode=3) + bytes(8)
        yield mk(read_script(r, len(stream), [0x2000, 0x300]), state=sane(open=0), stream=stream)
    stream = stream_of(r, 2, layer=2, id_=0, bri=11, mode=0) + bytes(8)
    yield mk(read_script(r, len(stream), [0x3000, 100]), state=sane(open=0), stream=stream, q=dict(L2=[0, 0, 5]))
    yield mk(read_script(r, 100, [0x3000, 100]), state=sane(open=0), stream=stream_of(r, 4, layer=2, id_=0, bri=11, mode=0), q=dict(L2=[0, -1]))
    # output offsets
    stream = stream_of(r, 4, layer=1, id_=0, bri=4, mode=3)
    for out in (0, 2, 0x40, 0x102, 0x1000):
        yield mk(read_script(r, len(stream), [0x500, 0x100], out), state=sane(open=0), stream=stream)
    # real frames from the game's banks
    for tag, streams in sdt().items():
        for data in streams[:2]:
            for reads in ([0x300] * 4, [0x500] * 5, [0x1200, 0x1200], [8] * 10 + [0x900]):
                yield mk(read_script(r, len(data), reads), state=sane(open=0), stream=data, q=dict(L2=[0]))
    # the fake-derived class: the preset state; the header virtual answers 0 and the layer decodes from the bit position
    for fb2 in (0x300, 0x600, 0x900, 0x1200):
        yield mk([(OP_READ, 0, 0x700), (OP_READ, 0, 0x100), (OP_READ, 0x100, 0x1800)], mode=1, stream=l1bytes(r, 8000),
                 state=sane(layer=1, channels=1 if fb2 < 0x600 else 2, fbytes=fb2, fbytes2=fb2, fconsumed=fb2), q=dict(L2=[0]))
    yield mk([(OP_READ, 0, 0x400)], mode=1, stream=l1bytes(r, 8000), q=dict(DH=[0, -1]), state=sane())
    yield mk([(OP_READ, 0, 0x100)], mode=1, stream=l1bytes(r, 8000), q=dict(DH=[-1]), state=sane(fconsumed=0))
    # object state variants: a part-consumed frame, consumed past the end
    for fconsumed in (0, 2, 0x2FE, 0x300, 0x302, 0x5000, -4):
        yield mk([(OP_READ, 0, 0x250), (OP_READ, 0, 0x250)], mode=1, stream=l1bytes(r, 8000), frame=rbytes(r, 0x1240),
                 state=sane(fconsumed=fconsumed, fdecoded=2, fbytes=0x300, fbytes2=0x300))


def read_random(r):
    kinds = [dict(layer=1, id_=0, bri=r.choice([2, 4, 8, 11]), mode=r.choice([3, 0, 1])), dict(layer=2, id_=0, bri=r.choice([5, 11]), mode=r.choice([3, 0])),
             dict(layer=1, id_=1, bri=r.choice([2, 4]), sri=r.randrange(3), mode=3), dict(layer=3, id_=0, bri=5)]
    if r.random() < 0.7:
        kw = r.choice(kinds)
        stream = stream_of(r, r.randrange(1, 7), **kw) + (bytes(8) if r.random() < 0.5 else b"")
        reads = [r.choice([0, 2, r.randrange(1, 0x400), 0x300, 0x600, 0x900, 0x1200, r.randrange(1, 0x2000)]) & ~1 for _ in range(r.randrange(1, 6))]
        return mk(read_script(r, len(stream), reads, r.choice([0, 0, 2, 0x40])), state=sane(open=0), stream=stream, q=dict(L2=[r.choice([0, 0, -1])]), fill=rfill(r))
    channels = r.choice([1, 2])
    fb = channels * 0x300
    reads = [r.choice([2, r.randrange(1, 0x500), fb, 2 * fb, 0x3000 - 100]) & ~1 for _ in range(r.randrange(1, 5))]
    return mk([(OP_READ, 0, n) for n in reads], mode=1, stream=l1bytes(r, 9000), frame=rbytes(r, 0x1240), q=dict(DH=[r.choice([0, 0, 0, -1])]),
              state=sane(layer=1, channels=channels, fbytes=fb, fbytes2=fb, fconsumed=r.choice([fb, fb, 0, r.randrange(0, fb) & ~1]), fdecoded=r.randrange(0, 4),
                         bitpos=r.choice([0, 7, 8 * 33])), fill=rfill(r))


# ---- Decode_Layer1

def layer1_edges():
    r = random.Random(81)
    for channels in (1, 2):
        for data in (bytes(4000), bytes([0xFF]) * 4000, bytes([0x55]) * 4000, bytes([0xAA]) * 4000, bytes([0x10]) * 4000, bytes([0x01]) * 4000,
                     bytes([0xF0, 0x0F]) * 2000, l1bytes(r, 4000), rbytes(r, 4000)):
            yield mk([(OP_LAYER1, 0)], state=sane(channels=channels), stream=data)
            yield mk([(OP_LAYER1, 0x100), (OP_LAYER1, -1)], state=sane(channels=channels, bitpos=3), stream=data)
    for channels, c32 in ((1, 32), (2, 64), (0, 0), (3, 96), (-1, -32), (2, 32), (1, 100), (1, 0), (2, 1)):
        yield mk([(OP_LAYER1, 0x40)], state=sane(channels=channels, channels32=c32), stream=l1bytes(r, 4000))
    for bitpos in (0, 1, 7, 8, 9, 31, 32, 33, 1000, 12345):
        for channels in (1, 2):
            yield mk([(OP_LAYER1, 0)], state=sane(channels=channels, bitpos=bitpos), stream=l1bytes(r, 8000))
    yield mk([(OP_LAYER1, 0), (OP_LAYER1, 0)], state=sane(channels=2), stream=l1bytes(r, 8000))
    for tag, streams in sdt().items():
        if tag.startswith("l1"):
            for data in streams[:2]:
                yield mk([(OP_LAYER1, 0)] * 3, state=sane(bitpos=32), stream=data)
                yield mk([(OP_LAYER1, 0)] * 2, state=sane(channels=2, bitpos=32), stream=data)


def layer1_random(r):
    channels = r.choice([1, 2, 1, 2, 0, 3])
    data = l1bytes(r, 6000) if r.random() < 0.7 else rbytes(r, 6000)
    return mk([(OP_LAYER1, r.choice([0, 0, 0x80, -1]))] * r.randrange(1, 3), state=sane(channels=channels, channels32=(channels << 5) if r.random() < 0.9 else r.randrange(0, 200),
                                                                                       bitpos=r.choice([0, 0, r.randrange(0, 400)])), stream=data, fill=rfill(r))


# ---- get_sb_stereo

def sb_case(r, bands, zero=0.3, small=False):
    def alloc():
        return r.choice([0, 0, 1, 2, 3, 5, 7, 11, 15, r.randrange(0, 17)]) if r.random() > zero else 0
    sb = dict(alloc=[alloc() for _ in range(64)], scale=[r.randrange(0, 64) for _ in range(64)],
              mul=[(r.randrange(-3000, 3000) if small else r.choice([r.getrandbits(32), r.randrange(-100000, 100000), 0, 4096, -4096])) & MASK for _ in range(2048)])
    sb["mul"] = [T.s32(v) for v in sb["mul"]]
    ops = [(OP_SB_STEREO, band, 2 * band + 0x100 * (i % 2)) for i, band in enumerate(bands)]
    return mk(ops, state=sane(bitpos=r.choice([0, 3, 100])), stream=rbytes(r, 600), sb=sb, fill=rfill(r))


def sb_edges():
    r = random.Random(91)
    for band in range(32):
        yield sb_case(r, [band])
    yield sb_case(r, list(range(32)), zero=0.1)
    yield sb_case(r, list(range(31, -1, -1)), small=True)
    sb = dict(alloc=[1] * 64, scale=[0] * 64, mul=[4096] * 2048)
    yield mk([(OP_SB_STEREO, 0, 0), (OP_SB_STEREO, 5, 0x40)], stream=bytes([0xFF]) * 50, sb=sb)
    sb = dict(alloc=[0] * 64, scale=[5] * 64, mul=[999] * 2048)
    yield mk([(OP_SB_STEREO, 3, 0x20)], stream=bytes([0xFF]) * 50, sb=sb)
    # one channel allocated, the other not; negative products; products that overflow 32 bits
    for a0, a1 in ((0, 5), (5, 0), (15, 15), (1, 16), (16, 1), (30, 30)):
        al = [0] * 64
        al[2 * 7], al[2 * 7 + 1] = a0, a1
        for mul in (-1, 1, 0x7FFFFFFF, -0x80000000, 0x10000, 123456789, -2):
            yield mk([(OP_SB_STEREO, 7, 0x80)], stream=rbytes(r, 40), sb=dict(alloc=al, scale=[r.randrange(64) for _ in range(64)], mul=[mul] * 2048))


def sb_random(r):
    return sb_case(r, [r.randrange(32) for _ in range(r.randrange(1, 6))], zero=r.choice([0.1, 0.3, 0.6]), small=r.random() < 0.3)


# ---- muldiv

def muldiv_edges():
    for a, b, c in ((5, 3, 2), (-5, 3, 2), (5, -3, 2), (5, 3, -2), (0, 3, 2), (5, 0, 2), (5, 3, 1), (5, 3, 0), (-5, 3, 0), (0, 0, 0),
                    (100000, 100000, 7), (0x7FFFFFFF, 2, 3), (0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, 1, -1), (-0x80000000, -1, 1),
                    (0x10000, 0x10000, 5), (0x10000, 0x8000, 1), (-0x80000000, 1, 1), (1, -0x80000000, -1), (7, 7, 7), (22050, 220, 1000)):
        yield mk([(OP_MULDIV, a, b, c)])
    yield mk([(OP_MULDIV, 5, 3, 2), (OP_MULDIV, 1, 1, 1)])


def muldiv_random(r):
    pick = lambda: r.choice([r.getrandbits(32), r.randrange(-1000, 1000), r.randrange(-100000, 100000), 0, 1, -1])
    return mk([(OP_MULDIV, T.s32(pick() & MASK), T.s32(pick() & MASK), T.s32(pick() & MASK)) for _ in range(r.randrange(1, 4))], fill=rfill(r))


# ---- the static tables the unit's data holds: muls and the integer allocation words

def tables_ppc(e, case):
    kind, row = case["table"]
    if kind == "muls":
        return dict(words=list(struct.unpack(">64I", e.read(D_MULS + 256 * row, 256))))
    return dict(words=[s32(v) for v in struct.unpack(">32i", e.read(0x10039440, 128))])


def tables_host(lib, case):
    bind(lib)
    kind, row = case["table"]
    if kind == "muls":
        return dict(words=[lib.tpw_mp_muls_bits(row, i) for i in range(64)])
    return dict(words=[lib.tpw_mp_int_alloc(i) for i in range(32)])


def tables_edges():
    for row in range(27):
        yield dict(table=("muls", row))
    yield dict(table=("alloc", 0))


def tables_random(r):
    return dict(table=("muls", r.randrange(27)))


TESTS = [
    FnTest("CMpegBase::CMpegBase() 0x10002604", run_ppc, host_case, edge=ctor_edges, rnd=ctor_random),
    FnTest("CMpegBase::~CMpegBase() 0x10002640", run_ppc, host_case, edge=dtor_edges, rnd=dtor_random),
    FnTest("_CS_CMpegBase::~_CS_CMpegBase() 0x10004110", run_ppc, host_case, edge=cs_edges, rnd=cs_random),
    FnTest("CMpegBase_DecodeMethod 0x1000422c and 0x10004190", run_ppc, host_case, edge=dm_edges, rnd=dm_random),
    FnTest("CMpegBase::operator new 0x10002398, operator delete 0x1000245c, TopUp 0x100024f4, SetLevel 0x100025cc, ClearCache 0x10002310",
           run_ppc, host_case, edge=pool_edges, rnd=pool_random),
    FnTest("CMpegBase::OpenSynth() 0x100026d8", run_ppc, host_case, edge=open_synth_edges, rnd=open_synth_random),
    FnTest("CMpegBase::CloseSynth() 0x100027f0", run_ppc, host_case, edge=leaf_edges(OP_CLOSE_SYNTH), rnd=leaf_random(OP_CLOSE_SYNTH)),
    FnTest("CMpegBase::CloseLayer() 0x10002c90", run_ppc, host_case, edge=leaf_edges(OP_CLOSE_LAYER), rnd=leaf_random(OP_CLOSE_LAYER)),
    FnTest("CMpegBase::Init_Tables() 0x10005a58", run_ppc, host_case, edge=leaf_edges(OP_INIT_TABLES), rnd=leaf_random(OP_INIT_TABLES)),
    FnTest("CMpegBase::Close_Tables() 0x10005aa8", run_ppc, host_case, edge=leaf_edges(OP_CLOSE_TABLES), rnd=leaf_random(OP_CLOSE_TABLES)),
    FnTest("CMpegBase::OpenLayer() 0x10002bb4", run_ppc, host_case, edge=open_layer_edges, rnd=open_layer_random),
    FnTest("CMpegBase::SeekBits(int) 0x10002754", run_ppc, host_case, edge=seek_edges, rnd=seek_random),
    FnTest("CMpegBase::GetBits(int) 0x1000278c", run_ppc, host_case, edge=getbits_edges, rnd=getbits_random),
    FnTest("CMpegBase::ProcessHeader(unsigned) 0x10002820", run_ppc, host_case, edge=process_edges, rnd=process_random),
    FnTest("CMpegBase::DecodeHeader() 0x10002a44", run_ppc, host_case, edge=decode_header_edges, rnd=decode_header_random),
    FnTest("CMpegBase::GetHeader() 0x10002aec and GetHeader(unsigned) 0x10002b54", run_ppc, host_case, edge=get_header_edges, rnd=get_header_random),
    FnTest("CMpegBase::Open(void*, int) 0x10002d9c and Open(unsigned, int) 0x10002ea4", run_ppc, host_case, edge=open_edges, rnd=open_random),
    FnTest("CMpegBase::Close() 0x1000301c", run_ppc, host_case, edge=close_edges, rnd=close_random),
    FnTest("CMpegBase::Reset() 0x10002fa8", run_ppc, host_case, edge=reset_edges, rnd=reset_random),
    FnTest("CMpegBase::Decode(short*) 0x10002cc4", run_ppc, host_case, edge=decode_edges, rnd=decode_random),
    FnTest("CMpegBase::Read(void*, int) 0x1000309c", run_ppc, host_case, edge=read_edges, rnd=read_random),
    FnTest("CMpegBase::Decode_Layer1(short*) 0x100043b0", run_ppc, host_case, edge=layer1_edges, rnd=layer1_random),
    FnTest("CMpegBase::get_sb_stereo 0x10004270", run_ppc, host_case, edge=sb_edges, rnd=sb_random),
    FnTest("muldiv(int, int, int) 0x100022e4", run_ppc, host_case, edge=muldiv_edges, rnd=muldiv_random),
    FnTest("the multiplier table muls (0x10035df0) and the integer allocation words (0x10039440)", tables_ppc, tables_host, edge=tables_edges, rnd=tables_random),
]
