"""Unit v5069, std::list<CToken>::insert 0x10210b44, against libtpw_unit_std_list_insert.dylib (tests/unit_std_list_insert.cpp), main app
("SimTheme Park"), src/COnlineParser_ParseGoto_std_list_insert.cpp:
  std::list<CToken,std::allocator<CToken>>::insert 0x10210b44

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory at the Mac's addresses, big-endian;
host library memory, little-endian; the host list and nodes use 32-bit links so the offsets are the Mac's). The callees outside the unit
(strlen, operator new, operator new[], strcpy, the counted pointer constructor, __throw, CToken's copy constructor) are fakes on both
sides that record their arguments.

The world of a case: a list object (size word), a sentinel-like position node with a previous node (or itself as previous: the empty
list), a source token of four random words. What is compared: the written iterator, the size, the position and previous nodes, the new
node (links named by role, token words), and the event log (the length error path included, since the fakes of __throw return).

No fused operations. Not tested: a NULL position (the original reads it unconditionally); the unwind code (unreachable here).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_insert"
MASK = 0xFFFFFFFF

FN = 0x10210B44
CTOKEN_COPY = 0x10210D4C
ADDR = dict(vtbl=0x10278B74, dtor=0x10265680)
HOST_LAYOUT = [0, 8, 16, 0x20, 0x10]      # the host's native pointers: prev, next, value, node size, CToken size


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update({FN: 376})


def cstr_of(e, a):
    return e.cstr(a)


def st_strlen(e):
    s = cstr_of(e, e.r(3))
    e.cl.rec("strlen", (), (s,))
    e.set_r(3, len(s.encode("latin-1")))


def st_strcpy(e):
    d, src = e.r(3), e.r(4)
    s = cstr_of(e, src)
    e.cl.rec("strcpy", (), (s,))
    e.write(d, s.encode("latin-1") + b"\0")


def st_new(e):
    n = e.r(3)
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def st_newa(e):
    n = e.r(3)
    e.cl.rec("new[]", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def st_rc(e):
    self, p = e.r(3), e.r(4)
    e.cl.rec("rc", (1 if p else 0,))
    e.write(self, struct.pack(">II", p & MASK, 0))


def st_throw(e):
    typ, obj, dtor = e.r(3), e.r(4), e.r(5)
    vt = 1 if e.u32(obj) == ADDR["vtbl"] else 0
    dt = 1 if dtor == ADDR["dtor"] else 0
    e.cl.rec("throw", (vt, dt), (cstr_of(e, typ), cstr_of(e, e.u32(obj + 4))))


def st_ctoken(e):
    this, src = e.r(3), e.r(4)
    w = struct.unpack(">4I", e.read(src, 16))
    e.cl.rec("CToken", w)
    e.write(this, e.read(src, 16))


def glue(fn):
    """A glue import: the real glue saves the caller's TOC pointer at 0x14(r1), which the caller reloads after the call."""

    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    return wrapped


STUBS.internal(0x10238650, glue(st_strlen))       # the glue addresses of the imports
STUBS.internal(0x10238410, glue(st_strcpy))
STUBS.internal(0x102384B8, glue(st_new))
STUBS.internal(0x10238668, glue(st_newa))
STUBS.internal(0x10238680, glue(st_throw))
STUBS.internal(0x100026AC, st_rc)
STUBS.internal(CTOKEN_COPY, st_ctoken)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptrs(self, a, ws):
        self.write(a, b"".join(struct.pack(self.pfmt, w & ((1 << (8 * self.pw)) - 1)) for w in ws))

    def rd_ptrs(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%d%s" % (n, self.pfmt[1]), self.read(a, self.pw * n)))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pw = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
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

    def call(self, out, lst, pos, tok):
        T.call_ppc(self.e, FN, [out, lst, pos, tok])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pw = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_tli_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, out, lst, pos, tok):
        self.lib.tpw_tli_insert(out, lst, pos, tok)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tli_alloc.restype, lib.tpw_tli_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_tli_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tli_insert.restype, lib.tpw_tli_insert.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tli_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        pw = s.pw
        out = s.alloc(8)
        lst = s.alloc(8)
        pos = s.alloc(0x20)
        prev = pos if case["empty"] else s.alloc(0x20)
        tok = s.alloc(16)
        s.words(lst, [case["size"]])
        s.ptrs(out, [0xDEADBEEF])
        s.ptrs(pos, [prev, case["pnext"]])
        if prev != pos:
            s.ptrs(prev, [case["qprev"], pos])
        s.words(tok, case["token"])
        s.call(out, lst, pos, tok)
        new = s.rd_ptrs(out, 1)[0]
        names = {0: "null", pos: "pos", prev: "prev", new: "new"}
        names.setdefault(pos, "pos")

        def nm(w):
            return names.get(w, w)

        def node(a, n=2):
            return [nm(w) for w in s.rd_ptrs(a, n)]

        res = {
            "out": nm(new),
            "size": s.rd_words(lst, 1),
            "pos": node(pos),
            "prev": node(prev),
            "token": s.rd_words(tok, 4),
            "log": s.log(),
        }
        if new not in (pos, prev, 0):
            res["new"] = node(new, 2) + s.rd_words(new + 2 * pw, 4)
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, size=None, empty=None):
    return {
        "size": r.choice([0, 1, 5, 100, 0x7FFFFFFE, 0x7FFFFFFF, r.getrandbits(32)]) if size is None else size,
        "empty": r.random() < 0.2 if empty is None else empty,
        "pnext": r.getrandbits(32),
        "qprev": r.getrandbits(32),
        "token": [r.getrandbits(32) for _ in range(4)],
    }


def edges():
    r = random.Random(5069)
    for size in (0, 1, 0x7FFFFFFD, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF):
        for empty in (True, False):
            yield make(r, size, empty)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("std::list<CToken,std::allocator<CToken>>::insert", ppc, host, edge=edges, rnd=rnd),
]
