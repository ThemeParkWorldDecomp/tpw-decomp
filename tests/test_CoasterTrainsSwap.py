"""Unit w192, the four in-place byte swaps, against libtpw_unit_CoasterTrainsSwap.dylib (tests/unit_CoasterTrainsSwap.cpp: the same
sources as test_CoasterTrains, built with TPW_SWAP_FILE_WORDS), main app ("SimTheme Park"), src/game/rides/BumperRide.cpp, src/game/rides/TPCoaster.cpp:
  USwapInplace16<unsigned short> 0x10043DB8, USwapInplace32<int> 0x10043E00, USwapInplace32<unsigned long> 0x10043E44, USwapInplaceFloat 0x10021BB4.
Each takes the address of a value and reverses its bytes in place. The original runs on a guarded buffer in emulator memory (the value written
big-endian), the translation on a ctypes buffer (little-endian); both read the value back as a plain number, so the two results are the byte
reversed words. The bytes around the value (guards) must not change. Floats are bit patterns (NaNs, signalling NaNs and denormals included):
the original loads the word and stores it again with integer instructions, the translation must not quiet a signalling NaN.
Not tested: the callers (test_CoasterTrains.py)."""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CoasterTrainsSwap"
MASK = 0xFFFFFFFF

ADDR = {"swap16": 0x10043DB8, "swap32i": 0x10043E00, "swap32u": 0x10043E44, "swapf": 0x10021BB4}
WIDTH = {"swap16": 2, "swap32i": 4, "swap32u": 4, "swapf": 4}

STUBS = CL.Stubs()
STUBS.real.update(ADDR.values())

GUARD = bytes([0xA5, 0x5A, 0xC3, 0x3C, 0x96, 0x69, 0xF0, 0x0F])


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n in ADDR:
        getattr(lib, "tpw_ctr_" + n).argtypes = [C.c_uint64]
    lib.bound = True


def ppc_run(name):
    def run(e, case):
        w = WIDTH[name]
        buf = e.alloc(16, GUARD + case["value"].to_bytes(w, "big") + GUARD[:8 - w])
        e.cl = CL.PpcLog({}, None)
        saved = STUBS.activate(e)
        try:
            T.call_ppc(e, ADDR[name], [buf + 8])
        finally:
            CL.Stubs.deactivate(e, saved)
        raw = e.read(buf, 16)
        return {"value": int.from_bytes(raw[8:8 + w], "big"), "guards": raw[:8] + raw[8 + w:]}
    return run


def host_run(name):
    def run(lib, case):
        bind(lib)
        w = WIDTH[name]
        buf = (C.c_ubyte * 16).from_buffer_copy(GUARD + case["value"].to_bytes(w, "little") + GUARD[:8 - w])
        getattr(lib, "tpw_ctr_" + name)(C.addressof(buf) + 8)
        raw = bytes(buf)
        return {"value": int.from_bytes(raw[8:8 + w], "little"), "guards": raw[:8] + raw[8 + w:]}
    return run


EDGES16 = [0, 1, 0xFF, 0x100, 0x1234, 0x7FFF, 0x8000, 0xFFFF, 0xFF00, 0x00FF]
EDGES32 = [0, 1, 0xFF, 0x100, 0xFFFF, 0x10000, 0x12345678, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFF000000, 0x00FF00FF, 0xDEADBEEF]
FLOATS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7FC00001, 0x7F800001, 0xFFBFFFFF,
          0x00000001, 0x007FFFFF, 0x80000001, 0x7F7FFFFF, 0x0000803F, 0x0000C07F, 0x0100807F, 0x437F0000]


def edges(name):
    if name == "swap16":
        return lambda: ({"value": v} for v in EDGES16)
    if name == "swapf":
        return lambda: ({"value": v} for v in FLOATS)
    return lambda: ({"value": v} for v in EDGES32)


def rnd(name):
    bits = 8 * WIDTH[name]
    return lambda r: {"value": r.getrandbits(bits)}


TESTS = [
    FnTest("USwapInplace16<unsigned short>", ppc_run("swap16"), host_run("swap16"), edge=edges("swap16"), rnd=rnd("swap16")),
    FnTest("USwapInplace32<int>", ppc_run("swap32i"), host_run("swap32i"), edge=edges("swap32i"), rnd=rnd("swap32i")),
    FnTest("USwapInplace32<unsigned long>", ppc_run("swap32u"), host_run("swap32u"), edge=edges("swap32u"), rnd=rnd("swap32u")),
    FnTest("USwapInplaceFloat", ppc_run("swapf"), host_run("swapf"), edge=edges("swapf"), rnd=rnd("swapf")),
]
