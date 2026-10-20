"""Unit v5734, the shop status panel's ApplyChanges, applytoallcallback and Show, against libtpw_unit_ShopStatusPanel_ApplyChanges.dylib
(tests/unit_ShopStatusPanel_ApplyChanges.cpp), main app ("SimTheme Park"), src/ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges.cpp:
  ShopStatusPanel::ApplyChanges 0x101bd9bc, applytoallcallback 0x101bdab8, Show 0x101bdee0

A case is a world and a call. The world is built twice by one piece of Python code. The callees outside the unit (the three CObject setters,
UIStatusPanel::OpenWindow, UIMain_AdvisorMessage) are fakes on both sides that record their arguments; objects and panels are told apart by a
tag word at +0x100.

The world of a case: the panel (quality, ingredient, price), the shop object of gStaffRoomPanelThing and the other object (object types),
gShopStatusObject. What is compared: the event log, gShopStatusObject (as: equals the thing given), the panel's fields.

No fused operations. Not tested: a NULL gStaffRoomPanelThing (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShopStatusPanel_ApplyChanges"
MASK = 0xFFFFFFFF

FN_APPLY, FN_ALL, FN_SHOW = 0x101bd9bc, 0x101bdab8, 0x101bdee0
SIZES = {FN_APPLY: 108, FN_ALL: 144, FN_SHOW: 80}
OWN = set(SIZES)

ADDR = dict(thing=0x103947a4, shop=0x103959dc, desc=0x102ad644)
MAC_OFFS = dict(q=0x24, i=0x28, p=0x2c, type=0x0e)
TAG = 0x100

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, a):
    return e.u32(a + TAG) & MASK


def st_amount(e):
    e.cl.rec("SetAmount", (tag(e, e.r(3)), T.s32(e.r(4))))


def st_quality(e):
    e.cl.rec("SetQuality", (tag(e, e.r(3)), e.r(4) & 0xFF))


def st_price(e):
    e.cl.rec("SetPrice", (tag(e, e.r(3)), T.s32(e.r(4))))


def st_open(e):
    e.cl.rec("OpenWindow", (tag(e, e.r(3)), int(e.r(4) == 0x102ad644), int(e.r(5) == 0x10267e18), T.s32(e.r(6)), T.s32(e.r(7)), T.s32(e.r(8))))


def st_adv(e):
    e.cl.rec("Advisor", (T.s32(e.r(3)),))


STUBS.internal(0x1010fc38, st_amount)
STUBS.internal(0x1010fb78, st_quality)
STUBS.internal(0x101105f4, st_price)
STUBS.internal(0x101935a0, st_open)
STUBS.internal(0x101821cc, st_adv)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, p):
        self.words(a, [p & MASK] + [p >> 32] * (self.psize // 4 - 1))

    def rd_ptr_low(self, a):
        return struct.unpack(self.fmt[0] + "I", self.read(a, 4))[0]

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    psize = 4
    offs = MAC_OFFS

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.offs = dict(q=lib.offs[0], i=lib.offs[1], p=lib.offs[2], type=lib.offs[3])
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ssp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ssp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        {FN_APPLY: self.lib.tpw_ssp_apply, FN_ALL: self.lib.tpw_ssp_all, FN_SHOW: self.lib.tpw_ssp_show}[fn](*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ssp_addr.restype, lib.tpw_ssp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ssp_alloc.restype, lib.tpw_ssp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ssp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ssp_apply.argtypes = [C.c_uint64]
    lib.tpw_ssp_all.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_ssp_show.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ssp_layout(out)
    lib.offs = list(out)[:n]
    assert lib.offs[3] == 0x0e, lib.offs
    lib.bound = True


def build(s, case):
    s.panel = s.alloc(0x200)
    s.words(s.panel + TAG, [0x7001])
    s.words(s.panel + s.offs["q"], [case["q"]])
    s.words(s.panel + s.offs["i"], [case["i"]])
    s.words(s.panel + s.offs["p"], [case["p"]])
    s.shop = s.alloc(0x200)
    s.words(s.shop + TAG, [0x7002])
    s.half(s.shop + s.offs["type"], case["shoptype"])
    s.other = s.alloc(0x200)
    s.words(s.other + TAG, [0x7003])
    s.half(s.other + s.offs["type"], case["othertype"])
    s.ptr(s.g("thing"), s.shop)
    s.ptr(s.g("shop"), s.panel)
    s.words(s.g("desc"), [0x00000001])  # the host's description is its own int16_t[2]; both stay unread


def execute(s, case):
    if case["op"] == "apply":
        s.call(FN_APPLY, [s.panel])
    elif case["op"] == "all":
        s.call(FN_ALL, [s.panel, s.other])
    else:
        s.call(FN_SHOW, [s.panel, s.other if case["thingnull"] == 0 else 0])


def observe(s, case):
    return {"log": s.log(),
            "shop": s.rd_ptr_low(s.g("shop")) == s.other & MASK,
            "shopzero": s.rd_ptr_low(s.g("shop")) == 0,
            "fields": [s.rd_words(s.panel + s.offs[k], 1)[0] for k in ("q", "i", "p")]}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def num(r):
    return r.choice([0, 1, -1, 2, 5, 0x7FFFFFFF, 0x80000000, 0x05, 3, 4, 100, 0x7FFFFFF, r.getrandbits(32), r.randrange(0, 10), r.randrange(0, 6)]) & MASK


def case_of(r, op, same=None, nul=None):
    shoptype = r.choice([0, 1, 0xFFFF, 0x8000, r.getrandbits(16)])
    if same is None:
        same = r.random() < 0.5
    other = shoptype if same else (shoptype ^ r.choice([1, 0x8000, r.getrandbits(16) | 1])) & 0xFFFF
    return {"op": op, "queues": {}, "q": num(r), "i": num(r), "p": num(r), "shoptype": shoptype, "othertype": other,
            "thingnull": r.choice([0, 1]) if nul is None else nul}


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        for same in (True, False):
            for nul in (0, 1):
                for q in (0, 1, 5, 6, 0x05555556, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFF):
                    c = case_of(r, op, same, nul)
                    c["q"] = q
                    yield c
    return gen


def rnd(op):
    return lambda r: case_of(r, op)


TESTS = [
    FnTest("ShopStatusPanel::ApplyChanges", ppc, host, edge=edges("apply", 11), rnd=rnd("apply")),
    FnTest("ShopStatusPanel::applytoallcallback", ppc, host, edge=edges("all", 12), rnd=rnd("all")),
    FnTest("ShopStatusPanel::Show", ppc, host, edge=edges("show", 13), rnd=rnd("show")),
]
