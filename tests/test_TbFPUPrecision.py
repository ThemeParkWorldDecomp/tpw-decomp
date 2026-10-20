"""Differential tests for bullfrog TbFPUPrecision.cpp (TbFPUFlag). See difftest.py and
tpwtest.py.

The Mac build compares against whatever r30 holds where the PC read the FPU word; the
PowerPC side sets r30 from the case before the call, the host's PlatformFPUFlags
placeholder returns the same value (its low half, which is all the code uses). DebugStr
messages compare in order (tpwtest's stub, the host placeholder's log). The destructor
(inline, 0x1006abdc) frees when its flag is positive (tpwtest's destructor convention).
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

VTABLE = 0x10073a90
VP, I32, U16 = ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint16


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def ppc_obj(e, c):
    return e.alloc(0xc, struct.pack(">I3HH", VTABLE, *c["fields"], 0x5A5A))


def ppc_fields(e, obj):
    return list(struct.unpack(">3H", e.read(obj + 4, 6)))


def ppc_call(e, c, name, args):
    obj = ppc_obj(e, c)
    e.debugstrs = []
    e.set_r(30, c["r30"])
    r3, _, _ = T.call_ppc(e, e.func(name), [T.I(obj)] + args)
    return obj, r3


def host_begin(lib, c):
    U16.in_dll(lib, "tpw_fpu_flags").value = c["r30"] & 0xFFFF
    I32.in_dll(lib, "tpw_debugstr_count").value = 0
    return F(lib, "tpw_TbFPUFlag_new", VP, U16, U16, U16)(*c["fields"])


def host_messages(lib):
    n = I32.in_dll(lib, "tpw_debugstr_count").value
    log = ((ctypes.c_ubyte * 256) * 16).in_dll(lib, "tpw_debugstr_log")
    return [bytes(log[i][1:1 + log[i][0]]) for i in range(min(n, 16))]


def host_fields(lib, obj):
    out = (U16 * 3)()
    F(lib, "tpw_TbFPUFlag_read", None, VP, VP)(obj, out)
    return list(out)


def check_ppc(e, c):
    obj, r3 = ppc_call(e, c, "CheckFlags__9TbFPUFlagFv", [])
    return dict(ret=T.s32(r3), fields=ppc_fields(e, obj), debug=e.debugstrs)


def check_host(lib, c):
    obj = host_begin(lib, c)
    r = F(lib, "tpw_TbFPUFlag_CheckFlags", I32, VP)(obj)
    out = dict(ret=r, fields=host_fields(lib, obj), debug=host_messages(lib))
    F(lib, "tpw_TbFPUFlag_free", None, VP)(obj)
    return out


def reset_ppc(e, c):
    obj, _ = ppc_call(e, c, "Reset__9TbFPUFlagFv", [])
    return dict(fields=ppc_fields(e, obj), debug=e.debugstrs)


def reset_host(lib, c):
    obj = host_begin(lib, c)
    F(lib, "tpw_TbFPUFlag_Reset", None, VP)(obj)
    out = dict(fields=host_fields(lib, obj), debug=host_messages(lib))
    F(lib, "tpw_TbFPUFlag_free", None, VP)(obj)
    return out


def set_ppc(e, c):
    obj, _ = ppc_call(e, c, "SetFlags__9TbFPUFlagFUsUs", [T.U16(c["set"]), T.U16(c["clear"])])
    return dict(fields=ppc_fields(e, obj), debug=e.debugstrs)


def set_host(lib, c):
    obj = host_begin(lib, c)
    F(lib, "tpw_TbFPUFlag_SetFlags", None, VP, U16, U16)(obj, c["set"], c["clear"])
    out = dict(fields=host_fields(lib, obj), debug=host_messages(lib))
    F(lib, "tpw_TbFPUFlag_free", None, VP)(obj)
    return out


def dtor_ppc(e, c):
    e.deletes = []
    obj, r3 = ppc_call(e, c, "__dt__9TbFPUFlagFv", [T.I(c["flag"])])
    return dict(fields=ppc_fields(e, obj), debug=e.debugstrs, deleted=e.deletes == [obj],
                vt_ok=e.u32(obj) == VTABLE, this_ret=r3 == obj)


def dtor_host(lib, c):
    obj = host_begin(lib, c)
    out = (U16 * 3)()
    F(lib, "tpw_TbFPUFlag_dtor", None, VP, VP)(obj, out)
    return dict(fields=list(out), debug=host_messages(lib), deleted=T.s16(c["flag"]) > 0, vt_ok=True,
                this_ret=True)


def r16(r):
    k = r.random()
    if k < 0.25:
        return 0
    if k < 0.5:
        return 1 << r.randrange(16)
    return r.getrandbits(16)


def case(r):
    return dict(fields=[r16(r), r16(r), r16(r)], r30=r.getrandbits(32), set=r16(r), clear=r16(r),
                flag=r.getrandbits(32))


def edges():
    out = []
    for fields in ([0, 0, 0], [0xFFFF, 0, 0], [0, 0x0300, 0], [0, 0, 0x003F], [0x1234, 0x0300, 0x003F],
                   [0xFFFF] * 3):
        for r30 in (0, 0xFFFF, 0x0300, 0x003F, 0x033F, 0xFFFF0000, 0x12340300):
            out.append(dict(fields=fields, r30=r30, set=0x0300, clear=0x0C00, flag=1))
    out += [dict(fields=[1, 2, 4], r30=0, set=s, clear=k, flag=f) for s in (0, 0xFFFF) for k in (0, 0xFFFF)
            for f in (0, 1, -1 & 0xFFFFFFFF, 0x10000, 0x8000)]
    return out


TESTS = [
    FnTest("TbFPUFlag::CheckFlags", check_ppc, check_host, edge=edges, rnd=case),
    FnTest("TbFPUFlag::Reset", reset_ppc, reset_host, edge=edges, rnd=case),
    FnTest("TbFPUFlag::SetFlags", set_ppc, set_host, edge=edges, rnd=case),
    FnTest("TbFPUFlag::~TbFPUFlag", dtor_ppc, dtor_host, edge=edges, rnd=case),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
    runner.emu.heap_in_scratch = True
