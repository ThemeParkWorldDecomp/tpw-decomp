"""Unit v5888, CThoughts (thought bubble and event history), against libtpw_unit_CThoughts_Serialise.dylib
(tests/unit_CThoughts_Serialise.cpp), main app ("SimTheme Park"), src/game/people/Thoughts.cpp:
  Serialise 0x1012ede4, CThoughts 0x1012ec88, ProcessThoughtBubbles 0x1012f058, ShowThoughtBubble 0x1012f0d8,
  RemoveThoughtBubble 0x1012f35c, RecordEvent 0x1012f3d4, GetRecentEvent 0x1012f4ec

A case is a world and a call. The object (0x90 bytes) is built twice by one piece of Python code. The callees outside the unit
(SAVEVAR/LOADVAR family, CTRACE, GAME_ASSERTMSG, SCR_KillScript, ESP_SpawnScript, __construct_array) are fakes on both sides
that record their arguments (value pointers as offsets from the object).

Compared: the answer, the whole object, the event log. Not tested: the CGameType constructor path of the first-use guard
(the host static is built once, the guard byte is always set on the PowerPC side); GetRecentEvent's result is compared as an
offset from the object.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CThoughts_Serialise"
MASK = 0xFFFFFFFF

F_SER, F_CT, F_PROC, F_SHOW, F_REM, F_REC, F_GET = (0x1012EDE4, 0x1012EC88, 0x1012F058, 0x1012F0D8, 0x1012F35C, 0x1012F3D4,
                                                     0x1012F4EC)
SIZES = {F_SER: 580, F_CT: 152, F_PROC: 72, F_SHOW: 568, F_REM: 68, F_REC: 168, F_GET: 136}
OWN = set(SIZES)

ADDR = dict(fpflags=0x102B5A40, gt=0x102B3AA0, guard=0x102B3AA4, gref=0x102A460C)
NAME0 = 0x10246836
FILE = 0x20001000
SCRIPT0 = 0x1029FB20
CTORVEC = 0x10266CC0
TURN_OFF = 0x1DA70C
HOST_LAYOUT = [0x1DA70C, 0x18]
STATE = {}

BLOCKS = [0x1012F180, 0x1012F18C, 0x1012F198, 0x1012F1A4, 0x1012F1B0, 0x1012F1BC, 0x1012F1C8, 0x1012F1D4, 0x1012F1E0,
          0x1012F1EC, 0x1012F1F8, 0x1012F204, 0x1012F210, 0x1012F21C, 0x1012F228, 0x1012F234, 0x1012F240, 0x1012F24C,
          0x1012F258, 0x1012F264, 0x1012F270, 0x1012F27C]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ser_stub(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4) - L.base), s32(e.r(5) - NAME0), s32(e.r(6))))
        e.set_r(3, L.ret("ser") & MASK)
    return f


for _addr, _name in ((0x100F45BC, "SAVEVAR<Ui>"), (0x100F44A4, "LOADVAR<Ui>"), (0x1000F384, "SAVEVAR<l>"), (0x1000EE04, "LOADVAR<l>"),
                     (0x1000F5DC, "SAVEVAR<Ul>"), (0x1000F02C, "LOADVAR<Ul>"), (0x1012F844, "SAVEVAR<TT>"),
                     (0x1012F700, "LOADVAR<TT>"), (0x1012F9A0, "SAVEVARSTRUCT<EH>"), (0x1012F5A8, "LOADVARSTRUCT<EH>")):
    STUBS.internal(_addr, ser_stub(_name))

STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "us"))
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "bs"))


def stub_kill(e):
    e.cl.rec("SCR_KillScript", (s32(e.r(3)),))


STUBS.internal(0x100D9478, stub_kill)


def stub_spawn(e):
    L = e.cl
    L.rec("ESP_SpawnScript", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), CL_fb(e.f(1)), CL_fb(e.f(2)), CL_fb(e.f(3))))
    e.set_r(3, L.ret("ESP") & MASK)


def CL_fb(x):
    import struct
    b = struct.unpack(">I", struct.pack(">f", x))[0]
    return 0x7FC00000 if (b & 0x7F800000) == 0x7F800000 and (b & 0x7FFFFF) else b


STUBS.internal(0x100DA18C, stub_spawn)


def stub_construct(e):
    L = e.cl
    L.rec("__construct_array", (s32(e.r(3) - L.base), L.lab(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7))))


STUBS.imp("__construct_array", stub_construct)


def setup(runner):
    T.install_std_stubs(runner.emu)
    STATE["world"] = T.persist_alloc(runner.emu, 0x1DA720)


class Side:
    def words(self, a, ws):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        import struct
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_halves(self, a, n):
        import struct
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def rd_words(self, a, n):
        import struct
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    file = FILE

    def __init__(self, e, case):
        self.e = e
        fixed = {FILE: 1, CTORVEC: 3}
        for k in range(22):
            fixed[SCRIPT0 + 0x18 * k] = 10 + k
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

    def set_base(self, obj):
        self.L.base = obj

    def world(self, turn):
        w = STATE["world"]
        self.words(w + TURN_OFF, [turn])
        self.words(w + 0x1DA714, [w])
        self.words(ADDR["gref"], [w + 0x1DA714])

    def guard(self):
        self.write(ADDR["guard"], b"\x01")

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, args)
        if entry == F_GET:
            return (r3 - args[0]) & MASK
        if entry != F_SER:
            return 0
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.file = self.g("file")
        fixed = {1: self.g("file"), 3: self.g("ctorvec")}
        for k in range(22):
            fixed[10 + k] = self.g("scripts") + 0x18 * k
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ct_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ct_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_base(self, obj):
        pass

    def world(self, turn):
        if not hasattr(self.lib, "world"):
            self.lib.world = self.lib.tpw_ct_alloc(0x1DA720)
        w = self.lib.world
        self.words(w + TURN_OFF, [turn])
        C.c_uint64.from_address(self.g("gworld")).value = w

    def guard(self):
        pass

    def call(self, entry, args):
        lib = self.lib
        obj = args[0]
        if entry == F_CT:
            lib.tpw_ct_ctor(obj)
            return 0
        if entry == F_PROC:
            lib.tpw_ct_process(obj)
            return 0
        if entry == F_REM:
            lib.tpw_ct_remove(obj)
            return 0
        if entry == F_SHOW:
            lib.tpw_ct_show(obj, s32(args[1]), args[2])
            return 0
        if entry == F_REC:
            lib.tpw_ct_record(obj, s32(args[1]), args[2] >> 16)
            return 0
        if entry == F_GET:
            return (lib.tpw_ct_get(obj, args[1] & MASK) - obj) & MASK
        return lib.tpw_ct_serialise(obj, args[1], s32(args[2]), s32(args[3]), args[4] & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]),
                         ("ctor", None, [C.c_uint64]), ("process", None, [C.c_uint64]), ("remove", None, [C.c_uint64]),
                         ("show", None, [C.c_uint64, C.c_int32, C.c_uint32]),
                         ("record", None, [C.c_uint64, C.c_int32, C.c_uint32]),
                         ("get", C.c_uint64, [C.c_uint64, C.c_uint32]),
                         ("serialise", C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32])):
        f = getattr(lib, "tpw_ct_" + n)
        f.restype, f.argtypes = res, args
    lib.tpw_ct_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ct_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def make_runner(op):
    entry = {"ser": F_SER, "ct": F_CT, "proc": F_PROC, "show": F_SHOW, "rem": F_REM, "rec": F_REC, "get": F_GET}[op]

    def run(s, case):
        try:
            obj = s.alloc(0x100)
            s.set_base(obj)
            s.words(obj, [case["last"]])
            hs = []
            for w in case["ev"]:
                hs += [(w >> 16) & 0xFFFF, w & 0xFFFF]
            s.halves(obj + 4, hs)
            s.words(obj + 0x84, [case["script"], case["idx"], case["time"]])
            s.words(s.g("fpflags"), [case["fp"]])
            s.words(s.g("gt"), [case["gt"]])
            s.guard()
            s.world(case["turn"])
            a = case["args"]
            if op == "ser":
                ans = s.call(entry, [obj, s.file, a[0], a[1], a[2]])
            else:
                ans = s.call(entry, [obj] + a)
            return {"answer": ans, "log": s.log(), "obj0": s.rd_words(obj, 1), "ev": s.rd_halves(obj + 4, 64),
                    "tail": s.rd_words(obj + 0x84, 3), "pad": s.rd_words(obj + 0x90, 4)}
        finally:
            s.done()
    return run


def mk_test(op):
    run = make_runner(op)
    return FnTest(NAMES[op], lambda e, c: run(Ppc(e, c), c), lambda lib, c: run(Host(lib, c), c),
                  edge=lambda: edges(op), rnd=lambda r: rand_case(r, op))


NAMES = {"ser": "CThoughts::Serialise", "ct": "CThoughts::CThoughts", "proc": "CThoughts::ProcessThoughtBubbles",
         "show": "CThoughts::ShowThoughtBubble", "rem": "CThoughts::RemoveThoughtBubble", "rec": "CThoughts::RecordEvent",
         "get": "CThoughts::GetRecentEvent"}


def half(r):
    return r.choice([0, 1, 2, 3, r.getrandbits(16)])


def rand_case(r, op, **kw):
    c = {"op": op}
    c["last"] = r.choice([0, 1, 5, 0x16, 0x17, r.getrandbits(32)])
    c["ev"] = [(half(r) << 16) | half(r) if op != "ct" else r.getrandbits(32) for _ in range(32)]
    c["script"] = r.choice([0, 0, r.randrange(1, 1000), r.getrandbits(32)])
    c["idx"] = r.randrange(32)
    c["time"] = r.choice([0, r.randrange(0, 1000), 0xFFFFFFF8, r.getrandbits(32)])
    c["fp"] = r.choice([0, 0, 0, 0x16, 1, 2, 4, 8, 0x10, 0x20, r.getrandbits(32)])
    c["gt"] = r.choice([0, 1, 1, 2, r.getrandbits(32)])
    c["turn"] = (c["time"] + r.choice([-1, 0, 1, 11, 12, 13, 19, 20, 21, 39, 40, 41, 59, 60, 61, r.randrange(-5, 140)])) & MASK
    n = r.randrange(0, 45)
    c["queues"] = {"ser": [0] * n + [1], "ESP": [r.randrange(1, 1 << 20)]}
    if op == "ser":
        c["args"] = [r.choice([0, 1, 5, -1]) & MASK, r.choice([-1, 0, 1, 2, 5, r.getrandbits(32)]) & MASK, r.getrandbits(32)]
    elif op == "show":
        c["args"] = [r.choice(list(range(0, 24)) + [-1, r.getrandbits(32)]) & MASK, r.choice([0, 1, 0x100, 0xFF])]
    elif op == "rec":
        t, i = r.choice([0, 1, 2, 3, -1]) & MASK, half(r)
        slot = (c["idx"] - 1) % 32
        k = r.random()
        if k < 0.4:
            c["ev"][slot] = ((t & 0xFFFF) << 16) | i
        elif k < 0.6:
            c["ev"][slot] = ((t & 0xFFFF) << 16) | ((i + 1) & 0xFFFF)
        c["args"] = [t, i << 16]
    elif op == "get":
        c["idx"] = r.choice([0, 1, 5, 31, 32, 33, 100, r.getrandbits(32)])
        c["args"] = [r.choice([0, 1, 2, 31, 32, 33, 0x40, r.getrandbits(32), r.randrange(0, 40)])]
    else:
        c["args"] = []
    if op == "rec":
        c["idx"] = r.randrange(32)
    return c


def edges(op):
    r = random.Random(hash(op) & 0xFFFF)
    if op == "show":
        for t in list(range(0, 24)) + [0xFFFFFFFF]:
            for alw in (0, 1):
                c = rand_case(r, op)
                c["args"] = [t, alw]
                c["fp"] = 0
                c["gt"] = r.choice([0, 1])
                yield c
    elif op == "get":
        for i in (0, 1, 31, 32, 33):
            for x in (0, 1, 2, 5, 31):
                c = rand_case(r, op)
                c["idx"], c["args"] = x, [i]
                yield c
    elif op == "proc":
        for d in (11, 12, 13, -1):
            c = rand_case(r, op)
            c["turn"] = (c["time"] + d) & MASK
            yield c
    elif op == "ser":
        for save in (0, 1):
            for v in (-1, 0, 1, 2):
                for n in (0, 1, 2, 33, 34, 35, 36, 40):
                    c = rand_case(r, op)
                    c["queues"]["ser"] = [0] * n + [1]
                    c["args"] = [save, v & MASK, 0]
                    yield c
    else:
        for _ in range(3):
            yield rand_case(r, op)


TESTS = [mk_test(op) for op in ("ser", "ct", "proc", "show", "rem", "rec", "get")]
