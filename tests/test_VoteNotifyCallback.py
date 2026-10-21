"""Unit v51009, five online functions, against libtpw_unit_VoteNotifyCallback.dylib (tests/unit_VoteNotifyCallback.cpp), main app
("SimTheme Park"), src/game/online/FTPClient.cpp, src/game/online/IPBase.cpp, src/game/online/online.cpp:
  VoteNotifyCallback 0x10213590, SwearFilter::Init 0x1020d1bc, stristr 0x1020cbec, IPBase::ShowError 0x1020cac8,
  FTPClient::ProcessEvents 0x1020c8cc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments and answers from the case: the Argument constructors and destructors, the
vector push_back and tear_down, operator new, the COnlineControl constructor and CallbackFn, std::list::erase and LoadWordList, WSAGetLastError
and MessageBoxA, bthread_cond::wait (which also stores the next state in the object), _Connect, _Download, SendCommandLine, RecvResponse,
Sock::Close, shutdown, recv, closesocket and the control's virtual callback (through __ptr_glue).

The world of a case: gOnlineControl (NULL or an object) for VoteNotifyCallback; the two paths and the answers of LoadWordList for Init; two
strings and the lower case table for stristr; the error tables and the winsock error for ShowError; an FTPClient block (socket, listen socket,
a control object with a vtable) and the queue of states for ProcessEvents. What is compared: the answer, the event log and the globals and
blocks the function writes (gOnlineControl as null / the old object / a new one, the haystack, the socket words, the ready byte, the state).

No fused operations. Not tested: ProcessEvents with a state that does nothing as the very first state (negative or 4 and above): the
result register is then the caller's garbage.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_VoteNotifyCallback"
MASK = 0xFFFFFFFF

FN_VOTE, FN_INIT, FN_STRISTR, FN_SHOW, FN_EVENTS = 0x10213590, 0x1020D1BC, 0x1020CBEC, 0x1020CAC8, 0x1020C8CC
SIZES = {FN_VOTE: 360, FN_INIT: 224, FN_STRISTR: 248, FN_SHOW: 140, FN_EVENTS: 388}
OWN = set(SIZES)

ADDR = dict(gOnlineControl=0x10396D28, codes=0x10396D1C, texts=0x10396D18, lowmap=0x103BCF9C)
MAC = dict(allowed=0x0C, sock=0x04, buf=0x20, listen=0x2428, cond=0x2764, ctl=0x275C, state=0x276C, ready=0x2770)
HL_NAMES = ["allowed", "sock", "buf", "listen", "cond", "ctl", "state", "ready", "size"]
HL = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(name, ints):
    return lambda e: e.cl.rec(name, ints(e))


def stub_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("new", (size,))
    e.set_r(3, 0 if L.ret("newnull") else e.alloc(size))


def stub_string_ctor(e):
    e.cl.rec("StringCtor", ())
    e.write(e.r(3), struct.pack(">I", 0x1111))


def stub_wstring_ctor(e):
    e.cl.rec("WStringCtor", ())
    e.write(e.r(3), struct.pack(">I", 0x2222))


def stub_wstring_dtor(e):
    e.cl.rec("WStringDtor", (e.u32(e.r(3)), T.s16(e.r(4))))


def stub_string_dtor(e):
    e.cl.rec("StringDtor", (e.u32(e.r(3)), T.s16(e.r(4))))


def stub_push(e):
    vec, a = e.r(3), e.r(4)
    v0 = e.u32(vec)
    e.cl.rec("push", (e.u32(a), e.u32(a + 4), e.u32(a + 8), v0))
    e.write(vec, struct.pack(">I", (v0 + 1) & MASK))


def stub_tear_down(e):
    e.cl.rec("tear_down", (e.u32(e.r(3)),))


def stub_callback_fn(e):
    v = e.r(5)
    e.cl.rec("CallbackFn", (e.cl.lab(e.r(3)), s32(e.r(4)), e.u32(v), e.u32(v + 4), e.u32(v + 8)))


def stub_erase(e):
    e.cl.rec("erase", (e.cl.lab(e.r(4)),))


def stub_wsa(e):
    e.set_r(3, e.cl.ret("wsa") & MASK)


def stub_connect(e):
    e.cl.rec("_Connect", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("_Connect") & MASK)


def stub_download(e):
    e.cl.rec("_Download", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("_Download") & MASK)


def stub_recv_response(e):
    e.cl.rec("RecvResponse", (e.cl.lab(e.r(3)), 1 if e.r(4) else 0, s32(e.r(5)), 1 if e.r(6) & 0xFF else 0))
    e.set_r(3, e.cl.ret("RecvResponse") & MASK)


def stub_recv(e):
    e.cl.rec("recv", (s32(e.r(3)), 1 if e.r(4) == e.vn_obj + MAC["buf"] else 0, s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("recv") & MASK)


def stub_cond_wait(e):
    obj = e.r(3) - MAC["cond"]
    state = e.cl.ret("state")
    e.write(obj + MAC["state"], struct.pack(">I", state & MASK))
    e.cl.rec("CondWait", (s32(e.r(4)), s32(state)))


def glue(fn):
    """A glue import: the real glue saves the TOC at 0x14(r1) and the caller reloads r2 from there after the call."""
    def run(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return run


STUBS.internal(0x1007363C, stub_string_ctor)
STUBS.internal(0x101E0B84, stub_wstring_ctor)
STUBS.internal(0x10201478, stub_push)
STUBS.internal(0x101468F8, stub_wstring_dtor)
STUBS.internal(0x1000221C, stub_string_dtor)
STUBS.internal(0x102384B8, glue(stub_new))
STUBS.internal(0x10213870, CL.recorder("COnlineControl", "N"))
STUBS.internal(0x10214B70, stub_callback_fn)
STUBS.internal(0x10201310, stub_tear_down)
STUBS.internal(0x1020DB8C, stub_erase)
STUBS.internal(0x1020CD08, CL.recorder("LoadWordList", "LsLb", ret="b"))
STUBS.internal(0x1023B0B0, glue(stub_wsa))
STUBS.internal(0x1023AB40, glue(CL.recorder("MessageBoxA", "Lssu")))
STUBS.internal(0x10200F30, stub_cond_wait)
STUBS.internal(0x1020BBE8, stub_connect)
STUBS.internal(0x1020C228, stub_download)
STUBS.internal(0x10201B14, CL.recorder("SendCommandLine", "Ls"))
STUBS.internal(0x10201BB4, stub_recv_response)
STUBS.internal(0x1020E358, rec("SockClose", lambda e: (e.cl.lab(e.r(3)),)))
STUBS.internal(0x1023A7B0, glue(rec("shutdown", lambda e: (s32(e.r(3)), s32(e.r(4))))))
STUBS.internal(0x1023A780, glue(stub_recv))
STUBS.internal(0x1023A7C8, glue(rec("closesocket", lambda e: (s32(e.r(3)),))))
STUBS.internal(0x1023BAA8, rec("ControlCallback", lambda e: (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)))))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def cstr(self, text):
        data = text.encode("ascii") + b"\0"
        a = self.alloc(len(data))
        self.write(a, data)
        return a

    def classify(self, a):
        p = self.get_ptr(a)
        return 0 if p == 0 else (1 if p == self.pre else 2)


class Ppc(Side):
    fmt = ">I"
    psize = 4

    def __init__(self, e, case):
        self.e = e
        self.obj, self.ctl, self.vt, self.pre = e.alloc(0x2800), e.alloc(16), e.alloc(16), e.alloc(0xE8C)
        e.vn_obj = self.obj
        self.L = e.cl = CL.PpcLog({self.obj: 1, self.ctl: 2, self.pre: 3, self.obj + MAC["allowed"]: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "lowmap":
            return self.e.u32(0x10261DDC)   # the emulator binds the import's data elsewhere: the TOC slot holds its address
        return ADDR[name]

    def o(self, name):
        return MAC[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [x & MASK for x in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj, self.ctl, self.vt, self.pre = self.alloc(0x2800), self.alloc(16), self.alloc(16), self.alloc(0xE8C)
        CL.host_begin(lib, {1: self.obj, 2: self.ctl, 3: self.pre, 4: self.obj + HL["allowed"]}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vn_addr(name.encode())

    def o(self, name):
        return HL[name]

    def alloc(self, n):
        return self.lib.tpw_vn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        f = getattr(self.lib, "tpw_vn_" + entry)
        r = f(*args)
        return (r or 0) & (2**64 - 1 if entry == "stristr" else MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vn_addr.restype, lib.tpw_vn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vn_alloc.restype, lib.tpw_vn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vn_vote.restype, lib.tpw_vn_vote.argtypes = None, [C.c_int32, C.c_int32]
    lib.tpw_vn_init.restype, lib.tpw_vn_init.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_vn_stristr.restype, lib.tpw_vn_stristr.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_vn_showerror.restype, lib.tpw_vn_showerror.argtypes = None, [C.c_uint64]
    lib.tpw_vn_events.restype, lib.tpw_vn_events.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vn_layout(out)
    assert n == len(HL_NAMES), n
    HL.update(dict(zip(HL_NAMES, list(out)[:n])))
    assert HL["size"] <= 0x2800
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the calls


def default_map():
    return [i + 32 if 0x41 <= i <= 0x5A else i for i in range(256)]


def build(s, case):
    op = case["op"]
    if op == "vote":
        s.put_ptr(s.g("gOnlineControl"), s.pre if case["have"] else 0)
    elif op == "stristr":
        s.halves(s.g("lowmap"), case["map"])
    elif op == "show":
        n = len(case["codes"])
        codes = s.alloc(2 * (n + 1))
        s.halves(codes, case["codes"] + [0])
        texts = s.alloc(8 * (n + 1))
        for i in range(n + 1):
            s.put_ptr(texts + s.psize * i, s.cstr("msg%d" % i))
        s.put_ptr(s.g("codes"), codes)
        s.put_ptr(s.g("texts"), texts)
    elif op == "events":
        s.words(s.obj + s.o("sock"), [case["sock"]])
        s.words(s.obj + s.o("listen"), [case["listen"]])
        s.write(s.obj + s.o("ready"), bytes([case["ready"]]))
        s.words(s.ctl, [s.vt])
        s.put_ptr(s.obj + s.o("ctl"), s.ctl)


def execute(s, case):
    op = case["op"]
    if op == "vote":
        a, b = case["args"]
        return s.call(FN_VOTE if isinstance(s, Ppc) else "vote", [a, b, 0, 0] if isinstance(s, Ppc) else [s32(a), s32(b)]) and 0
    if op == "init":
        p1, p2 = s.cstr(case["paths"][0]), s.cstr(case["paths"][1])
        r = s.call(FN_INIT if isinstance(s, Ppc) else "init", [s.obj, p1, p2, case["flag"]])
        return r & 0xFF
    if op == "stristr":
        base = s.alloc(2 * (len(case["hay"]) + 1))
        s.halves(base, case["hay"] + [0])
        needle = s.alloc(2 * (len(case["needle"]) + 1))
        s.halves(needle, case["needle"] + [0])
        s.hbase = base
        r = s.call(FN_STRISTR if isinstance(s, Ppc) else "stristr", [base, needle])
        return -1 if r == 0 else (r - base) // 2 if (r - base) % 2 == 0 else ("odd", r - base)
    if op == "show":
        s.call(FN_SHOW if isinstance(s, Ppc) else "showerror", [s.obj])
        return 0
    s.call(FN_EVENTS if isinstance(s, Ppc) else "events", [s.obj])
    return 0


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    op = case["op"]
    if op == "vote":
        out["ctl"] = s.classify(s.g("gOnlineControl"))
    elif op == "stristr":
        out["hay"] = s.rd_halves(s.hbase, len(case["hay"]) + 1)
    elif op == "events":
        out["sock"] = s.rd_word(s.obj + s.o("sock"))
        out["listen"] = s.rd_word(s.obj + s.o("listen"))
        out["ready"] = s.read(s.obj + s.o("ready"), 1)
        out["state"] = s.rd_word(s.obj + s.o("state"))
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


# ---------------------------------------------------------------------------------------------------------------- case generators

S32S = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x2711, 5]


def vote_case(r, have=None, newnull=None, a=None):
    return {"op": "vote", "have": r.choice([0, 1]) if have is None else have,
            "queues": {"newnull": [r.choice([0, 0, 1]) if newnull is None else newnull]},
            "args": [r.choice(S32S) if a is None else a, r.choice(S32S)]}


def vote_edges():
    r = random.Random(11)
    for have in (0, 1):
        for newnull in (0, 1):
            for a in (0, -1, 0x7FFFFFFF):
                yield vote_case(r, have, newnull, a)


def vote_random(r):
    return vote_case(r)


def init_case(r, flag=None, a=None, b=None):
    return {"op": "init", "flag": r.choice([0, 1]) if flag is None else flag,
            "paths": ["words%d.dat" % r.randrange(100), "allowed%d.dat" % r.randrange(100)],
            "queues": {"LoadWordList": [r.choice([0, 1]) if a is None else a, r.choice([0, 1]) if b is None else b]}}


def init_edges():
    r = random.Random(12)
    for flag in (0, 1):
        for a in (0, 1):
            for b in (0, 1):
                yield init_case(r, flag, a, b)


def init_random(r):
    return init_case(r)


ALPHA = [0x61, 0x41, 0x62, 0x42, 0x20, 0x5B, 0xFF, 0x100, 0x141, 0x1FF, 0xFFFF, 0x2A, 0xC0, 0xE0]


def stristr_case(r, hay=None, needle=None, mp=None):
    if hay is None:
        hay = [r.choice(ALPHA[:6]) for _ in range(r.randrange(0, 9))]
        if r.random() < 0.2:
            hay = [r.choice(ALPHA) for _ in range(r.randrange(0, 9))]
    if needle is None:
        if hay and r.random() < 0.5:
            i = r.randrange(len(hay))
            needle = hay[i:i + r.randrange(1, 5)]
            needle = [c ^ 0x20 if (0x41 <= c <= 0x7A and r.random() < 0.5) else c for c in needle]
        else:
            needle = [r.choice(ALPHA[:6]) for _ in range(r.randrange(0, 4))]
    if mp is None:
        mp = default_map()
        if r.random() < 0.25:
            mp = [r.choice(ALPHA + [0, r.randrange(65536)]) for _ in range(256)]
    return {"op": "stristr", "hay": hay, "needle": needle, "map": mp, "queues": {}}


def stristr_edges():
    r = random.Random(13)
    D = [ord(c) for c in "aab"]
    for hay, needle in [([], []), ([0x61], []), ([], [0x61]), ([0x61], [0x41]), ([0x41, 0x42], [0x61, 0x62]),
                        ([ord(c) for c in "aaab"], D), ([ord(c) for c in "xaab"], D), ([ord(c) for c in "aabaab"], D),
                        ([ord(c) for c in "ab"], [0x61, 0x62, 0x63]), ([ord(c) for c in "abc"], [0x61, 0x62, 0x63]),
                        ([0x100, 0x141], [0x100]), ([0x141], [0x141]), ([0xFFFF, 0xFFFF], [0xFFFF, 0xFFFF]),
                        ([0xFF, 0x100], [0xFF, 0x100]), ([0x61, 0x62, 0x61], [0x61, 0x62, 0x61, 0x62])]:
        yield stristr_case(r, hay, needle)


def stristr_random(r):
    return stristr_case(r)


def show_case(r, codes=None, wsa=None):
    if codes is None:
        pool = [10004, 10013, 10035, 10048, 10054, 10061, -1, -32768, 32767, 1, 2, 0x4000]
        codes = r.sample(pool, r.randrange(0, 6))
    if wsa is None:
        k = r.random()
        if k < 0.5 and codes:
            wsa = r.choice(codes)
        elif k < 0.6:
            wsa = 0
        elif k < 0.7 and codes:
            wsa = r.choice(codes) + 65536
        else:
            wsa = r.choice([10099, -5, 0x7FFFFFFF, -0x80000000, 3])
    return {"op": "show", "codes": codes, "queues": {"wsa": [wsa]}}


def show_edges():
    r = random.Random(14)
    yield show_case(r, [], 0)
    yield show_case(r, [], 5)
    yield show_case(r, [10004], 10004)
    yield show_case(r, [10004], 0)
    yield show_case(r, [10004, 10013], 10013)
    yield show_case(r, [10004, 10013], 1)
    yield show_case(r, [-32768, 5], -32768)
    yield show_case(r, [-32768, 5], 32768)
    yield show_case(r, [7], 7 + 65536)


def show_random(r):
    return show_case(r)


def events_case(r, states=None, sock=None, listen=None, resp=None):
    if states is None:
        states = [r.choice([0, 1, 2, 3])] + [r.choice([-2, -1, 0, 1, 2, 3, 4, 5, 2, 3, 1]) for _ in range(r.randrange(0, 6))]
        states = states + [0]
    recvs = [r.randrange(1, 0x401) for _ in range(r.randrange(0, 4))] + [r.choice([0, -1])]
    return {"op": "events", "sock": r.choice([-1, 5, 0x1234]) if sock is None else sock,
            "listen": r.choice([-1, 7, 0]) if listen is None else listen, "ready": r.choice([0, 1]),
            "queues": {"state": states,
                       "_Connect": [r.choice([0, 0x34, 0x38, 0x40, 0x41]) for _ in range(3)],
                       "_Download": [r.choice([0, 0x34, 0x44, 0x43]) for _ in range(3)],
                       "RecvResponse": [r.choice([0, 0xDD, 0x43, 0xC8, 1]) for _ in range(3)] if resp is None else [resp],
                       "recv": recvs}}


def events_edges():
    r = random.Random(15)
    yield events_case(r, [0])
    yield events_case(r, [1, 0])
    yield events_case(r, [3, 0])
    yield events_case(r, [1, 3, 0])
    yield events_case(r, [1, -1, 0])
    yield events_case(r, [3, 4, 0])
    yield events_case(r, [3, -3, 7, 0])
    for sock in (-1, 5):
        for listen in (-1, 9):
            for resp in (0, 0xDD, 0x43, 0xC8):
                yield events_case(r, [2, 0], sock, listen, resp)
    yield events_case(r, [2, 1, 2, 0], 5, 9, 0)


def events_random(r):
    return events_case(r)


TESTS = [
    FnTest("VoteNotifyCallback", ppc, host, edge=vote_edges, rnd=vote_random),
    FnTest("SwearFilter::Init", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("stristr", ppc, host, edge=stristr_edges, rnd=stristr_random),
    FnTest("IPBase::ShowError", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("FTPClient::ProcessEvents", ppc, host, edge=events_edges, rnd=events_random),
]
