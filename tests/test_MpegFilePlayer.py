"""Unit w80c, the sound library's MPEG file player, against libtpw_unit_MpegFilePlayer.dylib (tests/unit_MpegFilePlayer.cpp), in
the sound library ("sound symbols shared", Code at 0x10000000, TOC r2 = 0x1003abf0):
  src/sound/CSprocketsModule.cpp, src/sound/LbMpeg.cpp, src/sound/MpegDecode.cpp, src/sound/StreamableMpeg2.cpp  CMpegMacFilePlayerUnpooled: constructor 0x10007dac, destructor 0x10003a30, Open 0x100031e4, GetHeader
                            0x1000339c, DecodeHeader 0x10003420, SeekBits 0x100034d4, NextBuffer 0x100035c4, ReadBuffered
                            0x10003620, RequestBuffer 0x10003868, Close 0x100039a0; SamsUtilities::UPooled<..., 10>: constructor
                            0x10007d1c, destructor 0x100086a4, operator new 0x10007ffc, operator delete 0x10007e20,
                            StaticCleanup::~StaticCleanup 0x10003b98, ClearPool 0x10003c58, IsPoolObjectAvailable 0x10003f40,
                            ReportPoolStatistics 0x10004040, SetPoolKeepFull 0x1002b5ec, MaintainPoolLevel 0x1002b964,
                            MakeNewPoolElement 0x1002bb08; UInterruptSafeQueue<unsigned char[28004]>: constructor 0x100040ac,
                            GetHead 0x10003e08, GetSize 0x1002bab0, Add 0x10007ec0, Remove 0x10003e60, Element::Element
                            0x1002bd2c, GetNext 0x10003da8, GetData 0x10003fe0, GetElementFromDataPointer 0x10007f88;
                            __sinit_LbMpeg_cpp 0x10003acc; SamsUtilities::UDebug 0x10003950 (the platform's definition is the
                            host's: this test checks that the original does nothing, and that RequestBuffer calls it).

The functions run as the original, one script of operations per case. After every operation the player is compared as a "Mac
image" (the 0x6d64 bytes in the Mac's byte order, with the pointers as small codes: 0 null, 1 buffer 0, 2 buffer 1, 3 the cached
data, 4 the output, 5 the frame buffer; the vptr as 1 player, 2 pooled wrapper, 3 fake-derived, 0 destroyed) plus the contents of
both buffers and of the frame buffer, with the result, the output buffer, the cached data, the whole call log in order, and the
pool (the queue's four words, the two statics and the chain of elements as labels with each one's link, type word and player's
ioResult). Every callee outside the unit is a fake on both sides that records the same event: the file read (PBRead: reference,
offset, count, where the buffer is; the PowerPC side's PBReadAsync and PBReadSync stubs read from the same file image, which is
what the host's tpw::ReadFileReference does), BlockMoveData, DebugStr, UDebug, the allocator's Clear, the file's
GetMacFileReference, the heap, and PolySynth and Decode_Layer2 of unit w71b. Enqueue and Dequeue (the Mac OS queue) are modelled
in Python over the emulator's memory and are not recorded.
"""
import random
import struct

import calllog as CL
import test_MpegBase as MB
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_MpegFilePlayer"
MASK = 0xFFFFFFFF
C = MB.C

# the functions
A_PCTOR, A_OPEN, A_GETHEADER, A_DECODEHEADER, A_SEEKBITS, A_NEXTBUFFER = 0x10007DAC, 0x100031E4, 0x1000339C, 0x10003420, 0x100034D4, 0x100035C4
A_READBUFFERED, A_REQUESTBUFFER, A_CLOSE, A_PDTOR, A_SINIT, A_UDEBUG = 0x10003620, 0x10003868, 0x100039A0, 0x10003A30, 0x10003ACC, 0x10003950
A_CLEANUP_DTOR, A_CLEARPOOL, A_GETNEXT, A_GETHEAD, A_REMOVE, A_ISAVAIL = 0x10003B98, 0x10003C58, 0x10003DA8, 0x10003E08, 0x10003E60, 0x10003F40
A_GETDATA, A_REPORT, A_QCTOR, A_KEEPFULL, A_MAINTAIN, A_GETSIZE = 0x10003FE0, 0x10004040, 0x100040AC, 0x1002B5EC, 0x1002B964, 0x1002BAB0
A_MAKE, A_ELCTOR, A_UPCTOR, A_UPNEW, A_UPDEL, A_ADD, A_FROMDATA, A_UPDTOR = 0x1002BB08, 0x1002BD2C, 0x10007D1C, 0x10007FFC, 0x10007E20, 0x10007EC0, 0x10007F88, 0x100086A4
A_MGR = 0x1002BBE4        # UManager<UAllocator>::operator-> (the sound library's own, stubbed: it returns the allocator)
A_RESET, A_READ = 0x10002FA8, 0x1000309C
# the data
GQUEUE, KEEPFULL, TARGET, GUARD_QUEUE, GUARD_CLEANUP, USING_MMX = 0x1003E33C, 0x1003E338, 0x1003943C, 0x1003E329, 0x1003E328, 0x1003E380
VT_PLAYER, VT_POOL = 0x1003940C, 0x1003BE50
OBJ = 0x6D64
BUF = 0x2000
FRAME = 0x800
IMAGE = OBJ + 2 * BUF + FRAME
OUT_SIZE = 0x4000
DATA_SIZE = 0x2100
ELEM = 8           # the Mac element header
ALLOCATOR = 0x7C0F7000    # the fake allocator's address on the PowerPC side
ELEMENT_LABEL = 11

# the parameter block (offsets in the object, widths)
IO = {"qlink": (0x24F0, 4), "qtype": (0x24F4, 2), "iotrap": (0x24F6, 2), "cmd": (0x24F8, 4), "comp": (0x24FC, 4), "result": (0x2500, 2),
      "name": (0x2502, 4), "vref": (0x2506, 2), "refnum": (0x2508, 2), "vers": (0x250A, 1), "perm": (0x250B, 1), "misc": (0x250C, 4),
      "buffer": (0x2510, 4), "reqcount": (0x2514, 4), "actcount": (0x2518, 4), "posmode": (0x251C, 2), "posoffset": (0x251E, 4)}
PL = {"buf0": (0x6534, 4), "buf1": (0x6538, 4), "buf2": (0x653C, 4), "cur": (0x6540, 4), "pos": (0x6544, 4), "frameptr": (0x6D58, 4),
      "file": (0x6D5C, 4), "start": (0x6D60, 4)}
SIGNED = {"result", "refnum", "vref", "qtype", "iotrap", "posmode", "reqcount", "actcount", "posoffset", "cur", "pos", "start", "vers", "perm"}
CODE_OFFSETS = [0x24A8, 0x2510, 0x6534, 0x6538, 0x653C, 0x6D58, 0x6D5C]

(OP_CTOR, OP_DTOR, OP_OPEN, OP_CLOSE, OP_GET_HEADER, OP_DECODE_HEADER, OP_SEEK_BITS, OP_NEXT_BUFFER, OP_READ_BUFFERED, OP_REQUEST_BUFFER,
 OP_READ, OP_RESET, OP_NEW, OP_DELETE, OP_MAINTAIN, OP_KEEP_FULL, OP_CLEAR_POOL, OP_IS_AVAILABLE, OP_REPORT, OP_MAKE, OP_QADD, OP_QREMOVE,
 OP_QHEAD, OP_QSIZE, OP_GET_NEXT, OP_GET_DATA, OP_FROM_DATA, OP_ELEMENT_CTOR, OP_QUEUE_CTOR, OP_PLANT, OP_STATIC_NEW, OP_STATIC_DTOR,
 OP_SINIT, OP_SET_TARGET, OP_NEW_RAW, OP_QPLANT, OP_ELEMENT_TYPE, OP_UDEBUG) = range(38)

STUBS = None
STUBS_REAL_UDEBUG = None
FAKE_VTABLE = None
FILE_VTABLE = None


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------- the images

def base_state(**kw):
    """The CMpegBase fields of a decoder that is open on a layer I mono stream (the 'sane' state of test_MpegBase)."""
    return MB.sane(**kw)


def image(case):
    """The Mac image of the case's initial player, in codes (the same bytes for both sides): the object, buffer 0, buffer 1, the
    frame buffer."""
    fill = case["fill"]
    img = bytearray([fill]) * IMAGE
    if case.get("synth") is not None:
        img[0:0x1200] = case["synth"]
    if case.get("frame") is not None:
        img[0x1240:0x2480] = case["frame"]
    img[0x2480:0x2488] = b"\0" * 8
    state = dict(case["state"])
    state.setdefault("data", 0)
    for name, value in state.items():
        off, width = MB.FIELDS[name]
        img[off:off + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, "big")
    io = dict(refnum=case["ref"], posmode=1, posoffset=0x2000, result=0, actcount=0x2000, reqcount=0x2000, buffer=2)
    io.update(case.get("io", {}))
    pl = dict(buf0=1, buf1=2, buf2=3, cur=2, pos=0, frameptr=5, file=2, start=0)
    pl.update(case.get("pl", {}))
    for table, values in ((IO, io), (PL, pl)):
        for name, value in values.items():
            off, width = table[name]
            img[off:off + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, "big")
    for part, (name, size) in enumerate((("b0", BUF), ("b1", BUF), ("frame_buffer", FRAME))):
        content = case.get(name)
        if content is not None:
            start = OBJ + sum((BUF, BUF)[:part])
            img[start:start + size] = content[:size].ljust(size, b"\0")
    return bytes(img)


def parse(raw):
    """A Mac image (in codes) as a dict of its parts and named fields."""
    d = MB.parse(raw[:0x24F0])
    for table in (IO, PL):
        for name, (off, width) in table.items():
            d[name] = int.from_bytes(raw[off:off + width], "big", signed=name in SIGNED)
    d["b0"] = bytes(raw[OBJ:OBJ + BUF])
    d["b1"] = bytes(raw[OBJ + BUF:OBJ + 2 * BUF])
    d["frame_buffer"] = bytes(raw[OBJ + 2 * BUF:OBJ + 2 * BUF + FRAME])
    return d


# ---------------------------------------------------------------- the PowerPC side

def buffer0(obj):
    return (obj + 0x2522 + 15) & ~15


def frame_buffer(obj):
    return (obj + 0x6548 + 15) & ~15


def where(e, p):
    """Where a pointer points (as unit_MpegFilePlayer.cpp's Where does): kind << 24 | offset."""
    if p == 0:
        return 0
    obj = e.cx["obj"]
    if obj:
        b0, f = buffer0(obj), frame_buffer(obj)
        if b0 <= p < b0 + BUF:
            return (1 << 24) | (p - b0)
        if b0 + BUF <= p < b0 + 2 * BUF:
            return (2 << 24) | (p - b0 - BUF)
        if f <= p < f + FRAME:
            return (5 << 24) | (p - f)
    if e.cx["data"] <= p < e.cx["data"] + DATA_SIZE + 64:
        return (3 << 24) | (p - e.cx["data"])
    if e.cx["out"] <= p < e.cx["out"] + OUT_SIZE:
        return (4 << 24) | (p - e.cx["out"])
    return 7 << 24


def code_of(e, p):
    """A pointer as a code: 0 null, 1 buffer 0, 2 buffer 1, 3 the data, 4 the output, 5 the frame buffer, 0xff other."""
    cx = e.cx
    if p == 0:
        return 0
    if cx["obj"] and not cx.get("freed"):
        if p == buffer0(cx["obj"]):
            return 1
        if p == buffer0(cx["obj"]) + BUF:
            return 2
        if p == frame_buffer(cx["obj"]):
            return 5
    if p == cx["data"]:
        return 3
    if p == cx["out"]:
        return 4
    return 0xFF


def address_of(e, code):
    cx = e.cx
    if code == 1:
        return buffer0(cx["obj"])
    if code == 2:
        return buffer0(cx["obj"]) + BUF
    if code == 3:
        return cx["data"]
    if code == 4:
        return cx["out"]
    if code == 5:
        return frame_buffer(cx["obj"])
    return 0


def plab(L, p):
    """The label of a pool element given the element or its data (1000 + the element's label)."""
    if p == 0:
        return 0
    label = L.lab(p)
    if label > 0:
        return label
    label = L.lab(p - ELEM)
    return 1000 + label if label > 0 else -1


def make_fake_vtables(e):
    """The fake-derived player's vtable (the real player's with GetHeader a fake) and the file's (slot 0x2c a fake)."""
    def get_header(e):
        L = e.cl
        L.rec("GetHeader", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("GH", 0)))

    def mac_file_reference(e):
        L = e.cl
        L.rec("GetMacFileReference", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("REF", e.cx["ref"])))

    words = list(struct.unpack(">7I", e.read(VT_PLAYER, 28)))
    words[5] = T.fake_function(e, get_header)
    fake = T.persist_alloc(e, 0x20, struct.pack(">7I", *words))
    file_words = [0] * 14
    file_words[0x2C // 4] = T.fake_function(e, mac_file_reference)
    return fake, T.persist_alloc(e, 0x40, struct.pack(">14I", *file_words))


def build_stubs(real_udebug=False):
    S = MB.build_stubs()

    def new(e):
        L = e.cl
        size = e.r(3)
        L.rec("new", (size,))
        block = e.alloc(max(size, 16))
        L.newlab(block)
        e.set_r(3, block)

    def delete(e):
        L = e.cl
        block = e.r(3)
        if block != 0 and L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
            L.forget(block)

    def block_move(e):
        L = e.cl
        src, dst, count = e.r(3), e.r(4), s32(e.r(5))
        L.rec("BlockMoveData", (where(e, src), where(e, dst), count))
        if count > 0:
            data = e.read(src, count)
            e.write(dst, data)

    def debug_str(e):
        L = e.cl
        p = e.r(3)
        L.rec("DebugStr", (), (e.read(p + 1, e.read(p, 1)[0]).decode("latin-1"),))

    def udebug(e):
        L = e.cl
        L.rec("UDebug", (), (e.cstr(e.r(3)),))

    def pb_read(sync):
        def stub(e):
            L = e.cl
            block = e.r(3)
            ref = struct.unpack(">h", e.read(block + 0x18, 2))[0]
            offset = struct.unpack(">I", e.read(block + 0x2E, 4))[0]
            buffer = e.u32(block + 0x20)
            req = s32(e.u32(block + 0x24))
            L.rec("PBRead", (ref, offset, req & MASK, where(e, buffer)))
            image_ = e.cx["file"]
            if ref != e.cx["ref"]:
                actual = -1
            elif offset >= len(image_):
                actual = 0
            else:
                actual = min(req & MASK, len(image_) - offset)
            if actual > 0:
                e.write(buffer, image_[offset:offset + actual])
            act = max(actual, 0)
            e.write(block + 0x28, struct.pack(">I", act & MASK))
            e.write(block + 0x2E, struct.pack(">I", (offset + act) & MASK))
            result = -51 if actual < 0 else -39 if actual < req else 0
            e.write(block + 0x10, struct.pack(">h", result))
            e.set_r(3, T.u32(result) if sync else 0)
        return stub

    def enqueue(e):
        element, queue = e.r(3), e.r(4)
        e.write(element, b"\0\0\0\0")
        tail = e.u32(queue + 6)
        if tail == 0:
            e.write(queue + 2, struct.pack(">I", element))
        else:
            e.write(tail, struct.pack(">I", element))
        e.write(queue + 6, struct.pack(">I", element))

    def dequeue(e):
        element, queue = e.r(3), e.r(4)
        previous, node, n = 0, e.u32(queue + 2), 0
        while node != 0 and n < 1000:
            if node == element:
                link = e.u32(node)
                if previous == 0:
                    e.write(queue + 2, struct.pack(">I", link))
                else:
                    e.write(previous, struct.pack(">I", link))
                if e.u32(queue + 6) == node:
                    e.write(queue + 6, struct.pack(">I", previous))
                e.set_r(3, 0)
                return
            previous, node, n = node, e.u32(node), n + 1
        e.set_r(3, MASK)     # qErr (-1)

    def clear(e):
        L = e.cl
        allocator, dest, size = e.r(3), e.r(4), e.r(5)
        L.rec("Clear", (9 if allocator == ALLOCATOR else -1, plab(L, dest), size))
        e.write(dest, b"\0" * size)

    def register(e):
        L = e.cl
        which = {0x1003E370: 1, 0x1003E360: 2, 0x1003E32A: 3}.get(e.r(3), 0)
        L.rec("register", (which,))

    S.imp("__nw__FUl", new)
    S.imp("__dl__FPv", delete)
    S.imp("BlockMoveData", block_move)
    S.imp("DebugStr", debug_str)
    S.imp("PBReadAsync", pb_read(False))
    S.imp("PBReadSync", pb_read(True))
    S.imp("Enqueue", enqueue)
    S.imp("Dequeue", dequeue)
    S.imp("Clear__Q213SamsUtilities10UAllocatorFPvUl", clear)
    S.imp("__register_global_object", register)
    S.internal(A_MGR, lambda e: e.set_r(3, ALLOCATOR))
    if real_udebug:
        S.real.add(A_UDEBUG)
    else:
        S.internal(A_UDEBUG, udebug)
    return S


PORT_FIX = False


def setup(runner):
    global STUBS, STUBS_REAL_UDEBUG, FAKE_VTABLE, FILE_VTABLE, PORT_FIX
    runner.lib.tpw_mf_portfix.restype, runner.lib.tpw_mf_portfix.argtypes = C.c_int32, []
    PORT_FIX = runner.lib.tpw_mf_portfix() == 1
    T.install_std_stubs(runner.emu)
    FAKE_VTABLE, FILE_VTABLE = make_fake_vtables(runner.emu)
    STUBS = build_stubs()
    STUBS_REAL_UDEBUG = build_stubs(real_udebug=True)


def ppc_snapshot(e, L):
    """The player's Mac image with the pointers turned into codes."""
    cx = e.cx
    obj = cx["obj"]
    if obj == 0 or cx.get("freed"):
        return None
    raw = bytearray(e.read(obj, OBJ)) + bytearray(e.read(buffer0(obj), BUF)) + bytearray(e.read(buffer0(obj) + BUF, BUF)) + bytearray(e.read(frame_buffer(obj), FRAME))
    vptr = struct.unpack_from(">I", raw, 0x2480)[0]
    label = 0 if cx.get("gone") else {VT_PLAYER: 1, VT_POOL: 2, FAKE_VTABLE: 3}.get(vptr, 9)
    raw[0x2480:0x2484] = struct.pack(">I", label)
    link = struct.unpack_from(">I", raw, 0x2484)[0]
    raw[0x2484:0x2488] = struct.pack(">I", L.lab(link) & MASK)
    for off in CODE_OFFSETS[:-1]:
        raw[off:off + 4] = struct.pack(">I", code_of(e, struct.unpack_from(">I", raw, off)[0]))
    file_ptr = struct.unpack_from(">I", raw, 0x6D5C)[0]
    raw[0x6D5C:0x6D60] = struct.pack(">I", 0 if file_ptr == 0 else 2 if file_ptr == cx["fileobj"] else 0xFF)
    return parse(bytes(raw))


def ppc_pool(e, L):
    q = lambda off, n: struct.unpack(">I", e.read(GQUEUE + off, n))[0] if n == 4 else struct.unpack(">H", e.read(GQUEUE + off, 2))[0]
    head, tail = q(2, 4), q(6, 4)
    state = [q(0, 2), plab(L, head), plab(L, tail), s32(q(0xC, 4)), e.read(KEEPFULL, 1)[0], e.u32(TARGET)]
    node, n = head, 0
    while node != 0 and n < 40:
        state += [plab(L, node), plab(L, e.u32(node)), e.u32(node + 4), struct.unpack(">h", e.read(node + ELEM + 0x2500, 2))[0]]
        node, n = e.u32(node), n + 1
    state.append(-9)
    return state


def ppc_op(e, L, ctx, op):
    """One operation on the PowerPC side; returns the result as the comparison wants it."""
    kind, a, b, c = (tuple(op) + (0, 0, 0))[:4]
    obj = ctx["obj"]
    call = lambda addr, args: T.call_ppc(e, addr, args)[0]
    elem = lambda slot: ctx["elems"][slot]
    if kind == OP_CTOR:
        e.cx["gone"] = False
        e.cx["freed"] = False
        call(A_UPCTOR if ctx["mode"] == 2 else A_PCTOR, [obj])
        return 0
    if kind == OP_DTOR:
        flag = a & MASK
        call(A_UPDTOR if ctx["mode"] == 2 else A_PDTOR, [obj, flag])
        e.cx["gone"] = True
        if a > 0 and ctx["mode"] != 2:
            e.cx["freed"] = True
        return 0
    if kind == OP_OPEN:
        data = 0 if c < 0 else ctx["data"] + c
        return s32(call(A_OPEN, [obj, ctx["fileobj"], a & MASK, b & MASK, data]))
    if kind == OP_CLOSE:
        return s32(call(A_CLOSE, [obj]))
    if kind == OP_GET_HEADER:
        return s32(call(A_GETHEADER, [obj]))
    if kind == OP_DECODE_HEADER:
        return s32(call(A_DECODEHEADER, [obj]))
    if kind == OP_SEEK_BITS:
        call(A_SEEKBITS, [obj, a & MASK])
        return 0
    if kind == OP_NEXT_BUFFER:
        return s32(call(A_NEXTBUFFER, [obj]))
    if kind == OP_READ_BUFFERED:
        return call(A_READBUFFERED, [obj, 0 if a < 0 else ctx["out"] + a, b & MASK]) & MASK
    if kind == OP_REQUEST_BUFFER:
        call(A_REQUESTBUFFER, [obj, a & MASK, 1 if b else 0])
        return 0
    if kind == OP_READ:
        return s32(call(A_READ, [obj, 0 if a < 0 else ctx["out"] + a, b & MASK]))
    if kind == OP_RESET:
        call(A_RESET, [obj])
        return 0
    if kind == OP_NEW:
        block = call(A_UPNEW, [b & MASK])
        if block == 0 and PORT_FIX and b & MASK == OBJ:
            # The port's fixes (tpw-port issue #50): with no idle player, a new element (the Mac's own MakeNewPoolElement), its
            # player handed out off the queue.
            made = call(A_MAKE, [])
            block = made + ELEM if made else 0
        ctx["blocks"][a] = block
        return plab(L, block)
    if kind == OP_NEW_RAW:
        L.rec("new", (b & MASK,))
        block = e.alloc(max(b, 16))
        e.write(block, bytes([c & 0xFF]) * b)
        L.newlab(block)
        ctx["blocks"][a] = block
        return plab(L, block)
    if kind == OP_DELETE:
        call(A_UPDEL, [ctx["blocks"][a], b & MASK])
        if c:
            ctx["blocks"][a] = 0
        return 0
    if kind == OP_MAINTAIN:
        call(A_MAINTAIN, [])
        return 0
    if kind == OP_KEEP_FULL:
        call(A_KEEPFULL, [a & MASK])
        return 0
    if kind == OP_CLEAR_POOL:
        call(A_CLEARPOOL, [a & MASK])
        return 0
    if kind == OP_IS_AVAILABLE:
        return call(A_ISAVAIL, [elem(a) + ELEM]) & MASK
    if kind == OP_REPORT:
        call(A_REPORT, [])
        return 0
    if kind == OP_MAKE:
        ctx["elems"][a] = call(A_MAKE, [])
        return plab(L, ctx["elems"][a])
    if kind == OP_QADD:
        call(A_ADD, [GQUEUE, elem(a)])
        return 0
    if kind == OP_QREMOVE:
        return call(A_REMOVE, [GQUEUE, elem(a)]) & 0xFF
    if kind == OP_QHEAD:
        return plab(L, call(A_GETHEAD, [GQUEUE]))
    if kind == OP_QSIZE:
        return s32(call(A_GETSIZE, [GQUEUE]))
    if kind == OP_GET_NEXT:
        return plab(L, call(A_GETNEXT, [elem(a)]))
    if kind == OP_GET_DATA:
        return plab(L, call(A_GETDATA, [elem(a)]))
    if kind == OP_FROM_DATA:
        return plab(L, call(A_FROMDATA, [elem(a) + ELEM]))
    if kind == OP_ELEMENT_CTOR:
        call(A_ELCTOR, [ctx["blocks"][a] if b else elem(a)])
        return 0
    if kind == OP_QUEUE_CTOR:
        call(A_QCTOR, [GQUEUE])
        return 0
    if kind == OP_PLANT:
        e.write(elem(a) + ELEM + 0x2500, struct.pack(">h", short(b)))
        return 0
    if kind == OP_STATIC_NEW:
        block = e.alloc(16)
        L.rec("new", (1,))
        L.newlab(block)
        ctx["static"] = block
        return L.lab(block)
    if kind == OP_STATIC_DTOR:
        call(A_CLEANUP_DTOR, [ctx["static"], a & MASK])
        return 0
    if kind == OP_SINIT:
        e.write(GUARD_QUEUE, bytes([a & 0xFF]))
        e.write(GUARD_CLEANUP, bytes([b & 0xFF]))
        call(A_SINIT, [])
        return (e.read(GUARD_QUEUE, 1)[0] << 1) | e.read(GUARD_CLEANUP, 1)[0]
    if kind == OP_SET_TARGET:
        e.write(TARGET, struct.pack(">I", a & MASK))
        return 0
    if kind == OP_QPLANT:
        e.write(GQUEUE, struct.pack(">H", a & 0xFFFF))
        e.write(GQUEUE + 2, struct.pack(">I", 0 if b < 0 else elem(b)))
        e.write(GQUEUE + 6, struct.pack(">I", 0 if c < 0 else elem(c)))
        e.write(GQUEUE + 0xC, struct.pack(">I", 0x1234))
        return 0
    if kind == OP_ELEMENT_TYPE:
        return e.u32((ctx["blocks"][a] if b else elem(a)) + 4)
    if kind == OP_UDEBUG:
        text = e.alloc(64, b"hello \x00")
        call(A_UDEBUG, [text])
        return 0
    raise ValueError(kind)


def ppc_case(e, case):
    e.heap_in_scratch = True
    mode = case["mode"]
    fill = case["fill"]
    data = e.alloc(DATA_SIZE + 64, case.get("data", b"").ljust(DATA_SIZE + 64, b"\0")[:DATA_SIZE + 64])
    out = e.alloc(OUT_SIZE, bytes([case.get("outfill", 0)]) * OUT_SIZE)
    fileobj = e.alloc(16, struct.pack(">I", FILE_VTABLE) + b"\0" * 12)
    obj, elem = 0, 0
    if mode in (0, 1):
        obj = e.alloc(OBJ + 16, bytes([fill]) * (OBJ + 16))
    elif mode == 2:
        elem = e.alloc(ELEM + OBJ + 16, b"\0" * ELEM + bytes([fill]) * (OBJ + 16))
        obj = elem + ELEM
    fixed = {}
    if obj:
        fixed[obj] = 1
    if elem:
        fixed[elem] = ELEMENT_LABEL
    fixed[fileobj] = 2
    L = CL.PpcLog(fixed, case.get("q"))
    L.poly_calls = 0
    L.l2_calls = 0
    e.cl = L
    e.out_base, e.obj_base = out, obj
    e.cx = dict(obj=obj, data=data, out=out, fileobj=fileobj, file=case.get("file", b""), ref=case["ref"], mode=mode)
    ctx = dict(obj=obj, data=data, out=out, fileobj=fileobj, mode=mode, blocks=[0] * 16, elems=[0] * 16, static=0)
    saved = (STUBS_REAL_UDEBUG if case.get("udebug_real") else STUBS).activate(e)
    try:
        if obj:
            img = bytearray(image(case))
            for off in CODE_OFFSETS[:-1]:
                img[off:off + 4] = struct.pack(">I", address_of(e, struct.unpack_from(">I", img, off)[0]))
            if struct.unpack_from(">I", img, 0x6D5C)[0] == 2:
                img[0x6D5C:0x6D60] = struct.pack(">I", fileobj)
            else:
                img[0x6D5C:0x6D60] = b"\0\0\0\0"
            vptr = {0: VT_PLAYER, 1: FAKE_VTABLE, 2: VT_POOL}[mode]
            e.write(obj, bytes(img[:OBJ]))
            e.write(obj + 0x2480, struct.pack(">II", vptr, 0))
            e.write(buffer0(obj), bytes(img[OBJ:OBJ + BUF]))
            e.write(buffer0(obj) + BUF, bytes(img[OBJ + BUF:OBJ + 2 * BUF]))
            e.write(frame_buffer(obj), bytes(img[OBJ + 2 * BUF:IMAGE]))
        pool = case.get("pool", {})
        e.write(GQUEUE, b"\0" * 16)
        e.write(KEEPFULL, bytes([pool.get("keepfull", 0)]))
        e.write(TARGET, struct.pack(">I", pool.get("target", 10) & MASK))
        e.write(USING_MMX, struct.pack(">I", case.get("mmx", 0)))
        e.write(MB.D_SAMPLES, b"\0" * 0x300)
        steps = []
        for op in case["ops"]:
            try:
                result = ppc_op(e, L, ctx, op)
            except RuntimeError as ex:
                raise RuntimeError(f"{ex} in op {op} of {case}") from None
            steps.append(dict(result=result, obj=ppc_snapshot(e, L), out=e.read(out, OUT_SIZE), data=e.read(data, DATA_SIZE),
                              pool=ppc_pool(e, L), mmx=s32(e.u32(USING_MMX))))
        return dict(steps=steps, log=L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


# ---------------------------------------------------------------- the host side

def bind(lib):
    lib.tpw_mf_begin.argtypes = [C.c_int, C.c_int, C.c_int, C.c_int]
    lib.tpw_mf_object.restype = C.c_void_p
    lib.tpw_mf_element.restype = C.c_void_p
    lib.tpw_mf_file_object.restype = C.c_void_p
    lib.tpw_mf_set_file.argtypes = [C.c_char_p, C.c_int]
    lib.tpw_mf_set_data.argtypes = [C.c_char_p, C.c_int]
    lib.tpw_mf_load.argtypes = [C.c_char_p]
    lib.tpw_mf_snapshot.argtypes = [C.c_void_p, C.c_int]
    lib.tpw_mf_get_out.argtypes = [C.c_void_p, C.c_int]
    lib.tpw_mf_get_data.argtypes = [C.c_void_p]
    lib.tpw_mf_pool_state.argtypes = [C.c_void_p]
    lib.tpw_mf_set_pool.argtypes = [C.c_int, C.c_uint32]
    lib.tpw_mf_op.restype = C.c_int64
    lib.tpw_mf_op.argtypes = [C.c_int, C.c_int64, C.c_int64, C.c_int64]
    lib.tpw_mf_get_mmx.restype = C.c_int32


def host_result(op, value):
    kind = op[0]
    if kind in (OP_OPEN, OP_CLOSE, OP_GET_HEADER, OP_DECODE_HEADER, OP_NEXT_BUFFER, OP_READ, OP_QSIZE):
        return s32(value & MASK)
    if kind in (OP_READ_BUFFERED, OP_ELEMENT_TYPE, OP_IS_AVAILABLE):
        return value & MASK
    if kind == OP_QREMOVE:
        return value & 0xFF
    return value


def host_case(lib, case):
    bind(lib)
    mode = case["mode"]
    lib.tpw_mf_begin(mode, case["fill"], case.get("outfill", 0), case["ref"])
    file = case.get("file", b"")
    lib.tpw_mf_set_file(file, len(file))
    data = case.get("data", b"")
    lib.tpw_mf_set_data(data, len(data))
    if mode != 3:
        lib.tpw_mf_load(image(case))
    pool = case.get("pool", {})
    lib.tpw_mf_set_pool(pool.get("keepfull", 0), pool.get("target", 10) & MASK)
    C.c_int32.in_dll(lib, "using_MMX").value = case.get("mmx", 0)
    fixed = {}
    if mode != 3:
        fixed[1] = lib.tpw_mf_object()
    if mode == 2:
        fixed[ELEMENT_LABEL] = lib.tpw_mf_element()
    fixed[2] = lib.tpw_mf_file_object()
    CL.host_begin(lib, fixed, case.get("q"))
    steps = []
    out_buffer = (C.c_uint8 * OUT_SIZE)()
    data_buffer = (C.c_uint8 * DATA_SIZE)()
    pool_state = (C.c_int64 * 200)()
    freed = False
    try:
        for op in case["ops"]:
            kind, a, b, c = (tuple(op) + (0, 0, 0))[:4]
            value = lib.tpw_mf_op(kind, a, b, c)
            if kind == OP_DTOR and a > 0 and mode != 2:
                freed = True
            if kind == OP_CTOR:
                freed = False
            obj = None
            if mode != 3 and not freed:
                image_buffer = (C.c_uint8 * IMAGE)()
                lib.tpw_mf_snapshot(image_buffer, case["fill"])
                obj = parse(bytes(image_buffer))
            lib.tpw_mf_get_out(out_buffer, 1 if case.get("shorts") else 0)
            lib.tpw_mf_get_data(data_buffer)
            lib.tpw_mf_pool_state(pool_state)
            chain = []
            for v in pool_state[6:]:
                chain.append(v)
                if v == -9:
                    break
            steps.append(dict(result=host_result(op, value), obj=obj, out=bytes(out_buffer), data=bytes(data_buffer),
                              pool=list(pool_state[0:6]) + chain, mmx=lib.tpw_mf_get_mmx()))
    finally:
        log = CL.host_end(lib)
        lib.tpw_mf_end()
    return dict(steps=steps, log=log)


def run_ppc(emu, case):
    return ppc_case(emu, case)


# ---------------------------------------------------------------- the cases

FILLS = MB.FILLS
rbytes = MB.rbytes


def short(v):
    return ((v & 0xFFFF) ^ 0x8000) - 0x8000


def mk(ops, mode=0, state=None, **kw):
    c = dict(mode=mode, fill=0xA5, outfill=0x3C, ref=7, file=b"", data=b"", state=base_state(open=0) if state is None else state,
             ops=list(ops), q={}, pool={}, mmx=0)
    c.update(kw)
    return c


def rfill(r):
    return r.choice(FILLS)


def stream_of(r, n, **kw):
    return MB.stream_of(r, n, **kw)


# the layers a stream can have: (name, header keywords, frames); a layer II frame is about 700 bytes, a layer I one 136
KINDS = [("l1 mono", dict(layer=1, id_=0, bri=4, mode=3)), ("l1 stereo", dict(layer=1, id_=0, bri=8, mode=0)),
         ("l2 mono", dict(layer=2, id_=0, bri=9, mode=3)), ("l2 stereo", dict(layer=2, id_=0, bri=11, mode=0)),
         ("l1 mpeg1", dict(layer=1, id_=1, bri=4, mode=3, sri=1))]


def layout(stream, start=0, tail=b"", junk=b"\x5a"):
    """The bank file and the cached data of a sample whose bytes are `stream` and which starts at `start` in the file."""
    file = (junk * start)[:start] + stream + tail
    return dict(file=file, data=stream[:0x2000])


def sample_case(r, kind, frames, start, reads, tail=b"", ref=7, q=None, **kw):
    """Open a sample of `frames` frames at `start`, then Read the counts in `reads`."""
    stream = stream_of(r, frames, **dict(KINDS)[kind])
    ops = [(OP_OPEN, start, len(stream), 0)]
    for n in reads:
        ops.append((OP_READ, 0, n))
    queues = dict(L2=[0])
    queues.update(q or {})
    return mk(ops, ref=ref, shorts=True, q=queues, **layout(stream, start, tail), **kw)


# ---- the constructors and destructors

def ctor_edges():
    for mode in (0, 2):
        for fill in FILLS:
            yield mk([(OP_CTOR,)], mode=mode, fill=fill, state={"open": 1})
            yield mk([(OP_CTOR,)], mode=mode, fill=fill, state={"open": 0xFF, "frames": 5})


def ctor_random(r):
    return mk([(OP_CTOR,)], mode=r.choice([0, 2]), state=MB.rand_state(r, [n for n in MB.FIELDS if r.random() < 0.3] + ["open"]),
              fill=rfill(r), synth=rbytes(r, 0x1200), frame=rbytes(r, 0x1240))


def dtor_edges():
    for mode in (0, 1, 2):
        for open_ in (0, 1, 2, 0xFF):
            for flag in (1, 0, -1, 2, 0x7FFF, -0x8000):
                yield mk([(OP_DTOR, flag)], mode=mode, state=base_state(open=open_))
    for mode in (0, 2):
        yield mk([(OP_CTOR,), (OP_DTOR, 0), (OP_CTOR,), (OP_DTOR, 1)], mode=mode, state=base_state(open=1))
    yield mk([(OP_DTOR, 1), (OP_NEW, 0, 0x6D64), (OP_DELETE, 0, 0x6D64, 1)], mode=2)


def dtor_random(r):
    mode = r.choice([0, 1, 2])
    return mk([(OP_DTOR, short(r.choice([1, 0, -1, 2, r.getrandbits(16)])))], mode=mode,
              state=base_state(open=r.choice([0, 1, 2, 0xFF, r.getrandbits(8)])), fill=rfill(r))


# ---- Open, Close and the whole player: a sample read through the file

def open_edges():
    r = random.Random(801)
    for kind, kw in KINDS:
        for frames in (1, 3, 9, 30, 90):
            for start in (0, 0x123, 0x2000):
                yield sample_case(r, kind, frames, start, [0x300, 0x300, 0x900])
    # the pooled wrapper, as the game uses it: opened, read, closed, deleted (back to the pool's queue), taken again
    for kind, kw in KINDS:
        case = sample_case(r, kind, 30, 0x20, [0x300, 0x900, 0x300], mode=2)
        case["ops"] += [(OP_CLOSE,), (OP_DTOR, 1), (OP_QSIZE,), (OP_QHEAD,), (OP_NEW, 0, 0x6D64), (OP_QSIZE,)]
        yield case
    # a file that ends inside the sample, a file with nothing after the cached bytes, a reference the file is not known by
    yield sample_case(r, "l2 mono", 30, 0x40, [0x900] * 6, tail=b"")
    case = sample_case(r, "l2 mono", 30, 0x40, [0x900] * 6)
    case["file"] = case["file"][:0x2100]
    yield case
    case = sample_case(r, "l1 mono", 70, 0, [0x300] * 6)
    case["file"] = case["file"][:0x2000]
    yield case
    yield sample_case(r, "l1 mono", 70, 0, [0x300] * 6, ref=4464, q=dict(REF=[70000]))
    yield sample_case(r, "l1 mono", 70, 0, [0x300] * 6, ref=4464, q=dict(REF=[12]))
    yield sample_case(r, "l1 mono", 70, 0, [0x300] * 6, ref=30000, q=dict(REF=[-35536]))
    yield sample_case(r, "l1 mono", 70, 0, [0x300] * 6, ref=-4096, q=dict(REF=[61440]))
    yield sample_case(r, "l1 mono", 70, 0, [0x300] * 6, ref=-1, q=dict(REF=[-1]))
    # open twice, close and open again
    stream = stream_of(r, 20, **dict(KINDS)["l2 mono"])
    lay = layout(stream, 0x10)
    yield mk([(OP_OPEN, 0x10, len(stream), 0), (OP_READ, 0, 0x900), (OP_OPEN, 0x10, len(stream), 0), (OP_READ, 0, 0x900)], shorts=True,
             q=dict(L2=[0]), **lay)
    yield mk([(OP_OPEN, 0x10, len(stream), 0), (OP_CLOSE,), (OP_CLOSE,), (OP_OPEN, 0x10, len(stream), 0), (OP_READ, 0, 0x900), (OP_CLOSE,)],
             shorts=True, q=dict(L2=[0]), **lay)
    yield mk([(OP_OPEN, 0x10, len(stream), 0x10)], state=base_state(open=1), **lay)
    # sizes and starts at the edge of the arithmetic
    for size in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 5000):
        for start in (0, -0x2000, 0x7FFFE000, 0x7FFFFFFF, -1):
            yield mk([(OP_OPEN, start, size, 0)], **layout(stream, 0x10))
    # the fake-derived player: the header virtual refuses, answers, or takes the stream's first word
    for answers in ([0], [1], [-1], [2], [0x100], [0, 1]):
        yield mk([(OP_OPEN, 0x10, len(stream), 0), (OP_OPEN, 0x10, len(stream), 0)], mode=1, q=dict(GH=answers), **lay)
    yield mk([(OP_OPEN, 0x10, len(stream), 0)], mode=1, state=base_state(open=1), q=dict(GH=[1]), **lay)
    # the layer III and the reserved header, then a sample that is no stream at all
    for kw in (dict(layer=3, id_=0, bri=5), dict(layer=1, id_=0, bri=0), dict(layer=1, id_=0, bri=15), dict(layer=2, id_=1, bri=3, sri=3)):
        s = stream_of(r, 4, **kw)
        yield mk([(OP_OPEN, 0, len(s), 0), (OP_READ, 0, 0x300)], shorts=True, q=dict(L2=[0]), **layout(s, 0))
    yield mk([(OP_OPEN, 0, 100, 0), (OP_READ, 0, 0x300)], shorts=True, **layout(bytes(100), 0))
    yield mk([(OP_OPEN, 0, 100, 0)], **layout(b"", 0))
    # the real streams of the game's banks (read at test time, never copied)
    for tag, streams in MB.sdt().items():
        for data in streams[:2]:
            lay = layout(data, 0x77)
            yield mk([(OP_OPEN, 0x77, len(data), 0), (OP_READ, 0, 0x300), (OP_READ, 0, 0x900), (OP_READ, 0, 0x1200), (OP_READ, 0, 0x1200)],
                     shorts=True, q=dict(L2=[0]), **lay)


def open_random(r):
    kind, kw = r.choice(KINDS)
    frames = r.choice([1, 2, 5, 12, 40, r.randrange(1, 100)])
    stream = stream_of(r, frames, **kw)
    start = r.choice([0, 0, 0x40, r.randrange(0, 0x3000), 0x2000])
    tail = rbytes(r, r.choice([0, 0, 10, 300]))
    lay = layout(stream, start, tail, junk=bytes([r.getrandbits(8)]))
    if r.random() < 0.25:
        lay["file"] = lay["file"][:r.randrange(0, len(lay["file"]) + 1)]
    ops = [(OP_OPEN, start, r.choice([len(stream), len(stream), len(stream) // 2, r.randrange(0, 40000)]), r.choice([0, 0, 0, 0x20]))]
    for _ in range(r.randrange(1, 6)):
        n = r.choice([0, 2, r.randrange(1, 0x400) & ~1, 0x300, 0x600, 0x900, 0x1200, r.randrange(1, 0x2000) & ~1])
        ops.append((OP_READ, r.choice([0, 0, 2, 0x40]), n))
        if r.random() < 0.1:
            ops.append((OP_SEEK_BITS, r.choice([0, 0, 0x100, r.randrange(0, 4000)])))
        if r.random() < 0.08:
            ops.append((OP_RESET,))
    if r.random() < 0.2:
        ops.append(r.choice([(OP_CLOSE,), (OP_OPEN, start, len(stream), 0)]))
    return mk(ops, mode=r.choice([0, 0, 0, 1, 2]), shorts=True, q=dict(L2=[r.choice([0, 0, -1])], GH=[r.choice([0, 0, 1])]),
              state=base_state(open=r.choice([0, 0, 1])), fill=rfill(r), **lay)


def close_edges():
    for open_ in (0, 1, 2, 0xFF, 0x80):
        yield mk([(OP_CLOSE,), (OP_CLOSE,)], state=base_state(open=open_))
    yield mk([(OP_CLOSE,)], mode=1, state=base_state(open=1))


def close_random(r):
    return mk([(OP_CLOSE,)] * r.randrange(1, 3), mode=r.choice([0, 1]), state=base_state(open=r.choice([0, 1, 2, r.getrandbits(8)])), fill=rfill(r))


# ---- the header readers and the buffers, from planted state

def header_stream(r, kw, frames=3, junk=0):
    """`junk` bytes, then frames of the kind `kw`."""
    return rbytes(r, junk) + stream_of(r, frames, **kw)


def buffered_case(r, ops, cur, pos, content, act=0x2000, result=0, file=None, fill=None, **kw):
    """A player part way through its buffers: `content` is the bytes of the current buffer (the cached data when cur is 2), the
    other buffers hold other bytes; the last read got `act` bytes."""
    data = rbytes(r, 0x2100)
    b0, b1 = rbytes(r, BUF_), rbytes(r, BUF_)
    if cur == 2:
        data = content.ljust(0x2100, b"\0")
    elif cur == 0:
        b0 = content.ljust(BUF_, b"\0")
    else:
        b1 = content.ljust(BUF_, b"\0")
    pl = dict(cur=cur, pos=pos)
    pl.update(kw.pop("pl", {}))
    io = dict(actcount=act, result=result)
    io.update(kw.pop("io", {}))
    return mk(ops, fill=rfill(r) if fill is None else fill, data=data, b0=b0, b1=b1, pl=pl, io=io,
              file=rbytes(r, 0x5000) if file is None else file, **kw)


BUF_ = 0x2000


def header_edges():
    r = random.Random(802)
    for ops in ([(OP_GET_HEADER,)], [(OP_DECODE_HEADER,)], [(OP_DECODE_HEADER,)] * 3, [(OP_GET_HEADER,), (OP_DECODE_HEADER,)]):
        for kind, kw in KINDS:
            for cur in (2, 0, 1):
                for pos in (0, 1, 4, 0x100):
                    yield buffered_case(r, ops, cur, pos, header_stream(r, kw, 4, 0))
    # headers and frames that cross the end of the buffer, by every byte of the header and a few of the frame
    for cur in (2, 0, 1):
        for back in (1, 2, 3, 4, 5, 6, 100, 140):
            kw = dict(KINDS)["l1 mono"]
            s = stream_of(r, 3, **kw)
            junk = BUF_ - back
            for act in (0x2000, 0x1FFF, 0, 5, 0x2001):
                yield buffered_case(r, [(OP_DECODE_HEADER,), (OP_DECODE_HEADER,)], cur, 0, bytes(junk) + s, act=act)
                yield buffered_case(r, [(OP_GET_HEADER,), (OP_DECODE_HEADER,)], cur, junk, s + rbytes(r, 50), act=act)
    # no frame header there (DecodeHeader answers -1 and does not read the frame), a frame size of zero, a layer that is not decoded
    for words in (bytes(8), b"\xff\xf0\x00\x00", b"\xff\xff\xff\xff", b"\x00\x00\x00\x00" * 4, stream_of(r, 1, layer=3, id_=0, bri=5)):
        yield buffered_case(r, [(OP_DECODE_HEADER,), (OP_GET_HEADER,), (OP_DECODE_HEADER,)], 2, 0, words)
    # the frame size field left over from a state the code did not set
    for fsize in (0, 1, 0x7FF, 0x800, 0x801, 0x1FFF, 0x2000):
        yield buffered_case(r, [(OP_DECODE_HEADER,)], 2, 0, stream_of(r, 2, layer=1, id_=0, bri=4, mode=3), state=base_state(open=1, fsize=fsize))


def header_random(r):
    kind, kw = r.choice(KINDS)
    junk = r.choice([0, r.randrange(0, 40), BUF_ - r.randrange(1, 200), BUF_ - r.randrange(1, 12)])
    content = bytes(max(junk, 0)) + stream_of(r, r.randrange(1, 4), **kw)
    ops = [r.choice([(OP_GET_HEADER,), (OP_DECODE_HEADER,)]) for _ in range(r.randrange(1, 4))]
    return buffered_case(r, ops, r.choice([2, 0, 1]), r.choice([0, 0, r.randrange(0, 20), r.randrange(0, BUF_)]) if junk < BUF_ - 300 else 0,
                         content, act=r.choice([0x2000, 0x2000, 0, r.randrange(0, 0x2001), 0x2001]), result=r.choice([0, 0, 1, -39]))


def next_edges():
    for cur in (0, 1, 2, 3, -1, -2, 5, 0x7FFFFFFF, -0x80000000, 0x80000000 - 1, 0x100):
        yield mk([(OP_NEXT_BUFFER,)], pl=dict(cur=cur))
    yield mk([(OP_NEXT_BUFFER,), (OP_NEXT_BUFFER,)], pl=dict(cur=0))


def next_random(r):
    return mk([(OP_NEXT_BUFFER,)], pl=dict(cur=r.choice([0, 1, 2, r.getrandbits(32) - (1 << 31)])), fill=rfill(r))


def read_buffered_edges():
    r = random.Random(803)
    for cur in (0, 1, 2):
        for pos in (0, 1, 7, 0x100, 0x1FF0, 0x1FFC, 0x1FFE, 0x1FFF, 0x2000):
            for n in (0, 1, 2, 4, 8, 0x10, 0x100, 0x1FFF, 0x2000):
                if pos + n < 0x2000 or r.random() < 0.5:
                    yield buffered_case(r, [(OP_READ_BUFFERED, 0x10, n)], cur, pos, rbytes(r, BUF_))
    # a crossing read for each count of bytes the last read got (the tail of the new buffer is zeroed from there)
    for act in (0x2000, 0x1FFF, 0x1FF9, 0x1FF8, 0x1FF7, 0x1000, 9, 8, 7, 1, 0, 0x2001, 0x2008, 0x7FFFFFFF):
        for cur in (0, 1, 2):
            yield buffered_case(r, [(OP_READ_BUFFERED, 0x10, 0x40), (OP_READ_BUFFERED, 0x200, 0x10)], cur, 0x1FF0, rbytes(r, BUF_), act=act)
    # reads in a row through both buffers, every buffer index the read could start in
    for cur in (0, 1, 2):
        yield buffered_case(r, [(OP_READ_BUFFERED, 0, 0x700)] * 9, cur, 0, rbytes(r, BUF_), file=rbytes(r, 0x8000))
        yield buffered_case(r, [(OP_READ_BUFFERED, 0, 0x2000)] * 4, cur, 0, rbytes(r, BUF_), file=rbytes(r, 0x8000), io=dict(posoffset=0x2000))
        yield buffered_case(r, [(OP_READ_BUFFERED, 0, 0x2000), (OP_READ_BUFFERED, 0, 1)], cur, 0, rbytes(r, BUF_), result=1)
    # the output pointer and the read in flight
    for result in (0, 1, -39, 5):
        yield buffered_case(r, [(OP_READ_BUFFERED, 0x3FD0, 0x30)], 2, 0x1FF0, rbytes(r, BUF_), result=result)
    # the next read refused because one is in flight (UDebug), then not
    yield buffered_case(r, [(OP_READ_BUFFERED, 0, 0x40)], 0, 0x1FF0, rbytes(r, BUF_), act=0x2000, result=1)


def read_buffered_random(r):
    cur = r.choice([0, 1, 2])
    pos = r.choice([0, r.randrange(0, BUF_), BUF_ - r.randrange(1, 64), BUF_ - 1])
    ops = [(OP_READ_BUFFERED, r.choice([0, 0x10, 0x100]), r.choice([0, 1, 4, r.randrange(0, 300), r.randrange(0, BUF_ - pos + 40) if pos < BUF_ else 3]))
           for _ in range(r.randrange(1, 4))]
    return buffered_case(r, ops, cur, pos, rbytes(r, BUF_), act=r.choice([0x2000, 0x2000, 0, r.randrange(0, 0x2001), 0x2001]),
                         result=r.choice([0, 0, 0, 1, -39]), file=rbytes(r, r.choice([0, 0x2100, 0x6000])))


def request_edges():
    r = random.Random(804)
    for result in (0, 1, 2, 0x7FFF, -1, -39, -51, -0x8000, 0x100):
        for sync in (0, 1):
            for buffer in (0, 1, 2):
                yield buffered_case(r, [(OP_REQUEST_BUFFER, buffer, sync)], 0, 0, rbytes(r, 100), result=result, io=dict(posoffset=0x2000, actcount=0x55))
    # the file: whole buffers, a short one, nothing, past the end, a reference it does not have, a negative offset
    for size in (0, 1, 0x1FFF, 0x2000, 0x2001, 0x3FFF, 0x4000, 0x6000):
        for offset in (0, 0x100, 0x2000, 0x3FFF, 0x4000, 0x7000, -1, -0x2000, 0x7FFFFFFF):
            yield buffered_case(r, [(OP_REQUEST_BUFFER, 0, 0), (OP_REQUEST_BUFFER, 1, 1)], 0, 0, rbytes(r, 100), file=rbytes(r, size),
                                io=dict(posoffset=offset))
    for refnum in (7, 8, 0, -1, 0x7FFF, -0x8000):
        yield buffered_case(r, [(OP_REQUEST_BUFFER, 0, 0), (OP_REQUEST_BUFFER, 1, 1), (OP_REQUEST_BUFFER, 0, 0)], 0, 0, rbytes(r, 100),
                            file=rbytes(r, 0x5000), io=dict(refnum=refnum, posoffset=0x2000), ref=refnum if refnum in (7, 0, -1) else 7)
    # one request after another: the second is refused until the first is complete (the host's is at once)
    yield buffered_case(r, [(OP_REQUEST_BUFFER, 0, 0), (OP_REQUEST_BUFFER, 1, 0), (OP_REQUEST_BUFFER, 2, 0)], 0, 0, rbytes(r, 100), file=rbytes(r, 0x9000),
                        io=dict(posoffset=0x2000))


def request_random(r):
    size = r.choice([0, 0x1000, 0x2000, 0x2100, 0x4000, 0x5000])
    return buffered_case(r, [(OP_REQUEST_BUFFER, r.choice([0, 1, 2]), r.choice([0, 0, 1])) for _ in range(r.randrange(1, 4))], r.choice([0, 1, 2]),
                         r.randrange(0, BUF_), rbytes(r, 100), result=r.choice([0, 0, 0, 1, -39, -51, r.randrange(-100, 100)]),
                         file=rbytes(r, size), io=dict(posoffset=r.choice([0x2000, r.randrange(0, size + 0x3000), -r.randrange(1, 1000)]),
                                                       refnum=r.choice([7, 7, 7, 8]), actcount=r.randrange(0, 0x2001)))


def seek_edges():
    r = random.Random(805)
    for byte in (0, 1, 2, 0x100, 0x1FFF, 0x2000, 0x2001, 0x4000, -1, -0x2000, -0x2001, 0x7FFFFFFF, 0x7FFFE000, 0x7FFFDFFF, -0x80000000):
        for total in (0, 0x1FFF, 0x2000, 0x2001, 0x4000, 0x4001, 0x7FFFFFFF, -1):
            yield buffered_case(r, [(OP_SEEK_BITS, byte)], 1, 5, rbytes(r, 100), state=base_state(open=1, total=total, bitpos=77), pl=dict(start=0x1234),
                                file=rbytes(r, 0x6000), io=dict(posoffset=0x55))
    for start in (0, 0x1234, -0x2000, 0x7FFFE000, 0x7FFFFFFF, -1):
        for byte in (0, 5, -5):
            yield buffered_case(r, [(OP_SEEK_BITS, byte)], 2, 5, rbytes(r, 100), state=base_state(open=1, total=0x9000), pl=dict(start=start),
                                file=rbytes(r, 0x6000))
    # a read in flight (UDebug twice for a non-zero offset), each current buffer
    for result in (0, 1, -39):
        for cur in (0, 1, 2):
            for byte in (0, 0x100):
                yield buffered_case(r, [(OP_SEEK_BITS, byte), (OP_SEEK_BITS, byte)], cur, 5, rbytes(r, 100), result=result,
                                    state=base_state(open=1, total=0x9000), file=rbytes(r, 0x6000))


def seek_random(r):
    return buffered_case(r, [(OP_SEEK_BITS, r.choice([0, 0, r.randrange(1, 0x6000), r.randrange(-100, 100), r.getrandbits(32) - (1 << 31)])) for _ in range(r.randrange(1, 4))],
                         r.choice([0, 1, 2]), r.randrange(0, BUF_), rbytes(r, 100), result=r.choice([0, 0, 1, -39]),
                         state=base_state(open=1, total=r.choice([0, 0x2000, 0x5000, r.getrandbits(32) - (1 << 31)]), bitpos=r.getrandbits(32)),
                         pl=dict(start=r.choice([0, 0x300, r.randrange(0, 0x3000)])), file=rbytes(r, 0x6000))


# ---- the pool and its queue (mode 3: no player, or the player as a pool element)

def make_elements(n, add=True):
    ops = [(OP_MAKE, i) for i in range(n)]
    if add:
        ops += [(OP_QADD, i) for i in range(n)]
    return ops


def pool_case(ops, **kw):
    return mk(ops, mode=kw.pop("mode", 3), **kw)


def new_edges():
    N = (OP_NEW, 0, 0x6D64)
    yield pool_case([N])
    yield pool_case([N, N])
    for size in (0, 1, 2, 0x100, 0x6D63, 0x6D65, 0x6D6C, 0x24F0, 0x10000):
        yield pool_case([(OP_NEW, 0, size), (OP_DELETE, 0, size, 1)])
        yield pool_case(make_elements(2) + [(OP_NEW, 0, size), (OP_DELETE, 0, size, 1), (OP_QSIZE,)])
    for n in (1, 2, 3, 5):
        ops = make_elements(n) + [(OP_NEW, i, 0x6D64) for i in range(n)] + [(OP_NEW, 9, 0x6D64), (OP_QSIZE,), (OP_QHEAD,)]
        yield pool_case(ops)
        yield pool_case(ops + [(OP_DELETE, i, 0x6D64, 1) for i in range(n)] + [(OP_QHEAD,)])
        yield pool_case(ops + [(OP_DELETE, i, 0x6D64, 1) for i in reversed(range(n))] + [(OP_NEW, 8, 0x6D64), (OP_NEW, 7, 0x6D64)])
    # a player in use (a read in flight) is skipped, whatever its place in the queue; a finished one is taken
    for value in (1, 2, 5, 0x7FFF, 0x100):
        for busy in ([0], [1], [2], [0, 1], [0, 1, 2], [1, 2], [0, 2]):
            ops = make_elements(3) + [(OP_PLANT, i, value) for i in busy] + [(OP_NEW, 0, 0x6D64), (OP_QSIZE,), (OP_NEW, 1, 0x6D64), (OP_NEW, 2, 0x6D64), (OP_NEW, 3, 0x6D64)]
            yield pool_case(ops)
    for value in (0, -1, -39, -51, -0x8000):
        yield pool_case(make_elements(3) + [(OP_PLANT, i, value) for i in range(3)] + [(OP_NEW, 0, 0x6D64), (OP_NEW, 1, 0x6D64), (OP_NEW, 2, 0x6D64)])
    # a pooled player deleted by its destructor goes back to the queue, ready for the next new
    yield pool_case([(OP_DTOR, 1), (OP_NEW, 0, 0x6D64), (OP_NEW, 1, 0x6D64)], mode=2)
    yield pool_case([(OP_DTOR, 1), (OP_NEW, 0, 0x6D64), (OP_DELETE, 0, 0x6D64, 1), (OP_QSIZE,)], mode=2)


def new_random(r):
    ops = make_elements(r.randrange(0, 5))
    busy = r.random() < 0.5
    for _ in range(r.randrange(1, 8)):
        pick = r.random()
        if pick < 0.5:
            ops.append((OP_NEW, r.randrange(0, 6), r.choice([0x6D64, 0x6D64, 0x6D64, r.choice([0, 1, 0x6D63, 0x6D65, 0x100])])))
        elif pick < 0.8:
            ops.append((OP_PLANT, r.randrange(0, 4), r.choice([0, 1, 2, -1, -39, 0x7FFF])) if busy else (OP_QSIZE,))
        else:
            ops.append((OP_QHEAD,))
    return pool_case(sanitise(ops), mode=r.choice([3, 3, 3, 2]), fill=rfill(r))


def sanitise(ops):
    """Drops the operations that name an element slot that was not made."""
    made, out = set(), []
    for op in ops:
        if op[0] == OP_MAKE:
            made.add(op[1])
        if op[0] in (OP_PLANT, OP_QADD, OP_QREMOVE, OP_GET_NEXT, OP_GET_DATA, OP_FROM_DATA, OP_IS_AVAILABLE) and op[1] not in made:
            continue
        out.append(op)
    return out


def delete_edges():
    # a player given back goes to the end of the queue; any other size is freed to the heap
    yield pool_case(make_elements(2) + [(OP_NEW, 0, 0x6D64), (OP_DELETE, 0, 0x6D64, 1), (OP_QSIZE,), (OP_QHEAD,)])
    yield pool_case(make_elements(3) + [(OP_NEW, 0, 0x6D64), (OP_NEW, 1, 0x6D64), (OP_DELETE, 1, 0x6D64, 1), (OP_DELETE, 0, 0x6D64, 1), (OP_QHEAD,),
                                        (OP_QSIZE,), (OP_NEW, 2, 0x6D64), (OP_NEW, 3, 0x6D64), (OP_NEW, 4, 0x6D64)])
    yield pool_case([(OP_MAKE, 0), (OP_QADD, 0), (OP_NEW, 0, 0x6D64), (OP_DELETE, 0, 0x6D64, 0), (OP_QSIZE,), (OP_NEW, 1, 0x6D64)])
    for size in (0, 1, 0x6D63, 0x6D65, 0x6D6C, 0x24F0, 0x10000):
        yield pool_case([(OP_NEW_RAW, 0, size, 0xA5), (OP_DELETE, 0, size, 1)])
        yield pool_case(make_elements(2) + [(OP_NEW, 0, size), (OP_DELETE, 0, size, 1), (OP_QSIZE,)])
    # the player deleted by its destructor (the pooled wrapper's operator delete with the player's size)
    for flag in (1, 2, 0x7FFF):
        yield pool_case([(OP_DTOR, flag), (OP_QSIZE,), (OP_QHEAD,), (OP_NEW, 0, 0x6D64), (OP_QSIZE,)], mode=2)


def delete_random(r):
    n = r.randrange(1, 5)
    ops = make_elements(n)
    held = {}
    for _ in range(r.randrange(1, 10)):
        pick = r.random()
        free = [i for i in range(6) if i not in held]
        if (pick < 0.5 or not held) and free:
            slot = r.choice(free)
            size = r.choice([0x6D64, 0x6D64, 0x6D64, r.choice([0, 1, 0x6D63, 0x6D65, 0x100])])
            if size == 0x6D64 and sum(1 for v in held.values() if v == 0x6D64) >= n:
                continue
            ops.append((OP_NEW, slot, size))
            held[slot] = size
        elif held:
            slot = r.choice(sorted(held))
            ops.append((OP_DELETE, slot, held[slot], 1))
            del held[slot]
    ops.append((OP_QSIZE,))
    return pool_case(ops, fill=rfill(r))


def queue_edges():
    # the queue's own functions
    yield pool_case([(OP_QSIZE,), (OP_QHEAD,)])
    yield pool_case([(OP_QPLANT, 0xFFFF, -1, -1), (OP_QUEUE_CTOR,), (OP_QSIZE,), (OP_QHEAD,)])
    yield pool_case(make_elements(2) + [(OP_QPLANT, 0xFFFF, 0, 1), (OP_QUEUE_CTOR,), (OP_QSIZE,), (OP_QHEAD,)])
    for n in (1, 2, 3, 4):
        base = make_elements(n)
        yield pool_case(base + [(OP_QSIZE,), (OP_QHEAD,)] + [(OP_GET_NEXT, i) for i in range(n)] + [(OP_GET_DATA, i) for i in range(n)] +
                        [(OP_FROM_DATA, i) for i in range(n)])
        for victim in range(n):
            yield pool_case(base + [(OP_QREMOVE, victim), (OP_QSIZE,), (OP_QHEAD,), (OP_QREMOVE, victim), (OP_QSIZE,)])
            yield pool_case(base + [(OP_QREMOVE, victim), (OP_QADD, victim), (OP_QSIZE,)])
        yield pool_case(base + [(OP_QREMOVE, i) for i in range(n)] + [(OP_QHEAD,), (OP_QSIZE,), (OP_QADD, 0)])
        yield pool_case(base + [(OP_QREMOVE, i) for i in reversed(range(n))] + [(OP_QHEAD,), (OP_QSIZE,)])
    # a queue whose words disagree (the head empty with a tail, the tail empty with a head): Enqueue goes by the tail
    yield pool_case(make_elements(2, add=False) + [(OP_QPLANT, 0, -1, 0), (OP_QADD, 1), (OP_QHEAD,), (OP_QSIZE,)])
    yield pool_case(make_elements(2, add=False) + [(OP_QPLANT, 0, 0, -1), (OP_QADD, 1), (OP_QHEAD,), (OP_QSIZE,)])
    # removing what is not in the queue, adding what is in another position
    yield pool_case(make_elements(2, add=False) + [(OP_QREMOVE, 0), (OP_QSIZE,), (OP_QADD, 1), (OP_QREMOVE, 0), (OP_QREMOVE, 1), (OP_QSIZE,), (OP_QHEAD,)])
    # the element: its constructor over a dirty block, its type word
    for value in (0xA5, 0x00, 0xFF):
        yield pool_case([(OP_NEW_RAW, 0, 0x6D6C, value), (OP_ELEMENT_TYPE, 0, 1), (OP_ELEMENT_CTOR, 0, 1), (OP_ELEMENT_TYPE, 0, 1)])
    yield pool_case([(OP_MAKE, 0), (OP_MAKE, 1), (OP_QADD, 0), (OP_QADD, 1), (OP_ELEMENT_CTOR, 0), (OP_GET_NEXT, 0), (OP_QHEAD,), (OP_QSIZE,)])


def queue_random(r):
    n = r.randrange(1, 6)
    ops = make_elements(n)
    for _ in range(r.randrange(1, 8)):
        pick = r.random()
        slot = r.randrange(n)
        if pick < 0.3:
            ops.append((OP_QREMOVE, slot))
        elif pick < 0.4:
            ops.append((OP_QADD, slot))     # may already be queued: the queue is then a cycle for Remove to break; see below
        elif pick < 0.55:
            ops.append((OP_GET_NEXT, slot))
        elif pick < 0.7:
            ops.append((OP_GET_DATA, slot))
        elif pick < 0.8:
            ops.append((OP_FROM_DATA, slot))
        elif pick < 0.9:
            ops.append((OP_QSIZE,))
        else:
            ops.append((OP_QHEAD,))
    # an element added while it is queued would make a cycle: only add the ones the script removed last
    out, queued = [], set()
    for op in ops:
        if op[0] == OP_QADD and op[1] in queued and len([o for o in out if o[0] == OP_QADD]) >= n:
            if op[1] in queued:
                continue
        if op[0] == OP_QADD:
            if op[1] in queued:
                continue
            queued.add(op[1])
        if op[0] == OP_QREMOVE:
            queued.discard(op[1])
        out.append(op)
    return pool_case(out, fill=rfill(r))


def maintain_edges():
    for target in (0, 1, 2, 3, 10, 12):
        for keep in (0, 1):
            for have in (0, 1, 2, 3, 5, 9, 10, 11, 14):
                if have > 12:
                    continue
                ops = make_elements(have) + [(OP_MAINTAIN,), (OP_QSIZE,), (OP_MAINTAIN,), (OP_QSIZE,)]
                yield pool_case(ops, pool=dict(target=target, keepfull=keep))
    # with some of the players in use: the trimming skips them, and stops (keep-full off) at the first it frees
    for target in (1, 2, 3):
        for keep in (0, 1):
            for busy in ([0], [1], [0, 1], [2, 3], [0, 1, 2, 3, 4], [4]):
                ops = make_elements(5) + [(OP_PLANT, i, 1) for i in busy] + [(OP_MAINTAIN,), (OP_QSIZE,), (OP_MAINTAIN,)]
                yield pool_case(ops, pool=dict(target=target, keepfull=keep))
    # the target is unsigned: a target past 2^31 is a long way off, a target of -1 is never exceeded (keep-full off: it would not end)
    for target in (0x80000000, 0xFFFFFFFF, 0x7FFFFFFF + 2):
        for have in (0, 1, 3):
            yield pool_case(make_elements(have) + [(OP_MAINTAIN,), (OP_QSIZE,), (OP_MAINTAIN,), (OP_QSIZE,)], pool=dict(target=target, keepfull=0))
    # the pool kept full, then the level changed
    yield pool_case([(OP_KEEP_FULL, 1), (OP_MAINTAIN,), (OP_SET_TARGET, 3), (OP_MAINTAIN,), (OP_SET_TARGET, 6), (OP_MAINTAIN,), (OP_KEEP_FULL, 0),
                     (OP_SET_TARGET, 8), (OP_MAINTAIN,), (OP_MAINTAIN,), (OP_SET_TARGET, 2), (OP_MAINTAIN,), (OP_MAINTAIN,)], pool=dict(target=4))
    yield pool_case([(OP_KEEP_FULL, 0x100), (OP_MAINTAIN,), (OP_KEEP_FULL, 0x101), (OP_SET_TARGET, 2), (OP_MAINTAIN,)], pool=dict(target=3))
    yield pool_case([(OP_SET_TARGET, 0), (OP_MAINTAIN,)])
    # a take and a give in the middle of it
    yield pool_case([(OP_KEEP_FULL, 1), (OP_MAINTAIN,), (OP_NEW, 0, 0x6D64), (OP_NEW, 1, 0x6D64), (OP_MAINTAIN,), (OP_DELETE, 0, 0x6D64, 1), (OP_DELETE, 1, 0x6D64, 1),
                     (OP_MAINTAIN,), (OP_QSIZE,)], pool=dict(target=5))


def maintain_random(r):
    ops = make_elements(r.randrange(0, 8))
    n = len(ops) // 2
    for _ in range(r.randrange(1, 8)):
        pick = r.random()
        if pick < 0.4:
            ops.append((OP_MAINTAIN,))
        elif pick < 0.5:
            ops.append((OP_KEEP_FULL, r.choice([0, 1, 2, 0x100])))
        elif pick < 0.6:
            ops.append((OP_SET_TARGET, r.choice([0, 1, 2, 3, 6, 10])))
        elif pick < 0.7 and n:
            ops.append((OP_PLANT, r.randrange(n), r.choice([0, 1, 1, 2, -39])))
        elif pick < 0.8:
            ops.append((OP_NEW, r.randrange(4), 0x6D64))
        elif pick < 0.9:
            ops.append((OP_QSIZE,))
        else:
            ops.append((OP_QHEAD,))
    return pool_case(ops, pool=dict(target=r.choice([0, 1, 2, 3, 5, 10]), keepfull=r.choice([0, 1])), fill=rfill(r))


def clear_edges():
    for all_ in (0, 1, 2, 0xFF, 0x100, 0x101):
        for have in (0, 1, 2, 4):
            yield pool_case(make_elements(have) + [(OP_CLEAR_POOL, all_), (OP_QSIZE,), (OP_QHEAD,), (OP_CLEAR_POOL, all_)])
    # the players in use stay (the one-pass form only: the restarting form never ends with a busy head)
    for busy in ([0], [1], [2], [0, 2], [1, 2], [0, 1, 2]):
        yield pool_case(make_elements(3) + [(OP_PLANT, i, 1) for i in busy] + [(OP_CLEAR_POOL, 0), (OP_QSIZE,), (OP_QHEAD,), (OP_CLEAR_POOL, 0x100), (OP_QSIZE,)])
    for busy in ([1], [2], [1, 2]):
        # busy elements behind the head: the restarting form frees the head and stops at... the first busy one forever, so those come out first
        yield pool_case(make_elements(3) + [(OP_PLANT, i, 1) for i in busy] + [(OP_CLEAR_POOL, 0), (OP_QSIZE,)])
    # a taken player is not in the queue
    yield pool_case(make_elements(3) + [(OP_NEW, 0, 0x6D64), (OP_CLEAR_POOL, 1), (OP_QSIZE,), (OP_DELETE, 0, 0x6D64, 1), (OP_CLEAR_POOL, 1), (OP_QSIZE,)])
    yield pool_case(make_elements(3) + [(OP_NEW, 0, 0x6D64), (OP_CLEAR_POOL, 0), (OP_QSIZE,), (OP_DELETE, 0, 0x6D64, 1), (OP_CLEAR_POOL, 0), (OP_QSIZE,)])
    # the report and the static cleanup object
    yield pool_case([(OP_REPORT,), (OP_REPORT,)])


def clear_random(r):
    n = r.randrange(0, 6)
    ops = make_elements(n)
    if r.random() < 0.4:
        ops += [(OP_PLANT, r.randrange(n), r.choice([1, 2, 0x7FFF])) for _ in range(r.randrange(1, 3))] if n else []
        ops.append((OP_CLEAR_POOL, r.choice([0, 0x100])))
    else:
        ops.append((OP_CLEAR_POOL, r.choice([0, 1, 2, 0xFF])))
    ops += [(OP_QSIZE,), (OP_QHEAD,)]
    return pool_case(ops, fill=rfill(r))


def static_edges():
    for flag in (1, 0, -1, 2, 0x7FFF, -0x8000):
        yield pool_case([(OP_STATIC_NEW,), (OP_STATIC_DTOR, flag)])
        yield pool_case([(OP_STATIC_NEW,)] + make_elements(3) + [(OP_STATIC_DTOR, flag), (OP_QSIZE,), (OP_QHEAD,)])
        yield pool_case([(OP_STATIC_NEW,)] + make_elements(3) + [(OP_NEW, 0, 0x6D64), (OP_STATIC_DTOR, flag), (OP_QSIZE,)])


def static_random(r):
    n = r.randrange(0, 5)
    return pool_case([(OP_STATIC_NEW,)] + make_elements(n) + [(OP_STATIC_DTOR, short(r.choice([1, 0, -1, 2, r.getrandbits(16)])))] + [(OP_QSIZE,)], fill=rfill(r))


def make_edges():
    yield pool_case([(OP_MAKE, 0), (OP_QSIZE,)])
    yield pool_case([(OP_MAKE, 0), (OP_MAKE, 1), (OP_GET_NEXT, 0), (OP_GET_DATA, 0), (OP_ELEMENT_TYPE, 0), (OP_IS_AVAILABLE, 0), (OP_IS_AVAILABLE, 1)])
    for value in (0, 1, 2, -1, -39, 0x7FFF, -0x8000, 0x100, -0x100):
        yield pool_case([(OP_MAKE, 0), (OP_PLANT, 0, value), (OP_IS_AVAILABLE, 0)])
    yield pool_case([(OP_MAKE, 0), (OP_MAKE, 1), (OP_QADD, 0), (OP_QADD, 1), (OP_QREMOVE, 0), (OP_QREMOVE, 1), (OP_QSIZE,)], mmx=0)


def make_random(r):
    n = r.randrange(1, 5)
    ops = [(OP_MAKE, i) for i in range(n)]
    for i in range(n):
        if r.random() < 0.6:
            ops.append((OP_PLANT, i, r.choice([0, 1, 2, -1, -39, 0x7FFF, -0x8000, r.randrange(-300, 300)])))
        ops.append((OP_IS_AVAILABLE, i))
    return pool_case(ops, fill=rfill(r))


def keepfull_edges():
    for flag in (0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, -1):
        yield pool_case([(OP_KEEP_FULL, flag), (OP_MAINTAIN,), (OP_QSIZE,)], pool=dict(target=3))


def keepfull_random(r):
    return pool_case([(OP_KEEP_FULL, r.getrandbits(10)), (OP_MAINTAIN,), (OP_QSIZE,)], pool=dict(target=r.randrange(0, 6), keepfull=r.choice([0, 1])), fill=rfill(r))


def sinit_edges():
    for mmx in (0, 1, 7):
        for guards in ((0, 0), (1, 1), (0, 1), (1, 0), (2, 2), (0xFF, 0), (0, 0xFF)):
            yield pool_case([(OP_SINIT,) + guards], mmx=mmx)
            yield pool_case(make_elements(2) + [(OP_SINIT,) + guards, (OP_QSIZE,), (OP_QHEAD,)], mmx=mmx)


def sinit_random(r):
    return pool_case(make_elements(r.randrange(0, 3)) + [(OP_SINIT, r.choice([0, 1, r.getrandbits(8)]), r.choice([0, 1, r.getrandbits(8)]))], mmx=r.choice([0, 1, 9]),
                     fill=rfill(r))


def udebug_edges():
    # the sound library's own UDebug does nothing at all: no event, no change (the stub for it is lifted for this test)
    yield pool_case([(OP_UDEBUG,)], udebug_real=True)
    yield pool_case([(OP_UDEBUG,), (OP_UDEBUG,)] + make_elements(1), udebug_real=True)



def udebug_random(r):
    return pool_case([(OP_UDEBUG,)] * r.randrange(1, 4), fill=rfill(r), udebug_real=True)


def initial_ppc(emu, case):
    """The pool's statics as the program's data holds them (a fresh emulator: the other tests have written the ones in `emu`)."""
    import difftest
    import ppcemu
    fresh = ppcemu.PPCEmu(difftest.DUMPS / PROGRAM)
    return dict(target=fresh.u32(TARGET), keepfull=fresh.read(KEEPFULL, 1)[0])


def initial_host(lib, case):
    lib.tpw_mf_initial_target.restype = C.c_uint32
    lib.tpw_mf_initial_keepfull.restype = C.c_uint32
    return dict(target=lib.tpw_mf_initial_target(), keepfull=lib.tpw_mf_initial_keepfull())


def initial_edges():
    yield dict()


TESTS = [
    FnTest("CMpegMacFilePlayerUnpooled::CMpegMacFilePlayerUnpooled() 0x10007dac and UPooled::UPooled() 0x10007d1c", run_ppc, host_case, edge=ctor_edges,
           rnd=ctor_random),
    FnTest("CMpegMacFilePlayerUnpooled::~CMpegMacFilePlayerUnpooled() 0x10003a30 and UPooled::~UPooled() 0x100086a4", run_ppc, host_case, edge=dtor_edges,
           rnd=dtor_random),
    FnTest("CMpegMacFilePlayerUnpooled::Open 0x100031e4 (and the whole player: Read through the file)", run_ppc, host_case, edge=open_edges, rnd=open_random),
    FnTest("CMpegMacFilePlayerUnpooled::Close() 0x100039a0", run_ppc, host_case, edge=close_edges, rnd=close_random),
    FnTest("CMpegMacFilePlayerUnpooled::GetHeader() 0x1000339c and DecodeHeader() 0x10003420", run_ppc, host_case, edge=header_edges, rnd=header_random),
    FnTest("CMpegMacFilePlayerUnpooled::NextBuffer() 0x100035c4", run_ppc, host_case, edge=next_edges, rnd=next_random),
    FnTest("CMpegMacFilePlayerUnpooled::ReadBuffered(void*, unsigned) 0x10003620", run_ppc, host_case, edge=read_buffered_edges, rnd=read_buffered_random),
    FnTest("CMpegMacFilePlayerUnpooled::RequestBuffer(int, bool) 0x10003868", run_ppc, host_case, edge=request_edges, rnd=request_random),
    FnTest("CMpegMacFilePlayerUnpooled::SeekBits(int) 0x100034d4", run_ppc, host_case, edge=seek_edges, rnd=seek_random),
    FnTest("UPooled::operator new 0x10007ffc", run_ppc, host_case, edge=new_edges, rnd=new_random),
    FnTest("UPooled::operator delete 0x10007e20", run_ppc, host_case, edge=delete_edges, rnd=delete_random),
    FnTest("UInterruptSafeQueue: constructor 0x100040ac, GetHead 0x10003e08, GetSize 0x1002bab0, Add 0x10007ec0, Remove 0x10003e60, Element::Element "
           "0x1002bd2c, GetNext 0x10003da8, GetData 0x10003fe0, GetElementFromDataPointer 0x10007f88", run_ppc, host_case, edge=queue_edges, rnd=queue_random),
    FnTest("UPooled::MaintainPoolLevel() 0x1002b964", run_ppc, host_case, edge=maintain_edges, rnd=maintain_random),
    FnTest("UPooled::ClearPool(unsigned char) 0x10003c58 and ReportPoolStatistics() 0x10004040", run_ppc, host_case, edge=clear_edges, rnd=clear_random),
    FnTest("UPooled::StaticCleanup::~StaticCleanup() 0x10003b98", run_ppc, host_case, edge=static_edges, rnd=static_random),
    FnTest("UPooled::MakeNewPoolElement() 0x1002bb08 and IsPoolObjectAvailable 0x10003f40", run_ppc, host_case, edge=make_edges, rnd=make_random),
    FnTest("UPooled::SetPoolKeepFull(unsigned char) 0x1002b5ec", run_ppc, host_case, edge=keepfull_edges, rnd=keepfull_random),
    FnTest("__sinit_LbMpeg_cpp 0x10003acc", run_ppc, host_case, edge=sinit_edges, rnd=sinit_random),
    FnTest("the pool's statics gPoolTarget 0x1003943c and gPoolKeepFull 0x1003e338 as the program starts", initial_ppc, initial_host, edge=initial_edges,
           rnd=lambda r: dict()),
    FnTest("SamsUtilities::UDebug 0x10003950", run_ppc, host_case, edge=udebug_edges, rnd=udebug_random),
]
