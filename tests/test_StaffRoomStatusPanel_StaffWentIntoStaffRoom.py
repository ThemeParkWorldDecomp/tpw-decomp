"""Unit v5291, the staff room panel's member functions, against libtpw_unit_StaffRoomStatusPanel_StaffWentIntoStaffRoom.dylib
(tests/unit_StaffRoomStatusPanel_StaffWentIntoStaffRoom.cpp), main app ("SimTheme Park"),
src/game/ui/ui_staffRoomStatus.cpp:
  StaffWentIntoStaffRoom 0x101c3a80, UpdateWindow 0x101c39b8, StaffLeftStaffRoom 0x101c3bbc, Show 0x101c4014,
  UpdateStatusWindow 0x101c3054, DeInitViewWindow 0x101c2db4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's arena (little-endian) for the translation. The callees outside the unit are fakes on both sides
that record their arguments: GetWindow, the CStaff queries, the list control's AddItem / FindData / RemoveItem, FillStaffRoomList,
OpenWindow, the advisor message, GetCurrentResidualValue, SCR_KillScript. The virtual calls (the panel's table at +0x20 through
__ptr_glue, the window's SetValue / SetText) are fakes too; their TVectors are distinct blocks.

The world of a case: the panel (window pointers, table, script words), the windows, a staff thing with a float at +0x1f8, the globals
(room id, staff type, the panel thing). What is compared: the event log, the script words, the thing Show stored.

No fused operations. fctiwz values stay below 2^31. Not tested: a NULL thing for StaffWentIntoStaffRoom (the original reads +0x1f8 of it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffRoomStatusPanel_StaffWentIntoStaffRoom"
MASK = 0xFFFFFFFF

F_WENT, F_UPDWIN, F_LEFT, F_SHOW, F_UPDSTAT, F_DEINIT = 0x101C3A80, 0x101C39B8, 0x101C3BBC, 0x101C4014, 0x101C3054, 0x101C2DB4
SIZES = {F_WENT: 240, F_UPDWIN: 140, F_LEFT: 100, F_SHOW: 76, F_UPDSTAT: 104, F_DEINIT: 80}
OWN = set(SIZES)

C_GETWINDOW, C_ISREST, C_STYPE, C_NAME, C_ADD = 0x101DBE40, 0x10126C1C, 0x10126718, 0x10126F98, 0x101D3000
C_FIND, C_REMOVE, C_FILL, C_OPEN, C_ADVISOR = 0x101D4384, 0x101D39F0, 0x101C31D4, 0x101935A0, 0x101821CC
C_RESIDUAL, C_KILL, C_GLUE = 0x1010FFC8, 0x100D9478, 0x1023BAA8

ADDR = dict(room=0x10395A90, stype=0x10395A98, showthing=0x10395A88, pthing=0x103947A4)
SCRIPT_ADDR, CALLBACK_TV = 0x102AE9C8, 0x10267F18
HOST_LAYOUT = [0x30, 0x1C, 0x24, 4, 2]

# persistent objects: 0 panel, 1 list window, 2 status window, 3 thing, 4 text window, 5 panel window, 6 status holder,
# 7 panel table, 8 to 10 the panel's TVectors (0x1c, 0x20, 0x3c), 11 window table, 12 and 13 the window's TVectors (SetValue, SetText)
SIZES_OBJ = [0x40, 0x120, 0x120, 0x200, 0x120, 0x120, 0x120, 0x40, 0x10, 0x10, 0x10, 0x40, 0x10, 0x10]
OBJ = []
TVNAME = {}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_getwindow(e):
    L = e.cl
    i = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), i))
    e.set_r(3, OBJ[1] if i == 0x1E7B else OBJ[2])


def st_isrest(e):
    L = e.cl
    L.rec("IsResting", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, L.ret("IsResting") & MASK)


def st_stype(e):
    L = e.cl
    L.rec("GetStaffType", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetStaffType") & MASK)


def st_name(e):
    L = e.cl
    L.rec("GetName", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetName") & MASK)


def st_add(e):
    L = e.cl
    item = e.r(4)
    L.rec("AddItem", (L.lab(e.r(3)), e.u32(item), s32(e.u32(item + 4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, L.ret("AddItem") & MASK)


def st_find(e):
    L = e.cl
    L.rec("FindData", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("FindData") & MASK)


def st_remove(e):
    L = e.cl
    L.rec("RemoveItem", (L.lab(e.r(3)), s32(e.r(4))))


def st_fill(e):
    L = e.cl
    L.rec("Fill", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_open(e):
    L = e.cl
    L.rec("OpenWindow", (L.lab(e.r(3)), int(e.r(4) == SCRIPT_ADDR), int(e.r(5) == CALLBACK_TV), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def st_advisor(e):
    e.cl.rec("Advisor", (s32(e.r(3)),))


def st_residual(e):
    L = e.cl
    L.rec("Residual", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Residual") & MASK)


def st_kill(e):
    e.cl.rec("KillScript", (s32(e.r(3)),))


def st_glue(e):
    """__ptr_glue: r12 is the TVector; the glue saves the TOC at 0x14(r1) like the real one."""
    L = e.cl
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    name = TVNAME[e.r(12)]
    if name == "SetValue":
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    elif name == "SetText":
        L.rec("SetText", (L.lab(e.r(3)), e.r(4)))
    else:
        L.rec(name, (L.lab(e.r(3)),))
        if name == "Slot3c":
            e.set_r(3, L.ret("Slot3c") & MASK)


for a, f in ((C_GETWINDOW, st_getwindow), (C_ISREST, st_isrest), (C_STYPE, st_stype), (C_NAME, st_name), (C_ADD, st_add),
             (C_FIND, st_find), (C_REMOVE, st_remove), (C_FILL, st_fill), (C_OPEN, st_open), (C_ADVISOR, st_advisor),
             (C_RESIDUAL, st_residual), (C_KILL, st_kill), (C_GLUE, st_glue)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)
    OBJ[:] = [T.persist_alloc(runner.emu, n) for n in SIZES_OBJ]
    TVNAME.clear()
    TVNAME.update({OBJ[8]: "Slot1c", OBJ[9]: "Slot20", OBJ[10]: "Slot3c", OBJ[12]: "SetValue", OBJ[13]: "SetText"})


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def put_i32(self, name, v):
        self.words(self.g(name), [v])


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({OBJ[i]: i + 1 for i in range(7) if i != 7}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, i):
        return OBJ[i]

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def put_ptr(self, name, a):
        self.words(self.g(name), [a])

    def rd_ptr(self, name):
        return self.rd_words(self.g(name), 1)[0]

    def put_room(self, v):
        self.write(self.g("room"), struct.pack(">HH", v, 0))

    def call(self, entry, args):
        T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i + 1: self.obj(i) for i in range(7)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sr_addr(name.encode())

    def obj(self, i):
        return self.lib.tpw_sr_obj(i)

    def ptr(self, a):
        return self.lib.tpw_sr_off(a)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, name, a):
        self.write(self.g(name), struct.pack("<Q", a))

    def rd_ptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def put_room(self, v):
        self.write(self.g("room"), struct.pack("<H", v))

    def call(self, entry, args):
        getattr(self.lib, HOSTFN[entry])(*args)


HOSTFN = {F_WENT: "tpw_sr_went", F_UPDWIN: "tpw_sr_updwin", F_LEFT: "tpw_sr_left", F_SHOW: "tpw_sr_show",
          F_UPDSTAT: "tpw_sr_updstat", F_DEINIT: "tpw_sr_deinit"}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sr_addr.restype, lib.tpw_sr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sr_obj.restype, lib.tpw_sr_obj.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sr_off.restype, lib.tpw_sr_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_sr_layout.argtypes = [C.POINTER(C.c_int64)]
    for name in HOSTFN.values():
        fn = getattr(lib, name)
        fn.restype = None
        fn.argtypes = [C.c_uint64] * (2 if name in ("tpw_sr_went", "tpw_sr_left", "tpw_sr_show") else 1)
    out = (C.c_int64 * 32)()
    n = lib.tpw_sr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.obj
    f = [0] * 12
    f[3] = s.ptr(o(5)) if case["win"] else 0
    f[4] = s.ptr(o(2) if False else o(6))
    f[7] = s.ptr(o(4))
    f[8] = s.ptr(o(7))
    f[9], f[10], f[11] = case["script"], case["f28"], case["f2c"]
    s.words(o(0), f)
    table = [0] * 16
    table[7], table[8], table[15] = s.ptr(o(8)), s.ptr(o(9)), s.ptr(o(10))
    s.words(o(7), table)
    wtab = [0] * 12
    wtab[9], wtab[11] = s.ptr(o(12)), s.ptr(o(13))
    s.words(o(11), wtab)
    for i in (1, 2, 4, 5, 6):
        s.words(o(i) + 0x118, [s.ptr(o(11))])
    s.words(o(3) + 0x1F8, [case["f"]])
    s.put_room(case["room"])
    s.put_i32("stype", case["stype"])
    s.put_ptr("pthing", o(3) if case["pthing"] else 0)
    s.put_ptr("showthing", 0)


def execute(s, case):
    t = s.obj(3) if case["thing"] == "obj" else case["thing"]
    entry = case["entry"]
    args = [s.obj(0)] + ([t] if entry in (F_WENT, F_LEFT, F_SHOW) else [])
    s.call(entry, args)


def observe(s, case):
    shown = s.rd_ptr("showthing")
    return {"log": s.log(), "script": s.rd_words(s.obj(0) + 0x24, 3), "show": 1 if shown == s.obj(3) else shown}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

FLOATS = [0.0, -0.0, 1.0, 99.9, 100.0, 127.5, 255.0, 256.0, 257.5, 1023.9, -1.0, -1.5, -255.0, 1e9, -1e9, 2e9, -2e9, 12345.678]


def rfloat_bits(r):
    k = r.random()
    if k < 0.3:
        return fb(r.choice(FLOATS))
    if k < 0.7:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-2e9, 2e9))


def base(r, entry):
    st = r.choice([0, 1, 2, 3, 4, 5, -1, 0x7FFFFFFF, r.randrange(0, 6)])
    match = r.random() < 0.6
    return {
        "entry": entry,
        "thing": "obj",
        "win": r.random() < 0.8,
        "room": r.choice([0, 1, 0xFFFF, r.randrange(0, 0x10000)]),
        "stype": st,
        "pthing": r.random() < 0.8,
        "f": rfloat_bits(r),
        "script": r.choice([0, 1, -1, r.randrange(-100, 1 << 20)]),
        "f28": r.randrange(0, 1 << 32),
        "f2c": r.randrange(0, 1 << 32),
        "queues": {
            "IsResting": [r.choice([0, 1, 1])],
            "GetStaffType": [st if match else (st ^ 1)],
            "GetName": [r.randrange(1, 1 << 31)],
            "AddItem": [r.randrange(0, 5)],
            "FindData": [r.choice([-1, 0, 1, 7, r.randrange(-3, 50)])],
            "Residual": [r.choice([0, 1, 100, 0x7FFFFFFF, 0x80000000, MASK, r.randrange(0, 1 << 32)])],
            "Slot3c": [r.choice([0, 0x1234, MASK, r.randrange(0, 1 << 32)])],
        },
    }


def edges_for(entry, seed):
    def gen():
        r = random.Random(seed)
        c = base(r, entry)
        yield c
        if entry == F_WENT:
            for win in (False, True):
                for rest in (0, 1):
                    for match in (False, True):
                        c = base(r, entry)
                        c["win"] = win
                        c["queues"]["IsResting"] = [rest]
                        c["queues"]["GetStaffType"] = [c["stype"] if match else c["stype"] ^ 1]
                        yield c
            for x in FLOATS:
                c = base(r, entry)
                c["win"] = True
                c["queues"]["IsResting"] = [1]
                c["queues"]["GetStaffType"] = [c["stype"]]
                c["f"] = fb(x)
                yield c
        elif entry == F_LEFT:
            for win in (False, True):
                for fd in (-1, 0, 1, 7):
                    c = base(r, entry)
                    c["win"] = win
                    c["queues"]["FindData"] = [fd]
                    yield c
                c = base(r, entry)
                c["win"], c["thing"] = win, 0
                yield c
        elif entry == F_SHOW:
            for t in ("obj", 0, 0x1234, 0xFFFFFFF0):
                c = base(r, entry)
                c["thing"] = t
                yield c
        elif entry == F_UPDSTAT:
            for pt in (False, True):
                for res in (0, 1, 100, 0x7FFFFFFF, 0x80000000, MASK):
                    c = base(r, entry)
                    c["pthing"] = pt
                    c["queues"]["Residual"] = [res]
                    yield c
        elif entry == F_DEINIT:
            for sc in (0, 1, -1, 0x7FFFFFFF, 0x80000000):
                c = base(r, entry)
                c["script"] = sc
                yield c
        elif entry == F_UPDWIN:
            for st in (0, 1, -1, 0x7FFFFFFF):
                c = base(r, entry)
                c["stype"] = st
                yield c
            for tx in (0, 1, MASK):
                c = base(r, entry)
                c["queues"]["Slot3c"] = [tx]
                yield c
    return gen


def rnd_for(entry):
    def f(r):
        c = base(r, entry)
        if entry == F_LEFT and r.random() < 0.1:
            c["thing"] = 0
        if entry == F_SHOW and r.random() < 0.3:
            c["thing"] = r.choice([0, r.randrange(1, 1 << 32)])
        return c
    return f


NAMES = [("StaffRoomStatusPanel::StaffWentIntoStaffRoom", F_WENT), ("StaffRoomStatusPanel::UpdateWindow", F_UPDWIN),
         ("StaffRoomStatusPanel::StaffLeftStaffRoom", F_LEFT), ("StaffRoomStatusPanel::Show", F_SHOW),
         ("StaffRoomStatusPanel::UpdateStatusWindow", F_UPDSTAT), ("StaffRoomStatusPanel::DeInitViewWindow", F_DEINIT)]

TESTS = [FnTest(n, ppc, host, edge=edges_for(a, 1000 + i), rnd=rnd_for(a)) for i, (n, a) in enumerate(NAMES)]
