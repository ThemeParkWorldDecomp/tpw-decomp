"""Unit v5698, CreateLoansPanel, against libtpw_unit_CreateLoansPanel.dylib (tests/unit_CreateLoansPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_loansPanel.cpp:
  CreateLoansPanel 0x101a4de4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments: the windows are 64 slots of a block (a window is its slot, GetWindow answers the slot of
the id), the strings are offsets of a block (UIMain_GetString answers base + 4 * id), the game type constructor, Build, the summary window
(NULL or not), the history values and the eight loans answer from the case's queues.

The world of a case: the loans panel global (NULL or set), the game type object and its guard byte, the world and game window globals,
the lender list mode and the column colour. What is compared: the log of every call, the panel global (as a slot), the guard byte and
the game type word.

No fused operations. Not tested: nothing is left out (the windows the original does not test for NULL are never NULL here, as the fakes
always answer a slot).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateLoansPanel"
MASK = 0xFFFFFFFF

FN = 0x101A4DE4
SIZES = {FN: 2320}
OWN = set(SIZES)

ADDR = dict(panel=0x10394980, gametype=0x102B3AA0, guard=0x102B3AA4, world=0x1037EBFC, window=0x103949E4, mode=0x1039497C,
            colour=0x102AA83C)
SCRIPT = 0x102AA4A4
TV_BUILDCB, TV_LISTCB, TV_FORMAT, TV_POST = 0x10267C18, 0x10267BF8, 0x10267C08, 0x10267C00

HOST_LAYOUT = [0x1F100, 0x250, 0x10, 0x14, 0x20, 4]

SLOT = 0x200
K_ROOT, K_ANALYSER, K_BANK, K_WORLD, K_DRAWOBJ, K_FONT, K_GAMEWINDOW = 0, 58, 57, 56, 61, 62, 63
CUR = {}   # the PowerPC side's blocks of the running case


def s32(v):
    return T.s32(v)


def slot_of_id(i):
    if 0x12C4BD <= i <= 0x12C4CA:
        return i - 0x12C4BD + 1
    if 0x10 <= i <= 0x15:
        return 20 + i
    return 60


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def slot(k):
    return CUR["win"] + k * SLOT


def off(p):
    return -1 if p == 0 else p - CUR["str"]


def fake(addr, name, args=None, ret=None, glue=False):
    def stub(e):
        L = e.cl
        if glue:   # the glue saves the TOC at 0x14(r1) and the caller reloads r2 from there
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        L.rec(name, tuple(args(e, L)) if args else ())
        if ret:
            e.set_r(3, ret(e, L) & MASK)
    STUBS.internal(addr, stub)


def ctype_ctor(e, L):
    L.rec("CGameType", (1 if e.r(3) == ADDR["gametype"] else 0,))
    e.write(e.r(3), struct.pack(">I", L.ret("gtype") & MASK))


STUBS.internal(0x1016F5D8, lambda e: ctype_ctor(e, e.cl))
fake(0x10180514, "CheckPanel", None, lambda e, L: 0)
fake(0x101A57FC, "SetLastPanel", lambda e, L: (s32(e.r(3)), s32(e.r(4))))
fake(0x101DEB78, "Build", lambda e, L: (L.lab(e.r(3)), int(e.r(4) == SCRIPT) + 2 * int(e.r(5) == TV_BUILDCB)),
     lambda e, L: slot(K_ROOT) if L.ret("build") != 0 else 0)
fake(0x101805A0, "SetPanel", lambda e, L: (L.lab(e.r(3)),))
fake(0x10180468, "GetString", lambda e, L: (s32(e.r(3)),), lambda e, L: CUR["str"] + s32(e.r(3)) * 4)
fake(0x10180794, "SetTitleText", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), off(e.r(5))))


def get_window(e):
    L = e.cl
    i = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), i))
    if i == 0x12C4C1 and L.ret("summary") == 0:
        e.set_r(3, 0)
    else:
        e.set_r(3, slot(slot_of_id(i)))


STUBS.internal(0x101DBE40, get_window)
fake(0x1013EBC8, "GetParkAnalyser", lambda e, L: (L.lab(e.r(3)),), lambda e, L: slot(K_ANALYSER))
fake(0x102384B8, "new", lambda e, L: (s32(e.r(3)),), lambda e, L: e.alloc(e.r(3)), glue=True)
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "N"))
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "N"))
fake(0x101DB624, "SetClientDrawObject", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4))))
fake(0x10180270, "GetFont", lambda e, L: (e.r(3),), lambda e, L: slot(K_FONT))
fake(0x101DA118, "SetFont", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4))))
fake(0x101DE130, "SetClientDrawText", lambda e, L: (L.lab(e.r(3)), off(e.r(4))))
fake(0x101DA3AC, "SetFontColour", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
fake(0x101DA204, "SetFontJust", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
fake(0x1023BAA8, "SetValue", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), glue=True)


def get_value(e):
    L = e.cl
    L.rec("GetValue", (s32(e.r(3) - slot(K_ANALYSER)), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">I", L.ret("hist") & MASK))
    e.set_r(3, 1)


STUBS.internal(0x1001C2A8, get_value)
fake(0x1013EDFC, "GetBankAccount", lambda e, L: (L.lab(e.r(3)),), lambda e, L: slot(K_BANK))


def get_loan(e):
    L = e.cl
    L.rec("GetLoanInfo", (L.lab(e.r(4)), e.r(5)))
    e.write(e.r(3), struct.pack(">8I", *[L.ret("loan") & MASK for _ in range(8)]))


STUBS.internal(0x100F3C44, get_loan)
fake(0x101D2864, "SetColumnFormatsAll", lambda e, L: (L.lab(e.r(3)), int(e.r(4) == TV_FORMAT), 2 * int(e.r(5) == TV_POST)))
fake(0x101D2614, "SetColumnType", lambda e, L: (L.lab(e.r(3)), T.s16(e.r(4)), s32(e.r(5))))


def find_draw(e):
    L = e.cl
    L.rec("FindDrawObject", tuple(e.read(e.r(3), 8)))
    e.set_r(3, slot(K_DRAWOBJ))


STUBS.internal(0x101CD2D4, find_draw)
fake(0x101D1EB4, "SetSelectionObject", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4))))
fake(0x101DCBA8, "SetCallbackFunction", lambda e, L: (L.lab(e.r(3)), int(e.r(4) == TV_LISTCB)))
fake(0x101A4A38, "FillLenderList", lambda e, L: (L.lab(e.r(3)),))
fake(0x101818E4, "MakeColumnButton", lambda e, L: (L.lab(e.r(3)), off(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)),
                                                    s32(e.r(9))))
fake(0x101CC148, "SetCurrent", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), lambda e, L: 1)
fake(0x101A4230, "UpdateListHeadings")
fake(0x101821CC, "Advisor", lambda e, L: (s32(e.r(3)),))


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
        self.write(a, struct.pack(self.pfmt, v))

    def get_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def slot(self, k):
        return self.win + k * SLOT

    def as_slot(self, v):
        """A pointer word as a slot label (so the two memories agree)."""
        if v == 0:
            return 0
        for k in range(64):
            if v == self.slot(k):
                return 1000 + k
        return -1


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(64 * SLOT)
        self.str = e.alloc(0x1000)
        CUR["win"], CUR["str"] = self.win, self.str
        for k in range(64):
            e.write(self.win + k * SLOT + 0x118, struct.pack(">I", self.win + k * SLOT + 0x180))
        self.L = e.cl = CL.PpcLog({self.win + k * SLOT: 200 + k for k in range(64)}, case["queues"])
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
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"   # the host's pointers are 64 bits (its blocks are not 32-bit addressable)

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win = lib.tpw_ccl_alloc(64 * SLOT)
        self.str = lib.tpw_ccl_alloc(0x1000)
        lib.tpw_ccl_blocks(self.win, self.str)
        CL.host_begin(lib, {200 + k: self.win + k * SLOT for k in range(64)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ccl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_ccl_CreateLoansPanel()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ccl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "blocks").argtypes = [C.c_uint64, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.put_ptr(s.g("panel"), s.slot(case["panel"]) if case["panel"] >= 0 else 0)
    s.words(s.g("gametype"), [case["mtype"]])
    s.write(s.g("guard"), bytes([case["guard"]]))
    s.put_ptr(s.g("world"), s.slot(K_WORLD))
    s.put_ptr(s.g("window"), s.slot(K_GAMEWINDOW))
    s.words(s.g("mode"), [case["mode"]])
    s.words(s.g("colour"), [case["colour"]])


def observe(s, case):
    return {"log": s.log(), "panel": s.as_slot(s.get_ptr(s.g("panel"))), "guard": s.read(s.g("guard"), 1),
            "gametype": s.rd_word(s.g("gametype")), "mode": s.rd_word(s.g("mode")), "colour": s.rd_word(s.g("colour"))}


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

W = [0, 1, 2, 3, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 100, 5000, 0xFFFFFF00]


def rw(r):
    return r.choice(W) if r.random() < 0.5 else r.getrandbits(32)


def base(r):
    loans = []
    for _ in range(8):
        loans += [r.choice([0, 1]), rw(r), rw(r), rw(r), rw(r) if r.random() < 0.5 else r.randrange(0, 5000), r.choice([0, 0, 1, 7]),
                  r.randrange(0, 30), r.randrange(0, 20)]
    return {"panel": -1, "mtype": r.choice([0, 1, 2, 3, 0xFFFFFFFF]), "guard": r.choice([0, 1]), "mode": r.choice([0, 1, 2, 0xFFFFFFFF]),
            "colour": rw(r),
            "queues": {"gtype": [r.choice([0, 1, 2, 3, 0xFFFFFFFF])], "build": [1], "summary": [1], "loan": loans,
                       "hist": [rw(r), rw(r)]}}


def edges():
    r = random.Random(5698)
    c = base(r)
    c["panel"] = 5
    yield c                                                    # a panel exists already: nothing happens
    for guard in (0, 1):
        for mtype in (0, 1, 2, 3, 0xFFFFFFFF):
            for gtype in (0, 2):
                c = base(r)
                c["guard"], c["mtype"] = guard, mtype
                c["queues"]["gtype"] = [gtype]
                yield c                                        # the game type object: constructed or not, easy or not
    c = base(r)
    c["guard"], c["mtype"] = 1, 0
    c["queues"]["build"] = [0]
    yield c                                                    # Build fails
    for mode in (0, 1, 2):
        for summary in (0, 1):
            c = base(r)
            c["guard"], c["mtype"], c["mode"] = 1, 0, mode
            c["queues"]["summary"] = [summary]
            yield c
    c = base(r)
    c["guard"], c["mtype"] = 1, 0
    c["queues"]["loan"] = [0, 0, 0, 0, 7, 1, 0, 0] * 8         # every loan bought
    c["queues"]["hist"] = [1000, 0x7FFFFFFF]
    yield c
    c["queues"]["loan"] = [0, 0, 0, 0, 7, 0, 0, 0] * 8         # none bought
    yield c
    c["queues"]["loan"] = [0, 0, 0, 0, 0x80000000, 1, 0, 0] * 8   # the sum wraps
    yield c


def rnd(r):
    c = base(r)
    if r.random() < 0.8:
        c["guard"], c["mtype"] = 1, r.choice([0, 1, 3])
    if r.random() < 0.05:
        c["panel"] = r.randrange(0, 20)
    if r.random() < 0.1:
        c["queues"]["build"] = [0]
    if r.random() < 0.2:
        c["queues"]["summary"] = [0]
    return c


TESTS = [
    FnTest("CreateLoansPanel", ppc, host, edge=edges, rnd=rnd),
]
