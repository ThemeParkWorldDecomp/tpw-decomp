"""Unit w174, one challenge, against libtpw_unit_Challenge.dylib (tests/unit_Challenge.cpp), main app ("SimTheme Park"), src/Challenge.cpp:
  CChallenge::CChallenge 0x100f5a78, CChallenge::Serialise 0x100f5b80, CChallenge::DidThisWinIt 0x100f60d8, GetOfferId 0x100f8d7c,
  GetActiveId 0x100f8ee4, GetWinId 0x100f904c, GetLoseId 0x100f91b0, LOADVAR<ChallengeType> 0x100f93e8, LOADVAR<StaffType> 0x100f9518,
  SAVEVAR<ChallengeType> 0x100f9640, SAVEVAR<StaffType> 0x100f9788

A case is one call on a 0x30-byte challenge record. The record sits between two guards of 0x40 bytes in emulator memory (big-endian) for the
original and in the host library's memory (little-endian) for the translation; it is prefilled from the case (random bytes in the padding too)
and read back as fields, padding bytes and guards, so one decode serves both sides. The serialise instances are REAL on both sides (the original's
LOADVAR / SAVEVAR of int, bool, the object id and the thing id run in the emulator), so the file is the fake: LbFile_Read hands out the case's
bytes in order, LbFile_Write records them as hex, and the queues replace the result and the transferred count. Every other callee is a fake that
records its arguments and answers from the case's queue: CWorld::GetParkAnalyser (answers the analyser block; gGameWorld is a word the
case writes), the eight CParkAnalyser readers (an out id is written from the queue "<name>:id"), CTRACE, GAME_ASSERTMSG and the serialise log.

The ids the analyser takes by value are the HIGH halfword of a register on the Mac (the low half is stack garbage, which the stubs ignore); the
constructor test puts random garbage in the low halves. The staff type outside 0 to 4 leaves r28 undefined on the Mac; the test sets r28 to 0
before every DidThisWinIt call, as the translation passes 0.

What is compared: the answer, the value in/out of DidThisWinIt, the record (fields, padding bytes, guards), the log (file calls with the bytes
written, the serialise log with every name, the asserts, the analyser calls). No fused operations. Not tested: a bool argument other than 0 or 1
(the host's bool cannot hold it), a NULL record.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Challenge"
MASK = 0xFFFFFFFF

CTOR, SER, DID = 0x100F5A78, 0x100F5B80, 0x100F60D8
GETTERS = [("GetOfferId", 0x100F8D7C), ("GetActiveId", 0x100F8EE4), ("GetWinId", 0x100F904C), ("GetLoseId", 0x100F91B0)]
INSTANCES = [("LOADVAR<ChallengeType>", 0x100F93E8, True), ("LOADVAR<StaffType>", 0x100F9518, True),
             ("SAVEVAR<ChallengeType>", 0x100F9640, False), ("SAVEVAR<StaffType>", 0x100F9788, False)]
OWN = {CTOR, SER, DID} | {a for _, a in GETTERS} | {a for _, a, _ in INSTANCES}
# the real basic instances the serialiser calls: LOADVAR / SAVEVAR of int, bool, object id struct, thing id struct
REAL = {0x1000EF18, 0x1000F4B0, 0x1000F144, 0x1000F70C, 0x100F1300, 0x100F1188, 0x1000EB4C, 0x1000E9D4}

GSAVELOAD = 0x1037FEFC
GGAMEWORLD = 0x1037EBFC
FILE_HANDLE = 0x6E100000      # the PowerPC side's file (never dereferenced)
L_SAVELOAD, L_FILE, L_WORLD, L_AN = 1, 2, 3, 4
GUARD = 0x40
RECORD = 0x30
GUARD_BYTES = bytes((0xA5 + 7 * i) & 0xFF for i in range(GUARD))
# (offset, size) of the host's CChallenge members, in the order tpw_chl_layout prints them after sizeof
HOST_LAYOUT = [0x30, 0x00, 0x04, 0x08, 0x0C, 0x0E, 0x10, 0x14, 0x18, 0x1C, 0x1D, 0x20, 0x24, 0x28, 0x2C, 0x2E]
REC_FMT = "IIIHHIIIBBBBIIIBBH"
REC_KEYS = ["type", "time", "val", "obj", "obj2", "staff", "prize", "follow", "chk", "ind", "pad1e", "pad1f", "off", "lost", "decl", "won",
            "pad2d", "thing"]
FIELD_SIZES = [4, 4, 4, 2, 2, 4, 4, 4, 1, 4, 4, 4, 1, 1, 2]   # the serialiser's fifteen fields, in its order
P32 = [0, 1, 2, 3, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x7FFFFFFE, 0xFF, 0x100, 0x12345678, 0xFFFFFFFE]
ANS = [0, 1, 2, 3, -1, -2, 5, 100, 0x7FFFFFFF, -0x80000000, 50, 99]
NAMES = ["", "x", "mType", "A name with spaces 123", "L" * 159, "M" * 200]
NO_LOGS = [0, 1, -1, 2, 0x80000000]
RESULTS = [0, 1, 5, 0x7FFFFFFF, -1, -2, -0x80000000]
VP, I32, U32, U64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_uint64

# ======================================================================= the record


def r32(r):
    return r.choice(P32) if r.random() < 0.5 else r.getrandbits(32)


def rbyte(r):
    return r.choice([0, 1, 2, 0xFF, 0x80]) if r.random() < 0.6 else r.getrandbits(8)


def rrec(r):
    """A random record: every field and the three padding bytes."""
    rec = {k: r32(r) for k in ("type", "time", "val", "staff", "prize", "follow", "off", "lost", "decl")}
    rec.update(obj=r.getrandbits(16), obj2=r.getrandbits(16), thing=r.getrandbits(16))
    for k in ("chk", "ind", "pad1e", "pad1f", "won", "pad2d"):
        rec[k] = rbyte(r)
    return rec


def pack_rec(rec, order):
    return struct.pack(order + REC_FMT, *[rec[k] & MASK if k in ("type", "time", "val", "staff", "prize", "follow", "off", "lost", "decl") else rec[k]
                                         for k in REC_KEYS])


def unpack_rec(raw, order):
    return dict(zip(REC_KEYS, struct.unpack(order + REC_FMT, raw)))


def observe_block(raw, order):
    """The record's fields and padding, and the guards before and after, from the 0x110 bytes of the block."""
    return dict(rec=unpack_rec(raw[GUARD:GUARD + RECORD], order), before=raw[:GUARD].hex(), after=raw[GUARD + RECORD:].hex())


def le(value, size):
    return (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


# ======================================================================= PowerPC side


class PsState:
    pass


def stub_ctrace(e):
    e.cl.rec("CTRACE", [e.r(3) & MASK], [e.cstr(e.r(4))[:159]])


def lb_read(e):
    # r3 the result record, r4 the file, r5 the buffer, r6 the length, r7 the count
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = e.ps.file[e.ps.cur:]
    taken = min(len(data), length)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = T.s32(L.ret("LbFile_Read") & MASK)
    if count == length and result >= 0:
        e.write(buf, (data[:taken] + bytes(length))[:length])
    e.ps.cur += taken
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


def stub_get_analyser(e):
    e.cl.rec("CWorld::GetParkAnalyser", (e.cl.lab(e.r(3)),))
    e.set_r(3, AN_ADDR[0])


def id_reader(name, has_type, has_id):
    """A reader that takes (this, [id&], [object type in the high half of r5 or r4]) and answers from the queue `name`."""
    def stub(e):
        L = e.cl
        ints = [L.lab(e.r(3))]
        if has_type:
            ints.append((e.r(5) >> 16) & 0xFFFF)
        L.rec(name, ints)
        if has_id:
            e.write(e.r(4), (L.ret(name + ":id") & 0xFFFF).to_bytes(2, "big"))
        e.set_r(3, L.ret(name) & MASK)
    return stub


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(OWN | REAL)
    S.internal(0x1000DDE8, lambda e: e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
    S.internal(0x1015B3DC, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
    S.internal(0x1006FCBC, stub_ctrace)
    S.internal(0x1013EBC8, stub_get_analyser)
    S.internal(0x100ECC80, id_reader("CParkAnalyser::GetCoasterWithMostLoops", True, True))
    S.internal(0x100EC668, id_reader("CParkAnalyser::GetGokartWithMostCrossRoads", False, True))
    S.internal(0x100EC858, id_reader("CParkAnalyser::GetLongestTrackRide", True, True))
    S.internal(0x100EA3B0, R("CParkAnalyser::GetHighestUpgradeLevelForObject", "Lw", ret="i"))
    S.internal(0x100E7278, R("CParkAnalyser::NumberOfVisitorsInPark", "L", ret="i"))
    S.internal(0x100E7A94, R("CParkAnalyser::NumberOfVisitorsWithBalloons", "L", ret="i"))
    S.internal(0x100E7BD8, R("CParkAnalyser::NumberOfVisitorsWithCostumes", "L", ret="i"))
    S.internal(0x100E88A0, R("CParkAnalyser::AverageQualityOfStaffType", "Li", ret="i"))
    S.internal(0x100EA52C, R("CParkAnalyser::NumberOfObjectInPark", "Lw", ret="i"))
    S.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    S.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
    return S


STUBS = None
WORLD_ADDR = []
AN_ADDR = []


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    WORLD_ADDR[:] = [T.persist_alloc(e, 0x40)]
    AN_ADDR[:] = [T.persist_alloc(e, 0x40)]
    STUBS = build_stubs()


def ppc_begin(e, c):
    """The fakes active, a fresh log and the world pointer; returns the state to hand to ppc_end."""
    saved = STUBS.activate(e)
    e.cl = CL.PpcLog({GSAVELOAD: L_SAVELOAD, FILE_HANDLE: L_FILE, WORLD_ADDR[0]: L_WORLD, AN_ADDR[0]: L_AN}, c.get("q", {}))
    e.ps = PsState()
    e.ps.file, e.ps.cur = c.get("file", b""), 0
    e.write(GGAMEWORLD, WORLD_ADDR[0].to_bytes(4, "big"))
    return saved


def ppc_end(e, saved):
    CL.Stubs.deactivate(e, saved)
    e.cl = None


def ppc_block(e, c):
    return e.alloc(GUARD + RECORD + GUARD, GUARD_BYTES + pack_rec(c["rec"], ">") + GUARD_BYTES, alignment=16)


# ======================================================================= host side


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_chl_addr.restype, lib.tpw_chl_addr.argtypes = U64, [C.c_char_p]
    lib.tpw_chl_alloc.restype, lib.tpw_chl_alloc.argtypes = U64, [U32]
    lib.tpw_chl_set_file.argtypes = [C.c_char_p, I32]
    lib.tpw_chl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_chl_ctor.argtypes = [U64] + [I32] * 6 + [U32, U32, I32, I32]
    lib.tpw_chl_serialise.restype, lib.tpw_chl_serialise.argtypes = I32, [U64, I32, I32, U32]
    lib.tpw_chl_did.restype, lib.tpw_chl_did.argtypes = U32, [U64, U64, I32]
    lib.tpw_chl_get.restype, lib.tpw_chl_get.argtypes = I32, [I32, U64]
    lib.tpw_chl_inst.restype, lib.tpw_chl_inst.argtypes = I32, [I32, U64, C.c_char_p, I32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_chl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def host_begin(lib, c):
    bind(lib)
    lib.tpw_chl_set_file(c.get("file", b""), len(c.get("file", b"")))
    CL.host_begin(lib, {L_SAVELOAD: lib.tpw_chl_addr(b"gSaveLoad"), L_FILE: lib.tpw_chl_addr(b"file"), L_WORLD: lib.tpw_chl_addr(b"world"),
                        L_AN: lib.tpw_chl_addr(b"analyser")}, c.get("q", {}))


def host_block(lib, c):
    addr = lib.tpw_chl_alloc(GUARD + RECORD + GUARD)
    C.memmove(addr, GUARD_BYTES + pack_rec(c["rec"], "<") + GUARD_BYTES, GUARD + RECORD + GUARD)
    return addr


# ======================================================================= the constructor


def ctor_ppc(e, c):
    saved = ppc_begin(e, c)
    try:
        block = ppc_block(e, c)
        a = c["args"]
        T.call_ppc(e, CTOR, [block + GUARD, a["type"] & MASK, a["time"] & MASK, a["prize"] & MASK, a["follow"] & MASK, a["chk"], a["val"] & MASK,
                             ((a["obj"] << 16) | a["junk"]) & MASK, ((a["obj2"] << 16) | a["junk2"]) & MASK, a["staff"] & MASK,
                             ((a["junk3"] << 8) | a["ind"]) & MASK])
        out = observe_block(e.read(block, GUARD + RECORD + GUARD), ">")
        out["log"] = e.cl.log
    finally:
        ppc_end(e, saved)
    return out


def ctor_host(lib, c):
    host_begin(lib, c)
    addr = host_block(lib, c)
    a = c["args"]
    lib.tpw_chl_ctor(addr + GUARD, T.s32(a["type"]), T.s32(a["time"]), T.s32(a["prize"]), T.s32(a["follow"]), a["chk"], T.s32(a["val"]),
                     a["obj"], a["obj2"], T.s32(a["staff"]), a["ind"])
    out = observe_block(C.string_at(addr, GUARD + RECORD + GUARD), "<")
    out["log"] = CL.host_end(lib)
    return out


def ctor_case(r):
    return dict(rec=rrec(r), args=dict(type=r32(r), time=r32(r), prize=r32(r), follow=r32(r), chk=r.getrandbits(1), val=r32(r),
                                       obj=r.getrandbits(16), obj2=r.getrandbits(16), junk=r.getrandbits(16), junk2=r.getrandbits(16),
                                       junk3=r.getrandbits(24), staff=r32(r), ind=r.getrandbits(1)))


def ctor_edges():
    r = random.Random(1740)
    for chk in (0, 1):
        for ind in (0, 1):
            for v in P32:
                c = ctor_case(r)
                c["args"].update(chk=chk, ind=ind, type=v, time=v, prize=v, follow=v, val=v, staff=v)
                c["args"].update(obj=v & 0xFFFF, obj2=(v >> 8) & 0xFFFF)
                yield c
    for rec_fill in (0, 0xFF):
        c = ctor_case(r)
        c["rec"] = {k: (rec_fill if k in ("chk", "ind", "pad1e", "pad1f", "won", "pad2d") else (0xFFFF if k in ("obj", "obj2", "thing") else rec_fill * 0x1010101))
                    for k in REC_KEYS}
        yield c


# ======================================================================= Serialise


def ser_ppc(e, c):
    saved = ppc_begin(e, c)
    try:
        block = ppc_block(e, c)
        r3 = T.call_ppc(e, SER, [block + GUARD, FILE_HANDLE, c["save"] & MASK, c["version"] & MASK, c["unused"] & MASK])[0]
        out = observe_block(e.read(block, GUARD + RECORD + GUARD), ">")
        out.update(result=T.s32(r3), log=e.cl.log)
    finally:
        ppc_end(e, saved)
    return out


def ser_host(lib, c):
    host_begin(lib, c)
    addr = host_block(lib, c)
    result = lib.tpw_chl_serialise(addr + GUARD, T.s32(c["save"]), T.s32(c["version"]), c["unused"] & MASK)
    out = observe_block(C.string_at(addr, GUARD + RECORD + GUARD), "<")
    out.update(result=result, log=CL.host_end(lib))
    return out


def file_image(r, rec=None):
    """The 45 bytes the loader reads, little-endian, in its field order (bool bytes random)."""
    rec = rec or rrec(r)
    order = ["type", "time", "val", "obj", "obj2", "staff", "prize", "follow", "chk", "off", "lost", "decl", "won", "ind", "thing"]
    return b"".join(le(rec[k], size) for k, size in zip(order, FIELD_SIZES))


def fail_queue(r, loads):
    """A failure at field k (the result, a short count or a long one)."""
    k = r.randrange(15)
    call = "LbFile_Read" if loads else "LbFile_Write"
    kind = r.choice(["result", "short", "long", "both"])
    q = {}
    if kind in ("result", "both"):
        q[call] = [0] * k + [r.choice([-1, -2, 1 << 31, 5])]
    if kind in ("short", "long", "both"):
        bad = FIELD_SIZES[k] - 1 if kind == "short" else FIELD_SIZES[k] + r.choice([1, 4, 0x100000000 - FIELD_SIZES[k]])
        q[call + ":xfer"] = FIELD_SIZES[:k] + [bad & MASK if bad >= 0 else 0]
    return q


def ser_case(r, save=None, version=None, q=None, file=None):
    save = r.choice([0, 1]) if save is None else save
    if version is None:
        version = r.choice([0, 1, 0, 1, 0, 1, -1, 2, 3, r.getrandbits(32), r.randint(-5, 5)])
    rec = rrec(r)
    data = file_image(r) if file is None else file
    if q is None:
        q = fail_queue(r, save == 0) if r.random() < 0.25 else {}
    return dict(rec=rec, save=save, version=version, unused=r32(r), file=data if save == 0 else b"", q=q)


def ser_edges():
    r = random.Random(1741)
    for save in (0, 1, 2, -1, 0x80000000):
        for version in (-1, 0, 1, 2, 3, 0x7FFFFFFF, -0x80000000, 0x80000000):
            yield ser_case(r, save, version, {})
    for save in (0, 1):
        for version in (0, 1):
            for k in range(15):
                call = "LbFile_Read" if save == 0 else "LbFile_Write"
                yield ser_case(r, save, version, {call: [0] * k + [-1]})
                yield ser_case(r, save, version, {call + ":xfer": FIELD_SIZES[:k] + [FIELD_SIZES[k] - 1]})
                yield ser_case(r, save, version, {call + ":xfer": FIELD_SIZES[:k] + [FIELD_SIZES[k] + 1]})
                yield ser_case(r, save, version, {call + ":xfer": FIELD_SIZES[:k] + [0], call: [0] * k + [-3]})
            # a file cut short in every field, and an empty one
            image = file_image(r)
            for cut in range(0, 45, 3):
                yield ser_case(r, 0, version, {}, image[:cut])
            yield ser_case(r, 0, version, {}, image + b"\x01\x02\x03")
    # bool bytes of 2 and 0xff stay as they are
    rec = rrec(r)
    rec.update(chk=2, won=0xFF, ind=2)
    yield ser_case(r, 0, 1, {}, file_image(r, rec))
    rec.update(chk=0xFF, won=2, ind=0x80)
    yield ser_case(r, 1, 1, {}, b"")


def ser_random(r):
    return ser_case(r)


# ======================================================================= DidThisWinIt

TYPES = list(range(0, 0x23)) + [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x100, 0x40]
PAIRS = [(5, 5), (4, 5), (6, 5), (-0x80000000, 5), (0x7FFFFFFF, 5), (5, -0x80000000), (5, 0x7FFFFFFF), (0, 0), (-1, -1), (-1, 0), (0, -1)]
ANALYSER_KEYS = ["CParkAnalyser::" + n for n in (
    "GetCoasterWithMostLoops", "GetGokartWithMostCrossRoads", "GetLongestTrackRide", "GetHighestUpgradeLevelForObject", "NumberOfVisitorsInPark",
    "NumberOfVisitorsWithBalloons", "NumberOfVisitorsWithCostumes", "AverageQualityOfStaffType", "NumberOfObjectInPark")]
ID_KEYS = [k + ":id" for k in ANALYSER_KEYS[:3]]


def did_ppc(e, c):
    saved = ppc_begin(e, c)
    try:
        block = ppc_block(e, c)
        vbuf = e.alloc(12, GUARD_BYTES[:4] + (c["value"] & MASK).to_bytes(4, "big") + GUARD_BYTES[4:8], alignment=16)
        e.set_r(28, 0)   # the model the original leaves in r28 for a staff type outside 0 to 4: the translation passes 0
        r3 = T.call_ppc(e, DID, [block + GUARD, vbuf + 4, c["atend"]])[0]
        raw = e.read(vbuf, 12)
        out = observe_block(e.read(block, GUARD + RECORD + GUARD), ">")
        out.update(result=r3 & MASK, value=int.from_bytes(raw[4:8], "big"), vguard=(raw[:4] + raw[8:]).hex(), log=e.cl.log)
    finally:
        ppc_end(e, saved)
    return out


def did_host(lib, c):
    host_begin(lib, c)
    addr = host_block(lib, c)
    vbuf = lib.tpw_chl_alloc(12)
    C.memmove(vbuf, GUARD_BYTES[:4] + le(c["value"], 4) + GUARD_BYTES[4:8], 12)
    result = lib.tpw_chl_did(addr + GUARD, vbuf + 4, c["atend"])
    raw = C.string_at(vbuf, 12)
    out = observe_block(C.string_at(addr, GUARD + RECORD + GUARD), "<")
    out.update(result=result, value=int.from_bytes(raw[4:8], "little"), vguard=(raw[:4] + raw[8:]).hex(), log=CL.host_end(lib))
    return out


def answers(r, tval=None):
    """The queues of one DidThisWinIt case: every analyser reader answers (and writes an out id) from a one-value queue."""
    q = {}
    for key in ANALYSER_KEYS:
        if tval is not None and r.random() < 0.6:
            q[key] = [T.s32(tval + r.choice([-1, 0, 1]))]
        else:
            q[key] = [r.choice(ANS) if r.random() < 0.6 else T.s32(r.getrandbits(32))]
    for key in ID_KEYS:
        q[key] = [r.getrandbits(16)]
    return q


def did_case(r, t=None, value=None, tval=None, ttime=None, chk=None, atend=None, staff=None, q=None, obj=None):
    rec = rrec(r)
    if t is not None:
        rec["type"] = t
    if tval is not None:
        rec["val"] = tval & MASK
    if ttime is not None:
        rec["time"] = ttime & MASK
    if chk is not None:
        rec["chk"] = chk
    elif r.random() < 0.7:
        rec["chk"] = 0
    if staff is not None:
        rec["staff"] = staff & MASK
    if obj is not None:
        rec["obj"], rec["obj2"] = obj
    value = T.s32(r32(r)) if value is None else value
    atend = r.getrandbits(1) if atend is None else atend
    return dict(rec=rec, value=value, atend=atend, q=answers(r, T.s32(rec["val"])) if q is None else q)


def did_edges():
    r = random.Random(1742)
    for t in TYPES:
        for value, tval in PAIRS:
            yield did_case(r, t, value, tval, chk=0, atend=r.getrandbits(1))
            if t < 0x22 and t not in (0, 23, 26):
                c = did_case(r, t, value, tval, chk=0)
                c["q"] = {k: [T.s32(tval + d)] for k in ANALYSER_KEYS for d in (r.choice([-1, 0, 1]),)}
                c["q"].update({k: [r.getrandbits(16)] for k in ID_KEYS})
                yield c
    # mCheckAtEndOnly: a byte of 0, 1, 2 and 0xff, with atEnd 0 and 1, on a type that answers true
    for chk in (0, 1, 2, 0xFF, 0x80, 0x100 & 0xFF):
        for atend in (0, 1):
            for t in (0, 1, 9, 19, 23, 30, 33, 0x40):
                yield did_case(r, t, 7, 7, chk=chk, atend=atend)
    # the visitors: the percentage of balloons and costumes, with a product that wraps
    for t in (9, 10):
        for n in (0, -1, 1, 7, 0x7FFFFFFF, -0x80000000, 3, r.getrandbits(32)):
            for c in (0, 1, -1, 0x02000000, 0x028F5C29, 0x028F5C2A, 0x7FFFFFFF, -0x80000000, 33, r.getrandbits(32)):
                for tval in (0, 1, 33, 100, -1):
                    q = answers(r, tval)
                    q["CParkAnalyser::NumberOfVisitorsInPark"] = [T.s32(n)]
                    q["CParkAnalyser::NumberOfVisitorsWithBalloons"] = [T.s32(c)]
                    q["CParkAnalyser::NumberOfVisitorsWithCostumes"] = [T.s32(c)]
                    yield did_case(r, t, T.s32(r.getrandbits(32)), tval, chk=0, atend=1, q=q)
    # the staff quality: every staff type, the model passed and the trace
    for staff in (-1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, -0x80000000, r.getrandbits(32), 0x100000000 - 5):
        for avg in (-1, 0, 49, 50, 51, 100):
            q = answers(r)
            q["CParkAnalyser::AverageQualityOfStaffType"] = [avg]
            yield did_case(r, 29, T.s32(r.getrandbits(32)), 50, chk=0, atend=1, staff=staff, q=q)
    # the division types
    for t in (24, 25, 27):
        for ttime in (0, -1, 1, 2, 3, -3, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)):
            for value in (-0x80000000, 0x7FFFFFFF, 0, 10, -10, 99):
                for tval in (0, 1, 3, -1, -0x80000000, 0x7FFFFFFF, 33):
                    yield did_case(r, t, value, tval, ttime, chk=0)
    # the follow-up thing id: kept when the answer is below the target
    for t in (14, 15, 18):
        for ans in (-1, 0, 1, 2, 3, 4):
            q = answers(r)
            for k in ANALYSER_KEYS[:3]:
                q[k] = [ans]
            yield did_case(r, t, 0, 3, chk=0, q=q)
    # the object counts of types 30 to 33: the second is asked only when the first passes
    for t in (30, 31, 32, 33):
        for a in (-1, 0, 1, 2):
            q = answers(r)
            q["CParkAnalyser::NumberOfObjectInPark"] = [a, 0, 1] if t == 33 else [a]
            yield did_case(r, t, 0, 0, chk=0, q=q)
            q = answers(r)
            q["CParkAnalyser::NumberOfObjectInPark"] = [a, -1]
            yield did_case(r, t, 0, 0, chk=0, q=q, obj=(0xFFFF, 0))


def did_random(r):
    t = r.randrange(0, 0x23) if r.random() < 0.9 else r32(r)
    value = r.choice([T.s32(r32(r)), 0, 1, -1]) if r.random() < 0.8 else r.randint(-200, 200)
    tval = None
    if r.random() < 0.5:
        tval = value + r.choice([-1, 0, 1])
    c = did_case(r, t, value, tval, ttime=r32(r) if r.random() < 0.5 else None, staff=r.choice([-1, 0, 1, 2, 3, 4, 5, r32(r)]) if t == 29 else None)
    return c


# ======================================================================= the getters


def get_test(index):
    name, addr = GETTERS[index]

    def ppc(e, c):
        saved = ppc_begin(e, c)
        try:
            block = ppc_block(e, c)
            r3 = T.call_ppc(e, addr, [block + GUARD])[0]
            out = observe_block(e.read(block, GUARD + RECORD + GUARD), ">")
            out.update(result=T.s32(r3), log=e.cl.log)
        finally:
            ppc_end(e, saved)
        return out

    def host(lib, c):
        host_begin(lib, c)
        addr_h = host_block(lib, c)
        result = lib.tpw_chl_get(index, addr_h + GUARD)
        out = observe_block(C.string_at(addr_h, GUARD + RECORD + GUARD), "<")
        out.update(result=result, log=CL.host_end(lib))
        return out

    def case(r, t):
        rec = rrec(r)
        rec["type"] = t
        return dict(rec=rec)

    def edge():
        r = random.Random(1743 + index)
        return [case(r, t) for t in list(range(0, 0x24)) + [0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x100, 0x40, 0xFFFFFFDF, 0x22 + (1 << 32) - (1 << 32)]]

    def rnd(r):
        return case(r, r.randrange(0, 0x23) if r.random() < 0.5 else r32(r))

    return FnTest("CChallenge::" + name, ppc, host, edge=edge, rnd=rnd)


# ======================================================================= the four instances


def inst_test(index):
    name, addr, loads = INSTANCES[index]

    def ppc(e, c):
        saved = ppc_begin(e, c)
        try:
            buf = e.alloc(16, c["init"].to_bytes(4, "big") + GUARD_BYTES[:12], alignment=8)
            text = CL.new_text(e, c["name"])
            r3 = T.call_ppc(e, addr, [FILE_HANDLE, buf, text, c["no_log"] & MASK])[0]
            raw = e.read(buf, 16)
            out = dict(result=T.s32(r3), value=int.from_bytes(raw[:4], "big"), rest=raw[4:].hex(), log=e.cl.log)
        finally:
            ppc_end(e, saved)
        return out

    def host(lib, c):
        host_begin(lib, c)
        addr_h = lib.tpw_chl_alloc(16)
        C.memmove(addr_h, le(c["init"], 4) + GUARD_BYTES[:12], 16)
        result = lib.tpw_chl_inst(index, addr_h, c["name"].encode("latin-1"), c["no_log"])
        raw = C.string_at(addr_h, 16)
        return dict(result=result, value=int.from_bytes(raw[:4], "little"), rest=raw[4:].hex(), log=CL.host_end(lib))

    def make(value, init, data, name="x", no_log=0, q=None):
        return dict(init=init, file=bytes(data), name=name, no_log=no_log, q=q or {})

    call = "LbFile_Read" if loads else "LbFile_Write"

    def edge():
        cases = []
        for k, v in enumerate(P32):
            data = le(v, 4) if loads else b""
            cases.append(make(v, P32[(k + 3) % len(P32)], data))
            cases.append(make(v, v, data, name=NAMES[k % len(NAMES)], no_log=1))
        for no_log in NO_LOGS:
            cases.append(make(3, 0, le(3, 4) if loads else b"", no_log=no_log))
        for name_ in NAMES:
            cases.append(make(2, 0, le(2, 4) if loads else b"", name=name_))
        for result in RESULTS:
            cases.append(make(7, 1, le(7, 4) if loads else b"", q={call: [result]}))
        for count in (0, 1, 3, 4, 5, 8, 0xFFFFFFFF, 0xFFFFFFFC):
            cases.append(make(7, 1, le(7, 4) if loads else b"", q={call + ":xfer": [count]}))
            cases.append(make(7, 1, le(7, 4) if loads else b"", q={call + ":xfer": [count], call: [-1]}))
        if loads:
            cases.append(make(7, 1, b""))
            cases.append(make(7, 1, le(7, 4)[:3]))
            cases.append(make(7, 1, le(7, 4) + b"\x01\x02\x03\x04"))
        return cases

    def rnd(r):
        value, init = r32(r), r32(r)
        data = le(value, 4) if loads else b""
        if loads and r.random() < 0.1:
            data = data[:r.randint(0, 4)]
        q = {}
        if r.random() < 0.2:
            q[call] = [r.choice(RESULTS)]
        if r.random() < 0.2:
            q[call + ":xfer"] = [r.choice([0, 1, 3, 4, 5, 0xFFFFFFFF, r.randint(0, 9)])]
        name = r.choice(NAMES) if r.random() < 0.5 else "".join(r.choice("abcXYZ_[]019 ") for _ in range(r.randint(0, 30)))
        return make(value, init, data, name=name, no_log=r.choice(NO_LOGS + [0, 0, 1, 1]), q=q)

    return FnTest(name, ppc, host, edge=edge, rnd=rnd)


TESTS = [
    FnTest("CChallenge::CChallenge", ctor_ppc, ctor_host, edge=ctor_edges, rnd=ctor_case),
    FnTest("CChallenge::Serialise", ser_ppc, ser_host, edge=ser_edges, rnd=ser_random),
    FnTest("CChallenge::DidThisWinIt", did_ppc, did_host, edge=did_edges, rnd=did_random),
] + [get_test(i) for i in range(4)] + [inst_test(i) for i in range(4)]
