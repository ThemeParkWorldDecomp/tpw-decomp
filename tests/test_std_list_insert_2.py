"""Unit v5931, std::list<unsigned int>::insert, merge, remove, splice, against libtpw_unit_std_list_insert_2.dylib
(tests/unit_std_list_insert_2.cpp), main app ("SimTheme Park"), src/std_list_insert_2.cpp:
  insert 0x102092f8, merge 0x102095a0, remove 0x10208f44, splice 0x1020a9a0

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian for the original, the
host library's arena little-endian for the translation). Lists are a count and a sentinel at +4; nodes are (prev, next, data). The
callees outside the unit (operator new, new[], delete, strlen, strcpy, __throw) are fakes on both sides: new/new[]/delete log their
size or the node's data, __throw logs hashes of the type and message, whether the vtable and destructor vector are the length_error
ones and the counter. The fake __throw returns, so the original goes on after the length error; the size fields are overridden by
cases to reach it.

What is compared: the answer (the inserted node's index), each list's count, walk of the data forward, link consistency (every prev
matches), and the event log. No fused operations. Not tested: nothing excluded (NULL positions are not valid inputs).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_insert_2"
MASK = 0xFFFFFFFF

INSERT, MERGE, REMOVE, SPLICE = 0x102092F8, 0x102095A0, 0x10208F44, 0x1020A9A0
SIZES = {INSERT: 428, MERGE: 652, REMOVE: 336, SPLICE: 432}
OWN = set(SIZES)

NEW, NEWA, DELETE, DELETEA, THROW, STRLEN, STRCPY = 0x102384B8, 0x10238668, 0x10238218, 0x10238638, 0x10238680, 0x10238650, 0x10238410
VTBL, DTORTV = 0x10278B74, 0x10265680
HOST_LAYOUT = [12, 12]


def h(text):
    v = 0
    for c in text:
        v = (v * 31 + c) & MASK
    return v


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(n))


def s_newa(e):
    n = e.r(3) & MASK
    e.cl.rec("new[]", (n,))
    e.set_r(3, e.alloc(n))


def s_delete(e):
    e.cl.rec("delete", (e.u32((e.r(3) & MASK) + 8),))


def s_deletea(e):
    e.cl.rec("delete[]", ())


def cbytes(e, a):
    out = b""
    while True:
        b = e.read(a + len(out), 1)
        if b == b"\0":
            return out
        out += b


def s_throw(e):
    t, o = e.r(3) & MASK, e.r(4) & MASK
    msgp, cntp = e.u32(o + 4), e.u32(o + 8)
    e.cl.rec("throw", (h(cbytes(e, t)), h(cbytes(e, msgp)), int(e.u32(o) == VTBL), int((e.r(5) & MASK) == DTORTV),
                       e.u32(cntp) if cntp else MASK))


def s_strlen(e):
    e.set_r(3, len(cbytes(e, e.r(3) & MASK)))


def s_strcpy(e):
    d, s = e.r(3) & MASK, e.r(4) & MASK
    e.write(d, cbytes(e, s) + b"\0")


def glue(fn):
    """The import glue saves the TOC pointer at 0x14(r1); the original reloads r2 from there after the call."""
    def wrapped(e):
        e.write((e.r(1) & MASK) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


for _addr, _fn in ((NEW, s_new), (NEWA, s_newa), (DELETE, s_delete), (DELETEA, s_deletea), (THROW, s_throw), (STRLEN, s_strlen),
                   (STRCPY, s_strcpy)):
    STUBS.internal(_addr, glue(_fn))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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
        return T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_sl_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sl_alloc(n)

    def read(self, a, n):
        return C.string_at(self.base + a, n)

    def write(self, a, data):
        C.memmove(self.base + a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        l = self.lib
        if fn == INSERT:
            l.tpw_sl_insert(*args)
        elif fn == MERGE:
            l.tpw_sl_merge(*args)
        elif fn == REMOVE:
            l.tpw_sl_remove(*args)
        else:
            l.tpw_sl_splice(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sl_alloc.restype, lib.tpw_sl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sl_base.restype = C.c_uint64
    lib.tpw_sl_layout.argtypes = [C.POINTER(C.c_int64)]
    U = C.c_uint64
    lib.tpw_sl_insert.argtypes = [U, U, C.c_uint32, U]
    lib.tpw_sl_merge.argtypes = [U, U]
    lib.tpw_sl_remove.argtypes = [U, U]
    lib.tpw_sl_splice.argtypes = [U, C.c_uint32, U]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def make_list(s, data, size=None):
    """Returns (list address, node addresses); the sentinel is list + 4."""
    lst = s.alloc(12)
    nodes = [s.alloc(12) for _ in data]
    sent = lst + 4
    chain = [sent] + nodes + [sent]
    for i, n in enumerate(nodes):
        s.words(n, [chain[i], chain[i + 2], data[i]])
    s.words(lst, [len(data) if size is None else size, chain[-2], chain[1]])
    return lst, nodes


def walk(s, lst):
    sent = lst + 4
    size, back, cur = s.rd_words(lst, 3)
    out, addrs, ok, prev = [], [], True, sent
    for _ in range(80):
        if cur == sent:
            break
        p, nx, d = s.rd_words(cur, 3)
        ok = ok and p == prev
        out.append(d)
        addrs.append(cur)
        prev, cur = cur, nx
    else:
        ok = False
    ok = ok and back == prev
    return {"size": size, "data": out, "ok": ok}, addrs


def run(s, case):
    try:
        op = case["op"]
        la, na = make_list(s, case["a"], case.get("sizea"))
        lb, nb = make_list(s, case["b"], case.get("sizeb"))
        res = s.alloc(4)
        val = s.alloc(4)
        s.words(val, [case["val"]])
        pos = (na + [la + 4])[case["pos"]] if "pos" in case else 0
        if op == "insert":
            s.call(INSERT, [res, la, pos, val])
        elif op == "remove":
            s.call(REMOVE, [la, val])
        elif op == "splice":
            s.call(SPLICE, [la, pos, lb])
        elif op == "merge":
            s.call(MERGE, [la, lb])
        else:
            s.call(MERGE, [la, la])
        wa, aa = walk(s, la)
        wb, _ = walk(s, lb)
        r = s.rd_word(res)
        return {"a": wa, "b": wb, "res": aa.index(r) if r in aa else (-1 if r == 0 else -2), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


BIG = [0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x7FFFFFFD]


def lst(r, n, lo=0, hi=5, srt=False):
    d = [r.randrange(lo, hi + 1) for _ in range(n)]
    return sorted(d) if srt else d


def mk(r, op, edge=False):
    c = {"op": op, "queues": {}, "val": r.randrange(0, 4)}
    na = r.randrange(0, 7)
    nb = r.randrange(0, 7)
    srt = op in ("merge", "merge_self") and r.random() < 0.85
    c["a"] = lst(r, na, srt=srt)
    c["b"] = lst(r, nb, srt=srt)
    if op in ("insert", "splice"):
        c["pos"] = r.randrange(0, na + 1)
    if op == "remove":
        c["a"] = lst(r, r.randrange(0, 11), 0, 3)
        c["val"] = r.choice([0, 1, 2, 3, 3, 9])
    if r.random() < 0.2 and op != "remove":
        c["sizea"] = r.choice(BIG)
        c["sizeb"] = r.choice([None, 0, 1, 2, 0x7FFFFFFF]) if op != "insert" else None
        if c["sizeb"] is None:
            del c["sizeb"]
        elif c["sizeb"] and not c["b"]:
            c["b"] = [1]     # a count with no nodes is not a list the library produces (splice would link the sentinel)
    return c


def gen(op, seed, fixed):
    def edges():
        r = random.Random(seed)
        for c in fixed(r):
            yield c
    return edges


def ins_edges(r):
    for a, pos, sz in ([[], 0, None], [[5], 0, None], [[5], 1, None], [[1, 2, 3], 2, None], [[], 0, 0x7FFFFFFE], [[], 0, 0x7FFFFFFF],
                       [[1], 1, 0xFFFFFFFF], [[1], 0, 0x80000000]):
        c = {"op": "insert", "queues": {}, "a": a, "b": [], "pos": pos, "val": 7}
        if sz is not None:
            c["sizea"] = sz
        yield c


def rem_edges(r):
    for a, v in ([[], 1], [[1], 1], [[1], 2], [[1, 1, 1], 1], [[2, 1, 1], 1], [[1, 1, 2], 1], [[1, 2, 1, 1, 2, 1], 1], [[2, 2], 1],
                 [[1, 2, 2, 1], 2]):
        yield {"op": "remove", "queues": {}, "a": a, "b": [], "val": v}


def spl_edges(r):
    for a, b, pos, sa, sb in ([[], [], 0, None, None], [[1], [], 0, None, None], [[], [4, 5], 0, None, None], [[1, 2], [4, 5], 0, None, None],
                              [[1, 2], [4, 5], 1, None, None], [[1, 2], [4, 5], 2, None, None], [[1], [4], 1, 0x7FFFFFFE, None],
                              [[1], [4], 1, 0x7FFFFFFF, None], [[1], [4], 1, 0x7FFFFFFE, 1], [[1], [4], 1, 0x7FFFFFFF, 0],
                              [[1], [4], 0, 0x7FFFFFFF, 1], [[1], [4], 0, 0x7FFFFFFF, 0xFFFFFFFF]):
        c = {"op": "splice", "queues": {}, "a": a, "b": b, "pos": pos, "val": 0}
        if sa is not None:
            c["sizea"] = sa
        if sb is not None:
            c["sizeb"] = sb
        yield c


def mrg_edges(r):
    for a, b, sa, sb in ([[], [], None, None], [[1], [], None, None], [[], [1], None, None], [[1, 3], [2], None, None], [[2], [1, 3], None, None],
                         [[1, 2], [1, 2], None, None], [[1, 2], [5, 6], None, None], [[5, 6], [1, 2], None, None], [[1, 5], [2, 3, 4, 6], None, None],
                         [[3], [1, 1, 1, 2, 4], None, None], [[3, 3], [3], None, None], [[1], [2], 0x7FFFFFFE, 1], [[1], [2], 0x7FFFFFFF, 1],
                         [[1], [2], 0x7FFFFFFE, 2], [[1], [2], 0xFFFFFFFF, 0]):
        c = {"op": "merge", "queues": {}, "a": a, "b": b, "val": 0}
        if sa is not None:
            c["sizea"] = sa
        if sb is not None:
            c["sizeb"] = sb
        yield c
    yield {"op": "merge_self", "queues": {}, "a": [1, 2], "b": [3], "val": 0}
    yield {"op": "merge_self", "queues": {}, "a": [], "b": [], "val": 0}


def mk_op(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::insert", ppc, host, edge=gen("insert", 1, ins_edges), rnd=mk_op("insert")),
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::merge", ppc, host, edge=gen("merge", 2, mrg_edges),
           rnd=lambda r: mk(r, r.choice(["merge", "merge", "merge", "merge_self"]))),
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::remove", ppc, host, edge=gen("remove", 3, rem_edges), rnd=mk_op("remove")),
    FnTest("std::list<unsigned_int,std::allocator<unsigned_int>>::splice", ppc, host, edge=gen("splice", 4, spl_edges), rnd=mk_op("splice")),
]
