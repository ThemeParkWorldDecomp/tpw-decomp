"""Differential test for unit w177, the bank account's remaining members, CChallengeBalance's destructor and virtuals and CDelayedScript (main app
"SimTheme Park"; src/game/park/BankAccount.cpp, src/game/park/ChallengeHQ.cpp, src/game/DelayedScript.cpp), against libtpw_unit_BankScripts.dylib
(tests/unit_BankScripts.cpp).

Functions (24, all run real on the PowerPC side): CBankAccount::FinalDestroy 0x100f2cb8, MakeDeposit 0x100f32ec, BeenInTheRedFor 0x100f3490, ReceiveMessage
0x100f3564, PayAdmission 0x100f3ac8, GetNumberOfLoansBought 0x100f3bd0, ApplyForLoan 0x100f3db8, OutstandingOnLoan 0x100f4278, GetCheapestLoanTaken 0x100f42e4,
GetProfitThisYear 0x100f439c, TotalOutstandingOnLoans 0x100f43dc; CChallengeBalance::~CChallengeBalance 0x100f6acc, GetBalanceDescription 0x100f9318,
GetBalanceValuesStart 0x100f9368; CDelayedScript: constructor 0x100fa898, Serialise 0x100fa8d8, SetSpriteScript 0x100faa74, IsRunningThisScript 0x100faad0,
SpawnSpriteScript 0x100fab9c, KillSpriteScript 0x100fac4c, ProcessSpriteScripts 0x100facc0, SetServiceInterval 0x100fad70 and LOADVAR / SAVEVAR of ScriptIdx
0x100fade4 / 0x100faf30. The original inlines MakeDeposit into PayAdmission and ApplyForLoan, and OutstandingOnLoan into the two loops; the host calls them.

A case is an account image (0x128 bytes over 0x40 guard bytes on each side, random words; the delayed script is its first 12 bytes), two worlds (the 0x80
bytes at world + 0x1da6f0 are real memory, the turn word and the state word mF1DA738 among them; a world pointer is that block's address minus 0x1da6f0),
two analysers (blocks reaching their words +0x1fc90 and +0x1fee0, the rest poison), the opening cash slot of gMainBalance, a message (the PowerPC one has a
vtable whose GetType is a fake, the host's an EventShape over the same layout), the fake file and the queues. Every callee is a fake on both sides that
records the same event: CThing::FinalDestroy 0x1012e9f8, CBankAccount::MonthlyUpdate 0x100f36a8, GetParkAnalyser 0x1013ebc8, IsPlayingWinningAnim 0x1013bdd0,
StartBankruptcySequence 0x1013baa4, PersonPaidAdmission 0x100e4470, GetFunnyTimeSinceGameTurn 0x10112c80, CMsg::GetType, CBalanceTable::UnInit 0x1001ed40,
operator delete, GAME_ASSERTMSG, CTRACE, ESP_SpawnScript 0x100da18c, ESP_ForceGoto 0x100da318, ESP_RURunningThis 0x100da46c, SCR_KillScript 0x100d9478,
SCR_SetServiceInterval 0x100d9630, LOADVAR / SAVEVAR of long (0x1000ee04 / 0x1000f384), CSaveLoad::WriteSerialiseLog and LbFile_Read / LbFile_Write.

Compared: the answer, the call log, the account and its guards, both analysers (the two words by value, the rest by bytes), the world tails, the cash slot
and the 8-byte span. Not tested: the callees' own work; the vptr the destructor stores into the Mac block (the compiler's, not compared); a description
row beyond +-50 (the Mac's 32-bit pointer arithmetic against the host's 64-bit); IsPlayingWinningAnim's high bytes (the host fake answers a bool).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BankScripts"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
SCRIPTS = 26   # the words of the table at 0x102a08fc (CDelayedScript's ScriptIdx values; 13 to 16 are custom data block numbers)
POISON = b"\xa5"

A = dict(FinalDestroy=0x100F2CB8, MakeDeposit=0x100F32EC, BeenInTheRedFor=0x100F3490, ReceiveMessage=0x100F3564, PayAdmission=0x100F3AC8,
         GetNumberOfLoansBought=0x100F3BD0, ApplyForLoan=0x100F3DB8, OutstandingOnLoan=0x100F4278, GetCheapestLoanTaken=0x100F42E4,
         GetProfitThisYear=0x100F439C, TotalOutstandingOnLoans=0x100F43DC, Dtor=0x100F6ACC, Desc=0x100F9318, ValuesStart=0x100F9368,
         Ctor=0x100FA898, Serialise=0x100FA8D8, SetSpriteScript=0x100FAA74, IsRunning=0x100FAAD0, Spawn=0x100FAB9C, Kill=0x100FAC4C, Process=0x100FACC0,
         SetInterval=0x100FAD70, LoadIdx=0x100FADE4, SaveIdx=0x100FAF30)
CTHING_FINAL, MONTHLY, GETAN, WIN, BANKRUPT, PAID, FUNNY, UNINIT = 0x1012E9F8, 0x100F36A8, 0x1013EBC8, 0x1013BDD0, 0x1013BAA4, 0x100E4470, 0x10112C80, 0x1001ED40
ASSERT, CTRACE, WRITELOG = 0x1000DDE8, 0x1006FCBC, 0x1015B3DC
ESP_SPAWN, ESP_GOTO, ESP_RUN, SCR_KILL, SCR_INTERVAL = 0x100DA18C, 0x100DA318, 0x100DA46C, 0x100D9478, 0x100D9630
LOADLONG, SAVELONG = 0x1000EE04, 0x1000F384
G_WORLD, G_GAME, G_SAVELOAD, G_BALANCE = 0x1037EC0C, 0x1037EBFC, 0x1037FEFC, 0x102B4568
CASH_SLOT = G_BALANCE + 0x198
DESC_TABLE = 0x1028E908
CODE_ADDRESS = 0x1029F20C
SCRIPT_TABLE = [0, 0x1029F2AC, 0x1029F308, 0x1029F364, 0x1029F388, 0x1029F424, 0x1029F460, 0x1029F4D8, 0x1029F49C, 0x1029F210, 0x1029F514, 0x1029F570,
                0x1029F68C]
FILE = 0x1000
TAIL0 = 0x1DA6F0
TURN_WORD = (0x1DA70C - TAIL0) // 4
STATE_WORD = (0x1DA738 - TAIL0) // 4
PARKTIME = 0x2A0
GUARD = 0x40
OBJ_SIZE = 0x128
OBJ_BLOCK = GUARD + OBJ_SIZE + GUARD
AN_BLOCK = GUARD + 0x1FEE4 + GUARD
AN_W5, AN_W6 = GUARD + 0x1FC90, GUARD + 0x1FEE0
OUT_BLOCK = 0x60
HOST_LAYOUT = [0x128, 0x14, 0xC, 0x10, 0x118, 0x11C, 0x120, 0x124, 0x20, 0x1FC90, 0x1FEE0, 0x1DA70C, 0x2A0, 0x1DA738, 0xC, 0, 4, 8]
# labels: 1 the file, 2 and 3 the worlds, 4 and 5 the analysers, 6 the message, 7 the balance block, 8 the object, 9 gSaveLoad, 12 and 13 the park times
L_FILE, L_WORLD, L_AN, L_MSG, L_BAL, L_OBJ, L_SAVELOAD, L_TIME = 1, 2, 4, 6, 7, 8, 9, 12

BALANCE, BATCH, FEE, LAST, RED, PROFIT = 0xC, 0x10, 0x118, 0x11C, 0x120, 0x124
LOAN0 = 0x14
AVAIL, AMOUNT, APR, PERIOD, REPAY, BOUGHT, MONTHS, LENDER = 0, 4, 8, 0xC, 0x10, 0x14, 0x18, 0x1C
NAMES = ["", "x", "mNextAnim", "A name with spaces 123", "L" * 159, "M" * 200]
BANK_FNS = ["FinalDestroy", "MakeDeposit", "BeenInTheRedFor", "ReceiveMessage", "PayAdmission", "GetNumberOfLoansBought", "ApplyForLoan", "OutstandingOnLoan",
            "GetCheapestLoanTaken", "GetProfitThisYear", "TotalOutstandingOnLoans"]


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v >> 63 else v


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("GetType", [emu.cl.lab(emu.r(3))])
        emu.set_r(3, emu.cl.msg_type & MASK)

    tvec = T.fake_function(e, get_type)
    e.mc_vtable = T.persist_alloc(e, 0x20)
    e.write(e.mc_vtable + 8, struct.pack(">I", tvec))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", [s32(e.r(3))], [e.cstr(e.r(4))[:159]])


def stub_trace(e):
    e.cl.rec("CTRACE", [e.r(3) & MASK], [e.cstr(e.r(4))[:159]])


def stub_getan(e):
    L = e.cl
    L.rec("GetParkAnalyser", [L.lab(e.r(3))])
    e.set_r(3, L.an[0 if e.r(3) == L.worlds[0] else 1])


def stub_win(e):
    e.cl.rec("IsPlayingWinningAnim", [e.cl.lab(e.r(3))])
    e.set_r(3, e.cl.ret("win", 0) & MASK)


def stub_funny(e):
    e.cl.rec("GetFunnyTimeSinceGameTurn", [e.cl.lab(e.r(4)), e.r(5) & MASK])
    e.write(e.r(3), (e.cl.ret("span", 0) & M64).to_bytes(8, "big"))


def stub_var(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), e.r(4) - L.base, s32(e.r(6))), (e.cstr(e.r(5))[:159],))
        e.set_r(3, L.ret("vans", 0) & MASK)
    return stub


def stub_spawn(e):
    L = e.cl
    L.rec("ESP_SpawnScript", [e.r(3) & MASK, s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), CL.fbits(e.f(1)), CL.fbits(e.f(2)), CL.fbits(e.f(3))])
    e.set_r(3, L.ret("spawn", 0) & MASK)


def stub_running(e):
    e.cl.rec("ESP_RURunningThis", [s32(e.r(3)), e.r(4) & MASK])
    e.set_r(3, e.cl.ret("running", 0) & MASK)


def lb_read(e):
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = L.file[L.cur:]
    taken = min(len(data), length)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = s32(L.ret("LbFile_Read") & MASK)
    if count == length and result >= 0:
        e.write(buf, (data[:taken] + bytes(length))[:length])
    L.cur += taken
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


def stub_delete(e):
    L = e.cl
    if L.lab(e.r(3)) > 0:
        L.rec("delete", (L.lab(e.r(3)),))
    L.forget(e.r(3))


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(A.values())
    S.internal(CTHING_FINAL, R("CThing::FinalDestroy", "L"))
    S.internal(MONTHLY, R("MonthlyUpdate", "L"))
    S.internal(GETAN, stub_getan)
    S.internal(WIN, stub_win)
    S.internal(BANKRUPT, R("StartBankruptcySequence", "L"))
    S.internal(PAID, R("PersonPaidAdmission", "Li"))
    S.internal(FUNNY, stub_funny)
    S.internal(UNINIT, R("CBalanceTable::UnInit", "L"))
    S.internal(ASSERT, stub_assert)
    S.internal(CTRACE, stub_trace)
    S.internal(WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
    S.internal(ESP_SPAWN, stub_spawn)
    S.internal(ESP_GOTO, R("ESP_ForceGoto", "iu"))
    S.internal(ESP_RUN, stub_running)
    S.internal(SCR_KILL, R("SCR_KillScript", "i"))
    S.internal(SCR_INTERVAL, R("SCR_SetServiceInterval", "ii"))
    S.internal(LOADLONG, stub_var("LOADVAR<long>"))
    S.internal(SAVELONG, stub_var("SAVEVAR<long>"))
    S.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    S.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
    S.imp("__dl__FPv", stub_delete)
    return S


STUBS = build_stubs()


# ---------------------------------------------------------------------------------------------------------------- the two sides


def words(order, ws):
    return struct.pack(order + "%dI" % len(ws), *[w & MASK for w in ws])


def unwords(order, data):
    return list(struct.unpack(order + "%dI" % (len(data) // 4), data))


class Ppc:
    order = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({FILE: L_FILE, G_SAVELOAD: L_SAVELOAD}, case["queues"])
        self.L.msg_type = case["mtype"]
        self.L.file, self.L.cur = case["file"], 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def blocks(self, sizes, datas):
        return [self.e.alloc(n, d) for n, d in zip(sizes, datas)]

    def read(self, a, n):
        return self.e.read(a, n)

    def globals(self, case, b):
        e, L = self.e, self.L
        tails, ans = b["tails"], b["an"]
        L.worlds = [tails[0] - TAIL0, tails[1] - TAIL0]
        L.an = [ans[0] + GUARD, ans[1] + GUARD]
        for k in range(2):
            L.labels[L.worlds[k]] = L_WORLD + k
            L.labels[L.worlds[k] + PARKTIME] = L_TIME + k
            L.labels[L.an[k]] = L_AN + k
        L.base = b["obj"] + GUARD
        L.labels[L.base] = L_OBJ
        L.labels[b["msg"]] = L_MSG
        for addr, value in ((G_WORLD, L.worlds[case["world"]]), (G_GAME, L.worlds[case["game"]]), (CASH_SLOT, case["cash"])):
            e.write(addr, struct.pack(">I", value & MASK))
        e.write(b["msg"], struct.pack(">I", e.mc_vtable))
        e.write(b["msg"] + 4, struct.pack(">I", 0))
        e.write(b["msg"] + 8, struct.pack(">I", case["mevent"] & MASK))

    def cash(self):
        return self.e.u32(CASH_SLOT)

    def call(self, fn, b, case):
        obj = b["obj"] + GUARD
        return PPC_CALLS[fn](self, obj, b, case)

    def log(self):
        return self.L.log


class Host:
    order = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.case = case
        lib.tpw_bks_set_file(case["file"], len(case["file"]))
        self.fixed = {L_FILE: FILE, L_SAVELOAD: lib.tpw_bks_addr(7)}
        CL.host_begin(lib, self.fixed, case["queues"])

    def done(self):
        pass

    def blocks(self, sizes, datas):
        out = []
        for k, (n, d) in zip((0, 1, 2, 3, 4, 6, 8)[:len(sizes)], zip(sizes, datas)):
            if k == 8:
                out.append(0)
                continue
            a = self.lib.tpw_bks_addr(k) or 0
            C.memmove(a, d, n)
            out.append(a)
        return out

    def read(self, a, n):
        return C.string_at(a, n) if a else b""

    def globals(self, case, b):
        lib = self.lib
        worlds = [b["tails"][k] - TAIL0 for k in range(2)]
        for k in range(2):
            self.fixed[L_WORLD + k] = worlds[k]
            self.fixed[L_TIME + k] = worlds[k] + PARKTIME
            self.fixed[L_AN + k] = b["an"][k] + GUARD
        self.fixed[L_OBJ] = b["obj"] + GUARD
        CL.host_begin(lib, self.fixed, case["queues"])
        lib.tpw_bks_prepare(case["world"], case["game"], s32(case["cash"]))

    def cash(self):
        return self.lib.tpw_bks_cash() & MASK

    def call(self, fn, b, case):
        return HOST_CALLS[fn](self, self.lib, b, case)

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bks_addr.restype, lib.tpw_bks_addr.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_bks_prepare.restype, lib.tpw_bks_prepare.argtypes = None, [C.c_int32] * 3
    lib.tpw_bks_set_file.restype, lib.tpw_bks_set_file.argtypes = None, [C.c_char_p, C.c_int32]
    lib.tpw_bks_cash.restype = C.c_int32
    for name in ("finaldestroy", "redfor", "payadmission", "script_ctor", "script_kill", "script_process"):
        getattr(lib, "tpw_bks_" + name).restype = None
    lib.tpw_bks_makedeposit.restype, lib.tpw_bks_makedeposit.argtypes = None, [C.c_uint32]
    lib.tpw_bks_receive.restype, lib.tpw_bks_receive.argtypes = None, [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_bks_loansbought.restype = C.c_int32
    lib.tpw_bks_apply.restype, lib.tpw_bks_apply.argtypes = C.c_int32, [C.c_uint32]
    lib.tpw_bks_outstanding.restype, lib.tpw_bks_outstanding.argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_bks_cheapest.restype = C.c_uint32
    lib.tpw_bks_profit.restype = C.c_int32
    lib.tpw_bks_total.restype = C.c_uint32
    lib.tpw_bks_balance_new.restype = C.c_void_p
    lib.tpw_bks_balance_dtor.restype, lib.tpw_bks_balance_dtor.argtypes = C.c_int32, [C.c_void_p, C.c_int32]
    lib.tpw_bks_balance_desc.restype, lib.tpw_bks_balance_desc.argtypes = C.c_int64, [C.c_int32]
    lib.tpw_bks_balance_values.restype = C.c_int64
    lib.tpw_bks_script_serialise.restype, lib.tpw_bks_script_serialise.argtypes = C.c_int32, [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_bks_script_set.restype, lib.tpw_bks_script_set.argtypes = None, [C.c_int32]
    lib.tpw_bks_script_isrunning.restype, lib.tpw_bks_script_isrunning.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_bks_script_spawn.restype, lib.tpw_bks_script_spawn.argtypes = None, [C.c_int32] * 3
    lib.tpw_bks_script_interval.restype, lib.tpw_bks_script_interval.argtypes = None, [C.c_float]
    lib.tpw_bks_script_load.restype, lib.tpw_bks_script_load.argtypes = C.c_int32, [C.c_char_p, C.c_int32]
    lib.tpw_bks_script_save.restype, lib.tpw_bks_script_save.argtypes = C.c_int32, [C.c_char_p, C.c_int32]
    lib.tpw_bks_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 24)()
    n = lib.tpw_bks_layout(out)
    assert list(out)[:18] == HOST_LAYOUT, (list(out)[:n], HOST_LAYOUT)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the calls


def ppc_call(entry, args_of, ret="u32"):
    def call(s, obj, b, case):
        r3, _, _ = T.call_ppc(s.e, entry, args_of(s, obj, b, case))
        return {"u32": r3 & MASK, "s32": s32(r3), "bool": r3 & 0xFF, "none": 0}[ret]
    return call


def ppc_redfor(s, obj, b, case):
    T.call_ppc(s.e, A["BeenInTheRedFor"], [b["out"] + 0x20, obj])
    return 0


def ppc_receive(s, obj, b, case):
    T.call_ppc(s.e, A["ReceiveMessage"], [obj, b["msg"]])
    return 0


def new_text(s, text):
    return CL.new_text(s.e, text)


PPC_CALLS = {
    "FinalDestroy": ppc_call(A["FinalDestroy"], lambda s, o, b, c: [o], "none"),
    "MakeDeposit": ppc_call(A["MakeDeposit"], lambda s, o, b, c: [o, c["arg"] & MASK], "none"),
    "BeenInTheRedFor": ppc_redfor,
    "ReceiveMessage": ppc_receive,
    "PayAdmission": ppc_call(A["PayAdmission"], lambda s, o, b, c: [o], "none"),
    "GetNumberOfLoansBought": ppc_call(A["GetNumberOfLoansBought"], lambda s, o, b, c: [o], "s32"),
    "ApplyForLoan": ppc_call(A["ApplyForLoan"], lambda s, o, b, c: [o, c["arg"]], "bool"),
    "OutstandingOnLoan": ppc_call(A["OutstandingOnLoan"], lambda s, o, b, c: [o, c["arg"]], "u32"),
    "GetCheapestLoanTaken": ppc_call(A["GetCheapestLoanTaken"], lambda s, o, b, c: [o], "u32"),
    "GetProfitThisYear": ppc_call(A["GetProfitThisYear"], lambda s, o, b, c: [o], "s32"),
    "TotalOutstandingOnLoans": ppc_call(A["TotalOutstandingOnLoans"], lambda s, o, b, c: [o], "u32"),
    "Ctor": ppc_call(A["Ctor"], lambda s, o, b, c: [o], "none"),
    "Serialise": ppc_call(A["Serialise"], lambda s, o, b, c: [o, FILE, c["save"] & MASK, c["version"] & MASK, c["unused"] & MASK], "s32"),
    "SetSpriteScript": ppc_call(A["SetSpriteScript"], lambda s, o, b, c: [o, c["arg"] & MASK], "none"),
    "IsRunning": ppc_call(A["IsRunning"], lambda s, o, b, c: [o, c["arg"]], "bool"),
    "Spawn": ppc_call(A["Spawn"], lambda s, o, b, c: [o, c["arg"], c["arg2"] & MASK, c["arg3"] & MASK], "none"),
    "Kill": ppc_call(A["Kill"], lambda s, o, b, c: [o], "none"),
    "Process": ppc_call(A["Process"], lambda s, o, b, c: [o], "none"),
    "SetInterval": ppc_call(A["SetInterval"], lambda s, o, b, c: [o, T.F(c["rate"])], "none"),
    "LoadIdx": ppc_call(A["LoadIdx"], lambda s, o, b, c: [FILE, o + 4, new_text(s, c["name"]), c["arg"] & MASK], "s32"),
    "SaveIdx": ppc_call(A["SaveIdx"], lambda s, o, b, c: [FILE, o + 4, new_text(s, c["name"]), c["arg"] & MASK], "s32"),
}


def host_receive(s, lib, b, case):
    lib.tpw_bks_receive(s32(case["mtype"]), s32(case["mevent"]), L_MSG)
    return 0


def host_redfor(s, lib, b, case):
    lib.tpw_bks_redfor()
    return 0


def host_void(name, *fields):
    def call(s, lib, b, case):
        getattr(lib, "tpw_bks_" + name)(*[case[f] for f in fields])
        return 0
    return call


def host_val(name, mask, *fields):
    def call(s, lib, b, case):
        r = getattr(lib, "tpw_bks_" + name)(*[case[f] for f in fields])
        return r & mask if mask else r
    return call


HOST_CALLS = {
    "FinalDestroy": host_void("finaldestroy"),
    "MakeDeposit": lambda s, lib, b, c: lib.tpw_bks_makedeposit(c["arg"] & MASK) or 0,
    "BeenInTheRedFor": host_redfor,
    "ReceiveMessage": host_receive,
    "PayAdmission": host_void("payadmission"),
    "GetNumberOfLoansBought": host_val("loansbought", 0),
    "ApplyForLoan": lambda s, lib, b, c: lib.tpw_bks_apply(c["arg"]) & 0xFF,
    "OutstandingOnLoan": lambda s, lib, b, c: lib.tpw_bks_outstanding(c["arg"]) & MASK,
    "GetCheapestLoanTaken": host_val("cheapest", MASK),
    "GetProfitThisYear": host_val("profit", 0),
    "TotalOutstandingOnLoans": host_val("total", MASK),
    "Ctor": host_void("script_ctor"),
    "Serialise": lambda s, lib, b, c: lib.tpw_bks_script_serialise(s32(c["save"]), s32(c["version"]), c["unused"] & MASK),
    "SetSpriteScript": lambda s, lib, b, c: lib.tpw_bks_script_set(s32(c["arg"])) or 0,
    "IsRunning": lambda s, lib, b, c: lib.tpw_bks_script_isrunning(s32(c["arg"])) & 0xFF,
    "Spawn": lambda s, lib, b, c: lib.tpw_bks_script_spawn(s32(c["arg"]), s32(c["arg2"]), s32(c["arg3"])) or 0,
    "Kill": host_void("script_kill"),
    "Process": host_void("script_process"),
    "SetInterval": lambda s, lib, b, c: lib.tpw_bks_script_interval(c["rate"]) or 0,
    "LoadIdx": lambda s, lib, b, c: lib.tpw_bks_script_load(c["name"].encode("latin-1"), s32(c["arg"])),
    "SaveIdx": lambda s, lib, b, c: lib.tpw_bks_script_save(c["name"].encode("latin-1"), s32(c["arg"])),
}


def run(s, fn, case):
    try:
        rr = random.Random(case["seed"])
        guard = [rr.getrandbits(32) for _ in range(GUARD // 4)]
        obj = words(s.order, guard + case["obj"] + [rr.getrandbits(32) for _ in range(GUARD // 4)])
        tails = []
        for k in range(2):
            ws = [rr.getrandbits(32) for _ in range(32)]
            ws[TURN_WORD], ws[STATE_WORD] = case["turns"][k], case["states"][k]
            tails.append(words(s.order, ws))
        ans = []
        for k in range(2):
            a = bytearray(POISON * AN_BLOCK)
            a[AN_W5:AN_W5 + 4] = words(s.order, [case["an"][k][0]])
            a[AN_W6:AN_W6 + 4] = words(s.order, [case["an"][k][1]])
            ans.append(bytes(a))
        sizes = [OBJ_BLOCK, 0x80, 0x80, AN_BLOCK, AN_BLOCK, OUT_BLOCK, 0x20]
        datas = [obj, tails[0], tails[1], ans[0], ans[1], POISON * OUT_BLOCK, POISON * 0x20]
        got = s.blocks(sizes, datas)
        b = {"obj": got[0], "tails": got[1:3], "an": got[3:5], "out": got[5], "msg": got[6]}
        s.globals(case, b)
        result = s.call(fn, b, case)
        out = {"result": result, "obj": unwords(s.order, s.read(b["obj"], OBJ_BLOCK)),
               "tails": [unwords(s.order, s.read(b["tails"][k], 0x80)) for k in range(2)], "cash": s.cash()}
        for k in range(2):
            image = bytearray(s.read(b["an"][k], AN_BLOCK))
            out["an%d" % k] = unwords(s.order, bytes(image[AN_W5:AN_W5 + 4]) + bytes(image[AN_W6:AN_W6 + 4]))
            image[AN_W5:AN_W5 + 4] = b"\0\0\0\0"
            image[AN_W6:AN_W6 + 4] = b"\0\0\0\0"
            out["an_rest%d" % k] = bytes(image)
        raw = s.read(b["out"], OUT_BLOCK)
        out["span"] = struct.unpack(s.order + "q", raw[0x20:0x28])[0]
        out["out_rest"] = raw[:0x20] + raw[0x28:]
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc_fn(fn):
    return lambda e, case: run(Ppc(e, case), fn, case)


def host_fn(fn):
    return lambda lib, case: run(Host(lib, case), fn, case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base_case(r):
    return {"seed": r.getrandbits(32), "obj": [r.getrandbits(32) for _ in range(OBJ_SIZE // 4)], "world": r.randrange(2), "game": r.randrange(2),
            "turns": [r.getrandbits(32), r.getrandbits(32)], "states": [r.choice([4, 4, 0, 1, 2, 3, r.getrandbits(32)]) for _ in range(2)],
            "an": [[r.getrandbits(32), r.getrandbits(32)] for _ in range(2)], "queues": {}, "cash": r.choice([0, 1, -1, 100, INT_MAX, INT_MIN, r.getrandbits(32)]) & MASK,
            "file": b"", "arg": 0, "arg2": 0, "arg3": 0, "save": 0, "version": 0, "unused": 0, "mtype": 0, "mevent": 0, "rate": 1.0, "name": "x"}


def put(c, offset, value):
    c["obj"][offset // 4] = value & MASK


def get(c, offset):
    return c["obj"][offset // 4]


def loan_offset(i, field):
    return LOAN0 + 0x20 * i + field


def random_loan(r, c, i, bought=None, period=None, months=None, repay=None, amount=None):
    if period is None:
        period = r.choice([0, 1, 2, 3, 12, 24, r.randrange(1, 40), r.getrandbits(32)])
    if repay is None:
        repay = r.choice([0, 1, 50, 100, r.randrange(0, 5000), r.getrandbits(32), 0x10000, 0x10001])
    if months is None:
        months = r.choice([period - 1, period - 2, 0xFFFFFFFF, 0, 1, r.randrange(0, 40), r.getrandbits(32), period + 1])
    if amount is None:
        amount = r.choice([0, 1, 999999, 1000000, 1000001, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(0, 20000), r.randrange(0, 2000000)])
    if bought is None:
        bought = r.choice([0, 1, 1, r.getrandbits(32) | 1, 0x80000000, 0x100])
    for field, value in ((AVAIL, r.getrandbits(32)), (AMOUNT, amount), (APR, r.getrandbits(32)), (PERIOD, period), (REPAY, repay), (BOUGHT, bought),
                         (MONTHS, months), (LENDER, r.getrandbits(32))):
        put(c, loan_offset(i, field), value)


def loans_case(r, **kw):
    c = base_case(r)
    c.update(kw)
    for i in range(8):
        random_loan(r, c, i)
    return c


def set_cash(r, c):
    put(c, BALANCE, r.choice([0, 1, -1, 100, INT_MAX, INT_MIN, 0x7FFFF000, r.getrandbits(32), r.randrange(-3000, 3000)]))
    put(c, PROFIT, r.choice([0, 1, -1, INT_MAX, INT_MIN, r.getrandbits(32), r.randrange(-3000, 3000)]))
    put(c, FEE, r.choice([0, 1, 10, 999999, 1000000, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 2000000)]))
    put(c, BATCH, r.getrandbits(32))


def mk_simple(r, **kw):
    c = base_case(r)
    c.update(kw)
    set_cash(r, c)
    return c


def edge_from(maker, seed, variants):
    def edges():
        r = random.Random(seed)
        for v in variants:
            yield maker(r, **v)
    return edges


def mk_deposit(r, amount=None):
    c = mk_simple(r)
    c["arg"] = r.choice([0, 1, 999999, 1000000, 1000001, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.randrange(0, 2000000), r.randrange(0, 5000)]) \
        if amount is None else amount
    return c


def deposit_edges():
    r = random.Random(1771)
    for amount in (0, 1, 999999, 1000000, 1000001, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 5):
        for bal in (0, INT_MAX, INT_MIN, -1):
            c = mk_deposit(r, amount)
            put(c, BALANCE, bal)
            put(c, PROFIT, bal)
            yield c


def mk_red(r, last=None, red=None, turn=None):
    c = mk_simple(r)
    c["turns"] = [r.getrandbits(32), r.getrandbits(32)] if turn is None else [turn, turn]
    t = c["turns"][c["world"]]
    put(c, LAST, r.choice([-1, -1, 0, 1, INT_MIN, INT_MAX, r.getrandbits(32), r.randrange(-10, 10)]) if last is None else last)
    rel = r.choice(["lt", "eq", "gt", "any", "max"]) if red is None else red
    put(c, RED, {"lt": t - 1, "eq": t, "gt": t + 1, "any": r.getrandbits(32), "max": MASK}[rel] if isinstance(rel, str) else rel)
    c["queues"] = {"span": [r.choice([0, 1, -1, 25920000000000, 2 ** 63 - 1, -2 ** 63, r.randrange(-2 ** 62, 2 ** 62)])]}
    return c


def red_edges():
    r = random.Random(1772)
    for last in (-1, 0, 1, INT_MIN, INT_MAX):
        for red in ("lt", "eq", "gt", "max"):
            for turn in (0, 1, MASK, 0x80000000, 1000):
                yield mk_red(r, last, red, turn)


def mk_recv(r, mtype=None, event=None, state=None, win=None):
    c = mk_simple(r)
    c["mtype"] = r.choice([0xB, 0xC, 0xD, 0x13, 0x14, -1, 0, 1, 0x12, INT_MIN, INT_MAX, r.getrandbits(32)]) if mtype is None else mtype
    c["mevent"] = r.choice([2, 2, 10, 10, 0, 3, -1, 1, 9, 11, r.getrandbits(32)]) if event is None else event
    if state is not None:
        c["states"][c["game"]] = state
    c["queues"] = {"win": [r.randrange(2) if win is None else win]}
    return c


def recv_edges():
    r = random.Random(1773)
    for mtype in (0xB, 0xC, 0xD, 0x13, 0x14, -1, 0, 0x12, INT_MIN, INT_MAX):
        for event in (2, 10, 0):
            yield mk_recv(r, mtype, event)
    for event in (2, 10, 0, 3, -1, 1, 9, 11, 0x80000002):
        for state in (4, 0, 3, 5, 0x80000004, MASK):
            for win in (0, 1):
                yield mk_recv(r, 0x13, event, state, win)


def mk_pay(r):
    return mk_simple(r)


def pay_edges():
    r = random.Random(1774)
    for fee in (0, 1, 999999, 1000000, 0xFFFFFFFF, 0x80000000, 5):
        for bal in (0, INT_MAX, -1):
            c = mk_pay(r)
            put(c, FEE, fee)
            put(c, BALANCE, bal)
            yield c


def mk_loan_arg(r, loan=None):
    c = loans_case(r)
    set_cash(r, c)
    c["arg"] = r.randrange(8) if loan is None else loan
    return c


def apply_edges():
    r = random.Random(1775)
    for loan in range(8):
        for bought in (0, 1, 0x100, 0x80000000, MASK):
            c = mk_loan_arg(r, loan)
            put(c, loan_offset(loan, BOUGHT), bought)
            yield c
    for amount in (0, 1, 999999, 1000000, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF):
        for loan in (0, 7):
            c = mk_loan_arg(r, loan)
            put(c, loan_offset(loan, BOUGHT), 0)
            put(c, loan_offset(loan, AMOUNT), amount)
            put(c, BALANCE, r.choice([0, INT_MAX, INT_MIN]))
            put(c, PROFIT, r.choice([0, INT_MAX, INT_MIN]))
            yield c


def outstanding_edges():
    r = random.Random(1776)
    for loan in range(8):
        for bought in (0, 1, MASK):
            for period, months in ((0, 0), (12, 12), (12, 3), (3, 12), (0, 1), (0xFFFFFFFF, 1), (5, 0xFFFFFFFF), (0x10000, 0)):
                for repay in (0, 1, 7, 0x10000, 0x10001, MASK):
                    c = mk_loan_arg(r, loan)
                    for field, value in ((BOUGHT, bought), (PERIOD, period), (MONTHS, months), (REPAY, repay)):
                        put(c, loan_offset(loan, field), value)
                    yield c


def cheapest_edges():
    r = random.Random(1777)
    base = dict(period=10, months=0, repay=100)
    for _ in range(1):
        c = loans_case(r)                                  # all zero
        for i in range(8):
            put(c, loan_offset(i, BOUGHT), 0)
        yield c
    for only in range(8):                                  # one only
        c = loans_case(r)
        for i in range(8):
            random_loan(r, c, i, bought=1 if i == only else 0, **base)
        yield c
    for equal in ((0, 1), (3, 5), (6, 7), (0, 7), (2, 3, 4)):         # equal outstandings
        c = loans_case(r)
        for i in range(8):
            random_loan(r, c, i, bought=1 if i in equal else 0, **base)
        yield c
    for i in range(8):                                     # the cheapest at each place, then a larger after it
        c = loans_case(r)
        for j in range(8):
            random_loan(r, c, j, bought=1, period=10, months=0, repay=100 + (50 if j != i else 0))
        yield c
        c = loans_case(r)
        for j in range(8):
            random_loan(r, c, j, bought=1, period=10, months=0, repay=100 - (j if j > i else -50))
        yield c
    for wrap in range(3):                                  # a wrapping product (a huge unsigned outstanding)
        c = loans_case(r)
        for j in range(8):
            random_loan(r, c, j, bought=r.choice([0, 1]))
        random_loan(r, c, wrap, bought=1, period=0, months=1, repay=2)      # 2 * 0xffffffff
        yield c
    c = loans_case(r)
    for j in range(8):
        random_loan(r, c, j, bought=1, period=5, months=5, repay=r.getrandbits(32))      # every product zero
    yield c


def mk_loans_plain(r):
    c = loans_case(r)
    set_cash(r, c)
    return c


def ptr_case(r, **kw):
    return mk_simple(r)


# ---- CChallengeBalance
def mk_bal(r, null=None, flag=None, row=None):
    c = base_case(r)
    c["null"] = r.random() < 0.15 if null is None else null
    c["arg"] = r.choice([0, 1, -1, 0x10000, 0x18000, 0x8000, 0x7FFF, 0xFFFF, 2, 0x10001, MASK, INT_MIN, r.getrandbits(32)]) if flag is None else flag
    c["arg2"] = r.choice([0, 12, 13, 1, 5, -1, -2, 30, r.randrange(-50, 50)]) if row is None else row
    return c


def bal_edges():
    r = random.Random(1778)
    for null in (True, False):
        for flag in (0, 1, -1, 0x10000, 0x18000, 0x8000, 0x7FFF, 0xFFFF, 2, 0x10001, 0x7FFF0000, MASK):
            yield mk_bal(r, null, flag)


def desc_edges():
    r = random.Random(1779)
    for row in (0, 1, 12, 13, 59, 60, -1, -2, 50, -50, 5):
        yield mk_bal(r, False, 0, row)


def bal_run(fn):
    def ppc(e, case):
        s = Ppc(e, case)
        try:
            blk = e.alloc(0x970, bytes(rr % 251 for rr in range(0x970)))
            s.L.labels[blk] = L_BAL
            if fn == "Dtor":
                r3 = T.call_ppc(e, A["Dtor"], [0 if case["null"] else blk, case["arg"] & MASK])[0] & MASK
                res = 0 if r3 == 0 else L_BAL if r3 == blk else r3
            elif fn == "Desc":
                res = s32(T.call_ppc(e, A["Desc"], [blk, case["arg2"] & MASK])[0] - DESC_TABLE)
            else:
                res = (T.call_ppc(e, A["ValuesStart"], [blk])[0] - blk) & MASK == 8
            return {"result": res, "log": s.L.log}
        finally:
            s.done()

    def host(lib, case):
        s = Host(lib, case)
        p = lib.tpw_bks_balance_new()
        if fn == "Dtor":
            res = lib.tpw_bks_balance_dtor(None if case["null"] else p, s32(case["arg"]))
        elif fn == "Desc":
            res = s32(lib.tpw_bks_balance_desc(s32(case["arg2"])) // 0x3c * 0x3c) if False else lib.tpw_bks_balance_desc(s32(case["arg2"]))
        else:
            res = lib.tpw_bks_balance_values() == HOST_LAYOUT_VALUES[0]
        return {"result": res, "log": CL.host_end(lib)}
    return ppc, host


HOST_LAYOUT_VALUES = [0]


def bal_bind(lib):
    bind(lib)
    out = (C.c_int64 * 24)()
    n = lib.tpw_bks_layout(out)
    HOST_LAYOUT_VALUES[0] = list(out)[:n][-2] if False else list(out)[18]


# ---- CDelayedScript
def mk_script(r, **kw):
    c = base_case(r)
    c["arg"] = r.randrange(SCRIPTS)
    c["arg2"] = r.choice([4, 5, 6, 7, 8, 0, 1, 100, -1, r.getrandbits(32)])
    c["arg3"] = r.choice([0, 1, -1, 7, r.getrandbits(32)])
    put(c, 0, r.choice([0, 0, 1, -1, 5, 99, r.getrandbits(32)]))
    put(c, 4, r.choice([0, 0, 1, 2, 12, 13, -1, r.getrandbits(32)]) if False else r.choice([0, 0, 1, 2, 5, 12, 14, 25, r.randrange(SCRIPTS)]))
    put(c, 8, r.choice([0, 0, 1, 250, 251, -1, r.getrandbits(32), r.randrange(0, 300)]))
    c["queues"] = {"running": [r.choice([0, 0, 1, -1, 0x100, 0x80000000])], "spawn": [r.choice([0, 1, -1, 7, r.getrandbits(31)])]}
    c.update(kw)
    return c


def script_idx_edges(extra=None):
    def edges():
        r = random.Random(1780)
        for idx in range(SCRIPTS):
            for na in (0, idx, min(idx + 1, SCRIPTS - 1), 12, 25):
                for sprite in (0, 1, 3):
                    c = mk_script(r)
                    c["arg"] = idx
                    put(c, 4, na)
                    put(c, 0, r.choice([0, 1, -1, 77]))
                    yield c
    return edges


def process_edges():
    r = random.Random(1781)
    for idx in range(SCRIPTS):
        for iv in (0, 1, 250, 0xFFFFFFFF):
            for handle in (0, -1, 5):
                c = mk_script(r)
                put(c, 0, handle)
                put(c, 4, idx)
                put(c, 8, iv)
                yield c
    for na, iv in ((0, 0), (0, 3), (3, 0), (0, 0xFFFFFFFF)):
        c = mk_script(r)
        put(c, 4, na)
        put(c, 8, iv)
        yield c


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0] if not (math.isnan(x) or math.isinf(x)) else x


RATES = [1.0, 62.0, 0.248, 0.24, 0.2480001, 0.2479999, 0.0, -0.0, -1.0, 1e-30, math.inf, -math.inf, math.nan, 1e30, -1e30, 0.5, 0.25, 0.1, 0.01, 2.0, 31.0, 62.5,
         61.0, 124.0, 6.2, 0.2, 0.3, 1.5, 100.0, 1e-5, 1e-8, 3e-8, 2.9e-8]


def saturates(rate):
    """62 / rate reaching 2^31 or more: the hardware fctiwz saturates to 0x7fffffff, Unicorn's gives 0x80000000 (doubtful list)."""
    q = f32(62.0 / rate) if rate != 0 else math.inf
    return not math.isnan(q) and q >= 2147483648.0


def mk_rate(r, rate=None):
    c = mk_script(r)
    while rate is None or (saturates(rate) and rate not in RATES):
        rate = r.choice(RATES + [T.rfloat(r), r.uniform(0.0, 3.0), r.uniform(-2.0, 2.0), 62.0 / r.randrange(1, 400), r.uniform(0.1, 0.4)])
        if saturates(rate):
            rate = None
    if saturates(rate):
        rate = 1.0
    c["rate"] = f32(rate)
    return c


def rate_edges():
    r = random.Random(1782)
    for rate in RATES:
        if not saturates(rate):
            yield mk_rate(r, rate)
    for n in range(1, 300):
        yield mk_rate(r, f32(62.0 / n))


def mk_ser(r, save=None, version=None, nonzero=None, file=None):
    c = mk_script(r)
    c["save"] = r.choice([0, 1, 2, -1, INT_MIN, r.getrandbits(32)]) if save is None else save
    c["version"] = r.choice([0, 1, 1, 0, -1, 2, INT_MIN, INT_MAX, 0x80000000, r.getrandbits(32)]) if version is None else version
    c["unused"] = r.choice([0, MASK, r.getrandbits(32)])
    q = {"vans": ([0] * nonzero + [r.choice([1, -1, 5])]) if nonzero is not None else [0]}
    data = b"".join(struct.pack("<I", r.getrandbits(32)) for _ in range(4)) if file is None else file
    c["file"] = data
    if r.random() < 0.15:
        q["LbFile_Read"] = [r.choice([-1, 5, INT_MIN])]
        q["LbFile_Write"] = [r.choice([-1, 5, INT_MIN])]
    if r.random() < 0.15:
        q["LbFile_Read:xfer"] = [r.choice([0, 1, 3, 5, MASK])]
        q["LbFile_Write:xfer"] = [r.choice([0, 1, 3, 5, MASK])]
    c["queues"].update(q)
    return c


def ser_edges():
    r = random.Random(1783)
    for save in (0, 1, 2, -1, INT_MIN):
        for version in (-1, 0, 1, 2, INT_MIN, INT_MAX, 0x80000000):
            yield mk_ser(r, save, version, None)
    for save in (0, 1):
        for nonzero in (0, 1, 2, 3):
            for version in (0, 1):
                yield mk_ser(r, save, version, nonzero)
    for save in (0, 1):
        for cut in range(0, 13):
            yield mk_ser(r, save, 1, None, bytes(range(1, 13))[:cut])
        for res in (-1, 5, INT_MIN):
            c = mk_ser(r, save, 0, None)
            c["queues"]["LbFile_Read"] = [res]
            c["queues"]["LbFile_Write"] = [res]
            yield c
        for xfer in (0, 1, 3, 5, MASK):
            c = mk_ser(r, save, 0, None)
            c["queues"]["LbFile_Read:xfer"] = [xfer]
            c["queues"]["LbFile_Write:xfer"] = [xfer]
            yield c


def mk_ser_rnd(r):
    return mk_ser(r, nonzero=r.choice([None, None, 0, 1, 2, 3]))


def mk_inst(r, load, **kw):
    c = mk_script(r)
    c["name"] = r.choice(NAMES) if r.random() < 0.5 else "".join(r.choice("abcXYZ_[]019 ") for _ in range(r.randint(0, 30)))
    c["arg"] = r.choice([0, 1, -1, 2, 0x80000000, r.getrandbits(32)])
    c["file"] = struct.pack("<I", r.getrandbits(32)) if load else b""
    if load and r.random() < 0.1:
        c["file"] = c["file"][:r.randint(0, 3)]
    key = "LbFile_Read" if load else "LbFile_Write"
    if r.random() < 0.2:
        c["queues"][key] = [r.choice([0, 1, 5, INT_MAX, -1, -2, INT_MIN])]
    if r.random() < 0.2:
        c["queues"][key + ":xfer"] = [r.choice([0, 1, 3, 4, 5, MASK, r.randint(0, 9)])]
    c.update(kw)
    return c


def inst_edges(load):
    def edges():
        r = random.Random(1784 + load)
        key = "LbFile_Read" if load else "LbFile_Write"
        for name in NAMES:
            for no_log in (0, 1, -1, 2, 0x80000000):
                yield mk_inst(r, load, name=name, arg=no_log)
        for res in (0, 1, 5, INT_MAX, -1, -2, INT_MIN):
            c = mk_inst(r, load)
            c["queues"][key] = [res]
            yield c
        for count in (0, 1, 3, 4, 5, 8, MASK, 0xFFFFFFFC):
            c = mk_inst(r, load)
            c["queues"][key + ":xfer"] = [count]
            yield c
            c = mk_inst(r, load)
            c["queues"][key + ":xfer"] = [count]
            c["queues"][key] = [-1]
            yield c
        if load:
            for data in (b"", b"\x01\x02\x03", b"\x01\x02\x03\x04", b"\x01\x02\x03\x04\x05\x06\x07\x08"):
                c = mk_inst(r, load)
                c["file"] = data
                yield c
    return edges


def bal_fn(fn):
    return bal_run(fn)


def mk_bal_test(name, fn, edge):
    ppc, host = bal_run(fn)
    return FnTest(name, ppc, lambda lib, case: (bal_bind(lib), host(lib, case))[1], edge=edge, rnd=mk_bal)


def simple_test(name, key, edge, rnd):
    return FnTest(name, ppc_fn(key), host_fn(key), edge=edge, rnd=rnd)


def plain_edges(seed, n=1):
    def edges():
        r = random.Random(seed)
        for _ in range(n):
            yield mk_simple(r)
    return edges


def loansonly_edges(seed):
    def edges():
        r = random.Random(seed)
        for bought in (0, 1, 2, MASK):
            c = mk_loans_plain(r)
            for i in range(8):
                put(c, loan_offset(i, BOUGHT), bought)
            yield c
        for mask in range(0, 256, 5):
            c = mk_loans_plain(r)
            for i in range(8):
                put(c, loan_offset(i, BOUGHT), 1 if mask >> i & 1 else 0)
            yield c
    return edges


TESTS = [
    simple_test("CBankAccount::FinalDestroy", "FinalDestroy", plain_edges(1770, 3), mk_simple),
    simple_test("CBankAccount::MakeDeposit", "MakeDeposit", deposit_edges, mk_deposit),
    simple_test("CBankAccount::BeenInTheRedFor", "BeenInTheRedFor", red_edges, mk_red),
    simple_test("CBankAccount::ReceiveMessage", "ReceiveMessage", recv_edges, mk_recv),
    simple_test("CBankAccount::PayAdmission", "PayAdmission", pay_edges, mk_pay),
    simple_test("CBankAccount::GetNumberOfLoansBought", "GetNumberOfLoansBought", loansonly_edges(1775), mk_loans_plain),
    simple_test("CBankAccount::ApplyForLoan", "ApplyForLoan", apply_edges, mk_loan_arg),
    simple_test("CBankAccount::OutstandingOnLoan", "OutstandingOnLoan", outstanding_edges, mk_loan_arg),
    simple_test("CBankAccount::GetCheapestLoanTaken", "GetCheapestLoanTaken", cheapest_edges, mk_loans_plain),
    simple_test("CBankAccount::GetProfitThisYear", "GetProfitThisYear", plain_edges(1778, 4), mk_simple),
    simple_test("CBankAccount::TotalOutstandingOnLoans", "TotalOutstandingOnLoans", loansonly_edges(1779), mk_loans_plain),
    mk_bal_test("CChallengeBalance::~CChallengeBalance", "Dtor", bal_edges),
    mk_bal_test("CChallengeBalance::GetBalanceDescription", "Desc", desc_edges),
    mk_bal_test("CChallengeBalance::GetBalanceValuesStart", "Values", lambda: iter([mk_bal(random.Random(5))])),
    simple_test("CDelayedScript::CDelayedScript", "Ctor", plain_edges(1790, 2), mk_script),
    simple_test("CDelayedScript::Serialise", "Serialise", ser_edges, mk_ser_rnd),
    simple_test("CDelayedScript::SetSpriteScript", "SetSpriteScript", script_idx_edges(), mk_script),
    simple_test("CDelayedScript::IsRunningThisScript", "IsRunning", script_idx_edges(), mk_script),
    simple_test("CDelayedScript::SpawnSpriteScript", "Spawn", script_idx_edges(), mk_script),
    simple_test("CDelayedScript::KillSpriteScript", "Kill", plain_edges(1791, 3), mk_script),
    simple_test("CDelayedScript::ProcessSpriteScripts", "Process", process_edges, mk_script),
    simple_test("CDelayedScript::SetServiceInterval", "SetInterval", rate_edges, mk_rate),
    simple_test("LOADVAR<CDelayedScript::ScriptIdx>", "LoadIdx", inst_edges(1), lambda r: mk_inst(r, True)),
    simple_test("SAVEVAR<CDelayedScript::ScriptIdx>", "SaveIdx", inst_edges(0), lambda r: mk_inst(r, False)),
]
