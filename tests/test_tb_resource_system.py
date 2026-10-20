"""Differential tests for engine tb_resource_system.cpp and CErrorHandler::GetAltTabStatus
(tb_error.cpp). See difftest.py and tpwtest.py.

The callees are not translated yet, so both sides log their calls and the cases compare
the logs: OpenGL_Init/OpenGL_Close and the CMapWho methods (stub_internal on the PowerPC
side, weak placeholders in tests/shim_placeholders.cpp on the host), the renderer's
virtual ClearViewport/FlipScreen (a fake object whose vtable entries are Python
functions on the PowerPC side, a test CRenderSystem on the host; FlipScreen answers from
a list), FrontWindow (InterfaceLib) and NS_MacDoze::GlobalMemoryStatus (both answer
test values). __assertion_failed is recorded on both sides (msl_assertion_failed on the
host, tests/shim_placeholders.cpp) and the records compare.
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"

SYSTEM, DISPLAY, ERRORS, MAPWHO_PTR = 0x10165f80, 0x10165634, 0x10052b44, 0x100af618
OPENGL_INIT, OPENGL_CLOSE = 0x100451a0, 0x100452e0
MAPWHO = {0x10018d70: 10, 0x10017b48: 11, 0x10017c58: 12, 0x10017d38: 13, 0x10017dd4: 14, 0x10017f10: 15,
          0x100180b4: 16, 0x100181e8: 17}
MAPWHO_ARGS = {10: 0, 11: 2, 12: 2, 13: 1, 14: 6, 15: 5, 16: 4, 17: 0}
GMS = "GlobalMemoryStatus__10NS_MacDozeFPQ210NS_MacDoze13_MEMORYSTATUS"
SURFACE_LOST = 0x887601C2
FAKE_MAPWHO = 0x7C0F0000
CLOCK_MHZ = 0x1005245c         # gCPUClockMHz (CPU.cpp)

VP, U32, I32 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int32


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f

# ----------------------------------------------------------------- PowerPC fakes


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.rs_log, emu.rs_flips, emu.rs_obj, emu.ogl, emu.front, emu.gms = [], [1], 0, (0, 0), 0, [0] * 8

    def rs_clear(e):
        e.rs_log.append((1, e.r(3) == e.rs_obj))

    def rs_flip(e):
        e.rs_log.append((2, e.r(3) == e.rs_obj))
        i = sum(1 for k, _ in e.rs_log if k == 2) - 1
        e.set_r(3, T.u32(e.rs_flips[min(i, len(e.rs_flips) - 1)]))

    emu.rs_tv = [T.fake_function(emu, rs_clear), T.fake_function(emu, rs_flip)]

    def opengl_init(e):
        e.calls_log.append((1, 0, T.s32(e.r(3))))
        e.set_r(3, T.u32(e.ogl[0]))

    def opengl_close(e):
        e.calls_log.append((2, 0))
        e.set_r(3, T.u32(e.ogl[1]))

    T.stub_internal(emu, OPENGL_INIT, opengl_init)
    T.stub_internal(emu, OPENGL_CLOSE, opengl_close)
    for addr, ident in MAPWHO.items():
        def mapwho(e, ident=ident):
            n = MAPWHO_ARGS[ident]
            e.calls_log.append((ident, e.r(3)) + tuple(T.s32(e.r(4 + i)) for i in range(n)))
        T.stub_internal(emu, addr, mapwho)
    emu.stub("FrontWindow", lambda e: e.set_r(3, e.front))
    emu.stub(GMS, lambda e: e.write(e.r(3), struct.pack(">8I", *e.gms)))
    emu.calls_log = []


def ppc_render_system(emu, which):
    """which: 1 = a fake renderer object, 0 = NULL."""
    if which != 1:
        emu.rs_obj = 0
        return 0
    vt = emu.alloc(0x30)
    emu.write(vt + 8, struct.pack(">II", *emu.rs_tv))
    obj = emu.alloc(0x10)
    emu.write(obj, struct.pack(">I", vt))
    emu.rs_obj = obj
    return obj

# ----------------------------------------------------------------- state on both sides
# sys: dict(scalars=[flags, light, f08, f0c, clocks, phys, renderer, f20], window, rs, cpu)
# disp: (frame, f84); errors: (count, [(level, code)] * 64)


SCALAR_FMT = "8I"


def cpu_bytes(e, cpu):
    """CPU (0x20 bytes): four int32 fields, then the 16-byte name."""
    ints, name = cpu
    return struct.pack(e + "4i", *ints) + name


def cpu_value(e, raw):
    return (list(struct.unpack(e + "4i", raw[:16])), raw[16:32])


def ppc_put_system(emu, a, sys, is_global=True):
    sc = sys["scalars"]
    rs = ppc_render_system(emu, sys["rs"]) if is_global else 0      # only gei_System gets a renderer
    emu.write(a, struct.pack(">IiiiIIIiII", sc[0], T.s32(sc[1]), T.s32(sc[2]), T.s32(sc[3]), sys["window"], sc[4],
                             sc[5], T.s32(sc[6]), sc[7], rs))
    emu.write(a + 0x28, cpu_bytes(">", sys["cpu"]))


def ppc_get_system(emu, a):
    v = struct.unpack(">IIIIIIIIII", emu.read(a, 0x28))
    rs = v[9]
    return dict(scalars=[v[0], v[1], v[2], v[3], v[5], v[6], v[7], v[8]], window=v[4],
                rs=0 if rs == 0 else (1 if rs == emu.rs_obj else -1), cpu=cpu_value(">", emu.read(a + 0x28, 0x20)))


def host_put_system(lib, s, sys):
    arr = (U32 * 8)(*[T.u32(x) for x in sys["scalars"]])
    F(lib, "tpw_system_scalars", None, VP, ctypes.POINTER(U32), I32)(s, arr, 1)
    F(lib, "tpw_system_window", VP, VP, VP, I32)(s, sys["window"] or None, 1)
    F(lib, "tpw_system_renderer", I32, VP, I32, I32)(s, sys["rs"], 1)
    ctypes.memmove(F(lib, "tpw_system_cpu", VP, VP)(s), cpu_bytes("<", sys["cpu"]), 0x20)


def host_get_system(lib, s):
    arr = (U32 * 8)()
    F(lib, "tpw_system_scalars", None, VP, ctypes.POINTER(U32), I32)(s, arr, 0)
    window = F(lib, "tpw_system_window", VP, VP, VP, I32)(s, None, 0) or 0
    rs = F(lib, "tpw_system_renderer", I32, VP, I32, I32)(s, 0, 0)
    return dict(scalars=list(arr), window=window, rs=rs,
                cpu=cpu_value("<", ctypes.string_at(F(lib, "tpw_system_cpu", VP, VP)(s), 0x20)))


def pack_errors(e, errs):
    count, entries = errs
    return struct.pack(e + "128ii", *[x for pair in entries for x in pair], count)


def unpack_errors(e, data):
    v = struct.unpack(e + "128ii", data)
    return (v[128], [tuple(v[2 * i:2 * i + 2]) for i in range(64)])


def host_addr(lib, sym):
    return ctypes.addressof(ctypes.c_char.in_dll(lib, sym))

# ----------------------------------------------------------------- cases


def rsys(r, rs=None):
    renderer = 3 if r.random() < 0.8 else r.choice([0, 1, 2, 4, -1, 0x7fffffff])
    return dict(scalars=[r.getrandbits(32), T.s32(T.rint(r)), T.s32(T.rint(r)), T.s32(T.rint(r)),
                         r.getrandbits(32), r.getrandbits(32), renderer, r.getrandbits(32)],
                window=r.choice([0, 0x7C0E0000, 0x7C0E1000]), rs=rs if rs is not None else r.choice([1, 1, 1, 0]),
                cpu=([T.s32(T.rint(r)) for _ in range(4)], r.randbytes(16)))


def rerrors(r):
    count = r.choice([0, 1, 2, 3, 8, 63, 64]) if r.random() < 0.7 else r.randint(0, 64)
    entries = []
    for _ in range(64):
        k = r.random()
        code = SURFACE_LOST if k < 0.2 else (r.choice([0, 1, 0x887601C1, 0x887601C3, 0x087601C2, 0xb0754000])
                                             if k < 0.4 else r.getrandbits(32))
        entries.append((T.s32(r.getrandbits(32)), T.s32(code)))
    return (count, entries)


def rcase(r):
    sys = rsys(r)
    front = sys["window"] if r.random() < 0.7 else r.choice([0, 0x7C0E0000, 0x7C0E2000])
    return dict(sys=sys, disp=(T.s32(T.rint(r)), T.s32(T.rint(r))), errors=rerrors(r), front=front,
                flips=[r.choice([1, 1, 0, 1, -1, 2]) for _ in range(3)], ogl=(T.s32(T.rint(r)), T.s32(T.rint(r))),
                args=[T.s32(T.rint(r)) for _ in range(6)], link=r.choice([0, 0x7C0D0000, r.getrandbits(32)]),
                gms=[r.getrandbits(32) for _ in range(8)], mapwho=FAKE_MAPWHO,
                clock=0 if r.random() < 0.5 else T.s32(T.rint(r)),
                gestalt=(0 if r.random() < 0.8 else -5550, T.s32(T.rint(r))))

# ----------------------------------------------------------------- one driver for every function


def ppc_run(label, args_fn, obj=None):
    def run(emu, c):
        emu.calls_log, emu.rs_log, emu.asserts = [], [], []
        emu.rs_flips, emu.front, emu.ogl, emu.gms = c["flips"], c["front"], c["ogl"], c["gms"]
        ppc_put_system(emu, SYSTEM, c["sys"])
        emu.write(DISPLAY + 0x80, struct.pack(">ii", *c["disp"]))
        emu.write(ERRORS, pack_errors(">", c["errors"]))
        emu.write(MAPWHO_PTR, struct.pack(">I", c["mapwho"]))
        emu.write(CLOCK_MHZ, struct.pack(">i", c["clock"]))     # CPU::detect (static initialiser)
        emu.gestalt = (T.u32(c["gestalt"][0]), c["gestalt"][1])
        this = []
        if obj == "store":
            a = emu.alloc(0x48)
            ppc_put_system(emu, a, c["store"], is_global=False)
            this = [a]
        elif obj == "errors":
            a = emu.alloc(0x204, pack_errors(">", c["store_errors"]))
            this = [a]
        r3 = T.call_ppc(emu, emu.func(label) if isinstance(label, str) else label, this + args_fn(c))[0]
        out = dict(ret=T.s32(r3), sys=ppc_get_system(emu, SYSTEM), disp=struct.unpack(">ii", emu.read(DISPLAY + 0x80, 8)),
                   errors=unpack_errors(">", emu.read(ERRORS, 0x204)), calls=list(emu.calls_log), rs=list(emu.rs_log),
                   asserts=list(emu.asserts))
        if obj == "store":
            out["store"] = ppc_get_system(emu, this[0])
        elif obj == "errors":
            out["store_errors"] = unpack_errors(">", emu.read(this[0], 0x204))
        return out
    return run


def host_calls(lib):
    n = I32.in_dll(lib, "tpw_calls_n").value
    rows = ((ctypes.c_uint64 * 8) * 64).in_dll(lib, "tpw_calls")
    out = []
    for i in range(n):
        r = list(rows[i])
        ident = r[0]
        if ident == 1:
            out.append((1, 0, T.s32(r[2])))
        elif ident == 2:
            out.append((2, 0))
        else:
            out.append((ident, r[1]) + tuple(T.s32(x) for x in r[2:2 + MAPWHO_ARGS[ident]]))
    return out


def host_run(fn_name, res, argtypes, args_fn, obj=None):
    def run(lib, c):
        T.host_asserts(lib, reset=True)
        F(lib, "tpw_calls_reset", None)()
        F(lib, "tpw_rs_reset", None)()
        flips = c["flips"]
        (I32 * 8).in_dll(lib, "tpw_rs_flips")[:len(flips)] = flips
        I32.in_dll(lib, "tpw_rs_nflips").value = len(flips)
        VP.in_dll(lib, "tpw_front_window").value = c["front"] or None
        I32.in_dll(lib, "tpw_opengl_init_result").value = c["ogl"][0]
        I32.in_dll(lib, "tpw_opengl_close_result").value = c["ogl"][1]
        (U32 * 8).in_dll(lib, "tpw_memory_status")[:] = c["gms"]
        sysp = F(lib, "tpw_system_global", VP)()
        host_put_system(lib, sysp, c["sys"])
        ctypes.memmove(host_addr(lib, "gei_Display") + 0x80, struct.pack("<ii", *c["disp"]), 8)
        ctypes.memmove(host_addr(lib, "gei_ErrorHandler"), pack_errors("<", c["errors"]), 0x204)
        F(lib, "tpw_mapwho_set", None, ctypes.c_size_t)(c["mapwho"])
        I32.in_dll(lib, "gCPUClockMHz").value = c["clock"]
        ctypes.c_int16.in_dll(lib, "tpw_gestalt_err").value = c["gestalt"][0]
        I32.in_dll(lib, "tpw_gestalt_value").value = c["gestalt"][1]
        this, keep = [], None
        if obj == "store":
            st = F(lib, "tpw_system_store", VP)()
            host_put_system(lib, st, c["store"])
            this = [st]
        elif obj == "errors":
            keep = ctypes.create_string_buffer(pack_errors("<", c["store_errors"]), 0x204)
            this = [ctypes.addressof(keep)]
        ret = F(lib, fn_name, res, *([VP] * len(this) + argtypes))(*(this + args_fn(c)))
        log = [(int(k), bool(ok)) for k, ok in zip((I32 * 32).in_dll(lib, "tpw_rs_log")[:I32.in_dll(lib, "tpw_rs_log_n").value],
                                                   (I32 * 32).in_dll(lib, "tpw_rs_log_ok")[:I32.in_dll(lib, "tpw_rs_log_n").value])]
        host_asserts = T.host_asserts(lib, reset=True)
        out = dict(ret=T.s32(ret or 0) if res is not None else None, sys=host_get_system(lib, sysp),
                   disp=struct.unpack("<ii", ctypes.string_at(host_addr(lib, "gei_Display") + 0x80, 8)),
                   errors=unpack_errors("<", ctypes.string_at(host_addr(lib, "gei_ErrorHandler"), 0x204)),
                   calls=host_calls(lib), rs=log, asserts=host_asserts)
        if obj == "store":
            out["store"] = host_get_system(lib, this[0])
        elif obj == "errors":
            out["store_errors"] = unpack_errors("<", keep.raw)
        return out
    return run


def fn_test(name, label, host_fn, res="i32", argtypes=(), args_fn=lambda c: [], obj=None,
            rnd=rcase, edge=None, void=False):
    ppc = ppc_run(label, args_fn, obj)
    host = host_run(host_fn, None if void else I32, list(argtypes), args_fn, obj)

    def ppc_wrapped(emu, c):
        out = ppc(emu, c)
        if void:
            out["ret"] = None
        return out
    return FnTest(name, ppc_wrapped, host, rnd=rnd, edge=edge)


def with_store(r):
    c = rcase(r)
    c["store"] = rsys(r, rs=0)
    c["store"]["window"] = 0
    return c


def with_errors(r):
    c = rcase(r)
    c["store_errors"] = rerrors(r)
    return c


def flip_edges():
    out = []
    base = dict(sys=dict(scalars=[0x03000141, -1, 0, 0, 1000, 64 << 20, 3, 1], window=0x7C0E0000, rs=1,
                         cpu=([6, 0, 1, 0], b"FunkyPowerPC\0\0\0\0")),
                disp=(41, 7), front=0x7C0E0000, ogl=(1, 1), args=[0] * 6, link=0, gms=[0] * 8, mapwho=FAKE_MAPWHO,
                clock=266, gestalt=(0, 266000000))
    for flips in ([1, 1, 1], [0, 0, 0], [0, 1, 0]):
        for errs in ((0, [(0, 0)] * 64), (3, [(2, T.s32(SURFACE_LOST))] * 64),
                     (4, [(1, 5), (2, T.s32(SURFACE_LOST)), (3, 7), (4, T.s32(SURFACE_LOST))] + [(0, 0)] * 60),
                     (64, [(i, T.s32(SURFACE_LOST) if i % 3 == 0 else i) for i in range(64)])):
            for front in (0x7C0E0000, 0x7C0E1000):
                out.append(dict(base, flips=flips, errors=errs, front=front))
    return out


I32S = [I32] * 6

TESTS = [
    fn_test("CSystem::OutputDebugInfo", "OutputDebugInfo__7CSystemFv", "tpw_CSystem_OutputDebugInfo", obj="store",
            rnd=with_store, void=True),
    fn_test("CSystem::UpdateLights", "UpdateLights__7CSystemFv", "tpw_CSystem_UpdateLights", obj="store",
            rnd=with_store, void=True),
    fn_test("RenderSystem_Init", "RenderSystem_Init__Fi", "tpw_RenderSystem_Init", argtypes=[I32],
            args_fn=lambda c: [c["args"][0]]),
    fn_test("RenderSystem_Close", "RenderSystem_Close__Fv", "tpw_RenderSystem_Close"),
    fn_test("RenderSystem_ClearViewport", "RenderSystem_ClearViewport__Fv", "tpw_RenderSystem_ClearViewport",
            rnd=lambda r: dict(rcase(r), sys=rsys(r, rs=1)), void=True),
    fn_test("RenderSystem_FlipScreen", "RenderSystem_FlipScreen__Fv", "tpw_RenderSystem_FlipScreen",
            rnd=lambda r: dict(rcase(r), sys=rsys(r, rs=1)), edge=flip_edges),
    fn_test("RenderSystem_Render", "RenderSystem_Render__Fv", "tpw_RenderSystem_Render", void=True),
    fn_test("RenderSystem_Flush", "RenderSystem_Flush__Fv", "tpw_RenderSystem_Flush",
            rnd=lambda r: dict(rcase(r), sys=rsys(r, rs=1)), edge=flip_edges),
    fn_test("RenderSystem_FrontLink", "RenderSystem_FrontLink__FP12CMapLinkBasei", "tpw_RenderSystem_FrontLink",
            argtypes=[ctypes.c_size_t, I32], args_fn=lambda c: [c["link"], c["args"][0]], void=True),
    fn_test("RenderSystem_FrontUnlink", "RenderSystem_FrontUnlink__FP12CMapLinkBasei", "tpw_RenderSystem_FrontUnlink",
            argtypes=[ctypes.c_size_t, I32], args_fn=lambda c: [c["link"], c["args"][0]], void=True),
    fn_test("RenderSystem_FrontReset", "RenderSystem_FrontReset__Fi", "tpw_RenderSystem_FrontReset",
            argtypes=[I32], args_fn=lambda c: [c["args"][0]], void=True),
    fn_test("RenderSystem_MapLink", "RenderSystem_MapLink__FiiiiP12CMapLinkBasei", "tpw_RenderSystem_MapLink",
            argtypes=[I32] * 4 + [ctypes.c_size_t, I32], args_fn=lambda c: c["args"][:4] + [c["link"], c["args"][4]],
            void=True),
    fn_test("RenderSystem_MapUnLink", "RenderSystem_MapUnLink__FiiiiP12CMapLinkBase", "tpw_RenderSystem_MapUnLink",
            argtypes=[I32] * 4 + [ctypes.c_size_t], args_fn=lambda c: c["args"][:4] + [c["link"]], void=True),
    fn_test("RenderSystem_Reset", "RenderSystem_Reset__Fiiii", "tpw_RenderSystem_Reset", argtypes=[I32] * 4,
            args_fn=lambda c: c["args"][:4], void=True),
    fn_test("RenderSystem_ResetAll", "RenderSystem_ResetAll__Fv", "tpw_RenderSystem_ResetAll", void=True),
    fn_test("__sinit_tb_resource_system_cpp", "__sinit_tb_resource_system_cpp", "tpw_sinit_tb_resource_system",
            void=True),
    fn_test("CErrorHandler::GetAltTabStatus", "GetAltTabStatus__13CErrorHandlerFv", "tpw_CErrorHandler_GetAltTabStatus",
            obj="errors", rnd=with_errors),
]
