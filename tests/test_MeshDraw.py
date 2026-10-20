"""The main app's mesh drawing registration (TPEngine.cpp): the three EnableMeshDraw and two
DisableMeshDraw overloads, which link the engine mesh behind a tag_sMesh into the map lists
and keep the drawing flags in the mesh's first word. The engine calls (CMapWho::Link and
Unlink, CMesh::SetLocalMatrix) are recorded on both sides."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
SHIM_LIBRARY = "unit_MeshDraw"
ENTRIES = [0x100650c4, 0x10065178, 0x10065200, 0x100653a4, 0x1006548c]
MASK = 0xffffffff
def ws(e, a, v): e.write(a, struct.pack(">" + "I" * len(v), *[x & MASK for x in v]))
def rs(e, a, n): return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))
def setup(runner): T.install_std_stubs(runner.emu)
def ppc(e, case):
    op, flags, present, from_, to, mode, matrix_words = case
    e.cl = CL.PpcLog({})
    st = CL.Stubs()
    mesh = e.alloc(0x40)
    engine = e.alloc(0x15c)
    link = engine + 0x154
    ws(e, mesh, [flags])
    ws(e, mesh + 0x1c, [engine if present else 0])
    ws(e, engine, [link])
    matrix = e.alloc(64)
    ws(e, matrix, matrix_words)
    def single(name):
        return lambda e: (e.cl.rec(name, [int(e.r(4) == link), e.r(5)]), e.set_r(3, 0))
    def bounds(name):
        def stub(e):
            ints = [e.r(i) for i in (4, 5, 6, 7)] + [int(e.r(8) == link)]
            if name == "LinkRange": ints.append(e.r(9))
            e.cl.rec(name, ints)
            e.set_r(3, 0)
        return stub
    def local_matrix(e):
        e.cl.rec("SetLocalMatrix", [int(e.r(3) == engine), e.r(4)])
        words = rs(e, e.r(5), 16)
        e.cl.rec("Matrix", words[:8])
        e.cl.rec("Matrix", words[8:])
    st.imp("Link__7CMapWhoFP12CMapLinkBasei", single("Link"))
    st.imp("Unlink__7CMapWhoFP12CMapLinkBasei", single("Unlink"))
    st.imp("Link__7CMapWhoFiiiiP12CMapLinkBasei", bounds("LinkRange"))
    st.imp("Unlink__7CMapWhoFiiiiP12CMapLinkBase", bounds("UnlinkRange"))
    st.imp("SetLocalMatrix__5CMeshFiR7sMatrix", local_matrix)
    old = st.activate(e)
    try:
        T.call_ppc(e, ENTRIES[op], [mesh, matrix, mode & MASK] if op == 2 else [mesh, from_, to])
        return [e.u32(mesh)], e.cl.log
    finally:
        CL.Stubs.deactivate(e, old)
def host(lib, case):
    op, flags, present, from_, to, mode, matrix_words = case
    CL.host_begin(lib, {})
    fn = lib.tpw_md_run
    fn.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_uint32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32)]
    fn.restype = C.c_uint32
    out = fn(op, flags, present, from_, to, mode, (C.c_uint32 * 16)(*matrix_words))
    return [out], CL.host_end(lib)
def case(op, flags=0, present=1, from_=0xffff0002, to=0x00038000, mode=0, matrix_words=None):
    return [op, flags, present, from_, to, mode, matrix_words or [0xa5a5a500 + i for i in range(16)]]
def edges():
    for op in range(5):
        for flags in (0, 1, 8, 0x10, 0x18, 0xffffffff):
            yield case(op, flags)
            if op == 2:
                for mode in (-1, 2, -2147483648): yield case(op, flags, mode=mode)
        if op != 2: yield case(op, present=0)
    yield case(0, from_=0, to=0)
    yield case(4, from_=0xffffffff, to=0xffffffff)
def rnd(r):
    op = r.randrange(5)
    return case(op, r.getrandbits(32), 1 if op == 2 else r.randrange(2), r.getrandbits(32), r.getrandbits(32),
                r.choice([0, -1, 2, r.getrandbits(31)]), [r.getrandbits(32) for _ in range(16)])
TESTS = [FnTest("Mesh draw registration", ppc, host, edge=edges, rnd=rnd)]
