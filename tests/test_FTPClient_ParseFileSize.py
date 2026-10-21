"""Unit v5273, six FTPClient members, against libtpw_unit_FTPClient_ParseFileSize.dylib (tests/unit_FTPClient_ParseFileSize.cpp), main app,
src/game/online/FTPClient.cpp:
  FTPClient::FTPClient 0x1020b368 (the constructor body), Disconnect 0x1020beb8, _Disconnect 0x1020bf94, Download 0x1020c064,
  _CloseDSocket 0x1020c174, ParseFileSize 0x1020c744

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian)
for the original, and in the host library's object (little-endian, the host offsets come from tpw_ftp_layout) for the translation. The
callees outside the unit (Connection, the string and condition constructors, WSAStartup, strncpy, atoi, bthread_create, Sleep, signal,
SendCommandLine, RecvResponse, Sock::Close, shutdown, recv, closesocket, _Download) are fakes on both sides recording their arguments
(pointers as labels, texts as a hash) and answering from the case's queues; Sleep sets the ready flag when its answer is nonzero.

The world of a case: random bytes in every member the functions touch (the socket word at 4, the two sockets, the sockaddr, the path,
the control word, the string and condition words, the flags), a text block, two argument strings. What is compared: the answer, every
member read back, the event log. ParseFileSize runs with a character map written at 0x103bd07c (white space = bits 2 and 4); the host
tests white space directly. The original's _CloseDSocket receives into the connection's buffer at 0x20, the host into mBuffer: the
fakes log whether the buffer is that one.

No fused operations. Not tested: a listing line with fewer than four fields (the original runs on past the end), the real contents of the
C library's character map.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FTPClient_ParseFileSize"
MASK = 0xFFFFFFFF

FN_CTOR, FN_DISC, FN__DISC, FN_DOWNLOAD, FN_CLOSED, FN_PARSE = 0x1020B368, 0x1020BEB8, 0x1020BF94, 0x1020C064, 0x1020C174, 0x1020C744
SIZES = {FN_CTOR: 208, FN_DISC: 176, FN__DISC: 164, FN_DOWNLOAD: 228, FN_CLOSED: 132, FN_PARSE: 236}
OWN = set(SIZES)

CTYPE_MAP = 0x103BD07C
THREADFN = 0x10268728
OBJ_SIZE = 0x2800
TOC = 0x10267CC0

# key, Mac offset, kind (w words / b bytes), count, host member
FIELDS = [
    ("sock4", 0x4, "w", 1, "mF04"),
    ("data", 0x2424, "w", 1, "mDataSocket"),
    ("listen", 0x2428, "w", 1, "mListenSocket"),
    ("sa", 0x242C, "b", 16, "mFamily"),
    ("path", 0x255C, "b", 0x200, "mPath"),
    ("ctrl", 0x275C, "w", 1, "mControl"),
    ("server", 0x2760, "w", 1, "mServer"),
    ("condA", 0x2764, "w", 1, "mCondA"),
    ("condB", 0x2768, "w", 1, "mCondB"),
    ("f276c", 0x276C, "w", 1, "mF276c"),
    ("ready", 0x2770, "b", 1, "mReady"),
    ("started", 0x2771, "b", 1, "mThreadStarted"),
    ("threaded", 0x2772, "b", 1, "mThreaded"),
]
HOST_ORDER = ["mF04", "mDataSocket", "mListenSocket", "mFamily", "mBuffer", "mPath", "mControl", "mServer", "mCondA", "mCondB", "mF276c",
              "mReady", "mThreadStarted", "mThreaded"]
HOFF = {}


def s32(v):
    return T.s32(v)


def hbytes(b, limit=0x400):
    b = b[:limit]
    return sum(v * (i + 1) for i, v in enumerate(b)) & MASK


def cbytes(e, a, limit=0x400):
    out = b""
    while len(out) < limit:
        chunk = e.read(a + len(out), 32)
        k = chunk.find(b"\0")
        if k >= 0:
            out += chunk[:k]
            break
        out += chunk
    return out[:limit]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def stub(addr):
    def deco(fn):
        def wrapped(e):
            # the glue saves the TOC register at 0x14(r1) and the caller reloads it after the call
            e.write(e.r(1) + 0x14, struct.pack(">I", TOC))
            fn(e)
        STUBS.internal(addr, wrapped)
        return fn
    return deco


@stub(0x10201A84)
def st_connection(e):
    e.cl.rec("ConnectionCtor", (e.cl.lab(e.r(3)),))


@stub(0x1007363C)
def st_string(e):
    e.cl.rec("StringCtor", (e.cl.lab(e.r(3)),))


@stub(0x10200E34)
def st_cond(e):
    e.cl.rec("CondCtor", (e.cl.lab(e.r(3)),))


@stub(0x102394F0)
def st_wsa(e):
    e.cl.rec("WSAStartup", (s32(e.r(3)),))
    e.set_r(3, 0)


@stub(0x10238500)
def st_strncpy(e):
    dest, src, n = e.r(3), e.r(4), s32(e.r(5))
    data = cbytes(e, src, n)
    e.cl.rec("Strncpy", (e.cl.lab(dest), hbytes(data), n))
    e.write(dest, data.ljust(n, b"\0"))
    e.set_r(3, dest)


@stub(0x10238590)
def st_atoi(e):
    p = e.r(3)
    e.cl.rec("Atoi", (p - CUR["text"], hbytes(cbytes(e, p))))
    e.set_r(3, e.cl.ret("Atoi") & MASK)


@stub(0x10200C34)
def st_bthread(e):
    L = e.cl
    L.rec("BthreadCreate", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("BthreadCreate") & MASK)


@stub(0x1023A888)
def st_sleep(e):
    L = e.cl
    L.rec("Sleep", (e.r(3) & MASK,))
    if L.ret("Sleep") & MASK:
        e.write(CUR["obj"] + 0x2770, b"\x01")


@stub(0x10200FB4)
def st_signal(e):
    e.cl.rec("CondSignal", (e.cl.lab(e.r(3)),))


@stub(0x10201B14)
def st_sendcmd(e):
    e.cl.rec("SendCommandLine", (e.cl.lab(e.r(3)), hbytes(cbytes(e, e.r(4)))))


@stub(0x10201BB4)
def st_recvresp(e):
    L = e.cl
    L.rec("RecvResponse", (L.lab(e.r(3)), e.r(4) & MASK, s32(e.r(5)), e.r(6) & 0xFF))
    e.set_r(3, L.ret("RecvResponse") & MASK)


@stub(0x1020E358)
def st_sockclose(e):
    e.cl.rec("SockClose", (e.cl.lab(e.r(3)),))


@stub(0x1020C228)
def st_download(e):
    e.cl.rec("_Download", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("_Download") & MASK)


@stub(0x1023A7B0)
def st_shutdown(e):
    e.cl.rec("Shutdown", (s32(e.r(3)), s32(e.r(4))))


@stub(0x1023A780)
def st_recv(e):
    L = e.cl
    L.rec("Recv", (s32(e.r(3)), 1 if e.r(4) == CUR["obj"] + 0x20 else 0, s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Recv") & MASK)


@stub(0x1023A7C8)
def st_closesocket(e):
    e.cl.rec("CloseSocket", (s32(e.r(3)),))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


def ctype_table():
    t = bytearray(256)
    for c in range(256):
        if c == 32:
            t[c] = 0x0C
        elif 9 <= c <= 13:
            t[c] = 0x03
        elif c < 32 or c == 127:
            t[c] = 0x01
        elif 0x30 <= c <= 0x39:
            t[c] = 0x30
        elif 0x41 <= c <= 0x5A or 0x61 <= c <= 0x7A:
            t[c] = 0x41
        elif c < 128:
            t[c] = 0x08
    return bytes(t)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def faddr(self, f):
        return self.obj + self.foff(f)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(OBJ_SIZE)
        self.text = e.alloc(0x600)
        self.argA = e.alloc(0x400)
        self.argB = e.alloc(0x400)
        CUR.update(obj=self.obj, text=self.text)
        o = self.obj
        self.L = e.cl = CL.PpcLog({o: 1, o + 0x255C: 2, o + 0x265C: 3, o + 0x2760: 4, o + 0x2764: 5, o + 0x2768: 6, THREADFN: 7},
                                  case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def foff(self, f):
        return f[1]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def prepare(self):
        # the harness points the TOC slot at its own copy of the import's data: the map is written where the slot points
        self.write(self.e.u32(0x10260778), ctype_table())

    def vptr_ok(self):
        return self.rd_words(self.obj, 1)[0] == 0x102B17F8

    def call(self, op, case):
        o = self.obj
        if op == "parse":
            fn, a = FN_PARSE, [o, self.text]
        elif op == "closed":
            fn, a = FN_CLOSED, [o, o + case["which"]]
        elif op == "download":
            fn, a = FN_DOWNLOAD, [o, self.argA, self.argB]
        elif op == "disconnect":
            fn, a = FN_DISC, [o]
        elif op == "_disconnect":
            fn, a = FN__DISC, [o]
        else:
            fn, a = FN_CTOR, [o, case["ctl"], case["threaded"]]
        self.e.set_r(2, TOC)   # the TOC register (TOC[-0x7548] is 0x10260778)
        r3, _, _ = T.call_ppc(self.e, fn, a)
        r3 &= MASK
        if op == "ctor":
            return 1 if r3 == o else 0
        if op == "closed":
            return None
        return r3


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ftp_alloc(OBJ_SIZE)
        self.text = lib.tpw_ftp_alloc(0x600)
        self.argA = lib.tpw_ftp_alloc(0x400)
        self.argB = lib.tpw_ftp_alloc(0x400)
        o = self.obj
        CL.host_begin(lib, {1: o, 2: o + HOFF["mPath"], 3: o + HOFF["mPath"] + 0x100, 4: o + HOFF["mServer"], 5: o + HOFF["mCondA"],
                            6: o + HOFF["mCondB"], 7: lib.tpw_ftp_addr(b"threadfn")}, case["queues"])
        lib.tpw_ftp_setup(o, self.text)

    def done(self):
        pass

    def foff(self, f):
        return HOFF[f[4]]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def prepare(self):
        pass

    def vptr_ok(self):
        return C.string_at(self.obj, 8) != b"\0" * 8

    def call(self, op, case):
        L, o = self.lib, self.obj
        if op == "parse":
            return L.tpw_ftp_parse(o, self.text) & MASK
        if op == "closed":
            name = "mDataSocket" if case["which"] == 0x2424 else "mListenSocket"
            L.tpw_ftp_closed(o, o + HOFF[name])
            return None
        if op == "download":
            return L.tpw_ftp_download(o, self.argA, self.argB) & MASK
        if op == "disconnect":
            return L.tpw_ftp_disconnect(o) & MASK
        if op == "_disconnect":
            return L.tpw_ftp__disconnect(o) & MASK
        L.tpw_ftp_ctor(o, case["ctl"], case["threaded"])
        return 1


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    lib.tpw_ftp_addr.restype, lib.tpw_ftp_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_ftp_alloc.restype, lib.tpw_ftp_alloc.argtypes = u64, [u32]
    lib.tpw_ftp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ftp_setup.restype, lib.tpw_ftp_setup.argtypes = None, [u64, u64]
    lib.tpw_ftp_parse.restype, lib.tpw_ftp_parse.argtypes = i32, [u64, u64]
    lib.tpw_ftp_closed.restype, lib.tpw_ftp_closed.argtypes = None, [u64, u64]
    lib.tpw_ftp_download.restype, lib.tpw_ftp_download.argtypes = i32, [u64, u64, u64]
    lib.tpw_ftp_disconnect.restype, lib.tpw_ftp_disconnect.argtypes = i32, [u64]
    lib.tpw_ftp__disconnect.restype, lib.tpw_ftp__disconnect.argtypes = i32, [u64]
    lib.tpw_ftp_ctor.restype, lib.tpw_ftp_ctor.argtypes = None, [u64, u32, i32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ftp_layout(out)
    assert n == len(HOST_ORDER) + 1, n
    HOFF.update(dict(zip(HOST_ORDER, list(out)[:n - 1])))
    assert list(out)[n - 1] + 16 <= OBJ_SIZE
    assert HOFF["mListenSocket"] == HOFF["mDataSocket"] + 4 and HOFF["mPath"] + 0x204 <= HOFF["mServer"]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.prepare()
    for f in FIELDS:
        a = s.faddr(f)
        if f[2] == "w":
            s.words(a, case["pre"][f[0]])
        else:
            s.write(a, case["pre"][f[0]])
    s.write(s.text, case["text"] + b"\0")
    s.write(s.argA, case["a2"] + b"\0")
    s.write(s.argB, case["a3"] + b"\0")


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "text": s.read(s.text, len(case["text"]) + 1)}
    for f in FIELDS:
        a = s.faddr(f)
        out[f[0]] = s.rd_words(a, f[3]) if f[2] == "w" else s.read(a, f[3])
    if case["op"] == "ctor":
        out["vptr"] = s.vptr_ok()
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SOCKS = [MASK, 0, 5, 0xFFFF, 0xFFFE, 0x10000, 0x7FFFFFFF, 0x80000000]
FLAGS = [0, 1, 2, 0x80]


def rstr(r, n):
    return bytes(r.randrange(1, 256) for _ in range(n))


def rtext(r):
    alphabet = b"abcXYZ019-._/:" + bytes([1, 2, 7, 0x7F, 0x80, 0xA0, 0xFF])
    out = b""
    for _ in range(r.randrange(0, 8)):
        if r.random() < 0.5:
            out += bytes(r.choice(b" \t\n\r\v\f") for _ in range(r.randrange(1, 4)))
        else:
            out += bytes(r.choice(alphabet) for _ in range(r.randrange(0, 6)))
    return out + b" a b c d e 123 f"


def base(r, op):
    pre = {}
    for key, mac, kind, n, hn in FIELDS:
        if kind == "w":
            pre[key] = [r.getrandbits(32) for _ in range(n)]
        else:
            pre[key] = bytes(r.randrange(1, 256) for _ in range(n))
    pre["sock4"] = [r.choice(SOCKS + [r.getrandbits(32)])]
    pre["data"] = [r.choice(SOCKS + [r.getrandbits(32)])]
    pre["listen"] = [r.choice(SOCKS + [r.getrandbits(32)])]
    for k in ("ready", "started", "threaded"):
        pre[k] = bytes([r.choice(FLAGS)])
    recv = [r.randrange(1, 3000) for _ in range(r.randrange(0, 4))] + [r.choice([0, -1, -5, 0x80000000])]
    queues = {
        "Sleep": [0] * r.randrange(0, 4) + [1],
        "BthreadCreate": [r.choice([-1, 0, 5, 0xFFFF, 0x1234])],
        "RecvResponse": [r.choice([0, 0xDD, 1, 0xDC, 0xDE, -1, 0x1DD, 0x38])],
        "Recv": recv,
        "Atoi": [r.choice([0, 1, 123456, -5, 0x7FFFFFFF, r.getrandbits(31)])],
        "_Download": [r.choice([0, 0x38, 0x43, 0x44, 0x40, 7])],
    }
    return {"op": op, "pre": pre, "queues": queues, "text": rtext(r), "a2": rstr(r, r.choice([0, 3, 0x40, 0xFF, 0x100, 0x101, 0x180])),
            "a3": rstr(r, r.choice([0, 5, 0x80, 0xFF, 0x100, 0x102, 0x1F0])), "which": r.choice([0x2424, 0x2428]),
            "ctl": r.choice([0, 0x12345678, r.getrandbits(32)]), "threaded": r.choice([0, 1])}


def parse_edges():
    r = random.Random(11)
    for text in (b"-rw-r--r-- 1 user group 12345 Jan 1 f.txt", b" a b c d 99", b"\ta\tb\tc\td\t7\t", b"x\r\ny\vz\fw  \n 5 q",
                 b"\x80\xff \x01 \x7f   ~ 0 5 6 7", b"a b c d e", b"  a  b  c  d  e  f"):
        c = base(r, "parse")
        c["text"] = text + b" a b c d e f g"
        yield c
    for q in (0, 1, -1, 0x7FFFFFFF):
        c = base(r, "parse")
        c["queues"]["Atoi"] = [q]
        yield c


def closed_edges():
    r = random.Random(12)
    for sock in SOCKS:
        for which in (0x2424, 0x2428):
            for recv in ([0], [-1], [3, 0], [1, 1, 1, -1], [0x7FFFFFFF, 0]):
                c = base(r, "closed")
                c["pre"]["data" if which == 0x2424 else "listen"] = [sock]
                c["which"] = which
                c["queues"]["Recv"] = recv
                yield c


def thread_edges(op):
    r = random.Random(13 if op == "download" else 14)
    for threaded in (0, 1, 2):
        for started in (0, 1):
            for ready in (0, 1):
                for bt in (-1, 0, 5):
                    c = base(r, op)
                    c["pre"]["threaded"], c["pre"]["started"], c["pre"]["ready"] = bytes([threaded]), bytes([started]), bytes([ready])
                    c["queues"]["BthreadCreate"] = [bt]
                    yield c
    if op == "download":
        for n2 in (0, 0xFF, 0x100, 0x101):
            for n3 in (0, 0xFF, 0x100, 0x101):
                c = base(r, op)
                c["pre"]["threaded"] = b"\0"
                c["a2"], c["a3"] = rstr(r, n2), rstr(r, n3)
                yield c


def disc_edges():
    r = random.Random(15)
    for sock in (MASK, 0, 5):
        for resp in (0, 0xDD, 1, 0xDE, 0xDC, -1, 0x1DD):
            for listen in (MASK, 7):
                c = base(r, "_disconnect")
                c["pre"]["sock4"], c["pre"]["listen"] = [sock], [listen]
                c["queues"]["RecvResponse"] = [resp]
                yield c


def ctor_edges():
    r = random.Random(16)
    for threaded in (0, 1):
        for ctl in (0, 0x12345678, MASK):
            c = base(r, "ctor")
            c["threaded"], c["ctl"] = threaded, ctl
            yield c


def gen(op):
    return lambda r: base(r, op)


TESTS = [
    FnTest("FTPClient::FTPClient", ppc, host, edge=ctor_edges, rnd=gen("ctor")),
    FnTest("FTPClient::Disconnect", ppc, host, edge=lambda: thread_edges("disconnect"), rnd=gen("disconnect")),
    FnTest("FTPClient::_Disconnect", ppc, host, edge=disc_edges, rnd=gen("_disconnect")),
    FnTest("FTPClient::Download", ppc, host, edge=lambda: thread_edges("download"), rnd=gen("download")),
    FnTest("FTPClient::_CloseDSocket", ppc, host, edge=closed_edges, rnd=gen("closed")),
    FnTest("FTPClient::ParseFileSize", ppc, host, edge=parse_edges, rnd=gen("parse")),
]
