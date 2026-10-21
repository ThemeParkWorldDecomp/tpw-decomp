"""Unit v5981, against libtpw_unit_InterfaceWindow_link.dylib (tests/unit_InterfaceWindow_link.cpp), main app
("SimTheme Park"), src/InterfaceWindow_draw_InterfaceWindow_link.cpp:
  InterfaceWindow::link 0x101ddf20, __sinit_ui_islandLobbyController_cpp 0x101e1e3c, LobbyOnlineLoginPanelDlgMsg 0x101e26fc,
  AdressesTextPanelMsg 0x101e4b00, read_JPEG_file 0x101e538c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation. The callees outside the unit
(setZ, setPivot, UI_UpdatePointerIn, UI_PostMessage, the dialog handlers, SetTextOffset, CallDefaultWindowHandler, the window's SetText
virtual, the wide string constructor, __register_global_object, fopen, fclose, fprintf, setjmp and the libjpeg calls) are fakes on both
sides that record their arguments and answer from the case's queues.

The world of a case: five windows (links, depth, pivot), the keyboard window global, the vtable of the window, the file name and the
pixel buffer, and the JPEG geometry the start fake writes into the decompressor. What is compared: the answer, every window's links
and shorts, the keyboard window and the event log (the jpeg fakes log the row pointer as an offset from the pixel buffer).

Port fix (unit v5L48): a library built with the port fix macro makes read_JPEG_file open the host file of its path (tpw::GamePathForReading, a
recording fake that answers host_path) in place of the Mac path. There each host fopen must follow a GamePathForReading of the Mac path and
open host_path of it; that pair is then compared as the Mac's one fopen of the path, and any other host fopen is marked unmapped, so it fails.

No fused operations. Not tested: a NULL this window, a NULL window in the message functions (the original reads through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceWindow_link"
MASK = 0xFFFFFFFF

FN_LINK, FN_SINIT, FN_LOGIN, FN_ADR, FN_JPEG = 0x101DDF20, 0x101E1E3C, 0x101E26FC, 0x101E4B00, 0x101E538C
SIZES = {FN_LINK: 292, FN_SINIT: 116, FN_LOGIN: 120, FN_ADR: 156, FN_JPEG: 356}
OWN = set(SIZES)

ADDR = dict(kbd=0x10395C10, text=0x103952EC, files=0x103BD164, str1=0x10395CE0, node1=0x10395CF4, str2=0x10395CC0,
            node2=0x10395CD4, dtor=0x103BCBB8)
LABELS = dict(text=20, files=21, str1=30, node1=31, str2=32, node2=33, dtor=34)
MY_ERROR_EXIT = 0x102684D0
EMU_FILES = 0x7D3F50A8      # the emulator places the import __files at 0x7d3f5000: the stream the original prints to is __files + 0xa8
PPC_OFF = dict(next=0x50, prev=0x54, child=0x58, parent=0x5C, z=0xDC, px=0xE0, py=0xE2)
HOST_NAMES = ["next", "prev", "child", "parent", "z", "px", "py"]
WIN_SIZE = 0x138


def host_path(path):
    """The unit's fake tpw::GamePathForReading: "<host>/" and the path with ':' made '/'."""
    return "<host>/" + path.replace(":", "/")
NWIN = 5

STUBS = CL.Stubs()
STUBS.real.update(OWN)

R_TBDYN = CL.recorder("TbDyn", "LW")
R_FOPEN = CL.recorder("fopen", "ss", ret="i")
R_FPRINTF = CL.recorder("fprintf", "xss")


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def lab(e, r):
    return e.cl.lab(e.r(r))


def q(e, key):
    return e.cl.ret(key) & MASK


STUBS.internal(0x101DD8BC, lambda e: rec(e, "setZ", lab(e, 3), s16(e.r(4))))
STUBS.internal(0x101DDD34, lambda e: rec(e, "setPivot", lab(e, 3), s16(e.r(4)), s16(e.r(5))))
STUBS.internal(0x101C8F18, lambda e: rec(e, "upi"))
STUBS.internal(0x101C9400, lambda e: rec(e, "post", lab(e, 3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def stub_online(e):
    rec(e, "online", lab(e, 3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))
    e.set_r(3, q(e, "online"))


def stub_dflt(e):
    rec(e, "dflt", lab(e, 3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))
    e.set_r(3, q(e, "dflt"))


def stub_settextoffset(e):
    rec(e, "SetTextOffset", lab(e, 3), s32(e.r(4)), s32(e.r(5)))


def stub_cdwh(e):
    rec(e, "cdwh", lab(e, 3), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)))
    e.set_r(3, q(e, "cdwh"))


def stub_ptr_glue(e):
    rec(e, "virtual", lab(e, 3), e.r(12) & MASK, lab(e, 4))


def stub_tbdyn(e):
    a = e.r(4)
    units = []
    while len(units) < 64:
        u = struct.unpack(">H", e.read(a + 2 * len(units), 2))[0]
        if u == 0:
            break
        units.append(u)
    chars = (units + [0, 0, 0, 0])[:4] if len(units) < 4 else units[:4]
    rec(e, "TbDyn", lab(e, 3), len(units), sum(u * (k + 1) for k, u in enumerate(units)), *chars)


def stub_register(e):
    rec(e, "register", lab(e, 3), lab(e, 4), lab(e, 5))


STUBS.internal(0x101E24C8, stub_online)
STUBS.internal(0x101F1448, stub_dflt)
STUBS.internal(0x101DA484, stub_settextoffset)
STUBS.internal(0x101DE318, stub_cdwh)
STUBS.internal(0x1023BAA8, stub_ptr_glue)
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", stub_tbdyn)
STUBS.imp("__register_global_object", stub_register)


def stub_fprintf(e):
    rec(e, "fprintf_file", 1 if e.r(3) == EMU_FILES else 0)
    R_FPRINTF(e)


def stub_fclose(e):
    rec(e, "fclose", e.r(3) & MASK)


def stub_std_error(e):
    rec(e, "std_error")
    e.set_r(3, q(e, "std_error"))


def stub_setjmp(e):
    b = e.r(3)
    rec(e, "setjmp", e.u32(b + 0x11C), 1 if e.u32(b - 0x84) == MY_ERROR_EXIT else 0)
    e.set_r(3, q(e, "setjmp"))


def stub_create(e):
    rec(e, "create", e.u32(e.r(3)), s32(e.r(4)), e.r(5) & MASK)


def stub_start(e):
    c = e.r(3)
    rec(e, "start")
    e.write(c + 0x70, struct.pack(">I", q(e, "jw")))
    e.write(c + 0x74, struct.pack(">I", q(e, "jh")))
    e.write(c + 0x7C, struct.pack(">I", q(e, "jc")))
    e.write(c + 0x8C, struct.pack(">I", q(e, "js")))
    e.set_r(3, 1)


def stub_scanlines(e):
    c = e.r(3)
    row = e.u32(e.r(4))
    rec(e, "scanlines", (row - e.cl.pix) & MASK, e.r(5) & MASK, e.u32(c + 0x8C))
    e.write(c + 0x8C, struct.pack(">I", (e.u32(c + 0x8C) + 1) & MASK))
    e.set_r(3, 1)


STUBS.imp("fopen", R_FOPEN)
STUBS.imp("fprintf", stub_fprintf)
STUBS.imp("fclose", stub_fclose)
STUBS.imp("jpeg_std_error__FP14jpeg_error_mgr", stub_std_error)
STUBS.internal(0x10237FA0, stub_setjmp)
STUBS.imp("jpeg_destroy_decompress__FP22jpeg_decompress_struct", lambda e: rec(e, "destroy"))
STUBS.imp("jpeg_CreateDecompress__FP22jpeg_decompress_structiUl", stub_create)
STUBS.imp("jpeg_stdio_src__FP22jpeg_decompress_structPQ23std5_FILE", lambda e: rec(e, "stdio_src", e.r(4) & MASK))
STUBS.imp("jpeg_read_header__FP22jpeg_decompress_structi", lambda e: (rec(e, "read_header", s32(e.r(4))), e.set_r(3, 1)))
STUBS.imp("jpeg_start_decompress__FP22jpeg_decompress_struct", stub_start)
STUBS.imp("jpeg_read_scanlines__FP22jpeg_decompress_structPPUcUi", stub_scanlines)
STUBS.imp("jpeg_finish_decompress__FP22jpeg_decompress_struct", lambda e: (rec(e, "finish"), e.set_r(3, 1)))

WINS = {}
VT = {}


def setup(runner):
    T.install_std_stubs(runner.emu)
    for i in range(1, NWIN + 1):
        WINS[i] = T.persist_alloc(runner.emu, WIN_SIZE)
    VT["addr"] = T.persist_alloc(runner.emu, 0x80, bytes(0x2C) + struct.pack(">I", 0x2C) + bytes(0x80 - 0x30))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def wptr(self, i, name, target):
        a = self.win(i) + self.off[name]
        self.write(a, struct.pack(self.pfmt, self.win(target) if target else 0))

    def rptr(self, i, name):
        v = struct.unpack(self.pfmt, self.read(self.win(i) + self.off[name], self.psz))[0]
        return self.back.get(v, -1 if v else 0)

    def ws16(self, i, name, v):
        self.write(self.win(i) + self.off[name], struct.pack(self.fmt[0] + "h", s16(v)))

    def rs16(self, i, name):
        return struct.unpack(self.fmt[0] + "h", self.read(self.win(i) + self.off[name], 2))[0]

    def setptr(self, a, target):
        self.write(a, struct.pack(self.pfmt, self.win(target) if target else 0))


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    off = PPC_OFF

    def __init__(self, e, case):
        self.e = e
        fixed = {WINS[i]: i for i in WINS}
        fixed.update({ADDR[k]: v for k, v in LABELS.items()})
        self.back = {WINS[i]: i for i in WINS}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def win(self, i):
        return WINS[i]

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

    def zero_windows(self):
        for i in WINS:
            self.write(WINS[i], bytes(WIN_SIZE))

    def set_vtable(self):
        self.words(WINS[1] + 0x118, [VT["addr"]])

    def set_pix(self, pix):
        self.L.pix = pix

    def call(self, fn, args):
        entry = {"link": FN_LINK, "sinit": FN_SINIT, "login": FN_LOGIN, "adr": FN_ADR, "jpeg": FN_JPEG}[fn]
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = dict(zip(HOST_NAMES, lib.lay))
        self.wsize = lib.lay[7]
        self.addrs = {i: self.g("w%d" % i) for i in range(1, NWIN + 1)}
        self.back = {self.addrs[i]: i for i in self.addrs}
        fixed = {i: self.addrs[i] for i in self.addrs}
        fixed.update({v: self.g(k) for k, v in LABELS.items()})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def win(self, i):
        return self.addrs[i]

    def g(self, name):
        return self.lib.tpw_ilk_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ilk_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        log = CL.host_end(self.lib)
        if self.lib.tpw_ilk_portfix() == 0:
            return log
        out = []
        for ev in log:
            if ev[0] != "fopen":
                out.append(ev)
                continue
            prev = out[-1] if out else None
            if prev is not None and prev[0] == "GamePathForReading" and ev[2][0] == host_path(prev[2][0]):
                out[-1] = ("fopen", ev[1], (prev[2][0],) + ev[2][1:])
            else:
                out.append(("fopen unmapped",) + ev[1:])
        return out

    def zero_windows(self):
        for i in self.addrs:
            self.write(self.addrs[i] + 8, bytes(self.wsize - 8))     # the first word is the vtable pointer

    def set_vtable(self):
        pass

    def set_pix(self, pix):
        pass

    def call(self, fn, args):
        ps = {"link": [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32], "sinit": [], "login": [C.c_uint64, C.c_int32] * 1 + [C.c_int32] * 2,
              "adr": [C.c_uint64, C.c_int32, C.c_int32, C.c_int32], "jpeg": [C.c_uint64, C.c_uint64]}[fn]
        f = getattr(self.lib, "tpw_ilk_" + fn)
        f.argtypes = ps
        f.restype = C.c_int32
        return f(*[a if p is C.c_uint64 else s32(a) for p, a in zip(ps, args)]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ilk_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ilk_portfix.restype, lib.tpw_ilk_portfix.argtypes = C.c_int32, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lib.lay = list(out)[:n]
    assert n == 8 and lib.lay[7] >= 0x138
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.zero_windows()
    for i, w in case["wins"].items():
        for name in ("next", "prev", "child", "parent"):
            s.wptr(i, name, w[name])
        for name in ("z", "px", "py"):
            s.ws16(i, name, w[name])
    s.setptr(s.g("kbd"), case["kbd"])
    s.write(s.g("text"), b"")
    s.set_vtable()
    if case["op"] == "jpeg":
        path = s.alloc(64)
        s.write(path, case["path"] + b"\0")
        pix = s.alloc(4096)
        s.set_pix(pix)
        case["_ptrs"] = (path, pix)


def execute(s, case):
    op = case["op"]
    if op == "link":
        child = s.win(case["child"]) if case["child"] else 0
        return s.call("link", [s.win(1), child, case["a"], case["b"]])
    if op == "sinit":
        s.call("sinit", [])
        return 0
    if op in ("login", "adr"):
        return s.call(op, [s.win(1)] + case["args"])
    return s.call("jpeg", list(case["_ptrs"]))


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "kbd": s.rptr_abs(s.g("kbd")) if False else None}
    kv = struct.unpack(s.pfmt, s.read(s.g("kbd"), s.psz))[0]
    out["kbd"] = s.back.get(kv, -1 if kv else 0)
    for i in range(1, NWIN + 1):
        out["w%d" % i] = [s.rptr(i, n) for n in ("next", "prev", "child", "parent")] + [s.rs16(i, n) for n in ("z", "px", "py")]
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


def rs(r):
    return r.choice([0, 1, -1, 0x100, -0x8000, 0x7FFF, r.randrange(-300, 300), r.randrange(-0x8000, 0x8000)])


def base(r, op):
    c = {"op": op, "kbd": 0, "child": 0, "a": 0, "b": 0, "args": [],
         "queues": {"online": [r.choice([0, 1, -1, 5])], "dflt": [r.choice([0, 1, -1, 77, r.randrange(-1000, 1000)])],
                    "cdwh": [r.choice([0, 1, -1, 99, r.randrange(-1000, 1000)])], "fopen": [r.choice([0, 0x1234, 7, 0xFFFF0000])],
                    "std_error": [r.randrange(1, 0xFFFFF)], "setjmp": [r.choice([0, 0, 0, 1, 5])], "jw": [r.randrange(0, 40)],
                    "jh": [r.randrange(0, 5)], "jc": [r.choice([1, 3, 4])], "js": [0]}}
    c["wins"] = {i: dict(next=0, prev=0, child=0, parent=0, z=rs(r), px=rs(r), py=rs(r)) for i in range(1, NWIN + 1)}
    return c


def link_case(r, chain=None, state=None, kbd=None, a=None, b=None, nullchild=False):
    c = base(r, "link")
    chain = r.sample([3, 4, 5], r.choice([0, 0, 1, 2, 3])) if chain is None else chain
    w = c["wins"]
    w[1]["child"] = chain[0] if chain else 0
    for k, i in enumerate(chain):
        w[i]["parent"] = 1
        w[i]["next"] = chain[k + 1] if k + 1 < len(chain) else 0
        w[i]["prev"] = chain[k - 1] if k else 0
    c["child"] = 0 if nullchild else 2
    state = r.choice([None, None, None, "parent", "next", "prev", "all"]) if state is None else state
    if state in ("parent", "all"):
        w[2]["parent"] = r.randrange(1, 6)
    if state in ("next", "all"):
        w[2]["next"] = r.randrange(1, 6)
    if state in ("prev", "all"):
        w[2]["prev"] = r.randrange(1, 6)
    c["kbd"] = r.choice([0, 1, 1, 2, 3]) if kbd is None else kbd
    flags = [0, 1, -1, 0x80, 0x7F, 0x100, 0x180, 0x1FF]
    c["a"] = r.choice(flags) if a is None else a
    c["b"] = r.choice(flags) if b is None else b
    return c


def link_edges():
    r = random.Random(5981)
    yield link_case(r, nullchild=True)
    for st in ("parent", "next", "prev", "all"):
        yield link_case(r, state=st, a=1, b=1)
    for chain in ([], [3], [3, 4], [3, 4, 5]):
        for kbd in (0, 1, 2):
            for a, b in ((0, 0), (1, 0), (0, 1), (1, 1), (0x100, 0x180), (0x80, 0x7F)):
                yield link_case(r, chain=chain, state="none", kbd=kbd, a=a, b=b)


def link_random(r):
    return link_case(r)


def msg_args(r, m=None):
    return [r.choice([0x800, 0x800, 0, 1, 3, 0x17, -1, 0x7FF, 0x801, r.randrange(-5000, 5000)]) if m is None else m,
            r.choice([0, 1, -1, r.randrange(-10 ** 6, 10 ** 6)]), r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000)])]


def login_case(r, online=None):
    c = base(r, "login")
    c["args"] = msg_args(r)
    if online is not None:
        c["queues"]["online"] = [online]
    return c


def login_edges():
    r = random.Random(5982)
    for online in (0, 1, -1, 0x100):
        yield login_case(r, online)


def login_random(r):
    return login_case(r)


def adr_case(r, m=None):
    c = base(r, "adr")
    c["args"] = msg_args(r, m)
    return c


def adr_edges():
    r = random.Random(5983)
    for m in (0x800, 0x7FF, 0x801, 0, -0x7FFFF800, 0x1800):
        for _ in range(2):
            yield adr_case(r, m)
    c = adr_case(r, 0x800)
    c["args"][2] = -0x80000000
    yield c


def adr_random(r):
    return adr_case(r)


def sinit_case(r):
    return base(r, "sinit")


def sinit_edges():
    yield sinit_case(random.Random(5984))


def sinit_random(r):
    return sinit_case(r)


def jpeg_case(r, fopen=None, setjmp=None, w=None, h=None, c_=None, s=None):
    c = base(r, "jpeg")
    c["path"] = bytes(r.choice(b"abcxyz/._-019") for _ in range(r.randrange(1, 14)))
    if fopen is not None:
        c["queues"]["fopen"] = [fopen]
    if setjmp is not None:
        c["queues"]["setjmp"] = [setjmp]
    if w is not None:
        c["queues"]["jw"] = [w]
    if h is not None:
        c["queues"]["jh"] = [h]
    if c_ is not None:
        c["queues"]["jc"] = [c_]
    if s is not None:
        c["queues"]["js"] = [s]
    elif r.random() < 0.2:
        c["queues"]["js"] = [r.choice([1, 2, 3, 4, 5, 0xFFFFFFFF, 0x80000000])]
    return c


def jpeg_edges():
    r = random.Random(5985)
    yield jpeg_case(r, fopen=0)
    yield jpeg_case(r, fopen=0x1234, setjmp=1)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=10, h=0)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=10, h=1, c_=3, s=0)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=10, h=3, c_=3, s=0)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=7, h=3, c_=1, s=1)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=7, h=3, c_=1, s=3)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=7, h=3, c_=1, s=0xFFFFFFFF)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=0x10000000, h=3, c_=4, s=0)
    yield jpeg_case(r, fopen=0x1234, setjmp=0, w=0, h=3, c_=3, s=0)
    # The Mac path of an outbox mail's picture, as UpdatePreview passes it (the port fix maps it to the user or game folder).
    for fopen in (0, 0x1234):
        c = jpeg_case(r, fopen=fopen, setjmp=0, w=7, h=2, c_=3, s=0)
        c["path"] = b"TPW:Theme Park Data:save:users:1Tess:outbox:00000001.jpg"
        yield c


def jpeg_random(r):
    return jpeg_case(r, fopen=r.choice([0, 0x1234, 7]), setjmp=r.choice([0, 0, 1]))


TESTS = [
    FnTest("InterfaceWindow::link", ppc, host, edge=link_edges, rnd=link_random),
    FnTest("__sinit_ui_islandLobbyController_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("LobbyOnlineLoginPanelDlgMsg", ppc, host, edge=login_edges, rnd=login_random),
    FnTest("AdressesTextPanelMsg", ppc, host, edge=adr_edges, rnd=adr_random),
    FnTest("read_JPEG_file", ppc, host, edge=jpeg_edges, rnd=jpeg_random),
]
