"""Unit v5915, the ride highlight and flash functions and the shape edge marker, against libtpw_unit_DecideRideShapeDeformEdges.dylib
(tests/unit_DecideRideShapeDeformEdges.cpp), main app ("SimTheme Park"), src/game/park/SkirtDeform.cpp, src/game/rides/RidePreview.cpp:
  UnhighlightRide 0x1006d0f8, StartFlashingObject 0x1006d2f8, StopFlashingObject 0x1006d430, DecideRideShapeDeformEdges 0x1006d76c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides: MReleaseAllocF (logs size and flags, answers a block the case allocated), MReleaseFreeF (logs whether it got the block the case
expects, the set's own point block), HighlightRide (logs). memset and memcpy run for real on both sides.

The world of a case: one to four ride sets (ride, mesh, header, original header, node arrays, point blocks, flags), a chain of them in the
flash list, the highlighted ride, the table of saved colours (pointers to spare blocks or 0); for DecideRideShapeDeformEdges a random
shape and a pre-filled flag table. What is compared: the log, every flag word and pointer of every set (pointers by name), the point
blocks' bytes (memcpy), the globals (flash list head, highlighted ride, skirt flags, rows, columns).

No fused operations. Not tested: a set that is not in the flash list for StopFlashingObject (the original walks off the list), shapes
wider or taller than 20 (they run past the tables), a NULL set.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DecideRideShapeDeformEdges"
MASK = 0xFFFFFFFF

F_UNHL, F_START, F_STOP, F_DECIDE = 0x1006D0F8, 0x1006D2F8, 0x1006D430, 0x1006D76C
SIZES = {F_UNHL: 176, F_START: 252, F_STOP: 400, F_DECIDE: 760}
OWN = set(SIZES)
ENTRY = {"unhl": F_UNHL, "start": F_START, "stop": F_STOP, "decide": F_DECIDE}

CALLEE_HIGHLIGHT, CALLEE_ALLOC, CALLEE_FREE = 0x1006CE28, 0x10064BD4, 0x10064C54

ADDR = dict(hl=0x102C6CE8, flash=0x102D1F00, colours=0x102D1F0C, skirt=0x102DA2EC, rows=0x102DA92C, cols=0x102DA930)
HOST_LAYOUT = [0x30, 0x44, 0x4A, 0x5C, 0x70, 0x9C, 0, 0x58, 0x5C, 0x5E, 0x64, 0xA0, 8, 8]
MAC_LAY = dict(sflags=4, heads=8, fd0=0xD0, ssize=0x100, mhdr=4, msize=0x40, psz=4)

CUR = {"new": 0, "P": 0}                 # what the PowerPC fakes answer / expect, set by build


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_highlight(e):
    e.cl.rec("HighlightRide", ())


def stub_alloc(e):
    e.cl.rec("MReleaseAllocF", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, CUR["new"] & MASK)


def stub_free(e):
    e.cl.rec("MReleaseFreeF", (1 if (e.r(3) & MASK) == CUR["P"] else 0,))
    e.set_r(3, 0)


STUBS.internal(CALLEE_HIGHLIGHT, stub_highlight)
STUBS.internal(CALLEE_ALLOC, stub_alloc)
STUBS.internal(CALLEE_FREE, stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def off(self, a):
        """A Ptr32 word for an address (the Mac's address; the host's offset from the arena base)."""
        return a - self.base if a else 0

    def ptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.lay["psz"]))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    lay = MAC_LAY
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
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

    def cfg(self, new, p):
        CUR["new"], CUR["P"] = new, p

    def call(self, op, *args):
        T.call_ppc(self.e, ENTRY[op], [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    @property
    def base(self):
        return self.lib.tpw_dr_base()      # read late: the arena is created by the first allocation

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

    def cfg(self, new, p):
        self.lib.tpw_dr_cfg(new, p)

    def call(self, op, *args):
        fn = getattr(self.lib, "tpw_dr_" + op)
        if op == "decide":
            fn(args[0], s32(args[1]), s32(args[2]))
        else:
            fn(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dr_addr.restype, lib.tpw_dr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dr_alloc.restype, lib.tpw_dr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dr_base.restype, lib.tpw_dr_base.argtypes = C.c_uint64, []
    lib.tpw_dr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dr_cfg.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_dr_unhighlight.argtypes = []
    lib.tpw_dr_start.argtypes = [C.c_uint64]
    lib.tpw_dr_stop.argtypes = [C.c_uint64]
    lib.tpw_dr_decide.argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_dr_unhl = lib.tpw_dr_unhighlight
    out = (C.c_int64 * 32)()
    n = lib.tpw_dr_layout(out)
    vals = list(out)[:n]
    assert vals[:len(HOST_LAYOUT)] == HOST_LAYOUT, vals
    sflags, heads, fd0, ssize, mhdr, msize = vals[len(HOST_LAYOUT):]
    lib.lay = dict(sflags=sflags, heads=heads, fd0=fd0, ssize=ssize, mhdr=mhdr, msize=msize, psz=8)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mk_set(s, sp, known, tag):
    L = s.lay
    n = len(sp["nodes"])
    ride, mesh = s.alloc(max(L["ssize"], 0x100) + 0x10), s.alloc(max(L["msize"], 0x40) + 0x10)
    H, OH = s.alloc(0xC0), s.alloc(0xC0)
    nodes, onodes = s.alloc(0xA0 * max(n, 1)), s.alloc(0xA0 * max(n, 1))
    P, Q = s.alloc(16), s.alloc(16)
    known[s.off(P)], known[s.off(Q)] = tag + "P", tag + "Q"
    s.words(ride + L["sflags"], [sp["flags"]])
    s.ptr(ride + L["heads"], mesh)
    s.ptr(mesh + L["mhdr"], H)
    s.words(H + 0x30, [sp["hflags"]])
    s.half(H + 0x44, n)
    s.half(H + 0x4A, sp["pcount"])
    s.words(H + 0x5C, [s.off(P)])
    s.words(H + 0x70, [s.off(nodes)])
    s.words(H + 0x9C, [s.off(OH)])
    s.words(OH + 0x5C, [s.off(P if sp["same"] else Q)])
    s.words(OH + 0x70, [s.off(onodes)])
    dsts, srcs = [], []
    for i, nd in enumerate(sp["nodes"]):
        a, oa = nodes + 0xA0 * i, onodes + 0xA0 * i
        s.words(a, [nd["nf"]])
        s.half(a + 0x58, nd["c58"])
        s.half(a + 0x5C, nd["c5c"])
        s.half(a + 0x5E, nd["c5e"])
        dst = src = 0
        if nd["dst"]:
            dst = s.alloc(64)
            s.words(dst, nd["dstw"])
            known[s.off(dst)] = "%sD%d" % (tag, i)
        if nd["src"]:
            src = s.alloc(64)
            s.words(src, nd["srcw"])
            known[s.off(src)] = "%sS%d" % (tag, i)
        s.words(a + 0x64, [s.off(dst)])
        s.words(oa, [nd["onf"]])
        s.words(oa + 0x64, [s.off(src)])
        dsts.append(dst)
        srcs.append(src)
    return dict(ride=ride, H=H, OH=OH, nodes=nodes, onodes=onodes, n=n, P=P, dsts=dsts, srcs=srcs)


def build(s, case):
    w = dict(sets=[], known={}, spare=[], new=0, shape=0)
    known = w["known"]
    for k, sp in enumerate(case["sets"]):
        w["sets"].append(mk_set(s, sp, known, "s%d" % k))
    for k, ws in enumerate(case["spare"]):
        a = s.alloc(64)
        s.words(a, ws)
        known[s.off(a)] = "C%d" % k
        w["spare"].append(a)
    colours = s.alloc(64)
    s.words(colours, [s.off(w["spare"][c]) if c >= 0 else 0 for c in case["colours"]])
    s.ptr(s.g("colours"), colours)
    prev = 0
    for k in reversed(case["chain"]):
        s.ptr(w["sets"][k]["ride"] + s.lay["fd0"], prev)
        prev = w["sets"][k]["ride"]
    s.ptr(s.g("flash"), prev)
    s.ptr(s.g("hl"), w["sets"][case["hl"]]["ride"] if case["hl"] >= 0 else 0)
    w["new"] = s.alloc(256)
    known_p = w["sets"][case["target"]]["P"] if case["sets"] else 0
    s.cfg(w["new"], known_p)
    if case["op"] == "decide":
        w["shape"] = s.alloc(8 + 3200)
        s.words(w["shape"], [case["width"], case["height"]])
        s.words(w["shape"] + 8, case["cells"])
        s.words(s.g("skirt"), case["skirt"])
    return w


def norm(s, w, p):
    if p == 0:
        return "0"
    if p in w["known"]:
        return w["known"][p]
    off = p - s.off(w["new"])
    if 0 <= off < 0x2000000:
        return "new+%d" % off
    return "?%x" % (p & MASK)


def observe(s, case, w):
    L = s.lay
    ride_names = {st["ride"]: "set%d" % k for k, st in enumerate(w["sets"])}

    def rname(p):
        return ride_names.get(p, "0" if p == 0 else "?")

    out = {"log": s.log(), "flash": rname(s.rd_ptr(s.g("flash"))), "hl": rname(s.rd_ptr(s.g("hl"))),
           "skirt": s.rd_words(s.g("skirt"), 400), "rows": s.rd_word(s.g("rows")), "cols": s.rd_word(s.g("cols"))}
    for k, st in enumerate(w["sets"]):
        out["set%d" % k] = dict(
            flags=s.rd_word(st["ride"] + L["sflags"]), fd0=rname(s.rd_ptr(st["ride"] + L["fd0"])),
            hflags=s.rd_word(st["H"] + 0x30), hpoints=norm(s, w, s.rd_word(st["H"] + 0x5C)),
            opoints=norm(s, w, s.rd_word(st["OH"] + 0x5C)),
            nodes=[(s.rd_word(st["nodes"] + 0xA0 * i), norm(s, w, s.rd_word(st["nodes"] + 0xA0 * i + 0x64))) for i in range(st["n"])],
            onodes=[(s.rd_word(st["onodes"] + 0xA0 * i), norm(s, w, s.rd_word(st["onodes"] + 0xA0 * i + 0x64))) for i in range(st["n"])],
            blocks=[s.rd_words(a, 16) if a else [] for a in st["dsts"] + st["srcs"]])
    out["spare"] = [s.rd_words(a, 16) for a in w["spare"]]
    return out


def execute(s, case, w):
    op = case["op"]
    if op == "unhl":
        s.call("unhl")
    elif op == "decide":
        s.call("decide", w["shape"], case["width"], case["height"])
    else:
        s.call(op, w["sets"][case["target"]]["ride"])


def run(s, case):
    try:
        w = build(s, case)
        execute(s, case, w)
        return observe(s, case, w)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rb(r, p=0.5):
    return r.random() < p


def bits(r, probs):
    v = r.getrandbits(32)
    for b, p in probs.items():
        v = (v | b) if rb(r, p) else (v & ~b)
    return v & MASK


def rwords(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def gen_set(r, same=None, n=None, f20000=None, f4000000=None, h100000=None, h400000=None):
    nn = r.randint(0, 4) if n is None else n
    nodes = []
    for _ in range(nn):
        nodes.append(dict(nf=bits(r, {0x4000: 0.5, 0x1000000: 0.5}), c58=r.choice([0, 1, 2, 3, 0xFFFF, r.randint(0, 6)]),
                          c5c=r.choice([0, 1, 4, 0xFFFF, r.randint(0, 6)]), c5e=r.choice([0, 1, 2, 5, 3]),
                          dst=rb(r, 0.75), src=rb(r, 0.75), onf=r.getrandbits(32), dstw=rwords(r, 16), srcw=rwords(r, 16)))
    return dict(nodes=nodes, pcount=r.choice([0, 1, 5, r.randint(0, 9)]), same=rb(r) if same is None else same,
                flags=bits(r, {0x20000: 0.5 if f20000 is None else float(f20000), 0x4000000: 0.5 if f4000000 is None else float(f4000000)}),
                hflags=bits(r, {0x100000: 0.5 if h100000 is None else float(h100000), 0x400000: 0.5 if h400000 is None else float(h400000),
                                0x800000: 0.5}))


def base(r, op, ksets=None, **kw):
    k = r.randint(1, 4) if ksets is None else ksets
    sets = [gen_set(r, **kw) for _ in range(k)]
    target = r.randrange(k)
    others = [i for i in range(k) if i != target]
    r.shuffle(others)
    chain = others[:r.randint(0, len(others))]
    return dict(op=op, queues={}, sets=sets, target=target, chain=chain, spare=[rwords(r, 16) for _ in range(8)],
                colours=[r.randrange(-1, 8) for _ in range(8)], hl=r.randrange(-1, k))


def with_target_in_chain(r, c, head=None):
    pos = 0 if head else (len(c["chain"]) if head is False else r.randint(0, len(c["chain"])))
    if head is False and not c["chain"]:
        c["chain"] = [c["target"]]
    else:
        c["chain"].insert(pos, c["target"])
    return c


def unhl_case(r, **kw):
    c = base(r, "unhl", **kw)
    return c


def start_case(r, **kw):
    return base(r, "start", **kw)


def stop_case(r, head=None, hlmode=None, **kw):
    c = with_target_in_chain(r, base(r, "stop", **kw), head)
    if hlmode == "t":
        c["hl"] = c["target"]
    elif hlmode == "o":
        o = [i for i in range(len(c["sets"])) if i != c["target"]]
        c["hl"] = r.choice(o) if o else -1
    elif hlmode == "n":
        c["hl"] = -1
    return c


def shape_case(r, w=None, h=None):
    c = dict(op="decide", queues={}, sets=[], target=0, chain=[], spare=[], colours=[], hl=-1)
    c["width"] = r.randint(0, 20) if w is None else w
    c["height"] = r.randint(0, 20) if h is None else h
    pool = [0, 0, 1, 2, 4, 8, 16, 3, 5, 0xA, 0x10, 0x20, 0xF, 0x30]
    cells = []
    for _ in range(400):
        cells += [r.choice(pool) if rb(r, 0.9) else r.getrandbits(32), r.getrandbits(32)]
    c["cells"] = cells
    c["skirt"] = rwords(r, 400)
    c["width"] &= MASK
    c["height"] &= MASK
    return c


def unhl_edges():
    r = random.Random(11)
    for f2 in (False, True):
        for f4 in (False, True):
            for n in (0, 1, 3):
                for hl in (-1, 0):
                    c = unhl_case(r, ksets=2, n=n, f20000=f2, f4000000=f4, h400000=True)
                    c["hl"] = hl
                    yield c


def start_edges():
    r = random.Random(12)
    for same in (False, True):
        for n in (0, 1, 3):
            yield start_case(r, ksets=2, same=same, n=n)
    c = start_case(r, ksets=1, same=True, n=2)
    c["chain"] = [0]
    yield c


def stop_edges():
    r = random.Random(13)
    for free in (False, True):
        for head in (True, False):
            for hlmode in ("n", "t", "o"):
                for f2 in (False, True):
                    c = stop_case(r, head=head, hlmode=hlmode, ksets=3, h100000=free, f20000=f2, n=r.choice([1, 2, 4]))
                    yield c


def decide_edges():
    r = random.Random(14)
    for w, h in ((0, 0), (1, 1), (20, 20), (3, 5), (5, 3), (-1, 2), (2, -1), (20, 1), (1, 20), (2, 2), (0, 5), (5, 0)):
        yield shape_case(r, w, h)


TESTS = [
    FnTest("UnhighlightRide", ppc, host, edge=unhl_edges, rnd=lambda r: unhl_case(r)),
    FnTest("StartFlashingObject", ppc, host, edge=start_edges, rnd=lambda r: start_case(r)),
    FnTest("StopFlashingObject", ppc, host, edge=stop_edges, rnd=lambda r: stop_case(r)),
    FnTest("DecideRideShapeDeformEdges", ppc, host, edge=decide_edges, rnd=lambda r: shape_case(r)),
]
