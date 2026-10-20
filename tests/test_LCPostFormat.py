"""Unit v5004, the staff room panel's list formatter and message functions, against libtpw_unit_LCPostFormat.dylib
(tests/unit_LCPostFormat.cpp), main app ("SimTheme Park"), src/LCPostFormat.cpp:
  LCPostFormat 0x101c346c, StaffRoomPanelLCHandler 0x101c35a4, StaffRoomStatusMsg 0x101c3c68

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the draw object constructors, the window members, the list control's GetData and ChangeItem, the panel members, the panel
creators, DefaultStatusPanelMsg ...) are fakes on both sides that record their arguments; the virtual SetValue that LCPostFormat calls
through the child's vtable is a fake on both sides too.

The world of a case: the font index, the message 0x406 word, the thing and staff type globals, the panel object (its window, script
handle and two words), a list control block (item count), an item object (the float at +0x1f8) and what GetData and GetWindow answer.
What is compared: the answer, the event log, the 0x406 word and the panel's script words.

No fused operations (the float is converted with fctiwz: test values stay far below 2^31). Not tested: GetWindow answering NULL for
message 0x10 (the original reads the item count of NULL); the uninitialised first word of the item ChangeItem is given (column 0 is not
rewritten, so the fake logs only the second word).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCPostFormat"
MASK = 0xFFFFFFFF

FN_LC, FN_HAND, FN_MSG = 0x101c346c, 0x101c35a4, 0x101c3c68
SIZES = {FN_LC: 260, FN_HAND: 264, FN_MSG: 876}
OWN = set(SIZES)

ADDR = dict(font=0x102aed60, g406=0x102aed68, thing=0x103947a4, stafftype=0x10395a98, panel=0x10395a9c, ssp=0x10395b1c)
MAC_OFF = dict(pwin=0xC, script=0x24, f28=0x28, f2c=0x2C, child=0x58, count=0x150)
HOST_PANEL_LAYOUT = [0x10, 0x24, 0x28, 0x2C]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


fake(0x1018D11C, "SpriteCtor", "Ni")
fake(0x101DB624, "SetClientDrawObject", "LL")
fake(0x10180270, "GetFontTableEntry", "u", "i")
fake(0x101DA118, "SetFont", "Lu")
fake(0x101DE130, "SetClientDrawText", "Lu")
fake(0x101DBAC4, "GetChild", "Li", lambda e, log: e.u32(e.r(3) + MAC_OFF["child"]))
fake(0x10180940, "IndentClient", "Lhh")
fake(0x101898C8, "GradientCtor", "Ns")
fake(0x1023BAA8, "SetValue", "Li")

fake(0x101C31D4, "FillStaffRoomList", "Lii")
fake(0x101DE318, "CallDefaultWindowHandler", "Liii", "i")

fake(0x101C4098, "Remove", "L")
fake(0x1018227C, "CancelAdvisor", "i")
fake(0x101DD5E0, "setAnimTimer", "Lii", "h")
fake(0x101DD6B8, "startAnimTimer", "Li", "i")
fake(0x101DBE40, "GetWindow", "Li", lambda e, log: e.cl.fwin)
fake(0x10192C28, "GetNextObject", "L")
fake(0x10192DE0, "GetPreviousObject", "L")
fake(0x10193138, "EraseObject", "L")
fake(0x101983C4, "CreateAllItemsPanel", "iu")
fake(0x10199630, "CreateAllStaffPanel", "iu")
fake(0x101938D8, "Move", "L")
fake(0x101C2B88, "UpdateViewSprite", "Lu")
fake(0x100D9478, "SCR_KillScript", "i")
fake(0x101C54F0, "Show", "Lu")
fake(0x10193C78, "DefaultStatusPanelMsg", "Liii", "i")


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">I", L.fdata & MASK))
    e.set_r(3, 1)


def stub_changeitem(e):
    L = e.cl
    L.rec("ChangeItem", (L.lab(e.r(3)), s32(e.r(4)), e.u32(e.r(5) + 4), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


STUBS.internal(0x101D41F0, stub_getdata)
STUBS.internal(0x101D3660, stub_changeitem)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        o = self.o = {}
        for k in ("win", "pwin", "lst", "item"):
            o[k] = e.alloc(0x400)
        o["child"] = e.alloc(0x200)
        o["vt"] = e.alloc(0x80)
        fixed = {o["win"]: 1, o["child"]: 2, o["pwin"]: 3, o["lst"]: 4, ADDR["panel"]: 5, ADDR["ssp"]: 6}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def setfake(self, data, win):
        self.L.fdata, self.L.fwin = data, win

    def ptr(self, a, v):
        self.words(a, [v])

    def p32(self, addr):
        return addr

    def setup_vtable(self):
        self.words(self.o["child"] + 0x118, [self.o["vt"]])
        self.words(self.o["vt"] + 0x24, [0x1234])

    def call(self, op, args):
        entry = {"lc": FN_LC, "hand": FN_HAND, "msg": FN_MSG}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [self.o["win"]] + [a & MASK for a in args])
        return 0 if op == "lc" else s32(r3)   # LCPostFormat is void: r3 is whatever the last call left


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_lc_base()
        self.off = dict(pwin=lib.lay[0], script=lib.lay[1], f28=lib.lay[2], f2c=lib.lay[3], child=lib.lay[4], count=lib.lay[5])
        o = self.o = {}
        for k in ("win", "pwin", "lst", "item"):
            o[k] = lib.tpw_lc_alloc(0x400)
        o["child"] = lib.tpw_lc_make_window()
        fixed = {1: o["win"], 2: o["child"], 3: o["pwin"], 4: o["lst"], 5: self.g("panel"), 6: self.g("ssp")}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lc_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setfake(self, data, win):
        self.write(self.g("fakedata"), struct.pack("<I", data & MASK))
        self.write(self.g("fakewin"), struct.pack("<Q", win))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, addr):
        return addr - self.base

    def setup_vtable(self):
        pass

    def call(self, op, args):
        fn = {"lc": self.lib.tpw_lc_LCPostFormat, "hand": self.lib.tpw_lc_Handler, "msg": self.lib.tpw_lc_Msg}[op]
        if op == "lc":
            return fn(self.o["win"]) or 0
        return fn(self.o["win"], *[s32(a) for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lc_addr.restype, lib.tpw_lc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lc_base.restype, lib.tpw_lc_base.argtypes = C.c_uint64, []
    lib.tpw_lc_alloc.restype,lib.tpw_lc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lc_make_window.restype, lib.tpw_lc_make_window.argtypes = C.c_uint64, []
    lib.tpw_lc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lc_LCPostFormat.restype, lib.tpw_lc_LCPostFormat.argtypes = None, [C.c_uint64]
    for name in ("Handler", "Msg"):
        f = getattr(lib, "tpw_lc_" + name)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lc_layout(out)
    lib.lay = list(out)[:n]
    assert lib.lay[:4] == HOST_PANEL_LAYOUT, lib.lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    s.words(s.g("font"), [case["font"]])
    s.words(s.g("g406"), [case["g406"]])
    s.words(s.g("thing"), [case["thing"]])
    s.words(s.g("stafftype"), [case["stafftype"]])
    s.ptr(o["win"] + s.off["child"], o["child"])
    s.setup_vtable()
    p = s.g("panel")
    s.ptr(p + s.off["pwin"], o["pwin"])
    s.words(p + s.off["script"], [case["script"]])
    s.words(p + s.off["f28"], [case["f28"]])
    s.words(p + s.off["f2c"], [case["f2c"]])
    s.words(o["lst"] + s.off["count"], [case["count"]])
    s.words(o["item"] + 0x1F8, [case["itemf"]])
    s.setfake(s.p32(o["item"]) if case["fdata"] == "item" else case["fconst"], o["lst"] if case["fwin"] == "lst" else 0)


def observe(s, case, answer):
    p = s.g("panel")
    return {
        "answer": answer,
        "log": s.log(),
        "g406": s.rd_word(s.g("g406")),
        "script": [s.rd_word(p + s.off[k]) for k in ("script", "f28", "f2c")],
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

FLOATS = [0.0, 1.0, 0.99, 100.0, 127.5, 255.0, 255.5, 256.0, 257.0, -1.0, -0.5, -255.0, 1000.7, 70000.0, 12345.6, -300.25, 99999.0]


def base(r, op, args):
    return {
        "op": op,
        "args": args,
        "queues": {
            "GetFontTableEntry": [r.randrange(0, 1 << 32)],
            "CallDefaultWindowHandler": [r.randrange(-1000, 1000) & MASK],
            "DefaultStatusPanelMsg": [r.randrange(-1000, 1000) & MASK],
            "setAnimTimer": [r.choice([0, 1, -1, 0x10000, 0x18000, 0x7FFF])],
            "startAnimTimer": [r.choice([0, 1])],
        },
        "font": r.choice([7, 0, 1, 12, 0xFFFFFFFF, r.randrange(0, 1 << 32)]),
        "g406": r.choice([1, 0, r.randrange(0, 1 << 32)]),
        "thing": r.choice([0, 0x4000, r.randrange(0, 1 << 32)]),
        "stafftype": r.choice([0, 1, 2, 3, 4, r.randrange(0, 1 << 32)]),
        "script": r.choice([0, 0, 1, 77, r.randrange(1, 1 << 31)]),
        "f28": r.choice([0, 5, r.randrange(0, 1 << 32)]),
        "f2c": r.choice([0, 6, r.randrange(0, 1 << 32)]),
        "count": r.choice([0, 1, 2, 3, 4, r.randrange(0, 6)]),
        "itemf": fb(r.choice(FLOATS) if r.random() < 0.6 else r.uniform(-5000, 5000)),
        "fdata": "const",
        "fconst": r.choice([0x4000, 0x1234, r.randrange(0x100, 1 << 30)]),
        "fwin": r.choice(["lst", "none"]),
    }


MSGS = [0, 4, 5, 6, 0xF, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0xFF, 0x100, 0x101, 0x3FF, 0x400, 0x401, 0x402, 0x405, 0x406, 0x407, -1, 0x10000]
HAND_A = [0x1E7B, 0x1E7C, 0x1E7D, 0x1E7E, 0x1E7F, 0x1E80, 0x1E81, 0x1E82, 0x1E7A, 0, -1]
MSG_A100 = [-2, -1, 0, 0x1E79, 0x1E7A, 0x1E7B, 0x1E7C, 0x1E82, 0x1E83, 0x1E84, 0x1E85, 0x1E86, 0x1E87, 0x1E88, 0x1E89, 0x1E8A]


def lc_case(r):
    return base(r, "lc", [])


def hand_case(r, m=None, a=None, b=None):
    m = r.choice([0x101, 0x101, 0x101, 0x100, 0x102, 0, -1]) if m is None else m
    a = r.choice(HAND_A + [r.randrange(-5, 0x1F00)]) if a is None else a
    b = r.choice([1, 1, 0, 2, -1, r.randrange(-3, 5)]) if b is None else b
    return base(r, "hand", [m, a, b])


def msg_case(r, m=None, a=None, b=None):
    m = r.choice(MSGS) if m is None else m
    if a is None:
        if m == 0x100:
            a = r.choice(MSG_A100)
        elif m == 0x10:
            a = r.choice([0x80080, 0x80080, 0x80081, 0x8007F, 0, 0x180080, -0x80000 + 0x80])
        else:
            a = r.choice([0, 1, -1, 0x1E7B, r.randrange(-5, 0x2000)])
    if b is None:
        b = r.choice([-1, 0, 1, 5, -2, r.randrange(-3, 40)])
    c = base(r, "msg", [m, a, b])
    if m == 0x10 and a == 0x80080:
        c["fdata"] = "item"
        c["fwin"] = "lst"
    return c


def lc_edges():
    r = random.Random(11)
    for _ in range(4):
        yield lc_case(r)


def hand_edges():
    r = random.Random(12)
    for m in (0x101, 0x100, 0x102, 0):
        for a in HAND_A:
            for b in (1, 0, 2):
                yield hand_case(r, m, a, b)


def msg_edges():
    r = random.Random(13)
    for m in MSGS:
        for a in (MSG_A100 if m == 0x100 else [0x80080, 0x80081, 0x7FFFF, 0] if m == 0x10 else [0x1E7B, 0, -1]):
            for b in ([-1, 0, 3] if m in (0x400, 0x401, 0x406) else [0]):
                yield msg_case(r, m, a, b)
    for itemf in FLOATS:
        for count in (1, 3):
            c = msg_case(r, 0x10, 0x80080, 0)
            c["itemf"], c["count"] = fb(itemf), count
            yield c


TESTS = [
    FnTest("LCPostFormat", ppc, host, edge=lc_edges, rnd=lc_case),
    FnTest("StaffRoomPanelLCHandler", ppc, host, edge=hand_edges, rnd=hand_case),
    FnTest("StaffRoomStatusMsg", ppc, host, edge=msg_edges, rnd=msg_case),
]
