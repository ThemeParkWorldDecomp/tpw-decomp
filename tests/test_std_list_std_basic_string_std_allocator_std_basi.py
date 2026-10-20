"""Unit v5086, the MSL std::list<std::string> members insert, sort, erase (range), erase (one) and merge, against
libtpw_unit_std_list_std_basic_string_std_allocator_std_basi.dylib (tests/unit_std_list_std_basic_string_std_allocator_std_basi.cpp),
main app ("SimTheme Park"), src/ui_Postcard_panel_std_list_std_basic_string_std_allocator_std_basi.cpp:
  insert 0x101b25e0, sort 0x1020811c, erase 0x102084d8 (range), erase 0x10208838 (one node), merge 0x10209898

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's 32-bit addressable arena (little-endian) for the translation; the same code reads both back.
The callees outside the unit are fakes on both sides that record the same events: operator new / new[] (they hand out blocks from a pool
the case allocated, so the result has a label), operator delete, strlen and strcpy (silent, real behaviour), __throw (the type name, the
message and the exception object's vtable, destructor table, text and counter), the _RefCountedPtr constructor, the string copy
constructor (copies the rep word), the string operator< (compares the first words of the two strings, unsigned), list clear (empties the
list) and list splice (moves every node of the other list before pos). sort and merge call each other and themselves: those run as the
original on the PowerPC side.

The world of a case: one or two lists (count word, sentinel prev and next) with nodes (prev, next, string word), the node strings being
keys (sort, merge, insert) or shared representations with a count and a data block (the erases), a pool of 64-byte blocks, and a source
string word. What is compared: the answer (the iterator, as a label), every word of every block (addresses as labels) and the event log.

No fused operations. Not tested: a list whose count word differs from its node count in sort (it walks past the list), a negative count
in sort, a string word of 0 in the erases (the original reads the representation without a check).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_std_basic_string_std_allocator_std_basi"
MASK = 0xFFFFFFFF
PFX = "tpw_sls_"

F_INSERT, F_SORT, F_ERASE_R, F_ERASE_1, F_MERGE = 0x101B25E0, 0x1020811C, 0x102084D8, 0x10208838, 0x10209898
SIZES = {F_INSERT: 376, F_SORT: 348, F_ERASE_R: 312, F_ERASE_1: 268, F_MERGE: 676}
OWN = set(SIZES)

STRLEN, STRCPY, OP_NEW, OP_NEWA, OP_DELETE, THROW = 0x10238650, 0x10238410, 0x102384B8, 0x10238668, 0x10238218, 0x10238680
RC_CTOR, STR_CTOR, STR_LT, LIST_CLEAR, LIST_SPLICE = 0x100026AC, 0x101B2918, 0x10209F04, 0x1017FC04, 0x1020ABEC
VTBL_IDS = {0x10278B74: 1, 0x10278BB0: 2, 0x10278BC0: 3}
DTOR_TV = 0x10265680

# sizeof(MslStrNode), its mStr, sizeof(MslStrList), its mHead, sizeof(MslStrRep), its mData (the Mac's: 12 8 12 4 16 12)
HOST_LAYOUT = [24, 16, 24, 8, 24, 16]

NAME = "std::list<std::basic_string<char,std::char_traits<char>,std::allocator<char>>,std::allocator<std::basic_string<char,std::char_traits<char>,std::allocator<char>>>>::"

POOL_N, POOL_SIZE = 6, 64
MAXN = 16
CUR = [None]      # the PowerPC side of the running case, for the stubs


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rd(e, a):
    return e.u32(a & MASK)


def wr(e, a, v):
    e.write(a & MASK, struct.pack(">I", v & MASK))


def cbytes(e, a):
    s = e.cstr(a)
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


def stub_strlen(e):
    e.set_r(3, len(cbytes(e, e.r(3))))


def stub_strcpy(e):
    dst = e.r(3)
    e.write(dst, cbytes(e, e.r(4)) + b"\0")
    e.set_r(3, dst)


def take(e, name):
    s = CUR[0]
    size = e.r(3) & MASK
    p = s.pool[s.next]
    s.next += 1
    e.cl.rec(name, (size, e.cl.lab(p)))
    e.set_r(3, p)


def stub_new(e):
    take(e, "new")


def stub_newa(e):
    take(e, "new[]")


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_throw(e):
    L = e.cl
    typ, obj, dtor = e.r(3), e.r(4), e.r(5)
    ptr, cnt = rd(e, obj + 4), rd(e, obj + 8)
    L.rec("throw", (fnv(cbytes(e, typ)), fnv(cbytes(e, ptr)), VTBL_IDS.get(rd(e, obj), 0), 1 if dtor == DTOR_TV else 0,
                    L.lab(ptr), L.lab(cnt)))


def stub_rc_ctor(e):
    L = e.cl
    me, p = e.r(3), e.r(4)
    L.rec("rcctor", (L.lab(me), L.lab(p)))
    wr(e, me, p)
    wr(e, me + 4, 0)


def stub_str_ctor(e):
    L = e.cl
    me, src, pos, n = e.r(3), e.r(4), e.r(5) & MASK, e.r(6) & MASK
    L.rec("strctor", (L.lab(me), L.lab(src), pos, n))
    wr(e, me, rd(e, src))


def stub_lt(e):
    L = e.cl
    a, b = e.r(3), e.r(4)
    L.rec("lt", (L.lab(a), L.lab(b)))
    e.set_r(3, 1 if rd(e, a) < rd(e, b) else 0)


def stub_clear(e):
    L = e.cl
    me = e.r(3)
    L.rec("clear", (L.lab(me),))
    wr(e, me, 0)
    wr(e, me + 4, me + 4)
    wr(e, me + 8, me + 4)


def stub_splice(e):
    L = e.cl
    me, pos, oth = e.r(3), e.r(4), e.r(5)
    L.rec("splice", (L.lab(me), L.lab(pos), L.lab(oth)))
    if rd(e, oth + 8) != (oth + 4) & MASK:
        first, last, pp = rd(e, oth + 8), rd(e, oth + 4), rd(e, pos)
        wr(e, pp + 4, first)
        wr(e, first, pp)
        wr(e, last + 4, pos)
        wr(e, pos, last)
        wr(e, me, rd(e, me) + rd(e, oth))
        wr(e, oth, 0)
        wr(e, oth + 4, oth + 4)
        wr(e, oth + 8, oth + 4)


for _n, _f in (("strlen", stub_strlen), ("strcpy", stub_strcpy), ("__nw__FUl", stub_new), ("__nwa__FUl", stub_newa),
               ("__dl__FPv", stub_delete), ("__throw", stub_throw)):
    STUBS.imp(_n, _f)
for _a, _f in ((RC_CTOR, stub_rc_ctor), (STR_CTOR, stub_str_ctor), (STR_LT, stub_lt), (LIST_CLEAR, stub_clear),
               (LIST_SPLICE, stub_splice)):
    STUBS.internal(_a, _f)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """PW: pointer width, H: the list's sentinel offset, REPD: the representation's data pointer offset (the Mac's 4, 4, 12)."""

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.PW))[0]

    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def tr(self, w):
        return ("L", self.rev[w]) if w in self.rev else w

    def rpt(self, a):
        return self.tr(self.rp(a))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PW, H, REPD = 4, 4, 12

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.pool = []
        self.next = 0
        CUR[0] = self
        self.saved = None

    def begin(self, labels):
        self.rev = dict(labels)
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, W, case):
        res = W["res"]
        lst = W["L"][0]
        args = {"insert": lambda: [res, lst, case["pos"] & MASK, W["src"]],
                "sort": lambda: [lst],
                "erase_r": lambda: [res, lst, case["first"] & MASK, case["last"] & MASK],
                "erase_1": lambda: [res, lst, case["pos"] & MASK],
                "merge": lambda: [lst, W["L"][0] if case["same"] else W["L"][1]]}[op]()
        entry = {"insert": F_INSERT, "sort": F_SORT, "erase_r": F_ERASE_R, "erase_1": F_ERASE_1, "merge": F_MERGE}[op]
        T.call_ppc(self.e, entry, args)
        return self.rpt(res) if op in ("insert", "erase_r", "erase_1") else 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PW, H, REPD = 8, 8, 16

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, labels):
        self.rev = dict(labels)
        CL.host_begin(self.lib, {lab: a for a, lab in labels.items()}, self.case["queues"])
        arr = (C.c_uint64 * POOL_N)(*self.pool)
        self.lib.tpw_sls_pool(arr, POOL_N)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sls_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, W, case):
        lib = self.lib
        lst = W["L"][0]
        if op == "insert":
            return self.tr(lib.tpw_sls_insert(lst, case["pos"], W["src"]))
        if op == "sort":
            lib.tpw_sls_sort(lst)
            return 0
        if op == "erase_r":
            return self.tr(lib.tpw_sls_erase_r(lst, case["first"], case["last"]))
        if op == "erase_1":
            return self.tr(lib.tpw_sls_erase_1(lst, case["pos"]))
        lib.tpw_sls_merge(lst, W["L"][0] if case["same"] else W["L"][1])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sls_alloc.restype, lib.tpw_sls_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sls_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sls_pool.argtypes = [C.POINTER(C.c_uint64), C.c_int32]
    lib.tpw_sls_insert.restype, lib.tpw_sls_insert.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_sls_sort.restype, lib.tpw_sls_sort.argtypes = None, [C.c_uint64]
    lib.tpw_sls_erase_r.restype, lib.tpw_sls_erase_r.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_sls_erase_1.restype, lib.tpw_sls_erase_1.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_sls_merge.restype, lib.tpw_sls_merge.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sls_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_world(s, case):
    n0, n1 = case["n"]
    P, H = s.PW, s.H
    W = {"L": [s.alloc(H + 2 * P), s.alloc(H + 2 * P)]}
    W["nodes"] = [s.alloc(3 * P) for _ in range(n0 + n1)]
    W["reps"] = [s.alloc(s.REPD + P) for _ in range(n0 + n1)]
    W["data"] = [s.alloc(8) for _ in range(n0 + n1)]
    W["pool"] = [s.alloc(POOL_SIZE) for _ in range(POOL_N)]
    W["src"] = s.alloc(P)
    W["res"] = s.alloc(P)
    s.pool = list(W["pool"])
    labels = {W["L"][0]: 1, W["L"][0] + H: 2, W["L"][1]: 3, W["L"][1] + H: 4, W["src"]: 5}
    for k, a in enumerate(W["nodes"]):
        labels[a] = 10 + k
        labels[a + 2 * P] = 40 + k
        labels[W["reps"][k]] = 70 + k
        labels[W["data"][k]] = 200 + k
    for k, a in enumerate(W["pool"]):
        labels[a] = 300 + k
        labels[a + 2 * P] = 330 + k
    return W, labels


def build(s, W, case):
    base = 0
    P, H = s.PW, s.H
    for li in (0, 1):
        n = case["n"][li]
        head = W["L"][li]
        sent = head + H
        nodes = W["nodes"][base:base + n]
        s.w32(head, case["sizes"][li])
        s.wp(head + H, nodes[-1] if n else sent)
        s.wp(head + H + P, nodes[0] if n else sent)
        for k, a in enumerate(nodes):
            s.wp(a, nodes[k - 1] if k else sent)
            s.wp(a + P, nodes[k + 1] if k + 1 < n else sent)
            s.wp(a + 2 * P, W["reps"][base + k] if case["reps"] else case["keys"][li][k])
        base += n
    if case["reps"]:
        for k in range(len(W["nodes"])):
            r = W["reps"][k]
            s.w32(r, 5)
            s.w32(r + 4, 16)
            s.w32(r + 8, case["counts"][k])
            s.wp(r + s.REPD, W["data"][k])
    s.wp(W["src"], case["srckey"])


def pool_roles(case):
    """What the pool blocks hold after the call, in the order the original takes them."""
    if case["op"] == "insert":
        return ["text", "node"] if case["sizes"][0] > 0x7FFFFFFE else ["node"]
    if case["op"] == "merge" and not case["same"] and case["sizes"][0] > 0x7FFFFFFF - case["sizes"][1]:
        return ["text", "count"]
    return []


def observe(s, W, case, answer):
    P, H = s.PW, s.H
    mem = []
    for a in W["L"]:
        mem += [s.r32(a), s.rpt(a + H), s.rpt(a + H + P)]
    for a in W["nodes"]:
        mem += [s.rpt(a), s.rpt(a + P), s.rpt(a + 2 * P)]
    for a in W["reps"]:
        mem += [s.r32(a), s.r32(a + 4), s.r32(a + 8), s.rpt(a + s.REPD)]
    for a in W["data"]:
        mem.append(bytes(s.read(a, 8)))
    roles = pool_roles(case)
    for k, a in enumerate(W["pool"]):
        role = roles[k] if k < len(roles) else "zero"
        if role == "text":
            mem.append(bytes(s.read(a, POOL_SIZE)).split(b"\0")[0])
            mem.append(bytes(s.read(a, POOL_SIZE)).rstrip(b"\0") == bytes(s.read(a, POOL_SIZE)).split(b"\0")[0])
        elif role == "node":
            mem += [s.rpt(a), s.rpt(a + P), s.rpt(a + 2 * P)]
        elif role == "count":
            mem.append(s.r32(a))
        else:
            mem.append(bytes(s.read(a, POOL_SIZE)))
    mem.append(s.rpt(W["src"]))
    return {"answer": answer, "log": s.log(), "mem": mem}


def it(s, W, case, idx, li=0):
    """The iterator for position idx of list li: a node, or the sentinel at the end."""
    base = 0 if li == 0 else case["n"][0]
    if idx < case["n"][li]:
        return W["nodes"][base + idx]
    return W["L"][li] + s.H


def run(s, case):
    try:
        W, labels = make_world(s, case)
        s.begin(labels)
        build(s, W, case)
        c = dict(case)
        if "ifirst" in case:
            c["first"], c["last"] = it(s, W, case, case["ifirst"]), it(s, W, case, case["ilast"])
        if "ipos" in case:
            c["pos"] = it(s, W, case, case["ipos"])
        return observe(s, W, case, s.call(case["op"], W, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases


def base(r, op, n0=0, n1=0):
    return {"op": op, "queues": {}, "n": [n0, n1], "sizes": [n0, n1], "keys": [[r.randrange(0, 6) for _ in range(n0)],
            [r.randrange(0, 6) for _ in range(n1)]], "reps": False, "counts": [0] * (n0 + n1), "srckey": r.randrange(1, 1000),
            "same": False}


COUNTS = [0, 1, 2, 3, 0xFFFFFFFF, 5]

# ---- insert
BIG = [0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0]


def insert_case(r, n=None, pos=None, size=None):
    n = r.randrange(0, 5) if n is None else n
    c = base(r, "insert", n)
    c["ipos"] = r.randrange(0, n + 1) if pos is None else pos
    if size is not None:
        c["sizes"][0] = size
    elif r.random() < 0.15:
        c["sizes"][0] = r.choice(BIG)
    return c


def insert_edges():
    r = random.Random(11)
    for n, pos in ((0, 0), (1, 0), (1, 1), (3, 0), (3, 2), (3, 3)):
        yield insert_case(r, n, pos)
    for size in BIG:
        yield insert_case(r, 2, 1, size)


# ---- sort
def sort_case(r, n=None, keys=None):
    n = r.randrange(0, 10) if n is None else n
    c = base(r, "sort", n)
    if keys is not None:
        c["keys"][0] = list(keys)
    return c


def sort_edges():
    r = random.Random(12)
    for keys in ([], [3], [1, 2], [2, 1], [2, 2], [1, 2, 3], [3, 2, 1], [1, 3, 2], [2, 1, 3], [1, 1, 1, 1], [4, 3, 2, 1],
                 [1, 2, 3, 4, 5], [5, 4, 3, 2, 1, 0, 0], [0, 1, 2, 3, 4, 5, 6, 7]):
        yield sort_case(r, len(keys), keys)


# ---- erase
def erase_r_case(r, n=None, i=None, j=None):
    n = r.randrange(0, 7) if n is None else n
    c = base(r, "erase_r", n)
    c["reps"] = True
    c["counts"] = [r.choice(COUNTS) for _ in range(n)]
    i = r.randrange(0, n + 1) if i is None else i
    j = r.randrange(i, n + 1) if j is None else j
    c["ifirst"], c["ilast"] = i, j
    return c


def erase_r_edges():
    r = random.Random(13)
    for n, i, j in ((0, 0, 0), (1, 0, 0), (1, 1, 1), (1, 0, 1), (3, 0, 3), (3, 1, 2), (3, 0, 1), (3, 2, 3), (4, 1, 3), (4, 2, 2)):
        yield erase_r_case(r, n, i, j)
    for counts in ([0, 0, 0], [1, 1, 1], [2, 2, 2], [3, 1, 0], [0xFFFFFFFF, 2, 1]):
        c = erase_r_case(r, 3, 0, 3)
        c["counts"] = counts
        yield c


def erase_1_case(r, n=None, p=None):
    n = r.randrange(1, 7) if n is None else n
    c = base(r, "erase_1", n)
    c["reps"] = True
    c["counts"] = [r.choice(COUNTS) for _ in range(n)]
    c["ipos"] = r.randrange(0, n) if p is None else p
    return c


def erase_1_edges():
    r = random.Random(14)
    for n, p in ((1, 0), (2, 0), (2, 1), (3, 1), (4, 3)):
        yield erase_1_case(r, n, p)
    for cnt in COUNTS:
        c = erase_1_case(r, 2, 0)
        c["counts"][0] = cnt
        yield c


# ---- merge
def merge_case(r, n0=None, n1=None, k0=None, k1=None, sizes=None, same=False):
    n0 = r.randrange(0, 7) if n0 is None else n0
    n1 = r.randrange(0, 7) if n1 is None else n1
    c = base(r, "merge", n0, n1)
    if k0 is not None:
        c["keys"] = [list(k0), list(k1)]
        c["n"] = [len(k0), len(k1)]
        c["sizes"] = list(c["n"])
    elif r.random() < 0.8:
        c["keys"] = [sorted(c["keys"][0]), sorted(c["keys"][1])]
    if sizes is not None:
        c["sizes"] = list(sizes)
    elif r.random() < 0.08:
        c["sizes"] = [r.choice([0x7FFFFFFF, 0x7FFFFFFE, 0x40000000, 1]), r.choice([1, 2, 0x7FFFFFFF, 0x40000001])]
    c["same"] = same
    return c


def merge_edges():
    r = random.Random(15)
    for k0, k1 in (([], []), ([1], []), ([], [1]), ([1], [2]), ([2], [1]), ([1], [1]), ([1, 3], [2]), ([2], [1, 3]), ([1, 2], [3, 4]),
                   ([3, 4], [1, 2]), ([1, 5], [2, 3, 4]), ([2, 3, 4], [1, 5]), ([1, 1, 2], [1, 1, 2]), ([1, 4], [2, 3, 5, 6]),
                   ([3, 2, 1], [5, 4, 0])):
        yield merge_case(r, k0=k0, k1=k1)
    for sizes in ([0x7FFFFFFF, 1], [0x7FFFFFFE, 1], [0x7FFFFFFF, 0], [1, 0x7FFFFFFF], [0x80000000, 0x80000000], [0x40000000, 0x3FFFFFFF],
                  [0x40000000, 0x40000000], [0xFFFFFFFF, 1]):
        yield merge_case(r, k0=[1, 3], k1=[2], sizes=sizes)
    yield merge_case(r, k0=[1, 3], k1=[2], same=True)
    yield merge_case(r, k0=[], k1=[], same=True)


TESTS = [
    FnTest(NAME + "insert", ppc, host, edge=insert_edges, rnd=insert_case),
    FnTest(NAME + "sort", ppc, host, edge=sort_edges, rnd=sort_case),
    FnTest(NAME + "erase", ppc, host, edge=erase_r_edges, rnd=erase_r_case),
    FnTest(NAME + "erase", ppc, host, edge=erase_1_edges, rnd=erase_1_case),
    FnTest(NAME + "merge", ppc, host, edge=merge_edges, rnd=merge_case),
]
