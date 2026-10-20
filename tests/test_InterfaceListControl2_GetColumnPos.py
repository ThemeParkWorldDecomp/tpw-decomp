"""Unit v5955, the list control's GetColumnPos, GetItem, SetData, GetNonHierIndexedEntry and UI_DefaultListControl2Callback, against
libtpw_unit_InterfaceListControl2_GetColumnPos.dylib (tests/unit_InterfaceListControl2_GetColumnPos.cpp), main app ("SimTheme Park"),
src/InterfaceListControl2_items_InterfaceListControl2_GetColumnPos.cpp:
  InterfaceListControl2::GetColumnPos 0x101d42ec, GetItem 0x101d40bc, SetData 0x101d3fcc, GetNonHierIndexedEntry 0x101d2c30,
  UI_DefaultListControl2Callback 0x101d51f0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, wider pointers and item words, other offsets) for the translation; the
same code reads both back through per-side offset tables. Every callee outside the unit is a fake on both sides that records its arguments:
AdvanceIndex, UpdateDisplay, ColumnButtonClick, ChangeCurEntry, PropagateMessage, SelectClick, SetCurrent, GetWindow, Show, GetAttr,
CaptureKeys, UI_PostMessage, UI_SendMessage, UI_DefaultWindowCallback. The virtual calls (the rectangles' PointIn and Shift, the scroll bar's
SetValue and GetValue, the draw object's Show) go through __ptr_glue, which the PowerPC side fakes from a cookie in the vtable slot and the
host side through hand-made vtables; they record the receiver and the arguments too.

The world of a case: the list object (attributes, held buttons, rectangles, counts, block, windows, columns, draw object, parent),
gUIAutoRedraw, the scroll bar, an item block with a guard area before it (an entry of -2 writes there) and the answer queues of the fakes.
What is compared: the answer, the out parameters, the data words of the block (the string cells aside, which are pointers and are not
changed), mTopEntry, mCurrentEntry and the event log.

No fused operations. Not tested: a negative column (GetColumnPos and GetItem read before the column array, whose records are wider on the host),
a block pointer of NULL with an entry (the original dereferences NULL), a negative column count with a callback shift (loops 2^32 times).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceListControl2_GetColumnPos"
MASK = 0xFFFFFFFF

FN_COLPOS, FN_GETITEM, FN_SETDATA, FN_NONHIER, FN_CALLBACK = 0x101D42EC, 0x101D40BC, 0x101D3FCC, 0x101D2C30, 0x101D51F0
SIZES = {FN_COLPOS: 88, FN_GETITEM: 244, FN_SETDATA: 184, FN_NONHIER: 244, FN_CALLBACK: 1576}
OWN = set(SIZES)

CALLEES = dict(
    AdvanceIndex=0x101D2A84, ChangeCurEntry=0x101D4854, ColumnButtonClick=0x101D4FA4, PropagateMessage=0x101D4650,
    SelectClick=0x101D4A50, SetCurrent=0x101D4C64, UpdateDisplay=0x101D2220, CaptureKeys=0x101DCE4C, GetAttr=0x101DBB70,
    GetWindow=0x101DBE40, Show=0x101DC1EC, UI_DefaultWindowCallback=0x101DE3F8, UI_PostMessage=0x101C9400, UI_SendMessage=0x101C9518)
GLUE = 0x1023BAA8

ADDR = dict(auto=0x10395C28, keys=0x102AFBCC)

# the Mac's offsets (the host's come from tpw_ilc2_layout, in this order)
KEYS = ["client", "attrs", "parent", "id", "held", "lrect", "store", "wins", "cur", "count", "top", "first", "ncols", "cols", "rowh", "vis", "draw"]
MAC = dict(client=0x28, attrs=0x44, parent=0x5C, id=0x88, held=0x11C, lrect=0x138, store=0x144, wins=0x148, cur=0x14C, count=0x150,
           top=0x154, first=0x160, ncols=0x164, cols=0x16C, rowh=0x170, vis=0x172, draw=0x174)
HOST = {}

# the cookies the PowerPC vtables hold in the slots the code calls through (the glue stub dispatches on them)
C_POINTIN, C_SHIFT, C_SETVALUE, C_GETVALUE, C_DOSHOW = 0x7001, 0x7002, 0x7003, 0x7004, 0x7005
NENT, NW, PAD = 4, 40, 16


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, v):
    return e.cl.lab(v & MASK)


def mk(name, fields):
    """A fake that records (label of r3, then the fields: 'i' s32, 'h' s16 of r4, r5, r6) and answers r3 = 0."""
    def fn(e):
        L = e.cl
        vals = [lab(e, e.r(3))]
        for k, f in enumerate(fields):
            r = e.r(4 + k)
            vals.append(s32(r) if f == "i" else s16(r) if f == "h" else lab(e, r))
        L.rec(name, tuple(vals))
        e.set_r(3, 0)
    return fn


def st_advance(e):
    L = e.cl
    L.rec("AdvanceIndex", (lab(e, e.r(3)), s32(e.r(4)), 1 if e.r(5) else 0))
    e.set_r(3, L.ret("AdvanceIndex") & MASK)


def st_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (lab(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cs["bar"] if L.ret("GetWindow") else 0)


def st_getattr(e):
    L = e.cl
    L.rec("GetAttr", (lab(e, e.r(3)),))
    e.set_r(3, L.ret("GetAttr") & MASK)


def st_default(e):
    L = e.cl
    L.rec("UI_DefaultWindowCallback", (lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


def st_glue(e):
    """__ptr_glue: r12 holds the cookie of the virtual member, r3 the receiver."""
    L = e.cl
    c, r3 = e.r(12), e.r(3)
    if c == C_POINTIN:
        L.rec("PointIn", (lab(e, r3), s16(e.r(4)), s16(e.r(5))))
        e.set_r(3, L.ret("PointInC" if lab(e, r3) == 31 else "PointInL") & MASK)
    elif c == C_SHIFT:
        L.rec("Shift", (lab(e, r3), s16(e.r(4)), s16(e.r(5))))
    elif c == C_SETVALUE:
        L.rec("SetValue", (lab(e, r3), s32(e.r(4))))
    elif c == C_GETVALUE:
        L.rec("GetValue", (lab(e, r3),))
        e.set_r(3, L.ret("GetValue") & MASK)
    elif c == C_DOSHOW:
        L.rec("DOShow", (lab(e, r3), s16(e.r(4))))
    else:
        raise AssertionError("unknown virtual cookie %#x" % c)


STUBS.internal(CALLEES["AdvanceIndex"], st_advance)
STUBS.internal(CALLEES["UpdateDisplay"], mk("UpdateDisplay", ""))
STUBS.internal(CALLEES["ColumnButtonClick"], mk("ColumnButtonClick", "i"))
STUBS.internal(CALLEES["ChangeCurEntry"], mk("ChangeCurEntry", "i"))
STUBS.internal(CALLEES["PropagateMessage"], mk("PropagateMessage", "iii"))
STUBS.internal(CALLEES["SelectClick"], mk("SelectClick", "hh"))
STUBS.internal(CALLEES["SetCurrent"], mk("SetCurrent", "i"))
STUBS.internal(CALLEES["CaptureKeys"], mk("CaptureKeys", "l"))
STUBS.internal(CALLEES["GetAttr"], st_getattr)
STUBS.internal(CALLEES["GetWindow"], st_getwindow)
STUBS.internal(CALLEES["Show"], mk("Show", "h"))
STUBS.internal(CALLEES["UI_DefaultWindowCallback"], st_default)
STUBS.internal(CALLEES["UI_PostMessage"], mk("UI_PostMessage", "iii"))
STUBS.internal(CALLEES["UI_SendMessage"], mk("UI_SendMessage", "iii"))
STUBS.internal(GLUE, st_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def i32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def i16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def rd16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def rect(self, a, l, t, r, b):
        for k, v in zip(self.cr[1:], (l, t, r, b)):
            self.i16(a + k, v)

    def labels(self, A):
        """(label, address) of every object the log names."""
        out = [(1, A["this"]), (2, A["parent"]), (3, A["bar"]), (4, A["draw"]), (30, A["this"] + self.off["lrect"]),
               (31, A["this"] + self.off["client"]), (40, A["keys"])]
        out += [(10 + i, A["wins"][i]) for i in range(4)]
        out += [(20 + i, A["cols"] + i * self.cr[0]) for i in range(4)]
        return out

    def make_world(self, case):
        A = {}
        A["this"] = self.alloc(self.thissize)
        A["parent"] = self.alloc(0x20)
        A["bar"] = self.alloc(0x140)
        A["draw"] = self.alloc(0x40)
        A["wins"] = [self.alloc(0x20) for _ in range(4)]
        A["winarr"] = self.alloc(4 * self.PS)
        A["cols"] = self.alloc(4 * self.cr[0])
        A["blockmem"] = self.alloc((PAD + NW) * self.W)
        A["block"] = A["blockmem"] + PAD * self.W
        A["textbuf"] = self.alloc(128)
        A["out"] = self.alloc(16)
        A["sl"] = self.alloc(8)
        A["sr"] = self.alloc(8)
        A["keys"] = self.g("keys")
        self.A = A


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    pfmt = ">I"
    W = 4
    PS = 4
    off = MAC
    thissize = 0x180
    cr = (12, 4, 6, 8, 10)
    repdata = 0xC
    repsize = 0x10

    def __init__(self, e, case):
        self.e = e
        e.cs = {}
        self.make_world(case)
        self.L = e.cl = CL.PpcLog({addr: label for label, addr in self.labels(self.A)}, case["queues"])
        e.cs["bar"] = self.A["bar"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def vtables(self):
        A = self.A
        vt = self.alloc(0x40)
        for off, cookie in ((8, C_POINTIN), (0x10, C_SHIFT), (0x1C, C_DOSHOW), (0x24, C_SETVALUE), (0x28, C_GETVALUE)):
            self.ptr(vt + off, cookie)
        self.ptr(A["this"] + self.off["client"], vt)
        self.ptr(A["this"] + self.off["lrect"], vt)
        for i in range(4):
            self.ptr(A["cols"] + i * self.cr[0], vt)
        self.ptr(A["bar"] + 0x118, vt)
        self.ptr(A["draw"] + 0x38, vt)

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST["off"]
        self.thissize, self.cr, self.repdata, self.repsize, self.W = HOST["rest"]
        self.PS = 8
        self.make_world(case)
        self.lib.tpw_ilc2_setbar(self.A["bar"])
        CL.host_begin(lib, {label: addr for label, addr in self.labels(self.A)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ilc2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ilc2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def vtables(self):
        A, vt = self.A, self.lib.tpw_ilc2_vt
        vt(A["this"] + self.off["client"], 0)
        vt(A["this"] + self.off["lrect"], 0)
        for i in range(4):
            vt(A["cols"] + i * self.cr[0], 0)
        vt(A["bar"], 1)
        vt(A["draw"], 2)

    def call(self, fn, args):
        f = {FN_COLPOS: self.lib.tpw_ilc2_colpos, FN_GETITEM: self.lib.tpw_ilc2_getitem, FN_SETDATA: self.lib.tpw_ilc2_setdata,
             FN_NONHIER: self.lib.tpw_ilc2_nonhier, FN_CALLBACK: self.lib.tpw_ilc2_callback}[fn]
        kinds = {FN_COLPOS: "pipp", FN_GETITEM: "piip", FN_SETDATA: "pii", FN_NONHIER: "pii", FN_CALLBACK: "piii"}[fn]
        return f(*[a if k == "p" else s32(a) for k, a in zip(kinds, args)]) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout is read here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_ilc2_"
    lib.tpw_ilc2_addr.restype, lib.tpw_ilc2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ilc2_alloc.restype, lib.tpw_ilc2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ilc2_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ilc2_setbar.argtypes = [C.c_uint64]
    lib.tpw_ilc2_vt.argtypes = [C.c_uint64, C.c_int32]
    u64, i32 = C.c_uint64, C.c_int32
    for name, res, args in (("colpos", C.c_uint32, [u64, i32, u64, u64]), ("getitem", C.c_uint32, [u64, i32, i32, u64]),
                            ("setdata", C.c_uint32, [u64, i32, i32]), ("nonhier", C.c_int32, [u64, i32, i32]),
                            ("callback", C.c_int32, [u64, i32, i32, i32])):
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_ilc2_layout(out)
    v = list(out)[:n]
    assert n == 26, v
    HOST["off"] = dict(zip(KEYS, v[:17]))
    HOST["rest"] = (v[17], tuple(v[18:23]), v[23], v[24], v[25])
    assert v[25] == 8 and v[18] >= 12 and v[17] > max(v[:17]), v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's list, block and objects through Side: the same code for both memories."""
    w, A, o = case["w"], s.A, s.off
    t = A["this"]
    s.i32(t + o["attrs"], w["attrs"])
    s.ptr(t + o["parent"], A["parent"] if w["hasparent"] else 0)
    s.i32(t + o["id"], w["id"])
    s.i32(t + o["held"], w["held"])
    s.rect(t + o["lrect"], *w["lrect"])
    s.ptr(t + o["store"], A["block"] if w["hasstore"] else 0)
    s.ptr(t + o["wins"], A["winarr"] if w["haswins"] else 0)
    s.i32(t + o["cur"], w["cur"])
    s.i32(t + o["count"], w["count"])
    s.i32(t + o["top"], w["top"])
    s.i32(t + o["first"], w["first"])
    s.i16(t + o["ncols"], w["ncols"])
    s.ptr(t + o["cols"], A["cols"] if w["hascols"] else 0)
    s.i16(t + o["rowh"], w["rowh"])
    s.i16(t + o["vis"], w["vis"])
    s.ptr(t + o["draw"], A["draw"] if w["hasdraw"] else 0)
    s.i32(s.g("auto"), w["auto"])
    for i in range(4):
        s.ptr(A["winarr"] + i * s.PS, 0 if w["winnull"][i] else A["wins"][i])
        s.rect(A["cols"] + i * s.cr[0], *w["cols"][i])
    s.vtables()
    # the item block: entries of stride ncols + 4 words (links, data, child, parent, cells)
    stride = max(w["ncols"], 0) + 4
    A["textpos"] = set()
    base = A["block"]
    for idx in range(-PAD, NW):                       # fill with a pattern first, so a stray write shows
        s.i32(base + idx * s.W, 0x5A5A0000 + (idx & 0xFFFF))
    for k in range(NENT):
        e = k * stride
        nxt, data, child, par = w["ents"][k]
        for j, v in enumerate((nxt, data, child, par)):
            s.i32(base + (e + j) * s.W, v)
        for c in range(max(w["ncols"], 0)):
            cell = e + 4 + c
            if w["cols"][c][3] == 0:                  # a text cell: a string, its representation, the characters
                obj = s.alloc(s.PS)
                rep = s.alloc(s.repsize)
                s.ptr(rep + s.repdata, A["textbuf"] + 2 * (k * 8 + c))
                s.ptr(obj, rep)
                s.ptr(base + cell * s.W, obj)
                A["textpos"].add(cell)
            else:
                s.i32(base + cell * s.W, w["nums"][k][c])
    s.write(A["out"], b"\xa5" * 8)
    s.i16(A["sl"], 0x7777)
    s.i16(A["sr"], 0x5555)


def execute(s, case):
    A, op, a = s.A, case["op"], case["args"]
    t = A["this"]
    if op == "colpos":
        return s.call(FN_COLPOS, [t, a[0], A["sl"] if a[1] else 0, A["sr"] if a[2] else 0])
    if op == "getitem":
        s.call(FN_GETITEM, [t, a[0], a[1], A["out"]])   # declared void in the header (ContactMenu's unit): the answer is not compared
        return 0
    if op == "setdata":
        s.call(FN_SETDATA, [t, a[0], a[1]])   # the header declares SetData void (ColumnClick's unit needs that): the answer is not compared
        return 0
    if op == "nonhier":
        return s.call(FN_NONHIER, [t, a[0], a[1]])
    return s.call(FN_CALLBACK, [t, a[0], a[1], a[2]])


def observe(s, case, answer):
    A, o = s.A, s.off
    v = struct.unpack(s.pfmt, s.read(A["out"], s.PS))[0]
    tb = A["textbuf"]
    out = ("T", v - tb) if tb <= v < tb + 128 else v & MASK
    block = [s.rd32(A["block"] + i * s.W) for i in range(-PAD, NW) if i not in A["textpos"]]
    return {"answer": answer, "log": s.log(), "top": s.rd32(A["this"] + o["top"]), "cur": s.rd32(A["this"] + o["cur"]),
            "count": s.rd32(A["this"] + o["count"]), "out": out, "sl": s.rd16(A["sl"]), "sr": s.rd16(A["sr"]), "block": block}


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def world(r, **over):
    """A random list; `over` replaces fields."""
    ncols = r.choice([0, 1, 2, 3, 4])
    w = {
        "ncols": ncols,
        "hascols": r.random() < 0.85,
        "cols": [(r.randrange(-300, 300), r.choice([0, 0, 3]), r.randrange(-300, 300), r.choice([0, 1, 1, 0, 5, 2])) for _ in range(4)],
        "attrs": r.choice([0, 0x80, 0x200, 0x280, 0x80, 0x280, r.getrandbits(32), 0x7F]),
        "held": r.choice([0, 0, 0, 1, 4]),
        "id": r.randrange(-5, 0x20000),
        "hasparent": r.random() < 0.9,
        "lrect": (r.randrange(-50, 50), r.randrange(-50, 50), r.randrange(0, 300), r.randrange(0, 300)),
        "hasstore": r.random() < 0.85,
        "haswins": r.random() < 0.8,
        "winnull": [r.random() < 0.25 for _ in range(4)],
        "cur": r.choice([-1, -1, 0, 1, 3, 7, -5]),
        "count": r.choice([0, 1, 2, 3, 4, 6]),
        "top": r.choice([0, 1, 2, 5, -3]),
        "rowh": r.choice([0, 1, 5, 16, 16, -3]),
        "vis": r.choice([0, 1, 2, 3, 4, 4, -1]),
        "hasdraw": r.random() < 0.8,
        "auto": r.choice([0, 1, 2]),
    }
    w.update(over)
    stride = max(w["ncols"], 0) + 4
    valid = [k * stride for k in range(NENT)]
    pool = valid + [-1, -2]
    w["valid"] = valid
    w["ents"] = [(r.choice(pool), r.randrange(-1000, 1000), r.choice(pool), r.choice(pool)) for _ in range(NENT)]
    w["nums"] = [[r.randrange(-100000, 100000) for _ in range(4)] for _ in range(NENT)]
    if "first" not in over:
        w["first"] = r.choice(valid + [-1, -2, 0, 0])
    if not w["hasstore"]:
        w["first"] = -1
    return w


def queues(r, w, **over):
    pool = w["valid"] + [-1, -2, 0]
    q = {
        "AdvanceIndex": [r.choice(pool) for _ in range(6)],
        "GetWindow": [r.choice([0, 1, 1]) for _ in range(3)],
        "GetAttr": [r.choice([0, 0x200, 0x200, 1])],
        "Default": [r.randrange(-5, 5) for _ in range(2)],
        "PointInC": [r.choice([0, 1, 1]) for _ in range(3)],
        "PointInL": [r.choice([0, 1, 1]) for _ in range(3)],
        "GetValue": [r.choice([w["top"], w["top"] + 1, r.randrange(-3, 10)]) for _ in range(3)],
    }
    q.update(over)
    return q


def mkcase(r, op, args, over=None, qover=None):
    w = world(r, **(over or {}))
    return {"op": op, "w": w, "args": args, "queues": queues(r, w, **(qover or {}))}


# ---- GetColumnPos
def colpos_case(r, col=None, hl=None, hr=None, over=None):
    w0 = world(r, **(over or {}))
    n = w0["ncols"]
    col = r.choice([0, 1, 2, 3, n - 1, n, n + 1]) if col is None else col
    col = max(col, 0)
    return mkcase(r, "colpos", [col, r.random() < 0.9 if hl is None else hl, r.random() < 0.9 if hr is None else hr], over)


def colpos_edges():
    r = random.Random(5955)
    for hascols in (True, False):
        for ncols in (0, 1, 3, -1):
            for col in (0, 1, 2, 3):
                for hl, hr in ((1, 1), (0, 1), (1, 0), (0, 0)):
                    yield mkcase(r, "colpos", [col, hl, hr], dict(hascols=hascols, ncols=ncols))
    for col in (0, 3):
        yield mkcase(r, "colpos", [col, 1, 1], dict(hascols=True, ncols=4))


def colpos_random(r):
    return colpos_case(r)


# ---- GetItem
def getitem_case(r, pos=None, col=None, over=None):
    over = dict(over or {})
    over.setdefault("hascols", True)
    w0 = world(r, **over)
    n = w0["ncols"]
    pos = r.choice([-1, 0, 1, 2, 3, 5]) if pos is None else pos
    col = r.choice([0, 1, 2, 3, n - 1, n, n + 1, 0, 0]) if col is None else col
    return mkcase(r, "getitem", [pos, max(col, 0)], over)


def getitem_edges():
    r = random.Random(5956)
    for pos in (-2, -1, 0, 1, 2, 3, 4):
        for col in (0, 1, 2, 3, 4):
            for ncols in (1, 3, 4):
                yield getitem_case(r, pos, col, dict(ncols=ncols, count=3, hasstore=True))
    yield getitem_case(r, 0, 0, dict(hasstore=False))
    yield getitem_case(r, 2, 1, dict(hasstore=True, count=1))


def getitem_random(r):
    return getitem_case(r)


# ---- SetData
def setdata_case(r, pos=None, data=None, over=None):
    pos = r.choice([-1, 0, 1, 2, 3, 5]) if pos is None else pos
    data = r.choice([0, -1, 12345, r.randrange(-2 ** 31, 2 ** 31)]) if data is None else data
    return mkcase(r, "setdata", [pos, data], over)


def setdata_edges():
    r = random.Random(5957)
    for pos in (-2, -1, 0, 1, 2, 3, 4):
        for first in (0, -1, -2, 8):
            yield setdata_case(r, pos, 0x1234567, dict(count=3, first=first, hasstore=True, ncols=4))
    yield setdata_case(r, 1, 77, dict(hasstore=False))
    yield setdata_case(r, 0, -77, dict(hasstore=True, count=2, first=0, ncols=0))


def setdata_random(r):
    return setdata_case(r)


# ---- GetNonHierIndexedEntry
def nonhier_case(r, pos=None, parent=None, over=None):
    pos = r.choice([-1, 0, 1, 2, 3, 6]) if pos is None else pos
    parent = r.choice([-3, -2, -1, 0, 1, 2, 5]) if parent is None else parent
    return mkcase(r, "nonhier", [pos, parent], over)


def nonhier_edges():
    r = random.Random(5958)
    for parent in (-3, -2, -1, 0, 1, 2, 3, 4):
        for pos in (-2, -1, 0, 1, 2, 3, 4):
            for count in (0, 3):
                yield nonhier_case(r, pos, parent, dict(count=count, hasstore=True))
    yield nonhier_case(r, 1, 1, dict(hasstore=False))
    yield nonhier_case(r, 2, -1, dict(hasstore=False))


def nonhier_random(r):
    return nonhier_case(r)


# ---- UI_DefaultListControl2Callback
MSGS = [0x800, 0x1A, 0x15, 0x13, 0x11, 0x100, 0x1E, 0x1000A, 0x10000, 0x10001, 0x10002, 0x10003, 0x10004, 0x11005, 0x11006, 0x11007,
        0x11008, 0x1000B, 0x1000C, 0x1000D, 0x801, 0x14, 0x16, 0x19, 0x12, 0x10, 0x101, 0x1B, 0x1F, 0x0, 3, 0xFF, 0x7FFFFFFF, -1, 0x12345]
KEYS_A = [0x2600, 0x2400, 0x2300, 0x2800, 0x2700, 0x2500, 0x12600, 0xFFFF2800, 0, 0x2200, 0x2900]


def pos(r):
    """A packed pointer position x << 16 | y with y around the list's top."""
    x = r.choice([0, 5, -3, 100, 32767, -32768])
    y = r.choice([-60, -1, 0, 1, 7, 16, 40, 100, 32767, -32768, r.randrange(-100, 300)])
    return ((x << 16) | (y & 0xFFFF)) & MASK


def callback_case(r, msg=None, a=None, b=None, over=None, qover=None):
    msg = r.choice(MSGS) if msg is None else msg
    if a is None:
        if msg == 0x1000A:
            a = r.choice(KEYS_A)
        elif msg in (0x10001, 0x10003):
            a = pos(r)
        elif msg in (0x11006, 0x11007):
            a = r.choice([0, 0, 1, 2, -1, 0x10000])
        else:
            a = r.choice([0, 1, -1, 5, 0x10000, 0x7FFF, -32768, r.randrange(-1000, 1000), r.getrandbits(32)])
    if b is None:
        b = pos(r) if msg == 0x11006 else r.choice([0, 1, -1, 7, r.randrange(-1000, 1000), r.getrandbits(32)])
    return mkcase(r, "callback", [msg & MASK, a & MASK, b & MASK], over, qover)


def callback_edges():
    r = random.Random(5959)
    for msg in MSGS:
        for _ in range(4):
            yield callback_case(r, msg)
    for key in KEYS_A:
        for attrs in (0, 0x200, 0x80):
            yield callback_case(r, 0x1000A, key, 0, dict(attrs=attrs, count=5, cur=3))
    # pointer messages over every gate of the click path
    for msg in (0x10001, 0x10003):
        for attrs in (0, 0x80):
            for held in (0, 2):
                for rowh in (0, 16, -3):
                    for pc in (0, 1):
                        for pl in (0, 1):
                            for y in (-1, 0, 5, 16, 40, 100):
                                yield callback_case(r, msg, ((5 << 16) | (y & 0xFFFF)), 0,
                                                    dict(attrs=attrs, held=held, rowh=rowh, lrect=(0, 20, 100, 200)),
                                                    dict(PointInC=[pc], PointInL=[pl]))
    for a in (0, 1):
        for attrs in (0, 0x80):
            for rowh in (0, 16):
                for pl in (0, 1):
                    for y in (0, 25, 40, 100):
                        for cur in (-1, 0, 4):
                            yield callback_case(r, 0x11006, a, ((9 << 16) | y), dict(attrs=attrs, rowh=rowh, cur=cur, lrect=(0, 20, 100, 200)),
                                                dict(PointInL=[pl]))
    for a in (0, 1):
        for cur in (-1, 0, 2):
            yield callback_case(r, 0x11007, a, 0, dict(cur=cur))
    # the scroll bar
    for top in (0, 3):
        for count in (0, 1, 10):
            for vis in (0, 2, 4):
                for gv in (top, top + 1):
                    for auto in (0, 1):
                        yield callback_case(r, 0x800, 0, 0, dict(top=top, count=count, vis=vis, auto=auto), dict(GetWindow=[1], GetValue=[gv]))
    yield callback_case(r, 0x800, 0, 0, dict(), dict(GetWindow=[0]))
    for a in (0, 1, -5, 10):
        yield callback_case(r, 0x1000D, a, 0, dict(), dict(GetWindow=[1], GetValue=[7]))
    yield callback_case(r, 0x1000D, 3, 0, dict(), dict(GetWindow=[0]))
    # shown / hidden
    for a in (0, 1, 0x10000, 0x10001):
        for vis in (0, 1, 4):
            for haswins in (True, False):
                for cur in (-1, 2):
                    yield callback_case(r, 0x11, a, 0, dict(vis=vis, haswins=haswins, cur=cur, count=3, top=1))
    for ncols in (0, 1, 4):
        for hascols in (True, False):
            yield callback_case(r, 0x13, 0x10003, -2, dict(ncols=ncols, hascols=hascols))
    for attrs in (0, 0x200):
        for ga in (0, 0x200, 0x300):
            yield callback_case(r, 0x1A, 0, 0, dict(attrs=attrs), dict(GetAttr=[ga]))
            yield callback_case(r, 0x15, 0, 0, dict(attrs=attrs), dict(GetAttr=[ga]))


def callback_random(r):
    return callback_case(r)


TESTS = [
    FnTest("InterfaceListControl2::GetColumnPos", ppc, host, edge=colpos_edges, rnd=colpos_random),
    FnTest("InterfaceListControl2::GetItem", ppc, host, edge=getitem_edges, rnd=getitem_random),
    FnTest("InterfaceListControl2::SetData", ppc, host, edge=setdata_edges, rnd=setdata_random),
    FnTest("InterfaceListControl2::GetNonHierIndexedEntry", ppc, host, edge=nonhier_edges, rnd=nonhier_random),
    FnTest("UI_DefaultListControl2Callback", ppc, host, edge=callback_edges, rnd=callback_random),
]
