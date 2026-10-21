"""Unit w80, the sound library's four small codecs, against libtpw_unit_SoundCodecs.dylib (tests/unit_SoundCodecs.cpp), in the
program "sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/game/advisor/AdvisorScoring.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleWAV.cpp, src/sound/TbSoundSampleWAVFile.cpp, src/sound/TimerCodec.cpp  _TbSoundSampleWAV: constructor 0x10000d90, destructor 0x10011338, GetDataChunk 0x10011154, Restart
                         0x100113cc, SetDataStart 0x1001140c, operator new 0x10010ec4, operator delete 0x10010f90, TopUp
                         0x10011034, SetLevel 0x10011114, ClearCache 0x10010e34;
                         _TbSoundSampleWAVFile: constructor 0x10011868, destructor 0x10011b4c, GetDataChunk 0x10011934, Restart
                         0x10011ad8, operator new 0x100115c8, operator delete 0x10011698, TopUp 0x10011740, SetLevel
                         0x10011824, ClearCache 0x10011534;
                         _TbSoundCodecLoop: constructor 0x10000548, destructor 0x1000b060, GetDataChunk 0x1000aee0, operator new
                         0x1000ac50, operator delete 0x1000ad1c, TopUp 0x1000adc0, SetLevel 0x1000aea0, ClearCache 0x1000abc0;
                         TbTimerCodec: constructor 0x10013700, destructor 0x100138dc, GetDataChunk 0x10013858, operator new
                         0x10013484, operator delete 0x1001354c, TopUp 0x100135e8, SetLevel 0x100136c4, ClearCache 0x100133f8;
                         CMemoryCopy::Copy 0x10011294

Each function runs as the original in the emulator and as the host's translation, in scripts of operations on a world of six node
slots (a WAV codec, a WAV file codec, a loop, a timer, or a FakeNode that records every virtual slot), three sample headers with
0x200 bytes of data each, an open file of 0x400 bytes and an output buffer of 0x1000 bytes. After EVERY operation both sides dump
every slot (kind, mNext, mPrevious, mHandle, what the pool links hold, the owner, size, data start as header number and offset,
time, position, format byte, the file and its start), the four pools (the six statics and the chain of idle objects as labels),
the eight raw blocks of the pool operations, the output buffer (a CRC), the result and the whole event log. Every callee outside
the unit is a fake that records the same event on both sides: the runtime's new (the Mac's size and a label; blocks are filled
with 0xA5) and delete, the clock (LbTime_GetClock__Fv), the owner's AddRef and Release, the open file (slots +0x10 Read, +0x18
Seek, +0x20 GetPos of the Mac's CFile) and the FakeNode's slots (the virtual calls the real code makes through the Mac vtable: a
slot number that is wrong shows as another name in the log). The real vtables 0x1003c3f8, 0x1003c4e4, 0x1003c04c and 0x1003c758
are the Mac's own, and every operation on a node goes through its vtable. CMemoryCopy::Copy runs as the original too: the global
copier (0x1003f34c) points at an object whose vtable is the Mac's CStandardCopy one, so CStandardCopy::Copy 0x10014cac and the
runtime's memcpy run behind it.

The host's objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are mapped: the
Mac size of a class is the host size of that class, any other size travels as the Mac size + 0x1000.

Pointers compare as labels (0 NULL, the label of the object, 0xA5A5A5A5 for the fill, -1 for anything else); the data start as the
number of the sample header and the offset in it. A script only calls what is safe on the world it built (the Mac code copies a
negative count and follows a NULL codec: the unit file's ChunkSafe, RestartSafe and LoopTargetSafe say which, on both sides).
"""
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundCodecs"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

# vtables
WAV_VT, FILE_VT, LOOP_VT, TIMER_VT = 0x1003C3F8, 0x1003C4E4, 0x1003C04C, 0x1003C758
COPIER_VT, COPIER_PTR = 0x1003C900, 0x1003F34C   # CStandardCopy's vtable, the global CMemoryCopy::Copy calls through
PIPE_NEW = 0x1000EE48    # TbSoundSamplePipe::operator new (w78): the fake node's allocation
CODEC_CTOR0 = 0x100137A4

# the pools: [total, current, low, high, first] statics, level, link offset, Mac size, then operator new, delete, TopUp,
# SetLevel, ClearCache and the constructor
POOLS = [
    dict(name="WAV", total=0x1003F2B8, cur=0x1003F2BC, low=0x1003F2C0, high=0x1003F2C4, first=0x1003F2C8, level=0x1003C398,
         link=0x2C, size=0x30, new=0x10010EC4, delete=0x10010F90, topup=0x10011034, setlevel=0x10011114, clear=0x10010E34,
         ctor=0x10000D90),
    dict(name="WAVFile", total=0x1003F2DC, cur=0x1003F2E0, low=0x1003F2E4, high=0x1003F2E8, first=0x1003F2EC, level=0x1003C458,
         link=0x30, size=0x3C, new=0x100115C8, delete=0x10011698, topup=0x10011740, setlevel=0x10011824, clear=0x10011534,
         ctor=0x10011868),
    dict(name="Loop", total=0x1003EC04, cur=0x1003EC08, low=0x1003EC0C, high=0x1003EC10, first=0x1003EC14, level=0x1003C010,
         link=0x14, size=0x18, new=0x1000AC50, delete=0x1000AD1C, topup=0x1000ADC0, setlevel=0x1000AEA0, clear=0x1000ABC0,
         ctor=0x10000548),
    dict(name="Timer", total=0x1003F300, cur=0x1003F304, low=0x1003F308, high=0x1003F30C, first=0x1003F310, level=0x1003C6F8,
         link=0x2C, size=0x30, new=0x10013484, delete=0x1001354C, topup=0x100135E8, setlevel=0x100136C4, clear=0x100133F8,
         ctor=0x10013700),
]
P_WAV, P_FILE, P_LOOP, P_TIMER = range(4)
MAC_CLASS_SIZES = {0x30: "WAV/timer", 0x3C: "WAVFile", 0x18: "loop"}
# the imports
IMP_NEW, IMP_DEL, IMP_CLOCK = "__nw__FUl", "__dl__FPv", "LbTime_GetClock__Fv"

# the kinds of node
K_NONE, K_WAV, K_FILE, K_LOOP, K_TIMER, K_FAKE = range(6)
KIND_POOL = {K_WAV: P_WAV, K_FILE: P_FILE, K_LOOP: P_LOOP, K_TIMER: P_TIMER}
SLOTS = 6
HEADERS = 3
HEADER_BYTES = 0x28 + 0x200
BUF_BYTES = 0x1000
FILE_BYTES = 0x400
SENT = -(1 << 63)   # the "no scripted answer" of the queue "Pos"

# the operations (the order of enum Op in the unit file)
OPS = ["NEW", "DTOR", "GET_CHUNK", "RESTART", "SET_DATA_START", "ATTATCH", "NEWOP", "DELETEOP", "TOPUP", "SETLEVEL", "CLEARCACHE",
       "POKE", "FILE_POS", "COPY"]
globals().update({"OP_" + name: index for index, name in enumerate(OPS)})

VT = {}
STUBS = None


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def data_byte(header, index):
    return (index * 13 + header * 29 + 7) & 0xFF


def file_byte(index):
    return (index * 5 + 1) & 0xFF


FILE_DATA = bytes(file_byte(i) for i in range(FILE_BYTES))


# ---------------------------------------------------------------- the PowerPC side

def call(emu, entry, args):
    """A top-level call with the stack below the frame cleared (the host's local buffers start as zeros)."""
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
    emu.write(sp - 0x4000, b"\0" * 0x4000)
    return T.call_ppc(emu, entry, args)[0]


class World:
    """What one case's fakes work on: the node slots, the blocks of the raw pool operations, the fixtures."""


def buf_off(w, address):
    return address - w.buf if w.buf <= address < w.buf + BUF_BYTES else -1


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


# the open file's slots: +0x10 Read, +0x18 Seek, +0x20 GetPos

def f_read(e):
    L, w = e.cl, e.w
    dest, size = e.r(4), e.r(5)
    off = buf_off(w, dest)
    L.rec("File.Read", (size, off))
    mode = L.ret("Read", -2)
    if mode == -1:
        e.set_r(3, MASK)
        return
    wanted = size
    if 0 <= mode < wanted:
        wanted = mode
    left = 0 if w.fpos < 0 else FILE_BYTES - w.fpos
    space = 0 if off < 0 else BUF_BYTES - off
    count = wanted
    if count > left:
        count = left if left > 0 else 0
    if count > space:
        count = space
    if count > 0:
        e.write(dest, FILE_DATA[w.fpos:w.fpos + count])
    w.fpos += count
    e.set_r(3, count)


def f_seek(e):
    L, w = e.cl, e.w
    offset, whence = e.r(4), e.r(5)
    L.rec("File.Seek", (offset, whence))
    if L.ret("Seek", 0) == -1:
        e.set_r(3, MASK)
        return
    if whence == 0:
        target = offset
    elif whence == 1:
        target = w.fpos + s32(offset)
    elif whence == 2:
        target = FILE_BYTES + s32(offset)
    else:
        e.set_r(3, MASK)
        return
    if target < 0:
        e.set_r(3, MASK)
        return
    w.fpos = target
    e.set_r(3, target)


def f_getpos(e):
    L, w = e.cl, e.w
    L.rec("File.GetPos", ())
    scripted = L.ret("Pos", SENT)
    e.set_r(3, T.u32(w.fpos if scripted == SENT else scripted))


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
                elif kind == "h":
                    ints.append(value & 0xFFFF)
                elif kind == "b":
                    ints.append(value & 0xFF)
            L.rec(name, ints)
        return stub

    def with_zero(name, *fields):
        base = rec(name, *fields)

        def stub(e):
            base(e)
            e.set_r(3, 0)
        return stub

    def dtor(e):
        L, this, flag = e.cl, e.r(3), T.s16(e.r(4))
        L.rec("Fake.~", (L.lab(this), flag))
        if flag > 0:
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
            L.forget(this)

    def set_info(e):
        L, this, info = e.cl, e.r(4), e.r(5)
        L.rec("Fake.SetInfo", (L.lab(this), L.lab(info)))
        ws(e, e.r(3), [0])

    def set_handle(e):
        rec("Fake.SetHandle", "u")(e)
        e.set_r(3, e.r(4))

    def chunk(e):
        L, w, this = e.cl, e.w, e.r(3)
        L.rec("Fake.GetDataChunk", (L.lab(this), buf_off(w, e.r(4)), s32(e.r(5)), e.r(7) & 0xFF))
        ws(e, e.r(6), [L.ret("Got", 0)])
        e.set_r(3, T.u32(L.ret("Chunk", 0)))

    def set_time(e):
        rec("Fake.SetTime", "b", "u")(e)
        e.set_r(3, 0)

    def set_data_start(e):
        L, this = e.cl, e.r(3)
        L.rec("Fake.SetDataStart", (L.lab(this), start_word(e.w, e.r(4))))

    return [
        dtor, with_zero("Fake.Attatch", "l"), rec("Fake.Release"), rec("Fake.Detatch"), with_zero("Fake.FindPipe", "u"),
        rec("Fake.GetInfo", "l"), with_zero("Fake.GetSampleRate"), chunk, rec("Fake.Restart"), set_info, set_handle,
        rec("Fake.Completed", "b"), set_data_start, with_zero("Fake.GetDataStart"),
        rec("Fake.SetDataPosition", "u"), with_zero("Fake.GetDataPosition"), with_zero("Fake.GetDataSize"),
        rec("Fake.SetSampleRate", "h"), with_zero("Fake.GetBitRate"), with_zero("Fake.GetChannels"), with_zero("Fake.GetCodec"),
        set_time,
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
    VT["file"] = T.persist_alloc(e, 0x40)
    for slot, fn in ((0x10, f_read), (0x18, f_seek), (0x20, f_getpos)):
        ws(e, VT["file"] + slot, [T.fake_function(e, fn)])
    lib = runner.lib
    lib.tpw_sc_sizes.restype = C.c_int32
    lib.tpw_sc_sizes.argtypes = [C.c_int32]
    lib.tpw_sc_op_count.restype = C.c_int32
    assert lib.tpw_sc_op_count() == len(OPS), "the operations of the unit file and of the test differ"


def pool_chain(e, pool):
    chain, node = [], e.u32(POOLS[pool]["first"])
    while node and len(chain) < 64:
        chain.append(node)
        node = e.u32(node + POOLS[pool]["link"])
    return chain


def alive(e, w, slot):
    """A node exists, has its label and is not in its pool."""
    node = w.nodes[slot]
    if node == 0 or e.cl.lab(node) <= 0:
        return False
    pool = KIND_POOL.get(w.kinds[slot])
    return pool is None or node not in pool_chain(e, pool)


def pw(L, address):
    """A pointer field as the unit file's PW compares it."""
    if address == 0:
        return 0
    label = L.lab(address)
    if label > 0:
        return label
    return FILL if address == FILL else -1


def start_word(w, address):
    """A data start pointer: 0 for NULL, the number of the header and the offset in it, or the 32-bit word."""
    if address == 0:
        return 0
    for index, header in enumerate(w.headers):
        if header <= address < header + HEADER_BYTES:
            return 0x10000 * (index + 1) + address - header
    return address


def link_class(address):
    return 0 if address == 0 else 1 if address == FILL else 2


def ppc_slot_state(e, w, slot):
    out = [0] * 16
    node = w.nodes[slot]
    if node == 0:
        return out
    if not alive(e, w, slot):
        out[0] = 1
        return out
    L = e.cl
    kind = w.kinds[slot]
    out[0], out[1] = 2, kind
    out[2], out[3], out[4], out[5] = pw(L, e.u32(node + 8)), pw(L, e.u32(node + 0xC)), e.u32(node + 0x10), link_class(e.u32(node + 4))
    if kind != K_LOOP:
        out[6], out[7] = pw(L, e.u32(node + 0x14)), e.u32(node + 0x18)
        out[8], out[9], out[10] = start_word(w, e.u32(node + 0x1C)), e.u32(node + 0x20), e.u32(node + 0x24)
        out[11] = e.read(node + 0x28, 1)[0]
    if kind in KIND_POOL:
        out[12] = link_class(e.u32(node + POOLS[KIND_POOL[kind]]["link"]))
    if kind == K_FILE:
        out[13] = link_class(e.u32(node + 0x2C))
        out[14] = pw(L, e.u32(node + 0x34))
        out[15] = e.u32(node + 0x38)
    return out


def ppc_pool_state(e, pool):
    L = e.cl
    p = POOLS[pool]
    chain = pool_chain(e, pool)
    out = [pw(L, e.u32(p["first"])), s32(e.u32(p["cur"])), s32(e.u32(p["low"])), s32(e.u32(p["high"])), s32(e.u32(p["total"])),
           s32(e.u32(p["level"]))]
    out += [pw(L, node) for node in chain[:12]]
    out.append(-9)
    return out


def ppc_block_state(e, w, index):
    out = [0] * 6
    block = w.blocks[index]
    if block == 0:
        return out
    L = e.cl
    if L.lab(block) <= 0:
        out[0] = 1
        return out
    pool = w.bpools[index]
    if block in pool_chain(e, pool):
        out[0] = 3
        return out
    out[0] = 2
    if w.bsizes[index] == POOLS[pool]["size"]:
        out[1], out[2], out[3] = pw(L, e.u32(block + 8)), pw(L, e.u32(block + 0xC)), e.u32(block + 0x10)
        out[4], out[5] = link_class(e.u32(block + 4)), link_class(e.u32(block + POOLS[pool]["link"]))
    return out


def node_address(w, slot):
    return w.nodes[slot] if 0 <= slot < SLOTS else 0


def kind_of(w, address):
    for i in range(SLOTS):
        if address != 0 and w.nodes[i] == address:
            return w.kinds[i]
    return K_NONE


def loop_target_safe(e, w, nxt):
    kind = kind_of(w, nxt)
    if kind in (K_FAKE, K_TIMER):
        return True
    if kind not in (K_WAV, K_FILE):
        return False
    size, position, time = e.u32(nxt + 0x18), e.u32(nxt + 0x24), e.u32(nxt + 0x20)
    if time != 0 or size == 0 or size > 0x1E0 or position > size:
        return False
    if kind == K_FILE:
        return True
    return e.u32(nxt + 0x1C) in [h + 0x28 for h in w.headers]


def chunk_safe(e, w, kind, slot, offset, size):
    """The mirror of ChunkSafe in the unit file."""
    if offset < 0 or offset > 0x800:
        return False
    obj = w.nodes[slot]
    if kind == K_WAV:
        if e.u32(obj + 0x20) != 0:
            return True
        left = s32((e.u32(obj + 0x18) - e.u32(obj + 0x24)) & MASK)
        count = size if size < left else left
        if count < 0 or offset + count > BUF_BYTES:
            return False
        source = e.u32(obj + 0x1C) + e.u32(obj + 0x24)   # no wrap: the host adds to a 64-bit pointer
        for header in w.headers:
            if header <= source and source + count <= header + HEADER_BYTES:
                return True
        return count == 0
    if kind == K_TIMER:
        return size >= 0 and offset + size <= BUF_BYTES
    if kind == K_LOOP:
        if size < 0:
            return True   # the signed loop test fails at once: no round, no call, no write
        if offset + size > BUF_BYTES:
            return False
        return loop_target_safe(e, w, e.u32(obj + 8))
    return True


def restart_safe(e, w, kind, slot):
    if kind not in (K_LOOP, K_TIMER):
        return True
    return kind_of(w, e.u32(w.nodes[slot] + 8)) in (K_WAV, K_FILE, K_FAKE)


def data_pointer(w, value):
    if value == 0:
        return 0
    header = (value >> 16) - 1
    if header < 0 or header >= HEADERS:
        return 0
    return w.headers[header] + 0x28 + (value & 0xFFFF)


def vcall(e, obj, slot, args):
    """A virtual call through the object's vtable (the slot holds a function descriptor)."""
    vt = e.u32(obj)
    return call(e, e.u32(e.u32(vt + slot)), [obj] + args)


def ppc_op(e, w, op, a, b, c, d, x):
    L = e.cl
    aux = [0, 0]
    needs_node = op in (OP_DTOR, OP_GET_CHUNK, OP_RESTART, OP_SET_DATA_START, OP_ATTATCH, OP_POKE)
    kind = K_NONE
    if needs_node:
        if not 0 <= a < SLOTS or not alive(e, w, a):
            return -99, aux
        kind = w.kinds[a]
        if op == OP_GET_CHUNK and not chunk_safe(e, w, kind, a, b, c):
            return -99, aux
        if op == OP_RESTART and not restart_safe(e, w, kind, a):
            return -99, aux
        if op == OP_POKE and b >= 1 and kind == K_LOOP:
            return -99, aux
        if op == OP_POKE and b == 6 and kind != K_FILE:
            return -99, aux
    obj = node_address(w, a)
    if op == OP_NEW:
        if not 0 <= a < SLOTS or not K_WAV <= b <= K_FAKE:
            return -99, aux
        header = w.headers[c if 0 <= c < HEADERS else 0]
        if b == K_WAV:
            block = call(e, POOLS[P_WAV]["new"], [0x30])
            call(e, POOLS[P_WAV]["ctor"], [block, header, w.owners[d & 1]])
        elif b == K_FILE:
            block = call(e, POOLS[P_FILE]["new"], [0x3C])
            call(e, POOLS[P_FILE]["ctor"], [block, w.fileobj, header, x & MASK, w.owners[d & 1]])
        elif b == K_LOOP:
            block = call(e, POOLS[P_LOOP]["new"], [0x18])
            call(e, POOLS[P_LOOP]["ctor"], [block])
        elif b == K_TIMER:
            block = call(e, POOLS[P_TIMER]["new"], [0x30])
            call(e, POOLS[P_TIMER]["ctor"], [block, c & 0xFF, d & MASK])
        else:
            block = call(e, PIPE_NEW, [0x2C])
            call(e, CODEC_CTOR0, [block])
            ws(e, block, [VT["fake"]])
        for i in range(SLOTS):
            if w.nodes[i] == block:
                w.nodes[i] = 0
        w.nodes[a] = block
        w.kinds[a] = b
        return L.lab(block), aux
    if op == OP_DTOR:
        vcall(e, obj, 8, [b & MASK])
        w.nodes[a] = 0
        w.kinds[a] = K_NONE
        return 0, aux
    if op == OP_GET_CHUNK:
        done = e.alloc(8, struct.pack(">II", 0x5555, 0))
        result = s32(vcall(e, obj, 0x24, [w.buf + b, c & MASK, done, 1 if d else 0]))
        aux[0] = s32(e.u32(done))
        return result, aux
    if op == OP_RESTART:
        vcall(e, obj, 0x28, [])
        return 0, aux
    if op == OP_SET_DATA_START:
        vcall(e, obj, 0x38, [data_pointer(w, b)])
        return 0, aux
    if op == OP_ATTATCH:
        if b >= 0 and (b >= SLOTS or not alive(e, w, b)):
            return -99, aux
        return L.lab(vcall(e, obj, 0x0C, [node_address(w, b)])), aux
    if op in (OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL, OP_CLEARCACHE):
        pool = {OP_NEWOP: b, OP_DELETEOP: b, OP_TOPUP: a, OP_SETLEVEL: a, OP_CLEARCACHE: a}[op]
        if not 0 <= pool < 4:
            return -99, aux
        p = POOLS[pool]
        if op == OP_NEWOP:
            if not 0 <= a < 8:
                return -99, aux
            block = call(e, p["new"], [c & MASK])
            w.blocks[a], w.bsizes[a], w.bpools[a] = block, c, pool
            return L.lab(block), aux
        if op == OP_DELETEOP:
            call(e, p["delete"], [0 if a < 0 else w.blocks[a], c & MASK])
            if a >= 0 and d:
                w.blocks[a] = 0
            return 0, aux
        if op == OP_TOPUP:
            call(e, p["topup"], [])
        elif op == OP_SETLEVEL:
            call(e, p["setlevel"], [b & MASK])
        else:
            call(e, p["clear"], [])
        return 0, aux
    if op == OP_POKE:
        if b == 0:
            ws(e, obj + 0x10, [c])
        elif b == 1:
            e.write(obj + 0x28, bytes([c & 0xFF]))
        elif b in (2, 3, 4):
            ws(e, obj + {2: 0x20, 3: 0x24, 4: 0x18}[b], [c])
        elif b == 5:
            ws(e, obj + 0x1C, [data_pointer(w, c)])
        elif b == 6:
            ws(e, obj + 0x38, [c])
        return 0, aux
    if op == OP_FILE_POS:
        w.fpos = a
        return 0, aux
    if op == OP_COPY:
        if not (0 <= a <= 0x800 and 0 <= b <= 0x200 and 0 <= c <= 0x200):
            return -99, aux
        temp = e.alloc(16, b"\xa5" * 16)
        call(e, 0x10011294, [temp, w.buf + a, w.fsrc + b, c])
        return 0, aux
    raise ValueError(op)


DEFAULT_HEADERS = [(0x100, 0x5622, 0x10, 0), (0x40, 0xAC44, 8, 1), (0x1E0, 0x2B11, 0x10, 0x25)]


def ppc_case(e, case):
    e.heap_in_scratch = True
    w = e.w = World()
    w.nodes, w.kinds, w.blocks, w.bsizes, w.bpools = [0] * SLOTS, [K_NONE] * SLOTS, [0] * 8, [0] * 8, [0] * 8
    w.owners = [e.alloc(8, struct.pack(">II", VT["owner"], 1)) for _ in range(2)]
    w.headers = []
    for index, (size, rate, bits, flags) in enumerate(case["hdrs"]):
        data = struct.pack(">IHBB", size & MASK, rate & 0xFFFF, bits & 0xFF, flags & 0xFF) + b"\xa5" * 0x20
        data += bytes(data_byte(index, i) for i in range(0x200))
        w.headers.append(e.alloc(HEADER_BYTES, data))
    w.buf = e.alloc(BUF_BYTES, b"\xa5" * BUF_BYTES)
    w.fileobj = e.alloc(8, struct.pack(">II", VT["file"], 0))
    w.fsrc = e.alloc(FILE_BYTES, FILE_DATA)
    w.fpos = 0
    copier = e.alloc(8, struct.pack(">II", COPIER_VT, 0))
    ws(e, COPIER_PTR, [copier])
    fixed = {w.owners[0]: 1, w.owners[1]: 2, w.headers[0]: 4, w.headers[1]: 5, w.headers[2]: 6, w.fileobj: 7}
    e.cl = L = CL.PpcLog(fixed, case["q"])
    for pool, p in enumerate(POOLS):
        ws(e, p["first"], [0])
        current, low, high, total, level = case["pools"].get(pool, (0, 0, 0, 0, 10))
        ws(e, p["cur"], [current])
        ws(e, p["low"], [low])
        ws(e, p["high"], [high])
        ws(e, p["total"], [total])
        ws(e, p["level"], [level])
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
                              pools=[ppc_pool_state(e, p) for p in range(4)],
                              blocks=[ppc_block_state(e, w, i) for i in range(8)],
                              buffer=zlib.crc32(e.read(w.buf, BUF_BYTES)), log=list(L.log[before:])))
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
    lib.tpw_sc_begin.argtypes = []
    lib.tpw_sc_set_header.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_sc_pool_set.argtypes = [C.c_int32] * 6
    lib.tpw_sc_op.restype = C.c_int64
    lib.tpw_sc_op.argtypes = [C.c_int32] + [C.c_int64] * 5
    lib.tpw_sc_aux.restype = C.c_int64
    lib.tpw_sc_aux.argtypes = [C.c_int32]
    lib.tpw_sc_slot.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sc_pool.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sc_block.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_sc_buffer.argtypes = [C.c_void_p]
    CL.host_begin(lib, {}, case["q"])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib.tpw_sc_begin()
    for index, (size, rate, bits, flags) in enumerate(case["hdrs"]):
        lib.tpw_sc_set_header(index, size & MASK, rate & 0xFFFF, bits & 0xFF, flags & 0xFF)
    for pool in range(4):
        current, low, high, total, level = case["pools"].get(pool, (0, 0, 0, 0, 10))
        lib.tpw_sc_pool_set(pool, current, low, high, total, level)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    steps = []
    buffer = (C.c_uint8 * BUF_BYTES)()
    try:
        for op in case["ops"]:
            before = C.c_int32.in_dll(lib, "tpw_cl_n").value
            result = lib.tpw_sc_op(*op)
            aux = [lib.tpw_sc_aux(0), lib.tpw_sc_aux(1)]
            after = C.c_int32.in_dll(lib, "tpw_cl_n").value
            events = host_events(lib, before, after)
            slots = []
            for s in range(SLOTS):
                out = (C.c_int64 * 16)()
                lib.tpw_sc_slot(s, out)
                slots.append([int(x) for x in out[:16]])
            pools = []
            for p in range(4):
                out = (C.c_int64 * 24)()
                lib.tpw_sc_pool(p, out)
                values = []
                for x in out:
                    values.append(int(x))
                    if len(values) > 6 and x == -9:
                        break
                pools.append(values)
            blocks = []
            for i in range(8):
                out = (C.c_int64 * 8)()
                lib.tpw_sc_block(i, out)
                blocks.append([int(x) for x in out[:6]])
            lib.tpw_sc_buffer(buffer)
            steps.append(dict(result=result, aux=aux, slots=slots, pools=pools, blocks=blocks, buffer=zlib.crc32(bytes(buffer)),
                              log=events))
    finally:
        lib.tpw_sc_end()
    return dict(steps=steps)


# ---------------------------------------------------------------- the scripts

def op(kind, a=0, b=0, c=0, d=0, x=0):
    return (kind, a, b, c, d, x)


def case(ops, q=None, hdrs=None, pools=None):
    return dict(ops=ops, q=q or {}, hdrs=hdrs or DEFAULT_HEADERS, pools=pools or {})


def new_wav(slot, header=0, owner=0):
    return op(OP_NEW, slot, K_WAV, header, owner)


def new_file(slot, header=0, owner=0, start=0):
    return op(OP_NEW, slot, K_FILE, header, owner, start)


def new_loop(slot):
    return op(OP_NEW, slot, K_LOOP)


def new_timer(slot, fmt=0x21, ms=100):
    return op(OP_NEW, slot, K_TIMER, fmt, ms)


def new_fake(slot):
    return op(OP_NEW, slot, K_FAKE)


def chunk(slot, size, offset=0, stopped=0):
    return op(OP_GET_CHUNK, slot, offset, size, stopped)


def start_ptr(header, offset=0):
    """The number DataPointer takes: header (0 to 2) data + offset."""
    return 0x10000 * (header + 1) + offset


FLAGS = [1, 1, 1, 2, 0, -1 & MASK, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0x80000001]
SIZES = [0, 1, 2, 3, 0x10, 0x40, 0x7F, 0x80, 0x100, 0x1DF, 0x1E0, 0x1E1, 0x200, 0x400]


def clocks(r):
    return [r.choice([0, 1, 100, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)]) for _ in range(r.randrange(1, 4))]


# ---- the pools

def pool_literals(pool):
    own = POOLS[pool]["size"]
    return [0, 1, 2, own - 1, own, own, own, own + 1, 0x14, 0x2C, 0x30, 0x3C, 0x18, 0x100]


def pool_script(r):
    pool = r.randrange(4)
    sizes = pool_literals(pool)
    ops, blocks, live = [], {}, {}
    pools = {}
    if r.random() < 0.3:
        pools[pool] = (r.choice([0, 3, -2, 100]), r.choice([0, 5, -1]), r.choice([0, 7, 100]), r.choice([0, 9, 0x7FFFFFFF]),
                       r.choice([10, 0, 3, -4, 15]))
    for _ in range(r.randrange(3, 18)):
        c = r.random()
        if c < 0.22:
            index, size = r.randrange(8), r.choice(sizes)
            ops.append(op(OP_NEWOP, index, pool, size))
            blocks[index] = size
        elif c < 0.42 and blocks:
            index = r.choice(sorted(blocks))
            size = r.choice([POOLS[pool]["size"]] * 3 + [blocks[index], 0, 1, 0x100])
            if size == POOLS[pool]["size"] and blocks[index] < size:
                size = size + 1   # a block shorter than the object must not go on the list (its link would be written past it)
            ops.append(op(OP_DELETEOP, index, pool, size, 1))
            del blocks[index]
        elif c < 0.46:
            ops.append(op(OP_DELETEOP, -1, pool, r.choice([POOLS[pool]["size"], 0x14, 0])))
        elif c < 0.58:
            ops.append(op(OP_TOPUP, pool))
        elif c < 0.66:
            ops.append(op(OP_SETLEVEL, pool, r.choice([0, 1, 2, 3, 5, 10, 13, -1, 4])))
        elif c < 0.72:
            ops.append(op(OP_CLEARCACHE, pool))
        elif c < 0.9:
            slot = r.randrange(SLOTS)
            kind = r.choice([K_WAV, K_FILE, K_LOOP, K_TIMER, K_TIMER, K_WAV, K_FAKE])
            ops.append(op(OP_NEW, slot, kind, r.randrange(3), r.randrange(2), r.randrange(0, 0x100)))
            live[slot] = True
        elif live:
            slot = r.choice(sorted(live))
            ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
            del live[slot]
    return case(ops, {"Clock": clocks(r)}, pools=pools)


def pool_edges():
    out = []
    for pool in range(4):
        own = POOLS[pool]["size"]
        # a take from an empty list, a push, a take, the statics after each
        out.append(case([op(OP_NEWOP, 0, pool, own), op(OP_DELETEOP, 0, pool, own, 1), op(OP_NEWOP, 1, pool, own),
                         op(OP_NEWOP, 2, pool, own), op(OP_DELETEOP, 1, pool, own, 1), op(OP_DELETEOP, 2, pool, own, 1),
                         op(OP_TOPUP, pool), op(OP_CLEARCACHE, pool)]))
        # sizes: 0 counts as 1, anything but the class's own comes from the runtime and goes back to it
        for size in sorted(set([0, 1, own - 1, own + 1, 0x14, 0x2C, 0x30, 0x3C, 0x18, 0x100])):
            if size == own:
                continue
            out.append(case([op(OP_NEWOP, 0, pool, size), op(OP_DELETEOP, 0, pool, size, 1), op(OP_NEWOP, 1, pool, own),
                             op(OP_DELETEOP, 1, pool, size if size else own + 1, 1)]))
        out.append(case([op(OP_NEWOP, 0, pool, own), op(OP_NEWOP, 1, pool, own), op(OP_DELETEOP, 0, pool, own + 1, 1),
                         op(OP_DELETEOP, 1, pool, own, 0), op(OP_NEWOP, 2, pool, own)]))
        out.append(case([op(OP_DELETEOP, -1, pool, own), op(OP_DELETEOP, -1, pool, 0), op(OP_DELETEOP, -1, pool, own + 1)]))
        # TopUp: the level, the counts and the marks
        for level in (0, 1, 3, 10, 12, -1):
            out.append(case([op(OP_SETLEVEL, pool, level), op(OP_TOPUP, pool), op(OP_TOPUP, pool), op(OP_NEWOP, 0, pool, own),
                             op(OP_TOPUP, pool), op(OP_CLEARCACHE, pool), op(OP_TOPUP, pool)]))
        out.append(case([op(OP_SETLEVEL, pool, 2)] + [op(OP_NEWOP, i, pool, own) for i in range(4)]
                        + [op(OP_DELETEOP, i, pool, own, 1) for i in range(4)]
                        + [op(OP_TOPUP, pool), op(OP_NEWOP, 4, pool, own), op(OP_NEWOP, 5, pool, own), op(OP_NEWOP, 6, pool, own),
                           op(OP_TOPUP, pool)]))
        # the marks: low and high from given statics
        for start in ((0, 0, 0, 0, 10), (5, 5, 5, 5, 10), (5, 9, 2, 1, 3), (-3, 0, -5, 0, 4), (2, 2, 100, 0x7FFFFFFF, 0),
                      (0, 3, 0, 0, 2), (4, -1, 4, 4, 6)):
            out.append(case([op(OP_TOPUP, pool), op(OP_NEWOP, 0, pool, own), op(OP_NEWOP, 1, pool, own),
                             op(OP_DELETEOP, 0, pool, own, 1), op(OP_TOPUP, pool), op(OP_CLEARCACHE, pool)], pools={pool: start}))
            out.append(case([op(OP_NEWOP, 0, pool, own), op(OP_DELETEOP, 0, pool, own, 1), op(OP_NEWOP, 1, pool, own),
                             op(OP_CLEARCACHE, pool)], pools={pool: start}))
        # a class's pool does not feed another: the same size asked of the other classes
        for other in range(4):
            if other != pool:
                out.append(case([op(OP_NEWOP, 0, pool, own), op(OP_DELETEOP, 0, pool, own, 1), op(OP_NEWOP, 1, other, own),
                                 op(OP_DELETEOP, 1, other, own, 1), op(OP_NEWOP, 2, pool, own), op(OP_TOPUP, other)]))
    # nodes through the pools: each class goes to its own, the fake does not
    out.append(case([new_wav(0), new_file(1), new_loop(2), new_timer(3), new_fake(4), op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 1),
                     op(OP_DTOR, 2, 1), op(OP_DTOR, 3, 1), op(OP_DTOR, 4, 1), new_wav(0), new_file(1), new_loop(2), new_timer(3),
                     op(OP_TOPUP, 0), op(OP_TOPUP, 1), op(OP_TOPUP, 2), op(OP_TOPUP, 3), op(OP_CLEARCACHE, 0), op(OP_CLEARCACHE, 3)],
                    {"Clock": [5]}))
    return out


# ---- constructors and destructors

def basic_edges():
    out = []
    for kind in (K_WAV, K_FILE, K_LOOP, K_TIMER, K_FAKE):
        for flag in FLAGS + [-2 & MASK, 0x7FFF0000, 0x00008001]:
            out.append(case([op(OP_NEW, 0, kind, 1, 1, 0x28), op(OP_POKE, 0, 0, 0x1234), op(OP_NEW, 1, kind, 2, 0, 7),
                             op(OP_DTOR, 0, flag), op(OP_NEWOP, 0, kind - 1 if kind <= K_TIMER else 0, 0x30), op(OP_DTOR, 1, 1)],
                            {"Clock": [3]}))
    # constructors over every header combination and owner, then the destructors with the owner's Release
    rnd = random.Random(11)
    for flags in (0, 1, 0x25):
        for bits in (0x10, 8):
            for rate in (0x5622, 0xAC44, 0x2B11, 0):
                hdrs = [(rnd.getrandbits(8) * 4, rate, bits, flags)] * 3
                out.append(case([new_wav(0, 0, 0), new_wav(1, 1, 1), new_file(2, 2, 0, 0x10), new_file(3, 0, 1, 0), op(OP_DTOR, 0, 1),
                                 op(OP_DTOR, 1, 0), op(OP_DTOR, 2, 1), op(OP_DTOR, 3, 1)], hdrs=hdrs))
    # the timer's constructor: the format byte, the milliseconds and the clock (SetTime asks the rate, then the clock)
    for fmt in (0x00, 0x21, 0x22, 0x41, 0x42, 0x61, 0x5, 0xFF, 0x0C, 0x14, 0x24, 0x48, 0x50):
        for ms in (0, 1, 100, 1000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFEE00):
            out.append(case([new_timer(0, fmt, ms), op(OP_DTOR, 0, 1)], {"Clock": [1234]}))
    for clock in (0, 1, 0x7FFFFFFF, 0x80000000, MASK - 100, MASK):
        out.append(case([new_timer(0, 0x21, 500), new_timer(1, 0x42, 500), new_timer(2, 0x28, 0)], {"Clock": [clock]}))
    return out


def basic_script(r):
    ops = []
    kinds = [r.choice([K_WAV, K_FILE, K_LOOP, K_TIMER, K_FAKE]) for _ in range(3)]
    for i, kind in enumerate(kinds):
        ops.append(op(OP_NEW, i, kind, r.choice([r.randrange(3), r.getrandbits(8)]), r.choice([r.randrange(2), r.getrandbits(32)]),
                      r.choice([0, 0x28, r.getrandbits(32)])))
    for _ in range(r.randrange(0, 4)):
        ops.append(op(OP_POKE, r.randrange(3), r.choice([0, 1, 2, 3, 4, 5, 6]), r.choice([0, 1, MASK, r.getrandbits(32)])))
    for slot in r.sample(range(3), r.randrange(1, 4)):
        ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
    return case(ops, {"Clock": clocks(r)}, hdrs=random_headers(r))


def random_headers(r):
    return [(r.choice([0, 1, 0x40, 0x1E0, r.randrange(0, 0x1E1)]), r.choice([0x5622, 0xAC44, 0x2B11, r.getrandbits(16)]),
             r.choice([0x10, 8, r.getrandbits(8)]), r.choice([0, 1, r.getrandbits(8)])) for _ in range(3)]


# ---- the WAV codec

def wav_chunk_script(r):
    """A WAV codec (or two) played in pieces: sizes around the data left, both stopped flags, the timed path after the end."""
    hdrs = random_headers(r)
    ops = [new_wav(0, r.randrange(3), r.randrange(2)), new_wav(1, r.randrange(3), r.randrange(2))]
    q = {"Clock": clocks(r), "Rate": [r.choice([0x5622, 0xAC44, 0x2B11, r.randrange(1, 100000)])]}
    for _ in range(r.randrange(2, 14)):
        slot = r.randrange(2)
        c = r.random()
        if c < 0.55:
            ops.append(chunk(slot, r.choice(SIZES + [r.randrange(0, 0x220), -1, -5]), r.choice([0, 0, 1, 0x10, 0x7FF, 0x800]),
                             r.choice([0, 0, 1])))
        elif c < 0.65:
            ops.append(op(OP_RESTART, slot))
        elif c < 0.75:
            ops.append(op(OP_SET_DATA_START, slot, r.choice([0, start_ptr(0), start_ptr(1, 8), start_ptr(2, 0x10), start_ptr(1, 0x30)])))
        elif c < 0.85:
            ops.append(op(OP_POKE, slot, r.choice([2, 3, 4, 5]),
                          r.choice([0, 1, 0x20, 0x100, 0x1E0, r.randrange(0, 0x220), MASK, start_ptr(1, 4)])))
        else:
            ops.append(op(OP_POKE, slot, 1, r.getrandbits(8)))
    ops += [op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 1)]
    return case(ops, q, hdrs=hdrs)


def wav_edges():
    out = []
    # a header of `total` bytes read in pieces of `size`, with the stopped flag both ways, then the timed calls
    for total in (0, 1, 0x40, 0x100, 0x1E0):
        for size in (0, 1, total - 1, total, total + 1, total // 2, 0x200, 0x7F):
            if size < 0:
                continue
            for stopped in (0, 1):
                hdrs = [(total, 0x5622, 0x10, 0)] * 3
                out.append(case([new_wav(0, 0, 0)] + [chunk(0, size, 3, stopped) for _ in range(4)] + [op(OP_RESTART, 0), chunk(0, size, 0, stopped)],
                                {"Clock": [1000, 1000, 1000, 5000, 9000, 20000]}, hdrs=hdrs))
    # the end of the data: SetTime gets the bytes just read + 0x1200 (rate by format byte), and a stopped call answers 1
    for fmt in (0x21, 0x22, 0x41, 0x42, 0x29, 0x11):
        for clock in (0, 100, MASK - 1000):
            out.append(case([new_wav(0), op(OP_POKE, 0, 1, fmt), op(OP_POKE, 0, 3, 0x100), chunk(0, 0x20), chunk(0, 0x20), chunk(0, 0),
                             op(OP_POKE, 0, 2, 0), chunk(0, 0x20, 0, 1), chunk(0, 0x20, 0, 1)], {"Clock": [clock, clock + 5000, clock + 10 ** 9 & MASK]}))
    # a timer that is over / not over (IsTimeComplete boundary) and a timer of 0
    for mtime in (1, 100, 0x7FFFFFFF, 0x80000000, MASK):
        for clock in (0, 99, 100, 101, mtime - 1 & MASK, mtime, mtime + 1 & MASK, MASK):
            out.append(case([new_wav(0), op(OP_POKE, 0, 2, mtime), chunk(0, 0x40, 5), chunk(0, 0, 0, 1)], {"Clock": [clock]}))
    # positions around the size (the compare is unsigned) and sizes around what is left
    for position in (0, 1, 0xFF, 0x100, 0x101):
        for size in (-1, 0, 1, 0xFF, 0x100, 0x101, 0x1000):
            out.append(case([new_wav(0, 0, 0), op(OP_POKE, 0, 3, position), chunk(0, size, 0x10, 0), chunk(0, size, 0x10, 1)],
                            {"Clock": [7]}, hdrs=[(0x100, 0x5622, 0x10, 0)] * 3))
    # the end test is an unsigned compare of the position with the size (sizes and positions with the top bit set; only a count
    # of 0 is safe to copy there)
    for size, position in ((0, MASK), (1, MASK), (0x80000000, 0x7FFFFFFF), (0x7FFFFFFF, 0x7FFFFFFF), (0x80000000, 0x80000000),
                           (0x80000001, 0x80000000), (5, 5), (5, 4), (5, 6)):
        for stopped in (0, 1):
            out.append(case([new_wav(0), op(OP_POKE, 0, 4, size), op(OP_POKE, 0, 3, position), chunk(0, 0, 0, stopped), chunk(0, 0, 0, stopped)],
                            {"Clock": [3]}))
    # the data start moved (SetDataStart): into another header, to NULL; Restart
    out.append(case([new_wav(0, 0), op(OP_SET_DATA_START, 0, start_ptr(1, 4)), chunk(0, 0x20), op(OP_RESTART, 0), chunk(0, 0x10, 0x40),
                     op(OP_SET_DATA_START, 0, 0), chunk(0, 0, 0), op(OP_SET_DATA_START, 0, start_ptr(2)), op(OP_RESTART, 0), chunk(0, 0x30)],
                    {"Clock": [1]}))
    return out


# ---- the WAV file codec

def file_chunk_script(r):
    hdrs = random_headers(r)
    start = r.choice([0, 0, 0x28, 0x100, 0x3F0, r.randrange(0, 0x400)])
    ops = [new_file(0, r.randrange(3), r.randrange(2), start), new_file(1, r.randrange(3), r.randrange(2), r.choice([0, 0x28, start]))]
    q = {"Clock": clocks(r)}
    if r.random() < 0.3:
        q["Read"] = [r.choice([-2, 0, 1, 5, 0x20, -1]) for _ in range(r.randrange(1, 4))]
    if r.random() < 0.25:
        q["Seek"] = [r.choice([0, 0, -1]) for _ in range(r.randrange(1, 3))]
    if r.random() < 0.2:
        q["Pos"] = [r.choice([0, start, start + 5, start + 0x100, start - 4, 0x7FFFFFFF, -1, 0x10000]) for _ in range(r.randrange(1, 5))]
    ops.append(op(OP_FILE_POS, r.choice([start, start, 0, 0x30, 0x200, r.randrange(0, 0x420)])))
    for _ in range(r.randrange(2, 12)):
        slot = r.randrange(2)
        c = r.random()
        if c < 0.55:
            ops.append(chunk(slot, r.choice(SIZES + [r.randrange(0, 0x220), -1, -5]), r.choice([0, 0, 1, 0x10, 0x7FF, 0x800]), r.choice([0, 0, 1])))
        elif c < 0.68:
            ops.append(op(OP_RESTART, slot))
        elif c < 0.78:
            ops.append(op(OP_FILE_POS, r.choice([0, 0x28, 0x100, 0x3FF, 0x400, 0x500, -1, r.randrange(0, 0x420)])))
        elif c < 0.9:
            ops.append(op(OP_POKE, slot, r.choice([2, 3, 4, 6]), r.choice([0, 1, 0x20, 0x100, 0x1E0, MASK, r.randrange(0, 0x400)])))
        else:
            ops.append(op(OP_POKE, slot, 1, r.getrandbits(8)))
    ops += [op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 1)]
    return case(ops, q, hdrs=hdrs)


def file_edges():
    out = []
    for total in (0, 1, 0x40, 0x100):
        for start in (0, 0x28, 0x10):
            for size in (0, 1, total, total + 1, total - 1 if total else 5, 0x7F, 0x300):
                if size < 0:
                    continue
                for stopped in (0, 1):
                    hdrs = [(total, 0x5622, 0x10, 0)] * 3
                    out.append(case([new_file(0, 0, 0, start), op(OP_FILE_POS, start)] + [chunk(0, size, 2, stopped) for _ in range(3)]
                                    + [op(OP_RESTART, 0), chunk(0, size, 0, stopped)], {"Clock": [1000, 1000, 5000, 20000]}, hdrs=hdrs))
    # the position the file reports: before the start, at the end, past it (the compare of the position with the size is signed)
    for pos in (0, 5, 0x10, 0x11, 0x28, 0x2A, 0x100, 0x128, 0x129, 0x130, 0x400, 0x500, 0x7FFFFFFF, -1, -0x80000000):
        for size in (0, 1, 0x20, 0x100, 0x200):
            out.append(case([new_file(0, 0, 0, 0x28), op(OP_FILE_POS, 0), chunk(0, size, 0, 0), chunk(0, size, 0, 1)],
                            {"Pos": [pos, pos, pos], "Clock": [9]}, hdrs=[(0x100, 0x5622, 0x10, 0)] * 3))
            out.append(case([new_file(0, 0, 0, 0x28), op(OP_FILE_POS, 0), chunk(0, size, 0, 0), chunk(0, size, 0, 1)],
                            {"Pos": [pos, 0x28 + 0x100 - 1], "Clock": [9]}, hdrs=[(0x100, 0x5622, 0x10, 0)] * 3))
    # short reads and failed reads, a failed seek
    for read in (0, 1, 7, -1, -2):
        out.append(case([new_file(0, 0, 0, 0x10), op(OP_FILE_POS, 0x10), chunk(0, 0x40), chunk(0, 0x40), chunk(0, 0x40)],
                        {"Read": [read], "Clock": [3]}, hdrs=[(0x80, 0x5622, 0x10, 0)] * 3))
    for seek in (0, -1):
        out.append(case([new_file(0, 0, 0, 0x10), op(OP_FILE_POS, 0x200), op(OP_RESTART, 0), chunk(0, 0x20), op(OP_RESTART, 0)],
                        {"Seek": [seek], "Clock": [3]}))
    for start in (0, 1, 0x28, 0x3FF, 0x7FFFFFFF, MASK, 0x80000000):
        out.append(case([new_file(0, 0, 0, start), op(OP_RESTART, 0), op(OP_POKE, 0, 6, 0x44), op(OP_RESTART, 0), chunk(0, 8)],
                        {"Clock": [3]}))
    # the timed path, and the end of the data with the timer started (SetTime gets `done`, not done + 0x1200)
    for fmt in (0x21, 0x22, 0x41, 0x42):
        for clock in (0, 5000):
            out.append(case([new_file(0, 0, 0, 0x28), op(OP_POKE, 0, 1, fmt), op(OP_FILE_POS, 0x28), chunk(0, 0x300), chunk(0, 0x10),
                             chunk(0, 0x10, 0, 1), op(OP_POKE, 0, 2, 0), chunk(0, 0x300, 0, 1)],
                            {"Clock": [clock, clock + 50, clock + 100000], "Rate": [0x5622]}, hdrs=[(0x100, 0x5622, 0x10, 0)] * 3))
    return out


# ---- the loop

def loop_script(r):
    """A loop over the fake (progress from the queue "Got", answers from "Chunk") or over real codecs."""
    q = {"Clock": [0]}
    ops = []
    flavour = r.choice(["fake", "fake", "wav", "file", "timer"])
    if flavour == "fake":
        ops = [new_loop(0), new_fake(1), op(OP_ATTATCH, 0, 1)]
        q["Got"] = [r.choice([1, 2, 3, 7, 0x10, 0x40, 0x100]) for _ in range(r.randrange(1, 5))]
        q["Chunk"] = [r.choice([0, 1, 1, 2, -1, 0x100]) for _ in range(r.randrange(1, 6))]
        q["Chunk"].append(r.choice([0, 1]))
    elif flavour == "wav":
        ops = [new_loop(0), new_wav(1, r.randrange(3)), op(OP_ATTATCH, 0, 1)]
    elif flavour == "file":
        ops = [new_loop(0), new_file(1, r.randrange(3), 0, 0x28), op(OP_FILE_POS, 0x28), op(OP_ATTATCH, 0, 1)]
        if r.random() < 0.3:
            q["Read"] = [r.choice([-2, 0, 3, 0x20]) for _ in range(r.randrange(1, 3))]
    else:
        ops = [new_loop(0), new_timer(1, r.choice([0x21, 0x42]), r.choice([0, 100, 5000])), op(OP_ATTATCH, 0, 1)]
    for _ in range(r.randrange(1, 6)):
        c = r.random()
        if c < 0.7:
            ops.append(chunk(0, r.choice([0, 1, 5, 0x20, 0x100, 0x1E0, 0x201, 0x300, 0x400, r.randrange(0, 0x400)]), r.choice([0, 0, 0x10, 0x400]),
                             r.choice([0, 1])))
        elif c < 0.85:
            ops.append(op(OP_RESTART, 0))
        else:
            ops.append(chunk(1, r.choice([0, 0x20, 0x100]), 0, 1))
    ops.append(op(OP_DTOR, r.choice([0, 1]), r.choice([1, 1, 0])))
    hdrs = [(r.choice([0x10, 0x20, 0x80, 0x100, 0x1E0]), r.choice([0x5622, 0xAC44]), 0x10, r.choice([0, 1])) for _ in range(3)]
    return case(ops, q, hdrs=hdrs)


def loop_edges():
    out = []
    # the fake answers: the loop restarts the codec only on exactly 1
    for result in (0, 1, 2, -1, 0x100, 0x80000000):
        for got in (1, 4, 0x10, 0x30, 0x40):
            out.append(case([new_loop(0), new_fake(1), op(OP_ATTATCH, 0, 1), chunk(0, 0x40), chunk(0, 0), chunk(0, 1)],
                            {"Chunk": [result], "Got": [got]}))
    out.append(case([new_loop(0), new_fake(1), op(OP_ATTATCH, 0, 1), chunk(0, 0x40, 0, 1)], {"Chunk": [1, 0, 1, 1, 0], "Got": [0x10, 0x8, 0x30]}))
    # a negative size: the signed test `done < size` is false at once, so nothing is asked of the codec behind
    for size in (-1, -0x10, -0x80000000):
        out.append(case([new_loop(0), new_fake(1), op(OP_ATTATCH, 0, 1), chunk(0, size), chunk(0, size, 4, 1)],
                        {"Chunk": [1, 0], "Got": [0x10, 0x8]}))
    # over a WAV codec: the data is played again and again
    for size in (0x10, 0x40, 0x100):
        for total in (1, 3, 0x20, 0x100, 0x1E0):
            out.append(case([new_loop(0), new_wav(1, 0), op(OP_ATTATCH, 0, 1), chunk(0, size), chunk(0, size * 3, 4), op(OP_RESTART, 0),
                             chunk(0, 0x200, 0, 1)], {"Clock": [0]}, hdrs=[(total, 0x5622, 0x10, 0)] * 3))
    # over a WAV file codec
    for total in (1, 0x20, 0x100):
        out.append(case([new_loop(0), new_file(1, 0, 0, 0x28), op(OP_FILE_POS, 0x28), op(OP_ATTATCH, 0, 1), chunk(0, 0x300), chunk(0, 0x10)],
                        {"Clock": [0]}, hdrs=[(total, 0x5622, 0x10, 0)] * 3))
    # over a timer: one round fills the buffer with silence
    for size in (0, 1, 0x100):
        out.append(case([new_loop(0), new_timer(1, 0x21, 100), op(OP_ATTATCH, 0, 1), chunk(0, size, 3), chunk(0, size, 0, 1)], {"Clock": [0]}))
    return out


# ---- the timer

def timer_script(r):
    ops = []
    q = {"Clock": clocks(r), "Rate": [r.choice([0x5622, 0xAC44, 0x2B11, r.randrange(1, 100000)])]}
    ops.append(new_timer(0, r.choice([0x21, 0x22, 0x41, 0x42, r.getrandbits(8)]), r.choice([0, 1, 100, 5000, r.getrandbits(32)])))
    ops.append(new_fake(1))
    ops.append(new_timer(2, r.getrandbits(8), r.randrange(0, 20000)))
    for _ in range(r.randrange(1, 8)):
        slot = r.choice([0, 2])
        c = r.random()
        if c < 0.7:
            ops.append(chunk(slot, r.choice([0, 1, 2, 0x10, 0x100, 0x1200 & 0x7FF, 0x7FF, 0x800, r.randrange(0, 0x800)]), r.choice([0, 0, 1, 0x10, 0x7FF]),
                             r.choice([0, 1])))
        elif c < 0.85:
            ops.append(op(OP_POKE, slot, 2, r.choice([0, 1, r.getrandbits(32)])))
        else:
            ops.append(op(OP_POKE, slot, 1, r.getrandbits(8)))
    ops.append(op(OP_DTOR, r.choice([0, 2]), r.choice([1, 1, 0, 2])))
    return case(ops, q)


def timer_edges():
    out = []
    for size in (0, 1, 2, 0x10, 0x1200 & 0xFFF, 0x7FF, 0x800):
        for mtime in (0, 1, 500, MASK):
            for clock in (0, 499, 500, 501, MASK):
                out.append(case([new_timer(0, 0x21, 100), op(OP_POKE, 0, 2, mtime), chunk(0, size, 0x10, 0), chunk(0, size, 0, 1)],
                                {"Clock": [0, clock, clock]}))
    # offsets: the silence only covers the part asked for
    for offset in (0, 1, 0x100, 0x7FF, 0x800):
        out.append(case([new_timer(0, 0x21, 100), chunk(0, 0x100, offset), chunk(0, 0x20, 0)], {"Clock": [0]}))
    return out


# ---- CMemoryCopy::Copy

def copy_edges():
    out = []
    for count in (0, 1, 2, 3, 4, 7, 8, 0x10, 0x100, 0x200):
        for dest, src in ((0, 0), (1, 3), (0x10, 0x20), (0x7F0, 0x1F0), (0x800, 0x200), (0x801, 0)):
            out.append(case([op(OP_COPY, dest, src, count)]))
    return out


def copy_random(r):
    return case([op(OP_COPY, r.randrange(0, 0x800), r.randrange(0, 0x200), r.randrange(0, 0x200)) for _ in range(r.randrange(1, 5))])


# ---- mixed scripts

def mixed_script(r):
    n = r.randrange(2, 5)
    kinds = [r.choice([K_WAV, K_WAV, K_FILE, K_TIMER, K_FAKE]) for _ in range(n)]
    ops = []
    for i, kind in enumerate(kinds):
        ops.append(op(OP_NEW, i, kind, r.choice([r.randrange(3), 0x10]), r.randrange(2), r.choice([0, 0x28, 0x100])))
    q = {"Clock": clocks(r), "Rate": [r.choice([0x5622, 0xAC44, 0x2B11])], "Chunk": [r.choice([0, 1])], "Got": [1]}
    for _ in range(r.randrange(2, 14)):
        slot = r.randrange(n)
        c = r.random()
        if c < 0.4:
            ops.append(chunk(slot, r.choice(SIZES + [r.randrange(0, 0x220)]), r.choice([0, 0x10, 0x400]), r.randrange(2)))
        elif c < 0.5:
            ops.append(op(OP_RESTART, slot))
        elif c < 0.6:
            ops.append(op(OP_SET_DATA_START, slot, r.choice([0, start_ptr(0), start_ptr(2, 3)])))
        elif c < 0.7:
            ops.append(op(OP_FILE_POS, r.choice([0, 0x28, 0x100, 0x400, r.randrange(0, 0x420)])))
        elif c < 0.85:
            ops.append(op(OP_POKE, slot, r.choice([1, 2, 3, 4, 6]), r.choice([0, 1, 0x20, 0x100, r.randrange(0, 0x220), r.getrandbits(8)])))
        else:
            ops.append(op(r.choice([OP_TOPUP, OP_CLEARCACHE]), r.randrange(4)))
    for slot in r.sample(range(n), r.randrange(0, n + 1)):
        ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
    ops += [op(OP_NEW, r.randrange(SLOTS), r.choice([K_WAV, K_FILE, K_LOOP, K_TIMER]), 0, 0, 0x28) for _ in range(r.randrange(0, 3))]
    return case(ops, q, hdrs=random_headers(r))


# ---------------------------------------------------------------- the vtables

VTABLES = {
    WAV_VT: [0x10011338, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x10008600, 0x1000D9B8, 0x10011154, 0x100113CC, 0x1000D774,
             0x10000BB8, 0x1000F628, 0x1001140C, 0x10008580, 0x1000DC1C, 0x1000853C, 0x100085C0, 0x1000D914, 0x1000DA20, 0x1000DA78,
             0x1000866C, 0x1000DAD0],
    FILE_VT: [0x10011B4C, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x10008600, 0x1000D9B8, 0x10011934, 0x10011AD8, 0x1000D774,
              0x10000BB8, 0x1000F628, 0x1001140C, 0x10008580, 0x1000DC1C, 0x1000853C, 0x100085C0, 0x1000D914, 0x1000DA20, 0x1000DA78,
              0x1000866C, 0x1000DAD0],
    LOOP_VT: [0x1000B060, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x1000B4A0, 0x1000B350, 0x1000AEE0, 0x1000AFF4, 0x10011CD4,
              0x10000BB8, 0x1000F628, 0x1000123C, 0x1000B0F4, 0x1000B164, 0x1000B1E8, 0x1000B25C, 0x1000B2CC, 0x1000B3C0, 0x1000B430,
              0x1000866C],
    TIMER_VT: [0x100138DC, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x10008600, 0x1000D9B8, 0x10013858, 0x1000AFF4, 0x1000D774,
               0x10000BB8, 0x1000F628, 0x1000123C, 0x10008580, 0x1000DC1C, 0x1000853C, 0x100085C0, 0x1000D914, 0x1000DA20, 0x1000DA78,
               0x1000866C, 0x1000DAD0],
}


def vtable_cases():
    return [dict(vt=vt) for vt in VTABLES]


def vtable_ppc(e, c):
    vt = c["vt"]
    slots = [e.u32(e.u32(vt + 8 + 4 * i)) for i in range(len(VTABLES[vt]))]
    return dict(slots=slots, count=len(slots))


def vtable_host(lib, c):
    return dict(slots=list(VTABLES[c["vt"]]), count=len(VTABLES[c["vt"]]))


def stores_ppc(e, c):
    """What each constructor stores at +0 (the vptr), read from a block it ran on."""
    out = {}
    owner = e.alloc(8, struct.pack(">II", VT["owner"], 1))
    header = e.alloc(0x40, b"\0" * 0x40)
    for name, ctor, args in (("wav", POOLS[P_WAV]["ctor"], [header, owner]),
                             ("file", POOLS[P_FILE]["ctor"], [0, header, 0, owner]),
                             ("loop", POOLS[P_LOOP]["ctor"], []), ("timer", POOLS[P_TIMER]["ctor"], [0x21, 5])):
        block = e.alloc(0x60, b"\xa5" * 0x60)
        e.cl = CL.PpcLog({}, {"Clock": [0]})
        saved = STUBS.activate(e)
        try:
            call(e, ctor, [block] + args)
        finally:
            CL.Stubs.deactivate(e, saved)
        out[name] = e.u32(block)
    return out


def stores_host(lib, c):
    return dict(wav=WAV_VT, file=FILE_VT, loop=LOOP_VT, timer=TIMER_VT)


TESTS = [
    FnTest("pools: operator new, operator delete, TopUp, SetLevel and ClearCache of the four classes "
           "(0x10010ec4 0x10010f90 0x10011034 0x10011114 0x10010e34, 0x100115c8 0x10011698 0x10011740 0x10011824 0x10011534, "
           "0x1000ac50 0x1000ad1c 0x1000adc0 0x1000aea0 0x1000abc0, 0x10013484 0x1001354c 0x100135e8 0x100136c4 0x100133f8)",
           ppc_case, host_case, edge=pool_edges, rnd=pool_script),
    FnTest("constructors 0x10000d90 0x10011868 0x10000548 0x10013700 and destructors 0x10011338 0x10011b4c 0x1000b060 0x100138dc",
           ppc_case, host_case, edge=basic_edges, rnd=basic_script),
    FnTest("_TbSoundSampleWAV: GetDataChunk 0x10011154, Restart 0x100113cc, SetDataStart 0x1001140c",
           ppc_case, host_case, edge=wav_edges, rnd=wav_chunk_script),
    FnTest("_TbSoundSampleWAVFile: GetDataChunk 0x10011934, Restart 0x10011ad8", ppc_case, host_case, edge=file_edges,
           rnd=file_chunk_script),
    FnTest("_TbSoundCodecLoop: GetDataChunk 0x1000aee0", ppc_case, host_case, edge=loop_edges, rnd=loop_script),
    FnTest("TbTimerCodec: GetDataChunk 0x10013858, constructor 0x10013700", ppc_case, host_case, edge=timer_edges, rnd=timer_script),
    FnTest("CMemoryCopy::Copy 0x10011294", ppc_case, host_case, edge=copy_edges, rnd=copy_random),
    FnTest("mixed scripts over all of them", ppc_case, host_case, edge=lambda: [], rnd=mixed_script),
    FnTest("the vtables 0x1003c3f8 0x1003c4e4 0x1003c04c 0x1003c758", vtable_ppc, vtable_host, edge=vtable_cases,
           rnd=lambda r: dict(vt=r.choice(list(VTABLES)))),
    FnTest("the vptr stores of the constructors", stores_ppc, stores_host, edge=lambda: [dict()], rnd=lambda r: dict()),
]
