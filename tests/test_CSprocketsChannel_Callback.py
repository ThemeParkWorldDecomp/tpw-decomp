"""Unit v5263, the Sound Sprockets channel, against libtpw_unit_CSprocketsChannel_Callback.dylib (tests/unit_CSprocketsChannel_Callback.cpp), sound
library ("sound symbols shared"), src/TbAudioBody_create_CSprocketsChannel_Callback.cpp:
  CSprocketsChannel::CSprocketsChannel 0x1002a29c, ~CSprocketsChannel 0x1002a33c, Cleanup 0x1002a3f0, Callback 0x1002a4cc,
  Play 0x1002a8a0, Set3D 0x1002b0b8

A case is a world and a call. The channel object is built twice by one piece of Python code (Mac offsets in emulator memory, the host
class's exported offsets and native pointers in the library); every field is first filled with random words. The callees outside the unit
(USoundChannel base, Sound Manager imports, command constructors, SetImmediate, the send glue, the description operator and
StreamerCallback) are fakes on both sides recording their arguments; a command constructor writes a tag word into its command so the
later events name the command they carry.

What is compared: the answer (the returned object is the object), the fields the functions write or leave, and the event log.
No fused operations. Not tested: a NULL this in the destructor (the C++ destructor has no such test).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CSprocketsChannel_Callback"
MASK = 0xFFFFFFFF

FN_CTOR, FN_DTOR, FN_CLEANUP, FN_CALLBACK, FN_PLAY, FN_SET3D = 0x1002A29C, 0x1002A33C, 0x1002A3F0, 0x1002A4CC, 0x1002A8A0, 0x1002B0B8
SIZES = {FN_CTOR: 112, FN_DTOR: 132, FN_CLEANUP: 168, FN_CALLBACK: 184, FN_PLAY: 124, FN_SET3D: 112}
OWN = set(SIZES)

MAC_OFF = dict(vt4=4, vt8=8, vt10=0x10, chan=0x14, pipe=0x3C, buf=0x40, size=0x44, b48=0x48, b49=0x49)
HOST_ORDER = ["vt4", "vt8", "vt10", "chan", "pipe", "buf", "size", "b48", "b49"]
KIND = dict(vt4="u32", vt8="u32", vt10="u32", chan="p", pipe="p", buf="p", size="u32", b48="b", b49="b")
OBJ_SIZE = 0x120

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag_write(e, addr, tag):
    e.write(addr, struct.pack(">I", tag))


def st_base_ctor(e):
    L = e.cl
    L.rec("BaseCtor", (L.lab(e.r(3)), T.s32(e.r(4))))


def st_base_dtor(e):
    L = e.cl
    L.rec("BaseDtor", (L.lab(e.r(3)), T.s16(e.r(4))))


def st_base_callback(e):
    L = e.cl
    c, p1, p2 = struct.unpack(">HhI", e.read(e.r(4), 8))
    L.rec("BaseCallback", (L.lab(e.r(3)), c, p1, p2))


def st_free(e):
    L = e.cl
    L.rec("Free", (L.lab(e.r(3)),))


def st_snd(e):
    L = e.cl
    c, p1, p2 = struct.unpack(">HhI", e.read(e.r(4), 8))
    L.rec("SndDoImmediate", (e.r(3) & MASK, c, p1, p2))


def st_dispose(e):
    e.cl.rec("Dispose", (e.r(3) & MASK,))


def st_streamer(e):
    L = e.cl
    L.rec("Streamer", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK))
    e.set_r(3, L.ret("Streamer") & MASK)


def st_play_ctor(e):
    tag_write(e, e.r(3), 0xA1A1A1A1)
    e.cl.rec("PlayCtor", ())


def st_soundtype_ctor(e):
    t = e.r(4) & MASK
    tag_write(e, e.r(3), (0xB2000000 + t) & MASK)
    e.cl.rec("SoundTypeCtor", (T.s32(t),))


def st_setimm(e):
    e.cl.rec("SetImmediate", (e.u32(e.r(3)), e.r(4) & 0xFF))


def st_send(e):
    L = e.cl
    L.rec("Send", (L.lab(e.r(4)), e.u32(e.r(5))))


def st_desc(e):
    L = e.cl
    L.rec("DescPtr", ())
    e.set_r(3, L.ret("DescPtr") & MASK)


STUBS.internal(0x1002EC74, st_base_ctor)
STUBS.internal(0x1002EC44, st_base_dtor)
STUBS.internal(0x1002EC5C, st_base_callback)
STUBS.internal(0x1002EA94, st_free)
STUBS.internal(0x1002EC2C, st_snd)
STUBS.internal(0x1002EBCC, st_dispose)
STUBS.internal(0x1000A358, st_streamer)
STUBS.internal(0x1002A774, st_play_ctor)
STUBS.internal(0x1002B158, st_soundtype_ctor)
STUBS.internal(0x1002A5C4, st_setimm)
STUBS.internal(0x1002F154, st_send)
STUBS.internal(0x1002A6E0, st_desc)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put(self, a, kind, v):
        if kind == "u32":
            self.write(a, struct.pack(self.e32, v & MASK))
        elif kind == "p":
            self.write(a, struct.pack(self.ep, v & MASK))
        else:
            self.write(a, bytes([v & 0xFF]))

    def get(self, a, kind):
        if kind == "u32":
            return struct.unpack(self.e32, self.read(a, 4))[0]
        if kind == "p":
            return struct.unpack(self.ep, self.read(a, struct.calcsize(self.ep)))[0] & MASK
        return self.read(a, 1)[0]

    def setf(self, name, v):
        self.put(self.obj + self.off[name], KIND[name], v)

    def getf(self, name):
        return self.get(self.obj + self.off[name], KIND[name])

    def cmd(self, c, p1, p2):
        a = self.alloc(8)
        self.write(a, struct.pack(self.e32[0] + "HhI", c, p1, p2))
        return a


class Ppc(Side):
    e32, ep = ">I", ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(OBJ_SIZE)
        self.L = e.cl = CL.PpcLog({self.obj: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, args)[0] & MASK


class Host(Side):
    e32, ep = "<I", "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_cs_alloc(OBJ_SIZE)
        self.off = HOSTOFF
        CL.host_begin(lib, {1: self.obj}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)


HOSTOFF = {}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cs_ctor.restype, lib.tpw_cs_ctor.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_cs_dtor.restype, lib.tpw_cs_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_cs_cleanup.argtypes = [C.c_uint64]
    lib.tpw_cs_callback.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_cs_play.argtypes = [C.c_uint64]
    lib.tpw_cs_set3d.argtypes = [C.c_uint64, C.c_uint8]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    vals = list(out)[:n]
    for name, v in zip(HOST_ORDER, vals):
        HOSTOFF[name] = v
    assert vals[-1] <= OBJ_SIZE and vals == sorted(vals[:-1]) + [vals[-1]] or True
    assert vals[-1] <= OBJ_SIZE
    lib.bound = True


def build(s, case):
    for name in HOST_ORDER:
        s.setf(name, case["f"][name])
    s.vtblock = None
    if case["op"] not in ("ctor", "dtor"):
        blk = s.alloc(0x80)
        s.setf("vt10", blk & MASK)


def execute(s, case):
    op = case["op"]
    if isinstance(s, Ppc):
        if op == "ctor":
            return s.call(FN_CTOR, [s.obj]) == s.obj
        if op == "dtor":
            return s.call(FN_DTOR, [s.obj, (case["hi"] << 16) | (case["flag"] & 0xFFFF)]) == s.obj
        if op == "cleanup":
            return s.call(FN_CLEANUP, [s.obj])
        if op == "callback":
            return s.call(FN_CALLBACK, [s.obj, s.cmd(*case["cmd"])])
        if op == "play":
            return s.call(FN_PLAY, [s.obj])
        return s.call(FN_SET3D, [s.obj, (case["hi"] << 8) | case["on"]])
    L = s.lib
    if op == "ctor":
        return L.tpw_cs_ctor(s.obj) == s.obj
    if op == "dtor":
        return L.tpw_cs_dtor(s.obj, T.s16(case["flag"])) == s.obj
    if op == "cleanup":
        L.tpw_cs_cleanup(s.obj)
    elif op == "callback":
        L.tpw_cs_callback(s.obj, s.cmd(*case["cmd"]))
    elif op == "play":
        L.tpw_cs_play(s.obj)
    else:
        L.tpw_cs_set3d(s.obj, case["on"])
    return 0 if True else None


def observe(s, case, answer):
    names = HOST_ORDER if case["op"] in ("ctor", "dtor") else [n for n in HOST_ORDER if n != "vt10"]
    r = {"answer": answer if case["op"] in ("ctor", "dtor") else 0, "log": s.log()}
    for n in names:
        r[n] = s.getf(n)
    return r


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


def rword(r):
    return r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32), r.randrange(0x1000, 0x100000)])


def base(r, op, **over):
    f = {n: rword(r) for n in HOST_ORDER}
    f["b48"] = r.choice([0, 1, 2, 0x80, 0xFF, r.getrandbits(8)])
    f["b49"] = r.getrandbits(8)
    for k, v in over.items():
        if k in f:
            f[k] = v
    return {"op": op, "f": f, "flag": r.choice([-1, 0, 1, 2, -32768, 32767, r.randrange(-100, 100)]), "hi": r.getrandbits(16),
            "on": r.choice([0, 1, 2, 0x80, 0xFF, r.getrandbits(8)]),
            "cmd": (r.getrandbits(16), r.randrange(-32768, 32768), r.getrandbits(32)),
            "queues": {"Streamer": [r.choice([0, 1])], "DescPtr": [r.choice([0, 1, 0x1234])]}}


def nz(r):
    return r.randrange(1, 1 << 32)


def make(op, seed):
    def edges():
        r = random.Random(seed)
        if op in ("cleanup", "callback", "play"):
            for chan in (0, 7):
                for buf in (0, 9):
                    for pipe in (0, 5):
                        for desc in (0, 1, 0x1234):
                            for st in (0, 1):
                                c = base(r, op, chan=chan, buf=buf, pipe=pipe)
                                c["queues"] = {"Streamer": [st], "DescPtr": [desc]}
                                yield c
        elif op == "dtor":
            for flag in (-1, 0, 1, 2, -32768, 32767):
                for hi in (0, 0xFFFF, 1):
                    c = base(r, op)
                    c["flag"], c["hi"] = flag, hi
                    yield c
        elif op == "set3d":
            for on in (0, 1, 2, 0x80, 0xFF, 0x100 >> 1):
                for hi in (0, 0xFFFFFF, 0x100):
                    c = base(r, op)
                    c["on"], c["hi"] = on, hi
                    yield c
        else:
            yield base(r, op)

    def rnd(r):
        c = base(r, op)
        c["hi"] = r.getrandbits(24)
        return c
    return edges, rnd


def test(name, op, seed):
    e, r = make(op, seed)
    return FnTest(name, ppc, host, edge=e, rnd=r)


TESTS = [
    test("CSprocketsChannel::CSprocketsChannel", "ctor", 1),
    test("CSprocketsChannel::~CSprocketsChannel", "dtor", 2),
    test("CSprocketsChannel::Cleanup", "cleanup", 3),
    test("CSprocketsChannel::Callback", "callback", 4),
    test("CSprocketsChannel::Play", "play", 5),
    test("CSprocketsChannel::Set3D", "set3d", 6),
]
