"""Unit v5631, the advisor's message receiver, against libtpw_unit_CAdvisor_ReceiveMessage.dylib (tests/unit_CAdvisor_ReceiveMessage.cpp),
main app ("SimTheme Park"), src/AdvisorQueue_CAdvisor_ReceiveMessage.cpp:
  CAdvisor::ReceiveMessage 0x1000c874

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments and answer from the case's queues: the request constructor 0x1000deb4 (it also
fills the 0x18-byte request from the queues ReqBits and ReqScore), the scoring's constructor 0x10010054 and __register_global_object (the
inline guard of the scoring singleton; on the host the TheScoring fake records the same two events on a clear guard byte),
GetMessageCategory 0x10010a74, CanIQueueThis 0x1000b3d4, FindLowestScoringSlotInQueue 0x1000b7e8, HandleEvent 0x1000ba18, HandleStaffInfo
0x1000c40c, CTRACE, TRACE and GAME_ASSERTMSG. CAdvisorRequest::Valid 0x1000e894 runs as the original.

The world of a case: the advisor (0x16f0 bytes: the eight queued requests and the last tutorial request, the rest zero), the message (its
class by type, GetType answering the case's type: on the PowerPC a vtable whose GetType slot is a three-instruction function written per
case), the scoring guard byte, rows 0 and N of gThingArray and the thing of row N (types 0xe). What is compared: the whole advisor (the
requests field by field, the rest as words), the guard byte and the event log.

No floating point. Not tested: a type 0xe message whose row holds no thing (the original reads the NULL thing's bytes) and rows past the
table's 0x2800 entries (the original reads past gThingArray); FindLowestScoringSlotInQueue answers are kept within the queue (-2 to 7).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisor_ReceiveMessage"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN = 0x1000c874
SIZES = {FN: 5440}
OWN = set(SIZES)

# ---- the callees faked on both sides
REQ_CTOR = 0x1000deb4
SCORING_CTOR = 0x10010054
GET_CATEGORY = 0x10010a74
CAN_QUEUE = 0x1000b3d4
FIND_LOWEST = 0x1000b7e8
HANDLE_EVENT = 0x1000ba18
HANDLE_STAFF = 0x1000c40c
CTRACE = 0x1006fcbc
TRACE = 0x1006fcfc
ASSERTMSG = 0x1000dde8
VALID = 0x1000e894          # runs as the original
# The test messages' GetType: CMsgRideWantsUpgrade::GetType's address (8 bytes, never reached otherwise in a case), stubbed to answer the
# case's type (writing code per case would meet the emulator's stale translation cache).
GET_TYPE = 0x10110e44

ADDR = dict(guard=0x102b4100, scoring=0x102b3ac8, dtor=0x102657e0, node=0x102b4104, things=0x1034cbfc)
ADVISOR_SIZE = 0x16f0
HOST_LAYOUT = [0x16f0, 0x14, 0x16d0, 0x18, 0x14, 48, 0, 4, 6]
THING_ROWS = 0x2800

# the message class by type (the shim's kind)
KINDS = {5: 0, 8: 1, 0xe: 2, 0x11: 3, 0x13: 4, 0x17: 5, 0x19: 6}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(VALID)


def stub_request(e):
    """CAdvisorRequest(this, AdvisorMessage, tag_IDX_TYPE thing (high half of r5), unsigned long, int, bool, int): records the arguments and
    fills the request from the queues."""
    L = e.cl
    this, message, thing = e.r(3), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF
    secondary, response, force, score = e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))
    L.rec("CAdvisorRequest", (L.lab(this), message, thing, secondary, response, 1 if force else 0, score))
    bits = L.ret("ReqBits") & MASK
    sc = L.ret("ReqScore") & MASK
    w = [(thing << 16) | 0x5A00 | (bits >> 24), secondary, response & MASK, message & MASK, sc,
         ((bits & 0xFF) << 24) | ((1 if force else 0) << 16) | (((bits >> 8) & 0xFF) << 8) | ((bits >> 16) & 0xFF)]
    e.write(this, struct.pack(">6I", *w))


def stub_can_queue(e):
    """bool CAdvisor::CanIQueueThis(AdvisorMessage, bool) const: the bool is the low byte of r5."""
    L = e.cl
    L.rec("CanIQueueThis", (L.lab(e.r(3)), s32(e.r(4)), 1 if (e.r(5) & 0xFF) else 0))
    e.set_r(3, L.ret("CanIQueueThis") & MASK)


def stub_get_type(e):
    """The test message's GetType (the TVector of its vtable's GetType slot points here): the case's type, no event."""
    e.set_r(3, CURRENT["type"])


CURRENT = {"type": 0}
STUBS.internal(GET_TYPE, stub_get_type)
STUBS.internal(REQ_CTOR, stub_request)
STUBS.internal(CAN_QUEUE, stub_can_queue)
STUBS.internal(SCORING_CTOR, CL.recorder("CAdvisorScoring", "L"))
STUBS.imp("__register_global_object", CL.recorder("__register_global_object", "LLL"))
STUBS.internal(GET_CATEGORY, CL.recorder("GetMessageCategory", "Li", ret="i"))
STUBS.internal(FIND_LOWEST, CL.recorder("FindLowestScoringSlotInQueue", "L", ret="i"))
STUBS.internal(HANDLE_EVENT, CL.recorder("HandleEvent", "Li"))
STUBS.internal(HANDLE_STAFF, CL.recorder("HandleStaffInfo", "LL"))
STUBS.internal(CTRACE, CL.recorder("CTRACE", "us"))
STUBS.internal(TRACE, CL.recorder("TRACE", "si"))
STUBS.internal(ASSERTMSG, CL.recorder("GAME_ASSERTMSG", "is"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def put_req(self, a, t):
        thing, p0, p1, sec, resp, msg, score, valid, force, sovr, p17 = t
        self.half(a, thing)
        self.write(a + 2, bytes([p0, p1]))
        self.words(a + 4, [sec, resp, msg, score])
        self.write(a + 0x14, bytes([valid, force, sovr, p17]))

    def get_req(self, a):
        b = self.read(a + 2, 2)
        sec, resp, msg, score = self.rd_words(a + 4, 4)
        q = self.read(a + 0x14, 4)
        return [self.rd_half(a), b[0], b[1], sec, s32(resp), s32(msg), s32(score), q[0], q[1], q[2], q[3]]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.adv = e.alloc(ADVISOR_SIZE)
        self.msg = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.adv: 5, self.msg: 1, ADDR["scoring"]: 2, ADDR["dtor"]: 3, ADDR["node"]: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return bytes(self.e.read(a, n))

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_row(self, row, ptr):
        self.words(ADDR["things"] + 0x14 * row, [ptr])

    def receive(self, case):
        e = self.e
        CURRENT["type"] = case["type"] & MASK
        vt = e.alloc(0x10)
        tv = e.alloc(8)
        e.write(tv, struct.pack(">2I", GET_TYPE, 0))
        e.write(vt, struct.pack(">4I", 0, 0, tv, 0))
        e.write(self.msg, struct.pack(">2I", vt, 0) + struct.pack(">10I", *[w & MASK for w in case["body"]]))
        T.call_ppc(e, FN, [self.adv, self.msg])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.adv = lib.tpw_rm_alloc(ADVISOR_SIZE)
        self.msg = self.g("msg")
        CL.host_begin(lib, {5: self.adv, 1: self.msg, 2: self.g("scoring"), 3: self.g("dtor"), 4: self.g("node")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_row(self, row, ptr):
        self.write(self.g("things") + 48 * row, struct.pack("<Q", ptr))

    def receive(self, case):
        body = (C.c_uint32 * 10)(*[w & MASK for w in case["body"]])
        self.lib.tpw_rm_receive(self.adv, KINDS.get(case["type"], 7), s32(case["type"]), body)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rm_addr.restype, lib.tpw_rm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rm_alloc.restype, lib.tpw_rm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rm_receive.restype = None
    lib.tpw_rm_receive.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for i, t in enumerate(case["queue"]):
        s.put_req(s.adv + 0x14 + 0x18 * i, t)
    s.put_req(s.adv + 0x16D0, case["last"])
    s.write(s.g("guard"), bytes([case["guard"]]))
    if case["type"] == 0xE:
        row = (case["body"][0] >> 16) & 0xFFFF
        thing = s.alloc(16)
        s.write(thing, bytes(case["thing"]))
        if row == 0:
            s.set_row(0, thing)
        else:
            if case["row0"]:
                other = s.alloc(16)
                s.write(other, bytes(case["thing"][::-1]))
                s.set_row(0, other)
            else:
                s.set_row(0, 0)
            s.set_row(row, thing)


def observe(s, case):
    a = s.adv
    return {
        "log": s.log(),
        "queue": [s.get_req(a + 0x14 + 0x18 * i) for i in range(8)],
        "last": s.get_req(a + 0x16D0),
        "head": s.rd_words(a, 5),
        "middle": s.rd_words(a + 0xD4, (0x16D0 - 0xD4) // 4),
        "tail": s.rd_words(a + 0x16E8, 2),
        "guard": s.read(s.g("guard"), 1)[0],
    }


def run(s, case):
    try:
        build(s, case)
        s.receive(case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

HANDLED = [5, 8, 0xE, 0x11, 0x13, 0x17, 0x19]
OTHER_TYPES = [-5, -1, 0, 1, 4, 6, 7, 9, 0xA, 0xD, 0xF, 0x10, 0x12, 0x14, 0x15, 0x16, 0x18, 0x1A, 0x1B, 0x40, 0x7FFFFFFF, -0x80000000]


def mkreq(r, valid=None, score=None):
    return [r.randrange(0x10000), r.randrange(256), r.randrange(256), T.rint(r) & MASK, r.choice([-1, 0, 1, 2, T.rint(r)]),
            r.randrange(0x170), r.choice([-1, 0, 1, 5, 50, 100, T.rint(r)]) if score is None else score,
            (0 if r.random() < 0.4 else r.choice([1, 0x80, 0xFF])) if valid is None else valid,
            r.choice([0, 0, 1, 2, 0x80]), r.choice([0, 1, 0xFF]), r.randrange(256)]


def mkcase(r, typ, free=None, cat=None, can=None, lowest=None, old=None, new=None, guard=None, sub=None, row=None, row0=None):
    """A world for a message of `typ`. free: whether some queue slot is free (None: random); old/new: the replaced slot's and the new
    request's (valid, score); sub: the category, kind or line the message carries."""
    queue = [mkreq(r, valid=r.choice([1, 0x80, 0xFF])) for _ in range(8)]
    if free is None:
        free = r.random() < 0.5
    if free:
        for i in r.sample(range(8), r.randrange(1, 9)):
            queue[i][7] = 0
    low = r.choice([-2, -1, 0, 1, 3, 7] + list(range(8))) if lowest is None else lowest
    if old is not None and 0 <= low < 8:
        queue[low][7], queue[low][6] = old
    body = [T.rint(r) & MASK for _ in range(10)]
    bits = T.rint(r) & MASK
    score = r.choice([-1, 0, 1, 50, 100, T.rint(r)])
    if new is not None:
        bits = (bits & ~0xFF) | new[0]
        score = new[1]
    if typ == 5:
        req = mkreq(r, valid=r.choice([0, 1, 0x80]) if sub is None else sub)
        if new is not None:
            req[7], req[6] = new
        thing, p0, p1, sec, resp, msg, sc, valid, force, sovr, p17 = req
        body[:6] = [(thing << 16) | (p0 << 8) | p1, sec, resp & MASK, msg & MASK, sc & MASK, (valid << 24) | (force << 16) | (sovr << 8) | p17]
    elif typ == 0xE:
        if row is None:
            row = r.choice([0, 1, 2, THING_ROWS - 1, r.randrange(THING_ROWS)])
        body[0] = (row << 16) | (body[0] & 0xFFFF)
        body[1] = (r.choice([-1, 0, 1, 2, 3, 0x7FFFFFFF]) if sub is None else sub) & MASK
    elif typ == 0x11:
        body[1] = (r.choice([-1, 0, 1, 2, 3, 4, 5, -0x80000000]) if sub is None else sub) & MASK
    elif typ == 0x19:
        body[1] = (r.choice([0x160, 0x160, 0, 1, 0x15F, 0x161, r.randrange(0x170)]) if sub is None else sub) & MASK
    return {
        "type": typ,
        "body": body,
        "queue": queue,
        "last": mkreq(r),
        "guard": r.choice([0, 1, 1, 1, 0x80]) if guard is None else guard,
        "thing": [r.randrange(256) for _ in range(12)],
        "row0": (r.random() < 0.3) if row0 is None else row0,
        "queues": {
            "GetMessageCategory": [r.choice([0, 1, 1, 2, -1, 10]) if cat is None else cat],
            "CanIQueueThis": [r.choice([0, 1, 1, 1]) if can is None else can],
            "FindLowestScoringSlotInQueue": [low],
            "ReqBits": [bits],
            "ReqScore": [score & MASK],
        },
    }


def edges():
    r = random.Random(5631)
    for typ in OTHER_TYPES:
        yield mkcase(r, typ)
    for typ in HANDLED:
        for guard in (0, 1, 0x80):
            yield mkcase(r, typ, guard=guard)
    paths = [5, 8, 0xE, 0x11, 0x19]
    subs = {5: [1], 8: [None], 0xE: [0, 1, 2], 0x11: [0, 1, 2, 3, 4], 0x19: [0, 0x15F, 0x161]}
    for typ in paths:
        for sub in subs[typ]:
            for cat in (0, 1, 2):
                for can in (0, 1):
                    yield mkcase(r, typ, sub=sub, cat=cat, can=can)
            yield mkcase(r, typ, sub=sub, can=1, free=True)
            for low in (-2, -1, 0, 7):
                yield mkcase(r, typ, sub=sub, can=1, free=False, lowest=low)
            for old in ((0, 5), (1, 4), (1, 5), (1, 6), (0x80, -1), (0, -7)):
                for new in ((1, 5), (0, 5), (0x80, -1), (0, -1)):
                    if typ == 5 and new[0] == 0:
                        continue
                    yield mkcase(r, typ, sub=sub, can=1, free=False, lowest=r.randrange(8), old=old, new=new)
    for sub in (-1, 0, 2, 5):
        yield mkcase(r, 5, sub=sub)
    for sub in (-1, 3, 0x7FFFFFFF, -0x80000000):
        yield mkcase(r, 0xE, sub=sub)
    for sub in (-1, 5, -0x80000000, 0x7FFFFFFF):
        yield mkcase(r, 0x11, sub=sub)
    yield mkcase(r, 0x19, sub=0x160)
    for row in (0, 1, THING_ROWS - 1):
        for row0 in (False, True):
            yield mkcase(r, 0xE, row=row, row0=row0, sub=1)


def rnd(r):
    k = r.random()
    if k < 0.75:
        return mkcase(r, r.choice([5, 5, 8, 0xE, 0xE, 0x11, 0x11, 0x13, 0x17, 0x19, 0x19]))
    if k < 0.9:
        return mkcase(r, r.choice(OTHER_TYPES))
    return mkcase(r, s32(T.rint(r)))


TESTS = [
    FnTest("CAdvisor::ReceiveMessage", ppc, host, edge=edges, rnd=rnd),
]
