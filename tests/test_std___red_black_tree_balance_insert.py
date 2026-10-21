"""Unit v5942, the red-black tree's balance_insert, the subscription tree's insert_node_at and CSubscription::Load, against
libtpw_unit_std___red_black_tree_balance_insert.dylib (tests/unit_std___red_black_tree_balance_insert.cpp), main app
("SimTheme Park"), src/game/ui/MessageCentre.cpp:
  balance_insert 0x10153b24, insert_node_at 0x101538d8, CSubscription::Load 0x10152bb4

A case is a world and a call. The world is a tree built twice by ONE piece of Python code: in emulator memory (big-endian, 4-byte
pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; each side has its own
node offsets. The rotations balance_insert calls are the real ones on both sides. The callees outside the unit (operator new, new[],
strlen, strcpy, the _RefCountedPtr constructor, __throw, LbFile_Read, DEBUG_MESSAGE) are fakes on both sides that record the same events.

The world of a case: a random binary tree of 0..20 nodes with random colours (the root black), the call's node, parent and flags; for Load
a buffer, a file handle and the queued answers of LbFile_Read (bytes and count). What is compared: the answer, the event log, and every node
(left, right, parent, colour, value as indices), the root word and the count; for Load the id the buffer holds, as each side reads a
halfword (the file is little-endian: the Mac swaps the id it reads, the host does not). insert_node_at's first-node word (+8 on the Mac) is checked on the PowerPC side against the expected value; the host derives begin and has no such word.

No fused operations. Not tested: a parent that is a red root (the original reads the end node as a grandparent: the tests keep the root
black), a right child of the end node (it would store into the Mac's begin word).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___red_black_tree_balance_insert"
MASK = 0xFFFFFFFF

FN_BI, FN_INS, FN_LOAD = 0x10153B24, 0x101538D8, 0x10152BB4
SIZES = {FN_BI: 572, FN_INS: 332, FN_LOAD: 120}
OWN = set(SIZES)

G_STRLEN, G_NEWARR, G_STRCPY, G_THROW, G_NEW, G_READ = 0x10238650, 0x10238668, 0x10238410, 0x10238680, 0x102384B8, 0x10238848
RC_CTOR, DEBUG_MESSAGE = 0x100026AC, 0x1006FC90
ADDR = dict(dtortv=0x10265680, vtbl=0x10278B74)
HOST_LAYOUT = [24, 24, 32, 8, 16]


def H(s):
    h = 0
    for c in s:
        h = (h * 31 + c) & MASK
    return h


def cs(e, a):
    s = e.cstr(a)
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_strlen(e):
    s = cs(e, e.r(3))
    e.cl.rec("strlen", (len(s), H(s)))
    e.set_r(3, len(s))


def stub_strcpy(e):
    s = cs(e, e.r(4))
    e.cl.rec("strcpy", (len(s), H(s)))
    e.write(e.r(3), s + b"\0")


def stub_new(e):
    n = e.r(3)
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(n))


def stub_newarr(e):
    n = e.r(3)
    e.cl.rec("newarr", (n,))
    e.set_r(3, e.alloc(n + 16))


def stub_rcctor(e):
    p = e.r(4)
    e.cl.rec("RcCtor", (1 if p else 0,))
    e.write(e.r(3), struct.pack(">II", p, 0))


def stub_throw(e):
    L = e.cl
    obj = e.r(4)
    L.rec("throw", (H(cs(e, e.r(3))), L.lab(e.r(5)), L.lab(e.u32(obj)), H(cs(e, e.u32(obj + 4)))))


def stub_read(e):
    L = e.cl
    data = L.ret("data") & MASK
    L.rec("LbFile_Read", (e.r(4), e.r(6)))
    e.write(e.r(5), bytes([data & 255, (data >> 8) & 255]))
    e.write(e.r(7), struct.pack(">I", L.ret("xfer") & MASK))


def stub_debug(e):
    s = cs(e, e.r(3))
    e.cl.rec("DEBUG_MESSAGE", (len(s), H(s), e.r(4)))


def glue(fn):
    """What the import glue does: the caller's `lwz r2, 0x14(r1)` after the call reloads the TOC the glue saved there."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(G_STRLEN, glue(stub_strlen))
STUBS.internal(G_STRCPY, glue(stub_strcpy))
STUBS.internal(G_NEW, glue(stub_new))
STUBS.internal(G_NEWARR, glue(stub_newarr))
STUBS.internal(RC_CTOR, stub_rcctor)
STUBS.internal(G_THROW, glue(stub_throw))
STUBS.internal(G_READ, glue(stub_read))
STUBS.internal(DEBUG_MESSAGE, stub_debug)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def ptr(self, a, v):
        self.write(a, struct.pack(self.pf, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pf, self.read(a, self.psz))[0]

    def half(self, a):
        return struct.unpack(self.hf, self.read(a, 2))[0]


class Ppc(Side):
    pf, psz, hf, wf = ">I", 4, ">H", ">I"
    R, P, V, NS, ROOT = 4, 8, 0xC, 16, 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x10265680: 1, 0x10278B74: 2}, case["queues"])
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

    def set_begin(self, tree, node):
        self.ptr(tree + 8, node)

    def begin_ok(self, tree, expect):
        return self.rd_ptr(tree + 8) == expect

    def call_bi(self, node, root):
        T.call_ppc(self.e, FN_BI, [node, root])
        return 0

    def call_ins(self, tree, parent, left, leftmost, key):
        r3, _, _ = T.call_ppc(self.e, FN_INS, [tree, parent, left, leftmost, key])
        return r3 & MASK

    def call_load(self, this, filep):
        r3, _, _ = T.call_ppc(self.e, FN_LOAD, [this, filep])
        return r3 & MASK


class Host(Side):
    pf, psz, hf, wf = "<Q", 8, "<H", "<I"
    R, P, V, NS, ROOT = 8, 16, 24, 32, 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_bi_addr(b"dtortv"), 2: lib.tpw_bi_addr(b"vtbl")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_bi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_begin(self, tree, node):
        pass

    def begin_ok(self, tree, expect):
        return True      # the host derives begin: there is no word to compare

    def call_bi(self, node, root):
        self.lib.tpw_bi_balance(node, root)
        return 0

    def call_ins(self, tree, parent, left, leftmost, key):
        return self.lib.tpw_bi_insert(tree, parent, left, leftmost, key)

    def call_load(self, this, filep):
        return self.lib.tpw_bi_load(this, filep) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bi_addr.restype, lib.tpw_bi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bi_alloc.restype, lib.tpw_bi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bi_balance.restype, lib.tpw_bi_balance.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_bi_insert.restype, lib.tpw_bi_insert.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    lib.tpw_bi_load.restype, lib.tpw_bi_load.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_tree(s, case):
    shape = case["shape"]
    s.tree = tree = s.alloc(0x20)
    end = tree + s.ROOT
    s.nodes = nodes = [s.alloc(s.NS) for _ in shape]
    for i, (p, side, red, val) in enumerate(shape):
        a = nodes[i]
        if p < 0:
            par = end
            s.ptr(end, a)
        else:
            par = nodes[p]
            s.ptr(par + (s.R if side else 0), a)
        s.ptr(a + 8 + (s.P - 8), par | red)
        s.write(a + s.V, struct.pack(s.hf, val))
    s.write(tree, struct.pack(s.wf, case["count"]))
    first = 0
    i = 0
    while shape and True:
        first = i
        kids = [j for j, (p, side, _, _) in enumerate(shape) if p == i and side == 0]
        if not kids:
            break
        i = kids[0]
    s.begin0 = nodes[first] if shape else end
    s.set_begin(tree, s.begin0)


def build(s, case):
    if case["op"] in ("bi", "ins"):
        build_tree(s, case)
    if case["op"] == "ins":
        s.key = s.alloc(8)
        s.write(s.key, struct.pack(s.hf, case["key"]))
    if case["op"] == "load":
        s.buf = s.alloc(8)
        s.write(s.buf, struct.pack(s.hf, case["init"]))
        s.filep = s.alloc(8)
        s.ptr(s.filep, case["handle"])


def execute(s, case):
    op = case["op"]
    if op == "bi":
        return s.call_bi(s.nodes[case["node"]], s.nodes[case["root"]])
    if op == "ins":
        end = s.tree + s.ROOT
        par = end if case["parent"] < 0 else s.nodes[case["parent"]]
        s.parent_addr = par
        return s.call_ins(s.tree, par, case["left"], case["leftmost"], s.key)
    return s.call_load(s.buf, s.filep)


def dump(s, case, answer):
    end = s.tree + s.ROOT
    allnodes = list(s.nodes)
    if case["op"] == "ins" and answer:
        allnodes.append(answer)
    amap = {0: -1, end: -2}
    for i, a in enumerate(allnodes):
        amap[a] = i

    def m(p):
        return amap.get(p, -3)

    out = []
    for a in allnodes:
        par = s.rd_ptr(a + s.P)
        out.append((m(s.rd_ptr(a)), m(s.rd_ptr(a + s.R)), m(par & ~1), par & 1, s.half(a + s.V)))
    return {"nodes": out, "root": m(s.rd_ptr(end)), "count": struct.unpack(s.wf, s.read(s.tree, 4))[0]}


def observe(s, case, answer):
    if case["op"] == "load":
        return {"answer": answer, "log": s.log(), "buf": struct.unpack(s.hf, s.read(s.buf, 2))[0]}
    d = {"log": s.log()}
    if case["op"] == "ins":
        d["isnode"] = answer != 0
        d["parent_is_end"] = None
    d.update(dump(s, case, answer))
    if case["op"] == "ins":
        d["begin_ok"] = s.begin_ok(s.tree, answer if case["leftmost"] else s.begin0)
    return d


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


def gen_shape(r, n):
    shape = []
    free = [(-1, 0)]
    for i in range(n):
        slot = r.choice(free)
        free.remove(slot)
        red = 0 if i == 0 else r.randrange(2)
        shape.append((slot[0], slot[1], red, r.randrange(65536)))
        free += [(i, 0), (i, 1)]
    return shape, free


def bi_case(r, n=None):
    n = r.randrange(1, 21) if n is None else n
    shape, _ = gen_shape(r, n)
    # favour a node whose parent is red: that is where the loop works
    cand = [i for i, (p, _, _, _) in enumerate(shape) if p >= 0 and shape[p][2] == 1]
    node = r.choice(cand) if cand and r.random() < 0.8 else r.randrange(n)
    root = 0 if r.random() < 0.85 else r.randrange(n)
    return {"op": "bi", "queues": {}, "shape": shape, "count": n, "node": node, "root": root}


def ins_case(r, n=None, count=None):
    n = r.randrange(0, 21) if n is None else n
    shape, free = gen_shape(r, n)
    if n:
        free = [f for f in free if f[0] >= 0]
    slot = r.choice(free)
    return {"op": "ins", "queues": {}, "shape": shape, "count": n if count is None else count, "parent": slot[0], "left": 1 - slot[1] if False else (1 if slot[1] == 0 else 0),
            "leftmost": r.randrange(2), "key": r.choice([0, 1, 0xFFFF, 0x1234, r.randrange(65536)])}


def load_case(r, xfer=None, data=None):
    return {"op": "load", "queues": {"xfer": [r.choice([0, 1, 2, 2, 3]) if xfer is None else xfer],
                                     "data": [r.randrange(65536) if data is None else data]},
            "init": r.randrange(65536), "handle": r.choice([0, 1, 0x1234, r.randrange(1, 1 << 31)])}


def bi_edges():
    r = random.Random(5101)
    for n in (1, 1, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15):
        for _ in range(4):
            yield bi_case(r, n)


def bi_random(r):
    return bi_case(r)


def ins_edges():
    r = random.Random(5102)
    yield ins_case(r, 0)
    yield ins_case(r, 0, 0xFFFFFFFF)
    yield ins_case(r, 3, 0xFFFFFFFF)
    yield ins_case(r, 3, 0xFFFFFFFE)
    yield ins_case(r, 4, 0)
    for n in (1, 1, 2, 3, 4, 5, 6, 8, 10, 14):
        for _ in range(4):
            yield ins_case(r, n)


def ins_random(r):
    return ins_case(r, count=0xFFFFFFFF if r.random() < 0.05 else None)


def load_edges():
    r = random.Random(5103)
    for x in (0, 1, 2, 3):
        for d in (0, 0x0102, 0xFFFF, 0x8001):
            yield load_case(r, x, d)


def load_random(r):
    return load_case(r)


TESTS = [
    FnTest("std::__red_black_tree<__int128>::balance_insert", ppc, host, edge=bi_edges, rnd=bi_random),
    FnTest("std::__tree<CSubscription,std::less<CSubscription>,std::allocator<CSubscription>>::insert_node_at", ppc, host,
           edge=ins_edges, rnd=ins_random),
    FnTest("CSubscription::Load", ppc, host, edge=load_edges, rnd=load_random),
]
