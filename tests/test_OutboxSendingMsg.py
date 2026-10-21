"""Unit v5031, the outbox window's messages, against libtpw_unit_OutboxSendingMsg.dylib (tests/unit_OutboxSendingMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_onlineOutbox.cpp:
  MarkOutboxEmail 0x101e4004, SendMailStatusDisplay 0x101e4184, OutboxSendingMsg 0x101e4250

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian for the original, host
memory for the translation). The callees outside the unit are fakes on both sides that record their arguments: the user info
constructor, the outbox cursor and its four marks (the object recorded as 1 when it is the outbox inside gUserInfo), GetWindow (answers
its object), Show, UI_PostMessage and DialogBoxCallback (answers from the case's queues).

The world of a case: gUserInfo (none, or a block whose mail count word at 0x8c is set), gLobby with its window, the sending window
global, the four status globals. What is compared: the answer, the globals, whether the user info exists afterwards and the event log.

No fused operations. Not tested: a mail index whose masked value is huge (the original walks the cursor that many times).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OutboxSendingMsg"
MASK = 0xFFFFFFFF

FN_MARK, FN_STATUS, FN_MSG = 0x101E4004, 0x101E4184, 0x101E4250
SIZES = {FN_MARK: 248, FN_STATUS: 160, FN_MSG: 236}
OWN = set(SIZES)

ADDR = dict(window=0x10395E0C, shownB=0x10395E00, shownA=0x10395E08, statusB=0x10395DFC, statusA=0x10395E04,
            gUserInfo=0x10380B34, gLobby=0x102E4888)
HOST_LAYOUT = [4]
GLOBS = ("window", "shownB", "shownA", "statusB", "statusA")

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_ctor(e):
    e.write(e.r(3), bytes(0xD4))
    e.cl.rec("CFSUserInfo", ())


def outbox_stub(name, ret=False):
    def fn(e):
        L = e.cl
        L.rec(name, (1 if e.r(3) == e.u32(ADDR["gUserInfo"]) + 0x64 and e.u32(ADDR["gUserInfo"]) != 0 else 0,))
        if ret:
            e.set_r(3, L.ret(name) & 0xFF)
    return fn


def stub_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, e.cl.ret("Show") & MASK)


def msg_stub(name):
    def fn(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
        e.set_r(3, L.ret(name) & MASK)
    return fn


STUBS.internal(0x1017DE54, stub_ctor)
STUBS.internal(0x10205718, outbox_stub("First"))
STUBS.internal(0x1020578C, outbox_stub("Next", True))
STUBS.internal(0x10205828, outbox_stub("MarkToSend"))
STUBS.internal(0x102058B4, outbox_stub("UnmarkToSend"))
STUBS.internal(0x10205910, outbox_stub("MarkToDelete"))
STUBS.internal(0x102059A0, outbox_stub("UnmarkToDelete"))
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x101DC1EC, stub_show)
STUBS.internal(0x101C9400, msg_stub("UI_PostMessage"))
STUBS.internal(0x101F1520, msg_stub("DialogBoxCallback"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def set_lobby(self, window):
        lobby = self.alloc(0x38)
        self.words(lobby + 4, [window])
        self.words(ADDR["gLobby"], [lobby])

    def set_user(self, present, count):
        if present:
            u = self.alloc(0xD4)
            self.words(u + 0x8C, [count])
            self.words(ADDR["gUserInfo"], [u])
        else:
            self.words(ADDR["gUserInfo"], [0])

    def userstate(self):
        return 1 if self.rd_word(ADDR["gUserInfo"]) else 0

    def call_mark(self, flags):
        return T.call_ppc(self.e, FN_MARK, [flags & MASK])[0] & MASK

    def call_status(self):
        T.call_ppc(self.e, FN_STATUS, [])
        return 0

    def call_msg(self, window, m, a, b):
        return T.call_ppc(self.e, FN_MSG, [window, m & MASK, a & MASK, b & MASK])[0] & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"   # a host pointer is 8 bytes

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_osm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_osm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_lobby(self, window):
        self.lib.tpw_osm_setlobby(window)

    def set_user(self, present, count):
        self.lib.tpw_osm_setuser(present, count)

    def userstate(self):
        return self.lib.tpw_osm_userstate()

    def call_mark(self, flags):
        return self.lib.tpw_osm_mark(s32(flags)) & MASK

    def call_status(self):
        self.lib.tpw_osm_status()
        return 0

    def call_msg(self, window, m, a, b):
        return self.lib.tpw_osm_msg(window, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_osm_addr.restype, lib.tpw_osm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_osm_alloc.restype, lib.tpw_osm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_osm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_osm_setlobby.restype, lib.tpw_osm_setlobby.argtypes = None, [C.c_uint64]
    lib.tpw_osm_setuser.restype, lib.tpw_osm_setuser.argtypes = None, [C.c_int32, C.c_uint32]
    lib.tpw_osm_userstate.restype, lib.tpw_osm_userstate.argtypes = C.c_int32, []
    lib.tpw_osm_mark.restype, lib.tpw_osm_mark.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_osm_status.restype, lib.tpw_osm_status.argtypes = None, []
    lib.tpw_osm_msg.restype, lib.tpw_osm_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_osm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    win = s.alloc(16)
    lwin = s.alloc(16)
    s.win = win
    s.set_lobby(lwin)
    s.set_user(case["user"], case["count"])
    s.write(s.g("window"), struct.pack(s.pfmt, win if case["haswin"] else 0))
    for k in GLOBS[1:]:
        s.words(s.g(k), [case[k]])


def execute(s, case):
    op = case["op"]
    if op == "mark":
        return s.call_mark(case["flags"])
    if op == "status":
        return s.call_status()
    return s.call_msg(s.win, case["msg"], case["a"], case["b"])


def observe(s, case, answer):
    w = struct.unpack(s.pfmt, s.read(s.g("window"), struct.calcsize(s.pfmt)))[0]
    out = {"answer": answer, "log": s.log(), "user": s.userstate(),
           "window": 0 if w == 0 else (1 if w == s.win else 2)}
    for k in GLOBS[1:]:
        out[k] = s.rd_word(s.g(k))
    return out


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


def base(r, op):
    return {"op": op, "user": r.choice([0, 1, 1]), "count": r.choice([0, 0, 1, 5]), "haswin": r.choice([0, 1]),
            "shownB": r.randrange(3), "shownA": r.randrange(3), "statusB": r.randrange(3), "statusA": r.randrange(3),
            "queues": {"Next": [r.randrange(2)], "Show": [r.randrange(5)], "UI_PostMessage": [r.randrange(5)],
                       "DialogBoxCallback": [r.choice([0, 1, -1, r.randrange(1000)]) & MASK]},
            "flags": 0, "msg": 0, "a": 0, "b": 0}


def mark_case(r, flags=None):
    c = base(r, "mark")
    if flags is None:
        flags = r.randrange(12) | r.choice([0, 0, 0x1000000, 0x2000000, 0x3000000])
    c["flags"] = flags
    return c


def mark_edges():
    r = random.Random(31)
    for f in (0, 1, 2, 0x1000000, 0x2000000, 0x3000000, 0x1000003, 0x2000005, 0x3000001, 3, 0x10, 0x1000020, 0x2000007):
        for u in (0, 1):
            c = mark_case(r, f)
            c["user"] = u
            yield c


def status_case(r, pw=None, ss=None):
    c = base(r, "status")
    if pw is not None:
        c["haswin"] = pw
    return c


def status_edges():
    r = random.Random(32)
    for hw in (0, 1):
        for sa in (0, 1):
            for sb in (0, 1):
                for ha in (0, 1):
                    for hb in (0, 1):
                        c = status_case(r, hw)
                        c.update(statusA=sa, statusB=sb, shownA=sa if ha else sa + 1, shownB=sb if hb else sb + 1)
                        yield c


def msg_case(r, m=None):
    c = base(r, "msg")
    c["msg"] = r.choice([0x14, 0x1e, 0, 0x13, 0x15, 0x1d, 0x1f, -1, 0x100, r.randrange(0x30)]) if m is None else m
    c["a"] = r.choice([0, 1, -1, r.randrange(100)]) & MASK
    c["b"] = r.choice([0, 1, -1, r.randrange(100)]) & MASK
    return c


def msg_edges():
    r = random.Random(33)
    for m in (0x14, 0x1e, 0x13, 0x15, 0x1d, 0x1f, 0, -1):
        for u in (0, 1):
            for cnt in (0, 3):
                c = msg_case(r, m)
                c["user"], c["count"] = u, cnt
                yield c


TESTS = [
    FnTest("MarkOutboxEmail", ppc, host, edge=mark_edges, rnd=lambda r: mark_case(r)),
    FnTest("SendMailStatusDisplay", ppc, host, edge=status_edges, rnd=lambda r: status_case(r)),
    FnTest("OutboxSendingMsg", ppc, host, edge=msg_edges, rnd=lambda r: msg_case(r)),
]
