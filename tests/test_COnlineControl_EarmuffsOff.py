"""Unit v5121, COnlineControl's command senders and avatar-table accessors, against libtpw_unit_COnlineControl_EarmuffsOff.dylib
(tests/unit_COnlineControl_EarmuffsOff.cpp), main app, src/COnlineControl_..._EarmuffsOff.cpp:
  VoteForThisPark 0x10218b44, EarmuffsOn 0x10219424, EarmuffsOff 0x1021952c, GetPerson 0x1021969c, SetDestination 0x102197ec and 0x10219908,
  Teleport 0x10219a10 and 0x10219af4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both sides
that record their arguments: the string and vector helpers and DoCommand (the argument offsets, the command number and the vector words),
the world's used-list walk, GAME_ASSERTMSG (condition and text) and COnlinePerson's IsThisMyName, SetDest (both) and Teleport.

The world of a case: the COnlineControl block (0xe8c bytes, the earmuffs byte at 0xe50), gGameWorld, gNullThingId, the first eight entries of
the thing table with things of random kinds (entry 0 usually empty, which makes the assert true), and the queues for the walk. What is
compared: the answer (the bool low byte, or which thing GetPerson found), the whole control block, gNullThingId and the event log. The two
SetDestination and the two Teleport are one test each (the case picks the overload); the map Teleport is void, its answer is not compared.

No fused operations. Not tested: a thing index whose entry is empty (the original reads a NULL thing), a head index past the table.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_EarmuffsOff"
MASK = 0xFFFFFFFF

VOTE, ON, OFF, GET = 0x10218B44, 0x10219424, 0x1021952C, 0x1021969C
SETD_MAP, SETD_XY, TELE_MAP, TELE_XY = 0x102197EC, 0x10219908, 0x10219A10, 0x10219AF4
SIZES = {VOTE: 200, ON: 212, OFF: 212, GET: 220, SETD_MAP: 144, SETD_XY: 152, TELE_MAP: 92, TELE_XY: 140}
OWN = set(SIZES)

STRING_CTOR, WSTRING_CTOR, PUSH_BACK, WSTRING_DTOR, STRING_DTOR = 0x1007363C, 0x101E0B84, 0x10201478, 0x101468F8, 0x1000221C
DO_COMMAND, TEAR_DOWN, HEAD, NEXT, ASSERT = 0x10219BEC, 0x10201310, 0x1013ADB0, 0x1013AE08, 0x1000DDE8
IS_MY_NAME, SETDEST_MAP, SETDEST_XY, PERSON_TELEPORT = 0x10213124, 0x102131E0, 0x102132A0, 0x1021334C

ADDR = dict(world=0x1037EBFC, arr=0x1034CBFC, nul=0x102B1D64)
MAC_STRIDE = 0x14
NENT = 8
CTL_SIZE = 0xE8C
HOST = {}          # filled by bind: the host's entry stride


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def thing_id(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def st_string_ctor(e):
    e.cl.lastS = e.r(3)
    e.cl.rec("StringCtor", (0,))


def st_wstring_ctor(e):
    e.cl.rec("WStringCtor", (s32(e.r(3) - e.cl.lastS),))


def st_push_back(e):
    v, a = e.r(3), e.r(4)
    e.cl.rec("PushBack", (s32(a - e.cl.lastS), e.u32(a + 8), e.u32(v), e.u32(v + 4), e.u32(v + 8)))


def st_wdtor(e):
    e.cl.rec("WStringDtor", (s32(e.r(3) - e.cl.lastS), T.s16(e.r(4))))


def st_sdtor(e):
    e.cl.rec("StringDtor", (s32(e.r(3) - e.cl.lastS), T.s16(e.r(4))))


def st_tear_down(e):
    v = e.r(3)
    e.cl.rec("TearDown", (e.u32(v), e.u32(v + 4), e.u32(v + 8)))


def st_do_command(e):
    v = e.r(4)
    e.cl.rec("DoCommand", (int(e.r(3) == e.cl.this), e.u32(v), e.u32(v + 4), e.u32(v + 8)))


def st_head(e):
    e.cl.rec("GetUsedThingHead", (int(e.r(4) == e.u32(ADDR["world"])),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("head") & 0xFFFF))


def st_next(e):
    e.cl.rec("GetNextUsedThing", (int(e.r(4) == e.u32(ADDR["world"])), thing_id(e, e.r(5))))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("next") & 0xFFFF))


def st_is_my_name(e):
    e.cl.rec("IsThisMyName", (thing_id(e, e.r(3)), int(e.r(4) == e.cl.name)))
    e.set_r(3, e.cl.ret("mine") & 0xFF)


def st_setdest_map(e):
    e.cl.rec("SetDestMap", (thing_id(e, e.r(3)), e.r(4) >> 16))
    e.set_r(3, e.cl.ret("setdest") & MASK)


def mp(r):
    return (r >> 24) & 0xFF, (r >> 16) & 0xFF


def st_setdest_xy(e):
    e.cl.rec("SetDestXY", (thing_id(e, e.r(3)),) + mp(e.r(4)) + mp(e.r(5)))
    e.set_r(3, e.cl.ret("setdest") & MASK)


def st_person_teleport(e):
    e.cl.rec("PersonTeleport", (thing_id(e, e.r(3)),) + mp(e.r(4)) + mp(e.r(5)))


STUBS.internal(STRING_CTOR, st_string_ctor)
STUBS.internal(WSTRING_CTOR, st_wstring_ctor)
STUBS.internal(PUSH_BACK, st_push_back)
STUBS.internal(WSTRING_DTOR, st_wdtor)
STUBS.internal(STRING_DTOR, st_sdtor)
STUBS.internal(TEAR_DOWN, st_tear_down)
STUBS.internal(DO_COMMAND, st_do_command)
STUBS.internal(HEAD, st_head)
STUBS.internal(NEXT, st_next)
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(IS_MY_NAME, st_is_my_name)
STUBS.internal(SETDEST_MAP, st_setdest_map)
STUBS.internal(SETDEST_XY, st_setdest_xy)
STUBS.internal(PERSON_TELEPORT, st_person_teleport)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def h(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_h(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"
    stride = MAC_STRIDE

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = HOST["stride"]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5121_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v5121_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        L = self.lib
        this = args[0]
        if entry == GET:
            return L.tpw_v5121_get(this, args[1]) & 0xFFFFFFFFFFFFFFFF
        if entry in (VOTE, ON, OFF):
            {VOTE: L.tpw_v5121_vote, ON: L.tpw_v5121_on, OFF: L.tpw_v5121_off}[entry](this)
            return 0
        thing = args[1] >> 16
        if entry in (SETD_MAP, TELE_MAP):
            cell = args[2] >> 16
            if entry == SETD_MAP:
                return L.tpw_v5121_setdest_map(this, thing, cell)
            L.tpw_v5121_tele_map(this, thing, cell)
            return 0
        xc, xo = mp(args[2])
        yc, yo = mp(args[3])
        f = L.tpw_v5121_setdest_xy if entry == SETD_XY else L.tpw_v5121_tele_xy
        return f(this, thing, xc, xo, yc, yo)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5121_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("vote", "on", "off"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = None, [C.c_uint64]
    getattr(lib, p + "get").restype, getattr(lib, p + "get").argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    getattr(lib, p + "setdest_map").restype = C.c_uint32
    getattr(lib, p + "setdest_map").argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    getattr(lib, p + "tele_map").restype = None
    getattr(lib, p + "tele_map").argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    for n in ("setdest_xy", "tele_xy"):
        getattr(lib, p + n).restype = C.c_uint32
        getattr(lib, p + n).argtypes = [C.c_uint64, C.c_uint32] + [C.c_uint32] * 4
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    got = list(out)[:n]
    # entry size, mThing offset, pointer size, CThing mThingId and mKind offsets, control size, MapPos size
    assert got[1:] == [0, 8, 0, 2, CTL_SIZE, 2], got
    HOST["stride"] = got[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.this = s.alloc(CTL_SIZE)
    s.name = s.alloc(16)
    s.write(s.this + 0xE50, bytes([case["flag"]]))
    world = s.alloc(16)
    s.ptr(s.g("world"), world)
    s.h(s.g("nul"), case["nul"])
    s.things = {}
    for i in range(NENT):
        kind = case["kinds"][i]
        blk = 0
        if kind is not None:
            blk = s.alloc(16)
            s.h(blk, i)
            s.write(blk + 2, bytes([kind]))
            s.things[blk] = i
        s.ptr(s.g("arr") + i * s.stride, blk)
    if isinstance(s, Ppc):
        s.L.this = s.this
        s.L.name = s.name
        s.L.lastS = 0


def execute(s, case):
    op, a = case["op"], case["args"]
    if op in ("vote", "on", "off", "get"):
        entry = {"vote": VOTE, "on": ON, "off": OFF, "get": GET}[op]
        args = [s.this] + ([s.name] if op == "get" else [])
    else:
        entry = {"setd_map": SETD_MAP, "setd_xy": SETD_XY, "tele_map": TELE_MAP, "tele_xy": TELE_XY}[op]
        thing = case["thing"] << 16
        if op in ("setd_map", "tele_map"):
            args = [s.this, thing, case["cell"] << 16]
        else:
            (xc, xo), (yc, yo) = case["x"], case["y"]
            args = [s.this, thing, (xc << 24) | (xo << 16), (yc << 24) | (yo << 16)]
    r = s.call(entry, args)
    if op == "get":
        return s.things.get(r, -1) if r else -2
    if op == "tele_map" or op in ("vote", "on", "off"):
        return 0
    return r & 0xFF


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "ctl": s.read(s.this, CTL_SIZE), "nul": s.rd_h(s.g("nul"))}


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

KINDS = [0x12, 0x12, 0x12, 0x12, 3, 1, 0x13, 0x11, 0x14, 0x00]


def world(r, op, **kw):
    kinds = [r.choice(KINDS + [None]) for _ in range(NENT)]
    kinds[0] = r.choice(KINDS) if r.random() < 0.2 else None
    kinds[1] = r.choice(KINDS)
    kinds[2] = 0x12 if r.random() < 0.5 else kinds[2]
    nul = r.choice([0xFFFF, 9, 0x2800, 0])
    present = [i for i in range(NENT) if kinds[i] is not None]
    c = {"op": op, "kinds": kinds, "nul": nul, "flag": r.choice([0, 1, 2, 0x80, r.randrange(256)])}
    path = r.sample(present, r.randrange(0, min(5, len(present)) + 1))
    c["queues"] = {
        "head": [path[0] if path else nul],
        "next": path[1:] + [nul],
        "mine": [r.choice([0, 1]) for _ in range(r.randrange(1, 6))],
        "setdest": [r.choice([0, 1, 2, -1, 0x100, 5])],
    }
    c["thing"] = r.choice(present)
    c["cell"] = r.choice([0, 1, 2, 127, 128, 129, 255, 256, 0x4000, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0x10000)])
    c["x"] = (r.randrange(256), r.choice([0, 0x80, r.randrange(256)]))
    c["y"] = (r.randrange(256), r.choice([0, 0x80, r.randrange(256)]))
    c.update(kw)
    c["args"] = None
    return c


def of_kind(c, kind):
    """Make the case's chosen thing the given kind."""
    c["kinds"][c["thing"]] = kind
    return c


def op_edges(ops, seed):
    def gen():
        r = random.Random(seed)
        for op in ops:
            for kind in (0x12, 3, 0x11, 0x13, 0x00):
                for sd in (0, 1, 5, -1):
                    c = of_kind(world(r, op), kind)
                    c["queues"]["setdest"] = [sd]
                    yield c
            for cell in (0, 1, 2, 127, 128, 129, 255, 256, 0x7FFF, 0x8000, 0xFFFF):
                c = of_kind(world(r, op, cell=cell), 0x12)
                yield c
            for t in range(NENT):         # entry 0 non-empty makes the assert false
                c = world(r, op)
                if c["kinds"][t] is None:
                    c["kinds"][t] = 0x12
                c["thing"] = t
                yield c
    return gen


def op_random(ops, seed):
    def gen(r):
        c = world(r, r.choice(ops))
        if r.random() < 0.6:
            of_kind(c, 0x12)
        return c
    return gen


def flag_cases(op):
    def gen():
        r = random.Random(77)
        for flag in (0, 1, 2, 0x80, 0xFF):
            yield world(r, op, flag=flag)
    return gen


def flag_random(op):
    return lambda r: world(r, op)


def get_edges():
    r = random.Random(31)
    for nul in (0xFFFF, 0):
        c = world(r, "get", nul=nul)                   # empty list
        c["queues"].update(head=[nul], next=[nul])
        yield c
    for n_mine in range(3):                            # the match is the n-th person
        c = world(r, "get")
        c["kinds"] = [None, 0x12, 0x12, 0x12, 3, 0x13, None, None]
        c["nul"] = 0xFFFF
        c["queues"].update(head=[1], next=[2, 3, 4, 5, 0xFFFF], mine=[0] * n_mine + [1])
        yield c
    c = world(r, "get")                                # persons, none of them mine
    c["kinds"] = [None, 0x12, 0x12, 3, None, None, None, None]
    c["nul"] = 9
    c["queues"].update(head=[1], next=[2, 3, 9], mine=[0])
    yield c
    c = world(r, "get")                                # no person at all
    c["kinds"] = [None, 3, 1, 0x11, None, None, None, None]
    c["nul"] = 9
    c["queues"].update(head=[1], next=[2, 3, 9], mine=[1])
    yield c
    c = world(r, "get")                                # entry 0 holds a thing: the assert fails each step
    c["kinds"] = [0x12, 0x12, None, None, None, None, None, None]
    c["nul"] = 0xFFFF
    c["queues"].update(head=[0], next=[1, 0xFFFF], mine=[0, 1])
    yield c
    for _ in range(10):
        yield world(r, "get")


TESTS = [
    FnTest("VoteForThisPark", ppc, host, edge=flag_cases("vote"), rnd=flag_random("vote")),
    FnTest("EarmuffsOn", ppc, host, edge=flag_cases("on"), rnd=flag_random("on")),
    FnTest("EarmuffsOff", ppc, host, edge=flag_cases("off"), rnd=flag_random("off")),
    FnTest("GetPerson", ppc, host, edge=get_edges, rnd=flag_random("get")),
    FnTest("SetDestination", ppc, host, edge=op_edges(["setd_map", "setd_xy"], 5), rnd=op_random(["setd_map", "setd_xy"], 5)),
    FnTest("Teleport", ppc, host, edge=op_edges(["tele_map", "tele_xy"], 6), rnd=op_random(["tele_map", "tele_xy"], 6)),
]
