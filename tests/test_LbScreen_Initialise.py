"""Unit v5056, WinScrn.cpp: window creation and screen start-up, mode release and the window procedure, against
libtpw_unit_LbScreen_Initialise.dylib (tests/unit_LbScreen_Initialise.cpp), bullfrog library ("bullfrog"), src/LbScreen_Initialise.cpp:
  LbScreen_Initialise 0x10068c60, ReleaseMode 0x100689ac, WindowProc 0x10068820

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (the LbApp getters, the window
imports, the DirectDraw releases, the event notifier, the FPU reset, the user window procedure through __ptr_glue) are fakes on both
sides that record their arguments and answer from the case's queues. Objects the fakes get are constant fake addresses that are never
dereferenced, so pointers compare as plain words.

The world of a case: the screen globals (initialised flag, guid, flags, window, palette, clipper, DirectDraw, the three surfaces'
DirectDraw pointers, the FPU handler byte, the class name's first bytes). What is compared: the answer, every global written, the log.

No fused operations. Not tested: nothing skipped; the FPU handler's vtable word is not compared (a host pointer is 8 bytes).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LbScreen_Initialise"
PROGRAM = "bullfrog symbols shared"
MASK = 0xFFFFFFFF

FN_INIT, FN_REL, FN_WP = 0x10068C60, 0x100689AC, 0x10068820
SIZES = {FN_INIT: 380, FN_REL: 320, FN_WP: 348}
OWN = set(SIZES)

# r2 of the main app is 0x10267cc0: each address is r2 + the listing's offset.
R2 = 0x1006EDA0
ADDR = dict(init=R2 + 0x6350, guid=R2 + 0x637C, f120=R2 + 0x6380, active=R2 + 0x6384, clipper=R2 + 0x648C + 0 * 0,
            dd=R2 + 0x6494, coop=R2 + 0x649C, cls=R2 + 0x64A0, modeflag=R2 + 0x4CA8, focus=R2 + 0x5058, hwnd=R2 + 0x6498,
            palette=R2 + 0x6490, fpub=R2 + 0x635C, fpu=R2 + 0x6360, cb=R2 + 0x506C, ddA=R2 + 0x63BC, ddB=R2 + 0x640C,
            ddC=R2 + 0x645C)
HOST_LAYOUT = []
LABELS = {R2 + 0x6360: 1, R2 + 0x506C: 2}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

_imp = STUBS.imp


def _glue(name, fn):
    """The real glue saves the caller's TOC pointer at 0x14(r1) and the code reloads it after the call: do the same."""

    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    _imp(name, wrapped)


GLUE = {"GetSystemMetrics__10NS_MacDozeFi": 0x1006C024,
        "DefWindowProc__10NS_MacDozeFP8GrafPortUiUiPl": 0x1006BEBC,
        "SetCursor__10NS_MacDozeFPQ213SamsUtilities7UCursor": 0x1006BFC4,
        "CreateWindowEx__10NS_MacDozeFUlPCcPCcUliiiiP8GrafPortPvPvPv": 0x1006C03C,
        "SetCooperativeLevel__11IDirectDrawFP8GrafPortUl": 0x1006BDFC,
        "Release__Q210NS_MacDoze8IUnknownFv": 0x1006BD0C}


def _glue2(name, fn):
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    STUBS.internal(GLUE[name], wrapped)


STUBS.imp = _glue2


def rec(name, spec=None, ret_key=None):
    """A PowerPC fake logging the u32 of the first len(spec) argument registers and answering r3 from the queue."""

    def fn(e):
        e.cl.rec(name, tuple(e.r(3 + i) & MASK for i in range(spec or 0)))
        if ret_key:
            e.set_r(3, e.cl.ret(ret_key) & MASK)

    return fn


STUBS.internal(0x10013FE4, rec("SetHWnd", 1))
STUBS.internal(0x1006A824, rec("RegisterClass", 0))
STUBS.internal(0x10013F44, lambda e: (e.cl.rec("GetHInstance", ()), e.set_r(3, e.cl.ret("GetHInstance") & MASK)))
STUBS.internal(0x10014340, lambda e: (e.cl.rec("GetWindowName", ()), e.set_r(3, e.cl.ret("GetWindowName") & MASK)))
STUBS.internal(0x10013FB4, lambda e: (e.cl.rec("GetHWnd", ()), e.set_r(3, e.cl.ret("GetHWnd") & MASK)))
STUBS.internal(0x100140FC, lambda e: e.cl.rec("SetCloseDownRequest", (s32(e.r(3)),)))
STUBS.imp("GetSystemMetrics__10NS_MacDozeFi",
          lambda e: (e.cl.rec("GetSystemMetrics", (s32(e.r(3)),)), e.set_r(3, e.cl.ret("GetSystemMetrics") & MASK)))
STUBS.imp("DefWindowProc__10NS_MacDozeFP8GrafPortUiUiPl",
          lambda e: (e.cl.rec("DefWindowProc", tuple(e.r(3 + i) & MASK for i in range(4))),
                     e.set_r(3, e.cl.ret("DefWindowProc") & MASK)))
STUBS.imp("SetCursor__10NS_MacDozeFPQ213SamsUtilities7UCursor", lambda e: e.cl.rec("SetCursor", (e.r(3) & MASK,)))
STUBS.internal(0x10014170, lambda e: (e.cl.rec("GetUserWndProc", ()), e.set_r(3, e.cl.ret("GetUserWndProc") & MASK)))
STUBS.internal(0x1006C12C, lambda e: (e.cl.rec("UserWndProc", tuple(e.r(3 + i) & MASK for i in range(4))),
                                      e.set_r(3, e.cl.ret("UserWndProc") & MASK)))


def stub_create_window(e):
    sp = e.r(1)
    cls = e.u32(e.r(4))
    args = [e.r(3), cls, e.r(5), e.r(6), e.r(7), e.r(8), e.r(9), e.r(10)] + [e.u32(sp + 0x38 + 4 * i) for i in range(4)]
    e.cl.rec("CreateWindowEx", tuple(a & MASK for a in args[:2] + args[2:3] + args[3:8] + args[8:]))
    e.set_r(3, e.cl.ret("CreateWindowEx") & MASK)


STUBS.imp("CreateWindowEx__10NS_MacDozeFUlPCcPCcUliiiiP8GrafPortPvPvPv", stub_create_window)


def stub_modes(e):
    out = e.r(3)
    e.cl.rec("CreateScreenModeList", (e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK))
    e.write(out, struct.pack(">i", s32(e.cl.ret("CreateScreenModeList"))) + b"\0" * 12)


STUBS.internal(0x1006A5D8, stub_modes)
STUBS.internal(0x1001611C, lambda e: e.cl.rec("FPUReset", (e.cl.lab(e.r(3)),)))
STUBS.internal(0x1001462C, lambda e: e.cl.rec("EventNotification", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.u32(e.r(5))),
                                                                     s32(e.u32(e.r(5) + 4)))))
STUBS.imp("SetCooperativeLevel__11IDirectDrawFP8GrafPortUl",
          lambda e: e.cl.rec("SetCooperativeLevel", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK)))
STUBS.imp("Release__Q210NS_MacDoze8IUnknownFv", lambda e: e.cl.rec("Release", (e.r(3) & MASK,)))


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
        self.L = e.cl = CL.PpcLog(LABELS, case["queues"])
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

    def init(self, guid, mode, hwnd, window):
        out = self.e.alloc(16)
        T.call_ppc(self.e, FN_INIT, [out, guid, mode & MASK, hwnd, window])
        return struct.unpack(">i", self.e.read(out, 4))[0]

    def release(self):
        T.call_ppc(self.e, FN_REL, [])
        return 0

    def wproc(self, w, m, wp, lp):
        return s32(T.call_ppc(self.e, FN_WP, [w, m, wp, lp])[0])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("fpu"), 2: self.g("cb")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5056_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init(self, guid, mode, hwnd, window):
        return self.lib.tpw_v5056_init(guid, s32(mode), hwnd, window)

    def release(self):
        self.lib.tpw_v5056_release()
        return 0

    def wproc(self, w, m, wp, lp):
        return self.lib.tpw_v5056_wproc(w, m, wp, lp)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5056_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v5056_init.restype, lib.tpw_v5056_init.argtypes = C.c_int32, [C.c_uint32, C.c_int32, C.c_uint32, C.c_uint32]
    lib.tpw_v5056_release.restype, lib.tpw_v5056_release.argtypes = None, []
    lib.tpw_v5056_wproc.restype, lib.tpw_v5056_wproc.argtypes = C.c_int32, [C.c_uint32] * 4
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


WORDS = ("init", "guid", "f120", "active", "clipper", "dd", "coop", "modeflag", "hwnd", "palette", "ddA", "ddB", "ddC")


def build(s, case):
    g = case["g"]
    for n in WORDS:
        s.words(s.g(n), [g[n]])
    s.write(s.g("focus"), bytes([g["focus"]]))
    s.write(s.g("fpub"), bytes([g["fpub"]]))
    s.write(s.g("cls"), g["cls"])


def execute(s, case):
    op = case["op"]
    if op == "init":
        return s.init(*case["args"])
    if op == "rel":
        return s.release()
    return s.wproc(*case["args"])


def observe(s, case, answer):
    o = {"answer": answer, "log": s.log()}
    for n in WORDS:
        o[n] = s.rd_word(s.g(n))
    o["focus"] = s.read(s.g("focus"), 1)
    o["fpub"] = s.read(s.g("fpub"), 1)
    o["cls"] = s.read(s.g("cls"), 8)
    return o


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


def world(r):
    ptr = lambda base: r.choice([0, base])
    return {"init": r.choice([0, 0, 1]), "guid": r.choice([0, 0x7000]), "f120": r.choice([0, 1, 7]),
            "active": r.choice([0, 1, 5]), "clipper": ptr(0x6300), "dd": ptr(0x5000), "coop": ptr(0x5100),
            "modeflag": r.choice([0, 1, 3]), "hwnd": r.choice([0, 0x3300]), "palette": ptr(0x6400),
            "ddA": ptr(0x6100), "ddB": ptr(0x6200), "ddC": ptr(0x6000), "focus": r.choice([0, 1, 9]),
            "fpub": r.choice([0, 1, 0xAA]), "cls": bytes(r.randrange(256) for _ in range(8))}


def base(r, op, args, **q):
    queues = {"GetHInstance": [0x1111], "GetWindowName": [0x2222], "GetSystemMetrics": [r.choice([480, 600]), r.choice([640, 800])],
              "CreateWindowEx": [r.choice([0, 0x3000])], "CreateScreenModeList": [r.choice([-1, 0, 5])],
              "GetHWnd": [r.choice([0x1000, 0x2000])], "GetUserWndProc": [r.choice([0, 1])],
              "DefWindowProc": [r.randrange(-100, 100) & MASK], "UserWndProc": [r.randrange(-100, 100) & MASK]}
    queues.update(q)
    return {"op": op, "g": world(r), "queues": queues, "args": args}


# ---- LbScreen_Initialise
def init_edges():
    r = random.Random(11)
    for init in (0, 1):
        for hw in (0, 0x1234):
            for cwe in (0, 0x3000):
                for csml in (-1, 0, 5):
                    for flag in (0, 1):
                        c = base(r, "init", [0x7000, 3, hw, 0x4000], CreateWindowEx=[cwe], CreateScreenModeList=[csml])
                        c["g"]["init"] = init
                        c["g"]["modeflag"] = flag
                        yield c


def init_random(r):
    return base(r, "init", [r.choice([0, 0x7000]), r.randrange(-3, 10), r.choice([0, 0x1234]), r.choice([0, 0x4000])])


# ---- ReleaseMode
def rel_edges():
    r = random.Random(12)
    for bits in range(64):
        c = base(r, "rel", [])
        for i, n in enumerate(("ddA", "ddB", "ddC", "dd", "clipper", "palette")):
            c["g"][n] = [0x6100, 0x6200, 0x6000, 0x5000, 0x6300, 0x6400][i] if bits >> i & 1 else 0
        yield c


def rel_random(r):
    return base(r, "rel", [])


# ---- WindowProc
def wp_edges():
    r = random.Random(13)
    for m in (0x1C, 0x10, 6, 0x112, 0, 0x1B, 0x11, 0x113, 5, 7, 0x100):
        for wp in (0, 1, 2, 3, 0xF100, 0xF000, 0xFFFFFFFF):
            for hw in (0x1000, 0x2000):
                for user in (0, 1):
                    yield base(r, "wp", [0x1000, m, wp, 0x8000], GetHWnd=[hw], GetUserWndProc=[user])


def wp_random(r):
    return base(r, "wp", [r.choice([0x1000, 0x2000]), r.choice([0x1C, 0x10, 6, 0x112, r.randrange(0, 0x200)]),
                          r.choice([0, 1, 2, 3, 0xF100, r.randrange(0, 1 << 32)]), r.choice([0, 0x8000])])


TESTS = [
    FnTest("LbScreen_Initialise", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("ReleaseMode", ppc, host, edge=rel_edges, rnd=rel_random),
    FnTest("WindowProc", ppc, host, edge=wp_edges, rnd=wp_random),
]
