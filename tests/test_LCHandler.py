"""Unit v5231, the ride list panel's row functions, against libtpw_unit_LCHandler.dylib (tests/unit_LCHandler.cpp), main app
("SimTheme Park"), src/CreateRideListPanel_LCHandler.cpp:
  LCHandler 0x101b7d94, LCEntryHandler 0x101b7f30, LCFormat 0x101b811c, LCPostFormat 0x101b827c
(the host names are RideListLCHandler, RideListLCEntryHandler, RideListLCFormat and RideListLCPostFormat: the plain names exist at other addresses).

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (ChangeInfoWindow, FillRideList, the string and title text functions, the window's setters and its default handler, the object
control, the bank account, the ride database and the golden ticket control, the font table, the allocation, the constructors and the two
virtual calls) are fakes on both sides that record their arguments and answer from the case's queues. The windows, fonts and draw objects
are bare labelled addresses; the font and the two row windows of LCPostFormat have the Mac's vtable chains on the PowerPC side (the glue
call is a fake that tells the font's height call from the window's SetValue by the table word it is given).

The world of a case: the panel pointer, the world pointer, the screen height, the font index, the cached row height, the object control's
upgrade cost and the ride record's golden ticket word (+0xc4). What is compared: the answer (the window the format makes as a label), the
cached row height, the font index and the event log.

No fused operations. Not tested: a screen height of 0 (divw by 0 is undefined on the host).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCHandler"
MASK = 0xFFFFFFFF

FN_HANDLER, FN_ENTRY, FN_FORMAT, FN_POST = 0x101B7D94, 0x101B7F30, 0x101B811C, 0x101B827C
SIZES = {FN_HANDLER: 360, FN_ENTRY: 432, FN_FORMAT: 320, FN_POST: 388}
OWN = set(SIZES)

CALLEES = dict(
    ChangeInfoWindow=0x101B7108, FillRideList=0x101B5C14, GetString=0x10180468, SetTitleText=0x10180794,
    CallDefault=0x101DE318, GetObjectControl=0x100FA0B8, GetBankAccount=0x1013EDFC, GetBalance=0x100F32B4,
    GetRideInfo=0x10154868, HasBeenBought=0x100FBFEC, CanIAfford=0x100FC0C8, SetFontColour=0x101DA3AC,
    GetFontTableEntry=0x10180270, WindowCtor=0x101D98D8, SetCallback=0x101DCBA8, CreateObject=0x101DC750,
    SpriteCtor=0x1018D11C, CashCtor=0x1018E190, SetClientDrawObject=0x101DB624, SetFont=0x101DA118, GetChild=0x101DBAC4,
    SetFontJust=0x101DA204, GetNext=0x101DBB38, FindDrawObject=0x101CD2D4, SetDrawObject=0x101DB474, Glue=0x1023BAA8,
)

ADDR = dict(gRideListPanel=0x10395998, gGameWorld=0x1037EBFC, gUIRealScreenHeight=0x10395BBA, gRideListFontIndex=0x102ACED8,
            gRideListRowHeight=0x10395984, gRideDatabase=0x1037F6C8)
CALLBACK_TVECTOR = 0x10267D70          # the TVector of LCEntryHandler
HOST_LAYOUT = [392, 4]       # the host's offset of the ride record's slot 46 (the Mac's 0xc4: the host's balance values are 8 bytes) and the control's cost

WORLD = 0x20000000
# labels of the objects the log names; the bogus ones are the same numbers on both sides
BOGUS = dict(win=0x30001000, panel=0x30002000, draw=0x30006000, created=0x30007000, bank=0x30009000)
LABELS = dict(win=1, panel=2, font=3, child=4, next=5, draw=6, created=7, ctrl=8, bank=9, ride=10)
HOSTONLY = dict(font=0x30003000, child=0x30004000, next=0x30005000)
OBJ = {}            # the PowerPC addresses of the persistent objects (setup fills it)


def s32(v):
    return T.s32(v)


def hi(e, i):
    return (e.r(i) >> 16) & 0xFFFF


def off(e, i):
    return (e.r(i) - WORLD) & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(name, args, retkey=None, retobj=None):
    """args(e, L) -> the tuple of the event; the answer comes from the queue `retkey` or is the object `retobj`."""
    def stub(e):
        L = e.cl
        L.rec(name, tuple(args(e, L)))
        if retkey:
            e.set_r(3, L.ret(retkey) & MASK)
        elif retobj:
            e.set_r(3, OBJ[retobj])
    STUBS.internal(CALLEES[name], stub)


fake("ChangeInfoWindow", lambda e, L: (s32(e.r(3)),))
fake("FillRideList", lambda e, L: (s32(e.r(3)), s32(e.r(4))))
fake("GetString", lambda e, L: (s32(e.r(3)),), retkey="GetString")
fake("SetTitleText", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK))
fake("CallDefault", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), retkey="CallDefault")
fake("GetObjectControl", lambda e, L: (off(e, 3), hi(e, 4), s32(e.r(5))), retobj="ctrl")
fake("GetBankAccount", lambda e, L: (off(e, 3),), retobj="bank")
fake("GetBalance", lambda e, L: (L.lab(e.r(3)),), retkey="GetBalance")
fake("GetRideInfo", lambda e, L: (1 if e.r(3) == ADDR["gRideDatabase"] else 0, hi(e, 4)), retobj="ride")
fake("HasBeenBought", lambda e, L: (off(e, 3), hi(e, 4)), retkey="HasBeenBought")
fake("CanIAfford", lambda e, L: (off(e, 3), hi(e, 4)), retkey="CanIAfford")
fake("SetFontColour", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
fake("GetFontTableEntry", lambda e, L: (e.r(3) & MASK,), retobj="font")
fake("SetCallback", lambda e, L: (L.lab(e.r(3)), 1 if e.r(4) == CALLBACK_TVECTOR else 0))
fake("CreateObject", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)),
                                   T.s16(e.r(9)), T.s16(e.r(10))), retobj="created")
fake("SetClientDrawObject", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4))))
fake("SetDrawObject", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4))))
fake("SetFont", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4))))
fake("SetFontJust", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
fake("GetChild", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), retobj="child")
fake("GetNext", lambda e, L: (L.lab(e.r(3)),), retobj="next")

STUBS.internal(CALLEES["FindDrawObject"], CL.recorder("FindDrawObject", "s", ret=lambda e, log: OBJ["draw"]))
STUBS.internal(CALLEES["SpriteCtor"], CL.recorder("SpriteCtor", "N"))
STUBS.internal(CALLEES["CashCtor"], CL.recorder("CashCtor", "N"))
STUBS.internal(CALLEES["WindowCtor"], CL.recorder("WindowCtor", "NLiihhhh"))


def glue(e):
    """__ptr_glue: the table word (1 the font's height call, 2 the window's SetValue) tells which virtual it is; the glue saves the TOC."""
    L = e.cl
    if e.u32(e.r(12)) == 1:
        L.rec("GetHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("GetHeight") & MASK)
    else:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


STUBS.internal(CALLEES["Glue"], glue)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    be = lambda *ws: b"".join(struct.pack(">I", w & MASK) for w in ws)
    tv_font = T.persist_alloc(e, 8, be(1, 0))
    tv_win = T.persist_alloc(e, 8, be(2, 0))
    vt_font = T.persist_alloc(e, 0x20, be(0, 0, 0, tv_font))
    vt_win = T.persist_alloc(e, 0x40, be(*([0] * 9), tv_win))        # +0x24
    OBJ["font"] = T.persist_alloc(e, 0x40, be(0, 0, 0, 0, vt_font))  # the vptr at +0x10
    OBJ["child"] = T.persist_alloc(e, 0x140, be(*([0] * 70), vt_win))   # +0x118
    OBJ["next"] = T.persist_alloc(e, 0x140, be(*([0] * 70), vt_win))
    OBJ["ctrl"] = T.persist_alloc(e, 0x40)
    OBJ["ride"] = T.persist_alloc(e, 0x900)
    OBJ.update(BOGUS)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {OBJ[k]: v for k, v in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, name):
        return OBJ[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK

    def call_format(self):
        r3, _, _ = T.call_ppc(self.e, FN_FORMAT, [])
        return self.L.lab(r3 & MASK)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {v: self.obj(k) for k, v in LABELS.items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lc_addr(name.encode())

    def obj(self, name):
        if name in BOGUS:
            return BOGUS[name]
        if name in HOSTONLY:
            return HOSTONLY[name]
        return self.lib.tpw_lc_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        win, m, a, b = args
        f = self.lib.tpw_lc_handler if fn == FN_HANDLER else self.lib.tpw_lc_entry
        return f(win, s32(m), s32(a), s32(b)) & MASK

    def call_format(self):
        return self.lib.tpw_lc_format()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lc_addr.restype, lib.tpw_lc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lc_layout.argtypes = [C.POINTER(C.c_int64)]
    for f in (lib.tpw_lc_handler, lib.tpw_lc_entry):
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_lc_format.restype, lib.tpw_lc_format.argtypes = C.c_int64, []
    lib.tpw_lc_post.restype, lib.tpw_lc_post.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("gGameWorld"), [WORLD])
    s.words(s.g("gRideListPanel"), [s.obj("panel") if case["panel"] else 0])
    s.half(s.g("gUIRealScreenHeight"), case["screen"])
    s.words(s.g("gRideListFontIndex"), [case["font"]])
    s.words(s.g("gRideListRowHeight"), [case["rowh"]])
    s.words(s.obj("ctrl") + 4, [case["cost"]])
    s.words(s.obj("ride") + (0xC4 if isinstance(s, Ppc) else HOST_LAYOUT[0]), [case["golden"]])


def execute(s, case):
    op = case["op"]
    if op == "format":
        return s.call_format()
    if op == "post":
        if isinstance(s, Ppc):
            T.call_ppc(s.e, FN_POST, [s.obj("win")])
        else:
            s.lib.tpw_lc_post(s.obj("win"))
        return 0
    fn = FN_HANDLER if op == "handler" else FN_ENTRY
    return s.call(fn, [s.obj("win")] + case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "rowh": s.rd_words(s.g("gRideListRowHeight"), 1),
            "font": s.rd_words(s.g("gRideListFontIndex"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SCREENS = [480, 600, 768, 1, -1, 0x7FFF, -0x8000, 1024, 3, -7]
HEIGHTS = [0, 1, 12, 16, 0x7FFF, 0x8000, 0xFFFF, 0x10005, 0xFFFF8000, 0x12345678]


def base(r, op):
    q = {"GetString": [r.getrandbits(32)], "CallDefault": [r.getrandbits(32)], "GetBalance": [r.choice([0, 1, -1, r.randrange(-5000, 5000)]) & MASK],
         "GetHeight": [r.choice(HEIGHTS + [r.getrandbits(32), r.randrange(0, 40)])], "HasBeenBought": [r.choice([0, 1])],
         "CanIAfford": [r.choice([0, 1])]}
    return {"op": op, "queues": q, "panel": r.choice([1, 1, 0]), "screen": r.choice(SCREENS) & 0xFFFF,
            "font": r.choice([7, 0, 1, 0xFFFFFFFF, r.getrandbits(32)]),
            "rowh": r.choice([0, 0, 1, 0x7FFF, 0x8000, 0xFFFF8000, r.getrandbits(32)]),
            "cost": r.choice([0, 1, -1, r.randrange(-5000, 5000)]) & MASK, "golden": r.choice([0, 1, -1, r.getrandbits(32)]) & MASK,
            "args": []}


def handler_case(r, a=None, m=0x101, b=1):
    c = base(r, "handler")
    c["args"] = [m, r.choice([0x1F9, 0x1FA, 0x1FB, 0x1FC, 0x1FD, 0x1FE, 0x1F0, 0, -1, r.getrandbits(32)]) if a is None else a, b]
    if a is None and m == 0x101 and r.random() < 0.7:
        c["args"][1] = r.randrange(0x1F8, 0x200)
    return c


def handler_edges():
    r = random.Random(5231)
    for a in range(0x1F7, 0x201):
        yield handler_case(r, a)
    for a in (0x1FA, 0x1FB, 0x1FC, 0x1FD):
        yield handler_case(r, a, 0x100, 1)
        yield handler_case(r, a, 0x102, 1)
        yield handler_case(r, a, 0x101, 0)
        yield handler_case(r, a, 0x101, 2)
        yield handler_case(r, a, 0x101, -1)
    for a in (0x1FA, 0x1FB, 0x1FC, 0x1FD):
        c = handler_case(r, a)
        c["panel"] = 0
        yield c


def handler_random(r):
    return handler_case(r, None, r.choice([0x101, 0x101, 0x100, 0x102, 0, r.getrandbits(32)]), r.choice([1, 1, 0, 2, -1, r.getrandbits(32)]))


def entry_case(r, m=0x403, b=None, cost=None, balance=None):
    c = base(r, "entry")
    if b is None:
        b = r.choice([r.randrange(0, 0x10000), r.randrange(0x8000, 0x10000), r.getrandbits(32)])
    c["args"] = [m, r.getrandbits(32), b]
    if cost is not None:
        c["cost"] = cost & MASK
    if balance is not None:
        c["queues"]["GetBalance"] = [balance & MASK]
    return c


def entry_edges():
    r = random.Random(5232)
    for m in (0x403, 0x402, 0x404, 0x1E, 0, -1, 0x1F, 0x403 + 0x10000):
        yield entry_case(r, m, 0)
        yield entry_case(r, m, 0xFFFF)
    for b in (0xFFFF, 0x8000, 0x7FFF, 0, 1, 0x12348000, 0x1234FFFF, 0x12347FFF, 0xFFFF0000, 0x80000000, 0x7FFFFFFF):
        yield entry_case(r, 0x403, b)
    for golden in (0, 1, -1):
        for bought in (0, 1):
            for afford in (0, 1):
                c = entry_case(r, 0x403, 5)
                c["golden"] = golden & MASK
                c["queues"]["HasBeenBought"] = [bought]
                c["queues"]["CanIAfford"] = [afford]
                yield c
    for cost, balance in ((100, 99), (100, 100), (100, 101), (0, 0), (-1, -2), (-1, -1), (-1, 0), (0x7FFFFFFF, -0x80000000),
                          (-0x80000000, 0x7FFFFFFF), (5, 0)):
        c = entry_case(r, 0x403, 9, cost, balance)
        c["golden"] = 0
        yield c
        c = entry_case(r, 0x403, 9, cost, balance)
        c["golden"] = 7
        c["queues"]["HasBeenBought"] = [1]
        yield c


def entry_random(r):
    return entry_case(r, r.choice([0x403, 0x403, 0x403, 0x402, 0, r.getrandbits(32)]))


def format_case(r, rowh=None, screen=None, height=None, font=None):
    c = base(r, "format")
    if rowh is not None:
        c["rowh"] = rowh & MASK
    if screen is not None:
        c["screen"] = screen & 0xFFFF
    if height is not None:
        c["queues"]["GetHeight"] = [height & MASK]
    if font is not None:
        c["font"] = font & MASK
    return c


def format_edges():
    r = random.Random(5233)
    for rowh in (0, 1, -1, 0x7FFF, 0x8000, 0x12345, 0xFFFF8000, 0x10000, 0x80000000):
        yield format_case(r, rowh)
    for screen in SCREENS:
        for height in (0, 1, 15, 0x7FFF, 0x8000, 0xFFFF, 0x10005):
            yield format_case(r, 0, screen, height)
    for font in (7, 0, 1, 0xFFFFFFFF, 0x80000000):
        yield format_case(r, 0, 480, 16, font)


def format_random(r):
    return format_case(r)


def post_edges():
    r = random.Random(5234)
    for font in (7, 0, 1, 0xFFFFFFFF, 0x80000000, 0x12345678):
        c = base(r, "post")
        c["font"] = font
        yield c


def post_random(r):
    return base(r, "post")


TESTS = [
    FnTest("LCHandler", ppc, host, edge=handler_edges, rnd=handler_random),
    FnTest("LCEntryHandler", ppc, host, edge=entry_edges, rnd=entry_random),
    FnTest("LCFormat", ppc, host, edge=format_edges, rnd=format_random),
    FnTest("LCPostFormat", ppc, host, edge=post_edges, rnd=post_random),
]
