"""Unit v5528, the Bullfrog JPEG loader's three functions, against libtpw_unit_TbJPEGLoader_ReadHeaderInfo.dylib
(tests/unit_TbJPEGLoader_ReadHeaderInfo.cpp), bullfrog, src/bullfrog/TbJPEGSaver.cpp:
  TbJPEGLoader::ReadHeaderInfo 0x10024324, TbJPEGLoader::ReadData 0x100244c0, TbJPEGLoader::ValidImage 0x100248b8

A case is a world and a call. The world is built twice by one piece of Python code: in emulator memory (big-endian) for the original and in
the host library's memory for the translation. The callees outside the unit are fakes on both sides that record their arguments: the
file's Seek (reached through slot 0x18 of the file's vtable, __ptr_glue on the Mac), the eight libjpeg imports (the fakes write the
fields of the decompress struct that the loader reads: image size, output size, colour count, colour map, scanline), TbRenderArea's two
constructors, AllocMemory, FreeMemory, SetupClientRA, TbRAConv::Move and its destructor.

The world of a case: the file object, the loader (0x430 zero bytes), the description area (random words with the palettised flag, the width and
the height set), the output area (a pattern), the colour map (three 256-byte channels), the 8-bit format global, and the fakes' answer queues.
Compared: the answer, the log, the loader's bytes (the palette), the description, the output area and the format global.

No fused operations. Not tested: a loop bound below the picture's output height (the original never leaves that loop: the palettised path
tests the scanline against the description's height, the other path against its width).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbJPEGLoader_ReadHeaderInfo"
MASK = 0xFFFFFFFF

FN_HDR, FN_DATA, FN_VALID = 0x10024324, 0x100244C0, 0x100248B8
SIZES = {FN_HDR: 320, FN_DATA: 916, FN_VALID: 192}
OWN = set(SIZES)

PTR_GLUE = 0x1006C12C
G_STD_ERROR, G_CREATE, G_SRC, G_READ_HEADER = 0x1006BAE4, 0x1006BAFC, 0x1006BB14, 0x1006BB2C
G_DESTROY, G_START, G_SCAN, G_FINISH = 0x1006BB44, 0x1006BB5C, 0x1006BB74, 0x1006BB8C
RA_CTOR, RA_CTOR0, RA_ALLOC, RA_FREE, RA_SETUP = 0x1001667C, 0x10016630, 0x10016730, 0x100167F8, 0x1001686C
CONV_MOVE, CONV_DTOR = 0x1001901C, 0x10018FB4
FMT8_TOC_OFFSET = 0x5130
SEEK_CODE = 0x10099990

# the host's TbRenderArea has a native pointer (mData is 8 bytes): its fields from the width on sit 4 bytes later
HOST_LAYOUT = [0x1D0, 0x1C, 0x54, 0x70, 0x84, 0x8C, 0x30, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x430]
MAC_RA_OFFS = [0, 4, 8, 12, 0x10, 0x14, 0x1C, 0x20, 0x24]      # the nine words the tests set and compare (not mData)
HOST_RA_OFFS = [0, 4, 8, 12, 0x10, 0x14, 0x20, 0x24, 0x28]

# the decompress struct's fields (Mac offsets)
C_IW, C_IH, C_Q, C_OW, C_OH, C_NC, C_CM, C_SL = 0x1C, 0x20, 0x54, 0x70, 0x74, 0x84, 0x88, 0x8C


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = [None]      # the Ppc side of the running case: the stack objects' labels, the last buffer, the colour map


def wr(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def lab(e, a):
    c = CUR[0]
    if a in c.obj:
        return c.obj[a]
    return e.cl.lab(a)


def new_lab(a):
    c = CUR[0]
    n = c.next
    c.next += 1
    c.obj[a] = n
    return n


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def st_seek(e):
    tv = e.r(12)
    rec(e, "Seek", lab(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), 1 if e.u32(tv) == SEEK_CODE else 0)
    e.set_r(3, e.cl.ret("Seek") & MASK)


def st_std_error(e):
    rec(e, "jpeg_std_error", -1)
    e.set_r(3, e.cl.ret("jpeg_std_error") & MASK)


def st_create(e):
    c = e.r(3)
    rec(e, "jpeg_CreateDecompress", -1, s32(e.r(4)), s32(e.r(5)), s32(e.u32(c)))
    e.write(c + 4, bytes(0x1D0 - 4))


def st_src(e):
    rec(e, "jpeg_stdio_src", -1, lab(e, e.r(4)))


def st_read_header(e):
    c = e.r(3)
    rec(e, "jpeg_read_header", -1, s32(e.r(4)))
    wr(e, c + C_IW, e.cl.ret("iw"))
    wr(e, c + C_IH, e.cl.ret("ih"))
    e.set_r(3, e.cl.ret("jpeg_read_header") & MASK)


def st_destroy(e):
    rec(e, "jpeg_destroy_decompress", -1)


def st_start(e):
    c = e.r(3)
    rec(e, "jpeg_start_decompress", -1, s32(e.u32(c + C_Q)))
    wr(e, c + C_OW, e.cl.ret("ow"))
    wr(e, c + C_OH, e.cl.ret("oh"))
    wr(e, c + C_NC, e.cl.ret("nc"))
    wr(e, c + C_CM, CUR[0].cmap)
    e.set_r(3, e.cl.ret("jpeg_start_decompress") & MASK)


def st_scan(e):
    c = e.r(3)
    first = e.u32(e.r(4))
    sl = e.u32(c + C_SL)
    rec(e, "jpeg_read_scanlines", -1, 1 if (first != 0 and first == CUR[0].last) else 0, s32(e.r(5)), s32(sl))
    wr(e, c + C_SL, sl + 1)
    e.set_r(3, 1)


def st_finish(e):
    rec(e, "jpeg_finish_decompress", -1)
    e.set_r(3, 1)


def st_ctor(e):
    this = e.r(3)
    n = new_lab(this)
    size, fmt = e.r(4), e.r(5)
    rec(e, "RACtor", n, s32(e.u32(size)), s32(e.u32(size + 4)), *[s32(e.u32(fmt + 4 * i)) for i in range(6)],
        1 if e.r(6) else 0, s32(e.r(7)))


def st_ctor0(e):
    rec(e, "RACtor0", new_lab(e.r(3)))


def st_alloc(e):
    this = e.r(4)
    rec(e, "AllocMemory", lab(e, this))
    buf = e.alloc(0x400)
    CUR[0].last = buf
    wr(e, this + 0x18, buf)


def st_free(e):
    rec(e, "FreeMemory", lab(e, e.r(3)))


def st_setup(e):
    this, client, origin, size = e.r(4), e.r(5), e.r(6), e.r(7)
    rec(e, "SetupClientRA", lab(e, this), lab(e, client), s32(e.u32(origin)), s32(e.u32(origin + 4)),
        s32(e.u32(size)), s32(e.u32(size + 4)))


def st_move(e):
    rec(e, "Move", -1, lab(e, e.r(5)), lab(e, e.r(6)), 1 if e.r(7) else 0)


def st_dtor(e):
    rec(e, "TbRAConvDtor", -1, s32(e.r(4)))


def glue(fn):
    """A glue stub saves the caller's TOC at 0x14(r1) as the real glue does (the caller reloads r2 from there afterwards)."""
    def wrapped(e):
        wr(e, e.r(1) + 0x14, e.r(2))
        fn(e)
    return wrapped


for _a, _f in ((PTR_GLUE, st_seek), (G_STD_ERROR, st_std_error), (G_CREATE, st_create), (G_SRC, st_src),
               (G_READ_HEADER, st_read_header), (G_DESTROY, st_destroy), (G_START, st_start), (G_SCAN, st_scan),
               (G_FINISH, st_finish)):
    STUBS.internal(_a, glue(_f))
for _a, _f in ((RA_CTOR, st_ctor), (RA_CTOR0, st_ctor0), (RA_ALLOC, st_alloc), (RA_FREE, st_free),
               (RA_SETUP, st_setup), (CONV_MOVE, st_move), (CONV_DTOR, st_dtor)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ra_set(self, a, ws):
        for off, w in zip(self.ra_offs, ws):
            self.words(a + off, [w])

    def ra_get(self, a):
        return [self.rd_word(a + off) for off in self.ra_offs]


class Ppc(Side):
    fmt = ">I"
    ra_offs = MAC_RA_OFFS

    def __init__(self, e, case):
        self.e = e
        self.obj, self.next, self.last, self.cmap = {}, 100, 0, 0
        CUR[0] = self
        self.loader = e.alloc(0x430)
        self.desc = e.alloc(0x28)
        self.out = e.alloc(0x28)
        # the file object: a vtable pointer; slot 0x18 holds a TVector whose code is SEEK_CODE, the other slots other codes
        self.file = e.alloc(16)
        vt = e.alloc(0x40)
        for off in range(8, 0x30, 4):
            tv = e.alloc(8)
            wr(e, tv, SEEK_CODE if off == 0x18 else 0xDEAD0000 + off)
            wr(e, vt + off, tv)
        wr(e, self.file, vt)
        self.L = e.cl = CL.PpcLog({self.desc: 1, self.file: 2, self.loader: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "fmt8":
            return 0x10073ED0     # TbPixFormat8; the asm reaches it as TOC + 0x5130
        raise KeyError(name)

    def p32(self, a):
        return a

    def set_cmap(self, a):
        self.cmap = a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_hdr(self):
        ret = self.alloc(4)
        self.words(ret, [0x55555555])
        T.call_ppc(self.e, FN_HDR, [ret, self.loader, self.file, self.out])
        return s32(self.rd_word(ret))

    def call_data(self):
        ret = self.alloc(4)
        self.words(ret, [0x55555555])
        T.call_ppc(self.e, FN_DATA, [ret, self.loader, self.desc, self.file, self.out])
        return s32(self.rd_word(ret))

    def call_valid(self):
        r3, _, _ = T.call_ppc(self.e, FN_VALID, [self.loader, self.file])
        return r3 & 0xFF


class Host(Side):
    fmt = "<I"
    ra_offs = HOST_RA_OFFS

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_jpeg_reset()
        self.loader = lib.tpw_jpeg_alloc(0x430)
        self.desc = lib.tpw_jpeg_alloc(0x30)
        self.out = lib.tpw_jpeg_alloc(0x30)
        self.file = lib.tpw_jpeg_newfile()
        CL.host_begin(lib, {1: self.desc, 2: self.file, 3: self.loader}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_jpeg_addr(name.encode())

    def p32(self, a):
        return self.lib.tpw_jpeg_offset(a)

    def set_cmap(self, a):
        self.words(self.g("cmap"), [a])

    def alloc(self, n):
        return self.lib.tpw_jpeg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_hdr(self):
        return self.lib.tpw_jpeg_hdr(self.loader, self.file, self.out)

    def call_data(self):
        return self.lib.tpw_jpeg_data(self.loader, self.desc, self.file, self.out)

    def call_valid(self):
        return self.lib.tpw_jpeg_valid(self.loader, self.file) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_jpeg_addr.restype, lib.tpw_jpeg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_jpeg_alloc.restype, lib.tpw_jpeg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_jpeg_newfile.restype, lib.tpw_jpeg_newfile.argtypes = C.c_uint64, []
    lib.tpw_jpeg_reset.argtypes = []
    lib.tpw_jpeg_offset.restype, lib.tpw_jpeg_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_jpeg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_jpeg_hdr.restype, lib.tpw_jpeg_hdr.argtypes = C.c_int32, [C.c_uint64] * 3
    lib.tpw_jpeg_data.restype, lib.tpw_jpeg_data.argtypes = C.c_int32, [C.c_uint64] * 4
    lib.tpw_jpeg_valid.restype, lib.tpw_jpeg_valid.argtypes = C.c_uint32, [C.c_uint64] * 2
    out = (C.c_int64 * 32)()
    n = lib.tpw_jpeg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.ra_set(s.desc, case["descw"])
    s.ra_set(s.out, case["outw"])
    s.words(s.g("fmt8"), case["fmt8"])
    chans = []
    for k in range(3):
        a = s.alloc(256)
        s.write(a, bytes(case["ch"][k]))
        chans.append(a)
    cm = s.alloc(12)
    s.words(cm, [s.p32(a) for a in chans])
    s.set_cmap(s.p32(cm))


def execute(s, case):
    return {"hdr": s.call_hdr, "data": s.call_data, "valid": s.call_valid}[case["op"]]()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "loader": bytes(s.read(s.loader, 0x430)),
            "desc": s.ra_get(s.desc), "out": s.ra_get(s.out), "fmt8": s.rd_words(s.g("fmt8"), 6)}


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

def mk(r, op, **o):
    """A case: o overrides any field. The loop bounds are kept finite (see the docstring)."""
    pal = o.get("pal", r.random() < 0.5)
    oh = o.get("oh", r.choice([0, 1, 2, 3, 5, 8]))
    palflag = o.get("palflag", r.choice([1, 1, 5, 0x100, MASK]) if pal else 0)
    if pal:
        dh = o.get("dh", oh + r.choice([0, 0, 1, 7]))
        dw = o.get("dw", r.choice([0, 1, 2, 17, 200, MASK]))
    else:
        dw = o.get("dw", oh + r.choice([0, 0, 1, 9]))
        dh = o.get("dh", r.choice([0, 1, oh, oh + 3, 40]))
    descw = [r.getrandbits(32) for _ in range(9)]
    descw[5], descw[6], descw[7] = palflag, dw & MASK, dh & MASK
    sk = o.get("sk", r.choice([0, 0, 0, 1, MASK, 5]))
    q = {
        "Seek": [sk & MASK],
        "jpeg_std_error": [r.getrandbits(31) + 0x10000],
        "jpeg_read_header": [o.get("rh", r.choice([1, 1, 0, 2, 3, MASK])) & MASK],
        "jpeg_start_decompress": [o.get("sd", r.choice([1, 0])) & MASK],
        "iw": [o.get("iw", r.choice([0, 1, 640, 0x55555556, r.getrandbits(32), r.randrange(1, 2000)])) & MASK],
        "ih": [o.get("ih", r.choice([0, 1, 480, r.getrandbits(32), r.randrange(1, 2000)])) & MASK],
        "ow": [o.get("ow", r.choice([0, 1, 2, 7, 16, 100, r.randrange(1, 300)])) & MASK],
        "oh": [oh & MASK],
        "nc": [o.get("nc", r.choice([0, 1, 2, 16, 255, 256, -1, r.randrange(0, 257)])) & MASK],
    }
    return {"op": op, "queues": q, "descw": descw, "outw": [r.getrandbits(32) for _ in range(9)],
            "fmt8": [r.getrandbits(32) for _ in range(6)],
            "ch": [[r.getrandbits(8) for _ in range(256)] for _ in range(3)]}


def edges(op):
    def gen():
        r = random.Random({"hdr": 5281, "data": 5282, "valid": 5283}[op])
        if op == "valid":
            for sk in (0, 1, MASK):
                for rh in (0, 1, 2, 3, MASK):
                    yield mk(r, op, sk=sk, rh=rh)
        elif op == "hdr":
            for sk in (0, 1, MASK):
                for iw, ih in ((0, 0), (1, 1), (640, 480), (0x55555556, 7), (0x80000000, 0xFFFFFFFF)):
                    yield mk(r, op, sk=sk, iw=iw, ih=ih)
        else:
            for pal in (True, False):
                for sk in (0, 1, MASK):
                    for oh in (0, 1, 3):
                        for extra in (0, 1):
                            for nc in (0, 1, 256, -1):
                                if pal:
                                    yield mk(r, op, pal=True, sk=sk, oh=oh, dh=oh + extra, nc=nc)
                                else:
                                    yield mk(r, op, pal=False, sk=sk, oh=oh, dw=oh + extra, dh=max(0, oh - 1), nc=nc)
    return gen


def rnd(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("TbJPEGLoader::ReadHeaderInfo", ppc, host, edge=edges("hdr"), rnd=rnd("hdr")),
    FnTest("TbJPEGLoader::ReadData", ppc, host, edge=edges("data"), rnd=rnd("data")),
    FnTest("TbJPEGLoader::ValidImage", ppc, host, edge=edges("valid"), rnd=rnd("valid")),
]
