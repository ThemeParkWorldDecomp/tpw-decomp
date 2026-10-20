"""CMsgCheatMode's destructor/GetType, 0x10147050 and 0x101472bc.

Native vtables are ABI-specific. Check the original's exact derived-vtable write,
and compare payload, flags, untouched PowerPC padding, return-this and deletion.
The positive hidden destructor flag is a signed short, not an int or bool.
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest
from clock_support import (F, I32, MSG_VTABLE, U8, U32, VP, host_config, host_start,
                           host_stop, host_trace, ppc_config, setup)


def case(r):
    return dict(vtable=r.getrandbits(32), flags=r.getrandbits(32), payload=r.getrandbits(8),
                tail=r.randbytes(7), flag=r.choice([T.rint(r), -1, 0, 1, 0x10000, 0x1ffff]))


def edges():
    return [dict(vtable=0xdeadbeef, flags=0x81234567, payload=p, tail=b"abcdefg", flag=f)
            for p in (0, 1, 2, 255) for f in (-0x80000000, -0x8000, -1, 0, 1, 2, 0x7fff, 0x10000, 0x18000, 0x1ffff)]


def ppc_fn(dtor):
    def run(e, c):
        ppc_config(e, c)
        raw = struct.pack(">II", c["vtable"], c["flags"]) + bytes([c["payload"]]) + c["tail"]
        obj = e.alloc(16, raw)
        e.clock_bases = [("message", obj, 16)]
        ret = T.call_ppc(e, e.func("__dt__13CMsgCheatModeFv" if dtor else "GetType__13CMsgCheatModeCFv"),
                         [obj, T.S16(c["flag"])] if dtor else [obj])[0]
        after = e.read(obj, 16)
        return dict(ret_this=ret == obj if dtor else None, ret=None if dtor else T.s32(ret),
                    flags=e.u32(obj + 4), payload=after[8],
                    abi_ok=e.u32(obj) == (MSG_VTABLE if dtor else c["vtable"]),
                    tail_ok=after[9:] == c["tail"], trace=e.clock_trace)
    return run


def host_fn(dtor):
    def run(lib, c):
        host_config(lib, c)
        tail = ctypes.create_string_buffer(c["tail"], 7)
        obj = F(lib, "tpw_cheat_message_create", VP, U32, U8, VP)(c["flags"], c["payload"], tail)
        flags, payload = U32(), U8()
        after_tail = ctypes.create_string_buffer(7)
        # Creation is outside the observed call, just like constructing the raw PPC input.
        I32.in_dll(lib, "tpw_clock_log_n").value = 0
        I32.in_dll(lib, "tpw_alloc_n").value = 0
        host_start(lib)
        if dtor:
            ret = F(lib, "tpw_CMsgCheatMode_dtor", VP, VP, ctypes.c_int16, ctypes.POINTER(U32), ctypes.POINTER(U8), VP)(
                obj, T.s16(c["flag"]), ctypes.byref(flags), ctypes.byref(payload), after_tail)
        else:
            ret = F(lib, "tpw_CMsgCheatMode_GetType", I32, VP)(obj)
            F(lib, "tpw_cheat_message_fields", None, VP, ctypes.POINTER(U32), ctypes.POINTER(U8), VP)(
                obj, ctypes.byref(flags), ctypes.byref(payload), after_tail)
        host_stop(lib)
        out = dict(ret_this=ret == obj if dtor else None, ret=None if dtor else ret,
                   flags=flags.value, payload=payload.value, abi_ok=True, tail_ok=after_tail.raw == c["tail"],
                   trace=host_trace(lib, [("message", obj, 1)]))
        if not dtor or T.s16(c["flag"]) <= 0:
            F(lib, "tpw_cheat_message_free_storage", None, VP)(obj)
        return out
    return run


TESTS = [FnTest("CMsgCheatMode::~CMsgCheatMode", ppc_fn(True), host_fn(True), edge=edges, rnd=case),
         FnTest("CMsgCheatMode::GetType", ppc_fn(False), host_fn(False), edge=edges, rnd=case)]
