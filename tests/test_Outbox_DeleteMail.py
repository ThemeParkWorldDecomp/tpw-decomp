"""Unit v5690, Outbox::DeleteMail 0x1020638c, against libtpw_unit_Outbox_DeleteMail.dylib (tests/unit_Outbox_DeleteMail.cpp), main app,
src/email_load_Outbox_Send_Outbox_WriteMailData_Outbox_DeleteMail.cpp.

A case is an Outbox whose path string mF00 is a short text, and a mail id. The MSL string functions (ctor, assign, append, dtor) are
fakes on the PowerPC side that keep a rep block (text at +0xc); unlink is a fake on both sides that records the path (length and
big-endian words). sprintf and strlen are the harness's C library stubs. Compared: the unlink events (both paths in order) and the
text of mF00 afterwards. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Outbox_DeleteMail"
MASK = 0xFFFFFFFF
FN = 0x1020638C
CTOR, ASSIGN, APPEND, DTOR = 0x1007363C, 0x10001C80, 0x1020100C, 0x1000221C

STUBS = CL.Stubs()
STUBS.real.add(FN)


def new_rep(e, text):
    buf = e.alloc(len(text) + 1, text + b"\0")
    return e.alloc(0x10, bytes(0xC) + buf.to_bytes(4, "big"))


def text_of(e, obj):
    t = e.cstr(e.u32(e.u32(obj) + 0xC))
    return t if isinstance(t, bytes) else t.encode("latin-1")


def s_ctor(e):
    obj = e.r(3)
    e.write(obj, (new_rep(e, b"")).to_bytes(4, "big"))


def s_assign(e):
    obj, src, pos, n = e.r(3), e.r(4), e.r(5), e.r(6)
    t = text_of(e, src)
    assert pos == 0 and n == MASK
    e.write(obj, new_rep(e, t).to_bytes(4, "big"))


def s_append(e):
    obj, ptr, n = e.r(3), e.r(4), e.r(5)
    t = text_of(e, obj) + e.read(ptr, n)
    e.write(obj, new_rep(e, t).to_bytes(4, "big"))


def s_dtor(e):
    pass


def s_unlink(e):
    p = e.cstr(e.r(3))
    p = p if isinstance(p, bytes) else p.encode("latin-1")
    ws = [len(p)]
    for i in range(0, len(p), 4):
        c = p[i:i + 4].ljust(4, b"\0")
        ws.append(int.from_bytes(c, "big"))
    ws += [0] * (8 - len(ws))
    e.cl.rec("unlink", tuple(ws))
    e.set_r(3, 0)


STUBS.internal(CTOR, s_ctor)
STUBS.internal(ASSIGN, s_assign)
STUBS.internal(APPEND, s_append)
STUBS.internal(DTOR, s_dtor)
STUBS.imp("unlink", s_unlink)


def s_sprintf(e):
    fmt = e.cstr(e.r(4))
    fmt = fmt if isinstance(fmt, bytes) else fmt.encode("latin-1")
    assert fmt in (b":%08x.dat", b":%08x.jpg"), fmt
    out = (fmt.replace(b"%08x", b"%08x" % e.r(5))) + b"\0"
    e.write(e.r(3), out)
    e.set_r(3, len(out) - 1)


def s_strlen(e):
    p = e.cstr(e.r(3))
    e.set_r(3, len(p))


STUBS.internal(0x10238578, s_sprintf)
STUBS.internal(0x10238650, s_strlen)
STUBS.internal(0x102394D8, s_unlink)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_od_run.argtypes = [C.c_char_p, C.c_int32, C.c_char_p]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        obj = e.alloc(0x60)
        e.write(obj, new_rep(e, case["path"]).to_bytes(4, "big"))
        T.call_ppc(e, FN, [obj, case["id"] & MASK])
        return {"log": e.cl.log, "path": text_of(e, obj)}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, {})
    out = C.create_string_buffer(0x80)
    lib.tpw_od_run(case["path"], T.s32(case["id"]), out)
    return {"log": CL.host_end(lib), "path": out.value}


def mk(path, i):
    return {"path": path, "id": i & MASK}


def edges():
    for p in (b"", b"a", b"disk:Outbox", b"x" * 15):
        for i in (0, 1, -1, 0x7FFFFFFF, 0x80000000, 0x1234ABCD):
            yield mk(p, i)


def rnd(r):
    p = bytes(r.choice(b"abcxyz:_09") for _ in range(r.randrange(0, 16)))
    return mk(p, r.getrandbits(32))


TESTS = [FnTest("Outbox::DeleteMail", ppc, host, edge=edges, rnd=rnd)]
