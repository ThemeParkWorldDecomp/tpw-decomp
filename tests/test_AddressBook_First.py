"""Unit v51057, 22 small e-mail functions, against libtpw_unit_AddressBook_First.dylib (tests/unit_AddressBook_First.cpp), main app
("SimTheme Park"), src/game/online/email.cpp:
  AddressBook::First 0x10206534, Next 0x10206594, Outbox::UnmarkToSend 0x102058b4, UnmarkToDelete 0x102059a0, std::ios_base::exceptions
  0x10207664, operator|= 0x102076a0, state 0x10207700, operator& 0x10207734, rdstate 0x10207788, basic_ios::tie 0x10207964, good
  0x102079b4, istream::sentry::~sentry 0x10207f84, ~basic_istream 0x102043a8, the two list constructors 0x10208abc and 0x1020951c, the
  two list destructors 0x10203a4c and 0x1020236c, string operator< 0x10209f04, string begin 0x1020a068 and size 0x1020a0d4, mail_cbfn
  0x102029f4, CSock::~CSock 0x10201fcc.

A case is a world and a call. The world is one 0x400 byte block built twice by ONE piece of Python code (emulator memory big-endian for the
original, the host library's memory little-endian for the translation): random words with the structure each function walks written over
them (pointers are block offsets, resolved per side). The callees outside the unit (list remove and clear, operator delete,
ios_base::~ios_base, Sock::~Sock, string compare, Email::Callback) are fakes on both sides that log their arguments, pointers as the
offset from the block, and answer from the case's queue.

What is compared: the answer (pointers and global addresses made side independent), the whole block, the two list-init globals and the
event log. No fused operations. Not tested: a NULL object for the functions that never test it (the list constructors, accessors).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AddressBook_First"
MASK = 0xFFFFFFFF
BLOCK = 0x400

# name: (ppc address, size, host function suffix, answers)
OPS = {
    "first": (0x10206534, 52, True),
    "next": (0x10206594, 60, True),
    "unmark_send": (0x102058B4, 48, False),
    "unmark_delete": (0x102059A0, 48, False),
    "exceptions": (0x10207664, 8, True),
    "or_assign": (0x102076A0, 16, True),
    "state": (0x10207700, 8, True),
    "and": (0x10207734, 8, True),
    "rdstate": (0x10207788, 8, True),
    "tie": (0x10207964, 8, True),
    "good": (0x102079B4, 24, True),
    "sentry_dtor": (0x10207F84, 68, True),
    "istream_dtor": (0x102043A8, 152, True),
    "strlist_ctor": (0x10208ABC, 44, True),
    "uintlist_dtor": (0x10203A4C, 92, True),
    "uintlist_ctor": (0x1020951C, 44, True),
    "less": (0x10209F04, 140, True),
    "begin": (0x1020A068, 12, True),
    "size": (0x1020A0D4, 12, True),
    "mail_cbfn": (0x102029F4, 56, False),
    "strlist_dtor": (0x1020236C, 92, True),
    "csock_dtor": (0x10201FCC, 104, True),
}
OWN = set(v[0] for v in OPS.values())

REMOVE, CLEAR_UINT, CLEAR_STR, DELETE, IOS_DTOR, SOCK_DTOR, COMPARE, CALLBACK = (
    0x10208F44, 0x102090DC, 0x1017FC04, 0x10238218, 0x10238950, 0x1020E2D4, 0x10209CAC, 0x10202A54)

ADDR = dict(EmailBlob=0x10396AA8, OstreamVt1=0x1029D0B0, IfstreamVtblC=0x102B173C, ListStrInit=0x102B1794, ListUintInit=0x102B1790)
GLOBAL_SPAN = 0x40


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e, p):
    return 0 if p == 0 else (p - e.vbase) & MASK


def sum32(e, p, n):
    total = 0
    for byte in e.read(p, min(n, 32)):
        total = (total * 31 + byte) & MASK
    return total


def st_remove(e):
    e.cl.rec("Remove", (off(e, e.r(3)), e.u32(e.r(4))))


def st_clear_uint(e):
    e.cl.rec("ClearUint", (off(e, e.r(3)),))


def st_clear_str(e):
    e.cl.rec("ClearStr", (off(e, e.r(3)),))


def st_delete(e):
    e.cl.rec("Delete", (off(e, e.r(3)),))


def st_ios_dtor(e):
    e.cl.rec("IosDtor", (off(e, e.r(3)), e.r(4) & MASK))


def st_sock_dtor(e):
    e.cl.rec("SockDtor", (off(e, e.r(3)), e.r(4) & 0xFFFF))


def st_compare(e):
    L = e.cl
    L.rec("Compare", (off(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK, off(e, e.r(6)), e.r(7) & MASK, sum32(e, e.r(6), e.r(7) & MASK)))
    e.set_r(3, L.ret("Compare") & MASK)


def st_callback(e):
    e.cl.rec("Callback", (off(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK, off(e, e.r(6))))


for addr, fn in ((REMOVE, st_remove), (CLEAR_UINT, st_clear_uint), (CLEAR_STR, st_clear_str), (DELETE, st_delete),
                 (IOS_DTOR, st_ios_dtor), (SOCK_DTOR, st_sock_dtor), (COMPARE, st_compare), (CALLBACK, st_callback)):
    STUBS.internal(addr, fn)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def setbase(self, b):
        self.e.vbase = b

    def wbase(self, b):
        return b

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.e.cl.log

    def call(self, op, args):
        return T.call_ppc(self.e, OPS[op][0], [x & MASK for x in args])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51057_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v51057_alloc(n)

    def setbase(self, b):
        self.lib.tpw_v51057_setbase(b)

    def wbase(self, b):
        return b - self.lib.tpw_v51057_arena()

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        return getattr(self.lib, "tpw_v51057_" + op)(*[x & MASK for x in (list(args) + [0, 0, 0, 0])[:4]]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51057_addr.restype, lib.tpw_v51057_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51057_alloc.restype, lib.tpw_v51057_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51057_arena.restype, lib.tpw_v51057_arena.argtypes = C.c_uint64, []
    lib.tpw_v51057_setbase.restype, lib.tpw_v51057_setbase.argtypes = None, [C.c_uint64]
    for op in OPS:
        f = getattr(lib, "tpw_v51057_" + op)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 4
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def res(base, t):
    """A word of the case: an int, or ("p", offset) the address of that block offset."""
    return (base + t[1]) & MASK if isinstance(t, tuple) else t & MASK


def build(s, case):
    """Answers the block's word base (the arena offset on the host, the address on the PowerPC side)."""
    base = s.alloc(BLOCK)
    s.setbase(base)
    wb = s.wbase(base)
    s.words(base, [res(wb, t) for t in case["init"]])
    for o, t in case["set"].items():
        s.words(base + o, [res(wb, t)])
    for o, data in case.get("bytes", {}).items():
        s.write(base + o, data)
    for name, w in case["globals"].items():
        s.words(s.g(name), [w])
    return base


def norm(s, base, w):
    w &= MASK
    if base <= w < base + BLOCK:
        return ("b", w - base)
    for name in ADDR:
        d = w - (s.g(name) & MASK)
        if 0 <= d < GLOBAL_SPAN:
            return ("g", name, d)
    return w


def run(s, case):
    try:
        base = build(s, case)
        wb = s.wbase(base)
        args = [res(wb, t) for t in case["args"]]
        answer = s.call(case["op"], args)
        log = s.log()
        return {
            "ans": norm(s, wb, answer) if OPS[case["op"]][2] else None,
            "block": [None if any(o <= 4 * i < o + len(d) for o, d in case.get("bytes", {}).items()) else norm(s, wb, w)
                      for i, w in enumerate(s.rd_words(base, BLOCK // 4))],
            "bytes": [s.read(base + o, len(d)) for o, d in case.get("bytes", {}).items()],
            "globals": [s.rd_words(s.g("ListStrInit"), 1), s.rd_words(s.g("ListUintInit"), 1)],
            "log": log,
        }
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


class Pick:
    """Edge mode (r None): successive picks walk the options in mixed radix, so cases 0..N cover the combinations. Random mode: choice."""

    def __init__(self, r=None, i=0):
        self.r = r
        self.i = i
        self.rr = r if r is not None else random.Random(i * 7919 + 13)

    def __call__(self, opts):
        if self.r is not None:
            return self.r.choice(opts)
        v = opts[self.i % len(opts)]
        self.i //= len(opts)
        return v

    def w(self):
        return self.rr.getrandbits(32) | 0x80000000   # never a block or global address of either side


WORDS = [0, 1, 2, 4, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
FLAGS = [0, 1, 0xFFFF, 0x7FFF, 0x8000, 0x10000, 0x10001, 0x12345]


def word(p):
    return p(WORDS + ["rand"]) if False else (lambda v: p.w() if v == "rand" else v)(p(WORDS + ["rand"]))


def mk(op, p):
    case = {"op": op, "init": [p.w() for _ in range(BLOCK // 4)], "set": {}, "globals": {}, "queues": {"Compare": [0]}, "args": [0, 0, 0, 0]}
    S = case["set"]
    SELF = ("p", 0x20)
    if op in ("first", "next"):
        case["args"] = [SELF]
        end = ("p", 0x24)
        S[0x80 + 4] = p([("p", 0xA0), end])
        S[0xA0 + 4] = p([end, ("p", 0x80)])
        if op == "first":
            S[0x28] = p([end, ("p", 0x80), ("p", 0xA0)])
        else:
            S[0x2C] = p([end, ("p", 0x80), ("p", 0xA0)])
    elif op in ("unmark_send", "unmark_delete"):
        case["args"] = [SELF]
        S[0x20 + 0x4C] = ("p", 0x100)
        S[0x108] = word(p)
    elif op in ("exceptions", "rdstate", "tie", "good"):
        case["args"] = [SELF]
        S[0x20 + 0x34] = word(p)
        S[0x20 + 0x38] = word(p)
        S[0x20 + 0x3C] = word(p)
    elif op == "state":
        case["args"] = [SELF]
    elif op == "or_assign":
        case["args"] = [("p", 0x40), word(p)]
        S[0x40] = word(p)
    elif op == "and":
        case["args"] = [word(p), word(p)]
    elif op in ("sentry_dtor", "uintlist_dtor", "strlist_dtor", "csock_dtor", "istream_dtor"):
        case["args"] = [p([0, SELF]), p(FLAGS + ["rand"])]
        if case["args"][1] == "rand":
            case["args"][1] = p.w()
        if op == "istream_dtor":
            S[0x20] = ("p", 0x100)
    elif op in ("strlist_ctor", "uintlist_ctor"):
        case["args"] = [SELF]
        case["globals"]["ListStrInit" if op == "strlist_ctor" else "ListUintInit"] = word(p)
    elif op == "less":
        case["args"] = [("p", 0x20), ("p", 0x30)]
        S[0x20], S[0x30] = ("p", 0x40), ("p", 0x80)
        S[0x40] = p([0, 1, 2, 5, 32, 33, 0x1000, 0xFFFFFFFF])
        S[0x4C] = ("p", 0x60)
        S[0x80] = p([0, 1, 3, 31, 32, 40, 0x1000, 0xFFFFFFFF])
        S[0x8C] = ("p", 0xA0)
        case["bytes"] = {0xA0: bytes(p.rr.getrandbits(8) for _ in range(0x40))}   # text is bytes, not words (endianness)
        case["queues"]["Compare"] = [p([-5, -1, 0, 1, 7, -0x80000000, 0x7FFFFFFF])]
    elif op in ("begin", "size"):
        case["args"] = [SELF]
        S[0x20] = ("p", 0x40)
    elif op == "mail_cbfn":
        case["args"] = [word(p), word(p), p([0, ("p", 0x30)]), ("p", 0x10)]
    return case


def edges(op):
    def gen():
        for i in range(160):
            yield mk(op, Pick(None, i))
    return gen


def rnd(op):
    return lambda r: mk(op, Pick(r))


NAMES = [
    ("AddressBook::First", "first"),
    ("AddressBook::Next", "next"),
    ("Outbox::UnmarkToDelete", "unmark_delete"),
    ("Outbox::UnmarkToSend", "unmark_send"),
    ("std::ios_base::exceptions", "exceptions"),
    ("std::operator|=", "or_assign"),
    ("std::ios_base::state", "state"),
    ("std::operator&", "and"),
    ("std::ios_base::rdstate", "rdstate"),
    ("std::basic_ios<char,std::char_traits<char>>::tie", "tie"),
    ("std::ios_base::good", "good"),
    ("std::basic_istream<char,std::char_traits<char>>::sentry::~sentry", "sentry_dtor"),
    ("std::basic_istream<char,std::char_traits<char>>::~basic_istream", "istream_dtor"),
    ("std::list<std::basic_string<char,std::char_traits<char>,std::allocator<char>>,std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>>::list", "strlist_ctor"),
    ("std::list<unsigned_int,std::allocator<unsigned_int>>::~list", "uintlist_dtor"),
    ("std::list<unsigned_int,std::allocator<unsigned_int>>::list", "uintlist_ctor"),
    ("__lt<c,Q23std14char_traits<c>,Q23std12allocator<c>>__3stdFRCQ23std59basic_string<c,Q23std14char_traits<c>,Q23std12allocator<c>>RCQ23std59basic_string<c,Q23std14char_traits<c>,Q23std12allocator<c>>", "less"),
    ("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::begin", "begin"),
    ("mail_cbfn", "mail_cbfn"),
    ("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::size", "size"),
    ("std::list<std::basic_string<char,std::char_traits<char>,std::allocator<char>>,std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>>::~list", "strlist_dtor"),
    ("CSock::~CSock", "csock_dtor"),
]

TESTS = [FnTest(name, ppc, host, edge=edges(op), rnd=rnd(op)) for name, op in NAMES]
