"""Unit v5071, UIMain_GotoStatusPanelForThing against libtpw_unit_UIMain_GotoStatusPanelForThing.dylib
(tests/unit_UIMain_GotoStatusPanelForThing.cpp), main app ("SimTheme Park"),
src/ui_main_kbcapture_UIMain_ShowRidePreview_UIMain_GotoStatusPanelForThing.cpp:
  UIMain_GotoStatusPanelForThing 0x10181e2c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the nine panels' Show,
CObject::GetDBInfo, GetUpgradeParent, UpgradesWhat) are fakes on both sides that record their arguments (pointers as labels) and answer
from the case (the ride record's address sits in the thing block at 0x40; the two upgrade ids come from queues).

The world of a case: the thing block (kind byte at 2, flags halfword at 0x2e), the ride record (words at 0x288 and 0x7a8), the two halfword
globals, the first eight rows of the thing array. What is compared: the event log and the assertion list.

No fused operations. Not tested: nothing excluded except a thing whose kind is 3 and whose record is missing (the original reads it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMain_GotoStatusPanelForThing"
MASK = 0xFFFFFFFF

FN = 0x10181E2C
SIZES = {FN: 544}
OWN = set(SIZES)

CALLEES = {
    0x101C665C: "UpgradeShow", 0x101BC0BC: "RideShow", 0x101BDEE0: "ShopShow", 0x101C2A38: "ShowShow",
    0x1019AC5C: "BogShow", 0x101C4014: "StaffRoomShow", 0x1019EDC4: "FeatureShow", 0x101C54F0: "StaffShow",
    0x101433F8: "VisitorShow",
}
GET_DBINFO, GET_PARENT, UPGRADES_WHAT = 0x10107CD4, 0x1010DA64, 0x1011050C

ADDR = dict(ride=0x103959A4, upgrade=0x10395B4C, shop=0x103959E0, show=0x10395A58, bog=0x10394800, feature=0x10394870,
            staffroom=0x10395A9C, staff=0x10395B1C, visitor=0x1037EC14, array=0x1034CBFC, none1=0x102A7868, none2=0x102A786A)
PANELS = ["ride", "upgrade", "shop", "show", "bog", "feature", "staffroom", "staff", "visitor"]
ROWS = 8
HOST_LAYOUT = None   # filled by bind (the host row size and the offset of mThing)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_show(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), L.lab(e.r(4))))
    return stub


for _a, _n in CALLEES.items():
    STUBS.internal(_a, make_show(_n))


def stub_dbinfo(e):
    L = e.cl
    L.rec("GetDBInfo", (L.lab(e.r(3)),))
    e.set_r(3, e.u32(e.r(3) + 0x40))


def stub_parent(e):
    L = e.cl
    L.rec("GetUpgradeParent", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetUpgradeParent") & 0xFFFF))


def stub_what(e):
    L = e.cl
    L.rec("UpgradesWhat", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("UpgradesWhat") & 0xFFFF))


def stub_assert(e):
    L = e.cl
    text = e.cstr(e.r(4))
    text = text.encode("latin-1") if isinstance(text, str) else bytes(text)
    L.rec("assert", (s32(e.r(3)), sum((i + 1) * b for i, b in enumerate(text)) & MASK))   # the text as a checksum


def s32(v):
    return T.s32(v)


STUBS.internal(0x1000DDE8, stub_assert)
STUBS.internal(GET_DBINFO, stub_dbinfo)
STUBS.internal(GET_PARENT, stub_parent)
STUBS.internal(UPGRADES_WHAT, stub_what)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def g(self, name):
        return ADDR[name]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.thing = e.alloc(0x60)
        self.target = e.alloc(0x60)
        self.info = e.alloc(0x7C0)
        fixed = {ADDR[n]: i + 1 for i, n in enumerate(PANELS)}
        fixed[self.thing] = 20
        fixed[self.target] = 21
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def row(self, i, ptr):
        self.words(ADDR["array"] + 0x14 * i, [ptr])

    def log(self):
        return self.L.log

    def asserts(self):
        return list(self.e.asserts)

    def call(self, ptr):
        T.call_ppc(self.e, FN, [ptr & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.thing = lib.tpw_gsp_alloc(0x60)
        self.target = lib.tpw_gsp_alloc(0x60)
        self.info = lib.tpw_gsp_alloc(0x7C0)
        fixed = {i + 1: self.g(n) for i, n in enumerate(PANELS)}
        fixed[20] = self.thing
        fixed[21] = self.target
        CL.host_begin(lib, fixed, case["queues"])

    def g(self, name):
        return self.lib.tpw_gsp_addr(name.encode())

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def row(self, i, ptr):
        self.write(self.g("array") + HOST_LAYOUT[0] * i + HOST_LAYOUT[1], struct.pack("<Q", ptr))

    def log(self):
        return CL.host_end(self.lib)

    def asserts(self):
        return list(T.host_asserts(self.lib))

    def call(self, ptr):
        self.lib.tpw_gsp_go(ptr)


def bind(lib):
    global HOST_LAYOUT
    if getattr(lib, "bound", False):
        return
    lib.tpw_gsp_addr.restype, lib.tpw_gsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gsp_alloc.restype, lib.tpw_gsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gsp_go.restype, lib.tpw_gsp_go.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gsp_layout(out)
    HOST_LAYOUT = list(out)[:n]
    assert HOST_LAYOUT[1] == 0, HOST_LAYOUT
    lib.bound = True


def build(s, case):
    s.half(s.g("none1"), case["none1"])
    s.half(s.g("none2"), case["none2"])
    s.write(s.thing + 0x40, struct.pack(">I", s.info) if isinstance(s, Ppc) else struct.pack("<Q", s.info))
    s.write(s.thing + 2, bytes([case["kind"]]))
    s.half(s.thing + 0x2E, case["flags"])
    s.words(s.info + 0x288, [case["w288"]])
    s.words(s.info + 0x7A8, [case["type"]])
    for i in range(ROWS):
        if i == 0:
            s.row(0, s.info if case["row0"] else 0)
        else:
            s.row(i, s.target if (case["rows"] >> i) & 1 else 0)


def execute(s, case):
    s.call(0 if case["null"] else s.thing)


def observe(s, case):
    return {"log": s.log()}


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


def mk(r, **kw):
    c = {"kind": 3, "flags": 0, "type": 0, "w288": 0, "none1": 0, "none2": 0, "row0": 0, "rows": 0xFE, "null": False}
    p1 = r.choice([0, 0, 1, 5, 7, 0xFFFF])
    c["queues"] = {"GetUpgradeParent": [p1, r.choice([0, 1, 2, 3, 7])], "UpgradesWhat": [r.choice([0, 4, 9])]}
    c["none1"] = r.choice([0, 0, 1, 5, 0xFFFF])
    c["none2"] = r.choice([0, 4, 9, 0xFFFF])
    c.update(kw)
    return c


def edges():
    r = random.Random(71)
    yield mk(r, null=True)
    for kind in (0, 1, 2, 4, 5, 6, 7, 8, 9, 0xFF):
        yield mk(r, kind=kind, flags=0xFFFF)
    for t in (-1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000):
        for w in (2, 0, 1, 3):
            yield mk(r, type=t, w288=w)
    for fl in (0, 1, 2, 3, 4, 0x8000, 0xFFFC, 0x0100):
        yield mk(r, type=3, flags=fl)
    for p1 in (0, 3):
        for what in (0, 4):
            q = {"GetUpgradeParent": [p1, 2], "UpgradesWhat": [what]}
            yield mk(r, type=0, w288=1, none1=0, none2=0, queues=q)
            yield mk(r, type=0, w288=1, none1=3, none2=4, queues=q)
    for row0 in (0, 1):
        yield mk(r, type=0, w288=0, none1=9, queues={"GetUpgradeParent": [1, 3], "UpgradesWhat": [0]}, row0=row0)
        yield mk(r, type=0, w288=0, none1=9, queues={"GetUpgradeParent": [1, 0], "UpgradesWhat": [0]}, row0=row0, rows=0xFE)


def rnd(r):
    return mk(r, kind=r.choice([0, 1, 3, 3, 3, 4, 5, 6, 7, 8, 9]), flags=r.randrange(0x10000), type=r.choice([0, 0, 1, 2, 3, 4, -1]),
              w288=r.choice([0, 1, 2, 3]), row0=r.choice([0, 1]), rows=r.randrange(256), null=r.random() < 0.05)


TESTS = [
    FnTest("UIMain_GotoStatusPanelForThing", ppc, host, edge=edges, rnd=rnd),
]
