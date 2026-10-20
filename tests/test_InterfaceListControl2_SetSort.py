"""Unit v5908, six InterfaceListControl2 members, against libtpw_unit_InterfaceListControl2_SetSort.dylib
(tests/unit_InterfaceListControl2_SetSort.cpp), main app ("SimTheme Park"), src/InterfaceListControl2_items_InterfaceListControl2_SetSort.cpp:
  FindData 0x101d4384, PropagateMessage 0x101d4650, SelectClick 0x101d4a50, SetCurrent 0x101d4c64, SetSort 0x101d4db8,
  ColumnButtonClick 0x101d4fa4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, the host's own field offsets and pointer width) for the translation.
The callees outside the unit are fakes on both sides that record their arguments and answer from the case's queues: UI_SendMessage,
UI_PostMessage, AdvanceIndex, ChangeCurEntry, UpdateDisplay, Sort, GetWindow (answers the scroll bar), GetChild and GetNext (this + 0x20 and
this + 0x40, so the windows keep their labels), the list rectangle's PointIn (the virtual through the vtable at +0x138), the scroll bar's
SetValue (the virtual through the table at +0x118) and labs.

The world of a case: the list's fields, the item windows, the item words, the column rectangles. What is compared: the answer, the fields
the functions write (sort column and direction, first visible entry) and the event log.

No fused operations. Not tested: an AdvanceIndex answer below -1 or past the item words (the original reads outside the block); FindData on
a list without storage that has entries; SetCurrent's and SelectClick's windows being NULL (the original reads through them).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceListControl2_SetSort"
MASK = 0xFFFFFFFF

FN_FIND, FN_PROP, FN_SEL, FN_CUR, FN_SORT, FN_COL = 0x101d4384, 0x101d4650, 0x101d4a50, 0x101d4c64, 0x101d4db8, 0x101d4fa4
SIZES = {FN_FIND: 252, FN_PROP: 352, FN_SEL: 472, FN_CUR: 284, FN_SORT: 436, FN_COL: 424}
OWN = set(SIZES)

ADVANCE, CHANGECUR, UPDATE, SORT = 0x101d2a84, 0x101d4854, 0x101d2220, 0x101d1958
GETWINDOW, GETCHILD, GETNEXT = 0x101dbe40, 0x101dbac4, 0x101dbb38
POST, SEND, PTRGLUE = 0x101c9400, 0x101c9518, 0x1023baa8
TV_POINTIN, TV_SETVALUE = 0xABC00001, 0xABC00002     # what the fake vtables hold (the glue fake tells them by r12)

MAC = dict(attr=0x44, parent=0x5c, id=0x88, rtop=0x13e, storage=0x144, windows=0x148, cur=0x14c, count=0x150, top=0x154, first=0x160,
           ncol=0x164, sortcol=0x168, sortdesc=0x16a, columns=0x16c, rowh=0x170, vis=0x172)
MAC_SIZE, MAC_COL_STRIDE, MAC_COL_LEFT, MAC_COL_RIGHT = 0x200, 12, 4, 8
HOST_NAMES = ["attr", "parent", "id", "rtop", "storage", "windows", "cur", "count", "top", "first", "ncol", "sortcol", "sortdesc",
              "columns", "rowh", "vis", "size", "col_stride", "col_left", "col_right"]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def rec_ret(e, name, args):
    L = e.cl
    L.rec(name, args)
    e.set_r(3, L.ret(name) & MASK)


STUBS.internal(SEND, lambda e: rec_ret(e, "Send", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))))
STUBS.internal(POST, lambda e: rec_ret(e, "Post", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))))
STUBS.internal(ADVANCE, lambda e: rec_ret(e, "Advance", (e.cl.lab(e.r(3)), s32(e.r(4)), 1 if e.r(5) else 0)))
STUBS.internal(CHANGECUR, lambda e: e.cl.rec("ChangeCur", (e.cl.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(UPDATE, lambda e: e.cl.rec("Update", (e.cl.lab(e.r(3)),)))
STUBS.internal(SORT, lambda e: e.cl.rec("Sort", (e.cl.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(GETCHILD, lambda e: (e.cl.rec("GetChild", (e.cl.lab(e.r(3)), s32(e.r(4)))), e.set_r(3, (e.r(3) + 0x20) & MASK)))
STUBS.internal(GETNEXT, lambda e: (e.cl.rec("GetNext", (e.cl.lab(e.r(3)),)), e.set_r(3, (e.r(3) + 0x40) & MASK)))


def stub_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR["bar"])


def stub_glue(e):
    t = e.r(12)
    if t == TV_POINTIN:
        rec_ret(e, "PointIn", (s16(e.r(4)), s16(e.r(5))))
    elif t == TV_SETVALUE:
        e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))
    else:
        raise AssertionError("unknown glue target %x" % t)


def stub_labs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v if v < 0 else v) & MASK)


STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(PTRGLUE, stub_glue)
STUBS.imp("labs", stub_labs)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w(self, a, v, size):
        self.write(a, (v & ((1 << (8 * size)) - 1)).to_bytes(size, self.order))

    def r(self, a, size, signed=False):
        return int.from_bytes(self.read(a, size), self.order, signed=signed)

    def wf(self, name, v, size):
        self.w(self.obj + self.O[name], v, size)

    def wp(self, name, v):
        self.w(self.obj + self.O[name], v, self.psize)

    def make(self, case):
        """The blocks of the world (allocated before the log, so the labels are known)."""
        self.parent = self.alloc(0x40)
        self.bar = self.alloc(0x200)
        self.pool = self.alloc(0x2400)
        self.storage = self.alloc(0x400)
        self.winarr = self.alloc(16 * 8)
        self.colarr = self.alloc(8 * 16)
        self.new_object()
        self.fixed = {self.obj: 1, self.parent: 2, self.bar: 3}
        for k in range(0x2400 // 0x20):
            self.fixed[self.pool + 0x20 * k] = 10 + k


class Ppc(Side):
    order = "big"
    psize = 4
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.make(case)
        CUR["bar"] = self.bar
        self.L = e.cl = CL.PpcLog(self.fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def new_object(self):
        self.obj = self.alloc(MAC_SIZE)
        vt = self.alloc(0x10)
        self.w(vt + 8, TV_POINTIN, 4)
        self.w(self.obj + 0x138, vt, 4)
        tbl = self.alloc(0x40)
        self.w(tbl + 0x24, TV_SETVALUE, 4)
        self.w(self.bar + 0x118, tbl, 4)
        self.col_stride, self.col_left, self.col_right = MAC_COL_STRIDE, MAC_COL_LEFT, MAC_COL_RIGHT

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [self.obj] + [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    order = "little"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = dict(zip(HOST_NAMES, lib.layout))
        self.make(case)
        lib.tpw_ilc2s_set_bar(self.bar)
        CL.host_begin(lib, {label: addr for addr, label in self.fixed.items()}, case["queues"])

    def new_object(self):
        self.obj = self.lib.tpw_ilc2s_new_object()
        self.col_stride, self.col_left, self.col_right = self.O["col_stride"], self.O["col_left"], self.O["col_right"]

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ilc2s_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        f = {FN_FIND: self.lib.tpw_ilc2s_FindData, FN_PROP: self.lib.tpw_ilc2s_PropagateMessage, FN_SEL: self.lib.tpw_ilc2s_SelectClick,
             FN_CUR: self.lib.tpw_ilc2s_SetCurrent, FN_SORT: self.lib.tpw_ilc2s_SetSort, FN_COL: self.lib.tpw_ilc2s_ColumnButtonClick}[entry]
        r = f(self.obj, *[s32(a) for a in args])
        return (r or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ilc2s_alloc.restype, lib.tpw_ilc2s_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ilc2s_new_object.restype, lib.tpw_ilc2s_new_object.argtypes = C.c_uint64, []
    lib.tpw_ilc2s_set_bar.restype, lib.tpw_ilc2s_set_bar.argtypes = None, [C.c_uint64]
    lib.tpw_ilc2s_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, n, ret in (("FindData", 1, C.c_int32), ("PropagateMessage", 3, None), ("SelectClick", 2, None), ("SetCurrent", 1, None),
                         ("SetSort", 2, None), ("ColumnButtonClick", 1, None)):
        f = getattr(lib, "tpw_ilc2s_" + name)
        f.restype, f.argtypes = ret, [C.c_uint64] + [C.c_int32] * n
    out = (C.c_int64 * 32)()
    n = lib.tpw_ilc2s_layout(out)
    assert n == len(HOST_NAMES), n
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    s.wf("attr", c["attr"], 4)
    s.wp("parent", s.parent)
    s.wf("id", c["id"], 4)
    s.wf("rtop", c["rtop"], 2)
    s.wp("storage", s.storage if c["has_storage"] else 0)
    s.wp("windows", s.winarr if c["has_windows"] else 0)
    s.wf("cur", c["cur"], 4)
    s.wf("count", c["count"], 4)
    s.wf("top", c["top"], 4)
    s.wf("first", c["first"], 4)
    s.wf("ncol", c["ncol"], 2)
    s.wf("sortcol", c["sortcol"], 2)
    s.wf("sortdesc", c["sortdesc"], 2)
    s.wp("columns", s.colarr)
    s.wf("rowh", c["rowh"], 2)
    s.wf("vis", c["vis"], 2)
    for i, w in enumerate(c["words"]):
        s.w(s.storage + 4 * i, w, 4)
    for i in range(16):
        slot = c["slots"][i] if i < len(c["slots"]) else 0
        s.w(s.winarr + i * s.psize, s.pool + slot * 0x200 if slot else 0, s.psize)
    for i, (left, right) in enumerate(c["cols"]):
        s.w(s.colarr + i * s.col_stride + s.col_left, left, 2)
        s.w(s.colarr + i * s.col_stride + s.col_right, right, 2)


def observe(s, c, answer):
    o = s.obj
    return {"answer": answer, "log": s.log(),
            "sortcol": s.r(o + s.O["sortcol"], 2, True), "sortdesc": s.r(o + s.O["sortdesc"], 2, True),
            "top": s.r(o + s.O["top"], 4, True), "cur": s.r(o + s.O["cur"], 4, True)}


def run(s, c):
    try:
        build(s, c)
        answer = s.call(c["entry"], c["args"])
        return observe(s, c, s32(answer) if c["entry"] == FN_FIND else None)   # the others answer nothing
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases


def base(r, entry, args):
    cols = []
    for _ in range(8):
        left = r.randrange(-10, 100)
        cols.append((left, left + r.randrange(-5, 40)))
    return {
        "entry": entry, "args": args,
        "attr": (r.getrandbits(32) & ~0x90 & MASK) | r.choice([0, 0x10, 0x80, 0x90]),
        "id": r.choice([0, 1, 0x1234, -5, r.getrandbits(31)]),
        "rtop": r.randrange(-20, 100),
        "has_storage": r.random() < 0.85, "has_windows": r.random() < 0.85,
        "cur": r.choice([-1, -1, 0, 3, r.randrange(-1, 20)]),
        "count": r.choice([0, 1, 5, 20, r.randrange(0, 30)]),
        "top": r.randrange(0, 12),
        "first": r.choice([-1, 0, 2, r.randrange(-1, 25)]),
        "ncol": r.choice([0, 1, 2, 3, r.randrange(0, 9)]),
        "sortcol": r.randrange(-1, 5), "sortdesc": r.choice([-1, 0, 1, 2]),
        "rowh": r.choice([0, 10, 16, -4, r.randrange(-3, 40)]),
        "vis": r.choice([0, 1, 3, 6, r.randrange(0, 13)]),
        "cols": cols,
        "slots": [r.choice([0, r.randrange(1, 16)]) for _ in range(16)],
        "words": [r.choice([r.getrandbits(32), r.randrange(0, 8)]) for _ in range(64)],
        "queues": {"Send": [r.randrange(-3, 3)], "Post": [r.randrange(-3, 3)], "PointIn": [r.choice([0, 1])],
                   "Advance": [r.randrange(-1, 30) for _ in range(r.randrange(1, 12))]},
    }


def sortcase(r, column=None, sortcol=None, sortdesc=None, now=None):
    column = r.choice([0, 1, -1, 2, -2, 3, -3, 0x10001, -0x10001, -0x80000000, 0x7FFFFFFF, r.randrange(-6, 6)]) if column is None else column
    c = base(r, FN_SORT, [column, r.choice([0, 1]) if now is None else now])
    if sortcol is not None:
        c["sortcol"] = sortcol
    if sortdesc is not None:
        c["sortdesc"] = sortdesc
    return c


def sort_edges():
    r = random.Random(5908)
    for column in (0, 1, -1, 2, -2, 3, -3, 0x10001, -0x10001, -0x80000000, 0x7FFFFFFF):
        for sc in (-1, 0, 1, 2):
            for sd in (-1, 0, 1, 2):
                for now in (0, 1):
                    yield sortcase(r, column, sc, sd, now)


def sort_random(r):
    return sortcase(r)


def curcase(r, position=None, top=None, count=None, vis=None):
    c = base(r, FN_CUR, [r.choice([-1, 0, 1, 5, 10, 25, r.randrange(-3, 40)]) if position is None else position])
    if top is not None:
        c["top"] = top
    if count is not None:
        c["count"] = count
    if vis is not None:
        c["vis"] = vis
    return c


def cur_edges():
    r = random.Random(5909)
    yield dict(curcase(r, 3), has_storage=False)
    for position in (-1, 0, 4, 5, 6, 9, 10, 11, 14, 15, 19, 20, 25):
        for top in (0, 5, 12):
            for count in (0, 1, 10, 20):
                for vis in (0, 5, 12):
                    yield dict(curcase(r, position, top, count, vis), has_storage=True)


def cur_random(r):
    return curcase(r)


def colcase(r, button=None):
    c = base(r, FN_COL, [r.choice([0x10, 0x11, 0x12, 0x13, 0x0f, 0x18, 0x20, r.randrange(0, 0x1a), 0x10010, -0x10]) if button is None else button])
    if r.random() < 0.5:
        c["sortcol"] = (c["args"][0] - 0x10) if -1 < c["args"][0] - 0x10 < 8 else c["sortcol"]
    if r.random() < 0.8:
        c["attr"] |= 0x10
    return c


def col_edges():
    r = random.Random(5910)
    for button in (0x0f, 0x10, 0x11, 0x12, 0x13, 0x17, 0x18, 0x10010, 0x10011, -0x10):
        for sc in (-1, 0, 1, 2, 3):
            for sd in (-1, 0, 1, 2):
                for attr in (0, 0x10):
                    for cur in (-1, 0, 4):
                        c = colcase(r, button)
                        c.update(sortcol=sc, sortdesc=sd, attr=attr | (c["attr"] & ~0x10 & ~0x80), cur=cur, ncol=3)
                        yield c


def col_random(r):
    return colcase(r)


def selcase(r, x=None, y=None):
    c = base(r, FN_SEL, [r.randrange(-20, 150) if x is None else x, r.randrange(-30, 400) if y is None else y])
    c["args"] = [s16(a) & MASK for a in c["args"]]
    if r.random() < 0.5:
        c["rtop"] = r.randrange(-10, 20)
    return c


def sel_edges():
    r = random.Random(5911)
    for attr in (0, 0x80):
        for cur in (-1, 0, 7):
            for rowh in (0, 10, -10):
                for y in (-5, 0, 9, 10, 19, 20, 100, -0x8000):
                    for x in (-1, 0, 5, 6, 7, 20, 21, 0x7FFF):
                        for pin in (0, 1):
                            c = selcase(r, x, y)
                            c["attr"] = (c["attr"] & ~0x80) | attr
                            c.update(cur=cur, rowh=rowh, ncol=3, rtop=0, cols=[(0, 6), (5, 20), (20, 30)] + c["cols"][3:])
                            c["queues"]["PointIn"] = [pin]
                            yield c


def sel_random(r):
    return selcase(r)


def propcase(r, message=None):
    c = base(r, FN_PROP, [r.choice([0x1e, 0x1e, 3, 0x20, r.randrange(0, 40)]) if message is None else message,
                          r.choice([0, 7, r.getrandbits(31)]), r.choice([0, 9, -4, r.getrandbits(31)])])
    c["ncol"] = r.randrange(0, 9)
    c["has_storage"] = True     # the original reads the item words for message 0x1e without a test
    return c


def prop_edges():
    r = random.Random(5912)
    for message in (0x1e, 0x1d, 0x1f, 0, 3):
        for has in (False, True):
            for vis in (0, 1, 2, 5):
                for ncol in (0, 1, 2, 4):
                    for first in (-1, 0, 3):
                        c = propcase(r, message)
                        c.update(has_windows=has, vis=vis, ncol=ncol, first=first)
                        yield c


def prop_random(r):
    return propcase(r)


def findcase(r, data=None):
    c = base(r, FN_FIND, [r.choice([0, 1, 5, 7, r.getrandbits(32), r.randrange(0, 8)]) if data is None else data])
    c["has_storage"] = True
    if r.random() < 0.6:
        # make the wanted value present somewhere among the first words
        c["words"][r.randrange(1, 30)] = c["args"][0]
    return c


def find_edges():
    r = random.Random(5913)
    for count in (0, 1, 2, 5, 12):
        for first in (-1, 0, 3):
            for data in (0, 5, 6, 1234):
                c = findcase(r, data)
                c["words"] = [5 if i % 3 == 1 else 6 if i % 5 == 0 else 99 for i in range(64)]
                c.update(count=count, first=first)
                c["queues"]["Advance"] = [r.choice([-1, 0, 1, 2, 3, 4, 5, 6]) for _ in range(r.randrange(1, 8))]
                yield c


def find_random(r):
    return findcase(r)


TESTS = [
    FnTest("InterfaceListControl2::FindData", ppc, host, edge=find_edges, rnd=find_random),
    FnTest("InterfaceListControl2::PropagateMessage", ppc, host, edge=prop_edges, rnd=prop_random),
    FnTest("InterfaceListControl2::SelectClick", ppc, host, edge=sel_edges, rnd=sel_random),
    FnTest("InterfaceListControl2::SetCurrent", ppc, host, edge=cur_edges, rnd=cur_random),
    FnTest("InterfaceListControl2::SetSort", ppc, host, edge=sort_edges, rnd=sort_random),
    FnTest("InterfaceListControl2::ColumnButtonClick", ppc, host, edge=col_edges, rnd=col_random),
]
