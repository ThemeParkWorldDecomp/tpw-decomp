"""Unit v5673, RecalcBraceTangents, against libtpw_unit_RecalcBraceTangents.dylib (tests/unit_RecalcBraceTangents.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp:
  RecalcBraceTangents 0x1003d080

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, the host's field offsets from tpw_rbt_layout) for the
translation. The callees outside the unit (GetHeightForInterval, GetRequiredBraceTangents, GetAlignmentParamsSlow, CTRACE, GetBankAtBrace,
GetCarWantedSpeedAtHeight, SetupBraceAnimFrameData, AnimateRideSet, GetActualBraceTrackPositions, GetActualBraceTangents) are fakes on both
sides that record their arguments (the point and the orientation answers of GetAlignmentParamsSlow, the CTRACE text as a hash) and answer from
the case's queues; modf is the C library's on the host and a Python fake with the same semantics on the PowerPC side.

The world of a case: a coaster instance (type flags and the float at 0x48; the pylon list; the chain of braces), pylons with their data
(flags, control, x y z, braces), braces with every field the function reads, gRideAnimSets, gCoasterWord7310. On the Mac a brace's +0x100
word and the list node are one object; on the host they are two (the node and the data view), written alike. What is compared: the event
log and every field of every brace the function writes.

With the port fixes on (unit coaster64), the port fix in RecalcBraceTangents reads the type's word 0x48 as tag_sCoasterType's mF48, and
the ones in sTrackBraceData and sCoasterPylon put the data record's x y z, braces and control where sPylonRecord keeps them: the host world
writes them at those offsets (tpw_rbt_layout), and the type block is a whole tag_sCoasterType.

No fused operations. Not tested: a brace index of 2 (the host's data record overlays that slot with mF18Table), a brace index past its pylon's braces (a NULL read), NaN or infinite orientation answers (the doubles in
the CTRACE words), the first brace's stale animation set (r23 is zeroed before the call, the translation starts with NULL).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RecalcBraceTangents"
MASK = 0xFFFFFFFF

FN = 0x1003d080
SIZES = {FN: 1056}
OWN = set(SIZES)

CALLEES = dict(
    height=0x10040eb8, required=0x1003aef4, align=0x1004e230, ctrace=0x1006fcbc, bank=0x1004b1f8, speed=0x1004aedc,
    setup=0x1003bb44, animate=0x100c74d4, positions=0x1003cc34, tangents=0x1003c950,
)

ADDR = dict(gCoasterCurrentData=0x102b68a4, gCoasterCurrent=0x102b68a8, gCoasterWord7310=0x102b7310, gRideAnimSets=0x102c79a4)

# (name, Mac offset); the host's come from tpw_rbt_layout in this order
FIELDS = [
    ("flags", 8), ("prev", 0x64), ("point", 0x34), ("x4c", 0x4c), ("l50", 0x50), ("z54", 0x54), ("f6c", 0x6c), ("w70", 0x70), ("d4", 0xd4),
    ("d8", 0xd8), ("dc", 0xdc), ("angle", 0xe0), ("fe8", 0xe8), ("fec", 0xec), ("ff0", 0xf0), ("ff4", 0xf4), ("data", 0x100), ("idx", 4),
    ("number", 0),
    ("next", 0x28), ("count", 0x30), ("braces", 0x34),
    ("dflags", 4), ("f18", 0x18), ("xyz", 0x1c), ("dbraces", 0x34),
    ("f2c", 0x2c), ("fc4", 0xc4),
    ("type", 4), ("pylons", 0x4c), ("last", 0x54),
    ("tf48", 0x48), ("tsize", 0x100), ("portfix", 0),
]
MAC_OFF = dict(FIELDS)
HOST_OFF = {}
# the fields the function writes, compared after the call
OUT_FIELDS = ["x4c", "l50", "z54", "d4", "d8", "dc", "angle", "fec", "ff0", "ff4"]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def fnv(data):
    if isinstance(data, str):
        data = data.encode("latin1")
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake_height(e):
    L = e.cl
    L.rec("GetHeightForInterval", (fb(e.f(1)), L.lab(e.r(4))))
    e.set_f1(bitsf(L.ret("height")))


def fake_required(e):
    L = e.cl
    L.rec("GetRequiredBraceTangents", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


def fake_align(e):
    L = e.cl
    p = e.r(4)
    L.rec("GetAlignmentParamsSlow", (L.lab(e.r(3)), canon_f(e.u32(p)), canon_f(e.u32(p + 4)), canon_f(e.u32(p + 8)), fb(e.f(1)),
                                     e.r(7) & MASK, L.lab(e.r(8))))
    o = e.r(6)
    e.write(o + 4, struct.pack(">I", L.ret("gap4") & MASK))
    e.write(o + 8, struct.pack(">I", L.ret("gap8") & MASK))


def fake_ctrace(e):
    L = e.cl
    L.rec("CTRACE", (e.r(3) & MASK, fnv(e.cstr(e.r(4))), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK, e.r(8) & MASK, e.r(9) & MASK))


def fake_bank(e):
    L = e.cl
    L.rec("GetBankAtBrace", (L.lab(e.r(3)), L.lab(e.r(4)), fb(e.f(1))))
    e.set_f1(bitsf(L.ret("bank")))


def fake_speed(e):
    L = e.cl
    L.rec("GetCarWantedSpeedAtHeight", (L.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3)), fb(e.f(4))))
    e.set_f1(bitsf(L.ret("speed")))


def fake_setup(e):
    L = e.cl
    L.rec("SetupBraceAnimFrameData", (L.lab(e.r(3)), L.lab(e.r(4))))


def fake_animate(e):
    L = e.cl
    L.rec("AnimateRideSet", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def fake_positions(e):
    L = e.cl
    L.rec("GetActualBraceTrackPositions", (L.lab(e.r(3)), L.lab(e.r(4))))


def fake_tangents(e):
    L = e.cl
    L.rec("GetActualBraceTangents", (L.lab(e.r(3)), L.lab(e.r(4))))


def fake_modf(e):
    frac, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(frac)


for key, fn in (("height", fake_height), ("required", fake_required), ("align", fake_align), ("ctrace", fake_ctrace), ("bank", fake_bank),
                ("speed", fake_speed), ("setup", fake_setup), ("animate", fake_animate), ("positions", fake_positions),
                ("tangents", fake_tangents)):
    STUBS.internal(CALLEES[key], fn)
STUBS.imp("modf", fake_modf)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def o(self, name):
        return self.off[name]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    off = MAC_OFF
    shared = True

    def __init__(self, e, case):
        self.e = e
        self.saved = None

    def begin(self, case):
        self.L = self.e.cl = CL.PpcLog(self.lab, case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
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

    def call(self):
        self.e.set_r(23, 0)
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    shared = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST_OFF

    def begin(self, case):
        CL.host_begin(self.lib, {v: k for k, v in self.lab.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rbt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rbt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_rbt_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rbt_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, []
    out = (C.c_int64 * 48)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(FIELDS), n
    for (name, _), v in zip(FIELDS, out):
        HOST_OFF[name] = v
    # the fields the function copies as bytes sit at the Mac offsets on the host too (the port fix reads x y z at the record's mX)
    for name in ("flags", "point", "idx", "number", "dflags", "f2c", "fc4") + (() if HOST_OFF["portfix"] else ("xyz",)):
        assert HOST_OFF[name] == MAC_OFF[name], (name, HOST_OFF[name])
    assert HOST_OFF["tsize"] >= MAC_OFF["tsize"], HOST_OFF["tsize"]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.lab = {}
    o = s.o
    inst = s.alloc(0x800)
    s.lab[inst] = 1
    s.lab[inst + 0x3c] = 3
    typ = s.alloc(o("tsize"))
    s.lab[typ] = 2
    s.words(typ, [case["tflags"]])
    s.words(typ + o("tf48"), [case["tf48"]])
    s.ptr(inst + o("type"), typ)

    nodes, datas, ctrls = [], [], []
    for p, py in enumerate(case["pylons"]):
        count = len(py["braces"])
        node = s.alloc(0x100 + 8 * count)
        data = node if s.shared else s.alloc(0x100 + 8 * count)
        ctrl = s.alloc(0x400)
        s.lab[ctrl] = 40 + p
        s.words(ctrl + o("f2c"), [py["f2c"]])
        s.words(ctrl + o("fc4"), [py["fc4"]])
        nodes.append(node)
        datas.append(data)
        ctrls.append(ctrl)
    braces = []
    for i, b in enumerate(case["braces"]):
        a = s.alloc(0x400)
        s.lab[a] = 20 + i
        braces.append(a)
    for p, py in enumerate(case["pylons"]):
        node, data = nodes[p], datas[p]
        s.words(node + o("count"), [len(py["braces"])])
        for k, bi in enumerate(py["braces"]):
            s.ptr(node + o("braces") + s.psz * k, braces[bi])
            s.ptr(data + o("dbraces") + s.psz * k, braces[bi])
        s.words(data + o("dflags"), [py["dflags"]])
        s.ptr(data + o("f18"), ctrls[p])
        s.words(data + o("xyz"), py["xyz"])
    listed = [nodes[p] for p, py in enumerate(case["pylons"]) if py["listed"]]
    for k, node in enumerate(listed):
        s.ptr(node + o("next"), listed[k + 1] if k + 1 < len(listed) else 0)
    s.ptr(inst + o("pylons"), listed[0] if listed else 0)
    s.ptr(inst + o("last"), braces[0] if braces else 0)
    for i, b in enumerate(case["braces"]):
        a = braces[i]
        s.words(a + o("number"), [b["number"]])
        s.words(a + o("idx"), [b["idx"]])
        s.words(a + o("flags"), [b["flags"]])
        s.words(a + o("point"), b["point"])
        for name in ("x4c", "l50", "z54", "f6c", "w70", "d4", "d8", "dc", "angle", "fe8", "fec", "ff0", "ff4"):
            s.words(a + o(name), [b[name]])
        s.ptr(a + o("prev"), braces[i + 1] if i + 1 < len(braces) else 0)
        s.ptr(a + o("data"), datas[b["pylon"]])
    for k in range(4):
        if k == 0:
            s.ptr(s.g("gRideAnimSets"), 0)
        else:
            st = s.alloc(0x20)
            s.lab[st] = 60 + k
            s.ptr(s.g("gRideAnimSets") + s.psz * k, st)
    s.ptr(s.g("gCoasterCurrentData"), inst + 0x3c)
    s.ptr(s.g("gCoasterCurrent"), inst)
    s.words(s.g("gCoasterWord7310"), [case["w7310"]])
    s.braces = braces


def observe(s, case):
    out = {"log": s.log()}
    for i, a in enumerate(s.braces):
        out["brace%d" % i] = [canon_f(s.rd_words(a + s.o(n), 1)[0]) for n in OUT_FIELDS]
    return out


def run(s, case):
    try:
        build(s, case)
        s.begin(case)
        s.call()
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x40C90FDB, 0xC0C90FDB, 0x3F000000, 0xBF000000, 0x00000001, 0x7F7FFFFF]


def rf(r, nan=True):
    k = r.random()
    if k < 0.12:
        v = r.choice(SPECIAL + ([0x7FC00000] if nan else []))
        return v
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.85:
        return fb(r.uniform(-60, 60))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 6))


def make_case(r, nb=None, bflags=None, tflags=None, w7310=None, f2c=None, dflags=None, idx0=None, listed=None):
    nb = r.randint(0, 4) if nb is None else nb
    npy = r.choice([1, 1, 2, 3])
    pylons = []
    for p in range(npy):
        pylons.append({
            "braces": [], "dflags": r.choice([0, 0, 0, 1, 2, 3]) if dflags is None else dflags,
            "f2c": r.choice([0, 7, MASK, 0xFFFF]) if f2c is None else f2c, "fc4": rf(r),
            "xyz": [rf(r), rf(r), rf(r)], "listed": (r.random() < 0.85) if listed is None else listed,
        })
    braces = []
    for i in range(nb):
        p = r.randrange(npy)
        pylons[p]["braces"].append(i)
        braces.append({
            "pylon": p, "number": r.randrange(1 << 32), "flags": (r.choice([0, 1, 2, 4, 0x2000, 7, 0x2007, 0x40, r.randrange(1 << 16)])
                                                                if bflags is None else bflags),
            "point": [rf(r), rf(r), rf(r)], "x4c": rf(r), "l50": rf(r), "z54": rf(r), "f6c": rf(r), "w70": r.randint(0, 3),
            "d4": rf(r), "d8": rf(r), "dc": rf(r), "angle": rf(r, nan=False), "fe8": rf(r), "fec": rf(r), "ff0": rf(r), "ff4": rf(r),
        })
    for b in braces:
        n = len(pylons[b["pylon"]]["braces"])
        # idx 2 is not tested: the host's data record holds the second brace slot where its mF18Table sits (the Mac's 0x40 is not a pointer slot)
        choices = [i for i in range(1, n + 1) if i != 2]
        b["idx"] = 0 if (idx0 if idx0 is not None else r.random() < 0.4) or not choices else r.choice(choices)
        if r.random() < 0.3:
            b["d8"] = fb(r.uniform(-4, 4))
        if r.random() < 0.3:
            b["angle"] = fb(r.uniform(-20, 20))
    q = lambda: [rf(r, nan=False) for _ in range(r.randint(1, 3))]
    return {
        "pylons": pylons, "braces": braces, "tflags": r.choice([0, 0x10, 0x11, 0x1F, r.randrange(1 << 32)]) if tflags is None else tflags,
        "tf48": rf(r), "w7310": r.choice([0, 1]) if w7310 is None else w7310,
        "queues": {"height": [rf(r) for _ in range(r.randint(1, 3))], "gap4": q(), "gap8": q(), "bank": [rf(r) for _ in range(2)],
                   "speed": [rf(r) for _ in range(2)]},
    }


def edges():
    r = random.Random(673)
    yield make_case(r, nb=0)
    yield make_case(r, nb=0, listed=False)
    for tflags in (0, 0x10, 0x11):
        for w7310 in (0, 1):
            for bflags in (0, 1, 2, 4, 7, 0x2000, 0x2004):
                yield make_case(r, nb=3, bflags=bflags, tflags=tflags, w7310=w7310)
    for f2c in (MASK, 0, 0xFFFF):
        for dflags in (0, 1):
            for idx0 in (True, False):
                yield make_case(r, nb=3, f2c=f2c, dflags=dflags, idx0=idx0, listed=True)
    for nb in (1, 2, 4):
        yield make_case(r, nb=nb, tflags=0x10, bflags=0)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("RecalcBraceTangents", ppc, host, edge=edges, rnd=rnd),
]
