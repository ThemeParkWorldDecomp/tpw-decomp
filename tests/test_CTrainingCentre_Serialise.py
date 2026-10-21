"""CTrainingCentre::Serialise and UpdateMonthly, against libtpw_unit_CTrainingCentre_Serialise.dylib
(tests/unit_CTrainingCentre_Serialise.cpp), main app, src/game/buildings/TrainingCentre.cpp:
  CTrainingCentre::Serialise 0x10130008, CTrainingCentre::UpdateMonthly 0x1013018c

A case is an object and a call, built twice by one piece of Python code (emulator memory and host memory). The callees outside the unit
(SAVEVAR/LOADVAR int and bool, GAME_ASSERTMSG, CWorld::GetUsedThingHead / GetNextUsedThing, CStaff::TrainMe) are fakes on both sides that
record their arguments (the value pointer as an offset in the object, names and assert texts as length and byte sum, things by id).

The world of a case: the object's five words and byte, the training count globals, the two end markers, a table of things (id, kind) and
the walk sequences. Compared: the answer, the object, the event log.

No fused operations. Not tested: a count of zero for a kind with staff (divw by zero is undefined on hardware; counts start at 0 or more).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTrainingCentre_Serialise"
MASK = 0xFFFFFFFF

FN_SER, FN_UPD = 0x10130008, 0x1013018c
SIZES = {FN_SER: 332, FN_UPD: 928}
OWN = set(SIZES)

SAVE_I, SAVE_B, LOAD_I, LOAD_B = 0x1000F4B0, 0x1000F70C, 0x1000EF18, 0x1000F144
ASSERT, HEAD, NEXT, TRAIN = 0x1000DDE8, 0x1013ADB0, 0x1013AE08, 0x10125854

ADDR = dict(counts=0x102A4408, end1=0x102A441C, end2=0x102A441E, world=0x1037EBFC, arr=0x1034CBFC)
HOST_LAYOUT = [0x14, 0x18]
FILE = 0x12345678


def s32(v):
    return T.s32(v)


def key(e, a):
    s = e.cstr(a)
    if isinstance(s, str):
        s = s.encode("latin1")
    return len(s) * 100000 + sum(s)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_io(name):
    def f(e):
        L = e.cl
        L.rec(name, (s32(e.r(3)), s32(e.r(4) - e.obj), key(e, e.r(5)), s32(e.r(6))))
        e.set_r(3, L.ret("io") & MASK)
    return f


for a, n in ((SAVE_I, "save_i"), (SAVE_B, "save_b"), (LOAD_I, "load_i"), (LOAD_B, "load_b")):
    STUBS.internal(a, mk_io(n))


def stub_assert(e):
    e.cl.rec("assert", (e.r(3) & 0xFF, key(e, e.r(4))))


def stub_head(e):
    L = e.cl
    L.rec("head", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("head") & 0xFFFF))


def stub_next(e):
    L = e.cl
    L.rec("next", (L.lab(e.r(4)), struct.unpack(">H", e.read(e.r(5), 2))[0]))
    e.write(e.r(3), struct.pack(">H", L.ret("next") & 0xFFFF))


def stub_train(e):
    e.cl.rec("TrainMe", (struct.unpack(">H", e.read(e.r(3), 2))[0], s32(e.r(4))))


STUBS.internal(ASSERT, stub_assert)
STUBS.internal(HEAD, stub_head)
STUBS.internal(NEXT, stub_next)
STUBS.internal(TRAIN, stub_train)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def thing(self, tid, kind):
        a = self.alloc(8)
        self.write(a, struct.pack(self.fmt[0] + "HBB", tid, kind, 0))
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["world"]: 1}, case["queues"])
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

    def set_world(self):
        self.words(ADDR["world"], [ADDR["world"]])

    def set_thing(self, idx, ptr):
        self.words(ADDR["arr"] + idx * 0x14, [ptr])

    def ser(self, obj, save, ver, unused):
        self.e.obj = obj
        r3, _, _ = T.call_ppc(self.e, FN_SER, [obj, FILE, save & MASK, ver & MASK, unused & MASK])
        return r3 & MASK

    def upd(self, obj):
        self.e.obj = obj
        T.call_ppc(self.e, FN_UPD, [obj])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("world")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ct_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self):
        self.lib.tpw_ct_set_world()

    def set_thing(self, idx, ptr):
        self.lib.tpw_ct_set_thing(idx, ptr)

    def ser(self, obj, save, ver, unused):
        return self.lib.tpw_ct_serialise(obj, FILE, s32(save), s32(ver), unused & MASK) & MASK

    def upd(self, obj):
        self.lib.tpw_ct_update(obj)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ct_addr.restype, lib.tpw_ct_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ct_alloc.restype, lib.tpw_ct_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ct_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ct_set_thing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_ct_set_world.argtypes = []
    lib.tpw_ct_serialise.restype = C.c_int32
    lib.tpw_ct_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_ct_update.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ct_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    obj = s.alloc(0x18)
    s.words(obj, case["words"])
    s.write(obj + 0x14, bytes([case["flag"]]))
    s.words(s.g("counts"), case["counts"])
    s.half(s.g("end1"), case["end1"])
    s.half(s.g("end2"), case["end2"])
    s.set_world()
    s.set_thing(0, case["entry0"])
    for tid, kind in case["things"]:
        s.set_thing(tid, s.thing(tid, kind))
    return obj


def run(s, case):
    try:
        obj = build(s, case)
        if case["op"] == "ser":
            ans = s.ser(obj, *case["args"])
        else:
            ans = s.upd(obj)
        return {"answer": ans, "log": s.log(), "words": s.rd_words(obj, 5), "flag": s.read(obj + 0x14, 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


E1, E2 = 0x2F00, 0x2F11


def base(r, op):
    return {"op": op, "queues": {"io": [0], "head": [E1, E2], "next": [E1]},
            "words": [r.choice([0, 1, -1, r.randrange(0, 5000), r.randrange(-5000, 5000)]) & MASK for _ in range(5)],
            "flag": r.choice([0, 1, 7]), "counts": [r.choice([0, 1, 2, 5]) for _ in range(5)],
            "end1": E1, "end2": E2, "entry0": 0, "things": []}


def ser_case(r, save=None, ver=None, fail=None):
    c = base(r, "ser")
    save = r.choice([0, 1, 5, -1]) if save is None else save
    ver = r.choice([-1, 0, 1, 2, 3, r.randrange(-100, 100)]) if ver is None else ver
    c["args"] = [save, ver, r.choice([0, 1, 0xFFFFFFFF])]
    fail = r.choice([None, 0, 1, 2, 3, 4, 5]) if fail is None else fail
    c["queues"]["io"] = [0] * (fail if fail is not None else 6) + ([1] if fail is not None else [0])
    return c


def upd_case(r, n=None, kinds=None):
    c = base(r, "upd")
    n = r.randrange(0, 7) if n is None else n
    ids = r.sample(range(1, 0x2800), n)
    c["things"] = [(i, (kinds[k] if kinds else r.choice([4, 5, 6, 7, 8, 4, 5, 6, 7, 8, 0, 1, 3, 9, 0x13]))) for k, i in enumerate(ids)]
    c["entry0"] = r.choice([0, 0, 0, 0x1234])
    seq = ids[1:]
    c["queues"]["head"] = [ids[0] if ids else E1, ids[0] if ids else E2]
    c["queues"]["next"] = seq + [E1] + seq + [E2] if ids else [E1]
    return c


def ser_edges():
    r = random.Random(72)
    for save in (0, 1):
        for ver in (-1, 0, 1, 2, 3):
            for fail in (None, 0, 3, 4, 5):
                yield ser_case(r, save, ver, fail)


def upd_edges():
    r = random.Random(73)
    yield upd_case(r, 0)
    yield upd_case(r, 1, [4])
    for k in (4, 5, 6, 7, 8, 0, 3, 9, 3, 2):
        yield upd_case(r, 1, [k])
    yield upd_case(r, 5, [4, 5, 6, 7, 8])
    yield upd_case(r, 6, [8, 8, 7, 4, 4, 5])
    yield upd_case(r, 2, [5, 5])


TESTS = [
    FnTest("CTrainingCentre::Serialise", ppc, host, edge=ser_edges, rnd=lambda r: ser_case(r)),
    FnTest("CTrainingCentre::UpdateMonthly", ppc, host, edge=upd_edges, rnd=lambda r: upd_case(r)),
]
