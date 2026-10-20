"""Unit v5674, the ride hoardings' height offsets, against libtpw_unit_SetHoardingYOffsets.dylib (tests/unit_SetHoardingYOffsets.cpp), main app
("SimTheme Park"), src/LevelSystems_SetHoardingYOffsets.cpp:
  SetHoardingYOffsets 0x10060c80, GetRotatedHFieldCoord 0x10060ab8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The one callee outside
the unit, FloatToCell (0x10237cd8), is a fake on both sides that records its argument (the double's bits) and answers from the case's queue.

The world of a case (SetHoardingYOffsets): a ride set (flags, mesh with the rotation angle, hoarding geometry with its block of corner
coordinates, two corner pairs, a mesh set with the vertex sets), the height field header global with its height field and heights, the node
whose +0x44 float is the base height. Compared: the vertex sets written, the event log. GetRotatedHFieldCoord: the two outputs (preset to a
sentinel) and the event log.

Every float expression with a fused multiply-add runs under ROUNDING (hardware single rounding). Not tested: a height index past the height
array and a NULL mesh, header or geometry (the original reads them unconditionally).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_SetHoardingYOffsets"
MASK = 0xFFFFFFFF

FN_ROT, FN_SET = 0x10060AB8, 0x10060C80
SIZES = {FN_ROT: 400, FN_SET: 932}
OWN = set(SIZES)

CELL = 0x10237CD8

ADDR = dict(gHeightfieldHeader=0x102C6C7C)

# the struct offsets the module relies on, in the order tpw_sh_layout answers them (the Mac's, then the host checks nothing: it supplies its own)
NAMES = ["set_flags", "set_heads", "set_geom", "set_dms", "set_fc0", "set_fc4", "mesh_header", "mesh_f10", "geom_block", "dms_vs",
         "vsize", "hdr_f78", "hdr_hfield", "hf_f18", "hf_heights"]
MAC = [4, 8, 0xB0, 0xB4, 0xC0, 0xC4, 4, 0x10, 0xC, 0x10, 0x30, 0x78, 0x6C, 0x18, 0x28]

HEIGHTS_W = 4          # the height field is 5 x 5 cells; the cells the fake answers are 0..4
HEIGHTS_N = 25


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fake

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_cell(e):
    """int32_t FloatToCell(double): f1 in, r3 out."""
    L = e.cl
    bits = T.f64_bits(e.f(1))
    if (bits >> 52) & 0x7FF == 0x7FF and bits & ((1 << 52) - 1):
        bits = 0x7FF8000000000000
    if bits >= 1 << 63:
        bits -= 1 << 64
    L.rec("Cell", (bits,))
    e.set_r(3, L.ret("Cell") & MASK)


STUBS.internal(CELL, stub_cell)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def floats(self, a, xs):
        self.words(a, [fb(x) if not isinstance(x, int) else x for x in xs])


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    o = dict(zip(NAMES, MAC))

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def p32(self, a):
        return a

    def ptr(self, at, a):
        self.write(at, struct.pack(self.pfmt, a))

    def call_set(self, a):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_SET, [a & MASK])

    def call_rot(self, angle, fs, px, pz):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_ROT, [angle & MASK] + [T.F(bitsf(f)) for f in fs] + [px, pz])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.offsets
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, a):
        return self.lib.tpw_sh_off(a)

    def ptr(self, at, a):
        self.write(at, struct.pack(self.pfmt, a))

    def call_set(self, a):
        self.lib.tpw_sh_set(a)

    def call_rot(self, angle, fs, px, pz):
        self.lib.tpw_sh_rot(s32(angle), *[bitsf(f) for f in fs], px, pz)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sh_addr.restype, lib.tpw_sh_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sh_alloc.restype, lib.tpw_sh_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sh_off.restype, lib.tpw_sh_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sh_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sh_set.restype, lib.tpw_sh_set.argtypes = None, [C.c_uint64]
    lib.tpw_sh_rot.restype = None
    lib.tpw_sh_rot.argtypes = [C.c_int32] + [C.c_float] * 6 + [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sh_layout(out)
    got = list(out)[:n]
    assert n == len(NAMES)
    lib.offsets = dict(zip(NAMES, got))
    assert lib.offsets["vsize"] == 0x30
    for k in ("hdr_f78", "hdr_hfield", "hf_f18", "hf_heights"):
        assert lib.offsets[k] == dict(zip(NAMES, MAC))[k], k
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_set(s, case):
    o = s.o
    count = len(case["corners"])
    st = s.alloc(0x200)
    s.words(st + o["set_flags"], [case["flags"]])
    s.words(st + o["set_fc0"], [(case["fc0"][0] << 16) | case["fc0"][1]] if s.fmt == ">I" else [])
    if s.fmt == "<I":
        s.write(st + o["set_fc0"], struct.pack("<HH", *case["fc0"]))
        s.write(st + o["set_fc4"], struct.pack("<HH", *case["fc4"]))
    else:
        s.write(st + o["set_fc4"], struct.pack(">HH", *case["fc4"]))
    # mesh -> header -> node
    node = s.alloc(0x100)
    s.words(node + 0x44, [fb(case["base"])])
    hdr = s.alloc(0x100)
    s.words(hdr + o["hdr_f78"], [s.p32(node)])
    if case["hfield"]:
        hf = s.alloc(0x40)
        heights = s.alloc(4 * HEIGHTS_N)
        s.words(heights, [fb(h) for h in case["heights"]])
        s.words(hf + o["hf_f18"], [case["hwidth"]])
        s.words(hf + o["hf_heights"], [s.p32(heights)])
        s.words(hdr + o["hdr_hfield"], [s.p32(hf)])
    mesh = s.alloc(0x40)
    s.ptr(mesh + o["mesh_header"], hdr)
    s.words(mesh + o["mesh_f10"], [case["angle"]])
    s.ptr(st + o["set_heads"], mesh)
    # geometry
    geom = s.alloc(0x20)
    s.words(geom, [count, case["g1"], case["g2"]])
    block = s.alloc(4 * 2 * count + 8)
    for k, (x, z) in enumerate(case["corners"]):
        s.write(block + 4 * k, struct.pack(s.fmt[0] + "HH", x, z))
    s.ptr(geom + o["geom_block"], block)
    s.ptr(st + o["set_geom"], geom)
    # the mesh set
    dms = s.alloc(0x40)
    vs = s.alloc(0x30 * 4)
    s.ptr(dms + o["dms_vs"], vs)
    s.ptr(st + o["set_dms"], dms)
    s.floats(vs, case["vinit"])
    s.words(s.g("gHeightfieldHeader"), [0])
    s.ptr(s.g("gHeightfieldHeader"), hdr if case["header"] else 0)
    return st, vs


def observe_set(s, case):
    st, vs = build_set(s, case)
    s.call_set(st)
    return {"vs": [canon_f(w) for w in s.rd_words(vs, 12 * 4)], "log": s.log()}


def observe_rot(s, case):
    px, pz = s.alloc(4), s.alloc(4)
    s.words(px, [0xDEADBEEF])
    s.words(pz, [0xCAFEF00D])
    s.call_rot(case["angle"], case["fs"], px, pz)
    return {"x": s.rd_word(px) if hasattr(s, "rd_word") else s.rd_words(px, 1)[0], "z": s.rd_words(pz, 1)[0], "log": s.log()}


def run(s, case):
    try:
        return observe_set(s, case) if case["op"] == "set" else observe_rot(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
ANGLES = [0, 90, 180, 270, 1, 45, 89, 91, 179, 181, 269, 271, 360, -90, -1, 0x7FFFFFFF]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rfrac(r):
    return fb(r.choice([0.0, 1.0, 0.5, r.random(), r.random(), r.uniform(-0.2, 1.2)]))


def rot_case(r, angle=None):
    c = {"op": "rot", "angle": r.choice(ANGLES) if angle is None else angle,
         "queues": {"Cell": [r.choice([0, 1, 5, 0xFFFFFFFF, r.randrange(0, 1 << 32)]) for _ in range(4)]}}
    c["fs"] = [rf(r) for _ in range(4)] + [rfrac(r), rfrac(r)]
    return c


def set_case(r, angle=None, count=None, flags=None, header=True, hfield=True):
    count = r.choice([0, 1, 2, 3]) if count is None else count
    c = {"op": "set", "angle": r.choice(ANGLES) if angle is None else angle,
         "flags": r.choice([2, 2, 2, 3, 0, 1, 0x20, 0xFFFFFFFF]) if flags is None else flags,
         "header": header, "hfield": hfield, "hwidth": HEIGHTS_W,
         "base": r.uniform(-5, 5), "g1": r.choice([1, 4, 4, 8, 10, 0]), "g2": r.choice([1, 4, 4, 8, 10, 0]),
         "fc0": [r.randrange(0, 65536) if r.random() < 0.3 else r.randrange(0, 40) for _ in range(2)],
         "fc4": [r.randrange(0, 65536) if r.random() < 0.3 else r.randrange(0, 40) for _ in range(2)],
         "heights": [r.uniform(-20, 20) for _ in range(HEIGHTS_N)],
         "corners": [(r.randrange(0, 5), r.randrange(0, 5)) for _ in range(count * 2)],
         "vinit": [r.uniform(-3, 3) for _ in range(12)] + [r.uniform(-3, 3) for _ in range(36)],
         "queues": {"Cell": [r.randrange(0, 5) for _ in range(40)]}}
    c["corners"] = c["corners"][:count * 2]
    c["count"] = count
    return c


def set_build_fix(c):
    return c


def edges():
    r = random.Random(5674)
    for a in ANGLES:
        for count in (1, 3):
            yield set_case(r, a, count, flags=2)
    yield set_case(r, 0, 0, flags=2)
    yield set_case(r, 0, 2, flags=0)
    yield set_case(r, 0, 2, flags=1)
    yield set_case(r, 0, 2, flags=2, header=False)
    yield set_case(r, 0, 2, flags=2, hfield=False)
    yield set_case(r, 90, 2, flags=2, header=False, hfield=False)


def rnd(r):
    return set_case(r, flags=2) if r.random() < 0.8 else set_case(r)


def rot_edges():
    r = random.Random(7)
    for a in ANGLES:
        yield rot_case(r, a)
        yield rot_case(r, a)


def rot_rnd(r):
    return rot_case(r)


TESTS = [
    FnTest("SetHoardingYOffsets", ppc, host, edge=edges, rnd=rnd),
    FnTest("GetRotatedHFieldCoord", ppc, host, edge=rot_edges, rnd=rot_rnd),
]
