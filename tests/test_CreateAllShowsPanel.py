"""Unit v5677, CreateAllShowsPanel 0x10196ed0, against libtpw_unit_CreateAllShowsPanel.dylib, main app, src/LCPostFormat_CreateAllShowsPanel.cpp.

The callees outside the unit are fakes on both sides recording their arguments; windows are fixed fake addresses (never dereferenced).
GetWindow answers by (this, id); the answers for id 400 on the group come from the queue G400 (0 none, 1 an object); FindData from its queue.
World: the five globals. Compared: the event log, gAllItemsList (as 'is the list'), gAllItemsListType. No fused operations.
"""
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateAllShowsPanel"
MASK = 0xFFFFFFFF

FN = 0x10196ED0
SIZES = {FN: 936}
ADDR = dict(panel=0x103947E0, font=0x102A8CF4, sort=0x102A8D00, list=0x103947D8, type=0x103947DC)
SCRIPT, TV_FMT, TV_POST, TV_CB, TV_FILL = 0x102A8B42, 0x10267A30, 0x10267A28, 0x10267A38, 0x10267A68
HILIGHT = 0x1024DE34 + 0x53

PANEL, GROUP, OBJ400, LIST, DRAW, CHILD = 0x7F000100, 0x7F000200, 0x7F000300, 0x7F000400, 0x7F000500, 0x7F000600
LABELS = {PANEL: 1, GROUP: 2, OBJ400: 3, LIST: 4, DRAW: 5}
for _i in range(0x10, 0x16):
    LABELS[CHILD + _i] = 10 + _i


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def reg(addr, fn):
    STUBS.internal(addr, fn)


def get_window(e):
    L = e.cl
    me, wid = e.r(3), s32(e.r(4))
    res = 0
    if me == PANEL and wid == 0x12C4B8:
        res = GROUP
    elif me == GROUP and wid == 400:
        res = OBJ400 if L.ret("G400") else 0
    elif me == LIST and 0x10 <= wid <= 0x15:
        res = CHILD + wid
    L.rec("GetWindow", (L.lab(me), wid))
    e.set_r(3, res)


def send(e):
    L = e.cl
    L.rec("UI_SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), 1 if e.r(4) == SCRIPT else 0, e.r(5)))
    e.set_r(3, LIST)


def setcb(e):
    L = e.cl
    L.rec("SetCallbackFunction", (L.lab(e.r(3)), 1 if e.r(4) == TV_CB else 0))


def colfmt(e):
    L = e.cl
    L.rec("SetColumnFormatsAll", (L.lab(e.r(3)), 1 if e.r(4) == TV_FMT else 0, 1 if e.r(5) == TV_POST else 0))


def coltype(e):
    L = e.cl
    L.rec("SetColumnType", (L.lab(e.r(3)), T.s16(e.r(4)), s32(e.r(5))))


def finddraw(e):
    L = e.cl
    L.rec("FindDrawObject", tuple(e.read(e.r(3), 8)))
    e.set_r(3, DRAW)


def selobj(e):
    L = e.cl
    L.rec("SetSelectionObject", (L.lab(e.r(3)), e.r(4)))


def setsort(e):
    L = e.cl
    L.rec("SetSort", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))


def iterate(e):
    L = e.cl
    L.rec("UI_IterateThingList", (s32(e.r(3)), 1 if e.r(4) == TV_FILL else 0))
    e.set_r(3, 0)


def upd(e):
    L = e.cl
    L.rec("UpdateDisplay", (L.lab(e.r(3)),))


def finddata(e):
    L = e.cl
    L.rec("FindData", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("FindData") & MASK)


def setcur(e):
    L = e.cl
    L.rec("SetCurrent", (L.lab(e.r(3)), s32(e.r(4))))


def getstr(e):
    L = e.cl
    wid = s32(e.r(3))
    L.rec("UIMain_GetString", (wid,))
    e.set_r(3, 0x50000 + 2 * (wid & 0xFF))


def mkbtn(e):
    L = e.cl
    L.rec("MAKECOLUMNBUTTON", (L.lab(e.r(3)), (e.r(4) - 0x50000) // 2, s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), s32(e.r(9))))


for a, f in ((0x101DBE40, get_window), (0x101C9518, send), (0x101DEB78, build), (0x101DCBA8, setcb), (0x101D2864, colfmt),
             (0x101D2614, coltype), (0x101CD2D4, finddraw), (0x101D1EB4, selobj), (0x101D4DB8, setsort), (0x101F7DB0, iterate),
             (0x101D2220, upd), (0x101D4384, finddata), (0x101D4C64, setcur), (0x10180468, getstr), (0x101818E4, mkbtn)):
    reg(a, f)


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
        self.L = e.cl = CL.PpcLog(dict(LABELS), case["queues"])
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

    def call(self, thing):
        T.call_ppc(self.e, FN, [thing & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        import ctypes as C
        self.lib = lib
        self.C = C
        bind(lib)
        CL.host_begin(lib, {v: k for k, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cas_addr(name.encode())

    def read(self, a, n):
        return self.C.string_at(a, n)

    def write(self, a, data):
        self.C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, thing):
        self.lib.tpw_cas_run(thing & MASK)


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_cas_addr.restype, lib.tpw_cas_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cas_run.restype, lib.tpw_cas_run.argtypes = None, [C.c_uint32]
    lib.bound = True


def run(s, case):
    try:
        s.words(s.g("panel"), [PANEL])
        s.words(s.g("font"), [case["font"]])
        s.words(s.g("sort"), [case["sort"]])
        s.words(s.g("list"), [case["list0"]])
        s.words(s.g("type"), [case["type0"]])
        s.call(case["thing"])
        return {"log": s.log(), "list": s.rd_word(s.g("list")) == LIST, "type": s.rd_word(s.g("type"))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, g400=None, thing=None, fd=None):
    g = [r.choice([0, 1]) for _ in range(3)] if g400 is None else g400
    return {"queues": {"G400": g, "FindData": [r.choice([-1, 0, 5, -100, 77]) if fd is None else fd]},
            "thing": r.choice([0, 1, 0x1234, 0xFFFFFFFF]) if thing is None else thing,
            "font": r.choice([8, 0, -1, r.randrange(100)]), "sort": r.choice([0, 1, 2, -3, r.randrange(1000)]),
            "list0": r.choice([0, 0x1234]), "type0": r.choice([0, 1, 3, 9])}


def edges():
    r = random.Random(5677)
    for g in ([0, 0, 0], [1, 1, 1], [0, 1, 1], [0, 0, 1]):
        for thing in (0, 0x1234):
            for fd in (-1, 0, 9):
                yield mk(r, g, thing, fd)


def rnd(r):
    return mk(r)


TESTS = [FnTest("CreateAllShowsPanel", ppc, host, edge=edges, rnd=rnd)]
