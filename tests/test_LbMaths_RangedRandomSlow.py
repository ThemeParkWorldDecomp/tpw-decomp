"""Unit v51112, small functions of the sound program (random, LB_MIN/LB_MAX, static initialisers, CAutoArrayPtr<CAudioPlaceHolder*>,
CFile::Write, the CDiscFileStorage destructor ...), against libtpw_unit_LbMaths_RangedRandomSlow.dylib
(tests/unit_LbMaths_RangedRandomSlow.cpp), src/AudioPlaceHolder_LbMaths_Random_LbMaths_RangedRandomSlow.cpp (the sound program,
PROGRAM "sound symbols shared"):
  LbMaths_RangedRandomSlow 0x10018c88, bfl::maths::RangedRandomSlow 0x10018c30, bfl::maths::Random 0x10019ad0, RotateRight<long> 0x10018d6c,
  LB_MAX/LB_MIN 0x100196b0 0x1001a17c 0x1001bdec 0x1001e470 0x100112f8 0x1000e7d8, TbRect::operator= 0x1001c31c, operator_new 0x1001d248,
  CChannelIterator::operator++ 0x1000fed8, CFormat::operator const tWAVEFORMATEX& 0x1000214c, std::logf 0x1001a134, the five __sinit
  functions, CAutoArrayPtr<CAudioPlaceHolder*> 0x1001bce8 0x1001bc44 0x1001c110, CFile::Write 0x1001d9bc, CDiscFileStorage dtor 0x10020c50.

A case is a world and a call, built by one piece of Python code on both memories (the emulator's big-endian image and the host
library's). The callees outside the unit are fakes on both sides that record the same events: the runtime glue (register
0x1002eefc, seed 0x1002eaac, new[] 0x1002ebfc, delete[] 0x1002ebcc, delete 0x1002ea94, base destructor 0x1002eeb4, virtual-call glue
0x1002f154), FillWaveFormatEx; log (0x1002f094) is the libm log on both sides.

Compared: the answer, the memory the function writes, the event log. Notes: the host's operator_new and the channel iterator are
the sound program's own spellings in the unit's file (the existing ones sit in files that do not link into a test library). A zero range
of RangedRandomSlow is not tested (divwu by 0 is undefined on the hardware). The vtable word the CDiscFileStorage destructor stores is
checked on the PowerPC side only (the host's vtable is the compiler's). Array sizes on the host are 8-byte pointers: the new[] fake
logs the element count on both sides. No fused operations.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_LbMaths_RangedRandomSlow"
MASK = 0xFFFFFFFF

PPC_FN = dict(
    rrs=0x10018c88, brrs=0x10018c30, brand=0x10019ad0, rotr=0x10018d6c,
    lbmax_ul=0x100196b0, lbmin_f=0x1001a17c, lbmin_ui=0x1001bdec, lbmax_l=0x1001e470, lbmin_i1=0x100112f8, lbmin_i2=0x1000e7d8,
    tbrect=0x1001c31c, opnew=0x1001d248, itinc=0x1000fed8, fmt=0x1000214c, logf=0x1001a134,
    sinit_apl=0x1001bae4, sinit_wav=0x1001144c, sinit_mpeg=0x1000eccc, sinit_cb=0x1000d5a4, sinit_loop=0x1000b530,
    aapc=0x1001bce8, aapd=0x1001bc44, aapo=0x1001c110, write=0x1001d9bc, dfs=0x10020c50,
)
SIZES = dict(rrs=56, brrs=32, brand=32, rotr=20, lbmax_ul=24, lbmin_f=24, lbmin_ui=24, lbmax_l=24, lbmin_i1=24, lbmin_i2=24,
             tbrect=36, opnew=8, itinc=40, fmt=48, logf=40, sinit_apl=44, sinit_wav=48, sinit_mpeg=48, sinit_cb=48, sinit_loop=48,
             aapc=60, aapd=100, aapo=8, write=64, dfs=104)
OWN = set(PPC_FN.values()) | {0x10018cf4}

CU32, CI32, CU64, CF, CI16 = C.c_uint32, C.c_int32, C.c_uint64, C.c_float, C.c_int16
# key -> (C name, restype, argtypes); a pointer is a c_uint64
HOST_FN = dict(
    rrs=("tpw_v_rrs", CU32, [CU64, CU32]), brrs=("tpw_v_brrs", CU32, [CU64, CU32]), brand=("tpw_v_brand", CU32, [CU64]),
    rotr=("tpw_v_rotr", CI32, [CI32, CI32]),
    lbmax_ul=("tpw_v_lbmax_ul", CU64, [CU64, CU64]), lbmin_f=("tpw_v_lbmin_f", CU64, [CU64, CU64]),
    lbmin_ui=("tpw_v_lbmin_ui", CU64, [CU64, CU64]), lbmax_l=("tpw_v_lbmax_l", CU64, [CU64, CU64]),
    lbmin_i1=("tpw_v_lbmin_i", CU64, [CU64, CU64]), lbmin_i2=("tpw_v_lbmin_i", CU64, [CU64, CU64]),
    tbrect=("tpw_v_tbrect", CU64, [CU64, CU64]), opnew=("tpw_v_opnew", CU64, [CU32, CU64]), itinc=("tpw_v_itinc", CU64, [CU64]),
    fmt=("tpw_v_fmt", CU64, [CU64]), logf=("tpw_v_logf", CF, [CF]),
    sinit_apl=("tpw_v_sinit_apl", None, []), sinit_wav=("tpw_v_sinit_wav", None, []), sinit_mpeg=("tpw_v_sinit_mpeg", None, []),
    sinit_cb=("tpw_v_sinit_cb", None, []), sinit_loop=("tpw_v_sinit_loop", None, []),
    aapc=("tpw_v_aapc", CU64, [CU64, CU32]), aapd=("tpw_v_aapd", CU64, [CU64, CI16]), aapo=("tpw_v_aapo", CU64, [CU64]),
    write=("tpw_v_write", CI32, [CU64, CU64, CU32]), dfs=("tpw_v_dfs", CU64, [CU64, CI16]),
)

# the Mac's addresses of the globals
ADDR = dict(seed=0x1003f35c, wavobj=0x1003f2cc, wavnode=0x1003f2d0, mpegobj=0x1003f260, mpegnode=0x1003f264,
            cbobj=0x1003ec3c, cbnode=0x1003ec40, loopobj=0x1003ec18, loopnode=0x1003ec1c)
DTOR_ID = {0x10033ec4: 1, 0x10033ca4: 2, 0x10033b64: 3, 0x10033984: 4}
VT_DISC = 0x1003abf0 + 0x22d8
HL = {}   # host layout: impl offset of CFile


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def glue(addr, fn):
    """A glue function: the caller reloads r2 from 0x14(r1) after it, so the stub saves it there as the glue does."""
    def run(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    STUBS.internal(addr, run)


def st_register(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), DTOR_ID.get(e.r(4) & MASK, -1), L.lab(e.r(5))))


def st_seed(e):
    L = e.cl
    L.rec("seed", (0,))
    e.set_r(3, L.ret("seed") & MASK)


def st_fill(e):
    L = e.cl
    L.rec("Fill", (L.lab(e.r(3)),))


def st_newarr(e):
    L = e.cl
    n = e.r(3) & MASK
    L.rec("new[]", (n >> 2,))
    e.set_r(3, e.alloc(n if n else 4))


def st_delarr(e):
    e.cl.rec("delete[]", (1 if e.r(3) & MASK else 0,))


def st_del(e):
    e.cl.rec("delete", (1 if e.r(3) & MASK else 0,))


def st_basedtor(e):
    e.cl.rec("~CFileStorage", (1, e.r(4) & MASK))


def st_write(e):
    L = e.cl
    L.rec("Write", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))
    e.set_r(3, L.ret("Write") & MASK)


def st_log(e):
    x = e.f(1)
    if x != x:
        y = float("nan")
    elif x == 0:
        y = -math.inf
    elif x < 0:
        y = float("nan")
    elif x == math.inf:
        y = math.inf
    else:
        y = math.log(x)
    e.set_f1(y)


glue(0x1002eefc, st_register)
glue(0x1002eaac, st_seed)
STUBS.internal(0x100021b0, st_fill)
glue(0x1002ebfc, st_newarr)
glue(0x1002ebcc, st_delarr)
glue(0x1002ea94, st_del)
glue(0x1002eeb4, st_basedtor)
glue(0x1002f154, st_write)
glue(0x1002f094, st_log)


def setup(runner):
    T.install_std_stubs(runner.emu)
    ADDR["acc"] = T.persist_alloc(runner.emu, 0x40, struct.pack(">I", 0x10001000) * 16)
    ADDR["buf"] = T.persist_alloc(runner.emu, 0x40, bytes(range(64)))
    # the access object's word 0 is its vtable (slot 0x10 is the Write the glue stub stands for)
    vt = T.persist_alloc(runner.emu, 0x40, struct.pack(">I", 0x10001000) * 16)
    runner.emu.write(ADDR["acc"], struct.pack(">I", vt))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    ptrsz = 4
    impl_off = 4
    host = False

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR["acc"]: 7, ADDR["buf"]: 8}
        for i, k in enumerate(("wavobj", "wavnode", "mpegobj", "mpegnode", "cbobj", "cbnode", "loopobj", "loopnode")):
            fixed[ADDR[k]] = 10 + i
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

    def wptr(self, a, p):
        self.write(a, struct.pack(">I", p & MASK))

    def rptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def log(self):
        return self.L.log

    def vt_ok(self, obj):
        return self.rd_word(obj) == VT_DISC if obj else True

    def call(self, key, *args):
        a = [T.F(x) if isinstance(x, float) else x & MASK for x in args]
        r3, _, f1 = T.call_ppc(self.e, PPC_FN[key], a)
        if HOST_FN[key][1] is CF:
            return canon_f(fb(f1))
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ptrsz = 8
    host = True

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.impl_off = HL["impl"]
        names = ("wavobj", "wavnode", "mpegobj", "mpegnode", "cbobj", "cbnode", "loopobj", "loopnode")
        fixed = {7: self.g("acc"), 8: self.g("buf")}
        for i, k in enumerate(names):
            fixed[10 + i] = self.g(k)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def vt_ok(self, obj):
        return True

    def call(self, key, *args):
        r = getattr(self.lib, HOST_FN[key][0])(*args)
        rt = HOST_FN[key][1]
        if rt is CF:
            return canon_f(fb(r))
        if rt is None:
            return 0
        if rt is CU64:
            return r
        return r & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v_addr.restype, lib.tpw_v_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v_alloc.restype, lib.tpw_v_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, rt, at in HOST_FN.values():
        f = getattr(lib, name)
        f.restype, f.argtypes = rt, at
    out = (C.c_int64 * 8)()
    n = lib.tpw_v_layout(out)
    HL["pipe"], HL["impl"] = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the bodies


def rel(ret, base, stride=1):
    return -1 if ret == 0 else (ret - base) // stride


def b_rand(s, c):
    a = s.alloc(4)
    s.words(a, [c["seed"]])
    if c["op"] == "brand":
        r = s.call("brand", a)
    else:
        r = s.call(c["op"], a, c["range"])
    return {"answer": r, "seed": s.rd_words(a, 1)}


def b_ptr2(s, c):
    a = s.alloc(8)
    s.words(a, [c["a"], c["b"]])
    r = s.call(c["op"], a, a + 4)
    return {"answer": rel(r, a), "mem": s.rd_words(a, 2)}


def b_rotr(s, c):
    return {"answer": s.call("rotr", c["v"], c["n"])}


def b_logf(s, c):
    return {"answer": s.call("logf", c["x"])}


def b_sinit(s, c):
    if c["op"] == "sinit_apl":
        s.words(s.g("seed"), [c["pre"]])
    r = s.call(c["op"])
    return {"seed": s.rd_words(s.g("seed"), 1),
            "words": [s.rd_word(s.g(k)) for k in ("wavobj", "wavnode", "mpegobj", "mpegnode", "cbobj", "cbnode", "loopobj", "loopnode")]}


def b_tbrect(s, c):
    d = s.alloc(16)
    sr = s.alloc(16)
    s.words(d, c["d"])
    s.words(sr, c["s"])
    r = s.call("tbrect", d, sr)
    return {"answer": rel(r, d) if r != d else 0, "dest": s.rd_words(d, 4), "src": s.rd_words(sr, 4)}


def b_opnew(s, c):
    w = s.alloc(4)
    r = s.call("opnew", c["size"], w)
    return {"answer": 1 if r == w else 0}


def b_itinc(s, c):
    H = 0x60
    it = s.alloc(16)
    base = s.alloc(H * 8)
    s.wptr(it, base + H * c["i"])
    s.wptr(it + s.ptrsz, base + H * c["j"])
    r = s.call("itinc", it)
    return {"answer": rel(r, base, H) if r else -1, "cur": (s.rptr(it) - base) // H, "end": (s.rptr(it + s.ptrsz) - base) // H}


def b_fmt(s, c):
    o = s.alloc(32)
    r = s.call("fmt", o)
    return {"answer": r - o}


def b_write(s, c):
    f = s.alloc(32)
    if c["impl"]:
        s.wptr(f + s.impl_off, s.g("acc"))
    r = s.call("write", f, s.g("buf"), c["size"])
    return {"answer": T.s32(r)}


def b_dfs(s, c):
    o = s.alloc(32) if c["obj"] else 0
    if o:
        s.words(o, [0x11111111, 0x22222222, 0x33333333, 0x44444444])
    r = s.call("dfs", o, c["flag"])
    return {"answer": 0 if r == o else 1, "vt": s.vt_ok(o), "tail": s.rd_words(o + 8, 2) if o else 0}


def b_aapc(s, c):
    o = s.alloc(8)
    r = s.call("aapc", o, c["count"])
    return {"answer": r - o, "set": s.rptr(o) != 0}


def b_aapd(s, c):
    o = s.alloc(8) if c["obj"] else 0
    if o and c["data"]:
        s.wptr(o, s.alloc(8))
    r = s.call("aapd", o, c["flag"])
    return {"answer": 0 if r == o else 1}


def b_aapo(s, c):
    o = s.alloc(8)
    d = s.alloc(8) if c["data"] else 0
    s.wptr(o, d)
    r = s.call("aapo", o)
    return {"answer": 0 if r == d else 1}


BODY = {}
for k in ("rrs", "brrs", "brand"):
    BODY[k] = b_rand
for k in ("lbmax_ul", "lbmin_f", "lbmin_ui", "lbmax_l", "lbmin_i1", "lbmin_i2"):
    BODY[k] = b_ptr2
for k in ("sinit_apl", "sinit_wav", "sinit_mpeg", "sinit_cb", "sinit_loop"):
    BODY[k] = b_sinit
BODY.update(rotr=b_rotr, logf=b_logf, tbrect=b_tbrect, opnew=b_opnew, itinc=b_itinc, fmt=b_fmt, write=b_write, dfs=b_dfs,
            aapc=b_aapc, aapd=b_aapd, aapo=b_aapo)


def run(s, c):
    try:
        out = BODY[c["op"]](s, c)
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases

INTS = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678, 0x80000001, 0xFFFFFFFE]
FLOATS = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x40000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x7F7FFFFF]


def base(r, op, **kw):
    c = {"op": op, "queues": {"Write": [r.choice([0, 1, 5, -1 & MASK, 64])], "seed": [r.choice(INTS + [r.getrandbits(32)])]}}
    c.update(kw)
    return c


def mk(name, op, edges, rnd):
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


def rint(r):
    return r.choice(INTS) if r.random() < 0.5 else r.getrandbits(32)


# -- random
def gen_rand(op):
    def one(r, seed=None, rng=None):
        c = base(r, op, seed=rint(r) if seed is None else seed)
        c["range"] = (r.choice([1, 2, 3, 7, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]) if r.random() < 0.5
                      else r.randrange(1, 1 << r.randrange(1, 33))) if rng is None else rng
        return c

    def edges():
        r = random.Random(11)
        for seed in INTS:
            for rng in (1, 2, 10, 0x80000000, 0xFFFFFFFF):
                yield one(r, seed, rng)

    return edges, lambda r: one(r)


def gen_brand():
    def edges():
        r = random.Random(12)
        for seed in INTS:
            yield base(r, "brand", seed=seed)
    return edges, lambda r: base(r, "brand", seed=rint(r))


# -- pointer pairs
def gen_ptr2(op, vals):
    def one(r, a=None, b=None):
        return base(r, op, a=r.choice(vals) if a is None else a, b=r.choice(vals) if b is None else b)

    def edges():
        r = random.Random(21)
        for a in vals:
            for b in vals:
                yield one(r, a, b)

    return edges, lambda r: one(r) if r.random() < 0.5 else base(r, op, a=r.getrandbits(32), b=r.getrandbits(32))


def gen_rotr():
    def one(r, v=None, n=None):
        return base(r, "rotr", v=rint(r) if v is None else v, n=r.choice([0, 1, 13, 31, 32, 33, 63, 64, 65, -1 & MASK, r.getrandbits(32)]) if n is None else n)

    def edges():
        r = random.Random(31)
        for v in INTS:
            for n in (0, 1, 13, 31, 32, 33, 63, 64, 65, 0xFFFFFFFF, 0xFFFFFFE0, 100):
                yield one(r, v, n)

    return edges, lambda r: one(r)


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(FLOATS)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(0.0001, 3000))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def gen_logf():
    def edges():
        r = random.Random(41)
        for x in FLOATS + [fb(2.718281828), fb(0.5), fb(10.0), 0x00800000, 0x007FFFFF]:
            yield base(r, "logf", x=bitsf(x))

    return edges, lambda r: base(r, "logf", x=bitsf(rf(r)))


def gen_sinit(op):
    def one(r):
        return base(r, op, pre=rint(r))

    def edges():
        r = random.Random(51)
        for _ in range(4):
            yield one(r)

    return edges, one


def gen_tbrect():
    def one(r):
        return base(r, "tbrect", d=[rint(r) for _ in range(4)], s=[rint(r) for _ in range(4)])

    def edges():
        r = random.Random(61)
        yield base(r, "tbrect", d=[0] * 4, s=[1, 2, 3, 4])
        yield base(r, "tbrect", d=[1, 2, 3, 4], s=[0x80000000, 0xFFFFFFFF, 0, 0x7FFFFFFF])
        yield base(r, "tbrect", d=[5] * 4, s=[5] * 4)

    return edges, one


def gen_opnew():
    def one(r):
        return base(r, "opnew", size=rint(r))

    def edges():
        r = random.Random(71)
        for n in (0, 1, 0x20, 0xFFFFFFFF):
            yield base(r, "opnew", size=n)

    return edges, one


def gen_itinc():
    def one(r):
        return base(r, "itinc", i=r.randrange(0, 6), j=r.randrange(0, 7))

    def edges():
        r = random.Random(81)
        for i, j in ((0, 0), (0, 1), (0, 2), (1, 2), (2, 2), (5, 2), (3, 4), (6, 7), (4, 3)):
            yield base(r, "itinc", i=i, j=j)

    return edges, one


def gen_fmt():
    def edges():
        yield base(random.Random(91), "fmt")

    return edges, lambda r: base(r, "fmt")


def gen_write():
    def one(r):
        return base(r, "write", impl=r.random() < 0.8, size=rint(r))

    def edges():
        r = random.Random(101)
        for impl in (False, True):
            for size in (0, 1, 64, 0xFFFFFFFF):
                yield base(r, "write", impl=impl, size=size)

    return edges, one


FLAGS = [0, 1, -1 & MASK, 0x10000, 0x10001, 0x8000, 0x7FFF, 5, 0xFFFF]


def gen_dfs():
    def one(r):
        return base(r, "dfs", obj=r.random() < 0.85, flag=r.choice(FLAGS))

    def edges():
        r = random.Random(111)
        yield base(r, "dfs", obj=False, flag=1)
        yield base(r, "dfs", obj=False, flag=0)
        for f in FLAGS:
            yield base(r, "dfs", obj=True, flag=f)

    return edges, one


def gen_aapc():
    def one(r):
        return base(r, "aapc", count=r.choice([0, 1, 2, 3, 10, 100, r.randrange(0, 500)]))

    def edges():
        r = random.Random(121)
        for n in (0, 1, 2, 16, 1000):
            yield base(r, "aapc", count=n)

    return edges, one


def gen_aapd():
    def one(r):
        return base(r, "aapd", obj=r.random() < 0.85, data=r.random() < 0.7, flag=r.choice(FLAGS))

    def edges():
        r = random.Random(131)
        yield base(r, "aapd", obj=False, data=False, flag=1)
        for data in (False, True):
            for f in FLAGS:
                yield base(r, "aapd", obj=True, data=data, flag=f)

    return edges, one


def gen_aapo():
    def edges():
        r = random.Random(141)
        for d in (False, True):
            yield base(r, "aapo", data=d)

    return edges, lambda r: base(r, "aapo", data=r.random() < 0.6)


U32V = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF, 0xFFFFFFFE, 0x12345678]
F32V = FLOATS + [fb(1.5), fb(-1.5), fb(2.0), 0x7FC00001]


def T_(name, op, gen):
    e, rn = gen
    return mk(name, op, e, rn)


TESTS = [
    T_("LbMaths_RangedRandomSlow", "rrs", gen_rand("rrs")),
    T_("bfl::maths::RangedRandomSlow", "brrs", gen_rand("brrs")),
    T_("bfl::maths::Random", "brand", gen_brand()),
    T_("SamsUtilities::UBinary::RotateRight<long>", "rotr", gen_rotr()),
    T_("LB_MAX<unsigned_long,unsigned_long>", "lbmax_ul", gen_ptr2("lbmax_ul", U32V)),
    T_("LB_MIN<float,float>", "lbmin_f", gen_ptr2("lbmin_f", F32V)),
    T_("LB_MIN<unsigned_int,unsigned_int>", "lbmin_ui", gen_ptr2("lbmin_ui", U32V)),
    T_("LB_MAX<long,long>", "lbmax_l", gen_ptr2("lbmax_l", U32V)),
    T_("LB_MIN<int,int>@0x100112f8", "lbmin_i1", gen_ptr2("lbmin_i1", U32V)),
    T_("LB_MIN<int,int>@0x1000e7d8", "lbmin_i2", gen_ptr2("lbmin_i2", U32V)),
    T_("std::logf", "logf", gen_logf()),
    T_("__sinit_AudioPlaceHolder_cpp", "sinit_apl", gen_sinit("sinit_apl")),
    T_("__sinit_TbSoundSampleWAV_cpp", "sinit_wav", gen_sinit("sinit_wav")),
    T_("__sinit_TbSoundSampleMPEG2_cpp", "sinit_mpeg", gen_sinit("sinit_mpeg")),
    T_("__sinit_TbSoundSampleCallback_cpp", "sinit_cb", gen_sinit("sinit_cb")),
    T_("__sinit_TbSoundCodecLoop_cpp", "sinit_loop", gen_sinit("sinit_loop")),
    T_("TbRect::operator=", "tbrect", gen_tbrect()),
    T_("operator_new", "opnew", gen_opnew()),
    T_("TbSoundSampleStore::CChannelIterator::operator++", "itinc", gen_itinc()),
    T_("CFormat::operator_const_tWAVEFORMATEX&", "fmt", gen_fmt()),
    T_("CAutoArrayPtr<CAudioPlaceHolder*>::CAutoArrayPtr", "aapc", gen_aapc()),
    T_("CAutoArrayPtr<CAudioPlaceHolder*>::~CAutoArrayPtr", "aapd", gen_aapd()),
    T_("CAutoArrayPtr<CAudioPlaceHolder*>::operator_CAudioPlaceHolder**", "aapo", gen_aapo()),
    T_("bfl::FileSystem::CFile::Write", "write", gen_write()),
    T_("bfl::FileSystem::CDiscFileStorage::~CDiscFileStorage", "dfs", gen_dfs()),
]
