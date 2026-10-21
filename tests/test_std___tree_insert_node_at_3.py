"""Unit v5917, std::__tree<unsigned short> find / insert_one / insert_node_at, IPSystem::Startup / Shutdown and ccParkInfo::~ccParkInfo,
against libtpw_unit_std___tree_insert_node_at_3.dylib (tests/unit_std___tree_insert_node_at_3.cpp), main app
("SimTheme Park"), src/game/MslSupport.cpp, src/game/online/FS_Online.cpp:
  find 0x1016d1f8, insert_one 0x1016e9c4, insert_node_at 0x1016f164, Startup 0x1016f3c8, Shutdown 0x1016f448, ~ccParkInfo 0x1016ff48

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
32-bit words) for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads
both back (Side.P is the pointer width). The callees outside the unit are fakes on both sides that record their arguments: strlen,
operator new, new[], strcpy, __throw, the _RefCountedPtr constructor, balance_insert, free, operator delete, WSAStartup, WSACleanup and
IPBase::ShowError. The node operator new answers and the new[] block are blocks the case prepared (labels 90 and 91).

The world of a case: a tree (count, root, first node, nodes of a random binary tree with random colour bits), the key, the new node
(with junk in its parent word's colour bit), or an IPSystem block, or a ccParkInfo with up to five blocks. What is compared: the answer,
the whole tree and the new node (links as labels), the new[] block, the result block, the event log.

No fused operations. Not tested: a NULL tree or a NULL parent (the original reads them unconditionally); the throw path where the real
__throw does not return (the fakes return, and the node is made as the original would not).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___tree_insert_node_at_3"
MASK = 0xFFFFFFFF

FIND, ONE, AT = 0x1016D1F8, 0x1016E9C4, 0x1016F164
STARTUP, SHUTDOWN, DTOR = 0x1016F3C8, 0x1016F448, 0x1016FF48
SIZES = {FIND: 116, ONE: 208, AT: 340, STARTUP: 88, SHUTDOWN: 80, DTOR: 220}
OWN = set(SIZES)

BALANCE, RCCTOR, SHOWERROR = 0x10153B24, 0x100026AC, 0x1020CAC8
G_STRLEN, G_NEWARR, G_STRCPY, G_THROW = 0x10238650, 0x10238668, 0x10238410, 0x10238680
G_NEW, G_DELETE, G_FREE, G_STARTUP, G_CLEANUP = 0x102384B8, 0x10238218, 0x10238A70, 0x102394F0, 0x102386F8
LENGTH_VTBL, LENGTH_TV = 0x10278B74, 0x10265680

NEW_LABEL, ARR_LABEL, END_LABEL = 90, 91, 1000
SLOTS = [0x4, 0xC, 0x8, 0x44, 0x4C]
HOST_LAYOUT = [8, 16, 24, 8, 16, 8, 0x66]

ST = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def txt(e, a):
    t = e.cstr(a)
    return t.decode("latin1") if isinstance(t, (bytes, bytearray)) else t


def f_strlen(e):
    t = txt(e, e.r(3))
    e.cl.rec("strlen", (), (t,))
    e.set_r(3, len(t))


def f_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, ST["newnode"])


def f_newarr(e):
    e.cl.rec("newarr", (e.r(3),))
    e.set_r(3, ST["newarr"])


def f_rcctor(e):
    self, p = e.r(3), e.r(4)
    e.cl.rec("rcctor", (e.cl.lab(p),))
    e.write(self, struct.pack(">II", p, 0))


def f_strcpy(e):
    d, t = e.r(3), txt(e, e.r(4))
    e.cl.rec("strcpy", (e.cl.lab(d),), (t,))
    e.write(d, t.encode("latin1") + b"\0")


def f_throw(e):
    t, obj, dtor = txt(e, e.r(3)), e.r(4), e.r(5)
    e.cl.rec("throw", (int(e.u32(obj) == LENGTH_VTBL), int(dtor == LENGTH_TV), e.cl.lab(e.u32(obj + 4))), (t,))


def f_balance(e):
    e.cl.rec("balance", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def f_startup(e):
    e.cl.rec("WSAStartup", (e.r(3), int(e.r(4) != 0)))
    e.set_r(3, e.cl.ret("WSAStartup") & MASK)


def f_cleanup(e):
    e.cl.rec("WSACleanup", ())
    e.set_r(3, e.cl.ret("WSACleanup") & MASK)


def f_free(e):
    e.cl.rec("free", (e.cl.lab(e.r(3)),))


def f_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def f_showerror(e):
    e.cl.rec("ShowError", (e.cl.lab(e.r(3)),))


def glue(fn):
    """An import's glue saves the caller's TOC pointer at 0x14(r1); the code reloads r2 from there after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


for addr, fn in ((G_STRLEN, f_strlen), (G_NEW, f_new), (G_NEWARR, f_newarr), (G_STRCPY, f_strcpy),
                 (G_THROW, f_throw), (G_STARTUP, f_startup), (G_CLEANUP, f_cleanup), (G_FREE, f_free),
                 (G_DELETE, f_delete)):
    STUBS.internal(addr, glue(fn))
for addr, fn in ((RCCTOR, f_rcctor), (BALANCE, f_balance), (SHOWERROR, f_showerror)):
    STUBS.internal(addr, fn)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


def block_sizes(case):
    op = case["op"]
    if op in ("find", "one", "at"):
        return [0x20] + [0x20] * len(case["keys"]) + [16, 32]
    if op in ("startup", "shutdown"):
        return [0x20]
    return [0x80] + [8] * 5


class Side:
    def make_blocks(self, case):
        self.b = [self.alloc(n) for n in block_sizes(case)]
        self.newnode = self.alloc(0x20)
        self.newarr = self.alloc(0x40)
        self.names = {a: i + 1 for i, a in enumerate(self.b)}
        self.names[self.newnode] = NEW_LABEL
        self.names[self.newarr] = ARR_LABEL
        self.obs = dict(self.names)
        if case["op"] in ("find", "one", "at"):
            self.obs[self.b[0] + self.P] = END_LABEL

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.P))[0]

    def wh(self, a, v):
        self.write(a, struct.pack(self.hfmt, v & 0xFFFF))

    def rh(self, a):
        return struct.unpack(self.hfmt, self.read(a, 2))[0]

    def nm(self, v):
        return self.obs.get(v, 0 if v == 0 else -1)


class Ppc(Side):
    fmt, pfmt, hfmt, P = ">I", ">I", ">H", 4

    def __init__(self, e, case):
        self.e = e
        self.make_blocks(case)
        ST["newnode"], ST["newarr"] = self.newnode, self.newarr
        self.L = e.cl = CL.PpcLog(dict(self.names), case["queues"])
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

    def pw(self, a):
        return a

    def call(self, name, *args):
        entry = {"find": FIND, "one": ONE, "at": AT, "startup": STARTUP, "shutdown": SHUTDOWN, "dtor": DTOR}[name]
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt, pfmt, hfmt, P = "<I", "<Q", "<H", 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_blocks(case)
        lib.tpw_v5917_set_new(self.newnode, self.newarr)
        CL.host_begin(lib, {label: a for a, label in self.names.items()}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_v5917_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pw(self, a):
        return self.lib.tpw_v5917_off(a)

    def call(self, name, *args):
        lib = self.lib
        if name == "find":
            lib.tpw_v5917_find(args[1], args[2], args[0])
            return 0
        if name == "one":
            lib.tpw_v5917_insert_one(args[1], args[2], args[0])
            return 0
        if name == "at":
            return lib.tpw_v5917_insert_node_at(*args)
        if name == "startup":
            return lib.tpw_v5917_startup(*args)
        if name == "shutdown":
            return lib.tpw_v5917_shutdown(*args)
        return lib.tpw_v5917_dtor(args[0], s32(args[1]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    lib.tpw_v5917_alloc.restype, lib.tpw_v5917_alloc.argtypes = u64, [u32]
    lib.tpw_v5917_off.restype, lib.tpw_v5917_off.argtypes = u32, [u64]
    lib.tpw_v5917_set_new.restype, lib.tpw_v5917_set_new.argtypes = None, [u64, u64]
    lib.tpw_v5917_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v5917_find.restype, lib.tpw_v5917_find.argtypes = None, [u64, u64, u64]
    lib.tpw_v5917_insert_one.restype, lib.tpw_v5917_insert_one.argtypes = None, [u64, u64, u64]
    lib.tpw_v5917_insert_node_at.restype, lib.tpw_v5917_insert_node_at.argtypes = u64, [u64, u64, u32, u32, u64]
    lib.tpw_v5917_startup.restype, lib.tpw_v5917_startup.argtypes = u32, [u64]
    lib.tpw_v5917_shutdown.restype, lib.tpw_v5917_shutdown.argtypes = u32, [u64]
    lib.tpw_v5917_dtor.restype, lib.tpw_v5917_dtor.argtypes = u64, [u64, i32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v5917_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_tree(s, case):
    keys, (left, right, par, root, first) = case["keys"], case["shape"]
    P, tree = s.P, s.b[0]
    nn = len(keys)

    def ptr(i):
        return s.b[1 + i] if i >= 0 else 0

    s.words(tree, [case["count"]])
    s.wp(tree + P, ptr(root))
    s.wp(tree + 2 * P, ptr(first) if first >= 0 else tree + P)
    for i in range(nn):
        a = s.b[1 + i]
        s.wp(a, ptr(left[i]))
        s.wp(a + P, ptr(right[i]))
        s.wp(a + 2 * P, (ptr(par[i]) if par[i] >= 0 else tree + P) | case["colors"][i])
        s.wh(a + 3 * P, keys[i])
    s.wp(s.newnode + 2 * P, case["junk"])
    s.wh(s.newnode + 3 * P, case["junkval"])
    s.wh(s.b[nn + 1], case["key"])


def build(s, case):
    op = case["op"]
    if op in ("find", "one", "at"):
        build_tree(s, case)
    elif op == "dtor":
        obj = s.b[0]
        slot_words = {o // 4: j for j, o in enumerate(SLOTS)}
        for k in range(25):
            if k in slot_words:
                j = slot_words[k]
                s.words(obj + 4 * k, [s.pw(s.b[1 + j]) if case["present"][j] else 0])
            else:
                s.words(obj + 4 * k, [case["junk"][k]])


def execute(s, case):
    op = case["op"]
    nn = len(case.get("keys", ()))
    if op in ("find", "one"):
        s.call(op, s.b[nn + 2], s.b[0], s.b[nn + 1])
        return 0
    if op == "at":
        parent = s.b[0] + s.P if case["parent"] < 0 else s.b[1 + case["parent"]]
        return s.call("at", s.b[0], parent, int(case["left"]), int(case["leftmost"]), s.b[nn + 1])
    if op in ("startup", "shutdown"):
        return s.call(op, s.b[0]) & 0xFF
    this = 0 if case["null"] else s.b[0]
    return s.call("dtor", this, case["flag"])


def observe(s, case, answer):
    op = case["op"]
    out = {"answer": s.nm(answer) if op in ("at", "dtor") else answer, "log": s.log()}
    if op in ("find", "one", "at"):
        keys, nn, P, tree = case["keys"], len(case["keys"]), s.P, s.b[0]
        out["hdr"] = [s.rd_word(tree), s.nm(s.rp(tree + P)), s.nm(s.rp(tree + 2 * P))]
        nodes = []
        for a in s.b[1:1 + nn] + [s.newnode]:
            par = s.rp(a + 2 * P)
            nodes.append((s.nm(s.rp(a)), s.nm(s.rp(a + P)), s.nm(par & ~1), par & 1, s.rh(a + 3 * P)))
        out["nodes"] = nodes
        out["arr"] = s.read(s.newarr, 40)
        res = s.b[nn + 2]
        out["res"] = (s.nm(s.rp(res)), s.read(res + P, 1))
    elif op == "dtor":
        obj = s.b[0]
        slot_words = {o // 4 for o in SLOTS}
        out["words"] = [s.rd_word(obj + 4 * k) for k in range(25) if k not in slot_words]
        out["slots"] = [int(s.rd_word(obj + o) != 0) for o in SLOTS]
    return out


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

POOL = [0, 1, 2, 3, 5, 8, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF, 100, 200, 300, 0x100, 0xFF, 7, 9, 11, 13]


def bst(keys):
    n = len(keys)
    left, right, par = [-1] * n, [-1] * n, [-1] * n
    for i in range(1, n):
        cur = 0
        while True:
            side = left if keys[i] < keys[cur] else right
            if side[cur] < 0:
                side[cur] = i
                par[i] = cur
                break
            cur = side[cur]
    first = -1
    if n:
        first = 0
        while left[first] >= 0:
            first = left[first]
    return left, right, par, (0 if n else -1), first


def tree_case(r, op, nn=None):
    nn = r.randrange(0, 8) if nn is None else nn
    keys = r.sample(POOL, nn)
    c = {"op": op, "keys": keys, "shape": bst(keys), "colors": [r.randrange(2) for _ in keys], "count": nn, "queues": {},
         "junk": r.randrange(2), "junkval": r.randrange(65536), "key": r.choice(keys + POOL) if keys else r.choice(POOL)}
    return c


def probe_keys(c):
    out = [0, 0xFFFF]
    for k in c["keys"]:
        out += [k, (k - 1) & 0xFFFF, (k + 1) & 0xFFFF]
    return out


def find_edges(op):
    r = random.Random(11 if op == "find" else 12)
    for nn in (0, 1, 2, 3, 4, 7):
        c = tree_case(r, op, nn)
        for k in probe_keys(c):
            d = dict(c)
            d["key"] = k
            yield d


def find_random(op):
    return lambda r: tree_case(r, op)


def at_case(r, nn, parent, left, leftmost, count, junk):
    c = tree_case(r, "at", nn)
    c.update(parent=parent, left=left, leftmost=leftmost, count=count, junk=junk)
    return c


def at_edges():
    r = random.Random(13)
    for nn in (0, 3):
        for count in (0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD, 0, nn):
            for parent in ((-1,) if nn == 0 else (-1, 0, 2)):
                for left in (False, True):
                    for leftmost in (False, True):
                        for junk in (0, 1):
                            yield at_case(r, nn, parent, left, leftmost, count, junk)


def at_random(r):
    nn = r.randrange(0, 7)
    count = r.choice([nn, nn, 0xFFFFFFFF, 0xFFFFFFFE, r.randrange(1 << 32)])
    return at_case(r, nn, r.randrange(-1, nn), r.random() < 0.5, r.random() < 0.5, count, r.randrange(2))


def ip_case(op, answer):
    return {"op": op, "queues": {"WSAStartup" if op == "startup" else "WSACleanup": [answer]}}


def ip_edges(op):
    for a in (0, 1, -1, 10091, 0x80000000):
        yield ip_case(op, a & MASK)


def ip_random(op):
    return lambda r: ip_case(op, r.choice([0, 0, 1, -1, r.randrange(1 << 32)]) & MASK)


def dtor_case(r, present, null, flag):
    return {"op": "dtor", "queues": {}, "present": present, "null": null, "flag": flag, "junk": [r.randrange(1 << 32) for _ in range(25)]}


def dtor_edges():
    r = random.Random(14)
    for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10001, 0xFFFF8000, 0x7FFF0000):
        for j in range(6):
            present = [1] * 5 if j == 5 else [int(i == j) for i in range(5)]
            yield dtor_case(r, present, False, flag)
    yield dtor_case(r, [0] * 5, False, 1)
    yield dtor_case(r, [1] * 5, True, 1)
    yield dtor_case(r, [0] * 5, True, 0)


def dtor_random(r):
    return dtor_case(r, [r.randrange(2) for _ in range(5)], r.random() < 0.1, r.choice([0, 1, -1, r.randrange(1 << 32)]) & MASK)


PREFIX = "std::__tree<unsigned_short,std::less<unsigned_short>,std::allocator<unsigned_short>>::"
TESTS = [
    FnTest(PREFIX + "insert_node_at", ppc, host, edge=at_edges, rnd=at_random),
    FnTest(PREFIX + "insert_one", ppc, host, edge=lambda: find_edges("one"), rnd=find_random("one")),
    FnTest(PREFIX + "find<unsigned_short>", ppc, host, edge=lambda: find_edges("find"), rnd=find_random("find")),
    FnTest("IPSystem::Startup", ppc, host, edge=lambda: ip_edges("startup"), rnd=ip_random("startup")),
    FnTest("IPSystem::Shutdown", ppc, host, edge=lambda: ip_edges("shutdown"), rnd=ip_random("shutdown")),
    FnTest("ccParkInfo::~ccParkInfo", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
