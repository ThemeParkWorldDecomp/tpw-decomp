"""Unit v5L24, CSprocketsModule::Init 0x1002b408 (sound symbols shared), against libtpw_unit_CSprocketsModule_Init.dylib
(tests/unit_CSprocketsModule_Init.cpp), src/CSprocketsModule_Init.cpp.

A case is a module, a TbSoundSystemInfo and the answers of the fakes. The base's Init (TbSoundSystemModule::Init, 0x10012340 and
src/TbSoundSystemModule.cpp) and TbError::TbError (0x100015b8) run as themselves on both sides, so the info's mask, maximum and
buffers-to-use words decide mBuffers. The callees outside the unit are fakes on both sides that record their arguments: the
array allocation (0x1002ebfc, AllocBuffer_; recorded in 4-byte slots), the sound library's allocator (0x1002eac4; the size), the
channel constructor (0x1002a29c; the PowerPC fake also gives the channel a vtable for the virtual send), the three command
constructors (labels and the doubles' bits), SetImmediate (0x1002a5c4), the channel send and the virtual SetSpeakerConfig (both
through the glue 0x1002f154: with the module at r4 it is SetSpeakerConfig, otherwise the send), TbSpeaker::Stereo's constructor
(0x1002b65c), SetPoolKeepFull (0x1002b5ec) and Tick (0x1002b8c4). Both allocators can answer NULL from the case's queues.

Compared: the answer (the hidden r3 buffer on the Mac), the event log, the module's words (mBuffers, +8, +0xc), the array's label
and the label of each of its mBuffers slots (a slot left by a NULL channel keeps the allocation's fill and has no label). No fused
operations. Not tested: an mBuffers of 0x40000000 or more (the Mac's array size wraps in 32 bits, the host's in its own).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CSprocketsModule_Init"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1

F_INIT = 0x1002b408
F_BASE_INIT, F_TBERROR = 0x10012340, 0x100015b8
ALLOC_BUFFER, ALLOC, CHANNEL_CTOR, DISTANCE, SCALE, CPULOAD, SETIMMEDIATE, GLUE, STEREO, KEEPFULL, TICK = \
    0x1002ebfc, 0x1002eac4, 0x1002a29c, 0x1002b838, 0x1002b788, 0x1002b700, 0x1002a5c4, 0x1002f154, 0x1002b65c, 0x1002b5ec, \
    0x1002b8c4

HOST_LAYOUT = [24, 8]   # sizeof(CSprocketsModule) on the host, the array's slot
MAX_SLOTS = 64
CUR = {"module": 0, "vt": 0}


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update({F_INIT, F_BASE_INIT, F_TBERROR})


def save_toc(e):
    """Cross-TOC calls: the caller reloads its TOC from 0x14(r1)."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def fill(e, size):
    p = e.alloc(max(size, 8))
    e.write(p, b"\xa5" * size)
    return p


def fpr_bits(e, i):
    return s64(e.uc.reg_read(ppcemu.FPR[i]))


def stub_alloc_buffer(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("AllocBuffer", (size >> 2 if size % 4 == 0 else 0x10000000 | size,))
    if L.ret("AllocBufferNull"):
        e.set_r(3, 0)
    else:
        p = fill(e, size)
        L.newlab(p)
        e.set_r(3, p)
    save_toc(e)


def stub_alloc(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("Alloc", (size,))
    if L.ret("AllocNull"):
        e.set_r(3, 0)
    else:
        p = fill(e, size)
        L.newlab(p)
        e.set_r(3, p)
    save_toc(e)


def stub_channel_ctor(e):
    ch = e.r(3)
    e.cl.rec("ChannelCtor", (e.cl.lab(ch),))
    e.write(ch + 0x10, struct.pack(">I", CUR["vt"]))


def stub_distance(e):
    e.cl.rec("SoundDistance", (e.cl.newlab(e.r(3)), fpr_bits(e, 1)))


def stub_scale(e):
    e.cl.rec("ListenerScale", (e.cl.newlab(e.r(3)), fpr_bits(e, 1), fpr_bits(e, 2), fpr_bits(e, 3)))


def stub_cpuload(e):
    e.cl.rec("CPULoad", (e.cl.newlab(e.r(3)), fpr_bits(e, 1)))


def stub_glue(e):
    L = e.cl
    if e.r(4) == CUR["module"]:
        L.rec("SetSpeakerConfig", (L.lab(e.r(5)),))
        e.write(e.r(3), struct.pack(">I", L.ret("Config") & MASK))
    else:
        L.rec("Send", (L.lab(e.r(4)), L.lab(e.r(5))))
        e.write(e.r(3), struct.pack(">I", L.ret("Send") & MASK))
    save_toc(e)


def stub_stereo(e):
    e.cl.rec("Stereo", (e.cl.newlab(e.r(3)),))


STUBS.internal(ALLOC_BUFFER, stub_alloc_buffer)
STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(CHANNEL_CTOR, stub_channel_ctor)
STUBS.internal(DISTANCE, stub_distance)
STUBS.internal(SCALE, stub_scale)
STUBS.internal(CPULOAD, stub_cpuload)
STUBS.internal(SETIMMEDIATE, CL.recorder("SetImmediate", "Lb"))
STUBS.internal(GLUE, stub_glue)
STUBS.internal(STEREO, stub_stereo)
STUBS.internal(KEEPFULL, CL.recorder("SetPoolKeepFull", "b"))
STUBS.internal(TICK, CL.recorder("Tick"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


def slots_of(case):
    n = case["max"] if case["mask"] & 0x80 else case["use"]
    return min(n, MAX_SLOTS)


def ppc(e, case):
    L = e.cl = CL.PpcLog({}, case["queues"])
    saved = STUBS.activate(e)
    try:
        m = e.alloc(0x18)
        CUR["module"] = m
        CUR["vt"] = e.alloc(0x40)
        e.write(m, struct.pack(">6I", e.alloc(0x80), case["pre"], case["f008"], case["rate"], 0, 0x5a5a5a5a))
        info = e.alloc(0x4c)
        words = list(case["info"])
        words[3], words[4], words[5], words[6], words[7] = case["hi"], case["lo"], case["max"], case["mask"], case["use"]
        e.write(info, struct.pack(">19I", *[w & MASK for w in words]))
        buf = e.alloc(4)
        e.write(buf, struct.pack(">I", 0x55555555))
        T.call_ppc(e, F_INIT, [buf, m, info])
        answer = s32(e.u32(buf))
        buffers, f008, rate = e.u32(m + 4), e.u32(m + 8), e.u32(m + 0xc)
        arr = e.u32(m + 0x14)
        slots = [L.lab(e.u32(arr + 4 * i)) if arr else 0 for i in range(slots_of(case))]
        return {"answer": answer, "log": L.log, "module": [buffers, f008, rate], "array": L.lab(arr), "slots": slots}
    finally:
        CL.Stubs.deactivate(e, saved)
        CUR["module"] = 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u32 = C.c_uint32
    lib.tpw_csi_layout.restype, lib.tpw_csi_layout.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_csi_reset.restype, lib.tpw_csi_reset.argtypes = None, [u32, u32, u32, C.c_uint64]
    lib.tpw_csi_state.restype, lib.tpw_csi_state.argtypes = None, [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_csi_Init.restype, lib.tpw_csi_Init.argtypes = C.c_int32, [u32, u32, u32, u32, u32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_csi_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def host(lib, case):
    bind(lib)
    lib.tpw_csi_reset(case["pre"], case["f008"], case["rate"], 0x5a5a5a5a5a5a5a5a)
    CL.host_begin(lib, {}, case["queues"])
    answer = lib.tpw_csi_Init(case["mask"], case["max"], case["use"], case["hi"], case["lo"])
    log = CL.host_end(lib)
    n = slots_of(case)
    out = (C.c_int64 * (4 + MAX_SLOTS))()
    lib.tpw_csi_state(n, out)
    v = list(out)
    return {"answer": answer, "log": log, "module": [v[0] & MASK, v[1] & MASK, v[2] & MASK], "array": v[3],
            "slots": v[4:4 + n]}


# ---------------------------------------------------------------------------------------------------------------- the cases

DOUBLES = [0x3FF0000000000000, 0, 0x8000000000000000, 0x7FF0000000000000, 0xFFF0000000000000, 0x7FF8000000000000,
           0x7FF0000000000001, 0xFFFFFFFFFFFFFFFF, 0x0000000000000001, 0x3FE0000000000000, 0x4059000000000000]


def double_bits(r):
    k = r.random()
    if k < 0.3:
        return r.choice(DOUBLES)
    if k < 0.5:
        return r.randrange(0, 1 << 64)
    return struct.unpack(">Q", struct.pack(">d", r.uniform(0, 2)))[0]


def count(r):
    k = r.random()
    if k < 0.1:
        return 0
    if k < 0.85:
        return r.randrange(1, 9)
    return r.randrange(9, 40)


def make_case(r, n=None, array_null=None, nulls=None, load=None):
    """n: the module's channel count (through the info's mask and the chosen word); nulls: the channels the allocator refuses."""
    c = {}
    n = count(r) if n is None else n
    big = r.random() < 0.5
    other = count(r)
    c["mask"] = (r.randrange(0, 1 << 32) | 0x80) if big else (r.randrange(0, 1 << 32) & ~0x80)
    c["max"], c["use"] = (n, other) if big else (other, n)
    bits = double_bits(r) if load is None else load
    c["hi"], c["lo"] = bits >> 32, bits & MASK
    c["info"] = [r.randrange(0, 1 << 32) for _ in range(19)]
    c["pre"], c["f008"], c["rate"] = r.randrange(0, 1 << 32), r.randrange(0, 1 << 32), r.randrange(0, 1 << 32)
    if nulls is None:
        p = r.choice([0.0, 0.0, 0.2, 0.5, 1.0])
        nulls = [i for i in range(n) if r.random() < p]
    if array_null is None:
        array_null = r.random() < 0.08
    c["queues"] = {"AllocBufferNull": [1 if array_null else 0],
                   "AllocNull": [1 if i in nulls else 0 for i in range(n)] or [0],
                   "Send": [r.choice([0, 1, -1, r.randrange(-100, 100)]) & MASK for _ in range(2 * n + 2)],
                   "Config": [r.choice([0, -1, r.randrange(-100, 100)]) & MASK]}
    return c


def edges():
    r = random.Random(0x1002b408)
    for n in (0, 1, 2, 3, 5, 12):
        yield make_case(r, n=n, array_null=False, nulls=[])
        yield make_case(r, n=n, array_null=True, nulls=[])
        yield make_case(r, n=n, array_null=False, nulls=list(range(n)))
        yield make_case(r, n=n, array_null=False, nulls=[0])
        yield make_case(r, n=n, array_null=False, nulls=[0, 1])
        yield make_case(r, n=n, array_null=False, nulls=[1])
        yield make_case(r, n=n, array_null=False, nulls=[n - 1])
    for bits in DOUBLES:
        yield make_case(r, n=2, array_null=False, nulls=[], load=bits)


TESTS = [FnTest("CSprocketsModule::Init", ppc, host, edge=edges, rnd=make_case)]
