"""Unit v5249, FindStaticTag and AddTag, against libtpw_unit_FindStaticTag.dylib (tests/unit_FindStaticTag.cpp), main app,
src/game/ui/ui_tags.cpp:
  FindStaticTag 0x10191cf4, AddTag 0x101921f4

A case: the ten static tags' windows are written (gStaticTags, stride 0x208 on the Mac, an array of structs on the host) and the function
called. AddATag and ShowTagPanel are fakes on both sides recording their events (AddATag the show byte of the parameters; ShowTagPanel the
window as a label); AddATag answers from the case's queue. Compared: the answer and the log. No fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FindStaticTag"
MASK = 0xFFFFFFFF

FN_FIND, FN_ADD = 0x10191CF4, 0x101921F4
SIZES = {FN_FIND: 80, FN_ADD: 96}
OWN = set(SIZES)
ADDATAG, SHOWTAGPANEL = 0x10191EF8, 0x10190D0C
TAGS = 0x1038CD84

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_addatag(e):
    L = e.cl
    L.rec("AddATag", (e.read(e.r(3) + 0x24, 1)[0],))
    e.set_r(3, L.ret("AddATag") & MASK)


def stub_show(e):
    L = e.cl
    L.rec("ShowTagPanel", (L.lab(e.r(3)),))


STUBS.internal(ADDATAG, stub_addatag)
STUBS.internal(SHOWTAGPANEL, stub_show)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ft_set.restype, lib.tpw_ft_set.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_ft_find.restype, lib.tpw_ft_find.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_ft_add.restype, lib.tpw_ft_add.argtypes = C.c_uint32, [C.c_uint32]
    lib.bound = True


def ppc(e, case):
    e.cl = CL.PpcLog({}, case["queues"])
    saved = STUBS.activate(e)
    try:
        if case["op"] == "find":
            for i, w in enumerate(case["tags"]):
                e.write(TAGS + i * 0x208, struct.pack(">I", w))
            r3, _, _ = T.call_ppc(e, FN_FIND, [case["arg"]])
            ans = T.s32(r3)
        else:
            p = e.alloc(0x28)
            e.write(p + 0x24, bytes([case["show"]]))
            r3, _, _ = T.call_ppc(e, FN_ADD, [p])
            ans = r3 & 0xFF
        return {"answer": ans, "log": e.cl.log}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, case["queues"])
    if case["op"] == "find":
        for i, w in enumerate(case["tags"]):
            lib.tpw_ft_set(i, w)
        ans = lib.tpw_ft_find(case["arg"])
    else:
        ans = lib.tpw_ft_add(case["show"]) & 0xFF
    return {"answer": ans, "log": CL.host_end(lib)}


def find_case(r, tags=None, arg=None):
    t = tags if tags is not None else [r.choice([0, 0x100, 0x200, 0x300]) for _ in range(10)]
    a = arg if arg is not None else r.choice([0, 0x100, 0x200, 0x300, 0x400])
    return {"op": "find", "queues": {}, "tags": t, "arg": a}


def find_edges():
    r = random.Random(11)
    base = [0x100 + 0x10 * i for i in range(10)]
    for i in range(10):
        yield find_case(r, base, base[i])
    yield find_case(r, base, 0x999)
    yield find_case(r, [0] * 10, 0)
    yield find_case(r, [0x100] * 10, 0x100)
    yield find_case(r, [0] * 10, 0x100)


def add_case(r, show=None, ans=None):
    return {"op": "add", "show": r.choice([0, 1, 2, 255]) if show is None else show,
            "queues": {"AddATag": [r.choice([0, 0x5000]) if ans is None else ans]}}


def add_edges():
    r = random.Random(12)
    for show in (0, 1, 2, 255):
        for ans in (0, 0x5000):
            yield add_case(r, show, ans)


TESTS = [
    FnTest("FindStaticTag", ppc, host, edge=find_edges, rnd=find_case),
    FnTest("AddTag", ppc, host, edge=add_edges, rnd=add_case),
]
