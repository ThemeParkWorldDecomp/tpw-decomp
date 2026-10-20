"""Unit v5993, the std::set<std::wstring> tree's insert_node_at, erase and destroy, against
libtpw_unit_std___tree_std_basic_string_std_less_std_basic_s_3.dylib, main app ("SimTheme Park"),
src/std___tree_std_basic_string_std_less_std_basic_s_3.cpp:
  insert_node_at 0x10227c78, erase 0x10228104, destroy 0x10223204

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4 byte words) for the original, and in the host library's memory (little-endian, 8 byte pointers) for the translation. The callees outside
the unit (operator new, new[], delete, strlen, strcpy, __throw, the string constructor and destructor, the allocator's destroy and
deallocate, balance_insert, balance_erase) are fakes on both sides that record their arguments.

The world of a case: one region of 64 byte slots: the tree (slot 0), up to 8 nodes (slots 1..8), the key string (10..12), the node strings'
representations (21..28) and their data blocks (31..38), and a pool (40..47) that operator new hands out, filled with a byte that depends
on the slot (so the colour bit of a fresh node is both 0 and 1). Addresses in the region are logged and compared as tags (slot, field),
which are the same on both sides. What is compared: the answer, the event log, the tree words, every node, every representation.

No fused operations. Not tested: the exception unwinding landing pads (never reached by a returning __throw fake).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_std_basic_string_std_less_std_basic_s_3"
MASK = 0xFFFFFFFF

FN_INSERT, FN_ERASE, FN_DESTROY = 0x10227C78, 0x10228104, 0x10223204
SIZES = {FN_INSERT: 544, FN_ERASE: 308, FN_DESTROY: 588}
OWN = set(SIZES)

SLOT = 64
NSLOTS = 48
POOL0 = 40
KEY_OBJ, KEY_REP, KEY_DATA = 10, 11, 12
NODE_SLOTS = list(range(1, 9))
POOL_SLOTS = list(range(40, 44))      # the nodes operator new hands out
SMALL0 = 44
SMALL_SLOTS = list(range(44, 48))     # the small blocks (new[] text, the shared count)

HOST_LAYOUT = [32, 24, 24, 16, 8, 16]

_B = "std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>"
_TREE = "std::__tree<%s,std::less<%s>,std::allocator<%s>>" % (_B, _B, _B)
NAME_INSERT = _TREE + "::insert_node_at"
NAME_ERASE = _TREE + "::erase"
NAME_DESTROY = _TREE + "::destroy"

TYPE_TEXT = "!std::exception!!std::logic_error!!std::length_error!!"
VT_LENGTH, VT_B, VT_C, TV_LENGTH = 0x10278B74, 0x10278BB0, 0x10278BC0, 0x10265680

ST = {"R": 0, "k": 0, "fill": 0}


def s16(v):
    return T.s16(v)


def ptag(v):
    """The tag of a Mac address: slot * 16 + offset / 4, -1 outside the region."""
    R = ST["R"]
    if v < R or v >= R + SLOT * NSLOTS:
        return -1
    o = v - R
    return (o // SLOT) * 16 + (o % SLOT) // 4


def rel(v):
    return v - ST["R"]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def pool(e, small):
    key = "ks" if small else "k"
    k = ST[key]
    ST[key] += 1
    a = ST["R"] + SLOT * ((SMALL0 if small else POOL0) + k)
    e.write(a, bytes([(ST["fill"] + k + (4 if small else 0)) & 255]) * SLOT)
    return a


def st_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, pool(e, e.r(3) != 0x10))


def st_newarr(e):
    e.cl.rec("new[]", (e.r(3),))
    e.set_r(3, pool(e, True))


def st_delete(e):
    e.cl.rec("delete", (ptag(e.r(3)),))


def cbytes(e, a):
    s = e.cstr(a)
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


def st_strlen(e):
    n = len(cbytes(e, e.r(3)))
    e.cl.rec("strlen", (n,))
    e.set_r(3, n)


def st_strcpy(e):
    s = cbytes(e, e.r(4))
    e.cl.rec("strcpy", (ptag(e.r(3)), len(s)))
    e.write(e.r(3), s + b"\0")


def st_throw(e):
    obj, a = e.r(4), e.r(3)
    vt = e.u32(obj)
    ptr = e.u32(obj + 4)
    cnt = e.u32(obj + 8)
    vid = {VT_LENGTH: 1, VT_B: 2, VT_C: 3}.get(vt, 0)
    s = cbytes(e, ptr) if ptag(ptr) >= 0 else b""
    total = 0
    for c in s:
        total = (total * 31 + c) & MASK
    ct = ptag(cnt)
    e.cl.rec("throw", (vid, ptag(ptr), ct, e.u32(cnt) if ct >= 0 else -1, total,
                       int(cbytes(e, a).decode("latin1") == TYPE_TEXT), int(e.r(5) == TV_LENGTH)))


def st_binsert(e):
    e.cl.rec("binsert", (ptag(e.r(3)), ptag(e.r(4))))


def st_berase(e):
    e.cl.rec("berase", (ptag(e.r(3)), ptag(e.r(4))))


def st_string(e):
    e.cl.rec("string", (ptag(e.r(3)), ptag(e.r(4)), e.r(5), e.r(6)))
    e.write(e.r(3), e.read(e.r(4), 4))


def st_sdtor(e):
    e.cl.rec("sdtor", (ptag(e.r(3)), s16(e.r(4))))


def st_adestroy(e):
    e.cl.rec("adestroy", (ptag(e.r(4)), rel(e.r(3))))


def st_dealloc(e):
    e.cl.rec("dealloc", (rel(e.r(3)), ptag(e.r(4)), e.r(5)))


def glue(fn):
    """An import's glue saves the TOC pointer in the frame's slot 0x14(r1); the caller reloads it after the call."""
    def run(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return run


STUBS.internal(0x102384B8, glue(st_new))
STUBS.internal(0x10238668, glue(st_newarr))
STUBS.internal(0x10238218, glue(st_delete))
STUBS.internal(0x10238650, glue(st_strlen))
STUBS.internal(0x10238410, glue(st_strcpy))
STUBS.internal(0x10238680, glue(st_throw))
STUBS.internal(0x10153B24, st_binsert)
STUBS.internal(0x10153290, st_berase)
STUBS.internal(0x101F7424, st_string)
STUBS.internal(0x101468F8, st_sdtor)
STUBS.internal(0x102238D4, st_adestroy)
STUBS.internal(0x10223670, st_dealloc)
STUBS.internal(0x10201850, lambda e: e.set_r(3, e.r(4)))      # placement new: answers its pointer


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def at(self, slot, off=0):
        return self.R + slot * SLOT + off

    def u32(self, a, v):
        self.write(a, struct.pack(self.f32, v & MASK))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fp, v))

    def rd32(self, a):
        return struct.unpack(self.f32, self.read(a, 4))[0]

    def rdp(self, a):
        return struct.unpack(self.fp, self.read(a, self.W))[0]

    def tag(self, v):
        if v < self.R or v >= self.R + SLOT * NSLOTS:
            return -1
        o = v - self.R
        return (o // SLOT) * 16 + (o % SLOT) // self.W


class Ppc(Side):
    W = 4
    DATA = 0x0C
    f32 = ">I"
    fp = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, {})
        self.L = e.cl
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

    def begin(self, case):
        ST["R"] = self.R
        ST["k"] = 0
        ST["ks"] = 0
        ST["fill"] = case["fill"]

    def call(self, entry, args):
        r3 = T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK
        return r3 if entry == FN_INSERT else 0      # erase and destroy answer void


class Host(Side):
    W = 8
    DATA = 0x10
    f32 = "<I"
    fp = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wst_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def begin(self, case):
        self.lib.tpw_wst_begin(self.R, NSLOTS, POOL0, case["fill"])

    def call(self, entry, args):
        if entry == FN_INSERT:
            return self.lib.tpw_wst_insert(*args)
        if entry == FN_ERASE:
            self.lib.tpw_wst_erase(*args)
            return 0
        self.lib.tpw_wst_destroy(*args)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wst_alloc.restype, lib.tpw_wst_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wst_begin.restype, lib.tpw_wst_begin.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_wst_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wst_insert.restype = C.c_uint64
    lib.tpw_wst_insert.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    lib.tpw_wst_erase.restype, lib.tpw_wst_erase.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_wst_destroy.restype, lib.tpw_wst_destroy.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wst_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.R = s.alloc(SLOT * NSLOTS)
    s.write(s.R, bytes(SLOT * NSLOTS))
    s.begin(case)
    W = s.W
    end = s.R + W

    def A(idx):
        return end if idx == -1 else (0 if idx == 0 else s.at(idx))

    t = case["tree"]
    s.u32(s.at(0), t["count"])
    s.ptr(s.at(0, W), A(t["root"]))
    s.ptr(s.at(0, 2 * W), A(t["begin"]))
    for i, nd in case["nodes"].items():
        a = s.at(i)
        s.ptr(a, A(nd["l"]))
        s.ptr(a + W, A(nd["r"]))
        s.ptr(a + 2 * W, A(nd["p"]) | nd["c"])
        s.ptr(a + 3 * W, s.at(20 + i))
        ra = s.at(20 + i)
        s.u32(ra, nd["len"])
        s.u32(ra + 4, nd["cap"])
        s.u32(ra + 8, nd["refs"])
        s.ptr(ra + s.DATA, s.at(30 + i))
        s.write(s.at(30 + i), bytes([i, 2 * i, 3 * i, 4 * i, 5, 6, 7, 8]))
    k = case["key"]
    s.ptr(s.at(KEY_OBJ), s.at(KEY_REP))
    s.u32(s.at(KEY_REP), k["len"])
    s.u32(s.at(KEY_REP, 4), k["cap"])
    s.u32(s.at(KEY_REP, 8), k["refs"])
    s.ptr(s.at(KEY_REP, s.DATA), s.at(KEY_DATA))
    s.write(s.at(KEY_DATA), bytes([9, 8, 7, 6, 5, 4, 3, 2]))
    return A


def execute(s, case, A):
    tree = s.R
    if case["op"] == "insert":
        return s.call(FN_INSERT, [tree, A(case["parent"]), case["left"], case["leftmost"], s.at(KEY_OBJ)])
    if case["op"] == "erase":
        return s.call(FN_ERASE, [tree, A(case["pos"])])
    return s.call(FN_DESTROY, [tree, A(case["pos"])])


def node_words(s, slot):
    W = s.W
    a = s.at(slot)
    par = s.rdp(a + 2 * W)
    return [s.tag(s.rdp(a)), s.tag(s.rdp(a + W)), s.tag(par & ~1), par & 1, s.tag(s.rdp(a + 3 * W))]


def rep_words(s, slot):
    a = s.at(slot)
    return [s.rd32(a), s.rd32(a + 4), s.rd32(a + 8), s.tag(s.rdp(a + s.DATA))]


def observe(s, case, answer):
    W = s.W
    out = {"answer": s.tag(answer), "log": s.log()}
    out["tree"] = [s.rd32(s.at(0)), s.tag(s.rdp(s.at(0, W))), s.tag(s.rdp(s.at(0, 2 * W)))]
    for i in NODE_SLOTS + POOL_SLOTS:
        out["n%d" % i] = node_words(s, i)
    for i in NODE_SLOTS:
        out["r%d" % i] = rep_words(s, i + 20)
        out["d%d" % i] = s.read(s.at(30 + i), 8)
    out["s44"] = s.read(s.at(44), 28)                  # the copied text: the same bytes on both sides
    for i in SMALL_SLOTS[1:]:
        out["s%d" % i] = [s.rd32(s.at(i)), s.read(s.at(i, 4), 24)]    # the shared count (a word) and the untouched fill
    out["key"] = [s.tag(s.rdp(s.at(KEY_OBJ)))] + rep_words(s, KEY_REP)
    return out


def run(s, case):
    try:
        A = build(s, case)
        return observe(s, case, execute(s, case, A))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def gen_nodes(r, n):
    nodes = {}
    if n == 0:
        return nodes
    nodes[1] = dict(l=0, r=0, p=-1)
    free = [(1, "l"), (1, "r")]
    for i in range(2, n + 1):
        j, side = free.pop(r.randrange(len(free)))
        nodes[i] = dict(l=0, r=0, p=j)
        nodes[j][side] = i
        free += [(i, "l"), (i, "r")]
    for nd in nodes.values():
        nd["c"] = r.randrange(2)
        nd["refs"] = r.choice([0, 1, 1, 2, 3])
        nd["len"] = r.randrange(0, 9)
        nd["cap"] = r.randrange(0, 20)
    return nodes


def leftmost(nodes):
    i = 1
    while nodes[i]["l"]:
        i = nodes[i]["l"]
    return i


def base(r, op, n):
    nodes = gen_nodes(r, n)
    return {"op": op, "fill": r.randrange(256), "nodes": nodes,
            "tree": {"count": n, "root": 1 if n else 0, "begin": leftmost(nodes) if n else -1},
            "key": {"len": r.randrange(9), "cap": r.randrange(20), "refs": r.choice([0, 1, 2])}}


def insert_case(r, n=None, count=None, left=None, lm=None, parent=None):
    n = r.randrange(0, 6) if n is None else n
    c = base(r, "insert", n)
    if count is not None:
        c["tree"]["count"] = count
    elif r.random() < 0.1:
        c["tree"]["count"] = r.choice([0xFFFFFFFE, 0xFFFFFFFF, 0, r.randrange(1 << 32)])
    c["parent"] = parent if parent is not None else (-1 if n == 0 or r.random() < 0.2 else r.randrange(1, n + 1))
    c["left"] = r.randrange(2) if left is None else left
    c["leftmost"] = r.randrange(2) if lm is None else lm
    return c


def insert_edges():
    r = random.Random(5993)
    for count in (None, 0xFFFFFFFE, 0xFFFFFFFF, 0x7FFFFFFF):
        for left in (0, 1):
            for lm in (0, 1):
                for n, parent in ((0, -1), (3, -1), (3, 2)):
                    yield insert_case(r, n, count, left, lm, parent)


def insert_random(r):
    return insert_case(r)


def erase_case(r, n=None, pos=None, mode=None):
    n = r.randrange(1, 9) if n is None else n
    c = base(r, "erase", n)
    c["pos"] = r.randrange(1, n + 1) if pos is None else pos
    mode = r.choice(["left", "left", "pos", "pos", "random"]) if mode is None else mode
    if mode == "pos":
        c["tree"]["begin"] = c["pos"]
    elif mode == "random":
        c["tree"]["begin"] = r.randrange(1, n + 1)
    return c


def erase_edges():
    r = random.Random(5994)
    for n in range(1, 7):
        for pos in range(1, n + 1):
            for mode in ("left", "pos", "random"):
                yield erase_case(r, n, pos, mode)


def erase_random(r):
    return erase_case(r)


def destroy_case(r, n=None, pos=None):
    n = r.randrange(1, 9) if n is None else n
    c = base(r, "destroy", n)
    c["pos"] = 1 if pos is None and r.random() < 0.7 else (r.randrange(1, n + 1) if pos is None else pos)
    return c


def destroy_edges():
    r = random.Random(5995)
    for n in range(1, 9):
        for pos in (1, n):
            yield destroy_case(r, n, pos)
            yield destroy_case(r, n, pos)


def destroy_random(r):
    return destroy_case(r)


TESTS = [
    FnTest(NAME_INSERT, ppc, host, edge=insert_edges, rnd=insert_random),
    FnTest(NAME_ERASE, ppc, host, edge=erase_edges, rnd=erase_random),
    FnTest(NAME_DESTROY, ppc, host, edge=destroy_edges, rnd=destroy_random),
]
