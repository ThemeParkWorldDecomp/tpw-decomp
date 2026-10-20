"""Heightfield setup, optional height backup, gamut sizing and actual map setup."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
import test_MapSetup as M
from tpwtest import FnTest
PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_MapSetup"
def setup(runner):
    M.setup(runner)
    e = runner.emu
    if not hasattr(e, "ms_free"):
        e.ms_free = T.fake_function(e, lambda e: (e.cl.rec("Free"), M.ws(e, e.r(3), [0])))
def canon(v): return 0x7fc00000 if v & 0x7f800000 == 0x7f800000 and v & 0x007fffff else v
def normal(out):
    return [canon(v) if i in (4, 5, 6) or 11 <= i < 23 or (i >= 27 and i - 27 < 21) else v for i, v in enumerate(out)]
def ppc(e, seed):
    s = M.Side(e, M.case([]))
    try:
        h = e.alloc(0x12974, b"\xa5" * 0x12974); header = e.alloc(0xb8); field = e.alloc(0x30); gamut = e.alloc(0x2d8, b"\xa5" * 0x2d8)
        heights = e.alloc(48); M.ws(e, heights, seed[11:23]); oldbuffer = e.alloc(48); M.ws(e, oldbuffer, [M.fb(42.)] * 12)
        M.ws(e, header + 0x6c, [field]); e.write(field + 4, struct.pack(">H", 12))
        M.ws(e, field + 0x18, seed[3:5]); M.ws(e, field + 8, seed[5:9]); M.ws(e, field + 0x20, seed[9:11]); M.ws(e, field + 0x28, [heights])
        M.ws(e, h + 0x12970, [oldbuffer if seed[1] else 0])
        for a in (0x100af618, 0x10052db8, 0x10165634, 0x10051f74): s.saved[a] = e.read(a, 4)
        M.ws(e, 0x100af618, [s.obj]); M.ws(e, 0x10052db8, [gamut]); M.ws(e, 0x10165634, seed[:1]); M.ws(e, 0x10051f74, [e.ms_free]); s.fail = seed[2]
        st = CL.Stubs(); st.real.update([0x1000d740, 0x1000dbc8, 0x10009c44, 0x10009cb4, 0x10009bb8, 0x10017984, 0x10019640])
        st.internal(0x1000c0e8, lambda e: e.cl.rec("Adjust"))
        def clip(e):
            e.cl.rec("YClip", [CL.fbits(e.f(1)), CL.fbits(e.f(2))]); M.ws(e, e.r(3) + 0x30, [CL.fbits(e.f(1)), CL.fbits(e.f(2))])
        st.internal(0x10009b80, clip); old = st.activate(e)
        try:
            T.call_ppc(e, 0x1000d740, [h, header])
            shadow = e.u32(h + 0x12970)
            out = [int(e.u32(h) == field), int(e.u32(h + 4) != 0)] + M.rs(e, h + 8, 5) + M.rs(e, h + 0x20, 2) + [e.u32(h + 0x12968), int(shadow != 0)]
            out += M.rs(e, shadow, 12) if shadow else [0] * 12
            out += M.rs(e, s.obj + 0x38, 2) + [int(e.u32(s.obj + 0x10) != 0), e.u32(s.obj + 0x70)]
            out += M.rs(e, gamut, 0x2d8 // 4)
            return normal(out), s.log.log
        finally: CL.Stubs.deactivate(e, old)
    finally: s.close()
def host(lib, seed):
    CL.host_begin(lib, {})
    fn = lib.tpw_ms_hfield; fn.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]; fn.restype = None
    out = (C.c_uint32 * (27 + 0x2d8 // 4))(); fn((C.c_uint32 * 23)(*seed), out)
    return normal(list(out)), CL.host_end(lib)
def case(flags=0, old=0, fail=0, width=3, height=2, x=0., z=0., dx=10., dz=20., low=-5., high=7.):
    return [flags, old, fail, width, height] + [M.fb(v) for v in (x, z, dx, dz, low, high)] + [M.fb(i * 0.5) for i in range(12)]
def edges():
    for flags in (0, 0x10000):
        for old in (0, 1):
            for fail in (0, 1):
                yield case(flags=flags, old=old, fail=fail)
                yield case(flags=flags, old=old, fail=fail, low=5., high=5., dx=0., dz=0.)
def rnd(r):
    return case(flags=r.choice([0, 0x10000]), old=r.randrange(2), fail=r.randrange(3) == 0, width=r.randrange(1, 6), height=r.randrange(1, 6),
                x=r.uniform(-100, 100), z=r.uniform(-100, 100), dx=r.uniform(0.01, 50), dz=r.uniform(0.01, 50), low=r.uniform(-10, 0), high=r.uniform(0, 10))
TESTS = [FnTest("Heightfield initialization and map gamut", ppc, host, edge=edges, rnd=rnd)]
