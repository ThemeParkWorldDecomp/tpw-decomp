"""Unit v5088, the staff costs panel, against libtpw_unit_CreateStaffCostsPanel.dylib (tests/unit_CreateStaffCostsPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_StaffCostsPanel.cpp:
  CreateStaffCostsPanel 0x101be440

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit is a fake on both sides
that records its arguments: the window functions (GetWindow hands out a pool of fake windows in call order, each with a vtable whose
SetValue and Enable slots record), the draw object constructors, the fonts, the strings, the analyser's histories and averages, the
headquarters' wages, Build, SetPanel, SetTitleText and the rest.

The world of a case: whether the panel exists already (gStaffCostsPanel), whether Build answers a window, whether window 0x2de exists, the
five wage words of the headquarters, the five quality averages, the three history values (and whether each GetValue writes), the four
scratch words, and the game type (the guard byte is always set, so the constructor is never reached). What is compared: the event log, the
panel global (as a label) and the four scratch words.

No fused operations. Not tested: the guard byte at 0x102b3aa4 being 0 (the host's static is built once, before the first case, so the
branch cannot be repeated); the sizes passed to operator new (the host's objects have other sizes); a NULL result of GetWindow other than
0x2de (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateStaffCostsPanel"
MASK = 0xFFFFFFFF

FN = 0x101BE440
OWN = {FN}

ADDR = dict(panel=0x10395A20, word10=0x10395A10, word14=0x10395A14, withdrawals=0x10395A18, deposits=0x10395A1C, world=0x1037EBFC,
            window=0x103949E4, gtype=0x102B3AA0, guard=0x102B3AA4)
SCRIPT = 0x102ADC20
CALLBACK = 0x10267E78
TAG_ENABLE, TAG_SETVALUE = 0xE0001C, 0xE00024
STRING_IDS = (0x165, 0x166, 0x167, 0x169, 0x93)
POOL = 40

CUR = None


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rd32(e, a):
    return s32(e.u32(a))


def st_check(e):
    e.cl.rec("CheckPanel", ())
    e.set_r(3, 0)


def st_lastpanel(e):
    e.cl.rec("SetLastPanel", (s32(e.r(3)), s32(e.r(4))))


def st_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), int(e.r(4) == SCRIPT), int(e.r(5) == CALLBACK)))
    e.set_r(3, CUR.panel_w if L.ret("Build") else 0)


def st_setpanel(e):
    e.cl.rec("SetPanel", (e.cl.lab(e.r(3)),))


def st_getpark(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR.analyser)


def st_gethq(e):
    e.cl.rec("GetStaffHQ", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR.hq)


def st_getwindow(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    n = CUR.n
    CUR.n += 1
    if ident == 0x2DE and L.ret("gw2de") == 0:
        e.set_r(3, 0)
    else:
        e.set_r(3, CUR.pool[n])


def st_setclient(e):
    L = e.cl
    L.rec("SetClientDrawObject", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_minmax(e):
    L = e.cl
    L.rec("SetMinMax", (L.lab(e.r(3)), rd32(e, e.r(4)), rd32(e, e.r(5))))


def st_setinc(e):
    L = e.cl
    L.rec("SetInc", (L.lab(e.r(3)), rd32(e, e.r(4))))


def st_font_entry(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, CUR.font)


def st_setfont(e):
    L = e.cl
    L.rec("SetFont", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_fontjust(e):
    L = e.cl
    L.rec("SetFontJust", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_fontcolour(e):
    L = e.cl
    L.rec("SetFontColour", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def st_average(e):
    L = e.cl
    model = s32(e.r(4))
    L.rec("AverageQuality", (L.lab(e.r(3)), model))
    e.set_r(3, L.ret("aq%d" % model) & MASK)


def st_getstring(e):
    ident = s32(e.r(3))
    e.cl.rec("GetString", (ident,))
    e.set_r(3, CUR.strs.get(ident, 0))


TAGS = {ADDR["word10"]: 1, ADDR["word14"]: 2, ADDR["withdrawals"]: 3, ADDR["deposits"]: 4}


def st_getvalue(e):
    L = e.cl
    index = (e.r(3) - CUR.analyser - 0x1F100) // 0x250
    L.rec("GetValue", (index, s32(e.r(5)), TAGS.get(e.r(4), 0)))
    if L.ret("gvw%d" % index) == 0:
        e.set_r(3, 0)
        return
    e.write(e.r(4), struct.pack(">I", L.ret("gv%d" % index) & MASK))
    e.set_r(3, 1)


def st_glue(e):
    L = e.cl
    if e.r(12) == TAG_ENABLE:
        L.rec("Enable", (L.lab(e.r(3)), s32(e.r(4))))
    elif e.r(12) == TAG_SETVALUE:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        raise AssertionError("ptr_glue with r12 %x" % e.r(12))


for addr, fn in ((0x10180514, st_check), (0x101A57FC, st_lastpanel), (0x101DEB78, st_build), (0x101805A0, st_setpanel),
                 (0x1013EBC8, st_getpark), (0x1013E99C, st_gethq), (0x101DBE40, st_getwindow), (0x101DB624, st_setclient),
                 (0x101D8E44, st_minmax), (0x101D8EE8, st_setinc), (0x10180270, st_font_entry), (0x101DA118, st_setfont),
                 (0x101DA204, st_fontjust), (0x101DA3AC, st_fontcolour), (0x100E88A0, st_average), (0x10180468, st_getstring),
                 (0x1001C2A8, st_getvalue), (0x1023BAA8, st_glue)):
    STUBS.internal(addr, fn)
STUBS.internal(0x1018DE60, CL.recorder("CashTextObject", "N"))
STUBS.internal(0x1018D11C, CL.recorder("SpriteTextObject", "N"))
STUBS.internal(0x101898C8, CL.recorder("GradientObject", "Ns"))
STUBS.internal(0x101DE130, CL.recorder("SetClientDrawText", "LS"))
STUBS.internal(0x10180794, CL.recorder("SetTitleText", "LiS"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def label_of(self, word):
        return self.lmap.get(word, word)


def labels(side):
    side.lmap = {}
    for i, a in enumerate(side.pool):
        side.lmap[a & side.amask] = i + 1
    for a, n in ((side.panel_w, 41), (side.gw, 42), (side.world, 43), (side.analyser, 44), (side.hq, 45), (side.font, 46)):
        side.lmap[a & side.amask] = n


class Ppc(Side):
    fmt = ">I"
    amask = 0xFFFFFFFF

    def wptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)
    hq_off = 0x0C

    def __init__(self, e, case):
        global CUR
        self.e = e
        CUR = self
        self.n = 0
        vt = e.alloc(0x30)
        e.write(vt + 0x1C, struct.pack(">I", TAG_ENABLE))
        e.write(vt + 0x24, struct.pack(">I", TAG_SETVALUE))

        def window():
            a = e.alloc(0x160)
            e.write(a + 0x118, struct.pack(">I", vt))
            return a

        self.pool = [window() for _ in range(POOL)]
        self.panel_w = window()
        self.gw = window()
        self.world = e.alloc(16)
        self.analyser = e.alloc(0x22000)
        self.hq = e.alloc(0x68)
        self.font = e.alloc(16)
        self.strs = {}
        for ident in STRING_IDS:
            text = "T%X" % ident
            a = e.alloc(32)
            e.write(a, b"".join(struct.pack(">H", ord(c)) for c in text + "\0"))
            self.strs[ident] = a
        labels(self)
        self.L = e.cl = CL.PpcLog({a: n for a, n in self.lmap.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_type(self, t):
        self.words(ADDR["gtype"], [t])
        self.write(ADDR["guard"], b"\x01")

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"
    amask = (1 << 64) - 1

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.pool = [lib.tpw_scs_window() for _ in range(POOL)]
        self.panel_w = lib.tpw_scs_window()
        self.gw = lib.tpw_scs_window()
        self.world = lib.tpw_scs_alloc(16)
        self.analyser = lib.tpw_scs_alloc(lib.tpw_scs_analyser_size())
        self.hq = lib.tpw_scs_alloc(lib.tpw_scs_hq_size())
        self.font = lib.tpw_scs_alloc(16)
        self.hq_off = lib.tpw_scs_hq_offset()
        lib.tpw_scs_set(0, self.font)
        lib.tpw_scs_set(1, self.analyser)
        lib.tpw_scs_set(2, self.hq)
        lib.tpw_scs_set(3, self.panel_w)
        lib.tpw_scs_set(4, 0)
        for i, a in enumerate(self.pool):
            lib.tpw_scs_set(100 + i, a)
        labels(self)
        CL.host_begin(lib, {n: a for a, n in self.lmap.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_scs_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_type(self, t):
        self.lib.tpw_scs_type(t)

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_scs_call()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name in ("alloc", "window", "analyser_size", "hq_size", "hq_offset", "addr"):
        getattr(lib, "tpw_scs_" + name).restype = C.c_uint64
    lib.tpw_scs_alloc.argtypes = [C.c_uint32]
    lib.tpw_scs_addr.argtypes = [C.c_char_p]
    lib.tpw_scs_set.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_scs_type.argtypes = [C.c_int32]
    lib.bound = True


def build(s, case):
    s.wptr(s.g("world"), s.world)
    s.wptr(s.g("window"), s.gw)
    s.wptr(s.g("panel"), case["panel0"])
    s.words(s.g("word10"), [case["scratch"][0]])
    s.words(s.g("word14"), [case["scratch"][1]])
    s.words(s.g("withdrawals"), [case["scratch"][2]])
    s.words(s.g("deposits"), [case["scratch"][3]])
    s.words(s.hq + s.hq_off, case["hq"])
    s.set_type(case["gt"])


def observe(s, case):
    return {"log": s.log(),
            "panel": s.label_of(s.rd_ptr(s.g("panel"))),
            "scratch": [s.rd_word(s.g(n)) for n in ("word10", "word14", "withdrawals", "deposits")]}


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


VALUES = [0, 1, -1, 99, 100, 101, 1023, 1024, 0x7FFFFFFF, -0x80000000, 0x1FFFFF, 0x200000, 0x3FFFFF, 0x400000, 5000, -5000, 10000]


def rv(r):
    k = r.random()
    if k < 0.3:
        return r.choice(VALUES)
    if k < 0.8:
        return r.randrange(-2000, 20000)
    return r.randrange(-0x80000000, 0x80000000)


def base(r):
    q = {"Build": [1], "gw2de": [1]}
    for m in range(4, 9):
        q["aq%d" % m] = [rv(r) & MASK]
    for i in (2, 3, 5):
        q["gv%d" % i] = [rv(r) & MASK]
        q["gvw%d" % i] = [1]
    return {"queues": q, "panel0": 0, "gt": r.choice([0, 1, 2, 3, -1, 2]) & MASK,
            "hq": [rv(r) & MASK for _ in range(5)], "scratch": [r.randrange(0, 1 << 32) for _ in range(4)]}


def random_case(r):
    c = base(r)
    k = r.random()
    if k < 0.08:
        c["panel0"] = 0x1234
    elif k < 0.16:
        c["queues"]["Build"] = [0]
    elif k < 0.3:
        c["queues"]["gw2de"] = [0]
    for i in (2, 3, 5):
        if r.random() < 0.15:
            c["queues"]["gvw%d" % i] = [0]
    return c


def edges():
    r = random.Random(88)
    yield base(r)
    c = base(r)
    c["panel0"] = 0x1234
    yield c
    c = base(r)
    c["queues"]["Build"] = [0]
    yield c
    c = base(r)
    c["queues"]["gw2de"] = [0]
    yield c
    for gt in (0, 1, 2, 3, -1):
        c = base(r)
        c["gt"] = gt & MASK
        yield c
    for i in (2, 3, 5):
        for v in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
            c = base(r)
            c["queues"]["gv%d" % i] = [v & MASK]
            yield c
        c = base(r)
        c["queues"]["gvw%d" % i] = [0]
        yield c
    for m in range(4, 9):
        for v in VALUES:
            c = base(r)
            c["queues"]["aq%d" % m] = [v & MASK]
            yield c
    c = base(r)
    c["hq"] = [0, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 1]
    yield c


TESTS = [
    FnTest("CreateStaffCostsPanel", ppc, host, edge=edges, rnd=random_case),
]
