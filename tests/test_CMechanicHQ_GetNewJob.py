"""Unit v5889, the mechanic HQ, against libtpw_unit_CMechanicHQ_GetNewJob.dylib (tests/unit_CMechanicHQ_GetNewJob.cpp), main app
("SimTheme Park"), src/game/buildings/MechanicHQ.cpp:
  CMechanicHQ::GetNewJob 0x10105450, CMechanicHQ::Serialise 0x10105348, CMechanicHQ::ReassignJobsAfterDrop 0x10105818,
  CMechanicHQ::ReceiveMessage 0x10105988

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: the game assertion (condition and a hash of the text), the ride queries
(IsBrokenDown, IsFlaggedForMaintenance, RequiresUpgrade, GetAssignedStaffMember, UnassignStaffMember, answering from the ride's own
fields), GetThingId, CThing::Serialise, SAVEVARSTRUCT / LOADVARSTRUCT (the loaded value is written into the field), the staff state of a
mechanic, the advisor request constructor, GetAdvisor, QueueRequest, CTRACE and the message's GetType (through the pointer glue).

The world of a case: gWorld and its two list heads (mechanics +0x1da73e, objects +0x1da746), the thing table's entries (word 0 of the
entry 0 is zero or not, to hit the assertion both ways), a chain of rides (next at +0xc, type +0xe, flags +0x2e, staff +0x5a, flagged +0x60,
state +0x198, cell bytes +4 and +6), a chain of mechanics (next +0x20c, ride to repair +0x214, staff state +0x100), the eleven id constants and
the HQ's cursor. What is compared: the answer, the log of every call and the HQ's cursor halfword.

No fused operations. Not tested: a list that walks past its end marker or a thing id with no thing in the table (a NULL read on both sides).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMechanicHQ_GetNewJob"
MASK = 0xFFFFFFFF

FN_JOB, FN_SER, FN_REASSIGN, FN_RECV = 0x10105450, 0x10105348, 0x10105818, 0x10105988
SIZES = {FN_JOB: 876, FN_SER: 212, FN_REASSIGN: 256, FN_RECV: 876}
OWN = set(SIZES)

ASSERT, THING_SER, SAVE, LOAD = 0x1000dde8, 0x1012e010, 0x1000e9d4, 0x1000eb4c
ISBROKEN, FLAGGED, REQUP, GETASSIGNED, UNASSIGN = 0x1010c878, 0x1010c980, 0x1010c9c0, 0x1010c5a0, 0x1010c554
GETTHINGID, GENSTATE, CTRACE = 0x1012e1f8, 0x101267cc, 0x1006fcbc
ADVREQ, GETADVISOR, QUEUEREQ, PTRGLUE, LABS = 0x1000deb4, 0x1013eab0, 0x1000ae48, 0x1023baa8, 0x10239928

ADDR = dict(gWorld=0x1037ec0c, arr=0x1034cbfc,
            n9a=0x102a0b9a, n9c=0x102a0b9c, n9e=0x102a0b9e, na0=0x102a0ba0, na2=0x102a0ba2, na4=0x102a0ba4,
            na6=0x102a0ba6, na8=0x102a0ba8, naa=0x102a0baa, nac=0x102a0bac, nae=0x102a0bae)
CONSTS = ["n9a", "n9c", "n9e", "na0", "na2", "na4", "na6", "na8", "naa", "nac", "nae"]
HOST_LAYOUT = [0xc, 0xe, 0x2e, 0x5a, 0x60, 0x198, 4, 6, 0x20c, 0x214, 0xc, 0x1da73e, 0x1da746, 0x448]
MAC_STRIDE = 0x14

BLOCKS = {"world": 0x10, "hq": 0x20, "res": 8, "msg": 0x20, "vt": 0x20, "fd": 0x10, "mref": 0x20}
for _i in range(16):
    BLOCKS["r%d" % _i] = 0x448
    BLOCKS["m%d" % _i] = 0x218
PPC_BLK = {}


def s32(v):
    return T.s32(v)


def fnv(text):
    if isinstance(text, str):
        text = text.encode("latin-1")
    h = 2166136261
    for c in text:
        h = ((h ^ c) * 16777619) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rd16(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def wr16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def rd32(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (e.r(3) & MASK, fnv(e.cstr(e.r(4)))))


def stub_isbroken(e):
    o = e.r(3)
    e.cl.rec("IsBrokenDown", (rd16(e, o),))
    e.set_r(3, 1 if rd32(e, o + 0x198) == 1 else 0)


def stub_flagged(e):
    o = e.r(3)
    e.cl.rec("IsFlaggedForMaintenance", (rd16(e, o),))
    e.set_r(3, rd32(e, o + 0x60))


def stub_requp(e):
    o = e.r(3)
    e.cl.rec("RequiresUpgrade", (rd16(e, o),))
    e.set_r(3, 1 if rd32(e, o + 0x198) == 2 else 0)


def stub_getassigned(e):
    res, o = e.r(3), e.r(4)
    e.cl.rec("GetAssignedStaffMember", (rd16(e, o),))
    wr16(e, res, rd16(e, o + 0x5a))


def stub_unassign(e):
    e.cl.rec("UnassignStaffMember", (rd16(e, e.r(3)),))


def stub_getthingid(e):
    res, o = e.r(3), e.r(4)
    e.cl.rec("GetThingId", (rd16(e, o),))
    wr16(e, res, rd16(e, o))


def stub_thing_ser(e):
    e.cl.rec("CThing::Serialise", (e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))
    e.set_r(3, e.cl.ret("CThing::Serialise") & MASK)


def stub_save(e):
    e.cl.rec("SAVEVARSTRUCT", (e.r(3) & MASK, rd16(e, e.r(4)), fnv(e.cstr(e.r(5))), e.r(6) & MASK))
    e.set_r(3, e.cl.ret("SAVEVARSTRUCT") & MASK)


def stub_load(e):
    e.cl.rec("LOADVARSTRUCT", (e.r(3) & MASK, rd16(e, e.r(4)), fnv(e.cstr(e.r(5))), e.r(6) & MASK))
    wr16(e, e.r(4), e.cl.ret("LoadValue"))
    e.set_r(3, e.cl.ret("LOADVARSTRUCT") & MASK)


def stub_genstate(e):
    o = e.r(3)
    e.cl.rec("GetGeneralStaffState", (rd16(e, o),))
    e.set_r(3, rd32(e, o + 0x100))


def stub_getadvisor(e):
    e.cl.rec("GetAdvisor", (1 if e.r(3) == rd32(e, ADDR["gWorld"]) else 0,))
    e.set_r(3, 0x5000)


def stub_advreq(e):
    this = e.r(3)
    sec = e.r(6) & MASK
    msg = e.r(4) & MASK
    tid = (e.r(5) >> 16) & 0xFFFF
    e.cl.rec("CAdvisorRequest", (msg, tid, sec, e.r(7) & MASK, e.r(8) & 0xFF, e.r(9) & MASK))
    wr16(e, this, tid)
    e.write(this + 4, struct.pack(">I", sec))
    e.write(this + 0xC, struct.pack(">I", msg))


def stub_queue(e):
    req = e.r(4)
    e.cl.rec("QueueRequest", (e.r(3) & MASK, rd16(e, req), rd32(e, req + 4), rd32(e, req + 0xC)))
    e.set_r(3, 1)


def stub_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3) & MASK, fnv(e.cstr(e.r(4))), e.r(5) & MASK))


def stub_ptrglue(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    e.cl.rec("GetType", (0,))
    e.set_r(3, e.cl.ret("GetType") & MASK)


def stub_labs(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    e.set_r(3, abs(s32(e.r(3))) & MASK)


for _a, _f in ((ASSERT, stub_assert), (ISBROKEN, stub_isbroken), (FLAGGED, stub_flagged), (REQUP, stub_requp),
               (GETASSIGNED, stub_getassigned), (UNASSIGN, stub_unassign), (GETTHINGID, stub_getthingid),
               (THING_SER, stub_thing_ser), (SAVE, stub_save), (LOAD, stub_load), (GENSTATE, stub_genstate),
               (GETADVISOR, stub_getadvisor), (ADVREQ, stub_advreq), (QUEUEREQ, stub_queue), (CTRACE, stub_ctrace),
               (PTRGLUE, stub_ptrglue), (LABS, stub_labs)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name, size in BLOCKS.items():
        PPC_BLK[name] = T.persist_alloc(runner.emu, size)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.endian + "I", w & MASK) for w in ws))

    def ptr(self, a, v):
        """A pointer word: four bytes on the Mac, eight on the host."""
        if self.endian == ">":
            self.words(a, [v])
        else:
            self.write(a, struct.pack("<Q", v & 0xFFFFFFFFFFFFFFFF))

    def h(self, a, v):
        self.write(a, struct.pack(self.endian + "H", v & 0xFFFF))

    def b(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_h(self, a):
        return struct.unpack(self.endian + "H", self.read(a, 2))[0]

    def entry(self, idx):
        return self.g("arr") + idx * self.stride


class Ppc(Side):
    endian = ">"
    stride = MAC_STRIDE

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def blk(self, name):
        return PPC_BLK[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_job(self, hq, mech):
        res = self.blk("res")
        self.write(res, b"\xbe\xef\xbe\xef")
        T.call_ppc(self.e, FN_JOB, [res, hq, (mech & 0xFFFF) << 16])
        return self.rd_h(res)

    def call_ser(self, hq, file, save, version, unused):
        r3, _, _ = T.call_ppc(self.e, FN_SER, [hq, file, save & MASK, version & MASK, unused])
        return r3 & MASK

    def call_reassign(self, hq, cell):
        T.call_ppc(self.e, FN_REASSIGN, [hq, self.blk("mref"), (cell & 0xFFFF) << 16])
        return 0

    def call_recv(self, hq, tid):
        msg = self.blk("msg")
        self.write(msg, b"\0" * 0x20)
        self.words(msg, [self.blk("vt"), 0])
        self.h(msg + 8, tid)
        vt, fd = self.blk("vt"), self.blk("fd")
        self.words(vt, [0, 0, fd, 0])
        self.words(fd, [0x1000, 0])
        T.call_ppc(self.e, FN_RECV, [hq, msg])
        return 0


class Host(Side):
    endian = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stride = lib.stride
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hq_addr(name.encode())

    def blk(self, name):
        if name not in self.lib.blocks:
            self.lib.blocks[name] = self.lib.tpw_hq_alloc(BLOCKS[name])
        return self.lib.blocks[name]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_job(self, hq, mech):
        return self.lib.tpw_hq_getnewjob(hq, mech & 0xFFFF) & 0xFFFF

    def call_ser(self, hq, file, save, version, unused):
        return self.lib.tpw_hq_serialise(hq, file, s32(save), s32(version), unused) & MASK

    def call_reassign(self, hq, cell):
        self.lib.tpw_hq_reassign(hq, self.blk("mref"), cell & 0xFFFF)
        return 0

    def call_recv(self, hq, tid):
        self.lib.tpw_hq_receive(hq, tid)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hq_"
    lib.tpw_hq_addr.restype, lib.tpw_hq_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hq_alloc.restype, lib.tpw_hq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hq_getnewjob.restype, lib.tpw_hq_getnewjob.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_hq_serialise.restype = C.c_int32
    lib.tpw_hq_serialise.argtypes = [C.c_uint64, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_hq_reassign.restype, lib.tpw_hq_reassign.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_hq_receive.restype, lib.tpw_hq_receive.argtypes = None, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hq_layout(out)
    assert list(out)[:n - 1] == HOST_LAYOUT, list(out)[:n]
    lib.stride = out[n - 1]
    lib.blocks = {}
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for name in CONSTS:
        s.h(s.g(name), case["g"][name])
    wb = s.blk("world")                 # only the two heads are read: the world sits so that they land in a small block
    world = wb - 0x1da73e
    s.ptr(s.g("gWorld"), world)
    s.h(world + 0x1da73e, case["mech_head"])
    s.h(world + 0x1da746, case["obj_head"])
    s.ptr(s.entry(0), 0x1000 if case["arr0"] else 0)
    for i, o in enumerate(case["rides"]):
        a = s.blk("r%d" % i)
        s.write(a, bytes(0x448))
        s.h(a, o["id"])
        s.b(a + 4, o["cx"])
        s.b(a + 6, o["cy"])
        s.h(a + 0xC, o["next"])
        s.h(a + 0xE, o["type"])
        s.h(a + 0x2E, o["flags"])
        s.h(a + 0x5A, o["staff"])
        s.words(a + 0x60, [o["flagged"]])
        s.words(a + 0x198, [o["state"]])
        s.ptr(s.entry(o["id"]), a)
    for i, o in enumerate(case["mechs"]):
        a = s.blk("m%d" % i)
        s.write(a, bytes(0x218))
        s.h(a, o["id"])
        s.b(a + 4, o["cx"])
        s.b(a + 6, o["cy"])
        s.h(a + 0x20C, o["next"])
        s.h(a + 0x214, o["repair"])
        s.words(a + 0x100, [o["state"]])
        s.ptr(s.entry(o["id"]), a)
    hq = s.blk("hq")
    s.write(hq, bytes(0x20))
    s.h(hq + 0xC, case["f0c"])


def execute(s, case):
    hq = s.blk("hq")
    op = case["op"]
    if op == "job":
        return s.call_job(hq, case["mech"])
    if op == "ser":
        return s.call_ser(hq, case["file"], case["save"], case["version"], case["unused"])
    if op == "reassign":
        return s.call_reassign(hq, case["cell"])
    return s.call_recv(hq, case["msg_id"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "f0c": s.rd_h(s.blk("hq") + 0xC)}


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


# ---------------------------------------------------------------------------------------------------------------- case generators

IDPOOL = list(range(1, 40)) + [0x27FF, 0x1000, 0x7FF]


def world_case(r, op, nr=None, nm=None, empty=None, f0c_kind=None, msg_type=7):
    nr = r.randrange(1, 7) if nr is None else nr
    nm = r.randrange(1, 5) if nm is None else nm
    ids = r.sample(IDPOOL, nr + nm)
    rid, mid = ids[:nr], ids[nr:]
    end = r.choice([0, 0xFFFF, 0x8001])
    g = {"n9a": r.choice([end, 0x4444]), "n9c": end, "n9e": end, "na0": end, "na2": end, "na4": r.choice([0, 0x5555]),
         "na6": end, "na8": r.choice([0, 0x5555]), "naa": r.choice([end, end, 0x4444]), "nac": end,
         "nae": r.choice([0, 0, end, 0x6666])}
    mech_id = mid[0]
    rides = []
    for i, i_d in enumerate(rid):
        small = r.random() < 0.4
        rides.append({
            "id": i_d, "cx": r.randrange(8) if small else r.randrange(256), "cy": r.randrange(8) if small else r.randrange(256),
            "next": rid[i + 1] if i + 1 < nr else end, "type": r.randrange(0x10000), "flags": r.choice([0, 1, 2, 3, r.randrange(0x10000)]),
            "staff": r.choice([g["na4"], mech_id, mech_id, r.randrange(0x6000, 0x6100), g["na8"]]),
            "flagged": r.choice([0, 1]), "state": r.choice([0, 0, 1, 2, 3, 4]),
        })
    mechs = []
    for i, i_d in enumerate(mid):
        mechs.append({
            "id": i_d, "cx": r.randrange(256), "cy": r.randrange(256), "next": mid[i + 1] if i + 1 < nm else end,
            "repair": r.choice([g["nae"], g["nae"], rid[r.randrange(nr)], r.randrange(0x6000, 0x6100)]),
            "state": r.choice([0, 1, 2, 3, 4, 5]),
        })
    if empty is None:
        empty = r.random() < 0.15
    obj_head = end if empty else rid[0]
    if f0c_kind is None:
        f0c_kind = r.choice(["n9a", "ride", "ride", "stale", "end", "rand"])
    f0c = {"n9a": g["n9a"], "ride": rid[r.randrange(nr)], "ride_last": rid[-1], "ride_first": rid[0], "stale": 0x3000, "end": end,
           "rand": r.randrange(0x10000)}[f0c_kind]
    if f0c_kind == "rand" and f0c in rid + mid or f0c == 0x3000 and 0x3000 in rid:
        f0c = end
    case = {"op": op, "g": g, "rides": rides, "mechs": mechs, "obj_head": obj_head, "mech_head": mid[0] if r.random() > 0.1 else end,
            "arr0": 1 if r.random() < 0.12 else 0, "f0c": f0c, "mech": mech_id, "cell": r.randrange(0x10000),
            "msg_id": rid[r.randrange(nr)],
            "queues": {"GetType": [msg_type], "CThing::Serialise": [0], "SAVEVARSTRUCT": [0], "LOADVARSTRUCT": [0], "LoadValue": [0]}}
    return case


def job_random(r):
    return world_case(r, "job")


def job_edges():
    r = random.Random(5889)
    for nr in (1, 2, 4):
        for kind in ("n9a", "ride_first", "ride_last", "stale", "end", "ride"):
            for empty in (False, True):
                yield world_case(r, "job", nr=nr, empty=empty, f0c_kind=kind)
    for _ in range(12):
        c = world_case(r, "job", nr=3, empty=False, f0c_kind="ride_first")
        for o in c["rides"]:
            o["state"], o["staff"], o["cx"], o["cy"] = 1, c["g"]["na4"], r.randrange(2), r.randrange(2)
        yield c


def reassign_random(r):
    return world_case(r, "reassign")


def reassign_edges():
    r = random.Random(5890)
    for nr in (1, 2, 5):
        for empty in (False, True):
            for _ in range(3):
                yield world_case(r, "reassign", nr=nr, empty=empty)


def recv_random(r):
    return world_case(r, "recv", msg_type=r.choice([7, 7, 7, 7, 6, 0, 8, 9, 20, 0x7FFFFFFF]))


def recv_edges():
    r = random.Random(5891)
    for t in (0, 6, 8, 7):
        yield world_case(r, "recv", msg_type=t)
    for nm in (1, 2, 3):
        for empty in (False, True):
            for _ in range(6):
                c = world_case(r, "recv", nm=nm, empty=empty, msg_type=7)
                yield c
    # the head compare: no mechanics at all
    for _ in range(4):
        c = world_case(r, "recv", msg_type=7)
        c["mech_head"] = c["g"]["naa"]
        yield c
    # each mechanic state with the staff-state test (3, 4 and others)
    for st in range(6):
        c = world_case(r, "recv", nm=1, msg_type=7)
        c["mechs"][0]["repair"] = c["g"]["nae"]
        c["mechs"][0]["state"] = st
        c["mech_head"] = c["mechs"][0]["id"]
        yield c


def ser_case(r, save=None, version=None):
    c = world_case(r, "ser")
    c["file"] = r.choice([0x1000, 0x2000, r.randrange(1, 0x40000000)])
    c["save"] = r.choice([0, 1, 1, 0, 5, -1]) if save is None else save
    c["version"] = r.choice([-1, 0, 1, 2, 3, 0x7FFFFFFF, -0x80000000]) if version is None else version
    c["unused"] = r.choice([0, 1, r.randrange(1 << 32)])
    c["queues"] = {"GetType": [7], "CThing::Serialise": [r.choice([0, 1])], "SAVEVARSTRUCT": [r.choice([0, 1])],
                   "LOADVARSTRUCT": [r.choice([0, 1])], "LoadValue": [r.randrange(0x10000)]}
    return c


def ser_random(r):
    return ser_case(r)


def ser_edges():
    r = random.Random(5892)
    for save in (0, 1, 7):
        for version in (-1, 0, 1, 2, 3):
            for ts in (0, 1):
                for sv in (0, 1):
                    c = ser_case(r, save, version)
                    c["queues"]["CThing::Serialise"] = [ts]
                    c["queues"]["SAVEVARSTRUCT"] = [sv]
                    c["queues"]["LOADVARSTRUCT"] = [sv]
                    yield c


TESTS = [
    FnTest("CMechanicHQ::GetNewJob", ppc, host, edge=job_edges, rnd=job_random),
    FnTest("CMechanicHQ::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
    FnTest("CMechanicHQ::ReassignJobsAfterDrop", ppc, host, edge=reassign_edges, rnd=reassign_random),
    FnTest("CMechanicHQ::ReceiveMessage", ppc, host, edge=recv_edges, rnd=recv_random),
]
