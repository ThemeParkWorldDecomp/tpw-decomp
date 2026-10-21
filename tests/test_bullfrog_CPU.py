"""Differential tests for the bullfrog library's CPU class (src/bullfrog/LbLog.cpp,
namespace bullfrog). See difftest.py and tpwtest.py.

Gestalt answers from the case on both sides (tpwtest's stub, the host placeholder) and
both count the calls; the clock global (0x10073e84) and the 0x1c-byte object start from
the case's values.
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

CLOCK = 0x10073e84
VP, I32 = ctypes.c_void_p, ctypes.c_int32


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.gestalt_calls = []

    def gestalt(e):
        e.gestalt_calls.append(e.r(3))
        err, value = e.gestalt
        if err == 0:
            e.write(e.r(4), struct.pack(">i", T.s32(value)))
        e.set_r(3, T.u32(err))
    e.stub("Gestalt", gestalt)


def obj_words(raw):
    return dict(type=T.s32(struct.unpack(">I", raw[0:4])[0]), features=struct.unpack(">I", raw[4:8])[0],
                rdtsc=T.s32(struct.unpack(">I", raw[8:12])[0]), name=raw[12:28])


def detect_ppc(e, c):
    e.write(CLOCK, struct.pack(">i", c["clock"]))
    obj = e.alloc(0x1c, c["garbage"])
    e.gestalt, e.gestalt_calls = (c["err"], c["hz"]), []
    T.call_ppc(e, e.func("detect__3CPUFv"), [T.I(obj)])
    return dict(obj_words(e.read(obj, 0x1c)), clock=T.s32(e.u32(CLOCK)), gestalt=e.gestalt_calls)


def detect_host(lib, c):
    clock = I32.from_address(F(lib, "tpw_bfCPU_clock_mhz", VP)())
    clock.value = c["clock"]
    obj = ctypes.create_string_buffer(c["garbage"], 0x1c)
    ctypes.c_int16.in_dll(lib, "tpw_gestalt_err").value = c["err"]
    I32.in_dll(lib, "tpw_gestalt_value").value = c["hz"]
    I32.in_dll(lib, "tpw_gestalt_calls").value = 0
    F(lib, "tpw_bfCPU_detect", None, VP)(obj)
    raw = obj.raw
    native = struct.pack(">IIi", *struct.unpack("<IIi", raw[0:12])) + raw[12:28]
    calls = I32.in_dll(lib, "tpw_gestalt_calls").value
    sel = ctypes.c_uint32.in_dll(lib, "tpw_gestalt_selector").value
    return dict(obj_words(native), clock=clock.value, gestalt=[sel] * calls)


def clocks_ppc(e, c):
    e.write(CLOCK, struct.pack(">i", c["clock"]))
    obj = e.alloc(0x1c)
    r3, _, _ = T.call_ppc(e, e.func("clocks_per_millisec__3CPUFi"), [T.I(obj), T.I(c["arg"])])
    return dict(ret=T.s32(r3))


def clocks_host(lib, c):
    I32.from_address(F(lib, "tpw_bfCPU_clock_mhz", VP)()).value = c["clock"]
    obj = ctypes.create_string_buffer(0x1c)
    return dict(ret=F(lib, "tpw_bfCPU_clocks", I32, VP, I32)(obj, c["arg"]))


HZ = [0, 1, 999999, 1000000, 1999999, 300000000, 450000000, 0x7FFFFFFF, -1, -999999, -1000000, -1000001,
      -0x80000000]


def detect_case(r):
    return dict(clock=r.choice([0, 0, 0, 300, -5, T.s32(r.getrandbits(32))]),
                err=r.choice([0, 0, 0, -5550, 1, -1, 0x7FFF, -0x8000]),
                hz=r.choice(HZ) if r.random() < 0.3 else T.s32(r.getrandbits(32)), garbage=r.randbytes(0x1c))


TESTS = [
    FnTest("CPU::detect (bullfrog)", detect_ppc, detect_host,
           edge=lambda: [dict(clock=k, err=err, hz=hz, garbage=bytes([g]) * 0x1c)
                         for k in (0, 450) for err in (0, -5550) for hz in HZ for g in (0, 0xFF)],
           rnd=detect_case),
    FnTest("CPU::clocks_per_millisec (bullfrog)", clocks_ppc, clocks_host,
           edge=lambda: [dict(clock=k, arg=0) for k in (0, 1, 450, 2147483, 2147484, -1, 0x7FFFFFFF, -0x80000000)],
           rnd=lambda r: dict(clock=T.s32(r.getrandbits(32)) if r.random() < 0.5 else r.randint(-1000, 1000),
                              arg=T.s32(r.getrandbits(32)))),
]
