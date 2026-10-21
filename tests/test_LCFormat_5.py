"""Unit v5702, the save panel's row maker and OK handler, against libtpw_unit_LCFormat_5.dylib (tests/unit_LCFormat_5.cpp), main app
("SimTheme Park"), src/game/ui/ui_loadSavePanel.cpp:
  LCFormat 0x101a3154 (host name SavePanelLCFormat), SaveCallback_OK 0x101a2d74

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses and in the
host library's memory. The callees outside the unit are fakes on both sides that record their arguments: GetFontTableEntry, the height
virtual (__ptr_glue on the Mac, InterfaceFont::GetHeight on the host), the InterfaceWindow constructor and CreateObject, CFSLoadSave::Save,
RemoveSavePanel and the string constructor and destructor.

The world of a case: the row height global (0 or kept), the screen height, the name buffer. What is compared: the answer (a label), the
globals and the event log. No fused operations. Not tested: a screen height of 0 (the original divides by it unchecked; the emulator and
the hardware disagree).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCFormat_5"
MASK = 0xFFFFFFFF

FN_LCF, FN_SAVE = 0x101A3154, 0x101A2D74
SIZES = {FN_LCF: 228, FN_SAVE: 88}
OWN = set(SIZES)

GETFONT = 0x10180270
PTRGLUE = 0x1023BAA8
WIN_CT = 0x101D98D8
CREATE = 0x101DC750
SAVE = 0x101FC4F4
REMOVE = 0x101A3BE4
STR_CT = 0x10238800
STR_DT = 0x10238710

ADDR = dict(format=0x10394928, name=0x1039492C, screen=0x10395BBA, loadsave=0x10396A7C)
HOST_LAYOUT = [68, 4, 2]

STUBS = CL.Stubs()
STUBS.real.update(OWN)

def font_entry(e, log):
    """A font entry whose +0x10 holds a table whose +0xc holds a pointer (what the original reads before the glue call)."""
    a = e.alloc(0x80)
    e.write(a + 0x10, struct.pack(">I", a + 0x20))
    e.write(a + 0x2C, struct.pack(">I", a + 0x40))
    return a


STUBS.internal(GETFONT, CL.recorder("GetFontTableEntry", "u", ret=font_entry))
_ptr_glue = CL.recorder("ptr_glue", "L", ret="h")


def ptr_glue(e):
    """The real glue saves the caller's TOC pointer in the stack slot (0x14(r1)) that the caller reloads r2 from."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    _ptr_glue(e)


STUBS.internal(PTRGLUE, ptr_glue)
STUBS.internal(WIN_CT, CL.recorder("InterfaceWindow", "NLiihhhh"))
STUBS.internal(CREATE, CL.recorder("CreateObject", "Liiihhhh"))
STUBS.internal(SAVE, CL.recorder("Save", "LL"))
STUBS.internal(REMOVE, CL.recorder("RemoveSavePanel", ""))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", CL.recorder("StringCtor", "NS"))
STUBS.imp("__dt__26TbDynamicStringTemplate<w>Fv", CL.recorder("StringDtor", "Li"))


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
        self.L = e.cl = CL.PpcLog({ADDR["loadsave"]: 1}, case["queues"])
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

    def call_format(self):
        r3, _, _ = T.call_ppc(self.e, FN_LCF, [])
        return self.L.lab(r3 & MASK)

    def call_save(self):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [0])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("loadsave")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lcf_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_format(self):
        return self.lib.tpw_lcf_format()

    def call_save(self):
        return self.lib.tpw_lcf_save() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lcf_addr.restype, lib.tpw_lcf_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lcf_alloc.restype, lib.tpw_lcf_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lcf_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lcf_format.restype, lib.tpw_lcf_format.argtypes = C.c_int64, []
    lib.tpw_lcf_save.restype, lib.tpw_lcf_save.argtypes = C.c_int32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_lcf_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("format"), [case["format"]])
    s.write(s.g("screen"), struct.pack(s.fmt[0] + "h", case["screen"]))
    s.write(s.g("name"), struct.pack(s.fmt[0] + "34H", *case["name"]))


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "format": s.rd_words(s.g("format"), 1), "name": list(struct.unpack(s.fmt[0] + "34H", s.read(s.g("name"), 68)))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call_format() if case["op"] == "f" else s.call_save())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    n = r.randrange(0, 12)
    name = [r.randrange(1, 0x10000) for _ in range(n)] + [0] * (34 - n)
    return {"op": op,
            "format": r.choice([0, 0, 0, 0x38, -1, 0x12345, 0x18000, r.randrange(-1000, 1000)]) & MASK,
            "screen": r.choice([1, -1, 480, 600, 768, 1024, r.randrange(1, 3000), -r.randrange(1, 3000)]),
            "name": name,
            "queues": {"ptr_glue": [r.choice([0, 1, -1, 12, 16, 32767, -32768, r.randrange(-300, 300)])]}}


def f_edges():
    r = random.Random(7)
    for fmt in (0, 1, 0x38, -1 & MASK, 0x18000):
        for screen in (1, -1, 768):
            for h in (0, 12, -5, 32767, -32768):
                c = base(r, "f")
                c["format"], c["screen"], c["queues"] = fmt, screen, {"ptr_glue": [h]}
                yield c


def f_random(r):
    return base(r, "f")


def s_edges():
    r = random.Random(8)
    for n in (0, 1, 33, 34):
        c = base(r, "s")
        c["name"] = [r.randrange(1, 0x10000) for _ in range(min(n, 33))] + [0] * (34 - min(n, 33))
        yield c


def s_random(r):
    return base(r, "s")


TESTS = [
    FnTest("LCFormat", ppc, host, edge=f_edges, rnd=f_random),
    FnTest("SaveCallback_OK", ppc, host, edge=s_edges, rnd=s_random),
]
