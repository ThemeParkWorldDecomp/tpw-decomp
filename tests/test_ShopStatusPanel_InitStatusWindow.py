"""Unit v5936, the shop status panel's figure window, against libtpw_unit_ShopStatusPanel_InitStatusWindow.dylib
(tests/unit_ShopStatusPanel_InitStatusWindow.cpp), main app ("SimTheme Park"),
src/ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges_ShopStatusPanel_InitStatusWindow.cpp:
  ShopStatusPanel::InitStatusWindow 0x101bc6c4, ShopStatusPanel::UpdateWindow 0x101bd730

A case is a panel and a call. The panel, its windows (blocks tagged at +0x138 with their id), the shop object and the two fake
virtual tables (the panel's second table and each window's table at +0x118) are built twice by one piece of Python code. The callees
outside the unit (GetWindow, SetClientDrawObject, SetFont, SetClientDrawText, Show, SetState, the CObject getters, GetFontTableEntry,
UIMain_GetString, UIMain_ConvertMulti, UIMain_IndentClient, operator new, the three constructors and every call through __ptr_glue)
are fakes on both sides that record their arguments; the getters answer from the case's queues.

What is compared: the whole event log. Not tested: a NULL window from GetWindow where the original dereferences it unconditionally;
the Slot5/Slot6 panel argument cannot be seen on the host (the header's virtuals take none), so the PowerPC stub logs whether r3 was
the panel and the host logs 1. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShopStatusPanel_InitStatusWindow"
MASK = 0xFFFFFFFF

FN_INIT, FN_UPDATE = 0x101bc6c4, 0x101bd730
SIZES = {FN_INIT: 1892, FN_UPDATE: 600}
OWN = set(SIZES)

A_GETWINDOW, A_SETOBJ, A_GETFONT, A_SETFONT = 0x101dbe40, 0x101db624, 0x10180270, 0x101da118
A_GETSTRING, A_SETTEXT, A_SHOW, A_SETSTATE = 0x10180468, 0x101de130, 0x101dc1ec, 0x101cb1d0
A_ISCLOSED, A_QUALITY, A_AMOUNT, A_INGNAME, A_PRICE = 0x1010cae8, 0x1010fb38, 0x1010fc7c, 0x1010fcc4, 0x1010f4f4
A_CONVERT, A_INDENT = 0x101803cc, 0x10180940
A_SPRITE, A_CASH, A_GRADIENT, A_NEW, A_GLUE = 0x1018d11c, 0x1018de60, 0x101898c8, 0x102384b8, 0x1023baa8
MESH = 0x1024e580
STAFF = 0x103947a4

OPS_TOK, WIN_TOK = 0x71000000, 0x72000000
IDS = list(range(0xc070, 0xc087))
OBJ_TAG = 0x777
HOST_LAYOUT = None   # filled by bind

CUR = [None]


def s32(v):
    return T.s32(v)


def ident(p):
    s = CUR[0]
    p &= MASK
    if p == 0:
        return 0
    if p in s.objs:
        return 100 + s.objs[p]
    return s32(s.e.u32(p + 0x138))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(addr, name, args, ret=None):
    """A stub: logs (name, args(e)) and sets r3 from ret(e) if given."""
    def f(e):
        e.cl.rec(name, tuple(args(e)))
        if ret is not None:
            e.set_r(3, ret(e) & MASK)
    STUBS.internal(addr, f)


def rr(e, i):
    return s32(e.r(i))


def get_window(e):
    s = CUR[0]
    i = rr(e, 4)
    e.cl.rec("GetWindow", (ident(e.r(3)), i))
    e.set_r(3, s.win.get(i, 0))


STUBS.internal(A_GETWINDOW, get_window)
mk(A_SETOBJ, "SetObj", lambda e: (ident(e.r(3)), ident(e.r(4))))
mk(A_GETFONT, "GetFont", lambda e: (rr(e, 3),), lambda e: 0x5000 + e.r(3))
mk(A_SETFONT, "SetFont", lambda e: (ident(e.r(3)), rr(e, 4)))
mk(A_GETSTRING, "GetString", lambda e: (rr(e, 3),), lambda e: 0x6000 + e.r(3))
mk(A_SETTEXT, "SetText", lambda e: (ident(e.r(3)), rr(e, 4)))
mk(A_SHOW, "Show", lambda e: (ident(e.r(3)), T.s16(e.r(4))))
mk(A_SETSTATE, "SetState", lambda e: (ident(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
mk(A_ISCLOSED, "IsClosed", lambda e: (ident(e.r(3)),), lambda e: e.cl.ret("IsClosed"))
mk(A_QUALITY, "Quality", lambda e: (ident(e.r(3)),), lambda e: e.cl.ret("Quality"))
mk(A_AMOUNT, "Amount", lambda e: (ident(e.r(3)),), lambda e: e.cl.ret("Amount"))
mk(A_INGNAME, "IngName", lambda e: (ident(e.r(3)),), lambda e: e.cl.ret("IngName"))
mk(A_PRICE, "Price", lambda e: (ident(e.r(3)),), lambda e: e.cl.ret("Price"))
mk(A_CONVERT, "ConvertMulti", lambda e: (rr(e, 3),), lambda e: e.cl.ret("ConvertMulti"))
mk(A_INDENT, "Indent", lambda e: (ident(e.r(3)), T.s16(e.r(4)), T.s16(e.r(5))))
mk(A_SPRITE, "SpriteCtor", lambda e: (ident(e.r(3)), rr(e, 4)))
mk(A_CASH, "CashCtor", lambda e: (ident(e.r(3)), rr(e, 4)))
mk(A_GRADIENT, "GradientCtor", lambda e: (ident(e.r(3)), rr(e, 4) - MESH))


def op_new(e):
    s = CUR[0]
    size = e.r(3)
    e.cl.rec("OperatorNew", (size,))
    a = e.alloc(size)
    s.objs[a] = len(s.objs)
    e.set_r(3, a)


STUBS.internal(A_NEW, op_new)


def glue(e):
    s = CUR[0]
    t = e.r(12)
    L = e.cl
    flag = 1 if e.r(3) == s.panel else 0
    if t == OPS_TOK + 0x1c:
        L.rec("Ops5", (flag,))
    elif t == OPS_TOK + 0x20:
        L.rec("Ops6", (flag,))
    elif t == OPS_TOK + 0x3c:
        L.rec("Ops13", (flag,))
        e.set_r(3, L.ret("Ops13") & MASK)
    elif t == WIN_TOK + 0x24:
        L.rec("WSetValue", (ident(e.r(3)), rr(e, 4)))
    elif t == WIN_TOK + 0x2c:
        L.rec("WSetText", (ident(e.r(3)), rr(e, 4)))
    else:
        L.rec("BadGlue", (t,))


STUBS.internal(A_GLUE, glue)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.objs = {}
        self.win = {}
        self.panel = 0
        CUR[0] = self
        self.saved = STUBS.activate(e)
        self.off = dict(win=0xc, stat=0x10, w1c=0x1c, ops=0x20)
        self.psize = 0x30

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def p32(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def win_block(self, tag):
        a = self.alloc(0x140)
        self.words(a + 0x138, [tag])
        self.words(a + 0x118, [self.wtable])
        return a

    def set_window(self, i, a):
        self.win[i] = a

    def ops_ptr(self):
        return self.otable

    def prepare(self):
        self.otable = self.alloc(0x40)
        self.words(self.otable, [OPS_TOK + 4 * i for i in range(16)])
        self.wtable = self.alloc(0x40)
        self.words(self.wtable, [WIN_TOK + 4 * i for i in range(16)])

    def global_staff(self):
        return STAFF

    def call(self, op):
        entry = FN_INIT if op == "init" else FN_UPDATE
        T.call_ppc(self.e, entry, [self.panel])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ssiw_reset()
        CL.host_begin(lib, {}, case["queues"])
        self.off = dict(win=HOST_LAYOUT[0], stat=HOST_LAYOUT[1], w1c=HOST_LAYOUT[2] + 8, ops=HOST_LAYOUT[3])
        self.psize = HOST_LAYOUT[4]
        self.panel = 0

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ssiw_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, a, v):   # the panel's pointer fields are Ptr32 (arena offsets, unit v5L31)
        self.words(a, [self.lib.tpw_ssiw_off(v)])

    def log(self):
        return CL.host_end(self.lib)

    def win_block(self, tag):
        a = self.alloc(0x140)
        self.words(a + 0x138, [tag])
        return a

    def set_window(self, i, a):
        self.ptr(self.lib.tpw_ssiw_addr(b"winmap") + 8 * (i - 0xc000), a)

    def ops_ptr(self):
        return self.lib.tpw_ssiw_addr(b"ops")

    def prepare(self):
        pass

    def global_staff(self):
        return self.lib.tpw_ssiw_addr(b"staff")

    def call(self, op):
        (self.lib.tpw_ssiw_init if op == "init" else self.lib.tpw_ssiw_update)(self.panel)


def bind(lib):
    global HOST_LAYOUT
    if getattr(lib, "bound", False):
        return
    lib.tpw_ssiw_addr.restype, lib.tpw_ssiw_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ssiw_alloc.restype, lib.tpw_ssiw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ssiw_off.restype, lib.tpw_ssiw_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ssiw_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ssiw_reset.restype, lib.tpw_ssiw_reset.argtypes = None, []
    for n in ("init", "update"):
        f = getattr(lib, "tpw_ssiw_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ssiw_layout(out)
    HOST_LAYOUT = list(out)[:n]
    assert HOST_LAYOUT[:4] == [0xc, 0x10, 0x14, 0x20], HOST_LAYOUT   # the Mac's layout (unit v5L31)
    lib.bound = True


def run(s, case):
    try:
        s.prepare()
        for i in IDS:
            if i not in case["missing"]:
                s.set_window(i, s.win_block(i))
        panel = s.panel = s.alloc(s.psize)
        s.p32(panel + s.off["win"], s.win_block(1))
        s.p32(panel + s.off["stat"], s.win_block(2) if case["status"] else 0)
        s.p32(panel + s.off["w1c"], s.win_block(3))
        s.p32(panel + s.off["ops"], s.ops_ptr())
        obj = s.alloc(0x140)
        s.words(obj + 0x138, [OBJ_TAG])
        s.ptr(s.global_staff(), obj)
        s.call(case["op"])
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mkcase(r, op, status=True, closed=None, quality=None, amount=None, ing=None, conv=None, missing=()):
    q = {
        "IsClosed": [r.choice([0, 1]) if closed is None else closed],
        "Quality": [r.choice([0, 1, 49, 50, 99, 100, 149, 150, 255, r.randrange(256)]) if quality is None else quality],
        "Amount": [r.randrange(256) if amount is None else amount],
        "IngName": [r.choice([0, 0x4000]) if ing is None else ing],
        "ConvertMulti": [r.choice([0, 0x8000]) if conv is None else conv],
        "Price": [r.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(1 << 32)])],
        "Ops13": [r.choice([0, 0x9000, r.randrange(1 << 32)])],
    }
    return {"op": op, "queues": q, "status": status, "missing": set(missing)}


def init_edges():
    r = random.Random(5936)
    yield mkcase(r, "init", True)
    yield mkcase(r, "init", False)


def init_random(r):
    return mkcase(r, "init", r.random() < 0.85)


def update_edges():
    r = random.Random(5937)
    for quality in (0, 1, 49, 50, 51, 99, 100, 149, 150, 255):
        for closed in (0, 1):
            yield mkcase(r, "update", True, closed, quality, conv=0x8000)
    for conv in (0, 0x8000):
        yield mkcase(r, "update", True, conv=conv, ing=0)
    for m in ((0xc082,), (0xc06b,), (0xc085,), (0xc082, 0xc06b, 0xc085)):
        yield mkcase(r, "update", True, missing=m)


def update_random(r):
    return mkcase(r, "update", True, missing=[i for i in (0xc082, 0xc06b, 0xc085) if r.random() < 0.2])


TESTS = [
    FnTest("ShopStatusPanel::InitStatusWindow", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("ShopStatusPanel::UpdateWindow", ppc, host, edge=update_edges, rnd=update_random),
]
