"""Unit v5041, the DirectDraw/palette part of WinScrn.cpp, against libtpw_unit_CreateDirectDrawObject.dylib
(tests/unit_CreateDirectDrawObject.cpp), main app ("SimTheme Park"), src/bullfrog/WinScrn.cpp:
  CreateDirectDrawObject 0x1006a514, EnumModesCallback 0x1006a45c, LbScreen_GetPaletteEntries 0x1006a304,
  LbScreen_SetPaletteEntries 0x1006a220, LbScreen_SwapClear 0x1006a198, __sinit_WinScrn_cpp 0x1006a968

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit are fakes on both sides that record
their arguments: DirectDrawCreate, LbApp_SetHWnd, operator new (the size is the Mac's 0x14), LbList_Insert (the node's three words),
the two palette COM calls (reached through __ptr_glue on the PowerPC side, told apart by the vtable slot in r12), LbScreen_Swap,
LbSurface_ClearRect, TbRenderArea::TbRenderArea and __register_global_object.

The world of a case: the window handle, the DirectDraw palette object (or none), the screen surface pointer, the 1024 bytes of the global
palette, a data buffer, the three static surfaces and the static handler (pre-filled so that the initialiser's zeroing shows). What is
compared: the answer, the global palette, the buffer, the surfaces' colour, mF20 and DD surface flag, the handler's fields and the event log.

No fused operations. Not tested: a start and count past the 256 palette entries (the original copies past the table).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateDirectDrawObject"
PROGRAM = "bullfrog symbols shared"
MASK = 0xFFFFFFFF

FN_DDC, FN_ENUM, FN_GET, FN_SET, FN_SWAP, FN_SINIT = 0x1006A514, 0x1006A45C, 0x1006A304, 0x1006A220, 0x1006A198, 0x1006A968
SIZES = {FN_DDC: 112, FN_ENUM: 124, FN_GET: 180, FN_SET: 156, FN_SWAP: 84, FN_SINIT: 252}
OWN = set(SIZES)

CALLEE_DDC, CALLEE_HWND, CALLEE_NEW, CALLEE_INSERT = 0x1006BE2C, 0x10013FE4, 0x1006B2BC, 0x10014CC4
CALLEE_GLUE, CALLEE_SWAP, CALLEE_CLEAR, CALLEE_RACTOR, CALLEE_REG = 0x1006C12C, 0x10069D4C, 0x10067214, 0x10016630, 0x1006B82C
SLOT_GET, SLOT_SET = 0x0AAA0001, 0x0AAA0002
VTABLE = 0x10073A84

ADDR = dict(hwnd=0x10075238, pal=0x10075230, screen=0x10075228, lbpal=0x10074070,
            handler=0x100750FC, node=0x1007510C, dtor=0x10070E1C,
            areaA=0x10075160, areaB=0x100751B0, areaC=0x10075200)
SURF = {0: 0x10075138, 1: 0x10075188, 2: 0x100751D8}
HOST_LAYOUT = [4, 1024]

STATE = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_ddc(e):
    L = e.cl
    L.rec("DirectDrawCreate", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5)))
    e.set_r(3, L.ret("DirectDrawCreate") & MASK)


def st_hwnd(e):
    e.cl.rec("LbApp_SetHWnd", (e.r(3),))


def st_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("new", (size,))
    if L.ret("new") == 0:
        e.set_r(3, 0)
    else:
        a = e.alloc(size)
        STATE["new"] = a
        e.set_r(3, a)


def st_insert(e):
    L = e.cl
    node = e.r(5)
    L.rec("LbList_Insert", (L.lab(e.r(3)), L.lab(e.r(4)), int(node == STATE.get("new")), e.u32(node + 8), e.u32(node + 12), e.u32(node + 16)))


def st_glue(e):
    L = e.cl
    slot = e.r(12)
    if slot == SLOT_GET:
        entries = e.r(7)
        L.rec("GetEntries", (L.lab(e.r(3)), e.r(4), e.r(5), e.r(6), entries - ADDR["lbpal"]))
        if L.ret("fill") != 0:
            start, count = e.r(5), e.r(6)
            e.write(entries, bytes((k * 37 + start + 5) & 0xFF for k in range(count * 4)))
        e.set_r(3, L.ret("GetEntries") & MASK)
    elif slot == SLOT_SET:
        entries, count = e.r(7), e.r(6)
        data = e.read(entries, count * 4)
        L.rec("SetEntries", (L.lab(e.r(3)), e.r(4), e.r(5), count, L.lab(entries), sum((i + 1) * b for i, b in enumerate(data)) & MASK))
        e.set_r(3, 0)
    else:
        raise AssertionError("unknown ptr_glue slot %x" % slot)


def st_swap(e):
    e.cl.rec("LbScreen_Swap", (e.r(4),))


def st_clear(e):
    L = e.cl
    L.rec("LbSurface_ClearRect", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(6), e.r(7) >> 24, e.r(8)))


def st_ractor(e):
    e.cl.rec("TbRenderArea", (e.cl.lab(e.r(3)),))


def st_reg(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


R2 = 0x1006EDA0   # bullfrog's TOC base (0x1006f20c - 0x46c); the code reloads r2 from 0x14(r1) after every call


def with_toc(fn):
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", R2))   # what the glue's save of r2 leaves
        fn(e)
    return wrapped


for _a, _f in ((CALLEE_DDC, st_ddc), (CALLEE_HWND, st_hwnd), (CALLEE_NEW, st_new), (CALLEE_INSERT, st_insert), (CALLEE_GLUE, st_glue),
               (CALLEE_SWAP, st_swap), (CALLEE_CLEAR, st_clear), (CALLEE_RACTOR, st_ractor), (CALLEE_REG, st_reg)):
    STUBS.internal(_a, with_toc(_f))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        STATE["new"] = None
        self.list = e.alloc(12)
        self.prev = e.alloc(8)
        self.words(self.list, [0, 0, self.prev])
        self.screen = e.alloc(0x50)
        self.palobj = e.alloc(8)
        self.vt = e.alloc(0x40)
        self.words(self.palobj, [self.vt])
        self.words(self.vt + 0x1C, [SLOT_GET])
        self.words(self.vt + 0x24, [SLOT_SET])
        self.guid = e.alloc(16)
        self.out = e.alloc(8)
        self.buf = e.alloc(1024)
        fixed = {self.list: 1, self.prev: 2, self.screen: 4, self.palobj: 5, self.guid: 6, self.out: 7, self.buf: 8,
                 ADDR["handler"]: 9, ADDR["node"]: 10, ADDR["dtor"]: 11, ADDR["areaA"]: 12, ADDR["areaB"]: 13, ADDR["areaC"]: 14}
        e.set_r(2, R2)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_pal(self, on):
        self.words(ADDR["pal"], [self.palobj if on else 0])

    def set_screen(self):
        self.words(ADDR["screen"], [self.screen])

    def set_surf(self, i, w):
        b = SURF[i]
        self.write(b + 0x10, bytes(x & 0xFF for x in w[:5]))
        self.words(b + 0x20, [w[5], 0x1000 if w[6] else 0])

    def get_surf(self, i):
        b = SURF[i]
        return list(self.read(b + 0x10, 5)) + [self.rd_word(b + 0x20), int(self.rd_word(b + 0x24) != 0)]

    def set_handler(self, w):
        b = ADDR["handler"]
        self.write(b, bytes([w[0] & 0xFF]))
        self.words(b + 4, [VTABLE if w[1] else 0x1234])
        self.write(b + 8, struct.pack(">HHH", w[2] & 0xFFFF, w[3] & 0xFFFF, w[4] & 0xFFFF))

    def get_handler(self):
        b = ADDR["handler"]
        return [self.read(b, 1)[0], int(self.rd_word(b + 4) == VTABLE)] + list(struct.unpack(">HHH", self.read(b + 8, 6)))

    def call_cddo(self, case):
        res = self.e.alloc(4)
        T.call_ppc(self.e, FN_DDC, [res, 0 if case["guid_null"] else self.guid, 0 if case["out_null"] else self.out, case["unused"]])
        return s32(self.rd_word(res))

    def call_enum(self, h, w, bits):
        d = self.e.alloc(0x6C)
        self.words(d + 8, [h, w])
        self.words(d + 0x54, [bits])
        r3, _, _ = T.call_ppc(self.e, FN_ENUM, [d, self.list])
        return r3 & 0xFF

    def call_get(self, start, count):
        r3, _, _ = T.call_ppc(self.e, FN_GET, [self.buf, start, count])
        return int(r3 & MASK == self.buf)

    def call_set(self, start, count):
        T.call_ppc(self.e, FN_SET, [self.buf, start, count])
        return 0

    def call_swap(self, argb, index):
        T.call_ppc(self.e, FN_SWAP, [argb, (index & 0xFF) << 24])
        return 0

    def call_sinit(self):
        T.call_ppc(self.e, FN_SINIT, [])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.screen = lib.tpw_ws_alloc(0x50)
        self.guid = lib.tpw_ws_alloc(16)
        self.out = lib.tpw_ws_alloc(8)
        self.buf = lib.tpw_ws_alloc(1024)
        fixed = {1: self.g("list"), 2: self.g("prev"), 4: self.screen, 5: self.g("palobj"), 6: self.guid, 7: self.out, 8: self.buf,
                 9: self.g("handler"), 10: self.g("node"), 11: self.g("dtor"), 12: self.g("areaA"), 13: self.g("areaB"),
                 14: self.g("areaC")}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ws_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_pal(self, on):
        self.lib.tpw_ws_set_pal(1 if on else 0)

    def set_screen(self):
        self.write(self.g("screen"), struct.pack("<Q", self.screen))

    def set_surf(self, i, w):
        self.lib.tpw_ws_set_surf(i, (C.c_uint32 * 7)(*[x & MASK for x in w]))

    def get_surf(self, i):
        out = (C.c_uint32 * 7)()
        self.lib.tpw_ws_get_surf(i, out)
        return list(out)

    def set_handler(self, w):
        self.lib.tpw_ws_set_handler((C.c_uint32 * 5)(*[w[0] & 0xFF, w[1], w[2] & 0xFFFF, w[3] & 0xFFFF, w[4] & 0xFFFF]))

    def get_handler(self):
        out = (C.c_uint32 * 5)()
        self.lib.tpw_ws_get_handler(out)
        return list(out)

    def call_cddo(self, case):
        return self.lib.tpw_ws_create(0 if case["guid_null"] else self.guid, 0 if case["out_null"] else self.out, case["unused"])

    def call_enum(self, h, w, bits):
        return self.lib.tpw_ws_enum(h, w, bits) & 0xFF

    def call_get(self, start, count):
        return int(self.lib.tpw_ws_get(self.buf, start, count) == self.buf)

    def call_set(self, start, count):
        self.lib.tpw_ws_set(self.buf, start, count)
        return 0

    def call_swap(self, argb, index):
        self.lib.tpw_ws_swapclear(argb, index & 0xFF)
        return 0

    def call_sinit(self):
        self.lib.tpw_ws_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ws_addr.restype, lib.tpw_ws_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ws_alloc.restype, lib.tpw_ws_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ws_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ws_set_pal.argtypes = [C.c_uint32]
    lib.tpw_ws_set_surf.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
    lib.tpw_ws_get_surf.argtypes = [C.c_uint32, C.POINTER(C.c_uint32)]
    lib.tpw_ws_set_handler.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_ws_get_handler.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_ws_create.restype, lib.tpw_ws_create.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_ws_enum.restype, lib.tpw_ws_enum.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_ws_get.restype, lib.tpw_ws_get.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_ws_set.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_ws_swapclear.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ws_sinit.argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_ws_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("hwnd"), [case["hwnd"]])
    s.write(s.g("lbpal"), case["palette"])
    s.write(s.buf, case["buf"])
    s.set_pal(case["pal_on"])
    s.set_screen()
    for i in range(3):
        s.set_surf(i, case["surf"][i])
    s.set_handler(case["handler"])


def execute(s, case):
    op = case["op"]
    if op == "cddo":
        return s.call_cddo(case)
    if op == "enum":
        return s.call_enum(*case["args"])
    if op == "get":
        return s.call_get(*case["args"])
    if op == "set":
        return s.call_set(*case["args"])
    if op == "swap":
        return s.call_swap(*case["args"])
    return s.call_sinit()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "palette": s.read(s.g("lbpal"), 1024), "buf": s.read(s.buf, 1024),
            "surf": [s.get_surf(i) for i in range(3)], "handler": s.get_handler()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

WORDS = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678]


def rb(r, n):
    return bytes(r.randrange(256) for _ in range(n))


def base(r, op):
    surf = [[r.randrange(256) for _ in range(5)] + [r.choice([0, 7, 0xFFFFFFFF]), r.choice([0, 1])] for _ in range(3)]
    return {"op": op, "hwnd": r.choice([0, 0, r.randrange(1, 1 << 32)]), "palette": rb(r, 1024), "buf": rb(r, 1024),
            "pal_on": r.choice([0, 1]), "surf": surf,
            "handler": [r.randrange(1, 256), r.choice([0, 1]), r.randrange(1, 65536), r.randrange(1, 65536), r.randrange(1, 65536)],
            "guid_null": r.choice([0, 1]), "out_null": r.choice([0, 1]), "unused": r.choice([0, r.randrange(1 << 32)]),
            "queues": {"DirectDrawCreate": [r.choice([0, 1, -1])], "new": [r.choice([0, 1, 1])],
                       "GetEntries": [r.choice([0, 1, -1])], "fill": [r.choice([0, 1])]}}


def cddo_case(r, res=None, hwnd=None):
    c = base(r, "cddo")
    if res is not None:
        c["queues"]["DirectDrawCreate"] = [res]
    if hwnd is not None:
        c["hwnd"] = hwnd
    return c


def cddo_edges():
    r = random.Random(11)
    for res in (0, 1, -1):
        for hwnd in (0, 0x12345678):
            yield cddo_case(r, res, hwnd)


def cddo_random(r):
    return cddo_case(r)


def enum_case(r, new=None, args=None):
    c = base(r, "enum")
    c["args"] = args or [r.choice(WORDS + [r.randrange(1 << 32)]) for _ in range(3)]
    if new is not None:
        c["queues"]["new"] = [new]
    return c


def enum_edges():
    r = random.Random(12)
    yield enum_case(r, 0)
    yield enum_case(r, 1, [480, 640, 8])
    yield enum_case(r, 1, [0, 0, 0])
    yield enum_case(r, 1, [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF])


def enum_random(r):
    return enum_case(r)


def range_args(r):
    start = r.choice([0, 255, r.randrange(256)])
    count = r.choice([0, 256 - start, r.randrange(257 - start)])
    return [start, count]


def pal_case(r, op, on=None, args=None, ret=None, fill=None, dd=None):
    c = base(r, op)
    c["args"] = args if args is not None else range_args(r)
    if on is not None:
        c["pal_on"] = on
    if ret is not None:
        c["queues"]["GetEntries"] = [ret]
    if fill is not None:
        c["queues"]["fill"] = [fill]
    if dd is not None:
        c["surf"][1][6] = dd
    return c


def get_edges():
    r = random.Random(13)
    for on in (0, 1):
        for ret in (0, 1, -1):
            for fill in (0, 1):
                yield pal_case(r, "get", on, None, ret, fill)
    yield pal_case(r, "get", 1, [0, 256], 0, 1)
    yield pal_case(r, "get", 0, [255, 1])
    yield pal_case(r, "get", 1, [10, 0], 0, 1)


def get_random(r):
    return pal_case(r, "get")


def set_edges():
    r = random.Random(14)
    for on in (0, 1):
        for dd in (0, 1):
            yield pal_case(r, "set", on, None, None, None, dd)
    yield pal_case(r, "set", 1, [0, 256], None, None, 1)
    yield pal_case(r, "set", 1, [255, 1], None, None, 1)
    yield pal_case(r, "set", 1, [3, 0], None, None, 1)


def set_random(r):
    return pal_case(r, "set")


def swap_case(r, args=None):
    c = base(r, "swap")
    c["args"] = args or [r.choice(WORDS + [r.randrange(1 << 32)]), r.choice([0, 255, r.randrange(256)])]
    return c


def swap_edges():
    r = random.Random(15)
    yield swap_case(r, [0, 0])
    yield swap_case(r, [0xFFFFFFFF, 255])
    yield swap_case(r, [0x80402010, 1])


def swap_random(r):
    return swap_case(r)


def sinit_edges():
    r = random.Random(16)
    for _ in range(4):
        yield base(r, "sinit")


def sinit_random(r):
    return base(r, "sinit")


TESTS = [
    FnTest("CreateDirectDrawObject", ppc, host, edge=cddo_edges, rnd=cddo_random),
    FnTest("EnumModesCallback", ppc, host, edge=enum_edges, rnd=enum_random),
    FnTest("LbScreen_GetPaletteEntries", ppc, host, edge=get_edges, rnd=get_random),
    FnTest("LbScreen_SetPaletteEntries", ppc, host, edge=set_edges, rnd=set_random),
    FnTest("LbScreen_SwapClear", ppc, host, edge=swap_edges, rnd=swap_random),
    FnTest("__sinit_WinScrn_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
]
