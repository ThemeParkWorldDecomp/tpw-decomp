"""Unit v5092, the engine's machine detection, against libtpw_unit_SMachineParameters_Output.dylib
(tests/unit_SMachineParameters_Output.cpp), engine program, src/engine/tb_init.cpp:
  SMachineParameters::Output 0x10013d3c, SMachineParameters::Suggest 0x10013eec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides that record their arguments: fopen, fprintf (every argument, strings and the format as
a hash, doubles as two words), fflush, fclose, the render system's GetVRAMSize and the draw primitive's GetPixelFillRate (virtual
calls through a hand-built vtable and TVector on the PowerPC side), CTextureCache::TestUploadSpeed, CTransformBase::TestMathSpeed and
SMachineParameters::Return (the record as its offset in the data table).

The world of a case: the parameter block, gei_System's clock, memory, renderer and field 0x20, the device description in gei_DDraw, the
output counter, the answer queues. What is compared: the answer, the parameter block, the counter and the event log.

No fused operations. Not tested: a gei_DDraw, gei_TCache, gei_MathUnit, gei_DrawPrimitive or render system that is NULL (the original
reads them unconditionally).
"""
import re
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_SMachineParameters_Output"
MASK = 0xFFFFFFFF

FN_OUT, FN_SUG = 0x10013D3C, 0x10013EEC
SIZES = {FN_OUT: 380, FN_SUG: 668}
OWN = set(SIZES)

CALLEE_RETURN = 0x10013C2C
CALLEE_UPLOAD = 0x100339C8
CALLEE_MATH = 0x1001B000
FAKE_VRAM = 0x10013BBC       # the address the fake render system's GetVRAMSize slot points to (a function the unit never calls)
FAKE_FILL = 0x10003890       # the same for GetPixelFillRate
TOC = 0x10057470
SYS = 0x10165F80

ADDR = dict(
    **{"sys.mClocksPerMs": SYS + 0x14, "sys.mTotalPhys": SYS + 0x18, "sys.mRenderer": SYS + 0x1C, "sys.mF20": SYS + 0x20,
       "sys.mRenderSystem": SYS + 0x24},
    ddraw=0x10052AD0, tcache=0x10173184, mathunit=0x100AFE94, prim=0x10052B3C, count=0x100AF124)
HOST_LAYOUT = [0x10, 0x24, 0x28]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def fnv(b):
    h = 0x811C9DC5
    for c in b:
        h = ((h ^ c) * 0x01000193) & MASK
    return h


def cbytes(s):
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_fopen(e):
    L = e.cl
    L.rec("fopen", (fnv(cbytes(e.cstr(e.r(3)))), fnv(cbytes(e.cstr(e.r(4))))))
    e.set_r(3, L.ret("fopen") & MASK)


def stub_fprintf(e):
    L = e.cl
    fmt = cbytes(e.cstr(e.r(4)))
    specs = re.findall(rb"%[a-z]", fmt)
    L.rec("fprintf", (e.r(3) & MASK, fnv(fmt), len(specs)))
    gp = 5
    for sp in specs:
        if sp == b"%s":
            L.rec("arg", (1, fnv(cbytes(e.cstr(e.r(gp)))), 0))
            gp += 1
        elif sp == b"%d":
            L.rec("arg", (2, e.r(gp) & MASK, 0))
            gp += 1
        else:
            hi, lo = e.r(gp) & MASK, e.r(gp + 1) & MASK
            if (hi & 0x7FF00000) == 0x7FF00000 and ((hi & 0xFFFFF) or lo):
                hi, lo = 0x7FF80000, 0
            L.rec("arg", (3, hi, lo))
            gp += 2
    e.set_r(3, 0)


def stub_fflush(e):
    e.cl.rec("fflush", (e.r(3) & MASK,))
    e.set_r(3, 0)


def stub_fclose(e):
    e.cl.rec("fclose", (e.r(3) & MASK,))
    e.set_r(3, 0)


def stub_vram(e):
    L = e.cl
    L.rec("GetVRAMSize", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetVRAMSize") & MASK)


def stub_fill(e):
    L = e.cl
    L.rec("GetPixelFillRate", (L.lab(e.r(3)),))
    e.set_f1(bitsf(L.ret("GetPixelFillRate")))


def stub_upload(e):
    L = e.cl
    L.rec("TestUploadSpeed", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))
    e.set_f1(bitsf(L.ret("TestUploadSpeed")))


def stub_math(e):
    L = e.cl
    L.rec("TestMathSpeed", (L.lab(e.r(3)),))
    e.set_f1(bitsf(L.ret("TestMathSpeed")))


def stub_return(e):
    L = e.cl
    L.rec("Return", (L.lab(e.r(3)), (e.r(4) - (e.r(2) - 0x5C74)) & MASK, T.s32(e.r(5))))
    e.set_r(3, L.ret("Return") & MASK)


STUBS.imp("fopen", stub_fopen)
STUBS.imp("fprintf", stub_fprintf)
STUBS.imp("fflush", stub_fflush)
STUBS.imp("fclose", stub_fclose)
STUBS.internal(FAKE_VRAM, stub_vram)
STUBS.internal(FAKE_FILL, stub_fill)
STUBS.internal(CALLEE_UPLOAD, stub_upload)
STUBS.internal(CALLEE_MATH, stub_math)
STUBS.internal(CALLEE_RETURN, stub_return)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x40)
        self.tc = e.alloc(0x40)
        self.mu = e.alloc(0x40)
        self.dd = e.alloc(0x400)
        # gei_DrawPrimitive points at the subobject whose vtable pointer is at +0x2c; slot 0x40 is GetPixelFillRate
        self.prim = e.alloc(0x40)
        vt = e.alloc(0x80)
        tv = e.alloc(8)
        self.words(tv, [FAKE_FILL, TOC])
        self.words(vt + 0x40, [tv])
        self.words(self.prim + 0x2C, [vt])
        # the render system: vtable pointer at 0, slot 0x14 is GetVRAMSize
        self.rs = e.alloc(0x40)
        vt = e.alloc(0x80)
        tv = e.alloc(8)
        self.words(tv, [FAKE_VRAM, TOC])
        self.words(vt + 0x14, [tv])
        self.words(self.rs, [vt])
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.tc: 2, self.mu: 3, self.prim: 4, self.rs: 5}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        fn = FN_OUT if op == "out" else FN_SUG
        r3, _, _ = T.call_ppc(self.e, fn, [self.obj])
        return r3 & MASK if op == "sug" else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = self.alloc(0x40)
        self.tc = self.alloc(0x40)
        self.mu = self.alloc(0x40)
        self.dd = self.alloc(0x400)
        self.prim = self.g("o.prim")
        self.rs = self.g("o.rs")
        CL.host_begin(lib, {1: self.obj, 2: self.tc, 3: self.mu, 4: self.prim, 5: self.rs}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_smp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_smp_alloc(n)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def read(self, a, n):
        import ctypes as C
        return C.string_at(a, n)

    def write(self, a, data):
        import ctypes as C
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        if op == "out":
            self.lib.tpw_smp_output(self.obj)
            return 0
        return self.lib.tpw_smp_suggest(self.obj) & MASK


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_smp_addr.restype, lib.tpw_smp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_smp_alloc.restype, lib.tpw_smp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_smp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_smp_output.restype, lib.tpw_smp_output.argtypes = None, [C.c_uint64]
    lib.tpw_smp_suggest.restype, lib.tpw_smp_suggest.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_smp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("sys.mClocksPerMs"), [case["sys"][0]])
    s.words(s.g("sys.mTotalPhys"), [case["sys"][1]])
    s.words(s.g("sys.mRenderer"), [case["sys"][2]])
    s.words(s.g("sys.mF20"), [case["sys"][3]])
    s.ptr(s.g("sys.mRenderSystem"), s.rs)
    s.ptr(s.g("ddraw"), s.dd)
    s.ptr(s.g("tcache"), s.tc)
    s.ptr(s.g("mathunit"), s.mu)
    s.ptr(s.g("prim"), s.prim)
    s.words(s.g("count"), [case["count"]])
    s.write(s.dd + 0x21C, case["desc"] + b"\0")
    s.words(s.obj, case["p"])


def observe(s, case, answer):
    p = s.rd_words(s.obj, 10)
    for i in (5, 8, 9):
        p[i] = canon_f(p[i])
    return {"answer": answer, "log": s.log(), "p": p, "count": s.rd_word(s.g("count")),
            "sys": [s.rd_word(s.g(n)) for n in ("sys.mClocksPerMs", "sys.mTotalPhys", "sys.mRenderer", "sys.mF20")]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
INTS = [0, 1, 2, 999, 1000, 1001, 1023, 1024, 1025, 2000, -1, -999, -1000, -1001, -1024, -2001, 0x7FFFFFFF, -0x80000000,
        0x12345678, -0x12345678]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-3000, 3000))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rq(r):
    """A float answer for a queue: never a signalling NaN."""
    return rf(r)


def ri(r):
    k = r.random()
    return (r.choice(INTS) if k < 0.5 else r.randrange(0, 1 << 32)) & MASK


def base(r, op):
    p = [ri(r) for _ in range(10)]
    for i in (5, 8, 9):
        p[i] = rf(r)
    return {"op": op,
            "queues": {"fopen": [r.choice([0, 0x12345678, 0x7000])], "GetVRAMSize": [ri(r)], "TestUploadSpeed": [rq(r)],
                       "TestMathSpeed": [rq(r)], "GetPixelFillRate": [rq(r)],
                       "Return": [r.choice([0, 1, 2, 4, 0x80000000, MASK, ri(r)]) for _ in range(16)]},
            "sys": [ri(r), ri(r), r.choice([0, 1, 3, 4, 5, ri(r)]), r.choice([0, 1, 5, ri(r)])],
            "count": r.choice([0, 0, 1, 2, ri(r)]),
            "desc": bytes(r.randrange(32, 127) for _ in range(r.randrange(0, 60))),
            "p": p}


def out_edges():
    r = __import__("random").Random(5)
    for count in (0, 1, 0xFFFFFFFF):
        for fo in (0, 0x12345678):
            c = base(r, "out")
            c["count"] = count
            c["queues"]["fopen"] = [fo]
            yield c
    for sp in SPECIAL:
        for i in (5, 8, 9):
            c = base(r, "out")
            c["p"][i] = sp
            c["count"] = 0
            c["queues"]["fopen"] = [0x777]
            yield c
    c = base(r, "out")
    c["desc"] = b""
    c["queues"]["fopen"] = [0x777]
    yield c


def out_random(r):
    return base(r, "out")


def sug_edges():
    r = __import__("random").Random(6)
    for v in INTS:
        c = base(r, "sug")
        c["sys"][0] = v & MASK
        yield c
        c = base(r, "sug")
        c["sys"][1] = v & MASK
        yield c
        c = base(r, "sug")
        c["queues"]["GetVRAMSize"] = [v & MASK]
        yield c
    for fo in (0, 0x777):
        for count in (0, 1):
            c = base(r, "sug")
            c["queues"]["fopen"] = [fo]
            c["count"] = count
            yield c
    for sp in SPECIAL:
        c = base(r, "sug")
        for k in ("TestUploadSpeed", "TestMathSpeed", "GetPixelFillRate"):
            c["queues"][k] = [sp]
        yield c


def sug_random(r):
    return base(r, "sug")


TESTS = [
    FnTest("SMachineParameters::Output", ppc, host, edge=out_edges, rnd=out_random),
    FnTest("SMachineParameters::Suggest", ppc, host, edge=sug_edges, rnd=sug_random),
]
