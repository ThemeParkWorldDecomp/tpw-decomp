"""Unit v5548, the upgrade status panel's window handler and the staff status static initialiser, against
libtpw_unit_UpgradeStatusMsg.dylib (tests/unit_UpgradeStatusMsg.cpp), main app ("SimTheme Park"), src/UpgradeStatusMsg.cpp:
  UpgradeStatusMsg 0x101c63b4, __sinit_ui_staffStatus_cpp 0x101c5bec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the panel's methods, the window timers and GetWindow, the object and analyser getters, the SetValue virtual (__ptr_glue),
CreateAllItemsPanel, DefaultStatusPanelMsg, the UIStatusPanel constructor) are fakes on both sides that record their arguments and answer
from the case's queues. Objects are named in the log by labels (1 panel window, 2 count window, 3 value window, 4 upgrade panel, 5 analyser,
6 thing, 7 world, 8 staff panel, 9 message window).

The world of a case: the upgrade panel's window word (+0x10, or 0), the thing (its halfword at +0xe), the world, the count window (present
or not), the message and its arguments. What is compared: the answer and the event log; for the initialiser the first nine words of the staff
panel and the two new globals. No fused operations. Not tested: a NULL value window under message 0x10 with a panel window (the original
calls through it without a check).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpgradeStatusMsg"
MASK = 0xFFFFFFFF

FN_MSG, FN_SINIT = 0x101C63B4, 0x101C5BEC
SIZES = {FN_MSG: 620, FN_SINIT: 100}
OWN = set(SIZES)

ADDR = dict(up=0x10395B4C, thing=0x103947A4, world=0x1037EBFC, staff=0x10395B1C, f40=0x10395B40, f44=0x10395B44)
HOST_LAYOUT = [0, 0x10, 0x20]

LABS = {}      # label -> address of the case being run (either side)
REV = {}       # address -> label


def s32(v):
    return T.s32(v)


def lb(p):
    return 0 if p == 0 else REV.get(p, -1)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def simple(addr, name, regs=()):
    def f(e):
        e.cl.rec(name, tuple(lb(e.r(3)) if k == "L" else s32(e.r(int(k))) for k in regs))
    STUBS.internal(addr, f)


def with_ret(addr, name, regs, key):
    def f(e):
        e.cl.rec(name, tuple(lb(e.r(3)) if k == "L" else s32(e.r(int(k))) for k in regs))
        e.set_r(3, e.cl.ret(key) & MASK)
    STUBS.internal(addr, f)


simple(0x10181570, "Tick")
simple(0x101C66E0, "Remove", "L")
simple(0x10192C28, "Next", "L")
simple(0x10192DE0, "Prev", "L")
simple(0x10193138, "Erase", "L")
simple(0x101938D8, "Move", "L")
simple(0x101929E8, "UICtor", "L")
simple(0x1018227C, "Cancel", "3")
simple(0x101DD6B8, "StartAnim", ("L", "4"))
simple(0x10107CD4, "GetDBInfo", "L")
simple(0x1023BAA8, "SetValue", ("L", "4"))
with_ret(0x101DD5E0, "SetAnim", ("L", "4", "5"), "SetAnim")
with_ret(0x10193C78, "Default", ("L", "4", "5", "6"), "Default")
with_ret(0x1010FFC8, "Residual", "L", "Residual")


def stub_create_all(e):
    e.cl.rec("CreateAll", (s32(e.r(3)), lb(e.r(4))))


STUBS.internal(0x101983C4, stub_create_all)


def stub_get_window(e):
    e.cl.rec("GetWindow", (lb(e.r(3)), s32(e.r(4))))
    e.set_r(3, {0x5210: LABS.get(2, 0), 0x520E: LABS.get(3, 0)}.get(s32(e.r(4)), 0))


STUBS.internal(0x101DBE40, stub_get_window)


def stub_analyser(e):
    e.cl.rec("GetParkAnalyser", (lb(e.r(3)),))
    e.set_r(3, LABS[5])


STUBS.internal(0x1013EBC8, stub_analyser)


def stub_number(e):
    e.cl.rec("NumberOfObject", (lb(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.cl.ret("NumberOfObject") & MASK)


STUBS.internal(0x100EA52C, stub_number)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def labels(self, mapping):
        LABS.clear()
        REV.clear()
        for k, a in mapping.items():
            LABS[k] = a
            REV[a] = k
            self.setlab(k, a)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def wptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    def setlab(self, k, a):
        pass

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def msg(self, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_MSG, [w, m & MASK, a & MASK, b & MASK])
        return r3 & MASK

    def sinit(self):
        T.call_ppc(self.e, FN_SINIT, [])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_us_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_us_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_us_ptr(a)

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def setlab(self, k, a):
        self.lib.tpw_us_setlab(k, a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def msg(self, w, m, a, b):
        return self.lib.tpw_us_msg(w, s32(m), s32(a), s32(b)) & MASK

    def sinit(self):
        self.lib.tpw_us_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_us_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "ptr").restype, getattr(lib, p + "ptr").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "setlab").restype, getattr(lib, p + "setlab").argtypes = None, [C.c_int32, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "msg").restype, getattr(lib, p + "msg").argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    if case["op"] == "sinit":
        staff = s.g("staff")
        s.words(staff, case["words"])
        s.words(s.g("f40"), [case["f40"]])
        s.words(s.g("f44"), [case["f44"]])
        s.labels({8: staff})
        return
    w9, f10, cw, vw = s.alloc(0x130), s.alloc(0x130), s.alloc(0x130), s.alloc(0x130)
    for w in (f10, cw, vw):
        s.words(w + 0x118, [s.alloc(0x40)])
    an, thing, world = s.alloc(0x20), s.alloc(0x40), s.alloc(0x20)
    up = s.g("up")
    s.words(up, case["panel"])
    s.words(up + 0x10, [s.ptr(f10) if case["f10"] else 0])
    s.half(thing + 0xE, case["id"])
    s.wptr(s.g("thing"), thing)
    s.wptr(s.g("world"), world)
    labs = {1: f10, 2: cw if case["cnt"] else 0, 3: vw, 4: up, 5: an, 6: thing, 7: world, 9: w9}
    s.labels({k: a for k, a in labs.items() if a})
    LABS[2] = cw if case["cnt"] else 0
    s.setlab(2, LABS[2])
    s.w9 = w9


def execute(s, case):
    if case["op"] == "sinit":
        return s.sinit()
    return s.msg(s.w9, *case["args"])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    if case["op"] == "sinit":
        out["staff"] = s.rd_words(s.g("staff"), 9)
        out["f40"] = s.rd_words(s.g("f40"), 1)
        out["f44"] = s.rd_words(s.g("f44"), 1)
    return out


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


MSGS = [0x15, 5, 0x14, 0x13, 0x11, 0x10, 0xF, 0, 4, 6, 0x16, 0x1E, 0x1D, 0xFF, 0x100, 0x101, 0x800, 0x802, -1]
AS = [-1, -2, 0, 0x5208, 0x5209, 0x520A, 0x520B, 0x520C, 0x5212, 0x5213, 0x5214, 0x5215, 0x80080, 0x80081, 0x80000, 0x180080, 0x10]


def msg_case(r, m=None, a=None, f10=None, cnt=None):
    return {
        "op": "msg", "args": [r.choice(MSGS) if m is None else m, r.choice(AS) if a is None else a, r.choice([0, 1, -1, 7])],
        "f10": r.choice([0, 1]) if f10 is None else f10, "cnt": r.choice([0, 1]) if cnt is None else cnt,
        "id": r.choice([0, 1, 0xFFFF, r.randrange(0x10000)]), "panel": [r.randrange(1 << 32) for _ in range(4)],
        "queues": {"SetAnim": [r.choice([0, 1, 0x10000, 0xFFFF, -1])], "NumberOfObject": [r.randrange(0, 50)],
                   "Residual": [r.choice([0, 5, 0x7FFFFFFF, 0xFFFFFFFF, r.randrange(1 << 32)])],
                   "Default": [r.choice([0, 1, -1, 5])]},
    }


def msg_edges():
    r = random.Random(548)
    for m in MSGS:
        for a in AS:
            yield msg_case(r, m, a)
    for f10 in (0, 1):
        for cnt in (0, 1):
            for _ in range(2):
                yield msg_case(r, 0x10, 0x80080, f10, cnt)


def msg_random(r):
    return msg_case(r)


def sinit_case(r):
    return {"op": "sinit", "words": [r.randrange(1 << 32) for _ in range(9)], "f40": r.randrange(1 << 32), "f44": r.randrange(1 << 32),
            "queues": {}}


def sinit_edges():
    r = random.Random(77)
    for _ in range(3):
        yield sinit_case(r)


TESTS = [
    FnTest("UpgradeStatusMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("__sinit_ui_staffStatus_cpp", ppc, host, edge=sinit_edges, rnd=sinit_case),
]
