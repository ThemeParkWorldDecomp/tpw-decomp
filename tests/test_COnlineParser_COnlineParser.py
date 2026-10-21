"""Unit v5131, the COnlineParser constructor, destructor and the four argument-less commands, against
libtpw_unit_COnlineParser_COnlineParser.dylib (tests/unit_COnlineParser_COnlineParser.cpp), main app ("SimTheme Park"),
src/game/online/OnlineParser.cpp:
  COnlineParser::COnlineParser 0x1020e400, ParseEarmuffs 0x1020faf8, ParseAfk 0x1021023c, ParseVote 0x10210648,
  ParseFilter 0x10210720, ~COnlineParser 0x102107fc

A case is a world and a call, built twice by one piece of Python code (emulator memory, big-endian, and the host library, little-endian).
Every callee outside the unit is a fake on both sides that records its pointers as (range tag * 0x10000 + offset) so that labels of
objects allocated per case are checked; ranges: 1 the parser object, 2 the argument vector, 3 the wide string, 4 the initial text
global, 5 the Argument the Parse functions build on their stack (registered by the string constructor fake).

The world: the parser object prefilled with random words (offsets 0..0x3b), the vector and string blocks, the global initial text (16
characters, restored after each case on the PowerPC side). Compared: the answer, the object words, the pointer fields, the event log
(the Push event carries the command word of the Argument). No fused operations. Not tested: nothing skipped; the destructor's flag
gets random garbage in the high half of r4 (extsh).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineParser_COnlineParser"
MASK = 0xFFFFFFFF

FN_CTOR, FN_EARMUFFS, FN_AFK, FN_VOTE, FN_FILTER, FN_DTOR = 0x1020E400, 0x1020FAF8, 0x1021023C, 0x10210648, 0x10210720, 0x102107FC
SIZES = {FN_CTOR: 188, FN_EARMUFFS: 168, FN_AFK: 168, FN_VOTE: 168, FN_FILTER: 168, FN_DTOR: 124}
OWN = set(SIZES)
ADDR = dict(text=0x10250A28)
TEXT_BYTES = 32
# host: offsetof mArguments, offsetof mText, sizeof the text global
HOST_LAYOUT = [0x40, 0x48, 32]

RANGES = []


def reg(tag, a, n):
    RANGES[:] = [r for r in RANGES if r[2] != tag]
    RANGES.append((a & MASK, n, tag))


def P(a):
    a &= MASK
    for b, n, t in RANGES:
        if b <= a < b + n:
            return t * 0x10000 + a - b
    return -1


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, addr, fn):
    STUBS.internal(addr, fn)


def one(name, addr):
    def f(e):
        e.cl.rec(name, (P(e.r(3)),))
    STUBS.internal(addr, f)


def flagged(name, addr):
    def f(e):
        e.cl.rec(name, (P(e.r(3)), T.s16(e.r(4))))
    STUBS.internal(addr, f)


def stub_string_ctor(e):
    reg(5, e.r(3), 12)
    e.cl.rec("StringCtor", (P(e.r(3)),))


STUBS.internal(0x1007363C, stub_string_ctor)
one("WStringCtor", 0x101E0B84)
flagged("WStringDtor", 0x101468F8)
flagged("StringDtor", 0x1000221C)
flagged("ParserDtor", 0x102111D4)
one("Ignore", 0x10211EB0)
one("ParserCtor", 0x10211064)
one("ListCtor", 0x10210DD4)
one("ListClear", 0x102109FC)
one("VecClear", 0x102108A4)
one("InitTokenList", 0x1020E898)
one("delete", 0x10238218)


def stub_push(e):
    a = e.r(4)
    e.cl.rec("Push", (P(e.r(3)), P(a), e.u32(a + 8)))


STUBS.internal(0x10201478, stub_push)


def stub_wcslen(e):
    a = e.r(3)
    n = 0
    while n < 64 and e.read(a + 2 * n, 2) != b"\0\0":
        n += 1
    e.cl.rec("wcslen", (P(a), n))
    e.set_r(3, n)


STUBS.internal(0x102389C8, stub_wcslen)


def stub_assign(e):
    a = e.r(4)
    chars = struct.unpack(">8H", e.read(a, 16))
    e.cl.rec("Assign", (P(e.r(3)), P(a), e.r(5)) + tuple(chars))


STUBS.internal(0x101B2510, stub_assign)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def text(self, chars):
        self.write(self.g("text"), struct.pack(self.fmt[0] + "16H", *chars))


class Ppc(Side):
    fmt = ">I"
    OFF_ARGS, OFF_TEXT = 0x3C, 0x40

    def __init__(self, e, case):
        self.e = e
        RANGES[:] = []
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.orig = e.read(ADDR["text"], TEXT_BYTES)

    def done(self):
        self.e.write(ADDR["text"], self.orig)
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def reg(self, tag, a, n):
        reg(tag, a, n)

    def ptr(self, a):
        return P(a)

    def setptr(self, obj, off, v):
        self.words(obj + off, [v])

    def getptr(self, obj, off):
        return self.rd_words(obj + off, 1)[0]

    def log(self):
        return self.L.log

    def call(self, op, obj, vec, txt, flag):
        entry = {"earmuffs": FN_EARMUFFS, "afk": FN_AFK, "vote": FN_VOTE, "filter": FN_FILTER}
        if op == "ctor":
            return T.call_ppc(self.e, FN_CTOR, [obj, vec, txt])[0] & MASK
        if op == "dtor":
            return T.call_ppc(self.e, FN_DTOR, [obj, flag])[0] & MASK
        T.call_ppc(self.e, entry[op], [obj])
        return 0


class Host(Side):
    fmt = "<I"
    OFF_ARGS, OFF_TEXT = 0x40, 0x48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_oparser_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_oparser_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_oparser_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def reg(self, tag, a, n):
        self.lib.tpw_oparser_reg(tag, a, n)

    def ptr(self, a):
        return self.lib.tpw_oparser_p(a)

    def setptr(self, obj, off, v):
        self.write(obj + off, struct.pack("<Q", v))

    def getptr(self, obj, off):
        return struct.unpack("<Q", self.read(obj + off, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, obj, vec, txt, flag):
        L = self.lib
        if op == "ctor":
            return L.tpw_oparser_ctor(obj, vec, txt)
        if op == "dtor":
            return L.tpw_oparser_dtor(obj, C.c_int16(flag).value)
        getattr(L, "tpw_oparser_" + op)(obj)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_oparser_addr.restype, lib.tpw_oparser_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_oparser_alloc.restype, lib.tpw_oparser_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_oparser_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_oparser_reset.argtypes = []
    lib.tpw_oparser_reg.argtypes = [C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_oparser_p.restype, lib.tpw_oparser_p.argtypes = C.c_int64, [C.c_uint64]
    lib.tpw_oparser_ctor.restype, lib.tpw_oparser_ctor.argtypes = C.c_uint64, [C.c_uint64] * 3
    lib.tpw_oparser_dtor.restype, lib.tpw_oparser_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    for n in ("earmuffs", "afk", "vote", "filter"):
        getattr(lib, "tpw_oparser_" + n).argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_oparser_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        obj = s.alloc(0x60)
        s.reg(1, obj, 0x60)
        s.words(obj, case["pre"])
        vec = s.alloc(16)
        s.reg(2, vec, 16)
        txt = s.alloc(16)
        s.reg(3, txt, 16)
        s.reg(4, s.g("text"), TEXT_BYTES)
        s.text(case["chars"])
        if case["op"] != "ctor":
            s.setptr(obj, s.OFF_ARGS, vec)
            s.setptr(obj, s.OFF_TEXT, txt)
        target = 0 if case.get("null") else obj
        answer = s.call(case["op"], target, vec, txt, case["flag"])
        log = s.log()
        return {"answer": s.ptr(answer) if answer else 0, "log": log, "words": s.rd_words(obj, 15),
                "args": s.ptr(s.getptr(obj, s.OFF_ARGS)), "text": s.ptr(s.getptr(obj, s.OFF_TEXT))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mkcase(r, op, **kw):
    n = r.choice([0, 0, 1, 2, 5, 8, 8, 9, 12])
    chars = [r.choice([0x41, 0x2173, 0xFFFF, 0x20, r.randrange(1, 0x10000)]) for _ in range(min(n, 15))]
    chars = (chars + [0])[:16]
    chars += [r.randrange(1, 0x10000) for _ in range(16 - len(chars))]
    flag = r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)])
    c = {"op": op, "queues": {}, "pre": [r.getrandbits(32) for _ in range(15)], "chars": chars, "flag": flag & MASK}
    c["flag"] = (flag & 0xFFFF) | (r.getrandbits(16) << 16) if op == "dtor" else 0
    c.update(kw)
    return c


def edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        for i in range(6):
            yield mkcase(r, op)
        if op == "dtor":
            for f in (0, 1, -1, 0x7FFF, -0x8000, 0x10000, 0x18000, 0xFFFF0000):
                c = mkcase(r, op)
                c["flag"] = f & MASK
                yield c
            yield mkcase(r, op, null=True)
        if op == "ctor":
            c = mkcase(r, op)
            c["chars"] = [0x2173] * 15 + [0]
            yield c
            c = mkcase(r, op)
            c["chars"] = [0] * 16
            yield c
    return gen


def rnd(op):
    return lambda r: mkcase(r, op)


TESTS = [
    FnTest("COnlineParser::COnlineParser", ppc, host, edge=edges("ctor"), rnd=rnd("ctor")),
    FnTest("COnlineParser::ParseEarmuffs", ppc, host, edge=edges("earmuffs"), rnd=rnd("earmuffs")),
    FnTest("COnlineParser::ParseAfk", ppc, host, edge=edges("afk"), rnd=rnd("afk")),
    FnTest("COnlineParser::ParseVote", ppc, host, edge=edges("vote"), rnd=rnd("vote")),
    FnTest("COnlineParser::ParseFilter", ppc, host, edge=edges("filter"), rnd=rnd("filter")),
    FnTest("COnlineParser::~COnlineParser", ppc, host, edge=edges("dtor"), rnd=rnd("dtor")),
]
