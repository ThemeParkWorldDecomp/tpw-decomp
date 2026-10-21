"""Unit v5257, SetCoasterEditState / RejoinTrack / SetPlaceLoop, against libtpw_unit_SetCoasterEditState.dylib
(tests/unit_SetCoasterEditState.cpp), main app, src/game/rides/CoasterEdit.cpp:
  SetCoasterEditState 0x10053de8, RejoinTrack 0x10053eac, SetPlaceLoop 0x10054554

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian, host library little-endian). The callees
outside the unit are fakes on both sides recording their arguments: DisableCurrentPylonEditState, SetCoasterButtonState,
GetCoasterEditAvailableButtons (which writes the two button masks from the case), the two UIInterface calls, SetCoasterState, FlagGenerateTrack.

The world: the globals, an instance with three braces in a pool (the slots at +0x58, +0x60, +0x64 point into it, aliases included), the
editor's coaster with a type word. Compared: the answer, the globals, the instance word +0x3c, which pool brace each slot holds, the
braces' words, the event log. No fused operations. Not tested: a NULL brace in RejoinTrack (the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetCoasterEditState"
MASK = 0xFFFFFFFF

FN_SCE, FN_REJOIN, FN_LOOP = 0x10053DE8, 0x10053EAC, 0x10054554
SIZES = {FN_SCE: 152, FN_REJOIN: 180, FN_LOOP: 92}
OWN = set(SIZES)

DISABLE, SET_BTN, GET_BTN, CH_EN, CH_DN = 0x10053B58, 0x10057064, 0x10057278, 0x101F0F94, 0x101F0F2C
SET_STATE, FLAG_GEN = 0x10042BB8, 0x10040F08

ADDR = dict(edit=0x102B68B0, flags=0x102C1794, w1790=0x102C1790, enabled=0x102C16D4, down=0x102C16D8, ui=0x10396440)
HOST_LAYOUT = [8, 120, 88, 424, 464]   # the host struct has native pointers: type, braces, b3c, prev, b64 host offsets


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_disable(e):
    e.cl.rec("DisableCurrentPylonEditState", ())


def st_set_btn(e):
    e.cl.rec("SetCoasterButtonState", (e.r(3) & MASK, e.r(4) & MASK))


def st_get_btn(e):
    L = e.cl
    L.rec("GetCoasterEditAvailableButtons", (e.r(3) & MASK,))
    e.write(ADDR["enabled"], struct.pack(">I", L.ret("get_en") & MASK))
    e.write(ADDR["down"], struct.pack(">I", L.ret("get_dn") & MASK))


def st_ch_en(e):
    L = e.cl
    L.rec("ChangeCoasterButtonsEnabled", (L.lab(e.r(3)), e.r(4) & MASK))


def st_ch_dn(e):
    L = e.cl
    L.rec("ChangeCoasterButtonsDown", (L.lab(e.r(3)), e.r(4) & MASK))


def st_set_state(e):
    L = e.cl
    L.rec("SetCoasterState", (L.lab(e.r(3)), e.r(4) & MASK))


def st_flag_gen(e):
    L = e.cl
    L.rec("FlagGenerateTrack", (L.lab(e.r(3)), s32(e.r(4))))


for a, f in ((DISABLE, st_disable), (SET_BTN, st_set_btn), (GET_BTN, st_get_btn), (CH_EN, st_ch_en), (CH_DN, st_ch_dn),
             (SET_STATE, st_set_state), (FLAG_GEN, st_flag_gen)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    off = dict(type=4, braces=0x58, b3c=0x3C, prev=0x60, b64=0x64)

    def rdptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["ui"]: 1}, case["queues"])
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
        self.write(a, struct.pack(">I", v))

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK if entry == FN_LOOP else 0     # the other two answer nothing


class Host(Side):
    fmt = "<I"

    def rdptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        CL.host_begin(lib, {1: self.g("ui")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sce_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sce_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_SCE:
            self.lib.tpw_sce_set(args[0])
            return 0
        if entry == FN_REJOIN:
            self.lib.tpw_sce_rejoin(args[0])
            return 0
        return self.lib.tpw_sce_loop() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sce_addr.restype, lib.tpw_sce_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sce_alloc.restype, lib.tpw_sce_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sce_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sce_set.restype, lib.tpw_sce_set.argtypes = None, [C.c_uint32]
    lib.tpw_sce_rejoin.restype, lib.tpw_sce_rejoin.argtypes = None, [C.c_uint64]
    lib.tpw_sce_loop.restype, lib.tpw_sce_loop.argtypes = C.c_int32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sce_layout(out)
    lay = list(out)[:n]
    assert lay == HOST_LAYOUT, lay
    lib.off = dict(zip(("type", "braces", "b3c", "prev", "b64"), lay))
    lib.bound = True


def build(s, case):
    ctx = {}
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("w1790"), [case["w1790"]])
    s.words(s.g("enabled"), [case["en0"]])
    s.words(s.g("down"), [case["dn0"]])
    if case["op"] == "rejoin":
        inst = s.alloc(0x100)
        pool = [s.alloc(0x40) for _ in range(3)]
        for b, ws in zip(pool, case["braces"]):
            s.words(b, ws)
        o = s.off
        s.words(inst + o["b3c"], [case["w3c"]])
        s.ptr(inst + o["braces"], pool[case["k"]])
        s.ptr(inst + o["prev"], pool[case["i"]])
        s.ptr(inst + o["b64"], pool[case["j"]])
        ctx["inst"], ctx["pool"] = inst, pool
    if case["op"] == "loop":
        if case["inst"]:
            inst = s.alloc(0x100)
            ty = s.alloc(16)
            s.words(ty, [case["type0"]])
            s.ptr(inst + s.off["type"], ty)
            s.ptr(s.g("edit"), inst)
        else:
            s.ptr(s.g("edit"), 0)
    return ctx


def execute(s, case, ctx):
    if case["op"] == "sce":
        return s.call(FN_SCE, [case["state"]])
    if case["op"] == "rejoin":
        return s.call(FN_REJOIN, [ctx["inst"]])
    return s.call(FN_LOOP, [])


def observe(s, case, ctx, answer):
    d = {"answer": answer, "log": s.log()}
    for n in ("flags", "w1790", "enabled", "down"):
        d[n] = s.rd_words(s.g(n), 1)
    if case["op"] == "rejoin":
        inst, pool = ctx["inst"], ctx["pool"]
        d["w3c"] = s.rd_words(inst + s.off["b3c"], 1)
        d["slots"] = [pool.index(s.rdptr(inst + s.off[n])) for n in ("braces", "prev", "b64")]
        d["braces"] = [s.rd_words(b, 4) for b in pool]
    return d


def run(s, case):
    try:
        ctx = build(s, case)
        return observe(s, case, ctx, execute(s, case, ctx))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "queues": {"get_en": [r.getrandbits(32)], "get_dn": [r.getrandbits(32)]},
            "flags": r.getrandbits(32), "w1790": r.getrandbits(32), "en0": r.getrandbits(32), "dn0": r.getrandbits(32)}


def sce_case(r, state=None):
    c = base(r, "sce")
    c["state"] = r.choice([0, 1, 0x10, 0xFFFFFFFF, r.getrandbits(32)]) if state is None else state
    return c


def sce_edges():
    r = random.Random(11)
    for st in (0, 1, 0x10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        yield sce_case(r, st)


def sce_random(r):
    return sce_case(r)


def rejoin_case(r, i=None, j=None, k=None, bit=None):
    c = base(r, "rejoin")
    c["i"] = r.randrange(3) if i is None else i
    c["j"] = r.randrange(3) if j is None else j
    c["k"] = r.randrange(3) if k is None else k
    c["w3c"] = r.getrandbits(32)
    braces = [[r.getrandbits(32) for _ in range(4)] for _ in range(3)]
    if bit is not None:
        braces[c["i"]][2] = (braces[c["i"]][2] & ~0x400) | (0x400 if bit else 0)
    c["braces"] = braces
    return c


def rejoin_edges():
    r = random.Random(12)
    for i, j, k in ((0, 1, 2), (0, 0, 1), (1, 1, 1), (2, 0, 0), (0, 2, 1)):
        for bit in (0, 1):
            yield rejoin_case(r, i, j, k, bit)
    c = rejoin_case(r, 0, 1, 2, 1)
    c["braces"] = [[0xFFFFFFFF] * 4 for _ in range(3)]
    c["w3c"] = 0xFFFFFFFF
    yield c
    c = rejoin_case(r, 0, 1, 2, 0)
    c["braces"] = [[0xFFFFFFFF] * 4 for _ in range(3)]
    c["braces"][0][2] = 0xFFFFFBFF
    c["w3c"] = 0
    yield c


def rejoin_random(r):
    return rejoin_case(r)


def loop_case(r, flags=None, inst=None, bit2=None):
    c = base(r, "loop")
    if flags is not None:
        c["flags"] = flags
    c["inst"] = r.choice([0, 1, 1]) if inst is None else inst
    c["type0"] = r.getrandbits(32)
    if bit2 is not None:
        c["type0"] = (c["type0"] & ~2) | (2 if bit2 else 0)
    return c


def loop_edges():
    r = random.Random(13)
    for flags in (0, 0x40, 0xFFFFFFBF, 0xFFFFFFFF, 0x3F, 0x80):
        for inst in (0, 1):
            for bit2 in (0, 1):
                yield loop_case(r, flags, inst, bit2)


def loop_random(r):
    return loop_case(r)


TESTS = [
    FnTest("SetCoasterEditState", ppc, host, edge=sce_edges, rnd=sce_random),
    FnTest("RejoinTrack", ppc, host, edge=rejoin_edges, rnd=rejoin_random),
    FnTest("SetPlaceLoop", ppc, host, edge=loop_edges, rnd=loop_random),
]
