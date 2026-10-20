"""Unit v5223, the advisor's staff report and its save/load walk, against libtpw_unit_CAdvisor_HandleStaffInfo.dylib
(tests/unit_CAdvisor_HandleStaffInfo.cpp), main app ("SimTheme Park"), src/AdvisorQueue_CAdvisor_HandleStaffInfo.cpp:
  CAdvisor::HandleStaffInfo 0x1000c40c, CAdvisor::Serialise 0x1000a468

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back.
Callees outside the unit are fakes on both sides recording the same events: CTRACE, the CAdvisorRequest constructor (it fills the
request from the queue "req"), CAdvisorScoring::GetMessageCategory, the singleton's lazy construction (the constructor and
__register_global_object events behind the guard byte), CanIQueueThis, FindLowestScoringSlotInQueue, CThing::Serialise,
CAdvisorRequest::Serialise and the SAVEVAR / LOADVAR instances (every pointer a label of its offset in the advisor). CTimeSig::Serialise
and CAdvisorRequest::Valid run as the original on the PowerPC side (the header's inline code on the host).

The world of a case: the advisor's queue, last tutorial request and a few plain words, the guard byte, the staff report (type and
situation); for Serialise the answers of each callee (one of them fails at a chosen call). What is compared: the answer, the guard byte,
the advisor's queue, tutorial request and words, and the event log.

No fused operations. Not tested: a staff type outside 0..4 builds the request from r26 as the caller left it (an uninitialised register),
so the message of the constructor event is logged as 0 on both sides for those cases.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisor_HandleStaffInfo"
MASK = 0xFFFFFFFF

FN_HANDLE, FN_SER = 0x1000C40C, 0x1000A468
SIZES = {FN_HANDLE: 660, FN_SER: 1072}
OWN = set(SIZES)

CTRACE = 0x1006FCBC
REQ_CTOR = 0x1000DEB4
VALID = 0x1000E894
SCORING_CTOR = 0x10010054
CATEGORY = 0x10010A74
CANQ = 0x1000B3D4
FINDLOW = 0x1000B7E8
CTHING_SER = 0x1012E010
CTIMESIG_SER = 0x1015EF5C
REQ_SER = 0x1000E568

SAVE_FNS = {0x1000F384: "SAVEVAR<long>", 0x1000F4B0: "SAVEVAR<int>", 0x1000F5DC: "SAVEVAR<unsigned long>",
            0x1000F96C: "SAVEVARSTRUCT<AdvisorHistory>", 0x1000F70C: "SAVEVAR<bool>", 0x1000F820: "SAVEVAR<AdvisorMessage>"}
LOAD_FNS = {0x1000EE04: "LOADVAR<long>", 0x1000EF18: "LOADVAR<int>", 0x1000F02C: "LOADVAR<unsigned long>",
            0x1000ECB4: "LOADVARSTRUCT<AdvisorHistory>", 0x1000F144: "LOADVAR<bool>", 0x1000F250: "LOADVAR<AdvisorMessage>"}

ADDR = dict(guard=0x102B4100, object=0x102B3AC8, dtor=0x102657E0, node=0x102B4104)
HOST_LAYOUT = [0x14, 0xE0, 0x16D0, 0x16EC, 0x16F0, 12, 16]   # the host message has a native vptr: staff at 12, situation at 16

OBJ_SIZE = 0x16F0
STATE = {"staff": 0}
PERSIST = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update({CTIMESIG_SER, VALID})


def stub_req_ctor(e):
    """CAdvisorRequest::CAdvisorRequest(AdvisorMessage, thing id, secondary, responseNo, override, score): fills r3 from the queue."""
    L = e.cl
    msg = s32(e.r(4)) if 0 <= STATE["staff"] <= 4 else 0
    L.rec("CAdvisorRequest", (msg, (e.r(5) >> 16) & 0xFFFF, e.r(6) & MASK, s32(e.r(7)), 1 if e.r(8) & 0xFF else 0, s32(e.r(9))))
    vals = [L.ret("req") for _ in range(9)]
    e.write(e.r(3), struct.pack(">5I", *[v & MASK for v in vals[:5]]) + bytes(v & 0xFF for v in vals[5:]))


STUBS.internal(REQ_CTOR, stub_req_ctor)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "isi"))
STUBS.internal(SCORING_CTOR, CL.recorder("ScoringCtor", "L"))
STUBS.imp("__register_global_object", CL.recorder("register", "LLL"))
STUBS.internal(CATEGORY, CL.recorder("GetMessageCategory", "Li", ret="i"))
STUBS.internal(CANQ, CL.recorder("CanIQueueThis", "Lib", ret="b"))
STUBS.internal(FINDLOW, CL.recorder("FindLowest", "L", ret="i"))
STUBS.internal(CTHING_SER, CL.recorder("CThing::Serialise", "LLiiu", ret="i"))
STUBS.internal(REQ_SER, CL.recorder("CAdvisorRequest::Serialise", "LLiiu", ret="i"))
for _a, _n in list(SAVE_FNS.items()) + list(LOAD_FNS.items()):
    STUBS.internal(_a, CL.recorder(_n, "LLsi", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    PERSIST["obj"] = T.persist_alloc(runner.emu, OBJ_SIZE)
    PERSIST["file"] = T.persist_alloc(runner.emu, 16)
    PERSIST["msg"] = T.persist_alloc(runner.emu, 16)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put_req(self, a, req):
        self.words(a, req[:5])
        self.write(a + 0x14, bytes(req[5:9]))

    def get_req(self, a):
        return self.rd_words(a, 5) + list(self.read(a + 0x14, 4))


BASE_OFFSETS = {0, 0xC, 0x10, 0xD4, 0xD8, 0xDC, 0x16D0, 0x16E8, 0x16EC} | {0x14 + 0x18 * i for i in range(8)}


def label_offsets(case):
    """The harness keeps fewer than 100 labels: the object's own fields, every sixth history record, the last one and the failing one."""
    hist = set(range(0, 0x15F, 6)) | {0x15E} | set(case.get("hist", []))
    return sorted(BASE_OFFSETS | {0xE0 + 16 * i for i in hist})


def fixed_labels(obj, file, case):
    d = {obj + off: 10 + i for i, off in enumerate(label_offsets(case))}
    d[file] = 1
    return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        STATE["staff"] = case.get("staff", 0)
        self.obj, self.file, self.msg = PERSIST["obj"], PERSIST["file"], PERSIST["msg"]
        fixed = fixed_labels(self.obj, self.file, case)
        fixed.update({0x102B3AC8: 2, 0x102657E0: 3, 0x102B4104: 4})
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

    def call_handle(self, staff, sit):
        self.words(self.msg + 8, [staff, sit])
        with_ = T.call_ppc(self.e, FN_HANDLE, [self.obj, self.msg])
        return 0

    def call_ser(self, save, version, unused):
        r3, _, _ = T.call_ppc(self.e, FN_SER, [self.obj, self.file, save & MASK, version & MASK, unused & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if not hasattr(lib, "persist"):
            lib.persist = (lib.tpw_hsi_alloc(OBJ_SIZE), lib.tpw_hsi_alloc(16), lib.tpw_hsi_alloc(32))
        self.obj, self.file, self.msg = lib.persist
        fixed = {10 + i: self.obj + off for i, off in enumerate(label_offsets(case))}
        fixed.update({1: self.file, 2: self.g("object"), 3: self.g("dtor"), 4: self.g("node")})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hsi_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_handle(self, staff, sit):
        self.words(self.msg + 12, [staff, sit])
        self.lib.tpw_hsi_handle(self.obj, self.msg)
        return 0

    def call_ser(self, save, version, unused):
        return self.lib.tpw_hsi_serialise(self.obj, self.file, s32(save), s32(version), unused & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hsi_addr.restype, lib.tpw_hsi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hsi_alloc.restype, lib.tpw_hsi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hsi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hsi_handle.restype, lib.tpw_hsi_handle.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_hsi_serialise.restype = C.c_int32
    lib.tpw_hsi_serialise.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hsi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.obj
    s.write(s.g("guard"), bytes([case.get("guard", 0)]))
    if case["op"] == "handle":
        s.words(o, case["head"])
        for i in range(8):
            s.put_req(o + 0x14 + 0x18 * i, case["queue"][i])
        s.words(o + 0xD4, case["mid"])
        s.put_req(o + 0x16D0, case["tutorial"])
        s.words(o + 0x16E8, case["tail"])


def observe(s, case, answer):
    o = s.obj
    d = {"answer": answer, "log": s.log(), "guard": list(s.read(s.g("guard"), 1))}
    if case["op"] == "handle":
        d["head"] = s.rd_words(o, 5)
        d["queue"] = [s.get_req(o + 0x14 + 0x18 * i) for i in range(8)]
        d["mid"] = s.rd_words(o + 0xD4, 3)
        d["tutorial"] = s.get_req(o + 0x16D0)
        d["tail"] = s.rd_words(o + 0x16E8, 2)
    return d


def execute(s, case):
    if case["op"] == "handle":
        return s.call_handle(case["staff"], case["situation"])
    return s.call_ser(*case["args"])


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


# ------------------------------------------------------------------------------------------------------------------ cases

def rscore(r):
    return r.choice([-2, -1, 0, 1, 2, 3, r.randrange(-1000, 1000), 0x7FFFFFFF, -0x80000000]) & MASK


def rreq(r, valid=None):
    v = r.choice([0, 1, 2, 0xFF]) if valid is None else valid
    return [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.choice([0, 1, 0x22, 0x160, r.getrandbits(32)]), rscore(r),
            v, r.choice([0, 1]), r.choice([0, 1, 7]), r.choice([0, 1, 9])]


def handle_case(r, staff=None, **kw):
    staff = r.choice([-1, 0, 1, 2, 3, 4, 5, r.randrange(-50, 50)]) if staff is None else staff
    k = kw.get("valid_prefix", r.randrange(0, 10))
    queue = []
    for i in range(8):
        if k >= 8 or i < k:
            queue.append(rreq(r, r.choice([1, 2, 0xFF])))
        else:
            queue.append(rreq(r, 0 if r.random() < 0.7 else None))
    ctor = rreq(r)
    if "new_valid" in kw:
        ctor[5] = kw["new_valid"]
    low = kw.get("low", r.choice([-1, 0, 1, 2, 7, r.randrange(-1, 8)]))
    if "equal" in kw and 0 <= low < 8:
        queue[low][5] = 1
        queue[low][4] = ctor[4]
    return {
        "op": "handle", "staff": staff, "situation": kw.get("situation", r.choice([0, 1, 2, 3, -1, r.randrange(-100, 100), r.getrandbits(32)])),
        "guard": kw.get("guard", r.choice([0, 1, 5])),
        "head": [r.getrandbits(32) for _ in range(5)], "queue": queue, "mid": [r.getrandbits(32) for _ in range(3)],
        "tutorial": rreq(r), "tail": [r.getrandbits(32) for _ in range(2)],
        "queues": {"req": ctor, "GetMessageCategory": [kw.get("cat", r.choice([1, 1, 0, 2, 10]))],
                   "CanIQueueThis": [kw.get("can", r.choice([0, 1, 1]))], "FindLowest": [low]},
    }


def handle_edges():
    r = random.Random(5223)
    for staff in (-1, 0, 1, 2, 3, 4, 5):
        for can in (0, 1):
            for cat in (1, 0):
                for k in (0, 3, 8):
                    yield handle_case(r, staff, can=can, cat=cat, valid_prefix=k, guard=r.choice([0, 1]))
    for staff in (0, 4):
        for low in (-1, 0, 7):
            for nv in (0, 1, 2):
                for eq in (False, True):
                    kw = dict(can=1, valid_prefix=8, low=low, new_valid=nv, guard=1)
                    if eq:
                        kw["equal"] = True
                    yield handle_case(r, staff, **kw)


def handle_random(r):
    return handle_case(r)


# The harness keeps only the first part of an answer queue, so a failure is placed within the first 40 history records.
SER_SAVE = [("CThing::Serialise", 1), ("CAdvisorRequest::Serialise", 9), ("SAVEVAR<long>", 1), ("SAVEVAR<int>", 1),
            ("SAVEVAR<unsigned long>", 3), ("SAVEVARSTRUCT<AdvisorHistory>", 40), ("SAVEVAR<bool>", 1),
            ("SAVEVAR<AdvisorMessage>", 1)]
SER_LOAD = [(k.replace("SAVE", "LOAD"), n) for k, n in SER_SAVE]


def ser_case(r, save=None, version=None, key=None, j=None):
    save = r.choice([0, 1, -1, 5]) if save is None else save
    version = r.choice([-2, -1, 0, 1, 1, 1, 2, 3]) if version is None else version
    table = SER_LOAD if save == 0 else SER_SAVE
    queues = {k: [0] for k, _ in table}
    hist = []
    if key is None and r.random() < 0.85:
        key = r.choice(table)[0]
    if key is not None:
        n = dict(table).get(key)
        if n is not None:
            jj = r.randrange(0, n) if j is None else min(j, n - 1)
            queues[key] = [0] * jj + [r.choice([1, 7, -1])]
            if "STRUCT" in key:
                hist = [jj]
    return {"op": "ser", "args": [save, version, r.getrandbits(32)], "queues": queues, "hist": hist}


def ser_edges():
    r = random.Random(1468)
    for save in (0, 1):
        for version in (-1, 0, 1, 2):
            yield ser_case(r, save, version, key=None, j=0) | {"queues": {}}
        table = SER_LOAD if save == 0 else SER_SAVE
        for key, n in table:
            for j in sorted({0, 1, n // 2, n - 1}):
                if j < n:
                    yield ser_case(r, save, 1, key, j)
    for save in (-1, 5):
        yield ser_case(r, save, 1, None)


def ser_random(r):
    return ser_case(r)


TESTS = [
    FnTest("CAdvisor::HandleStaffInfo", ppc, host, edge=handle_edges, rnd=handle_random),
    FnTest("CAdvisor::Serialise", ppc, host, edge=ser_edges, rnd=ser_random),
]
