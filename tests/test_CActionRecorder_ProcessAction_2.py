"""Unit v5362, CActionRecorder replay and record functions, against libtpw_unit_CActionRecorder_ProcessAction_2.dylib
(tests/unit_CActionRecorder_ProcessAction_2.cpp), main app ("SimTheme Park"),
src/game/ActionRecorder.cpp:
  CActionRecorder::ProcessAction@0x10145e38 (action, halfword), CActionRecorder::ProcessAction@0x101465a8 (action, halfword, count,
  text), CActionRecorder::RecordAction (0x1014445c, action and three halfwords)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit is a fake on both sides that records its arguments: CTRACE (the format by a hash, the %d words and the %ls text), the
assert, the LLCC and Layout functions, the selected-object setter, the sign setters and getters, RegenerateRideSign, the swear filter and
the master save's constructor; the recorder's file (the virtual Write through __ptr_glue on the Mac, a CFile subclass on the host) logs its
bytes. The strings (basic_string constructor and destructor) are made by a Mac-side stand-in (a handle to a rep with the length at +4 and
the data pointer at +0xc); the host uses std::u16string. The things are objects of 0x20 bytes whose halfword at +0 (the thing id) is what the
fakes log as the object's identity.

The world of a case: the master save pointer (set or not), the world with its map cell chain (cell word at +0x2d8 + (cell - 1) * 0x44 +
0x24, the gates halfword at +0x1da732), the thing table entries of the chain, entry 0 (set or not: the assert), the two chain-end halfwords.
What is compared: the log, and whether the master save pointer is unset, the preset one or a new one.

No fused operations. Not tested: a text shorter than its count (the original reads past it), a map cell above 0x4000 (outside the world), a
chain through an unwritten thing entry (a NULL dereference), a chain that never ends, a halfword argument with high bits set (the caller's
convention clears them).
"""
import ctypes as C
import random
import re
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_ProcessAction_2"
P = "tpw_CActionRecorder_ProcessAction_2_"
MASK = 0xFFFFFFFF

FN1, FN2, FN3 = 0x10145E38, 0x101465A8, 0x1014445C
SIZES = {FN1: 472, FN2: 604, FN3: 296}
OWN = set(SIZES)

A_CTRACE, A_ASSERT = 0x1006FCBC, 0x1000DDE8
A_SETMODE, A_SETMODENR, A_RIDETRACK, A_RIDEQUEUE, A_ROTATION = 0x10090E18, 0x100911F0, 0x10091A5C, 0x10091994, 0x10090D40
A_SELECT, A_SETCUR = 0x101F0000, 0x10088590
A_PYLONUP, A_PYLONDOWN, A_EDITOLD, A_EDIT, A_REGEN = 0x10058C08, 0x10058C80, 0x10058FA0, 0x1005901C, 0x10058EC0
A_SETSIGNA, A_GETSIGNB, A_SETSIGNB, A_GETSIGNA, A_REGENSIGN = 0x10110948, 0x101108DC, 0x10110B10, 0x101107C4, 0x101EEB9C
A_STRCTOR, A_STRDTOR, A_FILTER, A_MSCTOR, A_GLUE = 0x10146B8C, 0x101468F8, 0x1020D2CC, 0x101694DC, 0x1023BAA8

ADDR = dict(gMasterSave=0x10380A8C, gGameWorld=0x1037EBFC, gThingArray=0x1034CBFC, gUIInterface=0x10396440,
            gReplayChainEndA=0x102A4BD4, gReplayChainEndB=0x102A4CEC)
HOST_LAYOUT = [0x2D8, 0x44, 0x24, 0x1DA732, 2, 0xA, 48]
WORLD_SIZE = 0x1DB000
CUR = {}


def s32(v):
    return T.s32(v)


def fmt_hash(text):
    h = 0
    for c in text.encode():
        h = (h * 31 + c) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u16s(e, addr, n):
    return [struct.unpack(">H", e.read(addr + 2 * i, 2))[0] for i in range(n)]


def text5(e, addr):
    chars = []
    if addr:
        while len(chars) < 5:
            c = u16s(e, addr + 2 * len(chars), 1)[0]
            if c == 0:
                break
            chars.append(c)
    return (len(chars),) + tuple(chars) + (0,) * (5 - len(chars))


def tag(e, addr):
    return struct.unpack(">H", e.read(addr, 2))[0] if addr else 0


def st_ctrace(e):
    L = e.cl
    fmt = e.cstr(e.r(4))
    vals = [e.r(3) & MASK, fmt_hash(fmt)]
    reg = 5
    for m in re.finditer(r"%(d|ls)", fmt):
        if m.group(1) == "d":
            vals.append(s32(e.r(reg)))
        else:
            vals.extend(text5(e, e.r(reg)))
        reg += 1
    L.rec("CTRACE", tuple(vals))


def st_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (e.r(3) & 0xFF, fmt_hash(e.cstr(e.r(4)))))


def two(name):
    def fn(e):
        e.cl.rec(name, (s32(e.r(3)), e.r(4) & 0xFF))
    return fn


def one(name, signed=True):
    def fn(e):
        e.cl.rec(name, (s32(e.r(3)) if signed else e.r(3) & MASK,))
    return fn


def st_setcur(e):
    e.cl.rec("LLCM_SetCurrentObject", (e.r(3) & 0xFF,))


def st_select(e):
    L = e.cl
    L.rec("SetSelectedObject", (L.lab(e.r(3)), tag(e, e.r(4))))


def st_setsign(name):
    def fn(e):
        e.cl.rec(name, (tag(e, e.r(3)),) + text5(e, e.r(4)))
    return fn


def put_sign(e, buf):
    v = e.cl.ret("sign")
    n = 1 + v % 4
    chars = [0x61 + ((v >> (3 * i)) & 7) for i in range(n)] + [0]
    e.write(buf, struct.pack(">%dH" % len(chars), *chars))


def st_getsign(name):
    def fn(e):
        e.cl.rec(name, (tag(e, e.r(3)), e.r(5) & MASK))
        put_sign(e, e.r(4))
    return fn


def st_regensign(e):
    e.cl.rec("RegenerateRideSign", (tag(e, e.r(3)),) + text5(e, e.r(4))[:5] + text5(e, e.r(5))[:5])


def st_strctor(e):
    this, text, n = e.r(3), e.r(4), e.r(5) & MASK
    rep = e.alloc(0x20, bytes(0x20))
    buf = e.alloc(2 * n + 2, bytes(2 * n + 2))
    if n:
        e.write(buf, e.read(text, 2 * n))
    e.write(rep + 4, struct.pack(">I", n))
    e.write(rep + 0xC, struct.pack(">I", buf))
    e.write(this, struct.pack(">I", rep))


def st_nothing(e):
    pass


def st_filter(e):
    L = e.cl
    rep = e.u32(e.r(4))
    n, buf = e.u32(rep + 4), e.u32(rep + 0xC)
    L.rec("Filter", (L.lab(e.r(3)), e.r(5) & 0xFF) + text5(e, buf))
    if L.ret("filter") != 0 and n > 0:
        e.write(buf, struct.pack(">H", 0x2A))


def st_glue(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))   # the glue saves the TOC where the caller reloads it
    n = e.r(5) & MASK
    data = e.read(e.r(4), n)
    e.cl.rec("FileWrite", (1 if e.r(3) == CUR["base"] + 0xC else 0, n, data[0] if n > 0 else 0, data[1] if n > 1 else 0))


for addr, fn in [(A_CTRACE, st_ctrace), (A_ASSERT, st_assert), (A_SETMODE, two("LLCC_SystemSetMode")),
                 (A_SETMODENR, two("LLCC_SystemSetModeNR")), (A_RIDETRACK, one("LLCC_SystemSetRideTrack")),
                 (A_RIDEQUEUE, one("LLCC_SystemSetRideQueue")), (A_ROTATION, two("LLCC_SetRideRotation")),
                 (A_SELECT, st_select), (A_SETCUR, st_setcur),
                 (A_PYLONUP, one("LayoutStackPylonUp", False)), (A_PYLONDOWN, one("LayoutStackPylonDown", False)),
                 (A_EDITOLD, one("LayoutSetCoasterEditOld", False)), (A_EDIT, one("LayoutSetCoasterEdit", False)),
                 (A_REGEN, one("LayoutRegenerateTrack", False)),
                 (A_SETSIGNA, st_setsign("SetSignNameA")), (A_SETSIGNB, st_setsign("SetSignNameB")),
                 (A_GETSIGNA, st_getsign("GetSignNameA")), (A_GETSIGNB, st_getsign("GetSignNameB")),
                 (A_REGENSIGN, st_regensign), (A_STRCTOR, st_strctor), (A_STRDTOR, st_nothing), (A_FILTER, st_filter),
                 (A_GLUE, st_glue)]:
    STUBS.internal(addr, fn)
STUBS.internal(A_MSCTOR, CL.recorder("MasterSaveCtor", "N"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    CUR["world"] = T.persist_alloc(runner.emu, WORLD_SIZE)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def u16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def u8(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def entry(self, i):
        return self.g("gThingArray") + i * self.stride


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    stride = 0x14

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["gUIInterface"]: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def world(self):
        return CUR["world"]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def recorder(self, state, recording):
        base = self.e.alloc(0x40)
        vt = self.e.alloc(0x40)
        self.ptr(base + 0xC, vt)
        self.write(base, struct.pack(">I", state & MASK))
        self.write(base + 0x24, struct.pack(">I", recording & MASK))
        CUR["base"] = base
        return base

    def p1(self, action, a):
        T.call_ppc(self.e, FN1, [0, action & MASK, a])

    def p2(self, action, a, count, text):
        T.call_ppc(self.e, FN2, [0, action & MASK, a, count, text])

    def record(self, base, action, a, b, c):
        T.call_ppc(self.e, FN3, [base, action & MASK, a, b, c])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    stride = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("gUIInterface")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def world(self):
        if not hasattr(self.lib, "world_block"):
            self.lib.world_block = self.alloc(WORLD_SIZE)
        return self.lib.world_block

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def recorder(self, state, recording):
        return getattr(self.lib, P + "recorder")(s32(state), s32(recording))

    def p1(self, action, a):
        getattr(self.lib, P + "p1")(s32(action), a)

    def p2(self, action, a, count, text):
        getattr(self.lib, P + "p2")(s32(action), a, count, text)

    def record(self, base, action, a, b, c):
        getattr(self.lib, P + "record")(base, s32(action), a, b, c)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = getattr(lib, P + "addr")
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = getattr(lib, P + "alloc")
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    f = getattr(lib, P + "recorder")
    f.restype, f.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    getattr(lib, P + "p1").argtypes = [C.c_int32, C.c_uint32]
    getattr(lib, P + "p2").argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint64]
    getattr(lib, P + "record").argtypes = [C.c_uint64, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    ms_preset = 0
    s.ptr(s.g("gMasterSave"), 0)
    if case["ms"]:
        ms_preset = s.alloc(16)
        s.ptr(s.g("gMasterSave"), ms_preset)
    s.preset = ms_preset
    w = s.world()
    s.ptr(s.g("gGameWorld"), w)
    s.u16(s.g("gReplayChainEndA"), case["sentinel"])
    s.u16(s.g("gReplayChainEndB"), case["sentinel"])
    s.ptr(s.entry(0), s.alloc(0x20) if case["t0"] else 0)
    cell = case["a"]
    if cell <= 0x4000:
        s.u16(w + 0x2D8 + (cell - 1) * 0x44 + 0x24, case["first"])
    s.u16(w + 0x1DA732, case["gates"])
    for (i, kind, nxt) in case["nodes"]:
        obj = s.alloc(0x20)
        s.u16(obj, i)
        s.u8(obj + 2, kind)
        s.u16(obj + 0xA, nxt)
        s.ptr(s.entry(i), obj)


def put_text(s, chars):
    buf = s.alloc(16)
    s.write(buf, struct.pack(s.fmt[0] + "%dH" % (len(chars) + 1), *(list(chars) + [0])))
    return buf


def execute(s, case):
    op = case["op"]
    if op == "p1":
        s.p1(case["action"], case["a"])
    elif op == "p2":
        s.p2(case["action"], case["a"], case["count"], put_text(s, case["text"]))
    else:
        base = s.recorder(case["state"], case["recording"])
        s.record(base, case["action"], case["a"], case["b"], case["c"])
        s.base = base


def observe(s, case):
    ms = s.rd_ptr(s.g("gMasterSave"))
    return {"log": s.log(), "ms": 0 if ms == 0 else (1 if ms == s.preset else 2)}


def run(s, case):
    try:
        if case["op"] != "rec":
            build(s, case)
        else:
            s.preset = 0
            s.ptr(s.g("gMasterSave"), 0)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

ACTIONS_P2 = [0x50, 0x55]


def chain(r, c):
    k = r.choice([0, 1, 1, 2, 3, 4])
    ids = r.sample(range(1, 60), k)
    sentinel = r.choice([0, 0xFFFF, r.randrange(60, 100)])
    nodes = []
    for j, i in enumerate(ids):
        kind = 3 if r.random() < 0.3 else r.choice([0, 1, 2, 4, 0x13])
        nodes.append((i, kind, ids[j + 1] if j + 1 < len(ids) else sentinel))
    c["sentinel"] = sentinel
    c["first"] = ids[0] if ids else sentinel
    c["nodes"] = nodes
    return c


def base(r, op):
    c = {"op": op, "queues": {"filter": [r.choice([0, 1])], "sign": [r.randrange(0, 1 << 12)]}, "ms": r.choice([0, 1]),
         "t0": r.random() < 0.25, "a": 1, "gates": r.randrange(1, 60), "nodes": [], "first": 0, "sentinel": 0}
    return c


def p1_case(r, action=None, a=None):
    c = base(r, "p1")
    c["action"] = r.choice(list(range(0x47)) + [0xFFFF, -1, 0x7FFFFFFF, 0x80000000]) if action is None else action
    # the world holds 0x4000 map cells: the halfword stays within them (the handlers that only pass it on are the same)
    c["a"] = r.choice([0, 1, 2, 0x4000, 0x3FFF, r.randrange(1, 0x4000), r.randrange(1, 0x4000)]) if a is None else a
    return chain(r, c)


def p2_case(r, action=None, a=None):
    c = base(r, "p2")
    c["action"] = r.choice(ACTIONS_P2 + [0x4F, 0x51, 0x54, 0x56, 0, -1, 0x150, 0x155]) if action is None else action
    c["a"] = r.choice([60000, 1, 2, 0x4000, r.randrange(1, 0x4000), r.randrange(1, 0x4000), 59999, 60001 - 1]) if a is None else a
    if c["a"] > 0x4000 and c["a"] != 60000:
        c["a"] = 0x4000
    chain(r, c)
    n = r.choice([0, 1, 2, 3, 4, 5])
    c["text"] = [r.choice([0x41, 0x62, 0x3042, 0xFF, 0x1234, r.randrange(1, 0xFFFF)]) for _ in range(5)]
    c["count"] = n
    if c["a"] == 60000:
        c["nodes"].append((c["gates"], r.choice([0, 3, 1]), r.randrange(0, 100)))
    return c


def rec_case(r, action=None):
    return {"op": "rec", "queues": {}, "a": 1, "action": r.choice([0, 1, 0x50, 0xFF, 0x100, 0x1FF, -1, r.randrange(0, 300)]) if action is None else action,
            "state": r.choice([0, 1, 1, 5, -1]), "recording": r.choice([0, 0, 0, 1, -5]),
            "b": r.choice([0, 1, 0x1234, 0xFFFF, r.randrange(0, 0x10000)]), "c": r.choice([0, 0x8000, 0x00FF, 0xFF00, r.randrange(0, 0x10000)]),
            **{}}


def rec_fix(c, r):
    c["a"] = r.choice([0, 1, 0x1234, 0xFFFF, r.randrange(0, 0x10000)])
    return c


def p1_edges():
    r = random.Random(5362)
    for action in list(range(0x48)) + [0xFFFF, -1, 0x7FFFFFFF, 0x80000000]:
        for _ in range(3):
            yield p1_case(r, action)
    for _ in range(30):
        yield p1_case(r)


def p1_random(r):
    return p1_case(r)


def p2_edges():
    r = random.Random(5363)
    for action in ACTIONS_P2 + [0x4F, 0x51, 0x54, 0x56, 0, -1, 0x150, 0x155]:
        for a in (60000, 1, 0x4000, 7):
            for _ in range(4):
                yield p2_case(r, action, a)


def p2_random(r):
    return p2_case(r)


def rec_one(r, action=None):
    return rec_fix(rec_case(r, action), r)


def rec_edges():
    r = random.Random(5364)
    for action in (0, 1, 0x50, 0xFF, 0x100, 0x1FF, -1):
        for state, recording in ((1, 0), (0, 0), (1, 1), (0, 1), (-1, 0), (5, -5)):
            c = rec_one(r, action)
            c["state"], c["recording"] = state, recording
            yield c
    for a in (0, 1, 0x00FF, 0xFF00, 0x1234, 0xFFFF):
        c = rec_one(r, 3)
        c["state"], c["recording"], c["a"] = 1, 0, a
        yield c


def rec_random(r):
    return rec_one(r)


TESTS = [
    FnTest("CActionRecorder::ProcessAction@0x10145e38", ppc, host, edge=p1_edges, rnd=p1_random),
    FnTest("CActionRecorder::ProcessAction@0x101465a8", ppc, host, edge=p2_edges, rnd=p2_random),
    FnTest("CActionRecorder::RecordAction", ppc, host, edge=rec_edges, rnd=rec_random),
]
