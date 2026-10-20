"""Unit v51075, small functions of the sound library, against libtpw_unit_TbSpeaker_Mono_operator_int.dylib
(tests/unit_TbSpeaker_Mono_operator_int.cpp), sound symbols shared, src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp:
  TbSpeaker::Mono/Headphones/Stereo::operator_int, TbSpeaker::Config::SetModuleSpeaker, __sinit_ChannelState_cpp, CBank::GetSampleName,
  GetSampleLength, IsType, TbSoundSampleStore::operator[] (src/TbSoundSampleStore.cpp), __sinit_UserCallback_cpp, __sinit_TimerCodec_cpp,
  __sinit_TbSoundSampleWAVFile_cpp, the three _CS_ destructors, CRefManager::CRefManager, TbStaticFileBank::IsType,
  TbDynamicStringTemplate<char>::operator_const_char*, std::powf, LB_MIN<int,int>, std::fabs (two), std::fabsf, TbRect::TbRect.
Unit v5L21 adds Bank::CBank::GetNumberOfSamples 0x100146a8 (slot +0x18, logged as 0x7018) and LB_MIN<unsigned int,unsigned int>
0x100081c4 (the header template, unsigned compare).

A case is a world and a call, built twice by one piece of Python code (emulator memory for the original, the host library's for the
translation). The callees outside the unit are fakes on both sides that record the same events: the virtual-call glue 0x1002f154
(the speaker and the bank are objects whose slots log a marker, the object and the argument), the free 0x1002ea94, the runtime's
__register_global_object 0x1002eefc, pow 0x1002ee54 (the answer comes from the case), the three ClearCache, CChannelState::CChannelState.

Compared: the answer, the event log, the speaker global, the blocks written. No fused operations. Not tested: a NULL speaker or a NULL
destructor object past the checks (the original reads through them unconditionally); signalling NaN arguments of the float functions
(the processors convert them differently).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_TbSpeaker_Mono_operator_int"
MASK = 0xFFFFFFFF

A = dict(mono=0x10014b14, head=0x10014aa8, stereo=0x10014a44, setspk=0x100149e8, sinit_cs=0x10014d1c, name=0x10014810,
         length=0x10014798, istype=0x1001463c, store=0x10015478, sinit_uc=0x10013ed0, d_timer=0x100139c8, sinit_timer=0x1001396c,
         refmgr=0x10013824, sfb=0x1001330c, nametext=0x10016bd8, powf=0x10012580, d_wavfile=0x10011c48, sinit_wavfile=0x10011be4,
         lbmin=0x10011a98, lbminu=0x100081c4, count=0x100146a8, fabs_f=0x10017bf8, fabsf=0x10017c38, fabs_d=0x10017c7c, rect=0x10018158, d_wav=0x100114ac)
GLUE, FREE, REG, POW, CSCTOR = 0x1002f154, 0x1002ea94, 0x1002eefc, 0x1002ee54, 0x10014d6c
CLR_TIMER, CLR_WAV, CLR_WAVFILE, STORE_CONV = 0x100133f8, 0x10010e34, 0x10011534, 0x1000fd90
ADDR = dict(mpSpeaker=0x1003f348)
SELF = 0x7F000
TAGS = {0x10034164: 1, 0x1003410c: 2, 0x10033f1c: 3}
BANK_M = (0x7020, 0x7024, 0x7030)
HOST_LAYOUT = [16]

SPK_ADDR = 0
BANK_ADDR = 0


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def canon_d(bits):
    return 0x7FF8000000000000 if (bits & 0x7FF0000000000000) == 0x7FF0000000000000 and (bits & 0xFFFFFFFFFFFFF) else bits


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def db(x):
    return struct.unpack(">Q", struct.pack(">d", x))[0]


def bitsd(b):
    return struct.unpack(">d", struct.pack(">Q", b))[0]


# ---------------------------------------------------------------------------------------------------------------- the fakes
STUBS = CL.Stubs()
STUBS.real.update(A.values())
STUBS.real.add(STORE_CONV)


def stub_glue(e):
    L = e.cl
    m = e.r(12)
    a = (m, L.lab(e.r(3)))
    if m in BANK_M:
        a += (T.s32(e.r(4)),)
    L.rec("vcall", a)
    e.set_r(3, L.ret("vcall") & MASK)


def stub_free(e):
    e.cl.rec("free", (e.cl.lab(e.r(3)),))


def stub_reg(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), TAGS.get(e.r(4), 0), L.lab(e.r(5))))


def stub_pow(e):
    L = e.cl
    a, b = T.f64_bits(e.f(1)), T.f64_bits(e.f(2))
    L.rec("pow", (a >> 32, a & MASK, b >> 32, b & MASK))
    hi, lo = L.ret("powhi") & MASK, L.ret("powlo") & MASK
    e.set_f1(bitsd((hi << 32) | lo))


def stub_csctor(e):
    e.cl.rec("CChannelState", (e.cl.lab(e.r(3)),))


STUBS.internal(GLUE, stub_glue)
STUBS.internal(FREE, stub_free)
STUBS.internal(REG, stub_reg)
STUBS.internal(POW, stub_pow)
STUBS.internal(CSCTOR, stub_csctor)
STUBS.internal(CLR_TIMER, lambda e: e.cl.rec("ClearTimer", (0,)))
STUBS.internal(CLR_WAV, lambda e: e.cl.rec("ClearWAV", (0,)))
STUBS.internal(CLR_WAVFILE, lambda e: e.cl.rec("ClearWAVFile", (0,)))


def setup(runner):
    global SPK_ADDR, BANK_ADDR
    e = runner.emu
    T.install_std_stubs(e)
    spk_vt = T.persist_alloc(e, 32, struct.pack(">5I", 0, 0, 0x7008, 0x700C, 0x7010) + bytes(12))
    SPK_ADDR = T.persist_alloc(e, 16, struct.pack(">I", spk_vt) + bytes(12))
    bank_vt = T.persist_alloc(e, 0x40, bytes(0x18) + struct.pack(">I", 0x7018) + bytes(4) + struct.pack(">I", 0x7020) + struct.pack(">I", 0x7024) + bytes(8)
                              + struct.pack(">I", 0x7030) + bytes(12))
    BANK_ADDR = T.persist_alloc(e, 16, struct.pack(">I", bank_vt) + bytes(12))


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
    ppc = True

    def __init__(self, e, case):
        self.e = e
        fixed = {SPK_ADDR: 1, BANK_ADDR: 2, 0x1003F350: 3, SELF: 4, 0x1003F338: 5, 0x1003F33C: 6, 0x1003F314: 7, 0x1003F318: 8,
                 0x1003F2F0: 9, 0x1003F2F4: 10}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call(self, addr, args):
        r3, _, f1 = T.call_ppc(self.e, addr, args)
        return r3 & MASK, f1


class Host(Side):
    fmt = "<I"
    ppc = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        g = lambda n: lib.tpw_u_addr(n.encode())
        CL.host_begin(lib, {1: g("speaker"), 2: g("bank"), 3: g("channel_state"), 4: SELF, 5: g("ucobj"), 6: g("ucnode"),
                            7: g("tcobj"), 8: g("tcnode"), 9: g("wfobj"), 10: g("wfnode")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_u_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lambda name, res, args: (setattr(getattr(lib, name), "restype", res), setattr(getattr(lib, name), "argtypes", args))
    f("tpw_u_addr", C.c_uint64, [C.c_char_p])
    f("tpw_u_alloc", C.c_uint64, [C.c_uint32])
    lib.tpw_u_layout.argtypes = [C.POINTER(C.c_int64)]
    f("tpw_u_speaker_op", C.c_uint32, [C.c_int32])
    f("tpw_u_put_speaker", None, [C.c_uint64])
    f("tpw_u_set_module_speaker", None, [C.c_uint64])
    f("tpw_u_get_speaker", C.c_uint64, [])
    f("tpw_u_sinit", None, [C.c_int32])
    f("tpw_u_bank", C.c_uint64, [C.c_int32, C.c_int32, C.c_uint32])
    f("tpw_u_store_index", C.c_uint32, [C.c_uint64, C.c_uint32])
    f("tpw_u_destruct", C.c_uint64, [C.c_int32, C.c_uint64, C.c_int32])
    f("tpw_u_refmgr", None, [C.c_uint64])
    f("tpw_u_static_istype", C.c_uint32, [C.c_uint32])
    f("tpw_u_nametext", C.c_uint64, [C.c_uint32])
    f("tpw_u_powf", C.c_float, [C.c_float, C.c_float])
    f("tpw_u_fabs_f", C.c_float, [C.c_float])
    f("tpw_u_fabsf", C.c_float, [C.c_float])
    f("tpw_u_fabs_d", C.c_double, [C.c_double])
    f("tpw_u_lbminu", C.c_uint32, [C.c_uint64, C.c_uint64])
    f("tpw_u_lbmin", C.c_uint32, [C.c_uint64, C.c_uint64])
    f("tpw_u_rect", None, [C.c_uint64])
    out = (C.c_int64 * 32)()
    n = lib.tpw_u_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the call
SINIT = dict(sinit_cs=0, sinit_uc=1, sinit_timer=2, sinit_wavfile=3)
SPEAKER = dict(mono=0, head=1, stereo=2)
BANK = dict(name=0, length=1, istype=2, count=3)
DTOR = dict(d_timer=0, d_wavfile=1, d_wav=2)


def execute(s, case):
    op, a = case["op"], case["args"]
    P = s.ppc
    obs = {}
    if op in SPEAKER:
        if P:
            s.words(ADDR["mpSpeaker"], [SPK_ADDR])
            r = s.call(A[op], [0x12345])[0]
        else:
            r = s.lib.tpw_u_speaker_op(SPEAKER[op])
        obs["answer"] = r & MASK
    elif op == "setspk":
        garbage, p = a
        if P:
            s.words(ADDR["mpSpeaker"], [garbage])
            s.call(A[op], [p])
            obs["speaker"] = s.rd_word(ADDR["mpSpeaker"])
        else:
            s.lib.tpw_u_put_speaker(garbage)
            s.lib.tpw_u_set_module_speaker(p)
            obs["speaker"] = s.lib.tpw_u_get_speaker()
    elif op in SINIT:
        if P:
            s.call(A[op], [])
        else:
            s.lib.tpw_u_sinit(SINIT[op])
    elif op in BANK:
        has, idx = a
        if P:
            blk = s.alloc(16)
            s.words(blk, [0, 0, BANK_ADDR if has else 0, 0])
            r = s.call(A[op], [blk, idx])[0]
        else:
            r = s.lib.tpw_u_bank(BANK[op], has, idx)
        obs["answer"] = r & MASK if op != "name" else r
        if op == "istype":
            obs["answer"] = r & 0xFF
    elif op == "store":
        base, idx = a
        if P:
            blk = s.alloc(16)
            s.words(blk, [0x11111111, 0x22222222, 0x33333333, base])
            d = (s.call(A[op], [blk, idx])[0] - base) & MASK
            r = d // 0x60 if d % 0x60 == 0 else 0xFFFFFFFF
        else:
            r = s.lib.tpw_u_store_index(base, idx)
        obs["answer"] = r
    elif op in DTOR:
        has, flag = a
        me = SELF if has else 0
        if P:
            r = s.call(A[op], [me, flag])[0]
        else:
            r = s.lib.tpw_u_destruct(DTOR[op], me, flag)
        obs["answer"] = r & MASK
    elif op == "refmgr":
        blk = s.alloc(16)
        s.words(blk, a)
        if P:
            s.call(A[op], [blk])
        else:
            s.lib.tpw_u_refmgr(blk)
        obs["w0"] = s.rd_word(blk)
        obs["w2"] = s.rd_word(blk + 8)
        obs["w3"] = s.rd_word(blk + 12)
        # the host owner pointer is two words: both must be zero (the Mac's is one word)
        obs["zero"] = s.rd_word(blk) == 0 and (P or s.rd_word(blk + 4) == 0)
    elif op == "sfb":
        if P:
            r = s.call(A[op], [0x5000, a[0]])[0]
        else:
            r = s.lib.tpw_u_static_istype(a[0])
        obs["answer"] = r & MASK
    elif op == "nametext":
        if P:
            blk = s.alloc(16)
            s.words(blk, [0xAAAA0001, 0xAAAA0002, a[0], 0xAAAA0004])
            r = s.call(A[op], [blk])[0]
        else:
            r = s.lib.tpw_u_nametext(a[0])
        obs["answer"] = r
    elif op == "powf":
        if P:
            f1 = s.call(A[op], [T.F(bitsf(a[0])), T.F(bitsf(a[1]))])[1]
            obs["answer"] = canon_f(fb(f1))
        else:
            obs["answer"] = canon_f(fb(s.lib.tpw_u_powf(bitsf(a[0]), bitsf(a[1]))))
    elif op in ("fabs_f", "fabsf"):
        if P:
            f1 = s.call(A[op], [T.F(bitsf(a[0]))])[1]
            obs["answer"] = canon_f(fb(f1))
        else:
            fn = s.lib.tpw_u_fabs_f if op == "fabs_f" else s.lib.tpw_u_fabsf
            obs["answer"] = canon_f(fb(fn(bitsf(a[0]))))
    elif op == "fabs_d":
        if P:
            f1 = s.call(A[op], [T.D(bitsd(a[0]))])[1]
        else:
            f1 = s.lib.tpw_u_fabs_d(bitsd(a[0]))
        obs["answer"] = canon_d(db(f1))
    elif op in ("lbmin", "lbminu"):
        blk = s.alloc(16)
        s.words(blk, [a[0], a[1], 0, 0])
        if P:
            r = s.call(A[op], [blk, blk + 4])[0]
            obs["answer"] = 0 if r == blk else (1 if r == blk + 4 else 2)
        else:
            obs["answer"] = (s.lib.tpw_u_lbmin if op == "lbmin" else s.lib.tpw_u_lbminu)(blk, blk + 4)
    elif op == "rect":
        blk = s.alloc(16)
        s.words(blk, a)
        if P:
            s.call(A[op], [blk])
        else:
            s.lib.tpw_u_rect(blk)
        obs["rect"] = s.rd_words(blk, 4)
    obs["log"] = s.log()
    return obs


def run(s, case):
    try:
        return execute(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases
def case(op, args=(), queues=None):
    return {"op": op, "args": list(args), "queues": queues or {}}


def u32(r):
    return r.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(16)])


VALUES = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x12345678]
FLAGS = [0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x10000, 0x18000, 0xFFFFFFFF, 0x80000000, 5]
SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0x80000001,
           0xFF7FFFFF]


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


DSPECIAL = [0, 0x8000000000000000, 0x7FF8000000000000, 0x7FF0000000000000, 0xFFF0000000000000, 1, 0x3FF0000000000000,
            0xBFF0000000000000, 0x47EFFFFFE0000000, 0x47EFFFFFE0000001, 0x47EFFFFFEFFFFFFF, 0x47EFFFFFF0000000, 0x3810000000000000,
            0x380FFFFFFFFFFFFF, 0x36A0000000000000, 0x3690000000000000, 0x7FEFFFFFFFFFFFFF, 0x0010000000000000]


def rd(r):
    k = r.random()
    if k < 0.25:
        return r.choice(DSPECIAL)
    if k < 0.7:
        return db(r.uniform(-8, 8))
    return db(r.uniform(-1, 1) * 10 ** r.uniform(-60, 60))


def pow_q(r, bits=None):
    b = rd(r) if bits is None else bits
    return {"powhi": [b >> 32], "powlo": [b & MASK]}


def mk(name, op, edges, rnd):
    return FnTest(name, ppc, host, edge=lambda: [case(op, a, q) for a, q in edges()], rnd=lambda r: case(op, *rnd(r)))


def speaker_edges():
    return [((), {"vcall": [v]}) for v in VALUES]


def speaker_rnd(r):
    return (), {"vcall": [u32(r)]}


def none_edges():
    return [((), {})]


def none_rnd(r):
    return (), {}


def setspk_edges():
    return [((g, p), {}) for g in (0, 0xDEADBEEF) for p in VALUES]


def setspk_rnd(r):
    return (u32(r), u32(r)), {}


def bank_edges(vals):
    return lambda: [((has, idx), {"vcall": [v]}) for has in (0, 1) for idx in (0, 1, 0xFFFFFFFF, 7) for v in vals]


def bank_rnd(vals):
    return lambda r: ((r.choice([0, 1, 1]), u32(r)), {"vcall": [r.choice(vals) if vals else u32(r)]})


def store_edges():
    return [((base, idx), {}) for base in (0x1000, 0x40000000, 0x12345678) for idx in (0, 1, 2, 0x1FFFFFF, 100)]


def store_rnd(r):
    return (r.randrange(0x1000, 0x40000000), r.choice([0, 1, r.randrange(0, 0x2000000)])), {}


def dtor_edges():
    return [((has, f), {}) for has in (0, 1) for f in FLAGS]


def dtor_rnd(r):
    return (r.choice([0, 1, 1]), r.choice(FLAGS + [r.getrandbits(32)])), {}


def refmgr_edges():
    return [((0, 0, 0, 0), {}), ((0xFFFFFFFF, 0xFFFFFFFF, 1, 2), {}), ((0x12345678, 0, 0x9ABCDEF0, 3), {})]


def refmgr_rnd(r):
    return tuple(u32(r) for _ in range(4)), {}


def type_edges():
    return [((t,), {}) for t in (0, 1, 2, 3, 4, 5, 0xFFFFFFFF, 0x80000003)]


def type_rnd(r):
    return (r.choice([3, 3, 2, 4, u32(r)]),), {}


def val_edges():
    return [((v,), {}) for v in VALUES + [8, 0x1003F338]]


def val_rnd(r):
    return (u32(r),), {}


def powf_edges():
    out = []
    for x in (0, 0x3F800000, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x40000000):
        for q in (0, 0x3FF0000000000000, 0x47EFFFFFE0000000, 0x47EFFFFFF0000000, 0x7FF8000000000000, 0x3690000000000000,
                  0x36A0000000000000, 0x380FFFFFFFFFFFFF, 0xFFF0000000000000):
            out.append(((x, 0x40000000), pow_q(None, q)))
    return out


def powf_rnd(r):
    return (rf(r), rf(r)), pow_q(r)


def float_edges():
    return [((x,), {}) for x in SPECIAL]


def float_rnd(r):
    return (rf(r),), {}


def double_edges():
    return [((x,), {}) for x in DSPECIAL]


def double_rnd(r):
    return (rd(r),), {}


def lbmin_edges():
    v = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 5]
    return [((a & MASK, b & MASK), {}) for a in v for b in v]


def lbmin_rnd(r):
    return (r.choice([0, -1, 1, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)]) & MASK,
            r.choice([0, -1, 1, r.randrange(-1000, 1000), r.randrange(-2**31, 2**31)]) & MASK), {}


TESTS = [
    mk("TbSpeaker::Mono::operator_int", "mono", speaker_edges, speaker_rnd),
    mk("TbSpeaker::Headphones::operator_int", "head", speaker_edges, speaker_rnd),
    mk("TbSpeaker::Stereo::operator_int", "stereo", speaker_edges, speaker_rnd),
    mk("TbSpeaker::Config::SetModuleSpeaker", "setspk", setspk_edges, setspk_rnd),
    mk("__sinit_ChannelState_cpp", "sinit_cs", none_edges, none_rnd),
    mk("Bank::CBank::GetSampleName", "name", bank_edges(VALUES), bank_rnd([])),
    mk("Bank::CBank::GetSampleLength", "length", bank_edges(VALUES), bank_rnd([])),
    mk("Bank::CBank::IsType", "istype", bank_edges([0, 1]), bank_rnd([0, 1])),
    mk("Bank::CBank::GetNumberOfSamples", "count", bank_edges(VALUES), bank_rnd([])),
    mk("TbSoundSampleStore::operator[]", "store", store_edges, store_rnd),
    mk("__sinit_UserCallback_cpp", "sinit_uc", none_edges, none_rnd),
    mk("_CS_TbTimerCodec::~_CS_TbTimerCodec", "d_timer", dtor_edges, dtor_rnd),
    mk("__sinit_TimerCodec_cpp", "sinit_timer", none_edges, none_rnd),
    mk("CRefManager::CRefManager", "refmgr", refmgr_edges, refmgr_rnd),
    mk("TbStaticFileBank::IsType", "sfb", type_edges, type_rnd),
    mk("TbDynamicStringTemplate<char>::operator_const_char*", "nametext", val_edges, val_rnd),
    mk("std::powf", "powf", powf_edges, powf_rnd),
    mk("_CS__TbSoundSampleWAVFile::~_CS__TbSoundSampleWAVFile", "d_wavfile", dtor_edges, dtor_rnd),
    mk("__sinit_TbSoundSampleWAVFile_cpp", "sinit_wavfile", none_edges, none_rnd),
    mk("LB_MIN<int,int>", "lbmin", lbmin_edges, lbmin_rnd),
    mk("LB_MIN<unsigned_int,unsigned_int>", "lbminu", lbmin_edges, lbmin_rnd),
    mk("std::fabs@0x10017bf8", "fabs_f", float_edges, float_rnd),
    mk("std::fabsf", "fabsf", float_edges, float_rnd),
    mk("std::fabs@0x10017c7c", "fabs_d", double_edges, double_rnd),
    mk("TbRect::TbRect", "rect", refmgr_edges, refmgr_rnd),
    mk("_CS__TbSoundSampleWAV::~_CS__TbSoundSampleWAV", "d_wav", dtor_edges, dtor_rnd),
]
