"""Unit v5376, std::basic_string<char> assign(str, pos, n) and change_size, against libtpw_unit_std_basic_string_assign.dylib
(tests/unit_std_basic_string_assign.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  std::basic_string<char,std::char_traits<char>,std::allocator<char>>::assign 0x10001c80
  std::basic_string<char,std::char_traits<char>,std::allocator<char>>::change_size 0x100027b0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (strlen, new, new[],
delete, strcpy, memcpy, __throw, the counted-pointer constructor 0x100026ac, replace 0x10002d20) are fakes on both sides that record their
arguments (pointers of the world as labels, copied bytes and message texts as checksums); replace does nothing, __throw returns.

The world of a case: up to two string words, up to two representations (len, cap, refs, a data block with guard bytes around it) which the
string words point at (equal reps = shared, equal strings = self assignment), and the terminator byte global 0x10278bd3. What is compared: the
answer (is it the this pointer), the log, every string word's representation (fields, data bytes of a new block), every original rep and
every original data block with its guards.

No fused operations. Not tested: the doubling loop of change_size with a capacity of 0 or a size past 2^31 (it never ends in the original),
a length past the capacity, an assign position past the end together with a count that reads past the data block (the reads would see
unrelated memory), the dead landing pads (delete[], __end__catch) and a source rep with refs 0 in the share branch (unreachable).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_assign"
MASK = 0xFFFFFFFF

FN_ASSIGN, FN_CS = 0x10001C80, 0x100027B0
SIZES = {FN_ASSIGN: 732, FN_CS: 884}
OWN = set(SIZES)

G_STRLEN, G_NEWA, G_STRCPY, G_THROW, G_DELETE, G_NEW, G_MEMCPY = 0x10238650, 0x10238668, 0x10238410, 0x10238680, 0x10238218, 0x102384b8, 0x102386B0
RCCTOR, REPLACE = 0x100026AC, 0x10002D20

ADDR = dict(nul=0x10278BD3)
VTBL_IDS = {0x10278BA0: 1, 0x10278BB0: 2, 0x10278BC0: 3, 0x10278B74: 4}
DTOR_IDS = {0x10265698: 1, 0x10265680: 2}
HOST_LAYOUT = [0, 4, 8, 16, 24]

STR_L, REP_L, DATA_L = 1, 10, 20
PAD = 8


def hsum(b):
    return sum((i + 1) * x for i, x in enumerate(b)) & MASK


def cs(e, a):
    v = e.cstr(a)
    return v.encode("latin1") if isinstance(v, str) else bytes(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    n = len(cs(e, e.r(3)))
    e.cl.rec("strlen", (n,))
    e.set_r(3, n)


def st_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    a = e.alloc(max(size, 4))
    e.write(a, bytes(max(size, 4)))
    e.set_r(3, a)


def st_newa(e):
    size = e.r(3)
    e.cl.rec("newa", (size,))
    a = e.alloc(max(size, 4))
    e.write(a, bytes(max(size, 4)))
    e.set_r(3, a)


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_strcpy(e):
    d, s = e.r(3), e.r(4)
    data = cs(e, s)
    e.cl.rec("strcpy", (len(data),))
    e.write(d, data + b"\0")
    e.set_r(3, d)


def st_memcpy(e):
    d, s, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("memcpy", (e.cl.lab(d), n, hsum(e.read(s, min(n, 4096)))))
    if n:
        e.write(d, e.read(s, n))
    e.set_r(3, d)


def st_throw(e):
    name, obj, dtor = e.r(3), e.r(4), e.r(5)
    e.cl.rec("throw", (hsum(cs(e, name)), VTBL_IDS.get(e.u32(obj), 0), hsum(cs(e, e.u32(obj + 4))), DTOR_IDS.get(dtor, 0)))


def st_rcctor(e):
    self_, p = e.r(3), e.r(4)
    e.cl.rec("rcctor", (1 if p else 0,))
    e.write(self_, struct.pack(">II", p, 0))
    e.set_r(3, self_)


def st_replace(e):
    L = e.cl
    L.rec("replace", (L.lab(e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7) & 0xFF))


def _glue(fn):
    """The cross-TOC glue saves r2 in the caller's frame (stw r2,0x14(r1)); the code reloads it after every call."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


for _a, _f in ((G_STRLEN, st_strlen), (G_NEW, st_new), (G_NEWA, st_newa), (G_DELETE, st_delete), (G_STRCPY, st_strcpy),
               (G_MEMCPY, st_memcpy), (G_THROW, st_throw), (RCCTOR, st_rcctor), (REPLACE, st_replace)):
    STUBS.internal(_a, _glue(_f))


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_world(self, case):
        """Allocates every block of the case; returns the (address, label) pairs."""
        w = {"strs": [self.alloc(8) for _ in range(2)], "reps": [], "blks": [], "data": []}
        for rep in case["reps"]:
            w["reps"].append(self.alloc(32))
            blk = self.alloc(PAD + len(rep["data"]) + PAD)
            w["blks"].append(blk)
            w["data"].append(blk + PAD)
        self.w = w
        pairs = [(a, STR_L + i) for i, a in enumerate(w["strs"])]
        pairs += [(a, REP_L + i) for i, a in enumerate(w["reps"])]
        pairs += [(a, DATA_L + i) for i, a in enumerate(w["data"])]
        return pairs


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    data_off = 0xC

    def __init__(self, e, case):
        self.e = e
        pairs = self.make_world(case)
        self.L = e.cl = CL.PpcLog({a: l for a, l in pairs}, case["queues"])
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

    def call_assign(self, this, src, pos, n):
        r3, _, _ = T.call_ppc(self.e, FN_ASSIGN, [this, src, pos & MASK, n & MASK])
        return (r3 & MASK) == this

    def call_cs(self, this, n, keep):
        T.call_ppc(self.e, FN_CS, [this, n & MASK, keep])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    data_off = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        pairs = self.make_world(case)
        CL.host_begin(lib, {l: a for a, l in pairs}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sba_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sba_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_assign(self, this, src, pos, n):
        return self.lib.tpw_sba_assign(this, src, pos & MASK, n & MASK) == this

    def call_cs(self, this, n, keep):
        self.lib.tpw_sba_change_size(this, n & MASK, keep)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sba_addr.restype, lib.tpw_sba_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sba_alloc.restype, lib.tpw_sba_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sba_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sba_assign.restype, lib.tpw_sba_assign.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_sba_change_size.restype, lib.tpw_sba_change_size.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sba_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    w = s.w
    for i, rep in enumerate(case["reps"]):
        a = w["reps"][i]
        s.words(a, [rep["len"], rep["cap"], rep["refs"]])
        s.wptr(a + s.data_off, w["data"][i])
        s.write(w["blks"][i], bytes([0xA5] * PAD) + rep["data"] + bytes([0x5A] * PAD))
    for k, ri in enumerate(case["strs"]):
        s.wptr(w["strs"][k], w["reps"][ri])
    s.write(s.g("nul"), bytes([case["nul"]]))


def rep_dump(s, a, known):
    f = s.rd_words(a, 3)
    p = s.rptr(a + s.data_off)
    lab = known["data"].get(p, -1)
    extra = b"" if lab != -1 else s.read(p, min(f[1] + 1, 600))
    return (f, lab, extra)


def observe(s, case, answer):
    w = s.w
    known = {"rep": {a: i for i, a in enumerate(w["reps"])}, "data": {a: i for i, a in enumerate(w["data"])}}
    out = {"answer": answer, "log": s.log()}
    strs = []
    for k in range(len(case["strs"])):
        ra = s.rptr(w["strs"][k])
        strs.append((known["rep"].get(ra, -1), rep_dump(s, ra, known) if ra not in known["rep"] else None))
    out["strs"] = strs
    out["reps"] = [rep_dump(s, a, known) for a in w["reps"]]
    out["blocks"] = [s.read(b, PAD + len(case["reps"][i]["data"]) + PAD) for i, b in enumerate(w["blks"])]
    return out


def execute(s, case):
    w = s.w
    this = w["strs"][0]
    if case["op"] == "assign":
        src = w["strs"][0] if case["same"] else w["strs"][1]
        return s.call_assign(this, src, *case["args"])
    return s.call_cs(this, *case["args"])


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


# ---------------------------------------------------------------------------------------------------------------- cases


def mkrep(r, length=None, cap=None, refs=None, dsz=None):
    if cap is None:
        cap = r.choice([1, 2, 3, 4, 5, 7, 8, 9, 12, 15, 16, 20, 31])
    if length is None:
        length = r.randrange(0, min(cap, 40) + 1)
    if refs is None:
        refs = r.choice([0, 1, 1, 2, 3])
    if dsz is None:
        dsz = cap + 1
    return {"len": length, "cap": cap, "refs": refs, "data": bytes(r.randrange(33, 127) for _ in range(dsz))}


def base(r, op, reps, strs, args, same=False):
    return {"op": op, "queues": {}, "reps": reps, "strs": strs, "args": args, "same": same, "nul": r.choice([0, 0, 7, 0xFF])}


def assign_case(r, kind=None, pos=None, n=None):
    kind = kind or r.choice(["distinct", "distinct", "distinct", "shared", "same"])
    if kind == "distinct":
        reps, strs, same = [mkrep(r), mkrep(r)], [0, 1], False
    elif kind == "shared":
        reps, strs, same = [mkrep(r, refs=r.choice([1, 2, 3]))], [0, 0], False
    else:
        reps, strs, same = [mkrep(r)], [0, 0], True
    srcrep = reps[strs[1]] if not same else reps[0]
    length = srcrep["len"]
    if pos is None:
        pos = r.choice([0, 0, 1, length // 2, length, length, length + 1, length + 5])
    if n is None:
        if pos > length:
            n = 0
        else:
            n = r.choice([0, 1, 3, length - pos, length - pos, length - pos + 1, 0xFFFFFFFF, 0xFFFFFFFF, r.randrange(0, 40)])
    if pos > length:
        n = 0
    return base(r, "assign", reps, strs, [pos, n], same)


def cs_case(r, n=None, keep=None, cap=None, refs=None, length=None):
    rep = mkrep(r, length=length, cap=cap, refs=refs)
    c = rep["cap"]
    if n is None:
        n = r.choice([0, 1, c - 1, c, c, c + 1, c + 2, 2 * c, 2 * c + 1, r.randrange(0, 100), r.randrange(0, 300)])
    if keep is None:
        keep = r.choice([0, 1])
    return base(r, "cs", [rep], [0], [n, keep])


def assign_edges():
    r = random.Random(5376)
    for kind in ("distinct", "shared", "same"):
        for _ in range(12):
            yield assign_case(r, kind)
    for _ in range(40):
        yield assign_case(r, "distinct", pos=0, n=0xFFFFFFFF)
    for _ in range(10):
        c = assign_case(r, "distinct")
        c["reps"][1]["refs"] = 0
        c["reps"][0]["refs"] = r.choice([0, 1, 2, 3])
        yield c
    for refs in (0, 1, 2, 3):
        for srefs in (0, 1, 2):
            c = assign_case(r, "distinct", pos=0, n=0xFFFFFFFF)
            c["reps"][0]["refs"], c["reps"][1]["refs"] = refs, srefs
            yield c
    c = assign_case(r, "distinct", pos=0, n=0xFFFFFFFF)
    c["reps"][0]["refs"], c["reps"][1]["refs"] = 2, 2
    c["reps"][1]["len"] = 0
    yield c


def cs_edges():
    r = random.Random(5377)
    for refs in (0, 1, 2, 3):
        for keep in (0, 1):
            for _ in range(6):
                yield cs_case(r, keep=keep, refs=refs)
    for n in (0, 1, 4, 5):
        for keep in (0, 1):
            yield cs_case(r, n=n, keep=keep, cap=4, refs=2, length=3)
            yield cs_case(r, n=n, keep=keep, cap=4, refs=1, length=3)
    for c in (1, 2, 3, 4, 5, 8):
        for n in (c, c + 1, 2 * c, 2 * c + 1, 2 * c + 3):
            yield cs_case(r, n=n, keep=1, cap=c, refs=1, length=c)
    # the length error: the capacity is already as large as the request, so the write lands at data - 1 (32-bit wrap)
    for refs in (0, 1):
        c = base(r, "cs", [mkrep(r, length=2, cap=0xFFFFFFFF, refs=refs, dsz=8)], [0], [0xFFFFFFFF, 0])
        yield c


def assign_random(r):
    return assign_case(r)


def cs_random(r):
    return cs_case(r)


NAME = "std::basic_string<char,std::char_traits<char>,std::allocator<char>>::"
TESTS = [
    FnTest(NAME + "change_size", ppc, host, edge=cs_edges, rnd=cs_random),
    FnTest(NAME + "assign", ppc, host, edge=assign_edges, rnd=assign_random),
]
