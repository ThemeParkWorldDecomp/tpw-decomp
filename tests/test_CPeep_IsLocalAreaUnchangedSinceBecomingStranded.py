"""Unit v5048, three CPeep functions, against libtpw_unit_CPeep_IsLocalAreaUnchangedSinceBecomingStranded.dylib
(tests/unit_CPeep_IsLocalAreaUnchangedSinceBecomingStranded.cpp), main app ("SimTheme Park"),
src/game/people/Peep.cpp:
  CPeep::IsLocalAreaUnchangedSinceBecomingStranded 0x101156fc, CPeep::SpawnParticleEffect 0x101152a4, CPeep::ReportAllEvents 0x10115c3c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (IsQueue, GetQCellBehind, GetTimeStamp,
the height field starts, DetermineHFieldHeightQuick, SCR_GetScriptPosition, PAR_InitEmitter, CTRACE, GAME_ASSERTMSG, GetWorld, GetRecentEvent) are
fakes on both sides that record their arguments (texts as a hash) and answer from the case's queues or from a pure function of their arguments.

The world of a case: a peep block (cell bytes, script handle, stranded turn), gWorldMap and gWorld as dummy addresses, the nine neighbour steps, the
two halfword constants, the thing table rows and the event block. What is compared: the answer and the event log.

The spawn uses fmadds and fctiwz (HardwareRounding on the unit's functions). Not tested: a walk back along the queue that starts at the end cell
(the original then leaves a stack word unwritten), the first event of the report having no ride with a kind other than 3 (the original's registers
for the type are then unwritten), non-finite or huge positions (fctiwz saturates differently in Unicorn).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CPeep_IsLocalAreaUnchangedSinceBecomingStranded"
MASK = 0xFFFFFFFF

FN_LOCAL, FN_SPAWN, FN_REPORT = 0x101156FC, 0x101152A4, 0x10115C3C
SIZES = {FN_LOCAL: 388, FN_SPAWN: 300, FN_REPORT: 1340}
OWN = set(SIZES)

ISQUEUE, BEHIND, STAMP, CTRACE, GETRECENT = 0x1009C490, 0x10109AF0, 0x101026A0, 0x1006FCBC, 0x1012F4EC
HX, HY, GETWORLD, HEIGHT, ASSERT, EMITTER, SCRPOS = 0x1013E89C, 0x1013E8DC, 0x1014205C, 0x1005EDA8, 0x1000DDE8, 0x100BEC30, 0x100D93B8

ADDR = dict(gWorldMap=0x1037EC08, gWorld=0x1037EC0C, thingarr=0x1034CBFC, steps=0x1034C3AC, qend=0x102A0E96, none=0x102A0E98)
HOST_LAYOUT = [8, 4, 4]
WORLD, MAP = 0x30000000, 0x20000000


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def strhash(s):
    if isinstance(s, str):
        s = s.encode("latin1")
    h = 0
    for b in s:
        h = (h * 31 + b) & MASK
    return h & 0x7FFFFFFF


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_isqueue(e):
    L = e.cl
    L.rec("IsQueue", ((e.r(3) - e.u32(ADDR["gWorldMap"])) // 0x44,))
    e.set_r(3, L.ret("IsQueue") & MASK)


def stub_behind(e):
    L = e.cl
    cell = e.r(4) >> 16
    L.rec("GetQCellBehind", (cell,))
    e.write(e.r(3), struct.pack(">H", (cell - 1) & 0xFFFF))


def stub_stamp(e):
    L = e.cl
    idx = e.r(4) >> 16
    L.rec("GetTimeStamp", (e.r(3) - e.u32(ADDR["gWorld"]), idx))
    e.set_r(3, (e.seed + idx * 7) % 50)


def stub_hx(e):
    L = e.cl
    L.rec("HeightfieldXStart", (e.r(3) - e.u32(ADDR["gWorld"]),))
    e.set_r(3, L.ret("HX") & MASK)


def stub_hy(e):
    L = e.cl
    L.rec("HeightfieldYStart", (e.r(3) - e.u32(ADDR["gWorld"]),))
    e.set_r(3, L.ret("HY") & MASK)


def stub_height(e):
    L = e.cl
    L.rec("DetermineHFieldHeightQuick", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2)))))
    e.set_f1(bitsf(L.ret("HEIGHT")))


def stub_scrpos(e):
    L = e.cl
    L.rec("SCR_GetScriptPosition", (s32(e.r(3)), e.r(7)))
    e.write(e.r(4), struct.pack(">I", L.ret("SPx") & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("SPy") & MASK))
    e.write(e.r(6), struct.pack(">I", L.ret("SPz") & MASK))


def stub_emitter(e):
    e.cl.rec("PAR_InitEmitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_ctrace(e):
    fmt = e.cstr(e.r(4))
    if isinstance(fmt, str):
        fmt = fmt.encode("latin1")
    n = 0
    i = 0
    while i < len(fmt):
        if fmt[i] == 0x25:
            if i + 1 < len(fmt) and fmt[i + 1] == 0x25:
                i += 1
            else:
                n += 1
        i += 1
    args = [s32(e.r(5 + k)) for k in range(min(n, 3))]
    e.cl.rec("CTRACE", tuple([e.r(3) & MASK, strhash(fmt), n] + args + [0] * (min(n, 3) - len(args))))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (1 if (e.r(3) & 0xFF) else 0, strhash(e.cstr(e.r(4)))))


def stub_getworld(e):
    e.cl.rec("GetWorld", (1 if e.r(3) else 0,))
    e.set_r(3, WORLD)


def stub_recent(e):
    e.cl.rec("GetRecentEvent", (1 if e.r(3) else 0, e.r(4)))
    e.set_r(3, (e.evbase + 4 * e.r(4)) & MASK)


STUBS.internal(ISQUEUE, stub_isqueue)
STUBS.internal(BEHIND, stub_behind)
STUBS.internal(STAMP, stub_stamp)
STUBS.internal(HX, stub_hx)
STUBS.internal(HY, stub_hy)
STUBS.internal(HEIGHT, stub_height)
STUBS.internal(SCRPOS, stub_scrpos)
STUBS.internal(EMITTER, stub_emitter)
STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(ASSERT, stub_assert)
STUBS.internal(GETWORLD, stub_getworld)
STUBS.internal(GETRECENT, stub_recent)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        e.seed = case["seed"]
        e.evbase = 0
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

    def set_thing(self, idx, addr):
        self.words(ADDR["thingarr"] + idx * 0x14, [addr])

    def set_evbase(self, addr):
        self.e.evbase = addr

    def call(self, fn, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, fn, args)
        return (r3 & MASK) if fn == FN_LOCAL else 0     # the other two answer void


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5048_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v5048_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_thing(self, idx, addr):
        self.lib.tpw_v5048_setthing(idx, addr)

    def set_evbase(self, addr):
        self.write(self.g("evbase"), struct.pack("<Q", addr))

    def call(self, fn, args):
        if fn == FN_LOCAL:
            return self.lib.tpw_v5048_islocal(args[0]) & MASK
        if fn == FN_SPAWN:
            self.lib.tpw_v5048_spawn(args[0], s32(args[1]))
            return 0
        self.lib.tpw_v5048_report(args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5048_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setthing").restype, getattr(lib, p + "setthing").argtypes = None, [C.c_uint32, C.c_uint64]
    getattr(lib, p + "islocal").restype, getattr(lib, p + "islocal").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "spawn").restype, getattr(lib, p + "spawn").argtypes = None, [C.c_uint64, C.c_int32]
    getattr(lib, p + "report").restype, getattr(lib, p + "report").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.h16(s.g("qend"), case["qend"])
    s.h16(s.g("none"), case["none"])
    if isinstance(s, Host):
        s.words(s.g("seed"), [case["seed"]])
    s.words(s.g("gWorld"), [WORLD])
    s.words(s.g("gWorldMap"), [MAP])
    s.words(s.g("steps"), case["steps"])
    peep = s.alloc(0x1B0)
    s.write(peep + 4, bytes([case["cellx"]]))
    s.write(peep + 6, bytes([case["celly"]]))
    s.words(peep + 0xC, [case["handle"]])
    s.words(peep + 0x198, [case["stranded"]])
    case["peep"] = peep
    for tid in range(8):
        s.set_thing(tid, 0)      # the host's table keeps the last case's rows
    for tid, t in case["things"].items():
        if t is None:
            continue
        kind, x, y, typ = t
        a = s.alloc(0x20)
        s.write(a + 2, bytes([kind]))
        s.write(a + 4, bytes([x]))
        s.write(a + 6, bytes([y]))
        s.h16(a + 0xE, typ)
        s.set_thing(tid, a)
    if case["thing0"]:
        s.set_thing(0, s.alloc(0x20))
    ev = s.alloc(4 * 32)
    for i, (typ, tid) in enumerate(case["events"]):
        s.write(ev + 4 * i, struct.pack(s.fmt[0] + "HH", typ, tid))
    s.set_evbase(ev)


def execute(s, case):
    return s.call(case["fn"], [case["peep"], case["effect"] & MASK])


def run(s, case):
    try:
        build(s, case)
        return {"answer": execute(s, case), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, fn):
    return {"fn": fn, "queues": {}, "seed": r.randrange(0, 50), "qend": 0, "none": 0, "steps": [0] * 18, "cellx": 0, "celly": 0,
            "handle": 0, "stranded": 0, "things": {}, "thing0": False, "events": [(0, 0)] * 32, "effect": 0}


STD_STEPS = [-1, -1, 0, -1, 1, -1, -1, 0, 0, 0, 1, 0, -1, 1, 0, 1, 1, 1]


def local_case(r, q=None, stranded=None, seed=None):
    c = base(r, FN_LOCAL)
    q = r.choice([0, 1]) if q is None else q
    c["queues"] = {"IsQueue": [q]}
    if seed is not None:
        c["seed"] = seed
    if q and r.random() < 0.5:
        c["cellx"], c["celly"] = r.randrange(0, 6), 0
    else:
        c["cellx"], c["celly"] = r.randrange(0, 256), r.randrange(0, 256)
    cell = c["cellx"] + (c["celly"] << 7) + 1
    c["qend"] = 0 if cell <= 6 else max(0, cell - r.randrange(1, 6))
    if q and cell > 6:
        c["qend"] = cell - r.randrange(1, 6)
    if r.random() < 0.7:
        c["steps"] = [(v * (128 if i % 2 else 1)) if False else v for i, v in enumerate(STD_STEPS)]
        c["steps"] = [v if i % 2 == 0 else v for i, v in enumerate(STD_STEPS)]
    else:
        c["steps"] = [r.choice([0, 1, -1, 2, -2, 0x7FFFFFFF, -0x80000000, r.randrange(0, 1 << 32)]) & MASK for _ in range(18)]
    c["stranded"] = r.choice([0, 1, 5, 25, 48, 49, 50, 59, MASK]) if stranded is None else stranded
    if r.random() < 0.5:
        c["stranded"] = r.randrange(0, 60)
    return c


def local_edges():
    r = random.Random(5048)
    for q in (0, 1):
        for stranded in (0, 1, 2, 10, 25, 48, 49, 50, MASK):
            for seed in (0, 7, 33):
                yield local_case(r, q, stranded, seed)


def local_random(r):
    return local_case(r)


def finite(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, fb(1.0), fb(-1.0), fb(1e-30), fb(0.25)])
    if k < 0.7:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-6, 3))


def spawn_case(r, hx=None, hy=None):
    c = base(r, FN_SPAWN)
    c["handle"] = r.choice([0, 1, -1, r.randrange(0, 1 << 32)]) & MASK
    c["effect"] = r.choice([0, 0x13, -1, r.randrange(0, 1 << 32)]) & MASK
    hx = r.choice([0, 1, -1, 100, -100, r.randrange(-150000, 150000)]) if hx is None else hx
    hy = r.choice([0, 1, -1, 100, -100, r.randrange(-150000, 150000)]) if hy is None else hy
    c["queues"] = {"SPx": [finite(r)], "SPy": [finite(r)], "SPz": [finite(r)], "HX": [hx], "HY": [hy], "HEIGHT": [finite(r)]}
    return c


def spawn_edges():
    r = random.Random(5049)
    for hx, hy in ((0, 0), (1, 1), (-1, -1), (150000, -150000), (-150000, 150000), (7, 3)):
        yield spawn_case(r, hx, hy)


def spawn_random(r):
    return spawn_case(r)


def report_case(r, types=None):
    c = base(r, FN_REPORT)
    c["none"] = r.choice([0, 0, 0, 3, 5])
    c["thing0"] = r.random() < 0.15
    things = {}
    for tid in range(1, 8):
        k = r.random()
        if k < 0.2:
            things[tid] = None
        else:
            things[tid] = (r.choice([3, 3, 1, 2, 4]), r.randrange(0, 256), r.randrange(0, 256), r.randrange(0, 0x10000))
    things[7] = None if r.random() < 0.4 else (3, r.randrange(0, 256), r.randrange(0, 256), r.randrange(0, 0x10000))
    c["things"] = things
    ids = [i for i in range(0, 7) if i != c["none"]] + [c["none"]]
    events = []
    for i in range(32):
        typ = r.randrange(0, 28) if types is None else types[i % len(types)]
        events.append((typ, r.choice(ids)))
    events[31] = (events[31][0], 7)
    c["events"] = events
    return c


def report_edges():
    r = random.Random(5050)
    for t in range(28):
        yield report_case(r, [t])
    for t in range(0, 28, 3):
        yield report_case(r, [t, 2, 25, 0, 17, 14])


def report_random(r):
    return report_case(r)


TESTS = [
    FnTest("CPeep::IsLocalAreaUnchangedSinceBecomingStranded", ppc, host, edge=local_edges, rnd=local_random),
    FnTest("CPeep::SpawnParticleEffect", ppc, host, edge=spawn_edges, rnd=spawn_random),
    FnTest("CPeep::ReportAllEvents", ppc, host, edge=report_edges, rnd=report_random),
]
