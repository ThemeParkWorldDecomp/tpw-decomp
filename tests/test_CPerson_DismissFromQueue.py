"""Unit v5150, a person in a ride's queue, against libtpw_unit_CPerson_DismissFromQueue.dylib (tests/unit_CPerson_DismissFromQueue.cpp), main app
("SimTheme Park"), src/game/people/PersonQueue.cpp:
  CPerson::DismissFromQueue 0x1011f1b8, EvaluateQueueEdit 0x1011f2f8, FindQueueDestination 0x1011ef14, ExitRide 0x1011f5a4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (GetThingId, CTRACE,
GAME_ASSERTMSG, the ride's queue calls, GetStandPoint, the peep's SetDest / Teleport / PlaySound, LowerHappiness, SetState, UseObject, the
thoughts, the map cell calls) is a fake on both sides that records its arguments (people and rides by their thing id, cells by index in the
map, texts by a hash) and answers from the case's queues.

The world of a case: a person block (0x240 random bytes with the fields the code reads set), a ride block, gThingArray row 0 (null or not)
and the ride's row, the world map (a few cells whose first halfword is the next cell id), the sound bank word. What is compared: the answer,
the event log, the person block (the multi-byte fields the test sets compared as values, the rest byte for byte).

No fused operations. Not tested: a ride row that is null (the original dereferences it), a map cell id of 0 handed to a real cell.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_DismissFromQueue"
MASK = 0xFFFFFFFF

FN_DISMISS, FN_EVAL, FN_FIND, FN_EXIT = 0x1011F1B8, 0x1011F2F8, 0x1011EF14, 0x1011F5A4
SIZES = {FN_DISMISS: 272, FN_EVAL: 636, FN_FIND: 368, FN_EXIT: 516}
OWN = set(SIZES)

ADDR = dict(thingarr=0x1034CBFC, map=0x1037EC08, sound=0x102F7C94)
TEXT = {}


def djb(text):
    h = 5381
    for ch in text:
        h = (h * 33 + ch) & MASK
    return h


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u16(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def nfmt(e, a):
    return e.cstr(a).count("%")


def hashed(e, a):
    s = e.read(a, 512).split(b"\0")[0]
    return djb(s)


def cell_index(e, a):
    base = e.u32(ADDR["map"])
    d = (a - base) & MASK
    if d & 0x80000000:
        d -= 1 << 32
    return d // 0x44


def sx(v):
    return v


def s_getthingid(e):
    i = u16(e, e.r(4))
    e.cl.rec("GetThingId", (i,))
    e.write(e.r(3), struct.pack(">H", i))


def s_ctrace(e):
    fmt = e.r(4)
    n = e.read(fmt, 512).split(b"\0")[0].count(b"%")
    args = (e.r(3), hashed(e, fmt), e.r(5), e.r(6))[: 2 + n]
    e.cl.rec("CTRACE", tuple(a & MASK for a in args))


def s_assert(e):
    e.cl.rec("assert", (e.r(3) & MASK, hashed(e, e.r(4))))


def s_script(e):
    e.cl.rec("Script", (e.r(3) & MASK,))


def s_getpos(e):
    e.cl.rec("GetPos", (u16(e, e.r(3)), e.r(4) >> 16))
    e.set_r(3, e.cl.ret("GetPos") & MASK)


def s_remove(e):
    e.cl.rec("Remove", (u16(e, e.r(3)), e.r(4) >> 16))


def s_convq(e):
    L = e.cl
    L.rec("ConvertQ", (u16(e, e.r(3)), e.r(4) & MASK))
    e.write(e.r(5), struct.pack(">H", L.ret("cq_cell") & 0xFFFF))
    e.write(e.r(6), bytes([L.ret("cq_x") & 0xFF]))
    e.write(e.r(7), bytes([L.ret("cq_y") & 0xFF]))
    e.set_r(3, 0)


def s_stand(e):
    L = e.cl
    L.rec("Stand", (u16(e, e.r(3)), e.r(4) & MASK))
    e.write(e.r(5), struct.pack(">H", L.ret("sp1") & 0xFFFF))
    e.write(e.r(6), struct.pack(">H", L.ret("sp2") & 0xFFFF))


def s_blocked(e):
    e.cl.rec("Blocked", (u16(e, e.r(3)),))


def s_play(e):
    e.cl.rec("PlaySound", (u16(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def s_setdestpos(e):
    e.cl.rec("SetDestPos", (u16(e, e.r(3)), e.r(4) >> 16, e.r(5) >> 16))
    e.set_r(3, e.cl.ret("SetDestPos") & MASK)


def s_setdestcell(e):
    e.cl.rec("SetDestCell", (u16(e, e.r(3)), e.r(4) >> 16))
    e.set_r(3, e.cl.ret("SetDestCell") & MASK)


def s_teleport(e):
    e.cl.rec("Teleport", (u16(e, e.r(3)), e.r(4) >> 16, e.r(5) >> 16))


def s_lower(e):
    e.cl.rec("Lower", (u16(e, e.r(3)), e.r(4) & MASK))


def s_setstate(e):
    e.cl.rec("SetState", (u16(e, e.r(3)), e.r(4) & MASK))


def s_use(e):
    e.cl.rec("UseObject", (u16(e, e.r(3)), u16(e, e.r(4))))


def s_record(e):
    e.cl.rec("RecordEvent", (u16(e, e.r(3) - 0x30), e.r(4) & MASK, e.r(5) >> 16))


def s_bubble(e):
    e.cl.rec("Bubble", (u16(e, e.r(3) - 0x30), e.r(4) & MASK, e.r(5) & 0xFF))


def s_dir(e):
    e.cl.rec("GetDirection", (cell_index(e, e.r(3)),))
    e.set_r(3, e.cl.ret("dir") & 0xFF)


def s_opp(e):
    e.cl.rec("OPPOSITE", (e.r(3) & 0xFF,))
    e.set_r(3, e.cl.ret("opp") & 0xFF)


def s_nb(e):
    e.cl.rec("Neighbour", (cell_index(e, e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, (e.u32(ADDR["map"]) + e.cl.ret("nb") * 0x44) & MASK)


def s_isq(e):
    e.cl.rec("IsQueue", (cell_index(e, e.r(3)),))
    e.set_r(3, e.cl.ret("IsQueue") & MASK)


for _a, _f in [(0x1012E1F8, s_getthingid), (0x1006FCBC, s_ctrace), (0x1000DDE8, s_assert), (0x100D93B8, s_script),
               (0x10108F5C, s_getpos), (0x10108C20, s_remove), (0x10109DA8, s_convq), (0x1010A4FC, s_stand),
               (0x1010A93C, s_blocked), (0x10115B80, s_play), (0x101154F0, s_setdestpos), (0x10115400, s_setdestcell),
               (0x101159D0, s_teleport), (0x1011B384, s_lower), (0x10120354, s_setstate), (0x101196A0, s_use),
               (0x1012F3D4, s_record), (0x1012F0D8, s_bubble), (0x10081930, s_dir), (0x10102500, s_opp),
               (0x10103814, s_nb), (0x1009C490, s_isq)]:
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put(self, a, fmt, v):
        self.write(a, struct.pack(self.end + fmt, v & ((1 << (8 * struct.calcsize(fmt))) - 1)))

    def get(self, a, fmt):
        return struct.unpack(self.end + fmt, self.read(a, struct.calcsize(fmt)))[0]


class Ppc(Side):
    end = ">"
    ptr = "I"
    stride = 0x14

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

    def call(self, op, person):
        fn = {"dismiss": FN_DISMISS, "eval": FN_EVAL, "find": FN_FIND, "exit": FN_EXIT}[op]
        r3, _, _ = T.call_ppc(self.e, fn, [person])
        return (r3 & MASK) if op in ("find", "exit") else 0


class Host(Side):
    end = "<"
    ptr = "Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = lib.layout[0]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, person):
        fn = {"dismiss": self.lib.tpw_dq_dismiss, "eval": self.lib.tpw_dq_evaluate, "find": self.lib.tpw_dq_find,
              "exit": self.lib.tpw_dq_exit}[op]
        r = fn(person)
        return (r or 0) & MASK if op in ("find", "exit") else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dq_addr.restype, lib.tpw_dq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dq_alloc.restype, lib.tpw_dq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dq_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("dismiss", "evaluate"):
        f = getattr(lib, "tpw_dq_" + n)
        f.restype, f.argtypes = None, [C.c_uint64]
    for n in ("find", "exit"):
        f = getattr(lib, "tpw_dq_" + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dq_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout[1] == 0 and lib.layout[2] == 4, lib.layout
    lib.bound = True


PERSON = 0x240
MASKED = [(0, 2), (0xC, 4), (0x1DC, 2), (0x220, 4)]


def build(s, c):
    p = s.alloc(PERSON)
    s.write(p, c["filler"])
    s.put(p, "H", c["pid"])
    s.put(p + 0xC, "I", c["handle"])
    s.put(p + 0x1DC, "H", c["rid"])
    s.put(p + 0x220, "I", c["state"])
    ride = s.alloc(0x40)
    s.put(ride, "H", c["rid"])
    s.put(ride + 0x32, "H", c["exitc"])
    s.put(ride + 0x34, "H", c["entryc"])
    s.put(ride + 0x3C, "I", c["length"])
    ta = s.g("thingarr")
    s.put(ta, s.ptr, 0x1000 if c["row0"] else 0)
    s.put(ta + c["rid"] * s.stride, s.ptr, ride)
    m = s.alloc(0x44 * 6)
    for i, v in enumerate(c["cells"]):
        s.put(m + i * 0x44, "H", v)
    s.put(s.g("map"), s.ptr, m)
    s.put(s.g("sound") + 4, "I", c["bank"])
    return p


def observe(s, c, p, answer):
    block = bytearray(s.read(p, PERSON))
    vals = []
    for off, n in MASKED:
        vals.append(s.get(p + off, {2: "H", 4: "I"}[n]))
        block[off:off + n] = bytes(n)
    return {"answer": answer, "log": s.log(), "block": bytes(block), "vals": vals}


def run(s, c):
    try:
        p = build(s, c)
        return observe(s, c, p, s.call(c["op"], p))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, **kw):
    c = {
        "op": op,
        "pid": r.choice([8, 16, 24, 3, 9, 5, 1, 0x1234, r.randrange(1, 0x10000), r.randrange(1, 0x10000)]),
        "rid": r.randrange(1, 9),
        "handle": r.randrange(1 << 32),
        "state": r.choice([0xE, 0xE, 0, 5, 0xB, r.randrange(1 << 32)]),
        "exitc": r.randrange(0, 6),
        "entryc": r.randrange(0, 6),
        "length": r.choice([0, 1, 2, 3, 4, 5]),
        "row0": r.random() < 0.1,
        "cells": [r.randrange(0, 6) for _ in range(6)],
        "bank": r.randrange(1 << 32),
        "filler": bytes(r.randrange(256) for _ in range(PERSON)),
    }
    c["entryc"] = r.choice([c["entryc"], c["exitc"]])
    c["queues"] = {
        "GetPos": [r.choice([0xFFFFFFFF, 0, 1, 3, 4, 7, 8, 15, 16, 17, 100, r.randrange(1 << 32)])],
        "cq_cell": [r.randrange(0, 0x10000)], "cq_x": [r.randrange(256)], "cq_y": [r.randrange(256)],
        "sp1": [r.randrange(0x10000)], "sp2": [r.randrange(0x10000)],
        "dir": [r.choice([1, 2, 4, 8, 0x10, 0x80, r.randrange(256)])], "opp": [r.randrange(256)],
        "nb": [r.randrange(0, 5)], "IsQueue": [r.choice([0, 1])],
        "SetDestPos": [r.choice([0, 1])], "SetDestCell": [r.choice([0, 1])],
    }
    c.update(kw)
    return c


def make_edges(op, seed):
    def edges():
        r = random.Random(seed)
        for pid in (8, 3, 5, 24, 1):
            for pos in (0xFFFFFFFF, 0, 4, 15, 16):
                for state in (0xE, 5):
                    for length in (0, 3, 4):
                        for flag in (0, 1):
                            c = base(r, op, pid=pid, state=state, length=length, row0=bool(flag & (pid == 5)),
                                     entryc=(2 if flag else 3), exitc=2)
                            c["queues"]["GetPos"] = [pos]
                            c["queues"]["SetDestPos"] = [flag]
                            c["queues"]["SetDestCell"] = [flag ^ (pos & 1)]
                            c["queues"]["IsQueue"] = [flag if pid != 24 else 1 - flag]
                            c["cells"] = [0, 1, 2, 3, 4, 5] if pos else [5, 0, 1, 2, 3, 4]
                            yield c
    return edges


def make_rnd(op):
    def rnd(r):
        return base(r, op)
    return rnd


TESTS = [
    FnTest("CPerson::DismissFromQueue", ppc, host, edge=make_edges("dismiss", 11), rnd=make_rnd("dismiss")),
    FnTest("CPerson::EvaluateQueueEdit", ppc, host, edge=make_edges("eval", 12), rnd=make_rnd("eval")),
    FnTest("CPerson::FindQueueDestination", ppc, host, edge=make_edges("find", 13), rnd=make_rnd("find")),
    FnTest("CPerson::ExitRide", ppc, host, edge=make_edges("exit", 14), rnd=make_rnd("exit")),
]
