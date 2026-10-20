"""Unit v5556, the peep's constructor, against libtpw_unit_CPeep_CPeep.dylib (tests/unit_CPeep_CPeep.cpp), main app, src/..._CPeep_CPeep.cpp:
  CPeep::CPeep 0x10112e18

A case is a block for the peep (random words, so every store shows), a world block, and the message centre either present or NULL. The callees
(CThing, CDelayedScript, CThoughts, Pilot ctors, GetRand, operator new, __construct_array, Subscribe, reset, get_pos) are fakes on both sides that
record their arguments; GetRand and get_pos answer from the case. Compared: the whole peep block, the log, the message centre pointer's state.
No fused operations. Not tested: the constructor arguments of __construct_array (TOC vectors, host passes none).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPeep_CPeep"
MASK = 0xFFFFFFFF

FN = 0x10112E18
OWN = {FN}

CALLEES = dict(CThing=0x1012DE58, CDelayedScript=0x100FA898, CThoughts=0x1012EC88, Pilot=0x10134610, GetRand=0x1013B1E4,
               Subscribe=0x10152388, reset=0x101347B0, get_pos=0x10134888, new=0x102384B8, array=0x10238758)

ADDR = dict(gWorld=0x1037EC0C, gMessageCentre=0x1037F6C4, gPersonTable=0x102A0D54)
HOST_LAYOUT = [2]
PEEP_SIZE = 0x1C0


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, spec):
    def stub(e):
        L = e.cl
        L.rec(name, tuple(spec(e, L)))
    return stub


def st_cthing(e):
    e.cl.rec("CThing", (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, e.r(5) & MASK))


def st_plain(name):
    def f(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return f


def st_getrand(e):
    L = e.cl
    L.rec("GetRand", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetRand") & MASK)


def st_subscribe(e):
    L = e.cl
    L.rec("Subscribe", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
    e.set_r(3, 0)


def st_reset(e):
    L = e.cl
    L.rec("reset", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))


def st_get_pos(e):
    L = e.cl
    L.rec("get_pos", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.write(e.r(4), struct.pack(">I", L.ret("gx") & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("gy") & MASK))


def st_new(e):
    assert e.r(3) & MASK == 0x1D0
    e.set_r(3, e.alloc(e.r(3) & MASK))


def st_array(e):
    L = e.cl
    L.rec("array", (L.lab(e.r(3)), e.r(6) & MASK, e.r(7) & MASK))


STUBS.internal(CALLEES["CThing"], st_cthing)
STUBS.internal(CALLEES["CDelayedScript"], st_plain("CDelayedScript"))
STUBS.internal(CALLEES["CThoughts"], st_plain("CThoughts"))
STUBS.internal(CALLEES["Pilot"], st_plain("Pilot"))
STUBS.internal(CALLEES["GetRand"], st_getrand)
STUBS.internal(CALLEES["Subscribe"], st_subscribe)
STUBS.internal(CALLEES["reset"], st_reset)
STUBS.internal(CALLEES["get_pos"], st_get_pos)
STUBS.internal(CALLEES["new"], st_new)
STUBS.internal(CALLEES["array"], st_array)


def setup(runner):
    T.install_std_stubs(runner.emu)


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
        self.peep = e.alloc(PEEP_SIZE)
        self.world = e.alloc(0x40)
        self.preset = e.alloc(0x1D0)
        self.L = e.cl = CL.PpcLog({self.peep: 1, self.peep + 0xC: 3, self.peep + 0x30: 4, self.peep + 0xD4: 5,
                                   self.world: 6, self.preset: 2}, case["queues"])
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

    def call(self, cell, model):
        r3, _, _ = T.call_ppc(self.e, FN, [self.peep, (cell << 16) & MASK, model & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.peep = lib.tpw_peep_alloc(PEEP_SIZE)
        self.world = lib.tpw_peep_alloc(0x40)
        self.preset = lib.tpw_peep_alloc(0x1D0)
        CL.host_begin(lib, {1: self.peep, 3: self.peep + 0xC, 4: self.peep + 0x30, 5: self.peep + 0xD4, 6: self.world, 2: self.preset},
                      case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_peep_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, cell, model):
        self.lib.tpw_peep_ctor(self.peep, cell & 0xFFFF, s32(model))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_peep_addr.restype, lib.tpw_peep_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_peep_alloc.restype, lib.tpw_peep_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_peep_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_peep_ctor.restype, lib.tpw_peep_ctor.argtypes = None, [C.c_uint64, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_peep_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


WORDS = [0x1C, 0x20, 0x24, 0x28, 0xC8, 0xD0, 0x190, 0x194, 0x198]
HALVES = [0xC0, 0xC2, 0xC4, 0xCC]
BYTES = [0x2C]
# the bytes the constructor never writes itself (the fakes write nothing either): compared raw
WRITTEN = set()
for _o in WORDS:
    WRITTEN.update(range(_o, _o + 4))
for _o in HALVES:
    WRITTEN.update(range(_o, _o + 2))
for _o in BYTES:
    WRITTEN.add(_o)


def put_ptr(s, a, v):
    if s.fmt == "<I":
        s.write(a, struct.pack("<Q", v))
    else:
        s.write(a, struct.pack(">I", v))


def get_ptr(s, a):
    if s.fmt == "<I":
        return struct.unpack("<Q", s.read(a, 8))[0]
    return struct.unpack(">I", s.read(a, 4))[0]


def build(s, case):
    s.words(s.peep, case["peep"])
    put_ptr(s, s.g("gWorld"), s.world)
    put_ptr(s, s.g("gMessageCentre"), s.preset if case["preset"] else 0)
    s.write(s.g("gPersonTable"), struct.pack(s.fmt[0] + "H", case["table0"]))


def execute(s, case):
    return s.call(*case["args"])


def observe(s, case, answer):
    mc = get_ptr(s, s.g("gMessageCentre"))
    raw = bytearray(s.read(s.peep, PEEP_SIZE))
    # the Pilot's bytes (0xd4..0x18f) belong to the faked Pilot::Pilot; the host fake's member initialisers write some of them
    raw[0xD4:0x190] = bytes(0x190 - 0xD4)
    for i in WRITTEN:       # whole words: a half or a byte sits at different places in a little-endian word
        raw[i & ~3:(i & ~3) + 4] = b"\0\0\0\0"
    if s.fmt == "<I":       # the case's words went in little-endian: show them as the Mac's big-endian words
        for i in range(0, PEEP_SIZE, 4):
            raw[i:i + 4] = raw[i:i + 4][::-1]
    f = s.fmt[0]
    fields = [struct.unpack(f + "I", s.read(s.peep + o, 4))[0] for o in WORDS]
    fields += [struct.unpack(f + "H", s.read(s.peep + o, 2))[0] for o in HALVES]
    fields += [s.read(s.peep + o, 1)[0] for o in BYTES]
    return {"log": s.log(), "raw": bytes(raw), "fields": fields,
            "mc": 0 if mc == 0 else (1 if mc == s.preset else 2),
            "preset": s.rd_words(s.preset, 0x1D0 // 4)}


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


def mkcase(r, cell, model=None, rand=None, preset=None):
    return {"args": [cell, r.getrandbits(32) if model is None else model],
            "peep": [r.getrandbits(32) for _ in range(PEEP_SIZE // 4)],
            "preset": r.choice([0, 1]) if preset is None else preset,
            "table0": r.choice([0, 25, r.getrandbits(16)]),
            "queues": {"GetRand": [r.getrandbits(32) if rand is None else rand],
                       "gx": [r.getrandbits(32)], "gy": [r.getrandbits(32)]}}


CELLS = [0, 1, 2, 3, 0x7F, 0x80, 0x81, 0x100, 0x101, 0x201, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 0x1234, 0xABCD]
RANDS = [0, 1, 2, 3, 4, 5, 6, 9, 10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE]


def edges():
    r = random.Random(5556)
    for cell in CELLS:
        yield mkcase(r, cell)
    for rv in RANDS:
        for preset in (0, 1):
            yield mkcase(r, r.getrandbits(16), rand=rv, preset=preset)


def rnd(r):
    return mkcase(r, r.getrandbits(16))


TESTS = [FnTest("CPeep::CPeep", ppc, host, edge=edges, rnd=rnd)]
