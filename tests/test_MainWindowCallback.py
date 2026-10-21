"""Unit v5002, the all-items panel's message functions and the all-features panel, against libtpw_unit_MainWindowCallback.dylib
(tests/unit_MainWindowCallback.cpp), main app ("SimTheme Park"), src/game/ui/ui_allItemsPanel.cpp:
  MainWindowCallback 0x10197c2c, ListTemplateCallback 0x10197dac, CreateAllFeaturesPanel 0x101979c0, ActualListCallback 0x10198064

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the panel makers, the
window and list control members, the object's takings, the history sum) are fakes on both sides that record their arguments and answer from
the case's queues. ListTemplateCallback reaches CreateAllFeaturesPanel, which runs as the original on the PowerPC side.

The world of a case: the panel, list-type, list, sort-column and column-font globals, a window (with the list's item count at its
Mac offset 0x150, the host's own offset) and a set of 0x200-byte objects the log names by label. What is compared: the answer, those
globals and the event log (the items ChangeItem is given included).

No fused operations. The double to integer helper at 0x10237cd8 (the customer sum) is modelled as a truncation on the PowerPC side; sums
stay in 0 to 2^32 - 1. Not tested: a list whose GetData answers no object when the callback reads the object's history (the original reads
the stack), the Mac's garbage low half of the thing id passed to UIMain_ZoomToThing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MainWindowCallback"
MASK = 0xFFFFFFFF

F_MAIN, F_LIST, F_FEAT, F_ACT = 0x10197c2c, 0x10197dac, 0x101979c0, 0x10198064
SIZES = {F_MAIN: 320, F_LIST: 632, F_FEAT: 568, F_ACT: 448}
OWN = set(SIZES)

ADDR = dict(panel=0x103947e0, ltype=0x103947dc, list=0x103947d8, sort0=0x102a8cf8, sort1=0x102a8cfc, sort2=0x102a8d00, sort3=0x102a8d04,
            font=0x102a8cf4, script=0x102a8c3c, fn_actual=0x10267a38, fn_lc=0x10267a30, fn_post=0x10267a28, fn_fill=0x10267a48)
EXTRA = ["script", "fn_actual", "fn_lc", "fn_post", "fn_fill"]
HOST_LAYOUT = [524]      # the host's offset of InterfaceListControl2::mItemCount (the Mac's is 0x150)

OBJS = ["win", "panel", "cont", "w190", "lst", "b10", "b11", "draw", "thing", "wa", "s4e", "s4f", "s50", "s51", "s62", "s63",
        "ob0", "ob1", "ob2", "ob3", "ob4"]
OBJ_SIZE = 0x400


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, spec, retkey=None, retarg=None):
    """A recording fake: spec has one letter per argument register from r3 (L label, i s32, h s16, b u8); a pointer or integer answer from
    the queue retkey (or retkey:argument number retarg)."""
    def stub(e):
        L = e.cl
        vals = []
        for k, c in enumerate(spec):
            v = e.r(3 + k)
            vals.append(L.lab(v) if c == "L" else s32(v) if c == "i" else s16(v) if c == "h" else v & 0xFF)
        L.rec(name, tuple(vals))
        if retkey is not None:
            key = retkey if retarg is None else "%s:%d" % (retkey, vals[retarg])
            e.set_r(3, L.ret(key) & MASK)
    return stub


def st_getdata(e):
    L = e.cl
    pos = s32(e.r(4))
    L.rec("GetData", (L.lab(e.r(3)), pos))
    e.write(e.r(5), struct.pack(">I", L.ret("GetData:%d" % pos) & MASK))
    e.set_r(3, 1)


def st_getthingid(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


def st_zoom(e):
    e.cl.rec("ZoomToThing", ((e.r(3) >> 16) & 0xFFFF,))


def st_finddraw(e):
    L = e.cl
    raw = e.read(e.r(3), 16).split(b"\0")[0][:8]
    L.rec("FindDrawObject", tuple([len(raw)] + list(raw) + [0] * (8 - len(raw))))
    e.set_r(3, L.ret("FindDrawObject") & MASK)


def st_change(e):
    L = e.cl
    words = [s32(w) for w in struct.unpack(">5I", e.read(e.r(5), 20))]
    L.rec("ChangeItem", tuple([L.lab(e.r(3)), s32(e.r(4))] + words + [s32(e.r(6)), s32(e.r(7)), s32(e.r(8))]))


def st_sum(e):
    L = e.cl
    L.rec("GetSumOfLast", (L.lab((e.r(3) - 0x1a4) & MASK), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">d", float(L.ret("GetSumOfLast"))))
    e.set_r(3, 1)


def st_conv(e):
    e.set_r(3, int(e.f(1)) & MASK)


def st_findata(e):
    L = e.cl
    L.rec("FindData", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("FindData") & MASK)


def st_sort(e):
    L = e.cl
    L.rec("SetSort", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))


def st_ctype(e):
    L = e.cl
    L.rec("SetColumnType", (L.lab(e.r(3)), s16(e.r(4)), s32(e.r(5))))


STUBS.internal(0x1019862c, mk("RemoveAllItemsPanel", ""))
STUBS.internal(0x101aca78, mk("CreateParkStatusPanel", ""))
STUBS.internal(0x101952bc, mk("CreateAllKidsPanel", "L"))
STUBS.internal(0x1019762c, mk("CreateAllRidesPanel", "L"))
STUBS.internal(0x101972ac, mk("CreateAllShopsPanel", "L"))
STUBS.internal(0x10196ed0, mk("CreateAllShowsPanel", "L"))
STUBS.internal(0x10199630, mk("CreateAllStaffPanel", "iL"))
STUBS.internal(0x1018227c, mk("CancelAdvisorMessage", "i"))
STUBS.internal(0x1018493c, mk("DefaultPanelMsg", "Liii", "DefaultPanelMsg"))
STUBS.internal(0x10180468, mk("GetString", "i", "GetString", 0))
STUBS.internal(0x10180794, mk("SetTitleText", "LiL"))
STUBS.internal(0x10181e2c, mk("GotoStatusPanelForThing", "L"))
STUBS.internal(0x10181b00, st_zoom)
STUBS.internal(0x1012e1f8, st_getthingid)
STUBS.internal(0x101c9518, mk("SendMessage", "Liii"))
STUBS.internal(0x101f7db0, mk("IterateThingList", "iL"))
STUBS.internal(0x1019600c, mk("UpdateRideListEntries", "L"))
STUBS.internal(0x10196208, mk("UpdateSideshowListEntries", "L"))
STUBS.internal(0x101818e4, mk("MAKECOLUMNBUTTON", "LLiiiii"))
STUBS.internal(0x101dbe40, mk("GetWindow", "Li", "GetWindow", 1))
STUBS.internal(0x101deb78, mk("Build", "LLL", "Build"))
STUBS.internal(0x101dcba8, mk("SetCallbackFunction", "LL"))
STUBS.internal(0x101de318, mk("CallDefaultWindowHandler", "Liii", "Default"))
STUBS.internal(0x101dd5e0, mk("setAnimTimer", "Lii", "setAnimTimer"))
STUBS.internal(0x101dd6b8, mk("startAnimTimer", "Li"))
STUBS.internal(0x101d41f0, st_getdata)
STUBS.internal(0x101d2864, mk("SetColumnFormatsAll", "LLL"))
STUBS.internal(0x101d2614, st_ctype)
STUBS.internal(0x101d1eb4, mk("SetSelectionObject", "LL"))
STUBS.internal(0x101d2220, mk("UpdateDisplay", "L"))
STUBS.internal(0x101d3660, st_change)
STUBS.internal(0x101d4384, st_findata)
STUBS.internal(0x101d4db8, st_sort)
STUBS.internal(0x101d4c64, mk("SetCurrent", "Li"))
STUBS.internal(0x101cd2d4, st_finddraw)
STUBS.internal(0x1010f8d4, mk("GetProfitability", "L", "GetProfitability"))
STUBS.internal(0x1010f894, mk("GetTotalRevenue", "L", "GetTotalRevenue"))
STUBS.internal(0x1010f9d0, mk("GetCustomerSatisfaction", "L", "GetCustomerSatisfaction"))
STUBS.internal(0x100f1000, st_sum)
STUBS.internal(0x10237cd8, st_conv)


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

    def setptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def getptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_objects(self):
        self.obj = {n: self.alloc(OBJ_SIZE) for n in OBJS}
        self.rev = {v: n for n, v in self.obj.items()}

    def resolve(self, v):
        if v is None:
            return 0
        return self.obj[v] if isinstance(v, str) else v

    def name(self, v):
        return None if v == 0 else self.rev.get(v, v)

    def queues(self, case):
        return {k: [self.resolve(x) for x in vs] for k, vs in case["queues"].items()}


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    count_off = 0x150

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        fixed = {self.obj[n]: i + 1 for i, n in enumerate(OBJS)}
        for j, n in enumerate(EXTRA):
            fixed[ADDR[n]] = len(OBJS) + 1 + j
        self.L = e.cl = CL.PpcLog(fixed, self.queues(case))
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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [x & MASK for x in args])
        return s32(r3)


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.count_off = lib.count_off
        self.make_objects()
        fixed = {i + 1: self.obj[n] for i, n in enumerate(OBJS)}
        for j, n in enumerate(EXTRA):
            fixed[len(OBJS) + 1 + j] = self.g(n)
        CL.host_begin(lib, fixed, self.queues(case))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5002_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v5002_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        fn = {F_MAIN: self.lib.tpw_v5002_main, F_LIST: self.lib.tpw_v5002_list, F_ACT: self.lib.tpw_v5002_act}[entry]
        return fn(args[0], s32(args[1]), s32(args[2]), s32(args[3]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5002_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("main", "list", "act"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    f = getattr(lib, p + "feat")
    f.restype, f.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.count_off = out[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = case["g"]
    s.setptr(s.g("panel"), s.resolve(g["panel"]))
    s.words(s.g("ltype"), [g["ltype"]])
    s.setptr(s.g("list"), s.resolve(g["list"]))
    for i in range(4):
        s.words(s.g("sort%d" % i), [g["sorts"][i]])
    s.words(s.g("font"), [g["font"]])
    s.words(s.obj["win"] + s.count_off, [case["count"]])


def execute(s, case):
    op = case["op"]
    if op == "feat":
        if isinstance(s, Ppc):
            T.call_ppc(s.e, F_FEAT, [s.resolve(case["thing"])])
        else:
            s.lib.tpw_v5002_feat(s.resolve(case["thing"]))
        return 0
    m, a, b = case["args"]
    entry = {"main": F_MAIN, "list": F_LIST, "act": F_ACT}[op]
    return s.call(entry, [s.obj["win"], m, a, b])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "panel": s.name(s.getptr(s.g("panel"))), "ltype": s.rd_word(s.g("ltype")), "list": s.name(s.getptr(s.g("list"))),
            "sorts": [s.rd_word(s.g("sort%d" % i)) for i in range(4)], "font": s.rd_word(s.g("font"))}


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


def rint(r):
    return r.choice([0, 1, -1, 2, 100, -100, r.randrange(-2 ** 31, 2 ** 31)])


def base(r, op):
    q = {"DefaultPanelMsg": [rint(r)], "Default": [rint(r)]}
    for i in (0x4e, 0x4f, 0x50, 0x51, 0x62, 0x63):
        q["GetString:%d" % i] = [r.choice(["s%x" % i, None])]
    for i in (0x77, 0x78):
        q["GetWindow:%d" % i] = [r.choice(["wa", "wa", None])]
    q["GetWindow:%d" % 0x12c4b8] = [r.choice(["cont", "cont", None]), r.choice(["cont", None])]
    q["GetWindow:400"] = [r.choice([None, "w190"]) for _ in range(3)]
    q["GetWindow:16"] = [r.choice(["b10", None])]
    q["GetWindow:17"] = [r.choice(["b11", None])]
    for i in range(5):
        q["GetData:%d" % i] = [("ob%d" % i) if op == "act" else r.choice(["thing", None, "ob0"])]
    q["GetThingId"] = [r.choice([0, 1, 0xFFFF, r.randrange(0x10000)])]
    q["FindDrawObject"] = [r.choice(["draw", None])]
    q["Build"] = [r.choice(["lst", "lst", None])]
    q["FindData"] = [r.choice([-1, 0, 3, 2 ** 31 - 1, -2 ** 31, r.randrange(-5, 50)])]
    q["setAnimTimer"] = [r.choice([0, 1, -1, 0x10000, 5])]
    q["GetSumOfLast"] = [r.choice([0, 1, 2 ** 32 - 1, 2 ** 31, r.randrange(2 ** 32)]) for _ in range(5)]
    q["GetProfitability"] = [rint(r) for _ in range(5)]
    q["GetTotalRevenue"] = [rint(r) for _ in range(5)]
    q["GetCustomerSatisfaction"] = [r.choice([0, 100, 99, 1, -1, 50, 2 ** 21, 2 ** 22 + 3, 2 ** 31 - 1, -2 ** 31, rint(r)]) for _ in range(5)]
    g = {"panel": r.choice(["panel", None]), "ltype": r.choice([0, 1, 2, 3, 4, -1, 5]), "list": r.choice(["lst", None]),
         "sorts": [r.randrange(-3, 10) for _ in range(4)], "font": r.randrange(0, 20)}
    return {"op": op, "queues": q, "g": g, "count": r.choice([0, 1, 2, 3, 5, -1, -3]), "thing": r.choice([None, "thing"]), "args": [0, 0, 0]}


# ---- MainWindowCallback
MAIN_M = [5, 0x14, 0x15, 0x13, 0x16, 4, 6, 0x100, 0, 0x101, -1, 0x7FFFFFFF]
MAIN_A = [0x12c4bf, 0x12c4c0, 0x12c4c1, 0x12c4c2, 0x12c4be, 0x12c4bc, 0x12c4be - 1, -2, -1, 0, -3, 0x7FFFFFFF, -0x80000000]


def main_case(r, m=None, a=None):
    c = base(r, "main")
    c["args"] = [r.choice(MAIN_M) if m is None else m, r.choice(MAIN_A) if a is None else a, rint(r)]
    return c


def main_edges():
    r = random.Random(11)
    for m in MAIN_M:
        yield main_case(r, m, 0)
    for a in MAIN_A:
        yield main_case(r, 0x100, a)
        yield main_case(r, 0x14, a)


def main_random(r):
    return main_case(r, r.choice([0x100, 0x100, 5, 0x14, None]))


# ---- ListTemplateCallback
LIST_M = [0x401, 0x400, 0x402, 0x403, 0x406, 0x405, 0x407, 0x101, 0x100, 0x102, 0, -1, 0x7FFFFFFF]
LIST_A = [0x12c4b8, 0x12c4b9, 0x12c4ba, 0x12c4bb, 0x12c4bc, 0x12c4bd, 0x12c4be, 0x12c4bf, 0x12c4c0, 0x77, 0x78, 0, -1]


def list_case(r, m=None, a=None, b=None):
    c = base(r, "list")
    m = r.choice(LIST_M) if m is None else m
    if a is None:
        a = r.choice([0x77, 0x78]) if m in (0x400, 0x402) else r.choice(LIST_A)
    if b is None:
        b = r.choice([1, 1, 0, -1, 2, 3, 4]) if m in (0x400, 0x402, 0x101) else rint(r)
    c["args"] = [m, a, b]
    return c


def list_edges():
    r = random.Random(12)
    for m in LIST_M:
        yield list_case(r, m)
    for a in LIST_A:
        for b in (1, 0, -1, 2):
            yield list_case(r, 0x101, a, b)
    for m in (0x400, 0x402):
        for b in (-1, 0, 1, 2, 3, 4):
            for a in (0x77, 0x78):
                yield list_case(r, m, a, b)
    for lt in (-1, 0, 1, 2, 3, 4):
        c = list_case(r, 0x406)
        c["g"]["ltype"] = lt
        yield c


def list_random(r):
    return list_case(r, r.choice([0x101, 0x101, 0x400, 0x402, 0x406, None]))


# ---- CreateAllFeaturesPanel
def feat_edges():
    r = random.Random(13)
    for thing in (None, "thing"):
        for w in ([None, None, None], ["w190", None, None], [None, "w190", None], [None, None, "w190"]):
            for fd in (-1, 0, 5):
                c = base(r, "feat")
                c["thing"] = thing
                c["queues"]["GetWindow:400"] = w
                c["queues"]["FindData"] = [fd]
                yield c


def feat_random(r):
    c = base(r, "feat")
    return c


# ---- ActualListCallback
ACT_M = [0x15, 0x10, 0x14, 0x16, 0, 5, 0x11, -1]
ACT_A = [0x80083, 0x80084, 0x80082, 0x83, 0x180083, 0, -1]


def act_case(r, m=None, a=None, lt=None, count=None):
    c = base(r, "act")
    c["args"] = [r.choice(ACT_M) if m is None else m, r.choice(ACT_A) if a is None else a, rint(r)]
    if lt is not None:
        c["g"]["ltype"] = lt
    if count is not None:
        c["count"] = count
    return c


def act_edges():
    r = random.Random(14)
    for m in ACT_M:
        yield act_case(r, m, 0x80083)
    for a in ACT_A:
        yield act_case(r, 0x10, a, 0, 2)
        yield act_case(r, 0x10, a, 1, 2)
    for lt in (-1, 0, 1, 2, 3, 4):
        yield act_case(r, 0x10, 0x80083, lt, 2)
    for count in (-3, -1, 0, 1, 2, 3, 5):
        yield act_case(r, 0x10, 0x80083, 1, count)
    for ret in (0, 1, -1, 0x10000, 5):
        c = act_case(r, 0x15, 0)
        c["queues"]["setAnimTimer"] = [ret]
        yield c
    for sat in (0, 1, 99, 100, 101, -1, -100, 2 ** 21, 2 ** 22, 2 ** 22 + 3, 2 ** 31 - 1, -2 ** 31, 1023, 1024, 12345678):
        c = act_case(r, 0x10, 0x80083, 1, 2)
        c["queues"]["GetCustomerSatisfaction"] = [sat, sat]
        yield c


def act_random(r):
    if r.random() < 0.7:
        return act_case(r, 0x10, 0x80083, r.choice([0, 1, 1, 2, 3, 4, -1]))
    return act_case(r)


TESTS = [
    FnTest("MainWindowCallback", ppc, host, edge=main_edges, rnd=main_random),
    FnTest("ListTemplateCallback", ppc, host, edge=list_edges, rnd=list_random),
    FnTest("CreateAllFeaturesPanel", ppc, host, edge=feat_edges, rnd=feat_random),
    FnTest("ActualListCallback", ppc, host, edge=act_edges, rnd=act_random),
]
