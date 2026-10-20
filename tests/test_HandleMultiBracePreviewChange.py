"""Unit v5688, the multi brace preview of the coaster editor, against libtpw_unit_HandleMultiBracePreviewChange.dylib
(tests/unit_HandleMultiBracePreviewChange.cpp), main app ("SimTheme Park"), src/TPCoaster_ticks_HandleMultiBracePreviewChange.cpp:
  HandleMultiBracePreviewChange 0x10054ea8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's offsets) for the translation. The callees
outside the unit (FlagGenerateTrack, SetTrackPreview2ndBrace, GetPylonUnderCursor, MoveBrace, CalculateBraceCost, RoundUp,
IsValidPylonPosition, LLCC_SystemAddToSessionCost) are fakes on both sides that record their arguments (objects as the id word they hold) and
answer from the case's queues.

The world of a case: the input flags, a chain of braces (numbers rising along the prev links), pylon nodes holding braces, the ride anim set
table, the marks, the dragged brace, the costs, the recorder flag. What is compared: the log, the cost word, the marks and the flags of
every brace.

No fused operations (the two fctiwz read values far below 2^31). Not tested: a NULL the original reads unconditionally (a held brace that
is NULL while a brace was found, a missing next brace under flag 0x40).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HandleMultiBracePreviewChange"
MASK = 0xFFFFFFFF

FN = 0x10054EA8
SIZES = {FN: 1428}
OWN = set(SIZES)

CALLEES = dict(FlagGenerateTrack=0x10040F08, SetTrack=0x10053A04, GetPylon=0x1003F848, MoveBrace=0x10054978, Cost=0x10058924,
               RoundUp=0x1004DA1C, Valid=0x100411E4, Session=0x10091D50)

ADDR = dict(edit=0x102B68B0, w16bc=0x102C16BC, movez=0x102C16C4, movex=0x102C16C8, second=0x102C176C, costbefore=0x102C1770,
            movebrace=0x102C177C, mark2=0x102C1780, mark1=0x102C1784, w1788=0x102C1788, drag=0x102C178C, inflags=0x102C1794,
            rides=0x102C79A4, rec=0x1037EC44 + 0x24)

NAMES = ["inst.mLastBrace", "inst.mPylons", "inst.mBraces", "node.mFlags", "node.mNext", "node.mCount", "node.mBraces", "brace.mNumber",
         "brace.mFlags", "brace.mPrev", "brace.mNext", "brace.mWord70", "brace.mData", "data.mFlags", "data.mCellX", "data.mCellZ",
         "data.mF14", "data.mF18Table", "data.mF30", "ctl.mFCC"]
MAC_OFF = [0x54, 0x4C, 0x58, 4, 0x28, 0x30, 0x34, 0, 8, 0x64, 0x68, 0x70, 0x100, 4, 0xC, 0x10, 0x14, 0x18, 0x30, 0xCC]
HOST_OFF = {}

CUR = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def ride(w):
    return 0x4000 + 0x10 * w


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def idw(e, p):
    p &= MASK
    return -1 if p == 0 else s32(e.u32(p))


def nn(p):
    return 1 if (p & MASK) else 0


def st_flag(e):
    e.cl.rec("FlagGenerateTrack", (idw(e, e.r(3)), s32(e.r(4))))


def st_set(e):
    e.cl.rec("SetTrackPreview2ndBrace", (idw(e, e.r(3)), s32(e.r(4))))


def st_pylon(e):
    L = e.cl
    L.rec("GetPylonUnderCursor", (nn(e.r(3)), nn(e.r(4)), nn(e.r(5))))
    idx = L.ret("GetPylonUnderCursor")
    pyl = CUR["side"].pylons
    e.set_r(3, pyl[idx] if 0 <= idx < len(pyl) else 0)


def st_move(e):
    d = e.u32(e.r(1) + 0x38)
    e.cl.rec("MoveBrace", (idw(e, e.r(3)), idw(e, e.r(4)), idw(e, e.r(5)), e.r(6) & MASK, e.r(7) & MASK, nn(e.r(8)), nn(e.r(9)),
                           s32(e.r(10)), s32(d)))


def st_cost(e):
    L = e.cl
    L.rec("CalculateBraceCost", (idw(e, e.r(3)), s32(e.r(4)), idw(e, e.r(5)), nn(e.r(6)), nn(e.r(7)), nn(e.r(8)), nn(e.r(9))))
    e.set_f1(bitsf(L.ret("CalculateBraceCost")))


def st_round(e):
    L = e.cl
    L.rec("RoundUp", (canon_f(fb(e.f(1))),))
    e.set_r(3, L.ret("RoundUp") & MASK)


def st_valid(e):
    L = e.cl
    L.rec("IsValidPylonPosition", (e.r(3) & MASK, e.r(4) & MASK, nn(e.r(5)), e.r(6) & MASK, e.r(7) & MASK, e.r(8) & MASK, e.r(9) & MASK))
    e.set_r(3, L.ret("IsValidPylonPosition") & MASK)


def st_session(e):
    e.cl.rec("LLCC_SystemAddToSessionCost", (s32(e.r(3)),))


for _k, _f in (("FlagGenerateTrack", st_flag), ("SetTrack", st_set), ("GetPylon", st_pylon), ("MoveBrace", st_move), ("Cost", st_cost),
               ("RoundUp", st_round), ("Valid", st_valid), ("Session", st_session)):
    STUBS.internal(CALLEES[_k], _f)


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

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.pw))[0]

    def off(self, name):
        return self.OFFS[NAMES.index(name)]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pw = 4
    is_host = False
    OFFS = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.pylons = []
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        CUR["side"] = self
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

    def call(self, inst, prev, pointed, changed, x, z):
        T.call_ppc(self.e, FN, [inst & MASK, prev & MASK, pointed & MASK, changed & MASK, x & MASK, z & MASK])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pw = 8
    is_host = True

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.OFFS = HOST_OFF["v"]
        self.pylons = []
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hmbpc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hmbpc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, inst, prev, pointed, changed, x, z):
        arr = (C.c_uint64 * 16)(*self.pylons)
        self.lib.tpw_hmbpc_setpylons(arr, len(self.pylons))
        self.lib.tpw_hmbpc_call(inst, prev, pointed, s32(changed), x & MASK, z & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hmbpc_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setpylons").argtypes = [C.POINTER(C.c_uint64), C.c_int32]
    getattr(lib, p + "call").argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(NAMES), n
    HOST_OFF["v"] = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mkdata(s, d, ctls):
    a = s.alloc(0x100)
    s.words(a, [d["id"]])
    s.words(a + s.off("data.mFlags"), [d["flags"]])
    s.words(a + s.off("data.mCellX"), [d["cx"]])
    s.words(a + s.off("data.mCellZ"), [d["cz"]])
    s.words(a + s.off("data.mF14"), [d["f14"]])
    s.ptr(a + s.off("data.mF18Table"), ctls[d["ctl"]])
    s.words(a + s.off("data.mF30"), [d["f30"]])
    return a


def build(s, case):
    ctls = []
    for fcc in case["ctls"]:
        a = s.alloc(0x200)
        s.words(a + 0xCC, [fcc])
        ctls.append(a)
    braces = []
    s.bidx = {0: -1}
    for i, b in enumerate(case["braces"]):
        a = s.alloc(0x400)
        braces.append(a)
        s.bidx[a] = i
    s.braces = braces
    n = len(braces)
    for i, b in enumerate(case["braces"]):
        a = braces[i]
        s.words(a, [b["num"]])
        s.words(a + s.off("brace.mFlags"), [b["flags"]])
        s.ptr(a + s.off("brace.mPrev"), braces[i + 1] if i + 1 < n else 0)
        s.ptr(a + s.off("brace.mNext"), braces[i - 1] if i > 0 else 0)
        s.words(a + s.off("brace.mWord70"), [b["w70"]])
        s.ptr(a + s.off("brace.mData"), mkdata(s, b["data"], ctls))
    for d in case["extras"]:
        s.pylons.append(mkdata(s, d, ctls))
    insts = []
    for iid in (7, 8):
        a = s.alloc(0x400)
        s.words(a, [iid])
        insts.append(a)
    edit = insts[0]
    s.insts = insts
    if not case["edit_null"]:
        s.ptr(edit + s.off("inst.mLastBrace"), braces[0] if case["edit_last"] else 0)
        s.ptr(edit + s.off("inst.mBraces"), braces[case["head"]])
        nodes = []
        for nd in case["nodes"]:
            a = s.alloc(0x100)
            s.words(a + s.off("node.mFlags"), [nd["flags"]])
            s.words(a + s.off("node.mCount"), [len(nd["idxs"])])
            for k, bi in enumerate(nd["idxs"]):
                s.ptr(a + s.off("node.mBraces") + k * s.pw, braces[bi])
            nodes.append(a)
        for k, a in enumerate(nodes):
            s.ptr(a + s.off("node.mNext"), nodes[k + 1] if k + 1 < len(nodes) else 0)
        s.ptr(edit + s.off("inst.mPylons"), nodes[0] if nodes else 0)
        s.ptr(s.g("edit"), edit)
    else:
        s.ptr(s.g("edit"), 0)

    def bp(i):
        return braces[i] if i is not None else 0

    s.ptr(s.g("mark1"), bp(case["mark1"]))
    s.ptr(s.g("mark2"), bp(case["mark2"]))
    s.ptr(s.g("movebrace"), bp(case["mv"]))
    s.ptr(s.g("drag"), bp(case["drag"]))
    held = bp(case["held"])
    s.words(s.g("w1788"), [held & MASK])
    if s.is_host:
        s.ptr(s.g("start"), held)
    for w in range(1, 8):
        s.ptr(s.g("rides") + w * s.pw, ride(w))
    s.words(s.g("inflags"), [case["iflags"]])
    s.words(s.g("w16bc"), [case["w16bc"]])
    s.words(s.g("movex"), [case["movex"]])
    s.words(s.g("movez"), [case["movez"]])
    s.words(s.g("second"), [case["second"]])
    s.words(s.g("costbefore"), [case["before"]])
    s.words(s.g("rec"), [case["rec"]])


def execute(s, case):
    inst = s.insts[case["instarg"]]
    s.call(inst, 0x1234, case["pointed"] or 0, case["changed"], case["cx"], case["cz"])
    return 0


def observe(s, case, answer):
    def pid(v):
        return s.bidx.get(v, -2)

    return {"log": s.log(), "w16bc": s.rd_word(s.g("w16bc")), "mark1": pid(s.rd_ptr(s.g("mark1"))), "mark2": pid(s.rd_ptr(s.g("mark2"))),
            "flags": [s.rd_word(a + s.off("brace.mFlags")) for a in s.braces]}


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def mkd(r, i):
    return dict(id=100 + i, flags=r.choice([0, 2, 1, 3, 6, 2]), cx=r.randrange(4), cz=r.randrange(4), f14=r.randrange(3), ctl=r.randrange(2),
                f30=r.randrange(7))


def find(c):
    if c["pointed"] is None or c["edit_null"] or not c["edit_last"]:
        return None
    for nd in c["nodes"]:
        if nd["flags"] & 1:
            continue
        for i in nd["idxs"]:
            if ride(c["braces"][i]["w70"]) == c["pointed"]:
                return i
    return None


def rcost(r):
    k = r.random()
    if k < 0.2:
        return fb(float(r.randrange(-300, 300)))
    return fb(r.uniform(-900, 900))


def mkcase(r, iflags=None, changed=None):
    n = r.randrange(3, 8)
    v = r.choice([0, 0, 1, 5])
    braces = []
    for i in range(n):
        fl = r.choice([0, 0x40, 0x80, 0xC0, 0x4000, 0x1000000, 0x40, 0xC0, 0x20]) | r.choice([0, 0, 0x4000])
        braces.append(dict(num=v, flags=fl, w70=r.randrange(1, 7), data=mkd(r, i)))
        v += r.randrange(1, 4)
    mv = r.randrange(n)
    extras = []
    for k in range(3):
        d = mkd(r, 50 + k)
        d["id"] = 200 + k
        if r.random() < 0.5:
            d["f14"] = braces[mv]["data"]["f14"]
            d["ctl"] = braces[mv]["data"]["ctl"]
        extras.append(d)
    c = dict(ctls=[r.randrange(1, 7), r.randrange(1, 7)], braces=braces, extras=extras, mv=mv)
    c["nodes"] = [dict(flags=r.choice([0, 0, 1]), idxs=[r.randrange(n) for _ in range(r.randrange(0, 4))]) for _ in range(r.randrange(0, 4))]
    c["edit_null"] = r.random() < 0.05
    c["edit_last"] = r.random() < 0.85
    c["head"] = r.randrange(n)
    k = r.random()
    if k < 0.15:
        c["pointed"] = None
    elif k < 0.85:
        c["pointed"] = ride(braces[r.randrange(n)]["w70"])
    else:
        c["pointed"] = 0x9990
    if c["pointed"] is not None:
        c["edit_null"] = False
    f = find(c)
    c["held"] = r.choice([None] + list(range(n))) if f is None else (f if r.random() < 0.4 else r.randrange(n))
    if f is not None and c["held"] == f:
        fl = braces[f]["flags"]
        if fl & 0x40:
            if (fl & 0x80) and not f < n - 1:
                braces[f]["flags"] &= ~0x40
            elif not (fl & 0x80) and f < 1:
                braces[f]["flags"] &= ~0x40
    c["mark1"] = r.choice([None] + list(range(n)))
    c["mark2"] = r.randrange(n) if c["mark1"] is not None and r.random() < 0.7 else None
    c["drag"] = r.randrange(n) if r.random() < 0.85 else None
    c["iflags"] = r.choice([0, 0x80, 0x100, 0x180, 0x100, 0x80, 0x40 | 0x100, 0x200]) if iflags is None else iflags
    c["changed"] = r.choice([0, 1, 1, 5, -1]) if changed is None else changed
    c["cx"] = r.randrange(4)
    c["cz"] = r.randrange(4)
    c["movex"] = r.randrange(4)
    c["movez"] = r.randrange(4)
    if r.random() < 0.2:
        c["movex"], c["movez"] = c["cx"], c["cz"]
    c["w16bc"] = r.choice([0, 5, 0xFFFFFFF0, r.getrandbits(32), 100])
    c["second"] = rcost(r)
    c["before"] = rcost(r)
    c["rec"] = r.choice([0, 0, 0, 1, 2])
    c["instarg"] = r.randrange(2)
    c["queues"] = {"GetPylonUnderCursor": [r.choice([-1, 0, 1, 2])], "CalculateBraceCost": [rcost(r), rcost(r)],
                   "RoundUp": [r.randrange(-3000, 3000), r.randrange(-3000, 3000)], "IsValidPylonPosition": [r.choice([0, 1, 7])]}
    return c


def edges():
    r = random.Random(688)
    for iflags in (0, 0x80, 0x100, 0x180, 0x200):
        for changed in (0, 1):
            for _ in range(16):
                yield mkcase(r, iflags, changed)


def rnd(r):
    return mkcase(r)


TESTS = [
    FnTest("HandleMultiBracePreviewChange", ppc, host, edge=edges, rnd=rnd),
]
