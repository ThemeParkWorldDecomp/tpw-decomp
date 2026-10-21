"""Unit v5028, the key-table spawn and loan actions, against libtpw_unit_spawn_handyman.dylib (tests/unit_spawn_handyman.cpp), main app
("SimTheme Park"), src/game/keyboard.cpp:
  spawn_person 0x1014dbf4, spawn_five_persons 0x1014dca0, start_winning_sequence 0x1014ddd0, spawn_entertainer 0x1014debc,
  spawn_handyman 0x1014df3c, spawn_bag_o_staff 0x1014e138, apply_for_loan_1 0x1014e248, apply_for_loan_2 0x1014e2e4

A case is a world and a call. The world is built twice by one piece of Python code. The callees (operator new, the six constructors, the
CWorld calls, ApplyForLoan, CTRACE) are fakes on both sides recording their arguments; the answers come from the case's queues.

The world of a case: gIntrface's halfword at +0x24, gGameWorld (a constant fake address, never dereferenced). Compared: the answer,
the event log, the halfword. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_spawn_handyman"
MASK = 0xFFFFFFFF

FUNCS = {
    "spawn_person": 0x1014dbf4, "spawn_five_persons": 0x1014dca0, "start_winning_sequence": 0x1014ddd0,
    "spawn_entertainer": 0x1014debc, "spawn_handyman": 0x1014df3c, "spawn_bag_o_staff": 0x1014e138,
    "apply_for_loan_1": 0x1014e248, "apply_for_loan_2": 0x1014e2e4,
}
SIZES = {0x1014dbf4: 136, 0x1014dca0: 176, 0x1014ddd0: 92, 0x1014debc: 88, 0x1014df3c: 88, 0x1014e138: 232,
         0x1014e248: 116, 0x1014e2e4: 116}
OWN = set(SIZES)

OPNEW = 0x102384b8
ADDR = dict(gIntrface=0x102dc1ac, gGameWorld=0x1037ebfc)
HOST_LAYOUT = [48]      # the host's offset of mF24 (native pointers before it); the Mac's is 0x24
WORLD = 0xAB0000
BANK = 0xAC0000
HOST_NAMES = {"gIntrface": 0, "gGameWorld": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the TOC where the caller reloads it


STUBS.internal(OPNEW, stub_new)
for addr, name in ((0x100fefd0, "CHandyman"), (0x100fb094, "CEntertainer"), (0x100fda68, "CGuard"),
                   (0x1010409c, "CMechanic"), (0x10120e34, "CResearcher"), (0x10116634, "CPerson")):
    STUBS.internal(addr, CL.recorder(name, "Nw"))
STUBS.internal(0x1013bdd0, CL.recorder("IsPlayingWinningAnim", "L", "b"))
STUBS.internal(0x1013be2c, CL.recorder("EndWinningSequence", "L"))
STUBS.internal(0x1013bc68, CL.recorder("StartWinningSequence", "L"))
STUBS.internal(0x1013ffb4, CL.recorder("ParkGates_GetStatus", "L", "i"))
STUBS.internal(0x1013fc3c, CL.recorder("SetParkOpenOrClosed", "Lii"))


def stub_bank(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, BANK)


STUBS.internal(0x1013edfc, stub_bank)
STUBS.internal(0x100f3db8, CL.recorder("ApplyForLoan", "Lu", "b"))
STUBS.internal(0x1006fcbc, CL.recorder("CTRACE", "us"))


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    f24 = 0x24

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WORLD: 1, BANK: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ptr(self, a, v):
        self.words(a, [v])

    def call(self, name):
        r3, _, _ = T.call_ppc(self.e, FUNCS[name], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    f24 = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: WORLD, 2: BANK}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sh_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a, v):
        self.words(a, [v, 0])

    def call(self, name):
        return getattr(self.lib, "tpw_sh_" + name)() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sh_addr.restype, lib.tpw_sh_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sh_alloc.restype, lib.tpw_sh_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sh_layout.argtypes = [C.POINTER(C.c_int64)]
    for name in FUNCS:
        f = getattr(lib, "tpw_sh_" + name)
        f.restype, f.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_sh_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def halfword(s, v):
    s.write(s.g("gIntrface") + s.f24, struct.pack(s.fmt[0] + "H", v & 0xFFFF))


def build(s, case):
    halfword(s, case["cell"])
    s.ptr(s.g("gGameWorld"), WORLD)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "cell": struct.unpack(s.fmt[0] + "H", s.read(s.g("gIntrface") + s.f24, 2))[0]}


def make_run(name):
    def run(s, case):
        try:
            build(s, case)
            return observe(s, case, s.call(name))
        finally:
            s.done()
    return run


def make_case(r, fixed=None):
    q = {
        "ParkGates_GetStatus": [r.choice([1, 1, 0, 2, -1, 0x100, 0x80000000]) & MASK],
        "IsPlayingWinningAnim": [r.choice([0, 1])],
        "ApplyForLoan": [r.choice([0, 1])],
    }
    q.update(fixed or {})
    return {"queues": q, "cell": r.choice([0, 1, 0xFFFF, 0x8000, r.randrange(0x10000), r.randrange(0x10000)])}


def edges_for(seed):
    def gen():
        r = random.Random(seed)
        for st in (1, 0, 2, -1):
            for flag in (0, 1):
                yield make_case(r, {"ParkGates_GetStatus": [st & MASK], "IsPlayingWinningAnim": [flag], "ApplyForLoan": [flag]})
    return gen


def tests():
    out = []
    for i, name in enumerate(FUNCS):
        run = make_run(name)
        out.append(FnTest(name, (lambda run: lambda e, c: run(Ppc(e, c), c))(run),
                          (lambda run: lambda lib, c: run(Host(lib, c), c))(run),
                          edge=edges_for(100 + i), rnd=make_case))
    return out


TESTS = tests()
