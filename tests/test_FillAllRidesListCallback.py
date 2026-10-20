"""Unit v51109, twenty small panel functions against libtpw_unit_FillAllRidesListCallback.dylib (tests/unit_FillAllRidesListCallback.cpp), main app
("SimTheme Park"), src/AddItemToAllRidesList_FillAllRidesListCallback_Upgrades_FillAllRidesListCallback.cpp:
  FillAllRidesListCallback 0x10195b40, __sinit_ui_allKidsPanel_cpp 0x1019592c, RemoveAllKidsPanel 0x101956a4,
  __sinit_ui_StatusPanel_cpp 0x101946ac, RemoveAllItemsPanel 0x1019862c, StatusPanelIteratorCallback 0x10192a3c,
  RemoveAllStaffPanel 0x10199e20, __sinit_ui_allStaffPanel_cpp 0x10199e84, __sinit_ui_textmanager_cpp 0x101902e8,
  CoasterButtonsDown 0x1019cd00, ShowControlPanel 0x1019d28c, RemoveControlPanel 0x1019d2f0, RemoveEntryPricePanel 0x1019da08,
  __sinit_ui_entryPricePanel_cpp 0x1019e260, GetMinYValue 0x1019f0bc, RemoveFinancePanelPanel 0x101a0c8c,
  RemoveHireListPanel 0x101a29b0, RemoveLoadPanel 0x101a3934, RemoveSavePanel 0x101a3be4, PaybackLoanOK 0x101a3c44

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (UI_SendMessage, AddItemToAllRidesList, WhichCheckButtonsAreDown, LinkSlideoutPanel, the advisor messages,
RemoveSlideoutPanel, GetParkAnalyser, MinAnalyserValue, GetBankAccount, PayOffLoan, GetWindow, FillLenderList, the CoolRect and string
constructors, __register_global_object) are fakes on both sides that record their arguments; objects are nine blocks named by label.

The world of a case: every panel global holds one of the blocks (or null), the finance flags and mode, the coaster globals, the counter and the
six sinit words start random. What is compared: the answer (of the functions that answer), the counter, the six sinit words and the event log.

No fused operations. Not tested: a NULL world or loans panel (the original passes the NULL on to the callee; the host would call a member
of a null object, undefined for the compiler).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FillAllRidesListCallback"
MASK = 0xFFFFFFFF

# name: (address, size, host suffix, kind) kind: v void, a answer, plus the argument style
FUNCS = {
    "FillAllRidesListCallback": (0x10195B40, 48, "FillAllRidesListCallback", "thing"),
    "__sinit_ui_allKidsPanel_cpp": (0x1019592C, 28, "sinit_allKids", "void"),
    "RemoveAllKidsPanel": (0x101956A4, 56, "RemoveAllKidsPanel", "void"),
    "__sinit_ui_StatusPanel_cpp": (0x101946AC, 28, "sinit_status", "void"),
    "RemoveAllItemsPanel": (0x1019862C, 56, "RemoveAllItemsPanel", "void"),
    "StatusPanelIteratorCallback": (0x10192A3C, 24, "StatusPanelIteratorCallback", "thing"),
    "RemoveAllStaffPanel": (0x10199E20, 56, "RemoveAllStaffPanel", "void"),
    "__sinit_ui_allStaffPanel_cpp": (0x10199E84, 28, "sinit_allStaff", "void"),
    "__sinit_ui_textmanager_cpp": (0x101902E8, 56, "sinit_textmanager", "void"),
    "CoasterButtonsDown": (0x1019CD00, 64, "CoasterButtonsDown", "mask"),
    "ShowControlPanel": (0x1019D28C, 60, "ShowControlPanel", "void"),
    "RemoveControlPanel": (0x1019D2F0, 52, "RemoveControlPanel", "void"),
    "RemoveEntryPricePanel": (0x1019DA08, 56, "RemoveEntryPricePanel", "void"),
    "__sinit_ui_entryPricePanel_cpp": (0x1019E260, 64, "sinit_entryPrice", "void"),
    "GetMinYValue": (0x1019F0BC, 380, "GetMinYValue", "answer"),
    "RemoveFinancePanelPanel": (0x101A0C8C, 56, "RemoveFinancePanelPanel", "void"),
    "RemoveHireListPanel": (0x101A29B0, 56, "RemoveHireListPanel", "void"),
    "RemoveLoadPanel": (0x101A3934, 56, "RemoveLoadPanel", "void"),
    "RemoveSavePanel": (0x101A3BE4, 56, "RemoveSavePanel", "void"),
    "PaybackLoanOK": (0x101A3C44, 104, "PaybackLoanOK", "param"),
}
OWN = {a for a, _, _, _ in FUNCS.values()}

# ---- the callees faked on both sides
UI_SENDMESSAGE, ADDITEM, WHICHCHECK, LINKSLIDEOUT = 0x101C9518, 0x10195978, 0x1019B120, 0x101A82EC
ADVISOR, REMOVESLIDEOUT, CANCELADVISOR = 0x101821CC, 0x101A83D8, 0x1018227C
GETANALYSER, MINANALYSER, GETBANK, PAYOFF, GETWINDOW, FILLLENDER, COOLRECT = (
    0x1013EBC8, 0x101F6030, 0x1013EDFC, 0x100F40C8, 0x101DBE40, 0x101A4A38, 0x101C7448)

# ---- the Mac's addresses of the globals; ptr: a pointer on the host (8 bytes), word: 4 bytes on both sides
ADDR = dict(
    allKids=0x103947C8, allItems=0x103947E0, staff=0x103947F0, parkScreen=0x10394848, finance=0x10394904, hire=0x10394924,
    save=0x10394970, load=0x10394974, itemsList=0x103947D8, control=0x10394838, loans=0x10394980, editMode=0x10394830,
    coaster=0x10394834, statusCount=0x103947A0, flags=0x102A9EE4, mode=0x10394900, world=0x1037EBFC,
    w47a8=0x103947A8, w47ac=0x103947AC, w47cc=0x103947CC, w47d0=0x103947D0, w47f4=0x103947F4, w47f8=0x103947F8,
    rect=0x1038CD5C, rectDtor=0x102680A0, rectNode=0x1038CD68, string=0x1039484C, stringNode=0x10394860, stringDtor=0x103BCBB8,
)
PTRS = ["allKids", "allItems", "staff", "parkScreen", "finance", "hire", "itemsList", "control", "loans", "coaster", "world"]
OFFS = ["save", "load"]                   # int-typed in the headers; they hold the window
WORDS = ["editMode", "statusCount", "flags", "mode"]
WATCH = ["statusCount", "w47a8", "w47ac", "w47cc", "w47d0", "w47f4", "w47f8"]
LABELS = {"rect": 20, "rectDtor": 21, "rectNode": 22, "string": 23, "stringNode": 24, "stringDtor": 25}
CHEAT = 0x1024E068        # the text sits 0x2a bytes in
HOST_LAYOUT = []
NBLK = 9                  # blocks 1..8 are objects, block 9 is the analyser

CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def _min(e):
    L = e.cl
    an = CUR["an"]
    L.rec("MinAnalyserValue", ((e.r(3) - an) & MASK, s32(e.u32(e.r(4))), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">I", L.ret("min") & MASK))


STUBS.internal(UI_SENDMESSAGE, CL.recorder("UI_SendMessage", "Liii", ret="i"))
STUBS.internal(ADDITEM, CL.recorder("AddItemToAllRidesList", "LLL"))
STUBS.internal(WHICHCHECK, CL.recorder("WhichCheckButtonsAreDown", "u"))
STUBS.internal(LINKSLIDEOUT, CL.recorder("LinkSlideoutPanel", "Li"))
STUBS.internal(ADVISOR, CL.recorder("UIMain_AdvisorMessage", "i"))
STUBS.internal(CANCELADVISOR, CL.recorder("UIMain_CancelAdvisorMessage", "i"))
STUBS.internal(REMOVESLIDEOUT, CL.recorder("RemoveSlideoutPanel", "h"))
STUBS.internal(GETANALYSER, CL.recorder("GetParkAnalyser", "L", ret=lambda e, log: CUR["an"]))
STUBS.internal(MINANALYSER, _min)
STUBS.internal(GETBANK, CL.recorder("GetBankAccount", "L", ret=lambda e, log: CUR["bank"]))
STUBS.internal(PAYOFF, CL.recorder("PayOffLoan", "Lu", ret="b"))
STUBS.internal(GETWINDOW, CL.recorder("GetWindow", "Li", ret=lambda e, log: CUR["window"]))
STUBS.internal(FILLLENDER, CL.recorder("FillLenderList", "L"))
STUBS.internal(COOLRECT, CL.recorder("CoolRect", "L"))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", CL.recorder("StringCtor", "LS"))
STUBS.imp("__register_global_object", CL.recorder("__register_global_object", "LLL"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def blkaddr(self, i):
        return self.blk[i - 1] if i else 0

    def text(self, a, s):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in s + "\0"))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blk = [e.alloc(0x100) for _ in range(NBLK)]
        fixed = {b: i + 1 for i, b in enumerate(self.blk)}
        fixed.update({ADDR[k]: v for k, v in LABELS.items()})
        CUR.update(an=self.blk[8], bank=self.blkaddr(case["bank"]), window=self.blkaddr(case["window"]))
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def setp(self, name, i):
        self.write(ADDR[name], struct.pack(">I", self.blkaddr(i)))

    setoff = setp

    def setcheat(self, s):
        self.text(CHEAT + 0x2A, s)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, name, case):
        addr, _, _, kind = FUNCS[name]
        args = []
        if kind in ("thing", "param"):
            args = [self.blkaddr(case["arg"])] if kind == "thing" else [case["arg"] & MASK]
        elif kind == "mask":
            args = [case["arg"] & MASK]
        r3, _, _ = T.call_ppc(self.e, addr, args)
        return r3 & MASK if kind in ("thing", "answer", "param") else None


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blk = [lib.tpw_fr_alloc(0x100) for _ in range(NBLK)]
        fixed = {i + 1: b for i, b in enumerate(self.blk)}
        fixed.update({v: self.g(k) for k, v in LABELS.items()})
        self.setq("fakeAnalyser", 9)
        self.setq("fakeBank", case["bank"])
        self.setq("fakeWindow", case["window"])
        CL.host_begin(lib, fixed, case["queues"])

    def setq(self, name, i):
        self.write(self.g(name), struct.pack("<Q", self.blkaddr(i)))

    def g(self, name):
        return self.lib.tpw_fr_addr(name.encode())

    def setp(self, name, i):
        self.write(self.g(name), struct.pack("<Q", self.blkaddr(i)))

    def setoff(self, name, i):
        self.write(self.g(name), struct.pack("<I", self.lib.tpw_fr_offset(self.blkaddr(i)) if i else 0))

    def setcheat(self, s):
        base = self.lib.tpw_fr_alloc(0x100)
        self.text(base + 0x2A, s)
        self.write(self.g("cheat"), struct.pack("<Q", base))

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, case):
        _, _, suffix, kind = FUNCS[name]
        f = getattr(self.lib, "tpw_fr_" + suffix)
        if kind == "thing":
            return f(self.blkaddr(case["arg"])) & MASK
        if kind in ("mask", "param"):
            r = f(case["arg"] & MASK)
            return r & MASK if kind == "param" else None
        r = f()
        return r & MASK if kind == "answer" else None


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fr_addr.restype, lib.tpw_fr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fr_alloc.restype, lib.tpw_fr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fr_offset.restype, lib.tpw_fr_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_fr_layout.argtypes = [C.POINTER(C.c_int64)]
    for suffix in ("FillAllRidesListCallback", "StatusPanelIteratorCallback"):
        f = getattr(lib, "tpw_fr_" + suffix)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_fr_CoasterButtonsDown.argtypes = [C.c_uint32]
    lib.tpw_fr_PaybackLoanOK.restype, lib.tpw_fr_PaybackLoanOK.argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_fr_GetMinYValue.restype = C.c_uint32
    out = (C.c_int64 * 32)()
    n = lib.tpw_fr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for k in PTRS:
        s.setp(k, case[k])
    for k in OFFS:
        s.setoff(k, case[k])
    for k in WORDS:
        s.words(s.g(k), [case[k]])
    for k in WATCH[1:]:
        s.words(s.g(k), [case[k]])
    s.setcheat(case["text"])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for k in WATCH:
        out[k] = s.rd_words(s.g(k), 1)
    return out


def run(name):
    def one(s, case):
        try:
            build(s, case)
            return observe(s, case, s.call(name, case))
        finally:
            s.done()
    return one


# ---------------------------------------------------------------------------------------------------------------- case generators

INTS = [0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, 0x10000, 100, -100]


def base(r, name):
    c = {k: r.choice([0, 0] + list(range(1, 9))) for k in PTRS + OFFS}
    c["world"] = r.randrange(1, 9)
    c["loans"] = r.randrange(1, 9)
    c["bank"] = r.choice([0] + list(range(1, 9)))
    c["window"] = r.choice([0] + list(range(1, 9)))
    c["editMode"] = r.choice(INTS)
    c["statusCount"] = r.choice(INTS + [r.randrange(0, 1000)])
    c["flags"] = r.choice([0xFF, 0, 0x100, 0xFFFFFF00, 0xFFFFFFFF] + [r.randrange(0, 256), r.getrandbits(32)])
    c["mode"] = r.choice(INTS)
    for k in WATCH[1:]:
        c[k] = r.getrandbits(32)
    c["text"] = "".join(chr(r.randrange(32, 127)) for _ in range(r.randrange(0, 20)))
    c["arg"] = r.choice([0, 1, 2, 5, 0x12345678, 0xFFFFFFFF, r.getrandbits(32)])
    if FUNCS[name][3] == "thing":
        c["arg"] = r.randrange(0, 9)
    c["queues"] = {"min": [r.choice(INTS) for _ in range(r.randrange(1, 9))], "PayOffLoan": [r.choice([0, 1])]}
    return c


def edges_for(name):
    def gen():
        r = random.Random(sum(map(ord, name)))
        for i in range(12):
            yield base(r, name)
        if name == "CoasterButtonsDown":
            for panel in (0, 3):
                for mode in (0, 1, -1, 0x10000, -0x80000000):
                    c = base(r, name)
                    c["coaster"], c["editMode"] = panel, mode
                    yield c
        if name == "GetMinYValue":
            for fl in [0, 0xFF, 0x100, 0xFFFFFF00] + [1 << b for b in range(8)]:
                c = base(r, name)
                c["flags"] = fl
                c["queues"]["min"] = [-5, 7, -0x80000000, 0x7FFFFFFF, 0, 3, -1, 9]
                yield c
        if name == "StatusPanelIteratorCallback":
            for v in (0, -1, 0x7FFFFFFF, -0x80000000):
                c = base(r, name)
                c["statusCount"] = v
                yield c
        if name == "PaybackLoanOK":
            for a in (0, 1, 0xFFFFFFFF, 0x80000000):
                c = base(r, name)
                c["arg"] = a
                yield c
    return gen


def rnd_for(name):
    return lambda r: base(r, name)


def make(name):
    f = run(name)
    return FnTest(name, lambda e, c: f(Ppc(e, c), c), lambda lib, c: f(Host(lib, c), c), edge=edges_for(name), rnd=rnd_for(name))


TESTS = [make(n) for n in FUNCS]
