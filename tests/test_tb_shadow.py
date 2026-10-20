"""Differential tests for engine tb_shadow.cpp (see difftest.py, tpwtest.py).

Stubs as test_tb_matrix (tpwtest.install_std_stubs: sqrt/asin/acos call the host libm on
both sides). AddLightSource reads gei_ViewMatrix (pointed at gei_InternalViewMatrix on
both sides), gUpVector and the heading reference, which the cases vary as well as using
the shipped values (0, 1, 0) and (0, 1).
"""
import ctypes
import math
import struct

import tpwtest as T
from tpwtest import Buf, Glob, I, S16, SVal, Test, f32, rwords

PROGRAM = "engine symbols shared"

SHADOW = 0x10166018
UP, HEADING = 0x10051c48, 0x10051da8
INT_VIEW, VIEW_PTR = 0x100afff8, 0x100b0078
OBJ = "108f4I2i"          # CShadow, 0x1c8 bytes


def robj(r, count=None, current=None):
    floats = [f32(r.uniform(-50, 50)) for _ in range(108)]
    shades = rwords(r, 4)
    return floats + shades + [count if count is not None else r.choice([-1, 0, 1, 2, 3, 4, 5]),
                              current if current is not None else r.choice([-1, 0, 1, 2, 3, 4, 5])]


def runit(r):
    while True:
        v = [r.uniform(-1, 1) for _ in range(3)]
        n = math.sqrt(sum(x * x for x in v))
        if n > 1e-3:
            return [f32(x / n * r.choice([1.0, 1.0, 3.0, 0.25])) for x in v]


DIR_EDGES = [[0.0, -1.0, 0.0], [0.0, 1.0, 0.0], [1.0, -1.0, 0.0], [0.0, -1.0, 1.0], [0.0, 0.0, 0.0],
             [1.0, 0.0, 0.0], [0.3, -0.5, -0.8], [-0.3, -0.5, 0.8], [-2.0, -4.0, -1.0], [1e-20, -1e-20, 0.0]]


def add_globs(c):
    return [Glob("gUpVector", UP, "gUpVector", "3f", c["up"]),
            Glob("gShadowHeadingReference", HEADING, "gShadowHeadingReference", "2f", c["ref"]),
            Glob("gei_InternalViewMatrix", INT_VIEW, "gei_InternalViewMatrix", "16f", c["view"])]


def view_ppc(emu, c, addr):
    emu.write(VIEW_PTR, struct.pack(">I", INT_VIEW))


def view_host(lib, c, hbuf):
    lib.tpw_matrix_set_pointers()


def add_case(r):
    k = r.random()
    return dict(obj=robj(r), dir=runit(r) if k < 0.8 else [f32(r.uniform(-5, 5)) for _ in range(3)],
                up=[0.0, 1.0, 0.0] if r.random() < 0.6 else runit(r),
                ref=[0.0, 1.0] if r.random() < 0.6 else [f32(r.uniform(-2, 2)) for _ in range(2)],
                view=[f32(r.uniform(-2, 2)) for _ in range(16)])


def offset_ret(r3, r4, f1, addr):
    return -1 if r3 == 0 else T.s32(r3 - addr["obj"])


def iter_case(r):
    return dict(obj=robj(r))


def dtor_check(p, h, c):
    deleted = p["addr"]["obj"] in p["emu"].deletes
    want = T.s16(c["flag"]) > 0
    fn = h["lib"]["tpw_CShadow_dtor"]
    host = bool(fn(ctypes.c_void_p(1), ctypes.c_int16(T.s16(c["flag"]))))
    return [] if deleted == want == host else [f"deleted ppc={deleted} host={host} want={want}"]


def dtor_setup(emu, c, addr):
    emu.deletes = []


def sinit_check(p, h, c):
    emu = p["emu"]
    want = [(SHADOW, 0x10050600, 0x101661e0)]
    mism = [] if emu.registered[-1:] == want else [f"__register_global_object {emu.registered[-1:]}"]
    if emu.u32(0x10050600) != emu.func("__dt__7CShadowFv"):
        mism.append("registered destructor is not CShadow::~CShadow")
    return mism


def sinit_setup(emu, c, addr):
    emu.registered = []


TESTS = [
    Test("CShadow::ResetLightSources", "ResetLightSources__7CShadowFv", "tpw_CShadow_ResetLightSources",
         lambda c: [Buf("obj", OBJ, c["obj"])], edge=lambda: [dict(obj=[1.0] * 108 + [5] * 4 + [3, 2])],
         rnd=iter_case),
    Test("CShadow::AddLightSource", "AddLightSource__7CShadowF10_D3DVECTOR", "tpw_CShadow_AddLightSource",
         lambda c: [Buf("obj", OBJ, c["obj"]), SVal("dir", "3f", c["dir"])], ret="i32", globs=add_globs,
         edge=lambda: [dict(obj=[2.0] * 108 + [7] * 4 + [n, 9], dir=d, up=[0.0, 1.0, 0.0], ref=[0.0, 1.0],
                            view=[1.0, 0.5, 0.25, 0, 0.5, 1.0, 0.75, 0, 0, 0, 1.0, 0, 3.0, 4.0, 5.0, 1.0])
                       for d in DIR_EDGES for n in (0, 3, 4)],
         rnd=add_case, ppc_setup=view_ppc, host_setup=view_host),
    Test("CShadow::GetFirstShadowMatrix", "GetFirstShadowMatrix__7CShadowFv", "tpw_CShadow_GetFirstShadowMatrix",
         lambda c: [Buf("obj", OBJ, c["obj"])], ret="i32", ppc_ret=offset_ret,
         edge=lambda: [dict(obj=[0.0] * 108 + [0] * 4 + [n, 7]) for n in (-1, 0, 1, 4)], rnd=iter_case),
    Test("CShadow::GetNextShadowMatrix", "GetNextShadowMatrix__7CShadowFv", "tpw_CShadow_GetNextShadowMatrix",
         lambda c: [Buf("obj", OBJ, c["obj"])], ret="i32", ppc_ret=offset_ret,
         edge=lambda: [dict(obj=[0.0] * 108 + [0] * 4 + [n, cur]) for n in (0, 1, 4) for cur in (-2, -1, 0, 2, 3)],
         rnd=iter_case),
    Test("CShadow::~CShadow", "__dt__7CShadowFv", "tpw_CShadow_dtor",
         lambda c: [Buf("obj", OBJ, [0.0] * 108 + [0] * 6), S16(c["flag"])], ret="none", compare=[],
         edge=lambda: [dict(flag=f) for f in (0, 1, -1, 2, 0x7FFF, -0x8000)],
         rnd=lambda r: dict(flag=r.randint(-0x8000, 0x7FFF)), ppc_setup=dtor_setup, check=dtor_check),
    Test("__sinit_tb_shadow_cpp", "__sinit_tb_shadow_cpp", "tpw_sinit_tb_shadow",
         lambda c: [], globs=lambda c: [Glob("gShadow", SHADOW, "gShadow", "114I", c["w"])],
         edge=lambda: [dict(w=[0] * 114), dict(w=[0xFFFFFFFF] * 114)], rnd=lambda r: dict(w=rwords(r, 114)),
         ppc_setup=sinit_setup, check=sinit_check),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
