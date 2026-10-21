"""Unit v5152, the staff costs panel's cost words and the two panel message functions, against libtpw_unit_UpdateStaffCost.dylib
(tests/unit_UpdateStaffCost.cpp), main app ("SimTheme Park"), src/game/ui/ui_StaffCostsPanel.cpp, src/game/ui/ui_shopStatus.cpp:
  UpdateStaffCost 0x101be030, StaffCostsPanelMsg 0x101be1f4, ShopStatusMsg 0x101bdb8c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetStaffHQ,
GetWindow, setAnimTimer, startAnimTimer, the panel creators and removers, the status panel's members, the default handlers,
UI_IterateThingList, the virtual calls through __ptr_glue) are fakes on both sides that record their arguments and answer from the case.
The windows, the world, the headquarters and the thing are fake objects named by labels (1 panel, 2 summary, 3 and 4 the two figures,
5 the window with id 0xc07f, 6 the shop panel's window, 7 the thing, 8 the callback window, 9 world, 10 headquarters, 11 shop panel).

The world of a case: the headquarters' five cost words and flag, the deposits and word-14 globals, the pointer globals, the shop panel's
numeric fields, the fakes' answers. Compared: the answer, the log, the cost words and flag, the globals, the panel's fields.

No fused operations. Not tested: the virtual call's panel argument on the host (the host's Slot5 takes none; the PowerPC side is not
checked for r3 either); a NULL gStaffCostsPanel on message 0x800 (the original calls GetWindow on it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateStaffCost"
MASK = 0xFFFFFFFF

FN_COST, FN_STAFF, FN_SHOP = 0x101BE030, 0x101BE1F4, 0x101BDB8C
SIZES = {FN_COST: 412, FN_STAFF: 524, FN_SHOP: 796}
OWN = set(SIZES)

ADDR = dict(gGameWorld=0x1037EBFC, gStaffCostsWord14=0x10395A14, gStaffCostsDeposits=0x10395A1C, gStaffCostsPanel=0x10395A20,
            gStaffRoomPanelThing=0x103947A4, panel=0x103959E0)
HOST_LAYOUT = [20, 24]

TAG_SLOT5, TAG_SETVALUE = 0x5001, 0x5002
APPLYTOALL = 0x10267E20


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = [None]


def lab(e, r):
    return e.cl.lab(e.r(r))


def simple(addr, name, spec, ret=None):
    """spec: one char per argument from r3: p label, i s32, x raw word."""
    def f(e):
        vals = []
        for k, ch in enumerate(spec):
            r = e.r(3 + k)
            vals.append(e.cl.lab(r) if ch == "p" else s32(r) if ch == "i" else r)
        e.cl.rec(name, tuple(vals))
        if ret:
            e.set_r(3, e.cl.ret(ret) & MASK)
    STUBS.internal(addr, f)


simple(0x101BF278, "RemoveStaffCostsPanel", "")
simple(0x101A001C, "CreateFinancePanel", "")
simple(0x1019D660, "CreateEntryPricePanel", "")
simple(0x101A4DE4, "CreateLoansPanel", "")
simple(0x10181570, "TickRidePreview", "")
simple(0x1018493C, "UIMain_DefaultPanelMsg", "piii", "UIMain_DefaultPanelMsg")
simple(0x10193C78, "DefaultStatusPanelMsg", "piii", "DefaultStatusPanelMsg")
simple(0x1018227C, "CancelAdvisorMessage", "i")
simple(0x101983C4, "CreateAllItemsPanel", "ip")
simple(0x101F7DB0, "UI_IterateThingList", "ix")
simple(0x101BDF64, "ShopRemove", "p")
simple(0x101BCEA0, "UpdateStatusWindow", "p")
simple(0x10192C28, "GetNextObject", "p")
simple(0x10192DE0, "GetPreviousObject", "p")
simple(0x10193138, "EraseObject", "p")
simple(0x101938D8, "Move", "p")
simple(0x10193010, "CloseObject", "pi")
simple(0x101DD5E0, "setAnimTimer", "pii", "setAnimTimer")
simple(0x101DD6B8, "startAnimTimer", "pi")


def stub_getstaffhq(e):
    e.cl.rec("GetStaffHQ", (lab(e, 3),))
    e.set_r(3, CUR[0].objs[10])


def stub_getwindow(e):
    e.cl.rec("GetWindow", (lab(e, 3), s32(e.r(4))))
    i = s32(e.r(4))
    o = CUR[0].objs
    if i == 0x2DE:
        r = o[2] if e.cl.ret("GetWindow:734") else 0
    else:
        r = {0x2DF: o[3], 0x2E4: o[4], 0xC07F: o[5]}.get(i, 0)
    e.set_r(3, r)


def stub_glue(e):
    tag = e.r(12)
    if tag == TAG_SLOT5:
        e.cl.rec("Slot5", ())
    else:
        assert tag == TAG_SETVALUE, hex(tag)
        e.cl.rec("SetValue", (lab(e, 3), s32(e.r(4))))


STUBS.internal(0x1013E99C, stub_getstaffhq)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = {i: e.alloc(0x138) for i in range(1, 10)}
        self.objs[10] = e.alloc(0x68)
        self.objs[11] = ADDR["panel"]
        table = e.alloc(0x40)
        e.write(table + 0x24, struct.pack(">I", TAG_SETVALUE))
        for i in range(1, 10):
            e.write(self.objs[i] + 0x118, struct.pack(">I", table))
        self.ops = e.alloc(0x40)
        e.write(self.ops + 0x1C, struct.pack(">I", TAG_SLOT5))
        self.L = e.cl = CL.PpcLog({a: i for i, a in self.objs.items()}, case["queues"])
        CUR[0] = self
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def log(self):
        return self.L.log

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def world(self, case):
        self.words(ADDR["gGameWorld"], [self.objs[9]])
        self.words(ADDR["gStaffCostsPanel"], [self.objs[1]])
        self.words(ADDR["gStaffRoomPanelThing"], [self.objs[case["thing"]] if case["thing"] else 0])
        self.write(ADDR["panel"], bytes(0x30))
        self.words(ADDR["panel"] + 0x0C, [self.objs[6]])
        self.words(ADDR["panel"] + 0x20, [self.ops])
        for i, w in enumerate(case["hq"][:5]):
            self.words(self.objs[10] + 0x0C + 4 * i, [w])
        self.write(self.objs[10] + 0x20, bytes([case["hq"][5] & 0xFF]))

    def hq(self):
        return self.rd_words(self.objs[10] + 0x0C, 5) + [self.read(self.objs[10] + 0x20, 1)[0]]

    def panel_field(self, off):
        return self.rd_words(ADDR["panel"] + off, 1)[0]

    def panel_label(self):
        v = self.rd_words(ADDR["gStaffCostsPanel"], 1)[0]
        back = {a: i for i, a in self.objs.items()}
        return 0 if v == 0 else back.get(v, -1)

    def win(self, i):
        return self.objs[i]

    def call(self, fn, args):
        w, m, a, b = args
        r3, _, _ = T.call_ppc(self.e, fn, [self.objs[w], m & MASK, a & MASK, b & MASK])
        return r3 & MASK

    def call_cost(self, a, b):
        T.call_ppc(self.e, FN_COST, [a & MASK, b & MASK])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i: lib.tpw_sm_obj(i) for i in range(1, 12)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sm_addr(name.encode())

    def log(self):
        return CL.host_end(self.lib)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def world(self, case):
        self.lib.tpw_sm_reset(1, 6, case["thing"])
        for i, w in enumerate(case["hq"][:6]):
            self.lib.tpw_sm_hq_set(i, w & MASK)

    def hq(self):
        return [self.lib.tpw_sm_hq_get(i) for i in range(6)]

    def panel_field(self, off):
        return self.lib.tpw_sm_panel_get(off) & MASK

    def panel_label(self):
        return self.lib.tpw_sm_get_panel()

    def call(self, fn, args):
        w, m, a, b = args
        f = self.lib.tpw_sm_StaffCostsPanelMsg if fn == FN_STAFF else self.lib.tpw_sm_ShopStatusMsg
        return f(w, s32(m), s32(a), s32(b)) & MASK

    def call_cost(self, a, b):
        self.lib.tpw_sm_UpdateStaffCost(s32(a), s32(b))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sm_"
    lib.tpw_sm_addr.restype, lib.tpw_sm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sm_alloc.restype, lib.tpw_sm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sm_obj.restype, lib.tpw_sm_obj.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sm_reset.argtypes = [C.c_int32] * 3
    lib.tpw_sm_hq_set.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_sm_hq_get.restype, lib.tpw_sm_hq_get.argtypes = C.c_uint32, [C.c_int32]
    lib.tpw_sm_panel_set.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sm_panel_get.restype, lib.tpw_sm_panel_get.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_sm_get_panel.restype = C.c_int32
    lib.tpw_sm_UpdateStaffCost.argtypes = [C.c_int32, C.c_int32]
    for n in ("StaffCostsPanelMsg", "ShopStatusMsg"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_int32] * 4
    out = (C.c_int64 * 32)()
    n = lib.tpw_sm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.world(case)
    s.words(s.g("gStaffCostsDeposits"), [case["dep"]])
    s.words(s.g("gStaffCostsWord14"), [case["w14"]])
    if isinstance(s, Ppc):
        for off, v in zip((0x24, 0x28, 0x2C), case["fields"]):
            s.words(ADDR["panel"] + off, [v])
    else:
        for off, v in zip((0x24, 0x28, 0x2C), case["fields"]):
            s.lib.tpw_sm_panel_set(off, s32(v))


def execute(s, case):
    if case["op"] == "cost":
        return s.call_cost(*case["args"])
    return s.call(FN_STAFF if case["op"] == "staff" else FN_SHOP, case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "hq": s.hq(), "dep": s.rd_words(s.g("gStaffCostsDeposits"), 1),
            "w14": s.rd_words(s.g("gStaffCostsWord14"), 1), "fields": [s.panel_field(o) for o in (0x24, 0x28, 0x2C)],
            "panel": s.panel_label()}


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


# ---------------------------------------------------------------------------------------------------------------- cases

WORDS = [0, 1, 5, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 12345]


def base(r, op, args):
    return {"op": op, "args": args,
            "queues": {"GetWindow:734": [r.choice([0, 1])], "setAnimTimer": [r.choice([0, 1, 0x10000, 0x10001, 0xFFFF])],
                       "UIMain_DefaultPanelMsg": [r.choice([0, 1, -1, r.randrange(1 << 31)])],
                       "DefaultStatusPanelMsg": [r.choice([0, 1, -1, r.randrange(1 << 31)])]},
            "dep": r.choice(WORDS) if r.random() < 0.5 else r.randrange(1 << 32),
            "w14": r.choice(WORDS) if r.random() < 0.5 else r.randrange(1 << 32),
            "hq": [r.choice(WORDS) if r.random() < 0.6 else r.randrange(1 << 32) for _ in range(5)] + [r.choice([0, 0, 1, 7])],
            "fields": [r.choice(WORDS) for _ in range(3)], "thing": r.choice([0, 7])}


COST_IDS = [0x2E6, 0x2E7, 0x2E8, 0x2E9, 0x2EA, 0x2EB, 0x2EC, 0x2ED, 0x2EE, 0, -1, 0x7FFFFFFF]
VALS = [-1, 0, 1, 5, 0x7FFFFFFF, -0x80000000, 100000]


def cost_edges():
    r = random.Random(11)
    for i in COST_IDS:
        for v in VALS:
            c = base(r, "cost", [i & MASK, v & MASK])
            if r.random() < 0.5:
                c["hq"][5] = 0
            yield c


def cost_random(r):
    return base(r, "cost", [r.choice(COST_IDS), r.choice(VALS + [r.randrange(-1000, 1000)]) & MASK])


STAFF_MSGS = [5, 0x14, 0x15, 0x13, 6, 0, 0x100, 0x101, 0x800, 0x7FF, 0x801, 0x10, 0x1E, -1]
STAFF_A = [-1, 0, 0x2DA, 0x2DB, 0x2DC, 0x2DD, 0x2DE, 0x2E7, 0x2EA, 0x2EB, 0x2EC, 0x2ED, 0x2EE, 0x2E8]


def staff_edges():
    r = random.Random(22)
    for m in STAFF_MSGS:
        for a in (STAFF_A if m in (0x100, 0x800) else [0, 0x2E7]):
            for b in ([-5, 0, 7, 1000] if m == 0x800 else [3]):
                for q in ([0, 1] if m == 0x800 else [0]):
                    c = base(r, "staff", [8, m & MASK, a & MASK, b & MASK])
                    c["queues"]["GetWindow:734"] = [q]
                    yield c


def staff_random(r):
    m = r.choice(STAFF_MSGS)
    return base(r, "staff", [8, m & MASK, r.choice(STAFF_A) & MASK, r.choice([-5, 0, 7, 1000, r.randrange(-3000, 3000)]) & MASK])


SHOP_MSGS = [5, 0x10, 0x14, 0x15, 0x1E, 0x100, 0x800, 0x11, 0x13, 0x16, 0xFF, 0x101, 0x7FF, 0x801, 0]
SHOP_A100 = [-3, -2, -1, 0, 1, 0xC066, 0xC067, 0xC068, 0xC069, 0xC06A, 0xC06B, 0xC07F, 0xC080, 0xC081, 0xC082, 0xC083, 0xC084, 0x7FFFFFFF,
             -0x80000000]
SHOP_A800 = [0xC06A, 0xC06B, 0xC06C, 0xC084, 0xC085, 0xC086, 0xC087, 0xC088, 0, -1, 0x7FFFFFFF]
SHOP_B = [-1, 0, 1, 2, 5, 31, 32, 33, 63, 64, 100, -32, 0x7FFFFFFF]


def shop_edges():
    r = random.Random(33)
    for m in SHOP_MSGS:
        As = SHOP_A100 if m == 0x100 else SHOP_A800 if m == 0x800 else [0x80080, 0x80081, 0x80000, 0, 0x80000 + 0x100, -0x7FF80000 + 1]
        for a in As:
            for b in (SHOP_B if m in (0x100, 0x800) else [1]):
                for q in ([0, 1, 0x10000] if m == 0x15 else [1]):
                    c = base(r, "shop", [8, m & MASK, a & MASK, b & MASK])
                    c["queues"]["setAnimTimer"] = [q]
                    yield c


def shop_random(r):
    m = r.choice(SHOP_MSGS)
    As = SHOP_A100 if m == 0x100 else SHOP_A800 if m == 0x800 else [0x80080, 0x80081, 0, r.randrange(1 << 32)]
    return base(r, "shop", [8, m & MASK, r.choice(As) & MASK, r.choice(SHOP_B + [r.randrange(-100, 100)]) & MASK])


TESTS = [
    FnTest("UpdateStaffCost", ppc, host, edge=cost_edges, rnd=cost_random),
    FnTest("StaffCostsPanelMsg", ppc, host, edge=staff_edges, rnd=staff_random),
    FnTest("ShopStatusMsg", ppc, host, edge=shop_edges, rnd=shop_random),
]
