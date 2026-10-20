"""Unit v51053, the position commands of CSprocketsModule.cpp, against libtpw_unit_SamsUtilities_UCommand3dPosition_SetOrientation.dylib
(tests/unit_SamsUtilities_UCommand3dPosition_SetOrientation.cpp), sound program, src/TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_
SamsUtilities_UCommand3dPosition_SetOrientation.cpp:
  UCommand3dPosition ctor 0x1002cd48, UCommandListenerPosition ctor 0x1002d1a0, UCommandPosition ctor 0x1002ccd0, UCommandPreroll ctor
  0x1002ceac, SetLocation 0x1002cbc8, SetVelocity 0x1002cc4c, SetOrientation 0x1002d420, SetUpVector 0x1002d39c, UStatus ctor 0x1002cf68,
  operator= 0x1002ce54, IsSet 0x1002cb7c, thirteen GetTag, Stereo/Headphones/Mono, SetSpeedOfSound 0x1002d034, SetListenerVelocity
  0x1002d4a4 and __sinit_CSprocketsModule_cpp 0x1002dd30.

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory, big-endian, and host memory,
little-endian). The callees outside the unit are fakes on both sides that record their arguments: UHasFlags SetFlag/ctor and
UCommand's base ctor (offset in the command), TbSoundPosition::Get (answers floats from the case), the channel send glue (records the
command's vtable word and velocity doubles), UStatus::IfOk, the pool queue ctor and __register_global_object (labels).

The world: a 128-byte command filled with random words, the pool flag bytes, a module with one channel (or none). Compared: the answer,
the command's words outside the doubles and the doubles' bits, the two flag bytes, the event log. No fused operations. Not tested: the
real bodies of the callees (their own units).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SamsUtilities_UCommand3dPosition_SetOrientation"
MASK = 0xFFFFFFFF
M64 = (1 << 64) - 1

# ---- the unit's own functions
CTORS = {0: 0x1002cd48, 1: 0x1002d1a0, 2: 0x1002ccd0, 3: 0x1002ceac}
SETTERS = {0: 0x1002cbc8, 1: 0x1002cc4c, 2: 0x1002d420, 3: 0x1002d39c}
TAGS = {0: 0x1002d884, 1: 0x1002d8d4, 2: 0x1002d928, 3: 0x1002d974, 4: 0x1002d9c0, 5: 0x1002da0c, 6: 0x1002da58, 7: 0x1002daa0,
        8: 0x1002daf4, 9: 0x1002db48, 10: 0x1002dc48, 11: 0x1002dc98, 12: 0x1002dce8}
TAG_VALUES = {0: 0x73706b72, 1: 0x6c697370, 2: 0x70726572, 3: 0x766f6c75, 4: 0x706f736e, 5: 0x70617573, 6: 0x73746f70, 7: 0x73726364,
              8: 0x6c697373, 9: 0x6370756c, 10: 0x7372636d, 11: 0x6c736e64, 12: 0x706c6179}
SPEAKERS = {0: 0x1002db94, 1: 0x1002dbd0, 2: 0x1002dc10}
F_USTATUS, F_ASSIGN, F_ISSET, F_SPEED, F_VELOCITY, F_SINIT = 0x1002cf68, 0x1002ce54, 0x1002cb7c, 0x1002d034, 0x1002d4a4, 0x1002dd30
OWN = set(CTORS.values()) | set(SETTERS.values()) | set(TAGS.values()) | set(SPEAKERS.values()) | \
    {F_USTATUS, F_ASSIGN, F_ISSET, F_SPEED, F_VELOCITY, F_SINIT, 0x100015b8}   # TbError::TbError runs as the original

# ---- callees
SETFLAG, FLAGS_CTOR, UCOMMAND_CTOR, GET, SEND, IFOK, QUEUE_CTOR, REGISTER = \
    0x1002a690, 0x1002a858, 0x1002c468, 0x10015f3c, 0x1002f154, 0x1002c144, 0x100040ac, 0x1002eefc

ADDR = dict(queueBuilt=0x1003e329, cleanupBuilt=0x1003e328, queue=0x1003e33c, cleanup=0x1003e32a, dtor=0x1003352c, node=0x1003f3b8)
HOST_LAYOUT = [0, 128, 128, 4]


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


def bitsd(b):
    return struct.unpack(">d", struct.pack(">Q", b & M64))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """The glue stores the caller's TOC at 0x14(r1) and the caller reloads it."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def off(e, p):
    base = e.marks[0]
    return s64(p - base) if base else 0


def stub_setflag(e):
    e.cl.rec("SetFlag", (off(e, e.r(3)), e.r(4) & MASK))


def stub_flags_ctor(e):
    e.cl.rec("UHasFlagsCtor", (off(e, e.r(3)),))


def stub_ucommand_ctor(e):
    e.cl.rec("UCommandCtor", (off(e, e.r(3)),))


def stub_get(e):
    L = e.cl
    L.rec("Get", (0,))
    for reg, key in ((4, "GetX"), (5, "GetY"), (6, "GetZ")):
        e.write(e.r(reg), struct.pack(">I", L.ret(key) & MASK))


def stub_send(e):
    L = e.cl
    cmd = e.r(5)
    vt = e.u32(cmd + 4)
    ds = [s64(struct.unpack(">Q", e.read(cmd + 0x24 + 8 * i, 8))[0]) for i in range(3)]
    L.rec("Send", (1 if e.r(4) == e.marks[1] else 0, vt, ds[0], ds[1], ds[2]))
    e.write(e.r(3), struct.pack(">I", L.ret("Send") & MASK))
    save_toc(e)


def stub_ifok(e):
    L = e.cl
    L.rec("IfOk", (s32(e.u32(e.r(3))), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("IfOk") & MASK)


def stub_queue_ctor(e):
    e.cl.rec("QueueCtor", (e.cl.lab(e.r(3)),))


def stub_register(e):
    L = e.cl
    L.rec("Register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    save_toc(e)


STUBS.internal(SETFLAG, stub_setflag)
STUBS.internal(FLAGS_CTOR, stub_flags_ctor)
STUBS.internal(UCOMMAND_CTOR, stub_ucommand_ctor)
STUBS.internal(GET, stub_get)
STUBS.internal(SEND, stub_send)
STUBS.internal(IFOK, stub_ifok)
STUBS.internal(QUEUE_CTOR, stub_queue_ctor)
STUBS.internal(REGISTER, stub_register)


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
    dfmt = ">Q"

    def __init__(self, e, case):
        self.e = e
        e.marks = (0, 0)
        fixed = {ADDR["cleanup"]: 1, ADDR["queue"]: 2, ADDR["dtor"]: 3, ADDR["node"]: 4}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def mark(self, base, chan):
        self.e.marks = (base, chan)

    def install_channel(self, mod, chan):
        arr = self.alloc(8)
        self.words(arr, [chan])
        self.words(mod + 0x14, [arr])

    def ctor(self, kind, obj, on):
        r3, _, _ = T.call_ppc(self.e, CTORS[kind], [obj, on & MASK])
        return (r3 - obj) & MASK

    def setter(self, kind, obj, x, y, z):
        T.call_ppc(self.e, SETTERS[kind], [obj, T.D(x), T.D(y), T.D(z)])
        return 0

    def tag(self, kind, obj):
        return T.call_ppc(self.e, TAGS[kind], [obj])[0] & MASK

    def speaker(self, kind, obj):
        return T.call_ppc(self.e, SPEAKERS[kind], [obj])[0] & MASK

    def isset(self, obj):
        return T.call_ppc(self.e, F_ISSET, [obj])[0] & 0xFF

    def ustatus(self, obj):
        T.call_ppc(self.e, F_USTATUS, [obj])
        return 0

    def assign(self, dst, src):
        T.call_ppc(self.e, F_ASSIGN, [dst, src])
        return 0

    def speed(self, mod, f):
        buf = self.alloc(4)
        self.words(buf, [0x55555555])
        T.call_ppc(self.e, F_SPEED, [buf, mod, T.F(f)])
        return s32(self.rd_word(buf))

    def velocity(self, mod, pos):
        buf = self.alloc(4)
        self.words(buf, [0x55555555])
        T.call_ppc(self.e, F_VELOCITY, [buf, mod, pos])
        return s32(self.rd_word(buf))

    def sinit(self):
        T.call_ppc(self.e, F_SINIT, [])
        return 0


class Host(Side):
    fmt = "<I"
    dfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("cleanup"), 2: self.g("queue"), 3: self.g("dtor"), 4: self.g("node")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ssc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ssc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def mark(self, base, chan):
        self.write(self.g("markBase"), struct.pack("<Q", base))
        self.write(self.g("markChan"), struct.pack("<Q", chan))

    def install_channel(self, mod, chan):
        self.lib.tpw_ssc_setchan(chan)

    def ctor(self, kind, obj, on):
        return (self.lib.tpw_ssc_ctor(kind, obj, on & MASK) - obj) & MASK

    def setter(self, kind, obj, x, y, z):
        self.lib.tpw_ssc_setter(kind, obj, x, y, z)
        return 0

    def tag(self, kind, obj):
        return self.lib.tpw_ssc_tag(kind, obj) & MASK

    def speaker(self, kind, obj):
        return self.lib.tpw_ssc_speaker(kind, obj) & MASK

    def isset(self, obj):
        return self.lib.tpw_ssc_isset(obj) & 0xFF

    def ustatus(self, obj):
        self.lib.tpw_ssc_ustatus(obj)
        return 0

    def assign(self, dst, src):
        self.lib.tpw_ssc_assign(dst, src)
        return 0

    def speed(self, mod, f):
        return self.lib.tpw_ssc_speed(mod, f)

    def velocity(self, mod, pos):
        return self.lib.tpw_ssc_velocity(mod, pos)

    def sinit(self):
        self.lib.tpw_ssc_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ssc_addr.restype, lib.tpw_ssc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ssc_alloc.restype, lib.tpw_ssc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ssc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ssc_setchan.argtypes = [C.c_uint64]
    lib.tpw_ssc_ctor.restype, lib.tpw_ssc_ctor.argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_uint32]
    lib.tpw_ssc_setter.restype, lib.tpw_ssc_setter.argtypes = None, [C.c_int32, C.c_uint64, C.c_double, C.c_double, C.c_double]
    lib.tpw_ssc_tag.restype, lib.tpw_ssc_tag.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    lib.tpw_ssc_speaker.restype, lib.tpw_ssc_speaker.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    lib.tpw_ssc_isset.restype, lib.tpw_ssc_isset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ssc_ustatus.restype, lib.tpw_ssc_ustatus.argtypes = None, [C.c_uint64]
    lib.tpw_ssc_assign.restype, lib.tpw_ssc_assign.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_ssc_speed.restype, lib.tpw_ssc_speed.argtypes = C.c_int32, [C.c_uint64, C.c_float]
    lib.tpw_ssc_velocity.restype, lib.tpw_ssc_velocity.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    lib.tpw_ssc_sinit.restype, lib.tpw_ssc_sinit.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ssc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.obj = s.alloc(128)
    s.words(s.obj, case["fill"])
    s.obj2 = s.alloc(16)
    s.words(s.obj2, case["fill2"][:4])
    s.write(s.g("queueBuilt"), bytes([case["flags"][0]]))
    s.write(s.g("cleanupBuilt"), bytes([case["flags"][1]]))


def execute(s, case):
    op, a = case["op"], case["args"]
    obj = s.obj
    if op == "ctor":
        s.mark(obj, 0)
        return s.ctor(a[0], obj, a[1])
    if op == "setter":
        s.mark(obj, 0)
        return s.setter(a[0], obj, a[1], a[2], a[3])
    if op == "tag":
        return s.tag(a[0], obj)
    if op == "speaker":
        return s.speaker(a[0], obj)
    if op == "isset":
        return s.isset(obj)
    if op == "ustatus":
        return s.ustatus(obj)
    if op == "assign":
        return s.assign(obj, s.obj2)
    if op == "sinit":
        return s.sinit()
    mod = s.alloc(0x18)
    chan = 0
    if not case["nochan"]:
        chan = s.alloc(0x40)
        s.words(chan + 0x10, [s.alloc(0x40)])
    s.install_channel(mod, chan)
    if op == "speed":
        return s.speed(mod, bitsf(a[0]))
    s.mark(0, chan)
    pos = s.alloc(16)
    return s.velocity(mod, pos)


def obj_state(s, fill):
    """The doubles' slots: 'same' while the two words still hold the fill, else the double's bits (the fill's words read as a double
    differ between the byte orders, a written double does not)."""
    raw = s.read(s.obj, 128)
    words = list(struct.unpack(s.fmt[0] + "32I", raw))
    keep = words[0:3] + words[27:32]
    doubles = []
    for i in range(12):
        if words[3 + 2 * i:5 + 2 * i] == [w & MASK for w in fill[3 + 2 * i:5 + 2 * i]]:
            doubles.append("same")
        else:
            doubles.append(struct.unpack(s.dfmt, raw[0xC + 8 * i:0xC + 8 * i + 8])[0])
    return keep, doubles, s.rd_words(s.obj2, 4)


def observe(s, case, answer):
    keep, doubles, second = obj_state(s, case["fill"])
    return {"answer": answer, "log": s.log(), "words": keep, "doubles": doubles, "second": second,
            "flags": [s.read(s.g("queueBuilt"), 1)[0], s.read(s.g("cleanupBuilt"), 1)[0]]}


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

DSPECIAL = [0, 1 << 63, 0x7FF0000000000000, 0xFFF0000000000000, 0x7FF8000000000000, 0x3FF0000000000000, 0xBFF0000000000000, 1]
FSPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rd(r):
    k = r.random()
    if k < 0.2:
        return bitsd(r.choice(DSPECIAL))
    if k < 0.7:
        return r.uniform(-1000, 1000)
    return r.uniform(-1, 1) * 10 ** r.uniform(-300, 300)


def rf(r):
    k = r.random()
    if k < 0.2:
        return r.choice(FSPECIAL)
    return struct.unpack(">I", struct.pack(">f", r.uniform(-500, 500)))[0]


def base(r, op, args=()):
    return {"op": op, "args": list(args), "nochan": False,
            "fill": [r.getrandbits(32) for _ in range(32)], "fill2": [r.getrandbits(32) for _ in range(32)],
            "flags": [r.choice([0, 1, 0x80, 0xFF]), r.choice([0, 1, 0x80, 0xFF])],
            "queues": {"GetX": [rf(r)], "GetY": [rf(r)], "GetZ": [rf(r)], "Send": [r.choice([0, 1, -1, 5]) & MASK],
                       "IfOk": [r.choice([0, -1, 7]) & MASK]}}


def make_test(name, make):
    """make(r, i) -> a case; i is None for a random case, else the edge index."""
    def edges():
        r = random.Random(sum(map(ord, name)))
        for i in range(6):
            yield make(r, i)

    def rnd(r):
        return make(r, None)
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


def mk_ctor(kind):
    return lambda r, i: base(r, "ctor", (kind, [0, 1, 0xFFFFFFFF, 0x80000000, 5, 99][i] if i is not None else r.getrandbits(32)))


def mk_setter(kind):
    def f(r, i):
        if i is not None and i < 4:
            v = [(0.0, 0.0, 0.0), (-0.0, 1.0, -1.0), (float("inf"), float("-inf"), 1e300), (1.5, 2.5, 3.5)][i]
            return base(r, "setter", (kind,) + v)
        return base(r, "setter", (kind, rd(r), rd(r), rd(r)))
    return f


def mk_simple(op, *args):
    return lambda r, i: base(r, op, args)


def mk_speed(r, i):
    c = base(r, "speed", (rf(r),))
    return c


def mk_velocity(r, i):
    c = base(r, "velocity")
    c["nochan"] = i in (0, 1)
    return c


def mk_sinit(r, i):
    c = base(r, "sinit")
    if i is not None and i < 6:
        c["flags"] = [[0, 0], [1, 0], [0, 1], [0x80, 0x80], [0xFF, 0], [0, 0xFF]][i]
    return c


TAG_NAMES = ["UCommandSpeakerType", "UCommandListenerPosition", "UCommandPreroll", "UCommandVolume", "UCommandPosition", "UCommandPause",
             "UCommandStop", "UCommandSoundDistance", "UCommandListenerScale", "UCommandCPULoad", "UCommandSoundType",
             "UCommandLoadSound", "UCommandPlay"]

TESTS = [
    make_test("SamsUtilities::UCommand3dPosition::SetOrientation", mk_setter(2)),
    make_test("SamsUtilities::UCommand3dPosition::SetUpVector", mk_setter(3)),
    make_test("SamsUtilities::UCommand3dPosition::UCommand3dPosition", mk_ctor(0)),
    make_test("SamsUtilities::UCommand3dPosition::SetVelocity", mk_setter(1)),
    make_test("SamsUtilities::UCommand3dPosition::SetLocation", mk_setter(0)),
    make_test("CSprocketsModule::SetListenerVelocity", mk_velocity),
    make_test("SamsUtilities::UCommandListenerPosition::UCommandListenerPosition", mk_ctor(1)),
    make_test("CSprocketsModule::SetSpeedOfSound", mk_speed),
    make_test("SamsUtilities::UStatus::UStatus", mk_simple("ustatus")),
    make_test("SamsUtilities::UCommandPreroll::UCommandPreroll", mk_ctor(3)),
    make_test("SamsUtilities::UStatus::operator=", mk_simple("assign")),
    make_test("SamsUtilities::UCommandPosition::UCommandPosition", mk_ctor(2)),
    make_test("CSprocketsSpeaker::Stereo", mk_simple("speaker", 0)),
    make_test("CSprocketsSpeaker::Headphones", mk_simple("speaker", 1)),
    make_test("CSprocketsSpeaker::Mono", mk_simple("speaker", 2)),
    make_test("SamsUtilities::UStatus::IsSet", mk_simple("isset")),
    make_test("__sinit_CSprocketsModule_cpp", mk_sinit),
] + [make_test("SamsUtilities::%s::GetTag" % TAG_NAMES[k], mk_simple("tag", k)) for k in range(13)]
