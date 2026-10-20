"""Unit v5371, the connection base class's reply reader and local address, against libtpw_unit_Connection_RecvResponse.dylib
(tests/unit_Connection_RecvResponse.cpp), main app, src/Email_Send_CSock_SendData_Connection_RecvResponse.cpp:
  Connection::RecvResponse 0x10201bb4, Connection::GetLocalIPAddress 0x10201d80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (atoi, strncpy, memmove, recv, gethostname, gethostbyname, socket, inet_addr, connect, getsockname, closesocket) are fakes on both
sides that record their arguments (pointers as offsets into the object) and answer from the case's queues; recv copies bytes of the case's
blob, strncpy and memmove really copy.

The world of a case: the object (socket at 4, buffer at 0x20, length at 0x420), an output buffer, the blob, the hostent. What is compared:
the answer, the whole object, the output buffer and the event log.

No fused operations. Not tested: a length above 0x3ff or below 0 (the original would write outside the buffer), a negative copy size.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Connection_RecvResponse"
MASK = 0xFFFFFFFF

FN_R, FN_I = 0x10201BB4, 0x10201D80
OBJ_SIZE = 0x440
OUT_SIZE = 0x80
HOST_LAYOUT = [4, 0x20, 0x420, 0x424]

CTX = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update({FN_R: 408, FN_I: 268})


def _rec(e, name, vals):
    e.cl.rec(name, tuple(vals))


def st_atoi(e):
    p = e.r(3)
    b = e.read(p, 4)
    _rec(e, "Atoi", [p - CTX["obj"]] + list(b))
    e.set_r(3, e.cl.ret("Atoi") & MASK)


def st_strncpy(e):
    d, sr, n = e.r(3), e.r(4), s32(e.r(5))
    _rec(e, "Strncpy", [d - CTX["out"], sr - CTX["obj"], n])
    src = e.read(sr, n) if n > 0 else b""
    k = src.find(b"\0")
    if k >= 0:
        src = src[:k] + b"\0" * (n - k)
    if n > 0:
        e.write(d, src)
    e.set_r(3, d)


def st_memmove(e):
    d, sr, n = e.r(3), e.r(4), s32(e.r(5))
    _rec(e, "Memmove", [d - CTX["obj"], sr - CTX["obj"], n])
    e.write(d, e.read(sr, n))
    e.set_r(3, d)


def st_recv(e):
    sock, buf, size, flags = s32(e.r(3)), e.r(4), s32(e.r(5)), s32(e.r(6))
    _rec(e, "Recv", [sock, buf - CTX["obj"], size, flags])
    n = s32(e.cl.ret("Recv") & MASK)
    if n > size:
        n = size
    if n > 0:
        off = CTX["off"]
        data = CTX["blob"][off:off + n]
        data = data + b"\0" * (n - len(data))
        e.write(buf, data)
        CTX["off"] = off + n
    e.set_r(3, n & MASK)


def st_gethostname(e):
    _rec(e, "Gethostname", [s32(e.r(4))])
    e.write(e.r(3), b"tpwhost\0")
    e.set_r(3, e.cl.ret("Gethostname") & MASK)


def st_gethostbyname(e):
    _rec(e, "GetHostByName", list(e.read(e.r(3), 8)))
    e.set_r(3, CTX["he"])


def st_socket(e):
    _rec(e, "Socket", [s32(e.r(3)), s32(e.r(4)), s32(e.r(5))])
    e.set_r(3, e.cl.ret("Socket") & MASK)


def st_inet_addr(e):
    _rec(e, "InetAddr", list(e.read(e.r(3), 12)))
    e.set_r(3, e.cl.ret("InetAddr") & MASK)


def st_connect(e):
    f, p, ip = struct.unpack(">HHI", e.read(e.r(4), 8))
    _rec(e, "Connect", [s32(e.r(3)), f, p, ip, s32(e.r(5))])
    e.set_r(3, e.cl.ret("Connect") & MASK)


def st_getsockname(e):
    ln = struct.unpack(">I", e.read(e.r(5), 4))[0]
    _rec(e, "GetSockName", [s32(e.r(3)), s32(ln)])
    e.write(e.r(4) + 4, struct.pack(">I", e.cl.ret("GetSockName") & MASK))
    e.set_r(3, 0)


def st_closesocket(e):
    _rec(e, "CloseSocket", [s32(e.r(3))])


for nm, fn in (("atoi", st_atoi), ("strncpy", st_strncpy), ("memmove", st_memmove), ("recv", st_recv),
               ("gethostname", st_gethostname), ("gethostbyname", st_gethostbyname), ("socket", st_socket),
               ("inet_addr", st_inet_addr), ("connect", st_connect), ("getsockname", st_getsockname),
               ("closesocket", st_closesocket)):
    STUBS.imp(nm, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
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
        return self.e.cl.log

    def setctx(self, obj, out, blob, he_on, he_ip):
        CTX.clear()
        CTX.update(obj=obj, out=out, blob=blob, off=0, he=0)
        if he_on:
            ipb = self.alloc(4)
            self.words(ipb, [he_ip])
            lst = self.alloc(8)
            self.words(lst, [ipb])
            he = self.alloc(16)
            self.words(he + 0xC, [lst])
            CTX["he"] = he

    def call_r(self, obj, out, n, flag):
        r3, _, _ = T.call_ppc(self.e, FN_R, [obj, out, n & MASK, flag])
        return s32(r3)

    def call_i(self, obj):
        r3, _, _ = T.call_ppc(self.e, FN_I, [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_conn_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setctx(self, obj, out, blob, he_on, he_ip):
        self.blob = C.create_string_buffer(bytes(blob) + b"\0", len(blob) + 1)
        self.lib.tpw_conn_ctx(obj, out, C.addressof(self.blob), len(blob), 1 if he_on else 0, he_ip & MASK)

    def call_r(self, obj, out, n, flag):
        return self.lib.tpw_conn_recv(obj, out, n, flag)

    def call_i(self, obj):
        return self.lib.tpw_conn_ip() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_conn_alloc.restype, lib.tpw_conn_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_conn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_conn_ctx.restype = None
    lib.tpw_conn_ctx.argtypes = [C.c_uint64, C.c_uint64, C.c_void_p, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_conn_recv.restype, lib.tpw_conn_recv.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint32]
    lib.tpw_conn_ip.restype, lib.tpw_conn_ip.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_conn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case, op):
    try:
        obj = s.alloc(OBJ_SIZE)
        if op == "r":
            out = s.alloc(OUT_SIZE) if case["out"] else 0
            if out:
                s.write(out, b"\xaa" * OUT_SIZE)
            s.setctx(obj, out, case["blob"], False, 0)
            s.words(obj + 4, [case["sock"]])
            s.write(obj + 0x20, case["buf"])
            s.words(obj + 0x420, [case["len"]])
            ans = s.call_r(obj, out, case["n"], case["flag"])
            return {"answer": ans, "log": s.log(), "obj": s.read(obj + 0x20, 0x400),
                    "words": s.rd_words(obj + 4, 1) + s.rd_words(obj + 0x420, 1),
                    "out": s.read(out, OUT_SIZE) if out else b""}
        s.setctx(obj, 0, b"", case["he_on"], case["he_ip"])
        ans = s.call_i(obj)
        return {"answer": ans, "log": s.log(), "obj": s.read(obj + 0x20, 0x400),
                "words": s.rd_words(obj + 4, 1) + s.rd_words(obj + 0x420, 1)}
    finally:
        s.done()


def ppc_r(e, case):
    return run(Ppc(e, case), case, "r")


def host_r(lib, case):
    return run(Host(lib, case), case, "r")


def ppc_i(e, case):
    return run(Ppc(e, case), case, "i")


def host_i(lib, case):
    return run(Host(lib, case), case, "i")


# ---------------------------------------------------------------------------------------------------------------- cases

FRAGS = [b"220 Hello\r\n", b"250-more\r\n", b"250 done\r\n", b"99 low\r\n", b"600 hi\r\n", b"abc\r\n", b"\r", b"\n", b"x", b"12",
         b"\r\n", b"530-\r\n", b"1234\r\n"]
ATOIS = [99, 100, 250, 599, 600, -5, 0, 220, 1000]
RECVS = [-1, 0, 1, 5, 20, 100, 0x3FF, 0x500]


def r_case(r, content=None, blob=None, atoi=None, recv=None, n=None, out=None, flag=None, sock=7):
    if content is None:
        content = b"".join(r.choice(FRAGS) for _ in range(r.randrange(0, 8)))
        if r.random() < 0.1:
            content += b"A" * r.randrange(900, 1100)
    content = content[:0x3FF]
    if blob is None:
        blob = b"".join(r.choice(FRAGS) for _ in range(r.randrange(0, 12)))
        if r.random() < 0.2:
            blob += b"B" * r.randrange(0, 0x600)
    buf = content + bytes(r.randrange(256) for _ in range(0x400 - len(content)))
    atoi = atoi if atoi is not None else [r.choice(ATOIS) for _ in range(r.randrange(1, 6))]
    recv = recv if recv is not None else [r.choice(RECVS) for _ in range(r.randrange(1, 5))]
    return {"queues": {"Atoi": atoi, "Recv": recv}, "buf": buf, "len": len(content), "blob": blob, "sock": sock,
            "n": r.choice([0, 1, 4, 10, 40, 60]) if n is None else n,
            "out": (r.random() < 0.8) if out is None else out, "flag": r.choice([0, 1]) if flag is None else flag}


def r_edges():
    r = random.Random(5371)
    yield r_case(r, b"", b"", [220], [0])
    yield r_case(r, b"", b"", [220], [-1])
    yield r_case(r, b"", b"220 ok\r\n", [220], [3, 100], n=20, flag=0)
    for flag in (0, 1):
        for out in (True, False):
            for code in (99, 100, 599, 600, 0, -3):
                yield r_case(r, b"xx\r\n220 OK\r\nrest", b"", [code, 250], [0], n=10, out=out, flag=flag)
    yield r_case(r, b"220-first\r\n220 last\r\ntail", b"", [220, 221], [0], n=30, out=True, flag=1)
    yield r_case(r, b"220-first\r\n220 last\r\ntail", b"", [220, 221], [0], n=30, out=True, flag=0)
    yield r_case(r, b"220 a\r", b"\n250 b\r\n", [220, 250], [1, 100], n=8, out=True, flag=1)
    yield r_case(r, b"abc\r\n", b"", [100], [0], n=8, out=True, flag=0)
    yield r_case(r, b"A" * 0x3FF, b"C" * 50, [100], [50], n=8)
    yield r_case(r, b"\r\n\r\n", b"", [500], [0], n=4, out=True, flag=1)


def r_random(r):
    return r_case(r)


def i_case(r, hn=None, he_on=None, he_ip=None, sock=None, conn=None, gs=None, inet=None):
    c = {"queues": {}}
    c["queues"]["Gethostname"] = [r.choice([0, 0, 1, -1]) if hn is None else hn]
    c["he_on"] = (r.random() < 0.6) if he_on is None else he_on
    c["he_ip"] = r.choice([0, 0x0A000001, 0x01020304, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]) if he_ip is None else he_ip
    c["queues"]["Socket"] = [r.choice([-1, 0, 3, 100, 0xFFFF]) if sock is None else sock]
    c["queues"]["Connect"] = [r.choice([0, 0, 1, -1]) if conn is None else conn]
    c["queues"]["GetSockName"] = [r.choice([0, 0x7F000001, r.getrandbits(32)]) if gs is None else gs]
    c["queues"]["InetAddr"] = [r.choice([0, 0x807F3201, r.getrandbits(32)]) if inet is None else inet]
    return c


def i_edges():
    r = random.Random(137)
    for hn in (0, 1):
        for he_on in (False, True):
            for he_ip in (0, 0x01020304):
                for sock in (-1, 5):
                    for conn in (0, 1):
                        yield i_case(r, hn, he_on, he_ip, sock, conn, 0x0A0B0C0D, 0x807F3201)
    yield i_case(r, 0, True, 0, 5, 0, 0, 0)


def i_random(r):
    return i_case(r)


TESTS = [
    FnTest("Connection::RecvResponse", ppc_r, host_r, edge=r_edges, rnd=r_random),
    FnTest("Connection::GetLocalIPAddress", ppc_i, host_i, edge=i_edges, rnd=i_random),
]
