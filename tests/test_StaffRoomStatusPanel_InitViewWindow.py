"""Unit v51051, the staff, show and upgrade status panels' view-window functions and the staff panel's small free functions, against
libtpw_unit_StaffRoomStatusPanel_InitViewWindow.dylib (tests/unit_StaffRoomStatusPanel_InitViewWindow.cpp), main app, src/LCPostFormat_
StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_InitViewWindow.cpp: the 27 functions of the OPS table.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (CloseWindow,
GetDBInfo, CoolRect, the InterfaceWindow setters, the preview functions, operator new, the sprite constructor, GetFontTableEntry,
RemoveSlideoutPanel, UI_SetSelectedPeep, the CStaff calls, applytoallcallback) are fakes on both sides that record their arguments and
answer from the case's queues.

The world of a case: a panel block of 16 random words (the view windows at +0x14 and +0x18 are a window block or 0), a window block with a
random z, the thing global (a block or NULL), a ride block with a random type number, the staff panel globals. What is compared: the answer
of the functions that have one, the event log, the panel block (window pointers as a class), the window's z and the globals.

No fused operations. Not tested: nothing is skipped (the thing and the windows are NULL in half the cases; the ride record is never NULL
because the original reads it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffRoomStatusPanel_InitViewWindow"
MASK = 0xFFFFFFFF

# (name, address, size, answers)
OPS = [
    ("StaffRoomStatusPanel::InitViewWindow", 0x101c2d60, 24, False),
    ("StaffRoomStatusPanel::UpdateViewWindow", 0x101c2e44, 4, False),
    ("StaffRoomStatusPanel::DeInitStatusWindow", 0x101c3010, 4, False),
    ("StaffRoomStatusPanel::DeInitWindow", 0x101c3978, 4, False),
    ("StaffRoomStatusPanel::Remove", 0x101c4098, 36, False),
    ("ShowStatusPanel::Remove", 0x101c2abc, 36, False),
    ("_applytoall", 0x101c25c0, 48, True),
    ("ShowStatusPanel::DeInitWindow", 0x101c218c, 4, False),
    ("StaffStatusPanel::InitViewWindow", 0x101c41a0, 4, False),
    ("StaffStatusPanel::DeInitViewWindow", 0x101c41dc, 36, False),
    ("ShowStatusPanel::DeInitStatusWindow", 0x101c1808, 4, False),
    ("StaffStatusPanel::DeInitStatusWindow", 0x101c4950, 4, False),
    ("ShowStatusPanel::UpdateViewWindow", 0x101c0ea4, 164, False),
    ("ShowStatusPanel::DeInitViewWindow", 0x101c0e48, 36, False),
    ("StaffStatusPanel::DeInitWindow", 0x101c4cf0, 4, False),
    ("ShowStatusPanel::InitViewWindow", 0x101c0d24, 236, False),
    ("__sinit_ui_staffPanel_cpp", 0x101c0cdc, 28, False),
    ("StaffPanelUp", 0x101c0cac, 12, True),
    ("RemoveStaffPanel", 0x101c0c5c, 40, False),
    ("StaffStatusPanel::SetSelected", 0x101c5358, 44, False),
    ("fireme", 0x101c53b8, 40, True),
    ("StaffStatusPanel::ChangeName", 0x101c558c, 56, False),
    ("StaffStatusPanel::GetName", 0x101c55f8, 64, True),
    ("StaffStatusPanel::Remove", 0x101c5668, 36, False),
    ("UpgradeStatusPanel::InitViewWindow", 0x101c5c80, 4, False),
    ("UpgradeStatusPanel::DeInitViewWindow", 0x101c5cc0, 36, False),
    ("UpgradeStatusPanel::UpdateViewWindow", 0x101c5d20, 164, False),
]
OWN = set(o[1] for o in OPS)

ADDR = dict(thing=0x103947a4, state=0x10395a38, show=0x10395a58, w4c=0x10395a4c, w50=0x10395a50)
HOST_LAYOUT_HEAD = [0x24, 0x28, 0x2c]
PANEL_WORDS = 16
LAYOUT = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("new", (size,))
    e.set_r(3, e.alloc(size) if L.ret("new") else 0)


STUBS.internal(0x10193774, CL.recorder("CloseWindow", "L"))
STUBS.internal(0x101c261c, CL.recorder("applytoallcallback", "LL"))
STUBS.internal(0x10107cd4, CL.recorder("GetDBInfo", "L", ret=lambda e, log: e.rideblk))
STUBS.internal(0x101c7448, CL.recorder("CoolRectCtor", "L"))
STUBS.internal(0x101c74e0, CL.recorder("CoolRectDtor", "L"))
STUBS.internal(0x101dba4c, CL.recorder("GetClientRect", "LL"))
STUBS.internal(0x101dd8bc, CL.recorder("setZ", "Lh"))
STUBS.internal(0x101dc1ec, CL.recorder("Show", "Lh"))
STUBS.internal(0x101db624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x101da204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x101da118, CL.recorder("SetFont", "Lu"))
STUBS.internal(0x101da3ac, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101de130, CL.recorder("SetClientDrawText", "Lu"))
STUBS.internal(0x10181400, CL.recorder("ShowRidePreview", "Lui"))
STUBS.internal(0x101815d4, CL.recorder("DestroyRidePreview", ""))
STUBS.internal(0x10181864, CL.recorder("DestroySpritePreview", ""))
STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret="i"))
STUBS.internal(0x1018d11c, CL.recorder("SpriteCtor", "L"))
STUBS.internal(0x101a83d8, CL.recorder("RemoveSlideoutPanel", "h"))
STUBS.internal(0x101947a8, CL.recorder("SetSelectedPeep", "L"))
STUBS.internal(0x10125660, CL.recorder("SackMe", "L"))
STUBS.internal(0x10126f30, CL.recorder("SetName", "LS"))
STUBS.internal(0x10126f98, CL.recorder("GetName", "L", ret="i"))
STUBS.imp("__nw__FUl", stub_new)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def text(self, s):
        a = self.alloc(2 * len(s) + 2)
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in s) + b"\0\0")
        return a


class Ppc(Side):
    fmt = ">I"
    zoff = 0xdc
    rideoff = 0x78c

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["show"]: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def p32(self, a):
        return a & MASK

    def set_ride(self, a):
        self.e.rideblk = a

    def log(self):
        return self.L.log

    def call(self, op, a, b):
        r3, _, _ = T.call_ppc(self.e, OPS[op][1], [a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.zoff, self.rideoff = LAYOUT["z"], LAYOUT["ride"]
        CL.host_begin(lib, {1: self.g("show")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sv_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def p32(self, a):
        return self.lib.tpw_sv_offset(a) if a else 0

    def set_ride(self, a):
        self.ptr(self.g("ride"), a)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, a, b):
        return self.lib.tpw_sv_call(op, a, b) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sv_addr.restype, lib.tpw_sv_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sv_alloc.restype, lib.tpw_sv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sv_offset.restype, lib.tpw_sv_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sv_call.restype, lib.tpw_sv_call.argtypes = C.c_uint64, [C.c_int32, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sv_layout(out)
    assert list(out)[:3] == HOST_LAYOUT_HEAD, list(out)[:n]
    LAYOUT["z"], LAYOUT["ride"] = out[3], out[4]
    lib.bound = True


def build(s, case):
    s.words(s.g("state"), [case["state"]])
    s.words(s.g("w4c"), [case["w4c"]])
    s.words(s.g("w50"), [case["w50"]])
    win = s.alloc(0x400)
    s.write(win + s.zoff, struct.pack(s.fmt[0] + "h", case["z"]))
    ride = s.alloc(0x800)
    s.words(ride + s.rideoff, [case["ridenum"]])
    s.set_ride(ride)
    thing = s.alloc(0x40) if case["thing"] else 0
    s.ptr(s.g("thing"), thing)
    words = list(case["panel"])
    words[5] = s.p32(win) if case["w14"] else 0
    words[6] = s.p32(win) if case["w18"] else 0
    panel = s.alloc(4 * PANEL_WORDS)
    s.words(panel, words)
    name = s.text(case["name"])
    return panel, win, thing, name


def observe(s, case, answer, panel, win):
    words = s.rd_words(panel, PANEL_WORDS)
    for i in range(3, 7):    # the pointer slots (+0x0c to +0x18): the window or something else
        words[i] = 1 if words[i] == s.p32(win) else (0 if words[i] == 0 else 2)
    z = struct.unpack(s.fmt[0] + "h", s.read(win + s.zoff, 2))[0]
    return {"answer": answer, "log": s.log(), "panel": words, "z": z,
            "state": s.rd_words(s.g("state"), 1), "w4c": s.rd_words(s.g("w4c"), 1), "w50": s.rd_words(s.g("w50"), 1)}


def run(s, case):
    try:
        panel, win, thing, name = build(s, case)
        op = case["op"]
        a = {6: thing, 20: panel}.get(op, panel)
        answer = s.call(op, a, name)
        if not OPS[op][3]:
            answer = 0
        return observe(s, case, answer, panel, win)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


Z_EDGES = [0, 1, -1, -10, -11, 0x7FF4, 0x7FF5, 0x7FFF, -0x8000, 0x100]


def base(r, op, w14, w18, thing, new, z):
    return {
        "op": op, "w14": w14, "w18": w18, "thing": thing, "z": z,
        "panel": [r.getrandbits(32) for _ in range(PANEL_WORDS)],
        "ridenum": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]),
        "state": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]),
        "w4c": r.getrandbits(32), "w50": r.getrandbits(32),
        "name": "".join(r.choice("abcXYZ019 ") for _ in range(r.randrange(0, 12))),
        "queues": {"new": [new], "GetFontTableEntry": [r.choice([0, 5, 0x12345678])],
                   "GetName": [r.choice([0, 0x1000, 0x7FFFFFF0])]},
    }


def make_tests():
    tests = []
    for op in range(len(OPS)):
        def edges(op=op):
            r = random.Random(100 + op)
            for w14 in (0, 1):
                for w18 in (0, 1):
                    for thing in (0, 1):
                        for new in (0, 1):
                            yield base(r, op, w14, w18, thing, new, r.choice(Z_EDGES))
            for z in Z_EDGES:
                yield base(r, op, 1, 1, 1, 1, z)

        def rnd(r, op=op):
            return base(r, op, r.choice([0, 1, 1]), r.choice([0, 1, 1]), r.choice([0, 1, 1]), r.choice([0, 1, 1]),
                        r.choice(Z_EDGES + [r.randrange(-0x8000, 0x8000)]))

        tests.append(FnTest(OPS[op][0], ppc, host, edge=edges, rnd=rnd))
    return tests


TESTS = make_tests()
