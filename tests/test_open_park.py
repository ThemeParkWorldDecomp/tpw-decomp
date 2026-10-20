"""Unit v51108, the keyboard cheat actions, against libtpw_unit_open_park.dylib (tests/unit_open_park.cpp), main app
("SimTheme Park"), src/ui_Postcard_panel_spawn_handyman_WeatherUpLots_open_park.cpp:
  open_park, normal_speed_time, close_park, WeatherReset, FPSDisplayToggle, complete_all_research, uncomplete_all_research,
  create_arrival_vehicle, populate_park, start_bankruptcy_sequence, spawn_mechanic, spawn_researcher, spawn_guard, get_doshed_up,
  snapshot, continuous_snapshot, togglegamewindow, togglehud, mainmenu, Screen_BuyObject.

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, big-endian;
host library memory, little-endian). Callees outside the unit are recording fakes on both sides: the CWorld methods, the research lab,
the bank account, CVariableTime::SetTimeRate, the staff constructors, operator new (its answer can be NULL), the GAMESYSTEM calls and the
UIInterface methods. The fakes that answer an object give a dummy block with a label (3 research lab, 4 bank account), the world is
label 5; only the weather block is real memory.

The world of a case: gIntrface (cell halfword, show-fps word), the hud and movie toggle words, the weather block's byte 0x64 (Mac) and
the world pointer. What is compared: the answer, the log, the toggle words, the show-fps word and the weather byte.

No fused operations. Not tested: nothing is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_open_park"
MASK = 0xFFFFFFFF

FUNCS = {
    "open_park": 0x1014d3bc, "normal_speed_time": 0x1014d364, "close_park": 0x1014d414, "WeatherReset": 0x1014d7e0,
    "FPSDisplayToggle": 0x1014d950, "complete_all_research": 0x1014d994, "uncomplete_all_research": 0x1014d9f8,
    "create_arrival_vehicle": 0x1014da60, "populate_park": 0x1014dd7c, "start_bankruptcy_sequence": 0x1014de5c,
    "spawn_mechanic": 0x1014dfbc, "spawn_researcher": 0x1014e03c, "spawn_guard": 0x1014e0bc, "get_doshed_up": 0x1014e4b8,
    "snapshot": 0x1014e518, "continuous_snapshot": 0x1014e560, "togglegamewindow": 0x1014e5c8, "togglehud": 0x1014e61c,
    "mainmenu": 0x1014e6c8, "Screen_BuyObject": 0x1014e714,
}
SIZES = {
    "open_park": 56, "normal_speed_time": 48, "close_park": 56, "WeatherReset": 56, "FPSDisplayToggle": 28,
    "complete_all_research": 56, "uncomplete_all_research": 56, "create_arrival_vehicle": 56, "populate_park": 48,
    "start_bankruptcy_sequence": 48, "spawn_mechanic": 88, "spawn_researcher": 88, "spawn_guard": 88, "get_doshed_up": 60,
    "snapshot": 40, "continuous_snapshot": 60, "togglegamewindow": 44, "togglehud": 60, "mainmenu": 44, "Screen_BuyObject": 44,
}
OWN = set(FUNCS[n] + 4 * i for n in FUNCS for i in range(SIZES[n] // 4))
OWN = set(FUNCS.values())

ADDR = dict(intr=0x102dc1ac, clock=0x1037f048, ui=0x10396440, world=0x1037ebfc, hud=0x1037f238, snap=0x1037f23c)
MAC_OFFS = [0x24, 0x28, 0x64]
FAKE_LAB, FAKE_BANK, FAKE_WORLD = 0x70000010, 0x70000020, 0x70000030
WEATHER = [0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ret_const(v):
    return lambda e, log: v


STUBS.internal(0x1013fc3c, CL.recorder("SetParkOpenOrClosed", "Lii"))
STUBS.internal(0x1013ed74, CL.recorder("GetWeather", "L", ret=lambda e, log: WEATHER[0]))
STUBS.internal(0x1013ece8, CL.recorder("GetResearchLab", "L", ret=ret_const(FAKE_LAB)))
STUBS.internal(0x10123578, CL.recorder("CheatCompleteAll", "L"))
STUBS.internal(0x10123644, CL.recorder("CheatCompleteOnlyPaidFor", "L"))
STUBS.internal(0x10140234, CL.recorder("ArrivalVehicle_DoNextLeg", "xLi"))
STUBS.internal(0x1013b6ac, CL.recorder("PopulatePark", "L"))
STUBS.internal(0x1013baa4, CL.recorder("StartBankruptcySequence", "L"))
STUBS.internal(0x1013edfc, CL.recorder("GetBankAccount", "L", ret=ret_const(FAKE_BANK)))
STUBS.internal(0x100f32ec, CL.recorder("MakeDeposit", "Lu"))
STUBS.internal(0x1010409c, CL.recorder("CMechanic", "Nw"))
STUBS.internal(0x10120e34, CL.recorder("CResearcher", "Nw"))
STUBS.internal(0x100fda68, CL.recorder("CGuard", "Nw"))
STUBS.internal(0x10236624, CL.recorder("Screenshot", ""))
STUBS.internal(0x10236710, CL.recorder("ScreenshotMovie", "i"))
STUBS.internal(0x10236750, CL.recorder("SetHudState", "i"))
STUBS.internal(0x101ef9fc, CL.recorder("SwitchGameWindow", "L"))
STUBS.internal(0x101f03b4, CL.recorder("ShowGameMainMenu", "L"))
STUBS.internal(0x101f0410, CL.recorder("Screen_BuyObject", "L"))


def stub_time_rate(e):
    L = e.cl
    L.rec("SetTimeRate", (L.lab(e.r(3)), T.f64_bits(e.f(1))))


def stub_new(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("new", (size,))
    e.set_r(3, 0 if L.ret("newnull") else e.alloc(size))


STUBS.internal(0x101688d4, stub_time_rate)
STUBS.internal(0x102384b8, stub_new)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    offs = MAC_OFFS

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x1037f048: 1, 0x10396440: 2, FAKE_LAB: 3, FAKE_BANK: 4, FAKE_WORLD: 5}, case["queues"])
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

    def set_world(self):
        self.words(ADDR["world"], [FAKE_WORLD])

    def set_weather(self, a):
        WEATHER[0] = a

    def call(self, name):
        r3, _, _ = T.call_ppc(self.e, FUNCS[name], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.offs = lib.offs
        CL.host_begin(lib, {1: self.g("clock"), 2: self.g("ui"), 3: self.g("lab"), 4: self.g("bank"), 5: self.g("worldobj")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ka_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ka_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self):
        self.write(self.g("world"), struct.pack("<Q", self.g("worldobj")))

    def set_weather(self, a):
        self.lib.tpw_ka_set_weather(a)

    def call(self, name):
        return getattr(self.lib, "tpw_ka_" + name)() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ka_addr.restype, lib.tpw_ka_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ka_alloc.restype, lib.tpw_ka_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ka_set_weather.restype, lib.tpw_ka_set_weather.argtypes = None, [C.c_uint64]
    lib.tpw_ka_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in FUNCS:
        f = getattr(lib, "tpw_ka_" + n)
        f.restype, f.argtypes = C.c_uint32, []
    out = (C.c_int64 * 8)()
    n = lib.tpw_ka_layout(out)
    lib.offs = list(out)[:n]
    lib.bound = True


def build(s, case):
    s.set_world()
    intr = s.g("intr")
    s.write(intr + s.offs[0], struct.pack(s.fmt[0] + "H", case["cell"]))
    s.words(intr + s.offs[1], [case["fps"]])
    s.words(s.g("hud"), [case["hud"]])
    s.words(s.g("snap"), [case["snap"]])
    w = s.alloc(0x100)
    s.write(w + s.offs[2], bytes([case["wbyte"]]))
    s.set_weather(w)
    s.wblock = w


def observe(s, case, answer):
    intr = s.g("intr")
    return {"answer": answer, "log": s.log(), "fps": s.rd_word(intr + s.offs[1]), "hud": s.rd_word(s.g("hud")),
            "snap": s.rd_word(s.g("snap")), "cell": struct.unpack(s.fmt[0] + "H", s.read(intr + s.offs[0], 2))[0],
            "wbyte": s.read(s.wblock + s.offs[2], 1)}


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


WORDS = [0, 1, 2, MASK, 0x7FFFFFFF, 0x80000000]


def rw(r):
    return r.choice(WORDS) if r.random() < 0.5 else r.getrandbits(32)


def mk(r, op, newnull=0):
    return {"op": op, "queues": {"newnull": [newnull]}, "cell": r.choice([0, 1, 0xFFFF, r.getrandbits(16)]),
            "fps": rw(r), "hud": rw(r), "snap": rw(r), "wbyte": r.choice([0, 1, 0x64, r.getrandbits(8)])}


def make_test(op):
    def edges():
        r = random.Random(sum(map(ord, op)))
        for newnull in ((0, 1) if op.startswith("spawn_") else (0,)):
            for v in WORDS:
                c = mk(r, op, newnull)
                c["fps"] = c["hud"] = c["snap"] = v
                yield c

    def rnd(r):
        return mk(r, op, 1 if op.startswith("spawn_") and r.random() < 0.2 else 0)

    return FnTest(op, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(n) for n in FUNCS]
