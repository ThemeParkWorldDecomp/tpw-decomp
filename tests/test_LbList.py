"""Differential tests for bullfrog LbList.cpp (the Amiga-style intrusive list). See
difftest.py and tpwtest.py.

Each case is a list header and up to six nodes whose pointer fields are labels ("list",
"tail" = the tail sentinel &mTail, "n0".., None, or "junk" = an address that is only ever
compared). Both sides build the same graph in their own layout (4- or 8-byte pointers),
call the function and read every field back as labels.
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

NODES = 6
VP = ctypes.c_void_p
JUNK_PPC, JUNK_HOST = 0x7C0F1230, 0x1230


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


class World:
    """Addresses of the labelled objects on one side; ptr is 4 (PowerPC) or 8 (host)."""

    def __init__(self, ptr, list_addr, node_addrs, junk):
        self.ptr = ptr
        self.addr = {"list": list_addr, "tail": list_addr + ptr, "junk": junk, None: 0}
        for i, a in enumerate(node_addrs):
            self.addr["n%d" % i] = a
        self.label_of = {v: k for k, v in self.addr.items()}

    def label(self, a):
        return self.label_of.get(a, "?%x" % a)


def ppc_world(e, c):
    lst = e.alloc(12)
    nodes = [e.alloc(8) for _ in range(NODES)]
    w = World(4, lst, nodes, JUNK_PPC)
    e.write(lst, struct.pack(">3I", *[w.addr[x] for x in c["list"]]))
    for i, n in enumerate(c["nodes"]):
        e.write(nodes[i], struct.pack(">2I", *[w.addr[x] for x in n]))
    return w


def ppc_read(e, w):
    lst = w.addr["list"]
    fields = {"list": [w.label(e.u32(lst + 4 * i)) for i in range(3)]}
    for i in range(NODES):
        a = w.addr["n%d" % i]
        fields["n%d" % i] = [w.label(e.u32(a)), w.label(e.u32(a + 4))]
    return fields


class HostWorld(World):
    def __init__(self, c):
        self.lst = (VP * 3)()
        self.nodes = [(VP * 2)() for _ in range(NODES)]
        super().__init__(8, ctypes.addressof(self.lst), [ctypes.addressof(n) for n in self.nodes], JUNK_HOST)
        for i, x in enumerate(c["list"]):
            self.lst[i] = self.addr[x]
        for i, n in enumerate(c["nodes"]):
            for j, x in enumerate(n):
                self.nodes[i][j] = self.addr[x]

    def read(self):
        fields = {"list": [self.label(self.lst[i] or 0) for i in range(3)]}
        for i in range(NODES):
            fields["n%d" % i] = [self.label(self.nodes[i][j] or 0) for j in range(2)]
        return fields

# ----------------------------------------------------------------- cases


LABELS = ["list", "tail", "junk", None] + ["n%d" % i for i in range(NODES)]


def garbage(r):
    return r.choice(LABELS)


def valid(r, k=None):
    """A well-formed list of k nodes in random order; the other nodes hold garbage."""
    k = r.randint(0, NODES) if k is None else k
    order = r.sample(["n%d" % i for i in range(NODES)], k)
    nodes = [[garbage(r), garbage(r)] for _ in range(NODES)]
    for i, n in enumerate(order):
        nodes[int(n[1:])] = [order[i + 1] if i + 1 < k else "tail", order[i - 1] if i else "list"]
    lst = [order[0] if k else "tail", None, order[-1] if k else "list"]
    return dict(list=lst, nodes=nodes, order=order)


def init_case(r):
    return dict(list=[garbage(r) for _ in range(3)], nodes=[[garbage(r), garbage(r)] for _ in range(NODES)])


def insert_case(r):
    c = valid(r)
    c["after"] = r.choice(["list"] + c["order"])
    c["node"] = "n%d" % r.randrange(NODES)
    return c


def exists_case(r):
    c = valid(r)
    c["node"] = r.choice(LABELS)
    return c


def end_case(r):
    c = valid(r)
    if r.random() < 0.3:
        c["list"][0], c["list"][2] = garbage(r), garbage(r)
    return c


def insert_edges():
    out = []
    for k in range(4):
        for after in ["list"] + ["n%d" % i for i in range(k)]:
            for node in ("n4", "n5", "n0"):
                c = dict(valid_k(k), after=after, node=node)
                out.append(c)
    return out


def valid_k(k):
    """valid() without randomness: nodes n0..n(k-1) in order, the rest null."""
    order = ["n%d" % i for i in range(k)]
    nodes = [[None, None] for _ in range(NODES)]
    for i, n in enumerate(order):
        nodes[i] = [order[i + 1] if i + 1 < k else "tail", order[i - 1] if i else "list"]
    return dict(list=[order[0] if k else "tail", None, order[-1] if k else "list"], nodes=nodes, order=order)

# ----------------------------------------------------------------- tests


def ppc_fn(name, args):
    def run(e, c):
        w = ppc_world(e, c)
        r3, _, _ = T.call_ppc(e, e.func(name), [T.I(w.addr[a] if isinstance(a, (str, type(None))) else a)
                                                 for a in args(c)])
        if name.startswith(("LbList_Init", "LbList_Insert")):
            return dict(fields=ppc_read(e, w))     # void
        return dict(ret=w.label(r3) if name.startswith("LbList_Get") else T.s32(r3), fields=ppc_read(e, w))
    return run


def host_fn(sym, args, ptr_ret):
    def run(lib, c):
        w = HostWorld(c)
        f = F(lib, sym, VP if ptr_ret else ctypes.c_int32, *[VP] * len(args(c)))
        r = f(*[w.addr[a] for a in args(c)])
        if sym in ("tpw_LbList_Init", "tpw_LbList_Insert"):
            return dict(fields=w.read())
        return dict(ret=w.label(r or 0) if ptr_ret else r, fields=w.read())
    return run


def make(title, name, sym, args, edge, rnd, ptr_ret=False):
    return FnTest(title, ppc_fn(name, args), host_fn(sym, args, ptr_ret), edge=edge, rnd=rnd)


TESTS = [
    make("LbList_Init", "LbList_Init__FP6TbList", "tpw_LbList_Init", lambda c: ["list"],
         lambda: [dict(list=[None] * 3, nodes=[[None, None]] * NODES)], init_case),
    make("LbList_Insert", "LbList_Insert__FP6TbListP10TbListNodeP10TbListNode", "tpw_LbList_Insert",
         lambda c: ["list", c["after"], c["node"]], insert_edges, insert_case),
    make("LbList_NodeExists", "LbList_NodeExists__FPC6TbListPC10TbListNode", "tpw_LbList_NodeExists",
         lambda c: ["list", c["node"]],
         lambda: [dict(valid_k(k), node=n) for k in range(4) for n in LABELS], exists_case),
    make("LbList_GetFirst", "LbList_GetFirst__FPC6TbList", "tpw_LbList_GetFirst", lambda c: ["list"],
         lambda: [valid_k(k) for k in range(4)], end_case, ptr_ret=True),
    make("LbList_GetLast", "LbList_GetLast__FPC6TbList", "tpw_LbList_GetLast", lambda c: ["list"],
         lambda: [valid_k(k) for k in range(4)], end_case, ptr_ret=True),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
