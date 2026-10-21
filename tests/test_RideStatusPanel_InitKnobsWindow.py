"""Unit v51011, the ride status panel's window functions, against libtpw_unit_RideStatusPanel_InitKnobsWindow.dylib
(tests/unit_RideStatusPanel_InitKnobsWindow.cpp), main app ("SimTheme Park"),
src/game/ui/ui_RideStatus.cpp:
  RideStatusPanel::InitWindow 0x101ba99c, UpdateWindow 0x101baaac, InitKnobsWindow 0x101bab6c, UpdateKnobsTitles 0x101baf28

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments: GetWindow (the window of an id, from a pool of fourteen), SetMinMax, the draw
objects' constructors and operator new, the font and text setters, Show, FillObjectList, InitEditBox, UpdateKnobsWindow, UpdateWhichButtons,
GetDBInfo, GetObjectName, CoolRect's constructor and destructor, TbITable::FormatString (reads the tag and writes a text made of the string
id and the tag), and the virtual calls through __ptr_glue (the window's SetValue and SetText, the ops table's two slots).

The world of a case: the panel (window, name window, ops table, speed, capacity, duration), the staff room thing and the show thing globals,
the text table pointer, the ride record's two slots (Mac +0x70 and +0x9c), the name text, the shared text buffer's old content, the
FillObjectList answer. What is compared: the event log and the shared buffer's first characters.

No fused operations. Not tested: a NULL panel window for InitWindow, UpdateWindow and UpdateKnobsTitles (the original reads it at once).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RideStatusPanel_InitKnobsWindow"
MASK = 0xFFFFFFFF

FN_INITWINDOW, FN_UPDATEWINDOW, FN_INITKNOBS, FN_TITLES = 0x101ba99c, 0x101baaac, 0x101bab6c, 0x101baf28
SIZES = {FN_INITWINDOW: 164, FN_UPDATEWINDOW: 140, FN_INITKNOBS: 900, FN_TITLES: 1328}
OWN = set(SIZES)

ADDR = dict(sroom=0x103947a4, showthing=0x1039599c, text6=0x1038047c, convbuf=0x10380c48)
# the host's offsets and sizes: panel mWindow, mNameWindow, mVtable, mSpeed, mCapacity, mDuration, sizeof; the ride record's slot 25 and 36, sizeof
HOST_LAYOUT = [0x0c, 0x1c, 0x20, 0x2c, 0x30, 0x34, 0x38, 0xE0, 0x138, 0xB08]
MAC_OFF = dict(window=0x0c, name=0x1c, ops=0x20, speed=0x2c, cap=0x30, dur=0x34)
MAC_SLOT = {25: 0x70, 36: 0x9c}
HOST = {}

WIDS = list(range(0x3e26, 0x3e34))
NAMES = ["panel", "showthing", "obj", "info", "wpanel", "wname", "font", "table", "convbuf"] + ["w%x" % i for i in WIDS]
LAB = {n: i + 1 for i, n in enumerate(NAMES)}
TOK_SV, TOK_ST, TOK_S5, TOK_S6 = 0x10000024, 0x1000002C, 0x1000001C, 0x10000020
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def R(name, spec, ret=None):
    return CL.recorder(name, spec, ret)


def st_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    k = wid - 0x3e26
    e.set_r(3, CUR["wins"][k] if 0 <= k < 14 else 0)


def st_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(max(size, 4)))


def st_fill(e):
    L = e.cl
    L.rec("FillObjectList", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("FillObjectList") & MASK)


def st_fmt(e):
    L = e.cl
    table, out, size, sid, tag = [e.r(i) for i in (3, 4, 5, 6, 7)]
    magic, tid, val, neg = [e.u32(tag + o) for o in (0, 8, 0xC, 0x10)]
    L.rec("FormatString", (L.lab(table), L.lab(out), size, sid, magic, tid, s32(val), s32(neg)))
    e.write(out, ("F%d_%d_%d_%d" % (sid, tid, s32(val), s32(neg))).encode("utf-16-be") + b"\0\0")
    e.set_r(3, 0)


SETVALUE = R("WindowSetValue", "Li")
SETTEXT = R("WindowSetText", "LS")


def st_ptr_glue(e):
    t = e.r(12)
    if t == TOK_SV:
        SETVALUE(e)
    elif t == TOK_ST:
        SETTEXT(e)
    elif t == TOK_S5:
        e.cl.rec("OpsSlot5", (0,))
    elif t == TOK_S6:
        e.cl.rec("OpsSlot6", (0,))
    else:
        raise AssertionError("unknown __ptr_glue target %#x" % t)


STUBS.internal(0x101dbe40, st_getwindow)
STUBS.internal(0x101d6b44, R("SetMinMax", "Lii"))
STUBS.internal(0x101db624, R("SetClientDrawObject", "LL"))
STUBS.internal(0x101da204, R("SetFontJust", "Lii"))
STUBS.internal(0x101da118, R("SetFont", "LL"))
STUBS.internal(0x101da3ac, R("SetFontColour", "Liiii"))
STUBS.internal(0x101de130, R("SetClientDrawText", "LS"))
STUBS.internal(0x101dc1ec, R("Show", "Lh"))
STUBS.internal(0x10180270, R("GetFontTableEntry", "u", lambda e, log: CUR["font"]))
STUBS.internal(0x101810fc, R("GetObjectName", "L", lambda e, log: CUR["nametext"]))
STUBS.internal(0x10192a90, st_fill)
STUBS.internal(0x10193dcc, R("InitEditBox", "Li"))
STUBS.internal(0x101bb494, R("UpdateKnobsWindow", "L"))
STUBS.internal(0x101b8cc8, R("UpdateWhichButtons", "L"))
STUBS.internal(0x10107cd4, R("GetDBInfo", "L", lambda e, log: CUR["info"]))
STUBS.internal(0x10239730, st_fmt)
STUBS.internal(0x102384b8, st_new)
STUBS.internal(0x1018d11c, R("SpriteCtor", "N"))
STUBS.internal(0x101898c8, R("GradientCtor", "Ns"))
STUBS.internal(0x101c7448, lambda e: e.cl.rec("RectCtor", (0,)))
STUBS.internal(0x101c74e0, lambda e: e.cl.rec("RectDtor", (0,)))
STUBS.internal(0x1023baa8, st_ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


def make_world(s):
    o = {"panel": s.alloc(s.panel_size)}
    for n in ("showthing", "obj", "font", "table"):
        o[n] = s.alloc(32)
    o["info"] = s.alloc(s.info_size)
    for n in ["wpanel", "wname"] + ["w%x" % i for i in WIDS]:
        o[n] = s.alloc(0x140)
    o["convbuf"] = s.g("convbuf")
    o["nametext"] = s.alloc(0x200)
    return o


class Side:
    def text(self, a, string):
        self.write(a, string.encode(self.enc) + b"\0\0")

    def rd_text(self, a, n):
        raw = self.read(a, 2 * n)
        out = []
        for i in range(n):
            ch = raw[2 * i:2 * i + 2].decode(self.enc, "replace") if raw[2 * i:2 * i + 2] != b"\0\0" else ""
            if ch == "":
                break
            out.append(ch)
        return "".join(out)


class Ppc(Side):
    enc = "utf-16-be"
    panel_size = 0x80
    info_size = 0x100

    def __init__(self, e, case):
        self.e = e
        self.o = make_world(self)
        self.wtab = e.alloc(0x40)
        self.opst = e.alloc(0x40)
        CUR.update(wins=[self.o["w%x" % i] for i in WIDS], font=self.o["font"], nametext=self.o["nametext"], info=self.o["info"])
        self.L = e.cl = CL.PpcLog({self.o[n]: LAB[n] for n in NAMES}, case["queues"])
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

    def w32(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    ptr = w32
    p32 = w32

    def off(self, name):
        return MAC_OFF[name]

    def slot(self, n):
        return MAC_SLOT[n]

    def ops(self):
        return self.opst

    def windows_tables(self):
        self.w32(self.wtab + 0x24, TOK_SV)
        self.w32(self.wtab + 0x2C, TOK_ST)
        self.w32(self.opst + 0x1C, TOK_S5)
        self.w32(self.opst + 0x20, TOK_S6)
        for n in ["wpanel", "wname"] + ["w%x" % i for i in WIDS]:
            self.w32(self.o[n] + 0x118, self.wtab)

    def log(self):
        return self.L.log

    def call(self, op, which):
        fn = {"initwindow": FN_INITWINDOW, "updatewindow": FN_UPDATEWINDOW, "initknobs": FN_INITKNOBS, "titles": FN_TITLES}[op]
        args = [self.o["panel"]] + ([which & MASK] if op == "titles" else [])
        T.call_ppc(self.e, fn, args)


class Host(Side):
    enc = "utf-16-le"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.panel_size = HOST["panel"]
        self.info_size = HOST["info"]
        self.o = make_world(self)
        CL.host_begin(lib, {LAB[n]: self.o[n] for n in NAMES}, case["queues"])
        lib.tpw_rsp_set(0, self.o["info"])
        lib.tpw_rsp_set(1, self.o["nametext"])
        lib.tpw_rsp_set(2, self.o["font"])
        for k, i in enumerate(WIDS):
            lib.tpw_rsp_set(10 + k, self.o["w%x" % i])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rsp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def w32(self, a, v):
        self.write(a, struct.pack("<I", v & MASK))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, a, v):   # the panel's pointer fields are Ptr32 (arena offsets, unit v5L31)
        self.w32(a, self.lib.tpw_rsp_off(v))

    def off(self, name):
        return HOST[name]

    def slot(self, n):
        return HOST[n]

    def ops(self):
        return self.g("ops")

    def windows_tables(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, which):
        panel = self.o["panel"]
        if op == "titles":
            self.lib.tpw_rsp_titles(panel, s32(which))
        else:
            getattr(self.lib, "tpw_rsp_" + op)(panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rsp_addr.restype, lib.tpw_rsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rsp_alloc.restype, lib.tpw_rsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rsp_off.restype, lib.tpw_rsp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rsp_set.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_rsp_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("initknobs", "updatewindow", "initwindow"):
        getattr(lib, "tpw_rsp_" + n).argtypes = [C.c_uint64]
    lib.tpw_rsp_titles.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rsp_layout(out)
    got = list(out)[:n]
    assert got == HOST_LAYOUT, [hex(x) for x in got]
    for name, v in zip(("window", "name", "ops", "speed", "cap", "dur", "panel", 25, 36, "info"), got):
        HOST[name] = v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    s.ptr(s.g("sroom"), o["obj"])
    s.ptr(s.g("showthing"), o["showthing"])
    s.ptr(s.g("text6"), o["table"])
    p = o["panel"]
    s.p32(p + s.off("window"), o["wpanel"] if case["haswin"] else 0)
    s.p32(p + s.off("name"), o["wname"])
    s.p32(p + s.off("ops"), s.ops())
    s.w32(p + s.off("speed"), case["speed"])
    s.w32(p + s.off("cap"), case["cap"])
    s.w32(p + s.off("dur"), case["dur"])
    s.w32(o["info"] + s.slot(25), case["s25"])
    s.w32(o["info"] + s.slot(36), case["s36"])
    s.text(o["nametext"], case["name"])
    s.text(o["convbuf"], case["buf"])
    s.windows_tables()


def observe(s, case):
    return {"log": s.log(), "buf": s.rd_text(s.o["convbuf"], 40)}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case["which"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

LETTERS = "abcdefghijklmnopqrstuvwxyz"


def word(r):
    return "".join(r.choice(LETTERS) for _ in range(r.randrange(0, 12)))


def number(r):
    return r.choice([0, 1, -1, 2, -2, 7, 100, -100, 0x7FFFFFFF, -0x80000000, r.randrange(-5000, 5000)]) & MASK


def base(r, op):
    return {"op": op, "which": 0, "haswin": True, "queues": {"FillObjectList": [r.choice([-1, 0, 1, 2, 3, 9])]},
            "speed": number(r), "cap": number(r), "dur": number(r),
            "s25": r.choice([-1, 0, 1, 2, 3, 4, 5, 9, r.randrange(-50, 50)]) & MASK,
            "s36": r.choice([0, 1, 2, 3, 4, r.randrange(-50, 50)]) & MASK,
            "name": word(r), "buf": word(r)}


def make(op, edges, rnd):
    def e():
        r = random.Random(sum(map(ord, op)))
        for kw in edges():
            c = base(r, op)
            c.update(kw)
            yield c

    def rn(r):
        c = base(r, op)
        rnd(r, c)
        return c
    return e, rn


def knobs_edges():
    yield {"haswin": True}
    yield {"haswin": False}


def knobs_rnd(r, c):
    c["haswin"] = r.random() < 0.85


def none_edges():
    yield {}
    yield {"name": ""}


def none_rnd(r, c):
    pass


def init_edges():
    for n in (-1, 0, 1, 2, 3):
        yield {"queues": {"FillObjectList": [n]}}


def init_rnd(r, c):
    pass


def titles_edges():
    for which in (0, 1, 2, 4, 3, 5, 6, 7, 8, 15, -1, 0xFFFFFFF8):
        for s36 in (3, 2, 0, 5):
            yield {"which": which, "s36": s36, "s25": 2}
    for s25 in (-1, 0, 1, 2, 3, 4, 5):
        for dur in (1, 0, -1, 2, -0x80000000):
            yield {"which": 4, "s25": s25, "dur": dur & MASK}
    for v in (0, 1, -1, 5, -5, 0x7FFFFFFF, -0x80000000):
        yield {"which": 7, "speed": v & MASK, "cap": v & MASK, "dur": v & MASK, "s36": 3, "s25": 3}
        yield {"which": 7, "speed": v & MASK, "cap": v & MASK, "dur": v & MASK, "s36": 2, "s25": 4}
        yield {"which": 7, "speed": v & MASK, "cap": v & MASK, "dur": v & MASK, "s36": 1, "s25": 1}


def titles_rnd(r, c):
    c["which"] = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 7, 7, r.randrange(0, 16), r.randrange(0, 1 << 32)])


E_K, R_K = make("initknobs", knobs_edges, knobs_rnd)
E_U, R_U = make("updatewindow", none_edges, none_rnd)
E_I, R_I = make("initwindow", init_edges, init_rnd)
E_T, R_T = make("titles", titles_edges, titles_rnd)

TESTS = [
    FnTest("RideStatusPanel::InitKnobsWindow", ppc, host, edge=E_K, rnd=R_K),
    FnTest("RideStatusPanel::UpdateWindow", ppc, host, edge=E_U, rnd=R_U),
    FnTest("RideStatusPanel::InitWindow", ppc, host, edge=E_I, rnd=R_I),
    FnTest("RideStatusPanel::UpdateKnobsTitles", ppc, host, edge=E_T, rnd=R_T),
]
