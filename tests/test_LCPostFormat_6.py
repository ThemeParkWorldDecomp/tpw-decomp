"""Unit v5560, the all-items list's attach function (LCPostFormat of ui_allKidsPanel.cpp, 0x101966ec), against
libtpw_unit_LCPostFormat_6.dylib (tests/unit_LCPostFormat_6.cpp), main app ("SimTheme Park"), src/game/ui/ui_allItemsPanel.cpp:
  LCPostFormat 0x101966ec (named AllItemsListLCPostFormat on the host)

A case is a list type and a font index. The row window and its six column windows are blocks made once (the code never reads them); GetChild
and GetNext walk the columns in order. Every callee outside the unit is a fake on both sides that records its arguments: the allocations, the
four draw object constructors (new labels), SetClientDrawObject, GetFontTableEntry, SetFont, SetClientDrawText, GetChild, GetNext,
SetFontJust, UIMain_IndentClient and the virtual SetValue (through __ptr_glue).

The world of a case: gAllItemsListType (0x103947dc) and gAllItemsFontIndex (0x102a8cf0). What is compared: the whole event log and both globals.
No fused operations. Not tested: a list type whose columns run past the six windows (none does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCPostFormat_6"
MASK = 0xFFFFFFFF

FN = 0x101966EC
SIZES = {FN: 1968}
OWN = set(SIZES)

ADDR = dict(type=0x103947DC, font=0x102A8CF0)
HOST_LAYOUT = [0]

NWIN = 7
WIN = []        # the PowerPC addresses of the windows
FONT = [0]
VT = [0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def next_of(window):
    i = WIN.index(window) if window in WIN else -1
    return WIN[i + 1] if 1 <= i < NWIN - 1 else 0


def save_toc(e):
    """The glue stubs save the caller's TOC pointer where the code reloads it from (lwz r2, 0x14(r1) after the call)."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_new(e):
    save_toc(e)
    size = e.r(3)
    e.cl.rec("OperatorNew", (size,))
    e.set_r(3, e.alloc(size))


def stub_get_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, FONT[0])


def stub_get_child(e):
    e.cl.rec("GetChild", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, WIN[1])


def stub_get_next(e):
    e.cl.rec("GetNext", (e.cl.lab(e.r(3)),))
    e.set_r(3, next_of(e.r(3)))


def stub_setvalue(e):
    save_toc(e)
    e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "N"))
STUBS.internal(0x1018D930, CL.recorder("NumberCtor", "N"))
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "N"))
STUBS.internal(0x101898C8, CL.recorder("GradientCtor", "Ns"))
STUBS.internal(0x101DB624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, stub_get_font)
STUBS.internal(0x101DA118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101DE130, CL.recorder("SetClientDrawText", "Lu"))
STUBS.internal(0x101DBAC4, stub_get_child)
STUBS.internal(0x101DBB38, stub_get_next)
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x10180940, CL.recorder("UIMain_IndentClient", "Lhh"))
STUBS.internal(0x1023BAA8, stub_setvalue)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    VT[0] = T.persist_alloc(e, 0x80)
    FONT[0] = T.persist_alloc(e, 0x40)
    WIN.clear()
    for _ in range(NWIN):
        a = T.persist_alloc(e, 0x140)
        e.write(a + 0x118, struct.pack(">I", VT[0]))
        WIN.append(a)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {WIN[i]: i + 1 for i in range(NWIN)}
        fixed[FONT[0]] = 50
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call(self):
        T.call_ppc(self.e, FN, [WIN[0]])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {i + 1: lib.tpw_lc_block(i) for i in range(NWIN)}
        fixed[50] = lib.tpw_lc_block(7)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lc_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_lc_post(self.lib.tpw_lc_block(0))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lc_addr.restype, lib.tpw_lc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lc_block.restype, lib.tpw_lc_block.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_lc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lc_post.restype, lib.tpw_lc_post.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("type"), [case["type"]])
    s.words(s.g("font"), [case["font"]])


def observe(s, case):
    return {"log": s.log(), "type": s.rd_words(s.g("type"), 1), "font": s.rd_words(s.g("font"), 1)}


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def make(r, t, f=None):
    return {"queues": {}, "type": t & MASK, "font": r.choice([7, 0, 1, 12, 0xFFFFFFFF, r.randrange(0, 40)]) if f is None else f}


def edges():
    r = random.Random(5560)
    for t in (0, 1, 2, 3, 4, -1, -2, 5, 100, -100, 0x7FFFFFFF, -0x80000000):
        yield make(r, t)
    yield make(r, 2, 7)
    yield make(r, 0, 0)


def rnd(r):
    return make(r, r.randrange(-3, 7))


TESTS = [
    FnTest("LCPostFormat", ppc, host, edge=edges, rnd=rnd),
]
