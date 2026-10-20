"""Unit v5190, the editor's pylon placement, against libtpw_unit_SetPotentialPylonArea.dylib (tests/unit_SetPotentialPylonArea.cpp), main app
("SimTheme Park"), src/TPCoaster_trains_SetPotentialPylonArea.cpp:
  SetPotentialPylonArea 0x10041a58, PlacePylon 0x10041c5c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4-byte pointers) for the original, and in the host library's memory (little-endian, 8-byte pointers, the host's field offsets from
tpw_spa_layout) for the translation. The callees outside the unit (NewPylon, GetNewBraceLoftInterval, CTRACE, DetermineLinkLevel,
NewTrackBrace, SetupBraceAnimFrameData, RecalcTrackParams, CalculateBraceCost, FloatToCell, GetBankAccount, GetBalance, MakeWithdrawal,
RejoinTrack, SetCoasterEditState, CoasterPanelEndTrack, UIHandleEndLoop, UIHandleMidLoop) are fakes on both sides that record every argument
(objects as labels) and answer from the case's queues. SetPotentialPylonArea runs for real inside PlacePylon on both sides; sqrt is the C one.

The world of a case: the edit instance, its type's two pylon controls, a chain of braces with cells, the dragged brace and its neighbours, up to
three pylons in the instance lists, the height field, the input flags, the loop counter, the recording flag, the answer queues. What is compared:
the answer, the four area words, the input flags, the loop counter, the instance's flags, both out parameters, the event log.

No fused operations (single-precision adds and one fctiwz of a small value). Not tested: SetPotentialPylonArea with no brace to find (the
original dereferences NULL); a pylon with no braces (the original reads its count word as a brace); with flag != 0 the original reads two
uninitialised stack words, so the test clears the stack under the call and the translation uses zeros.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from unicorn import UC_HOOK_CODE
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetPotentialPylonArea"
MASK = 0xFFFFFFFF

FN_SPPA, FN_PLACE = 0x10041A58, 0x10041C5C
SIZES = {FN_SPPA: 472, FN_PLACE: 988}
OWN = set(SIZES)

REJOIN, EDITSTATE, PANELEND, NEWPYLON, LOFT = 0x10053EAC, 0x10053DE8, 0x1019C158, 0x1003D610, 0x1003E108
CTRACE, LINKLEVEL, NEWBRACE, SETUPANIM, RECALC = 0x1006FCBC, 0x1003E7DC, 0x10040304, 0x1003BB44, 0x10050F88
BRACECOST, GETACCT, GETBAL, WITHDRAW, FLOATTOCELL = 0x10058924, 0x1013EDFC, 0x100F32B4, 0x100F33B0, 0x10237CD8
ENDLOOP, MIDLOOP = 0x100565CC, 0x10056548

ADDR = dict(w6890=0x102B6890, w6894=0x102B6894, w6898=0x102B6898, w689c=0x102B689C, edit=0x102B68B0, insts=0x102B68B4,
            pmove=0x102C177C, w1790=0x102C1790, input=0x102C1794, hh=0x102C6C64, hw=0x102C6C68, hys=0x102C6C6C, hxs=0x102C6C70,
            hdr=0x102C6C7C, world=0x1037EBFC, rec=0x1037EC44)

# The Mac's offsets of every field written by hand (the host's come from tpw_spa_layout, same order)
MAC = {
    "inst": dict(flags=0, type=4, next=0x18, pylons=0x4C, blk3c=0x3C, bcount=0x44, braces=0x58, b5c=0x5C, prev60=0x60, b64=0x64),
    "brace": dict(number=0, flags=8, prev=0x64, next=0x68, data=0x100),
    "bdata": dict(cellx=0xC, cellz=0x10),
    "pyl": dict(flags=4, index=8, control=0x18, x=0x1C, z=0x24, next=0x28, count=0x30, braces=0x34),
    "ctype": dict(A=0xA8, B=0x1C8),
    "ctrl": dict(fa0=0xA0),
    "hdr": dict(hfield=0x6C),
    "hf": dict(f18=0x18),
    "rec": dict(recording=0x24),
}
LNAMES = [(st, f) for st in MAC for f in MAC[st]]
HOST = {st: {} for st in MAC}

BR = ["mb", "mbn", "mbp", "pv", "l0", "l1", "l2", "bk"]
PY = ["pa", "pb", "pc", "np"]
CUR = [None]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ptr_of(e, a):
    return e.cl.lab(a)


def st_rejoin(e):
    e.cl.rec("RejoinTrack", (e.cl.lab(e.r(3)),))


def st_state(e):
    e.cl.rec("SetCoasterEditState", (e.r(3) & MASK,))


def st_panel(e):
    e.cl.rec("CoasterPanelEndTrack", ())


def st_newpylon(e):
    L = e.cl
    L.rec("NewPylon", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, s32(e.r(6)), L.lab(e.r(7)), L.lab(e.r(8))))
    e.set_r(3, CUR[0].N["np"])


def st_loft(e):
    L = e.cl
    L.rec("GetNewBraceLoftInterval", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), s32(e.r(7)), s32(e.r(8))))
    e.write(e.r(9), struct.pack(">I", L.ret("loft") & MASK))


def st_ctrace(e):
    s = e.cstr(e.r(4))
    if isinstance(s, str):
        s = s.encode("latin1")
    e.cl.rec("CTRACE", (e.r(3) & MASK, len(s), sum(s)))


def st_link(e):
    L = e.cl
    L.rec("DetermineLinkLevel", (canon_f(fb(e.f(1))), L.lab(e.r(4)), s32(e.r(7))))
    e.write(e.r(5), struct.pack(">I", L.ret("level") & MASK))
    e.write(e.r(6), struct.pack(">I", L.ret("dlout") & MASK))
    e.set_r(3, 1)


def st_newbrace(e):
    L = e.cl
    extra = e.u32(e.r(1) + 0x38)
    L.rec("NewTrackBrace", (L.lab(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), e.r(7) & MASK, L.lab(e.r(8)),
                            e.r(9) & MASK, e.r(10) & MASK, extra))
    e.set_r(3, CUR[0].N["nb"])


def st_setup(e):
    e.cl.rec("SetupBraceAnimFrameData", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_recalc(e):
    e.cl.rec("RecalcTrackParams", (e.cl.lab(e.r(3)),))


def st_cost(e):
    L = e.cl
    L.rec("CalculateBraceCost", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), canon_f(fb(e.f32(e.r(6)))), canon_f(fb(e.f32(e.r(7)))),
                                 L.lab(e.r(8)), L.lab(e.r(9))))
    e.set_f1(bitsf(L.ret("cost")))


def st_acct(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR[0].N["acct"])


def st_bal(e):
    e.cl.rec("GetBalance", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("balance") & MASK)


def st_wd(e):
    e.cl.rec("MakeWithdrawal", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def st_f2c(e):
    bits = T.f64_bits(e.f(1))
    e.cl.rec("FloatToCell", ((bits >> 32) & MASK, bits & MASK))
    e.set_r(3, e.cl.ret("cell") & MASK)


def st_endloop(e):
    e.cl.rec("UIHandleEndLoop", ())


def st_midloop(e):
    e.cl.rec("UIHandleMidLoop", ())


for _a, _f in ((REJOIN, st_rejoin), (EDITSTATE, st_state), (PANELEND, st_panel), (NEWPYLON, st_newpylon), (LOFT, st_loft),
               (CTRACE, st_ctrace), (LINKLEVEL, st_link), (NEWBRACE, st_newbrace), (SETUPANIM, st_setup), (RECALC, st_recalc),
               (BRACECOST, st_cost), (GETACCT, st_acct), (GETBAL, st_bal), (WITHDRAW, st_wd), (FLOATTOCELL, st_f2c),
               (ENDLOOP, st_endloop), (MIDLOOP, st_midloop)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    emu = runner.emu

    def clear_frame(uc, address, size, user):
        # PlacePylon's frame (0xa0 bytes under r1): its two uninitialised locals (flag != 0) read as zero, like the translation's
        emu.write(emu.r(1) - 0xA0, bytes(0xA0))

    emu.uc.hook_add(UC_HOOK_CODE, clear_frame, begin=FN_PLACE, end=FN_PLACE)


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
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def o(self, st, f):
        return self.OFF[st][f]

    def g(self, name):
        return self.gaddr(name)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    OFF = MAC

    def __init__(self, e, case):
        self.e = e
        self.saved = None

    def gaddr(self, name):
        return ADDR[name]

    def p32(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))      # zeroed like the host's blocks

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def start(self, case, N):
        self.N = N
        CUR[0] = self
        labels = {a: i + 1 for i, (n, a) in enumerate(sorted(N.items()))}
        self.e.cl = self.L = CL.PpcLog(labels, case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def log(self):
        return self.L.log

    def call_sppa(self):
        T.call_ppc(self.e, FN_SPPA, [])
        return 0

    def call_place(self, c, outp, outb):
        e = self.e
        r3, _, _ = T.call_ppc(e, FN_PLACE, [c["x"], c["z"], c["flag"] & MASK, outp, outb, c["level"], c["extra"]])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    OFF = HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)

    def gaddr(self, name):
        return self.lib.tpw_spa_addr(name.encode())

    def p32(self, a):
        return self.lib.tpw_spa_off(a)

    def alloc(self, n):
        return self.lib.tpw_spa_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def start(self, case, N):
        self.N = N
        labels = {i + 1: a for i, (n, a) in enumerate(sorted(N.items()))}
        CL.host_begin(self.lib, labels, case["queues"])
        self.lib.tpw_spa_setres(0, N["np"])
        self.lib.tpw_spa_setres(1, N["nb"])
        self.lib.tpw_spa_setres(2, N["acct"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def call_sppa(self):
        self.lib.tpw_spa_sppa()
        return 0

    def call_place(self, c, outp, outb):
        return self.lib.tpw_spa_place(c["x"], c["z"], s32(c["flag"]), outp, outb, c["level"], c["extra"]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_spa_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "setres").restype, getattr(lib, p + "setres").argtypes = None, [C.c_int32, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "sppa").restype, getattr(lib, p + "sppa").argtypes = None, []
    getattr(lib, p + "place").restype = C.c_uint32
    getattr(lib, p + "place").argtypes = [C.c_uint32, C.c_uint32, C.c_int32, C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 64)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(LNAMES), n
    for (st, f), v in zip(LNAMES, list(out)[:n]):
        HOST[st][f] = v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    N = {}
    for n in ["inst", "oth", "ty", "hdr", "hf", "world", "acct", "opy", "obr", "nb"] + BR + PY + ["d_" + b for b in BR]:
        N[n] = s.alloc(0x800 if n in ("inst", "oth", "ty") else 0x400)
    o, g = s.o, s.g
    N["ctlA"] = N["ty"] + o("ctype", "A")
    N["ctlB"] = N["ty"] + o("ctype", "B")

    def w(base, st, f, v):
        s.words(base + o(st, f), [v])

    def p(base, st, f, v):
        s.ptr(base + o(st, f), v)

    for k in ("hxs", "hys", "hw", "hh", "input", "w1790"):
        s.words(g(k), [c[k]])
    for k, v in zip(("w689c", "w6898", "w6894", "w6890"), c["junk"]):
        s.words(g(k), [v])
    s.ptr(g("edit"), N["inst"])
    s.ptr(g("pmove"), N["mb"])
    s.ptr(g("hdr"), N["hdr"])
    s.ptr(g("world"), N["world"])
    s.words(g("rec") + o("rec", "recording"), [c["recording"]])
    s.words(N["hdr"] + o("hdr", "hfield"), [s.p32(N["hf"])])
    w(N["hf"], "hf", "f18", c["stride"])

    # the braces
    for b in BR:
        d = c["br"][b]
        w(N[b], "brace", "flags", d["flags"])
        w(N[b], "brace", "number", d["num"])
        p(N[b], "brace", "data", N["d_" + b])
        w(N["d_" + b], "bdata", "cellx", d["cx"])
        w(N["d_" + b], "bdata", "cellz", d["cz"])
    p(N["mb"], "brace", "next", N["mbn"])
    p(N["mb"], "brace", "prev", N["mbp"])
    p(N["l0"], "brace", "next", N["l1"])
    p(N["l1"], "brace", "next", N["l2"])

    # the instance and its type
    w(N["inst"], "inst", "flags", c["iflags0"])
    w(N["inst"], "inst", "blk3c", c["blk"])
    w(N["inst"], "inst", "bcount", c["bcount"])
    p(N["inst"], "inst", "type", N["ty"])
    p(N["inst"], "inst", "braces", N["l0"])
    p(N["inst"], "inst", "prev60", N["pv"])
    if c["b64"]:
        p(N["inst"], "inst", "b64", N[c["b64"]])
    if c["b5c"]:
        p(N["inst"], "inst", "b5c", N[c["b5c"]])
    s.words(N["ctlA"] + o("ctrl", "fa0"), [c["fa0"][0]])
    s.words(N["ctlB"] + o("ctrl", "fa0"), [c["fa0"][1]])

    # the pylons
    target = (c["x"] + c["z"] * c["stride"]) & MASK
    for n in PY:
        d = c["py"][n]
        w(N[n], "pyl", "flags", d["flags"])
        w(N[n], "pyl", "index", (target + d["delta"]) & MASK)
        w(N[n], "pyl", "count", len(d["braces"]))
        w(N[n], "pyl", "x", d["x"])
        w(N[n], "pyl", "z", d["z"])
        for i, bn in enumerate(d["braces"]):
            s.ptr(N[n] + o("pyl", "braces") + i * s.psz, N[bn])

    def chain(names):
        for a, b in zip(names, names[1:]):
            p(N[a], "pyl", "next", N[b])

    if c["ilist"]:
        p(N["inst"], "inst", "pylons", N[c["ilist"][0]])
        chain(c["ilist"])
    if c["olist"]:
        p(N["oth"], "inst", "pylons", N[c["olist"][0]])
        chain(c["olist"])
    mode = c["insts"]
    s.ptr(g("insts"), 0)      # the host's globals keep their values from the case before
    if mode == "inst":
        s.ptr(g("insts"), N["inst"])
    elif mode == "oth_inst":
        s.ptr(g("insts"), N["oth"])
        p(N["oth"], "inst", "next", N["inst"])
    elif mode == "inst_oth":
        s.ptr(g("insts"), N["inst"])
        p(N["inst"], "inst", "next", N["oth"])
    elif mode == "oth":
        s.ptr(g("insts"), N["oth"])
    return N


def execute(s, c, N):
    if c["op"] == "sppa":
        return s.call_sppa()
    outp = N["opy"] if c["outp"] else 0
    outb = N["obr"] if c["outb"] else 0
    return s.call_place(c, outp, outb)


def observe(s, c, N, answer):
    rev = {v: k for k, v in N.items()}

    def nm(a):
        return rev.get(a, a)

    out = {"answer": answer, "log": s.log(),
           "area": [s.rd_word(s.g(k)) for k in ("w689c", "w6898", "w6894", "w6890")],
           "input": s.rd_word(s.g("input")), "w1790": s.rd_word(s.g("w1790")),
           "iflags": s.rd_word(N["inst"] + s.o("inst", "flags"))}
    if c["op"] == "place":
        out["opy"] = nm(s.rd_ptr(N["opy"]))
        out["obr"] = nm(s.rd_ptr(N["obr"]))
    return out


def run(s, c):
    try:
        N = build(s, c)
        s.start(c, N)
        return observe(s, c, N, execute(s, c, N))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rcase(r, op, **ov):
    c = {"op": op}
    noise = (r.getrandbits(32) & ~0x140) if r.random() < 0.3 else 0
    c["input"] = r.choice([0, 0x40, 0x100, 0x140]) | noise
    c["w1790"] = r.choice([0, 1, 2, 3])
    c["hxs"], c["hys"] = r.randint(-8, 20), r.randint(-8, 20)
    c["hw"], c["hh"] = r.randint(-2, 40), r.randint(-2, 40)
    c["stride"] = r.randint(1, 80)
    c["blk"] = r.choice([0, 2, 1, 3, 0x8002, 0x8000])
    c["bcount"] = r.randint(0, 4)
    c["iflags0"] = r.choice([0, 1, 0x1000, r.getrandbits(32)])
    c["fa0"] = [r.choice([0, 1, 2, 3, 4, 8, 9, 15, 16, 17, 24, 25, 99, 100, 255, 256, r.randrange(10000), r.getrandbits(32), 0xFFFFFFFF])
                for _ in range(2)]
    c["junk"] = [r.getrandbits(32) for _ in range(4)]
    br = {}
    for b in BR:
        br[b] = dict(flags=r.choice([0, 0, 0x2000, 0x10000, 0x12000, r.getrandbits(32)]), num=r.randint(0, 3),
                     cx=r.randint(-6, 45) & MASK, cz=r.randint(-6, 45) & MASK)
    br["l2"]["flags"] &= ~0x2000
    c["br"] = br
    c["x"], c["z"] = r.randint(0, 40), r.randint(0, 40)
    c["flag"] = r.choice([0, 0, 0, 1, 2])
    c["level"] = r.choice([0, 5, 0xFF, 0xFF, 0x7F, r.randrange(256)])
    c["extra"] = r.choice([0, r.getrandbits(32)])
    c["recording"] = r.choice([0, 0, 1])
    c["outp"], c["outb"] = r.random() < 0.8, r.random() < 0.8
    c["insts"] = r.choice(["none", "inst", "oth_inst", "inst_oth", "inst", "oth_inst", "oth"])
    c["ilist"] = r.choice([[], ["pa"], ["pa", "pb"]])
    c["olist"] = r.choice([[], ["pc"]])
    c["b64"] = r.choice([None, "bk", "l0", "l1", "pv"])
    c["b5c"] = r.choice([None, "l1", "pv"])
    py = {}
    for n in PY:
        py[n] = dict(flags=r.choice([0, 0, 0, 1, 2, 4, 5, 8]), delta=r.choice([0, 0, r.randint(1, 5), -3]),
                     braces=[r.choice(["l0", "l1", "l2", "pv", "bk", "mb"]) for _ in range(r.randint(1, 5))],
                     x=fb(r.uniform(-100, 100)) if r.random() < 0.9 else r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000]),
                     z=fb(r.uniform(-100, 100)))
    c["py"] = py
    c["queues"] = {"loft": [fb(r.uniform(-50, 50))], "level": [r.choice([0, 1, 5, 0x1FF, 0x12345678, 0xFF])],
                   "dlout": [fb(r.uniform(-50, 50))], "cost": [fb(r.choice([0.0, 0.5, 2.5, r.uniform(-1e5, 1e5)]))],
                   "balance": [r.randint(-1000, 1000) & MASK], "cell": [r.getrandbits(32)]}
    for k, v in ov.items():
        if k in ("py", "br"):
            for n, d in v.items():
                c[k][n].update(d)
        else:
            c[k] = v
    return c


def sppa_edges():
    r = random.Random(1901)
    for blk in (0, 2):
        for inp in (0, 0x100, 0x40, 0x140):
            for mbnf in (0, 0x10000):
                for f0 in (0, 0x2000):
                    for f1 in (0, 0x2000):
                        yield rcase(r, "sppa", blk=blk, input=inp, br={"mbn": dict(flags=mbnf), "l0": dict(flags=f0), "l1": dict(flags=f1)})
    for cell in (0, 2, 5, 9, 10):
        for cz in (0, 3, 9, 10):
            for fa0 in (0, 1, 4, 9, 0xFFFFFFFF):
                yield rcase(r, "sppa", blk=0, input=0, hxs=0, hys=0, hw=10, hh=10, fa0=[fa0, 0],
                            br={"l0": dict(flags=0, cx=cell, cz=cz)})
    for inp in (0x40, 0x140):
        yield rcase(r, "sppa", blk=0, input=inp, hxs=-3, hys=7, hw=12, hh=9, fa0=[16, 9], br={"l0": dict(flags=0, cx=2, cz=14)})


def sppa_random(r):
    return rcase(r, "sppa")


def pyl(**k):
    d = dict(flags=0, delta=0, count=2, braces=["l0", "bk"], x=fb(10.0), z=fb(20.0))
    d.update(k)
    return d


def place_edges():
    r = random.Random(1902)
    for insts in ("none", "inst"):
        for flag in (0, 1):
            for inp in (0, 0x40, 0x140):
                for w in (0, 1, 2, 3):
                    for lvl in (0xFF, 5):
                        for rec in (0, 1):
                            yield rcase(r, "place", insts=insts, ilist=[], olist=[], flag=flag, input=inp, w1790=w, level=lvl, recording=rec,
                                        blk=0, outp=True, outb=True)
    # a pylon found, the instance's brace among its braces (blk bit 2)
    for oi, ob in ((True, True), (False, False)):
        for b64 in ("bk", "l2"):
            yield rcase(r, "place", insts="inst", ilist=["pa"], olist=[], blk=2, b64=b64, outp=oi, outb=ob,
                        py={"pa": pyl(braces=["l0", "l1", "bk"])})
    # a pylon found, last brace is the track's last (answer 0 for flag 0)
    for flag in (0, 1):
        for oi in (True, False):
            yield rcase(r, "place", insts="oth_inst", ilist=["pa"], olist=["pc"], blk=0, bcount=4, flag=flag, outp=oi, outb=True,
                        input=0x40, w1790=2, py={"pa": pyl(flags=0, braces=["l0", "l1"]), "pc": pyl(flags=0, delta=1)},
                        br={"l1": dict(num=3)})
    # the same but the number differs
    yield rcase(r, "place", insts="inst", ilist=["pa"], olist=[], blk=0, bcount=4, flag=0, py={"pa": pyl(braces=["l0", "l1"])}, br={"l1": dict(num=2)})
    # flags 1, 4, 5 skipped, then a later pylon / another instance matches
    for fl in (1, 4, 5, 2, 8):
        yield rcase(r, "place", insts="inst_oth", ilist=["pa", "pb"], olist=["pc"], blk=0, bcount=4, flag=0,
                    py={"pa": pyl(flags=fl, braces=["l0"]), "pb": pyl(delta=3), "pc": pyl(flags=0, braces=["l2"])}, br={"l2": dict(num=0)})
    for fl in (1, 4, 5, 0):
        yield rcase(r, "place", insts="inst", ilist=["pa", "pb"], olist=[], blk=0, bcount=2, flag=0,
                    py={"pa": pyl(delta=2), "pb": pyl(flags=fl, braces=["l1"])}, br={"l1": dict(num=1)})
    # the counter run down
    for w in (0, 1, 2, 3):
        for rec in (0, 1):
            for flag in (0, 1):
                yield rcase(r, "place", insts="none", flag=flag, input=0x40, w1790=w, recording=rec, level=0xFF)


def place_random(r):
    return rcase(r, "place")


TESTS = [
    FnTest("SetPotentialPylonArea", ppc, host, edge=sppa_edges, rnd=sppa_random),
    FnTest("PlacePylon", ppc, host, edge=place_edges, rnd=place_random),
]
