"""Unit v5997, CSprocketsModule's constructor, Close, Tick, StreamData, SetChannelInfo and SetListenerDirection, against
libtpw_unit_CSprocketsModule_SetListenerDirection_2.dylib (tests/unit_CSprocketsModule_SetListenerDirection_2.cpp), "sound symbols shared",
src/TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_CSprocketsModule_SetListenerDirection_2.cpp:
  CSprocketsModule::CSprocketsModule 0x1002b234, Tick 0x1002b8c4, Close 0x1002bde8, StreamData 0x1002bf20,
  SetChannelInfo 0x1002c820, SetListenerDirection 0x1002d220

A case is a world and a call. The world is built twice by one piece of Python code: in emulator memory (big-endian) for the original and
in the host library (the channel objects, the array and the sample info are made by the library, the pipes and positions are raw blocks
written through Side.words). The callees outside the unit are fakes on both sides that record their arguments: the pool top-ups, the
pipe's GetIndex / GetSampleFlags / GetSampleHeader, TbSoundPosition::Get, the channel's Play / GetDescription / SetDescription, the
commands (constructors, SetImmediate, SetVelocity ...), the send glue 0x1002f154 (also the deleting destructor of a channel), IfOk,
IsSet, the allocator and the speaker. Run as the original on the PowerPC side: TbError's constructor, UStatus's constructor and
assignment, CSprocketsChannel::SetPipe and Is3D (the host's inline members); Set3D is a stub that stores the byte.

What is compared: the answer, the call log, the sample info's mask after SetChannelInfo, every channel's pipe pointer (as 0 / the
argument / the initial value / other) and 3D byte, the module's words after the constructor. Close deletes the channels, so only its
log is compared.

No fused operations (the volume is fsubs / fdivs). Not tested: a NULL channel array in StreamData, SetChannelInfo and
SetListenerDirection (the original reads it unconditionally), a channel index past the eight entries, a NULL pipe in SetChannelInfo when
0x2000 is set (it is dereferenced).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CSprocketsModule_SetListenerDirection_2"
MASK = 0xFFFFFFFF

FN_CTOR, FN_TICK, FN_CLOSE, FN_STREAM, FN_INFO, FN_DIR = 0x1002B234, 0x1002B8C4, 0x1002BDE8, 0x1002BF20, 0x1002C820, 0x1002D220
SIZES = {FN_CTOR: 108, FN_TICK: 112, FN_CLOSE: 160, FN_STREAM: 188, FN_INFO: 760, FN_DIR: 280}
OWN = set(SIZES)

SLOT_DTOR, SLOT_SEND = 0xD0001, 0xD0002
INIT_PIPE = 0x1234
HOST_LAYOUT = [24, 24]   # sizeof(CSprocketsModule), sizeof(TbSampleHeader) (its pointer is native)

ST = {"module": 0, "alloc": 0, "array": 0}


def s32(v):
    return T.s32(v)


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def d_halves(x):
    b = T.f64_bits(x)
    if ((b >> 52) & 0x7FF) == 0x7FF and (b & ((1 << 52) - 1)):
        b = 0x7FF8000000000000
    return (b >> 32) & MASK, b & MASK


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


def u32(e, a):
    return e.u32(a) & MASK


def wr(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


# TbSoundSystemModule's constructor (0x100122f0) runs as the original on this side and as the translation on the host.

@stub(0x1002EAC4)
def allocator(e):
    size = s32(e.r(3))
    e.cl.rec("Alloc", (size,))
    if e.cl.ret("AllocNull"):
        ST["alloc"] = 0
        e.set_r(3, 0)
    else:
        ST["alloc"] = e.alloc(max(size, 8))
        e.set_r(3, ST["alloc"])


@stub(0x1002B2D0)
def speaker_ctor(e):
    e.cl.rec("SpeakerCtor", (int(e.r(3) == ST["alloc"]),))


@stub(0x100149E8)
def set_module_speaker(e):
    p = e.r(3)
    e.cl.rec("SetModuleSpeaker", (0 if p == 0 else (1 if p == ST["alloc"] else 2),))


def topup(addr, n):
    STUBS.internal(addr, lambda e: e.cl.rec("TopUp", (n,)))


for _a, _n in ((0x1000E328, 1), (0x1000CE78, 2), (0x100024F4, 3), (0x10007860, 4), (0x100135E8, 5), (0x10013C54, 6),
               (0x1000EFB8, 7), (0x1000ADC0, 8), (0x1000F304, 9)):
    topup(_a, _n)


@stub(0x1002B964)
def maintain(e):
    e.cl.rec("MaintainPoolLevel", (0,))


@stub(0x10003C58)
def clear_pool(e):
    e.cl.rec("ClearPool", (e.r(3) & 0xFF,))


@stub(0x1002ECBC)
def idle(e):
    e.cl.rec("Idle", (0,))


@stub(0x1002EBCC)
def dispose(e):
    e.cl.rec("DisposeBuffer", (int(ST["array"] != 0 and e.r(3) == ST["array"]),))


@stub(0x1002F154)
def glue(e):
    L = e.cl
    wr(e, e.r(1) + 0x14, e.r(2))   # the glue saves the caller's TOC pointer, the caller reloads it (lwz r2, 0x14(r1))
    if e.r(12) == SLOT_DTOR:
        ch = e.r(3)
        L.rec("ChannelDtor", (u32(e, ch + 0xFC), s32(e.r(4))))
    else:
        out, ch, cmd = e.r(3), e.r(4), e.r(5)
        L.rec("Send", (u32(e, ch + 0xFC), u32(e, cmd)))
        wr(e, out, L.ret("Send"))


@stub(0x1002C144)
def if_ok(e):
    e.cl.rec("IfOk", (s32(u32(e, e.r(3))), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, e.cl.ret("IfOk") & MASK)


@stub(0x1002CB7C)
def is_set(e):
    e.cl.rec("IsSet", (s32(u32(e, e.r(3))),))
    e.set_r(3, 1 if e.cl.ret("IsSet") else 0)


def command(addr, name, kind, with_arg=True):
    def fn(e):
        wr(e, e.r(3), kind)
        e.cl.rec(name, (e.r(4) & MASK,))
    STUBS.internal(addr, fn)


command(0x1002D1A0, "ListenerPosition", 4)
command(0x1002CEAC, "Preroll", 1)
command(0x1002CCD0, "Position", 3)


@stub(0x1002CDCC)
def volume_ctor(e):
    wr(e, e.r(3), 2)
    e.cl.rec("Volume", d_halves(e.f(1)))


@stub(0x1002A5C4)
def set_immediate(e):
    e.cl.rec("SetImmediate", (u32(e, e.r(3)), e.r(4) & 0xFF))


def vec3(addr, name):
    def fn(e):
        h1, l1 = d_halves(e.f(1))
        h2, l2 = d_halves(e.f(2))
        h3, l3 = d_halves(e.f(3))
        e.cl.rec(name, (u32(e, e.r(3)), h1, l1, h2, l2, h3, l3))
    STUBS.internal(addr, fn)


vec3(0x1002CBC8, "SetLocation")
vec3(0x1002CC4C, "SetVelocity")
vec3(0x1002D420, "SetOrientation")
vec3(0x1002D39C, "SetUpVector")


@stub(0x10015F3C)
def position_get(e):
    src = e.r(3)
    ws = [u32(e, src + 4 * i) for i in range(3)]
    e.cl.rec("PositionGet", tuple(canon_f(w) for w in ws))
    for i in range(3):
        wr(e, e.r(4 + i), ws[i])


@stub(0x1000A19C)
def get_index(e):
    e.cl.rec("GetIndex", (u32(e, e.r(3) + 0x70),))
    e.set_r(3, e.cl.ret("GetIndex") & MASK)


@stub(0x1002C024)
def get_sample_flags(e):
    e.cl.rec("GetSampleFlags", (u32(e, e.r(3) + 0x70),))
    e.set_r(3, e.cl.ret("GetSampleFlags") & MASK)


@stub(0x1002CF24)
def get_sample_header(e):
    e.cl.rec("GetSampleHeader", (u32(e, e.r(3) + 0x70),))
    e.set_r(3, e.r(3) + 0x40)


@stub(0x1002A99C)
def get_description(e):
    e.cl.rec("GetDescription", (u32(e, e.r(3) + 0xFC),))
    e.set_r(3, e.cl.ret("GetDescription") & MASK)


@stub(0x1002A8A0)
def play(e):
    e.cl.rec("Play", (u32(e, e.r(3) + 0xFC),))


@stub(0x1002A9F8)
def set_description(e):
    h = e.r(4)
    e.cl.rec("SetDescription", (u32(e, e.r(3) + 0xFC), u32(e, h), u32(e, h + 8), u32(e, h + 12)))


@stub(0x1002B0B8)
def set_3d(e):
    e.write(e.r(3) + 0x48, bytes([e.r(4) & 0xFF]))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        ST.update(module=0, alloc=0, array=0)
        self.arr = 0

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

    def mk_world(self, case):
        mod = self.alloc(0x20)
        ST["module"] = self.module = mod
        self.words(mod + 4, [case["buffers"], 0x11111111, 0x22222222, 0xAAAAAAAA])
        arr = 0
        if case["has_array"]:
            vt = self.alloc(0x40)
            self.words(vt + 0x28, [SLOT_DTOR, SLOT_SEND])
            arr = self.alloc(0x20)
            for i in range(8):
                if (case["mask"] >> i) & 1:
                    ch = self.alloc(0x110)
                    is3d, pipe = case["chans"][i]
                    self.words(ch + 0x10, [vt])
                    self.words(ch + 0x3C, [pipe])
                    self.write(ch + 0x48, bytes([is3d]))
                    self.words(ch + 0xFC, [i])
                    self.words(arr + 4 * i, [ch])
        self.words(mod + 0x14, [arr])
        ST["array"] = self.arr = arr

    def chan_state(self, i):
        ch = self.rd_word(self.arr + 4 * i)
        if ch == 0:
            return None
        return self.rd_word(ch + 0x3C), self.read(ch + 0x48, 1)[0]

    def mk_info(self, f0c, flags, volume, pos, vel):
        a = self.alloc(0x40)
        self.words(a + 0xC, [f0c])
        self.words(a + 0x14, [flags])
        self.words(a + 0x1C, [volume])
        self.words(a + 0x2C, [pos])
        self.words(a + 0x30, [vel])
        self.info = a
        return a

    def info_flags(self):
        return self.rd_word(self.info + 0x14)

    def module_words(self):
        return self.rd_words(self.module + 4, 3)

    def call_err(self, entry, args):
        buf = self.alloc(4)
        self.words(buf, [0xDEADBEEF])
        T.call_ppc(self.e, entry, [buf, self.module] + args)
        return s32(self.rd_word(buf))

    def c_ctor(self):
        r3, _, _ = T.call_ppc(self.e, FN_CTOR, [self.module])
        return int(r3 & MASK == self.module)

    def c_close(self):
        T.call_ppc(self.e, FN_CLOSE, [self.module])

    def c_tick(self):
        T.call_ppc(self.e, FN_TICK, [self.module])

    def c_stream(self, pipe):
        return self.call_err(FN_STREAM, [pipe])

    def c_info(self, channel, pipe, info):
        return self.call_err(FN_INFO, [channel, pipe, info])

    def c_dir(self, front, top):
        return self.call_err(FN_DIR, [front, top])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
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

    def mk_world(self, case):
        self.lib.tpw_csm_reset(case["buffers"], int(case["has_array"]), case["mask"])
        self.has_array = case["has_array"]
        self.mask = case["mask"]
        if case["has_array"]:
            for i in range(8):
                if (case["mask"] >> i) & 1:
                    is3d, pipe = case["chans"][i]
                    self.lib.tpw_csm_chan_init(i, is3d, pipe)

    def chan_state(self, i):
        if not (self.has_array and (self.mask >> i) & 1):
            return None
        out = (C.c_int64 * 2)()
        self.lib.tpw_csm_chan_state(i, out)
        return out[0], out[1]

    def mk_info(self, f0c, flags, volume, pos, vel):
        return self.lib.tpw_csm_info(f0c, flags, volume, pos, vel)

    def info_flags(self):
        return self.lib.tpw_csm_info_flags()

    def module_words(self):
        out = (C.c_int64 * 3)()
        self.lib.tpw_csm_module_words(out)
        return [v & MASK for v in out]

    def c_ctor(self):
        return self.lib.tpw_csm_ctor()

    def c_close(self):
        self.lib.tpw_csm_Close()

    def c_tick(self):
        self.lib.tpw_csm_Tick()

    def c_stream(self, pipe):
        return self.lib.tpw_csm_StreamData(pipe)

    def c_info(self, channel, pipe, info):
        return self.lib.tpw_csm_SetChannelInfo(channel, pipe, info)

    def c_dir(self, front, top):
        return self.lib.tpw_csm_SetListenerDirection(front, top)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_csm_alloc.restype, lib.tpw_csm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_csm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_csm_reset.argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_csm_chan_init.argtypes = [C.c_int32, C.c_uint32, C.c_uint64]
    lib.tpw_csm_chan_state.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_csm_module_words.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_csm_info.restype, lib.tpw_csm_info.argtypes = C.c_uint64, [C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint64]
    lib.tpw_csm_info_flags.restype = C.c_uint32
    lib.tpw_csm_ctor.restype = C.c_int32
    lib.tpw_csm_StreamData.restype, lib.tpw_csm_StreamData.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_csm_SetChannelInfo.restype, lib.tpw_csm_SetChannelInfo.argtypes = C.c_int32, [C.c_uint32, C.c_uint64, C.c_uint64]
    lib.tpw_csm_SetListenerDirection.restype, lib.tpw_csm_SetListenerDirection.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_csm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.mk_world(case)
    s.o = {}
    for k in ("front", "top", "position", "velocity"):
        a = s.alloc(12)
        s.words(a, case["pos"][k])
        s.o[k] = a
    pipe = s.alloc(0x80)
    s.words(pipe + 0x70, [case["tag"]])
    s.words(pipe + 0x40, case["hdr"])
    s.o["pipe"] = 0 if case.get("pipe_null") else pipe
    if case["op"] == "info":
        s.o["info"] = s.mk_info(case["f0c"], case["flags"], case["volume"], s.o["position"], s.o["velocity"])


def execute(s, case):
    op = case["op"]
    if op == "ctor":
        return s.c_ctor()
    if op == "close":
        return s.c_close()
    if op == "tick":
        return s.c_tick()
    if op == "stream":
        return s.c_stream(s.o["pipe"])
    if op == "info":
        return s.c_info(case["channel"], s.o["pipe"], s.o["info"])
    return s.c_dir(s.o["front"], s.o["top"])


def norm_pipe(s, p):
    if p == 0:
        return 0
    if p == s.o["pipe"]:
        return 1
    if p == INIT_PIPE:
        return 2
    return 3


def observe(s, case, answer):
    op = case["op"]
    out = {"answer": answer, "log": s.log()}
    if op == "ctor":
        out["module"] = s.module_words()
    if op in ("stream", "info", "dir"):
        states = []
        for i in range(8):
            st = s.chan_state(i)
            states.append(None if st is None else (norm_pipe(s, st[0]), st[1]))
        out["chans"] = states
    if op == "info":
        out["flags"] = s.info_flags()
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


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def base(r, op):
    return {
        "op": op,
        "queues": {
            "Send": [r.choice([0, 1, MASK, 5]) for _ in range(3)],
            "IfOk": [r.choice([0, MASK, 7]) for _ in range(2)],
            "IsSet": [r.choice([0, 1]) for _ in range(2)],
            "GetIndex": [r.randrange(0, 8)],
            "GetSampleFlags": [r.choice([0, 1, 2, 3, 0x80000000, r.getrandbits(32)])],
            "GetDescription": [r.choice([0, 1, 0x100])],
            "AllocNull": [r.choice([0, 0, 1])],
        },
        "buffers": r.randrange(0, 9),
        "has_array": r.random() < 0.9,
        "mask": r.choice([0xFF, 0x00, r.getrandbits(8), r.getrandbits(8)]),
        "chans": [(r.choice([0, 1, 0x20, 0x21, 0x80, 0xFF]), r.choice([0, INIT_PIPE])) for _ in range(8)],
        "tag": r.randrange(1, 1 << 20),
        "hdr": [r.getrandbits(32) for _ in range(5)],
        "pos": {k: [rf(r) for _ in range(3)] for k in ("front", "top", "position", "velocity")},
    }


def with_array(c):
    c["has_array"] = True
    return c


def simple_edges(op, seed):
    r = random.Random(seed)
    for mask in (0, 0xFF, 1, 0xFE):
        for has_array in (True, False):
            for null in (False, True):
                c = base(r, op)
                c.update(mask=mask, has_array=has_array, pipe_null=null)
                if op in ("dir", "stream", "info"):
                    c["has_array"] = True
                    if op == "stream":
                        c["queues"]["GetIndex"] = [r.randrange(0, 8)]
                    c["queues"]["GetSampleFlags"] = [1]
                yield c
    for x in SPECIAL:
        c = base(r, op)
        c["pos"]["front"] = [x, x ^ 0x80000000, x]
        c["pos"]["top"] = [x, x, x ^ 0x80000000]
        if op in ("dir", "stream", "info"):
            c["has_array"] = True
            c["mask"] = 0xFF
        yield c


def ctor_case(r):
    return base(r, "ctor")


def ctor_edges():
    r = random.Random(11)
    for null in (0, 1):
        c = base(r, "ctor")
        c["queues"]["AllocNull"] = [null]
        yield c


def close_case(r):
    return base(r, "close")


def close_edges():
    r = random.Random(12)
    for has_array in (False, True):
        for buffers in (0, 1, 4, 8):
            for mask in (0, 0xFF, 0x0A):
                c = base(r, "close")
                c.update(has_array=has_array, buffers=buffers, mask=mask)
                yield c


def tick_case(r):
    return base(r, "tick")


def tick_edges():
    yield base(random.Random(13), "tick")


def stream_case(r):
    c = with_array(base(r, "stream"))
    c["pipe_null"] = r.random() < 0.15
    return c


def stream_edges():
    r = random.Random(14)
    for index in (0, 3, 7):
        for mask_has in (True, False):
            for desc in (0, 1, 0x100):
                c = with_array(base(r, "stream"))
                c["queues"]["GetIndex"] = [index]
                c["queues"]["GetDescription"] = [desc]
                c["mask"] = 0xFF if mask_has else 0
                yield c
    c = with_array(base(r, "stream"))
    c["pipe_null"] = True
    yield c


def dir_case(r):
    return with_array(base(r, "dir"))


def dir_edges():
    r = random.Random(15)
    for mask in (0, 0xFF, 0xFE, 1):
        for send in (0, 1, MASK):
            for ifok in (0, MASK, 7):
                c = with_array(base(r, "dir"))
                c.update(mask=mask)
                c["queues"]["Send"] = [send]
                c["queues"]["IfOk"] = [ifok]
                yield c
    yield from (c for c in simple_edges("dir", 16))


FLAG_BITS = [1, 2, 0x20, 0x200, 0x400000, 0x800000, 4]
F0C = [0, 0x20, 0x2000, 0x2020]
IS3D = [0, 0x20, 0x21, 1]


def info_fill(r, c, f0c, flags, is3d):
    c["op"] = "info"
    c["f0c"], c["flags"] = f0c, flags
    c["volume"] = r.choice([0, 1, 127, 128, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32), r.randrange(0, 200)])
    c["channel"] = r.randrange(0, 8)
    c["mask"] = c["mask"] | (1 << c["channel"]) if r.random() < 0.9 else c["mask"] & ~(1 << c["channel"])
    c["has_array"] = True
    ch = c["channel"]
    c["chans"][ch] = (is3d, r.choice([0, INIT_PIPE]))
    return c


def info_case(r):
    flags = 0
    for b in FLAG_BITS + [0x8000, 0x100]:
        if r.random() < 0.4:
            flags |= b
    return info_fill(r, base(r, "info"), r.choice(F0C + [r.getrandbits(32)]), flags, r.choice(IS3D + [r.getrandbits(8)]))


def info_edges():
    r = random.Random(17)
    k = 0
    for shift in (0, 3):
        for subset in range(1 << len(FLAG_BITS)):
            flags = 0
            for i, b in enumerate(FLAG_BITS):
                if (subset >> i) & 1:
                    flags |= b
            k += 1
            yield info_fill(r, base(r, "info"), F0C[(k + shift) % 4], flags, IS3D[(k // 4 + shift) % 4])
    # the null channel, the volume corner values
    c = info_fill(r, base(r, "info"), 0x2000, 2, 0x20)
    c["mask"] &= ~(1 << c["channel"])
    yield c
    for v in (0, 1, 126, 127, 128, 255, 0x01000001, 0x7FFFFFFF, 0x80000000, MASK):
        c = info_fill(r, base(r, "info"), 0, 2, 0)
        c["mask"] |= 1 << c["channel"]
        c["volume"] = v
        yield c


TESTS = [
    FnTest("CSprocketsModule::CSprocketsModule", ppc, host, edge=ctor_edges, rnd=ctor_case),
    FnTest("CSprocketsModule::Tick", ppc, host, edge=tick_edges, rnd=tick_case),
    FnTest("CSprocketsModule::Close", ppc, host, edge=close_edges, rnd=close_case),
    FnTest("CSprocketsModule::StreamData", ppc, host, edge=stream_edges, rnd=stream_case),
    FnTest("CSprocketsModule::SetChannelInfo", ppc, host, edge=info_edges, rnd=info_case),
    FnTest("CSprocketsModule::SetListenerDirection", ppc, host, edge=dir_edges, rnd=dir_case),
]
