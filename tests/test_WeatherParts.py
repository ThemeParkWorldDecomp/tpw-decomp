"""Unit w181, CWeather and CWeatherForecast, against libtpw_unit_WeatherParts.dylib (tests/unit_WeatherParts.cpp), main app ("SimTheme Park"),
src/Weather.cpp:
  CWeather: FinalDestroy 0x10137f3c, Serialise 0x10137f9c, ReceiveMessage 0x1013855c, FindLightningCoords 0x10138c10, SetQuality 0x10139000,
  SetWindDirection 0x101394d0, SetWindForce 0x101395a8, UpdateWindInfoToEngine 0x10139604;
  CWeatherForecast: Serialise 0x101396b8, Invalidate 0x10139854, Set 0x10139894;
  SAVEVARSTRUCT / LOADVARSTRUCT<D3DVECTOR_SWAPABLE> 0x10139a04 / 0x10139b90, LOADVAR / SAVEVAR<PrecipitationType> 0x10139ce8 / 0x10139e20.

A case is a weather object (0x68 bytes between two 0x40-byte guards of random bytes; every field a number, packed per side: the Mac's 64-bit stamps are
big-endian word pairs, the host's int64 little-endian), the globals (heightfield size, the seven balance values), a call and the queues. The serialise
instances over the basic types are REAL on both sides, so the file is the fake: LbFile_Read hands out the case's bytes in order, LbFile_Write records the
bytes as hex (the queues replace the result and the transferred count). Every other callee is a fake that records its arguments (CThing::Serialise and
FinalDestroy, CWorld::GetRand, CParkTime::GetFunnyTime, TbTimeDiff::Set, TbTimeStamp::operator+, CRain::Close / SetVelocity / SetWindParameters,
CWeather::SetPrecipitationType, CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG with the double its "%f" takes) and answers from its queue.
The message of ReceiveMessage is a fake object whose GetType answers the case's type. FindLightningCoords runs under the processor's fused
multiply-add (the emulator rounds fmadds twice).

What is compared: the answer, the whole call log, the object's fields (decoded per side), the guards, and the lightning vectors with their guards.
Not tested: a wind direction of infinity or a finite value so large that subtracting 360 changes nothing (the loops never end on either side).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_WeatherParts"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF

# (op index, name, address, size)
OPS = [
    (0, "CWeather::FinalDestroy", 0x10137F3C, 52), (1, "CWeather::Serialise", 0x10137F9C, 1424),
    (2, "CWeather::ReceiveMessage", 0x1013855C, 224), (3, "CWeather::FindLightningCoords", 0x10138C10, 528),
    (4, "CWeather::SetQuality", 0x10139000, 860), (5, "CWeather::SetWindDirection", 0x101394D0, 168),
    (6, "CWeather::SetWindForce", 0x101395A8, 48), (7, "CWeather::UpdateWindInfoToEngine", 0x10139604, 56),
    (8, "CWeatherForecast::Serialise", 0x101396B8, 356), (9, "CWeatherForecast::Invalidate", 0x10139854, 12),
    (10, "CWeatherForecast::Set", 0x10139894, 208), (11, "SAVEVARSTRUCT<D3DVECTOR_SWAPABLE>", 0x10139A04, 312),
    (12, "LOADVARSTRUCT<D3DVECTOR_SWAPABLE>", 0x10139B90, 260), (13, "LOADVAR<PrecipitationType>", 0x10139CE8, 236),
    (14, "SAVEVAR<PrecipitationType>", 0x10139E20, 260),
]
ADDR = {op: a for op, _, a, _ in OPS}
OWN = {a for _, _, a, _ in OPS}
# the basic instances the serialisers call (REAL on both sides): int, bool, long, float, TbTimeStamp (load and save of each)
REAL_EXTRA = {0x1000EF18, 0x1000F4B0, 0x1000F144, 0x1000F70C, 0x1000EE04, 0x1000F384, 0x100F16B4, 0x100F18D8, 0x100F1468, 0x100F1B1C}

ASSERT, WRITELOG = 0x1000DDE8, 0x1015B3DC
THING_SERIALISE, THING_FINAL, GETFUNNY, GETRAND, SETPTYPE = 0x1012E010, 0x1012E9F8, 0x101127D0, 0x1013B1E4, 0x10139388
G_SET, G_ADD, G_CLOSE, G_VELOCITY, G_WIND = 0x10239AC0, 0x10239958, 0x102399A0, 0x10239B08, 0x10239B20
G_WORLD, GSAVELOAD, G_RAIN = 0x1037EBFC, 0x1037FEFC, 0x103BCAA8
TOC_RAIN = 0x10261058
G_WIDTH, G_HEIGHT, BALANCE = 0x102C6C68, 0x102C6C64, 0x102B4568
BALANCE_OFFSETS = [0x4C4, 0x4C8, 0x4CC, 0x4D0, 0x4D4, 0x4D8, 0x4F0]   # snow low / high, rain low / high, lightning low / high, MaxRaindrops
FILE_HANDLE, WORLD, RAIN = 0x6E100000, 0x6E200000, 0x6E300000
L_OBJ, L_RAIN, L_FILE, L_WORLD, L_MSG, L_SAVELOAD, L_FORE, L_PARK = 1, 2, 3, 4, 5, 6, 7, 8
GUARD = 0x40
OBJ_BYTES = 0x68
HOST_LAYOUT = [0x68, 0x10, 0x20, 0x48, 0x58, 0x5C, 0x64, 12, 8, 12, 16]   # the host message has an 8-byte vtable pointer
FIELDS = [("base", "12s"), ("q", "I"), ("t10", "q"), ("t18", "q"), ("t20", "q"), ("fq", "I"), ("fv", "B"), ("fp", "3s"), ("cd", "I"), ("td", "I"),
          ("lt", "B"), ("lp", "3s"), ("lc", "I"), ("ln", "I"), ("tc", "I"), ("vx", "I"), ("vy", "I"), ("vz", "I"), ("rs", "I"), ("pt", "I"),
          ("wd", "I"), ("wf", "I"), ("ov", "B"), ("op", "3s")]
FMT = "".join(f for _, f in FIELDS)
assert struct.calcsize(">" + FMT) == OBJ_BYTES
VEC_FMT = "64s8I64s"
LIBM = C.CDLL(None)
LIBM.fmaf.restype, LIBM.fmaf.argtypes = C.c_float, [C.c_float, C.c_float, C.c_float]


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(REAL_EXTRA)
R = CL.recorder


def lb_read(e):
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = e.cb_file[e.cb_cur[0]:]
    taken = min(len(data), length)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = L.ret("LbFile_Read")
    if count == length and result >= 0:
        e.write(buf, (data[:taken] + bytes(length))[:length])
    e.cb_cur[0] += taken
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


def toc(e, *_):
    """What the glue's first instruction does (stw r2, 0x14(r1)): the caller reloads r2 from there afterwards."""
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))


def stub_assert(e):
    text = e.cstr(e.r(4))[:159]
    ints = [s32(e.r(3))]
    if "%f" in text:
        ints += [e.r(5) & MASK, e.r(6) & MASK]
    e.cl.rec("GAME_ASSERTMSG", ints, (text,))


def stub_funny(e):
    L, sp = e.cl, e.r(1)
    present = sum(1 for i in range(5, 11) if e.r(i)) + sum(1 for o in (0x38, 0x3C) if e.u32(sp + o))
    L.rec("CParkTime::GetFunnyTime", (L.lab(e.r(4)), present))
    e.write(e.r(3), (L.ret("GetFunnyTime") & M64).to_bytes(8, "big"))


def stub_set(e):
    args = [s32(e.r(i)) for i in range(5, 10)]
    toc(e)
    e.cl.rec("TbTimeDiff::Set", args)
    e.write(e.r(4), ((args[0] * 0x100000001) & M64).to_bytes(8, "big"))
    e.write(e.r(3), bytes(4))


def stub_add(e):
    toc(e)
    a, d = int.from_bytes(e.read(e.r(4), 8), "big"), int.from_bytes(e.read(e.r(5), 8), "big")
    e.cl.rec("TbTimeStamp::operator+", (s64(a), s64(d)))
    e.write(e.r(3), ((a + d) & M64).to_bytes(8, "big"))


def stub_close(e):
    toc(e)
    e.cl.rec("CRain::Close", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


STUBS.internal(ASSERT, stub_assert)
STUBS.internal(WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
STUBS.internal(THING_SERIALISE, R("CThing::Serialise", "LLiiu", "i"))
STUBS.internal(THING_FINAL, R("CThing::FinalDestroy", "L"))
STUBS.internal(GETFUNNY, stub_funny)
STUBS.internal(GETRAND, R("CWorld::GetRand", "L", "i"))
STUBS.internal(SETPTYPE, R("CWeather::SetPrecipitationType", "Li"))
STUBS.internal(G_SET, stub_set)
STUBS.internal(G_ADD, stub_add)
STUBS.internal(G_CLOSE, stub_close)
STUBS.internal(G_VELOCITY, R("CRain::SetVelocity", "Lf", then=toc))
STUBS.internal(G_WIND, R("CRain::SetWindParameters", "Lff", then=toc))
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("CMsg::GetType", [emu.cl.lab(emu.r(3))])
        emu.set_r(3, emu.cl.msg_type & MASK)

    tvec = T.fake_function(e, get_type)
    e.mc_vtable = T.persist_alloc(e, 0x20)
    e.write(e.mc_vtable + 8, struct.pack(">I", tvec))


class Fused:
    """The emulator computes fmadds in double and rounds to single; the processor rounds once. Inside [begin, begin + size) the result of each
    fmadds is replaced by the C library's fmaf (the operands are singles)."""

    def __init__(self, e, begin, size):
        self.e, self.begin, self.end, self.pending, self.hook = e, begin, begin + size, None, None

    def __enter__(self):
        self.hook = self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=self.begin, end=self.end - 1)

    def __exit__(self, *exc):
        self.e.uc.hook_del(self.hook)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            uc.reg_write(ppcemu.FPR[target], struct.unpack(">Q", struct.pack(">d", value))[0])
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) != 29:
            return
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = e.f(ra), e.f(rc), e.f(rb)
        if not all(x == x and abs(x) != float("inf") for x in (a, b, c)):
            return
        for x in (a, b, c):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        self.pending = (rd, float(LIBM.fmaf(a, c, b)))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    order = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({GSAVELOAD: L_SAVELOAD, FILE_HANDLE: L_FILE, WORLD: L_WORLD, WORLD + 0x2A0: L_PARK, RAIN: L_RAIN}, case["queues"])
        self.L.msg_type = s32(case["a"]) if case["op"] == 2 else 0
        e.cb_file, e.cb_cur = case["file"], [0]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None

    def build(self, raw, vec, c):
        e, L = self.e, self.L
        self.base = e.alloc(len(raw), raw)
        self.obj = self.base + GUARD
        self.vbase = e.alloc(len(vec), vec)
        L.labels[self.obj] = L_OBJ
        L.labels[self.obj + 0x20] = L_FORE
        self.msg = e.alloc(0x10, struct.pack(">IIII", e.mc_vtable, 0, c["b"] & MASK, c["c"] & MASK))
        L.labels[self.msg] = L_MSG
        e.write(G_WORLD, WORLD.to_bytes(4, "big"))
        e.write(TOC_RAIN, G_RAIN.to_bytes(4, "big"))   # the import slot of gei_Rain holds the variable's address (the file has the loader's placeholder)
        e.write(G_RAIN, RAIN.to_bytes(4, "big"))
        e.write(G_WIDTH, (c["gw"] & MASK).to_bytes(4, "big"))
        e.write(G_HEIGHT, (c["gh"] & MASK).to_bytes(4, "big"))
        for off, v in zip(BALANCE_OFFSETS, c["bal"]):
            e.write(BALANCE + off, (v & MASK).to_bytes(4, "big"))
        self.name = e.alloc(len(c["name"]) + 1, c["name"].encode() + b"\0")

    def read(self):
        return self.e.read(self.base, GUARD + OBJ_BYTES + GUARD), self.e.read(self.vbase, 160)

    def log(self):
        return self.L.log

    def call(self, c):
        e, op, a, b, k, o = self.e, c["op"], c["a"], c["b"], c["c"], self.obj
        w = {0: [o], 1: [o, FILE_HANDLE, a & MASK, b & MASK, k & MASK], 2: [o, self.msg],
             3: [o, self.vbase + GUARD, self.vbase + GUARD + 0x10, a & MASK], 4: [o, a & MASK],
             5: [o, T.F(T.bits_f32(a & MASK))], 6: [o, T.F(T.bits_f32(a & MASK))], 7: [o],
             8: [o + 0x20, FILE_HANDLE, a & MASK, b & MASK, k & MASK], 9: [o + 0x20], 10: [o + 0x20, a & MASK, b & MASK],
             11: [FILE_HANDLE, o + 0x48, self.name, a & MASK], 12: [FILE_HANDLE, o + 0x48, self.name, a & MASK],
             13: [FILE_HANDLE, o + 0x58, self.name, a & MASK], 14: [FILE_HANDLE, o + 0x58, self.name, a & MASK]}[op]
        if op == 3:
            with Fused(e, ADDR[3], 528):
                return T.call_ppc(e, ADDR[op], w, max_insns=50_000_000, timeout_us=300_000_000)[0] & MASK
        return T.call_ppc(e, ADDR[op], w, max_insns=50_000_000, timeout_us=300_000_000)[0] & MASK


class Host:
    order = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_wq_set_file(case["file"], len(case["file"]))
        self.fixed = None

    def done(self):
        pass

    def build(self, raw, vec, c):
        lib = self.lib
        self.base = lib.tpw_wq_addr(b"block")
        C.memmove(self.base, raw, len(raw))
        self.vbase = lib.tpw_wq_addr(b"vec")
        C.memmove(self.vbase, vec, len(vec))
        fixed = {L_OBJ: self.base + GUARD, L_FORE: self.base + GUARD + 0x20, L_RAIN: lib.tpw_wq_addr(b"rain"), L_FILE: lib.tpw_wq_addr(b"file"),
                 L_WORLD: lib.tpw_wq_addr(b"world"), L_PARK: lib.tpw_wq_addr(b"world") + 0x2A0, L_MSG: lib.tpw_wq_addr(b"msg"),
                 L_SAVELOAD: lib.tpw_wq_addr(b"gSaveLoad")}
        CL.host_begin(lib, fixed, c["queues"])
        bal = (C.c_int32 * 7)(*[s32(v) for v in c["bal"]])
        lib.tpw_wq_set_globals(s32(c["gw"]), s32(c["gh"]), bal)
        self.name = C.create_string_buffer(c["name"].encode())

    def read(self):
        return C.string_at(self.base, GUARD + OBJ_BYTES + GUARD), C.string_at(self.vbase, 160)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, c):
        return self.lib.tpw_wq_call(c["op"], c["a"], c["b"], c["c"], C.addressof(self.name), 0) & M64


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wq_addr.restype, lib.tpw_wq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wq_set_globals.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_wq_set_file.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_wq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wq_call.restype = C.c_int64
    lib.tpw_wq_call.argtypes = [C.c_uint32, C.c_int64, C.c_int64, C.c_int64, C.c_uint64, C.c_int64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- running a case

SER_OPS = {1, 8, 11, 12, 13, 14}


def run(s, c):
    try:
        f = c["fields"]
        raw = bytes(c["guard_a"]) + struct.pack(s.order + FMT, *[f[k] for k, _ in FIELDS]) + bytes(c["guard_b"])
        vec = bytes(c["vguard_a"]) + struct.pack(s.order + "8I", *c["vec"]) + bytes(c["vguard_b"])
        s.build(raw, vec, c)
        r = s.call(c)
        out = {"answer": s32(r) if c["op"] in SER_OPS else 0, "log": s.log()}
        data, vdata = s.read()
        out["guard_a"], out["guard_b"] = data[:GUARD], data[GUARD + OBJ_BYTES:]
        out["obj"] = struct.unpack(s.order + FMT, data[GUARD:GUARD + OBJ_BYTES])
        out["vec"] = struct.unpack(s.order + VEC_FMT, vdata)
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["mLastLightningLocation", "mPType", "x", "a long name 123"]
INT_EDGES = [0, 1, -1, 2, 3, 99, 100, 101, 0x7FFFFFFF, -0x80000000, 0x7FFFFFFE, -0x7FFFFFFF]


def fb(x):
    return T.f32_bits(x)


SAFE_WD = [fb(x) for x in (0.0, -0.0, 45.0, 44.9, 90.0, 100.5, 180.0, 359.9, 360.0, 361.0, 720.0, -1.0, -44.0, -45.0, -46.0, -360.0, -720.5, 1000.0,
                           -1000.0, 2999.75, -2999.5, 1e-30, -1e-30, 405.0)] + [0x7FC00000]
RANDV = [0, 1, 2, 3, 0x3FFFFFFF, 0x40000000, 0x80000000, 0xFFFFFFFF, 0xC0000000, 0x7FFFFFFF, 89, 90, 91, 0x20000000, 0x12345678, 0xFFFFFFFE]
STAMPS = [0, 1, -1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000, -0x8000000000000000, 0x7FFFFFFFFFFFFFFF, 0x123456789ABCDEF0]


def rfield(r, fmt):
    if fmt.endswith("s"):
        return r.randbytes(int(fmt[:-1]))
    bits = 8 * struct.calcsize(fmt)
    v = r.getrandbits(bits)
    return v - (1 << bits) if fmt.islower() and v >= 1 << (bits - 1) else v


def rf_bits(r):
    """Random float bits, no NaN with a payload (the emulator quiets a signalling NaN on the way through a register)."""
    while True:
        v = r.getrandbits(32)
        if (v >> 23) & 0xFF != 0xFF or v & 0x7FFFFF == 0:
            return v


def rstamp(r):
    return r.choice(STAMPS) if r.random() < 0.3 else s64(r.getrandbits(64))


def case(r, op, fields=None, a=0, b=0, c=0, queues=None, file=None, name=None, gw=None, gh=None, bal=None, safe=True, rands=None, nrand=8):
    f = {k: rfield(r, fm) for k, fm in FIELDS}
    if safe:
        f["wd"] = r.choice(SAFE_WD) if r.random() < 0.6 else fb(r.uniform(-3000, 3000))
    f.update(fields or {})
    q = {"GetFunnyTime": [rstamp(r) for _ in range(4)], "CWorld::GetRand": rands or [r.choice(RANDV + [r.getrandbits(32)]) for _ in range(nrand)],
         "CThing::Serialise": [0]}
    q.update(queues or {})
    if file is None:
        file = r.randbytes(r.choice([0, 3, 13, 91, 91, 91, 100, 120])) if r.random() < 0.85 else r.randbytes(r.randrange(0, 100))
    return {"op": op, "fields": f, "a": a, "b": b, "c": c, "queues": q, "file": file, "name": r.choice(NAMES) if name is None else name,
            "gw": r.choice([0, 1, 100, 256, 1000, r.randrange(0, 5000)]) if gw is None else gw,
            "gh": r.choice([0, 1, 100, 256, 1000, r.randrange(0, 5000)]) if gh is None else gh,
            "bal": random_balance(r) if bal is None else bal,
            "guard_a": r.randbytes(GUARD), "guard_b": r.randbytes(GUARD), "vguard_a": r.randbytes(GUARD), "vguard_b": r.randbytes(GUARD),
            "vec": [r.getrandbits(32) for _ in range(8)]}


def random_balance(r):
    def pair():
        k = r.random()
        if k < 0.6:
            lo = r.randrange(0, 101)
            return [lo, lo + r.randrange(0, 101)]
        if k < 0.8:
            return [r.choice(INT_EDGES), r.choice(INT_EDGES)]
        return [r.randrange(-0x80000000, 0x80000000), r.randrange(-0x80000000, 0x80000000)]
    maxd = r.choice([0, 1, 2, 3, 99, 100, 101, 1000, 5000, 0x7FFFFFFF, -1, -100, -0x80000000, r.randrange(-0x80000000, 0x80000000)])
    return pair() + pair() + pair() + [maxd]


def fail_queues(r, save, step, kind):
    key = "LbFile_Write" if save else "LbFile_Read"
    k = r.choice(["res", "short", "long"]) if kind is None else kind
    if k == "res":
        return {key: [0] * step + [r.choice([-1, 1, -0x80000000, 0x7FFFFFFF])]}
    if k == "short":
        return {key + ":xfer": [-1] * step + [r.choice([0, 1, 3])]}
    return {key + ":xfer": [-1] * step + [r.choice([5, 9, 0xFFFFFFFF])]}


VERSIONS = [-1, 0, 1, 2, 3, 100, -5, -0x80000000, 0x7FFFFFFF]


def ser_case(r, op, save=None, version=None, step=None, kind=None, base=None):
    save = r.choice([0, 1, 5, -1]) if save is None else save
    version = r.choice([0, 1, 1, 0, -1, 2, 3, 100, -5]) if version is None else version
    q = fail_queues(r, 1 if save else 0, step, kind) if step is not None else {}
    if base is not None:
        q["CThing::Serialise"] = [base]
    elif op == 1 and r.random() < 0.1:
        q["CThing::Serialise"] = [r.choice([1, -1, 5, -0x80000000])]
    return case(r, op, a=save, b=version, c=r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]), queues=q, safe=False,
                file=r.randbytes(r.choice([91, 91, 100, 13, 14])) if r.random() < 0.85 else None)


def ser_edges(op, steps):
    def gen():
        r = random.Random(1810 + op)
        for save in (0, 1):
            for version in VERSIONS:
                yield ser_case(r, op, save, version)
            for version in (0, 1):
                for step in range(steps):
                    for kind in ("res", "short", "long"):
                        yield ser_case(r, op, save, version, step, kind)
        for save in (5, -1, 0x80000000):
            yield ser_case(r, op, save, 1)
        if op == 1:
            for save in (0, 1):
                for version in (0, 1, 2, -1):
                    for base in (1, -1, 5, -0x80000000, 0x7FFFFFFF):
                        yield ser_case(r, op, save, version, None, None, base)
        for n in range(0, 100, 3):
            yield case(r, op, a=0, b=1, file=r.randbytes(n), safe=False)
    return gen


def ser_random(op, steps):
    return lambda r: ser_case(r, op, step=r.choice([None, None] + list(range(steps))))


def inst_case(r, op, step=None, kind=None, nolog=None):
    save = op in (11, 14)
    q = fail_queues(r, 1 if save else 0, 0, kind) if step is not None else {}
    n = r.choice([0, 1, 2, -1, 0x7FFFFFFF]) if nolog is None else nolog
    return case(r, op, a=n, queues=q, file=r.randbytes(r.choice([0, 3, 4, 11, 12, 13, 20])), safe=False)


def inst_edges(op):
    def gen():
        r = random.Random(1830 + op)
        for nolog in (0, 1, 2, -1):
            yield inst_case(r, op, None, None, nolog)
            for kind in ("res", "short", "long"):
                yield inst_case(r, op, 0, kind, nolog)
        for n in range(0, 16):
            yield case(r, op, a=0, file=r.randbytes(n), safe=False)
        for _ in range(20):
            yield inst_case(r, op)
    return gen


def inst_random(op):
    return lambda r: inst_case(r, op, step=r.choice([None, None, 0]))


def simple_case(op):
    return lambda r: case(r, op, safe=False)


def simple_edges(op):
    def gen():
        r = random.Random(1800 + op)
        for _ in range(6):
            yield case(r, op, safe=False)
    return gen


# ---- SetQuality and ReceiveMessage
QS = [-0x80000000, -5, -1, 0, 1, 2, 50, 99, 100, 101, 200, 0x7FFFFFFF]
KINDS = ["in", "low", "high", "below", "above", "point"]


def ranged(r, q, kind):
    if kind == "in":
        return [q - r.randrange(0, 5), q + r.randrange(0, 5)]
    if kind == "low":
        return [q, q + r.randrange(0, 5)]
    if kind == "high":
        return [q - r.randrange(0, 5), q]
    if kind == "below":
        return [q - 10, q - 1]
    if kind == "above":
        return [q + 1, q + 10]
    return [q, q]


def quality_case(r, q=None, kinds=None, rands=None, maxd=None, op=4, type_=None, locked=None):
    q = r.choice(QS + [r.randrange(-10, 120)]) if q is None else q
    qc = 1 if q < 1 else (100 if q > 99 else q)
    kinds = kinds or [r.choice(KINDS) for _ in range(3)]
    bal = ranged(r, qc, kinds[0]) + ranged(r, qc, kinds[1]) + ranged(r, qc, kinds[2]) + [r.choice([100, 1000, 5000, 0, 1, 7]) if maxd is None else maxd]
    if op == 4:
        return case(r, 4, a=q, bal=bal, rands=rands)
    f = {"ov": 0 if locked is None else locked}
    return case(r, 2, f, a=type_, b=q, c=r.choice([0, 1, -1, 3, 100, 0x7FFFFFFF, -0x80000000]), bal=bal, rands=rands)


def quality_edges():
    r = random.Random(1840)
    for q in (1, 2, 50, 99, 100):
        for k0 in KINDS:
            for k1 in KINDS:
                for bit in (0, 1):
                    yield quality_case(r, q, [k0, k1, r.choice(KINDS)], [bit + 2 * r.randrange(0, 40), r.choice(RANDV)])
    for q in QS:
        for k in KINDS:
            yield quality_case(r, q, [r.choice(KINDS), r.choice(KINDS), k])
    for maxd in (0, 1, 2, 99, 100, 101, 0x7FFFFFFF, -1, -0x80000000, 5000, 3):
        for q in (1, 50, 100):
            yield quality_case(r, q, ["in", "in", "in"], None, maxd)
            yield quality_case(r, q, ["in", "above", "in"], None, maxd)
            yield quality_case(r, q, ["above", "in", "in"], None, maxd)
    for rv in RANDV:
        yield quality_case(r, 40, ["above", "in", "in"], [rv, rv, rv])
        yield quality_case(r, 40, ["in", "in", "in"], [rv, rv, rv])
    for lo, hi in ((0, 0), (5, 5), (-0x80000000, 0x7FFFFFFF), (-0x80000000, 0), (0, 0x7FFFFFFF), (-5, 0x7FFFFFFF), (1, 0x7FFFFFFF), (-0x80000000, 100),
                   (-0x80000000, 1), (50, 50), (100, 100), (1, 1)):
        for q in (1, 50, 100):
            for slot in (0, 2):
                bal = [200, 300, 200, 300, 200, 300, r.choice([100, 5000, 0x7FFFFFFF])]
                bal[slot], bal[slot + 1] = lo, hi
                yield case(r, 4, a=q, bal=bal)
    for wd in SAFE_WD:
        yield case(r, 4, {"wd": wd}, a=r.randrange(1, 100))
    # the wind tail wraps: the direction minus 45 plus the random remainder, at the edges of the loops
    for wd in (0.0, 44.0, 45.0, 46.0, 135.0, 405.0, 450.0, 449.0, 360.0, 315.0):
        for rv in (0, 1, 89, 90, 91, 0xFFFFFFFF):
            yield case(r, 4, {"wd": fb(wd)}, a=50, rands=[0, rv, rv, rv])


def quality_random(r):
    return quality_case(r)


def message_edges():
    r = random.Random(1850)
    for type_ in (0, 1, 2, 3, -1, 0x7FFFFFFF, -0x80000000, 4, 15):
        for locked in (0, 1, 0xFF, 0x80, 0x7F):
            for q in (50, 0, 100, -1, 0x7FFFFFFF):
                yield quality_case(r, q, None, None, None, 2, type_, locked)
    for days in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 100, 36500):
        for q in (0, 50, -7, 0x7FFFFFFF):
            yield case(r, 2, a=2, b=q, c=days)


def message_random(r):
    return quality_case(r, None, None, None, None, 2, r.choice([0, 1, 1, 2, 2, 3, -1, r.getrandbits(32) - 0x80000000]), r.choice([0, 0, 0, 1, 0xFF, r.getrandbits(8)]))


# ---- FindLightningCoords
SIZES = [0, 1, 2, 3, 100, 256, 1000, 0x7FFFFFFF, -0x80000000, -1, 0x40000000, 0x1000000, 0x1000001, 0x1000003, 0xFFFFFF, 5000, 0x7FFFFF01]


def lightning_case(r, gw=None, gh=None, rands=None):
    gw = r.choice(SIZES + [r.randrange(0, 20000), r.getrandbits(32) - 0x80000000]) if gw is None else gw
    gh = r.choice(SIZES + [r.randrange(0, 20000), r.getrandbits(32) - 0x80000000]) if gh is None else gh
    return case(r, 3, a=r.choice([0, 1, 3, -1]), gw=gw, gh=gh, rands=rands or [r.choice(RANDV + [r.getrandbits(32)]) for _ in range(4)], safe=False)


def lightning_edges():
    r = random.Random(1860)
    for gw in SIZES:
        for gh in (0, 1, 100, 1000, -1, 0x7FFFFFFF, 0x1000001):
            yield lightning_case(r, gw, gh)
    for rv in RANDV:
        for gw in (1, 256, 0x1000001, 1000):
            yield lightning_case(r, gw, gw, [rv] * 4)
    for a in RANDV:
        for b in RANDV:
            yield lightning_case(r, 1000, 777, [a, b, b, a])
    for gw in (0, 1, 2, 3):
        for rv in (0, 1, 0x3FFFFFFF, 0xFFFFFFFF):
            yield lightning_case(r, gw, gw, [rv, rv, 0, 0x3FFFFFFF])


def lightning_random(r):
    return lightning_case(r)


# ---- the wind
def wind_floats(r):
    base = [0.0, -0.0, 1.0, -1.0, 45.0, 359.99997, 360.0, 360.00003, 361.0, 720.0, -360.0, -359.99997, -720.0, 1e-30, -1e-30, 1e-45, -1e-45, 1e6, -1e6,
            12345.5, -12345.5, 3.4e5, 100000.0, 0.5, 359.0, -0.5]
    return [fb(x) for x in base] + [0x7FC00000, 0x00000001, 0x80000001]


def wind_case(r, bits=None):
    if bits is None:
        bits = r.choice(wind_floats(r)) if r.random() < 0.4 else fb(r.uniform(-4000, 4000)) if r.random() < 0.7 else fb(r.uniform(-300000, 300000))
    return case(r, 5, a=bits, safe=False)


def wind_edges():
    r = random.Random(1870)
    for bits in wind_floats(r):
        yield wind_case(r, bits)
    for k in range(-12, 13):
        yield wind_case(r, fb(k * 360.0))
        yield wind_case(r, fb(k * 360.0 + 0.001))
        yield wind_case(r, fb(k * 360.0 - 0.001))


def force_floats():
    base = [0.0, -0.0, 1.0, 0.5, -0.5, 1.0000001, 0.99999994, -1e-45, 1e-45, 2.0, -2.0, 1e30, -1e30, 3.4e38, -3.4e38, 0.01]
    return [fb(x) for x in base] + [0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x80000000]


def force_case(r, bits=None):
    return case(r, 6, a=r.choice(force_floats()) if bits is None and r.random() < 0.5 else rf_bits(r) if bits is None else bits, safe=False)


def force_edges():
    r = random.Random(1880)
    for bits in force_floats():
        yield force_case(r, bits)


def update_case(r):
    return case(r, 7, {"wd": rf_bits(r), "wf": rf_bits(r)}, safe=False)


def update_edges():
    r = random.Random(1890)
    for wd in force_floats():
        yield case(r, 7, {"wd": wd, "wf": r.choice(force_floats())}, safe=False)


# ---- the forecast's Set
def set_case(r, days=None, q=None):
    days = r.choice(INT_EDGES + [r.randrange(-1000, 1000)]) if days is None else days
    return case(r, 10, a=days, b=r.choice(INT_EDGES + [r.randrange(-100, 200)]) if q is None else q, safe=False)


def set_edges():
    r = random.Random(1900)
    for d in INT_EDGES + [36500, -36500, 365]:
        yield set_case(r, d, 50)
    for t in STAMPS:
        yield case(r, 10, a=7, b=3, queues={"GetFunnyTime": [t]}, safe=False)


def set_random(r):
    return set_case(r)


TESTS = []
for _op, _name, _a, _s in OPS:
    if _op == 0:
        TESTS.append(FnTest(_name, ppc, host, edge=simple_edges(0), rnd=simple_case(0)))
    elif _op == 1:
        TESTS.append(FnTest(_name, ppc, host, edge=ser_edges(1, 19), rnd=ser_random(1, 19)))
    elif _op == 2:
        TESTS.append(FnTest(_name, ppc, host, edge=message_edges, rnd=message_random))
    elif _op == 3:
        TESTS.append(FnTest(_name, ppc, host, edge=lightning_edges, rnd=lightning_random))
    elif _op == 4:
        TESTS.append(FnTest(_name, ppc, host, edge=quality_edges, rnd=quality_random))
    elif _op == 5:
        TESTS.append(FnTest(_name, ppc, host, edge=wind_edges, rnd=wind_case))
    elif _op == 6:
        TESTS.append(FnTest(_name, ppc, host, edge=force_edges, rnd=force_case))
    elif _op == 7:
        TESTS.append(FnTest(_name, ppc, host, edge=update_edges, rnd=update_case))
    elif _op == 8:
        TESTS.append(FnTest(_name, ppc, host, edge=ser_edges(8, 4), rnd=ser_random(8, 4)))
    elif _op == 9:
        TESTS.append(FnTest(_name, ppc, host, edge=simple_edges(9), rnd=simple_case(9)))
    elif _op == 10:
        TESTS.append(FnTest(_name, ppc, host, edge=set_edges, rnd=set_random))
    else:
        TESTS.append(FnTest(_name, ppc, host, edge=inst_edges(_op), rnd=inst_random(_op)))
