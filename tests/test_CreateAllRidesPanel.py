"""Unit v5964, the all-rides and all-shops list panels, against libtpw_unit_CreateAllRidesPanel.dylib (tests/unit_CreateAllRidesPanel.cpp),
main app, src/game/ui/ui_allItemsPanel.cpp:
  CreateAllRidesPanel 0x1019762c, CreateAllShopsPanel 0x101972ac

A case is a world and a call. The world is built twice by one piece of Python code (PowerPC memory, host memory). Every callee outside
the unit (GetWindow, UI_SendMessage, Build, the list's setters, FindDrawObject, UI_IterateThingList, FindData, SetCurrent,
UIMain_GetString, MAKECOLUMNBUTTON) is a fake on both sides that records its arguments; function pointers are logged as small ids.

The world: seven blocks (panel, group, old list, new list, button, thing, text), the globals gAllItemsPanel, the two sorts and the font;
queues: whether the old list is found at the first, second or third try, and FindData's answer. Compared: the event log, gAllItemsListType,
gAllItemsList (as a label). No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateAllRidesPanel"
MASK = 0xFFFFFFFF

FN_RIDES, FN_SHOPS = 0x1019762C, 0x101972AC
SIZES = {FN_RIDES: 864, FN_SHOPS: 844}
OWN = set(SIZES)

ADDR = dict(panel=0x103947E0, list=0x103947D8, type=0x103947DC, ridesort=0x102A8CF8, shopsort=0x102A8CFC, font=0x102A8CF4)
HOST_LAYOUT = []

TVEC = {0x10267A38: 1, 0x10267A30: 2, 0x10267A28: 3, 0x10267A58: 4, 0x10267A50: 5, 0x10267A60: 6}
SCRIPT = 0x102A8A5A
NOBJ = 7
CUR = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def fid(a):
    return TVEC.get(a & MASK, 0)


def st_getwindow(e):
    L = e.cl
    this, wid = e.r(3), s32(e.r(4))
    lab = L.lab(this)
    L.rec("GetWindow", (lab, wid))
    out = 0
    if lab == 1 and wid == 0x12C4B8:
        out = CUR[2]
    elif lab == 2 and wid == 400:
        out = CUR[3] if L.ret("old") else 0
    elif lab == 4 and 0x10 <= wid <= 0x14:
        out = CUR[5]
    e.set_r(3, out)


def st_send(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def st_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), 1 if (e.r(4) & MASK) == SCRIPT else 0, e.r(5) & MASK))
    e.set_r(3, CUR[4])


def st_setcb(e):
    L = e.cl
    L.rec("SetCallback", (L.lab(e.r(3)), fid(e.r(4))))


def st_formats(e):
    L = e.cl
    L.rec("SetColumnFormatsAll", (L.lab(e.r(3)), fid(e.r(4)), fid(e.r(5))))


def st_coltype(e):
    L = e.cl
    L.rec("SetColumnType", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_find(e):
    L = e.cl
    L.rec("FindDrawObject", (1 if e.cstr(e.r(3)) == "hilight" else 0,))
    e.set_r(3, CUR[7])


def st_selobj(e):
    L = e.cl
    L.rec("SetSelectionObject", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_sort(e):
    L = e.cl
    L.rec("SetSort", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))


def st_iter(e):
    L = e.cl
    L.rec("IterateThingList", (s32(e.r(3)), fid(e.r(4))))
    e.set_r(3, 0)


def st_update(e):
    L = e.cl
    L.rec("UpdateDisplay", (L.lab(e.r(3)),))


def st_finddata(e):
    L = e.cl
    L.rec("FindData", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("find") & MASK)


def st_setcur(e):
    L = e.cl
    L.rec("SetCurrent", (L.lab(e.r(3)), s32(e.r(4))))


def st_getstring(e):
    L = e.cl
    L.rec("GetString", (s32(e.r(3)),))
    e.set_r(3, CUR[7])


def st_button(e):
    L = e.cl
    L.rec("MakeColumnButton", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), s32(e.r(9))))


for _a, _f in ((0x101DBE40, st_getwindow), (0x101C9518, st_send), (0x101DEB78, st_build), (0x101DCBA8, st_setcb),
               (0x101D2864, st_formats), (0x101D2614, st_coltype), (0x101CD2D4, st_find), (0x101D1EB4, st_selobj),
               (0x101D4DB8, st_sort), (0x101F7DB0, st_iter), (0x101D2220, st_update), (0x101D4384, st_finddata),
               (0x101D4C64, st_setcur), (0x10180468, st_getstring), (0x101818E4, st_button)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def label_of(self, v):
        for i, a in enumerate(self.objs):
            if a == v:
                return i + 1
        return -1 if v else 0


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = [e.alloc(0x200) for _ in range(NOBJ)]
        CUR.clear()
        for i, a in enumerate(self.objs):
            CUR[i + 1] = a
        self.L = e.cl = CL.PpcLog({a: i + 1 for i, a in enumerate(self.objs)}, case["queues"])
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

    def call(self, op, thing):
        T.call_ppc(self.e, FN_RIDES if op == "rides" else FN_SHOPS, [thing & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        out = (C.c_uint64 * NOBJ)()
        lib.tpw_cap_objects(out)
        self.objs = list(out)
        CL.host_begin(lib, {i + 1: a for i, a in enumerate(self.objs)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cap_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, thing):
        (self.lib.tpw_cap_rides if op == "rides" else self.lib.tpw_cap_shops)(thing)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cap_addr.restype, lib.tpw_cap_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cap_objects.argtypes = [C.POINTER(C.c_uint64)]
    lib.tpw_cap_rides.argtypes = [C.c_uint64]
    lib.tpw_cap_shops.argtypes = [C.c_uint64]
    lib.bound = True


def build(s, case):
    s.wptr(s.g("panel"), s.objs[0])
    s.words(s.g("ridesort"), [case["ridesort"]])
    s.words(s.g("shopsort"), [case["shopsort"]])
    s.words(s.g("font"), [case["font"]])
    s.words(s.g("type"), [0x55])
    s.wptr(s.g("list"), 0x66)


def run(s, case):
    try:
        build(s, case)
        thing = s.objs[5] if case["thing"] else 0
        s.call(case["op"], thing)
        return {"log": s.log(), "type": s.rd_words(s.g("type"), 1), "list": s.label_of(s.rptr(s.g("list"))),
                "panel": s.label_of(s.rptr(s.g("panel")))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, old=None, find=None, thing=None):
    return {"op": op, "ridesort": r.choice([0, 1, 2, 3, 4, -1, r.randrange(0, 9)]), "shopsort": r.choice([0, 1, 4, r.randrange(0, 9)]),
            "font": r.choice([8, 0, 7, r.randrange(0, 20)]),
            "thing": r.choice([0, 1]) if thing is None else thing,
            "queues": {"old": r.choice([[0], [1], [0, 1], [0, 0, 1], [0, 0, 0]]) if old is None else old,
                       "find": [r.choice([-1, 0, 3, 12])] if find is None else find}}


def edges(op):
    def gen():
        r = random.Random(5 if op == "rides" else 6)
        for old in ([0], [1], [0, 1], [0, 0, 1], [0, 0, 0]):
            for thing in (0, 1):
                for find in ([-1], [0], [5]):
                    yield mk(r, op, old, find, thing)
    return gen


TESTS = [
    FnTest("CreateAllRidesPanel", ppc, host, edge=edges("rides"), rnd=lambda r: mk(r, "rides")),
    FnTest("CreateAllShopsPanel", ppc, host, edge=edges("shops"), rnd=lambda r: mk(r, "shops")),
]
