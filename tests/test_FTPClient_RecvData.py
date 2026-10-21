"""Unit v5101, the FTP client: RecvData, the destructor body (the Mac's hidden delete flag is the argument), Connect and
_EnterPassiveMode, against libtpw_unit_FTPClient_RecvData.dylib (tests/unit_FTPClient_RecvData.cpp), main app, src/game/online/FTPClient.cpp:
  RecvData 0x1020b59c, ~FTPClient 0x1020b46c, Connect 0x1020b730, _EnterPassiveMode 0x1020b904

A case is a world and a call. The object is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian)
and in the host library's memory (the host's offsets come from the library's layout export). Every callee outside the unit (the socket
and C library imports, the connection base class, the thread library, _Connect) is a fake on both sides that records its arguments and
answers from the case's queues; recv fills the buffer with a pattern (a newline at a queued index), RecvResponse writes one of a table
of response lines. Pure C library calls (strchr, strncpy, strlen, atoi, sprintf) run for real on the host and as exact Python
equivalents on the PowerPC side. The inet_addr and GetLocalIPAddress answers are network order words: the host fake byte swaps so the
object's bytes are the same.

Compared: the answer, the call log and every member the functions touch (the whole buffer, sockets, sockaddr bytes, the three argument
copies, flags). No fused operations. Not tested: a NULL this in the destructor (the host cannot call a member on NULL), the vtable
stores (the compiler's on the host), unterminated PASV responses without commas (strchr NULL + 1 is a wild read), a count that recv
overruns the 0x2000 byte buffer with.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FTPClient_RecvData"
MASK = 0xFFFFFFFF

FN_RECV, FN_DTOR, FN_CONNECT, FN_PASV = 0x1020B59C, 0x1020B46C, 0x1020B730, 0x1020B904
SIZES = {FN_RECV: 360, FN_DTOR: 264, FN_CONNECT: 420, FN_PASV: 688}
OWN = set(SIZES)

OBJ_SIZE_MAC = 0x2780
THREADFN_TVECTOR = 0x10268728

# name, Mac offset, size, kind ('i' integer, 'b' bytes); the host offsets follow the library's layout export in this order
FIELDS = [
    ("total", 0x18, 4, "i"), ("done", 0x1C, 4, "i"), ("buf", 0x424, 0x2000, "b"), ("dsock", 0x2424, 4, "i"),
    ("lsock", 0x2428, 4, "i"), ("family", 0x242C, 2, "i"), ("port", 0x242E, 2, "i"), ("ip", 0x2430, 4, "b"),
    ("zero", 0x2434, 8, "b"), ("strs", 0x243C, 0x120, "b"), ("path", 0x255C, 0x204, "b"), ("f276c", 0x276C, 4, "i"),
    ("ready", 0x2770, 1, "i"), ("started", 0x2771, 1, "i"), ("threaded", 0x2772, 1, "i"),
]
MAC_OFF = {f[0]: f[1] for f in FIELDS}
MAC_OFF.update(condA=0x2764, condB=0x2768, server=0x2760)
HOFF = {}
HSIZE = [0]

RESP = [
    b"227 Entering Passive Mode (192,168,1,2,19,136).\r\n", b"227 no paren here\r\n", b"(1,2,3,4,5,6)",
    b"x(300,257,-1,255,256,70000)", b"(0,0,0,0,255,255)", b"(10,20,30,40,1,2", b"(127,0,0,1,-3,65)",
]
PORT_FMT = b"PORT %d,%d,%d,%d,%d,%d\r\n"

ST = {"obj": 0}


def s32(v):
    return T.s32(v)


def cbytes(e, p):
    out = b""
    while True:
        chunk = e.read(p + len(out), 64)
        i = chunk.find(b"\0")
        if i >= 0:
            return out + chunk[:i]
        out += chunk


def rv(L, key):
    return s32(L.ret(key))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def which(addr):
    d = addr - ST["obj"]
    return {0x2764: 1, 0x2768: 2, 0x2760: 3}.get(d, 0)


def s_accept(e):
    L = e.cl
    L.rec("accept", (s32(e.r(3)), e.u32(e.r(5))))
    e.set_r(3, rv(L, "accept") & MASK)


def s_recv(e):
    L = e.cl
    buf = e.r(4)
    L.rec("recv", (s32(e.r(3)), s32(e.r(5)), s32(e.r(6))))
    n = rv(L, "recv")
    nl = rv(L, "nl")
    if n > 0:
        b = bytearray(97 + i % 26 for i in range(n))
        if 0 <= nl < n:
            b[nl] = 10
        e.write(buf, bytes(b))
    e.set_r(3, n & MASK)


def s_fwrite(e):
    L = e.cl
    n = s32(e.r(5))
    data = e.read(e.r(3), n) if n > 0 else b""
    L.rec("fwrite", (s32(e.r(4)), n, e.r(6), sum(data)))


def s_fclose(e):
    e.cl.rec("fclose", (e.r(3),))


def s_socket(e):
    L = e.cl
    L.rec("socket", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, rv(L, "socket") & MASK)


def s_bind(e):
    L = e.cl
    a = e.r(4)
    fam = int.from_bytes(e.read(a, 2), "big")
    port = int.from_bytes(e.read(a + 2, 2), "big")
    ip = int.from_bytes(e.read(a + 4, 4), "big")
    L.rec("bind", (s32(e.r(3)), fam, port, ip, s32(e.r(5))))
    e.set_r(3, rv(L, "bind") & MASK)


def s_listen(e):
    L = e.cl
    L.rec("listen", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, rv(L, "listen") & MASK)


def s_getsockname(e):
    L = e.cl
    L.rec("getsockname", (s32(e.r(3)), e.u32(e.r(5))))
    port = rv(L, "gsn") & 0xFFFF
    e.write(e.r(4) + 2, struct.pack(">H", port))
    e.set_r(3, 0)


def s_sleep(e):
    L = e.cl
    L.rec("Sleep", (e.r(3),))
    if rv(L, "sleepready") != 0:
        e.write(ST["obj"] + 0x2770, b"\1")


def s_signal(e):
    e.cl.rec("signal", (which(e.r(3)),))


def s_wait(e):
    e.cl.rec("wait", (which(e.r(3)), s32(e.r(4))))


def s_conddtor(e):
    e.cl.rec("condDtor", (which(e.r(3)), s32(e.r(4))))


def s_wsacleanup(e):
    e.cl.rec("WSACleanup", ())


def s_stringdtor(e):
    e.cl.rec("stringDtor", (s32(e.r(4)),))


def s_csockdtor(e):
    e.cl.rec("CSockDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def s_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def s_bthread_create(e):
    L = e.cl
    L.rec("bthread_create", (1 if e.r(3) == THREADFN_TVECTOR else 0, L.lab(e.r(4))))
    e.set_r(3, rv(L, "bthread_create") & MASK)


def s_recvresponse(e):
    L = e.cl
    buf = e.r(4)
    L.rec("RecvResponse", (L.lab(e.r(3)), 1 if buf else 0, s32(e.r(5)), e.r(6) & 0xFF))
    code = rv(L, "RecvResponse")
    rid = rv(L, "respid")
    if buf:
        e.write(buf, RESP[rid] + b"\0")
    e.set_r(3, code & MASK)


def s_getlocal(e):
    L = e.cl
    L.rec("GetLocalIPAddress", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetLocalIPAddress") & MASK)


def s_connect(e):
    L = e.cl
    L.rec("_Connect", (L.lab(e.r(3)),))
    e.set_r(3, rv(L, "_Connect") & MASK)


def s_strchr(e):
    p, ch = e.r(3), e.r(4) & 0xFF
    s = cbytes(e, p) + b"\0"
    i = s.find(bytes([ch]))
    e.set_r(3, (p + i) & MASK if i >= 0 else 0)


def s_strncpy(e):
    dst, src, n = e.r(3), e.r(4), e.r(5)
    s = cbytes(e, src)[:n]
    e.write(dst, s + b"\0" * (n - len(s)))


def s_strlen(e):
    e.set_r(3, len(cbytes(e, e.r(3))))


def s_atoi(e):
    s = cbytes(e, e.r(3)).lstrip(b" \t\n\v\f\r")
    i = 0
    if s[:1] in (b"+", b"-"):
        i = 1
    while i < len(s) and 48 <= s[i] <= 57:
        i += 1
    digits = s[:i]
    v = int(digits) if digits not in (b"", b"+", b"-") else 0
    e.set_r(3, v & MASK)


def s_sprintf(e):
    fmt = cbytes(e, e.r(4))
    out = fmt % tuple(s32(e.r(i)) for i in range(5, 11))
    e.write(e.r(3), out + b"\0")
    e.set_r(3, len(out))


def glue(fn):
    """An import: the caller reloads its TOC pointer from 0x14(r1) after the call, where the real glue saved it."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return wrapped


STUBS.internal(0x1023A840, glue(s_accept))
STUBS.internal(0x1023A780, glue(s_recv))
STUBS.internal(0x1023A5E8, glue(s_fwrite))
STUBS.internal(0x102388C0, glue(s_fclose))
STUBS.internal(0x1023A708, glue(s_socket))
STUBS.internal(0x1023A738, glue(s_bind))
STUBS.internal(0x1023A828, glue(s_listen))
STUBS.internal(0x1023A810, glue(s_getsockname))
STUBS.internal(0x1023A888, glue(s_sleep))
STUBS.internal(0x10200FB4, s_signal)
STUBS.internal(0x10200F30, s_wait)
STUBS.internal(0x10200EA8, s_conddtor)
STUBS.internal(0x102386F8, glue(s_wsacleanup))
STUBS.internal(0x1000221C, s_stringdtor)
STUBS.internal(0x10201FCC, s_csockdtor)
STUBS.internal(0x10238218, glue(s_delete))
STUBS.internal(0x10200C34, s_bthread_create)
STUBS.internal(0x10201BB4, s_recvresponse)
STUBS.internal(0x10201D80, s_getlocal)
STUBS.internal(0x1020BBE8, s_connect)
STUBS.internal(0x1023A7E0, glue(s_strchr))
STUBS.internal(0x10238500, glue(s_strncpy))
STUBS.internal(0x10238650, glue(s_strlen))
STUBS.internal(0x10238590, glue(s_atoi))
STUBS.internal(0x10238578, glue(s_sprintf))
STUBS.internal(0x10201B14, CL.recorder("SendCommandLine", "Ls"))
STUBS.internal(0x102389B0, glue(CL.recorder("fopen", "ss", ret="i")))
STUBS.internal(0x1023A720, glue(CL.recorder("inet_addr", "s", ret="i")))
STUBS.internal(0x101B2440, CL.recorder("assign", "xsu"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def endian(self):
        return "big" if self.fmt == ">I" else "little"

    def setf(self, name, value):
        _, _, size, kind = next(f for f in FIELDS if f[0] == name)
        if kind == "i":
            self.write(self.obj + self.off(name), (value & ((1 << (8 * size)) - 1)).to_bytes(size, self.endian()))
        else:
            self.write(self.obj + self.off(name), bytes(value))

    def getf(self, name):
        _, _, size, kind = next(f for f in FIELDS if f[0] == name)
        raw = self.read(self.obj + self.off(name), size)
        return int.from_bytes(raw, self.endian()) if kind == "i" else raw

    def cstr_in(self, text):
        if text is None:
            return 0
        a = self.alloc(len(text) + 8)
        self.write(a, text + b"\0")
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(OBJ_SIZE_MAC)
        ST["obj"] = self.obj
        self.L = e.cl = CL.PpcLog({self.obj: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def off(self, name):
        return MAC_OFF[name]

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

    def set_rep(self, length, text):
        data = self.cstr_in(text)
        rep = self.alloc(16)
        self.write(rep, struct.pack(">IIII", length & MASK, 3, 1, data))
        self.write(self.obj + 0x2760, struct.pack(">I", rep))

    def call(self, op, args):
        o = self.obj
        if op == "recv":
            r = T.call_ppc(self.e, FN_RECV, [o, args[0], args[1] & MASK])[0]
        elif op == "dtor":
            r = T.call_ppc(self.e, FN_DTOR, [o, args[0] & 0xFFFF])[0]
            return 1 if r == o else 0
        elif op == "connect":
            ptrs = [self.cstr_in(t) for t in args]
            r = T.call_ppc(self.e, FN_CONNECT, [o] + ptrs)[0]
        else:
            r = T.call_ppc(self.e, FN_PASV, [o])[0]
        return s32(r)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ftp_alloc(HSIZE[0])
        CL.host_begin(lib, {1: self.obj}, case["queues"])

    def off(self, name):
        return HOFF[name]

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ftp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_rep(self, length, text):
        self.lib.tpw_ftp_setrep(self.obj, length, self.cstr_in(text))

    def call(self, op, args):
        o = self.obj
        if op == "recv":
            return self.lib.tpw_ftp_recvdata(o, args[0], s32(args[1]))
        if op == "dtor":
            return 1 if self.lib.tpw_ftp_destruct(o, s32(T.s16(args[0]))) == o else 0
        if op == "connect":
            return self.lib.tpw_ftp_connect(o, *[self.cstr_in(t) for t in args])
        return self.lib.tpw_ftp_pasv(o)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ftp_alloc.restype, lib.tpw_ftp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ftp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ftp_setrep.argtypes = [C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_ftp_recvdata.restype, lib.tpw_ftp_recvdata.argtypes = C.c_int32, [C.c_uint64, C.c_uint32, C.c_int32]
    lib.tpw_ftp_destruct.restype, lib.tpw_ftp_destruct.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_ftp_connect.restype, lib.tpw_ftp_connect.argtypes = C.c_int32, [C.c_uint64] * 5
    lib.tpw_ftp_pasv.restype, lib.tpw_ftp_pasv.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ftp_layout(out)
    vals = list(out)[:n]
    HSIZE[0] = vals[0]
    for f, v in zip(FIELDS, vals[1:]):
        HOFF[f[0]] = v
    HOFF.update(condA=vals[len(FIELDS) + 1], condB=vals[len(FIELDS) + 2], server=vals[len(FIELDS) + 3])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for name, value in case["init"].items():
        s.setf(name, value)
    if case.get("rep") is not None:
        s.set_rep(*case["rep"])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for f in FIELDS:
        out[f[0]] = s.getf(f[0])
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rbytes(r, n):
    return bytes(r.randrange(256) for _ in range(n))


def rinit(r):
    return {"total": r.randrange(1 << 32), "done": r.choice([0, 5, 0xFFFFFFFF, r.randrange(1 << 32)]),
            "dsock": r.choice([-1, -1, 4, 9]), "lsock": r.choice([-1, 3, 7]), "family": r.randrange(1 << 16),
            "port": r.randrange(1 << 16), "ip": rbytes(r, 4), "zero": b"\0" * 8, "buf": rbytes(r, 0x2000),
            "f276c": r.randrange(1 << 32), "ready": r.choice([0, 1]), "started": r.choice([0, 1]),
            "threaded": r.choice([0, 1]), "path": b"file" + bytes(r.randrange(97, 123) for _ in range(5)) + b"\0" * 20}


def pad_path(p):
    return p + b"\0" * 4


# ---- RecvData
def recv_case(r, **kw):
    init = rinit(r)
    entries = [r.choice([1, 5, 33, 100, 200]) for _ in range(r.randrange(0, 5))] + [r.choice([0, -1, -5])]
    save = r.choice([0, 1])
    pre = 0
    reach = r.randrange(0, len(entries))
    pre = sum(entries[:reach]) if all(x > 0 for x in entries[:reach]) else 0
    count = r.choice([pre, pre, r.randrange(0, 400), 0, 1])
    c = {"op": "recv", "init": init, "args": [save, count],
         "queues": {"recv": entries, "nl": [r.choice([-1, -1, 0, 3, 40, 150]) for _ in range(3)],
                    "accept": [r.choice([-1, 6, 12])], "fopen": [r.choice([0, 0x1234, 0xFFFF0000])]}}
    c.update(kw)
    return c


def recv_edges():
    r = random.Random(101)
    base = lambda **kw: recv_case(r, **kw)

    def mk(lsock, dsock, save, accept, fopen, entries, nl, count):
        c = recv_case(r)
        c["init"]["lsock"], c["init"]["dsock"] = lsock, dsock
        c["args"] = [save, count]
        c["queues"] = {"recv": entries, "nl": nl, "accept": [accept], "fopen": [fopen]}
        return c

    yield mk(7, -1, 0, -1, 0, [10, 0], [-1], 10)           # accept fails
    yield mk(7, -1, 0, 6, 0, [10, 0], [-1], 10)            # accept works
    yield mk(-1, -1, 0, 6, 0, [10, 0], [-1], 10)           # no listen socket: accept skipped
    yield mk(7, 4, 0, 6, 0, [10, 0], [-1], 10)             # already connected
    yield mk(7, -1, 1, 6, 0, [10, 0], [-1], 10)            # fopen fails, the loop still runs
    yield mk(7, -1, 1, 6, 0x1234, [10, 20, 0], [-1], 30)   # file written
    yield mk(7, 4, 1, 6, 0x1234, [10, 20, 0], [-1], 40)    # recv fails after two writes
    yield mk(7, 4, 0, 6, 0, [10, 20, 30], [5], 1000)       # a newline ends it
    yield mk(7, 4, 0, 6, 0, [10, 20, 0], [-1], 1000)       # recv returns 0
    yield mk(7, 4, 0, 6, 0, [10, -1], [-1], 1000)         # recv returns -1
    yield mk(7, 4, 0, 6, 0, [10], [-1], 0)                 # count 0: no recv
    yield mk(7, 4, 1, 6, 0x1234, [10], [-1], 0)            # count 0 with a file: opened and closed
    yield mk(7, 4, 0, 6, 0, [10, 20, 0], [9], 1000)        # the newline is the last byte of the first piece
    yield mk(7, 4, 0, 6, 0, [10, 20, 0], [10, 3], 1000)    # the newline is past the data
    yield mk(7, 4, 1, 6, 0x1234, [10, 5, 0], [3], 15)      # exact count with a file
    yield mk(7, 4, 0, 6, 0, [10, 0], [-1], 5)              # overshoot: never equal, ends on recv failure
    for _ in range(20):
        yield base()


def recv_random(r):
    return recv_case(r)


# ---- ~FTPClient
def dtor_case(r, flag=None):
    init = rinit(r)
    ready = init["ready"]
    n = r.randrange(0, 4)
    sl = [0] * n + [1]
    return {"op": "dtor", "init": init, "args": [r.choice([-1, 0, 1, 2, 0x7FFF, 0x8000, 0x10000]) if flag is None else flag],
            "queues": {"sleepready": sl}}


def dtor_edges():
    r = random.Random(102)
    for flag in (0, 1, -1, 2, 0x7FFF, 0x8000):
        for started in (0, 1):
            for ready in (0, 1):
                c = dtor_case(r, flag)
                c["init"]["started"], c["init"]["ready"] = started, ready
                yield c


def dtor_random(r):
    return dtor_case(r)


# ---- Connect
POOL = [b"", b"host.example.com", b"user\r\nrest", b"pass\nword", b"a\rb", b"x" * 0x5F, b"y" * 0x60, b"z" * 0x70,
        b"\r", b"\n", b"short"]


def connect_case(r, **kw):
    init = rinit(r)
    init["strs"] = rbytes(r, 0x120)
    init["path"] = b"\0" * 0x204
    args = [r.choice(POOL), r.choice(POOL), r.choice(POOL), r.choice([None, b"", b"ftp.server.net", b"10.0.0.1"])]
    sl = [r.choice([0, 0]) for _ in range(r.randrange(0, 3))] + [1]
    c = {"op": "connect", "init": init, "args": args,
         "queues": {"_Connect": [r.choice([0, 1, 0x35, -1])], "bthread_create": [r.choice([-1, 5, 0, 0x1000])], "sleepready": sl}}
    c.update(kw)
    return c


def connect_edges():
    r = random.Random(103)
    for threaded in (0, 1):
        for started in (0, 1):
            for ready in (0, 1):
                for bt in (-1, 5):
                    for srv in (None, b"ftp.server.net"):
                        c = connect_case(r)
                        c["init"]["threaded"], c["init"]["started"], c["init"]["ready"] = threaded, started, ready
                        c["queues"]["bthread_create"] = [bt]
                        c["args"][3] = srv
                        yield c
    for a in POOL:
        for b_ in POOL:
            c = connect_case(r)
            c["args"][1], c["args"][2] = a, b_
            yield c


def connect_random(r):
    return connect_case(r)


# ---- _EnterPassiveMode
def pasv_case(r):
    init = rinit(r)
    kind = r.choice(["pasv", "active"])
    if kind == "pasv":
        codes = [r.choice([0xE3, 0xE3, 0xE3, 0])]
        ids = [r.randrange(len(RESP))]
    else:
        codes = [r.choice([0xC8, 0x14E, 1]), r.choice([0xC8, 0xC8, 0, 0x1F4])]
        ids = [0, 0]
    rep = r.choice([(0, None), (0, b""), (14, b"192.168.0.77"), (3, b"1.2.3.4")])
    return {"op": "pasv", "init": init, "args": [], "rep": rep,
            "queues": {"RecvResponse": codes, "respid": ids, "socket": [r.choice([-1, 5, 0x20])],
                       "inet_addr": [r.choice([0, 0x01020304, 0xC0A8004D, 0xFF000080, 0x80000000])],
                       "GetLocalIPAddress": [r.choice([0x7F000001, 0x0A000001, 0xFFFFFFFF, 0x80818283])],
                       "bind": [r.choice([0, 0, 1, -1])], "listen": [r.choice([0, 0, 1, -1])],
                       "gsn": [r.choice([0, 0x1234, 0xFFFF, 0x0102, 0x8001, 0x00FF, 0xFF00])]}}


def pasv_edges():
    r = random.Random(104)
    for rid in range(len(RESP)):
        c = pasv_case(r)
        c["queues"]["RecvResponse"], c["queues"]["respid"] = [0xE3], [rid]
        yield c
    c = pasv_case(r)
    c["queues"]["RecvResponse"] = [0]
    yield c
    for second in (0, 0xC8, 0xC7, 0xC9):
        for rep in ((0, None), (14, b"192.168.0.77")):
            for bind_, listen_ in ((0, 0), (1, 0), (0, -1)):
                c = pasv_case(r)
                c["queues"]["RecvResponse"], c["queues"]["respid"] = [0x14E, second], [0, 0]
                c["rep"] = rep
                c["queues"]["bind"], c["queues"]["listen"] = [bind_], [listen_]
                yield c


def pasv_random(r):
    return pasv_case(r)


TESTS = [
    FnTest("FTPClient::RecvData", ppc, host, edge=recv_edges, rnd=recv_random),
    FnTest("FTPClient::~FTPClient", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("FTPClient::Connect", ppc, host, edge=connect_edges, rnd=connect_random),
    FnTest("FTPClient::_EnterPassiveMode", ppc, host, edge=pasv_edges, rnd=pasv_random),
]
