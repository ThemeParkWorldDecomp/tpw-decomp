"""Unit v5885, the entry price panel (the admission fee panel), against libtpw_unit_CreateEntryPricePanel_2.dylib
(tests/unit_CreateEntryPricePanel_2.cpp), main app ("SimTheme Park"), src/game/ui/ui_entryPricePanel.cpp:
  CreateEntryPricePanel 0x1019d660, EntryPricePanelTitleMsg 0x1019d544, EntryPricePanelMsg 0x1019d350

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the window members, the UI_main helpers, the world, the bank account, the other panels, the allocation and the two virtual
calls through the table at window +0x118) is a fake on both sides that records its arguments (pointers as labels) and answers from the case.

The world of a case: the panel pointer, the fee and closed globals, the game type word and its guard byte, the game window, the world (its
gates thing), the thing array rows 0 and the gates row, the bank account (its fee). What is compared: the answer, the globals, the game
type, and the event log.

No fused operations. Not tested: the guard byte of the game type being 0 on the host through the inline static is covered by the raw
storage CreateLoansPanel's unit also uses (the guard is a plain byte here); a gates index past the thing array (the original reads
past the table).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateEntryPricePanel_2"
MASK = 0xFFFFFFFF

FN_CREATE, FN_TITLE, FN_MSG = 0x1019D660, 0x1019D544, 0x1019D350
SIZES = {FN_CREATE: 892, FN_TITLE: 216, FN_MSG: 436}
OWN = set(SIZES)

ADDR = dict(parksc=0x10394848, gamewin=0x103949E4, world=0x1037EBFC, thing=0x1034CBFC, script=0x102A96A8, fee=0x10394844,
            closed=0x10394840, gtstore=0x102B3AA0, gtguard=0x102B3AA4, msgfn=0x10267B28, titlefn=0x10267B20)
HOST_LAYOUT = [0x1DA732, 0x118]

BLK = ["game", "panel", "t4af", "t4b0", "t4ae", "t4b1", "t4ac", "self", "obj", "name", "str", "font", "new1", "new2", "bank", "vt"]
ID2BLK = {0x4F3AF: "t4af", 0x4F3B0: "t4b0", 0x4F3AE: "t4ae", 0x4F3B1: "t4b1", 0x4F3AC: "t4ac"}
WINDOWS = ["game", "panel", "t4af", "t4b0", "t4ae", "t4b1", "t4ac", "self"]
L_WORLD, L_SCRIPT, L_MSGFN, L_TITLEFN, L_GT = 20, 21, 22, 23, 24
MAGIC_SETVALUE, MAGIC_ENABLE = 0xA0A00024, 0xA0A0001C
WORLD = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def blk(e, name):
    return e.blk[name]


def restore_toc(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def rec_ret(name, spec, fn):
    return CL.recorder(name, spec, fn)


def install(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


def rd32(e, a):
    return s32(struct.unpack(">I", e.read(a & MASK, 4))[0])


def get_window(e, log):
    return blk(e, ID2BLK[s32(e.r(4))]) if s32(e.r(4)) in ID2BLK else 0


def new_block(e, log):
    n = e.newn
    e.newn += 1
    return blk(e, "new1" if n == 0 else "new2")


install(0x10180514, "CheckPanel", "")
install(0x101A57FC, "SetLastPanel", "ii")
install(0x101DEB78, "Build", "LLL", lambda e, l: blk(e, "panel"))
install(0x101805A0, "SetPanel", "L")
install(0x101DBE40, "GetWindow", "Li", get_window)
install(0x101DCBA8, "SetCallbackFunction", "LL")
install(0x1000DDE8, "Assert", "i")
install(0x101810FC, "GetObjectName", "L", lambda e, l: blk(e, "name"))
install(0x10180794, "SetTitleText", "LiL")
install(0x1018D11C, "SpriteCtor", "L")
install(0x1018DE60, "CashCtor", "L")
install(0x101DB624, "SetClientDrawObject", "LL")
install(0x101DA204, "SetFontJust", "Lii")
install(0x10180270, "GetFontTableEntry", "i", lambda e, l: blk(e, "font"))
install(0x101DA118, "SetFont", "LL")
install(0x101DA3AC, "SetFontColour", "Liiii")
install(0x10180468, "GetString", "i", lambda e, l: blk(e, "str"))
install(0x101DE130, "SetClientDrawText", "LL")
install(0x1013EDFC, "GetBankAccount", "L", lambda e, l: blk(e, "bank"))
install(0x1013FF7C, "IsParkClosed", "L", "i")
install(0x101CB1D0, "SetState", "Lii")
install(0x101821CC, "AdvisorMessage", "i")
install(0x1019DA08, "RemoveEntryPricePanel", "")
install(0x100F3A40, "SetAdmissionFee", "Lu")
install(0x1018227C, "CancelAdvisorMessage", "i")
install(0x101BE440, "CreateStaffCostsPanel", "")
install(0x101A4DE4, "CreateLoansPanel", "")
install(0x101A001C, "CreateFinancePanel", "")
install(0x1013FC3C, "SetParkOpenOrClosed", "Lii")
install(0x1018493C, "DefaultPanelMsg", "Liii", "i")
install(0x1018BD3C, "UI_TITLE_COLOUR", "L")
install(0x1019DEAC, "ShowEditParkNamePanel", "")
install(0x101DE318, "CallDefaultWindowHandler", "Liii", "i")

_new_rec = CL.recorder("OperatorNew", "u", new_block)


def stub_new(e):
    _new_rec(e)
    restore_toc(e)


STUBS.internal(0x102384B8, stub_new)


def stub_minmax(e):
    e.cl.rec("SetMinMaxValues", (e.cl.lab(e.r(3)), rd32(e, e.r(4)), rd32(e, e.r(5))))


def stub_inc(e):
    e.cl.rec("SetInc", (e.cl.lab(e.r(3)), rd32(e, e.r(4))))


def stub_gametype(e):
    L = e.cl
    L.rec("CGameType", (L.lab(e.r(3)),))
    e.write(e.r(3) & MASK, struct.pack(">I", L.ret("gametype") & MASK))


def stub_glue(e):
    L = e.cl
    magic = e.r(12) & MASK
    if magic == MAGIC_SETVALUE:
        L.rec("WindowSetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif magic == MAGIC_ENABLE:
        L.rec("WindowEnable", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        L.rec("BadGlue", (magic,))
    restore_toc(e)


STUBS.internal(0x101D8E44, stub_minmax)
STUBS.internal(0x101D8EE8, stub_inc)
STUBS.internal(0x1016F5D8, stub_gametype)
STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD["ppc"] = T.persist_alloc(runner.emu, 0x1DB000)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, p):
        """A pointer cell: one word on the Mac, eight bytes on the host (the arena is not 32-bit addressable)."""
        self.write(a, struct.pack(self.pfmt, p))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_blocks(self):
        self.blk = {n: self.alloc(0x120) for n in BLK}
        self.world = self.world_block()
        self.labels = {self.blk[n]: i + 1 for i, n in enumerate(BLK)}
        self.labels[self.world] = L_WORLD
        self.labels[self.g("script")] = L_SCRIPT
        self.labels[self.g("msgfn")] = L_MSGFN
        self.labels[self.g("titlefn")] = L_TITLEFN
        self.labels[self.g("gtstore")] = L_GT

    def label_of(self, w):
        return self.labels.get(w, -1 if w else 0)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        e.blk = self.blk
        e.newn = 0
        self.L = e.cl = CL.PpcLog(dict(self.labels), case["queues"])
        self.saved = STUBS.activate(e)

    def world_block(self):
        return WORLD["ppc"]

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

    def set_thing(self, i, p):
        self.words(ADDR["thing"] + 20 * i, [p])

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [x & MASK for x in args])
        return 0 if fn == FN_CREATE else s32(r3)   # CreateEntryPricePanel answers nothing


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_blocks()
        for i, n in enumerate(BLK):
            lib.tpw_ep_blk(i, self.blk[n])
        CL.host_begin(lib, {lab: a for a, lab in self.labels.items()}, case["queues"])

    def world_block(self):
        if not hasattr(self.lib, "world_block"):
            self.lib.world_block = self.lib.tpw_ep_alloc(0x1DB000)
        return self.lib.world_block

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ep_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ep_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_thing(self, i, p):
        self.lib.tpw_ep_setthing(i, p)

    def call(self, fn, args):
        a = [s32(x) for x in args]
        if fn == FN_CREATE:
            self.lib.tpw_ep_create()
            return 0
        f = self.lib.tpw_ep_msg if fn == FN_MSG else self.lib.tpw_ep_title
        return s32(f(self.blk["self"], a[1], a[2], a[3]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ep_"
    lib.tpw_ep_addr.restype, lib.tpw_ep_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ep_alloc.restype, lib.tpw_ep_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ep_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ep_blk.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_ep_setthing.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_ep_create.argtypes = []
    for n in ("msg", "title"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ep_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    b = s.blk
    s.ptr(s.g("parksc"), b["panel"] if case["panel"] else 0)
    s.ptr(s.g("gamewin"), b["game"])
    s.ptr(s.g("world"), s.world)
    s.half(s.world + 0x1DA732, case["idx"])
    s.words(s.g("fee"), [case["gfee"]])
    s.words(s.g("closed"), [case["gclosed"]])
    s.words(s.g("gtstore"), [case["gt"]])
    s.write(s.g("gtguard"), bytes([case["guard"]]))
    s.set_thing(0, 0 if case["zero_null"] else b["obj"])
    s.set_thing(case["idx"], b["obj"]) if case["idx"] != 0 else None
    s.words(b["bank"] + 0x118, [case["bankfee"]])
    for n in WINDOWS:
        s.words(b[n] + 0x118, [b["vt"] if isinstance(s, Ppc) else 0])
    s.words(b["vt"] + 0x24, [MAGIC_SETVALUE])
    s.words(b["vt"] + 0x1C, [MAGIC_ENABLE])


def execute(s, case):
    return s.call(case["fn"], [s.blk["self"]] + case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "parksc": s.label_of(s.rd_ptr(s.g("parksc"))), "fee": s.rd_word(s.g("fee")), "closed": s.rd_word(s.g("closed")),
            "gt": s.rd_word(s.g("gtstore")), "guard": s.read(s.g("gtguard"), 1)[0]}


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


def base(r, fn, args):
    return {"fn": fn, "args": list(args), "idx": r.choice([0, 1, 3, 20, 100]), "panel": r.choice([0, 0, 0, 1]),
            "gfee": r.choice([0, 7, r.randrange(0, 1 << 32)]), "gclosed": r.choice([0, 1]), "gt": r.choice([0, 1, 2, 3]),
            "guard": r.choice([0, 1]), "zero_null": r.choice([0, 1]), "bankfee": r.choice([0, 5, 10000, r.randrange(0, 1 << 32)]),
            "queues": {"IsParkClosed": [r.choice([0, 1, 5])], "gametype": [r.choice([0, 1, 2, 3])],
                       "DefaultPanelMsg": [r.choice([0, 1, -1, 77])], "CallDefaultWindowHandler": [r.choice([0, 1, -1, 9])]}}


def create_case(r, **kw):
    c = base(r, FN_CREATE, [])
    c.update(kw)
    return c


def create_edges():
    r = random.Random(11)
    for panel in (0, 1):
        for gt in (0, 2, 3):
            for guard in (0, 1):
                for closed in (0, 1, 5):
                    for idx, zn in ((0, 1), (0, 0), (3, 0), (3, 1)):
                        yield create_case(r, panel=panel, gt=gt, guard=guard, queues=dict(base(r, 0, [])["queues"], IsParkClosed=[closed], gametype=[r.choice([0, 2])]),
                                          idx=idx, zero_null=zn)


def create_random(r):
    return create_case(r)


MSGS = [5, 0x14, 0x1E, 0x10, 0x800, 0x100, 0x7FF, 0x801, 0x101, 0, -1, 0x15, 0x1D, 0x11, 0xF, 0x13, 4, 6]
AVALS = [0x4F3AC, 0x4F3AB, 0x4F3AA, 0x4F3AD, 0x4F3AE, 0x4F3AF, 0x4F3B0, 0x4F3B1, 0x4F3B2, 0x4F3B3, -1, -2, -3, 0, 1, 2, -4,
         0x50000, 0x4F3A0]


def msg_case(r, m, a, b):
    return base(r, FN_MSG, [m, a, b])


def msg_edges():
    r = random.Random(22)
    for m in MSGS:
        if m != 0x100:
            for b in (0, 1, -5, 0x7FFFFFFF):
                yield msg_case(r, m, r.choice([0, 3]), b)
    for a in AVALS:
        for b in (0, 1, 2, -1):
            yield msg_case(r, 0x100, a, b)


def msg_random(r):
    m = r.choice(MSGS + [0x100, 0x100, 0x100])
    a = r.choice(AVALS) if r.random() < 0.8 else r.randrange(-1 << 31, 1 << 31)
    return msg_case(r, m, a, r.choice([0, 1, 2, -1, r.randrange(-1 << 31, 1 << 31)]))


TMSGS = [0x10000, 0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x10006, 0x10007, 0, -1, 0x1FFFF, 0x20000, 0x10006, 0xFFFF]


def title_edges():
    r = random.Random(33)
    for m in TMSGS:
        for a in (0, 1, -1):
            yield base(r, FN_TITLE, [m, a, r.randrange(-5, 5)])


def title_random(r):
    return base(r, FN_TITLE, [r.choice(TMSGS), r.choice([0, 0, 1, -1, r.randrange(-1 << 31, 1 << 31)]), r.randrange(-1 << 31, 1 << 31)])


TESTS = [
    FnTest("CreateEntryPricePanel", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("EntryPricePanelTitleMsg", ppc, host, edge=title_edges, rnd=title_random),
    FnTest("EntryPricePanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
]
