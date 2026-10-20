"""Unit v5697, the interface's progress bar, against libtpw_unit_CreateUIProgressBar.dylib (tests/unit_CreateUIProgressBar.cpp), main app
("SimTheme Park"), src/InterfaceWindow_virtuals_CreateUIProgressBar.cpp:
  CreateUIProgressBar(wchar_t*) 0x101dffd8, CreateUIProgressBar(int) 0x101e01f0, UpdateUIProgressBar 0x101e0300

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (Build, GetWindow,
the draw object constructors, SetClientDrawObject, the font setters, SetClientDrawText, setZ, GetFontTableEntry, UIMain_GetString and the
three virtuals Enable, SetValue and SetText) are fakes on both sides that record their arguments (windows, fonts and scripts as labels,
texts as text). The virtuals are called through __ptr_glue on the Mac: the window's vtable holds TVector ids the glue fake maps to events.

The world of a case: gUIProgressBar (none, the bar window, another window), gUIProgressBarText, the text the interface string fake gives
and the text argument. What is compared: the log, the two globals. The operator new sizes (0x124, 0x19c) are not compared (the host uses
the compiler's sizes). No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateUIProgressBar"
MASK = 0xFFFFFFFF

FN_BIG, FN_MID, FN_UPD = 0x101DFFD8, 0x101E01F0, 0x101E0300
SIZES = {FN_BIG: 492, FN_MID: 120, FN_UPD: 92}
OWN = set(SIZES)

ADDR = dict(root=0x10395C24, bar=0x10395C6C, textval=0x10395C68)
SCRIPT, MESH, CALLBACK = 0x102AFE24, 0x1024EDA3, 0x10268490
TV = {0x1C: 0x70000001, 0x24: 0x70000002, 0x2C: 0x70000003}   # Enable, SetValue, SetText
P = {}   # persistent PowerPC objects, made in setup

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ret_const(key):
    return lambda e, log: P[key]


STUBS.internal(0x101DBE40, CL.recorder("GetWindow", "Li", ret=ret_const("child")))
STUBS.internal(0x101DEB78, CL.recorder("Build", "LLL", ret=ret_const("bar")))
STUBS.internal(0x101898C8, CL.recorder("HGradientCtor", "NL"))
STUBS.internal(0x1018D11C, CL.recorder("TextSpriteCtor", "NL"))
STUBS.internal(0x101DB624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x101DA2D8, CL.recorder("SetFontWrap", "Lii"))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret=ret_const("font")))
STUBS.internal(0x101DA118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101DA3AC, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101DE130, CL.recorder("SetClientDrawText", "LS"))
STUBS.internal(0x101DD8BC, CL.recorder("setZ", "Lh"))
STUBS.internal(0x10180468, CL.recorder("UIMain_GetString", "i", ret=ret_const("text")))

VIRTUALS = {
    TV[0x1C]: CL.recorder("Enable", "Lh"),
    TV[0x24]: CL.recorder("SetValue", "Li"),
    TV[0x2C]: CL.recorder("SetText", "LS"),
}


def stub_glue(e):
    VIRTUALS[e.r(12)](e)


STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    vt = bytearray(0x40)
    for off, tv in TV.items():
        vt[off:off + 4] = struct.pack(">I", tv)
    P["vt"] = T.persist_alloc(emu, 0x40, bytes(vt))
    for name in ("root", "bar", "child"):
        blk = bytearray(0x140)
        blk[0x118:0x11C] = struct.pack(">I", P["vt"])
        P[name] = T.persist_alloc(emu, 0x140, bytes(blk))
    P["font"] = T.persist_alloc(emu, 0x40, bytes(0x40))
    P["text"] = T.persist_alloc(emu, 0x100, bytes(0x100))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def put_text(self, text):
        self.write(self.g("text"), b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text) + b"\0\0")


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({P["root"]: 1, P["bar"]: 2, P["child"]: 3, SCRIPT: 4, MESH: 5, CALLBACK: 6, P["font"]: 7}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return P[name] if name == "text" else ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self, bar, textval):
        self.words(ADDR["root"], [P["root"]])
        self.words(ADDR["bar"], [{0: 0, 1: P["bar"], 2: P["child"]}[bar]])
        self.words(ADDR["textval"], [textval])

    def state(self):
        w = self.rd_word(ADDR["bar"])
        return {0: 0, P["bar"]: 1, P["child"]: 2}.get(w, 9)

    def call(self, op, arg):
        if op == "big":
            T.call_ppc(self.e, FN_BIG, [P["text"]])
        elif op == "mid":
            T.call_ppc(self.e, FN_MID, [arg & MASK])
        else:
            T.call_ppc(self.e, FN_UPD, [arg & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("root"), 2: self.g("bar"), 3: self.g("child"), 4: self.g("script"), 5: self.g("mesh"),
                            6: self.g("callback"), 7: self.g("font")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cpb_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self, bar, textval):
        self.lib.tpw_cpb_setup(bar, s32(textval))

    def state(self):
        return self.lib.tpw_cpb_state()

    def call(self, op, arg):
        if op == "big":
            self.lib.tpw_cpb_big(self.g("text"))
        elif op == "mid":
            self.lib.tpw_cpb_mid(s32(arg))
        else:
            self.lib.tpw_cpb_upd(s32(arg))


def s32(v):
    return T.s32(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cpb_addr.restype, lib.tpw_cpb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cpb_setup.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_cpb_state.restype = C.c_int32
    lib.tpw_cpb_big.argtypes = [C.c_uint64]
    lib.tpw_cpb_mid.argtypes = [C.c_int32]
    lib.tpw_cpb_upd.argtypes = [C.c_int32]
    lib.tpw_cpb_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 8)()
    assert lib.tpw_cpb_layout(out) == 0
    lib.bound = True


def run(s, case):
    try:
        s.put_text(case["text"])
        s.set_world(case["bar"], case["textval"])
        s.call(case["op"], case["arg"])
        return {"log": s.log(), "state": s.state(), "textval": s.rd_word(s.g("textval"))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, op, bar=None, arg=None, same=None):
    if arg is None:
        arg = r.choice([0, 1, -1, 100, 0x7FFFFFFF, -0x80000000, r.randrange(-300, 300), r.randrange(-1 << 31, 1 << 31)])
    textval = arg if same else r.choice([0, 1, -5, r.randrange(-1 << 31, 1 << 31)])
    if same is None and r.random() < 0.4:
        textval = arg
    text = "".join(chr(r.randrange(0x20, 0x7F)) for _ in range(r.choice([0, 1, 5, 20])))
    return {"op": op, "queues": {}, "bar": r.choice([0, 1, 2]) if bar is None else bar, "arg": arg, "textval": textval & MASK, "text": text}


def edges(op):
    def gen():
        r = random.Random({"big": 11, "mid": 12, "upd": 13}[op])
        for bar in (0, 1, 2):
            for same in (True, False):
                yield mk(r, op, bar, same=same)
        if op == "upd":
            for v in (0, 1, 99, 100, 101, -1, -100, 0x1FFFFF, 0x200000, 0x7FFFFFFF, -0x80000000, 50, 12345):
                for bar in (0, 1):
                    yield mk(r, op, bar, arg=v)
    return gen


def rnd(op):
    return lambda r: mk(r, op)


TESTS = [
    FnTest("CreateUIProgressBar@0x101dffd8", ppc, host, edge=edges("big"), rnd=rnd("big")),
    FnTest("CreateUIProgressBar@0x101e01f0", ppc, host, edge=edges("mid"), rnd=rnd("mid")),
    FnTest("UpdateUIProgressBar", ppc, host, edge=edges("upd"), rnd=rnd("upd")),
]
