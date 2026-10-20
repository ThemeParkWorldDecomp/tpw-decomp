"""Unit v5987, the ride hoardings, against libtpw_unit_SetRideHoardingLevel.dylib (tests/unit_SetRideHoardingLevel.cpp), main app
("SimTheme Park"), src/LevelSystems_SetRideHoardingLevel.cpp:
  SetRideHoardingLevel 0x10061268, EnableRideHoardings 0x10061060, ActivateHoardings 0x100614a0, ReactivateHoardings 0x10061840

A case is a world and a call. The world (a ride animation set, its mesh, the dynamic mesh set with its entry, face set, vertices and UVs,
two mesh headers and nodes, the sine table) is built twice by ONE piece of Python code, with the Mac's offsets and 4 byte pointers on the
PowerPC side and the host library's offsets and 8 byte pointers on the other. SetMatrixIdentity, YRotateLMatrixNoTrans, DMEnableMeshRender
and DMDisableMeshRender are fakes on both sides that record their arguments (SetMatrixIdentity also writes the identity).

What is compared: the set's flags and speed, the face set's texture word, the matrix, the dynamic header's box, the vertices, the UVs and
the event log. Fused operations: HardwareRounding runs over the four functions. Not tested: a count past the vertex block (the original
reads past it too).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_SetRideHoardingLevel"
MASK = 0xFFFFFFFF

FN_SET, FN_ENABLE, FN_ACT, FN_REACT = 0x10061268, 0x10061060, 0x100614A0, 0x10061840
SIZES = {FN_SET: 416, FN_ENABLE: 456, FN_ACT: 752, FN_REACT: 344}
OWN = set(SIZES)

CALLEE_IDENT, CALLEE_YROT, CALLEE_DMEN, CALLEE_DMDIS = 0x100C0E28, 0x100C1698, 0x10030928, 0x10030A48

ADDR = dict(gTPGlobals=0x103B74C8)

MAC = dict(flags=4, heads=8, hoard=0xB0, dm=0xB4, level=0xB8, speed=0xBC, fc0=0xC0, fc4=0xC4, setsize=0x100,
           mhdr=4, mf10=0x10, meshsize=0x40,
           dmf00=0, dmentries=8, dmmesh=0x24, dmvsets=0x10, dmsize=0x40,
           ematrix=0x28, efacesets=0x2C, esize=0x30, pw=4)
HOST = {}


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


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ident(e):
    L = e.cl
    m = e.r(3)
    L.rec("SetMatrixIdentity", (L.lab(m),))
    e.write(m, b"".join(struct.pack(">f", 1.0 if i % 5 == 0 else 0.0) for i in range(16)))


def stub_yrot(e):
    L = e.cl
    L.rec("YRotate", (L.lab(e.r(3)), fb(e.f(1)), fb(e.f(2))))


def stub_dmen(e):
    L = e.cl
    L.rec("DMEnable", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, 1)


def stub_dmdis(e):
    L = e.cl
    L.rec("DMDisable", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, 1)


STUBS.internal(CALLEE_IDENT, stub_ident)
STUBS.internal(CALLEE_YROT, stub_yrot)
STUBS.internal(CALLEE_DMEN, stub_dmen)
STUBS.internal(CALLEE_DMDIS, stub_dmdis)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = STUBS.activate(e)

    def begin(self, labels):
        self.L = self.e.cl = CL.PpcLog(labels, self.case["queues"])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def p32(self, a):
        return a

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def hf(self, a, f00, f02):
        self.words(a, [(f00 << 16) | f02])

    def log(self):
        return self.L.log

    def call(self, fn, args):
        if fn == FN_SET:
            args = [args[0], T.F(bitsf(args[1]))]
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = HOST

    def begin(self, labels):
        CL.host_begin(self.lib, {v: k for k, v in labels.items()}, {})

    def done(self):
        pass

    def p32(self, a):
        return self.lib.tpw_hoard_off(a)

    def g(self, name):
        return self.lib.tpw_hoard_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hoard_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def hf(self, a, f00, f02):
        self.words(a, [f00 | (f02 << 16)])

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        a = args[1] if len(args) > 1 else 0
        if fn == FN_SET:
            self.lib.tpw_hoard_call(0, args[0], 0, bitsf(a))
        else:
            self.lib.tpw_hoard_call({FN_ENABLE: 1, FN_ACT: 2, FN_REACT: 3}[fn], args[0], a & MASK, 0.0)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hoard_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").argtypes = [C.c_int32, C.c_uint64, C.c_uint32, C.c_float]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    v = list(out)[:n]
    names = ["flags", "heads", "hoard", "dm", "level", "speed", "fc0", "fc4", "setsize", "mhdr", "mf10", "meshsize",
             "dmf00", "dmentries", "dmmesh", "dmvsets", "dmsize", "ematrix", "efacesets", "esize"]
    assert n == len(names) + 3, v
    HOST.update(zip(names, v))
    # the Mac-layout words the module relies on: sin table mask, scale, table
    assert v[len(names):] == [4, 8, 0xC], v
    lib.bound = True


def build(s, case):
    o = s.o
    count = case["count"]
    # sine table
    g = s.g("gTPGlobals")
    s.words(g, [0x1000, 0xFFF, fb(4096 / 6.283185307179586)])
    s.words(g + 0xC, [fb(__import__("math").sin(6.283185307179586 * i / 4096)) for i in range(0x1000)])
    # headers and nodes (the file's layout on both sides)
    n1, n2 = s.alloc(0xA0), s.alloc(0xA0)
    h1, h2 = s.alloc(0xB0), s.alloc(0xB0)
    s.words(n1 + 0x10, case["nodemat"])
    uvs = s.alloc(0x20 * max(count, 1) + 0x40)
    s.words(uvs, case["uvs"])
    s.words(n2 + 0x68, [s.p32(uvs)])
    s.words(h1 + 0x50, [case["tex1"]])
    s.words(h1 + 0x78, [s.p32(n1)])
    s.words(h1 + 0x80, case["box1"])
    s.words(h2 + 0x50, [case["tex2"]])
    s.words(h2 + 0x78, [s.p32(n2)])
    s.words(h2 + 0x80, case["box2"])
    m1, m2 = s.alloc(o["meshsize"]), s.alloc(o["meshsize"])
    s.ptr(m1 + o["mhdr"], h1)
    s.words(m1 + o["mf10"], [case["f10"]])
    s.ptr(m2 + o["mhdr"], h2)
    mat = s.alloc(0x40)
    s.words(mat, case["mat"])
    fs = s.alloc(0x20)
    s.words(fs, [case["ftex"]])
    ent = s.alloc(o["esize"] * 2)
    s.ptr(ent + o["ematrix"], mat)
    s.ptr(ent + o["efacesets"], fs)
    vs = s.alloc(0x30 * max(count, 1) + 0x40)
    s.words(vs, case["verts"])
    dm = s.alloc(o["dmsize"])
    s.words(dm + o["dmf00"], [case["dmf00"]])
    s.ptr(dm + o["dmentries"], ent)
    s.ptr(dm + o["dmmesh"], m2)
    s.ptr(dm + o["dmvsets"], vs)
    hb = s.alloc(0x20)
    s.words(hb, [count])
    st = s.alloc(o["setsize"])
    s.words(st + o["flags"], [case["flags"]])
    s.ptr(st + o["heads"], m1)
    if case["hoard"]:
        s.ptr(st + o["hoard"], hb)
    s.ptr(st + o["dm"], dm)
    s.words(st + o["level"], [case["hlevel"]])
    s.words(st + o["speed"], [case["speed"]])
    s.hf(st + o["fc0"], *case["fc0"])
    s.hf(st + o["fc4"], *case["fc4"])
    s.begin({mat: 1, dm: 2})
    return dict(st=st, mat=mat, fs=fs, h2=h2, vs=vs, uvs=uvs, dm=dm)


def observe(s, case, w):
    o = s.o
    n = max(case["count"], 1)
    cf = lambda ws: [canon_f(x) for x in ws]
    return {"log": s.log(), "flags": s.rd_word(w["st"] + o["flags"]), "speed": canon_f(s.rd_word(w["st"] + o["speed"])),
            "tex": s.rd_word(w["fs"]), "mat": cf(s.rd_words(w["mat"], 16)), "box": cf(s.rd_words(w["h2"] + 0x80, 6)),
            "verts": cf(s.rd_words(w["vs"], 12 * n)), "uvs": cf(s.rd_words(w["uvs"], 8 * n)), "dmf00": s.rd_word(w["dm"] + o["dmf00"])}


def run(s, case):
    try:
        w = build(s, case)
        s.call(*case["call"](w))
        return observe(s, case, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000]


def rf(r):
    k = r.random()
    if k < 0.05:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-2, 2))
    return fb(r.uniform(-300, 300))


def rw(r):
    return r.randrange(0, 1 << 32)


def base(r, op, **kw):
    count = kw.get("count", r.randrange(0, 13))
    c = {"op": op, "queues": {}, "count": count,
         "flags": kw.get("flags", r.choice([0, 2, 0x22, 0x62, 0xA2, 0x122, 0x222, 0x422, 0x822, 0xF22, 0x2, 0x2 | rw(r) & 0xFFFE]) & MASK),
         "hoard": kw.get("hoard", r.random() < 0.9), "f10": kw.get("f10", r.choice([0, 1, -1, r.randint(-100000, 100000)])),
         "tex1": rw(r), "tex2": rw(r), "ftex": rw(r), "dmf00": rw(r), "speed": rf(r),
         "hlevel": kw.get("hlevel", fb(r.choice([0.0, 0.25, 0.5, 1.0, r.uniform(-1, 2)]))),
         "box1": [rf(r) for _ in range(6)], "box2": [rf(r) for _ in range(6)], "mat": [rf(r) for _ in range(16)],
         "nodemat": [rf(r) for _ in range(16)], "verts": [rf(r) for _ in range(12 * max(count, 1))],
         "uvs": [rf(r) for _ in range(8 * max(count, 1))],
         "fc0": (r.randrange(0, 65536), r.randrange(0, 65536)), "fc4": (r.randrange(0, 65536), r.randrange(0, 65536))}
    return c


def set_case(r, level=None, **kw):
    kw["hoard"] = True   # the original reads **mHoardingMesh without a NULL check (not tested)
    c = base(r, "set", **kw)
    lv = rf(r) if level is None else level
    c["call"] = lambda w: (FN_SET, [w["st"], lv])
    return c


def enable_case(r, n=None, **kw):
    c = base(r, "enable", **kw)
    n = r.choice([0, 1, -1, 5]) if n is None else n
    c["call"] = lambda w: (FN_ENABLE, [w["st"], n & MASK])
    return c


def act_case(r, flag=None, **kw):
    c = base(r, "act", **kw)
    flag = r.choice([0, 1, 2, 4, 8, 3, 16]) if flag is None else flag
    c["call"] = lambda w: (FN_ACT, [w["st"], flag])
    return c


def react_case(r, **kw):
    c = base(r, "react", **kw)
    c["call"] = lambda w: (FN_REACT, [w["st"]])
    return c


def mk(fn, seed):
    def edges():
        r = random.Random(seed)
        for flags in (0, 2, 0x22, 0x62, 0xA2, 0x122, 0x222, 0x422, 0x822, 0x182, 0x3E2):
            for hoard in (True, False):
                for count in (0, 1, 2, 5):
                    yield fn(r, flags=flags, hoard=hoard, count=count)

    def rnd(r):
        return fn(r)

    return edges, rnd


def set_edges():
    r = random.Random(11)
    for lv in (0.0, 0.25, 0.5, 1.0, 1.25, -0.5, 2.0, 0.75, 0.125, 1.5, 3.0):
        for count in (0, 1, 2, 3, 8):
            yield set_case(r, fb(lv), flags=2, hoard=True, count=count)
    for c in (0, 1):
        yield set_case(r, fb(0.5), flags=c, count=3)


def en_edges():
    r = random.Random(12)
    for n in (0, 1, -1):
        for f10 in (0, 1, -1, 90, 360, -360, 1000, 100000):
            for flags in (2, 0, 0x22, 0x102):
                yield enable_case(r, n, flags=flags, f10=f10)


def act_edges():
    r = random.Random(13)
    for flag in (0, 1, 2, 4, 8, 3, 16):
        for flags in (0, 0x20, 0x80, 0x100, 0x200, 0x400, 0x800, 0x180, 0x280, 0x480, 0x880, 0xF00, 0x2A0):
            for hoard in (True, False):
                yield act_case(r, flag, flags=flags | 2, hoard=hoard, count=r.randrange(0, 4))


def react_edges():
    r = random.Random(14)
    for flags in (0, 0x20, 0x60, 0xA0, 0x20 | 0x100, 0x20 | 0x200, 0x400, 0x800, 0x860, 0xFA0, 0x100, 0x200, 0x2, 0x22, 0x62, 0xA2):
        for hoard in (True, False):
            yield react_case(r, flags=flags | (r.choice([0, 2])), hoard=hoard, count=r.randrange(0, 4))
            yield react_case(r, flags=flags | 2, hoard=hoard, count=r.randrange(0, 4))


TESTS = [
    FnTest("SetRideHoardingLevel", ppc, host, edge=set_edges, rnd=lambda r: set_case(r)),
    FnTest("EnableRideHoardings", ppc, host, edge=en_edges, rnd=lambda r: enable_case(r)),
    FnTest("ActivateHoardings", ppc, host, edge=act_edges, rnd=lambda r: act_case(r)),
    FnTest("ReactivateHoardings", ppc, host, edge=react_edges, rnd=lambda r: react_case(r)),
]
