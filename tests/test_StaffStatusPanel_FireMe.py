"""Unit v5107, the staff status panel's FireMe, Show, InitWindow and UpdateViewWindow, against libtpw_unit_StaffStatusPanel_FireMe.dylib
(tests/unit_StaffStatusPanel_FireMe.cpp), main app, src/StaffStatusPanel_FireMe.cpp:
  StaffStatusPanel::FireMe 0x101c5400, Show 0x101c54f0, InitWindow 0x101c4c34, UpdateViewWindow 0x101c423c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetString, the dialog
box, SackMe, the advisor, CoolRect's constructor and destructor, GetClientRect, GetWindow, Show, SetCallbackFunction, the sprite preview,
FillObjectList, InitEditBox, OpenWindow and the glue that calls through the table) are fakes on both sides that record what they read.

The world of a case: a panel block with the windows at 0x0c and 0x14 (the latter with a depth at 0xdc) and the table at 0x20 (entries 12 to 14
hold distinct functions, so the slot 13 = 0x34 is proved); the confirm-delete byte, the panel thing and the status thing globals; the
answer queues. What is compared: the event log and the status thing global. No fused operations. Not tested: nothing skipped (a NULL
window from GetWindow reaches Show as NULL on both sides, a NULL panel thing is a case).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffStatusPanel_FireMe"
MASK = 0xFFFFFFFF

FN_FIRE, FN_SHOW, FN_INIT, FN_UPDATE = 0x101C5400, 0x101C54F0, 0x101C4C34, 0x101C423C
SIZES = {FN_FIRE: 192, FN_SHOW: 104, FN_INIT: 136, FN_UPDATE: 140}

GLUE = 0x1023BAA8
ADDR = dict(confirm=0x1038071C + 0x37, panelThing=0x103947A4, statusThing=0x10395B18)
MARK = 0x05000F00            # what the fake UIMain_GetString answers on the PowerPC side
TV_FIREME, TV_MSG, TV_VIEWCB, SCRIPT = 0x10267FB8, 0x10267FC0, 0x10267FC8, 0x102AEDD0
MAC_ZOFF = 0xDC
MAC_IWSIZE = 0x200
HOST = {}
CUR = {}

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def stub_getstring(e):
    e.cl.rec("GetString", (s32(e.r(3)),))
    e.set_r(3, MARK)


def stub_dialog(e):
    p = e.r(3)
    text = struct.unpack(">I", e.read(p, 4))[0]
    r, g, b = e.read(p + 4, 3)
    v8, vc = struct.unpack(">II", e.read(p + 8, 8))
    e.cl.rec("DialogBox", (1 if text == MARK else 0, r, g, b, s32(v8), vc, s32(e.r(4)), 1 if e.r(5) == 0 else 0, s32(e.r(6)),
                           1 if e.r(7) == TV_FIREME else 0, s32(e.r(8)), (1 if e.r(9) == 0 else 0) | (2 if (e.r(10) & 0xFF) else 0)))
    e.set_r(3, 0)


def stub_sack(e):
    e.cl.rec("SackMe", (e.r(3),))


def stub_advisor(e):
    e.cl.rec("Advisor", (s32(e.r(3)),))


def stub_ctor(e):
    e.cl.rec("CoolRectCtor", (0,))


def stub_dtor(e):
    e.cl.rec("CoolRectDtor", (s32(e.r(4)),))


def stub_clientrect(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.write(e.r(4) + 4, struct.pack(">hhhh", s16(L.ret("rl")), s16(L.ret("rt")), s16(L.ret("rr")), s16(L.ret("rb"))))
    e.set_r(3, 1)


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    k = L.ret("GetWindow")
    e.set_r(3, CUR["wins"][k - 1] if k in (1, 2) else 0)


def stub_show(e):
    L = e.cl
    L.rec("Show", (L.lab(e.r(3)), s16(e.r(4))))
    e.set_r(3, 1)


def stub_setcb(e):
    L = e.cl
    L.rec("SetCallback", (L.lab(e.r(3)), 1 if e.r(4) == TV_VIEWCB else 0))


def stub_preview(e):
    L = e.cl
    rect = e.r(3)
    l, t, r, b = struct.unpack(">hhhh", e.read(rect + 4, 8))
    L.rec("Preview", (L.lab(rect), l, t, r, b, e.r(4), s32(e.r(5)), fb(e.f(1))))


def stub_fill(e):
    L = e.cl
    L.rec("Fill", (L.lab(e.r(3)), e.r(4)))
    e.set_r(3, L.ret("Fill") & MASK)


def stub_initedit(e):
    L = e.cl
    L.rec("InitEdit", (L.lab(e.r(3)), s32(e.r(4))))


def stub_open(e):
    L = e.cl
    L.rec("OpenWindow", (L.lab(e.r(3)), 1 if e.r(4) == SCRIPT else 0, 1 if e.r(5) == TV_MSG else 0, s32(e.r(6)), s32(e.r(7)),
                         s32(e.r(8))))


def stub_glue(e):
    L = e.cl
    L.rec("Virt", (L.lab(e.r(3)), CUR["ents"].get(e.r(12), -1)))


STUBS.internal(0x10180468, stub_getstring)
STUBS.internal(0x101F1800, stub_dialog)
STUBS.internal(0x10125660, stub_sack)
STUBS.internal(0x101821CC, stub_advisor)
STUBS.internal(0x101C7448, stub_ctor)
STUBS.internal(0x101C74E0, stub_dtor)
STUBS.internal(0x101DBA4C, stub_clientrect)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x101DC1EC, stub_show)
STUBS.internal(0x101DCBA8, stub_setcb)
STUBS.internal(0x10181648, stub_preview)
STUBS.internal(0x10192A90, stub_fill)
STUBS.internal(0x10193DCC, stub_initedit)
STUBS.internal(0x101935A0, stub_open)
STUBS.internal(GLUE, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def make_objects(self):
        """panel, window 0x0c, window 0x14, two children, table, three entry blocks (slots 12 to 14)."""
        self.panel = self.alloc(0x40)
        self.winA = self.alloc(self.iwsize)
        self.winB = self.alloc(self.iwsize)
        self.c1 = self.alloc(self.iwsize)
        self.c2 = self.alloc(self.iwsize)
        self.vt = self.alloc(0x40)
        self.ents = [self.alloc(16) for _ in range(3)]

    def labels(self):
        return [self.panel, self.winA, self.winB, self.c1, self.c2]


class Ppc(Side):
    fmt = ">I"
    hfmt = ">h"
    zoff = MAC_ZOFF
    iwsize = MAC_IWSIZE

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        CUR["wins"] = (self.c1, self.c2)
        CUR["ents"] = {self.ents[i]: 12 + i for i in range(3)}
        fixed = {a: i + 1 for i, a in enumerate(self.labels())}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a):
        return a

    def vfn(self, k):
        return b"\0" * 8

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        T.call_ppc(self.e, entry, args)


class Host(Side):
    fmt = "<I"
    hfmt = "<h"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.zoff = HOST["zoff"]
        self.iwsize = HOST["iwsize"]
        self.make_objects()
        lib.tpw_ssp_setwins(self.c1, self.c2)
        CL.host_begin(lib, {i + 1: a for i, a in enumerate(self.labels())}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ssp_addr(name.encode())

    def ptr(self, a):
        return self.lib.tpw_ssp_off(a)

    def vfn(self, k):
        return struct.pack("<Q", self.lib.tpw_ssp_vfn(k))

    def alloc(self, n):
        return self.lib.tpw_ssp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        {FN_FIRE: lambda: self.lib.tpw_ssp_fire(args[0]), FN_SHOW: lambda: self.lib.tpw_ssp_show(args[0], args[1] & MASK),
         FN_INIT: lambda: self.lib.tpw_ssp_init(args[0]), FN_UPDATE: lambda: self.lib.tpw_ssp_update(args[0])}[entry]()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ssp_addr.restype, lib.tpw_ssp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ssp_alloc.restype, lib.tpw_ssp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ssp_off.restype, lib.tpw_ssp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ssp_vfn.restype, lib.tpw_ssp_vfn.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_ssp_setwins.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_ssp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ssp_fire.argtypes = [C.c_uint64]
    lib.tpw_ssp_show.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_ssp_init.argtypes = [C.c_uint64]
    lib.tpw_ssp_update.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ssp_layout(out)
    vals = list(out)[:n]
    assert vals[4] == 0x20, vals       # the only host layout written by Mac offset (the panel's table); the others are read natively by the fakes
    HOST["zoff"], HOST["iwsize"] = vals[0], vals[1]
    lib.bound = True


def build(s, case):
    s.words(s.panel + 0x0C, [s.ptr(s.winA)])
    s.words(s.panel + 0x14, [s.ptr(s.winB)])
    s.words(s.panel + 0x20, [s.ptr(s.vt)])
    for i in range(3):
        s.words(s.vt + 4 * (12 + i), [s.ptr(s.ents[i])])
        s.write(s.ents[i], s.vfn(12 + i))
    s.write(s.winB + s.zoff, struct.pack(s.hfmt, case["z"]))
    s.write(s.g("confirm"), bytes([case["confirm"]]))
    s.words(s.g("panelThing"), [case["thing"]])
    s.words(s.g("statusThing"), [case["st0"]])


def run(s, case):
    try:
        build(s, case)
        op = case["op"]
        s.call({"fire": FN_FIRE, "show": FN_SHOW, "init": FN_INIT, "update": FN_UPDATE}[op],
               [s.panel, case["arg"]] if op == "show" else [s.panel])
        return {"log": s.log(), "status": s.rd_word(s.g("statusThing")), "thing": s.rd_word(s.g("panelThing")),
                "confirm": s.read(s.g("confirm"), 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


WORDS = [0, 1, 0x1234, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10394000]
SHORTS = [0, 1, -1, 0x7FFE, 0x7FFF, -0x8000, 100, -300]


def base(r, op, **kw):
    c = {"op": op, "confirm": r.choice([0, 1]), "thing": r.choice(WORDS + [r.getrandbits(32)]), "st0": r.getrandbits(32),
         "arg": r.choice(WORDS + [r.getrandbits(32)]), "z": r.choice(SHORTS + [r.randrange(-0x8000, 0x8000)])}
    c["queues"] = {"Fill": [r.choice([-3, 0, 1, 2, 3, 100])], "GetWindow": [r.choice([0, 1, 2]), r.choice([0, 1, 2])],
                   "rl": [r.choice(SHORTS)], "rt": [r.choice(SHORTS)], "rr": [r.choice(SHORTS)], "rb": [r.choice(SHORTS)]}
    c.update(kw)
    return c


def edges(op, seed):
    r = random.Random(seed)
    for confirm in (0, 1):
        for thing in (0, 0x1234, 0xFFFFFFFF):
            yield base(r, op, confirm=confirm, thing=thing)
    for z in SHORTS:
        yield base(r, op, z=z, thing=0x4321)
    for fill in (-1, 0, 1, 2, 3):
        for w in (0, 1, 2):
            c = base(r, op)
            c["queues"]["Fill"] = [fill]
            c["queues"]["GetWindow"] = [w, 3 - w if w else 2]
            yield c
    for arg in WORDS:
        yield base(r, op, arg=arg)


def mk(op, seed):
    return FnTest("StaffStatusPanel::" + {"fire": "FireMe", "show": "Show", "init": "InitWindow", "update": "UpdateViewWindow"}[op],
                  ppc, host, edge=lambda: edges(op, seed), rnd=lambda r: base(r, op))


TESTS = [mk("fire", 11), mk("show", 12), mk("init", 13), mk("update", 14)]
