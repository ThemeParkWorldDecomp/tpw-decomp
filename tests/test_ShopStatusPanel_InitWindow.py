"""Unit v5085, the shop status panel, against libtpw_unit_ShopStatusPanel_InitWindow.dylib (tests/unit_ShopStatusPanel_InitWindow.cpp), main app
("SimTheme Park"), src/game/ui/ui_shopStatus.cpp:
  ShopStatusPanel::InitWindow 0x101bd28c, ShopStatusPanel::UpdateStatusWindow 0x101bcea0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetWindow, Show, the windows' virtual SetValue and SetText, the draw objects' constructors, the shop's figures, FormatString, the
runtime's operator new and double to unsigned glue ...) are fakes on both sides that record their arguments and answer from the case.

The world of a case: the panel (state, window, status window, ops table, quality, ingredient), the two shop objects, the windows the
GetWindow fake finds (a missing one answers NULL), the sums of the three histories, the figures of the shop, the text table. What is
compared: the event log and the panel's state, quality and ingredient after the call.

No fused operations and no fctiwz in the unit's code (the double to unsigned glue is a fake that saturates like fctiwz). Not tested: a
missing window in UpdateStatusWindow (the original calls through its NULL vtable), negative history sums (the glue's input is a sum of
unsigned counts), a quality whose product with 50 overflows 32 bits.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShopStatusPanel_InitWindow"
MASK = 0xFFFFFFFF
TOC = 0x102654E8 + 0x27D8

FN_INIT, FN_UPDATE = 0x101BD28C, 0x101BCEA0
SIZES = {FN_INIT: 1080, FN_UPDATE: 944}
OWN = set(SIZES)

# ---- the callees faked on both sides
GETDBINFO = 0x10107CD4
GETWINDOW = 0x101DBE40
SHOW = 0x101DC1EC
FILLOBJECTLIST = 0x10192A90
COOLRECT_CTOR = 0x101C7448
COOLRECT_DTOR = 0x101C74E0
SETMINMAX = 0x101D6B44
OPERATOR_NEW = 0x102384B8
CASH_CTOR = 0x1018DE60
SPRITE_CTOR = 0x1018D11C
SETCLIENTDRAWOBJECT = 0x101DB624
SETMINMAXVALUES = 0x101D8E44
SETINC = 0x101D8EE8
GETFONTTABLEENTRY = 0x10180270
SETFONT = 0x101DA118
SETFONTJUST = 0x101DA204
SETFONTCOLOUR = 0x101DA3AC
GETSTRING = 0x10180468
SETCLIENTDRAWTEXT = 0x101DE130
GETSPECIALINGREDIENTNAME = 0x1010FCC4
CONVERTMULTI = 0x101803CC
INITEDITBOX = 0x10193DCC
GETSUMOFLAST = 0x100F1000
CVT = 0x10237CD8
FORMATSTRING = 0x10239730
GETPROSPECTIVECOST = 0x1010F598
GETCUSTOMERSATISFACTION = 0x1010F9D0
GETPROFITABILITY = 0x1010F8D4
GETRESIDUAL = 0x1010FFC8
GETPARKANALYSER = 0x1013EBC8
GETHAPPINESS = 0x100EF00C
GETOBJECTSTATE = 0x10180B84
# the windows' and the panel's table's virtual functions (the slots' code addresses are the fakes')
VT_SETVALUE = 0x101DFE84
VT_SETTEXT = 0x101DE248
VT_NOTIFY = 0x10192C28

ADDR = dict(staff=0x103947A4, shop=0x103959DC, world=0x1037EBFC, table=0x1037FF18 + 0x564, buffer=0x10380C48)
HOST_GLOBAL = dict(staff="gStaffRoomPanelThing", shop="gShopStatusObject", world="gGameWorld", table="gText_table", buffer="buffer")
MAC_PANEL = dict(state=4, window=0xC, status=0x10, ops=0x20, quality=0x24, ingr=0x28)
HOST_LAYOUT_TAIL = [0x6C, 0x1A4, 0x22C, 0x448]

INIT_IDS = [0xC069, 0xC06A, 0xC06B, 0xC086, 0xC085, 0xC07E, 0xC084, 0xC083]
UPDATE_IDS = [0xC070, 0xC076, 0xC077, 0xC079, 0xC078, 0xC07D, 0xC07C]
ALL_IDS = INIT_IDS + UPDATE_IDS
BLOCKS = ["object", "object2", "panel", "parent", "status", "font", "ingredient", "text27", "text28", "converted", "table", "analyser", "world"]
NAMES = BLOCKS + ["win%d" % i for i in ALL_IDS]       # label number = index + 1 (the buffer is 0x10380c48 on the Mac, "buffer" on the host)
LABELLED = NAMES + ["buffer"]

CUR = {}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def cvt(v):
    """The glue's result: fctiwz of the value (below 2^31) or of the value less 2^31, plus 2^31 (fctiwz saturates)."""
    def trunc(x):
        if x != x:
            return -0x80000000
        if x >= 2147483648.0:
            return 0x7FFFFFFF
        if x <= -2147483649.0:
            return -0x80000000
        return int(x)
    if v >= 2147483648.0:
        return (trunc(v - 2147483648.0) + 0x80000000) & MASK
    return trunc(v) & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        def wrapped(e):
            # the call saves r2 in the caller's frame (0x14(r1)) and the code reloads it afterwards
            e.write(e.r(1) + 0x14, struct.pack(">I", TOC))
            fn(e)
        STUBS.internal(addr, wrapped)
        return fn
    return deco


def rdw(e, a):
    return struct.unpack(">i", e.read(a, 4))[0]


@stub(GETDBINFO)
def st_getdbinfo(e):
    e.cl.rec("GetDBInfo", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


@stub(GETWINDOW)
def st_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    present = L.ret("win:%d" % wid)
    e.set_r(3, CUR["win"][wid] if present else 0)


@stub(SHOW)
def st_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 1)


@stub(FILLOBJECTLIST)
def st_fill(e):
    L = e.cl
    L.rec("FillObjectList", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("FillObjectList") & MASK)


@stub(COOLRECT_CTOR)
def st_coolrect(e):
    e.cl.rec("CoolRect", (e.cl.newlab(e.r(3)),))


@stub(COOLRECT_DTOR)
def st_coolrect_dtor(e):
    e.cl.rec("~CoolRect", (e.cl.lab(e.r(3)), s32(e.r(4))))


@stub(SETMINMAX)
def st_setminmax(e):
    e.cl.rec("SetMinMax", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(OPERATOR_NEW)
def st_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, e.alloc(0x19C))


@stub(CASH_CTOR)
def st_cash(e):
    e.cl.rec("CashCtor", (e.cl.newlab(e.r(3)), e.r(4)))


@stub(SPRITE_CTOR)
def st_sprite(e):
    e.cl.rec("SpriteCtor", (e.cl.newlab(e.r(3)), e.r(4)))


@stub(SETCLIENTDRAWOBJECT)
def st_setclientdrawobject(e):
    e.cl.rec("SetClientDrawObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


@stub(SETMINMAXVALUES)
def st_setminmaxvalues(e):
    e.cl.rec("SetMinMaxValues", (e.cl.lab(e.r(3)), rdw(e, e.r(4)), rdw(e, e.r(5))))


@stub(SETINC)
def st_setinc(e):
    e.cl.rec("SetInc", (e.cl.lab(e.r(3)), rdw(e, e.r(4))))


@stub(GETFONTTABLEENTRY)
def st_getfont(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, CUR["font"])


@stub(SETFONT)
def st_setfont(e):
    e.cl.rec("SetFont", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


@stub(SETFONTJUST)
def st_setfontjust(e):
    e.cl.rec("SetFontJust", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(SETFONTCOLOUR)
def st_setfontcolour(e):
    e.cl.rec("SetFontColour", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


@stub(GETSTRING)
def st_getstring(e):
    e.cl.rec("GetString", (s32(e.r(3)),))
    e.set_r(3, CUR["text28"] if s32(e.r(3)) == 0x28 else CUR["text27"])


@stub(SETCLIENTDRAWTEXT)
def st_setclientdrawtext(e):
    e.cl.rec("SetClientDrawText", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


@stub(GETSPECIALINGREDIENTNAME)
def st_getingredient(e):
    e.cl.rec("GetSpecialIngredientName", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["ingredient"] if e.cl.ret("ingr") else 0)


@stub(CONVERTMULTI)
def st_convertmulti(e):
    e.cl.rec("ConvertMulti", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["converted"])


@stub(INITEDITBOX)
def st_initeditbox(e):
    e.cl.rec("InitEditBox", (e.cl.lab(e.r(3)), s32(e.r(4))))


@stub(GETSUMOFLAST)
def st_sum(e):
    L = e.cl
    off = e.r(3) - CUR["object"]
    L.rec("Sum", (off, s32(e.r(5))))
    e.write(e.r(4), struct.pack(">d", float(L.ret("sum:%d" % off))))


@stub(CVT)
def st_cvt(e):
    e.set_r(3, cvt(float(e.f(1))))


@stub(FORMATSTRING)
def st_format(e):
    L = e.cl
    a, b = e.r(7), e.r(8)
    L.rec("FormatString", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5), e.r(6), rdw(e, a + 8), rdw(e, a + 0xC), rdw(e, b + 8), rdw(e, b + 0xC)))
    e.set_r(3, 0)


@stub(GETPROSPECTIVECOST)
def st_cost(e):
    L = e.cl
    L.rec("Cost", (L.lab(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2)))))
    e.set_r(3, L.ret("Cost") & MASK)


@stub(GETCUSTOMERSATISFACTION)
def st_satisfaction(e):
    e.cl.rec("Satisfaction", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Satisfaction") & MASK)


@stub(GETPROFITABILITY)
def st_profitability(e):
    e.cl.rec("Profitability", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Profitability") & MASK)


@stub(GETRESIDUAL)
def st_residual(e):
    e.cl.rec("Residual", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Residual") & MASK)


@stub(GETPARKANALYSER)
def st_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["analyser"])


@stub(GETHAPPINESS)
def st_happiness(e):
    L = e.cl
    L.rec("Happiness", (L.lab(e.r(3)), e.r(4) >> 16, e.r(5)))
    e.set_r(3, L.ret("Happiness") & MASK)


@stub(GETOBJECTSTATE)
def st_objectstate(e):
    L = e.cl
    L.rec("GetObjectState", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("GetObjectState") & MASK)


@stub(VT_SETVALUE)
def st_setvalue(e):
    e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 1)


@stub(VT_SETTEXT)
def st_settext(e):
    e.cl.rec("SetText", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
    e.set_r(3, 1)


@stub(VT_NOTIFY)
def st_notify(e):
    e.cl.rec("PanelNotify", (e.cl.lab(e.r(3)),))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def setint(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def setptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def setp32(self, a, v):
        """A 32-bit pointer field of the panel (the Mac's address; the host's arena offset, unit v5L31)."""
        self.write(a, struct.pack(self.fmt, v - self.base if v else 0))

    def rd_int(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    off = MAC_PANEL
    base = 0

    def __init__(self, e, case):
        self.e = e
        sizes = {"object": 0x448, "object2": 0x448, "panel": 0x40, "parent": 0x10, "status": 0x10, "font": 0x10, "ingredient": 0x10,
                 "text27": 0x20, "text28": 0x20, "converted": 0x20, "table": 0x10, "analyser": 0x10, "world": 0x10}
        self.a = {}
        for n in BLOCKS:
            if n in sizes:
                self.a[n] = e.alloc(sizes[n])
                e.write(self.a[n], bytes(sizes[n]))
        self.a["win"] = {}
        for i in ALL_IDS:
            w = e.alloc(0x120)
            e.write(w, bytes(0x120))
            self.a["win"][i] = w
            self.a["win%d" % i] = w
        # the windows' vtable: slot 0x24 is SetValue, 0x2c SetText; the panel's ops table has its notify at 0x2c
        toc = 0x102654E8 + 0x27D8   # r2: the double 2^52+2^31 constant is at -0x27d8(r2)
        vt = e.alloc(0x40)
        tv_value, tv_text, tv_notify = e.alloc(8), e.alloc(8), e.alloc(8)
        e.write(tv_value, struct.pack(">II", VT_SETVALUE, toc))
        e.write(tv_text, struct.pack(">II", VT_SETTEXT, toc))
        e.write(tv_notify, struct.pack(">II", VT_NOTIFY, toc))
        e.write(vt, bytes(0x40))
        e.write(vt + 0x24, struct.pack(">I", tv_value))
        e.write(vt + 0x2C, struct.pack(">I", tv_text))
        for i in ALL_IDS:
            e.write(self.a["win"][i] + 0x118, struct.pack(">I", vt))
        ops = e.alloc(0x40)
        e.write(ops, bytes(0x40))
        e.write(ops + 0x2C, struct.pack(">I", tv_notify))
        self.a["ops"] = ops
        self.a["buffer"] = ADDR["buffer"]
        CUR.update(object=self.a["object"], font=self.a["font"], text27=self.a["text27"], text28=self.a["text28"],
                   converted=self.a["converted"], ingredient=self.a["ingredient"], analyser=self.a["analyser"], win=self.a["win"])
        self.L = e.cl = CL.PpcLog({self.a[n]: i + 1 for i, n in enumerate(LABELLED)}, case["queues"])
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

    def call(self, fn):
        self.e.set_r(2, 0x102654E8 + 0x27D8)
        T.call_ppc(self.e, fn, [self.a["panel"]])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.panel_off
        self.a = {n: lib.tpw_shop_addr(n.encode()) for n in LABELLED}
        self.a["win"] = {i: self.a["win%d" % i] for i in ALL_IDS}
        self.a["ops"] = lib.tpw_shop_addr(b"opsobject")
        self.base = lib.tpw_shop_addr(b"arenabase")
        CL.host_begin(lib, {i + 1: self.a[n] for i, n in enumerate(LABELLED)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_shop_addr(HOST_GLOBAL[name].encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn):
        (self.lib.tpw_shop_init if fn == FN_INIT else self.lib.tpw_shop_update)(self.a["panel"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_shop_addr.restype, lib.tpw_shop_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_shop_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_shop_init.argtypes = [C.c_uint64]
    lib.tpw_shop_update.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_shop_layout(out)
    v = list(out)[:n]
    assert v[7:] == HOST_LAYOUT_TAIL, v
    lib.panel_off = dict(zip(["state", "window", "status", "ops", "quality", "ingr"], v[:6]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    a = s.a
    p = a["panel"]
    o = s.off
    if isinstance(s, Host):
        for n in ("object", "object2"):
            s.write(a[n], bytes(0x460))
        s.write(p, bytes(0x40))
    s.setptr(s.g("staff"), a["object"])
    s.setptr(s.g("shop"), a["object2"])
    s.setptr(s.g("world"), a["world"])
    s.setptr(s.g("table"), a["table"])
    s.setint(p + o["state"], case["state"])
    s.setp32(p + o["window"], a["parent"])
    s.setp32(p + o["status"], a["status"] if case["has_status"] else 0)
    s.setp32(p + o["ops"], a["ops"])
    s.setint(p + o["quality"], case["quality"])
    s.setint(p + o["ingr"], case["ingr"])
    s.write(a["object"] + 4, bytes([case["b4"]]))
    s.write(a["object"] + 6, bytes([case["b6"]]))


def execute(s, case):
    s.call(FN_INIT if case["op"] == "init" else FN_UPDATE)
    return None


def observe(s, case, answer):
    p = s.a["panel"]
    o = s.off
    return {"log": s.log(), "state": s.rd_int(p + o["state"]), "quality": s.rd_int(p + o["quality"]), "ingr": s.rd_int(p + o["ingr"])}


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

SUMS = [0, 1, 7, 100, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF, 12345678]


def rsum(r):
    k = r.random()
    if k < 0.3:
        return r.choice(SUMS)
    if k < 0.8:
        return r.randrange(0, 5000)
    return r.randrange(0, 1 << 32)


def rint(r):
    return r.choice([0, 1, -1, 99, 100, 101, -100, 0x7FFFFFFF, -0x80000000, 0x3FFFFF, 0x400000, r.randrange(-100000, 100000),
                     r.randrange(-(1 << 31), 1 << 31)])


def base(r, op, missing=()):
    q = {"win:%d" % i: [0 if i in missing else 1] for i in ALL_IDS}
    q["FillObjectList"] = [r.choice([0, 1, 2, 3, 7, -1])]
    q["ingr"] = [r.choice([0, 1])]
    q["sum:%d" % 0x1A4] = [rsum(r), rsum(r)]
    q["sum:%d" % 0x22C] = [rsum(r)]
    q["sum:%d" % 0x6C] = [rsum(r)]
    q["Cost"] = [rint(r)]
    q["Satisfaction"] = [rint(r)]
    q["Profitability"] = [rint(r)]
    q["Residual"] = [rint(r) & MASK]
    q["Happiness"] = [rint(r)]
    state = r.choice([0, 1, 2, -1, 5])
    q["GetObjectState"] = [state if r.random() < 0.4 else r.choice([0, 1, 2, -1, 5])]
    return {"op": op, "queues": q, "state": state & MASK, "quality": r.choice([0, 1, 2, 10, 100, 12345, -5]) & MASK,
            "ingr": r.choice([0, 1, 5, 77, -3]) & MASK, "has_status": r.random() < 0.85, "b4": r.randrange(256), "b6": r.randrange(256)}


def init_case(r, missing=()):
    return base(r, "init", missing)


def init_edges():
    r = random.Random(5085)
    for _ in range(4):
        yield init_case(r)
    for i in INIT_IDS:
        yield init_case(r, missing=(i,))
    yield init_case(r, missing=INIT_IDS)
    for n in (0, 1, 2, 3):
        c = init_case(r)
        c["queues"]["FillObjectList"] = [n]
        yield c


def init_random(r):
    return init_case(r, missing=[i for i in INIT_IDS if r.random() < 0.15])


def update_case(r):
    c = base(r, "update")
    return c


def update_edges():
    r = random.Random(1085)
    for sums in ([0, 0, 0, 0], [1, 2, 3, 4], [0x80000000, 0xFFFFFFFF, 0x80000001, 0x7FFFFFFF], [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF]):
        c = update_case(r)
        c["has_status"] = True
        c["queues"]["sum:%d" % 0x1A4] = [sums[0], sums[2]]
        c["queues"]["sum:%d" % 0x22C] = [sums[1]]
        c["queues"]["sum:%d" % 0x6C] = [sums[3]]
        yield c
    for v in (0, 100, -100, 1, 99, 0x3FFFFF, 0x400000, 0x7FFFFFFF, -0x80000000):
        c = update_case(r)
        c["has_status"] = True
        c["queues"]["Satisfaction"] = [v]
        c["queues"]["Happiness"] = [v]
        yield c
    for q, i in ((0, 0), (3, 4), (-5, -3), (1000000, 100000)):
        c = update_case(r)
        c["has_status"] = True
        c["quality"], c["ingr"] = q & MASK, i & MASK
        yield c
    for same in (True, False):
        c = update_case(r)
        c["has_status"] = r.choice([True, False])
        c["queues"]["GetObjectState"] = [c["state"] if same else (c["state"] + 1) & MASK]
        yield c
    c = update_case(r)
    c["has_status"] = False
    yield c
    for b4, b6 in ((0, 0), (255, 255), (1, 200)):
        c = update_case(r)
        c["has_status"] = True
        c["b4"], c["b6"] = b4, b6
        yield c


def update_random(r):
    return update_case(r)


TESTS = [
    FnTest("ShopStatusPanel::InitWindow", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("ShopStatusPanel::UpdateStatusWindow", ppc, host, edge=update_edges, rnd=update_random),
]
