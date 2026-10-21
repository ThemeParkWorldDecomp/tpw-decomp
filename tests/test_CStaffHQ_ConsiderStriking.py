"""Unit v5681, CStaffHQ::ConsiderStriking, against libtpw_unit_CStaffHQ_ConsiderStriking.dylib (tests/unit_CStaffHQ_ConsiderStriking.cpp), main app
("SimTheme Park"), src/game/buildings/StaffHQ.cpp:
  CStaffHQ::ConsiderStriking 0x1012b084

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (CTRACE, the park time,
StaffTypeHasAGrievance, StopStrikeInProgress, the CMsgStaffInfo constructor, CMessageCentre::PostMessage, operator new and __construct_array)
are fakes on both sides that record their arguments; the 64 bit division helper 0x10237e1c runs as the original.

The world of a case: the world (turn at +0x1da70c), the staff headquarters with its five strike records, the park time answer, the grievance
answer, and a message centre that exists or not. What is compared: the log, the whole headquarters object and which block the message centre
global holds afterwards.

No fused operations. Not tested: a staff type outside 0..4 (it indexes past the object).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CStaffHQ_ConsiderStriking"
MASK = 0xFFFFFFFF

FN = 0x1012B084
SIZES = {FN: 1076}
OWN = set(SIZES)

CTRACE = 0x1006FCBC
FUNNY = 0x10112B14
GRIEVANCE = 0x1012B56C
STOP = 0x1012BE24
MSGCTOR = 0x10151F14
POST = 0x1015256C
NEW = 0x102384B8
CONSTRUCT = 0x10238758
DIV64 = 0x10237E1C

ADDR = dict(gMessageCentre=0x1037F6C4, gWorld=0x1037EC0C, gGameWorld=0x1037EBFC)
HOST_LAYOUT = [0x1DA70C, 0x2A0, 0x24, 0x68]
D = 0x1792F8648000   # 30 days in 100 ns ticks


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(DIV64)

_rec_ctrace = CL.recorder("CTRACE", "us")
_rec_ctrace2 = CL.recorder("CTRACE2", "usu")
_rec_funny = CL.recorder("Funny", "xL")
_rec_grievance = CL.recorder("Grievance", "Li", ret="i")
_rec_stop = CL.recorder("Stop", "Li")
_rec_ctor = CL.recorder("MsgStaffInfo", "xii")
BLOCK = [0]


def stub_ctrace(e):
    (_rec_ctrace2 if e.r(3) == 2 else _rec_ctrace)(e)


def stub_funny(e):
    out = e.r(3)
    _rec_funny(e)
    L = e.cl
    hi, lo = L.ret("hi") & MASK, L.ret("lo") & MASK
    e.write(out, struct.pack(">II", hi, lo))


def stub_ctor(e):
    this, staff, sit = e.r(3), e.r(4), e.r(5)
    _rec_ctor(e)
    e.write(this + 4, struct.pack(">III", 0, staff & MASK, sit & MASK))


def stub_post(e):
    L = e.cl
    m = e.r(4)
    L.rec("Post", (L.lab(e.r(3)), e.u32(m + 4), e.u32(m + 8), e.u32(m + 12)))
    e.set_r(3, 1)


def stub_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, BLOCK[0])


def stub_construct(e):
    L = e.cl
    L.rec("construct", (L.lab(e.r(3)), e.r(6), e.r(7)))


STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(FUNNY, stub_funny)
STUBS.internal(GRIEVANCE, _rec_grievance)
STUBS.internal(STOP, _rec_stop)
STUBS.internal(MSGCTOR, stub_ctor)
STUBS.internal(POST, stub_post)
STUBS.internal(NEW, stub_new)
STUBS.internal(CONSTRUCT, stub_construct)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def blocks(self):
        self.world = self.alloc(0x1DA800)
        self.hq = self.alloc(0x68)
        self.centre = self.alloc(0x1D0)
        self.block = self.alloc(0x1D0)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        BLOCK[0] = self.block
        self.L = e.cl = CL.PpcLog({self.hq: 1, self.world + 0x2A0: 2, self.centre: 3, self.block: 4}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def log(self):
        return self.L.log

    def call(self, type_):
        T.call_ppc(self.e, FN, [self.hq, type_ & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        CL.host_begin(lib, {1: self.hq, 2: self.world + 0x2A0, 3: self.centre, 4: self.block}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, type_):
        self.lib.tpw_hq_consider(self.hq, s32(type_))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hq_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "consider").restype, getattr(lib, p + "consider").argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.world + 0x1DA70C, [case["turn"]])
    s.ptr(s.g("gWorld"), s.world)
    s.ptr(s.g("gGameWorld"), s.world)
    s.words(s.hq + 0x24, case["strikes"])
    s.ptr(s.g("gMessageCentre"), s.centre if case["centre"] else 0)
    if s.fmt == "<I":
        s.ptr(s.g("gNewBlock"), s.block)


def observe(s):
    mc = s.rd_ptr(s.g("gMessageCentre"))
    which = 0 if mc == 0 else 3 if mc == s.centre else 4 if mc == s.block else -1
    return {"log": s.log(), "hq": s.rd_words(s.hq, 26), "centre": which}


def run(s, case):
    try:
        build(s, case)
        s.call(case["type"])
        return observe(s)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


STATES = [0, 1, 2, 3, 4, 5, 9, 0xFFFFFFFF, 0x80000000]


def ticks_words(t):
    t &= (1 << 64) - 1
    return t >> 32, t & MASK


def base(r, months=None, rem=None):
    if months is None:
        months = r.choice([0, 1, 23, 24, 25, 100, 5000, 354000, -1, -30, r.randrange(0, 400), r.randrange(-50, 400)])
    if rem is None:
        rem = r.choice([0, 1, D - 1, r.randrange(0, D)])
    t = months * D + (rem if months >= 0 else -rem)
    hi, lo = ticks_words(t)
    strikes = [r.choice([0, 1, 2]) if r.random() < 0.5 else r.randrange(0, 1 << 32) for _ in range(15)]
    for i in range(0, 15, 3):
        strikes[i] = r.choice(STATES)
    return {"turn": r.choice([0, 1, 24, 0x7FFFFFFF, 0xFFFFFFFF, r.randrange(0, 1 << 32)]), "centre": r.choice([0, 1]),
            "type": r.randrange(0, 5), "strikes": strikes,
            "queues": {"hi": [hi], "lo": [lo], "Grievance": [r.choice([0, 1, 2, 0x100, -1])]}}


def edges():
    r = random.Random(5681)
    # the month threshold on both sides
    for m, rem in ((23, D - 1), (24, 0), (23, 0), (25, 5), (0, 0), (-1, 0), (-24, 3), (-25, 0)):
        c = base(r, m, rem)
        c["queues"]["Grievance"] = [1]
        c["strikes"][0] = 0
        c["type"] = 0
        yield c
    # every state, grievance or not, with and without a message centre, each staff type
    for st in STATES:
        for grievance in (0, 1):
            for centre in (0, 1):
                c = base(r, 30, 7)
                c["type"] = r.randrange(0, 5)
                c["strikes"][c["type"] * 3] = st
                c["strikes"][c["type"] * 3 + 1] = r.choice([0, 1, 77])
                c["centre"] = centre
                c["queues"]["Grievance"] = [grievance]
                yield c
    for t in range(5):
        c = base(r, 24, 0)
        c["type"] = t
        c["strikes"][t * 3] = 1
        c["queues"]["Grievance"] = [0]
        yield c


def rnd(r):
    return base(r)


TESTS = [
    FnTest("CStaffHQ::ConsiderStriking", ppc, host, edge=edges, rnd=rnd),
]
