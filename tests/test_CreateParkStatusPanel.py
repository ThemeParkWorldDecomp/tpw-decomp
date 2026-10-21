"""Unit v5572, the park status panel's creation, against libtpw_unit_CreateParkStatusPanel.dylib (tests/unit_CreateParkStatusPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_parkStatus.cpp:
  CreateParkStatusPanel 0x101aca78

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit is a fake on both
sides that records its arguments (the panel check, SetLastPanel, Build, the window calls, GetWindow, operator new, the draw object
constructors, the strings, the font table, the analyser readers, the virtual SetValue / SetText through __ptr_glue) and answers from the case.

The world of a case: the panel global (set or not), the flags, the range, the last values, 28 window blocks with a random z (the
panel Build makes, the children by id, the game window), the nulls Build, GetWindow(0x4a68bb) and GetWindow(0x4a68c1) can answer, the
setAnimTimer answer and the analyser's answers. What is compared: the event log, the panel and last-value globals.

No fused operations. Not tested: nothing is skipped; the windows other than 0x4a68bb and 0x4a68c1 are never NULL (the original uses them
unchecked).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateParkStatusPanel"
MASK = 0xFFFFFFFF

FN = 0x101ACA78
SIZES = {FN: 3932}
OWN = set(SIZES)

ADDR = dict(panel=0x10394A8C, flags=0x102ABBAC, range=0x10394A88, lastTotal=0x10394A18, lastInPark=0x10394A1C,
            gameWindow=0x103949E4, gameWorld=0x1037EBFC)
WIN_BASE = 0x4A68BB
NWIN = 28            # 0 the panel, 1 to 26 the children (id - WIN_BASE + 1), 27 the game window
ANALYSER = 29
CUR = {}             # the PowerPC fakes read the case's windows from here


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec_stub(address, name, spec):
    STUBS.internal(address, CL.recorder(name, spec))


rec_stub(0x10180514, "CheckPanel", "")
rec_stub(0x101A57FC, "SetLastPanel", "ii")
rec_stub(0x101805A0, "SetPanel", "L")
rec_stub(0x101DD6B8, "startAnimTimer", "Li")
rec_stub(0x101DA204, "SetFontJust", "Lii")
rec_stub(0x101DA118, "SetFont", "Lu")
rec_stub(0x101DA3AC, "SetFontColour", "Liiii")
rec_stub(0x101DE130, "SetClientDrawText", "Lu")
rec_stub(0x101DCBA8, "SetCallbackFunction", "Lu")
rec_stub(0x101ABA14, "DoDominantThoughts", "")
rec_stub(0x101821CC, "AdvisorMessage", "i")
rec_stub(0x101DB624, "SetClientDrawObject", "LL")
rec_stub(0x101DB474, "SetDrawObject", "LL")
rec_stub(0x101DD8BC, "setZ", "Lh")
rec_stub(0x101CB1D0, "SetState", "Lbb")
rec_stub(0x101CC148, "SetCurrent", "Li")
rec_stub(0x1018D11C, "SpriteCtor", "N")
rec_stub(0x1018C978, "DrawSpriteCtor", "N")
rec_stub(0x1018D930, "NumberCtor", "N")
rec_stub(0x101898C8, "GradientCtor", "Nu")


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, CUR["wins"][0])


def stub_getwindow(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    e.set_r(3, CUR["wins"][ident - WIN_BASE + 1])


def stub_anim(e):
    L = e.cl
    L.rec("setAnimTimer", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("setAnimTimer") & MASK)


def save_toc(e):
    """The import glue saves the caller's TOC pointer at 0x14(r1); the code reloads r2 from there after the call."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))


def stub_new(e):
    save_toc(e)
    L = e.cl
    size = e.r(3) & MASK
    L.rec("new", (size,))
    e.set_r(3, e.alloc(max(size, 4)))


def stub_font(e):
    L = e.cl
    index = e.r(3) & MASK
    L.rec("GetFontTableEntry", (index,))
    e.set_r(3, 0x6000 + index)


def stub_string(e):
    L = e.cl
    ident = s32(e.r(3))
    L.rec("GetString", (ident,))
    e.set_r(3, (0x5000 + ident * 4) & MASK)


def stub_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, CUR["wins"][ANALYSER])


def reader(name, with_arg):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4))) if with_arg else (L.lab(e.r(3)),))
        e.set_r(3, L.ret(name) & MASK)
    return stub


def stub_glue(e):
    save_toc(e)
    L = e.cl
    slot = e.r(12) & MASK
    if slot == 0x24:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif slot == 0x2C:
        L.rec("SetText", (L.lab(e.r(3)), e.r(4) & MASK))
    else:
        L.rec("badglue", (slot,))


STUBS.internal(0x101DEB78, stub_build)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x101DD5E0, stub_anim)
STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x10180270, stub_font)
STUBS.internal(0x10180468, stub_string)
STUBS.internal(0x1013EBC8, stub_analyser)
STUBS.internal(0x100E7278, reader("InPark", False))
STUBS.internal(0x100E7994, reader("Total", False))
STUBS.internal(0x100E7E98, reader("Under", True))
STUBS.internal(0x100E7D1C, reader("Over", True))
STUBS.internal(0x1023BAA8, stub_glue)


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

    def put_ptr(self, a, v):
        """A pointer global: 4 bytes on the Mac, 8 on the host."""
        self.write(a, struct.pack(self.pfmt, v))

    def get_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_blocks(self):
        """The window blocks, the world and the analyser, before the log is made (their labels)."""
        self.win = [self.alloc(self.wsize) for _ in range(NWIN)]
        self.world = self.alloc(16)
        self.an = self.alloc(16)
        self.labels = {self.win[i]: i + 1 for i in range(NWIN)}
        self.labels[self.world] = 40
        self.labels[self.an] = 41

    def label_of(self, a):
        return self.labels.get(a, a)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    zoff = 0xDC
    wsize = 0x128

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        self.vt = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog(dict(self.labels), case["queues"])
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

    def prep_window(self, a):
        self.words(a + 0x118, [self.vt])

    def set_wins(self, wins):
        self.words(self.vt + 0x24, [0x24])
        self.words(self.vt + 0x2C, [0x2C])
        CUR["wins"] = wins

    def call(self):
        T.call_ppc(self.e, FN, [])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.zoff, self.wsize = lib.zoff, lib.wsize
        self.make_blocks()
        CL.host_begin(lib, {label: a for a, label in self.labels.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ps_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ps_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def prep_window(self, a):
        pass

    def set_wins(self, wins):
        base = self.g("wins")
        for i, a in enumerate(wins):
            self.write(base + 8 * i, struct.pack("<Q", a))

    def call(self):
        self.lib.tpw_ps_create()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ps_addr.restype, lib.tpw_ps_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ps_alloc.restype, lib.tpw_ps_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ps_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ps_create.restype, lib.tpw_ps_create.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ps_layout(out)
    assert n == 2, n
    lib.zoff, lib.wsize = int(out[0]), int(out[1])
    assert 0 < lib.zoff < lib.wsize
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("range"), [case["range"]])
    s.words(s.g("lastTotal"), [case["last"][0]])
    s.words(s.g("lastInPark"), [case["last"][1]])
    s.put_ptr(s.g("panel"), s.win[0] if case["panel_set"] else 0)
    s.put_ptr(s.g("gameWindow"), s.win[NWIN - 1])
    s.put_ptr(s.g("gameWorld"), s.world)
    for i, a in enumerate(s.win):
        s.prep_window(a)
        s.write(a + s.zoff, struct.pack(s.fmt[0] + "h", case["zs"][i]))
    wins = [0] * 32
    for i in range(NWIN):
        wins[i] = s.win[i]
    wins[0] = 0 if case["build_null"] else s.win[0]
    if case["null68bb"]:
        wins[1] = 0
    if case["null68c1"]:
        wins[7] = 0
    wins[ANALYSER] = s.an
    s.set_wins(wins)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "panel": s.label_of(s.get_ptr(s.g("panel"))),
            "lastTotal": s.rd_words(s.g("lastTotal"), 1), "lastInPark": s.rd_words(s.g("lastInPark"), 1),
            "flags": s.rd_words(s.g("flags"), 1), "range": s.rd_words(s.g("range"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def mk(r, **kw):
    c = {"panel_set": False, "build_null": False, "null68bb": False, "null68c1": False,
         "flags": r.randrange(256), "range": r.choice([-1, 0, 1, 2, 3]),
         "last": [r.getrandbits(32), r.getrandbits(32)],
         "zs": [r.choice([0, 1, 0x7FFE, 0x7FFF, -1, -0x8000, r.randrange(-0x8000, 0x8000)]) for _ in range(NWIN)],
         "anim": r.choice([0, 1, 0x10000, 0x18000, -1]),
         "inpark": r.choice([-1, 0, 7, r.randrange(0, 5000), r.randrange(-0x80000000, 0x80000000)]),
         "total": r.choice([-1, 0, 9, r.randrange(0, 90000), r.randrange(-0x80000000, 0x80000000)]),
         "under": r.choice([0, 1, 33, 99, 100, r.randrange(0, 101), r.randrange(-0x80000000, 0x80000000)]),
         "over": r.choice([0, 1, 66, 100, 101, r.randrange(0, 101), r.randrange(-0x80000000, 0x80000000)])}
    c.update(kw)
    c["queues"] = {"setAnimTimer": [c["anim"] & MASK], "InPark": [c["inpark"] & MASK], "Total": [c["total"] & MASK],
                   "Under": [c["under"] & MASK], "Over": [c["over"] & MASK]}
    return c


def edges():
    r = random.Random(5572)
    yield mk(r)
    yield mk(r, panel_set=True)
    yield mk(r, build_null=True)
    for anim in (0, 1, 0x10000, 0x18000, -1, 0x8000):
        yield mk(r, anim=anim)
    yield mk(r, null68bb=True)
    yield mk(r, null68c1=True)
    yield mk(r, null68bb=True, null68c1=True)
    for rng in (-1, 0, 1, 2, 3, 0x7FFFFFFF, -0x80000000):
        yield mk(r, range=rng)
    for flags in list(range(33)) + [0x3F, 0xFF, 0xFFFFFFFF, 0x20, 0xE0]:
        yield mk(r, flags=flags)
    for inpark, total in ((-1, -1), (-1, 0), (0, -1), (5, 5), (0, 0), (0x7FFFFFFF, -0x80000000)):
        yield mk(r, inpark=inpark, total=total)
    for last in ([0, 0], [MASK, MASK], [5, 5]):
        yield mk(r, last=last)
    for under in (0, 1, 99, 100, 101, 0x200000, 0x400000, -1, 0x7FFFFFFF, -0x80000000):
        yield mk(r, under=under)
    for over in (0, 1, 100, 101, 200, -1, 0x7FFFFFFF, -0x80000000, 0x3FFFFF, 0x400000):
        yield mk(r, over=over)
    for z in (0x7FFD, 0x7FFE, 0x7FFF, -0x8000, -1, -2, 0, 5):
        c = mk(r)
        c["zs"] = [z] * NWIN
        yield c


def rnd(r):
    k = r.random()
    return mk(r, panel_set=k < 0.04, build_null=0.04 <= k < 0.08, null68bb=r.random() < 0.15, null68c1=r.random() < 0.15)


TESTS = [
    FnTest("CreateParkStatusPanel", ppc, host, edge=edges, rnd=rnd),
]
