"""Unit v5689, a bridge taken away and the thing under the pointer, against libtpw_unit_RemoveBridge.dylib (tests/unit_RemoveBridge.cpp), main app
("SimTheme Park"), src/LLCellSystem_hoardimage_RemoveBridge.cpp:
  RemoveBridge 0x1009722c, LLCC_SystemGetPointedPeepThing 0x10096bc8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetNeighbouringCell,
IsXYValid, GetDirection, IsBridge, IsFlag, SetFlag, UnSetFlag, SetHFieldTile, UI_GetMouseXY, SCR_WhichScriptHasThisSprite, CTRACE,
GAME_ASSERTMSG) are fakes on both sides that record their arguments and answer from the case (IsXYValid answers 0 <= x, y < 128 on both).

The world of a case: the globals, fake track cells (never dereferenced), the thing table, a world block with the map cells' thing heads,
the things (kind +2, next +0xa, script handle +0xc), the hot sprite and the mouse. What is compared: the answer (as a thing number), the
globals written and the event log.

The widescreen peep point (unit v5L51): a library built with the port fixes, with the unit space of a 16:9 screen (2730 units), puts the
pointer's x in the view's -1..1 as x times (2 / 2730), less 1, where the Mac has x / 1024 - 1. "the widescreen peep point" runs cases on the
original and expects that hot x from the host, the rest unchanged. A library built without the port fixes keeps x / 1024 - 1; there its own
hot x is replaced the same way, so the test is the 4:3 comparison again.

No fused operations that round (the products are exact). Not tested: a direction other than 1, 4, 0x10, 0x40 in RemoveBridge (the original
then uses the caller's r25 and r26, undefined), a thing list that is not finished or reaches a missing thing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RemoveBridge"
MASK = 0xFFFFFFFF

FN_RB, FN_PT = 0x1009722C, 0x10096BC8

GETNB, ISXY, GETDIR, UNSET, ISBRIDGE, ISFLAG, SETFLAG, SETHF = 0x1012FCB8, 0x101012F4, 0x10081930, 0x100A29CC, 0x100A34C4, 0x100A2918, 0x100A2964, 0x100A3BAC
GETMOUSE, CTRACE, SCRSPR, GASSERT = 0x101CA900, 0x1006FCBC, 0x100DA7E0, 0x1000DDE8

ADDR = dict(firstx=0x1029E02C, firsty=0x1029E030, e034=0x1029E034, e038=0x1029E038, end=0x1029E20C, f24=0x102DC1AC + 0x24,
            offs=0x102DE0E4, thingarr=0x1034CBFC, world=0x1037EBFC, hotx=0x103BC8D0 + 0x93C, hoty=0x103BC8D0 + 0x940,
            hotspr=0x103BC8D0 + 0x948, c0=0x7F001000, c1=0x7F002000, c2=0x7F003000, sprite=0x7F004000)
HOST_LAYOUT = [48, 0, 0x2D8, 0x44, 0x24]
OFFS = [(0, 0), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1)]
TEXTS = {"Found sprite %lx\n": 1, " Found script %lx\n": 2,
         "Thing has been allocated in ThingArray index 0 - shouldn't happen\n": 3, "  Checking against scripts %lx\n": 4}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update({FN_RB, FN_PT})


def ppc_valid(x, y):
    return 1 if 0 <= x < 128 and 0 <= y < 128 else 0


def stub_nb(e):
    L = e.cl
    d = e.r(4)
    L.rec("GetNeighbouringCell", (L.lab(e.r(3)), e.s32(d), e.s32(d + 4), s32(e.r(5))))
    e.set_r(3, [0, ADDR["c1"], ADDR["c2"], ADDR["c0"]][L.ret("nb") & 3])


def stub_isxy(e):
    L = e.cl
    x, y = s32(e.r(3)), s32(e.r(4))
    L.rec("IsXYValid", (x, y))
    e.set_r(3, ppc_valid(x, y))


def stub_dir(e):
    L = e.cl
    L.rec("GetDirection", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("dir") & 0xFF)


def stub_unset(e):
    L = e.cl
    L.rec("UnSetFlag", (L.lab(e.r(3)), e.r(4) & 0xFFFF))


def stub_setflag(e):
    L = e.cl
    L.rec("SetFlag", (L.lab(e.r(3)), e.r(4) & 0xFFFF))


def stub_bridge(e):
    L = e.cl
    L.rec("IsBridge", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("bridge") & MASK)


def stub_isflag(e):
    L = e.cl
    L.rec("IsFlag", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("isflag") & MASK)


def stub_sethf(e):
    L = e.cl
    L.rec("SetHFieldTile", (L.lab(e.r(3)),))


def stub_mouse(e):
    L = e.cl
    L.rec("UI_GetMouseXY", (1,))
    mx = T.s16(L.ret("mx"))
    my = T.s16(L.ret("my"))
    e.write(e.r(3), struct.pack(">h", mx))
    e.write(e.r(4), struct.pack(">h", my))


def stub_scr(e):
    L = e.cl
    L.rec("SCR_WhichScriptHasThisSprite", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("script") & MASK)


def stub_ctrace(e):
    L = e.cl
    s = e.cstr(e.r(4))
    if isinstance(s, bytes):
        s = s.decode("latin1")
    fid = TEXTS.get(s, -1)
    a = e.r(5)
    L.rec("CTRACE", (e.r(3) & MASK, fid, L.lab(a) if fid == 1 else s32(a)))


def stub_assert(e):
    L = e.cl
    s = e.cstr(e.r(4))
    if isinstance(s, bytes):
        s = s.decode("latin1")
    L.rec("GAME_ASSERTMSG", (s32(e.r(3)), TEXTS.get(s, -1)))


for _a, _f in ((GETNB, stub_nb), (ISXY, stub_isxy), (GETDIR, stub_dir), (UNSET, stub_unset), (ISBRIDGE, stub_bridge), (ISFLAG, stub_isflag),
               (SETFLAG, stub_setflag), (SETHF, stub_sethf), (GETMOUSE, stub_mouse), (SCRSPR, stub_scr), (CTRACE, stub_ctrace),
               (GASSERT, stub_assert)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put(self, a, ch, v):
        self.write(a, struct.pack(self.fmt[0] + ch, v))


class Ppc(Side):
    fmt = ">I"
    ptrfmt = "I"
    stride = 20

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["c0"]: 1, ADDR["c1"]: 2, ADDR["c2"]: 3, ADDR["sprite"]: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name in ("hotx", "hoty", "hotspr"):       # gei_Display is a data import: the TOC slot holds its resolved address
            return self.e.u32(0x1026067C) + ADDR[name] - 0x103BC8D0
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def tables(self):
        self.words(ADDR["offs"], [v for p in OFFS for v in p])

    def call_rb(self, a, b):
        pa = self.alloc(8)
        pb = self.alloc(8)
        self.words(pa, a)
        self.words(pb, b)
        r3, _, _ = T.call_ppc(self.e, FN_RB, [ADDR["c0"], pa, pb])
        return 0

    def call_pt(self):
        r3, _, _ = T.call_ppc(self.e, FN_PT, [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ptrfmt = "Q"
    stride = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("c0"), 2: self.g("c1"), 3: self.g("c2"), 4: self.g("sprite")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def tables(self):
        pass

    def call_rb(self, a, b):
        self.lib.tpw_rb_removebridge(self.g("c0"), a[0], a[1], b[0], b[1])
        return 0

    def call_pt(self):
        return self.lib.tpw_rb_peep()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, "tpw_rb_addr").restype, getattr(lib, "tpw_rb_addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, "tpw_rb_alloc").restype, getattr(lib, "tpw_rb_alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rb_removebridge.restype, lib.tpw_rb_removebridge.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_rb_peep.restype, lib.tpw_rb_peep.argtypes = C.c_uint64, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_rb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


GLOBALS = ("firstx", "firsty", "e034", "e038", "end", "f24")


def cells_of(case):
    """The (index, id) of the nine cells that are valid, the way the original computes them."""
    m = case["f24"] - 1
    bx = m & 0x7F
    by = (m & MASK) >> 7
    out = []
    for i, (ox, oy) in enumerate(OFFS):
        x, y = s32(bx + ox), s32(by + oy)
        if ppc_valid(x, y):
            out.append((i, (y * 128 + 1 + x) & 0xFFFF))
    return out


def build(s, case):
    s.tables()
    for n in ("firstx", "firsty", "e034", "e038"):
        s.words(s.g(n), [case[n]])
    s.put(s.g("end"), "H", case["end"])
    s.put(s.g("f24"), "H", case["f24"])
    if case["op"] != "pt":
        return
    ptr = s.ptrfmt
    s.words(s.g("hotx"), [0x11111111, 0x22222222])
    sp = s.g("sprite") if case["sprite"] else 0
    s.put(s.g("hotspr"), ptr, sp)
    world = s.alloc(0x2D8 + 0x4000 * 0x44)
    s.put(s.g("world"), ptr, world)
    s.things = {}
    for idx, th in case["things"].items():
        a = s.alloc(0x20)
        s.put(a + 2, "B", th["kind"])
        s.put(a + 0xA, "H", th["next"])
        s.put(a + 0xC, "I", th["handle"] & MASK)
        s.things[idx] = a
    for idx in range(0, 8):
        ent = s.g("thingarr") + s.stride * idx
        s.put(ent, ptr, s.things.get(idx, 0))
    for i, cid in cells_of(case):
        s.put(world + 0x2D8 + (cid - 1) * 0x44 + 0x24, "H", case["who"][i])


def execute(s, case):
    if case["op"] == "rb":
        return s.call_rb(case["a"], case["b"])
    ans = s.call_pt()
    if ans == 0:
        return -1
    for idx, a in s.things.items():
        if a == ans:
            return idx
    return -2


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for n in ("firstx", "firsty", "e034", "e038"):
        out[n] = s.rd_words(s.g(n), 1)
    if case["op"] == "pt":
        out["hot"] = s.rd_words(s.g("hotx"), 2)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    lib.tpw_rb_unitwidth.restype, lib.tpw_rb_unitwidth.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_rb_unitwidth(case.get("units", 0x800))
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

INTS = [0, 1, -1, 127, 128, -128, 0x7FFFFFFF, -0x80000000, 5, 77]


def rint(r):
    return r.choice(INTS + [r.randrange(-300, 300)])


def rb_case(r, dirn=None, nb=None, bridge=None, isflag=None):
    q = {"nb": nb if nb is not None else [r.choice([0, 1, 2, 3]) for _ in range(2)],
         "dir": [dirn if dirn is not None else r.choice([1, 4, 0x10, 0x40])],
         "bridge": bridge if bridge is not None else [r.choice([0, 1, 1]) for _ in range(2)],
         "isflag": isflag if isflag is not None else [r.choice([0, 1]) for _ in range(4)]}
    return {"op": "rb", "queues": q, "firstx": rint(r), "firsty": rint(r), "e034": rint(r), "e038": rint(r), "end": 0, "f24": 0,
            "a": [rint(r) & MASK, rint(r) & MASK], "b": [rint(r) & MASK, rint(r) & MASK]}


def rb_edges():
    r = random.Random(689)
    for d in (1, 4, 0x10, 0x40):
        for br in ([0, 0], [0, 1], [1, 0], [1, 1]):
            for k in range(16):
                fl = [(k >> j) & 1 for j in range(4)]
                yield rb_case(r, d, [1, 2], br, fl)
    for n1 in range(4):
        for n2 in range(4):
            yield rb_case(r, 4, [n1, n2], [1, 1], [0, 1, 1, 0])
            yield rb_case(r, 0x40, [n1, n2], [1, 1], [1, 0, 0, 1])


def rb_random(r):
    return rb_case(r)


def mouse(r):
    return r.choice([0, 1, -1, 767, 768, 1024, -32768, 32767, 512, r.randrange(-32768, 32768), r.randrange(-2000, 2000)])


KINDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x11, 0x12, 0x13, 0xFF, 0x81, 0x80]


def pt_case(r, kind=None, cell=None, sprite=None, script=None):
    n = r.randrange(1, 8)
    end = r.choice([0, 0, 0, 5, 1])
    thing0 = r.random() < 0.15
    ids = list(range(0 if thing0 else 1, n + 1))
    scr = r.choice([0x1234, 7, -5, 0x7FFFFFFF]) if script is None else script
    things = {}
    for i in ids:
        nxt = r.choice([j for j in ids if j > i] + [end])
        things[i] = {"kind": kind if kind is not None else r.choice(KINDS), "next": nxt,
                     "handle": scr if (kind is not None or r.random() < 0.4) else r.choice([0, 1, -5, 99, scr + 1])}
    who = []
    for i in range(9):
        who.append(r.choice(ids + [end] * 3) if ids else end)
    f24 = r.choice([1, 128, 127, 129, 0x4000, 0x3FFF, 16257, r.randrange(1, 0x4001), r.randrange(1, 0x4001), r.randrange(1, 0x4001)]) if cell is None else cell
    q = {"mx": [mouse(r)], "my": [mouse(r)], "script": [scr if script is None else script]}
    if sprite is None:
        sprite = r.random() < 0.9
    if script is None and r.random() < 0.1:
        q["script"] = [0]
    return {"op": "pt", "queues": q, "firstx": 0, "firsty": 0, "e034": 0, "e038": 0, "end": end, "f24": f24, "sprite": sprite,
            "things": things, "who": who}


def pt_edges():
    r = random.Random(1689)
    for k in KINDS + [3, 4]:
        yield pt_case(r, kind=k, cell=200)
    for cell in (0, 1, 127, 128, 129, 0x4000, 0x4001, 0xFFFF, 16257, 8000, 100):
        yield pt_case(r, cell=cell)
        yield pt_case(r, cell=cell, kind=1)
    yield pt_case(r, sprite=False)
    yield pt_case(r, sprite=True, script=0)
    for _ in range(30):
        yield pt_case(r, kind=r.choice(KINDS))


def pt_random(r):
    return pt_case(r)


# ---------------------------------------------------------------------------------------------------------------- the widescreen peep point

WIDE_UNITS = 2730   # the unit space of a 16:9 screen (3840 x 2160, or 2730 x 1536)


def f32(v):
    return struct.unpack(">f", struct.pack(">f", v))[0]


def wide_hot(out, case):
    """out with the hot x the widescreen gives: UI_UnitToViewX (include/UI_Base.h), x times (2 / units) less 1, each step a single."""
    x = T.s16(case["queues"]["mx"][0])
    hot = f32(f32(x * f32(2.0 / WIDE_UNITS)) - 1.0)
    out = dict(out)
    out["hot"] = [struct.unpack(">I", struct.pack(">f", hot))[0]] + list(out["hot"][1:])
    return out


def wide_case(r, mx=None):
    c = pt_case(r)
    c["units"] = WIDE_UNITS
    c["queues"]["mx"] = [r.randrange(0, WIDE_UNITS) if mx is None else mx]
    return c


def wide_edges():
    r = random.Random(5151)
    for mx in (0, 1, 1024, 1365, 2047, 2048, 2729, -1, 32767, -32768):
        yield wide_case(r, mx)


def ppc_wide(e, case):
    return wide_hot(ppc(e, case), case)


def host_wide(lib, case):
    out = host(lib, case)
    if lib.tpw_rb_unitwidth(0x800) == 0:
        out = wide_hot(out, case)
    return out


TESTS = [
    FnTest("RemoveBridge", ppc, host, edge=rb_edges, rnd=rb_random),
    FnTest("LLCC_SystemGetPointedPeepThing", ppc, host, edge=pt_edges, rnd=pt_random),
    FnTest("the widescreen peep point", ppc_wide, host_wide, edge=wide_edges, rnd=wide_case),
]
