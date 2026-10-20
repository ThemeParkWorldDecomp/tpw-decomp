"""Unit v5139, the bump ride's peep lists and BMP_LeaveBump, against libtpw_unit_BMP_PollForPeeps.dylib (tests/unit_BMP_PollForPeeps.cpp), main app
("SimTheme Park"), src/BMP_REMOVEBUMP_BMP_PollForPeeps.cpp:
  BMP_PollForPeeps 0x1002e198, BMP_AddPeepToRideLeavingList 0x1002e268, BMP_AddPeepToRideJoiningList 0x1002e070, BMP_LeaveBump 0x1002e370

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (TRACE, RemovePeepsFromBump, FlushDeferredAnim, SetCurRideAnim) are fakes on both sides that record their arguments.

The world of a case: the list's magic, the free list, the ride array (word 0 id part, +0x50, +0xc4 joining list, +0xc8 leaving list), the
peep nodes; for LeaveBump a bump, its owner and the ride anim set table. What is compared: the answer, the list head, the whole ride array and
the nodes (node pointers as node numbers), the bump, the owner, the event log.

No fused operations. Not tested: a wrong magic with ride 0 in the two Add functions (the original reads through a null record and crashes).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_PollForPeeps"
MASK = 0xFFFFFFFF

FN_POLL, FN_LEAVING, FN_JOINING, FN_LEAVE = 0x1002E198, 0x1002E268, 0x1002E070, 0x1002E370
SIZES = {FN_POLL: 168, FN_LEAVING: 208, FN_JOINING: 240, FN_LEAVE: 200}
OWN = set(SIZES)

TRACE = 0x1006FCFC
REMOVE = 0x1002E464
FLUSH = 0x100C6604
SETCUR = 0x100C6670

ADDR = dict(list=0x102B56C0, sets=0x102C79A4)
HOST_LAYOUT = [0x34, 0xA8]

NNODES = 10
NSETS = 16
MAGIC = 0x4A454647
NRIDES = 256


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(TRACE, CL.recorder("TRACE", "s"))
STUBS.internal(REMOVE, CL.recorder("RemovePeepsFromBump", "Li", ret="i"))
STUBS.internal(FLUSH, CL.recorder("FlushDeferredAnim", "Lu"))
STUBS.internal(SETCUR, CL.recorder("SetCurRideAnim", "Luuufu"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def setptr(self, i, a):
        self.write(self.g("sets") + i * self.psz, struct.pack(self.pfmt, a))

    def objects(self):
        self.bump = self.alloc(0xAC)
        self.owner = self.alloc(0x60)
        self.sets = [self.alloc(0x20) for _ in range(NSETS)]
        self.pool = self.alloc(0x14 * NNODES)
        self.rides = self.alloc(0xD0 * NRIDES)

    def enc(self, a):
        """A pointer word: the Mac's address; on the host the offset from the arena base (the low 32 bits)."""
        return a & MASK

    def norm(self, w):
        p = self.enc(self.pool)
        if p <= w < p + 0x14 * NNODES:
            return ("n", (w - p) // 0x14, (w - p) % 0x14)
        return w

    def nw(self, a, n):
        return [self.norm(w) for w in self.rd_words(a, n)]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4

    def __init__(self, e, case):
        self.e = e
        self.objects()
        fixed = {self.bump: 1}
        for i, a in enumerate(self.sets):
            fixed[a] = 2 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call(self, op, ride, peep):
        entry = {"poll": FN_POLL, "leaving": FN_LEAVING, "joining": FN_JOINING, "leave": FN_LEAVE}[op]
        args = [self.bump] if op == "leave" else [ride & MASK, peep & MASK]
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objects()
        fixed = {1: self.bump}
        for i, a in enumerate(self.sets):
            fixed[2 + i] = a
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def enc(self, a):
        return (a - self.lib.tpw_pfp_base()) & MASK

    def g(self, name):
        return self.lib.tpw_pfp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pfp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, ride, peep):
        L = self.lib
        if op == "poll":
            return L.tpw_pfp_poll(s32(ride)) & MASK
        if op == "leaving":
            L.tpw_pfp_leaving(s32(ride), peep & MASK)
            return 0
        if op == "joining":
            return L.tpw_pfp_joining(s32(ride), peep & MASK) & MASK
        L.tpw_pfp_leave(self.bump)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_pfp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype, getattr(lib, p + "base").argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "poll").restype, getattr(lib, p + "poll").argtypes = C.c_uint32, [C.c_int32]
    getattr(lib, p + "leaving").restype, getattr(lib, p + "leaving").argtypes = None, [C.c_int32, C.c_uint32]
    getattr(lib, p + "joining").restype, getattr(lib, p + "joining").argtypes = C.c_uint32, [C.c_int32, C.c_uint32]
    getattr(lib, p + "leave").restype, getattr(lib, p + "leave").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def chain(s, ids):
    """Links the pool nodes ids (next at +0x10); answers the head address or 0."""
    addr = [s.enc(s.pool + 0x14 * i) for i in ids]
    for k, i in enumerate(ids):
        nxt = addr[k + 1] if k + 1 < len(ids) else 0
        s.words(s.pool + 0x14 * i, [case_peep(i), 0x300 + i, 0x400 + i, 0x500 + i, nxt])
    return addr[0] if addr else 0


def case_peep(i):
    return 0x1000 + 7 * i


def build(s, case):
    g = s.g("list")
    if case["op"] == "leave":
        for i, a in enumerate(s.sets):
            s.setptr(i, a)
        for i in range(NSETS, 64):
            s.setptr(i, 0)
        s.words(s.bump, [case["flags"]])
        s.words(s.bump + 8, [case["idx"]])
        s.words(s.bump + 0x10, [case["state"]])
        s.words(s.bump + 0x9C, [s.enc(s.owner)])
        s.words(s.owner + 0x50, [case["ownerw"]])
        return
    ids = list(range(NNODES))
    nf, nl, nj = case["nfree"], case["nleft"], case["njoin"]
    free, left, join = ids[:nf], ids[nf:nf + nl], ids[nf + nl:nf + nl + nj]
    s.words(g, [MAGIC if case["magic"] else case["badmagic"]])
    s.words(g + 8, [s.enc(s.rides)])
    s.words(g + 0x34, [chain(s, free)])
    idx = case["idx"]
    for j, hi in ((idx, case["hi"]), (idx + 1, case["hi2"]), (idx - 1, case["hi2"] ^ 5)):
        if 0 <= j < NRIDES:
            s.words(s.rides + 0xD0 * j, [hi])
    rec = s.rides + 0xD0 * idx
    s.words(rec + 0x50, [case["flag"]])
    s.words(rec + 0xC4, [chain(s, join)])
    s.words(rec + 0xC8, [chain(s, left)])


def execute(s, case):
    r = s.call(case["op"], case["ride"], case["peep"])
    return r if case["op"] in ("poll", "joining") else 0      # the void functions leave r3 as whatever it was


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    if case["op"] == "leave":
        out["bump"] = s.rd_words(s.bump, 0xAC // 4)
        out["bump"][0x9C // 4] = "owner" if out["bump"][0x9C // 4] == s.enc(s.owner) else out["bump"][0x9C // 4]
        out["owner"] = s.rd_words(s.owner, 0x60 // 4)
    else:
        g = s.g("list")
        out["head"] = s.norm(s.rd_word(g + 0x34))
        out["rides"] = s.nw(s.rides, 0xD0 * NRIDES // 4)
        out["nodes"] = s.nw(s.pool, 5 * NNODES)
    return out


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


def base(r, op):
    idx = r.choice([0, 1, 5, 254, 255, r.randrange(256)])
    hi = r.choice([0, 1, 0xFFFFFF, r.randrange(0x1000000)])
    ride = ((hi << 8) | idx) & MASK
    kind = r.choice(["ok", "ok", "ok", "ok", "hi", "idx", "magic"])
    magic = kind != "magic"
    if kind == "hi":
        ride = (((hi ^ r.randrange(1, 0xFFFFFF)) << 8) | idx) & MASK
    if kind == "idx":
        ride = ((hi << 8) | ((idx + r.randrange(1, 255)) & 0xFF)) & MASK
    if not magic and ride == 0:
        ride = 0x100
    nfree = r.choice([0, 1, 2, 3])
    nleft = r.choice([0, 1, 2, 3])
    njoin = r.choice([0, 1, 2])
    return {
        "op": op, "magic": magic, "badmagic": r.choice([0, 1, 0x4A454646, 0xFFFFFFFF]),
        "idx": idx, "hi": hi, "hi2": r.randrange(0x1000000), "flag": r.choice([0, 1, 2, 0x80000000, -1 & MASK]),
        "ride": ride, "peep": r.choice([0, 1, 0xFFFFFFFF, r.randrange(1 << 32)]),
        "nfree": nfree, "nleft": nleft, "njoin": njoin,
        "queues": {},
    }


def list_edges(op):
    r = random.Random(op.__hash__() & 0xFFFF if False else {"poll": 11, "leaving": 12, "joining": 13}[op])
    for kind in ("ok", "hi", "idx", "magic"):
        for nfree in (0, 1, 3):
            for nleft in (0, 1, 3):
                for flag in (0, 1):
                    c = base(r, op)
                    idx, hi = c["idx"], c["hi"]
                    c["ride"] = ((hi << 8) | idx) & MASK
                    c["magic"] = kind != "magic"
                    if kind == "hi":
                        c["ride"] = (((hi ^ 1) << 8) | idx) & MASK
                    if kind == "idx":
                        c["ride"] = ((hi << 8) | ((idx + 1) & 0xFF)) & MASK
                    if kind == "magic" and c["ride"] == 0:
                        c["ride"] = 0x100
                    c["nfree"], c["nleft"], c["flag"] = nfree, nleft, flag
                    c["njoin"] = r.choice([0, 1])
                    yield c


def list_random(op):
    return lambda r: base(r, op)


def leave_case(r, **kw):
    c = {"op": "leave", "ride": 0, "peep": 0,
         "flags": r.choice([0, MASK, 0x7E, 0x81, r.randrange(1 << 32)]),
         "idx": r.choice([0, 1, 5, 15, r.randrange(NSETS)]),
         "state": r.choice([0xC, 0xC, 0, 1, 11, 13, r.randrange(1 << 32)]),
         "ownerw": r.choice([0, 1, 7, 0x80000000]),
         "queues": {"RemovePeepsFromBump": [r.choice([0, 0, 1, 2, -1 & MASK])]}}
    c.update(kw)
    return c


def leave_edges():
    r = random.Random(14)
    for rem in (0, 1):
        for idx in (0, 1, 15):
            for state in (0xC, 0, 0xB, 0xD):
                yield leave_case(r, idx=idx, state=state, queues={"RemovePeepsFromBump": [rem]})


def leave_random(r):
    return leave_case(r)


TESTS = [
    FnTest("BMP_PollForPeeps", ppc, host, edge=lambda: list_edges("poll"), rnd=list_random("poll")),
    FnTest("BMP_AddPeepToRideLeavingList", ppc, host, edge=lambda: list_edges("leaving"), rnd=list_random("leaving")),
    FnTest("BMP_AddPeepToRideJoiningList", ppc, host, edge=lambda: list_edges("joining"), rnd=list_random("joining")),
    FnTest("BMP_LeaveBump", ppc, host, edge=leave_edges, rnd=leave_random),
]
