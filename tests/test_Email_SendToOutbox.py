"""Unit v5162: Email::SendToOutbox and Email::BuildBody, against libtpw_unit_Email_SendToOutbox.dylib (tests/unit_Email_SendToOutbox.cpp), main app
("SimTheme Park"), src/game/online/EmailMessage.cpp:
  Email::SendToOutbox 0x10203010, Email::BuildBody 0x102031a8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The Email is a block of words (strings are words
that point to a representation whose word at +0xc points to the characters; the address list is a ring of nodes around the sentinel at
+8). The callees outside the unit are fakes on both sides that record their arguments: the string assign, append, constructor and
destructor, the list erase and insert, Outbox::WriteMailData and UTimer::GetAbsolute. strlen and sprintf are the C library's.

The world of a case: the Email (sender, subject, addresses, ids, the two UTF-16 names, an optional attachment), an Outbox block (the host
one is only an address to name the fields), the timer value and the answer of WriteMailData. What is compared: the answer, the event log
(every appended text), the characters of every string of the Email (BuildBody cuts them at a CR or LF) and the three numbers.

No fused operations. Not tested: texts that overflow the 132 byte line buffer (the original overruns its stack there), and the list erase's
iterators (the host has none: the stub checks that the original passes the first node and the sentinel).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Email_SendToOutbox"
MASK = 0xFFFFFFFF

FN_SEND, FN_BUILD = 0x10203010, 0x102031A8
SIZES = {FN_SEND: 360, FN_BUILD: 1904}
OWN = set(SIZES)

A_ERASE, A_ASSIGN, A_APPEND, A_CTOR, A_INSERT, A_DTOR = 0x102084D8, 0x101B2440, 0x1020100C, 0x10002338, 0x101B25E0, 0x1000221C
A_WRITE, A_TIMER = 0x10205154, 0x10239568

HOST_LAYOUT = [0x4C, 0xC, 0x38, 0x2C]

CUR = {"email": 0, "outbox": 0, "ctor": 0, "text": "", "time": 0}
DUMPED = []


def s16(v):
    return T.s16(v)


def fid(a):
    eb, ob = CUR["email"], CUR["outbox"]
    if eb <= a < eb + 0x4C:
        return a - eb
    if ob and ob <= a < ob + 0x60:
        return 0x1000 + a - ob
    return -2


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_assign(e):
    e.cl.rec("StringAssign", (fid(e.r(3)), e.r(5)), (e.cstr(e.r(4)),))


def stub_append(e):
    e.cl.rec("StringAppend", (fid(e.r(3)), e.r(5)), (e.cstr(e.r(4)),))


def stub_dtor(e):
    e.cl.rec("StringDtor", (s16(e.r(4)), 1 if e.r(3) == CUR["ctor"] else 0), ())


def stub_erase(e):
    lst = e.r(4)
    e.cl.rec("ListErase", (fid(lst), 1 if e.r(5) == e.u32(lst + 8) else 0, 1 if e.r(6) == lst + 4 else 0), ())


def stub_ctor(e):
    CUR["ctor"] = e.r(3)
    CUR["text"] = e.cstr(e.r(4))
    e.cl.rec("StringCtor", (), (CUR["text"],))


def stub_insert(e):
    lst = e.r(4)
    e.cl.rec("ListInsert", (fid(lst), 1 if e.r(5) == lst + 4 else 0, 1 if e.r(6) == CUR["ctor"] else 0), (CUR["text"],))


def stub_write(e):
    e.cl.rec("WriteMailData", (0,), ())
    e.set_r(3, e.cl.ret("WriteMailData") & MASK)


def stub_timer(e):
    t = CUR["time"]
    e.write(e.r(3), struct.pack(">II", t >> 32, t & MASK))
    e.cl.rec("GetAbsolute", (), ())


def stub_sprintf(e):
    """sprintf(buffer, format, ...) with %s %d %x (width and zero fill); the arguments are r5, r6, ... (no floats)."""
    fmt = e.cstr(e.r(4))
    out, i, reg = "", 0, 5
    while i < len(fmt):
        ch = fmt[i]
        if ch != "%":
            out += ch
            i += 1
            continue
        j = i + 1
        while fmt[j] in "0123456789":
            j += 1
        spec, conv = fmt[i + 1:j], fmt[j]
        arg = e.r(reg)
        reg += 1
        if conv == "s":
            out += e.cstr(arg)
        elif conv == "d":
            out += ("%" + spec + "d") % T.s32(arg)
        elif conv == "x":
            out += ("%" + spec + "x") % (arg & MASK)
        else:
            raise RuntimeError("sprintf conversion " + conv)
        i = j + 1
    e.write(e.r(3), out.encode("latin-1") + b"\0")
    e.set_r(3, len(out))


def stub_strlen(e):
    e.set_r(3, len(e.cstr(e.r(3))))


STUBS.internal(0x10238650, stub_strlen)
STUBS.internal(0x10238578, stub_sprintf)
STUBS.internal(A_ASSIGN, stub_assign)
STUBS.internal(A_APPEND, stub_append)
STUBS.internal(A_DTOR, stub_dtor)
STUBS.internal(A_ERASE, stub_erase)
STUBS.internal(A_CTOR, stub_ctor)
STUBS.internal(A_INSERT, stub_insert)
STUBS.internal(A_WRITE, stub_write)
STUBS.internal(A_TIMER, stub_timer)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    mac = True

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def p(self, a):
        return a

    def note(self, email, outbox):
        CUR.update(email=email, outbox=outbox, ctor=0, text="", time=self.time)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_send(self, email, outbox):
        r3, _, _ = T.call_ppc(self.e, FN_SEND, [email, outbox])
        return r3 & MASK

    def call_build(self, email, hi, lo):
        T.call_ppc(self.e, FN_BUILD, [email])
        return 0


class Host(Side):
    fmt = "<I"
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_se_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def p(self, a):
        return a - self.base

    def note(self, email, outbox):
        pass

    def alloc(self, n):
        return self.lib.tpw_se_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_send(self, email, outbox):
        return self.lib.tpw_se_send(email, outbox) & MASK

    def call_build(self, email, hi, lo):
        self.lib.tpw_se_build(email, hi, lo)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_se_alloc.restype, lib.tpw_se_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_se_base.restype, lib.tpw_se_base.argtypes = C.c_uint64, []
    lib.tpw_se_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_se_send.restype, lib.tpw_se_send.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_se_build.restype, lib.tpw_se_build.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_se_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def mkstr(s, text):
    """A string object: representation {size, 0, 0, characters}; returns (the word, the characters' address, the length)."""
    data = s.alloc(len(text) + 1)
    s.write(data, text + b"\0")
    rep = s.alloc(16)
    s.words(rep, [len(text), 0, 0, s.p(data)])
    return s.p(rep), data, len(text)


def mkwstr(s, chars):
    """A UTF-16 string: None is the empty one (size 0); else the high-byte-zero pairs of chars, then a 0 0 end."""
    if chars is None:
        data = s.alloc(4)
        rep = s.alloc(16)
        s.words(rep, [0, 0, 0, s.p(data)])
        return s.p(rep)
    raw = b"".join(bytes(pair) for pair in chars) + b"\0\0"
    data = s.alloc(len(raw) + 4)
    s.write(data, raw)
    rep = s.alloc(16)
    s.words(rep, [len(raw), 0, 0, s.p(data)])
    return s.p(rep)


def build(s, case):
    s.time = case["time"]
    em = s.alloc(0x4C)
    ow = s.alloc(0x400)
    s.note(em, ow)
    s.strs = []

    def st(text):
        w, d, n = mkstr(s, text)
        s.strs.append((d, n))
        return w

    nodes = []
    for t in case["to"]:
        nodes.append(st(t))
    sentinel = s.p(em) + 8
    addrs = [s.alloc(12) for _ in nodes]
    for i, a in enumerate(addrs):
        nxt = s.p(addrs[i + 1]) if i + 1 < len(addrs) else sentinel
        prv = s.p(addrs[i - 1]) if i else sentinel
        s.words(a, [prv, nxt, nodes[i]])
    first = s.p(addrs[0]) if addrs else sentinel
    last = s.p(addrs[-1]) if addrs else sentinel
    att = 0
    if case["att"] is not None:
        a = case["att"]
        name, typ = st(a["name"]), st(a["typ"])
        dw = 0
        if a["data"] is not None:
            dd = s.alloc(len(a["data"]) + 1)
            s.write(dd, a["data"] + b"\0")
            s.strs.append((dd, len(a["data"])))
            dw = s.p(dd)
        att = s.alloc(12)
        s.words(att, [name, typ, dw])
        att = s.p(att)
    s.words(em, [st(case["from"]), len(nodes), last, first, st(case["subj"]), st(case["body"]),
                 case["pid"], case["cid"], mkwstr(s, case["w20"]), mkwstr(s, case["w24"]), case["lang"], att])
    if s.mac:
        s.words(ow + 0x14, [case["ob14"]])
    s.em, s.ow = em, ow


def execute(s, case):
    if case["op"] == "send":
        return s.call_send(s.em, 0 if case["null"] else s.ow)
    t = case["time"]
    return s.call_build(s.em, t >> 32, t & MASK)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "strs": [s.read(d, n + 1) for d, n in s.strs],
            "nums": s.rd_words(s.em + 0x18, 2) + s.rd_words(s.em + 0x28, 1)}


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


# ---------------------------------------------------------------------------------------------------------------- cases

ALPHA = bytes(range(0x20, 0x7F))


def rtext(r, n=None, crlf=0.15):
    n = r.choice([0, 1, 2, 5, 12, 30, 60]) if n is None else n
    out = bytearray(r.choice(ALPHA) for _ in range(n))
    for _ in range(n):
        if out and r.random() < crlf / 4:
            out[r.randrange(len(out))] = r.choice([10, 13])
    return bytes(out)


def rwname(r):
    n = r.choice([0, 1, 2, 3, 7, 12])
    if n == 0:
        return None if r.random() < 0.6 else []
    return [(0, r.randrange(1, 0x7F)) for _ in range(n)]


def base(r, op):
    c = {"op": op, "null": False, "time": r.choice([0, 999, 1000, 1001, r.getrandbits(40), r.getrandbits(64), (1 << 63), (1 << 64) - 1, r.getrandbits(32) * 1000]),
         "from": rtext(r, crlf=0.4), "subj": rtext(r, crlf=0.4), "body": rtext(r, crlf=0.0),
         "to": [rtext(r, r.choice([0, 1, 8, 20]), crlf=0.4) for _ in range(r.choice([0, 1, 1, 2, 3, 5]))],
         "pid": T.s32(r.choice([0, 1, -1, r.getrandbits(32), 0x7FFFFFFF])), "cid": T.s32(r.choice([0, 1, -1, r.getrandbits(32), 0x80000000])),
         "lang": T.s32(r.choice([0, 1, 9, -1, r.getrandbits(32)])), "w20": rwname(r), "w24": rwname(r),
         "att": None, "ob14": r.getrandbits(32) | 1,
         "queues": {"WriteMailData": [r.choice([0, 1, -1 & MASK, r.getrandbits(32)])]}}
    if op == "build" and r.random() < 0.6:
        c["att"] = {"name": rtext(r, r.choice([0, 5, 20, 30])), "typ": rtext(r, r.choice([0, 9, 25])),
                    "data": None if r.random() < 0.2 else rtext(r, r.choice([0, 1, 40, 110]), crlf=0.0)}
    return c


def send_edges():
    r = random.Random(5162)
    for to in ([], [b""], [b"a@b.c"], [b"a@b.c", b"d@e.f", b"x"]):
        for null in (False, True):
            c = base(r, "send")
            c["to"], c["null"] = to, null
            yield c


def build_edges():
    r = random.Random(1625)
    for t in (0, 999, 1000, 1999, 2000, (1 << 32) * 1000 + 5, (1 << 63), (1 << 64) - 1, 0x1234567890ABCDEF):
        c = base(r, "build")
        c["time"] = t
        yield c
    for to in ([], [b"a@b.c"], [b"a\r\nb", b"\ncc", b"", b"z\r"]):
        for att in (None, {"name": b"pic.jpg", "typ": b"image/jpeg", "data": b"QUJD"}, {"name": b"", "typ": b"", "data": None},
                    {"name": b"n", "typ": b"t", "data": b""}):
            c = base(r, "build")
            c["to"], c["att"] = to, att
            yield c
    for w in (None, [], [(0, 0x41)], [(1, 0x41)], [(0, 0x41), (0, 0x42), (0, 0x43)], [(0, 0x41), (2, 0x42), (0, 0x43)], [(0, 0xE9)]):
        c = base(r, "build")
        c["w20"], c["w24"] = w, w
        yield c
        c = base(r, "build")
        c["w20"], c["w24"] = w, None
        yield c
        c = base(r, "build")
        c["w20"], c["w24"] = None, w
        yield c
    for s in (b"", b"\r", b"\n", b"\r\nabc", b"abc\r\ndef", b"abc"):
        c = base(r, "build")
        c["from"], c["subj"] = s, s
        yield c


TESTS = [
    FnTest("Email::SendToOutbox", ppc, host, edge=send_edges, rnd=lambda r: base(r, "send")),
    FnTest("Email::BuildBody", ppc, host, edge=build_edges, rnd=lambda r: base(r, "build")),
]
