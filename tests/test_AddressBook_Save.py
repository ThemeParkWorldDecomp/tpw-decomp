"""Unit v5009, AddressBook::Save, against libtpw_unit_AddressBook_Save.dylib (tests/unit_AddressBook_Save.cpp), main app
("SimTheme Park"), src/game/online/email.cpp:
  AddressBook::Save 0x102067cc

A case is a book (a list of names) and a path. The PowerPC side builds the MSL list and string objects in emulator memory; the host
side builds a std::list. Callees outside the unit are fakes on both sides: the ofstream constructor (it sets the stream's ios block:
state, buffer word and exception mask from the case), filebuf::open (records the name; sets the filebuf's open word), operator<<,
filebuf::close, the three stream destructors and __throw (which returns on both sides). String and C library calls are the
real semantics on both sides (not logged).

Compared: the answer, the event log and the ios block after the call. No fused operations. Not tested: names of 128 bytes or more (the
original's sprintf buffer overflows).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AddressBook_Save"
MASK = 0xFFFFFFFF

FN = 0x102067CC
SIZES = {FN: 820}
OWN = set(SIZES)

A_CTOR, A_STR_CTOR, A_ASSIGN, A_APPEND, A_OPEN = 0x10071E34, 0x1007363C, 0x101B2440, 0x1020100C, 0x10071C44
A_REFPTR, A_OUT, A_CLOSE = 0x100026AC, 0x10207150, 0x10071B54
A_FBDT, A_OSDT, A_IOSDT, A_STRDT, A_THROW = 0x100700E4, 0x10070000, 0x1006FF50, 0x1000221C, 0x10238680

HOST_LAYOUT = [0x24, 0x34, 0x38, 0x60]
CUR = {}
STATE = {}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def rec(L, name, texts=()):
    L.rec(name, (0,), tuple(texts))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_ctor(e):
    rec(e.cl, "ofstream")
    ios = e.alloc(0x60)
    w32(e, ios + 0x34, CUR["state"])
    w32(e, ios + 0x24, CUR["rdbuf"])
    w32(e, ios + 0x38, CUR["mask"])
    w32(e, e.r(3), ios)
    STATE["ios"] = ios


def st_str_ctor(e):
    rep = e.alloc(16)
    buf = e.alloc(1024)
    w32(e, rep + 0xC, buf)
    w32(e, e.r(3), rep)


def st_assign(e):
    buf = e.u32(e.u32(e.r(3)) + 0xC)
    n = e.r(5)
    e.write(buf, e.read(e.r(4), n) + b"\0")


def st_append(e):
    buf = e.u32(e.u32(e.r(3)) + 0xC)
    cur = len(e.cstr(buf).encode() if isinstance(e.cstr(buf), str) else e.cstr(buf))
    e.write(buf + cur, e.read(e.r(4), e.r(5)) + b"\0")


def st_open(e):
    rec(e.cl, "open", ())
    # the name is a text event: recorded below with the mode as the int
    name = e.cstr(e.r(4))
    e.cl.log.pop()
    e.cl.rec("open", (e.r(5),), (name if isinstance(name, str) else name.decode(),))
    w32(e, e.r(3) + 0x24, CUR["flag"])
    e.set_r(3, CUR["open_ret"])


def st_refptr(e):
    w32(e, e.r(3), e.r(4))


def st_out(e):
    t = e.cstr(e.r(4))
    e.cl.rec("out", (0,), (t if isinstance(t, str) else t.decode(),))


def st_close(e):
    rec(e.cl, "close")
    e.set_r(3, CUR["close_ret"])


def st_throw(e):
    obj = e.r(4)
    t = e.cstr(e.u32(obj + 4))
    e.cl.rec("throw", (0,), (t if isinstance(t, str) else t.decode(),))


def mk_dt(name):
    def f(e):
        rec(e.cl, name)
    return f


def nop(e):
    pass


STUBS.internal(A_CTOR, st_ctor)
STUBS.internal(A_STR_CTOR, st_str_ctor)
STUBS.internal(A_ASSIGN, st_assign)
STUBS.internal(A_APPEND, st_append)
STUBS.internal(A_OPEN, st_open)
STUBS.internal(A_REFPTR, st_refptr)
STUBS.internal(A_OUT, st_out)
STUBS.internal(A_CLOSE, st_close)
STUBS.internal(A_FBDT, mk_dt("filebuf_dtor"))
STUBS.internal(A_OSDT, mk_dt("ostream_dtor"))
STUBS.internal(A_IOSDT, mk_dt("ios_dtor"))
STUBS.internal(A_STRDT, nop)
STUBS.imp("__throw", st_throw)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_save_alloc.restype, lib.tpw_save_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_save_ios.restype = C.c_uint64
    lib.tpw_save_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_save_run.restype = C.c_int32
    lib.tpw_save_run.argtypes = [C.c_char_p, C.c_char_p, C.c_int32] + [C.c_uint32] * 6
    out = (C.c_int64 * 32)()
    n = lib.tpw_save_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build_book(e, names):
    a = e.alloc(16)
    prev = a + 4
    first = a + 4
    nodes = []
    for nm in names:
        node = e.alloc(12)
        rep = e.alloc(16)
        buf = e.alloc(256)
        e.write(buf, nm.encode() + b"\0")
        w32(e, rep + 0xC, buf)
        w32(e, node + 8, rep)
        nodes.append(node)
    seq = [a + 4] + nodes + [a + 4]
    for i, node in enumerate(nodes):
        w32(e, node, seq[i])
        w32(e, node + 4, seq[i + 2])
    w32(e, a + 4, nodes[-1] if nodes else a + 4)
    w32(e, a + 8, nodes[0] if nodes else a + 4)
    return a


def ppc(e, case):
    CUR.clear()
    CUR.update(case)
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        book = build_book(e, case["names"])
        path = e.alloc(len(case["path"]) + 1, case["path"].encode() + b"\0")
        with_r3 = T.call_ppc(e, FN, [book, path])[0]
        ios = STATE["ios"]
        return {"answer": with_r3 & 0xFF if False else with_r3 & MASK, "log": e.cl.log,
                "ios": [e.u32(ios + 0x24), e.u32(ios + 0x34), e.u32(ios + 0x38)]}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    blob = b"".join(n.encode() + b"\n" for n in case["names"])
    r = lib.tpw_save_run(case["path"].encode(), blob, len(case["names"]), case["state"], case["rdbuf"], case["mask"],
                         case["open_ret"], case["flag"], case["close_ret"])
    ios = lib.tpw_save_ios()
    return {"answer": r & MASK, "log": CL.host_end(lib), "ios": [struct.unpack("<I", C.string_at(ios + o, 4))[0] for o in (0x24, 0x34, 0x38)]}


ALPHA = "abcxyz@._-0189"


def rname(r):
    return "".join(r.choice(ALPHA) for _ in range(r.choice([0, 1, 5, 20, 90])))


def mk(r, **kw):
    c = {
        "names": [rname(r) for _ in range(r.choice([0, 1, 2, 3, 6]))],
        "path": rname(r) or "p",
        "state": r.choice([0, 0, 1, 2, 4, 5, 8]),
        "rdbuf": r.choice([0, 1, 0x1234]),
        "mask": r.choice([0, 0, 1, 4, 5, 2]),
        "open_ret": r.choice([0, 1, 1]),
        "flag": r.choice([0, 1, 1, 7]),
        "close_ret": r.choice([0, 1, 1]),
    }
    c.update(kw)
    return c


def edges():
    r = random.Random(5009)
    for names in ([], ["a@b.c"], ["", "x"], ["one", "two", "three"]):
        for open_ret in (0, 1):
            for flag in (0, 1):
                for close_ret in (0, 1):
                    for mask in (0, 4, 1, 5):
                        for rdbuf in (0, 1):
                            yield mk(r, names=names, open_ret=open_ret, flag=flag, close_ret=close_ret, mask=mask, rdbuf=rdbuf,
                                     state=0)
    yield mk(r, state=4, mask=4, open_ret=1, flag=1, close_ret=0)


def rnd(r):
    return mk(r)


TESTS = [FnTest("AddressBook::Save", ppc, host, edge=edges, rnd=rnd)]
