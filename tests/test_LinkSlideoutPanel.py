"""Unit v5262, the main panel's slide-out slot, against libtpw_unit_LinkSlideoutPanel.dylib (tests/unit_LinkSlideoutPanel.cpp), main app
("SimTheme Park"), src/ui_mainpanel2_window_LinkSlideoutPanel.cpp:
  LinkSlideoutPanel 0x101a82ec, RemoveSlideoutPanel 0x101a83d8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (GetWindow, getExtraParam,
setExtraParam, UI_SendMessage, UI_PostMessage, SetState) are fakes on both sides that record every argument; GetWindow answers a block
(labels 1..5, 0 NULL) from the queue "GetWindow:<id>", getExtraParam from its queue.

The world of a case: gMainPanel (NULL or block 1), gGameWindow (NULL or non-NULL), the byte at +0x134 of block 2 (the slot), the queues.
What is compared: the answer (as a label), both globals, the byte, the whole event log.

No fused operations. Not tested: nothing skipped (the fakes never read their `this`, so the Mac's missing NULL checks are exercised too).
"""
import ctypes as C
import random

import tpwtest as T
from tpwtest import FnTest
import calllog as CL

SHIM_LIBRARY = "unit_LinkSlideoutPanel"
MASK = 0xFFFFFFFF

FN_LINK, FN_REMOVE = 0x101A82EC, 0x101A83D8
SIZES = {FN_LINK: 176, FN_REMOVE: 256}
OWN = set(SIZES)

GETWINDOW, GETEXTRA, SETEXTRA, SENDMSG, POSTMSG, SETSTATE = 0x101DBE40, 0x101DD030, 0x101DCFD0, 0x101C9518, 0x101C9400, 0x101CB1D0

ADDR = dict(gMainPanel=0x103949D8, gGameWindow=0x103949E4)
HOST_LAYOUT = [484]       # the host object has native pointers: mF134 sits at 484 there, 0x134 on the Mac
BLK = 0x800
PBLK = [0] * 6

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def blk(k):
    return PBLK[k] if 0 < k < 6 else 0


def stub_getwindow(e):
    L = e.cl
    this, wid = e.r(3), s32(e.r(4))
    L.rec("GetWindow", (L.lab(this), wid))
    e.set_r(3, blk(L.ret("GetWindow:%d" % wid)))


def stub_getextra(e):
    L = e.cl
    L.rec("getExtraParam", (L.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, L.ret("getExtraParam") & MASK)


def stub_setextra(e):
    L = e.cl
    L.rec("setExtraParam", (L.lab(e.r(3)), T.s16(e.r(4)), e.r(5) & MASK))
    e.set_r(3, 0)


def stub_setstate(e):
    L = e.cl
    L.rec("SetState", (L.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def stub_send(e):
    L = e.cl
    L.rec("UI_SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_post(e):
    L = e.cl
    L.rec("UI_PostMessage", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(GETEXTRA, stub_getextra)
STUBS.internal(SETEXTRA, stub_setextra)
STUBS.internal(SETSTATE, stub_setstate)
STUBS.internal(SENDMSG, stub_send)
STUBS.internal(POSTMSG, stub_post)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for k in range(1, 6):
        PBLK[k] = T.persist_alloc(runner.emu, BLK)


class Side:
    def zero(self):
        for k in range(1, 6):
            self.write(self.blk(k), bytes(BLK))

    def label(self, a):
        if a == 0:
            return 0
        for k in range(1, 6):
            if self.blk(k) == a:
                return k
        return -1


class Ppc(Side):
    off134 = 0x134

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({PBLK[k]: k for k in range(1, 6)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def blk(self, k):
        return PBLK[k]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.write(a, (v & MASK).to_bytes(4, "big"))

    def getptr(self, a):
        return int.from_bytes(self.read(a, 4), "big")

    def log(self):
        return self.L.log

    def link(self, panel, n):
        r3, _, _ = T.call_ppc(self.e, FN_LINK, [panel & MASK, n & MASK])
        return r3 & MASK

    def remove(self, which):
        T.call_ppc(self.e, FN_REMOVE, [which & MASK])
        return 0


class Host(Side):
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {k: self.blk(k) for k in range(1, 6)}, case["queues"])

    off134 = 484

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lsp_addr(name.encode())

    def blk(self, k):
        return self.lib.tpw_lsp_addr(b"blk%d" % k)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, v.to_bytes(8, "little"))

    def getptr(self, a):
        return int.from_bytes(self.read(a, 8), "little")

    def log(self):
        return CL.host_end(self.lib)

    def link(self, panel, n):
        return self.lib.tpw_lsp_link(panel, s32(n))

    def remove(self, which):
        self.lib.tpw_lsp_remove(s32(which))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lsp_addr.restype, lib.tpw_lsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lsp_alloc.restype, lib.tpw_lsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lsp_link.restype, lib.tpw_lsp_link.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_lsp_remove.restype, lib.tpw_lsp_remove.argtypes = None, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lsp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.zero()
    s.setptr(s.g("gMainPanel"), s.blk(case["main"]) if case["main"] else 0)
    s.setptr(s.g("gGameWindow"), s.blk(5) if case["game"] else 0)
    s.write(s.blk(2) + s.off134, bytes([case["kind"]]))


def execute(s, case):
    if case["op"] == "link":
        panel, n = case["args"]
        return s.label(s.link(s.blk(panel) if panel else 0, n))
    return s.remove(case["args"][0])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "main": s.label(s.getptr(s.g("gMainPanel"))), "game": s.getptr(s.g("gGameWindow")) != 0,
            "kind": s.read(s.blk(2) + s.off134, 1)}


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


KINDS = [0, 1, 2, 3, 4, 5, 0x83, 0x84, 0x7F, 0xFF, 0x80]


def base(r, op):
    return {"op": op, "main": r.choice([0, 1, 1, 1]), "game": r.choice([0, 1, 1, 1]), "kind": r.choice(KINDS),
            "queues": {"GetWindow:33": [r.choice([0, 2, 2])], "GetWindow:37": [r.choice([0, 3, 3])], "GetWindow:39": [r.choice([0, 4, 4])],
                       "getExtraParam": [r.choice([0, 1, 0x1234])]}}


def link_case(r, main=None, panel=None, w=None, extra=None):
    c = base(r, "link")
    if main is not None:
        c["main"] = main
    if w is not None:
        c["queues"]["GetWindow:33"] = [w]
    if extra is not None:
        c["queues"]["getExtraParam"] = [extra]
    c["args"] = [r.choice([0, 5]) if panel is None else panel, r.choice([0, 1, -1, 0x7FFFFFFF])]
    return c


def link_edges():
    r = random.Random(5262)
    for main in (0, 1):
        for panel in (0, 5):
            for w in (0, 2):
                for extra in (0, 1):
                    yield link_case(r, main, panel, w, extra)


def link_random(r):
    return link_case(r)


def rem_case(r, game=None, main=None, w=None, kind=None, extra=None, which=None, g25=None, g27=None):
    c = base(r, "remove")
    if game is not None:
        c["game"] = game
    if main is not None:
        c["main"] = main
    if w is not None:
        c["queues"]["GetWindow:33"] = [w]
    if kind is not None:
        c["kind"] = kind
    if extra is not None:
        c["queues"]["getExtraParam"] = [extra]
    if g25 is not None:
        c["queues"]["GetWindow:37"] = [g25]
    if g27 is not None:
        c["queues"]["GetWindow:39"] = [g27]
    c["args"] = [r.choice([0, 1, -1, 0x10000, 0x8000, 0x7FFF, r.randrange(0, 1 << 32)]) if which is None else which]
    return c


def rem_edges():
    r = random.Random(5263)
    for game in (0, 1):
        for main in (0, 1):
            yield rem_case(r, game, main)
    for w in (0, 2):
        for kind in (2, 3, 4, 5, 0x83, 0x84, 0xFF):
            for extra in (0, 1):
                for which in (0, 1, -1, 0x10000, 0x8000):
                    yield rem_case(r, 1, 1, w, kind, extra, which)
    for g25 in (0, 3):
        for g27 in (0, 4):
            yield rem_case(r, 1, 1, 2, 3, 0, 1, g25, g27)


def rem_random(r):
    return rem_case(r)


TESTS = [
    FnTest("LinkSlideoutPanel", ppc, host, edge=link_edges, rnd=link_random),
    FnTest("RemoveSlideoutPanel", ppc, host, edge=rem_edges, rnd=rem_random),
]
