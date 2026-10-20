"""Unit v5103, COnlineControl's Shutdown, WantToShutDown, Update, CallbackFn and first-person moves, against
libtpw_unit_COnlineControl_WantToShutDown.dylib (tests/unit_COnlineControl_WantToShutDown.cpp), main app:
  Shutdown 0x10214840, WantToShutDown 0x102148bc, Update 0x10214944, CallbackFn 0x10214b70, EnteringFirstPerson 0x102177bc,
  LeavingFirstPerson 0x10217884, UpdateFirstPersonPos 0x1021797c, UnHideAvatar 0x10217aa8

A case is a world and a call. The object (0xe8c bytes) is built twice by one piece of Python code. The callees (mutex lock and unlock,
ProcessQueue, UDebug, Vote release and destructor, DisableVoteButton, the deque push_back, HideMySprite, DrawMySpriteAt, GetThingId,
Teleport, Chat::MovePlayer, SCR_SetFloatVariable) are fakes on both sides that record their arguments.

The world of a case: the Chat, CPeep and Vote blocks, the flag bytes, the peep's word at 0xc, the MapPos words (random low halves), the
thing id the GetThingId fake answers. What is compared: the answer, the event log, the whole object (pointer words as labels).
No fused operations. Not tested: a flag byte above 1 for WantToShutDown (the header says bool; the original returns the raw byte).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_WantToShutDown"
MASK = 0xFFFFFFFF

F_SHUT, F_WANT, F_UPDATE, F_CALLBACK = 0x10214840, 0x102148BC, 0x10214944, 0x10214B70
F_ENTER, F_LEAVE, F_UPPOS, F_UNHIDE = 0x102177BC, 0x10217884, 0x1021797C, 0x10217AA8
SIZES = {F_SHUT: 76, F_WANT: 80, F_UPDATE: 152, F_CALLBACK: 92, F_ENTER: 128, F_LEAVE: 176, F_UPPOS: 116, F_UNHIDE: 116}
OWN = set(SIZES)

OBJ_SIZE = 0xE8C
L_OBJ, L_CHAT, L_PEEP, L_VOTE, L_M18, L_M1C, L_VEC, L_DEQUE = 1, 2, 3, 4, 5, 6, 7, 8
HOST_LAYOUT = [2]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, fn):
    def stub(e):
        e.cl.rec(name, fn(e))
    STUBS.internal(addr, stub)


fake(0x10200D8C, "lock", lambda e: (e.cl.lab(e.r(3)),))
fake(0x10200DE0, "unlock", lambda e: (e.cl.lab(e.r(3)),))
fake(0x10214C34, "ProcessQueue", lambda e: (e.cl.lab(e.r(3)),))
fake(0x1023B200, "VoteRelease", lambda e: (e.cl.lab(e.r(3)),))
fake(0x1023B218, "VoteDtor", lambda e: (e.cl.lab(e.r(3)), T.s16(e.r(4))))
fake(0x101E6224, "DisableVoteButton", lambda e: (e.r(3) & 0xFF,))
fake(0x10224A3C, "PushBack", lambda e: (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
fake(0x10114758, "HideMySprite", lambda e: (e.cl.lab(e.r(3)),))
fake(0x10114624, "DrawMySpriteAt", lambda e: (e.cl.lab(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3)), s32(e.r(7)), s32(e.r(8))))
fake(0x100D95A0, "SCR_SetFloatVariable", lambda e: (s32(e.r(3)), e.r(4) & MASK, fb(e.f(1))))
fake(0x10219AF4, "Teleport", lambda e: (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF, (e.r(6) >> 16) & 0xFFFF))
fake(0x10239178, "MovePlayer", lambda e: (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.cl.lab(e.r(7))))


def stub_udebug(e):
    e.cl.rec("UDebug", (), (e.cstr(e.r(3)),))


STUBS.internal(0x10000E40, stub_udebug)


def stub_getthingid(e):
    e.cl.rec("GetThingId", (e.cl.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">I", (e.cl.ret("GetThingId") & 0xFFFF) << 16))


STUBS.internal(0x1012E1F8, stub_getthingid)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.queues = case["queues"]
        self.saved = STUBS.activate(e)

    def ptrword(self, a):
        return a

    def begin(self, labels):
        self.e.cl = self.L = CL.PpcLog({a: l for l, a in labels.items()}, self.queues)

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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & 0xFF if entry == F_WANT else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.queues = case["queues"]

    def begin(self, labels):
        self.lib.tpw_oc_reset()
        CL.host_begin(self.lib, labels, self.queues)

    def ptrword(self, a):
        return self.lib.tpw_oc_handle(a)

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_oc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        L = self.lib
        o = args[0]
        if entry == F_SHUT:
            L.tpw_oc_shutdown(o)
        elif entry == F_WANT:
            return L.tpw_oc_want(o) & 0xFF
        elif entry == F_UPDATE:
            L.tpw_oc_update(o)
        elif entry == F_CALLBACK:
            L.tpw_oc_callback(o, s32(args[1]), args[2])
        else:
            {F_ENTER: L.tpw_oc_entering, F_LEAVE: L.tpw_oc_leaving, F_UPPOS: L.tpw_oc_updatepos, F_UNHIDE: L.tpw_oc_unhide}[entry](
                o, args[1] & MASK, args[2] & MASK)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_oc_alloc.restype, lib.tpw_oc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_oc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_oc_shutdown.argtypes = [C.c_uint64]
    lib.tpw_oc_want.restype, lib.tpw_oc_want.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_oc_handle.restype, lib.tpw_oc_handle.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_oc_update.argtypes = [C.c_uint64]
    lib.tpw_oc_callback.argtypes = [C.c_uint64, C.c_int32, C.c_uint64]
    for n in ("entering", "leaving", "updatepos", "unhide"):
        getattr(lib, "tpw_oc_" + n).argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_oc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    obj = s.alloc(OBJ_SIZE)
    chat = s.alloc(16)
    peep = s.alloc(32)
    vote = s.alloc(16)
    vec = s.alloc(16)
    s.begin({L_OBJ: obj, L_CHAT: chat, L_PEEP: peep, L_VOTE: vote, L_M18: obj + 0x18, L_M1C: obj + 0x1C, L_VEC: vec,
             L_DEQUE: obj + 0xE68})
    pw = s.ptrword
    s.words(obj, [pw(chat), pw(peep)])
    s.write(obj + 0x10, bytes([case["b10"], case["b11"], case["b12"], case["b13"]]))
    s.write(obj + 0xE60, bytes([case["flag"]]))
    s.words(obj + 0xE64, [pw(vote) if case["vote"] else 0])
    s.words(peep + 0xC, [case["handle"]])
    s.labels = {pw(chat): L_CHAT, pw(peep): L_PEEP, pw(vote): L_VOTE, 0: 0}
    s.obj, s.vec = obj, vec


def execute(s, case):
    return s.call(case["entry"], [s.obj, case["a"], case["b"]] if case["entry"] != F_CALLBACK else [s.obj, case["a"], s.vec])


def observe(s, case, answer):
    raw = bytearray(s.read(s.obj, OBJ_SIZE))
    ptrs = []
    for off in (0, 4, 0xE64):
        w = s.rd_words(s.obj + off, 1)[0]
        ptrs.append(s.labels.get(w, -1))
        raw[off:off + 4] = b"\0\0\0\0"
    return {"answer": answer, "log": s.log(), "obj": bytes(raw), "ptrs": ptrs}


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


# ---------------------------------------------------------------------------------------------------------------- cases


def word(r):
    k = r.random()
    cell = r.choice([0, 1, 0x7F, 0x80, 0xFF, r.randrange(256)])
    off = r.choice([0, 1, 0x7F, 0x80, 0xFF, r.randrange(256)])
    return (cell << 24) | (off << 16) | (r.randrange(0x10000) if k < 0.5 else 0)


def base(r, entry, **kw):
    c = {"entry": entry, "queues": {"GetThingId": [r.randrange(0x10000)]}, "b10": r.randrange(256), "b11": r.randrange(256),
         "b12": r.randrange(2), "b13": r.randrange(256), "flag": r.choice([0, 1, 2, 0xFF]), "vote": r.random() < 0.5,
         "handle": r.randrange(1 << 32), "a": word(r), "b": word(r)}
    c.update(kw)
    return c


def edges_for(entry, seed):
    def gen():
        r = random.Random(seed)
        for flag in (0, 1, 2, 0xFF):
            for vote in (False, True):
                for b12 in (0, 1):
                    yield base(r, entry, flag=flag, vote=vote, b12=b12)
        for a in (0, 0x80000000, 0xFFFF0000, 0x7FFF0000, 0x00010000):
            yield base(r, entry, a=a, b=0x80FF0000)
            yield base(r, entry, a=a, b=a)
    return gen


def rnd_for(entry):
    return lambda r: base(r, entry)


def test(name, entry, seed):
    return FnTest(name, ppc, host, edge=edges_for(entry, seed), rnd=rnd_for(entry))


TESTS = [
    test("COnlineControl::Shutdown", F_SHUT, 1),
    test("COnlineControl::WantToShutDown", F_WANT, 2),
    test("COnlineControl::Update", F_UPDATE, 3),
    test("COnlineControl::CallbackFn", F_CALLBACK, 4),
    test("COnlineControl::EnteringFirstPerson", F_ENTER, 5),
    test("COnlineControl::LeavingFirstPerson", F_LEAVE, 6),
    test("COnlineControl::UpdateFirstPersonPos", F_UPPOS, 7),
    test("COnlineControl::UnHideAvatar", F_UNHIDE, 8),
]
