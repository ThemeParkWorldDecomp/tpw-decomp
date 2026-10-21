"""Unit v5904, the feature status panel's window handler, its static initialiser and the finance graph's two loops, against
libtpw_unit_FeatureStatusMsg_2.dylib (tests/unit_FeatureStatusMsg_2.cpp), main app ("SimTheme Park"),
src/game/ui/ui_FeatureStatus.cpp, src/game/ui/ui_FinancePanel.cpp:
  FeatureStatusMsg 0x1019eae4, __sinit_ui_FeatureStatus_cpp 0x1019eea0, GetMaxYValue 0x1019ef1c, KissMyLines 0x1019f25c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit is a fake on both
sides that records its arguments (windows by an id word, the panel by label, the thing by the halfword at +0xe) and answers from the
case's queues. The windows are blocks with a vtable whose SetValue slot (0x24 of the table at +0x118 on the Mac) records; the panel's
table slot 0x34 records too.

The world of a case: gStaffRoomPanelThing, gGameWorld, the finance flags and mode, the panel's twelve words (random, with the
window at +0x10 and the table at +0x20 set), the windows. What is compared: the answer, the panel's words and the event log.

No fused operations. Not tested: a NULL window under a handled message (the original reads it at once), FeatureStatusMsg with the
window at +0x10 set but the 0x1584 child missing (the original reads its table without a test).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FeatureStatusMsg_2"
MASK = 0xFFFFFFFF

FN_MSG, FN_SINIT, FN_MAXY, FN_KISS = 0x1019EAE4, 0x1019EEA0, 0x1019EF1C, 0x1019F25C
SIZES = {FN_MSG: 676, FN_SINIT: 76, FN_MAXY: 380, FN_KISS: 424}
OWN = set(SIZES)

ADDR = dict(panel=0x10394870, thing=0x103947A4, world=0x1037EBFC, flags=0x102A9EE4, mode=0x10394900, colours=0x103948D8)
HOST_LAYOUT = [0x10, 0x20, 0, 5]

WORLD, GRAPHER, ANALYSER = 0x32000000, 0x31000000, 0x30000000
MARK_SETVALUE, MARK_SLOT34 = 0xCC000024, 0xBB000034

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def objid(e, a):
    return struct.unpack(">H", e.read(a + 0xE, 2))[0]


def winid(e, a):
    return e.u32(a + 0x120)


def simple(addr, name, fn):
    def stub(e):
        fn(e, e.cl)
    STUBS.internal(addr, stub)


simple(0x10181570, "TickRidePreview", lambda e, L: L.rec("TickRidePreview", ()))
simple(0x1019EE48, "Remove", lambda e, L: L.rec("Remove", (L.lab(e.r(3)),)))
simple(0x1018227C, "CancelAdvisor", lambda e, L: L.rec("CancelAdvisor", (s32(e.r(3)),)))
simple(0x10192C28, "GetNext", lambda e, L: L.rec("GetNext", (L.lab(e.r(3)),)))
simple(0x10192DE0, "GetPrevious", lambda e, L: L.rec("GetPrevious", (L.lab(e.r(3)),)))
simple(0x10193138, "Erase", lambda e, L: L.rec("Erase", (L.lab(e.r(3)),)))
simple(0x101938D8, "Move", lambda e, L: L.rec("Move", (L.lab(e.r(3)),)))
simple(0x101983C4, "CreateAllItemsPanel", lambda e, L: L.rec("CreateAllItemsPanel", (s32(e.r(3)), e.r(4) & MASK)))
simple(0x1010ED8C, "GoFirstPerson", lambda e, L: L.rec("GoFirstPerson", (objid(e, e.r(3)), s32(e.r(4)))))
simple(0x10107CD4, "GetDBInfo", lambda e, L: L.rec("GetDBInfo", (objid(e, e.r(3)),)))
simple(0x101DD6B8, "startAnimTimer", lambda e, L: L.rec("startAnimTimer", (winid(e, e.r(3)), s32(e.r(4)))))


def stub_setanim(e):
    L = e.cl
    L.rec("setAnimTimer", (winid(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("setAnimTimer") & MASK)


def stub_getwindow(e):
    L = e.cl
    w, wid = e.r(3), s32(e.r(4))
    L.rec("GetWindow", (winid(e, w), wid))
    e.set_r(3, e.u32(w + 0x124) if wid == 0x1586 else e.u32(w + 0x128) if wid == 0x1584 else 0)


def stub_residual(e):
    L = e.cl
    L.rec("Residual", (objid(e, e.r(3)),))
    e.set_r(3, L.ret("Residual") & MASK)


def stub_analyser(e):
    e.cl.rec("GetParkAnalyser", (e.r(3) & MASK,))
    e.set_r(3, ANALYSER)


def stub_number(e):
    L = e.cl
    L.rec("NumberOfObjectInPark", ((e.r(4) >> 16) & 0xFFFF,))
    e.set_r(3, L.ret("NumberOfObjectInPark") & MASK)


def stub_default(e):
    L = e.cl
    L.rec("Default", (winid(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


def stub_construct(e):
    p = e.r(3)
    e.cl.rec("Construct", (e.cl.lab(p),))
    e.write(p, struct.pack(">I", 0x77))
    e.write(p + 0x20, struct.pack(">I", 0x88))


def stub_max(e):
    L = e.cl
    p = e.r(4)
    L.rec("Max", (e.r(3) & MASK, s32(e.u32(p)), s32(e.r(5))))
    e.write(p, struct.pack(">I", L.ret("Max") & MASK))


def stub_graphline(e):
    d = e.r(5) - ADDR["colours"]
    assert d % 5 == 0
    e.cl.rec("GraphLine", (e.r(3) & MASK, e.r(4) & MASK, d // 5, s32(e.r(6)), s32(e.r(7))))


def stub_glue(e):
    r12, L = e.r(12), e.cl
    if r12 == MARK_SETVALUE:
        L.rec("SetValue", (winid(e, e.r(3)), s32(e.r(4))))
    elif r12 == MARK_SLOT34:
        L.rec("Slot34", (L.lab(e.r(3)),))
    else:
        raise AssertionError("unknown glue target %x" % r12)


STUBS.internal(0x101DD5E0, stub_setanim)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x1010FFC8, stub_residual)
STUBS.internal(0x1013EBC8, stub_analyser)
STUBS.internal(0x100EA52C, stub_number)
STUBS.internal(0x10193C78, stub_default)
STUBS.internal(0x101929E8, stub_construct)
STUBS.internal(0x101F5E3C, stub_max)
STUBS.internal(0x101F6124, stub_graphline)
STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["panel"]: 1}, case["queues"])
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

    def p32(self, addr):
        return addr

    def nptr(self, a, addr):
        self.words(a, [addr])

    def mkwin(self, wid, c1, c2):
        w, vt = self.alloc(0x130), self.alloc(0x80)
        self.words(vt + 0x24, [MARK_SETVALUE])
        self.words(w + 0x118, [vt])
        self.words(w + 0x120, [wid, c1, c2])
        return w

    def mktable(self):
        t = self.alloc(0x80)
        self.words(t + 0x34, [MARK_SLOT34])
        return t

    def sinit_table(self, word):
        return word

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("panel")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, addr):
        return addr - self.lib.tpw_fs_base()

    def nptr(self, a, addr):
        self.write(a, struct.pack("<Q", addr))

    def mkwin(self, wid, c1, c2):
        return self.lib.tpw_fs_mkwin(wid, c1, c2)

    def mktable(self):
        return self.lib.tpw_fs_mktable()

    def sinit_table(self, word):
        # the host's table is the arena offset of FeatureStatusPanelTable() (unit v5L31)
        return 0x102A9BA0 if word == self.lib.tpw_fs_sinit_table() else word

    def call(self, entry, args):
        lib = self.lib
        if entry == FN_MSG:
            return lib.tpw_fs_msg(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK
        if entry == FN_SINIT:
            lib.tpw_fs_sinit()
            return 0
        if entry == FN_MAXY:
            return lib.tpw_fs_maxy() & MASK
        lib.tpw_fs_kiss(args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fs_addr.restype, lib.tpw_fs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fs_alloc.restype, lib.tpw_fs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fs_base.restype, lib.tpw_fs_base.argtypes = C.c_uint64, []
    lib.tpw_fs_mkwin.restype, lib.tpw_fs_mkwin.argtypes = C.c_uint64, [C.c_uint32, C.c_uint64, C.c_uint64]
    lib.tpw_fs_mktable.restype, lib.tpw_fs_mktable.argtypes = C.c_uint64, []
    lib.tpw_fs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fs_msg.restype, lib.tpw_fs_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_fs_sinit.restype, lib.tpw_fs_sinit.argtypes = None, []
    lib.tpw_fs_sinit_table.restype, lib.tpw_fs_sinit_table.argtypes = C.c_uint32, []
    lib.tpw_fs_maxy.restype, lib.tpw_fs_maxy.argtypes = C.c_int32, []
    lib.tpw_fs_kiss.restype, lib.tpw_fs_kiss.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    """The globals, the thing, the windows and the panel's words (offsets are the Mac's)."""
    obj = s.alloc(0x20)
    s.half(obj + 0xE, case["objid"])
    s.nptr(s.g("thing"), obj)
    s.nptr(s.g("world"), WORLD)
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("mode"), [case["mode"]])
    a = s.mkwin(12, 0, 0) if case["hasA"] else 0
    b = s.mkwin(13, 0, 0)
    p = s.mkwin(11, a, b) if case["hasP"] else 0
    case_win = s.mkwin(14, 0, 0)
    panel = s.g("panel")
    s.words(panel, case["junk"])
    s.words(panel + 0x10, [s.p32(p) if p else 0])
    s.words(panel + 0x20, [s.p32(s.mktable())])
    return case_win


def execute(s, case, win):
    if case["op"] == "msg":
        return s.call(FN_MSG, [win] + case["args"])
    if case["op"] == "sinit":
        s.call(FN_SINIT, [])        # void: r3 is whatever the constructor left
        return 0
    if case["op"] == "maxy":
        return s.call(FN_MAXY, [])
    s.call(FN_KISS, [GRAPHER])      # void
    return 0


def run(s, case):
    try:
        win = build(s, case)
        answer = execute(s, case, win)
        w = s.rd_words(s.g("panel"), 12)
        w[4] = 1 if w[4] else 0                                       # the window at +0x10 is a pointer: present or not
        w[8] = s.sinit_table(w[8])                                    # the static initialiser's table: the Mac's address on both sides
        w[8] = w[8] if w[8] in (0x88, 0x102A9BA0) else 0xFFFFFFFF     # the table at +0x20: a constant of the code, or the case's block
        return {"answer": answer, "log": s.log(), "panel": w}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, **kw):
    c = {
        "op": op,
        "flags": r.choice([r.randrange(0, 256), r.randrange(0, 1 << 32), 0xFF]),
        "mode": r.choice([0, 1, 2, -1, r.randrange(-5, 300)]) & MASK,
        "objid": r.randrange(0, 0x10000),
        "hasA": r.choice([0, 1]),
        "hasP": r.choice([0, 1]),
        "junk": [r.randrange(0, 1 << 32) for _ in range(12)],
        "args": [0, 0, 0],
        "queues": {
            "setAnimTimer": [r.choice([0, 1, 0x10000, 0xFFFF, MASK])],
            "Residual": [r.randrange(0, 1 << 32)],
            "NumberOfObjectInPark": [r.randrange(0, 1 << 32)],
            "Default": [r.randrange(0, 1 << 32)],
            "Max": [r.randrange(-1000, 1000) & MASK for _ in range(8)],
        },
    }
    c.update(kw)
    return c


MSGS = [0x15, 0x10, 5, 0x14, 0x13, 0x11, 0x12, 0x1E, 0x100, 0x16, 0xFF, 0x101, 0, -1, 4, 6, 0xF, 0x1D, 0x1F, 0x7FFFFFFF, -0x80000000]
ALIST = [-1, -2, 0x157A, 0x157B, 0x157C, 0x157D, 0x157E, 0x157F, 0x1580, 0x1581, 0x1582, 0x1583, 0x1584, 0, 0x80080, 0x80081, 0x7FF80]


def msg_case(r, msg=None, a=None, **kw):
    msg = r.choice(MSGS) if msg is None else msg
    if a is None:
        a = r.choice(ALIST + [r.randrange(-(1 << 31), 1 << 31)])
    return base(r, "msg", args=[msg & MASK, a & MASK, r.choice([0, 1, r.randrange(0, 1 << 32)])], **kw)


def msg_edges():
    r = random.Random(904)
    for msg in MSGS:
        for a in ALIST:
            yield msg_case(r, msg, a)
    for has_p in (0, 1):
        for has_a in (0, 1):
            yield msg_case(r, 0x10, 0x80080, hasP=has_p, hasA=has_a)
    for v in (0, 1, 0x10000, 0xFFFF, MASK, 0x8000, 0x7FFF):
        c = msg_case(r, 0x15, 0)
        c["queues"]["setAnimTimer"] = [v]
        yield c


def msg_random(r):
    return msg_case(r)


def sinit_edges():
    r = random.Random(905)
    for _ in range(3):
        yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


def flags_edges(op, seed):
    r = random.Random(seed)
    for f in [0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0xFF, 0x100, 0xFFFFFF00, 0xFFFFFFFF, 0xAA, 0x55]:
        yield base(r, op, flags=f)


TESTS = [
    FnTest("FeatureStatusMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("__sinit_ui_FeatureStatus_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("GetMaxYValue", ppc, host, edge=lambda: flags_edges("maxy", 906), rnd=lambda r: base(r, "maxy")),
    FnTest("KissMyLines", ppc, host, edge=lambda: flags_edges("kiss", 907), rnd=lambda r: base(r, "kiss")),
]
