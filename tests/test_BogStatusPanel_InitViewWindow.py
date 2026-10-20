"""Unit v5301, the bog status panel's window functions, against libtpw_unit_BogStatusPanel_InitViewWindow.dylib
(tests/unit_BogStatusPanel_InitViewWindow.cpp), main app ("SimTheme Park"), src/BogStatusPanel_InitViewWindow.cpp:
  BogStatusPanel::InitViewWindow 0x10199ed0, UpdateViewWindow 0x1019a050, InitWindow 0x1019a7c8, UpdateWindow 0x1019a8cc, Show 0x1019ac5c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, Ptr32 offsets) for the translation. Every callee outside the unit (the
window members, the font table, the new and the sprite constructor, the CoolRect constructor and destructor, GetDBInfo, the ride preview, the
advisor, the status panel's FillObjectList, InitEditBox and OpenWindow) is a fake on both sides that records its arguments and answers from
the case. The Mac's virtual calls through __ptr_glue are fakes keyed by the TVector's id word; on the host they are blocks holding the
fake's address (UpdateWindow's slots 0x3c, 0x20, 0x1c of the panel and the window's SetText).

The world of a case: the panel (the windows at +0xc, +0x14, +0x18, +0x1c, the table at +0x20), the windows' depth, the ride record word at
+0x78c, the thing's float at +0x40, the globals 0x103947fc and 0x103947a4, the answer queues. What is compared: the event log, both
globals (as labels). No fused operations. Not tested: a NULL panel window at +0xc or +0x1c (read unconditionally), a NULL thing for Show
(its float is read), a float past 2^31 or NaN for Show (the emulator's fctiwz differs from the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BogStatusPanel_InitViewWindow"
MASK = 0xFFFFFFFF

FN_IVW, FN_UVW, FN_IW, FN_UW, FN_SHOW = 0x10199ED0, 0x1019A050, 0x1019A7C8, 0x1019A8CC, 0x1019AC5C
SIZES = {FN_IVW: 236, FN_UVW: 164, FN_IW: 152, FN_UW: 120, FN_SHOW: 140}
OWN = set(SIZES)

ADDR = dict(gThing=0x103947FC, gPanelThing=0x103947A4, script=0x102A908C, callback=0x10267AA8)
HOST_NAME = dict(gThing="gBogStatusThing", gPanelThing="gPanelThing", script="script", callback="callback")
HOST_LAYOUT = [0xC, 0x14, 0x18, 0x1C, 0x20]

OBJ = {}            # label -> the PowerPC address of this case's objects (the stubs answer with them)


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, args, ret=None):
    """A recorder: args is a list of 'p' (label), 'i' (s32), 'u' (u32), 'h' (s16) for r3 up."""
    def run(e):
        L = e.cl
        vals = []
        for k, kind in enumerate(args):
            r = e.r(3 + k)
            vals.append(L.lab(r) if kind == "p" else s32(r) if kind == "i" else (r & MASK) if kind == "u" else T.s16(r))
        L.rec(name, tuple(vals))
        if ret is not None:
            e.set_r(3, ret(e, L) & MASK)
    STUBS.internal(addr, run)


fake(0x101DD8BC, "setZ", "ph")
fake(0x101DC1EC, "Show", "ph", lambda e, L: 1)
fake(0x1018D11C, "SpriteCtor", "p")
fake(0x101DB624, "SetClientDrawObject", "pp")
fake(0x101DA204, "SetFontJust", "pii")
fake(0x10180270, "GetFontTableEntry", "u", lambda e, L: OBJ[6])
fake(0x101DA118, "SetFont", "pp")
fake(0x101DA3AC, "SetFontColour", "piiii")
fake(0x101DE130, "SetClientDrawText", "pu")
fake(0x10107CD4, "GetDBInfo", "p", lambda e, L: OBJ[10])
fake(0x101C7448, "CoolRect", "")
fake(0x101821CC, "AdvisorMessage", "i")
fake(0x10192A90, "FillObjectList", "pp", lambda e, L: L.ret("Fill"))
fake(0x10193DCC, "InitEditBox", "pi")
def st_open(e):
    L = e.cl
    cb = e.r(5) & MASK
    L.rec("OpenWindow", (L.lab(e.r(3)), L.lab(e.r(4)), 13 if cb == ADDR["callback"] else cb, s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


STUBS.internal(0x101935A0, st_open)
fake(0x101DBE40, "GetWindow", "pi", lambda e, L: OBJ[L.ret("GetWindow:%d" % s32(e.r(4)))])


def st_coolrect(e):
    e.cl.rec("CoolRect", (0,))


def st_coolrect_dtor(e):
    e.cl.rec("~CoolRect", (0,))


def st_new(e):
    L = e.cl
    L.rec("New", (e.r(3) & MASK,))
    e.set_r(3, 0 if L.ret("newnull") else OBJ[7])


def st_getclientrect(e):
    L = e.cl
    L.rec("GetClientRect", (L.lab(e.r(3)),))
    e.write(e.r(4) + 4, struct.pack(">4h", *[T.s16(L.ret("rect:%d" % i)) for i in range(4)]))
    e.set_r(3, 1)


def st_preview(e):
    L = e.cl
    rect = struct.unpack(">4h", e.read(e.r(3) + 4, 8))
    L.rec("ShowRidePreview", rect + (e.r(4) & MASK, s32(e.r(5))))


def st_glue(e):
    L = e.cl
    ident = e.u32(e.r(12))
    if ident == 0x3C:
        L.rec("Slot", (0x3C, L.lab(e.r(3))))
        e.set_r(3, L.ret("slot3c") & MASK)
    elif ident in (0x20, 0x1C):
        L.rec("Slot", (ident, L.lab(e.r(3))))
        e.set_r(3, 0)
    else:
        L.rec("SetText", (L.lab(e.r(3)), e.r(4) & MASK))


STUBS.internal(0x101C7448, st_coolrect)
STUBS.internal(0x101C74E0, st_coolrect_dtor)
STUBS.internal(0x102384B8, st_new)
STUBS.internal(0x101DBA4C, st_getclientrect)
STUBS.internal(0x10181400, st_preview)
STUBS.internal(0x1023BAA8, st_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def write16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", v))

    def make(self):
        """The objects every case has (allocated in the same order on both sides)."""
        names = ["panel", "w0c", "w14", "w18", "w1c", "font", "sprite", "chA", "chB", "info", "thing"]
        self.o = {n: self.alloc(0x800 if n == "info" else 0x400) for n in names}
        self.table = self.alloc(0x80)

    def label_of(self, value):
        for lab, name in enumerate(["panel", "w0c", "w14", "w18", "w1c", "font", "sprite", "chA", "chB", "info", "thing"], 1):
            if self.o[name] == value:
                return lab
        return value


class Ppc(Side):
    fmt = ">I"
    zoff = 0xDC

    def __init__(self, e, case):
        self.e = e
        self.make()
        o = self.o
        fixed = {o[n]: i for i, n in enumerate(["panel", "w0c", "w14", "w18", "w1c", "font", "sprite", "chA", "chB", "info", "thing"], 1)}
        fixed[ADDR["script"]] = 12
        fixed[ADDR["callback"]] = 13
        OBJ.clear()
        OBJ.update({0: 0, 6: o["font"], 7: o["sprite"], 8: o["chA"], 9: o["chB"], 10: o["info"]})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def ptr(self, a):
        return a

    def setptr(self, a, v):
        self.words(a, [v])

    def getptr(self, a):
        return self.rd_words(a, 1)[0]

    def vslot(self, ident):
        b = self.alloc(8)
        self.words(b, [ident, 0])
        return b

    def window_vt(self, w):
        t = self.alloc(0x80)
        self.words(t + 0x2C, [self.vslot(0x12C)])
        self.words(w + 0x118, [t])

    def call(self, fn, *args):
        r3, _, _ = T.call_ppc(self.e, fn, list(args))
        return None


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.zoff = lib.mz
        self.make()
        o = self.o
        fixed = {i: o[n] for i, n in enumerate(["panel", "w0c", "w14", "w18", "w1c", "font", "sprite", "chA", "chB", "info", "thing"], 1)}
        fixed[12] = self.g("script")
        fixed[13] = self.g("callback")
        for i in (6, 7, 8, 9, 10):
            lib.tpw_bog_set_obj(i, fixed[i])
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bog_addr(HOST_NAME[name].encode())

    def alloc(self, n):
        return self.lib.tpw_bog_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a):
        return self.lib.tpw_bog_offset(a)

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def getptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def vslot(self, ident):
        b = self.alloc(16)
        self.write(b, struct.pack("<Q", self.lib.tpw_bog_slotfn(ident)))
        return self.ptr(b)

    def window_vt(self, w):
        self.write(w, struct.pack("<Q", self.lib.tpw_bog_window_vptr()))

    def call(self, fn, *args):
        name = {FN_IVW: "init_view_window", FN_UVW: "update_view_window", FN_IW: "init_window", FN_UW: "update_window",
                FN_SHOW: "show"}[fn]
        getattr(self.lib, "tpw_bog_" + name)(*args)
        return None


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bog_addr.restype, lib.tpw_bog_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bog_alloc.restype, lib.tpw_bog_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bog_offset.restype, lib.tpw_bog_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_bog_set_obj.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_bog_slotfn.restype, lib.tpw_bog_slotfn.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bog_window_vptr.restype = C.c_uint64
    lib.tpw_bog_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("init_view_window", "update_view_window", "init_window", "update_window"):
        getattr(lib, "tpw_bog_" + n).argtypes = [C.c_uint64]
    lib.tpw_bog_show.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bog_layout(out)
    assert list(out)[:5] == HOST_LAYOUT, list(out)[:n]
    lib.mz = out[5]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.o
    s.words(o["panel"] + 0x0C, [s.ptr(o["w0c"])])
    s.words(o["panel"] + 0x14, [s.ptr(o["w14"]) if case["has14"] else 0])
    s.words(o["panel"] + 0x18, [s.ptr(o["w18"]) if case["has18"] else 0])
    s.words(o["panel"] + 0x1C, [s.ptr(o["w1c"])])
    s.words(o["panel"] + 0x20, [s.ptr(s.table)])
    for name in ("w14", "w18"):
        s.write16(o[name] + s.zoff, case["z"])
    s.words(o["info"] + 0x78C, [case["id"]])
    s.words(o["thing"] + 0x40, [case["fbits"]])
    s.setptr(s.g("gThing"), o["thing"] if case["gthing"] else 0)
    s.setptr(s.g("gPanelThing"), o["thing"] if case["pthing"] else 0)
    # UpdateWindow's tables: the panel's (Ptr32 words) and the window's
    for slot in (0x3C, 0x20, 0x1C):
        s.words(s.table + slot, [s.vslot(slot)])
    s.window_vt(o["w1c"])


def execute(s, case):
    o = s.o
    op = case["op"]
    if op == "show":
        return s.call(FN_SHOW, o["panel"], o["thing"])
    return s.call({"ivw": FN_IVW, "uvw": FN_UVW, "iw": FN_IW, "uw": FN_UW}[op], o["panel"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "gThing": s.label_of(s.getptr(s.g("gThing"))),
            "gPanelThing": s.label_of(s.getptr(s.g("gPanelThing")))}


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

ZS = [0, 1, -1, 0x100, 0x7FF4, 0x7FF5, 0x7FFF, -0x8000, -0xB, -0xC]


def base(r, op, **kw):
    c = {"op": op, "has14": r.random() < 0.8, "has18": r.random() < 0.8, "z": r.choice(ZS + [r.randrange(-0x8000, 0x8000)]),
         "id": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]), "fbits": fb(r.uniform(-600, 600)), "gthing": r.random() < 0.7,
         "pthing": r.random() < 0.7}
    c["queues"] = {"Fill": [r.choice([-1, 0, 1, 2, 3, r.randrange(-5, 9)])], "newnull": [1 if r.random() < 0.2 else 0],
                   "slot3c": [r.randrange(1, 0x7FFFFFFF)],
                   "GetWindow:%d" % 0x15BB6: [r.choice([0, 8, 9])], "GetWindow:%d" % 0x15BB7: [r.choice([0, 8, 9])]}
    for i in range(4):
        c["queues"]["rect:%d" % i] = [r.choice([0, 1, -1, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)])]
    c.update(kw)
    return c


def gen(op, seed, edges):
    def edge():
        r = random.Random(seed)
        for kw in edges:
            yield base(r, op, **kw)

    def rnd(r):
        return base(r, op)
    return edge, rnd


IVW_EDGES = [dict(has18=h, z=z, queues=None) for h in (False, True) for z in ZS]
UVW_EDGES = [dict(has14=a, pthing=b, z=z) for a in (False, True) for b in (False, True) for z in ZS]
IW_EDGES = [dict(gthing=g, queues=None) for g in (False, True)]
SHOW_FLOATS = [0.0, -0.0, 24.0, 24.9, 25.0, 25.5, -1.0, -255.0, 255.0, 255.9, 256.0, 280.0, 281.0, 280.9, 1000000.0, -1000000.0, 2e9, -2e9]


def with_queues(op, seed, edges):
    """Edges that need chosen queue values override single keys."""
    def edge():
        r = random.Random(seed)
        for kw in edges:
            q = kw.pop("q", {})
            c = base(r, op, **kw)
            c["queues"].update(q)
            yield c

    def rnd(r):
        return base(r, op)
    return edge, rnd


def fill_edges():
    out = []
    for fill in (-1, 0, 1, 2, 3):
        for a in (0, 8, 9):
            for b in (0, 8, 9):
                out.append(dict(q={"Fill": [fill], "GetWindow:%d" % 0x15BB6: [a], "GetWindow:%d" % 0x15BB7: [b]}))
    return out


def ivw_edges():
    out = []
    for h in (False, True):
        for z in ZS:
            for nn in (0, 1):
                out.append(dict(has18=h, z=z, q={"newnull": [nn]}))
    return out


TESTS = []
for name, op, seed, edges in (
        ("BogStatusPanel::InitViewWindow", "ivw", 5301, ivw_edges()),
        ("BogStatusPanel::UpdateViewWindow", "uvw", 5302, UVW_EDGES),
        ("BogStatusPanel::InitWindow", "iw", 5303, fill_edges()),
        ("BogStatusPanel::UpdateWindow", "uw", 5304, [dict(), dict(slot3c_dummy=0)] if False else [dict(), dict()]),
        ("BogStatusPanel::Show", "show", 5305, [dict(fbits=fb(f)) for f in SHOW_FLOATS])):
    e_, r_ = with_queues(op, seed, [dict(x) for x in edges])
    TESTS.append(FnTest(name, ppc, host, edge=e_, rnd=r_))
