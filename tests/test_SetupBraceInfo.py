"""Unit v5731, the brace info blocks and the brace/pylon list links, against libtpw_unit_SetupBraceInfo.dylib (tests/unit_SetupBraceInfo.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp:
  SetupBraceInfo 0x1003df2c, DestroyBraceInfo 0x1003e064, LinkPylon 0x1003e344, LinkBrace 0x1003e48c, UnLinkBrace 0x1003e5c0

A case is a world and a call. The world is a set of named objects (braces, pylons, a parameters block, an info, blocks) built twice by ONE
piece of Python code: in emulator memory (4-byte pointers, Mac offsets) and in the host library (8-byte pointers, the host offsets exported by
tpw_sbi_layout). Pointers are compared as the names of the objects they point at. The callees outside the unit (MReleaseAllocF,
MReleaseFreeF, FindDataNodeIndexByID, GetRideRequiredYOffset, CorrectBraceNumbering) are fakes on both sides that record every argument.

What is compared: every field of every object after the call, the allocated entry blocks of SetupBraceInfo, and the event log.
No fused operations. Not tested: UnLinkBrace on a linked brace with no next brace when the head and tail differ (the original dereferences
it), a NULL pylon tail in LinkPylon and a NULL tail in LinkBrace (read unconditionally).

With the port fixes on (unit coaster64), the parameter block is the instance's words from mBlock3C, and LinkPylon, LinkBrace and
UnLinkBrace keep the Mac block's words 0x08 (the brace count), 0x14 (the last pylon) and 0x24 (the editor's brace) in the instance's
mBraceCount, mPylonTail and mPreviewBrace (the port fixes in sCoasterTrackParams, LinkPylon, LinkBrace and UnLinkBrace,
src/game/rides/CoasterPylon.cpp): the host world allocates the block as an instance and writes and reads those words there
(tpw_sbi_layout).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SetupBraceInfo"
MASK = 0xFFFFFFFF

FN = dict(setup=0x1003DF2C, destroy=0x1003E064, linkpylon=0x1003E344, linkbrace=0x1003E48C, unlink=0x1003E5C0)
SIZES = {0x1003DF2C: 212, 0x1003E064: 92, 0x1003E344: 256, 0x1003E48C: 236, 0x1003E5C0: 212}
OWN = set(SIZES)

ADDR = dict(gRideTypes=0x102D19B0)

# (kind, field, Mac offset, is a pointer), in the order of tpw_sbi_layout
FIELDS = [
    ("brace", "flags", 0x08, False), ("brace", "num", 0x00, False), ("brace", "prev", 0x64, True), ("brace", "next", 0x68, True),
    ("brace", "data", 0x100, True),
    ("data", "flags", 0x04, False),
    ("params", "pcnt", 0x04, False), ("params", "bcnt", 0x08, False), ("params", "ptail", 0x14, True), ("params", "bhead", 0x1C, True),
    ("params", "btail", 0x24, True),
    ("pylon", "num", 0x00, False), ("pylon", "flags", 0x04, False), ("pylon", "index", 0x08, False), ("pylon", "cx", 0x0C, False),
    ("pylon", "cz", 0x10, False), ("pylon", "x", 0x1C, False), ("pylon", "y", 0x20, False), ("pylon", "z", 0x24, False),
    ("pylon", "next", 0x28, True), ("pylon", "prev", 0x2C, True),
    ("info", "cnt", 0x00, False), ("info", "a", 0x04, True), ("info", "b", 0x08, True),
    ("ctl", "cnt", 0x0C, False), ("ctl", "base", 0x10, False),
    ("pctl", "rtype", 0x14, False),
]
MACOFF = {(k, f): o for k, f, o, p in FIELDS}
HOSTOFF = {}
PARAMS = {}        # where the host's parameter block sits in the object allocated for it, and that object's size (tpw_sbi_layout)
ISPTR = {(k, f): p for k, f, o, p in FIELDS}
FLOATS = {("pylon", "x"), ("pylon", "y"), ("pylon", "z")}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("MReleaseAllocF", (size, e.r(4) & MASK))
    n = size or 4
    a = e.alloc(n)
    e.write(a, bytes(n))
    e.set_r(3, a)


def stub_free(e):
    L = e.cl
    L.rec("MReleaseFreeF", (L.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_find(e):
    L = e.cl
    L.rec("Find", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("Find") & MASK)


def stub_yoff(e):
    L = e.cl
    L.rec("Yoff", tuple(e.r(i) & MASK for i in range(3, 10)))
    e.set_f1(bitsf(L.ret("Yoff")))


def stub_correct(e):
    L = e.cl
    L.rec("Correct", (L.lab(e.r(3)),))


STUBS.internal(0x10064BD4, stub_alloc)
STUBS.internal(0x10064C54, stub_free)
STUBS.internal(0x101FFDC8, stub_find)
STUBS.internal(0x101ED29C, stub_yoff)
STUBS.internal(0x100564AC, stub_correct)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def w32(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def pname(self, v):
        if v == 0:
            return 0
        return self.names.get(v, "new")

    def off(self, kind, f):
        return self.OFF[(kind, f)]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    OFF = MACOFF

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = STUBS.activate(e)

    def begin(self, labels):
        fixed = {self.obj[n]: l for n, l in labels.items()}
        self.L = self.e.cl = CL.PpcLog(fixed, self.case["queues"])

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, vals):
        T.call_ppc(self.e, FN[op], [v & MASK for v in vals])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.OFF = HOSTOFF

    def begin(self, labels):
        CL.host_begin(self.lib, {l: self.obj[n] for n, l in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_sbi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, vals):
        getattr(self.lib, "tpw_sbi_" + op)(*vals)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sbi_addr.restype, lib.tpw_sbi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sbi_alloc.restype, lib.tpw_sbi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sbi_layout.argtypes = [C.POINTER(C.c_int64)]
    P, U, I = C.c_uint64, C.c_uint32, C.c_int32
    lib.tpw_sbi_setup.argtypes = [P, P, P, P]
    lib.tpw_sbi_destroy.argtypes = [P, P]
    lib.tpw_sbi_linkpylon.argtypes = [P, P, U, U, U]
    lib.tpw_sbi_linkbrace.argtypes = [P, P, I]
    lib.tpw_sbi_unlink.argtypes = [P, P]
    out = (C.c_int64 * 64)()
    n = lib.tpw_sbi_layout(out)
    assert n == len(FIELDS) + 2, n
    for (k, f, o, p), v in zip(FIELDS, list(out)[:n]):
        HOSTOFF[(k, f)] = v
    PARAMS.update(base=out[n - 2], size=out[n - 1])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    objs = case["objs"]
    s.obj = {}
    s.names = {}
    for n in sorted(objs):
        if s.psz == 8 and objs[n][0] == "params":
            a = s.alloc(max(0x400, PARAMS["size"])) + PARAMS["base"]
        else:
            a = s.alloc(0x400)
        s.obj[n] = a
        s.names[a] = n
    for n, (kind, vals) in objs.items():
        for f, v in vals.items():
            a = s.obj[n] + s.off(kind, f)
            if ISPTR[(kind, f)]:
                s.wptr(a, 0 if v is None else s.obj[v])
            else:
                s.w32(a, v)
    s.begin({n: i + 1 for i, n in enumerate(sorted(objs))})
    if "rt" in case:
        if s.psz == 4:
            s.w32(ADDR["gRideTypes"] + 4 * case["rt"], s.obj["S"])
        else:
            s.wptr(s.lib.tpw_sbi_addr(b"gRideTypes") + 8 * case["rt"], s.obj["S"])


def execute(s, case):
    vals = [s.obj[v] if k == "o" else v for k, v in case["args"]]
    s.call(case["op"], vals)


def dump(s, kind, n):
    d = {}
    for k, f, o, p in FIELDS:
        if k != kind:
            continue
        a = s.obj[n] + s.off(kind, f)
        if p:
            d[f] = s.pname(s.rptr(a))
        else:
            v = s.r32(a)
            d[f] = canon_f(v) if (k, f) in FLOATS else v
    return d


def entries(s, p, count):
    out = []
    for k in range(count):
        b = p + k * 0x58
        out.append((s.read(b, 4), s.r32(b + 4), s.read(b + 8, 8), s.pname(s.rptr(b + 0x10)), s.read(b + 0x18, 0x40)))
    return out


def observe(s, case):
    res = {"objs": {n: dump(s, kind, n) for n, (kind, vals) in case["objs"].items()}, "log": s.log()}
    if case["op"] == "setup":
        info = s.obj["I"]
        count = s.r32(info + s.off("info", "cnt"))
        res["a"] = entries(s, s.rptr(info + s.off("info", "a")), count)
        res["b"] = entries(s, s.rptr(info + s.off("info", "b")), count)
    return res


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


def r32(r):
    return r.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(16)])


def mk(r, kind, **given):
    vals = {}
    for k, f, o, p in FIELDS:
        if k != kind:
            continue
        if f in given:
            vals[f] = given[f]
        elif p:
            vals[f] = None
        else:
            vals[f] = r32(r)
    return (kind, vals)


def rflags(r, bit=None, mask_bit=0x8000):
    v = r.choice([0, 0xFFFFFFFF, 0x200, 0x50000, 0x4000, r.getrandbits(32), r.getrandbits(32)])
    v |= r.choice([0, 0x200, 0x10000, 0x40000])
    if bit is not None:
        v = (v | mask_bit) if bit else (v & ~mask_bit & MASK)
    return v & MASK


def yoffbits(r):
    return r.choice([0, fb(1.5), fb(-3.0), 0x7FC00000, 0x7F800000, 0x80000000, fb(r.uniform(-1000, 1000))])


# ---- SetupBraceInfo
def setup_case(r, count=None, base=None):
    count = r.choice([0, 1, 2, 3, 7]) if count is None else count
    base = r.choice([0, 1, 5, 0xFFFFFFFE, r.getrandbits(32)]) if base is None else base
    rt = r.randrange(1, 330)
    objs = {"B": mk(r, "brace"), "I": mk(r, "info", a=None, b=None), "C": mk(r, "ctl", cnt=count, base=base),
            "P": mk(r, "pctl", rtype=rt), "S": ("set", {})}
    return {"op": "setup", "objs": objs, "rt": rt, "queues": {"Find": [r.getrandbits(32) for _ in range(8)]},
            "args": [("o", "B"), ("o", "I"), ("o", "C"), ("o", "P")]}


def setup_edges():
    r = random.Random(5731)
    for count in (0, 1, 2, 5):
        for base in (0, 1, 0xFFFFFFFF):
            yield setup_case(r, count, base)


def setup_random(r):
    return setup_case(r)


# ---- DestroyBraceInfo
def destroy_case(r, dflags=None):
    dflags = r.choice([0, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF, r.getrandbits(32)]) if dflags is None else dflags
    objs = {"B": mk(r, "brace", data="D"), "D": mk(r, "data", flags=dflags), "I": mk(r, "info", a="A", b="K"),
            "A": ("blk", {}), "K": ("blk", {})}
    return {"op": "destroy", "objs": objs, "queues": {}, "args": [("o", "B"), ("o", "I")]}


def destroy_edges():
    r = random.Random(5732)
    for f in (0, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF):
        yield destroy_case(r, f)


def destroy_random(r):
    return destroy_case(r)


# ---- LinkPylon
def linkpylon_case(r, x=None, z=None, nxt=None):
    x = r32(r) if x is None else x
    z = r32(r) if z is None else z
    nxt = r.choice([True, False]) if nxt is None else nxt
    objs = {"Y": mk(r, "pylon"), "P": mk(r, "params", ptail="T"), "T": mk(r, "pylon", next="N" if nxt else None),
            "N": mk(r, "pylon", prev=r.choice([None, "T"]))}
    return {"op": "linkpylon", "objs": objs, "queues": {"Yoff": [yoffbits(r)]},
            "args": [("o", "Y"), ("o", "P"), ("i", x), ("i", z), ("i", r32(r))]}


def linkpylon_edges():
    r = random.Random(5733)
    vals = (0, 1, 7, 0xFFFFFFFF, 0x1000001, 0x80000000, 100)
    for x in vals:
        for z in vals[::2]:
            yield linkpylon_case(r, x, z, r.choice([True, False]))
    yield linkpylon_case(r, 3, 4, True)
    yield linkpylon_case(r, 3, 4, False)


def linkpylon_random(r):
    return linkpylon_case(r)


# ---- LinkBrace
def linkbrace_case(r, valid=None, same=None, f200=None):
    valid = r.choice([0, 1, -1, 5]) if valid is None else valid
    same = r.choice([True, False]) if same is None else same
    f200 = r.choice([True, False]) if f200 is None else f200
    tflags = rflags(r, f200, 0x200)
    objs = {"B": mk(r, "brace", flags=rflags(r)), "P": mk(r, "params", btail="T", bhead="T" if same else "H"),
            "T": mk(r, "brace", flags=tflags, prev=r.choice([None, "V"])), "V": mk(r, "brace"), "H": mk(r, "brace")}
    return {"op": "linkbrace", "objs": objs, "queues": {}, "args": [("o", "B"), ("o", "P"), ("i", valid)]}


def linkbrace_edges():
    r = random.Random(5734)
    for valid in (0, 1, -1):
        for same in (True, False):
            for f200 in (True, False):
                yield linkbrace_case(r, valid, same, f200)


def linkbrace_random(r):
    return linkbrace_case(r)


# ---- UnLinkBrace
def unlink_case(r, linked=None, same=None):
    linked = r.choice([True, False]) if linked is None else linked
    same = r.choice([True, False]) if same is None else same
    nxt = r.choice([None, "X"]) if same else "X"
    objs = {"B": mk(r, "brace", flags=rflags(r, linked), prev=r.choice([None, "V"]), next=nxt),
            "P": mk(r, "params", bhead="H", btail="H" if same else "T"),
            "X": mk(r, "brace", prev="B"), "V": mk(r, "brace", next="B"), "H": mk(r, "brace"), "T": mk(r, "brace")}
    return {"op": "unlink", "objs": objs, "queues": {}, "args": [("o", "B"), ("o", "P")]}


def unlink_edges():
    r = random.Random(5735)
    for linked in (True, False):
        for same in (True, False):
            for _ in range(2):
                yield unlink_case(r, linked, same)


def unlink_random(r):
    return unlink_case(r)


TESTS = [
    FnTest("SetupBraceInfo", ppc, host, edge=setup_edges, rnd=setup_random),
    FnTest("DestroyBraceInfo", ppc, host, edge=destroy_edges, rnd=destroy_random),
    FnTest("LinkPylon", ppc, host, edge=linkpylon_edges, rnd=linkpylon_random),
    FnTest("LinkBrace", ppc, host, edge=linkbrace_edges, rnd=linkbrace_random),
    FnTest("UnLinkBrace", ppc, host, edge=unlink_edges, rnd=unlink_random),
]
