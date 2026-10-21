"""The version 0x13 wavelet texture decoder (engine, src/engine/EngineUtil.cpp, src/engine/Wavelet.cpp): LzssDecompress,
read_code, read_reset, CIgArray<float, float> (constructor, destructor, SetSize, GetData),
CWaveletImp::ReverseTransformVert, Decompress_v13 and the two dispatching Decompress overloads.

Decompress_v13 and the dispatch run on REAL version 0x13 files read at run time from
ROOT/extracted/wads (never copied into the repository; a missing file is skipped with a
message) and on random coefficient streams the test encodes itself, so they are valid. Before
every case both sides get the same decoder statics (the five planes, the D4 coefficients, the
LZSS state, a workspace for the LZSS output); afterwards all of them are compared with the
destination pixels, the heap traffic (blocks through new[], delete[], delete) and the call log.
Decompress_v12 is a recording fake on both sides; CTextureSettings::GetAlphaMask runs for real.

The original's fused multiply-adds run under HardwareRounding: Unicorn computes fmadds and
fnmsubs in double precision and rounds twice, the processor once. A hook on each fused
instruction of the unit computes the single-rounded result with the C library's fmaf, writes
it and skips the instruction (the variant of tests/test_OpenGLDraw.py's hook that only stops at
the fused instructions, since Decompress_v13 runs millions of others).

TPW_WD_SMALL=1 in the environment keeps the real files to 32 x 32 and below (for branchcov.py,
whose hook on every instruction is slow).
"""
import array
import ctypes as C
import math
import os
import pathlib
import struct

from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_WaveletDecode"

FN = dict(lzss=0x10039210, read_code=0x10039360, read_reset=0x10039424, dispatch=0x10039718, blit=0x100397dc,
          reverse_vert=0x1003d054, get_data=0x1003d1f8, v13=0x1003d230, ctor=0x1003ee44, dtor=0x1003ee88,
          set_size=0x1003ef48)
V12, SINIT, GET_ALPHA_MASK = 0x1003b0d4, 0x1003f1f0, 0x10029150
VTABLE, BASE_VTABLE = 0x10051f94, 0x10051fe4
PLANES, PLANE_FLOATS = 0x10173614, 0x10000
COEFFICIENTS = dict(h0=0x10173610, h1=0x1017360c, h2=0x10173608, h3=0x10173604)
LZ = dict(count=0x101735ec, buffer=0x101735f0, unused=0x101735f4, pos=0x101735f8, dest=0x101735fc, source=0x10173600)
WORKSPACE_POINTER, WORKSPACE_SIZE = 0x102b3614, 0x60000
FUSED_RANGES = [(0x1003d054, 0x1003d1b8), (0x1003d230, 0x1003eccc)]
MASK = 0xFFFFFFFF
REAL_H = None     # the host's D4 coefficients (set in setup)

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
WADS = ROOT / "extracted" / "wads"
# Real version 0x13 files: lobby textures of every size there is, with and without the alpha
# block, and level textures with odd shapes and other quality words.
FILES = [
    "lobby.wad.d/terrain/stexture/GK_top.wct",            # 8 x 8
    "lobby.wad.d/globe/stexture/LB_side2.wct",             # 16 x 16
    "lobby.wad.d/terrain/stexture/fgt_brim.wct",           # 16 x 16, alpha
    "lobby.wad.d/terrain/stexture/Sen_ant2.wct",           # 32 x 32, alpha
    "lobby.wad.d/globe/stexture/Lo_glo13.wct",             # 32 x 32
    "levels/hallow/sideshow/arcade.wad.d/stexture/ha_horn.wct",            # 12 x 12
    "levels/fantasy/features/donut.wad.d/stexture/puddle.wct",             # quality 58 75 64 12, alpha
    "levels/space/features/_pulsar_29.wad.d/stexture/pu_base2.wct",        # quality 5 30 12 20
    "levels/hallow/dynamic.wad.d/stexture/orange.wct",                     # alpha scale 1
    "lobby.wad.d/globe/textures/LB_side2.wct",             # 64 x 64
    "lobby.wad.d/terrain/textures/big_grass.wct",          # 64 x 64, alpha
    "levels/hallow/features/gates.wad.d/stexture/sign.wct",                # 64 x 32
    "levels/hallow/sideshow/arcade.wad.d/textures/ha_horn.wct",            # 48 x 48
    "lobby.wad.d/globe/textures/Lo_glo13.wct",             # 128 x 128
    "lobby.wad.d/terrain/textures/Sen_ant2.wct",           # 128 x 128, alpha
    "levels/space/rides/scentro.wad.d/textures/Sp_Botplate1b.wct",         # 127 x 128
    "levels/hallow/features/gates.wad.d/textures/sign.wct",                # 256 x 128
]
SMALL = os.environ.get("TPW_WD_SMALL") == "1"


def f32(x):
    return T.f32(x)


def fbits(x):
    return CL.fbits(x)


def bits_f(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


# ----------------------------------------------------------------- the files

class WaveFile:
    """A .WCT file as CWaveLoad loads it: the header, the blocks, the four scales."""

    def __init__(self, name, raw):
        self.name = name
        self.version, self.threshold, self.kind, self.level = raw[0], raw[1], raw[2], raw[3]
        self.width, self.height = struct.unpack("<HH", raw[4:8])
        q = struct.unpack("<4H", raw[8:16])
        data_size, alpha_size = struct.unpack("<II", raw[16:24])
        self.data = raw[0x1c:0x1c + data_size]
        self.alpha = raw[0x1c + data_size:0x1c + data_size + alpha_size] if self.kind == 0x20 else None
        # CWaveLoad::Decompress: fmadds(0.5, q0, 1), fmadds(0.25, q2, 1), fmadds(0.25, q1, 1), 1 + q3
        # (exact for these integers).
        self.scales = [f32(0.5 * q[0] + 1.0), f32(0.25 * q[2] + 1.0), f32(0.25 * q[1] + 1.0), f32(1.0 + q[3])]


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
            print(f"test_WaveletDecode: {path} is missing; its cases are skipped")
    return _files[name]


def available(limit=None):
    out = []
    for name in FILES:
        w = wave_file(name)
        if w is None or w.version != 0x13:
            continue
        bound = min(x for x in (32 if SMALL else None, limit) if x is not None) if (SMALL or limit) else None
        if bound is not None and max(w.width, w.height) > bound:
            continue
        out.append(w)
    return out


# ----------------------------------------------------------------- streams the test encodes

class BitWriter:
    """LZSS codes as read_code reads them: least significant bit first, bytes in order."""

    def __init__(self):
        self.out, self.acc, self.n = bytearray(), 0, 0

    def put(self, value, bits):
        self.acc |= (value & ((1 << bits) - 1)) << self.n
        self.n += bits
        while self.n >= 8:
            self.out.append(self.acc & 0xFF)
            self.acc >>= 8
            self.n -= 8

    def literal(self, byte):
        self.put(0, 1)
        self.put(byte, 8)

    def copy(self, offset, length):
        self.put(1, 1)
        self.put(offset, 12)
        self.put(length - 1, 7)

    def end(self, length_bits=0):
        self.put(1, 1)
        self.put(0, 12)
        self.put(length_bits, 7)

    def done(self):
        if self.n:
            self.out.append(self.acc & 0xFF)
            self.acc, self.n = 0, 0
        if len(self.out) % 2:
            self.out.append(0)
        return bytes(self.out) + b"\0" * 4      # the reader fetches two bytes at a time


def lzss_encode(data, max_tries=8):
    """Greedy LZSS of `data` (copies may overlap their own output)."""
    w = BitWriter()
    table = {}
    i, n = 0, len(data)
    while i < n:
        best_len, best_off = 0, 0
        if i + 3 <= n:
            for p in reversed(table.get(data[i:i + 3], [])[-max_tries:]):
                off = i - p
                if off > 4095:
                    continue
                length = 0
                while length < 128 and i + length < n and data[p + length] == data[i + length]:
                    length += 1
                if length > best_len:
                    best_len, best_off = length, off
        step = best_len if best_len >= 2 else 1
        if best_len >= 2:
            w.copy(best_off, best_len)
        else:
            w.literal(data[i])
        for k in range(i, i + step):
            if k + 3 <= n:
                table.setdefault(data[k:k + 3], []).append(k)
        i += step
    w.end()
    return w.done()


def lzss_plain(stream):
    """What LzssDecompress makes of a stream (to size buffers)."""
    out = bytearray()
    pos, acc, count = 0, 0, 0

    def code(bits):
        nonlocal pos, acc, count
        if count < bits:
            acc |= stream[pos] << count
            acc |= stream[pos + 1] << (count + 8)
            pos += 2
            count += 16
        v = acc & ((1 << bits) - 1)
        acc >>= bits
        count -= bits
        return v

    while True:
        if code(1) == 0:
            out.append(code(8))
            continue
        offset, length = code(12), code(7) + 1
        if offset == 0:
            return bytes(out)
        for _ in range(length):
            out.append(out[-offset])


def transform_size(length):
    bits, rest = 1, ((length - 1) & MASK) >> 1
    while rest:
        rest >>= 1
        bits += 1
    bits = max(bits, 3)
    return 0 if bits >= 32 else 1 << bits


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


def synthetic(r, width, height, alpha):
    """Valid packed blocks for an image of this size: the colour block (the brightness at the
    transform's size, the two differences at half) and the alpha block."""
    size = transform_size(width)
    size = max(size, transform_size(height))
    colour = random_coefficients(r, size * size + 2 * (size // 2) ** 2)
    data = lzss_encode(pack_coefficients(r, colour))
    block = lzss_encode(pack_coefficients(r, random_coefficients(r, size * size))) if alpha else None
    return data, block


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
    writes frD and skips the instruction. Non-finite operands are left to the emulator."""

    def __init__(self, e):
        self.e, self.hooks = e, []

    def __enter__(self):
        for address in self.e.wd_fused:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))
        return self

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        kind, rd, ra, rb, rc = self.e.wd_fused[address]
        a, c, b = fpr(uc, ra), fpr(uc, rc), fpr(uc, rb)
        if not (math.isfinite(a) and math.isfinite(c) and math.isfinite(b)):
            return
        value = float(LIBM.fmaf(a, c, -b if kind in (28, 30) else b))
        if kind in (30, 31):
            value = -value
        uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
        uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the decoder's statics


def _plane_pattern():
    """The planes' contents before each case: finite values, the same on both sides."""
    values = array.array("f", bytes(4 * 5 * PLANE_FLOATS))
    x = 0x2545F491
    for i in range(len(values)):
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        values[i] = ((x >> 8) & 0xFFFF) / 64.0 - 512.0
    little = values.tobytes()
    values.byteswap()
    return little, values.tobytes()


PATTERN_LE = PATTERN_BE = WORKSPACE_FILL = None


def ppc_reset(e, state):
    """The decoder's statics on the PowerPC side: planes, coefficients, LZSS state, workspace."""
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


def ppc_statics(e, labels):
    """The statics after a call; pointers as the labels of what they point at."""
    data = e.read(PLANES, 5 * 4 * PLANE_FLOATS)
    planes = []
    for k in range(5):
        values = array.array("f", data[k * 4 * PLANE_FLOATS:(k + 1) * 4 * PLANE_FLOATS])
        values.byteswap()
        planes.append(values.tobytes())
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
        g = lib.tpw_wd_global
        g.restype, g.argtypes = C.c_void_p, [C.c_char_p]
        self.addr = {n: g(n.encode()) for n in ("planes", "h0", "h1", "h2", "h3", "count", "buffer", "unused", "pos",
                                                 "dest", "source", "workspace", "object")}
        heap = lib.tpw_wd_heap
        heap.restype, heap.argtypes = None, [C.POINTER(C.c_int32)]
        self.heap_fn = heap
        self.workspace = C.create_string_buffer(WORKSPACE_SIZE)

    def heap(self):
        out = (C.c_int32 * 3)()
        self.heap_fn(out)
        return list(out)

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

    def statics(self, labels):
        planes = [C.string_at(self.addr["planes"] + k * 4 * PLANE_FLOATS, 4 * PLANE_FLOATS) for k in range(5)]
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
    e.wd_fused = scan_fused(e)
    if PATTERN_LE is None:
        PATTERN_LE, PATTERN_BE = _plane_pattern()
        WORKSPACE_FILL = bytes((i * 7 + 3) & 0xFF for i in range(WORKSPACE_SIZE))
    hs = host(runner.lib)
    REAL_H = {n: C.c_float.from_address(hs.addr[n]).value for n in ("h0", "h1", "h2", "h3")}


def stubs():
    """Decompress_v12 is the one callee outside the unit (recorded); the unit's own functions
    and GetAlphaMask run as the original even if another module left a stub on them."""
    st = CL.Stubs()

    def v12(e):
        L = e.cl
        ints = [L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), L.lab(e.r(7)), L.lab(e.r(8)),
                L.lab(e.r(9))] + [fbits(e.f(i)) for i in (1, 2, 3, 4)]
        L.rec("Decompress_v12", ints)
        e.set_r(3, T.u32(L.ret("v12")))

    st.internal(V12, v12)
    st.real = set(FN.values()) | {GET_ALPHA_MASK}
    return st


def ppc_object(e):
    """A CWaveletImp in scratch memory with the original's vtables."""
    obj = e.alloc(0x18)
    e.write(obj, struct.pack(">IIIIII", obj + 0x10, VTABLE, 1, 0, BASE_VTABLE, 0))
    return obj


def ppc_call(e, entry, args, rounding=True):
    if rounding:
        with HardwareRounding(e):
            return T.call_ppc(e, entry, args, max_insns=400_000_000, timeout_us=900_000_000)
    return T.call_ppc(e, entry, args, max_insns=400_000_000, timeout_us=900_000_000)


def heap_counts(e, start):
    log = e.alloc_log[start:]
    return [sum(1 for k, a, s in log if k == "new[]"), sum(1 for k, a, s in log if k == "delete[]"),
            sum(1 for k, a, s in log if k == "delete")]


# ----------------------------------------------------------------- the decoder and the dispatch
# A case: dict(entry="v13" | "dispatch" | "blit", ints=[...] (the integer arguments after `this`
# up to the blocks), data, alpha (bytes or None), dest_len, scales [4], state, q (queues)).


def decoder_ppc(e, case):
    e.reset_scratch()
    heap_mode, e.heap_in_scratch = e.heap_in_scratch, True
    e.cl = CL.PpcLog({}, case.get("q"))
    saved = stubs().activate(e)
    try:
        workspace = ppc_reset(e, case["state"])
        obj = ppc_object(e)
        data = e.alloc(len(case["data"]) + 8, case["data"] + b"\0" * 8) if case["data"] is not None else 0
        alpha = e.alloc(len(case["alpha"]) + 8, case["alpha"] + b"\0" * 8) if case["alpha"] is not None else 0
        dest = e.alloc(case["dest_len"], b"\xa5" * case["dest_len"])
        e.cl.labels.update({a: l for a, l in ((obj, 1), (data, 2), (alpha, 3), (dest, 4)) if a})
        ints = case["ints"]
        args = [obj] + [T.u32(v) for v in ints] + [data, alpha, dest] + [T.F(s) for s in case["scales"]]
        start = len(e.alloc_log)
        r3, _, _ = ppc_call(e, FN[case["entry"]], args)
        labels = {data: "data", alpha: "alpha", workspace: "workspace", dest: "dest"}
        labels.pop(0, None)
        pixels = array.array("H", e.read(dest, case["dest_len"]))
        pixels.byteswap()
        out = dict(log=e.cl.log, heap=heap_counts(e, start), pixels=pixels.tobytes(), statics=ppc_statics(e, labels))
        if case.get("compare_ret"):
            out["ret"] = T.s32(r3)
        return out
    finally:
        CL.Stubs.deactivate(e, saved)
        e.heap_in_scratch = heap_mode


def decoder_host(lib, case):
    hs = host(lib)
    hs.reset(case["state"])
    data = C.create_string_buffer(case["data"] + b"\0" * 8) if case["data"] is not None else None
    alpha = C.create_string_buffer(case["alpha"] + b"\0" * 8) if case["alpha"] is not None else None
    dest = C.create_string_buffer(b"\xa5" * case["dest_len"], case["dest_len"])
    addr = lambda b: C.addressof(b) if b is not None else None
    CL.host_begin(lib, {1: hs.addr["object"], 2: addr(data), 3: addr(alpha), 4: addr(dest)}, case.get("q"))
    hs.heap()
    ints = case["ints"]
    scales = [C.c_float(s) for s in case["scales"]]
    blocks = [C.c_void_p(addr(data)), C.c_void_p(addr(alpha)), C.c_void_p(addr(dest))]
    if case["entry"] == "v13":
        fn = lib.tpw_wd_v13
        fn.argtypes = [C.c_int32] * 3 + [C.c_void_p] * 3 + [C.c_float] * 4
    elif case["entry"] == "dispatch":
        fn = lib.tpw_wd_dispatch
        fn.argtypes = [C.c_int32] * 4 + [C.c_void_p] * 3 + [C.c_float] * 4
    else:
        fn = lib.tpw_wd_blit
        fn.argtypes = [C.c_int32] * 7 + [C.c_void_p] * 3 + [C.c_float] * 4
    fn.restype = C.c_int32
    result = fn(*([C.c_int32(T.s32(v)) for v in ints] + blocks + scales))
    log = CL.host_end(lib)
    labels = {addr(data): "data", addr(alpha): "alpha", C.addressof(hs.workspace): "workspace",
              addr(dest): "dest"}
    labels.pop(None, None)
    out = dict(log=log, heap=hs.heap(), pixels=dest.raw, statics=hs.statics(labels))
    if case.get("compare_ret"):
        out["ret"] = result
    return out


def dest_len(width, height):
    """Room for the pixels and the row and column the 2 x 2 writes go past an odd size."""
    return max(2 * (max(width, 0) + 2) * (max(height, 0) + 2) + 64, 64)


def v13_case(width, height, bpp, data, alpha, scales, state=None, entry="v13", ints=None, q=None, compare_ret=False):
    return dict(entry=entry, ints=ints if ints is not None else [width, height, bpp], data=data, alpha=alpha,
                dest_len=dest_len(width, height), scales=list(scales), state=state or default_state(), q=q,
                compare_ret=compare_ret)


def constant_stream(luma, cb, cr):
    """An 8 x 8 colour block of constant smooth coefficients and no details."""
    return lzss_encode(bytes(v & 0xFF for v in [luma, 0] * 32 + [cb, 0] * 8 + [cr, 0] * 8))


def v13_edges():
    # The first pixel's green component is 15.99999 with the fused green offset and 16 with an unfused
    # one, so its nibble tells them apart (the brightness scale was found by a search over its last bits).
    green, scale = constant_stream(100, 4, 5), bits_f(0x3F149C90)
    yield v13_case(8, 8, 2, green, None, [scale, 8.5, 8.5, 1.0])
    yield v13_case(8, 8, 2, green, lzss_encode(bytes([60, 0] * 32)), [scale, 8.5, 8.5, 1.0])
    files = available()
    for w in files:
        yield v13_case(w.width, w.height, 2, w.data, w.alpha, w.scales)
    by_name = {w.name: w for w in files}
    small = [w for w in files if max(w.width, w.height) <= 16]
    alpha_files = [w for w in files if w.alpha is not None and max(w.width, w.height) <= 32]
    for w in small[:2] + alpha_files[:1]:
        yield v13_case(w.width, w.height, 4, w.data, w.alpha, w.scales)      # 4 bytes a pixel: no pixels
    for w in alpha_files[:2]:
        yield v13_case(w.width, w.height, 2, w.data, None, w.scales)         # the alpha block left out
    eight = by_name.get(FILES[0])
    if eight is not None:
        yield v13_case(0, 0, 2, eight.data, None, eight.scales)              # no size: only the LZSS
        yield v13_case(0, 8, 2, eight.data, None, eight.scales)              # no width: rows of nothing
        yield v13_case(8, 0, 2, eight.data, None, eight.scales)              # no height: no rows
        yield v13_case(1, 1, 2, eight.data, None, eight.scales)              # one pixel, written over
        yield v13_case(7, 5, 2, eight.data, None, eight.scales)              # odd sizes
        yield v13_case(8, 8, 3, eight.data, None, eight.scales)
        yield v13_case(8, 8, 2, eight.data, None, [0.0, 0.0, 0.0, 0.0])
        yield v13_case(8, 8, 2, eight.data, None, [-11.0, 8.5, -8.5, 21.0])
        yield v13_case(8, 8, 2, eight.data, None, [1e6, -1e6, 1e6, 1e6])     # truncation saturates
    for w in alpha_files[:1]:
        yield v13_case(w.width, w.height, 2, w.data, w.alpha, [x * 6.0 for x in w.scales])
        yield v13_case(w.width, w.height, 2, w.data, w.alpha, [-x for x in w.scales])
        yield v13_case(0, 0, 2, w.data, w.alpha, w.scales)
        yield v13_case(0, w.height, 2, w.data, w.alpha, w.scales)


def random_scale(r):
    k = r.random()
    if k < 0.5:
        return f32(r.uniform(1.0, 25.0))
    if k < 0.85:
        return f32(r.uniform(-40.0, 40.0))
    return r.choice([0.0, -0.0, 1.0, 11.0, 8.5, 21.0, 1e5, -1e5])


def random_image(r):
    k = r.random()
    if k < 0.7:
        return r.randint(1, 24), r.randint(1, 24)
    if k < 0.9:
        side = r.choice([8, 16, 32])
        return side, side
    return r.randint(1, 48), r.randint(1, 48)


def v13_random(r):
    width, height = random_image(r)
    alpha = r.random() < 0.5
    data, block = synthetic(r, width, height, alpha)
    bpp = r.choice([2, 2, 2, 2, 2, 2, 4, 1, 3, 0])
    return v13_case(width, height, bpp, data, block, [random_scale(r) for _ in range(4)],
                    default_state(r, random_h=r.random() < 0.2))


def dispatch_edges():
    files = available(16)
    if files:
        w = files[0]
        data, alpha, scales, width, height = w.data, w.alpha, w.scales, w.width, w.height
    else:
        import random
        width = height = 8
        data, alpha = synthetic(random.Random(3), width, height, True)
        scales = [11.0, 8.5, 8.5, 21.0]
    for version in (0x13, 0x12, 0x11, 0x14, 0, -1, 0x7FFFFFFF, -0x80000000, 0x113):
        yield v13_case(width, height, 2, data, alpha, scales, entry="dispatch", ints=[version, width, height, 2],
                       q={"v12": [0x1234]}, compare_ret=version == 0x12)
    for w in files[:4]:
        yield v13_case(w.width, w.height, 2, w.data, w.alpha, w.scales, entry="dispatch",
                       ints=[0x13, w.width, w.height, 2])
        yield v13_case(w.width, w.height, 4, w.data, None, w.scales, entry="dispatch",
                       ints=[0x12, w.width, w.height, 4], q={"v12": [-7]}, compare_ret=True)


def dispatch_random(r):
    version = r.choice([0x13, 0x13, 0x12, 0x12, 0x11, 0x14, r.randint(-0x80000000, 0x7FFFFFFF), 0, 0x10])
    width, height = r.randint(1, 16), r.randint(1, 16)
    alpha = r.random() < 0.5
    data, block = synthetic(r, width, height, alpha) if version == 0x13 else (bytes(r.getrandbits(8) for _ in range(16)),
                                                                              b"\x01\x02" if alpha else None)
    bpp = r.choice([2, 2, 4, 3])
    return v13_case(width, height, bpp, data, block, [random_scale(r) for _ in range(4)], default_state(r),
                    entry="dispatch", ints=[version, width, height, bpp], q={"v12": [r.getrandbits(31)]},
                    compare_ret=version == 0x12)


def blit_case(version, width, height, dest_w, dest_h, pitch, bpp, data, alpha, scales, state=None):
    case = v13_case(width, height, bpp, data, alpha, scales, state, entry="blit",
                    ints=[version, width, height, dest_w, dest_h, pitch, bpp])
    case["dest_len"] = max(case["dest_len"], dest_len(dest_w, dest_h))
    return case


def blit_edges():
    files = available(16)
    if not files:
        return
    w = files[0]
    a, b = w.width, w.height
    for version in (0x13, 0x14, 0x12, 0x11, 0x7FFFFFFF, -1):
        yield blit_case(version, a, b, a, b, a, 2, w.data, w.alpha, w.scales)
    yield blit_case(0x13, a, b, a + 1, b, a, 2, w.data, w.alpha, w.scales)          # other width
    yield blit_case(0x13, a, b, a, b + 1, a, 2, w.data, w.alpha, w.scales)          # other height
    yield blit_case(0x13, a, b, a, b, 2 * a, 2, w.data, w.alpha, w.scales)          # a pitch in bytes
    yield blit_case(0x13, a, b, a, b, -a, 2, w.data, w.alpha, w.scales)             # flipped
    for w in files[1:4]:
        yield blit_case(0x13, w.width, w.height, w.width, w.height, w.width, 2, w.data, w.alpha, w.scales)


def blit_random(r):
    version = r.choice([0x13, 0x13, 0x14, 0x12, 0x11, r.randint(-0x80000000, 0x7FFFFFFF)])
    width, height = r.randint(1, 16), r.randint(1, 16)
    dest_w = width if r.random() < 0.7 else r.randint(0, 20)
    dest_h = height if r.random() < 0.7 else r.randint(0, 20)
    pitch = dest_w if r.random() < 0.6 else r.choice([2 * dest_w, -dest_w, r.randint(-40, 40)])
    alpha = r.random() < 0.5
    data, block = synthetic(r, width, height, alpha)
    return blit_case(version, width, height, dest_w, dest_h, pitch, r.choice([2, 2, 4]), data, block,
                     [random_scale(r) for _ in range(4)], default_state(r))


# ----------------------------------------------------------------- ReverseTransformVert
# A case: (count, stride, source values, dest values, alias, coefficients)


def reverse_lengths(count, stride):
    half = count >> 1
    return max(2 * half * stride + stride + 1, 2), max(2 * half, 2) + 4


def reverse_ppc(e, case):
    count, stride, source, dest, alias, h = case
    e.reset_scratch()
    saved = stubs().activate(e)
    try:
        for name, address in COEFFICIENTS.items():
            e.write(address, struct.pack(">f", h[name]))
        src = e.alloc(4 * len(source), struct.pack(">%df" % len(source), *source))
        dst = src if alias else e.alloc(4 * len(dest), struct.pack(">%df" % len(dest), *dest))
        ppc_call(e, FN["reverse_vert"], [0x7C0FE000, src, dst, T.u32(count), T.u32(stride)])
        out = dict(source=list(struct.unpack(">%df" % len(source), e.read(src, 4 * len(source)))))
        if not alias:
            out["dest"] = list(struct.unpack(">%df" % len(dest), e.read(dst, 4 * len(dest))))
        return out
    finally:
        CL.Stubs.deactivate(e, saved)


def reverse_host(lib, case):
    count, stride, source, dest, alias, h = case
    hs = host(lib)
    for name in ("h0", "h1", "h2", "h3"):
        C.c_float.from_address(hs.addr[name]).value = h[name]
    src = (C.c_float * len(source))(*source)
    dst = src if alias else (C.c_float * len(dest))(*dest)
    fn = lib.tpw_wd_reverse_vert
    fn.restype, fn.argtypes = None, [C.c_void_p, C.c_void_p, C.c_int32, C.c_int32]
    fn(C.addressof(src), C.addressof(dst), count, stride)
    out = dict(source=list(src))
    if not alias:
        out["dest"] = list(dst)
    return out


def reverse_case(r, count, stride, alias=False, h=None, wild=False):
    ns, nd = reverse_lengths(count, stride)
    if alias:
        ns = nd = max(ns, nd)
    pick = (lambda: T.rfloat(r)) if wild else (lambda: f32(r.uniform(-300.0, 300.0)))
    return (count, stride, [pick() for _ in range(ns)], [f32(r.uniform(-9.0, 9.0)) for _ in range(nd)], alias,
            h or dict(REAL_H))


def reverse_edges():
    import random
    r = random.Random(7)
    for count in (0, 1, 2, 3, 4, 5, 8, 16, 33):
        for stride in (1, 2, 5):
            yield reverse_case(r, count, stride)
    for count in (4, 8, 9):
        yield reverse_case(r, count, 1, alias=True)
        yield reverse_case(r, count, 2, alias=True)
    yield reverse_case(r, 16, 3, h=dict(h0=1.0, h1=2.0, h2=4.0, h3=8.0))
    yield reverse_case(r, 8, 1, wild=True)
    yield (4, 1, [math.inf, 1.0, -math.inf, 2.0, 0.0], [0.0] * 6, False, dict(REAL_H))
    yield (4, 1, [1e38, 3e38, -3e38, 1e38, 5.0], [0.0] * 6, False, dict(REAL_H))


def reverse_random(r):
    count = r.choice([r.randint(0, 40), r.choice([8, 16, 32, 64, 128])])
    stride = r.choice([1, 1, 2, 3, r.randint(1, 9)])
    h = dict(REAL_H) if r.random() < 0.6 else {n: f32(r.uniform(-2.0, 2.0)) for n in ("h0", "h1", "h2", "h3")}
    return reverse_case(r, count, stride, alias=r.random() < 0.1, h=h, wild=r.random() < 0.1)


# ----------------------------------------------------------------- LzssDecompress, read_code, read_reset
# An LZSS case: (stream bytes, output length, pre-state of the globals)


def lzss_ppc(e, case):
    stream, out_len, lz = case
    e.reset_scratch()
    saved = stubs().activate(e)
    try:
        for name in ("count", "buffer", "unused", "pos", "dest", "source"):
            e.write(LZ[name], struct.pack(">I", lz.get(name, 0) & MASK))
        src = e.alloc(len(stream), stream)
        dst = e.alloc(out_len, b"\x5a" * out_len)
        ppc_call(e, FN["lzss"], [src, dst], rounding=False)
        labels = {src: "source", dst: "dest"}
        state = {name: e.u32(LZ[name]) for name in ("count", "buffer", "unused", "pos")}
        state.update({name: labels.get(e.u32(LZ[name]), "other") for name in ("dest", "source")})
        return dict(out=e.read(dst, out_len), state=state)
    finally:
        CL.Stubs.deactivate(e, saved)


def lzss_host(lib, case):
    stream, out_len, lz = case
    hs = host(lib)
    for name in ("count", "buffer", "unused", "pos"):
        C.c_uint32.from_address(hs.addr[name]).value = lz.get(name, 0) & MASK
    src = C.create_string_buffer(stream, len(stream))
    dst = C.create_string_buffer(b"\x5a" * out_len, out_len)
    fn = lib.tpw_wd_lzss
    fn.restype, fn.argtypes = None, [C.c_void_p, C.c_void_p]
    fn(C.addressof(src), C.addressof(dst))
    labels = {C.addressof(src): "source", C.addressof(dst): "dest"}
    state = {name: C.c_uint32.from_address(hs.addr[name]).value for name in ("count", "buffer", "unused", "pos")}
    state.update({name: labels.get(C.c_void_p.from_address(hs.addr[name]).value or 0, "other")
                  for name in ("dest", "source")})
    return dict(out=dst.raw, state=state)


def lzss_case(writer, lz=None):
    stream = writer.done()
    return (stream, len(lzss_plain(stream)) + 64, lz or {})


def lzss_edges():
    w = BitWriter()
    w.end()
    yield lzss_case(w)                                       # nothing but the end code
    w = BitWriter()
    w.end(0x7F)
    yield lzss_case(w, dict(count=5, buffer=0xFFFFFFFF, unused=9, pos=77))   # end code with length bits
    w = BitWriter()
    for b in b"literal run of bytes \x00\xff\x80\x7f":
        w.literal(b)
    w.end()
    yield lzss_case(w)
    for length in (1, 2, 7, 8, 9, 15, 16, 17, 127, 128):
        w = BitWriter()
        w.literal(0x41)
        w.literal(0x42)
        w.copy(1, length)                                    # offset 1: a run of the last byte
        w.copy(2, length)
        w.end()
        yield lzss_case(w)
    w = BitWriter()
    for i in range(4095):
        w.literal((i * 37) & 0xFF)
    w.copy(4095, 128)                                        # the longest offset and length
    w.copy(4095, 9)
    w.end()
    yield lzss_case(w)
    w = BitWriter()
    w.literal(1)
    w.copy(1, 128)
    w.copy(129, 128)                                         # back to the start
    w.literal(2)
    w.end()
    yield lzss_case(w)
    for w0 in available(16)[:3]:
        yield (w0.data + b"\0" * 4, len(lzss_plain(w0.data + b"\0" * 4)) + 64, {})
        if w0.alpha is not None:
            yield (w0.alpha + b"\0" * 4, len(lzss_plain(w0.alpha + b"\0" * 4)) + 64, {})


def lzss_random(r):
    w = BitWriter()
    produced = 0
    for _ in range(r.randint(0, 120)):
        if produced == 0 or r.random() < 0.45:
            w.literal(r.getrandbits(8))
            produced += 1
        else:
            k = r.random()
            offset = 1 if k < 0.15 else r.randint(1, min(produced, 4095))
            length = r.choice([1, 8, 9, 128, r.randint(1, 128), r.randint(1, 20)])
            w.copy(offset, length)
            produced += length
    w.end(r.getrandbits(7) if r.random() < 0.2 else 0)
    lz = dict(count=r.getrandbits(32), buffer=r.getrandbits(32), unused=r.getrandbits(32), pos=r.getrandbits(32))
    return lzss_case(w, lz)


# A read_code case: (bits, count, buffer, pos, source bytes); read_reset: the same without bits.
def code_ppc(e, case, entry):
    bits, count, buffer, pos, source = case
    e.reset_scratch()
    saved = stubs().activate(e)
    try:
        src = e.alloc(len(source), source)
        for name, value in (("count", count), ("buffer", buffer), ("pos", pos), ("source", src), ("unused", 0x3C3C),
                            ("dest", 0x7C0FE000)):
            e.write(LZ[name], struct.pack(">I", value & MASK))
        r3, _, _ = ppc_call(e, FN[entry], [T.u32(bits)], rounding=False)
        out = {name: e.u32(LZ[name]) for name in ("count", "buffer", "pos", "unused")}
        out["source"] = e.u32(LZ["source"]) == src
        if entry == "read_code":
            out["ret"] = r3
        return out
    finally:
        CL.Stubs.deactivate(e, saved)


def code_host(lib, case, entry):
    bits, count, buffer, pos, source = case
    hs = host(lib)
    src = C.create_string_buffer(source, len(source))
    for name, value in (("count", count), ("buffer", buffer), ("pos", pos), ("unused", 0x3C3C)):
        C.c_uint32.from_address(hs.addr[name]).value = value & MASK
    C.c_void_p.from_address(hs.addr["source"]).value = C.addressof(src)
    out = {}
    if entry == "read_code":
        fn = lib.tpw_wd_read_code
        fn.restype, fn.argtypes = C.c_uint32, [C.c_uint32]
        out["ret"] = fn(bits & MASK)
    else:
        fn = lib.tpw_wd_read_reset
        fn.restype, fn.argtypes = None, []
        fn()
    out.update({name: C.c_uint32.from_address(hs.addr[name]).value for name in ("count", "buffer", "pos", "unused")})
    out["source"] = C.c_void_p.from_address(hs.addr["source"]).value == C.addressof(src)
    return out


def code_edges():
    src = bytes(range(0x30, 0x70))
    for bits in (0, 1, 7, 8, 12, 15, 16, 17, 31, 32, 33, 63, 64, 65):
        for count in (0, 1, 7, 8, 16, 17, 24, 31, 32, 40, 0xFFFFFFF8, 0xFFFFFFFF):
            yield (bits, count, 0x89ABCDEF, 3, src)


def code_random(r):
    bits = r.choice([1, 7, 8, 12, r.randint(0, 20), r.randint(0, 70)])
    count = r.choice([r.randint(0, 24), r.randint(0, 40), r.getrandbits(32)])
    return (bits, count, r.getrandbits(32), r.randint(0, 40), bytes(r.getrandbits(8) for _ in range(48)))


# ----------------------------------------------------------------- CIgArray<float, float>
# A script: [(op, a, b), ...] as tests/unit_WaveletDecode.cpp's tpw_wd_array reads it.


def fill_bits(seed, i):
    return ((seed * 2654435761) + i * 40503) & 0x3FFFFFFF


def array_ppc(e, script):
    e.reset_scratch()
    heap_mode, e.heap_in_scratch = e.heap_in_scratch, True
    saved = stubs().activate(e)
    try:
        obj = e.alloc(0x1C, b"\xa5" * 0x1C)
        ppc_call(e, FN["ctor"], [obj], rounding=False)
        words = struct.unpack(">7I", e.read(obj, 0x1C))
        out = [0 if words[0] == 0 else 1, T.s32(words[1]), T.s32(words[2]), T.s32(words[3])] + list(words[4:])
        for op, a, b in script:
            before = e.u32(obj)
            start = len(e.alloc_log)
            answer = -1
            if op == 0:
                ppc_call(e, FN["set_size"], [obj, T.u32(a), T.u32(b)], rounding=False)
            elif op == 1:
                data, size = e.u32(obj), T.s32(e.u32(obj + 4))
                if size > 0:
                    e.write(data, struct.pack(">%dI" % size, *[fill_bits(a, k) for k in range(size)]))
            elif op == 2:
                r3, _, _ = ppc_call(e, FN["get_data"], [obj], rounding=False)
                answer = 0 if r3 == 0 else (1 if r3 == e.u32(obj) else 2)
            else:
                ppc_call(e, FN["dtor"], [obj, T.u32(a)], rounding=False)
                out += [answer] + heap_counts(e, start)
                break
            heap = heap_counts(e, start)
            data, size, capacity, grow = struct.unpack(">IiiI", e.read(obj, 16))
            # the original's new[] sizes are four bytes per element of the new capacity
            for kind, address, n in e.alloc_log[start:]:
                if kind == "new[]" and n != (4 * capacity) & MASK:
                    heap[0] = -999
            out += [answer, int(data != before), size, capacity, T.s32(grow)] + heap
            if data:
                out += list(struct.unpack(">%dI" % max(size, 0), e.read(data, 4 * max(size, 0))))
        return out
    finally:
        CL.Stubs.deactivate(e, saved)
        e.heap_in_scratch = heap_mode


def array_host(lib, script):
    fn = lib.tpw_wd_array
    fn.restype, fn.argtypes = C.c_int32, [C.POINTER(C.c_int32), C.c_int32, C.POINTER(C.c_int64), C.c_int32]
    flat = [T.s32(v) for op in script for v in op]
    capacity = 1 << 16
    out = (C.c_int64 * capacity)()
    n = fn((C.c_int32 * max(len(flat), 1))(*flat), len(script), out, capacity)
    return list(out[:min(n, capacity)])


def array_edges():
    yield [(2, 0, 0), (3, -1, 0)]
    yield [(0, 0, -1), (2, 0, 0), (3, 1, 0)]
    yield [(0, 5, -1), (1, 3, 0), (2, 0, 0), (0, 5, -1), (0, 3, -1), (0, 5, -1), (3, -1, 0)]
    yield [(0, 4, -1), (1, 9, 0), (0, 6, -1), (1, 2, 0), (0, 0, -1), (0, 2, -1), (3, 1, 0)]
    yield [(0, 8, 0), (1, 1, 0), (0, 9, -1), (0, 60, -1), (0, 61, -1), (3, -1, 0)]       # step 4, then size / 8
    yield [(0, 1, 3), (1, 5, 0), (0, 2, -1), (0, 10, -1), (0, 11, 7), (0, 30, -1), (3, 1, 0)]
    yield [(0, 9000, 0), (1, 4, 0), (0, 9001, -1), (0, 10100, -1), (3, -1, 0)]           # step capped at 1024
    yield [(0, 3, -5), (0, 7, -1), (0, 4, 2), (0, 100, 1000), (0, 1200, -1), (3, 1, 0)]
    yield [(0, 2, -1), (2, 0, 0), (0, 0, 0), (2, 0, 0), (0, 0, -1), (3, -1, 0)]


def array_random(r):
    script = []
    for _ in range(r.randint(1, 10)):
        k = r.random()
        if k < 0.6:
            size = r.choice([0, r.randint(1, 16), r.randint(1, 80), r.randint(1, 300)])
            grow = r.choice([-1, -1, -1, 0, r.randint(1, 10), r.randint(-6, -2), 2000])
            script.append((0, size, grow))
        elif k < 0.85:
            script.append((1, r.getrandbits(16), 0))
        else:
            script.append((2, 0, 0))
    script.append((3, r.choice([-1, 1, 0, 2]), 0))
    return script


# ----------------------------------------------------------------- __sinit_Wavelet_cpp's values


def coefficients_ppc(e, case):
    """The case is what the four words hold before the initialisers run."""
    for word, address in zip(case, COEFFICIENTS.values()):
        e.write(address, struct.pack(">I", word))
    T.call_ppc(e, SINIT, [])
    return [fbits(struct.unpack(">f", e.read(COEFFICIENTS[n], 4))[0]) for n in ("h0", "h1", "h2", "h3")]


def coefficients_host(lib, case):
    hs = host(lib)
    return [fbits(C.c_float.from_address(hs.addr[n]).value) for n in ("h0", "h1", "h2", "h3")]


TESTS = [
    FnTest("D4 coefficients (the initialisers, __sinit_Wavelet_cpp)", coefficients_ppc, coefficients_host,
           edge=lambda: [[0, 0, 0, 0], [MASK] * 4], rnd=lambda r: [r.getrandbits(32) for _ in range(4)]),
    FnTest("CIgArray<float, float> (constructor, SetSize, GetData, destructor)", array_ppc, array_host,
           edge=array_edges, rnd=array_random),
    FnTest("read_reset", lambda e, c: code_ppc(e, c, "read_reset"), lambda lib, c: code_host(lib, c, "read_reset"),
           edge=lambda: [(0, 5, 0x1234, 3, b"\0" * 8), (0, 0xFFFFFFFF, 0xFFFFFFFF, 0, b"\0" * 8)], rnd=code_random),
    FnTest("read_code", lambda e, c: code_ppc(e, c, "read_code"), lambda lib, c: code_host(lib, c, "read_code"),
           edge=code_edges, rnd=code_random),
    FnTest("LzssDecompress", lzss_ppc, lzss_host, edge=lzss_edges, rnd=lzss_random),
    FnTest("CWaveletImp::ReverseTransformVert", lambda e, c: reverse_ppc(e, c), reverse_host, edge=reverse_edges,
           rnd=reverse_random),
    FnTest("CWaveletImp::Decompress_v13", decoder_ppc, decoder_host, edge=v13_edges, rnd=v13_random),
    FnTest("CWaveletImp::Decompress (the version dispatch)", decoder_ppc, decoder_host, edge=dispatch_edges,
           rnd=dispatch_random),
    FnTest("CWaveletImp::Decompress (the Blit entry)", decoder_ppc, decoder_host, edge=blit_edges, rnd=blit_random),
]
