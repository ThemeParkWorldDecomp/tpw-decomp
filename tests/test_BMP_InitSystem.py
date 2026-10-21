"""Unit v5659, the advisor's bump world, against libtpw_unit_BMP_InitSystem.dylib (tests/unit_BMP_InitSystem.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_InitSystem 0x10025a94, BMP_CloseSystem 0x10025cb0, BMP_FreeAll 0x10025db8, BMP_OpenRide 0x10025ee4, BMP_CloseRide 0x10026118

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, pointer fields as arena offsets) for the translation. The callees outside
the unit are fakes on both sides that record their arguments: TRACE (text and number), SetBuoySequence (ride index and state), the ride
performance, BMP_DestroyRide, GlobalAlloc (hands out blocks stamped with their size at +4, or NULL from the case's queue), GlobalFree (logs the
stamp of its block), rand. sin and memset are the C library's on both sides.

The world of a case: the 15 words at 0x102b56c0 (magic live or not), a ride array of 64 records, a bump array, a table of links, the other
blocks, each stamped. What is compared: the log, the sine table, the world's words (a block pointer as its stamp), the ride, bump and link
records and the 1024-record chain (pointers as indexes). No fused operations. Not tested: a live world with a NULL ride array or table, a ride id
of 0 on a world that is not live (the original reads through NULL), a ride count above the live rides (the walk runs off the array), an
InitSystem whose table allocation fails (the original writes through NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_InitSystem"
MASK = 0xFFFFFFFF
MAGIC = 0x4A454647

FN_INIT, FN_CLOSE_SYSTEM, FN_FREE_ALL, FN_OPEN, FN_CLOSE_RIDE = 0x10025A94, 0x10025CB0, 0x10025DB8, 0x10025EE4, 0x10026118
SIZES = {FN_INIT: 500, FN_CLOSE_SYSTEM: 224, FN_FREE_ALL: 264, FN_OPEN: 528, FN_CLOSE_RIDE: 400}
OWN = set(SIZES)

TRACE, SETBUOY, PERF, DESTROY = 0x1006FCFC, 0x1002D390, 0x10026C98, 0x10027274
GALLOC, GFREE, RAND = 0x10239610, 0x10238B18, 0x10239148

ADDR = dict(world=0x102B56C0, sine=0x102B4EC0)
HOST_LAYOUT = [0xD0, 0xAC]
SLOTS = (8, 0x10, 0x18, 0x20, 0x28, 0x30)
NRIDES, NLINKS = 64, 8


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(TRACE, CL.recorder("TRACE", "si"))


def ride_index(e, ride):
    base = e.u32(ADDR["world"] + 8)
    return (ride - base) // 0xD0


def stub_setbuoy(e):
    r = e.r(3) & MASK
    e.cl.rec("SetBuoySequence", (ride_index(e, r), s32(e.u32(r + 0x50))))


def stub_perf(e):
    e.cl.rec("Perf", (s32(e.r(3)), s32(e.r(4))))


def stub_destroy(e):
    e.cl.rec("Destroy", (s32(e.r(3)),))


def stub_alloc(e):
    L = e.cl
    flags, size = e.r(3) & MASK, e.r(4) & MASK
    L.rec("GlobalAlloc", (flags, size))
    a = 0
    if L.ret("GlobalAlloc"):
        a = e.alloc(size)
        e.write(a, bytes(size))
        e.write(a + 4, struct.pack(">I", size))
    e.set_r(3, a)


def stub_free(e):
    a = e.r(3) & MASK
    e.cl.rec("GlobalFree", (e.u32(a + 4) if a else -1,))
    e.set_r(3, 0)


def stub_rand(e):
    e.cl.rec("rand", ())
    e.set_r(3, e.cl.ret("rand") & MASK)


def glue(fn):
    """A glue function saves the caller's TOC pointer at 0x14(sp); the caller reloads r2 from there after the call."""

    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)

    return wrapped


STUBS.internal(SETBUOY, stub_setbuoy)
STUBS.internal(PERF, stub_perf)
STUBS.internal(DESTROY, stub_destroy)
STUBS.internal(GALLOC, glue(stub_alloc))
STUBS.internal(GFREE, glue(stub_free))
STUBS.internal(RAND, glue(stub_rand))


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


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a):
        return a

    def unptr(self, w):
        return w

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        T.call_ppc(self.e, fn, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_bmp_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bmp_addr(name.encode())

    def ptr(self, a):
        return a - self.base

    def unptr(self, w):
        return w + self.base

    def alloc(self, n):
        return self.lib.tpw_bmp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        name = {FN_INIT: "init", FN_CLOSE_SYSTEM: "close_system", FN_FREE_ALL: "free_all", FN_OPEN: "open_ride", FN_CLOSE_RIDE: "close_ride"}[fn]
        getattr(self.lib, "tpw_bmp_" + name)(*[s32(a) for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bmp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("open_ride", "close_ride"):
        getattr(lib, p + n).argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("sine"), [0xA5A5A5A5] * 512)
    world = list(case["world"])
    if case["op"] == "init":
        s.words(s.g("world"), world)
        return
    blk = s.blk = {}
    blk["rides"] = s.alloc(NRIDES * 0xD0)
    nb = len(case["bumps"])
    blk["bumps"] = s.alloc(max(nb, 1) * 0xAC)
    blk["links"] = s.alloc(NLINKS * 0x14)
    blk["table"] = s.alloc(0x5000)
    for slot in (0x18, 0x20, 0x28):
        blk[slot] = s.alloc(16)

    def res(w):
        if isinstance(w, tuple):
            return 0 if w[1] is None else s.ptr(blk["rides" if w[0] == "R" else "links"] + w[1] * (0xD0 if w[0] == "R" else 0x14))
        return w

    for i, rec in enumerate(case["rides"]):
        s.words(blk["rides"] + i * 0xD0, [res(w) for w in rec])
    for i, rec in enumerate(case["links"]):
        s.words(blk["links"] + i * 0x14, [res(w) for w in rec])
    for i, rec in enumerate(case["bumps"]):
        s.words(blk["bumps"] + i * 0xAC, [res(w) for w in rec])
    s.words(blk["table"], [r for r in case["table"]])
    # the stamps GlobalFree logs (word 1 of each block; overwrites that word of record 0)
    stamps = {"rides": 0x51, "bumps": 0x52, 0x18: 0x53, 0x20: 0x54, 0x28: 0x55, "table": 0x56}
    for k, v in stamps.items():
        s.words(blk[k] + 4, [v])
    world[2] = s.ptr(blk["rides"])
    world[4] = s.ptr(blk["bumps"]) if 0x10 not in case["null"] else 0
    for slot in (0x18, 0x20, 0x28):
        world[slot // 4] = s.ptr(blk[slot]) if slot not in case["null"] else 0
    world[12] = s.ptr(blk["table"])
    s.words(s.g("world"), world)


def norm_ptr(s, w, base, stride, n):
    if w == 0:
        return None
    rel = w - s.ptr(base)
    if 0 <= rel < stride * n and rel % stride == 0:
        return rel // stride
    return w


def observe(s, case):
    world = s.rd_words(s.g("world"), 15)
    out = {"log": s.log(), "sine": s.rd_words(s.g("sine"), 512)}
    wn = []
    for i, w in enumerate(world):
        if i * 4 in SLOTS:
            wn.append(None if w == 0 else s.rd_word(s.unptr(w) + 4))
        elif i == 13:
            wn.append(None if w == 0 else (w - world[12]) & MASK)
        else:
            wn.append(w)
    out["world"] = wn
    if case["op"] == "init":
        tb, ptr_tb = (s.unptr(world[12]), world[12]) if world[12] else (None, 0)
    else:
        blk = s.blk
        tb, ptr_tb = blk["table"], s.ptr(blk["table"])
        rides = s.rd_words(blk["rides"], NRIDES * 52)
        for i in range(NRIDES):
            for j in (49, 50):
                rides[i * 52 + j] = norm_ptr(s, rides[i * 52 + j], blk["links"], 0x14, NLINKS)
        out["rides"] = rides
        links = s.rd_words(blk["links"], NLINKS * 5)
        for i in range(NLINKS):
            links[i * 5 + 4] = norm_ptr(s, links[i * 5 + 4], blk["links"], 0x14, NLINKS)
        out["links"] = links
        nb = len(case["bumps"])
        bumps = s.rd_words(blk["bumps"], max(nb, 1) * 43)
        for i in range(nb):
            bumps[i * 43 + 39] = norm_ptr(s, bumps[i * 43 + 39], blk["rides"], 0xD0, NRIDES)
        out["bumps"] = bumps
    if tb:
        t = s.rd_words(tb, 0x5000 // 4)
        for i in range(1024):
            w = t[i * 5 + 4]
            t[i * 5 + 4] = None if w == 0 else (w - ptr_tb) & MASK
        out["table"] = t
    return out


def execute(s, case):
    fn = {"init": FN_INIT, "close_system": FN_CLOSE_SYSTEM, "free_all": FN_FREE_ALL, "open": FN_OPEN, "close_ride": FN_CLOSE_RIDE}[case["op"]]
    s.call(fn, case.get("args", []))


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

KINDS = list(range(-16, 3))


def small(r):
    return r.randrange(0, 1000)


def gen_ride(r, live=None):
    w = [small(r) for _ in range(52)]
    w[0] = r.choice(KINDS) if (live if live is not None else r.random() < 0.3) else 0
    w[20] = r.choice([0, 0, 1, 2, 3, 5])
    for j in (49, 50):
        w[j] = ("L", r.choice([None, None] + list(range(NLINKS))))
    return w


def base(r, op, magic_ok=True):
    world = [MAGIC if magic_ok else r.choice([0, 0x4A454646, r.getrandbits(32)])] + [small(r) for _ in range(14)]
    world[1] = r.getrandbits(32)
    world[14] = r.getrandbits(32)
    c = {"op": op, "world": world, "null": set(), "rides": [gen_ride(r) for _ in range(NRIDES)], "args": []}
    c["links"] = [[small(r), small(r), small(r), small(r), ("L", r.choice([None] + list(range(i + 1, NLINKS))) if i + 1 < NLINKS else None)] for i in range(NLINKS)]
    c["table"] = [0 if i % 5 == 4 else small(r) for i in range(0x5000 // 4)]   # the link words start NULL (they are pointers)
    world[13] = 0
    nb = r.randrange(0, 8)
    c["bumps"] = []
    for _ in range(nb):
        b = [small(r) for _ in range(43)]
        b[0] = r.getrandbits(32)
        b[39] = ("R", r.choice([None, None] + list(range(NRIDES))))
        c["bumps"].append(b)
    live = sum(1 for b in c["bumps"] if b[0] & 1)
    world[5] = r.randrange(0, live + 1)
    world[3] = sum(1 for x in c["rides"] if x[0] != 0)
    if r.random() < 0.3:
        world[3] = r.randrange(0, world[3] + 1)
    c["queues"] = {"GlobalAlloc": [1] * 6, "rand": [r.getrandbits(31)]}
    return c


def case_init(r):
    c = base(r, "init", r.random() < 0.8)
    c["queues"]["GlobalAlloc"] = [r.choice([0, 1]) for _ in range(5)] + [1]
    return c


def case_close_system(r):
    c = base(r, "close_system", r.random() < 0.8)
    if r.random() < 0.3:
        c["null"] = set(r.sample([0x10, 0x18, 0x20, 0x28], r.randrange(1, 4)))
    return c


def case_free_all(r):
    return base(r, "free_all", r.random() < 0.85)


def ride_id(c, idx, r, exact=True):
    kind = c["rides"][idx][0]
    if exact or r.random() < 0.7:
        return (kind << 8) | idx
    return r.choice([(kind + 1) << 8 | idx, kind << 8 | ((idx + 1) & 0xFF), r.getrandbits(32), 1, -1])


def ride_case(r, op, idx=None, kind=None, state=None, magic_ok=True, exact=None):
    c = base(r, op, magic_ok)
    if idx is None:
        idx = r.randrange(NRIDES)
    if kind is not None:
        c["rides"][idx][0] = kind
    if state is not None:
        c["rides"][idx][20] = state
    for b in c["bumps"]:
        if r.random() < 0.6:
            b[39] = ("R", idx)
        b[0] |= r.choice([0, 1, 1])
    live = sum(1 for b in c["bumps"] if b[0] & 1)
    c["world"][5] = r.randrange(0, live + 1) if r.random() < 0.5 else live
    arg = ride_id(c, idx, r, True if exact is None else exact) & MASK
    if not magic_ok and arg == 0:
        arg = 1
    c["args"] = [arg]
    return c


def case_open(r):
    return ride_case(r, "open", magic_ok=r.random() < 0.85, state=r.choice([0, 0, 1, 2, 7]), exact=r.random() < 0.8)


def case_close_ride(r):
    return ride_case(r, "close_ride", magic_ok=r.random() < 0.85, state=r.choice([0, 1, 2, 7]), exact=r.random() < 0.8,
                     kind=r.choice(KINDS + [-14, -14]))


def edges(op):
    def gen():
        r = random.Random(sum(map(ord, op)))
        if op == "init":
            for ok in (True, False):
                for allocs in ([1] * 6, [0] * 5 + [1], [1, 0, 1, 0, 1, 1]):
                    c = case_init(r)
                    c["world"][0] = MAGIC if ok else 0
                    c["queues"]["GlobalAlloc"] = allocs
                    yield c
        elif op in ("close_system", "free_all"):
            for ok in (True, False):
                for n in range(0, 4):
                    c = base(r, op, ok)
                    for x in c["rides"]:
                        x[0] = 0
                    for i in range(n):
                        c["rides"][r.randrange(NRIDES)][0] = r.choice(KINDS)
                    c["world"][3] = sum(1 for x in c["rides"] if x[0] != 0)
                    yield c
        else:
            for kind in KINDS:
                for state in (0, 1, 2):
                    for idx in (0, 5, 63):
                        yield ride_case(r, op, idx=idx, kind=kind, state=state)
            for exact in (False, False, False):
                yield ride_case(r, op, exact=False, kind=-14, state=1)
            yield ride_case(r, op, magic_ok=False, kind=-14, state=1)
    return gen


TESTS = [
    FnTest("BMP_InitSystem", ppc, host, edge=edges("init"), rnd=case_init),
    FnTest("BMP_CloseSystem", ppc, host, edge=edges("close_system"), rnd=case_close_system),
    FnTest("BMP_FreeAll", ppc, host, edge=edges("free_all"), rnd=case_free_all),
    FnTest("BMP_OpenRide", ppc, host, edge=edges("open"), rnd=case_open),
    FnTest("BMP_CloseRide", ppc, host, edge=edges("close_ride"), rnd=case_close_ride),
]
