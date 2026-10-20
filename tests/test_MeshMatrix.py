"""The engine's CMesh::SetLocalMatrix overloads (tb_mesh_matrix.cpp): the given matrix times a
Y flip goes into the mesh's first local matrix for flag 1 and its second for flag 2, and the
mesh flags take the same bits. The multiply runs through a real CMatrixPowerPC on both sides
(the original constructor builds the one in emulator memory), as in test_tb_matrix.py."""
import ctypes as C
import struct
import tpwtest as T
from tpwtest import FnTest
PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_MeshMatrix"
FN = [0x1001d3d0, 0x1001d654]   # CMatrix& then sMatrix&
MATH_UNIT = 0x100afe94
MASK = 0xffffffff
def fb(v): return struct.unpack(">I", struct.pack(">f", v))[0]
def ws(e, a, v): e.write(a, struct.pack(">" + "I" * len(v), *[x & MASK for x in v]))
def rs(e, a, n): return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))
def canonical(words): return [0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w for w in words]
def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    obj = T.persist_alloc(e, 0x14)
    T.call_ppc(e, e.func("__ct__14CMatrixPowerPCFv"), [obj, 1])
    e.mesh_matrix_unit = e.u32(obj)
def ppc(e, case):
    which, flags, matrix, state = case
    e.write(MATH_UNIT, struct.pack(">I", e.mesh_matrix_unit))
    mesh = e.alloc(0x15c, b"\xa5" * 0x15c)
    ws(e, mesh + 4, state[:1])
    ws(e, mesh + 0x10, state[1:])
    source = e.alloc(64)
    ws(e, source, matrix)
    T.call_ppc(e, FN[which], [mesh, flags & MASK, source])
    return canonical([e.u32(mesh + 4)] + rs(e, mesh + 0x10, 32)), []
def host(lib, case):
    which, flags, matrix, state = case
    fn = lib.tpw_mm_run
    fn.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]
    fn.restype = None
    io = (C.c_uint32 * 33)(*state)
    fn(which, flags, (C.c_uint32 * 16)(*matrix), io)
    return canonical(list(io)), []
IDENTITY = [fb(float(i % 5 == 0)) for i in range(16)]
def case(which, flags, matrix=None, state=None):
    return [which, flags, matrix or IDENTITY, state or [0xa5a5a500 + i for i in range(33)]]
def edges():
    matrices = [IDENTITY, [0] * 16, [fb(float(i)) for i in range(16)],
                [fb(v) for v in (1., 2., 3., 0., 2., -1., 1., 0., 0., 3., 2., 0., 500., 100., 500., 1.)],
                [fb(v) for v in (1e-8, 2., 7., 2., 3., 1e8, 2., 7., 9., 3., 1e-9, 4., 5., 3., 2., 1.)],
                [fb(float("inf")), fb(float("-inf")), fb(float("nan")), 0x7f800001] + [fb(1.)] * 12,
                [0x00000001, 0x80000001, 0x7f7fffff, 0xff7fffff] + [fb(-2.)] * 12]
    for which in (0, 1):
        for flags in (0, 1, 2, 3, 4, 0xffffffff, -2147483648):
            for matrix in matrices:
                yield case(which, flags, matrix)
        yield case(which, 3, state=[0] * 33)
        yield case(which, 3, state=[0xffffffff] * 33)
def rnd(r):
    matrix = [fb(r.uniform(-1000, 1000)) if r.random() < 0.9 else r.getrandbits(32) for _ in range(16)]
    return case(r.randrange(2), r.choice([0, 1, 2, 3, r.getrandbits(32)]), matrix,
                [r.getrandbits(32) for _ in range(33)])
TESTS = [FnTest("CMesh::SetLocalMatrix, both overloads", ppc, host, edge=edges, rnd=rnd)]
