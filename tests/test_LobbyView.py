"""Lobby camera/light/fog scripts against the original PowerPC, including untouched fields."""
import ctypes as C
import struct
import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LobbyView"
U = {1: 0x100a8c44, 2: 0x100a8e18, 3: 0x100a8ed8, 4: 0x100a8f08, 5: 0x100a8fc8,
     7: 0x100a9004, 8: 0x100a9428, 9: 0x100a94f8, 10: 0x100a992c, 11: 0x100a9970,
     12: 0x100a99b4, 14: 0x100a9a40, 16: 0x100a9b64, 17: 0x100a8bb0}
CTOR = 0x100a8b40
CAMERA, OPTIONS, SETTINGS = 0x102b5a50, 0x102b4da8, 0x1038071c
MASK = 0xffffffff

def fb(f): return struct.unpack(">I", struct.pack(">f", f))[0]
def bf(w): return struct.unpack(">f", struct.pack(">I", w & MASK))[0]
def canon(w): return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w
def words(e, a, values): e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))

def setup(runner):
    T.install_std_stubs(runner.emu)
    if "c" not in FS.VT: FS.make_vtables(runner.emu)

class Side:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.view = e.alloc(100, b"\xa5" * 100)
        self.display = e.alloc(0x93c)
        self.sky = e.alloc(0x2f14)
        self.saved = {}
        def save(address, size): self.saved[address] = e.read(address, size)
        for address, size in ((CAMERA, 64), (OPTIONS + 0x20, 4), (SETTINGS, 4)):
            save(address, size)
        for address, target in ((0x1026067c, self.display), (0x10260b80, self.sky)):
            save(address, 4); words(e, address, [target])
        self.diffuse = e.data_import("GlobalDiffuse")
        save(self.diffuse, 16)
        self.chain = e.data_import("gev_LightChain")
        save(self.chain, 4); words(e, self.chain, [0])
        self.light_results = [0, 0, 0]
        seed = case["seed"]
        for address, values in ((self.display, seed[:1]), (self.display + 0x8bc, seed[1:2]),
            (self.display + 0x8d0, seed[2:4]), (self.display + 0x924, seed[4:5]),
            (OPTIONS + 0x20, seed[5:6]), (SETTINGS, seed[6:7]), (CAMERA, seed[7:23]),
            (self.diffuse, seed[23:27]), (self.sky + 0xf60, [0x12345678] * 1024)):
            words(e, address, values)
        e.cl = self.log = CL.PpcLog({})
        self.st = CL.Stubs()
        FS.string_stubs(self.st)
        def new(e):
            size = e.r(3)
            block = e.alloc(size, b"\xa5" * size)
            e.alloc_log.append(("new", block, size))
            e.set_r(3, block)
        self.st.imp("__nw__FUl", new)
        self.st.real.update([CTOR, *U.values(), 0x100079f4, 0x10007688, 0x100076d0, 0x10007704, 0x10007a98])
        self.st.internal(0x100a56c8, lambda e: e.cl.rec("BaseCtor"))
        self.st.internal(0x100a5744, lambda e: e.cl.rec("BaseDtor"))
        self.st.internal(0x10033dc0, lambda e: e.cl.rec("External", [T.s32(e.r(3))]))
        def identity(e):
            e.cl.rec("Identity")
            words(e, e.r(3), [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)])
        self.st.internal(0x100c0e28, identity)
        self.st.internal(0x1003491c, lambda e: e.cl.rec("HandleCamera"))
        self.st.imp("SkyLoad__4CSkyFPc", lambda e: (e.cl.rec("SkyLoad", (), (e.cstr(e.r(4)),)), e.set_r(3, 1)))
        self.st.imp("SetSpherePos__4CSkyF10_D3DVECTOR", lambda e: e.cl.rec("Sphere", [e.r(4), e.r(5), e.r(6)]))
        self.st.imp("ResetSpherePos__4CSkyFv", lambda e: e.cl.rec("ResetSphere"))
        self.st.imp("Projection__7CMatrixFfff", lambda e: e.cl.rec("Projection", [CL.fbits(e.f(i)) for i in (1, 2, 3)]))
        self.st.imp("Init__6CGamutFf", lambda e: e.cl.rec("Gamut", [CL.fbits(e.f(1))]))
        self.st.imp("UpdateLights__7CSystemFv", lambda e: e.cl.rec("Lights"))
        self.st.imp("Link__7CMapWhoFP12CMapLinkBasei", lambda e: (e.cl.rec("Link", [int(e.r(4) != 0), T.s32(e.r(5))]), e.set_r(3, 1)))
        self.st.imp("Unlink__7CMapWhoFP12CMapLinkBasei", lambda e: (e.cl.rec("Unlink", [int(e.r(4) != 0), T.s32(e.r(5))]), e.set_r(3, 1)))
        def fog(e):
            e.cl.rec("Fog", [e.r(4), e.u32(self.display)])
            words(e, self.display + 0x8bc, [e.r(4)])
        # Use the import name for SetFogColor: shared by SetFogSettings and Close.
        self.st.imp("SetFogColor__8CDisplayFUl", fog)
        self.old = self.st.activate(e)
        self.call(CTOR, self.view)
        e.alloc_log = []

    def call(self, address, *args): return T.call_ppc(self.e, address, list(args))[0]
    def op(self, op, a):
        e, v = self.e, self.view
        if op == 6:
            words(e, v + 4, a[:7]); words(e, v + 0x28, a[7:10])
        elif op == 13: words(e, v + 0x44, a[:6])
        elif op == 15: words(e, v + 0x5c, a[:2])
        elif op == 17: self.call(U[op], v, 0); self.view = 0
        elif op == 18:
            light = e.u32(v + 0x20)
            words(e, light + 0x50, a[:1])
            self.light_results = [self.call(0x10007704, light), self.call(0x100076d0, light),
                                  int(self.call(0x10007688, light) == light + 0x38)]
        elif op == 19: self.call(0x10007a98, e.u32(v + 0x20), 1)
        elif op == 9:
            color = e.alloc(4); words(e, color, [a[2]])
            direction = e.alloc(12); words(e, direction, a[3:6])
            self.call(U[op], v, a[0] & MASK, color if a[1] else 0, direction if a[6] else 0, a[7] & MASK)
        elif op == 12: self.call(U[op], v, a[0] & MASK, T.F(bf(a[1])), T.F(bf(a[2])))
        else: self.call(U[op], v, *[x & MASK for x in a])

    def state(self):
        e, v = self.e, self.view
        def read(a, n): return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))
        out = read(v + 4, 24) if v else []
        if v:
            out[7] = int(e.u32(v + 0x20) != 0)
            for i in (0, 1, 2, 3, 4, 5, 6, 9, 10, 14, 15): out[i] = canon(out[i])
        out += [e.u32(self.display), e.u32(self.display + 0x8bc), canon(e.u32(self.display + 0x8d0)),
                canon(e.u32(self.display + 0x8d4)), e.u32(self.display + 0x924)]
        out += [canon(w) for w in read(CAMERA, 16) + read(self.diffuse, 4)]
        out += read(self.sky + 0xf60, 1024)
        light = e.u32(v + 0x20) if v else 0
        if light:
            out += read(light + 0x50, 2) + [canon(w) for w in read(light + 4, 4) + read(light + 0x2c, 3)]
            out += [int(e.u32(light + 0x58) == light)]
        out += self.light_results + [int(e.u32(self.chain) != 0)]
        return out
    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for address, data in self.saved.items(): self.e.write(address, bytes(data))

def ppc(e, case):
    s = Side(e, case)
    try:
        states = [s.state()]
        for op, a in case["ops"]:
            s.op(op, a); states.append(s.state())
        heap = [sum(kind == k for kind, _, _ in e.alloc_log) for k in ("new", "new[]", "delete", "delete[]")]
        return dict(states=states, log=s.log.log, heap=heap)
    finally: s.close()

def host(lib, case):
    CL.host_begin(lib, {})
    begin, op_fn, state_fn = lib.tpw_lv_begin, lib.tpw_lv_op, lib.tpw_lv_state
    begin.argtypes, begin.restype = [C.POINTER(C.c_uint32)], None
    op_fn.argtypes, op_fn.restype = [C.c_int32, C.POINTER(C.c_uint32)], None
    state_fn.argtypes, state_fn.restype = [C.POINTER(C.c_uint32)], C.c_int32
    output = (C.c_uint32 * 1100)()
    def state(): return list(output[:state_fn(output)])
    begin((C.c_uint32 * len(case["seed"]))( *[w & MASK for w in case["seed"]]))
    states = [state()]
    for op, a in case["ops"]:
        op_fn(op, (C.c_uint32 * max(len(a), 1))( *[w & MASK for w in a])); states.append(state())
    heap = list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))
    return dict(states=states, log=CL.host_end(lib), heap=heap)

def seed(flags=0x410100, quality=3, renderer=1):
    return [flags, 0x3210fedc, fb(123.0), fb(987.0), 37, quality, renderer] + [fb(i * 0.125) for i in range(20)]
def case(ops, **kwargs): return dict(seed=seed(**kwargs), ops=ops)
def camera(position, target, up=0.0, right=0.0, down=0):
    return [fb(115.0), *[fb(v) for v in (*position, *target, up, right)], down]
def light(on=1, color=0x11223344, direction=(1., 2., 3.), follow=1):
    return [on, int(color is not None), color or 0, *[fb(v) for v in direction or (0., 0., 0.)], int(direction is not None), follow]
def edges():
    yield case([(17, [])])
    yield case([(1, []), (1, []), (2, []), (3, []), (5, [1, 2, 3]), (4, []), (4, []), (17, [])])
    for flags in (0, 1, 2, 0x10000002, 0x10000000, 0xffffffff):
        yield case([(1, []), (9, light()), (18, [flags]), (18, [flags]), (19, []), (18, [flags]), (4, []), (17, [])])
    for position, target in (((0, 0, 0), (0, 1, 0)), ((0, 0, 0), (0, -1, 0)), ((0, 0, 0), (0, 0, 0)),
        ((13.7, 87.3, -15.1), (187.0, -0.01, 832.5)), ((1e-8, 2e-8, -3e-8), (9e-8, -7e-8, 2e-8))):
        for down in (0, 1):
            yield case([(1, []), (9, light()), (6, camera(position, target, 7.3, -11.7, down)),
                       (7, []), (9, light(color=None, direction=None)), (4, []), (17, [])])
    for quality in (-1, 0, 1, 2, 5):
        for renderer in (0, 1, 3):
            for current, target in (([0, 0, 0], [0, 0, 0]), ([1, -1, 5], [13, 2, -10]),
                ([0x12345678, 0x7fffffff, 0x80000000], [0xfedcba98, 0x80000000, 0x7fffffff])):
                yield case([(13, current + target), (14, []), (14, []), (17, [])], quality=quality, renderer=renderer)
    for current, target in ((0, 0), (0, 8), (0, 16), (0, 8000), (0, 8008), (0, 65536),
        (70000, 0), (-1, 256), (0x7fffffff, 0x80000000), (0x80000000, 0x7fffffff)):
        yield case([(15, [current, target]), (16, []), (16, []), (17, [])], flags=MASK)
    for color in (0, MASK, 0xff0000ff, 0x00ff0000, 0x0000ff01):
        yield case([(1, []), (8, [1, color]), (9, light(color=color, direction=None)),
            (9, light(color=None, direction=(0., 0., 0.))), (9, light(on=0, follow=-1)),
            (10, []), (11, []), (12, [color, fb(-7.), fb(3.)]), (4, []), (17, [])])
def rnd(r):
    ops = [(1, [])]
    for _ in range(r.randint(1, 12)):
        op = r.choice([2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
        a = []
        if op == 5: a = [r.getrandbits(32) for _ in range(3)]
        if op == 6: a = camera([r.uniform(-1000, 1000) for _ in range(3)], [r.uniform(-1000, 1000) for _ in range(3)], r.uniform(-20, 20), r.uniform(-20, 20), r.randrange(2))
        if op == 8: a = [r.choice([0, 1, -1]), r.getrandbits(32)]
        if op == 9: a = light(r.randrange(2), r.getrandbits(32) if r.randrange(2) else None,
            [r.uniform(-10, 10) for _ in range(3)] if r.randrange(2) else None, r.choice([0, 1, -1]))
        if op == 12: a = [r.getrandbits(32), fb(r.uniform(-10, 500)), fb(r.uniform(-10, 500))]
        if op == 13: a = [r.getrandbits(32) for _ in range(6)]
        if op == 15: a = [r.getrandbits(32) for _ in range(2)]
        ops.append((op, a))
    ops += [(4, []), (17, [])]
    return case(ops, flags=r.getrandbits(32), quality=r.randrange(-1, 6), renderer=r.randrange(4))

TESTS = [FnTest("CLBLobbyView camera, lights, fog, sky and fade", ppc, host, edge=edges, rnd=rnd)]
