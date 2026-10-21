"""Unit w168, the particle system's swap helpers, against libtpw_unit_ParticleSwap.dylib (tests/unit_ParticleSwap.cpp: the sources and
wrappers of tests/unit_ParticleSave.cpp built with TPW_SWAP_FILE_WORDS, so that the swaps really swap), main app ("SimTheme Park"),
src/game/Particles.cpp:
  tagPARTICLE::Byteswap 0x100BB110, USwapBlockStruct<tagEFFECTER> 0x100BB260, USwapBlockStruct<tagEMITTER> 0x100BB3B8,
  USwap32<tagPARTICLE*> 0x100BB6AC, USwap32<void*> 0x100BB720, USwap32<long*> 0x100BB77C

A case is a buffer of random bytes (or a word), put in the emulator's memory (big-endian) and in the host library's memory, and the
function called on it; the original swaps the fields in place with its own four helpers, and the translation's host copies of them swap
with __builtin_bswap in this library, so the same bytes come out when the field lists agree. The USwapBlock32 call of an emitter's
16 words is faked on both sides (it logs its count and swaps). Compared: the buffer (with a guard of 16 bytes after it), the
answer of the word swaps, the call log. No fused operations. Not tested: a negative or huge count (the original loops on it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
import test_ParticleSave as S

SHIM_LIBRARY = "unit_ParticleSwap"
MASK = 0xFFFFFFFF

BYTESWAP, EFFECTERS, EMITTERS = 0x100BB110, 0x100BB260, 0x100BB3B8
SWAP_PARTICLE_PTR, SWAP_VOID_PTR, SWAP_LONG_PTR = 0x100BB6AC, 0x100BB720, 0x100BB77C
SIZES = {BYTESWAP: 292, EFFECTERS: 260, EMITTERS: 672, SWAP_PARTICLE_PTR: 40, SWAP_VOID_PTR: 40, SWAP_LONG_PTR: 40}
OWN = set(SIZES)

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", S.stub_swap_block)

GUARD = 16


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound2", False):
        return
    S.bind(lib)
    lib.tpw_px_byteswap.argtypes = [C.c_uint64]
    lib.tpw_px_swap_emitters.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_px_swap_effecters.argtypes = [C.c_uint64, C.c_uint32]
    for n in ("particle", "void", "long"):
        f = getattr(lib, "tpw_px_swap_%s_ptr" % n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint32]
    lib.bound2 = True


def ppc(e, case):
    L = e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        if case["op"] == "word":
            r3, _, _ = T.call_ppc(e, case["entry"], [case["word"]])
            return {"answer": r3 & MASK, "log": L.log}
        n = len(case["data"])
        buf = e.alloc(n + GUARD, case["data"] + b"\x5a" * GUARD)
        if case["op"] == "byteswap":
            T.call_ppc(e, BYTESWAP, [buf])
        else:
            T.call_ppc(e, case["entry"], [buf, case["count"]])
        return {"buffer": e.read(buf, n + GUARD), "log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    if case["op"] == "word":
        return {"answer": getattr(lib, case["fn"])(case["word"]) & MASK, "log": CL.host_end(lib)}
    n = len(case["data"])
    buf = lib.tpw_px_alloc(n + GUARD)
    C.memmove(buf, case["data"] + b"\x5a" * GUARD, n + GUARD)
    if case["op"] == "byteswap":
        lib.tpw_px_byteswap(buf)
    else:
        getattr(lib, case["fn"])(buf, case["count"])
    return {"buffer": C.string_at(buf, n + GUARD), "log": CL.host_end(lib)}


class PCase(dict):
    def __repr__(self):
        return self["label"]


def patterns(r, n):
    """Bytes that tell the positions apart, zeros, ones and random."""
    return [bytes((i * 7 + 3) & 0xFF for i in range(n)), bytes(n), b"\xff" * n, bytes((i + 1) & 0xFF for i in range(n)), r.randbytes(n)]


def struct_cases(op, entry, fn, size, counts_edge, label):
    def make(data, count, tag):
        return PCase(op=op, entry=entry, fn=fn, data=data, count=count, label="%s %s count %d" % (label, tag, count))

    def edges():
        r = random.Random(1683)
        for count in counts_edge:
            for i, data in enumerate(patterns(r, size * count)):
                yield make(data, count, "pattern%d" % i)

    def rnd(r):
        count = r.randrange(0, 5)
        return make(r.randbytes(size * count), count, "random")

    return edges, rnd


def byteswap_edges():
    r = random.Random(1684)
    for i, data in enumerate(patterns(r, 0x34)):
        yield PCase(op="byteswap", data=data, label="particle pattern%d" % i)


def byteswap_random(r):
    return PCase(op="byteswap", data=r.randbytes(0x34), label="particle random")


def word_cases(entry, fn, label):
    def edges():
        for w in (0, 1, MASK, 0x80000000, 0x7FFFFFFF, 0x12345678, 0xDEADBEEF, 0x000000FF, 0xFF000000):
            yield PCase(op="word", entry=entry, fn=fn, word=w, label="%s %08x" % (label, w))

    def rnd(r):
        return PCase(op="word", entry=entry, fn=fn, word=r.getrandbits(32), label="%s random" % label)

    return edges, rnd


em_edges, em_rnd = struct_cases("emitters", EMITTERS, "tpw_px_swap_emitters", 0x140, (0, 1, 2, 3), "emitters")
ef_edges, ef_rnd = struct_cases("effecters", EFFECTERS, "tpw_px_swap_effecters", 0x68, (0, 1, 2, 3), "effecters")
pp_edges, pp_rnd = word_cases(SWAP_PARTICLE_PTR, "tpw_px_swap_particle_ptr", "particle ptr")
vp_edges, vp_rnd = word_cases(SWAP_VOID_PTR, "tpw_px_swap_void_ptr", "void ptr")
lp_edges, lp_rnd = word_cases(SWAP_LONG_PTR, "tpw_px_swap_long_ptr", "long ptr")

TESTS = [
    FnTest("tagPARTICLE::Byteswap", ppc, host, edge=byteswap_edges, rnd=byteswap_random),
    FnTest("USwapBlockStruct<tagEFFECTER>", ppc, host, edge=ef_edges, rnd=ef_rnd),
    FnTest("USwapBlockStruct<tagEMITTER>", ppc, host, edge=em_edges, rnd=em_rnd),
    FnTest("USwap32<tagPARTICLE*>", ppc, host, edge=pp_edges, rnd=pp_rnd),
    FnTest("USwap32<void*>", ppc, host, edge=vp_edges, rnd=vp_rnd),
    FnTest("USwap32<long*>", ppc, host, edge=lp_edges, rnd=lp_rnd),
]
