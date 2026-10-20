"""Unit v51038, the interface's mouse, research button, kid and staff room events and the error dialog's queue, against
libtpw_unit_UIInterface_CamcorderMode.dylib (tests/unit_UIInterface_CamcorderMode.cpp), main app ("SimTheme Park"),
src/UI_pump_UIInterface_CamcorderMode.cpp:
  UIInterface::CamcorderMode 0x101f0ffc, UIInterface::EnableResearchButton 0x101f10f8, UIInterface::StaffRoomEvent 0x101f0dcc,
  UIInterface::KidArrived 0x101f0d14, ErrorDlgCancel 0x101f1cd0, AddUIError 0x101f1df0,
  std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::~__cdeque 0x101f2520

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the mouse mode
constructors, AssignMouseMode, GAME_ASSERTMSG, the panels, the mutex, GetConstString, UIMain_ConvertMulti, the window's virtual SetText
through __ptr_glue, operator delete) are fakes on both sides that record their arguments; the deque's operator[] and pop_front and the
queue's push_back are faked on the PowerPC side only (the host uses std::deque), so the queue is compared by its size and contents.

The world of a case: the mouse, the game type object and its guard byte, the thing array's entries 0 and k, the error deque (a Mac deque
with a block table on the PowerPC side, a std::deque on the host), the dialog window with its table at +0x118, gText's table slot, a
block table object for the destructor. What is compared: the answer, the globals and the queue read back, the event log.

No fused operations. Not tested: a thing index of 0x2800 or more (the host's table is that long).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIInterface_CamcorderMode"
MASK = 0xFFFFFFFF

FN_CAM, FN_ENABLE, FN_STAFF, FN_KID, FN_CANCEL, FN_ADD, FN_DTOR = 0x101F0FFC, 0x101F10F8, 0x101F0DCC, 0x101F0D14, 0x101F1CD0, 0x101F1DF0, 0x101F2520
SIZES = {FN_CAM: 200, FN_ENABLE: 128, FN_STAFF: 168, FN_KID: 92, FN_CANCEL: 248, FN_ADD: 116, FN_DTOR: 104}
OWN = set(SIZES)

CHECK_PANEL, NEW_FPT, NEW_DEFAULT, ASSIGN_MODE = 0x10180514, 0x1007FE70, 0x1007EA78, 0x1007E2E0
ASSERT, KID_ADDED, WENT_IN, LEFT, MAIN_RESEARCH, GAMETYPE_CTOR = 0x1000DDE8, 0x101957B8, 0x101C3A80, 0x101C3BBC, 0x101A7830, 0x1016F5D8
LOCK, UNLOCK, GETSTR, CONVERT, PTR_GLUE, POP_FRONT, INDEX, PUSH_BACK, DELETE = (
    0x10200D8C, 0x10200DE0, 0x10238B78, 0x101803CC, 0x1023BAA8, 0x101F2074, 0x101F2004, 0x101F21AC, 0x10238218)

THING_ARRAY = 0x1034CBFC
DEQUE = 0x10396454
ADDR = dict(mutex=0x10396444, panel=0x10395A9C, storage=0x102B3AA0, guard=0x102B3AA4, mouse=0x102DC1F4, w1e0=0x102DC1E0,
            w1e4=0x102DC1E4, dialog=0x1039647C, gtext3=0x1037FF18 + 0x558)
HOST_LAYOUT = [4, 12]
KID_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"

STUBS = CL.Stubs()
STUBS.real.update(OWN)

STUBS.internal(CHECK_PANEL, CL.recorder("UIMain_CheckPanel", ""))
STUBS.internal(NEW_FPT, CL.recorder("CMMFirstPersonTarget", "N"))
STUBS.internal(NEW_DEFAULT, CL.recorder("CMMDefault", "N"))
STUBS.internal(ASSIGN_MODE, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "bs"))
STUBS.internal(KID_ADDED, CL.recorder("AllKidsPanel_KidAdded", "L"))
STUBS.internal(WENT_IN, CL.recorder("StaffWentIntoStaffRoom", "LL"))
STUBS.internal(LEFT, CL.recorder("StaffLeftStaffRoom", "LL"))
STUBS.internal(LOCK, CL.recorder("lock", "L"))
STUBS.internal(UNLOCK, CL.recorder("unlock", "L"))


def glue(fn):
    """A glue import: the caller reloads its TOC from 0x14(r1) afterwards, which the real glue stored there."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


def stub_main_research(e):
    e.cl.rec("MainPanel_EnableResearchButton", (1 if e.r(3) & 0xFF else 0,))   # the callee takes a bool


STUBS.internal(MAIN_RESEARCH, stub_main_research)
STUBS.internal(DELETE, glue(CL.recorder("delete", "L")))


def stub_gametype(e):
    e.cl.rec("CGameType", (e.cl.lab(e.r(3)),))
    e.write(e.r(3), struct.pack(">I", e.cl.ret("CGameType") & MASK))


def stub_getstr(e):
    e.cl.rec("GetConstString", (e.cl.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, e.cl.ret("GetConstString") & MASK)


def stub_convert(e):
    e.cl.rec("UIMain_ConvertMulti", (e.r(3) & MASK,))
    e.set_r(3, e.cl.ret("UIMain_ConvertMulti") & MASK)


def stub_ptr_glue(e):
    e.cl.rec("WindowSetText", (e.cl.lab(e.r(3)), e.r(4) & MASK, 1 if (e.r(12) & MASK) == e.desc else 0))


def stub_pop_front(e):
    d = e.r(3)
    e.write(d + 0x14, struct.pack(">I", (e.u32(d + 0x14) + 1) & MASK))
    e.write(d + 0x18, struct.pack(">I", (e.u32(d + 0x18) - 1) & MASK))


def stub_index(e):
    e.set_r(3, (e.blocktab + 4 * (e.r(4) & MASK)) & MASK)


def stub_push_back(e):
    rec = e.r(4)
    e.pushed.append((e.u32(rec), e.u32(rec + 4)))


STUBS.internal(GAMETYPE_CTOR, stub_gametype)
STUBS.internal(GETSTR, glue(stub_getstr))
STUBS.internal(CONVERT, stub_convert)
STUBS.internal(PTR_GLUE, glue(stub_ptr_glue))
STUBS.internal(POP_FRONT, stub_pop_front)
STUBS.internal(INDEX, stub_index)
STUBS.internal(PUSH_BACK, stub_push_back)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def tblidx(self, v):
        return self.tbl.index(v) if v in self.tbl else -1


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.tbl = [e.alloc(16) for _ in range(3)]
        self.win = e.alloc(0x120)
        self.mouse0 = e.alloc(16)
        self.x = [e.alloc(16), e.alloc(16)]
        fixed = {ADDR["mutex"]: 1, ADDR["panel"]: 2, self.win: 3, ADDR["storage"]: 4, self.mouse0: 5, self.x[0]: 6, self.x[1]: 7}
        for i, t in enumerate(self.tbl):
            fixed[t] = 10 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        e.pushed = []
        e.blocktab = 0
        e.desc = 0
        self.saved = CL.Stubs.activate(STUBS, e) if False else STUBS.activate(e)

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

    def log(self):
        return self.L.log

    def clear(self):
        pass

    def set_thing(self, i, v):
        self.words(THING_ARRAY + 0x14 * i, [v])

    def q_set(self, case):
        e = self.e
        bs, first, size, recs = case["bs"], case["first"], case["size"], case["recs"]
        nb = (first + size) // bs + 1
        tab = e.alloc(4 * nb)
        for b in range(nb):
            blk = e.alloc(8 * bs)
            for k in range(bs):
                t, i = recs[b * bs + k]
                self.words(blk + 8 * k, [self.tbl[t], i])
            self.words(tab + 4 * b, [blk])
        e.blocktab = tab
        self.words(DEQUE, [bs])
        self.words(DEQUE + 0x14, [first, size])
        vt = e.alloc(0x40)
        desc = e.alloc(8)
        self.words(self.win + 0x118, [vt])
        self.words(vt + 0x2C, [desc])
        e.desc = desc

    def q_size(self):
        return self.rd_word(DEQUE + 0x18)

    def q_items(self):
        return [(self.tblidx(t), i) for t, i in self.e.pushed]

    def call(self, op, case):
        e = self.e
        if op == "cam":
            T.call_ppc(e, FN_CAM, [0x10396440]); return 0
        if op == "enable":
            T.call_ppc(e, FN_ENABLE, [0x10396440, case["on"] & MASK]); return 0
        if op == "staff":
            T.call_ppc(e, FN_STAFF, [0x10396440, case["id"] & MASK, case["arriving"] & MASK]); return 0
        if op == "kid":
            T.call_ppc(e, FN_KID, [0x10396440, case["id"] & MASK]); return 0
        if op == "cancel":
            return T.call_ppc(e, FN_CANCEL, [0])[0] & MASK
        if op == "add":
            T.call_ppc(e, FN_ADD, [case["error"] & MASK, self.tbl[case["table"]] if case["table"] >= 0 else 0]); return 0
        if op == "dtor":
            T.call_ppc(e, FN_DTOR, [self.x[0] if case["self"] else 0, case["flag"] & MASK]); return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.tbl = [self.alloc(16) for _ in range(3)]
        self.win = self.alloc(0x120)
        self.mouse0 = self.alloc(16)
        self.x = [self.alloc(16), self.alloc(16)]
        lib.tpw_cam_set_high(self.x[1] >> 32)
        fixed = {1: self.g("mutex"), 2: self.g("panel"), 3: self.win, 4: self.g("storage"), 5: self.mouse0, 6: self.x[0], 7: self.x[1]}
        for i, t in enumerate(self.tbl):
            fixed[10 + i] = t
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cam_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cam_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def clear(self):
        self.lib.tpw_cam_clear()

    def set_thing(self, i, v):
        self.lib.tpw_cam_set_thing(i, v)

    def q_set(self, case):
        bs, first, size, recs = case["bs"], case["first"], case["size"], case["recs"]
        for k in range(size):
            t, i = recs[first + k]
            self.lib.tpw_cam_q_push(self.tbl[t], i)

    def q_size(self):
        return self.lib.tpw_cam_q_size()

    def q_items(self):
        out = []
        for k in range(self.lib.tpw_cam_q_size()):
            buf = (C.c_uint64 * 2)()
            self.lib.tpw_cam_q_at(k, buf)
            out.append((self.tblidx(buf[0]), buf[1]))
        return out

    def call(self, op, case):
        lib = self.lib
        if op == "cam":
            lib.tpw_cam_camcorder(); return 0
        if op == "enable":
            lib.tpw_cam_enable(case["on"] & MASK); return 0
        if op == "staff":
            lib.tpw_cam_staff((case["id"] >> 16) & 0xFFFF, T.s32(case["arriving"])); return 0
        if op == "kid":
            lib.tpw_cam_kid((case["id"] >> 16) & 0xFFFF); return 0
        if op == "cancel":
            return lib.tpw_cam_cancel() & MASK
        if op == "add":
            lib.tpw_cam_add(T.s32(case["error"]), self.tbl[case["table"]] if case["table"] >= 0 else 0); return 0
        if op == "dtor":
            lib.tpw_cam_dtor(self.x[0] if case["self"] else 0, T.s16(case["flag"])); return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    U64, U32, I32, I16 = C.c_uint64, C.c_uint32, C.c_int32, C.c_int16
    sigs = {
        "set_high": (None, [U64]), "addr": (U64, [C.c_char_p]), "alloc": (U64, [U32]), "set_thing": (None, [U32, U64]), "clear": (None, []),
        "q_push": (None, [U64, U32]), "q_size": (U32, []), "q_at": (None, [U32, C.POINTER(C.c_uint64)]),
        "camcorder": (None, []), "enable": (None, [U32]), "staff": (None, [U32, I32]), "kid": (None, [U32]),
        "cancel": (U32, []), "add": (None, [I32, U64]), "dtor": (None, [U64, I16]),
    }
    for name, (res, args) in sigs.items():
        f = getattr(lib, "tpw_cam_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_cam_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cam_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    op = case["op"]
    s.clear()
    if op == "cam":
        s.ptr(s.g("mouse"), s.mouse0 if case["mouse"] else 0)
        s.words(s.g("w1e0"), [case["w0"]])
        s.words(s.g("w1e4"), [case["w4"]])
    elif op == "enable":
        s.write(s.g("guard"), bytes([case["guard"] & 0xFF]))
        s.words(s.g("storage"), [case["type"]])
    elif op in ("kid", "staff"):
        s.set_thing(0, s.tbl[case["t0"]] if case["t0"] >= 0 else 0)
        idx = (case["id"] >> 16) & 0xFFFF
        if idx:
            s.set_thing(idx, s.tbl[case["tk"]] if case["tk"] >= 0 else 0)
    elif op == "cancel":
        s.ptr(s.g("dialog"), s.win)
        s.q_set(case)
    elif op == "add":
        s.ptr(s.g("gtext3"), s.tbl[case["gtext"]] if case["gtext"] >= 0 else 0)
    elif op == "dtor":
        s.words(s.x[0], [case["w0"], case["w4"], case["w8"], s.x[1] if case["wc"] else 0])


def observe(s, case, answer):
    op = case["op"]
    out = {"answer": answer, "log": s.log()}
    if op == "cam":
        m = s.rd_ptr(s.g("mouse"))
        out["mouse"] = "null" if m == 0 else ("same" if m == s.mouse0 else "new")
        out["w"] = s.rd_words(s.g("w1e0"), 1) + s.rd_words(s.g("w1e4"), 1)
    elif op == "enable":
        out["guard"] = s.read(s.g("guard"), 1)
        out["storage"] = s.rd_word(s.g("storage"))
    elif op == "cancel":
        out["size"] = s.q_size()
    elif op == "add":
        out["items"] = s.q_items()
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    return {"op": op, "queues": {"GetConstString": [r.randrange(1, 0x7FFFFFFF)], "UIMain_ConvertMulti": [r.randrange(1, 0x7FFFFFFF)],
                                 "CGameType": [r.choice([0, 1, 2, 3, 0xFFFFFFFF])]}}


# ---- CamcorderMode
def cam_case(r, mouse=None):
    c = base(r, "cam")
    c["mouse"] = r.choice([0, 1]) if mouse is None else mouse
    c["w0"], c["w4"] = r.randrange(1, MASK), r.randrange(1, MASK)
    return c


def cam_edges():
    r = random.Random(11)
    for m in (0, 1):
        yield cam_case(r, m)


# ---- EnableResearchButton
def enable_case(r, on=None, guard=None, typ=None):
    c = base(r, "enable")
    c["on"] = r.choice([0, 1, 2, 0x100, 0x1FF, 0xFFFFFF00, 0xABCD0001]) if on is None else on
    c["guard"] = r.choice([0, 1, 0x80, 0xFF, 5]) if guard is None else guard
    c["type"] = r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x20000]) if typ is None else typ
    return c


def enable_edges():
    r = random.Random(12)
    for on in (0, 1, 0x100, 0x1FF, 0xABCD0001):
        for guard in (0, 1, 0x80, 0xFF):
            for typ in (0, 2, 1, 3):
                yield enable_case(r, on, guard, typ)
    for q in (0, 1, 2, 3):
        c = enable_case(r, 1, 0, 0)
        c["queues"]["CGameType"] = [q]
        yield c


# ---- KidArrived, StaffRoomEvent
def thing_case(r, op, t0=None, idx=None, arriving=None):
    c = base(r, op)
    c["t0"] = r.choice([-1, 0, 1, 2]) if t0 is None else t0
    idx = r.choice([0, 1, 2, 0x27FF, r.randrange(0, 0x2800)]) if idx is None else idx
    c["id"] = (idx << 16) | r.randrange(0, 0x10000)
    c["tk"] = r.choice([-1, 0, 1, 2])
    c["arriving"] = r.choice([0, 1, -1, 5, 0x10000]) if arriving is None else arriving
    return c


def kid_edges():
    r = random.Random(13)
    for t0 in (-1, 0):
        for idx in (0, 1, 0x27FF, 77):
            for tk in (-1, 0, 2):
                c = thing_case(r, "kid", t0, idx)
                c["tk"] = tk
                yield c


def staff_edges():
    r = random.Random(14)
    for arriving in (0, 1, -1, 5, 0x10000):
        for t0 in (-1, 1):
            for idx in (0, 1, 0x27FF, 77):
                yield thing_case(r, "staff", t0, idx, arriving)


# ---- ErrorDlgCancel
def cancel_case(r, size=None):
    c = base(r, "cancel")
    c["bs"] = r.randrange(1, 5)
    c["size"] = r.choice([0, 1, 2, 3, 5]) if size is None else size
    c["first"] = r.randrange(0, 3 * c["bs"] + 1)
    nb = (c["first"] + c["size"]) // c["bs"] + 1
    c["recs"] = [(r.randrange(0, 3), r.randrange(0, MASK)) for _ in range(nb * c["bs"])]
    return c


def cancel_edges():
    r = random.Random(15)
    for size in (0, 1, 2, 5):
        for _ in range(4):
            yield cancel_case(r, size)
    for bs, first in ((1, 0), (1, 3), (4, 3), (4, 4), (4, 7), (2, 1)):
        c = cancel_case(r, 3)
        c["bs"], c["first"] = bs, first
        nb = (first + 3) // bs + 1
        c["recs"] = [(r.randrange(0, 3), r.randrange(0, MASK)) for _ in range(nb * bs)]
        yield c


# ---- AddUIError
def add_case(r, table=None, gtext=None):
    c = base(r, "add")
    c["error"] = r.choice([0, 1, -1, 0x4C, r.randrange(0, MASK)])
    c["table"] = r.choice([-1, 0, 1, 2]) if table is None else table
    c["gtext"] = r.choice([-1, 0, 1, 2]) if gtext is None else gtext
    return c


def add_edges():
    r = random.Random(16)
    for table in (-1, 0, 2):
        for gtext in (-1, 1):
            yield add_case(r, table, gtext)


# ---- ~__cdeque
def dtor_case(r, this=None, wc=None, flag=None):
    c = base(r, "dtor")
    c["self"] = r.choice([0, 1]) if this is None else this
    c["wc"] = r.choice([0, 1]) if wc is None else wc
    c["flag"] = r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, 0x18000, 0x10000]) if flag is None else flag
    c["w0"], c["w4"], c["w8"] = r.randrange(0, MASK), r.randrange(0, MASK), r.randrange(0, MASK)
    return c


def dtor_edges():
    r = random.Random(17)
    for this in (0, 1):
        for wc in (0, 1):
            for flag in (-1, 0, 1, 2, 0x7FFF, -0x8000, 0x10000):
                yield dtor_case(r, this, wc, flag)


TESTS = [
    FnTest("UIInterface::CamcorderMode", ppc, host, edge=cam_edges, rnd=lambda r: cam_case(r)),
    FnTest("UIInterface::EnableResearchButton", ppc, host, edge=enable_edges, rnd=lambda r: enable_case(r)),
    FnTest("UIInterface::StaffRoomEvent", ppc, host, edge=staff_edges, rnd=lambda r: thing_case(r, "staff")),
    FnTest("UIInterface::KidArrived", ppc, host, edge=kid_edges, rnd=lambda r: thing_case(r, "kid")),
    FnTest("ErrorDlgCancel", ppc, host, edge=cancel_edges, rnd=lambda r: cancel_case(r)),
    FnTest("AddUIError", ppc, host, edge=add_edges, rnd=lambda r: add_case(r)),
    FnTest("std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::~__cdeque", ppc, host, edge=dtor_edges, rnd=lambda r: dtor_case(r)),
]
