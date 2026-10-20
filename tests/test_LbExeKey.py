"""Differential tests for bullfrog LbExeKey.cpp. See difftest.py and tpwtest.py.

The key's reference number (0x10072678) and the guard byte and word that Initialise sets
(0x10073e7c, 0x10073e80) are seeded on both sides and compared afterwards. The guard is a
bool on the host, so it is seeded with 0 or 1 only.
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

KEY, GUARD, STATE = 0x10072674, 0x10073e7c, 0x10073e80
U32 = ctypes.c_uint32


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def ppc_seed(e, c):
    e.write(KEY, struct.pack(">3I", *c["key"]))
    e.write(GUARD, bytes([c["guard"]]))
    e.write(STATE, struct.pack(">I", c["state"]))


def ppc_read(e):
    return dict(key=list(struct.unpack(">3I", e.read(KEY, 12))), guard=e.read(GUARD, 1)[0], state=e.u32(STATE))


def host_seed(lib, c):
    (U32 * 3).in_dll(lib, "gLbExeKey")[:] = c["key"]
    ctypes.c_uint8.in_dll(lib, "gLbExeKeyInitialised").value = c["guard"]
    U32.in_dll(lib, "gLbExeKeyState").value = c["state"]


def host_read(lib):
    return dict(key=list((U32 * 3).in_dll(lib, "gLbExeKey")), guard=ctypes.c_uint8.in_dll(lib, "gLbExeKeyInitialised").value,
                state=U32.in_dll(lib, "gLbExeKeyState").value)


def case(r):
    return dict(key=[0x890cd511, r.getrandbits(32) if r.random() < 0.8 else r.choice([0, 1, 0xFFFFFFFF]), 0xe92a80c5],
                guard=r.choice([0, 1]), state=r.getrandbits(32))


def edges():
    return [dict(key=[0x890cd511, k, 0xe92a80c5], guard=g, state=s)
            for k in (0, 1, 0x80000000) for g in (0, 1) for s in (0, 0xFFFFFFFF)]


def ppc_fn(name, ret):
    def run(e, c):
        ppc_seed(e, c)
        r3, _, _ = T.call_ppc(e, e.func(name), [])
        return dict(ppc_read(e), **({"ret": r3} if ret else {}))
    return run


def host_fn(sym, ret):
    def run(lib, c):
        host_seed(lib, c)
        r = F(lib, sym, U32 if ret else None)()
        return dict(host_read(lib), **({"ret": r} if ret else {}))
    return run


TESTS = [
    FnTest("LbExeKey_ReferenceNumber", ppc_fn("LbExeKey_ReferenceNumber__Fv", True),
           host_fn("tpw_LbExeKey_ReferenceNumber", True), edge=edges, rnd=case),
    FnTest("LbExeKey_Initialise", ppc_fn("LbExeKey_Initialise__Fv", False),
           host_fn("tpw_LbExeKey_Initialise", False), edge=edges, rnd=case),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
