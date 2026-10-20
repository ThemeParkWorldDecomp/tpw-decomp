"""Unit v51127, small bullfrog functions, against libtpw_unit_LbApp_SetWindowIcon.dylib (tests/unit_LbApp_SetWindowIcon.cpp), bullfrog
(PROGRAM "bullfrog symbols shared"), src/LbFile_LbApp_GetWindowsScreenMode_LbApp_SetWindowIcon.cpp:
  LbApp_SetWindowIcon 0x10014238, Get/SetUserWndProc 0x10014170/0x10014138, Get/SetCloseDownRequest 0x100140c0/0x100140fc,
  Get/SetHWnd 0x10013fb4/0x10013fe4, Get/SetHInstance 0x10013f44/0x10013f7c, TbPtrList<_LbSEFunction>::TbPtrList 0x100144f0,
  TbTList<_LbSEFunction*,_LbSEFunction*>::~TbTList 0x1001491c, RemoveAll 0x100149c8, RemoveHead 0x10014a78, CConverter::~CConverter
  0x10019358, RotateLeft<unsigned long> 0x10019f3c, USwapInplace32<unsigned long> 0x10009d44 and 0x100445f0, USwapBlock32<unsigned long>
  0x10009ce4 and 0x10044590, TbJPEGLoader::TbJPEGLoader 0x10024228 and ~TbJPEGLoader 0x10024290, _LbSemaLock::_LbSemaLock 0x10024ac4 and
  Release 0x10024b9c, TbPoint::TbPoint 0x100591c4.

A case is a world and a call, built twice by ONE piece of Python code (emulator memory, big endian, 4 byte pointers; host memory, little
endian, 8 byte pointers: the list code takes the pointer size from the side). The callees outside the unit are fakes on both sides that
record their events: operator delete (the node or object label), the TbTList constructor (it zeroes the three list members), Release, the
loader base constructor and destructor (with its flag), and _USwapBlock32 (block label and count). The Mac stores a vtable pointer where the
host object has its own: neither is compared.

The world of a case: the five application globals (random), an object block (labelled 1), up to four list nodes (labels 10..13), a
semaphore word (label 2) and a result word. What is compared: the answer (this as a label), the globals, the list members and nodes (pointers
as labels), the words of the object, the result word and the event log. Not tested: a NULL list for RemoveAll and RemoveHead (read
unconditionally).
No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbApp_SetWindowIcon"
MASK = 0xFFFFFFFF

ADDR = dict(icon=0x10073dfa, wndproc=0x10073dfc, hwnd=0x10073e00, hinst=0x10073e04, close=0x10073e08)
HOST_LAYOUT = [8, 16, 24, 8, 16, 24]
P = "tpw_lbapp_"


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_tblist_ctor(e):
    this = e.r(3)
    e.cl.rec("TbTList", (e.cl.lab(this),))
    e.write(this + 4, bytes(12))


def stub_release(e):
    e.cl.rec("Release", (e.cl.lab(e.r(3)),))


def stub_base_ctor(e):
    e.cl.rec("BaseCtor", (e.cl.lab(e.r(3)),))


def stub_base_dtor(e):
    e.cl.rec("BaseDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_swap_block(e):
    e.cl.rec("USwapBlock32", (e.cl.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(0x1006b2d4, stub_delete)
STUBS.internal(0x100148b8, stub_tblist_ctor)
STUBS.internal(0x10016ff4, stub_release)
STUBS.internal(0x10022870, stub_base_ctor)
STUBS.internal(0x100228e0, stub_base_dtor)
STUBS.internal(0x1006b73c, stub_swap_block)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def init_world(self):
        """Allocations first (labels need the addresses); the subclass makes the log after."""
        self.obj = self.alloc(0x440)
        self.sema = self.alloc(16)
        self.res = self.alloc(16)
        self.nodes = [self.alloc(32) for _ in range(4)]
        self.tags = {self.obj: 1, self.sema: 2}
        for i, n in enumerate(self.nodes):
            self.tags[n] = 10 + i

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v & ((1 << (8 * self.psz)) - 1)))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def tag(self, v):
        if v == 0:
            return 0
        return self.tags.get(v, v | (1 << 40))

    def g(self, name):
        return ADDR[name]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.alloc = e.alloc
        self.init_world()
        fixed = dict(self.tags)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, spec, args):
        r3, _, _ = T.call_ppc(self.e, spec["addr"], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.alloc = getattr(lib, P + "alloc")
        self.init_world()
        CL.host_begin(lib, {label: addr for addr, label in self.tags.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, spec, args):
        fn = getattr(self.lib, P + spec["fn"])
        out = []
        for code, a in zip(spec["sig"].split(">")[0], args):
            out.append(s32(a) if code == "i" else a & MASK if code == "u" else a)
        r = fn(*out)
        return None if r is None else r & MASK


CT = {"u": C.c_uint32, "i": C.c_int32, "p": C.c_uint64, "n": None}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    for spec in OPS.values():
        f = getattr(lib, P + spec["fn"])
        a, r = spec["sig"].split(">")
        f.argtypes = [CT[c] for c in a]
        f.restype = CT[r]
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the operations
# ppc / host: argument tokens (obj, sema, res, $k = case args[k]); sig: host argument codes > answer code; kind: what the world holds.


def op(addr, fn, ppc, host, sig, kind, **kw):
    d = dict(addr=addr, fn=fn, ppc=ppc, host=host, sig=sig, kind=kind, this=False, k=0, edges=[[]], rnd=lambda r: [])
    d.update(kw)
    return d


VALS = [0, 1, MASK, 0x80000000, 0x7FFFFFFF, 0x1000, 0x12345678]
FLAGS = [0, 1, 2, MASK, 0x7FFF, 0x8000, 0x10000, 0x18000, 0xFFFF, 0xFFFF8000, 5]


def rv(r):
    return r.choice(VALS + [r.getrandbits(32)] * 4)


def one_val(r):
    return [rv(r)]


def flag_rnd(r):
    return [r.choice(FLAGS + [r.getrandbits(32)] * 3)]


def two_val(r):
    return [rv(r), rv(r)]


OPS = {
    "LbApp_SetWindowIcon": op(0x10014238, "SetWindowIcon", ["res", "$0"], ["$0", "res"], "ip>n", "res",
                              edges=[[0], [1], [0x7FFF], [0x8000], [0xFFFF], [0x10000], [0x12345678], [MASK], [0xFFFF8000]], rnd=one_val),
    "LbApp_GetUserWndProc": op(0x10014170, "GetUserWndProc", [], [], ">u", "none"),
    "LbApp_SetUserWndProc": op(0x10014138, "SetUserWndProc", ["$0"], ["$0"], "u>n", "none", edges=[[v] for v in VALS], rnd=one_val),
    "LbApp_SetCloseDownRequest": op(0x100140fc, "SetCloseDownRequest", ["$0"], ["$0"], "i>n", "none", edges=[[v] for v in VALS], rnd=one_val),
    "LbApp_GetCloseDownRequest": op(0x100140c0, "GetCloseDownRequest", [], [], ">i", "none"),
    "LbApp_SetHWnd": op(0x10013fe4, "SetHWnd", ["$0"], ["$0"], "u>n", "none", edges=[[v] for v in VALS], rnd=one_val),
    "LbApp_GetHWnd": op(0x10013fb4, "GetHWnd", [], [], ">u", "none"),
    "LbApp_SetHInstance": op(0x10013f7c, "SetHInstance", ["$0"], ["$0"], "u>n", "none", edges=[[v] for v in VALS], rnd=one_val),
    "LbApp_GetHInstance": op(0x10013f44, "GetHInstance", [], [], ">u", "none"),
    "TbPtrList<_LbSEFunction>::TbPtrList": op(0x100144f0, "PtrListCtor", ["obj"], ["obj"], "p>u", "list", this=True, ctor=True),
    "TbTList<_LbSEFunction*,_LbSEFunction*>::~TbTList": op(0x1001491c, "ListDtor", ["obj", "$0"], ["obj", "$0"], "pi>u", "list", this=True,
                                                          edges=[[f] for f in FLAGS], rnd=flag_rnd, nullable=True),
    "TbTList<_LbSEFunction*,_LbSEFunction*>::RemoveAll": op(0x100149c8, "RemoveAll", ["obj"], ["obj"], "p>n", "list"),
    "TbTList<_LbSEFunction*,_LbSEFunction*>::RemoveHead": op(0x10014a78, "RemoveHead", ["obj"], ["obj"], "p>u", "list"),
    "CConverter::~CConverter": op(0x10019358, "ConverterDtor", ["obj", "$0"], ["obj", "$0"], "pi>u", "none", this=True,
                                  edges=[[f] for f in FLAGS], rnd=flag_rnd, nullable=True),
    "SamsUtilities::UBinary::RotateLeft<unsigned_long>": op(
        0x10019f3c, "RotateLeft", ["$0", "$1"], ["$0", "$1"], "ui>u", "none",
        edges=[[v, n] for v in (0, 1, 0x80000000, MASK, 0x12345678) for n in (0, 1, 31, 32, 33, 0xFFFFFFFF, 0x80000000, 5)], rnd=two_val),
    "SamsUtilities::USwapInplace32<unsigned_long>@0x10009d44": op(0x10009d44, "Inplace_10009d44", ["obj"], ["obj"], "p>n", "words", k=1),
    "SamsUtilities::USwapInplace32<unsigned_long>@0x100445f0": op(0x100445f0, "Inplace_100445f0", ["obj"], ["obj"], "p>n", "words", k=1),
    "SamsUtilities::USwapBlock32<unsigned_long>@0x10009ce4": op(0x10009ce4, "Block_10009ce4", ["obj", "$0"], ["obj", "$0"], "pi>n", "words", k=4,
                                                                edges=[[v] for v in VALS], rnd=one_val),
    "SamsUtilities::USwapBlock32<unsigned_long>@0x10044590": op(0x10044590, "Block_10044590", ["obj", "$0"], ["obj", "$0"], "pi>n", "words", k=4,
                                                                edges=[[v] for v in VALS], rnd=one_val),
    "TbJPEGLoader::TbJPEGLoader": op(0x10024228, "JpegCtor", ["obj"], ["obj"], "p>u", "none", this=True),
    "TbJPEGLoader::~TbJPEGLoader": op(0x10024290, "JpegDtor", ["obj", "$0"], ["obj", "$0"], "pi>u", "none", this=True,
                                      edges=[[f] for f in FLAGS], rnd=flag_rnd, nullable=True),
    "_LbSemaLock::_LbSemaLock": op(0x10024ac4, "SemaCtor", ["obj", "sema", "$0"], ["obj", "sema", "$0"], "ppi>n", "words", k=3,
                                   edges=[[v] for v in VALS], rnd=one_val),
    "_LbSemaLock::Release": op(0x10024b9c, "SemaRelease", ["obj"], ["obj"], "p>n", "words", k=3),
    "TbPoint::TbPoint": op(0x100591c4, "PointCtor", ["obj", "$0", "$1"], ["obj", "$0", "$1"], "pii>n", "words", k=2,
                           edges=[[x, y] for x in VALS for y in (0, MASK, 7)], rnd=two_val),
}


STUBS.real.update(spec["addr"] for spec in OPS.values())


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    spec = OPS[case["op"]]
    s.half(s.g("icon"), case["g"][0])
    for name, v in zip(("wndproc", "hwnd", "hinst", "close"), case["g"][1:]):
        s.words(s.g(name), [v])
    kind = spec["kind"]
    if kind == "res":
        s.words(s.res, [0x12345678])
    elif kind == "words":
        s.words(s.obj, case["words"])
        s.words(s.sema, [case["sema"]])
    elif kind == "list":
        n = case["n"]
        ps = s.psz
        for i in range(4):
            node = s.nodes[i]
            s.ptr(node, case["data"][i])
            s.ptr(node + ps, s.nodes[i + 1] if i + 1 < n else 0)
            s.ptr(node + 2 * ps, s.nodes[i - 1] if 0 < i < n else 0)
        head = s.nodes[0] if n else case["h0"]
        tail = case["tail0"] if case["tail0"] is not None else (s.nodes[n - 1] if n else 0)
        s.ptr(s.obj + ps, head)
        s.ptr(s.obj + 2 * ps, tail)
        s.words(s.obj + 3 * ps, [case["count"]])


def execute(s, case):
    spec = OPS[case["op"]]
    obj = 0 if case.get("null") else s.obj

    def tok(t):
        if t == "obj":
            return obj
        if t == "sema":
            return s.sema
        if t == "res":
            return s.res
        return case["args"][int(t[1:])]

    return s.call(spec, [tok(t) for t in spec["ppc" if isinstance(s, Ppc) else "host"]])


def observe(s, case, ans):
    spec = OPS[case["op"]]
    out = {"log": s.log(), "icon": s.rd_half(s.g("icon"))}
    for name in ("wndproc", "hwnd", "hinst", "close"):
        out[name] = s.rd_words(s.g(name), 1)
    if spec["sig"].endswith("n"):
        out["ans"] = None
    elif spec["this"]:
        out["ans"] = "this" if ans == (0 if case.get("null") else s.obj & MASK) else ans
    else:
        out["ans"] = ans
    kind = spec["kind"]
    if kind == "res":
        out["res"] = s.rd_words(s.res, 1)
    elif kind == "words":
        out["words"] = s.rd_words(s.obj, spec["k"])
        out["sema"] = s.rd_words(s.sema, 1)
    elif kind == "list" and not case.get("null"):
        ps = s.psz
        out["head"] = s.tag(s.rd_ptr(s.obj + ps))
        out["tail"] = s.tag(s.rd_ptr(s.obj + 2 * ps))
        out["count"] = s.rd_words(s.obj + 3 * ps, 1)
        out["nodes"] = [(s.rd_ptr(n) & MASK, s.tag(s.rd_ptr(n + ps)), s.tag(s.rd_ptr(n + 2 * ps))) for n in s.nodes]
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

LIST_CONFIGS = [(0, 0, None), (0, 3, 0x1234), (1, 1, None), (1, 1, 0x4321), (2, 2, None), (3, 3, None), (4, 4, None), (3, 0, None),
                (2, 0x80000000, None), (1, 0, None)]


def make(r, name, args, **kw):
    spec = OPS[name]
    c = {"op": name, "queues": {}, "args": args, "g": [r.getrandbits(16)] + [rv(r) for _ in range(4)]}
    kind = spec["kind"]
    if kind == "words":
        c["words"] = [rv(r) for _ in range(spec["k"])]
        c["sema"] = rv(r)
    elif kind == "list":
        c.update(n=0, count=0, tail0=None, h0=rv(r) if spec.get("ctor") else 0, data=[rv(r) | 1 for _ in range(4)])
        if spec.get("ctor"):
            c.update(count=rv(r), tail0=rv(r))
    c.update(kw)
    return c


def edges_for(name):
    spec = OPS[name]

    def gen():
        r = random.Random(sum(map(ord, name)))
        for args in spec["edges"]:
            if spec["kind"] == "list" and not spec.get("ctor"):
                for n, count, tail0 in LIST_CONFIGS:
                    yield make(r, name, args, n=n, count=count, tail0=tail0)
                if spec.get("nullable"):
                    yield make(r, name, args, null=True)
            else:
                yield make(r, name, args)
                if spec.get("nullable"):
                    yield make(r, name, args, null=True)

    return gen


def random_for(name):
    spec = OPS[name]

    def gen(r):
        args = spec["rnd"](r)
        if spec["kind"] == "list" and not spec.get("ctor"):
            n = r.randrange(0, 5)
            count = r.choice([n, r.getrandbits(32), 0, n + 1])
            tail0 = r.choice([None, None, r.getrandbits(32)])
            if spec.get("nullable") and r.random() < 0.1:
                return make(r, name, args, null=True)
            return make(r, name, args, n=n, count=count, tail0=tail0)
        return make(r, name, args)

    return gen


TESTS = [FnTest(name, ppc, host, edge=edges_for(name), rnd=random_for(name)) for name in OPS]
