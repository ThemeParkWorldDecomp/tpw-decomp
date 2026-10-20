"""Unit v5660, the online panel's contact helpers, against libtpw_unit_ContactMenu.dylib (tests/unit_ContactMenu.cpp), main app
("SimTheme Park"), src/OnlineParkFindPanelMsg_ContactMenu.cpp:
  FillBuddyList 0x101e8608, ButtonsMenuMsg 0x101e87b8, ContactMenu 0x101e8b98

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (window lookups, list reads, the online control and its commands, the rectangle, the mouse) are fakes on both sides that record
their arguments and answer from the case's queues. Windows and name texts are blocks both sides allocate once; a pointer reaches the log as
the label of its block. The string commands log the first four characters of the name string they receive.

The world of a case: the panel, menu and control globals, the z of the two menu windows, the queues. What is compared: the answer, the
globals the functions write (gOnlineWord88, gOnlineControl) and the event log.

No fused operations. Not tested: GetData answering without writing its data word (the original reads the stack garbage), a NULL menu
window (the original reads its z), a NULL name text (the original's string constructor reads it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ContactMenu"
MASK = 0xFFFFFFFF

FN_FILL, FN_BTN, FN_CONTACT = 0x101E8608, 0x101E87B8, 0x101E8B98
SIZES = {FN_FILL: 152, FN_BTN: 804, FN_CONTACT: 844}
OWN = set(SIZES)

ADDR = dict(gOnlinePanel=0x10395E98, gE90=0x10395E90, gE8c=0x10395E8C, gW88=0x10395E88, gControl=0x10396D28)

# blocks both sides own for ever: name -> size (label)
BLOCKS = {"b1": 0x200, "b2": 0x200, "b3": 0x200, "b4": 0x200, "b5": 0x200, "b6": 0x200, "t0": 16, "t1": 16, "t2": 16,
          "ctl": 0xE8C, "tab": 128, "tabptr": 8}
LABELS = {"b1": 1, "b2": 2, "b3": 3, "b4": 4, "b5": 5, "b6": 6, "t0": 11, "t1": 12, "t2": 13, "ctl": 20}
TEXTS = [(65, 110, 110), (66, 111, 98), (67, 121, 33)]
HOST_LAYOUT = [360, 8, 10, 0xE8C]   # mZ of InterfaceWindow, mLeft and mTop of CoolRect, sizeof COnlineControl
PPC_BLK = {}


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tabword(e, idx):
    return e.u32(PPC_BLK["tab"] + 4 * idx)


def r_getwindow(e, log):
    return tabword(e, log.ret("GetWindow"))


def r_getdata(e, log):
    e.write(e.r(5), struct.pack(">I", log.ret("GetData") & MASK))
    return 1


def r_getitem(e, log):
    e.write(e.r(6), struct.pack(">I", tabword(e, log.ret("GetItem"))))
    return 0


def r_getpos(e, log):
    v = log.ret("GetPosition")
    e.write(e.r(4) + 4, struct.pack(">hh", s16(v >> 16), s16(v)))
    return 0


def r_mouse(e, log):
    v = log.ret("Mouse")
    e.write(e.r(3), struct.pack(">h", s16(v >> 16)))
    e.write(e.r(4), struct.pack(">h", s16(v)))
    return 0


def r_extra(e, log):
    return tabword(e, log.ret("ExtraParam"))


def rec(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


rec(0x101DBE40, "GetWindow", "Li", r_getwindow)
rec(0x101D41F0, "GetData", "Lix", r_getdata)
rec(0x101D40BC, "GetItem", "Liix", r_getitem)
rec(0x10213870, "COnlineControl::COnlineControl", "N")
rec(0x10217F34, "IsThisMe", "LL", "b")
rec(0x101DDF20, "link", "LLbb")
rec(0x101C7448, "CoolRect::CoolRect", "L")
rec(0x101DB884, "GetPosition", "Lxh", r_getpos)
rec(0x101CA900, "UI_GetMouseXY", "xx", r_mouse)
rec(0x101DA9D0, "ShiftPosition", "Lhh")
rec(0x101DD8BC, "setZ", "Lh")
rec(0x101DC1EC, "Show", "Lh")
rec(0x101DCCEC, "CaptureKb", "Li")
rec(0x101C74E0, "CoolRect::~CoolRect", "Lx")
rec(0x101CB1D0, "SetState", "Lbb")
rec(0x101DCFD0, "setExtraParam", "LhL")
rec(0x101DD030, "getExtraParam", "Lh", r_extra)
rec(0x101DE318, "CallDefaultWindowHandler", "Liii", "i")
rec(0x101E8B18, "HideButtonsMenu", "")
rec(0x101D3CC8, "RemoveAllItems", "L")
rec(0x1021C02C, "UpdateBuddyList", "L")


def str_ctor(e):
    e.write(e.r(3), struct.pack(">I", e.r(4) & MASK))


STUBS.internal(0x10146A14, str_ctor)
STUBS.internal(0x101468F8, lambda e: None)


def command(addr, name):
    def fn(e):
        obj = e.u32(e.r(4))
        chars = struct.unpack(">4H", e.read(obj, 8))
        e.cl.rec(name, (e.cl.lab(e.r(3)),) + chars)
    STUBS.internal(addr, fn)


for _a, _n in ((0x102184A0, "BlackMark"), (0x10219034, "IgnoreOn"), (0x1021922C, "IgnoreOff"), (0x1021899C, "Goto"),
               (0x10218C44, "BuddyOn"), (0x10218E3C, "BuddyOff"), (0x102187F4, "Locate"), (0x1021864C, "Page")):
    command(_a, _n)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name, size in BLOCKS.items():
        PPC_BLK[name] = T.persist_alloc(runner.emu, size)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptrs(self, a, ps):
        """Pointer words: 4 bytes on the Mac, 8 on the host."""
        self.write(a, b"".join(struct.pack(self.pfmt, p) for p in ps))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def blk(self, name):
        return self.blocks[name]

    def label_of(self, v):
        """The block a word names (compared by its low 32 bits, as a 32-bit global holds it)."""
        if v == 0:
            return 0
        for n in BLOCKS:
            if self.blocks[n] & MASK == v & MASK:
                return n
        return "other"


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    zoff = 0xDC

    def __init__(self, e, case):
        self.e = e
        self.blocks = PPC_BLK
        fixed = {PPC_BLK[n]: l for n, l in LABELS.items()}
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

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK if fn == FN_BTN else 0   # the other two answer nothing


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    zoff = 360   # the host's InterfaceWindow is wider than the Mac's (pointers), so mZ is not at 0xdc

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks = lib.blk
        CL.host_begin(lib, {l: lib.blk[n] for n, l in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name == "gControl":
            return self.lib.tpw_cm_addr(b"gControl")
        return self.lib.tpw_cm_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        a = [x & MASK for x in args]
        if fn == FN_FILL:
            self.lib.tpw_cm_fill()
            return 0
        if fn == FN_BTN:
            return self.lib.tpw_cm_buttons(args[0], s32(a[1]), s32(a[2]), s32(a[3])) & MASK   # args[0] is a full 64-bit address
        self.lib.tpw_cm_contact(s32(a[0]))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cm_addr.restype, lib.tpw_cm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cm_alloc.restype, lib.tpw_cm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cm_fill.restype, lib.tpw_cm_fill.argtypes = None, []
    lib.tpw_cm_buttons.restype, lib.tpw_cm_buttons.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_cm_contact.restype, lib.tpw_cm_contact.argtypes = None, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.blk = {name: lib.tpw_cm_alloc(size) for name, size in BLOCKS.items()}
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for n in ("b1", "b2", "b3", "b4", "b5", "b6", "ctl"):
        s.write(s.blk(n), bytes(0x200 if n != "ctl" else 0x20))
    s.write(s.blk("b5") + s.zoff, struct.pack(s.fmt[0] + "h", s16(case["z5"])))
    s.write(s.blk("b6") + s.zoff, struct.pack(s.fmt[0] + "h", s16(case["z6"])))
    for i, t in enumerate(TEXTS):
        s.write(s.blk("t%d" % i), struct.pack(s.fmt[0] + "4H", *t, 0))
    b = s.blk
    s.ptrs(b("tab"), [0, b("b1"), b("b2"), b("b3"), b("b4"), b("b5"), b("b6"), 0, b("t0"), b("t1"), b("t2")])
    if isinstance(s, Host):
        s.ptrs(s.g("tabptr"), [b("tab")])
    s.ptrs(s.g("gOnlinePanel"), [b("b1")])
    s.ptrs(s.g("gE90"), [b("b5")])
    s.ptrs(s.g("gE8c"), [b("b6")])
    s.words(s.g("gW88"), [b("b3") if case["w88"] else 0])
    s.ptrs(s.g("gControl"), [b("ctl") if case["ctl"] else 0])


def observe(s, answer):
    return {"answer": answer, "log": s.log(), "w88": s.label_of(s.rd_word(s.g("gW88"))),
            "ctl": s.label_of(s.rd_ptr(s.g("gControl")))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, s.call(case["fn"], case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

Z_EDGES = [0, 1, -1, -4, -5, 32763, 32764, 32767, -32768, 100]
SHORTS = [0, 1, -1, 5, -5, 300, 32767, -32768, 1000]


def pack_xy(x, y):
    return ((x & 0xFFFF) << 16) | (y & 0xFFFF)


def base(r, fn, args):
    n = r.randrange(1, 6)
    return {
        "fn": fn, "args": args,
        "queues": {
            "GetWindow": [r.randrange(1, 7) for _ in range(n)],
            "GetData": [r.choice([0, 1, 8, 9, 2, 0xFFFFFFFF, 0x10, r.getrandbits(32)])],
            "GetItem": [r.randrange(1, 7)],
            "IsThisMe": [r.choice([0, 0, 1])],
            "GetPosition": [pack_xy(r.choice(SHORTS), r.choice(SHORTS))],
            "Mouse": [pack_xy(r.choice(SHORTS), r.choice(SHORTS))],
            "ExtraParam": [r.randrange(8, 11)],
            "CallDefaultWindowHandler": [r.choice([0, 1, -1, r.getrandbits(32)]) & MASK],
        },
        "z5": r.choice(Z_EDGES), "z6": r.choice(Z_EDGES), "w88": r.choice([0, 1]), "ctl": r.choice([0, 1]),
    }


def fill_case(r):
    return base(r, FN_FILL, [])


def fill_edges():
    r = random.Random(11)
    for ctl in (0, 1):
        c = fill_case(r)
        c["ctl"] = ctl
        yield c


IDS = [0, 0x1788, 0x1789, 0x178A, 0x178B, 0x178C, 0x178D, 0x178E, 0x178F, 0x1790, 0x2000, 0x3002, 0x3003, 0x3004, 0x3005, 0x3006,
       0x3007, 0x3008, 0x3009, -1]


def btn_case(r, msg=None, a=None, b=None):
    msg = r.choice([0x100, 0x100, 0x100, 0, 0x1E, 0xFF, 0x101, -1]) if msg is None else msg
    a = r.choice(IDS) if a is None else a
    b = r.choice([0, 1, 1, 2, -1]) if b is None else b
    return base(r, FN_BTN, [LABEL_WIN, msg, a, b])


LABEL_WIN = None   # set below: the window is block b2, whose address differs per side


def btn_edges():
    r = random.Random(12)
    for a in IDS:
        for b in (0, 1, 2):
            for ctl in (0, 1):
                c = btn_case(r, 0x100, a, b)
                c["ctl"] = ctl
                yield c
    for msg in (0, 0x1E, 0xFF, 0x101, -1, 0xFF00):
        yield btn_case(r, msg, 0x178F, 1)


def btn_random(r):
    return btn_case(r)


def contact_case(r, pos=None):
    return base(r, FN_CONTACT, [r.choice([0, 1, -1, 7, r.getrandbits(32)]) if pos is None else pos])


def contact_edges():
    r = random.Random(13)
    for flags in (0, 1, 8, 9, 2, 0xFFFFFFFF):
        for me in (0, 1):
            for ctl in (0, 1):
                c = contact_case(r)
                c["queues"]["GetData"] = [flags]
                c["queues"]["IsThisMe"] = [me]
                c["ctl"] = ctl
                yield c
    for z in Z_EDGES:
        c = contact_case(r)
        c["queues"]["GetData"] = [1]
        c["queues"]["IsThisMe"] = [0]
        c["z5"] = c["z6"] = z
        yield c


def contact_random(r):
    return contact_case(r)


# the window argument of ButtonsMenuMsg is block b2: resolved when the call is made
_orig_ppc_call = Ppc.call
_orig_host_call = Host.call


def _ppc_call(self, fn, args):
    if fn == FN_BTN:
        args = [PPC_BLK["b2"]] + list(args[1:])
    return _orig_ppc_call(self, fn, args)


def _host_call(self, fn, args):
    if fn == FN_BTN:
        args = [self.lib.blk["b2"]] + list(args[1:])
    return _orig_host_call(self, fn, args)


Ppc.call = _ppc_call
Host.call = _host_call

TESTS = [
    FnTest("FillBuddyList", ppc, host, edge=fill_edges, rnd=fill_case),
    FnTest("ButtonsMenuMsg", ppc, host, edge=btn_edges, rnd=btn_random),
    FnTest("ContactMenu", ppc, host, edge=contact_edges, rnd=contact_random),
]
