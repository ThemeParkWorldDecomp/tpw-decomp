"""Unit w171, a ride script's screams and DrawPeep, against libtpw_unit_RideScream.dylib (tests/unit_RideScream.cpp), main app
("SimTheme Park"), src/game/rides/RideEffects.cpp, src/game/rides/rsse.cpp:
  SCREAM_Start 0x100CCB30, SCREAM_SetLevel 0x100CCCC0, SCREAM_Move 0x100CCD3C, SCREAM_Stop 0x100CCD98, SCREAM_Single 0x100CCDE8,
  SCREAM_SingleSad 0x100CD098, TriggerRepairParticleEffect 0x100CD190, DrawPeep 0x100CD388, RSSE_GetSoundEventCode 0x100D6D04,
  RSSE_GetCoasterHandle 0x100D6F34

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments and answer from the case's queues: SOUND_SendEvent, SOUND_SetParamValue,
SOUND_MoveSoundEffect, SOUND_FadeSoundEffect, PAR_InitEmitter, PAR_SetEmitterLife, CRideDatabase::GetRideInfo (a record whose repair effect
word the case sets), GetRideMasterBoundArea, GetNodePos (it fills the point from the queues "pos:<k>"), GetRideHeightAtPoint, the peep's
DrawMySpriteAt and HideMySprite, the clock (every read is logged) and the assertion.

The world of a case: the sound bank, the script record (Mac layout, arena blocks on the host) or the list of scripts of gRSSEState, the
walk record, gThingArray rows (objects with a type halfword), the ride set of gRideAnimSets (built by the library on the host), the two statics
of TriggerRepairParticleEffect and the sine table of gTPGlobals (restored after the case). What is compared: the answer, the log, the script
records, the walk record and the statics.

Fused operations: DrawPeep's multiply-adds and the fctiwz conversions run under HardwareRounding. Not tested: TriggerRepairParticleEffect
with a thing row that holds no object or a ride database that answers NULL (the original crashes); the point GetNodePos leaves unwritten when
it fails (the fake always writes it).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_RideScream"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST)
FN_START, FN_SETLEVEL, FN_MOVE, FN_STOP, FN_SINGLE, FN_SAD = 0x100CCB30, 0x100CCCC0, 0x100CCD3C, 0x100CCD98, 0x100CCDE8, 0x100CD098
FN_REPAIR, FN_DRAW, FN_EVENTCODE, FN_COASTER = 0x100CD190, 0x100CD388, 0x100D6D04, 0x100D6F34
SIZES = {FN_START: 356, FN_SETLEVEL: 84, FN_MOVE: 52, FN_STOP: 44, FN_SINGLE: 644, FN_SAD: 204, FN_REPAIR: 440, FN_DRAW: 1380,
         FN_EVENTCODE: 328, FN_COASTER: 140}
OWN = set(SIZES)

# ---- the callees faked on both sides
CLOCK_READ, GAME_ASSERT, SEND_EVENT, SET_PARAM, MOVE_SOUND, FADE_SOUND = 0x101472F4, 0x1000DDE8, 0x100DD9FC, 0x100DD380, 0x100DDF60, 0x100DE0FC
PAR_EMIT, PAR_LIFE, GET_RIDE_INFO, BOUND_AREA, DRAW_SPRITE, HIDE_SPRITE, GET_NODE_POS, RIDE_HEIGHT = (
    0x100BEC30, 0x100BE2E8, 0x10154868, 0x101ED058, 0x10114624, 0x10114758, 0x100CC888, 0x1005D198)

# ---- the Mac's addresses of the globals; the host library's come from tpw_rsc_addr(name)
ADDR = dict(gRideAnimSets=0x102C79A4, gThingArray=0x1034CBFC, gTPGlobals=0x103B74C8, gSoundBanks=0x102F7C94, gRSSEState=0x102F7BEC,
            gRepairEffectGuard=0x102F7BD4, gRepairEffectWord=0x102F7BD8)
# ---- the host's sizes and offsets the module relies on, checked once against tpw_rsc_layout (catches a header that moved)
HOST_LAYOUT = [0xF4, 32, 12, 4, 8, 0xC, 0x4000, 24, 0xE, 0x10]

# (name, argument kinds: p pointer, i int32; the answer: None void, "u" a word)
OPS = {
    "start": (FN_START, "start", "iiiiiii", "u"),
    "setlevel": (FN_SETLEVEL, "setlevel", "iii", "u"),
    "move": (FN_MOVE, "move", "iiii", "u"),
    "stop": (FN_STOP, "stop", "i", None),
    "single": (FN_SINGLE, "single", "iiiiii", "u"),
    "sad": (FN_SAD, "singlesad", "iiii", "u"),
    "repair": (FN_REPAIR, "repair", "pi", None),
    "draw": (FN_DRAW, "drawpeep", "ppiiii", None),
    "event": (FN_EVENTCODE, "eventcode", "ii", "u"),
    "coaster": (FN_COASTER, "coaster", "i", "u"),
}

SIN = [struct.unpack(">I", struct.pack(">f", __import__("math").sin(2 * __import__("math").pi * i / 4096)))[0] for i in range(4096)]


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only (Unicorn rounds fused operations twice and saturates fctiwz wrongly)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_clock(e):
    L = e.cl
    L.rec("Clock")
    e.set_r(3, T.u32(L.ret("clock")))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), [e.cstr(e.r(4))])


def stub_send(e):
    L = e.cl
    L.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, T.u32(L.ret("Event")))


def stub_param(e):
    L = e.cl
    L.rec("SOUND_SetParamValue", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, T.u32(L.ret("Param")))


def stub_move(e):
    L = e.cl
    L.rec("SOUND_MoveSoundEffect", tuple(s32(e.r(i)) for i in range(3, 7)))
    e.set_r(3, T.u32(L.ret("Move")))


def stub_fade(e):
    e.cl.rec("SOUND_FadeSoundEffect", (s32(e.r(3)),))


def stub_emitter(e):
    L = e.cl
    L.rec("PAR_InitEmitter", tuple(s32(e.r(i)) for i in range(3, 7)))
    e.set_r(3, T.u32(L.ret("emit")))


def stub_life(e):
    e.cl.rec("PAR_SetEmitterLife", (s32(e.r(3)), s32(e.r(4))))


def stub_info(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.alloc(0x100, struct.pack(">26I", *([0] * 26)) + struct.pack(">I", T.u32(L.ret("info"))) + bytes(0x100 - 108), alignment=4))


def stub_bound(e):
    L = e.cl
    L.rec("GetRideMasterBoundArea", (s32(e.r(3)),))
    answer = L.ret("bound")
    if answer:
        e.write(e.r(4), struct.pack(">6I", *[T.u32(L.ret("box%d" % i)) for i in range(6)]))
    e.set_r(3, T.u32(answer))


def stub_sprite(e):
    L = e.cl
    L.rec("DrawMySpriteAt", (L.lab(e.r(3)), canon_f(CL.fbits(e.f(1))), canon_f(CL.fbits(e.f(2))), canon_f(CL.fbits(e.f(3))), s32(e.r(7)), s32(e.r(8))))


def stub_hide(e):
    e.cl.rec("HideMySprite", (e.cl.lab(e.r(3)),))


def stub_nodepos(e):
    L = e.cl
    node, mask = s32(e.r(6)), T.u32(e.r(7))
    L.rec("GetNodePos", (L.lab(e.r(3)), node, s32(mask), 1 if e.r(5) else 0))
    k = (node & 3) * 6 + (3 if mask == 0x80 else 0)
    e.write(e.r(4), struct.pack(">3I", *[T.u32(L.ret("pos:%d" % (k + i))) for i in range(3)]))
    e.set_r(3, 1)


def stub_height(e):
    L = e.cl
    L.rec("GetRideHeightAtPoint", (canon_f(CL.fbits(e.f(1))), canon_f(CL.fbits(e.f(2)))))
    e.write(e.r(5), struct.pack(">I", T.u32(L.ret("height"))))


for _addr, _fn in ((CLOCK_READ, stub_clock), (GAME_ASSERT, stub_assert), (SEND_EVENT, stub_send), (SET_PARAM, stub_param),
                   (MOVE_SOUND, stub_move), (FADE_SOUND, stub_fade), (PAR_EMIT, stub_emitter), (PAR_LIFE, stub_life),
                   (GET_RIDE_INFO, stub_info), (BOUND_AREA, stub_bound), (DRAW_SPRITE, stub_sprite), (HIDE_SPRITE, stub_hide),
                   (GET_NODE_POS, stub_nodepos), (RIDE_HEIGHT, stub_height)):
    STUBS.internal(_addr, _fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, h0, h1):
        self.write(a, struct.pack(self.hfmt, h0 & 0xFFFF, h1 & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def save(self, a, n):
        self.undo.append((a, self.read(a, n)))

    def restore(self):
        for a, d in reversed(self.undo):
            self.write(a, d)
        self.undo = []


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    hfmt = ">HH"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.undo = []

    def done(self):
        self.restore()
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n, bytes(n), alignment=4)

    def ptr(self, a):
        return a

    def label(self, a, n):
        self.L.labels[a] = n

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def put_thing(self, index, obj):
        a = ADDR["gThingArray"] + 0x14 * index
        self.save(a, 4)
        self.words(a, [obj])

    def set_state(self, init, head):
        a = ADDR["gRSSEState"]
        self.save(a, 0x14)
        self.words(a, [init, 0, 0, 0, head])

    def make_ride(self, case):
        k, root = case["ride"], case["rootY"]
        if not k:
            return
        slot = ADDR["gRideAnimSets"] + 4 * k
        self.save(slot, 4)
        node = self.alloc(0x100)
        self.words(node + 0x44, [root])
        header = self.alloc(0x100)
        self.words(header + 0x78, [node])
        mesh = self.alloc(0x40)
        self.words(mesh + 4, [header])
        sset = self.alloc(0x100)
        self.words(sset + 8, [mesh])
        self.words(slot, [sset])

    def call(self, op, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, OPS[op][0], [a & MASK for a in args])
        return None if OPS[op][3] is None else (r3 & MASK)


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    hfmt = "<HH"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.undo = []
        self.rides = []
        self.things = []

    def done(self):
        self.restore()
        for k in self.rides:
            self.lib.tpw_rsc_ride_clear(k)
        for i in self.things:
            self.lib.tpw_rsc_thing(i, 0)
        self.lib.tpw_rsc_state(0, 0)

    def g(self, name):
        return self.lib.tpw_rsc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rsc_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def ptr(self, a):
        return self.lib.tpw_rsc_offset(a) if a else 0

    def label(self, a, n):
        self.lib.tpw_cl_label(n, a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_thing(self, index, obj):
        self.things.append(index)
        self.lib.tpw_rsc_thing(index, obj)

    def set_state(self, init, head):
        self.lib.tpw_rsc_state(init, head)

    def make_ride(self, case):
        k = case["ride"]
        if not k:
            return
        self.lib.tpw_rsc_ride(k, C.c_float(struct.unpack(">f", struct.pack(">I", case["rootY"]))[0]))
        self.rides.append(k)

    def call(self, op, args):
        fn = getattr(self.lib, "tpw_rsc_" + OPS[op][1])
        r = fn(*args)
        return None if OPS[op][3] is None else (r & MASK)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_rsc_addr.restype, lib.tpw_rsc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rsc_alloc.restype, lib.tpw_rsc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rsc_offset.restype, lib.tpw_rsc_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rsc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rsc_setinfo.argtypes = [C.c_int32]
    lib.tpw_rsc_ride.restype, lib.tpw_rsc_ride.argtypes = C.c_uint64, [C.c_int32, C.c_float]
    lib.tpw_rsc_ride_clear.argtypes = [C.c_int32]
    lib.tpw_rsc_thing.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_rsc_state.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    kinds = {"p": C.c_uint64, "i": C.c_int32}
    for _addr, name, spec, answer in OPS.values():
        fn = getattr(lib, "tpw_rsc_" + name)
        fn.argtypes = [kinds[k] for k in spec]
        fn.restype = None if answer is None else C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_rsc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

NOPTR = (0x00, 0x04, 0x1C, 0x88, 0xAC, 0xC0)        # the script record's links, whose values differ between the sides
SCRIPT_WORDS = [o for o in range(0, 0xF4, 4) if o not in NOPTR]


def put_script(s, d):
    """One script record of the case's dict d (offsets are the Mac's); answers its address."""
    sc = s.alloc(0xF4)
    s.words(sc + 0x08, [d.get("handle", 0)])
    s.words(sc + 0x14, [d.get("f14", 0)])
    s.words(sc + 0x80, [d.get("floatStart", 0)])
    s.words(sc + 0x84, [d.get("floatDur", 0)])
    s.halves(sc + 0x88, d.get("floatA", 0), d.get("floatB", 0))
    s.words(sc + 0x8C, [d.get("varCount", 0)])
    s.halves(sc + 0xAC, d.get("thingId", 0), 0)
    s.halves(sc + 0xC0, d.get("speed", 0), 0)
    s.words(sc + 0xC8, [d.get("rideAnim", 0)])
    s.words(sc + 0xCC, [d.get("fcc0", 0)])
    s.words(sc + 0xE0, [d.get("coaster", 0)])
    if d.get("vars"):
        vars_ = s.alloc(4 * len(d["vars"]))
        s.words(vars_, d["vars"])
        s.words(sc + 0x1C, [s.ptr(vars_)])
    return sc


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    g = s.g("gTPGlobals")
    s.save(g, 0xC + 4 * 4096)
    s.words(g, [case["sinmask"] + 1, case["sinmask"], case["sinscale"]])
    s.write(g + 0xC, struct.pack(s.fmt[0] + "4096I", *SIN))
    b = s.g("gSoundBanks")
    s.save(b, 8)
    s.words(b + 4, [case["bank"]])
    gd, gw = s.g("gRepairEffectGuard"), s.g("gRepairEffectWord")
    s.save(gd, 1)
    s.save(gw, 4)
    s.write(gd, bytes([case["guard"]]))
    s.words(gw, [case["word"]])
    s.make_ride(case)
    if case["info"] is not None:
        if isinstance(s, Host):
            s.lib.tpw_rsc_setinfo(C.c_int32(s32(case["info"])))
    for idx, kind in sorted(case["things"].items()):
        obj = s.alloc(0x20)
        s.label(obj, 30 + min(idx, 40))
        s.halves(obj + 0xC, 0, kind)
        s.put_thing(idx, obj)
    s.sc = s.walk = 0
    if case["sc"] is not None:
        s.sc = put_script(s, case["sc"])
        s.label(s.sc, 1)
    if case["walk"] is not None:
        w = case["walk"]
        s.walk = s.alloc(32)
        s.halves(s.walk, w["from"], w["to"])
        s.words(s.walk + 0x10, [w["peep"]])
        s.halves(s.walk + 0x14, w["dir"], w["kind"])
        s.halves(s.walk + 0x18, w["state"], w["last"])
    # the list of scripts
    addrs = [put_script(s, d) for d in case["scripts"]]
    for i, a in enumerate(addrs):
        s.label(a, 50 + i)
        if i + 1 < len(addrs):
            s.words(a, [s.ptr(addrs[i + 1])])
    s.scripts = addrs
    s.set_state(case["init"], addrs[0] if addrs else 0)


def execute(s, case):
    """The call."""
    args = [s.sc if a == "S" else s.walk if a == "W" else a for a in case["args"]]
    return s.call(case["op"], args)


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    out = {"answer": answer, "log": s.log()}
    out["statics"] = [s.read(s.g("gRepairEffectGuard"), 1)[0], s.rd_word(s.g("gRepairEffectWord"))]
    out["scripts"] = [[s.rd_word(a + o) for o in SCRIPT_WORDS] + s.rd_halves(a + 0x88, 2) + s.rd_halves(a + 0xAC, 2) + s.rd_halves(a + 0xC0, 2) for a in ([s.sc] if s.sc else []) + s.scripts]
    if s.walk:
        out["walk"] = s.rd_halves(s.walk, 4) + s.rd_words(s.walk + 8, 3) + s.rd_halves(s.walk + 0x14, 4)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

IMAX, IMIN = 2 ** 31 - 1, -2 ** 31
COUNTS = [-2, 0, 1, 2, 3, 4, 7, 8, 9, IMAX, IMIN]
SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000]


def wrap(v):
    return s32(v & MASK)


def base(r, op, args):
    return {"op": op, "args": args, "queues": {}, "bank": r.choice([0, 1, 7, 0x1234, -1]), "sinmask": 0xFFF,
            "sinscale": fb(651.8986), "guard": 0, "word": 0, "ride": 0, "rootY": 0, "info": None, "things": {}, "sc": None,
            "walk": None, "scripts": [], "init": 1}


def answers(r, c, names=("Event", "Param", "Move")):
    for n in names:
        c["queues"][n] = [r.choice([0, 0, 1, r.randint(-5, 1000), -1, IMAX, IMIN])]


def pair(r, total):
    a = r.randint(IMIN, IMAX)
    return a, wrap(total - a)


SUMS = [IMIN, -401, -200, -101, -100, -51, -50, -49, -3, -2, -1, 0, 1, 2, 3, 49, 50, 51, 99, 100, 101, 148, 149, 150, 151, 198, 199, 200,
        201, 202, 203, 250, 1000, IMAX]
WRAPS = [(IMAX, IMAX), (IMIN, IMIN), (IMAX, 1), (IMIN, -1), (IMAX, IMIN), (0, 0), (1, 0)]


def level_args(r, i):
    if i < len(WRAPS):
        return WRAPS[i]
    return pair(r, SUMS[(i - len(WRAPS)) % len(SUMS)])


def start_case(r, h=None, count=None, ab=None):
    a, b = ab if ab else (pair(r, r.choice(SUMS)) if r.random() < 0.8 else (r.randint(IMIN, IMAX), r.randint(IMIN, IMAX)))
    c = base(r, "start", [r.choice([0, 0, 0, 1, 5, -1, IMAX]) if h is None else h, r.choice(COUNTS) if count is None else count, a, b,
                          r.randint(IMIN, IMAX), r.choice([0, 1, -1, 77]), r.randint(-9, 9)])
    answers(r, c)
    return c


def start_edges():
    r = random.Random(17101)
    for count in COUNTS:
        for h in (0, 3):
            yield start_case(r, h, count)
    for i in range(len(WRAPS) + len(SUMS)):
        yield start_case(r, 0, r.choice([1, 2, 5]), level_args(r, i))


def setlevel_case(r, h=None, ab=None):
    a, b = ab if ab else (pair(r, r.choice(SUMS)) if r.random() < 0.8 else (r.randint(IMIN, IMAX), r.randint(IMIN, IMAX)))
    c = base(r, "setlevel", [r.choice([0, 1, 9, -4]) if h is None else h, a, b])
    answers(r, c)
    return c


def setlevel_edges():
    r = random.Random(17102)
    for i in range(len(WRAPS) + len(SUMS)):
        for h in (0, 2):
            yield setlevel_case(r, h, level_args(r, i))


def move_case(r, h=None):
    c = base(r, "move", [r.choice([0, 1, 7, -3]) if h is None else h, r.randint(IMIN, IMAX), r.randint(-5000, 5000), r.randint(IMIN, IMAX)])
    answers(r, c)
    return c


def move_edges():
    r = random.Random(17103)
    for h in (0, 1, -1, IMAX, IMIN):
        yield move_case(r, h)


def stop_case(r, h=None):
    return base(r, "stop", [r.choice([0, 1, 7, -3, IMAX]) if h is None else h])


def stop_edges():
    r = random.Random(17104)
    for h in (0, 1, -1, IMAX, IMIN):
        yield stop_case(r, h)


def single_case(r, count=None, ab=None):
    a, b = ab if ab else (pair(r, r.choice(SUMS)) if r.random() < 0.8 else (r.randint(IMIN, IMAX), r.randint(IMIN, IMAX)))
    c = base(r, "single", [r.choice(COUNTS) if count is None else count, a, b, r.randint(IMIN, IMAX), r.choice([0, 1, -1]), r.randint(-9, 9)])
    answers(r, c, ("Event",))
    return c


def single_edges():
    r = random.Random(17105)
    for count in COUNTS:
        yield single_case(r, count)
    for i in range(len(WRAPS) + len(SUMS)):
        for count in (1, 3, 5, 9):
            yield single_case(r, count, level_args(r, i))


def sad_case(r, count=None):
    c = base(r, "sad", [r.choice(COUNTS) if count is None else count, r.randint(IMIN, IMAX), r.choice([0, 1, -1]), r.randint(-9, 9)])
    answers(r, c, ("Event",))
    return c


def sad_edges():
    r = random.Random(17106)
    for count in COUNTS:
        yield sad_case(r, count)


def pf(r):
    """A coordinate's bits (small enough that 1024x stays inside 32 bits)."""
    k = r.random()
    if k < 0.1:
        return 0
    if k < 0.2:
        return 0x80000000
    return fb(r.uniform(-1000, 1000))


def repair_case(r, on=None, guard=None, fcc0=None, thing=None, info=None, ride=None, bound=None):
    on = r.choice([0, 1, 1]) if on is None else on
    c = base(r, "repair", ["S", on])
    c["guard"] = r.choice([0, 1]) if guard is None else guard
    c["word"] = r.choice([0, -1, 5, r.randint(IMIN, IMAX)])
    fcc = r.choice([0, 0, r.randint(1, MASK), 1]) if fcc0 is None else fcc0
    tid = r.choice([0, 1, 2, 3, 4, 5, 6, 6]) if thing is None else thing
    c["sc"] = {"thingId": tid, "fcc0": fcc, "rideAnim": r.choice([0, 1, 2, 5, 15]) if ride is None else ride}
    c["things"] = {i: r.choice([0, 1, 5, 0x7FFF, 0xFFFF, r.randrange(0x10000)]) for i in range(1, 7)}
    if r.random() < 0.15:
        c["things"][0] = 9
    c["info"] = r.choice([0, 0, r.randint(1, 60), r.randint(IMIN, IMAX), -1]) if info is None else info
    c["queues"]["info"] = [c["info"]]
    c["ride"] = c["sc"]["rideAnim"]
    c["rootY"] = pf(r)
    c["queues"]["bound"] = [r.choice([0, 1, 1]) if bound is None else bound]
    for i in range(6):
        c["queues"]["box%d" % i] = [pf(r)]
    c["queues"]["emit"] = [r.choice([0, 1, r.randint(1, MASK)])]
    return c


def repair_edges():
    r = random.Random(17107)
    for on in (0, 1):
        for guard in (0, 1):
            for fcc in (0, 5):
                yield repair_case(r, on, guard, fcc, 3, 7, 1, 1)
    for thing in range(0, 7):
        yield repair_case(r, 1, 1, 0, thing, 9, 2, 1)
    for info in (0, 1, -1):
        yield repair_case(r, 1, 0, 0, 2, info, 1, 1)
    for ride in (0, 1):
        for bound in (0, 1):
            yield repair_case(r, 1, 1, 0, 4, 3, ride, bound)
    c = repair_case(r, 1, 1, 0, 2, 3, 1, 1)
    c["things"][0] = 4
    yield c
    c = repair_case(r, 1, 1, 0, 2, 3, 1, 1)
    for i in range(6):
        c["queues"]["box%d" % i] = [0]
    c["rootY"] = 0
    yield c


NODE_CHOICES = [0, 1, 2, 3, 4, 5, 6, 7, -1, -2, 100, 0x7FFF, -0x8000]
FRACTIONS = [-5, 0, 1, 500, 999, 1000, 1001, IMAX, IMIN, 2000, -1000]


def draw_case(r, mode=None, state=None, kind=None, fraction=None, last=None, dur=None, miny=False):
    mode = r.randint(0, 3) if mode is None else mode
    state = r.choice([0, 1, 2, 2, 2, 3, 4]) if state is None else state
    kind = r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7, 8]) if kind is None else kind
    fraction = r.choice(FRACTIONS + [r.randint(-100, 1100)]) if fraction is None else fraction
    last = r.choice([0, 1, 2, 3, 0x7FFF, -1]) if last is None else last
    row = r.choice([1, 2, 3, 4, 5, 6])
    peep = ((r.choice([0, 1, 0x7FFF, 0xFFFF, r.randrange(0x10000)]) << 16) | row) & MASK
    if r.random() < 0.3:
        peep = (peep & ~0xFFFF) | r.choice([1, 2, 3, 4, 5, 6]) | (r.randrange(0x10000) & 0)   # keep the row; the id's low bits are the row
    c = base(r, "draw", ["S", "W", r.choice(NODE_CHOICES), r.choice(NODE_CHOICES), mode, fraction])
    c["sinscale"] = r.choice([fb(162.97466), fb(651.8986), fb(r.uniform(10, 700))])
    c["sinmask"] = r.choice([0xFFF, 0xFFF, 0x3FF, 0x1FF])
    now = r.choice([r.randint(0, 100000), r.randint(0, 50), r.randint(0, MASK), 0x7FFFFFFF])
    dur = r.choice([0, 1, r.randint(2, 5000), r.randint(1, MASK), 0x80000000, 0xFFFFFFFF]) if dur is None else dur
    start = now - r.choice([0, 1, 100, 500, -50, -4000, r.randint(-100000, 100000)])
    c["sc"] = {"floatDur": dur, "floatStart": start & MASK, "floatA": r.choice([0, 1000, 0xFFFF, r.randrange(0x10000)]),
               "floatB": r.choice([0, 1000, 0xFFFF, r.randrange(0x10000)]), "speed": r.choice([-100, 0, 50, 100, 300, r.randint(-100, 300)])}
    c["walk"] = {"from": 0, "to": 0, "peep": peep, "dir": r.choice([-8, -1, 0, 1, 7, 8, 15, r.randint(-8, 15)]), "kind": kind, "state": state,
                 "last": last}
    c["things"] = {i: 0 for i in range(1, 7)}
    if r.random() < 0.1:
        c["things"][0] = 3
    c["queues"]["clock"] = [r.choice([now, r.randint(0, 5000), r.randint(0, MASK)]) for _ in range(1)] + [now, r.choice([now, now + 7, r.randint(0, MASK)])]
    for k in range(24):
        c["queues"]["pos:%d" % k] = [pf(r)]
    c["queues"]["height"] = [pf(r)]
    return c


def draw_edges():
    r = random.Random(17108)
    for mode in range(4):
        for fraction in FRACTIONS:
            yield draw_case(r, mode, 0, 0, fraction, 0)
            yield draw_case(r, mode, 1, 0, fraction, 1)
    for kind in range(-1, 9):
        yield draw_case(r, 1, 2, kind, 500, 0, 0)
        yield draw_case(r, 3, 2, kind, 500, 1, 0)
    for state in range(5):
        yield draw_case(r, 2, state, 3, 250, 1, 0)
        yield draw_case(r, 0, state, 3, 250, 0, 1000)
    for dur in (1, 2, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        for _ in range(3):
            yield draw_case(r, r.randint(0, 3), 2, 2, r.randint(0, 1000), r.randint(0, 1), dur)
    for last in (0, 1, 2, 3):
        yield draw_case(r, 1, 2, 3, 100, last, 0)
        yield draw_case(r, 2, 2, 3, 100, last, 7)
    for _ in range(40):
        c = draw_case(r, r.randint(0, 3), 2, r.randint(0, 8), r.randint(0, 1000), r.randint(0, 1))
        c["sc"]["floatDur"] = r.randint(1, 5000)
        yield c


def event_case(r, init=None, nscripts=None, match=None, f14=None, nvars=None, index=None):
    init = r.choice([0, 1, 1, 1]) if init is None else init
    n = r.randint(0, 4) if nscripts is None else nscripts
    handles = [r.choice([1, 2, 3, 4, 5, 9, -3, 0x1234]) for _ in range(n)]
    scripts = [{"handle": h, "coaster": r.choice([0, 7, r.randint(1, MASK)]), "varCount": 0, "f14": 0} for h in handles]
    target = r.choice([-100, -100] + handles) if match is None else match
    for d in scripts:
        d["f14"] = r.choice([0, 1, 2, 3, 4, 5, 9, -3, r.randint(IMIN, IMAX)])
        k = r.randint(0, 6)
        d["varCount"] = k
        d["vars"] = [r.randint(IMIN, IMAX) for _ in range(k)]
    return init, scripts, target


def eventcode_case(r, init=None, nscripts=None, match=None, f14=None, nvars=None, index=None):
    init, scripts, target = event_case(r, init, nscripts, match)
    # the sound script: pick what the matched script's +0x14 points to
    hs = [d["handle"] for d in scripts]
    for d in scripts:
        if d["handle"] == target:
            mode = r.choice(["none", "self", "other", "other"]) if f14 is None else f14
            if mode == "none":
                d["f14"] = -77
            elif mode == "self":
                d["f14"] = d["handle"]
            else:
                d["f14"] = r.choice(hs)
            break
    if nvars is not None:
        for d in scripts:
            d["varCount"] = nvars
            d["vars"] = [r.randint(IMIN, IMAX) for _ in range(nvars)]
    c = base(r, "event", [target, r.randint(-2, 8) if index is None else index])
    c["scripts"], c["init"] = scripts, init
    return c


def eventcode_edges():
    r = random.Random(17109)
    yield eventcode_case(r, 0, 2, 1)
    for n in range(5):
        for match in ("none", "first", "last"):
            for f14 in ("none", "self", "other"):
                c = eventcode_case(r, 1, n, None, f14, 4, 1)
                hs = [d["handle"] for d in c["scripts"]]
                if n and match != "none":
                    t = hs[0] if match == "first" else hs[-1]
                    c["args"][0] = t
                    for d in c["scripts"]:
                        if d["handle"] == t:
                            d["f14"] = {"none": -77, "self": t, "other": hs[-1] if t != hs[-1] else hs[0]}[f14]
                            break
                else:
                    c["args"][0] = -100
                yield c
    for nvars in range(7):
        for index in range(-2, 9):
            c = eventcode_case(r, 1, 2, None, "self", nvars, index)
            c["args"][0] = c["scripts"][0]["handle"]
            c["scripts"][0]["f14"] = c["scripts"][0]["handle"]
            yield c


def coaster_case(r, init=None, nscripts=None, match=None):
    init, scripts, target = event_case(r, init, nscripts, match)
    c = base(r, "coaster", [target])
    c["scripts"], c["init"] = scripts, init
    return c


def coaster_edges():
    r = random.Random(17110)
    yield coaster_case(r, 0, 2, 1)
    for n in range(5):
        for match in ("none", "first", "last"):
            c = coaster_case(r, 1, n)
            hs = [d["handle"] for d in c["scripts"]]
            c["args"][0] = (hs[0] if match == "first" else hs[-1]) if n and match != "none" else -100
            yield c


def mk(name, edges, rnd):
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [
    mk("SCREAM_Start", start_edges, start_case),
    mk("SCREAM_SetLevel", setlevel_edges, setlevel_case),
    mk("SCREAM_Move", move_edges, move_case),
    mk("SCREAM_Stop", stop_edges, stop_case),
    mk("SCREAM_Single", single_edges, single_case),
    mk("SCREAM_SingleSad", sad_edges, sad_case),
    mk("TriggerRepairParticleEffect", repair_edges, repair_case),
    mk("DrawPeep", draw_edges, draw_case),
    mk("RSSE_GetSoundEventCode", eventcode_edges, eventcode_case),
    mk("RSSE_GetCoasterHandle", coaster_edges, coaster_case),
]
