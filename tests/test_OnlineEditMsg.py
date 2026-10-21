"""Unit v5658, the online panel's message functions, against libtpw_unit_OnlineEditMsg.dylib (tests/unit_OnlineEditMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_onlinePanel.cpp:
  OnlineEditMsg 0x101e71a8, OnlinePanelSubPanelMsg 0x101e70ec

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (key tables, CaptureKb, GetWindow, UI_SendMessage, setCursor, CKeyTable::Search, the mouse mode classes, AssignMouseMode,
COnlineControl, the string, CreateMainMenu, ContactMenu, the default handler, operator new and the three virtual calls through
__ptr_glue) are fakes on both sides that record their arguments and answer from the case's queues. A text is logged as a hash.

The world of a case: the edit text, the help text, the globals of the online panel, the mouse and the key table, windows as persistent
blocks. What is compared: the answer, the log, the two mouse words and where gMouse and gOnlineControl point (nothing, the old object or a new
one).

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineEditMsg"
MASK = 0xFFFFFFFF

FN_EDIT, FN_SUB = 0x101E71A8, 0x101E70EC
SIZES = {FN_EDIT: 1128, FN_SUB: 120}
OWN = set(SIZES)

ADDR = dict(gOnlineControl=0x10396D28, gMouseMode=0x102DC1F0, gOnlinePanelShown=0x10395E7A, gMouseWord1E0=0x102DC1E0,
            gMouseWord1E4=0x102DC1E4, gMouse=0x102DC1F4, gOnlineWindow=0x10395E94, gOnlinePanel=0x10395E98,
            gOnlineEmptyHelp=0x1024EF54, gOnlineBlockToggle=0x10395E79, gKeyTableShortcuts=0x1037F220, gOnlineEditText=0x10395E9C)
HOST_LAYOUT = []
BLOCKS = ["P", "A", "B", "G1", "G2", "G3", "KT", "MO", "MS", "CT", "VT1", "VT2"]
LABELS = {"P": 1, "A": 2, "B": 3, "G1": 4, "G2": 5, "G3": 6, "KT": 7, "MO": 8, "MS": 9, "CT": 10}
WIN = {}


def s32(v):
    return T.s32(v)


def hash16(chars):
    h = 0
    for ch in chars:
        h = (h * 31 + ch) & 0x7FFFFFFF
    return h


def read16(e, addr):
    out = []
    for i in range(300):
        ch = struct.unpack(">H", e.read(addr + 2 * i, 2))[0]
        if ch == 0:
            break
        out.append(ch)
    return out


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def keep_toc(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def rec_fn(name, spec, ret=None):
    return CL.recorder(name, spec, ret)


STUBS.internal(0x10182364, rec_fn("Disable", ""))
STUBS.internal(0x10182304, rec_fn("Enable", ""))
STUBS.internal(0x101DCDB0, rec_fn("UnCaptureKb", "Li"))
STUBS.internal(0x101DCCEC, rec_fn("CaptureKb", "Li"))
STUBS.internal(0x101C9518, rec_fn("UI_SendMessage", "Liii"))
STUBS.internal(0x101DD7F0, rec_fn("setCursor", "Lh"))
STUBS.internal(0x1014FD68, rec_fn("Search", "Lhh", "i"))
STUBS.internal(0x1007EA78, rec_fn("CMMDefault", "N"))
STUBS.internal(0x1007FAAC, rec_fn("CMMOnline", "N"))
STUBS.internal(0x1007E2E0, rec_fn("AssignMouseMode", "LL"))
STUBS.internal(0x10213870, rec_fn("COnlineControl", "N"))
STUBS.internal(0x101FC05C, rec_fn("CreateMainMenu", "i"))
STUBS.internal(0x101E8B98, rec_fn("ContactMenu", "i"))
STUBS.internal(0x101DE318, rec_fn("CallDefault", "Liii", "i"))


def stub_get_window(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    name = "G1" if wid == 0xC3123 else "G2" if wid == 0xC312B else "G3"
    e.set_r(3, WIN[name])


def stub_new(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("new", (size,))
    e.set_r(3, e.alloc(max(size, 1)))
    keep_toc(e)


def stub_glue(e):
    L = e.cl
    which = e.r(12) & MASK
    if which == 1:
        L.rec("GetMode", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("GetMode") & MASK)
    else:
        L.rec("SetText", (L.lab(e.r(3)), hash16(read16(e, e.r(4)))))
    keep_toc(e)


def stub_send_text(e):
    L = e.cl
    ptr = e.u32(e.r(4))
    L.rec("SendText", (L.lab(e.r(3)), hash16(read16(e, ptr))))


def stub_string_ctor(e):
    e.write(e.r(3), struct.pack(">I", e.r(4) & MASK))


def stub_string_dtor(e):
    pass


STUBS.internal(0x101E71A8 - 0x101E71A8 + 0x101DBE40, stub_get_window)
STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1023BAA8, stub_glue)
STUBS.internal(0x10217B5C, stub_send_text)
STUBS.internal(0x10146A14, stub_string_ctor)
STUBS.internal(0x101468F8, stub_string_dtor)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name in BLOCKS:
        WIN[name] = T.persist_alloc(runner.emu, 0x140)
        ADDR[name] = WIN[name]


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        """A pointer global: 4 bytes on the Mac, 8 on the host (host addresses are wider than 32 bits)."""
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {WIN[n]: lab for n, lab in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [x & MASK for x in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {lab: self.g(n) for n, lab in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_online_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = self.lib.tpw_online_edit if fn == FN_EDIT else self.lib.tpw_online_sub
        return f(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_online_addr.restype, lib.tpw_online_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_online_alloc.restype, lib.tpw_online_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_online_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("tpw_online_edit", "tpw_online_sub"):
        f = getattr(lib, n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_online_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def build(s, case):
    s.write(s.g("gOnlineEditText"), struct.pack(s.fmt[0] + "256H", *(case["text"] + [0] * (256 - len(case["text"])))))
    s.write(s.g("gOnlineEmptyHelp"), struct.pack(s.fmt[0] + "2H", *case["help"]))
    s.write(s.g("gOnlineBlockToggle"), bytes([case["block"]]))
    s.write(s.g("gOnlinePanelShown"), bytes([case["shown"]]))
    s.ptr(s.g("gOnlinePanel"), s.g("A"))
    s.ptr(s.g("gOnlineWindow"), s.g("B"))
    s.ptr(s.g("gKeyTableShortcuts"), s.g("KT"))
    s.ptr(s.g("gMouseMode"), s.g("MO") if case["mode"] else 0)
    s.ptr(s.g("gMouse"), s.g("MS") if case["mouse"] else 0)
    s.ptr(s.g("gOnlineControl"), s.g("CT") if case["control"] else 0)
    s.words(s.g("gMouseWord1E0"), [case["w0"]])
    s.words(s.g("gMouseWord1E4"), [case["w1"]])
    s.words(s.g("P") + 0x118, [s.g("VT2")])
    s.words(s.g("VT2") + 0x2C, [2])
    s.words(s.g("MO"), [s.g("VT1")])
    s.words(s.g("VT1") + 0x2C, [1])


def where(s, name, initial):
    v = s.rd_ptr(s.g(name))
    if v == 0:
        return "null"
    return "old" if v == s.g(initial) else "new"


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "w": s.rd_words(s.g("gMouseWord1E0"), 1) + s.rd_words(s.g("gMouseWord1E4"), 1),
            "mouse": where(s, "gMouse", "MS"), "control": where(s, "gOnlineControl", "CT")}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["fn"], [s.g("P"), case["msg"], case["a"], case["b"]]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def world(r, fn, msg, a, b):
    text = [r.choice([0x41, 0x20, 0x263A, 0xFFFF, 0x7F]) for _ in range(r.choice([0, 1, 5, 40, 255]))]
    return {"fn": fn, "msg": msg & MASK, "a": a & MASK, "b": b & MASK, "text": text,
            "help": [r.choice([0, 0x42]), 0],
            "block": r.choice([0, 0, 1, 2]), "shown": r.choice([0, 1, 2]), "mode": r.choice([0, 1, 1]),
            "mouse": r.choice([0, 1]), "control": r.choice([0, 1]), "w0": r.randrange(1 << 32), "w1": r.randrange(1 << 32),
            "queues": {"Search": [r.choice([0, 0, 1, 0x10000, 0x8000, -1, 0x20000])],
                       "GetMode": [r.choice([9, 9, 1, 0, 7])],
                       "CallDefault": [r.randrange(1 << 32) - (1 << 31)]}}


MSGS = [0x20, 0x1F, 0x1E, 0x21, 0x1000B, 0x1000A, 0x1000C, 0x402, 0, -1, 0x10020]
KEYS = [0xD, 9, 0x73, 0xC, 0xE, 0x72, 0x74, 0x41, 0, -1, 0x10D, 0x109, 0x173]
MODS = [0, 1, 0x7F, 0xFF, 0x100, 0x8000]


def b_of(r, mod):
    return ((mod & 0xFF) << 16) | (r.randrange(1 << 16)) | (r.choice([0, 0, 1]) << 24)


def edit_edges():
    r = random.Random(5658)
    for m in MSGS:
        for a in KEYS:
            for mod in ([0, 1] if m == 0x1000B else [0]):
                for _ in range(2 if m == 0x1000B else 1):
                    yield world(r, FN_EDIT, m, a, b_of(r, mod))
    for m in (0x1000B,):
        for a in (0xD, 9, 0x73, 0x41):
            for mod in MODS:
                for shown in (0, 1, 2):
                    for block in (0, 1):
                        for mode in (0, 1):
                            for mouse in (0, 1):
                                for gm in (9, 1):
                                    c = world(r, FN_EDIT, m, a, b_of(r, mod))
                                    c["shown"], c["block"], c["mode"], c["mouse"] = shown, block, mode, mouse
                                    c["queues"]["GetMode"] = [gm]
                                    yield c


def edit_random(r):
    m = r.choice([0x1000B, 0x1000B, 0x1000B] + MSGS)
    a = r.choice(KEYS + [r.randrange(256)])
    return world(r, FN_EDIT, m, a, b_of(r, r.choice(MODS)))


def sub_edges():
    r = random.Random(7)
    for m in (0x402, 0x401, 0x403, 0, -1, 0x10402):
        yield world(r, FN_SUB, m, r.randrange(1 << 32), r.randrange(1 << 32))


def sub_random(r):
    return world(r, FN_SUB, r.choice([0x402, 0x402, r.randrange(1 << 32)]), r.randrange(1 << 32), r.randrange(1 << 32))


TESTS = [
    FnTest("OnlineEditMsg", ppc, host, edge=edit_edges, rnd=edit_random),
    FnTest("OnlinePanelSubPanelMsg", ppc, host, edge=sub_edges, rnd=sub_random),
]
