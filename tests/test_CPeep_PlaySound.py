"""Unit v5281, CPeep::PlaySound, IsInsidePark, Teleport, UpdateTurn, IsStranded, SetStranded and the two SetDest, against
libtpw_unit_CPeep_PlaySound.dylib (tests/unit_CPeep_PlaySound.cpp), main app ("SimTheme Park"),
src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound.cpp:
  PlaySound 0x10115b80, IsInsidePark 0x10115a80, Teleport 0x101159d0, UpdateTurn 0x101158c8, IsStranded 0x1011566c,
  SetStranded 0x101155bc, SetDest(MapPos, MapPos) 0x101154f0, SetDest(cell) 0x10115400

A case is a peep block (random bytes, typed fields written over them) and a call. The callees outside the unit (script position, sound,
the map and its cell tests, the Pilot, CThing::Move, the map time stamp, IsLocalAreaUnchangedSinceBecomingStranded, GetThingId, CTRACE,
GAME_ASSERTMSG) are fakes on both sides that record their arguments (pointers as offsets from the peep) and answer from the case's queues.

What is compared: the answer, the event log, every byte of the peep block (the fields the code writes typed, the rest as bytes). Fused
operations: UpdateTurn fmadds and PlaySound fctiwz run under HardwareRounding. Not tested: the SetDest answer beyond zero/non-zero (the
original answers what the void Pilot::new_target leaves in r3, which the header does not model); NaN scripts positions (fctiwz of NaN
differs in the emulator).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CPeep_PlaySound"
MASK = 0xFFFFFFFF

PLAY, INSIDE, TELEPORT, TURN, ISSTR, SETSTR, SETDEST, SETDEST_CELL = (
    0x10115B80, 0x10115A80, 0x101159D0, 0x101158C8, 0x1011566C, 0x101155BC, 0x101154F0, 0x10115400)
SIZES = {PLAY: 148, INSIDE: 212, TELEPORT: 124, TURN: 224, ISSTR: 104, SETSTR: 136, SETDEST: 152, SETDEST_CELL: 160}
OWN = set(SIZES)

ADDR = dict(gWorld=0x1037EC0C, nullcell=0x102A0E94)
HOST_LAYOUT = [2]
PEEP_SIZE = 0x1C0
MAPBASE = 0x10000000
BASE = [0]
# typed fields of the peep the functions touch: (offset, size)
FIELDS = [(0xC, 4), (0x18, 4), (0xC0, 6), (0xC8, 4), (0x190, 12)]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(a):
    return (a - BASE[0]) & 0xFFFFFFFFFFFFFFFF if a >= BASE[0] else a - BASE[0]


def mk(addr, name, args, retkey=None):
    def fn(e):
        L = e.cl
        L.rec(name, tuple(args(e)))
        if retkey:
            e.set_r(3, L.ret(retkey) & MASK)
    STUBS.internal(addr, fn)


def wr_word(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def stub_scr(e):
    L = e.cl
    L.rec("SCR_GetScriptPosition", (s32(e.r(3)),))
    wr_word(e, e.r(4), L.ret("scrx"))
    wr_word(e, e.r(5), L.ret("scry"))
    wr_word(e, e.r(6), L.ret("scrz"))


STUBS.internal(0x100D93B8, stub_scr)
mk(0x100DD9FC, "SOUND_SendEvent", lambda e: [s32(e.r(i)) for i in range(3, 9)], "SOUND_SendEvent")
mk(0x10142098, "GetMap", lambda e: [off(e.r(3))])
STUBS.internal(0x10142098, lambda e: (e.cl.rec("GetMap", (off(e.r(3)),)), e.set_r(3, MAPBASE))[0])
for _n, _a in (("IsPath", 0x1009C44C), ("IsQueue", 0x1009C490), ("IsEntrance", 0x1009C4DC), ("IsExit", 0x1009C524), ("IsGrass", 0x1009C660)):
    mk(_a, _n, lambda e: [(e.r(3) - MAPBASE) // 0x44], _n)
mk(0x101347B0, "reset", lambda e: [off(e.r(3))] + [s32(e.r(i)) for i in range(4, 8)])


def stub_get_pos(e):
    L = e.cl
    L.rec("get_pos", (off(e.r(3)), off(e.r(4)), off(e.r(5))))
    wr_word(e, e.r(4), L.ret("gpx"))
    wr_word(e, e.r(5), L.ret("gpy"))


STUBS.internal(0x10134888, stub_get_pos)
mk(0x101347EC, "new_target", lambda e: [off(e.r(3)), s32(e.r(4)), s32(e.r(5))])
mk(0x101349CC, "set_speed", lambda e: [off(e.r(3)), canon_f(fb(e.f(1)))])
mk(0x100FACC0, "ProcessSpriteScripts", lambda e: [off(e.r(3))])
mk(0x1012E5B4, "Move", lambda e: [off(e.r(3)), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF])
mk(0x101025C4, "MakeNewTimeStamp", lambda e: [e.r(3) - e.u32(ADDR["gWorld"])], "MakeNewTimeStamp")
mk(0x101156FC, "IsLocal", lambda e: [off(e.r(3))], "IsLocal")


def stub_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (off(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetThingId") & 0xFFFF))


STUBS.internal(0x1012E1F8, stub_thing_id)
STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "usuu"))
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "bs"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

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

    def log(self):
        return self.L.log

    def call(self, op, peep, a, b):
        entry = {"play": PLAY, "inside": INSIDE, "teleport": TELEPORT, "turn": TURN, "isstr": ISSTR, "setstr": SETSTR,
                 "setdest": SETDEST, "setdest_cell": SETDEST_CELL}[op]
        args = [peep] + ([] if op in ("inside", "turn", "isstr") else [a & MASK] if op in ("setstr", "setdest_cell") else [a & MASK, b & MASK])
        if op == "play":
            args = [peep, a, b]
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, peep, a, b):
        L = self.lib
        if op == "play":
            L.tpw_cp_play(peep, s32(a), s32(b))
            return 0
        if op == "inside":
            return L.tpw_cp_inside(peep) & MASK
        if op == "teleport":
            L.tpw_cp_teleport(peep, a & MASK, b & MASK)
            return 0
        if op == "turn":
            L.tpw_cp_turn(peep)
            return 0
        if op == "isstr":
            return L.tpw_cp_isstranded(peep) & MASK
        if op == "setstr":
            L.tpw_cp_setstranded(peep, s32(a))
            return 0
        if op == "setdest":
            return L.tpw_cp_setdest_pos(peep, a & MASK, b & MASK) & MASK
        return L.tpw_cp_setdest_cell(peep, a & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cp_addr.restype, lib.tpw_cp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cp_alloc.restype, lib.tpw_cp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cp_play.argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_cp_inside.restype, lib.tpw_cp_inside.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_cp_teleport.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_cp_turn.argtypes = [C.c_uint64]
    lib.tpw_cp_isstranded.restype, lib.tpw_cp_isstranded.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_cp_setstranded.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_cp_setdest_pos.restype, lib.tpw_cp_setdest_pos.argtypes = C.c_int32, [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_cp_setdest_cell.restype, lib.tpw_cp_setdest_cell.argtypes = C.c_int32, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    world = s.alloc(0x400)
    s.words(s.g("gWorld"), [world])
    s.half(s.g("nullcell"), case["null"])
    peep = s.alloc(PEEP_SIZE)
    BASE[0] = peep
    s.write(peep, case["fill"])
    s.write(peep + 4, bytes([case["cx"]]))
    s.write(peep + 6, bytes([case["cy"]]))
    s.words(peep + 0xC, [case["script"]])
    s.half(peep + 0x18, case["d0"])
    s.half(peep + 0x1A, case["d1"])
    s.half(peep + 0xC0, case["h0"])
    s.half(peep + 0xC2, case["h2"])
    s.half(peep + 0xC4, case["h4"])
    s.words(peep + 0xC8, [case["speed"]])
    s.words(peep + 0x190, [case["p0"], case["p1"], case["stamp"]])
    return peep


def observe(s, peep, answer, case):
    block = bytearray(s.read(peep, PEEP_SIZE))
    for o, n in FIELDS:
        for i in range(n):
            block[o + i] = 0
    f = lambda a, n: [s.rd_word(a + 4 * i) for i in range(n)]
    d = struct.unpack(s.fmt[0] + "HH", s.read(peep + 0x18, 4))
    h = struct.unpack(s.fmt[0] + "HHH", s.read(peep + 0xC0, 6))
    if case["op"] in ("setdest", "setdest_cell"):
        answer = 1 if answer else 0
    if case["op"] in ("play", "teleport", "turn", "setstr"):
        answer = 0      # void: r3 is whatever the last callee left
    return {"answer": answer, "log": s.log(), "block": bytes(block), "dest": d, "turns": h, "speed": canon_f(s.rd_word(peep + 0xC8)),
            "script": s.rd_word(peep + 0xC), "pos": f(peep + 0x190, 3)}


def run(s, case):
    try:
        peep = build(s, case)
        ans = s.call(case["op"], peep, case["a"], case["b"])
        return observe(s, peep, ans, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

STAMPS = lambda r, now: r.choice([0, 1, now, now - 1 if now else 5, now + 1, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 100)]) & MASK


def rpos(r):
    return r.choice([0, 0xFF, 0x80, 0x7F, 1, r.randrange(256)]), r.choice([0, 0x80, 0xFF, r.randrange(256)])


def word_of(r):
    c, o = rpos(r)
    return (c << 24) | (o << 16) | r.getrandbits(16)


def rfloat(r):
    k = r.random()
    if k < 0.2:
        return fb(r.choice([0.0, -0.0, 0.5, -0.5, 0.99, -0.99, 1.0, -1.0, 1.5, -1.5, 2147483520.0, -2147483648.0, 100.0, 1e-30]))
    if k < 0.7:
        return fb(r.uniform(-500, 500))
    if k < 0.9:
        return fb(r.uniform(-2.1e9, 2.1e9))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def base(r, op):
    now = r.choice([0, 5, 100, 0xFFFFFFFF, r.getrandbits(32)]) & MASK
    return {"op": op, "a": 0, "b": 0, "fill": bytes(r.getrandbits(8) for _ in range(PEEP_SIZE)),
            "null": r.choice([0, 0, 1, 0xFFFF, r.getrandbits(16)]),
            "cx": r.randrange(256), "cy": r.randrange(256), "script": r.getrandbits(32), "d0": r.getrandbits(16), "d1": r.getrandbits(16),
            "h0": r.choice([0, 65535, r.getrandbits(16), r.randrange(300)]), "h2": r.choice([0, 65535, r.getrandbits(16), r.randrange(300)]),
            "h4": r.choice([0, 1, 99, 100, 65535, r.getrandbits(16), r.randrange(300)]),
            "speed": rfloat(r), "p0": r.getrandbits(32), "p1": r.getrandbits(32), "stamp": STAMPS(r, now),
            "queues": {"scrx": [rfloat(r)], "scry": [rfloat(r)], "scrz": [rfloat(r)], "SOUND_SendEvent": [r.getrandbits(32)],
                       "IsPath": [0], "IsQueue": [0], "IsEntrance": [0], "IsExit": [0], "IsGrass": [0],
                       "gpx": [r.getrandbits(32)], "gpy": [r.getrandbits(32)], "MakeNewTimeStamp": [now],
                       "IsLocal": [r.choice([0, 1])], "GetThingId": [r.getrandbits(16)]}}


def case_for(r, op, edge=None):
    c = base(r, op)
    if op == "play":
        c["a"], c["b"] = r.choice([0, 1, -1, r.getrandbits(32)]) & MASK, r.choice([0, 2, -5, r.getrandbits(32)]) & MASK
    elif op == "inside":
        k = r.randrange(6)
        for i, n in enumerate(("IsPath", "IsQueue", "IsEntrance", "IsExit", "IsGrass")):
            c["queues"][n] = [1 if i == k else 0]
        if edge is not None:
            c["cx"], c["cy"] = edge
    elif op in ("teleport", "setdest"):
        c["a"], c["b"] = word_of(r), word_of(r)
    elif op == "setstr":
        c["a"] = r.choice([0, 1, -1, 5, 0x100]) & MASK
    elif op == "setdest_cell":
        i = r.choice([0, 1, 2, 127, 128, 129, 0xFFFF, c["null"], r.getrandbits(16)])
        c["a"] = (i << 16) | r.getrandbits(16)
    if op in ("setdest", "setdest_cell", "isstr", "setstr") and r.random() < 0.7:
        now = c["queues"]["MakeNewTimeStamp"][0]
        c["stamp"] = r.choice([0, 1, now, (now - 1) & MASK, (now + 1) & MASK, 0xFFFFFFFF])
    return c


def gen(op, seed, n_edge):
    def edges():
        r = random.Random(seed)
        for i in range(n_edge):
            yield case_for(r, op, [(0, 0), (255, 255), (127, 0), (0, 1), (1, 0), (255, 0)][i % 6] if op == "inside" else None)

    def rnd(r):
        return case_for(r, op)
    return edges, rnd


TESTS = []
for _name, _op, _seed in (("CPeep::PlaySound", "play", 1), ("CPeep::IsInsidePark", "inside", 2), ("CPeep::Teleport", "teleport", 3),
                          ("CPeep::UpdateTurn", "turn", 4), ("CPeep::IsStranded", "isstr", 5), ("CPeep::SetStranded", "setstr", 6),
                          ("CPeep::SetDest", "setdest", 7), ("CPeep::SetDest(cell)", "setdest_cell", 8)):
    _e, _r = gen(_op, _seed, 40)
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
