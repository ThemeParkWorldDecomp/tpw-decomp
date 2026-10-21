"""Unit v5953: ConvertSquareFrom32, PT_Load, CSwitch::Do and COpenGLPrintf::Close, against libtpw_unit_ConvertSquareFrom32.dylib
(tests/unit_ConvertSquareFrom32.cpp), engine, src/engine/DirectDraw.cpp, src/engine/Switch.cpp, src/engine/tb_hfield.cpp, src/engine/tb_printf_opengl.cpp:
  ConvertSquareFrom32 0x10003454, PT_Load 0x1000bed0, CSwitch::Do 0x100328ac, COpenGLPrintf::Close 0x10046c64

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (UPixels constructor,
Convert and destructor, ULogManager, CHField::LoadProcTextures, CTextureCache Init and Close, COpenGL Close and the virtual ReInitialize,
CDirectDraw Init and Close, CErrorHandler::Push, EngineAdd_Reinit, glDeleteLists, glGetError) are fakes on both sides that record their
arguments (UPixels objects by a serial number the constructor fake stores in them) and answer from the case's queues.

The world of a case: the texture settings, system renderer, display size, the DirectDraw object (flags at 0x77c, device at 0x788), the
cache, OpenGL and height field pointers, the logger pointer, the CSwitch object and the display list word. What is compared: the answer,
the event log, the CSwitch fields, the logger pointer state and the list word.

No fused operations. Not tested: nothing is left out (the object pointers Do reads are never NULL, as in the running game).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_ConvertSquareFrom32"
MASK = 0xFFFFFFFF

FN_CONVERT, FN_PTLOAD, FN_DO, FN_CLOSE = 0x10003454, 0x1000bed0, 0x100328ac, 0x10046c64
SIZES = {FN_CONVERT: 236, FN_PTLOAD: 268, FN_DO: 468, FN_CLOSE: 92}
OWN = set(SIZES)

ADDR = dict(gei_TextureSettings=0x10165fc8, gei_System=0x10165f80, gei_Display=0x10165634, gei_DDraw=0x10052ad0,
            gei_ErrorHandler=0x10052b44, gei_TCache=0x10173184, gei_OpenGL=0x102b3640, gei_HField=0x100af11c,
            gLogger=0x102b36cc)
HOST_LAYOUT = [0x1c, 8, 0xC, 0x24, 0, 4, 0x77C, 0x788, 12]

GLUE_VT = 0x10001000


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

# (address, name, spec, ret kind)
FAKES = [
    (0x10034514, "TCClose", "L", "i"),
    (0x100455f4, "OGLClose", "L", "i"),
    (0x10004cc0, "DDClose", "L", "i"),
    (0x10003a48, "DDInit", "Liiiiii", "i"),
    (0x10008904, "Push", "Lii", None),
    (0x1004c858, "ReInit", "L", "i"),
    (0x10013bbc, "EAR", "", "b"),
    (0x1003439c, "TCInit", "Li", "i"),
    (0x1000d96c, "LPT", "Lsi", "i"),
    (0x1004bcb8, "ULogManager", "N", None),
    (0x1004bcd0, "SetContext", "Lssiu", None),
    (0x1004bce8, "Log", "Ls", None),
    (0x1004c840, "glDeleteLists", "uu", None),
    (0x1004c1f8, "glGetError", "", "i"),
]
GLUE = {0x1004c858, 0x1004bcb8, 0x1004bcd0, 0x1004bce8, 0x1004c840, 0x1004c1f8, 0x1004bf58, 0x1004bfa0, 0x1004bfe8}


def glue(fn):
    """The code after a glue call reloads r2 from 0x14(r1), where the real glue stored it."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


for _a, _n, _s, _r in FAKES:
    _f = CL.recorder(_n, _s, _r)
    STUBS.internal(_a, glue(_f) if _a in GLUE else _f)


def stub_upixels(e):
    L = e.cl
    L.n_px += 1
    e.write(e.r(3), struct.pack(">I", L.n_px))
    L.rec("UPixels", (L.n_px, s32(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), L.lab(e.r(9))))


def stub_convert(e):
    e.cl.rec("Convert", (e.u32(e.r(3)), e.u32(e.r(4)), e.r(5) & 0xFF))


def stub_dtor(e):
    e.cl.rec("~UPixels", (e.u32(e.r(3)),))


STUBS.internal(0x1004bca0, glue(lambda e: e.set_r(3, e.alloc(max(e.r(3), 4)))))   # operator new
STUBS.internal(0x1004bf58, glue(stub_upixels))
STUBS.internal(0x1004bfa0, glue(stub_convert))
STUBS.internal(0x1004bfe8, glue(stub_dtor))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_objects(self):
        self.dd = self.alloc(0x800)
        self.tc = self.alloc(0x20)
        self.hf = self.alloc(0x20)
        self.logger = self.alloc(0x20)
        self.dest = self.alloc(0x40)
        self.src = self.alloc(0x40)
        self.sw = self.alloc(12)
        self.lst = self.alloc(0x20)
        self.path = self.alloc(32)
        self.write(self.path, b"textures/pt.lnd\0")


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.off = {"renderer": 0x1C, "ddflags": 0x77C, "device": 0x788}
        self.make_objects()
        self.ogl = self.alloc(0x20)
        vt = self.alloc(0x40)
        self.words(self.ogl + 4, [vt])
        self.words(vt + 0x14, [GLUE_VT])
        self.eh = ADDR["gei_ErrorHandler"]
        self.L = e.cl = CL.PpcLog({self.dd: 1, self.tc: 2, self.ogl: 3, self.eh: 4, self.hf: 5, self.logger: 6,
                                   self.dest: 10, self.src: 11}, case["queues"])
        self.L.n_px = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "gLogger":      # an import: the loader decides where the data lives, the TOC slot holds it
            return self.e.u32(0x1004F6D0)
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_ptr(self, name, v):
        self.words(self.g(name), [v])

    def rd_ptr(self, name):
        return self.rd_word(self.g(name))

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        lib.tpw_cs_reset()
        self.make_objects()
        self.ogl = lib.tpw_cs_new_opengl()
        self.eh = self.g("gei_ErrorHandler")
        CL.host_begin(lib, {1: self.dd, 2: self.tc, 3: self.ogl, 4: self.eh, 5: self.hf, 6: self.logger,
                            10: self.dest, 11: self.src}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_ptr(self, name, v):
        self.write(self.g(name), struct.pack("<Q", v))

    def rd_ptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        lib = self.lib
        if entry == FN_CONVERT:
            lib.tpw_cs_convert(args[0], args[1], s32(args[2]))
            return 0
        if entry == FN_PTLOAD:
            return lib.tpw_cs_ptload(args[0], s32(args[1])) & MASK
        if entry == FN_DO:
            return lib.tpw_cs_do(args[0], s32(args[1])) & MASK
        return lib.tpw_cs_close(args[0]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_addr.restype, lib.tpw_cs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_new_opengl.restype, lib.tpw_cs_new_opengl.argtypes = C.c_uint64, []
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cs_convert.argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_cs_ptload.restype, lib.tpw_cs_ptload.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    lib.tpw_cs_do.restype, lib.tpw_cs_do.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    lib.tpw_cs_close.restype, lib.tpw_cs_close.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    lay = list(out)[:n]
    # the host's pointers are 8 bytes: only these three offsets differ from the Mac's
    assert [lay[i] for i in (1, 2, 3, 4, 5, 8)] == [HOST_LAYOUT[i] for i in (1, 2, 3, 4, 5, 8)], lay
    lib.off = {"renderer": lay[0], "ddflags": lay[6], "device": lay[7]}
    lib.bound = True


def build(s, case):
    s.words(s.g("gei_TextureSettings"), [case["tflags"], case["tformat"]])
    s.words(s.g("gei_System") + s.off["renderer"], [case["renderer"]])
    d = s.g("gei_Display")
    s.words(d + 8, [case["width"], case["height"]])
    s.words(d + 0x24, [case["bpp"]])
    s.words(s.dd + s.off["ddflags"], [case["ddflags"]])
    s.words(s.dd + s.off["device"], [case["device"]])
    s.set_ptr("gei_DDraw", s.dd)
    s.set_ptr("gei_TCache", s.tc)
    s.set_ptr("gei_OpenGL", s.ogl)
    s.set_ptr("gei_HField", s.hf if case["hfield"] else 0)
    s.set_ptr("gLogger", s.logger if case["logger"] else 0)
    s.words(s.sw, case["sw"])
    s.words(s.lst, [case["list"]])


def execute(s, case):
    op = case["op"]
    if op == "convert":
        s.call(FN_CONVERT, [s.dest, s.src, case["size"]])
        return 0
    if op == "ptload":
        return s.call(FN_PTLOAD, [s.path, case["mode"]])
    if op == "do":
        return s.call(FN_DO, [s.sw, case["toggle"]])
    return s.call(FN_CLOSE, [s.lst]) & 0xFF


def observe(s, case, answer):
    p = s.rd_ptr("gLogger")
    state = 0 if p == 0 else (1 if p == s.logger else 2)
    return {"answer": answer, "log": s.log(), "logger": state, "sw": s.rd_words(s.sw, 3), "list": s.rd_word(s.lst)}


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


RET = [0, 1, 1, 1, 2, 3, MASK]


def base(r, op):
    q = {}
    for k in ("TCClose", "OGLClose", "DDClose", "ReInit", "TCInit", "DDInit", "LPT", "glGetError"):
        q[k] = [r.choice(RET)]
    for k in ("DDClose", "DDInit", "TCInit", "ReInit", "TCClose", "OGLClose"):
        if r.random() < 0.7:
            q[k] = [1]
    q["EAR"] = [r.choice([0, 1, 1, 2])]
    if r.random() < 0.5:
        q["glGetError"] = [0]
    return {"op": op, "queues": q,
            "tflags": r.choice([0, 0x10000000, 0x17A, 0x1000017A, 0x4]) & MASK,
            "tformat": r.choice([0, 3, 4, 5, 6, 7, 0xFFFFFFFF]),
            "renderer": r.choice([3, 3, 3, 4, 0, 5]),
            "width": r.choice([640, 800, 1024]), "height": r.choice([480, 600, 768]), "bpp": r.choice([16, 24, 32]),
            "ddflags": r.choice([0, 1, 2, 3, 0x80000001, 0xFFFFFFFE]), "device": r.choice([0, 1, 2]),
            "hfield": r.choice([0, 1]), "logger": r.choice([0, 1]),
            "sw": [r.choice([0xFFFFFFFF, 0, 1, 5]), r.choice([0, 0, 320, 0xFFFFFFFF]), r.choice([0, 0, 240, 7])],
            "list": r.choice([0, 1, 7, 0x80000000, 0xFFFFFFFF])}


def convert_case(r, fmt=None, size=None):
    c = base(r, "convert")
    if fmt is not None:
        c["tformat"] = fmt
    c["size"] = r.choice([0, 1, 16, 32, 64, 0x10000, 0xFFFFFFFF]) if size is None else size
    return c


def convert_edges():
    r = random.Random(11)
    for fmt in (3, 4, 5, 6, 0, 0xFFFFFFFF, 0x80000000):
        for size in (0, 1, 64, 0xFFFFFFFF):
            yield convert_case(r, fmt, size)


def ptload_case(r, blocked=None, hfield=None, logger=None):
    c = base(r, "ptload")
    if blocked is not None:
        c["tflags"] = (0x1000017A if blocked else 0x17A)
    if hfield is not None:
        c["hfield"] = hfield
    if logger is not None:
        c["logger"] = logger
    c["mode"] = r.choice([0, 1, 2, 0xFFFFFFFF, 0x1234])
    return c


def ptload_edges():
    r = random.Random(22)
    for blocked in (0, 1):
        for hfield in (0, 1):
            for logger in (0, 1):
                for _ in range(2):
                    yield ptload_case(r, blocked, hfield, logger)


def do_case(r, toggle=None, renderer=None):
    c = base(r, "do")
    c["toggle"] = r.choice([0, 1, 2, 0xFFFFFFFF]) if toggle is None else toggle
    if renderer is not None:
        c["renderer"] = renderer
    return c


def do_edges():
    r = random.Random(33)
    for renderer in (3, 4):
        for toggle in (0, 1, 0xFFFFFFFF):
            for flags in (0, 1):
                for _ in range(3):
                    c = do_case(r, toggle, renderer)
                    c["ddflags"] = flags
                    yield c
    # each failing step of the renderer 3 path once, and the full success
    steps = ("TCClose", "OGLClose", "DDClose", "DDInit", "ReInit", "EAR", "TCInit")
    for bad in (None,) + steps:
        for toggle in (0, 1):
            c = do_case(r, toggle, 3)
            for k in steps:
                c["queues"][k] = [1]
            if bad:
                c["queues"][bad] = [0]
            yield c
    c = do_case(r, 0, 3)
    for k in steps:
        c["queues"][k] = [1]
    c["queues"]["TCClose"] = [2]
    c["queues"]["OGLClose"] = [1]
    yield c


def close_case(r, lst=None, err=None):
    c = base(r, "close")
    if lst is not None:
        c["list"] = lst
    if err is not None:
        c["queues"]["glGetError"] = [err]
    return c


def close_edges():
    r = random.Random(44)
    for lst in (0, 1, 0x80000000, 0xFFFFFFFF):
        for err in (0, 1, 0x500, 0x80000000, 0x100):
            yield close_case(r, lst, err)


TESTS = [
    FnTest("ConvertSquareFrom32", ppc, host, edge=convert_edges, rnd=convert_case),
    FnTest("PT_Load", ppc, host, edge=ptload_edges, rnd=ptload_case),
    FnTest("CSwitch::Do", ppc, host, edge=do_edges, rnd=do_case),
    FnTest("COpenGLPrintf::Close", ppc, host, edge=close_edges, rnd=close_case),
]
