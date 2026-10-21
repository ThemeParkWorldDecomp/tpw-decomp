"""Unit v5240, the status panel's button helpers, against libtpw_unit_SetButton.dylib (tests/unit_SetButton.cpp), main app
("SimTheme Park"), src/game/ui/ui_tags.cpp:
  SetButton 0x101904f8, DoButton 0x10190a60

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the draw object
library, InterfaceWindow::SetDrawObject, the virtual Enable through __ptr_glue, the panels, the zooms, the maintenance request, the challenge
answers, GAME_ASSERTMSG) are fakes on both sides that record their arguments; pointers the code never uses are small constants logged raw.

The world of a case: a window (help text word -1), gGameWorld, gThingArray[0] and [id] (mThing). What is compared: the answer, the window's
help text, and the event log. No fused operations. Not tested: a thing id past the table (the original does not check it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetButton"
MASK = 0xFFFFFFFF

FN_SET, FN_DO = 0x101904F8, 0x10190A60
SIZES = {FN_SET: 1316, FN_DO: 588}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, things=0x1034CBFC)
HOST_LAYOUT = [0, 10240]
NTHINGS = 10240


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
for addr, name, spec, ret in (
    (0x101CD2D4, "FindDrawObject", "s", "i"),
    (0x101DB474, "SetDrawObject", "Lu", None),
    (0x1023BAA8, "Enable", "Li", None),
    (0x101B8434, "CreateRideListPanel", "iw", None),
    (0x101A1C88, "CreateHireListPanel", "i", None),
    (0x101A4DE4, "CreateLoansPanel", "", None),
    (0x101B4F74, "CreateResearchPanel", "", None),
    (0x101ACA78, "CreateParkStatusPanel", "", None),
    (0x101BE440, "CreateStaffCostsPanel", "", None),
    (0x101C68D0, "CreateYearEndPanel", "", None),
    (0x1019D660, "CreateEntryPricePanel", "", None),
    (0x10181B00, "UIMain_ZoomToThing", "w", None),
    (0x1013EE88, "GetChallengeHQ", "u", "i"),
    (0x100F83CC, "ChallengeAccepted", "u", None),
    (0x100F853C, "ChallengeDeclined", "u", None),
    (0x1010BEB8, "RequestMaintenance", "ui", None),
    (0x1000DDE8, "GAME_ASSERTMSG", "is", None),
):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


def stub_zoom_map(e):
    L = e.cl
    L.rec("UIMain_ZoomToMapId", (struct.unpack(">H", e.read(e.r(3), 2))[0],))


STUBS.internal(0x10181D34, stub_zoom_map)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.window = e.alloc(0x150)
        self.vt = e.alloc(0x40)
        self.words(self.window + 0x118, [self.vt])
        self.L = e.cl = CL.PpcLog({self.window: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self):
        return ADDR["world"]

    def thing(self, i):
        return ADDR["things"] + 20 * i

    def helptext(self):
        return s32(self.rd_word(self.window + 0x8C))

    def set_help(self, v):
        self.words(self.window + 0x8C, [v])

    def call_set(self, action):
        T.call_ppc(self.e, FN_SET, [self.window, action & MASK])
        return 0

    def call_do(self, action, idword, extra):
        r3, _, _ = T.call_ppc(self.e, FN_DO, [action & MASK, idword & MASK, extra & MASK])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.window = lib.tpw_sb_newwindow()
        CL.host_begin(lib, {1: self.window}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self):
        return self.lib.tpw_sb_addr(b"world")

    def thing(self, i):
        return self.lib.tpw_sb_thing(i)

    def helptext(self):
        return s32(self.lib.tpw_sb_helptext(self.window))

    def set_help(self, v):
        pass   # the host constructor made it -1

    def call_set(self, action):
        self.lib.tpw_sb_set(self.window, s32(action))
        return 0

    def call_do(self, action, idword, extra):
        return self.lib.tpw_sb_do(s32(action), idword & MASK, s32(extra))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sb_addr.restype, lib.tpw_sb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sb_thing.restype, lib.tpw_sb_thing.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sb_newwindow.restype, lib.tpw_sb_newwindow.argtypes = C.c_uint64, []
    lib.tpw_sb_helptext.restype, lib.tpw_sb_helptext.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_sb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sb_set.restype, lib.tpw_sb_set.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_sb_do.restype, lib.tpw_sb_do.argtypes = C.c_int32, [C.c_int32, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.set_help(MASK)
    s.words(s.world(), [case["world"]])
    s.words(s.thing(0), [case["thing0"]])
    s.words(s.thing(case["tid"]), [case["thingid"]])
    if case["tid"] == 0:
        s.words(s.thing(0), [case["thingid"]])


def execute(s, case):
    if case["op"] == "set":
        return s.call_set(case["action"])
    return s.call_do(case["action"], case["idword"], case["extra"])


def observe(s, case, answer):
    return {"answer": answer, "help": s.helptext(), "log": s.log()}


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


def base(r, op, action):
    tid = r.choice([0, 1, r.randrange(NTHINGS), NTHINGS - 1])
    c = {
        "op": op,
        "action": action,
        "tid": tid,
        "world": r.choice([0x5000, 0x5100]),
        "thing0": r.choice([0, 0, 0x7000, 0x7100]),
        "thingid": r.choice([0x6000, 0x6100, r.randrange(1, 0x10000)]),
        "idword": (tid << 16) | r.choice([0, 0xFFFF, r.randrange(0x10000)]),
        "extra": r.choice([0, 1, 0xFFFF, 0x10000, 0x1234ABCD, r.getrandbits(32)]),
        "queues": {
            "FindDrawObject": [r.choice([0, 0x4000, r.randrange(1, 0x100000)])],
            "GetChallengeHQ": [r.choice([0, 0x9000, r.randrange(1, 0x100000)])],
        },
    }
    return c


ACTIONS = list(range(-2, 31)) + [0x7FFFFFFF, -0x80000000, 0xFFFFFFFF]


def mk(op):
    def edges():
        r = random.Random(5240 if op == "set" else 5241)
        for a in ACTIONS:
            for _ in range(2):
                yield base(r, op, a)

    def rnd(r):
        return base(r, op, r.choice(ACTIONS + list(range(3, 28))))

    return edges, rnd


se, sr = mk("set")
de, dr = mk("do")

TESTS = [
    FnTest("SetButton", ppc, host, edge=se, rnd=sr),
    FnTest("DoButton", ppc, host, edge=de, rnd=dr),
]
