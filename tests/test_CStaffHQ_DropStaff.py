"""Unit v5037, CStaffHQ::DropStaff and CStaffHQ::ForceStrike, against libtpw_unit_CStaffHQ_DropStaff.dylib
(tests/unit_CStaffHQ_DropStaff.cpp), main app, src/game/buildings/StaffHQ.cpp:
  DropStaff 0x1012c168, ForceStrike 0x1012c04c

A case is a headquarters object (0x68 bytes) plus gThingArray rows. GAME_ASSERTMSG, CTRACE and CStaff::DropMe are fakes on both
sides recording their arguments (the strings included). Compared: the whole object, the event log. No fused operations.
Not tested: strike types outside 0..4 (the original reads past the object).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CStaffHQ_DropStaff"
MASK = 0xFFFFFFFF

FN_DROP, FN_FORCE = 0x1012C168, 0x1012C04C
SIZES = {FN_DROP: 136, FN_FORCE: 128}
OWN = set(SIZES)

ASSERT, CTRACE, DROPME = 0x1000DDE8, 0x1006FCBC, 0x10125DE4
ADDR = dict(gThingArray=0x1034CBFC)
HOST_LAYOUT = [0x68, 0x24, 0x60, 0x64]
OBJ = [0x70000100 * 0 + 0x70000100 + 0x100 * k for k in range(4)]   # fake thing pointers, never dereferenced; label k+1

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(CTRACE, CL.recorder("CTRACE", "us"))
STUBS.internal(DROPME, CL.recorder("DropMe", "Lw"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({OBJ[k]: k + 1 for k in range(4)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def row(self, i, p):
        self.words(ADDR["gThingArray"] + 0x14 * i, [p])

    def drop(self, hq, cell):
        T.call_ppc(self.e, FN_DROP, [hq, (cell & 0xFFFF) << 16])

    def force(self, hq, t):
        T.call_ppc(self.e, FN_FORCE, [hq, t & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {k + 1: OBJ[k] for k in range(4)}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_hq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def row(self, i, p):
        self.lib.tpw_hq_row(i, p)

    def drop(self, hq, cell):
        self.lib.tpw_hq_drop(hq, cell & 0xFFFF)

    def force(self, hq, t):
        self.lib.tpw_hq_force(hq, T.s32(t))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hq_addr.restype, lib.tpw_hq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hq_alloc.restype, lib.tpw_hq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hq_row.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_hq_drop.argtypes = [C.c_uint64, C.c_uint32]
    lib.tpw_hq_force.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hq_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        hq = s.alloc(0x68)
        s.words(hq + 0x24, case["strikes"])
        s.write(hq + 0x60, struct.pack(s.fmt[0] + "H", case["f60"] & 0xFFFF))
        s.words(hq + 0x64, [case["f64"]])
        for i, p in enumerate(case["rows"]):
            s.row(i, p)
        if case["op"] == "drop":
            s.drop(hq, case["cell"])
        else:
            s.force(hq, case["type"])
        obj = s.rd_words(hq, 0x60 // 4)
        f60 = struct.unpack(s.fmt[0] + "H", s.read(hq + 0x60, 2))[0]
        return {"log": s.log(), "obj": obj, "f60": f60, "f64": s.rd_words(hq + 0x64, 1)[0]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    rows = [0 if r.random() < 0.7 else r.choice(OBJ) for _ in range(9)]
    rows[1:] = [r.choice(OBJ) for _ in range(8)]
    return {"op": op, "queues": {}, "strikes": [r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]) for _ in range(15)],
            "f60": r.choice([0, 1, 2, 8, r.randrange(9)]), "f64": r.choice([0, 1, r.getrandbits(32)]), "rows": rows,
            "cell": r.choice([0, 1, 0xFFFF, r.getrandbits(16)]), "type": r.randrange(5)}


def drop_case(r):
    c = base(r, "drop")
    if r.random() < 0.2:
        c["rows"][0] = r.choice(OBJ)
    return c


def force_case(r):
    return base(r, "force")


def drop_edges():
    r = random.Random(11)
    for z in (0, OBJ[0]):
        for f in (0, 1, 8):
            c = drop_case(r)
            c["rows"][0] = z
            c["f60"] = f
            yield c


def force_edges():
    r = random.Random(12)
    for t in range(5):
        for v in (0, 1, 2, 3, 0xFFFFFFFF, 0x80000000):
            for f64 in (0, 1, 7):
                c = force_case(r)
                c["type"] = t
                c["strikes"][t * 3] = v
                c["f64"] = f64
                yield c


TESTS = [
    FnTest("CStaffHQ::DropStaff", ppc, host, edge=drop_edges, rnd=drop_case),
    FnTest("CStaffHQ::ForceStrike", ppc, host, edge=force_edges, rnd=force_case),
]
