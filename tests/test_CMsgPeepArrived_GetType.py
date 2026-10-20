"""Unit v51064, twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp, against
libtpw_unit_CMsgPeepArrived_GetType.dylib (tests/unit_CMsgPeepArrived_GetType.cpp), main app ("SimTheme Park"),
src/CPerson_..._LOADVAR_CMsgPeepArrived_GetType.cpp: the OPS table below (name, address, size).

A case is a world and a call. The world is a 0x240-byte block (the person, the peep, the headquarters, a message or a behaviour) built twice by ONE piece
of Python code: in emulator memory at the Mac's addresses (big-endian) and in the host library's memory (little-endian). The block is tiled into
halfwords, words (the two state words) and bytes (where the function reads bytes), so that one logical value is stored the same way on both sides. The
callees outside the unit are fakes on both sides that record their arguments: SetSpriteScript, SetState, ProcessStateWaitingForScriptToAdmitMe,
CThing::FinalDestroy and operator delete.

What is compared: the answer, the event log, the whole block (but its first word, where the destructors and constructors store the vtable, which the
host does not model), the two words of a static initialiser, the flags and id of a constructed message, the words of a constructed BehaviourEntry (the
host pointer is 8 bytes wide: its high word is compared with 0). The destructors' delete flag is passed as the low halfword of r4 with junk above it.
No fused operations. Not tested: GetPreferredExcitement with a byte above 155 (the original reads past the balance table into other data; the host
table is shorter), a NULL person (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMsgPeepArrived_GetType"
MASK = 0xFFFFFFFF
BLOCK = 0x240

# name, address, size, call kind, result kind
OPS = [
    ("CMsgPeepArrived::GetType", 0x1011b794, 8, "none", "s32"),
    ("CMsgPeepArrived::~CMsgPeepArrived", 0x10116c54, 76, "dtor", "ptr"),
    ("CMsgPeepArrived::CMsgPeepArrived", 0x101521e4, 40, "id", "msg"),
    ("__sinit_Person_cpp", 0x1011ba64, 28, "none", "sinit"),
    ("CPerson::GetPreferredExcitement", 0x1011b4b4, 28, "this", "u8"),
    ("CMsgPrankery::~CMsgPrankery", 0x1011c5c4, 76, "dtor", "ptr"),
    ("CPerson::AddRideToRecentTemporaryHistory", 0x10119ac8, 52, "id", "none"),
    ("CPerson::AddRideToRecentHistory", 0x10119a30, 52, "id", "none"),
    ("CPerson::ForceRideAdmittance", 0x1011df70, 36, "this", "none"),
    ("CPerson::EndSpotAnim", 0x10117f80, 40, "this", "none"),
    ("CPerson::ReadyForAdmitting", 0x1011f0b8, 40, "this", "u8"),
    ("CPerson::InQueueingState", 0x10120b2c, 52, "this", "s32"),
    ("CMsgPrankery::GetType", 0x10120b90, 8, "none", "s32"),
    ("__sinit_PersonQueue_cpp", 0x10120bc8, 28, "none", "sinit"),
    ("CPeep::SetStandScript", 0x1011524c, 44, "this", "none"),
    ("CPeep::LastCellVisited", 0x10113814, 48, "last", "u16"),
    ("__sinit_MechanicHQ_cpp", 0x10105d2c, 28, "none", "sinit"),
    ("CMechanicHQ::FinalDestroy", 0x101052f4, 36, "this", "none"),
    ("Arrival::~Arrival", 0x10135474, 88, "dtor", "ptr"),
    ("Seek::~Seek", 0x101354f0, 88, "dtor", "ptr"),
    ("Behaviour::~Behaviour", 0x10135568, 76, "dtor", "ptr"),
    ("BehaviourEntry::BehaviourEntry", 0x1013567c, 16, "this", "entry"),
    ("__sinit_VehicleCallbacks_cpp", 0x10136060, 28, "none", "sinit"),
    ("__sinit_guard_cpp", 0x100fef90, 28, "none", "sinit"),
]
IDX = {o[0]: i for i, o in enumerate(OPS)}
SINIT = {3: (0x1034c4a0, 0x1034c4a4), 13: (0x1034c4a8, 0x1034c4ac), 16: (0x1034c2d0, 0x1034c2d4),
         22: (0x1034caa0, 0x1034caa4), 23: (0x1034c204, 0x1034c208)}
BAL = 0x102b4568 + 8          # gMainBalance.mValues[0]
ADDR = {"bal": BAL}
for _i, (_a, _b) in SINIT.items():
    ADDR["s%dA" % _i] = _a
    ADDR["s%dB" % _i] = _b

CALLEE_SPRITE, CALLEE_ADMIT, CALLEE_STATE, CALLEE_FINAL, CALLEE_DELETE = 0x100faa74, 0x1011dfc8, 0x10120354, 0x1012e9f8, 0x10238218
WORDS = {0x220, 0x224}
BYTES = {4: {0x1f0, 0x1f1}, 10: {0x1f0, 0x1f1}, 15: {4, 5, 6, 7}}
STATES = [0, 1, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0xffffffff, 0x7fffffff, 0x80000000, 0x1000000b, 0x10000008]
HOST_LAYOUT = None            # filled by bind (flags offset, id offset, sizeof BehaviourEntry, pointer offset)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(o[1] for o in OPS)


def stub_sprite(e):
    e.cl.rec("SetSpriteScript", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_admit(e):
    e.cl.rec("Admit", (e.cl.lab(e.r(3)),))


def stub_state(e):
    e.cl.rec("SetState", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_final(e):
    e.cl.rec("FinalDestroy", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.internal(CALLEE_SPRITE, stub_sprite)
STUBS.internal(CALLEE_ADMIT, stub_admit)
STUBS.internal(CALLEE_STATE, stub_state)
STUBS.internal(CALLEE_FINAL, stub_final)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd(self, a, code):
        n = struct.calcsize(code)
        return struct.unpack(self.fmt[0] + code, self.read(a, n))[0]

    def lab(self, v):
        if v == 0:
            return 0
        return 1 if v == self.obj else -1


class Ppc(Side):
    fmt = ">I"
    msg_offs = (4, 8)
    ptr_off = 4
    hi = False

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(BLOCK)
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.obj + 0xc: 2}, case["queues"])
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

    def call(self, case):
        name, addr, size, kind, res = OPS[case["i"]]
        a = case["args"]
        obj = 0 if (kind == "dtor" and a[0]) else self.obj
        if kind == "none":
            args = []
        elif kind == "this":
            args = [self.obj]
        elif kind == "id":
            args = [self.obj, ((a[0] << 16) | case["junk"]) & MASK]
        elif kind == "dtor":
            args = [obj, ((case["junk"] << 16) | (a[1] & 0xFFFF)) & MASK]
        else:
            buf = self.e.alloc(8)
            self.write(buf, b"\xa5\xa5\xa5\xa5")
            args = [buf, self.obj]
        r3, _, _ = T.call_ppc(self.e, addr, args)
        if kind == "last":
            return self.rd(buf, "H")
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    hi = True

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_v51064_alloc(BLOCK)
        self.msg_offs = (HOST_LAYOUT[0], HOST_LAYOUT[1])
        self.ptr_off = HOST_LAYOUT[3]
        CL.host_begin(lib, {1: self.obj, 2: self.obj + 0xc}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51064_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        name, addr, size, kind, res = OPS[case["i"]]
        a = case["args"]
        if kind == "none":
            return self.lib.tpw_v51064_call(case["i"], 0, 0) & MASK
        if kind == "dtor":
            r = self.lib.tpw_v51064_call(case["i"], 0 if a[0] else self.obj, s32(a[1]))
            return r
        b = a[0] if kind == "id" else 0
        return self.lib.tpw_v51064_call(case["i"], self.obj, b) & MASK


def bind(lib):
    global HOST_LAYOUT
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51064_addr.restype, lib.tpw_v51064_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51064_alloc.restype, lib.tpw_v51064_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51064_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v51064_call.restype, lib.tpw_v51064_call.argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_int64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51064_layout(out)
    HOST_LAYOUT = list(out)[:n]
    assert HOST_LAYOUT[2] == 16 and HOST_LAYOUT[3] == 8, HOST_LAYOUT
    lib.bound = True


def tiles(i):
    """(offset, kind, size) over the block from offset 4; kinds H halfword, W word, B byte."""
    out = []
    off = 4
    by = BYTES.get(i, set())
    while off < BLOCK:
        if off in WORDS:
            out.append((off, "I", 4))
            off += 4
        elif off in by:
            out.append((off, "B", 1))
            off += 1
        else:
            out.append((off, "H", 2))
            off += 2
    return out


def build(s, case):
    i = case["i"]
    res = OPS[i][4]
    if res == "sinit":
        s.words(s.g("s%dA" % i), [case["g"][0]])
        s.words(s.g("s%dB" % i), [case["g"][1]])
        return
    if res in ("msg", "entry"):
        s.words(s.obj, [0xA5A5A5A5] * 4)
        return
    for off, code, size in tiles(i):
        s.write(s.obj + off, struct.pack(s.fmt[0] + code, case["vals"][off] & ((1 << (8 * size)) - 1)))
    if "bal" in case:
        # the host's BalanceValue is a union with a pointer (8 bytes); the integer is its low word
        ws = []
        for w in case["bal"]:
            ws += [w, 0] if s.hi else [w]
        s.words(s.g("bal"), ws)


def observe(s, case, answer):
    i = case["i"]
    res = OPS[i][4]
    o = {"log": s.log()}
    if res == "ptr":
        o["ret"] = s.lab(answer)
    elif res in ("s32",):
        o["ret"] = s32(answer)
    elif res == "u8":
        o["ret"] = answer & 0xFF
    elif res == "u16":
        o["ret"] = answer & 0xFFFF
    if res == "sinit":
        o["g"] = [s.rd(s.g("s%dA" % i), "I"), s.rd(s.g("s%dB" % i), "I")]
    elif res == "msg":
        o["flags"] = s.rd(s.obj + s.msg_offs[0], "I")
        o["id"] = s.rd(s.obj + s.msg_offs[1], "H")
    elif res == "entry":
        o["w"] = s.rd(s.obj, "I")
        o["p"] = s.rd(s.obj + s.ptr_off, "I")
        o["phi"] = s.rd(s.obj + s.ptr_off + 4, "I") if s.hi else 0
    else:
        o["blk"] = [s.rd(s.obj + off, code) for off, code, size in tiles(i)]
    return o


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rv(r, code):
    if code == "H":
        return r.choice([0, 0xFFFF, r.getrandbits(16)])
    if code == "B":
        return r.choice([0, 1, 255, r.getrandbits(8)])
    return r.choice(STATES) if r.random() < 0.8 else r.getrandbits(32)


def rand_args(r, i):
    kind = OPS[i][3]
    if kind in ("id",):
        return [r.choice([0, 0xFFFF, 1, r.randrange(65536)])]
    if kind == "dtor":
        return [r.random() < 0.15, r.choice([-32768, -1, 0, 1, 2, 32767, r.randrange(-32768, 32768)])]
    return []


def make_case(r, i, force=None, args=None):
    c = {"i": i, "queues": {}, "vals": {}, "junk": r.getrandbits(16)}
    for off, code, size in tiles(i):
        c["vals"][off] = rv(r, code)
    if i == 4:
        c["vals"][0x1F0] = r.randrange(156)
        c["bal"] = [r.getrandbits(32) for _ in range(504)]
    if i == 10 and r.random() < 0.5:
        c["vals"][0x1F1] = 0
    c["vals"].update(force or {})
    c["args"] = args if args is not None else rand_args(r, i)
    if OPS[i][4] == "sinit":
        c["g"] = [r.getrandbits(32), r.getrandbits(32)]
    return c


def edges(i):
    r = random.Random(1000 + i)
    kind, res = OPS[i][3], OPS[i][4]
    yield make_case(r, i)
    if i == 4:
        for k in (0, 1, 2, 77, 154, 155):
            yield make_case(r, i, {0x1F0: k})
    elif i == 10:
        for st in (0xB, 0xA, 0xC, 0x1000000B, 0, 0xFFFFFFFF):
            for flag in (0, 1, 0xFF):
                yield make_case(r, i, {0x220: st, 0x1F1: flag})
    elif i == 11:
        for st in STATES:
            for st2 in (0, 0xA, 0xB, 0xE, 0xF, 0xFFFFFFFF):
                yield make_case(r, i, {0x220: st, 0x224: st2})
    elif i == 15:
        for x, y in ((0, 0), (255, 0), (0, 255), (127, 1), (128, 128), (255, 255)):
            yield make_case(r, i, {4: x, 6: y})
    elif kind == "dtor":
        for null in (False, True):
            for flag in (-32768, -1, 0, 1, 2, 32767):
                yield make_case(r, i, args=[null, flag])
    elif kind == "id":
        for v in (0, 1, 0xFFFF, 0x8000):
            yield make_case(r, i, args=[v])


TESTS = [FnTest(o[0], ppc, host, edge=(lambda i=i: edges(i)), rnd=(lambda r, i=i: make_case(r, i)))
         for i, o in enumerate(OPS)]
