"""Unit v5020, the upgrade effect of a ride script, against libtpw_unit_RSSE_UpgradeRideEffect.dylib (tests/unit_RSSE_UpgradeRideEffect.cpp),
main app ("SimTheme Park"), src/rsse_objects_RSSE_UpgradeRideEffect.cpp:
  RSSE_UpgradeRideEffect 0x100d67e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (GAME_ASSERTMSG, the ride database's
GetRideInfo, GetRideMasterBoundArea, PAR_InitEmitter, PAR_SetEmitterRandomPos, SOUND_SendEvent) are fakes on both sides that record every
argument and answer from the case.

The world of a case: the script list (the target among others), the skipped thing id, the thing table, the ride anim sets (a chain down to
the float at node +0x44), the ride record's emitter word (+0x6c), the sound bank. What is compared: the event log (every call and argument).

No fused operations; fctiwz stays far below 2^31. Not tested: the system not initialised or an unknown handle (the original then reads
a NULL script).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RSSE_UpgradeRideEffect"
MASK = 0xFFFFFFFF

FN = 0x100D67E0
GETRIDEINFO = 0x10154868
BOUND = 0x101ED058
INITEMITTER = 0x100BEC30
SETRANDOMPOS = 0x100BE3F4
SENDEVENT = 0x100DD9FC
ASSERT = 0x1000DDE8

ADDR = dict(state=0x102F7BEC, skip=0x1029F152, bank2=0x102F7C9C, db=0x1037F6C8, things=0x1034CBFC, anims=0x102C79A4)
HOST_LAYOUT = [0x08, 0xAC, 0xC8, 0xF4]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_getrideinfo(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.info_block)


def stub_bound(e):
    L = e.cl
    ans = s32(L.ret("Bound"))
    L.rec("Bound", (e.r(3), ans))
    if ans != 0:
        vals = [L.ret("BoundF") & MASK for _ in range(6)]
        e.write(e.r(4), b"".join(struct.pack(">I", v) for v in vals))
    e.set_r(3, ans & MASK)


STUBS.internal(GETRIDEINFO, stub_getrideinfo)
STUBS.internal(BOUND, stub_bound)
STUBS.internal(INITEMITTER, CL.recorder("PAR_InitEmitter", "iiii", ret="i"))
STUBS.internal(SETRANDOMPOS, CL.recorder("PAR_SetEmitterRandomPos", "iiii"))
STUBS.internal(SENDEVENT, CL.recorder("SOUND_SendEvent", "iiiiii"))
STUBS.internal(ASSERT, CL.recorder("Assert", "is"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["db"]: 1}, case["queues"])
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

    def reset(self):
        for i in range(32):
            self.words(ADDR["things"] + 0x14 * i, [0] * 5)
            self.words(ADDR["anims"] + 4 * i, [0])

    def head(self, script):
        self.words(ADDR["state"] + 0x10, [script])

    def link(self, a, b):
        self.words(a, [b])

    def set_thing(self, i, typ):
        a = self.alloc(0x40)
        self.half(a + 0xE, typ)
        self.words(ADDR["things"] + 0x14 * i, [a])

    def set_info(self, a):
        self.e.info_block = a

    def make_anim(self, idx, bits):
        node = self.alloc(0x100)
        self.words(node + 0x44, [bits])
        header = self.alloc(0x100)
        self.words(header + 0x78, [node])
        mesh = self.alloc(0x40)
        self.words(mesh + 4, [header])
        st = self.alloc(0x100)
        self.words(st + 8, [mesh])
        self.words(ADDR["anims"] + 4 * idx, [st])

    def call(self, handle):
        T.call_ppc(self.e, FN, [handle & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("db")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ur_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ur_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def reset(self):
        self.lib.tpw_ur_reset()

    def head(self, script):
        self.lib.tpw_ur_set_head(script)

    def link(self, a, b):
        self.lib.tpw_ur_link(a, b)

    def set_thing(self, i, typ):
        self.lib.tpw_ur_set_thing(i, typ)

    def set_info(self, a):
        self.lib.tpw_ur_set_info(a)

    def make_anim(self, idx, bits):
        self.lib.tpw_ur_make_anim(idx, bits)

    def call(self, handle):
        self.lib.tpw_ur_call(s32(handle))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ur_addr.restype, lib.tpw_ur_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ur_alloc.restype, lib.tpw_ur_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ur_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ur_reset.argtypes = []
    lib.tpw_ur_set_head.argtypes = [C.c_uint64]
    lib.tpw_ur_link.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_ur_set_thing.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ur_set_info.argtypes = [C.c_uint64]
    lib.tpw_ur_make_anim.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ur_call.argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ur_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.reset()
    s.half(s.g("skip"), case["skip"])
    s.words(s.g("state"), [1, 0, 1, len(case["scripts"])])
    s.words(s.g("bank2"), [case["bank"]])
    prev = 0
    addrs = []
    for sc in case["scripts"]:
        a = s.alloc(0xF4)
        s.words(a + 8, [sc["handle"]])
        s.half(a + 0xAC, sc["thing"])
        s.words(a + 0xC8, [sc["anim"]])
        addrs.append(a)
    s.head(addrs[0])
    for a, b in zip(addrs, addrs[1:]):
        s.link(a, b)
    for i, typ in case["things"].items():
        s.set_thing(i, typ)
    info = s.alloc(0x100)
    s.words(info + 0x6C, [case["emitter"]])
    s.set_info(info)
    for idx, bits in case["anims"].items():
        s.make_anim(idx, bits)


def run(s, case):
    try:
        build(s, case)
        s.call(case["handle"])
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rbox(r):
    return [fb(r.choice([r.uniform(-20, 20), r.uniform(-300, 300), 0.5, -0.5, 0.0, 1.0])) for _ in range(6)]


def make(r, ok1=None, ok2=None, anim=None, same=None, emitter=None, row0=None):
    n = r.randint(1, 4)
    handles = r.sample(range(1, 40), n)
    skip = r.choice([0, 5, r.randrange(1, 20)])
    k = r.randrange(n)
    target_thing = r.randrange(1, 20)
    if same is None:
        same = r.random() < 0.2
    if same:
        skip = target_thing
    elif skip == target_thing:
        skip = target_thing + 1
    scripts = []
    for i, h in enumerate(handles):
        thing = target_thing if i == k else r.choice([skip, r.randrange(1, 20)])
        if anim is None:
            a = r.choice([0, 0, r.randrange(1, 9)])
        else:
            a = anim if i == k else r.randrange(0, 9)
        scripts.append({"handle": h, "thing": thing, "anim": a})
    things = {i: r.randrange(0, 0x400) for i in range(1, 20)}
    if row0 if row0 is not None else r.random() < 0.1:
        things[0] = r.randrange(0, 0x400)
    if ok1 is None:
        ok1 = r.choice([0, 1, 1, 2])
    if ok2 is None:
        ok2 = r.choice([0, 1, 1, 3])
    return {
        "handle": handles[k], "skip": skip, "scripts": scripts, "things": things, "bank": r.randrange(-5, 1000),
        "emitter": r.choice([0, r.randrange(1, 2000)]) if emitter is None else emitter,
        "anims": {i: fb(r.choice([r.uniform(-20, 20), 0.5, -0.5, 0.0, r.uniform(-300, 300)])) for i in range(1, 9)},
        "queues": {"Bound": [ok1, ok2], "BoundF": rbox(r) + rbox(r), "PAR_InitEmitter": [r.randrange(0, 1000)]},
    }


def edges():
    r = random.Random(20)
    for ok1 in (0, 1):
        for ok2 in (0, 1):
            for anim in (0, 3):
                for same in (False, True):
                    for emitter in (0, 7):
                        yield make(r, ok1, ok2, anim, same, emitter, False)
    yield make(r, 1, 1, 3, False, 7, True)
    yield make(r, 1, 1, 3, False, 7, True)


def rnd(r):
    return make(r)


TESTS = [FnTest("RSSE_UpgradeRideEffect", ppc, host, edge=edges, rnd=rnd)]
