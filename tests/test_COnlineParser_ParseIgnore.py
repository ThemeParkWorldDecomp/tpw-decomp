"""Unit v5555, COnlineParser::ParseIgnore and DoParse, against libtpw_unit_COnlineParser_ParseIgnore.dylib
(tests/unit_COnlineParser_ParseIgnore.cpp), main app, src/COnlineParser_ParseGoto_COnlineParser_ParseIgnore.cpp:
  ParseIgnore 0x1020fbd4, DoParse 0x1020e608

A case builds a parser object (arguments vector pointer, text pointer) twice, calls the function and compares the answer, the
event log (every callee is a recording fake; wide strings are opaque words, the Argument words are logged at push_back) and the
words of the text objects. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineParser_ParseIgnore"
MASK = 0xFFFFFFFF

FN_IGNORE, FN_DOPARSE = 0x1020FBD4, 0x1020E608
SIZES = {FN_IGNORE: 360, FN_DOPARSE: 544}
OWN = set(SIZES)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def w(e, a):
    return e.u32(a)


def put(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def s_string_ctor(e):
    e.cl.rec("StringCtor", ())
    put(e, e.r(3), 0x11110000)


def s_wstring_ctor(e):
    e.cl.rec("WStringCtor", ())
    put(e, e.r(3), e.cl.ret("wctor") & MASK)


def s_wstring_dtor(e):
    e.cl.rec("WStringDtor", (w(e, e.r(3)), s32(e.r(4))))


def s_string_dtor(e):
    e.cl.rec("StringDtor", (w(e, e.r(3)), s32(e.r(4))))


def s_wstring_copy(e):
    v = w(e, e.r(4))
    e.cl.rec("WStringCopy", (v, s32(e.r(5)), s32(e.r(6)) & MASK))
    put(e, e.r(3), v + 0x100)


def s_argument_ctor(e):
    v = w(e, e.r(4))
    e.cl.rec("ArgumentCtor", (v,))
    a = e.r(3)
    put(e, a, 0x22220000)
    put(e, a + 4, v + 0x1000)
    put(e, a + 8, 0x77)


def s_push(e):
    a = e.r(4)
    e.cl.rec("PushBack", (e.cl.lab(e.r(3)), w(e, a), w(e, a + 4), w(e, a + 8)))


def s_get_string(e):
    e.cl.rec("GetString", (e.cl.lab(e.r(3)), w(e, e.r(4))))
    put(e, e.r(4), e.cl.ret("gsval") & MASK)
    e.set_r(3, 1 if e.cl.ret("GetString") else 0)


def s_ignore(e):
    e.cl.rec("IgnoreRemaining", (e.cl.lab(e.r(3)),))
    e.set_r(3, 1 if e.cl.ret("IgnoreRemaining") else 0)


def s_setup(e):
    p = e.r(6)
    e.cl.rec("Setup", (e.cl.lab(e.r(3)), w(e, e.r(4)), e.cl.lab(e.r(5)), e.read(p, 1)[0], e.read(p + 1, 1)[0], e.read(p + 2, 1)[0]))


def s_skip(e):
    e.cl.rec("SkipWhitespace", (e.cl.lab(e.r(3)),))


def s_getchar(e):
    e.cl.rec("GetChar", (e.cl.lab(e.r(3)),))
    e.write(e.r(4), struct.pack(">H", e.cl.ret("ch") & 0xFFFF))
    e.set_r(3, 1 if e.cl.ret("GetChar") else 0)


def s_parse(e):
    e.cl.rec("Parse", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Parse") & MASK)


def s_assign(e):
    v = w(e, e.r(4))
    e.cl.rec("Assign", (e.cl.lab(e.r(3)), v, s32(e.r(5)), s32(e.r(6)) & MASK))
    put(e, e.r(3), v + 0x55)


for addr, fn in ((0x1007363C, s_string_ctor), (0x101E0B84, s_wstring_ctor), (0x101468F8, s_wstring_dtor),
                 (0x1000221C, s_string_dtor), (0x101F7424, s_wstring_copy), (0x102011A8, s_argument_ctor),
                 (0x10201478, s_push), (0x10211B3C, s_get_string), (0x10211EB0, s_ignore), (0x102113DC, s_setup),
                 (0x10211860, s_skip), (0x10211EF8, s_getchar), (0x102114F8, s_parse), (0x101F7014, s_assign)):
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    args_off, text_off = 0x3C, 0x40

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = STUBS.activate(e)

    def begin(self, this, vec, dest):
        self.e.cl = CL.PpcLog({this: 1, vec: 2, dest: 3, this + 0x30: 4}, self.case["queues"])

    def ptr(self, a, v):
        self.e.write(a, struct.pack(">I", v))

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.e.cl.log

    def ignore(self, this):
        with_ = T.call_ppc(self.e, FN_IGNORE, [this])
        return with_[0] & 0xFF

    def doparse(self, this, text):
        T.call_ppc(self.e, FN_DOPARSE, [this, text])
        return 0


class Host(Side):
    fmt = "<I"
    args_off, text_off = 0x40, 0x48

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, this, vec, dest):
        CL.host_begin(self.lib, {1: this, 2: vec, 3: dest, 4: this + 0x30}, self.case["queues"])

    def ptr(self, a, v):
        C.memmove(a, struct.pack("<Q", v), 8)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def ignore(self, this):
        return self.lib.tpw_pi_ParseIgnore(this) & 0xFF

    def doparse(self, this, text):
        self.lib.tpw_pi_DoParse(this, text)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pi_alloc.restype, lib.tpw_pi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pi_ParseIgnore.restype, lib.tpw_pi_ParseIgnore.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pi_DoParse.restype, lib.tpw_pi_DoParse.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pi_layout(out)
    assert list(out)[:n] == [0x40, 0x48, 0], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        this = s.alloc(0x60)
        vec = s.alloc(16)
        dest = s.alloc(4)
        text = s.alloc(4)
        s.begin(this, vec, dest)
        s.ptr(this + s.args_off, vec)
        s.ptr(this + s.text_off, dest)
        s.write(dest, struct.pack(">I" if s.fmt == ">I" else "<I", case["dest"]))
        s.write(text, struct.pack(s.fmt, case["text"]))
        ans = s.ignore(this) if case["op"] == "ignore" else s.doparse(this, text)
        return {"answer": ans, "log": s.log(), "dest": s.rd_word(dest), "text": s.rd_word(text)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op, **kw):
    q = {"wctor": [r.randrange(1 << 32)], "gsval": [r.randrange(1 << 32)], "GetString": [r.choice([0, 1])],
         "IgnoreRemaining": [r.choice([0, 1])], "ch": [r.choice([0x2F, 0x2F, 0x41, 0, 0x2E, 0x30, 0xFF2F])],
         "GetChar": [r.choice([0, 1, 1])], "Parse": [r.choice([0, 1, -1, 5]) & MASK]}
    for k, v in kw.items():
        q[k] = [v]
    return {"op": op, "queues": q, "dest": r.randrange(1 << 32), "text": r.randrange(1 << 32)}


def ign_edges():
    r = random.Random(11)
    for gs in (0, 1):
        for ig in (0, 1):
            yield base(r, "ignore", GetString=gs, IgnoreRemaining=ig)


def ign_random(r):
    return base(r, "ignore")


def dp_edges():
    r = random.Random(12)
    for gc in (0, 1):
        for ch in (0x2F, 0x2E, 0x12F, 0x2F00):
            for pa in (0, 1, MASK):
                yield base(r, "doparse", GetChar=gc, ch=ch, Parse=pa)


def dp_random(r):
    return base(r, "doparse")


TESTS = [
    FnTest("COnlineParser::ParseIgnore", ppc, host, edge=ign_edges, rnd=ign_random),
    FnTest("COnlineParser::DoParse", ppc, host, edge=dp_edges, rnd=dp_random),
]
