"""Unit v5944, std::basic_filebuf<wchar_t>::basic_filebuf(FILE*) 0x10226ef4, std::basic_filebuf<wchar_t>::open 0x10221c30 and
std::__tree<pair<const nocase_string, BuddyInfo>, ...>::insert_node_at 0x102273bc, against libtpw_unit_std_basic_filebuf_basic_filebuf_3.dylib
(tests/unit_std_basic_filebuf_basic_filebuf_3.cpp), main app, src/game/MslSupport.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, for the original; host
arena memory for the translation); the same code reads both back. The callees outside the unit (locale ctor/dtor, getloc, use_facet, the
codecvt virtual call, fopen/fseek/fclose, delete[], unshift, strlen, new, new[], strcpy, __throw, the pair copy constructor and
balance_insert) are fakes on both sides that record their arguments and answer from the case's queues; the fakes that fill memory (locale,
pair copy, strcpy) write the same values on both sides.

The world of a case: a filebuf object (random words), a codecvt object, a tree header with its parent and new node blocks. What is
compared: the answer (as 'is the object'), every word of the objects read back, and the event log.

No fused operations. Not tested: nothing skipped; the stack words the original passes as the second argument of use_facet are unread.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_basic_filebuf_3"
MASK = 0xFFFFFFFF

FN_CTOR, FN_OPEN, FN_INS = 0x10226EF4, 0x10221C30, 0x102273BC
SIZES = {FN_CTOR: 272, FN_OPEN: 396, FN_INS: 536}
OWN = set(SIZES)

ADDR = dict(vtblC=0x102B1E60, vtblD=0x102B1EB0)
HOST_LAYOUT = [0x24, 0x2C, 0x30, 0x4C, 0x4D, 32, 24, 8, 16, 24, 32]
CVT_HANDLE = 0x5000      # the host stores the codecvt in a 32-bit word: an opaque handle (the arena is not 32-bit addressable)

LEN_VTBL, LEN_TV = 0x10278B74, 0x10265680

CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def pk(e, a, *ws):
    e.write(a, b"".join(struct.pack(">I", w & MASK) for w in ws))


def f_locale_ctor(e, log):
    pk(e, e.r(3), log.ret("LocA"), log.ret("LocB"))
    return e.r(3)


def f_getloc(e, log):
    pk(e, e.r(3), log.ret("GetlocA"), log.ret("GetlocB"))
    return e.r(3)


def f_usefacet(e, log):
    log.rec("UseFacetW", (e.u32(e.r(3)), e.u32(e.r(3) + 4)))
    return CUR["cvt"]


def f_locale_dtor(e, log):
    log.rec("LocaleDtorW", (e.u32(e.r(3)), e.u32(e.r(3) + 4)))
    return 0


def f_strlen(e, log):
    return len(e.cstr(e.r(3)))


def f_newarray(e, log):
    return 0 if log.ret("ArrNull") else CUR["arr"]


def f_new(e, log):
    if e.r(3) == 0x14:
        return CUR["node"]
    return 0 if log.ret("RefNull") else CUR["ref"]


def f_strcpy(e, log):
    if e.r(3):
        e.write(e.r(3), e.cstr(e.r(4)).encode("latin1") + b"\0")
    return e.r(3)


def f_throw(e, log):
    obj = e.r(4)
    vtbl, data, refs = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    dok = (1 if e.cstr(data) == "tree::insert length error" else 0) if data else 2
    log.rec("ThrowObj", (int(vtbl == LEN_VTBL), int(e.r(5) == LEN_TV), log.lab(data), log.lab(refs), s32(e.u32(refs)) if refs else -1, dok))
    return 0


def f_pair(e, log):
    pk(e, e.r(3), log.ret("PairA"), log.ret("PairB"))
    return e.r(3)


def R(name, spec, ret=None):
    """A recorder that also saves the TOC pointer where the glue of an import saves it (the caller reloads r2 from 0x14(r1))."""
    rec = CL.recorder(name, spec, ret=ret)

    def fn(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return rec(e)
    return fn


STUBS.internal(0x10072D24, R("LocaleCtor", "L", ret=f_locale_ctor))
STUBS.internal(0x10220AB4, R("Getloc", "LL", ret=f_getloc))
STUBS.internal(0x102298E4, R("UseFacet", "", ret=f_usefacet))
STUBS.internal(0x10071F34, R("LocaleDtor", "xi", ret=f_locale_dtor))
STUBS.internal(0x1023BAA8, R("Noconv", "L", "i"))
STUBS.internal(0x102389B0, R("Fopen", "ss", "i"))
STUBS.internal(0x102388F0, R("Fseek", "uii", "i"))
STUBS.internal(0x102388C0, R("Fclose", "u", "i"))
STUBS.internal(0x10238638, R("DeleteArray", "u"))
STUBS.internal(0x10226DCC, R("Unshift", "L", "b"))
STUBS.internal(0x10238650, R("Strlen", "s", ret=f_strlen))
STUBS.internal(0x10238668, R("NewArray", "u", ret=f_newarray))
STUBS.internal(0x102384B8, R("New", "u", ret=f_new))
STUBS.internal(0x10238410, R("Strcpy", "Ls", ret=f_strcpy))
STUBS.internal(0x10238680, R("Throw", "s", ret=f_throw))
STUBS.internal(0x10227840, R("PairCtor", "LL", ret=f_pair))
STUBS.internal(0x10153B24, R("Balance", "LL"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    tab = {0x10221C8C: 0, 0x10221C94: 1, 0x10221C9C: 2, 0x10221CA4: 3, 0x10221CAC: 4, 0x10221CB4: 5, 0x10221CBC: 6, 0x10221CC4: 7,
           0x10221CCC: 8, 0x10221CD4: 9}
    print("MODETABLE", [tab.get(e.u32(0x102B1FB4 + 4 * i), -1) for i in range(0x35)])


# ---------------------------------------------------------------------------------------------------------------- the two sides

LABELS = {
    "ctor": [("obj", 0, 1), ("obj", 0x1C, 2), ("cvt", 0, 3)],
    "open": [("obj", 0, 1)],
    "ins": [("tree", 0, 1), ("parent", 0, 2), ("node", 0, 3), ("node", "PO", 4), ("ref", 0, 5), ("arr", 0, 6), ("pair", 0, 7), ("root", 0, 8)],
}
BLOCKS = {
    "ctor": [("obj", 0x60), ("cvt", 8), ("vt", 0x40)],
    "open": [("obj", 0x60), ("name", 16)],
    "ins": [("tree", 0x40), ("parent", 0x40), ("node", 0x40), ("ref", 8), ("arr", 64), ("pair", 8), ("root", 0x20)],
}


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptrs(self, a, ws):
        """Pointer-sized slots: 4 bytes on the Mac, 8 on the host."""
        self.write(a, b"".join(struct.pack(self.pfmt, w) for w in ws))

    def rd_ptrs(self, a, n):
        return list(struct.unpack(self.pfmt[0] + "%d%s" % (n, self.pfmt[1]), self.read(a, self.ps * n)))

    def blocks(self, case):
        self.b = {name: self.alloc(size) for name, size in BLOCKS[case["op"]]}
        fixed = {}
        for n, off, lab in LABELS[case["op"]]:
            a = self.b[n] + (self.po if off == "PO" else off)
            if n == "cvt" and self.cvt_handle:
                a = self.cvt_handle
            fixed[a] = lab
        return fixed


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    ps = 4
    po = 0xC
    cvt_handle = None

    def __init__(self, e, case):
        self.e = e
        fixed = self.blocks(case)
        CUR.clear()
        CUR.update(self.b)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def ctor(self, b, file):
        return self.call(FN_CTOR, [b["obj"], file])

    def open(self, b, mode):
        return self.call(FN_OPEN, [b["obj"], b["name"], mode])

    def ins(self, b, left, lm):
        return self.call(FN_INS, [b["tree"], b["parent"], left, lm, b["pair"]])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    ps = 8
    po = 24
    cvt_handle = CVT_HANDLE

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = self.blocks(case)
        CL.host_begin(lib, {lab: a for a, lab in fixed.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fbw3_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fbw3_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self, b, file):
        return self.lib.tpw_fbw3_ctor(b["obj"], file & MASK, CVT_HANDLE) & MASK

    def open(self, b, mode):
        return self.lib.tpw_fbw3_open(b["obj"], b["name"], mode & MASK) & MASK

    def ins(self, b, left, lm):
        return self.lib.tpw_fbw3_insert(b["tree"], b["parent"], left & MASK, lm & MASK, b["pair"], b["node"], b["ref"], b["arr"]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fbw3_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    u64, u32 = C.c_uint64, C.c_uint32
    getattr(lib, p + "ctor").restype, getattr(lib, p + "ctor").argtypes = u32, [u64, u32, u64]
    getattr(lib, p + "open").restype, getattr(lib, p + "open").argtypes = u32, [u64, u64, u32]
    getattr(lib, p + "insert").restype, getattr(lib, p + "insert").argtypes = u32, [u64, u64, u32, u32, u64, u64, u64, u64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    b, op = s.b, case["op"]
    if op in ("ctor", "open"):
        s.words(b["obj"], case["obj"])
        s.write(b["obj"] + 0x4C, bytes(case["tail"]))
    if op == "ctor":
        s.words(b["cvt"], [b["vt"]])
        s.words(b["vt"], case["vt"])
    elif op == "open":
        s.write(b["name"], case["name"] + b"\0")
    else:
        root = b["root"] if case["root"] else 0
        s.words(b["tree"], [case["count"]])
        s.ptrs(b["tree"] + s.ps, [root, case["w8"], case["w12"]])
        s.ptrs(b["parent"], case["parent"])
        s.ptrs(b["node"], case["node"])
        s.words(b["pair"], case["src"])
        s.words(b["root"], case["rootw"])
        s.words(b["ref"], case["refw"])


def execute(s, case):
    op, b = case["op"], s.b
    if op == "ctor":
        return s.ctor(b, case["file"])
    if op == "open":
        return s.open(b, case["mode"])
    return s.ins(b, case["left"], case["lm"])


def observe(s, case, answer):
    b, op = s.b, case["op"]
    res = {"log": s.log()}
    if op in ("ctor", "open"):
        obj = b["obj"]
        w = s.rd_words(obj, 19)
        buf = (obj + 0x30) & MASK
        if op == "ctor":
            res["vt"] = int(w[0] == (s.g("vtblD") & MASK))
            res["cvt"] = int(w[11] == (s.cvt_handle or b["cvt"]) & MASK)
            w = [int(x == buf) * 0xA5A5A5A5 or x for x in w]
            res["words"] = w[1:11] + w[12:]
        else:
            res["words"] = [int(x == buf) * 0xA5A5A5A5 or x for x in w]
        res["tail"] = list(s.read(obj + 0x4C, 4))
        res["answer"] = int(answer == obj & MASK)
    else:
        nm = {b["node"]: 0xA1000001, b["parent"]: 0xA1000002, b["root"]: 0xA1000003, b["pair"]: 0xA1000004}
        nz = lambda x: nm.get(x, x)
        res["tree"] = s.rd_words(b["tree"], 1) + [nz(x) for x in s.rd_ptrs(b["tree"] + s.ps, 3)]
        res["parent"] = [nz(x) for x in s.rd_ptrs(b["parent"], 4)]
        nd = s.rd_ptrs(b["node"], 3)
        pw = nd[2]
        res["node"] = [nz(x) for x in nd[:2]] + [0xA2000000 | (pw & 1) if (pw & ~1) == b["parent"] else pw]
        res["pair"] = s.rd_words(b["node"] + s.po, 2)
        res["arr"] = list(s.read(b["arr"], 64))
        res["ref"] = s.rd_words(b["ref"], 2)
        res["answer"] = int(answer == b["node"] & MASK)
    return res


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

W = [0, 1, MASK, 0x7FFFFFFF, 0x80000000, 0x1234, 0x5000]


def rw(r):
    return r.choice(W) if r.random() < 0.3 else r.getrandbits(32)


def queues(r):
    return {
        "LocA": [rw(r)], "LocB": [rw(r)], "GetlocA": [rw(r)], "GetlocB": [rw(r)],
        "Noconv": [r.choice([0, 1, 0x100, 0x1FF, 0xFF, -1, r.getrandbits(32)])],
        "Fopen": [r.choice([0, 0x1000, rw(r)])], "Fseek": [r.choice([0, 1, -1])], "Fclose": [r.choice([0, 1, -1])],
        "Unshift": [r.choice([0, 1])],
        "ArrNull": [int(r.random() < 0.2)], "RefNull": [int(r.random() < 0.2)],
        "PairA": [rw(r)], "PairB": [rw(r)],
    }


def ctor_case(r, file=None):
    return {"op": "ctor", "queues": queues(r), "file": rw(r) if file is None else file, "obj": [rw(r) for _ in range(19)],
            "tail": [r.getrandbits(8) for _ in range(4)], "vt": [rw(r) for _ in range(16)]}


def open_case(r, mode=None, filew=None, flag=None):
    c = {"op": "open", "queues": queues(r), "obj": [rw(r) for _ in range(19)], "mode": r.getrandbits(7) if mode is None else mode,
         "name": r.choice([b"a.txt", b"x", b"some/long/path.dat"]), "tail": [r.choice([0, 1, 0x80]), r.getrandbits(8), 0, 0]}
    c["obj"][9] = (0 if r.random() < 0.8 else rw(r)) if filew is None else filew
    if flag is not None:
        c["tail"][0] = flag
    return c


def ins_case(r, count=None):
    return {"op": "ins", "queues": queues(r), "count": r.choice([0, 1, 5, 0xFFFFFFFE, 0xFFFFFFFF, rw(r)]) if count is None else count,
            "root": r.choice([0, 1]), "w8": rw(r), "w12": rw(r), "parent": [r.choice([0, rw(r)]) for _ in range(4)],
            "node": [rw(r) for _ in range(3)], "src": [rw(r), rw(r)], "rootw": [rw(r) for _ in range(8)],
            "refw": [rw(r), rw(r)], "left": r.choice([0, 1, 0x100, 0x101, MASK]), "lm": r.choice([0, 1, 0x100, 0x101, MASK])}


def ctor_edges():
    r = random.Random(5944)
    for f in (0, 1, MASK, 0x1000):
        yield ctor_case(r, f)


def open_edges():
    r = random.Random(5945)
    for mode in range(0x45):
        yield open_case(r, mode, 0)
    for mode in (0x08, 0x0A, 0x10, 0x12, 0x3C, 0x3E):
        for fl in (0, 1):
            for fs in (0, 1):
                c = open_case(r, mode, 0, fl)
                c["queues"]["Fopen"] = [0x1000]
                c["queues"]["Fseek"] = [fs]
                c["queues"]["Unshift"] = [1 - fl]
                yield c
                c = open_case(r, mode, 0, fl)
                c["queues"]["Fopen"] = [0x1000]
                c["queues"]["Fseek"] = [1]
                c["queues"]["Unshift"] = [fl]
                c["queues"]["Fclose"] = [fs]
                yield c
    yield open_case(r, 8, 0x1000)


def ins_edges():
    r = random.Random(5946)
    for count in (0, 1, 0xFFFFFFFD, 0xFFFFFFFE, 0xFFFFFFFF):
        for ar in (0, 1):
            for rn in (0, 1):
                c = ins_case(r, count)
                c["queues"]["ArrNull"] = [ar]
                c["queues"]["RefNull"] = [rn]
                yield c
    for left in (0, 1, 0x100):
        for lm in (0, 1, 0x100):
            c = ins_case(r, 3)
            c["left"], c["lm"] = left, lm
            yield c
    for w in (0, 1):
        c = ins_case(r, 3)
        c["node"][2] = w
        yield c


TESTS = [
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::basic_filebuf", ppc, host, edge=ctor_edges, rnd=ctor_case),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::open", ppc, host, edge=open_edges, rnd=open_case),
    FnTest("std::__tree<std::pair<const_nocase_string,BuddyInfo>,std::map<nocase_string,BuddyInfo,std::less<nocase_string>,"
           "std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::value_compare,std::allocator<std::pair<const_nocase_string,BuddyInfo>>>"
           "::insert_node_at", ppc, host, edge=ins_edges, rnd=ins_case),
]
