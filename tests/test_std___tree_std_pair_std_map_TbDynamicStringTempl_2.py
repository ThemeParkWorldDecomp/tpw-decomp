"""Unit v5544, the theme map's tree (ParkThemeTree), against libtpw_unit_std___tree_std_pair_std_map_TbDynamicStringTempl_2.dylib
(tests/unit_std___tree_std_pair_std_map_TbDynamicStringTempl_2.cpp), main app ("SimTheme Park"),
src/Serialise_basic_std___tree_std_pair_std_map_TbDynamicStringTempl_2.cpp:
  destroy 0x1016d784, insert_node_at 0x1016eb98

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
32-bit words) for the original, and in the host library's memory (little-endian, 64-bit pointers) for the translation, so the code takes the
field offsets from the side. Every callee outside the unit is a fake on both sides that records its arguments: the allocator's destroy and
deallocate, the pair's destructor and copy constructor, the key's destructor, operator new / new[] / delete, strlen, strcpy, the
_RefCountedPtr constructor, __throw and balance_insert. Addresses are logged as labels (the tree, the nodes, the value, the library
globals); the interior pointers (a node's pair) are logged as the label of their node. Texts are logged as a hash of their bytes.

destroy: a random binary tree of nodes (all shapes down to depth 5, so the inlined levels and the recursive calls are all met); compared:
the whole call log (its order too). insert_node_at: the tree's four words, a parent (the end node, the root or another node) with
pre-filled words, left and leftmost flags, a count from 0 to 0xffffffff (the length error: __throw returns and the insertion goes on),
operator new answering a block whose old parent word has a random colour bit. Compared: the answer, the tree words, the parent's words, the
new node's words (parent pointer and colour bit apart) and the call log.

No fused operations. Not tested: a NULL subtree in destroy and a NULL parent in insert_node_at (the original reads them unconditionally);
the exception unwinding path of the pair constructor (not reachable, the fake does not throw); the Mac node size (0x24) is logged for the
host's larger node, since the host's pointers are 8 bytes.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_std_pair_std_map_TbDynamicStringTempl_2"
MASK = 0xFFFFFFFF

FN_D, FN_I = 0x1016D784, 0x1016EB98
SIZES = {FN_D: 496, FN_I: 400}
OWN = set(SIZES)

PAIR_DESTROY, DEALLOCATE, PAIR_DTOR, PAIR_COPY, BALANCE = 0x1016DFD0, 0x1016DC80, 0x1016B168, 0x1016F080, 0x10153B24
KEY_DTOR, OP_DELETE, OP_NEW, NEW_ARRAY = 0x102382F0, 0x10238218, 0x102384B8, 0x10238668
STRLEN, STRCPY, THROW, RC_CTOR = 0x10238650, 0x10238410, 0x10238680, 0x100026AC

ADDR = dict(vtblB=0x10278BB0, vtblC=0x10278BC0, vtblLen=0x10278B74, dtorTV=0x10265680)
GLOBAL_LABELS = dict(vtblB=900, vtblC=901, vtblLen=902, dtorTV=903)
# the offsets of the host's pointer-sized fields: node mRight, mParent, mValue; tree mRoot, mSlot8, mBegin
HOST_LAYOUT = [8, 16, 24, 8, 16, 24]

STATE = {"new": 0}


def s32(v):
    return T.s32(v)


def text_of(e, a):
    t = e.cstr(a)
    return t.encode("latin1") if isinstance(t, str) else bytes(t)


def hsh(b):
    h = 0
    for c in b:
        h = (h * 31 + c) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_pair_destroy(e):
    L = e.cl
    L.rec("PairDestroy", (L.lab(e.r(3)), L.lab(e.r(4) - 0xC)))


def st_dealloc(e):
    L = e.cl
    L.rec("NodeDeallocate", (L.lab(e.r(3) - 4), L.lab(e.r(4)), e.r(5)))


def st_pair_dtor(e):
    L = e.cl
    L.rec("PairDtor", (L.lab(e.r(3) - 0xC), T.s16(e.r(4))))


def st_pair_copy(e):
    L = e.cl
    L.rec("PairCopy", (s32(e.r(3) - STATE["new"] - 0xC), L.lab(e.r(4))))


def st_balance(e):
    L = e.cl
    L.rec("BalanceInsert", (1 if e.r(3) == STATE["new"] else 0, L.lab(e.r(4))))


def st_key_dtor(e):
    assert e.r(4) == MASK, "the hidden delete flag is -1"
    L = e.cl
    L.rec("KeyDtor", (L.lab(e.r(3) - 0xC),))


def st_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


def st_new(e):
    L = e.cl
    L.rec("New", (e.r(3),))
    a = e.alloc(0x100)
    e.write(a + 8, struct.pack(">I", L.ret("new") & MASK))
    STATE["new"] = a
    e.set_r(3, a)


def st_new_array(e):
    L = e.cl
    L.rec("NewArray", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)))


def st_strlen(e):
    L = e.cl
    t = text_of(e, e.r(3))
    L.rec("Strlen", (hsh(t),))
    e.set_r(3, len(t))


def st_strcpy(e):
    L = e.cl
    t = text_of(e, e.r(4))
    L.rec("Strcpy", (hsh(t),))
    e.write(e.r(3), t + b"\0")


def st_rc_ctor(e):
    L = e.cl
    L.rec("RcCtor", (1 if e.r(4) else 0,))
    e.write(e.r(3), struct.pack(">I", e.r(4)))


def st_throw(e):
    L = e.cl
    obj = e.r(4)
    L.rec("Throw", (hsh(text_of(e, e.r(3))), L.lab(e.u32(obj)), L.lab(e.r(5)), hsh(text_of(e, e.u32(obj + 4)))))


def glue(fn):
    """A glue import's fake: the real glue saves the TOC at 0x14(r1) for the caller's reload after the call."""
    def stub(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return stub


for _a, _f in ((PAIR_DESTROY, st_pair_destroy), (DEALLOCATE, st_dealloc), (PAIR_DTOR, st_pair_dtor), (PAIR_COPY, st_pair_copy),
               (BALANCE, st_balance), (RC_CTOR, st_rc_ctor)):
    STUBS.internal(_a, _f)
for _a, _f in ((KEY_DTOR, st_key_dtor), (OP_DELETE, st_delete), (OP_NEW, st_new), (NEW_ARRAY, st_new_array),
               (STRLEN, st_strlen), (STRCPY, st_strcpy), (THROW, st_throw)):
    STUBS.internal(_a, glue(_f))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.PS))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PS = 4
    NL, NR, NP, NV = 0, 4, 8, 0xC
    TC, TR, TS, TB = 0, 4, 8, 0xC

    def __init__(self, e, case):
        self.e = e
        STATE["new"] = 0
        self.world = build(self, case)
        fixed = dict(self.world["names"])
        for k, lab in GLOBAL_LABELS.items():
            fixed[ADDR[k]] = lab
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call_destroy(self, tree, node):
        T.call_ppc(self.e, FN_D, [tree & MASK, node & MASK])

    def call_insert(self, tree, parent, left, leftmost, value):
        r3, _, _ = T.call_ppc(self.e, FN_I, [tree & MASK, parent & MASK, left, leftmost, value & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PS = 8
    NL, NR, NP, NV = 0, 8, 16, 24
    TC, TR, TS, TB = 0, 8, 16, 24

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = build(self, case)
        fixed = {lab: a for a, lab in self.world["names"].items()}
        for k, lab in GLOBAL_LABELS.items():
            fixed[lab] = lib.tpw_pt_addr(k.encode())
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_destroy(self, tree, node):
        self.lib.tpw_pt_destroy(tree, node)

    def call_insert(self, tree, parent, left, leftmost, value):
        return self.lib.tpw_pt_insert(tree, parent, left, leftmost, value)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pt_addr.restype, lib.tpw_pt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pt_alloc.restype, lib.tpw_pt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pt_destroy.restype, lib.tpw_pt_destroy.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_pt_insert.restype = C.c_uint64
    lib.tpw_pt_insert.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_node(s, shape, names, counter):
    a = s.alloc(0x80)
    names[a] = counter[0]
    counter[0] += 1
    left = build_node(s, shape[0], names, counter) if shape[0] else 0
    right = build_node(s, shape[1], names, counter) if shape[1] else 0
    s.ptr(a + s.NL, left)
    s.ptr(a + s.NR, right)
    return a


def build(s, case):
    names = {}
    tree = s.alloc(0x80)
    names[tree] = 1
    w = {"tree": tree, "names": names}
    if case["op"] == "destroy":
        w["node"] = build_node(s, case["shape"], names, [10])
        return w
    value = s.alloc(0x80)
    names[value] = 2
    w["value"] = value
    tok = case["tok"]
    s.words(tree + s.TC, [case["count"]])
    root = 0
    if case["has_root"]:
        root = s.alloc(0x80)
        names[root] = 3
        s.ptr(root + s.NL, tok[0])
        s.ptr(root + s.NR, tok[1])
        s.ptr(root + s.NP, tok[2])
    names[tree + s.TR] = 5
    s.ptr(tree + s.TR, root)
    s.ptr(tree + s.TS, tok[3])
    s.ptr(tree + s.TB, {0: tree + s.TR, 1: root or (tree + s.TR), 2: tok[4]}[case["begin"]])
    if case["pk"] == 0:
        parent = tree + s.TR
    elif case["pk"] == 1:
        parent = root
    else:
        parent = s.alloc(0x80)
        names[parent] = 4
        s.ptr(parent + s.NL, tok[0])
        s.ptr(parent + s.NR, tok[1])
        s.ptr(parent + s.NP, tok[2])
    w["parent"] = parent
    return w


def execute(s, case):
    w = s.world
    if case["op"] == "destroy":
        s.call_destroy(w["tree"], w["node"])
        return None
    return s.call_insert(w["tree"], w["parent"], case["left"], case["leftmost"], w["value"])


def observe(s, case, answer):
    out = {"log": s.log()}
    if case["op"] == "destroy":
        return out
    w = s.world
    names = dict(w["names"])
    names[answer] = 6

    def mp(v):
        return 0 if v == 0 else names.get(v, ("raw", v))

    def pw(a):
        v = s.rd_ptr(a)
        return (mp(v & ~1), v & 1)

    tree = w["tree"]
    out["answer"] = mp(answer)
    out["count"] = s.rd_words(tree + s.TC, 1)
    out["root"] = mp(s.rd_ptr(tree + s.TR))
    out["slot8"] = mp(s.rd_ptr(tree + s.TS))
    out["begin"] = mp(s.rd_ptr(tree + s.TB))
    if case["pk"] != 0:
        p = w["parent"]
        out["parent"] = (mp(s.rd_ptr(p + s.NL)), mp(s.rd_ptr(p + s.NR)), pw(p + s.NP))
    out["new"] = (mp(s.rd_ptr(answer + s.NL)), mp(s.rd_ptr(answer + s.NR)), pw(answer + s.NP))
    return out


def run(s, case):
    try:
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rshape(r, depth):
    if depth == 0:
        return (None, None)
    return (rshape(r, depth - 1) if r.random() < 0.6 else None, rshape(r, depth - 1) if r.random() < 0.6 else None)


def chain(n, side):
    shape = (None, None)
    for _ in range(n):
        shape = (shape, None) if side == 0 else (None, shape)
    return shape


def full(n):
    return (None, None) if n == 0 else (full(n - 1), full(n - 1))


def destroy_case(shape):
    return {"op": "destroy", "queues": {}, "shape": shape}


def destroy_edges():
    leaf = (None, None)
    for shape in (leaf, (leaf, None), (None, leaf), (leaf, leaf), full(2), full(3), full(4), chain(5, 0), chain(5, 1),
                  chain(2, 0), chain(3, 1), ((leaf, None), (None, leaf)), ((None, leaf), (leaf, None)),
                  (chain(3, 0), chain(3, 1)), (full(2), None), (None, full(2))):
        yield destroy_case(shape)


def destroy_random(r):
    return destroy_case(rshape(r, r.randrange(0, 6)))


def insert_case(r, count=None, pk=None, left=None, leftmost=None, bit=None):
    pk = r.randrange(3) if pk is None else pk
    garbage = r.getrandbits(32) & ~1
    if bit is None:
        bit = r.getrandbits(1)
    return {
        "op": "insert",
        "queues": {"new": [garbage | bit]},
        "count": r.choice([0, 1, 7, 100, 0xFFFFFFFD, 0xFFFFFFFE, 0xFFFFFFFF, r.getrandbits(32)]) if count is None else count,
        "pk": pk,
        "has_root": 1 if pk == 1 else r.getrandbits(1),
        "begin": r.randrange(3),
        "left": r.getrandbits(1) if left is None else left,
        "leftmost": r.getrandbits(1) if leftmost is None else leftmost,
        "tok": [r.choice([0, r.randrange(1, 0x1000) * 8 + r.getrandbits(1)]) for _ in range(5)],
    }


def insert_edges():
    r = random.Random(5544)
    i = 0
    for count in (0, 5, 0xFFFFFFFE, 0xFFFFFFFF):
        for pk in (0, 1, 2):
            for left in (0, 1):
                for leftmost in (0, 1):
                    yield insert_case(r, count, pk, left, leftmost, i & 1)
                    i += 1


def insert_random(r):
    return insert_case(r)


PREFIX = ("std::__tree<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>,std::map<TbDynamicStringTemplate<char>,"
          "CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,"
          "CFSGlobalParkData*>>>::value_compare,std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::")

TESTS = [
    FnTest(PREFIX + "destroy", ppc, host, edge=destroy_edges, rnd=destroy_random),
    FnTest(PREFIX + "insert_node_at", ppc, host, edge=insert_edges, rnd=insert_random),
]
