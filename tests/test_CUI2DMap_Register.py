"""Unit v51105, twenty small functions, against libtpw_unit_CUI2DMap_Register.dylib (tests/unit_CUI2DMap_Register.cpp), main app
("SimTheme Park"), src/CUI2DMap_..._CUI2DMap_UnRegister_CUI2DMap_Register.cpp:
  CUI2DMap::Register 0x10230c70, GetMapGridCell 0x1022ddd0, ~CUI2DMap 0x1022bf04, CUI2DMapSprite::SetPosition 0x10231014,
  TUI2DMapMapGridCell::TUI2DMapMapGridCell 0x1022bed0, std::ctype<wchar_t>::~ctype 0x10229860, do_toupper 0x10229808,
  do_tolower 0x102297b0, do_narrow 0x10229760, the __cdeque iterator copy constructor 0x102295ec, std::__destroy<vector<Argument>*>
  0x1022935c, deque::make_iterator 0x10228e2c, nocase_string::nocase_string 0x10228770, nocase basic_string::init 0x1022705c,
  the nocase and plain wide string size/begin 0x10226134 0x102260c8 0x1022605c 0x10225ff0, deque::deque 0x10225938,
  const_iterator::operator* 0x10225874.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian, 32-bit pointers)
and in the host library's memory (little-endian, 8-byte native pointers where the header says so). The callees outside the unit are
fakes on both sides that record the same events: AddTail, SetSpritePosition, operator delete and new, wmemcpy, the cdeque iterator
add (it moves the slot and the index from the case's queues), the vector destructor and the substring constructor.

What is compared: the answer (pointers as offsets from their object), the words and blocks the function may write, the event log.
No fused operations. Not tested: the catch block of init (operator new never fails), a negative length for init (wmemcpy of a
huge count), a zero block size for make_iterator (divwu by zero is undefined on the hardware).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUI2DMap_Register"
MASK = 0xFFFFFFFF

F_REGISTER, F_GRID, F_DTOR, F_SETPOS, F_GRIDCTOR = 0x10230C70, 0x1022DDD0, 0x1022BF04, 0x10231014, 0x1022BED0
F_CDTOR, F_UPPER, F_LOWER, F_NARROW = 0x10229860, 0x10229808, 0x102297B0, 0x10229760
F_CURCOPY, F_DESTROY, F_MAKEIT, F_NOCOPY, F_INIT = 0x102295EC, 0x1022935C, 0x10228E2C, 0x10228770, 0x1022705C
F_NSIZE, F_NBEGIN, F_WSIZE, F_WBEGIN, F_DQCTOR, F_DEREF = 0x10226134, 0x102260C8, 0x1022605C, 0x10225FF0, 0x10225938, 0x10225874
SIZES = {F_REGISTER: 44, F_GRID: 64, F_DTOR: 76, F_SETPOS: 64, F_GRIDCTOR: 4, F_CDTOR: 88, F_UPPER: 36, F_LOWER: 36, F_NARROW: 28,
         F_CURCOPY: 28, F_DESTROY: 80, F_MAKEIT: 240, F_NOCOPY: 56, F_INIT: 264, F_NSIZE: 12, F_NBEGIN: 12, F_WSIZE: 12,
         F_WBEGIN: 12, F_DQCTOR: 44, F_DEREF: 8}
OWN = set(SIZES)
ENTRY = {"register": F_REGISTER, "gridcell": F_GRID, "dtor": F_DTOR, "setpos": F_SETPOS, "gridctor": F_GRIDCTOR,
         "ctype_dtor": F_CDTOR, "toupper": F_UPPER, "tolower": F_LOWER, "narrow": F_NARROW, "cursor_copy": F_CURCOPY,
         "destroy": F_DESTROY, "makeit": F_MAKEIT, "nocase_copy": F_NOCOPY, "init": F_INIT, "nsize": F_NSIZE, "nbegin": F_NBEGIN,
         "wsize": F_WSIZE, "wbegin": F_WBEGIN, "dequector": F_DQCTOR, "deref": F_DEREF}
# name -> (number of arguments, has an answer)
SIG = {"register": (2, 0), "gridcell": (3, 1), "dtor": (2, 1), "setpos": (2, 0), "gridctor": (1, 0), "ctype_dtor": (2, 1),
       "toupper": (2, 1), "tolower": (2, 1), "narrow": (3, 1), "cursor_copy": (2, 0), "destroy": (2, 0), "makeit": (3, 0),
       "nocase_copy": (2, 1), "init": (3, 0), "nsize": (1, 1), "nbegin": (1, 1), "wsize": (1, 1), "wbegin": (1, 1),
       "dequector": (1, 0), "deref": (1, 1)}

# callees faked on both sides
A_ADDTAIL, A_SETSPRITEPOS, A_DELETE, A_NEW, A_WMEMCPY = 0x102318F0, 0x1023AE28, 0x10238218, 0x102384B8, 0x102389E0
A_ITERADD, A_VECDTOR, A_SUBSTR = 0x101F2BEC, 0x102010F8, 0x1022213C

# the TOC slots (the asm's comments) whose content is the global's address: vt 0x102b296c, gei 0x103bca60, upper 0x103bd0a0,
# lower 0x103bcf9c, block 0x102b1f90, term 0x102b208c
SLOT = dict(vt=0x10261E80, gei=0x10261E68, upper=0x10261DEC, lower=0x10261DDC, block=0x10264314, term=0x10264310)


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_addtail(e):
    e.cl.rec("AddTail", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_setpos(e):
    L = e.cl
    L.rec("SetSpritePosition", (L.lab(e.r(3)), L.lab(e.r(4)), canon_f(e.r(5)), canon_f(e.r(6)), canon_f(e.r(7))))
    e.set_r(3, 0)


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size if size >= 32 else 32))


def st_wmemcpy(e):
    dst, src, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("wmemcpy", (e.cl.lab(dst), e.cl.lab(src), n))
    if n:
        e.write(dst, e.read(src, 2 * n))
    e.set_r(3, dst)


def st_iteradd(e):
    L = e.cl
    cur = e.r(3)
    L.rec("IterAdd", (e.u32(cur + 4), s32(e.r(4))))
    adv = L.ret("adv")
    e.write(cur + 8, struct.pack(">I", (e.u32(cur + 8) + 4 * adv) & MASK))
    e.write(cur + 4, struct.pack(">I", L.ret("idx") & MASK))
    e.set_r(3, cur)


def st_vecdtor(e):
    e.cl.rec("VectorDtor", (e.cl.lab(e.r(3)), T.s16(e.r(4))))


def st_substr(e):
    e.cl.rec("Copy", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.r(5), e.r(6)))


for _a, _f in ((A_ADDTAIL, st_addtail), (A_SETSPRITEPOS, st_setpos), (A_DELETE, st_delete), (A_NEW, st_new),
               (A_WMEMCPY, st_wmemcpy), (A_ITERADD, st_iteradd), (A_VECDTOR, st_vecdtor), (A_SUBSTR, st_substr)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

BLOCKS = [0x20100, 0x800, 0x800, 0x800, 0x800, 0x800]     # o[0] is a whole map; o[3] holds vectors of 0xc bytes


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def hwords(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_h(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def vec_labels(self):
        d = {20 + k: self.o[3] + 12 * k for k in range(12)}
        d.update({1: self.o[0], 2: self.o[1], 3: self.o[2], 6: self.o[5]})
        return d


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    off_sprite = 0x28
    off_data = 0xC

    def __init__(self, e, case):
        self.e = e
        self.o = [e.alloc(n) for n in BLOCKS]
        fixed = {a: l for l, a in self.vec_labels().items()}
        fixed[self.g("gei")] = 30
        self.vtword = self.g("vt")
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        """The address of a global as the code loads it: the content of its TOC slot (imports sit elsewhere than the listing says)."""
        return self.e.u32(SLOT[name])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, name, *args):
        r3, _, _ = T.call_ppc(self.e, ENTRY[name], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8
    off_data = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = [lib.tpw_u2_alloc(n) for n in BLOCKS]
        fixed = self.vec_labels()
        fixed[30] = lib.tpw_u2_addr(b"gei")
        CL.host_begin(lib, fixed, case["queues"])
        self.off_sprite = lib.layout[0]
        self.vtword = lib.tpw_u2_addr(b"vt") & MASK

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_u2_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, *args):
        r = getattr(self.lib, "tpw_u2_" + name)(*[a & 0xFFFFFFFFFFFFFFFF for a in args])
        return r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_u2_addr.restype, lib.tpw_u2_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_u2_alloc.restype, lib.tpw_u2_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_u2_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, (n, ans) in SIG.items():
        f = getattr(lib, "tpw_u2_" + name)
        f.argtypes = [C.c_uint64] * n
        f.restype = C.c_uint64 if ans else None
    out = (C.c_int64 * 32)()
    n = lib.tpw_u2_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the operations


def rel(ans, base):
    """A pointer answer as an offset from its object, 0 for NULL."""
    return (ans - base) if ans else 0


def op_register(s, c):
    m, lst, sp = s.o[0], s.o[1], s.o[2]
    s.ptr(m + 0x20080, lst)
    s.call("register", m, [0, sp, 0xDEADBEEF][c["sk"]])
    return {"log": s.log()}


def op_gridcell(s, c):
    m = s.o[0]
    ans = s.call("gridcell", m, c["x"], c["y"])
    return {"ans": rel(ans, m), "log": s.log()}


def op_dtor(s, c):
    m = s.o[0]
    s.words(m, c["w"])
    this = m if c["this"] else 0
    ans = s.call("dtor", this, c["flag"])
    w = s.rd_words(m, 5)
    return {"ans": 1 if (ans and ans == m) else ans, "w0vt": w[0] == (s.vtword & MASK), "w0": -1 if w[0] == (s.vtword & MASK) else w[0],
            "rest": w[1:], "log": s.log()}


def op_setpos(s, c):
    sp, inst, vec = s.o[0], s.o[1], s.o[2]
    s.ptr(sp + s.off_sprite, inst if c["inst"] else 0)
    s.words(vec, c["vec"])
    s.call("setpos", sp, vec)
    return {"vec": s.rd_words(vec, 3), "log": s.log()}


def op_gridctor(s, c):
    s.words(s.o[0], c["w"])
    s.call("gridctor", s.o[0])
    return {"w": s.rd_words(s.o[0], 8), "log": s.log()}


def op_ctype_dtor(s, c):
    m = s.o[0]
    s.words(m, c["w"])
    this = m if c["this"] else 0
    ans = s.call("ctype_dtor", this, c["flag"])
    return {"ans": 1 if (ans and ans == m) else ans, "w": s.rd_words(m, 4), "log": s.log()}


def op_map(s, c):
    s.hwords(s.g("upper"), c["upper"])
    s.hwords(s.g("lower"), c["lower"])
    ans = s.call(c["fn"], s.o[0], c["c"])
    return {"ans": ans & 0xFFFF, "log": s.log()}


def op_narrow(s, c):
    ans = s.call("narrow", s.o[0], c["c"], c["d"])
    return {"ans": ans & 0xFF, "log": s.log()}


def op_curcopy(s, c):
    d, sr = s.o[0], s.o[1]
    s.words(d, c["d"])
    s.words(sr, c["s"])
    s.call("cursor_copy", d, sr)
    return {"d": s.rd_words(d, 4), "s": s.rd_words(sr, 3), "log": s.log()}


def op_destroy(s, c):
    v = s.o[3]
    s.call("destroy", v + 12 * c["a"], v + 12 * c["b"] + c["extra"])
    return {"log": s.log()}


def op_makeit(s, c):
    out, dq, tbl = s.o[0], s.o[1], s.o[2]
    s.words(out, [0xAAAAAAAA] * 8)
    s.words(dq, [c["bs"], c["w1"], c["count"], c["start"], tbl, 0, 0, 0])
    s.words(tbl, c["table"])
    s.call("makeit", out, dq, c["index"])
    w = s.rd_words(out, 8)
    return {"w0": (w[0] - (dq + 4)) & MASK, "w1": w[1], "w2": (w[2] - tbl) & MASK, "rest": w[3:], "log": s.log()}


def op_dequector(s, c):
    s.words(s.g("block"), [c["bs"]])
    s.words(s.o[0], c["w"])
    s.call("dequector", s.o[0])
    return {"w": s.rd_words(s.o[0], 8), "log": s.log()}


def op_deref(s, c):
    s.words(s.o[0], c["w"])
    ans = s.call("deref", s.o[0])
    return {"ans": ans & MASK, "log": s.log()}


def op_nocopy(s, c):
    ans = s.call("nocase_copy", s.o[0], s.o[1])
    return {"ans": 1 if ans == s.o[0] else ans, "log": s.log()}


def op_init(s, c):
    this, src = s.o[0], s.o[1]
    s.hwords(src, c["chars"])
    s.hwords(s.g("term"), [c["term"]])
    s.ptr(this, 0x1234)
    s.call("init", this, src, src + c["bytes"])
    rep = s.rptr(this)
    ln, cap, refs = s.rd_words(rep, 3)
    dp = s.rptr(rep + s.off_data)
    return {"len": ln, "cap": cap, "refs": refs, "chars": s.rd_h(dp, min(cap, 64) + 1), "log": s.log()}


def op_str(s, c):
    obj, rep, data = s.o[0], s.o[1], s.o[2]
    s.ptr(obj, rep)
    s.words(rep, [c["len"]])
    s.ptr(rep + s.off_data, data + c["off"])
    ans = s.call(c["fn"], obj)
    if c["fn"] in ("nsize", "wsize"):
        return {"ans": ans & MASK, "log": s.log()}
    return {"ans": (ans - data) & MASK, "log": s.log()}


OPS = {"register": op_register, "gridcell": op_gridcell, "dtor": op_dtor, "setpos": op_setpos, "gridctor": op_gridctor,
       "ctype_dtor": op_ctype_dtor, "map": op_map, "narrow": op_narrow, "cursor_copy": op_curcopy, "destroy": op_destroy,
       "makeit": op_makeit, "dequector": op_dequector, "deref": op_deref, "nocase_copy": op_nocopy, "init": op_init, "str": op_str}


def run(s, case):
    try:
        return OPS[case["op"]](s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def W(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def H(r, n):
    return [r.getrandbits(16) for _ in range(n)]


def case(op, **kw):
    c = {"op": op, "queues": {"adv": [0], "idx": [0]}}
    c.update(kw)
    return c


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF]


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    return fb(r.uniform(-500, 500))


S16 = [-32768, -1, 0, 1, 2, 32767]


def gen(fn_edges, fn_rand, seed):
    def edges():
        r = random.Random(seed)
        return list(fn_edges(r))

    def rnd(r):
        return fn_rand(r)
    return edges, rnd


# Register
def reg_edges(r):
    for sk in (0, 1, 2):
        yield case("register", sk=sk)


def reg_rand(r):
    return case("register", sk=r.randrange(3))


# GetMapGridCell
def grid_edges(r):
    for x in (-1, 0, 1, 64, 127, 128, 129, -100, 1 << 30, -(1 << 31)):
        for y in (-1, 0, 1, 64, 127, 128, 129, -100, 1 << 30):
            yield case("gridcell", x=x & MASK, y=y & MASK)


def grid_rand(r):
    return case("gridcell", x=r.randrange(-6, 135) & MASK, y=r.randrange(-6, 135) & MASK)


# destructors
def dtor_edges(r):
    for this in (0, 1):
        for flag in S16:
            yield case("dtor", this=this, flag=flag & MASK, w=W(r, 5))


def dtor_rand(r):
    return case("dtor", this=r.randrange(2), flag=r.choice(S16 + [r.randrange(-40000, 40000)]) & MASK, w=W(r, 5))


def cdtor_edges(r):
    for this in (0, 1):
        for flag in S16:
            yield case("ctype_dtor", this=this, flag=flag & MASK, w=W(r, 4))


def cdtor_rand(r):
    return case("ctype_dtor", this=r.randrange(2), flag=r.choice(S16) & MASK, w=W(r, 4))


# SetPosition
def setpos_edges(r):
    for inst in (0, 1):
        for sp in SPECIAL:
            yield case("setpos", inst=inst, vec=[sp, rf(r), sp])


def setpos_rand(r):
    return case("setpos", inst=r.randrange(2), vec=[rf(r), rf(r), rf(r)])


# grid cell constructor
def gc_edges(r):
    yield case("gridctor", w=W(r, 8))


def gc_rand(r):
    return case("gridctor", w=W(r, 8))


# ctype maps
CHARS = [0, 1, 0x41, 0x61, 0x7F, 0x80, 0xFE, 0xFF, 0x100, 0x101, 0x8000, 0xFFFF]


def map_case(r, fn, c):
    return case("map", fn=fn, c=c, upper=H(r, 256), lower=H(r, 256))


def upper_edges(r):
    for c in CHARS:
        yield map_case(r, "toupper", c)


def upper_rand(r):
    return map_case(r, "toupper", r.choice(CHARS) if r.random() < 0.3 else r.randrange(0x10000) if r.random() < 0.5 else r.randrange(0x110))


def lower_edges(r):
    for c in CHARS:
        yield map_case(r, "tolower", c)


def lower_rand(r):
    return map_case(r, "tolower", r.choice(CHARS) if r.random() < 0.3 else r.randrange(0x10000) if r.random() < 0.5 else r.randrange(0x110))


def narrow_edges(r):
    for c in CHARS + [0x7E, 0xFD]:
        for d in (0, 0x41, 0x80, 0xFF):
            yield case("narrow", c=c, d=d)


def narrow_rand(r):
    return case("narrow", c=r.choice(CHARS) if r.random() < 0.5 else r.randrange(0x10000), d=r.randrange(256))


# cursor copy
def cc_edges(r):
    yield case("cursor_copy", d=W(r, 4), s=W(r, 3))
    yield case("cursor_copy", d=[0] * 4, s=[MASK] * 3)


def cc_rand(r):
    return case("cursor_copy", d=W(r, 4), s=W(r, 3))


# __destroy
def destroy_edges(r):
    for a in range(5):
        for b in range(5):
            for extra in (0, 5):
                yield case("destroy", a=a, b=b, extra=extra)


def destroy_rand(r):
    return case("destroy", a=r.randrange(8), b=r.randrange(8), extra=r.choice([0, 0, 1, 11, 12]))


# make_iterator
def make_case(r, bs, index, count, start):
    c = case("makeit", bs=bs, index=index, count=count, start=start, w1=r.getrandbits(32),
             table=[r.randrange(0x1000, 0x7FFFFFFF) for _ in range(16)])
    c["queues"] = {"adv": [r.randrange(0, 8)], "idx": [r.getrandbits(32)]}
    return c


def make_edges(r):
    for bs in (1, 2, 42):
        for index in (0, bs - 1, bs, bs + 1, 2 * bs + 3):
            for count in (0, 1):
                for start in (0, 3):
                    yield make_case(r, bs, index, count, start)


def make_rand(r):
    bs = r.randrange(1, 60)
    return make_case(r, bs, r.randrange(0, 5 * bs), r.randrange(2), r.randrange(0, 9))


# nocase_string ctor
def nc_edges(r):
    yield case("nocase_copy")


def nc_rand(r):
    return case("nocase_copy")


# init
def init_case(r, n, odd, term):
    c = case("init", chars=H(r, 40), bytes=2 * n + odd, term=term)
    return c


def init_edges(r):
    for n in range(0, 14):
        for odd in (0, 1):
            yield init_case(r, n, odd, r.choice([0, 0x41, 0xFFFF]))


def init_rand(r):
    return init_case(r, r.randrange(0, 30), r.randrange(2), r.getrandbits(16))


# string accessors
def str_case(r, fn, ln):
    return case("str", fn=fn, len=ln, off=r.choice([0, 2, 8, 100]))


def make_str(fn):
    def edges(r):
        for ln in (0, 1, 2, 0x7FFFFFFF, 0xFFFFFFFF):
            yield str_case(r, fn, ln)

    def rnd(r):
        return str_case(r, fn, r.getrandbits(32))
    return edges, rnd


# deque ctor
def dq_edges(r):
    for bs in (0, 0x2A, 1, MASK):
        yield case("dequector", bs=bs, w=W(r, 8))


def dq_rand(r):
    return case("dequector", bs=r.getrandbits(32), w=W(r, 8))


# iterator deref
def deref_edges(r):
    yield case("deref", w=[0] * 6)
    yield case("deref", w=[MASK] * 6)


def deref_rand(r):
    return case("deref", w=W(r, 6))


def FT(name, edge_fn, rand_fn, seed):
    edges, rnd = gen(edge_fn, rand_fn, seed)
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


_ARG = "std::vector<Argument,std::allocator<Argument>>"
_DQ = "std::deque<%s,std::allocator<%s>>" % (_ARG, _ARG)
_NC = "std::basic_string<wchar_t,nocase_wchar_traits,std::allocator<wchar_t>>"
_PL = "std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>"
_CD = "std::__cdeque<%s*,std::allocator<%s*>>" % (_ARG, _ARG)
ne, nr = make_str("nsize")
nbe, nbr = make_str("nbegin")
we, wr = make_str("wsize")
wbe, wbr = make_str("wbegin")

TESTS = [
    FT("CUI2DMap::Register", reg_edges, reg_rand, 1),
    FT("CUI2DMap::GetMapGridCell", grid_edges, grid_rand, 2),
    FT("CUI2DMap::~CUI2DMap", dtor_edges, dtor_rand, 3),
    FT("CUI2DMapSprite::SetPosition", setpos_edges, setpos_rand, 4),
    FT("TUI2DMapMapGridCell::TUI2DMapMapGridCell", gc_edges, gc_rand, 5),
    FT("std::ctype<wchar_t>::~ctype", cdtor_edges, cdtor_rand, 6),
    FT("std::ctype<wchar_t>::do_toupper", upper_edges, upper_rand, 7),
    FT("std::ctype<wchar_t>::do_tolower", lower_edges, lower_rand, 8),
    FT("std::ctype<wchar_t>::do_narrow", narrow_edges, narrow_rand, 9),
    FT(_CD + "::iterator::iterator", cc_edges, cc_rand, 10),
    FT("std::__destroy<%s*>" % _ARG, destroy_edges, destroy_rand, 11),
    FT(_DQ + "::make_iterator", make_edges, make_rand, 12),
    FT("nocase_string::nocase_string", nc_edges, nc_rand, 13),
    FT(_NC + "::init", init_edges, init_rand, 14),
    FT(_NC + "::size", ne, nr, 15),
    FT(_NC + "::begin", nbe, nbr, 16),
    FT(_PL + "::size", we, wr, 17),
    FT(_PL + "::begin", wbe, wbr, 18),
    FT(_DQ + "::deque", dq_edges, dq_rand, 19),
    FT(_DQ + "::const_iterator::operator*", deref_edges, deref_rand, 20),
]
