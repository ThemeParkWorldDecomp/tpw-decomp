"""Unit v5739, CServiceQueue, against libtpw_unit_CServiceQueue_CServiceQueue.dylib (tests/unit_CServiceQueue_CServiceQueue.cpp), program
"sound symbols shared", src/sound/ServiceQueue.cpp:
  CServiceQueue::CServiceQueue 0x10007410, ~CServiceQueue 0x1000726c, Close 0x10007514, Poll 0x10007590

A case is a world and a call. The queue object is built twice by ONE piece of Python code: in emulator memory (big-endian, 4-byte
pointers) and in the host library's memory (little-endian, 8-byte pointers); the constructor itself builds it (the callee
CAutoArrayPtr<void*> runs as the original on the PowerPC side), then the case writes the flags and the event handles into the array.
The callees outside the unit are fakes on both sides that record the same events: the wait (0x1002edc4, answers from a queue), set event
(0x1002eddc), free event (0x1002edac), free storage (0x1002ea94, the deleting destructor) and TbSoundPipeChannel::FillServiceBuffer
(0x1000a408, logged as the channel index: the Mac steps 0x60 bytes per channel, the host its own sizeof).

What is compared: the event log, the object's fields (channel pointer by equality, counts, flags, array pointer non-null, the vptr set,
the event handles in the array), after the constructor, Close and Poll. After the destructor only the log (the memory is released).
No fused operations. Not tested: a NULL this for the destructor (the host `delete` never calls it), the array contents after the
constructor (the original does not initialise them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_CServiceQueue_CServiceQueue"
MASK = 0xFFFFFFFF

CT, DT, CLOSE, POLL = 0x10007410, 0x1000726c, 0x10007514, 0x10007590
SIZES = {CT: 88, DT: 168, CLOSE: 80, POLL: 144}
OWN = set(SIZES)

WAIT, SETEV, FREEEV, FREE, FILL = 0x1002edc4, 0x1002eddc, 0x1002edac, 0x1002ea94, 0x1000a408
MAC_VPTR = 0x1003abf0 + 0x1120

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {"obj": 0, "arr": 0, "chan": 0}


def stub_wait(e):
    L = e.cl
    L.rec("Wait", (e.r(3), 1 if e.r(4) == CUR["arr"] else 0, e.r(5), e.r(6)))
    e.set_r(3, L.ret("Wait") & MASK)


def stub_set(e):
    e.cl.rec("Set", (e.r(3),))


def stub_freeev(e):
    e.cl.rec("EventFree", (e.r(3),))


def stub_free(e):
    e.cl.rec("Free", (1 if e.r(3) == CUR["obj"] else 0,))


def stub_fill(e):
    e.cl.rec("Fill", ((e.r(3) - CUR["chan"]) // 0x60,))


def stub_arr_dtor(e):
    """~CAutoArrayPtr<void*> (0x10007340): the original needs the runtime's array allocator, which the harness does not model; the
    host runs the header's inline template (delete[]), which no fake can observe, so neither side logs it."""


STUBS.internal(0x10007340, stub_arr_dtor)
STUBS.internal(WAIT, stub_wait)
STUBS.internal(SETEV, stub_set)
STUBS.internal(FREEEV, stub_freeev)
STUBS.internal(FREE, stub_free)
STUBS.internal(FILL, stub_fill)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & self.mask) for w in ws))

    def rd(self, a):
        return struct.unpack(self.fmt, self.read(a, self.psz))[0]

    def rd_w(self, a):
        return struct.unpack(self.fmt32, self.read(a, 4))[0]


class Ppc(Side):
    fmt, fmt32, psz, mask = ">I", ">I", 4, MASK
    off = dict(chan=4, slots=8, last=0xC, items=0x10, flags=0x14, size=0x18)

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

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

    def prepare(self, obj, chan):
        CUR["obj"], CUR["chan"] = obj, chan
        CUR["arr"] = self.rd(obj + self.off["items"])

    def ctor(self, obj, chan, last):
        CUR["obj"] = obj
        T.call_ppc(self.e, CT, [obj, chan, last & MASK])

    def close(self, obj):
        T.call_ppc(self.e, CLOSE, [obj])

    def poll(self, obj):
        T.call_ppc(self.e, POLL, [obj])

    def dtor(self, obj, flag):
        T.call_ppc(self.e, DT, [obj, flag & MASK])

    def vptr_ok(self, obj):
        return self.rd_w(obj) == MAC_VPTR


class Host(Side):
    fmt, fmt32, psz, mask = "<Q", "<I", 8, (1 << 64) - 1
    off = None

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        Host.off = lib.off
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cq_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def prepare(self, obj, chan):
        self.lib.tpw_cq_prepare(obj)

    def ctor(self, obj, chan, last):
        self.lib.tpw_cq_ctor(obj, chan, last & MASK)

    def close(self, obj):
        self.lib.tpw_cq_close(obj)

    def poll(self, obj):
        self.lib.tpw_cq_poll(obj)

    def dtor(self, obj, flag):
        self.lib.tpw_cq_dtor(obj, T.s32(flag))

    def vptr_ok(self, obj):
        return self.lib.tpw_cq_vptr(obj) != 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cq_alloc.restype, lib.tpw_cq_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cq_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cq_ctor.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_cq_prepare.argtypes = [C.c_uint64]
    lib.tpw_cq_close.argtypes = [C.c_uint64]
    lib.tpw_cq_poll.argtypes = [C.c_uint64]
    lib.tpw_cq_dtor.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_cq_vptr.restype, lib.tpw_cq_vptr.argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cq_layout(out)
    v = list(out)[:n]
    lib.off = dict(chan=v[0], slots=v[1], last=v[2], items=v[3], flags=v[4], size=v[5])
    lib.bound = True


def build(s, case):
    chan = s.alloc(16)
    obj = s.alloc(s.off["size"])
    s.ctor(obj, chan, case["last"])
    if case["op"] != "ctor":
        s.words(obj + s.off["flags"], [case["flags"]])
        arr = s.rd(obj + s.off["items"])
        s.words(arr, case["items"])
        s.prepare(obj, chan)
    return obj, chan


def observe(s, case, obj, chan):
    o = s.off
    d = {"log": s.log()}
    if case["op"] == "dtor":
        return d
    d.update(
        chan=s.rd(obj + o["chan"]) == chan,
        slots=s.rd_w(obj + o["slots"]),
        last=s.rd_w(obj + o["last"]),
        flags=s.rd_w(obj + o["flags"]),
        arr_set=s.rd(obj + o["items"]) != 0,
        vptr=s.vptr_ok(obj),
    )
    if case["op"] != "ctor":
        arr = s.rd(obj + o["items"])
        d["items"] = [s.rd(arr + s.psz * i) for i in range(case["last"] + 1)]
    return d


def run(s, case):
    try:
        obj, chan = build(s, case)
        if case["op"] == "close":
            s.close(obj)
        elif case["op"] == "poll":
            s.poll(obj)
        elif case["op"] == "dtor":
            s.dtor(obj, case["flag"])
        return observe(s, case, obj, chan)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def case_for(r, op, last=None, flags=None, flag=None, waits=None):
    last = r.choice([0, 1, 2, 3, 7, 12]) if last is None else last
    items = [r.randrange(1, 1 << 32) for _ in range(last + 1)] if last < 100 else []
    if flags is None:
        flags = r.choice([0, 1, 2, 3, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF, 4, r.randrange(0, 1 << 32)])
    if waits is None:
        waits = [r.randrange(0, last + 1) for _ in range(r.randrange(0, 5))] + [last]
    return {"op": op, "last": last, "items": items, "flags": flags,
            "flag": r.choice([0, 1, -1, 0x7FFF, -0x8000, 0x12340001, 0x12340000, 5]) if flag is None else flag,
            "queues": {"Wait": waits}}


def ctor_edges():
    r = random.Random(11)
    for last in (0, 1, 2, 5, 31, 0xFFFFFFFF):
        yield case_for(r, "ctor", last)


def ctor_random(r):
    return case_for(r, "ctor", r.randrange(0, 40))


def close_edges():
    r = random.Random(12)
    for last in (0, 1, 2, 9):
        for flags in (0, 1, 2, 3, 0xFFFFFFFF):
            yield case_for(r, "close", last, flags)


def close_random(r):
    return case_for(r, "close")


def poll_edges():
    r = random.Random(13)
    for flags in (0, 1, 2, 3, 0xFFFFFFFC, 0xFFFFFFFF):
        yield case_for(r, "poll", 3, flags, waits=[3])
        yield case_for(r, "poll", 3, flags, waits=[0, 3])
        yield case_for(r, "poll", 3, flags, waits=[2, 1, 0, 2, 3])
    yield case_for(r, "poll", 0, 0, waits=[0])
    yield case_for(r, "poll", 1, 0, waits=[0, 0, 1])


def poll_random(r):
    return case_for(r, "poll")


def dtor_edges():
    r = random.Random(14)
    for last in (0, 1, 4):
        for flag in (0, 1, -1, 0x7FFF, -0x8000, 0x12340001, 0x12340000):
            yield case_for(r, "dtor", last, flag=flag)


def dtor_random(r):
    return case_for(r, "dtor")


TESTS = [
    FnTest("CServiceQueue::CServiceQueue", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("CServiceQueue::Close", ppc, host, edge=close_edges, rnd=close_random),
    FnTest("CServiceQueue::Poll", ppc, host, edge=poll_edges, rnd=poll_random),
    FnTest("CServiceQueue::~CServiceQueue", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
