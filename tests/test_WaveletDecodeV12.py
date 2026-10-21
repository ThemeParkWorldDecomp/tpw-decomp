"""The version 0x12 wavelet texture decoder (engine, src/engine/Wavelet.cpp):
CWaveletImp::Decompress_v12, called directly and through the version dispatch.

The decoder runs on REAL version 0x12 files read at run time from ROOT/extracted/wads
(selected by the version byte; never copied into the repository; a missing file is skipped
with a message) and on synthetic coefficient streams the test makes itself. Its one callee
outside the unit, the bullfrog library's LbZLib_MemDecompress (an import on the PowerPC side),
is a recording fake on both sides that writes the case's unpacked bytes into its destination:
Python's zlib inflates a real file's blocks after their 0x1c-byte "BILZ" header, and a block
without the tag (three 8 x 8 files store their coefficients raw) gets nothing written, as the
real unpacker does. ReverseTransformVert, CIgArray, the dispatch and GetAlphaMask run as the
original.

Before every case both sides get the same decoder statics (the five planes, the D4
coefficients, the LZSS words, a workspace pattern); afterwards all of them are compared with
the whole destination buffer (with a 0xa5 margin BEFORE it as well as after, since an odd
height writes one row before the destination), the heap traffic (blocks through new[],
delete[], delete) and the call log.

The original's fused multiply-adds run under HardwareRounding (tests/test_OpenGLDraw.py's
correction in tests/test_WaveletDecode.py's form: a hook on each fused instruction computes
the single-rounded result with the C library's fmaf, writes it and skips the instruction).
NaNs in the planes compare as one value in the few cases whose scales can make them.

TPW_WD12_SMALL=1 in the environment keeps the real files to 32 x 32 and below (for
branchcov.py, whose hook on every instruction is slow).
"""
import array
import ctypes as C
import math
import os
import pathlib
import struct
import zlib

from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_WaveletDecodeV12"

FN = dict(v12=0x1003b0d4, dispatch=0x10039718)
# The unit's callees that run as the original (another module may have left a stub on them).
REAL = {0x1003b0d4, 0x10039718, 0x1003d054, 0x1003d1f8, 0x1003ee44, 0x1003ee88, 0x1003ef48, 0x10029150}
UNPACKER = "LbZLib_MemDecompress__FPvPvPv"
VTABLE, BASE_VTABLE = 0x10051f94, 0x10051fe4
PLANES, PLANE_FLOATS = 0x10173614, 0x10000
COEFFICIENTS = dict(h0=0x10173610, h1=0x1017360c, h2=0x10173608, h3=0x10173604)
LZ = dict(count=0x101735ec, buffer=0x101735f0, unused=0x101735f4, pos=0x101735f8, dest=0x101735fc, source=0x10173600)
WORKSPACE_POINTER, WORKSPACE_SIZE = 0x102b3614, 0x60000
FUSED_RANGES = [(0x1003b0d4, 0x1003d014), (0x1003d054, 0x1003d1b8)]
MASK = 0xFFFFFFFF
CANONICAL_NAN = 0x7FC00000
REAL_H = None     # the host's D4 coefficients (set in setup)
# Labels in the call log.
OBJECT, DATA, ALPHA, DEST, BUFFER, WORKSPACE = 1, 2, 3, 4, 5, 6

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
WADS = ROOT / "extracted" / "wads"
# Real version 0x12 files: the interface's (ui.wad.d) and others, every size there is, with
# and without the alpha block, and the quality words' variety.
FILES = [
    "global/advisor.wad.d/stexture/Bowler_Btie.wct",                 # 8 x 8, coefficients stored raw
    "levels/space/rides/gokarts.wad.d/stexture/s_disc.wct",          # 8 x 8, alpha, both blocks raw
    "global/advisor.wad.d/stexture/Handy_Top.wct",                   # 8 x 8
    "ui.wad.d/stexture/progress.wct",                                # 16 x 16, quality 6 10 2 20
    "ui.wad.d/stexture/white.wct",                                   # 16 x 16, quality 40 50 50 20
    "ui.wad.d/stexture/boxtick.wct",                                 # 16 x 16, alpha
    "global/advisor.wad.d/stexture/Kiosk_Smiley.wct",                # 16 x 16, alpha
    "ui.wad.d/stexture/b_allstaff.wct",                              # 26 x 26, alpha
    "ui.wad.d/stexture/aerial.wct",                                  # 32 x 32
    "ui.wad.d/stexture/grid.wct",                                    # 32 x 32, alpha, quality 0 0 0 20
    "ui.wad.d/textures/gticket.wct",                                 # 32 x 32, alpha, quality 0 5 0 20
    "ui.wad.d/textures/h_shop.wct",                                  # 32 x 32, alpha, quality 16 44 33 20
    "ui.wad.d/textures/ridestatbar.wct",                             # 32 x 32
    "levels/space/rides/gokarts.wad.d/stexture/s_top.wct",           # 32 x 32, alpha, header byte 1 = 0
    "ui.wad.d/textures/nutag_a.wct",                                 # 32 x 32, quality 0 0 0 20
    "ui.wad.d/stexture/b_1person.wct",                               # 51 x 51, alpha
    "ui.wad.d/stexture/b_address.wct",                               # 51 x 51, alpha
    "ui.wad.d/stexture/wave.wct",                                    # 64 x 64, quality 0 12 10 20
    "ui.wad.d/textures/Cash_alpha.wct",                              # 64 x 64, alpha
    "ui.wad.d/textures/b_on1.wct",                                   # 64 x 64, alpha, quality 0 19 0 20
    "global/advisor.wad.d/textures/Eye2.wct",                        # 64 x 64
    "ui.wad.d/textures/b_doorg.wct",                                 # 64 x 64, alpha, quality 0 44 33 20
    "ui.wad.d/textures/purple_button.wct",                           # 128 x 128, alpha
    "ui.wad.d/stexture/f_chat1.wct",                                 # 128 x 128, quality 4 30 30 20
    "ui.wad.d/textures/effort.wct",                                  # 128 x 128, alpha, quality 0 44 18 20
    "levels/fantasy/rides/gokarts.wad.d/textures/hivesign01.wct",    # 128 x 128, alpha
]
SMALL = os.environ.get("TPW_WD12_SMALL") == "1"


def f32(x):
    return T.f32(x)


def fbits(x):
    return CL.fbits(x)


def bits_f(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


# ----------------------------------------------------------------- the files


def unpacked(block):
    """What LbZLib_MemDecompress writes for a block: the inflated stream after a "BILZ"
    header (at most the header's unpacked size), or None (nothing written) without the tag."""
    if block is None or block[:4] != b"BILZ":
        return None
    size = struct.unpack("<I", block[4:8])[0]
    return zlib.decompress(block[0x1c:])[:size]


class WaveFile:
    """A .WCT file as CWaveLoad loads it: the header, the blocks, the four scales."""

    def __init__(self, name, raw):
        self.name = name
        self.version, self.kind = raw[0], raw[2]
        self.width, self.height = struct.unpack("<HH", raw[4:8])
        q = struct.unpack("<4H", raw[8:16])
        data_size, alpha_size = struct.unpack("<II", raw[16:24])
        self.data = raw[0x1c:0x1c + data_size]
        self.alpha = raw[0x1c + data_size:0x1c + data_size + alpha_size] if self.kind == 0x20 else None
        # CWaveLoad::Decompress: fmadds(0.5, q0, 1), fmadds(0.25, q2, 1), fmadds(0.25, q1, 1), 1 + q3
        # (exact for these integers).
        self.scales = [f32(0.5 * q[0] + 1.0), f32(0.25 * q[2] + 1.0), f32(0.25 * q[1] + 1.0), f32(1.0 + q[3])]
        self.payloads = [unpacked(self.data)] + ([unpacked(self.alpha)] if self.alpha is not None else [])


_files, _missing = {}, []


def wave_file(name):
    """The file, or None (once reported) when ROOT/extracted does not have it."""
    if name not in _files:
        path = WADS / name
        if path.is_file():
            _files[name] = WaveFile(name, path.read_bytes())
        else:
            _files[name] = None
            _missing.append(name)
            print(f"test_WaveletDecodeV12: {path} is missing; its cases are skipped")
    return _files[name]


def available(limit=None):
    out = []
    for name in FILES:
        w = wave_file(name)
        if w is None or w.version != 0x12:
            continue
        bound = 32 if SMALL else None
        if limit is not None:
            bound = limit if bound is None else min(bound, limit)
        if bound is not None and max(w.width, w.height) > bound:
            continue
        out.append(w)
    return out


# ----------------------------------------------------------------- streams the test makes


def transform_size(length):
    bits, rest = 1, ((length - 1) & MASK) >> 1
    while rest:
        rest >>= 1
        bits += 1
    bits = max(bits, 3)
    return 0 if bits >= 32 else 1 << bits


def image_size(width, height):
    return max(transform_size(width), transform_size(height))


def random_coefficients(r, n):
    """Mostly zeros and small values, as real detail coefficients are, and some that need the
    escape (-128 and anything outside -127..127)."""
    out = []
    for _ in range(n):
        k = r.random()
        if k < 0.5:
            out.append(0)
        elif k < 0.8:
            out.append(r.randint(-20, 20))
        elif k < 0.96:
            out.append(r.randint(-127, 127))
        else:
            out.append(r.choice([-128, 128, -129, 32767, -32768, r.randint(-32768, 32767)]))
    return out


def pack_coefficients(r, values):
    b = bytearray()
    for v in values:
        if -127 <= v <= 127 and r.random() > 0.02:
            b.append(v & 0xFF)
        else:
            b.append(0x80)      # escape, then the little-endian 16-bit value (even a small one)
            b += struct.pack("<h", v)
    return bytes(b)


def bilz(payload):
    """A block as the files store it: the 0x1c-byte header, then the zlib stream."""
    packed = zlib.compress(payload)
    return struct.pack("<7I", 0x5A4C4942, len(payload), len(packed), 15, 9, 0, 0) + packed


def synthetic(r, width, height, alpha):
    """Unpacked streams for an image of this size (the brightness at the transform's size, the
    two differences at half, the alpha at full size) and the blocks they come from."""
    size = image_size(width, height)
    colour = pack_coefficients(r, random_coefficients(r, size * size + 2 * (size // 2) ** 2))
    block = pack_coefficients(r, random_coefficients(r, size * size)) if alpha else None
    return colour, block


def constant_stream(size, luma, cb, cr):
    """A colour stream of constant smooth coefficients and no details."""
    half = size // 2
    rows = [luma] * half + [0] * half
    small = [cb] * (half // 2) + [0] * (half // 2)
    other = [cr] * (half // 2) + [0] * (half // 2)
    return bytes(v & 0xFF for v in rows * size + small * half + other * half)


# ----------------------------------------------------------------- the emulator's fused multiply-adds

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(uc, i):
    v = uc.reg_read(ppcemu.FPR[i])
    if not isinstance(v, int):
        return float(v)
    return struct.unpack(">d", struct.pack(">Q", v))[0]


def scan_fused(e):
    """address -> (kind, frD, frA, frB, frC) of every fused instruction in the unit."""
    out = {}
    for begin, end in FUSED_RANGES:
        for a in range(begin, end, 4):
            word = struct.unpack(">I", e.read(a, 4))[0]
            if word >> 26 == 59 and ((word >> 1) & 31) in (28, 29, 30, 31):
                assert word & 1 == 0, "a fused instruction that sets CR1"
                out[a] = ((word >> 1) & 31, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31)
    return out


class HardwareRounding:
    """fmadds, fmsubs, fnmadds, fnmsubs with one rounding: a hook on each fused instruction of
    the unit computes frA * frC +- frB with fmaf (negated for the n forms, after rounding),
    writes frD and skips the instruction. Non-finite operands are left to the emulator.
    `changed` counts the results where Unicorn's double rounding would have differed."""

    changed = 0
    fused = 0

    def __init__(self, e):
        self.e, self.hooks = e, []

    def __enter__(self):
        for address in self.e.wd12_fused:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))
        return self

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        kind, rd, ra, rb, rc = self.e.wd12_fused[address]
        a, c, b = fpr(uc, ra), fpr(uc, rc), fpr(uc, rb)
        if not (math.isfinite(a) and math.isfinite(c) and math.isfinite(b)):
            return
        addend = -b if kind in (28, 30) else b
        value = float(LIBM.fmaf(a, c, addend))
        HardwareRounding.fused += 1
        if math.isfinite(value) and f32(a * c + addend) != value:
            HardwareRounding.changed += 1
        if kind in (30, 31):
            value = -value
        uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
        uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the decoder's statics


def _plane_pattern():
    """The planes' contents before each case: finite values, the same on both sides."""
    values = array.array("f", bytes(4 * 5 * PLANE_FLOATS))
    x = 0x1B873593
    for i in range(len(values)):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        values[i] = ((x >> 8) & 0xFFFF) / 64.0 - 512.0
    little = values.tobytes()
    values.byteswap()
    return little, values.tobytes()


PATTERN_LE = PATTERN_BE = WORKSPACE_FILL = None


def canonical(plane):
    """A plane (little-endian floats) with every NaN as one bit pattern."""
    words = array.array("I", plane)
    for i, w in enumerate(words):
        if w & 0x7F800000 == 0x7F800000 and w & 0x7FFFFF:
            words[i] = CANONICAL_NAN
    return words.tobytes()


def ppc_reset(e, state):
    """The decoder's statics on the PowerPC side: planes, coefficients, LZSS words, workspace."""
    e.write(PLANES, PATTERN_BE)
    for name, address in COEFFICIENTS.items():
        e.write(address, struct.pack(">f", state["h"][name]))
    for name in ("count", "buffer", "unused", "pos"):
        e.write(LZ[name], struct.pack(">I", state["lz"][name] & MASK))
    e.write(LZ["dest"], struct.pack(">I", 0))
    e.write(LZ["source"], struct.pack(">I", 0))
    workspace = e.alloc(WORKSPACE_SIZE, WORKSPACE_FILL)
    e.write(WORKSPACE_POINTER, struct.pack(">I", workspace))
    return workspace


def ppc_statics(e, labels, nan):
    """The statics after a call; pointers as the labels of what they point at."""
    data = e.read(PLANES, 5 * 4 * PLANE_FLOATS)
    planes = []
    for k in range(5):
        values = array.array("f", data[k * 4 * PLANE_FLOATS:(k + 1) * 4 * PLANE_FLOATS])
        values.byteswap()
        planes.append(canonical(values.tobytes()) if nan else values.tobytes())
    lz = {name: e.u32(LZ[name]) for name in ("count", "buffer", "unused", "pos")}
    for name in ("dest", "source"):
        lz[name] = labels.get(e.u32(LZ[name]), "other")
    workspace = e.u32(WORKSPACE_POINTER)
    h = [fbits(struct.unpack(">f", e.read(COEFFICIENTS[n], 4))[0]) for n in ("h0", "h1", "h2", "h3")]
    return dict(planes=planes, lz=lz, workspace=e.read(workspace, WORKSPACE_SIZE), h=h)


class Host:
    """The unit library's statics and entry points."""

    def __init__(self, lib):
        self.lib = lib
        g = lib.tpw_wd12_global
        g.restype, g.argtypes = C.c_void_p, [C.c_char_p]
        self.addr = {n: g(n.encode()) for n in ("planes", "h0", "h1", "h2", "h3", "count", "buffer", "unused", "pos",
                                                 "dest", "source", "workspace", "object")}
        heap = lib.tpw_wd12_heap
        heap.restype, heap.argtypes = None, [C.POINTER(C.c_int32)]
        self.heap_fn = heap
        self.workspace = C.create_string_buffer(WORKSPACE_SIZE)
        self.buffer = C.create_string_buffer(16)
        lib.tpw_wd12_set_buffer.restype, lib.tpw_wd12_set_buffer.argtypes = None, [C.c_void_p]
        lib.tpw_wd12_set_buffer(C.addressof(self.buffer))
        lib.tpw_wd12_payload_reset.restype, lib.tpw_wd12_payload_reset.argtypes = None, []
        lib.tpw_wd12_payload.restype, lib.tpw_wd12_payload.argtypes = None, [C.c_char_p, C.c_int32]

    def heap(self):
        out = (C.c_int32 * 3)()
        self.heap_fn(out)
        return list(out)

    def plant(self, payloads):
        self.lib.tpw_wd12_payload_reset()
        for p in payloads:
            if p is None:
                self.lib.tpw_wd12_payload(None, -1)
            else:
                self.lib.tpw_wd12_payload(p, len(p))

    def reset(self, state):
        C.memmove(self.addr["planes"], PATTERN_LE, len(PATTERN_LE))
        for name in ("h0", "h1", "h2", "h3"):
            C.c_float.from_address(self.addr[name]).value = state["h"][name]
        for name in ("count", "buffer", "unused", "pos"):
            C.c_uint32.from_address(self.addr[name]).value = state["lz"][name] & MASK
        C.c_void_p.from_address(self.addr["dest"]).value = None
        C.c_void_p.from_address(self.addr["source"]).value = None
        C.memmove(self.workspace, WORKSPACE_FILL, WORKSPACE_SIZE)
        C.c_void_p.from_address(self.addr["workspace"]).value = C.addressof(self.workspace)

    def statics(self, labels, nan):
        planes = []
        for k in range(5):
            plane = C.string_at(self.addr["planes"] + k * 4 * PLANE_FLOATS, 4 * PLANE_FLOATS)
            planes.append(canonical(plane) if nan else plane)
        lz = {name: C.c_uint32.from_address(self.addr[name]).value for name in ("count", "buffer", "unused", "pos")}
        for name in ("dest", "source"):
            lz[name] = labels.get(C.c_void_p.from_address(self.addr[name]).value or 0, "other")
        h = [fbits(C.c_float.from_address(self.addr[n]).value) for n in ("h0", "h1", "h2", "h3")]
        return dict(planes=planes, lz=lz, workspace=self.workspace.raw, h=h)


HOST = None


def host(lib):
    global HOST
    if HOST is None or HOST.lib is not lib:
        HOST = Host(lib)
    return HOST


def default_state(r=None, random_h=False):
    h = dict(REAL_H)
    if random_h and r is not None:
        h = {n: f32(r.uniform(-1.5, 1.5)) for n in ("h0", "h1", "h2", "h3")}
    lz = dict(count=0, buffer=0, unused=0, pos=0)
    if r is not None:
        lz = dict(count=r.getrandbits(32), buffer=r.getrandbits(32), unused=r.getrandbits(32), pos=r.getrandbits(32))
    return dict(h=h, lz=lz)


def setup(runner):
    global PATTERN_LE, PATTERN_BE, WORKSPACE_FILL, REAL_H
    e = runner.emu
    T.install_std_stubs(e)
    e.wd12_fused = scan_fused(e)
    if PATTERN_LE is None:
        PATTERN_LE, PATTERN_BE = _plane_pattern()
        WORKSPACE_FILL = bytes((i * 11 + 5) & 0xFF for i in range(WORKSPACE_SIZE))
    hs = host(runner.lib)
    REAL_H = {n: C.c_float.from_address(hs.addr[n]).value for n in ("h0", "h1", "h2", "h3")}


def stubs():
    """LbZLib_MemDecompress (an import of the engine) is the one callee outside the unit: it
    records its three pointers and writes the next planted payload into its destination. The
    unit's own functions and GetAlphaMask run as the original even if another module left a
    stub on them."""
    st = CL.Stubs()

    def unpacker(e):
        L = e.cl
        L.rec("LbZLib_MemDecompress", [L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))])
        payload = e.wd12_payloads.pop(0) if e.wd12_payloads else None
        if payload:
            e.write(e.r(4), payload)
        e.set_r(3, T.u32(L.ret("zlib")))

    st.imp(UNPACKER, unpacker)
    st.real = set(REAL)
    return st


def ppc_object(e, buffer):
    """A CWaveletImp in scratch memory with the original's vtables and mBuffer."""
    obj = e.alloc(0x18)
    e.write(obj, struct.pack(">IIIIII", obj + 0x10, VTABLE, 1, buffer, BASE_VTABLE, 0))
    return obj


def ppc_call(e, entry, args):
    with HardwareRounding(e):
        return T.call_ppc(e, entry, args, max_insns=600_000_000, timeout_us=1_200_000_000)


def heap_counts(e, start):
    log = e.alloc_log[start:]
    return [sum(1 for k, a, s in log if k == "new[]"), sum(1 for k, a, s in log if k == "delete[]"),
            sum(1 for k, a, s in log if k == "delete")]


# ----------------------------------------------------------------- the decoder and the dispatch
# A case: dict(entry="v12" | "dispatch", ints=[...] (the integer arguments after `this` up to
# the blocks), data, alpha (the blocks' bytes or None), payloads (what the unpacker writes,
# call by call; None writes nothing), front and dest_len (the destination buffer: the margin
# before the destination and the whole length), scales [4], state, q (queues), nan).


def decoder_ppc(e, case):
    e.reset_scratch()
    heap_mode, e.heap_in_scratch = e.heap_in_scratch, True
    e.cl = CL.PpcLog({}, case.get("q"))
    e.wd12_payloads = list(case["payloads"])
    saved = stubs().activate(e)
    try:
        workspace = ppc_reset(e, case["state"])
        buffer = e.alloc(16)
        obj = ppc_object(e, buffer)
        data = e.alloc(len(case["data"]) + 8, case["data"] + b"\0" * 8) if case["data"] is not None else 0
        alpha = e.alloc(len(case["alpha"]) + 8, case["alpha"] + b"\0" * 8) if case["alpha"] is not None else 0
        whole = e.alloc(case["dest_len"], b"\xa5" * case["dest_len"])
        dest = whole + case["front"]
        e.cl.labels.update({a: l for a, l in ((obj, OBJECT), (data, DATA), (alpha, ALPHA), (dest, DEST),
                                              (buffer, BUFFER), (workspace, WORKSPACE)) if a})
        args = [obj] + [T.u32(v) for v in case["ints"]] + [data, alpha, dest] + [T.F(s) for s in case["scales"]]
        start = len(e.alloc_log)
        ppc_call(e, FN[case["entry"]], args)
        labels = {data: "data", alpha: "alpha", workspace: "workspace", dest: "dest"}
        labels.pop(0, None)
        pixels = array.array("H", e.read(whole, case["dest_len"]))
        pixels.byteswap()
        return dict(log=e.cl.log, heap=heap_counts(e, start), pixels=pixels.tobytes(),
                    statics=ppc_statics(e, labels, case.get("nan")))
    finally:
        CL.Stubs.deactivate(e, saved)
        e.heap_in_scratch = heap_mode


def decoder_host(lib, case):
    hs = host(lib)
    hs.reset(case["state"])
    hs.plant(case["payloads"])
    data = C.create_string_buffer(case["data"] + b"\0" * 8) if case["data"] is not None else None
    alpha = C.create_string_buffer(case["alpha"] + b"\0" * 8) if case["alpha"] is not None else None
    whole = C.create_string_buffer(b"\xa5" * case["dest_len"], case["dest_len"])
    dest = C.addressof(whole) + case["front"]
    addr = lambda b: C.addressof(b) if b is not None else None
    CL.host_begin(lib, {OBJECT: hs.addr["object"], DATA: addr(data), ALPHA: addr(alpha), DEST: dest,
                        BUFFER: C.addressof(hs.buffer), WORKSPACE: C.addressof(hs.workspace)}, case.get("q"))
    hs.heap()
    scales = [C.c_float(s) for s in case["scales"]]
    blocks = [C.c_void_p(addr(data)), C.c_void_p(addr(alpha)), C.c_void_p(dest)]
    if case["entry"] == "v12":
        fn = lib.tpw_wd12_v12
        fn.argtypes = [C.c_int32] * 3 + [C.c_void_p] * 3 + [C.c_float] * 4
    else:
        fn = lib.tpw_wd12_dispatch
        fn.argtypes = [C.c_int32] * 4 + [C.c_void_p] * 3 + [C.c_float] * 4
    fn.restype = C.c_int32
    fn(*([C.c_int32(T.s32(v)) for v in case["ints"]] + blocks + scales))
    log = CL.host_end(lib)
    labels = {addr(data): "data", addr(alpha): "alpha", C.addressof(hs.workspace): "workspace", dest: "dest"}
    labels.pop(None, None)
    return dict(log=log, heap=hs.heap(), pixels=whole.raw, statics=hs.statics(labels, case.get("nan")))


def v12_case(width, height, bpp, data, alpha, payloads, scales, state=None, entry="v12", ints=None, q=None,
             nan=False):
    """A case for a width x height image; the destination has room for the row before it and
    the row and column after it that the 2 x 2 writes reach."""
    w, h = max(width, 0), max(height, 0)
    front = 2 * (w + 2) + 64
    return dict(entry=entry, ints=ints if ints is not None else [width, height, bpp], data=data, alpha=alpha,
                payloads=list(payloads), front=front, dest_len=front + 2 * (w + 2) * (h + 2) + 64,
                scales=list(scales), state=state or default_state(), q=q or {"zlib": [0]}, nan=nan)


def file_case(w, bpp=2, alpha=True, scales=None, state=None, entry="v12", width=None, height=None):
    """A case for a real file (its alpha block left out when `alpha` is False)."""
    block = w.alpha if alpha else None
    payloads = w.payloads[:2 if block is not None else 1]
    sizes = [len(p) if p is not None else 0 for p in payloads]
    width = w.width if width is None else width
    height = w.height if height is None else height
    ints = [width, height, bpp] if entry == "v12" else [0x12, width, height, bpp]
    return v12_case(width, height, bpp, w.data, block, payloads, scales or w.scales, state, entry, ints,
                    q={"zlib": sizes})


def synthetic_case(r, width, height, bpp, alpha, scales, state=None, entry="v12", version=0x12):
    colour, block = synthetic(r, width, height, alpha)
    ints = [width, height, bpp] if entry == "v12" else [version, width, height, bpp]
    payloads = [colour] + ([block] if alpha else [])
    return v12_case(width, height, bpp, bilz(colour), bilz(block) if alpha else None, payloads, scales, state, entry,
                    ints, q={"zlib": [len(colour), len(block) if alpha else 0]})


def v12_edges():
    import random
    r = random.Random(12)
    files = available()
    for w in files:
        yield file_case(w)
    small = [w for w in files if max(w.width, w.height) <= 16]
    alpha_files = [w for w in files if w.alpha is not None and max(w.width, w.height) <= 32]
    for w in small[:3] + alpha_files[:2]:
        yield file_case(w, bpp=4)                                   # 4 bytes a pixel: no pixels
    for w in alpha_files[:3]:
        yield file_case(w, alpha=False)                             # the alpha block left out
    for bpp in (0, 1, 3, -1, 0x7FFFFFFF):
        yield synthetic_case(r, 8, 8, bpp, True, [11.0, 8.5, 8.5, 21.0])
    # The smallest images, a single eight-column block, odd and empty sizes.
    for width, height in ((8, 8), (1, 1), (2, 1), (1, 2), (3, 3), (7, 5), (5, 7), (9, 9), (16, 16), (15, 17),
                          (17, 3), (2, 2)):
        yield synthetic_case(r, width, height, 2, False, [11.0, 8.5, 8.5, 21.0])
        yield synthetic_case(r, width, height, 2, True, [11.0, 8.5, 8.5, 21.0])
    for width, height in ((0, 0), (0, 8), (8, 0), (-1, 8), (8, -1), (-5, -5), (0, 1), (1, 0), (-3, 16)):
        yield synthetic_case(r, width, height, 2, True, [11.0, 8.5, 8.5, 21.0])
    for width, height in ((0, 8), (-3, 16), (8, 0), (0, 0)):
        yield synthetic_case(r, width, height, 2, False, [11.0, 8.5, 8.5, 21.0])
    # The quality words at their extremes (0 and 65535 give these scales), and other scales.
    yield synthetic_case(r, 16, 16, 2, True, [1.0, 1.0, 1.0, 1.0])
    yield synthetic_case(r, 16, 16, 2, True, [32768.5, 16384.75, 16384.75, 65536.0])
    yield synthetic_case(r, 8, 8, 2, True, [0.0, 0.0, 0.0, 0.0])
    yield synthetic_case(r, 8, 8, 2, True, [-0.0, -0.0, -0.0, -0.0])
    yield synthetic_case(r, 8, 8, 2, True, [-11.0, 8.5, -8.5, -21.0])
    yield synthetic_case(r, 8, 8, 2, False, [1e6, -1e6, 1e6, 1e6])                 # truncation saturates
    yield synthetic_case(r, 8, 8, 2, True, [3e4, 3e4, -3e4, 3e4])
    # Scales that make infinities and NaNs (NaNs compare as one value).
    for scales in ([1e38, 1e38, 1e38, 1e38], [math.inf, 1.0, -math.inf, math.inf], [math.nan, 1.0, 1.0, math.nan]):
        case = synthetic_case(r, 8, 8, 2, True, scales)
        case["nan"] = True
        yield case
    # Constant colour streams: every pixel of a square the same, every component in range.
    for luma, cb, cr in ((100, 4, 5), (0, 0, 0), (127, -127, 127), (-60, 30, -30)):
        stream = constant_stream(8, luma, cb, cr)
        yield v12_case(8, 8, 2, bilz(stream), None, [stream], [1.0, 1.0, 1.0, 1.0])
    # The green offset fused or not: this brightness scale (found by a search over its last bits) puts pixel (0, 1)'s
    # green component at 15.99999 with the fused offset and 16 with an unfused one, so its nibble tells them apart.
    green, scale = constant_stream(8, 100, 4, 5), bits_f(0x3F149C90)
    alpha_rows = bytes([60, 0, 0, 0, 0, 0, 0, 0] * 8)
    yield v12_case(8, 8, 2, bilz(green), None, [green], [scale, 8.5, 8.5, 1.0])
    yield v12_case(8, 8, 2, bilz(green), bilz(alpha_rows), [green, alpha_rows], [scale, 8.5, 8.5, 1.0])
    # The unpacker writing nothing (a block without the tag): the stale workspace is decoded.
    yield v12_case(16, 16, 2, b"\x01\x02\x03\x04", b"\x05\x06", [None, None], [11.0, 8.5, 8.5, 21.0])
    colour, block = synthetic(r, 8, 8, True)
    yield v12_case(8, 8, 2, bilz(colour), bilz(block), [colour, None], [11.0, 8.5, 8.5, 21.0])
    # Random D4 coefficients.
    yield synthetic_case(r, 16, 16, 2, True, [11.0, 8.5, 8.5, 21.0], default_state(r, random_h=True))
    # A real file decoded at another size than its own (the stream is read for that size).
    if small:
        w = small[-1]
        yield file_case(w, width=w.width - 3, height=w.height - 1)


def random_scale(r):
    k = r.random()
    if k < 0.5:
        return f32(r.uniform(1.0, 25.0))
    if k < 0.85:
        return f32(r.uniform(-40.0, 40.0))
    return r.choice([0.0, -0.0, 1.0, 11.0, 8.5, 21.0, 1e5, -1e5, 32768.5, 65536.0])


def random_image(r):
    k = r.random()
    if k < 0.7:
        return r.randint(1, 24), r.randint(1, 24)
    if k < 0.9:
        side = r.choice([8, 16, 26, 32])
        return side, side
    return r.randint(1, 48), r.randint(1, 48)


def v12_random(r):
    width, height = random_image(r)
    alpha = r.random() < 0.5
    bpp = r.choice([2, 2, 2, 2, 2, 2, 4, 1, 3, 0])
    case = synthetic_case(r, width, height, bpp, alpha, [random_scale(r) for _ in range(4)],
                          default_state(r, random_h=r.random() < 0.2))
    k = r.random()
    if k < 0.05:
        case["payloads"][0] = None                                  # nothing unpacked: stale workspace
    elif k < 0.1 and alpha:
        case["payloads"][1] = None
    elif k < 0.2:
        payload = case["payloads"][0]
        case["payloads"][0] = payload[:r.randint(0, len(payload))]  # a short stream: the rest is stale
    return case


def dispatch_edges():
    import random
    r = random.Random(13)
    for w in available(32)[:6]:
        yield file_case(w, entry="dispatch")
    for w in [w for w in available(16) if w.alpha is not None][:1]:
        yield file_case(w, bpp=4, entry="dispatch")
        yield file_case(w, alpha=False, entry="dispatch")
    for version in (0x12, 0x11, 0x14, 0, -1):
        yield synthetic_case(r, 8, 8, 2, True, [11.0, 8.5, 8.5, 21.0], entry="dispatch", version=version)
    yield synthetic_case(r, 5, 3, 2, False, [11.0, 8.5, 8.5, 21.0], entry="dispatch")


def dispatch_random(r):
    width, height = r.randint(1, 16), r.randint(1, 16)
    version = r.choice([0x12, 0x12, 0x12, 0x12, 0x11, 0x14, r.randint(-0x80000000, 0x11)])
    return synthetic_case(r, width, height, r.choice([2, 2, 4, 3]), r.random() < 0.5,
                          [random_scale(r) for _ in range(4)], default_state(r), entry="dispatch", version=version)


TESTS = [
    FnTest("CWaveletImp::Decompress_v12", decoder_ppc, decoder_host, edge=v12_edges, rnd=v12_random),
    FnTest("CWaveletImp::Decompress_v12 through the version dispatch", decoder_ppc, decoder_host,
           edge=dispatch_edges, rnd=dispatch_random),
]
