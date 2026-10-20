"""Unit v5272, the light-node helpers, against libtpw_unit_SetLightColourExplicit.dylib (tests/unit_SetLightColourExplicit.cpp), main app
("SimTheme Park"), src/LevelSystems_SetLightColourExplicit.cpp:
  SetLightColourExplicit 0x1006498c, EnableLight 0x10064ab8, SetLightColourRelative 0x10064854, DisableLight 0x10064b30

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian, 4-byte pointers)
for the original, and in the host library's memory (little-endian, 8-byte pointers, host offsets of CLight) for the translation.
CSystem::UpdateLights (0x10239aa8) is a recording fake on both sides.

The world of a case: a parameter block (light slot pointer, colour pointer), the slot holding the CLight, three source colour floats, and a
CLight with random colour floats, flags and colour word. What is compared: the CLight's four colour floats, flags and colour word, the
source colours, and the event log.

fctiwz is hooked by ROUNDING (Unicorn saturates wrongly). No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_SetLightColourExplicit"
MASK = 0xFFFFFFFF

FN_EXPLICIT, FN_ENABLE, FN_RELATIVE, FN_DISABLE = 0x1006498C, 0x10064AB8, 0x10064854, 0x10064B30
SIZES = {FN_EXPLICIT: 232, FN_ENABLE: 68, FN_RELATIVE: 248, FN_DISABLE: 68}
OWN = set(SIZES)

UPDATE_LIGHTS = 0x10239AA8

# CLight offsets: the Mac's, then the host's (from the layout export)
MAC = dict(color=0x04, flags=0x50, word=0x54, size=0x68, colour_ptr=4, ptr=4)
HOST_LAYOUT_N = 5


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_update_lights(e):
    e.cl.rec("UpdateLights", ())


STUBS.imp("UpdateLights__7CSystemFv", stub_update_lights)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.psize == 4 else "Q"), v))


class Ppc(Side):
    fmt = ">I"
    psize = 4
    off = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, p, fl):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, [p] + [T.F(bitsf(x)) for x in fl])


class Host(Side):
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_light_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, p, fl):
        L = self.lib
        if fn == FN_EXPLICIT:
            L.tpw_light_explicit(p, *[bitsf(x) for x in fl])
        elif fn == FN_RELATIVE:
            L.tpw_light_relative(p, bitsf(fl[0]))
        elif fn == FN_ENABLE:
            L.tpw_light_enable(p)
        else:
            L.tpw_light_disable(p)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_light_alloc.restype, lib.tpw_light_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_light_layout.argtypes = [C.POINTER(C.c_int64)]
    f = C.c_float
    lib.tpw_light_explicit.argtypes = [C.c_uint64, f, f, f]
    lib.tpw_light_relative.argtypes = [C.c_uint64, f]
    lib.tpw_light_enable.argtypes = [C.c_uint64]
    lib.tpw_light_disable.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_light_layout(out)
    assert n == HOST_LAYOUT_N
    lib.off = dict(color=out[0], flags=out[1], word=out[2], size=out[3], colour_ptr=out[4], ptr=8)
    lib.bound = True


def build(s, case):
    o = s.off
    light = s.alloc(o["size"])
    slot = s.alloc(8)
    colour = s.alloc(16)
    params = s.alloc(16)
    s.ptr(slot, light)
    s.ptr(params, slot)
    s.ptr(params + o["colour_ptr"], colour)
    s.words(colour, case["src"])
    s.words(light + o["color"], case["color"])
    s.words(light + o["flags"], [case["flags"]])
    s.words(light + o["word"], [case["word"]])
    s.ctx = (params, light, colour)


def execute(s, case):
    params = s.ctx[0]
    fn = {"explicit": FN_EXPLICIT, "enable": FN_ENABLE, "relative": FN_RELATIVE, "disable": FN_DISABLE}[case["op"]]
    s.call(fn, params, case["f"])


def canon(b):
    return 0x7FC00000 if (b & 0x7F800000) == 0x7F800000 and (b & 0x7FFFFF) else b


def observe(s, case):
    o = s.off
    _, light, colour = s.ctx
    return {
        "log": s.log(),
        "color": [canon(w) for w in s.rd_words(light + o["color"], 4)],
        "flags": s.rd_words(light + o["flags"], 1),
        "word": s.rd_words(light + o["word"], 1),
        "src": [canon(w) for w in s.rd_words(colour, 3)],
    }


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x3B808081, 0x437F0000, 0x437FFFFF, 0x43800000,
           0x43000000, 0xC37F0000, 0xBF000000, 0x3F7FFFFF, 0x3C800000]


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-1.2, 1.2))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 3))


def base(r, op):
    return {"op": op, "queues": {}, "src": [rf(r) for _ in range(3)], "color": [r.getrandbits(32) for _ in range(4)],
            "flags": r.getrandbits(32), "word": r.getrandbits(32), "f": []}


def explicit_case(r, f=None):
    c = base(r, "explicit")
    c["f"] = f if f is not None else [rf(r) for _ in range(3)]
    return c


def relative_case(r, f=None):
    c = base(r, "relative")
    c["f"] = f if f is not None else [rf(r)]
    return c


def flag_case(op):
    def make(r, flags=None):
        c = base(r, op)
        if flags is not None:
            c["flags"] = flags
        return c
    return make


enable_case, disable_case = flag_case("enable"), flag_case("disable")


def explicit_edges():
    r = random.Random(11)
    for v in SPECIAL:
        yield explicit_case(r, [v, fb(0.5), fb(1.0)])
        yield explicit_case(r, [fb(0.5), v, fb(1.0)])
        yield explicit_case(r, [fb(1.0), fb(0.5), v])


def relative_edges():
    r = random.Random(12)
    for v in SPECIAL:
        yield relative_case(r, [v])
    for v in (fb(1.0), fb(0.5), fb(0.0)):
        c = relative_case(r, [v])
        c["src"] = [fb(1.0), fb(0.5), fb(0.25)]
        yield c


def flag_edges(make, seed):
    def edges():
        r = random.Random(seed)
        for fl in (0, 0xFFFFFFFF, 0x10000000, 0xEFFFFFFF, 0x80000000, 1):
            yield make(r, fl)
    return edges


TESTS = [
    FnTest("SetLightColourExplicit", ppc, host, edge=explicit_edges, rnd=explicit_case),
    FnTest("EnableLight", ppc, host, edge=flag_edges(enable_case, 13), rnd=enable_case),
    FnTest("SetLightColourRelative", ppc, host, edge=relative_edges, rnd=relative_case),
    FnTest("DisableLight", ppc, host, edge=flag_edges(disable_case, 14), rnd=disable_case),
]
