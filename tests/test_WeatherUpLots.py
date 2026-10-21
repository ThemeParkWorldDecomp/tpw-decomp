"""Unit v5267, the keyboard cheat actions, against libtpw_unit_WeatherUpLots.dylib (tests/unit_WeatherUpLots.cpp), main app
("SimTheme Park"), src/game/keyboard.cpp:
  WeatherUpLots 0x1014d6bc, WeatherDown 0x1014d62c, WeatherDownLots 0x1014d74c, WeatherUp 0x1014d5a0, DeactivateWeather 0x1014d83c,
  ChatterToggle 0x1014d51c, RealTimeLightingMasterToggle 0x1014d8bc, start_cheating 0x1014d470

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory and host memory). The callees
(GetWeather, SetQuality, GetBankAccount, MakeDeposit, CCheat ctor, SetCheatStatus, SND_SetChatterOnOff, CTRACE) are fakes on both sides
that record their arguments.

The world: gGameWorld, the weather block (quality word at 0xc, byte at 0x64), the display flags, the chatter guard and value, the cheat
guard. Compared: the answer, the log, and all of those after the call. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_WeatherUpLots"
MASK = 0xFFFFFFFF

FNS = [
    ("WeatherUpLots", 0x1014D6BC, 108),
    ("WeatherDown", 0x1014D62C, 108),
    ("WeatherDownLots", 0x1014D74C, 108),
    ("WeatherUp", 0x1014D5A0, 108),
    ("DeactivateWeather", 0x1014D83C, 88),
    ("ChatterToggle", 0x1014D51C, 96),
    ("RealTimeLightingMasterToggle", 0x1014D8BC, 96),
    ("start_cheating", 0x1014D470, 132),
]
OWN = set(a for _, a, _ in FNS)

ADDR = dict(world_ptr=0x1037EBFC, display=0x103BC8D0, cheat=0x102DA934, cheat_guard=0x102DA940,
            chatter_guard=0x1037F240, chatter_on=0x1037F244)
HOST_LAYOUT = [0x0C, 0x64, 0, 12]

PP = {}   # persistent PowerPC blocks

STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(0x1013ED74, CL.recorder("GetWeather", "L", ret=lambda e, log: PP["weather"]))
STUBS.internal(0x1013EDFC, CL.recorder("GetBankAccount", "L", ret=lambda e, log: PP["bank"]))
STUBS.internal(0x10139000, CL.recorder("SetQuality", "Li"))
STUBS.internal(0x100F32EC, CL.recorder("MakeDeposit", "Lu"))
STUBS.internal(0x10146F24, CL.recorder("CCheat", "L"))
STUBS.internal(0x10146F5C, CL.recorder("SetCheatStatus", "Lb"))
STUBS.internal(0x100DEDA4, CL.recorder("SND_SetChatterOnOff", "i"))
STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "uss"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    for n in ("world", "weather", "bank"):
        PP[n] = T.persist_alloc(runner.emu, 0x100)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_byte(self, a):
        return self.read(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({PP["world"]: 1, PP["weather"]: 2, PP["bank"]: 3, ADDR["cheat"]: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "display":
            return self.e.u32(0x1026067C)      # the import's real address, patched into the TOC slot
        return ADDR[name]

    def setptr(self, a, p):
        self.words(a, [p])

    def blk(self, name):
        return PP[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FNS[op][1], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.B = {n: lib.tpw_wl_alloc(0x100) for n in ("world", "weather", "bank")}
        lib.tpw_wl_set(self.B["weather"], self.B["bank"])
        CL.host_begin(lib, {1: self.B["world"], 2: self.B["weather"], 3: self.B["bank"], 4: self.g("cheat")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_wl_addr(name.encode())

    def setptr(self, a, p):
        self.write(a, struct.pack("<Q", p))     # a native pointer is 8 bytes

    def blk(self, name):
        return self.B[name]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return self.lib.tpw_wl_call(op) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wl_addr.restype, lib.tpw_wl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wl_alloc.restype, lib.tpw_wl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wl_set.restype, lib.tpw_wl_set.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_wl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wl_call.restype, lib.tpw_wl_call.argtypes = C.c_int32, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.setptr(s.g("world_ptr"), s.blk("world"))
    w = s.blk("weather")
    s.words(w + 0x0C, [case["quality"]])
    s.words(w + 0x64, [case["wbyte"] << 24 if s.fmt == ">I" else case["wbyte"]])
    s.words(s.g("display"), [case["flags"]])
    s.write(s.g("chatter_guard"), bytes([case["cguard"]]))
    s.words(s.g("chatter_on"), [case["con"]])
    s.write(s.g("cheat_guard"), bytes([case["kguard"]]))


def observe(s, case, answer):
    w = s.blk("weather")
    return {"answer": answer, "log": s.log(),
            "quality": s.rd_word(w + 0x0C), "wbyte": s.rd_byte(w + 0x64),
            "flags": s.rd_word(s.g("display")),
            "cguard": s.rd_byte(s.g("chatter_guard")), "con": s.rd_word(s.g("chatter_on")),
            "kguard": s.rd_byte(s.g("cheat_guard"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


QUALS = [0, 1, 9, 10, 11, 99, 100, 101, 0x7FFFFFF6, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFF6, 0xFFFFFFF5]


def mk(r, op, quality=None, flags=None, cguard=None, con=None, kguard=None):
    return {"op": op, "queues": {},
            "quality": r.choice(QUALS + [r.getrandbits(32)]) if quality is None else quality,
            "wbyte": r.choice([0, 1, 0x80, r.randrange(256)]),
            "flags": r.getrandbits(32) if flags is None else flags,
            "cguard": r.choice([0, 1, 0x80, 0xFF, r.randrange(256)]) if cguard is None else cguard,
            "con": r.choice([0, 1, r.getrandbits(32)]) if con is None else con,
            "kguard": r.choice([0, 1, 0x80, 0xFF, r.randrange(256)]) if kguard is None else kguard}


def make_test(op, name):
    def edges():
        r = random.Random(100 + op)
        for q in QUALS:
            yield mk(r, op, quality=q)
        for fl in (0, 0x800, 0xFFFFFFFF, 0xFFFFF7FF):
            yield mk(r, op, flags=fl)
        for cg in (0, 1, 0x80, 0xFF):
            for con in (0, 1, 5):
                yield mk(r, op, cguard=cg, con=con)
        for kg in (0, 1, 0x80, 0xFF):
            yield mk(r, op, kguard=kg)

    def rnd(r):
        return mk(r, op)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(i, n) for i, (n, _, _) in enumerate(FNS)]
