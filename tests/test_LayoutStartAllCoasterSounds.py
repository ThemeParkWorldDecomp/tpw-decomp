"""Unit v5205, a train's sounds started, against libtpw_unit_LayoutStartAllCoasterSounds.dylib (tests/unit_LayoutStartAllCoasterSounds.cpp),
main app ("SimTheme Park"), src/game/rides/CoasterSounds.cpp:
  StartTrainSounds 0x10046a18, LayoutStartAllCoasterSounds 0x10046bc0

A case is a world and a call. The world is built twice by ONE piece of Python code (Mac layout in the emulator, the host's layout from
tpw_lsc_layout in the library). StopTrainSounds, RSSE_GetSoundEventCode, SND_SendIndirectedEvent and SetTrainSoundState are fakes on both
sides that record their arguments (an instance by its script id, a train by an id word the case writes into its mF08) and answer from queues.

The world: the coaster list (0 to 3 coasters of 0 to 3 trains, ids unique), each train's position floats mF48 and its old sound words, the
sound bank word, the recorder's mRecording word. Compared: the log, every train's words 0x54..0x68. No fused operations; floats stay
inside the int range (fctiwz saturation differs in Unicorn). Not tested: NaN or huge floats (same reason).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LayoutStartAllCoasterSounds"
MASK = 0xFFFFFFFF

FN_START, FN_ALL = 0x10046A18, 0x10046BC0
SIZES = {FN_START: 344, FN_ALL: 388}
OWN = set(SIZES)

STOP, RSSE, SND, STATE = 0x100471BC, 0x100D6D04, 0x100DF858, 0x100473E4

ADDR = dict(head=0x102B68B4, bank=0x102F7C94 + 0x1C, rec=0x1037EC44 + 0x24)
KEYS = ["ci_script", "ci_next", "ci_count", "ci_trains", "ci_size", "tr_id", "tr_size", "tr_f48", "tr_f54", "tr_f64", "tr_f68"]
MAC = dict(ci_script=0x24, ci_next=0x18, ci_count=0xF4, ci_trains=0x128, ci_size=0x14C, tr_id=0x08, tr_size=0x80, tr_f48=0x48,
           tr_f54=0x54, tr_f64=0x64, tr_f68=0x68)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_stop(e):
    L = e.cl
    L.rec("stop", (s32(e.u32(e.r(3) + MAC["ci_script"])), e.u32(e.r(4) + MAC["tr_id"])))


def stub_rsse(e):
    L = e.cl
    L.rec("rsse", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("rsse") & MASK)


def stub_snd(e):
    L = e.cl
    L.rec("snd", tuple(s32(e.r(i)) for i in range(3, 10)))
    e.set_r(3, L.ret("snd") & MASK)


def stub_state(e):
    L = e.cl
    L.rec("state", (e.u32(e.r(3) + MAC["tr_id"]), e.r(4) & MASK, e.r(5) & MASK))


STUBS.internal(STOP, stub_stop)
STUBS.internal(RSSE, stub_rsse)
STUBS.internal(SND, stub_snd)
STUBS.internal(STATE, stub_state)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    O = MAC

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

    def wptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call_start(self, inst, train, flag):
        T.call_ppc(self.e, FN_START, [inst, train, flag & MASK])

    def call_all(self):
        T.call_ppc(self.e, FN_ALL, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.O
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lsc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lsc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call_start(self, inst, train, flag):
        self.lib.tpw_lsc_start(inst, train, s32(flag))

    def call_all(self):
        self.lib.tpw_lsc_all()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lsc_addr.restype, lib.tpw_lsc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lsc_alloc.restype, lib.tpw_lsc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lsc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lsc_start.argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_lsc_all.argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_lsc_layout(out)
    assert n == len(KEYS)
    lib.O = dict(zip(KEYS, list(out)[:n]))
    lib.bound = True


def build(s, case):
    O = s.O
    s.words(s.g("bank"), [case["bank"]])
    s.words(s.g("rec"), [case["rec"]])
    insts = []
    tid = 1
    for i, trains in enumerate(case["insts"]):
        a = s.alloc(O["ci_size"])
        s.words(a + O["ci_script"], [500 + i])
        s.words(a + O["ci_count"], [len(trains)])
        base = s.alloc(max(1, len(trains)) * O["tr_size"])
        s.wptr(a + O["ci_trains"], base)
        ts = []
        for j, (pos, old) in enumerate(trains):
            t = base + j * O["tr_size"]
            s.words(t + O["tr_id"], [tid])
            tid += 1
            s.words(t + O["tr_f48"], pos)
            s.words(t + O["tr_f54"], old[:4])
            s.words(t + O["tr_f64"], [old[4]])
            s.words(t + O["tr_f68"], [old[5]])
            ts.append(t)
        insts.append((a, ts))
    for i, (a, _) in enumerate(insts):
        s.wptr(a + O["ci_next"], insts[i + 1][0] if i + 1 < len(insts) else 0)
    s.wptr(s.g("head"), insts[0][0] if insts else 0)
    s.insts = insts


def execute(s, case):
    if case["op"] == "all":
        s.call_all()
    else:
        a, ts = s.insts[0]
        s.call_start(a, ts[0], case["flag"])


def observe(s, case):
    O = s.O
    out = []
    for a, ts in s.insts:
        for t in ts:
            out.append(s.rd_words(t + O["tr_f54"], 4) + s.rd_words(t + O["tr_f64"], 1) + s.rd_words(t + O["tr_f68"], 1))
    return {"log": s.log(), "trains": out}


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


def rfl(r):
    k = r.random()
    if k < 0.15:
        return r.choice([0, 0x80000000, fb(0.5), fb(-0.5), fb(0.99), fb(-0.99), fb(1.0), fb(-1.0), fb(2147483520.0), fb(-2147483648.0)])
    if k < 0.7:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1e6, 1e6))


def rw(r):
    return r.choice([0, 1, MASK, r.getrandbits(32)])


def train(r):
    return ([rfl(r) for _ in range(3)], [rw(r) for _ in range(6)])


def base(r, op, insts, rec=None):
    return {"op": op, "insts": insts, "flag": r.choice([0, 0, 1, -1, 5]), "rec": r.choice([0, 0, 1, 7]) if rec is None else rec,
            "bank": r.choice([0, 77, rw(r)]),
            "queues": {"rsse": [rw(r) for _ in range(5)], "snd": [rw(r) for _ in range(5)]}}


def start_case(r, rec=None, flag=None):
    c = base(r, "start", [[train(r)]], rec)
    if flag is not None:
        c["flag"] = flag
    return c


def all_case(r, rec=None, counts=None):
    if counts is None:
        counts = [r.randrange(0, 4) for _ in range(r.randrange(0, 4))]
    return base(r, "all", [[train(r) for _ in range(n)] for n in counts], rec)


def start_edges():
    r = random.Random(11)
    for rec in (0, 1):
        for flag in (0, 1, -1):
            yield start_case(r, rec, flag)


def all_edges():
    r = random.Random(12)
    for counts in ([], [0], [1], [2, 1], [0, 3, 0], [1, 1, 1]):
        for rec in (0, 1):
            yield all_case(r, rec, counts)


TESTS = [
    FnTest("StartTrainSounds", ppc, host, edge=start_edges, rnd=lambda r: start_case(r)),
    FnTest("LayoutStartAllCoasterSounds", ppc, host, edge=all_edges, rnd=lambda r: all_case(r)),
]
