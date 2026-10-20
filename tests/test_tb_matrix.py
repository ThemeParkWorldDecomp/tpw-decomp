"""Differential tests for engine tb_matrix.cpp, tb_light.cpp, MatrixPowerPC.cpp and the
pieces of tb_profiler.cpp / tb_resource_display.cpp translated with them.

Stubs (tpwtest.install_std_stubs): sin/cos/sqrt/asin/acos call the host libm on both
sides (the original's MathLib is not verified); memset/memcpy, operator new/delete,
__construct_array (calls the original constructor back) are faithful; _USwapBlock32
swaps on the PowerPC side and is a no-op on a little-endian host (sMatrix::Byteswap feeds
both sides the same file bytes and compares the words); GetAbsoluteCycles returns test
values on both sides.

gei_MathUnit is a real CMatrixPowerPC on both sides: the original constructor builds
it in emulator memory (setup), the host uses a C++ instance, so View, operator*,
PreprocessMatrices and TestMathSpeed run the original MultMatrix/Transform/Project.
"""
import ctypes
import math
import struct

import tpwtest as T
from tpwtest import F, Buf, Glob, I, PpcOnly, S16, SVal, Same, Test, f32, rfloat, rwords

PROGRAM = "engine symbols shared"

A = dict(MathUnit=0x100afe94, ViewAngle=0x100afe98, Screen=0x100afe9c, ViewDir2D=0x100afedc,
         ViewDir2DPtr=0x100afeec, IntFinal=0x100afef0, FinalPtr=0x100aff70, IntInvView=0x100aff74,
         InvViewPtr=0x100afff4, IntView=0x100afff8, ViewPtr=0x100b0078, Proj=0x100b007c, Light=0x100b00bc,
         F00fc=0x100b00fc, FOV=0x100afe8c, Near=0x100afe90, Far=0x10051c54, LightInv=0x100af150,
         Display=0x10165634, System=0x10165f80, Profiler=0x100b050c)
MPPC_VTBL, MPPC_VTBL_BASE, CTB_VTBL = 0x100520b4, 0x100520d8, 0x10051c70
UNIT = {}

DISPLAY = {"mFlags": (0x00, "I"), "mF04": (0x04, "f"), "mZ48": (0x48, "f"), "mZ4C": (0x4c, "f"),
           "mZ54": (0x54, "f"), "mZ58": (0x58, "f"), "mZ5C": (0x5c, "f"), "mZ60": (0x60, "f"),
           "mZ68": (0x68, "f"), "mZ6C": (0x6c, "f"), "mZ70": (0x70, "f"), "mZ74": (0x74, "f"),
           "mZ78": (0x78, "f"), "mZ7C": (0x7c, "f")}
SYSTEM = {"mFlags": (0x00, "I"), "mF20": (0x20, "i")}


def dglob(field, v):
    off, fmt = DISPLAY[field]
    return Glob("gei_Display." + field, A["Display"] + off, ("tpw_matrix_field", "gei_Display." + field), fmt, [v])


def sglob(field, v):
    off, fmt = SYSTEM[field]
    return Glob("gei_System." + field, A["System"] + off, ("tpw_matrix_field", "gei_System." + field), fmt, [v])


def fglob(sym, addr, v):
    return Glob(sym, addr, sym, "f", [v])


def mglob(sym, addr, vals, fmt="f"):
    return Glob(sym, addr, sym, f"{len(vals)}{fmt}", vals)

# ----------------------------------------------------------------- value generators


def rmat(r, lo=-10.0, hi=10.0):
    return [f32(r.uniform(lo, hi)) for _ in range(16)]


IDENT = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]
IDENT = [float(x) for x in IDENT]


def matmul(a, b):
    return [sum(a[i * 4 + k] * b[k * 4 + j] for k in range(4)) for i in range(4) for j in range(4)]


def rrot(r, translate=True):
    """Rotation (and translation) matrix as float32 values."""
    ax, ay, az = (r.uniform(-math.pi, math.pi) for _ in range(3))
    cx, sx, cy, sy, cz, sz = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay), math.cos(az), math.sin(az)
    mx = [1, 0, 0, 0, 0, cx, -sx, 0, 0, sx, cx, 0, 0, 0, 0, 1]
    my = [cy, 0, sy, 0, 0, 1, 0, 0, -sy, 0, cy, 0, 0, 0, 0, 1]
    mz = [cz, -sz, 0, 0, sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    m = matmul(matmul(mx, my), mz)
    if translate:
        m[12], m[13], m[14] = (r.uniform(-500, 500) for _ in range(3))
    return [f32(x) for x in m]


def rinvertible(r):
    k = r.random()
    if k < 0.4:
        return rrot(r)
    if k < 0.7:
        m = rrot(r)
        s = [f32(r.uniform(0.1, 10.0)) for _ in range(3)]
        return [f32(m[i] * (s[i // 4] if i < 12 else 1.0)) for i in range(16)]
    return rmat(r)


LU_EDGES = [
    IDENT, [0.0] * 16, [2.0, 0, 0, 0, 0, 3.0, 0, 0, 0, 0, 4.0, 0, 0, 0, 0, 5.0],
    [0, 1.0, 0, 0, 1.0, 0, 0, 0, 0, 0, 0, 1.0, 0, 0, 1.0, 0],           # permutation
    [1.0, 2.0, 3.0, 4.0, 2.0, 4.0, 6.0, 8.0, 1.0, 0, 1.0, 0, 0, 1.0, 0, 1.0],   # rank deficient
    [1.0, 2.0, 3.0, 4.0, 0, 0, 0, 0, 5.0, 6.0, 7.0, 8.0, 9.0, 1.0, 2.0, 3.0],   # zero row
    [0, 2.0, 3.0, 4.0, 0, 6.0, 7.0, 8.0, 0, 1.0, 2.0, 3.0, 0, 5.0, 6.0, 7.0],   # zero column 0
    [1.0, 2.0, 3.0, 4.0, 0, 5.0, 6.0, 7.0, 0, 0, 8.0, 9.0, 0, 0, 0, 10.0],      # upper triangular
    [1e-30, 0, 0, 0, 0, 1e30, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0],
    [f32(x) for x in (1e-20, 1, 1, 1, 1, 1e-20, 1, 1, 1, 1, 1e-20, 1, 1, 1, 1, 1e-20)],
]
LU_EDGES = [[f32(float(x)) for x in m] for m in LU_EDGES]

ANGLES = [0.0, -0.0, f32(math.pi / 2), f32(math.pi), f32(-math.pi), f32(2 * math.pi), 1e-8, 1e10,
          3.4028234663852886e38, math.inf, -math.inf, math.nan, 1.0, -1.0, 100.0]


def rangle(r):
    k = r.random()
    if k < 0.5:
        return f32(r.uniform(-2 * math.pi, 2 * math.pi))
    if k < 0.8:
        return rfloat(r)
    return f32(r.uniform(-1e4, 1e4))


def rvec(r, lo=-100.0, hi=100.0):
    return [f32(r.uniform(lo, hi)) for _ in range(3)]

# ----------------------------------------------------------------- setups


def unit_ppc(emu, c, addr):
    emu.write(A["MathUnit"], struct.pack(">I", emu.unit_base))


def unit_host(lib, c, hbuf):
    lib.tpw_matrix_use_unit()


def cycles_ppc(vals):
    def f(emu, c, addr):
        emu.cycles, emu.cycle_i = list(vals(c)), 0
        unit_ppc(emu, c, addr)
    return f


def cycles_host(vals):
    def f(lib, c, hbuf):
        v = list(vals(c))
        arr = (ctypes.c_uint64 * 16).in_dll(lib, "tpw_cycles")
        for i, x in enumerate(v):
            arr[i] = x
        ctypes.c_int32.in_dll(lib, "tpw_cycles_n").value = len(v)
        ctypes.c_int32.in_dll(lib, "tpw_cycles_i").value = 0
        unit_host(lib, c, hbuf)
    return f


def reset_records_ppc(emu, c, addr):
    emu.asserts, emu.debugstrs, emu.news, emu.deletes = [], [], [], []

# ----------------------------------------------------------------- tests

TESTS = []


def add(*a, **k):
    TESTS.append(Test(*a, **k))


add("CMatrix::CMatrix()", "__ct__7CMatrixFv", "tpw_CMatrix_ctor",
    lambda c: [Buf("m", "16I", c["m"])],
    edge=lambda: [dict(m=[0] * 16), dict(m=[0xFFFFFFFF] * 16), dict(m=list(range(16)))],
    rnd=lambda r: dict(m=rwords(r, 16)))

add("CMatrix::Return", "Return__7CMatrixFii", "tpw_CMatrix_Return",
    lambda c: [Buf("m", "16f", c["m"]), I(c["row"]), I(c["col"])], ret="f32",
    edge=lambda: [dict(m=[float(i) for i in range(16)], row=i, col=j) for i in range(4) for j in range(4)],
    rnd=lambda r: dict(m=rmat(r, -1e6, 1e6), row=r.randrange(4), col=r.randrange(4)))

for _axis in "XYZ":
    add(f"CMatrix::Rotate{_axis}", f"Rotate{_axis}__7CMatrixFf", f"tpw_CMatrix_Rotate{_axis}",
        lambda c: [Buf("m", "16f", c["m"]), F(c["a"])],
        edge=lambda: [dict(m=[7.0] * 16, a=a) for a in ANGLES],
        rnd=lambda r: dict(m=rmat(r), a=rangle(r)))

add("lubksb", "lubksb__FR7sMatrixPiPf", "tpw_lubksb",
    lambda c: [Buf("a", "16f", c["a"]), Buf("indx", "4i", c["indx"]), Buf("b", "4f", c["b"])],
    edge=lambda: ([dict(a=IDENT, indx=[0, 1, 2, 3], b=b) for b in ([1.0, 0, 0, 0], [0, 0, 0, 1.0], [0.0] * 4)]
                  + [dict(a=m, indx=p, b=[1.0, 2.0, 3.0, 4.0]) for m in LU_EDGES[2:6]
                     for p in ([3, 2, 1, 0], [0, 0, 0, 0], [1, 3, 3, 3])]),
    rnd=lambda r: dict(a=rmat(r), indx=[r.randrange(4) for _ in range(4)],
                       b=[0.0 if r.random() < 0.2 else f32(r.uniform(-50, 50)) for _ in range(4)]))

add("ludcmp", "ludcmp__FR7sMatrixPiPf", "tpw_ludcmp",
    lambda c: [Buf("a", "16f", c["a"]), Buf("indx", "4i", [-7] * 4), Buf("d", "f", [7.0])],
    edge=lambda: [dict(a=m) for m in LU_EDGES],
    rnd=lambda r: dict(a=rinvertible(r) if r.random() < 0.5 else rmat(r)))

add("CMatrix::Inverse", "Inverse__7CMatrixFv", "tpw_CMatrix_Inverse",
    lambda c: [Buf("m", "16f", c["m"])],
    edge=lambda: [dict(m=m) for m in LU_EDGES],
    rnd=lambda r: dict(m=rinvertible(r)))

add("CMatrix::Load", "Load__7CMatrixFR7sMatrix", "tpw_CMatrix_Load",
    lambda c: [Buf("m", "16I", c["m"]), Buf("src", "16I", c["src"])],
    edge=lambda: [dict(m=[0] * 16, src=list(range(16)))],
    rnd=lambda r: dict(m=rwords(r, 16), src=rwords(r, 16)))

VIEW_EDGES = [
    dict(frm=[0.0, 0.0, -10.0], at=[0.0, 0.0, 0.0], up=[0.0, 1.0, 0.0], roll=0.0),
    dict(frm=[0.0, 0.0, -10.0], at=[0.0, 0.0, 0.0], up=[0.0, 1.0, 0.0], roll=f32(0.3)),
    dict(frm=[5.0, 5.0, 5.0], at=[5.0, 5.0, 5.0], up=[0.0, 1.0, 0.0], roll=0.0),      # zero view direction
    dict(frm=[0.0, 0.0, 0.0], at=[0.0, 10.0, 0.0], up=[0.0, 1.0, 0.0], roll=0.0),     # up parallel to view
    dict(frm=[1.0, 2.0, 3.0], at=[4.0, 5.0, 6.0], up=[0.0, 0.0, 0.0], roll=f32(-1.0)),
    dict(frm=[1.0, 2.0, 3.0], at=[4.0, 5.0, 6.0], up=[0.0, 1.0, 0.0], roll=math.nan),
]

add("CMatrix::View", "View__7CMatrixF10_D3DVECTOR10_D3DVECTOR10_D3DVECTORf", "tpw_CMatrix_View",
    lambda c: [Buf("m", "16f", [7.0] * 16), SVal("from", "3f", c["frm"]), SVal("at", "3f", c["at"]),
               SVal("up", "3f", c["up"]), F(c["roll"])],
    edge=lambda: VIEW_EDGES,
    rnd=lambda r: dict(frm=rvec(r), at=rvec(r), up=rvec(r, -1, 1) if r.random() < 0.7 else [0.0, 1.0, 0.0],
                       roll=0.0 if r.random() < 0.4 else rangle(r)),
    ppc_setup=unit_ppc, host_setup=unit_host)


def proj_globs(c):
    return [dglob("mFlags", c["flags"]), fglob("gProjectionNear", A["Near"], -3.0),
            fglob("gProjectionFar", A["Far"], -4.0), fglob("gProjectionFOV", A["FOV"], -5.0)]


add("CMatrix::Projection", "Projection__7CMatrixFfff", "tpw_CMatrix_Projection",
    lambda c: [Buf("m", "16f", [7.0] * 16), F(c["n"]), F(c["f"]), F(c["fov"])],
    globs=proj_globs,
    edge=lambda: [dict(n=n, f=f, fov=fov, flags=fl) for (n, f, fov) in
                  [(1.0, 1000.0, 60.0), (10.0, 10.0, 90.0), (0.0, 100.0, 45.0), (1.0, 0.0, 30.0),
                   (5.0, 2000.0, 0.0), (5.0, 2000.0, 180.0), (-1.0, 5.0, 360.0)] for fl in (0, 0x200, 0xFFFFFFFF)],
    rnd=lambda r: dict(n=f32(r.uniform(0.01, 100)) if r.random() < 0.7 else rfloat(r),
                       f=f32(r.uniform(100, 20000)) if r.random() < 0.7 else rfloat(r),
                       fov=f32(r.uniform(5, 150)) if r.random() < 0.8 else rfloat(r),
                       flags=r.getrandbits(32)))

ZIN = ("mZ48", "mZ54", "mZ58", "mZ5C")
ZOUT = ("mZ68", "mZ6C", "mZ70", "mZ74", "mZ78")


def adjust_globs(c):
    g = [dglob("mFlags", c["flags"]), dglob("mZ4C", c["z4c"]), fglob("gProjectionNear", A["Near"], c["n"]),
         fglob("gProjectionFOV", A["FOV"], c["fov"]), fglob("gProjectionFar", A["Far"], -4.0),
         dglob("mZ60", 11.0), dglob("mZ7C", 12.0)]
    g += [dglob(k, v) for k, v in zip(ZIN, c["zin"])]
    g += [dglob(k, -9.0) for k in ZOUT]
    return g


add("CMatrix::AdjustProjection", "AdjustProjection__7CMatrixFf", "tpw_CMatrix_AdjustProjection",
    lambda c: [Buf("m", "16f", [7.0] * 16), F(c["f"])],
    globs=adjust_globs,
    edge=lambda: [dict(f=f, n=n, fov=60.0, flags=fl, z4c=0.003, zin=[1.0, 0.1, 0.2, 1.0])
                  for f in (1000.0, 1.0, 0.0, 5000.0) for n in (1.0, 50.0) for fl in (0, 0x200)],
    rnd=lambda r: dict(f=f32(r.uniform(10, 20000)) if r.random() < 0.8 else rfloat(r),
                       n=f32(r.uniform(0.1, 100)), fov=f32(r.uniform(5, 150)), flags=r.getrandbits(32),
                       z4c=f32(r.uniform(0.0001, 2.0)), zin=[f32(r.uniform(-5, 5)) for _ in range(4)]))


def tv_check(p, h, c):
    want = [] if 0 <= c["type"] <= 4 else [("0", "tb_matrix.cpp", 601)]
    got, host = p["emu"].asserts, T.host_asserts(h["lib"], reset=True)
    return [] if got == want == host else [f"asserts: ppc={got} host={host} want={want}"]


add("CMatrix::TransformVertex", "TransformVertex__7CMatrixFPvPvi", "tpw_CMatrix_TransformVertex",
    lambda c: [Buf("m", "16f", c["m"]), Buf("out", "4f", [7.0] * 4), Buf("in", "4f", c["v"]), I(c["type"])],
    edge=lambda: [dict(m=rrot_fixed, v=[1.0, 2.0, 3.0, 1.0], type=t) for t in range(-2, 7)],
    rnd=lambda r: dict(m=rmat(r) if r.random() < 0.5 else rrot(r), v=[f32(r.uniform(-100, 100)) for _ in range(4)],
                       type=r.choice([0, 1, 2, 3, 4, 0, 1, 2, 3, 4, -1, 5, 9, -0x80000000])),
    ppc_setup=reset_records_ppc, check=tv_check)

add("CMatrix::operator*", "__ml__7CMatrixFRC7CMatrix", "tpw_CMatrix_mul",
    lambda c: [Buf("out", "16f", [7.0] * 16), Buf("a", "16f", c["a"]), Buf("b", "16f", c["b"])],
    edge=lambda: [dict(a=IDENT, b=IDENT), dict(a=LU_EDGES[4], b=LU_EDGES[7])],
    rnd=lambda r: dict(a=rinvertible(r), b=rinvertible(r)),
    ppc_setup=unit_ppc, host_setup=unit_host)

def byteswap_check(p, h, c):
    # File bytes in (the same 64 little-endian bytes on both sides), matrix words out:
    # the Mac swaps them into big-endian words, the host's _USwapBlock32 leaves them.
    pv, hv = struct.unpack(">16I", p["m"][0]), struct.unpack("<16I", h["m"][0])
    return [] if pv == hv else [f"words ppc={[hex(v) for v in pv]} host={[hex(v) for v in hv]}"]


add("sMatrix::Byteswap", "Byteswap__7sMatrixFv", "tpw_sMatrix_Byteswap",
    lambda c: [Buf("m", "64s", [struct.pack("<16I", *c["m"])])], compare=[], check=byteswap_check,
    edge=lambda: [dict(m=list(range(16))), dict(m=[0x12345678] * 16)],
    rnd=lambda r: dict(m=rwords(r, 16)))


def zsetup_globs(c):
    return [dglob(k, v) for k, v in zip(ZIN, c["zin"])] + [dglob(k, -9.0) for k in ZOUT]


add("CDisplay::ZSetup", "ZSetup__8CDisplayFfff", "tpw_CDisplay_ZSetup",
    lambda c: [PpcOnly(A["Display"]), F(c["a"]), F(c["b"]), F(c["c"])],
    globs=zsetup_globs,
    edge=lambda: [dict(a=1000.0, b=10.0, c=20.0, zin=[1.0, 0.1, 0.2, 1.0]),
                  dict(a=0.0, b=0.0, c=0.0, zin=[1.0, 0.1, 0.2, 1.0]),
                  dict(a=1.0, b=5.0, c=5.0, zin=[2.0, 0.5, 0.5, 0.5])],
    rnd=lambda r: dict(a=rfloat(r), b=f32(r.uniform(-1000, 1000)), c=f32(r.uniform(-1000, 1000)),
                       zin=[f32(r.uniform(-5, 5)) for _ in range(4)]))

add("CProfiler::GetClockCycleCount10h", "GetClockCycleCount10h__9CProfilerFv", "tpw_GetClockCycleCount10h",
    lambda c: [PpcOnly(A["Profiler"])], ret="i32",
    edge=lambda: [dict(v=v) for v in (0, 1, 0xFFFFFFFF, 0x100000000, 0x123456789ABCDEF0, 0xFFFFFFFFFFFFFFFF)],
    rnd=lambda r: dict(v=r.getrandbits(64)),
    ppc_setup=cycles_ppc(lambda c: [c["v"]]), host_setup=cycles_host(lambda c: [c["v"]]))

# ----------------------------------------------------------------- CMatrixPowerPC


def unit_arg():
    return PpcOnly(UNIT["obj"])


def soa_case(r):
    first, count = r.randrange(8), r.randrange(10)
    blocks = (first + count) // 4 + 1
    stride = r.choice([16, 20, 32])
    return dict(m=rinvertible(r), first=first, count=count, stride=stride,
                inp=[f32(r.uniform(-100, 100)) for _ in range(12 * blocks)])


add("CMatrixPowerPC::Transform(sVertexPosnSet)", "Transform__14CMatrixPowerPCFP7sMatrixPvP14sVertexPosnSetiii",
    "tpw_mppc_Transform4",
    lambda c: [unit_arg(), Buf("m", "16f", c["m"]), Buf("out", f"{max(1, c['count']) * c['stride'] // 4}f",
                                                         [7.0] * (max(1, c["count"]) * c["stride"] // 4)),
               Buf("in", f"{len(c['inp'])}f", c["inp"]), I(c["stride"]), I(c["first"]), I(c["count"])],
    edge=lambda: [dict(m=IDENT, first=f, count=n, stride=16, inp=[float(i) for i in range(36)])
                  for f in (0, 1, 3, 4) for n in (0, 1, 2, 5)],
    rnd=soa_case)


def strided_case(r):
    count = r.randrange(10)
    ins, outs = r.choice([12, 16, 20, 32]), r.choice([12, 16, 24, 32])
    t = r.choice([0, 1, 2, 3, 0, 1, 2, 3, -1, 4, 7])
    return dict(m=rinvertible(r), count=count, ins=ins, outs=outs, type=t,
                inp=[f32(r.uniform(-100, 100)) for _ in range(max(1, count) * ins // 4 + 4)])


add("CMatrixPowerPC::Transform(strided)", "Transform__14CMatrixPowerPCFP7sMatrixPvPviiii", "tpw_mppc_Transform",
    lambda c: [unit_arg(), Buf("m", "16f", c["m"]),
               Buf("out", f"{max(1, c['count']) * c['outs'] // 4 + 4}f", [7.0] * (max(1, c["count"]) * c["outs"] // 4 + 4)),
               Buf("in", f"{len(c['inp'])}f", c["inp"]), I(c["outs"]), I(c["ins"]), I(c["count"]), I(c["type"])],
    edge=lambda: [dict(m=rrot_fixed, count=n, ins=16, outs=16, type=t, inp=[float(i) for i in range(40)])
                  for t in (-1, 0, 1, 2, 3, 4) for n in (0, 1, 2, 3)],
    rnd=strided_case)

PROJ_Z = ("mZ60", "mZ68", "mZ6C", "mZ70", "mZ74", "mZ78", "mZ7C")


def project_case(r):
    count = r.randrange(10)
    stride = r.choice([16, 20, 32])
    verts = []
    for _ in range(max(1, count) * stride // 4):
        verts.append(0.0 if r.random() < 0.05 else f32(r.uniform(-100, 100)))
    z = [f32(r.uniform(-5, 5)) for _ in PROJ_Z]
    if r.random() < 0.3:
        z[1] = verts[2]                      # depth split exactly at the first vertex's z
    return dict(count=count, stride=stride, type=r.choice([0, 1, 2, 0, 1, 2, -1, 3, 9]), v=verts, z=z)


add("CMatrixPowerPC::Project", "Project__14CMatrixPowerPCFPviii", "tpw_mppc_Project",
    lambda c: [unit_arg(), Buf("v", f"{len(c['v'])}f", c["v"]), I(c["stride"]), I(c["count"]), I(c["type"])],
    ret="f32",
    globs=lambda c: [dglob(k, v) for k, v in zip(PROJ_Z, c["z"])],
    edge=lambda: [dict(count=n, stride=16, type=t, v=[1.0, 2.0, 3.0, 4.0] * 8, z=[0.5, split, 2.0, 3.0, 4.0, 5.0, 6.0])
                  for t in (-1, 0, 1, 2, 3) for n in (0, 1, 4, 5, 8) for split in (1.0, 3.0, 5.0)],
    rnd=project_case)


def debugstr_check(p, h, c):
    lib = h["lib"]
    want = [b"code missing"]
    host_n = ctypes.c_int32.in_dll(lib, "tpw_debugstr_count").value - c["_n0"]
    last = bytes((ctypes.c_ubyte * 256).in_dll(lib, "tpw_debugstr_last"))
    host = [last[1:1 + last[0]]] * host_n
    return [] if p["emu"].debugstrs == want and host == want else [f"DebugStr ppc={p['emu'].debugstrs} host={host}"]


def debugstr_host_setup(lib, c, hbuf):
    c["_n0"] = ctypes.c_int32.in_dll(lib, "tpw_debugstr_count").value


for _name, _mangled, _host in (("Light", "Light__14CMatrixPowerPCFP11SFVF_TLCTEXP9tag_sNodeP8sFaceSeti", "tpw_mppc_Light"),
                               ("TransformUV", "TransformUV__14CMatrixPowerPCFP16STMCoordinateSetP6sUVSetiii",
                                "tpw_mppc_TransformUV")):
    add(f"CMatrixPowerPC::{_name}", _mangled, _host,
        lambda c: [unit_arg(), PpcOnly(c["x"]), PpcOnly(c["x"]), PpcOnly(c["x"]), PpcOnly(c["x"])],
        edge=lambda: [dict(x=0)], rnd=lambda r: dict(x=r.getrandbits(32)),
        ppc_setup=reset_records_ppc, host_setup=debugstr_host_setup, check=debugstr_check)


def mult_args(c):
    a, b = Buf("a", "16f", c["a"]), Buf("b", "16f", c["b"])
    if c["alias"] == "a":
        return [unit_arg(), a, b, Same("a")]
    if c["alias"] == "b":
        return [unit_arg(), a, b, Same("b")]
    return [unit_arg(), a, b, Buf("out", "16f", [7.0] * 16)]


add("CMatrixPowerPC::MultMatrix", "MultMatrix__14CMatrixPowerPCFPCfPCfPf", "tpw_mppc_MultMatrix",
    mult_args,
    edge=lambda: [dict(a=a, b=b, alias=al) for (a, b) in ((IDENT, IDENT), (LU_EDGES[4], LU_EDGES[7]),
                                                           ([-0.0] * 16, [0.0] * 16)) for al in ("", "a", "b")],
    rnd=lambda r: dict(a=rinvertible(r), b=rmat(r), alias=r.choice(["", "", "a", "b"])))


def ctor_ppc_check(p, h, c):
    """PowerPC side: the vtables the constructor installs dispatch every CTransformBase
    slot to CMatrixPowerPC's own functions (through the -8 thunks)."""
    emu = p["emu"]
    obj = p["addr"]["obj"]
    names = []
    funcs = {f["entry"]: f["name"] for f in emu.functions}
    vbase = emu.u32(obj) if c["flag"] else None
    if c["flag"]:
        if vbase != obj + 8:
            return [f"vbase pointer {vbase:#x} != this+8"]
        vptr = emu.u32(vbase + 4)
        for slot in range(2, 9):
            code = emu.u32(emu.u32(vptr + 4 * slot))
            w = emu.u32(code + 16)   # thunk: li; lwzx; add; addi; b target
            if (w >> 26) == 18:
                off = w & 0x03FFFFFC
                off = off - 0x4000000 if off & 0x2000000 else off
                code = code + 16 + off
            names.append(funcs.get(code, hex(code)))
    want = ["CMatrixPowerPC::~CMatrixPowerPC", "CMatrixPowerPC::Transform", "CMatrixPowerPC::Transform",
            "CMatrixPowerPC::Project", "CMatrixPowerPC::Light", "CMatrixPowerPC::TransformUV",
            "CMatrixPowerPC::MultMatrix"] if c["flag"] else []
    mism = [] if names == want else [f"dispatch ppc={names}"]
    if emu.u32(obj + 4) != MPPC_VTBL:
        mism.append(f"vtable {emu.u32(obj + 4):#x}")
    return mism


add("CMatrixPowerPC::CMatrixPowerPC", "__ct__14CMatrixPowerPCFv", "tpw_mppc_ctor_check",
    lambda c: [Buf("obj", "5I", [0xAAAAAAAA] * 5), S16(c["flag"])], ret="none",
    edge=lambda: [dict(flag=1)], rnd=lambda r: dict(flag=r.choice([1, 2, 0x7FFF, -1])), compare=[],
    check=lambda p, h, c: ctor_ppc_check(p, h, c) + (
        [] if h["lib"].tpw_mppc_ctor_check() == 127 else ["host dispatch check failed"]))


def dtor_case_setup(construct):
    def f(emu, c, addr):
        reset_records_ppc(emu, c, addr)
        if construct:
            T.call_ppc(emu, emu.func("__ct__14CMatrixPowerPCFv"), [addr["obj"], 1])
    return f


def dtor_check(p, h, c):
    deleted = p["addr"]["obj"] in p["emu"].deletes
    want = c["flag"] > 0 if -0x8000 <= c["flag"] <= 0x7FFF else T.s16(c["flag"]) > 0
    return [] if deleted == want else [f"deleted ppc={deleted} want={want}"]


FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x1FFFF]

add("CMatrixPowerPC::~CMatrixPowerPC", "__dt__14CMatrixPowerPCFv", "tpw_matrix_unit_is_null",
    lambda c: [Buf("obj", "5I", [0] * 5), PpcOnly(T.u32(T.s16(c["flag"])))], ret="none",
    edge=lambda: [dict(flag=f) for f in FLAGS], rnd=lambda r: dict(flag=r.randint(-0x8000, 0x7FFF)),
    ppc_setup=dtor_case_setup(True), compare=[], check=dtor_check)

add("CTransformBase::~CTransformBase", "__dt__14CTransformBaseFv", "tpw_matrix_unit_is_null",
    lambda c: [Buf("obj", "3I", [1, 2, 3]), PpcOnly(T.u32(T.s16(c["flag"])))], ret="none",
    edge=lambda: [dict(flag=f) for f in FLAGS], rnd=lambda r: dict(flag=r.randint(-0x8000, 0x7FFF)),
    ppc_setup=dtor_case_setup(False), compare=[],
    check=lambda p, h, c: dtor_check(p, h, c) + (
        [] if p["obj"][1] == CTB_VTBL and p["obj"][0] == 1 and p["obj"][2] == 3 else [f"obj {p['obj']}"]))

# ----------------------------------------------------------------- file-level functions

PTR_ADDRS = [(A["ViewPtr"], A["IntView"]), (A["InvViewPtr"], A["IntInvView"]), (A["FinalPtr"], A["IntFinal"]),
             (A["ViewDir2DPtr"], A["ViewDir2D"])]


def ppc_pointer_codes(emu):
    codes = []
    for ptr, want in PTR_ADDRS:
        v = emu.u32(ptr)
        codes.append(0 if v == 0 else (1 if v == want else 2))
    unit = emu.u32(A["MathUnit"])
    ok = unit != 0 and emu.news and unit == emu.news[-1][1] + 8 and emu.u32(unit + 4) == MPPC_VTBL_BASE
    codes.append(0 if unit == 0 else (1 if ok else 2))
    return codes


def host_pointer_codes(lib):
    out = (ctypes.c_int32 * 5)()
    lib.tpw_matrix_pointer_codes(out)
    return list(out)


def init_ppc(emu, c, addr):
    reset_records_ppc(emu, c, addr)
    for ptr, _ in PTR_ADDRS:
        emu.write(ptr, b"\0\0\0\0")
    emu.write(A["MathUnit"], b"\0\0\0\0")


def init_host(lib, c, hbuf):
    lib.tpw_matrix_clear_pointers()
    lib.tpw_matrix_drop_unit()


def init_check(p, h, c):
    pc, hc = ppc_pointer_codes(p["emu"]), host_pointer_codes(h["lib"])
    h["lib"].tpw_matrix_drop_unit()
    news = [n for n, _ in p["emu"].news]
    mism = [] if pc == hc == [1, 1, 1, 1, 1] else [f"pointers ppc={pc} host={hc}"]
    return mism + ([] if news == [0x14] else [f"operator new sizes {news}"])


add("Matrix_Init", "Matrix_Init__Fv", "tpw_Matrix_Init",
    lambda c: [], ret="i32",
    globs=lambda c: [dglob("mF04", c["f04"]), mglob("gScreenMatrix", A["Screen"], c["screen"]),
                     sglob("mF20", c["f20"])],
    edge=lambda: [dict(f04=v, screen=[7.0] * 16, f20=0) for v in (0.0, 1.0, 0.75, -1.0, math.inf)],
    rnd=lambda r: dict(f04=rfloat(r), screen=rmat(r), f20=T.rint(r)),
    ppc_setup=init_ppc, host_setup=init_host, check=init_check)


def close_ppc(emu, c, addr):
    reset_records_ppc(emu, c, addr)
    if c["live"]:
        obj = T.persist_alloc(emu, 0x14)
        T.call_ppc(emu, emu.func("__ct__14CMatrixPowerPCFv"), [obj, 1])
        c["_obj"] = obj
        emu.write(A["MathUnit"], struct.pack(">I", obj + 8))
    else:
        emu.write(A["MathUnit"], b"\0\0\0\0")


def close_host(lib, c, hbuf):
    lib.tpw_matrix_drop_unit()
    if c["live"]:
        lib.tpw_matrix_new_unit()


def close_check(p, h, c):
    emu = p["emu"]
    want_del = [c["_obj"]] if c["live"] else []
    mism = [] if emu.deletes == want_del else [f"deletes {emu.deletes} want {want_del}"]
    if emu.u32(A["MathUnit"]) != 0 or not h["lib"].tpw_matrix_unit_is_null():
        mism.append("gei_MathUnit not cleared")
    return mism


add("Matrix_Close", "Matrix_Close__Fv", "tpw_Matrix_Close",
    lambda c: [], ret="none",
    edge=lambda: [dict(live=True), dict(live=False)], rnd=lambda r: dict(live=r.random() < 0.8),
    ppc_setup=close_ppc, host_setup=close_host, check=close_check)


def pre_globs(c):
    return [mglob("gei_InternalViewMatrix", A["IntView"], c["view"]),
            mglob("gei_ProjectionMatrix", A["Proj"], c["proj"]),
            mglob("gScreenMatrix", A["Screen"], c["screen"]),
            sglob("mFlags", c["flags"]),
            mglob("gei_InternalInvViewMatrix", A["IntInvView"], [5.0] * 32),
            mglob("gei_InternalFinalMatrix", A["IntFinal"], [6.0] * 32),
            mglob("gViewDir2D", A["ViewDir2D"], [8.0] * 4),
            fglob("gViewAngle", A["ViewAngle"], 9.0),
            mglob("gei_LightMatrix", A["Light"], [3.0] * 16)]


def pre_ppc(emu, c, addr):
    unit_ppc(emu, c, addr)
    for ptr, want in PTR_ADDRS:
        emu.write(ptr, struct.pack(">I", want))


def pre_host(lib, c, hbuf):
    unit_host(lib, c, hbuf)
    lib.tpw_matrix_set_pointers()


def proj_matrix(r):
    n, f, fov = r.uniform(0.5, 50), r.uniform(100, 20000), math.radians(r.uniform(20, 120))
    c, s = math.cos(fov / 2), math.sin(fov / 2)
    q = s / (1 - n / f)
    m = [0.0] * 16
    m[0], m[5], m[10], m[11], m[14] = c, c, q, s, -q * n
    return [f32(x) for x in m]


SCREEN = [f32(x) for x in (1.5, 0, 0, 0, 0, -2.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0)]

add("PreprocessMatrices", "PreprocessMatrices__Fv", "tpw_PreprocessMatrices",
    lambda c: [], ret="i32", globs=pre_globs,
    edge=lambda: [dict(view=IDENT + IDENT, proj=IDENT, screen=IDENT, flags=f) for f in (0, 0x20000000, 0xFFFFFFFF)]
                 + [dict(view=[0.0] * 32, proj=IDENT, screen=SCREEN, flags=0x20000000)],
    rnd=lambda r: dict(view=rrot(r) + rrot(r), proj=proj_matrix(r) if r.random() < 0.7 else rmat(r),
                       screen=SCREEN if r.random() < 0.5 else rmat(r), flags=r.getrandbits(32)),
    ppc_setup=pre_ppc, host_setup=pre_host)


def speed_this():
    return PpcOnly(UNIT["base"])


add("CTransformBase::TestMathSpeed", "TestMathSpeed__14CTransformBaseFv", "tpw_TestMathSpeed",
    lambda c: [speed_this()], ret="f32",
    globs=lambda c: [dglob(k, v) for k, v in zip(PROJ_Z, c["z"])],
    edge=lambda: [dict(t0=a, t1=b, z=[0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0])
                  for a, b in ((0, 0), (0, 1000), (0xFFFFFFF0, 0x100000010), (5, 3), (0, 0x7FFFFFFF))],
    rnd=lambda r: dict(t0=r.getrandbits(64), t1=r.getrandbits(64), z=[f32(r.uniform(-5, 5)) for _ in PROJ_Z]),
    ppc_setup=cycles_ppc(lambda c: [c["t0"], c["t1"]]), host_setup=cycles_host(lambda c: [c["t0"], c["t1"]]))


def sinit_globs(c):
    w = c["w"]
    return [Glob("g_100b00fc", A["F00fc"], "g_100b00fc", "I", w[0:1]),
            mglob("gei_LightMatrix", A["Light"], w[1:17], "I"),
            mglob("gei_ProjectionMatrix", A["Proj"], w[17:33], "I"),
            mglob("gei_InternalViewMatrix", A["IntView"], w[33:65], "I"),
            mglob("gei_InternalInvViewMatrix", A["IntInvView"], w[65:97], "I"),
            mglob("gei_InternalFinalMatrix", A["IntFinal"], w[97:129], "I"),
            mglob("gScreenMatrix", A["Screen"], w[129:145], "I")]


add("__sinit_tb_matrix_cpp", "__sinit_tb_matrix_cpp", "tpw_sinit_tb_matrix",
    lambda c: [], globs=sinit_globs,
    edge=lambda: [dict(w=[0] * 145), dict(w=[0xFFFFFFFF] * 145)], rnd=lambda r: dict(w=rwords(r, 145)))

add("__sinit_tb_light_cpp", "__sinit_tb_light_cpp", "tpw_sinit_tb_light",
    lambda c: [], globs=lambda c: [mglob("gLightInverseWorld", A["LightInv"], c["w"], "I")],
    edge=lambda: [dict(w=[0] * 16)], rnd=lambda r: dict(w=rwords(r, 16)))

add("__sinit_tb_profiler_cpp", "__sinit_tb_profiler_cpp", "tpw_sinit_tb_profiler",
    lambda c: [], globs=lambda c: [Glob("gProfiler", A["Profiler"], "gProfiler", "1092I", c["w"])],
    edge=lambda: [dict(w=[0] * 1092)], rnd=lambda r: dict(w=rwords(r, 1092)))

rrot_fixed = rrot(__import__("random").Random(7))


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    obj = T.persist_alloc(emu, 0x14)
    T.call_ppc(emu, emu.func("__ct__14CMatrixPowerPCFv"), [obj, 1])
    emu.unit_obj, emu.unit_base = obj, emu.u32(obj)
    UNIT["obj"], UNIT["base"] = obj, emu.unit_base
    runner.lib.tpw_matrix_use_unit()
