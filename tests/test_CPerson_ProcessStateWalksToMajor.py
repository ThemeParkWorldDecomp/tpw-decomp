"""Unit v5563, CPerson::ProcessStateWalksToMajor and CPerson::DoSomeThinking, against libtpw_unit_CPerson_ProcessStateWalksToMajor.dylib
(tests/unit_CPerson_ProcessStateWalksToMajor.cpp), main app ("SimTheme Park"), src/CPerson_..._CPerson_ProcessStateWalksToMajor.cpp:
  CPerson::ProcessStateWalksToMajor 0x1011cf18, CPerson::DoSomeThinking 0x10117fd4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the peep, the ride's
queue, the thoughts, the person's own state machine, the world, the park analyser, CTRACE and GAME_ASSERTMSG) is a fake on both sides that
records its arguments (a text as the sum of its bytes) and answers from the case's queues.

The world of a case: a person block of 0x240 random bytes with the fields the code reads set (cell bytes +4 and +6, counter +0x2c, ride +0x1dc,
floats +0x19c and +0x1a4 to +0x1b0, +0x1fc, +0x20c), gThingArray rows 0 to 7, the world pointer, gSoundBanks and the two globals
gNoMajorRide / gNoMajorCell. What is compared: the event log and the whole person block (typed reads for the words the code writes).

No fused operations; fctiwz only sees values below 2^31 (Unicorn saturates wrongly above). Not tested: a NaN or huge float in the person's
floats (Unicorn's fctiwz differs from the hardware there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_ProcessStateWalksToMajor"
MASK = 0xFFFFFFFF

FN_WALKS, FN_THINK = 0x1011CF18, 0x10117FD4
SIZES = {FN_WALKS: 1200, FN_THINK: 872}
OWN = set(SIZES)

ADDR = dict(gThingArray=0x1034CBFC, gWorld=0x1037EC0C, gSoundBanks=0x102F7C94, gNoMajorRide=0x102A0FF8, gNoMajorCell=0x102A0FFA)
PPC_STRIDE = 20
HOST = {"stride": 0}
HOST_LAYOUT_HEAD = 0   # offsetof(mThing) in CThingArrayEntry

PERS = {}   # name -> Mac address of the persistent blocks (setup)
LABELS = {"person": 1, "object": 2, "thoughts": 3, "world": 4, "analyser": 5, "cell": 6}
PBLK = 0x240

# excised from the raw comparison: fields written by typed packers (endian dependent) are compared through typed reads
FIELDS = [(4, 4), (0x2C, 4), (0x19C, 4), (0x1A4, 16), (0x1DC, 4), (0x1FC, 4), (0x20C, 4)]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def csum(text):
    if isinstance(text, str):
        text = text.encode("latin-1")
    return sum(text) & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(e, a):
    return e.cl.lab(a)


def mk(name, f):
    return name, f


def st_assert(e):
    e.cl.rec("assert", (e.r(3) & MASK, csum(e.cstr(e.r(4)))))


def st_ctrace(e):
    fmt = e.cstr(e.r(4))
    n = (fmt.decode("latin-1") if isinstance(fmt, bytes) else fmt).count("%")
    e.cl.rec("CTRACE", tuple([e.r(3) & MASK, csum(fmt)] + [e.r(5 + i) & MASK for i in range(n)]))


def st_peepwalks(e):
    e.cl.rec("PeepWalks", (lab(e, e.r(3)),))
    e.set_r(3, e.cl.ret("PeepWalks") & MASK)


def st_setdest(e):
    e.cl.rec("SetDest", (lab(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.cl.ret("SetDest") & MASK)


def st_playsound(e):
    e.cl.rec("PlaySound", (lab(e, e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def st_back(e):
    e.cl.rec("GetBackOfQueue", (lab(e, e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetBackOfQueue") & 0xFFFF))


def st_free(e):
    e.cl.rec("HasFreeQueueSpace", (lab(e, e.r(3)),))
    e.set_r(3, e.cl.ret("HasFreeQueueSpace") & MASK)


def st_excite(e):
    e.cl.rec("GetExcitementLevel", (lab(e, e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, e.cl.ret("GetExcitementLevel") & MASK)


def st_toolong(e):
    e.cl.rec("QueueTooLong", (lab(e, e.r(3)),))
    e.set_r(3, e.cl.ret("QueueTooLong") & MASK)


def st_addperson(e):
    e.cl.rec("AddPersonToQueue", (lab(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def st_removeperson(e):
    e.cl.rec("RemovePersonFromQueue", (lab(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def st_thingid(e):
    l = lab(e, e.r(4))
    e.cl.rec("GetThingId", (l,))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("ThingId1" if l == 1 else "ThingId2") & 0xFFFF))


def st_record(e):
    e.cl.rec("RecordEvent", (lab(e, e.r(3)), e.r(4) & MASK, (e.r(5) >> 16) & 0xFFFF))


def st_bubble(e):
    e.cl.rec("ShowThoughtBubble", (lab(e, e.r(3)), e.r(4) & MASK, e.r(5) & 0xFF))


def st_lower(e):
    e.cl.rec("LowerHappiness", (lab(e, e.r(3)), e.r(4) & MASK))


def st_setstate(e):
    e.cl.rec("SetState", (lab(e, e.r(3)), e.r(4) & MASK))


def st_dismiss(e):
    e.cl.rec("DismissFromQueue", (lab(e, e.r(3)),))


def st_finddest(e):
    e.cl.rec("FindQueueDestination", (lab(e, e.r(3)),))
    e.set_r(3, e.cl.ret("FindQueueDestination") & MASK)


def st_score(e):
    e.cl.rec("ScoreOnCloserInspection", (lab(e, e.r(3)), lab(e, e.r(4))))
    e.set_r(3, e.cl.ret("Score") & MASK)


def st_addride(e):
    e.cl.rec("AddRide", (lab(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def st_minor(e):
    e.cl.rec("LookForMinorDest", (lab(e, e.r(3)),))


def st_closed(e):
    e.cl.rec("IsParkClosed", (lab(e, e.r(3)),))
    e.set_r(3, e.cl.ret("ParkClosed") & MASK)


def st_analyser(e):
    e.cl.rec("GetParkAnalyser", (lab(e, e.r(3)),))
    e.set_r(3, PERS["analyser"])


def st_cell(e):
    e.cl.rec("GetAnalyserCell", (lab(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, PERS["cell"])


def st_walkable(e):
    e.cl.rec("GetNumWalkableCells", (lab(e, e.r(3)), e.r(4) & MASK))
    e.set_r(3, e.cl.ret("Walkable") & MASK)


def st_littered(e):
    e.cl.rec("GetNumLitteredCells", (lab(e, e.r(3)), e.r(4) & MASK))
    e.set_r(3, e.cl.ret("Littered") & MASK)


for addr, fn in [(0x1000DDE8, st_assert), (0x1006FCBC, st_ctrace), (0x10114FA8, st_peepwalks), (0x10115400, st_setdest),
                 (0x10115B80, st_playsound), (0x101092F4, st_back), (0x101085A0, st_free), (0x1010CFB0, st_excite),
                 (0x10108818, st_toolong), (0x10108A28, st_addperson), (0x10108C20, st_removeperson), (0x1012E1F8, st_thingid),
                 (0x1012F3D4, st_record), (0x1012F0D8, st_bubble), (0x1011B384, st_lower), (0x10120354, st_setstate),
                 (0x1011F1B8, st_dismiss), (0x1011EF14, st_finddest), (0x10119068, st_score), (0x10119AC8, st_addride),
                 (0x1011915C, st_minor), (0x1013FF7C, st_closed), (0x1013EBC8, st_analyser), (0x100E4C38, st_cell),
                 (0x100E659C, st_walkable), (0x100E670C, st_littered)]:
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name, size in (("person", PBLK), ("object", 0x40), ("world", 0x40), ("analyser", 0x40), ("cell", 0x40)):
        PERS[name] = T.persist_alloc(runner.emu, size)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, data):
        self.write(a, data)

    def h(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def w(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "I", v & MASK))

    def rd_h(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def rd_w(self, a):
        return struct.unpack(self.fmt[0] + "I", self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {PERS["person"]: 1, PERS["object"]: 2, PERS["person"] + 0x30: 3, PERS["world"]: 4, PERS["analyser"]: 5, PERS["cell"]: 6}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name] if name in ADDR else PERS[name]

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def thing_row(self, i):
        return ADDR["gThingArray"] + i * PPC_STRIDE

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FN_WALKS if op == "walks" else FN_THINK, [PERS["person"]])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {1: self.g("person"), 2: self.g("object"), 3: self.g("person") + 0x30, 4: self.g("world"), 5: self.g("analyser"), 6: self.g("cell")}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_wtm_addr(name.encode())

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def thing_row(self, i):
        return self.g("gThingArray") + i * HOST["stride"]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        (self.lib.tpw_wtm_walks if op == "walks" else self.lib.tpw_wtm_think)(self.g("person"))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_wtm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("walks", "think"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lay = list(out)[:n]
    assert lay[1] == 0 and lay[2] == 4, lay
    HOST["stride"] = lay[0]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    p = s.g("person")
    s.write(p, case["blk"])
    s.write(p + 4, bytes([case["x"], 0, case["y"]]) if False else b"")
    s.write(p + 4, bytes([case["x"]]))
    s.write(p + 6, bytes([case["y"]]))
    s.write(p + 0x2C, bytes([case["counter"]]))
    s.h(p + 0x1DC, case["ride"])
    s.h(p + 0x1DE, case["h1de"])
    s.w(p + 0x19C, case["f19c"])
    s.w(p + 0x1A4, case["f1a4"])
    s.w(p + 0x1A8, case["f1a8"])
    s.w(p + 0x1AC, case["f1ac"])
    s.w(p + 0x1B0, case["f1b0"])
    s.w(p + 0x1FC, case["w1fc"])
    s.w(p + 0x20C, case["w20c"])
    for i in range(8):
        s.ptr(s.thing_row(i), s.g("object") if case["rows"][i] else 0)
    s.ptr(s.g("gWorld"), s.g("world"))
    for i, v in enumerate(case["banks"]):
        s.w(s.g("gSoundBanks") + 4 * i, v)
    s.h(s.g("gNoMajorRide"), case["nomajor"])
    s.h(s.g("gNoMajorCell"), case["nocell"])


def observe(s, case):
    p = s.g("person")
    raw = s.read(p, PBLK)
    keep = bytearray(raw)
    for off, n in FIELDS:
        keep[off:off + n] = b"\0" * n
    return {"log": s.log(), "raw": bytes(keep), "x": s.read(p + 4, 1), "y": s.read(p + 6, 1), "counter": s.read(p + 0x2C, 1),
            "ride": s.rd_h(p + 0x1DC), "h1de": s.rd_h(p + 0x1DE), "w19c": s.rd_w(p + 0x19C), "fl": [s.rd_w(p + o) for o in (0x1A4, 0x1A8, 0x1AC, 0x1B0)],
            "w1fc": s.rd_w(p + 0x1FC), "w20c": s.rd_w(p + 0x20C), "rides": s.rd_h(s.g("gNoMajorRide")), "cell": s.rd_h(s.g("gNoMajorCell"))}


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

INTS = [0, 1, 5, 9, 10, 11, 50, 89, 90, 91, 98, 99, 100, 101, 127, 128, 200, 255, 256, 346, 355, 356, 1000, 100000, -1, -9, -10, -100, -256, -255]


def rfloat_word(r):
    k = r.random()
    if k < 0.6:
        v = r.choice(INTS)
        frac = r.choice([0.0, 0.0, 0.5, 0.99, 0.999, -0.5])
        return fb(v + frac if v + frac == v + frac else v)
    if k < 0.8:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(0, 120))


def base(r, op):
    x, y = r.randrange(256), r.randrange(256)
    place = (x + y * 128 + 1) & 0xFFFF
    ride = r.randrange(0, 8)
    c = {"op": op, "x": x, "y": y, "place": place, "ride": ride, "h1de": r.randrange(65536), "counter": r.choice([0, 1, 5, 10, 11, 12, 255, r.randrange(256)]),
         "blk": bytes(r.randrange(256) for _ in range(PBLK)), "w1fc": r.randrange(1 << 32), "w20c": r.randrange(1 << 32),
         "f19c": rfloat_word(r), "f1a4": rfloat_word(r), "f1a8": rfloat_word(r), "f1ac": rfloat_word(r), "f1b0": rfloat_word(r),
         "rows": [r.choice([0, 1]) for _ in range(8)], "banks": [r.randrange(1 << 32) for _ in range(8)],
         "nomajor": r.choice([ride, r.randrange(8), r.randrange(65536)]), "nocell": r.choice([0, place, r.randrange(65536)])}
    back = place if r.random() < 0.6 else r.choice([r.randrange(65536), (place + 1) & 0xFFFF, (place - 1) & 0xFFFF])
    back2 = r.choice([c["nocell"], r.randrange(65536), place])
    c["queues"] = {"PeepWalks": [r.choice([0, 0, 0, 1, 2, 3, -1])], "GetBackOfQueue": [back, back2], "HasFreeQueueSpace": [r.choice([0, 1, 1])],
                   "Score": [r.choice([r.randrange(-100, 100), 44, 45, -44, -45, 0, -(1 << 31), (1 << 31) - 1])],
                   "GetExcitementLevel": [r.choice([0, 1, 5, 255])], "QueueTooLong": [r.choice([0, 1])], "FindQueueDestination": [r.choice([0, 1])],
                   "SetDest": [r.choice([0, 1])], "ParkClosed": [r.choice([0, 1])],
                   "ThingId1": [r.choice([0x10, 0x20, 0x1230, 0x1234, 0, 0xFFFF, r.randrange(65536)])], "ThingId2": [r.choice([0x10, 0x4321, r.randrange(65536)])],
                   "Walkable": [r.choice([0, 1, 5, 10, 100, 1000, -5, 16777217, r.randrange(1, 5000)])],
                   "Littered": [r.choice([0, 1, 2, 3, 20, 200, 16777217, r.randrange(0, 1000)])]}
    return c


def over(r, op, queues=None, **kw):
    c = base(r, op)
    for k, v in kw.items():
        c[k] = v
    for k, v in (queues or {}).items():
        c["queues"][k] = v
    if kw.get("match") is not None:
        c["queues"]["GetBackOfQueue"][0] = c["place"] if kw["match"] else (c["place"] + 7) & 0xFFFF
    return c


def walks_edges():
    r = random.Random(5563)
    for peep in (1, 2, 3, -1):
        for closed in (0, 1):
            for counter in (0, 10, 11, 255):
                yield over(r, "walks", {"PeepWalks": [peep], "ParkClosed": [closed]}, counter=counter)
    for nm in (0, 1):
        yield over(r, "walks", {"PeepWalks": [0]}, nomajor=0 if nm else 3, ride=0 if nm else 3)
    for second in (0, 1):
        for same in (0, 1):
            c = over(r, "walks", {"PeepWalks": [0], "SetDest": [second]}, match=False, nomajor=(5 if same else 6), ride=5)
            c["nocell"] = 0x1111
            c["queues"]["GetBackOfQueue"] = [0x2222, 0x1111 if same else 0x3333]
            yield c
    for free in (0, 1):
        for score in (-45, -44, 44, 45, 0, -(1 << 31), 100000, -100000):
            for exc in (0, 1):
                for toolong in (0, 1):
                    for fd in (0, 1):
                        yield over(r, "walks", {"PeepWalks": [0], "HasFreeQueueSpace": [free], "Score": [score], "GetExcitementLevel": [exc], "QueueTooLong": [toolong],
                                                "FindQueueDestination": [fd]}, match=True, nomajor=9, ride=2)
    yield over(r, "walks", {"PeepWalks": [0]}, match=True, nomajor=9, ride=0, rows=[1] * 8)
    yield over(r, "walks", {"PeepWalks": [0]}, match=True, nomajor=9, ride=4, rows=[0] * 8)


def walks_random(r):
    return base(r, "walks")


def think_case(r, **kw):
    c = base(r, "think")
    for k, v in kw.items():
        if k in ("Walkable", "Littered", "ThingId1"):
            c["queues"][k] = [v]
        else:
            c[k] = v
    return c


def think_edges():
    r = random.Random(10117)
    fw = fb
    big, small = fw(100.0), fw(50.0)
    for a8, a4 in ((91.0, 91.0), (91.0, 90.0), (90.0, 91.0), (355.0, 355.0), (100.0, 50.0), (99.0, 50.0), (99.9, 50.0), (50.0, 100.0), (50.0, 99.0), (0.0, 0.0)):
        for ac, b0 in ((50.0, 50.0), (100.0, 50.0), (50.0, 100.0), (99.0, 99.0)):
            yield think_case(r, f1a8=fw(a8), f1a4=fw(a4), f1ac=fw(ac), f1b0=fw(b0))
    quiet = dict(f1a8=small, f1a4=small, f1ac=small, f1b0=small)
    for walk, lit in ((5, 1), (10, 2), (15, 3), (4, 1), (0, 0), (0, 3), (-5, -1), (16777217, 3355443), (16777217, 3355444), (1000, 201), (1000, 200), (1000, 199)):
        yield think_case(r, Walkable=walk, Littered=lit, **quiet)
    for happy in (9.0, 9.99, 10.0, 10.5, 50.0, 98.99, 99.0, 99.5, 100.0, 255.0, 256.0, 266.0, -1.0, -5.5, 0.0):
        for lit in (0, 1):
            yield think_case(r, f19c=fw(happy), Walkable=100, Littered=lit * 50, **quiet)
    for tid in (0x10, 0x20, 0x11, 0x1F, 0, 0xFFF0, 0xFFFF):
        for f1ac in (50.0, 100.0):
            yield think_case(r, ThingId1=tid, f1ac=fw(f1ac), f1a8=small, f1a4=small, f1b0=small)
        for f19c in (9.0, 100.0):
            yield think_case(r, ThingId1=tid, f19c=fw(f19c), Walkable=100, Littered=0, **quiet)
        yield think_case(r, ThingId1=tid, f1a8=big, f1a4=small, f1ac=small, f1b0=small)
        yield think_case(r, ThingId1=tid, f1a8=small, f1a4=small, f1ac=small, f1b0=big)
        yield think_case(r, ThingId1=tid, f1a8=small, f1a4=small, f1ac=small, f1b0=small, Walkable=10, Littered=9)


def think_random(r):
    return base(r, "think")


TESTS = [
    FnTest("CPerson::ProcessStateWalksToMajor", ppc, host, edge=walks_edges, rnd=walks_random),
    FnTest("CPerson::DoSomeThinking", ppc, host, edge=think_edges, rnd=think_random),
]
