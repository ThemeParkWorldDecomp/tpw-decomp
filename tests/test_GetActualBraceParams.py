"""Unit v5278: the brace walk and the pylon stack update, against libtpw_unit_GetActualBraceParams.dylib
(tests/unit_GetActualBraceParams.cpp), main app ("SimTheme Park"), src/TPCoaster_loaders_GetActualBraceParams.cpp:
  GetActualBraceParams 0x1003d4cc, UpdatePylonStackStructures 0x1003d844

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, 4-byte pointers; host
memory, little-endian, 8-byte pointers and the host's field offsets from tpw_gabp_layout). GetAlignmentParamsSlow (0x1004e230) is a fake on
both sides recording the first word of its params block, the point's floats, the float argument, the int and the out pointer.

The world: GetActualBraceParams: the current-data global points at a block whose +0x18 word heads a list of nodes (next at +0x64, flags +8,
float +0x6c, record list +0x90 with count and the 0x58-byte records). UpdatePylonStackStructures: the skip global, a stack with count and
pylon pointers, each pylon with flags and a chain +0x100 -> +0x18 -> word. Compared: the log, the node flags, the pylon flags and the two
stored pointers (as which area of the target they name). Fused operations: none (fadds only). Not tested: NULL pointers the original
dereferences.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GetActualBraceParams"
MASK = 0xFFFFFFFF

FN_GET, FN_UPD = 0x1003D4CC, 0x1003D844
CALLEE = 0x1004E230
ADDR = dict(skip=0x102B68A0, data=0x102B68A4)
KEYS = ["n_flags", "n_next", "n_f", "n_list", "l_recs", "l_end", "d_head", "s_pyl", "p_f74", "p_a78", "p_a84", "p_f90", "p_ctl", "c_q",
        "q_a30", "q_a44", "r_size", "r_pt", "r_f", "dup"]
MAC = dict(zip(KEYS, [8, 0x64, 0x6C, 0x90, 4, 8, 0x18, 0x34, 0x74, 0x78, 0x84, 0x90, 0x100, 0x18, 0x30, 0x44, 0x58, 0x28, 0xC, 8]))
HOST_LAYOUT = []


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


STUBS = CL.Stubs()
STUBS.real.update({FN_GET, FN_UPD})


def stub_callee(e):
    L = e.cl
    a = e.r(4)
    L.rec("GetAlignmentParamsSlow", (e.u32(e.r(3)), canon_f(e.u32(a)), canon_f(e.u32(a + 4)), canon_f(e.u32(a + 8)),
                                     canon_f(fb(e.f(1))), T.s32(e.r(7)), e.r(8) & MASK))


STUBS.internal(CALLEE, stub_callee)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def call(self, op, arg):
        if op == "get":
            T.call_ppc(self.e, FN_GET, [])
        else:
            T.call_ppc(self.e, FN_UPD, [0, arg])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.offsets
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gabp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gabp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, arg):
        if op == "get":
            self.lib.tpw_gabp_get()
        else:
            self.lib.tpw_gabp_upd(arg)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gabp_addr.restype, lib.tpw_gabp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gabp_alloc.restype, lib.tpw_gabp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gabp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gabp_get.argtypes = []
    lib.tpw_gabp_upd.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gabp_layout(out)
    assert n == len(KEYS), n
    lib.offsets = dict(zip(KEYS, list(out)[:n]))
    for k in ("r_size", "r_pt", "r_f", "n_flags", "q_a30", "q_a44", "n_list" if False else "dup"):
        assert lib.offsets[k] == MAC[k], (k, lib.offsets[k])
    lib.bound = True


def build(s, case):
    o = s.o
    world = {}
    if case["op"] == "get":
        block = s.alloc(0x40)
        s.words(block, [case["first"]])
        nodes = [s.alloc(0x200) for _ in case["nodes"]]
        s.ptr(block + o["d_head"], nodes[0] if nodes else 0)
        s.ptr(s.g("data"), block)
        world["nodes"] = nodes
        for i, n in enumerate(case["nodes"]):
            node = nodes[i]
            lst = s.alloc(0x20)
            recs = s.alloc(o["r_size"] * max(1, len(n["recs"])) + 8)
            s.words(lst, [len(n["recs"])])
            s.ptr(lst + o["l_recs"], recs)
            s.ptr(lst + o["l_end"], recs)
            s.words(node + o["n_flags"], [n["flags"]])
            s.ptr(node + o["n_next"], nodes[i + 1] if i + 1 < len(nodes) else 0)
            s.words(node + o["n_f"], [n["f"]])
            s.ptr(node + o["n_list"], lst)
            s.ptr(node + 0x100, s.alloc(16))     # the original loads a word through it after each call (dead)
            for j, r in enumerate(n["recs"]):
                s.words(recs + o["r_size"] * j + o["r_f"], [r[0]])
                s.words(recs + o["r_size"] * j + o["r_pt"], r[1:4])
    else:
        s.words(s.g("skip"), [case["skip"]])
        stack = s.alloc(o["s_pyl"] + 8 * max(1, len(case["pylons"])) + 8)
        s.words(stack + 0x30, [case["count"]])
        pyl = []
        for i, p in enumerate(case["pylons"]):
            a = s.alloc(0x200)
            ctl = s.alloc(0x40)
            q = s.alloc(0x100)
            s.words(q, [p["q"]])
            s.ptr(ctl + o["c_q"], q)
            s.ptr(a + o["p_ctl"], ctl)
            s.words(a + 8, [p["flags"]])
            s.ptr(stack + o["s_pyl"] + s.psize * i, a)
            pyl.append((a, q))
        world["stack"] = stack
        world["pyl"] = pyl
    return world


def which(v, base, areas):
    if v == 0:
        return "none"
    for name, off in areas:
        if v == base + off:
            return name
    return "other %d" % (v - base)


def observe(s, case, w):
    o = s.o
    out = {"log": s.log()}
    if case["op"] == "get":
        out["flags"] = [s.rd_word(n + 8) for n in w["nodes"]]
    else:
        out["skip"] = s.rd_word(s.g("skip"))
        r = []
        for a, q in w["pyl"]:
            r.append((s.rd_word(a + 8), which(s.rd_ptr(a + o["p_f90"]), a, [("78", o["p_a78"]), ("84", o["p_a84"])]),
                      which(s.rd_ptr(a + o["p_f74"]), q, [("30", o["q_a30"]), ("44", o["q_a44"])])))
        out["pyl"] = r
    return out


def run(s, case):
    try:
        w = build(s, case)
        s.call(case["op"], w.get("stack", 0))
        return observe(s, case, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0x3F800000])
    return fb(r.uniform(-300, 300))


def get_case(r, nn=None):
    nn = r.randrange(0, 4) if nn is None else nn
    nodes = []
    for _ in range(nn):
        nodes.append({"flags": r.choice([0, 0x800, 0x1000, 0x1800, r.getrandbits(32), 0xFFFFFFFF]), "f": rf(r),
                      "recs": [[rf(r), rf(r), rf(r), rf(r)] for _ in range(r.choice([0, 1, 2, 3]))]})
    return {"op": "get", "queues": {}, "first": r.getrandbits(32), "nodes": nodes}


def upd_case(r, n=None, skip=None):
    n = r.randrange(0, 6) if n is None else n
    count = n if r.random() < 0.9 else r.choice([0, 1, 2])
    pyl = [{"flags": r.choice([0, 0x100, 0x2000, 0x2100, r.getrandbits(32)]),
            "q": r.choice([0, 8, 0xFFFFFFF7, r.getrandbits(32)])} for _ in range(max(n, count))]
    return {"op": "upd", "queues": {}, "skip": r.choice([0, 0, 1, 0x80000000]) if skip is None else skip, "count": count, "pylons": pyl}


def get_edges():
    r = random.Random(11)
    for nn in (0, 1, 2, 3):
        yield get_case(r, nn)


def upd_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 3, 5):
        for sk in (0, 1):
            yield upd_case(r, n, sk)
    for _ in range(12):
        yield upd_case(r, 4, 0)


TESTS = [
    FnTest("GetActualBraceParams", ppc, host, edge=get_edges, rnd=get_case),
    FnTest("UpdatePylonStackStructures", ppc, host, edge=upd_edges, rnd=upd_case),
]
