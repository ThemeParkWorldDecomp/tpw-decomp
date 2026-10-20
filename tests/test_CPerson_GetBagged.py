"""Unit v5293, the CPerson balloon, happiness and catch helpers, against libtpw_unit_CPerson_GetBagged.dylib (tests/unit_CPerson_GetBagged.cpp),
main app ("SimTheme Park"), src/CPerson_..._CPerson_GetBagged.cpp:
  LosingMods 0x1011b004, DrawBalloon 0x1011b084, MakeBalloonPop 0x1011b11c, GetHappinessAmount 0x1011b198, RaiseHappiness 0x1011b258,
  LowerHappiness 0x1011b384, GetBagged 0x1011b508, IsCatchable 0x1011b700

A case is a person block and a call, built by one piece of Python in both memories. The callees (SCR_KillScript, ESP_ForceGoto, SCR_SetScriptPosition,
KillSpriteScript, SetState, CTRACE, GAME_ASSERTMSG) are fakes on both sides that record their arguments (CTRACE also the %d argument).

The world: the person block (+0x19c happiness, +0x210 balloon handle, +0x218 and +0x21c floats, +0x220 state) and the three balance words
at gMainBalance +0x18, +0x1c, +0x20. Compared: the answer, the log, the whole person block, the balance words.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_GetBagged"
MASK = 0xFFFFFFFF

LOSING, DRAW, POP, AMOUNT, RAISE, LOWER, BAGGED, CATCH = 0x1011b004, 0x1011b084, 0x1011b11c, 0x1011b198, 0x1011b258, 0x1011b384, 0x1011b508, 0x1011b700
SIZES = {LOSING: 76, DRAW: 104, POP: 76, AMOUNT: 124, RAISE: 236, LOWER: 240, BAGGED: 100, CATCH: 104}
OWN = set(SIZES)

SCR_KILL, KILL_SPRITE, SET_STATE, CTRACE, ESP_GOTO, ASSERT, SET_POS = 0x100d9478, 0x100fac4c, 0x10120354, 0x1006fcbc, 0x100da318, 0x1000dde8, 0x100d932c

ADDR = dict(bal=0x102b4568, target=0x1029fd84)
HOST_LAYOUT = [1]
BLOCK = 0x240


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

STUBS.internal(SCR_KILL, CL.recorder("SCR_KillScript", "i"))
STUBS.internal(KILL_SPRITE, CL.recorder("KillSpriteScript", "L"))
STUBS.internal(SET_STATE, CL.recorder("SetState", "Li"))
STUBS.internal(ESP_GOTO, CL.recorder("ESP_ForceGoto", "iL"))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))

_ctrace = CL.recorder("CTRACE", "us")


def stub_ctrace(e):
    text = e.cstr(e.r(4))
    if isinstance(text, bytes):
        text = text.decode("latin-1")
    _ctrace(e)
    if "%d" in text:
        e.cl.rec("CTRACEarg", (s32(e.r(5)),))


STUBS.internal(CTRACE, stub_ctrace)


def stub_setpos(e):
    e.cl.rec("SCR_SetScriptPosition", (s32(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), s32(e.r(7))))   # floats reserve r4 to r6


STUBS.internal(SET_POS, stub_setpos)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(BLOCK)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.obj + 0xC: 2, ADDR["target"]: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, a):
        entry = FN[op]
        if op == "draw":
            args = [self.obj, T.F(bitsf(a[0])), T.F(bitsf(a[1])), T.F(bitsf(a[2])), a[3] & MASK]
        elif op in ("raise", "lower", "amount"):
            args = [self.obj, a[0] & MASK]
        else:
            args = [self.obj]
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK if op in ("amount", "catch") else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_gb_alloc(BLOCK)
        CL.host_begin(lib, {1: self.obj, 2: self.obj + 0xC, 3: self.g("target")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gb_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, a):
        f = getattr(self.lib, "tpw_gb_" + HOSTNAME[op])
        if op == "draw":
            f(self.obj, bitsf(a[0]), bitsf(a[1]), bitsf(a[2]), s32(a[3]))
            return 0
        if op in ("raise", "lower", "amount"):
            r = f(self.obj, s32(a[0]))
            return r & MASK if op == "amount" else 0
        r = f(self.obj)
        return r & MASK if op == "catch" else 0


FN = dict(losing=LOSING, draw=DRAW, pop=POP, amount=AMOUNT, raise_=RAISE, lower=LOWER, bagged=BAGGED, catch=CATCH)
FN["raise"] = RAISE
HOSTNAME = dict(losing="losing", draw="draw", pop="pop", amount="amount", raise_="raise", lower="lower", bagged="getbagged", catch="catch")
HOSTNAME["raise"] = "raise"


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gb_addr.restype, lib.tpw_gb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gb_alloc.restype, lib.tpw_gb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gb_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("getbagged", "pop", "losing"):
        f = getattr(lib, "tpw_gb_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    for n in ("lower", "raise"):
        f = getattr(lib, "tpw_gb_" + n)
        f.restype, f.argtypes = None, [C.c_uint64, C.c_int32]
    lib.tpw_gb_catch.restype, lib.tpw_gb_catch.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_gb_amount.restype, lib.tpw_gb_amount.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_gb_draw.restype, lib.tpw_gb_draw.argtypes = None, [C.c_uint64, C.c_float, C.c_float, C.c_float, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


FLOAT_IDX = (0x19C // 4, 0x218 // 4, 0x21C // 4)


def build(s, case):
    s.words(s.g("bal") + 0x18, case["bal"])
    s.words(s.obj, case["block"])


def observe(s, case, answer):
    blk = s.rd_words(s.obj, BLOCK // 4)
    for i in FLOAT_IDX:
        blk[i] = canon_f(blk[i])
    return {"answer": answer, "log": s.log(), "block": blk, "bal": s.rd_words(s.g("bal") + 0x18, 3)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["args"]))
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
    if k < 0.5:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def mkcase(r, op, hap=None, bal=None, amount=None, state=None, handle=None):
    block = [0] * (BLOCK // 4)
    block[0x19C // 4] = rf(r) if hap is None else hap
    block[0x210 // 4] = r.choice([0, 1, 77, r.randrange(1, 1000), 0x80000001]) if handle is None else handle
    block[0x218 // 4] = rf(r)
    block[0x21C // 4] = rf(r)
    block[0x220 // 4] = r.choice([6, 7, 8, 9, 10, r.randrange(-3, 25), r.getrandbits(32)]) if state is None else state
    block[0x1A0 // 4] = r.getrandbits(32)
    if bal is None:
        bal = [r.choice([r.randrange(0, 256), r.getrandbits(32), 0, 255]) for _ in range(3)]
    if amount is None:
        amount = r.choice([-2, -1, 0, 1, 2, 3, 4, 0x7FFFFFFF, r.randrange(-5, 8), s32(r.getrandbits(32))])
    if op == "draw":
        args = [rf(r), rf(r), rf(r), r.choice([0, 1, -1, r.getrandbits(32)])]
    else:
        args = [amount]
    return {"op": op, "queues": {}, "block": block, "bal": bal, "args": args}


def edges_for(op, seed):
    def gen():
        r = random.Random(seed)
        if op in ("raise", "lower", "amount"):
            for amt in (-2, -1, 0, 1, 2, 3, 4, 0x7FFFFFFF, -0x80000000):
                for bal in ([7, 9, 11], [0, 0, 0], [255, 255, 255], [0x1234, 0x15678, 0xFFFFFF01]):
                    v = bal[amt] & 0xFF if amt in (0, 1, 2) else 0
                    for h in (0, 0x80000000, fb(100.0), fb(100.0 - v), fb(100.0 + v), fb(float(v)), fb(-float(v)),
                              fb(100.0 - v + 0.001), fb(100.0 + v - 0.001), 0x7FC00000, 0x7F800000, 0xFF800000, fb(50.0), fb(-3.0)):
                        yield mkcase(r, op, hap=h, bal=bal, amount=amt)
        elif op == "catch":
            for st in list(range(-1, 14)) + [0x80000000, 0x7FFFFFFF]:
                yield mkcase(r, op, state=st)
        elif op in ("pop", "bagged", "draw"):
            for hd in (0, 1, -1, 0x7FFFFFFF):
                for _ in range(3):
                    yield mkcase(r, op, handle=hd & MASK)
        else:
            yield mkcase(r, op)
    return gen


def rnd_for(op):
    return lambda r: mkcase(r, op)


TESTS = [
    FnTest(name, ppc, host, edge=edges_for(op, i + 1), rnd=rnd_for(op))
    for i, (name, op) in enumerate([
        ("CPerson::GetBagged", "bagged"), ("CPerson::LowerHappiness", "lower"), ("CPerson::IsCatchable", "catch"),
        ("CPerson::RaiseHappiness", "raise"), ("CPerson::GetHappinessAmount", "amount"), ("CPerson::MakeBalloonPop", "pop"),
        ("CPerson::DrawBalloon", "draw"), ("CPerson::LosingMods", "losing")])
]
