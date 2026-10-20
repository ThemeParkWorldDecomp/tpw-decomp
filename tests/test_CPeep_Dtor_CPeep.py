"""Unit v5298, the CPeep destructor, HideMySprite, DrawMySpriteAt and FinalDestroy, against libtpw_unit_CPeep_Dtor_CPeep.dylib
(tests/unit_CPeep_Dtor_CPeep.cpp), main app ("SimTheme Park"), src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_Dtor_CPeep.cpp:
  CPeep::~CPeep 0x10116d18, CPeep::HideMySprite 0x10114758, CPeep::DrawMySpriteAt 0x10114624, CPeep::FinalDestroy 0x101130ac

A case is a peep block (0x300 bytes of random words with the kind byte, the script handle and the +0x210 word steered over the branches)
written twice by one piece of Python code, and a call. Every callee outside the unit is a fake on both sides that records its arguments
(pointers as their offset from the peep block, floats as canonical bits) and answers from the case: Vehicle::~Vehicle, the script
members, CThoughts members, CPerson::MakeBalloonPop, CThing::FinalDestroy, CPeep::ReportAllEvents, DetermineHFieldHeight,
SCR_SetScriptPosition.

What is compared: the event log and the whole block after the call. No fused operations (one fsubs).
Not tested: the destructor's delete-storage flag above 0 (the host never frees; the PowerPC call passes 0) and a NULL this.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPeep_Dtor_CPeep"
MASK = 0xFFFFFFFF

DTOR, HIDE, DRAW, FINAL = 0x10116d18, 0x10114758, 0x10114624, 0x101130ac
SIZES = {DTOR: 104, HIDE: 100, DRAW: 232, FINAL: 96}
OWN = set(SIZES)

VEH_DTOR = 0x101332cc
KILL, THOUGHT_REMOVE, BALLOON = 0x100fac4c, 0x1012f35c, 0x1011b11c
SPAWN, ISRUN, SETSCR = 0x100fab9c, 0x100faad0, 0x100faa74
HEIGHT, SETPOS = 0x1005ea70, 0x100d932c
THING_FINAL, REPORT, THOUGHTS_FINAL = 0x1012e9f8, 0x10115c3c, 0x1012ed84

BLOCK = 0x300
HOST_LAYOUT = [12]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(e, p):
    return (p - e.cl.base) & MASK


def mk_obj(name, addr):
    def fn(e):
        e.cl.rec(name, (rel(e, e.r(3)),))
    STUBS.internal(addr, fn)


for _n, _a in (("Kill", KILL), ("RemoveBubble", THOUGHT_REMOVE), ("Balloon", BALLOON), ("ThingFinal", THING_FINAL),
               ("Report", REPORT), ("ThoughtsFinal", THOUGHTS_FINAL)):
    mk_obj(_n, _a)


def stub_veh(e):
    e.cl.rec("VehDtor", (rel(e, e.r(3)),))


def stub_spawn(e):
    e.cl.rec("Spawn", (rel(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_isrun(e):
    e.cl.rec("IsRun", (rel(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("IsRun") & 1)


def stub_setscr(e):
    e.cl.rec("SetScr", (rel(e, e.r(3)), s32(e.r(4))))


def stub_height(e):
    e.cl.rec("Height", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), s32(e.r(5))))
    e.set_f1(bitsf(e.cl.ret("Height")))


def stub_setpos(e):
    e.cl.rec("SetPos", (s32(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), s32(e.r(7))))


STUBS.internal(VEH_DTOR, stub_veh)
STUBS.internal(SPAWN, stub_spawn)
STUBS.internal(ISRUN, stub_isrun)
STUBS.internal(SETSCR, stub_setscr)
STUBS.internal(HEIGHT, stub_height)
STUBS.internal(SETPOS, stub_setpos)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"
    kind_shift = 8

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        a = self.e.alloc(n)
        self.L.base = a
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, this, args):
        if op == "dtor":
            T.call_ppc(self.e, DTOR, [this, 0])
        elif op == "hide":
            T.call_ppc(self.e, HIDE, [this])
        elif op == "final":
            T.call_ppc(self.e, FINAL, [this])
        else:
            x, y, z, frame, script = args
            T.call_ppc(self.e, DRAW, [this, T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), frame & MASK, script & MASK])


class Host(Side):
    fmt = "<I"
    kind_shift = 16

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cpeep_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, this, args):
        lib = self.lib
        if op == "dtor":
            lib.tpw_cpeep_dtor(this)
        elif op == "hide":
            lib.tpw_cpeep_hide(this)
        elif op == "final":
            lib.tpw_cpeep_final(this)
        else:
            x, y, z, frame, script = args
            lib.tpw_cpeep_draw(this, bitsf(x), bitsf(y), bitsf(z), s32(frame), s32(script))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_cpeep_"
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("dtor", "hide", "final"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = None, [C.c_uint64]
    lib.tpw_cpeep_draw.restype = None
    lib.tpw_cpeep_draw.argtypes = [C.c_uint64, C.c_float, C.c_float, C.c_float, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        this = s.alloc(BLOCK)
        blk = list(case["block"])
        kind = blk[0] >> 8 & 0xFF
        blk[0] = kind << s.kind_shift      # byte 2 is bits 8..15 of the word on the Mac, 16..23 on the host
        s.words(this, blk)
        s.call(case["op"], this, case.get("args"))
        out = list(struct.unpack(s.fmt[0] + "%dI" % (BLOCK // 4), s.read(this, BLOCK)))
        out[0] = (out[0] >> s.kind_shift) & 0xFF
        return {"log": s.log(), "block": out}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op, kind=None, handle=None, w210=None):
    blk = [r.getrandbits(32) for _ in range(BLOCK // 4)]
    if kind is None:
        kind = r.choice([1, 1, 0, 2, 3, 0xFF])
    blk[0] = (blk[0] & 0xFFFF00FF) | (kind << 8)
    if handle is None:
        handle = r.choice([0, 0, r.getrandbits(32), 1, 0xFFFFFFFF])
    blk[3] = handle
    if w210 is None:
        w210 = r.choice([0, 1, r.getrandbits(32), 0x80000000])
    blk[0x210 // 4] = w210
    return {"op": op, "block": blk, "queues": {"IsRun": [r.choice([0, 1])], "Height": [rf(r)]}}


def draw_case(r, handle=None, run=None):
    c = base(r, "draw", handle=handle)
    if run is not None:
        c["queues"]["IsRun"] = [run]
    c["args"] = [rf(r), rf(r), rf(r), r.choice([0, 1, -1, 2, r.getrandbits(32)]) & MASK, r.choice([2, 3, 0, 12, r.getrandbits(32)]) & MASK]
    return c


def op_edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        for kind in (0, 1, 2, 0xFF):
            for w210 in (0, 1, 0x80000000):
                for handle in (0, 5):
                    c = base(r, op, kind, handle, w210)
                    if op == "draw":
                        c = draw_case(r, handle)
                        c["block"][0] = (c["block"][0] & 0xFFFF00FF) | (kind << 8)
                        c["block"][0x210 // 4] = w210
                    yield c
        if op == "draw":
            for run in (0, 1):
                for handle in (0, 7):
                    yield draw_case(r, handle, run)
    return gen


def op_random(op):
    def gen(r):
        return draw_case(r) if op == "draw" else base(r, op)
    return gen


TESTS = [FnTest(n, ppc, host, edge=op_edges(o), rnd=op_random(o))
         for n, o in (("CPeep::~CPeep", "dtor"), ("CPeep::HideMySprite", "hide"), ("CPeep::DrawMySpriteAt", "draw"),
                      ("CPeep::FinalDestroy", "final"))]
