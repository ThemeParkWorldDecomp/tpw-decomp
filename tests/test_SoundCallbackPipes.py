"""Unit w79i, the sound library's sample callback pipes, against libtpw_unit_SoundCallbackPipes.dylib
(tests/unit_SoundCallbackPipes.cpp), in the program "sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base
r2 = 0x1003abf0):
  src/game/advisor/AdvisorScoring.cpp, src/sound/TbSoundBankInfo.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleCallback.cpp, src/sound/TbSoundSampleCodec.cpp, src/sound/TbSoundSamplePipe.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/UserCallback.cpp
    TbSoundUserCallback: constructors 0x10000d20 and 0x100002c8, destructor 0x100001b0, SetInfo 0x10013d78, GetInfo 0x10013e4c,
                         operator new 0x10013ae0, operator delete 0x10013bb0, TopUp 0x10013c54, SetLevel 0x10013d38, ClearCache
                         0x10013a4c, the cleanup destructor 0x10013f2c;
    _TbSoundSampleCallback: constructor 0x10000c94, destructor 0x1000d50c, SetInfo 0x1000cfa0, GetInfo 0x1000d078, GetDataChunk
                         0x1000d11c, GetNextSample 0x1000d380, operator new 0x1000cd00, operator delete 0x1000cdd0, TopUp
                         0x1000ce78, SetLevel 0x1000cf5c, ClearCache 0x1000cc68, the cleanup destructor 0x1000d608;
    _TbSoundSampleCompletedCallback: constructor 0x10000114, destructor 0x10001018, GetDataChunk 0x100010bc, operator new
                         0x1000f178, operator delete 0x1000f254, TopUp 0x1000f304, SetLevel 0x1000f3f4, ClearCache 0x1000f0d8,
                         the cleanup destructor 0x1000f948;
    CAutoDetach: constructor 0x1000d334, operator= 0x1000d26c, destructor 0x1000d29c

Each function runs as the original in the emulator and as the host's translation, in scripts of operations on a world of six node
slots (a user callback, a sample callback, a completed callback, a plain pipe, a timer codec, or a FakeNode that records every
virtual slot and answers GetDataChunk from a script), two fake TbSndCallbacks (labels 7 and 8), one sample info and an output
buffer of 0x10000 bytes. After EVERY operation both sides dump every slot (kind, mNext, mPrevious, mHandle, what the pool links
hold, the user data, the callback, the codec words of a fake or a timer, and the timer codec that GetNextSample made behind a
pipe), the five pools (the six statics and the chain of idle objects as labels; the three of the unit, the timer's and the pipe's),
the eight raw blocks of the pool operations, the five words of the sample info the pipes read and write, the output buffer (a CRC),
the result and the whole event log. Every callee outside the unit is a fake that records the same event on both sides: the
runtime's new (the Mac's size and a label; blocks are filled with 0xA5) and delete, the clock (LbTime_GetClock__Fv), the
FakeNode's slots (the virtual calls the real code makes through the Mac vtable: a slot number that is wrong shows as another name
in the log) and the callbacks' SoundCallback (slot +8). The real vtables 0x1003c7d8, 0x1003c1a8, 0x10035bbc, 0x1003c35c and
0x1003c758 are the Mac's own, and every operation on a node goes through its vtable. The timer codec, the sample info setters and
the pipe base run as the originals too.

The host's objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are mapped: the
Mac size of a class is the host size of that class, any other size travels as the Mac size + 0x1000.

Pointers compare as labels (0 NULL, the label of the object, 0xA5A5A5A5 for the fill, -1 for anything else). A script only calls
what is safe on the world it built (the Mac code reads the format byte of whatever is behind a sample pipe and loops until the pipe
behind it delivers: the unit file's ChunkSafe and ChainAlive say which, and this file mirrors them).
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
SHIM_LIBRARY = "unit_SoundCallbackPipes"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

# the vtables
USER_VT, SAMPLE_VT, COMPLETED_VT, PIPE_VT, TIMER_VT = 0x1003C7D8, 0x1003C1A8, 0x10035BBC, 0x1003C35C, 0x1003C758

# the pools: [total, current, low, high, first] statics, level, link offset, Mac size, then operator new, delete, TopUp, SetLevel,
# ClearCache and the cleanup destructor; the timer's and the pipe's are other units' functions that the callback pipes use
POOLS = [
    dict(name="User", total=0x1003F324, cur=0x1003F328, low=0x1003F32C, high=0x1003F330, first=0x1003F334, level=0x1003C7B8,
         link=0x14, size=0x1C, new=0x10013AE0, delete=0x10013BB0, topup=0x10013C54, setlevel=0x10013D38, clear=0x10013A4C),
    dict(name="Sample", total=0x1003EC28, cur=0x1003EC2C, low=0x1003EC30, high=0x1003EC34, first=0x1003EC38, level=0x1003C16C,
         link=0x1C, size=0x24, new=0x1000CD00, delete=0x1000CDD0, topup=0x1000CE78, setlevel=0x1000CF5C, clear=0x1000CC68),
    dict(name="Completed", total=0x1003F270, cur=0x1003F274, low=0x1003F278, high=0x1003F27C, first=0x1003F280, level=0x1003C350,
         link=0x1C, size=0x24, new=0x1000F178, delete=0x1000F254, topup=0x1000F304, setlevel=0x1000F3F4, clear=0x1000F0D8),
    dict(name="Timer", total=0x1003F300, cur=0x1003F304, low=0x1003F308, high=0x1003F30C, first=0x1003F310, level=0x1003C6F8,
         link=0x2C, size=0x30, new=0x10013484, delete=0x1001354C, topup=0x100135E8, setlevel=0x100136C4, clear=0x100133F8),
    dict(name="Pipe", total=0x1003F294, cur=0x1003F298, low=0x1003F29C, high=0x1003F2A0, first=0x1003F2A4, level=0x1003C34C,
         link=0x04, size=0x14, new=0x1000EE48, delete=0x1000EF14, topup=0x1000EFB8, setlevel=0x1000F098, clear=0x1000EDB8),
]
P_USER, P_SAMPLE, P_COMPLETED, P_TIMER, P_PIPE = range(5)
POOLS_N = len(POOLS)

# the functions
CTOR_USER0, CTOR_USER1, CTOR_SAMPLE, CTOR_COMPLETED = 0x10000D20, 0x100002C8, 0x10000C94, 0x10000114
CTOR_PIPE, CTOR_CODEC0, CTOR_TIMER = 0x10000344, 0x100137A4, 0x10013700
GET_NEXT = 0x1000D380
GUARD_CTOR, GUARD_ASSIGN, GUARD_DTOR = 0x1000D334, 0x1000D26C, 0x1000D29C
CS_DTOR = [0x10013F2C, 0x1000D608, 0x1000F948]

# the imports
IMP_NEW, IMP_DEL, IMP_CLOCK = "__nw__FUl", "__dl__FPv", "LbTime_GetClock__Fv"

# the kinds of node
K_NONE, K_USER, K_SAMPLE, K_COMPLETED, K_FAKE, K_PIPE, K_TIMER = range(7)
KIND_POOL = {K_USER: P_USER, K_SAMPLE: P_SAMPLE, K_COMPLETED: P_COMPLETED, K_TIMER: P_TIMER, K_PIPE: P_PIPE}
SLOTS = 6
BUF_BYTES = 0x10000
BUF_BASE = 0x8000
OUTSIDE = -(1 << 40)
FILL_STEP = 0x7FFFFFFF   # the default answer of the fake: result 0, all that was asked

# the operations (the order of enum Op in the unit file)
OPS = ["NEW", "DTOR", "GET_CHUNK", "GET_NEXT", "SET_INFO", "GET_INFO", "GUARD", "NEWOP", "DELETEOP", "TOPUP", "SETLEVEL",
       "CLEARCACHE", "CS_DTOR", "POKE", "LINK", "INFO"]
globals().update({"OP_" + name: index for index, name in enumerate(OPS)})

VT = {}
STUBS = None


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


def buf_off(w, address):
    if w.buf <= address < w.buf + BUF_BYTES:
        return address - w.buf - BUF_BASE
    return OUTSIDE


def new_block(e, size):
    """What the runtime's new does for the original: a block of 0xA5, the event with the Mac's size and the new label."""
    L = e.cl
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    L.rec("new", (size, L.newlab(block)))
    return block


def stub_new(e):
    e.set_r(3, new_block(e, e.r(3)))


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


def pool_chain(e, pool):
    chain, node = [], e.u32(POOLS[pool]["first"])
    while node and len(chain) < 64:
        chain.append(node)
        node = e.u32(node + POOLS[pool]["link"])
    return chain


def pooled_in(e, pool, node):
    steps, idle = 0, e.u32(POOLS[pool]["first"])
    while idle and steps < 64:
        if idle == node:
            return True
        idle = e.u32(idle + POOLS[pool]["link"])
        steps += 1
    return False


def pooled_any(e, node):
    return any(pooled_in(e, p, node) for p in range(POOLS_N))


def alive(e, w, slot):
    """A node exists, has its label and is not in its pool."""
    node = w.nodes[slot]
    if node == 0 or e.cl.lab(node) <= 0:
        return False
    pool = KIND_POOL.get(w.kinds[slot])
    return pool is None or not pooled_in(e, pool, node)


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


def node_address(w, slot):
    return w.nodes[slot] if 0 <= slot < SLOTS else 0


def type_of(e, w, node):
    """The unit file's TypeOf: the kind of a node by its vptr; K_NONE for NULL, a dead node (no label, or idle in a pool)."""
    if node == 0 or e.cl.lab(node) <= 0 or pooled_any(e, node):
        return K_NONE
    return {w.fake_vt: K_FAKE, TIMER_VT: K_TIMER, SAMPLE_VT: K_SAMPLE, COMPLETED_VT: K_COMPLETED, USER_VT: K_USER}.get(
        e.u32(node), K_PIPE)


def caller_of(e, w, handle):
    """The sample pipe that called SoundCallback: the first alive one with nothing behind it and the handle."""
    for i in range(SLOTS):
        node = w.nodes[i]
        if node == 0 or type_of(e, w, node) != K_SAMPLE:
            continue
        if e.u32(node + 8) == 0 and e.u32(node + 0x10) == handle:
            return node
    return 0


def callback_for(w, code):
    if code in (0, 1):
        return w.cbs[code]
    return 0x1234 if code == 2 else 0


def chunk_safe(e, w, node, size, depth=0):
    """The mirror of ChunkSafe in the unit file."""
    kind = type_of(e, w, node)
    if kind in (K_USER, K_PIPE, K_FAKE):
        return True
    if kind == K_TIMER:
        return 0 <= size <= 0x1000
    if kind == K_SAMPLE:
        nxt = e.u32(node + 8)
        if nxt == 0:
            return True
        nkind = type_of(e, w, nxt)
        if nkind not in (K_FAKE, K_TIMER):
            return False
        if e.cl.lab(e.u32(node + 0x20)) not in (7, 8):
            return False
        return nkind != K_TIMER or size <= 0x1000
    if kind == K_COMPLETED:
        if depth > 5:
            return False
        nxt = e.u32(node + 8)
        if nxt == 0:
            return True
        return chunk_safe(e, w, nxt, size, depth + 1)
    return False


def chain_alive(e, w, node):
    """The mirror of ChainAlive."""
    nxt, depth = e.u32(node + 8), 0
    while nxt:
        if type_of(e, w, nxt) == K_NONE or depth >= 6:
            return False
        nxt = e.u32(nxt + 8)
        depth += 1
    return True


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

    def with_result(name, key, *fields):
        base = rec(name, *fields)

        def stub(e):
            base(e)
            e.set_r(3, T.u32(e.cl.ret(key, 0)))
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

    def find(e):
        rec("Fake.FindPipe", "u")(e)
        value = e.cl.ret("FindPipe", -1)
        if value == -2:
            e.set_r(3, e.u32(e.r(3) + 8))
        else:
            e.set_r(3, node_address(e.w, value))

    def set_info(e):
        L, this, info = e.cl, e.r(4), e.r(5)
        L.rec("Fake.SetInfo", (L.lab(this), L.lab(info)))
        ws(e, e.r(3), [L.ret("SetInfoErr", 0)])

    def set_handle(e):
        rec("Fake.SetHandle", "u")(e)
        e.set_r(3, e.r(4))

    def set_time(e):
        rec("Fake.SetTime", "b", "u")(e)
        e.set_r(3, 0)

    def chunk(e):
        L, w, this = e.cl, e.w, e.r(3)
        size, donep = s32(e.r(5)), e.r(6)
        L.rec("Fake.GetDataChunk", (L.lab(this), buf_off(w, e.r(4)), size, e.r(7) & 0xFF, s32(e.u32(donep))))
        step = L.ret("Step", FILL_STEP)
        result = s32((step >> 32) & MASK)
        count = s32(step & MASK)
        if count == 0x7FFFFFFF:
            count = size
        if result == 1 and (size > 0x1000 or size < 0):
            result = 0
        if result == 1:
            count = 0 if count < 0 else size if count > size else count
        ws(e, donep, [count])
        e.set_r(3, result & MASK)

    return [
        dtor, with_zero("Fake.Attatch", "l"), rec("Fake.Release"), rec("Fake.Detatch"), find, rec("Fake.GetInfo", "l"),
        with_result("Fake.GetSampleRate", "Rate"), chunk, rec("Fake.Restart"), set_info, set_handle,
        rec("Fake.Completed", "b"), rec("Fake.SetDataStart", "u"), with_zero("Fake.GetDataStart"),
        rec("Fake.SetDataPosition", "u"), with_zero("Fake.GetDataPosition"), with_zero("Fake.GetDataSize"),
        rec("Fake.SetSampleRate", "h"), with_zero("Fake.GetBitRate"), with_zero("Fake.GetChannels"), with_zero("Fake.GetCodec"),
        set_time,
    ]


def callback_slots():
    """The callback's slots: +8 SoundCallback, +0xc CompletedCallback."""
    def sound(e):
        L, w = e.cl, e.w
        this, sample, data = e.r(3), e.r(4), e.r(5)
        caller = caller_of(e, w, sample)
        L.rec("Cb.Sound", (L.lab(this), sample & MASK, data & MASK, pw(L, e.u32(caller + 8)) if caller else -7))
        node = node_address(w, L.ret("Attach", -1))
        if caller and node and type_of(e, w, node) == K_FAKE:
            ws(e, caller + 8, [node])
            ws(e, node + 0xC, [caller])

    def completed(e):
        L = e.cl
        L.rec("Cb.Completed", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, s32(e.r(6))))

    return [sound, completed]


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs()
    words = [0, 0] + [T.fake_function(e, fn) for fn in fake_slots()]
    VT["fake"] = T.persist_alloc(e, 4 * len(words), struct.pack(">%dI" % len(words), *words))
    words = [0, 0] + [T.fake_function(e, fn) for fn in callback_slots()]
    VT["cb"] = T.persist_alloc(e, 4 * len(words), struct.pack(">%dI" % len(words), *words))
    lib = runner.lib
    lib.tpw_cb_sizes.restype = C.c_int32
    lib.tpw_cb_sizes.argtypes = [C.c_int32]
    lib.tpw_cb_op_count.restype = C.c_int32
    assert lib.tpw_cb_op_count() == len(OPS), "the operations of the unit file and of the test differ"


def ppc_slot_state(e, w, slot):
    out = [0] * 24
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
    if K_USER <= kind <= K_COMPLETED:
        out[6], out[7] = link_class(e.u32(node + 0x14)), e.u32(node + 0x18)
    if kind in (K_SAMPLE, K_COMPLETED):
        out[8], out[9] = link_class(e.u32(node + 0x1C)), pw(L, e.u32(node + 0x20))
    if kind in (K_FAKE, K_TIMER):
        out[10], out[11], out[12] = e.u32(node + 0x18), e.u32(node + 0x20), e.u32(node + 0x24)
        out[13] = e.read(node + 0x28, 1)[0]
        out[14], out[15] = pw(L, e.u32(node + 0x14)), link_class(e.u32(node + 0x1C))
    if kind == K_TIMER:
        out[16] = link_class(e.u32(node + 0x2C))
    nxt = e.u32(node + 8)
    if nxt:
        is_timer = e.u32(nxt) == TIMER_VT
        out[17] = 1 if is_timer else 2
        if is_timer:
            out[18] = e.read(nxt + 0x28, 1)[0]
            out[19], out[20], out[21] = e.u32(nxt + 0x20), e.u32(nxt + 0x24), e.u32(nxt + 0x18)
            out[22], out[23] = link_class(e.u32(nxt + 0x2C)), pw(L, e.u32(nxt + 8))
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
    out = [0] * 7
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
        out[1], out[2], out[3], out[4] = pw(L, e.u32(block + 8)), pw(L, e.u32(block + 0xC)), e.u32(block + 0x10), link_class(e.u32(block + 4))
        if pool in (P_USER, P_SAMPLE, P_COMPLETED):
            out[5] = link_class(e.u32(block + 0x14))
        if pool != P_USER:
            out[6] = link_class(e.u32(block + POOLS[pool]["link"]))
    return out


def ppc_info_state(e, w):
    L = e.cl
    return [e.u32(w.info + 0x14), e.u32(w.info + 0x18), pw(L, e.u32(w.info + 0x44)), pw(L, e.u32(w.info + 0x48)), e.u32(w.info + 0x4C)]


def vcall(e, obj, slot, args):
    """A virtual call through the object's vtable (the slot holds a function descriptor)."""
    vt = e.u32(obj)
    return call(e, e.u32(e.u32(vt + slot)), [obj] + args)


def ppc_op(e, w, op, a, b, c, d):
    L = e.cl
    aux = [0, 0]
    needs_node = op in (OP_DTOR, OP_GET_CHUNK, OP_GET_NEXT, OP_SET_INFO, OP_GET_INFO, OP_GUARD, OP_POKE, OP_LINK)
    kind = K_NONE
    if needs_node:
        if not 0 <= a < SLOTS or not alive(e, w, a):
            return -99, aux
        kind = w.kinds[a]
        obj = w.nodes[a]
        if op == OP_GET_CHUNK:
            if b < 0 or b > 0x800 or not chunk_safe(e, w, obj, c):
                return -99, aux
        if op in (OP_SET_INFO, OP_GET_INFO) and not chain_alive(e, w, obj):
            return -99, aux
        if op == OP_GET_NEXT:
            if kind != K_SAMPLE:
                return -99, aux
            if type_of(e, w, e.u32(obj + 8)) not in (K_FAKE, K_TIMER) or L.lab(e.u32(obj + 0x20)) not in (7, 8):
                return -99, aux
        if op == OP_POKE:
            if b == 3 and not K_USER <= kind <= K_COMPLETED:
                return -99, aux
            if b == 4 and kind not in (K_SAMPLE, K_COMPLETED):
                return -99, aux
            if b >= 5 and kind not in (K_FAKE, K_TIMER):
                return -99, aux
        if op == OP_LINK:
            if b >= SLOTS or b < -1:
                return -99, aux
            if b >= 0 and (not alive(e, w, b) or b == a):
                return -99, aux
    obj = node_address(w, a)
    if op == OP_NEW:
        if not 0 <= a < SLOTS or not K_USER <= b <= K_TIMER:
            return -99, aux
        if b == K_USER:
            block = call(e, POOLS[P_USER]["new"], [0x1C])
            if d:
                call(e, CTOR_USER0, [block])
            else:
                call(e, CTOR_USER1, [block, c & MASK])
        elif b == K_SAMPLE:
            block = call(e, POOLS[P_SAMPLE]["new"], [0x24])
            call(e, CTOR_SAMPLE, [block, callback_for(w, c)])
        elif b == K_COMPLETED:
            block = call(e, POOLS[P_COMPLETED]["new"], [0x24])
            call(e, CTOR_COMPLETED, [block, callback_for(w, c), d & MASK])
        elif b == K_PIPE:
            block = call(e, POOLS[P_PIPE]["new"], [0x14])
            call(e, CTOR_PIPE, [block])
        elif b == K_TIMER:
            block = call(e, POOLS[P_TIMER]["new"], [0x30])
            call(e, CTOR_TIMER, [block, c & 0xFF, d & MASK])
        else:
            block = call(e, POOLS[P_PIPE]["new"], [0x2C])
            call(e, CTOR_CODEC0, [block])
            ws(e, block, [VT["fake"]])
        for i in range(SLOTS):
            if w.nodes[i] == block:
                w.nodes[i] = 0
        w.nodes[a] = block
        w.kinds[a] = b
        return L.lab(block), aux
    if op == OP_DTOR:
        vcall(e, obj, 8, [b & MASK])
        if T.s16(b & 0xFFFF) <= 0:
            L.forget(obj)   # destroyed but not freed: the object is dead to the test
        w.nodes[a] = 0
        w.kinds[a] = K_NONE
        return 0, aux
    if op == OP_GET_CHUNK:
        done = e.alloc(8, struct.pack(">II", 0x5555, 0))
        result = s32(vcall(e, obj, 0x24, [w.buf + BUF_BASE + b, c & MASK, done, 1 if d else 0]))
        aux[0] = s32(e.u32(done))
        return result, aux
    if op == OP_GET_NEXT:
        call(e, GET_NEXT, [obj, b & MASK])
        return 0, aux
    if op == OP_SET_INFO:
        result = e.alloc(16, b"\xa5" * 16)
        vt = e.u32(obj)
        call(e, e.u32(e.u32(vt + 0x2C)), [result, obj, w.info])
        return s32(e.u32(result)), aux
    if op == OP_GET_INFO:
        vcall(e, obj, 0x1C, [w.info])
        return 0, aux
    if op == OP_GUARD:
        guard = e.alloc(16, b"\xa5" * 16)
        call(e, GUARD_CTOR, [guard, obj])
        aux[0] = e.read(guard + 4, 1)[0]
        if b == 1:
            call(e, GUARD_ASSIGN, [guard, 1])
        if b == 2:
            call(e, GUARD_ASSIGN, [guard, 1])
            call(e, GUARD_ASSIGN, [guard, 0])
        aux[1] = e.read(guard + 4, 1)[0]
        result = L.lab(e.u32(guard))
        call(e, GUARD_DTOR, [guard, MASK])
        return result, aux
    if op in (OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL, OP_CLEARCACHE):
        pool = {OP_NEWOP: b, OP_DELETEOP: b, OP_TOPUP: a, OP_SETLEVEL: a, OP_CLEARCACHE: a}[op]
        if not 0 <= pool < POOLS_N:
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
    if op == OP_CS_DTOR:
        if not 0 <= a <= 2:
            return -99, aux
        block = new_block(e, 1)
        call(e, CS_DTOR[a], [block, b & MASK])
        return 0, aux
    if op == OP_POKE:
        if b == 0:
            ws(e, obj + 0x10, [c])
        elif b == 3:
            ws(e, obj + 0x18, [c])
        elif b == 4:
            ws(e, obj + 0x20, [callback_for(w, c)])
        elif b == 5:
            e.write(obj + 0x28, bytes([c & 0xFF]))
        elif b in (6, 7, 8):
            ws(e, obj + {6: 0x20, 7: 0x24, 8: 0x18}[b], [c])
        return 0, aux
    if op == OP_LINK:
        if b < 0:
            ws(e, obj + 8, [0])
        else:
            ws(e, obj + 8, [w.nodes[b]])
            ws(e, w.nodes[b] + 0xC, [obj])
        return 0, aux
    if op == OP_INFO:
        offset = {0: 0x14, 1: 0x18, 2: 0x44, 3: 0x4C, 4: 0x48}.get(a)
        if offset is None:
            return -99, aux
        value = callback_for(w, b) if a in (2, 4) else b
        ws(e, w.info + offset, [value])
        return 0, aux
    raise ValueError(op)


def ppc_case(e, case):
    e.heap_in_scratch = True
    w = e.w = World()
    w.nodes, w.kinds, w.blocks, w.bsizes, w.bpools = [0] * SLOTS, [K_NONE] * SLOTS, [0] * 8, [0] * 8, [0] * 8
    w.fake_vt = VT["fake"]
    w.info = e.alloc(0x100, b"\xa5" * 0x100)
    w.cbs = [e.alloc(8, struct.pack(">II", VT["cb"], 0)) for _ in range(2)]
    w.buf = e.alloc(BUF_BYTES, b"\xa5" * BUF_BYTES)
    fixed = {w.info: 3, w.cbs[0]: 7, w.cbs[1]: 8}
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
                              pools=[ppc_pool_state(e, p) for p in range(POOLS_N)],
                              blocks=[ppc_block_state(e, w, i) for i in range(8)], info=ppc_info_state(e, w),
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
    lib.tpw_cb_begin.argtypes = []
    lib.tpw_cb_pool_set.argtypes = [C.c_int32] * 6
    lib.tpw_cb_op.restype = C.c_int64
    lib.tpw_cb_op.argtypes = [C.c_int32] + [C.c_int64] * 4
    lib.tpw_cb_aux.restype = C.c_int64
    lib.tpw_cb_aux.argtypes = [C.c_int32]
    lib.tpw_cb_slot.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_cb_pool.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_cb_block.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_cb_info.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cb_buffer.argtypes = [C.c_void_p]
    CL.host_begin(lib, {}, case["q"])
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib.tpw_cb_begin()
    for pool in range(POOLS_N):
        current, low, high, total, level = case["pools"].get(pool, (0, 0, 0, 0, 10))
        lib.tpw_cb_pool_set(pool, current, low, high, total, level)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    steps = []
    buffer = (C.c_uint8 * BUF_BYTES)()
    try:
        for op in case["ops"]:
            before = C.c_int32.in_dll(lib, "tpw_cl_n").value
            result = lib.tpw_cb_op(*op)
            aux = [lib.tpw_cb_aux(0), lib.tpw_cb_aux(1)]
            after = C.c_int32.in_dll(lib, "tpw_cl_n").value
            events = host_events(lib, before, after)
            slots = []
            for s in range(SLOTS):
                out = (C.c_int64 * 24)()
                lib.tpw_cb_slot(s, out)
                slots.append([int(x) for x in out[:24]])
            pools = []
            for p in range(POOLS_N):
                out = (C.c_int64 * 24)()
                lib.tpw_cb_pool(p, out)
                values = []
                for x in out:
                    values.append(int(x))
                    if len(values) > 6 and x == -9:
                        break
                pools.append(values)
            blocks = []
            for i in range(8):
                out = (C.c_int64 * 8)()
                lib.tpw_cb_block(i, out)
                blocks.append([int(x) for x in out[:7]])
            info = (C.c_int64 * 5)()
            lib.tpw_cb_info(info)
            lib.tpw_cb_buffer(buffer)
            steps.append(dict(result=result, aux=aux, slots=slots, pools=pools, blocks=blocks, info=[int(x) for x in info],
                              buffer=zlib.crc32(bytes(buffer)), log=events))
    finally:
        lib.tpw_cb_end()
    return dict(steps=steps)


# ---------------------------------------------------------------- the scripts

def op(kind, a=0, b=0, c=0, d=0):
    return (kind, a, b, c, d)


def case(ops, q=None, pools=None):
    return dict(ops=ops, q=q or {}, pools=pools or {})


def step(result, count):
    """One scripted answer of the fake: the result and the bytes delivered (FILL_STEP: all that was asked)."""
    return (result << 32) | (count & MASK)


FLAGS = [1, 1, 1, 2, 0, -1 & MASK, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0x80000001]
COOKIES = [0, 1, 0x1234, 0x7FFFFFF0, 0x80000000, MASK, 0xFFFF]
INFO_FLAGS = [0, 0x800, 0x1000, 0x1800, 0x2000, 0x3800, MASK, 0x400, 0xFFFFE7FF, 0x1, 0x7FF, 0xA5A5A5A5]
INFO_REQUESTS = [0, 0x800, 0x1000, 0x1800, 0x2000, 0x3800, MASK, 0x400, 0xFFFFE7FF, 0xA5A5A5A5]


def new_ops(layout, r=None):
    """NEW for each (kind, c, d) of the layout, in slots 0 to n-1."""
    return [op(OP_NEW, i, kind, c, d) for i, (kind, c, d) in enumerate(layout)]


# ---------------------------------------------------------------- the pools

def pool_tail_nodes(r):
    ops = []
    for _ in range(r.randrange(0, 3)):
        kind = r.choice([K_USER, K_SAMPLE, K_COMPLETED, K_USER, K_SAMPLE])
        ops.append(op(OP_NEW, r.randrange(SLOTS), kind, r.choice(COOKIES), r.choice([0, 1])))
    return ops


def pool_script(r):
    ops, blocks, live = [], {}, {}
    pool_index = r.choice([0, 0, 1, 1, 2, 2, 3, 4])
    mac = POOLS[pool_index]["size"]
    sizes = [0, 1, mac - 1, mac, mac, mac, mac + 1, 0x2C, 0x100, 0x14, 0x1C, 0x24]
    pools = {}
    if r.random() < 0.3:
        pools[pool_index] = (r.choice([0, 3, -2, 100]), r.choice([0, 5, -1]), r.choice([0, 7, 100]),
                             r.choice([0, 9, 0x7FFFFFFF]), r.choice([10, 0, 3, -4, 15]))
    for _ in range(r.randrange(3, 18)):
        c = r.random()
        pool = pool_index if r.random() < 0.7 else r.randrange(POOLS_N)
        macsize = POOLS[pool]["size"]
        if c < 0.22:
            index, size = r.randrange(8), r.choice(sizes if pool == pool_index else [macsize, macsize, 0, 1, macsize + 1])
            ops.append(op(OP_NEWOP, index, pool, size))
            blocks[index] = (pool, size)
        elif c < 0.42 and blocks:
            index = r.choice(sorted(blocks))
            bpool, bsize = blocks[index]
            mac_b = POOLS[bpool]["size"]
            # the class's size puts the block on the list: only a block that was asked for at least that much can take it
            if bsize >= mac_b:
                size = r.choice([mac_b, mac_b, mac_b, mac_b + 1, 0x100, 0, 1])
            else:
                size = r.choice([mac_b + 1, 0x100, 0, 1])
            ops.append(op(OP_DELETEOP, index, bpool, size, 1))
            del blocks[index]
        elif c < 0.46:
            ops.append(op(OP_DELETEOP, -1, pool, r.choice([macsize, 0x2C, 0])))
        elif c < 0.58:
            ops.append(op(OP_TOPUP, pool))
        elif c < 0.66:
            ops.append(op(OP_SETLEVEL, pool, r.choice([0, 1, 2, 3, 5, 10, 13, -1, 4])))
        elif c < 0.72:
            ops.append(op(OP_CLEARCACHE, pool))
        elif c < 0.9:
            slot = r.randrange(SLOTS)
            kind = r.choice([K_USER, K_USER, K_SAMPLE, K_SAMPLE, K_COMPLETED, K_COMPLETED, K_PIPE, K_TIMER, K_FAKE])
            ops.append(op(OP_NEW, slot, kind, r.choice(COOKIES) if kind != K_TIMER else r.choice([0x21, 0x41]),
                          r.choice([0, 1]) if kind != K_TIMER else r.randrange(1000)))
            live[slot] = True
        elif live:
            slot = r.choice(sorted(live))
            ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
            del live[slot]
    return case(ops, {"Clock": [r.choice([0, 100, r.getrandbits(32)])]}, pools)


def pool_edges():
    out = []
    for p in range(3):
        mac = POOLS[p]["size"]
        # a take from an empty list, a push, a take, the statics after each
        out.append(case([op(OP_NEWOP, 0, p, mac), op(OP_DELETEOP, 0, p, mac, 1), op(OP_NEWOP, 1, p, mac), op(OP_NEWOP, 2, p, mac),
                         op(OP_DELETEOP, 1, p, mac, 1), op(OP_DELETEOP, 2, p, mac, 1), op(OP_TOPUP, p), op(OP_CLEARCACHE, p)]))
        # sizes: 0 counts as 1, anything but the class's size comes from the runtime and goes back to it
        for size in (0, 1, mac - 1, mac + 1, 0x14, 0x2C, 0x100):
            out.append(case([op(OP_NEWOP, 0, p, size), op(OP_DELETEOP, 0, p, size, 1), op(OP_NEWOP, 1, p, mac),
                             op(OP_DELETEOP, 1, p, size if size else mac + 1, 1)]))
        out.append(case([op(OP_NEWOP, 0, p, mac), op(OP_NEWOP, 1, p, mac), op(OP_DELETEOP, 0, p, mac + 1, 1), op(OP_DELETEOP, 1, p, mac, 0),
                         op(OP_NEWOP, 2, p, mac)]))
        out.append(case([op(OP_DELETEOP, -1, p, mac), op(OP_DELETEOP, -1, p, 0), op(OP_DELETEOP, -1, p, 0x100)]))
        # TopUp: the level, the counts and the marks
        for level in (0, 1, 3, 10, 12, -1):
            out.append(case([op(OP_SETLEVEL, p, level), op(OP_TOPUP, p), op(OP_TOPUP, p), op(OP_NEWOP, 0, p, mac), op(OP_TOPUP, p),
                             op(OP_CLEARCACHE, p), op(OP_TOPUP, p)]))
        out.append(case([op(OP_SETLEVEL, p, 2), op(OP_NEWOP, 0, p, mac), op(OP_NEWOP, 1, p, mac), op(OP_NEWOP, 2, p, mac),
                         op(OP_NEWOP, 3, p, mac), op(OP_DELETEOP, 0, p, mac, 1), op(OP_DELETEOP, 1, p, mac, 1),
                         op(OP_DELETEOP, 2, p, mac, 1), op(OP_DELETEOP, 3, p, mac, 1), op(OP_TOPUP, p), op(OP_NEWOP, 4, p, mac),
                         op(OP_NEWOP, 5, p, mac), op(OP_NEWOP, 6, p, mac), op(OP_TOPUP, p)]))
        # the marks: low and high from given statics (the low mark only moves down on a take, the high up on a push or TopUp)
        for state in ((0, 0, 0, 0, 10), (5, 5, 5, 5, 10), (5, 9, 2, 1, 3), (-3, 0, -5, 0, 4), (2, 2, 100, 0x7FFFFFFF, 0), (0, 3, 0, 0, 2),
                      (4, -1, 4, 4, 6)):
            out.append(case([op(OP_TOPUP, p), op(OP_NEWOP, 0, p, mac), op(OP_NEWOP, 1, p, mac), op(OP_DELETEOP, 0, p, mac, 1),
                             op(OP_TOPUP, p), op(OP_CLEARCACHE, p)], pools={p: state}))
            out.append(case([op(OP_NEWOP, 0, p, mac), op(OP_DELETEOP, 0, p, mac, 1), op(OP_NEWOP, 1, p, mac), op(OP_CLEARCACHE, p)],
                            pools={p: state}))
    # SetLevel with the other classes' pools untouched, and TopUp of one pool leaves the others alone
    out.append(case([op(OP_SETLEVEL, 0, 2), op(OP_SETLEVEL, 1, 3), op(OP_SETLEVEL, 2, 4), op(OP_TOPUP, 0), op(OP_TOPUP, 1),
                     op(OP_TOPUP, 2), op(OP_CLEARCACHE, 1), op(OP_CLEARCACHE, 0), op(OP_CLEARCACHE, 2)]))
    # nodes through the pools: each class goes to its own, the plain pipe to the pipe's, the fake and the timer elsewhere
    out.append(case([op(OP_NEW, 0, K_USER, 5, 0), op(OP_NEW, 1, K_SAMPLE, 0), op(OP_NEW, 2, K_COMPLETED, 1, 7), op(OP_NEW, 3, K_PIPE),
                     op(OP_NEW, 4, K_FAKE), op(OP_NEW, 5, K_TIMER, 0x21, 5), op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 1), op(OP_DTOR, 2, 1),
                     op(OP_DTOR, 3, 1), op(OP_DTOR, 4, 1), op(OP_DTOR, 5, 1), op(OP_NEW, 0, K_USER, 9, 0), op(OP_NEW, 1, K_SAMPLE, 1),
                     op(OP_NEW, 2, K_COMPLETED, 0, 8), op(OP_NEW, 3, K_PIPE), op(OP_TOPUP, 0), op(OP_TOPUP, 1), op(OP_TOPUP, 2),
                     op(OP_CLEARCACHE, 0), op(OP_CLEARCACHE, 1), op(OP_CLEARCACHE, 2)], {"Clock": [0]}))
    return out


# ---------------------------------------------------------------- constructors, destructors, cleanup objects

def basic_edges():
    out = []
    layouts = [(K_USER, 0x1234, 0), (K_USER, 0x1234, 1), (K_USER, MASK, 0), (K_SAMPLE, 0, 0), (K_SAMPLE, 1, 0), (K_SAMPLE, 2, 0),
               (K_SAMPLE, 3, 0), (K_COMPLETED, 0, 0x55), (K_COMPLETED, 1, MASK), (K_COMPLETED, 2, 0), (K_COMPLETED, 3, 7)]
    for kind, c, d in layouts:
        for flag in FLAGS + [-2 & MASK, 0x7FFF0000, 0x00008001]:
            out.append(case([op(OP_NEW, 0, kind, c, d), op(OP_POKE, 0, 0, 0x1234), op(OP_DTOR, 0, flag), op(OP_NEWOP, 0, kind - 1, 0x1C if kind == K_USER else 0x24)]))
    # a block recycled from the pool keeps the words a constructor does not write (the pool links, the old user data of nothing)
    for kind in (K_USER, K_SAMPLE, K_COMPLETED):
        out.append(case([op(OP_NEW, 0, kind, 1, 0x77), op(OP_POKE, 0, 0, 0x99), op(OP_POKE, 0, 3, 0x1234), op(OP_DTOR, 0, 1),
                         op(OP_NEW, 1, kind, 2, 0x88), op(OP_NEW, 2, kind, 0, 0), op(OP_DTOR, 2, 1), op(OP_DTOR, 1, 0x8000),
                         op(OP_NEW, 3, kind, 1, 1), op(OP_DTOR, 3, 0x10001)]))
    # a pipe behind another dies: nothing else is released
    out.append(case([op(OP_NEW, 0, K_SAMPLE, 0), op(OP_NEW, 1, K_FAKE), op(OP_LINK, 0, 1), op(OP_DTOR, 0, 1), op(OP_DTOR, 1, 1)]))
    out.append(case([op(OP_NEW, 0, K_COMPLETED, 0, 1), op(OP_NEW, 1, K_FAKE), op(OP_LINK, 0, 1), op(OP_DTOR, 0, 0), op(OP_DTOR, 1, 0)]))
    return out


def basic_script(r):
    ops = []
    kinds = [r.choice([K_USER, K_SAMPLE, K_COMPLETED, K_PIPE, K_FAKE]) for _ in range(4)]
    for i, kind in enumerate(kinds):
        ops.append(op(OP_NEW, i, kind, r.choice([0, 1, 2, 3, 0x1234, MASK]), r.choice([0, 1, 0x77, MASK])))
    for _ in range(r.randrange(1, 8)):
        slot = r.randrange(4)
        c = r.random()
        if c < 0.3:
            ops.append(op(OP_POKE, slot, 0, r.choice(COOKIES)))
        elif c < 0.5:
            ops.append(op(OP_POKE, slot, r.choice([3, 4]), r.choice([0, 1, 2, 0x1234, MASK])))
        elif c < 0.7:
            ops.append(op(OP_GET_CHUNK, slot, 0, r.choice([0, 1, 64]), r.randrange(2)))
        else:
            ops.append(op(OP_NEW, r.randrange(SLOTS), r.choice([K_USER, K_SAMPLE, K_COMPLETED]), r.choice([0, 1]), r.choice([0, 5])))
    for slot in r.sample(range(4), r.randrange(1, 5)):
        ops.append(op(OP_DTOR, slot, r.choice(FLAGS)))
    return case(ops, {"Step": [FILL_STEP]})


def cs_edges():
    out = []
    for which in range(3):
        for flag in FLAGS + [-2 & MASK, 0x7FFF0000]:
            out.append(case([op(OP_NEWOP, 0, which, POOLS[which]["size"]), op(OP_NEWOP, 1, which, POOLS[which]["size"]),
                             op(OP_DELETEOP, 0, which, POOLS[which]["size"], 1), op(OP_DELETEOP, 1, which, POOLS[which]["size"], 1),
                             op(OP_TOPUP, which), op(OP_CS_DTOR, which, flag), op(OP_NEWOP, 2, which, POOLS[which]["size"]),
                             op(OP_CS_DTOR, which, 1)]))
    # a sweeper clears only its own class's pool
    out.append(case([op(OP_TOPUP, 0), op(OP_TOPUP, 1), op(OP_TOPUP, 2), op(OP_CS_DTOR, 1, 0), op(OP_CS_DTOR, 0, 1), op(OP_CS_DTOR, 2, 0x8000),
                     op(OP_TOPUP, 0)]))
    return out


# ---------------------------------------------------------------- SetInfo and GetInfo

def info_script_for(r, layout, with_tail, info=None):
    ops = new_ops(layout)
    if with_tail:
        ops.append(op(OP_NEW, len(layout), K_FAKE))
        ops.append(op(OP_LINK, 0, len(layout)))
    return ops


def info_edges():
    out = []
    layouts = [[(K_USER, 5, 0)], [(K_SAMPLE, 0, 0)], [(K_COMPLETED, 1, 6)], [(K_SAMPLE, 3, 0)]]
    for layout in layouts:
        for tail in (False, True):
            for err in (0, -1, 1, 5, -0x80000000, 0x7FFFFFFF):
                if not tail and err:
                    continue
                for flags in INFO_FLAGS:
                    for request in INFO_REQUESTS[:6]:
                        ops = info_script_for(None, layout, tail)
                        ops += [op(OP_INFO, 0, flags), op(OP_INFO, 1, request), op(OP_INFO, 2, 1), op(OP_INFO, 3, 0xCAFE),
                                op(OP_INFO, 4, 0), op(OP_SET_INFO, 0), op(OP_GET_INFO, 0), op(OP_POKE, 0, 3, 0x4321),
                                op(OP_SET_INFO, 0), op(OP_GET_INFO, 0)]
                        out.append(case(ops, {"SetInfoErr": [err]}))
    # the callback codes and the stored words
    for code in (0, 1, 2, 3):
        out.append(case(info_script_for(None, [(K_SAMPLE, 1, 0)], False) +
                        [op(OP_INFO, 0, 0x800), op(OP_INFO, 2, code), op(OP_INFO, 3, 0x1111), op(OP_SET_INFO, 0), op(OP_INFO, 1, 0x800),
                         op(OP_GET_INFO, 0)]))
    # a failure of the chain leaves the stored words alone
    out.append(case(info_script_for(None, [(K_SAMPLE, 1, 0), (K_FAKE, 0, 0)], False) +
                    [op(OP_LINK, 0, 1), op(OP_INFO, 0, 0x1800), op(OP_INFO, 2, 0), op(OP_INFO, 3, 0x5A5A), op(OP_SET_INFO, 0)],
                    {"SetInfoErr": [-1]}))
    out.append(case(info_script_for(None, [(K_COMPLETED, 1, 0x33), (K_FAKE, 0, 0)], False) +
                    [op(OP_LINK, 0, 1), op(OP_INFO, 0, 0x1800), op(OP_INFO, 3, 0x5A5A), op(OP_SET_INFO, 0)], {"SetInfoErr": [-5]}))
    # chains of callbacks: the pipe's SetInfo goes down, GetInfo does not
    out.append(case(new_ops([(K_COMPLETED, 0, 1), (K_SAMPLE, 1, 0), (K_USER, 3, 0), (K_FAKE, 0, 0)]) +
                    [op(OP_LINK, 0, 1), op(OP_LINK, 1, 2), op(OP_LINK, 2, 3), op(OP_INFO, 0, 0x1800), op(OP_INFO, 1, 0x1800),
                     op(OP_INFO, 2, 0), op(OP_INFO, 3, 0x777), op(OP_SET_INFO, 0), op(OP_GET_INFO, 0), op(OP_SET_INFO, 1),
                     op(OP_GET_INFO, 1), op(OP_SET_INFO, 2), op(OP_GET_INFO, 2)], {"SetInfoErr": [0, 0, -2, 0]}))
    return out


def info_random(r):
    n = r.randrange(1, 4)
    layout = [(r.choice([K_USER, K_SAMPLE, K_COMPLETED, K_PIPE, K_FAKE]), r.choice([0, 1, 2, 3, 0x1234]), r.choice([0, 1, 0x77]))
              for _ in range(n)]
    ops = new_ops(layout)
    for i in range(n - 1):
        ops.append(op(OP_LINK, i, i + 1))
    for _ in range(r.randrange(1, 10)):
        c = r.random()
        if c < 0.25:
            ops.append(op(OP_INFO, 0, r.choice(INFO_FLAGS + [r.getrandbits(32)])))
        elif c < 0.45:
            ops.append(op(OP_INFO, 1, r.choice(INFO_REQUESTS + [r.getrandbits(32)])))
        elif c < 0.55:
            ops.append(op(OP_INFO, r.choice([2, 3, 4]), r.choice([0, 1, 2, 3, 0x1234, 0xCAFE])))
        elif c < 0.62:
            ops.append(op(OP_POKE, r.randrange(n), r.choice([3, 4]), r.choice([0, 1, 2, 0x4321])))
        elif c < 0.81:
            ops.append(op(OP_SET_INFO, r.randrange(n)))
        else:
            ops.append(op(OP_GET_INFO, r.randrange(n)))
    q = {}
    if r.random() < 0.7:
        q["SetInfoErr"] = [r.choice([0, 0, -1, 1, 5, -7, 0x7FFFFFFF, -0x80000000]) for _ in range(r.randrange(1, 4))]
    return case(ops, q)


# ---------------------------------------------------------------- GetDataChunk and GetNextSample

TEMPLATES = [
    # (name, layout of (kind, c, d), links, the slots GetDataChunk is asked of)
    ("sample over fake", [(K_SAMPLE, 0, 0), (K_FAKE, 0, 0)], [(0, 1)], [0]),
    ("completed over sample over fake", [(K_COMPLETED, 0, 5), (K_SAMPLE, 1, 0), (K_FAKE, 0, 0)], [(0, 1), (1, 2)], [0, 1]),
    ("pipe before sample over fake", [(K_PIPE, 0, 0), (K_SAMPLE, 0, 0), (K_FAKE, 0, 0)], [(0, 1), (1, 2)], [0, 1]),
    ("completed over fake", [(K_COMPLETED, 1, 6), (K_FAKE, 0, 0)], [(0, 1)], [0]),
    ("sample over timer", [(K_SAMPLE, 1, 0), (K_TIMER, 0x21, 50)], [(0, 1)], [0, 1]),
    ("sample alone", [(K_SAMPLE, 0, 0)], [], [0]),
    ("completed alone", [(K_COMPLETED, 0, 0)], [], [0]),
    ("user over fake", [(K_USER, 4, 0), (K_FAKE, 0, 0)], [(0, 1)], [0]),
]


def template_ops(index, handle=0x1234, spare=2):
    name, layout, links, targets = TEMPLATES[index]
    ops = new_ops(layout)
    for i in range(len(layout), len(layout) + spare):
        ops.append(op(OP_NEW, i, K_FAKE))
    for a, b in links:
        ops.append(op(OP_LINK, a, b))
    for i, (kind, c, d) in enumerate(layout):
        if kind in (K_SAMPLE, K_COMPLETED, K_USER):
            ops.append(op(OP_POKE, i, 0, handle + i))
            ops.append(op(OP_POKE, i, 3, 0x5000 + i))
    return ops, targets, len(layout)


def chunk_edges():
    out = []
    sizes = [0, 1, 7, 64, 4096, -1, -0x80000000]
    for index in range(len(TEMPLATES)):
        for size in sizes:
            for flag in (0, 1):
                ops, targets, n = template_ops(index)
                # the fake delivers everything at once, partial chunks, nothing (answer 0 with 0 bytes), or finishes
                for script in ([FILL_STEP], [step(0, 3), FILL_STEP], [step(1, FILL_STEP)], [step(1, 3), FILL_STEP],
                               [step(0, 0), step(0, 0), step(1, 0), FILL_STEP], [step(1, size if size > 0 else 0)]):
                    q = {"Step": script, "Attach": [-1], "Clock": [0, 10 ** 6]}
                    out.append(case(ops + [op(OP_GET_CHUNK, targets[0], 0, size, flag), op(OP_GET_CHUNK, targets[-1], 16, size, flag)], q))
    # the attach: the callback attaches the fake of a spare slot (or nothing) behind the finished pipe
    for index in (0, 1, 2, 4):
        for attach in (-1, 3, 4, 2, 5, 0, 1, 99):
            ops, targets, n = template_ops(index, spare=3)
            for size in (64, 4096, 7):
                q = {"Step": [step(1, 5), step(0, 2), step(1, 9), FILL_STEP], "Attach": [attach, -1], "Clock": [0, 10 ** 6]}
                out.append(case(ops + [op(OP_GET_CHUNK, targets[0], 0, size, 1), op(OP_GET_CHUNK, targets[0], 4, size, 0)], q))
    # answers other than 0 and 1 do not finish a sample
    for result in (-1, 2, 5, 0x7FFFFFFF, -0x80000000):
        ops, targets, n = template_ops(0)
        out.append(case(ops + [op(OP_GET_CHUNK, 0, 0, 64, 1)], {"Step": [step(result, 10), step(result, 20), FILL_STEP]}))
    # negative bytes delivered, bytes beyond the request, and a request that is not reached
    for chunks in ([-3, 5, FILL_STEP], [100, 5], [0x7FFFFFFE, 1], [-300, -300, FILL_STEP], [64, 1], [63, 1, 1], [65]):
        ops, targets, n = template_ops(0)
        out.append(case(ops + [op(OP_GET_CHUNK, 0, 0, 64, 1)], {"Step": [step(0, c) if c != FILL_STEP else FILL_STEP for c in chunks]}))
        out.append(case(ops + [op(OP_GET_CHUNK, 0, 0, 64, 1)], {"Step": [step(1, c) if c != FILL_STEP else FILL_STEP for c in chunks]}))
    # sizes at the edge of the 32-bit range: the loop test is signed and the remaining size and the count wrap (the fake only,
    # so that no timer is asked for that many bytes)
    for size in (0x7FFFFFFF, 0x7FFFFFFE, -0x7FFFFFFF):
        for chunks in ([0x7FFFFFFE, 1], [-300, 0x7FFFFFFE, FILL_STEP], [-300, -300, 0x7FFFFFFF], [0x40000000, 0x3FFFFFFF, 1],
                       [0x7FFFFFFF], [-1, 0x7FFFFFFF]):
            ops, targets, n = template_ops(0)
            out.append(case(ops + [op(OP_GET_CHUNK, 0, 0, size, 1)],
                            {"Step": [step(0, c) if c != 0x7FFFFFFF else FILL_STEP for c in chunks] + [FILL_STEP]}))
            ops, targets, n = template_ops(3)
            out.append(case(ops + [op(OP_GET_CHUNK, 0, 0, size, 0)],
                            {"Step": [step(0, c) if c != 0x7FFFFFFF else FILL_STEP for c in chunks] + [FILL_STEP]}))
    # the timer: appears when the callback attaches nothing, takes the format byte of the old pipe and the position
    for fmt in (0x21, 0x41, 0x22, 0x42, 0x00, 0xFF):
        for done in (0, 5, 63, 0x7FFF):
            ops, targets, n = template_ops(0)
            out.append(case(ops + [op(OP_POKE, 1, 5, fmt), op(OP_GET_CHUNK, 0, 0, 0x1000, 1)],
                            {"Step": [step(1, done), FILL_STEP], "Attach": [-1], "Clock": [100, 5000]}))
    # the sample behind a pipe changes the handle and user data the callback is given
    for handle in (0, 1, 0x1234, MASK):
        for data in (0, 0xBEEF, MASK):
            ops, targets, n = template_ops(0)
            out.append(case(ops + [op(OP_POKE, 0, 0, handle), op(OP_POKE, 0, 3, data), op(OP_GET_CHUNK, 0, 0, 32, 1)],
                            {"Step": [step(1, 1), FILL_STEP], "Attach": [-1], "Clock": [0]}))
    # a timer behind: GetDataChunk of the timer finishes when the clock passes its time
    for clock in (0, 1, 50, 0x1000, 10 ** 6, MASK):
        ops, targets, n = template_ops(4)
        out.append(case(ops + [op(OP_GET_CHUNK, 0, 0, 64, 1), op(OP_GET_CHUNK, 0, 0, 0x1000, 1), op(OP_GET_CHUNK, 1, 0, 8, 0)],
                        {"Clock": [clock, 0, MASK], "Attach": [-1]}))
    # a NULL or a stray callback never gets called when nothing is behind
    out.append(case(new_ops([(K_SAMPLE, 3, 0)]) + [op(OP_GET_CHUNK, 0, 0, 64, 1)]))
    out.append(case(new_ops([(K_SAMPLE, 2, 0)]) + [op(OP_GET_CHUNK, 0, 0, 64, 1)]))
    return out


def next_edges():
    out = []
    for fmt in (0x21, 0x41, 0x42, 0x00, 0xFF, 0x24):
        for position in (0, 1, 5, 0x1000, 0x7FFFFFFF, 0x80000000, MASK):
            for attach in (-1, 3):
                for nxt in (K_FAKE, K_TIMER):
                    layout = [(K_SAMPLE, 0, 0), (nxt, 0x21, 50)]
                    ops = new_ops(layout) + [op(OP_NEW, 2, K_FAKE), op(OP_NEW, 3, K_FAKE), op(OP_LINK, 0, 1), op(OP_POKE, 0, 0, 0x4242),
                                             op(OP_POKE, 0, 3, 0x99), op(OP_POKE, 1, 5, fmt), op(OP_GET_NEXT, 0, position),
                                             op(OP_GET_NEXT, 0, position + 1)]
                    out.append(case(ops, {"Attach": [attach, -1], "Clock": [0, 7, 10 ** 6]}))
    return out


def chunk_random(r):
    index = r.randrange(len(TEMPLATES))
    spare = r.randrange(0, 4)
    ops, targets, n = template_ops(index, handle=r.choice([0, 0x1234, 7]), spare=spare)
    q = {"Attach": [r.choice([-1, -1, 2, 3, 4, 5, 0, 1]) for _ in range(r.randrange(1, 4))],
         "Clock": [r.choice([0, 10, 5000, 10 ** 6, r.getrandbits(32)]) for _ in range(r.randrange(1, 4))]}
    q["Step"] = [step(r.choice([0, 0, 0, 1, 1, -1, 2, 5]), r.choice([0, 1, 3, 7, 16, 64, 100, -3, -300, 5000, 0x7FFFFFFE]))
                 for _ in range(r.randrange(0, 6))] + [FILL_STEP]
    slots = list(range(n))
    for _ in range(r.randrange(1, 7)):
        c = r.random()
        slot = r.choice(slots)
        if c < 0.55:
            ops.append(op(OP_GET_CHUNK, r.choice(targets), r.choice([0, 0, 16, 0x800]), r.choice([0, 1, 7, 64, 4096, -1, 100]), r.randrange(2)))
        elif c < 0.62:
            ops.append(op(OP_POKE, slot, r.choice([0, 3, 4, 5, 6, 7, 8]), r.choice([0, 1, 2, 3, 0x21, 0x4242, MASK])))
        elif c < 0.7:
            ops.append(op(OP_GET_NEXT, slot, r.choice([0, 5, 1000, MASK])))
        elif c < 0.78:
            ops.append(op(OP_GUARD, slot, r.randrange(3)))
        elif c < 0.84:
            ops.append(op(OP_DTOR, r.choice(slots + list(range(n, n + spare)) if spare else slots), r.choice(FLAGS)))
        elif c < 0.92:
            ops.append(op(r.choice([OP_SET_INFO, OP_GET_INFO]), slot))
        else:
            ops.append(op(OP_NEW, r.randrange(SLOTS), r.choice([K_FAKE, K_TIMER, K_PIPE, K_USER]), r.choice([0x21, 0x41]), r.randrange(100)))
    return case(ops, q)


# ---------------------------------------------------------------- the guard

def guard_edges():
    out = []
    for kind, c, d in ((K_SAMPLE, 0, 0), (K_COMPLETED, 0, 1), (K_USER, 3, 0), (K_PIPE, 0, 0), (K_FAKE, 0, 0)):
        for mode in (0, 1, 2):
            # alone, behind another pipe (the previous one is linked to what is behind), and in front of one
            out.append(case([op(OP_NEW, 0, kind, c, d), op(OP_GUARD, 0, mode), op(OP_NEWOP, 0, P_USER, 0x1C), op(OP_NEW, 1, K_USER, 1, 0)]))
            out.append(case(new_ops([(K_PIPE, 0, 0), (kind, c, d), (K_FAKE, 0, 0)]) + [op(OP_LINK, 0, 1), op(OP_LINK, 1, 2),
                                                                                       op(OP_GUARD, 1, mode), op(OP_GUARD, 0, 0),
                                                                                       op(OP_POKE, 0, 0, 1)]))
            out.append(case(new_ops([(kind, c, d), (K_FAKE, 0, 0)]) + [op(OP_LINK, 0, 1), op(OP_GUARD, 0, mode), op(OP_GUARD, 1, mode)]))
    return out


def guard_random(r):
    layout = [(r.choice([K_SAMPLE, K_COMPLETED, K_USER, K_PIPE, K_FAKE]), r.choice([0, 1]), r.choice([0, 3])) for _ in range(3)]
    ops = new_ops(layout)
    for i in range(2):
        if r.random() < 0.7:
            ops.append(op(OP_LINK, i, i + 1))
    for _ in range(r.randrange(1, 5)):
        ops.append(op(OP_GUARD, r.randrange(3), r.randrange(3)))
        if r.random() < 0.3:
            ops.append(op(OP_NEW, r.randrange(SLOTS), r.choice([K_USER, K_SAMPLE, K_COMPLETED]), 0, 0))
    return case(ops)


# ---------------------------------------------------------------- mixed scripts

def mixed_random(r):
    c = r.random()
    if c < 0.4:
        return chunk_random(r)
    if c < 0.55:
        return info_random(r)
    if c < 0.7:
        return pool_script(r)
    if c < 0.85:
        return guard_random(r)
    return basic_script(r)


# ---------------------------------------------------------------- the vtables

VTABLES = {
    USER_VT: [0x100001B0, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x10013E4C, 0x1000118C, 0x10001148, 0x10001204, 0x10013D78,
              0x10000BB8, 0x1000F628, 0x1000123C],
    SAMPLE_VT: [0x1000D50C, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x1000D078, 0x1000118C, 0x1000D11C, 0x10001204, 0x1000CFA0,
                0x10000BB8, 0x1000F628, 0x1000123C],
    COMPLETED_VT: [0x10001018, 0x1000F440, 0x1000F588, 0x1000F834, 0x1000F7B0, 0x10013E4C, 0x1000118C, 0x100010BC, 0x10001204,
                   0x10013D78, 0x10000BB8, 0x1000F628, 0x1000123C],
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
    for name, ctor, args in (("user0", CTOR_USER0, []), ("user1", CTOR_USER1, [0x55]), ("sample", CTOR_SAMPLE, [0]),
                             ("completed", CTOR_COMPLETED, [0, 0x66])):
        block = e.alloc(0x40, b"\xa5" * 0x40)
        call(e, ctor, [block] + args)
        out[name] = e.u32(block)
        out[name + "_words"] = [e.u32(block + 4 * i) for i in range(1, 9)]
    return out


def stores_host(lib, c):
    expected = dict(user0=USER_VT, user1=USER_VT, sample=SAMPLE_VT, completed=COMPLETED_VT)
    words = dict(user0=[FILL, 0, 0, 0, FILL, 0, FILL, FILL], user1=[FILL, 0, 0, 0, FILL, 0x55, FILL, FILL],
                 sample=[FILL, 0, 0, 0, FILL, 0, FILL, 0], completed=[FILL, 0, 0, 0, FILL, 0x66, FILL, 0])
    out = {}
    for name, vt in expected.items():
        out[name] = vt
        out[name + "_words"] = words[name]
    return out


TESTS = [
    FnTest("pools: operator new, operator delete, TopUp, SetLevel and ClearCache of the user callback "
           "(0x10013ae0 0x10013bb0 0x10013c54 0x10013d38 0x10013a4c), the sample callback (0x1000cd00 0x1000cdd0 0x1000ce78 0x1000cf5c "
           "0x1000cc68) and the completed callback (0x1000f178 0x1000f254 0x1000f304 0x1000f3f4 0x1000f0d8)",
           ppc_case, host_case, edge=pool_edges, rnd=pool_script),
    FnTest("constructors 0x10000d20 0x100002c8 0x10000c94 0x10000114 and destructors 0x100001b0 0x1000d50c 0x10001018",
           ppc_case, host_case, edge=basic_edges, rnd=basic_script),
    FnTest("cleanup destructors 0x10013f2c 0x1000d608 0x1000f948", ppc_case, host_case, edge=cs_edges, rnd=pool_script),
    FnTest("SetInfo 0x10013d78 0x1000cfa0 and GetInfo 0x10013e4c 0x1000d078", ppc_case, host_case, edge=info_edges, rnd=info_random),
    FnTest("_TbSoundSampleCallback::GetDataChunk 0x1000d11c, GetNextSample 0x1000d380 and _TbSoundSampleCompletedCallback::GetDataChunk "
           "0x100010bc", ppc_case, host_case, edge=chunk_edges, rnd=chunk_random),
    FnTest("GetNextSample 0x1000d380 called directly", ppc_case, host_case, edge=next_edges, rnd=chunk_random),
    FnTest("CAutoDetach: constructor 0x1000d334, operator= 0x1000d26c, destructor 0x1000d29c", ppc_case, host_case, edge=guard_edges,
           rnd=guard_random),
    FnTest("mixed scripts over all of them", ppc_case, host_case, edge=lambda: [], rnd=mixed_random),
    FnTest("the vtables 0x1003c7d8 0x1003c1a8 0x10035bbc", vtable_ppc, vtable_host, edge=vtable_cases,
           rnd=lambda r: dict(vt=r.choice(list(VTABLES)))),
    FnTest("the vptr stores of the constructors", stores_ppc, stores_host, edge=lambda: [dict()], rnd=lambda r: dict()),
]
