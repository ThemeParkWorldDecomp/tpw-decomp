"""Unit v5117, CTimeSig::Serialise and CTimeSig::SecondsSinceLastUpdate, against libtpw_unit_CTimeSig_Serialise.dylib
(tests/unit_CTimeSig_Serialise.cpp), main app, src/TPOptions_CTimeSig_Serialise.cpp:
  CTimeSig::Serialise 0x1015ef5c, CTimeSig::SecondsSinceLastUpdate 0x1015f124

A case is an object and a call. The object (one word) and a world block (mTurn at 0x1da70c) are built by one piece of Python code in both
memories. SAVEVAR/LOADVAR<unsigned long> and CWorldSavvy::GetWorld are fakes on both sides recording their arguments (file label, the word
at the value pointer, a hash of the name, noLog); LOADVAR stores a queued word into the value.

What is compared: the answer, the object's word, the world's turn, the event log. No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTimeSig_Serialise"
MASK = 0xFFFFFFFF

FN_SER, FN_SEC = 0x1015ef5c, 0x1015f124
SIZES = {FN_SER: 160, FN_SEC: 72}
OWN = set(SIZES)
SAVEVAR, LOADVAR, GETWORLD = 0x1000f5dc, 0x1000f02c, 0x1014205c
FILE = 0x1234
TURN_OFF = 0x1da70c
WORLD = [0]


def name_hash(s):
    return sum(s) + 256 * len(s)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_save(e):
    L = e.cl
    v = e.u32(e.r(4))
    L.rec("SAVEVAR", (L.lab(e.r(3)), v, name_hash(e.cstr(e.r(5)).encode() if isinstance(e.cstr(e.r(5)), str) else e.cstr(e.r(5))), s32(e.r(6))))
    e.set_r(3, L.ret("SAVEVAR") & MASK)


def stub_load(e):
    L = e.cl
    v = e.u32(e.r(4))
    nm = e.cstr(e.r(5))
    nm = nm.encode() if isinstance(nm, str) else nm
    L.rec("LOADVAR", (L.lab(e.r(3)), v, name_hash(nm), s32(e.r(6))))
    e.write(e.r(4), struct.pack(">I", L.ret("LOADVAL") & MASK))
    e.set_r(3, L.ret("LOADVAR") & MASK)


def stub_getworld(e):
    e.cl.rec("GetWorld", ())
    e.set_r(3, WORLD[0])


def s32(v):
    return T.s32(v)


STUBS.internal(SAVEVAR, stub_save)
STUBS.internal(LOADVAR, stub_load)
STUBS.internal(GETWORLD, stub_getworld)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({FILE: 7}, case["queues"])
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

    def set_world(self, w):
        WORLD[0] = w

    def ser(self, obj, save, ver, flags):
        r3, _, _ = T.call_ppc(self.e, FN_SER, [obj, FILE, save & MASK, ver & MASK, flags & MASK])
        return r3 & MASK

    def sec(self, obj):
        r3, _, _ = T.call_ppc(self.e, FN_SEC, [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {7: FILE}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ts_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self, w):
        self.lib.tpw_ts_setworld(w)

    def ser(self, obj, save, ver, flags):
        return self.lib.tpw_ts_serialise(obj, FILE, s32(save), s32(ver), flags & MASK) & MASK

    def sec(self, obj):
        return self.lib.tpw_ts_seconds(obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ts_alloc.restype, lib.tpw_ts_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ts_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ts_setworld.argtypes = [C.c_uint64]
    lib.tpw_ts_serialise.restype = C.c_int32
    lib.tpw_ts_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_ts_seconds.restype = C.c_int32
    lib.tpw_ts_seconds.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ts_layout(out)
    assert list(out)[:n] == [0, TURN_OFF], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(16)
        world = s.alloc(TURN_OFF + 16)
        s.set_world(world)
        s.words(obj, [case["stamp"]])
        s.words(world + TURN_OFF, [case["turn"]])
        if case["op"] == "ser":
            ans = s.ser(obj, *case["args"])
        else:
            ans = s.sec(obj)
        return {"answer": ans, "log": s.log(), "stamp": s.rd_word(obj), "turn": s.rd_word(world + TURN_OFF)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rw(r):
    return r.choice([0, 1, 3, 4, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.randrange(0, 1000)])


def base(r, op):
    return {"op": op, "stamp": rw(r), "turn": rw(r),
            "queues": {"SAVEVAR": [r.choice([0, 1, 5])], "LOADVAR": [r.choice([0, 1, 5])], "LOADVAL": [rw(r)]}}


def ser_case(r, save=None, ver=None):
    c = base(r, "ser")
    c["args"] = [r.choice([0, 1, 2, -1, 5]) if save is None else save,
                 r.choice([0, 1, 2, -1, 3, -100, 0x7FFFFFFF]) if ver is None else ver,
                 r.getrandbits(32)]
    return c


def ser_edges():
    r = random.Random(5117)
    for save in (0, 1, 7):
        for ver in (-1, 0, 1, 2, 3, -0x80000000, 0x7FFFFFFF):
            for q in (0, 1, 5):
                c = ser_case(r, save, ver)
                c["queues"] = {"SAVEVAR": [q], "LOADVAR": [q], "LOADVAL": [r.getrandbits(32)]}
                yield c


def ser_random(r):
    return ser_case(r)


def sec_edges():
    r = random.Random(5118)
    for stamp, turn in ((0, 0), (3, 4), (4, 3), (0xFFFFFFFF, 0), (0, 0xFFFFFFFF), (0x80000000, 0x7FFFFFFF), (7, 8), (8, 7), (1, 4)):
        c = base(r, "sec")
        c["stamp"], c["turn"] = stamp, turn
        yield c


def sec_random(r):
    return base(r, "sec")


TESTS = [
    FnTest("CTimeSig::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
    FnTest("CTimeSig::SecondsSinceLastUpdate", ppc, host, edge=sec_edges, rnd=sec_random),
]
