"""Unit v51059: CParser's set helpers, the std::set<wchar_t> tree functions, the online set/map destructors and a few small functions,
against libtpw_unit_CParser_IgnoreRemainingString.dylib (tests/unit_CParser_IgnoreRemainingString.cpp), main app
("SimTheme Park"), src/COnlineParser_..._CParser_IgnoreRemainingString.cpp:
  CParser::IgnoreRemainingString 0x10211eb0, AddWhitespace 0x102117fc, AddSeparator 0x10211798, SetupSeparatorSet 0x102112b0,
  std::__tree<wchar_t>: find 0x10212060, destroy 0x10212604, insert_one 0x10212960, __tree 0x10212a88, allocator deallocate 0x10212854,
  allocator<wchar_t>::destroy 0x10212928, std::set<wchar_t>::~set 0x10211110, COnlinePerson::FinalDestroy 0x10212f58, ReceiveMessage
  0x10212fb0, ProcessIdle 0x102133e4, std::list<CToken>::list 0x10210dd4, std::__destroy<Argument*> 0x10210944, the three online set/map
  destructors 0x10213fbc 0x10214148 0x1021425c and IPSystem::~IPSystem 0x10214348.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4-byte pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers for the trees and objects the host
declares natively) for the translation. The trees are built node by node from a list of values (a plain binary search tree in insertion order).
The callees outside the unit are fakes on both sides that record the same events: insert_node_at (tree, parent's value or end, left flag,
leftmost flag, key; answers a fixed fake node), operator delete (label of the block), the three online tree destroyers (tree, root),
Argument::~Argument (element, flag) and CPeep::FinalDestroy. Everything inside the unit runs as the original.

What is compared: the answer, the event log, every tree (count, root, first node and each node's value and links as labels), the parser's
position word, the list and vtable words and the unwritten bytes around them.

No fused operations. Not tested: a NULL tree, parser or root the original dereferences (find, insert_one, destroy and the parser
functions read them unconditionally); destroy of an empty node.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CParser_IgnoreRemainingString"
MASK = 0xFFFFFFFF

IGN, ADDWS, ADDSEP, SETUPSEP = 0x10211EB0, 0x102117FC, 0x10211798, 0x102112B0
FIND, DESTROY, DEALLOC, ADESTROY = 0x10212060, 0x10212604, 0x10212854, 0x10212928
INSERT, TREECTOR, WSETDTOR = 0x10212960, 0x10212A88, 0x10211110
FINALDESTROY, LISTCTOR, RECV, IDLE = 0x10212F58, 0x10210DD4, 0x10212FB0, 0x102133E4
DESTROYARG, WSSETDTOR, MAPDTOR, NOCASEDTOR, IPDTOR = 0x10210944, 0x10213FBC, 0x10214148, 0x1021425C, 0x10214348
SIZES = {IGN: 20, ADDWS: 56, ADDSEP: 56, SETUPSEP: 100, FIND: 116, DESTROY: 448, DEALLOC: 40, ADESTROY: 4, INSERT: 208, TREECTOR: 44,
         WSETDTOR: 116, FINALDESTROY: 36, LISTCTOR: 44, RECV: 4, IDLE: 4, DESTROYARG: 80, WSSETDTOR: 116, MAPDTOR: 116,
         NOCASEDTOR: 116, IPDTOR: 88}
OWN = set(SIZES)

INSERT_NODE_AT, CPEEP_FINALDESTROY, ARGUMENT_DTOR, OPERATOR_DELETE = 0x10212B2C, 0x101130AC, 0x1020127C, 0x10238218
TREE_WSTRING_DESTROY, TREE_BUDDY_DESTROY, TREE_NOCASE_DESTROY = 0x10223204, 0x10222564, 0x10223E64

ADDR = dict(inserted=0x102B1B54, notinserted=0x102B1B55, countinit=0x102B1B50, listinit=0x102B1B38,
            ipbase_vt=0x102B1F84, ipsys_vt=0x102B1940)
HOST_LAYOUT = [8, 0x10, 0x28, 0x40, 0x48, 24, 8, 16, 0, 8, 16, 24, 8, 8, 16, 12]
FAKE_NODE = 0xABCDE0

# op -> (name of the function in the brief, PowerPC entry, number of host arguments)
OPS = {
    "ignore": ("CParser::IgnoreRemainingString", IGN, 1),
    "addws": ("CParser::AddWhitespace", ADDWS, 2),
    "addsep": ("CParser::AddSeparator", ADDSEP, 2),
    "setupsep": ("CParser::SetupSeparatorSet", SETUPSEP, 1),
    "find": ("std::__tree<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::find<wchar_t>", FIND, 2),
    "destroy": ("std::__tree<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::destroy", DESTROY, 2),
    "dealloc": ("std::allocator<std::__tree<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::node>::deallocate", DEALLOC, 3),
    "adestroy": ("std::allocator<wchar_t>::destroy", ADESTROY, 2),
    "insert_one": ("std::__tree<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::insert_one", INSERT, 3),
    "tree_ctor": ("std::__tree<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::__tree", TREECTOR, 3),
    "wset_dtor": ("std::set<wchar_t,std::less<wchar_t>,std::allocator<wchar_t>>::~set", WSETDTOR, 2),
    "final_destroy": ("COnlinePerson::FinalDestroy", FINALDESTROY, 1),
    "list_ctor": ("std::list<CToken,std::allocator<CToken>>::list", LISTCTOR, 1),
    "recv": ("COnlinePerson::ReceiveMessage", RECV, 2),
    "idle": ("COnlinePerson::ProcessIdle", IDLE, 1),
    "destroy_arg": ("std::__destroy<Argument*>", DESTROYARG, 2),
    "wsset_dtor": ("std::set<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>,std::less<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>,std::allocator<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>>::~set", WSSETDTOR, 2),
    "map_dtor": ("std::map<nocase_string,BuddyInfo,std::less<nocase_string>,std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::~map", MAPDTOR, 2),
    "nocase_dtor": ("std::set<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>::~set", NOCASEDTOR, 2),
    "ip_dtor": ("IPSystem::~IPSystem", IPDTOR, 2),
}
KIND = {}
for _op in ("find", "destroy", "dealloc", "adestroy", "insert_one", "tree_ctor", "wset_dtor"):
    KIND[_op] = "tree"
for _op in ("ignore", "addws", "addsep", "setupsep"):
    KIND[_op] = "parser"
for _op in ("wsset_dtor", "map_dtor", "nocase_dtor"):
    KIND[_op] = "w32"
for _op in ("final_destroy", "recv", "idle", "list_ctor", "ip_dtor", "destroy_arg"):
    KIND[_op] = _op


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_insert_node_at(e):
    """insert_node_at(tree, parent, bool left, bool leftmost, const wchar_t& key): answers a fixed fake node."""
    L = e.cl
    tree, parent = e.r(3), e.r(4)
    pv = 0x10000 if parent == tree + 4 else struct.unpack(">H", e.read(parent + 12, 2))[0]
    key = struct.unpack(">H", e.read(e.r(7), 2))[0]
    L.rec("insert_node_at", (L.lab(tree), pv, e.r(5) & 0xFF, e.r(6) & 0xFF, key))
    e.set_r(3, FAKE_NODE)


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def make_destroyer(name):
    def stub(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
    return stub


def stub_argument_dtor(e):
    e.cl.rec("Argument::Destruct", (e.cl.lab(e.r(3)), T.s16(e.r(4))))


def stub_finaldestroy(e):
    e.cl.rec("CPeep::FinalDestroy", (e.cl.lab(e.r(3)),))


STUBS.internal(INSERT_NODE_AT, stub_insert_node_at)
STUBS.internal(OPERATOR_DELETE, stub_delete)
STUBS.internal(TREE_WSTRING_DESTROY, make_destroyer("TreeWStringDestroy"))
STUBS.internal(TREE_BUDDY_DESTROY, make_destroyer("TreeBuddyDestroy"))
STUBS.internal(TREE_NOCASE_DESTROY, make_destroyer("TreeNocaseDestroy"))
STUBS.internal(ARGUMENT_DTOR, stub_argument_dtor)
STUBS.internal(CPEEP_FINALDESTROY, stub_finaldestroy)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory (E: the byte order, P: the native pointer size)."""

    def pf(self, P):
        return self.E + ("I" if P == 4 else "Q")

    def wptr(self, a, v, P):
        self.write(a, struct.pack(self.pf(P), v & (MASK if P == 4 else (1 << 64) - 1)))

    def rptr(self, a, P):
        return struct.unpack(self.pf(P), self.read(a, P))[0]

    def w32(self, a, v):
        self.write(a, struct.pack(self.E + "I", v & MASK))

    def r32(self, a):
        return struct.unpack(self.E + "I", self.read(a, 4))[0]

    def w16(self, a, v):
        self.write(a, struct.pack(self.E + "H", v & 0xFFFF))

    def r16(self, a):
        return struct.unpack(self.E + "H", self.read(a, 2))[0]

    def g(self, name):
        return self.gaddr(name)


class Ppc(Side):
    E = ">"
    P = 4
    ppc = True
    OFF = dict(str=4, setA=8, setB=0x14, pos=0x24)

    def __init__(self, e):
        self.e = e
        self.saved = None

    def begin(self, W, case):
        self.L = self.e.cl = CL.PpcLog({a: lb for lb, a in W["lab"].items()}, case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def gaddr(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, OPS[op][1], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    E = "<"
    P = 8
    ppc = False
    OFF = dict(str=8, setA=0x10, setB=0x28, pos=0x48)

    def __init__(self, lib):
        self.lib = lib
        bind(lib)

    def begin(self, W, case):
        CL.host_begin(self.lib, dict(W["lab"]), case["queues"])

    def done(self):
        pass

    def gaddr(self, name):
        return self.lib.tpw_v51059_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v51059_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        return getattr(self.lib, "tpw_v51059_" + op)(*[C.c_uint64(a & 0xFFFFFFFFFFFFFFFF) for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51059_addr.restype, lib.tpw_v51059_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51059_alloc.restype, lib.tpw_v51059_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51059_layout.argtypes = [C.POINTER(C.c_int64)]
    for op, (_, _, n) in OPS.items():
        f = getattr(lib, "tpw_v51059_" + op)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * n
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51059_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_tree(s, at, vals, P, base, W):
    """A tree {count 0, root P, first node 2P} at `at` whose nodes {left 0, right P, parent 2P, value 3P} come from a plain binary search
    tree built by inserting vals in order. Labels: base = the tree, base+1 = its end node (at+P), base+2+i = the i-th inserted node."""
    nodes, left, right, parent = {}, {}, {}, {}
    root = None
    for i, v in enumerate(vals):
        a = s.alloc(4 * P)
        nodes[v] = a
        W["lab"][base + 2 + i] = a
        s.w16(a + 3 * P, v)
        if root is None:
            root = v
            continue
        c = root
        while True:
            side = left if v < c else right
            if c in side:
                c = side[c]
                continue
            side[c] = v
            parent[v] = c
            break
    end = at + P
    for v, a in nodes.items():
        s.wptr(a, nodes[left[v]] if v in left else 0, P)
        s.wptr(a + P, nodes[right[v]] if v in right else 0, P)
        s.wptr(a + 2 * P, nodes[parent[v]] if v in parent else end, P)
    s.w32(at, len(vals))
    s.wptr(at + P, nodes[root] if vals else 0, P)
    s.wptr(at + 2 * P, nodes[min(vals)] if vals else end, P)
    W["lab"][base] = at
    W["lab"][base + 1] = end
    W["trees"].append((at, P, [base + 2 + i for i in range(len(vals))]))


def build(s, case):
    P = s.P
    W = {"lab": {}, "trees": [], "kind": KIND[case["op"]]}
    s.write(s.g("inserted"), bytes([case["ins"]]))
    s.write(s.g("notinserted"), bytes([case["notins"]]))
    s.w32(s.g("countinit"), case["countinit"])
    s.w32(s.g("listinit"), case["listinit"])
    kind = W["kind"]
    if kind == "tree":
        T0 = s.alloc(64)
        if case["op"] == "tree_ctor":
            s.write(T0, bytes([0xA5]) * 32)
            W["lab"][1], W["lab"][2] = T0, T0 + P
            W["less"] = s.alloc(8)
            s.write(W["less"], bytes([case["lessbyte"]]) * 8)
        else:
            build_tree(s, T0, case["vals"], P, 1, W)
        W["tree"] = T0
        W["key"] = s.alloc(8)
        s.w16(W["key"], case["key"])
        W["R"] = s.alloc(32)
        s.write(W["R"], bytes([0x5A]) * 32)
        W["lab"][90] = W["R"]
    elif kind == "parser":
        obj = s.alloc(0x60)
        W["obj"] = obj
        W["lab"][60] = obj
        rep = s.alloc(16)
        W["lab"][61] = rep
        s.w32(rep, case["rep0"])
        s.w32(rep + 4, case["rep0"] ^ 0x55AA55AA)
        s.wptr(obj + s.OFF["str"], rep, P)
        s.w32(obj + s.OFF["pos"], case["pos"])
        build_tree(s, obj + s.OFF["setA"], case["vals"], P, 1, W)
        build_tree(s, obj + s.OFF["setB"], case["vals2"], P, 20, W)
    elif kind == "w32":
        tree = s.alloc(16)
        W["tree"] = tree
        W["lab"][1] = tree
        if case["has_root"]:
            node = s.alloc(16)
            W["lab"][5] = node
            s.wptr(tree + 4, node, 4)
            s.wptr(tree + 8, node, 4)
        s.w32(tree, case["countinit"])
        s.write(tree + 12, bytes([0x77]) * 4)
    elif kind == "list":
        pass
    elif kind == "list_ctor":
        blk = s.alloc(32)
        W["tree"] = blk
        W["lab"][1], W["lab"][2] = blk, blk + P
        s.write(blk, bytes([0xC3]) * 32)
    elif kind == "ip_dtor":
        blk = s.alloc(32)
        W["tree"] = blk
        W["lab"][1] = blk
        s.write(blk, bytes(case["fill"]))
    elif kind in ("final_destroy", "recv", "idle"):
        blk = s.alloc(32)
        W["tree"] = blk
        W["lab"][1] = blk
        msg = s.alloc(32)
        W["msg"] = msg
        W["lab"][2] = msg
        s.write(blk, bytes(case["fill"]))
    elif kind == "destroy_arg":
        arr = s.alloc(12 * 8 + 16)
        W["arr"] = arr
        for i in range(8):
            W["lab"][20 + i] = arr + 12 * i
    W["rev"] = {a: lb for lb, a in W["lab"].items()}
    W["rev"].update({a & MASK: lb for lb, a in W["lab"].items()})      # 32-bit words the host keeps of its addresses
    return W


def norm(W, p):
    if p == 0:
        return None
    if p == FAKE_NODE:
        return "new"
    return W["rev"].get(p, ("raw",))


def obs_tree(s, W, at, P, labels):
    out = [s.r32(at), norm(W, s.rptr(at + P, P)), norm(W, s.rptr(at + 2 * P, P))]
    for lb in labels:
        a = W["lab"][lb]
        out.append((lb, s.r16(a + 3 * P), norm(W, s.rptr(a, P)), norm(W, s.rptr(a + P, P)), norm(W, s.rptr(a + 2 * P, P))))
    return out


VOID = {"addws", "addsep", "setupsep", "destroy", "dealloc", "adestroy", "final_destroy", "idle", "recv", "list_ctor", "destroy_arg"}


def execute(s, W, case):
    answer = execute_op(s, W, case)
    return None if case["op"] in VOID else answer


def execute_op(s, W, case):
    op = case["op"]
    P = s.P
    kind = W["kind"]
    if op == "ignore":
        return s.call(op, [W["obj"]]) & 0xFF
    if op in ("addws", "addsep"):
        return s.call(op, [W["obj"], case["c"]])
    if op == "setupsep":
        return s.call(op, [W["obj"]])
    if op == "find":
        if s.ppc:
            s.call(op, [W["R"], W["tree"], W["key"]])
            return norm(W, s.rptr(W["R"], P))
        return norm(W, s.call(op, [W["tree"], W["key"]]))
    if op == "destroy":
        return s.call(op, [W["tree"], W["lab"][2 + case["sub"]]])
    if op == "dealloc":
        return s.call(op, [W["tree"] + P, W["lab"][2 + case["sub"]], case["count"]])
    if op == "adestroy":
        return s.call(op, [W["tree"], W["lab"][2 + case["sub"]] + 3 * P])
    if op == "insert_one":
        s.call(op, [W["R"], W["tree"], W["key"]])
        return (norm(W, s.rptr(W["R"], P)), s.read(W["R"] + P, 1)[0])
    if op == "tree_ctor":
        s.call(op, [W["tree"], W["less"], W["less"] + 4])
        return None
    if op == "wset_dtor":
        r = s.call(op, [0 if case["null"] else W["tree"], case["flag"]])
        return norm(W, r)
    if kind == "w32":
        s.call(op, [0 if case["null"] else W["tree"], case["flag"]])
        return None
    if op == "final_destroy" or op == "idle":
        return s.call(op, [W["tree"]])
    if op == "recv":
        return s.call(op, [W["tree"], W["msg"]])
    if op == "list_ctor":
        return s.call(op, [W["tree"]])
    if op == "ip_dtor":
        return norm(W, s.call(op, [0 if case["null"] else W["tree"], case["flag"]]))
    if op == "destroy_arg":
        return s.call(op, [W["arr"] + 12 * case["a"], W["arr"] + 12 * case["b"] + case["extra"]])
    raise AssertionError(op)


def observe(s, W, case, answer):
    out = {"answer": answer, "log": s.log()}
    P = s.P
    out["trees"] = [obs_tree(s, W, at, p, labels) for at, p, labels in W["trees"]]
    kind = W["kind"]
    if kind == "tree":
        out["R"] = s.read(W["R"] + P, 1)[0] if case["op"] != "insert_one" else None
        out["key"] = s.r16(W["key"])
        if case["op"] == "tree_ctor":
            t = W["tree"]
            out["ctor"] = (s.r32(t), norm(W, s.rptr(t + P, P)), norm(W, s.rptr(t + 2 * P, P)), s.read(t + 24, 8))
    elif kind == "parser":
        out["pos"] = s.r32(W["obj"] + s.OFF["pos"])
        out["str"] = norm(W, s.rptr(W["obj"] + s.OFF["str"], P))
        out["rep"] = [s.r32(W["lab"][61] + 4 * i) for i in range(4)]
    elif kind == "w32":
        t = W["tree"]
        out["w32"] = (s.r32(t), norm(W, s.rptr(t + 4, 4)), norm(W, s.rptr(t + 8, 4)), s.read(t + 12, 4))
    elif kind == "list_ctor":
        t = W["tree"]
        out["list"] = (s.r32(t), norm(W, s.rptr(t + P, P)), norm(W, s.rptr(t + 2 * P, P)), s.read(t + 24, 8))
    elif kind == "ip_dtor":
        t = W["tree"]
        vt = s.rptr(t, P)
        names = {s.g("ipbase_vt"): "base", s.g("ipsys_vt"): "sys"}
        out["ip"] = (None if case["null"] else names.get(vt, vt), s.read(t + 8, 24))
    elif kind in ("final_destroy", "recv", "idle"):
        out["obj"] = s.read(W["tree"], 32)
    return out


def run(s, case):
    try:
        W = build(s, case)
        s.begin(W, case)
        return observe(s, W, case, execute(s, W, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e), case)


def host(lib, case):
    return run(Host(lib), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SHAPES = [
    [], [5], [5, 3], [5, 7], [1, 2, 3, 4, 5, 6], [6, 5, 4, 3, 2, 1], [4, 2, 6, 1, 3, 5, 7], [0xFFFF, 0, 0x8000, 0x7FFF],
    [8, 4, 12, 2, 6, 10, 14, 1, 3, 5, 7, 9, 11, 13], [3, 1, 2], [3, 5, 4], [0x8000], [0xFFFF], [0],
]
FLAGS = [0, 1, 2, -1, -2, 0x7FFF, -0x8000, 0x100, 0x10000, -0x10000]
POOL = list(range(0, 12)) + [0x7FFF, 0x8000, 0xFFFF, 0x100, 0x1FF, 0x8001]


def rvals(r, lo=0, hi=12):
    n = r.randrange(lo, hi + 1)
    pool = sorted(set(POOL + [r.randrange(65536) for _ in range(6)]))
    return r.sample(pool, n)


def keys_for(vals):
    ks = set(vals) | {0, 0xFFFF, 0x8000, 0x7FFF, 1}
    if vals:
        ks |= {min(vals) - 1 if min(vals) > 0 else 0, max(vals) + 1 if max(vals) < 0xFFFF else 0xFFFF, (min(vals) + max(vals)) // 2}
    return sorted(ks)


def base(r, op, vals=None):
    vals = rvals(r, 1 if op in ("destroy", "dealloc", "adestroy") else 0) if vals is None else vals
    return {"op": op, "queues": {}, "vals": vals, "vals2": rvals(r), "key": r.choice(vals + [r.randrange(65536)] if vals else [r.randrange(65536)]),
            "c": r.choice([r.randrange(65536), r.choice(POOL)]) | r.choice([0, r.randrange(1, 65536) << 16]),
            "flag": (r.choice(FLAGS + [r.randrange(-40000, 40000)])) & MASK | (0 if r.random() < 0.7 else (r.randrange(65536) << 16)),
            "ins": r.choice([0, 1, 1, 7]), "notins": r.choice([0, 0, 1, 9]), "countinit": r.choice([0, r.randrange(1 << 32)]),
            "listinit": r.choice([0, r.randrange(1 << 32)]), "null": r.random() < 0.15, "has_root": r.random() < 0.7,
            "rep0": r.randrange(1 << 32), "pos": r.randrange(1 << 32), "lessbyte": r.randrange(256),
            "fill": [r.randrange(256) for _ in range(32)], "sub": 0, "count": r.choice([0, 1, 5, r.randrange(1 << 32)]),
            "a": 0, "b": 0, "extra": 0}


def fix(c, r):
    n = len(c["vals"])
    c["sub"] = r.randrange(n) if n else 0
    c["flag"] = ((c["flag"] & 0xFFFF) | (c["flag"] & 0xFFFF0000)) & MASK
    return c


def make(r, op):
    c = base(r, op)
    if op in ("destroy_arg",):
        c["a"], c["b"], c["extra"] = r.randrange(0, 7), r.randrange(0, 7), r.choice([0, 0, 1, 5, 11])
    return fix(c, r)


def edges(op):
    r = random.Random(1000 + list(OPS).index(op))
    if op == "destroy_arg":
        for a in range(0, 4):
            for b in range(0, 7):
                for extra in (0, 1, 11):
                    c = base(r, op)
                    c["a"], c["b"], c["extra"] = a, b, extra
                    yield fix(c, r)
        return
    for shape in SHAPES:
        if not shape and op in ("destroy", "dealloc", "adestroy"):
            continue
        if op in ("find", "insert_one"):
            for k in keys_for(shape):
                c = base(r, op, list(shape))
                c["key"] = k
                yield fix(c, r)
        elif op in ("addws", "addsep"):
            for k in (0, 0x20, 0xA, 5, 7, 0xFFFF, 0x8000):
                c = base(r, op, list(shape))
                c["c"] = k
                c["vals2"] = list(reversed(shape))
                yield fix(c, r)
        elif op in ("destroy", "dealloc", "adestroy"):
            for sub in range(len(shape)):
                c = base(r, op, list(shape))
                c["sub"] = sub
                yield c
        else:
            for flag in FLAGS:
                c = base(r, op, list(shape))
                c["flag"] = flag & MASK
                c["null"] = False
                yield fix(c, r)
            c = base(r, op, list(shape))
            c["null"] = True
            yield fix(c, r)
            c = base(r, op, list(shape))
            c["has_root"] = not c["has_root"]
            c["null"] = False
            yield fix(c, r)


def tests():
    out = []
    for op, (name, _, _) in OPS.items():
        out.append(FnTest(name, ppc, host, edge=(lambda op=op: edges(op)), rnd=(lambda r, op=op: make(r, op))))
    return out


TESTS = tests()
