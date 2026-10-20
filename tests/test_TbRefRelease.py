"""Differential tests for bullfrog TbRefRelease.cpp. See difftest.py and tpwtest.py.

Objects are 8 bytes on the PowerPC (vptr, count). The PowerPC side uses the real vtable
(0x10072690), or for "derived" cases a copy whose destructor slot is a Python fake that
logs (this, flag), as a derived class's destructor would run. The host uses TbRefRelease
or a derived TestRef that counts its destructor calls; operator delete is observed through
tests/shim_alloc.cpp's log. The destructor's hidden flag is a full random word on the
PowerPC (only its low half counts) and an int16_t on the host; storage is freed when the
flag is positive (tpwtest's destructor convention).
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

VTABLE = 0x10072690
VP, I32 = ctypes.c_void_p, ctypes.c_int32


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.heap_in_scratch = True
    e.ref_dtor_log = []

    def fake_dtor(e):
        e.ref_dtor_log.append((e.r(3), T.s16(e.r(4))))
    tv = T.fake_function(e, fake_dtor)
    vt = T.persist_alloc(e, 0x14)       # RTTI, offset, Release, AddRef, then the fake destructor
    e.write(vt, e.read(VTABLE, 0x10) + struct.pack(">I", tv))
    e.ref_fake_vtable = vt


def host_deletes(lib, obj):
    n = I32.in_dll(lib, "tpw_alloc_n").value
    kinds = (I32 * 1024).in_dll(lib, "tpw_alloc_kind")
    ptrs = (ctypes.c_uint64 * 1024).in_dll(lib, "tpw_alloc_ptr")
    return [k for i, k in enumerate(kinds[:n]) if ptrs[i] == obj and k == 3]


def alloc_log(lib, on):
    I32.in_dll(lib, "tpw_alloc_log_on").value = on
    if on:
        I32.in_dll(lib, "tpw_alloc_n").value = 0

# ----------------------------------------------------------------- tests


def ctor_ppc(e, c):
    obj = e.alloc(8, struct.pack(">2I", *c["garbage"]))
    r3, _, _ = T.call_ppc(e, e.func("__ct__12TbRefReleaseFv"), [T.I(obj)])
    return dict(count=T.s32(e.u32(obj + 4)), vt_ok=e.u32(obj) == VTABLE, this_ret=r3 == obj)


def ctor_host(lib, c):
    block = ctypes.create_string_buffer(bytes(16))
    count = I32()
    ok = F(lib, "tpw_TbRefRelease_ctor", I32, VP, ctypes.POINTER(I32))(block, ctypes.byref(count))
    return dict(count=count.value, vt_ok=bool(ok), this_ret=True)


def dtor_ppc(e, c):
    obj = e.alloc(8, struct.pack(">Ii", c["garbage"][0], c["count"]))
    e.deletes = []
    r3, _, _ = T.call_ppc(e, e.func("__dt__12TbRefReleaseFv"), [T.I(obj), T.I(c["flag"])])
    return dict(deleted=e.deletes == [obj], count=T.s32(e.u32(obj + 4)), vt_ok=e.u32(obj) == VTABLE,
                this_ret=r3 == obj)


def dtor_host(lib, c):
    obj = F(lib, "tpw_TbRefRelease_new", VP, I32, I32)(0, c["count"])
    count = F(lib, "tpw_TbRefRelease_count", I32, VP)(obj)
    flag = T.s16(c["flag"])
    alloc_log(lib, 1)
    F(lib, "tpw_TbRefRelease_dtor", None, VP, ctypes.c_int16)(obj, flag)
    deleted = host_deletes(lib, obj) == [3]
    alloc_log(lib, 0)
    if flag <= 0:
        F(lib, "tpw_TbRefRelease_free", None, VP)(obj)
    return dict(deleted=deleted, count=count, vt_ok=True, this_ret=True)


def addref_ppc(e, c):
    obj = e.alloc(8, struct.pack(">Ii", VTABLE, c["count"]))
    r3, _, _ = T.call_ppc(e, e.func("AddRef__12TbRefReleaseFv"), [T.I(obj)])
    return dict(ret=T.s32(r3), count=T.s32(e.u32(obj + 4)))


def addref_host(lib, c):
    obj = F(lib, "tpw_TbRefRelease_new", VP, I32, I32)(0, c["count"])
    r = F(lib, "tpw_TbRefRelease_AddRef", I32, VP)(obj)
    count = F(lib, "tpw_TbRefRelease_count", I32, VP)(obj)
    F(lib, "tpw_TbRefRelease_free", None, VP)(obj)
    return dict(ret=r, count=count)


def release_ppc(e, c):
    vt = e.ref_fake_vtable if c["derived"] else VTABLE
    obj = e.alloc(8, struct.pack(">Ii", vt, c["count"]))
    e.deletes, e.ref_dtor_log = [], []
    r3, _, _ = T.call_ppc(e, e.func("Release__12TbRefReleaseFv"), [T.I(obj)])
    destroyed = e.deletes == [obj] or e.ref_dtor_log == [(obj, 1)]
    return dict(ret=T.s32(r3), destroyed=destroyed, derived_dtor=len(e.ref_dtor_log),
                count=None if destroyed else T.s32(e.u32(obj + 4)))


def release_host(lib, c):
    obj = F(lib, "tpw_TbRefRelease_new", VP, I32, I32)(1 if c["derived"] else 0, c["count"])
    I32.in_dll(lib, "tpw_test_ref_dtors").value = 0
    alloc_log(lib, 1)
    r = F(lib, "tpw_TbRefRelease_Release", I32, VP)(obj)
    destroyed = host_deletes(lib, obj) == [3]
    alloc_log(lib, 0)
    derived = I32.in_dll(lib, "tpw_test_ref_dtors").value
    count = None
    if not destroyed:
        count = F(lib, "tpw_TbRefRelease_count", I32, VP)(obj)
        F(lib, "tpw_TbRefRelease_free", None, VP)(obj)
    return dict(ret=r, destroyed=destroyed, derived_dtor=derived, count=count)


COUNTS = [0, 1, 2, -1, 0x7FFFFFFF, -0x80000000, 100]


def rcount(r):
    return r.choice(COUNTS) if r.random() < 0.5 else T.s32(r.getrandbits(32)) if r.random() < 0.5 else r.randint(-3, 5)


TESTS = [
    FnTest("TbRefRelease::TbRefRelease", ctor_ppc, ctor_host,
           edge=lambda: [dict(garbage=[0, 0]), dict(garbage=[0xFFFFFFFF] * 2)],
           rnd=lambda r: dict(garbage=T.rwords(r, 2))),
    FnTest("TbRefRelease::~TbRefRelease", dtor_ppc, dtor_host,
           edge=lambda: [dict(garbage=[0x12345678], count=n, flag=f) for n in (0, 1)
                         for f in (0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0xFFFF0001)],
           rnd=lambda r: dict(garbage=[r.getrandbits(32)], count=rcount(r), flag=r.getrandbits(32))),
    FnTest("TbRefRelease::AddRef", addref_ppc, addref_host,
           edge=lambda: [dict(count=n) for n in COUNTS], rnd=lambda r: dict(count=rcount(r))),
    FnTest("TbRefRelease::Release", release_ppc, release_host,
           edge=lambda: [dict(count=n, derived=d) for n in COUNTS for d in (0, 1)],
           rnd=lambda r: dict(count=rcount(r), derived=r.choice([0, 1]))),
]
