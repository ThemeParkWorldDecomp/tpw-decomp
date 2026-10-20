"""Unit v5227, FTPClient::_Connect and FTPClient::_Download, against libtpw_unit_FTPClient__Download.dylib
(tests/unit_FTPClient__Download.cpp), main app ("SimTheme Park"), src/FTPClient_RecvData_FTPClient__Download.cpp:
  FTPClient::_Connect 0x1020bbe8, FTPClient::_Download 0x1020c228

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian)
for the original, and in the host library's memory (little-endian, the host's own FTPClient layout) for the translation. Every callee
outside the unit is a recording fake on both sides: SendCommandLine, RecvResponse, RecvData, ParseFileSize, _EnterPassiveMode,
Sock::Close, _CloseDSocket, and the imports socket, bind, connect, inet_addr, gethostbyname, shutdown, recv, closesocket, strcmp (answers
from the case's queues; the PowerPC memset and sprintf are the harness's standard stubs).

The world of a case: the control socket and its sockaddr_in, the data and listen sockets, the saved passive address and port, the server
string (a std::string representation), the host/user/password/path texts, the progress words and a hostent. What is compared: the answer,
the call log and the object's changed words (socket, sockaddr, progress words, data and listen sockets).

No fused operations. The Mac's receive buffer at this+0x20 and the response buffer at this+0x424 are the host's mBuffer; the fakes log
only whether recv and ParseFileSize got that buffer. Not tested: nothing is left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FTPClient__Download"
MASK = 0xFFFFFFFF

FN_CONNECT, FN_DOWNLOAD = 0x1020BBE8, 0x1020C228
SIZES = {FN_CONNECT: 676, FN_DOWNLOAD: 1264}
OWN = set(SIZES)

ADDR = {}
# sizeof, mF04, mTotal, mDone, mBuffer, mDataSocket, mListenSocket, mPort, mIpWord, mStrings, mPath, mServer, FTPStringRep.mData, its size
HOST_LAYOUT = [0x2378, 8, 0x1C, 0x20, 0x24, 0x2024, 0x2028, 0x202E, 0x2030, 0x203C, 0x215C, 0x2360, 16, 24]

# Offsets in the object: the Mac's and the host's. sa is the control sockaddr_in (family, port, ip, 8 zero bytes).
MAC = dict(size=0x2780, zero=0, sock=4, sa=8, total=0x18, done=0x1C, dsock=0x2424, lsock=0x2428, port=0x242E, ipw=0x2430,
           strs=0x243C, path=0x255C, server=0x2760)
HOST = dict(size=HOST_LAYOUT[0], zero=8, sock=8, sa=12, total=0x1C, done=0x20, dsock=0x2024, lsock=0x2028, port=0x202E, ipw=0x2030,
            strs=0x203C, path=0x215C, server=0x2360)

OBJ = [0]       # the Mac object (persistent), set by setup


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_addr_call(name):
    """bind / connect (int sock, sockaddr_in* addr, int length): the family, port and ip words are what the real one reads."""
    def f(e):
        L = e.cl
        fam, port, ip = struct.unpack(">HHI", e.read(e.r(4), 8))
        L.rec(name, (s32(e.r(3)), fam, port, ip, s32(e.r(5))))
        e.set_r(3, L.ret(name) & MASK)
    return f


def stub_recv(e):
    L = e.cl
    L.rec("recv", (s32(e.r(3)), 1 if e.r(4) == OBJ[0] + 0x20 else 0, s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("recv") & MASK)


def stub_parse(e):
    L = e.cl
    L.rec("ParseFileSize", (L.lab(e.r(3)), 1 if e.r(4) == OBJ[0] + 0x424 else 0))
    e.set_r(3, L.ret("ParseFileSize") & MASK)


def stub_closed(e):
    L = e.cl
    L.rec("CloseDSocket", (L.lab(e.r(3)), 1 if e.r(4) == OBJ[0] + 0x2428 else 0))


def stub_strcmp(e):
    e.set_r(3, 0 if e.cstr(e.r(3)) == e.cstr(e.r(4)) else 1)


STUBS.internal(0x10201B14, CL.recorder("SendCommandLine", "Ls"))
STUBS.internal(0x10201BB4, CL.recorder("RecvResponse", "Liib", ret="i"))
STUBS.internal(0x1020B59C, CL.recorder("RecvData", "Lbi", ret="i"))
STUBS.internal(0x1020B904, CL.recorder("EnterPassiveMode", "L", ret="i"))
STUBS.internal(0x1020E358, CL.recorder("SockClose", "L"))
STUBS.internal(0x1020C744, stub_parse)
STUBS.internal(0x1020C174, stub_closed)
STUBS.imp("socket", CL.recorder("socket", "iii", ret="i"))
STUBS.imp("inet_addr", CL.recorder("inet_addr", "s", ret="i"))
STUBS.imp("gethostbyname", CL.recorder("gethostbyname", "s", ret=lambda e, log: e.cl.hostent if e.cl.ret("gethostbyname") else 0))
STUBS.imp("shutdown", CL.recorder("shutdown", "ii", ret="i"))
STUBS.imp("closesocket", CL.recorder("closesocket", "i", ret="i"))
STUBS.imp("bind", stub_addr_call("bind"))
STUBS.imp("connect", stub_addr_call("connect"))
STUBS.imp("recv", stub_recv)
STUBS.imp("strcmp", stub_strcmp)


def setup(runner):
    T.install_std_stubs(runner.emu)
    OBJ[0] = T.persist_alloc(runner.emu, 0x2800)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def put(self, a, bits, v):
        self.write(a, struct.pack(self.fmt[0] + {16: "H", 32: "I"}[bits], v & ((1 << bits) - 1)))

    def get(self, a, bits):
        return struct.unpack(self.fmt[0] + {16: "H", 32: "I"}[bits], self.read(a, bits // 8))[0]

    def cstring(self, text):
        a = self.alloc(len(text) + 8)
        self.write(a, text.encode() + b"\0")
        return a


class Ppc(Side):
    fmt = ">I"
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.obj = OBJ[0]
        self.L = e.cl = CL.PpcLog({self.obj: 1}, case["queues"])
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

    def set_server(self, length, text):
        rep = self.alloc(16)
        self.words(rep, [length, 0, 1, self.cstring(text)])
        self.words(self.obj + self.o["server"], [rep])

    def make_hostent(self, ip):
        ipa, lst, h = self.alloc(4), self.alloc(8), self.alloc(16)
        self.words(ipa, [ip])
        self.words(lst, [ipa, 0])
        self.words(h + 0xC, [lst])
        self.L.hostent = h

    def call(self, fn):
        r3, _, _ = T.call_ppc(self.e, fn, [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    o = HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.obj
        CL.host_begin(lib, {1: self.obj}, case["queues"])

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

    def set_server(self, length, text):
        rep = self.alloc(24)
        self.write(rep, struct.pack("<iiiIQ", length, 0, 1, 0, self.cstring(text)))
        self.write(self.obj + self.o["server"], struct.pack("<Q", rep))

    def make_hostent(self, ip):
        ipa, lst, h = self.alloc(4), self.alloc(8), self.alloc(32)
        self.words(ipa, [ip])
        self.write(lst, struct.pack("<Q", ipa))
        self.write(h + 24, struct.pack("<Q", lst))
        self.write(self.lib.tpw_ftp_addr(b"hostent"), struct.pack("<Q", h))

    def call(self, fn):
        f = self.lib.tpw_ftp_connect if fn == FN_CONNECT else self.lib.tpw_ftp_download
        return f(self.obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ftp_addr.restype, lib.tpw_ftp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ftp_alloc.restype, lib.tpw_ftp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ftp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ftp_connect.restype, lib.tpw_ftp_connect.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_ftp_download.restype, lib.tpw_ftp_download.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ftp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, [hex(x) for x in list(out)[:n]]
    lib.obj = lib.tpw_ftp_alloc(HOST_LAYOUT[0] + 16)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o, ob = s.o, s.obj
    s.write(ob + o["zero"], bytes(o["size"] - o["zero"]))
    s.put(ob + o["sock"], 32, case["sock"])
    fam, port, ip, zero = case["sa"]
    sa = ob + o["sa"]
    s.put(sa, 16, fam)
    s.put(sa + 2, 16, port)
    s.put(sa + 4, 32, ip)
    s.write(sa + 8, zero)
    s.put(ob + o["total"], 32, case["total"])
    s.put(ob + o["done"], 32, case["done"])
    s.put(ob + o["dsock"], 32, case["dsock"])
    s.put(ob + o["lsock"], 32, case["lsock"])
    s.put(ob + o["port"], 16, case["port"])
    s.put(ob + o["ipw"], 32, case["ipw"])
    s.write(ob + o["strs"], case["host"].encode() + b"\0")
    s.write(ob + o["strs"] + 0x60, case["user"].encode() + b"\0")
    s.write(ob + o["strs"] + 0xC0, case["pw"].encode() + b"\0")
    s.write(ob + o["path"] + 0x100, case["path"].encode() + b"\0")
    s.set_server(case["slen"], case["stext"])
    s.make_hostent(case["hostip"])


def observe(s, case, answer):
    o, ob = s.o, s.obj
    sa = ob + o["sa"]
    return {
        "answer": answer, "log": s.log(),
        "sock": s.get(ob + o["sock"], 32),
        "sa": (s.get(sa, 16), s.get(sa + 2, 16), s.get(sa + 4, 32), s.read(sa + 8, 8)),
        "total": s.get(ob + o["total"], 32), "done": s.get(ob + o["done"], 32),
        "dsock": s.get(ob + o["dsock"], 32), "lsock": s.get(ob + o["lsock"], 32),
        "port": s.get(ob + o["port"], 16), "ipw": s.get(ob + o["ipw"], 32),
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["fn"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

HOSTS = ["ftp.example.com", "255.255.255.255", "10.1.2.3", ""]
U32 = [0, 1, 0x0100007F, 0x7F000001, MASK, 0x80000000, 0x12345678]


def base(r, fn, seq):
    q = {
        "RecvResponse": seq,
        "EnterPassiveMode": [r.choice([0, 0, 0, 0x38, 0x34]), r.choice([0, 0, 0, 0x43])],
        "RecvData": [r.choice([0, 0, 1, 2]), r.choice([0, 3])],
        "ParseFileSize": [r.choice([-1, 0, 1, 1000, 123456])],
        "socket": [r.choice([-1, 3, 12, 0])],
        "inet_addr": [r.choice(U32) for _ in range(3)],
        "bind": [r.choice([0, -1])],
        "connect": [r.choice([0, 0, -1, 1])],
        "gethostbyname": [r.choice([0, 1])],
        "shutdown": [0], "closesocket": [0],
        "recv": r.choice([[0], [5, 0], [3, -1], [1, 2, 0], [-1]]),
    }
    return {
        "fn": fn, "queues": q,
        "sock": r.choice([-1, 5, 5, 5, 5, 0]),
        "sa": (r.choice([0, 2, 7]), r.randrange(1 << 16), r.choice(U32), bytes(r.randrange(256) for _ in range(8))),
        "total": r.choice([0, 5, -3, 1000]) , "done": r.choice([0, 9, -1]),
        "dsock": r.choice([-1, -1, 7, 0]), "lsock": r.choice([-1, -1, -1, 4, 0]),
        "port": r.randrange(1 << 16), "ipw": r.choice(U32),
        "host": r.choice(HOSTS), "user": r.choice(["anon", "bob"]), "pw": r.choice(["x@y", "secret"]),
        "path": r.choice(["dir/file.zip", "a", "park.tpw"]),
        "slen": r.choice([0, 0, 5, 11]), "stext": r.choice(["192.168.0.7", "10.0.0.1"]),
        "hostip": r.choice(U32),
    }


# ---- _Download
DL_POOL = [0, 0x7D, 0x96, 0xFA, 0xE2, 0x44]


def dl_random(r):
    seq = [r.choice(DL_POOL) if r.random() < 0.5 else r.choice([0x7D, 0x96, 0xFA, 0xE2]) for _ in range(4)]
    return base(r, FN_DOWNLOAD, seq)


def dl_edges():
    r = random.Random(2271)
    # the happy paths first, with and without the second passive round
    for seq in ([0x7D, 0xFA, 0x96, 0xFA], [0x96, 0xE2, 0x7D, 0xE2], [5, 0x96, 0xFA], [5, 0x7D, 0xE2], [0x7D, 0xE2, 0x96, 0xFA]):
        for _ in range(4):
            c = base(r, FN_DOWNLOAD, seq)
            c["sock"] = 5
            c["queues"]["EnterPassiveMode"] = [0, 0]
            c["queues"]["RecvData"] = [0, 0]
            yield c
    for a in (0, 0x7D, 5):
        for b in (0, 0xFA, 0xE2, 5):
            for c3 in (0, 0x96, 0x7D, 5):
                for d in (0, 0xE2, 0xFA, 5):
                    yield base(r, FN_DOWNLOAD, [a, b, c3, d])
    for _ in range(40):
        yield dl_random(r)


# ---- _Connect
def cn_random(r):
    seq = [r.choice([0xDC, 0xDC, 0, 5]), r.choice([0x14B, 0x14C, 0xC8, 0, 5, 0xDC]), r.choice([0xE6, 0xC8, 0, 5]), r.choice([0xC8, 0, 5])]
    return base(r, FN_CONNECT, seq)


def cn_edges():
    r = random.Random(2272)
    for a in (0, 0xDC, 5):
        for b in (0, 0x14B, 0x14C, 0xC8, 5):
            for c3 in (0, 0xE6, 0xC8, 5):
                for d in (0, 0xC8, 5):
                    yield base(r, FN_CONNECT, [a, b, c3, d])
    for sock in (-1, 0, 5):
        for host_ in HOSTS:
            c = base(r, FN_CONNECT, [0xDC, 0x14B, 0xE6, 0xC8])
            c["sock"], c["host"] = sock, host_
            c["queues"]["socket"] = [sock]
            yield c


TESTS = [
    FnTest("FTPClient::_Connect", ppc, host, edge=cn_edges, rnd=cn_random),
    FnTest("FTPClient::_Download", ppc, host, edge=dl_edges, rnd=dl_random),
]
