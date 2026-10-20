"""Unit v5334, the screen swap functions and the mode list, against libtpw_unit_LbScreen_PartSwap.dylib (tests/unit_LbScreen_PartSwap.cpp), bullfrog
(WinScrn.cpp), src/CreateDirectDrawObject_LbScreen_PartSwap.cpp:
  LbScreen_PartSwap 0x10069fec, LbScreen_Swap 0x10069d4c, CreateScreenModeList 0x1006a5d8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the DirectDraw and window calls, the event callback, the surface restore, the list init, the app getters) are fakes on both sides
that record their arguments and answer from the case's queues.

The world of a case: the flags word and DirectDraw surface of the primary surface, the second surface and a heap surface behind
gWinScrnSurface, the double buffer word, the mode gate word, the client origin, the window handle, the class name and the queues of answers.
What is compared: the answer (the hidden result word), the globals and the event log.

No fused operations. Not tested: nothing is skipped (a NULL gWinScrnSurface is read unconditionally by the original).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbScreen_PartSwap"
MASK = 0xFFFFFFFF

FN_SWAP, FN_PART, FN_LIST = 0x10069D4C, 0x10069FEC, 0x1006A5D8
SIZES = {FN_SWAP: 632, FN_PART: 368, FN_LIST: 512}
OWN = set(SIZES)

CB = dict(event=0x1001462C, restore=0x10067178, gethwnd=0x10013FB4, gethinst=0x10013F44, sethwnd=0x10013FE4,
          regclass=0x1006A824, listinit=0x10014C80,
          blt=0x1006BCDC, bltfast=0x1006BCF4, flip=0x1006BD9C, release=0x1006BD0C, zero=0x1006BD3C, getrect=0x1006BDB4,
          c2s=0x1006BDCC, coop=0x1006BDFC, ddc=0x1006BE2C, createwin=0x1006C03C, enum=0x1006C054, destroy=0x1006C06C)

ADDR = dict(A=0x10075138, B=0x10075188, SURFPTR=0x10075228, DBL=0x10075118, GATE=0x10073A64, LIST=0x10075128,
            ORIGIN=0x10073A4C, HWND=0x10075238, CLASS=0x10075240)
TOC_ENUM = 0x10070E2C
DDOBJ = [0]
R_LOST, R_RETRY1, R_RETRY2 = 0x887601C2, 0x887601AE, 0x8876021C


def hsh(s):
    h = 0
    for c in s:
        h = (h * 31 + ord(c)) & MASK
    return h


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(e, name, *ints):
    e.cl.rec(name, tuple(int(x) & MASK for x in ints))


def ret(e, key):
    return e.cl.ret(key) & MASK


def cstr_hash(e, a):
    return hsh(e.cstr(a)) if a else 0


def s_event(e):
    d = e.r(5)
    rec(e, "Event", e.r(4), e.u32(d), e.cl.lab(e.u32(d + 4)))


def s_restore(e):
    rec(e, "Restore", e.cl.lab(e.r(3)))
    e.set_r(3, 0)


def s_gethwnd(e):
    rec(e, "GetHWnd")
    e.set_r(3, ret(e, "hwnd"))


def s_gethinst(e):
    rec(e, "GetHInstance")
    e.set_r(3, ret(e, "hinst"))


def s_sethwnd(e):
    rec(e, "SetHWnd", e.r(3))


def s_regclass(e):
    rec(e, "RegisterClass")


def s_listinit(e):
    rec(e, "ListInit", e.cl.lab(e.r(3)))


def s_blt(e):
    this, dest, src, srcrect, flags, fx = [e.r(i) for i in (3, 4, 5, 6, 7, 8)]
    f = struct.unpack(">25I", e.read(fx, 100)) if fx else [0] * 25
    d = struct.unpack(">4I", e.read(dest, 16)) if dest else [0] * 4
    rec(e, "Blt", e.cl.lab(this), e.cl.lab(src), srcrect, flags, 1 if fx else 0, f[0] if fx else 0, 1 if any(f[1:]) else 0, 1 if dest else 0, *d)
    e.set_r(3, ret(e, "blt"))


def s_bltfast(e):
    this, x, y, src, rect, flags = [e.r(i) for i in (3, 4, 5, 6, 7, 8)]
    d = struct.unpack(">4I", e.read(rect, 16)) if rect else [0] * 4
    rec(e, "BltFast", e.cl.lab(this), x, y, e.cl.lab(src), 1 if rect else 0, flags, *d)
    e.set_r(3, ret(e, "bltfast"))


def s_flip(e):
    rec(e, "Flip", e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.r(5))
    e.set_r(3, ret(e, "flip"))


def s_release(e):
    rec(e, "Release", e.cl.lab(e.r(3)))
    e.set_r(3, 0)


def s_zero(e):
    e.write(e.r(3), bytes(e.r(4)))
    rec(e, "Zero", e.r(4))


def s_getrect(e):
    rec(e, "GetClientRect", e.r(3))
    e.write(e.r(4), struct.pack(">4I", *[ret(e, "rect%d" % i) for i in range(4)]))


def s_c2s(e):
    p = e.r(4)
    x, y = e.u32(p), e.u32(p + 4)
    rec(e, "ClientToScreen", e.r(3), x, y)
    e.write(p, struct.pack(">2I", (x + ret(e, "cx")) & MASK, (y + ret(e, "cy")) & MASK))


def s_coop(e):
    rec(e, "SetCoop", e.cl.lab(e.r(3)), e.r(4), e.r(5))
    e.set_r(3, ret(e, "coop"))


def s_ddc(e):
    rec(e, "DirectDrawCreate", e.r(3), e.r(5))
    r = ret(e, "ddc")
    if r == 0:
        e.write(e.r(4), struct.pack(">I", DDOBJ[0]))
    e.set_r(3, r)


def s_createwin(e):
    sp = e.r(1)
    rec(e, "CreateWindowEx", e.r(3), cstr_hash(e, e.r(4)), cstr_hash(e, e.r(5)), e.r(6), e.r(7), e.r(8), e.r(9), e.r(10),
        e.u32(sp + 0x38), e.u32(sp + 0x3C), e.u32(sp + 0x40), e.u32(sp + 0x44))
    e.set_r(3, ret(e, "createwindow"))


def s_enum(e):
    rec(e, "EnumModes", e.cl.lab(e.r(3)), e.r(4), e.r(5), e.cl.lab(e.r(6)), 1 if e.r(7) == TOC_ENUM else 0)
    e.set_r(3, 0)


def s_destroy(e):
    rec(e, "DestroyWindow", e.r(3))


for _k, _f in dict(event=s_event, restore=s_restore, gethwnd=s_gethwnd, gethinst=s_gethinst, sethwnd=s_sethwnd, regclass=s_regclass,
                   listinit=s_listinit).items():
    STUBS.internal(CB[_k], _f)

GLUE = dict(
    blt=("Blt__18IDirectDrawSurfaceFPQ313SamsUtilities10URectangle8LongRectP18IDirectDrawSurfacePQ313SamsUtilities10URectangle8LongRectUlP8_DDBLTFX", s_blt),
    bltfast=("BltFast__18IDirectDrawSurfaceFUlUlP18IDirectDrawSurfacePQ313SamsUtilities10URectangle8LongRectUl", s_bltfast),
    flip=("Flip__18IDirectDrawSurfaceFP18IDirectDrawSurfaceUl", s_flip),
    release=("Release__Q210NS_MacDoze8IUnknownFv", s_release),
    zero=("ZeroMemory__10NS_MacDozeFPvUl", s_zero),
    getrect=("GetClientRect__10NS_MacDozeFP8GrafPortPQ313SamsUtilities10URectangle8LongRect", s_getrect),
    c2s=("ClientToScreen__10NS_MacDozeFP8GrafPortPQ210NS_MacDoze8tagPOINT", s_c2s),
    coop=("SetCooperativeLevel__11IDirectDrawFP8GrafPortUl", s_coop),
    ddc=("DirectDrawCreate__FPQ210NS_MacDoze5_GUIDPP11IDirectDrawPQ210NS_MacDoze8IUnknown", s_ddc),
    createwin=("CreateWindowEx__10NS_MacDozeFUlPCcPCcUliiiiP8GrafPortPvPvPv", s_createwin),
    enum=("EnumDisplayModes__11IDirectDrawFUlP14_DDSURFACEDESCPvPFP14_DDSURFACEDESCPv_l", s_enum),
    destroy=("DestroyWindow__10NS_MacDozeFP8GrafPort", s_destroy))
for _name, _f in GLUE.values():
    STUBS.imp(_name, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def ptr(self, a, p):
        self.words(a, [p])
    f1c, dd = 0x1C, 0x24

    def __init__(self, e, case):
        self.e = e
        self.surf = e.alloc(0x60)
        self.objs = [e.alloc(16) for _ in range(4)]
        DDOBJ[0] = self.objs[3]
        self.L = e.cl = CL.PpcLog({self.surf: 1, ADDR["B"]: 2, ADDR["A"]: 3, ADDR["LIST"]: 4, self.objs[0]: 5, self.objs[1]: 6,
                                   self.objs[2]: 7, self.objs[3]: 8}, case["queues"])
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

    def swap(self, flags):
        res = self.alloc(4)
        T.call_ppc(self.e, FN_SWAP, [res, flags & MASK])
        return self.rd_words(res, 1)[0]

    def part(self, x, y, rc, flags):
        res, at, rect = self.alloc(4), self.alloc(8), self.alloc(16)
        self.words(at, [x, y])
        self.words(rect, rc)
        T.call_ppc(self.e, FN_PART, [res, at, rect, flags & MASK])
        return self.rd_words(res, 1)[0]

    def modelist(self, mode, window, guid):
        res = self.alloc(4)
        T.call_ppc(self.e, FN_LIST, [res, mode & MASK, window, guid])
        return self.rd_words(res, 1)[0]


class Host(Side):
    fmt = "<I"

    def ptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.f1c, self.dd = lib.offs
        self.surf = self.alloc(0x100)
        self.objs = [lib.tpw_sw_newobj(0), lib.tpw_sw_newobj(0), lib.tpw_sw_newobj(0), lib.tpw_sw_newobj(1)]
        lib.tpw_sw_set_ddobject(self.objs[3])
        CL.host_begin(lib, {1: self.surf, 2: self.g("B"), 3: self.g("A"), 4: self.g("LIST"), 5: self.objs[0], 6: self.objs[1],
                            7: self.objs[2], 8: self.objs[3]}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sw_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sw_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def swap(self, flags):
        return self.lib.tpw_sw_swap(flags & MASK)

    def part(self, x, y, rc, flags):
        return self.lib.tpw_sw_partswap(s32(x), s32(y), *[s32(v) for v in rc], flags & MASK)

    def modelist(self, mode, window, guid):
        return self.lib.tpw_sw_modelist(s32(mode), window, guid)


def s32(v):
    return T.s32(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sw_addr.restype, lib.tpw_sw_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sw_alloc.restype, lib.tpw_sw_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sw_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sw_newobj.restype, lib.tpw_sw_newobj.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sw_set_ddobject.restype, lib.tpw_sw_set_ddobject.argtypes = None, [C.c_uint64]
    lib.tpw_sw_swap.restype, lib.tpw_sw_swap.argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_sw_partswap.restype, lib.tpw_sw_partswap.argtypes = C.c_uint32, [C.c_int32] * 6 + [C.c_uint32]
    lib.tpw_sw_modelist.restype, lib.tpw_sw_modelist.argtypes = C.c_uint32, [C.c_int32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sw_layout(out)
    lib.offs = list(out)[:n]
    assert n == 2 and 0 < lib.offs[0] < lib.offs[1] < 0x100, lib.offs
    lib.bound = True


def build(s, case):
    b, a = s.g("B"), s.g("A")
    s.words(b + s.f1c, [case["f1c"]])
    s.ptr(b + s.dd, s.objs[0])
    s.ptr(a + s.dd, s.objs[1])
    s.ptr(s.surf + s.dd, s.objs[2])
    s.ptr(s.g("SURFPTR"), s.surf)
    s.words(s.g("DBL"), [case["dbl"]])
    s.words(s.g("GATE"), [case["gate"]])
    s.words(s.g("ORIGIN"), case["origin"])
    s.words(s.g("HWND"), [case["hwnd"]])
    s.write(s.g("CLASS"), case["cls"].encode().ljust(64, b"\0"))


def execute(s, case):
    op = case["op"]
    if op == "swap":
        return s.swap(case["flags"])
    if op == "part":
        return s.part(*case["at"], case["rect"], case["flags"])
    return s.modelist(case["mode"], case["window"], case["guid"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "hwnd": s.rd_words(s.g("HWND"), 1), "origin": s.rd_words(s.g("ORIGIN"), 2),
            "dbl": s.rd_words(s.g("DBL"), 1), "gate": s.rd_words(s.g("GATE"), 1), "cls": s.read(s.g("CLASS"), 64)}


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

def result(r):
    return r.choice([0, 0, R_LOST, 1, 0x80004005, r.getrandbits(32)])


def base(r, op):
    flips = [r.choice([R_RETRY1, R_RETRY2]) for _ in range(r.choice([0, 0, 1, 3]))]
    flips.append(r.choice([0, R_LOST, 0x80004005, r.getrandbits(32) | 1]) if True else 0)
    q = {"flip": flips, "bltfast": [result(r)], "blt": [result(r)], "coop": [r.choice([0, 0, 1, 0x80070057]), r.choice([0, 1])],
         "ddc": [r.choice([0, 0, 0, 1, MASK])], "createwindow": [r.choice([0, 0x73000001, 0x73000002])],
         "hwnd": [0x71000000 + r.randrange(1, 9)], "hinst": [0x74000000 + r.randrange(1, 9)],
         "cx": [r.randrange(-50, 50)], "cy": [r.randrange(-50, 50)]}
    for i in range(4):
        q["rect%d" % i] = [r.randrange(-100, 1000) & MASK]
    return {"op": op, "queues": q, "f1c": r.getrandbits(32) if r.random() < 0.3 else r.choice([0, 2, 0x10, 0x12, 0x1C, 4, 0x20]),
            "dbl": r.choice([0, 0, 1, 7, MASK]), "gate": r.choice([0, 0, 0, 1, MASK]),
            "origin": [r.randrange(-30, 30) & MASK, r.randrange(-30, 30) & MASK], "hwnd": r.choice([0, 0x75000001]),
            "cls": r.choice(["BullfrogClass", "", "x"])}


def swap_case(r, f1c=None, dbl=None, flags=None):
    c = base(r, "swap")
    if f1c is not None:
        c["f1c"] = f1c
    if dbl is not None:
        c["dbl"] = dbl
    c["flags"] = r.choice([0, 1, 2, 3, MASK]) if flags is None else flags
    return c


def part_case(r, f1c=None, dbl=None, flags=None):
    c = base(r, "part")
    if f1c is not None:
        c["f1c"] = f1c
    if dbl is not None:
        c["dbl"] = dbl
    c["flags"] = r.choice([0, 1, 2, 3, MASK]) if flags is None else flags
    c["at"] = (r.randrange(-20, 400), r.randrange(-20, 400))
    c["rect"] = [r.randrange(0, 100), r.randrange(0, 100), r.randrange(100, 640), r.randrange(100, 480)]
    return c


def list_case(r, mode=None, window=None, guid=None, gate=None, ddc=None, coop=None, cw=None):
    c = base(r, "list")
    c["mode"] = r.choice([0, 1, MASK, 5]) if mode is None else mode
    c["window"] = r.choice([0, 0x76000001]) if window is None else window
    c["guid"] = r.choice([0, 0x72000001]) if guid is None else guid
    if gate is not None:
        c["gate"] = gate
    if ddc is not None:
        c["queues"]["ddc"] = [ddc]
    if coop is not None:
        c["queues"]["coop"] = [coop, r.choice([0, 1])]
    if cw is not None:
        c["queues"]["createwindow"] = [cw]
    return c


F1CS = (0, 2, 0x10, 0x12, 0x1C, 0xFFFFFFFF, 0xFFFFFFFD, 0xFFFFFFEF)


def swap_edges():
    r = random.Random(11)
    for f1c in F1CS:
        for dbl in (0, 1):
            for flags in (0, 1, 3):
                yield swap_case(r, f1c, dbl, flags)
    for i in range(10):
        c = swap_case(r, 2, 0, i & 1)
        c["queues"]["flip"] = [R_RETRY1, R_RETRY2, R_RETRY1, R_RETRY2, 0][: 1 + i % 5] + ([0] if i % 5 < 4 else [])
        yield c
    for res in (0, R_LOST, 1):
        for f1c in (0, 0x10):
            c = swap_case(r, f1c, 0, 0)
            c["queues"]["bltfast"] = [res]
            c["queues"]["blt"] = [res]
            yield c


def part_edges():
    r = random.Random(12)
    for f1c in F1CS:
        for dbl in (0, 1):
            for flags in (0, 1, 3):
                yield part_case(r, f1c, dbl, flags)
    for res in (0, R_LOST, 1, MASK):
        c = part_case(r, 0, 0, 0)
        c["queues"]["bltfast"] = [res]
        yield c


def list_edges():
    r = random.Random(13)
    for mode in (0, 1, MASK, 5):
        for window in (0, 0x76000001):
            for guid in (0, 0x72000001):
                for gate in (0, 1, MASK):
                    for ddc in (0, 1):
                        for coop in (0, 1):
                            for cw in (0, 0x73000001):
                                yield list_case(r, mode, window, guid, gate, ddc, coop, cw)


TESTS = [
    FnTest("LbScreen_Swap", ppc, host, edge=swap_edges, rnd=lambda r: swap_case(r)),
    FnTest("LbScreen_PartSwap", ppc, host, edge=part_edges, rnd=lambda r: part_case(r)),
    FnTest("CreateScreenModeList", ppc, host, edge=list_edges, rnd=lambda r: list_case(r)),
]
