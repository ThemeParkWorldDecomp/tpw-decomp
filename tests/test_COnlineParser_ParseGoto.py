"""Unit v5051, the COnlineParser line parsers, against libtpw_unit_COnlineParser_ParseGoto.dylib (tests/unit_COnlineParser_ParseGoto.cpp), main app
("SimTheme Park"), src/game/online/OnlineParser.cpp:
  ParseBlackMark 0x1020fd70, ParseBuddy 0x1020ff0c, ParsePage 0x102100a4, ParseGoto 0x10210314, ParseLocate 0x102104ac

A case is a parser object (the argument vector pointer at 0x3c) and a call. The callees (string constructors and destructors, wide string
copy, Argument, vector push_back, GetString, IgnoreRemainingString) are fakes on both sides recording the words they read; they write marker
words so the data flow shows in the log. The world: the vector words (first word a marker), the queues for GetString (answer and the word it
stores) and IgnoreRemainingString. Compared: the answer (low byte), the event log, the parser object and the vector. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineParser_ParseGoto"
MASK = 0xFFFFFFFF

FNS = {"BlackMark": 0x1020fd70, "Buddy": 0x1020ff0c, "Page": 0x102100a4, "Goto": 0x10210314, "Locate": 0x102104ac}
SIZES = {a: 360 for a in FNS.values()}
OWN = set(SIZES)
HOST_LAYOUT = [0x40]      # the vector pointer is native on the host (8 bytes, after the 0x3c bytes of base)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(name, word):
    def fn(e):
        e.cl.rec(name, ())
        e.write(e.r(3), struct.pack(">I", word))
    return fn


def stub_dtor(name):
    def fn(e):
        e.cl.rec(name, (e.u32(e.r(3)), T.s16(e.r(4))))
    return fn


def stub_wcopy(e):
    v = e.u32(e.r(4))
    e.cl.rec("WStringCopy", (v, e.r(5) & MASK, e.r(6) & MASK))
    e.write(e.r(3), struct.pack(">I", v ^ 0x1234))


def stub_argctor(e):
    v = e.u32(e.r(4))
    e.cl.rec("ArgumentCtor", (v,))
    e.write(e.r(3), struct.pack(">III", 0xAC01, v, 0xAC02))


def stub_push(e):
    v = e.u32(e.r(3))
    a = [e.u32(e.r(4) + 4 * i) for i in range(3)]
    e.cl.rec("PushBack", (v, a[0], a[1], a[2]))


def stub_getstring(e):
    L = e.cl
    ok = 1 if L.ret("GetString") != 0 else 0
    word = L.ret("Word") & MASK
    L.rec("GetString", (ok,))
    e.write(e.r(4), struct.pack(">I", word))
    e.set_r(3, ok)


def stub_ignore(e):
    L = e.cl
    L.rec("Ignore", ())
    e.set_r(3, 1 if (L.ret("Ignore") & 0xFF) != 0 else 0)


STUBS.internal(0x1007363c, stub_ctor("StringCtor", 0xC0DE0001))
STUBS.internal(0x101e0b84, stub_ctor("WStringCtor", 0xC0DE0002))
STUBS.internal(0x101468f8, stub_dtor("WStringDtor"))
STUBS.internal(0x1000221c, stub_dtor("StringDtor"))
STUBS.internal(0x101f7424, stub_wcopy)
STUBS.internal(0x102011a8, stub_argctor)
STUBS.internal(0x10201478, stub_push)
STUBS.internal(0x10211b3c, stub_getstring)
STUBS.internal(0x10211eb0, stub_ignore)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def set_ptr(self, parser, vec):
        self.write(parser + 0x3c, struct.pack(">I", vec))

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def call(self, op, parser):
        r3, _, _ = T.call_ppc(self.e, FNS[op], [parser])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"

    def set_ptr(self, parser, vec):
        self.write(parser + HOST_LAYOUT[0], struct.pack("<Q", vec))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_parse_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, parser):
        return getattr(self.lib, "tpw_parse_" + op)(parser) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_parse_alloc.restype, lib.tpw_parse_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_parse_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in FNS:
        f = getattr(lib, "tpw_parse_" + op)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_parse_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        parser = s.alloc(0x80)
        vec = s.alloc(16)
        s.words(vec, [case["marker"], 0, 0])
        s.words(parser, case["fill"])
        s.set_ptr(parser, vec)
        ans = s.call(case["op"], parser)
        return {"answer": ans, "log": s.log(), "parser": s.rd_words(parser, 15), "vec": s.rd_words(vec, 3)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make_case(r, op, ok=None, ign=None):
    ok = r.choice([0, 1]) if ok is None else ok
    ign = r.choice([0, 1]) if ign is None else ign
    return {"op": op, "marker": r.getrandbits(32), "fill": [r.getrandbits(32) for _ in range(15)],
            "queues": {"GetString": [ok], "Word": [r.getrandbits(32)], "Ignore": [ign]}}


def edges_for(op):
    def gen():
        r = random.Random(5051 + FNS[op])
        for ok in (0, 1):
            for ign in (0, 1):
                yield make_case(r, op, ok, ign)
    return gen


def rnd_for(op):
    return lambda r: make_case(r, op)


TESTS = [FnTest("COnlineParser::Parse" + op, ppc, host, edge=edges_for(op), rnd=rnd_for(op)) for op in FNS]
