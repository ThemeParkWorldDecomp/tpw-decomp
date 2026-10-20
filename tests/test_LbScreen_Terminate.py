"""Unit v5737, the WinScrn terminate / message / mode enumeration functions, against libtpw_unit_LbScreen_Terminate.dylib
(tests/unit_LbScreen_Terminate.cpp), bullfrog, src/LbScreen_Initialise_LbScreen_Terminate.cpp:
  LbScreen_Terminate 0x10068b10, LbApp_ShowMessage 0x10068e54, LbScreen_GetDirectDraw 0x100687a8,
  LbScreen_ModeFindFirst 0x10068f70, LbScreen_ModeFindNext 0x10069024

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian with 4-byte pointers for the original,
host library memory little-endian with 8-byte pointers for the translation). Callees outside the unit (ReleaseMode, the DirectDraw
members, DestroyWindow, SetWindowPos, MessageBoxA, vsprintf's caller, operator delete, LbSurface_RegisterDirectDraw) are fakes on both
sides that record their arguments. LbList_GetFirst runs for real on both sides. vsprintf is the C library on the host and a Python
formatter on the PowerPC side (integer formats only).

The world: the globals (initialised flag, DirectDraw, window, cooperative window), the mode list at 0x10075128 with 0 to 3 display
modes, a find state and a mode record. Compared: the answer, the globals, the whole list and node pointers (as indices), the mode record,
the find cursor and the event log. No fused operations. Not tested: a NULL cursor for ModeFindNext, and so ModeFindFirst on an empty list (LbList_GetFirst answers NULL and the original reads
through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbScreen_Terminate"
MASK = 0xFFFFFFFF

FN_TERM, FN_SHOW, FN_GETDD, FN_FIRST, FN_NEXT = 0x10068B10, 0x10068E54, 0x100687A8, 0x10068F70, 0x10069024
SIZES = {FN_TERM: 232, FN_SHOW: 240, FN_GETDD: 72, FN_FIRST: 100, FN_NEXT: 72}
OWN = set(SIZES)

ADDR = dict(init=0x100750F0, dd=0x10075234, hwnd=0x10075238, coop=0x1007523C, list=0x10075128)
HOST_LAYOUT = [16, 16, 16, 24]

# dummy object pointers (never dereferenced) and their labels
WIN_COOP, DD_OBJ, HWND_OBJ = 0x5000, 0x6000, 0x7000
FIXED = {WIN_COOP: 1, DD_OBJ: 2, HWND_OBJ: 3}

FORMATS = {
    "hello": lambda a, b: "hello",
    "%d": lambda a, b: "%d" % T.s32(a),
    "%d and %d": lambda a, b: "%d and %d" % (T.s32(a), T.s32(b)),
    "%x/%u": lambda a, b: "%x/%u" % (a & MASK, b & MASK),
    "%5d|%-5d|": lambda a, b: "%5d|%-5d|" % (T.s32(a), T.s32(b)),
    "100%% %d": lambda a, b: "100%% %d" % T.s32(a) if False else "100%% %d".replace("%%", "%").replace("%d", str(T.s32(a))),
}
FMT_LIST = sorted(FORMATS)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_release_mode(e):
    e.cl.rec("ReleaseMode", (e.u32(ADDR["init"]),))


def stub_vsprintf(e):
    dst, fmt, ap = e.r(3), e.cstr(e.r(4)), e.r(5)
    text = FORMATS[fmt](e.u32(ap), e.u32(ap + 4))
    e.write(dst, text.encode() + b"\0")
    e.set_r(3, len(text))


def toc(fn):
    """The glue saves the caller's TOC at 0x14(r1) and the caller reloads it afterwards; the fake does the saving."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return wrapped


def internal(addr, fn):
    STUBS.internal(addr, toc(fn))


STUBS.internal = lambda addr, fn, _orig=STUBS.internal: _orig(addr, toc(fn))
STUBS.internal(0x100689AC, stub_release_mode)
STUBS.internal(0x1006BE14, CL.recorder("RestoreDisplayMode", "L"))
STUBS.internal(0x1006BD0C, CL.recorder("Release", "L"))
STUBS.internal(0x1006BC4C, CL.recorder("AddRef", "L"))
STUBS.internal(0x10066FC0, CL.recorder("RegisterDirectDraw", "L"))
STUBS.internal(0x1006C06C, CL.recorder("DestroyWindow", "L"))
STUBS.internal(0x1006B2D4, CL.recorder("delete", "L"))
STUBS.internal(0x1006BE44, CL.recorder("FlipToGDISurface", "L"))
STUBS.internal(0x1006BE5C, CL.recorder("SetWindowPos", "Liiiiiu"))
STUBS.internal(0x1006BE74, CL.recorder("MessageBoxA", "Lssu"))
STUBS.internal(0x1006B91C, stub_vsprintf)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def m(self, v):
        """A pointer as a name: list/tail/node index, else the raw value."""
        return self.names.get(v, v)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    cur_off = 0x0C

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog(FIXED, case["queues"])
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
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK

    def term(self):
        self.call(FN_TERM, [])

    def show(self, fmt, a, b):
        self.call(FN_SHOW, [fmt, a & MASK, b & MASK])

    def getdd(self, n):
        return self.call(FN_GETDD, [n & MASK])

    def first(self):
        self.call(FN_FIRST, [self.res, self.find, self.out])
        return self.rd_word(self.res)

    def next(self):
        self.call(FN_NEXT, [self.res, self.find, self.out])
        return self.rd_word(self.res)

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    cur_off = 16

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {v: k for k, v in FIXED.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ws_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ws_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def term(self):
        self.lib.tpw_ws_term()

    def show(self, fmt, a, b):
        self.lib.tpw_ws_show(fmt, T.s32(a), T.s32(b))

    def getdd(self, n):
        return self.lib.tpw_ws_getdd(T.s32(n)) & MASK

    def first(self):
        code = self.lib.tpw_ws_first(self.find, self.out) & MASK
        self.words(self.res, [code])
        return code

    def next(self):
        code = self.lib.tpw_ws_next(self.find, self.out) & MASK
        self.words(self.res, [code])
        return code


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ws_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ws_term.restype, lib.tpw_ws_term.argtypes = None, []
    lib.tpw_ws_show.restype, lib.tpw_ws_show.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_ws_getdd.restype, lib.tpw_ws_getdd.argtypes = C.c_uint64, [C.c_int32]
    for n in ("first", "next"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("init"), [case["init"]])
    s.ptr(s.g("dd"), case["dd"])
    s.ptr(s.g("hwnd"), case["hwnd"])
    s.ptr(s.g("coop"), case["coop"])
    p, L = s.psz, s.g("list")
    modes = case["modes"]
    s.nodes = [s.alloc(0x40) for _ in modes]
    tail = L + p
    s.names = {L: "list", tail: "tail"}
    for i, a in enumerate(s.nodes):
        s.names[a] = i
    for i, (a, mode) in enumerate(zip(s.nodes, modes)):
        s.ptr(a, s.nodes[i + 1] if i + 1 < len(modes) else tail)
        s.ptr(a + p, s.nodes[i - 1] if i else L)
        s.words(a + 2 * p, mode)
    s.ptr(L, s.nodes[0] if modes else tail)
    s.ptr(L + p, 0)
    s.ptr(L + 2 * p, s.nodes[-1] if modes else L)
    s.find, s.out, s.res = s.alloc(0x40), s.alloc(16), s.alloc(8)
    s.words(s.out, [0x11111111, 0x22222222, 0x33333333])
    s.words(s.res, [0x12345678])
    cur = case["cur"]
    s.ptr(s.find + s.cur_off, 0x1234 if cur is None else (s.nodes[cur] if cur < len(modes) else tail))
    s.fmt_addr = s.alloc(64)
    s.write(s.fmt_addr, case["fmt"].encode() + b"\0")


def execute(s, case):
    op = case["op"]
    if op == "term":
        s.term()
    elif op == "show":
        s.show(s.fmt_addr, case["a"], case["b"])
    elif op == "getdd":
        return s.getdd(case["addref"])
    elif op == "first":
        return s.first()
    else:
        return s.next()
    return 0


def observe(s, case, answer):
    p, L = s.psz, s.g("list")
    nodes = [(s.m(s.rd_ptr(a)), s.m(s.rd_ptr(a + p)), s.rd_words(a + 2 * p, 3)) for a in s.nodes]
    return {
        "answer": answer, "log": s.log(),
        "init": s.rd_words(s.g("init"), 1),
        "dd": s.rd_ptr(s.g("dd")), "hwnd": s.rd_ptr(s.g("hwnd")), "coop": s.rd_ptr(s.g("coop")),
        "list": [s.m(s.rd_ptr(L)), s.rd_ptr(L + p), s.m(s.rd_ptr(L + 2 * p))],
        "nodes": nodes,
        "cursor": s.m(s.rd_ptr(s.find + s.cur_off)),
        "out": s.rd_words(s.out, 3),
        "res": s.rd_words(s.res, 1),
    }


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


def rword(r):
    return r.choice([0, 1, 640, 800, 1024, 16, 32, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(1 << 32)])


def base(r, op, nmodes=None, init=None, dd=None, hwnd=None, coop=None):
    n = r.randrange(4) if nmodes is None else nmodes
    c = {"op": op, "queues": {}}
    c["init"] = r.choice([0, 1, 2, 0xFFFFFFFF]) if init is None else init
    c["dd"] = r.choice([0, DD_OBJ]) if dd is None else dd
    c["hwnd"] = r.choice([0, HWND_OBJ]) if hwnd is None else hwnd
    c["coop"] = r.choice([0, WIN_COOP]) if coop is None else coop
    c["modes"] = [[rword(r) for _ in range(3)] for _ in range(n)]
    c["cur"] = None
    c["fmt"] = "hello"
    c["a"] = c["b"] = 0
    c["addref"] = 0
    return c


def term_edges():
    r = random.Random(11)
    for init in (0, 1):
        for dd in (0, DD_OBJ):
            for hw in (0, HWND_OBJ):
                for n in (0, 1, 3):
                    yield base(r, "term", n, init, dd, hw)


def term_random(r):
    return base(r, "term")


def show_case(r, fmt=None, dd=None, coop=None):
    c = base(r, "show", 0, 0, dd, None, coop)
    c["fmt"] = r.choice(FMT_LIST) if fmt is None else fmt
    c["a"], c["b"] = rword(r), rword(r)
    return c


def show_edges():
    r = random.Random(12)
    for fmt in FMT_LIST:
        for dd in (0, DD_OBJ):
            for coop in (0, WIN_COOP):
                yield show_case(r, fmt, dd, coop)


def show_random(r):
    return show_case(r)


def getdd_case(r, addref=None, dd=None):
    c = base(r, "getdd", 0, 0, dd)
    c["addref"] = r.choice([0, 1, -1, 5, 0x80000000]) & MASK if addref is None else addref
    return c


def getdd_edges():
    r = random.Random(13)
    for a in (0, 1, MASK, 7):
        for dd in (0, DD_OBJ):
            yield getdd_case(r, a, dd)


def getdd_random(r):
    return getdd_case(r)


def first_edges():
    r = random.Random(14)
    for n in (1, 2, 3):
        yield base(r, "first", n)


def first_random(r):
    return base(r, "first", r.randrange(1, 4))


def next_case(r):
    c = base(r, "next", r.randrange(1, 4))
    c["cur"] = r.randrange(len(c["modes"]) + 1)
    return c


def next_edges():
    r = random.Random(15)
    for n in (0, 1, 3):
        for cur in range(n + 1):
            c = base(r, "next", n)
            c["cur"] = cur
            yield c


TESTS = [
    FnTest("LbScreen_Terminate", ppc, host, edge=term_edges, rnd=term_random),
    FnTest("LbApp_ShowMessage", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("LbScreen_GetDirectDraw", ppc, host, edge=getdd_edges, rnd=getdd_random),
    FnTest("LbScreen_ModeFindFirst", ppc, host, edge=first_edges, rnd=first_random),
    FnTest("LbScreen_ModeFindNext", ppc, host, edge=next_edges, rnd=next_case),
]
