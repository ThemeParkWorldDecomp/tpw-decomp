"""Unit v5219, the all-kids panel and the rides / sideshows list rows, against libtpw_unit_AddItemToAllRidesList.dylib
(tests/unit_AddItemToAllRidesList.cpp), main app ("SimTheme Park"), src/AddItemToAllRidesList.cpp:
  CreateAllKidsPanel 0x101952bc, AllKidsPanel_KidAdded 0x101957b8, AddItemToAllRidesList 0x10195978,
  FillAllSideshowsListCallback 0x10195dc8, UpdateRideListEntries 0x1019600c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments (pointers as labels, the cells of a row word by word) and answers from the
case's queues. The two runtime helpers the listings call (0x10237cd8, double to word; 0x10237e1c, 64-bit divide) are NOT faked: they run
as the original on the PowerPC side, which pins the host's translation of them.

The world of a case: a person / ride block (the fields the rows read, at their Mac offsets), a second ride and a list block, the world block
(only its address + 0x2a0 is used), the thing array's entries 0 and the parent's, the globals the panel code reads and writes. What is
compared: the answer, the event log (including every cell of every row added or changed) and the globals written.

No fused operations. Not tested: a NaN or an infinity or a value past 2^62 as the history sum, a float past 2^31 (fctiwz saturates
differently in the emulator), a thing array entry that is NULL when read, GetData answering 0 (the original then uses an unset stack word).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AddItemToAllRidesList"
MASK = 0xFFFFFFFF

FN_CREATE, FN_KID, FN_ADD, FN_FILL, FN_UPD = 0x101952BC, 0x101957B8, 0x10195978, 0x10195DC8, 0x1019600C
SIZES = {FN_CREATE: 952, FN_KID: 320, FN_ADD: 368, FN_FILL: 288, FN_UPD: 440}
OWN = set(SIZES)

# ---- callees faked on both sides
CALLEES = dict(
    CheckPanel=0x10180514, SetLastPanel=0x101A57FC, Build=0x101DEB78, SetPanel=0x101805A0, GetWindow=0x101DBE40,
    SetColumnFormatsAll=0x101D2864, SetColumnType=0x101D2614, FindDrawObject=0x101CD2D4, SetSelectionObject=0x101D1EB4,
    SetSort=0x101D4DB8, Iterate=0x101F7DB0, UpdateDisplay=0x101D2220, FindData=0x101D4384, SetCurrent=0x101D4C64,
    GetString=0x10180468, MakeButton=0x101818E4, SetAnim=0x101DD5E0, StartAnim=0x101DD6B8, Advisor=0x101821CC,
    GetDBInfo=0x10107CD4, GetObjectName=0x101810FC, GetSumOfLast=0x100F1000, GetExcitement=0x1010CFB0, GetRemainingLife=0x10108024,
    AddItem=0x101D3000, GetProfit=0x1010F8D4, GetRevenue=0x1010F894, GetSatisfaction=0x1010F9D0, GetUpgradeParent=0x1010DA64,
    Assert=0x1000DDE8, GetData=0x101D41F0, ChangeItem=0x101D3660, GetTimeInPark=0x1011962C, GetFunny=0x10112928,
)

# ---- the Mac's addresses of the globals
ADDR = dict(gGameWindow=0x103949E4, gGameWorld=0x1037EBFC, gAllKidsListControl=0x103947C4, gAllKidsMsgWindow=0x103947C8,
            gAllItemsList=0x103947D8, gAllKidsSelectedIndex=0x102A8880, gAllKidsColumnFont=0x102A887C, gAllKidsCounter=0x103947C0,
            gAllRidesNoParent=0x102A8D0A, script=0x102A868C, cbMsg=0x10267A08, cbFormat=0x10267A00, cbPost=0x102679F8, cbFill=0x10267A10)
THING_ARRAY = 0x1034CBFC       # five words an entry on the Mac, the thing at word 0
MSG = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"

TOK_WIN, TOK_GAME = 0x70000006, 0x70000007
TOK_BTN = 0x70000020           # + i: the column button windows
LABELS = dict(thing=1, obj=2, obj2=3, list=4, funny=5, win=6, gamewin=7, script=8, cbMsg=9, cbFormat=10, cbPost=11, cbFill=12)
HOST_LAYOUT = [0x40, 0x1A4]    # the CObject's mF40 and mHistory1A4; the third word of the layout is the list's mItemCount on the host


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def simple(name, kinds, ret=False):
    def stub(e):
        L = e.cl
        vals = []
        for i, k in enumerate(kinds):
            v = e.r(3 + i)
            vals.append(L.lab(v) if k == "L" else (s32(v) if k == "i" else v & MASK))
        L.rec(name, tuple(vals))
        if ret:
            e.set_r(3, L.ret(name) & MASK)
    return stub


for _name, _kinds, _ret in [
    ("UIMain_CheckPanel", "", False), ("SetLastPanel", "ii", False), ("Build", "LLL", True), ("UIMain_SetPanel", "L", False),
    ("SetColumnFormatsAll", "LLL", False), ("SetColumnType", "Lii", False), ("SetSelectionObject", "Lu", False),
    ("SetSort", "Lii", False), ("UI_IterateThingList", "iL", False), ("UpdateDisplay", "L", False), ("FindData", "LL", True),
    ("SetCurrent", "Li", False), ("UIMain_GetString", "i", True), ("MAKECOLUMNBUTTON", "Luiiiii", False), ("setAnimTimer", "Lii", True),
    ("startAnimTimer", "Li", False), ("UIMain_AdvisorMessage", "i", False), ("GetDBInfo", "L", False),
    ("UIMain_GetObjectName", "L", True), ("GetExcitementLevel", "Li", True), ("GetRemainingLife", "L", True),
    ("GetProfitability", "L", True), ("GetTotalRevenue", "L", True), ("GetCustomerSatisfaction", "L", True),
    ("GetTimeInPark", "L", True),
]:
    _key = {"UIMain_CheckPanel": "CheckPanel", "SetLastPanel": "SetLastPanel", "Build": "Build", "UIMain_SetPanel": "SetPanel",
            "SetColumnFormatsAll": "SetColumnFormatsAll", "SetColumnType": "SetColumnType", "SetSelectionObject": "SetSelectionObject",
            "SetSort": "SetSort", "UI_IterateThingList": "Iterate", "UpdateDisplay": "UpdateDisplay", "FindData": "FindData",
            "SetCurrent": "SetCurrent", "UIMain_GetString": "GetString", "MAKECOLUMNBUTTON": "MakeButton", "setAnimTimer": "SetAnim",
            "startAnimTimer": "StartAnim", "UIMain_AdvisorMessage": "Advisor", "GetDBInfo": "GetDBInfo",
            "UIMain_GetObjectName": "GetObjectName", "GetExcitementLevel": "GetExcitement", "GetRemainingLife": "GetRemainingLife",
            "GetProfitability": "GetProfit", "GetTotalRevenue": "GetRevenue", "GetCustomerSatisfaction": "GetSatisfaction",
            "GetTimeInPark": "GetTimeInPark"}[_name]
    STUBS.internal(CALLEES[_key], simple(_name, _kinds, _ret))


def stub_get_window(e):
    L = e.cl
    ident = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), ident))
    key = "GetWindow_list" if ident == 0x1E497 else "GetWindow_b%d" % (ident - 0x10) if 0x10 <= ident < 0x16 else "GetWindow_other"
    e.set_r(3, L.ret(key) & MASK)


def stub_find_draw_object(e):
    L = e.cl
    L.rec("FindDrawObject", (1 if e.cstr(e.r(3)) == "hilight" else 0,))
    e.set_r(3, L.ret("FindDrawObject") & MASK)


def stub_assert(e):
    L = e.cl
    L.rec("GAME_ASSERTMSG", (s32(e.r(3)), 1 if e.cstr(e.r(4)) == MSG else 0))


def stub_get_sum(e):
    L = e.cl
    L.rec("GetSumOfLast", (L.lab((e.r(3) - 0x1A4) & MASK), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">II", L.ret("sum_hi") & MASK, L.ret("sum_lo") & MASK))
    e.set_r(3, 0)


def stub_get_parent(e):
    L = e.cl
    L.rec("GetUpgradeParent", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetUpgradeParent") & 0xFFFF))


def stub_funny(e):
    L = e.cl
    L.rec("GetFunnyTimeForGameTurns", (L.lab(e.r(4)), e.r(5) & MASK))
    e.write(e.r(3), struct.pack(">II", L.ret("funny_hi") & MASK, L.ret("funny_lo") & MASK))


def stub_get_data(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    data = L.ret("GetDataData") & MASK
    e.write(e.r(5), struct.pack(">I", data))
    e.set_r(3, L.ret("GetData") & MASK)


def stub_add(e):
    L = e.cl
    n = L.ret("ncols")
    cells = [s32(w) for w in struct.unpack(">%dI" % n, e.read(e.r(4), 4 * n))]
    L.rec("AddItem", (L.lab(e.r(3)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7))) + tuple(cells))
    e.set_r(3, L.ret("AddItem") & MASK)


def stub_change(e):
    L = e.cl
    cells = [s32(w) for w in struct.unpack(">5I", e.read(e.r(5), 20))]
    L.rec("ChangeItem", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))) + tuple(cells))


STUBS.internal(CALLEES["GetWindow"], stub_get_window)
STUBS.internal(CALLEES["FindDrawObject"], stub_find_draw_object)
STUBS.internal(CALLEES["Assert"], stub_assert)
STUBS.internal(CALLEES["GetSumOfLast"], stub_get_sum)
STUBS.internal(CALLEES["GetUpgradeParent"], stub_get_parent)
STUBS.internal(CALLEES["GetFunny"], stub_funny)
STUBS.internal(CALLEES["GetData"], stub_get_data)
STUBS.internal(CALLEES["AddItem"], stub_add)
STUBS.internal(CALLEES["ChangeItem"], stub_change)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def label(self, v):
        return self.rev.get(v, v)

    def finish_syms(self):
        self.rev = {v: k for k, v in self.syms.items() if v}

    def queues(self, case):
        q = {}
        for k, vs in case["queues"].items():
            q[k] = [self.syms[v] if isinstance(v, str) else v for v in vs]
        return q


class Ppc(Side):
    fmt = ">I"
    COUNT = 0x150

    def __init__(self, e, case):
        self.e = e
        self.syms = {"0": 0, "thing": e.alloc(0x440), "obj": e.alloc(0x440), "obj2": e.alloc(0x440), "list": e.alloc(0x180),
                     "world": e.alloc(0x2B0), "win": TOK_WIN, "gamewin": TOK_GAME}
        self.syms["funny"] = self.syms["world"] + 0x2A0
        for i in range(6):
            self.syms["b%d" % i] = TOK_BTN + i
        self.finish_syms()
        fixed = {self.syms[k]: LABELS[k] for k in ("thing", "obj", "obj2", "list", "funny", "win", "gamewin")}
        fixed.update({ADDR[k]: LABELS[k] for k in ("script", "cbMsg", "cbFormat", "cbPost", "cbFill")})
        fixed.update({TOK_BTN + i: 20 + i for i in range(6)})
        self.L = e.cl = CL.PpcLog(fixed, self.queues(case))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def set_thing(self, index, value):
        self.words(THING_ARRAY + 20 * index, [value])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, case):
        a = case["args"]
        entry = {"create": FN_CREATE, "kid": FN_KID, "add": FN_ADD, "fill": FN_FILL, "upd": FN_UPD}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [self.syms[x] for x in a])
        return r3 & MASK if op == "fill" else 0


class Host(Side):
    fmt = "<I"
    COUNT = None

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.COUNT = lib.count_offset
        self.syms = {"0": 0, "thing": lib.tpw_av_alloc(0x440), "obj": lib.tpw_av_alloc(0x440), "obj2": lib.tpw_av_alloc(0x440),
                     "list": lib.tpw_av_alloc(0x400), "world": lib.tpw_av_alloc(0x2B0), "win": TOK_WIN, "gamewin": TOK_GAME}
        self.syms["funny"] = self.syms["world"] + 0x2A0
        for i in range(6):
            self.syms["b%d" % i] = TOK_BTN + i
        for k in ("script", "cbMsg", "cbFormat", "cbPost", "cbFill"):
            self.syms[k] = lib.tpw_av_addr(k.encode())
        self.finish_syms()
        fixed = {LABELS[k]: self.syms[k] for k in ("thing", "obj", "obj2", "list", "funny", "win", "gamewin")}
        fixed.update({LABELS[k]: self.syms[k] for k in ("script", "cbMsg", "cbFormat", "cbPost", "cbFill")})
        fixed.update({20 + i: TOK_BTN + i for i in range(6)})
        CL.host_begin(lib, fixed, self.queues(case))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_av_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def set_thing(self, index, value):
        self.lib.tpw_av_set_thing(index, value)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, case):
        a = [self.syms[x] for x in case["args"]]
        lib = self.lib
        if op == "create":
            lib.tpw_av_create(*a)
        elif op == "kid":
            lib.tpw_av_kid(*a)
        elif op == "add":
            lib.tpw_av_add(*a)
        elif op == "upd":
            lib.tpw_av_upd(*a)
        else:
            return lib.tpw_av_fill(*a) & MASK
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_av_addr.restype, lib.tpw_av_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_av_alloc.restype, lib.tpw_av_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_av_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_av_set_thing.argtypes = [C.c_uint32, C.c_uint64]
    for n in ("create", "kid", "upd"):
        f = getattr(lib, "tpw_av_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    lib.tpw_av_add.restype, lib.tpw_av_add.argtypes = None, [C.c_uint64] * 3
    lib.tpw_av_fill.restype, lib.tpw_av_fill.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_av_layout(out)
    assert list(out)[:2] == HOST_LAYOUT, list(out)[:n]
    lib.count_offset = out[2]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for k in ("thing", "obj", "obj2"):
        f = case[k]
        base = s.syms[k]
        s.words(base + 0x30, [f["w30"]])
        s.words(base + 0x40, [f["f40"]])
        s.words(base + 0x19C, [f["f19c"]])
        s.words(base + 0x1A0, [f["w1a0"]])
        s.words(base + 0x1C4, [f["w1c4"]])
        s.words(base + 0x1D8, [f["w1d8"]])
    s.words(s.syms["list"] + s.COUNT, [case["count"]])
    s.ptr(s.g("gGameWindow"), s.syms[case["gamewin"]])
    s.ptr(s.g("gGameWorld"), s.syms["world"])
    s.ptr(s.g("gAllKidsMsgWindow"), s.syms[case["msgwin"]])
    s.ptr(s.g("gAllKidsListControl"), s.syms[case["lc0"]])
    s.ptr(s.g("gAllItemsList"), s.syms["list"])
    s.words(s.g("gAllKidsSelectedIndex"), [case["sel"]])
    s.words(s.g("gAllKidsColumnFont"), [case["font"]])
    s.words(s.g("gAllKidsCounter"), [case["cnt0"]])
    s.half(s.g("gAllRidesNoParent"), case["noparent"])
    s.set_thing(0, s.syms[case["ta0"]])
    for index in case["parents"]:
        s.set_thing(index, s.syms["obj2"])


def execute(s, case):
    return s.call(case["op"], case)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "msgwin": s.label(s.rd_ptr(s.g("gAllKidsMsgWindow"))), "lc": s.label(s.rd_ptr(s.g("gAllKidsListControl"))),
            "items": s.label(s.rd_ptr(s.g("gAllItemsList"))),
            "sel": s.rd_word(s.g("gAllKidsSelectedIndex")), "font": s.rd_word(s.g("gAllKidsColumnFont")),
            "cnt": s.rd_word(s.g("gAllKidsCounter")), "noparent": s.rd_half(s.g("gAllRidesNoParent"))}


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

F40S = [100.0, 0.0, -0.0, 99.99, 25.5, 255.9, 256.0, 300.7, 1023.5, -1.0, -5.5, 1000000.5, 2.0e9, -2.0e9, 0.5, 49.999]
SUMS = [0.0, -0.0, 1.0, 1234.5, 2.0 ** 31, 2.0 ** 32 - 1, 2.0 ** 32, 3.0e9, 1e10, -1.0, -2.0 ** 31, 12345678.9, 1e15, 2.0 ** 52,
        2.0 ** 33 + 5, -3.0e9, 255.0, 0.999]
TICKS = [0, 1, 0x861C46800, -0x861C46800, 0x861C46800 - 1, 0x861C46800 * 5 + 77, -0x861C46800 * 3 - 5, 10 ** 15, -(10 ** 15),
         0x7FFFFFFFFFFF, 0x100000000, 0x861C46800 * 0x12345, 0x861C46800 * 0x100000000, -0x861C46800 * 0x100000000 - 9]
I32 = [0, 1, -1, 99, 100, 101, 0x7FFFFFFF, -0x80000000, 0x3FC00, 255, 256, 0x12345678, -12345]


def rf40(r):
    k = r.random()
    return fb(r.choice(F40S)) if k < 0.5 else fb(r.uniform(-300, 600))


def rint(r):
    return r.choice(I32) if r.random() < 0.4 else r.randrange(-(1 << 31), 1 << 31)


def block(r):
    return dict(w30=r.choice([0, 1, 2, 999, 1000, -1, r.randrange(0, 2000)]) & MASK, f40=rf40(r), f19c=rf40(r),
                w1a0=rint(r) & MASK, w1c4=rint(r) & MASK, w1d8=rint(r) & MASK)


def double_words(d):
    bits = struct.unpack(">Q", struct.pack(">d", d))[0]
    return [bits >> 32, bits & MASK]


def base(r, op, args):
    sums = [r.choice(SUMS) if r.random() < 0.7 else r.uniform(0, 5e6) for _ in range(3)]
    ticks = r.choice(TICKS) if r.random() < 0.8 else r.randrange(-(1 << 52), 1 << 52)
    q = {
        "ncols": [{"add": 5, "upd": 5, "kid": 6, "fill": 6, "create": 6}[op]],
        "sum_hi": [double_words(x)[0] for x in sums], "sum_lo": [double_words(x)[1] for x in sums],
        "GetExcitementLevel": [r.choice([0, 1, 50, 99, 100, 255, 256, 0x1FF, r.randrange(0, 0x10000)]) for _ in range(3)],
        "GetRemainingLife": [r.choice(I32) & MASK if r.random() < 0.5 else r.randrange(0, 1000) for _ in range(3)],
        "GetProfitability": [rint(r) & MASK, rint(r) & MASK], "GetTotalRevenue": [rint(r) & MASK, rint(r) & MASK],
        "GetCustomerSatisfaction": [rint(r) & MASK, rint(r) & MASK],
        "UIMain_GetObjectName": [0x70001000 + r.randrange(0, 100) * 2, 0x70001000 + r.randrange(0, 100) * 2],
        "GetTimeInPark": [r.choice([0, 1, 100, 0x7FFFFFFF, 0xFFFFFFFF, r.randrange(0, 1 << 32)])],
        "funny_hi": [(ticks >> 32) & MASK], "funny_lo": [ticks & MASK],
        "FindData": [r.choice([-1, 0, 3, 7, -5])], "AddItem": [0],
    }
    return {"op": op, "args": args, "queues": q, "thing": block(r), "obj": block(r), "obj2": block(r), "count": r.randrange(0, 5),
            "gamewin": "gamewin", "msgwin": r.choice(["win", "0"]), "lc0": r.choice(["0", "list"]), "sel": rint(r) & MASK,
            "font": r.choice([8, 0, 3, rint(r) & MASK]), "cnt0": r.choice([0, 5, 99]), "noparent": r.choice([0, 0, 1, 3]),
            "ta0": "0", "parents": [], }


# ---- AddItemToAllRidesList
def add_case(r, **kw):
    c = base(r, "add", [r.choice(["thing", "thing", "0"]) if False else "thing", "list", r.choice(["obj", "0"])])
    c.update(kw)
    return c


def add_edges():
    r = random.Random(5219)
    for obj in ("0", "obj"):
        for fd in (-1, 0, 4):
            c = add_case(r)
            c["args"] = ["thing", "list", obj]
            c["queues"]["FindData"] = [fd]
            yield c
    for sm in range(len(SUMS)):
        c = add_case(r)
        c["queues"]["sum_hi"], c["queues"]["sum_lo"] = [double_words(SUMS[sm])[0]], [double_words(SUMS[sm])[1]]
        yield c
    for f in F40S:
        c = add_case(r)
        c["thing"]["f40"] = c["obj"]["f40"] = fb(f)
        c["args"] = ["thing", "list", r.choice(["0", "obj"])]
        yield c


def add_random(r):
    return add_case(r)


# ---- AllKidsPanel_KidAdded
def kid_case(r, **kw):
    c = base(r, "kid", ["thing"])
    c.update(kw)
    return c


def kid_edges():
    r = random.Random(5220)
    yield kid_case(r, msgwin="0")
    yield kid_case(r, msgwin="win")
    for w30 in (0, 1, 2, 0x80000000, 0xFFFFFFFF):
        c = kid_case(r, msgwin="win")
        c["thing"]["w30"] = w30
        yield c
    for t in TICKS:
        c = kid_case(r, msgwin="win")
        c["queues"]["funny_hi"], c["queues"]["funny_lo"] = [(t >> 32) & MASK], [t & MASK]
        yield c
    for f in F40S:
        c = kid_case(r, msgwin="win")
        c["thing"]["f19c"] = fb(f)
        yield c


def kid_random(r):
    return kid_case(r)


# ---- FillAllSideshowsListCallback
def fill_case(r, **kw):
    c = base(r, "fill", ["thing"])
    c.update(kw)
    return c


def fill_edges():
    r = random.Random(5221)
    for v in I32:
        c = fill_case(r)
        c["queues"]["GetCustomerSatisfaction"] = [v & MASK]
        yield c
    for x in (0, 1, 255, 256, 0x1FF):
        c = fill_case(r)
        c["queues"]["GetExcitementLevel"] = [x]
        yield c
    for s in SUMS:
        c = fill_case(r)
        c["queues"]["sum_hi"], c["queues"]["sum_lo"] = [double_words(s)[0]], [double_words(s)[1]]
        yield c


def fill_random(r):
    return fill_case(r)


# ---- UpdateRideListEntries
def upd_case(r, **kw):
    c = base(r, "upd", ["list"])
    n = r.randrange(0, 4)
    c["count"] = r.choice([0, 1, 2, 3, 4])
    nop = c["noparent"]
    pool = [r.randrange(4, 0x2800) for _ in range(3)]
    seq = []
    for _ in range(4):
        v = r.choice([nop, nop, r.choice(pool)])
        seq.append(v)
        if v != nop:
            seq.append(v)
    c["queues"]["GetUpgradeParent"] = seq
    c["parents"] = pool
    c["ta0"] = r.choice(["0", "obj2"])
    c["queues"]["GetDataData"] = [r.choice(["obj", "obj"]) for _ in range(4)]
    c["queues"]["GetData"] = [1]
    c.update(kw)
    return c


def upd_edges():
    r = random.Random(5222)
    yield upd_case(r, args=["0"])
    for cnt in range(5):
        for ta0 in ("0", "obj2"):
            c = upd_case(r, count=cnt, ta0=ta0)
            yield c
    c = upd_case(r, count=2, noparent=0)
    c["queues"]["GetUpgradeParent"] = [5, 5, 0, 7]
    c["parents"] = [5, 7]
    yield c
    c = upd_case(r, count=3, noparent=9)
    c["queues"]["GetUpgradeParent"] = [9, 9, 9, 0x27FF, 0x27FF]
    c["parents"] = [9, 0x27FF]
    yield c


def upd_random(r):
    return upd_case(r)


# ---- CreateAllKidsPanel
def create_case(r, **kw):
    c = base(r, "create", [r.choice(["thing", "0"])])
    q = c["queues"]
    q["Build"] = [r.choice(["win", "win", "0"])]
    q["GetWindow_list"] = [r.choice(["list", "list", "0"]), r.choice(["list", "list", "0"])]
    for i in range(6):
        q["GetWindow_b%d" % i] = ["b%d" % i]
    q["FindDrawObject"] = [0x70002000]
    q["FindData"] = [r.choice([-1, 0, 2, 9])]
    q["UIMain_GetString"] = [0x70003000 + 16 * i for i in range(6)]
    q["setAnimTimer"] = [r.choice([0, 1, 0x10000, 0xFFFF, 0xFFFFFFFF, 0x7FFF8000])]
    c.update(kw)
    return c


def create_edges():
    r = random.Random(5223)
    for build in ("win", "0"):
        for lists in (("list", "list"), ("0", "0"), ("list", "0")):
            for arg in ("thing", "0"):
                for fd in (-1, 0, 4):
                    c = create_case(r, args=[arg])
                    c["queues"]["Build"] = [build]
                    c["queues"]["GetWindow_list"] = list(lists)
                    c["queues"]["FindData"] = [fd]
                    yield c
    for anim in (0, 1, 0x10000, 0xFFFF, 0x8000, 0xFFFFFFFF):
        c = create_case(r)
        c["queues"]["Build"] = ["win"]
        c["queues"]["setAnimTimer"] = [anim]
        yield c


def create_random(r):
    return create_case(r)


TESTS = [
    FnTest("CreateAllKidsPanel", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("AllKidsPanel_KidAdded", ppc, host, edge=kid_edges, rnd=kid_random),
    FnTest("AddItemToAllRidesList", ppc, host, edge=add_edges, rnd=add_random),
    FnTest("FillAllSideshowsListCallback", ppc, host, edge=fill_edges, rnd=fill_random),
    FnTest("UpdateRideListEntries", ppc, host, edge=upd_edges, rnd=upd_random),
]
