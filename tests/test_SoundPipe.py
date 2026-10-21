"""Unit w78, the sound library's sample pipe base, against libtpw_unit_SoundPipe.dylib (tests/unit_SoundPipe.cpp), in the program
"sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/StreamableMpeg2.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleCallback.cpp, src/sound/TbSoundSampleCodec.cpp, src/sound/TbSoundSamplePipe.cpp, src/sound/TbSoundSystemInfo.cpp  TbSoundSamplePipe: destructor 0x10000248, constructor 0x10000344, SetHandle 0x10000bb8, GetDataChunk
                       0x10001148, GetSampleRate 0x1000118c, Restart 0x10001204, SetDataStart 0x1000123c, ClearCache 0x1000edb8,
                       operator new 0x1000ee48, operator delete 0x1000ef14, TopUp 0x1000efb8, SetLevel 0x1000f098, Attatch
                       0x1000f440, AttachReplace 0x1000f4e4, Release 0x1000f588, Completed 0x1000f628, SetInfo 0x1000f694,
                       GetInfo 0x1000f734, FindPipe 0x1000f7b0, Detatch 0x1000f834;
                       TbSoundSPCodec: destructor 0x100005b0, constructor 0x10000644, GetCodec 0x1000866c, Restart 0x1000aff4,
                       GetDataStart 0x1000b0f4, SetDataPosition 0x1000b164, GetDataPosition 0x1000b1e8, GetDataSize 0x1000b25c,
                       SetSampleRate 0x1000b2cc, GetSampleRate 0x1000b350, GetBitRate 0x1000b3c0, GetChannels 0x1000b430,
                       GetInfo 0x1000b4a0, SetInfo 0x10011cd4;
                       TbSoundSampleCodec: destructor 0x10000e1c, GetDataPosition 0x1000853c, GetDataStart 0x10008580,
                       GetDataSize 0x100085c0, GetInfo 0x10008600, GetSampleFormat 0x1000d4c8, constructor 0x1000d694, SetInfo
                       0x1000d774, SetSampleFormatFromHeader 0x1000d848, SetSampleRate 0x1000d914, GetSampleRate 0x1000d9b8,
                       GetBitRate 0x1000da20, GetChannels 0x1000da78, SetTime 0x1000dad0, IsTimeComplete 0x1000db98,
                       SetDataPosition 0x1000dc1c, Timed 0x1000e818, default constructor 0x100137a4

Each function runs as the original in the emulator and as the host's translation, in scripts of operations on a world of six
node slots (a pipe, an SP codec, a sample codec made with a header, a default sample codec, or a FakeNode that records every
virtual slot). After every operation the state of every slot (kind, mNext, mPrevious, mHandle, what the pool link holds, the
owner, size, data start, time, position and format byte of a codec), the pool (its six statics and the chain of idle objects as
labels), the sample info's flag word, the result and the log of events are compared. Every callee outside the unit is a fake that
records the same event on both sides: the runtime's new (the Mac's size and a label; blocks are filled with 0xA5) and delete (a
labelled block), the clock (LbTime_GetClock__Fv), the owner's AddRef and Release, and the FakeNode's slots (the virtual calls
the real code makes through the Mac vtable: a slot number that is wrong shows as another name in the log). The real vtables
0x1003c35c, 0x1003c560 and 0x1003c21c are the Mac's own; the fake node's vtable has a recording function in each slot 0x08 to 0x5c.
The host's objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are mapped:
0x14 is the host pipe (and SP codec), 0x2c the host sample codec.

Pointers compare as labels (0 NULL, the label of the object, 0xA5A5A5A5 for the fill, -1 for anything else); the data start as the
number of the sample header and the offset in it. A script only calls functions that are safe on the chain it built (the Mac code
calls through NULL and through slots a pipe does not have; the model in Chain.safe says which).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundPipe"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

# vtables
PIPE_VT, SP_VT, CODEC_VT = 0x1003C35C, 0x1003C560, 0x1003C21C
# statics
S_TOTAL, S_CURRENT, S_LOW, S_HIGH, S_FIRST, S_LEVEL = 0x1003F294, 0x1003F298, 0x1003F29C, 0x1003F2A0, 0x1003F2A4, 0x1003C34C
# functions
A = dict(
    p_dtor=0x10000248, p_ctor=0x10000344, set_handle=0x10000BB8, chunk=0x10001148, p_rate=0x1000118C, p_restart=0x10001204,
    p_setstart=0x1000123C, clear=0x1000EDB8, new=0x1000EE48, delete=0x1000EF14, topup=0x1000EFB8, setlevel=0x1000F098,
    attatch=0x1000F440, replace=0x1000F4E4, release=0x1000F588, completed=0x1000F628, p_setinfo=0x1000F694, p_getinfo=0x1000F734,
    find=0x1000F7B0, detatch=0x1000F834,
    sp_dtor=0x100005B0, sp_ctor=0x10000644, sp_getcodec=0x1000866C, sp_restart=0x1000AFF4, sp_getstart=0x1000B0F4,
    sp_setpos=0x1000B164, sp_getpos=0x1000B1E8, sp_getsize=0x1000B25C, sp_setrate=0x1000B2CC, sp_getrate=0x1000B350,
    sp_getbit=0x1000B3C0, sp_getch=0x1000B430, sp_getinfo=0x1000B4A0, sp_setinfo=0x10011CD4,
    c_dtor=0x10000E1C, c_getpos=0x1000853C, c_getstart=0x10008580, c_getsize=0x100085C0, c_getinfo=0x10008600,
    c_getfmt=0x1000D4C8, c_ctor=0x1000D694, c_setinfo=0x1000D774, c_setfmt=0x1000D848, c_setrate=0x1000D914,
    c_getrate=0x1000D9B8, c_getbit=0x1000DA20, c_getch=0x1000DA78, c_settime=0x1000DAD0, c_istime=0x1000DB98,
    c_setpos=0x1000DC1C, c_timed=0x1000E818, c_ctor0=0x100137A4,
)
# the imports
IMP_NEW, IMP_DEL, IMP_CLOCK = "__nw__FUl", "__dl__FPv", "LbTime_GetClock__Fv"

# the kinds of node
K_NONE, K_PIPE, K_SP, K_CODEC, K_DEFAULT, K_FAKE = range(6)
SLOTS = 6
HOST_SIZES = {}

# the operations (the order of enum Op in the unit file)
OPS = [
    "NEW", "DTOR", "SET_HANDLE", "GET_DATA_CHUNK", "PIPE_GET_SAMPLE_RATE", "PIPE_RESTART", "PIPE_SET_DATA_START", "PIPE_GET_INFO",
    "PIPE_SET_INFO", "PIPE_COMPLETED", "FIND_PIPE", "ATTATCH", "ATTACH_REPLACE", "RELEASE", "DETATCH", "NEWOP", "DELETEOP", "TOPUP",
    "SETLEVEL", "CLEARCACHE", "SP_GET_CODEC", "SP_RESTART", "SP_GET_DATA_START", "SP_SET_DATA_POSITION", "SP_GET_DATA_POSITION",
    "SP_GET_DATA_SIZE", "SP_SET_SAMPLE_RATE", "SP_GET_SAMPLE_RATE", "SP_GET_BIT_RATE", "SP_GET_CHANNELS", "SP_GET_INFO",
    "SP_SET_INFO", "SC_GET_DATA_POSITION", "SC_GET_DATA_START", "SC_GET_DATA_SIZE", "SC_GET_INFO", "SC_GET_SAMPLE_FORMAT",
    "SC_SET_INFO", "SC_SET_FORMAT_FROM_HEADER", "SC_SET_SAMPLE_RATE", "SC_GET_SAMPLE_RATE", "SC_GET_BIT_RATE", "SC_GET_CHANNELS",
    "SC_SET_TIME", "SC_IS_TIME_COMPLETE", "SC_SET_DATA_POSITION", "SC_TIMED", "POKE",
]
globals().update({"OP_" + name: index for index, name in enumerate(OPS)})
NO_NODE = {OP_NEW, OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL, OP_CLEARCACHE}

STUBS = None
VT = {}


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


# ---------------------------------------------------------------- the PowerPC side

def call(emu, entry, args):
    """A top-level call with the stack below the frame cleared (the host's local buffers start as zeros)."""
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
    emu.write(sp - 0x4000, b"\0" * 0x4000)
    return T.call_ppc(emu, entry, args)[0]


class World:
    """What one case's fakes work on: the node slots, the blocks of the raw pool operations, the fixtures."""


def stub_new(e):
    L, size = e.cl, e.r(3)
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    L.rec("new", (size, L.newlab(block)))
    e.set_r(3, block)


def stub_delete(e):
    L, block = e.cl, e.r(3)
    if block != 0 and L.lab(block) > 0:
        L.rec("delete", (L.lab(block),))
        L.forget(block)


def stub_clock(e):
    L = e.cl
    L.rec("Clock", ())
    e.set_r(3, T.u32(L.ret("Clock", 0)))


def build_stubs():
    S = CL.Stubs()
    S.imp(IMP_NEW, stub_new)
    S.imp(IMP_DEL, stub_delete)
    S.imp(IMP_CLOCK, stub_clock)
    return S


def queued_node(e, key, this):
    """A queue value that names a node: -1 NULL, -2 the node's own mNext, otherwise a slot."""
    value = e.cl.ret(key, -1)
    if value == -2:
        return e.u32(this + 8)
    return e.w.nodes[value] if 0 <= value < SLOTS else 0


def fake_slots():
    """The FakeNode's slot functions, in the order of the Mac vtable from +0x08."""
    def rec(name, *fields):
        def stub(e):
            L, this = e.cl, e.r(3)
            ints = [L.lab(this)]
            for index, kind in enumerate(fields):
                value = e.r(4 + index)
                if kind == "l":
                    ints.append(L.lab(value))
                elif kind == "u":
                    ints.append(value & MASK)
                elif kind == "i":
                    ints.append(s32(value))
                elif kind == "h":
                    ints.append(value & 0xFFFF)
                elif kind == "b":
                    ints.append(value & 0xFF)
            L.rec(name, ints)
        return stub

    def with_result(name, key, *fields):
        base = rec(name, *fields)

        def stub(e):
            base(e)
            e.set_r(3, T.u32(e.cl.ret(key, 0)))
        return stub

    def dtor(e):
        L, this, flag = e.cl, e.r(3), T.s16(e.r(4))
        L.rec("Fake.~", (L.lab(this), flag))
        if flag > 0:
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
            L.forget(this)

    def attatch(e):
        rec("Fake.Attatch", "l")(e)
        e.set_r(3, 0)

    def find(e):
        rec("Fake.FindPipe", "u")(e)
        e.set_r(3, queued_node(e, "FindPipe", e.r(3)))

    def set_info(e):
        L, this, info = e.cl, e.r(4), e.r(5)
        L.rec("Fake.SetInfo", (L.lab(this), L.lab(info)))
        ws(e, e.r(3), [L.ret("SetInfoErr", 0)])

    def set_handle(e):
        rec("Fake.SetHandle", "u")(e)
        e.set_r(3, e.r(4))

    def get_codec(e):
        rec("Fake.GetCodec")(e)
        e.set_r(3, queued_node(e, "Codec", e.r(3)))

    def set_time(e):
        rec("Fake.SetTime", "b", "u")(e)
        e.set_r(3, 1 if e.cl.ret("SetTime", 0) != 0 else 0)

    def chunk(e):
        L, this = e.cl, e.r(3)
        L.rec("Fake.GetDataChunk", (L.lab(this), e.r(4), s32(e.r(5)), e.r(7) & 0xFF))
        e.set_r(3, T.u32(L.ret("Chunk", 0)))

    return [
        dtor, attatch, rec("Fake.Release"), rec("Fake.Detatch"), find, rec("Fake.GetInfo", "l"),
        with_result("Fake.GetSampleRate", "Rate"), chunk, rec("Fake.Restart"), set_info, set_handle,
        rec("Fake.Completed", "b"), rec("Fake.SetDataStart", "u"), with_result("Fake.GetDataStart", "DataStart"),
        rec("Fake.SetDataPosition", "u"), with_result("Fake.GetDataPosition", "DataPosition"),
        with_result("Fake.GetDataSize", "DataSize"), rec("Fake.SetSampleRate", "h"), with_result("Fake.GetBitRate", "BitRate"),
        with_result("Fake.GetChannels", "Channels"), get_codec, set_time,
    ]


def owner_slots():
    """The owner's slots: +8 Release, +0xc AddRef."""
    def release(e):
        L = e.cl
        L.rec("Owner.Release", (L.lab(e.r(3)),))
        e.set_r(3, 0)

    def addref(e):
        L = e.cl
        L.rec("Owner.AddRef", (L.lab(e.r(3)),))
        e.set_r(3, 0)

    return [release, addref]


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs()
    words = [0, 0] + [T.fake_function(e, fn) for fn in fake_slots()]
    VT["fake"] = T.persist_alloc(e, 4 * len(words), struct.pack(">%dI" % len(words), *words))
    words = [0, 0] + [T.fake_function(e, fn) for fn in owner_slots()]
    VT["owner"] = T.persist_alloc(e, 4 * len(words), struct.pack(">%dI" % len(words), *words))
    lib = runner.lib
    lib.tpw_sp_sizes.restype = C.c_int32
    lib.tpw_sp_sizes.argtypes = [C.c_int32]
    HOST_SIZES.update(pipe=lib.tpw_sp_sizes(0), codec=lib.tpw_sp_sizes(1), info=lib.tpw_sp_sizes(2), header=lib.tpw_sp_sizes(3))
    lib.tpw_sp_op_count.restype = C.c_int32
    assert lib.tpw_sp_op_count() == len(OPS), "the operations of the unit file and of the test differ"


def pool_chain(e):
    chain, node = [], e.u32(S_FIRST)
    while node and len(chain) < 64:
        chain.append(node)
        node = e.u32(node + 4)
    return chain


def alive(e, w, slot):
    """A node exists, has its label and is not in the pool."""
    node = w.nodes[slot]
    return node != 0 and e.cl.lab(node) > 0 and node not in pool_chain(e)


def pw(L, address):
    """A pointer field as the unit file's PW compares it."""
    if address == 0:
        return 0
    label = L.lab(address)
    if label > 0:
        return label
    return FILL if address == FILL else -1


def start_word(e, w, address):
    """A data start pointer: the number of the header and the offset in it, or the 32-bit word."""
    for index, header in enumerate(w.headers):
        if header <= address < header + 0x60:
            return 0x10000 * (index + 1) + address - header
    return address


def link_class(address):
    return 0 if address == 0 else 1 if address == FILL else 2


def ppc_slot_state(e, w, slot):
    out = [0] * 12
    node = w.nodes[slot]
    if node == 0:
        return out
    if not alive(e, w, slot):
        out[0] = 1
        return out
    L = e.cl
    out[0], out[1] = 2, w.kinds[slot]
    out[2], out[3], out[4], out[5] = pw(L, e.u32(node + 8)), pw(L, e.u32(node + 0xC)), e.u32(node + 0x10), link_class(e.u32(node + 4))
    if w.kinds[slot] >= K_CODEC:
        out[6], out[7] = pw(L, e.u32(node + 0x14)), e.u32(node + 0x18)
        out[8], out[9], out[10] = start_word(e, w, e.u32(node + 0x1C)), e.u32(node + 0x20), e.u32(node + 0x24)
        out[11] = e.read(node + 0x28, 1)[0]
    return out


def ppc_pool_state(e):
    L = e.cl
    chain = pool_chain(e)
    out = [pw(L, e.u32(S_FIRST)), s32(e.u32(S_CURRENT)), s32(e.u32(S_LOW)), s32(e.u32(S_HIGH)), s32(e.u32(S_TOTAL)), s32(e.u32(S_LEVEL))]
    out += [pw(L, node) for node in chain[:40]]
    out.append(-9)
    return out


def ppc_block_state(e, w, index):
    out = [0] * 5
    block = w.blocks[index]
    if block == 0:
        return out
    L = e.cl
    if L.lab(block) <= 0:
        out[0] = 1
        return out
    if block in pool_chain(e):
        out[0] = 3
        return out
    out[0] = 2
    if w.bsizes[index] >= 0x14 and w.bsizes[index] != 0x15:
        out[1], out[2], out[3], out[4] = pw(L, e.u32(block + 8)), pw(L, e.u32(block + 0xC)), e.u32(block + 0x10), link_class(e.u32(block + 4))
    return out


def node_address(w, slot):
    return w.nodes[slot] if 0 <= slot < SLOTS else 0


def ppc_op(e, w, op, a, b, c, d):
    L = e.cl
    aux = [0, 0]
    if op == OP_NEW:
        if not 0 <= a < SLOTS:
            return -99, aux
        size = 0x14 if b in (K_PIPE, K_SP) else 0x2c
        block = call(e, A["new"], [size])
        if b == K_PIPE:
            call(e, A["p_ctor"], [block])
        elif b == K_SP:
            call(e, A["sp_ctor"], [block])
        elif b == K_CODEC:
            call(e, A["c_ctor"], [block, w.headers[c], w.owners[d]])
        else:
            call(e, A["c_ctor0"], [block])
            if b == K_FAKE:
                ws(e, block, [VT["fake"]])
        for i in range(SLOTS):
            if w.nodes[i] == block:
                w.nodes[i] = 0
        w.nodes[a] = block
        w.kinds[a] = b
        return L.lab(block), aux
    if op not in NO_NODE:
        if not 0 <= a < SLOTS or not alive(e, w, a):
            return -99, aux
        kind = w.kinds[a]
        if OP_SP_GET_CODEC <= op <= OP_SP_SET_INFO and kind < K_SP:
            return -99, aux
        if OP_SC_GET_DATA_POSITION <= op <= OP_SC_TIMED and kind < K_CODEC:
            return -99, aux
        if op == OP_POKE and b > 0 and kind < K_CODEC:
            return -99, aux
    obj = node_address(w, a)
    if op == OP_DTOR:
        vt = e.u32(obj)
        call(e, e.u32(e.u32(vt + 8)), [obj, b & MASK])
        w.nodes[a] = 0
        w.kinds[a] = K_NONE
        return 0, aux
    if op == OP_SET_HANDLE:
        return call(e, A["set_handle"], [obj, b & MASK]), aux
    if op == OP_GET_DATA_CHUNK:
        done = e.alloc(8, struct.pack(">II", 0x5555, 0))
        result = s32(call(e, A["chunk"], [obj, b & MASK, c & MASK, done, 1 if d else 0]))
        aux[0] = s32(e.u32(done))
        return result, aux
    if op == OP_PIPE_GET_SAMPLE_RATE:
        return call(e, A["p_rate"], [obj]), aux
    if op == OP_PIPE_RESTART:
        call(e, A["p_restart"], [obj])
        return 0, aux
    if op == OP_PIPE_SET_DATA_START:
        call(e, A["p_setstart"], [obj, b & MASK])
        return 0, aux
    if op == OP_PIPE_GET_INFO:
        call(e, A["p_getinfo"], [obj, w.info])
        return 0, aux
    if op in (OP_PIPE_SET_INFO, OP_SP_SET_INFO, OP_SC_SET_INFO):
        address = {OP_PIPE_SET_INFO: A["p_setinfo"], OP_SP_SET_INFO: A["sp_setinfo"], OP_SC_SET_INFO: A["c_setinfo"]}[op]
        result = e.alloc(16, b"\xa5" * 16)
        call(e, address, [result, obj, w.info])
        return s32(e.u32(result)), aux
    if op == OP_PIPE_COMPLETED:
        call(e, A["completed"], [obj, 1 if b else 0])
        return 0, aux
    if op == OP_FIND_PIPE:
        return L.lab(call(e, A["find"], [obj, b & MASK])), aux
    if op == OP_ATTATCH:
        if b >= 0 and (b >= SLOTS or not alive(e, w, b)):
            return -99, aux
        return L.lab(call(e, A["attatch"], [obj, node_address(w, b)])), aux
    if op == OP_ATTACH_REPLACE:
        if not 0 <= b < SLOTS or not alive(e, w, b):
            return -99, aux
        return L.lab(call(e, A["replace"], [obj, node_address(w, b), node_address(w, c)])), aux
    if op == OP_RELEASE:
        call(e, A["release"], [obj])
        return 0, aux
    if op == OP_DETATCH:
        call(e, A["detatch"], [obj])
        return 0, aux
    if op == OP_NEWOP:
        block = call(e, A["new"], [b & MASK])
        w.blocks[a] = block
        w.bsizes[a] = b
        return L.lab(block), aux
    if op == OP_DELETEOP:
        call(e, A["delete"], [0 if a < 0 else w.blocks[a], b & MASK])
        if a >= 0 and c:
            w.blocks[a] = 0
        return 0, aux
    if op == OP_TOPUP:
        call(e, A["topup"], [])
        return 0, aux
    if op == OP_SETLEVEL:
        call(e, A["setlevel"], [a & MASK])
        return 0, aux
    if op == OP_CLEARCACHE:
        call(e, A["clear"], [])
        return 0, aux
    simple = {
        OP_SP_GET_CODEC: "sp_getcodec", OP_SP_RESTART: "sp_restart", OP_SP_GET_DATA_START: "sp_getstart",
        OP_SP_GET_DATA_POSITION: "sp_getpos", OP_SP_GET_DATA_SIZE: "sp_getsize", OP_SP_GET_SAMPLE_RATE: "sp_getrate",
        OP_SP_GET_BIT_RATE: "sp_getbit", OP_SP_GET_CHANNELS: "sp_getch", OP_SC_GET_DATA_POSITION: "c_getpos",
        OP_SC_GET_DATA_START: "c_getstart", OP_SC_GET_DATA_SIZE: "c_getsize", OP_SC_GET_SAMPLE_FORMAT: "c_getfmt",
        OP_SC_GET_SAMPLE_RATE: "c_getrate", OP_SC_GET_BIT_RATE: "c_getbit", OP_SC_GET_CHANNELS: "c_getch",
        OP_SC_IS_TIME_COMPLETE: "c_istime", OP_SC_TIMED: "c_timed",
    }
    if op in simple:
        r = call(e, A[simple[op]], [obj])
        if op == OP_SP_GET_CODEC:
            return L.lab(r), aux
        if op == OP_SP_GET_DATA_START or op == OP_SC_GET_DATA_START:
            return start_word(e, w, r & MASK), aux
        if op == OP_SP_RESTART:
            return 0, aux
        if op == OP_SC_GET_SAMPLE_FORMAT:
            return r & 0xFF, aux
        if op == OP_SC_IS_TIME_COMPLETE:
            return r & 0xFF, aux
        return r & MASK, aux
    if op == OP_SP_SET_DATA_POSITION:
        call(e, A["sp_setpos"], [obj, b & MASK])
        return 0, aux
    if op == OP_SC_SET_DATA_POSITION:
        call(e, A["c_setpos"], [obj, b & MASK])
        return 0, aux
    if op == OP_SP_SET_SAMPLE_RATE:
        call(e, A["sp_setrate"], [obj, b & 0xFFFF])
        return 0, aux
    if op == OP_SC_SET_SAMPLE_RATE:
        call(e, A["c_setrate"], [obj, b & 0xFFFF])
        return 0, aux
    if op == OP_SP_GET_INFO:
        call(e, A["sp_getinfo"], [obj, w.info])
        return 0, aux
    if op == OP_SC_GET_INFO:
        call(e, A["c_getinfo"], [obj, w.info])
        return 0, aux
    if op == OP_SC_SET_FORMAT_FROM_HEADER:
        call(e, A["c_setfmt"], [obj, w.headers[b]])
        return 0, aux
    if op == OP_SC_SET_TIME:
        return call(e, A["c_settime"], [obj, 1 if b else 0, c & MASK]) & 0xFF, aux
    if op == OP_POKE:
        offset = {0: 0x10, 1: 0x28, 2: 0x20, 3: 0x24, 4: 0x18, 5: 0x1C}[b]
        if b == 1:
            e.write(obj + offset, bytes([c & 0xFF]))
        else:
            ws(e, obj + offset, [c])
        return 0, aux
    raise ValueError(op)


DEFAULT_HEADERS = [(0x1000, 0x5622, 0x10, 0), (100, 0xAC44, 8, 1), (MASK, 0x2B11, 0x10, 0x25)]


def ppc_case(e, case):
    e.heap_in_scratch = True
    w = e.w = World()
    w.nodes, w.kinds, w.blocks, w.bsizes = [0] * SLOTS, [K_NONE] * SLOTS, [0] * 8, [0] * 8
    w.owners = [e.alloc(8, struct.pack(">II", VT["owner"], 1)) for _ in range(2)]
    w.info = e.alloc(0x100, b"\xa5" * 0x100)
    ws(e, w.info + 0xC, [case["info"]])
    w.headers = []
    for size, rate, bits, flags in case["hdrs"]:
        data = struct.pack(">IHBB", size & MASK, rate & 0xFFFF, bits & 0xFF, flags & 0xFF) + b"\xa5" * 0x58
        w.headers.append(e.alloc(0x60, data))
    fixed = {w.owners[0]: 1, w.owners[1]: 2, w.info: 3, w.headers[0]: 4, w.headers[1]: 5, w.headers[2]: 6}
    e.cl = L = CL.PpcLog(fixed, case["q"])
    ws(e, S_FIRST, [0])
    current, low, high, total, level = case["pool"]
    ws(e, S_CURRENT, [current])
    ws(e, S_LOW, [low])
    ws(e, S_HIGH, [high])
    ws(e, S_TOTAL, [total])
    ws(e, S_LEVEL, [level])
    saved = STUBS.activate(e)
    steps = []
    try:
        for op in case["ops"]:
            before = len(L.log)
            try:
                result, aux = ppc_op(e, w, *op)
            except RuntimeError as ex:
                steps.append(dict(crash=str(ex)[:120], op=op))
                break
            for s in range(SLOTS):
                if w.nodes[s] != 0 and not alive(e, w, s):
                    w.nodes[s], w.kinds[s] = 0, K_NONE
            steps.append(dict(result=result, aux=aux, slots=[ppc_slot_state(e, w, s) for s in range(SLOTS)],
                              pool=ppc_pool_state(e), blocks=[ppc_block_state(e, w, i) for i in range(8)], info=e.u32(w.info + 0xC),
                              log=list(L.log[before:])))
        return dict(steps=steps)
    finally:
        CL.Stubs.deactivate(e, saved)


# ---------------------------------------------------------------- the host side

def host_events(lib, before, after):
    log = (CL.TpwCallEvent * 4096).in_dll(lib, "tpw_cl_log")
    out = []
    for i in range(before, min(after, 4096)):
        ev = log[i]
        out.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                    tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
    if after > 4096:
        out.append(("OVERFLOW", (after,), ()))
    return out


def host_case(lib, case):
    lib.tpw_sp_begin.argtypes = []
    lib.tpw_sp_set_header.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_sp_set_info.argtypes = [C.c_uint32]
    lib.tpw_sp_get_info.restype = C.c_uint32
    lib.tpw_sp_pool_set.argtypes = [C.c_int32] * 5
    lib.tpw_sp_op.restype = C.c_int64
    lib.tpw_sp_op.argtypes = [C.c_int32] + [C.c_int64] * 4
    lib.tpw_sp_aux.restype = C.c_int64
    lib.tpw_sp_aux.argtypes = [C.c_int32]
    lib.tpw_sp_slot.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sp_pool.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sp_block.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    CL.host_begin(lib, {}, case["q"])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib.tpw_sp_begin()
    for index, (size, rate, bits, flags) in enumerate(case["hdrs"]):
        lib.tpw_sp_set_header(index, size & MASK, rate & 0xFFFF, bits & 0xFF, flags & 0xFF)
    lib.tpw_sp_set_info(case["info"] & MASK)
    lib.tpw_sp_pool_set(*case["pool"])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    steps = []
    try:
        for op in case["ops"]:
            before = C.c_int32.in_dll(lib, "tpw_cl_n").value
            result = lib.tpw_sp_op(*op)
            aux = [lib.tpw_sp_aux(0), lib.tpw_sp_aux(1)]
            after = C.c_int32.in_dll(lib, "tpw_cl_n").value
            events = host_events(lib, before, after)
            slots = []
            for s in range(SLOTS):
                out = (C.c_int64 * 16)()
                lib.tpw_sp_slot(s, out)
                slots.append([int(x) for x in out[:12]])
            pool = (C.c_int64 * 48)()
            lib.tpw_sp_pool(pool)
            values = []
            for x in pool:
                values.append(int(x))
                if len(values) > 6 and x == -9:
                    break
            blocks = []
            for i in range(8):
                out = (C.c_int64 * 8)()
                lib.tpw_sp_block(i, out)
                blocks.append([int(x) for x in out[:5]])
            steps.append(dict(result=result, aux=aux, slots=slots, pool=values, blocks=blocks, info=lib.tpw_sp_get_info(), log=events))
    finally:
        lib.tpw_sp_end()
    return dict(steps=steps)


# ---------------------------------------------------------------- the scripts

def op(kind, a=0, b=0, c=0, d=0):
    return (kind, a, b, c, d)


def case(ops, q=None, hdrs=None, info=0, pool=(0, 0, 0, 0, 10)):
    return dict(ops=ops, q=q or {}, hdrs=hdrs or DEFAULT_HEADERS, info=info, pool=pool)


PIPE_NAMES = ("GetSampleRate", "GetInfo", "SetInfo", "Restart", "SetDataStart", "Completed", "FindPipe")
SP_NAMES = ("GetDataStart", "SetDataPosition", "GetDataPosition", "GetDataSize", "SetSampleRate", "GetBitRate", "GetChannels")


def impl(kind, name):
    """What a virtual slot does in a node of this kind: fake, own, empty (nothing more is called), pipe (forwards to mNext when
    it is set), sp (calls through GetCodec, without a test), or None (the slot does not exist in the pipe's vtable)."""
    if kind == K_FAKE:
        return "fake"
    if name in SP_NAMES:
        return None if kind == K_PIPE else "sp" if kind == K_SP else "own"
    if name == "Restart":
        return "empty" if kind == K_PIPE else "sp"
    if name == "GetSampleRate":
        return "pipe" if kind == K_PIPE else "sp" if kind == K_SP else "own"
    if name in ("GetInfo", "SetInfo"):
        return "sp" if kind == K_SP else "pipe"
    return "pipe"


class Chain:
    """The chain a script built: kinds[i] is the kind of slot i, nxt[i] the slot behind it (or None). safe(name, version, slot)
    says whether calling the function `name` with the pipe's (version "pipe"), the SP codec's ("sp") or the sample codec's
    ("own") code on that slot is safe: nothing is called through NULL or through a slot the node's class does not have."""

    def __init__(self, kinds, nxt, qcodec=None):
        self.kinds, self.nxt, self.qcodec = kinds, nxt, qcodec

    def virtual(self, name, n):
        how = impl(self.kinds[n], name)
        if how is None:
            return False
        if how in ("fake", "own", "empty"):
            return True
        m = self.nxt.get(n)
        if how == "pipe":
            return m is None or self.virtual(name, m)
        return m is not None and self.virtual(name, m)

    def safe(self, name, version, n):
        if version == "own":
            if name in ("GetInfo", "SetInfo"):
                m = self.nxt.get(n)
                return m is None or self.virtual(name, m)
            return True
        if version == "pipe":
            m = self.nxt.get(n)
            return m is None or self.virtual(name, m)
        m = self.qcodec if self.kinds[n] == K_FAKE else self.nxt.get(n)
        return m is not None and self.virtual(name, m)


# (op, name, version) of the probes: the functions that read the chain
PROBES = [
    (OP_PIPE_GET_SAMPLE_RATE, "GetSampleRate", "pipe"), (OP_PIPE_SET_DATA_START, "SetDataStart", "pipe"),
    (OP_PIPE_GET_INFO, "GetInfo", "pipe"), (OP_PIPE_SET_INFO, "SetInfo", "pipe"), (OP_PIPE_COMPLETED, "Completed", "pipe"),
    (OP_FIND_PIPE, "FindPipe", "pipe"), (OP_PIPE_RESTART, "Restart", "pipe"), (OP_SET_HANDLE, "SetHandle", "pipe"),
    (OP_GET_DATA_CHUNK, "GetDataChunk", "pipe"),
    (OP_SP_RESTART, "Restart", "sp"), (OP_SP_GET_DATA_START, "GetDataStart", "sp"),
    (OP_SP_SET_DATA_POSITION, "SetDataPosition", "sp"), (OP_SP_GET_DATA_POSITION, "GetDataPosition", "sp"),
    (OP_SP_GET_DATA_SIZE, "GetDataSize", "sp"), (OP_SP_SET_SAMPLE_RATE, "SetSampleRate", "sp"),
    (OP_SP_GET_SAMPLE_RATE, "GetSampleRate", "sp"), (OP_SP_GET_BIT_RATE, "GetBitRate", "sp"),
    (OP_SP_GET_CHANNELS, "GetChannels", "sp"), (OP_SP_GET_INFO, "GetInfo", "sp"), (OP_SP_SET_INFO, "SetInfo", "sp"),
    (OP_SP_GET_CODEC, "GetCodec", "sp"),
    (OP_SC_GET_INFO, "GetInfo", "own"), (OP_SC_SET_INFO, "SetInfo", "own"),
]
SC_PLAIN = [OP_SC_GET_DATA_POSITION, OP_SC_GET_DATA_START, OP_SC_GET_DATA_SIZE, OP_SC_GET_SAMPLE_FORMAT, OP_SC_GET_SAMPLE_RATE,
            OP_SC_GET_BIT_RATE, OP_SC_GET_CHANNELS, OP_SC_IS_TIME_COMPLETE, OP_SC_TIMED]
COOKIES = [0, 1, 0x1000, 0x1234, 0x7FFFFFF0, 0x80000000, MASK, 0xFFFF]


def probe_allowed(chain, kind, name, version, slot):
    k = chain.kinds[slot]
    if version == "sp" and k < K_SP:
        return False
    if version == "own" and k < K_CODEC:
        return False
    if name in ("GetCodec", "SetHandle", "GetDataChunk"):
        return True
    return chain.safe(name, version, slot)


def probe(r, chain, slots, versions=("pipe", "sp", "own")):
    """One random read of the chain: (op tuple) or None."""
    for _ in range(8):
        kind, name, version = r.choice(PROBES)
        if version not in versions:
            continue
        slot = r.choice(slots)
        if not probe_allowed(chain, kind, name, version, slot):
            continue
        b = r.choice(COOKIES)
        if kind == OP_FIND_PIPE:
            return op(kind, slot, b if r.random() < 0.5 else r.choice([0, 1, 2, 3]))
        if kind in (OP_SP_SET_SAMPLE_RATE,):
            return op(kind, slot, r.choice([0x5622, 0xAC44, 0x2B11, 0, 1, 0xFFFF, r.getrandbits(16)]))
        if kind == OP_GET_DATA_CHUNK:
            return op(kind, slot, b, r.choice([0, 1, 100, -1 & MASK]), r.randrange(2))
        return op(kind, slot, b, 0, 0)
    return None


def random_kinds(r, n):
    kinds = []
    for i in range(n):
        last = i == n - 1
        pool = [K_PIPE, K_CODEC, K_DEFAULT, K_FAKE] if last else [K_PIPE, K_SP, K_SP, K_CODEC, K_DEFAULT, K_FAKE]
        kinds.append(r.choice(pool))
    return kinds


def random_queues(r):
    q = {}
    for key in ("Rate", "DataStart", "DataPosition", "DataSize", "BitRate", "Channels", "SetTime", "Chunk"):
        if r.random() < 0.7:
            q[key] = [r.choice([0, 1, 0x5622, 0xAC44, 0x2B11, r.getrandbits(32), r.randrange(0, 100)]) for _ in range(r.randrange(1, 4))]
    if r.random() < 0.6:
        q["SetInfoErr"] = [r.choice([0, 0, -1, 1, 5, -7, 0x7FFFFFFF, -0x80000000]) for _ in range(r.randrange(1, 4))]
    return q


def build_chain(r, kinds, hdr=None):
    """The setup of a chain: NEW for each slot, then Attatch from the head."""
    ops = []
    for i, kind in enumerate(kinds):
        ops.append(op(OP_NEW, i, kind, hdr if hdr is not None else r.randrange(3), r.randrange(2)))
    for i in range(len(kinds) - 1):
        ops.append(op(OP_ATTATCH, i, i + 1))
    return ops


def chain_script(r, destructive=None, extra_kinds=0, versions=("pipe", "sp", "own"), steps=10):
    n = r.randrange(1, 5)
    kinds = random_kinds(r, n)
    spare = []
    # spare standalone nodes after the chain (destructive operations use them)
    for _ in range(extra_kinds):
        kinds.append(r.choice([K_PIPE, K_SP, K_CODEC, K_DEFAULT]))
        spare.append(len(kinds) - 1)
    nxt = {i: i + 1 for i in range(n - 1)}
    ops = build_chain(r, kinds[:n])
    for s in spare:
        ops.append(op(OP_NEW, s, kinds[s], r.randrange(3), r.randrange(2)))
    # sp-capable targets for a FakeNode that is asked to forward as an SP codec
    qcodec = None
    q = random_queues(r)
    fake_slots_ = [i for i, k in enumerate(kinds[:n]) if k == K_FAKE]
    candidates = [i for i, k in enumerate(kinds) if k in (K_FAKE, K_CODEC, K_DEFAULT)]
    if fake_slots_ and candidates:
        qcodec = r.choice(candidates)
        q["Codec"] = [qcodec]
    if fake_slots_:
        q["FindPipe"] = [r.choice([-1, -2, 0, 1, 2])]
    chain = Chain(kinds, nxt, qcodec)
    slots = list(range(n))
    info = r.choice([0, 0x4000, 0x4001, MASK, 0xFFFFBFFF, r.getrandbits(32)])
    for _ in range(r.randrange(1, steps)):
        choice = r.random()
        if choice < 0.15:
            slot = r.choice(slots)
            if kinds[slot] >= K_CODEC:
                ops.append(op(OP_POKE, slot, r.choice([1, 2, 3, 4, 5]), r.choice([0, 1, 2, 0x3C, 0xFF, 0x1000, MASK, r.getrandbits(32)])))
            else:
                ops.append(op(OP_POKE, slot, 0, r.choice(COOKIES)))
            continue
        if choice < 0.3:
            slot = r.choice(slots)
            ops.append(op(OP_POKE, slot, 0, r.choice(COOKIES)))
            continue
        p = probe(r, chain, slots, versions)
        if p is not None:
            ops.append(p)
    if destructive:
        ops += destructive(r, kinds, nxt, n, spare)
    return case(ops, q, info=info)


# ---------------------------------------------------------------- the destructive ends of a script

FLAGS = [1, 1, 1, 2, 0, -1 & MASK, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0x80000001]


def pool_tail(r):
    """What may follow a destructive operation: the pool operations and new nodes in empty slots."""
    ops = []
    for _ in range(r.randrange(0, 5)):
        c = r.random()
        if c < 0.25:
            ops.append(op(OP_NEWOP, r.randrange(8), r.choice([0x14, 0x14, 0x14, 0x2C])))
        elif c < 0.4:
            ops.append(op(OP_TOPUP))
        elif c < 0.5:
            ops.append(op(OP_SETLEVEL, r.choice([0, 1, 3, 12, -1])))
        elif c < 0.6:
            ops.append(op(OP_CLEARCACHE))
        else:
            ops.append(op(OP_NEW, r.randrange(SLOTS), r.choice([K_PIPE, K_SP, K_DEFAULT, K_CODEC]), r.randrange(3), r.randrange(2)))
    return ops


def tail_release(r, kinds, nxt, n, spare):
    return [op(OP_RELEASE, r.randrange(n))] + pool_tail(r)


def tail_detatch(r, kinds, nxt, n, spare):
    return [op(OP_DETATCH, r.randrange(n))] + pool_tail(r)


def tail_dtor(r, kinds, nxt, n, spare):
    ops = [op(OP_DTOR, r.randrange(n), r.choice(FLAGS))]
    return ops + pool_tail(r)


def tail_attatch_over(r, kinds, nxt, n, spare):
    if not spare:
        return tail_release(r, kinds, nxt, n, spare)
    return [op(OP_ATTATCH, r.randrange(n), r.choice(spare))] + pool_tail(r)


def tail_attach_replace(r, kinds, nxt, n, spare):
    if not spare:
        return tail_detatch(r, kinds, nxt, n, spare)
    return [op(OP_ATTACH_REPLACE, r.choice(spare), r.randrange(n), r.choice([-1, 0, 1, 2]))] + pool_tail(r)


def tail_any(r, kinds, nxt, n, spare):
    return r.choice([tail_release, tail_detatch, tail_dtor, tail_attatch_over, tail_attach_replace])(r, kinds, nxt, n, spare)


def tail_in_range(r, kinds, nxt, n, spare):
    return r.choice([tail_release, tail_detatch, tail_dtor, tail_attatch_over, tail_attach_replace, lambda *a: []])(r, kinds, nxt, n, spare)


# ---------------------------------------------------------------- the pool

def pool_script(r):
    sizes = [0, 1, 0x13, 0x14, 0x14, 0x14, 0x15, 0x2C, 0x100]
    ops, blocks, live = [], {}, {}
    pool = (0, 0, 0, 0, 10)
    if r.random() < 0.3:
        pool = (r.choice([0, 3, -2, 100]), r.choice([0, 5, -1]), r.choice([0, 7, 100]), r.choice([0, 9, 0x7FFFFFFF]), r.choice([10, 0, 3, -4, 15]))
    for _ in range(r.randrange(3, 18)):
        c = r.random()
        if c < 0.22:
            index, size = r.randrange(8), r.choice(sizes)
            ops.append(op(OP_NEWOP, index, size))
            blocks[index] = size
        elif c < 0.42 and blocks:
            index = r.choice(sorted(blocks))
            size = r.choice([0x14, 0x14, 0x15, 0x2C, 0x100, 0, 1])
            if size == 0x14 and blocks[index] not in (0x14, 0x2C, 0x100):
                size = 0x15
            ops.append(op(OP_DELETEOP, index, size, 1))
            del blocks[index]
        elif c < 0.46:
            ops.append(op(OP_DELETEOP, -1, r.choice([0x14, 0x2C, 0])))
        elif c < 0.58:
            ops.append(op(OP_TOPUP))
        elif c < 0.66:
            ops.append(op(OP_SETLEVEL, r.choice([0, 1, 2, 3, 5, 10, 13, -1, 0x7FFFFFFF if r.random() < 0.01 else 4])))
        elif c < 0.72:
            ops.append(op(OP_CLEARCACHE))
        elif c < 0.9:
            slot = r.randrange(SLOTS)
            ops.append(op(OP_NEW, slot, r.choice([K_PIPE, K_PIPE, K_SP, K_SP, K_DEFAULT, K_CODEC, K_FAKE]), r.randrange(3), r.randrange(2)))
            live[slot] = True
        elif live:
            slot = r.choice(sorted(live))
            ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
            del live[slot]
    return case(ops, random_queues(r), pool=pool)


def pool_edges():
    out = []
    # a take from an empty list, a push, a take, the statics after each
    out.append(case([op(OP_NEWOP, 0, 0x14), op(OP_DELETEOP, 0, 0x14, 1), op(OP_NEWOP, 1, 0x14), op(OP_NEWOP, 2, 0x14),
                     op(OP_DELETEOP, 1, 0x14, 1), op(OP_DELETEOP, 2, 0x14, 1), op(OP_TOPUP), op(OP_CLEARCACHE)]))
    # sizes: 0 counts as 1, anything but 0x14 comes from the runtime and goes back to it
    for size in (0, 1, 0x13, 0x15, 0x2C, 0x100):
        out.append(case([op(OP_NEWOP, 0, size), op(OP_DELETEOP, 0, size, 1), op(OP_NEWOP, 1, 0x14), op(OP_DELETEOP, 1, size if size else 0x15, 1)]))
    out.append(case([op(OP_NEWOP, 0, 0x14), op(OP_NEWOP, 1, 0x14), op(OP_DELETEOP, 0, 0x15, 1), op(OP_DELETEOP, 1, 0x14, 0), op(OP_NEWOP, 2, 0x14)]))
    out.append(case([op(OP_DELETEOP, -1, 0x14), op(OP_DELETEOP, -1, 0x2C), op(OP_DELETEOP, -1, 0)]))
    # TopUp: the level, the counts and the marks
    for level in (0, 1, 3, 10, 12, -1):
        out.append(case([op(OP_SETLEVEL, level), op(OP_TOPUP), op(OP_TOPUP), op(OP_NEWOP, 0, 0x14), op(OP_TOPUP), op(OP_CLEARCACHE), op(OP_TOPUP)]))
    out.append(case([op(OP_SETLEVEL, 2), op(OP_NEWOP, 0, 0x14), op(OP_NEWOP, 1, 0x14), op(OP_NEWOP, 2, 0x14), op(OP_NEWOP, 3, 0x14),
                     op(OP_DELETEOP, 0, 0x14, 1), op(OP_DELETEOP, 1, 0x14, 1), op(OP_DELETEOP, 2, 0x14, 1), op(OP_DELETEOP, 3, 0x14, 1),
                     op(OP_TOPUP), op(OP_NEWOP, 4, 0x14), op(OP_NEWOP, 5, 0x14), op(OP_NEWOP, 6, 0x14), op(OP_TOPUP)]))
    # the marks: low and high from given statics (the low mark only moves down on a take, the high up on a push or TopUp)
    for pool in ((0, 0, 0, 0, 10), (5, 5, 5, 5, 10), (5, 9, 2, 1, 3), (-3, 0, -5, 0, 4), (2, 2, 100, 0x7FFFFFFF, 0), (0, 3, 0, 0, 2), (4, -1, 4, 4, 6)):
        out.append(case([op(OP_TOPUP), op(OP_NEWOP, 0, 0x14), op(OP_NEWOP, 1, 0x14), op(OP_DELETEOP, 0, 0x14, 1), op(OP_TOPUP), op(OP_CLEARCACHE)],
                        pool=pool))
        out.append(case([op(OP_NEWOP, 0, 0x14), op(OP_DELETEOP, 0, 0x14, 1), op(OP_NEWOP, 1, 0x14), op(OP_CLEARCACHE)], pool=pool))
    # nodes through the pool: a pipe and an SP codec go to it, a sample codec does not
    out.append(case([op(OP_NEW, 0, K_PIPE), op(OP_NEW, 1, K_SP), op(OP_NEW, 2, K_CODEC, 0, 0), op(OP_NEW, 3, K_DEFAULT), op(OP_DTOR, 0, 1),
                     op(OP_DTOR, 1, 1), op(OP_DTOR, 2, 1), op(OP_DTOR, 3, 1), op(OP_NEW, 0, K_PIPE), op(OP_NEW, 1, K_SP), op(OP_NEW, 4, K_PIPE),
                     op(OP_TOPUP), op(OP_CLEARCACHE)]))
    return out


# ---------------------------------------------------------------- one node: constructors, destructors, the small functions

def basic_script(r):
    ops = []
    kinds = [r.choice([K_PIPE, K_SP, K_CODEC, K_DEFAULT, K_FAKE]) for _ in range(3)]
    for i, kind in enumerate(kinds):
        ops.append(op(OP_NEW, i, kind, r.randrange(3), r.randrange(2)))
    for _ in range(r.randrange(1, 8)):
        slot = r.randrange(3)
        c = r.random()
        if c < 0.3:
            ops.append(op(OP_SET_HANDLE, slot, r.choice(COOKIES)))
        elif c < 0.55:
            ops.append(op(OP_GET_DATA_CHUNK, slot, r.choice(COOKIES), r.choice([0, 1, 100, MASK]), r.randrange(2)))
        elif c < 0.7:
            ops.append(op(OP_PIPE_RESTART, slot))
        elif c < 0.85:
            ops.append(op(OP_PIPE_GET_SAMPLE_RATE, slot))
        else:
            ops.append(op(OP_FIND_PIPE, slot, r.choice(COOKIES)))
    for slot in r.sample(range(3), r.randrange(1, 4)):
        ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
    return case(ops, random_queues(r))


def basic_edges():
    out = []
    for kind in (K_PIPE, K_SP, K_CODEC, K_DEFAULT, K_FAKE):
        for flag in FLAGS + [-2 & MASK, 0x7FFF0000, 0x00008001]:
            out.append(case([op(OP_NEW, 0, kind, 1, 1), op(OP_SET_HANDLE, 0, 0x1234), op(OP_FIND_PIPE, 0, 0x1234), op(OP_FIND_PIPE, 0, 0x1235),
                             op(OP_DTOR, 0, flag), op(OP_NEWOP, 0, 0x14)]))
        out.append(case([op(OP_NEW, 0, kind, 2, 0), op(OP_GET_DATA_CHUNK, 0, 0x1000, 16, 1), op(OP_GET_DATA_CHUNK, 0, 0, MASK, 0),
                         op(OP_PIPE_RESTART, 0), op(OP_SET_HANDLE, 0, 0), op(OP_SET_HANDLE, 0, MASK), op(OP_PIPE_GET_SAMPLE_RATE, 0)]))
    return out


# ---------------------------------------------------------------- chains

def chain_edges():
    out = []
    # a chain of two real pipes ending in a fake: attach, replace, find, detach, release at each place
    for tail in (K_FAKE, K_PIPE, K_CODEC):
        base = [op(OP_NEW, 0, K_PIPE), op(OP_NEW, 1, K_PIPE), op(OP_NEW, 2, tail, 0, 0), op(OP_ATTATCH, 0, 1), op(OP_ATTATCH, 1, 2),
                op(OP_SET_HANDLE, 0, 10), op(OP_SET_HANDLE, 1, 11), op(OP_SET_HANDLE, 2, 12)]
        for h in (10, 11, 12, 13, 0):
            out.append(case(base + [op(OP_FIND_PIPE, 0, h), op(OP_FIND_PIPE, 1, h), op(OP_FIND_PIPE, 2, h)], {"FindPipe": [-1]}))
        out.append(case(base + [op(OP_FIND_PIPE, 0, 99)], {"FindPipe": [2]}))
        out.append(case(base + [op(OP_FIND_PIPE, 0, 99)], {"FindPipe": [-2]}))
        for slot in (0, 1, 2):
            out.append(case(base + [op(OP_RELEASE, slot), op(OP_TOPUP)]))
            out.append(case(base + [op(OP_DETATCH, slot), op(OP_NEW, 4, K_PIPE)]))
            out.append(case(base + [op(OP_DTOR, slot, 1)]))
        # replace what is behind a node
        out.append(case(base + [op(OP_NEW, 3, K_PIPE), op(OP_ATTATCH, 0, 3), op(OP_NEW, 4, K_PIPE)]))
        out.append(case(base + [op(OP_NEW, 3, K_PIPE), op(OP_ATTATCH, 1, 3)]))
        out.append(case(base + [op(OP_NEW, 3, K_SP), op(OP_ATTATCH, 2, 3)]))
        out.append(case(base + [op(OP_ATTATCH, 0, -1), op(OP_ATTATCH, 2, -1), op(OP_ATTATCH, 1, -1)]))
        out.append(case(base + [op(OP_NEW, 3, K_PIPE), op(OP_ATTACH_REPLACE, 3, 0, 1), op(OP_NEW, 4, K_PIPE)]))
        out.append(case(base + [op(OP_NEW, 3, K_PIPE), op(OP_ATTACH_REPLACE, 3, 2, -1)]))
        out.append(case(base + [op(OP_NEW, 3, K_CODEC, 1, 1), op(OP_ATTACH_REPLACE, 3, 1, 0)]))
    # attaching to a fake records the Attatch slot; a fake in the middle ends the virtual calls
    out.append(case([op(OP_NEW, 0, K_FAKE), op(OP_NEW, 1, K_PIPE), op(OP_ATTACH_REPLACE, 1, 0, -1)]))
    out.append(case([op(OP_NEW, 0, K_FAKE), op(OP_NEW, 1, K_PIPE), op(OP_NEW, 2, K_PIPE), op(OP_ATTATCH, 1, 0), op(OP_ATTATCH, 0, 2),
                     op(OP_PIPE_GET_SAMPLE_RATE, 1), op(OP_RELEASE, 1)]))
    return out


def chain_random(destructive):
    def make(r):
        return chain_script(r, destructive, extra_kinds=r.randrange(0, 3))
    return make


# ---------------------------------------------------------------- the SP codec

def sp_edges():
    out = []
    for codec in (K_FAKE, K_CODEC, K_DEFAULT):
        for name, kind, args in (("Restart", OP_SP_RESTART, ()), ("GetDataStart", OP_SP_GET_DATA_START, ()),
                                 ("SetDataPosition", OP_SP_SET_DATA_POSITION, (0x1234,)), ("GetDataPosition", OP_SP_GET_DATA_POSITION, ()),
                                 ("GetDataSize", OP_SP_GET_DATA_SIZE, ()), ("SetSampleRate", OP_SP_SET_SAMPLE_RATE, (0x5622,)),
                                 ("GetSampleRate", OP_SP_GET_SAMPLE_RATE, ()), ("GetBitRate", OP_SP_GET_BIT_RATE, ()),
                                 ("GetChannels", OP_SP_GET_CHANNELS, ()), ("GetInfo", OP_SP_GET_INFO, ()), ("SetInfo", OP_SP_SET_INFO, ()),
                                 ("GetCodec", OP_SP_GET_CODEC, ())):
            q = {"Rate": [0x5622], "DataStart": [0x1000], "DataPosition": [7], "DataSize": [9], "BitRate": [16], "Channels": [2], "SetInfoErr": [-3]}
            if name == "Restart" and codec != K_FAKE:
                continue
            out.append(case([op(OP_NEW, 0, K_SP), op(OP_NEW, 1, codec, 1, 0), op(OP_ATTATCH, 0, 1), op(kind, 0, *args)], q))
    # a fake that is the SP codec: GetCodec is its own (queue -2 is its mNext, a slot number names another node)
    for target in (1, 2, 3):
        q = {"Codec": [target], "Rate": [0xAC44], "DataSize": [5]}
        script = [op(OP_NEW, 0, K_FAKE), op(OP_NEW, 1, K_FAKE), op(OP_NEW, 2, K_CODEC, 2, 1), op(OP_NEW, 3, K_DEFAULT)]
        for kind, args in ((OP_SP_GET_CODEC, ()), (OP_SP_GET_SAMPLE_RATE, ()), (OP_SP_GET_DATA_SIZE, ()), (OP_SP_SET_DATA_POSITION, (3,)),
                           (OP_SP_GET_INFO, ()), (OP_SP_SET_INFO, ()), (OP_SP_SET_SAMPLE_RATE, (0x2B11,)), (OP_SP_GET_BIT_RATE, ()),
                           (OP_SP_GET_CHANNELS, ()), (OP_SP_GET_DATA_POSITION, ()), (OP_SP_GET_DATA_START, ())):
            script.append(op(kind, 0, *args))
        out.append(case(script, q))
    # SetInfo's error goes through the chain unchanged
    for err in (0, -1, 1, 5, -0x80000000, 0x7FFFFFFF):
        out.append(case([op(OP_NEW, 0, K_SP), op(OP_NEW, 1, K_FAKE), op(OP_ATTATCH, 0, 1), op(OP_SP_SET_INFO, 0), op(OP_PIPE_SET_INFO, 0)],
                        {"SetInfoErr": [err]}))
    return out


def sp_random(r):
    return chain_script(r, tail_in_range, extra_kinds=r.randrange(0, 2), versions=("sp",), steps=8)


# ---------------------------------------------------------------- the sample codec

def header_cases():
    out = []
    for flags in (0, 1, 0x20, 0x21, 0xFF, 0xFE, 0x24, 0x25):
        for bits in (0x10, 8, 0x11, 0, 0xFF, 0x0F):
            for rate in (0x5622, 0xAC44, 0x2B11, 0, 1, 0xFFFF, 0x5623, 0xAC43, 22050 & 0xFFFF, 8000):
                out.append((r_size(flags, bits, rate), rate, bits, flags))
    return out


def r_size(*values):
    return (values[0] * 0x10001 + values[1] * 7 + values[2]) & MASK


def sc_edges():
    out = []
    rnd = random.Random(5)
    combos = header_cases()
    rnd.shuffle(combos)
    for i in range(0, len(combos), 3):
        hdrs = combos[i:i + 3]
        if len(hdrs) < 3:
            hdrs = hdrs + DEFAULT_HEADERS[len(hdrs):]
        out.append(case([op(OP_NEW, 0, K_CODEC, 0, 0), op(OP_NEW, 1, K_CODEC, 1, 1), op(OP_NEW, 2, K_CODEC, 2, 0),
                         op(OP_SC_GET_SAMPLE_FORMAT, 0), op(OP_SC_GET_SAMPLE_RATE, 0), op(OP_SC_GET_BIT_RATE, 0), op(OP_SC_GET_CHANNELS, 0),
                         op(OP_SC_GET_SAMPLE_FORMAT, 1), op(OP_SC_GET_SAMPLE_RATE, 1), op(OP_SC_GET_BIT_RATE, 1), op(OP_SC_GET_CHANNELS, 1),
                         op(OP_SC_GET_SAMPLE_FORMAT, 2), op(OP_SC_GET_SAMPLE_RATE, 2), op(OP_SC_GET_BIT_RATE, 2), op(OP_SC_GET_CHANNELS, 2),
                         op(OP_SC_GET_DATA_SIZE, 0), op(OP_SC_GET_DATA_START, 1), op(OP_SC_GET_DATA_POSITION, 2), op(OP_SC_TIMED, 0),
                         op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 0), op(OP_DTOR, 2, 1)], hdrs=hdrs))
    # every format byte through the getters
    for fmt in range(256):
        out.append(case([op(OP_NEW, 0, K_DEFAULT), op(OP_POKE, 0, 1, fmt), op(OP_SC_GET_SAMPLE_FORMAT, 0), op(OP_SC_GET_SAMPLE_RATE, 0),
                         op(OP_SC_GET_BIT_RATE, 0), op(OP_SC_GET_CHANNELS, 0), op(OP_SP_GET_SAMPLE_RATE, 0) if False else op(OP_SC_TIMED, 0)]))
    # SetSampleRate from every format byte for the three rates and others
    for fmt in (0x00, 0x04, 0x08, 0x10, 0x1C, 0xFF, 0x21, 0x42, 0x62, 0xE3, 0xA5):
        for rate in (0x5622, 0xAC44, 0x2B11, 0, 1, 0x5623, 0xAC43, 0xFFFF, 0x10000 & 0xFFFF):
            out.append(case([op(OP_NEW, 0, K_DEFAULT), op(OP_POKE, 0, 1, fmt), op(OP_SC_SET_SAMPLE_RATE, 0, rate), op(OP_SC_GET_SAMPLE_RATE, 0)]))
    # SetSampleFormatFromHeader on the codec and on a fake (the virtual SetSampleRate is the fake's)
    for h in range(3):
        for kind in (K_DEFAULT, K_CODEC, K_FAKE):
            out.append(case([op(OP_NEW, 0, kind, 0, 0), op(OP_POKE, 0, 1, 0xFF), op(OP_SC_SET_FORMAT_FROM_HEADER, 0, h), op(OP_SC_GET_SAMPLE_FORMAT, 0)]))
    return out


def sc_random(r):
    ops = [op(OP_NEW, 0, K_CODEC, r.randrange(3), r.randrange(2)), op(OP_NEW, 1, K_DEFAULT), op(OP_NEW, 2, K_FAKE),
           op(OP_NEW, 3, K_CODEC, r.randrange(3), r.randrange(2))]
    hdrs = [(r.getrandbits(32), r.choice([0x5622, 0xAC44, 0x2B11, r.getrandbits(16)]), r.choice([0x10, 8, r.getrandbits(8)]),
             r.choice([0, 1, r.getrandbits(8)])) for _ in range(3)]
    for _ in range(r.randrange(2, 14)):
        slot = r.randrange(4)
        c = r.random()
        if c < 0.12:
            ops.append(op(OP_POKE, slot, 1, r.choice([0, 0xFF, r.getrandbits(8)])))
        elif c < 0.2:
            ops.append(op(OP_POKE, slot, r.choice([2, 3, 4, 5]), r.choice([0, 1, MASK, 0x7FFFFFFF, r.getrandbits(32)])))
        elif c < 0.3:
            ops.append(op(OP_SC_SET_SAMPLE_RATE, slot, r.choice([0x5622, 0xAC44, 0x2B11, r.getrandbits(16)])))
        elif c < 0.38:
            ops.append(op(OP_SC_SET_FORMAT_FROM_HEADER, slot, r.randrange(3)))
        elif c < 0.45:
            ops.append(op(OP_SC_SET_DATA_POSITION, slot, r.choice([0, 1, MASK, r.getrandbits(32)])))
        elif c < 0.55:
            ops.append(op(r.choice([OP_SC_SET_INFO, OP_SC_GET_INFO]), slot))
        elif c < 0.7:
            ops.append(op(OP_SC_SET_TIME, slot, r.choice([0, 0, 1]), r.choice([0, 1, 100, 0x10000, 0xFFFFFFFF, r.getrandbits(32)])))
        else:
            ops.append(op(r.choice(SC_PLAIN), slot))
    return case(ops, random_queues(r), hdrs=hdrs, info=r.choice([0, 0x4000, 0xFFFFFFFF, r.getrandbits(32)]))


def info_edges():
    out = []
    # SetInfo on the sample codec: with and without 0x4000, with the chain's error
    for info in (0, 0x4000, 0x3FFF, 0x8000, 0xC000, 0x4001, MASK):
        for err in (0, -1, 3):
            for tail in (K_FAKE, None, K_DEFAULT):
                nodes = [op(OP_NEW, 0, K_CODEC, 1, 1)]
                if tail is not None:
                    nodes += [op(OP_NEW, 1, tail), op(OP_ATTATCH, 0, 1)]
                out.append(case(nodes + [op(OP_POKE, 0, 4, 77), op(OP_SC_SET_INFO, 0), op(OP_SC_SET_DATA_POSITION, 0, 5), op(OP_SC_SET_INFO, 0),
                                         op(OP_SC_GET_INFO, 0)], {"SetInfoErr": [err]}, info=info))
    return out


def time_edges():
    out = []
    clocks = [0, 1, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFF00, MASK]
    for fmt in (0x20 | 1 | 4, 0x40 | 1 | 8, 0x20 | 2 | 8, 0x40 | 2 | 0x10, 0x40 | 2 | 4, 0x20 | 1 | 0x10):
        for nbytes in (0, 1, 3, 4, 1000, 44100, 0x100000, 0xFFFFFFFF, 0x7FFFFFFF, 0x418937):
            for clock in (0, 5000, MASK - 100):
                out.append(case([op(OP_NEW, 0, K_DEFAULT), op(OP_POKE, 0, 1, fmt), op(OP_SC_SET_TIME, 0, 0, nbytes), op(OP_SC_TIMED, 0),
                                 op(OP_SC_IS_TIME_COMPLETE, 0)], {"Clock": [clock]}))
    # stopped: the time is left alone and the clock is not asked
    out.append(case([op(OP_NEW, 0, K_DEFAULT), op(OP_POKE, 0, 2, 1234), op(OP_SC_SET_TIME, 0, 1, 99), op(OP_SC_SET_TIME, 0, 255, 99),
                     op(OP_SC_TIMED, 0)], {"Clock": [7]}))
    # IsTimeComplete at the boundary
    for time in (0, 1, 100, 0x7FFFFFFF, 0x80000000, MASK):
        for clock in sorted(set([0, 1, 99, 100, 101, time, (time + 1) & MASK, (time - 1) & MASK, MASK])):
            out.append(case([op(OP_NEW, 0, K_DEFAULT), op(OP_POKE, 0, 2, time), op(OP_SC_IS_TIME_COMPLETE, 0)], {"Clock": [clock]}))
    # the virtual rate: a fake answers the queue (0 is a division by zero)
    for rate in (0, 1, 0x5622, 0xAC44, 7, MASK, 1000, 999, 1001):
        for nbytes in (0, 1, 1000, 999, 0xFFFF, 0x10000, 0xFFFFFFFF):
            out.append(case([op(OP_NEW, 0, K_FAKE), op(OP_POKE, 0, 1, r_fmt(nbytes)), op(OP_SC_SET_TIME, 0, 0, nbytes), op(OP_SC_TIMED, 0)],
                            {"Rate": [rate], "Clock": [123]}))
    return out


def r_fmt(n):
    return [0x21, 0x41, 0x22, 0x42, 0x00, 0xFF][n % 6]


def time_random(r):
    ops = [op(OP_NEW, 0, K_DEFAULT), op(OP_NEW, 1, K_FAKE), op(OP_NEW, 2, K_CODEC, r.randrange(3), 0)]
    q = {"Clock": [r.choice([0, 100, r.getrandbits(32)]) for _ in range(r.randrange(1, 4))],
         "Rate": [r.choice([0x5622, 0xAC44, 0x2B11, r.randrange(1, 100000), 1, 3])]}
    for _ in range(r.randrange(1, 8)):
        slot = r.randrange(3)
        c = r.random()
        if c < 0.2:
            ops.append(op(OP_POKE, slot, 1, r.getrandbits(8)))
        elif c < 0.3:
            ops.append(op(OP_POKE, slot, 2, r.choice([0, r.getrandbits(32)])))
        elif c < 0.7:
            ops.append(op(OP_SC_SET_TIME, slot, r.choice([0, 0, 0, 1]), r.choice([0, r.getrandbits(32), r.randrange(0, 200000)])))
        elif c < 0.85:
            ops.append(op(OP_SC_IS_TIME_COMPLETE, slot))
        else:
            ops.append(op(OP_SC_TIMED, slot))
    return case(ops, q)


# ---------------------------------------------------------------- the vtables

FP = lambda n: n
VTABLES = {
    PIPE_VT: [0x10000248, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x1000F734, 0x1000118C, 0x10001148, 0x10001204, 0x1000F694,
              0x10000BB8, 0x1000F628, 0x1000123C],
    SP_VT: [0x100005B0, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x1000B4A0, 0x1000B350, 0x10001148, 0x1000AFF4, 0x10011CD4,
            0x10000BB8, 0x1000F628, 0x1000123C, 0x1000B0F4, 0x1000B164, 0x1000B1E8, 0x1000B25C, 0x1000B2CC, 0x1000B3C0, 0x1000B430,
            0x1000866C],
    CODEC_VT: [0x10000E1C, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x10008600, 0x1000D9B8, 0x10001148, 0x1000AFF4, 0x1000D774,
               0x10000BB8, 0x1000F628, 0x1000123C, 0x10008580, 0x1000DC1C, 0x1000853C, 0x100085C0, 0x1000D914, 0x1000DA20, 0x1000DA78,
               0x1000866C, 0x1000DAD0],
}


def vtable_cases():
    return [dict(vt=vt) for vt in VTABLES]


def vtable_ppc(e, c):
    vt = c["vt"]
    slots = [e.u32(e.u32(vt + 8 + 4 * i)) for i in range(len(VTABLES[vt]))]
    # the destructor stores the vtable's own address (the constructors do): the first word of each is the address of the code
    return dict(slots=slots, count=len(slots), after=e.u32(vt + 8 + 4 * len(slots)) != 0)


def vtable_host(lib, c):
    return dict(slots=list(VTABLES[c["vt"]]), count=len(VTABLES[c["vt"]]), after=True)


def stores_ppc(e, c):
    """What the constructors and destructors store at +0 (the vptr), read from a block they ran on."""
    out = {}
    for name, size, ctor, args in (("pipe", 0x14, A["p_ctor"], []), ("sp", 0x14, A["sp_ctor"], []), ("codec0", 0x2C, A["c_ctor0"], [])):
        block = e.alloc(0x40, b"\xa5" * 0x40)
        call(e, ctor, [block] + args)
        out[name] = e.u32(block)
    return out


def stores_host(lib, c):
    return dict(pipe=PIPE_VT, sp=SP_VT, codec0=CODEC_VT)


TESTS = [
    FnTest("pool: operator new 0x1000ee48, operator delete 0x1000ef14, TopUp 0x1000efb8, SetLevel 0x1000f098, ClearCache 0x1000edb8",
           ppc_case, host_case, edge=pool_edges, rnd=pool_script),
    FnTest("pipe: constructor 0x10000344, destructor 0x10000248, SetHandle 0x10000bb8, GetDataChunk 0x10001148, Restart 0x10001204, "
           "SP codec constructor 0x10000644 and destructor 0x100005b0, sample codec destructor 0x10000e1c and constructors 0x1000d694 0x100137a4",
           ppc_case, host_case, edge=basic_edges, rnd=basic_script),
    FnTest("pipe chain: Attatch 0x1000f440, AttachReplace 0x1000f4e4, FindPipe 0x1000f7b0, Detatch 0x1000f834, Release 0x1000f588",
           ppc_case, host_case, edge=chain_edges, rnd=chain_random(tail_any)),
    FnTest("pipe forwards: GetSampleRate 0x1000118c, SetDataStart 0x1000123c, Completed 0x1000f628, SetInfo 0x1000f694, GetInfo 0x1000f734",
           ppc_case, host_case, edge=lambda: [],
           rnd=lambda r: chain_script(r, None, versions=("pipe",), steps=10)),
    FnTest("SP codec: GetCodec 0x1000866c, Restart 0x1000aff4, GetDataStart 0x1000b0f4, SetDataPosition 0x1000b164, GetDataPosition 0x1000b1e8, "
           "GetDataSize 0x1000b25c, SetSampleRate 0x1000b2cc, GetSampleRate 0x1000b350, GetBitRate 0x1000b3c0, GetChannels 0x1000b430, "
           "GetInfo 0x1000b4a0, SetInfo 0x10011cd4", ppc_case, host_case, edge=sp_edges, rnd=sp_random),
    FnTest("sample codec: SetSampleFormatFromHeader 0x1000d848, SetSampleRate 0x1000d914, GetSampleRate 0x1000d9b8, GetBitRate 0x1000da20, "
           "GetChannels 0x1000da78, GetSampleFormat 0x1000d4c8, GetDataPosition 0x1000853c, GetDataStart 0x10008580, GetDataSize 0x100085c0, "
           "SetDataPosition 0x1000dc1c, Timed 0x1000e818", ppc_case, host_case, edge=sc_edges, rnd=sc_random),
    FnTest("sample codec: GetInfo 0x10008600, SetInfo 0x1000d774", ppc_case, host_case, edge=info_edges,
           rnd=lambda r: chain_script(r, None, versions=("own",), steps=8)),
    FnTest("sample codec: SetTime 0x1000dad0, IsTimeComplete 0x1000db98", ppc_case, host_case, edge=time_edges, rnd=time_random),
    FnTest("mixed scripts over all of them", ppc_case, host_case, edge=lambda: [], rnd=chain_random(tail_in_range)),
    FnTest("the vtables 0x1003c35c 0x1003c560 0x1003c21c", vtable_ppc, vtable_host, edge=vtable_cases,
           rnd=lambda r: dict(vt=r.choice(list(VTABLES)))),
    FnTest("the vptr stores of the constructors", stores_ppc, stores_host, edge=lambda: [dict()], rnd=lambda r: dict()),
]
