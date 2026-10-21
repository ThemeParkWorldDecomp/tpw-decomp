"""Unit v5202, the finance/park graph helpers, against libtpw_unit_SetupGraph.dylib (tests/unit_SetupGraph.cpp), main app
("SimTheme Park"), src/game/ui/ui_graph.cpp:
  SetupGraph 0x101f6480, GraphLine 0x101f62cc (float history), DrawMeGraph 0x101f66c0, GraphLine 0x101f6124 (int history)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the CUIGrapher
methods, LbPalette_FindColour, the history methods, the render-area calls, the surface calls, the callback glue) is a fake on both sides
that records its arguments and answers from the case.

The world of a case: the screen size globals, the rectangle, the history answers, the colour block, the graph buffer (zeroed). What is
compared: the event log and the whole graph buffer (160 words).

fctiwz of the float GraphLine runs under HardwareRounding. Not tested: float values beyond the int range (fctiwz saturation differs in
Unicorn), NaN.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_SetupGraph"
MASK = 0xFFFFFFFF

FN_SETUP, FN_GLF, FN_DRAW, FN_GLI = 0x101F6480, 0x101F62CC, 0x101F66C0, 0x101F6124
SIZES = {FN_SETUP: 512, FN_GLF: 360, FN_DRAW: 320, FN_GLI: 348}
OWN = set(SIZES)

ADDR = dict(h=0x10395BBA, w=0x10395BBC, pal=0x7D3F9400,   # the data import _lbGlobalPalette (TOC slot 0x103bcd28) is relocated here in the emulator
 buf=0x10396480)
HOST_LAYOUT = [5, 4]
BUFWORDS = 160


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def col(e, r4, r5):
    return (r4 & MASK, (r5 >> 24) & 0xFF)


def style(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),) + col(e, e.r(4), e.r(5)))
    return f


def stub_draw_area(e):
    L = e.cl
    L.rec("SetDrawArea", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def rng(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    return f


def simple(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
    return f


def stub_find(e):
    L = e.cl
    L.rec("FindColour", (L.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
    e.set_r(3, L.ret("FindColour") & 0xFF)


def stub_draw_graph(e):
    L = e.cl
    L.rec("DrawGraph", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["buf"] else 0, s32(e.r(5)), s32(e.r(6))) + col(e, e.r(7), e.r(8)))


def stub_how_far(e):
    L = e.cl
    L.rec("HowFarBack", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("HowFarBack") & MASK)


def stub_get_value(e):
    L = e.cl
    back = s32(e.r(5))
    L.rec("GetValue", (L.lab(e.r(3)), back))
    idx = getattr(L, "gcount", 0)
    L.gcount = idx + 1
    ok = L.ret("GetOk:%d" % (idx // 32))
    v = L.ret("GetValue:%d" % (idx // 32))
    if ok & 0xFF:
        e.write(e.r(4), struct.pack(">I", v & MASK))
    e.set_r(3, ok & 0xFF)


def stub_surface(e):
    L = e.cl
    L.rec("GetSurface", (L.lab(e.r(3)),))
    e.write(e.r(4), struct.pack(">I", L.ret("w") & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("h") & MASK))
    e.set_r(3, L.ret("surface") & MASK)


def stub_make(e):
    L = e.cl
    L.rec("MakeRenderArea", (e.r(3) & MASK, s32(e.r(5)), s32(e.r(6))))


def stub_rect(e):
    L = e.cl
    rect = struct.unpack(">4i", e.read(e.r(3), 16))
    L.rec("RectFilled", tuple(rect) + col(e, e.r(4), e.r(5)))


def stub_ctor(e):
    e.cl.rec("RenderAreaCtor", ())


def stub_setcur(e):
    e.cl.rec("SetCurrent", ())


STUBS.internal(0x101FD818, stub_draw_area)
STUBS.internal(0x101FD880, style("SetXAxisStyle"))
STUBS.internal(0x101FD8D4, style("SetYAxisStyle"))
STUBS.internal(0x101FD97C, style("SetBGStyle"))
STUBS.internal(0x101FD928, style("SetGridStyle"))
STUBS.internal(0x101FD7D8, rng("SetYRange"))
STUBS.internal(0x101FD798, rng("SetXRange"))
STUBS.internal(0x101FD9D0, simple("Clear"))
STUBS.internal(0x101FDA4C, simple("DrawAxes"))
STUBS.internal(0x101FDB1C, simple("DrawGrid"))
STUBS.internal(0x101FDDBC, stub_draw_graph)
STUBS.imp("LbPalette_FindColour__FPC9TbPaletteUcUcUc", stub_find)
STUBS.internal(0x101F69D8, stub_how_far)
STUBS.internal(0x1001C254, stub_how_far)
STUBS.internal(0x101F6854, stub_get_value)
STUBS.internal(0x1001C2A8, stub_get_value)
STUBS.internal(0x101DBC98, stub_surface)
STUBS.internal(0x101DBD1C, simple("Finish"))
STUBS.internal(0x1018B360, stub_make)
STUBS.imp("LbDraw_RectangleFilled__FRC6TbRect8TbColour", stub_rect)
STUBS.imp("__ct__12TbRenderAreaFv", stub_ctor)
STUBS.imp("LbDraw_SetCurrent__FP12TbRenderArea", stub_setcur)
STUBS.internal(0x1023BAA8, simple("Callback"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.grapher, self.window, self.hist, self.colour, self.rect, self.cb = [e.alloc(32) for _ in range(6)]
        self.L = e.cl = CL.PpcLog({ADDR["pal"]: 1, self.grapher: 2, self.window: 3, self.hist: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        with ROUNDING(self.e):
            T.call_ppc(self.e, entry, args)

    def setup(self, a):
        l, t, r, b = a[:4]
        self.words(self.rect + 4, [((l & 0xFFFF) << 16) | (t & 0xFFFF), ((r & 0xFFFF) << 16) | (b & 0xFFFF)])
        self.call(FN_SETUP, [self.grapher, self.rect] + [x & MASK for x in a[4:]])

    def gl(self, entry, step, mode):
        self.call(entry, [self.grapher, self.hist, self.colour, step & MASK, mode & MASK])

    def draw(self, cb):
        self.words(self.cb, [0x1000, 0x2000])
        self.call(FN_DRAW, [self.window, self.grapher, self.cb if cb else 0])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.grapher, self.window, self.hist, self.colour, self.rect, self.cb = [lib.tpw_v5202_alloc(32) for _ in range(6)]
        CL.host_begin(lib, {1: self.g("pal"), 2: self.grapher, 3: self.window, 4: self.hist}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5202_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup(self, a):
        self.lib.tpw_v5202_setup(self.grapher, *[s32(x) for x in a])

    def gl(self, entry, step, mode):
        fn = self.lib.tpw_v5202_graphline_f if entry == FN_GLF else self.lib.tpw_v5202_graphline_i
        fn(self.grapher, self.hist, self.colour, s32(step), s32(mode))

    def draw(self, cb):
        self.lib.tpw_v5202_drawme(self.window, self.grapher, 1 if cb else 0)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5202_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setup").argtypes = [C.c_uint64] + [C.c_int32] * 10
    for n in ("graphline_f", "graphline_i"):
        getattr(lib, p + n).argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    getattr(lib, p + "drawme").argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("buf"), [0] * BUFWORDS)
    s.write(s.g("h"), struct.pack(">h" if s.fmt == ">I" else "<h", case["H"]))
    s.write(s.g("w"), struct.pack(">h" if s.fmt == ">I" else "<h", case["W"]))
    s.write(s.colour, bytes(case["colour"]))


def execute(s, case):
    op = case["op"]
    if op == "setup":
        s.setup(case["args"])
    elif op == "glf":
        s.gl(FN_GLF, *case["args"])
    elif op == "gli":
        s.gl(FN_GLI, *case["args"])
    else:
        s.draw(case["cb"])


def observe(s, case):
    return {"log": s.log(), "buf": s.rd_words(s.g("buf"), BUFWORDS), "colour": s.read(s.colour, 8)}


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


def i16(r):
    return r.choice([0, 1, -1, 32767, -32768, 1536, -1536, 2048, -2048, r.randrange(-32768, 32768), r.randrange(-300, 300)])


def base(r, op):
    return {
        "op": op, "H": i16(r), "W": i16(r), "colour": [r.randrange(256) for _ in range(8)], "cb": r.choice([0, 1]),
        "queues": {"FindColour": [r.randrange(256) for _ in range(4)], "HowFarBack": [0], "GetOk": [1], "GetValue": [0],
                   "w": [r.randrange(0, 800)], "h": [r.randrange(0, 600)], "surface": [r.choice([0, 0x20000, 0x7FFF0000])]},
    }


def vals_f(r, n):
    out = []
    for _ in range(n):
        k = r.random()
        if k < 0.3:
            out.append(fb(float(r.randrange(-1000, 1000))))
        elif k < 0.5:
            out.append(fb(r.uniform(-1.5, 1.5)))
        elif k < 0.9:
            out.append(fb(r.uniform(-1e6, 1e6)))
        else:
            out.append(fb(r.uniform(-2e9, 2e9)))
    return out


def gl_case(r, op, mode=None, how=None, step=None):
    c = base(r, op)
    mode = r.choice([-1, 0, 1, 2, 3, 100, r.randrange(-5, 6)]) if mode is None else mode
    step = r.choice([0, 1, -1, 2, 5, r.randrange(-3, 4)]) if step is None else step
    limit = {0: 12, 1: 36, 2: 144}.get(mode, 0)
    how = r.choice([-1, -7, 0, 1, limit - 1, limit, limit + 1, 200, r.randrange(-3, 150)]) if how is None else how
    c["args"] = [step, mode]
    q = c["queues"]
    q["HowFarBack"] = [how & MASK]
    vals = vals_f(r, 150) if op == "glf" else [r.choice([r.randrange(0, 2 ** 32), 0, 0x80000000, 0x7FFFFFFF, MASK]) for _ in range(150)]
    for g in range(5):   # the host queues hold 64 entries at most: one queue per 32 calls
        q["GetOk:%d" % g] = [1 if r.random() < 0.85 else 0 for _ in range(32)]
        q["GetValue:%d" % g] = vals[g * 32:g * 32 + 32]
    return c


def gl_edges(op):
    def gen():
        r = random.Random(11 if op == "glf" else 12)
        for mode in (-1, 0, 1, 2, 3, 100):
            for how in (-1, 0, 1, 11, 12, 13, 35, 36, 37, 143, 144, 145, 200):
                yield gl_case(r, op, mode, how)
    return gen


def setup_case(r):
    c = base(r, "setup")
    c["args"] = [i16(r), i16(r), i16(r), i16(r)] + [r.choice([0, 1, -1, r.randrange(-1000, 1000), r.randrange(-2 ** 31, 2 ** 31)]) for _ in range(6)]
    c["queues"]["FindColour"] = [r.randrange(256) for _ in range(4)]
    return c


def setup_edges():
    r = random.Random(5)
    for H, dy in ((1536, 1), (1536, -1), (-1, 1535), (-1, 1536), (-1, 1537), (1, -1537), (1, 1536), (0, 5), (32767, 32767), (-32768, 32767)):
        c = setup_case(r)
        c["H"] = H
        c["W"] = r.choice([0, 1])
        c["args"][1], c["args"][3] = 0, dy
        c["args"][0], c["args"][2] = 0, 0
        yield c
    for W, dx in ((2048, -1), (2048, 1), (1, -2048), (1, -2049), (1, 2047), (1, 2049), (-1, 2048), (-1, 2049), (-32768, 32767), (32767, 32767)):
        c = setup_case(r)
        c["W"] = W
        c["H"] = 0
        c["args"][0], c["args"][2] = 0, dx
        c["args"][1], c["args"][3] = 0, 0
        yield c


def draw_case(r):
    return base(r, "draw")


def draw_edges():
    r = random.Random(9)
    for surface in (0, 0x20000):
        for cb in (0, 1):
            c = draw_case(r)
            c["queues"]["surface"] = [surface]
            c["cb"] = cb
            yield c


TESTS = [
    FnTest("SetupGraph", ppc, host, edge=setup_edges, rnd=setup_case),
    FnTest("DrawMeGraph", ppc, host, edge=draw_edges, rnd=draw_case),
    FnTest("GraphLine", ppc, host, edge=gl_edges("glf"), rnd=lambda r: gl_case(r, "glf")),
    FnTest("GraphLine_int", ppc, host, edge=gl_edges("gli"), rnd=lambda r: gl_case(r, "gli")),
]
