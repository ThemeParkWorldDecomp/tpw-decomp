"""Array helper ownership: HLSprite's 16 engine CSpriteList objects, reverse order.
The imported element destructor is a boundary fake, including its hidden -1 flag.
"""
import ctypes as C
import tpwtest as T
from batch3_support import *
from batch3_support import setup as common_setup

ARRAY = 0x102dbdfc

def setup(runner):
    common_setup(runner)
    def dtor(e):
        a = e.r(3); assert T.s16(e.r(4)) == -1
        e.b3_trace.append((41, pl(e, a)))
        e.set_r(3, a)
    runner.emu.stub("__dt__11CSpriteListFv", dtor)
    def destroy(e, call):
        a, dtor, size, count = [e.r(i) for i in range(3, 7)]
        assert (a, size, count) == (ARRAY, 0x28, 16)
        assert e.trap_to_import[e.u32(dtor)]["name"] == "__dt__11CSpriteListFv"
        for i in range(count - 1, -1, -1): call(dtor, a + size * i, 0xffff)
    T.stub_cb(runner.emu, "__destroy_arr", destroy)

def case(r): return dict(raw=r.randbytes(640), counts=[T.s32(r.getrandbits(32)) for _ in range(16)])

def ppc(e, c):
    e.write(ARRAY, c["raw"])
    for i, count in enumerate(c["counts"]): put(e, ARRAY + 0x28 * i + 4, [T.u32(count)])
    before = e.read(ARRAY, 640)
    begin_ppc(e, c, [("element%d" % i, ARRAY + i * 40, 40) for i in range(16)])
    T.call_ppc(e, e.func("__arraydtor$3071"), [])
    return dict(counts=[T.s32(e.u32(ARRAY + i * 40 + 4)) for i in range(16)],
                unchanged=e.read(ARRAY, 640) == before, trace=e.b3_trace, allocs=allocs(e))

def host(lib, c):
    begin_host(lib, c); values = (I32 * 16)(*c["counts"])
    F(lib, "tpw_HLSprite_seed", None, VP)(values)
    bases = [("element%d" % i, F(lib, "tpw_HLSprite_element", VP, I32)(i), 1) for i in range(16)]
    F(lib, "tpw_calls_reset", None)(); start_host(lib); F(lib, "tpw_HLSprite_arraydtor", None)()
    n = I32.in_dll(lib, "tpw_calls_n").value
    trace = [(int(row[0]), label(row[1], bases)) for row in ((U64 * 8) * 64).in_dll(lib, "tpw_calls")[:n]]
    F(lib, "tpw_HLSprite_read", None, VP)(values)
    out = end_host(lib, bases); out["trace"] = trace
    return dict(counts=list(values), unchanged=list(values) == c["counts"], **out)

TESTS = [T.FnTest("__arraydtor$3071 (HLSprite.cpp)", ppc, host, rnd=case,
         edge=lambda: [dict(raw=bytes([v]) * 640, counts=[v] * 16) for v in (0, 255)])]
