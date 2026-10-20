"""Level terrain loading through the real ride loader, plus backup and shutdown."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
import test_RideAnimLoad as R
from tpwtest import FnTest
SHIM_LIBRARY = "unit_RideAnimLoad"
setup = R.setup
SET, QUADS, HEADER, COPY = 0x102c6c74, 0x102c6c78, 0x102c6c7c, 0x102c6c80
def state(e):
    p = e.u32(COPY); out = [int(e.u32(HEADER) != 0), int(e.u32(QUADS) != 0), int(p != 0)]
    if p:
        counts = struct.unpack(">HH", e.read(p + 4, 4)); heights, quads = e.u32(p + 0x28), e.u32(p + 0x2c)
        out += list(counts) + [int(heights == p + 0x30), int(quads == p + 0x30 + 48)]
        out += list(struct.unpack(">12I", e.read(heights, 48))) + list(struct.unpack(">12H", e.read(quads, 24)))
    return out
def ppc(e, c):
    s = R.Side(e, c)
    try:
        actual_options = e.u32(0x10260b10)
        for a in (SET, QUADS, HEADER, COPY, actual_options): s.saved[a] = e.read(a, 4)
        for a in (SET, QUADS, HEADER, COPY): R.ws(e, a, [0])
        R.ws(e, actual_options, [c["options"]])
        level = e.alloc(20); R.ws(e, level + 8, [e.alloc(20, b"Data:Lobby\0")])
        theme = e.alloc(32, b"Data:Fantasy\0")
        st = CL.Stubs(); st.real.update([0x1005bc44, 0x1005beec, 0x1005c0fc, R.ENTRY])
        st.internal(0x10005050, lambda e: e.set_r(3, level))
        st.internal(0x1015139c, lambda e: e.set_r(3, theme))
        def shared(e): e.cl.rec("SharedPath"); e.write(e.r(3), b"Data:Fantasy:sharetex\0")
        st.internal(0x100b8960, shared)
        st.internal(0x100680fc, lambda e: (e.cl.rec("Directory", (), [e.cstr(e.r(i)) for i in (3, 4, 5)]), e.set_r(3, 1)))
        for address, name in ((0x100319c0, "HFieldTextures"), (0x100b971c, "NodeGamuts"), (0x1005b9d0, "Flatness")):
            st.internal(address, lambda e, name=name: (e.cl.rec(name), e.set_r(3, 1)))
        st.internal(0x1005b8fc, lambda e: e.cl.rec("Procedural", [T.s32(e.r(3))]))
        st.internal(0x10065178, lambda e: e.cl.rec("Enable"))
        def init_set(e):
            e.cl.rec("InitSet", [e.r(4), e.r(5), e.r(6)])
            R.ws(e, e.r(3) + 4, [e.u32(e.r(3) + 4) | c["set_flags"]]); e.set_r(3, 1)
        st.internal(0x100c7768, init_set)
        st.internal(0x100650c4, lambda e: e.cl.rec("EnableRange", [e.r(4) >> 16, e.r(4) & 65535, e.r(5) >> 16, e.r(5) & 65535]))
        st.internal(0x10064c54, lambda e: e.cl.rec("Free", [e.cl.lab(e.r(3))]))
        st.internal(0x1006a5e8, lambda e: e.cl.rec("DestroySet", [e.r(4)]))
        st.internal(0x10031b10, lambda e: e.cl.rec("FreeTextures"))
        old = st.activate(e)
        try:
            result = T.call_ppc(e, 0x1005bc44, [c["level"] & 0xffffffff, c["mode"] & 0xffffffff])[0]
            R.ws(e, s.output, [e.u32(SET) or 0x12121212])
            before = state(e); ride = s.state()
            if c["destroy"]: T.call_ppc(e, 0x1005c0fc, [])
            return result, ride, before, state(e), s.log.log
        finally: CL.Stubs.deactivate(e, old)
    finally: s.close()
def host(lib, c):
    CL.host_begin(lib, {})
    begin, load, ride, copy, destroy = lib.tpw_ral_begin, lib.tpw_ral_level, lib.tpw_ral_state, lib.tpw_ral_copy, lib.tpw_ral_destroy_level
    begin.argtypes = [C.POINTER(C.c_uint32)]; begin.restype = None
    load.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_uint32]; load.restype = C.c_int32
    for fn in (ride, copy): fn.argtypes = [C.POINTER(C.c_uint32)]; fn.restype = C.c_int32
    destroy.restype = None
    begin((C.c_uint32 * 18)(*c["seed"])); result = load(c["level"], c["mode"], c["options"], c["set_flags"])
    out = (C.c_uint32 * 256)(); rs = list(out[:ride(out)]); before = list(out[:copy(out)])
    if c["destroy"]: destroy()
    return result, rs, before, list(out[:copy(out)]), CL.host_end(lib)
def case(level=1, mode=0, options=0, load=1, destroy=1, catalog=None, set_flags=0):
    return dict(**R.case(load=load, catalog=catalog), level=level, mode=mode, options=options, destroy=destroy, set_flags=set_flags)
def edges():
    for level in (0, 1, -1):
        for options in (0, 0x2000):
            for load in (0, 1, 2):
                for flags in (0, 0x4000): yield case(level=level, options=options, load=load, mode=-3, set_flags=flags)
def rnd(r): return case(level=r.choice([0, 1, -1]), options=r.getrandbits(32), mode=r.randrange(-5, 6), load=r.randrange(3), destroy=r.randrange(2), set_flags=r.choice([0, 0x4000]), catalog=[r.choice([0, 1, 2, 16]) for _ in range(12)])
TESTS = [FnTest("Level heightfield, backup and destruction", ppc, host, edge=edges, rnd=rnd)]
