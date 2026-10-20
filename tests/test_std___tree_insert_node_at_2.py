"""Unit v5653: std::__tree<wchar_t, less, allocator>::insert_node_at (std::set<wchar_t>'s tree), against
libtpw_unit_std___tree_insert_node_at_2.dylib (tests/unit_std___tree_insert_node_at_2.cpp), main app, src/COnlinePerson_SetDest_std___tree_insert_node_at_2.cpp:
  insert_node_at 0x10212b2c

A case is a world and a call. The world is built twice by ONE piece of Python code (Mac words are 32 bits, the host's pointers 8 bytes: the
side says its pointer size and field offsets). The callees strlen, strcpy, operator new, operator new[], __throw and balance_insert are
fakes on both sides that record their arguments; __throw returns, so the code goes on after the length-error branch.

The world of a case: a tree (count, root, begin), a parent node and another node (children, parent word, character), the new node's
garbage colour bit, the flags and the character. What is compared: the answer as a label, the tree words, both nodes and the new node
(children, parent label and colour, character), and the event log (including the thrown object's fields).

No fused operations. Not tested: nothing excluded.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_insert_node_at_2"
MASK = 0xFFFFFFFF

FN = 0x10212B2C
SIZES = {FN: 464}
OWN = set(SIZES)

STRLEN, STRCPY, NEW, NEWA, THROW, BALANCE = 0x10238650, 0x10238410, 0x102384B8, 0x10238668, 0x10238680, 0x10153B24
VT_LEN, DTOR_LEN = 0x10278B74, 0x10265680
HOST_LAYOUT = [0, 8, 16, 0, 8, 16, 24]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab_of(e, v):
    return e.cl.lab(v & ~1)


def st_strlen(e):
    pass


def ret_strlen(e, log):
    return len(e.cstr(e.r(3)))


def ret_strcpy(e, log):
    d = e.r(3)
    e.write(d, e.cstr(e.r(4)).encode("latin1") + b"\0")
    return d


def ret_new(e, log):
    size = e.r(3)
    data = bytearray(max(size, 64))
    if size == 0xE:
        data[11] = e.cl.ret("newfill") & 1
    return e.alloc(len(data), bytes(data))


def ret_newa(e, log):
    return e.alloc(max(e.r(3), 64))


def ret_throw(e, log):
    o = e.r(4)
    ptr, cnt = e.u32(o + 4), e.u32(o + 8)
    s = 0
    if ptr:
        s = sum(e.cstr(ptr).encode("latin1"))
    e.cl.rec("ThrowObj", (int(e.u32(o) == VT_LEN), int(e.r(5) == DTOR_LEN), int(ptr != 0), int(cnt != 0),
                          T.s32(e.u32(cnt)) if cnt else -1, s))
    return e.r(3)


def stub_balance(e):
    L = e.cl
    n, root = e.r(3), e.r(4)
    left, right, par = e.u32(n), e.u32(n + 4), e.u32(n + 8)
    ch = struct.unpack(">H", e.read(n + 0xC, 2))[0]
    L.rec("balance", (L.lab(root), L.lab(par & ~1), par & 1, L.lab(left), L.lab(right), ch))


def glue(fn):
    """The import glue saves the caller's TOC at 0x14(r1); the function reloads r2 from there after the call."""
    def f(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return f


STUBS.internal(STRLEN, glue(CL.recorder("strlen", "s", ret=ret_strlen)))
STUBS.internal(STRCPY, glue(CL.recorder("strcpy", "xs", ret=ret_strcpy)))
STUBS.internal(NEW, glue(CL.recorder("new", "u", ret=ret_new)))
STUBS.internal(NEWA, glue(CL.recorder("newa", "u", ret=ret_newa)))
STUBS.internal(THROW, glue(CL.recorder("throw", "s", ret=ret_throw)))
STUBS.internal(BALANCE, stub_balance)


def setup(runner):
    pass


class Side:
    def put(self, a, v):
        self.write(a, struct.pack(self.pf, v))

    def get(self, a):
        return struct.unpack(self.pf, self.read(a, self.ps))[0]

    def put32(self, a, v):
        self.write(a, struct.pack(self.f32, v & MASK))

    def get32(self, a):
        return struct.unpack(self.f32, self.read(a, 4))[0]

    def put16(self, a, v):
        self.write(a, struct.pack(self.f16, v & 0xFFFF))

    def get16(self, a):
        return struct.unpack(self.f16, self.read(a, 2))[0]

    def blocks(self):
        self.B = {"T": self.alloc(3 * self.ps + 8), "P": self.alloc(4 * self.ps + 8), "Q": self.alloc(4 * self.ps + 8)}

    def addr(self, name):
        return 0 if name is None else self.B[name]


class Ppc(Side):
    pf, ps, f32, f16 = ">I", 4, ">I", ">H"

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        B = self.B
        self.L = e.cl = CL.PpcLog({B["T"]: 1, B["P"]: 2, B["Q"]: 3}, case["queues"])
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

    def call(self, left, leftmost, value):
        v = self.e.alloc(4)
        self.put16(v, value)
        r3, _, _ = T.call_ppc(self.e, FN, [self.B["T"], self.B["P"], left, leftmost, v])
        return r3 & MASK


class Host(Side):
    pf, ps, f32, f16 = "<Q", 8, "<I", "<H"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        B = self.B
        CL.host_begin(lib, {1: B["T"], 2: B["P"], 3: B["Q"]}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wtree_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, left, leftmost, value):
        return self.lib.tpw_wtree_insert(self.B["T"], self.B["P"], left, leftmost, value)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wtree_alloc.restype, lib.tpw_wtree_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wtree_insert.restype = C.c_uint64
    lib.tpw_wtree_insert.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_wtree_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wtree_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


NODE = {"T": None, "P": 0, "Q": 0}


def build(s, case):
    ps = s.ps
    T_, P, Q = s.B["T"], s.B["P"], s.B["Q"]
    s.put32(T_, case["count"])
    s.put(T_ + ps, s.addr(case["root"]))
    s.put(T_ + 2 * ps, s.addr(case["begin"]))
    for name, key in (("P", "p"), ("Q", "q")):
        n = s.B[name]
        left, right, parent, col, ch = case[key]
        s.put(n, s.addr(left))
        s.put(n + ps, s.addr(right))
        s.put(n + 2 * ps, s.addr(parent) | col)
        s.put16(n + 3 * ps, ch)


def label(s, v, extra=None):
    inv = {a: k for k, a in s.B.items()}
    if v == 0:
        return None
    if v in inv:
        return inv[v]
    return "NEW"


def node_state(s, a):
    ps = s.ps
    par = s.get(a + 2 * ps)
    return (label(s, s.get(a)), label(s, s.get(a + ps)), label(s, par & ~1), par & 1, s.get16(a + 3 * ps))


def observe(s, case, answer):
    ps = s.ps
    T_ = s.B["T"]
    out = {"answer": label(s, answer), "log": s.log(), "count": s.get32(T_),
           "root": label(s, s.get(T_ + ps)), "begin": label(s, s.get(T_ + 2 * ps)),
           "P": node_state(s, s.B["P"]), "Q": node_state(s, s.B["Q"])}
    out["new"] = node_state(s, answer) if answer else None
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["left"], case["leftmost"], case["value"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, count=None, left=None, leftmost=None, fill=None):
    names = [None, "P", "Q"]
    def nd():
        return (r.choice(names), r.choice(names), r.choice(["P", "Q", None]), r.randrange(2), r.choice([0, 0x41, 0xFFFF, r.randrange(65536)]))
    return {
        "queues": {"newfill": [r.randrange(2) if fill is None else fill]},
        "count": r.choice([0, 1, 7, 0xFFFFFFFD, 0xFFFFFFFE, 0xFFFFFFFF]) if count is None else count,
        "root": r.choice([None, "P", "Q"]), "begin": r.choice([None, "P", "Q"]),
        "p": nd(), "q": nd(),
        "left": r.randrange(2) if left is None else left, "leftmost": r.randrange(2) if leftmost is None else leftmost,
        "value": r.choice([0, 1, 0x41, 0x7FFF, 0x8000, 0xFFFF, r.randrange(65536)]),
    }


def edges():
    import random
    r = random.Random(53)
    for count in (0, 5, 0xFFFFFFFE, 0xFFFFFFFF):
        for left in (0, 1):
            for leftmost in (0, 1):
                for fill in (0, 1):
                    yield mk(r, count, left, leftmost, fill)


def rnd(r):
    return mk(r)


TESTS = [
    FnTest("std::__tree<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::insert_node_at", ppc, host, edge=edges, rnd=rnd),
]
