"""Unit v5952, the all-items lists' rows and the kids list, against libtpw_unit_FillAllRidesListCallback_Upgrades.dylib
(tests/unit_FillAllRidesListCallback_Upgrades.cpp), main app ("SimTheme Park"),
src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allKidsPanel.cpp:
  FillAllRidesListCallback_Upgrades 0x10195ba8, FillAllShopsListCallback 0x10195cb0, FillAllFeaturesListCallback 0x10195f24,
  AllKidsPanel_KidGone 0x10195708, LCPostFormat 0x10194fa8 (the kids list's attach function)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Sixteen blocks stand for the things, windows,
draw objects and so on: the fakes log a pointer as its block's number. Every callee outside the unit is a fake on both sides that records
its arguments (the list's cells are logged word by word) and answers from the case's queues.

The world of a case: the list, the world and window globals, the thing array's first entries, the object's type halfword, the font
index and the queues of answers. What is compared: the answer, the kid counter and the event log.

No fused operations. Not tested: an upgrade parent index past the thing array (the Mac reads past the table; indices 0 to 7 are used).
"""
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FillAllRidesListCallback_Upgrades"
MASK = 0xFFFFFFFF

FN_RIDES, FN_SHOPS, FN_FEAT, FN_KID, FN_LCP = 0x10195BA8, 0x10195CB0, 0x10195F24, 0x10195708, 0x10194FA8
SIZES = {FN_RIDES: 200, FN_SHOPS: 224, FN_FEAT: 172, FN_KID: 124, FN_LCP: 736}
OWN = set(SIZES)

ADDR = dict(list=0x103947D8, counter=0x103947C0, msgwin=0x103947C8, world=0x1037EBFC, font=0x102A8878, noparent=0x102A8D08,
            thingarr=0x1034CBFC)
HOST_LAYOUT = [0x0E, 0x1A4]

BLK_N = 16
WIN0, DRAW, FONT, TEXT, WORLD, ANALYSER, LIST, OBJ, PO1, PO2, TBL = 0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
CUR = {"addrs": [], "ncells": 0}


def s32(v):
    return T.s32(v)


def lab_of(addr):
    if addr == 0:
        return 0
    try:
        return CUR["addrs"].index(addr) + 1
    except ValueError:
        return -1


def where(addr):
    if addr == 0:
        return 0
    for i, b in enumerate(CUR["addrs"]):
        if b <= addr < b + 0x800:
            return (i + 1) * 0x1000 + addr - b
    return -1


def blk(i):
    return CUR["addrs"][i]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getupgradeparent(e):
    L = e.cl
    L.rec("GetUpgradeParent", (lab_of(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetUpgradeParent") & 0xFFFF))


def stub_getdbinfo(e):
    e.cl.rec("GetDBInfo", (lab_of(e.r(3)),))
    e.set_r(3, 0)


def stub_getobjectname(e):
    e.cl.rec("UIMain_GetObjectName", (lab_of(e.r(3)),))
    e.set_r(3, blk(TEXT))


def stub_sum(e):
    L = e.cl
    L.rec("GetSumOfLast", (where(e.r(3)), s32(e.r(5))))
    hi, lo = L.ret("sumhi") & MASK, L.ret("sumlo") & MASK
    e.write(e.r(4), struct.pack(">II", hi, lo))
    e.set_r(3, 0)


def int_getter(name):
    def stub(e):
        e.cl.rec(name, (where(e.r(3)) if False else lab_of(e.r(3)),))
        e.set_r(3, e.cl.ret(name) & MASK)
    return stub


def stub_additem(e):
    L = e.cl
    n = CUR["ncells"]
    ws = [e.u32(e.r(4) + 4 * i) for i in range(n)]
    L.rec("AddItem", (lab_of(e.r(3)), lab_of(ws[0])) + tuple(s32(w) for w in ws[1:]) + (lab_of(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), e.cstr(e.r(4))))


def stub_getparkanalyser(e):
    e.cl.rec("GetParkAnalyser", (lab_of(e.r(3)),))
    e.set_r(3, blk(ANALYSER))


def stub_numberofobject(e):
    e.cl.rec("NumberOfObjectInPark", (lab_of(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.cl.ret("NumberOfObjectInPark") & MASK)


def stub_getwindow(e):
    e.cl.rec("GetWindow", (lab_of(e.r(3)), s32(e.r(4))))
    e.set_r(3, blk(LIST))


def stub_finddata(e):
    e.cl.rec("FindData", (lab_of(e.r(3)), lab_of(e.r(4))))
    e.set_r(3, e.cl.ret("FindData") & MASK)


def stub_removeitem(e):
    e.cl.rec("RemoveItem", (lab_of(e.r(3)), s32(e.r(4))))


def stub_additem_rides(e):
    e.cl.rec("AddItemToAllRidesList", (lab_of(e.r(3)), lab_of(e.r(4)), lab_of(e.r(5))))


def stub_setclientdrawobject(e):
    e.cl.rec("SetClientDrawObject", (lab_of(e.r(3)), e.cl.lab(e.r(4))))


def stub_setfont(e):
    e.cl.rec("SetFont", (lab_of(e.r(3)), lab_of(e.r(4))))


def stub_setfontjust(e):
    e.cl.rec("SetFontJust", (lab_of(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def stub_getchild(e):
    e.cl.rec("GetChild", (lab_of(e.r(3)), s32(e.r(4))))
    e.set_r(3, blk(WIN0 + 1))


def stub_getnext(e):
    e.cl.rec("GetNext", (lab_of(e.r(3)),))
    i = lab_of(e.r(3)) - 1
    e.set_r(3, blk(i + 1 if 0 <= i < 6 else 0))


def stub_getfont(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, blk(FONT))


def stub_indent(e):
    e.cl.rec("UIMain_IndentClient", (lab_of(e.r(3)), T.s16(e.r(4)), T.s16(e.r(5))))


def stub_finddraw(e):
    e.cl.rec("FindDrawObject", (0, e.cstr(e.r(3))))
    e.set_r(3, blk(DRAW))


def stub_new(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("OperatorNew", (size,))
    e.set_r(3, e.alloc(size) if L.ret("OperatorNew") else 0)


def ctor(name, text=False):
    def stub(e):
        L = e.cl
        mem = e.r(3)
        label = L.newlab(mem) if hasattr(L, "newlab") else None
        if text:
            L.rec(name, (label, e.cstr(e.r(4))))
        else:
            L.rec(name, (label,))
    return stub


def stub_setvalue(e):
    e.cl.rec("WindowSetValue", (lab_of(e.r(3)), s32(e.r(4))))


def stub_rectctor(e):
    e.cl.rec("CoolRectCtor", (0,))


def stub_rectdtor(e):
    e.cl.rec("CoolRectDtor", (0,))


for addr, fn in [
    (0x1010DA64, stub_getupgradeparent), (0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "is")), (0x10195978, stub_additem_rides), (0x10107CD4, stub_getdbinfo),
    (0x101810FC, stub_getobjectname), (0x100F1000, stub_sum), (0x1010F8D4, int_getter("GetProfitability")),
    (0x1010F894, int_getter("GetTotalRevenue")), (0x1010F9D0, int_getter("GetCustomerSatisfaction")), (0x101D3000, stub_additem),
    (0x1013EBC8, stub_getparkanalyser), (0x100EA52C, stub_numberofobject), (0x101DBE40, stub_getwindow), (0x101D4384, stub_finddata),
    (0x101D39F0, stub_removeitem), (0x101C7448, stub_rectctor), (0x101C74E0, stub_rectdtor), (0x102384B8, stub_new),
    (0x1018EB38, CL.recorder("VisitorNumCtor", "N")), (0x1018DE60, CL.recorder("CashCtor", "N")),
    (0x1018EDF4, CL.recorder("TimeCtor", "N")), (0x1018D930, CL.recorder("NumberCtor", "N")),
    (0x101898C8, CL.recorder("GradientCtor", "Ns")), (0x101DB624, stub_setclientdrawobject), (0x10180270, stub_getfont),
    (0x101DA118, stub_setfont), (0x101DA204, stub_setfontjust), (0x101DBAC4, stub_getchild), (0x101DBB38, stub_getnext),
    (0x10180940, stub_indent), (0x101CD2D4, CL.recorder("FindDrawObject", "s", ret=lambda e, log: blk(DRAW))), (0x1023BAA8, stub_setvalue),
]:
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        """A pointer global: one word on the Mac, a full pointer on the host."""
        self.write(a, struct.pack(">I", v) if self.fmt == ">I" else struct.pack("<Q", v))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.addrs = [e.alloc(0x800) for _ in range(BLK_N)]
        CUR["addrs"] = self.addrs
        self.L = e.cl = CL.PpcLog({a: i + 1 for i, a in enumerate(self.addrs)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def entry(self, index, ptr):
        self.words(ADDR["thingarr"] + 0x14 * index, [ptr])

    def log(self):
        return self.L.log

    def call(self, op, arg):
        CUR["ncells"] = {"shops": 5, "feat": 2}.get(op, 0)
        r3, _, _ = T.call_ppc(self.e, {"rides": FN_RIDES, "shops": FN_SHOPS, "feat": FN_FEAT, "kid": FN_KID, "lcp": FN_LCP}[op], [arg])
        return r3 & MASK if op in ("rides", "shops", "feat") else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        bind(lib)
        self.addrs = [lib.tpw_fav_alloc(0x800) for _ in range(BLK_N)]
        for i, a in enumerate(self.addrs):
            lib.tpw_fav_setblock(i, a)
        CL.host_begin(lib, {i + 1: a for i, a in enumerate(self.addrs)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fav_addr(name.encode())

    def read(self, a, n):
        return self.C.string_at(a, n)

    def write(self, a, data):
        self.C.memmove(a, data, len(data))

    def entry(self, index, ptr):
        self.lib.tpw_fav_setentry(index, ptr)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, arg):
        fn = {"rides": self.lib.tpw_fav_rides, "shops": self.lib.tpw_fav_shops, "feat": self.lib.tpw_fav_features,
              "kid": self.lib.tpw_fav_kidgone, "lcp": self.lib.tpw_fav_lcpost}[op]
        return fn(arg) & MASK if op in ("rides", "shops", "feat") else (fn(arg) and 0)


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_fav_addr.restype, lib.tpw_fav_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fav_alloc.restype, lib.tpw_fav_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fav_setblock.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fav_setentry.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fav_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("rides", "shops", "features", "kidgone", "lcpost"):
        f = getattr(lib, "tpw_fav_" + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fav_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    a = s.addrs
    s.ptr(s.g("list"), a[LIST])
    s.words(s.g("counter"), [case["counter"]])
    s.ptr(s.g("msgwin"), a[WIN0] if case["win"] else 0)
    s.ptr(s.g("world"), a[WORLD])
    s.words(s.g("font"), [case["font"]])
    s.half(s.g("noparent"), case["noparent"])
    for i, v in enumerate(case["entries"]):
        s.entry(i, [0, a[OBJ], a[PO1], a[PO2]][v])
    s.half(a[OBJ] + 0x0E, case["objtype"])
    for i in range(6):
        s.words(a[i] + 0x118, [a[TBL]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "counter": s.rd_words(s.g("counter"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], s.addrs[WIN0] if case["op"] == "lcp" else s.addrs[OBJ]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

DOUBLES = [0.0, -0.0, -1.0, 0.5, 1.0, 1.5, 2147483647.5, 2147483648.0, 4294967295.0, 4294967295.5, 4294967296.0, 1e10, 1e20,
           float("inf"), float("-inf"), float("nan"), 123456.789]
INTS = [0, 1, -1, 99, 100, 101, 0x7FFFFFFF, 0x80000000, 0x400000, 0x3FFFFF, 0xFFFFFFFF, 0x1FFFFF, 12345]


def dbits(x):
    return struct.unpack(">Q", struct.pack(">d", x))[0]


def rint(r):
    return r.choice(INTS) if r.random() < 0.5 else r.randrange(1 << 32)


def make(r, op, **o):
    c = {"op": op, "counter": r.choice([0, 1, 5, 0x7FFFFFFF, 0x80000000, r.randrange(1 << 32)]), "win": r.choice([0, 1, 1]),
         "font": r.choice([7, 0, r.randrange(1 << 32)]), "noparent": r.choice([0, 0, 1, 0xFFFF, r.randrange(65536)]),
         "entries": [r.randrange(4) for _ in range(8)], "objtype": r.choice([0, 1, 0xFFFF, r.randrange(65536)])}
    p1 = r.choice([c["noparent"], c["noparent"], r.randrange(65536), (c["noparent"] + 1) & 0xFFFF])
    d = r.choice(DOUBLES) if r.random() < 0.6 else r.uniform(-10, 5e9)
    bits = dbits(d)
    c["queues"] = {
        "GetUpgradeParent": [p1, r.randrange(8)], "sumhi": [bits >> 32], "sumlo": [bits & MASK],
        "GetProfitability": [rint(r)], "GetTotalRevenue": [rint(r)], "GetCustomerSatisfaction": [rint(r)],
        "NumberOfObjectInPark": [rint(r)], "FindData": [r.choice([-1, 0, 3, -100, 0x7FFFFFFF, rint(r)])],
        "OperatorNew": [1 if (r.random() < 0.8) else 0 for _ in range(5)],
    }
    c.update(o)
    if "q" in o:
        c["queues"].update(o["q"])
    return c


def rides_edges():
    r = random.Random(11)
    yield make(r, "rides", noparent=5, q={"GetUpgradeParent": [5, 1]})
    yield make(r, "rides", noparent=5, q={"GetUpgradeParent": [4, 2]}, entries=[0, 1, 2, 3, 0, 0, 0, 0])
    yield make(r, "rides", noparent=0, q={"GetUpgradeParent": [1, 0]}, entries=[1, 1, 2, 3, 0, 0, 0, 0])
    yield make(r, "rides", noparent=0, q={"GetUpgradeParent": [1, 0]}, entries=[0, 1, 2, 3, 0, 0, 0, 0])
    yield make(r, "rides", noparent=0xFFFF, q={"GetUpgradeParent": [0xFFFF, 0]})
    yield make(r, "rides", noparent=0xFFFF, q={"GetUpgradeParent": [0xFFFE, 7]}, entries=[0, 0, 0, 0, 0, 0, 0, 2])
    yield make(r, "rides", noparent=0, q={"GetUpgradeParent": [0, 3]})


def shops_edges():
    r = random.Random(12)
    for d in DOUBLES:
        b = dbits(d)
        yield make(r, "shops", q={"sumhi": [b >> 32], "sumlo": [b & MASK]})
    for v in INTS:
        yield make(r, "shops", q={"GetCustomerSatisfaction": [v]})
        yield make(r, "shops", q={"GetProfitability": [v], "GetTotalRevenue": [v ^ 1]})


def feat_edges():
    r = random.Random(13)
    for t in (0, 1, 0xFFFF, 0x8000, 0x1234):
        for n in (0, 1, 0x7FFFFFFF, 0xFFFFFFFF):
            yield make(r, "feat", objtype=t, q={"NumberOfObjectInPark": [n]})


def kid_edges():
    r = random.Random(14)
    for win in (0, 1):
        for pos in (-1, 0, 1, -100, 0x7FFFFFFF, 0x80000000):
            for counter in (0, 1, 0x80000000):
                yield make(r, "kid", win=win, counter=counter, q={"FindData": [pos]})


def lcp_edges():
    r = random.Random(15)
    yield make(r, "lcp", q={"OperatorNew": [1, 1, 1, 1, 1]})
    for i in range(5):
        flags = [1] * 5
        flags[i] = 0
        yield make(r, "lcp", q={"OperatorNew": flags})
    yield make(r, "lcp", q={"OperatorNew": [0] * 5})
    yield make(r, "lcp", font=0, q={"OperatorNew": [1] * 5})
    yield make(r, "lcp", font=0xFFFFFFFF, q={"OperatorNew": [1] * 5})


TESTS = [
    FnTest("FillAllRidesListCallback_Upgrades", ppc, host, edge=rides_edges, rnd=lambda r: make(r, "rides")),
    FnTest("FillAllShopsListCallback", ppc, host, edge=shops_edges, rnd=lambda r: make(r, "shops")),
    FnTest("FillAllFeaturesListCallback", ppc, host, edge=feat_edges, rnd=lambda r: make(r, "feat")),
    FnTest("AllKidsPanel_KidGone", ppc, host, edge=kid_edges, rnd=lambda r: make(r, "kid")),
    FnTest("LCPostFormat", ppc, host, edge=lcp_edges, rnd=lambda r: make(r, "lcp")),
]
