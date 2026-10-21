"""Unit v5195, the loans panel, against libtpw_unit_LoansPanelMsg.dylib (tests/unit_LoansPanelMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_loansPanel.cpp:
  LoansPanelMsg 0x101a3f6c, Payback 0x101a3cd4, UpdateListHeadings 0x101a4230

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the panels, the advisor, the bank account and the loan info, the window lookup, the list's data, the string table, the dialog,
the draw text setter and the virtual SetText through __ptr_glue) are fakes on both sides that record their arguments and answer from the
case's queues. The dialog's text is the string the FormatString fake writes (from the id and the tag's value), so it is compared through
the dialog's parameters (length and the first four characters).

The world of a case: gGameWorld, the string table word of gText (+0x564), gLoansPanel (0x10394980), gLenderListMode, six windows (the
first is the panel and the argument; the other five are what GetWindow answers by its id: the queue "GetWindow:<id>" holds the window
number, 0 for NULL) with help text words reset to -1. What is compared: the answer, the panel global, the two help text words of every
window and the event log.

No fused operations. Not tested: UpdateListHeadings with a window lookup that answers NULL (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LoansPanelMsg"
MASK = 0xFFFFFFFF

FN_MSG, FN_PAYBACK, FN_UPDATE = 0x101A3F6C, 0x101A3CD4, 0x101A4230
SIZES = {FN_MSG: 652, FN_PAYBACK: 492, FN_UPDATE: 604}
OWN = set(SIZES)

A_REMOVE, A_CANCEL, A_FINANCE, A_STAFF, A_ENTRY = 0x101A571C, 0x1018227C, 0x101A001C, 0x101BE440, 0x1019D660
A_GETWINDOW, A_GETDATA, A_BANK, A_LOANINFO, A_BALANCE = 0x101DBE40, 0x101D41F0, 0x1013EDFC, 0x100F3C44, 0x100F32B4
A_DIALOG, A_DEFAULT, A_GETSTRING, A_CLIENTTEXT, A_GLUE = 0x101F1800, 0x1018493C, 0x10180468, 0x101DE130, 0x1023BAA8

ADDR = dict(world=0x1037EBFC, table=0x1037FF18 + 0x564, panel=0x10394980, mode=0x1039497C)
TV_PAYBACK, TV_TAKE = 0x10267C28, 0x10267C20
BANK, WORLD, TABLE = 0x20000010, 0x20000020, 0x20000030
GLUE_MARK = 0x5E7E7E70
NWIN = 6
PPC_WIN_SIZE = 0x138

WIN = []          # the PowerPC windows' addresses (persistent), filled by setup

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def cbid(tv):
    return 0 if tv == 0 else 1 if tv == TV_PAYBACK else 2 if tv == TV_TAKE else -1


def noarg(name):
    return lambda e: e.cl.rec(name, ())


STUBS.internal(A_REMOVE, noarg("RemoveLoansPanel"))
STUBS.internal(A_FINANCE, noarg("CreateFinancePanel"))
STUBS.internal(A_STAFF, noarg("CreateStaffCostsPanel"))
STUBS.internal(A_ENTRY, noarg("CreateEntryPricePanel"))


def stub_cancel(e):
    e.cl.rec("CancelAdvisorMessage", (s32(e.r(3)),))


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    k = L.ret("GetWindow:%d" % s32(e.r(4)))
    e.set_r(3, WIN[k] if k else 0)


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">I", L.ret("data") & MASK))
    e.set_r(3, 0)


def stub_bank(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, BANK)


def stub_loaninfo(e):
    L = e.cl
    L.rec("GetLoanInfo", (L.lab(e.r(4)), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">8I", *[L.ret("loan%d" % i) & MASK for i in range(8)]))


def stub_balance(e):
    e.cl.rec("GetBalance", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("balance") & MASK)


def stub_format(e):
    L = e.cl
    tag = e.r(7)
    magic, tid, value = s32(e.u32(tag)), s32(e.u32(tag + 8)), s32(e.u32(tag + 12))
    ident = e.r(6)
    L.rec("FormatString", (L.lab(e.r(3)), s32(e.r(5)), s32(ident), magic, tid, value))
    e.write(e.r(4), struct.pack(">3H", (ident | 0x4000) & 0xFFFF, ((value & 0x7FFF) | 0x100) & 0xFFFF, 0))
    e.set_r(3, 0)


def stub_dialog(e):
    L = e.cl
    params = e.r(3)
    L.rec("DialogBox", (s32(e.r(4)), cbid(e.r(5)), s32(e.r(6)), cbid(e.r(7)), s32(e.r(8)), cbid(e.r(9)), e.r(10) & 0xFF))
    text = e.u32(params)
    chars = list(struct.unpack(">256H", e.read(text, 512)))
    length = chars.index(0) if 0 in chars else 256
    rgb = e.read(params + 4, 3)
    L.rec("DialogParams", (rgb[0], rgb[1], rgb[2], s32(e.u32(params + 8)), s32(e.u32(params + 12)), length, chars[0], chars[1], chars[2],
                           chars[3]))
    e.set_r(3, 0)


def stub_default(e):
    L = e.cl
    L.rec("DefaultPanelMsg", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("default") & MASK)


def stub_getstring(e):
    e.cl.rec("GetString", (s32(e.r(3)),))
    e.set_r(3, 0x30000000 + e.r(3) * 16)


def stub_clienttext(e):
    e.cl.rec("SetClientDrawText", (e.cl.lab(e.r(3)), e.r(4)))


def stub_glue(e):
    code = e.u32(e.r(12))
    if code == GLUE_MARK:
        e.cl.rec("SetText", (e.cl.lab(e.r(3)), e.r(4)))
    else:
        e.cl.rec("BadGlue", (s32(code),))


STUBS.internal(A_CANCEL, stub_cancel)
STUBS.internal(A_GETWINDOW, stub_getwindow)
STUBS.internal(A_GETDATA, stub_getdata)
STUBS.internal(A_BANK, stub_bank)
STUBS.internal(A_LOANINFO, stub_loaninfo)
STUBS.internal(A_BALANCE, stub_balance)
STUBS.imp("FormatString__8TbITableCFPwUiUle", stub_format)
STUBS.internal(A_DIALOG, stub_dialog)
STUBS.internal(A_DEFAULT, stub_default)
STUBS.internal(A_GETSTRING, stub_getstring)
STUBS.internal(A_CLIENTTEXT, stub_clienttext)
STUBS.internal(A_GLUE, stub_glue)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    tv = T.persist_alloc(e, 8, struct.pack(">II", GLUE_MARK, 0))
    vt = T.persist_alloc(e, 0x40, bytes(0x2C) + struct.pack(">I", tv) + bytes(0x10))
    WIN.clear()
    for k in range(NWIN):
        WIN.append(T.persist_alloc(e, PPC_WIN_SIZE, bytes(0x118) + struct.pack(">I", vt) + bytes(PPC_WIN_SIZE - 0x11C)))


class Side:
    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    OFF_HELP, OFF_F90 = 0x8C, 0x90

    def __init__(self, e, case):
        self.e = e
        fixed = {BANK: 1, WORLD: 2, TABLE: 3}
        fixed.update({WIN[k]: 10 + k for k in range(NWIN)})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def win(self, k):
        return WIN[k]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_msg(self, w, m, a, b):
        return T.call_ppc(self.e, FN_MSG, [w, m & MASK, a & MASK, b & MASK])[0] & MASK

    def call_payback(self, loan):
        T.call_ppc(self.e, FN_PAYBACK, [loan & MASK])
        return 0

    def call_update(self):
        T.call_ppc(self.e, FN_UPDATE, [])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.OFF_HELP, self.OFF_F90 = lib.off_help, lib.off_f90
        fixed = {1: BANK, 2: WORLD, 3: TABLE}
        fixed.update({10 + k: lib.wins[k] for k in range(NWIN)})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lm_addr(name.encode())

    def win(self, k):
        return self.lib.wins[k]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_msg(self, w, m, a, b):
        return self.lib.tpw_lm_msg(w, T.s32(m), T.s32(a), T.s32(b)) & MASK

    def call_payback(self, loan):
        self.lib.tpw_lm_payback(T.s32(loan))
        return 0

    def call_update(self):
        self.lib.tpw_lm_update()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lm_addr.restype, lib.tpw_lm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lm_alloc.restype, lib.tpw_lm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lm_makewin.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_lm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lm_msg.restype, lib.tpw_lm_msg.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_lm_payback.argtypes = [C.c_int32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_lm_layout(out)
    size, lib.off_help, lib.off_f90 = out[0], out[1], out[2]
    assert n == 3 and 0 < lib.off_help < size and 0 < lib.off_f90 < size
    lib.wins = []
    for k in range(1, NWIN):
        pass
    for k in range(NWIN):
        a = lib.tpw_lm_alloc(size)
        lib.tpw_lm_makewin(k, a)
        lib.wins.append(a)
    lib.bound = True


def build(s, case):
    s.words(s.g("world"), [WORLD])
    s.words(s.g("table"), [TABLE])
    s.wptr(s.g("panel"), s.win(0) if case["panel"] else 0)
    s.words(s.g("mode"), [case["mode"]])
    for k in range(NWIN):
        s.words(s.win(k) + s.OFF_HELP, [MASK])
        s.words(s.win(k) + s.OFF_F90, [MASK])


def execute(s, case):
    op = case["op"]
    if op == "msg":
        return s.call_msg(s.win(0), *case["args"])
    if op == "pay":
        return s.call_payback(case["args"][0])
    return s.call_update()


def observe(s, case, answer):
    panel = s.rptr(s.g("panel"))
    d = {"answer": answer, "log": s.log(), "panel": "win0" if panel == s.win(0) else panel}
    for k in range(NWIN):
        d["w%d" % k] = [s.rd_word(s.win(k) + s.OFF_HELP), s.rd_word(s.win(k) + s.OFF_F90)]
    return d


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

MSGS = [0x1E, 5, 0x14, 0x13, 0x15, 0x1D, 0x1F, 4, 6, 0, -1, 0x100, 0x400, 0x3FF, 0x401, 0xFF, 0x101, 0x7FFFFFFF, 0x13, 1, 0x17]
BTNS = [0x12C4B9, 0x12C4BA, 0x12C4BB, 0x12C4BC, 0x12C4BD, -3, -2, -1, 0, 1, 0x7FFFFFFF, -0x80000000, 0x12C4B8, 0x12C4BE]
HEAD_IDS = {0x12C4BD: 1, 0x11: 2, 0x12: 3, 0x15: 4, 0x12C4CA: 5}


def loan_words(r, kind=None):
    period, months = r.randrange(0, 60), r.randrange(0, 60)
    monthly = r.choice([0, 1, r.randrange(0, 3000), r.randrange(0, 0x7FFFFFFF)])
    amount = r.choice([0, 1, 5000, 10000, 10001, r.randrange(0, 1 << 32)])
    return [r.choice([0, 1]), amount, r.randrange(0, 30), period, monthly, r.choice([0, 1]), months, r.randrange(0, 8)]


def cost_of(w):
    return s32(w[4] * s32(w[3] - w[6]))


def base(r, op, balance=None):
    w = loan_words(r)
    cost = cost_of(w)
    if balance is None:
        balance = r.choice([cost, cost + 1, cost - 1, 0, -1, r.randrange(-100000, 100000), 0x7FFFFFFF, -0x80000000, cost + 2, cost - 2])
    q = {"loan%d" % i: [w[i]] for i in range(8)}
    q["balance"] = [s32(balance)]
    q["default"] = [r.choice([0, 1, -1, r.randrange(-1000, 1000)])]
    q["data"] = [r.choice([-5, -2, -1, 0, 1, 2, 3, 8, 100, r.randrange(-50, 50)])]
    for i, k in HEAD_IDS.items():
        q["GetWindow:%d" % i] = [k]
    return {"op": op, "queues": q, "panel": r.choice([0, 1, 1]), "mode": r.choice([0, 1, 2, -1, 0x7FFFFFFF]), "args": []}


def msg_case(r, msg=None, a=None, b=None, data=None, k=None):
    c = base(r, "msg")
    c["panel"] = 1
    msg = r.choice(MSGS) if msg is None else msg
    a = r.choice(BTNS + [r.randrange(-5, 40), 0x11, 0x12C4CA]) if a is None else a
    b = r.choice([-1, 0, 1, 5, r.randrange(-3, 40), 0x7FFFFFFF]) if b is None else b
    c["args"] = [msg, a, b]
    c["queues"]["GetWindow:%d" % a] = [r.choice([0, 1, 2, 3, 4, 5]) if k is None else k]
    if data is not None:
        c["queues"]["data"] = [data]
    return c


def msg_edges():
    r = random.Random(5195)
    for m in MSGS:
        yield msg_case(r, m)
    for m in (0x100, 0x100, 0x101, 0xFF):
        for a in BTNS:
            yield msg_case(r, m, a)
    for k in (0, 1):
        for b in (-1, 0, 7):
            for data in (-5, -1, 0, 1, 2, 9, 0x7FFFFFFF, -0x80000000):
                yield msg_case(r, 0x400, 0x11, b, data, k)
    for m in (0x14, 0x1E, 5):
        yield msg_case(r, m, 0, 0)


def msg_random(r):
    return msg_case(r)


def pay_case(r, loan=None, balance=None):
    c = base(r, "pay", balance)
    c["args"] = [r.choice([0, 1, 7, -1, r.randrange(0, 100)]) if loan is None else loan]
    return c


def pay_edges():
    r = random.Random(5196)
    for loan in (0, 1, 7, -1, 0x7FFFFFFF):
        for i in range(8):
            c = pay_case(r, loan)
            w = [c["queues"]["loan%d" % j][0] for j in range(8)]
            cost = cost_of(w)
            c["queues"]["balance"] = [[cost, cost + 1, cost - 1, 0, -1, cost + 2, cost - 2, 0x7FFFFFFF][i] & MASK]
            yield c


def pay_random(r):
    return pay_case(r)


def upd_case(r):
    c = base(r, "upd")
    return c


def upd_edges():
    r = random.Random(5197)
    for panel in (0, 1):
        for mode in (0, 1, -1, 2, 0x80000000):
            c = upd_case(r)
            c["panel"], c["mode"] = panel, mode
            yield c


def upd_random(r):
    return upd_case(r)


TESTS = [
    FnTest("LoansPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("Payback", ppc, host, edge=pay_edges, rnd=pay_random),
    FnTest("UpdateListHeadings", ppc, host, edge=upd_edges, rnd=upd_random),
]
