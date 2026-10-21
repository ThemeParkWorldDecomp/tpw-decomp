"""Unit v5024, the research lab's message handler and its save/load, against libtpw_unit_CResearchLab_ReceiveMessage.dylib
(tests/unit_CResearchLab_ReceiveMessage.cpp), main app ("SimTheme Park"), src/game/buildings/ResearchLab.cpp:
  CResearchLab::ReceiveMessage 0x10123144, CResearchLab::Serialise 0x10121ce8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (the ride database, the project and
category parts, UpdateHighestGroup, IsAllResearchDone, the message centre, the master save, the game type constructor, the advisor, the
winning sequence, CThing/CResearchProject/CResearchCategory::Serialise and SAVEVAR/LOADVAR) are fakes on both sides that record their
arguments (labels for the lab and its parts) and answer from the case's queues. operator new and the array constructor are fakes too.

The world of a case: the lab block (project count, work load, best project, active flag), gWorld, gMessageCentre and gMasterSave (null or
an existing block), the game type word (the guard byte is set on the PowerPC side only). What is compared: the answer, the lab's four
words, where the three globals point afterwards (null, the new object, another) and the event log.

No fused operations. Not tested: the game type's guard byte on the host (a function-local static there; the constructor fake does nothing,
so the type word is the case's).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CResearchLab_ReceiveMessage"
MASK = 0xFFFFFFFF

FN_RECEIVE, FN_SERIALISE = 0x10123144, 0x10121CE8
SIZES = {FN_RECEIVE: 716, FN_SERIALISE: 744}
OWN = set(SIZES)

PTR_GLUE = 0x1023BAA8
FILE = 0x12345670

ADDR = dict(gWorld=0x1037EC0C, gMessageCentre=0x1037F6C4, gMasterSave=0x10380A8C, gametype=0x102B3AA0, guard=0x102B3AA4,
            db=0x1037F6C8, token=0x1037F6CC)
HOST_LAYOUT = [0x146C, 0x145C, 0x1460, 0x1464, 0x1468]

# the lab's offsets that the log names
LABELS = {0: 1}
for _i in range(0x100):
    LABELS[0xC + _i * 0x14] = 100 + _i
for _i in range(5):
    LABELS[0x140C + _i * 0x10] = 400 + _i
LABELS.update({0x145C: 500, 0x1460: 501, 0x1464: 502, 0x1468: 503})

LAB = [0]


def s32(v):
    return T.s32(v)


def be(v):
    return struct.pack(">I", v & MASK)


def save_toc(e):
    e.write(e.r(1) + 0x14, be(e.r(2)))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    save_toc(e)
    e.set_r(3, e.cl.ret("GetType") & MASK)


STUBS.internal(PTR_GLUE, stub_glue)


def stub_new(e):
    size = e.r(3)
    save_toc(e)
    a = e.alloc(size if size else 4)
    e.set_r(3, a)


def stub_construct_array(e):
    save_toc(e)
    e.cl.rec("construct_array", (int(e.r(4) == 0x102657E8), int(e.r(5) == 0x10267288), e.r(6), e.r(7), kind(e, e.r(3))))


STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x10238758, stub_construct_array)


def kind(e, p):
    if p == 0:
        return 0
    return 2 if p in e.presets else 1


STUBS.internal(0x10154868, CL.recorder("GetRideInfo", "Lw", ret=lambda e, log: 0x1037F6CC if log.ret("ride") else 0))
STUBS.internal(0x10123E6C, CL.recorder("SetupStatUpgrade", "LL", "b"))
STUBS.internal(0x10122E28, CL.recorder("UpdateHighestGroup", "Li"))
STUBS.internal(0x101234D0, CL.recorder("IsAllResearchDone", "L", "b"))


def stub_msgctor(e):
    e.cl.rec("MsgCtor", (s32(e.r(4)),))
    e.write(e.r(3) + 8, be(e.r(4)))


STUBS.internal(0x10151D94, stub_msgctor)


def stub_post(e):
    e.cl.rec("PostMessage", (kind(e, e.r(3)), e.u32(e.r(4) + 8)))


STUBS.internal(0x1015256C, stub_post)
STUBS.internal(0x101694DC, lambda e: e.cl.rec("MSctor", (kind(e, e.r(3)),)))
STUBS.internal(0x1016BA04, lambda e: e.cl.rec("SetAllResearch", (kind(e, e.r(3)),)))


def stub_hasall(e):
    e.cl.rec("HasAll", (kind(e, e.r(3)),))
    e.set_r(3, e.cl.ret("HasAll") & 0xFF)


STUBS.internal(0x1016B94C, stub_hasall)
STUBS.internal(0x1016F5D8, lambda e: None)
STUBS.internal(0x1000DEB4, CL.recorder("AdvisorRequest", "xiwuibi", ret=lambda e, log: e.r(3)))


def stub_getadvisor(e):
    e.cl.rec("GetAdvisor", (int(e.r(3) == e.u32(ADDR["gWorld"])),))


def stub_queue(e):
    e.cl.rec("QueueRequest", (int(e.r(3) == e.u32(ADDR["gWorld"])),))
    e.set_r(3, 1)


def stub_winning(e):
    e.cl.rec("StartWinning", (int(e.r(3) == e.u32(ADDR["gWorld"])),))


STUBS.internal(0x1013EAB0, stub_getadvisor)
STUBS.internal(0x1000AE48, stub_queue)
STUBS.internal(0x1013BC68, stub_winning)

STUBS.internal(0x1012E010, CL.recorder("ThingSer", "Luiiu", "i"))
STUBS.internal(0x10123AF0, CL.recorder("ProjectSer", "Luiiu", "i"))
STUBS.internal(0x101242E4, CL.recorder("CategorySer", "Luiiu", "i"))
for _addr, _name in ((0x1000F4B0, "SAVEVAR<int>"), (0x1000F5DC, "SAVEVAR<unsigned long>"), (0x1000F70C, "SAVEVAR<bool>"),
                     (0x1000EF18, "LOADVAR<int>"), (0x1000F02C, "LOADVAR<unsigned long>"), (0x1000F144, "LOADVAR<bool>")):
    STUBS.internal(_addr, CL.recorder(_name, "uLsi", "i"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    LAB[0] = T.persist_alloc(runner.emu, 0x146C)


class Side:
    fmt = ">I"

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def setptr(self, name, v):
        self.words(self.g(name), [v])

    def kind(self, name):
        v = self.getptr(name)
        if v == 0:
            return 0
        return 2 if v in self.presets() else 1


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.presets = set()
        self.lab = LAB[0]
        fixed = {self.lab + off: lbl for off, lbl in LABELS.items()}
        fixed[ADDR["db"]] = 50
        fixed[ADDR["token"]] = 51
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def getptr(self, name):
        return self.rd_word(self.g(name))

    def presets(self):
        return self.e.presets

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.presets.add(a)
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setguard(self, v):
        self.write(ADDR["guard"], bytes([v]))

    def receive(self, type_, id_, cat):
        e = self.e
        vt = e.alloc(16)
        msg = e.alloc(16)
        self.words(msg, [vt, 0, id_ << 16, cat])
        T.call_ppc(e, FN_RECEIVE, [self.lab, msg])
        return 0

    def serialise(self, save, version, unused):
        r3, _, _ = T.call_ppc(self.e, FN_SERIALISE, [self.lab, FILE, save & MASK, version & MASK, unused & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.mine = set()
        bind(lib)
        lib.tpw_rl_reset()
        if not hasattr(lib, "hlab"):
            lib.hlab = lib.tpw_rl_alloc(0x146C)
        self.lab = lib.hlab
        fixed = {lbl: self.lab + off for off, lbl in LABELS.items()}
        fixed[50] = self.g("db")
        fixed[51] = self.g("token")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rl_addr(name.encode())

    def getptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def setptr(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def presets(self):
        return self.mine

    def alloc(self, n):
        a = self.lib.tpw_rl_alloc(n)
        self.mine.add(a)
        return a

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setguard(self, v):
        pass

    def receive(self, type_, id_, cat):
        self.lib.tpw_rl_receive(self.lab, type_, id_, cat)
        return 0

    def serialise(self, save, version, unused):
        return self.lib.tpw_rl_serialise(self.lab, s32(save), s32(version), unused & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rl_"
    lib.tpw_rl_addr.restype, lib.tpw_rl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rl_alloc.restype, lib.tpw_rl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rl_reset.restype, lib.tpw_rl_reset.argtypes = None, []
    lib.tpw_rl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rl_receive.restype, lib.tpw_rl_receive.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_rl_serialise.restype, lib.tpw_rl_serialise.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.lab, bytes(0x146C))
    s.words(s.lab + 0x145C, case["lab"])
    s.setptr("gWorld", s.alloc(16))
    s.setptr("gMessageCentre", s.alloc(0x1D0) if case["gmc"] else 0)
    s.setptr("gMasterSave", s.alloc(0x4C) if case["gms"] else 0)
    s.words(s.g("gametype"), [case["gt"]])
    s.setguard(case["guard"])


def execute(s, case):
    if case["op"] == "receive":
        return s.receive(case["type"], case["id"], case["cat"])
    return s.serialise(*case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "lab": s.rd_words(s.lab + 0x145C, 4), "gmc": s.kind("gMessageCentre"),
            "gms": s.kind("gMasterSave")}


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
    return {"op": op, "queues": {}, "lab": [r.choice([0, 1, 5, 255, 256]) & MASK, r.randrange(0, 200), r.choice([-1, 3]) & MASK,
                                             r.choice([0, 1])],
            "gmc": r.choice([0, 1]), "gms": r.choice([0, 1]), "gt": r.choice([0, 1, 2, 3]), "guard": r.choice([0, 1])}


def rcv_case(r, **kw):
    c = base(r, "receive")
    c["type"] = r.choice([0x11, 0x11, 0x11, 0x10, 0x12, 0, -1])
    c["id"] = r.randrange(0, 0x10000)
    c["cat"] = r.randrange(0, 6)
    c["queues"] = {"GetType": [c["type"]], "ride": [r.choice([0, 1])], "SetupStatUpgrade": [r.choice([0, 1, 0x100, 0x1FF])],
                   "IsAllResearchDone": [r.choice([0, 1])], "HasAll": [r.choice([0, 1])]}
    c.update(kw)
    c["queues"]["GetType"] = [c["type"]]
    if "done" in kw:
        c["queues"]["IsAllResearchDone"] = [kw["done"]]
        del c["done"]
    if "hasall" in kw:
        c["queues"]["HasAll"] = [kw["hasall"]]
        del c["hasall"]
    if "stat" in kw:
        c["queues"]["SetupStatUpgrade"] = [kw["stat"]]
        del c["stat"]
    return c


def rcv_edges():
    r = random.Random(5024)
    for typ in (0x11, 0x10):
        for done in (0, 1):
            for gt in (0, 1, 2, 3):
                for hasall in (0, 1):
                    for gmc in (0, 1):
                        for gms in (0, 1):
                            for guard in (0, 1):
                                yield rcv_case(r, type=typ, done=done, gt=gt, hasall=hasall, gmc=gmc, gms=gms, guard=guard)
    for stat in (0, 1, 0x100):
        for count in (0, 255, 256):
            c = rcv_case(r, type=0x11, stat=stat)
            c["lab"][0] = count
            c["queues"]["GetType"] = [0x11]
            yield c


def rcv_random(r):
    return rcv_case(r)


def ser_case(r, **kw):
    c = base(r, "serialise")
    c["args"] = [r.choice([0, 1, 7, -1]), r.choice([-1, 0, 1, 2, 5]), r.choice([0, 0xFFFFFFFF, 7])]

    def fails(n):
        k = r.randrange(0, min(n, 30) + 3)
        return [0] * k + [1] if k < min(n, 30) else [0]
    c["queues"] = {"ThingSer": r.choice([[0], [0], [0], [1]]), "ProjectSer": fails(0x100), "CategorySer": fails(5)}
    for pre in ("SAVEVAR", "LOADVAR"):
        c["queues"][pre + "<int>"] = r.choice([[0], [0, 0], [1], [0, 1]])
        c["queues"][pre + "<unsigned long>"] = r.choice([[0], [1]])
        c["queues"][pre + "<bool>"] = r.choice([[0], [1], [5]])
    c.update(kw)
    return c


def ser_edges():
    r = random.Random(1021)
    for save in (0, 1):
        for version in (-1, 0, 1, 2):
            yield ser_case(r, args=[save, version, 9], queues={"ThingSer": [0], "ProjectSer": [0], "CategorySer": [0]})
    for save in (0, 1):
        pre = "SAVEVAR" if save else "LOADVAR"
        for fl in ([1], [0, 1], [0, 0]):
            for ul in ([0], [1]):
                for bl in ([0], [1]):
                    yield ser_case(r, args=[save, 1, 3], queues={"ThingSer": [0], "ProjectSer": [0], "CategorySer": [0],
                                                                 pre + "<int>": fl, pre + "<unsigned long>": ul, pre + "<bool>": bl})
        yield ser_case(r, args=[save, 0, 3], queues={"ThingSer": [1]})
        yield ser_case(r, args=[save, 0, 3], queues={"ThingSer": [0], "ProjectSer": [0] * 30 + [1]})
        yield ser_case(r, args=[save, 0, 3], queues={"ThingSer": [0], "ProjectSer": [0], "CategorySer": [0] * 4 + [1]})


def ser_random(r):
    return ser_case(r)


TESTS = [
    FnTest("CResearchLab::ReceiveMessage", ppc, host, edge=rcv_edges, rnd=rcv_random),
    FnTest("CResearchLab::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
]
