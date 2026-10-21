"""Unit v51077, the ride animation leaves, the fly accessors, the ride pointers, Debug.cpp leaves, the keyboard actions and the static
initialisers, against libtpw_unit_UpdateRideAnimSpeedAllChannels.dylib (tests/unit_UpdateRideAnimSpeedAllChannels.cpp), main app
("SimTheme Park"), src/advisor_leaves_UpdateRideAnimSpeedAllChannels.cpp:
  UpdateRideAnimSpeedAllChannels 0x100c7ae4, UpdateRideAnimSpeedImmediate 0x100c7b58, GetCurrentAnim 0x100c7a00, FlushDeferredAnim 0x100c6604,
  GetAnimRemainingMilliseconds 0x100c5db8, GetRideAnimTime 0x100c3268, SetRideAnimTime 0x100c3234, __sinit_lobby_screen_cpp 0x100b7c9c,
  __sinit_lobby_island_cpp 0x100b54f8, IslandOnlineParkInfoPanelCancelCallback 0x100b43bc, __sinit_lobby_globe_cpp 0x100afe1c,
  __sinit_BlockSave_cpp 0x100f4754, __sinit_GoldenTickets_cpp 0x100fda00, FLY_SetRunway 0x100790c0, FLY_GetCarInBase 0x10078f54,
  __sinit_Debug_cpp 0x1006fd80, DEBUG_MESSAGE 0x1006fc90,
  __sinit_TPRide_cpp 0x1006c134, abort_coaster_edit 0x1014cdd4,
  slow_down_time 0x1014d2c0, speed_up_time 0x1014d314

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the string constructors,
__register_global_object, ShowCityLobbyController, UIInterface::EndCoasterEdit, CVariableTime::Increase/DecreaseTimeRate) are fakes on both
sides that record their arguments (the strings as length and hash).

The world of a case: a ride set with time controls (random words), the fly base table, the highlighted ride pointers, the clock words.
What is compared: the answer (GetCurrentAnim's is the flags word, which ProcessInstruction's ops 19 and 20 read), the control words, the
out words, the globals and the event log. No fused operations. Not tested: the answers of FlushDeferredAnim (1 or 0 in r3) and FLY_SetRunway
(garbage in r3), whose headers declare void or an unread answer; a control index past the count (reads past the block).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UpdateRideAnimSpeedAllChannels"
MASK = 0xFFFFFFFF

# (name, address, size)
FUNCS = [
    ("UpdateRideAnimSpeedAllChannels", 0x100c7ae4, 44), ("UpdateRideAnimSpeedImmediate", 0x100c7b58, 20),
    ("GetCurrentAnim", 0x100c7a00, 52), ("FlushDeferredAnim", 0x100c6604, 48), ("GetAnimRemainingMilliseconds", 0x100c5db8, 24),
    ("GetRideAnimTime", 0x100c3268, 28), ("SetRideAnimTime", 0x100c3234, 12), ("__sinit_lobby_screen_cpp", 0x100b7c9c, 60),
    ("__sinit_lobby_island_cpp", 0x100b54f8, 148), ("IslandOnlineParkInfoPanelCancelCallback", 0x100b43bc, 36),
    ("__sinit_lobby_globe_cpp", 0x100afe1c, 64), ("__sinit_BlockSave_cpp", 0x100f4754, 4), ("__sinit_GoldenTickets_cpp", 0x100fda00, 60),
    ("FLY_SetRunway", 0x100790c0, 44), ("FLY_GetCarInBase", 0x10078f54, 52), ("__sinit_Debug_cpp", 0x1006fd80, 28),
    ("DEBUG_MESSAGE", 0x1006fc90, 4), ("__sinit_TPRide_cpp", 0x1006c134, 60),
    ("abort_coaster_edit", 0x1014cdd4, 44), ("slow_down_time", 0x1014d2c0, 44),
    ("speed_up_time", 0x1014d314, 44),
]
ENTRY = {n: a for n, a, _ in FUNCS}
OWN = set(a for _, a, _ in FUNCS)

CTOR_W, CTOR_N, REGISTER = 0x10238800, 0x102382d8, 0x10239940
SHOW_LOBBY, END_COASTER, DECREASE, INCREASE = 0x101e0858, 0x101f0ed4, 0x10168988, 0x10168910
DESTROY_W, DESTROY_N = 0x103bcbb8, 0x103bcd38

ADDR = dict(clock=0x1037f048, ui=0x10396440, tp=0x103b74c8, fly=0x102da964,
            dbgA=0x102da944, dbgB=0x102da948, iw1=0x102e4938, in1=0x102e494c, iw2=0x102e4918, in2=0x102e492c, ino=0x102e48f8,
            in3=0x102e490c, sw=0x102e49bc, sn=0x102e49d0, gw=0x102e48d8, gn=0x102e48ec, tw=0x1034c1e4, tn=0x1034c1f8,
            rw=0x102d1edc, rn=0x102d1ef0)
LABELS = {name: i + 1 for i, name in enumerate(sorted(ADDR))}
SINIT = {"__sinit_lobby_screen_cpp": 0, "__sinit_lobby_island_cpp": 1, "__sinit_lobby_globe_cpp": 2, "__sinit_GoldenTickets_cpp": 3,
         "__sinit_TPRide_cpp": 4, "__sinit_BlockSave_cpp": 5, "__sinit_Debug_cpp": 6}
# host layout (after the two host-only offsets): sizeof(sAnimTimeControl) and the offsets the Mac's words sit at
HOST_LAYOUT_TAIL = [0x38, 0x04, 0x08, 0x0C, 0x1C, 0x20, 0x24, 0x30]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def hash_wide(e, a, wide):
    n, h = 0, 2166136261
    while n < 64:
        c = struct.unpack(">H" if wide else ">B", e.read(a + (2 * n if wide else n), 2 if wide else 1))[0]
        if c == 0:
            break
        h = ((h ^ c) * 16777619) & MASK
        n += 1
    return n, h


def save_toc(e):
    """The import glue saves the caller's TOC pointer at 0x14(r1); the code reloads it after the call."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def stub_ctor_w(e):
    save_toc(e)
    L = e.cl
    n, h = hash_wide(e, e.r(4), True)
    L.rec("ctorW", (L.lab(e.r(3)), n, h))


def stub_ctor_n(e):
    save_toc(e)
    L = e.cl
    n, h = hash_wide(e, e.r(4), False)
    L.rec("ctorN", (L.lab(e.r(3)), n, h))


def stub_register(e):
    save_toc(e)
    L = e.cl
    which = {DESTROY_W: 1, DESTROY_N: 2}.get(e.r(4), 0)
    L.rec("register", (L.lab(e.r(3)), which, L.lab(e.r(5))))


def stub_lobby(e):
    e.cl.rec("ShowCityLobbyController", ())


def stub_this(name):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return stub


STUBS.internal(CTOR_W, stub_ctor_w)
STUBS.internal(CTOR_N, stub_ctor_n)
STUBS.internal(REGISTER, stub_register)
STUBS.internal(SHOW_LOBBY, stub_lobby)
STUBS.internal(END_COASTER, stub_this("EndCoasterEdit"))
STUBS.internal(DECREASE, stub_this("DecreaseTimeRate"))
STUBS.internal(INCREASE, stub_this("IncreaseTimeRate"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR[n]: LABELS[n] for n in ADDR}, case["queues"])
        self.saved = STUBS.activate(e)
        self.x = {}

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

    # pointers are 4 bytes here, 8 on the host
    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_word(a)

    def set_fields(self, st, count, ctl):
        self.write(st + 0x0E, struct.pack(">H", count))
        self.put_ptr(st + 0x10, ctl)

    def call(self, name, args):
        r3, _, f1 = T.call_ppc(self.e, ENTRY[name], args)
        return r3 & MASK, f1

    @staticmethod
    def fl(f):
        return T.F(bitsf(f))


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {LABELS[n]: self.g(n) for n in ADDR}, case["queues"])
        self.x = {}

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ra_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ra_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def set_fields(self, st, count, ctl):
        self.write(st + self.lib.layout[0], struct.pack("<H", count))
        self.put_ptr(st + self.lib.layout[1], ctl)

    def call(self, name, args):
        v = getattr(self.lib, "tpw_ra_" + HOSTFN[name])(*args)
        return v, v

    @staticmethod
    def fl(f):
        return bitsf(f)


HOSTFN = {"UpdateRideAnimSpeedAllChannels": "all", "UpdateRideAnimSpeedImmediate": "imm", "GetCurrentAnim": "cur",
          "FlushDeferredAnim": "flush", "GetAnimRemainingMilliseconds": "remain", "GetRideAnimTime": "gettime",
          "SetRideAnimTime": "settime", "IslandOnlineParkInfoPanelCancelCallback": "cb", "FLY_SetRunway": "flyset",
          "FLY_GetCarInBase": "flyget", "DEBUG_MESSAGE": "dbg",
          "abort_coaster_edit": "abort", "slow_down_time": "slow", "speed_up_time": "speed"}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32, f32 = C.c_uint64, C.c_uint32, C.c_int32, C.c_float
    lib.tpw_ra_addr.restype, lib.tpw_ra_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_ra_alloc.restype, lib.tpw_ra_alloc.argtypes = u64, [u32]
    lib.tpw_ra_layout.argtypes = [C.POINTER(C.c_int64)]
    sigs = {"all": (None, [u64, f32]), "imm": (None, [u64, f32, i32]), "cur": (u32, [u64, u64, u64, u32]),
            "flush": (None, [u64, u32]), "remain": (f32, [u64]), "gettime": (u32, [i32]), "settime": (None, [u32]),
            "sinit": (None, [i32]), "cb": (None, []), "flyset": (None, [i32, i32]), "flyget": (i32, [i32]),
            "dbg": (None, []), "abort": (i32, []), "slow": (i32, []), "speed": (i32, [])}
    for k, (res, args) in sigs.items():
        f = getattr(lib, "tpw_ra_" + k)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_ra_layout(out)
    lay = list(out)[:n]
    assert lay[2:] == HOST_LAYOUT_TAIL, lay
    lib.layout = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

NCTL = 14   # words in a time control


def build_set(s, c):
    count = c["count"]
    ctl = s.alloc(0x38 * max(count, 1) + 8)
    s.words(ctl, c["words"])
    st = s.alloc(0x200)
    s.set_fields(st, count, ctl)
    s.x.update(set=st, ctl=ctl, count=count)


def ctl_words(s):
    return s.rd_words(s.x["ctl"], NCTL * max(s.x["count"], 1))


def build_flies(s, c):
    bases = [s.alloc(0x60) for _ in range(3)]
    for b, w in zip(bases, c["bases"]):
        s.words(b, w)
    tab = s.g("fly")
    for i, k in enumerate(c["table"]):
        s.put_ptr(tab + i * (4 if s.fmt == ">I" else 8), bases[k - 1] if k else 0)
    s.x["bases"] = bases


def op_all(s, c):
    build_set(s, c)
    s.call(c["fn"], [s.x["set"], s.fl(c["f"])])
    return 0, {"ctl": ctl_words(s)}


def op_imm(s, c):
    build_set(s, c)
    s.call(c["fn"], [s.x["set"], s.fl(c["f"]), c["idx"]])
    return 0, {"ctl": ctl_words(s)}


def op_cur(s, c):
    build_set(s, c)
    outs = []
    for k in ("kp", "ip"):
        a = s.alloc(8) if c[k] else 0
        if a:
            s.words(a, [0xA5A5A5A5, 0x5A5A5A5A])
        outs.append(a)
    r, _ = s.call(c["fn"], [s.x["set"], outs[0], outs[1], c["idx"]])
    return r, {"ctl": ctl_words(s), "kout": s.rd_words(outs[0], 2) if outs[0] else None, "iout": s.rd_words(outs[1], 2) if outs[1] else None}


def op_flush(s, c):
    build_set(s, c)
    s.call(c["fn"], [s.x["set"], c["idx"]])
    return 0, {"ctl": ctl_words(s)}


def op_remain(s, c):
    c = dict(c, count=1)
    build_set(s, c)
    _, f1 = s.call(c["fn"], [s.x["ctl"]])
    if isinstance(f1, float):
        bits = fb(f1)
    else:
        bits = f1
    return canon(bits), {"ctl": ctl_words(s)}


def canon(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def op_time(s, c):
    tp = s.g("tp")
    s.words(tp + 0x4010, [c["a"]])
    s.words(tp + 0x4018, [c["b"]])
    if c["fn"] == "GetRideAnimTime":
        r, _ = s.call(c["fn"], [c["arg"] & MASK] if s.fmt == ">I" else [s32(c["arg"])])
        return r, {}
    s.call(c["fn"], [c["arg"] & MASK])
    return 0, {"a": s.rd_word(tp + 0x4010), "b": s.rd_word(tp + 0x4018)}


def op_sinit(s, c):
    s.words(s.g("dbgA"), [c.get("a", 0)])
    s.words(s.g("dbgB"), [c.get("b", 0)])
    if s.fmt == ">I":
        T.call_ppc(s.e, ENTRY[c["fn"]], [])
    else:
        s.lib.tpw_ra_sinit(SINIT[c["fn"]])
    return 0, {"dbg": s.rd_words(s.g("dbgA"), 2)}


def op_plain(s, c):
    r, _ = s.call(c["fn"], [])
    return s32(r) if c["fn"] in ("abort_coaster_edit", "slow_down_time", "speed_up_time") else 0, {}


def op_flyset(s, c):
    build_flies(s, c)
    s.call(c["fn"], [c["fly"] & MASK, c["a"] & MASK] if s.fmt == ">I" else [s32(c["fly"]), s32(c["a"])])
    return 0, {"bases": [s.rd_words(b, 24) for b in s.x["bases"]]}


def op_flyget(s, c):
    build_flies(s, c)
    r, _ = s.call(c["fn"], [c["fly"] & MASK] if s.fmt == ">I" else [s32(c["fly"])])
    return s32(r), {"bases": [s.rd_words(b, 24) for b in s.x["bases"]]}


OPS = {"UpdateRideAnimSpeedAllChannels": op_all, "UpdateRideAnimSpeedImmediate": op_imm, "GetCurrentAnim": op_cur,
       "FlushDeferredAnim": op_flush, "GetAnimRemainingMilliseconds": op_remain, "GetRideAnimTime": op_time, "SetRideAnimTime": op_time,
       "IslandOnlineParkInfoPanelCancelCallback": op_plain, "FLY_SetRunway": op_flyset, "FLY_GetCarInBase": op_flyget,
       "DEBUG_MESSAGE": op_plain, "abort_coaster_edit": op_plain, "slow_down_time": op_plain,
       "speed_up_time": op_plain}
for _n in SINIT:
    OPS[_n] = op_sinit


def run(s, case):
    try:
        ans, extra = OPS[case["fn"]](s, case)
        d = {"answer": ans, "log": s.log()}
        d.update(extra)
        return d
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

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


def rw(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def base(r, fn):
    return {"fn": fn, "queues": {}}


def set_case(r, fn, count=None, **kw):
    c = base(r, fn)
    n = r.choice([0, 1, 2, 3, 5]) if count is None else count
    c["count"] = n
    c["words"] = rw(r, NCTL * max(n, 1))
    c["f"] = rf(r)
    c["idx"] = r.randrange(max(n, 1))
    c["kp"], c["ip"] = r.choice([0, 1]), r.choice([0, 1])
    if fn == "FlushDeferredAnim" or True:
        for i in range(max(n, 1)):
            if r.random() < 0.5:
                c["words"][NCTL * i + 9] = 0xC
    c.update(kw)
    return c


def edges_set(fn):
    def gen():
        r = random.Random(11)
        for n in (0, 1, 2, 4):
            for f in (0, 0x80000000, 0x3F800000, 0x7FC00000, 0xFF800000):
                yield set_case(r, fn, n, f=f)
        for kp in (0, 1):
            for ip in (0, 1):
                yield set_case(r, fn, 3, kp=kp, ip=ip, idx=2)
        for v in (0xC, 0xB, 0xD, 0, MASK):
            c = set_case(r, fn, 2, idx=1)
            c["words"][NCTL + 9] = v
            yield c
    return gen


def remain_case(r, fn):
    c = set_case(r, fn, 1)
    c["words"][7], c["words"][8] = rf(r), rf(r)
    return c


def edges_remain():
    r = random.Random(12)
    for a, b in ((0, 0), (0x3F800000, 0), (0, 0x3F800000), (0x7F800000, 0x7F800000), (0x7F7FFFFF, 0xFF7FFFFF), (0x7FC00000, 0x3F800000),
                 (0x40000000, 0x3F800000), (0x00000001, 0), (0x42C80000, 0x42C60000)):
        c = set_case(r, "GetAnimRemainingMilliseconds", 1)
        c["words"][7], c["words"][8] = a, b
        yield c


def time_case(r, fn):
    c = base(r, fn)
    c["a"], c["b"] = r.getrandbits(32), r.getrandbits(32)
    c["arg"] = r.choice([0, 1, -1, 2, 0x10000, r.getrandbits(32)]) if fn == "GetRideAnimTime" else r.getrandbits(32)
    return c


def edges_time(fn):
    def gen():
        r = random.Random(13)
        for arg in ((0, 1, -1, 2, 0x80000000, 0x7FFFFFFF) if fn == "GetRideAnimTime" else (0, 1, MASK, 0x80000000, 0x12345678)):
            yield dict(time_case(r, fn), arg=arg)
    return gen


def sinit_case(r, fn):
    c = base(r, fn)
    c["a"], c["b"] = r.getrandbits(32), r.getrandbits(32)
    return c


def fly_case(r, fn):
    c = base(r, fn)
    c["bases"] = [rw(r, 24) for _ in range(3)]
    c["table"] = [r.choice([0, 0, 1, 2, 3]) for _ in range(100)]
    c["fly"] = r.choice([r.randrange(-3, 105), r.randrange(1, 100)])
    c["a"] = r.getrandbits(32)
    return c


def edges_fly(fn):
    def gen():
        r = random.Random(14)
        for fly in (-1, 0, 1, 2, 50, 98, 99, 100, 101, 0x80000000, 0x7FFFFFFF):
            c = fly_case(r, fn)
            c["fly"] = fly
            c["table"] = [1 + (i % 3) for i in range(100)]
            yield c
            c = dict(c)
            c["table"] = [0] * 100
            yield c
    return gen


def plain_case(r, fn):
    return base(r, fn)


def single(fn):
    def gen():
        yield plain_case(random.Random(17), fn)
    return gen


TESTS = []
for _n, _a, _s in FUNCS:
    if _n in ("UpdateRideAnimSpeedAllChannels", "UpdateRideAnimSpeedImmediate", "GetCurrentAnim", "FlushDeferredAnim"):
        TESTS.append(FnTest(_n, ppc, host, edge=edges_set(_n), rnd=(lambda n: lambda r: set_case(r, n))(_n)))
    elif _n == "GetAnimRemainingMilliseconds":
        TESTS.append(FnTest(_n, ppc, host, edge=edges_remain, rnd=lambda r: remain_case(r, "GetAnimRemainingMilliseconds")))
    elif _n in ("GetRideAnimTime", "SetRideAnimTime"):
        TESTS.append(FnTest(_n, ppc, host, edge=edges_time(_n), rnd=(lambda n: lambda r: time_case(r, n))(_n)))
    elif _n in SINIT:
        TESTS.append(FnTest(_n, ppc, host, edge=single(_n), rnd=(lambda n: lambda r: sinit_case(r, n))(_n)))
    elif _n in ("FLY_SetRunway", "FLY_GetCarInBase"):
        TESTS.append(FnTest(_n, ppc, host, edge=edges_fly(_n), rnd=(lambda n: lambda r: fly_case(r, n))(_n)))
    else:
        TESTS.append(FnTest(_n, ppc, host, edge=single(_n), rnd=(lambda n: lambda r: plain_case(r, n))(_n)))
