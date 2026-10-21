"""Unit v5967, the pylon-move abort and six small coaster/heightfield functions, against libtpw_unit_AbortMovePylon.dylib
(tests/unit_AbortMovePylon.cpp), main app ("SimTheme Park"), src/game/park/TPHField.cpp, src/game/rides/CoasterEdit.cpp, src/game/rides/CoasterTrack.cpp:
  AbortMovePylon 0x10057df4, SingleHFieldTextureChange 0x1005b840, PlacePylon (4 arguments) 0x10052a30, IsCoasterUseable 0x10052954,
  IsTrackValid 0x100528d8, IsTrackClosed (void*) 0x10052850, DecCoasterConst2 0x10052628

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, host offsets and 8-byte pointers) for the translation; the same code reads
both back. The callees outside the unit (FlagGenerateTrack, MoveBrace, LLCC_AbortPylonMove, LockPointedAtRide,
DisableCurrentPylonEditState, SetHeightfieldTexture, TPForce3DRefresh, FindCoasterInstanceFromEntranceID, IsTrackClosed(instance), CTRACE and
the 7-argument PlacePylon) are fakes on both sides that record their arguments (objects as labels) and answer from the case.

The world of a case: the pylon-move globals, a chain of four braces linked through +0x64, the dragged brace, two instances, a heightfield header,
a tile, a ride object. What is compared: the answer, every global and brace flag word written, the out word of PlacePylon and the event log.

No fused operations. Not tested: a NULL mark1 when mark2 is set, a NULL dragged brace (both dereferenced unconditionally by the original),
and mark chains that never reach the end brace (an endless walk).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AbortMovePylon"
MASK = 0xFFFFFFFF

F_ABORT, F_SINGLE, F_PLACE, F_USABLE, F_VALID, F_CLOSED, F_DEC = 0x10057DF4, 0x1005B840, 0x10052A30, 0x10052954, 0x100528D8, 0x10052850, 0x10052628
SIZES = {F_ABORT: 356, F_SINGLE: 108, F_PLACE: 108, F_USABLE: 180, F_VALID: 88, F_CLOSED: 96, F_DEC: 132}
OWN = set(SIZES)

C_FLAG, C_MOVE, C_LLCC, C_LOCK, C_DISABLE = 0x10040F08, 0x10054978, 0x1008F1A4, 0x10067288, 0x10053B58
C_SETHF, C_REFRESH, C_FIND, C_ITC, C_CTRACE, C_PLACE7 = 0x100B8080, 0x1005B68C, 0x10043380, 0x10052780, 0x1006FCBC, 0x10041C5C

ADDR = dict(edit=0x102B68B0, kind=0x102B74A8, w16bc=0x102C16BC, movez=0x102C16C4, movex=0x102C16C8, w16e0=0x102C16E0, orig=0x102C1774,
            brace=0x102C177C, mark2=0x102C1780, mark1=0x102C1784, flags=0x102C1794, hdr=0x102C6C7C, table=0x1029CC2C, rec=0x1037EC44)

MAC = dict(i_type=4, i_b3c=0x3C, i_f140=0x140, i_size=0x150, b_num=0, b_flags=8, b_prev=0x64, b_data=0x100, b_w70=0x70, b_size=0x110,
           h_flags=0x30, h_size=0x80, rec=0x24, rec_size=0x40)
HL = {}                                  # the host's, from tpw_av_layout
CUR = [None]                             # the side of the running case, read by the PowerPC fakes


def s32(v):
    return T.s32(v)


def canon_d(hi, lo):
    if (hi & 0x7FF00000) == 0x7FF00000 and ((hi & 0xFFFFF) or lo):
        return 0x7FF80000, 0
    return hi, lo


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab(p):
    return CUR[0].L.lab(p & MASK)


def st_flag(e):
    CUR[0].L.rec("FlagGenerateTrack", (lab(e.r(3)), s32(e.r(4))))


def st_move(e):
    CUR[0].L.rec("MoveBrace", (lab(e.r(3)), lab(e.r(4)), lab(e.r(5)), e.r(6) & MASK, e.r(7) & MASK, lab(e.r(8)), lab(e.r(9)), s32(e.r(10)),
                               s32(e.u32((e.r(1) + 0x38) & MASK))))


def st_llcc(e):
    CUR[0].L.rec("LLCC_AbortPylonMove", ())


def st_lock(e):
    CUR[0].L.rec("LockPointedAtRide", (s32(e.r(3)),))


def st_disable(e):
    CUR[0].L.rec("DisableCurrentPylonEditState", ())


def st_sethf(e):
    CUR[0].L.rec("SetHeightfieldTexture", (lab(e.r(3)), lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), s32(e.r(9))))


def st_refresh(e):
    CUR[0].L.rec("TPForce3DRefresh", (e.r(3) & MASK,))


def st_find(e):
    CUR[0].L.rec("Find", (e.r(3) & MASK,))
    e.set_r(3, CUR[0].slots["find"])


def st_itc(e):
    L = CUR[0].L
    L.rec("ITC", (lab(e.r(3)),))
    e.set_r(3, L.ret("ITC") & MASK)


def st_ctrace(e):
    s = e.cstr(e.r(4))
    raw = s.encode("latin1") if isinstance(s, str) else bytes(s)
    parts = []
    for a, b in ((5, 6), (7, 8)):                  # the doubles travel in r5..r8
        hi, lo = canon_d(e.r(a) & MASK, e.r(b) & MASK)
        parts += [hi, lo]
    CUR[0].L.rec("CTRACE", (e.r(3) & MASK, sum(raw), len(raw)) + tuple(parts))


def st_place7(e):
    cur = CUR[0]
    cur.L.rec("PlacePylon7", (e.r(3) & MASK, e.r(4) & MASK, s32(e.r(5)), e.r(8) & 0xFF, e.r(9) & MASK))
    e.write(e.r(6) & MASK, struct.pack(">I", cur.slots["pylon"]))
    e.write(e.r(7) & MASK, struct.pack(">I", cur.slots["brace"]))
    e.set_r(3, cur.L.ret("PP") & MASK)


for _a, _f in ((C_FLAG, st_flag), (C_MOVE, st_move), (C_LLCC, st_llcc), (C_LOCK, st_lock), (C_DISABLE, st_disable), (C_SETHF, st_sethf),
               (C_REFRESH, st_refresh), (C_FIND, st_find), (C_ITC, st_itc), (C_CTRACE, st_ctrace), (C_PLACE7, st_place7)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

LABELS = dict(E=1, A=2, C0=3, C1=4, C2=5, C3=6, D=7, P=8, Q=9, H=10)
TABLE_LABEL = 11


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rdptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def make_objects(self):
        sz = dict(E=self.O["i_size"], A=self.O["i_size"], D=self.O["b_size"], H=self.O["h_size"], P=16, Q=16)
        for i in range(4):
            sz["C%d" % i] = self.O["b_size"]
        sz.update(T=0x20, tile=16, slot=16, obj=0x40)
        self.o = {k: self.alloc(v) for k, v in sz.items()}
        self.rev = {self.o[k]: v for k, v in LABELS.items()}
        self.rev[self.g("table")] = TABLE_LABEL
        self.rev[0] = 0

    def lbl(self, p):
        return self.rev.get(p, -1)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.slots = {}
        self.make_objects()
        fixed = {self.o[k]: v for k, v in LABELS.items()}
        fixed[ADDR["table"]] = TABLE_LABEL
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        CUR[0] = self
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

    def slot(self, name, v):
        self.slots[name] = v

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return 0 if fn in (F_ABORT, F_SINGLE) else r3 & MASK      # those two are void


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = HL
        self.make_objects()
        fixed = {v: self.o[k] for k, v in LABELS.items()}
        fixed[TABLE_LABEL] = self.g("table")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_av_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_av_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def slot(self, name, v):
        self.ptr(self.g("slot_" + name), v)

    def call(self, fn, args):
        lib = self.lib
        if fn == F_ABORT:
            lib.tpw_av_abort()
            return 0
        if fn == F_SINGLE:
            lib.tpw_av_single(s32(args[0]), s32(args[1]), args[2])
            return 0
        if fn == F_PLACE:
            return lib.tpw_av_place(args[0] & MASK, args[1] & MASK, s32(args[2]), args[3]) & MASK
        if fn == F_USABLE:
            return lib.tpw_av_usable(args[0]) & MASK
        if fn == F_VALID:
            return lib.tpw_av_valid(args[0]) & MASK
        if fn == F_CLOSED:
            return lib.tpw_av_closed(args[0]) & MASK
        return lib.tpw_av_dec() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_av_addr.restype, lib.tpw_av_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_av_alloc.restype, lib.tpw_av_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_av_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_av_abort.restype, lib.tpw_av_abort.argtypes = None, []
    lib.tpw_av_single.restype, lib.tpw_av_single.argtypes = None, [C.c_int32, C.c_int32, C.c_uint64]
    lib.tpw_av_place.restype, lib.tpw_av_place.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32, C.c_int32, C.c_uint64]
    for n in ("usable", "valid", "closed"):
        f = getattr(lib, "tpw_av_" + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_av_dec.restype, lib.tpw_av_dec.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_av_layout(out)
    v = list(out)[:n]
    names = ["i_type", "i_b3c", "i_f140", "i_size", "b_num", "b_flags", "b_prev", "b_data", "b_w70", "b_size", "h_flags", "h_size", "rec"]
    HL.update(zip(names, v))
    HL["rec_size"] = 0x40
    assert HL["b_num"] == 0 and HL["b_flags"] == 8 and HL["h_flags"] == 0x30, HL
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    o, O = s.o, s.O
    s.words(s.g("flags"), [c["flags"]])
    s.words(s.g("w16bc"), [c["w16bc"]])
    s.words(s.g("w16e0"), [c["w16e0"]])
    s.words(s.g("kind"), [c["kind"]])
    s.words(s.g("movex"), [c["movex"]])
    s.words(s.g("movez"), [c["movez"]])
    s.words(s.g("rec") + O["rec"], [c["rec"]])
    # braces
    for i in range(4):
        b = o["C%d" % i]
        s.words(b + O["b_num"], [c["nums"][i]])
        s.words(b + O["b_flags"], [c["cflags"][i]])
        s.ptr(b + O["b_prev"], o["C%d" % (i + 1)] if i < 3 else 0)
    P, Q = o["P"], o["Q"]
    pv = {0: 0, 1: P, 2: Q}
    s.words(o["D"] + O["b_flags"], [c["dflags"]])
    s.ptr(o["D"] + O["b_data"], pv[c["ddata"]])
    s.words(o["D"] + O["b_w70"], [c["w70"]])
    s.ptr(s.g("orig"), pv[c["orig"]])
    s.ptr(s.g("brace"), o["D"])
    s.ptr(s.g("mark1"), o["C%d" % c["m1"]] if c["m1"] is not None else 0)
    s.ptr(s.g("mark2"), o["C%d" % c["m2"]] if c["m2"] is not None else 0)
    # instances
    s.ptr(s.g("edit"), o["E"] if c["edit"] else 0)
    s.words(o["T"] + 8, [c["tf1"], c["tf2"]])
    s.ptr(o["E"] + O["i_type"], o["T"] if c["has_type"] else 0)
    s.words(o["E"] + O["i_f140"], [c["e140"]])
    s.words(o["E"] + O["i_b3c"], [c["eb3c"]])
    s.words(o["A"] + O["i_f140"], [c["a140"]])
    s.words(o["A"] + O["i_b3c"], [c["ab3c"]])
    # heightfield
    s.words(o["H"] + O["h_flags"], [c["hflags"]])
    s.ptr(s.g("hdr"), o["H"] if c["hdr"] else 0)
    s.words(o["tile"], c["tile"])
    # place and find
    s.slot("find", {0: 0, 1: o["E"], 2: o["A"]}[c["find"]])
    s.slot("pylon", o["P"] if c["pylon"] else 0)
    s.slot("brace", o["D"])
    s.words(o["obj"] + 0x1C, [c["eid"]])
    s.words(o["slot"], [0xDEADBEEF])


def execute(s, c):
    o = s.o
    op = c["op"]
    obj = o["obj"] if c["obj"] else 0
    if op == "abort":
        return s.call(F_ABORT, [])
    if op == "single":
        return s.call(F_SINGLE, [c["x"], c["y"], o["tile"]])
    if op == "place":
        return s.call(F_PLACE, [c["x"], c["z"], c["pflag"], o["slot"] if c["hasslot"] else 0])
    fn = dict(usable=F_USABLE, valid=F_VALID, closed=F_CLOSED).get(op)
    if fn:
        return s.call(fn, [obj])
    return s.call(F_DEC, [])


def observe(s, c, answer):
    o = s.o
    d = {"answer": answer, "log": s.log()}
    for n in ("flags", "w16bc", "w16e0", "kind", "movex", "movez"):
        d[n] = s.rd_word(s.g(n))
    for n in ("edit", "orig", "brace", "mark1", "mark2", "hdr"):
        d[n] = s.lbl(s.rdptr(s.g(n)))
    d["cflags"] = [s.rd_word(o["C%d" % i] + s.O["b_flags"]) for i in range(4)]
    d["dflags"] = s.rd_word(o["D"] + s.O["b_flags"])
    d["slot"] = s.rd_word(o["slot"])
    return d


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def w(r):
    return r.choice([0, 1, 2, 0x100, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32), r.getrandbits(8)])


def base(r, op):
    c = {"op": op}
    for n in ("flags", "w16bc", "w16e0", "kind", "movex", "movez", "dflags", "w70", "eid", "e140", "eb3c", "a140", "ab3c", "hflags", "x", "z", "y"):
        c[n] = w(r)
    c["cflags"] = [r.getrandbits(32) for _ in range(4)]
    c["rec"] = r.choice([0, 1, 5, 0x80000000])
    c["ddata"] = r.choice([0, 1, 2])
    c["orig"] = r.choice([0, 1, 2])
    c["edit"] = r.random() < 0.9
    c["has_type"] = r.random() < 0.85
    c["tf1"], c["tf2"] = rf(r), rf(r)
    c["hdr"] = r.random() < 0.8
    c["tile"] = [w(r), w(r), w(r)]
    c["find"] = r.choice([0, 1, 2, 2])
    c["pylon"] = r.random() < 0.5
    c["obj"] = r.random() < 0.9
    c["hasslot"] = r.random() < 0.7
    c["pflag"] = r.choice([0, 1, -1, 5])
    c["x"], c["z"] = r.getrandbits(32), r.getrandbits(32)
    # the mark chain
    i = r.randrange(4)
    j = r.randrange(i, 4)
    mode = r.choice(["a", "b", "eq", "same", "none", "none"])
    nums = [r.getrandbits(32) for _ in range(4)]
    m1 = m2 = None
    if mode == "none":
        m1 = r.choice([None, r.randrange(4)])
    elif mode == "same" or i == j:
        m1 = m2 = i
    elif mode == "a":
        m2, m1 = i, j
        lo = r.choice([r.randrange(0, 0xFFFFFFF0), r.randrange(0, 0x80000000), 0x7FFFFFFF])
        nums[m2] = lo
        nums[m1] = r.choice([lo + 1, r.randrange(lo + 1, 0x100000000), 0xFFFFFFFF if lo < 0xFFFFFFFF else lo + 0])
    elif mode == "b":
        m1, m2 = i, j
        lo = r.randrange(0, 0xFFFFFFF0)
        nums[m1] = lo
        nums[m2] = r.randrange(lo, 0x100000000)
    else:
        m1, m2 = i, j
        nums[m1] = nums[m2] = r.getrandbits(32)
    # mode "a" with a degenerate hi (equal) would make mark1 not greater: then the walk starts at mark1; keep the chain valid for both
    if m1 is not None and m2 is not None and m1 != m2:
        if m1 < m2:
            if nums[m1] > nums[m2]:
                nums[m1], nums[m2] = nums[m2], nums[m1]
        elif nums[m1] <= nums[m2]:
            if nums[m2] < 0xFFFFFFFF:
                nums[m1] = nums[m2] + 1
            else:
                nums[m1], nums[m2] = 0xFFFFFFFF, 5
    c["nums"], c["m1"], c["m2"] = nums, m1, m2
    c["queues"] = {"ITC": [r.choice([0, 1])], "PP": [r.choice([0, 1, 7, 0xFFFFFFFF])]}
    return c


def abort_case(r):
    c = base(r, "abort")
    if r.random() < 0.85:
        c["flags"] |= 0x100
    else:
        c["flags"] &= ~0x100
    return c


def abort_edges():
    r = random.Random(5967)
    for _ in range(40):
        c = abort_case(r)
        yield c
    for flagbit in (0, 0x100):
        for mode in range(5):
            for rec in (0, 3):
                for dd, og in ((0, 0), (1, 1), (1, 2), (0, 1), (2, 2)):
                    c = abort_case(r)
                    c["flags"] = (c["flags"] & ~0x100) | flagbit
                    c["rec"] = rec
                    c["ddata"], c["orig"] = dd, og
                    c["cflags"] = [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF]
                    if mode == 0:
                        c["m1"] = c["m2"] = None
                    elif mode == 1:
                        c["m1"] = c["m2"] = 2
                    elif mode == 2:
                        c["m2"], c["m1"] = 0, 3
                        c["nums"] = [1, 5, 6, 0x80000000]
                    elif mode == 3:
                        c["m1"], c["m2"] = 0, 3
                        c["nums"] = [0x80000000, 1, 2, 0x80000000]
                    else:
                        c["m1"], c["m2"] = 1, 3
                        c["nums"] = [0, 3, 2, 4]
                    yield c


def abort_random(r):
    return abort_case(r)


def single_case(r):
    c = base(r, "single")
    c["hflags"] = r.choice([0, 2, 3, 1, 0xFFFFFFFD, 0xFFFFFFFF, w(r)])
    return c


def single_edges():
    r = random.Random(11)
    for hdr in (False, True):
        for hf in (0, 1, 2, 3):
            c = single_case(r)
            c["hdr"], c["hflags"] = hdr, hf
            yield c


def place_case(r):
    return base(r, "place")


def place_edges():
    r = random.Random(12)
    for pylon in (False, True):
        for hasslot in (False, True):
            c = place_case(r)
            c["pylon"], c["hasslot"] = pylon, hasslot
            yield c


def usable_case(r, op="usable"):
    return base(r, op)


def mk_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for obj in (False, True):
            for find in (0, 1, 2):
                for e140 in (0, 1):
                    for itc in (0, 1):
                        for b3c in (0, 1, 2, 3):
                            c = base(r, op)
                            c["obj"], c["find"], c["e140"], c["a140"] = obj, find, e140, e140
                            c["eb3c"] = c["ab3c"] = b3c
                            c["queues"]["ITC"] = [itc]
                            yield c

    return gen


def dec_edges():
    r = random.Random(13)
    for edit in (False, True):
        for has_type in (False, True):
            c = base(r, "dec")
            c["edit"], c["has_type"] = edit, has_type
            yield c
    for bits in SPECIAL:
        c = base(r, "dec")
        c["edit"] = c["has_type"] = True
        c["tf1"], c["tf2"] = bits, SPECIAL[(SPECIAL.index(bits) + 3) % len(SPECIAL)]
        yield c


TESTS = [
    FnTest("AbortMovePylon", ppc, host, edge=abort_edges, rnd=abort_random),
    FnTest("SingleHFieldTextureChange", ppc, host, edge=single_edges, rnd=single_case),
    FnTest("PlacePylon", ppc, host, edge=place_edges, rnd=place_case),
    FnTest("IsCoasterUseable", ppc, host, edge=mk_edges("usable", 21), rnd=lambda r: base(r, "usable")),
    FnTest("IsTrackValid", ppc, host, edge=mk_edges("valid", 22), rnd=lambda r: base(r, "valid")),
    FnTest("IsTrackClosed", ppc, host, edge=mk_edges("closed", 23), rnd=lambda r: base(r, "closed")),
    FnTest("DecCoasterConst2", ppc, host, edge=dec_edges, rnd=lambda r: base(r, "dec")),
]
