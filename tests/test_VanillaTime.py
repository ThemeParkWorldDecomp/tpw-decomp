"""File bytes, ignored aggregate result, count >> 2, state and call order."""
import ctypes as C
import random
import struct
import tpwtest as T
SHIM_LIBRARY = "legacy_testshim"
from batch3_support import *

GLOBAL = 0x10380a04

def case(r):
    count = r.choice(list(range(10)) + [r.getrandbits(32), 0xffffffff])
    return dict(words=T.rwords(r, 4), global_word=r.getrandbits(32), clock=r.getrandbits(32),
                count=count, result=r.getrandbits(32), data=r.randbytes(4),
                read_size=4 if 4 <= count < 8 else r.randrange(5), file=r.randbytes(32), null=r.choice([True, False]))

def edges():
    r = random.Random(30); out = []
    for count in list(range(10)) + [0x80000000, 0xffffffff]:
        for clock in (0, 1, 0x80000000, 0xffffffff):
            c = case(r); c.update(count=count, clock=clock, data=struct.pack("<I", clock ^ 0xffffffff), read_size=4)
            out.append(c)
    return out

def ppc(mode):
    def run(e, c):
        obj = e.alloc(16, struct.pack(">4I", *c["words"])); file = e.alloc(32, c["file"])
        put(e, GLOBAL, [c["global_word"]]); begin_ppc(e, c, [("object", obj, 16), ("file", file, 32)])
        ret = T.call_ppc(e, e.func(mode + "__12CVanillaTimeFPv"), [obj, 0 if c["null"] else file])[0]
        return dict(ret=ret, words=words(e, obj, 4), global_word=e.u32(GLOBAL), file=e.read(file, 32), trace=e.b3_trace, allocs=allocs(e))
    return run

def host(mode):
    def run(lib, c):
        begin_host(lib, c)
        obj = (U32 * 4)(*c["words"]); file = C.create_string_buffer(c["file"], 32)
        glob = F(lib, "tpw_VanillaTime_global", VP)(); U32.from_address(glob).value = c["global_word"]
        start_host(lib)
        ret = F(lib, "tpw_VanillaTime_" + mode, C.c_bool, VP, VP)(obj, None if c["null"] else C.addressof(file))
        return dict(ret=int(ret), words=list(obj), global_word=U32.from_address(glob).value, file=file.raw,
                    **end_host(lib, [("object", C.addressof(obj), 16), ("file", C.addressof(file), 32)]))
    return run

def sinit_ppc(e, c):
    put(e, GLOBAL, [c["global_word"]]); begin_ppc(e, c, [])
    T.call_ppc(e, e.func("__sinit_VanillaTime_cpp"), [])
    return dict(word=e.u32(GLOBAL), trace=e.b3_trace, allocs=allocs(e), registrations=e.registered)

def sinit_host(lib, c):
    begin_host(lib, c); obj = F(lib, "tpw_VanillaTime_global", VP)(); U32.from_address(obj).value = c["global_word"]
    start_host(lib); F(lib, "tpw_sinit_VanillaTime", None)()
    return dict(word=U32.from_address(obj).value, registrations=[], **end_host(lib, []))

TESTS = [T.FnTest("CVanillaTime::" + mode, ppc(mode), host(mode), edge=edges, rnd=case) for mode in ("Save", "Load")]
TESTS += [T.FnTest("__sinit_VanillaTime_cpp", sinit_ppc, sinit_host, edge=edges, rnd=case)]
