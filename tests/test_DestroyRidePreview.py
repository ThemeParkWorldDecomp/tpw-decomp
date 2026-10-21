"""Unit v5977, the ride preview's destroy and tick and the ride highlight, against libtpw_unit_DestroyRidePreview.dylib
(tests/unit_DestroyRidePreview.cpp), main app ("SimTheme Park"), src/game/rides/RidePreview.cpp:
  DestroyRidePreview 0x1006ccd0, HighlightRide 0x1006ce28, TickRidePreview 0x1006c970

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers where the host structs have them) for the translation.
The callees outside the unit are fakes on both sides: DestroyRideInstance (records its argument), UnhighlightRide (records), CClock::Read
(records, answers from a queue), YRotateLMatrix and XRotateLMatrix (record both floats and write two words of the matrix).

The world: a ride anim set (flags, mesh, header, nodes of 0xa0 bytes, face sets, point blocks), the preview record, the globals of the
ride tables, the highlight globals, the sine table of gTPGlobals. What is compared: the preview record, the face set texture words, the
nodes' flags and point pointers (as block + offset), the colour buffers, the globals and the event log.

Fused operations and fctiwz: the tick runs under HardwareRounding. Not tested: a NULL face colour buffer in HighlightRide (the host cannot
hold the small pointer values the Mac then stores in the nodes), a NULL point block with a non-zero count (the original copies from a tiny
address), angles whose scaled value passes 2^31 (Unicorn's fctiwz saturates differently).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_DestroyRidePreview"
MASK = 0xFFFFFFFF

FN_D, FN_H, FN_T = 0x1006CCD0, 0x1006CE28, 0x1006C970
SIZES = {FN_D: 284, FN_H: 344, FN_T: 808}
OWN = set(SIZES)

CALLEE_DESTROY = 0x1006A3EC
CALLEE_UNHL = 0x1006D0F8
CALLEE_CLOCK = 0x101472F4
CALLEE_YROT = 0x100C1458
CALLEE_XROT = 0x100C1308

ADDR = dict(rideSets=0x102C79A4, rideTypes=0x102D19B0, hl=0x102C6CE8, pot=0x102D1F08, dis=0x102D1F04, nodeCol=0x102D1F0C,
            faceCol=0x102D1F10, tpg=0x103B74C8)

POOL = [0x11, 0x22, 0x33, 0x44]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_destroy(e):
    e.cl.rec("DestroyRideInstance", (e.r(3) & MASK,))


def stub_unhl(e):
    e.cl.rec("UnhighlightRide", ())


def stub_clock(e):
    e.cl.rec("ClockRead", ())
    e.set_r(3, e.cl.ret("ClockRead") & MASK)


def make_rot(name, w0, w1):
    def stub(e):
        c, s = e.f(1), e.f(2)
        e.cl.rec(name, (canon_f(fb(c)), canon_f(fb(s))))
        m = e.r(3)
        e.write(m + 4 * w0, struct.pack(">f", c))
        e.write(m + 4 * w1, struct.pack(">f", s))
    return stub


STUBS.internal(CALLEE_DESTROY, stub_destroy)
STUBS.internal(CALLEE_UNHL, stub_unhl)
STUBS.internal(CALLEE_CLOCK, stub_clock)
STUBS.internal(CALLEE_YROT, make_rot("YRotateLMatrix", 0, 6))
STUBS.internal(CALLEE_XROT, make_rot("XRotateLMatrix", 10, 8))


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

    def u16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def new(self, name, size):
        a = self.alloc(size + 16)
        self.names.append((name, a, size))
        return a

    def norm(self, v):
        """An absolute address as (block name, offset)."""
        if v == 0:
            return 0
        for name, a, size in self.names:
            if a <= v <= a + size:
                return (name, v - a)
        return ("?", v)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    O = dict(flags=4, heads=8, fd0=0xD0, mhdr=4)

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.names = []

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

    def p32(self, a):
        return a

    def abs32(self, v):
        return v

    def call(self, fn, args):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, args)


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.O
        self.base = lib.base
        self.names = []
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def p32(self, a):
        return a - self.lib.tpw_dr_base() if a else 0

    def abs32(self, v):
        return v + self.lib.tpw_dr_base() if v else 0

    def call(self, fn, args):
        f = {FN_D: self.lib.tpw_dr_destroy, FN_H: self.lib.tpw_dr_highlight, FN_T: self.lib.tpw_dr_tick}[fn]
        f(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_dr_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dr_destroy.argtypes = [C.c_uint64]
    lib.tpw_dr_tick.argtypes = [C.c_uint64]
    lib.tpw_dr_highlight.argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_dr_layout(out)
    v = list(out)[:n]
    assert v[6] == 0x88 and v[7] == 0xA0 and v[8:] == [4, 8, 12], v
    assert v[4] <= 0x200 and v[5] <= 0x80, v
    lib.O = dict(flags=v[0], heads=v[1], fd0=v[2], mhdr=v[3])
    lib.base = lib.tpw_dr_base()
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_ride(s, rd, tag):
    """A ride anim set with its mesh, header, nodes, face sets and point blocks. Returns the addresses."""
    nodes = rd["nodes"]
    k = len(nodes)
    R = s.new(tag + "R", 0x200)
    M = s.new(tag + "M", 0x80)
    H = s.new(tag + "H", 0x100)
    N = s.new(tag + "N", max(k, 1) * 0xA0)
    s.words(R + s.O["flags"], [rd["rflags"]])
    s.ptr(R + s.O["heads"], M)
    s.ptr(M + s.O["mhdr"], H)
    s.words(H + 0x30, [rd["hflags"]])
    s.u16(H + 0x44, k)
    s.words(H + 0x70, [s.p32(N)])
    root = 0
    if rd.get("root") is not None:
        root = s.new(tag + "root", 0x100)
        s.words(root, rd["root"])
        s.words(H + 0x78, [s.p32(root)])
    rr = dict(R=R, H=H, N=N, root=root, fs=[], pts=[], rd=rd)
    for i, nd in enumerate(nodes):
        a = N + 0xA0 * i
        s.words(a, [nd["flags"]])
        s.u16(a + 0x5A, nd["nfs"])
        s.u16(a + 0x5C, nd["f5c"])
        s.u16(a + 0x5E, nd["f5e"])
        fs = s.new("%sfs%d" % (tag, i), max(nd["nfs"], 1) * 0x10)
        s.words(fs, nd["fs"])
        s.words(a + 0x6C, [s.p32(fs)])
        rr["fs"].append((fs, nd["nfs"]))
        if nd["nopts"]:
            rr["pts"].append(None)
        else:
            pts = s.new("%spts%d" % (tag, i), (nd["f5e"] + nd["f5c"]) * 12)
            s.words(pts, nd["pts"])
            s.words(a + 0x64, [s.p32(pts)])
            rr["pts"].append((pts, len(nd["pts"])))
    return rr


def build_tables(s, case):
    rs, rt = s.g("rideSets"), s.g("rideTypes")
    for i in range(8):
        s.ptr(rs + s.psz * i, 0)
        s.ptr(rt + s.psz * i, 0)


def build_info(s, case):
    info = s.new("info", 0x88)
    s.words(info, case["info"])
    return info


def build(s, case):
    op = case["op"]
    build_tables(s, case)
    s.rides = []
    s.info = 0
    if op == "destroy":
        if case.get("null"):
            return
        s.info = build_info(s, case)
        rr = build_ride(s, case["ride"], "a")
        s.rides.append(rr)
        s.ptr(s.g("rideSets") + s.psz * case["info"][1], rr["R"])
        T_ = s.new("type", 0x200)
        if case["fd0"]:
            s.ptr(T_ + s.O["fd0"], rr["R"])
        s.ptr(s.g("rideTypes") + s.psz * case["info"][33], T_)
    elif op == "tick":
        s.info = build_info(s, case)
        rr = build_ride(s, case["ride"], "a")
        s.rides.append(rr)
        s.ptr(s.g("rideSets") + s.psz * case["info"][1], rr["R"])
        tpg = s.g("tpg")
        s.words(tpg + 4, [case["mask"], case["scale"]])
        s.words(tpg + 12, case["table"])
    else:
        r1 = build_ride(s, case["ride1"], "a")
        r2 = build_ride(s, case["ride2"], "b")
        s.rides = [r1, r2]
        pick = [0, r1["R"], r2["R"]]
        s.ptr(s.g("hl"), pick[case["hl"]])
        s.ptr(s.g("pot"), pick[case["pot"]])
        s.words(s.g("dis"), [case["dis"]])
        s.cbuf = s.new("faceBuf", len(case["face"]) * 4)
        s.words(s.cbuf, case["face"])
        s.nbuf = s.new("nodeBuf", len(case["nodecol"]) * 4)
        s.words(s.nbuf, case["nodecol"])
        s.ptr(s.g("faceCol"), s.cbuf)
        s.ptr(s.g("nodeCol"), s.nbuf)


def execute(s, case):
    op = case["op"]
    if op == "destroy":
        s.call(FN_D, [s.info])
    elif op == "tick":
        s.call(FN_T, [s.info])
    else:
        s.call(FN_H, [])


def observe(s, case):
    out = {"log": s.log()}
    op = case["op"]
    if op == "destroy":
        if case.get("null"):
            return out
        out["info"] = s.rd_words(s.info, 34)
    if op == "tick":
        out["info"] = [canon_f(w) for w in s.rd_words(s.info, 34)]
        out["root"] = [canon_f(w) for w in s.rd_words(s.rides[0]["root"], 64)]
        return out
    for i, rr in enumerate(s.rides):
        out["fs%d" % i] = [s.rd_words(a, 4 * max(n, 1)) for a, n in rr["fs"]]
        out["rflags%d" % i] = s.rd_word(rr["R"] + s.O["flags"])
        out["hflags%d" % i] = s.rd_word(rr["H"] + 0x30)
        out["nodes%d" % i] = [(s.rd_word(rr["N"] + 0xA0 * j), s.norm(s.abs32(s.rd_word(rr["N"] + 0xA0 * j + 0x64))))
                              for j in range(len(rr["rd"]["nodes"]))]
        out["pts%d" % i] = [None if p is None else s.rd_words(p[0], p[1]) for p in rr["pts"]]
    if op == "highlight":
        out["hl"] = s.norm(s.rd_ptr(s.g("hl")))
        out["pot"] = s.norm(s.rd_ptr(s.g("pot")))
        out["face"] = s.rd_words(s.cbuf, len(case["face"]))
        out["nodecol"] = [s.norm(s.abs32(w)) for w in s.rd_words(s.nbuf, len(case["nodecol"]))]
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def w32(r):
    return r.getrandbits(32)


def rf(r):
    k = r.random()
    if k < 0.06:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000])
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rand_node(r, hl=False):
    nfs = r.randrange(0, 4)
    f5c, f5e = r.randrange(0, 4), r.randrange(0, 4)
    flags = w32(r) & ~0x4000
    if r.random() < 0.4:
        flags |= 0x4000
    fs = []
    for _ in range(max(nfs, 1)):
        fs += [r.choice(POOL + [0, w32(r)])] + [w32(r) for _ in range(3)]
    return dict(flags=flags, nfs=nfs, f5c=f5c, f5e=f5e, fs=fs, nopts=(f5c == 0 and r.random() < 0.3),
                pts=[w32(r) for _ in range((f5e + f5c) * 3)])


def rand_ride(r, n=None, root=False):
    n = r.randrange(0, 4) if n is None else n
    rd = dict(rflags=w32(r), hflags=w32(r), nodes=[rand_node(r) for _ in range(n)])
    rd["rflags"] &= ~0x20000
    if root:
        rd["root"] = [rf(r) for _ in range(64)]
    return rd


def tex(r):
    return [r.choice(POOL + [0]) for _ in range(4)]


def destroy_case(r, null=False, typ=None, fd0=None, n=None):
    c = {"op": "destroy", "queues": {}}
    if null:
        c["null"] = True
        return c
    info = [w32(r) for _ in range(34)]
    info[1] = r.randrange(1, 4)
    info[29:33] = tex(r)
    info[33] = r.choice([0, 1, 2, 3]) if typ is None else typ
    c["info"] = info
    c["fd0"] = r.choice([0, 1, 1]) if fd0 is None else fd0
    c["ride"] = rand_ride(r, n)
    return c


def highlight_case(r, hl=None, pot=None, dis=None, p20000=None):
    c = {"op": "highlight", "queues": {}}
    c["hl"] = r.choice([0, 1, 2, 1]) if hl is None else hl
    c["pot"] = r.choice([0, 1, 2, 1]) if pot is None else pot
    c["dis"] = r.choice([0, 0, 0, 1, -1 & MASK]) if dis is None else dis
    c["ride1"] = rand_ride(r)
    c["ride2"] = rand_ride(r)
    if p20000 is not None:
        key = "ride1" if c["pot"] == 1 else "ride2"
        c[key]["rflags"] = (c[key]["rflags"] & ~0x20000) | (0x20000 if p20000 else 0)
    elif r.random() < 0.15:
        c["ride1"]["rflags"] |= 0x20000
    total = 0
    for key in ("ride1", "ride2"):
        t = sum((nd["f5e"] + nd["f5c"]) * 12 for nd in c[key]["nodes"] if not nd["flags"] & 0x4000)
        total = max(total, t)
    c["face"] = [w32(r) for _ in range(total // 4 + 4)]
    c["nodecol"] = [0] * 6
    return c


def tick_case(r, active=None, mask=None, angle=None):
    c = {"op": "tick"}
    info = [rf(r) for _ in range(34)]
    info[0] = r.choice([0, 1]) if active is None else active
    info[1] = r.randrange(1, 4)
    info[28] = w32(r)
    k = r.choice([0, 3, 7, 15, 63, 255, 1023, 4095, 4095])
    mask = k if mask is None else mask
    if angle is None:
        angle = fb(r.choice([0.0, -0.0, 1.0, -1.0, r.uniform(-10, 10), r.uniform(-10, 10), r.uniform(-1e-3, 1e-3), 6.2831855]))
    info[18] = angle
    c["info"] = info
    c["mask"] = mask
    c["scale"] = fb(r.choice([651.8986, r.uniform(0.5, 800), 1.0, 10.0]))
    c["table"] = [rf(r) for _ in range(min(mask, 4095) + 1)]
    c["ride"] = rand_ride(r, 0, root=True)
    a = w32(r)
    delta = r.choice([0, 1, 16, 1000, 0x7FFFFFFF, 0x80000000, MASK, w32(r), 100000])
    if info[0]:
        c["queues"] = {"ClockRead": [a, (a + delta) & MASK]}
    else:
        c["queues"] = {"ClockRead": [(info[28] + delta) & MASK]}
    return c


def d_edges():
    r = random.Random(5977)
    yield destroy_case(r, null=True)
    for typ in (0, 1, 3):
        for fd0 in (0, 1):
            for n in (0, 1, 3):
                yield destroy_case(r, typ=typ, fd0=fd0, n=n)
    for _ in range(12):
        yield destroy_case(r, typ=2, fd0=1, n=3)


def d_random(r):
    return destroy_case(r)


def h_edges():
    r = random.Random(5978)
    for hl in (0, 1, 2):
        for pot in (0, 1, 2):
            for dis in (0, 1, MASK):
                for p in (False, True):
                    yield highlight_case(r, hl, pot, dis, p)
    for _ in range(10):
        yield highlight_case(r, 1, 1, 0, False)


def h_random(r):
    return highlight_case(r)


def t_edges():
    r = random.Random(5979)
    for active in (0, 1):
        for mask in (0, 15, 4095):
            for angle in (0, 0x80000000, fb(1.0), fb(-1.0), fb(6.2831855), fb(100.0)):
                yield tick_case(r, active, mask, angle)


def t_random(r):
    return tick_case(r)


TESTS = [
    FnTest("DestroyRidePreview", ppc, host, edge=d_edges, rnd=d_random),
    FnTest("HighlightRide", ppc, host, edge=h_edges, rnd=h_random),
    FnTest("TickRidePreview", ppc, host, edge=t_edges, rnd=t_random),
]
