"""Differential test for unit w173, the bank account's save, month and loans (main app "SimTheme Park", src/BankAccount.cpp), against
libtpw_unit_BankAccount.dylib (tests/unit_BankAccount.cpp).

Functions: CBankAccount::Serialise 0x100f2d10 (1,392 bytes), MonthlyUpdate 0x100f36a8 (752), GetLoanInfo 0x100f3c44 (320), AnyLoanAvailable 0x100f3ee0 (432),
PayOffLoan 0x100f40c8 (384). The original inlines CBankAccount::MakeWithdrawal 0x100f33b0 into MonthlyUpdate and PayOffLoan; the host calls the translated one
(src/Things2_small.cpp), so the log shows the same GetParkAnalyser call on both sides.

A case is an account image (0x128 bytes over 0x40 guard bytes on each side, random words), two worlds (the 0x80 bytes at world + 0x1da6f0 are real memory, the
turn word among them; a world pointer is that block's address minus 0x1da6f0), two analysers (blocks reaching their words +0x1f5a0 and +0x1fc90, the rest
poison) and the message centre (a poison block, or none). gWorld and gGameWorld are chosen apart. Every callee is a fake on both sides that records the same
event: CThing::Serialise 0x1012e010, the six LOADVAR / SAVEVAR instances (0x100f44a4 0x1000ef18 0x1000f02c / 0x100f45bc 0x1000f4b0 0x1000f5dc), GAME_ASSERTMSG,
CTRACE (level and one int per "%d"), CWorld::GetParkAnalyser, CParkAnalyser::GetParkValue (answers the queue "pv"), CParkTime::GetFunnyTimeSinceGameTurn (the
queue "span"), CMsgEvent's constructor, the nothrow new (queue "newnull"), the CSubscriptionHeader array construction and CMessageCentre::PostMessage. The
Serialise fakes share one counter: the call whose number equals the case's `fail` answers `fail_value`, every other answers 0 (72 calls in a full run).

Compared: the answer, the call log, the account and its guards, both analysers (the two words by value, the rest by bytes), the world tails, the message centre
pointer's label and the CLoan result (0x20 bytes over guards). Not tested: the callees' own work; spans whose 64-bit quotient has bits above the low word
(a span is at most 2^63 ticks, so the quotient stays below 2^19).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BankAccount"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
POISON = b"\xa5"

SERIALISE, MONTHLY, LOANINFO, ANYLOAN, PAYOFF, HELPER = 0x100F2D10, 0x100F36A8, 0x100F3C44, 0x100F3EE0, 0x100F40C8, 0x10237E1C
CTHING, GETAN, PARKVALUE, FUNNY, CTOR, POST = 0x1012E010, 0x1013EBC8, 0x100E5110, 0x10112C80, 0x10151DF8, 0x1015256C
ASSERT, CTRACE = 0x1000DDE8, 0x1006FCBC
VARS = {0x100F44A4: "LOADVAR<unsigned int>", 0x1000EF18: "LOADVAR<int>", 0x1000F02C: "LOADVAR<unsigned long>",
        0x100F45BC: "SAVEVAR<unsigned int>", 0x1000F4B0: "SAVEVAR<int>", 0x1000F5DC: "SAVEVAR<unsigned long>"}
G_WORLD, G_GAME, G_CENTRE = 0x1037EC0C, 0x1037EBFC, 0x1037F6C4
CENTRE_CTOR, CENTRE_DTOR = 0x102657E8, 0x10267288
FILE = 0x1000
TAIL0 = 0x1DA6F0
TURN_WORD = (0x1DA70C - TAIL0) // 4
PARKTIME = 0x2A0
GUARD = 0x40
OBJ_SIZE = 0x128
OBJ_BLOCK = GUARD + OBJ_SIZE + GUARD
AN_BLOCK = GUARD + 0x1FC94 + GUARD
AN_W2, AN_W5 = GUARD + 0x1F5A0, GUARD + 0x1FC90
OUT_BLOCK = 0x60
HOST_LAYOUT = [0x128, 0x14, 0x114, 0x124, 0x20, 0x1F5A0, 0x1FC90, 0x1DA70C, 0x2A0]
# labels: 1 the file, 2 and 3 the worlds, 4 and 5 the analysers, 6 the message centre, 12 and 13 the worlds' park times
L_FILE, L_WORLD, L_AN, L_CENTRE, L_TIME = 1, 2, 4, 6, 12

# the account's words
BALANCE, BATCH, ENABLED, FEE, LAST, RED, PROFIT = 0xC, 0x10, 0x114, 0x118, 0x11C, 0x120, 0x124
LOAN0 = 0x14
AVAIL, AMOUNT, APR, PERIOD, REPAY, BOUGHT, MONTHS, LENDER = 0, 4, 8, 0xC, 0x10, 0x14, 0x18, 0x1C
MONTH = 25920000000000      # thirty days of 100 ns ticks (0x1792f8648000)
SPANS = [0, 5 * MONTH, 5 * MONTH - 1, 5 * MONTH + 1, 6 * MONTH, 6 * MONTH - 1, 6 * MONTH + 1, 7 * MONTH, -1, -MONTH, -6 * MONTH, -6 * MONTH - 1,
         355000 * MONTH, 2 ** 63 - 1, -2 ** 63, 1, 12 * MONTH]


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


def pv_clamp(v):
    """Unicorn's fctiwz answers 0x80000000 for a float of 2^31 or more where the processor saturates to 0x7fffffff, so the park values that round to 2^31
    (0x7fffffc0 and above) are not tested; the translation saturates (ppc_fctiwz), see the doubtful list."""
    v = s32(v)
    return 0x7FFFFFBF if v >= 0x7FFFFFC0 else v


def qn(q):
    return {k: [s64(pv_clamp(v) if k == "pv" else v) for v in vs] for k, vs in q.items()}


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == CENTRE_CTOR:
            emu.cl.rec("CMessageCentre", [emu.cl.newlab(p), int(ctor == CENTRE_CTOR), int(dtor == CENTRE_DTOR), size, count])

    T.stub_cb(e, "__construct_array", construct)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update({SERIALISE, MONTHLY, LOANINFO, ANYLOAN, PAYOFF, HELPER})
STATE = {"count": 0, "fail": -1, "value": 0}


def answer(e):
    number = STATE["count"]
    STATE["count"] += 1
    e.set_r(3, (STATE["value"] if number == STATE["fail"] else 0) & MASK)


def stub_cthing(e):
    L = e.cl
    L.rec("CThing::Serialise", (e.r(3) - L.base, L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK), ())
    answer(e)


def var_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.r(4) - L.base, s32(e.r(6))), (e.cstr(e.r(5))[:159],))
        answer(e)
    return stub


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", [e.r(3) & 0xFF], [e.cstr(e.r(4))[:159]])


def stub_trace(e):
    text = e.cstr(e.r(4))[:159]
    count = min(text.count("%d"), 3)
    e.cl.rec("CTRACE", [e.r(3) & MASK] + [s32(e.r(5 + k)) for k in range(count)], [text])


def stub_getan(e):
    L = e.cl
    L.rec("GetParkAnalyser", [L.lab(e.r(3))])
    e.set_r(3, L.an[0 if e.r(3) == L.worlds[0] else 1])


def stub_parkvalue(e):
    e.cl.rec("GetParkValue", [e.cl.lab(e.r(3)), e.r(4) & MASK])
    e.set_r(3, e.cl.ret("pv", 0) & MASK)


def stub_funny(e):
    e.cl.rec("GetFunnyTimeSinceGameTurn", [e.cl.lab(e.r(4)), e.r(5) & MASK])
    e.write(e.r(3), (e.cl.ret("span", 0) & M64).to_bytes(8, "big"))


def stub_ctor(e):
    e.cl.rec("CMsgEvent", [e.cl.newlab(e.r(3)), s32(e.r(4))])
    e.write(e.r(3) + 8, (e.r(4) & MASK).to_bytes(4, "big"))


def stub_post(e):
    L = e.cl
    L.rec("PostMessage", [L.lab(e.r(3)), L.lab(e.r(4)), s32(e.u32(e.r(4) + 8))])
    e.set_r(3, 1)


def op_new(e):
    size = e.r(3)
    fail = bool(e.cl.ret("newnull", 0))
    e.cl.rec("new", [size, int(fail)])
    e.set_r(3, 0 if fail else e.alloc(size, POISON * size))


STUBS.internal(CTHING, stub_cthing)
for _a, _n in VARS.items():
    STUBS.internal(_a, var_stub(_n))
for _a, _f in ((ASSERT, stub_assert), (CTRACE, stub_trace), (GETAN, stub_getan), (PARKVALUE, stub_parkvalue), (FUNNY, stub_funny),
               (CTOR, stub_ctor), (POST, stub_post)):
    STUBS.internal(_a, _f)
STUBS.imp("__nw__FUl", op_new)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    order = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({FILE: L_FILE}, qn(case["queues"]))
        STATE.update(count=0, fail=case["fail"], value=case["fail_value"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def blocks(self, sizes, datas):
        return [self.e.alloc(n, d) for n, d in zip(sizes, datas)]

    def read(self, a, n):
        return self.e.read(a, n)

    def globals(self, case, b):
        e, L = self.e, self.L
        tails, ans, centre = b["tails"], b["an"], b["centre"]
        L.worlds = [tails[0] - TAIL0, tails[1] - TAIL0]
        L.an = [ans[0] + GUARD, ans[1] + GUARD]
        for k in range(2):
            L.labels[L.worlds[k]] = L_WORLD + k
            L.labels[L.worlds[k] + PARKTIME] = L_TIME + k
            L.labels[L.an[k]] = L_AN + k
        L.labels[centre] = L_CENTRE
        L.base = b["obj"] + GUARD
        for addr, value in ((G_WORLD, L.worlds[case["world"]]), (G_GAME, L.worlds[case["game"]]), (G_CENTRE, centre if case["centre"] else 0)):
            e.write(addr, struct.pack(">I", value & MASK))

    def centre(self):
        return self.L.lab(self.e.u32(G_CENTRE))

    def call(self, fn, b, case):
        obj = b["obj"] + GUARD
        args = {"serialise": [obj, FILE, case["save"] & MASK, case["version"] & MASK, case["unused"] & MASK], "monthly": [obj],
                "loaninfo": [b["out"] + 0x20, obj, case["loan"]], "anyloan": [obj], "payoff": [obj, case["loan"]]}[fn]
        entry = {"serialise": SERIALISE, "monthly": MONTHLY, "loaninfo": LOANINFO, "anyloan": ANYLOAN, "payoff": PAYOFF}[fn]
        r3, _, _ = T.call_ppc(self.e, entry, args)
        return r3 & (0xFF if fn in ("anyloan", "payoff") else MASK) if fn in ("anyloan", "payoff", "serialise") else 0

    def log(self):
        return self.L.log


class Host:
    order = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.case = case
        self.fixed = {L_FILE: FILE}
        CL.host_begin(lib, self.fixed, qn(case["queues"]))

    def done(self):
        pass

    def blocks(self, sizes, datas):
        out = []
        for k, (n, d) in enumerate(zip(sizes, datas)):
            a = self.lib.tpw_bnk_addr(k) or 0
            C.memmove(a, d, n)
            out.append(a)
        return out

    def read(self, a, n):
        return C.string_at(a, n)

    def globals(self, case, b):
        lib = self.lib
        worlds = [b["tails"][k] - TAIL0 for k in range(2)]
        for k in range(2):
            self.fixed[L_WORLD + k] = worlds[k]
            self.fixed[L_TIME + k] = worlds[k] + PARKTIME
            self.fixed[L_AN + k] = b["an"][k] + GUARD
        self.fixed[L_CENTRE] = b["centre"]
        CL.host_begin(lib, self.fixed, qn(case["queues"]))
        lib.tpw_bnk_prepare(case["world"], case["game"], case["centre"], case["fail"], s32(case["fail_value"]))

    def centre(self):
        return self.lib.tpw_bnk_centre()

    def call(self, fn, b, case):
        lib = self.lib
        if fn == "serialise":
            return lib.tpw_bnk_serialise(s32(case["save"]), s32(case["version"]), case["unused"] & MASK) & MASK
        if fn == "monthly":
            lib.tpw_bnk_monthly()
            return 0
        if fn == "loaninfo":
            lib.tpw_bnk_loaninfo(case["loan"])
            return 0
        if fn == "anyloan":
            return lib.tpw_bnk_anyloan() & 0xFF
        return lib.tpw_bnk_payoff(case["loan"]) & 0xFF

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bnk_addr.restype, lib.tpw_bnk_addr.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_bnk_prepare.restype, lib.tpw_bnk_prepare.argtypes = None, [C.c_int32] * 5
    lib.tpw_bnk_centre.restype = C.c_int64
    lib.tpw_bnk_serialise.restype, lib.tpw_bnk_serialise.argtypes = C.c_int32, [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_bnk_monthly.restype = None
    lib.tpw_bnk_loaninfo.restype, lib.tpw_bnk_loaninfo.argtypes = None, [C.c_uint32]
    lib.tpw_bnk_anyloan.restype = C.c_int32
    lib.tpw_bnk_payoff.restype, lib.tpw_bnk_payoff.argtypes = C.c_int32, [C.c_uint32]
    lib.tpw_bnk_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 16)()
    n = lib.tpw_bnk_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, (list(out)[:n], HOST_LAYOUT)
    lib.bound = True


def words(order, ws):
    return struct.pack(order + "%dI" % len(ws), *[w & MASK for w in ws])


def unwords(order, data):
    return list(struct.unpack(order + "%dI" % (len(data) // 4), data))


def run(s, fn, case):
    try:
        rr = random.Random(case["seed"])
        guard = [rr.getrandbits(32) for _ in range(GUARD // 4)]
        obj = words(s.order, guard + case["obj"] + [rr.getrandbits(32) for _ in range(GUARD // 4)])
        tails = [words(s.order, [rr.getrandbits(32) if i != TURN_WORD else case["turns"][k] for i in range(32)]) for k in range(2)]
        ans = []
        for k in range(2):
            a = bytearray(POISON * AN_BLOCK)
            a[AN_W2:AN_W2 + 4] = words(s.order, [case["an"][k][0]])
            a[AN_W5:AN_W5 + 4] = words(s.order, [case["an"][k][1]])
            ans.append(bytes(a))
        sizes = [OBJ_BLOCK, 0x80, 0x80, AN_BLOCK, AN_BLOCK, 0x1D0, OUT_BLOCK]
        datas = [obj, tails[0], tails[1], ans[0], ans[1], POISON * 0x1D0, POISON * OUT_BLOCK]
        got = s.blocks(sizes, datas)
        b = {"obj": got[0], "tails": got[1:3], "an": got[3:5], "centre": got[5], "out": got[6]}
        s.globals(case, b)
        result = s.call(fn, b, case)
        centre = s.centre()
        out = {"result": result, "centre": centre, "obj": unwords(s.order, s.read(b["obj"], OBJ_BLOCK)),
               "tails": [unwords(s.order, s.read(b["tails"][k], 0x80)) for k in range(2)], "centre_block": s.read(b["centre"], 0x1D0)}
        for k in range(2):
            image = bytearray(s.read(b["an"][k], AN_BLOCK))
            out["an%d" % k] = unwords(s.order, bytes(image[AN_W2:AN_W2 + 4]) + bytes(image[AN_W5:AN_W5 + 4]))
            image[AN_W2:AN_W2 + 4] = b"\0\0\0\0"
            image[AN_W5:AN_W5 + 4] = b"\0\0\0\0"
            out["an_rest%d" % k] = bytes(image)
        if fn == "loaninfo":
            out["out"] = unwords(s.order, s.read(b["out"], OUT_BLOCK))
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc_fn(fn):
    return lambda e, case: run(Ppc(e, case), fn, case)


def host_fn(fn):
    return lambda lib, case: run(Host(lib, case), fn, case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def f32_int(v):
    """The park value the way the listing turns it into the loan test: exact double, fsubs to single, fctiwz."""
    f = struct.unpack(">f", struct.pack(">f", float(v)))[0]
    return INT_MAX if f >= 2147483648.0 else INT_MIN if f <= -2147483649.0 else int(f)


def py_debt(obj):
    debt = 0
    for i in range(8):
        base = (LOAN0 + 0x20 * i) // 4
        if obj[base + BOUGHT // 4] != 0:
            debt += obj[base + REPAY // 4] * ((obj[base + PERIOD // 4] - obj[base + MONTHS // 4]) & MASK)
    return debt & MASK


def base_case(r):
    return {"seed": r.getrandbits(32), "obj": [r.getrandbits(32) for _ in range(OBJ_SIZE // 4)], "world": r.randrange(2), "game": r.randrange(2),
            "turns": [r.getrandbits(32), r.getrandbits(32)], "an": [[r.getrandbits(32), r.getrandbits(32)] for _ in range(2)], "centre": r.randrange(2),
            "queues": {}, "fail": -1, "fail_value": 1, "save": 0, "version": 0, "unused": 0, "loan": 0}


def put(c, offset, value):
    c["obj"][offset // 4] = value & MASK


def get(c, offset):
    return c["obj"][offset // 4]


def loan_offset(i, field):
    return LOAN0 + 0x20 * i + field


def random_loan(r, c, i, bought=None, period=None, months=None):
    if period is None:
        period = r.choice([0, 1, 2, 3, 12, 24, r.randrange(1, 40), r.getrandbits(32)])
    repay = r.choice([0, 1, 50, 100, r.randrange(0, 5000), r.getrandbits(32)])
    if months is None:
        months = r.choice([period - 1, period - 2, 0xFFFFFFFF, 0, 1, r.randrange(0, 40), r.getrandbits(32)])
    full = (repay * period) & MASK
    amount = r.choice([0, full, full, (full + 1), (full + r.randrange(1, 1000)), (full - 1), r.getrandbits(32), r.randrange(0, 20000), 0x80000000])
    if bought is None:
        bought = r.choice([0, 1, 1, r.getrandbits(32) | 1])
    for field, value in ((AVAIL, r.getrandbits(32)), (AMOUNT, amount), (APR, r.getrandbits(32)), (PERIOD, period), (REPAY, repay), (BOUGHT, bought),
                         (MONTHS, months), (LENDER, r.getrandbits(32))):
        put(c, loan_offset(i, field), value)


def mk_monthly(r, batch=None, balance=None, last=None, enabled=None, red=None, span=None, loans=None, **kw):
    c = base_case(r)
    c.update(kw)
    put(c, BATCH, r.choice([0, 999999, 1000000, -1, 1, r.getrandbits(32), r.randrange(0, 5000), r.randrange(-5000, 0)]) if batch is None else batch)
    put(c, BALANCE, r.choice([0, 1, -1, 100, -100, INT_MAX, INT_MIN, r.getrandbits(32), r.randrange(-3000, 3000)]) if balance is None else balance)
    put(c, LAST, r.choice([-1, -1, 0, 1, r.getrandbits(32), r.randrange(-10, 10)]) if last is None else last)
    put(c, ENABLED, r.choice([0, 1, 1, 1, 2, r.getrandbits(32)]) if enabled is None else enabled)
    turn = c["turns"][c["world"]]
    rel = r.choice(["lt", "eq", "gt", "any"]) if red is None else red
    put(c, RED, {"lt": turn - 1, "eq": turn, "gt": turn + 1, "any": r.getrandbits(32)}[rel] if isinstance(rel, str) else rel)
    for i in range(8):
        random_loan(r, c, i, **((loans or {}).get(i, {})))
    sp = r.choice(SPANS + [r.randrange(-2 ** 62, 2 ** 62), r.randrange(0, 12 * MONTH)]) if span is None else span
    c["queues"] = {"span": [sp], "newnull": [r.choice([0, 0, 0, 1])]}
    return c


def monthly_edges():
    r = random.Random(173)
    for batch in (0, 999999, 1000000, -1, 1, 5):
        yield mk_monthly(r, batch=batch)
    # the last payment, the one before, a wrapped count, period 0 and 1, withdrawals on and off
    for period in (0, 1, 3):
        for months in (period - 1, period - 2, 0xFFFFFFFF, 0):
            for enabled in (0, 1):
                yield mk_monthly(r, enabled=enabled, loans={i: dict(period=period, months=months, bought=1) for i in range(8)})
                yield mk_monthly(r, enabled=enabled, loans={0: dict(period=period, months=months, bought=1), 3: dict(period=period, months=months, bought=r.getrandbits(32) | 1)})
    # the balance crossing 0 from either side and the tail
    for balance in (-1, 0, 5, -5000):
        for last in (-1, 0, 7):
            for red in ("lt", "eq", "gt"):
                for span in (0, 5 * MONTH, 6 * MONTH - 1, 6 * MONTH, 6 * MONTH + 1, -6 * MONTH, 2 ** 63 - 1, -2 ** 63):
                    for centre, newnull in ((0, 0), (0, 1), (1, 0)):
                        yield mk_monthly(r, balance=balance, batch=0, last=last, red=red, span=span, centre=centre, enabled=0, loans={i: dict(bought=0) for i in range(8)})
    for span in SPANS:
        yield mk_monthly(r, balance=-100, batch=0, last=-1, red="lt", span=span, enabled=1, loans={i: dict(bought=0) for i in range(8)})


def monthly_random(r):
    return mk_monthly(r)


def mk_loaninfo(r, loan=None, pv=None, room=None):
    c = base_case(r)
    c["loan"] = r.randrange(8) if loan is None else loan
    for i in range(8):
        random_loan(r, c, i)
    value = r.choice([0, 1, -1, 2 ** 24 - 1, 2 ** 24 + 1, 2 ** 24 + 3, 0x7FFFFFC0 + r.randrange(0x40), INT_MIN, r.getrandbits(32) & MASK, r.randrange(0, 100000)]) if pv is None else pv
    value = s32(value)
    if room is None:
        room = r.choice([None, -1, 0, 1])
    if room is not None:
        put(c, loan_offset(c["loan"], AMOUNT), f32_int(value) - py_debt(c["obj"]) + room)
    c["queues"] = {"pv": [value]}
    return c


def loaninfo_edges():
    r = random.Random(1730)
    for loan in range(8):
        yield mk_loaninfo(r, loan=loan)
    for value in (0, 1, -1, 2 ** 24 - 1, 2 ** 24 + 1, 2 ** 24 + 3, 0x7FFFFFC0, 0x7FFFFFFE, 0x7FFFFFFF, INT_MIN, 100, 12345):
        for room in (-1, 0, 1):
            yield mk_loaninfo(r, pv=value, room=room)
    for amount in (0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0):
        c = mk_loaninfo(r, pv=100, room=None)
        put(c, loan_offset(c["loan"], AMOUNT), amount)
        yield c


def mk_any(r, first=None):
    c = base_case(r)
    for i in range(8):
        random_loan(r, c, i)
    pvs = [r.choice([0, 1, 100, 100000, INT_MAX, -1, INT_MIN, r.getrandbits(32), r.randrange(0, 50000)]) for _ in range(r.randrange(1, 10))]
    if first is not None:
        pvs = [r.choice([100000, INT_MAX, 5000])]
        for i in range(8):
            if i < first:
                put(c, loan_offset(i, BOUGHT), r.choice([1, 5]))
                put(c, loan_offset(i, AMOUNT), r.choice([0, 100]))
                put(c, loan_offset(i, MONTHS), get(c, loan_offset(i, PERIOD)))
            else:
                put(c, loan_offset(i, BOUGHT), 0 if i == first else r.choice([0, 1]))
                put(c, loan_offset(i, AMOUNT), 0 if i == first else r.choice([0, 0x7FFFFFFF, 0x80000000]))
    elif first == -1:
        pass
    c["queues"] = {"pv": [s32(v) for v in pvs]}
    return c


def any_edges():
    r = random.Random(1731)
    for first in range(8):
        yield mk_any(r, first)
        yield mk_any(r, first)
    # none available: every amount above the value, every loan unbought; and bought loans that are available
    for bought in (0, 1):
        c = mk_any(r)
        for i in range(8):
            put(c, loan_offset(i, BOUGHT), bought)
            put(c, loan_offset(i, AMOUNT), 0x7FFFFFFF)
        c["queues"] = {"pv": [100]}
        yield c
        c = mk_any(r)
        for i in range(8):
            put(c, loan_offset(i, BOUGHT), bought)
            put(c, loan_offset(i, AMOUNT), 0)
        c["queues"] = {"pv": [100000]}
        yield c


def mk_payoff(r, **over):
    c = base_case(r)
    c["loan"] = r.randrange(8)
    i = c["loan"]
    random_loan(r, c, i, bought=over.get("bought"), period=over.get("period"), months=over.get("months"))
    cost = (get(c, loan_offset(i, REPAY)) * ((get(c, loan_offset(i, PERIOD)) - get(c, loan_offset(i, MONTHS))) & MASK)) & MASK
    balance = over.get("balance")
    if balance is None:
        balance = r.choice([cost, cost - 1, cost + 1, -1, -5, 0, INT_MAX, INT_MIN, r.getrandbits(32), cost + r.randrange(0, 1000), cost + r.randrange(0, 1000)])
    put(c, BALANCE, balance)
    put(c, ENABLED, over["enabled"] if "enabled" in over else r.choice([0, 1, 1, 2, r.getrandbits(32)]))
    put(c, LAST, over["last"] if "last" in over else r.choice([-1, 0, 1, r.getrandbits(32), r.randrange(-10, 10)]))
    return c


def payoff_edges():
    r = random.Random(1732)
    for bought in (0, 1):
        for enabled in (0, 1):
            for dist in (0, -1, 1, 5000, -2 ** 31):
                c = mk_payoff(r, bought=bought, enabled=enabled)
                i = c["loan"]
                cost = (get(c, loan_offset(i, REPAY)) * ((get(c, loan_offset(i, PERIOD)) - get(c, loan_offset(i, MONTHS))) & MASK)) & MASK
                put(c, BALANCE, cost + dist)
                yield c
    for period in (0, 1, 12):
        for months in (0, period - 1, period + 3, 0xFFFFFFFF):
            for last in (-1, 0):
                yield mk_payoff(r, bought=1, period=period, months=months, last=last, enabled=1, balance=-7)
                yield mk_payoff(r, bought=1, period=period, months=months, last=last, enabled=1, balance=INT_MAX)
    for loan in range(8):
        c = mk_payoff(r, bought=1)
        c["loan"] = loan
        random_loan(r, c, loan, bought=1)
        put(c, BALANCE, INT_MAX)
        yield c


def mk_serialise(r, save=None, version=None, fail=-1, fail_value=None, unused=None):
    c = base_case(r)
    c["save"] = r.choice([0, 0, 1, 1, 2, -1, INT_MIN, T.rint(r)]) if save is None else save
    c["version"] = r.choice([0, 0, 1, 1, -1, 2, INT_MIN, INT_MAX, 0x80000000, T.rint(r)]) if version is None else version
    c["fail"] = fail
    c["fail_value"] = r.choice([1, -1, 2, 0x80000000, r.getrandbits(32)]) & MASK if fail_value is None else fail_value & MASK
    c["unused"] = r.choice([0, MASK, r.getrandbits(32)]) if unused is None else unused
    return c


def serialise_edges():
    r = random.Random(1733)
    for save in (0, 1, 2, -1, INT_MIN):
        for version in (-1, 0, 1, 2, INT_MIN, INT_MAX, 0x80000000):
            for fail in (-1, 0):
                yield mk_serialise(r, save, version, fail)
    for unused in (0, MASK):
        yield mk_serialise(r, 0, 1, -1, None, unused)
        yield mk_serialise(r, 1, 0, -1, None, unused)
    for save in (0, 1):
        for fail in range(0, 72):
            yield mk_serialise(r, save, fail % 2, fail)
        for fail in range(0, 9):
            yield mk_serialise(r, save, 1 - save, fail, 0x80000000)
    yield mk_serialise(r, 0, 0, 72)
    yield mk_serialise(r, 1, 1, 72)


def serialise_random(r):
    return mk_serialise(r, fail=r.choice([-1, -1] + list(range(72))))


TESTS = [
    FnTest("CBankAccount::Serialise", ppc_fn("serialise"), host_fn("serialise"), edge=serialise_edges, rnd=serialise_random),
    FnTest("CBankAccount::MonthlyUpdate", ppc_fn("monthly"), host_fn("monthly"), edge=monthly_edges, rnd=monthly_random),
    FnTest("CBankAccount::GetLoanInfo", ppc_fn("loaninfo"), host_fn("loaninfo"), edge=loaninfo_edges, rnd=mk_loaninfo),
    FnTest("CBankAccount::AnyLoanAvailable", ppc_fn("anyloan"), host_fn("anyloan"), edge=any_edges, rnd=mk_any),
    FnTest("CBankAccount::PayOffLoan", ppc_fn("payoff"), host_fn("payoff"), edge=payoff_edges, rnd=mk_payoff),
]
