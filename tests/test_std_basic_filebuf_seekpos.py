"""Unit v5233, std::basic_filebuf<char>::seekpos / seekoff / overflow, against libtpw_unit_std_basic_filebuf_seekpos.dylib
(tests/unit_std_basic_filebuf_seekpos.cpp), main app, src/game/MslSupport.cpp:
  seekpos 0x10070eb4, seekoff 0x10071058, overflow 0x10071294

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory, big-endian, and host arena memory).
Callees outside the unit are fakes on both sides recording the same events: __ptr_glue (codecvt encoding() through vtable slot 0x18 and
out() through slot 0xc, told apart by a magic value in the slot), unshift, fseek, ftell, fputc and the string init/assign/resize/dtor
(the string body is built in each memory the same way). Answers come from per-case queues.

The world of a case: a filebuf object (get pointers, FILE word, flags, pushback bytes), a codecvt object with a fake vtable. What is
compared: the answer (fpos pair or int), the object after the call (pointers relative to the object), the event log.

No fused operations. Not tested: the dead loop after the multibyte loop of overflow (the flag it tests is never changed, so it cannot be
reached); the queue for the codecvt always ends with the error answer in the multibyte path, or the original would loop forever.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_seekpos"
MASK = 0xFFFFFFFF

FN_SEEKPOS, FN_SEEKOFF, FN_OVERFLOW = 0x10070EB4, 0x10071058, 0x10071294
SIZES = {FN_SEEKPOS: 304, FN_SEEKOFF: 444, FN_OVERFLOW: 720}
OWN = set(SIZES)

PTR_GLUE = 0x1023BAA8
UNSHIFT = 0x10072A94
FSEEK, FTELL, FPUTC = 0x102388F0, 0x10238908, 0x102388A8
STR_INIT, STR_ASSIGN, STR_RESIZE, STR_DTOR = 0x10072E6C, 0x100730E8, 0x10073558, 0x1000221C
MAGIC_ENC, MAGIC_OUT = 0xC0DE18, 0xC0DE0C

HOST_LAYOUT = [0x24, 0x28, 0x2C, 0x3E, 0x3F, 0x40]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def q_enc(e):
    e.cl.rec("Enc", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Enc") & MASK)


def q_out(e):
    L = e.cl
    state, frm, frm_end, fnext, to, to_end, tnext = e.r(4), e.r(5), e.r(6), e.r(7), e.r(8), e.r(9), e.r(10)
    L.rec("Out", (L.lab(e.r(3)), L.lab(state - 0x28), e.read(frm, 1)[0], frm_end - frm, to_end - to))
    r = L.ret("Out") & MASK
    fa, ta, b = L.ret("OutFa"), L.ret("OutTa"), L.ret("OutByte")
    e.write(fnext, struct.pack(">I", (frm + fa) & MASK))
    e.write(tnext, struct.pack(">I", (to + ta) & MASK))
    if to_end > to:
        e.write(to, bytes([b & 0xFF]))
    e.set_r(3, r)


def stub_glue(e):
    r12 = e.r(12)
    if r12 == MAGIC_ENC:
        q_enc(e)
    elif r12 == MAGIC_OUT:
        q_out(e)
    else:
        raise AssertionError("glue r12 %x" % r12)


def stub_unshift(e):
    e.cl.rec("Unshift", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Unshift") & MASK)


def stub_fseek(e):
    e.cl.rec("Fseek", (e.r(3), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, e.cl.ret("Fseek") & MASK)


def stub_ftell(e):
    e.cl.rec("Ftell", (e.r(3),))
    e.set_r(3, e.cl.ret("Ftell") & MASK)


def stub_fputc(e):
    e.cl.rec("Fputc", (s32(e.r(3)), e.r(4)))
    e.set_r(3, e.cl.ret("Fputc") & MASK)


def rep_of(e, sp):
    rep = e.u32(sp)
    return rep, list(struct.unpack(">IIII", e.read(rep, 16)))


def stub_str_init(e):
    sp, n, c = e.r(3), e.r(4) & MASK, e.r(5) & 0xFF
    e.cl.rec("StrInit", (n, c))
    rep = e.alloc(16)
    data = e.alloc(n + 32)
    e.write(data, bytes([c]) * n)
    e.write(rep, struct.pack(">IIII", n, n, 0, data))
    e.write(sp, struct.pack(">I", rep))


def stub_str_assign(e):
    sp, n, c = e.r(3), e.r(4) & MASK, e.r(5) & 0xFF
    rep, (ln, cap, refs, data) = rep_of(e, sp)
    e.cl.rec("StrAssign", (ln, n, c))
    e.write(data, bytes([c]) * n + b"\0")
    e.write(rep, struct.pack(">I", n))


def stub_str_resize(e):
    sp, n, c = e.r(3), e.r(4) & MASK, e.r(5) & 0xFF
    rep, (ln, cap, refs, data) = rep_of(e, sp)
    e.cl.rec("StrResize", (ln, n, c))
    if n > cap:
        nd = e.alloc(n + 32)
        e.write(nd, e.read(data, ln))
        data, cap = nd, n
    if n > ln:
        e.write(data + ln, bytes([c]) * (n - ln))
    e.write(data + n, b"\0")
    e.write(rep, struct.pack(">IIII", n, cap, refs, data))


def stub_str_dtor(e):
    e.cl.rec("StrDtor", (s32(e.r(4)),))


STUBS.internal(PTR_GLUE, stub_glue)
STUBS.internal(UNSHIFT, stub_unshift)
STUBS.internal(FSEEK, stub_fseek)
STUBS.internal(FTELL, stub_ftell)
STUBS.internal(FPUTC, stub_fputc)
STUBS.internal(STR_INIT, stub_str_init)
STUBS.internal(STR_ASSIGN, stub_str_assign)
STUBS.internal(STR_RESIZE, stub_str_resize)
STUBS.internal(STR_DTOR, stub_str_dtor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def setup_objects(self):
        self.fb = self.alloc(0x48)
        self.cvt = self.alloc(16)
        self.vt = self.alloc(0x40)
        self.res = self.alloc(8)
        self.cvtword = self.cvt if self.fmt == ">I" else 0x7C0DE   # host: opaque handle (arena is not 32-bit addressable)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.setup_objects()
        self.L = e.cl = CL.PpcLog({self.fb: 1, self.cvt: 2}, case["queues"])
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

    def seekpos(self, off, state, mode):
        T.call_ppc(self.e, FN_SEEKPOS, [self.res, self.fb, off & MASK, state & MASK, mode & MASK])
        return self.rd_words(self.res, 2)

    def seekoff(self, off, dirn, mode):
        T.call_ppc(self.e, FN_SEEKOFF, [self.res, self.fb, off & MASK, dirn & MASK, mode & MASK])
        return self.rd_words(self.res, 2)

    def overflow(self, c):
        r3, _, _ = T.call_ppc(self.e, FN_OVERFLOW, [self.fb, c & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.setup_objects()
        CL.host_begin(lib, {1: self.fb}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_fbso_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def seekpos(self, off, state, mode):
        v = self.lib.tpw_fbso_seekpos(self.fb, s32(off), s32(state), mode & MASK)
        return [v & MASK, v >> 32]

    def seekoff(self, off, dirn, mode):
        v = self.lib.tpw_fbso_seekoff(self.fb, s32(off), s32(dirn), mode & MASK)
        return [v & MASK, v >> 32]

    def overflow(self, c):
        return self.lib.tpw_fbso_overflow(self.fb, s32(c)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fbso_alloc.restype, lib.tpw_fbso_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fbso_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fbso_seekpos.restype = C.c_uint64
    lib.tpw_fbso_seekpos.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_fbso_seekoff.restype = C.c_uint64
    lib.tpw_fbso_seekoff.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_fbso_overflow.restype = C.c_int32
    lib.tpw_fbso_overflow.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fbso_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    fb = s.fb
    s.words(fb, [0, fb + case["eback"], fb + case["gptr"], fb + case["egptr"]])
    s.words(fb + 0x24, [case["file"], case["state"], s.cvtword])
    s.write(fb + 0x30, bytes(case["buf"]))
    s.write(fb + 0x3E, bytes([case["f3e"], case["unbuf"]]))
    s.words(s.cvt, [s.vt])
    s.words(s.vt + 0xC, [MAGIC_OUT])
    s.words(s.vt + 0x18, [MAGIC_ENC])


def execute(s, case):
    a = case["args"]
    if case["op"] == "seekpos":
        return s.seekpos(*a)
    if case["op"] == "seekoff":
        return s.seekoff(*a)
    return s.overflow(*a)


def observe(s, case, answer):
    fb = s.fb
    ptrs = [(w - fb) & MASK for w in s.rd_words(fb + 4, 3)]
    return {"answer": answer, "log": s.log(), "ptrs": ptrs,
            "mid": s.rd_words(fb + 0x10, 7), "tail": s.read(fb + 0x30, 0x10)}


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


class Case(dict):
    def __repr__(self):
        return "%s%s" % (self["op"], self["args"])


def base(r, op, enc=None, multi=False):
    enc = r.choice([-1, 0, 1, 2, 3, -5]) if enc is None else enc
    if op == "overflow" and enc == 1:
        out = [r.choice([-1, 0, 1, 2, 3, 4, 7])]
    else:
        out = [r.choice([0, 1, 3, 4, -1]) for _ in range(r.randrange(0, 5))] + [2]
    q = {
        "Enc": [enc],
        "Unshift": [r.choice([0, 1, 0x100, 0x101])],
        "Fseek": [r.choice([0, 0, -1, 3])],
        "Ftell": [r.choice([0, 17, 1000, -4])],
        "Fputc": [r.choice([-1, 65, 0, 200])],
        "Out": out,
        "OutFa": [r.choice([0, 1])],
        "OutTa": [r.randrange(0, 5) for _ in range(3)],
        "OutByte": [r.randrange(0, 256) for _ in range(3)],
    }
    return Case({"op": op, "queues": q, "file": r.choice([0, 0x4000, 0x4000, 0x12345678]), "state": r.randrange(0, 1 << 32),
                 "eback": r.randrange(0, 0x30), "gptr": r.randrange(0, 0x30), "egptr": r.randrange(0, 0x30),
                 "buf": [r.randrange(0, 256) for _ in range(14)], "f3e": r.choice([0, 1, 2]), "unbuf": r.choice([0, 1, 5]),
                 "args": None})


OFFS = [0, 1, -1, 5, -7, 1000, 0x7FFFFFFF, -0x80000000]


def seekpos_case(r, **kw):
    c = base(r, "seekpos", kw.get("enc"))
    c["args"] = [r.choice(OFFS), r.randrange(0, 1 << 32), r.choice([0, 8, 0x10, 0x18, 0x1F, 4, r.randrange(0, 64)])]
    for k in ("file", "f3e", "mode", "unshift", "fseek"):
        if k in kw:
            if k == "file":
                c["file"] = kw[k]
            elif k == "f3e":
                c["f3e"] = kw[k]
            elif k == "mode":
                c["args"][2] = kw[k]
            elif k == "unshift":
                c["queues"]["Unshift"] = [kw[k]]
            elif k == "fseek":
                c["queues"]["Fseek"] = [kw[k]]
    return c


def seekpos_edges():
    r = random.Random(11)
    for file in (0, 0x4000):
        for mode in (0, 8, 0x10, 0x18, 0x1F, 4):
            for enc in (-1, 0, 1):
                for f3e in (0, 1):
                    for un in (0, 1, 0x100):
                        for fs in (0, 1):
                            yield seekpos_case(r, file=file, mode=mode, enc=enc, f3e=f3e, unshift=un, fseek=fs)


def seekoff_case(r, off=None, dirn=None, mode=None, enc=None, file=None, f3e=None):
    c = base(r, "seekoff", enc)
    c["args"] = [r.choice(OFFS) if off is None else off, r.randrange(-1, 7) if dirn is None else dirn,
                 r.choice([0, 8, 0x10, 0x18, 8 | 4, r.randrange(0, 64)]) if mode is None else mode]
    if file is not None:
        c["file"] = file
    if f3e is not None:
        c["f3e"] = f3e
    return c


def seekoff_edges():
    r = random.Random(12)
    for off in (0, 5, -7):
        for dirn in range(-1, 7):
            for enc in (-1, 0, 1):
                for mode in (0, 8):
                    for f3e in (0, 1):
                        for file in (0, 0x4000):
                            yield seekoff_case(r, off, dirn, mode, enc, file, f3e)


def overflow_case(r, c=None, enc=None, file=None, unbuf=None):
    cs = base(r, "overflow", enc)
    cs["args"] = [r.choice([-1, 0x41, 0xFF, 0x80, 0x1234, 0, 0x7F, -2]) if c is None else c]
    if file is not None:
        cs["file"] = file
    if unbuf is not None:
        cs["unbuf"] = unbuf
    return cs


def overflow_edges():
    r = random.Random(13)
    for c in (-1, 0x41, 0xFF, 0x80, 0x1234, 0):
        for file in (0, 0x4000):
            for unbuf in (0, 1):
                for enc in (1, 0, -3):
                    for _ in range(3):
                        yield overflow_case(r, c, enc, file, unbuf)


TESTS = [
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::seekpos", ppc, host, edge=seekpos_edges, rnd=seekpos_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::seekoff", ppc, host, edge=seekoff_edges, rnd=seekoff_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::overflow", ppc, host, edge=overflow_edges, rnd=overflow_case),
]
