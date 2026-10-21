"""Unit v5912, Email::Email (the constructor) and Email::Callback (the mail connection's callback), against
libtpw_unit_Email_Callback_2.dylib (tests/unit_Email_Callback_2.cpp), main app ("SimTheme Park"), src/game/online/EmailMessage.cpp:
  Email::Callback 0x10202a54, Email::Email 0x10202244

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory for the original, host memory for the
translation). Callees outside the unit are fakes on both sides that record the same events: the string, wstring and list constructors
(they write zero words), the substring constructor and assign (they copy the source's string word), string assign and destructor, list
erase, operator delete and delete[], Mail::DoTo/DoFrom/DoData/Quit/DoReset (with the address text), the callback glue (the TVector, two
arguments), Email::BuildBody, Outbox::NextMail (answers from the case's queue) and WSAStartup.

The world of a callback case: an e-mail block (junk filled), up to three list nodes with string objects, a mail, an outbox, an
attachment block and its data; the state word, the cursor (the unaligned word at +0x3e), the flags and the callback word. What is
compared: the e-mail words (pointers as labels), the flag bytes, the cursor, the nodes, attachment and mail blocks and the event log.
Pointer words are compared as labels because the Mac holds addresses and the host arena offsets.

No fused operations. Not tested: a NULL mail or outbox where the original passes them on (the fakes would log them, but the original
dereferences the outbox), a cursor outside the list.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Email_Callback_2"
MASK = 0xFFFFFFFF

FN_CB, FN_CT = 0x10202a54, 0x10202244
SIZES = {FN_CB: 1124, FN_CT: 260}
OWN = set(SIZES)

TEXT_ADDR = 0x1024faeb
ADDR = {}
HOST_LAYOUT = [0x48, 0x4c, 0x34, 0x38, 0x44, 0x0c]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e, a):
    base = e.cl.email
    return a - base if base <= a < base + 0x4c else -1


def mk_init(name, n):
    def f(e):
        a = e.r(3)
        e.cl.rec(name, (off(e, a),))
        e.write(a, bytes(n))
    return f


def mk_sub(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))
        e.write(e.r(3), e.read(e.r(4), 4))
    return f


def stub_ptr_glue(e):
    e.cl.rec("PtrGlue", (e.r(12) & MASK, s32(e.r(3)), s32(e.r(4))))


def stub_wsa(e):
    e.cl.rec("WSAStartup", (s32(e.r(3)),))
    e.set_r(3, 0)


def stub_strlen(e):
    e.set_r(3, len(e.cstr(e.r(3))))


STUBS.internal(0x10238650, stub_strlen)
STUBS.internal(0x1007363c, mk_init("StringInit", 4))
STUBS.internal(0x101e0b84, mk_init("WStringInit", 4))
STUBS.internal(0x10208abc, mk_init("ListInit", 12))
STUBS.internal(0x101b2918, mk_sub("StringSubCtor"))
STUBS.internal(0x10001c80, mk_sub("StringSubAssign"))
STUBS.internal(0x1023baa8, stub_ptr_glue)
STUBS.internal(0x102394f0, stub_wsa)
STUBS.internal(0x10239538, CL.recorder("DoTo", "Ls"))
STUBS.internal(0x10239550, CL.recorder("DoFrom", "Ls"))
STUBS.internal(0x10239580, CL.recorder("DoData", "Ls"))
STUBS.internal(0x10239598, CL.recorder("Quit", "L"))
STUBS.internal(0x102395b0, CL.recorder("DoReset", "L"))
STUBS.internal(0x10238218, CL.recorder("OperatorDelete", "L"))
STUBS.internal(0x10238638, CL.recorder("OperatorDeleteArray", "L"))
STUBS.internal(0x101b2440, CL.recorder("StringAssign", "Lsu"))
STUBS.internal(0x1000221c, CL.recorder("StringDtor", "Lh"))
STUBS.internal(0x102084d8, CL.recorder("ListErase", "xLLL"))
STUBS.internal(0x102031a8, CL.recorder("BuildBody", "L"))
STUBS.internal(0x10205ed0, CL.recorder("NextMail", "LL", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make_objects(self):
        self.email = self.alloc(0x4c)
        self.mail = self.alloc(16)
        self.att = self.alloc(12)
        self.data = self.alloc(16)
        self.outbox = self.alloc(16)
        self.nodes = [self.alloc(12) for _ in range(3)]
        e = self.email
        pairs = [(1, e), (6, e + 4), (4, e + 8), (5, e + 0x38), (2, self.mail), (3, self.att), (8, self.att + 4),
                 (7, self.data), (9, self.outbox)]
        for i, n in enumerate(self.nodes):
            pairs += [(10 + i, n), (20 + i, n + 8)]
        self.pairs = pairs
        self.names = {self.enc(a): "obj%d" % lab for lab, a in pairs}

    def string(self, text):
        """A string object (character pointer at +0xc) and its characters; answers the string word."""
        obj = self.alloc(16)
        chars = self.alloc(len(text) + 1)
        self.write(chars, text.encode() + b"\0")
        self.words(obj + 0xc, [self.enc(chars)])
        w = self.enc(obj)
        self.names[w] = "str:" + text
        return w

    def norm(self, w):
        return self.names.get(w, w)

    def norms(self, ws):
        return [self.norm(w) for w in ws]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        self.L = e.cl = CL.PpcLog({a: lab for lab, a in self.pairs}, case["queues"])
        self.L.email = self.email
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def enc(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_cb(self, msg, code):
        T.call_ppc(self.e, FN_CB, [self.email, msg & MASK, code & MASK, 0])
        return 0

    def call_ct(self):
        r3, _, _ = T.call_ppc(self.e, FN_CT, [self.email])
        return self.norm(r3 & MASK)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        CL.host_begin(lib, {lab: a for lab, a in self.pairs}, case["queues"])

    def done(self):
        pass

    def enc(self, a):
        return self.lib.tpw_ecb_off(a)

    def alloc(self, n):
        return self.lib.tpw_ecb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_cb(self, msg, code):
        self.lib.tpw_ecb_callback(self.email, msg & MASK, s32(code))
        return 0

    def call_ct(self):
        return self.norm(self.enc(self.lib.tpw_ecb_ctor(self.email)))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ecb_"
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "callback").restype, getattr(lib, p + "callback").argtypes = None, [C.c_uint64, C.c_uint32, C.c_int32]
    getattr(lib, p + "ctor").restype, getattr(lib, p + "ctor").argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

JUNK = 0xA5A5A5A5


def build(s, case):
    e = s.email
    s.words(e, [JUNK] * 19)
    s.words(s.mail, [JUNK] * 4)
    s.words(s.att, [JUNK] * 3)
    s.words(s.data, [JUNK] * 4)
    for n in s.nodes:
        s.words(n, [JUNK] * 3)
    if case["op"] == "ct":
        s.words(e, [case["junk"]] * 19)
        return
    t = case["texts"]
    sentinel = s.enc(e + 8)
    nodes = [s.enc(n) for n in s.nodes[:case["n"]]]
    for i, n in enumerate(s.nodes[:case["n"]]):
        prev = nodes[i - 1] if i > 0 else sentinel
        nxt = nodes[i + 1] if i + 1 < len(nodes) else sentinel
        s.words(n, [prev, nxt, s.string(t[4 + i])])
    first = nodes[0] if nodes else sentinel
    last = nodes[-1] if nodes else sentinel
    s.words(e, [s.string(t[0]), case["n"], last, first, s.string(t[2]), s.string(t[3])])
    s.words(e + 0x30, [case["state"] & MASK, s.enc(s.mail), s.string(t[1])])
    s.write(e + 0x3c, bytes([case["f3c"], case["f3d"]]))
    cursors = nodes + [sentinel]
    cur = cursors[case["cursor"] % len(cursors)]
    s.write(e + 0x3e, struct.pack(s.fmt, cur))
    s.write(e + 0x42, bytes([0x5A, 0xA5]))
    s.words(e + 0x2c, [s.enc(s.att) if case["att"] else 0])
    s.words(e + 0x44, [s.enc(s.outbox), case["cb"]])
    s.words(s.att, [JUNK, JUNK, s.enc(s.data) if case["data"] else 0])


def execute(s, case):
    return s.call_cb(case["msg"], case["code"]) if case["op"] == "cb" else s.call_ct()


def observe(s, case, answer):
    e = s.email
    out = {"answer": answer, "log": s.log()}
    out["words"] = s.norms(s.rd_words(e, 15) + s.rd_words(e + 0x40 + 4, 2))
    out["flags"] = s.read(e + 0x3c, 2)
    out["cursor"] = s.norm(struct.unpack(s.fmt, s.read(e + 0x3e, 4))[0])
    out["tail"] = s.read(e + 0x42, 2)
    out["nodes"] = [s.norms(s.rd_words(n, 3)) for n in s.nodes]
    out["att"] = s.norms(s.rd_words(s.att, 3))
    out["mail"] = s.norms(s.rd_words(s.mail, 4))
    out["data"] = s.norms(s.rd_words(s.data, 4))
    return out


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

LETTERS = "abcdefghij@.xyz"
CODES = [0xdd, 0xfa, 0x1a5, 0x1c2, 0x226, 0x227, 0x228, 0x229, 0x1a4, 0x1a6, 0x1c1, 0x1c3, 0x225, 0xdc, 0xde, 0xf9, 0xfb, 0, -1, 0x300]
MSGS = [0x78, 0x78, 0x78, 0x78, 0xFFFFFFFB, 0xFFFFFFFC, 0, 0x79, 0xFFFFFFFD, 0xFFFFFFFA]


def text(r):
    return "".join(r.choice(LETTERS) for _ in range(r.randrange(0, 12)))


def cb_case(r, **kw):
    c = {"op": "cb", "n": r.randrange(0, 4), "state": r.choice([-1, 0, 1, 2, 3, 4, 5, 0x100]), "cursor": r.randrange(4),
         "cb": r.choice([0, 0x10268718, r.getrandbits(32) | 1]), "f3c": r.choice([0, 1, 2]), "f3d": r.choice([0, 1, 2]),
         "att": r.choice([0, 1]), "data": r.choice([0, 1]), "texts": [text(r) for _ in range(7)],
         "msg": r.choice(MSGS), "code": r.choice(CODES)}
    c.update(kw)
    c["queues"] = {"NextMail": [kw.get("nm", r.choice([0, 1, 0x100, 0x101, 2, -1]))]}
    c.pop("nm", None)
    return c


def cb_edges():
    r = random.Random(5912)
    for msg in (0xFFFFFFFB, 0xFFFFFFFC):
        for cb in (0, 0x10268718):
            for f3c in (0, 1, 2):
                yield cb_case(r, msg=msg, cb=cb, f3c=f3c)
    for msg in (0, 0x79, 0xFFFFFFFD, 0x10078 & MASK):
        yield cb_case(r, msg=msg)
    for code in CODES:
        for cb in (0, 0x10268718):
            yield cb_case(r, msg=0x78, code=code, cb=cb)
    for state in (-1, 0, 1, 2, 3, 4, 5):
        for n in range(4):
            for cur in range(n + 1):
                yield cb_case(r, msg=0x78, code=0xfa, state=state, n=n, cursor=cur)
                yield cb_case(r, msg=0x78, code=0x228, state=state, n=n, cursor=cur)
    for f3d in (0, 1):
        for att in (0, 1):
            for data in (0, 1):
                for nm in (0, 1, 0x100, 0x101):
                    yield cb_case(r, msg=0x78, code=0xfa, state=3, f3d=f3d, att=att, data=data, nm=nm)


def cb_random(r):
    return cb_case(r)


def ct_case(r):
    return {"op": "ct", "junk": r.choice([JUNK, 0, 0xFFFFFFFF, r.getrandbits(8) * 0x01010101]), "queues": {"NextMail": [0]}}


def ct_edges():
    r = random.Random(7)
    for _ in range(4):
        yield ct_case(r)


def ct_random(r):
    return ct_case(r)


TESTS = [
    FnTest("Email::Callback", ppc, host, edge=cb_edges, rnd=cb_random),
    FnTest("Email::Email", ppc, host, edge=ct_edges, rnd=ct_random),
]
