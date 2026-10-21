"""Unit v5094, the swear filter, against libtpw_unit_SwearFilter_Filter.dylib (tests/unit_SwearFilter_Filter.cpp), main app
("SimTheme Park"), src/game/online/IPBase.cpp:
  SwearFilter::Filter 0x1020d2cc, SwearFilter::LoadWordList 0x1020cd08

A case is a world and a call. The original runs in the emulator on Mac-layout lists and strings (a list is {size, sentinel prev, sentinel
next}, a node {prev, next, string}, a string one pointer to {length, capacity, refs, data}); the translation runs on std::list and
std::u16string, built by the host library's helpers from the same case. The callees outside the unit (stristr, _wcsnicmp, the stream
constructor, destructors, open, close and read, the exception pieces) are fakes on both sides that record the same events. The string
constructor, the list insert and the string destructor of the Mac are fakes that really build the Mac's list (no event): the lists
are compared after the call instead.

The world of a case: the character class table (__wctype_map at 0x103bd05c), the word lists and the text (Filter), or the file's
characters as the read fake's queue, the open, close, mfile, rdbuf and mask answers and a list (LoadWordList). What is compared:
the answer, the text or the list after the call, and the event log (every fake's arguments, the stream's state at every destructor).

No fused operations. Not tested: a word whose first character is a separator (the original loops until it crashes), a file whose line
is longer than the Mac's 0xac byte buffer (it overruns the frame).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SwearFilter_Filter"
MASK = 0xFFFFFFFF

FN_FILTER, FN_LOAD = 0x1020d2cc, 0x1020cd08
SIZES = {FN_FILTER: 888, FN_LOAD: 980}
OWN = set(SIZES)

STRISTR = 0x1020cbec
WCSNICMP = 0x1023a660
IFS_CTOR, FB_DTOR, IS_DTOR, IOS_DTOR = 0x1020801c, 0x100700e4, 0x102043a8, 0x1006ff50
FB_OPEN, FB_CLOSE, IS_READ = 0x10071c44, 0x10071b54, 0x1020d6b4
WSTR_CTOR, WSTR_DTOR, LIST_INSERT = 0x10146a14, 0x101468f8, 0x1020deec
RC_CTOR, THROW = 0x100026ac, 0x10238680

# The emulator resolves the data import __wctype_map (TOC slot 0x10260774, the image's 0x103bd05c) to 0x7d3f6400; the table is written there.
ADDR = dict(map=0x7d3f6400)
IFS_VTBL, FAIL_VTBL, DTOR_VEC = 0x102b1778, 0x1029d084, 0x102661d0
HOST_LAYOUT = [512]

TEXTBASE = [0]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def fnv(units):
    h = 2166136261
    for c in units:
        h = ((h ^ c) * 16777619) & MASK
    return h & 0x7FFFFFFF


def low(c):
    return c + 32 if 65 <= c <= 90 else c


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rd16(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def rd16z(e, a):
    out = []
    while True:
        c = rd16(e, a + 2 * len(out))
        if c == 0:
            return out
        out.append(c)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def r32(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def cbytes(e, a):
    s = e.cstr(a)
    return s.encode("latin-1") if isinstance(s, str) else bytes(s)


def glue(fn):
    def wrapped(e):
        w32(e, e.r(1) + 0x14, e.r(2))        # the glue's own TOC save, which the caller reloads
        fn(e)
    return wrapped


def off(p):
    return (p - TEXTBASE[0]) // 2


def stub_stristr(e):
    h, n = e.r(3), e.r(4)
    hs, ns = rd16z(e, h), rd16z(e, n)
    found = -1
    if not ns:
        found = 0
    else:
        for i in range(len(hs)):
            if all(i + j < len(hs) and low(hs[i + j]) == low(ns[j]) for j in range(len(ns))):
                found = i
                break
    e.cl.rec("stristr", (off(h), fnv(hs), fnv(ns), -1 if found < 0 else off(h) + found))
    e.set_r(3, 0 if found < 0 else h + 2 * found)


def stub_wcsnicmp(e):
    p, q, n = e.r(3), e.r(4), e.r(5)
    res = 0
    for i in range(n):
        a, b = low(rd16(e, p + 2 * i)), low(rd16(e, q + 2 * i))
        if a != b:
            res = a - b
            break
        if a == 0:
            break
    e.cl.rec("wcsnicmp", (off(p), fnv([rd16(e, q + 2 * i) for i in range(n)]), n, res))
    e.set_r(3, res & MASK)


def stub_ifs_ctor(e):
    L, this = e.cl, e.r(3)
    L.rec("IfsCtor", (s32(e.r(4)),))
    ios = this + 0x4c
    w32(e, this, ios)
    w32(e, this + 4, 0)
    w32(e, this + 8, 0)
    w32(e, this + 0x30, 0)
    w32(e, ios, 0)
    w32(e, ios + 0x24, L.ret("rdbuf"))
    w32(e, ios + 0x34, 0)
    w32(e, ios + 0x38, L.ret("mask"))
    w32(e, ios + 0x44, 0)


def stub_fb_dtor(e):
    e.cl.rec("FbDtor", (s16(e.r(4) & 0xFFFF), r32(e, e.r(3) + 0x24)))


def stub_is_dtor(e):
    this = e.r(3)
    ios = r32(e, this)
    e.cl.rec("IsDtor", (s16(e.r(4) & 0xFFFF), r32(e, ios + 0x34), int(r32(e, this + 8) == IFS_VTBL),
                        int(r32(e, ios) == IFS_VTBL + 0xc), s32(r32(e, ios + 0x44))))


def stub_ios_dtor(e):
    e.cl.rec("IosDtor", (s16(e.r(4) & 0xFFFF), r32(e, e.r(3) + 0x34)))


def stub_open(e):
    L, this = e.cl, e.r(3)
    L.rec("Open", (s32(e.r(5)), fnv(list(cbytes(e, e.r(4))))))
    w32(e, this + 0x24, L.ret("mfile"))
    e.set_r(3, this if L.ret("open") else 0)


def stub_close(e):
    L, this = e.cl, e.r(3)
    L.rec("Close", ())
    e.set_r(3, this if L.ret("close") else 0)


def stub_read(e):
    L, this = e.cl, e.r(3)
    v = L.ret("rd")
    L.rec("Read", (s32(e.r(5)), v))
    e.write(e.r(4), bytes([(v >> 8) & 0xFF, v & 0xFF]))
    if v & 0x10000:
        ios = r32(e, this)
        w32(e, ios + 0x34, r32(e, ios + 0x34) | 2)


def make_rep(e, chars):
    n = len(chars)
    rep = e.alloc(16 + 2 * (n + 1))
    w32(e, rep, n)
    w32(e, rep + 4, n)
    w32(e, rep + 8, 1)
    w32(e, rep + 0xc, rep + 16)
    e.write(rep + 16, struct.pack(">%dH" % (n + 1), *(chars + [0])))
    return rep


def stub_wstr_ctor(e):
    w32(e, e.r(3), make_rep(e, rd16z(e, e.r(4))))


def stub_list_insert(e):
    res, lst, pos, strp = e.r(3), e.r(4), e.r(5), e.r(6)
    node = e.alloc(12)
    prev = r32(e, pos)
    w32(e, node, prev)
    w32(e, node + 4, pos)
    w32(e, node + 8, r32(e, strp))
    w32(e, prev + 4, node)
    w32(e, pos, node)
    w32(e, lst, r32(e, lst) + 1)
    w32(e, res, node)


def stub_rc_ctor(e):
    w32(e, e.r(3), e.r(4))
    w32(e, e.r(3) + 4, 0)


def stub_throw(e):
    obj = e.r(4)
    e.cl.rec("Throw", (fnv(list(cbytes(e, e.r(3)))), int(r32(e, obj) == FAIL_VTBL), int(e.r(5) == DTOR_VEC),
                       fnv(list(cbytes(e, r32(e, obj + 4))))))


STUBS.internal(STRISTR, stub_stristr)
STUBS.internal(WCSNICMP, glue(stub_wcsnicmp))
STUBS.internal(IFS_CTOR, stub_ifs_ctor)
STUBS.internal(FB_DTOR, stub_fb_dtor)
STUBS.internal(IS_DTOR, stub_is_dtor)
STUBS.internal(IOS_DTOR, stub_ios_dtor)
STUBS.internal(FB_OPEN, stub_open)
STUBS.internal(FB_CLOSE, stub_close)
STUBS.internal(IS_READ, stub_read)
STUBS.internal(WSTR_CTOR, stub_wstr_ctor)
STUBS.internal(WSTR_DTOR, lambda e: None)
STUBS.internal(LIST_INSERT, stub_list_insert)
STUBS.internal(RC_CTOR, stub_rc_ctor)
STUBS.internal(THROW, glue(stub_throw))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def h16(self, a, vs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vs), *vs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def cstring(self, text):
        a = self.alloc(len(text) + 1)
        self.write(a, text.encode() + b"\0")
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    # strings and lists in the Mac's layout
    def put_str(self, chars):
        rep = make_rep(self.e, list(chars))
        so = self.alloc(4)
        self.words(so, [rep])
        return so

    def text_base(self, so):
        return self.rd_word(self.rd_word(so) + 0xc)

    def get_str(self, so):
        rep = self.rd_word(so)
        n = self.rd_word(rep)
        data = self.rd_word(rep + 0xc)
        return list(struct.unpack(">%dH" % n, self.read(data, 2 * n)))

    def append(self, lst, so):
        sent = lst + 4
        last = self.rd_word(sent)
        node = self.alloc(12)
        self.words(node, [last, sent, self.rd_word(so)])
        self.words(last + 4, [node])
        self.words(sent, [node])
        self.words(lst, [self.rd_word(lst) + 1])

    def mk_list(self, items):
        lst = self.alloc(12)
        self.words(lst, [0, lst + 4, lst + 4])
        for it in items:
            self.append(lst, self.put_str(it))
        return lst

    def mk_filter(self, words, allowed):
        f = self.alloc(0x18)
        self.words(f, [0, f + 4, f + 4, 0, f + 0x10, f + 0x10])
        for base, items in ((0, words), (0xc, allowed)):
            for it in items:
                self.append(f + base, self.put_str(it))
        return f

    def list_items(self, lst):
        out = []
        node = self.rd_word(lst + 8)
        while node != lst + 4:
            rep = self.rd_word(node + 8)
            n = self.rd_word(rep)
            data = self.rd_word(rep + 0xc)
            out.append(list(struct.unpack(">%dH" % n, self.read(data, 2 * n))))
            node = self.rd_word(node + 4)
        return out

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_swear_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_swear_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    @staticmethod
    def arr(chars):
        return (C.c_uint16 * max(len(chars), 1))(*chars)

    def put_str(self, chars):
        return self.lib.tpw_swear_new_str(self.arr(list(chars)), len(chars))

    def text_base(self, so):
        return self.lib.tpw_swear_str_data(so)

    def get_str(self, so):
        n = self.lib.tpw_swear_str_len(so)
        return list(struct.unpack("<%dH" % n, C.string_at(self.lib.tpw_swear_str_data(so), 2 * n)))

    def mk_list(self, items):
        lst = self.lib.tpw_swear_new_list()
        for it in items:
            self.lib.tpw_swear_list_push(lst, self.arr(it), len(it))
        return lst

    def mk_filter(self, words, allowed):
        f = self.lib.tpw_swear_new_filter()
        for which, items in ((0, words), (1, allowed)):
            for it in items:
                self.lib.tpw_swear_add(f, which, self.arr(it), len(it))
        return f

    def list_items(self, lst):
        out = []
        buf = (C.c_uint16 * 512)()
        for i in range(self.lib.tpw_swear_list_count(lst)):
            n = self.lib.tpw_swear_list_item(lst, i, buf)
            out.append(list(buf[:n]))
        return out

    def call(self, fn, args):
        if fn == FN_FILTER:
            return self.lib.tpw_swear_filter(args[0], args[1], args[2]) & 0xFF
        return self.lib.tpw_swear_load(args[0], args[1], args[2], args[3]) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_swear_"
    vp, u64, i32 = C.c_void_p, C.c_uint64, C.c_int32
    sig = {
        "addr": (u64, [C.c_char_p]), "alloc": (u64, [C.c_uint32]), "layout": (i32, [C.POINTER(C.c_int64)]),
        "new_filter": (u64, []), "add": (None, [u64, i32, vp, i32]), "new_str": (u64, [vp, i32]),
        "str_data": (u64, [u64]), "str_len": (i32, [u64]), "new_list": (u64, []), "list_push": (None, [u64, vp, i32]),
        "list_count": (i32, [u64]), "list_item": (i32, [u64, i32, vp]),
        "filter": (C.c_uint32, [u64, u64, i32]), "load": (C.c_uint32, [u64, u64, u64, i32]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_swear_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.h16(s.g("map"), case["table"])
    h = {}
    if case["op"] == "filter":
        h["f"] = s.mk_filter(case["words"], case["allowed"])
        h["text"] = s.put_str(case["text"])
        TEXTBASE[0] = s.text_base(h["text"])
    else:
        h["f"] = s.mk_filter([], [])
        h["path"] = s.cstring(case["path"])
        h["list"] = s.mk_list(case["items"])
    return h


def execute(s, case, h):
    if case["op"] == "filter":
        return s.call(FN_FILTER, [h["f"], h["text"], case["replace"]])
    return s.call(FN_LOAD, [h["f"], h["path"], h["list"], case["flag"]])


def observe(s, case, h, answer):
    out = {"answer": answer, "log": s.log()}
    if case["op"] == "filter":
        out["text"] = s.get_str(h["text"])
    else:
        out["items"] = s.list_items(h["list"])
    return out


def run(s, case):
    try:
        h = build(s, case)
        return observe(s, case, h, execute(s, case, h))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

LET = [ord(c) for c in "abcAB"]
TEXT = [ord(c) for c in "abcAB .,*-"] + [0x120]
SEP = [ord(c) for c in " .,-"]


def table(r):
    t = []
    for c in range(256):
        if (65 <= c <= 90) or (97 <= c <= 122):
            t.append(r.choice([0, 0x100, 0x80]))
        else:
            t.append(r.choice([0, 2, 4, 6, 0x16, 0x10, 0x200]))
    for c in SEP:
        t[c] = r.choice([2, 4, 6, 6, 6])
    return t


def rword(r):
    w = [r.choice(LET) for _ in range(r.randrange(0, 4))]
    return r.choice([[], [], [0x20], [0x2a]]) + w


def rtext(r):
    return [r.choice(TEXT) for _ in range(r.randrange(0, 14))]


def filter_case(r, words=None, allowed=None, text=None, replace=None, tbl=None):
    return {"op": "filter", "queues": {}, "table": table(r) if tbl is None else tbl,
            "words": [rword(r) for _ in range(r.randrange(0, 4))] if words is None else words,
            "allowed": [rword(r) for _ in range(r.randrange(0, 3))] if allowed is None else allowed,
            "text": rtext(r) if text is None else text, "replace": r.randrange(2) if replace is None else replace}


def o(s):
    return [ord(c) for c in s]


def filter_edges():
    r = random.Random(9401)
    base = table(r)
    for words, allowed, text, rep in (
        ([], [], o("abc"), 1),
        ([o("")], [], o("abc"), 1),
        ([o(" ")], [], o("abc"), 1),
        ([o("ab")], [], o("ab"), 1),
        ([o("ab")], [], o("ab"), 0),
        ([o("ab")], [], o("xx ab yy"), 1),
        ([o("ab")], [], o("cab ab"), 1),
        ([o("ab")], [], o("abab ab"), 1),
        ([o("ab")], [o("ab")], o("ab"), 1),
        ([o("ab")], [o("abc")], o("abc ab"), 1),
        ([o(" ab")], [], o("ab"), 1),
        ([o(" ab")], [], o("cab ab"), 1),
        ([o(" ab")], [], o("c ab c"), 1),
        ([o(" ab")], [], o("c.abb"), 0),
        ([o(" ab")], [o("ab")], o("ab.abc"), 1),
        ([o("*ab")], [], o("cabab"), 1),
        ([o("*ab")], [], o("cabab"), 0),
        ([o("*ab")], [o("ab")], o("cabab"), 1),
        ([o("*ab")], [o("aba")], o("abab"), 1),
        ([o("*")], [], o("abab"), 1),
        ([o("AB"), o("*c"), o(" b")], [o("c")], o("ab cab Ab.b,c-ab"), 1),
        ([o("ab"), o("b")], [], o("a b ab bab"), 1),
        ([o("ab")], [], [0x61, 0x120, 0x62, 0x20, 0x61, 0x62, 0x120], 1),
    ):
        yield filter_case(r, words, allowed, text, rep, base)


def filter_random(r):
    return filter_case(r)


PATHS = ["data:swear.dat", "words", "a:b:c.txt"]


def encoded(chars):
    return [(~c) & 0xFFFF for c in chars]


def load_case(r, flag=None, lines=None, **kw):
    rd = []
    if lines is None:
        lines = []
        for _ in range(r.randrange(0, 4)):
            k = r.random()
            if k < 0.15:
                lines.append(("raw", [0x2300 | r.randrange(256)] + [r.randrange(0x41, 0x60) for _ in range(r.randrange(0, 4))]))
            elif k < 0.25:
                lines.append(("raw", []))
            else:
                lines.append(("raw", encoded([r.choice(TEXT) for _ in range(r.randrange(1, 8))])))
    for kind, chars in lines:
        rd.extend(chars)
        rd.append(0x0a)
    if r.random() < 0.2:
        rd.extend(r.choice([[0x0d], [0xFFFE, 0x0d, 0x0a]]))
    if not rd:
        rd = [0x0a]
    if r.random() < 0.15:
        rd.append(0xFFF0)       # a line the file leaves unterminated
    rd[-1] |= 0x10000
    q = {"rdbuf": [r.choice([0, 1, 7])], "mask": [r.choice([0, 0, 1, 4, 5, 7])], "mfile": [r.choice([0, 0x1234, 0x1234])],
         "open": [r.choice([0, 1, 1])], "close": [r.choice([0, 1, 1])], "rd": rd}
    q.update(kw)
    return {"op": "load", "queues": q, "table": [0] * 256, "path": r.choice(PATHS),
            "flag": r.randrange(2) if flag is None else flag,
            "items": [[r.randrange(1, 0x7f) for _ in range(r.randrange(0, 4))] for _ in range(r.randrange(0, 2))]}


def load_edges():
    r = random.Random(9402)
    inv = encoded(o("bad"))
    yield load_case(r, 0)
    yield load_case(r, 1, [("raw", inv)], rdbuf=[1], mask=[0], mfile=[0x1234], open=[1], close=[1])
    yield load_case(r, 1, [("raw", inv), ("raw", encoded(o("x")))], rdbuf=[1], mask=[0], mfile=[0x1234], open=[1], close=[1])
    yield load_case(r, 1, [("raw", [0x2301, 0x41]), ("raw", inv)], rdbuf=[1], mask=[0], mfile=[0x1234], open=[1], close=[1])
    yield load_case(r, 1, [("raw", [])], rdbuf=[1], mask=[0], mfile=[0x1234], open=[1], close=[1])
    # a line ended by 0x000d: copied up to it, the 0x000a comes after
    yield load_case(r, 1, [("raw", inv + [0x0d])], rdbuf=[1], mask=[0], mfile=[0x1234], open=[1], close=[1])
    for rdbuf in (0, 1):
        for mask in (0, 1, 4, 5):
            for opn in (0, 1):
                for mfile in (0, 0x1234):
                    yield load_case(r, 1, [("raw", inv)], rdbuf=[rdbuf], mask=[mask], mfile=[mfile], open=[opn], close=[1])
            for cls in (0, 1):
                yield load_case(r, 1, [("raw", inv)], rdbuf=[rdbuf], mask=[mask], mfile=[0x1234], open=[1], close=[cls])


def load_random(r):
    return load_case(r)


TESTS = [
    FnTest("SwearFilter::Filter", ppc, host, edge=filter_edges, rnd=filter_random),
    FnTest("SwearFilter::LoadWordList", ppc, host, edge=load_edges, rnd=load_random),
]
