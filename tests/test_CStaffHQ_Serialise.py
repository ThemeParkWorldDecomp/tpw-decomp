"""Unit v5918, the staff headquarters: Serialise 0x1012ac08, ReceiveMessage 0x1012a9e8, UpdateMonthly 0x1012ae40 and
FindNearestAvailableGuard 0x1012be78, against libtpw_unit_CStaffHQ_Serialise.dylib (tests/unit_CStaffHQ_Serialise.cpp), main app
("SimTheme Park"), src/game/buildings/StaffHQ.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (CThing and CTrainingCentre Serialise, the SAVEVAR and LOADVAR instances, CGuard IsAvailable and ProsecutePerp, the map's
GetRegionalEffectCell, the world's IsParkClosed and GetParkAnalyser, the analyser's GetVisitorsInPark, the headquarters' IsStriking,
StopStrikeInProgress and ConsiderStriking, CTRACE, GAME_ASSERTMSG and the message's GetType) are fakes on both sides that record their
arguments (the value pointers of the SAVEVARs as offsets into the headquarters) and answer from the case's queues.

The world of a case: the headquarters block (strike words, picked-up halfword, forced flag, cell bytes), the world block (turn and the five
staff list heads), the thing table rows (guards with their cell bytes and next links, the perp), the seven halfwords at 0x102a11bc..dc, the
region cell and the message. What is compared: the answer, the event log and the headquarters block.

No fused operations. labs is the C library's on both sides. Not tested: a NULL thing table row for a guard or a perp (the original reads it
unconditionally).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CStaffHQ_Serialise"
MASK = 0xFFFFFFFF

FN_SER, FN_RECV, FN_MONTH, FN_FIND = 0x1012ac08, 0x1012a9e8, 0x1012ae40, 0x1012be78
SIZES = {FN_SER: 520, FN_RECV: 492, FN_MONTH: 532, FN_FIND: 368}
OWN = set(SIZES)

ADDR = dict(gWorld=0x1037ec0c, gWorldMap=0x1037ec08, arr=0x1034cbfc, noguard=0x102a11bc, s0=0x102a11be, s1=0x102a11c0, s2=0x102a11c2,
            s3=0x102a11c4, s4=0x102a11c6, gend=0x102a11dc)
HOST_LAYOUT = [0x1da70c, 0x1da73c, 0x1da744, 0x68, 0x24, 0x60, 0x20c, 1, 1, 4, 6]

ANALYSER = 0x50000200
MAP = 0x50000300
FILE = 0x50000400
STUB_VT = 0x1012a998            # CStaffHQ::FinalDestroy: never called here, so its address stands for the message's GetType
HEADS = (0x1da73c, 0x1da73e, 0x1da740, 0x1da742, 0x1da744)
WORLD = {}
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def var_stub(name):
    inner = CL.recorder(name, "uxsi", ret="i")

    def fn(e):
        e.cl.rec("off:" + name, (s32(e.r(4) - CUR["hq"]),))
        inner(e)
    return fn


for _addr, _name in ((0x1000f4b0, "SAVEVAR<int>"), (0x1000ef18, "LOADVAR<int>"), (0x1000e9d4, "SAVEVARSTRUCT<tag>"),
                     (0x1000eb4c, "LOADVARSTRUCT<tag>"), (0x1012c3d4, "SAVEVARSTRUCT<Strike>"), (0x1012c27c, "LOADVARSTRUCT<Strike>")):
    STUBS.internal(_addr, var_stub(_name))
STUBS.internal(0x1012e010, CL.recorder("CThing::Serialise", "Luiiu", ret="i"))
STUBS.internal(0x10130008, CL.recorder("CTrainingCentre::Serialise", "Luiiu", ret="i"))
STUBS.internal(0x1013018c, CL.recorder("CTrainingCentre::UpdateMonthly", "L"))
STUBS.internal(0x1013ff7c, CL.recorder("IsParkClosed", "L", ret="i"))
STUBS.internal(0x1013ebc8, CL.recorder("GetParkAnalyser", "L", ret=lambda e, log: ANALYSER))
STUBS.internal(0x100e5414, CL.recorder("GetVisitorsInPark", "Lu", ret="i"))
STUBS.internal(0x100fe134, CL.recorder("ProsecutePerp", "Lw"))
STUBS.internal(0x100fe2b0, CL.recorder("IsAvailable", "L", ret="b"))
STUBS.internal(0x1012bde0, CL.recorder("IsStriking", "Li", ret="i"))
STUBS.internal(0x1012be24, CL.recorder("StopStrikeInProgress", "Li"))
STUBS.internal(0x1012b084, CL.recorder("ConsiderStriking", "Li"))
STUBS.internal(0x1006fcbc, CL.recorder("CTRACE", "us"))
STUBS.internal(0x1000dde8, CL.recorder("GAME_ASSERTMSG", "is"))


def stub_region_cell(e):
    e.cl.rec("GetRegionalEffectCell", (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, CUR["cell"])


def stub_get_type(e):
    e.cl.rec("GetType", (1,))
    e.set_r(3, e.cl.ret("msgtype") & MASK)


STUBS.internal(0x10101640, stub_region_cell)
STUBS.internal(STUB_VT, stub_get_type)
STUBS.imp("labs", lambda e: e.set_r(3, abs(s32(e.r(3))) & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD["ppc"] = T.persist_alloc(runner.emu, 0x1DB000)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        """A pointer global: one word on the Mac, 64 bits on the host (the arena is not below 4 GB)."""
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.fmt[0] == ">" else "Q"), v))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.hq = e.alloc(0x68)
        self.things = [e.alloc(0x300) for _ in case["things"]]
        self.cellp = e.alloc(16)
        self.out = e.alloc(8)
        self.msg, self.vt, self.tv = e.alloc(0x10), e.alloc(0x10), e.alloc(8)
        self.worldp = WORLD["ppc"]
        CUR["hq"], CUR["cell"] = self.hq, self.cellp
        fixed = {self.hq: 1, self.worldp: 2, ANALYSER: 3, MAP: 4}
        fixed.update({a: 10 + i for i, a in enumerate(self.things)})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_thing(self, row, addr):
        self.words(ADDR["arr"] + 0x14 * row, [addr])

    def make_msg(self, perp):
        self.words(self.tv, [STUB_VT, 0])
        self.words(self.vt, [0, 0, self.tv])
        self.words(self.msg, [self.vt, 0])
        self.half(self.msg + 8, perp)
        return self.msg

    def call(self, op, case):
        if op == "ser":
            save, version, unused = case["args"]
            return T.call_ppc(self.e, FN_SER, [self.hq, FILE, save & MASK, version & MASK, unused & MASK])[0] & MASK
        if op == "recv":
            T.call_ppc(self.e, FN_RECV, [self.hq, self.make_msg(case["perp"])])
            return 0
        if op == "month":
            T.call_ppc(self.e, FN_MONTH, [self.hq])
            return 0
        T.call_ppc(self.e, FN_FIND, [self.out, self.hq, (case["args"][0] & 0xFFFF) << 16])
        return self.rd_half(self.out)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.hq = lib.tpw_sh_alloc(0x68)
        self.things = [lib.tpw_sh_alloc(0x300) for _ in case["things"]]
        self.cellp = lib.tpw_sh_addr(b"regcell")
        self.worldp = lib.tpw_sh_world()
        fixed = {1: self.hq, 2: self.worldp, 3: ANALYSER, 4: MAP}
        fixed.update({10 + i: a for i, a in enumerate(self.things)})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sh_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_thing(self, row, addr):
        self.lib.tpw_sh_setthing(row, addr)

    def make_msg(self, perp):
        return self.lib.tpw_sh_msg(perp)

    def call(self, op, case):
        lib = self.lib
        if op == "ser":
            save, version, unused = case["args"]
            return lib.tpw_sh_ser(self.hq, FILE, s32(save), s32(version), unused & MASK) & MASK
        if op == "recv":
            lib.tpw_sh_recv(self.hq, self.make_msg(case["perp"]))
            return 0
        if op == "month":
            lib.tpw_sh_month(self.hq)
            return 0
        return lib.tpw_sh_find(self.hq, case["args"][0] & 0xFFFF) & 0xFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    f = lib.tpw_sh_addr
    f.restype, f.argtypes = C.c_uint64, [C.c_char_p]
    f = lib.tpw_sh_alloc
    f.restype, f.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sh_world.restype, lib.tpw_sh_world.argtypes = C.c_uint64, []
    lib.tpw_sh_setthing.restype, lib.tpw_sh_setthing.argtypes = None, [C.c_uint32, C.c_uint64]
    lib.tpw_sh_msg.restype, lib.tpw_sh_msg.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sh_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sh_ser.restype, lib.tpw_sh_ser.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_sh_recv.restype, lib.tpw_sh_recv.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_sh_month.restype, lib.tpw_sh_month.argtypes = None, [C.c_uint64]
    lib.tpw_sh_find.restype, lib.tpw_sh_find.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sh_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.ptr(s.g("gWorld"), s.worldp)
    s.ptr(s.g("gWorldMap"), MAP)
    w = s.worldp
    s.words(w + 0x1DA70C, [case["turn"]])
    for off, v in zip(HEADS, case["heads"]):
        s.half(w + off, v)
    for i in range(5):
        s.half(s.g("s%d" % i), case["sent"][i])
    s.half(s.g("noguard"), case["noguard"])
    s.half(s.g("gend"), case["gend"])
    s.words(s.hq + 0x24, case["strikes"])
    s.half(s.hq + 0x60, case["f60"])
    s.words(s.hq + 0x64, [case["f64"]])
    s.write(s.hq + 4, bytes([case["hqx"], 0x80, case["hqy"], 0x80]))
    for blk, (row, x, y, nxt) in zip(s.things, case["things"]):
        s.write(blk + 4, bytes([x, 0x80, y, 0x80]))
        s.half(blk + 0x20C, nxt)
        s.set_thing(row, blk)
    s.set_thing(0, 0x1000 if case["row0"] else 0)
    s.half(s.cellp + 6, case["cellv"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "cellbytes": s.read(s.hq, 12), "hq": s.rd_words(s.hq + 12, 21), "f60": s.rd_half(s.hq + 0x60), "f64": s.rd_words(s.hq + 0x64, 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

KEYS = ["CThing::Serialise", "SAVEVAR<int>", "LOADVAR<int>", "SAVEVARSTRUCT<tag>", "LOADVARSTRUCT<tag>", "SAVEVARSTRUCT<Strike>",
        "LOADVARSTRUCT<Strike>", "CTrainingCentre::Serialise"]


def lst(r, n=6, p=0.3):
    return [1 if r.random() < p else 0 for _ in range(n)]


def sm(r):
    return r.choice([0, 1, 2, 3, 0x27FF])


def base(r, op):
    q = {k: [0] for k in KEYS}
    q.update({"IsParkClosed": [r.choice([0, 1])], "GetVisitorsInPark": [r.choice([0, 0, 5])], "IsStriking": lst(r, 6, 0.5),
              "IsAvailable": lst(r, 8, 0.6), "msgtype": [r.choice([12, 14, 14, 14, 13, 11, 15, 0, -1, 99])]})
    turn = r.choice([0, 1, 5, 0xFFFFFFFF, r.getrandbits(32)])
    return {"op": op, "queues": q, "turn": turn, "heads": [sm(r) for _ in range(5)], "sent": [sm(r) for _ in range(5)],
            "noguard": r.choice([0, 0, 0, 7]), "gend": r.choice([0, 0, 0, 9]), "f64": r.choice([0, 1, r.getrandbits(32)]),
            "f60": r.choice([0, r.getrandbits(16)]), "strikes": [r.choice([0, turn, r.getrandbits(32)]) for _ in range(15)],
            "hqx": r.randrange(256), "hqy": r.randrange(256), "things": [], "row0": r.choice([0, 0, 0, 1]),
            "cellv": r.choice([-1, 0, 1, 7, -32768, 32767]), "perp": 5, "args": (0, 0, 0)}


def chain(r, c, n, avoid=()):
    """n guards linked into the guard list; the world's guard head is the first."""
    rows = []
    while len(rows) < n:
        v = r.choice([1, 2, 3, 0x27FF, r.randrange(1, 0x2800)])
        if v not in rows and v != c["gend"] and v not in avoid:
            rows.append(v)
    span = r.choice([2, 4, 128, 256])
    for i, row in enumerate(rows):
        c["things"].append((row, r.randrange(span), r.randrange(span), rows[i + 1] if i + 1 < n else c["gend"]))
    c["heads"][4] = rows[0] if rows else c["gend"]
    c["hqx"], c["hqy"] = r.randrange(span), r.randrange(span)
    return rows


# ---- Serialise
def ser_case(r, save=None, version=None, fail=None):
    c = base(r, "ser")
    c["args"] = (r.choice([0, 1, -1, 5]) if save is None else save, r.choice([-3, -1, 0, 1, 2, 3, 4]) if version is None else version,
                 r.getrandbits(32))
    if fail is None:
        for k in KEYS:
            c["queues"][k] = lst(r, 6, 0.08)
    else:
        key, idx = fail
        c["queues"][key] = [0] * idx + [1]
    return c


def ser_edges():
    r = random.Random(5918)
    fails = [None, ("CThing::Serialise", 0)]
    for save in (0, 1):
        s = "SAVE" if save else "LOAD"
        fails += [(s + "VAR<int>", 0), (s + "VARSTRUCT<tag>", 0), ("CTrainingCentre::Serialise", 0)]
        fails += [(s + "VARSTRUCT<Strike>", i) for i in range(5)]
    for save in (0, 1):
        for version in (-1, 0, 1, 2, 3):
            for fail in fails:
                yield ser_case(r, save, version, fail)


def ser_random(r):
    return ser_case(r)


# ---- UpdateMonthly
def month_case(r, f64=None, closed=None, vis=None, only=None, same=None, striking=None):
    c = base(r, "month")
    if f64 is not None:
        c["f64"] = f64
    if closed is not None:
        c["queues"]["IsParkClosed"] = [closed]
    if vis is not None:
        c["queues"]["GetVisitorsInPark"] = [vis]
    if only is not None:
        c["heads"] = [c["sent"][[0, 1, 2, 4, 3][i]] if False else 0 for i in range(5)]
        order = (0x1DA73C, 0x1DA73E, 0x1DA740, 0x1DA742, 0x1DA744)
        # type t reads the head at order[0,1,2,4,3][t]; make every head equal to its sentinel but type `only`'s
        slot = {0: 0, 1: 1, 2: 2, 3: 4, 4: 3}
        c["sent"] = [r.choice([0, 3]) for _ in range(5)]
        for t in range(5):
            c["heads"][slot[t]] = c["sent"][t] + (1 if t == only else 0)
        c["strikes"][3 * only + 2] = c["turn"] if same else (c["turn"] ^ 1)
        c["queues"]["IsStriking"] = [striking]
    return c


def month_edges():
    r = random.Random(11)
    for f64 in (0, 1):
        for closed in (0, 1):
            for vis in (0, 3):
                yield month_case(r, f64, closed, vis)
    for t in range(5):
        for same in (0, 1):
            for striking in (0, 1):
                yield month_case(r, 1, 0, 0, t, same, striking)


def month_random(r):
    return month_case(r)


# ---- FindNearestAvailableGuard
def find_case(r, n=None):
    c = base(r, "find")
    chain(r, c, r.choice([0, 1, 2, 3, 5]) if n is None else n)
    c["args"] = (r.getrandbits(16), 0, 0)
    return c


def find_edges():
    r = random.Random(77)
    for n in (0, 1, 2, 3, 4, 6):
        for avail in itertools.product((0, 1), repeat=min(n, 3)):
            c = find_case(r, n)
            c["queues"]["IsAvailable"] = list(avail) or [0]
            yield c
    for _ in range(12):
        c = find_case(r, 4)
        c["queues"]["IsAvailable"] = [1]
        yield c


def find_random(r):
    return find_case(r)


# ---- ReceiveMessage
def recv_case(r, mtype=None, cellv=None, n=None, noguard=None, row0=None):
    c = base(r, "recv")
    if mtype is not None:
        c["queues"]["msgtype"] = [mtype]
    rows = chain(r, c, r.choice([0, 1, 2, 4]) if n is None else n)
    perp = r.choice([5, 6, 0x27FE, r.randrange(1, 0x2800)])
    while perp in rows or perp == c["gend"]:
        perp += 1
    c["perp"] = perp
    c["things"].append((perp, r.randrange(256), r.randrange(256), 0))
    if cellv is not None:
        c["cellv"] = cellv
    if noguard is not None:
        c["noguard"] = noguard
    if row0 is not None:
        c["row0"] = row0
    return c


def recv_edges():
    r = random.Random(99)
    for mtype in (-1, 0, 11, 12, 13, 14, 15, 16, 0x7FFFFFFF):
        yield recv_case(r, mtype, 1, 2)
    for cellv in (-1, 0, 1, 32767, -32768):
        yield recv_case(r, 14, cellv, 2)
    for n in (0, 1, 3):
        for noguard in (0, 7):
            for row0 in (0, 1):
                c = recv_case(r, 14, 3, n, noguard, row0)
                c["queues"]["IsAvailable"] = [1]
                yield c
    for _ in range(4):
        c = recv_case(r, 14, 3, 3)
        c["queues"]["IsAvailable"] = [0]
        yield c


def recv_random(r):
    return recv_case(r)


TESTS = [
    FnTest("CStaffHQ::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
    FnTest("CStaffHQ::ReceiveMessage", ppc, host, edge=recv_edges, rnd=recv_random),
    FnTest("CStaffHQ::UpdateMonthly", ppc, host, edge=month_edges, rnd=month_random),
    FnTest("CStaffHQ::FindNearestAvailableGuard", ppc, host, edge=find_edges, rnd=find_random),
]
