"""Unit v5284, the bullfrog LbSCBack.cpp helpers, against libtpw_unit_LbApp_GetWindowsScreenMode.dylib
(tests/unit_LbApp_GetWindowsScreenMode.cpp), src/LbFile_LbApp_GetWindowsScreenMode.cpp:
  LbApp_ProcessWindowMessages 0x10014018, LbApp_EnableWindowsScreenSaver 0x100141a8, LbApp_SetWindowName 0x10014278,
  LbApp_GetWindowsScreenMode 0x10014374, __sinit_LbSCBack_cpp 0x10014b4c

The NS_MacDoze calls, __construct_array and __register_global_object are fakes on both sides that record their arguments and answer from
the case's queues; strncpy and operator new behave as the C library on both sides. Compared: the answers (result words), the window name
buffer, the screen mode block, the global object and the call log (the message pump logs the first 40, the last messages and every 250th,
each with its index, so the count of messages is compared).
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbApp_GetWindowsScreenMode"
MASK = 0xFFFFFFFF

FN_PUMP, FN_SAVER, FN_NAME, FN_MODE, FN_SINIT = 0x10014018, 0x100141A8, 0x10014278, 0x10014374, 0x10014B4C
SIZES = {FN_PUMP: 116, FN_SAVER: 88, FN_NAME: 100, FN_MODE: 156, FN_SINIT: 124}
OWN = set(SIZES)

G_CREATEDC, G_GETCAPS, G_DELETEDC, G_DISPATCH, G_TRANSLATE, G_PEEK = 0x1006B94C, 0x1006B964, 0x1006B97C, 0x1006B9C4, 0x1006B9AC, 0x1006B9DC
G_SPI, G_INITCS, G_CONSTRUCT_ARRAY, G_REGISTER, G_NEW, G_STRNCPY = 0x1006B994, 0x1006C09C, 0x1006C084, 0x1006B82C, 0x1006B2BC, 0x1006B37C

ADDR = dict(name=0x10072600, obj=0x10073E0C, node=0x10073E70, ctor=0x100700CC, dtor=0x100700C4, cbdtor=0x100700BC)
HOST_LAYOUT = [0x60, 0x68, 12, 4, 64]


def s32(v):
    return T.s32(v)


def logged(n):
    return n <= 40 or n >= 1995 or n % 250 == 0


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_createdc(e):
    L = e.cl
    L.rec("CreateDC", (L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6))), (e.cstr(e.r(3)),))
    e.set_r(3, L.ret("CreateDC") & MASK)


def stub_getcaps(e):
    L = e.cl
    L.rec("GetDeviceCaps", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("GetDeviceCaps") & MASK)


def stub_deletedc(e):
    L = e.cl
    L.rec("DeleteDC", (L.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_peek(e):
    L = e.cl
    n = L.peekN = getattr(L, "peekN", 0) + 1
    e.write(e.r(3), struct.pack(">I", n))
    if logged(n):
        L.rec("PeekMessage", (n, L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))
    e.set_r(3, 1 if n <= L.ret("PeekLimit") else 0)


def stub_translate(e):
    n = e.u32(e.r(3))
    if logged(n):
        e.cl.rec("TranslateMessage", (n,))
    e.set_r(3, 0)


def stub_dispatch(e):
    n = e.u32(e.r(3))
    if logged(n):
        e.cl.rec("DispatchMessage", (n,))
    e.set_r(3, 0)


def stub_spi(e):
    L = e.cl
    L.rec("SystemParametersInfoA", (e.r(3) & MASK, e.r(4) & MASK, L.lab(e.r(5)), e.r(6) & MASK))
    e.set_r(3, L.ret("SystemParametersInfoA") & MASK)


def stub_initcs(e):
    L = e.cl
    L.rec("InitializeCriticalSection", (L.lab(e.r(3)),))


def stub_construct_array(e):
    L = e.cl
    L.rec("__construct_array", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7))))


def stub_register(e):
    L = e.cl
    L.rec("__register_global_object", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


def stub_new(e):
    e.set_r(3, e.alloc(max(e.r(3) & MASK, 4)))


def stub_strncpy(e):
    dst, src, n = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    data = bytearray()
    while len(data) < n:
        b = e.read(src + len(data), 1)
        if b == b"\0":
            break
        data += b
    data += b"\0" * (n - len(data))
    e.write(dst, bytes(data))
    e.set_r(3, dst)


for _a, _f in ((G_CREATEDC, stub_createdc), (G_GETCAPS, stub_getcaps), (G_DELETEDC, stub_deletedc), (G_PEEK, stub_peek),
               (G_TRANSLATE, stub_translate), (G_DISPATCH, stub_dispatch), (G_SPI, stub_spi), (G_INITCS, stub_initcs),
               (G_CONSTRUCT_ARRAY, stub_construct_array), (G_REGISTER, stub_register), (G_NEW, stub_new), (G_STRNCPY, stub_strncpy)):
    def _wrapped(e, _f=_f):
        # the import glue saves the TOC pointer at 20(r1); the caller reloads r2 from there after the call
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        _f(e)
    STUBS.internal(_a, _wrapped)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    ptrsz = 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({0x10073E0C: 1, 0x10073E70: 2, 0x100700CC: 3, 0x100700C4: 4, 0x100700BC: 5}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_setname(self, name_addr):
        buf = self.alloc(4)
        self.words(buf, [0x55555555])
        T.call_ppc(self.e, FN_NAME, [buf, name_addr])
        return self.rd_words(buf, 1)[0]

    def call_saver(self, enable):
        buf = self.alloc(4)
        self.words(buf, [0x55555555])
        T.call_ppc(self.e, FN_SAVER, [buf, enable & MASK])
        return self.rd_words(buf, 1)[0]

    def call_pump(self):
        T.call_ppc(self.e, FN_PUMP, [])
        return 0

    def call_mode(self, addr):
        T.call_ppc(self.e, FN_MODE, [addr])
        return 0

    def call_sinit(self):
        T.call_ppc(self.e, FN_SINIT, [])
        return 0


class Host(Side):
    fmt = "<I"
    ptrsz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_lbapp_reset()
        CL.host_begin(lib, {1: self.g("obj"), 2: self.g("node"), 3: self.g("ctor"), 4: self.g("dtor"), 5: self.g("cbdtor")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lbapp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lbapp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_setname(self, name_addr):
        return self.lib.tpw_lbapp_setname(name_addr) & MASK

    def call_saver(self, enable):
        return self.lib.tpw_lbapp_saver(s32(enable)) & MASK

    def call_pump(self):
        self.lib.tpw_lbapp_pump()
        return 0

    def call_mode(self, addr):
        self.lib.tpw_lbapp_mode(addr)
        return 0

    def call_sinit(self):
        self.lib.tpw_lbapp_sinit()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lbapp_addr.restype, lib.tpw_lbapp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lbapp_alloc.restype, lib.tpw_lbapp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lbapp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lbapp_setname.restype, lib.tpw_lbapp_setname.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_lbapp_saver.restype, lib.tpw_lbapp_saver.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_lbapp_mode.argtypes = [C.c_uint64]
    lib.tpw_lbapp_reset.argtypes = []
    lib.tpw_lbapp_pump.argtypes = []
    lib.tpw_lbapp_sinit.argtypes = []
    out = (C.c_int64 * 32)()
    n = lib.tpw_lbapp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.g("name"), bytes(case["init"]))
    s.write(s.g("obj"), b"\0" * (0x60 + s.ptrsz))


def execute(s, case):
    op = case["op"]
    if op == "name":
        a = 0
        if case["str"] is not None:
            a = s.alloc(len(case["str"]) + 1)
            s.write(a, bytes(case["str"]) + b"\0")
        return s.call_setname(a)
    if op == "saver":
        return s.call_saver(case["enable"])
    if op == "pump":
        return s.call_pump()
    if op == "mode":
        a = s.alloc(12)
        s.words(a, [0x11111111] * 3)
        case["_mode"] = a
        s.call_mode(a)
        return s.rd_words(a, 3)
    return s.call_sinit()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "name": s.read(s.g("name"), 64),
            "lists": s.read(s.g("obj"), 0x60),
            "cs": any(s.read(s.g("obj") + 0x60, s.ptrsz))}


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


# ---------------------------------------------------------------------------------------------------------------- cases

def base(r, op, queues=None):
    return {"op": op, "queues": queues or {}, "init": [r.randrange(1, 256) for _ in range(64)], "str": None, "enable": 0}


def name_case(r, text="rand"):
    c = base(r, "name")
    if text == "rand":
        n = r.choice([0, 1, 5, 30, 62, 63, 64, 65, 66, 100, r.randrange(0, 110)])
        text = [r.randrange(1, 256) for _ in range(n)]
    c["str"] = text
    return c


def name_edges():
    r = random.Random(11)
    yield name_case(r, None)
    for n in (0, 1, 8, 63, 64, 65, 100):
        yield name_case(r, [0x41 + (i % 26) for i in range(n)])


def name_random(r):
    return name_case(r, None) if r.random() < 0.1 else name_case(r)


def saver_case(r, enable=None, ans=None):
    q = {"SystemParametersInfoA": [r.choice([0, 1, 5, -1]) if ans is None else ans]}
    c = base(r, "saver", q)
    c["enable"] = r.choice([0, 1, -1, r.randrange(-1000, 1000)]) if enable is None else enable
    return c


def saver_edges():
    r = random.Random(12)
    for enable in (0, 1, -1, 0x7FFFFFFF):
        for ans in (0, 1, -1):
            yield saver_case(r, enable, ans)


def saver_random(r):
    return saver_case(r)


def pump_case(r, k):
    q = {"PeekLimit": [10 ** 6 if k is None else k]}
    return base(r, "pump", q)


def pump_edges():
    r = random.Random(13)
    for k in (0, 1, 2, 3, 41, 250, 251, 1999, 2000, 2001, 2002, None):
        yield pump_case(r, k)


def pump_random(r):
    return pump_case(r, r.choice([r.randrange(0, 10), r.randrange(0, 600), None, 2000, 1999, 2001]))


def mode_case(r, handle=None):
    h = r.choice([0, 0x5000]) if handle is None else handle
    return base(r, "mode", {"CreateDC": [h], "GetDeviceCaps": [r.randrange(-2 ** 31, 2 ** 31) for _ in range(3)]})


def mode_edges():
    r = random.Random(14)
    yield mode_case(r, 0)
    yield mode_case(r, 0x5000)
    yield mode_case(r, 0x7FFFFFF0)


def mode_random(r):
    return mode_case(r)


def sinit_case(r):
    return base(r, "sinit")


def sinit_edges():
    yield sinit_case(random.Random(15))


TESTS = [
    FnTest("LbApp_SetWindowName", ppc, host, edge=name_edges, rnd=name_random),
    FnTest("LbApp_EnableWindowsScreenSaver", ppc, host, edge=saver_edges, rnd=saver_random),
    FnTest("LbApp_ProcessWindowMessages", ppc, host, edge=pump_edges, rnd=pump_random),
    FnTest("LbApp_GetWindowsScreenMode", ppc, host, edge=mode_edges, rnd=mode_random),
    FnTest("__sinit_LbSCBack_cpp", ppc, host, edge=sinit_edges, rnd=sinit_case),
]
