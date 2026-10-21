"""Unit w80b, the sound library's codec factory, bank file wrapper and MPEG sample codecs, against
libtpw_unit_SoundCodecFactory.dylib (tests/unit_SoundCodecFactory.cpp), in the program "sound symbols shared" (the Mac's sound
library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/FileAccess.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleStore.cpp          CCodecFactory: constructor 0x10010b10, destructor 0x10010a88, CreateCodec 0x100007c4, CreateController
                                 0x10000054, GetNextSampleFromBank 0x10000390, CreateCodecModifier 0x10000438;
                                 _CFileAccess: constructors 0x10001cf0 and 0x10001d90, Read 0x10001e30, Seek 0x10001fb0,
                                 SeekToDataStart 0x10002030, destructor 0x100020a8
  src/game/advisor/AdvisorScoring.cpp, src/sound/StreamableMpeg2.cpp, src/sound/TbBank.cpp, src/sound/TbSoundSampleMPEG2.cpp, src/sound/TbSoundSamplePipe.cpp     _TbSoundSampleMPEG2: constructor 0x1000e44c, destructor 0x10007a74, SetInfo 0x1000e510, Release
                                 0x1000e618, GetDataChunk 0x1000e690, Restart 0x1000e850, OpenMpegFile 0x1000e8ec, CloseMpegFile
                                 0x1000eb44, GetMpegBlock 0x1000ebe4, SetNewDataPosition 0x1000ec48, SetDataPosition 0x1000ec8c,
                                 SetDataStart 0x100084f0, operator new 0x1000e1b4, operator delete 0x1000e284, TopUp 0x1000e328,
                                 SetLevel 0x1000e40c, ClearCache 0x1000e120, the cleanup destructor 0x1000ed30;
                                 _TbSoundSampleMPEG2File: constructor 0x1000798c, destructor 0x10008458, OpenMpegFile 0x10007b0c, Read
                                 0x100080f4, Seek 0x10008208, GetMpegBlock 0x1000828c, CloseMpegFile 0x10008304, Restart 0x100083a8,
                                 SetNewDataPosition 0x10008410, operator new 0x100076e4, operator delete 0x100077b8, TopUp
                                 0x10007860, SetLevel 0x10007948, ClearCache 0x1000764c, the cleanup destructor 0x1000881c
  src/sound/Bank.cpp, src/sound/ListSentence.cpp, src/sound/TbBank.cpp, src/sound/TbMMFileBank.cpp, src/sound/TbSoundBankInfo.cpp, src/sound/TbSoundManager.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundSampleInfo.cpp (added by this unit)  Bank::CBank: the bank as a TbRefRelease 0x1001458c, IsStreaming 0x100145cc,
                                 GetSamplePosition 0x10014720

Each function runs as the original in the emulator and as the host's translation, in scripts of operations on a world of six node
slots (the nodes the script makes and the chain heads the factory answers), two factories, two file wrappers, two sample infos, a
channel, two banks with their handles (and a handle without a bank), three sample headers with 0x200 bytes of data each, a bank
file and a default file of 0x400 bytes and an output buffer of 0x1000 bytes. After EVERY operation both sides dump every slot (the
head and up to three pipes behind it: kind, label, chain links, handle, what the pool links hold, owner, size, data start as header
number and offset, time, position, format, flags, length, the decoder with its layer, rate, length and stream, the file and its
start), the five pools (the six statics and the chain of idle objects as labels), the eight raw blocks of the pool operations, the
channel, the infos, the factories, the file wrappers, both file positions, the output buffer and the decoder's two scratch buffers
(a CRC), the result and the whole event log. Every callee outside the unit is a fake that records the same event on both sides:
the runtime's new (the Mac's size and a label; blocks are filled with 0xA5) and delete, the clock, the banks (AddRef, Release,
RetrieveSample through the real Bank::CBank, GetSamplePosition, GetHandle, IsStreaming), the channel's SetSampleHeader,
GetNextHandle and ReleasePipe, the decoder CMpegBase (new, constructor, Open, Read, Reset, Close, the destructor through its
vtable) and the file player, the callback pipes (new and constructors; their behaviour is the SP codec's: the vtable the PowerPC
side gives them is the SP codec's with a destructor that records the delete), the bank's file (slots +0x10 Read, +0x18 Seek, +0x20
GetPos of the Mac's CFile), the default file of a _CFileAccess (the same slots, recorded as Own.*), the semaphore and its lock. The
real vtables of the codecs are the Mac's own, and every operation on a node goes through its vtable.

The host's objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are mapped: the Mac
size of a class is the host size of that class, any other size travels as the Mac size + 0x1000. Pointers compare as labels (0 NULL,
the label of the object, 0xA5A5A5A5 for the fill, -1 for anything else); the data start as the number of the sample header and the
offset in it.
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
SHIM_LIBRARY = "unit_SoundCodecFactory"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

# the vtables
WAV_VT, FILE_VT, LOOP_VT = 0x1003C3F8, 0x1003C4E4, 0x1003C04C
MPEG2_VT, MPEG2F_VT, ACCESS_VT, SPCODEC_VT = 0x1003C2DC, 0x1003BDA8, 0x10035C94, 0x1003C560

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
    dict(name="MPEG2", total=0x1003F24C, cur=0x1003F250, low=0x1003F254, high=0x1003F258, first=0x1003F25C, level=0x1003C27C,
         link=0x3C, size=0x40, new=0x1000E1B4, delete=0x1000E284, topup=0x1000E328, setlevel=0x1000E40C, clear=0x1000E120,
         ctor=0x1000E44C),
    dict(name="MPEG2File", total=0x1003EBD4, cur=0x1003EBD8, low=0x1003EBDC, high=0x1003EBE0, first=0x1003EBE4, level=0x1003BD1C,
         link=0x40, size=0x50, new=0x100076E4, delete=0x100077B8, topup=0x10007860, setlevel=0x10007948, clear=0x1000764C,
         ctor=0x1000798C),
]
P_WAV, P_FILE, P_LOOP, P_MPEG2, P_MPEG2F = range(5)
NPOOLS = 5

# the functions of the unit
F_FACTORY_CTOR, F_FACTORY_DTOR = 0x10010B10, 0x10010A88
F_CREATE_CODEC, F_CREATE_CONTROLLER, F_GET_NEXT, F_MODIFIER = 0x100007C4, 0x10000054, 0x10000390, 0x10000438
F_ACCESS_CTOR0, F_ACCESS_CTOR1, F_ACCESS_READ, F_ACCESS_SEEK, F_ACCESS_SEEK_START, F_ACCESS_DTOR = (
    0x10001CF0, 0x10001D90, 0x10001E30, 0x10001FB0, 0x10002030, 0x100020A8)
F_FILE_READ, F_FILE_SEEK = 0x100080F4, 0x10008208
F_CS_DTOR = [0x1000ED30, 0x1000881C]
F_CBANK_REF, F_CBANK_STREAMING, F_CBANK_POSITION = 0x1001458C, 0x100145CC, 0x10014720
# the callees the PowerPC side replaces (in-binary functions of other units)
F_CHAN_SET_HEADER, F_CHAN_NEXT_HANDLE, F_CHAN_RELEASE_PIPE = 0x10000F98, 0x10000BF8, 0x1000A7E0
F_CB_NEW, F_CB_CTOR, F_CCB_NEW, F_CCB_CTOR = 0x1000CD00, 0x10000C94, 0x1000F178, 0x10000114
F_MPEG_NEW, F_MPEG_CTOR, F_MPEG_OPEN, F_MPEG_CLOSE, F_MPEG_RESET, F_MPEG_READ = (
    0x10002398, 0x10002604, 0x10002D9C, 0x1000301C, 0x10002FA8, 0x1000309C)
F_PLAYER_OPEN, F_UPOOLED_NEW, F_UPOOLED_CTOR = 0x100031E4, 0x10007FFC, 0x10007D1C
SCRATCH_SETINFO, SCRATCH_RESTART = 0x1003EF4C, 0x1003EC4C
MPEG_VPTR = 0x2480   # the decoder's vptr
# the imports
IMP_NEW, IMP_DEL, IMP_CLOCK = "__nw__FUl", "__dl__FPv", "LbTime_GetClock__Fv"

# the kinds of slot
K_NONE, K_MPEG2, K_MPEG2F, K_CHAIN = range(4)
SLOTS, HEADERS, BANKS, CBANKS, INFOS, FACTORIES, ACCESSES = 6, 3, 2, 3, 2, 2, 2
ROW, ROWS = 26, 4
HEADER_BYTES = 0x28 + 0x200
BUF_BYTES = 0x1000
FILE_BYTES = 0x400
SENT = -(1 << 63)   # the "no scripted answer" of the queues "Pos" and "OwnPos"

# the operations (the order of enum Op in the unit file)
OPS = ["NEW", "DTOR", "RELEASE", "GET_CHUNK", "RESTART", "SET_DATA_START", "SET_INFO", "OPEN", "CLOSE", "GET_BLOCK",
       "SET_NEW_POSITION", "SET_POSITION", "FILE_READ", "FILE_SEEK", "NEWOP", "DELETEOP", "TOPUP", "SETLEVEL", "CLEARCACHE",
       "CS_DTOR", "POKE", "FACTORY_NEW", "FACTORY_DTOR", "CREATE_CODEC", "CREATE_CONTROLLER", "GET_NEXT", "MODIFIER",
       "ACCESS_NEW", "ACCESS_READ", "ACCESS_SEEK", "ACCESS_SEEK_START", "ACCESS_DTOR", "INFO_SET", "FILE_POS", "CBANK",
       "CHANNEL_POKE"]
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


def own_byte(index):
    return (index * 7 + 3) & 0xFF


FILE_DATA = bytes(file_byte(i) for i in range(FILE_BYTES))
OWN_DATA = bytes(own_byte(i) for i in range(FILE_BYTES))


# ---------------------------------------------------------------- the PowerPC side

def call(emu, entry, args):
    """A top-level call with the stack below the frame cleared (the host's local buffers start as zeros)."""
    sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
    emu.write(sp - 0x4000, b"\0" * 0x4000)
    return T.call_ppc(emu, entry, args)[0]


class World:
    """What one case's fakes work on."""


def buf_off(w, address):
    """The offset of an address in the output buffer, 0x10000 / 0x20000 plus the offset in a scratch buffer, or -1."""
    if w.buf <= address < w.buf + BUF_BYTES:
        return address - w.buf
    if SCRATCH_SETINFO <= address < SCRATCH_SETINFO + 0x300:
        return 0x10000 + address - SCRATCH_SETINFO
    if SCRATCH_RESTART <= address < SCRATCH_RESTART + 0x300:
        return 0x20000 + address - SCRATCH_RESTART
    return -1


def start_word(w, address):
    """A data pointer: 0 for NULL, the number of the header and the offset in it, or the 32-bit word."""
    if address == 0:
        return 0
    for index, header in enumerate(w.headers):
        if header <= address < header + HEADER_BYTES:
            return 0x10000 * (index + 1) + address - header
    return address


def header_index(w, address):
    return w.headers.index(address) if address in w.headers else -1


def lock_word(e):
    """The critical section word of the factory CreateCodec runs on (the unit file's LockWord), 0 when none does."""
    factory = getattr(getattr(e, "w", None), "lock_factory", 0)
    return e.u32(factory) if factory else 0


def stub_new(e):
    L, size = e.cl, e.r(3)
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    L.rec("new", (size, L.newlab(block), lock_word(e)))
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


def new_block(e, size):
    """What the runtime's new does for the original: a block of 0xA5, the event with the Mac's size and the new label."""
    L = e.cl
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    L.rec("new", (size, L.newlab(block), lock_word(e)))
    return block


# the files: +0x10 Read, +0x18 Seek, +0x20 GetPos of the Mac's CFile; the bank's file (File.*) and the default one (Own.*)

def make_file_slots(prefix, data_name, pos_name, read_q, seek_q, pos_q):
    def f_read(e):
        L, w = e.cl, e.w
        dest, size = e.r(4), e.r(5)
        off = buf_off(w, dest)
        L.rec(prefix + ".Read", (size, off))
        mode = L.ret(read_q, -2)
        if mode == -1:
            e.set_r(3, MASK)
            return
        wanted = size
        if 0 <= mode < wanted:
            wanted = mode
        pos = getattr(w, pos_name)
        left = 0 if pos < 0 else FILE_BYTES - pos
        space = 0 if off < 0 else BUF_BYTES - off
        count = wanted
        if count > left:
            count = left if left > 0 else 0
        if count > space:
            count = space
        if count > 0:
            e.write(dest, getattr(w, data_name)[pos:pos + count])
        setattr(w, pos_name, pos + count)
        e.set_r(3, count)

    def f_seek(e):
        L, w = e.cl, e.w
        offset, whence = e.r(4), e.r(5)
        L.rec(prefix + ".Seek", (offset, whence))
        if L.ret(seek_q, 0) == -1:
            e.set_r(3, MASK)
            return
        pos = getattr(w, pos_name)
        if whence == 0:
            target = offset
        elif whence == 1:
            target = pos + s32(offset)
        elif whence == 2:
            target = FILE_BYTES + s32(offset)
        else:
            e.set_r(3, MASK)
            return
        if target < 0:
            e.set_r(3, MASK)
            return
        setattr(w, pos_name, target)
        e.set_r(3, target)

    def f_getpos(e):
        L, w = e.cl, e.w
        L.rec(prefix + ".GetPos", ())
        scripted = L.ret(pos_q, SENT)
        e.set_r(3, T.u32(getattr(w, pos_name) if scripted == SENT else scripted))

    return f_read, f_seek, f_getpos


# the bank's slots: +8 Release, +0xc AddRef, +0x1c RetrieveSample, +0x28 GetSamplePosition, +0x2c GetHandle, +0x34 IsStreaming

def bank_slots(w_of):
    def release(e):
        L = e.cl
        L.rec("Bank.Release", (L.lab(e.r(3)),))
        e.set_r(3, 0)

    def addref(e):
        L = e.cl
        L.rec("Bank.AddRef", (L.lab(e.r(3)),))
        e.set_r(3, 0)

    def retrieve(e):
        L, w = e.cl, e.w
        out, this, outp, index = e.r(3), e.r(4), e.r(5), e.r(6)
        L.rec("Bank.Retrieve", (L.lab(this), index))
        error = s32(T.u32(L.ret("RetErr", 0)))
        if error >= 0:
            which = L.ret("Hdr", 0)
            ws(e, outp, [w.headers[which if 0 <= which < HEADERS else 0]])
        ws(e, out, [error])

    def position(e):
        L = e.cl
        L.rec("Bank.Position", (L.lab(e.r(3)), e.r(4)))
        e.set_r(3, T.u32(L.ret("Pos", 0x100)))

    def get_handle(e):
        L, w = e.cl, e.w
        L.rec("Bank.GetHandle", (L.lab(e.r(3)),))
        e.set_r(3, w.fileobj)

    def streaming(e):
        L = e.cl
        L.rec("Bank.IsStreaming", (L.lab(e.r(3)),))
        e.set_r(3, 1 if L.ret("Stream", 0) != 0 else 0)

    def noop(e):
        e.set_r(3, 0)

    slots = [noop] * 15
    slots[0], slots[1] = release, addref
    slots[(0x1C - 8) // 4] = retrieve
    slots[(0x28 - 8) // 4] = position
    slots[(0x2C - 8) // 4] = get_handle
    slots[(0x34 - 8) // 4] = streaming
    return slots


def mpeg_fill(e):
    """What the fake Open leaves in the decoder (the Mac fields): the stream, the rate, the length, the layer."""
    L = e.cl
    answer = s32(T.u32(L.ret("Open", 0)))
    if answer != -1:
        this, data = e.r(3), e.r(4)
        ws(e, this + 0x24A8, [data])
        ws(e, this + 0x248C, [T.u32(L.ret("Rate", 0x5622))])
        ws(e, this + 0x24B4, [T.u32(L.ret("Total", 0x400))])
        e.write(this + 0x2494, bytes([L.ret("Layer", 2) & 0xFF]))
    return answer


def build_stubs():
    S = CL.Stubs()
    S.imp(IMP_NEW, stub_new)
    S.imp(IMP_DEL, stub_delete)
    S.imp(IMP_CLOCK, stub_clock)

    # the default file of a _CFileAccess
    def file_copy(e):
        e.cl.rec("Own.copy", (e.cl.lab(e.r(4)),))
        ws(e, e.r(3), [VT["ownfile"], 0])

    S.imp("__ct__Q33bfl10FileSystem5CFileFRCQ33bfl10FileSystem5CFile", file_copy)
    S.imp("__dt__Q33bfl10FileSystem5CFileFv", lambda e: e.cl.rec("Own.dtor", ()))
    S.imp("__ct__12_LbSemaphoreFv", lambda e: e.cl.rec("Sema.ctor", ()))
    S.imp("__dt__12_LbSemaphoreFv", lambda e: e.cl.rec("Sema.dtor", ()))

    def lock_ctor(e):
        sema = e.r(4)
        ws(e, e.r(3), [e.u32(sema), 0, 0])

    def lock_lock(e):
        L, this = e.cl, e.r(3)
        words = struct.unpack(">3I", e.read(this, 12))
        L.rec("SemaLock.Lock", (*words, s32(e.r(4))))
        e.set_r(3, T.u32(L.ret("SemaLock.Lock", 1)))

    def lock_dtor(e):
        e.cl.rec("SemaLock.dtor", struct.unpack(">3I", e.read(e.r(3), 12)))

    S.imp("__ct__11_LbSemaLockFP12_LbSemaphorei", lock_ctor)
    S.imp("Lock__11_LbSemaLockFi", lock_lock)
    S.imp("__dt__11_LbSemaLockFv", lock_dtor)

    # the channel
    def set_header(e):
        e.cl.rec("Chan.SetSampleHeader", (e.cl.lab(e.r(3)), header_index(e.w, e.r(4))))

    def next_handle(e):
        L = e.cl
        L.rec("Chan.GetNextHandle", (L.lab(e.r(3)), lock_word(e)))
        e.set_r(3, T.u32(L.ret("Handle", 0x1234)))

    def release_pipe(e):
        L = e.cl
        L.rec("Chan.ReleasePipe", (L.lab(e.r(3)), e.r(4) & 0xFF, lock_word(e)))
        e.set_r(3, 1 if L.ret("RelRet", 0) != 0 else 0)

    S.internal(F_CHAN_SET_HEADER, set_header)
    S.internal(F_CHAN_NEXT_HANDLE, next_handle)
    S.internal(F_CHAN_RELEASE_PIPE, release_pipe)

    # the callbacks: a pipe with the SP codec's behaviour
    def pipe_init(e):
        this = e.r(3)
        ws(e, this, [VT["cb"]])
        ws(e, this + 8, [0, 0, 0])

    def cb_new(e):
        e.set_r(3, new_block(e, e.r(3)))

    def cb_ctor(e):
        e.cl.rec("Callback.ctor", (e.cl.lab(e.r(3)), e.r(4)))
        pipe_init(e)

    def ccb_ctor(e):
        e.cl.rec("Completed.ctor", (e.cl.lab(e.r(3)), e.r(4), e.r(5)))
        pipe_init(e)

    S.internal(F_CB_NEW, cb_new)
    S.internal(F_CB_CTOR, cb_ctor)
    S.internal(F_CCB_NEW, cb_new)
    S.internal(F_CCB_CTOR, ccb_ctor)

    # the decoder and the file player
    def mpeg_new(e):
        e.set_r(3, new_block(e, e.r(3)))

    def mpeg_ctor(e):
        this = e.r(3)
        e.cl.rec("Mpeg.ctor", (e.cl.lab(this),))
        ws(e, this + MPEG_VPTR, [VT["mpeg"]])

    def mpeg_open(e):
        L, w = e.cl, e.w
        this, data, size = e.r(3), e.r(4), e.r(5)
        L.rec("Mpeg.Open", (L.lab(this), start_word(w, data), s32(size)))
        e.set_r(3, T.u32(mpeg_fill(e)))

    def mpeg_close(e):
        e.cl.rec("Mpeg.Close", (e.cl.lab(e.r(3)),))
        e.set_r(3, 0)

    def mpeg_reset(e):
        e.cl.rec("Mpeg.Reset", (e.cl.lab(e.r(3)),))

    def mpeg_read(e):
        L, w = e.cl, e.w
        this, out, nbytes = e.r(3), e.r(4), s32(e.r(5))
        offset = buf_off(w, out)
        L.rec("Mpeg.Read", (L.lab(this), offset, nbytes))
        room = 0
        if 0 <= offset < 0x10000:
            room = BUF_BYTES - offset
        elif 0x10000 <= offset < 0x20000:
            room = 0x300 - (offset - 0x10000)
        elif offset >= 0x20000:
            room = 0x300 - (offset - 0x20000)
        count = max(nbytes, 0)
        if count > room:
            count = room
        e.write(out, bytes((w.pattern + i) & 0xFF for i in range(count)))
        w.pattern += count
        e.set_r(3, T.u32(L.ret("ReadRet", nbytes)))

    def mpeg_dtor(e):
        L, this, flag = e.cl, e.r(3), T.s16(e.r(4))
        L.rec("Mpeg.dtor", (L.lab(this), flag))
        if flag > 0:
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
            L.forget(this)

    def player_open(e):
        L, w = e.cl, e.w
        this, file, start, size, data = e.r(3), e.r(4), s32(e.r(5)), s32(e.r(6)), e.r(7)
        L.rec("Player.Open", (L.lab(this), L.lab(file), start, size, start_word(w, data)))
        e.set_r(3, T.u32(mpeg_fill_player(e, this, data)))

    def mpeg_fill_player(e, this, data):
        L = e.cl
        answer = s32(T.u32(L.ret("Open", 0)))
        if answer != -1:
            ws(e, this + 0x24A8, [data])
            ws(e, this + 0x248C, [T.u32(L.ret("Rate", 0x5622))])
            ws(e, this + 0x24B4, [T.u32(L.ret("Total", 0x400))])
            e.write(this + 0x2494, bytes([L.ret("Layer", 2) & 0xFF]))
        return answer

    def upooled_ctor(e):
        this = e.r(3)
        e.cl.rec("Mpeg.ctor", (e.cl.lab(this),))
        ws(e, this + MPEG_VPTR, [VT["mpeg"]])
        e.cl.rec("Player.ctor", (e.cl.lab(this),))

    S.internal(F_MPEG_NEW, mpeg_new)
    S.internal(F_MPEG_CTOR, mpeg_ctor)
    S.internal(F_MPEG_OPEN, mpeg_open)
    S.internal(F_MPEG_CLOSE, mpeg_close)
    S.internal(F_MPEG_RESET, mpeg_reset)
    S.internal(F_MPEG_READ, mpeg_read)
    S.internal(F_PLAYER_OPEN, player_open)
    S.internal(F_UPOOLED_NEW, mpeg_new)
    S.internal(F_UPOOLED_CTOR, upooled_ctor)
    S.mpeg_dtor = mpeg_dtor
    return S


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs()
    # the bank's vtable: slots from +8
    words = [0, 0] + [T.fake_function(e, fn) for fn in bank_slots(None)]
    VT["bank"] = T.persist_alloc(e, 4 * len(words), struct.pack(">%dI" % len(words), *words))
    # the bank's file and the default file: +0x10 Read, +0x18 Seek, +0x20 GetPos
    VT["file"] = T.persist_alloc(e, 0x40)
    for slot, fn in zip((0x10, 0x18, 0x20), make_file_slots("File", "fdata", "fpos", "Read", "Seek", "Pos")):
        ws(e, VT["file"] + slot, [T.fake_function(e, fn)])
    VT["ownfile"] = T.persist_alloc(e, 0x40)
    for slot, fn in zip((0x10, 0x18, 0x20), make_file_slots("Own", "odata", "opos", "OwnRead", "OwnSeek", "OwnPos")):
        ws(e, VT["ownfile"] + slot, [T.fake_function(e, fn)])
    # the decoder's vtable: the destructor at +8
    VT["mpeg"] = T.persist_alloc(e, 16, struct.pack(">IIII", 0, 0, T.fake_function(e, STUBS.mpeg_dtor), 0))
    # the callback pipes' vtable: the SP codec's (descriptors copied) with a destructor that records the delete
    def cb_dtor(e2):
        L, this, flag = e2.cl, e2.r(3), T.s16(e2.r(4))
        if flag > 0:
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
            L.forget(this)

    sp = [e.u32(SPCODEC_VT + 8 + 4 * i) for i in range(21)]
    sp[0] = T.fake_function(e, cb_dtor)
    words = [0, 0] + sp
    VT["cb"] = T.persist_alloc(e, 4 * len(words), struct.pack(">%dI" % len(words), *words))
    lib = runner.lib
    lib.tpw_cf_sizes.restype = C.c_int32
    lib.tpw_cf_sizes.argtypes = [C.c_int32]
    lib.tpw_cf_op_count.restype = C.c_int32
    assert lib.tpw_cf_op_count() == len(OPS), "the operations of the unit file and of the test differ"


def pool_chain(e, pool):
    chain, node = [], e.u32(POOLS[pool]["first"])
    while node and len(chain) < 64:
        chain.append(node)
        node = e.u32(node + POOLS[pool]["link"])
    return chain


def pooled_anywhere(e, node):
    return any(node in pool_chain(e, p) for p in range(NPOOLS))


def node_alive(e, node):
    return node != 0 and e.cl.lab(node) > 0 and not pooled_anywhere(e, node)


def alive(e, w, slot):
    return node_alive(e, w.nodes[slot])


def pw(L, address):
    """A pointer field as the unit file's PW compares it."""
    if address == 0:
        return 0
    label = L.lab(address)
    if label > 0:
        return label
    return FILL if address == FILL else -1


def link_class(address):
    return 0 if address == 0 else 1 if address == FILL else 2


def node_type(e, node):
    return {WAV_VT: 1, FILE_VT: 2, MPEG2_VT: 3, MPEG2F_VT: 4, LOOP_VT: 5, VT["cb"]: 6}.get(e.u32(node), 0)


def node_row(e, w, node):
    L = e.cl
    out = [0] * ROW
    t = node_type(e, node)
    out[0], out[1] = t, L.lab(node)
    out[2], out[3], out[4], out[5] = pw(L, e.u32(node + 8)), pw(L, e.u32(node + 0xC)), e.u32(node + 0x10), link_class(e.u32(node + 4))
    if 1 <= t <= 4:
        out[6], out[7] = pw(L, e.u32(node + 0x14)), e.u32(node + 0x18)
        out[8], out[9], out[10] = start_word(w, e.u32(node + 0x1C)), e.u32(node + 0x20), e.u32(node + 0x24)
        out[11] = e.read(node + 0x28, 1)[0]
    if t == 1:
        out[12] = link_class(e.u32(node + 0x2C))
    elif t == 2:
        out[12], out[13] = link_class(e.u32(node + 0x30)), link_class(e.u32(node + 0x2C))
        out[14], out[15] = pw(L, e.u32(node + 0x34)), e.u32(node + 0x38)
    elif t == 5:
        out[12] = link_class(e.u32(node + 0x14))
    if t in (3, 4):
        out[12] = link_class(e.u32(node + 0x3C))
        out[14], out[15] = e.u32(node + 0x2C), e.u32(node + 0x30)
        mpeg = e.u32(node + 0x34)
        out[16] = pw(L, mpeg)
        if mpeg != 0 and L.lab(mpeg) > 0:
            out[17] = e.read(mpeg + 0x2494, 1)[0]
            out[18], out[19] = s32(e.u32(mpeg + 0x248C)), s32(e.u32(mpeg + 0x24B4))
            out[20] = start_word(w, e.u32(mpeg + 0x24A8))
        if t == 4:
            out[13] = link_class(e.u32(node + 0x40))
            out[22], out[23], out[24] = pw(L, e.u32(node + 0x44)), e.u32(node + 0x48), start_word(w, e.u32(node + 0x4C))
    return out


def ppc_slot_state(e, w, slot):
    out = [0] * (2 + ROWS * ROW)
    node = w.nodes[slot]
    if node == 0:
        return out
    if not alive(e, w, slot):
        out[0] = -1
        return out
    out[1] = w.kinds[slot]
    rows = 0
    while node_alive(e, node) and rows < ROWS:
        out[2 + rows * ROW:2 + (rows + 1) * ROW] = node_row(e, w, node)
        rows += 1
        node = e.u32(node + 8)
    out[0] = rows
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


def ppc_info_state(e, w, i):
    a = w.infos[i]
    return [e.u32(a), pw(e.cl, e.u32(a + 4)), e.u32(a + 0x14), e.u32(a + 0x40), e.u32(a + 0x44), e.u32(a + 0x48), e.u32(a + 0x4C),
            e.u32(a + 0x88)]


def ppc_factory_state(e, w, i):
    f = w.factories[i]
    if f == 0 or e.cl.lab(f) <= 0:
        return [0, 0]
    return [1, e.u32(f)]


def ppc_access_state(e, w, i):
    a = w.accesses[i]
    if a == 0 or e.cl.lab(a) <= 0:
        return [0, 0, 0, 0]
    return [1, s32(e.u32(a + 4)), e.u32(a + 8), link_class(e.u32(a + 0x14))]


def kind_of(e, w, address):
    for i in range(SLOTS):
        if address != 0 and w.nodes[i] == address:
            return w.kinds[i]
    return K_NONE


def vcall(e, obj, slot, args):
    """A virtual call through the object's vtable (the slot holds a function descriptor)."""
    vt = e.u32(obj)
    return call(e, e.u32(e.u32(vt + slot)), [obj] + args)


def vcall_error(e, obj, slot, args):
    """A virtual call that answers a TbError through the hidden result buffer."""
    buf = e.alloc(8, b"\xa5" * 8)
    vt = e.u32(obj)
    call(e, e.u32(e.u32(vt + slot)), [buf, obj] + args)
    return s32(e.u32(buf))


def chain_alive(e, node):
    for _ in range(8):
        if node == 0:
            return True
        if not node_alive(e, node):
            return False
        node = e.u32(node + 8)
    return True


def set_info_safe(e, node):
    """The mirror of SetInfoSafe in the unit file."""
    for _ in range(8):
        if not node_alive(e, node):
            return False
        t = node_type(e, node)
        if 1 <= t <= 4:
            return True
        if t not in (5, 6):
            return False
        node = e.u32(node + 8)
    return False


def is_mpeg_type(e, node):
    return e.u32(node) in (MPEG2_VT, MPEG2F_VT)


def has_decoder(e, w, slot):
    node = w.nodes[slot]
    if not is_mpeg_type(e, node):
        return False
    mpeg = e.u32(node + 0x34)
    return mpeg != 0 and e.cl.lab(mpeg) > 0


def chunk_safe(offset, size):
    return 0 <= offset <= 0x800 and size >= 0 and offset + size <= BUF_BYTES


def keep(e, w, slot, pipe):
    if not 0 <= slot < SLOTS or pipe == 0:
        return
    w.nodes[slot] = pipe
    t = node_type(e, pipe)
    w.kinds[slot] = K_MPEG2 if t == 3 else K_MPEG2F if t == 4 else K_CHAIN


def factory_ok(e, w, a):
    return 0 <= a < FACTORIES and w.factories[a] != 0 and e.cl.lab(w.factories[a]) > 0


def access_ok(e, w, a):
    return 0 <= a < ACCESSES and w.accesses[a] != 0 and e.cl.lab(w.accesses[a]) > 0


def owner_address(w, which):
    return w.banks[which & 1]


NEEDS_NODE = {"DTOR", "RELEASE", "GET_CHUNK", "RESTART", "SET_DATA_START", "SET_INFO", "OPEN", "CLOSE", "GET_BLOCK",
              "SET_NEW_POSITION", "SET_POSITION", "FILE_READ", "FILE_SEEK", "POKE"}
MPEG_ONLY = {"GET_CHUNK", "RESTART", "SET_DATA_START", "OPEN", "CLOSE", "GET_BLOCK", "SET_NEW_POSITION", "SET_POSITION",
             "FILE_READ", "FILE_SEEK", "POKE"}


def ppc_op(e, w, op, a, b, c, d, x):
    L = e.cl
    aux = [0, 0]
    name = OPS[op]
    obj = w.nodes[a] if 0 <= a < SLOTS else 0
    kind = K_NONE
    if name in NEEDS_NODE:
        if not 0 <= a < SLOTS or not alive(e, w, a):
            return -99, aux
        kind = w.kinds[a]
        mpeg = kind in (K_MPEG2, K_MPEG2F)
        if name in MPEG_ONLY and not mpeg:
            return -99, aux
        if name in ("FILE_READ", "FILE_SEEK") and kind != K_MPEG2F:
            return -99, aux
        if name == "GET_CHUNK":
            if not chunk_safe(b, c):
                return -99, aux
            if e.u32(obj + 0x20) == 0 and not has_decoder(e, w, a):
                return -99, aux
        if name in ("RESTART", "SET_DATA_START") and not has_decoder(e, w, a):
            return -99, aux
        if name == "GET_BLOCK":
            if not chunk_safe(b, c):
                return -99, aux
            if kind == K_MPEG2 and not has_decoder(e, w, a):
                return -99, aux
        if name == "FILE_READ" and (not 0 <= b <= 0x800 or e.u32(obj + 0x44) == 0):
            return -99, aux
        if name == "FILE_SEEK" and e.u32(obj + 0x44) == 0:
            return -99, aux
        if name == "POKE" and b == 6 and kind != K_MPEG2F:
            return -99, aux
        if name == "POKE" and b == 8 and not has_decoder(e, w, a):
            return -99, aux
        if name == "SET_INFO" and (not 0 <= c < INFOS or not set_info_safe(e, obj)):
            return -99, aux
        if name in ("RELEASE", "SET_INFO") and not chain_alive(e, obj):
            return -99, aux
    if name == "NEW":
        if not 0 <= a < SLOTS or not K_MPEG2 <= b <= K_MPEG2F:
            return -99, aux
        header = w.headers[c if 0 <= c < HEADERS else 0]
        if b == K_MPEG2:
            block = call(e, POOLS[P_MPEG2]["new"], [0x40])
            call(e, POOLS[P_MPEG2]["ctor"], [block, header, owner_address(w, d)])
        else:
            block = call(e, POOLS[P_MPEG2F]["new"], [0x50])
            call(e, POOLS[P_MPEG2F]["ctor"], [block, 0 if d & 4 else w.fileobj, header, x & MASK, owner_address(w, d & 3)])
        for i in range(SLOTS):
            if w.nodes[i] == block:
                w.nodes[i] = 0
        w.nodes[a] = block
        w.kinds[a] = b
        return L.lab(block), aux
    if name == "DTOR":
        vcall(e, obj, 8, [b & MASK])
        w.nodes[a] = 0
        w.kinds[a] = K_NONE
        return 0, aux
    if name == "RELEASE":
        vcall(e, obj, 0x10, [])
        return 0, aux
    if name == "GET_CHUNK":
        done = e.alloc(8, struct.pack(">II", 0x5555, 0))
        result = s32(vcall(e, obj, 0x24, [w.buf + b, c & MASK, done, 1 if d else 0]))
        aux[0] = s32(e.u32(done))
        return result, aux
    if name == "RESTART":
        vcall(e, obj, 0x28, [])
        return 0, aux
    if name == "SET_DATA_START":
        ptr = 0
        if b != 0:
            header = (b >> 16) - 1
            if 0 <= header < HEADERS:
                ptr = w.headers[header] + (b & 0xFFFF)
        vcall(e, obj, 0x38, [ptr])
        return 0, aux
    if name == "SET_INFO":
        return vcall_error(e, obj, 0x2C, [w.infos[c]]), aux
    if name == "OPEN":
        return vcall_error(e, obj, 0x64, []), aux
    if name == "CLOSE":
        vcall(e, obj, 0x68, [])
        return 0, aux
    if name == "GET_BLOCK":
        return s32(vcall(e, obj, 0x6C, [w.buf + b, c & MASK])), aux
    if name == "SET_NEW_POSITION":
        vcall(e, obj, 0x60, [])
        return 0, aux
    if name == "SET_POSITION":
        vcall(e, obj, 0x40, [b & MASK])
        return 0, aux
    if name == "FILE_READ":
        return s32(call(e, F_FILE_READ, [obj, w.buf + b, c & MASK])), aux
    if name == "FILE_SEEK":
        return s32(call(e, F_FILE_SEEK, [obj, b & MASK])), aux
    if name in ("NEWOP", "DELETEOP", "TOPUP", "SETLEVEL", "CLEARCACHE"):
        pool = {"NEWOP": b, "DELETEOP": b, "TOPUP": a, "SETLEVEL": a, "CLEARCACHE": a}[name]
        if not 0 <= pool < NPOOLS:
            return -99, aux
        p = POOLS[pool]
        if name == "NEWOP":
            if not 0 <= a < 8:
                return -99, aux
            block = call(e, p["new"], [c & MASK])
            w.blocks[a], w.bsizes[a], w.bpools[a] = block, c, pool
            return L.lab(block), aux
        if name == "DELETEOP":
            call(e, p["delete"], [0 if a < 0 else w.blocks[a], c & MASK])
            if a >= 0 and d:
                w.blocks[a] = 0
            return 0, aux
        if name == "TOPUP":
            call(e, p["topup"], [])
        elif name == "SETLEVEL":
            call(e, p["setlevel"], [b & MASK])
        else:
            call(e, p["clear"], [])
        return 0, aux
    if name == "CS_DTOR":
        if not 0 <= a <= 1:
            return -99, aux
        block = new_block(e, 1)
        call(e, F_CS_DTOR[a], [block, b & MASK])
        return 0, aux
    if name == "POKE":
        if b == 0:
            ws(e, obj + 0x10, [c])
        elif b == 1:
            e.write(obj + 0x28, bytes([c & 0xFF]))
        elif b in (2, 3, 4, 5, 7):
            ws(e, obj + {2: 0x20, 3: 0x24, 4: 0x18, 5: 0x30, 7: 0x2C}[b], [c])
        elif b == 6:
            ws(e, obj + 0x48, [c])
        elif b == 8:
            e.write(e.u32(obj + 0x34) + 0x2494, bytes([c & 0xFF]))
        return 0, aux
    if name == "FACTORY_NEW":
        if not 0 <= a < FACTORIES:
            return -99, aux
        block = new_block(e, 4)
        call(e, F_FACTORY_CTOR, [block])
        w.factories[a] = block
        return L.lab(block), aux
    if name == "FACTORY_DTOR":
        if not factory_ok(e, w, a):
            return -99, aux
        call(e, F_FACTORY_DTOR, [w.factories[a], b & MASK])
        return 0, aux
    if name == "CREATE_CODEC":
        if not factory_ok(e, w, a) or not 0 <= c < INFOS or not 0 <= d < SLOTS:
            return -99, aux
        info = w.infos[c]
        if b == 0 and not (e.u32(info) == 0 or e.u32(info + 4) == 0):
            return -99, aux
        w.lock_factory = w.factories[a]
        pipe = call(e, F_CREATE_CODEC, [w.factories[a], w.chan if b == 1 else 0, info])
        w.lock_factory = 0
        keep(e, w, d, pipe)
        return L.lab(pipe), aux
    if name == "CREATE_CONTROLLER":
        if not factory_ok(e, w, a) or not 0 <= b < INFOS or not 0 <= c < SLOTS:
            return -99, aux
        pipe = call(e, F_CREATE_CONTROLLER, [w.factories[a], w.infos[b]])
        keep(e, w, c, pipe)
        return L.lab(pipe), aux
    if name == "GET_NEXT":
        if not factory_ok(e, w, a) or not 0 <= b < INFOS:
            return -99, aux
        out = e.alloc(8, b"\0" * 8)
        result = s32(call(e, F_GET_NEXT, [w.factories[a], out, w.infos[b]]))
        header = e.u32(out)
        aux[0] = -1 if header == 0 else header_index(w, header) if header in w.headers else -2
        return result, aux
    if name == "MODIFIER":
        if not factory_ok(e, w, a) or not 0 <= b < INFOS or not 0 <= d < SLOTS or not 0 <= x < SLOTS or not alive(e, w, d):
            return -99, aux
        pipe = call(e, F_MODIFIER, [w.factories[a], w.infos[b], w.headers[c if 0 <= c < HEADERS else 0], w.nodes[d]])
        keep(e, w, x, pipe)
        return L.lab(pipe), aux
    if name == "ACCESS_NEW":
        if not 0 <= a < ACCESSES or not 0 <= b <= 1:
            return -99, aux
        block = new_block(e, 0x18)
        if b == 0:
            call(e, F_ACCESS_CTOR0, [block, w.fileobj])
        else:
            call(e, F_ACCESS_CTOR1, [block, w.fileobj, c & MASK])
        w.accesses[a] = block
        return L.lab(block), aux
    if name == "ACCESS_READ":
        if not access_ok(e, w, a) or not 0 <= b <= 0x800:
            return -99, aux
        return s32(call(e, F_ACCESS_READ, [w.accesses[a], w.buf + b, c & MASK])), aux
    if name == "ACCESS_SEEK":
        if not access_ok(e, w, a):
            return -99, aux
        call(e, F_ACCESS_SEEK, [w.accesses[a], b & MASK])
        return 0, aux
    if name == "ACCESS_SEEK_START":
        if not access_ok(e, w, a):
            return -99, aux
        call(e, F_ACCESS_SEEK_START, [w.accesses[a]])
        return 0, aux
    if name == "ACCESS_DTOR":
        if not access_ok(e, w, a):
            return -99, aux
        call(e, F_ACCESS_DTOR, [w.accesses[a], b & MASK])
        return 0, aux
    if name == "INFO_SET":
        if not 0 <= a < INFOS:
            return -99, aux
        info = w.infos[a]
        offset = {0: 0, 1: 4, 2: 0x14, 3: 0x40, 4: 0x44, 5: 0x48, 6: 0x4C, 7: 0x88}.get(b)
        if offset is None:
            return 0, aux
        value = (w.cbanks[c - 1] if 1 <= c <= 3 else 0) if b == 1 else c
        ws(e, info + offset, [value])
        return 0, aux
    if name == "FILE_POS":
        w.fpos, w.opos = a, b
        return 0, aux
    if name == "CBANK":
        if not 0 <= a < CBANKS:
            return -99, aux
        cbank = w.cbanks[a]
        if b == 0:
            return call(e, F_CBANK_STREAMING, [cbank]) & 0xFF, aux
        if b == 1:
            return call(e, F_CBANK_POSITION, [cbank, c & MASK]) & MASK, aux
        if b == 2:
            return L.lab(call(e, F_CBANK_REF, [cbank])), aux
        return -99, aux
    if name == "CHANNEL_POKE":
        if a == 0:
            e.write(w.chan + 0x15, bytes([b & 0xFF]))
        if a == 1:
            ws(e, w.chan + 0x24, [b])
        return 0, aux
    raise ValueError(op)


# header (size, rate, bits, flags, field8, fieldC, data offset or -1) and info (number, bank, flags, loop, callback, completed
# callback, user data, streaming buffer size) defaults: a WAV sample, an MPEG sample and an MPEG sample with its length given
DEFAULT_HEADERS = [(0x100, 0x5622, 0x10, 0x00, 0x11, 0x22, 0), (0x180, 0x5622, 0x10, 0x20, 0x33, 0x44, 8),
                   (0x1E0, 0x5622, 0x10, 0x24, 0x55, 0x1C0, -1)]
DEFAULT_INFOS = [(1, 1, 0, 0, 0, 0, 0, 0), (2, 2, 0, 0, 0, 0, 0, 0)]


def ppc_case(e, case):
    e.heap_in_scratch = True
    w = e.w = World()
    w.nodes, w.kinds, w.blocks, w.bsizes, w.bpools = [0] * SLOTS, [K_NONE] * SLOTS, [0] * 8, [0] * 8, [0] * 8
    w.factories, w.accesses = [0] * FACTORIES, [0] * ACCESSES
    w.pattern = 0
    w.fpos = w.opos = 0
    w.fdata, w.odata = FILE_DATA, OWN_DATA
    w.headers = []
    for index, (size, rate, bits, flags, field8, fieldC, data) in enumerate(case["hdrs"]):
        base = e.alloc(HEADER_BYTES, b"\xa5" * HEADER_BYTES)
        ptr = 0 if data < 0 else base + 0x28 + data
        content = struct.pack(">IHBBIII", size & MASK, rate & 0xFFFF, bits & 0xFF, flags & 0xFF, field8 & MASK, fieldC & MASK, ptr)
        content += b"\xa5" * (0x28 - len(content))
        content += bytes(data_byte(index, i) for i in range(0x200))
        e.write(base, content)
        w.headers.append(base)
    w.banks = [e.alloc(0x20, struct.pack(">II", VT["bank"], 1) + b"\xa5" * 0x18) for _ in range(BANKS)]
    w.cbanks = [e.alloc(0xC, struct.pack(">III", 0, 1, w.banks[i] if i < BANKS else 0)) for i in range(CBANKS)]
    w.chan = e.alloc(0x60, b"\xa5" * 0x60)
    w.infos = [e.alloc(0x90, b"\xa5" * 0x90) for _ in range(INFOS)]
    for index, (number, bank, flags, loop, cb, ccb, ud, sbs) in enumerate(case["infos"]):
        info = w.infos[index]
        ws(e, info, [number])
        ws(e, info + 4, [w.cbanks[bank - 1] if 1 <= bank <= 3 else 0])
        ws(e, info + 0x14, [flags])
        ws(e, info + 0x40, [loop, cb, ccb, ud])
        ws(e, info + 0x88, [sbs])
    w.buf = e.alloc(BUF_BYTES, b"\xa5" * BUF_BYTES)
    e.write(SCRATCH_SETINFO, b"\xa5" * 0x300)
    e.write(SCRATCH_RESTART, b"\xa5" * 0x300)
    w.fileobj = e.alloc(8, struct.pack(">II", VT["file"], 0))
    fixed = {w.banks[0]: 1, w.banks[1]: 2, w.cbanks[0]: 3, w.cbanks[1]: 4, w.cbanks[2]: 5, w.fileobj: 7, w.infos[0]: 8,
             w.infos[1]: 9, w.chan: 10, w.headers[0]: 40, w.headers[1]: 41, w.headers[2]: 42}
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
                              pools=[ppc_pool_state(e, p) for p in range(NPOOLS)],
                              blocks=[ppc_block_state(e, w, i) for i in range(8)],
                              channel=[e.read(w.chan + 0x15, 1)[0], e.u32(w.chan + 0x24), e.read(w.chan + 0x14, 1)[0]],
                              infos=[ppc_info_state(e, w, i) for i in range(INFOS)],
                              factories=[ppc_factory_state(e, w, i) for i in range(FACTORIES)],
                              accesses=[ppc_access_state(e, w, i) for i in range(ACCESSES)],
                              files=[w.fpos, w.opos],
                              buffer=zlib.crc32(e.read(w.buf, BUF_BYTES)),
                              scratch=zlib.crc32(e.read(SCRATCH_SETINFO, 0x300) + e.read(SCRATCH_RESTART, 0x300)),
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
    lib.tpw_cf_begin.argtypes = []
    lib.tpw_cf_set_header.argtypes = [C.c_int32] + [C.c_uint32] * 6 + [C.c_int32]
    lib.tpw_cf_set_info.argtypes = [C.c_int32, C.c_uint32, C.c_int32] + [C.c_uint32] * 6
    lib.tpw_cf_pool_set.argtypes = [C.c_int32] * 6
    lib.tpw_cf_op.restype = C.c_int64
    lib.tpw_cf_op.argtypes = [C.c_int32] + [C.c_int64] * 5
    lib.tpw_cf_aux.restype = C.c_int64
    lib.tpw_cf_aux.argtypes = [C.c_int32]
    for name in ("slot", "pool", "block", "info", "factory", "access"):
        getattr(lib, "tpw_cf_" + name).argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_cf_channel.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cf_files.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cf_buffer.argtypes = [C.c_void_p]
    lib.tpw_cf_scratch.argtypes = [C.c_void_p]
    CL.host_begin(lib, {}, case["q"])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib.tpw_cf_begin()
    for index, (size, rate, bits, flags, field8, fieldC, data) in enumerate(case["hdrs"]):
        lib.tpw_cf_set_header(index, size & MASK, rate & 0xFFFF, bits & 0xFF, flags & 0xFF, field8 & MASK, fieldC & MASK, data)
    for index, (number, bank, flags, loop, cb, ccb, ud, sbs) in enumerate(case["infos"]):
        lib.tpw_cf_set_info(index, number, bank, flags, loop, cb, ccb, ud, sbs)
    for pool in range(NPOOLS):
        current, low, high, total, level = case["pools"].get(pool, (0, 0, 0, 0, 10))
        lib.tpw_cf_pool_set(pool, current, low, high, total, level)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    steps = []
    buffer = (C.c_uint8 * BUF_BYTES)()
    scratch = (C.c_uint8 * 0x600)()

    def fetch(name, index, words):
        out = (C.c_int64 * words)()
        getattr(lib, "tpw_cf_" + name)(index, out)
        return [int(x) for x in out]

    try:
        for op in case["ops"]:
            before = C.c_int32.in_dll(lib, "tpw_cl_n").value
            result = lib.tpw_cf_op(*op)
            aux = [lib.tpw_cf_aux(0), lib.tpw_cf_aux(1)]
            after = C.c_int32.in_dll(lib, "tpw_cl_n").value
            events = host_events(lib, before, after)
            slots = [fetch("slot", s, 2 + ROWS * ROW) for s in range(SLOTS)]
            pools = []
            for p in range(NPOOLS):
                values = []
                for x in fetch("pool", p, 24):
                    values.append(x)
                    if len(values) > 6 and x == -9:
                        break
                pools.append(values)
            blocks = [fetch("block", i, 8)[:6] for i in range(8)]
            channel = (C.c_int64 * 3)()
            lib.tpw_cf_channel(channel)
            files = (C.c_int64 * 2)()
            lib.tpw_cf_files(files)
            lib.tpw_cf_buffer(buffer)
            lib.tpw_cf_scratch(scratch)
            steps.append(dict(result=result, aux=aux, slots=slots, pools=pools, blocks=blocks, channel=[int(x) for x in channel],
                              infos=[fetch("info", i, 8) for i in range(INFOS)],
                              factories=[fetch("factory", i, 2) for i in range(FACTORIES)],
                              accesses=[fetch("access", i, 4)[:4] for i in range(ACCESSES)],
                              files=[int(x) for x in files], buffer=zlib.crc32(bytes(buffer)), scratch=zlib.crc32(bytes(scratch)),
                              log=events))
    finally:
        lib.tpw_cf_end()
    return dict(steps=steps)


# ---------------------------------------------------------------- the scripts

def op(kind, a=0, b=0, c=0, d=0, x=0):
    return (kind, a, b, c, d, x)


def case(ops, q=None, hdrs=None, infos=None, pools=None):
    return dict(ops=ops, q=q or {}, hdrs=hdrs or DEFAULT_HEADERS, infos=infos or DEFAULT_INFOS, pools=pools or {})


def new_mpeg(slot, header=1, owner=0):
    return op(OP_NEW, slot, K_MPEG2, header, owner)


def new_file(slot, header=1, owner=0, start=0x28, nofile=False):
    return op(OP_NEW, slot, K_MPEG2F, header, owner | (4 if nofile else 0), start)


def chunk(slot, size, offset=0, stopped=0):
    return op(OP_GET_CHUNK, slot, offset, size, stopped)


def start_ptr(header, offset=0):
    """The number SET_DATA_START takes: the header's block + offset."""
    return 0x10000 * (header + 1) + offset


def info_set(info, field, value):
    return op(OP_INFO_SET, info, field, value)


MPEG_RATES = [0x5622, 0xAC44, 0x2B11, 0, 0x10000 + 0x5622, MASK, 0x7FFFFFFF, 0x80000000, 8000, 48000, 22050, 44100, 11025, 1,
              0x5623, 0x5621, 0x15622, 0xFFFF]
TOTALS = [0, 1, 2, 0x100, 0x400, 0x401, 0x7FFFFFFF, 0x80000000, MASK, 0x1FF, 0x1E0, 0x181]
SIZES = [0, 1, 2, 3, 0x10, 0x40, 0x7F, 0x80, 0x100, 0x1DF, 0x1E0, 0x1E1, 0x200, 0x400]


def clocks(r):
    return [r.choice([0, 1, 100, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)]) for _ in range(r.randrange(1, 4))]


def random_headers(r):
    out = []
    for _ in range(3):
        out.append((r.choice([0, 1, 0x40, 0x1E0, r.randrange(0, 0x1E1), 0x80000000]),
                    r.choice([0x5622, 0xAC44, 0x2B11, r.getrandbits(16)]), r.choice([0x10, 8, r.getrandbits(8)]),
                    r.choice([0, 0x20, 0x24, 0x04, 0x25, 0x21, 0x60, r.getrandbits(8)]), r.getrandbits(32),
                    r.choice([0, 1, 0x100, 0x1C0, 0x400, r.getrandbits(32)]), r.choice([-1, 0, 8, 0x40, 0x1F0])))
    return out


# ---- the pools of the MPEG codecs

def pool_literals(pool):
    own = POOLS[pool]["size"]
    return [0, 1, 2, own - 1, own, own, own, own + 1, 0x14, 0x2C, 0x30, 0x3C, 0x18, 0x40, 0x50, 0x100]


def pool_script(r):
    pool = r.choice([P_MPEG2, P_MPEG2F, P_MPEG2, P_MPEG2F, P_WAV, P_FILE, P_LOOP])
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
            if r.random() < 0.5:
                ops.append(new_mpeg(slot, r.randrange(3), r.randrange(3)))
            else:
                ops.append(new_file(slot, r.randrange(3), r.randrange(3), r.choice([0, 0x28, 0x100])))
            live[slot] = True
        elif live:
            slot = r.choice(sorted(live))
            ops.append(op(OP_DTOR, slot, r.choice([1, 1, 1, 0, 2, 0x8000, -1 & MASK])))
            del live[slot]
    return case(ops, {"Clock": clocks(r)}, pools=pools)


def pool_edges():
    out = []
    for pool in (P_MPEG2, P_MPEG2F):
        own = POOLS[pool]["size"]
        # a take from an empty list, a push, a take, the statics after each
        out.append(case([op(OP_NEWOP, 0, pool, own), op(OP_DELETEOP, 0, pool, own, 1), op(OP_NEWOP, 1, pool, own),
                         op(OP_NEWOP, 2, pool, own), op(OP_DELETEOP, 1, pool, own, 1), op(OP_DELETEOP, 2, pool, own, 1),
                         op(OP_TOPUP, pool), op(OP_CLEARCACHE, pool)]))
        # sizes: 0 counts as 1, anything but the class's own comes from the runtime and goes back to it
        for size in sorted(set([0, 1, own - 1, own + 1, 0x14, 0x2C, 0x30, 0x3C, 0x18, 0x40, 0x50, 0x100])):
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
        for other in range(NPOOLS):
            if other != pool:
                out.append(case([op(OP_NEWOP, 0, pool, own), op(OP_DELETEOP, 0, pool, own, 1), op(OP_NEWOP, 1, other, own),
                                 op(OP_DELETEOP, 1, other, own, 1), op(OP_NEWOP, 2, pool, own), op(OP_TOPUP, other)]))
    # the nodes go to their own pools; the cleanup destructors empty them
    out.append(case([new_mpeg(0), new_file(1), op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 1), new_mpeg(0), new_file(1), op(OP_TOPUP, 3),
                     op(OP_TOPUP, 4), op(OP_CS_DTOR, 0, 0), op(OP_CS_DTOR, 1, 0), op(OP_CS_DTOR, 0, 1), op(OP_CS_DTOR, 1, 1)]))
    out.append(case([op(OP_TOPUP, 3), op(OP_TOPUP, 4), op(OP_CS_DTOR, 1, 1), op(OP_TOPUP, 3), op(OP_TOPUP, 4),
                     op(OP_CS_DTOR, 0, 0x7FFF), op(OP_CS_DTOR, 1, 0x8000), op(OP_CS_DTOR, 0, -2 & MASK)]))
    return out


# ---- constructors and destructors

def basic_edges():
    out = []
    for flags in (0, 4, 0x20, 0x24, 0x25, 0x04 | 0x40, 0x3F, 0xFF, 0x20 | 4 | 1):
        for fieldC in (0, 1, 0x100, 0x7FFFFFFF, 0x80000000, MASK):
            hdrs = [(0x100, 0x5622, 0x10, flags, 0x77, fieldC, 0)] * 3
            out.append(case([new_mpeg(0, 1, 0), new_mpeg(1, 2, 1), new_file(2, 0, 2, 0x10), new_file(3, 1, 0, MASK),
                             op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 0), op(OP_DTOR, 2, 1), op(OP_DTOR, 3, 0x7FFF)], hdrs=hdrs))
    for flag in (1, 0, -1 & MASK, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF):
        out.append(case([new_mpeg(0), new_file(1), op(OP_DTOR, 0, flag), op(OP_DTOR, 1, flag), op(OP_NEWOP, 0, P_MPEG2, 0x40),
                         op(OP_NEWOP, 1, P_MPEG2F, 0x50)]))
    # the data pointer of the header (the cached first bytes) reaches the file codec; the file and its start
    for data in (-1, 0, 8, 0x1F0):
        for start in (0, 0x28, 0x7FFFFFFF, MASK):
            hdrs = [(0x100, 0x5622, 0x10, 0x20, 1, 2, data)] * 3
            out.append(case([new_file(0, 0, 0, start), new_file(1, 1, 1, start, True)], hdrs=hdrs))
    return out


def basic_script(r):
    ops = []
    for i in range(3):
        if r.random() < 0.5:
            ops.append(op(OP_NEW, i, K_MPEG2, r.randrange(3), r.choice([0, 1, 2, r.getrandbits(8)])))
        else:
            ops.append(op(OP_NEW, i, K_MPEG2F, r.randrange(3), r.choice([0, 1, 2, 4, 5, 6]), r.choice([0, 0x28, r.getrandbits(32)])))
    for _ in range(r.randrange(0, 4)):
        slot = r.randrange(3)
        ops.append(op(OP_POKE, slot, r.choice([0, 1, 2, 3, 4, 5, 6, 7]), r.choice([0, 1, MASK, r.getrandbits(32)])))
    for slot in r.sample(range(3), r.randrange(1, 4)):
        ops.append(op(OP_DTOR, slot, r.choice([1, 1, 0, 2, 0x8000, -1 & MASK])))
    return case(ops, {"Clock": clocks(r)}, hdrs=random_headers(r))


# ---- opening, closing and decoding

def open_script(r):
    """MPEG codecs opened (through SetInfo or the virtual), read, restarted and closed with random decoder answers."""
    hdrs = random_headers(r)
    kinds = [r.choice([K_MPEG2, K_MPEG2F]) for _ in range(2)]
    ops = []
    for i, k in enumerate(kinds):
        if k == K_MPEG2:
            ops.append(new_mpeg(i, r.randrange(3), r.randrange(3)))
        else:
            ops.append(new_file(i, r.randrange(3), r.randrange(3), r.choice([0, 0x28, 0x100])))
    q = {"Clock": clocks(r), "Open": [r.choice([0, 0, 0, 1, -1, 5, -2, MASK]) for _ in range(r.randrange(1, 4))],
         "Total": [r.choice(TOTALS) for _ in range(r.randrange(1, 4))],
         "Layer": [r.choice([1, 1, 2, 2, 3, 0, 0xA5, 4]) for _ in range(r.randrange(1, 4))],
         "ReadRet": [r.choice([0x10, 0, -1, 0x300, 7])], "Rate": [r.choice(MPEG_RATES) for _ in range(r.randrange(1, 4))]}
    ops.append(op(OP_FILE_POS, r.choice([0, 0x28, 0x100])))
    for _ in range(r.randrange(2, 14)):
        slot = r.randrange(2)
        c = r.random()
        if c < 0.2:
            ops.append(op(OP_SET_INFO, slot, 0, r.randrange(2)))
        elif c < 0.3:
            ops.append(op(OP_OPEN, slot))
        elif c < 0.4:
            ops.append(op(OP_CLOSE, slot))
        elif c < 0.55:
            ops.append(chunk(slot, r.choice(SIZES + [r.randrange(0, 0x220)]), r.choice([0, 0, 1, 0x10, 0x7FF, 0x800]), r.choice([0, 0, 1])))
        elif c < 0.65:
            ops.append(op(OP_RESTART, slot))
        elif c < 0.72:
            ops.append(op(OP_SET_DATA_START, slot, r.choice([0, start_ptr(0), start_ptr(1, 8), start_ptr(2, 0x10), start_ptr(1, 0x30)])))
        elif c < 0.8:
            ops.append(op(OP_GET_BLOCK, slot, r.choice([0, 0x10]), r.choice([0, 1, 0x20, 0x300, 0x400])))
        elif c < 0.9:
            ops.append(op(OP_POKE, slot, r.choice([1, 2, 3, 4, 5, 7, 8]), r.choice([0, 1, 0x20, 0x100, 0x1E0, r.randrange(0, 0x220), MASK, 0x21, 0x41])))
        else:
            ops.append(op(r.choice([OP_SET_NEW_POSITION, OP_SET_POSITION]), slot, r.choice([0, 5, MASK])))
    ops += [op(OP_RELEASE, 0), op(OP_RELEASE, 1)]
    return case(ops, q, hdrs=hdrs, infos=[(1, 1, r.choice([0, 0x40, 0x800, 0x840]), 0, 0, 0, 0, 0),
                                          (2, 2, r.choice([0, 0x40, 0x800, 0x840]), 0, 0, 0, 0, 0)])


def open_edges():
    out = []
    # the rate: the codec's format byte says 22050 for this header (rate 0x5622); the decoder says otherwise or the same
    for rate in MPEG_RATES:
        for total in (0x400, 0, 0x7FFFFFFF):
            for kind in (K_MPEG2, K_MPEG2F):
                maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
                out.append(case([maker, op(OP_OPEN, 0), op(OP_RELEASE, 0)], {"Rate": [rate], "Total": [total]}))
    # the header's rate sets the codec's side of the comparison
    for hrate in (0x5622, 0xAC44, 0x2B11, 0x1234, 0):
        for rate in (0x5622, 0xAC44, 0x2B11, 0x1234, 0, 0x10000, 0x15622):
            for total in (0x400, 0x401, 0x3, 1, 0):
                hdrs = [(0x100, hrate, 0x10, 0x20, 0, 0, 0)] * 3
                out.append(case([new_mpeg(0, 1), op(OP_OPEN, 0), op(OP_RELEASE, 0)], {"Rate": [rate], "Total": [total]}, hdrs=hdrs))
    # the answers of Open: only -1 fails
    for answer in (0, 1, -1, 5, -2, 0x7FFFFFFF, 0x80000000):
        for kind in (K_MPEG2, K_MPEG2F):
            maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
            out.append(case([maker, op(OP_OPEN, 0), op(OP_OPEN, 0), op(OP_CLOSE, 0), op(OP_OPEN, 0), op(OP_RELEASE, 0)],
                            {"Open": [answer]}))
    # the length: from the decoder, or the header's when flag 4 is set; the position against it
    for flags in (0x20, 0x24):
        for fieldC in (0, 1, 0x100, 0x400):
            for total in (0, 1, 0x100, 0x400):
                for position in (0, 1, 0xFF, 0x100, 0x101, 0x3FF, 0x400):
                    hdrs = [(0x100, 0x5622, 0x10, flags, 0, fieldC, 0)] * 3
                    out.append(case([new_mpeg(0, 1), op(OP_POKE, 0, 3, position), op(OP_OPEN, 0), op(OP_RELEASE, 0)],
                                    {"Total": [total]}, hdrs=hdrs))
    # a decoder rate that scales the length: the length divided by the quotient and made even
    for rate in (0x5622, 0xAC44, 0x2B11, 0x2B11 + 1, 0x4000, 0x7FFF, 0x2000, 0x1000, 0x100, 0xFF, 1):
        for total in (0x400, 0x401, 0x1000, 0x3, 0x7FFFFFFF, 0xFFFFFFFF):
            out.append(case([new_mpeg(0, 1), op(OP_POKE, 0, 1, 0x42), op(OP_OPEN, 0), op(OP_RELEASE, 0)],
                            {"Rate": [rate], "Total": [total]}))
            out.append(case([new_mpeg(0, 1), op(OP_POKE, 0, 1, 0x4A), op(OP_OPEN, 0), op(OP_RELEASE, 0)],
                            {"Rate": [rate], "Total": [total]}))
    # SetInfo: the layer I priming for the loop (0x40) and callback (0x800) flags
    for layer in (1, 2, 3, 0):
        for flags in (0, 0x40, 0x800, 0x840, 0x80, 0xF7BF, 0x1000, MASK):
            for kind in (K_MPEG2, K_MPEG2F):
                maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
                out.append(case([maker, op(OP_SET_INFO, 0, 0, 0), op(OP_RESTART, 0), op(OP_RELEASE, 0)], {"Layer": [layer]},
                                infos=[(1, 1, flags, 0, 0, 0, 0, 0), (2, 2, 0, 0, 0, 0, 0, 0)]))
    # a SetInfo that fails: the answer is -1 and no priming; one that succeeds: 0
    out.append(case([new_mpeg(0, 1), op(OP_SET_INFO, 0, 0, 0), op(OP_SET_INFO, 0, 0, 1), op(OP_RELEASE, 0)], {"Open": [-1, 0]}))
    out.append(case([new_file(0, 1), op(OP_SET_INFO, 0, 0, 0), op(OP_SET_INFO, 0, 0, 1), op(OP_RELEASE, 0)], {"Open": [-1, 0]}))
    return out


def chunk_edges():
    out = []
    # a header of `total` bytes decoded in pieces of `size`, with the stopped flag both ways, then the timed calls
    for total in (0x40, 0x100, 0x1E0):
        for size in (0, 1, total - 1, total, total + 1, total // 2, 0x200, 0x7F):
            for stopped in (0, 1):
                for kind in (K_MPEG2, K_MPEG2F):
                    maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
                    out.append(case([maker, op(OP_OPEN, 0)] + [chunk(0, size, 3, stopped) for _ in range(4)] +
                                    [op(OP_RESTART, 0), chunk(0, size, 0, stopped), op(OP_RELEASE, 0)],
                                    {"Total": [total], "Clock": [1000, 1000, 1000, 5000, 9000, 20000]}))
    # the end of the data: SetTime gets the bytes just decoded + 0x1200 (rate by format byte), and a stopped call answers 1
    for fmt in (0x21, 0x22, 0x41, 0x42, 0x29, 0x11):
        for clock in (0, 100, MASK - 1000):
            out.append(case([new_mpeg(0), op(OP_POKE, 0, 1, fmt), op(OP_OPEN, 0), op(OP_POKE, 0, 3, 0x100), chunk(0, 0x20),
                             chunk(0, 0x20), chunk(0, 0), op(OP_POKE, 0, 2, 0), chunk(0, 0x20, 0, 1), chunk(0, 0x20, 0, 1)],
                            {"Clock": [clock, clock + 5000, clock + 10 ** 9 & MASK], "Total": [0x200]}))
    # a timer that is over / not over (the boundary of IsTimeComplete)
    for mtime in (1, 100, 0x7FFFFFFF, 0x80000000, MASK):
        for clock in (0, 99, 100, 101, mtime - 1 & MASK, mtime, mtime + 1 & MASK, MASK):
            out.append(case([new_mpeg(0), op(OP_OPEN, 0), op(OP_POKE, 0, 2, mtime), chunk(0, 0x40, 5), chunk(0, 0, 0, 1)],
                            {"Clock": [clock]}))
    # positions around the length (the compare is unsigned) and sizes around what is left
    for position in (0, 1, 0xFF, 0x100, 0x101):
        for size in (0, 1, 0xFF, 0x100, 0x101, 0x400):
            out.append(case([new_mpeg(0, 1), op(OP_OPEN, 0), op(OP_POKE, 0, 5, 0x100), op(OP_POKE, 0, 3, position),
                             chunk(0, size, 0x10, 0), chunk(0, size, 0x10, 1)], {"Clock": [7], "Total": [0x100]}))
    # lengths and positions with the top bit set (only a count of 0 is decoded there)
    for total, position in ((0, MASK), (1, MASK), (0x80000000, 0x7FFFFFFF), (0x7FFFFFFF, 0x7FFFFFFF), (0x80000000, 0x80000000),
                            (0x80000001, 0x80000000), (5, 5), (5, 4), (5, 6)):
        for stopped in (0, 1):
            out.append(case([new_mpeg(0), op(OP_OPEN, 0), op(OP_POKE, 0, 5, total), op(OP_POKE, 0, 3, position),
                             chunk(0, 0, 0, stopped), chunk(0, 0, 0, stopped)], {"Clock": [3]}))
    # what the decoder answers to Read does not matter: the count is the minimum
    for ret in (0, -1, 5, 0x1000):
        out.append(case([new_mpeg(0, 1), op(OP_OPEN, 0), chunk(0, 0x40), op(OP_GET_BLOCK, 0, 0, 0x20)], {"ReadRet": [ret]}))
    return out


def decoder_edges():
    out = []
    # Restart: the position and the decoder reset; the layer I priming
    for layer in (1, 2, 0xA5):
        for kind in (K_MPEG2, K_MPEG2F):
            maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
            out.append(case([maker, op(OP_OPEN, 0), chunk(0, 0x20), op(OP_RESTART, 0), chunk(0, 0x20), op(OP_RELEASE, 0)],
                            {"Layer": [layer]}))
    # SetDataStart: the codec's start and the decoder's stream
    for kind in (K_MPEG2, K_MPEG2F):
        maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
        out.append(case([maker, op(OP_OPEN, 0), op(OP_SET_DATA_START, 0, start_ptr(2, 0x30)), op(OP_SET_DATA_START, 0, 0),
                         op(OP_SET_DATA_START, 0, start_ptr(0)), op(OP_CLOSE, 0), op(OP_RELEASE, 0)]))
    # GetMpegBlock: the decoder's Read (the file codec answers 0 without a decoder; the memory codec has none to ask)
    for kind in (K_MPEG2, K_MPEG2F):
        maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
        for n in (0, 1, 0x20, 0x300, 0x800, 0x1000):
            out.append(case([maker, op(OP_OPEN, 0), op(OP_GET_BLOCK, 0, 0x10, n), op(OP_CLOSE, 0), op(OP_RELEASE, 0)]))
    out.append(case([new_file(0, 1), op(OP_GET_BLOCK, 0, 0, 0x20), op(OP_CLOSE, 0), op(OP_RESTART, 0) if False else op(OP_RELEASE, 0)]))
    # the empty virtuals, Close without a decoder, Release
    for kind in (K_MPEG2, K_MPEG2F):
        maker = new_mpeg(0, 1) if kind == K_MPEG2 else new_file(0, 1)
        out.append(case([maker, op(OP_SET_NEW_POSITION, 0), op(OP_SET_POSITION, 0, 5), op(OP_CLOSE, 0), op(OP_OPEN, 0), op(OP_OPEN, 0),
                         op(OP_CLOSE, 0), op(OP_CLOSE, 0), op(OP_RELEASE, 0)]))
    return out


# ---- the file codec's Read and Seek

def file_script(r):
    hdrs = random_headers(r)
    start = r.choice([0, 0x28, 0x100, 0x3F0, r.randrange(0, 0x400)])
    ops = [new_file(0, r.randrange(3), r.randrange(3), start), new_file(1, r.randrange(3), r.randrange(3), r.choice([0, start, 0x28]))]
    q = {}
    if r.random() < 0.3:
        q["Read"] = [r.choice([-2, 0, 1, 5, 0x20, -1]) for _ in range(r.randrange(1, 4))]
    if r.random() < 0.25:
        q["Seek"] = [r.choice([0, 0, -1]) for _ in range(r.randrange(1, 3))]
    if r.random() < 0.3:
        q["Pos"] = [r.choice([0, start, start + 5, start + 0x100, start - 4, 0x7FFFFFFF, -1, 0x10000, start + 0x1E0]) for _ in range(r.randrange(1, 5))]
    ops.append(op(OP_FILE_POS, r.choice([start, start, 0, 0x30, 0x200, r.randrange(0, 0x420)])))
    for _ in range(r.randrange(2, 10)):
        slot = r.randrange(2)
        c = r.random()
        if c < 0.45:
            ops.append(op(OP_FILE_READ, slot, r.choice([0, 0, 1, 0x10, 0x7FF]), r.choice(SIZES + [r.randrange(0, 0x220), 0x80000000, MASK, 0x7FFFFFFF])))
        elif c < 0.7:
            ops.append(op(OP_FILE_SEEK, slot, r.choice([0, 1, 5, 0x28, 0x100, 0x3FF, MASK, 0x80000000, r.getrandbits(32)])))
        elif c < 0.8:
            ops.append(op(OP_FILE_POS, r.choice([0, 0x28, 0x100, 0x3FF, 0x400, 0x500, -1, r.randrange(0, 0x420)])))
        elif c < 0.9:
            ops.append(op(OP_POKE, slot, r.choice([4, 6]), r.choice([0, 1, 0x20, 0x100, 0x1E0, MASK, r.randrange(0, 0x400)])))
        else:
            ops.append(op(OP_OPEN, slot))
    ops += [op(OP_RELEASE, 0), op(OP_RELEASE, 1)]
    return case(ops, q, hdrs=hdrs)


def file_edges():
    out = []
    # Read: the bytes left from the file's position (the sample's size minus the position relative to the start)
    for size in (0, 1, 0x40, 0x100):
        for start in (0, 0x28, 0x10):
            for count in (0, 1, size, size + 1, size - 1 if size else 5, 0x7F, 0x300):
                if count < 0:
                    continue
                hdrs = [(size, 0x5622, 0x10, 0x20, 0, 0, -1)] * 3
                out.append(case([new_file(0, 0, 0, start), op(OP_FILE_POS, start), op(OP_FILE_READ, 0, 2, count),
                                 op(OP_FILE_READ, 0, 2, count), op(OP_FILE_READ, 0, 2, count), op(OP_RELEASE, 0)], hdrs=hdrs))
    # the position the file reports: before the start, at the end, past it (the minimum is tested as a signed number)
    for pos in (0, 5, 0x10, 0x11, 0x28, 0x2A, 0x100, 0x128, 0x129, 0x130, 0x400, 0x500, 0x7FFFFFFF, MASK, 0x80000000):
        for count in (0, 1, 0x20, 0x100, 0x200, 0x80000000, MASK):
            out.append(case([new_file(0, 0, 0, 0x28), op(OP_FILE_READ, 0, 0, count), op(OP_RELEASE, 0)], {"Pos": [pos, pos]},
                            hdrs=[(0x100, 0x5622, 0x10, 0x20, 0, 0, -1)] * 3))
    # short reads and failed reads, a failed seek
    for read in (0, 1, 7, -1, -2):
        out.append(case([new_file(0, 0, 0, 0x10), op(OP_FILE_POS, 0x10), op(OP_FILE_READ, 0, 0, 0x40), op(OP_FILE_READ, 0, 0, 0x40),
                         op(OP_RELEASE, 0)], {"Read": [read]}, hdrs=[(0x80, 0x5622, 0x10, 0x20, 0, 0, -1)] * 3))
    for seek in (0, -1):
        for offset in (0, 1, 0x28, MASK, 0x7FFFFFFF, 0x80000000):
            for start in (0, 0x10, 0x7FFFFFFF, MASK):
                out.append(case([new_file(0, 0, 0, start), op(OP_FILE_SEEK, 0, offset), op(OP_RELEASE, 0)], {"Seek": [seek]}))
    # the file codec's Restart does not touch the file
    out.append(case([new_file(0, 1), op(OP_OPEN, 0), op(OP_RESTART, 0), op(OP_RELEASE, 0)]))
    return out


# ---- the file wrapper

def access_script(r):
    ops = []
    q = {}
    if r.random() < 0.3:
        q["OwnRead"] = [r.choice([-2, 0, 1, 5, 0x20, -1]) for _ in range(r.randrange(1, 4))]
    if r.random() < 0.25:
        q["OwnSeek"] = [r.choice([0, 0, -1]) for _ in range(r.randrange(1, 3))]
    if r.random() < 0.3:
        q["OwnPos"] = [r.choice([0, 0x10, 0x28, 0x100, -1, 0x7FFFFFFF, 0x10000, r.randrange(0, 0x420)]) for _ in range(r.randrange(1, 5))]
    for slot in range(ACCESSES):
        ops.append(op(OP_ACCESS_NEW, slot, r.randrange(2), r.choice([0, 0x10, 0x28, 0x7FFFFFFF, MASK, r.getrandbits(32)])))
    ops.append(op(OP_FILE_POS, 0, r.choice([0, 0x10, 0x28, 0x100, r.randrange(0, 0x420)])))
    for _ in range(r.randrange(2, 12)):
        slot = r.randrange(ACCESSES)
        c = r.random()
        if c < 0.4:
            ops.append(op(OP_ACCESS_READ, slot, r.choice([0, 0, 1, 0x10, 0x7FF]), r.choice([0, 1, 5, 0x20, 0x100, 0x300, 0x400, 0x80000000, MASK])))
        elif c < 0.6:
            ops.append(op(OP_ACCESS_SEEK, slot, r.choice([0, 1, 5, 0x28, 0x100, 0x3FF, MASK, 0x80000000, r.getrandbits(32)])))
        elif c < 0.7:
            ops.append(op(OP_ACCESS_SEEK_START, slot))
        elif c < 0.85:
            ops.append(op(OP_FILE_POS, 0, r.choice([0, 0x10, 0x28, 0x100, 0x3FF, 0x400, 0x500, -1, r.randrange(0, 0x420)])))
        else:
            ops.append(op(OP_ACCESS_DTOR, slot, r.choice([1, 1, 0, 2, 0x8000, -1 & MASK])))
    return case(ops, q)


def access_edges():
    out = []
    # both constructors, with the file position matching or not (Read seeks again when it is elsewhere)
    for ctor in (0, 1):
        for start in (0, 0x10, 0x28, 0x7FFFFFFF, MASK):
            for pos in (0, 0x10, 0x28, 0x29, 0x100, -1):
                out.append(case([op(OP_ACCESS_NEW, 0, ctor, start), op(OP_FILE_POS, 0, pos), op(OP_ACCESS_READ, 0, 0, 0x20),
                                 op(OP_ACCESS_READ, 0, 0, 0x20), op(OP_ACCESS_SEEK, 0, 5), op(OP_ACCESS_READ, 0, 0x40, 0x10),
                                 op(OP_ACCESS_SEEK_START, 0), op(OP_ACCESS_READ, 0, 0, 0), op(OP_ACCESS_DTOR, 0, 1)]))
    # the file's answers: short reads, failed reads (the position moves by -1), a failed seek, the position it reports
    for read in (0, 1, 7, -1, -2):
        out.append(case([op(OP_ACCESS_NEW, 0, 1, 0x10), op(OP_FILE_POS, 0, 0x10), op(OP_ACCESS_READ, 0, 0, 0x40),
                         op(OP_ACCESS_READ, 0, 0, 0x40), op(OP_ACCESS_READ, 0, 0, 0x40)], {"OwnRead": [read]}))
    for seek in (0, -1):
        for offset in (0, 1, 0x28, MASK, 0x7FFFFFFF, 0x80000000):
            out.append(case([op(OP_ACCESS_NEW, 0, 1, 0x20), op(OP_ACCESS_SEEK, 0, offset), op(OP_ACCESS_SEEK_START, 0),
                             op(OP_ACCESS_READ, 0, 0, 8)], {"OwnSeek": [seek]}))
    for pos in (0, 0x10, 0x11, 0x28, 0x2A, 0x100, 0x400, MASK, 0x80000000, 0x7FFFFFFF):
        out.append(case([op(OP_ACCESS_NEW, 0, 1, 0x28), op(OP_ACCESS_READ, 0, 0, 8), op(OP_ACCESS_READ, 0, 0, 8)],
                        {"OwnPos": [pos, pos, 0x28]}))
    # the destructor: the flag decides the delete; the second constructor's start
    for flag in (1, 0, -1 & MASK, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF):
        out.append(case([op(OP_ACCESS_NEW, 0, 0, 0), op(OP_ACCESS_NEW, 1, 1, 9), op(OP_ACCESS_DTOR, 0, flag), op(OP_ACCESS_DTOR, 1, flag)]))
    return out


# ---- the bank handle

def cbank_edges():
    out = []
    for stream in (0, 1, 2, -1):
        for pos in (0, 1, 0x100, MASK):
            for number in (0, 1, 2, 0x7FFFFFFF, MASK):
                ops = []
                for cb in range(CBANKS):
                    ops += [op(OP_CBANK, cb, 0), op(OP_CBANK, cb, 1, number), op(OP_CBANK, cb, 2)]
                out.append(case(ops, {"Stream": [stream], "Pos": [pos]}))
    return out


def cbank_script(r):
    ops = []
    for _ in range(r.randrange(1, 8)):
        ops.append(op(OP_CBANK, r.randrange(CBANKS), r.randrange(3), r.choice([0, 1, 2, r.getrandbits(32)])))
    return case(ops, {"Stream": [r.choice([0, 1]) for _ in range(3)], "Pos": [r.choice([0, 0x100, r.getrandbits(32)]) for _ in range(3)]})


# ---- the factory

def factory_edges():
    out = []
    # constructor and destructor
    for flag in (1, 0, -1 & MASK, 2, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF):
        out.append(case([op(OP_FACTORY_NEW, 0), op(OP_FACTORY_NEW, 1), op(OP_FACTORY_DTOR, 0, flag), op(OP_FACTORY_DTOR, 1, flag)]))
    # GetNextSampleFromBank: the number, the bank, the retrieve answers, the header the bank gave
    for number in (0, 1, 2, MASK):
        for bank in (0, 1, 2, 3):
            for err in (0, -1, 1, -2, 5):
                for hdr in (0, 1, 2, 7):
                    out.append(case([op(OP_FACTORY_NEW, 0), op(OP_GET_NEXT, 0, 0)], {"RetErr": [err], "Hdr": [hdr]},
                                    infos=[(number, bank, 0, 0, 0, 0, 0, 0), DEFAULT_INFOS[1]]))
    # CreateCodecModifier: the loop flag and the count (unsigned, above 1)
    for flags in (0, 0x40, 0x80, 0x41, 0xFFFFFFBF, MASK, 0x800, 0x2000):
        for loop in (0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, MASK):
            for level in (10, 0):
                out.append(case([new_mpeg(0, 1), op(OP_FACTORY_NEW, 0), op(OP_MODIFIER, 0, 0, 1, 0, 1), op(OP_RELEASE, 1)],
                                infos=[(1, 1, flags, loop, 0, 0, 0, 0), DEFAULT_INFOS[1]], pools={P_LOOP: (0, 0, 0, 0, level)}))
    # CreateController: the completion flag and what it makes
    for flags in (0, 0x2000, 0x2001, 0xFFFF, 0xDFFF, MASK, 0x4000, 0x800):
        for ccb in (0, 0x11, MASK):
            for ud in (0, 0x22, MASK):
                out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CONTROLLER, 0, 0, 0), op(OP_RELEASE, 0)],
                                infos=[(1, 1, flags, 0, 0x33, ccb, ud, 0), DEFAULT_INFOS[1]]))
    return out


def codec_info(r):
    flags = 0
    for bit in (0x40, 0x800, 0x2000, 0x8000, 0x1, 0x4000):
        if r.random() < 0.35:
            flags |= bit
    return (r.choice([1, 1, 2, 3, 0, 5]), r.choice([1, 2, 1, 2, 3, 0]), flags, r.choice([0, 1, 2, 3, 5, 0x80000000, MASK]),
            r.choice([0, 0x11, 0x2345]), r.choice([0, 0x55]), r.choice([0, 0x77]), r.choice([0, 0x1000, 0x2000, MASK]))


def factory_script(r):
    hdrs = random_headers(r)
    ops = [op(OP_FACTORY_NEW, 0), op(OP_FACTORY_NEW, 1)]
    q = {"Clock": clocks(r), "Stream": [r.choice([0, 1]) for _ in range(3)], "Pos": [r.choice([0, 0x100, 0x3F0, r.getrandbits(32)]) for _ in range(2)],
         "RetErr": [r.choice([0, 0, 0, 0, -1, 1]) for _ in range(3)], "Hdr": [r.randrange(3) for _ in range(3)],
         "Handle": [r.choice([0x1234, 0, MASK, r.getrandbits(32)])],
         "Open": [r.choice([0, 0, 0, -1, 1]) for _ in range(2)], "Rate": [r.choice(MPEG_RATES[:5]) for _ in range(2)],
         "Total": [r.choice(TOTALS) for _ in range(2)], "Layer": [r.choice([1, 2, 2]) for _ in range(2)],
         "RelRet": [r.choice([0, 1])]}
    for _ in range(r.randrange(1, 7)):
        c = r.random()
        if c < 0.55:
            ops.append(op(OP_CREATE_CODEC, r.randrange(2), 1, r.randrange(2), r.randrange(SLOTS)))
        elif c < 0.62:
            ops.append(op(OP_CREATE_CODEC, r.randrange(2), 0, r.randrange(2), r.randrange(SLOTS)))
        elif c < 0.7:
            ops.append(op(OP_CREATE_CONTROLLER, r.randrange(2), r.randrange(2), r.randrange(SLOTS)))
        elif c < 0.78:
            ops.append(op(OP_GET_NEXT, r.randrange(2), r.randrange(2)))
        elif c < 0.85:
            ops.append(op(OP_INFO_SET, r.randrange(2), r.randrange(8), r.choice([0, 1, 2, 3, 0x40, 0x800, 0x2000, 0x8000, r.getrandbits(8)])))
        elif c < 0.92:
            ops.append(op(OP_SET_INFO, r.randrange(SLOTS), 0, r.randrange(2)))
        elif c < 0.96:
            ops.append(op(OP_CHANNEL_POKE, r.randrange(2), r.choice([0, 1, 0x1234, MASK])))
        else:
            ops.append(op(OP_RELEASE, r.randrange(SLOTS)))
    for slot in range(SLOTS):
        if r.random() < 0.6:
            ops.append(op(OP_RELEASE, slot))
    ops.append(op(OP_FACTORY_DTOR, 0, r.choice([0, 1])))
    return case(ops, q, hdrs=hdrs, infos=[codec_info(r), codec_info(r)])


def factory_codec_edges():
    """CreateCodec over the header kinds, the bank's streaming answer, the flags and the failure of every step."""
    out = []
    # the header: WAV or MPEG, in memory or streamed
    for hflags in (0x00, 0x20, 0x24, 0x04, 0x25, 0x21, 0x40, 0xFF):
        for stream in (0, 1):
            for bank in (1, 2):
                for iflags in (0, 0x40, 0x800, 0x840, 0x8000, 0x2000, 0x8840):
                    hdrs = [(0x100, 0x5622, 0x10, hflags, 0x11, 0x180, 8)] * 3
                    out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_SET_INFO, 1, 0, 1),
                                     op(OP_RELEASE, 0)], {"Stream": [stream], "Pos": [0x100 if stream else 0], "Layer": [1, 2]},
                                    hdrs=hdrs, infos=[(3, bank, iflags, 3, 0x11, 0x22, 0x33, 0x1234), DEFAULT_INFOS[1]]))
    # the sizes of the header: 0 fails
    for size in (0, 1, -1 & MASK, 0x80000000):
        for kind in (0x00, 0x20):
            hdrs = [(size, 0x5622, 0x10, kind, 0, 0x100, 0)] * 3
            out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_RELEASE, 0)], hdrs=hdrs))
    # the bank's answers: an error, a header index, a NULL channel when the bank fails early
    for err in (0, -1, 1, -2):
        for ch in (0, 1):
            out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, ch, 0, 0), op(OP_RELEASE, 0)], {"RetErr": [err]},
                            infos=[(0, 1, 0, 0, 0, 0, 0, 0), DEFAULT_INFOS[1]] if ch == 0 else DEFAULT_INFOS))
    for ch in (0, 1):
        out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, ch, 0, 0), op(OP_CREATE_CODEC, 0, ch, 1, 1)],
                        infos=[(1, 0, 0, 0, 0, 0, 0, 0), (1, 3, 0, 0, 0, 0, 0, 0)]))
    # the info's flags: the buffer size only with 0x8000, the callback only with 0x800 (and the flag cleared), the loop only with
    # 0x40 and a count above 1
    for flags in (0, 0x8000, 0x800, 0x40, 0x8840, 0x7FFF, 0xFFFF, MASK, 0x7FFF7FFF):
        for loop in (0, 1, 2, 9):
            for sbs in (0, 0x1000, MASK):
                out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_RELEASE, 0)], {"Layer": [2]},
                                infos=[(1, 1, flags, loop, 0x44, 0x55, 0x66, sbs), DEFAULT_INFOS[1]],
                                hdrs=[(0x100, 0x5622, 0x10, 0x20, 0, 0x180, 8)] * 3))
    # SetInfo fails: the pipe is released and the channel's pipe too; the channel's streaming byte only on success
    for opens in (-1, 0):
        for iflags in (0, 0x40, 0x800):
            for rel in (0, 1):
                for stream in (0, 1):
                    out.append(case([op(OP_CHANNEL_POKE, 0, 0x77), op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_RELEASE, 0)],
                                    {"Open": [opens], "RelRet": [rel], "Stream": [stream]},
                                    infos=[(2, 1, iflags, 3, 0x44, 0x55, 0x66, 0), DEFAULT_INFOS[1]]))
    # no channel although the bank step succeeds (CreateCodec 0x10000810, the channel test is then taken): the header is not handed
    # to a channel, the handle is asked of NULL (the fake records it), and SetInfo failing releases the pipe only (no ReleasePipe);
    # only the MPEG codec's SetInfo fails (Open -1); a success would write the channel's streaming byte through NULL, so only the failing SetInfo is run
    for iflags in (0, 0x40, 0x800, 0x8000, 0x8840):
        for stream in (0, 1):
            for kind in (0x20,):
                out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 2, 0, 0), op(OP_RELEASE, 0)],
                                {"Open": [-1], "Stream": [stream], "Layer": [2]},
                                infos=[(2, 1, iflags, 3, 0x44, 0x55, 0x66, 0x1000), DEFAULT_INFOS[1]],
                                hdrs=[(0x100, 0x5622, 0x10, kind, 0, 0x180, 8)] * 3))
    # the handle goes to the pipe
    for handle in (0, 1, 0x1234, MASK, 0x80000000):
        out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_RELEASE, 0)], {"Handle": [handle]}))
    # allocation exhausted pools or full ones: the pools of the codecs feed the factory
    for level in (0, 1, 10):
        out.append(case([op(OP_TOPUP, P_MPEG2), op(OP_TOPUP, P_MPEG2F), op(OP_TOPUP, P_WAV), op(OP_TOPUP, P_FILE), op(OP_TOPUP, P_LOOP),
                         op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_CREATE_CODEC, 0, 1, 1, 1),
                         op(OP_RELEASE, 0), op(OP_RELEASE, 1), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_RELEASE, 0)],
                        {"Stream": [0, 1, 0], "Layer": [2]}, infos=[(1, 1, 0x40, 2, 0, 0, 0, 0), (2, 2, 0, 0, 0, 0, 0, 0)],
                        pools={p: (0, 0, 0, 0, level) for p in range(NPOOLS)}))
    # controllers beside codecs: the flag is cleared in the info
    out.append(case([op(OP_FACTORY_NEW, 0), op(OP_CREATE_CODEC, 0, 1, 0, 0), op(OP_CREATE_CONTROLLER, 0, 0, 1), op(OP_CREATE_CONTROLLER, 0, 0, 2),
                     op(OP_RELEASE, 0), op(OP_RELEASE, 1)], infos=[(1, 1, 0x2840, 3, 0x44, 0x55, 0x66, 0), DEFAULT_INFOS[1]]))
    return out


# ---- the vtables and the stores of the constructors

VTABLES = {
    MPEG2_VT: [0x10007A74, 0x1000F440, 0x1000E618, 0x1000F834, 0x1000F7B0, 0x10008600, 0x1000D9B8, 0x1000E690, 0x1000E850,
               0x1000E510, 0x10000BB8, 0x1000F628, 0x100084F0, 0x10008580, 0x1000EC8C, 0x1000853C, 0x100085C0, 0x1000D914,
               0x1000DA20, 0x1000DA78, 0x1000866C, 0x1000DAD0, 0x1000EC48, 0x1000E8EC, 0x1000EB44, 0x1000EBE4],
    MPEG2F_VT: [0x10008458, 0x1000F440, 0x1000E618, 0x1000F834, 0x1000F7B0, 0x10008600, 0x1000D9B8, 0x1000E690, 0x100083A8,
                0x1000E510, 0x10000BB8, 0x1000F628, 0x100084F0, 0x10008580, 0x1000EC8C, 0x1000853C, 0x100085C0, 0x1000D914,
                0x1000DA20, 0x1000DA78, 0x1000866C, 0x1000DAD0, 0x10008410, 0x10007B0C, 0x10008304, 0x1000828C],
    ACCESS_VT: [0x100020A8, 0x10001E30, 0x10001FB0],
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
    owner = e.alloc(8, struct.pack(">II", VT["bank"], 1))
    header = e.alloc(0x40, b"\0" * 0x40)
    e.cl = CL.PpcLog({}, {})
    e.w = World()
    e.w.headers = []
    saved = STUBS.activate(e)
    try:
        for name, ctor, args, size in (("mpeg2", POOLS[P_MPEG2]["ctor"], [header, owner], 0x60),
                                       ("file", POOLS[P_MPEG2F]["ctor"], [0, header, 0, owner], 0x60),
                                       ("access0", F_ACCESS_CTOR0, [0], 0x20), ("access1", F_ACCESS_CTOR1, [0, 5], 0x20)):
            block = e.alloc(size, b"\xa5" * size)
            call(e, ctor, [block] + args)
            out[name] = e.u32(block)
    finally:
        CL.Stubs.deactivate(e, saved)
    return out


def stores_host(lib, c):
    return dict(mpeg2=MPEG2_VT, file=MPEG2F_VT, access0=ACCESS_VT, access1=ACCESS_VT)


# ---- mixed scripts

def mixed_script(r):
    hdrs = random_headers(r)
    ops = [op(OP_FACTORY_NEW, 0)]
    q = {"Clock": clocks(r), "Stream": [r.choice([0, 1]) for _ in range(3)], "Pos": [r.choice([0, 0x100]) for _ in range(2)],
         "Hdr": [r.randrange(3) for _ in range(3)], "Open": [r.choice([0, 0, -1]) for _ in range(2)],
         "Rate": [r.choice(MPEG_RATES[:5]) for _ in range(2)], "Total": [r.choice(TOTALS) for _ in range(3)],
         "Layer": [r.choice([1, 2, 2]) for _ in range(2)], "Read": [r.choice([-2, -2, 0x20])]}
    for _ in range(r.randrange(3, 14)):
        c = r.random()
        slot = r.randrange(SLOTS)
        if c < 0.18:
            ops.append(op(OP_CREATE_CODEC, 0, 1, r.randrange(2), slot))
        elif c < 0.28:
            ops.append(new_mpeg(slot, r.randrange(3), r.randrange(3)) if r.random() < 0.5 else new_file(slot, r.randrange(3), r.randrange(3), r.choice([0, 0x28])))
        elif c < 0.4:
            ops.append(chunk(slot, r.choice(SIZES), r.choice([0, 0x10, 0x400]), r.randrange(2)))
        elif c < 0.48:
            ops.append(op(OP_SET_INFO, slot, 0, r.randrange(2)))
        elif c < 0.54:
            ops.append(op(OP_RESTART, slot))
        elif c < 0.6:
            ops.append(op(OP_FILE_READ, slot, 0, r.choice([0x10, 0x100])))
        elif c < 0.65:
            ops.append(op(OP_FILE_SEEK, slot, r.choice([0, 5, 0x28])))
        elif c < 0.7:
            ops.append(op(OP_MODIFIER, 0, r.randrange(2), r.randrange(3), slot, r.randrange(SLOTS)))
        elif c < 0.76:
            ops.append(op(OP_POKE, slot, r.choice([2, 3, 4, 5, 7, 8]), r.choice([0, 1, 0x20, 0x100, 0x1E0, r.randrange(0, 0x220)])))
        elif c < 0.82:
            ops.append(op(r.choice([OP_TOPUP, OP_CLEARCACHE]), r.randrange(NPOOLS)))
        elif c < 0.88:
            ops.append(op(OP_RELEASE, slot))
        elif c < 0.92:
            ops.append(op(OP_DTOR, slot, r.choice([1, 0])))
        else:
            ops.append(op(OP_INFO_SET, r.randrange(2), r.randrange(8), r.choice([0, 1, 2, 0x40, 0x800, 0x8840, r.getrandbits(8)])))
    for slot in range(SLOTS):
        ops.append(op(OP_RELEASE, slot))
    return case(ops, q, hdrs=hdrs, infos=[codec_info(r), codec_info(r)])


TESTS = [
    FnTest("pools of the MPEG codecs: operator new, operator delete, TopUp, SetLevel and ClearCache "
           "(0x1000e1b4 0x1000e284 0x1000e328 0x1000e40c 0x1000e120, 0x100076e4 0x100077b8 0x10007860 0x10007948 0x1000764c) and the "
           "cleanup destructors 0x1000ed30 0x1000881c", ppc_case, host_case, edge=pool_edges, rnd=pool_script),
    FnTest("constructors 0x1000e44c 0x1000798c and destructors 0x10007a74 0x10008458", ppc_case, host_case, edge=basic_edges,
           rnd=basic_script),
    FnTest("_TbSoundSampleMPEG2 and _TbSoundSampleMPEG2File: SetInfo 0x1000e510, OpenMpegFile 0x1000e8ec 0x10007b0c, CloseMpegFile "
           "0x1000eb44 0x10008304, Release 0x1000e618", ppc_case, host_case, edge=open_edges, rnd=open_script),
    FnTest("GetDataChunk 0x1000e690", ppc_case, host_case, edge=chunk_edges, rnd=open_script),
    FnTest("Restart 0x1000e850 0x100083a8, SetDataStart 0x100084f0, GetMpegBlock 0x1000ebe4 0x1000828c, SetNewDataPosition 0x1000ec48 "
           "0x10008410, SetDataPosition 0x1000ec8c", ppc_case, host_case, edge=decoder_edges, rnd=open_script),
    FnTest("_TbSoundSampleMPEG2File: Read 0x100080f4, Seek 0x10008208", ppc_case, host_case, edge=file_edges, rnd=file_script),
    FnTest("_CFileAccess: constructors 0x10001cf0 0x10001d90, Read 0x10001e30, Seek 0x10001fb0, SeekToDataStart 0x10002030, "
           "destructor 0x100020a8", ppc_case, host_case, edge=access_edges, rnd=access_script),
    FnTest("Bank::CBank: operator TbRefRelease* 0x1001458c, IsStreaming 0x100145cc, GetSamplePosition 0x10014720", ppc_case, host_case,
           edge=cbank_edges, rnd=cbank_script),
    FnTest("CCodecFactory: constructor 0x10010b10, destructor 0x10010a88, GetNextSampleFromBank 0x10000390, CreateCodecModifier "
           "0x10000438, CreateController 0x10000054", ppc_case, host_case, edge=factory_edges, rnd=factory_script),
    FnTest("CCodecFactory::CreateCodec 0x100007c4", ppc_case, host_case, edge=factory_codec_edges, rnd=factory_script),
    FnTest("mixed scripts over all of them", ppc_case, host_case, edge=lambda: [], rnd=mixed_script),
    FnTest("the vtables 0x1003c2dc 0x1003bda8 0x10035c94", vtable_ppc, vtable_host, edge=vtable_cases,
           rnd=lambda r: dict(vt=r.choice(list(VTABLES)))),
    FnTest("the vptr stores of the constructors", stores_ppc, stores_host, edge=lambda: [dict()], rnd=lambda r: dict()),
]
