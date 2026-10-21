"""Unit v5575, the menu callbacks, against libtpw_unit_UIMenu_DefaultItemCallback.dylib, main app,
src/InterfaceWindow_virtuals_UIMenu_DefaultItemCallback.cpp:
  UIMenu_DefaultItemCallback 0x1018aa48, UIMenu_DefaultCallback 0x1018ab78

Callees (getExtraParam, ReleaseString, UIMain_UISound, GetID, GetParent, UI_SendMessage, CallDefaultWindowHandler) are fakes on
both sides recording their arguments; answers come from the case's queues. Extra parameter 0 answers a menu block that the case
fills with nonzero words. After the call the menu is compared by field: the four words, the window (UIMenu::mWindow, 4 bytes at
0x10 on the Mac, a full pointer on the host whose initial value has a nonzero high half, so a 32-bit store shows) as "kept" or its
new value, and whether the bytes past the menu are unchanged. Not tested: DefaultCallback with message 0x14 and a NULL extra.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIMenu_DefaultItemCallback"
MASK = 0xFFFFFFFF

FN_ITEM, FN_DEF = 0x1018AA48, 0x1018AB78
SIZES = {FN_ITEM: 232, FN_DEF: 136}
OWN = set(SIZES)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def st_extra(e):
    L = e.cl
    idx = T.s16(e.r(4))
    L.rec("getExtraParam", (idx,))
    if idx == 0:
        e.set_r(3, L.extra & MASK)
    else:
        e.set_r(3, L.ret("extra1") & MASK)


def st_release(e):
    e.cl.rec("ReleaseString", (e.r(3),))


def st_sound(e):
    e.cl.rec("UISound", (s32(e.r(3)),))


def st_getid(e):
    e.cl.rec("GetID", ())
    e.set_r(3, e.cl.ret("id") & MASK)


def st_parent(e):
    e.cl.rec("GetParent", ())
    e.set_r(3, e.cl.ret("parent") & MASK)


def st_send(e):
    e.cl.rec("UI_SendMessage", (e.r(3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("send") & MASK)


def st_default(e):
    e.cl.rec("CallDefault", (s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("default") & MASK)


STUBS.internal(0x101DD030, st_extra)
STUBS.internal(0x101D7B10, st_release)
STUBS.internal(0x101802CC, st_sound)
STUBS.internal(0x101DBE08, st_getid)
STUBS.internal(0x101DBAFC, st_parent)
STUBS.internal(0x101C9518, st_send)
STUBS.internal(0x101DE318, st_default)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, d):
        self.e.write(a, d)

    def log(self):
        return self.L.log

    def set_extra(self, a):
        self.L.extra = a

    def layout(self):
        return 0x10, 4, 0x14

    def window(self):
        return 0x55555555

    def call(self, fn, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, fn, [w & MASK, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host:
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_uimenu_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, d):
        C.memmove(a, d, len(d))

    def log(self):
        return CL.host_end(self.lib)

    def set_extra(self, a):
        self.lib.tpw_uimenu_set_extra(a)

    def layout(self):
        out = (C.c_int64 * 3)()
        self.lib.tpw_uimenu_layout(out)
        return tuple(out)

    def window(self):
        return 0x0000007855555555

    def call(self, fn, w, m, a, b):
        f = self.lib.tpw_uimenu_item if fn == FN_ITEM else self.lib.tpw_uimenu_def
        return f(w, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_uimenu_alloc.restype, lib.tpw_uimenu_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_uimenu_set_extra.restype, lib.tpw_uimenu_set_extra.argtypes = None, [C.c_uint64]
    lib.tpw_uimenu_layout.restype, lib.tpw_uimenu_layout.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    for n in ("item", "def"):
        f = getattr(lib, "tpw_uimenu_" + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.bound = True


def run(s, case):
    try:
        blk = s.alloc(64)
        s.write(blk, b"".join(struct.pack(s.fmt, 0x11111111 * (i % 15 + 1)) for i in range(16)))
        wo, wn, size = s.layout()
        order = "big" if s.fmt[0] == ">" else "little"
        s.write(blk + wo, s.window().to_bytes(wn, order))
        before = s.read(blk, 64)
        s.set_extra(blk)
        w = s.alloc(16)
        ans = s.call(case["fn"], w, *case["args"])
        after = s.read(blk, 64)
        win = after[wo:wo + wn]
        return {"answer": ans, "log": s.log(), "fields": list(struct.unpack(s.fmt[0] + "4I", after[:16])),
                "window": "kept" if win == before[wo:wo + wn] else int.from_bytes(win, order),
                "tail unchanged": after[size:] == before[size:]}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MSGS = [0x14, 0x10006, 0x10005, 0x10007, 0x13, 0x15, 0, -1, 0x100001, 0x7FFFFFFF, -0x7FFFFFFF - 1, 0x10006 + 0x10000]


def mk(r, fn, m=None):
    q = {k: [r.choice([0, 1, -1, r.randrange(0, 0x1000)])] for k in ("extra1", "id", "parent", "send", "default")}
    q["extra1"] = [r.randrange(0, 0x1000)]
    args = [r.choice(MSGS) if m is None else m, r.randrange(-100, 100), r.choice([0, 7, -3])]
    return {"fn": fn, "queues": q, "args": args}


def edges_item():
    r = random.Random(11)
    for m in MSGS:
        yield mk(r, FN_ITEM, m)


def edges_def():
    r = random.Random(12)
    for m in MSGS:
        yield mk(r, FN_DEF, m)


TESTS = [
    FnTest("UIMenu_DefaultItemCallback", ppc, host, edge=edges_item, rnd=lambda r: mk(r, FN_ITEM)),
    FnTest("UIMenu_DefaultCallback", ppc, host, edge=edges_def, rnd=lambda r: mk(r, FN_DEF)),
]
