"""Unit v5141, CFSOnlineRide's constructor, against libtpw_unit_CFSOnlineRide_CFSOnlineRide.dylib
(tests/unit_CFSOnlineRide_CFSOnlineRide.cpp), main app ("SimTheme Park"), src/game/online/FS_OnlineList.cpp:
  CFSOnlineRide::CFSOnlineRide 0x1017ca7c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit: the string
constructor (0x10238740, three times, faked: records a new label and stores a vtable whose slot 0x20 is a function descriptor) and the
virtual assignment from a wide C string reached through __ptr_glue (the glue runs as the original; the descriptor's code is faked and
records the string's label and the text). The list's AddTail (0x1017dcd0) runs as the original on the PowerPC side and as the header's
template on the host, with operator new from the standard stubs.

The world of a case: a ride info (id, two strings, six words, a string, three words), a ride list holding 0 to 3 nodes, a zeroed ride
block. What is compared: the answer (this), the ride's copied members, the string events, the whole list (count, links, values, tail).

No fused operations. Not tested: a NULL string pointer in the info (the fake reads the text unconditionally); the vtable word the
original stores at offset 0 (the host's is its own).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineRide_CFSOnlineRide"
MASK = 0xFFFFFFFF

FN = 0x1017CA7C
SIZES = {FN: 292}
OWN = set(SIZES)
ADDTAIL = 0x1017DCD0
ASSIGN_CODE = 0x1017DD60       # the fake string assignment's code address (never executed)
HOST_LAYOUT = [0x34, 4, 0x24, 0x30]

VT = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(ADDTAIL)

def after_ctor(e, *a):
    """The real glue saves the TOC at 20(r1) (the caller reloads r2 from there) and the constructor stores the vtable."""
    e.write(e.r(3), struct.pack(">I", VT["vt"]))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


STUBS.internal(0x10238740, CL.recorder("StrCtor", "N", then=after_ctor))
STUBS.internal(ASSIGN_CODE, CL.recorder("StrAssign", "LS"))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    tv = T.persist_alloc(e, 8, struct.pack(">II", ASSIGN_CODE, e.r(2) & MASK))
    vt = T.persist_alloc(e, 0x40, b"\0" * 0x20 + struct.pack(">I", tv) + b"\0" * 0x1C)
    VT["vt"] = vt


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wstr(self, text):
        data = b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text + "\0")
        a = self.alloc(len(data) + 4)
        self.write(a, data)
        return a


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

    def p32(self, a):
        return a

    def newlist(self, k):
        sysl = self.alloc(0x34)
        obj = self.alloc(0x10)
        nodes = [self.alloc(12) for _ in range(k)]
        for i, n in enumerate(nodes):
            self.words(n, [0x1000 + i, nodes[i + 1] if i + 1 < k else 0, nodes[i - 1] if i else 0])
        self.words(obj, [0, nodes[0] if k else 0, nodes[-1] if k else 0, k])
        self.words(sysl + 0x2c, [obj])
        return sysl

    def construct(self, info, lst):
        ride = self.alloc(0x6c)
        r3, _, _ = T.call_ppc(self.e, FN, [ride, info, lst])
        return ride, int((r3 & MASK) == ride)

    def read_ride(self, ride, lst):
        w = self.rd_words(ride, 0x6c // 4)
        return [int(w[1] == lst), w[2]] + w[0x34 // 4:0x4C // 4] + w[0x60 // 4:0x6C // 4]

    def read_list(self, lst, ride):
        obj = self.rd_word(lst + 0x2c)
        _, head, tail, count = self.rd_words(obj, 4)
        nodes = []
        p = head
        while p and len(nodes) < 8:
            nodes.append(p)
            p = self.rd_word(p + 4)

        def code(p):
            return nodes.index(p) + 1 if p in nodes else 0

        out = [count, len(nodes)]
        for p in nodes:
            d, nx, pv = self.rd_words(p, 3)
            out += [1 if d == ride else d, code(nx), code(pv)]
        return out + [code(tail)]


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rd_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, a):
        return a - self.lib.tpw_rd_base()

    def newlist(self, k):
        return self.lib.tpw_rd_newlist(k)

    def construct(self, info, lst):
        return self.lib.tpw_rd_ctor(info, lst), 1

    def read_ride(self, ride, lst):
        out = (C.c_int64 * 32)()
        n = self.lib.tpw_rd_read(ride, lst, out)
        return list(out)[:n]

    def read_list(self, lst, ride):
        out = (C.c_int64 * 32)()
        n = self.lib.tpw_rd_listread(lst, ride, out)
        return list(out)[:n]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name, res, args in (
        ("alloc", C.c_uint64, [C.c_uint32]),
        ("base", C.c_uint64, []),
        ("newlist", C.c_uint64, [C.c_int32]),
        ("ctor", C.c_uint64, [C.c_uint64, C.c_uint64]),
        ("read", C.c_int32, [C.c_uint64, C.c_uint64, C.POINTER(C.c_int64)]),
        ("listread", C.c_int32, [C.c_uint64, C.c_uint64, C.POINTER(C.c_int64)]),
        ("layout", C.c_int32, [C.POINTER(C.c_int64)]),
    ):
        f = getattr(lib, "tpw_rd_" + name)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_rd_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        lst = s.newlist(case["k"])
        sp = [s.wstr(t) for t in case["strs"]]
        info = s.alloc(0x34)
        w = case["words"]
        s.words(info, [w[0], s.p32(sp[0]), s.p32(sp[1])] + w[1:7] + [s.p32(sp[2])] + w[7:10])
        ride, ans = s.construct(info, lst)
        return {"answer": ans, "ride": s.read_ride(ride, lst), "list": s.read_list(lst, ride), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rword(r):
    return r.choice([0, 1, MASK, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32)])


def rstr(r):
    return "".join(r.choice("abcXYZ019 _") for _ in range(r.choice([0, 1, 2, 5, 12])))


def make(r, k=None):
    return {"queues": {}, "k": r.randrange(4) if k is None else k,
            "words": [rword(r) for _ in range(10)], "strs": [rstr(r) for _ in range(3)]}


def edges():
    r = random.Random(5141)
    for k in (0, 1, 2, 3):
        yield make(r, k)
    c = make(r, 0)
    c["strs"] = ["", "", ""]
    c["words"] = [0] * 10
    yield c
    c = make(r, 1)
    c["words"] = [MASK] * 10
    yield c


def rnd(r):
    return make(r)


TESTS = [
    FnTest("CFSOnlineRide::CFSOnlineRide", ppc, host, edge=edges, rnd=rnd),
]
