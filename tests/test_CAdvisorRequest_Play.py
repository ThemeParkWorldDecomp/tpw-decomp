"""Unit v51003, CAdvisorRequest::Play and PlayWithoutScore, against libtpw_unit_CAdvisorRequest_Play.dylib
(tests/unit_CAdvisorRequest_Play.cpp), main app, src/game/advisor/Advisor.cpp:
  CAdvisorRequest::Play 0x1000dff4, CAdvisorRequest::PlayWithoutScore 0x1000e340

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory big-endian, host little-endian).
The callees outside the unit (the scoring object's LastMinuteCheck, GetNumResponsesFor, GetFirstResponseFor, ADV_Call, ADV_GetTag, the
message constructor, PostMessage, and __construct_array which the host sees as 29 subscription header constructors) are fakes on both
sides that record their arguments and answer from the case's queues.

The world: a request object (valid flag, ids, message), the "no thing" id global, the scoring guard byte (set, so the scoring object
exists) and its +0x20 word (3, the constant the host uses), gMessageCentre (null or a preset object). Compared: the answer, the length
word, the whole request, the message centre's state (null / preset / newly made), the event log.

No fused operations. Not tested: the scoring object's lazy construction (the guard byte is always set; the host's singleton is the
test's fake and cannot record the constructor and __register_global_object events), and operator new returning NULL (never happens).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorRequest_Play"
MASK = 0xFFFFFFFF

FN_PLAY, FN_PWS = 0x1000DFF4, 0x1000E340
SIZES = {FN_PLAY: 684, FN_PWS: 492}
OWN = set(SIZES)

CALLEE_LMC, CALLEE_NUM, CALLEE_FIRST = 0x100102D8, 0x1001055C, 0x10010464
CALLEE_CALL, CALLEE_TAG, CALLEE_MSG, CALLEE_POST = 0x1000887C, 0x10009050, 0x10151F7C, 0x1015256C

ADDR = dict(guard=0x102B4100, scoring=0x102B3AC8, mc=0x1037F6C4, nullid=0x1027DE64)
HOST_LAYOUT = [4, 0x14, 0x18]
MC = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def be_h(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def be_w(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def stub_lmc(e):
    L = e.cl
    L.rec("LastMinuteCheck", (L.lab(e.r(3)), s32(e.r(4))))
    if L.ret("lmc_write"):
        be_h(e, e.r(5), L.ret("lmc_id"))
        be_w(e, e.r(6), L.ret("lmc_val"))
    e.set_r(3, L.ret("lmc") & MASK)


def stub_num(e):
    L = e.cl
    L.rec("GetNumResponsesFor", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("num") & MASK)


def stub_first(e):
    L = e.cl
    L.rec("GetFirstResponseFor", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("first") & MASK)


def stub_call(e):
    L = e.cl
    L.rec("ADV_Call", (s32(e.r(3)),))
    e.set_r(3, L.ret("call") & MASK)


def stub_tag(e):
    L = e.cl
    L.rec("ADV_GetTag", (s32(e.r(3)),))
    e.set_r(3, L.ret("tag") & MASK)


def stub_msg(e):
    L = e.cl
    obj = e.r(3)
    for i, off in enumerate((8, 0xC, 0x10, 0x14)):
        be_w(e, obj + off, e.r(4 + i))
    L.rec("CMsgTag", (s32(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, s32(e.r(7))))


def stub_post(e):
    L = e.cl
    m = e.r(4)
    L.rec("PostMessage", (L.lab(e.r(3)), s32(e.u32(m + 8)), e.u32(m + 0xC), e.u32(m + 0x10), s32(e.u32(m + 0x14))))


def stub_construct(e):
    for _ in range(e.r(7)):
        e.cl.rec("sub_ctor", ())


for a, f in ((CALLEE_LMC, stub_lmc), (CALLEE_NUM, stub_num), (CALLEE_FIRST, stub_first), (CALLEE_CALL, stub_call),
             (CALLEE_TAG, stub_tag), (CALLEE_MSG, stub_msg), (CALLEE_POST, stub_post)):
    STUBS.internal(a, f)
STUBS.imp("__construct_array", stub_construct)


def setup(runner):
    T.install_std_stubs(runner.emu)
    MC["ppc"] = T.persist_alloc(runner.emu, 0x1D0)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["scoring"]: 1, MC["ppc"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)
        self.mcblock = MC["ppc"]

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

    def pre(self):
        self.write(ADDR["guard"], b"\x01")
        self.words(ADDR["scoring"] + 0x20, [3])

    def set_mc(self, p):
        self.words(ADDR["mc"], [p])

    def get_mc(self):
        return self.rd_words(ADDR["mc"], 1)[0]

    def call(self, fn, req, length, n):
        r3, _, _ = T.call_ppc(self.e, fn, [req, length, n & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if "mc" not in MC:
            MC["mc"] = lib.tpw_adv_alloc(0x1D0)
        self.mcblock = MC["mc"]
        CL.host_begin(lib, {1: self.g("scoring"), 2: self.mcblock}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_adv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def pre(self):
        pass

    def set_mc(self, p):
        self.lib.tpw_adv_set_mc(p)

    def get_mc(self):
        return self.lib.tpw_adv_get_mc()

    def call(self, fn, req, length, n):
        f = self.lib.tpw_adv_play if fn == FN_PLAY else self.lib.tpw_adv_playws
        return f(req, length, s32(n)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_addr.restype, lib.tpw_adv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_adv_set_mc.argtypes = [C.c_uint64]
    lib.tpw_adv_get_mc.restype = C.c_uint64
    for n in ("tpw_adv_play", "tpw_adv_playws"):
        f = getattr(lib, n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.pre()
    s.write(s.g("nullid"), struct.pack(s.fmt[0] + "H", case["nullid"]))
    s.set_mc(s.mcblock if case["mc"] else 0)
    req = s.alloc(0x18)
    s.write(req, struct.pack(s.fmt[0] + "H", case["id"]) + bytes(case["pad"]))
    s.words(req + 4, [case["sec"], case["resp"], case["msg"], case["score"]])
    s.write(req + 0x14, bytes([case["valid"], case["once"], case["over"], case["pad17"]]))
    length = s.alloc(4)
    s.words(length, [0xDEADBEEF])
    return req, length


def observe(s, case, req, length, answer):
    raw = s.read(req, 0x18)
    after = s.get_mc()
    state = "null" if after == 0 else ("preset" if after == s.mcblock else "new")
    return {"answer": answer, "log": s.log(), "id": struct.unpack(s.fmt[0] + "H", raw[:2])[0], "pad": raw[2:4],
            "words": s.rd_words(req + 4, 4), "flags": raw[0x14:], "length": s.rd_words(length, 1), "mc": state}


def run(s, case):
    try:
        req, length = build(s, case)
        ans = s.call(case["fn"], req, length, case["n"])
        return observe(s, case, req, length, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, fn):
    q = {
        "lmc": [r.choice([-1, 0, 2, 3, 4, 10])],
        "lmc_write": [r.choice([0, 1])],
        "lmc_id": [r.choice([0, 5, 7, 0xFFFF])],
        "lmc_val": [r.choice([0, 0x1234, 0xFFFFFFFF])],
        "num": [r.choice([0, 1, 2, 3])],
        "first": [r.choice([0, 5, 0x266, 100, -3])],
        "call": [r.randrange(0, 5000)],
        "tag": [r.randrange(0, 0x180)],
    }
    return {"fn": fn, "queues": q, "nullid": r.choice([0, 5]), "mc": r.choice([0, 1]), "id": r.choice([0, 3, 0xABCD]),
            "pad": [r.randrange(256), r.randrange(256)], "sec": r.choice([0, 9, 0xFFFFFFF0]), "resp": r.randrange(-5, 99),
            "msg": r.choice([0x160, 0, 17, 0x15F]), "score": r.randrange(-3, 3), "valid": r.choice([0, 1, 1, 2, 255]),
            "once": r.randrange(256), "over": r.randrange(256), "pad17": r.randrange(256), "n": r.choice([-1, 0, 1, 2, 3, 4])}


def edges(fn, seed):
    def gen():
        r = random.Random(seed)
        for valid in (0, 1):
            for mc in (0, 1):
                for num in (0, 1, 2, 3):
                    for n in (-1, 0, 1, 2, 3, 4):
                        for first in (0x266, 7):
                            for lmc in (2, 3):
                                c = base(r, fn)
                                c.update(valid=valid, mc=mc, n=n)
                                c["queues"].update(num=[num], first=[first], lmc=[lmc])
                                yield c
        for lmc in (-1, 2, 3, 4):
            for wr in (0, 1):
                for ident in (0, 5, 7):
                    c = base(r, fn)
                    c.update(valid=1, nullid=5)
                    c["queues"].update(lmc=[lmc], lmc_write=[wr], lmc_id=[ident], lmc_val=[r.choice([0, 77])])
                    yield c
    return gen


def rnd(fn):
    return lambda r: base(r, fn)


TESTS = [
    FnTest("CAdvisorRequest::Play", ppc, host, edge=edges(FN_PLAY, 11), rnd=rnd(FN_PLAY)),
    FnTest("CAdvisorRequest::PlayWithoutScore", ppc, host, edge=edges(FN_PWS, 12), rnd=rnd(FN_PWS)),
]
