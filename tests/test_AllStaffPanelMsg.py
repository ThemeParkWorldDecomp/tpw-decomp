"""Unit v51004: the all-staff panel's message function, the finance panel's message function and UIStatusPanel::RequestMaintenance,
against libtpw_unit_AllStaffPanelMsg.dylib (tests/unit_AllStaffPanelMsg.cpp), main app, src/MainWindowCallback_CreateAllItemsPanel_AllStaffPanelMsg.cpp:
  AllStaffPanelMsg 0x10198ac4, UIStatusPanel::RequestMaintenance 0x101932a0, FinancePanelPanelMsg 0x1019f434

Every callee outside the unit is a fake on both sides that logs its arguments as raw integers (pointers the unit never dereferences are
random words in both memories). The panel's second table (Mac +0x20) is a block of descriptor words 0x5000 + slot on the PowerPC side and a
fake slot table on the host (tests/fake_status_panel_table.h); the glue stand-in and the host slot both log the slot reached.

Compared: the answer, the three message globals and the event log. No fused operations. Not tested: the thing pointer of
RequestMaintenance is only tested as non-NULL (the original calls through it unconditionally).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AllStaffPanelMsg"
MASK = 0xFFFFFFFF

FN_STAFF, FN_REQ, FN_FIN = 0x10198ac4, 0x101932a0, 0x1019f434
SIZES = {FN_STAFF: 596, FN_REQ: 88, FN_FIN: 368}
OWN = set(SIZES)

ADDR = dict(gStaffSort=0x102a9088, gStaffPanel=0x103947f0, gFinanceGraphPanel=0x10394904, gStaffRoomPanelThing=0x103947a4,
            gStaffStatusPanel=0x10395b1c)
HOST_LAYOUT = []

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def s32(v):
    return T.s32(v)


def mk(name, addr, nargs, ret=False, fn=None):
    def stub(e):
        L = e.cl
        L.rec(name, tuple(s32(e.r(3 + i)) for i in range(nargs)))
        if ret:
            e.set_r(3, L.ret(name) & MASK)
    STUBS.internal(addr, stub)


mk("GetString", 0x10180468, 1, True)
mk("SetTitleText", 0x10180794, 3)
mk("RemoveAllStaffPanel", 0x10199e20, 0)
mk("UpdateListEntries", 0x10198944, 0)
mk("GetWindow", 0x101dbe40, 2, True)
mk("CancelAdvisor", 0x1018227c, 1)
mk("CreateAllItemsPanel", 0x101983c4, 2)
mk("CreateParkStatusPanel", 0x101aca78, 0)
mk("CreateAllKidsPanel", 0x101952bc, 1)
mk("DefaultPanelMsg", 0x1018493c, 4, True)
mk("RemoveFinance", 0x101a0c8c, 0)
mk("CreateLoans", 0x101a4de4, 0)
mk("CreateStaffCosts", 0x101be440, 0)
mk("CreateEntryPrice", 0x1019d660, 0)


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (s32(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), (L.ret("GetData") & MASK).to_bytes(4, "big"))


def stub_show(e):
    e.cl.rec("Show", (1 if e.r(3) == ADDR["gStaffStatusPanel"] else 0, s32(e.r(4))))


def stub_thingid(e):
    L = e.cl
    L.rec("GetThingId", (s32(e.r(4)),))
    e.write(e.r(3), ((L.ret("GetThingId") & 0xFFFF) << 16).to_bytes(4, "big"))


def stub_zoom(e):
    e.cl.rec("Zoom", ((e.r(3) >> 16) & 0xFFFF,))


def stub_objreq(e):
    e.cl.rec("ObjRequest", (e.u32(e.r(3)), s32(e.r(4))))


def stub_glue(e):
    assert e.r(3) == CUR["panel"], "glue got the wrong panel"
    e.cl.rec("Slot", ((e.r(12) - 0x5000) & MASK,))


STUBS.internal(0x101d41f0, stub_getdata)
STUBS.internal(0x101c54f0, stub_show)
STUBS.internal(0x1012e1f8, stub_thingid)
STUBS.internal(0x10181b00, stub_zoom)
STUBS.internal(0x1010beb8, stub_objreq)
STUBS.internal(0x1023baa8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def rd_word(self, a):
        return int.from_bytes(self.read(a, 4), self.order)


class Ppc(Side):
    order = "big"

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

    def make_ops(self):
        vt = self.alloc(0x40)
        self.words(vt, [0x5000 + (4 * k - 8) // 4 if k >= 2 else 0 for k in range(16)])
        return vt, vt

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ash_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ash_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make_ops(self):
        return 0, self.lib.tpw_ash_ops()

    def call(self, fn, args):
        if fn == FN_REQ:
            return self.lib.tpw_ash_request(args[0], args[1] & MASK) & MASK
        f = self.lib.tpw_ash_staff if fn == FN_STAFF else self.lib.tpw_ash_finance
        return f(args[0] & MASK, s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ash_"
    lib.tpw_ash_addr.restype, lib.tpw_ash_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ash_alloc.restype, lib.tpw_ash_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ash_alloc(16)   # maps the arena before any case: the list's words are offsets from its base (unit v5L31)
    lib.tpw_ash_ops.restype, lib.tpw_ash_ops.argtypes = C.c_uint32, []
    lib.tpw_ash_layout.argtypes = [C.POINTER(C.c_int64)]
    for nm in ("staff", "finance"):
        f = getattr(lib, p + nm)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ash_request.restype, lib.tpw_ash_request.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_ash_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("gStaffSort"), [case["sort"]])
    s.words(s.g("gStaffPanel"), [case["staffpanel"]])
    s.words(s.g("gFinanceGraphPanel"), [case["fingraph"]])
    if case["op"] == "req":
        thing = s.alloc(16)
        s.words(thing, [case["marker"]])
        panel = s.alloc(0x30)
        vt, ops_word = s.make_ops()
        s.words(panel + 0x20, [ops_word])
        s.words(s.g("gStaffRoomPanelThing"), [thing & MASK])
        if s.order == "little":
            s.write(s.g("gStaffRoomPanelThing") + 4, (thing >> 32).to_bytes(4, "little"))
        case["panel"] = panel
        CUR["panel"] = panel


def execute(s, case):
    if case["op"] == "req":
        s.call(FN_REQ, [case["panel"], case["on"]])   # void: r3 is not an answer
        return 0
    return s.call(FN_STAFF if case["op"] == "staff" else FN_FIN, [case["window"]] + case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "sort": s.rd_word(s.g("gStaffSort")), "sp": s.rd_word(s.g("gStaffPanel")),
            "fg": s.rd_word(s.g("gFinanceGraphPanel"))}


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


def rw(r):
    return r.choice([0, 1, 0x1000, r.getrandbits(32) & ~3, 0xFFFFFFFF])


def base(r, op):
    q = {k: [rw(r)] for k in ("GetString", "GetWindow", "GetData", "GetThingId", "DefaultPanelMsg")}
    q["GetWindow"] = [r.choice([0, 0x1000, r.getrandbits(32) | 1])]
    return {"op": op, "queues": q, "sort": r.getrandbits(32), "staffpanel": r.choice([0, r.getrandbits(32)]),
            "fingraph": r.choice([0, r.getrandbits(32)]), "window": r.getrandbits(32) | 1, "marker": r.getrandbits(32),
            "args": [0, 0, 0]}


STAFF_MSGS = [0x100, 5, 0x10, 0x15, 0x14, 0x400, 0x402, 0x406, 0, 1, 4, 6, 0xf, 0x11, 0x13, 0x16, 0xff, 0x101, 0x3ff, 0x401, 0x403,
              0x405, 0x407, -1]
STAFF_A = [0x32e, 0x32f, 0x330, -2, -1, 0, 0x32d, 0x331, 0x80083, 0x80082, 0x80084, 0x70083, 0x180083, 0x321, 0x7fffffff, -3]
FIN_MSGS = [0x15, 5, 0x14, 0x100, 0x1e, 0, 4, 6, 0x13, 0x16, 0x1f, 0xff, 0x101, -1]
FIN_A = [0x1227f, 0x12280, 0x12281, 0x12282, 0x1227e, -1, -2, -3, 0, 1, 0x7fffffff, -0x80000000]


def staff_case(r, m=None, a=None, b=None):
    c = base(r, "staff")
    c["args"] = [r.choice(STAFF_MSGS) if m is None else m, r.choice(STAFF_A) if a is None else a,
                 r.choice([-1, 0, 1, 7, r.getrandbits(32) - (1 << 31)]) if b is None else b]
    return c


def fin_case(r, m=None, a=None, b=None):
    c = base(r, "fin")
    c["args"] = [r.choice(FIN_MSGS) if m is None else m, r.choice(FIN_A) if a is None else a, r.getrandbits(32) - (1 << 31) if b is None else b]
    return c


def req_case(r, on=None):
    c = base(r, "req")
    c["on"] = r.choice([0, 1, 2, 0x80, 0xff]) if on is None else on
    return c


def staff_edges():
    r = random.Random(11)
    for m in STAFF_MSGS:
        for a in STAFF_A:
            yield staff_case(r, m, a)
    for m in (0x400, 0x402):
        for b in (-1, 0, 5):
            for gw in (0, 0x1000):
                c = staff_case(r, m, 0x321, b)
                c["queues"]["GetWindow"] = [gw]
                yield c
    for b in (-1, 0, 1, 0x7fffffff, -0x80000000):
        yield staff_case(r, 0x406, 0, b)


def fin_edges():
    r = random.Random(12)
    for m in FIN_MSGS:
        for a in FIN_A:
            yield fin_case(r, m, a)


def req_edges():
    r = random.Random(13)
    for on in (0, 1, 2, 0x80, 0xff):
        yield req_case(r, on)


TESTS = [
    FnTest("AllStaffPanelMsg", ppc, host, edge=staff_edges, rnd=staff_case),
    FnTest("UIStatusPanel::RequestMaintenance", ppc, host, edge=req_edges, rnd=req_case),
    FnTest("FinancePanelPanelMsg", ppc, host, edge=fin_edges, rnd=fin_case),
]
