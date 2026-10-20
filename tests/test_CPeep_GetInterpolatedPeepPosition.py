"""Unit v5533, CPeep::GetInterpolatedPeepPosition and CPeep::Serialise, against libtpw_unit_CPeep_GetInterpolatedPeepPosition.dylib
(tests/unit_CPeep_GetInterpolatedPeepPosition.cpp), main app ("SimTheme Park"),
src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_GetInterpolatedPeepPosition.cpp:
  CPeep::GetInterpolatedPeepPosition 0x101147e8, CPeep::Serialise 0x10113138

A case is a peep block (0x1a8 bytes of random words, built twice by the same code: big-endian in the emulator, little-endian on the host) and a
call. The callees outside the unit (CThing / CDelayedScript / Vehicle / CThoughts Serialise, every SAVEVAR / LOADVAR instance, ESP_GetNumberOfSprites,
CPeep::SetDest, CTRACE, Pilot::get_pos) are fakes on both sides that record their arguments (a pointer into the peep as its offset, the text of a
name in the event name) and answer from the case's queues: "R" the result of every serialise call in call order (one failure at a chosen call),
"D" SetDest, "N" the sprite count, "px" / "py" the pilot position.

What is compared: the answer, the whole peep block, the output block (x, y, z, stamp) and the event log. The fused multiply-adds of the
interpolation run under HardwareRounding. Not tested: a sprite count of 0 (the original's divide by zero is undefined).
"""
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CPeep_GetInterpolatedPeepPosition"
MASK = 0xFFFFFFFF

FN_INTERP, FN_SER = 0x101147E8, 0x10113138
SIZES = {FN_INTERP: 416, FN_SER: 1712}
OWN = set(SIZES)

PEEP_WORDS = 0x1A8 // 4
BASE = [0]


def s32(v):
    return T.s32(v)


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


class ROUNDING(HardwareRounding):
    def __enter__(self):
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=FN_INTERP, end=FN_INTERP + SIZES[FN_INTERP] - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_var(kind):
    def f(e):
        L = e.cl
        text = e.cstr(e.r(5))
        L.rec(kind + ":" + text, (L.lab(e.r(3)), (e.r(4) - BASE[0]) & MASK, s32(e.r(6))))
        e.set_r(3, L.ret("R") & MASK)
    return f


def mk_sub(kind):
    def f(e):
        L = e.cl
        L.rec(kind, ((e.r(3) - BASE[0]) & MASK, L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK))
        e.set_r(3, L.ret("R") & MASK)
    return f


VARS = {
    0x100F5658: "SAVEVAR<s>", 0x100F518C: "LOADVAR<s>",
    0x100F5530: "SAVEVAR<Us>", 0x100F5074: "LOADVAR<Us>",
    0x100F1A04: "SAVEVAR<Uc>", 0x100F17C8: "LOADVAR<Uc>",
    0x101162D4: "SAVEVAR<ESP>", 0x101161A8: "LOADVAR<ESP>",
    0x100F58C0: "SAVEVARSTRUCT<map>", 0x100F53CC: "LOADVARSTRUCT<map>",
    0x100F18D8: "SAVEVAR<f>", 0x100F16B4: "LOADVAR<f>",
    0x1000F384: "SAVEVAR<l>", 0x1000EE04: "LOADVAR<l>",
    0x1000F5DC: "SAVEVAR<Ul>", 0x1000F02C: "LOADVAR<Ul>",
    0x1000F4B0: "SAVEVAR<i>", 0x1000EF18: "LOADVAR<i>",
    0x100F45BC: "SAVEVAR<Ui>", 0x100F44A4: "LOADVAR<Ui>",
}
SUBS = {
    0x1012E010: "CThing::Serialise", 0x100FA8D8: "CDelayedScript::Serialise",
    0x10130EB4: "Vehicle::Serialise", 0x1012EDE4: "CThoughts::Serialise",
}
for _a, _n in VARS.items():
    STUBS.internal(_a, mk_var(_n))
for _a, _n in SUBS.items():
    STUBS.internal(_a, mk_sub(_n))


def stub_esp(e):
    L = e.cl
    L.rec("ESP_GetNumberOfSprites", (s32(e.r(3)),))
    e.set_r(3, L.ret("N") & MASK)


def stub_setdest(e):
    L = e.cl
    a, b = e.r(4), e.r(5)
    L.rec("SetDest", ((e.r(3) - BASE[0]) & MASK, (a >> 24) & 255, (a >> 16) & 255, (b >> 24) & 255, (b >> 16) & 255))
    e.set_r(3, L.ret("D") & MASK)


def stub_ctrace(e):
    L = e.cl
    L.rec("CTRACE:" + e.cstr(e.r(4)), (e.r(3) & MASK,))


def stub_getpos(e):
    L = e.cl
    L.rec("Pilot::get_pos", ((e.r(3) - BASE[0]) & MASK,))
    e.write(e.r(4), struct.pack(">I", L.ret("px") & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("py") & MASK))


STUBS.internal(0x1007C8B0, stub_esp)
STUBS.internal(0x101154F0, stub_setdest)
STUBS.internal(0x1006FCBC, stub_ctrace)
STUBS.internal(0x10134888, stub_getpos)


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
        self.peep = e.alloc(0x1C0)
        self.file = e.alloc(16)
        self.out = e.alloc(16)
        BASE[0] = self.peep
        self.L = e.cl = CL.PpcLog({self.file: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def serialise(self, save, version, unused):
        r3, _, _ = T.call_ppc(self.e, FN_SER, [self.peep, self.file, save & MASK, version & MASK, unused & MASK])
        return r3 & MASK

    def interp(self, t, ranged):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_INTERP, [self.peep, T.F(bitsf(t)), ranged & MASK, self.out, self.out + 4, self.out + 8, self.out + 12])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        import ctypes as C
        self.C = C
        self.lib = lib
        bind(lib)
        self.peep = lib.tpw_peep_alloc(0x1C0)
        self.file = lib.tpw_peep_alloc(16)
        self.out = lib.tpw_peep_alloc(16)
        CL.host_begin(lib, {1: self.file}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return self.C.string_at(a, n)

    def write(self, a, data):
        self.C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def serialise(self, save, version, unused):
        return self.lib.tpw_peep_serialise(self.peep, self.file, s32(save), s32(version), unused & MASK) & MASK

    def interp(self, t, ranged):
        self.lib.tpw_peep_interp(self.peep, bitsf(t), s32(ranged), self.out)
        return 0


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_peep_alloc.restype, lib.tpw_peep_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_peep_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_peep_serialise.restype = C.c_int32
    lib.tpw_peep_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_peep_interp.restype = None
    lib.tpw_peep_interp.argtypes = [C.c_uint64, C.c_float, C.c_int32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_peep_layout(out)
    assert list(out)[:n] == [2, 2], list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.peep, case["peep"])
    s.write(s.peep + 0x18, bytes(case["dest"]))     # the two MapPos are bytes: the same byte string in either order
    s.words(s.out, [0xDEADBEEF] * 4)


def observe(s, case, answer):
    peep = s.rd_words(s.peep, PEEP_WORDS)
    peep[6] = list(s.read(s.peep + 0x18, 4))
    return {"answer": answer, "log": s.log(), "peep": peep, "out": s.rd_words(s.out, 4)}


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "ser":
            ans = s.serialise(*case["args"])
        else:
            ans = s.interp(*case["args"])
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------------ cases

def ri(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 1])
    if k < 0.7:
        return r.randrange(-100000, 100000) & MASK
    return r.randrange(-(1 << 26), 1 << 26) & MASK


def base(r, op, fail=None):
    peep = [r.getrandbits(32) for _ in range(PEEP_WORDS)]
    n_calls = 24
    R = [0] * n_calls if fail is None else [0] * fail + [1]
    return {"op": op, "peep": peep, "dest": [r.getrandbits(8) for _ in range(4)],
            "queues": {"R": R, "D": [r.choice([0, 1])], "N": [r.randrange(1, 60)], "px": [ri(r)], "py": [ri(r)]}}


def ser_case(r, save=None, version=None, fail=None):
    c = base(r, "ser", fail)
    if save is None:
        save = r.choice([0, 1, 1, 5])
    if version is None:
        version = r.choice([-1, 0, 1, 1, 2, 7, r.randrange(-5, 5)])
    c["args"] = [save, version & MASK, r.getrandbits(32)]
    return c


def ser_edges():
    r = random.Random(5331)
    for save in (0, 1):
        for version in (-1, 0, 1, 2):
            for fail in [None] + list(range(0, 23)):
                if version not in (0, 1) and fail not in (None, 0, 1, 2):
                    continue
                yield ser_case(r, save, version, fail)
    for d in (0, 1):
        for save in (0, 1):
            c = ser_case(r, save, 1)
            c["queues"]["D"] = [d]
            yield c
    for n in (1, 2, 3, 59):
        c = ser_case(r, 0, 0)
        c["queues"]["N"] = [n]
        c["peep"][8] = 0xFFFFFFFF
        yield c


def ser_random(r):
    return ser_case(r, fail=r.choice([None, None, r.randrange(0, 23)]))


SPECIAL = [0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 0, fb(1.0), fb(-1.0), fb(2.0), fb(0.0), fb(0.5), fb(-0.5), fb(1.5),
           fb(-1.0000001), fb(2.0000002), fb(1.0000001), 0x7F7FFFFF, 0xFF7FFFFF, 0x00000001]


def rt(r):
    k = r.random()
    if k < 0.2:
        return r.choice(SPECIAL)
    if k < 0.8:
        return fb(r.uniform(-1.5, 2.5))
    return fb(r.uniform(-300, 300))


def interp_case(r, t=None, ranged=None):
    c = base(r, "interp")
    c["args"] = [rt(r) if t is None else t, r.choice([0, 1, -1, 5, 0x100]) if ranged is None else ranged]
    return c


def interp_edges():
    r = random.Random(5332)
    for t in SPECIAL:
        for ranged in (0, 1, 2, 0x10000):
            c = interp_case(r, t, ranged)
            yield c
    for ranged in (0, 1):
        for _ in range(12):
            c = interp_case(r, None, ranged)
            c["queues"]["px"] = [0x7FFFFFFF]
            c["queues"]["py"] = [0x80000000]
            yield c


def interp_random(r):
    return interp_case(r)


def _args_fix():
    pass


TESTS = [
    FnTest("CPeep::GetInterpolatedPeepPosition", ppc, host, edge=interp_edges, rnd=interp_random),
    FnTest("CPeep::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
]
