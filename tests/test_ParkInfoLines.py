"""Unit v5232, the park status panel, against libtpw_unit_ParkInfoLines.dylib (tests/unit_ParkInfoLines.cpp), main app
("SimTheme Park"), src/game/ui/ui_parkStatus.cpp:
  ParkInfoLines 0x101abb90, ParkStatusPanelMsg 0x101abce4, DoDominantThoughts 0x101aba14

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetWindow, Show,
the analyser readers, GraphLine, the UIMain functions, the Create...Panel functions, RemoveParkStatusPanel, the glue call of the windows'
SetValue) are fakes on both sides that record their arguments (objects as labels, GraphLine's pointers as offsets) and answer from queues.

The world of a case: the flags, range, colours, the two shown counts, the panel pointer and the world pointer. What is compared: the answer,
the globals, the panel pointer and the event log.

No fused operations. Not tested: a window answered NULL where the original calls SetValue through it (a crash).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParkInfoLines"
MASK = 0xFFFFFFFF

FN_INFO, FN_MSG, FN_DOM = 0x101ABB90, 0x101ABCE4, 0x101ABA14
SIZES = {FN_INFO: 292, FN_MSG: 800, FN_DOM: 336}
OWN = set(SIZES)

ADDR = dict(flags=0x102ABBAC, world=0x1037EBFC, lastTotal=0x10394A18, lastInPark=0x10394A1C, colours=0x10394A6C, range=0x10394A88,
            panel=0x10394A8C)
HOST_LAYOUT = [5, 25]

ANALYSER = 0x30300000
# labels: 1..4 windows (made in setup), 5 world, 6 analyser, 7 bank account, 8 grapher, 9 string, 10 the window argument
OBJ_PPC = {5: 0x30100000, 6: ANALYSER, 7: 0x30200000, 8: 0x30400000, 9: 0x30500000, 10: 0x30600000}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ret(e, key):
    return e.cl.ret(key) & MASK


def reg(addr, name, spec, answer=None):
    """A recording fake: spec is a list of (kind) per register r3..: 'l' label, 'i' s32, 'u' u32, 'h' s16."""

    def fn(e):
        vals = []
        for n, k in enumerate(spec):
            r = e.r(3 + n)
            vals.append(e.cl.lab(r) if k == "l" else T.u32(r) if k == "u" else T.s16(r) if k == "h" else s32(r))
        e.cl.rec(name, tuple(vals))
        if answer:
            answer(e)

    STUBS.internal(addr, fn)


def a_obj(label):
    return lambda e: e.set_r(3, OBJ_PPC[label])


def a_queue(key):
    return lambda e: e.set_r(3, ret(e, key))


def a_getwindow(e):
    k = s32(ret(e, "GetWindow"))
    e.set_r(3, OBJ_PPC[k] if k else 0)


def a_three(e):
    for reg_no, key in ((4, "t1"), (5, "t2"), (6, "t3")):
        e.write(e.r(reg_no), bytes([ret(e, key) & 0xFF]))


def graph(name):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), s32(e.r(4) - ANALYSER), s32(e.r(5) - ADDR["colours"]), s32(e.r(6)), s32(e.r(7))))

    return fn


def glue(e):
    e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))     # the glue saves the TOC where the caller reloads it
    e.set_r(3, 1)


reg(0x101DBE40, "GetWindow", "li", a_getwindow)
reg(0x101DC1EC, "Show", "lh", lambda e: e.set_r(3, 1))
reg(0x1013EBC8, "GetParkAnalyser", "l", a_obj(6))
reg(0x1013EDFC, "GetBankAccount", "l", a_obj(7))
reg(0x100E7278, "NumberOfVisitorsInPark", "l", a_queue("InPark"))
reg(0x100E7994, "NumberOfVisitorsSinceBeginning", "l", a_queue("Total"))
reg(0x100E7E98, "Under", "li", a_queue("Under"))
reg(0x100E7D1C, "Over", "li", a_queue("Over"))
reg(0x100E5DC8, "GetThree", "l", a_three)
reg(0x100F3A40, "SetAdmissionFee", "lu")
reg(0x10180468, "GetString", "i", a_obj(9))
reg(0x10180794, "SetTitleText", "lil")
reg(0x1018227C, "CancelAdvisorMessage", "i")
reg(0x1018493C, "DefaultPanelMsg", "liii", a_queue("Default"))
reg(0x101952BC, "CreateAllKidsPanel", "l")
reg(0x101983C4, "CreateAllItemsPanel", "il")
reg(0x10199630, "CreateAllStaffPanel", "il")
reg(0x101ADA00, "RemoveParkStatusPanel", "")
STUBS.internal(0x101F6124, graph("GraphLineI"))
STUBS.internal(0x101F62CC, graph("GraphLineF"))
STUBS.internal(0x1023BAA8, glue)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vt = T.persist_alloc(e, 0x80)
    for label in range(1, 5):
        data = bytearray(0x140)
        struct.pack_into(">I", data, 0x118, vt)
        OBJ_PPC[label] = T.persist_alloc(e, 0x140, bytes(data))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({a: l for l, a in OBJ_PPC.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, label):
        return OBJ_PPC[label]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def put_ptr(self, name, label):
        self.words(ADDR[name], [OBJ_PPC[label] if label else 0])

    def get_ptr(self, name):
        w = self.rd_words(ADDR[name], 1)[0]
        rev = {a: l for l, a in OBJ_PPC.items()}
        return 0 if w == 0 else rev.get(w, -1)

    def info(self):
        T.call_ppc(self.e, FN_INFO, [OBJ_PPC[8]])
        return 0

    def msg(self, m, a, b):
        return T.call_ppc(self.e, FN_MSG, [OBJ_PPC[10], m & MASK, a & MASK, b & MASK])[0] & MASK

    def dom(self):
        T.call_ppc(self.e, FN_DOM, [])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {l: lib.tpw_pn_obj(l) for l in range(1, 11)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pn_addr(name.encode())

    def obj(self, label):
        return self.lib.tpw_pn_obj(label)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_ptr(self, name, label):
        self.write(self.g(name), struct.pack("<Q", self.obj(label) if label else 0))

    def get_ptr(self, name):
        (p,) = struct.unpack("<Q", self.read(self.g(name), 8))
        if p == 0:
            return 0
        for l in range(1, 11):
            if self.obj(l) == p:
                return l
        return -1

    def info(self):
        self.lib.tpw_pn_info(self.obj(8))
        return 0

    def msg(self, m, a, b):
        return self.lib.tpw_pn_msg(self.obj(10), s32(m), s32(a), s32(b)) & MASK

    def dom(self):
        self.lib.tpw_pn_dom()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pn_addr.restype, lib.tpw_pn_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pn_obj.restype, lib.tpw_pn_obj.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_pn_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pn_info.restype, lib.tpw_pn_info.argtypes = None, [C.c_uint64]
    lib.tpw_pn_msg.restype, lib.tpw_pn_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_pn_dom.restype, lib.tpw_pn_dom.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_pn_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("flags"), [case["flags"]])
    s.words(s.g("range"), [case["range"]])
    s.words(s.g("lastTotal"), [case["last_total"]])
    s.words(s.g("lastInPark"), [case["last_inpark"]])
    s.write(s.g("colours"), case["colours"])
    s.put_ptr("world", 5)
    s.put_ptr("panel", case["panel"])


def execute(s, case):
    if case["op"] == "info":
        return s.info()
    if case["op"] == "dom":
        return s.dom()
    return s.msg(*case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "flags": s.rd_words(s.g("flags"), 1), "range": s.rd_words(s.g("range"), 1),
            "last": s.rd_words(s.g("lastTotal"), 1) + s.rd_words(s.g("lastInPark"), 1), "panel": s.get_ptr("panel"),
            "colours": s.read(s.g("colours"), 25)}


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


# ---------------------------------------------------------------------------------------------------------------- cases

PERCENT = [0, 1, 32, 33, 50, 99, 100, 101, -1, 0x400000, 0x7FFFFF, -0x400000, 0x1FFFFF]
THOUGHTS = [0, 1, 2, 0x16, 0x17, 0x18, 0xFF]
MSGS = [0x15, 0x10, 5, 0x14, 0x13, 0x11, 0x100, 0x800, 0x1E, 0, 0x20, -1, 0x4, 0x6, 0xF, 0x101, 0x7FF, 0x801]
A_VALUES = [0x80083, 0x80082, 0x80084, 0, -1, 0x4A68C5, 0x4A68C6, 0x4A68C7, 0x4A68C8, 0x4A68C9, 0x4A68C4, 0x4A68C6 + 3, 0x7FFFFFFF]


def rint(r):
    return r.choice([0, 1, -1, 5, 100, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000), r.randrange(-(1 << 31), 1 << 31)])


def base(r, op):
    q = {"GetWindow": [r.choice([0, 1, 2, 3, 4])] + [r.randrange(1, 5) for _ in range(15)],
         "InPark": [rint(r)], "Total": [rint(r)], "Under": [r.choice(PERCENT)], "Over": [r.choice(PERCENT)],
         "Default": [rint(r)], "t1": [r.choice(THOUGHTS)], "t2": [r.choice(THOUGHTS)], "t3": [r.choice(THOUGHTS)]}
    return {"op": op, "queues": q, "flags": r.choice([r.randrange(0, 64), r.randrange(0, 1 << 32)]) & MASK,
            "range": rint(r) & MASK, "last_total": rint(r) & MASK, "last_inpark": rint(r) & MASK,
            "panel": r.choice([0, 1, 2, 3, 4]), "colours": bytes(r.randrange(256) for _ in range(25))}


def info_case(r, flags=None):
    c = base(r, "info")
    if flags is not None:
        c["flags"] = flags
    return c


def info_edges():
    r = random.Random(11)
    for f in list(range(32)) + [0xFFFFFFE0, 0xFFFFFFFF, 0x20, 0x40, 0x80000000]:
        yield info_case(r, f)


def info_random(r):
    return info_case(r)


def dom_case(r, t=None, panel=None, first=None):
    c = base(r, "dom")
    if t is not None:
        for k, v in zip(("t1", "t2", "t3"), t):
            c["queues"][k] = [v]
    if panel is not None:
        c["panel"] = panel
    if first is not None:
        c["queues"]["GetWindow"][0] = first
    return c


def dom_edges():
    r = random.Random(12)
    yield dom_case(r, panel=0)
    yield dom_case(r, (1, 2, 3), 1, 0)
    for a in THOUGHTS:
        for b in THOUGHTS:
            for c in THOUGHTS:
                yield dom_case(r, (a, b, c), r.randrange(1, 5), r.randrange(1, 5))


def dom_random(r):
    return dom_case(r)


def msg_case(r, m=None, a=None, b=None, same_in=None, same_total=None, panel=None):
    c = base(r, "msg")
    c["args"] = [r.choice(MSGS) if m is None else m, r.choice(A_VALUES) if a is None else a, rint(r) if b is None else b]
    if c["args"][0] == 0x10 and m is None and r.random() < 0.7:
        c["args"][1] = 0x80083
    if same_in is not None:
        c["last_inpark"] = c["queues"]["InPark"][0] & MASK if same_in else (c["queues"]["InPark"][0] + 1) & MASK
    if same_total is not None:
        c["last_total"] = c["queues"]["Total"][0] & MASK if same_total else (c["queues"]["Total"][0] + 1) & MASK
    if panel is not None:
        c["panel"] = panel
    return c


def msg_edges():
    r = random.Random(13)
    for m in MSGS:
        for a in A_VALUES:
            yield msg_case(r, m, a)
    for p in (0, 1, 3):
        for si in (True, False):
            for st in (True, False):
                for u in (0, 33, 100, -1, 0x400000):
                    c = msg_case(r, 0x10, 0x80083, 7, si, st, p)
                    c["queues"]["Under"] = [u]
                    c["queues"]["Over"] = [u]
                    yield c
    for p in PERCENT:
        c = msg_case(r, 0x10, 0x80083, 0, True, True, 2)
        c["queues"]["Under"] = [p]
        c["queues"]["Over"] = [p]
        yield c
    for b in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
        yield msg_case(r, 0x800, 0, b)


def msg_random(r):
    return msg_case(r)


TESTS = [
    FnTest("ParkInfoLines", ppc, host, edge=info_edges, rnd=info_random),
    FnTest("ParkStatusPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("DoDominantThoughts", ppc, host, edge=dom_edges, rnd=dom_random),
]
