"""Unit v5767, six CAdvisorScoring members, against libtpw_unit_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.dylib
(tests/unit_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.cpp), main app ("SimTheme Park"),
src/AdvisorScoring_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.cpp:
  _ADVISOR_TUTORIAL_UI_PANEL 0x1001b540, _ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT 0x1001b30c, _ADVISOR_PRICING_DRINKS_POOR_VALUE 0x100195c4,
  Save 0x10011294, Load 0x100111f0, GetFirstResponseFor 0x10010464

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the park time,
the bank account, the analyser, the master save, CTRACE, UDebug, LbFile_Read and LbFile_Write, the 64-bit division, operator new) are
fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the scoring object with its balance values, the game world pointers, gMasterSave, gMainBalance's value, the
"no drinks" id, the message table (every record's number and first response). What is compared: the answer, the object's fields, the
master save pointer (none, the old one or a new one) and the event log.

No fused operations. Not tested: a message index outside -1 .. 0x15e in GetFirstResponseFor (the original reads outside the table),
NaN or huge products in the gold ticket scorer (fctiwz differs between the emulator and the hardware).
"""
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1

F_UI, F_GOLD, F_DRINKS, F_SAVE, F_LOAD, F_FIRST = 0x1001B540, 0x1001B30C, 0x100195C4, 0x10011294, 0x100111F0, 0x10010464
SIZES = {F_UI: 128, F_GOLD: 236, F_DRINKS: 192, F_SAVE: 148, F_LOAD: 116, F_FIRST: 172}
OWN = set(SIZES)

C_FUNNY, C_BANK, C_PROFIT, C_CTOR, C_HAVE = 0x10112B14, 0x1013EDFC, 0x100F439C, 0x101694DC, 0x1016A12C
C_ANALYSER, C_WORST, C_CTRACE, C_UDEBUG = 0x1013EBC8, 0x100EAC7C, 0x1006FCBC, 0x10000E40
C_READ, C_WRITE, C_DIV = 0x10238848, 0x10238878, 0x10237E1C

G_WORLDREF, G_MASTER, G_MBAL, G_NONE, G_TABLE = 0x102A460C, 0x10380A8C, 0x102B4568, 0x1027DF0C, 0x1027EFA4
HANDLE = 0x12345678
DIVISOR = 864000000000

LAY = []          # the host's layout, filled by bind


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def slot(off):
    return (off - 0x14) // 4


CUR = {}
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_funny(e):
    L = e.cl
    L.rec("FunnyTime", (L.lab(e.r(4) - 0x2A0),))
    e.write(e.r(3), struct.pack(">Q", L.ret("ticks") & M64))


def stub_bank(e):
    L = e.cl
    L.rec("GetBankAccount", (L.lab(e.r(3)),))
    e.set_r(3, CUR["bank"])


def stub_profit(e):
    L = e.cl
    L.rec("GetProfit", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("profit") & MASK)


def stub_analyser(e):
    L = e.cl
    L.rec("GetParkAnalyser", (L.lab(e.r(3)),))
    e.set_r(3, CUR["analyser"])


def stub_worst(e):
    L = e.cl
    L.rec("WorstValue", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">H", L.ret("worstid") & 0xFFFF))
    e.set_r(3, L.ret("worst") & MASK)


def stub_have(e):
    L = e.cl
    L.rec("HaveTicket", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 1 if L.ret("have") else 0)


def save_toc(e):
    """The glue stores the caller's TOC pointer at 0x14(r1) for the reload after the call; the stub stands in for the glue."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def stub_read(e):
    save_toc(e)
    L = e.cl
    L.rec("Read", (e.r(4), e.r(6), 1 if e.r(5) == CUR["obj"] else 0))
    e.write(e.r(5), struct.pack(">I", L.ret("readdata") & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("xfer") & MASK))


def stub_write(e):
    save_toc(e)
    L = e.cl
    b = e.read(e.r(5), 4)
    L.rec("Write", (e.r(4), e.r(6), b[0], b[1], b[2], b[3]))
    e.write(e.r(7), struct.pack(">I", L.ret("xfer") & MASK))


def stub_div(e):
    n = (e.r(3) << 32) | e.r(4)
    if n >= 1 << 63:
        n -= 1 << 64
    d = (e.r(5) << 32) | e.r(6)
    if d >= 1 << 63:
        d -= 1 << 64
    q = abs(n) // abs(d)
    if (n < 0) != (d < 0):
        q = -q
    q &= M64
    e.set_r(3, q >> 32)
    e.set_r(4, q & MASK)


STUBS.internal(C_FUNNY, stub_funny)
STUBS.internal(C_BANK, stub_bank)
STUBS.internal(C_PROFIT, stub_profit)
STUBS.internal(C_CTOR, CL.recorder("Ctor", "N"))
STUBS.internal(C_HAVE, stub_have)
STUBS.internal(C_ANALYSER, stub_analyser)
STUBS.internal(C_WORST, stub_worst)
STUBS.internal(C_CTRACE, CL.recorder("CTRACE", "usi"))
STUBS.internal(C_UDEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(C_READ, stub_read)
STUBS.internal(C_WRITE, stub_write)
STUBS.internal(C_DIV, stub_div)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world, self.bank, self.analyser, self.master = e.alloc(0x400), e.alloc(0x40), e.alloc(0x40), e.alloc(0x40)
        self.obj = e.alloc(0x700)
        self.cell = e.alloc(16)
        self.wcell = e.alloc(16)
        CUR.update(bank=self.bank, analyser=self.analyser, obj=self.obj)
        self.L = e.cl = CL.PpcLog({self.world: 1, self.bank: 2, self.analyser: 3, self.master: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self):
        self.words(self.wcell, [self.world])
        self.words(G_WORLDREF, [self.wcell])

    def set_master(self, which):
        self.words(G_MASTER, [{0: 0, 1: self.master}[which]])

    def master_state(self):
        v = self.rd_words(G_MASTER, 1)[0]
        return 0 if v == 0 else (4 if v == self.master else 9)

    def set_none(self, v):
        self.half(G_NONE, v)

    def bal(self, off, bits):
        self.words(self.obj + off, [bits])

    def mbal(self, bits):
        self.words(G_MBAL + 0x760, [bits])

    def put_handle(self):
        self.words(self.cell, [HANDLE])
        self.words(self.cell + 8, [self.cell])      # unused
        return self.cell

    def fields(self):
        return (self.read(self.obj, 4), self.rd_half(self.obj + 4), self.rd_words(self.obj + 8, 1)[0])

    def set_fields(self, raw, w4, w8):
        self.write(self.obj, raw)
        self.half(self.obj + 4, w4)
        self.words(self.obj + 8, [w8])

    def rec(self, i, msg, resp):
        a = G_TABLE + 0x30 * i
        self.words(a + 4, [msg])
        self.words(a + 0x20, [resp])

    def call(self, op, case):
        e = self.e
        args = {"ui": [], "gold": [], "drinks": [], "load": [self.cell], "save": [self.cell]}.get(op)
        if args is None:
            args = [case["msg"] & MASK]
        r3, _, _ = T.call_ppc(e, {"ui": F_UI, "gold": F_GOLD, "drinks": F_DRINKS, "load": F_LOAD, "save": F_SAVE, "first": F_FIRST}[op],
                              [self.obj] + args)
        return r3 & MASK if op in ("ui", "gold", "drinks", "first") else r3 & 0xFF


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if not hasattr(lib, "world"):
            lib.world = lib.tpw_v5767_alloc(LAY[6])
        self.world = lib.world
        self.bank, self.analyser, self.master = (lib.tpw_v5767_addr(n) for n in (b"bank", b"analyser", b"master"))
        self.obj = lib.tpw_v5767_alloc(LAY[4])
        self.cell = lib.tpw_v5767_alloc(16)
        CL.host_begin(lib, {1: self.world, 2: self.bank, 3: self.analyser, 4: self.master}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        import ctypes as C
        return C.string_at(a, n)

    def write(self, a, data):
        import ctypes as C
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self):
        self.lib.tpw_v5767_set_world(self.world)

    def set_master(self, which):
        self.lib.tpw_v5767_set_master({0: 0, 1: self.master}[which])

    def master_state(self):
        v = self.lib.tpw_v5767_get_master()
        return 0 if v == 0 else (4 if v == self.master else 9)

    def set_none(self, v):
        self.write(self.lib.tpw_v5767_addr(b"none"), struct.pack("<H", v & 0xFFFF))

    def bal(self, off, bits):
        self.words(self.obj + LAY[2] + LAY[3] * slot(off), [bits])

    def mbal(self, bits):
        self.words(self.lib.tpw_v5767_addr(b"mbal") + LAY[10] + LAY[3] * ((0x760 - 8) // 4), [bits])

    def put_handle(self):
        self.write(self.cell, struct.pack("<Q", HANDLE))
        return self.cell

    def fields(self):
        return (self.read(self.obj, 4), self.rd_half(self.obj + LAY[0]), self.rd_words(self.obj + LAY[1], 1)[0])

    def set_fields(self, raw, w4, w8):
        self.write(self.obj, raw)
        self.half(self.obj + LAY[0], w4)
        self.words(self.obj + LAY[1], [w8])

    def rec(self, i, msg, resp):
        a = self.lib.tpw_v5767_addr(b"table") + LAY[7] * (i + 1)
        self.words(a + LAY[8], [msg])
        self.words(a + LAY[9], [resp])

    def call(self, op, case):
        L = self.lib
        if op == "first":
            return L.tpw_v5767_first(self.obj, s32(case["msg"])) & MASK
        fn = {"ui": L.tpw_v5767_tutorial, "gold": L.tpw_v5767_gold, "drinks": L.tpw_v5767_drinks}.get(op)
        if fn:
            return fn(self.obj) & MASK
        return (L.tpw_v5767_load if op == "load" else L.tpw_v5767_save)(self.obj, self.cell) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    import ctypes as C
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    sig = {"addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "set_world": (None, [u64]), "set_master": (None, [u64]),
           "get_master": (u64, []), "tutorial": (u32, [u64]), "gold": (u32, [u64]), "drinks": (u32, [u64]),
           "load": (u32, [u64, u64]), "save": (u32, [u64, u64]), "first": (u32, [u64, i32])}
    for n, (r, a) in sig.items():
        f = getattr(lib, "tpw_v5767_" + n)
        f.restype, f.argtypes = r, a
    lib.tpw_v5767_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v5767_layout(out)
    LAY[:] = list(out)[:n]
    assert LAY[0] == 4 and LAY[1] == 8 and LAY[5] == 0x2A0, LAY
    lib.bound = True


def build(s, case):
    s.set_world()
    s.set_master(case.get("master", 0))
    s.set_none(case.get("none", 0))
    s.mbal(case.get("mb", 0))
    for off, bits in case.get("bal", {}).items():
        s.bal(off, bits)
    if "fields" in case:
        s.set_fields(*case["fields"])
    if "table" in case:
        for i, (msg, resp) in enumerate(case["table"]):       # record -1 .. 0x15e
            s.rec(i - 1, msg, resp)
    s.put_handle()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "fields": s.fields(), "master": s.master_state()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def rbits(r):
    return r.choice([0, 1, MASK, 0x7FFFFFFF, 0x80000000, r.randrange(1 << 32), r.randrange(-50, 50) & MASK, r.randrange(1000)])


def rfields(r):
    return (bytes(r.randrange(256) for _ in range(4)), r.randrange(1 << 16), r.randrange(1 << 32))


def base(r, op):
    return {"op": op, "queues": {}, "fields": rfields(r), "bal": {}}


# ---- _ADVISOR_TUTORIAL_UI_PANEL
def ui_case(r, day=None, lo=None, hi=None, neg=False):
    c = base(r, "ui")
    lo = r.randrange(-3, 10) if lo is None else lo
    hi = lo + r.randrange(-1, 12) if hi is None else hi
    day = r.randrange(lo - 3, hi + 3) if day is None else day
    ticks = day * DIVISOR + r.choice([0, 1, DIVISOR - 1, r.randrange(DIVISOR)])
    if neg:
        ticks = -abs(ticks)
    c["queues"]["ticks"] = [ticks]
    c["bal"] = {0x5DC: lo & MASK, 0x5E0: hi & MASK, 0x5E4: rbits(r)}
    return c


def ui_edges():
    r = random.Random(5767)
    for lo, hi, day in [(2, 6, 2), (2, 6, 3), (2, 6, 5), (2, 6, 6), (2, 6, 1), (2, 6, 7), (5, 5, 5), (6, 2, 4), (-3, 3, -1), (-3, 3, 0),
                        (-1, 2, 0), (0, 1, 0), (-1, 1, 0)]:
        yield ui_case(r, day, lo, hi)
    for lo, hi in [(-1, 2), (-5, 1), (0, 3)]:
        yield ui_case(r, 0, lo, hi, neg=True)
    c = ui_case(r, 1, 0, 5)
    c["queues"]["ticks"] = [-1]
    yield c
    c["queues"]["ticks"] = [(1 << 63) - 1]
    yield c


def ui_random(r):
    return ui_case(r, neg=r.random() < 0.2)


# ---- _ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT
def gold_case(r, mb=None, mult=None, off=None, master=None, have=None):
    c = base(r, "gold")
    mb = r.choice([0, 1, -1, 1000, 100000, -100000, r.randrange(-100000, 100000)]) if mb is None else mb
    mult = fb(r.choice([0.0, 1.0, 1.5, -1.0, 0.1, 2.0, 33.3, r.uniform(-50, 50)])) if mult is None else mult
    thr = int(T.f32(T.f32(mb) * struct.unpack(">f", struct.pack(">I", mult))[0]))
    off = r.choice([-1, 0, 1, r.randrange(-5000, 5000), 1 << 30, -(1 << 30)]) if off is None else off
    c["mb"] = mb & MASK
    c["bal"] = {0x5D4: mult, 0x5D8: rbits(r)}
    c["master"] = r.choice([0, 1]) if master is None else master
    c["queues"] = {"profit": [(thr + off)], "have": [r.choice([0, 1]) if have is None else have]}
    return c


def gold_edges():
    r = random.Random(5768)
    for off in (-1, 0, 1):
        for master in (0, 1):
            for have in (0, 1):
                yield gold_case(r, 1000, fb(1.5), off, master, have)
    yield gold_case(r, -7, fb(2.0), 0, 0, 0)
    yield gold_case(r, -7, fb(2.0), -13, 1, 0)
    yield gold_case(r, 100, fb(-0.5), -51, 0, 0)
    yield gold_case(r, 100, fb(-0.5), -50, 0, 0)
    yield gold_case(r, 100, fb(0.0), 0, 0, 1)


def gold_random(r):
    return gold_case(r)


# ---- _ADVISOR_PRICING_DRINKS_POOR_VALUE
def drinks_case(r, same=None, fig=None, lim=None):
    c = base(r, "drinks")
    none = r.choice([0, 1, 7, 0xFFFF, r.randrange(1 << 16)])
    same = r.random() < 0.2 if same is None else same
    wid = none if same else (none + r.choice([1, -1, 0x100, r.randrange(1, 1 << 16)])) & 0xFFFF
    lim = r.choice([0, 5, -5, 100, r.randrange(-1000, 1000)]) if lim is None else lim
    fig = lim + r.choice([-1, 0, 1, r.randrange(-300, 300), -(1 << 31) - lim, (1 << 31) - 1 - lim]) if fig is None else fig
    fig = s32(fig)
    c["none"] = none
    c["bal"] = {0x4F0: lim & MASK, 0x4F4: rbits(r)}
    c["queues"] = {"worstid": [wid], "worst": [fig & MASK]}
    return c


def drinks_edges():
    r = random.Random(5769)
    for same in (True, False):
        for fig in (-1, 0, 1):
            yield drinks_case(r, same, 10 + fig, 10)
    yield drinks_case(r, False, -(1 << 31), 5)
    yield drinks_case(r, False, (1 << 31) - 1, 5)
    yield drinks_case(r, False, -(1 << 31), -(1 << 31) + 1)


def drinks_random(r):
    return drinks_case(r)


# ---- Load and Save
def io_case(r, op, xfer=None):
    c = base(r, op)
    c["queues"] = {"xfer": [r.choice([0, 1, 3, 4, 5, 7, 8, 12, 16, MASK, r.randrange(1 << 32)]) if xfer is None else xfer],
                   "readdata": [r.randrange(1 << 32)]}
    return c


def load_edges():
    r = random.Random(5770)
    for x in (0, 3, 4, 5, 6, 7, 8, 11, 12, MASK, 1 << 31):
        yield io_case(r, "load", x)


def save_edges():
    r = random.Random(5771)
    for x in (0, 3, 4, 5, 6, 7, 8, 11, 12, MASK, 1 << 31):
        yield io_case(r, "save", x)


# ---- GetFirstResponseFor
def first_case(r, msg=None, mode=None):
    c = base(r, "first")
    msg = r.choice([-1, 0, 1, 2, 0x15D, 0x15E, 0x15F, 0x160, r.randrange(-1, 0x15F), r.randrange(-1, 0x15F)]) if msg is None else msg
    mode = r.choice(["match", "elsewhere", "none"]) if mode is None else mode
    table = [[i, r.randrange(1 << 32)] for i in range(-1, 0x15F)]          # record -1 .. 0x15e: its own number, a first response
    if -1 <= msg < 0x15F:
        k = msg + 1
        if mode == "elsewhere":
            table[k][0] = r.choice([msg + 1, msg - 1000, 0, 5])
            if table[k][0] == msg:
                table[k][0] = msg + 7
            j = r.choice([j for j in range(0, 0x15F) if j + 0 != k])
            table[j][0] = msg
        elif mode == "none":
            table[k][0] = msg + 1
        # "match": the default
    c["msg"] = msg & MASK
    c["table"] = [tuple(t) for t in table]
    return c


def first_edges():
    r = random.Random(5772)
    for msg in (-1, 0, 1, 0x15E, 0x15F, 0x160):
        for mode in ("match", "elsewhere", "none"):
            yield first_case(r, msg, mode)


TESTS = [
    FnTest("CAdvisorScoring::_ADVISOR_TUTORIAL_UI_PANEL", ppc, host, edge=ui_edges, rnd=ui_random),
    FnTest("CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT", ppc, host, edge=gold_edges, rnd=gold_random),
    FnTest("CAdvisorScoring::_ADVISOR_PRICING_DRINKS_POOR_VALUE", ppc, host, edge=drinks_edges, rnd=drinks_random),
    FnTest("CAdvisorScoring::Save", ppc, host, edge=save_edges, rnd=lambda r: io_case(r, "save")),
    FnTest("CAdvisorScoring::Load", ppc, host, edge=load_edges, rnd=lambda r: io_case(r, "load")),
    FnTest("CAdvisorScoring::GetFirstResponseFor", ppc, host, edge=first_edges, rnd=lambda r: first_case(r)),
]
