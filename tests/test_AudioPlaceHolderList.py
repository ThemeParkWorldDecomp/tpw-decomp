"""Unit w79g1, CAudioPlaceHolderList and its CMemBlock, against libtpw_unit_AudioPlaceHolderList.dylib
(tests/unit_AudioPlaceHolderList.cpp), in the program "sound symbols shared" (Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/AudioPlaceHolderList.cpp, src/sound/PlaceHolderTypes.cpp, src/sound/TbSysCommand.cpp  CAudioPlaceHolderList: constructor 0x1001bb40, SetNumberOfChannels 0x1001bd68, destructor 0x1001be30,
      ProcessPlaceHolders 0x1001bf78, RemovePlaceHolder 0x1001c170, SetBoundingRectangle 0x1001c23c, GetPlaceholderAndSoundHandle
      0x1001c36c, StopAllPlaceHolders 0x1001c418, UpdatePlaceHolder 0x1001c4c0, UpdatePlaceHolderParameter 0x1001c5b4,
      CreatePlaceHolder 0x1001c678, FindPlaceHolder 0x1001c8d8, NewPlaceHolderMemory 0x1001c9ec, MoveToUsedItemList 0x1001cae0,
      FindFreeItem 0x1001cb8c, CreateHandle 0x1001cc20, NewHandleIndex 0x1001cd10, AddNewBlock 0x1001cd60, AddToFreeItemList
      0x1001cf60, AddToUsedItemList 0x1001cfdc, AddToEndOfUsedItemList 0x1001d058, MoveToFreeItemList 0x1001d270, Handle2Ptr
      0x1001d318, InitialisePriorityList 0x1001d3e0, AddToHiPriorityList 0x1001d46c, Save 0x1001d624, Load 0x1001da8c,
      GetNumberOfChannels 0x10028950; CMemBlock: GetNext 0x1001bf2c, Data 0x1001ceac, SetNext 0x1001cef4, CreateObject 0x1001d0f4,
      constructor 0x1001d1f0, destructor 0x1001e694

Every case is a script of operations run on both sides: the original in the emulator and the host's translation, against a world
the script builds: a list, placeholders made through CreatePlaceHolder (the subclasses' CreateObject statics are recording fakes that
construct a test placeholder of the class they stand for into the slot), state poked into slots, a memory file. The small members of
CAudioPlaceHolder that the list calls (GetHandle, SetHandle, FinishedPlaying, the priority word) run as the original on the PowerPC
side and as copies on the host; the others (VolumeControlIsOn, ProcessPositionAndPriority, AllocateExtraData, Save, Load) and the
virtuals (Release +0x14, PlaySample +0x1c, Stop +0x20, Update +0x30, UpdateParameter +0x34) are recording fakes on both sides, with
the sound lock's nesting count in their events. The file is a memory file whose Read, Write, Seek and GetPos record (slots +0x10,
+0x14, +0x18, +0x20); the streamer's Instance and the three fix-up table calls, the clock and the event's GetType (slot +8) are fakes.
The answers of every operation, the state of the list when a script asks (counters, block chain, free list, used list with the
fields the list touches, the priority list), the call logs (including the allocations of the pool blocks and of the priority array)
and the bytes of the file are compared.

Slots are numbered 0 NULL, 1 + 100 * block + index (blocks in the order of the run's pool allocations), 0xFFFFFFFF any other pointer.
Labels: 1 the list, 2 the category container, 3 the sound data, 4 the event, 5 the file, 7 the streamer, 9 and 10 the position and
velocity, 31.. the standalone blocks.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_AudioPlaceHolderList"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5
OUTCAP = 1 << 16

# the operations (the unit file's Op enum); eight words each
OP_NAMES = ("NEW DELETE SNAP SETSLOT CONSTRUCT POKE FAILNEXT SETCH GETCH PROCESS REMOVE SETBOUND GETPH STOPALL UPDATE UPDPARAM CREATE "
            "FIND NEWMEM MOVEUSED FINDFREE CREATEHANDLE NEWHANDLEINDEX ADDBLOCK ADDFREE ADDUSED ADDEND MOVEFREE HANDLE2PTR INITPRIO "
            "ADDHI SAVE LOAD FILEPOS BLOCKNEW BLOCKCREATE BLOCKGETNEXT BLOCKSETNEXT BLOCKDATA BLOCKINFO BLOCKDEL UNLINK").split()
for _i, _n in enumerate(OP_NAMES):
    globals()["K_" + _n] = _i

ADDR = {
    "ctor": 0x1001BB40, "setch": 0x1001BD68, "dtor": 0x1001BE30, "process": 0x1001BF78, "remove": 0x1001C170, "setbound": 0x1001C23C,
    "getph": 0x1001C36C, "stopall": 0x1001C418, "update": 0x1001C4C0, "updparam": 0x1001C5B4, "create": 0x1001C678, "find": 0x1001C8D8,
    "newmem": 0x1001C9EC, "moveused": 0x1001CAE0, "findfree": 0x1001CB8C, "createhandle": 0x1001CC20, "newindex": 0x1001CD10,
    "addblock": 0x1001CD60, "addfree": 0x1001CF60, "addused": 0x1001CFDC, "addend": 0x1001D058, "movefree": 0x1001D270,
    "handle2ptr": 0x1001D318, "initprio": 0x1001D3E0, "addhi": 0x1001D46C, "save": 0x1001D624, "load": 0x1001DA8C, "getch": 0x10028950,
    "getnext": 0x1001BF2C, "data": 0x1001CEAC, "setnext": 0x1001CEF4, "blockcreate": 0x1001D0F4, "blockctor": 0x1001D1F0,
    "blockdtor": 0x1001E694,
}
# the placeholder members the list calls that are fakes
A_VCON, A_PPP, A_ALLOCX, A_SAVE, A_LOAD = 0x10019B1C, 0x1001A21C, 0x10018034, 0x1001A82C, 0x1001B020
A_GETHANDLE = 0x1001C9B0
# the streamer
A_INSTANCE, A_SAVEFIX, A_LOADFIX, A_UNLOADFIX = 0x1001F6EC, 0x100233E0, 0x1002370C, 0x10023310
# the CreateObject statics: address -> (name, class)
CREATE4 = {0x1001E7E8: ("Create.ListSentence", 7), 0x10024E64: ("Create.SentenceShuffle", 8), 0x10025504: ("Create.BranchingSentence", 1),
           0x100256F4: ("Create.LinearSentence", 2), 0x100259A0: ("Create.OneShot", 3), 0x10025C38: ("Create.OneShotBranchingSentence", 6),
           0x1002669C: ("Create.OneShotSentence", 4), 0x10026C24: ("Create.Sentence", 5)}
CREATE1 = {0x1001E384: ("Load.BranchingSentence", 1), 0x1001E204: ("Load.LinearSentence", 2), 0x1001E194: ("Load.OneShot", 3),
           0x1001E11C: ("Load.OneShotSentence", 4), 0x1001E03C: ("Load.Sentence", 5), 0x1001DDBC: ("Load.OneShotBranchingSentence", 6)}
SLOT = 0x60
HDR = 0xC
FIXUP_MARKER = 0xFEEDF00D


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def le(words):
    return struct.pack("<%dI" % len(words), *[w & MASK for w in words])


# ---------------------------------------------------------------- the script words

def flatten(items):
    out = []
    for a in items:
        out += flatten(a) if isinstance(a, (tuple, list)) else [a]
    return out


class Op:
    """An operation under construction: the code and its argument words (a handle pair counts as two); `op(K_X) + pair + (...)`
    adds words."""

    def __init__(self, code, args=()):
        self.code, self.args = code, tuple(args)

    def __add__(self, other):
        return Op(self.code, self.args + tuple(flatten([other])))

    def words(self):
        assert len(self.args) <= 7, (self.code, self.args)
        return (self.code,) + tuple(a & MASK for a in self.args) + (0,) * (7 - len(self.args))


def op(code, *args):
    return Op(code, flatten(args))


def ABS(n):
    return (1 << 24) | n


def FREE(k):
    return (2 << 24) | k


def USED(k):
    return (3 << 24) | k


def USEDEND(k):
    return (4 << 24) | k


# handle pairs (kind, value)
def H_LIT(v):
    return (0, v & MASK)


def H_USED(k):
    return (1, k)


def H_FREE(k):
    return (2, k)


def H_GEN(k):
    return (3, k)


def H_PLUS1(k):
    return (4, k)


def H_LOW(k):
    return (5, k)


def H_PLUSGEN(k):
    return (6, k)


# ---------------------------------------------------------------- the PowerPC side

class World:
    pass


def sid(w, addr):
    """The slot's number as the unit file's SlotId computes it."""
    if addr == 0:
        return 0
    for i, base in enumerate(w.blocks):
        data = base + HDR
        if data <= addr < data + 100 * SLOT and (addr - data) % SLOT == 0:
            return 1 + 100 * i + (addr - data) // SLOT
    return MASK


def depth(e):
    w = e.w
    return T.s32(e.u32(w.list + 4)) if w.alive else -1


def first8(data):
    return int.from_bytes(data[:8], "little")


def file_read(e, size):
    """The file's Read (slot +0x10): the bytes at the position (zeros past the end), recorded after the read."""
    w, L = e.w, e.cl
    data = bytes(w.file[w.pos + i] if w.pos + i < len(w.file) else 0 for i in range(size))
    L.rec("File.Read", (L.lab(w.fileobj), size, first8(data), depth(e)))
    w.pos = (w.pos + size) & MASK
    return data


def file_write(e, data):
    """The file's Write (slot +0x14): recorded, then the bytes go to the position (the file grows, zero filled)."""
    w, L = e.w, e.cl
    size = len(data)
    L.rec("File.Write", (L.lab(w.fileobj), size, first8(data), depth(e)))
    for i in range(size):
        idx = w.pos + i
        if idx >= (1 << 16):
            break
        if idx >= len(w.file):
            w.file.extend(b"\0" * (idx - len(w.file)))
            w.file.append(0)
        w.file[idx] = data[i]
    w.pos = (w.pos + size) & MASK


def construct(e, slot, cls):
    """What the subclass constructor does to the slot: the class's vtable and the base constructor's writes (the handle at +0x38, the
    links at +0x40 and +0x44 and the bytes +0x52..+0x5b are left alone)."""
    cls = cls if 1 <= cls <= 7 else 8
    e.w.built.add(slot)
    e.write(slot, pack([e.vts[cls]]))
    e.write(slot + 4, pack([0] * 3))
    e.write(slot + 0x10, pack([0] * 6))            # position and velocity: 0.0f
    e.write(slot + 0x28, pack([0, 0, 0, 0]))       # end time, priority, sound handle, category
    e.write(slot + 0x3C, pack([0]))
    e.write(slot + 0x48, pack([0, 0]))             # pause time, max distance
    e.write(slot + 0x50, b"\xff\xff")
    e.write(slot + 0x5C, pack([0]))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.vts = {}

    # ---- the runtime: pool blocks are arrays made outside the constructor; the priority array is the constructor's
    def nwa(e):
        w, L, size = e.w, e.cl, e.r(3)
        if not (L.track and w.track):
            e.set_r(3, e.alloc(max(size, 16), b"\xa5" * max(size, 16)))
            return
        if w.in_ctor:
            block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
            w.others.append(block)
            L.rec("new[]", (2, 0, size // 4))
            e.set_r(3, block)
            return
        if w.fail_next:
            w.fail_next = False
            L.rec("new[]", (1, -1, 0))
            e.set_r(3, 0)
            return
        block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
        L.rec("new[]", (1, len(w.blocks), (size - HDR) // SLOT if size >= HDR else 0))
        w.blocks.append(block)
        e.set_r(3, block)

    def dla(e):
        w, L, p = e.w, e.cl, e.r(3)
        if p and L.track and w.track:
            if p in w.blocks:
                L.rec("delete[]", (1, w.blocks.index(p)))
            elif p in w.others:
                L.rec("delete[]", (2, 0))

    def dl(e):
        L, p = e.cl, e.r(3)
        if p != 0 and L.lab(p) > 0:
            L.rec("delete", (L.lab(p),))
            L.forget(p)
    e.stub("__nwa__FUl", nwa)
    e.stub("__dla__FPv", dla)
    e.stub("__dl__FPv", dl)

    def clock(e):
        e.cl.rec("Clock", ())
        e.set_r(3, T.u32(e.cl.ret("Clock", 1000)))
    e.stub("LbTime_GetClock__Fv", clock)

    # ---- the test subclasses' virtuals: Release +0x14, PlaySample +0x1c, Stop +0x20, Update +0x30, UpdateParameter +0x34
    def v_release(e):
        this = e.r(3)
        e.cl.rec("V.Release", (sid(e.w, this), depth(e), sid(e.w, e.u32(this + 0x40)), sid(e.w, e.u32(this + 0x44))))
        e.write(this + 0x38, pack([e.u32(this + 0x38) & 0xFFFF]))

    def v_play(e):       # (buffer r3, this r4, now r5)
        L = e.cl
        L.rec("V.PlaySample", (sid(e.w, e.r(4)), e.r(5), depth(e)))
        e.write(e.r(3), pack([L.ret("Play", 0)]))

    def v_stop(e):       # (buffer r3, this r4)
        L = e.cl
        L.rec("V.Stop", (sid(e.w, e.r(4)), depth(e)))
        e.write(e.r(3), pack([L.ret("Stop", 0)]))

    def v_update(e):     # (buffer r3, this r4, position r5, velocity r6)
        L = e.cl
        L.rec("V.Update", (sid(e.w, e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), depth(e)))
        e.write(e.r(3), pack([L.ret("Update", 0)]))

    def v_param(e):      # (this r3, id r4, value r5)
        e.cl.rec("V.UpdateParameter", (sid(e.w, e.r(3)), e.r(4), e.r(5), depth(e)))
    fns = {0x14: T.fake_function(e, v_release), 0x1C: T.fake_function(e, v_play), 0x20: T.fake_function(e, v_stop),
           0x30: T.fake_function(e, v_update), 0x34: T.fake_function(e, v_param)}
    e.vt_class = {}
    for cls in range(1, 9):
        table = bytearray(0x58)
        for slot, fn in fns.items():
            table[slot:slot + 4] = pack([fn])
        e.vts[cls] = T.persist_alloc(e, len(table), bytes(table))
        e.vt_class[e.vts[cls]] = cls

    # ---- the placeholder's members the list calls that are fakes (in-binary functions stubbed at their entries)
    def vcon(e):
        L = e.cl
        L.rec("P.VolumeControlIsOn", (sid(e.w, e.r(3)), depth(e)))
        e.set_r(3, 1 if L.ret("VC", 1) != 0 else 0)

    def ppp(e):
        L = e.cl
        L.rec("P.ProcessPositionAndPriority", (sid(e.w, e.r(3)), depth(e)))
        e.set_r(3, 1 if L.ret("PPP", 1) != 0 else 0)

    def allocx(e):
        this = e.r(3)
        e.cl.rec("P.AllocateExtraData", (sid(e.w, this), depth(e)))
        if e.u32(this + 0x5C) == 0:
            extra = e.alloc(0x20, b"\xa5" * 0x20)
            e.write(extra, b"\0")                    # the constructor: id 0, the four values, the rectangle and the child are zero
            e.write(extra + 4, pack([0] * 5))
            e.write(extra + 0x18, pack([0]))
            e.write(this + 0x5C, pack([extra]))

    def p_save(e):       # (buffer r3, this r4, file r5, now r6)
        this = e.r(4)
        e.cl.rec("P.Save", (sid(e.w, this), e.cl.lab(e.r(5)), e.r(6), depth(e)))
        cls = e.vt_class.get(e.u32(this), 0)
        file_write(e, le([cls]))
        file_write(e, le([e.u32(this + 0x3C)]))
        e.write(e.r(3), pack([0]))

    def p_load(e):
        this = e.r(4)
        e.cl.rec("P.Load", (sid(e.w, this), e.cl.lab(e.r(5)), e.r(6), depth(e)))
        flags = struct.unpack("<I", file_read(e, 4))[0]
        e.write(this + 0x3C, pack([flags]))
        e.write(e.r(3), pack([0]))
    def get_handle(e):
        this = e.r(3)
        e.cl.rec("P.GetHandle", (sid(e.w, this), depth(e), sid(e.w, e.u32(this + 0x40)), sid(e.w, e.u32(this + 0x44))))
        e.set_r(3, e.u32(this + 0x38))
    for address, fn in ((A_VCON, vcon), (A_PPP, ppp), (A_ALLOCX, allocx), (A_SAVE, p_save), (A_LOAD, p_load), (A_GETHANDLE, get_handle)):
        T.stub_internal(e, address, fn)

    # ---- the CreateObject statics
    def create4(name, cls):
        def stub(e):     # (container r3, slot r4, sound r5, event r6)
            L = e.cl
            slot = e.r(4)
            L.rec(name, (sid(e.w, slot), L.lab(e.r(3)), L.lab(e.r(5)), L.lab(e.r(6)), depth(e)))
            construct(e, slot, cls)
        return stub

    def create1(name, cls):
        def stub(e):     # (slot r3)
            slot = e.r(3)
            e.cl.rec(name, (sid(e.w, slot), depth(e)))
            construct(e, slot, cls)
        return stub
    for address, (name, cls) in CREATE4.items():
        T.stub_internal(e, address, create4(name, cls))
    for address, (name, cls) in CREATE1.items():
        T.stub_internal(e, address, create1(name, cls))

    # ---- the streamer
    def instance(e):
        e.cl.rec("Streamer.Instance", ())
        e.set_r(3, e.w.streamer)

    def save_fix(e):     # (buffer r3, streamer r4, file r5)
        L = e.cl
        L.rec("Streamer.SaveFixup", (L.lab(e.r(4)), L.lab(e.r(5)), depth(e)))
        file_write(e, le([FIXUP_MARKER]))
        e.write(e.r(3), pack([0]))

    def load_fix(e):
        L = e.cl
        L.rec("Streamer.LoadFixup", (L.lab(e.r(4)), L.lab(e.r(5)), depth(e)))
        file_read(e, 4)
        e.write(e.r(3), pack([0]))

    def unload_fix(e):
        e.cl.rec("Streamer.UnloadFixup", (e.cl.lab(e.r(3)), depth(e)))
    for address, fn in ((A_INSTANCE, instance), (A_SAVEFIX, save_fix), (A_LOADFIX, load_fix), (A_UNLOADFIX, unload_fix)):
        T.stub_internal(e, address, fn)

    # ---- the file object's table: +0x10 Read, +0x14 Write, +0x18 Seek, +0x20 GetPos
    def f_read(e):       # (this r3, destination r4, size r5)
        size = e.r(5)
        data = file_read(e, size)
        if size:
            e.write(e.r(4), data)
        e.set_r(3, size)

    def f_write(e):      # (this r3, source r4, size r5)
        size = e.r(5)
        file_write(e, e.read(e.r(4), size) if size else b"")
        e.set_r(3, size)

    def f_seek(e):       # (this r3, offset r4, whence r5)
        w, L = e.w, e.cl
        offset, whence = e.r(4), e.r(5)
        L.rec("File.Seek", (L.lab(e.r(3)), offset, whence, depth(e)))
        if whence == 0:
            w.pos = offset
        elif whence == 1:
            w.pos = (w.pos + offset) & MASK
        else:
            w.pos = (len(w.file) + offset) & MASK
        e.set_r(3, 0)

    def f_getpos(e):
        w, L = e.w, e.cl
        L.rec("File.GetPos", (L.lab(e.r(3)), w.pos, depth(e)))
        e.set_r(3, w.pos)
    words = [0] * 12
    words[4], words[5], words[6], words[8] = (T.fake_function(e, f_read), T.fake_function(e, f_write), T.fake_function(e, f_seek),
                                              T.fake_function(e, f_getpos))
    e.vt_file = T.persist_alloc(e, 48, pack(words))

    # ---- the event's table: +8 GetType
    def gettype(e):
        L = e.cl
        L.rec("Event.GetType", (L.lab(e.r(3)), depth(e)))
        e.set_r(3, e.u32(e.r(3) + 4))
    words = [0] * 8
    words[2] = T.fake_function(e, gettype)
    e.vt_event = T.persist_alloc(e, 32, pack(words))


def ppc_slot(e, selector):
    """The slot a selector names (0 when it does not resolve)."""
    w = e.w
    kind, k = selector >> 24, selector & 0xFFFFFF
    if not w.alive:
        return 0
    if kind == 1:
        if k == 0 or (k - 1) // 100 >= len(w.blocks):
            return 0
        return w.blocks[(k - 1) // 100] + HDR + ((k - 1) % 100) * SLOT
    if kind in (2, 3):
        return nth(e, e.u32(w.list + (0xC if kind == 2 else 0x10)), k)
    if kind == 4:
        return nth_from_end(e, e.u32(w.list + 0x10), k)
    return 0


def nth(e, head, k):
    guard = 0
    p = head
    while p != 0 and guard < 100000:
        if k == 0:
            return p
        k -= 1
        p = e.u32(p + 0x40)
        guard += 1
    return 0


def nth_from_end(e, head, k):
    tail = head
    guard = 0
    while tail != 0 and e.u32(tail + 0x40) != 0 and guard < 100000:
        tail = e.u32(tail + 0x40)
        guard += 1
    while tail != 0 and k > 0:
        tail = e.u32(tail + 0x44)
        k -= 1
    return tail


def ppc_handle(e, kind, value):
    w = e.w
    if kind == 0:
        return value & MASK
    if not w.alive:
        return 0
    slot = nth(e, e.u32(w.list + (0xC if kind == 2 else 0x10)), value)
    if slot == 0:
        return 0
    handle = e.u32(slot + 0x38)
    if kind in (1, 2):
        return handle
    if kind == 3:
        return handle ^ 0x10000
    if kind == 4:
        return (handle + 1) & MASK
    if kind == 5:
        return handle & 0xFFFF
    if kind == 6:
        return (handle + 0x10000) & MASK
    return 0


def snapshot(e, out, priority):
    w = e.w
    u = e.u32
    out.append(0xB000)
    if not w.alive:
        out.append(0xDEAD)
        return
    lst = w.list
    out += [u(lst + 0x14), u(lst + 0x18), u(lst + 0x1C), u(lst + 0x24), u(lst + 0x28), u(lst + 4)]
    chain, b, guard = [], u(lst + 8), 0
    while b and guard < 64:
        chain.append(b)
        b = u(b + 8)
        guard += 1
    out.append(len(chain))
    for b in chain:
        out.append(w.blocks.index(b) if b in w.blocks else MASK)
        out.append(u(b + 4) // SLOT)
    free, p, n = [], u(lst + 0xC), 0
    while p and n < 2000:
        free.append(p)
        p = u(p + 0x40)
        n += 1
    out.append(len(free))
    for p in free:
        out += [sid(w, p), u(p + 0x38), sid(w, u(p + 0x44)), u(p + 0x3C)]
    used, p, n = [], u(lst + 0x10), 0
    while p and n < 2000:
        used.append(p)
        p = u(p + 0x40)
        n += 1
    out.append(len(used))
    for p in used:
        out += [sid(w, p), u(p + 0x38), sid(w, u(p + 0x44)), u(p + 0x3C), u(p + 0x30), u(p + 0x2C), u(p + 0x28), u(p + 0x48), u(p + 0xC)]
        extra = u(p + 0x5C) if p in w.built else 0
        out.append(1 if extra else 0)
        if extra:
            out += [u(extra + 8), u(extra + 0xC), u(extra + 0x10), u(extra + 0x14)]
    if priority:
        array = u(lst + 0x20)
        for i in range(31):
            out.append(sid(w, u(array + 4 * i)))


def ppc_run(e, c):
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    w = e.w = World()
    w.alive, w.list, w.blocks, w.others, w.built = False, 0, [], [], set()
    w.in_ctor, w.fail_next, w.track = False, False, False
    w.file, w.pos = bytearray(c["filein"]), 0
    w.fileobj = e.alloc(8, pack([e.vt_file, 0]))
    w.streamer = e.alloc(0x40, b"\xa5" * 0x40)
    w.event = e.alloc(8, pack([e.vt_event, 0]))
    w.container = e.alloc(16, b"\xa5" * 16)
    w.sound = e.alloc(0x14, b"\xa5" * 0x14)
    w.pos_arg = e.alloc(12, pack([0x3F800000, 0x40000000, 0x40400000]))
    w.vel_arg = e.alloc(12, pack([0x3F000000, 0x3E800000, 0x3E000000]))
    w.blockobjs = [0] * 4
    for addr, label in ((w.fileobj, 5), (w.streamer, 7), (w.pos_arg, 9), (w.vel_arg, 10), (w.container, 2), (w.event, 4), (w.sound, 3)):
        L.labels[addr] = label
    out = []
    call = T.call_ppc
    w.track = True
    L.track = True
    ops = c["ops"]
    for index, o in enumerate(ops):
        code, a, b, cc, d, f, g, h = o
        out.append(0xA000 + index)
        lst = w.list
        if code == K_NEW:
            w.in_ctor = True
            if w.list:
                L.forget(w.list)
            w.list = lst = e.alloc(0x40, b"\xa5" * 0x40)
            call(e, ADDR["ctor"], [lst, a])
            w.in_ctor = False
            w.alive = True
            L.labels[lst] = 1
        elif code == K_DELETE:
            if not w.alive:
                out.append(0xDEAD)
                continue
            call(e, ADDR["dtor"], [lst, a])
            w.alive = False
        elif code == K_SNAP:
            snapshot(e, out, a != 0)
        elif code in (K_SETSLOT, K_CONSTRUCT, K_REMOVE, K_MOVEUSED, K_CREATEHANDLE, K_ADDFREE, K_ADDUSED, K_ADDEND, K_MOVEFREE, K_ADDHI,
                      K_UNLINK):
            slot = ppc_slot(e, a)
            if slot == 0:
                out.append(0xDEAD)
                continue
            if code == K_SETSLOT:
                offset = {0: 0x3C, 1: 0x30, 2: 0x2C, 3: 0xC, 4: 0x28, 5: 0x48}.get(b, 0x38)
                e.write(slot + offset, pack([cc]))
            elif code == K_CONSTRUCT:
                construct(e, slot, b)
            elif code == K_REMOVE:
                call(e, ADDR["remove"], [lst, slot])
            elif code == K_MOVEUSED:
                call(e, ADDR["moveused"], [lst, slot])
            elif code == K_CREATEHANDLE:
                out.append(call(e, ADDR["createhandle"], [lst, slot])[0] & MASK)
            elif code == K_ADDFREE:
                call(e, ADDR["addfree"], [lst, slot])
            elif code == K_ADDUSED:
                call(e, ADDR["addused"], [lst, slot])
            elif code == K_ADDEND:
                call(e, ADDR["addend"], [lst, slot])
            elif code == K_MOVEFREE:
                call(e, ADDR["movefree"], [lst, slot])
            elif code == K_ADDHI:
                call(e, ADDR["addhi"], [lst, slot])
            else:   # K_UNLINK
                head_offset = 0xC if (b & 1) == 0 else 0x10
                prev, nxt = e.u32(slot + 0x44), e.u32(slot + 0x40)
                if prev:
                    e.write(prev + 0x40, pack([nxt]))
                else:
                    e.write(lst + head_offset, pack([nxt]))
                if nxt:
                    e.write(nxt + 0x44, pack([prev]))
                if (b & 2) == 0:
                    e.write(slot + 0x40, pack([0, 0]))
        elif code == K_POKE:
            if not w.alive:
                out.append(0xDEAD)
                continue
            offset = {0: 0x1C, 1: 0x18, 2: 0x24}.get(a, 0x14)
            e.write(lst + offset, pack([b]))
        elif code == K_FAILNEXT:
            w.fail_next = True
        elif code in (K_SETCH, K_GETCH, K_PROCESS, K_STOPALL, K_NEWHANDLEINDEX, K_INITPRIO, K_ADDBLOCK, K_SAVE, K_LOAD, K_NEWMEM, K_FINDFREE,
                      K_SETBOUND, K_GETPH, K_UPDATE, K_UPDPARAM, K_CREATE, K_FIND, K_HANDLE2PTR):
            if not w.alive:
                out.append(0xDEAD)
                continue
            if code == K_SETCH:
                call(e, ADDR["setch"], [lst, a])
            elif code == K_GETCH:
                out.append(call(e, ADDR["getch"], [lst])[0] & MASK)
            elif code == K_PROCESS:
                call(e, ADDR["process"], [lst])
            elif code == K_STOPALL:
                call(e, ADDR["stopall"], [lst, a])
            elif code == K_NEWHANDLEINDEX:
                out.append(call(e, ADDR["newindex"], [lst])[0] & MASK)
            elif code == K_INITPRIO:
                call(e, ADDR["initprio"], [lst])
            elif code == K_ADDBLOCK:
                buf = e.alloc(4, pack([FILL]))
                call(e, ADDR["addblock"], [buf, lst])
                out.append(e.u32(buf))
            elif code == K_SAVE:
                buf = e.alloc(4, pack([FILL]))
                call(e, ADDR["save"], [buf, lst, w.fileobj])
                out.append(e.u32(buf))
            elif code == K_LOAD:
                buf = e.alloc(4, pack([FILL]))
                call(e, ADDR["load"], [buf, lst, w.fileobj, a & 0xFF])
                out.append(e.u32(buf))
            elif code == K_NEWMEM:
                cell = e.alloc(4, pack([0xDEADBEEF]))
                slot = call(e, ADDR["newmem"], [lst, cell])[0]
                out += [sid(w, slot), e.u32(cell)]
            elif code == K_FINDFREE:
                out.append(sid(w, call(e, ADDR["findfree"], [lst, a])[0]))
            elif code == K_SETBOUND:
                rect = e.alloc(16, pack([cc, d, f, g]))
                buf = e.alloc(4, pack([FILL]))
                call(e, ADDR["setbound"], [buf, lst, ppc_handle(e, a, b), rect])
                out.append(e.u32(buf))
            elif code == K_GETPH:
                cell = e.alloc(4, pack([FILL]))
                out.append(call(e, ADDR["getph"], [lst, ppc_handle(e, a, b), cell])[0] & MASK)
                out.append(sid(w, e.u32(cell)))
            elif code == K_UPDATE:
                out.append(call(e, ADDR["update"], [lst, ppc_handle(e, a, b), w.pos_arg, w.vel_arg])[0] & MASK)
            elif code == K_UPDPARAM:
                out.append(call(e, ADDR["updparam"], [lst, ppc_handle(e, a, b), cc & 0xFF, d])[0] & MASK)
            elif code == K_CREATE:
                e.write(w.sound + 0x10, struct.pack(">H", b & 0xFFFF))
                e.write(w.event + 4, pack([cc]))
                out.append(call(e, ADDR["create"], [lst, w.container if a else 0, w.sound, w.event])[0] & MASK)
            elif code == K_FIND:
                out.append(sid(w, call(e, ADDR["find"], [lst, ppc_handle(e, a, b)])[0]))
            elif code == K_HANDLE2PTR:
                out.append(sid(w, call(e, ADDR["handle2ptr"], [lst, ppc_handle(e, a, b)])[0]))
        elif code == K_FILEPOS:
            w.pos = a
        elif code == K_BLOCKNEW:
            obj = e.alloc(16, b"\xa5" * 16)
            call(e, ADDR["blockctor"], [obj, b])
            w.blockobjs[a & 3] = obj
            L.labels[obj] = 31 + (a & 3)
        elif code == K_BLOCKCREATE:
            buf = e.alloc(4, pack([FILL]))
            cell = e.alloc(4, pack([FILL]))
            call(e, ADDR["blockcreate"], [buf, 0 if cc else cell, b])
            out.append(e.u32(buf))
            if not cc and T.s32(e.u32(buf)) >= 0:
                w.blockobjs[a & 3] = e.u32(cell)
                L.labels[e.u32(cell)] = 31 + (a & 3)
        elif code in (K_BLOCKGETNEXT, K_BLOCKSETNEXT, K_BLOCKDATA, K_BLOCKINFO, K_BLOCKDEL):
            blk = w.blockobjs[a & 3]
            if blk == 0:
                out.append(0xDEAD)
                continue
            if code == K_BLOCKGETNEXT:
                out.append(L.lab(call(e, ADDR["getnext"], [blk])[0]) & MASK)
            elif code == K_BLOCKSETNEXT:
                call(e, ADDR["setnext"], [blk, 0 if b == 0 else w.blockobjs[(b - 1) & 3]])
            elif code == K_BLOCKDATA:
                out.append(1 if (call(e, ADDR["data"], [blk])[0] - blk) & MASK == HDR else 0)
            elif code == K_BLOCKINFO:
                out.append(e.u32(blk + 4) // SLOT if b else e.u32(blk + 4))
                out.append(L.lab(e.u32(blk + 8)) & MASK)
            else:
                call(e, ADDR["blockdtor"], [blk, b])
                w.blockobjs[a & 3] = 0
    L.track = False
    w.track = False
    out.append(0xF000)
    out.append(len(w.file))
    data = bytes(w.file) + b"\0" * (-len(w.file) % 4)
    out += list(struct.unpack("<%dI" % (len(data) // 4), data))
    return dict(out=out, log=list(L.log))


def host_run(lib, c):
    flat = [v for o in c["ops"] for v in o]
    ops = (C.c_uint32 * len(flat))(*flat)
    out = (C.c_uint32 * OUTCAP)()
    count = C.c_uint32()
    filein = c["filein"]
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_apl_run(ops, len(c["ops"]), filein if filein else b"", len(filein), out, C.byref(count), OUTCAP)
    log = CL.host_end(lib)
    return dict(out=list(out[:min(count.value, OUTCAP)]), log=log)


lib_ready = {}


def host_run_checked(lib, c):
    if id(lib) not in lib_ready:
        lib.tpw_apl_run.argtypes = [C.c_void_p, C.c_uint32, C.c_char_p, C.c_uint32, C.c_void_p, C.c_void_p, C.c_uint32]
        lib_ready[id(lib)] = True
    return host_run(lib, c)


# ---------------------------------------------------------------- building cases

def case(ops, q=None, filein=b""):
    queues = {k: (v if isinstance(v, list) else [v]) for k, v in (q or {}).items()}
    return dict(ops=[o.words() for o in ops], q=queues, filein=filein)


def new(ch):
    return op(K_NEW, ch)


def create(flags=0, type_=0, container=1):
    return op(K_CREATE, container, flags, type_)


def snap(priority=0):
    return op(K_SNAP, priority)


def setslot(sel, field, value):
    return op(K_SETSLOT, sel, field, value)


# the sound flag half-words: bit 4 (looping), 2, 0x10, 0x400 and 0x100 choose the class
F_LOOP, F_B2, F_LINEAR, F_BRANCH, F_SHUFFLE = 4, 2, 0x10, 0x400, 0x100
CLASS_FLAGS = [0, F_LOOP, F_LOOP | F_B2, F_LOOP | F_LINEAR, F_LOOP | F_BRANCH, F_LOOP | F_B2 | F_BRANCH, F_LOOP | F_B2 | F_SHUFFLE,
               F_LOOP | F_B2 | F_BRANCH | F_SHUFFLE, F_LOOP | F_LINEAR | F_B2, F_LOOP | F_LINEAR | F_BRANCH | F_SHUFFLE, F_B2, F_BRANCH]
CHANNELS = [0, 1, 2, 3, 12, 29, 30, 31, 32, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]


def rflags(r):
    base = r.choice(CLASS_FLAGS)
    if r.random() < 0.3:
        base |= r.choice([1, 8, 0x20, 0x40, 0x80, 0x200, 0x800, 0x8000, r.getrandbits(16)])
    return base & 0xFFFF


def rtype(r):
    return r.choice([0, 0, 0, 1, 2, 3, 0xFFFFFFFF])


def prefix(r, n=None, ch=None, big=0.04, init=True, remove=True):
    """A new list with n placeholders (a few of them released again); returns (ops, used count)."""
    if n is None:
        n = r.choice([0, 1, 2, 3, 4, 5, 6, 8]) if r.random() > big else r.choice([99, 100, 101, 150, 201])
    ops = [new(ch if ch is not None else r.choice([12, 30, 1, 5, 12, 12]))]
    if init:
        ops.append(op(K_INITPRIO))
    for _ in range(n):
        ops.append(create(rflags(r), rtype(r)))
    used = n
    for _ in range(r.randrange(0, 3) if n and remove else 0):
        if used == 0:
            break
        ops.append(op(K_REMOVE, USED(r.randrange(used))))
        used -= 1
    return ops, used


def steer(r, used, prio_domain=6, sample_domain=4):
    """Random state for the first `used` used slots: flags, priority word, sample word, end and pause time, sound handle."""
    ops = []
    for k in range(used):
        flags = r.choice([0, 0, 1, 2, 3, 0x10, 0x40, 0x100, 0x142, 0x400, 0x410, 0x412, r.getrandbits(13)])
        ops.append(setslot(USED(k), 0, flags))
        ops.append(setslot(USED(k), 2, r.choice([0, 1, 5, 5, 0x10000, 0x10005, 0x0FFFF, 0x1FFFF, r.randrange(prio_domain) * 0x10001, r.getrandbits(32)])))
        ops.append(setslot(USED(k), 3, r.randrange(sample_domain)))
        ops.append(setslot(USED(k), 4, r.choice([0, 500, 999, 1000, 1001, 2000, 0xFFFFFFFF])))
        ops.append(setslot(USED(k), 5, r.choice([0, 0, 1, 100, 0xFFFFFFFF])))
        ops.append(setslot(USED(k), 1, r.choice([0, 0x77, 0xDEAD0001, r.getrandbits(32)])))
    return ops


def rhandle(r, used, free, safe=False):
    """A handle pair that names a used slot, a free slot, a stale one, or nothing. `safe`: no literal that could name a slot nobody
    constructed (or released), for the operations that call a virtual of the placeholder."""
    k = r.choice(["used", "used", "used", "free", "gen", "plus1", "low", "plusgen", "lit", "lit0"])
    if k == "used" and used:
        return H_USED(r.randrange(used))
    if k == "free" and free:
        return H_FREE(r.randrange(free))
    if k == "gen" and used:
        return H_GEN(r.randrange(used))
    if k == "plus1" and used:
        return H_PLUS1(r.randrange(used))
    if k == "low" and used:
        return H_LOW(r.randrange(used))
    if k == "plusgen" and used:
        return H_PLUSGEN(r.randrange(used))
    if k == "lit0":
        return H_LIT(0)
    value = r.choice([0, 1, 99, 100, 101, 0xFFFF, 0x10000, 0x10001, 0x20063, 0x1FFFF, 0x12C, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(16)])
    if safe and (value >> 16) == 0 and (value & 0xFFFF) < 1000:
        value |= 0x80000000
    return H_LIT(value)


def qlist(r, n, values, length=None):
    return [r.choice(values) for _ in range(length if length is not None else n + 2)]


TESTS = []


def register(name, address, edge, rnd):
    TESTS.append(FnTest("%s 0x%08x" % (name, address), ppc_run, host_run_checked, edge=edge, rnd=rnd))


# ---- the edge and random cases, one pair per function

def edge_ctor():
    return [case([new(ch), snap(1)]) for ch in CHANNELS] + [case([new(12), op(K_NEW, 30), snap(1)])]


def rnd_ctor(r):
    return case([new(r.choice(CHANNELS + [r.getrandbits(32), r.getrandbits(5)])), snap(1)])


def edge_setch():
    return [case([new(12), op(K_SETCH, n), op(K_GETCH), snap()]) for n in CHANNELS]


def rnd_setch(r):
    return case([new(r.choice(CHANNELS + [r.getrandbits(5)])), op(K_SETCH, r.choice(CHANNELS + [r.getrandbits(32), r.getrandbits(5)])), op(K_GETCH), snap()])


def edge_getch():
    return [case([new(n), op(K_GETCH), op(K_SETCH, 5), op(K_GETCH)]) for n in (0, 12, 30, 31, 0xFFFFFFFF)]


def rnd_getch(r):
    return case([new(r.getrandbits(r.choice([3, 5, 32]))), op(K_GETCH), op(K_SETCH, r.getrandbits(r.choice([3, 5, 32]))), op(K_GETCH), snap()])


FLAGS_DEL = [0xFFFF, 0, 1, 2, 0x7FFF, 0x8000, 0x10001, 0x1FFFF, 0xFFFFFFFF, 0x80000001]


def edge_dtor():
    cases = [case([new(12), op(K_DELETE, f)]) for f in FLAGS_DEL]
    for n in (1, 2, 3, 99, 100, 101, 205):
        for f in (0xFFFF, 1):
            cases.append(case([new(12)] + [create(F_LOOP | F_B2, 0)] * n + [op(K_DELETE, f)]))
    cases.append(case([new(12)] + [create(0)] * 4 + [op(K_REMOVE, USED(1)), op(K_REMOVE, USED(0)), op(K_DELETE, 1)]))
    return cases


def rnd_dtor(r):
    ops, used = prefix(r)
    return case(ops + [snap(), op(K_DELETE, r.choice(FLAGS_DEL)), snap()])


def edge_getnext():
    cases = []
    for linked in (0, 1, 2):
        ops = [op(K_BLOCKNEW, 0, 96), op(K_BLOCKNEW, 1, 192), op(K_BLOCKGETNEXT, 0), op(K_BLOCKGETNEXT, 1)]
        if linked:
            ops += [op(K_BLOCKSETNEXT, 0, 2), op(K_BLOCKGETNEXT, 0), op(K_BLOCKSETNEXT, 1, 1), op(K_BLOCKGETNEXT, 1)]
        if linked == 2:
            ops += [op(K_BLOCKSETNEXT, 0, 0), op(K_BLOCKGETNEXT, 0), op(K_BLOCKINFO, 0, 0)]
        cases.append(case(ops))
    cases.append(case([op(K_BLOCKGETNEXT, 3)]))
    return cases


def rnd_getnext(r):
    ops = [op(K_BLOCKNEW, i, r.choice([0, 96, 9600, r.getrandbits(32)])) for i in range(r.randrange(1, 4))]
    for _ in range(r.randrange(1, 6)):
        ops.append(r.choice([op(K_BLOCKGETNEXT, r.randrange(4)), op(K_BLOCKSETNEXT, r.randrange(4), r.randrange(5)), op(K_BLOCKINFO, r.randrange(4), 0)]))
    return case(ops)


def edge_data():
    cases = []
    for count in (0, 1, 2, 100, 150):
        cases.append(case([op(K_BLOCKCREATE, 0, count, 0), op(K_BLOCKDATA, 0), op(K_BLOCKINFO, 0, 1)]))
    cases.append(case([op(K_BLOCKNEW, 0, 96), op(K_BLOCKDATA, 0)]))
    return cases


def rnd_data(r):
    ops = []
    for i in range(r.randrange(1, 4)):
        ops.append(r.choice([op(K_BLOCKNEW, i, r.getrandbits(r.choice([5, 32]))), op(K_BLOCKCREATE, i, r.choice([0, 1, 3, 100, r.randrange(200)]), 0)]))
        ops.append(op(K_BLOCKDATA, i))
    return case(ops)


def edge_setnext():
    return [case([op(K_BLOCKNEW, 0, 96), op(K_BLOCKNEW, 1, 96), op(K_BLOCKSETNEXT, 0, 2), op(K_BLOCKINFO, 0, 0), op(K_BLOCKSETNEXT, 0, 0),
                  op(K_BLOCKINFO, 0, 0), op(K_BLOCKSETNEXT, 1, 2), op(K_BLOCKINFO, 1, 0)])]


def rnd_setnext(r):
    ops = [op(K_BLOCKNEW, i, 96 * i) for i in range(3)]
    for _ in range(r.randrange(1, 6)):
        ops.append(op(K_BLOCKSETNEXT, r.randrange(3), r.randrange(4)))
        ops.append(op(K_BLOCKINFO, r.randrange(3), 0))
    return case(ops)


def edge_blockctor():
    return [case([op(K_BLOCKNEW, 0, s), op(K_BLOCKINFO, 0, 0)]) for s in (0, 1, 0x60, 0x2580, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)]


def rnd_blockctor(r):
    return case([op(K_BLOCKNEW, r.randrange(4), r.getrandbits(r.choice([4, 16, 32]))), op(K_BLOCKINFO, 0, 0), op(K_BLOCKINFO, 1, 0), op(K_BLOCKINFO, 2, 0), op(K_BLOCKINFO, 3, 0)])


def edge_blockdtor():
    cases = []
    for f in FLAGS_DEL:
        cases.append(case([op(K_BLOCKNEW, 0, 96), op(K_BLOCKDEL, 0, f), op(K_BLOCKINFO, 0, 0)]))
        cases.append(case([op(K_BLOCKNEW, 1, 96), op(K_BLOCKNEW, 2, 96), op(K_BLOCKSETNEXT, 1, 3), op(K_BLOCKDEL, 1, f), op(K_BLOCKINFO, 2, 0)]))
    return cases


def rnd_blockdtor(r):
    ops = [op(K_BLOCKNEW, i, r.getrandbits(8)) for i in range(r.randrange(1, 4))]
    for _ in range(r.randrange(1, 4)):
        ops.append(op(K_BLOCKDEL, r.randrange(4), r.choice(FLAGS_DEL + [r.getrandbits(32)])))
    return case(ops)


def edge_blockcreate():
    cases = []
    for count in (0, 1, 2, 100, 101, 1000):
        for outnull in (0, 1):
            cases.append(case([op(K_BLOCKCREATE, 0, count, outnull), op(K_BLOCKINFO, 0, 1), op(K_BLOCKGETNEXT, 0)]))
        cases.append(case([op(K_FAILNEXT), op(K_BLOCKCREATE, 0, count, 0), op(K_BLOCKINFO, 0, 1), op(K_BLOCKCREATE, 1, count, 0), op(K_BLOCKINFO, 1, 1)]))
    return cases


def rnd_blockcreate(r):
    ops = []
    for i in range(r.randrange(1, 4)):
        if r.random() < 0.25:
            ops.append(op(K_FAILNEXT))
        ops.append(op(K_BLOCKCREATE, i, r.choice([0, 1, 7, 100, r.randrange(300)]), r.random() < 0.2))
        ops.append(op(K_BLOCKINFO, i, 1))
    return case(ops)


def edge_process():
    cases = []
    # one placeholder of each flag combination, each queue answer
    for flags in (0, 1, 2, 3, 0x10, 0x11, 0x40, 0x100, 0x140, 0x142, 0x400, 0x410, 0x42, 0x143, 0x1FF):
        for end, pause, clock in ((0, 0, 1000), (500, 0, 1000), (999, 0, 1000), (1000, 0, 1000), (900, 100, 1000), (1000, 1, 1001), (0, 0, 0)):
            for vc, ppp in ((1, 1), (0, 1), (1, 0)):
                cases.append(case([new(12), create(0), setslot(USED(0), 0, flags), setslot(USED(0), 4, end), setslot(USED(0), 5, pause), setslot(USED(0), 2, 5),
                                   op(K_PROCESS), snap(1)], q={"Clock": clock, "VC": vc, "PPP": ppp}))
    # the priority list gets several; the channel limit cuts it
    for ch in (1, 2, 3, 12):
        ops = [new(ch), op(K_INITPRIO)] + [create(0)] * 5
        for k in range(5):
            ops += [setslot(USED(k), 2, (k * 7 % 5 + 1) * 0x10001), setslot(USED(k), 3, k + 1), setslot(USED(k), 4, 0xFFFFFFF0)]
        ops += [op(K_PROCESS), snap(1), op(K_PROCESS), snap(1)]
        cases.append(case(ops, q={"Clock": 1000}))
    cases.append(case([new(12), op(K_PROCESS), snap(1)]))
    return cases


def rnd_process(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 3, 4, 6, 9, 15, 35]), ch=r.choice([1, 2, 3, 5, 12, 30]))
    ops += steer(r, used)
    for _ in range(r.randrange(1, 3)):
        ops += [op(K_PROCESS), snap(1)]
    q = {"Clock": [r.choice([0, 500, 1000, 1001, 5000, 0xFFFFFFFF]) for _ in range(3)], "VC": qlist(r, used, [0, 1, 1, 1]),
         "PPP": qlist(r, used, [0, 1, 1, 1]), "Play": qlist(r, used, [0, -1, 5]), "Stop": qlist(r, used, [0, -1])}
    return case(ops, q=q)


def edge_remove():
    cases = []
    for n in (1, 2, 3, 5):
        for k in range(n):
            cases.append(case([new(12)] + [create(0)] * n + [op(K_REMOVE, USED(k)), snap()]))
            cases.append(case([new(12)] + [create(0)] * n + [op(K_REMOVE, USEDEND(k)), snap()]))
    cases.append(case([new(12)] + [create(0)] * 3 + [op(K_REMOVE, USED(0)), op(K_REMOVE, USED(0)), op(K_REMOVE, USED(0)), snap()]))
    return cases


def rnd_remove(r):
    ops, used = prefix(r)
    for _ in range(r.randrange(1, 4)):
        if used:
            ops.append(op(K_REMOVE, r.choice([USED(r.randrange(used)), USEDEND(r.randrange(used))])))
            used -= 1
    return case(ops + [snap()])


def edge_setbound():
    cases = []
    for kind in ("used", "gen", "low", "zero", "stale"):
        for rect in ((0, 0, 0, 0), (1, 2, 3, 4), (-5, 7, 0x7FFFFFFF, -0x80000000)):
            h = {"used": H_USED(0), "gen": H_GEN(0), "low": H_LOW(0), "zero": H_LIT(0), "stale": H_PLUS1(0)}[kind]
            cases.append(case([new(12), create(0), create(0), op(K_SETBOUND,) + h + tuple(v & MASK for v in rect), snap()]))
    cases.append(case([new(12), create(0), op(K_SETBOUND,) + H_USED(0) + (1, 2, 3, 4), op(K_SETBOUND,) + H_USED(0) + (5, 6, 7, 8), snap()]))
    return cases


def rnd_setbound(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 3, 5]))
    ops += steer(r, used)
    for _ in range(r.randrange(1, 4)):
        h = rhandle(r, used, 0, True)
        ops.append(op(K_SETBOUND,) + h + tuple(r.choice([0, 1, -1, 100, r.getrandbits(32)]) & MASK for _ in range(4)))
    return case(ops + [snap()])


def edge_getph():
    cases = []
    for h in (H_USED(0), H_USED(1), H_FREE(0), H_GEN(0), H_LOW(0), H_LIT(0), H_PLUS1(1), H_LIT(0x10001)):
        for sound in (0, 0x77, 0xFFFFFFFF):
            cases.append(case([new(12), create(0), create(0), setslot(USED(0), 1, sound), setslot(USED(1), 1, sound ^ 5), op(K_GETPH,) + h]))
    return cases


def rnd_getph(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 4]))
    ops += steer(r, used)
    for _ in range(r.randrange(1, 4)):
        ops.append(op(K_GETPH,) + rhandle(r, used, 3))
    return case(ops)


def edge_stopall():
    cases = []
    for n in (0, 1, 2, 4):
        for stop in (0, -1):
            cases.append(case([new(12)] + [create(0)] * n + [op(K_STOPALL, 0), snap(1)], q={"Stop": stop}))
    return cases


def rnd_stopall(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 3, 5, 12]))
    ops += steer(r, used)
    ops += [op(K_STOPALL, r.getrandbits(32)), snap(1)]
    return case(ops, q={"Stop": qlist(r, used, [0, -1]), "VC": qlist(r, used, [0, 1]), "PPP": qlist(r, used, [0, 1]), "Clock": [r.choice([0, 1000, 5000])]})


def edge_update():
    cases = []
    for h in (H_USED(0), H_USED(1), H_GEN(0), H_LOW(0), H_LIT(0), H_PLUS1(0), H_PLUSGEN(0)):
        for err in (0, 1, 5, -1, -2, 0x7FFFFFFF, -0x80000000):
            cases.append(case([new(12), create(0), create(0), op(K_UPDATE,) + h], q={"Update": err}))
    return cases


def rnd_update(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 4]))
    for _ in range(r.randrange(1, 5)):
        ops.append(op(K_UPDATE,) + rhandle(r, used, 0, True))
    return case(ops, q={"Update": [r.choice([0, 1, -1, 5, -2, 0x7FFFFFFF, -0x80000000]) for _ in range(5)]})


def edge_updparam():
    cases = []
    for h in (H_USED(0), H_USED(1), H_GEN(0), H_LIT(0), H_PLUS1(0)):
        for pid, value in ((0, 0), (1, 5), (255, 0xFFFFFFFF), (7, 0x80000000)):
            cases.append(case([new(12), create(0), create(0), op(K_UPDPARAM,) + h + (pid, value)]))
    return cases


def rnd_updparam(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 4]))
    for _ in range(r.randrange(1, 5)):
        ops.append(op(K_UPDPARAM,) + rhandle(r, used, 0, True) + (r.getrandbits(8), r.getrandbits(32)))
    return case(ops)


def edge_create():
    cases = []
    # every class: the event type and the flag bits that choose, with the extra bits the choice must ignore
    for typ in (1, 0, 2, 0xFFFFFFFF):
        for flags in range(0, 0x800, 1):
            if flags & ~(4 | 2 | 0x10 | 0x400 | 0x100):
                continue
            cases.append(case([new(12), create(flags, typ), snap()]))
    for extra in (1, 8, 0x20, 0x40, 0x80, 0x200, 0x800, 0x8000, 0xFFFF):
        for flags in CLASS_FLAGS:
            cases.append(case([new(12), create(flags | extra, 0), snap()]))
    cases.append(case([new(12), create(0, 0, 0), create(4, 1, 0), snap()]))
    # a full block: the 101st placeholder makes a second block; a failing block answers 0
    cases.append(case([new(12)] + [create(0)] * 101 + [snap()]))
    cases.append(case([new(12), op(K_FAILNEXT), create(0), snap(), create(0), snap()]))
    cases.append(case([new(12)] + [create(0)] * 100 + [op(K_FAILNEXT), create(0), snap(), create(0), snap()]))
    return cases


def rnd_create(r):
    ops = [new(r.choice([12, 30, 5]))]
    n = r.choice([1, 2, 3, 4, 6, 10]) if r.random() > 0.05 else r.choice([100, 101, 150, 201])
    for i in range(n):
        if r.random() < 0.04:
            ops.append(op(K_FAILNEXT))
        ops.append(create(rflags(r), rtype(r), r.randrange(2)))
        if r.random() < 0.2 and i:
            ops.append(op(K_REMOVE, USED(r.randrange(i))))
    return case(ops + [snap()])


def edge_find():
    cases = []
    for h in (H_USED(0), H_USED(1), H_USED(2), H_FREE(0), H_FREE(5), H_GEN(0), H_PLUS1(0), H_LOW(0), H_PLUSGEN(0), H_LIT(0), H_LIT(1), H_LIT(0xFFFF),
              H_LIT(0x10000), H_LIT(0x7FFFFFFF), H_LIT(0xFFFFFFFF), H_LIT(0x12C), H_LIT(0x1000064), H_LIT(0x30063)):
        cases.append(case([new(12), create(0), create(0), create(0), op(K_FIND,) + h]))
        cases.append(case([new(12)] + [create(0)] * 101 + [op(K_REMOVE, USED(0)), op(K_FIND,) + h, op(K_FIND,) + H_LIT(0x10065), op(K_FIND,) + H_LIT(100), op(K_FIND,) + H_LIT(0x1000064)]))
    cases.append(case([op(K_NEW, 12), op(K_FIND, 0, 0), op(K_FIND, 0, 5)]))
    return cases


def rnd_find(r):
    ops, used = prefix(r)
    for _ in range(r.randrange(1, 6)):
        ops.append(op(K_FIND,) + rhandle(r, used, 3))
    return case(ops)


def edge_newmem():
    cases = [case([new(12)] + [op(K_NEWMEM)] * n + [snap()]) for n in (1, 2, 3, 99, 100, 101, 200, 201)]
    cases.append(case([new(12), op(K_FAILNEXT), op(K_NEWMEM), snap(), op(K_NEWMEM), snap()]))
    cases.append(case([new(12), op(K_POKE, 0, 0xFFFF), op(K_NEWMEM), op(K_NEWMEM), op(K_POKE, 0, 0xFFFFFFFE), op(K_NEWMEM), op(K_NEWMEM), snap()]))
    cases.append(case([new(12)] + [create(0)] * 5 + [op(K_REMOVE, USED(2)), op(K_NEWMEM), op(K_NEWMEM), snap()]))
    return cases


def rnd_newmem(r):
    ops = [new(12)]
    for _ in range(r.choice([1, 2, 3, 5, 8])):
        ops.append(r.choice([op(K_NEWMEM), op(K_NEWMEM), create(0), op(K_FAILNEXT)]))
    return case(ops + [snap()])


def edge_moveused():
    cases = []
    for n in (0, 1, 2, 3):
        for k in range(0, 3):
            cases.append(case([new(12), op(K_ADDBLOCK)] + [create(0)] * n + [op(K_MOVEUSED, FREE(k)), snap()]))
    cases.append(case([new(12), op(K_ADDBLOCK), op(K_MOVEUSED, FREE(99)), snap(), op(K_MOVEUSED, FREE(0)), snap(), op(K_MOVEUSED, FREE(50)), snap()]))
    cases.append(case([new(12), op(K_ADDBLOCK), op(K_MOVEUSED, FREE(0)), op(K_MOVEUSED, FREE(0)), snap()]))
    return cases


def rnd_moveused(r):
    ops = [new(12), op(K_ADDBLOCK)] + [create(0)] * r.choice([0, 1, 5, 20])
    for _ in range(r.randrange(1, 5)):
        ops.append(op(K_MOVEUSED, FREE(r.choice([0, 0, 1, 2, r.randrange(40), 70, 90]))))
    return case(ops + [snap()])


def edge_findfree():
    cases = []
    for value in (0, 1, 98, 99, 100, 101, 0xFFFF, 0x10000, 0x10063, 0x62, 0xFFFFFFFF):
        cases.append(case([new(12), op(K_ADDBLOCK), op(K_FINDFREE, value)]))
        cases.append(case([new(12)] + [create(0)] * 3 + [op(K_REMOVE, USED(0)), op(K_FINDFREE, value), op(K_FINDFREE, 99), op(K_FINDFREE, 98)]))
        cases.append(case([new(12)] + [create(0)] * 101 + [op(K_FINDFREE, value), op(K_FINDFREE, 101), op(K_FINDFREE, 198)]))
    cases.append(case([new(12), op(K_FINDFREE, 0)]))
    return cases


def rnd_findfree(r):
    ops, used = prefix(r, n=r.choice([0, 1, 3, 10, 101]))
    for _ in range(r.randrange(1, 6)):
        ops.append(op(K_FINDFREE, r.choice([r.randrange(210), r.randrange(210) | 0x10000, r.getrandbits(32), 99, 98, 0])))
    return case(ops)


def edge_createhandle():
    cases = []
    for gen in (0, 1, 0xFFFE, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF):
        for stored in (None, 0x63, 0x10063, 0xFFFFFFFF, 0, 0x80000000):
            ops = [new(12), op(K_ADDBLOCK), op(K_POKE, 0, gen)]
            if stored is not None:
                ops.append(setslot(FREE(3), 6, stored))
            ops += [op(K_CREATEHANDLE, FREE(3)), op(K_CREATEHANDLE, FREE(3)), snap()]
            cases.append(case(ops))
    return cases


def rnd_createhandle(r):
    ops = [new(12), op(K_ADDBLOCK), op(K_POKE, 0, r.choice([0, 1, 0xFFFF, 0xFFFFFFFF, r.getrandbits(32)]))]
    for _ in range(r.randrange(1, 5)):
        k = r.randrange(100)
        if r.random() < 0.5:
            ops.append(setslot(FREE(k), 6, r.choice([r.getrandbits(32), r.getrandbits(16), 0])))
        ops.append(op(K_CREATEHANDLE, FREE(k)))
    return case(ops + [snap()])


def edge_newindex():
    cases = [case([new(12), op(K_POKE, 0, g), op(K_NEWHANDLEINDEX), op(K_NEWHANDLEINDEX), snap()]) for g in (0, 1, 0xFFFE, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF)]
    return cases


def rnd_newindex(r):
    return case([new(12), op(K_POKE, 0, r.getrandbits(32))] + [op(K_NEWHANDLEINDEX)] * r.randrange(1, 4) + [snap()])


def edge_addblock():
    cases = [case([new(12)] + [op(K_ADDBLOCK)] * n + [snap()]) for n in (1, 2, 3, 4)]
    cases.append(case([new(12), op(K_FAILNEXT), op(K_ADDBLOCK), snap(), op(K_ADDBLOCK), snap()]))
    cases.append(case([new(12), op(K_ADDBLOCK), op(K_FAILNEXT), op(K_ADDBLOCK), snap()]))
    cases.append(case([new(12), op(K_POKE, 1, 0xFFFFFFC0), op(K_ADDBLOCK), snap()]))
    cases.append(case([new(12), op(K_POKE, 1, 0xFFC0), op(K_ADDBLOCK), op(K_ADDBLOCK), snap()]))
    cases.append(case([new(12), create(0), op(K_ADDBLOCK), op(K_REMOVE, USED(0)), op(K_ADDBLOCK), snap()]))
    return cases


def rnd_addblock(r):
    ops = [new(12)]
    for _ in range(r.randrange(1, 5)):
        ops.append(r.choice([op(K_ADDBLOCK), op(K_ADDBLOCK), op(K_FAILNEXT), create(0), op(K_POKE, 1, r.getrandbits(32))]))
    return case(ops + [snap()])


def uid(n, k):
    """The number of used[k] after n creations in a fresh list, none released (the first block's slots go from index 99 down)."""
    return 101 - n + k


def fid(n, j):
    """The number of free[j] after n creations in a fresh list, none released."""
    return 100 - n - j


def edge_addfree():
    cases = []
    for n in (0, 1, 2, 3):
        for j in (0, 1, 5):
            # a free slot taken off its list goes back at the head
            cases.append(case([new(12), op(K_ADDBLOCK)] + [create(0)] * n + [op(K_UNLINK, FREE(j), 0), snap(), op(K_ADDFREE, ABS(fid(n, j))), snap()]))
        for k in range(n):
            # a used slot taken off the used list goes to the head of the free list
            cases.append(case([new(12)] + [create(0)] * n + [op(K_UNLINK, USED(k), 1), snap(), op(K_ADDFREE, ABS(uid(n, k))), snap()]))
    cases.append(case([new(12), create(0), op(K_UNLINK, FREE(0), 0), op(K_UNLINK, FREE(0), 0), op(K_ADDFREE, ABS(98)), op(K_ADDFREE, ABS(99)), snap()]))
    return cases


def rnd_addfree(r):
    n = r.choice([0, 1, 2, 4, 7])
    ops, used = prefix(r, n=n, big=0.0, remove=False)
    for _ in range(r.randrange(1, 4)):
        if used and r.random() < 0.5:
            k = r.randrange(used)
            ops += [op(K_UNLINK, USED(k), 1), op(K_ADDFREE, ABS(uid(n, k)))]
            n_used_gone = True
            break
        j = r.randrange(0, 100 - n)
        ops += [op(K_UNLINK, FREE(j), 0), op(K_ADDFREE, ABS(fid(n, j)))]
        break
    return case(ops + [snap()])


def edge_addused():
    cases = []
    for n in (0, 1, 2, 3):
        for j in (0, 1, 5):
            cases.append(case([new(12), op(K_ADDBLOCK)] + [create(0)] * n + [op(K_UNLINK, FREE(j), 0), op(K_ADDUSED, ABS(fid(n, j))), snap()]))
        for k in range(n):
            cases.append(case([new(12)] + [create(0)] * n + [op(K_UNLINK, USED(k), 1), op(K_ADDUSED, ABS(uid(n, k))), snap()]))
    return cases


def rnd_addused(r):
    n = r.choice([0, 1, 2, 4, 7])
    ops, used = prefix(r, n=n, big=0.0, remove=False)
    if used and r.random() < 0.4:
        k = r.randrange(used)
        ops += [op(K_UNLINK, USED(k), 1), op(K_ADDUSED, ABS(uid(n, k)))]
    else:
        j = r.randrange(0, 100 - n)
        ops += [op(K_UNLINK, FREE(j), 0), op(K_ADDUSED, ABS(fid(n, j)))]
    return case(ops + [snap()])


def edge_addend():
    cases = []
    for n in (0, 1, 2, 3):
        for j in (0, 1, 5):
            cases.append(case([new(12), op(K_ADDBLOCK)] + [create(0)] * n + [op(K_UNLINK, FREE(j), 2), op(K_ADDEND, ABS(fid(n, j))), snap()]))
        for k in range(n):
            cases.append(case([new(12)] + [create(0)] * n + [op(K_UNLINK, USED(k), 3), op(K_ADDEND, ABS(uid(n, k))), snap()]))
    return cases


def rnd_addend(r):
    n = r.choice([0, 1, 2, 4, 7])
    ops, used = prefix(r, n=n, big=0.0, remove=False)
    if used and r.random() < 0.4:
        k = r.randrange(used)
        ops += [op(K_UNLINK, USED(k), 3), op(K_ADDEND, ABS(uid(n, k)))]
    else:
        j = r.randrange(0, 100 - n)
        ops += [op(K_UNLINK, FREE(j), 2), op(K_ADDEND, ABS(fid(n, j)))]
    return case(ops + [snap()])


def edge_movefree():
    cases = []
    for n in (1, 2, 3, 4):
        for k in range(n):
            cases.append(case([new(12)] + [create(0)] * n + [op(K_MOVEFREE, USED(k)), snap()]))
    cases.append(case([new(12)] + [create(0)] * 3 + [op(K_MOVEFREE, USED(0)), op(K_MOVEFREE, USED(0)), op(K_MOVEFREE, USED(0)), snap()]))
    return cases


def rnd_movefree(r):
    ops, used = prefix(r, n=r.choice([1, 2, 3, 6]), big=0.0)
    for _ in range(r.randrange(1, 4)):
        if used:
            ops.append(op(K_MOVEFREE, r.choice([USED(r.randrange(used)), USEDEND(r.randrange(used))])))
            used -= 1
    return case(ops + [snap()])


def edge_handle2ptr():
    cases = []
    for blocks in (0, 1, 2, 3):
        base = [new(12)] + [op(K_ADDBLOCK)] * blocks
        for value in (0, 1, 98, 99, 100, 101, 198, 199, 200, 201, 299, 300, 301, 0xFFFF, 0x10000, 0x10063, 0x1FFFF, 0xFFFFFFFF, 0x12C, 0x270F):
            cases.append(case(base + [op(K_HANDLE2PTR, 0, value)]))
    return cases


def rnd_handle2ptr(r):
    blocks = r.randrange(0, 4)
    ops = [new(12)] + [op(K_ADDBLOCK)] * blocks
    for _ in range(r.randrange(1, 6)):
        ops.append(op(K_HANDLE2PTR, 0, r.choice([r.randrange(450), r.getrandbits(16), r.getrandbits(32), r.randrange(450) | (r.getrandbits(16) << 16)])))
    return case(ops)


def edge_initprio():
    cases = []
    for n in (0, 1, 3):
        cases.append(case([new(12), op(K_INITPRIO), snap(1)]))
        ops = [new(5), op(K_INITPRIO)] + [create(0)] * n
        for k in range(n):
            ops += [setslot(USED(k), 2, (k + 1) * 0x10001), op(K_ADDHI, USED(k))]
        ops += [snap(1), op(K_INITPRIO), snap(1)]
        cases.append(case(ops))
    cases.append(case([new(12), op(K_POKE, 2, 0xFFFFFFFF), op(K_INITPRIO), snap(1)]))
    return cases


def rnd_initprio(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 4, 8]), ch=r.choice([1, 2, 5, 12]))
    ops += steer(r, used)
    for k in range(used):
        ops.append(op(K_ADDHI, USED(k)))
    ops += [snap(1), op(K_POKE, 2, r.getrandbits(32)), op(K_INITPRIO), snap(1)]
    return case(ops)


def edge_addhi():
    cases = []
    # priority orders and ties, with the channel count cutting the list
    orders = [(5, 5, 5), (1, 2, 3), (3, 2, 1), (2, 3, 2), (0, 0, 1), (1, 1, 0), (0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF), (0x80000000, 0x7FFFFFFF, 0x80000001)]
    for ch in (1, 2, 3, 12, 30):
        for prios in orders:
            ops = [new(ch), op(K_INITPRIO)] + [create(0)] * 3
            for k in range(3):
                ops += [setslot(USED(k), 2, prios[k]), setslot(USED(k), 3, k + 1), op(K_ADDHI, USED(k))]
            cases.append(case(ops + [snap(1)]))
    # the looping duplicate: same sample, looping holder, entry not playing / playing
    for floop in (0, 2):
        for eplay in (0, 1):
            for same in (0, 1):
                ops = [new(3), op(K_INITPRIO)] + [create(0)] * 3
                ops += [setslot(USED(0), 0, eplay), setslot(USED(0), 2, 3), setslot(USED(0), 3, 7), op(K_ADDHI, USED(0))]
                ops += [setslot(USED(1), 0, floop), setslot(USED(1), 2, 9), setslot(USED(1), 3, 7 if same else 8), op(K_ADDHI, USED(1))]
                ops += [setslot(USED(2), 0, floop), setslot(USED(2), 2, 5), setslot(USED(2), 3, 7), op(K_ADDHI, USED(2))]
                cases.append(case(ops + [snap(1)]))
    # a full list and the lowest-priority gate
    ops = [new(2), op(K_INITPRIO)] + [create(0)] * 4
    for k, p in enumerate((9, 8, 7, 10)):
        ops += [setslot(USED(k), 2, p), setslot(USED(k), 3, k), op(K_ADDHI, USED(k)), snap(1)]
    cases.append(case(ops))
    cases.append(case([new(12), op(K_INITPRIO), create(0), setslot(USED(0), 2, 4), op(K_POKE, 2, 4), op(K_ADDHI, USED(0)), snap(1), op(K_POKE, 2, 3), op(K_ADDHI, USED(0)), snap(1)]))
    return cases


def rnd_addhi(r):
    n = r.choice([1, 2, 3, 5, 8, 14, 35])
    ch = r.choice([1, 2, 3, 4, 5, 8, 12, 30])
    ops = [new(ch), op(K_INITPRIO)] + [create(0)] * n
    for k in range(n):
        ops += [setslot(USED(k), 0, r.choice([0, 1, 2, 3, 0x10, 2, 2, r.getrandbits(8)])),
                setslot(USED(k), 2, r.choice([0, 1, 2, 3, 4, 5, 0x10000, 0x10001, 0xFFFFFFFF, r.randrange(6), r.getrandbits(32)])),
                setslot(USED(k), 3, r.randrange(r.choice([2, 3, 6, 40])))]
    order = list(range(n))
    r.shuffle(order)
    for i, k in enumerate(order):
        ops.append(op(K_ADDHI, USED(k)))
        if r.random() < 0.15:
            ops.append(snap(1))
    ops.append(snap(1))
    return case(ops)


def save_file(blocks, entries, count=None, size=0, marker=FIXUP_MARKER):
    """The bytes of a save block as the translation writes them (little-endian words): size, count, the fix-up marker, the block
    count, and per entry the handle, the class id and the flags the fake placeholder writes."""
    body = [size, len(entries) if count is None else count, marker, blocks]
    for handle, cls, flags in entries:
        body += [handle, cls, flags]
    return le(body)


def edge_save():
    cases = []
    for n in (0, 1, 2, 3, 5):
        for variant in range(3):
            ops = [new(12)] + [create(F_LOOP | F_B2 if variant else 0)] * n
            for k in range(n):
                flags = (0, 0x10, 0x400, 0x410, 1, 0x412)[(k + variant) % 6]
                ops.append(setslot(USED(k), 0, flags))
            ops += [op(K_SAVE), snap()]
            cases.append(case(ops, q={"Clock": 1234}))
    cases.append(case([new(12)] + [create(0)] * 101 + [op(K_SAVE), snap()]))
    cases.append(case([new(12), op(K_FILEPOS, 6), op(K_SAVE), op(K_FILEPOS, 0), op(K_SAVE)], q={"Clock": 1}))
    return cases


def rnd_save(r):
    ops, used = prefix(r, n=r.choice([0, 1, 2, 3, 4, 7]))
    for k in range(used):
        ops.append(setslot(USED(k), 0, r.choice([0, 0x10, 0x400, 0x410, 1, 2, 0x412, r.getrandbits(13), 0x8000_0000 | 0x410])))
    if r.random() < 0.2:
        ops.append(op(K_FILEPOS, r.randrange(0, 20)))
    return case(ops + [op(K_SAVE), snap()], q={"Clock": [r.choice([0, 1000, 0xFFFFFFFF])]})


def edge_load():
    cases = []
    # a round trip: save a list, load it into a fresh one (with and without placeholders of its own)
    for n in (0, 1, 3, 6):
        for pre in (0, 2):
            for flag in (1, 0):
                ops = [new(12)] + [create(F_LOOP | F_B2, 0)] * n
                for k in range(n):
                    ops.append(setslot(USED(k), 0, (k * 3 + 1) & 0xFF))
                ops += [op(K_SAVE), op(K_DELETE, 1), op(K_FILEPOS, 0), new(12)] + [create(0)] * pre + [op(K_LOAD, flag), snap(), op(K_DELETE, 1)]
                cases.append(case(ops, q={"Clock": 77}))
    # a crafted file: class ids outside 1..6, handles of used slots, block counts
    for classes in ([1, 2, 3, 4, 5, 6], [0, 7, 8, 0xFFFFFFFF, 0x80000000, 3], [3, 3, 3]):
        entries = [((i * 5 + 1) | (0x10000 * (i + 1)), c, 0x100 + i) for i, c in enumerate(classes)]
        for blocks in (0, 1, 2, 3, 0x90000000, 0xFFFFFFFF):
            cases.append(case([new(12), op(K_LOAD, 1), snap()], filein=save_file(blocks, entries)))
    cases.append(case([new(12), op(K_ADDBLOCK), op(K_ADDBLOCK), op(K_LOAD, 1), snap()], filein=save_file(1, [(5, 3, 7)])))
    cases.append(case([new(12), op(K_LOAD, 0), op(K_LOAD, 0)], filein=save_file(1, [], size=32) + b"\0" * 64))
    for size in (0, 1, 3, 4, 5, 0x20, 0xFFFFFFFC):
        cases.append(case([new(12), create(0), op(K_LOAD, 0), snap()], filein=save_file(1, [], size=size) + b"\1" * 40))
    cases.append(case([new(12), create(0), create(0), op(K_LOAD, 1), snap()], filein=b""))
    # the handle's slot is in use: the record is skipped without reading its data (the stream slips)
    cases.append(case([new(12), op(K_LOAD, 1), snap()], filein=save_file(1, [(99, 3, 5), (99, 3, 6), (98, 2, 7)])))
    cases.append(case([new(12)] + [create(0)] * 2 + [op(K_SAVE), op(K_FILEPOS, 0), op(K_LOAD, 1), snap()], q={"Clock": 5}))
    return cases


def rnd_load(r):
    ops = [new(r.choice([12, 5]))]
    pre = r.choice([0, 0, 1, 2, 4])
    ops += [create(rflags(r), rtype(r)) for _ in range(pre)]
    blocks = r.choice([0, 1, 1, 2, 3, 0x90000000, 0xFFFFFFFF, r.randrange(4)])
    entries = []
    for _ in range(r.choice([0, 1, 2, 3, 6, 12])):
        slot = r.choice([r.randrange(100), r.randrange(100), 99, 98, 0, r.randrange(250)])
        entries.append((slot | (r.choice([0, 1, 5, 0xFFFF]) << 16), r.choice([1, 2, 3, 4, 5, 6, 0, 7, 8, 1, 3, 0xFFFFFFFF, 0x80000001]), r.getrandbits(13)))
    count = None if r.random() > 0.2 else r.choice([0, 1, len(entries) + 1, len(entries) + 3, 40])
    size = r.choice([0, 4, 8, 0x40, 0xFFFFFFFC, r.getrandbits(8)])
    data = save_file(blocks, entries, count, size)
    if r.random() < 0.3:
        data += bytes(r.getrandbits(8) for _ in range(r.randrange(0, 40)))
    ops += [op(K_LOAD, r.choice([1, 1, 1, 0])), snap(), op(K_DELETE, r.choice([1, 0xFFFF])), snap()]
    return case(ops, q={"Clock": [r.choice([0, 100, 0xFFFFFFFF])]}, filein=data)


# the registration order is the address order of the library
for _name, _addr, _edge, _rnd in (
        ("CAudioPlaceHolderList::CAudioPlaceHolderList", 0x1001BB40, edge_ctor, rnd_ctor),
        ("CAudioPlaceHolderList::SetNumberOfChannels", 0x1001BD68, edge_setch, rnd_setch),
        ("CAudioPlaceHolderList::~CAudioPlaceHolderList", 0x1001BE30, edge_dtor, rnd_dtor),
        ("CAudioPlaceHolderList::CMemBlock::GetNext", 0x1001BF2C, edge_getnext, rnd_getnext),
        ("CAudioPlaceHolderList::ProcessPlaceHolders", 0x1001BF78, edge_process, rnd_process),
        ("CAudioPlaceHolderList::RemovePlaceHolder", 0x1001C170, edge_remove, rnd_remove),
        ("CAudioPlaceHolderList::SetBoundingRectangle", 0x1001C23C, edge_setbound, rnd_setbound),
        ("CAudioPlaceHolderList::GetPlaceholderAndSoundHandle", 0x1001C36C, edge_getph, rnd_getph),
        ("CAudioPlaceHolderList::StopAllPlaceHolders", 0x1001C418, edge_stopall, rnd_stopall),
        ("CAudioPlaceHolderList::UpdatePlaceHolder", 0x1001C4C0, edge_update, rnd_update),
        ("CAudioPlaceHolderList::UpdatePlaceHolderParameter", 0x1001C5B4, edge_updparam, rnd_updparam),
        ("CAudioPlaceHolderList::CreatePlaceHolder", 0x1001C678, edge_create, rnd_create),
        ("CAudioPlaceHolderList::FindPlaceHolder", 0x1001C8D8, edge_find, rnd_find),
        ("CAudioPlaceHolderList::NewPlaceHolderMemory", 0x1001C9EC, edge_newmem, rnd_newmem),
        ("CAudioPlaceHolderList::MoveToUsedItemList", 0x1001CAE0, edge_moveused, rnd_moveused),
        ("CAudioPlaceHolderList::FindFreeItem", 0x1001CB8C, edge_findfree, rnd_findfree),
        ("CAudioPlaceHolderList::CreateHandle", 0x1001CC20, edge_createhandle, rnd_createhandle),
        ("CAudioPlaceHolderList::NewHandleIndex", 0x1001CD10, edge_newindex, rnd_newindex),
        ("CAudioPlaceHolderList::AddNewBlock", 0x1001CD60, edge_addblock, rnd_addblock),
        ("CAudioPlaceHolderList::CMemBlock::Data", 0x1001CEAC, edge_data, rnd_data),
        ("CAudioPlaceHolderList::CMemBlock::SetNext", 0x1001CEF4, edge_setnext, rnd_setnext),
        ("CAudioPlaceHolderList::AddToFreeItemList", 0x1001CF60, edge_addfree, rnd_addfree),
        ("CAudioPlaceHolderList::AddToUsedItemList", 0x1001CFDC, edge_addused, rnd_addused),
        ("CAudioPlaceHolderList::AddToEndOfUsedItemList", 0x1001D058, edge_addend, rnd_addend),
        ("CAudioPlaceHolderList::CMemBlock::CreateObject", 0x1001D0F4, edge_blockcreate, rnd_blockcreate),
        ("CAudioPlaceHolderList::CMemBlock::CMemBlock", 0x1001D1F0, edge_blockctor, rnd_blockctor),
        ("CAudioPlaceHolderList::MoveToFreeItemList", 0x1001D270, edge_movefree, rnd_movefree),
        ("CAudioPlaceHolderList::Handle2Ptr", 0x1001D318, edge_handle2ptr, rnd_handle2ptr),
        ("CAudioPlaceHolderList::InitialisePriorityList", 0x1001D3E0, edge_initprio, rnd_initprio),
        ("CAudioPlaceHolderList::AddToHiPriorityList", 0x1001D46C, edge_addhi, rnd_addhi),
        ("CAudioPlaceHolderList::Save", 0x1001D624, edge_save, rnd_save),
        ("CAudioPlaceHolderList::Load", 0x1001DA8C, edge_load, rnd_load),
        ("CAudioPlaceHolderList::CMemBlock::~CMemBlock", 0x1001E694, edge_blockdtor, rnd_blockdtor),
        ("CAudioPlaceHolderList::GetNumberOfChannels", 0x10028950, edge_getch, rnd_getch)):
    register(_name, _addr, _edge, _rnd)
