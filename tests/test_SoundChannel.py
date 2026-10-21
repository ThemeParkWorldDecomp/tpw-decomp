"""Unit w79c, the sound library's pipe channel and sample store, against libtpw_unit_SoundChannel.dylib
(tests/unit_SoundChannel.cpp), in the program "sound symbols shared" (Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundSampleWAV.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/CSprocketsModule.cpp  TbSoundPipeChannel: constructor 0x10009a9c, destructor 0x1000ab10, GetNextHandle 0x10000bf8,
      SetBufferSize 0x10000f54, SetSampleHeader 0x10000f98, SetPriority 0x10009b68, SetPipeInfo 0x10009e18, GetChannelInfo
      0x10009f68, InitChannelInfo 0x1000a038, GetIndex 0x1000a19c, SetVolume 0x1000a238, SetInfo 0x1000a278, StreamerCallback
      0x1000a358, FillServiceBuffer 0x1000a408, GetDataChunk 0x1000a4ac, FindPipe 0x1000a5e8, Attatch 0x1000a6a0, GetChannelUsed
      0x1000a754, ReleasePipe 0x1000a7e0, GetVolume 0x1000a9b4, GetPriority 0x1000c19c, GetActualVolume 0x1000c1dc, SetIndex
      0x1000fd34, ControlledBy 0x10010520, GetHandle 0x100107ac, GetBufferSize 0x1001245c, GetSampleFlags 0x1002c024,
      GetSampleHeader 0x1002cf24
  src/sound/TbSoundSampleStore.cpp, src/sound/TbSoundCore.cpp  TbSoundSampleStore: constructor 0x1000fa64, destructor 0x10010054, Init 0x1000fc60, ReleaseAllPipes
      0x1000fe34, StopAllChannels 0x1001038c, StopAllChannelsWithVolumeControl 0x1001043c, SetNumberOfChannelsToUse 0x10010568,
      GetFreeChannel 0x10010660, GetPipesFromHandle 0x10010c90, CreateNewSample 0x1001093c, CreateNewControl 0x10010b6c,
      operator[] 0x100100e8 and 0x10015478, FindChannelAndPipe 0x100101b0, GetValidChannelIndexFromHandle 0x100102ac,
      GetNumberOfUsedChannels 0x1000be68, GetMaxNumberOfChannels 0x1000beb4; CChannelIterator 0x1000ffa4, ++ 0x1000fed8, operator
      0x1000ff44; CUsedChannelIterator 0x10010858; CAutoArrayPtr<TbSoundPipeChannel> 0x1000fb94, 0x1000faec, 0x1000fd90

Each function runs as the original in the emulator and as the host's translation. A case is a world (a store of M channels built
by the constructors themselves, up to four fake pipes for the codecs behind the channels, a sample info, two volume controls, a
data buffer) whose channels, pipes and info are set from the same words on both sides; one operation runs; then everything is
compared: the result, the out parameters, every word of every channel (pointers as labels), the store, the pipes, the info, the
channel state count, the handle statics, the data buffer's bytes and the log of events. The callees outside the unit are fakes
that record the same event on both sides: the pipes behind a channel (every virtual slot, answers from queues), the core's
StopAndReleasePipe, the codec factory (four members), the runtime's double to unsigned long (the double's bits), memset (the
destination as a region offset, the byte and the unsigned length; it writes only inside the case's buffers, so a negative length
(done above size) is recorded and not executed on either side), and the runtime heap (new, new[] with the Mac's sizes and
labels, delete; the service buffer of every channel is allocated at an address whose low five bits the case chooses).
The array runtime (__construct_new_array and __destroy_new_array) is stubbed with an eight-byte cookie in front of the elements; the
elements are destroyed from the last to the first (the C++ order; the runtime itself is not in the dump).
Virtual calls on the PowerPC side go through the object's real Mac vtable (0x1003bfd0 for a channel), so a wrong slot shows.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundChannel"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5
VT_CHANNEL = 0x1003BFD0
NAN_BITS = 0x7FF8000000000000

# The words of a case (the unit file's S_ enum).
S_OP, S_M, S_USED, S_TARGET, S_PIPES, S_EXTRA, S_NSKEW, S_SKEW, S_A, S_INFO, S_VOL, S_SYS, S_GLOBALS, S_CH, S_CHW, S_PIPE, S_SIZE = (
    0, 1, 2, 3, 4, 5, 6, 7, 16, 24, 40, 50, 51, 64, 24, 256, 272)
O_RET, O_SLOT, O_PIPE, O_CHANNEL, O_SEQ, O_DUMP, O_STORE, O_PIPES, O_INFO, O_STATE, O_COUNT = 0, 1, 5, 6, 7, 16, 304, 310, 326, 342, 350
DATA_BYTES, DATA_START, DATA_SIZE = 0x6000, 0x2000, 0x2000

# The operations: id -> (name, address, result kind).
OPS = {
    0: ("TbSoundPipeChannel::TbSoundPipeChannel", 0x10009A9C),
    1: ("TbSoundPipeChannel::~TbSoundPipeChannel", 0x1000AB10),
    2: ("TbSoundPipeChannel::GetNextHandle", 0x10000BF8),
    3: ("TbSoundPipeChannel::SetBufferSize", 0x10000F54),
    4: ("TbSoundPipeChannel::GetBufferSize", 0x1001245C),
    5: ("TbSoundPipeChannel::SetSampleHeader", 0x10000F98),
    6: ("TbSoundPipeChannel::SetPriority", 0x10009B68),
    7: ("TbSoundPipeChannel::GetPriority", 0x1000C19C),
    8: ("TbSoundPipeChannel::SetVolume", 0x1000A238),
    9: ("TbSoundPipeChannel::GetActualVolume", 0x1000C1DC),
    10: ("TbSoundPipeChannel::GetSampleFlags", 0x1002C024),
    11: ("TbSoundPipeChannel::GetSampleHeader", 0x1002CF24),
    12: ("TbSoundPipeChannel::ControlledBy", 0x10010520),
    13: ("TbSoundPipeChannel::GetHandle", 0x100107AC),
    14: ("TbSoundPipeChannel::GetIndex", 0x1000A19C),
    15: ("TbSoundPipeChannel::SetIndex", 0x1000FD34),
    16: ("TbSoundPipeChannel::SetPipeInfo", 0x10009E18),
    17: ("TbSoundPipeChannel::GetChannelInfo", 0x10009F68),
    18: ("TbSoundPipeChannel::InitChannelInfo", 0x1000A038),
    19: ("TbSoundPipeChannel::SetInfo", 0x1000A278),
    20: ("TbSoundPipeChannel::Attatch", 0x1000A6A0),
    21: ("TbSoundPipeChannel::FindPipe", 0x1000A5E8),
    22: ("TbSoundPipeChannel::GetDataChunk", 0x1000A4AC),
    23: ("TbSoundPipeChannel::FillServiceBuffer", 0x1000A408),
    24: ("TbSoundPipeChannel::StreamerCallback", 0x1000A358),
    25: ("TbSoundPipeChannel::GetChannelUsed", 0x1000A754),
    26: ("TbSoundPipeChannel::ReleasePipe", 0x1000A7E0),
    27: ("TbSoundPipeChannel::GetVolume", 0x1000A9B4),
    30: ("TbSoundSampleStore::TbSoundSampleStore", 0x1000FA64),
    31: ("TbSoundSampleStore::~TbSoundSampleStore", 0x10010054),
    32: ("TbSoundSampleStore::Init", 0x1000FC60),
    33: ("TbSoundSampleStore::SetNumberOfChannelsToUse", 0x10010568),
    34: ("TbSoundSampleStore::GetNumberOfUsedChannels", 0x1000BE68),
    35: ("TbSoundSampleStore::GetMaxNumberOfChannels", 0x1000BEB4),
    36: ("TbSoundSampleStore::operator[](unsigned long)", 0x10015478),
    37: ("TbSoundSampleStore::operator[](const CUniqueHandle&)", 0x100100E8),
    38: ("TbSoundSampleStore::FindChannelAndPipe", 0x100101B0),
    39: ("TbSoundSampleStore::GetValidChannelIndexFromHandle", 0x100102AC),
    40: ("TbSoundSampleStore::CChannelIterator (constructor, ++, operator)", 0x1000FFA4),
    41: ("TbSoundSampleStore::CUsedChannelIterator (constructor, ++, operator)", 0x10010858),
    42: ("TbSoundSampleStore::ReleaseAllPipes", 0x1000FE34),
    43: ("TbSoundSampleStore::StopAllChannels", 0x1001038C),
    44: ("TbSoundSampleStore::StopAllChannelsWithVolumeControl", 0x1001043C),
    45: ("TbSoundSampleStore::GetFreeChannel", 0x10010660),
    46: ("TbSoundSampleStore::GetPipesFromHandle", 0x10010C90),
    47: ("TbSoundSampleStore::CreateNewSample", 0x1001093C),
    48: ("TbSoundSampleStore::CreateNewControl", 0x10010B6C),
    49: ("CAutoArrayPtr<TbSoundPipeChannel>::CAutoArrayPtr", 0x1000FB94),
    50: ("CAutoArrayPtr<TbSoundPipeChannel>::~CAutoArrayPtr", 0x1000FAEC),
    51: ("CAutoArrayPtr<TbSoundPipeChannel>::operator TbSoundPipeChannel*", 0x1000FD90),
}
A_STOP, A_FACTORY_CTOR, A_FACTORY_DTOR, A_CREATE_CODEC, A_CREATE_CONTROLLER, A_CVT = (
    0x1000BF50, 0x10010B10, 0x10010A88, 0x100007C4, 0x10000054, 0x1002E988)

VT = {}


def s32(v):
    return T.s32(v)


def u32(v):
    return v & MASK


def pack(*words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def double_bits(x):
    if math.isnan(x):
        return NAN_BITS
    return struct.unpack(">q", struct.pack(">d", x))[0]


# ---------------------------------------------------------------- the PowerPC side: the fakes


def ref(e, address):
    """A pointer into one of the case's regions as the unit file's Ref names it."""
    for label, start, size in e.w.regions:
        if 0 <= address - start < size:
            return label * 1000000 + address - start
    for label, start, size in e.w.regions:
        offset = address - start
        if -0x2000 <= offset < size + 0x2000:
            return label * 1000000 + offset
    return -1


def pw(e, address):
    """A pointer field as the unit file's PW compares it."""
    if address == 0:
        return 0
    label = e.cl.lab(address)
    if label > 0:
        return label
    return FILL if address == FILL else -1


def link_class(address):
    return 0 if address == 0 else 1 if address == FILL else 2


def node(e, index):
    return e.w.pipes[index] if 0 <= index < len(e.w.pipes) else 0


def record(e, name, ints):
    """A callee event: the arguments and then the sum of the channels' lock words (see the unit file's Lock)."""
    lock = sum(e.u32(address) for address in e.w.locks) & MASK
    e.cl.rec(name, tuple(ints) + (lock,))


def stub_memset(e):
    L, dest, value, count = e.cl, e.r(3), e.r(4) & 0xFF, e.r(5)
    record(e, "memset", (ref(e, dest), value, count))
    for label, start, size in e.w.regions:
        offset = dest - start
        if offset >= 0 and count <= size - offset:
            if count:
                e.write(dest, bytes([value]) * count)
            break


def stub_new(e):
    size = e.r(3)
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    if not e.w.setup:
        e.cl.rec("new", (size, e.cl.newlab(block)))
    e.set_r(3, block)


def stub_new_array(e):
    w, size = e.w, e.r(3)
    if size == 0x1220:
        skew = w.skews[w.skew_next] & 31 if w.skew_next < len(w.skews) else 0
        w.skew_next += 1
        base = e.alloc(size + 96, b"\xa5" * (size + 96), alignment=32)
        block = base + skew
    else:
        block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
        if w.setup:
            w.array_raw = block
    if not w.setup:
        e.cl.rec("new[]", (size, e.cl.newlab(block)))
    e.set_r(3, block)


def stub_delete(e):
    L, block = e.cl, e.r(3)
    if block != 0 and L.lab(block) > 0:
        L.rec("delete", (L.lab(block),))
        L.forget(block)


def construct_array(e, call):
    block, ctor, size, count = e.r(3), e.r(4), e.r(6), e.r(7)
    e.write(block, pack(size, count))
    for i in range(count):
        call(ctor, block + 8 + i * size, 1)
    return block + 8


def destroy_array(e, call):
    pointer, dtor = e.r(3), e.r(4)
    raw = pointer - 8
    size, count = e.u32(raw), e.u32(raw + 4)
    for i in range(count - 1, -1, -1):
        call(dtor, pointer + i * size, 0xFFFF)
    stub_delete_at(e, raw)
    return None


def stub_delete_at(e, block):
    L = e.cl
    if block != 0 and L.lab(block) > 0:
        L.rec("delete", (L.lab(block),))
        L.forget(block)


def fake_pipe_slots():
    """The fake pipe's slot functions, in the order of the Mac vtable from +0x08."""
    def rec(name, *fields):
        def stub(e):
            L, this = e.cl, e.r(3)
            ints = [L.lab(this)]
            for index, kind in enumerate(fields):
                value = e.r(4 + index)
                ints.append(L.lab(value) if kind == "l" else value & MASK if kind == "u" else value & 0xFF)
            record(e, name, ints)
        return stub

    def attatch(e):
        rec("Pipe.Attatch", "l")(e)
        e.set_r(3, node(e, e.cl.ret("Attatch", -1)))

    def find(e):
        rec("Pipe.FindPipe", "u")(e)
        value = e.cl.ret("FindPipe", -1)
        e.set_r(3, e.r(3) if value == -2 else node(e, value))

    def sample_rate(e):
        rec("Pipe.GetSampleRate")(e)
        e.set_r(3, 0)

    def chunk(e):
        L, this = e.cl, e.r(3)
        record(e, "Pipe.GetDataChunk", (L.lab(this), ref(e, e.r(4)), s32(e.r(5)), e.r(7) & 0xFF))
        if getattr(e.w, "show_done", False):   # the StreamerCallback op: the done word handed in (the unit file's gShowDone)
            record(e, "Pipe.GetDataChunk.done", (L.lab(this), e.u32(e.r(6))))
        e.write(e.r(6), pack(L.ret("Done", 0)))
        e.set_r(3, u32(L.ret("Chunk", 0)))

    def set_info(e):
        L, this, info = e.cl, e.r(4), e.r(5)
        record(e, "Pipe.SetInfo", (L.lab(this), L.lab(info)))
        e.write(e.r(3), pack(L.ret("SetInfoErr", 0)))

    def set_handle(e):
        rec("Pipe.SetHandle", "u")(e)
        e.set_r(3, e.r(4))

    return [lambda e: None, attatch, rec("Pipe.Release"), rec("Pipe.Detatch"), find, rec("Pipe.GetInfo", "l"), sample_rate, chunk,
            rec("Pipe.Restart"), set_info, set_handle, rec("Pipe.Completed", "b"), rec("Pipe.SetDataStart", "u")]


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_sc_run.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_sc_cvt.argtypes = [C.c_double]
    lib.tpw_sc_cvt.restype = C.c_uint32
    lib.tpw_sc_sizes.argtypes = [C.c_int32]
    lib.tpw_sc_sizes.restype = C.c_int32
    e.stub("memset", stub_memset)
    e.stub("__nw__FUl", stub_new)
    e.stub("__nwa__FUl", stub_new_array)
    e.stub("__dl__FPv", stub_delete)
    e.stub("__dla__FPv", stub_delete)
    T.stub_cb(e, "__construct_new_array", construct_array)
    T.stub_cb(e, "__destroy_new_array", destroy_array)

    def stop(e):
        L = e.cl
        record(e, "Core.StopAndReleasePipe", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(6) & 0xFF))
        e.write(e.r(3), pack(L.ret("StopErr", 0)))

    def create_codec(e):
        L = e.cl
        record(e, "Factory.CreateCodec", (L.lab(e.r(4)), L.lab(e.r(5))))
        e.set_r(3, node(e, L.ret("Codec", -1)))

    def create_controller(e):
        L = e.cl
        record(e, "Factory.CreateController", (L.lab(e.r(4)),))
        e.set_r(3, node(e, L.ret("Control", -1)))

    def cvt(e):
        value = e.f(1)
        record(e, "CvtDblUl", (double_bits(value),))
        e.set_r(3, lib.tpw_sc_cvt(value))

    T.stub_internal(e, A_STOP, stop)
    T.stub_internal(e, A_FACTORY_CTOR, lambda e: record(e, "Factory.ctor", ()))
    T.stub_internal(e, A_FACTORY_DTOR, lambda e: record(e, "Factory.dtor", ()))
    T.stub_internal(e, A_CREATE_CODEC, create_codec)
    T.stub_internal(e, A_CREATE_CONTROLLER, create_controller)
    T.stub_internal(e, A_CVT, cvt)
    words = [0, 0] + [T.fake_function(e, fn) for fn in fake_pipe_slots()]
    VT["pipe"] = T.persist_alloc(e, 4 * len(words), pack(*words))
    VT["sizes"] = [lib.tpw_sc_sizes(i) for i in range(4)]


# ---------------------------------------------------------------- the case


def blank_channel():
    return dict(w=[0] * 20, next=-1, prev=-1)


def new_case(op, M=2, used=None, target=0, **kw):
    case = dict(op=op, M=M, used=M if used is None else used, target=target, pipes=[], ch=[blank_channel() for _ in range(min(M, 8))],
                a=[0] * 8, info=[0] * 12, vol=[0x3F800000, 0x3F000000, 0x40000000, 0x3E800000, 0x3F400000, 0x3FC00000, 0, 0x40400000],
                sys=10, globals=(0, 0, 26, 0), skews=[], queues={}, extra=-1)
    case.update(kw)
    return case


def spec_of(c):
    sp = [0] * S_SIZE
    sp[S_OP], sp[S_M], sp[S_USED], sp[S_TARGET], sp[S_PIPES], sp[S_EXTRA] = c["op"], c["M"], c["used"], c["target"], len(c["pipes"]), c["extra"]
    sp[S_NSKEW] = len(c["skews"])
    sp[S_SKEW:S_SKEW + len(c["skews"])] = c["skews"]
    sp[S_A:S_A + 8] = c["a"]
    sp[S_INFO:S_INFO + 12] = c["info"]
    sp[S_VOL:S_VOL + 8] = c["vol"]
    sp[S_SYS] = c["sys"]
    sp[S_GLOBALS:S_GLOBALS + 4] = c["globals"]
    for i, ch in enumerate(c["ch"]):
        words = list(ch["w"])
        words[16], words[17] = ch["next"], ch["prev"]
        sp[S_CH + i * S_CHW:S_CH + i * S_CHW + 20] = words
    for j, p in enumerate(c["pipes"]):
        sp[S_PIPE + j * 4:S_PIPE + j * 4 + 3] = [p[0], p[1], p[2]]
    return sp


# Channel words: 0 inUse, 1 volume, 2 sampleFlags, 3 flags, 4 bufferSize, 5 controlMask, 6 field2C, 7 handle, 8 serviceResult,
# 9 volume table (0 none, 1, 2), 10 table count, 11..15 header (size, rate/bits/flags, priority, field C, data), 16 next pipe,
# 17 previous pipe, 18 the base's handle, 19 lock.
def host_run(lib, c):
    spec = (C.c_int64 * S_SIZE)(*spec_of(c))
    out = (C.c_int64 * O_COUNT)()
    data = (C.c_uint8 * DATA_BYTES)()
    CL.host_begin(lib, {}, c["queues"])
    lib.tpw_sc_run(spec, out, data)
    log = CL.host_end(lib)
    result = collect(list(out), bytes(data), log)
    result["vptr"] = True
    return result


def collect(o, data, log):
    """The answer words in the shape both sides compare."""
    return dict(ret=o[O_RET], slots=o[O_SLOT:O_SLOT + 4], pipe=o[O_PIPE], channel=o[O_CHANNEL], seq=o[O_SEQ:O_SEQ + 9],
                dumps=[o[O_DUMP + i * 32:O_DUMP + i * 32 + 32] for i in range(9)], store=o[O_STORE:O_STORE + 4],
                pipes=o[O_PIPES:O_PIPES + 16], info=o[O_INFO:O_INFO + 16], state=o[O_STATE:O_STATE + 4], data=data, log=log)


class World:
    pass


def ppc_dump_channel(e, addr):
    w, o = e.w, [0] * 32
    o[0], o[1], o[2], o[3] = pw(e, e.u32(addr + 8)), pw(e, e.u32(addr + 0xC)), e.u32(addr + 0x10), link_class(e.u32(addr + 4))
    o[4] = e.read(addr + 0x14, 1)[0]
    for k, off in zip(range(5, 13), (0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34)):
        o[k] = e.u32(addr + off)
    table = e.u32(addr + 0x38)
    o[13] = 0 if table == 0 else 1 if table == w.floats[0] else 2 if table == w.floats[1] else FILL if table == FILL else -1
    o[14] = e.u32(addr + 0x3C)
    o[15], o[16], o[17], o[18], o[19] = (e.u32(addr + off) for off in (0x40, 0x44, 0x48, 0x4C, 0x50))
    data, aligned = e.u32(addr + 0x54), e.u32(addr + 0x58)
    o[20] = pw(e, data)
    data_set = data != 0 and data != FILL
    o[21] = 0 if aligned == 0 else FILL if aligned == FILL else -1 if not data_set else s32(aligned - data)
    o[22] = data & 31 if data_set else -1
    o[23] = e.u32(addr + 0x5C)
    return o


def ppc_dump_info(e, info):
    w = e.w

    def vc(address):
        return 0 if address == 0 else 1 if address == w.controls[0] else 2 if address == w.controls[1] else FILL if address == FILL else -1
    u = e.u32
    return [u(info), u(info + 8), u(info + 0xC), u(info + 0x10), u(info + 0x14), u(info + 0x18), u(info + 0x1C), u(info + 0x28),
            vc(u(info + 0x80)), u(info + 0x84), u(info + 0x88), u(info + 0x8C), u(info + 0x24), u(info + 0x40), u(info + 4), u(info + 0x20)]


def call(e, address, args):
    return T.call_ppc(e, address, args)[0]


def virtual(e, obj, slot, expected):
    """The code address of a virtual slot of the object's Mac vtable (checked against the listing)."""
    address = e.u32(e.u32(e.u32(obj) + slot))
    assert address == expected, (hex(address), hex(expected))
    return address


def build_world(e, c):
    """Everything the unit file builds before the operation: fixtures, the store (through the constructors), the words."""
    w = World()
    e.w = w
    op, M = c["op"], c["M"]
    w.setup, w.skews, w.skew_next, w.array_raw, w.regions, w.pipes, w.locks = True, list(c["skews"]), 0, 0, [], [], []
    sp = spec_of(c)
    a64 = lambda i: struct.pack(">I", i & MASK)
    e.write(0x1003F350, pack(sp[S_GLOBALS + 3]))
    e.write(0x1003E320, bytes([sp[S_GLOBALS] & 255]))
    e.write(0x1003E324, pack(sp[S_GLOBALS + 1]))
    e.write(0x10035C88, pack(sp[S_GLOBALS + 2]))
    w.core = e.alloc(16, b"\xa5" * 16)
    w.data = e.alloc(DATA_BYTES, b"\xa5" * DATA_BYTES)
    w.floats = [e.alloc(16, b"".join(struct.pack(">I", sp[S_VOL + 4 * k + i] & MASK) for i in range(4))) for k in range(2)]
    w.controls = [e.alloc(8, pack(w.floats[0], 4)), e.alloc(8, pack(w.floats[1], 2))]
    w.info = e.alloc(0x90, b"\xa5" * 0x90)
    v = sp[S_INFO:S_INFO + 12]
    vc = 0 if v[5] == 0 else w.controls[0] if v[5] == 1 else w.controls[1]
    for off, val in ((0, v[10]), (8, v[11]), (0xC, v[0]), (0x10, v[1]), (0x14, v[2]), (0x18, v[7]), (0x1C, v[3]), (0x28, v[4]),
                     (0x80, vc), (0x84, v[6]), (0x88, v[8]), (0x8C, v[9])):
        e.write(w.info + off, pack(val))
    w.sys = e.alloc(0x4C, b"\xa5" * 0x4C)
    e.write(w.sys + 0x1C, pack(sp[S_SYS]))
    w.slots = e.alloc(16, pack(FILL, FILL, FILL, FILL))
    for j in range(len(c["pipes"])):
        w.pipes.append(e.alloc(0x14, b"\xa5" * 0x14))
    w.store = w.alone = w.holder = 0
    w.channels = []
    if op == 1:
        w.alone = e.alloc(0x60, b"\xa5" * 0x60)
        call(e, 0x10009A9C, [w.alone])
    elif op in (50, 51):
        w.holder = e.alloc(4, pack(FILL))
        call(e, 0x1000FB94, [w.holder, c["a"][1]])
        if op == 50 and c["a"][2]:
            e.write(w.holder, pack(0))   # the holder's pointer is NULL (the array leaks)
    elif op not in (0, 30, 49):
        w.store = e.alloc(0x10, b"\xa5" * 0x10)
        call(e, 0x1000FA64, [w.store, w.core, M])
        base = e.u32(w.store + 0xC)
        w.channels = [base + 0x60 * i for i in range(M)]
        e.write(w.store + 8, pack(c["used"]))
        for i in range(min(M, 8)):
            ch = c["ch"][i]
            x = sp[S_CH + i * S_CHW:S_CH + i * S_CHW + 20]
            x[16], x[17] = ch["next"], ch["prev"]
            addr = w.channels[i]
            e.write(addr + 0x14, bytes([x[0] & 255]))
            for off, val in ((0x18, x[1]), (0x1C, x[2]), (0x20, x[3]), (0x24, x[4]), (0x28, x[5]), (0x2C, x[6]), (0x30, x[7]),
                             (0x34, x[8]), (0x38, w.floats[x[9] - 1] if x[9] in (1, 2) else 0), (0x3C, x[10]), (0x40, x[11]),
                             (0x44, x[12]), (0x48, x[13]), (0x4C, x[14]), (0x50, x[15]), (0x10, x[18]), (0x5C, x[19])):
                e.write(addr + off, pack(val))
    # the fake pipes
    for j, address in enumerate(w.pipes):
        p = sp[S_PIPE + j * 4:S_PIPE + j * 4 + 3]
        e.write(address, pack(VT["pipe"]))
        e.write(address + 0x10, pack(p[0]))
    return w, sp


def link_and_label(e, c, w, sp):
    """The links between the objects and the labels (the unit file does the same after its setup)."""
    op, M, t = c["op"], c["M"], c["target"]
    labels = {w.core: 1, w.info: 3, w.controls[0]: 4, w.controls[1]: 5, w.data + DATA_START: 6, w.sys: 7, w.floats[0]: 8, w.floats[1]: 9}
    w.regions.append((6, w.data + DATA_START, DATA_SIZE))
    for j, address in enumerate(w.pipes):
        labels[address] = 20 + j
        p = sp[S_PIPE + j * 4:S_PIPE + j * 4 + 3]
        e.write(address + 8, pack(node(e, p[1])))
        e.write(address + 0xC, pack(w.channels[t] if p[2] == -2 and w.channels else node(e, p[2])))
    if w.store:
        labels[w.store] = 2
        labels[w.array_raw] = 30
        for i in range(min(M, 8)):
            ch = c["ch"][i]
            addr = w.channels[i]
            labels[addr] = 10 + i
            block = e.u32(addr + 0x54)
            labels[block] = 40 + i
            if i == t:
                w.regions.append((40 + i, block, 0x1220))
            e.write(addr + 8, pack(node(e, ch["next"])))
            e.write(addr + 0xC, pack(node(e, ch["prev"])))
    if w.alone:
        labels[w.alone] = 9
        block = e.u32(w.alone + 0x54)
        labels[block] = 40
        w.regions.append((40, block, 0x1220))
    if w.holder:
        labels[w.holder] = 2
        labels[w.array_raw] = 30
    w.locks = [address + 0x5C for address in w.channels[:8]]
    e.cl.labels.update(labels)
    e.cl.log.clear()
    w.setup = False


def ppc_run(e, c):
    sp = spec_of(c)
    e.cl = CL.PpcLog({}, c["queues"])
    w, sp = build_world(e, c)
    link_and_label(e, c, w, sp)
    L, op, a, M = e.cl, c["op"], c["a"], c["M"]
    t = c["target"]
    ch = w.channels[t] if w.channels else w.alone
    buffer = w.data + DATA_START + a[2]
    handle = e.alloc(4, pack(a[0]))
    ret, pipe_out, channel_out, seq = 0, 0, 0 if op == 46 else -1, [0] * 9
    slots = w.slots

    def tb_error(address, *args):
        result = e.alloc(4, pack(FILL))
        call(e, address, [result] + list(args))
        return e.u32(result)

    def index_of(channel):
        if channel == 0:
            return -1
        base = e.u32(w.store + 0xC)
        return -2 if channel < base else (channel - base) // 0x60

    if op == 0:
        w.alone = ch = e.alloc(0x60, b"\xa5" * 0x60)
        L.labels[ch] = 9
        call(e, 0x10009A9C, [ch])
        ch_vptr = e.u32(ch) == VT_CHANNEL
    elif op == 1:
        call(e, virtual(e, ch, 8, 0x1000AB10), [ch, T.S16(a[0])])
    elif op == 2:
        ret = call(e, 0x10000BF8, [ch])
    elif op == 3:
        call(e, 0x10000F54, [ch, a[0]])
    elif op == 4:
        ret = call(e, 0x1001245C, [ch])
    elif op == 5:
        header = e.alloc(0x14, pack(*a[:5]))
        call(e, 0x10000F98, [ch, header])
    elif op == 6:
        call(e, 0x10009B68, [ch, a[0]])
    elif op == 7:
        ret = call(e, 0x1000C19C, [ch])
    elif op == 8:
        call(e, 0x1000A238, [ch, a[0]])
    elif op == 9:
        ret = call(e, 0x1000C1DC, [ch])
    elif op == 10:
        ret = call(e, 0x1002C024, [ch])
    elif op == 11:
        ret = call(e, 0x1002CF24, [ch]) - ch
    elif op == 12:
        ret = call(e, 0x10010520, [ch, a[0]])
    elif op == 13:
        ret = call(e, 0x100107AC, [ch])
    elif op == 14:
        ret = call(e, 0x1000A19C, [ch])
    elif op == 15:
        call(e, 0x1000FD34, [ch, T.U16(a[0])])
    elif op == 16:
        ret = tb_error(0x10009E18, ch, a[0], w.info)
    elif op == 17:
        call(e, 0x10009F68, [ch, slots + 4, slots + 8])
    elif op == 18:
        ret = call(e, 0x1000A038, [ch, w.info, slots + 4])
    elif op == 19:
        ret = tb_error(virtual(e, ch, 0x2C, 0x1000A278), ch, w.info)
    elif op == 20:
        ret = pw(e, call(e, virtual(e, ch, 0x0C, 0x1000A6A0), [ch, node(e, a[0])]))
    elif op == 21:
        ret = pw(e, call(e, virtual(e, ch, 0x3C, 0x1000A5E8), [ch, handle]))
    elif op == 22:
        ret = call(e, virtual(e, ch, 0x24, 0x1000A4AC), [ch, buffer, T.I(a[1]), slots + 4, 1 if a[3] else 0])
    elif op == 23:
        ret = call(e, 0x1000A408, [ch])
    elif op == 24:
        pipe = ch if a[1] == 0 else node(e, a[1] - 1)
        w.show_done = True
        ret = call(e, 0x1000A358, [buffer, a[0], pipe]) & 0xFF
        w.show_done = False
    elif op == 25:
        ret = pw(e, call(e, 0x1000A754, [ch]))
    elif op == 26:
        ret = call(e, 0x1000A7E0, [ch, 1 if a[0] else 0]) & 0xFF
    elif op == 27:
        ret = call(e, 0x1000A9B4, [ch])
    elif op == 30:
        w.store = e.alloc(0x10, b"\xa5" * 0x10)
        L.labels[w.store] = 2
        call(e, 0x1000FA64, [w.store, w.core, M])
        base = e.u32(w.store + 0xC)
        w.channels = [base + 0x60 * i for i in range(M)]
    elif op == 31:
        call(e, 0x10010054, [w.store, T.S16(a[0])])
    elif op == 32:
        ret = tb_error(0x1000FC60, w.store, a[0])
    elif op == 33:
        call(e, 0x10010568, [w.store, w.sys])
    elif op == 34:
        ret = call(e, 0x1000BE68, [w.store])
    elif op == 35:
        ret = call(e, 0x1000BEB4, [w.store])
    elif op == 36:
        ret = index_of(call(e, 0x10015478, [w.store, a[0]]))
    elif op == 37:
        ret = index_of(call(e, 0x100100E8, [w.store, handle]))
    elif op == 38:
        out = e.alloc(4, pack(FILL))
        channel_out = index_of(call(e, 0x100101B0, [w.store, handle, out]))
        ret = channel_out
        pipe_out = pw(e, e.u32(out))
    elif op == 39:
        ret = call(e, 0x100102AC, [w.store, handle, slots + 4]) & 0xFF
    elif op in (40, 41):
        it = e.alloc(8, b"\xa5" * 8)
        call(e, 0x1000FFA4 if op == 40 else 0x10010858, [it, w.store] if op == 40 else [it, w.store])
        base = e.u32(w.store + 0xC)
        seq[0] = index_of(call(e, 0x1000FF44, [it]))
        for k in range(1, 7):
            seq[k] = index_of(call(e, 0x1000FED8, [it]))
        seq[7] = s32(e.u32(it + 4) - base) // 0x60
        seq[8] = s32(e.u32(it) - base) // 0x60
    elif op == 42:
        ret = call(e, 0x1000FE34, [w.store])
    elif op == 43:
        call(e, 0x1001038C, [w.store, 1 if a[0] else 0])
    elif op == 44:
        call(e, 0x1001043C, [w.store, a[0]])
    elif op == 45:
        ret = index_of(call(e, 0x10010660, [w.store, w.info]))
    elif op == 46:
        out = e.alloc(4, pack(FILL))
        pipe = call(e, 0x10010C90, [w.store, a[0], out, w.info])
        ret = pw(e, pipe)
        channel_out = pw(e, e.u32(out))
    elif op == 47:
        ret = call(e, 0x1001093C, [w.store, a[0], w.info, slots + 4])
    elif op == 48:
        ret = call(e, 0x10010B6C, [w.store, a[0], w.info])
    elif op == 49:
        w.holder = e.alloc(4, pack(FILL))
        L.labels[w.holder] = 2
        call(e, 0x1000FB94, [w.holder, a[1]])
        ret = 1 if e.u32(w.holder) != 0 else 0
    elif op == 50:
        call(e, 0x1000FAEC, [w.holder, T.S16(a[0])])
    elif op == 51:
        ret = 1 if call(e, 0x1000FD90, [w.holder]) == e.u32(w.holder) else 0

    # the answer, in the unit file's order
    o = [0] * O_COUNT
    o[O_RET] = ret
    for k in range(4):
        o[O_SLOT + k] = e.u32(slots + 4 * k)
    o[O_PIPE], o[O_CHANNEL] = pipe_out, channel_out
    o[O_SEQ:O_SEQ + 9] = seq
    if op in (0, 1):
        o[O_DUMP:O_DUMP + 32] = ppc_dump_channel(e, w.alone)
    if w.store:
        for i in range(min(M, 8)):
            o[O_DUMP + 32 * i:O_DUMP + 32 * i + 32] = ppc_dump_channel(e, w.channels[i])
        if 0 <= c["extra"] < M:
            o[O_DUMP + 256:O_DUMP + 288] = ppc_dump_channel(e, w.channels[c["extra"]])
        o[O_STORE:O_STORE + 4] = [pw(e, e.u32(w.store)), e.u32(w.store + 4), e.u32(w.store + 8), 1 if e.u32(w.store + 0xC) else 0]
    if op >= 49:
        o[O_STORE + 3] = 1 if e.u32(w.holder) != 0 else 0
    for j, address in enumerate(w.pipes):
        o[O_PIPES + 4 * j:O_PIPES + 4 * j + 4] = [pw(e, e.u32(address + 8)), pw(e, e.u32(address + 0xC)), e.u32(address + 0x10),
                                                  link_class(e.u32(address + 4))]
    o[O_INFO:O_INFO + 16] = ppc_dump_info(e, w.info)
    o[O_STATE:O_STATE + 4] = [s32(e.u32(0x1003F350)), e.read(0x1003E320, 1)[0], e.u32(0x1003E324), e.u32(0x10035C88)]
    result = collect(o, e.read(w.data, DATA_BYTES), list(L.log))
    result["vptr"] = ch_vptr if op == 0 else True
    return result


def make_test(op):
    name, address = OPS[op]
    return FnTest(f"{name} 0x{address:08x}", ppc_run, host_run, edge=lambda op=op: edges(op), rnd=lambda r, op=op: randoms(op, r))


# ---------------------------------------------------------------- the cases


def pattern(r):
    return r.choice([0, 1, 2, 0x7FFFFFFF, 0x80000000, MASK, 0xFFFE, 0x10000, FILL, 0x1200]) if r.random() < 0.5 else r.getrandbits(32)


def handle_word(r, M, bits=26):
    index = r.randrange(M) if r.random() < 0.9 else 0
    return ((index << bits) | (r.getrandbits(bits) if r.random() < 0.5 else r.choice([0, 1, 2, 0x3FFFFFF]))) & MASK


def rand_channel(r, P, M, table=True):
    w = [0] * 20
    w[0] = r.choice([0, 0, 1, 1, r.getrandbits(8)])
    w[1] = pattern(r)
    w[2] = pattern(r)
    w[3] = r.choice([0, 1, 2, 3, 0xFFFFFFFD, pattern(r)])
    w[4] = pattern(r)
    w[5] = r.choice([0, 1, 2, 4, 8, 15, 0x10, 0xF0, 3, 5, 6, 7, 9, 12, 10, pattern(r)])
    w[6] = pattern(r)
    w[7] = handle_word(r, M)
    w[8] = r.choice([0, 1, pattern(r)])
    w[9] = r.choice([1, 2]) if table else r.choice([0, 1, 2])
    w[10] = r.choice([0, 1, 4, pattern(r)])
    w[11] = pattern(r)
    w[12] = r.choice([0x5622_0001 & MASK, 0x56221000, pattern(r)])
    w[13] = r.choice([0, 1, 2, 0xFFFFFFFF, pattern(r)])
    w[14] = pattern(r)
    w[15] = pattern(r)
    w[18] = r.choice([w[7], pattern(r), 0])
    w[19] = r.choice([0, 1, 2, 5, pattern(r)])
    nxt = r.randrange(P) if P and r.random() < 0.6 else -1
    prv = r.randrange(P) if P and r.random() < 0.3 else r.choice([-1, -1, -2])
    return dict(w=w, next=nxt, prev=prv)


def rand_pipes(r, P):
    pipes = []
    for j in range(P):
        nxt = r.randrange(P) if P > 1 and r.random() < 0.3 else -1
        if nxt == j:
            nxt = -1
        pipes.append([pattern(r) if r.random() < 0.5 else 0, nxt, r.choice([-1, -2, -1]) if r.random() < 0.7 else (j + 1) % P])
    return pipes


def rand_floats(r):
    return [T.f32_bits(r.choice([0.0, 1.0, 0.5, 2.0, 0.25, 1.5, 0.1, 3.0, 100.0, 1e-3, 0.7, 1.0 / 3.0, r.uniform(0, 4), r.uniform(0, 1e6)]))
            for _ in range(8)]


def rand_info(r, vc=True):
    flags = 0
    for bit in (0x400, 2, 0x40000, 0x80, 8, 0x2000, 1, 0x100):
        if r.random() < 0.5:
            flags |= bit
    if r.random() < 0.15:
        flags |= r.getrandbits(32)
    kind = r.choice([1, 2]) if (flags & 0x40000 or vc) else 0
    return [pattern(r), pattern(r), flags, pattern(r), r.choice([0, 1, 5, 100, 0x7FFFFFFF, 0x80000000, MASK, pattern(r)]), kind, r.choice([0, 1, 2, 3, 5, 15, 0x10, pattern(r)]),
            pattern(r), pattern(r), pattern(r), pattern(r), pattern(r)]


def rand_world(r, op, M=None, P=None, mask_ok=True):
    M = M if M is not None else r.choice([1, 2, 3, 4, 5, 7, 8])
    P = P if P is not None else r.choice([0, 1, 2, 3, 4])
    used = r.choice([M, M, r.randrange(M + 1), 0, 1 if M else 0])
    c = new_case(op, M=M, used=min(used, M), target=r.randrange(M))
    c["ch"] = [rand_channel(r, P, M) for _ in range(min(M, 8))]
    c["pipes"] = rand_pipes(r, P)
    c["info"] = rand_info(r)
    c["vol"] = rand_floats(r)
    c["sys"] = r.choice([0, 1, M, M + 1, 3, 10, 0xFFFFFFFF, pattern(r)])
    c["skews"] = [r.randrange(32) for _ in range(8)]
    c["globals"] = (r.choice([0, 1]), r.choice([0, (1 << 27) - 1, pattern(r)]), r.choice([26, 26, 26, 25, 27, 31, 32, 33]), r.randrange(-3, 5) & MASK)
    c["extra"] = r.choice([-1, r.randrange(M)])
    c["queues"] = {"Chunk": [r.choice([1, 0, -1, 2, 1, 1])], "Done": [r.choice([0, 0x10, 0x1200, 0x1300, 0x7FFFFFFF, 0x80000000, MASK, 5])],
                   "SetInfoErr": [r.choice([0, -1, 5])], "FindPipe": [r.choice([-1, -2, 0, 1, 2])] * 2, "Attatch": [r.choice([-1, 0, 1, 2])],
                   "StopErr": [r.choice([0, -1])], "Codec": [r.choice([-1, 0, 1, 2, 3])], "Control": [r.choice([-1, 0, 1, 2, 3])]}
    c["a"] = [pattern(r) for _ in range(8)]
    c["a"][2] = r.choice([0, 0, 0x10, 0x100, 0x1000, 0x1FF0, 0x2000, -0x10, -0x1000, -0x2000])
    return c


def edges(op):
    r = random.Random(op * 7919 + 1)
    out = []
    if op == 0:
        # the constructor over every skew (low five bits of the allocation address), M irrelevant
        for skew in range(32):
            out.append(new_case(0, M=1, skews=[skew]))
    elif op == 1:
        for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFFFFFF, 0x1FFFF):
            out.append(new_case(1, M=1, a=[flag] + [0] * 7))
    elif op == 5:
        for words in ([1, 0x56220001, 2, 3, 4], [0x7FFFFFFF, MASK, MASK, MASK, MASK], [0, 0, 0, 0, 0], [5, 6, 7, 8, 9]):
            c = rand_world(r, 5, M=2, P=0)
            c["a"] = words + [0, 0, 0]
            out.append(c)
    elif op == 18:
        for mflags in (0, 2, 1, 3, MASK):
            for iflags in (0, 0x400, 2, 0x40000, 0x400 | 2, 0x400 | 0x40000, 2 | 0x40000, 0x400 | 2 | 0x40000, MASK):
                for kind in (1, 2):
                    c = new_case(18, M=2)
                    c["ch"][0]["w"][3] = mflags
                    c["ch"][0]["w"][2] = 0x1111
                    c["ch"][0]["w"][5] = 0x77
                    c["ch"][0]["w"][1] = 0x55
                    c["ch"][0]["w"][7] = 3 << 26 | 9
                    c["info"] = [0xC0FFEE, 0x1234, iflags, 0x8888, 0x9999, kind, 0xA, 0x6, 7, 8, 9, 10]
                    out.append(c)
    elif op == 12:
        for mask in (0, 1, 2, 4, 8, 0xF, 0x10, 0x20, MASK):
            for control in (0, 1, 3, 0x10, MASK):
                c = new_case(12, M=1, a=[mask] + [0] * 7)
                c["ch"][0]["w"][5] = control
                out.append(c)
    elif op == 15:
        for bits in (26, 25, 0, 31, 32):
            for index in (0, 1, 2, 63, 64, 0xFFFF, 0x10000, 0x12345):
                for cached, mask in ((0, 0), (1, 0x7FFFFFF), (1, 0x3FFFFFF)):
                    c = new_case(15, M=1, a=[index] + [0] * 7, globals=(cached, mask, bits, 0))
                    c["ch"][0]["w"][7] = r.choice([0, 1 << 26, 0x5555555, MASK])
                    out.append(c)
    elif op == 27:
        for mask in range(0, 32):
            for kind in (1, 2):
                c = new_case(27, M=1)
                c["ch"][0]["w"][5] = mask
                c["ch"][0]["w"][9] = kind
                c["ch"][0]["w"][1] = 1000
                c["vol"] = [T.f32_bits(x) for x in (0.5, 0.25, 0.75, 0.9, 0.2, 2.0, 1.5, 3.0)]
                out.append(c)
        for volume in (0, 1, 2, 0x7FFFFF, 0x800000, 0x1000001, 0xFFFFFF, 0x1FFFFFF, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF, 0xFFFFFFC0, 0xFFFFFF80):
            for table in (0x3F800000, 0x3F000000, 0x3F800001, 0x3F7FFFFF, 0x40000000, 0x3FFFFFFF, 0x34000000, 0x7F000000, 0x3E99999A, 0x80000000, 0xBF800000, 0x7F7FFFFF):
                c = new_case(27, M=1)
                c["ch"][0]["w"][5] = 1
                c["ch"][0]["w"][9] = 1
                c["ch"][0]["w"][1] = volume
                c["vol"] = [table, 0x3F800000, 0x3F800000, 0x3F800000, 0, 0, 0, 0]
                out.append(c)
        for a_bits in (0x3F800001, 0x3FAAAAAB, 0x3F7FFFFF, 0x3E99999A):
            for b_bits in (0x3F800001, 0x3FAAAAAB, 0x3F7FFFFF, 0x3E99999A, 0x40490FDB):
                for volume in (0xFFFFFFFF, 0x12345679, 3, 0x1000003):
                    c = new_case(27, M=1)
                    c["ch"][0]["w"][5] = 3
                    c["ch"][0]["w"][9] = 1
                    c["ch"][0]["w"][1] = volume
                    c["vol"] = [a_bits, b_bits, 0x3F800000, 0x3F800000, 0, 0, 0, 0]
                    out.append(c)
        for table in (0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF):
            for volume in (0, 1, 5, MASK):
                c = new_case(27, M=1)
                c["ch"][0]["w"][5] = 1
                c["ch"][0]["w"][9] = 1
                c["ch"][0]["w"][1] = volume
                c["vol"] = [table, 0, 0, 0, 0, 0, 0, 0]
                out.append(c)
        # bits above 3 are ignored; no table is read when the mask is 0
        c = new_case(27, M=1)
        c["ch"][0]["w"][5] = 0xF0
        c["ch"][0]["w"][1] = 0xFFFFFFFF
        out.append(c)
    elif op == 32:
        for M in (0, 1, 3, 7, 8):
            for n in sorted({0, 1, max(M - 1, 0), M, M + 1, 0x10000, MASK}):
                c = new_case(32, M=M, used=0, a=[n] + [0] * 7)
                c["pipes"] = []
                out.append(c)
    elif op == 50:
        # the destructor of the array holder, in place (flag not positive) and deleting, with the pointer set and with it NULL
        # (a[2]: the 0x1000fb14 test, never taken by a holder that was built by the count constructor)
        for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000):
            for null in (0, 1):
                out.append(new_case(50, M=2, a=[flag, 2, null, 0, 0, 0, 0, 0], ch=[]))
    elif op == 24:
        # the pipe's answer is compared as a whole word with 1 (cmpwi): only 1 turns it into false
        for chunk in (1, 0, -1, 2, 3, 0x100, 0x101, 0x7FFFFFFF, -0x80000000, -0x7FFFFFFF):
            for size in (0, 0x1200):
                c = new_case(24, M=1, pipes=[[0, -1, -1]], a=[size, 1, 0, 0, 0, 0, 0, 0])
                c["queues"] = {"Chunk": [chunk], "Done": [0x10]}
                out.append(c)
    elif op in (34, 35, 36):
        for M in (0, 1, 8):
            out.append(new_case(op, M=M, used=0, a=[0] + [0] * 7))
            out.append(new_case(op, M=M, used=M, a=[M - 1 if M else 0] + [0] * 7))
    elif op == 45:
        # all busy: the request's priority below, equal to and above the lowest; ties between channels; free ones past the used count
        for request in (0, 4, 5, 6, MASK):
            for priorities in ((5, 9, 7), (9, 5, 7), (7, 9, 5), (5, 5, 9), (9, 5, 5), (5, 5, 5), (MASK, MASK, MASK), (0, 0, 0)):
                for used in (0, 1, 2, 3):
                    c = new_case(45, M=4, used=used, pipes=[[0, -1, -1]])
                    c["info"] = [0, 0x11, 0x400, 0, request, 1, 0, 0, 0, 0, 0, 0]
                    for k in range(4):
                        c["ch"][k]["w"][0] = 1
                        c["ch"][k]["w"][13] = priorities[k] if k < 3 else 1
                        c["ch"][k]["w"][7] = (k << 26) | (k + 1)
                    out.append(c)
        # a free channel is claimed whatever its position, a pipe makes it busy
        for free in range(4):
            c = new_case(45, M=4, used=4, pipes=[[0, -1, -1]])
            for k in range(4):
                c["ch"][k]["w"][0] = 0 if k == free else 1
                c["ch"][k]["next"] = 0 if k == (free + 1) % 4 else -1
                c["ch"][k]["w"][13] = 3
            c["info"] = [0, 0, 0, 0, 100, 1, 0, 0, 0, 0, 0, 0]
            out.append(c)
    elif op in (46, 47, 48):
        # every way to the pipe (a free channel, a channel that holds the handle, a pipe behind one, nothing), the flag gates, the
        # factory's codec or none, and the attach's answer
        handle = (1 << 26) | 7
        for kind in ("null", "channel", "pipe", "none"):
            for flags in (0x88, 0x80, 0x08, 0, 0x2000, 0x2088, 0x400 | 0x88, 0x2000 | 0x400):
                for factory in (-1, 0):
                    for attach in (-1, 1):
                        c = new_case(op, M=3, used=3, pipes=[[0, -1, -1], [0, -1, -1]])
                        for k in range(3):
                            c["ch"][k]["w"][0] = 0 if k == 0 else 1
                            c["ch"][k]["w"][13] = 2 + k
                            c["ch"][k]["w"][7] = (k << 26) | (k + 1)
                        c["info"] = [0xAA, 0, flags, 0x40, 77, 1, 0, 0, 0, 0, 5, 6]
                        c["a"] = [0 if kind == "null" else handle] + [0] * 7
                        if kind == "channel":
                            c["ch"][1]["w"][18] = handle
                        if kind == "pipe":
                            c["ch"][1]["next"] = 0
                            c["queues"]["FindPipe"] = [1]
                        c["queues"]["Codec"] = [factory]
                        c["queues"]["Control"] = [factory]
                        c["queues"]["Attatch"] = [attach]
                        out.append(c)
    elif op in (3, 6, 8):
        for value in (0, 1, 0x7FFFFFFF, 0x80000000, MASK, FILL):
            c = new_case(op, M=1, a=[value] + [0] * 7)
            c["ch"][0]["w"] = [FILL] * 20
            out.append(c)
    elif op == 21 or op == 37 or op == 38 or op == 39:
        for index in range(4):
            for count in (0, 1, 5):
                for match in (0, 1):
                    h = (index << 26) | count
                    c = new_case(op, M=4, used=4, target=0, pipes=[[h if match else h + 1, -1, -1], [h, -1, -1]], a=[h] + [0] * 7)
                    c["ch"][index]["w"][18] = h if match else 0
                    c["ch"][index]["next"] = 0 if not match else -1
                    c["queues"] = {"FindPipe": [-1, 1, -2]}
                    out.append(c)
        # the last channel of a full array is found; a handle with the last index
        c = new_case(op, M=8, used=8, a=[(7 << 26) | 3] + [0] * 7)
        c["ch"][7]["w"][18] = (7 << 26) | 3
        out.append(c)
    return out


def randoms(op, r):
    if op == 0:
        return new_case(0, M=1, skews=[r.randrange(32)])
    if op == 1:
        return new_case(1, M=1, a=[r.choice([0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, r.getrandbits(32)])] + [0] * 7)
    if op == 30:
        M = r.choice([0, 1, 2, 3, 5, 7, 8])
        return new_case(30, M=M, skews=[r.randrange(32) for _ in range(8)], ch=[])
    if op == 49:
        count = r.choice([0, 1, 2, 3, 7])
        return new_case(49, M=count, a=[0, count, 0, 0, 0, 0, 0, 0], skews=[r.randrange(32) for _ in range(8)], ch=[])
    if op in (50, 51):
        count = r.choice([0, 1, 2, 3, 7])
        return new_case(op, M=count, a=[r.choice([0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000]), count, 0, 0, 0, 0, 0, 0],
                        skews=[r.randrange(32) for _ in range(8)], ch=[])
    c = rand_world(r, op)
    M = c["M"]
    P = len(c["pipes"])
    t = c["target"]
    a = c["a"]
    if op == 15:
        a[0] = r.choice([0, 1, 2, 63, 64, 0xFFFF, 0x10000, r.getrandbits(16), r.getrandbits(32)])
    if op in (21, 37, 38, 39, 16):
        a[0] = handle_word(r, M, 26)
        c["globals"] = (c["globals"][0], c["globals"][1], 26, c["globals"][3])
        owner = t if op in (21, 16) else min(a[0] >> 26, len(c["ch"]) - 1)
        if r.random() < 0.6:
            c["ch"][owner]["w"][18] = a[0]
    if op in (20,):
        a[0] = r.choice([-1, -1, 0, 1, 2, 3]) if P else -1
    if op == 22:
        a[1] = r.choice([0, 1, 0x10, 0x1200, 0x1000, 0x2000, 0x7FFFFFFF, 0x80000000, MASK, 5, 0x20])
        a[3] = r.getrandbits(1)
        c["ch"][t]["next"] = r.randrange(P) if P and r.random() < 0.75 else -1
    if op == 23:
        c["ch"][t]["next"] = r.randrange(P) if P and r.random() < 0.75 else -1
        c["queues"]["Chunk"] = [r.choice([1, 0, -1, 2])]
    if op == 24:
        a[1] = r.choice([0, 0, 0] + list(range(1, P + 1)))
        a[0] = r.choice([0, 0x10, 0x1200, 0x1000, 0x2000, 0x20])
    if op == 26:
        a[0] = r.getrandbits(1)
    if op == 27:
        c["ch"][t]["w"][9] = r.choice([1, 2])
    if op in (45, 46, 47, 48):
        c["info"][5] = r.choice([1, 2])
        if op == 46 or op == 47 or op == 48:
            handle = handle_word(r, M, 26)
            a[0] = r.choice([0, 0, handle])
            if a[0]:
                if r.random() < 0.6:
                    c["ch"][min(a[0] >> 26, len(c["ch"]) - 1)]["w"][18] = a[0]
                c["globals"] = (c["globals"][0], c["globals"][1], 26, c["globals"][3])
        if op in (47, 48):
            c["info"][2] |= r.choice([0x88, 0x2000, 0x80, 0x8, 0x2088])
    if op == 31:
        a[0] = r.choice([0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000])
    if op == 36:
        a[0] = r.randrange(M) if r.random() < 0.8 else M - 1
    if op in (45,):
        # make some channels busy and others free, priorities spread
        for chn in c["ch"]:
            chn["w"][0] = r.choice([0, 1, 1, 1])
            chn["w"][13] = r.choice([0, 1, 2, 5, 100, 0xFFFFFFFF, pattern(r)])
            chn["next"] = r.choice([-1, 0 if P else -1])
    if op in (42, 43, 44):
        for chn in c["ch"]:
            chn["next"] = r.choice([-1, r.randrange(P)]) if P else -1
        a[0] = r.choice([0, 1, 2, 4, 8, 0xF, r.getrandbits(32)]) if op == 44 else r.getrandbits(1)
    return c


TESTS = [make_test(op) for op in sorted(OPS)]
