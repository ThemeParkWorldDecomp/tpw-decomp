"""Unit w23: the bullfrog library's zlib unpacker and semaphore lock, the particle system's start-up (PAR_InitParticleSystem,
PAR_ResetParticleSystem, PAR_LoadLibrary and their helpers), the lobby start's small leaves (the theme texture folders, the
authorised user, the debug font, the string heap) and the ltms font render parameters, against
libtpw_unit_ParticlesInit.dylib (tests/unit_ParticlesInit.cpp).

The functions live in three programs of the original (the main app, the bullfrog library, ltms); each test runs on the
emulator of its own (OnProgram below).

The zlib unpacker is tested on the bullfrog library's program with the game's own zlib running in a second emulator
(tests/zlib_bridge.py): LbZLib_MemDecompress and bfl::compress::CZLib::MemDecompress on REAL "BILZ" blocks of the
version 0x12 wavelet textures read at run time from ROOT/extracted/wads (never copied into the repository; a missing
folder skips those cases with a message) and on blocks the test makes with Python's zlib: wrong tags, truncated
streams, damaged streams, wrong sizes. The host side inflates with the host's zlib (1.2.x against the game's 1.1.3):
the destination and the answer must agree; the workspace's contents are zlib's own state and are not compared.

The particle code is compared through everything it can touch: the state, the library, the sine table, the loader's two
words, the pool, with the callees (allocation, the file calls, the byte swap, the assertion) faked on both sides and
logged. PAR_LoadLibrary reads the REAL ROOT/extracted/.../Data/Particle/Tp2.plb at run time (never copied) and a long list of
files the test makes. The unit library swaps the library's fields like the Mac does, so that its bytes are the original's.
"""
import ctypes as C
import os
import pathlib
import struct
import zlib

import calllog as CL
import tpwtest as T
import zlib_bridge as ZB
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParticlesInit"

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
DUMPS = pathlib.Path(os.environ.get("TPW_DUMPS", ROOT / "ghidra/dumps"))
WADS = ROOT / "extracted" / "wads"
MASK = 0xFFFFFFFF
BULLFROG = "bullfrog symbols shared"
MAIN = "SimTheme Park"
LTMS = "ltms symbols shared"


class OnProgram(FnTest):
    """A free-form test that runs on the emulator of `program` (the module's units live in three programs)."""

    def __init__(self, program, name, ppc, host, edge=None, rnd=None, exact_nan=False):
        super().__init__(name, ppc, host, edge, rnd, exact_nan)
        self.program = program

    def run_case(self, runner, case):
        runner.use(self.program)
        return super().run_case(runner, case)


def once(emu, key, action):
    """Runs `action(emu)` the first time this emulator sees `key` (one-time stubs)."""
    done = emu.__dict__.setdefault("w23_done", set())
    if key not in done:
        done.add(key)
        action(emu)


def hostfn(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


# ----------------------------------------------------------------- the zlib unpacker

LB_ZLIB = "LbZLib_MemDecompress__FPvPvPv"
CZ_DECOMPRESS = "MemDecompress__Q33bfl8compress5CZLibCFPUcPUcPUc"
CZ_SIZE = "CompressWorkspaceSize__Q33bfl8compress5CZLibCFv"
SWAP_HEADER = "SwapHeader__FPQ29@unnamed@16_ZLibComp_Header"
TAG, MARKER = 0x5A4C4942, 0x53574150
MARGIN = 64
WORKSPACE = 0x62A39 + 64


def bilz(payload, level=9, bits=15, tag=TAG, size=None, packed=None):
    """A block as the files store it: the 0x1c-byte header, then the zlib stream."""
    stream = zlib.compress(payload, level) if bits == 15 else _deflate(payload, level, bits)
    header = struct.pack("<7I", tag, len(payload) if size is None else size,
                         len(stream) if packed is None else packed, bits & MASK, level, 0, 0)
    return header + stream


def _deflate(payload, level, bits):
    co = zlib.compressobj(level, zlib.DEFLATED, bits)
    return co.compress(payload) + co.flush()


class Case(dict):
    """A zlib case; it prints as its label."""

    def __repr__(self):
        return f"<{self['label']}>"


def zcase(label, block, dest_size, pad=b"\0" * 256, workspace=0, twice=False, refuse=False, loose=False):
    """block: header and stream as the file has them; pad: what follows the block in memory (a truncated stream reads
    on into it); dest_size: room for the destination (a margin of MARGIN bytes follows it); workspace: 0 for NULL (the
    unpacker makes its own), else the size of the block handed in; twice: the unpacker runs again on the same block;
    refuse: operator new[] refuses the block it is asked for; loose: the stream is cut or damaged, so that what the
    unpacker reads is garbage: the game's zlib 1.1.3 does not look for a copy that reaches back before the start of the output
    (it copies the old bytes of its window, whatever the workspace held) and zlib 1.2 stops there, so the two leave
    different parts of the destination written; only the answer, the header and the bytes beyond the destination's
    size field (which no inflate may touch) are compared."""
    return Case(label=label, block=bytes(block), pad=bytes(pad), dest_size=dest_size, workspace=workspace,
                twice=twice, refuse=refuse, loose=loose)


def header_words(raw, swapped_big_endian):
    """The seven header words: the PowerPC side's block holds the first five and the last big-endian once SwapHeader has
    run (marker in the last word; the sixth word is not swapped) and as the file has them (little-endian) otherwise;
    the host's always as the file has them."""
    if swapped_big_endian and struct.unpack(">I", raw[0x18:0x1C])[0] == MARKER:
        return list(struct.unpack(">5I", raw[:0x14])) + list(struct.unpack("<I", raw[0x14:0x18])) + \
            list(struct.unpack(">I", raw[0x18:0x1C]))
    return list(struct.unpack("<7I", raw[:0x1C]))


def z_stubs(e, case):
    """The imports of the bullfrog library the unpacker reaches: operator new[] and delete[] (recorded, labelled), the
    static object's registration (nothing), and the three zlib calls (the game's zlib in the second emulator)."""
    ZB.install(e, DUMPS)
    st = CL.Stubs()
    st.imp("__register_global_object", lambda e: None)

    def new_array(e):
        size = e.r(3)
        refuse = case["refuse"] or size >= 0x40000000
        p = 0 if refuse else e.alloc(size, b"\xa5" * size)
        e.cl.rec("new[]", (size, e.cl.newlab(p) if p else 0))
        e.set_r(3, p)

    def delete_array(e):
        p = e.r(3)
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)

    st.imp("__nwa__FUl", new_array)
    st.imp("__dla__FPv", delete_array)
    return st


def z_prepare(e, case):
    """Builds the case in the emulator: the block (with its pad), the destination (with its margin) and the workspace."""
    data = case["block"] + case["pad"]
    block = e.alloc(len(data), data)
    total = case["dest_size"] + MARGIN
    dest = e.alloc(total, b"\xa5" * total)
    workspace = e.alloc(case["workspace"], b"\x5a" * case["workspace"]) if case["workspace"] else 0
    return block, dest, workspace


def z_dest(case, raw):
    """The destination bytes a case compares: all of them, or for a loose case only those beyond the size field."""
    if not case["loose"]:
        return raw
    size_field = struct.unpack("<I", (case["block"] + bytes(8))[4:8])[0]
    return raw[min(size_field, case["dest_size"]):]


def z_observe_ppc(e, case, block, dest, results):
    out = {"results": results, "dest": z_dest(case, e.read(dest, case["dest_size"] + MARGIN)),
           "header": header_words(e.read(block, 0x1C), True),
           "swapped": struct.unpack(">I", e.read(block + 0x18, 4))[0] == MARKER,
           "stream": e.read(block + 0x1C, len(case["block"]) - 0x1C + len(case["pad"])),
           "log": e.cl.log}
    return out


def lbzlib_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    e.cl = CL.PpcLog({})
    st = z_stubs(e, case)
    block, dest, workspace = z_prepare(e, case)
    old = st.activate(e)
    try:
        results = [T.call_ppc(e, e.func(LB_ZLIB), [block, dest, workspace])[0]]
        if case["twice"]:
            results.append(T.call_ppc(e, e.func(LB_ZLIB), [block, dest, workspace])[0])
        return z_observe_ppc(e, case, block, dest, results)
    finally:
        CL.Stubs.deactivate(e, old)


def czlib_ppc(e, case):
    """CZLib::MemDecompress itself, on an object whose vtable slot 0x14 is the real CompressWorkspaceSize."""
    once(e, "std", T.install_std_stubs)
    e.cl = CL.PpcLog({})
    st = z_stubs(e, case)
    block, dest, workspace = z_prepare(e, case)
    entry = e.alloc(8, struct.pack(">II", e.func(CZ_SIZE), e.toc))
    vtable = e.alloc(0x30, b"\0" * 0x14 + struct.pack(">I", entry) + b"\0" * 0x18)
    this = e.alloc(8, struct.pack(">I", vtable))
    old = st.activate(e)
    try:
        results = [T.call_ppc(e, e.func(CZ_DECOMPRESS), [this, dest, block, workspace])[0]]
        if case["twice"]:
            results.append(T.call_ppc(e, e.func(CZ_DECOMPRESS), [this, dest, block, workspace])[0])
        return z_observe_ppc(e, case, block, dest, results)
    finally:
        CL.Stubs.deactivate(e, old)


def z_host(lib, case, symbol):
    data = case["block"] + case["pad"]
    block = C.create_string_buffer(data, len(data))
    total = case["dest_size"] + MARGIN
    dest = C.create_string_buffer(b"\xa5" * total, total)
    workspace = C.create_string_buffer(b"\x5a" * case["workspace"], case["workspace"]) if case["workspace"] else None
    CL.host_begin(lib, {}, {"array failure": [1] if case["refuse"] else []})
    if symbol == "tpw_pi_lbzlib":
        fn = hostfn(lib, symbol, C.c_uint32, C.c_void_p, C.c_void_p, C.c_void_p)
        args = [block, dest, workspace]
    else:
        fn = hostfn(lib, symbol, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p)
        args = [dest, block, workspace]
    results = [fn(*args) & MASK]
    if case["twice"]:
        results.append(fn(*args) & MASK)
    raw = block.raw
    return {"results": results, "dest": z_dest(case, dest.raw[:total]),
            "header": header_words(raw, False),
            "swapped": struct.unpack("<I", raw[0x18:0x1C])[0] == MARKER,
            "stream": raw[0x1C:0x1C + len(case["block"]) - 0x1C + len(case["pad"])],
            "log": CL.host_end(lib)}


def lbzlib_host(lib, case):
    return z_host(lib, case, "tpw_pi_lbzlib")


def czlib_host(lib, case):
    return z_host(lib, case, "tpw_pi_czlib")


# ---- the real blocks

_real = None
_missing = []


def real_blocks():
    """(label, block) of the "BILZ" blocks of version 0x12 wavelet textures under ROOT/extracted/wads: at least one of
    every unpacked size there is, colour blocks and alpha blocks. A .wct file is a 0x1c-byte header (version,
    kind at byte 2, width and height halfwords at 4 and 6, the colour and alpha byte counts at 16 and 20) followed by
    the colour block and, for kind 0x20, the alpha block."""
    global _real
    if _real is not None:
        return _real
    found = []
    if not WADS.is_dir():
        _missing.append(str(WADS))
    else:
        for folder, _, names in os.walk(WADS):
            for name in sorted(names):
                if not name.lower().endswith(".wct"):
                    continue
                path = pathlib.Path(folder) / name
                raw = path.read_bytes()
                if len(raw) < 0x1C or raw[0] != 0x12:
                    continue
                width, height = struct.unpack("<HH", raw[4:8])
                data_size, alpha_size = struct.unpack("<II", raw[16:24])
                relative = path.relative_to(WADS).as_posix()
                blocks = [("colour", raw[0x1C:0x1C + data_size])]
                if raw[2] == 0x20:
                    blocks.append(("alpha", raw[0x1C + data_size:0x1C + data_size + alpha_size]))
                for kind, block in blocks:
                    if block[:4] == b"BILZ":
                        found.append((struct.unpack("<I", block[4:8])[0], f"{relative} {kind} {width}x{height}", block))
    found.sort(key=lambda item: (item[0], item[1]))
    # one block of every unpacked size there is (120 of them, from 96 bytes to 32 KB), and for each size one more of the
    # other kind (colour or alpha) or, if there is none, the one with the longest stream
    _real = []
    by_size = {}
    for size, label, block in found:
        by_size.setdefault(size, []).append((label, block))
    for size in sorted(by_size):
        blocks = by_size[size]
        _real.append(blocks[0])
        others = [x for x in blocks[1:] if x[0].split()[1] != blocks[0][0].split()[1]] or blocks[1:]
        if others:
            _real.append(max(others, key=lambda x: len(x[1])))
    return _real


def real_cases():
    out = []
    for i, (label, block) in enumerate(real_blocks()):
        size = struct.unpack("<I", block[4:8])[0]
        out.append(zcase(label, block, size, pad=bytes((i * 7 + k) & 0xFF for k in range(256)),
                         workspace=WORKSPACE if i % 3 == 1 else 0, twice=(i % 5 == 2)))
    if _missing:
        print(f"test_ParticlesInit: {_missing[0]} is missing; the cases on real blocks are skipped")
    return out


# ---- blocks the test makes

def payload_for(r, size):
    """Bytes that compress in the ways real textures do: runs, ramps and noise."""
    out = bytearray()
    while len(out) < size:
        kind = r.randrange(4)
        n = r.randint(1, 3000)
        if kind == 0:
            out += bytes([r.randrange(256)]) * n
        elif kind == 1:
            start = r.randrange(256)
            out += bytes((start + k) & 0xFF for k in range(n))
        elif kind == 2:
            out += bytes(r.randrange(256) for _ in range(min(n, 400)))
        else:
            out += bytes(r.choice((0, 0, 0, 1, 255)) for _ in range(n))
    return bytes(out[:size])


def synthetic_cases(r, count):
    """Good blocks of many sizes and levels, then the damaged ones."""
    out = []
    sizes = [0, 1, 2, 7, 64, 255, 256, 1000, 4096, 16384, 32767, 32768, 32769, 65535, 65536, 70000, 100000]
    for k, size in enumerate(sizes):
        payload = payload_for(r, size)
        for level in (0, 1, 9):
            if size > 40000 and level != 9:
                continue
            out.append(zcase(f"good {size} level {level}", bilz(payload, level), size,
                             workspace=WORKSPACE if (k + level) % 2 else 0))
    payload = payload_for(r, 3000)
    good = bilz(payload)
    stream = good[0x1C:]
    pads = {"zeros": b"\0" * 512, "ones": b"\xff" * 512, "noise": bytes(r.randrange(256) for _ in range(512)),
            "ramp": bytes(range(256)) * 2}
    # the tag: every byte changed, zero, the byte-swapped tag, and a good tag with a wrong size field
    for n in range(4):
        bad = bytearray(good)
        bad[n] ^= 0x5A
        out.append(zcase(f"tag byte {n} changed", bad, len(payload)))
    out.append(zcase("tag zero", bytes(4) + good[4:], len(payload)))
    out.append(zcase("tag swapped", good[:4][::-1] + good[4:], len(payload)))
    out.append(zcase("tag good, pad refused", good, len(payload), workspace=0, refuse=True))
    out.append(zcase("empty block", b"", 16))
    out.append(zcase("header only", good[:0x1C], 16))
    out.append(zcase("zero length stream", bilz(b""), 0))
    out.append(zcase("zero length, size field 100", bilz(b"", size=100), 100))
    # the size field
    for delta in (-1, 1, -100, 500, -3000):
        size = max(0, len(payload) + delta)
        out.append(zcase(f"size field {size} for {len(payload)}", bilz(payload, size=size), max(size, len(payload))))
    out.append(zcase("size field huge", bilz(payload, size=0xFFFFFFF0), len(payload)))
    out.append(zcase("size field 0", bilz(payload, size=0), len(payload)))
    # the window bits: smaller than the stream needs, the same, larger, raw is not used by the game
    big = payload_for(r, 60000)
    for bits in (15, 14, 13, 12, 10, 9, 8):
        out.append(zcase(f"stream packed with {bits} bits", bilz(big, 9, bits), len(big)))
        out.append(zcase(f"stream packed with {bits} bits, header says 15", _with_bits(bilz(big, 9, bits), 15), len(big)))
        out.append(zcase(f"stream packed with 15 bits, header says {bits}", _with_bits(bilz(big, 9, 15), bits), len(big)))
    # window bits the header may hold: zlib 1.1.3 takes 8 to 15 (zlib streams) and -8 to -15 (raw streams) only
    for value in (0, 1, 7, 16, 17, 24, 31, 32, 40, 47, 48, 63, 64, 0x80000000, 0xFFFFFFFF, 0xFFFFFFF9, 0x7FFFFFFF):
        out.append(zcase(f"header window bits {value:#x}", _with_bits(good, value), len(payload)))
    for bits in (-15, -14, -12, -9):
        out.append(zcase(f"raw stream, {bits} bits", bilz(big, 9, bits), len(big)))
        out.append(zcase(f"raw stream, header says 15", _with_bits(bilz(big, 9, bits), 15), len(big), loose=True))
        out.append(zcase(f"zlib stream, header says {bits}", _with_bits(bilz(big, 9, 15), bits), len(big), loose=True))
    for value in (0xFFFFFFF8, 0xFFFFFFF7, 0xFFFFFFF1, 0xFFFFFFF0, 0x80000000):
        out.append(zcase(f"raw stream, header {value:#x}", _with_bits(bilz(big, 9, -15), value), len(big), loose=True))
    # truncated streams read on into the pad
    for name, pad in pads.items():
        for cut in (0, 1, 2, 3, 10, len(stream) // 2, len(stream) - 5, len(stream) - 4, len(stream) - 1):
            out.append(zcase(f"stream cut to {cut}, pad {name}", good[:0x1C + cut], len(payload), pad=pad,
                             loose=name in ("ones", "noise", "ramp")))
    # damaged streams
    for n in (2, 3, 5, 20, len(stream) // 2, len(stream) - 6, len(stream) - 5, len(stream) - 4, len(stream) - 1):
        if 0 <= n < len(stream):
            for bit in (0x01, 0x80):
                bad = bytearray(good)
                bad[0x1C + n] ^= bit
                out.append(zcase(f"stream byte {n} xor {bit:#x}", bad, len(payload), loose=True))
    out.append(zcase("stream followed by more stream", good + good[0x1C:], len(payload)))
    out.append(zcase("two blocks back to back", good + good, len(payload)))
    # the second call on the same block (the swap marker is there), with and without a workspace
    out.append(zcase("twice", good, len(payload), twice=True))
    out.append(zcase("twice with workspace", good, len(payload), twice=True, workspace=WORKSPACE))
    out.append(zcase("twice, bad tag", bytes(4) + good[4:], len(payload), twice=True))
    # workspace of the size the decompressor needs (the Mac's DecompressWorkspaceSize is 0x15388)
    out.append(zcase("workspace 0x15388", good, len(payload), workspace=0x15388))
    out.append(zcase("big with workspace 0x15388", bilz(big), len(big), workspace=0x15388))
    for _ in range(count):
        out.append(random_zcase(r))
    return out


PADS = [b"\0" * 512, b"\xff" * 512, bytes(range(256)) * 2]


def random_zcase(r):
    """A block of a random size, damaged in one of eight ways (or not at all)."""
    size = r.choice((0, 1, r.randrange(1, 300), r.randrange(300, 6000), r.randrange(6000, 40000)))
    data = payload_for(r, size)
    block = bytearray(bilz(data, r.choice((0, 1, 6, 9))))
    what = r.randrange(8)
    loose = False
    if what == 1 and len(block) > 0x1C:
        block = block[:0x1C + r.randrange(len(block) - 0x1C)]
        loose = True
    elif what == 2 and len(block) > 0x1C:
        block[0x1C + r.randrange(len(block) - 0x1C)] ^= 1 << r.randrange(8)
        loose = True
    elif what == 3:
        block[r.randrange(4)] ^= 1 << r.randrange(8)
    elif what == 4:
        block[4:8] = struct.pack("<I", max(0, size + r.choice((-2, -1, 1, 2, 1000))) & MASK)
    pad = bytes(r.randrange(256) for _ in range(512)) if r.random() < 0.3 else r.choice(PADS)
    return zcase(f"random {size} {what}", block, size + (1000 if what == 4 else 0), pad=pad,
                 workspace=WORKSPACE if r.random() < 0.4 else 0, twice=r.random() < 0.15, loose=loose)


def _with_bits(block, bits):
    return block[:12] + struct.pack("<I", bits & MASK) + block[16:]


_synthetic = None


def zlib_edges():
    global _synthetic
    import random
    if _synthetic is None:
        _synthetic = synthetic_cases(random.Random(23), 0)
    return real_cases() + _synthetic


def zlib_random(r):
    return random_zcase(r)


# ---- SwapHeader, CompressWorkspaceSize

def swap_ppc(e, case):
    """case: (words, premarked). A block that has been swapped already holds its words big-endian on the PowerPC side
    (little-endian on the host's) and the marker in the last word; the sixth word is never swapped. A block the file
    just delivered holds them as the file has them on both."""
    once(e, "std", T.install_std_stubs)
    words, premarked = case
    if premarked:
        image = struct.pack(">5I", *words[:5]) + struct.pack("<I", words[5]) + struct.pack(">I", MARKER)
    else:
        image = struct.pack("<7I", *words)
    block = e.alloc(0x20, image + b"\xa5" * 4)
    T.call_ppc(e, e.func(SWAP_HEADER), [block])
    raw = e.read(block, 0x20)
    swapped = struct.unpack(">I", raw[0x18:0x1C])[0] == MARKER
    return {"words": header_words(raw, True), "swapped": swapped, "after": raw[0x1C:]}


def swap_host(lib, case):
    words, premarked = case
    image = struct.pack("<7I", *words[:6], MARKER) if premarked else struct.pack("<7I", *words)
    block = C.create_string_buffer(image + b"\xa5" * 4, 0x20)
    hostfn(lib, "tpw_pi_swapheader", None, C.c_void_p)(block)
    raw = block.raw
    return {"words": header_words(raw, False), "swapped": struct.unpack("<I", raw[0x18:0x1C])[0] == MARKER,
            "after": raw[0x1C:]}


def swap_edges():
    base = [TAG, 100, 60, 15, 9, 0, 0]
    words = [base, [0] * 7, [MASK] * 7, [x ^ 0x01020304 for x in base]]
    for n in range(7):
        w = list(base)
        w[n] = 0x80000000
        words.append(w)
    return [(w, flag) for w in words for flag in (False, True)]


def swap_random(r):
    return ([r.getrandbits(32) for _ in range(7)], r.random() < 0.3)


def size_ppc(e, case):
    return {"size": T.call_ppc(e, e.func(CZ_SIZE), [e.alloc(8)])[0]}


def size_host(lib, case):
    return {"size": hostfn(lib, "tpw_pi_czlib_size", C.c_int32)() & MASK}

# ----------------------------------------------------------------- the particle system's start-up (main app)

# The run of globals from the sine table to the end of the state (0x102e49f4 to 0x102f7bd0, see include/rsse.h).
SIN_TABLE, STATE, STATE_SIZE, LIB, LIB_SIZE = 0x102E49F4, 0x102EDD68, 0x9E68, 0x102E5208, 0x8B60
W_B, W_A, RUNNING, BUSY, DRAWLIST = 0x102E51F4, 0x102E51F8, 0x102E51FC, 0x102E5200, 0x102E5204
PARTICLE_BASE, PARTICLE_SIZE = SIN_TABLE, STATE + STATE_SIZE - SIN_TABLE
DUMMY_TV = 0x10266920                     # the transition vector of DummyHeightCallBack (the default height callback)
ASSERT_FN, OPEN_FN = 0x1000DDE8, 0x10003FD4
PAR_INIT, PAR_RESET, PAR_LOAD = 0x100BDC24, 0x100BDD64, 0x100BA3E0
PAR_SIN, PAR_MALLOC, PAR_LOWSPEC, DUMMY_FN = 0x100BA324, 0x100BABF8, 0x100BE03C, 0x100BDBF0
SW16S, SW16U, SW32, SWBLOCK = 0x100BABB4, 0x100BAB6C, 0x100BAB28, 0x100BAACC
PAR_REAL = {PAR_INIT, PAR_RESET, PAR_LOAD, PAR_SIN, PAR_MALLOC, PAR_LOWSPEC, DUMMY_FN, SW16S, SW16U, SW32, SWBLOCK}
LABEL_FILE, LABEL_OWNER, LABEL_POOL = 5, 7, 9
MARGIN_POOL = 0x100            # bytes of 0xcc kept before and after a pool the case makes (the originals write outside it)
HEADER_POKES = {0: (0x00, ">i"), 1: (0x04, ">i"), 2: (0x08, ">i"), 4: (0x10, ">i"), 5: (0x14, ">i"), 6: (0x18, ">h"),
                7: (0x1A, ">h"), 9: (0x20, ">i")}
PLB = ROOT / "extracted" / "disc-hfs" / "Theme Park Data" / "data" / "Particle" / "Tp2.plb"
SA2_LITTLE, SA2_BIG = b"\xe8\x03", b"\x03\xe8"     # 1000 written by the translation (host order) and by the original


class PCase(dict):
    """A particle-system case; it prints as its label."""

    def __repr__(self):
        return f"<{self['label']}>"


def pcase(label, op, **kw):
    """op: init, reset, load, sin, malloc, lowspec, height. fill: the byte every global of the particle system holds before
    the call; running, maximum, word_a, word_b: the running flag, the pool's length and the library's two words before;
    pool: 'own' (a pool of the case with margins), 'state' (the pool the call makes) or None; pokes: header words to set
    (HEADER_POKES); args: the call's arguments; queues: the fakes' result queues; file, path: the library file."""
    case = dict(label=label, op=op, fill=0xA5, running=1, maximum=100, word_a=0xA5A5A5A5, word_b=0xA5A5A5A5, pool=None,
                pokes={}, args=(), queues={}, file=b"", path="Data:Particle:Tp2.plb", owner=False, void=False,
                margin=0)
    case.update(kw)
    return PCase(case)


def le16_to_be(raw):
    """Pairs of bytes reversed: a run of 16-bit fields of the host, in the PowerPC side's order."""
    out = bytearray(raw)
    out[0::2], out[1::2] = raw[1::2], raw[0::2]
    return bytes(out)


def le32_to_be(raw):
    out = bytearray(raw)
    out[0::4], out[1::4], out[2::4], out[3::4] = raw[3::4], raw[2::4], raw[1::4], raw[0::4]
    return bytes(out)


def fix_library(raw):
    """The host's library bytes in the original's order: the swapped fields are big-endian (the unit library swaps like the
    Mac) but the 1000 that the translation writes into a zero 0xa2 halfword is in the host's own order."""
    out = bytearray(raw)
    for i in range(0x69):
        at = i * 0x140 + 0xA2
        if out[at:at + 2] == SA2_LITTLE:
            out[at:at + 2] = SA2_BIG
    return bytes(out)


# ---- the PowerPC side

def particle_stubs(e, case):
    """The callees of the particle code, recorded in the order they are called (the host's fakes record the same)."""
    st = CL.Stubs()
    st.real = set(PAR_REAL)
    margin = case["margin"]

    def mem_alloc(e):
        L, size = e.cl, e.r(3)
        refuse = L.ret("LbMem_Alloc fails") != 0 or size >= 0x40000000
        if size >= 0x200000:        # not made (nothing touches it): an address of its own
            L.big = getattr(L, "big", 0) + 1
            block = 0 if refuse else 0x6F000000 + 0x1000 * L.big
        else:
            block = 0 if refuse else e.alloc(max(size, 1), b"\xa5" * max(size, 1))
        L.rec("LbMem_Alloc", (size, L.newlab(block) if block else 0))
        e.set_r(3, block)

    def new_array(e):
        L, size = e.cl, e.r(3)
        refuse = L.ret("array failure") != 0 or size >= 0x40000000
        total = max(size + margin, 1)
        block = 0 if refuse else e.alloc(total, b"\xa5" * total)
        L.rec("new[]", (size, L.newlab(block) if block else 0))
        e.set_r(3, block)

    def game_assert(e):
        e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def tp_open(e):
        L = e.cl
        L.rec("TPFile_Open", (), (e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159]))
        L.position = 0
        e.set_r(3, e.w23_file if L.ret("TPFile_Open", 1) != 0 else 0)

    def lb_read(e):
        # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done. A short read hands over
        # what there is and zeroes the rest of the buffer (the original's locals are not set when the file is short).
        L, buffer, length = e.cl, e.r(5), e.r(6)
        L.rec("LbFile_Read", (L.lab(e.r(4)), length))
        chunk = L.file[L.position:L.position + length]
        L.position += len(chunk)
        if length:
            e.write(buffer, chunk + bytes(length - len(chunk)))
        if e.r(7):
            e.write(e.r(7), struct.pack(">I", len(chunk)))
        e.write(e.r(3), bytes(4))

    def lb_seek(e):
        L, offset = e.cl, T.s32(e.r(4))
        L.rec("LbFile_Seek", (L.lab(e.r(3)), offset, e.r(5)))
        L.position = max(0, min(len(L.file), L.position + offset))
        e.set_r(3, 0)

    def lb_close(e):
        e.cl.rec("LbFile_Close", (e.cl.lab(e.r(4)),))
        e.write(e.r(3), bytes(4))

    def swap_block(e):
        # the real swap on every buffer (the PowerPC side is big-endian); the event says whether it is in the library
        a, n = e.r(3), T.s32(e.r(4))
        inside = LIB <= a < LIB + LIB_SIZE
        e.cl.rec("SwapBlock32", (n, 1 if inside else 0))
        for i in range(max(n, 0)):
            e.write(a + 4 * i, e.read(a + 4 * i, 4)[::-1])

    st.imp("LbMem_Alloc__FUi", mem_alloc)
    st.imp("__nwa__FUl", new_array)
    st.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    st.imp("LbFile_Seek__FPvl14TbFileSeekMode", lb_seek)
    st.imp("LbFile_Close__FPv", lb_close)
    st.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap_block)
    st.internal(ASSERT_FN, game_assert)
    st.internal(OPEN_FN, tp_open)
    return st


def prepare_ppc(e, case, pool_address):
    e.write(STATE, bytes([case["fill"]]) * STATE_SIZE)
    e.write(LIB, bytes([case["fill"]]) * LIB_SIZE)
    e.write(SIN_TABLE, bytes([case["fill"]]) * 0x800)
    e.write(RUNNING, struct.pack(">i", case["running"]))
    e.write(BUSY, struct.pack(">i", case["fill"]))
    e.write(DRAWLIST, bytes(4))
    e.write(W_A, struct.pack(">I", case["word_a"]))
    e.write(W_B, struct.pack(">I", case["word_b"]))
    e.write(STATE + 0x0C, struct.pack(">i", T.s32(case["maximum"])))
    e.write(STATE + 0x28, struct.pack(">I", pool_address))
    for index, value in case["pokes"].items():
        offset, fmt = HEADER_POKES[index]
        e.write(STATE + offset, struct.pack(fmt, T.s16(value) if fmt == ">h" else T.s32(value)))


def snapshot_ppc(e, case, pool_address):
    """Everything the call may have touched, as numbers and bytes in one shape for both sides (16-bit fields as big-endian)."""
    L = e.cl

    def s32(a):
        return struct.unpack(">i", e.read(a, 4))[0]

    def s16(a):
        return struct.unpack(">h", e.read(a, 2))[0]

    sines, callback, pool = e.u32(STATE + 0x1C), e.u32(STATE + 0x24), e.u32(STATE + 0x28)
    out = {
        "header": [s32(STATE), s32(STATE + 4), s32(STATE + 8), s32(STATE + 0xC), s32(STATE + 0x10), s32(STATE + 0x14),
                   s16(STATE + 0x18), s16(STATE + 0x1A), 1 if sines == SIN_TABLE else 0 if sines == 0 else 2,
                   s32(STATE + 0x20),
                   0 if callback == 0 else 1 if L.lab(callback) == LABEL_OWNER else 2 if callback == DUMMY_TV else 3,
                   L.lab(pool)],
        "globals": [s32(RUNNING), s32(BUSY), L.lab(e.u32(DRAWLIST)), e.u32(W_A), e.u32(W_B)],
        "emitters": e.read(STATE + 0x2C, 0x9600),
        "effecters": e.read(STATE + 0x962C, 0x820),
        "heads": e.read(STATE + 0x9E4C, 0x1C),
        "library": e.read(LIB, LIB_SIZE),
        "sines": e.read(SIN_TABLE, 0x800),
    }
    if case["pool"] == "own":
        size = max(T.s32(case["maximum"]), 0) * 0x34
        out["pool"] = e.read(pool_address - MARGIN_POOL, 2 * MARGIN_POOL + size)
    elif case["pool"] == "state" and pool != 0 and s32(STATE + 0xC) > 0:
        out["pool"] = e.read(pool, s32(STATE + 0xC) * 0x34)
    return out


def particle_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    if not hasattr(e, "w23_file"):
        e.w23_file = T.persist_alloc(e, 16)
    saved = e.read(PARTICLE_BASE, PARTICLE_SIZE)
    try:
        owner = e.alloc(16) if case["owner"] else 0
        pool_address = 0
        if case["pool"] == "own":
            size = max(T.s32(case["maximum"]), 0) * 0x34
            region = e.alloc(2 * MARGIN_POOL + size, b"\xcc" * (2 * MARGIN_POOL + size))
            pool_address = region + MARGIN_POOL
        fixed = {e.w23_file: LABEL_FILE}
        if owner:
            fixed[owner] = LABEL_OWNER
        if pool_address:
            fixed[pool_address] = LABEL_POOL
        e.cl = CL.PpcLog(fixed, case["queues"])
        e.cl.file, e.cl.position = case["file"], 0
        prepare_ppc(e, case, pool_address)
        st = particle_stubs(e, case)
        old = st.activate(e)
        try:
            result = run_op_ppc(e, case, owner)
        finally:
            CL.Stubs.deactivate(e, old)
        out = snapshot_ppc(e, case, pool_address)
        out.update(result=result, log=e.cl.log)
        return out
    finally:
        e.write(PARTICLE_BASE, saved)


def run_op_ppc(e, case, owner):
    op, args = case["op"], case["args"]
    if op == "init":
        T.call_ppc(e, PAR_INIT, [owner, T.u32(args[0]), T.u32(args[1])])
        return None
    if op == "reset":
        T.call_ppc(e, PAR_RESET, [])
        return None
    if op == "load":
        path = e.alloc(len(case["path"]) + 1, case["path"].encode("latin-1") + b"\0") if case["path"] is not None else 0
        r3 = T.call_ppc(e, PAR_LOAD, [path])[0]
        return None if case["void"] else T.s32(r3)
    if op == "sin":
        return T.call_ppc(e, PAR_SIN, [])[0] == SIN_TABLE
    if op == "malloc":
        return e.cl.lab(T.call_ppc(e, PAR_MALLOC, [T.u32(args[0])])[0])
    if op == "lowspec":
        T.call_ppc(e, PAR_LOWSPEC, [T.u32(args[0])])
        return None
    if op == "height":
        return T.s32(T.call_ppc(e, DUMMY_FN, [T.u32(args[0]), T.u32(args[1])])[0])
    raise ValueError(op)


# ---- the host side

def bind_particles(lib):
    if getattr(lib, "pi_bound", False):
        return
    lib.pi_bound = True
    c32, vp = C.c_int32, C.c_void_p
    hostfn(lib, "tpw_pi_prepare", None, c32, c32, c32, vp, C.c_uint32, C.c_uint32)
    hostfn(lib, "tpw_pi_poke_header", None, c32, c32)
    hostfn(lib, "tpw_pi_set_file", None, C.c_char_p, c32)
    hostfn(lib, "tpw_pi_file_label", None)
    hostfn(lib, "tpw_pi_init", None, vp, c32, c32)
    hostfn(lib, "tpw_pi_reset", None)
    hostfn(lib, "tpw_pi_load", c32, C.c_char_p)
    hostfn(lib, "tpw_pi_load_void", None, C.c_char_p)
    hostfn(lib, "tpw_pi_lowspec", None, c32)
    hostfn(lib, "tpw_pi_malloc", vp, c32)
    hostfn(lib, "tpw_pi_sin", vp)
    hostfn(lib, "tpw_pi_sin_is_table", c32, vp)
    hostfn(lib, "tpw_pi_height", c32, c32, c32)
    hostfn(lib, "tpw_pi_header", None, vp)
    hostfn(lib, "tpw_pi_globals", None, vp)
    hostfn(lib, "tpw_pi_region", vp, c32, C.POINTER(c32))
    hostfn(lib, "tpw_pi_set_margin", None, c32)
    hostfn(lib, "tpw_cl_lab", c32, vp)


def host_region(lib, which):
    size = C.c_int32(0)
    address = lib.tpw_pi_region(which, C.byref(size))
    return address, size.value


def snapshot_host(lib, case, pool_buffer):
    header, globs = (C.c_int64 * 12)(), (C.c_int64 * 8)()
    lib.tpw_pi_header(header)
    lib.tpw_pi_globals(globs)

    def region(which):
        address, size = host_region(lib, which)
        return C.string_at(address, size) if address and size > 0 else b""

    out = {"header": list(header), "globals": list(globs)[:5],
           "emitters": le16_to_be(region(0)), "effecters": le16_to_be(region(1)), "heads": le16_to_be(region(2)),
           "library": fix_library(region(3)), "sines": le32_to_be(region(5))}
    if case["pool"] == "own":
        out["pool"] = le16_to_be(pool_buffer.raw)
    elif case["pool"] == "state":
        pool = region(4)
        if pool:
            out["pool"] = le16_to_be(pool)
    return out


def particle_host(lib, case):
    bind_particles(lib)
    owner = C.create_string_buffer(16)
    pool_buffer, pool_address = None, 0
    fixed = {}
    if case["owner"]:
        fixed[LABEL_OWNER] = C.addressof(owner)
    if case["pool"] == "own":
        total = 2 * MARGIN_POOL + max(T.s32(case["maximum"]), 0) * 0x34
        pool_buffer = C.create_string_buffer(b"\xcc" * total, total)
        pool_address = C.addressof(pool_buffer) + MARGIN_POOL
        fixed[LABEL_POOL] = pool_address
    lib.tpw_pi_prepare(case["fill"], case["running"], T.s32(case["maximum"]), pool_address, case["word_a"], case["word_b"])
    for index, value in case["pokes"].items():
        lib.tpw_pi_poke_header(index, T.s16(value) if HEADER_POKES[index][1] == ">h" else T.s32(value))
    lib.tpw_pi_set_file(case["file"], len(case["file"]))
    lib.tpw_pi_set_margin(case["margin"])
    CL.host_begin(lib, fixed, case["queues"])
    lib.tpw_pi_file_label()
    op, args = case["op"], case["args"]
    result = None
    if op == "init":
        lib.tpw_pi_init(C.addressof(owner) if case["owner"] else None, T.s32(args[0]), T.s32(args[1]))
    elif op == "reset":
        lib.tpw_pi_reset()
    elif op == "load":
        path = case["path"].encode("latin-1") if case["path"] is not None else None
        if case["void"]:
            lib.tpw_pi_load_void(path)
        else:
            result = lib.tpw_pi_load(path)
    elif op == "sin":
        result = bool(lib.tpw_pi_sin_is_table(lib.tpw_pi_sin()))
    elif op == "malloc":
        pointer = lib.tpw_pi_malloc(T.s32(args[0]))
        result = lib.tpw_cl_lab(pointer) if pointer else 0
    elif op == "lowspec":
        lib.tpw_pi_lowspec(T.s32(args[0]))
    elif op == "height":
        result = lib.tpw_pi_height(T.s32(args[0]), T.s32(args[1]))
    log = CL.host_end(lib)
    out = snapshot_host(lib, case, pool_buffer)
    out.update(result=result, log=log)
    lib.tpw_pi_set_margin(0)
    return out


# ---- cases of the particle functions

FILLS = (0x00, 0xA5, 0xFF)
WORD_EDGES = [0, 1, 5, 9, 10, 11, 33, 100, 499, 500, 501, 1000, 1024, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF]


def random_pokes(r):
    """Distinct values in some of the header words, so that a word the call leaves or sets shows."""
    return {i: T.rint(r) for i in HEADER_POKES if r.random() < 0.5}


def init_case(label, density, limit, **kw):
    """PAR_InitParticleSystem(owner, density, limit); the keyword maximum is the pool's length before the call."""
    kw.setdefault("pool", "state")
    return pcase(label, "init", args=(density, limit), **kw)


def init_edges():
    out = []
    for fill in FILLS:
        for owner in (False, True):
            out.append(init_case(f"init game call fill {fill:#x} owner {owner}", 1024, 0x800, fill=fill, running=0,
                                 owner=owner))
    for maximum in (0, 1, 99, 100, 101, 0x7FFE, 0x7FFF, 0x8000, 0x10000, 0x7FFFFFFF, -1, -100, -0x80000000, 5000):
        for owner in (False, True):
            out.append(init_case(f"init max {maximum}", 1024, maximum, running=0, owner=owner,
                                 maximum=0x1234 if owner else 0))
    for density in (0, 1, 2, -1, 1023, 1025, 0x7FFFFFFF, -0x80000000, 0x10000):
        out.append(init_case(f"init density {density}", density, 300, running=0, fill=0xFF))
    # running already: nothing changes, whatever the arguments
    for fill in FILLS:
        for owner in (False, True):
            out.append(init_case(f"init while running fill {fill:#x}", 1024, 0x800, fill=fill, running=1, owner=owner,
                                 maximum=77, pool=None, pokes={2: 5, 9: 6}))
    out.append(init_case("init while running (flag 2)", 1, 100, running=2, pool=None))
    out.append(init_case("init while running (flag -1)", 1, 100, running=-1, pool=None))
    # the drawing list's block is refused: its pointer stays NULL
    out.append(init_case("init, drawing list refused", 1024, 0x800, running=0, queues={"LbMem_Alloc fails": [0, 1]}))
    out.append(init_case("init, nothing refused", 1024, 0x800, running=0, queues={"LbMem_Alloc fails": [0, 0]}))
    return out


def init_random(r):
    maximum = r.choice((r.randint(-300, 600), r.randint(0, 0x8200), T.rint(r), r.randint(100, 400)))
    return init_case("random init", T.rint(r), maximum, fill=r.choice(FILLS), running=int(r.random() < 0.15) * r.choice((1, 1, 2, -1)),
                     owner=r.random() < 0.5, maximum=T.rint(r), pokes=random_pokes(r),
                     queues={"LbMem_Alloc fails": [0, 1]} if r.random() < 0.1 else {})


def reset_case(label, maximum, **kw):
    kw.setdefault("pool", "own")
    return pcase(label, "reset", maximum=maximum, **kw)


def reset_edges():
    out = []
    for fill in FILLS:
        for maximum in (1, 2, 3, 4, 5, 6, 50, 99, 100, 101, 300, 1000):
            out.append(reset_case(f"reset max {maximum} fill {fill:#x}", maximum, fill=fill))
    out.append(reset_case("reset max 0 (writes before the pool)", 0))
    out.append(reset_case("reset max 0 fill 0", 0, fill=0))
    for fill in FILLS:
        out.append(reset_case(f"reset not running fill {fill:#x}", 100, fill=fill, running=0))
    out.append(reset_case("reset running flag 2", 120, running=2))
    out.append(reset_case("reset running flag -1", 120, running=-1))
    out.append(reset_case("reset with pokes", 150, pokes={0: 1, 1: 2, 2: 3, 4: 4, 5: 5, 6: 6, 7: 7, 9: 8}))
    out.append(reset_case("reset the game's pool", 0x800))
    return out


def reset_random(r):
    return reset_case("random reset", r.choice((0, 1, 2, r.randint(1, 120), r.randint(100, 400))), fill=r.choice(FILLS),
                      running=int(r.random() < 0.9), pokes=random_pokes(r))


def plb_bytes(r, n_emit=105, n_eff=20, s_emit=0x140, s_eff=0x68, word_a=33, word_b=1024, zero_sa2=0.3):
    """A particle library as the file has it: little-endian counts and sizes, the records (random bytes, the 0xa2 halfword of
    an emitter zero now and then, never the pair that the translation's own 1000 would be mistaken for), the two words."""
    def emitter():
        rec = bytearray(r.randbytes(s_emit))
        if s_emit >= 0xA4:
            if r.random() < zero_sa2:
                rec[0xA2:0xA4] = b"\0\0"
            elif rec[0xA2:0xA4] == b"\x03\xe8":
                rec[0xA2] = 4
        return bytes(rec)

    out = struct.pack("<II", n_emit, s_emit) + b"".join(emitter() for _ in range(n_emit))
    out += struct.pack("<II", n_eff, s_eff) + r.randbytes(n_eff * s_eff)
    return out + struct.pack("<II", word_a, word_b)


def real_plb():
    return PLB.read_bytes() if PLB.is_file() else None


def load_case(label, data, maximum=100, **kw):
    kw.setdefault("pool", "own")
    return pcase(label, "load", file=data, maximum=maximum, **kw)


def load_edges():
    import random
    r = random.Random(2301)
    out = []
    real = real_plb()
    if real is None:
        print(f"test_ParticlesInit: {PLB} is missing; the cases on the real library are skipped")
    else:
        for i in range(0x69):                                   # the translation's 1000 must not look like a loaded one
            assert real[8 + i * 0x140 + 0xA2:8 + i * 0x140 + 0xA4] != b"\x03\xe8"
        for fill in FILLS:
            for maximum in (1, 100, 700):
                out.append(load_case(f"load Tp2.plb fill {fill:#x} max {maximum}", real, maximum, fill=fill,
                                     word_a=0xA5A5A5A5 if fill else 7, word_b=3))
        out.append(load_case("load Tp2.plb through the void form", real, void=True))
        out.append(load_case("load Tp2.plb, running flag 2", real, running=2))
        out.append(load_case("load Tp2.plb, library full of ones", real, fill=0xFF, word_a=0, word_b=0xFFFFFFFF))
        # cut short at every kind of place
        first, second = 8 + 105 * 0x140, 8 + 105 * 0x140 + 8 + 20 * 0x68
        for cut in (0, 1, 3, 4, 5, 7, 8, 9, 0x20, 8 + 0x140 * 3 + 5, first - 1, first, first + 3, first + 4, first + 7, first + 8,
                    first + 9, first + 8 + 0x68 * 7 + 1, second - 1, second, second + 3, second + 4, second + 5,
                    second + 7, len(real) - 1):
            out.append(load_case(f"load Tp2.plb cut at {cut}", real[:cut], fill=0xA5 if cut % 2 else 0x00))
    out.append(load_case("load: no path", b"", path=None))
    out.append(load_case("load: no path, not running", b"", path=None, running=0))
    out.append(load_case("load: not running (asserts)", b"abc", running=0, fill=0xFF))
    out.append(load_case("load: not running, other path", b"abc", running=0, path=""))
    out.append(load_case("load: cannot open", b"abc", queues={"TPFile_Open": [0]}))
    out.append(load_case("load: empty path", b"", path=""))
    # counts and sizes of the file
    for n_emit, n_eff in ((0, 0), (1, 1), (2, 3), (50, 10), (104, 19), (105, 20), (106, 20), (105, 21), (107, 22), (110, 25),
                          (120, 30), (130, 40)):
        out.append(load_case(f"load {n_emit} emitters {n_eff} effecters", plb_bytes(r, n_emit, n_eff), fill=0xA5 if n_emit % 2 else 0))
    for s_emit, s_eff in ((0, 0), (4, 4), (0x10, 0x10), (0xA2, 0x60), (0xA3, 0x61), (0xA4, 0x62), (0x100, 0x40), (0x13F, 0x67),
                          (0x141, 0x68), (0x150, 0x68), (0x120, 0x30)):
        n_emit = min(105, 0x8B60 // max(s_emit, 1), 105) if s_emit else 105
        n_eff = min(20, 0x820 // max(s_eff, 1)) if s_eff else 20
        out.append(load_case(f"load record sizes {s_emit:#x} and {s_eff:#x}", plb_bytes(r, n_emit, n_eff, s_emit, s_eff), fill=0xA5))
    out.append(load_case("load: skip past the end", plb_bytes(r, 200, 40, 0x10, 0x10)))
    # the library's two words
    for word_a in WORD_EDGES:
        out.append(load_case(f"load word {word_a:#x}", plb_bytes(r, 3, 2, word_a=word_a, word_b=word_a ^ 0x55)))
    return out


def load_random(r):
    n_emit = r.choice((105, 105, r.randint(0, 130)))
    n_eff = r.choice((20, 20, r.randint(0, 30)))
    s_emit = r.choice((0x140, 0x140, 0x140, r.randint(0, 339)))
    s_eff = r.choice((0x68, 0x68, 0x68, r.randint(0, 104)))
    shown_emit, shown_eff = min(n_emit, 105), min(n_eff, 20)
    if s_emit * shown_emit > LIB_SIZE:
        s_emit = LIB_SIZE // max(shown_emit, 1)
    if s_eff * shown_eff > 0x820:
        s_eff = 0x820 // max(shown_eff, 1)
    data = plb_bytes(r, n_emit, n_eff, s_emit, s_eff, r.choice(WORD_EDGES + [r.getrandbits(32)]), r.getrandbits(32))
    if r.random() < 0.1:
        data = data[:r.randrange(len(data) + 1)]
    return load_case("random load", data, r.randint(1, 300), fill=r.choice(FILLS), running=int(r.random() < 0.95),
                     path=r.choice(("Data:Particle:Tp2.plb", "x", "", "a:b:c.plb")), void=r.random() < 0.2,
                     word_a=r.getrandbits(32), word_b=r.getrandbits(32), pokes=random_pokes(r),
                     queues={"TPFile_Open": [0]} if r.random() < 0.05 else {})


def sin_edges():
    return [pcase(f"sine table fill {fill:#x}", "sin", fill=fill) for fill in FILLS]


def sin_random(r):
    return pcase("random sine table", "sin", fill=r.choice(FILLS), pokes=random_pokes(r))


MALLOC_SIZES = [0, 1, 2, 3, 4, 0x34, 100, 0xAF0, 0x1000, 0x7FFF * 0x34, 0x200000, 0x3FFFFFFF, 0x40000000, 0x7FFFFFFF,
                -0x80000000, -1, -2, -0x3FFFFFFF]


def malloc_edges():
    out = []
    for size in MALLOC_SIZES:
        out.append(pcase(f"malloc {size}", "malloc", args=(size,)))
        out.append(pcase(f"malloc {size}, refused", "malloc", args=(size,), queues={"LbMem_Alloc fails": [1]}))
    return out


def malloc_random(r):
    return pcase("random malloc", "malloc", args=(r.choice((T.rint(r), r.randint(0, 5000), r.choice(MALLOC_SIZES))),),
                 fill=r.choice(FILLS), queues={"LbMem_Alloc fails": [int(r.random() < 0.2)]})


def lowspec_edges():
    return [pcase(f"low spec {value}", "lowspec", args=(value,), fill=fill, pokes={2: 7})
            for value in (0, 1, -1, 2, 0x7FFFFFFF, -0x80000000) for fill in FILLS]


def lowspec_random(r):
    return pcase("random low spec", "lowspec", args=(T.rint(r),), fill=r.choice(FILLS), pokes=random_pokes(r))


def height_edges():
    return [pcase(f"height {x} {z}", "height", args=(x, z)) for x, z in ((0, 0), (1, 2), (-1, -2), (0x7FFFFFFF, -0x80000000))]


def height_random(r):
    return pcase("random height", "height", args=(T.rint(r), T.rint(r)))


# ---- the byte swap helpers

SWAP_ENTRIES = {"swap16 signed": SW16S, "swap16 unsigned": SW16U, "swap32": SW32, "swap block": SWBLOCK}


def swap_case(label, op, data, offset, count=0):
    return Case(label=label, op=op, data=bytes(data), offset=offset, count=count)


def swaps_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    e.cl = CL.PpcLog({}, {})
    st = particle_stubs(e, pcase("swaps", "swaps"))
    buffer = e.alloc(64, case["data"])
    old = st.activate(e)
    try:
        args = [buffer + case["offset"]] + ([T.u32(case["count"])] if case["op"] == "swap block" else [])
        T.call_ppc(e, SWAP_ENTRIES[case["op"]], args)
    finally:
        CL.Stubs.deactivate(e, old)
    return {"data": e.read(buffer, 64), "log": e.cl.log}


def swaps_host(lib, case):
    bind_particles(lib)
    hostfn(lib, "tpw_pi_swap16s", None, C.c_void_p)
    hostfn(lib, "tpw_pi_swap16u", None, C.c_void_p)
    hostfn(lib, "tpw_pi_swap32", None, C.c_void_p)
    hostfn(lib, "tpw_pi_swapblock", None, C.c_void_p, C.c_int32)
    buffer = C.create_string_buffer(case["data"], 64)
    at = C.addressof(buffer) + case["offset"]
    CL.host_begin(lib, {}, {"swap everywhere": [1]})
    {"swap16 signed": lib.tpw_pi_swap16s, "swap16 unsigned": lib.tpw_pi_swap16u, "swap32": lib.tpw_pi_swap32}.get(
        case["op"], lambda p: lib.tpw_pi_swapblock(p, T.s32(case["count"])))(at)
    return {"data": buffer.raw, "log": CL.host_end(lib)}


def swaps_edges():
    import random
    r = random.Random(77)
    out = []
    for op in SWAP_ENTRIES:
        step = 2 if "16" in op else 4
        for offset in (0, step, 3 * step, 12, 16, 28, 32, 60):
            if op == "swap block" and offset > 16:
                continue
            out.append(swap_case(f"{op} at {offset}", op, bytes(range(1, 65)), offset, 3 if op == "swap block" else 0))
        out.append(swap_case(f"{op} of zeros", op, bytes(64), 0, 4))
        out.append(swap_case(f"{op} of ones", op, b"\xff" * 64, 4, 4))
    for count in (0, 1, 2, 5, 12, -1, -7):
        out.append(swap_case(f"swap block {count} words", "swap block", r.randbytes(64), 0, count))
    return out


def swaps_random(r):
    op = r.choice(list(SWAP_ENTRIES))
    if op == "swap block":
        return swap_case("random swap", op, r.randbytes(64), r.randrange(0, 20, 4), r.randint(-2, 12))
    step = 2 if "16" in op else 4
    return swap_case("random swap", op, r.randbytes(64), r.randrange(0, 60, step), 0)


# ----------------------------------------------------------------- the lobby start's leaves (main app)

SET_TEXTURES, SEC_USER, GEN_FONT, HEAP_ALLOCATE = 0x101EF430, 0x101EF4E0, 0x1016906C, 0x101D7808
G_THEME, G_SHARED, G_USER = 0x103963CC, 0x103963BC, 0x103963DC
G_KEY, G_TEXT, G_MASK, G_STAMP = 0x1029D311, 0x1024F351, 0x102B1324, 0x102B1368
HEAP_GLOBALS = 0x10395C34         # table B, table A, rows B, width B, rows A, width A (six words)
LEAVES_REAL = {SET_TEXTURES, SEC_USER, GEN_FONT, HEAP_ALLOCATE}


def real_stubs(*addresses):
    """Stubs that only lift whatever other test modules left on the unit's own addresses."""
    st = CL.Stubs()
    st.real = set(addresses)
    return st


def with_stubs(e, st, call):
    old = st.activate(e)
    try:
        return call()
    finally:
        CL.Stubs.deactivate(e, old)


# ---- SetThemeTextureDirectories

def texdirs_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    saved = e.read(G_SHARED, 0x20)
    try:
        e.write(G_THEME, bytes([case["fill"]]) * 16)
        e.write(G_SHARED, bytes([case["fill"]]) * 16)
        with_stubs(e, real_stubs(SET_TEXTURES), lambda: T.call_ppc(e, SET_TEXTURES, [T.u32(case["small"])]))
        return {"theme": e.read(G_THEME, 16), "shared": e.read(G_SHARED, 16)}
    finally:
        e.write(G_SHARED, saved)


def texdirs_host(lib, case):
    hostfn(lib, "tpw_pi_texdirs", None, C.c_int32, C.c_int32, C.c_char_p, C.c_char_p)
    theme, shared = C.create_string_buffer(16), C.create_string_buffer(16)
    lib.tpw_pi_texdirs(T.s32(case["small"]), case["fill"], C.cast(theme, C.c_char_p), C.cast(shared, C.c_char_p))
    return {"theme": theme.raw, "shared": shared.raw}


def texdirs_edges():
    return [Case(label=f"small {small} fill {fill:#x}", small=small, fill=fill)
            for small in (0, 1, -1, 2, 0x100, 0x7FFFFFFF, -0x80000000) for fill in (0x00, 0xA5, 0xFF)]


def texdirs_random(r):
    return Case(label="random", small=T.rint(r) * r.randint(0, 1), fill=r.choice((0x00, 0xA5, 0xFF)))


# ---- SEC_AuthorizedUser

def strcmp_stub(e):
    a, b = e.cstr(e.r(3), 4096).encode("latin-1"), e.cstr(e.r(4), 4096).encode("latin-1")
    e.set_r(3, T.u32((a > b) - (a < b)))


def sec_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    areas = {"key": G_KEY, "text": G_TEXT, "mask": G_MASK}
    sizes = {"key": 80, "text": 70, "mask": 68}
    saved = {name: e.read(areas[name], sizes[name]) for name in areas}
    saved_user = e.read(G_USER, 80)
    try:
        for name, patches in case["patches"].items():
            for offset, data in patches:
                e.write(areas[name] + offset, data)
        e.write(G_USER, bytes([case["fill"]]) * 80)
        st = real_stubs(SEC_USER)
        st.imp("strcmp", strcmp_stub)
        answer = with_stubs(e, st, lambda: T.call_ppc(e, SEC_USER, [])[0])
        text = lambda a: e.cstr(a, 100)
        return {"kind": 0 if answer == G_USER else 1 if answer == G_STAMP else 2, "user": e.read(G_USER, 80),
                "key": text(G_KEY), "text": text(G_TEXT), "mask": text(G_MASK), "stamp": text(G_STAMP)}
    finally:
        for name, data in saved.items():
            e.write(areas[name], data)
        e.write(G_USER, saved_user)


def sec_host(lib, case):
    hostfn(lib, "tpw_pi_sec", C.c_int32, C.c_int32, C.c_char_p, C.c_char_p)
    hostfn(lib, "tpw_pi_sec_area", C.c_void_p, C.c_int32, C.POINTER(C.c_int32))
    area = {}
    for name, which in (("key", 0), ("text", 1), ("mask", 2), ("stamp", 3)):
        size = C.c_int32(0)
        area[name] = (lib.tpw_pi_sec_area(which, C.byref(size)), size.value)
    saved = {name: C.string_at(a, n) for name, (a, n) in area.items()}
    try:
        for name, patches in case["patches"].items():
            for offset, data in patches:
                C.memmove(area[name][0] + offset, data, len(data))
        text, user = C.create_string_buffer(160), C.create_string_buffer(80)
        kind = lib.tpw_pi_sec(case["fill"], C.cast(text, C.c_char_p), C.cast(user, C.c_char_p))
        return {"kind": kind, "user": user.raw,
                **{name: C.string_at(a, 100).split(b"\0")[0].decode("latin-1") for name, (a, n) in area.items()}}
    finally:
        for name, data in saved.items():
            C.memmove(area[name][0], data, len(data))


def sec_case(label, fill=0xA5, **patches):
    """patches: for 'key', 'text' and 'mask' the (offset, bytes) pairs to write into the array before the call."""
    return Case(label=label, fill=fill, patches={name: [value] for name, value in patches.items()})


SHIPPED_MASK = b"PQPOEDIJKMCIOOSPLADTOSZAFKLASDJPOYCGFKHGYGHEJLFPZVLKFLIPQWDLFOLFMIE"


def loop_case(label, end, fill=0xA5, mask=SHIPPED_MASK, high=False, **more):
    """A key whose text differs from the string table's (the first character of the table's text changed) and whose
    characters from the 13th differ from the mask's up to index `end`, where they agree: the loop stops there. `high`: the
    key's characters have the top bit set (the differences are taken on signed characters). `more`: further patches."""
    key = bytearray(end + 1)
    for i in range(end):
        key[i] = (mask[i] + 1 + i % 7) & 0xFF
        if high:
            key[i] |= 0x80
            if key[i] == mask[i]:
                key[i] ^= 1
    key[end] = mask[end]
    patches = {"text": [(0, b"Z")], "key": [(0xC, bytes(key))]}
    if mask is not SHIPPED_MASK:
        patches["mask"] = [(0, mask[:end + 1])]
    for name, value in more.items():
        patches[name] = patches.get(name, []) + [value]
    return Case(label=label, fill=fill, patches=patches)


def sec_edges():
    out = [sec_case(f"as shipped fill {fill:#x}", fill) for fill in (0x00, 0xA5, 0xFF)]
    # the compare against the table's text: equal (the stamp), or not (the loop)
    for index in (0, 1, 30, 62):
        out.append(loop_case(f"text differs at {index}", 3, 0x00, text=(index, b"#")))
    out.append(sec_case("key changed before the 13th character", 0x00, key=(0xA, b"zz")))
    out.append(sec_case("mask changed, key equal", 0x00, mask=(0, b"x")))
    out.append(loop_case("key shorter than the text", 4, 0x00, key=(0xD + 20, b"\x00")))
    out.append(loop_case("text shorter than the key", 4, 0x00, text=(10, b"\x00")))
    for end in (0, 1, 2, 3, 7, 20, 49, 60):
        for fill in (0x00, 0xA5):
            out.append(loop_case(f"loop ends at {end} fill {fill:#x}", end, fill))
    for end in (0, 1, 9):
        out.append(loop_case(f"loop ends at {end}, key above 0x7f", end, 0xFF, high=True))
    out.append(loop_case("loop over a changed mask", 6, 0x00, mask=b"\xff\x80\x01\x7f\x00\x81\x90" + SHIPPED_MASK[7:]))
    out.append(loop_case("loop over a changed mask, key high", 6, 0xFF, high=True,
                         mask=b"\x80\x81\x01\x7f\x01\x81\x90" + SHIPPED_MASK[7:]))
    return out


def sec_random(r):
    end = r.randint(0, 55)
    mask = bytes(r.randrange(1, 256) for _ in range(68))
    key = bytearray(r.randrange(256) for _ in range(end + 1))
    key[end] = mask[end]
    for i in range(end):
        if key[i] == mask[i]:
            key[i] = (key[i] + 1) & 0xFF
    patches = {"text": [(r.randrange(0, 62), bytes([r.randrange(1, 256)]))], "key": [(0xC, bytes(key))], "mask": [(0, mask)]}
    return Case(label="random", fill=r.choice((0x00, 0xA5, 0xFF)), patches=patches)


# ---- InterfaceFont::Gen8x8DebugFont

def font_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    e.cl = CL.PpcLog({}, case["queues"])
    obj = e.alloc(0x20, bytes([case["fill"]]) * 0x20)
    st = particle_stubs(e, pcase("font", "font"))
    st.real.add(GEN_FONT)
    a, b, c, d, f = (T.u32(T.s16(x)) for x in case["args"])
    with_stubs(e, st, lambda: T.call_ppc(e, GEN_FONT, [obj, a, b, c, d, f]))
    u16 = lambda at: struct.unpack(">H", e.read(obj + at, 2))[0]
    pointer = e.u32(obj + 8)
    out = {"f00": u16(0), "f02": u16(2), "f04": u16(4), "height": u16(6), "bitmap": e.cl.lab(pointer),
           "id": e.u32(obj + 0xC), "vptr": e.read(obj + 0x10, 4) == bytes([case["fill"]]) * 4, "log": e.cl.log}
    if pointer and e.cl.lab(pointer) > 0:
        out["bytes"] = e.read(pointer, min((T.s16(case["args"][0]) << 3) & MASK, 0x40000))
    return out


def font_host(lib, case):
    hostfn(lib, "tpw_pi_font_prepare", C.c_void_p, C.c_int32)
    hostfn(lib, "tpw_pi_font_offsets", None, C.c_void_p)
    hostfn(lib, "tpw_pi_font_gen", None, C.c_int16, C.c_int16, C.c_int16, C.c_int16, C.c_int16)
    offsets = (C.c_int32 * 7)()
    lib.tpw_pi_font_offsets(offsets)
    storage = lib.tpw_pi_font_prepare(case["fill"])
    CL.host_begin(lib, {}, case["queues"])
    lib.tpw_pi_font_gen(*(T.s16(x) for x in case["args"]))
    log = CL.host_end(lib)
    raw = C.string_at(storage, offsets[6])
    u16 = lambda at: struct.unpack("<H", raw[offsets[at]:offsets[at] + 2])[0]
    pointer = struct.unpack("<Q", raw[offsets[4]:offsets[4] + 8])[0]
    out = {"f00": u16(0), "f02": u16(1), "f04": u16(2), "height": u16(3), "bitmap": lib.tpw_cl_lab(pointer) if pointer else 0,
           "id": struct.unpack("<I", raw[offsets[5]:offsets[5] + 4])[0], "vptr": raw[:8] == bytes([case["fill"]]) * 8,
           "log": log}
    if pointer and out["bitmap"] > 0:
        out["bytes"] = C.string_at(pointer, min((T.s16(case["args"][0]) << 3) & MASK, 0x40000))
    return out


def font_case(label, a, b, c, d, e, fill=0xA5, queues=None):
    return Case(label=label, args=(a, b, c, d, e), fill=fill, queues=queues or {}, margin=0)


def font_edges():
    out = []
    for a in (0, 1, 2, 3, 7, 8, 100, 255, 256, 1000, 0x1000, 0x7FFF, -1, -2, -0x8000, -100):
        out.append(font_case(f"font {a} entries", a, 0x41, 2, 8, 0x7F, fill=0xA5))
    for fill in (0x00, 0xA5, 0xFF):
        out.append(font_case(f"font fill {fill:#x}", 10, 0xFF, -3, 0x7FFF, -0x80, fill=fill))
    out.append(font_case("font entries refused", 10, 1, 2, 3, 4, queues={"array failure": [1]}))
    out.append(font_case("font entries refused, none", 0, 1, 2, 3, 4, queues={"array failure": [1]}))
    for value in (0, 1, 0x7F, 0x80, 0xFF, 0x100, -1, -0x80, 0x7FFF, -0x8000):
        out.append(font_case(f"font bytes {value}", 5, value, value, value, value))
    return out


def font_random(r):
    a = r.choice((r.randint(-5, 300), r.randint(0, 0x7FFF) if r.random() < 0.02 else r.randint(0, 100)))
    return font_case("random font", a, T.s16(r.getrandbits(16)), T.s16(r.getrandbits(16)), T.s16(r.getrandbits(16)),
                     T.s16(r.getrandbits(16)), fill=r.choice((0x00, 0xA5, 0xFF)),
                     queues={"array failure": [1]} if r.random() < 0.1 else {})


# ---- InterfaceStringHeap::Allocate

def heap_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    e.cl = CL.PpcLog({}, case["queues"])
    saved = e.read(HEAP_GLOBALS, 24)
    try:
        e.write(HEAP_GLOBALS, bytes([case["fill"]]) * 24)
        st = particle_stubs(e, pcase("heap", "heap", margin=case["margin"]))
        st.real.add(HEAP_ALLOCATE)
        result = with_stubs(e, st, lambda: T.s32(T.call_ppc(e, HEAP_ALLOCATE, [T.u32(x) for x in case["args"]])[0]))
        s32 = lambda i: struct.unpack(">i", e.read(HEAP_GLOBALS + 4 * i, 4))[0]
        table_b, table_a = e.u32(HEAP_GLOBALS), e.u32(HEAP_GLOBALS + 4)
        out = {"result": None if case["void"] else result, "width_a": s32(5), "rows_a": s32(4), "width_b": s32(3),
               "rows_b": s32(2), "table_a": e.cl.lab(table_a), "table_b": e.cl.lab(table_b), "log": e.cl.log}
        for name, pointer, rows, width in (("bytes_a", table_a, s32(4), s32(5)), ("bytes_b", table_b, s32(2), s32(3))):
            if pointer and e.cl.lab(pointer) > 0:
                out[name] = e.read(pointer, min(((rows * width * 2) & MASK) + case["margin"], 0x80000))
        return out
    finally:
        e.write(HEAP_GLOBALS, saved)


def heap_host(lib, case):
    hostfn(lib, "tpw_pi_heap_prepare", None, C.c_int32)
    hostfn(lib, "tpw_pi_heap_alloc", C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32)
    hostfn(lib, "tpw_pi_heap_alloc_void", None, C.c_int32, C.c_int32, C.c_int32, C.c_int32)
    hostfn(lib, "tpw_pi_heap_globals", None, C.c_void_p)
    hostfn(lib, "tpw_pi_heap_table", C.c_void_p, C.c_int32)
    bind_particles(lib)
    lib.tpw_pi_heap_prepare(case["fill"])
    lib.tpw_pi_set_margin(case["margin"])
    CL.host_begin(lib, {}, case["queues"])
    args = [T.s32(x) for x in case["args"]]
    result = None
    if case["void"]:
        lib.tpw_pi_heap_alloc_void(*args)
    else:
        result = lib.tpw_pi_heap_alloc(*args)
    log = CL.host_end(lib)
    lib.tpw_pi_set_margin(0)
    words = (C.c_int64 * 6)()
    lib.tpw_pi_heap_globals(words)
    out = {"result": result, "width_a": words[0], "rows_a": words[1], "width_b": words[2], "rows_b": words[3],
           "table_a": words[4], "table_b": words[5], "log": log}
    for name, which, rows, width in (("bytes_a", 0, words[1], words[0]), ("bytes_b", 1, words[3], words[2])):
        pointer = lib.tpw_pi_heap_table(which)
        if pointer and out["table_a" if which == 0 else "table_b"] > 0:
            out[name] = C.string_at(pointer, min(((rows * width * 2) & MASK) + case["margin"], 0x80000))
    return out


def heap_case(label, a, b, c, d, fill=0xA5, queues=None, void=False):
    return Case(label=label, args=(a, b, c, d), fill=fill, queues=queues or {}, void=void, margin=4096)


def heap_edges():
    out = [heap_case("the game's call", 0x40, 0x80, 0x10, 0x80),
           heap_case("the game's call, void form", 0x40, 0x80, 0x10, 0x80, void=True)]
    for index in range(4):
        args = [0x40, 0x80, 0x10, 0x80]
        args[index] = 0
        out.append(heap_case(f"argument {index} is zero", *args))
    for c in (-5, 0, 1, 4, 5, 6, 0x3F, 0x40, 0x41):
        out.append(heap_case(f"third argument {c}", 0x40, 8, c, 8))
    for a in (5, 6, 7, 0x10, 0x11, -1):
        out.append(heap_case(f"first argument {a}", a, 8, 5, 8))
    for b, d in ((1, 1), (2, 5), (8, 8), (9, 8), (12, 8), (20, 8), (30, 1), (-1, 8), (8, -1), (-3, -3)):
        out.append(heap_case(f"rows {b} and {d}", 0x20, b, 0x10, d))
    out.append(heap_case("table A too big", 0x10000000, 4, 5, 4))
    out.append(heap_case("table B too big", 0x20, 4, 5, 0x10000000))
    for fill in (0x00, 0xFF):
        out.append(heap_case(f"fill {fill:#x}", 0x14, 4, 6, 4, fill=fill))
    out.append(heap_case("first table refused", 0x14, 4, 6, 4, queues={"array failure": [1, 0]}))
    out.append(heap_case("second table refused", 0x14, 4, 6, 4, queues={"array failure": [0, 1]}))
    out.append(heap_case("both refused", 0x14, 4, 6, 4, queues={"array failure": [1]}))
    out.append(heap_case("first table refused, void", 0x14, 4, 6, 4, queues={"array failure": [1, 0]}, void=True))
    return out


def heap_random(r):
    a = r.choice((r.randint(-3, 80), r.randint(5, 40)))
    c = r.choice((r.randint(-3, 80), r.randint(3, a) if a >= 3 else 5))
    b = r.randint(-2, 12)
    d = b + r.randint(0, 6) if b > 0 and r.random() < 0.9 else r.randint(-2, 12)
    if b > d:
        d = b       # more rows in A than in B would write further past table B than the margin takes
    return heap_case("random heap", a, b, c, d, fill=r.choice((0x00, 0xA5, 0xFF)), void=r.random() < 0.2,
                     queues={"array failure": [int(r.random() < 0.5), int(r.random() < 0.5)]} if r.random() < 0.1 else {})


# ----------------------------------------------------------------- the ltms font render parameters

FRP_DEFAULT, FRP_DTOR, FRP_COPY, FRP_ALPHA = 0x10002350, 0x10002400, 0x100024AC, 0x1000296C
AREA_CTOR, OPERATOR_DELETE_LTMS = 0x1000F724, 0x1000F694
FRP_BYTES = 0x50
GUARD = 16                                # bytes kept behind the object, untouched by the call


def frp_font(e):
    """The font whose AddRef and Release record: an object (vtable pointer at +0) whose vtable has fake functions in slots 8
    and 0xc."""
    if not hasattr(e, "w23_font"):
        vtable = T.persist_alloc(e, 0x40)
        e.w23_font = T.persist_alloc(e, 0x10)
        e.write(e.w23_font, struct.pack(">I", vtable))
        e.write(vtable + 0x8, struct.pack(">I", T.fake_function(e, lambda em: em.cl.rec("Release", (em.cl.lab(em.r(3)),)))))
        e.write(vtable + 0xC, struct.pack(">I", T.fake_function(e, lambda em: em.cl.rec("AddRef", (em.cl.lab(em.r(3)),)))))
    return e.w23_font


def frp_stubs(e):
    st = real_stubs(FRP_DEFAULT, FRP_DTOR, FRP_COPY, FRP_ALPHA)

    def delete(e):
        L, p = e.cl, e.r(3)
        if p and L.lab(p) > 0:
            L.rec("delete", (L.lab(p),))
            L.forget(p)

    st.internal(OPERATOR_DELETE_LTMS, delete)

    def area_ctor(e):
        L, this = e.cl, e.r(3)
        L.rec("TbRenderArea::TbRenderArea", (L.newlab(this),))
        e.write(this, b"".join(struct.pack(">I", 0x7A000000 + i) for i in range(10)))

    st.internal(AREA_CTOR, area_ctor)
    return st


def frp_words_ppc(e, obj):
    """The object as both sides describe it: the Mac words by their offsets (word 1 as the font's label, the render area's
    ten words at 5 to 14), the flag bytes with the three bytes behind them, the last word, the untouched tail."""
    word = lambda m: e.u32(obj + 4 * m)
    return {"words": [e.cl.lab(word(1)), word(2), word(3), word(4)] + [word(m) for m in range(5, 15)] + [word(15), word(16)],
            "flags": e.read(obj + 0x44, 8), "word19": word(19), "tail": e.read(obj + FRP_BYTES, GUARD)}


def frp_words_host(lib, raw, size, area):
    """The same from the host's object: a vtable pointer, then slots of 8 bytes (slot k is the Mac's word k + 1). `area`:
    'ctor' for the ten words of the render area object the constructor made (at the start of slot 4), 'slots' for the
    words as the slots hold them (the copy and UseAlphaWriteMode)."""
    slot = lambda k: struct.unpack("<Q", raw[8 + 8 * k:16 + 8 * k])[0]
    low = lambda k: slot(k) & MASK
    font = slot(0)
    area_words = ([struct.unpack("<I", raw[40 + 4 * i:44 + 4 * i])[0] for i in range(10)] if area == "ctor"
                  else [low(k) for k in range(4, 14)])
    return {"words": [lab_of(lib, font), low(1), low(2), low(3)] + area_words + [low(14), low(15)],
            "flags": raw[8 + 8 * 16:16 + 8 * 16], "word19": low(18), "tail": raw[size:size + GUARD]}


def lab_of(lib, pointer):
    return lib.tpw_cl_lab(pointer) if pointer else 0


def frp_object_ppc(e, case, font):
    """An object with the case's words: the font (or none), words 2 to 16, the flags and the last word."""
    obj = e.alloc(FRP_BYTES + GUARD, b"\x77" * (FRP_BYTES + GUARD))
    e.write(obj + 4, struct.pack(">I", font if case["font"] else 0))
    for k in range(1, 16):
        e.write(obj + 4 + 4 * k, struct.pack(">I", case["slots"][k] & MASK))
    e.write(obj + 0x44, case["flags"])
    e.write(obj + 0x4C, struct.pack(">I", case["slots"][18] & MASK))
    return obj


def frp_object_host(case, size, font):
    raw = bytearray(b"\x77" * (size + GUARD))
    for k in range(19):
        raw[8 + 8 * k:16 + 8 * k] = struct.pack("<Q", (font if case["font"] else 0) if k == 0 else case["slots"][k])
    raw[8 + 8 * 16:16 + 8 * 16] = case["flags"]
    return C.create_string_buffer(bytes(raw), size + GUARD)


def frp_bind(lib):
    hostfn(lib, "tpw_pi_frp_size", C.c_int32)
    hostfn(lib, "tpw_pi_frp_font", C.c_void_p)
    hostfn(lib, "tpw_pi_frp_default", None, C.c_void_p)
    hostfn(lib, "tpw_pi_frp_copy", None, C.c_void_p, C.c_void_p)
    hostfn(lib, "tpw_pi_frp_alpha", C.c_int32, C.c_void_p)
    hostfn(lib, "tpw_pi_frp_new", C.c_void_p, C.c_void_p, C.c_int32)
    hostfn(lib, "tpw_pi_frp_destroy", None, C.c_void_p, C.c_int32)
    hostfn(lib, "tpw_pi_frp_free", None, C.c_void_p)
    hostfn(lib, "tpw_pi_frp_slots", None, C.c_void_p, C.c_void_p, C.c_int32)
    hostfn(lib, "tpw_cl_lab", C.c_int32, C.c_void_p)


def frp_default_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    e.cl = CL.PpcLog({}, {})
    fill = bytes([case["fill"]])
    obj = e.alloc(FRP_BYTES + GUARD, fill * (FRP_BYTES + GUARD))
    with_stubs(e, frp_stubs(e), lambda: T.call_ppc(e, FRP_DEFAULT, [obj]))
    out = frp_words_ppc(e, obj)
    out.update(vptr=e.u32(obj) != struct.unpack(">I", fill * 4)[0], log=e.cl.log)
    return out


def frp_default_host(lib, case):
    frp_bind(lib)
    size = lib.tpw_pi_frp_size()
    buffer = C.create_string_buffer(bytes([case["fill"]]) * (size + GUARD), size + GUARD)
    CL.host_begin(lib, {}, {})
    lib.tpw_pi_frp_default(C.addressof(buffer))
    log = CL.host_end(lib)
    raw = buffer.raw
    out = frp_words_host(lib, raw, size, "ctor")
    out.update(vptr=raw[:8] != bytes([case["fill"]]) * 8, log=log)
    return out


def frp_default_edges():
    return [Case(label=f"fill {fill:#x}", fill=fill) for fill in (0x00, 0xA5, 0xFF, 0x5A)]


def frp_source(r):
    return {"font": True, "slots": [r.getrandbits(64) for _ in range(19)], "flags": r.randbytes(8)}


# the copy constructor: the font takes a reference (a NULL font would fault, on both sides, and is not tried)

def frp_copy_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    font = frp_font(e)
    e.cl = CL.PpcLog({font: 1}, {})
    fill = bytes([case["fill"]])
    source = frp_object_ppc(e, case, font)
    dest = e.alloc(FRP_BYTES + GUARD, fill * (FRP_BYTES + GUARD))
    with_stubs(e, frp_stubs(e), lambda: T.call_ppc(e, FRP_COPY, [dest, source]))
    out = frp_words_ppc(e, dest)
    out.update(vptr=e.u32(dest) != struct.unpack(">I", fill * 4)[0], log=e.cl.log,
               same=all(e.u32(dest + 4 * m) == e.u32(source + 4 * m) for m in range(1, 17)))
    return out


def frp_copy_host(lib, case):
    frp_bind(lib)
    size = lib.tpw_pi_frp_size()
    font = lib.tpw_pi_frp_font()
    source = frp_object_host(case, size, font)
    dest = C.create_string_buffer(bytes([case["fill"]]) * (size + GUARD), size + GUARD)
    CL.host_begin(lib, {1: font}, {})
    lib.tpw_pi_frp_copy(C.addressof(dest), C.addressof(source))
    log = CL.host_end(lib)
    after = dest.raw
    out = frp_words_host(lib, after, size, "slots")
    out.update(vptr=after[:8] != bytes([case["fill"]]) * 8, log=log,
               same=all(after[8 + 8 * k:16 + 8 * k] == source.raw[8 + 8 * k:16 + 8 * k] for k in range(16)))
    return out


def frp_copy_edges():
    import random
    r = random.Random(23)
    out = [Case(label=f"copy fill {fill:#x}", fill=fill, **frp_source(r)) for fill in (0x00, 0xA5, 0xFF)]
    out.append(Case(label="copy of zeros", fill=0xA5, font=True, slots=[0] * 19, flags=bytes(8)))
    out.append(Case(label="copy of ones", fill=0x00, font=True, slots=[(1 << 64) - 1] * 19, flags=b"\xff" * 8))
    return out


def frp_copy_random(r):
    return Case(label="random copy", fill=r.choice((0x00, 0xA5, 0xFF)), **frp_source(r))


# UseAlphaWriteMode: with a font the fourth word is cleared and the answer is 0, without -1

def alpha_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    font = frp_font(e)
    e.cl = CL.PpcLog({font: 1}, {})
    obj = frp_object_ppc(e, case, font)
    sret = e.alloc(8, b"\xa5" * 8)
    with_stubs(e, frp_stubs(e), lambda: T.call_ppc(e, FRP_ALPHA, [sret, obj]))
    out = frp_words_ppc(e, obj)
    out.update(result=T.s32(e.u32(sret)), log=e.cl.log)
    return out


def alpha_host(lib, case):
    frp_bind(lib)
    size = lib.tpw_pi_frp_size()
    font = lib.tpw_pi_frp_font()
    buffer = frp_object_host(case, size, font)
    CL.host_begin(lib, {1: font}, {})
    result = lib.tpw_pi_frp_alpha(C.addressof(buffer))
    log = CL.host_end(lib)
    out = frp_words_host(lib, buffer.raw, size, "slots")
    out.update(result=result, log=log)
    return out


def alpha_edges():
    import random
    r = random.Random(29)
    out = [Case(label=f"alpha font {font} word {value:#x}", font=font, slots=[0] + [value] * 18, flags=b"\x01" * 8)
           for font in (True, False) for value in (0, 1, 0xFFFFFFFF, 0x12345678)]
    out += [Case(label="alpha random", font=font, slots=[r.getrandbits(64) for _ in range(19)], flags=r.randbytes(8))
            for font in (True, False)]
    return out


def alpha_random(r):
    return Case(label="random alpha", font=r.random() < 0.6, slots=[r.getrandbits(64) for _ in range(19)],
                flags=r.randbytes(8))


# the destructor: the font is released and forgotten; the deleting form (a positive flag) deletes the object

FRP_OBJECT_LABEL = 2


def dtor_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    font = frp_font(e)
    obj = frp_object_ppc(e, case, font)
    e.cl = CL.PpcLog({font: 1, obj: FRP_OBJECT_LABEL}, {})
    flag = T.s16(case["flag"])
    result = with_stubs(e, frp_stubs(e), lambda: T.call_ppc(e, FRP_DTOR, [obj, T.u32(flag)])[0] == obj)
    out = {"result": result, "log": e.cl.log}
    if flag <= 0:
        out.update(frp_words_ppc(e, obj))
    return out


def dtor_host(lib, case):
    frp_bind(lib)
    size = lib.tpw_pi_frp_size()
    font = lib.tpw_pi_frp_font()
    source = frp_object_host(case, size, font).raw
    flag = T.s16(case["flag"])
    obj = lib.tpw_pi_frp_new(source[8:size], size - 8)
    CL.host_begin(lib, {1: font, FRP_OBJECT_LABEL: obj}, {})
    lib.tpw_pi_frp_destroy(obj, flag)
    log = CL.host_end(lib)
    out = {"result": True, "log": log}
    if flag <= 0:
        slots = C.create_string_buffer(size - 8)
        lib.tpw_pi_frp_slots(obj, slots, size - 8)
        out.update(frp_words_host(lib, bytes(8) + slots.raw + b"\x77" * GUARD, size, "slots"))
        lib.tpw_pi_frp_free(obj)
    return out


def dtor_edges():
    import random
    r = random.Random(31)
    return [Case(label=f"destructor flag {flag}, font {font}", font=font, flag=flag, slots=[r.getrandbits(64) for _ in range(19)],
                 flags=r.randbytes(8))
            for font in (True, False) for flag in (0, 1, 2, -1, 0x7FFF, -0x8000)]


def dtor_random(r):
    return Case(label="random destructor", font=r.random() < 0.6, flag=T.s16(r.getrandbits(16)),
                slots=[r.getrandbits(64) for _ in range(19)], flags=r.randbytes(8))


# ----------------------------------------------------------------- the bullfrog semaphore lock

SEMA_DTOR, SEMA_LOCK, OPERATOR_DELETE = 0x10024B00, 0x10024B6C, 0x1006B2D4
SEMA_LABEL = 1


def sema_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    lock = 0 if case["null"] else e.alloc(16, struct.pack(">IIII", case["word"], 0, 0, 0))
    e.cl = CL.PpcLog({lock: SEMA_LABEL} if lock else {}, {})
    st = real_stubs(SEMA_DTOR, SEMA_LOCK)

    def delete(e):
        L, p = e.cl, e.r(3)
        if p and L.lab(p) > 0:
            L.rec("delete", (L.lab(p),))
            L.forget(p)

    st.internal(OPERATOR_DELETE, delete)
    deleted = case["op"] == "dtor" and T.s16(case["arg"]) > 0
    if case["op"] == "dtor":
        result = with_stubs(e, st, lambda: T.call_ppc(e, SEMA_DTOR, [lock, T.u32(case["arg"])])[0] == lock)
    else:
        result = with_stubs(e, st, lambda: T.s32(T.call_ppc(e, SEMA_LOCK, [lock, T.u32(case["arg"])])[0]))
    return {"result": result, "log": e.cl.log,
            "words": list(struct.unpack(">III", e.read(lock, 12))) if lock and not deleted else None}


def sema_host(lib, case):
    hostfn(lib, "tpw_pi_sema_new", C.c_void_p, C.c_uint32)
    hostfn(lib, "tpw_pi_sema_dtor", None, C.c_void_p, C.c_int32)
    hostfn(lib, "tpw_pi_sema_lock", C.c_int32, C.c_void_p, C.c_int32)
    hostfn(lib, "tpw_pi_sema_free", None, C.c_void_p)
    lock = None if case["null"] else lib.tpw_pi_sema_new(case["word"])
    CL.host_begin(lib, {SEMA_LABEL: lock} if lock else {}, {})
    deleted = case["op"] == "dtor" and T.s16(case["arg"]) > 0
    if case["op"] == "dtor":
        lib.tpw_pi_sema_dtor(lock, T.s16(case["arg"]))
        result = True
    else:
        result = lib.tpw_pi_sema_lock(lock, T.s32(case["arg"]))
    log = CL.host_end(lib)
    words = None
    if lock and not deleted:
        words = list(struct.unpack("<III", C.string_at(lock, 12)))
        lib.tpw_pi_sema_free(lock)
    return {"result": result, "log": log, "words": words}


def sema_case(label, op, arg, word=0x12345678, null=False):
    return Case(label=label, op=op, arg=arg, word=word, null=null)


def sema_dtor_edges():
    out = []
    for flag in (0, 1, 2, -1, 0x7FFF, -0x8000):
        for null in (False, True):
            out.append(sema_case(f"destructor flag {flag}, null {null}", "dtor", flag, null=null))
    return out


def sema_dtor_random(r):
    return sema_case("random destructor", "dtor", r.choice((r.randint(-0x8000, 0x7FFF), 0, 1)), r.getrandbits(32), r.random() < 0.2)


def sema_lock_edges():
    return [sema_case(f"lock {wait}", "lock", wait) for wait in (0, 1, -1, 2, 1000, 0x7FFFFFFF, -0x80000000)]


def sema_lock_random(r):
    return sema_case("random lock", "lock", T.rint(r), r.getrandbits(32))


TESTS = [
    OnProgram(BULLFROG, "LbZLib_MemDecompress (the zlib unpacker)", lbzlib_ppc, lbzlib_host, edge=zlib_edges,
              rnd=zlib_random),
    OnProgram(BULLFROG, "bfl::compress::CZLib::MemDecompress", czlib_ppc, czlib_host, edge=zlib_edges, rnd=zlib_random),
    OnProgram(BULLFROG, "SwapHeader (CZLib's header swap)", swap_ppc, swap_host, edge=swap_edges, rnd=swap_random),
    OnProgram(BULLFROG, "bfl::compress::CZLib::CompressWorkspaceSize", size_ppc, size_host, edge=lambda: [0], rnd=lambda r: 0),
    OnProgram(MAIN, "PAR_InitParticleSystem", particle_ppc, particle_host, edge=init_edges, rnd=init_random),
    OnProgram(MAIN, "PAR_ResetParticleSystem", particle_ppc, particle_host, edge=reset_edges, rnd=reset_random),
    OnProgram(MAIN, "PAR_LoadLibrary (the library loader)", particle_ppc, particle_host, edge=load_edges, rnd=load_random),
    OnProgram(MAIN, "PAR_SetupSin", particle_ppc, particle_host, edge=sin_edges, rnd=sin_random),
    OnProgram(MAIN, "PAR_Malloc", particle_ppc, particle_host, edge=malloc_edges, rnd=malloc_random),
    OnProgram(MAIN, "PAR_SetLowSpec", particle_ppc, particle_host, edge=lowspec_edges, rnd=lowspec_random),
    OnProgram(MAIN, "DummyHeightCallBack", particle_ppc, particle_host, edge=height_edges, rnd=height_random),
    OnProgram(MAIN, "SamsUtilities swap helpers (USwapInplace16 x2, USwapInplace32, USwapBlock32)", swaps_ppc, swaps_host,
              edge=swaps_edges, rnd=swaps_random),
    OnProgram(MAIN, "SetThemeTextureDirectories", texdirs_ppc, texdirs_host, edge=texdirs_edges, rnd=texdirs_random),
    OnProgram(MAIN, "SEC_AuthorizedUser", sec_ppc, sec_host, edge=sec_edges, rnd=sec_random),
    OnProgram(MAIN, "InterfaceFont::Gen8x8DebugFont", font_ppc, font_host, edge=font_edges, rnd=font_random),
    OnProgram(MAIN, "InterfaceStringHeap::Allocate", heap_ppc, heap_host, edge=heap_edges, rnd=heap_random),
    OnProgram(LTMS, "TbFontRenderParams::TbFontRenderParams()", frp_default_ppc, frp_default_host, edge=frp_default_edges,
              rnd=lambda r: Case(label="random", fill=r.getrandbits(8))),
    OnProgram(LTMS, "TbFontRenderParams::~TbFontRenderParams", dtor_ppc, dtor_host, edge=dtor_edges, rnd=dtor_random),
    OnProgram(LTMS, "TbFontRenderParams copy constructor", frp_copy_ppc, frp_copy_host, edge=frp_copy_edges,
              rnd=frp_copy_random),
    OnProgram(LTMS, "TbFontRenderParams::UseAlphaWriteMode", alpha_ppc, alpha_host, edge=alpha_edges, rnd=alpha_random),
    OnProgram(BULLFROG, "_LbSemaLock::~_LbSemaLock", sema_ppc, sema_host, edge=sema_dtor_edges, rnd=sema_dtor_random),
    OnProgram(BULLFROG, "_LbSemaLock::Lock", sema_ppc, sema_host, edge=sema_lock_edges, rnd=sema_lock_random),
]
