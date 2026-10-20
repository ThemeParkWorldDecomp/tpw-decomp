"""Clock.cpp, 0x101472f4..0x10147754. All 25 object words are compared.
Time callees are controlled boundary fakes, described in clock_support.py.
"""
import ctypes
import random
import struct

import tpwtest as T
from tpwtest import FnTest
from clock_support import (CLOCK, F, I32, VP, host_config, host_start, host_stop,
                           host_trace, ppc_config, setup)

FUNCTIONS = [("Read", True, False), ("ReadFixed", True, False), ("Pause", False, False),
             ("UnPause", False, False), ("TogglePause", False, False), ("GetPauseFlag", True, False),
             ("Save", True, True), ("Load", True, True), ("PreSave", False, False),
             ("PostLoad", False, False)]


def case(r):
    return dict(words=T.rwords(r, 25), global_words=T.rwords(r, 25),
                returns=[r.choice([0, 1, -1, 2, -2, T.s32(r.getrandbits(32))]) for _ in range(32)],
                masks=[r.choice([0, r.getrandbits(32)]) for _ in range(32)],
                file=r.randbytes(32), null=r.random() < 0.2)


def edges():
    r = random.Random(7)
    out = []
    for first in (0, 1, -1, 2, -0x80000000, 0x7fffffff):
        for second in (0, 1, -1, 0x12345678):
            c = case(r)
            c["returns"][1] = first
            c["returns"][2] = second
            c["returns"][9] = first
            c["returns"][10] = c["returns"][12] = first
            c["returns"][11] = c["returns"][13] = second
            c["masks"] = [0xffffffff] * 32
            out.append(c)
    return out


def ppc_fn(name, returns, file_arg):
    def run(e, c):
        ppc_config(e, c)
        e.write(CLOCK, struct.pack(">25I", *c["global_words"]))
        obj = e.alloc(100, struct.pack(">25I", *c["words"]))
        file = e.alloc(32, c["file"])
        e.clock_bases = [("clock", obj, 100), ("file", file, 32)]
        e.clock_context = obj
        args = [obj] + ([0 if c["null"] else file] if file_arg else [])
        ret = T.call_ppc(e, e.func(name + "__6CClockF" + ("Pv" if file_arg else "v")), args)[0]
        return dict(ret=T.s32(ret) if returns else None,
                    words=list(struct.unpack(">25I", e.read(obj, 100))),
                    global_words=list(struct.unpack(">25I", e.read(CLOCK, 100))),
                    file=e.read(file, 32), trace=e.clock_trace)
    return run


def host_fn(name, returns, file_arg):
    def run(lib, c):
        host_config(lib, c)
        g = F(lib, "tpw_gClock", VP)()
        ctypes.memmove(g, struct.pack("<25I", *c["global_words"]), 100)
        obj = ctypes.create_string_buffer(struct.pack("<25I", *c["words"]), 100)
        file = ctypes.create_string_buffer(c["file"], 32)
        bases = [("clock", ctypes.addressof(obj), 100), ("file", ctypes.addressof(file), 32)]
        VP.in_dll(lib, "tpw_clock_context").value = ctypes.addressof(obj)
        host_start(lib)
        args = [ctypes.addressof(obj)] + ([None if c["null"] else ctypes.addressof(file)] if file_arg else [])
        ret = F(lib, "tpw_CClock_" + name, I32 if returns else None, VP, *([VP] if file_arg else []))(*args)
        host_stop(lib)
        return dict(ret=ret, words=list(struct.unpack("<25I", obj.raw)),
                    global_words=list(struct.unpack("<25I", ctypes.string_at(g, 100))),
                    file=file.raw, trace=host_trace(lib, bases))
    return run


def sinit_ppc(e, c):
    ppc_config(e, c)
    e.write(CLOCK, struct.pack(">25I", *c["words"]))
    e.clock_bases = [("clock", CLOCK, 100)]
    e.clock_context = CLOCK
    T.call_ppc(e, e.func("__sinit_Clock_cpp"), [])
    return dict(words=list(struct.unpack(">25I", e.read(CLOCK, 100))), trace=e.clock_trace)


def sinit_host(lib, c):
    host_config(lib, c)
    obj = F(lib, "tpw_gClock", VP)()
    ctypes.memmove(obj, struct.pack("<25I", *c["words"]), 100)
    VP.in_dll(lib, "tpw_clock_context").value = obj
    host_start(lib)
    F(lib, "tpw_sinit_Clock", None)()
    host_stop(lib)
    return dict(words=list(struct.unpack("<25I", ctypes.string_at(obj, 100))),
                trace=host_trace(lib, [("clock", obj, 100)]))


TESTS = [FnTest("CClock::" + name, ppc_fn(name, ret, file), host_fn(name, ret, file), edge=edges, rnd=case)
         for name, ret, file in FUNCTIONS]
TESTS.append(FnTest("__sinit_Clock_cpp", sinit_ppc, sinit_host, edge=edges, rnd=case))
