"""Unit w149, the peep tables of a ride script, against libtpw_unit_RidePeeps.dylib (tests/unit_RidePeeps.cpp), main app
("SimTheme Park"), src/rsse_peeps.cpp:
  GetNodePos 0x100cc888, WALKNODE_Tick 0x100cd928, WALKNODE_Trigger 0x100cdd4c, WALKNODE_Get 0x100cdfc8, WALKNODE_Float 0x100ce054,
  WALKNODE_Release 0x100ce0f4, BOUNCE_Put 0x100ce3d4, BOUNCE_Get 0x100ce4ec, BOUNCE_ForceGet 0x100ce610, BOUNCE_Tick 0x100ce740,
  LIMBO_Put 0x100ceab0, LIMBO_Get 0x100ceb90, LIMBO_ForceGet 0x100cec58

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments and answer from the case's queues: the clock (every read is logged), the
assertion (format and the script's name), DrawPeep, SetRideSpritePosnFocus, EnableRideHead, DisableRideHead, FindDataNodeIndexByID (one
queue for the walk-node mask 0x800, one for the head mask 0x80), PAR_InitEmitter, CPeep::DrawMySpriteAt, GetRideMasterBoundArea,
GetRideFromID, GetDataNodeWorldMatrix (it may also change the index it is given by reference) and NormaliseVector. GetNodePos is the real
one in every case that reaches it.

The world of a case: the script record (Mac layout, arena blocks on the host) with its walk, bounce and limbo tables and variables; the ride
set k of gRideAnimSets with its mesh, root node height and embedded records (built by the library on the host, because their pointers are
native there); gThingArray rows; the sine table of gTPGlobals (restored after the case). What is compared: the answer, the log, the script's
counters, the three tables, and the position and direction buffers of GetNodePos.

Fused operations: the squared distance of a leg (fmadds twice), BOUNCE_Tick's height (fmadds), and the fctiwz conversions run under
HardwareRounding. Not tested: a walk record whose end time equals its start time (divwu by zero is undefined); a walk-node lookup that fails
inside Release or Tick (the original then reads stack garbage; Trigger's failing lookups are tested with the end time and direction of the
record left out of the comparison); a ride number whose set is missing, a thing row that holds no object.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_RidePeeps"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST)
FN_GN, FN_TICK, FN_TRIG, FN_WGET, FN_WFLOAT, FN_WREL = 0x100CC888, 0x100CD928, 0x100CDD4C, 0x100CDFC8, 0x100CE054, 0x100CE0F4
FN_BPUT, FN_BGET, FN_BFORCE, FN_BTICK = 0x100CE3D4, 0x100CE4EC, 0x100CE610, 0x100CE740
FN_LPUT, FN_LGET, FN_LFORCE = 0x100CEAB0, 0x100CEB90, 0x100CEC58
SIZES = {FN_GN: 608, FN_TICK: 1008, FN_TRIG: 576, FN_WGET: 92, FN_WFLOAT: 104, FN_WREL: 680, FN_BPUT: 232, FN_BGET: 244, FN_BFORCE: 252,
         FN_BTICK: 832, FN_LPUT: 176, FN_LGET: 152, FN_LFORCE: 88}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
CLOCK_READ, GAME_ASSERT, DRAW_PEEP, FIND_NODE, DRAW_SPRITE, PAR_EMIT = 0x101472F4, 0x1000DDE8, 0x100CD388, 0x101FFDC8, 0x10114624, 0x100BEC30
DISABLE_HEAD, ENABLE_HEAD, BOUND_AREA, SET_FOCUS, GET_RIDE, WORLD_MATRIX, NORMALISE = (
    0x1020039C, 0x10200230, 0x101ED058, 0x1005CC10, 0x1002AE38, 0x1002ADDC, 0x1002AD34)

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_rp_addr(name)
ADDR = dict(gRideAnimSets=0x102C79A4, gThingArray=0x1034CBFC, gTPGlobals=0x103B74C8)
# ---- the host's sizes the module relies on, checked once against tpw_rp_layout (catches a header that moved)
HOST_LAYOUT = [0xF4, 32, 16, 8, 12, 4, 8, 0xC, 0x4000]

# (address, name, argument kinds after the script: p pointer, i int32, u uint32; void functions have no answer)
OPS = {
    "gn": (FN_GN, "getnodepos", "pppii", False),
    "trig": (FN_TRIG, "trigger", "puiiiiii", True),
    "wget": (FN_WGET, "walkget", "p", False),
    "wfloat": (FN_WFLOAT, "walkfloat", "piii", True),
    "wrel": (FN_WREL, "walkrelease", "pu", True),
    "wtick": (FN_TICK, "walktick", "p", True),
    "bput": (FN_BPUT, "bounceput", "pui", False),
    "bget": (FN_BGET, "bounceget", "p", False),
    "bforce": (FN_BFORCE, "bounceforceget", "p", False),
    "btick": (FN_BTICK, "bouncetick", "p", True),
    "lput": (FN_LPUT, "limboput", "pui", False),
    "lget": (FN_LGET, "limboget", "p", False),
    "lforce": (FN_LFORCE, "limboforceget", "p", False),
}

SIN = [struct.unpack(">I", struct.pack(">f", math.sin(2 * math.pi * i / 4096)))[0] for i in range(4096)]


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


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
    L = e.cl
    fmt = e.cstr(e.r(4))
    texts = [fmt]
    if "%s" in fmt:
        texts.append(e.cstr(e.r(5)))
    L.rec("GAME_ASSERTMSG", (s32(e.r(3)),), texts)


def stub_draw_peep(e):
    L = e.cl
    L.rec("DrawPeep", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def stub_focus(e):
    e.cl.rec("SetRideSpritePosnFocus", (e.cl.lab(e.r(3)),))


def stub_enable(e):
    e.cl.rec("EnableRideHead", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def stub_disable(e):
    e.cl.rec("DisableRideHead", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_find(e):
    L = e.cl
    L.rec("FindDataNodeIndexByID", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, T.u32(L.ret("findHead" if T.u32(e.r(4)) == 0x80 else "findWalk")))


def stub_emitter(e):
    e.cl.rec("PAR_InitEmitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_sprite(e):
    L = e.cl
    L.rec("DrawMySpriteAt", (L.lab(e.r(3)), canon_f(CL.fbits(e.f(1))), canon_f(CL.fbits(e.f(2))), canon_f(CL.fbits(e.f(3))), s32(e.r(7)), s32(e.r(8))))


def stub_bound(e):
    L = e.cl
    L.rec("GetRideMasterBoundArea", (s32(e.r(3)),))
    answer = L.ret("bound")
    if answer:
        e.write(e.r(4), struct.pack(">6I", *[T.u32(L.ret("box%d" % i)) for i in range(6)]))
    e.set_r(3, T.u32(answer))


def stub_get_ride(e):
    L = e.cl
    ident = e.u32(e.r(3))
    L.rec("GetRideFromID", (s32(ident),))
    cell = e.alloc(4, struct.pack(">I", e.u32(ADDR["gRideAnimSets"] + 4 * ident)), alignment=4)
    e.set_r(3, cell)


def stub_matrix(e):
    L = e.cl
    mesh, index = e.u32(e.r(3)), e.u32(e.r(4))
    L.rec("GetDataNodeWorldMatrix", (L.lab(mesh), s32(index)))
    changed = L.ret("newIndex", -1)
    if changed != -1:
        e.write(e.r(4), struct.pack(">I", T.u32(changed)))
    if L.ret("matrix") == 0:
        e.set_r(3, 0)
        return
    data = struct.pack(">16I", *[T.u32(L.ret("m%d" % i)) for i in range(16)])
    e.set_r(3, e.alloc(64, data, alignment=4))


def stub_normalise(e):
    L = e.cl
    a = e.r(3)
    L.rec("NormaliseVector", tuple(canon_f(e.u32(a + 4 * i)) for i in range(3)))
    e.write(a, struct.pack(">3I", *[T.u32(L.ret("nv%d" % i)) for i in range(3)]))


for _addr, _fn in ((CLOCK_READ, stub_clock), (GAME_ASSERT, stub_assert), (DRAW_PEEP, stub_draw_peep), (FIND_NODE, stub_find),
                   (DRAW_SPRITE, stub_sprite), (PAR_EMIT, stub_emitter), (DISABLE_HEAD, stub_disable), (ENABLE_HEAD, stub_enable),
                   (BOUND_AREA, stub_bound), (SET_FOCUS, stub_focus), (GET_RIDE, stub_get_ride), (WORLD_MATRIX, stub_matrix),
                   (NORMALISE, stub_normalise)):
    STUBS.internal(_addr, _fn)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)       # the C library's memset, memcpy, strlen, sqrt ...
    e.stub("atan2", lambda emu: emu.set_f1(math.atan2(emu.f(1), emu.f(2))))


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

    def make_ride(self, case):
        k = case["ride"]
        if not k:
            return
        slot = ADDR["gRideAnimSets"] + 4 * k
        self.save(slot, 4)
        recs = case["recs"]
        node = self.alloc(0x100)
        self.words(node + 0x44, [case["rootY"]])
        header = self.alloc(0x100)
        self.words(header + 0x78, [node])
        records = self.alloc(0x14 * len(recs))
        for i, (flags, has, blk) in enumerate(recs):
            block = 0
            if has:
                block = self.alloc(0x40)
                self.words(block, blk)
            self.words(records + 0x14 * i, [flags, block])
        embedded = self.alloc(0x14)
        self.words(embedded + 4, [records])
        mesh = self.alloc(0x40)
        self.words(mesh + 4, [header])
        self.words(mesh + 0x28, [embedded])
        sset = self.alloc(0x100)
        self.words(sset + 8, [mesh])
        self.words(slot, [sset])
        self.label(sset, 2)
        self.label(mesh, 3)

    def call(self, op, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, OPS[op][0], [a & MASK for a in args])
        return None if OPS[op][3] else (r3 & MASK)


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
            self.lib.tpw_rp_ride_clear(k)
        for i in self.things:
            self.lib.tpw_rp_thing(i, 0)

    def g(self, name):
        return self.lib.tpw_rp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rp_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def ptr(self, a):
        return self.lib.tpw_rp_offset(a) if a else 0

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
        self.lib.tpw_rp_thing(index, obj)

    def make_ride(self, case):
        k = case["ride"]
        if not k:
            return
        recs = case["recs"]
        n = len(recs)
        flags = (C.c_uint32 * n)(*[r[0] for r in recs])
        has = (C.c_uint8 * n)(*[1 if r[1] else 0 for r in recs])
        blocks = (C.c_float * (16 * n))()
        for i, r in enumerate(recs):
            for j in range(16):
                blocks[16 * i + j] = bitsf(r[2][j])
        sset = self.lib.tpw_rp_ride(k, n, flags, has, blocks, C.c_float(bitsf(case["rootY"])))
        self.rides.append(k)
        self.label(sset, 2)
        self.label(self.lib.tpw_rp_mesh(k), 3)

    def call(self, op, args):
        fn = getattr(self.lib, "tpw_rp_" + OPS[op][1])
        r = fn(*args)
        return None if OPS[op][3] else (r & MASK)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_rp_addr.restype, lib.tpw_rp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rp_alloc.restype, lib.tpw_rp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rp_offset.restype, lib.tpw_rp_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_rp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rp_ride.restype = C.c_uint64
    lib.tpw_rp_ride.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint8), C.POINTER(C.c_float), C.c_float]
    lib.tpw_rp_mesh.restype, lib.tpw_rp_mesh.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_rp_ride_clear.argtypes = [C.c_int32]
    lib.tpw_rp_thing.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    kinds = {"p": C.c_uint64, "i": C.c_int32, "u": C.c_uint32}
    for _addr, name, spec, void in OPS.values():
        fn = getattr(lib, "tpw_rp_" + name)
        fn.argtypes = [kinds[k] for k in spec]
        fn.restype = None if void else C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_rp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

# the script record's words the comparison reads (the links are pointers, whose values differ between the sides)
SCRIPT_WORDS = [o for o in range(0x3C, 0x90, 4) if o not in (0x6C, 0x88)]


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    g = s.g("gTPGlobals")
    s.save(g, 0xC + 4 * 4096)
    s.words(g, [case["sinmask"] + 1, case["sinmask"], case["sinscale"]])
    s.write(g + 0xC, struct.pack(s.fmt[0] + "4096I", *SIN))
    s.make_ride(case)
    sc = s.sc = s.alloc(0xF4)
    s.label(sc, 1)
    d = case["sc"]
    walks, bounces, limbos = case["walks"], case["bounces"], case["limbos"]
    vars_ = s.alloc(64)
    s.words(vars_ + 28, [d["vars7"]])
    name = s.alloc(16)
    s.write(name, b"Ride\0")
    wt = s.walk0 = s.alloc(32 * max(len(walks), 1))
    bt = s.bounce0 = s.alloc(16 * max(len(bounces), 1))
    lt = s.limbo0 = s.alloc(8 * max(len(limbos), 1))
    s.words(sc + 0x1C, [s.ptr(vars_)])
    s.words(sc + 0x24, [s.ptr(lt)])
    s.words(sc + 0x28, [s.ptr(bt)])
    s.words(sc + 0x2C, [s.ptr(wt)])
    s.words(sc + 0x38, [s.ptr(name)])
    s.words(sc + 0x58, [len(limbos)])
    s.words(sc + 0x60, [d["limboUsed"]])
    s.words(sc + 0x64, [len(bounces)])
    s.halves(sc + 0x6C, d["bounceUsed"], d["height"])
    s.words(sc + 0x70, [d["base"]])
    s.words(sc + 0x7C, [len(walks)])
    s.words(sc + 0x80, [d["floatStart"]])
    s.words(sc + 0x84, [d["floatDur"]])
    s.halves(sc + 0x88, d["floatA"], d["floatB"])
    s.halves(sc + 0xC0, d["speed"], 0)
    s.words(sc + 0xC8, [case["ride"]])
    for i, w in enumerate(walks):
        a = wt + 32 * i
        s.label(a, 10 + i)
        s.halves(a, w["from"], w["to"])
        s.halves(a + 4, w["ofrom"], w["oto"])
        s.words(a + 8, [w["start"], w["end"], w["peep"]])
        s.halves(a + 0x14, w["dir"], w["kind"])
        s.halves(a + 0x18, w["state"], w["last"])
    for i, (peep, node, rel, start) in enumerate(bounces):
        s.words(bt + 16 * i, [peep, node, rel, start])
    for i, (peep, rel) in enumerate(limbos):
        s.words(lt + 8 * i, [peep, rel])
    for idx in sorted(case["things"]):
        obj = s.alloc(16)
        s.label(obj, 30 + min(idx, 40))
        s.put_thing(idx, obj)
    s.pos = s.dir = 0
    if case["op"] == "gn":
        s.pos = s.alloc(12)
        s.words(s.pos, [0x3F000000] * 3)
        if case["dirbuf"]:
            s.dir = s.alloc(12)
            s.words(s.dir, [0x3F000000] * 3)


def execute(s, case):
    """The call."""
    op, a = case["op"], case["args"]
    if op == "gn":
        return s.call(op, [s.sc, s.pos, s.dir, a[0], a[1]])
    return s.call(op, [s.sc] + list(a))


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    sc = s.sc
    out = {"answer": answer, "log": s.log()}
    out["script"] = [s.rd_word(sc + o) for o in SCRIPT_WORDS]
    out["halves"] = s.rd_halves(sc + 0x6C, 2) + s.rd_halves(sc + 0x88, 2)
    walks = []
    for i in range(len(case["walks"])):
        a = s.walk0 + 32 * i
        w = s.rd_halves(a, 4) + s.rd_words(a + 8, 3) + s.rd_halves(a + 0x14, 4)
        if case.get("lossy"):
            w[5] = 0       # mEnd and mDirection come from positions the original left as stack garbage
            w[7] = 0
        walks.append(w)
    out["walks"] = walks
    out["bounces"] = [s.rd_words(s.bounce0 + 16 * i, 4) for i in range(len(case["bounces"]))]
    out["limbos"] = [s.rd_words(s.limbo0 + 8 * i, 2) for i in range(len(case["limbos"]))]
    if s.pos:
        out["pos"] = [canon_f(w) for w in s.rd_words(s.pos, 3)]
    if s.dir:
        out["dir"] = [canon_f(w) for w in s.rd_words(s.dir, 3)]
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

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
NODES = [0, 1, 2, 3, 5, -1, 100, -100, 0x7FFF, -0x8000]
ELAPSED = [0, 1, 199, 200, 201, 399, 400, 999, 1000, 1001, 1199, 1200, 2000, 2999, 12345, -1, -199, -200, -999, -1000, -1001, -12345]


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def pf(r):
    """A position's bits: mostly within a park, rarely a special value."""
    k = r.random()
    if k < 0.04:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-30, 30))
    return fb(r.uniform(-1000, 1000))


def mk_recs(r, n):
    recs = []
    for _ in range(n):
        flags = r.choice([0, 1, 0x10, 0x11, 0x12, 0x30, 0x31, 0x1F])
        blk = [rf(r) for _ in range(16)]
        for j in (12, 13, 14):
            blk[j] = pf(r)
        recs.append((flags, r.random() < 0.85, blk))
    return recs


def valid(r, n, count=12):
    return [r.randrange(n) for _ in range(count)]


def base(r, op, ride=None, nrec=None):
    """The world every case starts from: a ride set, a script with empty tables, and the fakes' answer queues (key -> list)."""
    n = r.randint(3, 6) if nrec is None else nrec
    k = r.choice([1, 2, 3]) if ride is None else ride
    q = {
        "clock": [r.randint(-100000, 100000)],
        "findWalk": valid(r, n), "findHead": valid(r, n),
        "bound": [r.choice([0, 1, 1])],
        "matrix": [r.choice([0, 1, 1])],
        "nv0": [rf(r)], "nv1": [rf(r)], "nv2": [rf(r)],
    }
    for i in range(6):
        q["box%d" % i] = [pf(r)]
    for i in range(16):
        q["m%d" % i] = [rf(r)]
    if r.random() < 0.3:
        q["newIndex"] = [r.randrange(n)]
    return {
        "op": op, "ride": k, "recs": mk_recs(r, n), "rootY": pf(r), "queues": q, "args": [],
        "sinmask": r.choice([0xFFF, 0x3FF, 0x1FF]), "sinscale": r.choice([fb(162.97466), fb(651.8986), fb(r.uniform(10, 700))]),
        "sc": {"vars7": r.choice([0, 1]), "limboUsed": r.randint(-2, 5), "bounceUsed": r.randint(-2, 5), "height": r.choice([0, 10, -7, r.randint(-300, 300)]),
               "base": r.choice([0, 1, 100, -5, r.randint(-1000, 1000)]), "floatStart": 0, "floatDur": 0, "floatA": 0, "floatB": 0,
               "speed": r.choice([0, 100, -50, 200, -200, r.randint(-32768, 32767)])},
        "walks": [], "bounces": [], "limbos": [], "things": set(), "dirbuf": False, "lossy": False,
    }


def peep_id(r, idx=None):
    """A thing id: the low halfword is the row of gThingArray, the high half is whatever."""
    i = r.choice([1, 2, 5, 0x27FF, 3]) if idx is None else idx
    return ((r.choice([0, 0, 1, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0x10000)]) << 16) | i) & MASK


def mk_walk(r, now, state=None, kind=None, n=3):
    start = now - r.randint(-1500, 3000)
    node = lambda: r.choice([0, 1, 2, n - 1, 1, 0, -1]) if r.random() < 0.95 else r.choice(NODES)   # noqa: E731
    return {"from": node(), "to": node(), "ofrom": node(), "oto": node(), "start": start,
            "end": start + r.choice([1, 50, 500, 1000, 1500, 2500, 7, -100]), "peep": r.choice([peep_id(r), peep_id(r), 0, 1]) if r.random() < 0.1 else peep_id(r),
            "dir": r.randint(0, 7), "kind": r.choice([0, 1, 2, 3, 4, 4, 2, 5]) if kind is None else kind,
            "state": r.choice([0, 1, 2, 3, 4, 5, -1, 1, 2, 3, 4]) if state is None else state, "last": r.choice([0, 1, -1, 12345])}


def mk_tables(r, c, nw=None, nb=None, nl=None, now=0, nodes=True):
    n = len(c["recs"])
    for _ in range(r.randint(0, 3) if nw is None else nw):
        c["walks"].append(mk_walk(r, now, n=n))
    for _ in range(r.randint(0, 4) if nb is None else nb):
        free = r.random() < 0.35
        peep = 0 if free else peep_id(r)
        c["bounces"].append((peep, r.choice([r.randrange(-3, 8), r.randrange(-100000, 100000)]), now + r.choice([-1000, -1, 0, 1, 1000, -100000, 100000, 1500]) if r.random() < 0.9 else r.randint(-2 ** 31, 2 ** 31 - 1),
                             now - r.choice(ELAPSED + [r.randint(-5000, 5000)])))
    for _ in range(r.randint(0, 4) if nl is None else nl):
        free = r.random() < 0.35
        c["limbos"].append((0 if free else peep_id(r), now + r.choice([-1000, -1, 0, 1, 1000, -100000, 100000, 1500]) if r.random() < 0.9 else r.randint(-2 ** 31, 2 ** 31 - 1)))


def things_for(c):
    for peep, _node, _rel, _start in c["bounces"]:
        if peep:
            c["things"].add(peep & 0xFFFF)


# ---- GetNodePos
def gn_case(r, node=None, mask=None, ride=None, dirbuf=None):
    ride0 = r.random() < 0.15 if ride is None else ride == 0
    c = base(r, "gn", 0 if ride0 else None if ride is None else ride)
    n = len(c["recs"])
    node = r.choice([-1, 0, 1, n - 1, 99, -2 ** 31, r.randrange(n)]) if node is None else node
    mask = r.choice([0x800, 0x80]) if mask is None else mask
    c["dirbuf"] = r.random() < 0.7 if dirbuf is None else dirbuf
    c["args"] = [node, mask]
    ans = r.choice([r.randrange(n), r.randrange(n), 0xFFFFFFFF])
    c["queues"]["findWalk"] = [ans]
    c["queues"]["findHead"] = [ans]
    return c


def gn_edges():
    r = random.Random(14901)
    for node in (-1, 0, 2, 99):
        for ride in (0, 1):
            for mask in (0x800, 0x80):
                for dirbuf in (False, True):
                    for flip in range(4):
                        c = gn_case(r, node, mask, ride, dirbuf)
                        q = c["queues"]
                        q["findWalk"] = q["findHead"] = [[0, 1, 0xFFFFFFFF, 2][flip]]
                        q["matrix"] = [flip & 1]
                        q["bound"] = [(flip >> 1) & 1]
                        c["recs"][0] = (flip & 0x10 and 0x10 or 0, flip != 3, c["recs"][0][2])
                        c["recs"][1] = (0x10, True, c["recs"][1][2])
                        if flip == 2:
                            q["newIndex"] = [1]
                        yield c


def gn_random(r):
    return gn_case(r)


# ---- WALKNODE_Trigger
def trig_case(r, lossy_ok=True, **kw):
    ride0 = r.random() < 0.1
    c = base(r, "trig", 0 if ride0 else None)
    n = len(c["recs"])
    node = lambda: r.choice([0, 1, 2, n - 1, -1, 7, 0, 1]) if r.random() < 0.9 else r.choice(NODES)   # noqa: E731
    kind = r.choice([0, 1, 2, 3, 4, 4, 5, -1])
    c["args"] = [peep_id(r), node(), node(), node(), node(), kind, r.choice([0, 1, -1, 12345])]
    c["queues"]["clock"] = [r.randint(-100000, 100000), r.randint(-100000, 100000)]
    walks = r.randint(0, 4)
    for _ in range(walks):
        c["walks"].append(mk_walk(r, 0, state=r.choice([0, 0, 1, 2, 3, 4]), n=n))
    if lossy_ok and r.random() < 0.25:
        c["queues"]["findWalk"] = [r.choice([0xFFFFFFFF, r.randrange(n)]) for _ in range(4)]
        c["queues"]["findHead"] = [r.choice([0xFFFFFFFF, r.randrange(n)]) for _ in range(4)]
        c["lossy"] = True
    return c


def trig_octant(r, ang, dist, kind=0):
    c = base(r, "trig", 1, 3)
    p0 = (r.uniform(-20, 20), r.uniform(-5, 5), r.uniform(-20, 20))
    dx, dz = dist * math.cos(ang), dist * math.sin(ang)
    c["recs"][0] = (0, True, [fb(1.0)] * 12 + [fb(p0[0]), fb(p0[1]), fb(p0[2]), fb(1.0)])
    c["recs"][1] = (0, True, [fb(1.0)] * 12 + [fb(p0[0] + dx), fb(p0[1] + r.uniform(-5, 5)), fb(p0[2] + dz), fb(1.0)])
    c["queues"]["findWalk"] = [0, 1]
    c["queues"]["findHead"] = [0, 1]
    c["args"] = [peep_id(r), 0, 1, 2, 0, kind, 3]
    c["queues"]["clock"] = [1000, 2000]
    for _ in range(2):
        c["walks"].append(mk_walk(r, 0, state=r.choice([0, 3]), n=3))
    c["walks"][0]["state"] = 0
    return c


def trig_edges():
    r = random.Random(14902)
    for k in range(16):
        for eps in (0.0, 1e-4, -1e-4):
            yield trig_octant(r, k * math.pi / 8 + eps, r.choice([0.0, 0.5, 7.3, 33.0, 2500.0]), r.choice([0, 4, 2]))
    for dx, dz in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, 1), (1, -1), (-1, -1)):
        c = trig_octant(r, 0.0, 1.0)
        c["recs"][1] = (0, True, [fb(1.0)] * 12 + [fb(bitsf(c["recs"][0][2][12]) + dx), c["recs"][0][2][13], fb(bitsf(c["recs"][0][2][14]) + dz), fb(1.0)])
        yield c
    for kind in (0, 1, 2, 3, 4, 5):          # a free slot first, none free, an empty table, the node failures
        for shape in range(6):
            c = trig_case(r, False)
            c["args"][5] = kind
            c["walks"] = [mk_walk(r, 0, state=[0, 1, 1, 2, 0, 3][shape], n=3) for _ in range([3, 3, 2, 1, 0, 4][shape])]
            if shape == 5:
                c["queues"]["findWalk"] = [0xFFFFFFFF, 1, 2]
                c["queues"]["findHead"] = [1, 0xFFFFFFFF]
                c["lossy"] = True
            yield c


def trig_random(r):
    return trig_case(r)


# ---- WALKNODE_Get, Float
def wget_case(r):
    c = base(r, "wget")
    mk_tables(r, c, nw=r.randint(0, 5), nb=0, nl=0)
    return c


def wget_edges():
    r = random.Random(14903)
    for states in ([], [4], [0, 4], [3, 4, 4], [1, 2, 3, 0], [4, 4, 4, 4], [5, -1]):
        c = base(r, "wget")
        c["walks"] = [mk_walk(r, 0, state=s_, n=3) for s_ in states]
        yield c


def wfloat_case(r, vals=None):
    c = base(r, "wfloat")
    c["args"] = list(vals) if vals else [r.choice([0, 1, -1, 5000, 2 ** 31 - 1, -2 ** 31, r.randint(-10 ** 6, 10 ** 6)]), r.choice([0, 1, -1, 0x12345, 0x7FFF, -0x8000]), r.choice([0, 7, -3, 0xFFFF, 0x8000, 0x10001])]
    c["queues"]["clock"] = [r.choice([0, 1, -1, r.randint(-10 ** 6, 10 ** 6), 2 ** 31 - 1, -2 ** 31])]
    mk_tables(r, c, nw=r.randint(0, 2), nb=0, nl=0)
    return c


def wfloat_edges():
    r = random.Random(14904)
    for vals in ((0, 0, 0), (1000, 3, 4), (-1, -1, -1), (123456, 0x12345, 0xFFFF), (2 ** 31 - 1, 0x7FFF, -0x8000)):
        yield wfloat_case(r, vals)


# ---- WALKNODE_Release
def wrel_case(r, state=None, kind=None):
    c = base(r, "wrel")
    n = len(c["recs"])
    now = c["queues"]["clock"][0]
    for _ in range(r.randint(1, 4)):
        w = mk_walk(r, now, n=n)
        for key in ("ofrom", "oto"):
            if r.random() < 0.9:
                w[key] = r.randrange(n)
        c["walks"].append(w)
    pick = r.choice(c["walks"])
    if state is not None:
        pick["state"] = state
    if kind is not None:
        pick["kind"] = kind
    c["queues"]["clock"] = [r.randint(-100000, 100000), r.randint(-100000, 100000)]
    peeps = [w["peep"] for w in c["walks"] if w["state"] != 0]
    c["args"] = [r.choice(peeps) if peeps and r.random() < 0.85 else peep_id(r, 6)]
    c["queues"]["findWalk"] = valid(r, n)
    c["queues"]["findHead"] = [r.randrange(n), r.choice([r.randrange(n), 0xFFFFFFFF])]
    return c


def wrel_edges():
    r = random.Random(14905)
    for state in (0, 1, 2, 3, 4):
        for kind in (0, 1, 2, 3, 4, 5):
            for _ in range(2):
                c = wrel_case(r, state, kind)
                c["walks"][0]["peep"] = peep_id(r)
                c["walks"][0]["state"] = state
                c["walks"][0]["kind"] = kind
                c["args"] = [c["walks"][0]["peep"]]
                if _ == 1:
                    c["queues"]["findHead"] = [1, 0xFFFFFFFF]
                yield c
    for _ in range(4):                      # nothing to release
        c = wrel_case(r)
        c["args"] = [peep_id(r, 6)]
        for w in c["walks"]:
            w["state"] = 0
        yield c


def wrel_random(r):
    return wrel_case(r)


# ---- WALKNODE_Tick
def wtick_case(r, **kw):
    c = base(r, "wtick")
    n = len(c["recs"])
    now = r.randint(-100000, 100000)
    c["queues"]["clock"] = [now + r.randint(-20, 20) for _ in range(12)]
    for _ in range(r.randint(1, 4)):
        w = mk_walk(r, now, n=n)
        for key in ("from", "to", "ofrom", "oto"):
            if r.random() < 0.95:
                w[key] = r.randrange(n)
        c["walks"].append(w)
    if r.random() < 0.5:
        c["sc"]["floatStart"] = now - r.randint(0, 2000)
        c["sc"]["floatDur"] = r.choice([0, 1, 500, 1000, 5000, -1])
    c["queues"]["findWalk"] = valid(r, n, 16)
    c["queues"]["findHead"] = [r.choice([r.randrange(n), 0xFFFFFFFF]) for _ in range(6)]
    return c


def wtick_edges():
    r = random.Random(14906)
    for state in (1, 2, 3, 4, 0, 5):
        for kind in (0, 1, 2, 3, 4):
            for d, length in ((0, 1000), (999, 1000), (1000, 1000), (1001, 1000), (-1, 1000), (5000000, 1000), (3, 7)):
                if state in (2, 4, 0, 5) and (d, length) not in ((0, 1000), (999, 1000)):
                    continue
                c = wtick_case(r)
                now = 50000
                c["queues"]["clock"] = [now]
                w = mk_walk(r, now, state=state, kind=kind, n=len(c["recs"]))
                w["start"] = now - d
                w["end"] = w["start"] + length
                w["from"], w["to"], w["ofrom"], w["oto"] = 0, 1, 2, 1
                c["walks"] = [w]
                if state == 1 and kind == 4 and d == 999:
                    c["queues"]["findHead"] = [0xFFFFFFFF]
                yield c
    for start, dur, now in ((100, 50, 149), (100, 50, 150), (100, 50, 151), (100, 0, 99999), (-2 ** 31, -1, 5), (2 ** 31 - 1, 2, -2 ** 31), (7, -5, 8)):
        c = wtick_case(r)
        c["walks"] = [mk_walk(r, now, state=0, n=3)]
        c["queues"]["clock"] = [now]
        c["sc"]["floatStart"], c["sc"]["floatDur"] = start, dur
        yield c
    c = wtick_case(r)                         # no walk records at all: the float time is not looked at either
    c["walks"] = []
    c["sc"]["floatStart"], c["sc"]["floatDur"] = 0, 1
    yield c
    for k in range(3):
        c = wtick_case(r)
        c["walks"][0]["peep"] = 0
        c["walks"][0]["state"] = [1, 2, 3][k]
        yield c


def wtick_random(r):
    return wtick_case(r)


# ---- BOUNCE_Put, LIMBO_Put
def put_case(r, op, count=None, free=None, time=None):
    c = base(r, op)
    now = r.randint(-100000, 100000)
    c["queues"]["clock"] = [now, now + r.choice([0, 5, 1000])]
    n = r.randint(0, 4) if count is None else count
    for i in range(n):
        busy = (r.random() < 0.5) if free is None else (i != free)
        entry = (peep_id(r) if busy else 0)
        if op == "bput":
            c["bounces"].append((entry, r.randint(-9, 9), r.randint(-1000, 1000), r.randint(-1000, 1000)))
        else:
            c["limbos"].append((entry, r.randint(-1000, 1000)))
    c["args"] = [r.choice([peep_id(r), 0, 1, 0xFFFFFFFF]), r.choice([0, 1, 3, -1, 5000, 2 ** 31 - 1, -2 ** 31, 4294967, r.randint(-10 ** 5, 10 ** 5)]) if time is None else time]
    return c


def put_edges(op):
    r = random.Random(14907 if op == "bput" else 14908)
    for count in (0, 1, 3):
        for free in (None, 0, 1, 2, 7):
            yield put_case(r, op, count, free)
    for time in (0, 1, -1, 4294967, 2 ** 31 - 1, -2 ** 31):
        yield put_case(r, op, 2, 1, time)


# ---- BOUNCE_Get, BOUNCE_ForceGet, LIMBO_Get, LIMBO_ForceGet
def get_case(r, op, elapsed=None, rel_off=None):
    c = base(r, op)
    a = r.randint(-100000, 100000)
    b = a + r.choice([0, 3, 1000, -1000])
    c["queues"]["clock"] = [a, b]
    nb = r.randint(0, 4)
    for _ in range(nb):
        free = r.random() < 0.3
        peep = 0 if free else peep_id(r)
        e = r.choice(ELAPSED) if elapsed is None else elapsed
        rel = a + (r.choice([-1000, -1, 0, 1, 1000, -100000, 100000]) if rel_off is None else rel_off)
        if op in ("bget", "bforce"):
            c["bounces"].append((peep, r.randint(-3, 9), rel, b - e))
        else:
            c["limbos"].append((peep, rel))
    c["sc"]["bounceUsed"] = r.randint(-2, 5)
    return c


def get_edges(op):
    r = random.Random(14909 + len(op))
    elapsed = ELAPSED if op in ("bget", "bforce") else [0]
    rels = (-1000, -1, 0, 1, 1000) if op in ("bget", "lget") else [0]
    for e in elapsed:
        for rel in rels:
            c = get_case(r, op, e, rel)
            c["bounces"] = [(peep_id(r) if i == len(c["bounces"]) - 1 else 0, nd, rl, st) for i, (_p, nd, rl, st) in enumerate(c["bounces"])] if c["bounces"] else c["bounces"]
            yield c
    for n in (0, 1, 3):
        c = get_case(r, op)
        c["bounces"], c["limbos"] = c["bounces"][:n], c["limbos"][:n]
        yield c


# ---- BOUNCE_Tick
def btick_case(r, still=None, speed=None, elapsed=None, height=None):
    c = base(r, "btick")
    n = len(c["recs"])
    b = r.randint(-100000, 100000)
    c["queues"]["clock"] = [b]
    for _ in range(r.randint(1, 4)):
        free = r.random() < 0.2
        e = r.choice(ELAPSED + [r.randint(-5000, 5000)]) if elapsed is None else elapsed
        c["bounces"].append((0 if free else peep_id(r), r.choice([r.randint(-3, 9), r.randint(-100000, 100000)]), 0, b - e))
    if still is not None:
        c["sc"]["vars7"] = still
    if speed is not None:
        c["sc"]["speed"] = speed
    if height is not None:
        c["sc"]["height"] = height
    things_for(c)
    if r.random() < 0.1:
        c["things"].add(0)
    c["queues"]["findWalk"] = [r.choice([r.randrange(n), r.randrange(n), 0xFFFFFFFF, 0x80000005, 0x7FFFFFFF - 0x7FFFFFF0 if False else r.randrange(n)]) for _ in range(6)]
    return c


def btick_edges():
    r = random.Random(14910)
    for still in (0, 1):
        for speed in (0, 100, -50, 200, -200, 32767):
            for elapsed in (0, 199, 999, 1000, 4321, -1, -777):
                c = btick_case(r, still, speed, elapsed, r.choice([0, 25, -9]))
                c["queues"]["findWalk"] = [r.randrange(len(c["recs"]))] * 6
                yield c
    for _ in range(6):                        # the root height and the node's height decide which is higher
        c = btick_case(r, 0, 100, r.choice([0, 250, 500]))
        c["queues"]["findWalk"] = [r.randrange(len(c["recs"]))] * 6
        yield c
    for peep in (0x10003, 0x7FFF0006, 0x80000007, 0xFFFF0001, 7, 14, 6):
        c = btick_case(r, r.choice([0, 1]), 100, 250)
        c["bounces"] = [(peep, 1, 0, c["bounces"][0][3])]
        c["things"] = {peep & 0xFFFF}
        c["queues"]["findWalk"] = [0] * 6
        yield c
    c = btick_case(r)                         # an object in row 0
    c["things"].add(0)
    yield c
    c = btick_case(r)
    c["bounces"] = []
    yield c


def btick_random(r):
    return btick_case(r)


def mk(name, addr, edges, rnd):
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [
    mk("GetNodePos", FN_GN, gn_edges, gn_random),
    mk("WALKNODE_Trigger", FN_TRIG, trig_edges, trig_random),
    mk("WALKNODE_Get", FN_WGET, wget_edges, wget_case),
    mk("WALKNODE_Float", FN_WFLOAT, wfloat_edges, wfloat_case),
    mk("WALKNODE_Release", FN_WREL, wrel_edges, wrel_random),
    mk("WALKNODE_Tick", FN_TICK, wtick_edges, wtick_random),
    mk("BOUNCE_Put", FN_BPUT, lambda: put_edges("bput"), lambda r: put_case(r, "bput")),
    mk("BOUNCE_Get", FN_BGET, lambda: get_edges("bget"), lambda r: get_case(r, "bget")),
    mk("BOUNCE_ForceGet", FN_BFORCE, lambda: get_edges("bforce"), lambda r: get_case(r, "bforce")),
    mk("BOUNCE_Tick", FN_BTICK, btick_edges, btick_random),
    mk("LIMBO_Put", FN_LPUT, lambda: put_edges("lput"), lambda r: put_case(r, "lput")),
    mk("LIMBO_Get", FN_LGET, lambda: get_edges("lget"), lambda r: get_case(r, "lget")),
    mk("LIMBO_ForceGet", FN_LFORCE, lambda: get_edges("lforce"), lambda r: get_case(r, "lforce")),
]
