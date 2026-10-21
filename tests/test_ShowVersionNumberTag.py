"""Unit v5289, the version number tag and the delme dialog handler, against libtpw_unit_ShowVersionNumberTag.dylib
(tests/unit_ShowVersionNumberTag.cpp), main app ("SimTheme Park"), src/game/ui/ui_StatusPanel.cpp, src/game/ui/ui_tags.cpp:
  ShowVersionNumberTag 0x101928dc, delme 0x10193084

A case is a world and a call, built by one piece of Python code on both sides. The callees outside the unit (swprintf, AddATag, ShowTagPanel,
UI_SetSelectedObject, LLCM_SetCurrentObject, LLCC_SystemSetMode, LLCM_LeftMouseButtonUp) are fakes on both sides that record their arguments;
swprintf writes two characters from the case into the buffer and AddATag reads the tag block (all fields, the first two characters of its text)
and answers 0 or a window from the case.

The world of a case: the format string at 0x1024dde8 (written on both sides), the answers of the fakes, and for delme an object with the bytes
at offsets 4 and 6. What is compared: the answer, the log, the format string afterwards.

No fused operations. Not tested: nothing left out.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowVersionNumberTag"
MASK = 0xFFFFFFFF

FN_SHOW, FN_DELME = 0x101928DC, 0x10193084
SIZES = {FN_SHOW: 224, FN_DELME: 148}
OWN = set(SIZES)

ADDR_ADDATAG, ADDR_SHOWTAGPANEL = 0x10191EF8, 0x10190D0C
ADDR_SELECT, ADDR_SETCUR, ADDR_MODE, ADDR_MOUSEUP = 0x10194768, 0x10088590, 0x10090E18, 0x10084CA4

ADDR = dict(fmt=0x1024DDE8)
HOST_LAYOUT = [64]
WINDOW = 0x5000

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_swprintf(e):
    L = e.cl
    fmt = struct.unpack(">6H", e.read(e.r(5), 12))
    L.rec("swprintf", (e.r(4), T.s32(e.r(6)), T.s32(e.r(7))) + fmt)
    c0, c1 = L.ret("sw0") & 0xFFFF, L.ret("sw1") & 0xFFFF
    e.write(e.r(3), struct.pack(">HHH", c0, c1, 0))
    e.set_r(3, 0)


def stub_addatag(e):
    L = e.cl
    p = e.r(3)
    ident, f4, text, tm = struct.unpack(">4I", e.read(p, 16))
    frame = struct.unpack(">h", e.read(p + 0x10, 2))[0]
    f14, f18 = struct.unpack(">2i", e.read(p + 0x14, 8))
    f1c = struct.unpack(">H", e.read(p + 0x1C, 2))[0]
    f20 = struct.unpack(">i", e.read(p + 0x20, 4))[0]
    show = e.read(p + 0x24, 1)[0]
    if text:
        c0, c1 = struct.unpack(">2H", e.read(text, 4))
    else:
        c0 = c1 = -1
    L.rec("AddATag", (ident, T.s32(f4), T.s32(tm), frame, f14, f18, f1c, f20, show, c0, c1, 1 if text else 0))
    e.set_r(3, L.ret("AddATag") & MASK)


def stub_showtagpanel(e):
    e.cl.rec("ShowTagPanel", (e.cl.lab(e.r(3)),))


def stub_select(e):
    e.cl.rec("UI_SetSelectedObject", (e.cl.lab(e.r(3)),))


def stub_setcur(e):
    e.cl.rec("LLCM_SetCurrentObject", (e.r(3) & 0xFF,))


def stub_mode(e):
    e.cl.rec("LLCC_SystemSetMode", (T.s32(e.r(3)), e.r(4) & 0xFF))


def stub_mouseup(e):
    e.cl.rec("LLCM_LeftMouseButtonUp", (T.s32(e.r(3)), T.s32(e.r(4)), e.r(5) & 0xFFFF, T.s32(e.r(6))))


STUBS.imp("swprintf", stub_swprintf)
STUBS.internal(ADDR_ADDATAG, stub_addatag)
STUBS.internal(ADDR_SHOWTAGPANEL, stub_showtagpanel)
STUBS.internal(ADDR_SELECT, stub_select)
STUBS.internal(ADDR_SETCUR, stub_setcur)
STUBS.internal(ADDR_MODE, stub_mode)
STUBS.internal(ADDR_MOUSEUP, stub_mouseup)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WINDOW: 1}, case["queues"])
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

    def show(self):
        T.call_ppc(self.e, FN_SHOW, [])
        return 0

    def delme(self, obj):
        r3, _, _ = T.call_ppc(self.e, FN_DELME, [obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: WINDOW}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_vnt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_vnt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def show(self):
        self.lib.tpw_vnt_show()
        return 0

    def delme(self, obj):
        return self.lib.tpw_vnt_delme(obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_vnt_addr.restype, lib.tpw_vnt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_vnt_alloc.restype, lib.tpw_vnt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_vnt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_vnt_show.restype, lib.tpw_vnt_show.argtypes = None, []
    lib.tpw_vnt_delme.restype, lib.tpw_vnt_delme.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_vnt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.g("fmt"), struct.pack(">12H", *case["fmt"]) if s.fmt == ">I" else struct.pack("<12H", *case["fmt"]))
    if case["op"] == "delme":
        data = bytearray(16)
        data[4], data[6] = case["b4"], case["b6"]
        s.obj = s.alloc(16)
        s.write(s.obj, bytes(data))


def execute(s, case):
    return s.show() if case["op"] == "show" else s.delme(s.obj)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "fmt": list(struct.unpack(s.fmt[0] + "12H", s.read(s.g("fmt"), 24)))}


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


def base(r, op):
    fmt = [r.choice([0x25, 0x56, 0x65, r.randrange(1, 0x300)]) for _ in range(11)] + [0]
    return {"op": op, "fmt": fmt, "b4": r.randrange(256), "b6": r.randrange(256),
            "queues": {"sw0": [r.choice([0, 0x41, r.randrange(0x10000)])], "sw1": [r.choice([0, 0x2E, r.randrange(0x10000)])],
                       "AddATag": [r.choice([0, WINDOW])]}}


def show_edges():
    r = random.Random(11)
    for w in (0, WINDOW):
        for s0 in (0, 0x41, 0xFFFF):
            c = base(r, "show")
            c["queues"]["AddATag"] = [w]
            c["queues"]["sw0"] = [s0]
            yield c


def show_random(r):
    return base(r, "show")


def delme_edges():
    r = random.Random(12)
    for b4, b6 in ((0, 0), (0xFF, 0xFF), (0x7F, 0), (0x80, 0), (0x7F, 1), (0, 1), (1, 0), (0xFF, 0), (0, 0xFF), (0x7E, 1), (0x80, 1)):
        c = base(r, "delme")
        c["b4"], c["b6"] = b4, b6
        yield c


def delme_random(r):
    return base(r, "delme")


TESTS = [
    FnTest("ShowVersionNumberTag", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("delme", ppc, host, edge=delme_edges, rnd=delme_random),
]
