"""Unit v5269, the coaster queues and the script commands on a coaster, against libtpw_unit_AllocateQueue.dylib
(tests/unit_AllocateQueue.cpp), main app ("SimTheme Park"), src/AllocateQueue.cpp:
  AllocateQueue 0x10049180, CoasterAddPeep 0x10049288, HACKCoasterReturnLostPeep 0x10049324, CoasterGetQueueSpace 0x1004944c,
  CoasterGetPeep 0x100494c0, CoasterSetRepairState 0x10049588, CoasterSetSpeed 0x10049630, CoasterSetOpenClosedState 0x100496c0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Callees outside the unit (MReleaseAllocF,
SetCoasterState, the 3-argument CoasterSetSpeed) are fakes on both sides recording their arguments (the instance as a label).

The world of a case: the object table slot of the coaster number, one instance (flags, state, the word at +0x30, the two queues with their
blocks), the limit global and the action recorder's recording word. The host's instance has a different layout (native pointers): the
module maps the Mac offsets to the host's. The queues' read and write places are compared relative to the block. What is compared: the
answer (the functions that return one), the log, the instance and queue words, the blocks, the globals.

No fused operations. Not tested: a queue capacity of 0 with a push (the guard fails first), a coaster number without an instance (the
original reads it unconditionally except in CoasterSetSpeed, which has a NULL case), a place outside its block.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AllocateQueue"
MASK = 0xFFFFFFFF

F_ALLOC, F_ADD, F_HACK, F_SPACE, F_GET, F_REPAIR, F_SPEED, F_OPEN = (0x10049180, 0x10049288, 0x10049324, 0x1004944C, 0x100494C0,
                                                                    0x10049588, 0x10049630, 0x100496C0)
SIZES = {F_ALLOC: 112, F_ADD: 116, F_HACK: 224, F_SPACE: 72, F_GET: 160, F_REPAIR: 120, F_SPEED: 100, F_OPEN: 100}
OWN = set(SIZES)

CALLEE_ALLOC = 0x10064BD4
CALLEE_STATE = 0x10042BB8
CALLEE_SPEED = 0x1004B94C

ADDR = dict(table=0x102B6F04, lim=0x1029CE24, rec=0x1037EC44 + 0x24)
MAC_INST_SIZE = 0x200


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("MReleaseAllocF", (size, e.r(4) & MASK))
    e.set_r(3, e.alloc(max(size, 4)))


def stub_state(e):
    L = e.cl
    L.rec("SetCoasterState", (L.lab(e.r(3)), e.r(4) & MASK))


def stub_speed(e):
    L = e.cl
    L.rec("CoasterSetSpeed", (L.lab(e.r(3)), e.r(4) & MASK, s32(e.r(5))))


STUBS.internal(CALLEE_ALLOC, stub_alloc)
STUBS.internal(CALLEE_STATE, stub_state)
STUBS.internal(CALLEE_SPEED, stub_speed)


def setup(runner):
    T.install_std_stubs(runner.emu)


HL = {}      # the host layout, read once


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def mkinst(self):
        self.inst = self.alloc(self.inst_size)

    def tab(self, idx):
        return self.g("table") + idx * self.psz


class Ppc(Side):
    fmt = ">I"
    psz = 4
    inst_size = MAC_INST_SIZE

    def __init__(self, e, case):
        self.e = e
        self.mkinst()
        self.L = e.cl = CL.PpcLog({self.inst: 1}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rdptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def f(self, name):
        return self.inst + {"flags": 0, "state": 8, "f30": 0x30}[name]

    def q(self, qi, m):
        return self.inst + 0xB0 + 0x20 * qi + m

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.inst_size = HL["size"] + 64
        self.mkinst()
        CL.host_begin(lib, {1: self.inst}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_aq_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_aq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rdptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def f(self, name):
        return self.inst + HL[name]

    def q(self, qi, m):
        base = self.inst + (HL["qb"] if qi == 0 else HL["qd"])
        if m == 0:
            return base
        if m == 4:
            return base + HL["blk"]
        return base + HL["f08"] + (m - 8)

    def call(self, fn, args):
        return getattr(self.lib, "tpw_aq_" + fn)(*args) & MASK


FN_NAME = {F_ALLOC: "alloc", F_ADD: "add", F_HACK: "hack", F_SPACE: "space", F_GET: "get", F_REPAIR: "repair", F_SPEED: "speed", F_OPEN: "open"}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_aq_addr.restype, lib.tpw_aq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_aq_alloc.restype, lib.tpw_aq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_aq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_aq_alloc_queue.restype, lib.tpw_aq_alloc_queue.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_aq_add.restype, lib.tpw_aq_add.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32]
    lib.tpw_aq_hack.restype, lib.tpw_aq_hack.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    for n in ("space", "get"):
        getattr(lib, "tpw_aq_" + n).restype, getattr(lib, "tpw_aq_" + n).argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_aq_repair.restype, lib.tpw_aq_repair.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32]
    lib.tpw_aq_speed.restype, lib.tpw_aq_speed.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32]
    lib.tpw_aq_open.restype, lib.tpw_aq_open.argtypes = C.c_uint32, [C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_aq_layout(out)
    v = list(out)[:n]
    HL.update(flags=v[0], state=v[1], f30=v[2], qb=v[3], qd=v[4], size=v[5], blk=v[6], f08=v[7], qsize=v[8])
    assert HL["flags"] == 0 and HL["qd"] - HL["qb"] == HL["qsize"] and HL["blk"] == 8 and HL["f08"] == 16 and HL["qsize"] == 0x28, v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

FIELDS = ("flags", "state", "f30")


def build_queue(s, qi, spec):
    """spec: cap, f08, c1, c2, hasblock, rd (index or None), wr (index), f1c ('zero', 'rd', or a value), data (block words)."""
    if spec is None:
        return
    cap = spec["cap"]
    blk = s.alloc(max(4 * cap, 4) + 16) if spec["hasblock"] else 0
    if blk:
        s.words(blk, spec["data"])
    s.words(s.q(qi, 0), [cap])
    s.ptr(s.q(qi, 4), blk)
    rd = 0 if spec["rd"] is None else blk + 4 * spec["rd"]
    wr = 0 if spec["wr"] is None else blk + 4 * spec["wr"]
    f1c = {"zero": 0, "rd": rd}.get(spec["f1c"], spec["f1c"]) if isinstance(spec["f1c"], str) else (blk + spec["f1c"]) & MASK
    s.words(s.q(qi, 8), [spec["f08"], spec["c1"], spec["c2"], rd, wr, f1c])


def build(s, case):
    s.ptr(s.tab(case["idx"]), 0 if case.get("null") else s.inst)
    s.words(s.g("lim"), [case["lim"]])
    s.words(s.g("rec"), [case["rec"]])
    for name, v in zip(FIELDS, case["fields"]):
        s.words(s.f(name), [v])
    for qi in (0, 1):
        build_queue(s, qi, case["q"][qi])


def rel(p, blk):
    return None if p == 0 else (p - blk) & MASK


def observe_queue(s, qi, with_block=True):
    cap = s.rd_word(s.q(qi, 0))
    blk = s.rdptr(s.q(qi, 4))
    f08, c1, c2, rd, wr, f1c = s.rd_words(s.q(qi, 8), 6)
    out = {"cap": cap, "hasblk": blk != 0, "w": [f08, c1, c2], "rd": rel(rd, blk), "wr": rel(wr, blk), "f1c": rel(f1c, blk)}
    if blk and cap < 64 and with_block:     # AllocateQueue's new block is uninitialised memory: not compared
        out["blk"] = s.rd_words(blk, cap + 4)
    return out


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "lim": s.rd_word(s.g("lim")), "rec": s.rd_word(s.g("rec"))}
    for name in FIELDS:
        out[name] = s.rd_word(s.f(name))
    for qi in (0, 1):
        out["q%d" % qi] = observe_queue(s, qi, case["op"] != "alloc")
    return out


def execute(s, case):
    op = case["op"]
    a = case["args"]
    if op == "alloc":
        return s.call(F_ALLOC if isinstance(s, Ppc) else "alloc_queue", [s.q(case["qi"], 0)] + a)
    if op == "hack":
        return s.call(F_HACK if isinstance(s, Ppc) else "hack", [s.inst] + a)
    fn = {"add": F_ADD, "space": F_SPACE, "get": F_GET, "repair": F_REPAIR, "speed": F_SPEED, "open": F_OPEN}[op]
    return s.call(fn if isinstance(s, Ppc) else op, [case["idx"]] + a)


NO_ANSWER = ("add", "hack", "alloc", "repair", "speed", "open")


def run(s, case):
    try:
        build(s, case)
        ans = execute(s, case)
        if case["op"] in NO_ANSWER:
            ans = None
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def qspec(r, cap=None, tot=None, c1=None, wr=None, rd="r", f1c="zero", f08=None, hasblock=True):
    cap = r.randrange(1, 9) if cap is None else cap
    if tot is None:
        tot = r.randrange(0, cap + 3)
    if c1 is None:
        c1 = r.randrange(0, tot + 1)
    return {"cap": cap, "f08": r.randrange(0, 20) if f08 is None else f08, "c1": c1, "c2": (tot - c1) & MASK, "hasblock": hasblock,
            "rd": r.randrange(0, cap) if rd == "r" else rd, "wr": r.randrange(0, cap) if wr is None else wr, "f1c": f1c,
            "data": [r.getrandbits(32) for _ in range(cap + 4)]}


def base(r, op):
    return {"op": op, "queues": {}, "idx": r.choice([0, 1, 7, 0x40, 0xFF, r.randrange(256)]), "lim": r.choice([0, 1, 5, 100, MASK, 0x80000000, r.getrandbits(32)]),
            "rec": r.choice([0, 1, MASK, r.getrandbits(32)]), "fields": [r.getrandbits(32) for _ in range(3)], "q": [None, None], "args": []}


def inst_state(r):
    return r.choice([0, 2, 4, 6, 8, 0xA, 0xC, 0xE, 0x40, 0x10, r.getrandbits(8), r.getrandbits(32)])


# ---- AllocateQueue
def alloc_case(r, cap=None):
    c = base(r, "alloc")
    c["qi"] = r.choice([0, 1])
    c["args"] = [r.choice([0, 1, 2, 5, 16, r.randrange(0, 40)]) if cap is None else cap]
    junk = qspec(r)
    junk["f1c"] = r.getrandbits(8)
    junk["hasblock"] = r.choice([True, False])
    c["q"][c["qi"]] = junk
    return c


def alloc_edges():
    r = random.Random(11)
    for cap in (0, 1, 2, 3, 16, 33):
        yield alloc_case(r, cap)


# ---- AddPeep / Hack
def push_case(r, op, cap=None, tot=None, wr=None, tot2=None):
    c = base(r, op)
    for qi in (0, 1):
        c["q"][qi] = qspec(r, cap=cap if qi == 0 or op == "add" else None, tot=tot if qi == 0 else tot2, wr=wr)
    c["args"] = [r.choice([0, 1, MASK, r.getrandbits(32)])]
    return c


def push_edges(op):
    def gen():
        r = random.Random(21 if op == "add" else 22)
        for cap in (1, 2, 5):
            for dt in (-2, -1, 0, 1):
                for wr in (0, cap - 1, cap // 2):
                    yield push_case(r, op, cap=cap, tot=max(0, cap + dt), wr=wr, tot2=r.choice([0, 3, 100]))
        if op == "hack":
            for cap in (1, 3):
                for t1 in (cap, cap + 1):
                    for dt in (-1, 0):
                        c = push_case(r, op, cap=cap, tot=t1, wr=cap - 1)
                        c["q"][1] = qspec(r, cap=cap, tot=max(0, cap + dt), wr=cap - 1)
                        yield c
    return gen


# ---- GetQueueSpace
def space_case(r, lim=None, f08=None, tot=None):
    c = base(r, "space")
    c["q"][0] = qspec(r, cap=1, tot=tot, f08=f08, hasblock=False)
    if lim is not None:
        c["lim"] = lim
    return c


def space_edges():
    r = random.Random(31)
    for f08, tot in ((10, 3), (3, 10), (0, 0), (5, 5)):
        for lim in (0, 6, 7, 8, 100, MASK, 0x80000000, 0x7FFFFFFF):
            yield space_case(r, lim, f08, tot)


# ---- GetPeep
def get_case(r, kind=None):
    c = base(r, "get")
    kind = kind or r.choice(["normal", "normal", "wrap", "empty", "norun", "f1c"])
    cap = r.randrange(1, 9)
    if kind == "empty":
        spec = qspec(r, cap=cap, c1=0, tot=0, f08=0)
    elif kind == "norun":
        spec = qspec(r, cap=cap, c1=r.randrange(1, cap + 2), tot=cap, rd=None)
    elif kind == "f1c":
        spec = qspec(r, cap=cap, c1=r.randrange(1, cap + 2), tot=cap, f1c="rd")
    elif kind == "wrap":
        spec = qspec(r, cap=cap, c1=r.randrange(1, cap + 2), tot=cap, rd=cap - 1)
    else:
        spec = qspec(r, cap=cap, c1=r.randrange(1, cap + 2), tot=cap)
    if r.random() < 0.2 and spec["rd"] is not None:
        spec["f1c"] = r.getrandbits(4) * 4
    c["q"][1] = spec
    c["q"][0] = qspec(r)
    return c


def get_edges():
    r = random.Random(41)
    for kind in ("normal", "wrap", "empty", "norun", "f1c"):
        for _ in range(3):
            yield get_case(r, kind)


# ---- the commands on the state
def repair_case(r, state=None):
    c = base(r, "repair")
    c["args"] = [r.choice([0, 1, 2, 3, MASK, MASK - 1, 0x80000000, 0x7FFFFFFF, 4, 5, r.getrandbits(32)]) if state is None else state & MASK]
    c["fields"][1] = inst_state(r)
    return c


def repair_edges():
    r = random.Random(51)
    for st in (0, 1, 2, 3, 4, MASK, 0x80000000, 0x7FFFFFFF):
        yield repair_case(r, st)


def open_case(r, state=None):
    c = base(r, "open")
    c["args"] = [r.choice([0, 1, -1, 5, 0x80000000, r.getrandbits(32)]) if state is None else state]
    c["fields"][1] = inst_state(r)
    return c


def open_edges():
    r = random.Random(61)
    for arg in (0, 1, -1):
        for st in (0, 2, 4, 6, 8, 0xA, 0xE, 1, 0xFFFFFFF1):
            c = open_case(r, arg)
            c["fields"][1] = st
            yield c


def speed_case(r):
    c = base(r, "speed")
    c["null"] = r.random() < 0.15
    c["args"] = [r.choice([0, 1, 0x3C, MASK, r.getrandbits(32)])]
    if r.random() < 0.3:
        c["fields"][2] = c["args"][0]
    c["fields"][0] = r.choice([0, 0x8000, 0x7FFF, 0xFFFF8000, r.getrandbits(32)])
    return c


def speed_edges():
    r = random.Random(71)
    for null in (False, True):
        for flags in (0, 0x8000, 0x7FFF):
            for rec in (0, 1, MASK):
                for same in (False, True):
                    c = speed_case(r)
                    c["null"], c["fields"][0], c["rec"] = null, flags, rec
                    c["fields"][2] = c["args"][0] if same else (c["args"][0] ^ 1)
                    yield c


def mk_random(fn):
    return lambda r: fn(r)


TESTS = [
    FnTest("AllocateQueue", ppc, host, edge=alloc_edges, rnd=lambda r: alloc_case(r)),
    FnTest("CoasterAddPeep", ppc, host, edge=push_edges("add"), rnd=lambda r: push_case(r, "add")),
    FnTest("HACKCoasterReturnLostPeep", ppc, host, edge=push_edges("hack"), rnd=lambda r: push_case(r, "hack")),
    FnTest("CoasterGetQueueSpace", ppc, host, edge=space_edges, rnd=lambda r: space_case(r)),
    FnTest("CoasterGetPeep", ppc, host, edge=get_edges, rnd=lambda r: get_case(r)),
    FnTest("CoasterSetRepairState", ppc, host, edge=repair_edges, rnd=lambda r: repair_case(r)),
    FnTest("CoasterSetSpeed", ppc, host, edge=speed_edges, rnd=lambda r: speed_case(r)),
    FnTest("CoasterSetOpenClosedState", ppc, host, edge=open_edges, rnd=lambda r: open_case(r)),
]
