"""Unit v51025, the buddy map's tree (std::map<nocase_string, BuddyInfo>), against
libtpw_unit_std___tree_std_pair_std_map_nocase_string_BuddyI_2.dylib (tests/unit_std___tree_std_pair_std_map_nocase_string_BuddyI_2.cpp), main app,
src/game/online/online.cpp:
  find 0x1021f8d8, insert_one 0x10222d9c, destroy 0x10222564, erase 0x102278fc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (32-bit big-endian words) for the
original, and in the host library's memory (native 64-bit little-endian pointers) for the translation; the same code reads both back. The
callees outside the unit are fakes on both sides that record their arguments: the nocase string compare (answers from the case's queue),
balance_erase, insert_node_at (answers a spare node), the allocator's destroy and deallocate, the pair destructor and operator delete.

The world of a case: a tree object (count, root, field 8, leftmost), up to 14 node slots (a random binary tree over the first n of them, a
spare one for insert_node_at's answer), each with a string rep (refs, data pointer) and a data block, and a key block (the key argument).
Pointers are compared as labels (node, pair, rep, data, tree words). What is compared: the answer (the iterator, the inserted flag), the
whole pool (tree words, every node's links, colour, key and flag bytes, every rep) and the event log.

No fused operations. Not tested: a NULL rep in a node (the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_std_pair_std_map_nocase_string_BuddyI_2"
MASK = 0xFFFFFFFF

P = ("std::__tree<std::pair<const_nocase_string,BuddyInfo>,std::map<nocase_string,BuddyInfo,std::less<nocase_string>,"
     "std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::value_compare,"
     "std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::")

FN_ERASE, FN_INSERT, FN_DESTROY, FN_FIND = 0x102278FC, 0x10222D9C, 0x10222564, 0x1021F8D8
SIZES = {FN_ERASE: 324, FN_INSERT: 236, FN_DESTROY: 604, FN_FIND: 196}
OWN = set(SIZES)

CALLEE_LT = 0x10225D2C
CALLEE_BALANCE_ERASE = 0x10153290
CALLEE_INSERT_NODE_AT = 0x102273BC
CALLEE_DEALLOC = 0x102229F8
CALLEE_ALLOC_DESTROY = 0x10222C78
CALLEE_PAIR_DTOR = 0x1021BD24
CALLEE_DELETE = 0x10238218

HOST_LAYOUT = [8, 0x18, 0x20, 0x10, 0x18, 8, 0x10, 8, 8]

NN = 14          # node slots
SPARE = 13       # the node insert_node_at answers

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s16(v):
    return T.s16(v)


def st_lt(e):
    L = e.cl
    L.rec("lt", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("lt") & 1)


def st_balance_erase(e):
    L = e.cl
    L.rec("balance_erase", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_insert_node_at(e):
    L = e.cl
    L.rec("insert_node_at", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF, L.lab(e.r(7))))
    e.set_r(3, L.ret("node") & MASK)


def st_dealloc(e):
    L = e.cl
    L.rec("dealloc", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5)))


def st_alloc_destroy(e):
    L = e.cl
    L.rec("alloc_destroy", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_pair_dtor(e):
    L = e.cl
    L.rec("pair_dtor", (L.lab(e.r(3)), s16(e.r(4) & 0xFFFF)))


def st_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(CALLEE_LT, st_lt)
STUBS.internal(CALLEE_BALANCE_ERASE, st_balance_erase)
STUBS.internal(CALLEE_INSERT_NODE_AT, st_insert_node_at)
STUBS.internal(CALLEE_DEALLOC, st_dealloc)
STUBS.internal(CALLEE_ALLOC_DESTROY, st_alloc_destroy)
STUBS.internal(CALLEE_PAIR_DTOR, st_pair_dtor)
STUBS.internal(CALLEE_DELETE, st_delete)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory (PW = pointer width, E = byte order)."""

    def pk(self, a, v, size):
        self.write(a, struct.pack(self.E + {1: "B", 2: "H", 4: "I", 8: "Q"}[size], v & ((1 << (8 * size)) - 1)))

    def u(self, a, size):
        return struct.unpack(self.E + {1: "B", 2: "H", 4: "I", 8: "Q"}[size], self.read(a, size))[0]

    def wp(self, a, v):
        self.pk(a, v, self.PW)

    def rp(self, a):
        return self.u(a, self.PW)

    def geom(self):
        PW = self.PW
        self.tsz, self.NS, self.RS = 4 * PW, 6 * PW, 4 * PW
        self.REPD = 12 if PW == 4 else 16
        total = self.tsz + NN * (self.NS + self.RS + 16) + self.NS + 4 * PW
        b = self.alloc(total)
        self.tree = b
        self.nodes = b + self.tsz
        self.reps = self.nodes + NN * self.NS
        self.datas = self.reps + NN * self.RS
        self.key = self.datas + NN * 16
        self.out = self.key + self.NS

    def node(self, i):
        return self.nodes + i * self.NS

    def rep(self, i):
        return self.reps + i * self.RS

    def data(self, i):
        return self.datas + i * 16

    def end(self):
        return self.tree + self.PW

    def labels(self):
        d = {1: self.tree, 2: self.tree + self.PW, 3: self.key}
        for i in range(NN):
            d[10 + i] = self.node(i)
            d[30 + i] = self.node(i) + 3 * self.PW
            d[50 + i] = self.rep(i)
            d[70 + i] = self.data(i)
        return d

    def lab(self, v):
        if v == 0:
            return 0
        return self.rev.get(v, -1)

    def queues(self, case):
        q = dict(case["queues"])
        q["node"] = [self.node(SPARE)]
        return q


class Ppc(Side):
    PW = 4
    E = ">"

    def __init__(self, e, case):
        self.e = e
        self.geom()
        lab = self.labels()
        self.rev = {a: l for l, a in lab.items()}
        self.L = e.cl = CL.PpcLog(dict(self.rev), self.queues(case))
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

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [a & MASK for a in args])

    def call_find(self):
        self.call(FN_FIND, [self.out, self.tree, self.key])

    def call_insert(self):
        self.call(FN_INSERT, [self.out, self.tree, self.key])

    def call_destroy(self, node):
        self.call(FN_DESTROY, [self.tree, node])

    def call_erase(self, node):
        self.call(FN_ERASE, [self.tree, node])


class Host(Side):
    PW = 8
    E = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.geom()
        lab = self.labels()
        self.rev = {a: l for l, a in lab.items()}
        CL.host_begin(lib, lab, self.queues(case))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_buddy_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_find(self):
        self.lib.tpw_buddy_find(self.out, self.tree, self.key)

    def call_insert(self):
        self.lib.tpw_buddy_insert_one(self.out, self.tree, self.key)

    def call_destroy(self, node):
        self.lib.tpw_buddy_destroy(self.tree, node)

    def call_erase(self, node):
        self.lib.tpw_buddy_erase(self.tree, node)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_buddy_alloc.restype, lib.tpw_buddy_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_buddy_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, n in (("find", 3), ("insert_one", 3), ("destroy", 2), ("erase", 2)):
        f = getattr(lib, "tpw_buddy_" + name)
        f.restype, f.argtypes = None, [C.c_uint64] * n
    out = (C.c_int64 * 32)()
    n = lib.tpw_buddy_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    PW = s.PW
    a = s.tree
    s.pk(a, c["count"], 4)
    s.wp(a + PW, s.node(c["root"]) if c["root"] >= 0 else 0)
    s.pk(a + 2 * PW, c["f8"], 4)
    s.wp(a + 3 * PW, s.node(c["leftmost"]) if c["leftmost"] >= 0 else s.end())
    for i in range(NN):
        nd = s.node(i)
        s.wp(nd, s.node(c["L"][i]) if c["L"][i] >= 0 else 0)
        s.wp(nd + PW, s.node(c["R"][i]) if c["R"][i] >= 0 else 0)
        p = c["P"][i]
        pa = 0 if p == -1 else (s.end() if p == -2 else s.node(p))
        s.wp(nd + 2 * PW, pa | c["col"][i])
        s.wp(nd + 3 * PW, s.rep(i))
        s.pk(nd + 4 * PW, c["info"][i][0], 1)
        s.pk(nd + 4 * PW + 1, c["info"][i][1], 1)
        rp = s.rep(i)
        s.pk(rp, i + 1, 4)
        s.pk(rp + 4, 16, 4)
        s.pk(rp + 8, c["refs"][i], 4)
        s.wp(rp + s.REPD, 0 if c["dnull"][i] else s.data(i))


def execute(s, c):
    op = c["op"]
    if op == "find":
        s.call_find()
    elif op == "insert":
        s.call_insert()
    elif op == "destroy":
        s.call_destroy(s.node(c["target"]))
    else:
        s.call_erase(s.node(c["target"]))


def observe(s, c):
    PW = s.PW
    a = s.tree
    o = {"log": s.log(), "count": s.u(a, 4), "root": s.lab(s.rp(a + PW)), "f8": s.u(a + 2 * PW, 4),
         "lm": s.lab(s.rp(a + 3 * PW))}
    if c["op"] == "find":
        o["answer"] = s.lab(s.rp(s.out))
    elif c["op"] == "insert":
        o["answer"] = (s.lab(s.rp(s.out)), s.u(s.out + PW, 1))
    for i in range(NN):
        nd = s.node(i)
        par = s.rp(nd + 2 * PW)
        o["n%d" % i] = [s.lab(s.rp(nd)), s.lab(s.rp(nd + PW)), s.lab(par & ~1), par & 1, s.lab(s.rp(nd + 3 * PW)),
                        s.u(nd + 4 * PW, 1), s.u(nd + 4 * PW + 1, 1)]
        rp = s.rep(i)
        o["r%d" % i] = [s.u(rp, 4), s.u(rp + 4, 4), s.u(rp + 8, 4), s.lab(s.rp(rp + s.REPD))]
    return o


def run(s, c):
    try:
        build(s, c)
        execute(s, c)
        return observe(s, c)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def make(r, op, n=None, bias=None, target=None, lt=None, trefs=None):
    if n is None:
        n = r.randrange(0, 11)
    if op in ("destroy", "erase"):
        n = max(n, 1)
    bias = r.choice([0.1, 0.5, 0.9]) if bias is None else bias
    L, R, Pa = [-1] * NN, [-1] * NN, [-1] * NN
    if n:
        Pa[0] = -2
    for k in range(1, n):
        cur = 0
        while True:
            left = r.random() < bias
            nxt = L[cur] if left else R[cur]
            if nxt == -1:
                if left:
                    L[cur] = k
                else:
                    R[cur] = k
                Pa[k] = cur
                break
            cur = nxt
    lm = -1
    if n:
        lm = 0
        while L[lm] != -1:
            lm = L[lm]
    refs = [r.choice([0, 1, 1, 2, 3]) for _ in range(NN)]
    if target is None:
        if op == "erase" and r.random() < 0.4:
            target = lm
        else:
            target = r.randrange(n) if n else 0
    if trefs is not None:
        refs[target] = trefs
    c = {
        "op": op, "n": n, "L": L, "R": R, "P": Pa, "root": 0 if n else -1, "leftmost": lm, "target": target,
        "col": [r.randrange(2) for _ in range(NN)], "refs": refs, "dnull": [r.random() < 0.2 for _ in range(NN)],
        "info": [(r.randrange(256), r.randrange(256)) for _ in range(NN)], "f8": r.getrandbits(32),
        "count": n if r.random() < 0.8 else r.getrandbits(32),
        "queues": {"lt": lt if lt is not None else [r.choice([0, 1]) for _ in range(40)]},
    }
    return c


def edges(op):
    def gen():
        r = random.Random(hash(op) & 0xFFFF if False else {"find": 11, "insert": 12, "destroy": 13, "erase": 14}[op])
        if op in ("find", "insert"):
            for n in range(0, 7):
                for lt in ([0], [1], [0, 1], [1, 0], [1, 1, 0], [0, 0, 1], None, None):
                    yield make(r, op, n=n, lt=lt)
        elif op == "destroy":
            for n in range(1, 9):
                for t in range(n):
                    for bias in (0.1, 0.5, 0.9):
                        yield make(r, op, n=n, bias=bias, target=t)
        else:
            for n in range(1, 8):
                for bias in (0.1, 0.5, 0.9):
                    for t in range(n):
                        for tr in (None, 0, 1, 2):
                            yield make(r, op, n=n, bias=bias, target=t, trefs=tr)
    return gen


def rnd(op):
    return lambda r: make(r, op)


TESTS = [FnTest(P + name, ppc, host, edge=edges(op), rnd=rnd(op))
         for name, op in (("erase", "erase"), ("insert_one", "insert"), ("destroy", "destroy"), ("find<nocase_string>", "find"))]
