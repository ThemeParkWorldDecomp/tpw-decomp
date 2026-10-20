"""Unit v51000, LbScreen_SetMode of WinScrn.cpp, against libtpw_unit_LbScreen_SetMode_2.dylib (tests/unit_LbScreen_SetMode_2.cpp), program
"bullfrog", src/LbScreen_Initialise_LbScreen_Terminate_LbScreen_SetMode_2.cpp:
  LbScreen_SetMode 0x10069100

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; the same code reads both back.
Every callee outside the unit (the DirectDraw calls, the window calls, ReleaseMode, EventNotification, the FPU flag calls, the surface
initialiser, RegisterDirectDraw, ShowMessage, GetHWnd, memcpy) is a fake on both sides that records its arguments and answers from the
case's queues. The objects the fakes hand out are twelve fixed blocks, labelled 1..12 in the log and in the pointer globals.

The world of a case: the FPU handler byte, the guid, window, DirectDraw, clipper, palette and surface pointers and counters the function
reads or overwrites, the global palette (0x400 bytes of noise, so the untouched fourth byte of every entry shows) and an optional input
palette. What is compared: the answer, every global the function writes (pointers as labels), the global palette, and the event log
(the creation descriptors' fields, the palette hashes, the system event records).

No fused operations. Not tested: a NULL palette object reaching the 8-bit release (the original calls through it unchecked), and a surface
creation that succeeds with a NULL surface for the primary surface (the original calls on it unchecked).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbScreen_SetMode_2"
MASK = 0xFFFFFFFF

FN = 0x10069100

ADDR = dict(fpu=0x100750FC, fpunode=0x10075100, doublebuf=0x10075118, guid=0x1007511C, f120=0x10075120, active=0x10075124,
            A_dd=0x1007515C, B_surf=0x10075188, B_dd=0x100751AC, C_surf=0x100751D8, C_dd=0x100751FC, surface=0x10075228,
            clipper=0x1007522C, palette=0x10075230, dd=0x10075234, hwnd=0x10075238, coop=0x1007523C, gpal=0x10074070)
HOST_LAYOUT = [0x400, 28, 1]
NOBJ = 12
OBJ_PPC = []          # the persistent blocks of the emulator, objects 1..12
INPAL_PPC = []


def s32(v):
    return T.s32(v)


def hsh(data):
    h = 0
    for c in data:
        h = (h * 31 + c) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)


def lab(e, v):
    return e.cl.lab(v & MASK)


def hr(e, key):
    return s32(e.cl.ret(key)) & MASK


def ptr_word(addr):
    return struct.pack(">I", addr & MASK)


def st_check_flags(e):
    e.cl.rec("CheckFlags", (lab(e, e.r(3)),))
    e.set_r(3, 0)


def st_set_flags(e):
    e.cl.rec("SetFlags", (lab(e, e.r(3)), e.r(4) & 0xFFFF, e.r(5) & 0xFFFF))


def st_event(e):
    data = e.r(5)
    w0 = e.u32(data)
    if w0 == 7:
        e.cl.rec("Event", (e.r(4), 7, e.u32(data + 4), e.u32(data + 8), e.read(data + 12, 1)[0], e.u32(data + 16)))
    else:
        e.cl.rec("Event", (e.r(4), w0, e.u32(data + 4), 0, 0, 0))


def st_release_mode(e):
    e.cl.rec("ReleaseMode", (0,))


def st_create_dd(e):
    L = e.cl
    L.rec("CreateDD", (lab(e, e.r(4)), 1 if e.r(5) == ADDR["dd"] else 0, e.r(6)))
    e.write(e.r(3), struct.pack(">i", s32(L.ret("CreateDD"))))
    if L.ret("CreateDD.out") != 0:
        e.write(e.r(5), ptr_word(OBJ_PPC[0]))


def st_register_dd(e):
    e.cl.rec("RegisterDD", (lab(e, e.r(3)),))


def st_show_message(e):
    text = e.cstr(e.r(3))
    data = text.encode() if isinstance(text, str) else bytes(text)
    e.cl.rec("ShowMessage", (hsh(data),))


def st_get_hwnd(e):
    e.cl.rec("GetHWnd", (0,))
    k = e.cl.ret("GetHWnd")
    e.set_r(3, OBJ_PPC[k - 1] if k > 0 else 0)


def st_init_surface(e):
    e.cl.rec("InitialiseSurface", (lab(e, e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7)))


def st_release(e):
    e.cl.rec("Release", (lab(e, e.r(3)),))
    e.set_r(3, 0)


def st_restore(e):
    e.cl.rec("RestoreDisplayMode", (lab(e, e.r(3)),))
    e.set_r(3, 0)


def st_coop(e):
    e.cl.rec("SetCooperativeLevel", (lab(e, e.r(3)), lab(e, e.r(4)), e.r(5)))
    e.set_r(3, hr(e, "SetCooperativeLevel"))


def st_create_clipper(e):
    L = e.cl
    L.rec("CreateClipper", (lab(e, e.r(3)), e.r(4), 1 if e.r(5) == ADDR["clipper"] else 0, e.r(6)))
    k = hr(e, "CreateClipper")
    if L.ret("CreateClipper.out") != 0:
        e.write(e.r(5), ptr_word(OBJ_PPC[1]))
    e.set_r(3, k)


def st_clipper_sethwnd(e):
    e.cl.rec("ClipperSetHWnd", (lab(e, e.r(3)), e.r(4), lab(e, e.r(5))))
    e.set_r(3, hr(e, "SetHWnd"))


def st_set_display_mode(e):
    e.cl.rec("SetDisplayMode", (lab(e, e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7), e.r(8)))
    e.set_r(3, hr(e, "SetDisplayMode"))


def st_create_surface(e):
    L = e.cl
    d = e.r(4)
    out = e.r(5)
    which = 1 if out == ADDR["B_dd"] else (2 if out == ADDR["C_dd"] else 0)
    L.rec("CreateSurface", (lab(e, e.r(3)), e.u32(d), e.u32(d + 4), e.u32(d + 8), e.u32(d + 12), e.u32(d + 0x14), e.u32(d + 0x68),
                            which, e.r(6)))
    k = hr(e, "CreateSurface")
    if which == 1:
        e.write(out, ptr_word(OBJ_PPC[3]))
    elif L.ret("CreateSurface.out") != 0:
        e.write(out, ptr_word(OBJ_PPC[4]))
    e.set_r(3, k)


def st_get_surface_desc(e):
    L = e.cl
    L.rec("GetSurfaceDesc", (lab(e, e.r(3)),))
    e.write(e.r(4) + 0x68, struct.pack(">I", L.ret("GetSurfaceDesc.caps") & MASK))
    e.set_r(3, hr(e, "GetSurfaceDesc"))


def st_surface_set_clipper(e):
    e.cl.rec("SurfaceSetClipper", (lab(e, e.r(3)), lab(e, e.r(4))))
    e.set_r(3, 0)


def st_get_display_mode(e):
    L = e.cl
    L.rec("GetDisplayMode", (lab(e, e.r(3)), e.u32(e.r(4))))
    e.write(e.r(4) + 0x54, struct.pack(">I", L.ret("GetDisplayMode.bits") & MASK))
    e.set_r(3, hr(e, "GetDisplayMode"))


def st_create_palette(e):
    L = e.cl
    entries = e.r(5)
    L.rec("CreatePalette", (lab(e, e.r(3)), e.r(4), lab(e, entries), 1 if e.r(6) == ADDR["palette"] else 0, e.r(7),
                            hsh(e.read(entries, 0x400))))
    k = hr(e, "CreatePalette")
    if s32(k) >= 0:
        e.write(e.r(6), ptr_word(OBJ_PPC[2]))
    e.set_r(3, k)


def st_get_attached(e):
    L = e.cl
    out = e.r(5)
    which = 3 if out == ADDR["A_dd"] else (2 if out == ADDR["C_dd"] else 0)
    L.rec("GetAttachedSurface", (lab(e, e.r(3)), e.u32(e.r(4)), which))
    k = hr(e, "GetAttachedSurface")
    if L.ret("GetAttachedSurface.out") != 0:
        e.write(out, ptr_word(OBJ_PPC[5] if which == 3 else OBJ_PPC[4]))
    e.set_r(3, k)


def st_get_dc(e):
    e.cl.rec("GetDC", (lab(e, e.r(3)),))
    e.write(e.r(4), ptr_word(OBJ_PPC[9]))
    e.set_r(3, hr(e, "GetDC"))


def st_release_dc(e):
    e.cl.rec("ReleaseDC", (lab(e, e.r(3)), lab(e, e.r(4))))
    e.set_r(3, 0)


def st_set_palette(e):
    e.cl.rec("SetPalette", (lab(e, e.r(3)), lab(e, e.r(4))))
    e.set_r(3, 0)


def st_zero(e):
    e.cl.rec("ZeroMemory", (e.r(4),))
    e.write(e.r(3), bytes(e.r(4)))


def st_show_window(e):
    e.cl.rec("ShowWindow", (lab(e, e.r(3)), e.r(4)))
    e.set_r(3, 0)


def st_peek(e):
    L = e.cl
    L.rec("PeekMessage", (e.r(4), e.r(5), e.r(6), e.r(7)))
    k = L.ret("PeekMessage")
    if k != 0:
        e.write(e.r(3), struct.pack(">4I", 0x1000 + k, 0x2000 + k, 0x3000 + k, 0x4000 + k))
        e.set_r(3, 1)
    else:
        e.set_r(3, 0)


def st_defwindowproc(e):
    e.cl.rec("DefWindowProc", (e.r(3), e.r(4), e.r(5), e.r(6)))
    e.set_r(3, 0)


def st_sleep(e):
    e.cl.rec("Sleep", (e.r(3),))


def st_gspe(e):
    L = e.cl
    L.rec("GetSystemPaletteEntries", (lab(e, e.r(3)), e.r(4), e.r(5), 1 if e.r(6) == ADDR["gpal"] else 0))
    seed = L.ret("GetSystemPaletteEntries")
    e.write(e.r(6), bytes((i * 7 + seed) & 0xFF for i in range(0x400)))
    e.set_r(3, 0)


for _addr, _fn in [
    (0x10016070, st_check_flags), (0x100161B4, st_set_flags), (0x1001462C, st_event), (0x100689AC, st_release_mode),
    (0x1006A514, st_create_dd), (0x10066FC0, st_register_dd), (0x10068E54, st_show_message), (0x10013FB4, st_get_hwnd),
    (0x1006710C, st_init_surface), (0x1006BD0C, st_release), (0x1006C12C, st_release), (0x1006BE14, st_restore),
    (0x1006BDFC, st_coop), (0x1006BED4, st_create_clipper), (0x1006BEEC, st_clipper_sethwnd), (0x1006BF04, st_set_display_mode),
    (0x1006BD24, st_create_surface), (0x1006BA6C, st_get_surface_desc), (0x1006BF4C, st_surface_set_clipper),
    (0x1006BE8C, st_get_display_mode), (0x1006BF1C, st_create_palette), (0x1006BF34, st_get_attached), (0x1006BF64, st_get_dc),
    (0x1006BF94, st_release_dc), (0x1006BFAC, st_set_palette), (0x1006BD3C, st_zero), (0x1006BEA4, st_show_window),
    (0x1006B9DC, st_peek), (0x1006BEBC, st_defwindowproc), (0x1006BC1C, st_sleep), (0x1006BF7C, st_gspe),
]:
    if _addr >= 0x1006B000:
        # a glue import: the real glue saves the TOC in the caller's frame and the caller reloads it from 0x14(r1) afterwards
        def _glued(e, _fn=_fn):
            e.write(e.r(1) + 0x14, ptr_word(e.r(2)))
            _fn(e)
        STUBS.internal(_addr, _glued)
    else:
        STUBS.internal(_addr, _fn)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for _ in range(NOBJ):
        OBJ_PPC.append(T.persist_alloc(runner.emu, 16))
    INPAL_PPC.append(T.persist_alloc(runner.emu, 0x400))
    # a palette object the original releases through its vtable (slot 5, loaded before the call): objects 3 and 11 get one
    vtable = T.persist_alloc(runner.emu, 0x40, bytes(0x40))
    for index in (2, 10):
        runner.emu.write(OBJ_PPC[index], ptr_word(vtable))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, label):
        """Writes a pointer-sized global: object `label` (1-based, 0 NULL)."""
        v = self.obj(label) if label else 0
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        v = struct.unpack(self.pfmt, self.read(a, self.psize))[0]
        return self.name_of(v)

    def name_of(self, v):
        if v == 0:
            return 0
        for k, a in self.known().items():
            if a == v:
                return k
        return ("raw", v & MASK)

    def known(self):
        d = {i + 1: self.obj(i + 1) for i in range(NOBJ)}
        d[20] = self.g("B_surf")
        d[21] = self.g("C_surf")
        return d

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4

    def __init__(self, e, case):
        self.e = e
        fixed = {OBJ_PPC[i]: i + 1 for i in range(NOBJ)}
        fixed.update({ADDR["B_surf"]: 20, ADDR["C_surf"]: 21, ADDR["fpunode"]: 22, ADDR["gpal"]: 30, INPAL_PPC[0]: 31})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, i):
        return OBJ_PPC[i - 1]

    def inpal(self):
        return INPAL_PPC[0]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, args):
        res = self.e.alloc(4)
        w, h, bpp, flags, pal = args
        T.call_ppc(self.e, FN, [res, w & MASK, h & MASK, bpp & MASK, flags & MASK, pal])
        return s32(struct.unpack(">I", self.e.read(res, 4))[0])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = [lib.tpw_lbsm_addr(b"obj%d" % (i + 1)) for i in range(NOBJ)]
        fixed = {i + 1: self.objs[i] for i in range(NOBJ)}
        fixed.update({20: self.g("B_surf"), 21: self.g("C_surf"), 22: self.g("fpunode"), 30: self.g("gpal"), 31: self.inpal()})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lbsm_addr(name.encode())

    def obj(self, i):
        return self.objs[i - 1]

    def inpal(self):
        return self.lib.tpw_lbsm_addr(b"inpal")

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, args):
        w, h, bpp, flags, pal = args
        return self.lib.tpw_lbsm_SetMode(w & MASK, h & MASK, bpp & MASK, flags & MASK, pal)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_lbsm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "SetMode").restype = C.c_int32
    getattr(lib, p + "SetMode").argtypes = [C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

PTR_GLOBALS = ("guid", "hwnd", "dd", "clipper", "palette", "B_dd", "C_dd", "A_dd", "coop", "surface")


def build(s, case):
    wd = case["world"]
    s.write(s.g("fpu"), bytes([wd["fpu"]]))
    for name in PTR_GLOBALS:
        s.ptr(s.g(name), wd[name])
    for name in ("doublebuf", "f120", "active"):
        s.words(s.g(name), [wd[name]])
    s.write(s.g("gpal"), wd["gpal"])
    s.write(s.inpal(), wd["inpal"])


def execute(s, case):
    pal = s.inpal() if case["usepal"] else 0
    return s.call(list(case["args"]) + [pal])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "fpu": s.read(s.g("fpu"), 1)[0], "gpal": s.read(s.g("gpal"), 0x400)}
    for name in PTR_GLOBALS:
        out[name] = s.rd_ptr(s.g(name))
    out["A_dd"] = s.rd_ptr(s.g("A_dd"))
    for name in ("doublebuf", "f120", "active"):
        out[name] = s.rd_word(s.g(name))
    return out


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

FAILS = [-1, s32(0x80004005), s32(0x887602FC), -2]
HR_KEYS = ["CreateDD", "GetDisplayMode", "SetCooperativeLevel", "CreateClipper", "SetHWnd", "SetDisplayMode", "CreatePalette",
           "CreateSurface", "GetSurfaceDesc", "GetAttachedSurface", "GetDC"]


def quiet_world(r):
    return {"fpu": 0, "guid": 9, "hwnd": 8, "dd": 0, "clipper": 0, "palette": 0, "B_dd": 0, "C_dd": 0, "A_dd": 0, "coop": 0, "surface": 0,
            "doublebuf": 0, "f120": 0, "active": 0, "gpal": bytes(r.randrange(256) for _ in range(0x400)),
            "inpal": bytes(r.randrange(256) for _ in range(0x400))}


def rand_world(r):
    w = quiet_world(r)
    w["fpu"] = r.choice([0, 1])
    w["guid"] = r.choice([0, 9])
    w["hwnd"] = r.choice([0, 8])
    w["dd"] = r.choice([0, 0, 1])
    for name in ("clipper", "B_dd", "C_dd", "A_dd", "surface", "coop"):
        w[name] = r.choice([0, 11])
    w["palette"] = r.choice([0, 3, 11])
    for name in ("doublebuf", "f120", "active"):
        w[name] = r.choice([0, 1, 0x200000, 7])
    return w


def good_queues(bpp):
    q = {k: [0] for k in HR_KEYS}
    q.update({"CreateDD.out": [1], "CreateClipper.out": [1], "CreateSurface.out": [1], "GetAttachedSurface.out": [1],
              "GetDisplayMode.bits": [bpp], "GetSurfaceDesc.caps": [0], "GetSystemPaletteEntries": [3], "PeekMessage": [0],
              "GetHWnd": [7]})
    return q


def make(world, args, usepal, **over):
    q = good_queues(args[2])
    for k, v in over.items():
        q[k.replace("_", ".")] = list(v) if isinstance(v, (list, tuple)) else [v]
    return {"world": world, "args": tuple(args), "usepal": usepal, "queues": q}


def rand_queues(r, bpp):
    q = good_queues(bpp)
    for k in HR_KEYS:
        q[k] = [r.choice([0, 0, 0, 0, r.choice(FAILS)]) for _ in range(r.randrange(1, 4))]
    q["CreateClipper.out"] = [r.choice([1, 1, 1, 0])]
    q["CreateSurface.out"] = [r.choice([1, 1, 1, 0]) for _ in range(2)]
    q["GetAttachedSurface.out"] = [r.choice([1, 1, 1, 0]) for _ in range(2)]
    q["GetDisplayMode.bits"] = [bpp if r.random() < 0.75 else r.choice([8, 16, 32])]
    q["GetSurfaceDesc.caps"] = [r.choice([0, 0x200000, 0x200000, 0x200208, 0x10])]
    q["GetSystemPaletteEntries"] = [r.randrange(256)]
    q["PeekMessage"] = [r.randrange(1, 9) for _ in range(r.randrange(0, 3))] + [0]
    q["GetHWnd"] = [r.choice([0, 7, 7, 8])]
    return q


def rand_case(r):
    bpp = r.choice([8, 8, 16, 24, 32])
    flags = r.randrange(256) | r.choice([0, 0, 0x100])
    args = (r.choice([0, 320, 640, 800, 1024]), r.choice([0, 240, 480, 600, 768]), bpp, flags)
    c = make(rand_world(r), args, r.random() < 0.5)
    c["queues"] = rand_queues(r, bpp)
    return c


FLAGS = [0, 1, 2, 3, 4, 8, 9, 0x0A, 0x10, 0x11, 0x12, 0x14, 0x18, 0x20, 0x30, 0x40, 0x41, 0x42, 0x50, 0x80, 0x82, 0xCA, 0xFF, 0x5B, 0x100]


def edges():
    r = random.Random(51000)
    for bpp in (8, 16):
        for flags in FLAGS:
            for pal in (False, True):
                yield make(rand_world(r), (640, 480, bpp, flags), pal)
    # one failure at each callee, with the plain, the flipping and the windowed paths
    for key in HR_KEYS:
        for flags in (0, 1, 2, 3, 0x10, 0x40, 0x90 | 1):
            for bpp in (8, 16):
                for fail in FAILS[:2]:
                    yield make(rand_world(r), (800, 600, bpp, flags), r.random() < 0.5, **{key: fail})
    # second calls: a back buffer re-created or attached after the first call succeeded
    for flags in (1, 3, 0x81 | 2):
        for key in ("CreateSurface", "GetAttachedSurface"):
            for fail in FAILS[:2]:
                for bpp in (8, 16):
                    yield make(rand_world(r), (640, 480, bpp, flags), False, **{key: [0, fail], "GetSurfaceDesc_caps": 0x200000})
    for flags in (0, 1, 2, 3, 0x10, 0x11, 8, 0x28):
        for caps in (0, 0x200000, 0x200208, 0x10):
            for bpp in (8, 16):
                yield make(rand_world(r), (640, 480, bpp, flags), False, **{"GetSurfaceDesc_caps": caps})
    # surfaces that come back NULL, the display mode that does not match, the window messages
    for flags in (0, 2, 0x10, 0x40):
        for bpp in (8, 16):
            yield make(rand_world(r), (640, 480, bpp, flags), True, CreateSurface_out=0, GetAttachedSurface_out=0)
            yield make(rand_world(r), (640, 480, bpp, flags), True, CreateSurface_out=[1, 0], GetAttachedSurface_out=[1, 0])
            yield make(rand_world(r), (640, 480, bpp, flags), False, GetDisplayMode_bits=24)
            yield make(rand_world(r), (640, 480, bpp, flags), False, PeekMessage=[3, 2, 1, 0])
    for hw in (0, 8):
        for gh in (0, 7, 8):
            w = rand_world(r)
            w["hwnd"] = hw
            yield make(w, (640, 480, 16, 0), False, GetHWnd=gh, PeekMessage=[5, 0])
    for dd0 in (0, 1):
        for guid in (0, 9):
            w = rand_world(r)
            w["dd"] = dd0
            w["guid"] = guid
            for fpu in (0, 1):
                w["fpu"] = fpu
                yield make(dict(w), (640, 480, 8, 0), True)
                yield make(dict(w), (640, 480, 8, 0), True, CreateDD=-1)
    w = quiet_world(r)
    w["palette"] = 3
    yield make(w, (320, 200, 16, 0), False)
    yield make(w, (320, 200, 32, 0x40), False)
    yield make(w, (0xFFFFFFFF, 0x80000000, 0x108, 0xFFFFFFFF), True)


TESTS = [
    FnTest("LbScreen_SetMode", ppc, host, edge=edges, rnd=rand_case),
]
