"""Unit v5978, the options panel's DoButton and OptionsPanelMsg, against libtpw_unit_DoButton_2.dylib (tests/unit_DoButton_2.cpp), main app
("SimTheme Park"), src/ui_mainpanel2_window_bumhole_DoScroll_3_DoButton_2.cpp:
  DoButton 0x101a9808, OptionsPanelMsg 0x101a9d94

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the settings getters and setters, DoScroll, StartScroll, SetOnOffText, GetWindow, UIMain_GetString, the two virtual calls on a
window, wcsncpy, wcscat, the default window handler ...) are fakes on both sides that record their arguments and answer from the case.

The world of a case: the settings block (0x3c bytes), the root window, the DirectDraw device count, the game flags, the lobby and the
game window. What is compared: the answer, the settings block, the globals, the string buffer and the event log.

No fused operations. Not tested: strings of 0x3f characters or more in the label helper (the original leaves the local array unterminated).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DoButton_2"
MASK = 0xFFFFFFFF

FN_BUTTON, FN_MSG = 0x101A9808, 0x101A9D94
SIZES = {FN_BUTTON: 1388, FN_MSG: 488}
OWN = set(SIZES)

CALLEES = {
    0x10165DDC: "GetSFX", 0x10165D9C: "GetMusic", 0x10165E1C: "GetSpeech", 0x10165E5C: "GetMovie",
}
PTR_GLUE = 0x1023BAA8
GET_WINDOW, GET_STRING, SET_ONOFF = 0x101DBE40, 0x10180468, 0x101A8E60
WCSNCPY, WCSCAT, SET_RENDER = 0x10238CB0, 0x10239370, 0x10239820

ADDR = dict(settings=0x1038071C, root=0x10394A14, buffer=0x10380C48, ddraw=0x103BC98C, mapwho=0x103BC908, lobby=0x102E4888,
            game=0x1037F134, gamewindow=0x103949E4, f0c=0x10394A0C, renderoptions=0x10394A10, running=0x10394A08)
TOC_IMPORTS = dict(ddraw=0x10260B28, mapwho=0x102607E4)
HOST_LAYOUT =[0x60, 8, 0x7A0, 0, 0x3C, 0x36, 0x3A]

ROOT, MAPWHO, GAMEWIN, LOBBYWIN, MSGWIN = 0x32000000, 0x32100000, 0x32000100, 0x32000200, 0x32000300
PPC_BASE = None          # set in setup: the persistent window objects (vtable word at +0x118)
HOST_BASE = 0x31000000
STRIDE = 0x140
BUF_CHARS = 0x100


def win_addr(side_ppc, level, wid):
    base = PPC_BASE if side_ppc else HOST_BASE
    return base + (level * 0x40 + (wid - 0x1D4C0)) * STRIDE


def label_map(side_ppc):
    m = {ROOT: 1, MAPWHO: 2, GAMEWIN: 3, LOBBYWIN: 4, MSGWIN: 5}
    for k, (level, wid) in enumerate(((0, 0x1D4C8), (0, 0x1D4CA), (0, 0x1D4D0), (0, 0x1D4D1), (0, 0x1D4D6), (1, 0x1D4C9))):
        m[win_addr(side_ppc, level, wid)] = 10 + k      # labels 100 and up are the constructors' new objects
    return m


def s32(v):
    return T.s32(v)


def utf16(e, a, limit=0x4000):
    out = []
    for i in range(limit):
        c = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def getter(name):
    def f(e):
        e.cl.rec(name, ())
        e.set_r(3, e.cl.ret(name) & MASK)
    return f


for _a, _n in CALLEES.items():
    STUBS.internal(_a, getter(_n))


def simple(addr, name, regs):
    def f(e):
        e.cl.rec(name, tuple(s32(e.r(r)) for r in regs))
    STUBS.internal(addr, f)


simple(0x101664FC, "SetRendererType", [4])
simple(0x101665C0, "SetVideoCard", [4])
simple(0x10165E9C, "UpdateSettings", [])
simple(0x10166158, "ResetSettings", [])
simple(0x101A8FB8, "DoScroll", [3, 4])
simple(0x101A96F8, "StartScroll", [3, 4])
simple(0x101AB904, "RemoveOptionsPanel", [])
simple(0x101499D0, "Unpause", [])
simple(0x101D8BA0, "UnPauseTimer", [])
simple(0x1018116C, "DecideKbCapture", [])
simple(0x101CAAAC, "EnableHelp", [])
simple(0x101CAA4C, "DisableHelp", [])
simple(0x1007AB60, "HighLightKill", [])


def stub_send(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


STUBS.internal(0x101C9518, stub_send)


def stub_render(e):
    L = e.cl
    L.rec("SetRenderOptions", (L.lab(e.r(3)), s32(e.r(4))))


def glue(fn):
    """An import's glue saves the caller's TOC at 0x14(r1), which the code reloads after the call."""
    def f(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return f


STUBS.internal(SET_RENDER, glue(stub_render))


def stub_onoff(e):
    e.cl.rec("SetOnOffText", tuple(s32(e.r(r)) for r in (3, 4)) + tuple(e.r(r) & 0xFF for r in (5, 6, 7)) + (s32(e.r(8)),))


STUBS.internal(SET_ONOFF, stub_onoff)


def stub_getwindow(e):
    L = e.cl
    this, wid = e.r(3), s32(e.r(4))
    L.rec("GetWindow", (L.lab(this), wid))
    level = 0 if this == ROOT else 1
    e.set_r(3, win_addr(True, level, wid))


STUBS.internal(GET_WINDOW, stub_getwindow)


def stub_getstring(e):
    wid = s32(e.r(3))
    e.cl.rec("GetString", (wid,))
    text = [ord("s")] + [ord("0123456789abcdef"[(wid >> sh) & 15]) for sh in (12, 8, 4, 0)] + [0]
    e.write(ADDR["buffer"], b"".join(struct.pack(">H", c) for c in text))
    e.set_r(3, ADDR["buffer"])


STUBS.internal(GET_STRING, stub_getstring)


def stub_wcsncpy(e):
    dst, src, n = e.r(3), e.r(4), e.r(5)
    chars = []
    for i in range(n):
        c = struct.unpack(">H", e.read(src + 2 * i, 2))[0]
        if c == 0:
            break
        chars.append(c)
    chars += [0] * (n - len(chars))
    e.write(dst, b"".join(struct.pack(">H", c) for c in chars))
    e.set_r(3, dst)


def stub_wcscat(e):
    dst, src = e.r(3), e.r(4)
    d = utf16(e, dst)
    s = utf16(e, src)
    e.write(dst + 2 * len(d), b"".join(struct.pack(">H", c) for c in s + [0]))
    e.set_r(3, dst)


STUBS.internal(WCSNCPY, glue(stub_wcsncpy))
STUBS.internal(WCSCAT, glue(stub_wcscat))


def stub_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


STUBS.internal(0x101DE318, stub_default)

SLOT_SETVALUE, SLOT_SETTEXT = 0xF0000024, 0xF000002C


def stub_glue(e):
    """The virtual call through the table at window +0x118: r12 holds the slot's target (a marker)."""
    L = e.cl
    target = e.r(12)
    if target == SLOT_SETVALUE:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
        e.set_r(3, L.ret("SetValue") & MASK)
    elif target == SLOT_SETTEXT:
        text = utf16(e, e.r(4), 0x100)
        h = 0
        for i, c in enumerate(text):
            h = (h + (i + 1) * c) & MASK
        L.rec("SetText", (L.lab(e.r(3)), len(text), h, 1 if e.r(4) == ADDR["buffer"] else 0))
    else:
        raise AssertionError("unknown glue target %x" % target)


STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    global PPC_BASE
    T.install_std_stubs(runner.emu)
    e = runner.emu
    table = T.persist_alloc(e, 0x40)
    e.write(table + 0x24, struct.pack(">I", SLOT_SETVALUE))
    e.write(table + 0x2C, struct.pack(">I", SLOT_SETTEXT))
    PPC_BASE = T.persist_alloc(e, 0x80 * STRIDE)
    for i in range(0x80):
        e.write(PPC_BASE + i * STRIDE + 0x118, struct.pack(">I", table))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptr(self, a, v):
        """A pointer to a block: four bytes on the Mac, eight on the host (the arena is not below 4 GB)."""
        self.write(a, struct.pack(">I", v) if self.ppc else struct.pack("<Q", v))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    ppc = True
    lobby_off = 4
    f3c_off = 0x3C
    dd_off = 0x784

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog(label_map(True), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name in TOC_IMPORTS:       # a data import: the loader moved the cell, the TOC slot holds its address
            return self.e.u32(TOC_IMPORTS[name])
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def button(self, i, on):
        T.call_ppc(self.e, FN_BUTTON, [i & MASK, on & MASK])
        return 0

    def msg(self, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_MSG, [w, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ppc = False
    lobby_off = 8
    f3c_off = 0x60
    dd_off = 0x7A0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {v: k for k, v in label_map(False).items()}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_db_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_db_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def button(self, i, on):
        self.lib.tpw_db_button(s32(i), s32(on))
        return 0

    def msg(self, w, m, a, b):
        return self.lib.tpw_db_msg(w, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_db_addr.restype, lib.tpw_db_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_db_alloc.restype, lib.tpw_db_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_db_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_db_button.restype, lib.tpw_db_button.argtypes = None, [C.c_int32, C.c_int32]
    lib.tpw_db_msg.restype, lib.tpw_db_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_db_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.write(s.g("settings"), case["settings"] if s.ppc else le(case["settings"]))
    s.words(s.g("root"), [ROOT])
    s.write(s.g("buffer"), bytes(0x200))
    dd = s.alloc(0x800)
    s.words(dd + s.dd_off, [case["devices"]])
    s.ptr(s.g("ddraw"), dd)
    s.words(s.g("mapwho"), [MAPWHO])
    if case["lobby"]:
        lb = s.alloc(0x40)
        s.words(lb + s.lobby_off, [LOBBYWIN])
        s.ptr(s.g("lobby"), lb)
    else:
        s.ptr(s.g("lobby"), 0)
    s.words(s.g("game") + s.f3c_off, [case["f3c"]])
    s.words(s.g("gamewindow"), [GAMEWIN])
    s.words(s.g("f0c"), [case["f0c"]])
    s.words(s.g("renderoptions"), [case["render"]])
    s.write(s.g("running"), bytes([case["running"]]))


def execute(s, case):
    if case["op"] == "button":
        return s.button(*case["args"])
    return s.msg(MSGWIN, *case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "settings": s.read(s.g("settings"), 0x3C) if s.ppc else le(s.read(s.g("settings"), 0x3C)), "root": s.rd_words(s.g("root"), 1),
            "buffer": s.read(s.g("buffer"), 0x200) if s.ppc else swap16(s.read(s.g("buffer"), 0x200)), "f0c": s.rd_words(s.g("f0c"), 1), "f3c": s.rd_words(s.g("game") + s.f3c_off, 1)}


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


def settings_bytes(r):
    b = bytearray()
    b += struct.pack(">I", r.choice([0, 1, 2, 0x80000000, r.randrange(0, 1 << 32)]))      # renderer
    b += struct.pack(">I", r.randrange(0, 4))                                              # resolution
    b += struct.pack(">I", r.choice([0, 1, 2, 3, MASK - 2, 0x7FFFFFFF, 0x80000000, 101, r.randrange(0, 1 << 32)]))   # detail
    b += struct.pack(">I", r.choice([0, 1, 2]))                                            # video card
    for _ in range(4):
        b += bytes([r.choice([0, 1]), 0, 0, 0]) + struct.pack(">I", r.randrange(0, 1 << 32))
    b += struct.pack(">I", r.randrange(0, 5))
    b += bytes([r.choice([0, 1]) for _ in range(7)]) + bytes([0])
    return bytes(b)


def swap16(b):
    return b"".join(b[i:i + 2][::-1] for i in range(0, len(b), 2))


def le(b):
    """The settings block as the host stores it: the integer words little-endian, the bool bytes as they are."""
    out = bytearray(b)
    for off in [0, 4, 8, 12, 0x14, 0x1C, 0x24, 0x2C, 0x30]:
        out[off:off + 4] = b[off:off + 4][::-1]
    return bytes(out)


def base(r, op):
    sb = settings_bytes(r)
    return {"op": op, "settings": sb, "devices": r.choice([0, 1, 2, 3, -1 & MASK]), "lobby": r.choice([0, 1]), "f3c": r.choice([0, 1, 2]),
            "f0c": r.randrange(0, 5), "render": r.choice([0, 0x21, r.randrange(0, 1 << 32)]), "running": r.choice([0, 1]),
            "queues": {"GetSFX": [r.randrange(0, 1 << 32)], "GetMusic": [r.randrange(0, 1 << 32)], "GetSpeech": [r.randrange(0, 1 << 32)],
                       "GetMovie": [r.randrange(0, 1 << 32)], "SetValue": [r.choice([0, 1, 0x10000, 0x10001, MASK, 0x8000])],
                       "Default": [r.randrange(0, 1 << 32)]}}


def fix_settings(c, host_side):
    return c


def button_case(r, i=None, on=None):
    c = base(r, "button")
    c["args"] = [r.choice([0x1D4C1 + k for k in range(29)] + [0x1D4C0, 0x1D4DE, 0, -1]) if i is None else i,
                 r.choice([0, 1, 2, -1, 5]) if on is None else on]
    return c


def button_edges():
    r = random.Random(11)
    for i in list(range(0x1D4C0, 0x1D4DF)) + [0, -1, 0x7FFFFFFF, 0x80000000, 0x1D4C1 - 0x10000, 0x1D4C1 + 0x10000]:
        for on in (0, 1, 2, -1):
            yield button_case(r, i, on)


def button_random(r):
    return button_case(r)


def msg_case(r, m=None, a=None, b=None):
    c = base(r, "msg")
    c["args"] = [r.choice([0x14, 0x100, 0x800, 0x801, 0x802, 0x13, 0x15, 0xFF, 0x101, 0x7FF, 0x803, 0, 0x10003]) if m is None else m,
                 r.choice([-1, -2, -3, 0, 1, 0x1D4C1, 0x1D4D8, 0x1D4C5, 5]) if a is None else a,
                 r.choice([0, 1, 2, -1, 77]) if b is None else b]
    return c


def msg_edges():
    r = random.Random(12)
    for m in (0x14, 0x100, 0x800, 0x801, 0x802, 0x13, 0x15, 0xFF, 0x101, 0x7FF, 0x803, 0, -1, 0x10003):
        for a in (-1, -2, -3, 0, 0x1D4C1, 0x1D4D8, 0x1D4D6):
            yield msg_case(r, m, a, r.choice([0, 1, 2]))
    for k in range(16):      # the message 0x14 with every combination of its flags
        c = msg_case(r, 0x14, 0, 0)
        c["f3c"], c["running"], c["lobby"] = k & 1, (k >> 1) & 1, (k >> 2) & 1
        sb = bytearray(c["settings"])
        sb[0x36] = (k >> 3) & 1
        c["settings"] = bytes(sb)
        yield c


def msg_random(r):
    return msg_case(r)


TESTS = [
    FnTest("DoButton", ppc, host, edge=button_edges, rnd=button_random),
    FnTest("OptionsPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
]
