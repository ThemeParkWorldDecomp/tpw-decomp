"""The lobby's actual external-camera branch and the main application's LU helpers."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
SHIM_LIBRARY = "unit_CameraMatrix"
FN = [0x10033dc0, 0x100c0e28, 0x100c2230, 0x100c210c, 0x100c249c, 0x1003491c]
FLAGS, EXTERNAL, INVERSE, CURRENT = 0x102b5a40, 0x102b5a50, 0x102b5ab0, 0x102b5af0
MASK = 0xffffffff
def fb(v): return struct.unpack(">I", struct.pack(">f", v))[0]
def ws(e, a, values): e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))
def rs(e, a, n): return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))
def canonical(out): return [0x7fc00000 if (i < 64 or 68 <= i <= 72) and v & 0x7f800000 == 0x7f800000 and v & 0x007fffff else v for i, v in enumerate(out)]
def setup(runner): T.install_std_stubs(runner.emu)
def ppc(e, case):
    op, argument, flags, seed = case
    matrix = e.alloc(64); ws(e, matrix, seed[:16])
    destination = e.alloc(64, b"\xa5" * 64)
    view = e.alloc(64, b"\xa5" * 64)
    indices = e.alloc(16); ws(e, indices, seed[16:20])
    values = e.alloc(16); ws(e, values, seed[20:24])
    parity = e.alloc(4); ws(e, parity, seed[24:25])
    vp = e.data_import("gei_ViewMatrix")
    saved = {a: e.read(a, n) for a, n in ((FLAGS, 4), (EXTERNAL, 64), (INVERSE, 64), (CURRENT, 4), (vp, 4))}
    ws(e, FLAGS, [flags]); ws(e, EXTERNAL, seed[:16]); ws(e, CURRENT, [0]); ws(e, vp, [view]); e.write(INVERSE, b"\xa5" * 64)
    e.cl = log = CL.PpcLog({}); st = CL.Stubs(); st.real.update(FN)
    def load(e): e.cl.rec("Load"); e.write(e.r(3), e.read(e.r(4), 64))
    st.imp("Load__7CMatrixFR7sMatrix", load); old = st.activate(e)
    try:
        args = [[argument & MASK], [matrix], [matrix, indices, parity], [matrix, indices, values],
                [matrix if argument else destination, matrix], []][op]
        T.call_ppc(e, FN[op], args)
        out = rs(e, matrix, 16) + rs(e, destination, 16) + rs(e, view, 16) + rs(e, INVERSE, 16)
        out += rs(e, indices, 4) + rs(e, values, 4) + rs(e, parity, 1) + [e.u32(FLAGS), int(e.u32(CURRENT) == EXTERNAL)]
        return canonical(out), log.log
    finally:
        CL.Stubs.deactivate(e, old)
        for a, b in saved.items(): e.write(a, bytes(b))
def host(lib, case):
    op, argument, flags, seed = case
    CL.host_begin(lib, {})
    fn = lib.tpw_cm_case; fn.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]; fn.restype = None
    out = (C.c_uint32 * 75)(); fn(op, argument, flags, (C.c_uint32 * 25)(*seed), out)
    return canonical(list(out)), CL.host_end(lib)
def case(op, matrix, argument=0, flags=0, indices=(0, 1, 2, 3), values=(1., 2., 3., 4.)):
    return op, argument, flags, [*[fb(v) for v in matrix], *indices, *[fb(v) for v in values], fb(-3.)]
def edges():
    identity = [float(i % 5 == 0) for i in range(16)]
    for flags in (0, 0x80, 0x16, MASK):
        for enabled in (0, 1, -1, -2147483648): yield case(0, identity, enabled, flags)
    matrices = [identity, [0.] * 16, list(range(16)), [1., 2., 3., 0., 2., -1., 1., 0., 0., 3., 2., 0., 500., 100., 500., 1.],
                [1e-8, 2., 7., 2., 3., 1e8, 2., 7., 9., 3., 1e-9, 4., 5., 3., 2., 1.]]
    for matrix in matrices:
        for op in (1, 2, 3, 4, 5):
            yield case(op, matrix, flags=0x80)
            if op == 4: yield case(op, matrix, argument=1)
    for values in ((0., 0., 0., 0.), (0., 0., 1., 0.), (9., -7., 1., 0.)):
        yield case(3, matrices[3], indices=(3, 1, 2, 0), values=values)
def rnd(r):
    op = r.randrange(6); matrix = [r.uniform(-1000, 1000) for _ in range(16)]
    indices = list(range(4)); r.shuffle(indices)
    return case(op, matrix, argument=r.randrange(2), flags=r.getrandbits(32) | 0x80,
                indices=indices, values=[r.uniform(-100, 100) for _ in range(4)])
TESTS = [FnTest("External camera and main-app matrix helpers", ppc, host, edge=edges, rnd=rnd)]
