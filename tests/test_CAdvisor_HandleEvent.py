"""Unit v5679, CAdvisor::HandleEvent, against libtpw_unit_CAdvisor_HandleEvent.dylib (tests/unit_CAdvisor_HandleEvent.cpp), main app
("SimTheme Park"), src/game/advisor/Advisor.cpp:
  CAdvisor::HandleEvent 0x1000ba18

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the request
constructor, CAdvisorScoring::CAdvisorScoring, __register_global_object, GetMessageCategory, CanIQueueThis,
FindLowestScoringSlotInQueue, UDebug) are fakes on both sides that record their arguments and answer from the case's queues. The request
constructor fake also fills the request (valid byte, score, force flag, score-overridden flag come from queues, the padding is zero).
CAdvisorRequest::Valid (8 bytes) runs as the original on the PowerPC side.

The world of a case: an advisor object (header words, the queue of 8 requests, three words, 351 message states, the last tutorial request,
two tail words), the game type word, the scoring guard byte at 0x102b4100 (both values, so the first-use path runs). What is compared: the
whole advisor object field by field (decoded per side), the guard byte, the game type word, the event log.
No fused operations. Not tested: the game type guard at 0x102b3aa4 is 1 (the host's is the compiler's own static guard, which a test cannot
reset, so the first-use constructor of CGameType is not run); FindLowestScoringSlotInQueue answers stay in -1..7 (the original indexes with
whatever it gets).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisor_HandleEvent"
MASK = 0xFFFFFFFF

FN_HANDLE = 0x1000ba18
SIZES = {FN_HANDLE: 2496}
OWN = set(SIZES)

CALLEE_REQ_CTOR = 0x1000deb4
CALLEE_SCORING_CTOR = 0x10010054
CALLEE_CATEGORY = 0x10010a74
CALLEE_CANQUEUE = 0x1000b3d4
CALLEE_LOWEST = 0x1000b7e8
CALLEE_UDEBUG = 0x10000e40

ADDR = dict(scoring_guard=0x102b4100, scoring=0x102b3ac8, gametype=0x102b3aa0, gametype_guard=0x102b3aa4)
HOST_LAYOUT = [0x14, 0x18, 0xE0, 0x16D0, 0x10, 0x16F0, 0x14, 0x10]

REQ = "HBBIiiiBBBB"          # a CAdvisorRequest, 0x18 bytes
STATE = "Ii" + "B3si"[0:1] + "3si"   # AdvisorMessageState, 0x10 bytes: time word, response index, said once, 3 pad bytes, kill count
STATE = "IiB3si"
NSTATE = 0x15F
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_req_ctor(e):
    L = e.cl
    msg, tid, sec, resp, over, score = s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))
    L.rec("CAdvisorRequest", (msg, tid, sec, resp, over, score))
    sc = s32(L.ret("rscore"))
    valid = L.ret("rvalid") & 0xFF
    ov = L.ret("rover") & 1
    so = L.ret("rso") & 0xFF
    e.write(e.r(3), struct.pack(">" + REQ, tid, 0, 0, sec, resp, msg, sc, valid, ov, so, 0))


def stub_scoring_ctor(e):
    L = e.cl
    L.rec("CAdvisorScoring", (L.lab(e.r(3)),))


def stub_register(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def stub_category(e):
    L = e.cl
    L.rec("GetMessageCategory", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("cat") & MASK)


def stub_canqueue(e):
    L = e.cl
    L.rec("CanIQueueThis", (s32(e.r(3) - CUR["adv"]), s32(e.r(4)), e.r(5) & 0xFF))
    e.set_r(3, L.ret("can") & 0xFF)


def stub_lowest(e):
    L = e.cl
    L.rec("FindLowest", (s32(e.r(3) - CUR["adv"]),))
    e.set_r(3, L.ret("low") & MASK)


STUBS.internal(CALLEE_REQ_CTOR, stub_req_ctor)
STUBS.internal(CALLEE_SCORING_CTOR, stub_scoring_ctor)
STUBS.imp("__register_global_object", stub_register)
STUBS.internal(CALLEE_CATEGORY, stub_category)
STUBS.internal(CALLEE_CANQUEUE, stub_canqueue)
STUBS.internal(CALLEE_LOWEST, stub_lowest)
STUBS.internal(CALLEE_UDEBUG, CL.recorder("UDebug", "s"))


def setup(runner):
    T.install_std_stubs(runner.emu)


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

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x102b3ac8: 1, 0x102b3aa0: 3}, case["queues"])
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

    def call(self, adv, event):
        T.call_ppc(self.e, FN_HANDLE, [adv, event & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("scoring"), 3: self.g("gametype")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hev_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hev_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, adv, event):
        self.lib.tpw_hev_call(adv, s32(event))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hev_addr.restype, lib.tpw_hev_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hev_alloc.restype, lib.tpw_hev_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hev_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hev_call.restype, lib.tpw_hev_call.argtypes = None, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hev_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    adv = s.alloc(0x16F0)
    s.adv = adv
    CUR["adv"] = adv
    p = s.fmt[0]
    s.words(adv, case["head"])
    for i, q in enumerate(case["queue"]):
        s.write(adv + 0x14 + 0x18 * i, struct.pack(p + REQ, *q))
    s.words(adv + 0xD4, case["mid"])
    flat = []
    for st in case["states"]:
        flat.extend(st)
    s.write(adv + 0xE0, struct.pack(p + STATE * NSTATE, *flat))
    s.write(adv + 0x16D0, struct.pack(p + REQ, *case["last"]))
    s.words(adv + 0x16E8, case["tail"])
    s.write(s.g("scoring_guard"), bytes([case["sguard"]]))
    s.words(s.g("gametype"), [case["gametype"]])
    s.write(s.g("gametype_guard"), bytes([1]))


def execute(s, case):
    s.call(s.adv, case["event"])
    return None


def observe(s, case, answer):
    a = s.adv
    p = s.fmt[0]
    return {
        "log": s.log(),
        "head": s.rd_words(a, 5),
        "queue": [list(struct.unpack(p + REQ, s.read(a + 0x14 + 0x18 * i, 0x18))) for i in range(8)],
        "mid": s.rd_words(a + 0xD4, 3),
        "states": list(struct.unpack(p + STATE * NSTATE, s.read(a + 0xE0, 16 * NSTATE))),
        "last": list(struct.unpack(p + REQ, s.read(a + 0x16D0, 0x18))),
        "tail": s.rd_words(a + 0x16E8, 2),
        "sguard": s.read(s.g("scoring_guard"), 1),
        "gametype": s.rd_word(s.g("gametype")),
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    if not CUR.get("printed"):
        CUR["printed"] = True
        print("jump table", [hex(e.u32(0x1027DDE4 + 4 * i)) for i in range(11)])
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SCORES = [-3, -2, -1, 0, 1, 2, 3, 5, 6]


def rreq(r, valid=None):
    return [r.choice([0, 1, 0xFFFF, r.randrange(65536)]), r.randrange(256), r.randrange(256), r.getrandbits(32), s32(r.getrandbits(32)),
            r.randrange(0, 0x160), r.choice(SCORES), r.choice([0, 1, 2, 0xFF]) if valid is None else valid, r.randrange(256),
            r.randrange(256), r.randrange(256)]


def base(r, event):
    allvalid = r.random() < 0.5
    queue = [rreq(r, valid=(1 if allvalid or r.random() < 0.6 else 0)) for _ in range(8)]
    for q in queue:
        if q[7] and r.random() < 0.3:
            q[7] = r.choice([2, 0x80, 0xFF])
    states = [[r.getrandbits(32), r.choice([-1, 0, 1, 5, s32(r.getrandbits(32))]), r.choice([0, 1, 2, 0xFF]),
               bytes(r.randrange(256) for _ in range(3)), r.choice([0, 1, 7, s32(r.getrandbits(32))])] for _ in range(NSTATE)]

    def two(vals):
        return [r.choice(vals), r.choice(vals)]

    return {
        "event": event,
        "head": [r.getrandbits(32) for _ in range(5)],
        "queue": queue,
        "mid": [r.getrandbits(32) for _ in range(3)],
        "states": states,
        "last": rreq(r),
        "tail": [r.getrandbits(32) for _ in range(2)],
        "sguard": r.choice([0, 1]),
        "gametype": r.choice([0, 1, 2, 2, 2, 3, 0xFFFFFFFF]),
        "queues": {
            "rvalid": two([0, 1, 1, 2]),
            "rscore": two(SCORES),
            "rover": two([0, 1]),
            "rso": two([0, 1, 2]),
            "cat": two([1, 1, 0, 2, 10, -1]),
            "can": two([0, 1, 1, 1]),
            "low": two([-1, 0, 1, 2, 3, 4, 5, 6, 7]),
        },
    }


def edges():
    r = random.Random(679)
    for event in list(range(0, 13)) + [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF]:
        for _ in range(6):
            yield base(r, event)


def randoms(r):
    event = r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, r.getrandbits(32)])
    return base(r, event)


TESTS = [
    FnTest("CAdvisor::HandleEvent", ppc, host, edge=edges, rnd=randoms),
]
