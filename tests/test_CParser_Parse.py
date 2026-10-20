"""Unit v5911, the chat line parser's base class CParser, against libtpw_unit_CParser_Parse.dylib (tests/unit_CParser_Parse.cpp), main app
("SimTheme Park"), src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser_CParser_Parse.cpp:
  SetupForParsing 0x102113dc, Parse 0x102114f8, Match 0x10211630, ~CParser 0x102111d4, SkipWhitespace 0x10211860,
  SkipWhitespaceAndSeparator 0x1021193c, CountNonWhitespace 0x10211a60, GetString 0x10211b3c, GetRemainingString 0x10211d2c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian words)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation. The callees outside the unit are fakes
on both sides that record their arguments: operator[] 0x10212298, substr 0x1021212c, assign 0x101f7014, begin 0x10212448, compare 0x101d5864,
cmp_nocase 0x10210e68, the set's find 0x10212060 and destroy 0x10212604, the string destructor 0x101468f8, operator delete 0x10238218 and
the pointer-to-member glue 0x1023ba70. The string fakes work on real representations (one pointer to length, capacity, refs, data) so
the unit's own reads of them are tested. Strings are logged by length-aware hash; find answers come from a per (tree, character) queue.

The world of a case: the parser block (string, the two character sets' root words, position, the three flag bytes, the token list with
names and handler words), an output string, a count word. What is compared: the answer, the position, flags, string, cursor offset,
roots, vtable word, output string and count, and the whole event log.

No fused operations. Not tested: a position more than one past the end (the library's operator[] would fail there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CParser_Parse"
MASK = 0xFFFFFFFF

F_SETUP, F_PARSE, F_MATCH, F_DTOR = 0x102113DC, 0x102114F8, 0x10211630, 0x102111D4
F_SKIPWS, F_SKIPWSSEP, F_COUNT, F_GETSTR, F_GETREM = 0x10211860, 0x1021193C, 0x10211A60, 0x10211B3C, 0x10211D2C
SIZES = {F_SETUP: 104, F_PARSE: 276, F_MATCH: 256, F_DTOR: 184, F_SKIPWS: 172, F_SKIPWSSEP: 232, F_COUNT: 168, F_GETSTR: 388,
         F_GETREM: 268}
OWN = set(SIZES)

C_INDEX, C_FIND, C_SUBSTR, C_ASSIGN, C_BEGIN, C_COMPARE = 0x10212298, 0x10212060, 0x1021212C, 0x101F7014, 0x10212448, 0x101D5864
C_CMPNOCASE, C_STRDTOR, C_TREEDESTROY, C_DELETE, C_HANDLER = 0x10210E68, 0x101468F8, 0x10212604, 0x10238218, 0x1023BA70

ADDR = dict(vtbl=0x102B1B44)
HOST_LAYOUT = [8, 0x10, 0x28, 0x40, 0x48, 0x4D, 0x4E, 0x4F, 0x50, 0x58, 8, 0x10, 0x28, 8, 8, 0x10, 0x18, 8, 0x18, 0x10]

CUR = None
UNSET_CURSOR = 0x5555


def h32(chars):
    h = 0
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(e, name, *vals):
    e.cl.rec(name, tuple(v & MASK for v in vals))


def s16(v):
    return ((v & 0xFFFF) ^ 0x8000) - 0x8000


def new_rep_for(chars, pos, n):
    avail = len(chars) - pos if pos <= len(chars) else 0
    cnt = min(n, avail)
    return CUR.mkrep(chars[pos:pos + cnt] if cnt else [])


def stub_index(e):
    idx = e.r(4)
    rec(e, "index", idx)
    rep = CUR.rd_ptr(e.r(3))
    data = CUR.rd_ptr(rep + CUR.REPDATA)
    e.set_r(3, (data + 2 * idx) & MASK)


def stub_find(e):
    result, tree, key = e.r(3), e.r(4), e.r(5)
    off = (tree - CUR.parser) & MASK
    ch = struct.unpack(">H", e.read(key, 2))[0]
    rec(e, "find", off, ch)
    ans = e.cl.ret("find:%d" % ((off << 16) | ch))
    e.write(result, struct.pack(">I", (tree + 4 if ans == 0 else tree + 0x10) & MASK))


def stub_substr(e):
    result, selfp, pos, n = e.r(3), e.r(4), e.r(5), e.r(6)
    rec(e, "substr", pos, n)
    CUR.wr_ptr(result, new_rep_for(CUR.chars(selfp), pos, n))
    e.set_r(3, result)


def stub_assign(e):
    selfp, strp, pos, n = e.r(3), e.r(4), e.r(5), e.r(6)
    chars = CUR.chars(strp)
    rec(e, "assign", pos, n, h32(chars))
    CUR.wr_ptr(selfp, new_rep_for(chars, pos, n))
    e.set_r(3, selfp)


def stub_begin(e):
    rec(e, "begin")
    v = e.cl.ret("begin") & 3
    rep = CUR.rd_ptr(e.r(3))
    e.set_r(3, (CUR.rd_ptr(rep + CUR.REPDATA) + 2 * v) & MASK)


def stub_compare(e):
    selfp, pos, n1, s, n2 = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    sc = list(struct.unpack(">%dH" % n2, e.read(s, 2 * n2))) if n2 else []
    rec(e, "compare", pos, n1, n2, h32(sc), h32(CUR.chars(selfp)))
    e.set_r(3, e.cl.ret("compare") & MASK)


def stub_cmp_nocase(e):
    rec(e, "cmp_nocase", h32(CUR.chars(e.r(3))), h32(CUR.chars(e.r(4))), e.r(5) & 0xFF)
    e.set_r(3, e.cl.ret("cmp_nocase") & MASK)


def stub_strdtor(e):
    s = e.r(3)
    which = 4 if s == CUR.parser + 4 else 99
    rec(e, "strdtor", which, s16(e.r(4)), h32(CUR.chars(s)))


def stub_treedestroy(e):
    rec(e, "treedestroy", e.r(3) - CUR.parser, e.r(4))


def stub_delete(e):
    rec(e, "delete", 1 if e.r(3) == CUR.parser else 0)


def stub_handler(e):
    r12 = e.r(12)
    w = [e.u32(r12 + 4 * i) for i in range(3)]
    rec(e, "handler", 0 if e.r(3) == CUR.parser else 1, *w)
    e.set_r(3, e.cl.ret("handler") & MASK)


for _a, _f in ((C_INDEX, stub_index), (C_FIND, stub_find), (C_SUBSTR, stub_substr), (C_ASSIGN, stub_assign), (C_BEGIN, stub_begin),
               (C_COMPARE, stub_compare), (C_CMPNOCASE, stub_cmp_nocase), (C_STRDTOR, stub_strdtor),
               (C_TREEDESTROY, stub_treedestroy), (C_DELETE, stub_delete), (C_HANDLER, stub_handler)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.P))[0]

    def wr_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def mkrep(self, chars):
        rep = self.alloc(0x18)
        data = self.alloc(2 * len(chars) + 8)
        if chars:
            self.write(data, struct.pack(self.fmt[0] + "%dH" % len(chars), *chars))
        self.words(rep, [len(chars), len(chars), 0])
        self.wr_ptr(rep + self.REPDATA, data)
        return rep

    def mkstr(self, chars):
        obj = self.alloc(8)
        self.wr_ptr(obj, self.mkrep(chars))
        return obj

    def chars(self, obj):
        rep = self.rd_ptr(obj)
        n = self.rd_word(rep)
        data = self.rd_ptr(rep + self.REPDATA)
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(data, 2 * n))) if n else []


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    P = 4
    REPDATA = 0xC
    PSZ, NSZ, LSZ = 0x30, 0x18, 0x0C
    NNEXT, NNAME, NH, LSENT, LFIRST, TROOT = 4, 8, 0xC, 4, 8, 4
    O = dict(str=4, sep=8, ws=0x14, cur=0x20, pos=0x24, f=0x29, list=0x2C)

    def __init__(self, e, case):
        global CUR
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.L = e.cl
        self.saved = STUBS.activate(e)
        CUR = self

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

    def call(self, fn, args):
        return T.call_ppc(self.e, F_BY_OP[fn], [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    P = 8
    REPDATA = 0x10
    PSZ, NSZ, LSZ = 0x58, 0x28, 0x18
    NNEXT, NNAME, NH, LSENT, LFIRST, TROOT = 8, 0x10, 0x18, 8, 0x10, 8
    O = dict(str=8, sep=0x10, ws=0x28, cur=0x40, pos=0x48, f=0x4D, list=0x50)

    def __init__(self, lib, case):
        global CUR
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        CUR = self

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        res = getattr(self.lib, "tpw_cp_" + fn)(*args)
        if fn in ("match", "dtor"):
            return res
        return 0 if res is None else res & MASK


F_BY_OP = dict(setup=F_SETUP, parse=F_PARSE, match=F_MATCH, dtor=F_DTOR, skipws=F_SKIPWS, skipwssep=F_SKIPWSSEP, count=F_COUNT,
               getstring=F_GETSTR, getremaining=F_GETREM)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cp_addr.restype, lib.tpw_cp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cp_alloc.restype, lib.tpw_cp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cp_layout.argtypes = [C.POINTER(C.c_int64)]
    u64 = C.c_uint64
    sig = dict(setup=(None, [u64] * 4), parse=(C.c_int32, [u64]), match=(u64, [u64, u64]), dtor=(u64, [u64, C.c_int32]),
               skipws=(C.c_int32, [u64]), skipwssep=(C.c_int32, [u64]), count=(C.c_int32, [u64]),
               getstring=(C.c_uint32, [u64, u64]), getremaining=(C.c_uint32, [u64, u64, u64]))
    for name, (res, args) in sig.items():
        f = getattr(lib, "tpw_cp_" + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_cp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(x) for x in list(out)[:n]]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    O = s.O
    s.parser = 0
    s.nodes = []
    s.lst = 0
    s.out = s.cnt = s.arg = s.text = s.props = 0
    if case.get("null"):
        return
    p = s.parser = s.alloc(s.PSZ)
    s.wr_ptr(p, 0x1111)
    s.wr_ptr(p + O["str"], s.mkrep(case["chars"]))
    s.wr_ptr(p + O["sep"] + s.TROOT, case["sep_root"])
    s.wr_ptr(p + O["ws"] + s.TROOT, case["ws_root"])
    s.wr_ptr(p + O["cur"], UNSET_CURSOR)
    s.words(p + O["pos"], [case["pos"]])
    s.write(p + O["f"], bytes([case["c29"], case["c2a"], case["c2b"]]))
    # the token list
    lst = s.lst = s.alloc(s.LSZ)
    sent = lst + s.LSENT
    for t in case["tokens"]:
        n = s.alloc(s.NSZ)
        s.nodes.append(n)
        s.wr_ptr(n + s.NNAME, s.mkrep(t["name"]))
        s.words(n + s.NH, t["h"])
    for i, n in enumerate(s.nodes):
        s.wr_ptr(n, s.nodes[i - 1] if i else sent)
        s.wr_ptr(n + s.NNEXT, s.nodes[i + 1] if i + 1 < len(s.nodes) else sent)
    s.wr_ptr(sent, s.nodes[-1] if s.nodes else sent)
    s.wr_ptr(lst + s.LFIRST, s.nodes[0] if s.nodes else sent)
    s.words(lst, [len(s.nodes)])
    s.wr_ptr(p + O["list"], lst)
    s.out = s.mkstr(case["out"])
    s.cnt = s.alloc(4) if case["count"] else 0
    if s.cnt:
        s.words(s.cnt, [0x77777777])
    s.arg = s.mkstr(case["arg"])
    s.text = s.mkstr(case["text"])
    s.props = s.alloc(4)
    s.write(s.props, bytes(case["props"]))


def execute(s, case):
    op, p = case["op"], s.parser
    if op == "setup":
        s.call(op, [p, s.text, s.lst, s.props])
        return 0
    if op == "match":
        res = s.call(op, [p, s.arg])
        return s.nodes.index(res - s.NNAME) if res else -1
    if op == "dtor":
        res = s.call(op, [p, case["flag"]])
        return 0 if res == 0 else (1 if res == p else 2)
    if op == "getstring":
        return s.call(op, [p, s.out]) & 0xFF
    if op == "getremaining":
        return s.call(op, [p, s.out, s.cnt]) & 0xFF
    return s.call(op, [p])


def observe(s, case, answer):
    r = {"answer": answer, "log": s.log()}
    p, O = s.parser, s.O
    if p:
        rep = s.rd_ptr(p + O["str"])
        data = s.rd_ptr(rep + s.REPDATA)
        cur = s.rd_ptr(p + O["cur"])
        r["pos"] = s.rd_word(p + O["pos"])
        r["flags"] = list(s.read(p + O["f"], 3))
        r["tokens"] = s.rd_ptr(p + O["list"]) == s.lst
        r["vtbl"] = s.rd_ptr(p) == s.g("vtbl")
        r["str"] = s.chars(p + O["str"])
        r["cursor"] = "unset" if cur == UNSET_CURSOR else cur - data
        r["roots"] = [s.rd_ptr(p + O["sep"] + s.TROOT), s.rd_ptr(p + O["ws"] + s.TROOT)]
        r["out"] = s.chars(s.out)
        r["count"] = s.rd_word(s.cnt) if s.cnt else None
        r["nodes"] = [[s.chars(n + s.NNAME), s.rd_words(n + s.NH, 3)] for n in s.nodes]
    return r


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


# ---------------------------------------------------------------------------------------------------------------- case generators

ALPHA = [0x61, 0x62, 0x20, 0x2C, 0x09]


def rtext(r, lo=0, hi=7):
    return [r.choice(ALPHA) for _ in range(r.randint(lo, hi))]


def base(r, op):
    chars = rtext(r)
    q = {}
    for off in (8, 0x14):
        for ch in ALPHA + [0]:
            q["find:%d" % ((off << 16) | ch)] = [r.choice([0, 1])]
    q["cmp_nocase"] = [r.choice([0, 1, MASK, 5]) for _ in range(r.randint(1, 4))]
    q["compare"] = [r.choice([0, 1, MASK, 5]) for _ in range(r.randint(1, 4))]
    q["handler"] = [r.choice([0, 1, 1, 1]) for _ in range(r.randint(1, 3))]
    q["begin"] = [r.randrange(4)]
    return dict(op=op, queues=q, chars=chars, pos=r.randint(0, len(chars) + 1), c29=r.choice([0, 1]), c2a=r.choice([0, 0, 1]),
                c2b=r.choice([0, 0, 1]), sep_root=r.choice([0, 0x1234]), ws_root=r.choice([0, 0x5678]),
                tokens=[dict(name=rtext(r, 0, 3), h=[r.getrandbits(32) for _ in range(3)]) for _ in range(r.randint(0, 3))],
                text=rtext(r), out=rtext(r, 0, 3), arg=rtext(r, 0, 3), props=[r.choice([0, 1]), r.choice([0, 1]), r.choice([0, 1])],
                count=r.choice([True, False]), flag=r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000]) & 0xFFFF)


TEXTS = [[], [0x61], [0x20, 0x61], [0x61, 0x20, 0x62], [0x20, 0x2C, 0x61, 0x2C, 0x20, 0x62, 0x20], [0x2C, 0x2C], [0x61, 0x62, 0x09, 0x61]]
SETS = [((0x2C,), (0x20, 0x09)), ((), (0x20,)), ((0x20, 0x2C), ()), ((0x2C, 0x20, 0x09), (0x2C, 0x20, 0x09)), ((), ())]


def exact(c, sets):
    for off, members in ((8, sets[0]), (0x14, sets[1])):
        for ch in ALPHA + [0]:
            c["queues"]["find:%d" % ((off << 16) | ch)] = [1 if ch in members else 0]
    return c


def edge_gen(op, seed):
    def gen():
        r = random.Random(seed)
        for text in TEXTS:
            for sets in SETS:
                for pos in sorted({0, len(text) // 2, len(text), len(text) + 1}):
                    c = exact(base(r, op), sets)
                    c["chars"], c["pos"] = text, pos
                    yield c
    return gen


def match_edges():
    r = random.Random(77)
    for c29 in (0, 1):
        for c2b in (0, 1):
            for ntok in (0, 1, 2, 3):
                for cmpq in ([0], [1, 0], [1, 1, 1], [1, 1, 0]):
                    c = base(r, "match")
                    c["c29"], c["c2b"] = c29, c2b
                    c["tokens"] = c["tokens"][:ntok] + [dict(name=rtext(r, 0, 3), h=[1, 2, 3]) for _ in range(ntok - len(c["tokens"]))]
                    c["queues"]["cmp_nocase"] = cmpq
                    c["queues"]["compare"] = cmpq
                    yield c


def parse_edges():
    r = random.Random(78)
    yield from edge_gen("parse", 78)()
    for stop in (0, 1):
        for handler in ([1], [0], [1, 0], [1, 1, 0]):
            for cmpq in ([0], [1, 0], [1, 1, 0]):
                c = exact(base(r, "parse"), SETS[0])
                c["chars"], c["pos"], c["c2a"] = [0x61, 0x20, 0x62, 0x2C, 0x61], 0, stop
                c["queues"]["handler"] = handler
                c["queues"]["cmp_nocase"] = c["queues"]["compare"] = cmpq
                c["tokens"] = [dict(name=[0x61], h=[5, 6, 7]), dict(name=[0x62], h=[8, 9, 10])]
                yield c


def setup_edges():
    r = random.Random(79)
    for props in ([0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], [1, 1, 1], [0x80, 0x7F, 2]):
        for text in TEXTS[:4]:
            for begin in (0, 1, 2, 3):
                c = base(r, "setup")
                c["props"], c["text"], c["queues"]["begin"] = props, text, [begin]
                yield c


def dtor_edges():
    r = random.Random(80)
    c = base(r, "dtor")
    c["null"] = True
    yield c
    for sep in (0, 0x1234):
        for ws in (0, 0x5678):
            for flag in (-1, 0, 1, 2, 0x7FFF, -0x8000, 0x8000, 0xFFFF):
                c = base(r, "dtor")
                c["sep_root"], c["ws_root"], c["flag"] = sep, ws, flag & 0xFFFF
                yield c


def ge(op, seed):
    def gen():
        yield from edge_gen(op, seed)()
        r = random.Random(seed + 1000)
        for count in (True, False):
            for sets in SETS:
                c = exact(base(r, op), sets)
                c["count"] = count
                yield c
    return gen


def rnd(op):
    return lambda r: base(r, op)


TESTS = [
    FnTest("CParser::Parse", ppc, host, edge=parse_edges, rnd=rnd("parse")),
    FnTest("CParser::SetupForParsing", ppc, host, edge=setup_edges, rnd=rnd("setup")),
    FnTest("CParser::Match", ppc, host, edge=match_edges, rnd=rnd("match")),
    FnTest("CParser::~CParser", ppc, host, edge=dtor_edges, rnd=rnd("dtor")),
    FnTest("CParser::SkipWhitespace", ppc, host, edge=edge_gen("skipws", 81), rnd=rnd("skipws")),
    FnTest("CParser::SkipWhitespaceAndSeparator", ppc, host, edge=edge_gen("skipwssep", 82), rnd=rnd("skipwssep")),
    FnTest("CParser::CountNonWhitespace", ppc, host, edge=edge_gen("count", 83), rnd=rnd("count")),
    FnTest("CParser::GetString", ppc, host, edge=ge("getstring", 84), rnd=rnd("getstring")),
    FnTest("CParser::GetRemainingString", ppc, host, edge=ge("getremaining", 85), rnd=rnd("getremaining")),
]
