"""Unit v5899, the red-black tree of a std::set<nocase_string>: erase, destroy and insert_node_at, against
libtpw_unit_std___tree_erase_2.dylib (tests/unit_std___tree_erase_2.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  erase 0x1022419c, destroy 0x10223e64, insert_node_at 0x10228458

A case is a world and a call. The world (a tree of up to 8 nodes with random shape, colours, reference counts) is built twice by ONE piece
of Python code: in emulator memory (big-endian, 32-bit words) and in the host library (little-endian, native pointers). Every callee
outside the unit is a fake on both sides that records its arguments: operator new, new[], delete, strlen, strcpy, __throw, the string's
copy constructor and destructor, the allocator's destroy and deallocate, balance_insert and balance_erase (these two change nothing).

What is compared: the answer, the event log (pointers as labels), the tree's count, root and begin, and every node's links, colour,
value word and the reference count of its representation. No fused operations. Not tested: a NULL position or subtree (read
unconditionally); the exception clean-up paths of insert_node_at (never reached with fakes that do not throw).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_erase_2"
MASK = 0xFFFFFFFF

FN_ERASE, FN_DESTROY, FN_INSERT = 0x1022419C, 0x10223E64, 0x10228458
SIZES = {FN_ERASE: 320, FN_DESTROY: 596, FN_INSERT: 536}

HOST_LAYOUT = [0, 8, 16, 24, 8, 16, 0, 8, 16]

STUBS = CL.Stubs()
STUBS.real.update(SIZES)

def glue(fake):
    """The import glue saves the caller's r2 at 0x14(r1) (the original reloads it after the call); the stub does the same."""
    def f(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fake(e)
    return f


STUBS.internal(0x102384B8, glue(CL.recorder("new", "u", ret=lambda e, log: e.alloc(max(e.r(3), 64)))))
STUBS.internal(0x10238668, glue(CL.recorder("newarray", "u", ret=lambda e, log: e.alloc(max(e.r(3), 64)))))
STUBS.internal(0x10238218, glue(CL.recorder("delete", "L")))
STUBS.internal(0x10238650, glue(CL.recorder("strlen", "s", ret=lambda e, log: len(e.cstr(e.r(3))))))
STUBS.internal(0x10238410, glue(CL.recorder("strcpy", "xs")))
STUBS.internal(0x10238680, glue(CL.recorder("throw", "sxx")))
STUBS.internal(0x10153290, CL.recorder("balance_erase", "LL"))
STUBS.internal(0x10153B24, CL.recorder("balance_insert", "LL"))
STUBS.internal(0x1021ECB8, CL.recorder("alloc_destroy", "LL"))
STUBS.internal(0x1021EDA4, CL.recorder("dealloc", "LLu"))
STUBS.internal(0x102176D8, CL.recorder("string_dtor", "Lh"))
STUBS.internal(0x10228770, CL.recorder("string_copy", "LL"))


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.ps))[0]

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt, pfmt, ps = ">I", ">I", 4
    node_size, rep_size = 16, 16
    data_off = 12

    def __init__(self, e, case):
        self.e = e
        self.case = case

    def start(self, labels):
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"], first_new=1000)
        self.saved = STUBS.activate(self.e)

    def done(self):
        if hasattr(self, "saved"):
            CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        entry = {"erase": FN_ERASE, "destroy": FN_DESTROY, "insert": FN_INSERT}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt, pfmt, ps = "<I", "<Q", 8
    node_size, rep_size = 32, 24
    data_off = 16

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def start(self, labels):
        CL.host_begin(self.lib, {v: k for k, v in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_nocase_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        lib = self.lib
        if op == "erase":
            lib.tpw_nocase_erase(*args)
            return 0
        if op == "destroy":
            lib.tpw_nocase_destroy(*args)
            return 0
        return lib.tpw_nocase_insert(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_nocase_addr.restype, lib.tpw_nocase_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_nocase_alloc.restype, lib.tpw_nocase_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_nocase_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_nocase_erase.restype, lib.tpw_nocase_erase.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_nocase_destroy.restype, lib.tpw_nocase_destroy.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_nocase_insert.restype = C.c_uint64
    lib.tpw_nocase_insert.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_nocase_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


class World:
    pass


def build(s, case):
    ps = s.ps
    w = World()
    n = len(case["nodes"])
    w.tree = s.alloc(3 * ps)
    w.key = s.alloc(8)
    w.end = w.tree + ps
    w.nodes = [s.alloc(s.node_size) for _ in range(n)]
    w.reps = [s.alloc(s.rep_size) for _ in range(n)]
    w.datas = [s.alloc(8) for _ in range(n)]
    w.labels = {w.tree: 1, w.tree + 4: 2, w.key: 3}
    w.lab = {}
    w.replab = {}
    for i in range(n):
        w.labels[w.nodes[i]] = 10 + i
        w.labels[w.nodes[i] + 3 * ps] = 40 + i
        w.labels[w.reps[i]] = 70 + i
        w.labels[w.datas[i]] = 100 + i
        w.lab[w.nodes[i]] = 10 + i
        w.replab[w.reps[i]] = 70 + i
    s.words(w.tree, [case["count"]])
    s.ptr(w.tree + ps, w.nodes[0] if n else 0)
    s.ptr(w.tree + 2 * ps, w.nodes[case["begin"]] if n else 0)
    for i, nd in enumerate(case["nodes"]):
        a = w.nodes[i]
        left, right, parent, colour, refs = nd
        s.ptr(a, w.nodes[left] if left >= 0 else 0)
        s.ptr(a + ps, w.nodes[right] if right >= 0 else 0)
        s.ptr(a + 2 * ps, (w.nodes[parent] if parent >= 0 else w.end) | colour)
        s.ptr(a + 3 * ps, w.reps[i])
        s.words(w.reps[i] + 8, [refs])
        s.ptr(w.reps[i] + s.data_off, w.datas[i])
    return w


def labof(s, w, a):
    if a == 0:
        return 0
    if a in w.lab:
        return w.lab[a]
    if a == w.end:
        return "end"
    return "new"


def observe(s, case, w, answer):
    ps = s.ps
    out = {"answer": labof(s, w, answer) if case["op"] == "insert" else 0, "log": s.log()}
    out["count"] = s.rd_word(w.tree)
    out["root"] = labof(s, w, s.rd_ptr(w.tree + ps))
    out["begin"] = labof(s, w, s.rd_ptr(w.tree + 2 * ps))
    dump = []
    addrs = list(w.nodes) + ([answer] if case["op"] == "insert" and answer not in w.lab else [])
    for a in addrs:
        parent = s.rd_ptr(a + 2 * ps)
        value = s.rd_ptr(a + 3 * ps)
        refs = s.rd_word(value + 8) if value in w.replab else None
        dump.append([labof(s, w, s.rd_ptr(a)), labof(s, w, s.rd_ptr(a + ps)), labof(s, w, parent & ~1), parent & 1,
                     w.replab.get(value, value), refs])
    out["nodes"] = dump
    return out


def run(s, case):
    try:
        w = build(s, case)
        s.start(w.labels)
        op = case["op"]
        if op == "insert":
            parent = w.end if case["parent"] < 0 else w.nodes[case["parent"]]
            ans = s.call(op, [w.tree, parent, case["left"], case["leftmost"], w.key])
        elif op == "erase":
            ans = s.call(op, [w.tree, w.nodes[case["pos"]]])
        else:
            ans = s.call(op, [w.tree, w.nodes[case["pos"]]])
        return observe(s, case, w, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def make_tree(r, n):
    """n nodes of random shape: (left, right, parent, colour, refs) with indexes, -1 for none."""
    left = [-1] * n
    right = [-1] * n
    parent = [-1] * n
    for i in range(1, n):
        slots = [(p, side) for p in range(i) for side in (0, 1) if (left if side == 0 else right)[p] < 0]
        p, side = r.choice(slots)
        (left if side == 0 else right)[p] = i
        parent[i] = p
    nodes = [(left[i], right[i], parent[i], r.choice([0, 1]), r.choice([0, 1, 1, 2, 3, 7])) for i in range(n)]
    begin = 0
    while n and left[begin] >= 0:
        begin = left[begin]
    return nodes, begin


def base(r, op, n):
    nodes, begin = make_tree(r, n)
    return {"op": op, "queues": {}, "nodes": nodes, "begin": begin, "count": r.choice([n, n, r.randrange(0, 50)])}


def pick_pos(r, c, kind):
    n = len(c["nodes"])
    if kind == "begin":
        return c["begin"]
    if kind == "root":
        return 0
    return r.randrange(n)


def erase_case(r, n=None, kind=None):
    n = r.randrange(1, 9) if n is None else n
    c = base(r, "erase", n)
    c["pos"] = pick_pos(r, c, kind or r.choice(["begin", "begin", "root", "any"]))
    return c


def destroy_case(r, n=None, kind=None):
    n = r.randrange(1, 9) if n is None else n
    c = base(r, "destroy", n)
    c["pos"] = pick_pos(r, c, kind or r.choice(["root", "root", "any"]))
    return c


def insert_case(r, n=None, count=None):
    n = r.randrange(0, 7) if n is None else n
    c = base(r, "insert", n)
    if count is not None:
        c["count"] = count
    c["parent"] = r.randrange(n) if n and r.random() < 0.9 else -1
    c["left"] = r.choice([0, 1])
    c["leftmost"] = r.choice([0, 1])
    return c


def erase_edges():
    r = random.Random(5899)
    for n in range(1, 9):
        for kind in ("begin", "root", "any"):
            for _ in range(4):
                yield erase_case(r, n, kind)


def destroy_edges():
    r = random.Random(5900)
    for n in range(1, 9):
        for kind in ("root", "any"):
            for _ in range(4):
                yield destroy_case(r, n, kind)


def insert_edges():
    r = random.Random(5901)
    for count in (0, 5, 0xFFFFFFFD, 0xFFFFFFFE, 0xFFFFFFFF):
        for n in range(0, 5):
            for _ in range(3):
                yield insert_case(r, n, count)


TESTS = [
    FnTest("std::__tree<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>::erase", ppc, host,
           edge=erase_edges, rnd=erase_case),
    FnTest("std::__tree<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>::destroy", ppc, host,
           edge=destroy_edges, rnd=destroy_case),
    FnTest("std::__tree<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>::insert_node_at", ppc, host,
           edge=insert_edges, rnd=insert_case),
]
