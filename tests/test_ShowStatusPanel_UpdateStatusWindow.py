"""Unit v5547, the sideshow status panel, against libtpw_unit_ShowStatusPanel_UpdateStatusWindow.dylib
(tests/unit_ShowStatusPanel_UpdateStatusWindow.cpp), main app ("SimTheme Park"), src/ShowStatusPanel_UpdateStatusWindow.cpp:
  ShowStatusPanel::UpdateStatusWindow 0x101c1848, ApplyChanges 0x101c2508, applytoallcallback 0x101c261c, Show 0x101c2a38

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are fakes on both
sides that record their arguments and answer from the case's queues: the CObject getters and setters, the histories' sums (doubles), the
windows (GetWindow, and the window calls the original makes through the table at +0x118, __ptr_glue running as the original), FormatString
(which reads the two variable tags), the analyser, UIStatusPanel::OpenWindow and the advisor message.

The world of a case: the panel (state, parent window, the three ints at +0x24 +0x28 +0x2c, the table with the notify slot), the object (cell
bytes at +4 and +6, the type at +0x0e), a second object for applytoallcallback, the ride record's word at +0x10c, the queues. What is
compared: the log of every call, the panel's words, the Show global, the first characters of the format buffer.

No fused operations. The conversion routine at 0x10237cd8 runs as the original. Not tested: windows 0xa09d, 0xa0a0 and 0xa09e answering NULL
(the original reads their table at once).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowStatusPanel_UpdateStatusWindow"
MASK = 0xFFFFFFFF

FN_UPDATE, FN_APPLY, FN_ALL, FN_SHOW = 0x101C1848, 0x101C2508, 0x101C261C, 0x101C2A38
SIZES = {FN_UPDATE: 1036, FN_APPLY: 132, FN_ALL: 168, FN_SHOW: 80}
OWN = set(SIZES)

PTR_GLUE = 0x1023BAA8
# the "code addresses" the TVectors in the tables hold; the glue fake below dispatches on them
A_SETVALUE, A_SETTEXT, A_NOTIFY = 0x101DFE84, 0x101DE130, 0x101DCBA8
# stubbed callees: address -> (event name, argument spec, queue of the answer or None)
CALLEES = {
    0x10107CD4: ("GetDBInfo", "L", "ride"),
    0x1010FA40: ("GetPercentageOfWinners", "L", "u:GetPercentageOfWinners"),
    0x1010F8D4: ("GetProfitability", "L", "i:GetProfitability"),
    0x1010CBA0: ("GetProspectiveExcitementLevel", "Liii", "i:GetProspectiveExcitementLevel"),
    0x1010F9D0: ("GetCustomerSatisfaction", "L", "i:GetCustomerSatisfaction"),
    0x1010FFC8: ("GetCurrentResidualValue", "L", "u:GetCurrentResidualValue"),
    0x1013EBC8: ("GetParkAnalyser", "L", "analyser"),
    0x100EF00C: ("Happiness", "Lhu", "i:Happiness"),
    0x10180B84: ("GetObjectState", "Li", "i:GetObjectState"),
    0x101105F4: ("SetPricePerUse", "Li", None),
    0x1010FBF8: ("SetChanceOfWinning", "Lb", None),
    0x1010F564: ("SetPrizeValue", "Li", None),
    0x101935A0: ("OpenWindow", "LLLiii", None),
    0x101821CC: ("AdvisorMessage", "i", None),
    # the three targets of the window and panel calls through tables (real functions of the binary, used as addresses to hook)
    0x101DFE84: ("SetValue", "Li", None),
    0x101DCBA8: ("Notify", "L", None),
}
A_GETWINDOW, A_SUM, A_FORMAT = 0x101DBE40, 0x100F1000, 0x10239730

ADDR = dict(gStaffRoomPanelThing=0x103947A4, gGameWorld=0x1037EBFC, table_slot=0x1037FF18 + 0x564, buffer=0x10380C48,
            gShowStatusThing=0x10395A54, script=0x102AE3E8, callback=0x10267EA8, analyser=0x1037EE00)
IDS = [0xA09D, 0xA096, 0xA093, 0xA092, 0xA099, 0xA094, 0xA0A0, 0xA09E]
REQUIRED = {0xA09D, 0xA0A0, 0xA09E}
HOST_LAYOUT = [0x6C, 0x1A4, 0x22C, 0x0E, 4, 6, 0x448, 0x24]
ST = {}
TOC = 0x10267CC0      # r2 of the main code (TOC slot -0x6800 is at 0x102614c0)


def s32(v):
    return T.s32(v)


def make_stub(name, spec, ans):
    def stub(e):
        L = e.cl
        args = []
        for k, c in enumerate(spec):
            v = e.r(3 + k)
            if c == "L":
                args.append(L.lab(v))
            elif c == "i":
                args.append(s32(v))
            elif c == "u":
                args.append(v & MASK)
            elif c == "b":
                args.append(v & 0xFF)
            elif c == "h":
                args.append((v >> 16) & 0xFFFF)
        L.rec(name, tuple(args))
        if ans == "ride":
            e.set_r(3, ST["ride"])
        elif ans == "analyser":
            e.set_r(3, ADDR["analyser"])
        elif ans:
            e.set_r(3, L.ret(ans.split(":")[1]) & MASK)
    return stub


STUBS = CL.Stubs()
STUBS.real.update(OWN)
for _a, (_n, _s, _r) in CALLEES.items():
    if _a not in (A_SETVALUE, A_NOTIFY):
        STUBS.internal(_a, make_stub(_n, _s, _r))


SET_TEXT = CL.recorder("SetText", "LS")
TARGETS = {A_SETVALUE: make_stub(*CALLEES[A_SETVALUE]), A_SETTEXT: SET_TEXT, A_NOTIFY: make_stub(*CALLEES[A_NOTIFY])}


def stub_glue(e):
    """__ptr_glue: the call through the TVector in r12 (the code word at +0). The Mac's glue switches the TOC and the caller restores it;
    here the targets are the recording fakes above, called directly."""
    print("glue", hex(e.r(1)), hex(e.r(2)), hex(e.r(12)), hex(e.u32(e.r(12))))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the caller's TOC where the caller restores it from
    TARGETS[e.u32(e.r(12))](e)


STUBS.internal(PTR_GLUE, stub_glue)


def stub_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    on = L.ret("Win%d" % wid)
    e.set_r(3, ST["win"].get(wid, 0) if on else 0)


def stub_sum(e):
    L = e.cl
    off = (e.r(3) - ST["obj"]) & MASK
    L.rec("GetSumOfLast", (off, s32(e.r(5))))
    v = s32(L.ret("Sum%d" % off)) / 4.0
    e.write(e.r(4), struct.pack(">d", v))
    e.set_r(3, 0)


def stub_format(e):
    L = e.cl
    tags = []
    for reg in (7, 8):
        t = e.r(reg)
        tags += [e.u32(t), e.u32(t + 8), s32(e.u32(t + 0xC)), s32(e.u32(t + 0x10))]
    L.rec("FormatString", tuple([L.lab(e.r(3)), 1 if e.r(4) == ADDR["buffer"] else 0, e.r(5) & MASK, e.r(6) & MASK] + tags))
    v = s32(L.ret("FormatString"))
    e.write(e.r(4), struct.pack(">HH", 0x41 + (v & 15), 0))
    e.set_r(3, 0)


STUBS.internal(A_GETWINDOW, stub_getwindow)
STUBS.internal(A_SUM, stub_sum)
STUBS.internal(A_FORMAT, stub_format)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def alloc_world(self, case):
        self.panel = self.alloc(0x30)
        self.obj = self.alloc(0x448)
        self.obj2 = self.alloc(0x448)
        self.parent = self.alloc(0x20)
        self.world = self.alloc(0x10)
        self.table = self.alloc(0x10)
        self.vt = self.alloc(0x40)
        self.entry = self.alloc(8)
        ST["obj"] = self.obj

    def build_common(self, case):
        p = case["panel"]
        self.words(self.panel, [0, p["state"], 0, 0, self.off(self.parent) if p["parent"] else 0, 0, 0, 0, self.off(self.vt), p["a24"],
                                p["a28"], p["a2c"]])
        self.words(self.vt + 0x2C, [self.off(self.entry)])
        self.notify_entry(self.entry)
        self.ptr("gStaffRoomPanelThing", self.obj)
        self.ptr("gGameWorld", self.world)
        self.ptr("table_slot", self.table)
        for o, typ in ((self.obj, case["type"]), (self.obj2, case["type2"])):
            self.write(o + 4, bytes([case["cx"]]))
            self.write(o + 6, bytes([case["cy"]]))
            self.write(o + 0xE, struct.pack(self.fmt[0] + "H", typ))
        self.set_flag(case["flag"])


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.alloc_world(case)
        self.make_windows()
        fixed = {self.panel: 1, self.obj: 2, self.obj2: 13, self.parent: 11, ADDR["analyser"]: 12, self.table: 14, self.world: 15,
                 ADDR["script"]: 20, ADDR["callback"]: 21}
        for i, wid in enumerate(IDS):
            fixed[ST["win"][wid]] = 3 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def make_windows(self):
        ST["win"] = {}
        ST["ride"] = self.alloc(0x800)
        tab = self.alloc(0x30)
        ent_v, ent_t = self.alloc(8), self.alloc(8)
        self.words(ent_v, [A_SETVALUE, TOC])
        self.words(ent_t, [A_SETTEXT, TOC])
        self.words(tab + 0x24, [ent_v])
        self.words(tab + 0x2C, [ent_t])
        for wid in IDS:
            w = self.alloc(0x120)
            self.words(w + 0x118, [tab])
            ST["win"][wid] = w

    def notify_entry(self, a):
        self.words(a, [A_NOTIFY, TOC])

    def off(self, a):
        return a

    def ptr(self, name, value):
        self.words(ADDR[name], [value])

    def set_flag(self, v):
        self.words(ST["ride"] + 0x10C, [v])

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

    def call(self, fn, *args):
        T.call_ppc(self.e, fn, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.alloc_world(case)
        fixed = {1: self.panel, 2: self.obj, 13: self.obj2, 11: self.parent, 12: self.g("analyser"), 14: self.table, 15: self.world,
                 20: self.g("script"), 21: self.g("callback")}
        for i in range(8):
            fixed[3 + i] = self.g("win%d" % i)
        CL.host_begin(lib, fixed, case["queues"])

    def notify_entry(self, a):
        f = self.g("notify_fn")
        self.words(a, [f & MASK, f >> 32])

    def off(self, a):
        return a - self.lib.tpw_uvs_base()

    def ptr(self, name, value):
        self.words(self.g(name), [value & MASK, value >> 32])

    def set_flag(self, v):
        self.lib.tpw_uvs_set_flag(s32(v))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_uvs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_uvs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, *args):
        {FN_UPDATE: self.lib.tpw_uvs_update, FN_APPLY: self.lib.tpw_uvs_apply, FN_ALL: self.lib.tpw_uvs_applyall,
         FN_SHOW: self.lib.tpw_uvs_show}[fn](*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_uvs_addr.restype, lib.tpw_uvs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_uvs_alloc.restype, lib.tpw_uvs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_uvs_base.restype = C.c_uint64
    lib.tpw_uvs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uvs_set_flag.argtypes = [C.c_int32]
    lib.tpw_uvs_update.argtypes = [C.c_uint64]
    lib.tpw_uvs_apply.argtypes = [C.c_uint64]
    lib.tpw_uvs_applyall.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_uvs_show.argtypes = [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_uvs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def execute(s, case):
    op = case["op"]
    if op == "update":
        s.call(FN_UPDATE, s.panel)
    elif op == "apply":
        s.call(FN_APPLY, s.panel)
    elif op == "all":
        s.call(FN_ALL, s.panel, s.obj2)
    else:
        s.call(FN_SHOW, s.panel, case["thing"])


def observe(s, case):
    ch = struct.unpack(s.fmt[0] + "HH", s.read(s.g("buffer"), 4))
    pw = s.rd_words(s.panel, 12)
    pw[4] = 1 if pw[4] == s.off(s.parent) else pw[4] + 0x10000000000   # the pointer words as labels (the two memories differ)
    pw[8] = 1 if pw[8] == s.off(s.vt) else pw[8] + 0x10000000000
    return {"log": s.log(), "panel": pw, "thing": s.rd_word(s.g("gShowStatusThing")), "buf": ch}


def run(s, case):
    try:
        s.build_common(case)
        s.call_done = execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rv(r):
    return r.choice([0, 1, -1, 50, 99, 100, 101, 255, 256, r.randrange(-2 ** 31, 2 ** 31), r.randrange(0, 3000), -r.randrange(1, 3000)])


def base(r, op):
    q = {}
    for key in ("GetPercentageOfWinners", "GetProfitability", "GetProspectiveExcitementLevel", "GetCustomerSatisfaction",
                "GetCurrentResidualValue", "Happiness", "FormatString", "Sum556", "Sum108"):
        q[key] = [rv(r)]
    q["Sum420"] = [rv(r), rv(r)]
    state = r.choice([0, 1, 2, 3, 4, rv(r)])
    q["GetObjectState"] = [state if r.random() < 0.35 else rv(r)]
    for wid in IDS:
        q["Win%d" % wid] = [1 if wid in REQUIRED else r.choice([0, 1, 1, 1])]
    ctype = r.randrange(0, 0x10000)
    return {"op": op, "queues": q,
            "panel": {"state": state, "parent": r.random() < 0.9, "a24": rv(r), "a28": rv(r), "a2c": rv(r)},
            "cx": r.randrange(256), "cy": r.randrange(256), "type": ctype,
            "type2": ctype if r.random() < 0.5 else r.randrange(0, 0x10000), "flag": r.choice([0, 0, 1, -1, 5, rv(r)]),
            "thing": r.choice([0, 1, 0x5000, r.randrange(MASK)])}


def gen(op, edges):
    def edge():
        r = random.Random(op + "edge")
        for k in range(edges):
            c = base(r, op)
            yield c

    def rnd(r):
        return base(r, op)
    return edge, rnd


def update_edge():
    r = random.Random(77)
    for k in range(40):
        c = base(r, "update")
        if k == 0:
            c["panel"]["parent"] = False
        if k == 1:
            c["cx"], c["cy"] = 0, 0
        if k == 2:
            c["cx"], c["cy"] = 255, 255
        if 3 <= k < 8:
            c["queues"]["Win%d" % IDS[k - 2]] = [0]
        if k == 8:
            for wid in IDS:
                if wid not in REQUIRED:
                    c["queues"]["Win%d" % wid] = [0]
        if k == 9:
            c["queues"]["GetObjectState"] = [c["panel"]["state"]]
        if k == 10:
            c["queues"]["GetObjectState"] = [c["panel"]["state"] + 1]
        if 11 <= k < 16:
            v = [0, 99, 100, 101, -1][k - 11]
            for key in ("GetPercentageOfWinners", "GetCustomerSatisfaction", "Happiness", "GetProspectiveExcitementLevel"):
                c["queues"][key] = [v]
        if 16 <= k < 20:
            c["queues"]["Sum420"] = [[0, 3, -3, 4][k - 16]] * 2
            c["queues"]["Sum556"] = [[1, 5, -5, 6][k - 16]]
            c["queues"]["Sum108"] = [[7, 9, -9, 10][k - 16]]
        yield c


def update_rnd(r):
    return base(r, "update")


def apply_edge():
    r = random.Random(78)
    for flag in (0, 1, -1, 5, 0, 0):
        c = base(r, "apply")
        c["flag"] = flag
        yield c


def all_edge():
    r = random.Random(79)
    for same in (True, False, True, False):
        for flag in (0, 1):
            c = base(r, "all")
            c["type2"] = c["type"] if same else (c["type"] ^ 1)
            c["flag"] = flag
            yield c


def show_edge():
    r = random.Random(80)
    for thing in (0, 1, 0x5000, 0x7FFFFFFF, 0xFFFFFFFF):
        c = base(r, "show")
        c["thing"] = thing
        yield c


TESTS = [
    FnTest("ShowStatusPanel::UpdateStatusWindow", ppc, host, edge=update_edge, rnd=update_rnd),
    FnTest("ShowStatusPanel::ApplyChanges", ppc, host, edge=apply_edge, rnd=lambda r: base(r, "apply")),
    FnTest("ShowStatusPanel::applytoallcallback", ppc, host, edge=all_edge, rnd=lambda r: base(r, "all")),
    FnTest("ShowStatusPanel::Show", ppc, host, edge=show_edge, rnd=lambda r: base(r, "show")),
]
