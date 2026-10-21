"""Unit v51054, 25 small MSL helpers (use_facet wrappers, locale::facet dtor, filebuf<char> unshift and sync, ios_base::getloc,
the CharArray allocator calls, vector<facet*> size and operator[], _RefCountedPtr copy, the streambuf<char> defaults,
ios_base::failure dtor and what, FLY_GetCameraMesh), against libtpw_unit_std_use_facet.dylib (tests/unit_std_use_facet.cpp),
main app ("SimTheme Park"), src/game/MslSupport.cpp, src/game/rides/FlyingRide.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian) for the translation. One 0x400-byte block holds
the objects of the case; every pointer the log shows is an offset from that block (so both sides log the same numbers). The
callees outside the unit are fakes on both sides: operator delete, the _RefCountedPtr<char> dtor, the codecvt unshift
(__ptr_glue), fputc, fflush and the four __use_facet instances; they answer from the case's queues.

What is compared: the answer, the object words the function may write (pointers as block offsets), the globals it reads and the
event log. No fused operations. Not tested: a NULL this for the functions that read it unconditionally.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_use_facet"
MASK = 0xFFFFFFFF

# name, op number, address, size
OPS = [
    ("std::use_facet<std::ctype<char>>", 0, 0x10073934, 40),
    ("std::use_facet<std::codecvt<char,char,int>>", 1, 0x1007245C, 40),
    ("std::use_facet<std::ctype<wchar_t>>", 2, 0x1021EB10, 40),
    ("std::use_facet<std::codecvt<wchar_t,char,int>>", 3, 0x10225CB8, 40),
    ("std::locale::facet::~facet", 4, 0x10072DF0, 76),
    ("std::basic_filebuf<char,std::char_traits<char>>::unshift", 5, 0x10072A94, 216),
    ("std::basic_filebuf<char,std::char_traits<char>>::sync", 6, 0x100703E4, 92),
    ("std::ios_base::getloc", 7, 0x10072A34, 48),
    ("std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>::CharArray>::destroy", 8, 0x1007272C, 48),
    ("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::CharArray::removeRef", 9, 0x10072688, 52),
    ("std::vector<std::locale::facet*,std::allocator<std::locale::facet*>>::size", 10, 0x10072618, 8),
    ("std::vector<std::locale::facet*,std::allocator<std::locale::facet*>>::operator[]", 11, 0x100725A0, 16),
    ("std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>::CharArray>::deallocate", 12, 0x10072364, 40),
    ("std::_RefCountedPtr<std::__locale_imp<__int128>,std::_Single<std::__locale_imp<__int128>>>::_RefCountedPtr", 13, 0x10072030, 44),
    ("std::basic_streambuf<char,std::char_traits<char>>::seekoff", 14, 0x10070B60, 20),
    ("std::basic_streambuf<char,std::char_traits<char>>::seekpos", 15, 0x10070AD8, 20),
    ("std::basic_streambuf<char,std::char_traits<char>>::overflow", 16, 0x100707B0, 8),
    ("std::basic_streambuf<char,std::char_traits<char>>::pbackfail", 17, 0x10070754, 8),
    ("std::basic_streambuf<char,std::char_traits<char>>::underflow", 18, 0x10070634, 8),
    ("std::basic_streambuf<char,std::char_traits<char>>::showmanyc", 19, 0x100705D8, 8),
    ("std::basic_streambuf<char,std::char_traits<char>>::sync", 20, 0x10070584, 8),
    ("std::basic_streambuf<char,std::char_traits<char>>::setbuf", 21, 0x10070530, 4),
    ("std::ios_base::failure::~failure", 22, 0x1007020C, 136),
    ("std::ios_base::failure::what", 23, 0x100702C8, 8),
    ("FLY_GetCameraMesh", 24, 0x10075278, 52),
]
ADDR_OF = {n: a for _, n, a, _ in OPS}
OWN = {a for _, _, a, _ in OPS}

CALLEE_DELETE, CALLEE_RCDTOR, CALLEE_PTRGLUE = 0x10238218, 0x100025E0, 0x1023BAA8
CALLEE_FPUTC, CALLEE_FFLUSH = 0x102388A8, 0x10238920
USE_FACETS = {0x10073B54: "UseFacetCtypeChar", 0x100739A0: "UseFacetCodecvtCharChar",
              0x102261A0: "UseFacetCtypeWchar", 0x102298E4: "UseFacetCodecvtWcharChar"}

ADDR = dict(ios=0x1029D084, exc=0x10278BC0, fly=0x102DA964)
HOST_LAYOUT = [8, 16, 4, 8, 8, 24, 0x24, 0x2C, 0x20, 8]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def rel(e, p):
    p &= MASK
    b = e.base
    return p - b if b <= p < b + 0x400 else s32(p)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_delete(e):
    e.cl.rec("Delete", (rel(e, e.r(3)),))


def stub_rcdtor(e):
    e.cl.rec("RcDtor", (rel(e, e.r(3)) - 4, s16(e.r(4))))


def stub_unshift(e):
    L = e.cl
    to, to_end, nextp = e.r(5), e.r(6), e.r(7)
    L.rec("Unshift", (rel(e, e.r(3)), rel(e, e.r(4)), (to_end - to) & MASK, (e.u32(nextp) - to) & MASK))
    result = L.ret("unshift_r")
    n = L.ret("unshift_n")
    data = bytes(L.ret("unshift_b") & 0xFF for _ in range(max(n, 0)))
    if data:
        e.write(to, data)
    e.write(nextp, struct.pack(">I", (to + max(n, 0)) & MASK))
    e.set_r(3, result & MASK)


def stub_fputc(e):
    e.cl.rec("Fputc", (s32(e.r(3)), e.r(4) & MASK))
    e.set_r(3, e.cl.ret("fputc") & MASK)


def stub_fflush(e):
    e.cl.rec("Fflush", (e.r(3) & MASK,))
    e.set_r(3, e.cl.ret("fflush") & MASK)


def make_use_facet(name):
    def stub(e):
        e.cl.rec(name, (rel(e, e.r(3)),))
        e.set_r(3, e.cl.ret("use_facet") & MASK)
    return stub


STUBS.internal(CALLEE_DELETE, stub_delete)
STUBS.internal(CALLEE_RCDTOR, stub_rcdtor)
STUBS.internal(CALLEE_PTRGLUE, stub_unshift)
STUBS.internal(CALLEE_FPUTC, stub_fputc)
STUBS.internal(CALLEE_FFLUSH, stub_fflush)
for _a, _n in USE_FACETS.items():
    STUBS.internal(_a, make_use_facet(_n))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def nrm(self, w):
        w &= MASK
        b = self.B & MASK
        return w - b if b <= w < b + 0x400 else w

    def nrms(self, a, n):
        return [self.nrm(w) for w in self.rd_words(a, n)]

    def new_block(self):
        self.B = self.alloc(0x400)
        self.write(self.B, bytes(0x400))
        self.set_base(self.B)
        return self.B


class Ppc(Side):
    fmt = ">I"
    PS = 4
    OFF_DATA = 0xC
    OFF_RC = 4

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

    def set_base(self, b):
        self.e.base = b

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, ADDR_OF[op], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    PS = 8
    OFF_DATA = 0x10
    OFF_RC = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sf_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sf_alloc(n)

    def set_base(self, b):
        self.lib.tpw_sf_set_base(b)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v & 0xFFFFFFFFFFFFFFFF))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        a = [x & 0xFFFFFFFFFFFFFFFF for x in args] + [0] * (5 - len(args))
        return self.lib.tpw_sf_call(op, *a) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sf_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "set_base").restype, getattr(lib, p + "set_base").argtypes = None, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").restype = C.c_uint64
    getattr(lib, p + "call").argtypes = [C.c_int32] + [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

# offsets inside the block
OBJ, AUX, CNT, CVT, VT, BIG = 0x20, 0x40, 0x180, 0x100, 0x140, 0x200


def build(s, c):
    B = s.new_block()
    op, w = c["op"], c["w"]
    if op == 4 or op == 22 and False:
        s.words(B + OBJ, w[:2])
    elif op == 5:
        ws = list(w)
        ws[9] = c["file"]
        ws[10] = w[10]
        ws[11] = B + CVT
        s.words(B + OBJ, ws)
        s.words(B + CVT, [B + VT])
    elif op == 6:
        ws = list(w)
        ws[9] = c["file"]
        s.words(B + OBJ, ws)
    elif op == 7:
        s.ptr(B + 0x10, w[0])
        s.ptr(B + 0x10 + s.PS, w[1])
        s.words(B + AUX, w[:8])
        s.ptr(B + AUX + 0x20, B + 0x80)
        s.ptr(B + 0x80, w[10])
        s.ptr(B + 0x80 + s.PS, (B + CNT) if not c["null"] else 0)
        s.words(B + CNT, [c["cell"]])
    elif op == 8:
        s.words(B + AUX, w[:3])
        s.ptr(B + AUX + s.OFF_DATA, B + c["dataoff"] if c["dataoff"] >= 0 else c["dataraw"])
    elif op == 9:
        s.words(B + AUX, [w[0], w[1], c["refs"]])
    elif op == 10 or op == 11:
        s.words(B + AUX, [w[0], w[1]])
        s.ptr(B + AUX + 8, B + BIG)
    elif op == 13:
        s.ptr(B + 0x10, w[0])
        s.ptr(B + 0x10 + s.PS, w[1])
        s.ptr(B + AUX, w[2])
        s.ptr(B + AUX + s.PS, (B + CNT) if not c["null"] else 0)
        s.words(B + CNT, [c["cell"]])
    elif op == 14 or op == 15:
        s.words(B + 0x10, w[:2])
    elif op == 22:
        s.ptr(B + OBJ, w[0])
        s.ptr(B + OBJ + s.OFF_RC, c["msg"])
        s.ptr(B + OBJ + s.OFF_RC + s.PS, w[1])
    elif op == 23:
        s.ptr(B + OBJ, w[0])
        s.ptr(B + OBJ + s.OFF_RC, c["msg"])
    elif op == 24:
        s.words(B + BIG + 0x11C, [c["mesh"]])
        t = c["tour"]
        if 0 <= t < 100:
            s.ptr(s.g("fly") + t * s.PS, 0 if c["null"] else B + BIG)
        for k in c["fill"]:
            s.ptr(s.g("fly") + k * s.PS, B + BIG)
    if op == 4:
        s.words(B + OBJ, w[:2])


def args_of(s, c):
    B, op, w = s.B, c["op"], c["w"]
    if op <= 3:
        return [B + 0x10, c["junk"]]
    if op == 4:
        return [0 if c["null"] else B + OBJ, c["flagw"]]
    if op in (5, 6):
        return [B + OBJ]
    if op == 7:
        return [B + 0x10, B + AUX]
    if op == 8:
        return [B + 0x10, 0 if c["null"] else B + AUX]
    if op == 9:
        return [B + AUX]
    if op == 10:
        return [B + AUX]
    if op == 11:
        return [B + AUX, c["i"]]
    if op == 12:
        return [B + 0x10, (B + c["p"]) if c["p"] >= 0 else c["praw"], c["n"]]
    if op == 13:
        return [B + 0x10, B + AUX]
    if op in (14, 15):
        return [B + 0x10, B + AUX, w[3], w[4], w[5]]
    if op in (16, 17):
        return [B + AUX, c["junk"]]
    if op in (18, 19, 20):
        return [B + AUX]
    if op == 21:
        return [B + AUX, B + 0x30, w[3]]
    if op == 22:
        return [0 if c["null"] else B + OBJ, c["flagw"]]
    if op == 23:
        return [B + OBJ]
    if op == 24:
        return [c["tour"]]
    raise AssertionError(op)


def execute(s, c):
    r = s.call(c["op"], args_of(s, c))
    op = c["op"]
    if op in (7, 8, 12, 14, 15):      # void: r3 is whatever was left
        return 0
    if op in (4, 9, 13, 21, 22):
        return 0 if r == 0 else (r - (s.B & MASK)) & MASK
    if op == 11:
        return (r - ((s.B + BIG) & MASK)) & MASK
    if op in (5, 23) and False:
        return r
    if op == 5:
        return r & 0xFF
    return r


def vt_label(s, w):
    w &= MASK
    for n in ("ios", "exc"):
        if w == s.g(n) & MASK:
            return n
    return w


def observe(s, c, ans):
    B, op = s.B, c["op"]
    o = {"answer": ans, "log": s.log()}
    if op == 4:
        o["obj"] = s.rd_words(B + OBJ, 2)
    elif op in (5, 6):
        o["obj"] = s.nrms(B + OBJ, 16)
    elif op == 7:
        o["out"] = [s.nrm(s.rd_ptr(B + 0x10)), s.nrm(s.rd_ptr(B + 0x10 + s.PS))]
        o["ios"] = s.nrms(B + AUX, 8) + [s.nrm(s.rd_ptr(B + AUX + 0x20))]
        o["loc"] = [s.nrm(s.rd_ptr(B + 0x80)), s.nrm(s.rd_ptr(B + 0x80 + s.PS))]
        o["cell"] = s.rd_words(B + CNT, 1)
    elif op == 8:
        o["arr"] = s.nrms(B + AUX, 3)
        o["data"] = s.nrm(s.rd_ptr(B + AUX + s.OFF_DATA))
    elif op == 9:
        o["refs"] = s.rd_words(B + AUX + 8, 1)
    elif op in (10, 11):
        o["vec"] = s.nrms(B + AUX, 2)
        o["data"] = s.nrm(s.rd_ptr(B + AUX + 8))
    elif op == 13:
        o["self"] = [s.nrm(s.rd_ptr(B + 0x10)), s.nrm(s.rd_ptr(B + 0x10 + s.PS))]
        o["other"] = [s.nrm(s.rd_ptr(B + AUX)), s.nrm(s.rd_ptr(B + AUX + s.PS))]
        o["cell"] = s.rd_words(B + CNT, 1)
    elif op in (14, 15):
        o["out"] = s.rd_words(B + 0x10, 2)
    elif op == 22:
        o["vtbl"] = vt_label(s, s.rd_ptr(B + OBJ))
        o["msg"] = s.rd_ptr(B + OBJ + s.OFF_RC) & MASK
        o["cnt"] = s.rd_ptr(B + OBJ + s.OFF_RC + s.PS) & MASK
    elif op == 23:
        o["obj"] = s.rd_ptr(B + OBJ + s.OFF_RC) & MASK
    return o


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

WORDS = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x12345678]


def rw(r):
    return r.choice(WORDS) if r.random() < 0.3 else r.getrandbits(32)


def flagword(r, flag=None):
    if flag is None:
        flag = r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, r.randint(-0x8000, 0x7FFF)])
    hi = r.choice([0, 0xFFFF, r.getrandbits(16)])
    return ((hi << 16) | (flag & 0xFFFF)) & MASK


def base(r, op):
    return {"op": op,
            "queues": {"use_facet": [rw(r)], "unshift_r": [0], "unshift_n": [0], "unshift_b": [0], "fflush": [0], "fputc": [0]},
            "w": [rw(r) for _ in range(16)], "junk": rw(r), "null": False, "flagw": 0, "file": rw(r), "cell": rw(r),
            "refs": 1, "dataoff": 0x200, "dataraw": 0, "i": 0, "p": 0x40, "praw": 0, "n": rw(r), "msg": rw(r),
            "tour": 1, "mesh": rw(r), "fill": []}


def unshift_queues(r, forced=None):
    k = r.randint(0, 3)
    res = [1] * k + [r.choice([0, 2, 3, -1, 5])]
    if forced:
        res = forced
    n = [r.randint(0, 4) for _ in res]
    bytes_ = [r.choice([0, 0x7F, 0x80, 0xFF, r.getrandbits(8)]) for _ in range(16)]
    fputc = [r.choice([0, 1, 5]) for _ in range(r.randint(0, 4))] + [r.choice([0, -1, 7])]
    return {"unshift_r": res, "unshift_n": n, "unshift_b": bytes_, "fputc": fputc}


def over(op, r, edge=None):
    """The op's own fields: random, or the edge number `edge`."""
    d = {}
    q = {}
    e = edge
    if op <= 3:
        q["use_facet"] = [0 if e == 0 else rw(r)]
    elif op in (4, 22):
        d["null"] = (e == 0) if e is not None else r.random() < 0.15
        flags = [-1, 0, 1, 2, 0x7FFF, -0x8000]
        d["flagw"] = flagword(r, flags[e - 1] if e is not None and 1 <= e <= 6 else None)
        if e == 7:
            d["flagw"] = 0xFFFF0001
        if e == 8:
            d["flagw"] = 0x0001FFFF
        d["msg"] = r.choice([0, rw(r)])
    elif op == 5:
        forced = [[2], [3], [0], [-1], [1, 0], [1, 1, 2], [1, 1, 1, 0], [5], [1, 3]][e] if e is not None and e < 9 else None
        q.update(unshift_queues(r, forced))
        if e is not None:
            q["unshift_n"] = [4, 4, 4, 4]
            q["fputc"] = [[0], [-1], [0, -1], [0, 0, 0, 0, -1]][e % 4]
        d["file"] = rw(r)
    elif op == 6:
        d["file"] = 0 if (e == 0 or (e is None and r.random() < 0.25)) else rw(r) | 1
        q["fflush"] = [[0, -1, 1, 0x10000, 0xFFFFFFFF][e % 5] if e is not None else r.choice([0, 0, -1, 1, rw(r)])]
    elif op in (7, 13):
        d["null"] = (e == 0) if e is not None else r.random() < 0.25
        d["cell"] = [0, 0xFFFFFFFF, 0x7FFFFFFF, 1][e % 4] if e is not None else rw(r)
    elif op == 8:
        d["null"] = (e == 0) if e is not None else r.random() < 0.2
        d["dataoff"] = r.choice([0x200, 0x300, 0x10, -1]) if e is None else [0x200, -1, -1][e % 3]
        d["dataraw"] = r.choice([0, rw(r)]) if e is None else [0, 0, 0x1000][e % 3]
    elif op == 9:
        d["refs"] = [0, 1, 2, 0xFFFFFFFF, 0x80000000][e] if e is not None else r.choice([0, 1, 2, 3, rw(r)])
    elif op == 11:
        d["i"] = [0, 1, 2, 3, 0x3FFFFFFF, 0x40000000, 0x80000000, 0xFFFFFFFF][e] if e is not None else \
            r.choice([0, 1, 2, 3, 100, rw(r)])
    elif op == 12:
        c = [(0x40, 0), (-1, 0), (-1, 0x12345), (0x40, 1)][e % 4] if e is not None else r.choice([(0x40, 0), (-1, rw(r)), (-1, 0)])
        d["p"], d["praw"] = c
    elif op in (16, 17):
        d["junk"] = [0, -1 & MASK, 0x41, 0x80][e % 4] if e is not None else rw(r)
    elif op == 24:
        tours = [-1, 0, 1, 50, 98, 99, 100, 101, 0x7FFFFFFF, -0x80000000, 2, 3]
        d["tour"] = tours[e] if e is not None else r.choice(tours + [r.randint(-5, 110)])
        d["null"] = (e is not None and e in (2, 4)) or (e is None and r.random() < 0.25)
        d["fill"] = [0, 100] if False else ([0] if e == 1 else [])
    return d, q


EDGE_COUNT = {0: 2, 1: 2, 2: 2, 3: 2, 4: 9, 22: 9, 5: 12, 6: 6, 7: 4, 13: 4, 8: 3, 9: 5, 11: 8, 12: 4, 16: 4, 17: 4, 24: 12}


def mk(op, r, edge=None):
    c = base(r, op)
    d, q = over(op, r, edge)
    c.update(d)
    c["queues"].update(q)
    return c


def make_test(name, op):
    def edges():
        r = random.Random(1000 + op)
        for e in range(EDGE_COUNT.get(op, 1)):
            yield mk(op, r, e)

    def rnd(r):
        return mk(op, r)

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(name, op) for name, op, _, _ in OPS]
