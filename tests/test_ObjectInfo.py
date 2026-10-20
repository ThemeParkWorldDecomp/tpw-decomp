"""Unit w210: a ride's names, kind, excitement and stand point, and its final destroy. The 16 functions are CObject's
UpgradesWhat, GetControl, GetFileName, GetUIName, GetSignNameA/B, SetSignNameA/B, IsCoaster, IsTrackRide,
GetProspectiveExcitementLevel, GetExcitementLevel, GetStandPoint and FinalDestroy (src/Object_info.cpp), the constructor
of CMsgRideDeleted (src/ThingMessages2.cpp) and CRideDatabase::GetFileName (src/RideDB_names.cpp).

A case is a dict. "op" names the function, "seed" fills the object block (0x448 bytes between two guard blocks of 0x40)
and the world tail with the same bytes on both sides, and the other keys are what the function reads or writes: the
object fields by their offset names (tid, cx, cy, type, rot, f20, f24, flags, f32, f34, f4c, f50, f54, f58, f59), the
ride record's value slots (wtt = WhichTrackType, exc = ExcitementLevel, dur and spd = the four levels of
Upgrades_InitDuration and Upgrades_InitSpeed, upg = UpgradesId, sx sy = the entry stand point, ex ey = the exit appear
point, uitype = mUIType, 2 is a sideshow), the sign texts (sa and sb are the object's mSignA and mSignB, text is the
argument of SetSignNameA/B), the game type (gtype, guard, ctortype), the world (gates, f738) and "queues": what the fakes
answer, by the name of the fake's event (the last answer repeats). Every other slot of the ride record holds seeded noise.

Faked on both sides, each call one event of the log (name, arguments in order; compared in order): GetRideInfo (the id is
the high halfword on the PowerPC), CControlManager::GetObjectControl, CWorld::Unregister, CWorld::GetBankAccount,
CBankAccount::MakeDeposit, CMap::RemoveRegionalEffector (the cell is the high halfword), PostMessage (the message's type,
flags and id are read at the call), GetThingId, CThing::FinalDestroy, the CObject queries (IsDirty,
GetCurrentResidualValue, GetPricePerUse, GetPrizeValue, GetChanceOfWinning), RSSE_GetScriptPointer, RSSE_Getvar,
SCR_KillScript, RSSE_KillScript, RSSE_GetCoasterHandle, GetCoasterPeepStats, BMP_CountTrackSections, CGameType's
constructor, the master save's constructor, SetSignNameA/B and gMasterSave afterwards, RecordAction, GAME_ASSERTMSG,
operator new (nothrow, "new" event with the size and whether it failed) and the message centre's array construction
(event "CMessageCentre"). wcslen, wcsncpy and swprintf are not events: they are the platform's on both sides.
The unit's own functions run as the original on the PowerPC side (IsCoaster, GetControl, CMsgRideDeleted's constructor,
CRideDatabase::GetFileName, GetProspectiveExcitementLevel and the rest), so a translation is checked as soon as its own
callees are done. Order: IsCoaster, CRideDatabase::GetFileName, CMsgRideDeleted::CMsgRideDeleted, GetControl,
GetProspectiveExcitementLevel, then their callers.

Compared per case: "answer" (a pointer is compared as a label: 0 for NULL, the label of the block it points into, -1
otherwise), "log" (the events), "obj" (the object block and its two guards, every byte; the decoded fields by value),
"tail" (the world's tail), "ctl" (the control block), "out" and "bufs" (the sign names: the caller's buffer with
8 units of guard and the two scratch buffers), "x" and "y" (the stand point), "game" (the game type and its guard),
"db" (the ride database's count, cached id and cached ride), "msg", "crashed" (a host crash is caught, not fatal).

Not tested: sign texts longer than 33 units for the names (the original overruns its 0x21-unit scratch buffers), the
u8 mask of the length RecordAction gets, zero over zero or NaN ratios and NaN or infinite values reaching a full-word
conversion in GetProspectiveExcitementLevel, the unused bytes of CMsgRideDeleted (the host object is 16 bytes, the Mac's 12).
"""
import ctypes as C
import itertools
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ObjectInfo"
MASK = 0xFFFFFFFF

ENTRY = dict(
    upgrades=0x1011050C, control=0x10110580, fname=0x1011062C, uiname=0x10110690, signa=0x101107C4,
    signb=0x101108DC, setsigna=0x10110948, setsignb=0x10110B10, coaster=0x10110CD8, track=0x10110D50,
    prospect=0x1010CBA0, excite=0x1010CFB0, stand=0x1010A4FC, destroy=0x10107828, msg=0x10151BA4,
    dbname=0x10154930,
)
SIZES = dict(
    upgrades=72, control=72, fname=56, uiname=264, signa=232, signb=60, setsigna=412, setsignb=412, coaster=76,
    track=200, prospect=976, excite=252, stand=1028, destroy=724, msg=40, dbname=128,
)
OWN = set(ENTRY.values())
PROSPECT, STAND = ENTRY["prospect"], ENTRY["stand"]

GUARD = 0x40
OBJ = 0x448
OBJ_BLOCK = GUARD + OBJ + GUARD
TAIL = 0x40
TAIL0 = 0x1DA70C        # CWorld::mTurn: the tail block starts there, mGatesThing is at +0x26, mF1DA738 at +0x2c
CTLMGR = 0x1D943C       # CWorld::GetControlManager() is this + 0x1d943c
INFO_MAC = 0x4400
INFO_HOST = 0x20000
BANK_ADDR = 0x6000
MAP_ADDR = 0x7000
G_WORLD = 0x1037EC0C
G_MAP = 0x1037EC08
G_DB = 0x1037F6C8
G_CENTRE = 0x1037F6C4
G_MASTER = 0x10380A8C
G_UI = 0x10396440
G_REC = 0x1037EC44
GAME_TYPE = 0x102B3AA0
GAME_GUARD = 0x102B3AA4
BUF_SIGN = 0x1034C2D8
BUF_UI = 0x1034C31A
RIDE_DELETED_VT = 0x102A0C9C
CENTRE_CTOR, CENTRE_DTOR = 0x102657E8, 0x10267288
NAME_OFF = 0x68C

L_OBJ, L_DB, L_UI, L_BANK, L_MAP, L_REC, L_WORLD, L_CTLMGR = 1, 2, 3, 4, 5, 6, 7, 8
L_CTL, L_OTHER, L_CENTRE, L_MASTER, L_TEXT, L_SCRIPT = 9, 10, 11, 12, 13, 14

INFO_OPS = ("upgrades", "coaster", "track", "prospect", "excite", "stand")
NAME_OPS = ("uiname", "signa", "signb")
SET_OPS = ("setsigna", "setsignb")
DB_OPS = ("fname", "dbname")
OBJ_FIELDS = {
    "tid": (0, 2), "cx": (4, 1), "cy": (6, 1), "type": (0xE, 2), "rot": (0x10, 4), "f20": (0x20, 4), "f24": (0x24, 4),
    "flags": (0x2E, 2), "f32": (0x32, 2), "f34": (0x34, 2), "f4c": (0x4C, 1), "f50": (0x50, 4), "f54": (0x54, 4),
    "f58": (0x58, 1), "f59": (0x59, 1),
}
HOST_NAMES = dict(
    upgrades="upgrades_what", control="get_control", fname="get_file_name", uiname="ui_name", signa="sign_a",
    signb="sign_b", setsigna="set_sign_a", setsignb="set_sign_b", coaster="is_coaster", track="is_track_ride",
    prospect="prospective", excite="excitement", stand="stand_point", destroy="final_destroy", msg="msg_make",
    dbname="db_file_name",
)


def s32(v):
    return T.s32(v)


def put_fmt(order, size):
    return order + {1: "B", 2: "H", 4: "I", 8: "Q"}[size]


def u16(e, address):
    return struct.unpack(">H", e.read(address, 2))[0]


def keep_toc(e):
    # a glue call stores the caller's TOC at 0x14(r1) and the caller reloads r2 from there
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def wide_units(e, address, limit=512):
    units = []
    while len(units) < limit:
        u = u16(e, address + 2 * len(units))
        if u == 0:
            break
        units.append(u)
    return units


def wide(e, address):
    return "".join(chr(u) for u in wide_units(e, address))


STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder

FAKES = [
    (0x10154868, R("GetRideInfo", "Lw", ret=lambda e, L: L.info)),
    (0x1012E9F8, R("CThing::FinalDestroy", "L")),
    (0x100FA0B8, R("CControlManager::GetObjectControl", "Lwi", ret=lambda e, L: L.ctl)),
    (0x1013FAB4, R("CWorld::Unregister", "LL")),
    (0x1013EDFC, R("CWorld::GetBankAccount", "L", ret=lambda e, L: BANK_ADDR)),
    (0x100F32EC, R("CBankAccount::MakeDeposit", "Lu")),
    (0x1010176C, R("CMap::RemoveRegionalEffector", "Liw")),
    (0x101694DC, R("CFSGlobalMasterSave", "N")),
    (0x101EFFA4, R("UIInterface::GetSelectedObject", "L", ret=lambda e, L: L.selected)),
    (0x101F0000, R("UIInterface::SetSelectedObject", "LL")),
    (0x1010C900, R("CObject::IsDirty", "L", ret="i")),
    (0x1010FFC8, R("CObject::GetCurrentResidualValue", "L", ret="i")),
    (0x1010F4F4, R("CObject::GetPricePerUse", "L", ret="i")),
    (0x1010F52C, R("CObject::GetPrizeValue", "L", ret="i")),
    (0x1010FBB8, R("CObject::GetChanceOfWinning", "L", ret="i")),
    (0x100D668C, R("RSSE_GetScriptPointer", "i", ret=lambda e, L: L.script)),
    (0x100D6C58, R("RSSE_Getvar", "Li", ret="i")),
    (0x100D9478, R("SCR_KillScript", "i")),
    (0x100D42D0, R("RSSE_KillScript", "ii")),
    (0x100D6F34, R("RSSE_GetCoasterHandle", "i", ret="i")),
]
for address, fake in FAKES:
    STUBS.internal(address, fake)


def stub_get_thing_id(e):
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), e.read(e.r(4), 2))


def stub_post_message(e):
    L = e.cl
    message = e.r(4)
    kind = 10 if e.u32(message) == RIDE_DELETED_VT else -1
    L.rec("PostMessage", (L.lab(e.r(3)), kind, e.u32(message + 4), u16(e, message + 8)))
    e.set_r(3, 1)


def stub_new(e):
    keep_toc(e)
    L = e.cl
    size = e.r(3)
    fail = 1 if L.ret("newnull", 0) else 0
    L.rec("new", (size, fail))
    e.set_r(3, 0 if fail else e.alloc(size, b"\xA5" * size))


def stub_construct_array(e):
    keep_toc(e)
    L = e.cl
    p, ctor, dtor, size, count = (e.r(i) for i in range(3, 8))
    L.rec("CMessageCentre", (L.newlab(p), int(ctor == CENTRE_CTOR), int(dtor == CENTRE_DTOR), size, count))


def master_sign(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), L.lab(e.r(4))), (wide(e, e.r(4)),))
    return stub


def stub_record_action(e):
    L = e.cl
    L.rec("RecordAction", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFFFF, e.r(6) & 0xFF, L.lab(e.r(7))),
          (wide(e, e.r(7)),))


def stub_peep(e):
    L = e.cl
    L.rec("GetCoasterPeepStats", (e.r(3) & MASK,))
    e.write(e.r(4), struct.pack(">I", L.ret("peep_first", 0) & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("peep_second", 0) & MASK))


def stub_bmp(e):
    L = e.cl
    L.rec("BMP_CountTrackSections", (s32(e.r(3)),) + tuple(s32(e.u32(e.r(i))) for i in range(4, 8)))
    for i in range(4, 8):
        e.write(e.r(i), struct.pack(">I", L.ret("tracks", 0) & MASK))
    e.set_r(3, 0)


def stub_game_type(e):
    e.cl.rec("CGameType", (1,))
    e.write(e.r(3), struct.pack(">I", e.cl.ctortype & MASK))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), (e.r(5) >> 16) & 0xFFFF), (e.cstr(e.r(4))[:159],))


def stub_wcslen(e):
    keep_toc(e)
    e.set_r(3, len(wide_units(e, e.r(3))))


def stub_wcsncpy(e):
    keep_toc(e)
    dst, src, count = e.r(3), e.r(4), e.r(5)
    assert count <= 4096, "wcsncpy count"
    data, ended = bytearray(), False
    for i in range(count):
        u = 0 if ended else u16(e, src + 2 * i)
        ended = ended or u == 0
        data += struct.pack(">H", u)
    e.write(dst, bytes(data))
    e.set_r(3, dst)


def stub_swprintf(e):
    # the platform's swprintf (platform/msl.cpp): room is size - 1, the count goes on past it, -1 when it did not fit
    keep_toc(e)
    buf, size, fmt = e.r(3), e.r(4), e.r(5)
    args = [e.r(6), e.r(7)]
    out, f, i = [], wide(e, fmt), 0
    while i < len(f):
        if f[i] == "%":
            assert f[i + 1:i + 3] == "ls", "swprintf format " + repr(f)
            out.extend(wide_units(e, args.pop(0)))
            i += 3
        else:
            out.append(ord(f[i]))
            i += 1
    room = size - 1 if size > 0 else 0
    if size == 0:
        e.set_r(3, MASK)
        return
    kept = out[:room]
    e.write(buf, struct.pack(">%dH" % (len(kept) + 1), *kept, 0))
    e.set_r(3, len(out) if len(out) <= room else MASK)


for address, fake in (
    (0x1012E1F8, stub_get_thing_id), (0x1015256C, stub_post_message), (0x102384B8, stub_new),
    (0x10238758, stub_construct_array), (0x1016B31C, master_sign("CFSGlobalMasterSave::SetSignNameA")),
    (0x1016B388, master_sign("CFSGlobalMasterSave::SetSignNameB")), (0x10144A00, stub_record_action),
    (0x1004D920, stub_peep), (0x10026F20, stub_bmp), (0x1016F5D8, stub_game_type), (0x1000DDE8, stub_assert),
    (0x102389C8, stub_wcslen), (0x10238CB0, stub_wcsncpy), (0x10239160, stub_swprintf),
):
    STUBS.internal(address, fake)


def setup(runner):
    T.install_std_stubs(runner.emu)


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, in GetProspectiveExcitementLevel and
    GetStandPoint (GetExcitementLevel runs the first one): fctiwz of a value of 2^31 or more must give 0x7FFFFFFF, and
    the single-precision fused multiply-add family (fmadds, fmsubs, fnmsubs, fnmadds) must round once. Both are patched
    after the instruction has run, from a code hook on the instruction before the next one."""

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in ((PROSPECT, PROSPECT + 976), (STAND, STAND + 1028)):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            else:
                uc.reg_write(ppcemu.FPR[target], struct.unpack(">Q", struct.pack(">d", value))[0])
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        value = float(LIBM.fmaf(a, c, -b if kind in (28, 30) else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(put_fmt(self.order, size), v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(put_fmt(self.order, size), self.read(a, size))[0]

    def units(self, a, n):
        return [self.get(a + 2 * i, 2) for i in range(n)]

    def put_units(self, a, units):
        for i, u in enumerate(units):
            self.put(a + 2 * i, 2, u)

    def label(self, address, n):
        address &= self.amask
        self.known[address] = n
        self.set_label(address, n)

    def name_of(self, address):
        address &= self.amask
        return 0 if address == 0 else self.known.get(address, -1)

    def clear_slots(self):
        self.write(self.db(), bytes(self.ptr * 0x14A))

    def set_slot(self, i, record):
        self.put(self.db() + self.ptr * i, self.ptr, record)


class Ppc(Side):
    ppc = True
    order = ">"
    amask = MASK
    ptr = 4
    info_size = INFO_MAC
    nameoff = NAME_OFF
    uitype_off = 0x7A8
    ADDR = {"ui": G_UI, "rec": G_REC, "db": G_DB, "signbuf": BUF_SIGN, "uibuf": BUF_UI}

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.known = {}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def set_label(self, address, n):
        self.L.labels[address] = n

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def crashed(self):
        return 0

    def info_slot(self, info, slot):
        return info + 0x0C + 4 * slot

    def addr(self, name):
        return self.ADDR[name]

    def db(self):
        return G_DB

    def script_block(self):
        return self.e.alloc(16)

    def new_ride(self, ident):
        record = self.e.alloc(0x10)
        self.e.write(record, bytes(0x10))
        self.put(record, 2, ident)
        return record

    def setup(self, tail, info, ctl, sel, script):
        L = self.L
        L.info, L.ctl, L.selected, L.script = info, ctl, sel, script
        self.put(G_WORLD, 4, (tail - TAIL0) & MASK)
        self.put(G_MAP, 4, MAP_ADDR)

    def set_globals(self, centre, master, recording, gtype, guard, ctortype):
        self.put(G_CENTRE, 4, centre)
        self.put(G_MASTER, 4, master)
        self.put(G_REC + 0x24, 4, recording)
        self.put(GAME_TYPE, 4, gtype)
        self.write(GAME_GUARD, bytes([guard]))
        self.L.ctortype = ctortype

    def game(self):
        return {"type": self.get(GAME_TYPE, 4), "guard": 1 if self.read(GAME_GUARD, 1)[0] else 0}

    def set_db(self, count, cid, cached, records):
        self.put(G_DB + 0x528, 4, count)
        self.put(G_DB + 0x730, 2, cid)
        self.put(G_DB + 0x734, 4, cached)

    def db_state(self):
        return self.get(G_DB + 0x528, 4), self.get(G_DB + 0x730, 2), self.get(G_DB + 0x734, 4)

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK if isinstance(a, int) else a for a in args],
                          max_insns=20_000_000)[0] & MASK


class Host(Side):
    ppc = False
    order = "<"
    amask = (1 << 64) - 1
    ptr = 8
    info_size = INFO_HOST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        self.known = {}
        self.nameoff = lib.nameoff
        self.uitype_off = lib.uitypeoff

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_oin_alloc(n)

    def set_label(self, address, n):
        self.lib.tpw_cl_label(n, address)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def crashed(self):
        return self.lib.tpw_oin_crashed()

    def info_slot(self, info, slot):
        return info + self.lib.infooff + 8 * slot

    def addr(self, name):
        return self.lib.tpw_oin_addr(name.encode())

    def db(self):
        return self.addr("db")

    def script_block(self):
        return self.addr("script")

    def new_ride(self, ident):
        return self.lib.tpw_oin_ride_new(ident)

    def setup(self, tail, info, ctl, sel, script):
        self.lib.tpw_oin_setup(tail, info, ctl, sel)

    def set_globals(self, centre, master, recording, gtype, guard, ctortype):
        lib = self.lib
        lib.tpw_oin_set_centre(centre)
        lib.tpw_oin_set_master(master)
        lib.tpw_oin_set_recording(s32(recording))
        lib.tpw_oin_game(1 if guard else 0, s32(gtype), s32(ctortype))

    def game(self):
        return {"type": self.lib.tpw_oin_game_type() & MASK, "guard": self.lib.tpw_oin_game_guard()}

    def set_db(self, count, cid, cached, records):
        slots = (C.c_uint64 * 0x14A)(*(list(records) + [0] * (0x14A - len(records))))
        self.lib.tpw_oin_db_set(s32(count), cid, cached, slots)

    def db_state(self):
        out = (C.c_int64 * 3)()
        self.lib.tpw_oin_db_read(out)
        return out[0] & MASK, out[1] & 0xFFFF, out[2] & self.amask


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32, p64 = C.c_uint64, C.c_int32, C.c_uint32, C.POINTER(C.c_int64)
    sigs = dict(
        addr=(u64, [C.c_char_p]), alloc=(u64, [u32]), setup=(None, [u64, u64, u64, u64]),
        set_centre=(None, [u64]), set_master=(None, [u64]), set_recording=(None, [i32]), centre=(u64, []),
        master=(u64, []), game=(None, [i32, i32, i32]), game_type=(i32, []), game_guard=(i32, []),
        db_set=(None, [i32, u32, u64, C.POINTER(u64)]), db_read=(None, [p64]), ride_new=(u64, [u32]),
        crashed=(i32, []), upgrades_what=(u64, [u64]), get_control=(u64, [u64]), get_file_name=(u64, [u64]),
        ui_name=(None, [u64, u64, u32]), sign_a=(None, [u64, u64, u32]), sign_b=(None, [u64, u64, u32]),
        set_sign_a=(None, [u64, u64]), set_sign_b=(None, [u64, u64]), is_coaster=(i32, [u64]),
        is_track_ride=(i32, [u64]), prospective=(i32, [u64, i32, i32, i32]), excitement=(u32, [u64, u32]),
        stand_point=(None, [u64, i32, u64, u64]), final_destroy=(None, [u64]), msg_make=(None, [u64, u32]),
        msg_read=(None, [u64, p64]), db_file_name=(u64, [u32]),
    )
    for name, (res, args) in sigs.items():
        fn = getattr(lib, "tpw_oin_" + name)
        fn.restype, fn.argtypes = res, args
    lib.tpw_oin_layout.argtypes = [p64]
    out = (C.c_int64 * 16)()
    n = lib.tpw_oin_layout(out)
    assert n == 10 and out[0] == 0x448 and out[2] == 8 and out[6] == 0x20 and out[7] == 0x26 and out[8] == 0x2C \
        and out[9] == 16, "layout %s" % list(out[:n])
    lib.infooff, lib.nameoff, lib.uitypeoff, lib.infosize = out[1], out[3], out[4], out[5]
    lib.bound = True


def fill(seed, n):
    r = random.Random(seed)
    return bytes(r.getrandbits(8) for _ in range(n))


def noise(seed, slot):
    return (seed * 0x9E3779B1 + slot * 0x85EBCA6B + 0x1234567) & MASK


def sign_units(c, key):
    units = [ord(ch) for ch in c[key]]
    if len(units) < 33:
        j = random.Random(c["seed"] * 3 + (1 if key == "sa" else 2))
        units += [0] + [j.randrange(1, 0xFFFF) for _ in range(32 - len(units))]
    return units


def buf_units(seed, k):
    j = random.Random(seed * 5 + k)
    return [j.randrange(1, 0xFFFF) for _ in range(33)]


def text_units(c):
    j = random.Random(c["seed"] * 7 + 3)
    return [ord(ch) for ch in c["text"]] + [0] + [j.randrange(1, 0xFFFF) for _ in range(3)]


def rec_of(c):
    rec = {}
    for slot, key in ((36, "wtt"), (76, "exc"), (158, "upg"), (50, "sx"), (51, "sy"), (52, "ex"), (53, "ey")):
        if key in c:
            rec[slot] = T.f32_bits(c[key]) if key in ("sx", "sy", "ex", "ey") else c[key]
    for lvl in range(4):
        if "dur" in c:
            rec[101 + 16 * lvl] = c["dur"][lvl]
        if "spd" in c:
            rec[103 + 16 * lvl] = c["spd"][lvl]
    return rec


def obj_fields(c):
    fields = [OBJ_FIELDS[k] for k in OBJ_FIELDS if k in c]
    for key, off in (("sa", 0x3C4), ("sb", 0x406)):
        if key in c:
            fields += [(off + 2 * i, 2) for i in range(33)]
    if "sa" in c or "sb" in c:
        fields.append((0x448, 2))
    return fields


def name_label(s, address):
    address &= s.amask
    if address == 0:
        return 0
    for i, record in enumerate(s.recs):
        if address == record + s.nameoff:
            return i + 1
    return 50 if address == s.stale + s.nameoff else -1


def rec_label(s, address):
    address &= s.amask
    if address == 0:
        return 0
    for i, record in enumerate(s.recs):
        if address == record:
            return i + 1
    return 50 if address == s.stale else -1


def build_db(s, c):
    s.recs = [s.new_ride(ident) for ident in c["ids"]]
    s.stale = s.new_ride(0x5555)
    s.clear_slots()
    for i, record in enumerate(s.recs):
        s.set_slot(i, record)
    cref = c["cref"]
    cached = 0 if cref is None else (s.stale if cref == "stale" else s.recs[cref])
    s.set_db(c["count"], c["cid"], cached, s.recs)


def build(s, c):
    op = c["op"]
    ob = s.alloc(OBJ_BLOCK)
    s.write(ob, fill(c["seed"], OBJ_BLOCK))
    eb = ob + GUARD
    s.eb = eb
    for key, (off, size) in OBJ_FIELDS.items():
        if key in c:
            s.put(eb + off, size, c[key])
    for key, off in (("sa", 0x3C4), ("sb", 0x406)):
        if key in c:
            s.put_units(eb + off, sign_units(c, key))
    if "sa" in c or "sb" in c:
        s.put(eb + 0x448, 2, 0)
    tail = s.alloc(TAIL)
    s.write(tail, fill(c["seed"] + 1, TAIL))
    s.put(tail, 4, 0)
    s.put(tail + 0x26, 2, c.get("gates", 0))
    s.put(tail + 0x2C, 4, c.get("f738", 0))
    s.tail = tail
    info = 0
    if op in INFO_OPS:
        info = s.alloc(s.info_size)
        s.write(info, bytes(s.info_size))
        for slot in range(256):
            s.put(s.info_slot(info, slot), 4, noise(c["seed"], slot))
        for slot, v in rec_of(c).items():
            s.put(s.info_slot(info, slot), 4, v)
        s.put(info + s.uitype_off, 4, c.get("uitype", 0))
    s.ctl = 0
    if op in ("control", "destroy") and not c.get("null"):
        s.ctl = s.alloc(0x20)
        s.write(s.ctl, fill(c["seed"] + 2, 0x20))
        s.put(s.ctl + 0x18, 4, c.get("nip", 0))
        s.label(s.ctl, L_CTL)
    sel = c.get("sel", 0)
    selected = {0: 0, 1: eb}.get(sel, 0)
    if sel == 2:
        selected = s.alloc(0x40)
        s.write(selected, fill(c["seed"] + 3, 0x40))
        s.label(selected, L_OTHER)
    centre = master = 0
    if c.get("centre"):
        centre = s.alloc(0x40)
        s.write(centre, fill(c["seed"] + 4, 0x40))
        s.label(centre, L_CENTRE)
    if c.get("master"):
        master = s.alloc(0x40)
        s.write(master, fill(c["seed"] + 5, 0x40))
        s.label(master, L_MASTER)
    script = s.script_block()
    s.label(eb, L_OBJ)
    s.label(s.db(), L_DB)
    s.label(s.addr("ui"), L_UI)
    s.label(s.addr("rec"), L_REC)
    s.label(BANK_ADDR, L_BANK)
    s.label(MAP_ADDR, L_MAP)
    s.label(tail - TAIL0, L_WORLD)
    s.label(tail - TAIL0 + CTLMGR, L_CTLMGR)
    s.label(script, L_SCRIPT)
    s.setup(tail, info, s.ctl, selected, script)
    s.set_globals(centre, master, c.get("recording", 0), c.get("gtype", 0), c.get("guard", 0), c.get("ctortype", 0))
    if op in DB_OPS:
        build_db(s, c)
    if op in SET_OPS:
        s.text = s.alloc(2 * len(c["text"]) + 16)
        s.put_units(s.text, text_units(c))
        s.label(s.text, L_TEXT)
    if op in NAME_OPS:
        s.bufsign, s.bufui = s.addr("signbuf"), s.addr("uibuf")
        s.put_units(s.bufsign, buf_units(c["seed"], 1))
        s.put_units(s.bufui, buf_units(c["seed"], 2))


def hostfn(lib, op):
    return getattr(lib, "tpw_oin_" + HOST_NAMES[op])


def execute(s, c):
    op, eb = c["op"], s.eb
    entry = ENTRY[op]
    if op == "upgrades":
        if s.ppc:
            buf = s.alloc(16)
            s.write(buf, b"\xAA" * 16)
            s.call(entry, [buf, eb])
            return s.get(buf, 2)
        return hostfn(s.lib, op)(eb) & 0xFFFF
    if op == "control":
        return s.name_of(s.call(entry, [eb]) if s.ppc else hostfn(s.lib, op)(eb))
    if op == "fname":
        return name_label(s, s.call(entry, [eb]) if s.ppc else hostfn(s.lib, op)(eb))
    if op == "dbname":
        if s.ppc:
            return name_label(s, s.call(entry, [G_DB, (c["type"] << 16) | c["junk"]]))
        return name_label(s, hostfn(s.lib, op)(c["type"]))
    if op in NAME_OPS:
        n = c["size"]
        s.out = s.alloc(2 * (n + 8))
        s.write(s.out, b"\xAA" * (2 * (n + 8)))
        if s.ppc:
            s.call(entry, [eb, s.out, n])
        else:
            hostfn(s.lib, op)(eb, s.out, n)
        return None
    if op in SET_OPS:
        if s.ppc:
            s.call(entry, [eb, s.text])
            s.L.rec("gMasterSave", (s.L.lab(s.get(G_MASTER, 4)),))
        else:
            hostfn(s.lib, op)(eb, s.text)
        return None
    if op in ("coaster", "track"):
        return s32(s.call(entry, [eb])) if s.ppc else hostfn(s.lib, op)(eb)
    if op == "prospect":
        args = (c["a"], c["b"], c["c"])
        if s.ppc:
            with HardwareRounding(s.e):
                return s32(s.call(entry, [eb, *args]))
        return hostfn(s.lib, op)(eb, *args)
    if op == "excite":
        if s.ppc:
            with HardwareRounding(s.e):
                return s.call(entry, [eb, c["flagword"]]) & 0xFF
        return hostfn(s.lib, op)(eb, c["flagword"] & MASK) & 0xFF
    if op == "stand":
        s.xb, s.yb = s.alloc(16), s.alloc(16)
        s.write(s.xb, fill(c["seed"] + 6, 16))
        s.write(s.yb, fill(c["seed"] + 7, 16))
        if s.ppc:
            s.e.set_f(30, 0.0)
            s.e.set_f(31, 0.0)
            with HardwareRounding(s.e):
                s.call(entry, [eb, c["which"], s.xb, s.yb])
        else:
            hostfn(s.lib, op)(eb, s32(c["which"]), s.xb, s.yb)
        return None
    if op == "destroy":
        if s.ppc:
            s.call(entry, [eb])
            s.L.rec("gMessageCentre", (s.L.lab(s.get(G_CENTRE, 4)),))
        else:
            hostfn(s.lib, op)(eb)
        return None
    if op == "msg":
        s.msg = s.alloc(0x40)
        s.write(s.msg, fill(c["seed"], 0x40))
        if s.ppc:
            s.call(entry, [s.msg, (c["id"] << 16) | c["junk"]])
            return (10 if s.get(s.msg, 4) == RIDE_DELETED_VT else -1, s.get(s.msg + 4, 4), s.get(s.msg + 8, 2))
        hostfn(s.lib, op)(s.msg, c["id"])
        out = (C.c_int64 * 3)()
        s.lib.tpw_oin_msg_read(s.msg, out)
        return (out[0], out[1] & MASK, out[2] & 0xFFFF)
    raise AssertionError(op)


def snapshot(s, base, size, fields, skew=GUARD):
    data = bytearray(s.read(base, size))
    values = []
    for off, n in fields:
        i = skew + off
        values.append(s.get(base + i, n))
        data[i:i + n] = bytes(n)
    return values, bytes(data)


def observe(s, c, answer):
    op = c["op"]
    out = {"answer": answer, "log": s.log(), "crashed": s.crashed()}
    if op == "msg":
        out["rest"] = s.read(s.msg + 16, 0x30)
        return out
    if op in DB_OPS:
        count, cid, cached = s.db_state()
        out["db"] = (count, cid, rec_label(s, cached))
    if op == "dbname":
        return out
    out["obj"] = snapshot(s, s.eb - GUARD, OBJ_BLOCK, obj_fields(c))
    out["tail"] = snapshot(s, s.tail, TAIL, [(0, 4), (0x26, 2), (0x2C, 4)], 0)
    if s.ctl:
        out["ctl"] = snapshot(s, s.ctl, 0x20, [(0x18, 4)], 0)
    if op in NAME_OPS:
        out["out"] = s.units(s.out, c["size"] + 8)
        out["bufs"] = (s.units(s.bufsign, 33), s.units(s.bufui, 33))
    if op == "stand":
        out["x"], out["y"] = s.read(s.xb, 16), s.read(s.yb, 16)
    if op in SET_OPS:
        out["game"] = s.game()
    return out


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------- generators

WORDS = [0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, MASK,
         0x12345678, 0xDEADBEEF]
TYPES = [0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0x100, 0x1234, 0x7FFF, 0x8000, 0xFFFF]
ID_POOL = [0, 1, 2, 3, 0x7F, 0x80, 0x100, 0x1234, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF]
KINDS = [0, 1, 2, 3, 3, 3, 1, 2, 4, 5, 0x101, 0x103, 0x10001, 0x10002, 0x10003, 0x80000001, 0x80000003, 0x3000000,
         MASK, 0x80000000, 0x100]
SIDE_N = [-5, -1, 0, 1, 2, 4, 9, 16, 25, 50, 81, 99, 100, 101, 150, 5000, 0x7FFFFFFF, -0x80000000]
CS = [0, 1, -1, 5, 10, 100, -100, 1000, 77, 255, 0x7FFFFFFF, -0x80000000, 3, 20, 33]
BASES = [0, 1, 2, 5, 20, 50, 99, 100, 101, 150, 200, 1000, -1, -50, 0x7FFFFF, 0x1000000, 0x7FFFFFFF, -0x80000000,
         0x2AAAAAAA, 0x40000000]
SPEEDS = [1, 2, 3, 4, 5, 8, 10, 16, 20, 25, 50, 100, 128, 1000, 0x1000000, 0x1000001, 0x7FFFFFFF, -4, -0x80000000, 0]
RATIOS = [0.5, 0.6, 0.7, 0.75, 0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.25, 1.3, 1.5, 2.0, 3.0, 10.0]
HANDLES = [0, 0, 1, 2, 7, 100, -1, 0x7FFFFFFF, -0x80000000, 0x10000]
TRACKS = [0, 1, 2, 3, 4, 5, 8, 10, 13, 14, 20, 40, 100, -1, -5, 0x7FFFFFFF, -0x80000000, 0x55555555]
PEEP = [0.0, 1.0, -1.0, 20.0, 99.0, 100.0, -100.0, -101.0, -99.0, 0.5, -0.5, 1234.5678, -2.5e9, 2.5e9, 1e30, -1e30,
        3.4e38, -3.4e38, 4294967296.0, 100.7]
CHANCES = [0, 1, 5, 50, 100, 0xFF, 0x100, 0x1234, 0x80000001, MASK]
FLAG_ON = [1, 2, 0x80, 0xFF, 0x101, 0x1FF, MASK, 0x12345678, 0x80000001]
FLAG_OFF = [0, 0x100, 0x200, 0x12345600, 0xFFFFFF00, 0x80000000, 0x10000, 0x7F00]
UNIT = [0.0, -0.0, 1.0, 0.5, 0.25, 0.75, 0.125, 1 / 255, 2 / 255, 127 / 255, 128 / 255, 254 / 255, 0.001, 0.999,
        0.99999994, 1.0000001, 1e-30, -1e-30, 0.3, 0.7, 0.1, 0.9, 0.6, -0.5, 2.0, 1.5, -1.0, 100.0, 0.0078125,
        255 / 256]
ODD = [float("nan"), float("inf"), float("-inf"), 1e10, -1e10, 3.4028234663852886e38, -3.4028234663852886e38,
       2147483648.0 / 255, 1.6777216e7]
CELLS = [0, 1, 2, 3, 0x7E, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x101, 0x3FFF, 0x4000, 0x7FFF, 0x8000, 0xFFFF]
ROTS = [0, 90, 180, 270, 0, 90, 180, 270, 1, 45, 89, 91, 179, 181, 269, 271, 360, -90, 0x15A, 0x1B4, 0x20E,
        -0x80000000, -1, 0x7FFFFFFF]
WHICH = [0, 1, 2, -1, 0x100, -0x80000000, 0x10000, 0x7FFFFFFF, 5]
FLAG_SETS = [0, 0x10, 0x80, 0x90, 1, 0x11, 0x81, 0x91]
GAME_TYPES = [0, 1, 2, 3, MASK, 0x80000000, 0x7FFFFFFF, 0x101]
OUT_SIZES = [0, 1, 2, 3, 5, 8, 16, 31, 32, 33, 34, 40, 64, 100]
ASCII = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.!"
WIDE = "éü中Ж☃耀￮퟿"


def common(r, op):
    return {"op": op, "seed": r.getrandbits(24), "queues": {}}


def rword(r):
    return r.choice(WORDS) if r.random() < 0.6 else r.getrandbits(32)


def rsword(r):
    return s32(rword(r))


def rtype(r):
    return r.choice(TYPES) if r.random() < 0.5 else r.getrandbits(16)


def rcell(r):
    return r.choice(CELLS) if r.random() < 0.7 else r.getrandbits(16)


def clamp32(v):
    return max(-0x80000000, min(0x7FFFFFFF, int(v)))


def runit(r):
    x = r.random()
    if x < 0.6:
        return T.f32(r.choice(UNIT))
    if x < 0.7:
        return T.f32(r.choice(ODD))
    if x < 0.85:
        return T.f32(r.randrange(0, 256) / 255.0 + r.choice((0.0, 1e-7, -1e-7, 1e-4)))
    return T.f32(r.random())


def rsign(r, n=None, dashes=None, wide=False):
    if n is None:
        n = r.choice((0, 0, 1, 2, 3, 4, 6, 9, 12, 20, 30, 31, 32, 33))
    pool = ASCII + "--" + (WIDE if wide else "")
    chars = [r.choice(pool) for _ in range(n)]
    d = r.choice((0, 0, 1, 1, 2, 3)) if dashes is None else dashes
    for i in range(min(d, n)):
        chars[n - 1 - i] = "-"
    return "".join(chars)


def ratio_arg(r, den):
    # the numerator of a ratio against den: never 0 over 0 (a NaN), but a nonzero numerator over 0 is allowed
    if den == 0:
        return r.choice((1, -1, 7, 0x7FFFFFFF))
    return clamp32(den * r.choice(RATIOS)) if r.random() < 0.8 else rsword(r)


def grid(seed, mk, **axes):
    def edges():
        r = random.Random(seed)
        keys = list(axes)
        for combo in itertools.product(*[axes[k] for k in keys]):
            yield mk(r, **dict(zip(keys, combo)))
    return edges


def chain(*gens):
    def edges():
        for g in gens:
            yield from g()
    return edges


def mk_upgrades(r, upg=None, ty=None):
    c = common(r, "upgrades")
    c["type"] = rtype(r) if ty is None else ty
    c["upg"] = rword(r) if upg is None else upg
    return c


def mk_control(r, ty=None, null=None):
    c = common(r, "control")
    c["type"] = rtype(r) if ty is None else ty
    c["nip"] = rword(r)
    c["null"] = int(r.random() < 0.15) if null is None else null
    return c


def walk_ok(ids, count, sid, cid):
    # the original compares the cached id first, then walks slot 0.. while i < count (signed): it must not meet a NULL slot
    if sid == cid:
        return True
    for i in range(max(s32(count), 0)):
        if i >= len(ids):
            return False
        if ids[i] == sid:
            return True
    return True


DB_MODES = ("empty", "first", "mid", "last", "miss", "dup", "same", "null", "stale", "diff", "beyond", "negative")


def mk_db(r, op, mode=None, rep=0):
    c = common(r, op)
    mode = mode or r.choice(DB_MODES)
    n = 0 if mode == "empty" else r.choice((3, 4, 5, 8, 12) if mode in ("mid", "dup", "beyond", "same", "diff")
                                           else (1, 2, 3, 5, 8, 12))
    ids = []
    while len(ids) < n:
        v = r.choice(ID_POOL) if r.random() < 0.6 else r.getrandbits(16)
        if v not in ids:
            ids.append(v)

    def absent():
        while True:
            v = r.choice(ID_POOL) if r.random() < 0.5 else r.getrandbits(16)
            if v not in ids:
                return v

    count, cid, cref = n, absent(), None
    if mode == "empty":
        sid, count = absent(), r.choice((0, 0, MASK, 0x80000000))
    elif mode == "first":
        sid, count = ids[0], r.choice((n, n, 0x7FFFFFFF))
    elif mode == "mid":
        sid = ids[n // 2]
    elif mode == "last":
        sid = ids[-1]
    elif mode == "miss":
        sid = absent()
    elif mode == "dup":
        j = r.randrange(1, n)
        i = r.randrange(0, j)
        ids[j] = ids[i]
        sid = ids[i]
    elif mode == "same":
        sid = r.choice(ids + [absent()])
        cid, cref = sid, r.randrange(n)
    elif mode == "null":
        sid = r.choice(ids)
        cid = sid
    elif mode == "stale":
        sid = r.choice(ids + [absent()])
        cid, cref = sid, "stale"
    elif mode == "diff":
        sid, cref = r.choice(ids), r.choice((None, "stale", r.randrange(n)))
    elif mode == "beyond":
        k = r.randrange(1, n)
        sid, count = ids[k], k
    else:
        sid, count = ids[0], r.choice((0, MASK, 0x80000000, 0xFFFFFF00))
    c.update(ids=ids, count=count, cid=cid, cref=cref, type=sid, junk=r.getrandbits(16))
    if not walk_ok(ids, count, sid, cid):
        c["count"] = n
    return c


def mk_names(r, op, sa=None, sb=None, size=None):
    c = common(r, op)
    wide = r.random() < 0.3
    c["sa"] = rsign(r, wide=wide) if sa is None else sa
    c["sb"] = rsign(r, wide=wide, dashes=0) if sb is None else sb
    if len(c["sa"]) >= 33:
        c["sb"] = ""        # mSignA unterminated runs into mSignB: wcslen over 33 would overrun the 0x21-unit scratch buffer
    c["size"] = r.choice(OUT_SIZES) if size is None else size
    return c


def mk_setsign(r, op, n=None, gtype=None, guard=None, ctortype=None, gates=None, master=None, newnull=None, sa=None,
               sb=None):
    c = common(r, op)
    c["sa"] = rsign(r, wide=r.random() < 0.3) if sa is None else sa
    c["sb"] = rsign(r, wide=r.random() < 0.3) if sb is None else sb
    c["text"] = rsign(r, n=r.choice((0, 1, 2, 5, 10, 20, 31, 32, 33, 34, 40)) if n is None else n)
    c["tid"], c["cx"], c["cy"] = r.getrandbits(16), r.getrandbits(8), r.getrandbits(8)
    equal = r.random() < 0.5 if gates is None else bool(gates)
    c["gates"] = c["tid"] if equal else (c["tid"] ^ r.choice((1, 0x100, 0x8000, 0xFFFF, 0x1234))) & 0xFFFF
    c["gtype"] = r.choice(GAME_TYPES) if gtype is None else gtype
    c["guard"] = r.choice((0, 0, 1, 3, 0x7F, 0xFF)) if guard is None else guard
    c["ctortype"] = r.choice((0, 1, 2, 3, MASK, 0x80000000)) if ctortype is None else ctortype
    c["master"] = r.randrange(2) if master is None else master
    c["queues"]["newnull"] = [r.randrange(2) if newnull is None else newnull]
    return c


def mk_kind(r, op, wtt=None, ty=None):
    c = common(r, op)
    c["type"] = rtype(r) if ty is None else ty
    c["wtt"] = r.choice(KINDS) if wtt is None else wtt
    return c


def mk_ride(r, c, kind, **kw):
    """The ride record and the object fields of the excitement functions. kind: 'side' (a sideshow), 'coaster' or
    'ride' (anything else). kw overrides a case key, or handle, peep (peep_second) and tracks (the four counts)."""
    c["type"] = rtype(r)
    c["f20"] = r.choice((0, 1, 7, 100, -1, 0x7FFFFFFF, -0x80000000)) if r.random() < 0.7 else r.randrange(-1000, 1000)
    c["f4c"] = r.randrange(4)
    c["f24"] = r.choice((0, 0, 0, 1, 7, 0x100, -1, 0x7FFFFFFF, -0x80000000))
    c["uitype"] = 2 if kind == "side" else r.choice((0, 1, 3, 4, 0x10002, 0x102, 0x80000002, MASK))
    c["wtt"] = 3 if kind == "coaster" else r.choice([k for k in KINDS if k != 3])
    if kind == "side" and r.random() < 0.3:
        c["wtt"] = 3
    c["exc"] = r.choice(BASES) if r.random() < 0.8 else r.getrandbits(32)
    c["dur"] = [r.choice(SPEEDS) for _ in range(4)]
    c["spd"] = [r.choice(SPEEDS) for _ in range(4)]
    q = c["queues"]
    q["RSSE_GetCoasterHandle"] = [r.choice(HANDLES) & MASK]
    q["peep_first"] = [T.f32_bits(r.choice(PEEP))]
    q["peep_second"] = [T.f32_bits(r.choice(PEEP))]
    q["tracks"] = [r.choice(TRACKS) & MASK for _ in range(4)]
    for k, v in kw.items():
        if k == "handle":
            q["RSSE_GetCoasterHandle"] = [v & MASK]
        elif k == "peep":
            q["peep_second"] = [T.f32_bits(v)]
        elif k == "tracks":
            q["tracks"] = [x & MASK for x in v]
        else:
            c[k] = v


def mk_prospect(r, kind=None, n=None, a=None, b=None, cc=None, ra=None, rb=None, **kw):
    kind = kind or r.choice(("side", "coaster", "ride", "ride", "ride"))
    c = common(r, "prospect")
    mk_ride(r, c, kind, **kw)
    lvl = c["f4c"]
    if ra is not None:
        c["spd"][lvl] = 40
        a = int(40 * ra)
    if rb is not None:
        c["dur"][lvl] = 40
        b = int(40 * rb)
    if kind == "side":
        b = r.choice(CS) if b is None else b
        n = r.choice(SIDE_N) if n is None else n
        a = s32(b + n) if a is None else a
    else:
        a = ratio_arg(r, c["spd"][lvl]) if a is None else a
        b = ratio_arg(r, c["dur"][lvl]) if b is None else b
    c["a"], c["b"], c["c"] = a, b, (r.choice(CS) if cc is None else cc)
    return c


def mk_excite(r, kind=None, flag=None, n=None, chance=None, **kw):
    kind = kind or r.choice(("side", "ride", "ride", "coaster"))
    c = common(r, "excite")
    mk_ride(r, c, kind, **kw)
    if "exc" not in kw and r.random() < 0.12:
        c["exc"] = 0
    if flag is None:
        flag = r.random() < 0.3
    c["flagword"] = r.choice(FLAG_ON if flag else FLAG_OFF)
    lvl = c["f4c"]
    c["f54"], c["f58"], c["f59"] = rword(r), r.randrange(256), r.randrange(256)
    q = c["queues"]
    price = r.choice(CS) & MASK
    n = r.choice(SIDE_N) if n is None else n
    q["CObject::GetPricePerUse"] = [price]
    q["CObject::GetPrizeValue"] = [(price + n) & MASK]
    q["CObject::GetChanceOfWinning"] = [(r.choice(CHANCES) if chance is None else chance) & MASK]
    if kind != "side":
        c["f54"] = ratio_arg(r, c["spd"][lvl]) & MASK
        if r.random() < 0.7:
            c["f58"] = r.choice((0, 1, 5, 10, 33, 100, 200, 255))
        if c["f58"] == 0 and c["dur"][lvl] == 0:
            c["dur"][lvl] = 1
        elif c["f58"] and r.random() < 0.7:
            c["dur"][lvl] = max(1, int(c["f58"] / r.choice(RATIOS)))
    return c


def mk_stand(r, which=None, rot=None, f32=None, f34=None, sx=None, sy=None, ex=None, ey=None, ty=None):
    c = common(r, "stand")
    c["type"] = rtype(r) if ty is None else ty
    c["which"] = (r.choice(WHICH) if r.random() < 0.3 else r.randrange(2)) if which is None else which
    c["rot"] = r.choice(ROTS) if rot is None else rot
    c["f32"] = rcell(r) if f32 is None else f32
    c["f34"] = rcell(r) if f34 is None else f34
    for key, v in (("sx", sx), ("sy", sy), ("ex", ex), ("ey", ey)):
        c[key] = runit(r) if v is None else T.f32(v)
    return c


def mk_destroy(r, flags=None, getvar=None, dirty=None, sel=None, centre=None, recording=None, f738=None, f50=None,
               newnull=None):
    c = common(r, "destroy")
    c["tid"], c["cx"], c["cy"], c["type"] = r.getrandbits(16), r.getrandbits(8), r.getrandbits(8), rtype(r)
    if flags is None:
        flags = r.choice(FLAG_SETS) | (r.getrandbits(16) & 0xFF6E if r.random() < 0.5 else 0)
    c["flags"] = flags
    c["f20"] = rsword(r)
    c["f50"] = (r.choice((0, 0, 1, 0x100, MASK, 0x80000000, r.getrandbits(32))) if f50 is None else f50)
    c["nip"] = rword(r)
    c["sel"] = r.choice((0, 1, 1, 2)) if sel is None else sel
    c["centre"] = r.randrange(2) if centre is None else centre
    c["recording"] = r.choice((0, 0, 1, 2, 0x100, 0x10000, MASK)) if recording is None else recording
    c["f738"] = r.choice((0, 1, 2, 2, 3, 0x102, 0x20000002, MASK)) if f738 is None else f738
    q = c["queues"]
    q["RSSE_Getvar"] = [r.choice((0, 1, 2, 3, MASK, 0x80000000, 0x7FFFFFFF)) if getvar is None else getvar & MASK]
    q["CObject::IsDirty"] = [r.choice((0, 0, 1, 2, 0x100, MASK)) if dirty is None else dirty & MASK]
    q["CObject::GetCurrentResidualValue"] = [rword(r)]
    q["newnull"] = [r.randrange(2) if newnull is None else newnull]
    return c


def mk_msg(r, ident=None, junk=None):
    c = common(r, "msg")
    c["id"] = rtype(r) if ident is None else ident
    c["junk"] = r.getrandbits(16) if junk is None else junk
    return c


def op_mk(mk, op):
    return lambda r, **kw: mk(r, op, **kw)


SA_EDGES = ["", "-", "--", "a", "a-", "ab-", "a--", "-a", "a-b-", "x" * 31 + "-", "x" * 32 + "-", "y" * 31 + "--",
            "z" * 33, "x" * 30 + "--"]
SB_EDGES = ["", "bb", "-", "q" * 33, "r" * 17]
SIGN_TEXTS = [0, 1, 5, 31, 32, 33, 34, 40]

EDGE_UPGRADES = grid(11, mk_upgrades, upg=[0, 1, 0xFFFF, 0x10000, 0x12345, 0x80000000, MASK, 0xDEADBEEF],
                     ty=[0, 1, 0x8000, 0xFFFF, 0x1234])
EDGE_CONTROL = grid(12, mk_control, ty=[0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x1234, 0x100], null=[0, 1])
EDGE_FNAME = grid(13, op_mk(mk_db, "fname"), mode=list(DB_MODES), rep=[0, 1, 2])
EDGE_DBNAME = grid(14, op_mk(mk_db, "dbname"), mode=list(DB_MODES), rep=[0, 1, 2])
EDGE_UINAME = grid(15, op_mk(mk_names, "uiname"), sa=SA_EDGES, sb=SB_EDGES[:3], size=[0, 1, 7, 40])
EDGE_SIGNA = grid(16, op_mk(mk_names, "signa"), sa=SA_EDGES, size=[0, 1, 4, 33, 34, 64])
EDGE_SIGNB = grid(17, op_mk(mk_names, "signb"), sb=SB_EDGES, size=[0, 1, 3, 33, 34])
EDGE_COASTER = grid(18, op_mk(mk_kind, "coaster"),
                    wtt=[0, 1, 2, 3, 4, 5, 0x101, 0x102, 0x103, 0x10001, 0x10002, 0x10003, 0x80000000, MASK],
                    ty=[0, 0xFFFF])
EDGE_TRACK = grid(19, op_mk(mk_kind, "track"),
                  wtt=[0, 1, 2, 3, 4, 5, 0x101, 0x102, 0x103, 0x10001, 0x10002, 0x10003, 0x80000000, MASK],
                  ty=[0, 0xFFFF])
EDGE_SETSIGN = {}
for _op, _seed in (("setsigna", 20), ("setsignb", 30)):
    EDGE_SETSIGN[_op] = chain(
        grid(_seed, op_mk(mk_setsign, _op), n=SIGN_TEXTS, gtype=[0, 1, 2], guard=[0, 1]),
        grid(_seed + 1, op_mk(mk_setsign, _op), gates=[0, 1], master=[0, 1], newnull=[0, 1], gtype=[0, 2], guard=[1]),
        grid(_seed + 2, op_mk(mk_setsign, _op), ctortype=[0, 1, 2, 3, MASK], guard=[0], gates=[1], master=[0, 1]),
        grid(_seed + 3, op_mk(mk_setsign, _op), sa=["", "p" * 33, "q" * 20], sb=["", "r" * 33, "s" * 7],
             n=[3, 33]),
    )
EDGE_PROSPECT = chain(
    grid(40, mk_prospect, kind=["side"], n=SIDE_N, cc=[0, 7, -3, 1000]),
    grid(41, mk_prospect, kind=["coaster"], handle=[0, 1, -1, -0x80000000],
         peep=[0.0, 1.0, -1.0, -100.0, -101.0, 100.7, 1e30, -1e30, 4294967296.0]),
    grid(42, mk_prospect, kind=["ride"], f24=[0], exc=BASES, f4c=[0, 3]),
    grid(43, mk_prospect, kind=["ride"], f24=[1], exc=[0, 50, 100, 200, -1],
         tracks=[[0, 0, 0, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1], [5, 5, 5, 5], [10, 10, 10, 10],
                 [20, 10, 5, 0], [-1, -1, -1, -1], [0x7FFFFFFF] * 4, [0x55555555] * 4]),
    grid(44, mk_prospect, kind=["ride"], f24=[0], ra=[0.5, 0.75, 0.8, 1.0, 1.25, 2.0],
         rb=[0.5, 0.75, 0.8, 1.0, 1.25, 2.0]),
    # the bounds of the track sections' weight (3c + 2a + b: 39, 40, 41, -1) and of the clamped base (100, 101)
    grid(45, mk_prospect, kind=["ride"], f24=[1], exc=[0, 50, 100, 101, 102, 150, -1, -50, -101],
         tracks=[[0, 39, 0, 0], [0, 40, 0, 0], [0, 41, 0, 0], [0, -1, 0, 0], [1, 0, 0, 0], [0, 0, 1, 0],
                 [0, 0, 0, 5], [13, 0, 0, 0], [0, 0, 13, 0]]),
)
EDGE_EXCITE = chain(
    grid(50, mk_excite, kind=["ride"], flag=[True, False], exc=[0, 1, 0x1FF, 0x100, 0x80000000, 77]),
    grid(51, mk_excite, kind=["side"], flag=[False], n=SIDE_N[:8], chance=[0, 5, 0xFF, 0x100, 0x1234]),
    grid(52, mk_excite, kind=["ride"], flag=[False], f24=[0, 1], exc=BASES[:10]),
    grid(53, mk_excite, kind=["coaster"], flag=[False], handle=[0, 1], peep=[0.0, -101.0, 100.7]),
)
EDGE_STAND = chain(
    grid(60, mk_stand, which=[0, 1, 2, -1, 0x100, -0x80000000, 0x7FFFFFFF],
         rot=[0, 90, 180, 270, 45, -90, 360, 0x15A, 256, 0x1005A]),
    grid(61, mk_stand, which=[0], rot=[0], sx=[float("nan"), 0.0, -0.0, 0.5, 1.0, 0.999, -1.0, float("inf")],
         sy=[float("nan"), 0.0, 0.5, 1.0, 0.999, -1.0, float("inf"), 2 / 255]),
    grid(62, mk_stand, which=[1], rot=[0], ex=[float("nan"), 0.0, -0.0, 0.5, 1.0, 0.999, -1.0, float("inf")],
         ey=[float("nan"), 0.0, 0.5, 1.0, 0.999, -1.0, float("inf"), 2 / 255]),
    grid(63, mk_stand, which=[0, 1], f32=CELLS, f34=[0, 1, 0xFFFF]),
    grid(64, mk_stand, which=[0, 1], rot=[0, 90, 180, 270], sx=[1 / 255, 2 / 255, 127 / 255, 254 / 255, 0.999],
         sy=[1 / 255, 3 / 255, 128 / 255, 253 / 255], ex=[1 / 255, 254 / 255], ey=[2 / 255, 253 / 255]),
)
EDGE_DESTROY = chain(
    grid(70, mk_destroy, flags=[0, 0x10, 0x80, 0x90, 1, 0x11, 0x81, 0x91], sel=[0, 1, 2], centre=[0, 1]),
    grid(71, mk_destroy, flags=[0x80, 0x91], getvar=[0, 1, 2, -1, 0x80000000, 0x7FFFFFFF]),
    grid(72, mk_destroy, f738=[0, 1, 2, 3, 0x102, 0x20000002, MASK], recording=[0, 1, 0x100]),
    grid(73, mk_destroy, f50=[0, 1, 0x100, MASK, 0x80000000], newnull=[0, 1], centre=[0, 1]),
    grid(74, mk_destroy, dirty=[0, 1, 2, 0x100, MASK], flags=[0x01, 0x11, 0x91]),
)
EDGE_MSG = grid(80, mk_msg, ident=[0, 1, 0x7F, 0x80, 0x100, 0x1234, 0x8000, 0xFFFF, 0xABCD], junk=[0, 0xFFFF, 0x1234])


def fn(name, mk, edges):
    return FnTest(name, ppc, host, edge=edges, rnd=lambda r: mk(r))


TESTS = [
    fn("CObject::UpgradesWhat", mk_upgrades, EDGE_UPGRADES),
    fn("CObject::GetControl", mk_control, EDGE_CONTROL),
    fn("CObject::GetFileName", op_mk(mk_db, "fname"), EDGE_FNAME),
    fn("CObject::GetUIName", op_mk(mk_names, "uiname"), EDGE_UINAME),
    fn("CObject::GetSignNameA", op_mk(mk_names, "signa"), EDGE_SIGNA),
    fn("CObject::GetSignNameB", op_mk(mk_names, "signb"), EDGE_SIGNB),
    fn("CObject::SetSignNameA", op_mk(mk_setsign, "setsigna"), EDGE_SETSIGN["setsigna"]),
    fn("CObject::SetSignNameB", op_mk(mk_setsign, "setsignb"), EDGE_SETSIGN["setsignb"]),
    fn("CObject::IsCoaster", op_mk(mk_kind, "coaster"), EDGE_COASTER),
    fn("CObject::IsTrackRide", op_mk(mk_kind, "track"), EDGE_TRACK),
    fn("CObject::GetProspectiveExcitementLevel", mk_prospect, EDGE_PROSPECT),
    fn("CObject::GetExcitementLevel", mk_excite, EDGE_EXCITE),
    fn("CObject::GetStandPoint", mk_stand, EDGE_STAND),
    fn("CObject::FinalDestroy", mk_destroy, EDGE_DESTROY),
    fn("CMsgRideDeleted::CMsgRideDeleted", mk_msg, EDGE_MSG),
    fn("CRideDatabase::GetFileName", op_mk(mk_db, "dbname"), EDGE_DBNAME),
]
