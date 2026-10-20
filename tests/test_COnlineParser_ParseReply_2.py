"""Unit v5891, the COnlineParser commands Reply, Last, Shout, Emote and EmoteTell, against libtpw_unit_COnlineParser_ParseReply_2.dylib
(tests/unit_COnlineParser_ParseReply_2.cpp), main app, src/COnlineParser_ParseGoto_COnlineParser_ParseReply_2.cpp:
  ParseReply 0x1020f2f0, ParseLast 0x1020f460, ParseShout 0x1020f5d0, ParseEmote 0x1020f72c, ParseEmoteTell 0x1020f888

A case is a parser object (the vector pointer and the text pointer at its Mac offsets 0x3c and 0x40) and a call. The callees (string and
Argument constructors and destructors, the vector push_back, GetString, GetRemainingString) are fakes on both sides that log their
arguments (objects as labels; the command word of a pushed Argument; the first characters of the initial text); GetString,
GetRemainingString and the count come from the case's queues. Compared: the answer and the whole event log. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineParser_ParseReply_2"
MASK = 0xFFFFFFFF

FNS = {"reply": 0x1020F2F0, "last": 0x1020F460, "shout": 0x1020F5D0, "emote": 0x1020F72C, "emotetell": 0x1020F888}
SIZES = {0x1020F2F0: 320, 0x1020F460: 320, 0x1020F5D0: 300, 0x1020F72C: 300, 0x1020F888: 572}
OWN = set(SIZES)

ADDR = dict(text=0x10250A28)
HOST_LAYOUT = [0x40, 0x48]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u16(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def lab(e, a):
    return e.cl.lab(a)


def st_string_ctor(e):
    e.cl.rec("StringCtor", (lab(e, e.r(3)),))


def st_wstring_ctor(e):
    e.cl.rec("WStringCtor", (lab(e, e.r(3)),))


def st_wstring_ctor_from(e):
    ch = struct.unpack(">4H", e.read(e.r(4), 8))
    e.cl.rec("WStringCtorFrom", (lab(e, e.r(3)),) + ch)


def st_wstring_dtor(e):
    e.cl.rec("WStringDtor", (lab(e, e.r(3)), T.s16(e.r(4))))


def st_string_dtor(e):
    e.cl.rec("StringDtor", (lab(e, e.r(3)), T.s16(e.r(4))))


def st_wstring_copy(e):
    e.cl.rec("WStringCopy", (lab(e, e.r(3)), lab(e, e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))


def st_argument_ctor(e):
    e.cl.rec("ArgumentCtor", (lab(e, e.r(3)), lab(e, e.r(4))))
    e.write(e.r(3) + 8, b"\0\0\0\0")


def st_push_back(e):
    e.cl.rec("PushBack", (lab(e, e.r(3)), e.u32(e.r(4) + 8)))


def st_get_string(e):
    e.cl.rec("GetString", (lab(e, e.r(3)), lab(e, e.r(4))))
    e.set_r(3, 1 if e.cl.ret("GetString") else 0)


def st_get_remaining(e):
    p = e.r(5)
    e.cl.rec("GetRemaining", (lab(e, e.r(3)), lab(e, e.r(4)), 1 if p else 0))
    if p:
        e.write(p, struct.pack(">i", T.s32(e.cl.ret("count"))))
    e.set_r(3, 1 if e.cl.ret("GetRemaining") else 0)


STUBS.internal(0x1007363C, st_string_ctor)
STUBS.internal(0x101E0B84, st_wstring_ctor)
STUBS.internal(0x10146A14, st_wstring_ctor_from)
STUBS.internal(0x101468F8, st_wstring_dtor)
STUBS.internal(0x1000221C, st_string_dtor)
STUBS.internal(0x101F7424, st_wstring_copy)
STUBS.internal(0x102011A8, st_argument_ctor)
STUBS.internal(0x10201478, st_push_back)
STUBS.internal(0x10211B3C, st_get_string)
STUBS.internal(0x10211D2C, st_get_remaining)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def chars(self, a, cs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", c & 0xFFFF) for c in cs))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj, vec, txt = e.alloc(0x50), e.alloc(16), e.alloc(16)
        self.words(self.obj + 0x3C, [vec, txt])
        self.L = e.cl = CL.PpcLog({self.obj: 1, vec: 2, txt: 3}, case["queues"])
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

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FNS[op], [self.obj])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj, vec, txt = lib.tpw_cop_alloc(0x58), lib.tpw_cop_alloc(16), lib.tpw_cop_alloc(16)
        self.write(self.obj + 0x40, struct.pack("<QQ", vec, txt))
        CL.host_begin(lib, {1: self.obj, 2: vec, 3: txt}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cop_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        return getattr(self.lib, "tpw_cop_" + op)(self.obj) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cop_addr.restype, lib.tpw_cop_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cop_alloc.restype, lib.tpw_cop_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cop_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in FNS:
        f = getattr(lib, "tpw_cop_" + op)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cop_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.chars(s.g("text"), case["text"])
        answer = s.call(case["op"])
        return {"answer": answer, "log": s.log(),
                "text": list(struct.unpack(s.fmt[0] + "16H", s.read(s.g("text"), 32)))}
    finally:
        s.done()


def make(op):
    def ppc(e, case):
        return run(Ppc(e, case), case)

    def host(lib, case):
        return run(Host(lib, case), case)

    def case_of(r, gs, gr, count):
        return {"op": op, "text": [r.randrange(1, 0xFFFF) for _ in range(16)],
                "queues": {"GetString": [gs], "GetRemaining": [gr], "count": [count]}}

    def edges():
        r = random.Random(891)
        for gs in (0, 1):
            for gr in (0, 1):
                for count in (0, 1, -1, 7):
                    yield case_of(r, gs, gr, count)

    def rnd(r):
        return case_of(r, r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 1, 2, 100, -5]))

    return ppc, host, edges, rnd


def test(name, op):
    ppc, host, edges, rnd = make(op)
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [
    test("COnlineParser::ParseReply", "reply"),
    test("COnlineParser::ParseLast", "last"),
    test("COnlineParser::ParseShout", "shout"),
    test("COnlineParser::ParseEmote", "emote"),
    test("COnlineParser::ParseEmoteTell", "emotetell"),
]
