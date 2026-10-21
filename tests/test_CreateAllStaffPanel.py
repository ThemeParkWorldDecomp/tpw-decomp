"""Unit v5019, the all-staff panel, against libtpw_unit_CreateAllStaffPanel.dylib (tests/unit_CreateAllStaffPanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_allStaffPanel.cpp:
  CreateAllStaffPanel 0x10199630

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. Every callee (the window, list, check button, analyser and
helper functions, the constructors of the sprite and the gradient, the two virtual calls SetValue and SetText) is a fake on both sides
that records its arguments; windows are raw blocks (the Mac's vtable pointer at 0x118, the host's at 0) and answer GetWindow by id.

The world of a case: gGameWindow, gGameWorld, the font, sort and type globals, the old gStaffPanel, which windows exist (the list 0x321
and the check buttons 0x322 may be missing), the answers of Build, FindData, the happiness level, the happiness average and setAnimTimer.
What is compared: the call log and the globals gStaffPanel (as a label), gStaffType, gStaffSort, gStaffColumnFont.

No fused operations (one fctiwz, in range). Not tested: a staff kind outside 0 to 4 (the original passes the caller's leftover r30 as the
model for the happiness average); a level so large that fctiwz saturates (Unicorn answers differently).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateAllStaffPanel"
MASK = 0xFFFFFFFF

FN = 0x10199630

ADDR = dict(gamewin=0x103949e4, world=0x1037ebfc, stafftype=0x103947ec, staffsort=0x102a9088, stafffont=0x102a9084,
            staffpanel=0x103947f0)
WIN_IDS = [0x321, 0x10, 0x11, 0x12, 0x13, 0x14, 0x322, 0x329, 0x32A, 0x32B, 0x32C, 0x32D]
LABELS = {"gamewin": 1, "panel": 2, "drawp": 3, "fontp": 4, "world": 5, "analyserp": 6}
HOST_LAYOUT = []

CUR = {}                  # name -> PowerPC address of the case's objects


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)


def win_ret(e, L):
    # GetWindow(window, id): the id is in r4
    wid = e.r(4) & MASK
    if L.ret("w%x" % wid):
        return CUR["w%x" % wid]
    return 0


def rec(addr, name, spec, ret=None, then=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret, then=then))


rec(0x10180514, "CheckPanel", "")
rec(0x101a57fc, "SetLastPanel", "ii")
rec(0x101deb78, "Build", "Luu", ret=lambda e, L: CUR["panel"] if L.ret("build") else 0)
rec(0x101805a0, "SetPanel", "L")
rec(0x101dbe40, "GetWindow", "Li", ret=win_ret)
rec(0x101d2864, "SetColumnFormatsAll", "Luu")
rec(0x101d2614, "SetColumnType", "Lhi")
rec(0x101cd2d4, "FindDrawObject", "s", ret=lambda e, L: CUR["drawp"])
rec(0x101d1eb4, "SetSelectionObject", "LL")
rec(0x101dcba8, "SetCallbackFunction", "Lu")
rec(0x101d4db8, "SetSort", "Lib")
rec(0x10180468, "GetString", "i", ret=lambda e, L: (0x70000000 + (e.r(3) & MASK)) & MASK)
rec(0x101818e4, "MAKECOLUMNBUTTON", "Luiiiii")
rec(0x101987fc, "FillAllStaffList", "ii")
rec(0x101d4384, "FindData", "Li", ret="i")
rec(0x101d4c64, "ListSetCurrent", "Li")
rec(0x101cc148, "CheckSetCurrent", "Li")
rec(0x1018d11c, "SpriteCtor", "Nu")
rec(0x101898c8, "GradientCtor", "Ns")
rec(0x101db624, "SetClientDrawObject", "LL")
rec(0x10180270, "GetFontTableEntry", "u", ret=lambda e, L: CUR["fontp"])
rec(0x101da118, "SetFont", "LL")
rec(0x101de130, "SetClientDrawText", "Lu")
rec(0x10180940, "IndentClient", "Lhh")
rec(0x1013ebc8, "GetParkAnalyser", "L", ret=lambda e, L: CUR["analyserp"])
rec(0x101d4c64 + 0, "ListSetCurrent", "Li")


def level_ret(e, L):
    e.set_f1(bitsf(L.ret("level")))
    return 0


rec(0x100e4ebc, "GetStaffHappinessLevel", "Lu", ret=level_ret)
rec(0x100e89fc, "Average", "Li", ret="i")
rec(0x101dd5e0, "setAnimTimer", "Lii", ret="h")
rec(0x101dd6b8, "startAnimTimer", "Li")
rec(0x101821cc, "AdvisorMessage", "i")

_set_value = CL.recorder("SetValue", "Li")
_set_text = CL.recorder("SetText", "Lu")


def ptr_glue(e):
    (_set_value if (e.r(12) & MASK) == 0xA1 else _set_text)(e)


STUBS.internal(0x1023baa8, ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def make_objects(self):
        self.objs = {}
        for name in ("gamewin", "panel", "drawp", "fontp", "world", "analyserp"):
            self.objs[name] = self.new_object()
        for i in WIN_IDS:
            self.objs["w%x" % i] = self.new_object()


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        CUR.clear()
        CUR.update(self.objs)
        fixed = {self.objs[n]: l for n, l in LABELS.items()}
        for k, i in enumerate(WIN_IDS):
            fixed[self.objs["w%x" % i]] = 10 + k
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def new_object(self):
        vt = self.e.alloc(0x40)
        self.e.write(vt + 0x24, struct.pack(">I", 0xA1))
        self.e.write(vt + 0x2C, struct.pack(">I", 0xA2))
        obj = self.e.alloc(0x200)
        self.e.write(obj + 0x118, struct.pack(">I", vt))
        return obj

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def put_ptr(self, name, value):
        self.words(self.g(name), [value])

    def get_ptr(self, name):
        return self.rd_word(self.g(name))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, typ, thing):
        T.call_ppc(self.e, FN, [typ & MASK, thing & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        fixed = {l: self.objs[n] for n, l in LABELS.items()}
        for k, i in enumerate(WIN_IDS):
            fixed[10 + k] = self.objs["w%x" % i]
        wins = self.g("wins")
        for k, i in enumerate(WIN_IDS):
            self.write(wins + 8 * k, struct.pack("<Q", self.objs["w%x" % i]))
        self.write(self.g("panelp"), struct.pack("<Q", self.objs["panel"]))
        self.write(self.g("analyserp"), struct.pack("<Q", self.objs["analyserp"]))
        self.write(self.g("fontp"), struct.pack("<Q", self.objs["fontp"]))
        self.write(self.g("drawp"), struct.pack("<Q", self.objs["drawp"]))
        CL.host_begin(lib, fixed, case["queues"])

    def new_object(self):
        obj = self.lib.tpw_staff_alloc(0x200)
        self.write(obj, struct.pack("<Q", self.g("vtbl")))
        return obj

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_staff_addr(name.encode())

    def put_ptr(self, name, value):
        self.write(self.g(name), struct.pack("<Q", value))

    def get_ptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, typ, thing):
        self.lib.tpw_staff_create(T.s32(typ), thing & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_staff_addr.restype, lib.tpw_staff_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_staff_alloc.restype, lib.tpw_staff_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_staff_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_staff_create.restype, lib.tpw_staff_create.argtypes = None, [C.c_int32, C.c_uint32]
    out = (C.c_int64 * 4)()
    n = lib.tpw_staff_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.put_ptr("gamewin", s.objs["gamewin"])
    s.put_ptr("world", s.objs["world"])
    s.put_ptr("staffpanel", s.objs["panel"] if case["oldpanel"] else 0)
    s.words(s.g("stafftype"), [case["oldtype"]])
    s.words(s.g("staffsort"), [case["sort"]])
    s.words(s.g("stafffont"), [case["font"]])


def label_of(s, ptr):
    for n, l in LABELS.items():
        if s.objs[n] == ptr:
            return l
    return 0 if ptr == 0 else -1


def observe(s, case):
    return {"log": s.log(), "panel": label_of(s, s.get_ptr("staffpanel")),
            "type": s.rd_word(s.g("stafftype")), "sort": s.rd_word(s.g("staffsort")), "font": s.rd_word(s.g("stafffont"))}


def run(s, case):
    try:
        build(s, case)
        s.call(*case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def base(r):
    q = {"build": [r.choice([1, 1, 1, 0])],
         "w321": [r.choice([1, 1, 1, 0])], "w322": [r.choice([1, 1, 0])],
         "FindData": [r.choice([-1, 0, 3, 17, -5])],
         "level": [fb(r.choice([0.0, 1.0, 0.5, 99.9, 100.0, r.uniform(-5, 5), r.uniform(-300, 300), r.uniform(0, 100)]))],
         "Average": [r.choice([0, 1, 50, 99, 100, 200, 255, -1, r.randrange(-100000, 100000)])],
         "setAnimTimer": [r.choice([0, 1, 5, 0x10000, -1])]}
    for i in WIN_IDS:
        q.setdefault("w%x" % i, [1])
    return {"queues": q, "oldpanel": r.choice([0, 1]), "oldtype": r.randrange(0, 5), "sort": r.randrange(-2, 7),
            "font": r.choice([8, 6, 0, 3])}


def make(r, typ=None, thing=None):
    c = base(r)
    c["args"] = [r.choice([-1, 0, 1, 2, 3, 4]) if typ is None else typ,
                 r.choice([0, 0x1234, 0x2000abc0]) if thing is None else thing]
    return c


def edges():
    r = random.Random(5019)
    for t in (-1, 0, 1, 2, 3, 4):
        for th in (0, 0x4321):
            for _ in range(2):
                yield make(r, t, th)
    c = make(r, 2, 5)
    c["queues"]["build"] = [0]
    yield c
    c = make(r, 4, 5)
    c["queues"]["w321"] = [0]
    yield c
    c = make(r, 3, 5)
    c["queues"]["w322"] = [0]
    yield c
    c = make(r, 1, 5)
    c["queues"]["FindData"] = [0]
    yield c


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CreateAllStaffPanel", ppc, host, edge=edges, rnd=rnd),
]
