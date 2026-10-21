"""Unit v51070: FTPClient::_threadfn and 18 small library functions (constructors, destructors, list erase and splice, string
equality, tree find, stream sentries, seekoff and seekpos, a cursor copy), against libtpw_unit_FTPClient__threadfn.dylib
(tests/unit_FTPClient__threadfn.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp, src/game/online/FTPClient.cpp.

A case is a world and a call. The world is one 0x4000 byte block built twice by ONE piece of Python code (big-endian words in the
emulator, little-endian words on the host); objects sit at fixed offsets in it, so every pointer a callee logs is the offset from
the block (anything else is -1). The callees outside the unit (operator new and delete, strlen, strcpy, __throw, the wide string
destructor and compares and less-than, the token list clear, flush, ProcessEvents, bthread_cond::signal, Sock::Close) are fakes on
both sides that record their arguments and answer from the case's queues. What is compared: the answer, the whole block (pointers
normalised to offsets or names), the globals the constructor writes and the event log.

No fused operations. Not tested: a list node whose string word is 0 (erase reads the representation at address 0), a NULL list or
tree argument (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FTPClient__threadfn"
MASK = 0xFFFFFFFF
WS = 0x4000

ENTRY = dict(threadfn=0x1020c860, ipctor=0x1020ca80, ipdtor=0x1020cb7c, erase=0x1020db8c, sockctor=0x1020e2a0, sockdtor=0x1020e2d4,
             splice=0x1020abec, listdtor=0x1020e560, tokendtor=0x1020eeec, weq=0x1021f130, nweq=0x1021f254, ostream=0x1021f7b0,
             tie=0x1021f888, wfind=0x1021fae4, nfind=0x1021fd4c, seekpos=0x10220194, seekoff=0x1022021c, istream=0x1022083c,
             itercopy=0x101f5d6c)
SIZES = dict(zip([ENTRY[k] for k in ENTRY], [64, 36, 76, 312, 20, 100, 432, 92, 96, 76, 76, 132, 8, 196, 196, 20, 20, 68, 28]))
OWN = set(SIZES)
ARGP = dict(threadfn=(0,), ipctor=(0,), ipdtor=(0,), erase=(0, 1), sockctor=(0,), sockdtor=(0,), splice=(0, 2), listdtor=(0,),
            tokendtor=(0,), weq=(0, 1), nweq=(0, 1), ostream=(0,), tie=(0,), wfind=(0, 1, 2), nfind=(0, 1, 2), seekpos=(0,),
            seekoff=(0,), istream=(0,), itercopy=(0, 1))   # the arguments that are native pointers on the host
RET_PTR = set(ENTRY) - {"weq", "nweq", "tie"}
CONSTS = {0x102b1940: "vt", 0x102b1954: "svt", 0x102b1804: "codes", 0x102b186c: "texts"}
G_CODES, G_TEXTS = 0x10396d1c, 0x10396d18
HOST_LAYOUT = [8, 4]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def chk(text):
    return sum((i + 1) * b for i, b in enumerate(text))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rel(e, a):
    off = (a - e.cl.W) & MASK
    return off if off < WS else -1


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def alloc_or_null(e, key, size):
    if e.cl.ret(key) == 0:
        return 0
    return e.alloc(max(size, 4))


STUBS.internal(0x1020c8cc, lambda e: rec(e, "ProcessEvents", rel(e, e.r(3))))


def stub_signal(e):
    rec(e, "signal", rel(e, e.r(3)))
    e.set_r(3, 0)


STUBS.internal(0x10200fb4, stub_signal)
STUBS.internal(0x1020e358, lambda e: rec(e, "SockClose", rel(e, e.r(3))))
STUBS.internal(0x101468f8, lambda e: rec(e, "WStringDtor", rel(e, e.r(3)), s16(e.r(4))))
STUBS.internal(0x102109fc, lambda e: rec(e, "ListClear", rel(e, e.r(3))))
STUBS.internal(0x102268f8, lambda e: rec(e, "flush", rel(e, e.r(3))))
def glue(fn):
    """A glue import faked at its address: the glue saves the caller's TOC pointer at 0x14(r1), which the caller reloads."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(0x10238218, glue(lambda e: rec(e, "delete", rel(e, e.r(3)))))        # the glue of operator delete
STUBS.internal(0x10238638, glue(lambda e: rec(e, "deleteArray", rel(e, e.r(3)))))   # operator delete[]


def stub_new(e):
    rec(e, "new", e.r(3))
    e.set_r(3, alloc_or_null(e, "new", e.r(3)))


def stub_new_array(e):
    rec(e, "newArray", e.r(3))
    e.set_r(3, alloc_or_null(e, "newArray", e.r(3)))


STUBS.internal(0x102384b8, glue(stub_new))
STUBS.internal(0x10238668, glue(stub_new_array))


def stub_strlen(e):
    text = e.cstr(e.r(3))
    text = text.encode() if isinstance(text, str) else bytes(text)
    rec(e, "strlen", chk(text))
    e.set_r(3, len(text))


def stub_strcpy(e):
    src = e.cstr(e.r(4))
    src = src.encode() if isinstance(src, str) else bytes(src)
    rec(e, "strcpy", int(e.r(3) != 0), chk(src))


def stub_throw(e):
    text = e.cstr(e.r(3))
    text = text.encode() if isinstance(text, str) else bytes(text)
    rec(e, "throw", chk(text), int(e.r(4) != 0), int(e.r(5) == 0x10265680))


STUBS.internal(0x10238650, glue(stub_strlen))
STUBS.internal(0x10238410, glue(stub_strcpy))
STUBS.internal(0x10238680, glue(stub_throw))


def stub_compare(name):
    def fn(e):
        rec(e, name, rel(e, e.r(3)), e.r(4), e.r(5), rel(e, e.r(6)), e.r(7))
        e.set_r(3, e.cl.ret(name) & MASK)
    return fn


def stub_less(name):
    def fn(e):
        rec(e, name, rel(e, e.r(3)), rel(e, e.r(4)))
        e.set_r(3, e.cl.ret(name) & 1)
    return fn


STUBS.internal(0x101d5864, stub_compare("compare"))
STUBS.internal(0x10225a5c, stub_compare("ncompare"))
STUBS.internal(0x10225e8c, stub_less("lt"))
STUBS.internal(0x10225d2c, stub_less("nlt"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def nv(self, w):
        w &= MASK
        if (w - self.W) & MASK < WS:
            return ("p", w - self.W)
        return self.consts.get(w, w)

    def dump(self, n=WS // 4):
        return [self.nv(w) for w in self.rd_words(self.W, n)]

    def new_world(self):
        """W is the world's word address (an arena offset on the host), NW its native address."""
        self.NW = self.alloc(WS)
        self.base = self.get_base()
        self.W = self.NW - self.base
        if self.W < 0x2000000:      # small arena offsets would make small constants look like pointers in the dump
            self.alloc(0x2000000)
            self.NW = self.alloc(WS)
            self.W = self.NW - self.base
        self.start()
        return self.W


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.W = 0
        self.consts = CONSTS
        self.saved = STUBS.activate(e)

    def start(self):
        self.L.W = self.W

    def get_base(self):
        return 0

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

    def call(self, op, *args):
        a = [x & MASK for x in args] + [0] * (4 - len(args))
        return T.call_ppc(self.e, ENTRY[op], a)[0] & MASK

    def setptr(self, which, v):
        self.words(G_CODES if which == 0 else G_TEXTS, [v])

    def getptr(self, which):
        return self.rd_words(G_CODES if which == 0 else G_TEXTS, 1)[0]


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.consts = {lib.tpw_v51070_addr(n.encode()) & MASK: n for n in ("vt", "svt", "codes", "texts")}

    def start(self):
        self.lib.tpw_v51070_world(self.NW)

    def get_base(self):
        return self.lib.tpw_v51070_base()

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_v51070_alloc(n)

    def read(self, a, n):
        return C.string_at(a + self.base, n)

    def write(self, a, data):
        C.memmove(a + self.base, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, *args):
        a = [x & MASK for x in args] + [0] * (4 - len(args))
        for k in ARGP[op]:
            if a[k]:
                a[k] += self.base
        r = getattr(self.lib, "tpw_v51070_" + op)(*a)
        if op in RET_PTR and r:
            r -= self.base
        return r & MASK

    def setptr(self, which, v):
        self.lib.tpw_v51070_setptr(which, v & MASK)

    def getptr(self, which):
        return self.lib.tpw_v51070_getptr(which) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51070_addr.restype, lib.tpw_v51070_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51070_alloc.restype, lib.tpw_v51070_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51070_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v51070_world.argtypes = [C.c_uint64]
    lib.tpw_v51070_base.restype, lib.tpw_v51070_base.argtypes = C.c_uint64, []
    lib.tpw_v51070_setptr.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_v51070_getptr.restype, lib.tpw_v51070_getptr.argtypes = C.c_uint64, [C.c_int32]
    for op in ENTRY:
        f = getattr(lib, "tpw_v51070_" + op)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 4
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51070_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the worlds

OBJ = 0x100


def list_nodes(s, head, nodes, sentinel_off=4):
    """Links a ring: head+4 is the sentinel (prev at +4, next at +8 of the list); nodes are addresses, data left alone."""
    sent = head + 4
    ring = [sent] + nodes
    n = len(ring)
    for k, a in enumerate(ring):
        s.words(a, [ring[(k - 1) % n], ring[(k + 1) % n]])
    return sent


def b_threadfn(s, c):
    return [s.W + OBJ]


def b_ipctor(s, c):
    s.words(s.W + 0x10, [c["rw"][0]])
    s.setptr(0, c["rw"][1])
    s.setptr(1, c["rw"][2])
    return [s.W + 0x10]


def selfarg(s, c, words):
    s.words(s.W + OBJ, words)
    return 0 if c["null"] else s.W + OBJ


def b_flagged(s, c):
    return [selfarg(s, c, c["rw"][:4]), c["flag"]]


def b_sockctor(s, c):
    s.words(s.W + OBJ, c["rw"][:2])
    return [s.W + OBJ]


def b_erase(s, c):
    W = s.W
    n, i, j = c["n"], c["i"], c["j"]
    head = W + 0x80
    addr = [W + 0x200 + k * 0x20 for k in range(n)]
    sent = list_nodes(s, head, addr)
    s.words(head, [n])
    for k in range(n):
        rep = W + 0x1000 + k * 0x20
        s.words(addr[k] + 8, [rep])
        s.words(rep, [c["rw"][k], c["rw"][k + 6], c["refs"][k], W + 0x2000 + k * 0x10])
    s.words(W + 0x40, [c["rw"][12]])
    pts = addr + [sent]
    return [W + 0x40, head, pts[i], pts[j]]


def b_splice(s, c):
    W = s.W
    n1, n2 = c["n1"], c["n2"]
    h1, h2 = W + 0x100, W + 0x180
    a1 = [W + 0x300 + k * 0x10 for k in range(n1)]
    a2 = [W + 0x800 + k * 0x10 for k in range(n2)]
    sent = list_nodes(s, h1, a1)
    list_nodes(s, h2, a2)
    for k, a in enumerate(a1 + a2):
        s.words(a + 8, [c["rw"][k]])
    size1 = n1 if c["big"] is None else (0x7FFFFFFF - n2 + c["big"]) & MASK
    s.words(h1, [size1])
    s.words(h2, [n2])
    return [h1, (a1 + [sent])[c["pos"]], h2]


def b_weq(s, c):
    W = s.W
    s.words(W + 0x100, [W + 0x200])
    s.words(W + 0x110, [W + 0x220])
    s.words(W + 0x200, [c["sa"], c["rw"][0], c["rw"][1], W + 0x300])
    s.words(W + 0x220, [c["sb"], c["rw"][2], c["rw"][3], W + 0x340])
    return [W + 0x100, W + 0x110]


def b_ostream(s, c):
    W = s.W
    me = W + OBJ
    s.write(me, bytes([c["b0"], c["b1"]]))
    s.write(me + 2, struct.pack(s.fmt, W + 0x200))
    s.words(W + 0x200, [W + 0x300])
    s.words(W + 0x300 + 0x30, [c["flags"], c["state"]])
    return [0 if c["null"] else me, c["flag"]]


def b_tie(s, c):
    W = s.W
    s.words(W + OBJ + 0x3C, [W + 0x500 if c["ptr"] else c["rw"][0]])
    s.words(W + OBJ, c["rw"][1:4])
    return [W + OBJ]


def b_find(s, c):
    W = s.W
    n = c["n"]
    addr = [W + 0x400 + k * 0x20 for k in range(n)]
    tree = W + 0x100
    slots = [("root",)]
    child = {k: [0, 0] for k in range(n)}
    root = 0
    for k in range(n):
        slot = slots.pop(int(c["place"][k] * len(slots)))
        if slot[0] == "root":
            root = addr[k]
        else:
            child[slot[0]][slot[1]] = addr[k]
        slots += [(k, 0), (k, 1)]
    s.words(tree, [c["rw"][0], root, c["rw"][1]])
    for k in range(n):
        s.words(addr[k], [child[k][0], child[k][1], c["rw"][2 + k]])
    s.words(W + 0x200, c["rw"][8:12])
    s.words(W + 0x40, [c["rw"][12]])
    return [W + 0x40, tree, W + 0x200]


def b_seek(s, c):
    s.words(s.W + 0x40, c["rw"][:2])
    return [s.W + 0x40, c["rw"][2], c["rw"][3], c["rw"][4]]


def b_itercopy(s, c):
    s.words(s.W + 0x100, c["rw"][:3])
    s.words(s.W + 0x140, [s.W + 0x500 if c["ptr"] else c["rw"][3], c["rw"][4], c["rw"][5]])
    return [s.W + 0x100, s.W + 0x140]


BUILD = dict(threadfn=b_threadfn, ipctor=b_ipctor, ipdtor=b_flagged, erase=b_erase, sockctor=b_sockctor, sockdtor=b_flagged,
             splice=b_splice, listdtor=b_flagged, tokendtor=b_flagged, weq=b_weq, nweq=b_weq, ostream=b_ostream, tie=b_tie,
             wfind=b_find, nfind=b_find, seekpos=b_seek, seekoff=b_seek, istream=b_flagged, itercopy=b_itercopy)


def scenario(s, case):
    op = case["op"]
    s.new_world()
    args = BUILD[op](s, case)
    answer = s.call(op, *args)
    out = {"answer": None if op in ("splice", "erase") else s.nv(answer), "log": s.log(), "dump": s.dump()}
    if op == "ostream":      # the unaligned stream word differs in byte order: compare the two bytes before it
        out["dump"][OBJ // 4] = out["dump"][OBJ // 4 + 1] = None
        out["sentry"] = s.read(s.W + OBJ, 2)
    if op == "ipctor":
        out["codes"] = s.nv(s.getptr(0))
        out["texts"] = s.nv(s.getptr(1))
    return out


def ppc(e, case):
    s = Ppc(e, case)
    try:
        return scenario(s, case)
    finally:
        s.done()


def host(lib, case):
    return scenario(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases


def base(r, op, **kw):
    c = {"op": op, "queues": {"new": [1], "newArray": [1], "compare": [0], "ncompare": [0], "lt": [0], "nlt": [0]},
         "rw": [r.getrandbits(32) for _ in range(16)], "null": False, "flag": 0}
    c.update(kw)
    return c


def flagv(r, f=None):
    if f is None:
        f = r.choice([0, 1, -1, 2, 0x7FFF, -0x8000, r.randrange(-100, 100)])
    return (f & 0xFFFF) | (r.choice([0, 0xFFFF, r.getrandbits(16)]) << 16)


def g_plain(op):
    return lambda r: base(r, op)


def g_flagged(op):
    return lambda r: base(r, op, null=r.random() < 0.2, flag=flagv(r))


def g_erase(r):
    n = r.randrange(0, 6)
    i = r.randrange(0, n + 1)
    return base(r, "erase", n=n, i=i, j=r.randrange(i, n + 1), refs=[r.choice([0, 1, 1, 2, 3, 0x80000000]) for _ in range(6)])


def g_splice(r):
    n1, n2 = r.randrange(0, 4), r.randrange(0, 4)
    return base(r, "splice", n1=n1, n2=n2, pos=r.randrange(0, n1 + 1), big=r.choice([None, None, -1, 0, 1, 5]))


def g_weq(op):
    def g(r):
        c = base(r, op, sa=r.choice([0, 1, 5, r.getrandbits(32)]), sb=r.choice([0, 1, 5, r.getrandbits(32)]))
        c["queues"][("compare" if op == "weq" else "ncompare")] = [r.choice([0, 0, 1, -1, 5, 0x80000000, 0x100000000 - 1])]
        return c
    return g


def g_ostream(r):
    return base(r, "ostream", null=r.random() < 0.15, flag=flagv(r), b0=r.randrange(256), b1=r.choice([0, 0, 1, 0x80, r.randrange(256)]),
                flags=r.choice([0x2000, 0, r.getrandbits(32), r.getrandbits(32) | 0x2000, r.getrandbits(32) & ~0x2000]),
                state=r.choice([0, 1, 4, 5, 2, 0x10, r.getrandbits(32)]))


def g_tie(r):
    return base(r, "tie", ptr=r.random() < 0.5)


def g_find(op):
    def g(r):
        c = base(r, op, n=r.randrange(0, 7), place=[r.random() for _ in range(6)])
        c["queues"]["lt" if op == "wfind" else "nlt"] = [r.randrange(2) for _ in range(r.randrange(1, 20))]
        return c
    return g


def g_itercopy(r):
    return base(r, "itercopy", ptr=r.random() < 0.5)


GEN = dict(threadfn=g_plain("threadfn"), ipctor=g_plain("ipctor"), ipdtor=g_flagged("ipdtor"), erase=g_erase, sockctor=g_plain("sockctor"),
           sockdtor=g_flagged("sockdtor"), splice=g_splice, listdtor=g_flagged("listdtor"), tokendtor=g_flagged("tokendtor"),
           weq=g_weq("weq"), nweq=g_weq("nweq"), ostream=g_ostream, tie=g_tie, wfind=g_find("wfind"), nfind=g_find("nfind"),
           seekpos=g_plain("seekpos"), seekoff=g_plain("seekoff"), istream=g_flagged("istream"), itercopy=g_itercopy)


def edges(op):
    r = random.Random(sum(ord(ch) for ch in op) * 77 + 5)
    g = GEN[op]
    if op in ("ipdtor", "sockdtor", "listdtor", "tokendtor", "istream", "ostream"):
        for null in (True, False):
            for f in (0, 1, -1, 2, 0x7FFF, -0x8000):
                c = g(r)
                c["null"], c["flag"] = null, flagv(r, f)
                if op == "ostream":
                    for flags in (0x2000, 0x0, 0x1FFF, 0x2000 | 0x5):
                        for state in (0, 1, 4, 5, 2):
                            for b1 in (0, 1):
                                d = dict(c, flags=flags, state=state, b1=b1)
                                yield d
                else:
                    yield c
    elif op == "erase":
        for n in range(0, 4):
            for i in range(n + 1):
                for j in range(i, n + 1):
                    c = g(r)
                    c.update(n=n, i=i, j=j, refs=[r.choice([0, 1, 2, 3]) for _ in range(6)])
                    yield c
    elif op == "splice":
        for n1 in range(0, 3):
            for n2 in range(0, 3):
                for pos in range(n1 + 1):
                    for big in (None, -1, 0, 1):
                        for qn in (1, 0):
                            c = g(r)
                            c.update(n1=n1, n2=n2, pos=pos, big=big)
                            c["queues"]["newArray"] = [qn]
                            c["queues"]["new"] = [1 - qn if qn else 0]
                            yield c
                            c = g(r)
                            c.update(n1=n1, n2=n2, pos=pos, big=big)
                            c["queues"]["new"] = [qn]
                            yield c
    elif op in ("wfind", "nfind"):
        key = "lt" if op == "wfind" else "nlt"
        for n in range(0, 5):
            for bits in ([0], [1], [0, 1], [1, 0], [0, 0, 1], [1, 1, 0], [0, 1, 1, 0]):
                c = g(r)
                c["n"] = n
                c["queues"][key] = bits
                yield c
    elif op in ("weq", "nweq"):
        key = "compare" if op == "weq" else "ncompare"
        for q in (0, 1, -1, 0x80000000, 0x10000, 0x100):
            c = g(r)
            c["queues"][key] = [q & MASK]
            yield c
    else:
        for _ in range(8):
            yield g(r)
        if op in ("tie", "itercopy"):
            for ptr in (True, False):
                yield dict(g(r), ptr=ptr)


def mk(name, op):
    return FnTest(name, ppc, host, edge=lambda: edges(op), rnd=GEN[op])


WSTR = "std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>"
NSTR = "std::basic_string<wchar_t,nocase_wchar_traits,std::allocator<wchar_t>>"
CSTR = "std::basic_string<char,std::char_traits<char>,std::allocator<char>>"
WLIST = "std::list<%s,std::allocator<%s>>" % (WSTR, WSTR)
CLIST = "std::list<%s,std::allocator<%s>>" % (CSTR, CSTR)

TESTS = [
    mk("FTPClient::_threadfn", "threadfn"),
    mk("IPBase::IPBase", "ipctor"),
    mk("IPBase::~IPBase", "ipdtor"),
    mk(WLIST + "::erase", "erase"),
    mk("Sock::Sock", "sockctor"),
    mk("Sock::~Sock", "sockdtor"),
    mk(CLIST + "::splice", "splice"),
    mk("std::list<CToken,std::allocator<CToken>>::~list", "listdtor"),
    mk("CToken::~CToken", "tokendtor"),
    mk("std::operator==<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>", "weq"),
    mk("std::operator==<wchar_t,nocase_wchar_traits,std::allocator<wchar_t>>", "nweq"),
    mk("std::basic_ostream<wchar_t,std::char_traits<wchar_t>>::sentry::~sentry", "ostream"),
    mk("std::basic_ios<wchar_t,std::char_traits<wchar_t>>::tie", "tie"),
    mk("std::__tree<%s,std::less<%s>,std::allocator<%s>>::find<%s>" % (WSTR, WSTR, WSTR, WSTR), "wfind"),
    mk("std::__tree<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>::find<nocase_string>", "nfind"),
    mk("std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::seekpos", "seekpos"),
    mk("std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::seekoff", "seekoff"),
    mk("std::basic_istream<wchar_t,std::char_traits<wchar_t>>::sentry::~sentry", "istream"),
    mk("std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::iterator::iterator", "itercopy"),
]
