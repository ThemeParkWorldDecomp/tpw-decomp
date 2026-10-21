"""Unit v5050, the bump area functions, against libtpw_unit_BMP_InsideArea.dylib (tests/unit_BMP_InsideArea.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_InsideArea 0x1002c260, BMP_OutsideArea 0x1002be04

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. PointInColObject is
a fake on both sides that records (x, y, object label) and answers from the case's queue; atan2 (Python's math.atan2), labs and sqrt are
the exact C library functions on both sides.

The world of a case: the sine table (gHostSineTable), the collision list global (first, count), a bump (tagBUMP, 0xa8 bytes) with its owner,
a current collision object and up to 8 list objects. What is compared: the bump's words (its current object as a label), the event log.

HardwareRounding covers the fused fnmsub and fctiwz of both functions. Not tested: BMP_InsideArea with a box object, a pushing owner kind
and no overshoot (the original then reads r23, which it never set).
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_BMP_InsideArea"
MASK = 0xFFFFFFFF

FN_IN, FN_OUT = 0x1002C260, 0x1002BE04
SIZES = {FN_IN: 1372, FN_OUT: 1068}
OWN = set(SIZES)

CALLEE_PIC = 0x1002BC34

ADDR = dict(sine=0x102B4EC0, list=0x102B56C0)
HOST_LAYOUT = [0x34, 0x58, 0x98, 0x9C, 0xA4, 0xA8, 0x20, 0x18, 0x1C, 0x2C]

SINE = [int(256 * math.sin(math.pi * 2 * i / 512)) for i in range(512)]
KINDS = [-5, -8, -12, -13]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_pic(e):
    L = e.cl
    L.rec("PointInColObject", (s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    e.set_r(3, L.ret("PointInColObject") & MASK)


STUBS.internal(CALLEE_PIC, stub_pic)
STUBS.imp("atan2", lambda e: e.set_f1(math.atan2(e.f(1), e.f(2))))
STUBS.imp("labs", lambda e: e.set_r(3, (-s32(e.r(3)) if s32(e.r(3)) < 0 else s32(e.r(3))) & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    base = 0       # a Ptr32 word is an address minus base (0 on the PowerPC side)

    def w(self, addr):
        return 0 if addr == 0 else addr - self.base

    def a(self, word):
        return 0 if word == 0 else word + self.base

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def blocks(self):
        self.bump = self.alloc(0xA8)
        self.owner = self.alloc(0x30)
        self.cur = self.alloc(0x20)
        self.arr = self.alloc(8 * 0x20)

    def labels(self):
        d = {self.cur: 9}
        for i in range(8):
            d[self.arr + 32 * i] = 10 + i
        return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
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

    def call(self, entry):
        with ROUNDING(self.e):
            T.call_ppc(self.e, entry, [self.bump])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        self.base = lib.tpw_bmp_base()      # after the first allocation (the arena starts lazily)
        CL.host_begin(lib, {label: addr for addr, label in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bmp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bmp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry):
        (self.lib.tpw_bmp_inside if entry == FN_IN else self.lib.tpw_bmp_outside)(self.bump)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bmp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bmp_base.restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bmp_inside.argtypes = [C.c_uint64]
    lib.tpw_bmp_outside.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---- the world

BUMP_IDX = dict(flags=0, x=13, y=14, vx=15, vy=16, dirX=17, dirY=18, push=22, counter=41)


def objwords(o):
    return [o["flags"], o["x"], o["y"], o["r1"], o["r2"], 0, 0, 0]


def build(s, case):
    s.words(s.g("sine"), SINE)
    n_live = case["count"]
    first = s.w(s.arr) if case["first"] else 0
    s.words(s.g("list"), [0] * 6 + [first, n_live])
    s.words(s.owner,[case["kind"]] + [0] * 10 + [case["scale"]])
    s.words(s.cur, objwords(case["cur"]))
    for i, o in enumerate(case["objs"]):
        s.words(s.arr + 32 * i, objwords(o))
    w = [0] * 42
    for k, i in BUMP_IDX.items():
        w[i] = case["b"][k]
    sel = case["sel"]
    w[38] = 0 if sel < 0 else s.w(s.cur if sel == 0 else s.arr + 32 * (sel - 1))
    w[39] = s.w(s.owner)
    s.words(s.bump, w)


def observe(s, case):
    w = s.rd_words(s.bump, 42)
    lab = s.labels()
    w[38] = lab.get(s.a(w[38]), 0 if w[38] == 0 else -1)
    w[39] = 1 if s.a(w[39]) == s.owner else 0
    return {"bump": w, "log": s.log()}


def make_run(entry):
    def run(s, case):
        try:
            build(s, case)
            s.call(entry)
            return observe(s, case)
        finally:
            s.done()
    return run


def mk_ppc(entry):
    r = make_run(entry)
    return lambda e, case: r(Ppc(e, case), case)


def mk_host(entry):
    r = make_run(entry)
    return lambda lib, case: r(Host(lib, case), case)


# ---- the cases

def rpos(r, big=0.1):
    k = r.random()
    if k < 0.45:
        return r.randrange(-60, 61)
    if k < 1 - big:
        return r.randrange(-4000, 4001)
    return r.randrange(-400000, 400001)


def robj(r, live=None, outside=None):
    flags = 0
    if live is None:
        live = r.random() < 0.85
    if live:
        flags |= 1
    if r.random() < 0.5:
        flags |= 0x20
    if outside is None:
        outside = r.random() < 0.5
    if outside:
        flags |= 0x40
    flags |= r.choice([0, 0, 2, 0x80, 0x10000])
    return dict(flags=flags, x=rpos(r), y=rpos(r), r1=r.choice([r.randrange(1, 50), r.randrange(1, 3000), r.randrange(1, 40000)]),
                r2=r.choice([r.randrange(1, 50), r.randrange(1, 3000)]))


def vel(r):
    return r.choice([0, r.randrange(-30, 31), r.randrange(-3000, 3001), r.randrange(-100000, 100001)])


def base(r):
    objs = [robj(r) for _ in range(8)]
    live = sum(1 for o in objs if o["flags"] & 1)
    cur = robj(r, live=True)
    kind = r.choice(KINDS + [0, 7, -6, -1, -14])
    c = dict(
        b=dict(flags=r.choice([0, 1, 0xFFFF, 0x20000]), x=rpos(r), y=rpos(r), vx=vel(r), vy=vel(r), dirX=vel(r), dirY=vel(r),
               push=r.choice([0, r.randrange(-5000, 5000), r.randrange(-100000, 100000)]),
               counter=r.choice([-1, -5, 0, 3, 100])),
        kind=kind, scale=r.choice([0, 1024, 2048, 700, -300, r.randrange(-2000, 3000)]),
        cur=cur, objs=objs, sel=r.choice([-1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8]),
        first=r.random() < 0.9, count=r.randrange(0, live + 1),
        queue=[r.choice([0, 1]) for _ in range(r.randrange(1, 12))])
    c["queues"] = {"PointInColObject": c["queue"]}
    return c


def fix(c, entry):
    """Keep the original's unset r23 out of the world: a box current object with a pushing kind overshoots."""
    if entry == FN_IN and c["kind"] in KINDS:
        sel = c["sel"]
        o = c["cur"] if sel == 0 else (c["objs"][sel - 1] if sel > 0 else None)
        if o is not None and o["flags"] & 0x20 and abs(c["b"]["x"] - o["x"]) <= o["r1"] and abs(c["b"]["y"] - o["y"]) <= o["r2"]:
            c["b"]["x"] = o["x"] + o["r1"] + 1 + abs(c["b"]["x"]) % 50
    # circle centre hit exactly (a zero divisor) is allowed: the host copies the emulator's answer for divw by 0
    return c


def rnd_case(entry):
    def f(r):
        c = base(r)
        if r.random() < 0.5:
            c["queue"][:] = [0] * r.randrange(0, 4) + [1] + c["queue"]
        return fix(c, entry)
    return f


def edges(entry, seed):
    def f():
        r = random.Random(seed)
        for _ in range(3):
            for kind in KINDS[:2] + [0]:
                for box in (False, True):
                    for sel in (-1, 0, 2):
                        for q in ([1], [0, 0, 0], [0, 1], [0, 0, 1, 1], [0]):
                            c = base(r)
                            c["kind"], c["sel"], c["queue"][:] = kind, sel, q
                            c["queues"] = {"PointInColObject": c["queue"]}
                            c["cur"]["flags"] = (c["cur"]["flags"] & ~0x20) | (0x20 if box else 0) | 1
                            c["count"] = sum(1 for o in c["objs"] if o["flags"] & 1)
                            c["first"] = True
                            yield fix(c, entry)
        # boundaries of the box test: distance exactly the radius, one more, one less, on both axes and signs
        for d in (-1, 0, 1):
            for sign in (-1, 1):
                for axis in "xy":
                    c = base(r)
                    c["kind"], c["sel"], c["queue"][:] = -5, 0, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
                    c["queues"] = {"PointInColObject": c["queue"]}
                    c["cur"]["flags"] = 0x21
                    c["count"] = 0
                    o = c["cur"]
                    c["b"]["x"] = o["x"] + sign * (o["r1"] + d) if axis == "x" else o["x"] + o["r1"] + 1
                    c["b"]["y"] = o["y"] + sign * (o["r2"] + d) if axis == "y" else o["y"] + 1
                    yield fix(c, entry)
    return f


TESTS = [
    FnTest("BMP_InsideArea", mk_ppc(FN_IN), mk_host(FN_IN), edge=edges(FN_IN, 5050), rnd=rnd_case(FN_IN)),
    FnTest("BMP_OutsideArea", mk_ppc(FN_OUT), mk_host(FN_OUT), edge=edges(FN_OUT, 5051), rnd=rnd_case(FN_OUT)),
]
