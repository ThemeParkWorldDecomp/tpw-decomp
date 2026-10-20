"""CCheat, main app 0x10146f24..0x10147294. See clock_support.py for fakes.

Compare all 12 object bytes, file-reference identity, file buffers, the singleton,
all 116 message-centre words, allocations and calls in order. Inputs include loaded
noncanonical boolean bytes. SetCheatStatus itself takes valid C++ bools (0 or 1).
"""
import ctypes
import random
import struct

import tpwtest as T
SHIM_LIBRARY = "legacy_testshim"
from tpwtest import FnTest
from clock_support import (CENTRE, F, I32, U8, VP, host_config, host_start, host_stop,
                           host_trace, ppc_config, setup)


def case(r):
    return dict(obj=r.randbytes(12), status=r.randint(0, 1), present=r.random() < 0.5,
                centre=T.rwords(r, 116), flags=r.getrandbits(32), sub=T.rwords(r, 4),
                post_mask=r.getrandbits(32), file=r.randbytes(32), null=r.random() < 0.2,
                counts=[r.choice([0, 1, 1, 1, 2, 0xffffffff, r.getrandbits(32)]) for _ in range(2)],
                results=T.rwords(r, 2), data=[r.getrandbits(8), r.getrandbits(8)],
                writes=[r.randint(0, 1), r.randint(0, 1)])


def edges():
    r = random.Random(8)
    out = []
    for a in (0, 1, 2, 255):
        for first, second in ((0, 1), (1, 0), (1, 1), (2, 1), (1, 2), (0xffffffff, 0xffffffff)):
            for present in (False, True):
                c = case(r)
                c.update(obj=bytes(range(9)) + bytes([a, a, 255 - a]), counts=[first, second],
                         present=present, status=a & 1, writes=[1, 1], data=[255 - a, a])
                out.append(c)
    return out


def ppc_fn(name):
    def run(e, c):
        ppc_config(e, c)
        obj = e.alloc(12, c["obj"])
        file = e.alloc(32, c["file"])
        ref = e.alloc(4, struct.pack(">I", 0 if c["null"] else file))
        centre = e.alloc(0x1d0, struct.pack(">116I", *c["centre"]))
        e.write(CENTRE, struct.pack(">I", centre if c["present"] else 0))
        e.clock_bases = [("cheat", obj, 12), ("file", file, 32), ("centre", centre, 0x1d0)]
        args = [obj]
        suffix = "v"
        if name == "SetCheatStatus":
            args.append(c["status"])
            suffix = "b"
        elif name in ("Save", "Load"):
            args.append(ref)
            suffix = "RPv"
        label = "__ct__6CCheatFv" if name == "ctor" else name + "__6CCheat" + ("C" if name == "Save" else "") + "F" + suffix
        ret = T.call_ppc(e, e.func(label), args)[0]
        actual_centre = e.u32(CENTRE)
        return dict(ret=(ret == obj) if name == "ctor" else (ret if name in ("Save", "Load", "IsCheating") else None),
                    obj=e.read(obj, 12), file=e.read(file, 32), ref=e.u32(ref) == (0 if c["null"] else file),
                    centre=None if actual_centre == 0 else list(struct.unpack(">116I", e.read(actual_centre, 0x1d0))),
                    unused_centre=list(struct.unpack(">116I", e.read(centre, 0x1d0))), trace=e.clock_trace)
    return run


def host_fn(name):
    def run(lib, c):
        host_config(lib, c)
        obj = ctypes.create_string_buffer(c["obj"], 12)
        file = ctypes.create_string_buffer(c["file"], 32)
        ref = VP(None if c["null"] else ctypes.addressof(file))
        centre = ctypes.create_string_buffer(struct.pack("<116I", *c["centre"]), 0x1d0)
        F(lib, "tpw_cheat_set_centre", None, VP)(ctypes.addressof(centre) if c["present"] else None)
        host_start(lib)
        if name == "SetCheatStatus":
            ret = F(lib, "tpw_CCheat_" + name, None, VP, ctypes.c_bool)(ctypes.addressof(obj), c["status"])
        elif name in ("Save", "Load"):
            ret = F(lib, "tpw_CCheat_" + name, I32, VP, ctypes.POINTER(VP))(ctypes.addressof(obj), ctypes.byref(ref))
        else:
            ret = F(lib, "tpw_CCheat_" + name, U8 if name == "IsCheating" else None, VP)(ctypes.addressof(obj))
        host_stop(lib)
        ptr = F(lib, "tpw_cheat_get_centre", VP)()
        bases = [("cheat", ctypes.addressof(obj), 12), ("file", ctypes.addressof(file), 32)]
        if ptr:
            bases.append(("centre", ptr, 0x1d0))
        out = dict(ret=True if name == "ctor" else ret, obj=obj.raw, file=file.raw,
                   ref=ref.value == (None if c["null"] else ctypes.addressof(file)),
                   centre=None if ptr is None else list(struct.unpack("<116I", ctypes.string_at(ptr, 0x1d0))),
                   unused_centre=list(struct.unpack("<116I", centre.raw)), trace=host_trace(lib, bases))
        if ptr is not None and not c["present"]:
            F(lib, "tpw_cheat_release_centre", None)()
        F(lib, "tpw_cheat_set_centre", None, VP)(None)
        return out
    return run


TESTS = [FnTest("CCheat::" + ("CCheat" if name == "ctor" else name), ppc_fn(name), host_fn(name), edge=edges, rnd=case)
         for name in ("ctor", "SetCheatStatus", "IsCheating", "Load", "Save")]
