"""Unit v5626, the tag system's text formatter, against libtpw_unit_CTagSystem_FormatTagText.dylib (tests/unit_CTagSystem_FormatTagText.cpp),
main app ("SimTheme Park"), src/ThingLoop_CTagSystem_FormatTagText.cpp:
  CTagSystem::FormatTagText 0x1012cc3c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides: TbITable::FormatString (records the table, the buffer, the size, the text id and the count of
tags the text takes, from the queue "nargs:<id>", as the real one finds it in the text; then one "vtag" event per tag: kind (integer or
wide string by its vtable), magic, id and the signed number or the text; writes "s<id>" into the buffer), swprintf (the buffer, the size,
the format's text), TRACE and GAME_ASSERTMSG (the condition, the text), CRideDatabase::GetTypeName (the id; answers "T<id>" or NULL from
the queue) and its three-argument form (the id and the size; writes "N<id>").

The world of a case: a CTagSystem, the element (text id, format) or NULL, the challenge (the five numbers, both object ids, the staff
type), the "none" thing id and ride type globals, gThingArray's entry 0 and the thing's entry, gText's two string tables, the five
balance values. What is compared: the answer, the 0x400 characters of the object's text buffer, the event log.

No fused operations. Not tested: a thing id at or past gThingArray's 0x2800 entries (the original reads past the table into other data);
a staff type outside 0 to 4 in the formats that name the staff (the name buffer is then uninitialised stack the outer call reads);
a NULL challenge in the formats that read it (dereferenced unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTagSystem_FormatTagText"
MASK = 0xFFFFFFFF

# ---- the unit's own function
FN = 0x1012cc3c
SIZES = {FN: 4308}
OWN = set(SIZES)

# ---- the callees faked on both sides
GET_TYPE_NAME = 0x10154a10
GET_TYPE_NAME3 = 0x10154af0
TRACE = 0x1006fcfc
GAME_ASSERTMSG = 0x1000dde8
# the TOC slots holding the imported vtables of TbIntegerVTag (0x103bcaf0) and TbUniStringVTag (0x103bcb34), as the emulator binds them
INT_VT_SLOT, STR_VT_SLOT = 0x10261344, 0x10261348
JUMP_TABLE = 0x102a1218

# ---- the Mac's addresses of the globals
ADDR = dict(thing0=0x1034cbfc, noThing=0x102a1214, noRide=0x102a1216, table560=0x1037ff18 + 0x560, table590=0x1037ff18 + 0x590,
            bal750=0x102b4568 + 0x750, bal754=0x102b4568 + 0x754, bal760=0x102b4568 + 0x760, bal764=0x102b4568 + 0x764,
            bal768=0x102b4568 + 0x768, ridedb=0x1037f6c8)
BALS = ["bal750", "bal754", "bal760", "bal764", "bal768"]
MAC_MSG = dict(size=0x30, mPrize=0x14, mTargetTime=0x18, mTargetVal=0x1c, mDays=0x20, mValue=0x24, mObj=0x28, mObj2=0x2a, mStaff=0x2c)
MSG_FIELDS = ["mPrize", "mTargetTime", "mTargetVal", "mDays", "mValue", "mObj", "mObj2", "mStaff"]
# the host's: CTagSystem::mF0C, TagTableElement's two words, sizeof(CTagSystem), CThingArrayEntry::mThing
HOST_LAYOUT = [0xc, 0xc, 0x1c, 0x80e, 0]
THING_COUNT = 0x2800

CUR = {}


def s32(v):
    return T.s32(v)


def wstr(e, a):
    """A big-endian UTF-16 string in emulator memory."""
    out = []
    for k in range(0x400):
        ch = struct.unpack(">H", e.read(a + 2 * k, 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return "".join(out)


def wwrite(e, a, text):
    e.write(a, text.encode("utf-16-be") + b"\0\0")


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_format_string(e):
    """int TbITable::FormatString(wchar_t* out, unsigned size, unsigned long id, ...) const."""
    L = e.cl
    tid = e.r(6) & MASK
    count = CUR["queues"].get("nargs:%d" % tid, [0])[0]
    L.rec("FormatString", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5)), tid, count))
    for k in range(count):
        p = e.r(7 + k) if k < 4 else e.u32(e.r(1) + 0x38 + 4 * (k - 4))
        magic, vt, tag_id = e.u32(p), e.u32(p + 4), e.u32(p + 8)
        if vt == e.u32(INT_VT_SLOT):
            mag, neg = e.u32(p + 0xc), e.u32(p + 0x10)
            L.rec("vtag", (1, magic, tag_id, s32(-mag if neg else mag)), ("",))
        elif vt == e.u32(STR_VT_SLOT):
            t = e.u32(p + 0xc)
            L.rec("vtag", (2, magic, tag_id, 1 if t else 0), (wstr(e, t) if t else "",))
        else:
            L.rec("vtag", (-1, magic, tag_id, 0), ("",))
    wwrite(e, e.r(4), "s%d" % tid)
    e.set_r(3, 0)


def stub_swprintf(e):
    L = e.cl
    L.rec("swprintf", (L.lab(e.r(3)), s32(e.r(4))), (wstr(e, e.r(5)),))
    e.set_r(3, 0)


def stub_trace(e):
    e.cl.rec("TRACE", (), (e.cstr(e.r(3)),))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4)),))


def stub_get_type_name(e):
    L = e.cl
    tid = (e.r(4) >> 16) & 0xFFFF
    L.rec("GetTypeName", (L.lab(e.r(3)), tid))
    if L.ret("GetTypeName"):
        wwrite(e, CUR["namebuf"], "T%d" % tid)
        e.set_r(3, CUR["namebuf"])
    else:
        e.set_r(3, 0)


def stub_get_type_name3(e):
    L = e.cl
    tid = (e.r(4) >> 16) & 0xFFFF
    L.rec("GetTypeName3", (L.lab(e.r(3)), tid, s32(e.r(6))))
    wwrite(e, e.r(5), "N%d" % tid)


STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format_string)
STUBS.imp("swprintf", stub_swprintf)
STUBS.internal(TRACE, stub_trace)
STUBS.internal(GAME_ASSERTMSG, stub_assert)
STUBS.internal(GET_TYPE_NAME, stub_get_type_name)
STUBS.internal(GET_TYPE_NAME3, stub_get_type_name3)


def setup(runner):
    T.install_std_stubs(runner.emu)
    CUR["namebuf"] = T.persist_alloc(runner.emu, 64)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        CUR["queues"] = case["queues"]
        self.msg = MAC_MSG
        self.saved = None

    def begin(self, labels):
        self.L = self.e.cl = CL.PpcLog({a: k for k, a in labels.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a, v):
        self.words(a, [v])

    def thing_slot(self, i):
        return ADDR["thing0"] + 20 * i

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, obj, elem, thing, value, msg, junk):
        r3, _, _ = T.call_ppc(self.e, FN, [obj, elem, ((thing << 16) | junk) & MASK, value & MASK, msg])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.msg = lib.msg_layout

    def begin(self, labels):
        CL.host_begin(self.lib, labels, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tag_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def thing_slot(self, i):
        return self.g("thing0") + self.lib.thing_stride * i

    def alloc(self, n):
        return self.lib.tpw_tag_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, elem, thing, value, msg, junk):
        return self.lib.tpw_tag_format(obj, elem, thing, value & MASK, msg) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tag_addr.restype, lib.tpw_tag_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tag_alloc.restype, lib.tpw_tag_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_tag_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tag_format.restype = C.c_uint32
    lib.tpw_tag_format.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tag_layout(out)
    vals = list(out)[:n]
    assert vals[:4] == HOST_LAYOUT[:4] and vals[4] == HOST_LAYOUT[4], vals
    lib.thing_stride = vals[5]
    lib.msg_layout = dict(size=vals[6], **dict(zip(MSG_FIELDS, vals[7:15])))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    obj = s.alloc(0x810)
    s.write(obj + 0xc, b"\0" * 0x800)
    elem = 0
    if case["format"] is not None:
        elem = s.alloc(0x20)
        s.words(elem + 0xc, [case["textId"]])
        s.words(elem + 0x1c, [case["format"]])
    msg = s.alloc(s.msg["size"])
    m = case["msg"]
    for f in ("mPrize", "mTargetTime", "mTargetVal", "mDays", "mValue", "mStaff"):
        s.words(msg + s.msg[f], [m[f]])
    s.half(msg + s.msg["mObj"], m["mObj"])
    s.half(msg + s.msg["mObj2"], m["mObj2"])
    s.half(s.g("noThing"), case["noThing"])
    s.half(s.g("noRide"), case["noRide"])
    s.ptr(s.thing_slot(0), case["thing0"])
    s.ptr(s.thing_slot(case["thing"]), case["thingAt"])
    t560, t590 = s.alloc(0x10), s.alloc(0x10)
    s.ptr(s.g("table560"), t560)
    s.ptr(s.g("table590"), t590)
    for name, v in zip(BALS, case["bal"]):
        s.words(s.g(name), [v])
    s.begin({2: obj + 0xc, 3: t560, 4: t590, 5: s.g("ridedb")})
    return obj, elem, msg


def observe(s, obj, answer):
    return {"answer": answer, "log": s.log(), "text": s.rd_halves(obj + 0xc, 0x400)}


def run(s, case):
    try:
        obj, elem, msg = build(s, case)
        answer = s.call(obj, elem, case["thing"], case["value"], msg, case["junk"])
        return observe(s, obj, answer)
    finally:
        s.done()


def ppc(e, case):
    assert [e.u32(JUMP_TABLE + 4 * i) for i in range(0x25)] == TABLE
    assert e.u32(INT_VT_SLOT) != e.u32(STR_VT_SLOT)
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

# the jump table's targets by format (0x1012dcf8: no call, answers 1), and the count of tags each block passes
BLOCK_NARGS = {0x1012ccc4: 0, 0x1012cce0: 0, 0x1012cd60: 1, 0x1012cddc: 0, 0x1012cdf4: 1, 0x1012ce54: 1, 0x1012ceb8: 1, 0x1012cf1c: 1,
               0x1012cf80: 2, 0x1012d030: 3, 0x1012d12c: 4, 0x1012d274: 4, 0x1012d450: 4, 0x1012d594: 4, 0x1012d6dc: 5,
               0x1012d874: 5, 0x1012daa0: 4, 0x1012dbe4: 1, 0x1012dc4c: 2, 0x1012dcf8: 0}
TABLE = [0x1012ccc4, 0x1012cce0, 0x1012cd60, 0x1012cdf4, 0x1012cddc, 0x1012ce54, 0x1012ceb8, 0x1012cf1c,
         0x1012cf80, 0x1012d030, 0x1012d12c, 0x1012d030, 0x1012d274, 0x1012d274, 0x1012d12c, 0x1012d450,
         0x1012d594, 0x1012d6dc, 0x1012d594, 0x1012d874, 0x1012d874, 0x1012d6dc, 0x1012daa0] + [0x1012dbe4] * 7 + [0x1012dc4c] * 7
WORD_EDGES = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 2, -2, 100, -100]


def rword(r):
    return r.choice(WORD_EDGES) if r.random() < 0.3 else r.randrange(-0x80000000, 0x80000000)


def make(r, fmt, **kw):
    thing = r.randrange(THING_COUNT)
    text_id = r.randrange(16, 0x80000000)
    nargs = 0 if fmt is None or fmt > 0x24 else BLOCK_NARGS[TABLE[fmt]]
    c = {"format": fmt, "textId": text_id, "thing": thing, "value": rword(r) & MASK, "junk": r.randrange(0x10000),
         "noThing": thing if r.random() < 0.2 else r.randrange(0x10000), "thing0": r.choice([0, 0, 0x1234]),
         "thingAt": r.choice([0, 0x5678, 0x5678]), "bal": [rword(r) & MASK for _ in BALS],
         "msg": {"mPrize": rword(r) & MASK, "mTargetTime": rword(r) & MASK, "mTargetVal": rword(r) & MASK, "mDays": rword(r) & MASK,
                 "mValue": rword(r) & MASK, "mObj": r.randrange(0x10000), "mObj2": r.randrange(0x10000), "mStaff": r.randrange(5)},
         "queues": {"GetTypeName": [r.choice([1, 1, 1, 0])], "nargs:%d" % text_id: [nargs]}}
    c["noRide"] = c["value"] & 0xFFFF if r.random() < 0.2 else r.randrange(0x10000)
    c.update(kw)
    if c["format"] is not None and c["format"] <= 0x24:
        c["queues"]["nargs:%d" % c["textId"]] = [BLOCK_NARGS[TABLE[c["format"]]]]
    return c


def edges():
    r = random.Random(5626)
    yield make(r, None)
    for fmt in (0x25, 0x26, 0x80000000, 0xFFFFFFFF):
        yield make(r, fmt)
    for fmt in range(0x25):
        for k in range(4):
            yield make(r, fmt)
        for staff in range(5):
            c = make(r, fmt)
            c["msg"]["mStaff"] = staff
            yield c
    for fmt in range(0x25):
        if TABLE[fmt] == 0x1012cce0:
            yield make(r, fmt, thing=7, noThing=7)
            yield make(r, fmt, thing=7, noThing=8, thingAt=0)
            yield make(r, fmt, thing=7, noThing=8, thingAt=0x99, thing0=0)
            yield make(r, fmt, thing=7, noThing=8, thingAt=0x99, thing0=0x77)
            yield make(r, fmt, thing=0, noThing=1, thingAt=0x99, thing0=0)
            yield make(r, fmt, thing=THING_COUNT - 1, noThing=0, thingAt=0x99)
        if TABLE[fmt] == 0x1012cd60:
            yield make(r, fmt, value=0x12345678, noRide=0x5678)
            yield make(r, fmt, value=0x12345678, noRide=0x1234)
            yield make(r, fmt, value=0xFFFF, noRide=0xFFFF)
            yield make(r, fmt, value=0x10000, noRide=0)
            c = make(r, fmt, value=0x10001, noRide=0)
            c["queues"]["GetTypeName"] = [0]
            yield c
        if TABLE[fmt] in (0x1012cdf4,):
            for v in (0, 1, MASK, 0x80000000, 0x7FFFFFFF):
                yield make(r, fmt, value=v)
        if TABLE[fmt] in (0x1012ce54, 0x1012ceb8, 0x1012cf1c, 0x1012cf80):
            for v in (0, 1, MASK, 0x80000000, 0x7FFFFFFF):
                yield make(r, fmt, bal=[v] * 5)


def rnd(r):
    k = r.random()
    if k < 0.03:
        return make(r, None)
    if k < 0.08:
        return make(r, r.randrange(0x25, 0x10000) if r.random() < 0.5 else r.randrange(0x25, 0x100000000))
    return make(r, r.randrange(0x25))


TESTS = [
    FnTest("CTagSystem::FormatTagText", ppc, host, edge=edges, rnd=rnd),
]
