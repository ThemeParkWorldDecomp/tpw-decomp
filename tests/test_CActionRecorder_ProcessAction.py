"""Unit v5254, the eight CActionRecorder::ProcessAction overloads, against libtpw_unit_CActionRecorder_ProcessAction.dylib, main app,
src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction.cpp:
  0x10145b28 (action), 0x10145d34 (u8,u8,u16), 0x10145c24 (u8,u8), 0x10146058 (u16,u8), 0x10146140 (float,u16),
  0x10146248 (float,float,u16), 0x1014634c (u16,u16), 0x1014644c (u16,u16,u16)

A case is an action number and operands. Every callee (CTRACE, the mouse, ride and layout functions) is a fake on both sides recording its
arguments; CTRACE records the level, the format's index and the argument words the format reads (a %g is two words of the double).
Compared: the call log. No fused operations. Operands are in range of their types (the callers pass them so).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_ProcessAction"
MASK = 0xFFFFFFFF

FNS = {0: 0x10145b28, 1: 0x10145d34, 2: 0x10145c24, 3: 0x10146058, 4: 0x10146140, 5: 0x10146248, 6: 0x1014634c, 7: 0x1014644c}
SIZES = {0x10145b28: 184, 0x10145d34: 184, 0x10145c24: 200, 0x10146058: 160, 0x10146140: 192, 0x10146248: 188, 0x1014634c: 184,
         0x1014644c: 140}
OWN = set(SIZES)

FORMATS = ["Erroneous stuff going down\n", "AR replay - %d\n", "AR replay - %d (%d, %d)\n", "AR replay - %d (%d, %d, %d)\n",
           "AR replay - %d (%g, %d)\n", "AR replay - %d (%g, %g, %d)\n"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctrace(e):
    L = e.cl
    fmt = e.cstr(e.r(4))
    fid = FORMATS.index(fmt) if fmt in FORMATS else -1
    regs = [e.r(i) & MASK for i in range(5, 11)]
    words = []
    n = 0
    i = 0
    while i < len(fmt):
        if fmt[i] == "%":
            i += 1
            if fmt[i] == "d":
                words.append(s32(regs[n]))
                n += 1
            elif fmt[i] == "g":
                hi, lo = regs[n], regs[n + 1]
                n += 2
                if (hi & 0x7FF00000) == 0x7FF00000 and ((hi & 0xFFFFF) or lo):
                    hi, lo = 0x7FF80000, 0
                words += [s32(hi), s32(lo)]
        i += 1
    L.rec("CTRACE", tuple([s32(e.r(3)), fid] + words))


def stub_down(e):
    e.cl.rec("Down", (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, s32(e.r(6))))


def stub_up(e):
    e.cl.rec("Up", (s32(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, s32(e.r(6))))


def stub_ride(e):
    e.cl.rec("SetRide", (struct.unpack(">H", e.read(e.r(3), 2))[0], s32(e.r(4))))


def fl(name):
    def f(e):
        e.cl.rec(name, (fb(e.f(1)), e.r(4) & MASK))
    return f


def stub_wobble(e):
    e.cl.rec("Wobble", (fb(e.f(1)), fb(e.f(2)), e.r(5) & MASK))


def plain(name):
    def f(e):
        e.cl.rec(name, ())
    return f


def ints(name, n, mask):
    def f(e):
        e.cl.rec(name, tuple(e.r(3 + i) & mask for i in range(n)))
    return f


STUBS.internal(0x1006fcbc, stub_ctrace)
STUBS.internal(0x10084a00, stub_down)
STUBS.internal(0x10084ca4, stub_up)
STUBS.internal(0x100915bc, stub_ride)
STUBS.internal(0x10055eac, fl("Loft"))
STUBS.internal(0x10055ff0, fl("Rotate"))
STUBS.internal(0x10056180, stub_wobble)
STUBS.internal(0x10058b68, plain("Normal"))
STUBS.internal(0x10058bb8, plain("Special"))
STUBS.internal(0x10058e48, plain("Regen"))
STUBS.internal(0x10058f4c, plain("DelBrace"))
STUBS.internal(0x10058cf8, ints("DelMulti", 2, 0xFFFF))
STUBS.internal(0x10059120, ints("Speed", 2, MASK))
STUBS.internal(0x10058da4, ints("Move", 3, 0xFFFF))


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc_args(case):
    op, a, b, c = case["op"], case["a"], case["b"], case["c"]
    t, act = 0, case["action"] & MASK
    if op == 0:
        return [t, act]
    if op == 4:
        return [t, act, T.F(bitsf(case["fa"])), b]
    if op == 5:
        return [t, act, T.F(bitsf(case["fa"])), T.F(bitsf(case["fb"])), c]
    if op in (2, 3, 6):
        return [t, act, a, b]
    return [t, act, a, b, c]


def ppc(e, case):
    e.cl = L = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FNS[case["op"]], ppc_args(case))
        return {"log": L.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    if not getattr(lib, "bound", False):
        lib.tpw_pa_addr.restype, lib.tpw_pa_addr.argtypes = C.c_uint64, [C.c_char_p]
        lib.tpw_pa_alloc.restype, lib.tpw_pa_alloc.argtypes = C.c_uint64, [C.c_uint32]
        lib.tpw_pa_layout.argtypes = [C.POINTER(C.c_int64)]
        lib.tpw_pa_call.restype = None
        lib.tpw_pa_call.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_float, C.c_float]
        lib.bound = True
    CL.host_begin(lib, {}, {})
    lib.tpw_pa_call(case["op"], s32(case["action"]), case["a"], case["b"], case["c"], bitsf(case["fa"]), bitsf(case["fb"]))
    return {"log": CL.host_end(lib)}


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]

ACTIONS = {0: [0x36, 0x37, 0x38, 0x39, 0x3F, 0x40, 0x41, 0x42, 0x43],
           1: [0x27, 0x28, 0x29, 0x2C, 0x2D, 0x2E],
           2: [0xE, 0xF, 0x10, 0x13, 0x14, 0x15],
           3: [4, 5, 6],
           4: [0x38, 0x39, 0x3A, 0x3B],
           5: [0x3A, 0x3B, 0x3C],
           6: [0x3E, 0x3F, 0x40, 0x44, 0x45, 0x46],
           7: [0x3B, 0x3C, 0x3D]}


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    return fb(r.uniform(-300, 300))


def mk(r, op, action=None):
    wide = op in (3, 6, 7)
    a = r.choice([0, 1, 0xFF, r.randrange(256)]) if op in (1, 2) else r.choice([0, 1, 0xFFFF, r.randrange(65536)])
    b = r.choice([0, 1, 0xFF, r.randrange(256)]) if op in (1, 2, 3) else r.choice([0, 1, 0xFFFF, r.randrange(65536)])
    c = r.choice([0, 1, 0xFFFF, r.randrange(65536)])
    if action is None:
        action = r.choice(ACTIONS[op] + [0, 0xFFFFFFFF, r.randrange(0x100), r.randrange(MASK + 1)])
    return {"op": op, "action": action & MASK, "a": a, "b": b, "c": c, "fa": rf(r), "fb": rf(r)}


def make_test(op):
    def edges():
        r = random.Random(100 + op)
        for act in ACTIONS[op] + [0, 0xFFFFFFFF, 0x80000000]:
            for _ in range(3):
                yield mk(r, op, act)

    def rnd(r):
        return mk(r, op)

    return FnTest("CActionRecorder::ProcessAction", ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(op) for op in range(8)]
