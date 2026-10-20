"""Weather scripts. Compare results, events and every scalar field after each
operation. Random and libm fakes are shared deterministic dependencies; texture entry
arrays have constructors, a first explicit Close, array destructors and labelled delete.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_Weather"
RAIN, SKY, SYSTEM, SETTINGS = 0x100b1628, 0x101661ec, 0x10165f80, 0x10165fc8
FN = {
    0: 0x10020458, 1: 0x1002051c, 2: 0x1002059c, 3: 0x100206f8, 4: 0x10020c64,
    5: 0x10020f98, 6: 0x10020fc8, 7: 0x10020ff8, 8: 0x1002102c, 9: 0x10021088,
    10: 0x100210c4, 11: 0x100214d0, 12: 0x1002151c, 13: 0x100233ac,
    14: 0x1002996c, 15: 0x1002ab70, 16: 0x1002b17c, 17: 0x1002b474,
    18: 0x1002a938, 19: 0x1002a9b4, 20: 0x1002aa08, 21: 0x1002aa6c,
    22: 0x1002aad8, 23: 0x1002b554, 24: 0x1002b584, 25: 0x10029e18, 26: 0x1002cac8,
    31: 0x10020d44, 32: 0x10020e28, 33: 0x10029e94, 34: 0x10029f88,
    35: 0x1002a1f8, 36: 0x1002a3a8, 37: 0x1002a42c, 38: 0x1002a5f4,
}
NAMES = [None, "weather:a.tga", "weather:b.tga", "weather:c.tga", "", "Park:LostKingdom"]


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    e.weather_filename = T.fake_function(e, lambda emu: emu.weather_side.filename(emu))


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xffffffff))


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        self.manager = e.alloc(4, bytes(4))
        self.L = CL.PpcLog({SKY: 1, self.manager: 2}, case.get("q"))
        e.weather_side = self
        e.cl = self.L
        self.random = case["seed"]
        self.random_calls = 0
        self.arrays = {}
        self.pixels = e.alloc(256 * 256 * 4, bytes(256 * 256 * 4))
        self.out = e.alloc(4, struct.pack(">I", 55))
        self.names = [0 if n is None else e.alloc(len(n) + 1, n.encode() + b"\0") for n in NAMES]

    def array(self, count, flags, width=32, height=32, pitch=32):
        e = self.e
        base = e.alloc(8 + count * 0x5c, bytes(8 + count * 0x5c))
        self.L.rec("new[]", (self.L.newlab(base),))
        block = base + 8
        self.L.newlab(block)
        self.arrays[block] = count
        e.write(block + 0xc, struct.pack(">hh", width, height))
        e.write(block + 0x30, struct.pack(">h", pitch))
        w32(e, block + 8, flags)
        return block

    def load(self, e):
        self.L.rec("LoadFile", (self.L.lab(e.r(3)), T.s32(e.u32(SKY + 0x21dc))),
                   (e.cstr(e.r(4)) if e.r(4) else "(null)",))
        count = self.L.ret("LoadFile", 1)
        flags = self.L.ret("Flags", 0xffffffff)
        e.set_r(3, self.array(count, flags) if count else 0)

    def filename(self, e):
        name = e.cstr(e.r(3))
        self.L.rec("FileName", (T.s32(e.r(4)),), (name,))
        result = ("resolved/" + name).encode() + b"\0"
        e.set_r(3, e.alloc(len(result), result) if self.L.ret("FileName", 1) else 0)

    def close(self, e):
        rain = e.u32(RAIN)
        slots = ([rain + x for x in (0x3348, 0x334c, 0x3350)] if rain else [None] * 3)
        slots += [SKY + x for x in (0x1fd0, 0x1fd4, 0x1fd8)]
        slots += [SKY + 0x1fdc + i * 32 for i in range(16)]
        owners = sum(1 << i for i, slot in enumerate(slots) if slot is not None and e.u32(slot) == e.r(3))
        self.L.rec("Entry.Close", (self.L.lab(e.r(3)), owners, T.s32(e.u32(SKY + 0x21dc))))
        e.set_r(3, self.L.ret("Entry.Close", 1))

    def rand(self, e):
        self.random = (self.random * 0x41c64e6d + 0x3039) & 0xffffffff
        self.random_calls += 1
        e.set_r(3, (self.random >> 16) & 0x7fff)

    def timer(self, e):
        self.L.rec("Timer")
        value = self.L.ret("Timer", 123456789) & 0xffffffffffffffff
        e.write(e.r(3), struct.pack(">Q", value))

    def new(self, size):
        if not self.L.ret("New", 1):
            return 0
        block = self.e.alloc(size, b"\xa5" * size)
        self.L.rec("new", (self.L.newlab(block),))
        return block

    def delete(self, e):
        block = e.r(3)
        self.L.rec("delete", (self.L.lab(block),))
        self.L.forget(block)

    def destroy_array(self, e):
        block = e.r(3)
        count = self.arrays[block]
        for _ in range(count):
            self.L.rec("Entry.dtor")
        self.L.rec("delete", (self.L.lab(block - 8),))
        self.L.forget(block - 8)

    def stubs(self):
        st = CL.Stubs()
        st.real = set(FN.values()) | {0x1004ba50}
        st.imp("rand", self.rand)
        st.imp("GetAbsolute__Q213SamsUtilities6UTimerFv", self.timer)
        st.imp("__nw__FUl", lambda e: e.set_r(3, self.new(e.r(3))))
        st.imp("__dl__FPv", self.delete)
        st.imp("__destroy_new_array", self.destroy_array)
        st.imp("__assertion_failed", lambda e: self.L.rec("assert", (T.s32(e.r(5)),),
                                                        (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        st.imp("strcat", lambda e: e.write(e.r(3), (e.cstr(e.r(3)) + e.cstr(e.r(4))).encode() + b"\0"))
        st.internal(0x10008904, CL.recorder("Error", "xii"))
        st.internal(0x10040f7c, self.close)
        st.internal(0x10041274, CL.recorder("Entry.GetData", "L", ret=lambda e, L: self.pixels))
        st.internal(0x10015f9c, self.load)
        return st

    def reset(self):
        e = self.e
        w32(e, RAIN, 0)
        e.write(SKY, b"\xa5" * 0x2f14)
        w32(e, SYSTEM + 0x1c, 3)
        T.call_ppc(e, FN[14], [SKY])
        for i in range(16):
            w32(e, SKY + 0x1fdc + i * 32, 0)
        w32(e, 0x100af1c8, self.manager)
        w32(e, 0x101735d0, e.weather_filename)

    def call(self, op, *args):
        return T.call_ppc(self.e, FN[op], list(args))[0]

    def op(self, op, a):
        e, u = self.e, self.e.u32
        rain = u(RAIN)
        fl = lambda bits: T.F(T.bits_f32(bits))
        if 3 <= op <= 13 and not rain:
            return -9
        if op in (0, 1):
            return T.s32(self.call(op))
        if op == 2:
            if rain:
                return -9
            block = self.new(0xa1cc)
            w32(e, RAIN, block)
            self.call(2, block)
        elif op in (3, 4):
            return T.s32(self.call(op, rain))
        elif op in (5, 7):
            self.call(op, rain, a[0])
        elif op in (6, 11):
            self.call(op, rain, fl(a[0]))
        elif op == 8:
            self.call(op, rain, *a)
        elif op in (9, 13):
            self.call(op, rain, *[fl(v) for v in a])
        elif op == 10:
            self.call(op, rain, fl(a[0]), fl(a[1]))
        elif op == 12:
            self.call(op, rain)
        elif op == 14:
            w32(e, SYSTEM + 0x1c, a[0])
            self.call(op, SKY)
        elif op in (15, 17):
            return T.s32(self.call(op, SKY))
        elif op in (16, 20):
            self.call(op, SKY)
        elif op == 18:
            self.call(op, SKY, *a)
        elif op == 19:
            self.call(op, SKY, fl(a[0]))
        elif op in (21, 22):
            self.call(op, SKY, *a)
        elif op == 23:
            return self.L.lab(self.call(op, SKY))
        elif op == 24:
            return int(self.call(op, SKY) == SKY + 0x1fa0)
        elif op == 25:
            self.call(op, SKY, 0)
        elif op == 26:
            self.call(op)
        elif op == 27:
            slots = [rain + 0x3348, rain + 0x334c, rain + 0x3350, SKY + 0x1fd0, SKY + 0x1fd4, SKY + 0x1fd8]
            if a[0] < 3 and not rain:
                return -9
            slot = slots[a[0]] if a[0] < 6 else SKY + 0x1fdc + (a[0] - 6) * 32
            if u(slot):
                return -9
            block = self.array(a[1], a[5], a[2], a[3], a[4])
            w32(e, slot, block)
            e.write(block + 0xc, struct.pack(">hh", a[2], a[3]))
            e.write(block + 0x30, struct.pack(">h", a[4]))
            w32(e, block + 8, a[5])
        elif op == 28:
            w32(e, SETTINGS + 4, a[0])
            if a[0] in (2, 4):
                values = [(a[1] + i * 40503) & 0xffff for i in range(256 * 256)]
                e.write(self.pixels, struct.pack(">65536H", *values))
            else:
                values = [(a[1] + i * 2654435761) & 0xffffffff for i in range(256 * 256)]
                e.write(self.pixels, struct.pack(">65536I", *values))
        elif op == 29:
            w32(e, SKY + 0x21e0, a[0])
            w32(e, SKY + 0x21e4, a[1])
        elif op == 30:
            block = self.new(0x2f14)
            self.call(14, block)
            self.call(25, block, 1)
        elif op == 31:
            return T.s32(self.call(op, rain, a[0], self.names[a[1]]))
        elif op == 32:
            return T.s32(self.call(op, rain))
        elif op == 33:
            return T.s32(self.call(op, SKY, SKY + 0x1fd0 + a[0] * 4, self.names[a[1]]))
        elif op == 34:
            return T.s32(self.call(op, SKY, *[self.names[i] for i in a]))
        elif op == 35:
            return T.s32(self.call(op, SKY, self.out, self.names[a[0]], *a[1:4], fl(a[4]), *a[5:7]))
        elif op == 36:
            self.call(op, SKY, a[0])
        elif op == 37:
            ptr = 0x777 if a[0] == -1 else 0 if a[0] == -2 else u(SKY + 0x1fdc + a[0] * 32)
            return T.s32(self.call(op, SKY, ptr))
        elif op == 38:
            return T.s32(self.call(op, SKY, self.names[a[0]]))
        elif op == 39:
            w32(e, SKY + 0x21dc, a[0])
        elif op == 40:
            for i in range(16):
                w32(e, SKY + 0x1fdc + i * 32 + 0x1c, a[0] + i * 17)
        else:
            return -99
        return 0

    def state(self):
        e, u, L = self.e, self.e.u32, self.L
        out = [self.random, self.random_calls, L.lab(u(RAIN)) & 0xffffffff]
        out.append(L.lab(u(self.out)) & 0xffffffff)

        def words(a, n):
            out.extend(struct.unpack(f">{n}I", e.read(a, n * 4)))

        def entry(a):
            p = u(a)
            out.append(L.lab(p) & 0xffffffff)
            if p:
                out.append(u(p + 8))
                out.extend(struct.unpack(">HH", e.read(p + 0xc, 4)))
                out.append(struct.unpack(">H", e.read(p + 0x30, 2))[0])

        rain = u(RAIN)
        if rain:
            words(rain, 0x3348 // 4)
            for offset in (0x3348, 0x334c, 0x3350):
                entry(rain + offset)
            words(rain + 0x3354, (0xa1cc - 0x3354) // 4)
        words(SKY, 0x1fd0 // 4)
        for offset in (0x1fd0, 0x1fd4, 0x1fd8):
            entry(SKY + offset)
        for i in range(16):
            entry(SKY + 0x1fdc + i * 0x20)
            words(SKY + 0x1fdc + i * 0x20 + 4, 7)
        words(SKY + 0x21dc, 8)
        out.extend(struct.unpack(">1638H", e.read(SKY + 0x21fc, 1638 * 2)))
        words(SKY + 0x2ec8, 19)
        return tuple(out)


def ppc(e, case):
    side = PpcSide(e, case)
    saved = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks = [], [], []
        for op, args in case["script"]:
            results.append(side.op(op, args))
            states.append(side.state())
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(results=results, states=states, marks=marks, log=side.L.log)


def host(lib, case):
    reset = lib.tpw_weather_reset
    reset.argtypes = [C.c_uint32]
    reset.restype = None
    op_fn = lib.tpw_weather_op
    op_fn.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    op_fn.restype = C.c_int64
    state = lib.tpw_weather_state
    state.argtypes = [C.POINTER(C.c_uint32)]
    state.restype = C.c_int32
    lib.tpw_weather_sky.restype = C.c_void_p
    lib.tpw_weather_manager.restype = C.c_void_p
    reset(case["seed"])
    CL.host_begin(lib, {1: lib.tpw_weather_sky(), 2: lib.tpw_weather_manager()}, case.get("q"))
    results, states, marks = [], [], []
    buffer = (C.c_uint32 * 20000)()
    try:
        for op, args in case["script"]:
            results.append(op_fn(op, (C.c_int32 * 16)(*[T.s32(v) for v in args])))
            count = state(buffer)
            states.append(tuple(buffer[:count]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, states=states, marks=marks, log=log)


def fb(x):
    return T.f32_bits(x)


def case(script, seed=1, q=None):
    return dict(seed=seed, script=script, q=q or {})


def edge_rain():
    yield case([(0, []), (0, []), (3, []), (4, []), (1, []), (1, [])])
    yield case([(0, []), (0, []), (1, [])], q={"New": [0, 1]})
    for timer in (0, 999, 1000, 1001, 0xffffffff, 0x123456789abcdef):
        yield case([(2, []), (3, []), (1, [])], q={"Timer": [timer]})
    for kind in (0, 1, 2, 3, 4, 7, -1, -2):
        yield case([(2, []), (5, [kind]), (11, [fb(3.5)]), (12, []), (1, [])])
    for speed in (0.0, -0.0, 0.3, -2.0, 1e-30, 200.0):
        yield case([(2, []), (10, [fb(37.5), fb(speed)]), (1, [])])
    for mask in range(8):
        script = [(2, [])]
        script += [(27, [i, 1 + i, 32, 32, 32, 0x345]) for i in range(3) if mask & (1 << i)]
        yield case(script + [(4, []), (4, []), (1, [])])


def rain_case(r):
    script = [(2, [])]
    for op in range(5, 14):
        if op in (5, 7):
            args = [r.choice([0, 1, 2, 3, -1, r.randrange(-3000, 3000)])]
        elif op in (8, 9, 13):
            args = [fb(r.uniform(-100, 100)) for _ in range(3)]
        elif op == 10:
            args = [fb(r.uniform(-1000, 1000)), fb(r.uniform(-100, 100))]
        elif op == 12:
            args = []
        else:
            args = [fb(r.uniform(-100, 100))]
        script.append((op, args))
    return case(script + [(1, [])], r.getrandbits(32))


def edge_sky():
    yield case([(14, [4]), (14, [3]), (26, []), (15, []), (24, []), (23, []), (25, []), (17, []), (30, [])])
    for layers in (-1, 0, 1, 2, 3, 4, 5, 0x7fffffff, -0x80000000):
        yield case([(22, [layers])])
    for y in (-1000.0, 0.0, 143.0, 144.0, 145.0, 300.0, 1000.0):
        yield case([(18, [fb(1.3), fb(y), fb(-2.9)]), (19, [fb(0.0)]), (20, [])])
    for mask in range(8):
        script = [(27, [3 + i, i + 1, 32, 32, 32, 0x789]) for i in range(3) if mask & (1 << i)]
        yield case(script + [(23, []), (25, []), (17, [])])


def sky_case(r):
    script = [(14, [r.choice([0, 1, 3, 4])]), (29, [r.randrange(-3000, 3000), r.randrange(-3000, 3000)]),
              (18, [fb(r.uniform(-1000, 1000)) for _ in range(3)]),
              (21, [r.randrange(4)] + [fb(r.uniform(-2, 2)) for _ in range(3)]),
              (22, [r.randrange(-1, 6)]), (19, [fb(r.uniform(-1000, 1000))]), (24, []), (23, []),
              (20, []), (25, []), (26, []), (17, [])]
    return case(script, r.getrandbits(32))


def colors_case(r, format=None):
    return case([(27, [4, r.randrange(1, 4), r.choice([16, 32, 64, 128, 256]), r.choice([16, 32, 64, 128, 256]),
                      256, r.getrandbits(32)]), (28, [r.randrange(0, 8) if format is None else format, r.getrandbits(32)]),
                 (16, []), (17, []), (16, [])], r.getrandbits(32))


def edge_colors():
    yield case([(16, [])])
    r = random.Random(171)
    for fmt in (-1, 0, 1, 2, 3, 4, 5, 6, 7):
        yield colors_case(r, fmt)


def edge_rain_load():
    for kind in (1, 2, 3):
        yield case([(2, []), (31, [kind, 0]), (31, [kind, 1]), (31, [kind, 2]),
                    (31, [kind, 3]), (1, [])], q={"LoadFile": [0, 2], "Flags": [0x100, 0x456]})
    for mask in range(8):
        script = [(2, []), (12, [])]
        script += [(27, [i, 2, 32, 32, 32, 0x101]) for i in range(3) if mask & (1 << i)]
        yield case(script + [(32, []), (32, []), (1, [])],
                   q={"FileName": [0, 1, 0, 1, 0, 1], "LoadFile": [0, 2, 1]})


def rain_load_case(r):
    script = [(2, []), (12, [])]
    for _ in range(8):
        script += [(31, [r.randrange(1, 4), r.randrange(5)]), (32, [])]
    return case(script + [(1, [])], r.getrandbits(32),
                {"FileName": [r.randrange(2) for _ in range(24)],
                 "LoadFile": [r.randrange(4) for _ in range(24)],
                 "Flags": [r.getrandbits(32) for _ in range(24)]})


def edge_sky_load():
    for mask in range(8):
        script = [(28, [3, 0x123])]
        script += [(27, [3 + i, 2, 32, 32, 32, 0x101]) for i in range(3) if mask & (1 << i)]
        yield case(script + [(34, [1, 2, 3]), (38, [5]), (17, [])],
                   q={"LoadFile": [(mask >> i) & 1 for i in range(3)] * 2,
                      "FileName": [0, 1, 0]})
    for slot in range(3):
        yield case([(33, [slot, 0]), (33, [slot, 4]), (33, [slot, 1]), (17, [])],
                   q={"LoadFile": [2, 0, 3], "Flags": [0xffffffff, 0, 0x100]})


def sky_load_case(r):
    script = [(28, [r.choice([2, 3, 4, 5, 6]), r.getrandbits(32)]),
              (34, [r.randrange(5) for _ in range(3)]),
              (33, [r.randrange(3), r.randrange(5)]), (38, [r.choice([4, 5])]),
              (34, [1, 2, 3]), (17, [])]
    return case(script, r.getrandbits(32),
                {"LoadFile": [r.randrange(4) for _ in range(10)],
                 "Flags": [r.getrandbits(32) for _ in range(10)],
                 "FileName": [r.randrange(2) for _ in range(3)]})


def add_args(r):
    return [r.randrange(5)] + [fb(r.uniform(-200, 200)) for _ in range(4)] + [r.getrandbits(32), r.getrandbits(32)]


def edge_adds():
    r = random.Random(173)
    yield case([(37, [-1]), (36, [0])])
    yield case([(35, add_args(r)), (37, [0]), (37, [-1])])
    yield case([(35, add_args(r)), (37, [-2])], q={"LoadFile": [0]})
    script = [(40, [0x12345678])] + [(35, add_args(r)) for _ in range(17)]
    yield case(script + [(37, [-1]), (37, [0]), (36, [1]), (36, [1])])
    yield case([(40, [0x23456789]), (39, [3]), (27, [9, 2, 32, 32, 32, 0x100]), (37, [3])])
    yield case([(27, [6, 3, 32, 32, 32, 0x100]), (35, add_args(r)), (36, [0])],
               q={"LoadFile": [0]})


def adds_case(r):
    script = [(40, [r.getrandbits(32)])] + [(35, add_args(r)) for _ in range(r.randrange(3, 18))]
    script += [(36, [0]), (37, [1]), (37, [-1]), (37, [-2])]
    return case(script, r.getrandbits(32),
                {"LoadFile": [r.randrange(4) for _ in range(17)],
                 "Flags": [r.getrandbits(32) for _ in range(17)]})


TESTS = [
    FnTest("CRain construction, tables, lifecycle and setters", ppc, host, edge=edge_rain, rnd=rain_case),
    FnTest("CSky construction, mesh, lifecycle and setters", ppc, host, edge=edge_sky, rnd=sky_case),
    FnTest("CSky color sampling", ppc, host, edge=edge_colors, rnd=colors_case),
    FnTest("CRain random tables and clock", ppc, host,
           rnd=lambda r: case([(0, []), (1, [])], r.getrandbits(32), {"Timer": [r.getrandbits(63)]})),
    FnTest("CRain loading and game paths", ppc, host, edge=edge_rain_load, rnd=rain_load_case),
    FnTest("CSky loading and color lifetime", ppc, host, edge=edge_sky_load, rnd=sky_load_case),
    FnTest("CSky added objects and removal", ppc, host, edge=edge_adds, rnd=adds_case),
]
