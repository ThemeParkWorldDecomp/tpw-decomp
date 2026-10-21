"""Unit v5142, the park status panel, against libtpw_unit_sinit_ui_parkStatus_cpp.dylib (tests/unit_sinit_ui_parkStatus_cpp.cpp), main app
("SimTheme Park"), src/game/ui/ui_parkStatus.cpp:
  __sinit_ui_parkStatus_cpp 0x101adbe4, UI_ParkStatusPanelDateChange 0x101ada64

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory little-endian).
Callees outside the unit are fakes on both sides recording their arguments: GetParkAnalyser, the two MaxAnalyserValue (history as an offset
from the analyser, value in, range; they write the next "max" queue value), GetWindow, SetYRange, DrawMeGraph, the grapher constructor and
LbPalette_FindColour (answers the "colour" queue).

The world of a case: flags, range, the panel pointer (null or a block), the world pointer, the analyser block, the child window answer.
Compared: the event log, the 25 colour bytes. No fused operations. Not tested: a null global world (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_ui_parkStatus_cpp"
MASK = 0xFFFFFFFF

FN_DATE, FN_SINIT = 0x101ADA64, 0x101ADBE4
SIZES = {FN_DATE: 332, FN_SINIT: 312}
OWN = set(SIZES)

FIND_COLOUR = 0x10238BF0
GET_ANALYSER = 0x1013EBC8
MAX_INT = 0x101F5E3C
MAX_FLOAT = 0x101F5F2C
SET_YRANGE = 0x101FD7D8
GET_WINDOW = 0x101DBE40
DRAW_GRAPH = 0x101F66C0
GRAPHER_CTOR = 0x101FD6E8

ADDR = dict(colours=0x10394A6C, grapher=0x10394A28, range=0x10394A88, panel=0x10394A8C, flags=0x102ABBAC,
            world=0x1037EBFC, lines=0x101ABB90, palette=0x103BCD28)   # lines: ParkInfoLines, the code word of the TVector 0x10267ca8 the Mac passes to DrawMeGraph
HOST_LAYOUT = [25]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_find(e):
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the import glue saves the TOC here; the caller reloads it
    L.rec("FindColour", (L.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF, e.r(6) & 0xFF))
    e.set_r(3, L.ret("colour") & 0xFF)


def stub_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, e.t_an)


def mk_max(name):
    def stub(e):
        L = e.cl
        L.rec(name, (T.s32(e.r(3) - e.t_an), e.s32(e.r(4)), T.s32(e.r(5))))
        e.write(e.r(4), struct.pack(">i", T.s32(L.ret("max"))))
    return stub


def stub_yrange(e):
    L = e.cl
    L.rec("SetYRange", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))


def stub_window(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, e.t_child if L.ret("child") else 0)


def stub_draw(e):
    L = e.cl
    L.rec("DrawMeGraph", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.u32(e.r(5)))))     # the callback: the function its TVector holds


def stub_ctor(e):
    L = e.cl
    L.rec("CUIGrapherCtor", (L.lab(e.r(3)),))


STUBS.internal(FIND_COLOUR, stub_find)
STUBS.internal(GET_ANALYSER, stub_analyser)
STUBS.internal(MAX_INT, mk_max("MaxInt"))
STUBS.internal(MAX_FLOAT, mk_max("MaxFloat"))
STUBS.internal(SET_YRANGE, stub_yrange)
STUBS.internal(GET_WINDOW, stub_window)
STUBS.internal(DRAW_GRAPH, stub_draw)
STUBS.internal(GRAPHER_CTOR, stub_ctor)

AN_SIZE = 0x22000


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def ptr(self, a, v):
        self.words(a, [v])

    def __init__(self, e, case):
        self.e = e
        self.panel_blk = e.alloc(16)
        self.world_blk = e.alloc(16)
        self.an = e.alloc(AN_SIZE)
        self.child = e.alloc(16)
        e.t_an, e.t_child = self.an, self.child
        self.L = e.cl = CL.PpcLog({self.panel_blk: 1, self.world_blk: 2, self.child: 3, ADDR["grapher"]: 4,
                                   ADDR["lines"]: 5, e.u32(0x10260D44): 6}, case["queues"])
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

    def call(self, op):
        entry = FN_DATE if op == "date" else FN_SINIT
        args = [5] if op == "date" else []
        T.call_ppc(self.e, entry, args)


class Host(Side):
    fmt = "<I"

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.panel_blk = lib.tpw_parkstatus_alloc(16)
        self.world_blk = lib.tpw_parkstatus_alloc(16)
        self.an = lib.tpw_parkstatus_alloc(AN_SIZE)
        self.child = lib.tpw_parkstatus_alloc(16)
        lib.tpw_parkstatus_set(self.an, self.child)
        CL.host_begin(lib, {1: self.panel_blk, 2: self.world_blk, 3: self.child, 4: self.g("grapher"), 5: self.g("lines"),
                            6: self.g("palette")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_parkstatus_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        if op == "date":
            self.lib.tpw_parkstatus_date(5)
        else:
            self.lib.tpw_parkstatus_sinit()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_parkstatus_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "set").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "date").argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("range"), [case["range"]])
    s.ptr(s.g("panel"), s.panel_blk if case["panel"] else 0)
    s.ptr(s.g("world"), s.world_blk)
    s.write(s.g("colours"), bytes(case["colours"]))


def observe(s, case):
    return {"log": s.log(), "colours": s.read(s.g("colours"), 25)}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "flags": r.choice([r.randrange(0, 32), r.getrandbits(32)]), "range": T.s32(r.getrandbits(32)) & MASK,
            "panel": r.choice([0, 1, 1]), "colours": [r.randrange(256) for _ in range(25)],
            "queues": {"max": [r.choice([0, r.randrange(1, 1000), r.getrandbits(32)]) for _ in range(5)],
                       "child": [r.choice([0, 1])],
                       "colour": [r.randrange(256) for _ in range(5)]}}


def date_edges():
    r = random.Random(11)
    for flags in (0, 1, 2, 4, 8, 0x10, 0x1F, 0xE0, 0xFFFFFFFF, 0x15, 0x0A):
        for panel in (0, 1):
            c = base(r, "date")
            c["flags"], c["panel"] = flags, panel
            yield c
    c = base(r, "date")
    c["queues"]["child"] = [0]
    yield c


def date_random(r):
    return base(r, "date")


def sinit_edges():
    r = random.Random(12)
    for _ in range(3):
        yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


TESTS = [
    FnTest("UI_ParkStatusPanelDateChange", ppc, host, edge=date_edges, rnd=date_random),
    FnTest("__sinit_ui_parkStatus_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
]
