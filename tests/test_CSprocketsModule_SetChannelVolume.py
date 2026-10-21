"""Unit v5250, the channel commands of CSprocketsModule (sound symbols shared), against libtpw_unit_CSprocketsModule_SetChannelVolume.dylib
(tests/unit_CSprocketsModule_SetChannelVolume.cpp), src/sound/CSprocketsModule.cpp:
  StopChannel 0x1002c068, StopAll 0x1002c238, PauseChannel 0x1002c314, PauseAll 0x1002c670, GetPosition 0x1002cfa8,
  SetListenerPosition 0x1002d090, SetChannelVolume 0x1002d5b4, SetSpeakerConfig 0x1002d6cc

A case is a world and a call. The world is built twice by ONE piece of code: in emulator memory at the Mac's addresses (big-endian) and in
the host library's memory (little-endian). The callees outside the unit are fakes on both sides that record their arguments: the command
constructors, SetImmediate, SetLocation, IfOk, the channel send (the Mac's glue 0x1002f154 with the channel at r4, the command at r5, the
status written to the buffer at r3), the speaker type of a configuration (the same glue, called with the configuration) and
TbSoundPosition::Get. TbError::TbError (0x100015b8) runs as the original; the hidden r3 buffer is read back.

The world of a case: the module (mBuffers, the channel array, six slots, some NULL, each channel tagged by a word at +4 that the send fake
logs), a sample info (volume at +0x1c), a speaker configuration (tag at +4), a position (three words). What is compared: the answer and the
event log. No fused operations. Not tested: a channel index past the six slots of the array (the original reads without a bound).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CSprocketsModule_SetChannelVolume"
MASK = 0xFFFFFFFF

FN = dict(StopChannel=0x1002c068, StopAll=0x1002c238, PauseChannel=0x1002c314, PauseAll=0x1002c670, GetPosition=0x1002cfa8,
          SetListenerPosition=0x1002d090, SetChannelVolume=0x1002d5b4, SetSpeakerConfig=0x1002d6cc)
SIZES = {0x1002c068: 164, 0x1002c238: 172, 0x1002c314: 168, 0x1002c670: 164, 0x1002cfa8: 68, 0x1002d090: 192, 0x1002d5b4: 200,
         0x1002d6cc: 176}
OWN = set(SIZES)

HOST_LAYOUT = [8, 36, 4]   # mBuffers, TbSoundSampleInfo::mVolume (native pointers move it from the Mac's 0x1c), sizeof(UStatus)
NSLOT = 6
CUR = {"cfg": 0}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(0x100015b8)

STUBS.internal(0x1002c1c4, CL.recorder("UCommandStop", "N"))
STUBS.internal(0x1002a5c4, CL.recorder("SetImmediate", "Lb"))
STUBS.internal(0x1002c3f4, CL.recorder("UCommandPause", "Nu"))
STUBS.internal(0x1002cdcc, CL.recorder("UCommandVolume", "Nf"))
STUBS.internal(0x1002d7cc, CL.recorder("UCommandSpeakerType", "Nui"))
STUBS.internal(0x1002cbc8, CL.recorder("SetLocation", "Lfff"))
STUBS.internal(0x1002d1a0, CL.recorder("UCommandListenerPosition", "Nu"))


def stub_ifok(e):
    L = e.cl
    L.rec("IfOk", (e.u32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("IfOk") & MASK)


def stub_get(e):
    L = e.cl
    w = struct.unpack(">3I", e.read(e.r(3), 12))
    L.rec("Get", w)
    for reg, key in ((4, "GetX"), (5, "GetY"), (6, "GetZ")):
        e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def stub_glue(e):
    L = e.cl
    if CUR["cfg"] and e.r(3) == CUR["cfg"]:
        L.rec("ConfigType", (e.u32(e.r(3) + 4),))
        e.set_r(3, L.ret("ConfigType") & MASK)
        return
    L.rec("Send", (e.u32(e.r(4) + 4), L.lab(e.r(5))))
    e.write(e.r(3), struct.pack(">I", L.ret("Send") & MASK))


STUBS.internal(0x1002c144, stub_ifok)
STUBS.internal(0x10015f3c, stub_get)
STUBS.internal(0x1002f154, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def read_all(self):
        return None


class Ppc(Side):
    fmt = ">I"
    off_buffers = 4
    off_volume = 0x1c

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        CUR["cfg"] = 0

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ptrs(self, a, ps):
        self.words(a, ps)

    def set_chan(self, m, arr):
        self.words(m + 0x14, [arr])

    def vtable(self, ch):
        self.words(ch + 0x10, [self.alloc(0x40)])

    def call(self, op, m, a, b):
        buf = self.alloc(4)
        entry = FN[op]
        args = {"StopChannel": [a], "StopAll": [], "PauseChannel": [a], "PauseAll": [], "GetPosition": [a, b],
                "SetListenerPosition": [b], "SetChannelVolume": [a, b], "SetSpeakerConfig": [b]}[op]
        T.call_ppc(self.e, entry, [buf, m] + args)
        return s32(struct.unpack(">I", self.e.read(buf, 4))[0])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off_buffers = HOST_LAYOUT[0]
        self.off_volume = HOST_LAYOUT[1]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_csm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptrs(self, a, ps):
        self.write(a, b"".join(struct.pack("<Q", p) for p in ps))

    def set_chan(self, m, arr):
        self.write(self.lib.tpw_csm_addr(b"channels"), struct.pack("<Q", arr))

    def vtable(self, ch):
        pass

    def call(self, op, m, a, b):
        f = getattr(self.lib, "tpw_csm_" + op)
        args = {"StopChannel": [a], "StopAll": [], "PauseChannel": [a], "PauseAll": [], "GetPosition": [a, b],
                "SetListenerPosition": [b], "SetChannelVolume": [a, b], "SetSpeakerConfig": [b]}[op]
        return f(m, *args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_csm_addr.restype, lib.tpw_csm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_csm_alloc.restype, lib.tpw_csm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_csm_layout.argtypes = [C.POINTER(C.c_int64)]
    u64, u32 = C.c_uint64, C.c_uint32
    sigs = dict(StopChannel=[u64, u32], StopAll=[u64], PauseChannel=[u64, u32], PauseAll=[u64], GetPosition=[u64, u32, u64],
                SetListenerPosition=[u64, u64], SetChannelVolume=[u64, u32, u64], SetSpeakerConfig=[u64, u64])
    for name, argt in sigs.items():
        f = getattr(lib, "tpw_csm_" + name)
        f.restype, f.argtypes = C.c_int32, argt
    out = (C.c_int64 * 32)()
    n = lib.tpw_csm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    """Returns (module, a, b): the operation's two arguments (channel; info, position or configuration)."""
    m = s.alloc(0x40)
    s.words(m + s.off_buffers, [case["nbuf"]])
    arr = s.alloc(8 * NSLOT)
    ps = []
    for i, present in enumerate(case["present"]):
        if present:
            ch = s.alloc(0x40)
            s.words(ch + 4, [0x1000 + i])
            s.vtable(ch)
            ps.append(ch)
        else:
            ps.append(0)
    s.ptrs(arr, ps)
    s.set_chan(m, arr)
    b = 0
    if case["op"] in ("SetChannelVolume",):
        b = s.alloc(0x90)
        s.words(b + s.off_volume, [case["volume"]])
    elif case["op"] == "SetListenerPosition" or case["op"] == "GetPosition":
        b = s.alloc(16)
        s.words(b, case["pos"])
    elif case["op"] == "SetSpeakerConfig":
        b = s.alloc(0x20)
        s.words(b + 4, [0x2000 + case["tag"]])
        if isinstance(s, Ppc):
            s.words(b, [s.alloc(0x40)])
            CUR["cfg"] = b
    return m, case["channel"], b


def run(s, case):
    try:
        m, a, b = build(s, case)
        ans = s.call(case["op"], m, a, b)
        return {"answer": s32(ans), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000])
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def base(r, op, present=None, nbuf=None, channel=None):
    c = {"op": op}
    c["present"] = [r.random() < 0.7 for _ in range(NSLOT)] if present is None else present
    c["nbuf"] = r.randrange(0, NSLOT + 1) if nbuf is None else nbuf
    c["channel"] = r.randrange(0, NSLOT) if channel is None else channel
    c["queues"] = {"Send": [r.choice([0, 1, -1, r.randrange(-100, 100)]) & MASK],
                   "IfOk": [r.choice([0, -1, 7, r.randrange(-100, 100)]) & MASK],
                   "GetX": [rf(r)], "GetY": [rf(r)], "GetZ": [rf(r)],
                   "ConfigType": [r.choice([0, 1, 2, 3, r.randrange(0, 100)])]}
    c["volume"] = r.choice([0, 1, 126, 127, 128, 254, 0xFFFFFFFF, 0x80000000, 0x01000001, 0x01000003, r.randrange(0, 1 << 32),
                            r.randrange(0, 300)])
    c["pos"] = [rf(r), rf(r), rf(r)]
    c["tag"] = r.randrange(0, 1000)
    return c


def make(op):
    def one(r, **kw):
        return base(r, op, **kw)

    def edges():
        r = random.Random(sum(map(ord, op)))
        full = [True] * NSLOT
        none = [False] * NSLOT
        for present in (full, none, [False, True, False, True, True, False]):
            for nbuf in (0, 1, 3, NSLOT):
                for channel in (0, 1, 2, 5):
                    yield one(r, present=list(present), nbuf=nbuf, channel=channel)
        for _ in range(8):
            yield one(r)

    return FnTest(op, ppc, host, edge=edges, rnd=lambda r: one(r))


TESTS = [make(op) for op in FN]
