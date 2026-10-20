"""Unit v5368, OnlineConfig::~OnlineConfig 0x10003508 (892 B), against libtpw_unit_OnlineConfig_Dtor_OnlineConfig.dylib
(tests/unit_OnlineConfig_Dtor_OnlineConfig.cpp), main app ("SimTheme Park"), src/FS_Registry_OnlineConfig_Dtor_OnlineConfig.cpp.

The host OnlineConfig holds real std::strings, so the translation (online_config_mac::Destroy) works on the Mac's object: words of 32-bit
addresses, built in both memories by one piece of Python code. The world is a fixed pool (allocated once per side, so the addresses are
stable labels): the object (0x28 bytes), 7 string reps (len, cap, refs, data pointer) and 7 data blocks. A case points each of the seven
string members (+0 +4 +8 +0xc +0x10 +0x14 +0x24) at a rep (sometimes shared), sets the reference counts, and gives each rep a data block
or NULL. operator delete is a fake on both sides recording its pointer as a label. Compared: the answer (this or NULL), the event log,
the object's words (rep addresses as labels) and the reps' words.

No fused operations. Not tested: a string member holding NULL (the original reads the rep unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineConfig_Dtor_OnlineConfig"
MASK = 0xFFFFFFFF

FN = 0x10003508
OPERATOR_DELETE = 0x10238218
SLOTS = [0x24, 0x14, 0x10, 0xC, 0x8, 0x4, 0x0]
N = 7

POOL = {}          # side name -> (obj, [reps], [datas])


def pool_labels(p):
    obj, reps, datas = p
    d = {obj: 1}
    for i in range(N):
        d[reps[i]] = 10 + i
        d[datas[i]] = 20 + i
    return d


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(OPERATOR_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    POOL["ppc"] = (T.persist_alloc(e, 0x28), [T.persist_alloc(e, 0x10) for _ in range(N)], [T.persist_alloc(e, 0x10) for _ in range(N)])
    STUBS.internal(OPERATOR_DELETE, stub_delete)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def label_of(self, w):
        return self.labels.get(w, -1 if w else 0)

    def put_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def get_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def slot(self, i):
        return self.pool[0] + i * self.psz


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    dataoff = 0xC

    def __init__(self, e, case):
        self.e = e
        self.pool = POOL["ppc"]
        self.labels = pool_labels(self.pool)
        self.L = e.cl = CL.PpcLog(dict(self.labels), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, obj, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK, flag & 0xFFFF])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    dataoff = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if "host" not in POOL:
            POOL["host"] = (lib.tpw_oc_alloc(0x50), [lib.tpw_oc_alloc(0x20) for _ in range(N)], [lib.tpw_oc_alloc(0x10) for _ in range(N)])
        self.pool = POOL["host"]
        self.labels = pool_labels(self.pool)
        CL.host_begin(lib, {v: k for k, v in self.labels.items()}, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, flag):
        return self.lib.tpw_oc_dtor(obj, flag & 0xFFFF)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_oc_alloc.restype, lib.tpw_oc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_oc_dtor.restype, lib.tpw_oc_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_oc_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 4)()
    assert lib.tpw_oc_layout(out) == 0
    lib.bound = True


def build(s, case):
    obj, reps, datas = s.pool
    for i in (6, 7, 8):
        s.words(s.slot(i), [case["fill"][i]])
    for j, off in enumerate(SLOTS):
        s.put_ptr(s.slot(off // 4), reps[case["slot"][j]])
    for i in range(N):
        d = case["data"][i]
        s.words(reps[i], [case["len"][i], case["cap"][i], case["refs"][i]])
        s.put_ptr(reps[i] + s.dataoff, datas[d] if d >= 0 else 0)
        s.words(datas[i], [0xD0 + i, 0, 0, 0])


def run(s, case):
    try:
        build(s, case)
        obj, reps, datas = s.pool
        this = 0 if case["null"] else obj
        ans = s.call(this, case["flag"])
        return {
            "answer": s.label_of(ans),
            "log": s.log(),
            "obj": [s.label_of(s.get_ptr(s.slot(i))) for i in (0, 1, 2, 3, 4, 5, 9)] + [s.rd_words(s.slot(i), 1)[0] for i in (6, 7, 8)],
            "reps": [s.rd_words(r, 3) + [s.label_of(s.get_ptr(r + s.dataoff))] for r in reps],
        }
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


REFS = [0, 1, 2, 3, 0xFFFFFFFF, 0x80000000]
FLAGS = [0, 1, -1, 5, 0x7FFF, -0x8000, 0x10001, 0x18000, 0xFFFF0001]


def make(r, null=False, flag=None, slot=None, refs=None, data=None):
    return {
        "queues": {},
        "null": null,
        "flag": r.choice(FLAGS) if flag is None else flag,
        "fill": [r.getrandbits(32) for _ in range(10)],
        "slot": list(slot) if slot is not None else ([r.randrange(N) for _ in range(N)] if r.random() < 0.3 else list(range(N))),
        "refs": list(refs) if refs is not None else [r.choice(REFS) for _ in range(N)],
        "data": list(data) if data is not None else [r.choice([-1, r.randrange(N)]) for _ in range(N)],
        "len": [r.getrandbits(32) for _ in range(N)],
        "cap": [r.getrandbits(32) for _ in range(N)],
    }


def edges():
    r = random.Random(5368)
    yield make(r, null=True)
    yield make(r, null=True, flag=1)
    for flag in (0, 1, -1, 0x7FFF, -0x8000, 0x10000, 0x18000, 0x8000):
        yield make(r, flag=flag, refs=[1] * N, data=list(range(N)))
    for v in REFS:
        yield make(r, refs=[v] * N, data=list(range(N)))
    yield make(r, refs=[2] * N, data=[-1] * N)
    yield make(r, refs=[0] * N, data=[-1] * N)
    yield make(r, refs=[1] * N, slot=[0] * N, data=list(range(N)))
    yield make(r, refs=[2] * N, slot=[0] * N, data=list(range(N)))
    for k in range(N):
        yield make(r, refs=[2 if i != k else 1 for i in range(N)], flag=1)
        yield make(r, refs=[1 if i != k else 2 for i in range(N)], flag=0)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("OnlineConfig::~OnlineConfig", ppc, host, edge=edges, rnd=rnd),
]
