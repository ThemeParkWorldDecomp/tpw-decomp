"""w87: fast mono/color decoders, eight-column inverse and BGR conversion.

Shared wavelet helpers initialize and compare all five full planes, coefficients,
LZSS state and the unpacked workspace. The real bullfrog unpacker runs in a second
emulator with its original zlib via zlib_bridge; the host uses src/bullfrog/LbZLib.cpp.
Only platform allocator calls are functional recording fakes. Its opaque state and
zlib's private arena bytes are not compared. Fused instructions use fmaf hooks.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
import test_WaveletDecodeV12 as W
import test_ParticlesInit as Z
import zlib_bridge as ZB
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_WaveletFast"
FN = dict(color=0x10039894, mono=0x1003a67c, reverse=0x1003ab50, convert=0x1003af4c)
REAL = set(FN.values()) | {0x1003ee44, 0x1003ee88, 0x1003ef48, 0x1003d1f8}
PLANE_BYTES = 4 * W.PLANE_FLOATS
DUMPS = W.ROOT / "ghidra/dumps"
BULLFROG = None


class Case(dict):
    """Keep failure output legible even for large coefficient streams."""

    def __repr__(self):
        return f"<{self['entry']} {self['width']}x{self['height']} stride={self['stride']} {self.get('label', '')}>"


def setup(runner):
    global BULLFROG
    W.FUSED_RANGES = [(0x10039894, 0x1003a638), (0x1003a67c, 0x1003ab0c),
                      (0x1003ab50, 0x1003af0c), (0x1003af4c, 0x1003b098)]
    W.setup(runner)
    hs = W.host(runner.lib)
    hs.buffer = C.create_string_buffer(0x62a39)
    runner.lib.tpw_wd12_set_buffer(C.addressof(hs.buffer))
    BULLFROG = ppcemu.PPCEmu(DUMPS / "bullfrog symbols shared")
    T.install_std_stubs(BULLFROG)
    ZB.install(BULLFROG, DUMPS)


def packed_floats(values, endian):
    return struct.pack(endian + str(len(values)) + "f", *values)


def patches(case):
    return case.get("patches", [])


def engine_unpack(e):
    """Mirror the engine buffers at identical addresses into the real bullfrog unpacker."""
    b = BULLFROG
    source, dest, arena = e.r(3), e.r(4), e.r(5)
    size = e.scratch_top - ppcemu.SCRATCH_BASE
    b.reset_scratch()
    b.write(ppcemu.SCRATCH_BASE, e.read(ppcemu.SCRATCH_BASE, size))
    b.scratch_top = e.scratch_top
    b.heap_in_scratch = True
    answer = T.call_ppc(b, b.func(Z.LB_ZLIB), [source, dest, arena],
                        max_insns=200_000_000, timeout_us=120_000_000)[0]
    e.write(source, b.read(source, 0x1c))
    e.write(dest, b.read(dest, W.WORKSPACE_SIZE))
    e.set_r(3, answer)


def stubs():
    st = CL.Stubs()

    def allocator(e):
        e.cl.labels[e.r(3)] = 7
        e.cl.rec("UAllocator", [7])
        e.write(e.r(3), bytes(8))

    def clear(e):
        e.cl.rec("Clear", [e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.r(5)])
        e.write(e.r(4), bytes(e.r(5)))

    def new_array(e):
        size = e.r(3)
        block = e.alloc(max(size, 4))
        e.wf_arrays.append(block)
        e.cl.rec("new[]", [size, len(e.wf_arrays)])
        e.alloc_log.append(("new[]", block, size))
        e.set_r(3, block)

    def delete_array(e):
        block = e.r(3)
        ordinal = e.wf_arrays.index(block) + 1
        e.cl.rec("delete[]", [ordinal])
        e.alloc_log.append(("delete[]", block, 0))

    st.imp("__ct__Q213SamsUtilities10UAllocatorFv", allocator)
    st.imp("Clear__Q213SamsUtilities10UAllocatorFPvUl", clear)
    st.imp(Z.LB_ZLIB, engine_unpack)
    st.imp("__nwa__FUl", new_array)
    st.imp("__dla__FPv", delete_array)
    st.real = set(REAL)
    return st


def ppc(e, case):
    e.reset_scratch()
    e.heap_in_scratch = True
    e.cl = CL.PpcLog({})
    e.wf_arrays = []
    saved = stubs().activate(e)
    try:
        workspace = W.ppc_reset(e, case["state"])
        for plane, values in patches(case):
            e.write(W.PLANES + plane * PLANE_BYTES, packed_floats(values, ">"))
        arena = e.alloc(0x62a39, bytes(0x62a39))
        obj = W.ppc_object(e, arena)
        old = e.alloc(8, bytes(8)) if case["allocator"] else 0
        e.write(e.data_import("gMemory__13SamsUtilities"), struct.pack(">I", old))
        data = e.alloc(len(case["data"]) + 64, case["data"] + bytes(64))
        whole = e.alloc(case["dest_len"], b"\xa5" * case["dest_len"])
        dest = whole + 64
        e.cl.labels.update({obj: 1, data: 2, dest: 4, arena: 5, workspace: 6})
        if old:
            e.cl.labels[old] = 7
        for plane in range(5):
            e.cl.labels[W.PLANES + plane * PLANE_BYTES] = 10 + plane
        start = len(e.alloc_log)
        entry = case["entry"]
        if entry in ("mono", "color"):
            args = [obj, case["width"], case["height"], data, dest] + [T.F(v) for v in case["scales"]]
        elif entry == "reverse":
            args = [obj, W.PLANES + 4 * PLANE_BYTES, W.PLANES + 3 * PLANE_BYTES, case["stride"], case["width"]]
        else:
            args = [obj, case["width"], case["height"], case["stride"], dest]
        W.ppc_call(e, FN[entry], args)
        statics = W.ppc_statics(e, {workspace: "workspace"}, True)
        result = dict(log=e.cl.log, heap=W.heap_counts(e, start), pixels=e.read(whole, case["dest_len"]),
                      statics=statics, allocator=bool(e.u32(e.data_import("gMemory__13SamsUtilities"))))
        # Header words have been swapped on the Mac and retained in host order on the port.
        result["header"] = Z.header_words(e.read(data, 0x1c), True)
        return result
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    hs = W.host(lib)
    hs.reset(case["state"])
    for plane, values in patches(case):
        raw = packed_floats(values, "<")
        C.memmove(hs.addr["planes"] + plane * PLANE_BYTES, raw, len(raw))
    lib.tpw_wf_allocator.argtypes = [C.c_int32]
    lib.tpw_wf_allocator(case["allocator"])
    old = lib.tpw_wd12_global(b"allocator")
    data = C.create_string_buffer(case["data"] + bytes(64))
    whole = C.create_string_buffer(b"\xa5" * case["dest_len"], case["dest_len"])
    dest = C.addressof(whole) + 64
    labels = {1: hs.addr["object"], 2: C.addressof(data), 4: dest, 5: C.addressof(hs.buffer),
              6: C.addressof(hs.workspace), 7: old}
    for plane in range(5):
        labels[10 + plane] = hs.addr["planes"] + plane * PLANE_BYTES
    CL.host_begin(lib, labels)
    hs.heap()
    entry = case["entry"]
    fn = getattr(lib, "tpw_wf_" + entry)
    fn.restype = None
    if entry in ("mono", "color"):
        fn.argtypes = [C.c_uint32] * 2 + [C.c_void_p] * 2 + [C.c_float] * len(case["scales"])
        fn(case["width"], case["height"], C.addressof(data), dest, *case["scales"])
    elif entry == "reverse":
        fn.argtypes = [C.c_void_p] * 2 + [C.c_uint32] * 2
        fn(hs.addr["planes"] + 4 * PLANE_BYTES, hs.addr["planes"] + 3 * PLANE_BYTES,
           case["stride"], case["width"])
    else:
        fn.argtypes = [C.c_uint32] * 3 + [C.c_void_p]
        fn(case["width"], case["height"], case["stride"], dest)
    return dict(log=CL.host_end(lib), heap=hs.heap(), pixels=whole.raw,
                statics=hs.statics({C.addressof(hs.workspace): "workspace"}, True),
                allocator=bool(case["allocator"] or entry == "color"), header=Z.header_words(data.raw[:0x1c], False))


def base(entry, width, height, stride=None, r=None):
    return Case(entry=entry, width=width, height=height, stride=stride or max(width, 1), data=bytes(28),
                dest_len=128 + 3 * width * height, scales=[], state=W.default_state(r, r is not None), allocator=True)


def decoder_case(entry, r, width, height, scales=None, short=False, tagged=True):
    size = max(W.transform_size(width), W.transform_size(height))
    count = size * size * (3 if entry == "color" else 2) // 2
    if short:
        count = r.randrange(max(count, 1))
    payload = W.pack_coefficients(r, W.random_coefficients(r, count))
    case = base(entry, width, height, size, r)
    case["data"] = W.bilz(payload) if tagged else bytes(28)
    case["scales"] = scales or [r.choice([1.0, 0.5, 1 / 128, 1.401298464324817e-45, -2.0, 17.5])
                                for _ in range(3 if entry == "color" else 1)]
    case["allocator"] = r.choice([False, True])
    return case


def decoder_edges(entry):
    r = random.Random(87)
    for width, height in [(1, 1), (7, 1), (8, 7), (9, 8), (16, 9), (31, 16), (64, 31), (128, 64), (256, 1),
                          (1, 128), (0, 0), (0, 7), (7, 0)]:
        for scale in [1.0, 0.5, 1 / 128, 1.401298464324817e-45]:
            yield decoder_case(entry, r, width, height, [scale] * (3 if entry == "color" else 1))
    for scale in [0.0, -0.0, -1.0, 40000.0, float("nan"), float("inf"), -float("inf")]:
        yield decoder_case(entry, r, 8, 8, [scale] * (3 if entry == "color" else 1))
    for width in [1, 8, 16, 32]:
        yield decoder_case(entry, r, width, 7, short=True)
        yield decoder_case(entry, r, width, 7, tagged=False)
    # These widths shift by 32; a zero height keeps the pixel loops empty.
    for width in [0x80000001, 0xffffffff]:
        yield decoder_case(entry, r, width, 0)


def decoder_random(entry, r):
    width = r.choice([1, 7, 8, 9, 16, 31, 32, 64])
    height = r.choice([1, 7, 8, 9, 16, 31, 32])
    return decoder_case(entry, r, width, height, short=r.random() < 0.1, tagged=r.random() > 0.05)


def special_float(r):
    if r.random() < 0.1:
        return r.choice([0.0, -0.0, 1.401298464324817e-45, -1.401298464324817e-45,
                         float("nan"), float("inf"), -float("inf"), 40000.0, -40000.0, 1e10])
    return T.f32(r.uniform(-1000.0, 1000.0))


def reverse_case(r, count, stride):
    case = base("reverse", count, count, stride, r)
    case["patches"] = [(4, [special_float(r) for _ in range(count * stride + 16)])]
    return case


def reverse_edges():
    r = random.Random(88)
    for count in [4, 8, 16, 32, 64, 0, 1, 2, 3, 7, 9, 15, 17]:
        for extra in [0, 9]:
            yield reverse_case(r, count, max(8, (count + 7) & ~7) + extra)


def convert_case(r, width, height, stride):
    case = base("convert", width, height, stride, r)
    case["patches"] = [(plane, [special_float(r) for _ in range(height * stride)]) for plane in [1, 2, 3]]
    return case


def convert_edges():
    r = random.Random(89)
    values = [-40000.0, -32769.0, -32768.0, -1.0, -0.0, 0.0, 0.9, 1.0, 172.9, 173.0, 174.0,
              254.9, 255.0, 256.0, 32767.0, 32768.0, 40000.0, 65536.0, 1e10, -1e10,
              float("nan"), float("inf"), -float("inf"), 1.401298464324817e-45]
    for plane in [1, 2, 3]:
        case = base("convert", len(values), 1)
        case["patches"] = [(k, values if k == plane else [0.0] * len(values)) for k in [1, 2, 3]]
        yield case
    for width, height, stride in [(0, 0, 8), (0, 8, 8), (8, 0, 8), (1, 1, 8), (7, 9, 16), (32, 16, 40)]:
        yield convert_case(r, width, height, stride)
    # Operands found by search where the fused multiply-add and a rounded product plus a sum
    # truncate to different bytes: (colour difference, brightness) for the blue, red and green
    # terms, three pixels each; the other difference is zero so the other term passes through.
    blue = [(-36.7859116, 83.1846313), (-51.7040253, 234.619522), (15.8288498, 117.951271)]
    red = [(-44.596611, 64.5244446), (-51.4708862, 99.162178), (52.7354813, 94.0648499)]
    green_cb = [(54.6572647, 179.809525), (-44.6723366, 195.626633), (26.5835419, 190.148346)]
    green_cr = [(52.7399902, 44.6635246), (27.0587158, 76.3236008), (-52.1804657, 48.7360458)]
    pixels = ([(luma, cb, 0.0) for cb, luma in blue] + [(luma, 0.0, cr) for cr, luma in red] +
              [(luma, cb, 0.0) for cb, luma in green_cb] + [(luma, 0.0, cr) for cr, luma in green_cr])
    case = base("convert", len(pixels), 1)
    # The planes are numbered red difference, blue difference, brightness.
    case["patches"] = [(plane, [T.f32(pixel[3 - plane]) for pixel in pixels]) for plane in [1, 2, 3]]
    yield case


TESTS = [
    FnTest("CWaveletImp::DecompressColorFast", ppc, host, edge=lambda: decoder_edges("color"),
           rnd=lambda r: decoder_random("color", r)),
    FnTest("CWaveletImp::DecompressMonoFast", ppc, host, edge=lambda: decoder_edges("mono"),
           rnd=lambda r: decoder_random("mono", r)),
    FnTest("CWaveletImp::ReverseChannelFast", ppc, host, edge=reverse_edges,
           rnd=lambda r: reverse_case(r, r.choice([8, 16, 32, 64]), r.choice([64, 72, 80]))),
    FnTest("CWaveletImp::ConvertAndBlend", ppc, host, edge=convert_edges,
           rnd=lambda r: convert_case(r, r.randint(0, 32), r.randint(0, 16), 40)),
]
