"""Unit v5126, the theme map's tree, against libtpw_unit_std___tree_std_pair_std_map_TbDynamicStringTempl.dylib
(tests/unit_std___tree_std_pair_std_map_TbDynamicStringTempl.cpp), main app, src/FS_MasterSave_global_std___tree_std_pair_std_map_TbDynamicStringTempl.cpp:
  find 0x1016cf68, clear 0x1016cd78, insert_one 0x1016e0b8

A case is a tree of up to 6 nodes (any shape, built from left/right links) and a call. Both memories are built by ONE piece of Python
code (Mac words 4 bytes big-endian, host pointers 8 bytes little-endian). The callees are fakes on both sides that record their
arguments as labels: TbStringBase<char>::operator< (answers from the queue "lt"), destroy, and insert_node_at (answers the node
numbered by the queue "ins"). What is compared: the result block (node label and the inserted byte), the tree object's words and the
event log. The two TOC bytes of insert_one are given random values on both sides. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_std_pair_std_map_TbDynamicStringTempl"
MASK = 0xFFFFFFFF

FN_FIND, FN_CLEAR, FN_INSERT = 0x1016CF68, 0x1016CD78, 0x1016E0B8
SIZES = {FN_FIND: 196, FN_CLEAR: 96, FN_INSERT: 236}
OWN = set(SIZES)
CALLEE_LT, CALLEE_DESTROY, CALLEE_INSNODE = 0x10238890, 0x1016D784, 0x1016EB98

ADDR = dict(inserted=0x102A7238, notinserted=0x102A7239)
HOST_LAYOUT = [8, 24, 8, 24, 8]

NMAX = 6
CUR = [None]


def lbl_node(i):
    return 10 + i


def lbl_val(i):
    return 30 + i


# ------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_lt(e):
    L = e.cl
    L.rec("lt", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("lt") & 1)


def stub_destroy(e):
    L = e.cl
    L.rec("destroy", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_insnode(e):
    L = e.cl
    L.rec("insert_node_at", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF, L.lab(e.r(7))))
    k = L.ret("ins")
    e.set_r(3, CUR[0].table[k & 15] & MASK)


STUBS.internal(CALLEE_LT, stub_lt)
STUBS.internal(CALLEE_DESTROY, stub_destroy)
STUBS.internal(CALLEE_INSNODE, stub_insnode)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def prep(self):
        W = self.W
        self.tree = self.alloc(0x40)
        self.nodes = [self.alloc(0x40) for _ in range(NMAX)]
        self.newnode = self.alloc(0x40)
        self.key = self.alloc(0x40)
        self.res = self.alloc(0x20)
        self.table = self.nodes + [self.newnode] + [0] * 9
        self.names = {self.tree: 1, self.tree + W: 2, self.key: 3, self.res: 4, self.newnode: 20}
        for i, n in enumerate(self.nodes):
            self.names[n] = lbl_node(i)
        self.fixed_val = {lbl_val(i): n + 3 * W for i, n in enumerate(self.nodes)}

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pf, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pf, self.read(a, self.W))[0]

    def name(self, v):
        return 0 if v == 0 else self.names.get(v, -1)

    def u32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def rd_u32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def fixed_map(self):
        d = {self.tree: 1, self.tree + self.W: 2, self.key: 3, self.res: 4}
        for i, n in enumerate(self.nodes):
            d[n] = lbl_node(i)
            d[n + 3 * self.W] = lbl_val(i)
        return d


class Ppc(Side):
    fmt = ">I"
    pf = ">I"
    W = 4

    def __init__(self, e, case):
        self.e = e
        CUR[0] = self
        self.prep()
        self.L = e.cl = CL.PpcLog(self.fixed_map(), case["queues"])
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

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    pf = "<Q"
    W = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.prep()
        for i, a in enumerate(self.table):
            lib.tpw_ptree_table(i, a)
        fm = {v: k for k, v in self.fixed_map().items()}
        CL.host_begin(lib, fm, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ptree_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ptree_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = {FN_FIND: self.lib.tpw_ptree_find, FN_CLEAR: self.lib.tpw_ptree_clear, FN_INSERT: self.lib.tpw_ptree_insert}[fn]
        f(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ptree_addr.restype, lib.tpw_ptree_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ptree_alloc.restype, lib.tpw_ptree_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ptree_table.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_ptree_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ptree_find.argtypes = [C.c_uint64] * 3
    lib.tpw_ptree_clear.argtypes = [C.c_uint64]
    lib.tpw_ptree_insert.argtypes = [C.c_uint64] * 3
    out = (C.c_int64 * 32)()
    n = lib.tpw_ptree_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    W = s.W
    n = case["n"]
    links = case["links"]
    for i in range(NMAX):
        l, r = links[i] if i < n else (-1, -1)
        s.ptr(s.nodes[i], s.nodes[l] if l >= 0 else 0)
        s.ptr(s.nodes[i] + W, s.nodes[r] if r >= 0 else 0)
        s.ptr(s.nodes[i] + 2 * W, case["junk"][i])
    s.u32(s.tree, case["count"])
    s.ptr(s.tree + W, s.nodes[0] if n > 0 else 0)
    s.ptr(s.tree + 2 * W, case["slot8"])
    s.ptr(s.tree + 3 * W, s.nodes[case["begin"]])
    s.ptr(s.res, 0x1234)
    s.write(s.res + W, bytes([0xEE]))
    s.write(s.g("inserted"), bytes([case["gi"]]))
    s.write(s.g("notinserted"), bytes([case["gn"]]))


def execute(s, case):
    if case["op"] == "find":
        s.call(FN_FIND, [s.res, s.tree, s.key])
    elif case["op"] == "clear":
        s.call(FN_CLEAR, [s.tree])
    else:
        s.call(FN_INSERT, [s.res, s.tree, s.key])


def observe(s, case):
    W = s.W
    return {
        "res": s.name(s.rd_ptr(s.res)),
        "flag": s.read(s.res + W, 1),
        "count": s.rd_u32(s.tree),
        "root": s.name(s.rd_ptr(s.tree + W)),
        "slot8": s.rd_ptr(s.tree + 2 * W),
        "begin": s.name(s.rd_ptr(s.tree + 3 * W)),
        "nodes": [(s.name(s.rd_ptr(n)), s.name(s.rd_ptr(n + W)), s.rd_ptr(n + 2 * W)) for n in s.nodes],
        "log": s.log(),
    }


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ------------------------------------------------------------------------------------------------------------- cases


def shape(r, n):
    """A random binary tree on nodes 0..n-1 rooted at 0: each next node hangs on a free child slot."""
    links = [[-1, -1] for _ in range(NMAX)]
    for i in range(1, n):
        free = [(p, d) for p in range(i) for d in (0, 1) if links[p][d] < 0]
        p, d = r.choice(free)
        links[p][d] = i
    return [tuple(x) for x in links]


def mk(r, op, n=None, lt=None, ins=None, links=None):
    n = r.randrange(0, NMAX + 1) if n is None else n
    return {
        "op": op, "n": n, "links": shape(r, n) if links is None else links,
        "junk": [r.randrange(0, 1 << 31) for _ in range(NMAX)],
        "count": r.randrange(0, 20), "slot8": r.randrange(0, 1 << 31), "begin": r.randrange(0, NMAX),
        "gi": r.randrange(0, 256), "gn": r.randrange(0, 256),
        "queues": {"lt": [r.choice([0, 1]) for _ in range(r.randrange(1, 10))] if lt is None else lt,
                   "ins": [r.randrange(0, NMAX + 1) if ins is None else ins]},
    }


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        for n in (0, 1, 2, 3, 6):
            for lt in ([0], [1], [0, 1], [1, 0], [1, 1, 0], [0, 0, 1], [1, 0, 1, 0]):
                for ins in (0, 1, NMAX):
                    yield mk(r, op, n, lt, ins)
    return gen


def rnd(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("std::__tree<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>,std::map<TbDynamicStringTemplate<char>,CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::value_compare,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::find<TbDynamicStringTemplate<char>>",
           ppc, host, edge=edges("find", 11), rnd=rnd("find")),
    FnTest("std::__tree<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>,std::map<TbDynamicStringTemplate<char>,CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::value_compare,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::clear",
           ppc, host, edge=edges("clear", 12), rnd=rnd("clear")),
    FnTest("std::__tree<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>,std::map<TbDynamicStringTemplate<char>,CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::value_compare,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::insert_one",
           ppc, host, edge=edges("insert", 13), rnd=rnd("insert")),
]
