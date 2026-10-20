"""Unit v5540, the message handler of the coaster edit panel, against libtpw_unit_CoasterPanelMsg.dylib (tests/unit_CoasterPanelMsg.cpp),
main app ("SimTheme Park"), src/ui_entryPricePanel_CoasterPanelMsg.cpp:
  CoasterPanelMsg 0x1019b2b8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (HandleCoasterButtonClick, the cell system, the key tables, the advisor, the mouse modes and their constructors, the default window
handler, the virtual GetMode of gMouseMode through the glue) are fakes on both sides that record their arguments and answer from the case.

The world of a case: the mouse, its mode object, the edit mode and busy flags, the panel pointer, the two key tables, the queues of the
fakes. What is compared: the answer, the globals, the event log. The jump table at 0x102a9610 is checked against the order the
translation uses.

No fused operations. Not tested: operator new answering NULL (the runtime never does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CoasterPanelMsg"
MASK = 0xFFFFFFFF

FN = 0x1019B2B8
TABLE = 0x102A9610
# the handler of each control 0x19 .. 0x21 (the jump table's words), the order src/ui_entryPricePanel_CoasterPanelMsg.cpp switches in
TABLE_WORDS = [0x1019B8B8, 0x1019BA10, 0x1019B7D0, 0x1019BB68, 0x1019BE24, 0x1019BF7C, 0x1019B5D8, 0x1019B6C0, 0x1019BCC0]

ADDR = dict(gMouseWord1E0=0x102DC1E0, gMouseWord1E4=0x102DC1E4, gMouseMode=0x102DC1F0, gMouse=0x102DC1F4,
            gKeyTableCoaster=0x1037F224, gKeyTableCamera=0x1037F22C, gCoasterEditBusy=0x10394828,
            gCoasterEditObject=0x1039482C, gCoasterEditMode=0x10394830, gCoasterPanel=0x10394834)
HOST_LAYOUT = [0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.add(FN)
STUBS.internal(0x10058510, CL.recorder("HandleCoasterButtonClick", "uu"))
STUBS.internal(0x100914F4, CL.recorder("LLCC_SystemGetMode", "", ret="i"))
STUBS.internal(0x10090E18, CL.recorder("LLCC_SystemSetMode", "ib"))
STUBS.internal(0x10091A5C, CL.recorder("LLCC_SystemSetRideTrack", "i"))
STUBS.internal(0x10088590, CL.recorder("LLCM_SetCurrentObject", "b"))
STUBS.internal(0x10058758, CL.recorder("SetCoasterEdit", "Li"))
STUBS.internal(0x1018116C, CL.recorder("UIMain_DecideKbCapture", ""))
STUBS.internal(0x1018227C, CL.recorder("UIMain_CancelAdvisorMessage", "i"))
STUBS.internal(0x101821CC, CL.recorder("UIMain_AdvisorMessage", "i"))
STUBS.internal(0x101A8C58, CL.recorder("UI_SetGameWindowCursor", "h"))
STUBS.internal(0x1014FDEC, CL.recorder("KeyDown", "Lhh", ret="i"))
STUBS.internal(0x1014FEF0, CL.recorder("KeyUp", "Lhh", ret="i"))
STUBS.internal(0x101DE318, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))
STUBS.internal(0x1007E2E0, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(0x1007EA78, CL.recorder("CMMDefault", "N"))
STUBS.internal(0x1007FD14, CL.recorder("CMMCoaster", "N"))
STUBS.internal(0x1007E6E4, CL.recorder("CMMPathDraw", "Ni"))


def stub_buttons(e):
    L = e.cl
    L.rec("GetButtons", ())
    e.write(e.r(3), struct.pack(">I", L.ret("GetButtons") & MASK))


def stub_ptr_glue(e):
    L = e.cl
    L.rec("GetMode", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetMode") & MASK)


STUBS.internal(0x10057278, stub_buttons)
STUBS.internal(0x1023BAA8, stub_ptr_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)
    got = [runner.emu.u32(TABLE + 4 * i) for i in range(9)]
    assert got == TABLE_WORDS, [hex(x) for x in got]


class Side:
    def ptr(self, a, v):
        self.write(a, struct.pack(self.ptrfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.ptrfmt, self.read(a, struct.calcsize(self.ptrfmt)))[0]

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    ptrfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.win, self.cam, self.coa, self.mouse, self.mode = (e.alloc(32) for _ in range(5))
        vt = e.alloc(0x40)
        e.write(self.mode, struct.pack(">I", vt))
        self.L = e.cl = CL.PpcLog({self.win: 1, self.cam: 2, self.coa: 3, self.mouse: 4, self.mode: 5}, case["queues"])
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

    def call(self, msg, a, b):
        r3, _, _ = T.call_ppc(self.e, FN, [self.win, msg & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ptrfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win, self.cam, self.coa, self.mouse = (lib.tpw_cpm_alloc(32) for _ in range(4))
        self.mode = lib.tpw_cpm_addr(b"fakemode")
        CL.host_begin(lib, {1: self.win, 2: self.cam, 3: self.coa, 4: self.mouse, 5: self.mode}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cpm_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, msg, a, b):
        return self.lib.tpw_cpm_call(self.win, s32(msg), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cpm_addr.restype, lib.tpw_cpm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cpm_alloc.restype, lib.tpw_cpm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cpm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cpm_call.restype, lib.tpw_cpm_call.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cpm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("gMouseWord1E0"), [case["w0"]])
    s.words(s.g("gMouseWord1E4"), [case["w4"]])
    s.ptr(s.g("gMouseMode"), s.mode if case["hasmode"] else 0)
    s.ptr(s.g("gMouse"), s.mouse if case["hasmouse"] else 0)
    s.ptr(s.g("gKeyTableCoaster"), s.coa)
    s.ptr(s.g("gKeyTableCamera"), s.cam)
    s.write(s.g("gCoasterEditBusy"), bytes([case["busy"]]))
    s.ptr(s.g("gCoasterEditObject"), case["editobj"])
    s.words(s.g("gCoasterEditMode"), [case["editmode"]])
    s.ptr(s.g("gCoasterPanel"), s.win if case["haspanel"] else 0)


def observe(s, case, answer):
    mouse = s.rd_ptr(s.g("gMouse"))
    return {"answer": answer, "log": s.log(),
            "w": s.rd_words(s.g("gMouseWord1E0"), 1) + s.rd_words(s.g("gMouseWord1E4"), 1),
            "mouse": 0 if mouse == 0 else (1 if mouse == s.mouse else 2),
            "busy": s.read(s.g("gCoasterEditBusy"), 1),
            "editobj": s.rd_ptr(s.g("gCoasterEditObject")),
            "editmode": s.rd_words(s.g("gCoasterEditMode"), 1),
            "panel": 0 if s.rd_ptr(s.g("gCoasterPanel")) == 0 else 1}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["msg"], case["a"], case["b"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MSGS = [0x101, 0x1D, 0x14, 0x1000A, 0x1000B, 0x1001D8, 0, 0x13, 0x15, 0x1C, 0x1E, 0x100, 0x102, 0x10009, 0x1000C, 0x1001D7,
        0x1001D9, 0x7FFFFFFF, -1, 0x10000, 0x20000]


def case(r, msg=None, a=None, b=None):
    return {"msg": r.choice(MSGS) if msg is None else msg,
            "a": r.choice([0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x18, 0x22, 1, 0, -1,
                           r.getrandbits(32)]) if a is None else a,
            "b": r.choice([0, 1, 2, -1, 0x00FF0000, 0x01230000, r.getrandbits(32)]) if b is None else b,
            "hasmouse": r.randrange(2), "hasmode": r.randrange(2), "w0": r.getrandbits(32), "w4": r.getrandbits(32),
            "busy": r.randrange(3), "editobj": r.choice([0, 0x1234]), "editmode": r.choice([0, 1, 5]),
            "haspanel": r.randrange(2),
            "queues": {"KeyDown": [r.randrange(2)], "KeyUp": [r.randrange(2)], "GetMode": [r.choice([0, 1, 3, 8])],
                       "LLCC_SystemGetMode": [r.choice([0, 3, 5])], "GetButtons": [r.getrandbits(32)],
                       "CallDefaultWindowHandler": [r.getrandbits(32)]}}


def edges():
    r = random.Random(540)
    for msg in MSGS:
        for _ in range(3):
            yield case(r, msg)
    for a in range(0x17, 0x24):
        for b in (0, 1, 2):
            for m in (0, 1):
                for gm in (0, 1, 3, 8):
                    c = case(r, 0x101, a, b)
                    c["hasmouse"], c["hasmode"] = m, 1
                    c["queues"]["GetMode"] = [gm]
                    c["queues"]["GetButtons"] = [r.choice([0, 0x200, 0x1FF, 0x2FF])]
                    yield c
                c["hasmode"] = 0
                yield c
    for a in (0, 1, 2, -1):
        yield case(r, 0x1D, a)
    for em in (0, 1):
        for hm in (0, 1):
            for hmode in (0, 1):
                for sm in (0, 3, 5):
                    for gm in (0, 1, 8):
                        c = case(r, 0x1001D8)
                        c.update(editmode=em, hasmouse=hm, hasmode=hmode)
                        c["queues"]["LLCC_SystemGetMode"] = [sm]
                        c["queues"]["GetMode"] = [gm]
                        yield c
    for kd in (0, 1):
        for msg in (0x1000A, 0x1000B):
            for b in (0, 0x00FF0000, 0xFFFFFFFF, 0x00800000):
                c = case(r, msg, r.choice([0x7FFF, 0x8000, 0xFFFF, 0x12345678, 5]), b)
                c["queues"]["KeyDown"] = [kd]
                c["queues"]["KeyUp"] = [kd]
                yield c


def rnd(r):
    return case(r)


TESTS = [FnTest("CoasterPanelMsg", ppc, host, edge=edges, rnd=rnd)]
