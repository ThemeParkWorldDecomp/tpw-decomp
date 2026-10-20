"""Unit v5538, leaving the coaster edit mode and enabling its buttons, against libtpw_unit_EndCoasterEditMode.dylib
(tests/unit_EndCoasterEditMode.cpp), main app ("SimTheme Park"), src/ui_coasteredit_kb_EndCoasterEditMode.cpp:
  EndCoasterEditMode 0x1019cad0, CoasterButtonsEnabled 0x1019cd6c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit are fakes on both sides that record
their arguments: RemoveSlideoutPanel, HandleCoasterButtonClick, LLCC_SystemGetMode (answers from the case), LLCC_SystemSetMode,
SetCoasterEdit, UIMain_DecideKbCapture, CMMDefault's constructor, CMouse::AssignMouseMode, InterfaceWindow::GetWindow. The virtual
calls (the mouse mode's GetMode, the windows' Enable) go through fake tables on the PowerPC side (the glue __ptr_glue is faked and reads
the TVector in r12) and through fake classes on the host; both record the object and the argument.

The world of a case: gCoasterEditMode, the recording word of the action recorder, the mouse (NULL or a block), the mouse mode (NULL or
the fake), the two mouse words, the coaster object (byte +0x4c), the panel, the answers of LLCC_SystemGetMode and GetMode. What is
compared: the event log, the edit mode word, the object pointer being cleared, the mouse state (NULL, unchanged or newly made), both
mouse words and the mode pointer.

No fused operations. Not tested: CoasterButtonsEnabled with bit 2 set and no object (the original reads the object unconditionally).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_EndCoasterEditMode"
MASK = 0xFFFFFFFF

FN_END, FN_BTN = 0x1019CAD0, 0x1019CD6C
SIZES = {FN_END: 516, FN_BTN: 576}
OWN = set(SIZES)

CALLEES = {
    "RemoveSlideoutPanel": 0x101A83D8, "HandleCoasterButtonClick": 0x10058510, "LLCC_SystemGetMode": 0x100914F4,
    "CMMDefault": 0x1007EA78, "AssignMouseMode": 0x1007E2E0, "LLCC_SystemSetMode": 0x10090E18, "SetCoasterEdit": 0x10058758,
    "UIMain_DecideKbCapture": 0x1018116C, "GetWindow": 0x101DBE40,
}
PTR_GLUE = 0x1023BAA8
CODE_ENABLE, CODE_GETMODE = 0x11110001, 0x11110002

ADDR = dict(editmode=0x10394830, editobject=0x1039482C, panelptr=0x10394834, w1e0=0x102DC1E0, w1e4=0x102DC1E4, modeptr=0x102DC1F0,
            mouse=0x102DC1F4, recording=0x1037EC44 + 0x24)
HOST_LAYOUT = []
WIN_IDS = list(range(0x18, 0x22))

STUBS = CL.Stubs()
STUBS.real.update(OWN)
PP = {}      # persistent PowerPC objects, set in setup


def s32(v):
    return T.s32(v)


def stub_get_window(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, PP["win"].get(s32(e.r(4)), 0))


def stub_glue(e):
    """__ptr_glue: the TVector in r12; saves the TOC where the caller reloads it from."""
    L = e.cl
    code = e.u32(e.r(12))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
    if code == CODE_ENABLE:
        L.rec("Enable", (L.lab(e.r(3)), e.r(4) & 0xFF))
        e.set_r(3, 0)
    elif code == CODE_GETMODE:
        L.rec("GetMode", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("GetMode") & MASK)
    else:
        raise AssertionError("unknown glue target %x" % code)


STUBS.internal(CALLEES["RemoveSlideoutPanel"], CL.recorder("RemoveSlideoutPanel", "h"))
STUBS.internal(CALLEES["HandleCoasterButtonClick"], CL.recorder("HandleCoasterButtonClick", "uu"))
STUBS.internal(CALLEES["LLCC_SystemGetMode"], CL.recorder("LLCC_SystemGetMode", "", ret="i"))
STUBS.internal(CALLEES["CMMDefault"], CL.recorder("CMMDefault", "N"))
STUBS.internal(CALLEES["AssignMouseMode"], CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(CALLEES["LLCC_SystemSetMode"], CL.recorder("LLCC_SystemSetMode", "ib"))
STUBS.internal(CALLEES["SetCoasterEdit"], CL.recorder("SetCoasterEdit", "Li"))
STUBS.internal(CALLEES["UIMain_DecideKbCapture"], CL.recorder("UIMain_DecideKbCapture", ""))
STUBS.internal(CALLEES["GetWindow"], stub_get_window)
STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    vt = T.persist_alloc(emu, 0x40)
    tv_en = T.persist_alloc(emu, 8)
    emu.write(tv_en, struct.pack(">II", CODE_ENABLE, 0))
    emu.write(vt + 0x1C, struct.pack(">I", tv_en))
    PP["win"] = {}
    for i in WIN_IDS:
        w = T.persist_alloc(emu, 0x140)
        emu.write(w + 0x118, struct.pack(">I", vt))
        PP["win"][i] = w
    PP["panel"] = T.persist_alloc(emu, 0x140)
    mvt = T.persist_alloc(emu, 0x40)
    tv_gm = T.persist_alloc(emu, 8)
    emu.write(tv_gm, struct.pack(">II", CODE_GETMODE, 0))
    emu.write(mvt + 0x2C, struct.pack(">I", tv_gm))
    PP["mode"] = T.persist_alloc(emu, 16)
    emu.write(PP["mode"], struct.pack(">I", mvt))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {PP["panel"]: 50, PP["mode"]: 1}
        for i, w in PP["win"].items():
            fixed[w] = 200 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "panel":
            return PP["panel"]
        if name == "modeobj":
            return PP["mode"]
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, op, flags):
        if op == "end":
            T.call_ppc(self.e, FN_END, [])
        else:
            T.call_ppc(self.e, FN_BTN, [flags & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {50: self.g("panel"), 1: self.g("modeobj")}
        for i in WIN_IDS:
            fixed[200 + i] = self.g("win%d" % i)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ec_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ec_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, flags):
        if op == "end":
            self.lib.tpw_ec_end()
        else:
            self.lib.tpw_ec_buttons(flags & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ec_addr.restype, lib.tpw_ec_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ec_alloc.restype, lib.tpw_ec_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ec_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ec_end.restype, lib.tpw_ec_end.argtypes = None, []
    lib.tpw_ec_buttons.restype, lib.tpw_ec_buttons.argtypes = None, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ec_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def build(s, case):
    s.words(s.g("editmode"), [case["editmode"]])
    s.words(s.g("recording"), [case["recording"]])
    s.words(s.g("w1e0"), [case["w1e0"]])
    s.words(s.g("w1e4"), [case["w1e4"]])
    s.mouse0 = s.alloc(16) if case["mouse"] else 0
    s.put_ptr(s.g("mouse"), s.mouse0)
    s.put_ptr(s.g("modeptr"), s.g("modeobj") if case["modeobj"] else 0)
    obj = s.alloc(0x60)
    s.write(obj + 0x4C, bytes([case["b4c"]]))
    s.put_ptr(s.g("editobject"), obj if case["object"] else 0)
    s.put_ptr(s.g("panelptr"), s.g("panel") if case["panel"] else 0)


def observe(s, case):
    mouse = s.get_ptr(s.g("mouse"))
    state = "null" if mouse == 0 else ("same" if mouse == s.mouse0 else "new")
    return {
        "log": s.log(),
        "editmode": s.rd_word(s.g("editmode")),
        "object_set": s.get_ptr(s.g("editobject")) != 0,
        "panel_set": s.get_ptr(s.g("panelptr")) != 0,
        "mouse": state,
        "mode_set": s.get_ptr(s.g("modeptr")) != 0,
        "w1e0": s.rd_word(s.g("w1e0")),
        "w1e4": s.rd_word(s.g("w1e4")),
        "recording": s.rd_word(s.g("recording")),
    }


def run(s, case):
    try:
        build(s, case)
        s.call(case["op"], case.get("flags", 0))
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "editmode": r.choice([0, 1, 1, 2, MASK]), "recording": r.choice([0, 0, 1, 5]),
            "mouse": r.choice([0, 1]), "modeobj": r.choice([0, 1]), "w1e0": r.choice([0, 7, MASK]), "w1e4": r.choice([0, 9, MASK]),
            "b4c": r.choice([0, 1, 2, 0xFF]), "object": 1, "panel": r.choice([0, 1, 1, 1]), "flags": 0,
            "queues": {"LLCC_SystemGetMode": [r.choice([3, 0, 1, 2, 4])], "GetMode": [r.choice([1, 0, 2, -1])]}}


def end_case(r):
    c = base(r, "end")
    c["object"] = r.choice([0, 1])
    return c


def end_edges():
    r = random.Random(5381)
    for em, rec, sysmode, mouse, modeobj, gm in itertools.product([0, 1], [0, 1], [3, 0], [0, 1], [0, 1], [1, 0]):
        c = end_case(r)
        c.update(editmode=em, recording=rec, mouse=mouse, modeobj=modeobj)
        c["queues"] = {"LLCC_SystemGetMode": [sysmode], "GetMode": [gm]}
        yield c


def btn_case(r, flags=None):
    c = base(r, "buttons")
    c["flags"] = r.choice([0, MASK, 0x1FF, r.getrandbits(32), r.getrandbits(9)]) if flags is None else flags
    return c


def btn_edges():
    r = random.Random(77)
    flag_list = [0, MASK, 0x1FF, 0x200, 0xFFFFFE00] + [1 << i for i in range(10)] + [~(1 << i) & MASK for i in range(9)]
    for f in flag_list:
        c = btn_case(r, f)
        c.update(editmode=1, panel=1)
        yield c
    for b in (0, 1, 2, 0xFF, 0x80):
        c = btn_case(r, 2)
        c.update(editmode=1, panel=1, b4c=b)
        yield c
    c = btn_case(r, MASK)
    c.update(editmode=0, panel=1)
    yield c
    c = btn_case(r, MASK)
    c.update(editmode=1, panel=0)
    yield c


TESTS = [
    FnTest("EndCoasterEditMode", ppc, host, edge=end_edges, rnd=end_case),
    FnTest("CoasterButtonsEnabled", ppc, host, edge=btn_edges, rnd=btn_case),
]
