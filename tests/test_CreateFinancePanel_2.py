"""Unit v51026, CreateFinancePanel 0x101a001c, against libtpw_unit_CreateFinancePanel_2.dylib (tests/unit_CreateFinancePanel_2.cpp), main app
("SimTheme Park"), src/game/ui/ui_FinancePanel.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the window methods,
the text sprite constructors, operator new, the string table, the virtual SetText / Enable called through the table at window +0x118) is a
fake on both sides that records its arguments. Windows are real blocks on both sides (so the original can read +0x118) found by a table.

The world of a case: the panel global, the Build answer, which of the two windows that may be missing exist, the graph mode and flags, the
game type guard and word and the value its constructor writes. What is compared: the event log, the panel, flags, mode, guard and type words.

No fused operations. Not tested: a missing window other than 0x12272 and 0x12273 (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateFinancePanel_2"
MASK = 0xFFFFFFFF

FN_A = 0x101A001C
SIZES = {FN_A: 3140}
OWN = set(SIZES)

ADDR = dict(flags=0x102A9EE4, mode=0x10394900, panel=0x10394904, gamewin=0x103949E4, guard=0x102B3AA4, gametype=0x102B3AA0,
            script=0x102A9BEC, cbpanel=0x10267BA8, cbgraph=0x10267B98)
HOST_LAYOUT = [4]

IDS = [0x12272, 0x12273, 0x12275, 0x12276, 0x12274, 0x12277, 0x1227B, 0x12279, 0x12278, 0x1227C, 0x1227D, 0x1227A, 0x1227E, 0x1227F]
ROOT_LABEL, GAME_LABEL, SCRIPT_LABEL, CBPANEL_LABEL, CBGRAPH_LABEL, GT_LABEL = 20, 21, 22, 23, 24, 25

CUR = None
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u(v):
    return v & MASK


def s_noop(name, regs=()):
    pass


def stub_check(e):
    e.cl.rec("CheckPanel", ())
    e.set_r(3, 0)


def stub_lastpanel(e):
    e.cl.rec("SetLastPanel", (T.s32(e.r(3)), T.s32(e.r(4))))


def stub_setpanel(e):
    L = e.cl
    L.rec("SetPanel", (L.lab(e.r(3)),))


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.set_r(3, CUR.build_res)


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, CUR.wins.get(e.r(4), 0))


def save_toc(e):
    """The glue the original calls through saves the TOC pointer at 0x14(r1); the code reloads it afterwards."""
    e.write(u(e.r(1)) + 0x14, struct.pack(">I", u(e.r(2))))


def stub_new(e):
    save_toc(e)
    e.cl.rec("new", (u(e.r(3)),))
    e.set_r(3, 0x62000000 + 0x1000 * CUR.news)
    CUR.news += 1


def stub_spritector(e):
    e.cl.rec("SpriteCtor", (u(e.r(3)),))


def stub_drawctor(e):
    e.cl.rec("DrawSpriteCtor", (u(e.r(3)),))


def stub_setobj(e):
    L = e.cl
    L.rec("SetClientDrawObject", (L.lab(e.r(3)), u(e.r(4))))


def stub_setcb(e):
    L = e.cl
    L.rec("SetCallbackFunction", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_fontjust(e):
    L = e.cl
    L.rec("SetFontJust", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))


def stub_fonttable(e):
    e.cl.rec("GetFontTableEntry", (u(e.r(3)),))
    e.set_r(3, u(0x64000000 + u(e.r(3))))


def stub_setfont(e):
    L = e.cl
    L.rec("SetFont", (L.lab(e.r(3)), u(e.r(4))))


def stub_fontcolour(e):
    L = e.cl
    L.rec("SetFontColour", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))))


def stub_drawtext(e):
    L = e.cl
    L.rec("SetClientDrawText", (L.lab(e.r(3)), u(e.r(4))))


def stub_getstring(e):
    e.cl.rec("GetString", (T.s32(e.r(3)),))
    e.set_r(3, u(0x63000000 + u(e.r(3))))


def stub_ptrglue(e):
    save_toc(e)
    L = e.cl
    r4 = u(e.r(4))
    if u(e.r(12)) == 0x5C5C001C:
        r4 = T.s16(r4)
    L.rec("virt", (L.lab(e.r(3)), u(e.r(12)), r4))


def stub_setcurrent(e):
    L = e.cl
    L.rec("SetCurrent", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, 0)


def stub_setstate(e):
    L = e.cl
    L.rec("SetState", (L.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def stub_gametype(e):
    L = e.cl
    L.rec("CGameType", (L.lab(e.r(3)),))
    e.write(e.r(3), struct.pack(">I", u(L.ret("gtype"))))


def stub_advisor(e):
    e.cl.rec("Advisor", (T.s32(e.r(3)),))


STUBS.internal(0x10180514, stub_check)
STUBS.internal(0x101A57FC, stub_lastpanel)
STUBS.internal(0x101805A0, stub_setpanel)
STUBS.internal(0x101DEB78, stub_build)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1018D11C, stub_spritector)
STUBS.internal(0x1018C978, stub_drawctor)
STUBS.internal(0x101DB624, stub_setobj)
STUBS.internal(0x101DCBA8, stub_setcb)
STUBS.internal(0x101DA204, stub_fontjust)
STUBS.internal(0x10180270, stub_fonttable)
STUBS.internal(0x101DA118, stub_setfont)
STUBS.internal(0x101DA3AC, stub_fontcolour)
STUBS.internal(0x101DE130, stub_drawtext)
STUBS.internal(0x10180468, stub_getstring)
STUBS.internal(0x1023BAA8, stub_ptrglue)
STUBS.internal(0x101CC148, stub_setcurrent)
STUBS.internal(0x101CB1D0, stub_setstate)
STUBS.internal(0x1016F5D8, stub_gametype)
STUBS.internal(0x101821CC, stub_advisor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_world(self, case):
        """Allocates the windows (the ones the case lets exist), each with a table at +0x118 holding the two virtual slots."""
        self.wins = {}
        for i in IDS:
            if (i == 0x12272 and case["no72"]) or (i == 0x12273 and case["no73"]):
                continue
            a = self.alloc(0x130)
            vt = self.alloc(0x40)
            self.words(a + 0x118, [vt])
            self.words(vt + 0x2C, [0x5C5C002C])
            self.words(vt + 0x1C, [0x5C5C001C])
            self.wins[i] = a
        self.root = self.alloc(0x130)
        vt = self.alloc(0x40)
        self.words(self.root + 0x118, [vt])
        self.gw = self.alloc(0x130)
        self.build_res = 0 if case["nobuild"] else self.root
        self.news = 0

    def label_of(self, v):
        if v == 0:
            return 0
        if v == self.root:
            return ROOT_LABEL
        return -v


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        global CUR
        CUR = self
        self.e = e
        self.make_world(case)
        fixed = {a: 1 + n for n, a in enumerate(self.wins.values())}
        fixed = {}
        for n, i in enumerate(IDS):
            if i in self.wins:
                fixed[self.wins[i]] = 1 + n
        fixed[self.root] = ROOT_LABEL
        fixed[self.gw] = GAME_LABEL
        fixed[ADDR["script"]] = SCRIPT_LABEL
        fixed[ADDR["cbpanel"]] = CBPANEL_LABEL
        fixed[ADDR["cbgraph"]] = CBGRAPH_LABEL
        fixed[ADDR["gametype"]] = GT_LABEL
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_word(a)

    def install(self):
        pass

    def call(self):
        T.call_ppc(self.e, FN_A, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_world(case)
        fixed = {}
        for n, i in enumerate(IDS):
            if i in self.wins:
                fixed[1 + n] = self.wins[i]
        fixed[ROOT_LABEL] = self.root
        fixed[GAME_LABEL] = self.gw
        fixed[SCRIPT_LABEL] = self.g("script")
        fixed[CBPANEL_LABEL] = self.g("cbpanel")
        fixed[CBGRAPH_LABEL] = self.g("cbgraph")
        fixed[GT_LABEL] = self.g("gametype")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fin_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fin_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def install(self):
        tab = b"".join(struct.pack("<QQ", i, self.wins.get(i, 0)) for i in IDS)
        self.write(self.g("wintab"), tab)
        self.put_ptr(self.g("buildres"), self.build_res)
        self.words(self.g("newcnt"), [0])

    def call(self):
        self.lib.tpw_fin_call()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fin_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.install()
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("mode"), [case["mode"]])
    s.put_ptr(s.g("panel"), s.root if case["panel"] else 0)
    s.put_ptr(s.g("gamewin"), s.gw)
    s.write(s.g("guard"), bytes([case["guard"]]))
    s.words(s.g("gametype"), [case["gt"]])


def observe(s, case):
    return {"log": s.log(), "flags": s.rd_word(s.g("flags")), "mode": s.rd_word(s.g("mode")),
            "panel": s.label_of(s.get_ptr(s.g("panel"))), "guard": s.read(s.g("guard"), 1),
            "gametype": s.rd_word(s.g("gametype"))}


def run(s, case):
    try:
        build(s, case)
        s.call()
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, **kw):
    c = {"panel": 0, "nobuild": 0, "no72": 0, "no73": 0, "mode": r.choice([-1, 0, 1, 2, 3, r.randrange(-5, 6)]),
         "flags": r.choice([0, 0xFF, r.randrange(0, 256), r.getrandbits(32)]), "guard": r.choice([0, 1, 0x80, 0xFF]),
         "gt": r.choice([0, 1, 2, 3, MASK]), "ctor": r.choice([0, 1, 2, 3, MASK])}
    c.update(kw)
    c["queues"] = {"gtype": [c["ctor"]]}
    return c


def finance_edges():
    r = random.Random(51026)
    yield base(r, panel=1)
    yield base(r, nobuild=1)
    yield base(r, no72=1)
    yield base(r, no73=1)
    for mode in (-2, -1, 0, 1, 2, 3, 4, 0x7FFFFFFF, -0x80000000):
        yield base(r, mode=mode, guard=1, gt=0)
    for b in range(8):
        yield base(r, flags=1 << b, guard=1, gt=0)
    yield base(r, flags=0, guard=1, gt=0)
    yield base(r, flags=0xFFFFFFFF, guard=1, gt=2)
    for guard in (0, 1, 0x80, 0xFF):
        for gt in (0, 1, 2, 3, MASK):
            for ctor in (0, 2, 1):
                yield base(r, guard=guard, gt=gt, ctor=ctor)


def finance_random(r):
    return base(r, panel=int(r.random() < 0.1), nobuild=int(r.random() < 0.1), no72=int(r.random() < 0.1), no73=int(r.random() < 0.15))


TESTS = [
    FnTest("CreateFinancePanel", ppc, host, edge=finance_edges, rnd=finance_random),
]
