"""Unit v5950, the Outbox cursor and mark functions, a string ==, the basic_ifstream<char> constructor and Email::Attachment::~Attachment,
against libtpw_unit_Outbox_First.dylib (tests/unit_Outbox_First.cpp), main app ("SimTheme Park"),
src/game/online/email.cpp:
  Outbox::First 0x10205718, Next 0x1020578c, MarkToSend 0x10205828, MarkToDelete 0x10205910, Delete 0x10206278,
  std::operator== (string) 0x1020702c, std::basic_ifstream<char>::basic_ifstream 0x1020801c, Email::Attachment::~Attachment 0x10202894

The Outbox cases build the three number lists (at +0x28, +0x34, +0x40) as MSL lists in emulator memory and as std::lists on the host (the
host wrapper takes arrays), with the cursor as a node on one side and an index on the other. The original's list functions (insert, sort,
unique, remove, erase) run as the original on the PowerPC side. ReadMailData, DeleteMail and Save are fakes on both sides (Save logs the
three list sizes). The string case fakes the compare; the ifstream case fakes the istream and filebuf constructors (the istream one writes
the virtual ios pointer from the case's queue); the attachment case fakes the string destructor and the two deletes.

Compared: the answer, the event log, the three lists, the cursor (as an index), the whole 0x94 byte stream object (vtable words by name,
the virtual ios pointer relative to the object), the attachment's three words. No fused operations. Not tested: MarkToSend or MarkToDelete
with an empty list at +0x28 (the original reads the sentinel's neighbouring word), a cursor that is not a node of the list.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Outbox_First"
MASK = 0xFFFFFFFF

FN_FIRST, FN_NEXT, FN_MTS, FN_MTD, FN_DELETE = 0x10205718, 0x1020578c, 0x10205828, 0x10205910, 0x10206278
FN_EQ, FN_IFS, FN_ATT = 0x1020702c, 0x1020801c, 0x10202894
SIZES = {FN_FIRST: 80, FN_NEXT: 120, FN_MTS: 100, FN_MTD: 100, FN_DELETE: 240, FN_EQ: 76, FN_IFS: 180, FN_ATT: 140}
OWN = set(SIZES)
OPS = [FN_FIRST, FN_NEXT, FN_MTS, FN_MTD, FN_DELETE]

CALLEE_READ, CALLEE_DELETEMAIL, CALLEE_SAVE = 0x102049fc, 0x1020638c, 0x1020461c
CALLEE_COMPARE = 0x10209cac
CALLEE_ISTREAM, CALLEE_FILEBUF = 0x1020a4c0, 0x10072bbc
CALLEE_STRDTOR, CALLEE_DELETE, CALLEE_DELARR = 0x1000221c, 0x10238218, 0x10238638

ADDR = dict(gIfstreamVtbl=0x102b1778, gOstreamVt1=0x1029d0b0, gIosBaseVt=0x103bd034)
HOST_LAYOUT = [12, 0x4c, 0x94]
OFFS = (0x28, 0x34, 0x40)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mode(e, m):
    return getattr(e.cl, "mode", None) == m


def stub_read(e):
    L = e.cl
    L.rec("ReadMailData", (1 if e.r(3) == e.tb else 0, s32(e.r(4))))
    e.set_r(3, L.ret("ReadMailData") & MASK)


def stub_deletemail(e):
    e.cl.rec("DeleteMail", (1 if e.r(3) == e.tb else 0, s32(e.r(4))))


def stub_save(e):
    b = e.tb
    e.cl.rec("Save", (1 if e.r(3) == b else 0, e.u32(b + 0x28), e.u32(b + 0x34), e.u32(b + 0x40)))


def stub_compare(e):
    L = e.cl
    s = e.r(6)
    L.rec("compare", (e.u32(e.u32(e.r(3))), e.r(4) & MASK, e.r(5) & MASK, e.r(7) & MASK, e.read(s, 1)[0],
                      1 if e.r(3) == e.ta else 0, 1 if s == e.ts else 0))
    e.set_r(3, L.ret("compare") & MASK)


def stub_istream(e):
    L = e.cl
    b = e.tb
    L.rec("IstreamCtor", (e.r(3) - b, e.r(4) & MASK, (e.r(5) - b) & MASK))
    off = L.ret("vb")
    e.write(b, struct.pack(">I", b + off))


def stub_filebuf(e):
    e.cl.rec("FilebufCtor", (e.r(3) - e.tb, e.r(4) & MASK))


def stub_strdtor(e):
    if mode(e, "att"):
        v = e.r(4) & 0xFFFF
        e.cl.rec("StringDtor", (e.r(3) - e.tb, v - 0x10000 if v & 0x8000 else v))


def stub_delete(e):
    if mode(e, "att"):
        e.cl.rec("Delete", (1 if e.r(3) == e.tb else 0,))


def stub_delarr(e):
    if mode(e, "att"):
        e.cl.rec("DeleteArray", (1 if e.r(3) == e.td else 0,))


STUBS.internal(CALLEE_READ, stub_read)
STUBS.internal(CALLEE_DELETEMAIL, stub_deletemail)
STUBS.internal(CALLEE_SAVE, stub_save)
STUBS.internal(CALLEE_COMPARE, stub_compare)
STUBS.internal(CALLEE_ISTREAM, stub_istream)
STUBS.internal(CALLEE_FILEBUF, stub_filebuf)
STUBS.internal(CALLEE_STRDTOR, stub_strdtor)
STUBS.internal(CALLEE_DELETE, stub_delete)
STUBS.internal(CALLEE_DELARR, stub_delarr)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.mode = case["kind"]
        e.tb = e.ta = e.ts = e.td = -1
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def enc(self, a):
        return a

    def dec(self, w):
        return w

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    # ---- Outbox
    def build_list(self, la, ids):
        e = self.e
        nodes = [e.alloc(12) for _ in ids]
        sent = la + 4
        n = len(ids)
        for i, (node, v) in enumerate(zip(nodes, ids)):
            prev = nodes[i - 1] if i else sent
            nxt = nodes[i + 1] if i < n - 1 else sent
            e.write(node, struct.pack(">3I", prev, nxt, v))
        first = nodes[0] if n else sent
        last = nodes[-1] if n else sent
        e.write(la, struct.pack(">3I", n, last, first))
        return nodes

    def read_list(self, la):
        e = self.e
        n = e.u32(la)
        node = e.u32(la + 8)
        out = []
        for _ in range(min(n, 32)):
            out.append(e.u32(node + 8))
            node = e.u32(node + 4)
        return n, out

    def ob_run(self, case):
        e = self.e
        base = e.alloc(0x80)
        e.tb = base
        e.write(base, bytes(0x80))
        nodes = []
        for k, off in enumerate(OFFS):
            nodes.append(self.build_list(base + off, case["lists"][k]))
        sent = base + 0x2c
        idx = case["cursor"]
        e.write(base + 0x4c, struct.pack(">I", nodes[0][idx] if idx < len(nodes[0]) else sent))
        op = case["op"]
        r3, _, _ = T.call_ppc(e, OPS[op], [base])
        out = {"ans": (r3 & 0xFF) if op in (0, 1, 4) else 0}
        sizes = []
        for k, off in enumerate(OFFS):
            n, ids = self.read_list(base + off)
            sizes.append(n)
            out["l%d" % k] = ids
        out["sizes"] = sizes
        cur = e.u32(base + 0x4c)
        walk = []
        node = e.u32(base + 0x30)
        for _ in range(min(sizes[0], 32)):
            walk.append(node)
            node = e.u32(node + 4)
        out["cursor"] = walk.index(cur) if cur in walk else (len(walk) if cur == sent else -1)
        out["log"] = self.log()
        return out

    # ---- the string
    def call_sq(self, case, buf):
        e = self.e
        ra, rb, a, b = e.alloc(16), e.alloc(16), e.alloc(4), e.alloc(4)
        e.write(ra, struct.pack(">4I", case["lenA"], 0, 0, 0))
        e.write(rb, struct.pack(">4I", case["lenB"], 0, 0, buf))
        e.write(a, struct.pack(">I", ra))
        e.write(b, struct.pack(">I", rb))
        e.ta, e.ts = a, buf
        r3, _, _ = T.call_ppc(e, FN_EQ, [a, b])
        return r3 & 0xFF

    def call_ifs(self, obj, flag):
        self.e.tb = obj
        r3, _, _ = T.call_ppc(self.e, FN_IFS, [obj, flag & MASK])
        return r3 & MASK

    def call_att(self, obj, flag, data):
        self.e.tb, self.e.td = obj, data
        r3, _, _ = T.call_ppc(self.e, FN_ATT, [obj, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.arena = lib.tpw_ob_arena()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ob_addr(name.encode())

    def enc(self, a):
        return a - self.arena if a else 0

    def dec(self, w):
        return w + self.arena if w else 0

    def alloc(self, n):
        return self.lib.tpw_ob_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ob_run(self, case):
        data = (C.c_uint32 * 192)()
        counts = (C.c_uint32 * 3)()
        cursor = (C.c_uint32 * 1)(case["cursor"])
        for k in range(3):
            counts[k] = len(case["lists"][k])
            for i, v in enumerate(case["lists"][k]):
                data[64 * k + i] = v
        op = case["op"]
        ans = self.lib.tpw_ob_run(op, data, counts, cursor)
        out = {"ans": (ans & 0xFF) if op in (0, 1, 4) else 0}
        out["sizes"] = [counts[k] for k in range(3)]
        for k in range(3):
            out["l%d" % k] = [data[64 * k + i] for i in range(counts[k])]
        out["cursor"] = cursor[0]
        out["log"] = self.log()
        return out

    def call_sq(self, case, buf):
        return self.lib.tpw_ob_streq(case["lenA"], case["lenB"], buf) & 0xFF

    def call_ifs(self, obj, flag):
        return self.lib.tpw_ob_ifstream(obj, s32(flag))

    def call_att(self, obj, flag, data):
        return self.lib.tpw_ob_attach(obj, s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ob_addr.restype, lib.tpw_ob_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ob_alloc.restype, lib.tpw_ob_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ob_arena.restype, lib.tpw_ob_arena.argtypes = C.c_uint64, []
    lib.tpw_ob_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ob_run.restype, lib.tpw_ob_run.argtypes = C.c_uint32, [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_ob_streq.restype, lib.tpw_ob_streq.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32, C.c_uint64]
    lib.tpw_ob_ifstream.restype, lib.tpw_ob_ifstream.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_ob_attach.restype, lib.tpw_ob_attach.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ob_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the worlds


def run_sq(s, case):
    buf = s.alloc(16)
    s.write(buf, case["data"] + bytes(8))
    ans = s.call_sq(case, buf)
    return {"ans": ans, "log": s.log()}


def run_ifs(s, case):
    obj = s.alloc(0x98)
    s.words(obj, case["fill"])
    ret = s.call_ifs(obj, case["flag"])
    words = s.rd_words(obj, 37)
    syms = {}
    for nm in ADDR:
        a = s.g(nm) & MASK
        syms[a] = nm
        syms[(a + 0xC) & MASK] = nm + "+c"
    norm = []
    for i, w in enumerate(words):
        if i == 0:
            norm.append((s.dec(w) - obj) & MASK)
        elif w in syms:
            norm.append("S:" + syms[w])
        else:
            norm.append(w)
    return {"ans": 1 if ret == obj else 0, "words": norm, "log": s.log()}


def run_att(s, case):
    if case["null"]:
        obj = 0
        data = 0
    else:
        obj = s.alloc(12)
        data = s.alloc(16) if case["hasdata"] else 0
        s.words(obj, [case["w0"], case["w1"], s.enc(data)])
    ret = s.call_att(obj, case["flag"], data)
    out = {"ans": 1 if ret == obj else 0, "log": s.log()}
    if obj:
        w = s.rd_words(obj, 3)
        out["words"] = [w[0], w[1], 0 if w[2] == 0 else (1 if s.dec(w[2]) == data else 2)]
    return out


def run_ob(s, case):
    return s.ob_run(case)


RUNNERS = {"ob": run_ob, "sq": run_sq, "ifs": run_ifs, "att": run_att}


def ppc(e, case):
    s = Ppc(e, case)
    try:
        return RUNNERS[case["kind"]](s, case)
    finally:
        s.done()


def host(lib, case):
    s = Host(lib, case)
    try:
        return RUNNERS[case["kind"]](s, case)
    finally:
        s.done()


# ---------------------------------------------------------------------------------------------------------------- case generators

POOL = [0, 1, 2, 3, 0x80000000, 0xFFFFFFFF]


def rlist(r, n):
    return [r.choice(POOL) for _ in range(n)]


def ob_case(r, op, n28=None, idx=None, q=None):
    if n28 is None:
        n28 = r.choice([0, 1, 2, 3, 5])
    if op in (2, 3) and n28 == 0:
        n28 = 1
    lists = [rlist(r, n28), rlist(r, r.randrange(0, 5)), rlist(r, r.randrange(0, 5))]
    if idx is None:
        idx = r.randrange(n28) if n28 else 0
    return {"kind": "ob", "op": op, "lists": lists, "cursor": idx,
            "queues": {"ReadMailData": [r.choice([0, 1]) if q is None else q]}}


def ob_edges(op):
    def gen():
        r = random.Random(100 + op)
        for n in (0, 1, 2, 4):
            for q in (0, 1):
                yield ob_case(r, op, n, None, q)
                yield ob_case(r, op, n, 0, q)
                if n > 1:
                    yield ob_case(r, op, n, n - 1, q)
    return gen


def ob_random(op):
    return lambda r: ob_case(r, op)


def sq_case(r, q=None):
    return {"kind": "sq", "lenA": r.choice([0, 1, 3, 8, 0xFFFFFFFF, r.randrange(0, 1000)]),
            "lenB": r.choice([0, 1, 3, 8, 0x80000000, r.randrange(0, 1000)]),
            "data": bytes(r.randrange(256) for _ in range(8)),
            "queues": {"compare": [r.choice([0, 1, -1, 0x100, 0x80000000, 0x7FFFFFFF]) if q is None else q]}}


def sq_edges():
    r = random.Random(7)
    for q in (0, 1, -1, 0x100, 0x80000000, 0x7FFFFFFF, 0x10000):
        yield sq_case(r, q)
        yield sq_case(r, q)


def ifs_case(r, flag=None):
    return {"kind": "ifs", "fill": [r.randrange(1 << 32) for _ in range(37)],
            "flag": r.choice([0, 1, -1, 2, 0x8000, 0x10000, 0xFFFF, 0x7FFF, -32768, 0x20000]) if flag is None else flag,
            "queues": {"vb": [r.choice([0x4C, 0x10, 0x20])]}}


def ifs_edges():
    r = random.Random(11)
    for flag in (0, 1, -1, 0x8000, 0x10000, 0xFFFF, 0x7FFF, 0x20000):
        for _ in range(2):
            yield ifs_case(r, flag)


def att_case(r, null=None, hasdata=None, flag=None):
    return {"kind": "att", "null": r.random() < 0.1 if null is None else null,
            "hasdata": r.random() < 0.6 if hasdata is None else hasdata,
            "w0": r.randrange(1 << 32), "w1": r.randrange(1 << 32),
            "flag": r.choice([-1, 0, 1, 2, 0x8000, 0x10000, 0x7FFF, -32768, 0xFFFF]) if flag is None else flag,
            "queues": {}}


def att_edges():
    r = random.Random(13)
    yield att_case(r, True, False, 1)
    yield att_case(r, True, False, 0)
    for hasdata in (False, True):
        for flag in (-1, 0, 1, 2, 0x8000, 0x10000, 0x7FFF, -32768, 0xFFFF):
            yield att_case(r, False, hasdata, flag)


TESTS = [
    FnTest("Outbox::First", ppc, host, edge=ob_edges(0), rnd=ob_random(0)),
    FnTest("Outbox::Next", ppc, host, edge=ob_edges(1), rnd=ob_random(1)),
    FnTest("Outbox::MarkToSend", ppc, host, edge=ob_edges(2), rnd=ob_random(2)),
    FnTest("Outbox::MarkToDelete", ppc, host, edge=ob_edges(3), rnd=ob_random(3)),
    FnTest("Outbox::Delete", ppc, host, edge=ob_edges(4), rnd=ob_random(4)),
    FnTest("std::operator==<char,std::char_traits<char>,std::allocator<char>>", ppc, host, edge=sq_edges, rnd=sq_case),
    FnTest("std::basic_ifstream<char,std::char_traits<char>>::basic_ifstream", ppc, host, edge=ifs_edges, rnd=ifs_case),
    FnTest("Email::Attachment::~Attachment", ppc, host, edge=att_edges, rnd=att_case),
]
