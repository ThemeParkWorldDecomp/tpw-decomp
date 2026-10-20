"""Unit v5066, the all-items panel builder, its entry handler and the staff list functions, against libtpw_unit_CreateAllItemsPanel.dylib
(tests/unit_CreateAllItemsPanel.cpp), main app ("SimTheme Park"), src/MainWindowCallback_CreateAllItemsPanel.cpp:
  CreateAllItemsPanel 0x101983c4, AllItemsLCEntryHandler 0x10198264, FillAllStaffListCallback 0x10198690,
  FillAllStaffList 0x101987fc, UpdateListEntries 0x10198944

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit (the window and list members, the staff members, the panel builders, the string table, the iterator, the asserts) is a
fake on both sides that records its arguments and answers from the case's queues; pointers are the case's eight objects (labels 1 to 8,
a queue value k hands back object k, 0 is NULL).

The world of a case: the globals (the list type, the panels, the game window, the staff list control, the text table, the upgrade-parent
id), the thing array's entries for a small pool of ids, and the eight objects (a list's item count, a staff member's happiness float).
What is compared: the answer, the log, and the globals the functions write.

No fused operations; the happiness floats stay below 2^31 (fctiwz). Not tested: NULL windows or lists (the original calls through
them), thing ids past the pool (the table has 0x2800 entries and the original does not check), NaN happiness.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateAllItemsPanel"
MASK = 0xFFFFFFFF

FN_CREATE, FN_HANDLER, FN_CALLBACK, FN_FILL, FN_UPDATE = 0x101983C4, 0x10198264, 0x10198690, 0x101987FC, 0x10198944
SIZES = {FN_CREATE: 556, FN_HANDLER: 284, FN_CALLBACK: 308, FN_FILL: 276, FN_UPDATE: 344}
OWN = set(SIZES)

ADDR = dict(gAllItemsListType=0x103947DC, gAllItemsPanel=0x103947E0, gGameWindow=0x103949E4, gStaffPanel=0x103947F0,
            gStaffType=0x103947EC, gStaffListControl=0x103947E8, gAllItemsNoParent=0x102A8D0C, gText10=0x1037FF18 + 0x574,
            gThingArray=0x1034CBFC)
TV_MAIN, TV_LIST, TV_FILLCB, SCRIPT = 0x10267A20, 0x10267A18, 0x10267A90, 0x102A8884
PPC_ENTRY = 20
HOST_LAYOUT = [524, 500, 48, 8]       # mItemCount, mHappiness (host offsets), the thing array entry size, an LCWord
OFF_COUNT, OFF_HAPPY = 0x150, 0x1F4       # the Mac's
NOBJ = 8
POOL = [0, 1, 2, 7, 0x27FF]
ASSERT_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def obj(q):
    q &= MASK
    return CUR["objs"][q - 1] if 1 <= q <= NOBJ else 0


def qret(e, key):
    return e.cl.ret(key)


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def lab(e, a):
    return e.cl.lab(a & MASK)


def stub(addr, name, args, ret=None):
    """A PowerPC fake: args are (kind) codes per register: L label, i s32, f flag-address handled by name."""
    def fn(e):
        vals = []
        for i, k in enumerate(args):
            r = e.r(3 + i)
            vals.append(lab(e, r) if k == "L" else s32(r))
        rec(e, name, *vals)
        if ret == "obj":
            e.set_r(3, obj(qret(e, name)))
        elif ret == "i":
            e.set_r(3, qret(e, name) & MASK)
        else:
            e.set_r(3, 0)
    STUBS.internal(addr, fn)


STUBS.internal(0x10180514, lambda e: (rec(e, "CheckPanel", 0), e.set_r(3, 0)))
stub(0x101A57FC, "SetLastPanel", ["i", "i"])
stub(0x101DBE40, "GetWindow", ["L", "i"], "obj")
stub(0x101CC148, "SetCurrent", ["L", "i"])
stub(0x10196ED0, "CreateAllShows", ["L"])
stub(0x101972AC, "CreateAllShops", ["L"])
stub(0x1019762C, "CreateAllRides", ["L"])
stub(0x101979C0, "CreateAllFeatures", ["L"])
stub(0x10180468, "GetString", ["i"], "obj")
stub(0x10180794, "SetTitleText", ["L", "i", "L"])
stub(0x101805A0, "SetPanel", ["L"])
stub(0x101821CC, "Advisor", ["i"])
stub(0x101DA3AC, "SetFontColour", ["L", "i", "i", "i", "i"])
stub(0x10180B84, "GetObjectState", ["L", "i"], "i")
stub(0x1012686C, "GetSkill", ["L"], "i")
stub(0x101267CC, "GetState", ["L"], "i")
stub(0x10126F98, "GetName", ["L"], "obj")
stub(0x10127A78, "HasPatrolArea", ["L"], "i")
stub(0x10126FC8, "GetSalary", ["L"], "i")
stub(0x10180334, "ConvertMulti", ["L"], "obj")
stub(0x101D3CC8, "RemoveAll", ["L"])
stub(0x101D2220, "UpdateDisplay", ["L"])


def stub_build(e):
    rec(e, "Build", lab(e, e.r(3)), int(e.r(4) == SCRIPT), int(e.r(5) == TV_MAIN))
    e.set_r(3, obj(qret(e, "Build")))


def stub_setcb(e):
    rec(e, "SetCallback", lab(e, e.r(3)), int(e.r(4) == TV_LIST))
    e.set_r(3, 0)


def stub_parent(e):
    rec(e, "GetUpgradeParent", lab(e, e.r(4)))
    e.write(e.r(3), struct.pack(">H", qret(e, "GetUpgradeParent") & 0xFFFF))


def stub_assert(e):
    t = e.cstr(e.r(4))
    t = t.decode("latin-1") if isinstance(t, bytes) else t
    rec(e, "Assert", s32(e.r(3) & 0xFF), int(t == ASSERT_TEXT))


def stub_colour(e):
    rec(e, "TextColour", s32(e.r(4)))
    e.write(e.r(3), struct.pack(">I", qret(e, "TextColour") & MASK))


def stub_constring(e):
    rec(e, "GetConstString", lab(e, e.r(3)), e.r(4) & MASK)
    e.set_r(3, obj(qret(e, "GetConstString")))


def stub_iterate(e):
    rec(e, "Iterate", s32(e.r(3)), int(e.r(4) == TV_FILLCB))
    e.set_r(3, 0)


def stub_additem(e):
    it = e.r(4)
    w = [e.u32(it + 4 * i) for i in range(5)]
    rec(e, "AddItem", lab(e, e.r(3)), lab(e, w[0]), lab(e, w[1]), s32(w[2]), s32(w[3]), s32(w[4]), lab(e, e.r(5)), s32(e.r(6)), s32(e.r(7)))
    e.set_r(3, 0)


def stub_changeitem(e):
    it = e.r(5)
    w = [e.u32(it + 4 * i) for i in range(5)]
    rec(e, "ChangeItem", lab(e, e.r(3)), s32(e.r(4)), s32(w[0]), lab(e, w[1]), s32(w[2]), s32(w[3]), s32(w[4]), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)))


def stub_getdata(e):
    rec(e, "GetData", lab(e, e.r(3)), s32(e.r(4)))
    e.write(e.r(5), struct.pack(">I", obj(qret(e, "GetData"))))
    e.set_r(3, 0)


def stub_calldefault(e):
    b = lab(e, e.r(6))
    rec(e, "CallDefault", lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), b, s32(e.r(6)) if b == -1 else 0)
    e.set_r(3, qret(e, "CallDefault") & MASK)


STUBS.internal(0x101DE318, stub_calldefault)
STUBS.internal(0x101DEB78, stub_build)
STUBS.internal(0x101DCBA8, stub_setcb)
STUBS.internal(0x1010DA64, stub_parent)
STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(0x10180FC0, stub_colour)
STUBS.internal(0x101F7DB0, stub_iterate)
STUBS.internal(0x101D3000, stub_additem)
STUBS.internal(0x101D3660, stub_changeitem)
STUBS.internal(0x101D41F0, stub_getdata)
STUBS.imp("GetConstString__8TbITableCFUl", stub_constring)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def obj(self, k):
        return self.objs[k - 1] if k else 0

    def dataword(self, address):
        """A list data word for an object: its address on the Mac, its offset from the arena base on the host."""
        return address

    def lab_of(self, v):
        if v == 0:
            return 0
        return self.objs.index(v) + 1 if v in self.objs else -1

    def entry_addr(self, i):
        return self.g("gThingArray") + i * self.entry


class Ppc(Side):
    fmt = ">I"
    off_count, off_happy, entry = OFF_COUNT, OFF_HAPPY, PPC_ENTRY

    def __init__(self, e, case):
        self.e = e
        self.objs = [e.alloc(0x400) for _ in range(NOBJ)]
        CUR["objs"] = self.objs
        self.L = e.cl = CL.PpcLog({self.objs[i]: i + 1 for i in range(NOBJ)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def hw(self, a, v):
        self.write(a, struct.pack(">H", v & 0xFFFF))

    def rd_hw(self, a):
        return struct.unpack(">H", self.read(a, 2))[0]

    def log(self):
        return self.L.log

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off_count, self.off_happy, self.entry = lib.layout[0], lib.layout[1], lib.layout[2]
        self.objs = [lib.tpw_cap_alloc(0x400) for _ in range(NOBJ)]
        arr = (C.c_uint64 * NOBJ)(*self.objs)
        lib.tpw_cap_setobjs(arr)
        CL.host_begin(lib, {i + 1: self.objs[i] for i in range(NOBJ)}, case["queues"])

    def done(self):
        pass

    def dataword(self, address):
        return self.lib.tpw_cap_offset(address)

    def g(self, name):
        return self.lib.tpw_cap_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def hw(self, a, v):
        self.write(a, struct.pack("<H", v & 0xFFFF))

    def rd_hw(self, a):
        return struct.unpack("<H", self.read(a, 2))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        L = self.lib
        a = [x & 0xFFFFFFFFFFFFFFFF for x in args]
        if fn == FN_CREATE:
            return L.tpw_cap_create(s32(a[0]), a[1]) and 0
        if fn == FN_HANDLER:
            return L.tpw_cap_handler(a[0], s32(a[1]), s32(a[2]), s32(a[3])) & MASK
        if fn == FN_CALLBACK:
            return L.tpw_cap_callback(a[0]) & MASK
        if fn == FN_FILL:
            return L.tpw_cap_fill(s32(a[0]), s32(a[1])) and 0
        return L.tpw_cap_update() and 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cap_addr.restype, lib.tpw_cap_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cap_alloc.restype, lib.tpw_cap_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cap_offset.restype, lib.tpw_cap_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_cap_setobjs.argtypes = [C.POINTER(C.c_uint64)]
    lib.tpw_cap_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cap_create.restype, lib.tpw_cap_create.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_cap_handler.restype, lib.tpw_cap_handler.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_cap_callback.restype, lib.tpw_cap_callback.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_cap_fill.restype, lib.tpw_cap_fill.argtypes = None, [C.c_int32, C.c_int32]
    lib.tpw_cap_update.restype, lib.tpw_cap_update.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_cap_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout == HOST_LAYOUT, lib.layout
    lib.bound = True


def build(s, case):
    for i in range(NOBJ):
        s.words(s.objs[i] + s.off_count, [case["count"][i]])
        s.words(s.objs[i] + s.off_happy, [case["happy"][i]])
    s.words(s.g("gAllItemsListType"), [case["listtype"]])
    s.ptr(s.g("gAllItemsPanel"), s.obj(case["panel0"]))
    s.ptr(s.g("gGameWindow"), s.obj(case["game"]))
    s.ptr(s.g("gStaffPanel"), s.obj(case["staffpanel"]))
    s.ptr(s.g("gStaffListControl"), s.obj(case["listctl0"]))
    s.ptr(s.g("gText10"), s.obj(case["text"]))
    s.hw(s.g("gAllItemsNoParent"), case["noparent"])
    s.words(s.g("gStaffType"), [case["stafftype0"]])
    for i, k in zip(POOL, case["table"]):
        s.ptr(s.entry_addr(i), s.obj(k))


def execute(s, case):
    op = case["op"]
    if op == "create":
        return s.call(FN_CREATE, [case["type"], s.obj(case["thing"])])
    if op == "handler":
        b = s.dataword(s.obj(case["bk"])) if case["bk"] else case["bnum"]
        return s.call(FN_HANDLER, [s.obj(case["window"]), case["message"], case["a"], b])
    if op == "callback":
        return s.call(FN_CALLBACK, [s.obj(case["thing"])])
    if op == "fill":
        return s.call(FN_FILL, [case["type"], case["flag"]])
    return s.call(FN_UPDATE, [])


def observe(s, case, answer):
    return {"answer": answer if case["op"] in ("handler", "callback") else 0, "log": s.log(),
            "listtype": s.rd_words(s.g("gAllItemsListType"), 1), "panel": s.lab_of(s.rd_ptr(s.g("gAllItemsPanel"))),
            "stafftype": s.rd_words(s.g("gStaffType"), 1), "listctl": s.lab_of(s.rd_ptr(s.g("gStaffListControl"))),
            "noparent": s.rd_hw(s.g("gAllItemsNoParent")),
            "table": [s.lab_of(s.rd_ptr(s.entry_addr(i))) for i in POOL]}


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

QKEYS = ["Build", "GetWindow", "GetString", "GetUpgradeParent", "GetObjectState", "TextColour", "CallDefault", "GetSkill", "GetState",
         "GetName", "HasPatrolArea", "GetSalary", "GetConstString", "ConvertMulti", "GetData"]
HAPPY = [fb(x) for x in (0, 0.5, -0.5, -1, 1, 254.9, 255, 255.9, 256, 257, 300, 511, 512.5, -255, -256, -257, 1e5, -1e5, 100.0, 99.99, 1e9, -1e9)]
SKILLS = [0, 1, 100, 99, 101, 1023, -1, -100, 0x7FFFFF, 0x400000, 0x200000, 0x7FFFFFFF, -0x80000000, 0x3FFFFF]
STATES = [0, 1, 2, 3, 4, 5, -1, 99]


def rnd32(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-2**31, 2**31), r.randrange(0, 1000)])


def base(r, op):
    q = {k: [r.randrange(0, NOBJ + 1)] for k in QKEYS}
    q["Build"] = [r.randrange(1, NOBJ + 1)]
    q["GetWindow"] = [r.randrange(1, NOBJ + 1) for _ in range(3)]
    return {"op": op, "queues": q, "count": [r.choice([0, 1, 2, 3, 5]) for _ in range(NOBJ)],
            "happy": [r.choice(HAPPY) if r.random() < 0.5 else fb(r.uniform(-700, 700)) for _ in range(NOBJ)],
            "listtype": r.choice([0, 1, 2, 3, 4, -1, -2, 9]), "panel0": r.randrange(0, 9), "game": r.randrange(1, 9),
            "staffpanel": r.randrange(0, 9), "listctl0": r.randrange(0, 9), "text": r.randrange(1, 9),
            "noparent": r.choice([0, 0, 1, 7, 0xFFFF, r.randrange(0, 65536)]), "stafftype0": rnd32(r),
            "table": [r.randrange(0, 9) for _ in POOL]}


# ---- CreateAllItemsPanel
def create_case(r, kind=None, lt=None):
    c = base(r, "create")
    c["type"] = r.choice([-1, -1, 0, 1, 2, 3, 4, -2, r.randrange(-5, 9)]) if kind is None else kind
    if lt is not None:
        c["listtype"] = lt
    c["thing"] = r.randrange(0, 9)
    c["queues"]["GetWindow"] = [r.randrange(1, 9) for _ in range(3)]
    return c


def create_edges():
    r = random.Random(5066)
    for kind in (-1, -2, 0, 1, 2, 3, 4, 5, 100, -100):
        for lt in (0, 1, 2, 3, 4, -1, -2, 9):
            yield create_case(r, kind, lt)


# ---- AllItemsLCEntryHandler
def handler_case(r, message=None, same=None):
    c = base(r, "handler")
    c["message"] = r.choice([0x403, 0x403, 0x403, 0x1E, 0x404, 0x402, 0, -1, r.randrange(-2000, 2000)]) if message is None else message
    c["window"] = r.randrange(1, 9)
    c["a"] = rnd32(r)
    c["bk"] = r.randrange(1, 9)
    c["bnum"] = rnd32(r)
    if c["message"] != 0x403 and r.random() < 0.3:
        c["bk"] = 0
    p1 = c["noparent"] if (same if same is not None else r.random() < 0.4) else r.choice(POOL)
    c["queues"]["GetUpgradeParent"] = [p1, r.choice(POOL), r.choice(POOL)]
    c["table"] = [r.randrange(0, 9) for _ in POOL]
    if r.random() < 0.3:
        c["table"][0] = 0
    return c


def handler_edges():
    r = random.Random(1)
    for message in (0x403, 0x1E, 0x404, 0x402, 0, -1):
        for same in (True, False):
            for _ in range(2):
                yield handler_case(r, message, same)
    for i in range(len(POOL)):
        c = handler_case(r, 0x403, False)
        c["queues"]["GetUpgradeParent"] = [c["noparent"] ^ 1, POOL[i]]
        c["table"][0] = i % 2
        yield c


# ---- FillAllStaffListCallback
def callback_case(r, happy=None, skill=None):
    c = base(r, "callback")
    c["thing"] = r.randrange(1, 9)
    if happy is not None:
        c["happy"] = [happy] * NOBJ
    c["listctl0"] = r.randrange(1, 9)
    q = c["queues"]
    q["GetSkill"] = [r.choice(SKILLS) if skill is None else skill]
    q["GetState"] = [r.choice(STATES), r.choice(STATES)]
    q["HasPatrolArea"] = [r.choice([0, 1, 5])]
    q["GetSalary"] = [rnd32(r)]
    return c


def callback_edges():
    r = random.Random(2)
    for h in HAPPY:
        yield callback_case(r, h)
    for s in SKILLS:
        yield callback_case(r, None, s)
    for st in (0, 1, 2):
        for pa in (0, 1):
            c = callback_case(r)
            c["queues"]["GetState"] = [st, st]
            c["queues"]["HasPatrolArea"] = [pa]
            yield c


# ---- FillAllStaffList
def fill_case(r, kind=None, flag=None):
    c = base(r, "fill")
    c["type"] = r.choice([0, 1, 2, 3, 4, 5, -1, r.randrange(-4, 9)]) if kind is None else kind
    c["flag"] = r.choice([0, 1, -1, 7]) if flag is None else flag
    c["staffpanel"] = r.randrange(1, 9)
    c["queues"]["GetWindow"] = [r.randrange(1, 9)]
    return c


def fill_edges():
    r = random.Random(3)
    for kind in (-2, -1, 0, 1, 2, 3, 4, 5, 6, 1000):
        for flag in (0, 1):
            yield fill_case(r, kind, flag)


# ---- UpdateListEntries
def update_case(r, panel=None, count=None):
    c = base(r, "update")
    c["staffpanel"] = r.randrange(0, 9) if panel is None else panel
    n = r.choice([0, 1, 2, 3, 4, -1]) if count is None else count
    c["queues"]["GetWindow"] = [r.randrange(1, 9)]
    c["count"] = [n] * NOBJ
    q = c["queues"]
    q["GetData"] = [r.randrange(1, 9) for _ in range(4)]
    q["GetState"] = [r.choice(STATES) for _ in range(4)]
    q["HasPatrolArea"] = [r.choice([0, 1, 5]) for _ in range(3)]
    q["GetSkill"] = [r.choice(SKILLS) for _ in range(4)]
    q["GetSalary"] = [rnd32(r) for _ in range(4)]
    return c


def update_edges():
    r = random.Random(4)
    yield update_case(r, 0, 3)
    for n in (-1, 0, 1, 2, 3, 5):
        yield update_case(r, 1, n)
    for h in HAPPY:
        c = update_case(r, 1, 2)
        c["happy"] = [h] * NOBJ
        yield c


TESTS = [
    FnTest("CreateAllItemsPanel", ppc, host, edge=create_edges, rnd=create_case),
    FnTest("AllItemsLCEntryHandler", ppc, host, edge=handler_edges, rnd=handler_case),
    FnTest("FillAllStaffListCallback", ppc, host, edge=callback_edges, rnd=callback_case),
    FnTest("FillAllStaffList", ppc, host, edge=fill_edges, rnd=fill_case),
    FnTest("UpdateListEntries", ppc, host, edge=update_edges, rnd=update_case),
]
