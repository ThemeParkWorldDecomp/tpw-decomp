"""Unit v5554, the hire panel's list functions, against libtpw_unit_HirePanelLCHandler.dylib (tests/unit_HirePanelLCHandler.cpp), main app
("SimTheme Park"), src/game/ui/ui_hirePanel.cpp:
  LCPostFormat 0x101a18a4 (host name HirePanelLCPostFormat), HirePanelLCHandler 0x101a19f8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (operator new, the two draw object constructors, SetClientDrawObject, SetFont, GetChild, SetFontJust, GetFontTableEntry, the
virtual SetValue through the pointer glue, FillHireList, UIMain_GetString, UIMain_SetTitleText, SCR_KillScript, CallDefaultWindowHandler)
are fakes on both sides that record their arguments and answer from the case.

The world of a case: the four globals (font index, script handle, the two selection words, the hire panel window), a window and its child
(with a vtable block at +0x118 on the Mac side). What is compared: the answer, the globals and the event log.

No fused operations. Not tested: the NULL branch after operator new (it never returns NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HirePanelLCHandler"
MASK = 0xFFFFFFFF

FN_POST, FN_HANDLER = 0x101A18A4, 0x101A19F8
SIZES = {FN_POST: 288, FN_HANDLER: 592}
OWN = set(SIZES)

ADDR = dict(font=0x102AA31C, script=0x1039490C, sela=0x102AA328, selb=0x102AA32C, hirewin=0x10394924)
HOST_LAYOUT = []


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

CUR = {}


def stub_new(e):
    L = e.cl
    L.rec("new", (s32(e.r(3)),))
    e.set_r(3, e.alloc(0x19C))


def stub_getchild(e):
    L = e.cl
    L.rec("GetChild", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR["child"])


def stub_glue(e):
    L = e.cl
    L.rec("WindowSetValue", (L.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1018D11C, CL.recorder("SpriteCtor", "N"))
STUBS.internal(0x1018DE60, CL.recorder("CashCtor", "N"))
STUBS.internal(0x101DB624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret="i"))
STUBS.internal(0x101DA118, CL.recorder("SetFont", "Lu"))
STUBS.internal(0x101DBAC4, stub_getchild)
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x1023BAA8, stub_glue)
STUBS.internal(0x101A0FC8, CL.recorder("FillHireList", "ii"))
STUBS.internal(0x10180468, CL.recorder("UIMain_GetString", "i", ret="i"))
STUBS.internal(0x10180794, CL.recorder("UIMain_SetTitleText", "Liu"))
STUBS.internal(0x100D9478, CL.recorder("SCR_KillScript", "i"))
STUBS.internal(0x101DE318, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.win = e.alloc(0x120)
        self.child = e.alloc(0x120)
        self.panel = e.alloc(0x120)
        vt = e.alloc(0x40)
        e.write(self.child + 0x118, struct.pack(">I", vt))
        CUR["child"] = self.child
        self.L = e.cl = CL.PpcLog({self.win: 1, self.child: 2, self.panel: 3}, case["queues"])
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

    def post(self):
        T.call_ppc(self.e, FN_POST, [self.win])
        return 0

    def handler(self, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_HANDLER, [self.win, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win = lib.tpw_hire_alloc(0x120)
        self.child = lib.tpw_hire_alloc(0x120)
        self.panel = lib.tpw_hire_alloc(0x120)
        lib.tpw_hire_setchild(self.child)
        CL.host_begin(lib, {1: self.win, 2: self.child, 3: self.panel}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hire_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def post(self):
        self.lib.tpw_hire_post(self.win)
        return 0

    def handler(self, m, a, b):
        return self.lib.tpw_hire_handler(self.win, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hire_addr.restype, lib.tpw_hire_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hire_alloc.restype, lib.tpw_hire_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hire_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hire_setchild.argtypes = [C.c_uint64]
    lib.tpw_hire_post.argtypes = [C.c_uint64]
    lib.tpw_hire_handler.restype, lib.tpw_hire_handler.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hire_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("font"), [case["font"]])
    s.words(s.g("script"), [case["script"]])
    s.words(s.g("sela"), [case["sela"]])
    s.words(s.g("selb"), [case["selb"]])
    s.write(s.g("hirewin"), struct.pack(s.pfmt, s.panel))


def execute(s, case):
    if case["op"] == "post":
        return s.post()
    return s.handler(*case["args"])


def observe(s, case, answer):
    # the hire window global holds a pointer: compare it as "unchanged"
    same = struct.unpack(s.pfmt, s.read(s.g("hirewin"), struct.calcsize(s.pfmt)))[0] == s.panel
    return {"answer": answer, "log": s.log(), "font": s.rd_words(s.g("font"), 1), "script": s.rd_words(s.g("script"), 1),
            "sela": s.rd_words(s.g("sela"), 1), "selb": s.rd_words(s.g("selb"), 1), "win": same}


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


def base(r, op):
    return {"op": op,
            "queues": {"GetFontTableEntry": [r.getrandbits(32)], "UIMain_GetString": [r.getrandbits(32)],
                       "CallDefaultWindowHandler": [r.choice([0, 1, -1, r.getrandbits(32)]) & MASK]},
            "font": r.choice([0, 7, 12, r.getrandbits(32)]),
            "script": r.choice([0, 0, r.getrandbits(32)]),
            "sela": r.choice([0, r.getrandbits(32), MASK]), "selb": r.choice([0, r.getrandbits(32), MASK])}


def post_case(r):
    return base(r, "post")


def post_edges():
    r = random.Random(11)
    for _ in range(3):
        yield post_case(r)


def h_case(r, m=None, a=None, b=None):
    c = base(r, "handler")
    c["args"] = [r.choice([0x101, 0x101, 0x100, 0x102, 0, r.getrandbits(32)]) if m is None else m,
                 r.choice([0x2490, 0x2491, 0x2492, 0x2493, 0x2494, 0x248F, 0x2495, 0, r.getrandbits(32)]) if a is None else a,
                 r.choice([1, 1, 0, 2, MASK, r.getrandbits(32)]) if b is None else b]
    return c


def h_edges():
    r = random.Random(12)
    for a in (0x248F, 0x2490, 0x2491, 0x2492, 0x2493, 0x2494, 0x2495, 0, MASK):
        for b in (1, 0, 2):
            for m in (0x101, 0x100, 0x102):
                yield h_case(r, m, a, b)


TESTS = [
    FnTest("LCPostFormat", ppc, host, edge=post_edges, rnd=post_case),
    FnTest("HirePanelLCHandler", ppc, host, edge=h_edges, rnd=lambda r: h_case(r)),
]
