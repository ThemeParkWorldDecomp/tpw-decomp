"""Unit v5049, CFSOnlineServer's constructor, against libtpw_unit_CFSOnlineServer_CFSOnlineServer.dylib
(tests/unit_CFSOnlineServer_CFSOnlineServer.cpp), main app ("SimTheme Park"), src/game/online/FS_OnlineList.cpp:
  CFSOnlineServer::CFSOnlineServer 0x1017c7d0

A case is a server list and one to three constructions on it. The world is built twice by ONE piece of Python code: in emulator memory at
the Mac's addresses (big-endian) and in the host library's memory (little-endian). The string members' constructor (glue 0x10238740) and
their virtual operator=(const wchar_t*) (vtable slot 0x20, reached through __ptr_glue, which runs as the original) are fakes on both sides
that record the member (0..4) and the first four characters of the string assigned. The list's AddTail (0x1017dbe8) runs as the original.

The world of a case: per construction a ccServerInfo (0x26 bytes, packed, five string pointers to distinct 4-character strings, random
words and halfword) and the list. What is compared: the event log, the scalar members of every object (vtable word, list, 08, 0c, 38, 3c,
68) and the list (count, head, tail, and per node its object and prev link).

No fused operations. Not tested: a NULL string pointer (the fake reads the characters).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineServer_CFSOnlineServer"
MASK = 0xFFFFFFFF

FN = 0x1017C7D0
ADDTAIL = 0x1017DBE8
STR_CTOR = 0x10238740
ASSIGN_CODE = 0x1017C914            # where the fake vtable's slot 0x20 leads (never executed code, hooked)
VPTR = 0x102A73D8
SIZES = {FN: 324}
OWN = set(SIZES)
OWN.add(ADDTAIL)

HOST_LAYOUT = [0x26, 0x22, 0x16, 0x14]
MEMBERS = {0x10: 0, 0x24: 1, 0x40: 2, 0x54: 3, 0x6C: 4}
CUR = {"base": 0, "vt": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_str_ctor(e):
    """TbDynamicStringTemplate<wchar_t>::TbDynamicStringTemplate(): the real one sets the vtable word; the fake does only that."""
    this = e.r(3)
    e.cl.rec("StrCtor", (MEMBERS.get(this - CUR["base"], -1),))
    e.write(this, struct.pack(">I", CUR["vt"]))


def stub_assign(e):
    """TbDynamicStringTemplate<wchar_t>& operator=(const wchar_t*)."""
    this, p = e.r(3), e.r(4)
    chars = struct.unpack(">4H", e.read(p, 8))
    e.cl.rec("StrAssign", (MEMBERS.get(this - CUR["base"], -1),) + chars)


STUBS.imp("__ct__26TbDynamicStringTemplate<w>Fv", stub_str_ctor)
STUBS.internal(ASSIGN_CODE, stub_assign)


def setup(runner):
    T.install_std_stubs(runner.emu)
    tv = T.persist_alloc(runner.emu, 8, struct.pack(">II", ASSIGN_CODE, 0))
    vt = bytearray(0x40)
    struct.pack_into(">I", vt, 0x20, tv)
    CUR["vt"] = T.persist_alloc(runner.emu, 0x40, bytes(vt))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def make_list(self):
        outer = self.alloc(0x34)
        inner = self.alloc(0x10)
        self.words(outer + 0x2C, [inner])
        return outer

    def ctor(self, info, lst):
        obj = self.alloc(0x80)
        CUR["base"] = obj
        T.call_ppc(self.e, FN, [obj, info, lst])
        return obj

    def srv(self, obj, lst):
        w = self.rd_words(obj, 0x1B)
        h = struct.unpack(">H", self.read(obj + 0x3C, 2))[0]
        return [1 if w[0] == VPTR else 0, 1 if w[1] == lst else 0, w[2], w[3], w[14], h, w[26]]

    def lst(self, lst, objs):
        def kind(p):
            if p == 0:
                return 0
            return objs.index(p) + 1 if p in objs else 9
        inner = self.rd_words(lst + 0x2C, 1)[0]
        head, tail, count = self.rd_words(inner + 4, 3)
        out = [count, kind(self.rd_words(head, 1)[0]) if head else 0, kind(self.rd_words(tail, 1)[0]) if tail else 0]
        prev, node = 0, head
        for _ in range(5):
            if node == 0:
                out += [0, 0]
                continue
            data, nxt, pv = self.rd_words(node, 3)
            out += [kind(data), 1 if pv == prev else 0]
            prev, node = node, nxt
        return out


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_osv_alloc(n)

    def ptr(self, a):
        return self.lib.tpw_osv_off(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make_list(self):
        return self.lib.tpw_osv_make_list()

    def ctor(self, info, lst):
        return self.lib.tpw_osv_ctor(info, lst)

    def srv(self, obj, lst):
        out = (C.c_int64 * 16)()
        n = self.lib.tpw_osv_srv(obj, lst, out)
        return list(out)[:n]

    def lst(self, lst, objs):
        arr = (C.c_uint64 * 4)(*objs)
        out = (C.c_int64 * 32)()
        n = self.lib.tpw_osv_list(lst, arr, len(objs), out)
        return list(out)[:n]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_osv_alloc.restype, lib.tpw_osv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_osv_off.restype, lib.tpw_osv_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_osv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_osv_make_list.restype, lib.tpw_osv_make_list.argtypes = C.c_uint64, []
    lib.tpw_osv_ctor.restype, lib.tpw_osv_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_osv_srv.restype, lib.tpw_osv_srv.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.POINTER(C.c_int64)]
    lib.tpw_osv_list.restype = C.c_int32
    lib.tpw_osv_list.argtypes = [C.c_uint64, C.POINTER(C.c_uint64), C.c_int32, C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_osv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def make_info(s, info):
    blk = s.alloc(0x30)
    ptrs = []
    for chars in info["s"]:
        a = s.alloc(16)
        s.write(a, struct.pack(s.fmt[0] + "5H", *chars, 0))
        ptrs.append(s.ptr(a))
    f00, f04, f10, f1e = info["w"]
    s.words(blk + 0x00, [f00, f04, ptrs[0], ptrs[1], f10])
    s.half(blk + 0x14, info["h"])
    s.words(blk + 0x16, [ptrs[2]])
    s.words(blk + 0x1A, [ptrs[3]])
    s.words(blk + 0x1E, [f1e])
    s.words(blk + 0x22, [ptrs[4]])
    return blk


def run(s, case):
    try:
        lst = s.make_list()
        objs = []
        for info in case["infos"]:
            objs.append(s.ctor(make_info(s, info), lst))
        return {"servers": [s.srv(o, lst) for o in objs], "list": s.lst(lst, objs), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rinfo(r, special=None):
    if special is not None:
        w = [special] * 4
        h = special & 0xFFFF
    else:
        w = [r.choice([0, MASK, 0x80000000, r.getrandbits(32)]) for _ in range(4)]
        h = r.choice([0, 0xFFFF, 0x8000, r.getrandbits(16)])
    return {"w": w, "h": h, "s": [tuple(r.randrange(1, 0xFFFF) for _ in range(4)) for _ in range(5)]}


def case_n(r, n, special=None):
    return {"queues": {}, "infos": [rinfo(r, special if i == 0 else None) for i in range(n)]}


def edges():
    r = random.Random(4049)
    for n in (1, 2, 3):
        yield case_n(r, n)
    for sp in (0, MASK, 0x80000000, 0x7FFF, 0x8000):
        yield case_n(r, 2, sp)


def rnd(r):
    return case_n(r, r.choice([1, 2, 3]))


TESTS = [
    FnTest("CFSOnlineServer::CFSOnlineServer", ppc, host, edge=edges, rnd=rnd),
]
