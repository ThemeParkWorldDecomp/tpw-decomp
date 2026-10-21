"""Unit v5039, the pylon setters of the editor's coaster, against libtpw_unit_LayoutSetRotatePylon.dylib (tests/unit_LayoutSetRotatePylon.cpp), main app
("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  SetLoftPylon 0x10055e0c, LayoutSetLoftPylon 0x10055eac, SetRotatePylon 0x10055f38, LayoutSetRotatePylon 0x10055ff0,
  SetWobblePylon 0x1005608c, LayoutSetWobblePylon 0x10056180, ToggleDrawUpSpline 0x10056258, CorrectBraceNumbering 0x100564ac

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian at the Mac's offsets, host
memory little-endian at the host struct's offsets taken from tpw_v5039_layout). GetTrackBrace, HandleCoasterDataChange and FlagGenerateTrack
are fakes on both sides recording their arguments (the brace and the instance as "is the expected one", the +0x3c block as an offset from
the instance).

The world: the edit instance global, a brace with random flags and random words in the fields written, the draw-up-spline flag, and for
CorrectBraceNumbering a chain of braces. Compared: the log, the brace's fields, the chain's numbers and floats, the flag.

No fused operations. Not tested: a NULL edit instance for the setters (the original reads through the global without checking, only
GetTrackBrace's fake sees the address).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LayoutSetRotatePylon"
MASK = 0xFFFFFFFF

F_SETLOFT, F_LSETLOFT, F_SETROT, F_LSETROT, F_SETWOB, F_LSETWOB, F_TOGGLE, F_CORRECT = (
    0x10055e0c, 0x10055eac, 0x10055f38, 0x10055ff0, 0x1005608c, 0x10056180, 0x10056258, 0x100564ac)
SIZES = {F_SETLOFT: 120, F_LSETLOFT: 96, F_SETROT: 140, F_LSETROT: 108, F_SETWOB: 200, F_LSETWOB: 168, F_TOGGLE: 80, F_CORRECT: 92}
OWN = set(SIZES)

GETBRACE, HANDLE, FLAGGEN = 0x1004de40, 0x10052318, 0x10040f08
ADDR = dict(inst=0x102b68b0, draw=0x102b74a4)
INSTANCE_SIZE = 0x400
FIELDS = ["flags", "d8", "dc", "ang", "e8", "f8"]
PPC_OFF = dict(num=0x00, flags=0x08, prev=0x64, next=0x68, f6c=0x6c, ang=0xe0, d8=0xd8, dc=0xdc, e8=0xe8, f8=0xf8, size=0x104)
HOST_KEYS = ["num", "flags", "prev", "next", "f6c", "ang", "d8", "dc", "e8", "f8", "size", "isize"]

CUR = {"brace": 0, "inst": 0}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getbrace(e):
    e.cl.rec("GetTrackBrace", ((e.r(3) - CUR["inst"]) & MASK, e.r(4) & MASK))
    e.set_r(3, CUR["brace"])


def stub_handle(e):
    e.cl.rec("HandleCoasterDataChange", (e.r(3) & MASK, int(e.r(4) == CUR["brace"]), int(e.r(5) == CUR["inst"]),
                                         s32(e.r(6)), s32(e.r(7))))


def stub_flaggen(e):
    e.cl.rec("FlagGenerateTrack", (int(e.r(3) == CUR["inst"]), s32(e.r(4))))


STUBS.internal(GETBRACE, stub_getbrace)
STUBS.internal(HANDLE, stub_handle)
STUBS.internal(FLAGGEN, stub_flaggen)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.fmt[0] + self.pfmt, self.read(a, self.psize))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = "I"
    psize = 4
    off = PPC_OFF

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def set_brace(self, a):
        CUR["brace"] = a

    def call(self, fn, args, raw):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5039_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v5039_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_brace(self, a):
        self.lib.tpw_v5039_set_brace(a)

    def call(self, fn, args, raw):
        names = {F_SETLOFT: "set_loft", F_LSETLOFT: "lset_loft", F_SETROT: "set_rot", F_LSETROT: "lset_rot", F_SETWOB: "set_wob",
                 F_LSETWOB: "lset_wob", F_TOGGLE: "toggle", F_CORRECT: "correct"}
        r = getattr(self.lib, "tpw_v5039_" + names[fn])(*raw)
        return (r or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5039_"
    lib.tpw_v5039_addr.restype, lib.tpw_v5039_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v5039_alloc.restype, lib.tpw_v5039_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v5039_set_brace.argtypes = [C.c_uint64]
    lib.tpw_v5039_layout.argtypes = [C.POINTER(C.c_int64)]
    f, u, i = C.c_float, C.c_uint32, C.c_int32
    sig = dict(set_loft=(None, [f, u, i, i]), lset_loft=(None, [f, u]), set_rot=(None, [f, u, i, i]), lset_rot=(None, [f, u]),
               set_wob=(None, [f, f, u, i, i]), lset_wob=(None, [f, f, u]), toggle=(C.c_int32, []), correct=(None, [C.c_uint64]))
    for k, (rt, at) in sig.items():
        fn = getattr(lib, p + k)
        fn.restype, fn.argtypes = rt, at
    out = (C.c_int64 * 32)()
    n = lib.tpw_v5039_layout(out)
    lib.off = dict(zip(HOST_KEYS, list(out)[:n]))
    lib.bound = True


def flt(v):
    """A float argument for call_ppc; the host unwraps it."""
    return T.F(bitsf(v))


def build(s, case):
    off = s.off
    inst = s.alloc(INSTANCE_SIZE)
    CUR["inst"] = inst
    s.wptr(s.g("inst"), inst)
    s.words(s.g("draw"), [case["draw"]])
    nodes = []
    for nd in case["nodes"]:
        a = s.alloc(off["size"] + 16)
        nodes.append(a)
    for a, nd in zip(nodes, case["nodes"]):
        s.words(a + off["num"], [nd["num"]])
        s.words(a + off["flags"], [nd["flags"]])
        for k in ("ang", "d8", "dc", "e8", "f8", "f6c"):
            s.words(a + off[k], [nd[k]])
        s.wptr(a + off["prev"], nodes[nd["prev"]] if nd["prev"] is not None else 0)
        s.wptr(a + off["next"], nodes[nd["next"]] if nd["next"] is not None else 0)
    case["_nodes"] = nodes
    s.set_brace(nodes[0])
    s.start = nodes[case["start"]] if case["start"] is not None else 0


def execute(s, case):
    op, a = case["op"], case["args"]
    if op == "toggle":
        return s.call(F_TOGGLE, [], [])
    if op == "correct":
        s.call(F_CORRECT, [s.start], [s.start])   # void: r3 is left over
        return 0
    fn = {"set_loft": F_SETLOFT, "lset_loft": F_LSETLOFT, "set_rot": F_SETROT, "lset_rot": F_LSETROT, "set_wob": F_SETWOB,
          "lset_wob": F_LSETWOB}[op]
    conv, raw = [], []
    for x, k in zip(a, case["kinds"]):
        conv.append(flt(x) if k == "f" else x & MASK)
        raw.append(bitsf(x) if k == "f" else (x & MASK if k == "u" else s32(x)))
    s.call(fn, conv, raw)   # void: r3 is left over
    return 0


def observe(s, case, answer):
    off = s.off
    nodes = []
    for a in case["_nodes"]:
        d = {"num": s.rd_word(a + off["num"]), "flags": s.rd_word(a + off["flags"])}
        for k in ("ang", "d8", "dc", "e8", "f8", "f6c"):
            d[k] = canon_f(s.rd_word(a + off[k]))
        for k in ("prev", "next"):
            p = s.rptr(a + off[k])
            d[k] = case["_nodes"].index(p) + 1 if p in case["_nodes"] else (0 if p == 0 else -1)
        nodes.append(d)
    return {"answer": answer, "log": s.log(), "nodes": nodes, "draw": s.rd_word(s.g("draw"))}


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


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def node(r, flags=None):
    return {"num": r.choice([0, 1, 5, 0x7FFFFFFF, 0xFFFFFFFF, r.randrange(0, 1 << 32)]),
            "flags": r.choice([0, 0x40, 0x46, r.randrange(0, 1 << 32), r.randrange(0, 1 << 32) | 0x40, r.randrange(0, 1 << 32) & ~0x40])
            if flags is None else flags,
            "ang": r.randrange(0, 1 << 32), "d8": r.randrange(0, 1 << 32), "dc": r.randrange(0, 1 << 32),
            "e8": r.randrange(0, 1 << 32), "f8": r.randrange(0, 1 << 32), "f6c": r.randrange(0, 1 << 32),
            "prev": None, "next": None}


def base(r, op, flags=None):
    return {"op": op, "queues": {}, "draw": r.choice([0, 1, 2, 0x100, r.randrange(0, 1 << 32)]),
            "nodes": [node(r, flags)], "start": None, "args": [], "kinds": []}


KINDS = {"set_loft": "fuii", "lset_loft": "fu", "set_rot": "fuii", "lset_rot": "fu", "set_wob": "ffuii", "lset_wob": "ffu"}


def set_case(op, r, flags=None, ints=None):
    c = base(r, op, flags)
    c["kinds"] = KINDS[op]
    args = []
    for k in c["kinds"]:
        if k == "f":
            args.append(rf(r))
        elif k == "u":
            args.append(r.choice([0, 1, 7, r.randrange(0, 1 << 32)]))
        else:
            args.append(r.choice([0, 1, -1, r.randrange(-100, 100)]) & MASK)
    if ints is not None:
        n = 0
        for j, k in enumerate(c["kinds"]):
            if k == "i":
                args[j] = ints[n] & MASK
                n += 1
    c["args"] = args
    return c


def set_edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        for flags in (0, 0x40, 0x46, 0xFFFFFFFF):
            for ints in ((0, 0), (1, 0), (0, 5), (-1, -1), (3, 1)):
                yield set_case(op, r, flags, ints)
    return gen


def set_random(op):
    return lambda r: set_case(op, r)


def toggle_case(r, draw=None):
    c = base(r, "toggle")
    if draw is not None:
        c["draw"] = draw
    return c


def toggle_edges():
    r = random.Random(77)
    for d in (0, 1, 2, 0xFFFFFFFF, 0x80000000):
        yield toggle_case(r, d)


def correct_case(r, k=None, start=None, nullstart=False):
    c = base(r, "correct")
    n = r.randrange(1, 6) if k is None else k
    c["nodes"] = [node(r) for _ in range(n + 2)]
    for i in range(n + 2):
        c["nodes"][i]["prev"] = i + 1 if i + 1 < n + 2 and r.random() < 0.9 else None
        c["nodes"][i]["next"] = r.choice([None, r.randrange(0, n + 2)])
    c["start"] = None if nullstart else (r.randrange(0, n + 2) if start is None else start)
    return c


def correct_edges():
    r = random.Random(99)
    yield correct_case(r, nullstart=True)
    for k in (1, 2, 3):
        for st in range(k):
            yield correct_case(r, k, st)


TESTS = [
    FnTest("SetLoftPylon", ppc, host, edge=set_edges("set_loft"), rnd=set_random("set_loft")),
    FnTest("LayoutSetLoftPylon", ppc, host, edge=set_edges("lset_loft"), rnd=set_random("lset_loft")),
    FnTest("SetRotatePylon", ppc, host, edge=set_edges("set_rot"), rnd=set_random("set_rot")),
    FnTest("LayoutSetRotatePylon", ppc, host, edge=set_edges("lset_rot"), rnd=set_random("lset_rot")),
    FnTest("SetWobblePylon", ppc, host, edge=set_edges("set_wob"), rnd=set_random("set_wob")),
    FnTest("LayoutSetWobblePylon", ppc, host, edge=set_edges("lset_wob"), rnd=set_random("lset_wob")),
    FnTest("ToggleDrawUpSpline", ppc, host, edge=toggle_edges, rnd=lambda r: toggle_case(r)),
    FnTest("CorrectBraceNumbering", ppc, host, edge=correct_edges, rnd=lambda r: correct_case(r)),
]
