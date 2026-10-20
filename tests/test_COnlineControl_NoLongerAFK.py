"""Unit v51062: four small COnlineControl members and the library functions around them, against
libtpw_unit_COnlineControl_NoLongerAFK.dylib (tests/unit_COnlineControl_NoLongerAFK.cpp), main app
("SimTheme Park"), src/COnlineControl_..._NoLongerAFK.cpp:
  NoLongerAFK 0x10219634, DestroyVisitor 0x102183f4, UpdateAvatarPos 0x1021823c, HideAvatar 0x10217a38, Argument::BStrVal 0x10217764,
  Argument::StrVal 0x10217684, nocase_string::~nocase_string 0x102176d8, basic_ios<wchar_t>::~basic_ios 0x1021cbb8,
  basic_ostream<wchar_t>::~basic_ostream 0x1021cc68, ctype<wchar_t>::do_widen 0x1021d584, basic_streambuf<wchar_t>: ~ 0x1021e14c,
  imbue 0x1021e2d4, setbuf 0x1021e378, sync 0x1021e3cc, showmanyc 0x1021e420, underflow 0x1021e47c, pbackfail 0x1021e5a8,
  overflow 0x1021e608, the buddy map's erase_one 0x1021e8e8, allocator<set<nocase_string> node>::deallocate 0x1021eda4.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
32-bit pointers) for the original, and in the host library's memory (little-endian, native pointers where the headers say so) for the
translation. The callees outside the unit (Chat::NotAFK, Chat::MovePlayer, GetPerson, CThing::KillMe, the string copy constructors,
operator delete, ios_base::~ios_base, the locale's destructor and assignment, value_compare::operator() and the tree's erase) are fakes on
both sides that record every argument; a pointer is logged as (block index, offset) with the host's offsets mapped back to the Mac's.

The world of a case: the blocks the call touches (the COnlineControl with its chat word, flag byte and own-avatar pointer; the avatar's
halfwords; the stream objects with their virtual base table; a random binary tree of nodes). What is compared: the answer where the
original has one the host also has, the bytes and pointer words the call writes (vtable words as the table they point to plus the byte
offset), the event log.

No fused operations. Not tested: a stream or streambuf whose address plus 8 or 0x1c wraps to zero (addic. in the original), and the
answers of the functions the headers declare void (the original returns this).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_NoLongerAFK"
MASK = 0xFFFFFFFF
P = "tpw_v51062_"

ENTRY = dict(afk=0x10219634, destroy=0x102183f4, updpos=0x1021823c, hide=0x10217a38, bstrval=0x10217764, strval=0x10217684,
             nocase=0x102176d8, ios=0x1021cbb8, ostream=0x1021cc68, widen=0x1021d584, sbdtor=0x1021e14c, imbue=0x1021e2d4,
             setbuf=0x1021e378, sync=0x1021e3cc, showmanyc=0x1021e420, underflow=0x1021e47c, pbackfail=0x1021e5a8,
             overflow=0x1021e608, erase=0x1021e8e8, dealloc=0x1021eda4)
SIZES = dict(afk=52, destroy=52, updpos=64, hide=60, bstrval=48, strval=44, nocase=96, ios=104, ostream=152, widen=8, sbdtor=144,
             imbue=72, setbuf=4, sync=8, showmanyc=8, underflow=12, pbackfail=12, overflow=12, erase=220, dealloc=40)
OWN = set(ENTRY.values())

A_NOTAFK, A_MOVE, A_GETPERSON, A_KILLME = 0x10239910, 0x10239178, 0x1021969c, 0x1012e728
A_WCOPY, A_CCOPY, A_NCDTOR, A_DELETE = 0x101f7424, 0x101b2918, 0x10222020, 0x10238218
A_IOSBASE, A_LOCDTOR, A_LOCASSIGN = 0x10238950, 0x10071f34, 0x10072128
A_CMPNK, A_CMPKN, A_TREEERASE = 0x1021f000, 0x1021eed0, 0x102278fc

ADDR = dict(gIfsWVtblC=0x102b1e60, gOfsWVtblB=0x102b1f08, gIfsWVtblE=0x102b1f20)
HOST_LAYOUT = [0xe8c, 0x20, 8, 0x10, 0x48, 8, 0x10, 8, 0x18]

# Mac and host offsets of the fields the module touches (the host has native pointers in the stream, streambuf and tree headers)
OFF_PPC = dict(slot=4, ios=8, baseoff=0x44, sbloc=0x1c, troot=4, tf8=8, nleft=0, nright=4, npair=0xc)
OFF_HOST = dict(slot=8, ios=0x10, baseoff=0x48, sbloc=0x20, troot=8, tf8=0x10, nleft=0, nright=8, npair=0x18)
SENT = 0x11223344

OBJS = []        # (base, size) of the case's blocks, for the PowerPC fakes


def s32(v):
    return T.s32(v)


def ptrid(v):
    if v == 0:
        return 0
    for i, (b, n) in enumerate(OBJS):
        if b <= v < b + n:
            return (i + 1) * 0x10000 + (v - b)
    return -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


@stub(A_NOTAFK)
def stub_notafk(e):
    e.cl.rec("NotAFK", (e.r(3) & MASK, ptrid(e.r(4))))


@stub(A_MOVE)
def stub_move(e):
    e.cl.rec("MovePlayer", (e.r(3) & MASK, s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), ptrid(e.r(7))))


@stub(A_GETPERSON)
def stub_getperson(e):
    e.cl.rec("GetPerson", (ptrid(e.r(3)), ptrid(e.r(4))))
    e.set_r(3, e.cl.ret("GetPerson") & MASK)


@stub(A_KILLME)
def stub_killme(e):
    e.cl.rec("KillMe", (e.r(3) & MASK,))


@stub(A_CCOPY)
def stub_ccopy(e):
    e.cl.rec("StringCopy", (ptrid(e.r(3)), ptrid(e.r(4)), s32(e.r(5)), e.r(6) & MASK))


@stub(A_WCOPY)
def stub_wcopy(e):
    e.cl.rec("WStringCopy", (ptrid(e.r(3)), ptrid(e.r(4)), s32(e.r(5)), e.r(6) & MASK))


@stub(A_NCDTOR)
def stub_ncdtor(e):
    e.cl.rec("NocaseWStringDtor", (ptrid(e.r(3)), T.s16(e.r(4))))


@stub(A_DELETE)
def stub_delete(e):
    e.cl.rec("delete", (ptrid(e.r(3)),))


@stub(A_IOSBASE)
def stub_iosbase(e):
    e.cl.rec("IosBaseDtor", (ptrid(e.r(3)), s32(e.r(4))))


@stub(A_LOCDTOR)
def stub_locdtor(e):
    e.cl.rec("LocaleDtor", (ptrid(e.r(3)), T.s16(e.r(4))))


@stub(A_LOCASSIGN)
def stub_locassign(e):
    e.cl.rec("LocaleAssign", (ptrid(e.r(3)), ptrid(e.r(4))))


@stub(A_CMPNK)
def stub_cmpnk(e):
    e.cl.rec("CmpNK", (ptrid(e.r(3)), ptrid(e.r(4)), ptrid(e.r(5))))
    e.set_r(3, e.cl.ret("CmpNK") & MASK)


@stub(A_CMPKN)
def stub_cmpkn(e):
    e.cl.rec("CmpKN", (ptrid(e.r(3)), ptrid(e.r(4)), ptrid(e.r(5))))
    e.set_r(3, e.cl.ret("CmpKN") & MASK)


@stub(A_TREEERASE)
def stub_treeerase(e):
    e.cl.rec("TreeErase", (ptrid(e.r(3)), ptrid(e.r(4))))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.hfmt, T.s16(v)))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rbyte(self, a):
        return self.read(a, 1)[0]

    def fill(self, a, n, b):
        self.write(a, bytes([b]) * n)

    def o(self, key):
        return self.OFF[key]

    def call(self, key, args):
        return self.invoke(key, list(args))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    hfmt = ">h"
    psz = 4
    OFF = OFF_PPC

    def __init__(self, e, case):
        self.e = e
        self.blocks = [e.alloc(n) for n in case["objs"]]
        OBJS[:] = list(zip(self.blocks, case["objs"]))
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = CL.Stubs.activate(STUBS, e) if False else STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def arena(self):
        return 0

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def invoke(self, key, args):
        r3, _, _ = T.call_ppc(self.e, ENTRY[key], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    hfmt = "<h"
    psz = 8
    OFF = OFF_HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks = [getattr(lib, P + "alloc")(n) for n in case["objs"]]
        OBJS[:] = list(zip(self.blocks, case["objs"]))
        getattr(lib, P + "clear")()
        for b, n in OBJS:
            getattr(lib, P + "obj")(b, n)
        for idx, key in case["maps"]:
            getattr(lib, P + "map")(idx, OFF_HOST[key], OFF_PPC[key])
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def arena(self):
        return self.g("arenaBase")

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def invoke(self, key, args):
        args = list(args) + [0] * (3 - len(args))
        return getattr(self.lib, P + key)(*args) & 0xFFFFFFFFFFFFFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "obj").restype, getattr(lib, P + "obj").argtypes = C.c_int32, [C.c_uint64, C.c_uint32]
    getattr(lib, P + "map").argtypes = [C.c_int32, C.c_uint32, C.c_uint32]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    for key in ENTRY:
        f = getattr(lib, P + key)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 3
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def normptr(s, v, names):
    """A vtable word as (table name, byte offset), or the raw word when it is none of them."""
    for nm in names:
        d = v - s.g(nm)
        if 0 <= d < 0x20:
            return (nm, d)
    return ("raw", v)


# ---------------------------------------------------------------------------------------------------------------- the worlds


def do_afk(s, c):
    A = s.blocks[0]
    s.fill(A, 0xe90, 0xAA)
    s.w32(A, c["chat"])
    s.byte(A + 0xe51, c["old"])
    s.call("afk", [A, c["flag"]])
    return {"log": s.log(), "b": s.read(A + 0xe48, 16), "chat": s.r32(A)}


def do_destroy(s, c):
    A, N = s.blocks
    s.fill(A, 0xe90, 0xAA)
    s.fill(N, 0x40, 0x11)
    s.call("destroy", [A, N])
    return {"log": s.log()}


def do_updpos(s, c):
    A = s.blocks[0]
    s.fill(A, 0xe90, 0xAA)
    s.w32(A, c["chat"])
    s.call("updpos", [A, c["x"], c["y"]])
    return {"log": s.log(), "chat": s.r32(A)}


def do_hide(s, c):
    A, B = s.blocks
    s.fill(A, 0xe90, 0xAA)
    s.fill(B, 0x20, 0x77)
    s.w32(A, c["chat"])
    s.w32(A + 4, B - s.arena())
    s.half(B + 4, c["hx"])
    s.half(B + 6, c["hy"])
    s.call("hide", [A])
    return {"log": s.log()}


def do_copy(key):
    def f(s, c):
        A, R = s.blocks
        s.fill(A, 0x20, 0x33)
        s.fill(R, 0x20, 0x44)
        s.call(key, [R, A])
        return {"log": s.log()}
    return f


def do_nocase(s, c):
    A = s.blocks[0]
    s.fill(A, 0x20, 0x66)
    s.call("nocase", [0 if c["null"] else A, c["flag"]])
    return {"log": s.log()}


def do_dealloc(s, c):
    N, Al = s.blocks
    s.fill(N, 0x20, 0x66)
    s.fill(Al, 0x20, 0x67)
    s.call("dealloc", [Al, 0 if c["null"] else N, c["count"]])
    return {"log": s.log()}


def do_ios(s, c):
    A = s.blocks[0]
    s.fill(A, 0x20, 0x5A)
    s.wptr(A, SENT)
    s.call("ios", [0 if c["null"] else A, c["flag"]])
    return {"log": s.log(), "vt": normptr(s, s.rptr(A), ["gIfsWVtblE"])}


def do_ostream(s, c):
    A, Tb = s.blocks
    s.fill(A, 0x20, 0x5A)
    s.fill(Tb, 0x60, 0x5B)
    s.wptr(A, Tb)
    s.wptr(A + s.o("slot"), SENT)
    s.wptr(A + s.o("ios"), SENT)
    s.wptr(Tb, SENT)
    s.w32(Tb + s.o("baseoff"), SENT)
    s.call("ostream", [0 if c["null"] else A, c["flag"]])
    off = s.r32(Tb + s.o("baseoff"))
    exp = (A + s.o("ios") - Tb) & MASK
    return {"log": s.log(), "vb": s.rptr(A) == Tb, "slot": normptr(s, s.rptr(A + s.o("slot")), ["gOfsWVtblB"]),
            "ios": normptr(s, s.rptr(A + s.o("ios")), ["gIfsWVtblE"]), "tab": normptr(s, s.rptr(Tb), ["gOfsWVtblB"]),
            "base": "raw" if off == SENT else (off - exp) & MASK}


def do_sbdtor(s, c):
    A = s.blocks[0]
    s.fill(A, 0x40, 0x5A)
    s.wptr(A, SENT)
    s.call("sbdtor", [0 if c["null"] else A, c["flag"]])
    return {"log": s.log(), "vt": normptr(s, s.rptr(A), ["gIfsWVtblC"])}


def do_imbue(s, c):
    A, B = s.blocks
    s.fill(A, 0x40, 0x5A)
    s.fill(B, 0x20, 0x5B)
    s.call("imbue", [A, B])
    return {"log": s.log()}


def do_setbuf(s, c):
    A = s.blocks[0]
    s.fill(A, 0x40, 0x5A)
    r = s.call("setbuf", [A, c["buf"], c["n"]])
    return {"answer": ptrid(r), "log": s.log()}


def do_plain(key):
    def f(s, c):
        A = s.blocks[0]
        s.fill(A, 0x40, 0x5A)
        r = s.call(key, [A, c["arg"]])
        return {"answer": r & MASK, "log": s.log()}
    return f


def do_widen(s, c):
    A = s.blocks[0]
    s.fill(A, 0x20, 0x5A)
    r = s.call("widen", [A, c["arg"]])
    return {"answer": r & 0xFFFF, "log": s.log()}


def do_erase(s, c):
    Tr, K = s.blocks[0], s.blocks[1]
    nodes = s.blocks[2:]
    s.fill(Tr, 0x40, 0)
    s.fill(K, 0x20, 0x3C)
    for nd in nodes:
        s.fill(nd, 0x30, 0)
    s.w32(Tr, 0x1234)
    if nodes:
        s.wptr(Tr + s.o("troot"), nodes[0])
    for i, par, side in c["shape"]:
        s.wptr(nodes[par] + s.o("nleft" if side == "l" else "nright"), nodes[i])
    r = s.call("erase", [Tr, K])
    return {"answer": r & MASK, "log": s.log(), "count": s.r32(Tr),
            "root": s.rptr(Tr + s.o("troot")) == (nodes[0] if nodes else 0)}


DO = dict(afk=do_afk, destroy=do_destroy, updpos=do_updpos, hide=do_hide, strval=do_copy("strval"), bstrval=do_copy("bstrval"),
          nocase=do_nocase, dealloc=do_dealloc, ios=do_ios, ostream=do_ostream, sbdtor=do_sbdtor, imbue=do_imbue, setbuf=do_setbuf,
          sync=do_plain("sync"), showmanyc=do_plain("showmanyc"), underflow=do_plain("underflow"), pbackfail=do_plain("pbackfail"),
          overflow=do_plain("overflow"), widen=do_widen, erase=do_erase)


def run(s, case):
    try:
        return DO[case["op"]](s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def pick(r, v, choices):
    return r.choice(choices) if v is None else v


def base(op, objs, maps=(), queues=None):
    return {"op": op, "objs": list(objs), "maps": list(maps), "queues": queues or {}}


def rflag(r):
    return r.choice([0, 1, 2, -1, -2, 0x7fff, 0x8000, 0x10001, 0x18000, 0xffff, 0x12345, r.randrange(-40000, 40000)]) & MASK


def mk_afk(r, chat=None, old=None, flag=None):
    c = base("afk", [0xe90])
    c.update(chat=r.getrandbits(32) if chat is None else chat, old=pick(r, old, [0, 1, 0x55, 0xff]), flag=pick(r, flag, [0, 1, 2, 0x100]))
    return c


def mk_destroy(r, person=None):
    c = base("destroy", [0xe90, 0x40], queues={"GetPerson": [pick(r, person, [0, 0x71234560, 1, r.getrandbits(32)])]})
    return c


def mapword(r, cell=None, off=None):
    cell = pick(r, cell, [0, 1, 0x7f, 0x80, 0xff, r.randrange(256)])
    off = pick(r, off, [0, 1, 0x7f, 0x80, 0xff, r.randrange(256)])
    return (cell << 24 | off << 16 | r.getrandbits(16)) & MASK


def mk_updpos(r, x=None, y=None, chat=None):
    c = base("updpos", [0xe90])
    c.update(chat=r.getrandbits(32) if chat is None else chat, x=mapword(r) if x is None else x, y=mapword(r) if y is None else y)
    return c


def mk_hide(r, hx=None, hy=None, chat=None):
    c = base("hide", [0xe90, 0x20])
    c.update(chat=r.getrandbits(32) if chat is None else chat, hx=pick(r, hx, [0, -1, 0x7fff, -0x8000, r.randrange(-0x8000, 0x8000)]),
             hy=pick(r, hy, [0, -1, 0x7fff, -0x8000, r.randrange(-0x8000, 0x8000)]))
    return c


def mk_copy(op):
    def f(r):
        return base(op, [0x20, 0x20])
    return f


def mk_nocase(r, null=None, flag=None):
    c = base("nocase", [0x20])
    c.update(null=pick(r, null, [False, False, True]), flag=rflag(r) if flag is None else flag)
    return c


def mk_dealloc(r, null=None, count=None):
    c = base("dealloc", [0x20, 0x20])
    c.update(null=pick(r, null, [False, False, True]), count=r.getrandbits(32) if count is None else count)
    return c


def mk_ios(r, null=None, flag=None):
    c = base("ios", [0x20])
    c.update(null=pick(r, null, [False, False, True]), flag=rflag(r) if flag is None else flag)
    return c


def mk_ostream(r, null=None, flag=None):
    c = base("ostream", [0x20, 0x60], maps=[(0, "ios")])
    c.update(null=pick(r, null, [False, False, True]), flag=rflag(r) if flag is None else flag)
    return c


def mk_sbdtor(r, null=None, flag=None):
    c = base("sbdtor", [0x40], maps=[(0, "sbloc")])
    c.update(null=pick(r, null, [False, False, True]), flag=rflag(r) if flag is None else flag)
    return c


def mk_imbue(r):
    return base("imbue", [0x40, 0x20], maps=[(0, "sbloc")])


def mk_setbuf(r, buf=None, n=None):
    c = base("setbuf", [0x40])
    c.update(buf=r.getrandbits(32) if buf is None else buf, n=r.getrandbits(32) if n is None else n)
    return c


def mk_plain(op):
    def f(r, arg=None):
        c = base(op, [0x40])
        c["arg"] = r.getrandbits(32) if arg is None else arg
        return c
    return f


def mk_widen(r, arg=None):
    c = base("widen", [0x20])
    c["arg"] = pick(r, arg, [0, 1, 0x7f, 0x80, 0xff, 0x1ff, 0xffffff80, r.getrandbits(32), r.randrange(256)])
    return c


def mk_erase(r, n=None, nk=None, kn=None):
    n = r.randrange(0, 7) if n is None else n
    shape = []
    free = [(0, "l"), (0, "r")] if n > 0 else []
    for i in range(1, n):
        slot = r.choice(free)
        free.remove(slot)
        shape.append((i, slot[0], slot[1]))
        free += [(i, "l"), (i, "r")]
    nk = nk if nk is not None else [r.choice([0, 1]) for _ in range(r.randrange(1, 7))]
    kn = kn if kn is not None else [r.choice([0, 1])]
    c = base("erase", [0x40, 0x20] + [0x30] * n, maps=[(0, "troot"), (0, "tf8")] + [(2 + i, "npair") for i in range(n)],
             queues={"CmpNK": nk, "CmpKN": kn})
    c["shape"] = shape
    return c


def edges_of(mk, lst):
    def gen():
        r = random.Random(77)
        for kw in lst:
            yield mk(r, **kw)
    return gen


def rnd_of(mk):
    return lambda r: mk(r)


FLAGS = [dict(flag=f & MASK) for f in (0, 1, 2, -1, 0x7fff, 0x8000, 0x10001, 0x18000, 0xffff, 0x10000)]
NULLFLAG = [dict(null=True, flag=1), dict(null=True, flag=0)] + [dict(null=False, **f) for f in FLAGS]


def test(name, mk, edges):
    return FnTest(name, ppc, host, edge=edges_of(mk, edges), rnd=rnd_of(mk))


NK = "std::basic_streambuf<wchar_t,std::char_traits<wchar_t>>::"
TESTS = [
    test("COnlineControl::NoLongerAFK", mk_afk, [dict(old=0, flag=0), dict(old=1, flag=1), dict(old=0xff, flag=2), dict(chat=0), dict(chat=0xffffffff)]),
    test("COnlineControl::DestroyVisitor", mk_destroy, [dict(person=0), dict(person=0x71234560), dict(person=1), dict(person=0xffffffff)]),
    test("COnlineControl::UpdateAvatarPos", mk_updpos,
         [dict(x=0, y=0), dict(x=0x7fff0000, y=0x80000000), dict(x=0xffff0000, y=0xffffffff), dict(x=0x00800000, y=0x80ff0000),
          dict(x=0x01000000, y=0x00010000), dict(chat=0)]),
    test("COnlineControl::HideAvatar", mk_hide,
         [dict(hx=0, hy=0), dict(hx=-1, hy=1), dict(hx=0x7fff, hy=-0x8000), dict(hx=-0x8000, hy=0x7fff), dict(chat=0)]),
    FnTest("Argument::BStrVal", ppc, host, edge=edges_of(lambda r: mk_copy("bstrval")(r), [dict()]), rnd=rnd_of(mk_copy("bstrval"))),
    FnTest("Argument::StrVal", ppc, host, edge=edges_of(lambda r: mk_copy("strval")(r), [dict()]), rnd=rnd_of(mk_copy("strval"))),
    test("nocase_string::~nocase_string", mk_nocase, NULLFLAG),
    test("std::basic_ios<wchar_t,std::char_traits<wchar_t>>::~basic_ios", mk_ios, NULLFLAG),
    test("std::basic_ostream<wchar_t,std::char_traits<wchar_t>>::~basic_ostream", mk_ostream, NULLFLAG),
    test("std::ctype<wchar_t>::do_widen", mk_widen,
         [dict(arg=a) for a in (0, 1, 0x7f, 0x80, 0xff, 0x100, 0x1ff, 0xffffff80, 0xffffffff, 0x12345680)]),
    test(NK + "~basic_streambuf", mk_sbdtor, NULLFLAG),
    test(NK + "imbue", mk_imbue, [dict()]),
    test(NK + "setbuf", mk_setbuf, [dict(buf=0, n=0), dict(buf=0xffffffff, n=0xffffffff), dict(buf=0x1000, n=5)]),
    test(NK + "sync", mk_plain("sync"), [dict(arg=0), dict(arg=1)]),
    test(NK + "showmanyc", mk_plain("showmanyc"), [dict(arg=0), dict(arg=1)]),
    test(NK + "underflow", mk_plain("underflow"), [dict(arg=0), dict(arg=1)]),
    test(NK + "pbackfail", mk_plain("pbackfail"), [dict(arg=0), dict(arg=0xffff), dict(arg=0xffffffff), dict(arg=0x41)]),
    test(NK + "overflow", mk_plain("overflow"), [dict(arg=0), dict(arg=0xffff), dict(arg=0xffffffff), dict(arg=0x41)]),
    test("std::__tree<std::pair<const_nocase_string,BuddyInfo>,std::map<nocase_string,BuddyInfo,std::less<nocase_string>,"
         "std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::value_compare,std::allocator<std::pair<const_nocase_string,"
         "BuddyInfo>>>::erase_one<nocase_string>", mk_erase,
         [dict(n=0), dict(n=1, nk=[1], kn=[0]), dict(n=1, nk=[0], kn=[0]), dict(n=1, nk=[0], kn=[1]), dict(n=3, nk=[0, 1, 0], kn=[0]),
          dict(n=3, nk=[1, 0, 0], kn=[0]), dict(n=4, nk=[0, 0, 0, 0], kn=[0]), dict(n=4, nk=[1, 1, 1, 1], kn=[0]),
          dict(n=5, nk=[0, 1, 1, 0, 1], kn=[1])]),
    test("std::allocator<std::__tree<nocase_string,std::less<nocase_string>,std::allocator<nocase_string>>::node>::deallocate", mk_dealloc,
         [dict(null=False, count=0), dict(null=False, count=0xffffffff), dict(null=True, count=1)]),
]
