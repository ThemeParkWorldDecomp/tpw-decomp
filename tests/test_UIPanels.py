"""w131: the park's UI panel message functions. MainPanelButtonsMsg, HappyBarMsg, CashWindowCallback, BobbleCallback and GoldenStuffMsg
(src/game/ui/ui_mainpanel2.cpp), CashPanelMsg and ControlPanelMsg (src/game/ui/ui_cashPanel.cpp, src/game/ui/ui_controlPanel.cpp), StaffPanelMsg, StaffLocMsg and
ModifiedCheckButtonsCallback (src/game/ui/ui_staffPanel.cpp), UIInterface::CheckMouseContext (src/game/ui/UIInterface.cpp) and
CIntrface::Process (src/game/ui/intrface.cpp) run for real on both sides (tests/unit_UIPanels.cpp, cmake/units/UIPanels.cmake). Every callee
outside the unit records the same event on both sides: the window members, the panels the handlers open, the master save, the park analyser,
the advisor and the status panels. swprintf is the platform's on the host and a stub that formats "%d  x" (any other format shows) on the
PowerPC side.

Windows are fixed labelled stores (1 to 15, 0x200 bytes with a vtable at +0x118 on the PowerPC side). GetWindow answers by the child's id
from a queue per id ("W<hex id>", default the window the id has in the unit file, else the generic child 9). The original does not survive a
NULL window where it reaches a virtual (SetValue, SetText), so the cases keep the window under test and the children that get a SetText or a
SetValue non-NULL; every other NULL (the window, the child of a Show, the staff panel's window C, the cash window's) reaches only recorded
non-virtual members and is tested. The game type's guard cannot be reset, so the PowerPC side sets its guard byte in every case; the
construct-on-first-use path of the master save is tested (its `new` never answers NULL on the host, so that branch is reviewed only).
"""
import ctypes as C
import itertools
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIPanels"
MASK = 0xFFFFFFFF

FUNCTIONS = [
    ("MainPanelButtonsMsg", 0x101A5A2C, 256),
    ("CashPanelMsg", 0x1019ADE8, 244),
    ("StaffPanelMsg", 0x101BFDD8, 416),
    ("HappyBarMsg", 0x101A76F0, 264),
    ("ControlPanelMsg", 0x1019D070, 212),
    ("StaffLocMsg", 0x101C00F0, 468),
    ("CashWindowCallback", 0x101A60C0, 208),
    ("BobbleCallback", 0x101A67F4, 160),
    ("ModifiedCheckButtonsCallback", 0x101BFFB0, 248),
    ("UIInterface::CheckMouseContext", 0x101EFB0C, 40),
    ("CIntrface::Process", 0x1007DB78, 24),
    ("GoldenStuffMsg", 0x101A6404, 948),
]
BUTTONS, CASHP, STAFFP, HAPPY, CONTROL, STAFFLOC, CASHW, BOBBLE, MODCHECK, MOUSE, PROCESS, GOLDEN = range(12)

WINDOWS = 15
LAB_ANALYSER, LAB_THING, LAB_WORLD, LAB_MASTER, LAB_INTRFACE = 11, 12, 16, 17, 18
LAB_UIINTERFACE, LAB_MAP, LAB_STAFFSTATUS, LAB_VISITORSTATUS = 19, 20, 21, 22
GENERIC, PARENT = 9, 10
CHILDREN = {0x36: 3, 0x37: 4, 0x34: 5, 0x35: 6, 0x149: 7}

# the state words, in the order of tests/unit_UIPanels.cpp: address, size (4 word, 1 byte)
STATE_NAMES = ["state", "thing", "a", "c", "cash", "control", "aerial", "fpf", "cguard", "cframes", "cword", "sguard", "lspend", "kguard",
               "lkeys", "master", "gtype", "kind", "iword", "icount"]
(S_STATE, S_THING, S_A, S_C, S_CASH, S_CONTROL, S_AERIAL, S_FPF, S_CGUARD, S_CFRAMES, S_CWORD, S_SGUARD, S_LSPEND, S_KGUARD, S_LKEYS,
 S_MASTER, S_GTYPE, S_KIND, S_IWORD, S_ICOUNT) = range(20)
ADDR = dict(state=0x10395A38, thing=0x10395A3C, a=0x10395A48, c=0x10395A40, cash=0x10394824, control=0x10394838, aerial=0x103949D4,
            fpf=0x102B5A40, cframes=0x1039499C, cword=0x103949AC, lspend=0x10394994, lkeys=0x1039498C, master=0x10380A8C, gtype=0x102B3AA0)
BYTES = dict(cguard=0x10394998, sguard=0x10394990, kguard=0x10394988)
GAME_GUARD = 0x102B3AA4
GAMEWORLD_REF = 0x102A460C
UIINTERFACE, MAP, STAFFSTATUS, VISITORSTATUS = 0x10396440, 0x1039742C, 0x10395B1C, 0x1037EC14
INTRFACE_SIZE = 0x2C

DEFAULT_STATE = dict(state=1, thing=LAB_THING, a=2, c=13, cash=14, control=15, aerial=8, fpf=0x12345678, cguard=1, cframes=0, cword=0x77,
                     sguard=1, lspend=-5, kguard=1, lkeys=-6, master=LAB_MASTER, gtype=0, kind=5, iword=0x11223344, icount=0x01020304)
STATE_ORDER = ["state", "thing", "a", "c", "cash", "control", "aerial", "fpf", "cguard", "cframes", "cword", "sguard", "lspend", "kguard",
               "lkeys", "master", "gtype", "kind", "iword", "icount"]

STUBS = None


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def s32(e, a):
    return T.s32(e.u32(a))


def F(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype = result
    fn.argtypes = list(args)
    return fn


def bits_f(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def wide_read(e, a, limit=64):
    out = []
    while len(out) < limit:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return "".join(out)


def state_of(case):
    st = dict(DEFAULT_STATE)
    st.update(case.get("st", {}))
    return [T.s32(st[name] & MASK) for name in STATE_ORDER]


# ======================================================================= the PowerPC side


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    S = STUBS = CL.Stubs()
    R = CL.recorder
    S.real.update(a for _, a, _ in FUNCTIONS)

    def new(emu):
        size = emu.r(3)
        a = emu.alloc(max(size, 4), b"\xa5" * max(size, 4))
        emu.cl.newlab(a)
        emu.cl.heap[0] += 1
        emu.cl.rec("operator_new", (size, emu.cl.lab(a)))
        emu.set_r(3, a)

    def get_window(emu):
        ident = emu.r(4)
        emu.cl.rec("GetWindow", (emu.cl.lab(emu.r(3)), T.s32(ident)))
        label = emu.cl.ret("W%x" % ident, CHILDREN.get(ident, GENERIC))
        emu.set_r(3, emu.cl.addr.get(label, 0))

    def set_value(emu):
        emu.cl.rec("SetValue", (emu.cl.lab(emu.r(3)), T.s32(emu.r(4))))
        emu.set_r(3, 0)

    def set_text(emu):
        text = CL.wide_text(emu, emu.r(4)) if emu.r(4) else "(null)"
        emu.cl.rec("SetText", (emu.cl.lab(emu.r(3)),), (text,))
        emu.set_r(3, 0)

    def swprintf(emu):
        out, size, fmt = emu.r(3), emu.r(4), wide_read(emu, emu.r(5))
        if fmt != "%d  x":
            raise AssertionError("unexpected swprintf format %r" % fmt)
        text = "%d  x" % T.s32(emu.r(6))
        if size:
            emu.write(out, b"".join(struct.pack(">H", ord(c)) for c in text[:size - 1]) + b"\0\0")
        emu.set_r(3, T.u32(len(text) if size and len(text) <= size - 1 else -1))

    def happy(emu):
        emu.cl.rec("GetHappinessLevel", (emu.cl.lab(emu.r(3)), T.s32(emu.r(4))))
        emu.set_f1(bits_f(emu.cl.ret("Happy", 0)))

    e.up_window_vt = T.persist_alloc(e, 0x40)
    ws(e, e.up_window_vt + 0x24, [T.fake_function(e, set_value)])
    ws(e, e.up_window_vt + 0x2C, [T.fake_function(e, set_text)])
    S.imp("__nw__FUl", new)
    S.imp("swprintf", swprintf)
    for a, name, spec in [(0x101DC1EC, "Show", "Lh"), (0x101DE084, "unlink", "L"), (0x101DD6B8, "startAnimTimer", "Li"),
                          (0x101CC148, "SetCurrent", "Li"), (0x101CB1D0, "SetState", "Lbb"), (0x101C9400, "UI_PostMessage", "Liii"),
                          (0x101A587C, "GotoLastUsedPanel", "i"), (0x1019D28C, "ShowControlPanel", ""), (0x1019D2F0, "RemoveControlPanel", ""),
                          (0x101B4F74, "CreateResearchPanel", ""), (0x1022C080, "CreateMapPanel", "L"), (0x101A4DE4, "CreateLoansPanel", ""),
                          (0x101BE440, "CreateStaffCostsPanel", ""), (0x101C68D0, "CreateYearEndPanel", ""),
                          (0x101A83D8, "RemoveSlideoutPanel", "h"), (0x101B232C, "UI_DoPostcard", ""), (0x101F0FFC, "CamcorderMode", "L"),
                          (0x1018227C, "UIMain_CancelAdvisorMessage", "i"), (0x101BF5C4, "UpdateStatus", ""),
                          (0x101BF9F4, "StaffLocationSwitch", "i"), (0x101BFB48, "hackfollowmode", ""), (0x101A5CA8, "DoCashWin", "L"),
                          (0x10091D14, "LLCC_SystemSetSessionCost", "i"), (0x10182548, "ContextMouse", "i"),
                          (0x101433F8, "VisitorStatusPanel.Show", "LL"), (0x101C54F0, "StaffStatusPanel.Show", "LL")]:
        S.internal(a, R(name, spec))
    S.internal(0x101DE318, R("CallDefault", "Liii", ret=lambda emu, L: T.u32(L.ret("Default", 0))))
    S.internal(0x101DBE40, get_window)
    S.internal(0x101DBAFC, R("GetParent", "L", ret=lambda emu, L: L.addr.get(L.ret("Parent", PARENT), 0)))
    S.internal(0x101DD5E0, R("setAnimTimer", "Lii", ret=lambda emu, L: L.ret("setAnim", 1) & MASK))
    S.internal(0x1013EBC8, R("GetParkAnalyser", "L", ret=lambda emu, L: L.addr.get(L.ret("Analyser", LAB_ANALYSER), 0)))
    S.internal(0x100E4CD8, happy)
    S.internal(0x101694DC, R("CFSGlobalMasterSave", "N"))
    S.internal(0x10169EA0, R("HowManyKeys", "L", ret="i"))
    S.internal(0x10169D68, R("SpendingPower", "L", ret="i"))
    S.internal(0x1016F5D8, lambda emu: None)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        self.L.heap = [0, 0, 0, 0]
        self.L.addr = {}
        self.saved = STUBS.activate(e)
        L = self.L

        def store(label, size, address=None):
            a = address if address is not None else e.alloc(size, b"\xa5" * size)
            L.labels[a] = label
            L.addr[label] = a
            return a

        for label in range(1, WINDOWS + 1):
            ws(e, store(label, 0x200) + 0x118, [e.up_window_vt])
        store(LAB_ANALYSER, 0x40)
        store(LAB_THING, 0x40)
        store(LAB_WORLD, 0x40)
        store(LAB_MASTER, 0x100)
        store(LAB_INTRFACE, INTRFACE_SIZE)
        for label, address in ((LAB_UIINTERFACE, UIINTERFACE), (LAB_MAP, MAP), (LAB_STAFFSTATUS, STAFFSTATUS), (LAB_VISITORSTATUS, VISITORSTATUS)):
            store(label, 0, address)
        st = dict(zip(STATE_ORDER, state_of(case)))
        for name, a in ADDR.items():
            value = st[name]
            if name in ("thing", "a", "c", "cash", "control", "aerial", "master"):
                value = L.addr.get(value, 0)
            ws(e, a, [value])
        for name, a in BYTES.items():
            e.write(a, bytes([st[name] & 0xFF]))
        e.write(GAME_GUARD, b"\1")
        e.write(L.addr[LAB_THING] + 2, bytes([st["kind"] & 0xFF]))
        ws(e, e.u32(GAMEWORLD_REF), [L.addr[LAB_WORLD]])
        ws(e, L.addr[LAB_INTRFACE], [st["iword"]])
        ws(e, L.addr[LAB_INTRFACE] + 0xC, [st["icount"]])

    def dump(self):
        e, L = self.e, self.L
        out = [s32(e, ADDR["state"]), L.lab(e.u32(ADDR["thing"])), L.lab(e.u32(ADDR["a"])), L.lab(e.u32(ADDR["c"])),
               L.lab(e.u32(ADDR["cash"])), L.lab(e.u32(ADDR["control"])), L.lab(e.u32(ADDR["aerial"])), s32(e, ADDR["fpf"]),
               e.read(BYTES["cguard"], 1)[0], s32(e, ADDR["cframes"]), s32(e, ADDR["cword"]), e.read(BYTES["sguard"], 1)[0],
               s32(e, ADDR["lspend"]), e.read(BYTES["kguard"], 1)[0], s32(e, ADDR["lkeys"]), L.lab(e.u32(ADDR["master"])),
               s32(e, ADDR["gtype"]), e.read(L.addr[LAB_THING] + 2, 1)[0]]
        out += L.heap
        block = L.addr[LAB_INTRFACE]
        out += [s32(e, block), s32(e, block + 0xC)]
        out += [e.read(block + i, 1)[0] for i in range(4, INTRFACE_SIZE) if not 0xC <= i < 0x10]
        return out

    def run(self, case):
        e, L = self.e, self.L
        op, m, a, b = case["op"], case.get("m", 0), case.get("a", 0), case.get("b", 0)
        address = FUNCTIONS[op][1]
        window = L.addr.get(case.get("w", 1), 0)
        result = 0
        if op == MOUSE:
            T.call_ppc(e, address, [UIINTERFACE, m & MASK])
        elif op == PROCESS:
            T.call_ppc(e, address, [L.addr[LAB_INTRFACE]])
        else:
            result = T.s32(T.call_ppc(e, address, [window, m & MASK, a & MASK, b & MASK])[0])
        return dict(result=result, state=self.dump(), log=L.log)


def ppc_case(e, case):
    p = Ppc(e, case)
    try:
        return p.run(case)
    finally:
        CL.Stubs.deactivate(e, p.saved)


# ======================================================================= the host side


def host_case(lib, case):
    F(lib, "tpw_up_reset", None)()
    obj = F(lib, "tpw_up_obj", C.c_void_p, C.c_int32)
    fixed = {i: obj(i) for i in range(1, LAB_VISITORSTATUS + 1)}
    CL.host_begin(lib, fixed, case.get("q"))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    st = state_of(case)
    F(lib, "tpw_up_state", None, C.POINTER(C.c_int32))((C.c_int32 * len(st))(*st))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    args = [T.s32(case.get(k, 0) & MASK) for k in ("m", "a", "b")]
    result = F(lib, "tpw_up_call", C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32)(case["op"], case.get("w", 1), *args)
    log = CL.host_end(lib)
    out = (C.c_int32 * 200)()
    n = F(lib, "tpw_up_dump", C.c_int32, C.POINTER(C.c_int32))(out)
    return dict(result=result, state=list(out[:n]), log=log)


# ======================================================================= the cases

MSGS = [0x14, 0x15, 0x1E, 0x10, 0x13, 0x16, 0xFF, 0x100, 0x101, 0x1D, 0x1F, 0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x10001, 0x10002, 0x10006,
        0x10007, 0x1001D8, 0x1001D7, 0x1001D9, 0x100003, 0x80000100, 0x80000014, 0x10100, 0x10014, 0x10015, 0x1001E, 0x200000, 0x114, 0x115]
BS = [1, 0, 2, -1, 0x7FFFFFFF, -0x80000000, 0x100000, 0x10001]
WORDS = [0, 1, -1, 2, 3, 0x7FFFFFFF, -0x80000000, 0x10000, 0x8000, 0x80000, 0x80083, 0xFFFF, 0x10001]
DEFAULTS = [0, -9, 0x7FFFFFFF, -0x80000000, 1, 0x12345678]


def case(op, **values):
    return dict(op=op, **values)


def default_q(r):
    return {"Default": [r.choice(DEFAULTS + [r.getrandbits(32)])]}


def with_default(c, answer=-9):
    c.setdefault("q", {})
    c["q"].setdefault("Default", [answer])
    return c


def all_messages(op, **extra):
    """Every message of the list with a = 0 and 1 and b = 1 and 0: the default handler's answer and nothing else."""
    for m in MSGS:
        for a, b in ((0, 0), (1, 1), (0x26, 1), (-1, 7)):
            yield with_default(case(op, m=m, a=a, b=b, **extra))


def rnd_msg(r, special):
    return r.choice(special) if r.random() < 0.6 else r.choice(MSGS + [r.getrandbits(32)])


def rnd_word(r, special):
    return r.choice(special + [r.getrandbits(32), r.randrange(-10, 10)])


def rnd_state(r):
    st = {S_NAMES[i]: v for i, v in {}.items()}
    st["state"] = r.choice([0, 1, -1, 2, 0x100, r.getrandbits(31)])
    st["a"] = r.choice([0, 2, 13, 14])
    st["c"] = r.choice([0, 13, 1, 14])
    st["cash"] = r.choice([0, 14, 15, 1])
    st["control"] = r.choice([0, 14, 15, 1])
    st["aerial"] = r.choice([0, 8, 1, 13])
    st["fpf"] = r.choice([0, -1, 0x1000, 0xEFFF, 0x12345678, 0x80001000, r.getrandbits(32)])
    st["cguard"] = r.choice([0, 1, 0x80, 0xFF, 0x7F, 2])
    st["cframes"] = r.choice([0, 1, 2, 3, 4, 5, 6, 7, 0xFFFFFFFF, 0xFFFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000002, r.getrandbits(32)])
    st["cword"] = r.choice([0, -1, 0x77, r.getrandbits(32)])
    st["sguard"] = r.choice([0, 1, 0x80, 0xFF, 0x7F, 2])
    st["kguard"] = r.choice([0, 1, 0x80, 0xFF, 0x7F, 2])
    st["lspend"] = r.choice([-1, 0, 5, 77, -5, r.getrandbits(32)])
    st["lkeys"] = r.choice([-1, 0, 5, 77, -6, r.getrandbits(32)])
    st["master"] = r.choice([LAB_MASTER, LAB_MASTER, 0])
    st["gtype"] = r.choice([0, 1, 2, 2, 3, -1, 0x100])
    st["kind"] = r.randrange(256)
    st["iword"] = r.getrandbits(32)
    st["icount"] = r.choice([0, 1, -1, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)])
    return st


S_NAMES = STATE_NAMES


def rnd_window(r):
    return 0 if r.random() < 0.06 else 1


# ---- MainPanelButtonsMsg
def buttons_edges():
    for a in list(range(0x20, 0x30)) + [0, -1, 0x7FFFFFFF, -0x80000000, 0x126, 0x10026, 0x1000026, 0x80000026, 0x127, 0x10027]:
        for b in BS:
            yield with_default(case(BUTTONS, m=0x100, a=a, b=b))
    yield with_default(case(BUTTONS, m=0x100, a=0x26, b=1, w=0))
    yield with_default(case(BUTTONS, m=0x100, a=0x29, b=0, w=0))
    for m in MSGS:
        for a in (0x26, 0x27, 0x29, 0x2B):
            yield with_default(case(BUTTONS, m=m, a=a, b=1))
    yield from all_messages(BUTTONS)


def buttons_random(r):
    return with_default(case(BUTTONS, w=rnd_window(r), m=rnd_msg(r, [0x100]), a=rnd_word(r, [0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x25, 0x2C]),
                             b=rnd_word(r, [1, 0]), q=default_q(r)))


# ---- CashPanelMsg
def cash_edges():
    for a in [0x1B58, 0x1B59, 0x1B5A, 0x1B5B, 0x1B5C, 0x1B57, 0, -1, 0x7FFFFFFF, -0x80000000, 0x11B59, 0x1B59 + 0x10000, 0x80001B59]:
        for b in (1, 0, -1):
            yield with_default(case(CASHP, m=0x100, a=a, b=b))
    for cash in (0, 14, 15, 1):
        for m in (0x14, 0x100, 0x15, 0x114, 0x10014, 0x80000014):
            yield with_default(case(CASHP, m=m, a=0x1B59, b=1, st={"cash": cash}))
    yield with_default(case(CASHP, m=0x14, w=0))
    yield from all_messages(CASHP)


def cash_random(r):
    return with_default(case(CASHP, w=rnd_window(r), m=rnd_msg(r, [0x100, 0x14]), a=rnd_word(r, [0x1B59, 0x1B5A, 0x1B5B, 0x1B58, 0x1B5C]),
                             b=rnd_word(r, [1, 0]), st=rnd_state(r), q=default_q(r)))


# ---- StaffPanelMsg
def staff_edges():
    for state in (0, 1, -1, 0x100, 0x7FFFFFFF, -0x80000000):
        for a in list(range(0x95A, 0x969)) + [0, -1, 0x7FFFFFFF, -0x80000000, 0x95E + 0x100000000 - 0x100000000, 0x1095E, 0x8000095E, 0x95E - 0x10000]:
            for b in (1, 0, 2, -1):
                yield with_default(case(STAFFP, m=0x101, a=a, b=b, st={"state": state}))
    for m in (0x1E, 0x14, 0x1001D8):
        for c in (0, 13, 1):
            for fpf in (0, -1, 0x1000, 0xEFFF, 0x12345678, 0x80001000):
                yield with_default(case(STAFFP, m=m, st={"c": c, "fpf": fpf, "state": 5, "thing": 12, "a": 2}))
    for thing in (0, 12):
        yield with_default(case(STAFFP, m=0x1001D8, st={"thing": thing, "state": 0}))
    yield with_default(case(STAFFP, m=0x14, st={"a": 0}))
    yield with_default(case(STAFFP, m=0x101, a=0x95E, b=1, w=0))
    yield from all_messages(STAFFP)


def staff_random(r):
    return with_default(case(STAFFP, w=rnd_window(r), m=rnd_msg(r, [0x101, 0x101, 0x1E, 0x14, 0x1001D8]),
                             a=rnd_word(r, list(range(0x95B, 0x968))), b=rnd_word(r, [1, 0]), st=rnd_state(r), q=default_q(r)))


# ---- HappyBarMsg
FRACTIONS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x42480000, 0x42C80000, 0x43160000, 0x7FC00000, 0xFFC00000, 0x7F800000,
             0xFF800000, 0x00000001, 0x007FFFFF, 0x80000001, 0x7F7FFFFF, 0xFF7FFFFF, 0x3C23D70A, 0x42C7FFFF, 0x42C80001, 0x4F000000,
             0xCF000000, 0x4EFFFFFF, 0xCEFFFFFF, 0x4B800000, 0x3DCCCCCD, 0x41200000, 0x42C6FFFF, 0xC2C80000, 0x7F000000, 0x1E000000]


def emulator_overflow(c):
    """The emulator answers fctiwz of +infinity or of a number above 2^31 - 1 with 0x80000000 (the hardware and ppc_fctiwz with 0x7fffffff;
    FINDINGS 2026-10-12): such a level (the scaled value is 10.24 times the level) is left out, the NaN and negative ones stay."""
    bits = c.get("q", {}).get("Happy", [0])[0] & MASK
    value = bits_f(bits)
    return value == value and value > 2e8


def happy_edges():
    for c in happy_edges_all():
        if not emulator_overflow(c):
            yield c


def happy_edges_all():
    for answer in (0, 1, 0x10000, 0x8000, -1, 0xFFFF, 0x7FFF, 0x10001, -0x8000, 0xFFFF0000, 0x20000, 0x12345678):
        yield with_default(case(HAPPY, m=0x15, q={"setAnim": [answer]}))
    yield with_default(case(HAPPY, m=0x15, w=0))
    for a in (0x80083, 0x80084, 0x83, 0x180083, 0x80082, 0x80000083, 0x10083, 0, -1, 0x7FFFFFFF, 0x80083 + 0x10000000):
        for analyser in (LAB_ANALYSER, 0, 1):
            yield with_default(case(HAPPY, m=0x10, a=a, q={"Analyser": [analyser], "Happy": [0x42480000]}))
    for bits in FRACTIONS:
        yield with_default(case(HAPPY, m=0x10, a=0x80083, q={"Happy": [bits]}))
    yield with_default(case(HAPPY, m=0x10, a=0x80083, w=0, q={"Happy": [0x42480000]}))
    yield with_default(case(HAPPY, m=0x10, a=0x80083, w=0, q={"Analyser": [0]}))
    for m in (0x14, 0x13, 0x11, 0x110, 0x10010, 0x80000010, 0x80000015, 0x10015, 0, -1, 0x1E, 0x100):
        yield with_default(case(HAPPY, m=m, a=0x80083, q={"Happy": [0x42480000]}))
    yield from all_messages(HAPPY)


def happy_random(r):
    while True:
        c = happy_random_one(r)
        if not emulator_overflow(c):
            return c


def happy_random_one(r):
    bits = r.getrandbits(32)
    if r.random() < 0.7:
        bits = struct.unpack(">I", struct.pack(">f", r.choice([-1.0, 0.0, 1.0, 50.0, 100.0, 150.0, r.uniform(-300, 300), r.uniform(0, 100),
                                                               r.uniform(-1e10, 1e10), r.uniform(0, 3e6), r.uniform(-3e6, 0)])))[0]
    q = {"Happy": [bits], "setAnim": [r.choice([0, 1, 0x10000, 0x8000, -1, r.getrandbits(32)])],
         "Analyser": [r.choice([LAB_ANALYSER, LAB_ANALYSER, LAB_ANALYSER, 0])]}
    q.update(default_q(r))
    return with_default(case(HAPPY, w=rnd_window(r), m=rnd_msg(r, [0x10, 0x10, 0x15]), a=rnd_word(r, [0x80083, 0x80083]), b=rnd_word(r, [0, 1]), q=q))


# ---- ControlPanelMsg
def control_edges():
    for a in (100, 0x63, 0, 99, 101, -1, 0x164, 0x10064, 0x80000064, 0x7FFFFFFF, -0x80000000, 0x10063, 1):
        for b in (1, 0, -1):
            yield with_default(case(CONTROL, m=0x100, a=a, b=b))
    for control in (0, 14, 15, 1):
        for m in (0x14, 0x100, 0x15, 0x114, 0x10014, 0x80000014):
            yield with_default(case(CONTROL, m=m, a=100, b=1, st={"control": control}))
    yield with_default(case(CONTROL, m=0x14, w=0))
    yield with_default(case(CONTROL, m=0x100, a=100, w=0))
    yield from all_messages(CONTROL)


def control_random(r):
    return with_default(case(CONTROL, w=rnd_window(r), m=rnd_msg(r, [0x100, 0x14]), a=rnd_word(r, [100, 0x63, 0x64, 99]), b=rnd_word(r, [1, 0]),
                             st=rnd_state(r), q=default_q(r)))


# ---- StaffLocMsg
def staffloc_edges():
    for kind in range(256):
        yield with_default(case(STAFFLOC, m=0x10006, a=0, st={"kind": kind}))
    for a in (1, -1, 2, 0x10000, -0x80000000, 0x7FFFFFFF):
        yield with_default(case(STAFFLOC, m=0x10006, a=a, st={"kind": 5}))
    for m in (0x10001, 0x10002, 0x14, 0x10003, 0x10000, 0x10005, 0x10007, 0x100001, 0x10001 + 0x80000000, 0x80010002, 0x114):
        for c in (0, 13, 1):
            yield with_default(case(STAFFLOC, m=m, a=0, st={"c": c}))
    for answer in (0, 7, 8, 13):
        yield with_default(case(STAFFLOC, m=0x10001, q={"W149": [answer]}))
        yield with_default(case(STAFFLOC, m=0x10002, q={"W149": [answer]}))
    yield with_default(case(STAFFLOC, m=0x14, w=0))
    yield with_default(case(STAFFLOC, m=0x10006, a=0, w=0))
    yield from all_messages(STAFFLOC)


def staffloc_random(r):
    st = rnd_state(r)
    st["thing"] = LAB_THING
    st["kind"] = r.choice([r.randrange(256), r.randrange(2, 11)])
    return with_default(case(STAFFLOC, w=rnd_window(r), m=rnd_msg(r, [0x10006, 0x10006, 0x10001, 0x10002, 0x14]), a=rnd_word(r, [0, 0, 1]),
                             b=rnd_word(r, [0, 1]), st=st, q=default_q(r)))


# ---- CashWindowCallback
def cashw_edges():
    for guard in (0, 1, 0x80, 0xFF, 0x7F, 2):
        for frames in list(range(10)) + [0xFFFFFFFF, 0xFFFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000002, 0x12345676, 0xFFFFFFFD]:
            yield with_default(case(CASHW, m=0x1E, st={"cguard": guard, "cframes": frames}))
    for guard in (0, 1, 0x80, 0xFF):
        for m in (0x15, 0x14, 0x1D, 0x1F, 0, -1, 0x1001E, 0x8000001E, 0x10015, 0x80000015):
            yield with_default(case(CASHW, m=m, st={"cguard": guard, "cframes": 7, "cword": 0x55}))
    yield with_default(case(CASHW, m=0x15, w=0))
    yield with_default(case(CASHW, m=0x14, w=0, st={"cguard": 0}))
    yield from all_messages(CASHW)


def cashw_random(r):
    st = rnd_state(r)
    return with_default(case(CASHW, w=rnd_window(r), m=rnd_msg(r, [0x1E, 0x1E, 0x1E, 0x15]), a=rnd_word(r, [0]), b=rnd_word(r, [0]), st=st,
                             q=default_q(r)))


# ---- BobbleCallback
def bobble_edges():
    for a in (1, 0, 2, -1, 0x7FFFFFFF, -0x80000000, 0x10001, 0x100000, 0x80000001):
        for aerial in (8, 0, 1):
            yield with_default(case(BOBBLE, m=0x10006, a=a, b=0, st={"aerial": aerial}))
    for m in (0x10006 + 0x10000, 0x6, 0x10005, 0x10007, 0x110006, 0x80010006, 0x100006, 0x10106):
        yield with_default(case(BOBBLE, m=m, a=1))
    yield with_default(case(BOBBLE, m=0x10006, a=1, w=0))
    yield with_default(case(BOBBLE, m=0x10006, a=1, b=1))
    yield from all_messages(BOBBLE)


def bobble_random(r):
    return with_default(case(BOBBLE, w=rnd_window(r), m=rnd_msg(r, [0x10006, 0x10006]), a=rnd_word(r, [1, 1]), b=rnd_word(r, [0, 1]),
                             st=rnd_state(r), q=default_q(r)))


# ---- ModifiedCheckButtonsCallback
def modcheck_edges():
    for a in (0x95E, 0x95F, 0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x36, 0x149):
        for b in BS:
            for parent in (PARENT, 0, 1):
                yield with_default(case(MODCHECK, m=0x100, a=a, b=b, q={"Parent": [parent]}))
    for a, b in ((0x95E, 1), (0x95E, 0), (-1, 0), (0x7FFFFFFF, 3), (0, 1)):
        for child in (0, 9, 1, 3, 4):
            yield with_default(case(MODCHECK, m=0x100, a=a, b=b, q={"W%x" % (a & MASK): [child]}))
    for m in (0x101, 0x14, 0x15, 0x1E, 0xFF, 0x10100, 0x80000100, 0x200, 0x10, 0x114, 0, -1):
        yield with_default(case(MODCHECK, m=m, a=0x95E, b=0))
    yield with_default(case(MODCHECK, m=0x100, a=0x95E, b=0, w=0))
    yield with_default(case(MODCHECK, m=0x100, a=0x95E, b=1, w=0))
    yield from all_messages(MODCHECK)


def modcheck_random(r):
    a = rnd_word(r, [0x95E, 0x95F, 0x960, 0x95D])
    q = {"Parent": [r.choice([PARENT, PARENT, 0, 1])]}
    if r.random() < 0.15:
        q["W%x" % (a & MASK)] = [r.choice([0, 3, 9])]
    q.update(default_q(r))
    return with_default(case(MODCHECK, w=rnd_window(r), m=rnd_msg(r, [0x100, 0x100, 0x100]), a=a, b=rnd_word(r, [1, 0, 2]), q=q))


# ---- UIInterface::CheckMouseContext
def mouse_edges():
    for context in (0, 1, -1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000, 0x100, 0xFFFF, 0x10000, -0x8000, 0x7FFF, 0x8000):
        yield case(MOUSE, m=context)


def mouse_random(r):
    return case(MOUSE, m=r.choice([r.getrandbits(32), r.randrange(-8, 16), r.randrange(-40000, 40000)]))


# ---- CIntrface::Process
def process_edges():
    for count in (0, 1, -1, 0x7FFFFFFF, -0x80000000, 2, 0xFFFF, 0x10000, 0x12345678, 0xFFFFFFFE):
        for word in (0, 1, -1, 0x11223344, -0x80000000):
            yield case(PROCESS, st={"icount": count, "iword": word})


def process_random(r):
    return case(PROCESS, st={"icount": r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]),
                             "iword": r.choice([0, -1, r.getrandbits(32)])})


# ---- GoldenStuffMsg
KEY_SEQUENCES = [[0, 0, 0, 0], [5, 5, 5, 5], [5, 6, 7, 8], [0, 5, 5, 5], [5, 0, 5, 5], [5, 5, 0, 5], [12345678] * 4, [-5] * 4, [7, 7, 7, 0],
                 [0, 0, 5, 5], [5, 5, 5, 0], [99999, 99999, 99999, 99999], [-1, -1, -1, -1], [0x7FFFFFFF] * 4, [-0x80000000] * 4]
SPEND_PAIRS = [(-1, 0), (-1, 5), (0, 0), (5, 5), (5, 0), (0, 5), (77, -3), (-1, 99999999), (3, 12345678), (-3, -3), (-1, -1), (7, 70000)]


def golden_edges():
    for sg, kg in itertools.product((0, 1, 0x80, 0xFF, 0x7F, 2), repeat=2):
        for m in (0x1E, 0x15, 0x14):
            for lspend, lkeys in ((-1, -1), (3, 5), (0, 0)):
                yield with_default(case(GOLDEN, m=m, st={"sguard": sg, "kguard": kg, "lspend": lspend, "lkeys": lkeys},
                                        q={"HowManyKeys": [5] * 4, "SpendingPower": [3]}))
    for gtype, master, lkeys in itertools.product((0, 2, 1, -1), (LAB_MASTER, 0), (-1, 0, 5)):
        for keys in KEY_SEQUENCES:
            for lspend, spend in SPEND_PAIRS[:6]:
                yield with_default(case(GOLDEN, m=0x1E, st={"gtype": gtype, "master": master, "lkeys": lkeys, "lspend": lspend},
                                        q={"HowManyKeys": keys, "SpendingPower": [spend]}))
    for gtype in (3, 0x100, -0x80000000, 0x10002, 2):
        yield with_default(case(GOLDEN, m=0x1E, st={"gtype": gtype}, q={"HowManyKeys": [4, 4, 4, 4], "SpendingPower": [9]}))
    for lspend, spend in SPEND_PAIRS:
        for lkeys, keys in ((-1, 0), (-1, 7), (7, 7), (8, 7)):
            yield with_default(case(GOLDEN, m=0x1E, st={"lspend": lspend, "lkeys": lkeys}, q={"HowManyKeys": [keys] * 4, "SpendingPower": [spend]}))
    for m in (0x15, 0x14, 0x1D, 0x1F, 0, -1, 0x1001E, 0x8000001E, 0x10015, 0x80000015, 0x10, 0x100):
        for master in (LAB_MASTER, 0):
            yield with_default(case(GOLDEN, m=m, st={"master": master, "lspend": 4, "lkeys": 4}, q={"HowManyKeys": [1, 2, 3, 4], "SpendingPower": [8]}))
    yield with_default(case(GOLDEN, m=0x15, w=0, st={"sguard": 0, "kguard": 0}))
    for child in (0, 9):
        yield with_default(case(GOLDEN, m=0x14, w=0, q={"W36": [child]}))
    for answers in ({"W36": [3], "W37": [4]}, {"W34": [0], "W35": [0]}, {"W34": [9], "W35": [9], "W36": [4], "W37": [3]}):
        q = {"HowManyKeys": [5] * 4, "SpendingPower": [8]}
        q.update(answers)
        yield with_default(case(GOLDEN, m=0x1E, st={"lspend": 1, "lkeys": 1}, q=q))
    yield from all_messages(GOLDEN, st={"lspend": 3, "lkeys": 3})


def golden_random(r):
    keys = [r.choice([0, 1, -1, 5, 100, 12345678, -12345678, 0x7FFFFFFF, -0x80000000, 7, 1000, 99999, 9999999, 10000000])]
    keys = keys * 4 if r.random() < 0.5 else [r.choice([0, 5, 7, -2, 1234567]) for _ in range(4)]
    st = rnd_state(r)
    if r.random() < 0.5:
        st["lkeys"] = r.choice(keys)
    q = {"HowManyKeys": keys, "SpendingPower": [r.choice([0, 0, 1, 5, -3, 77, 99999999, 1234567, 12345, 123456, -123456, r.getrandbits(32)])]}
    if r.random() < 0.5:
        st["lspend"] = q["SpendingPower"][0]
    q.update(default_q(r))
    return with_default(case(GOLDEN, w=1, m=rnd_msg(r, [0x1E, 0x1E, 0x1E, 0x15]), a=rnd_word(r, [0]), b=rnd_word(r, [0]), st=st, q=q))


def fn(op, edges, random):
    return FnTest(FUNCTIONS[op][0], ppc_case, host_case, edge=edges, rnd=random)


TESTS = [
    fn(BUTTONS, buttons_edges, buttons_random),
    fn(CASHP, cash_edges, cash_random),
    fn(STAFFP, staff_edges, staff_random),
    fn(HAPPY, happy_edges, happy_random),
    fn(CONTROL, control_edges, control_random),
    fn(STAFFLOC, staffloc_edges, staffloc_random),
    fn(CASHW, cashw_edges, cashw_random),
    fn(BOBBLE, bobble_edges, bobble_random),
    fn(MODCHECK, modcheck_edges, modcheck_random),
    fn(MOUSE, mouse_edges, mouse_random),
    fn(PROCESS, process_edges, process_random),
    fn(GOLDEN, golden_edges, golden_random),
]
