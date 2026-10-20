"""Unit v5095, InterfaceListControl2::RemoveAllItems / RemoveItem / RemoveMe, against libtpw_unit_InterfaceListControl2_RemoveAllItems.dylib
(tests/unit_InterfaceListControl2_RemoveAllItems.cpp), main app ("SimTheme Park"),
src/InterfaceListControl2_items_InterfaceListControl2_RemoveAllItems.cpp:
  RemoveAllItems 0x101d3cc8, RemoveItem 0x101d39f0, RemoveMe 0x101d3818

A case is a world and a call. The world (a list object, an item block of Mac words with guard words around it, a column array, string
objects, a scroll bar child, a parent, a draw object) is built twice by ONE piece of Python code: big-endian 4-byte words in emulator
memory, little-endian 8-byte words in the host library (the field offsets differ and come from the host layout). The callees outside the
unit are fakes on both sides: AdvanceIndex and FindFreeEntry (small deterministic stand-ins that read the block), ChangeCurEntry,
UpdateDisplay, UI_PostMessage, UI_SendMessage, GetWindow, Show, SetMinMax, the string destructor, and the two virtual calls through glue
(scroll bar SetValue, draw object Show).

What is compared: the answer, the event log, the list's integer fields and the whole block (guards included, string cells as labels).
No fused operations. Not tested: a column count of -4 (divw by zero in RemoveAllItems); a block word count whose entry count exceeds the
block (the original writes past it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceListControl2_RemoveAllItems"
MASK = 0xFFFFFFFF

FN_ALL, FN_ITEM, FN_ME = 0x101D3CC8, 0x101D39F0, 0x101D3818
SIZES = {FN_ALL: 712, FN_ITEM: 672, FN_ME: 412}
OWN = set(SIZES)

ADV, CUR, FREE, UPD = 0x101D2A84, 0x101D4854, 0x101D2D6C, 0x101D2220
POST, SEND, GETWIN, SHOW, MINMAX = 0x101C9400, 0x101C9518, 0x101DBE40, 0x101DC1EC, 0x101D6B44
GLUE, STRDTOR = 0x1023BAA8, 0x101468F8
GLOBAL_AUTO = 0x10395C28
VT_BAR, VT_DO = 0xAAAA0001, 0xAAAA0002

GUARD = 4
MAC = dict(storage=0x144, cur=0x14C, count=0x150, top=0x154, bw=0x158, first=0x160, cols=0x164, colptr=0x16C, vis=0x172, draw=0x174,
           parent=0x5C, crstride=0xC, crbottom=0xA, size=0x178)
HOST_NAMES = ["size", "storage", "cur", "count", "top", "bw", "first", "cols", "colptr", "vis", "draw", "parent", "crstride", "crbottom"]
PTRS = ("storage", "colptr", "draw", "parent")
I16 = ("cols", "vis")
INTS = ("cur", "count", "top", "bw", "first", "cols", "vis")


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, a):
    return e.cl.lab(a)


def mkrec(name, fields):
    def stub(e):
        L = e.cl
        args = []
        for kind, reg in fields:
            v = e.r(reg)
            args.append(L.lab(v) if kind == "L" else (T.s16(v) if kind == "h" else s32(v)))
        L.rec(name, tuple(args))
        e.set_r(3, 0)
    return stub


def words_of(e, this):
    return e.u32(this + MAC["storage"])


def stub_adv(e):
    L = e.cl
    this, idx = e.r(3), s32(e.r(4))
    L.rec("AdvanceIndex", (L.lab(this), idx, 1 if e.r(5) else 0))
    st = words_of(e, this)

    def w(i):
        return e.s32(st + 4 * i)
    res = -1
    c = w(idx + 2)
    if c >= 0:
        res = c
    else:
        n = w(idx)
        if n >= 0:
            res = n
        else:
            p = w(idx + 3)
            k = 0
            while p >= 0 and k < 64:
                n = w(p)
                if n >= 0:
                    res = n
                    break
                p = w(p + 3)
                k += 1
    e.set_r(3, res & MASK)


def stub_free(e):
    L = e.cl
    this = e.r(3)
    L.rec("FindFreeEntry", (L.lab(this),))
    st = words_of(e, this)
    found, before, ent = -1, -1, e.s32(this + MAC["first"])
    n = 0
    while ent != -1 and n < 64:
        if e.s32(st + 4 * (ent + 3)) == -2:
            found = ent
            break
        before = ent
        ent = e.s32(st + 4 * ent)
        n += 1
    e.write(e.r(4), struct.pack(">I", found & MASK))
    e.write(e.r(5), struct.pack(">I", before & MASK))


def stub_getwin(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.bar)


def stub_glue(e):
    L = e.cl
    t = e.r(12)
    if t == VT_BAR:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif t == VT_DO:
        L.rec("DOShow", (L.lab(e.r(3)), T.s16(e.r(4))))
    else:
        L.rec("BadGlue", (t,))


STUBS.internal(ADV, stub_adv)
STUBS.internal(CUR, mkrec("ChangeCurEntry", [("L", 3), ("i", 4)]))
STUBS.internal(FREE, stub_free)
STUBS.internal(UPD, mkrec("UpdateDisplay", [("L", 3)]))
STUBS.internal(POST, mkrec("UI_PostMessage", [("L", 3), ("i", 4), ("i", 5), ("i", 6)]))
STUBS.internal(SEND, mkrec("UI_SendMessage", [("L", 3), ("i", 4), ("i", 5), ("i", 6)]))
STUBS.internal(GETWIN, stub_getwin)
STUBS.internal(SHOW, mkrec("Show", [("L", 3), ("h", 4)]))
STUBS.internal(MINMAX, mkrec("SetMinMax", [("L", 3), ("i", 4), ("i", 5)]))
STUBS.internal(GLUE, stub_glue)
STUBS.internal(STRDTOR, mkrec("WStringDtor", [("L", 3), ("h", 4)]))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    wsz = 4

    def wr(self, a, v, n):
        fmt = {1: "B", 2: "H", 4: "I", 8: "Q"}[n]
        self.write(a, struct.pack(self.end + fmt, v & ((1 << (8 * n)) - 1)))

    def rd(self, a, n):
        fmt = {1: "B", 2: "H", 4: "I", 8: "Q"}[n]
        return struct.unpack(self.end + fmt, self.read(a, n))[0]

    def fld(self, name, v):
        n = self.ps if name in PTRS else (2 if name in I16 else 4)
        self.wr(self.obj + self.off[name], v, n)

    def getf(self, name):
        n = 2 if name in I16 else 4
        v = self.rd(self.obj + self.off[name], n)
        return v - (1 << (8 * n)) if v >> (8 * n - 1) else v

    def prepare(self, case):
        """Allocates and fills the world; answers {label: address}."""
        w = case["world"]
        self.obj = self.alloc(0x400)
        fixed = {1: self.obj}
        self.bar = self.parent = self.draw = 0
        if w["bar"]:
            self.bar = self.alloc(0x200)
            fixed[2] = self.bar
        if w["parent"]:
            self.parent = self.alloc(0x200)
            fixed[3] = self.parent
        if w["draw"]:
            self.draw = self.alloc(0x100)
            fixed[4] = self.draw
        self.strs = []
        for k in range(w["nstr"]):
            a = self.alloc(8)
            self.strs.append(a)
            fixed[10 + k] = a
        n = len(w["words"])
        self.nwords = n
        self.blk = self.alloc(self.wsz * (n + 2 * GUARD))
        self.base = self.blk + self.wsz * GUARD
        for i in range(GUARD):
            self.wr(self.blk + self.wsz * i, 0x5A5A0000 + i, self.wsz)
            self.wr(self.base + self.wsz * (n + i), 0x6B6B0000 + i, self.wsz)
        for i, v in enumerate(w["words"]):
            if isinstance(v, tuple):
                v = self.strs[v[1]]
            self.wr(self.base + self.wsz * i, v, self.wsz)
        C_ = w["cols"]
        self.colarr = self.alloc(max(1, C_) * self.off["crstride"])
        for j, t in enumerate(w["types"]):
            self.wr(self.colarr + j * self.off["crstride"] + self.off["crbottom"], t, 2)
        self.fld("storage", self.base)
        self.fld("cur", w["cur"])
        self.fld("count", w["count"])
        self.fld("top", w["top"])
        self.fld("bw", w["bw"])
        self.fld("first", w["first"])
        self.fld("cols", C_)
        self.fld("colptr", self.colarr)
        self.fld("vis", w["vis"])
        self.fld("draw", self.draw)
        self.fld("parent", self.parent)
        self.setup_virtuals()
        self.set_auto(w["auto"])
        return fixed

    def block(self):
        out = []
        for i in range(-GUARD, self.nwords + GUARD):
            v = self.rd(self.base + self.wsz * i, self.wsz) & MASK
            for k, a in enumerate(self.strs):
                if v == (a & MASK):
                    v = ("S", k)
            out.append(v)
        return out


class Ppc(Side):
    end = ">"
    ps = 4

    def __init__(self, e, case):
        self.e = e
        self.off = MAC
        fixed = self.prepare(case)
        self.L = e.cl = CL.PpcLog({a: l for l, a in fixed.items()}, case["queues"])
        self.L.bar = self.bar
        self.saved = STUBS.activate(e)

    def setup_virtuals(self):
        if self.bar:
            vt = self.alloc(0x40)
            self.wr(vt + 0x24, VT_BAR, 4)
            self.wr(self.bar + 0x118, vt, 4)
        if self.draw:
            vt = self.alloc(0x40)
            self.wr(vt + 0x1C, VT_DO, 4)
            self.wr(self.draw + 0x38, vt, 4)

    def set_auto(self, v):
        self.e.write(GLOBAL_AUTO, struct.pack(">I", v & MASK))

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

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [self.obj] + [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    end = "<"
    ps = 8
    wsz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = dict(zip(HOST_NAMES, lib.layout))
        fixed = self.prepare(case)
        self.lib.tpw_ilc_setbar(self.bar)
        CL.host_begin(lib, fixed, case["queues"])

    def setup_virtuals(self):
        if self.bar:
            self.wr(self.bar, self.lib.tpw_ilc_vptr(0), 8)
        if self.draw:
            self.wr(self.draw, self.lib.tpw_ilc_vptr(1), 8)

    def set_auto(self, v):
        self.lib.tpw_ilc_setauto(s32(v))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ilc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        L = self.lib
        if fn == FN_ALL:
            L.tpw_ilc_remove_all(self.obj)
            return 0
        if fn == FN_ITEM:
            return L.tpw_ilc_remove_item(self.obj, s32(args[0])) & MASK
        L.tpw_ilc_remove_me(self.obj, *[s32(a) for a in args])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ilc_alloc.restype, lib.tpw_ilc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ilc_vptr.restype, lib.tpw_ilc_vptr.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_ilc_setbar.argtypes = [C.c_uint64]
    lib.tpw_ilc_setauto.argtypes = [C.c_int32]
    lib.tpw_ilc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ilc_remove_all.argtypes = [C.c_uint64]
    lib.tpw_ilc_remove_item.restype, lib.tpw_ilc_remove_item.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    lib.tpw_ilc_remove_me.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ilc_layout(out)
    assert n == len(HOST_NAMES), n
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def run(s, case, fn, args):
    try:
        ans = s.call(fn, args)
        res = {"answer": ans if fn == FN_ITEM else 0, "log": s.log(), "block": s.block()}
        for name in INTS:
            res[name] = s.getf(name)
        return res
    finally:
        s.done()


def make(side_cls, fn):
    def go(x, case):
        s = side_cls(x, case)
        return run(s, case, fn, case["args"])
    return go


# ---------------------------------------------------------------------------------------------------------------- case generators


def world(r, n=None, C_=None, vis=None, bar=None, draw=None, auto=None, T_=None, cur=None, top=None):
    C_ = r.choice([0, 1, 2, 3]) if C_ is None else C_
    S = C_ + 4
    T_ = r.randrange(2, 8) if T_ is None else T_
    n = r.choice([0, 1, 1, 2, 3, 4, 5, 6]) if n is None else n
    n = min(n, T_)
    types = [r.choice([0, 0, 1, 1, 2, -1]) for _ in range(C_)]
    slots = list(range(T_))
    r.shuffle(slots)
    used, free = slots[:n], slots[n:]
    par = []
    kids = [[] for _ in range(n)]
    roots = []
    for i in range(n):
        p = -1 if i == 0 or r.random() < 0.35 else r.randrange(i)
        par.append(p)
        (roots if p < 0 else kids[p]).append(i)
    nstr = 0
    ents = {}

    def cells(is_used):
        nonlocal nstr
        out = []
        for t in types:
            if t == 0 and is_used:
                out.append(("S", nstr))
                nstr += 1
            else:
                out.append(r.randrange(-1000, 1000) & MASK)
        return out

    def ent(slot):
        return slot * S
    for i in range(n):
        sibs = roots if par[i] < 0 else kids[par[i]]
        k = sibs.index(i)
        if k + 1 < len(sibs):
            nxt = ent(used[sibs[k + 1]])
        elif par[i] < 0:
            nxt = ent(free[0]) if free else -1
        else:
            nxt = -1
        child = ent(used[kids[i][0]]) if kids[i] else -1
        parent = ent(used[par[i]]) if par[i] >= 0 else -1
        ents[used[i]] = [nxt, r.randrange(0, 1000), child, parent] + cells(True)
    for j, slot in enumerate(free):
        nxt = ent(free[j + 1]) if j + 1 < len(free) else -1
        ents[slot] = [nxt, r.randrange(0, 1000), -1, -2] + cells(False)
    words = []
    for slot in range(T_):
        words += [v & MASK if isinstance(v, int) else v for v in ents[slot]]
    bw = T_ * S
    if r.random() < 0.2:
        bw = max(S, bw + r.randrange(-(S - 1), S))
    return {
        "cols": C_, "types": types, "words": words, "nstr": nstr, "bw": bw, "T": T_, "S": S, "n": n,
        "first": ent(used[0]) if n else -1, "count": n,
        "cur": r.choice([-1, 0, 1, n - 1, n, 2]) if cur is None else cur,
        "top": r.choice([0, 0, 1, 2, n]) if top is None else top,
        "vis": r.choice([1, 2, 3, 4, 6, -1, 0]) if vis is None else vis,
        "bar": r.choice([0, 1, 1, 1]) if bar is None else bar,
        "draw": r.choice([0, 1]) if draw is None else draw,
        "auto": r.choice([0, 1]) if auto is None else auto,
        "parent": r.choice([0, 1, 1]),
    }


def base(r, op, **kw):
    return {"op": op, "queues": {}, "world": world(r, **kw)}


def all_case(r, **kw):
    c = base(r, "all", **kw)
    c["args"] = []
    return c


def item_case(r, pos=None, **kw):
    c = base(r, "item", **kw)
    n = c["world"]["n"]
    c["args"] = [r.choice([-1, 0, 0, 1, n - 1, n, n + 1, r.randrange(0, n + 2), 100]) if pos is None else pos]
    return c


def me_case(r, **kw):
    c = base(r, "me", **kw)
    w = c["world"]
    S, T_ = w["S"], w["T"]
    entry = r.randrange(T_) * S
    prev = r.choice([-1, -1, r.randrange(T_) * S])
    c["args"] = [r.randrange(0, 5), entry, prev, r.choice([0, 1])]
    return c


def all_edges():
    r = random.Random(11)
    for n in (0, 1, 3, 6):
        for bar in (0, 1):
            for auto in (0, 1):
                for vis in (-1, 0, 3):
                    for draw in (0, 1):
                        yield all_case(r, n=n, C_=r.choice([0, 2, 3]), bar=bar, auto=auto, vis=vis, draw=draw)


def item_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 5, 6):
        for pos in (-1, 0, 1, 2, n - 1, n, n + 1):
            for auto in (0, 1):
                yield item_case(r, pos=pos, n=n, auto=auto, bar=r.choice([0, 1]))
    for vis in (-1, 1, 2, 5):
        for top in (0, 1, 3):
            for cur in (-1, 0, 1, 2, 4):
                for pos in (0, 1, 2, 3):
                    yield item_case(r, pos=pos, n=5, vis=vis, top=top, cur=cur, auto=1, bar=1)


def me_edges():
    r = random.Random(13)
    for flag in (0, 1):
        for n in (1, 2, 4, 6):
            for k in range(6):
                c = me_case(r, n=n)
                c["args"][3] = flag
                yield c


TESTS = [
    FnTest("InterfaceListControl2::RemoveAllItems", make(Ppc, FN_ALL), make(Host, FN_ALL), edge=all_edges, rnd=all_case),
    FnTest("InterfaceListControl2::RemoveItem", make(Ppc, FN_ITEM), make(Host, FN_ITEM), edge=item_edges, rnd=item_case),
    FnTest("InterfaceListControl2::RemoveMe", make(Ppc, FN_ME), make(Host, FN_ME), edge=me_edges, rnd=me_case),
]
