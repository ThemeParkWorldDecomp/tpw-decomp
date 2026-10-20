"""Unit v5579, the chat postcard hand-over and two file leftovers, against libtpw_unit_ChatPrePostCard.dylib
(tests/unit_ChatPrePostCard.cpp), main app, src/LCPostFormat_2_ChatPrePostCard.cpp:
  ChatPrePostCard 0x101e568c, ChatReinstateAfterPostcard 0x101e5700, LockChatPanelOff 0x101e55ec,
  __sinit_ui_onlineOutbox_cpp 0x101e551c, my_error_exit 0x101e5310

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian, host memory little-endian). The callees
outside the unit (UI_SendMessage, CaptureKb, the string and render area constructors, __register_global_object, the pointer glue and
the long jump) are fakes on both sides recording their arguments. Window pointers are arbitrary words (never dereferenced).

The world: the window globals, the two byte flags, the chat words 0x80 and 0x84, for my_error_exit a block with the manager. Compared:
the log and every global the functions touch. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ChatPrePostCard"
MASK = 0xFFFFFFFF

PRE, REINSTATE, LOCK, SINIT, EXIT = 0x101e568c, 0x101e5700, 0x101e55ec, 0x101e551c, 0x101e5310
SIZES = {PRE: 76, REINSTATE: 136, LOCK: 120, SINIT: 160, EXIT: 68}
OWN = set(SIZES)

UI_SEND, CAPTURE_KB, STR_CTOR, REG_GLOBAL, RENDER_AREA, PTR_GLUE, LONGJMP = (
    0x101c9518, 0x101dccec, 0x10238800, 0x10239940, 0x1023ba40, 0x1023baa8, 0x10238018)
DESTROY_WSTRING = 0x103bcbb8
SEPARATORS_PLUS_6 = 0x1024eed0

ADDR = dict(w80=0x10395e80, w84=0x10395e84, window=0x10395e94, panel=0x10395e98, toggle=0x10395e79, shown=0x10395e7a,
            string=0x10395e40, node=0x10395e54, preview=0x10395e10)
HOST_LAYOUT = [4, 0]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ui_send(e):
    e.cl.rec("UI_SendMessage", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK))
    e.set_r(3, 0)


def stub_capture(e):
    e.cl.rec("CaptureKb", (e.r(3) & MASK, e.r(4) & MASK))


def stub_str_ctor(e):
    e.cl.rec("StringCtor", (e.cl.lab(e.r(3)), 1 if e.r(4) == SEPARATORS_PLUS_6 else 0))


def stub_register(e):
    e.cl.rec("RegisterGlobal", (e.cl.lab(e.r(3)), 1 if e.r(4) == DESTROY_WSTRING else 0, e.cl.lab(e.r(5))))


def stub_render(e):
    L = e.cl
    size = e.r(4)
    fmt = e.r(5)
    w, h = struct.unpack(">II", e.read(size, 8))
    f = struct.unpack(">6I", e.read(fmt, 24))
    L.rec("RenderArea", (L.lab(e.r(3)), w, h) + f + (e.r(6) & MASK, e.r(7) & MASK))


def stub_glue(e):
    e.cl.rec("ErrorOutput", (e.r(12) & MASK, e.cl.lab(e.r(3))))


def stub_longjmp(e):
    e.cl.rec("LongJmp", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def glue(fn):
    """A glue import saves the TOC pointer at 0x14(r1) and the caller reloads it afterwards."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


STUBS.internal(UI_SEND, stub_ui_send)
STUBS.internal(CAPTURE_KB, stub_capture)
STUBS.internal(STR_CTOR, glue(stub_str_ctor))
STUBS.internal(REG_GLOBAL, glue(stub_register))
STUBS.internal(RENDER_AREA, glue(stub_render))
STUBS.internal(PTR_GLUE, glue(stub_glue))
STUBS.internal(LONGJMP, stub_longjmp)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.cinfo = e.alloc(16)
        self.err = e.alloc(0x100)
        fixed = {ADDR["string"]: 1, ADDR["node"]: 2, ADDR["preview"]: 3, self.cinfo: 4, self.err + 0x84: 5}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, args=()):
        T.call_ppc(self.e, addr, list(args))


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.cinfo = lib.tpw_cpc_alloc(16)
        self.err = lib.tpw_cpc_alloc(0x100)
        fixed = {1: self.g("string"), 2: self.g("node"), 3: self.g("preview"), 4: self.cinfo, 5: self.err + 0x84}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cpc_addr(name.encode())

    def off(self, a):
        return self.lib.tpw_cpc_offset(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, addr, args=()):
        lib = self.lib
        if addr == PRE:
            lib.tpw_cpc_pre()
        elif addr == REINSTATE:
            lib.tpw_cpc_reinstate()
        elif addr == LOCK:
            lib.tpw_cpc_lock()
        elif addr == SINIT:
            lib.tpw_cpc_sinit()
        else:
            lib.tpw_cpc_exit(args[0])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, arg in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("offset", C.c_uint32, [C.c_uint64])):
        f = getattr(lib, "tpw_cpc_" + n)
        f.restype, f.argtypes = res, arg
    lib.tpw_cpc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cpc_exit.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cpc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("w80"), [case["w80"]])
    s.words(s.g("w84"), [case["w84"]])
    s.words(s.g("window"), [case["window"]])
    s.words(s.g("panel"), [case["panel"]])
    s.write(s.g("toggle"), bytes([case["toggle"]]))
    s.write(s.g("shown"), bytes([case["shown"]]))
    s.words(s.cinfo, [s.off(s.err)])
    s.words(s.err + 8, [case["vec"]])


def observe(s, case):
    return {"log": s.log(), "w80": s.rd_word(s.g("w80")), "w84": s.rd_word(s.g("w84")), "window": s.rd_word(s.g("window")),
            "panel": s.rd_word(s.g("panel")), "toggle": s.read(s.g("toggle"), 1), "shown": s.read(s.g("shown"), 1),
            "vec": s.rd_word(s.err + 8)}


def run(s, case):
    try:
        build(s, case)
        addr = case["fn"]
        s.call(addr, [s.cinfo] if addr == EXIT else [])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def word(r):
    return r.choice([0, 1, 0x1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(1, 1 << 32), r.randrange(0x1000, 0x2000000)])


def base(r, fn):
    c = {"fn": fn, "queues": {}, "w80": r.choice([0, word(r)]), "w84": word(r), "window": word(r), "panel": word(r),
         "toggle": r.choice([0, 1, 2, 0xFF]), "shown": r.choice([0, 1, 2, 0xFF]), "vec": word(r)}
    if r.random() < 0.4:
        c["w84"] = c["panel"]          # the equal branch of ChatReinstateAfterPostcard
    if r.random() < 0.1:
        c["panel"] = c["window"]
    return c


def edges(fn):
    def gen():
        r = random.Random(fn)
        for w80 in (0, 5):
            for shown in (0, 1, 0xFF):
                for same in (False, True):
                    c = base(r, fn)
                    c["w80"], c["shown"] = w80, shown
                    c["w84"] = c["panel"] if same else (c["panel"] ^ 4)
                    yield c
        for z in (0, 1):
            c = base(r, fn)
            c["panel"] = c["w84"] = c["window"] = 0
            c["w80"] = z
            yield c
    return gen


def rnd(fn):
    return lambda r: base(r, fn)


TESTS = [FnTest(name, ppc, host, edge=edges(fn), rnd=rnd(fn)) for name, fn in (
    ("ChatPrePostCard", PRE), ("ChatReinstateAfterPostcard", REINSTATE), ("LockChatPanelOff", LOCK),
    ("__sinit_ui_onlineOutbox_cpp", SINIT), ("my_error_exit", EXIT))]
