"""Unit v5919, the online chat panel's first-person switches, against libtpw_unit_ChatBackToThirdPerson_2.dylib
(tests/unit_ChatBackToThirdPerson_2.cpp), main app ("SimTheme Park"), src/game/ui/ui_onlinePanel.cpp:
  ChatBackToThirdPerson 0x101e5ba0, ChatGoFirstPerson 0x101e5968, UpdateAvatarPos 0x101e5e08, ChatGoFullScreen 0x101e57bc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: UI_SendMessage, InterfaceWindow::setCursor / GetPosition / GetWindow /
CaptureKb, CoolRect's constructor and destructor, COnlineControl's constructor, HideAvatar and the four first-person calls (map positions
as the high halfword), operator new, CMouse::AssignMouseMode, CMMDefault's constructor and GetMode (reached through __ptr_glue, which runs
as the original).

The world of a case: the flags words, the panel pointers (panel, window, the saved one, each null or one of three blocks), the camera and
pointer positions, the saved map positions, the countdown, the bytes at 0x10395e78..7a, the control (preset or made on first use), the
mouse and its mode. What is compared: every global the functions write (map positions, countdown, saved flags, saved camera position,
pointers as names, mouse words) and the whole event log.

The one fused operation (fmadds of an offset byte times 1/256 plus a cell byte) is exact in both roundings, so no HardwareRounding.
Not tested: positions whose tenth does not fit 32 bits and NaN/infinity (fctiwz saturation differs in the emulator).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ChatBackToThirdPerson_2"
MASK = 0xFFFFFFFF

FN_BACK, FN_FIRST, FN_UPDATE, FN_FULL = 0x101E5BA0, 0x101E5968, 0x101E5E08, 0x101E57BC
SIZES = {FN_BACK: 572, FN_FIRST: 528, FN_UPDATE: 824, FN_FULL: 388}
OWN = set(SIZES)

OPERATOR_NEW = 0x102384B8
SEND_MESSAGE = 0x101C9518
SET_CURSOR = 0x101DD7F0
GET_POSITION = 0x101DB884
GET_WINDOW = 0x101DBE40
CAPTURE_KB = 0x101DCCEC
COOLRECT_CTOR, COOLRECT_DTOR = 0x101C7448, 0x101C74E0
CONTROL_CTOR = 0x10213870
ENTERING, LEAVING, UPDATE_POS, UNHIDE, HIDE = 0x102177BC, 0x10217884, 0x1021797C, 0x10217AA8, 0x10217A38
ASSIGN_MODE, MMDEFAULT_CTOR = 0x1007E2E0, 0x1007EA78
GET_MODE = 0x10081468
PTR_GLUE = 0x1023BAA8

ADDR = dict(flags=0x102B5A40, view=0x102B5BC0, saved=0x102B5BCC, campos=0x1029CC78, pointer=0x102C6CF8, m1e0=0x102DC1E0,
            m1e4=0x102DC1E4, mode=0x102DC1F0, mouse=0x102DC1F4, b78=0x10395E78, b79=0x10395E79, shown=0x10395E7A,
            word80=0x10395E80, window=0x10395E94, panel=0x10395E98, control=0x10396D28, cnt=0x10395E6C, sflags=0x10395E70,
            mz=0x10395E74, mx=0x10395E76)
HOST_LAYOUT = [2, 2, 8, 12]

NAMES = ["P", "W", "X", "G1", "G2", "G3", "C"]       # blocks, labels 1..7


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(PTR_GLUE)


def new_ret(e, log):
    return e.alloc(max(e.r(3) & MASK, 4)) & MASK


def win_ret(e, log):
    return e.ccp_wins.pop(0) & MASK if e.ccp_wins else 0


_NEW_REC = CL.recorder("OperatorNew", "u", ret=new_ret)


def stub_new(e):
    """The import glue saves the caller's TOC pointer at 0x14(sp), which the caller reloads after the call."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
    _NEW_REC(e)


STUBS.internal(OPERATOR_NEW, stub_new)
STUBS.internal(SEND_MESSAGE, CL.recorder("UI_SendMessage", "Liii"))
STUBS.internal(SET_CURSOR, CL.recorder("setCursor", "Lh"))
STUBS.internal(GET_POSITION, CL.recorder("GetPosition", "LLh"))
STUBS.internal(GET_WINDOW, CL.recorder("GetWindow", "Li", ret=win_ret))
STUBS.internal(CAPTURE_KB, CL.recorder("CaptureKb", "Li"))
STUBS.internal(COOLRECT_CTOR, CL.recorder("CoolRect", "N"))
STUBS.internal(COOLRECT_DTOR, CL.recorder("~CoolRect", "L"))
STUBS.internal(CONTROL_CTOR, CL.recorder("COnlineControl", "N"))
STUBS.internal(ENTERING, CL.recorder("EnteringFirstPerson", "Lww"))
STUBS.internal(LEAVING, CL.recorder("LeavingFirstPerson", "Lww"))
STUBS.internal(UPDATE_POS, CL.recorder("UpdateFirstPersonPos", "Lww"))
STUBS.internal(UNHIDE, CL.recorder("UnHideAvatar", "Lww"))
STUBS.internal(HIDE, CL.recorder("HideAvatar", "L"))
STUBS.internal(ASSIGN_MODE, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(MMDEFAULT_CTOR, CL.recorder("CMMDefault", "N"))
STUBS.internal(GET_MODE, CL.recorder("GetMode", "", ret="i"))


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

    def byte(self, name, v):
        self.write(self.g(name), bytes([v & 255]))

    def rd_byte(self, name):
        return self.read(self.g(name), 1)[0]

    def name_of(self, p):
        """A pointer word as the block's name, 0, or 'alloc' (made by operator new)."""
        if p == 0:
            return 0
        for k, v in self.blk.items():
            if v == p:
                return k
        return "alloc"

    def setp(self, name, key):
        self.ptr(self.g(name), 0 if key == 0 else self.blk[key])

    def getp(self, name):
        return self.name_of(self.rd_ptr(self.g(name)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blk = {k: e.alloc(16) for k in NAMES}
        e.ccp_wins = [self.blk["G1"], self.blk["G2"], self.blk["G3"]]
        self.L = e.cl = CL.PpcLog({self.blk[k]: i + 1 for i, k in enumerate(NAMES)}, case["queues"])
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
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def make_mode(self):
        tv = self.e.alloc(8)
        self.words(tv, [GET_MODE, 0])
        vt = self.e.alloc(0x40)
        self.words(vt + 0x2C, [tv])
        obj = self.e.alloc(8)
        self.words(obj, [vt])
        return obj

    def call(self, op):
        fn = {"back": FN_BACK, "first": FN_FIRST, "update": FN_UPDATE, "full": FN_FULL}[op]
        self.e.set_r(2, 0x10267CC0)       # the TOC base: 0x102608c8 + 0x73f8
        T.call_ppc(self.e, fn, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blk = {k: lib.tpw_ccp_alloc(16) for k in NAMES}
        lib.tpw_ccp_wins(self.blk["G1"], self.blk["G2"], self.blk["G3"])
        CL.host_begin(lib, {i + 1: self.blk[k] for i, k in enumerate(NAMES)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ccp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def make_mode(self):
        return self.lib.tpw_ccp_probe()

    def call(self, op):
        getattr(self.lib, "tpw_ccp_" + {"back": "back", "first": "first", "update": "update", "full": "full"}[op])()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ccp_addr.restype, lib.tpw_ccp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ccp_alloc.restype, lib.tpw_ccp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ccp_wins.restype, lib.tpw_ccp_wins.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_ccp_probe.restype, lib.tpw_ccp_probe.argtypes = C.c_uint64, []
    lib.tpw_ccp_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("back", "first", "update", "full"):
        f = getattr(lib, "tpw_ccp_" + n)
        f.restype, f.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ccp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("sflags"), [case["sflags"]])
    s.words(s.g("cnt"), [case["cnt"]])
    s.words(s.g("view"), [fb(v) for v in case["view"]])
    s.words(s.g("campos"), [fb(v) for v in case["campos"]])
    s.words(s.g("pointer"), [fb(v) for v in case["pointer"]])
    s.words(s.g("saved"), [fb(v) for v in case["saved"]])
    s.byte("b78", case["b78"])
    s.byte("b79", case["b79"])
    s.byte("shown", case["shown"])
    s.write(s.g("mx"), bytes(case["mx"]))
    s.write(s.g("mz"), bytes(case["mz"]))
    s.setp("word80", case["word80"])
    s.setp("window", case["window"])
    s.setp("panel", case["panel"])
    s.ptr(s.g("control"), s.blk["C"] if case["control"] else 0)
    s.ptr(s.g("mouse"), s.blk["X"] if case["mouse"] else 0)
    s.ptr(s.g("mode"), s.make_mode() if case["mode"] else 0)
    s.words(s.g("m1e0"), [case["m1e0"]])
    s.words(s.g("m1e4"), [case["m1e4"]])


def observe(s, case):
    return {"log": s.log(), "flags": s.rd_words(s.g("flags"), 1), "sflags": s.rd_words(s.g("sflags"), 1), "cnt": s.rd_words(s.g("cnt"), 1),
            "saved": s.rd_words(s.g("saved"), 3), "view": s.rd_words(s.g("view"), 3), "b78": s.rd_byte("b78"),
            "b79": s.rd_byte("b79"), "shown": s.rd_byte("shown"), "mx": list(s.read(s.g("mx"), 2)), "mz": list(s.read(s.g("mz"), 2)),
            "word80": s.getp("word80"), "window": s.getp("window"), "panel": s.getp("panel"), "control": s.getp("control"),
            "mouse": s.getp("mouse"), "modeset": s.rd_ptr(s.g("mode")) != 0, "m1e0": s.rd_words(s.g("m1e0"), 1),
            "m1e4": s.rd_words(s.g("m1e4"), 1)}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def mappos(x):
    """The cell and the 1/256 offset the code makes of a position."""
    q = T.f32(T.f32(x) / T.f32(10.0))
    i = int(q)
    cell = i & 255
    off = int(T.f32(256.0 * T.f32(q - cell))) & 255
    return [cell, off]


def pos(r):
    k = r.random()
    if k < 0.3:
        return T.f32(r.uniform(-50, 3000))
    if k < 0.5:
        return T.f32(r.randrange(-30, 400) * 10.0)
    if k < 0.65:
        return T.f32(r.randrange(-30, 400) * 10.0 - r.choice([0.001, 0.01, 0.5, 0.0]))
    if k < 0.75:
        return r.choice([0.0, -0.0, 10.0, 9.999, 2550.0, 2560.0, -5.0, 2559.99, 5.0, -10.0, 2549.9])
    return T.f32(r.uniform(-20000, 200000))


def ptrname(r):
    return r.choice([0, "P", "W", "X"])


FLAG_SETS = [0, 0x400, 4, 0x16, 0x416, 2, 0x10, 0x1, 0x404, 0x8000, 0x402]


def base(r, op):
    c = {"op": op,
         "flags": r.choice(FLAG_SETS) | r.choice([0, 0, 0x100, 0x20]),
         "sflags": r.choice(FLAG_SETS),
         "cnt": r.choice([0, 0, 1, 2, 5, -1, r.randrange(-3, 9)]),
         "view": [pos(r), pos(r), pos(r)], "campos": [pos(r), pos(r), pos(r)], "pointer": [pos(r), pos(r), pos(r)],
         "saved": [pos(r), pos(r), pos(r)],
         "b78": r.choice([0, 1, 1, 0xFF, 2]), "b79": r.choice([0, 0, 1, 7]), "shown": r.choice([0, 1, 1, 2, 0xFF]),
         "mx": [r.randrange(256), r.randrange(256)], "mz": [r.randrange(256), r.randrange(256)],
         "word80": ptrname(r), "window": r.choice([0, "W", "W", "P"]), "panel": r.choice([0, "P", "P", "W"]),
         "control": r.choice([0, 1]), "mouse": r.choice([0, 1]), "mode": r.choice([0, 1, 1]),
         "m1e0": r.randrange(1 << 32), "m1e4": r.randrange(1 << 32),
         "queues": {"GetMode": [r.choice([9, 9, 1, 0, 8, 10, r.randrange(-5, 20)])]}}
    if op == "back" and r.random() < 0.4:
        c["word80"] = c["panel"]
    if op in ("first", "full"):
        c["window"] = r.choice(["W", "W", "P", "X"])
        c["panel"] = r.choice(["P", "P", "W", "X"])
    if op == "update":
        src = c["campos"] if r.random() < 0.5 else c["view"]
        if r.random() < 0.5:
            c["mx"], c["mz"] = mappos(src[0]), mappos(src[2])
        if r.random() < 0.15:
            c["mx"] = mappos(src[0])
    return c


def make(op):
    def one(r):
        return base(r, op)
    return one


def edges(op):
    def gen():
        r = random.Random({"back": 11, "first": 12, "update": 13, "full": 14}[op])
        if op in ("back", "first"):
            for flags in (0, 0x400, 4, 0x404, 0x16):
                for word80 in (0, "P", "W", "X"):
                    for ctl in (0, 1):
                        for panel in (0, "P", "W"):
                            c = base(r, op)
                            c.update(flags=flags, word80=word80, control=ctl, panel=panel)
                            if op == "first":
                                c["window"] = r.choice(["W", "P"])
                                if panel == 0:
                                    c["panel"] = "P"
                                c["shown"] = r.choice([0, 1])
                            yield c
            for p in (0.0, -0.0, 10.0, 9.999, 2550.0, 2560.0, -5.0, -10.0, 5.0, 2559.99, 1e6, -2550.0, 2549.9):
                c = base(r, op)
                c["flags"] = 0
                c["view"] = [p, p, p]
                c["pointer"] = [p, p, p]
                c["control"] = 1
                yield c
        elif op == "update":
            for flags in FLAG_SETS:
                for sflags in FLAG_SETS:
                    for cnt in (0, 1, 2, -1):
                        for b78 in (0, 1):
                            for same in (0, 1):
                                c = base(r, op)
                                c.update(flags=flags, sflags=sflags, cnt=cnt, b78=b78)
                                src = c["campos"] if r.random() < 0.5 else c["view"]
                                if same:
                                    c["mx"], c["mz"] = mappos(src[0]), mappos(src[2])
                                yield c
            for p in (0.0, -0.0, 10.0, 9.999, 2550.0, 2560.0, -5.0, -10.0, 5.0, 2559.99, 1e6, -2550.0, 2549.9):
                c = base(r, op)
                c.update(flags=0x400, sflags=0x400, cnt=0, b78=1, control=1, view=[p, p, p], campos=[p, p, p])
                yield c
                c = base(r, op)
                c.update(flags=0x4, sflags=0x400, cnt=0, b78=0, control=1, view=[p, p, p], campos=[p, p, p])
                yield c
        else:
            for shown in (0, 1, 2, 255):
                for b79 in (0, 1):
                    for mouse in (0, 1):
                        for mode in (0, 1):
                            for gm in (9, 1):
                                c = base(r, op)
                                c.update(shown=shown, b79=b79, mouse=mouse, mode=mode, queues={"GetMode": [gm]})
                                yield c
    return gen


TESTS = [
    FnTest("ChatBackToThirdPerson", ppc, host, edge=edges("back"), rnd=make("back")),
    FnTest("ChatGoFirstPerson", ppc, host, edge=edges("first"), rnd=make("first")),
    FnTest("UpdateAvatarPos", ppc, host, edge=edges("update"), rnd=make("update")),
    FnTest("ChatGoFullScreen", ppc, host, edge=edges("full"), rnd=make("full")),
]
