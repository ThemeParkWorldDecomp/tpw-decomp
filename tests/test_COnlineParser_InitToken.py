"""Unit v5187, COnlineParser's token list and the Say and Tell commands, against libtpw_unit_COnlineParser_InitToken.dylib
(tests/unit_COnlineParser_InitToken.cpp), main app ("SimTheme Park"), src/game/online/OnlineParser.cpp:
  InitToken 0x1020ed70, InitTokenList 0x1020e898, ParseSay 0x1020ef70, ParseTell 0x1020f0cc

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian, host memory
little-endian). The callees outside the unit are fakes on both sides that record their arguments: the string, Argument and CToken
constructors and destructors, the vector push_back, the list insert, CParser::GetString and GetRemainingString (answers from the case's
queues), the table's FormatString (writes "n<value>") and the converter's ConvertString (logs the source bytes, writes "w<value>").

The world of a case: gText's converter (word at 0x54c) and table (0x598), the parser object (the list position word at 0x38, the
argument vector and the text pointers), the 36 handler words at 0x102b1960. What is compared: the answer and the event log.

No fused operations. Not tested: a NULL table (the original calls FormatString on it unconditionally). With no converter the wide buffer
is read uninitialised in the original: that case is run and compared, the host leaves its buffer uninitialised as well.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineParser_InitToken"
MASK = 0xFFFFFFFF

FN_INIT_TOKEN, FN_INIT_LIST, FN_SAY, FN_TELL = 0x1020ED70, 0x1020E898, 0x1020EF70, 0x1020F0CC
SIZES = {FN_INIT_TOKEN: 308, FN_INIT_LIST: 1188, FN_SAY: 300, FN_TELL: 500}
OWN = set(SIZES)

ADDR = dict(text=0x1037FF18, handlers=0x102B1960)
HOST_LAYOUT = [0x40, 0x48, 12, 2288]

STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(0x1007363C, CL.recorder("StringCtor", "Nx"))
STUBS.internal(0x1000221C, CL.recorder("StringDtor", "Li"))
STUBS.internal(0x101E0B84, CL.recorder("WStringCtor", "Nx"))
_ctor_from = CL.recorder("WStringCtorFrom", "Nx")


def stub_ctor_from(e):
    _ctor_from(e)
    chars = []
    for i in range(10):
        c = struct.unpack(">H", e.read(e.r(4) + 2 * i, 2))[0]
        chars.append(c)
        if c == 0:
            break
    chars += [0] * (10 - len(chars))
    e.cl.rec("WStringText", tuple(chars))


STUBS.internal(0x10146A14, stub_ctor_from)
STUBS.internal(0x101468F8, CL.recorder("WStringDtor", "Li"))
STUBS.internal(0x101F7424, CL.recorder("WStringCopy", "NLui"))
STUBS.internal(0x102011A8, CL.recorder("ArgumentCtor", "NL"))
STUBS.internal(0x10201478, CL.recorder("PushBack", "LL"))
STUBS.internal(0x10211B3C, CL.recorder("ParserGetString", "LL", ret="b"))
STUBS.internal(0x10211D2C, CL.recorder("ParserGetRemainingString", "LLi", ret="b"))
STUBS.internal(0x10211F88, CL.recorder("CTokenCtor", "NLuuu"))

PARSER = [0]


def stub_list_insert(e):
    L = e.cl
    L.rec("ListInsert", (L.lab(e.r(4) - 0x30), e.r(5) & MASK, L.lab(e.r(6))))


STUBS.internal(0x10210B44, stub_list_insert)


def stub_format(e):
    L = e.cl
    L.rec("FormatString", (L.lab(e.r(3)), e.r(5) & MASK, e.r(6) & MASK))
    e.write(e.r(4), b"n%d\0" % T.s32(L.ret("fmt")))
    # the wide buffer (r1+0x16c = buffer + 0x100) is uninitialised in the original; the host zeroes it, so the stale stack is cleared here
    e.write(e.r(4) + 0x100, bytes(268))


def stub_convert(e):
    L = e.cl
    src = e.read(e.r(6), 10)
    src = src.split(b"\0")[0]
    src = src + bytes(10 - len(src))
    L.rec("ConvertString", tuple([L.lab(e.r(4)), e.r(7) & 0xFFFF] + list(src)))
    text = "w%d" % T.s32(L.ret("conv"))
    e.write(e.r(5), b"".join(struct.pack(">H", ord(c)) for c in text) + b"\0\0")
    e.set_r(3, 0)


STUBS.imp("FormatString__8TbITableCFPUcUiUle", stub_format)
STUBS.imp("ConvertString__14TbIMBToUnicodeFPwPCUcUs", stub_convert)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.parser = e.alloc(0x60)
        self.vec = e.alloc(16)
        self.text = e.alloc(16)
        self.table = e.alloc(16)
        self.mb = e.alloc(16)
        self.L = e.cl = CL.PpcLog({self.parser: 1, self.vec: 2, self.text: 3, self.table: 4, self.mb: 5}, case["queues"])
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

    def setup_world(self, case):
        self.words(ADDR["text"] + 0x54C, [self.mb if case["mb"] else 0])
        self.words(ADDR["text"] + 0x598, [self.table])
        self.words(self.parser + 0x38, [case["pos"]])
        self.words(self.parser + 0x3C, [self.vec, self.text])

    def call(self, op, case):
        if op == "tok":
            return T.call_ppc(self.e, FN_INIT_TOKEN, [self.parser, case["id"], *case["handler"]])[0] & 0
        fn = {"list": FN_INIT_LIST, "say": FN_SAY, "tell": FN_TELL}[op]
        return T.call_ppc(self.e, fn, [self.parser])[0] & (0 if op == "list" else 0xFF)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.parser = lib.tpw_ci_alloc(0x60)
        self.vec = lib.tpw_ci_alloc(16)
        self.text = lib.tpw_ci_alloc(16)
        self.table = lib.tpw_ci_alloc(16)
        self.mb = lib.tpw_ci_alloc(16)
        CL.host_begin(lib, {1: self.parser, 2: self.vec, 3: self.text, 4: self.table, 5: self.mb}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ci_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup_world(self, case):
        self.lib.tpw_ci_setup(self.mb if case["mb"] else 0, self.table)
        self.words(self.parser + 0x38, [case["pos"]])
        self.write(self.parser + 0x40, struct.pack("<QQ", self.vec, self.text))

    def call(self, op, case):
        lib = self.lib
        if op == "tok":
            lib.tpw_ci_InitToken(self.parser, case["id"], *case["handler"])
            return 0
        if op == "list":
            lib.tpw_ci_InitTokenList(self.parser)
            return 0
        return (lib.tpw_ci_ParseSay if op == "say" else lib.tpw_ci_ParseTell)(self.parser) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ci_addr.restype, lib.tpw_ci_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ci_alloc.restype, lib.tpw_ci_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ci_setup.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_ci_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ci_InitToken.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_ci_InitTokenList.argtypes = [C.c_uint64]
    for n in ("tpw_ci_ParseSay", "tpw_ci_ParseTell"):
        getattr(lib, n).restype, getattr(lib, n).argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ci_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.setup_world(case)
        hs = [w for h in case["handlers"] for w in h]
        s.words(s.g("handlers"), hs)
        answer = s.call(case["op"], case)
        return {"answer": answer, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    q = {"fmt": [r.randrange(0, 10 ** r.choice([1, 3, 7])) for _ in range(36)],
         "conv": [r.randrange(0, 10 ** r.choice([1, 3, 7])) for _ in range(36)],
         "ParserGetString": [r.choice([0, 1])],
         "ParserGetRemainingString": [r.choice([0, 1])]}
    return {"op": op, "queues": q, "mb": r.random() < 0.85, "pos": r.choice([0, 0x12345678, r.getrandbits(32)]),
            "id": r.choice([0, 1, 0x2B, 0x42, r.getrandbits(32)]), "handler": [r.getrandbits(32) for _ in range(3)],
            "handlers": [[r.getrandbits(32) for _ in range(3)] for _ in range(36)]}


def make(op, edges):
    def edge():
        r = random.Random(sum(map(ord, op)))
        for mb in (True, False):
            for q in edges:
                c = base(r, op)
                c["mb"] = mb
                c["queues"].update(q)
                yield c

    def rnd(r):
        return base(r, op)
    return edge, rnd


NONE = [{}]
SAY_Q = [{"ParserGetRemainingString": [0]}, {"ParserGetRemainingString": [1]}]
TELL_Q = [{"ParserGetString": [0]}, {"ParserGetString": [1], "ParserGetRemainingString": [0]},
          {"ParserGetString": [1], "ParserGetRemainingString": [1]}]

TESTS = []
for name, op, edges in (("COnlineParser::InitToken", "tok", NONE), ("COnlineParser::InitTokenList", "list", NONE),
                        ("COnlineParser::ParseSay", "say", SAY_Q), ("COnlineParser::ParseTell", "tell", TELL_Q)):
    e_, r_ = make(op, edges)
    TESTS.append(FnTest(name, ppc, host, edge=e_, rnd=r_))
