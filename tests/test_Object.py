"""Unit w126: CObject::CObject (0x10105d74) and CObject::SetState (0x1010dc94) against the original PowerPC code.

Every callee outside the unit (the thing base, the world, the message centre and its four ride messages, the ride record, the map, the
golden ticket control, the bank, the controls, the script, the engine members of the object, the sign names, strncpy) is a recording
fake on both sides; each logs its arguments and a hash of the object's fields at the call. The two history constructors the
constructor runs and the SetState it calls (for 0 and 3) run as the original. Every written scalar compares by value; every other
byte of the object (and its 0x40 guard) must keep the fill. Fills are palindromic per field so the hash is the same on both sides.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Object"
ADDR = [0x10105D74, 0x1010DC94]
NAMES = ["CObject::CObject", "CObject::SetState"]
OWN = {0x10105D74, 0x1010DC94, 0x10110FA0, 0x10111514}
MASK = 0xFFFFFFFF
OBJ_SIZE = 0x448 + 0x40
FILLS = [0, 0xA5, 0xFF, "rand"]
G_WORLD, G_CENTRE, G_MAP, G_THINGS, G_ANIM, G_RIDEDB = 0x1037EC0C, 0x1037F6C4, 0x1037EC08, 0x1034CBFC, 0x102C79A4, 0x1037F6C8
WORLD_TAIL = 0x1DA6F0
MISC = 15
BANK, ANALYSER = 1, 2
CTL, PERSON, SCRIPT, ANIM = 3, 6, 9, 11   # first misc index of each group
LAB_MISC = 5

# The ride record's value slots the constructor reads (RideBalanceSlot) and the Mac word each lives in (0xc + 4 * slot).
S_NAME, S_CHOOSABLE, S_QUEUE, S_CONTINUOUS, S_GOLDEN, S_PRICE, S_CHANCE = 2, 12, 13, 15, 46, 54, 56
S_SECURITY, S_RELIEF, S_LITTER, S_CHILLS, S_SPRITE, S_FIREWORKS, S_GOODS = 57, 58, 59, 60, 62, 65, 77
S_CAPACITY, S_DURATION, S_SPEED, S_UPGRADE = 99, 101, 103, 107
FLAG_SLOTS = [S_RELIEF, S_LITTER, S_CHILLS, S_CHOOSABLE, S_SPRITE, S_QUEUE, S_SECURITY, S_FIREWORKS, S_CONTINUOUS]
LAST_SLOT = 120


def history(base, width):
    return [(base, width), (base + 0x7C, 4), (base + 0x80, 4), (base + 0x84, 1)]


CTOR_FIELDS = [(i, 1) for i in range(0xC)]
CTOR_FIELDS += [(0xC, 2), (0xE, 2), (0x10, 4), (0x14, 8), (0x1C, 4), (0x20, 4), (0x24, 4), (0x28, 4), (0x2C, 2), (0x2E, 2),
                (0x30, 2), (0x32, 2), (0x34, 2), (0x36, 2), (0x38, 2), (0x3C, 4), (0x40, 4), (0x44, 4), (0x48, 4), (0x4C, 1),
                (0x50, 4), (0x54, 4), (0x58, 1), (0x59, 1), (0x5A, 2), (0x5C, 4), (0x60, 4), (0x64, 4), (0x68, 2)]
for b in (0x6C, 0xF4, 0x1A4, 0x22C, 0x2B4):
    CTOR_FIELDS += history(b, 4)
CTOR_FIELDS += history(0x33C, 4)
CTOR_FIELDS += [(o, 4) for o in (0x17C, 0x180, 0x184, 0x188, 0x18C, 0x190, 0x194, 0x198, 0x19C, 0x1A0)]
STATE_SNAP = [(0, 2), (0x1C, 4), (0x20, 4), (0x38, 2), (0x64, 4), (0x68, 2), (0x198, 4)]
STATE_WRITTEN = [(0x64, 4), (0x68, 2), (0x198, 4)]
WRITTEN = [CTOR_FIELDS, STATE_WRITTEN]
SNAP = [CTOR_FIELDS, STATE_SNAP]


def untouched(fields):
    covered = {i for o, w in fields for i in range(o, o + w)}
    return [i for i in range(OBJ_SIZE) if i not in covered]


UNTOUCHED = [untouched(f) for f in WRITTEN]


def hash_values(values):
    h = 2166136261
    for v in values:
        h = ((h ^ v) * 16777619) & MASK
    return h


def s64(v):
    v &= 0xFFFFFFFFFFFFFFFF
    return v - (1 << 64) if v & (1 << 63) else v


def pack(value, width, little):
    return (value & ((1 << (8 * width)) - 1)).to_bytes(width, "little" if little else "big")


def image(c, little):
    """The object block before the call: the fill (palindromic per snapshot field, so the hash agrees) and the case's own fields."""
    r = random.Random(c["seed"])
    buf = bytearray(r.randbytes(OBJ_SIZE) if c["fill"] == "rand" else bytes([c["fill"]]) * OBJ_SIZE)
    for o, w in SNAP[c["op"]]:
        buf[o:o + w] = bytes([buf[o]]) * w
    if c["op"] == 1:
        for (o, w), value in zip(STATE_SNAP, c["fields"]):
            buf[o:o + w] = pack(value, w, little)
    return bytes(buf)


def person_image(c, i, little):
    r = random.Random(c["seed"] + 1000 + i)
    buf = bytearray(r.randbytes(kBlock))
    buf[0x220:0x224] = pack(c["person"][i], 4, little)
    return bytes(buf)


kBlock = 0x280


def ride_words(c):
    """slot -> signed value of every slot the case sets, the special ones last."""
    return c["ride"]


def report(buf, img, op, little, ctl):
    order = "little" if little else "big"
    values = [int.from_bytes(buf[o:o + w], order) for o, w in WRITTEN[op]]
    changed = [(i, buf[i]) for i in UNTOUCHED[op] if buf[i] != img[i]]
    return {"fields": values, "untouched": changed, "controls": ctl}


def ctl_report(blocks, little):
    order = "little" if little else "big"
    out = []
    for buf in blocks:
        out.append([int.from_bytes(buf[0x18:0x1C], order), int.from_bytes(buf[0x1C:0x20], order),
                    [i for i in range(kBlock) if i not in range(0x18, 0x20) and buf[i] != 0]])
    return out


def ctl_image(c, i, little):
    buf = bytearray(kBlock)
    w18, w1c = c["ctl"][i]
    buf[0x18:0x1C] = pack(w18, 4, little)
    buf[0x1C:0x20] = pack(w1c, 4, little)
    return bytes(buf)


def people_report(blocks, c, little):
    return [[i for i in range(kBlock) if buf[i] != person_image(c, k, little)[i]] for k, buf in enumerate(blocks)]


def setup(runner):
    T.install_std_stubs(runner.emu)


def sp(c):
    """The fields of the table in the order of the report, for the PowerPC call arguments."""
    return c


def ppc_case(e, c, _unused=None):
    op = c["op"]
    L = CL.PpcLog({}, c["q"])
    e.cl = L
    img = image(c, False)
    obj = e.alloc(OBJ_SIZE, img)
    tail = e.alloc(0x100, b"\xa5" * 0x100)
    base = tail - WORLD_TAIL
    if base <= 0:
        raise RuntimeError("the world base would be below zero")
    centre = e.alloc(0x1D0, b"\xa5" * 0x1D0)
    ride = e.alloc(0x800, b"\0" * 0x800)
    misc = [e.alloc(kBlock, b"\0" * kBlock) for _ in range(MISC)]
    ctls = []
    for i in range(3):
        e.write(misc[CTL + i], ctl_image(c, i, False))
        ctls.append(misc[CTL + i])
    persons = []
    for i in range(3):
        e.write(misc[PERSON + i], person_image(c, i, False))
        persons.append(misc[PERSON + i])
    L.labels.update({base: 1, obj: 2, centre: 3, ride: 4})
    for i, a in enumerate(misc):
        L.labels[a] = LAB_MISC + i
    snapfields = SNAP[op]
    allocations = []

    def snap():
        return hash_values(int.from_bytes(e.read(obj + o, w), "big") for o, w in snapfields)

    def w32(a, v):
        e.write(a, struct.pack(">I", v & MASK))

    def sw(emu, a):
        return T.s32(emu.u32(a))

    def base_ctor(emu):
        p = emu.r(3)
        L.rec("CThing", [L.lab(p), emu.r(4) >> 16, T.s32(emu.r(5)), snap()])
        emu.write(p, b"\x5a" * 0xC)
        emu.write(p + 4, bytes([L.ret("cx", 0x5A) & 0xFF]))
        emu.write(p + 6, bytes([L.ret("cy", 0x5A) & 0xFF]))

    def thing_id(emu):
        ident = L.ret("id", 0x1234)
        L.rec("GetThingId", [L.lab(emu.r(4)), snap()])
        emu.write(emu.r(3), struct.pack(">H", ident & 0xFFFF))

    def register(emu):
        L.rec("Register", [L.lab(emu.r(3)), L.lab(emu.r(4)), snap()])

    def subscribe(emu):
        L.rec("Subscribe", [L.lab(emu.r(3)), L.lab(emu.r(4)), T.s32(emu.r(5)), snap()])
        if L.ret("clear_sub", 0):
            w32(G_CENTRE, 0)

    def message(name, kind):
        def stub(emu):
            p = emu.r(3)
            L.rec(name, [emu.r(4) >> 16, snap()])
            w32(p, kind)
            w32(p + 4, 0)
            e.write(p + 8, struct.pack(">H", emu.r(4) >> 16))
            if L.ret("clear_msg", 0):
                w32(G_CENTRE, 0)
        return stub

    def post(emu):
        m = emu.r(4)
        L.rec("PostMessage", [L.lab(emu.r(3)), e.u32(m), e.u32(m + 4), int.from_bytes(e.read(m + 8, 2), "big"), snap()])
        emu.set_r(3, 0)

    def count_args(text):
        return min(text.count("%"), 2)

    def trace(emu):
        text = emu.cstr(emu.r(4))
        n = count_args(text)
        L.rec("CTRACE", [emu.r(3), n, emu.r(5) if n > 0 else 0, emu.r(6) if n > 1 else 0, snap()], [text])

    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [T.s32(emu.r(3)), snap()], [emu.cstr(emu.r(4))])

    def db_info(emu):
        L.rec("GetDBInfo", [L.lab(emu.r(3)), snap()])
        emu.set_r(3, ride)

    def rotate(emu):
        out, this, rotation = emu.r(3), emu.r(4), emu.r(5)
        which = {0x7AC: 0, 0x790: 1, 0x79C: 2}.get(this - ride, -1)
        L.rec("Rotate", [which, sw(emu, this), sw(emu, this + 4), T.s32(rotation), snap()])
        w32(out, L.ret("rx", 0))
        w32(out + 4, L.ret("ry", 0))
        e.write(out + 8, bytes([L.ret("rf", 0) & 0xFF]))

    def effector(emu):
        L.rec("AddRegionalEffector", [L.lab(emu.r(3)), T.s32(emu.r(4)), emu.r(5) >> 16, snap()])

    def rel(a):
        return a - base

    def bought(emu):
        L.rec("HasBeenBought", [rel(emu.r(3)), emu.r(4) >> 16, snap()])
        emu.set_r(3, 1 if L.ret("bought", 0) else 0)

    def spend(emu):
        L.rec("SpendTicketOn", [rel(emu.r(3)), emu.r(4) >> 16, snap()])

    def bank(emu):
        L.rec("GetBankAccount", [L.lab(emu.r(3)), snap()])
        emu.set_r(3, misc[BANK])

    def withdrawal(emu):
        L.rec("MakeWithdrawal", [L.lab(emu.r(3)), emu.r(4), snap()])

    def analyser(emu):
        L.rec("GetParkAnalyser", [L.lab(emu.r(3)), snap()])
        emu.set_r(3, misc[ANALYSER])

    def built(emu):
        L.rec("BuiltSomething", [L.lab(emu.r(3)), snap()])

    def funny(emu):
        world = emu.u32(G_WORLD)
        args = [int(emu.r(i) != 0) for i in range(5, 11)]
        args += [int(emu.u32(emu.r(1) + o) != 0) for o in (0x38, 0x3C)]
        L.rec("GetFunnyTime", [L.lab(world), (emu.r(4) - world), *args, snap()])
        emu.write(emu.r(3), (L.ret("funny", 0) & 0xFFFFFFFFFFFFFFFF).to_bytes(8, "big"))

    def control(emu):
        which = L.ret("ctl", 0) % 3
        L.rec("GetObjectControl", [rel(emu.r(3)), emu.r(4) >> 16, T.s32(emu.r(5)), which, snap()])
        emu.set_r(3, ctls[which])

    def member(name, spec):
        def stub(emu):
            ints = [L.lab(emu.r(3))]
            if spec == "i":
                ints.append(T.s32(emu.r(4)))
            elif spec == "u":
                ints.append(emu.r(4))
            elif spec == "b":
                ints.append(emu.r(4) & 0xFF)
            elif spec == "w":
                ints.append(emu.r(4) >> 16)
            L.rec(name, ints + [snap()])
        return stub

    def strncpy(emu):
        L.rec("strncpy", [int(emu.r(3) != 0), emu.r(5)], [emu.cstr(emu.r(4))])
        emu.set_r(3, emu.r(3))

    def sign(name):
        def stub(emu):
            L.rec(name, [int(emu.r(3) == G_RIDEDB), emu.r(4) >> 16, emu.r(5) - obj, emu.r(6), snap()])
        return stub

    def script_pointer(emu):
        which = L.ret("scr", 0) & 1
        L.rec("RSSE_GetScriptPointer", [T.s32(emu.r(3)), which, snap()])
        emu.set_r(3, misc[SCRIPT + which])

    def getvar(emu):
        L.rec("RSSE_Getvar", [L.lab(emu.r(3)), T.s32(emu.r(4)), snap()])
        emu.set_r(3, L.ret("var", 0) & MASK)

    def setvar(emu):
        L.rec("RSSE_Setvar", [L.lab(emu.r(3)), T.s32(emu.r(4)), T.s32(emu.r(5)), snap()])

    def hoardings(emu):
        L.rec("ActivateHoardings", [L.lab(emu.r(3)), emu.r(4), snap()])

    def op_new(emu):
        size = emu.r(3)
        fail = bool(L.ret("newnull", 0))
        L.rec("new", [size, int(fail)])
        p = 0 if fail else emu.alloc(size, b"\xa5" * size)
        if p:
            allocations.append(p)
        emu.set_r(3, p)

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == 0x102657E8:
            L.rec("CMessageCentre", [L.newlab(p), int(ctor == 0x102657E8), int(dtor == 0x10267288), size, count])
            return
        for i in range(count):
            call(ctor, p + i * size, 1)

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in [
        (0x1012DE58, base_ctor), (0x1012E1F8, thing_id), (0x1013FA10, register), (0x10152388, subscribe),
        (0x10151998, message("CMsgNewRide", 6)), (0x10151A14, message("CMsgRideBrokenDown", 7)),
        (0x10151A98, message("CMsgRideCondemned", 8)), (0x10151B1C, message("CMsgRideWantsUpgrade", 9)),
        (0x1015256C, post), (0x1006FCBC, trace), (0x1000DDE8, assertion), (0x10107CD4, db_info), (0x10103D88, rotate),
        (0x101016CC, effector), (0x100FBFEC, bought), (0x100FBEA0, spend), (0x1013EDFC, bank), (0x100F33B0, withdrawal),
        (0x1013EBC8, analyser), (0x100E46A4, built), (0x101127D0, funny), (0x100FA0B8, control),
        (0x10107D38, member("UpdateEngine", "i")), (0x101076B0, member("CreateRideScript", "")),
        (0x10108068, member("SetOperatingSpeed", "u")), (0x10108248, member("SetOperatingCapacity", "b")),
        (0x10108104, member("SetOperatingDuration", "b")), (0x1010ABCC, member("CloseObject", "")),
        (0x10108C20, member("RemovePersonFromQueue", "w")), (0x1011DF70, member("ForceRideAdmittance", "")),
        (0x1011F1B8, member("DismissFromQueue", "")), (0x10154C24, sign("GetSignNameA")), (0x10154D68, sign("GetSignNameB")),
        (0x100D668C, script_pointer), (0x100D6C58, getvar), (0x100D6734, setvar), (0x100614A0, hoardings)]:
        S.internal(addr, fn)
    S.imp("strncpy", strncpy)
    S.imp("__nw__FUl", op_new)
    saved = S.activate(e)
    try:
        T.stub_cb(e, "__construct_array", construct)
        w32(G_WORLD, base)
        w32(G_MAP, misc[0])
        w32(G_CENTRE, centre if c["centre"] else 0)
        w32(base + 0x1DA70C, c["turn"])
        for i in range(16):
            w32(G_ANIM + 4 * i, misc[ANIM + c["anim"][i]] if c["anim"][i] >= 0 else 0)
        w32(G_THINGS, 0)
        for index, block in c["things"].items():
            w32(G_THINGS + 0x14 * index, misc[block] if block >= 0 else 0)
        if op == 0:
            e.write(ride, struct.pack(">H", c["type"]) + bytes(c["typeg"].to_bytes(2, "big")))
            for slot, v in c["ride"].items():
                if slot != S_NAME:
                    w32(ride + 0xC + 4 * slot, v)
            name = e.alloc(0x80, c["name"].encode() + b"\0")
            w32(ride + 0xC + 4 * S_NAME, name)
            for k, off in enumerate((0x7AC, 0x7B0, 0x790, 0x794, 0x79C, 0x7A0)):
                w32(ride + off, c["tri"][k])
            args = [obj, (c["map"] << 16) | c["mapg"], (c["type"] << 16) | c["typeg"], c["rot"] & MASK, c["b"] & MASK]
        else:
            args = [obj, c["state"] & MASK]
        T.call_ppc(e, ADDR[op], args, max_insns=50_000_000, timeout_us=60_000_000)
        out = report(e.read(obj, OBJ_SIZE), img, op, False, ctl_report([e.read(a, kBlock) for a in ctls], False))
        for i in range(3):
            pass
        out["controls"] = [[w18, w1c, [] if not extra else extra] for w18, w1c, extra in out["controls"]]
        # the controls' own changed bytes beyond the two words
        out["people"] = people_report([e.read(a, kBlock) for a in persons], c, False)
        out["globals"] = [L.lab(e.u32(G_WORLD)), L.lab(e.u32(G_CENTRE)), len(allocations)]
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def host_case(lib, c, _unused=None):
    op = c["op"]
    lib.tpw_obj_ptr.restype = C.c_void_p
    lib.tpw_obj_ptr.argtypes = [C.c_int32]

    def ptr(k):
        return lib.tpw_obj_ptr(k)

    img = image(c, True)
    obj = ptr(0)
    C.memmove(obj, img, OBJ_SIZE)
    table = (C.c_int32 * (2 * len(SNAP[op])))(*[v for pair in SNAP[op] for v in pair])
    lib.tpw_obj_fields.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_obj_fields(table, len(SNAP[op]))
    lib.tpw_obj_prepare.argtypes = [C.c_int32]
    lib.tpw_obj_prepare(c["centre"])
    base = ptr(1)
    tail = ptr(4)
    C.memmove(tail, b"\xa5" * 0x100, 0x100)
    C.memmove(tail + 0x1C, struct.pack("<I", c["turn"] & MASK), 4)
    C.memmove(ptr(2), b"\xa5" * 0x1D0, 0x1D0)
    for k in range(MISC):
        C.memmove(ptr(LAB_MISC + k), b"\0" * kBlock, kBlock)
    for i in range(3):
        C.memmove(ptr(LAB_MISC + CTL + i), ctl_image(c, i, True), kBlock)
        C.memmove(ptr(LAB_MISC + PERSON + i), person_image(c, i, True), kBlock)
    lib.tpw_obj_thing.argtypes = [C.c_int32, C.c_int32]
    for index, block in c["things"].items():
        lib.tpw_obj_thing(index, block)
    lib.tpw_obj_anim.argtypes = [C.c_int32, C.c_int32]
    for i in range(16):
        lib.tpw_obj_anim(i, c["anim"][i] + ANIM if c["anim"][i] >= 0 else -1)
    if op == 0:
        lib.tpw_obj_ride_id.argtypes = [C.c_int32]
        lib.tpw_obj_ride_id(c["type"])
        lib.tpw_obj_ride_value.argtypes = [C.c_int32, C.c_int32]
        for slot, v in c["ride"].items():
            if slot != S_NAME:
                lib.tpw_obj_ride_value(slot, T.s32(v))
        lib.tpw_obj_ride_name.argtypes = [C.c_char_p]
        lib.tpw_obj_ride_name(c["name"].encode())
        tri = (C.c_int32 * 6)(*[T.s32(v) for v in c["tri"]])
        lib.tpw_obj_ride_map.argtypes = [C.POINTER(C.c_int32)]
        lib.tpw_obj_ride_map(tri)
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    labels = {1: base, 2: obj, 3: ptr(2), 4: ptr(3)}
    for k in range(MISC):
        labels[LAB_MISC + k] = ptr(LAB_MISC + k)
    CL.host_begin(lib, labels, c["q"])
    lib.tpw_obj_call.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32]
    try:
        lib.tpw_obj_call(op, c.get("map", 0), c.get("type", 0), c.get("rot", 0), c.get("b", 0), c.get("state", 0))
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * 3)()
    lib.tpw_obj_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_obj_globals(state)
    out = report(C.string_at(obj, OBJ_SIZE), img, op, True,
                 ctl_report([C.string_at(ptr(LAB_MISC + CTL + i), kBlock) for i in range(3)], True))
    out["people"] = people_report([C.string_at(ptr(LAB_MISC + PERSON + i), kBlock) for i in range(3)], c, True)
    out.update(globals=list(state), log=log)
    return out


class ObjectTest(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except T.ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if p["untouched"] or h["untouched"]:
            errors.append("a byte outside the written table of the object changed")
        if difference:
            errors.append(difference)
        return errors


# ---------------------------------------------------------------- cases

EDGE32 = [0, 1, -1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x1FF, 0x7FFFFFFF, -0x80000000, 0x12345678, 100, 101, 99]
IDS = [0, 1, 0x1234, 0x7FFF, 0x8000, 0xFFFF]
STATES = [-1, 0, 1, 2, 3, 4, 5, 6, 0x7FFFFFFF, -0x80000000, 0x100, 0xFFFF, -2]


def pick(r, values):
    return r.choice(values) if r.random() < 0.6 else r.getrandbits(32)


def mk_ctor(r, **kw):
    ride = {slot: T.s32(pick(r, EDGE32)) for slot in range(1, LAST_SLOT + 1) if slot != S_NAME}
    for slot in FLAG_SLOTS:
        ride[slot] = r.choice([0, 0, 1, -1, 2, 0x100, 0x1000000, 0x7FFFFFFF, -0x80000000])
    ride[S_NAME] = 0
    c = {"op": 0, "seed": r.getrandbits(32), "fill": r.choice(FILLS), "map": r.getrandbits(16), "mapg": r.getrandbits(16),
         "type": r.getrandbits(16), "typeg": r.getrandbits(16), "rot": T.s32(pick(r, EDGE32)), "b": T.s32(pick(r, EDGE32)),
         "ride": ride, "name": r.choice(["", "A", "Big Wheel", "x" * 0x1F, "y" * 0x20, "z" * 0x21, "w" * 0x70]),
         "tri": [T.s32(pick(r, EDGE32)) for _ in range(6)], "centre": r.randrange(2), "turn": r.getrandbits(32),
         "ctl": [(r.choice([0, 1, MASK, r.getrandbits(32)]), r.choice([0, 0, 1, MASK, r.getrandbits(32)])) for _ in range(3)],
         "anim": [-1] * 16, "things": {}, "person": [0, 0, 0],
         "q": {"cx": [r.randrange(256), r.randrange(256)], "cy": [r.randrange(256)], "id": [r.choice(IDS) for _ in range(4)],
               "rx": [T.s32(pick(r, EDGE32)) for _ in range(3)], "ry": [T.s32(pick(r, EDGE32)) for _ in range(3)],
               "rf": [r.randrange(2) for _ in range(3)], "bought": [r.randrange(2)],
               "funny": [s64(r.choice([0, 1, MASK, 1 << 32, (1 << 63) - 1, -(1 << 63), r.getrandbits(64)]))],
               "ctl": [r.randrange(3) for _ in range(3)], "newnull": [r.randrange(2) for _ in range(3)],
               "clear_sub": [r.randrange(2)], "clear_msg": [r.randrange(2)]}}
    c.update(kw)
    return c


def ctor_edges():
    r = random.Random(12600)
    out = [mk_ctor(r, fill=f) for f in FILLS]
    zero = {slot: 0 for slot in range(1, LAST_SLOT + 1)}

    def with_ride(**kw):
        base = mk_ctor(r, fill=0)
        base["ride"] = dict(zero)
        base["ride"].update(kw.pop("ride", {}))
        base["q"].update(kw.pop("q", {}))
        base.update(kw)
        return base
    out.append(with_ride())
    for slot in FLAG_SLOTS:
        for centre in (0, 1):
            out.append(with_ride(ride={slot: 1}, centre=centre))
    out.append(with_ride(ride={s: 1 for s in FLAG_SLOTS}))
    out.append(with_ride(ride={s: -1 for s in FLAG_SLOTS}))
    for value in (-0x80000000, -1, 0, 1, 0x7FFFFFFF):
        for bought in (0, 1):
            out.append(with_ride(ride={S_GOLDEN: value, S_UPGRADE: 0x1234, S_GOODS: 7}, q={"bought": [bought]}))
    for slot in (S_SPEED, S_CAPACITY, S_DURATION):
        for value in (-0x80000000, -1, 0, 1, 0xFF, 0x100, 0x1FF, 0x7FFFFFFF):
            out.append(with_ride(ride={slot: value}))
    for value in (-5, 0, 1, 100, 101, 0x7FFFFFFF, -0x80000000):
        out.append(with_ride(ride={S_CHANCE: value, S_PRICE: value, S_GOODS: value, S_UPGRADE: value}))
    for turn in (0, 1, MASK):
        for w1c in (0, 1, MASK):
            out.append(with_ride(turn=turn, ctl=[(5, w1c), (6, 7), (8, 9)]))
            out.append(with_ride(turn=turn, ctl=[(MASK, w1c), (6, 7), (8, 9)], q={"ctl": [0, 0, 0]}))
    for ids in ([0], [0xFFFF], [1, 2, 3]):
        out.append(with_ride(q={"id": ids}))
    for fills in FILLS:
        for centre in (0, 1):
            for nulls in ([0], [1, 0], [1, 1, 0], [1, 1, 1]):
                for clears in ([0], [1]):
                    out.append(with_ride(fill=fills, centre=centre, q={"newnull": nulls, "clear_sub": clears, "clear_msg": clears}))
    for tag in (0, 1, 0x7F, 0x80, 0x7FFF, 0xFFFF):
        out.append(with_ride(map=tag, type=tag, q={"cx": [0xFF], "cy": [0xFF]}))
    for rot in (0, 1, 90, -90, 0x7FFFFFFF, -0x80000000):
        out.append(with_ride(rot=rot, b=-rot if rot != -0x80000000 else 5))
    for x, y in ((0, 0), (1, 0), (0, 1), (-1, -1), (0x7FFFFFFF, 1), (-0x80000000, 0x7FFFFFFF), (200, 3)):
        out.append(with_ride(q={"rx": [x, y, 3], "ry": [y, x, -3]}))
    return out


def mk_state(r, **kw):
    queued = r.choice([0, 1, 2, 0x27FF, r.randrange(0x2800)])
    var = r.choice([queued, queued, (queued + 1) & MASK, 0, MASK, r.getrandbits(32), queued | 0x10000])
    state = r.choice(STATES)
    persons = [r.choice([0xE, 0xE, 0, 1, 0xD, 0xE0000, 0xE000000, MASK, r.getrandbits(32)]) for _ in range(3)]
    person = r.randrange(3)
    c = {"op": 1, "seed": r.getrandbits(32), "fill": r.choice(FILLS), "state": state, "centre": r.randrange(2),
         "fields": [r.getrandbits(16), r.choice([0, 1, 3, 15, r.getrandbits(32)]), r.getrandbits(32), queued,
                    r.choice([0, 1, MASK, r.getrandbits(32)]), r.choice([0, 1, 0xFFFF, r.getrandbits(16)]),
                    r.choice([0, 1, 2, 3, 4, r.getrandbits(32)])],
         "things": {0: r.choice([-1, -1, 0, 1]), queued: person if var == queued and r.random() < 0.3 else person},
         "turn": 0, "ctl": [(0, 0)] * 3, "anim": [r.choice([-1, 0, 1, 2, 3]) for _ in range(16)], "person": persons,
         "q": {"id": [r.choice(IDS) for _ in range(3)], "var": [var], "scr": [r.randrange(2), r.randrange(2)],
               "newnull": [r.randrange(2) for _ in range(3)], "clear_msg": [r.randrange(2)]}}
    c["fields"][1] = r.randrange(16)
    if queued == 0:
        c["things"] = {0: c["things"][0]}
        c["things"][0] = c["things"][0]
    if var != queued and c["things"].get(queued, -1) < 0:
        c["things"][queued] = person
    c.update(kw)
    return c


def state_edges():
    r = random.Random(12601)
    out = [mk_state(r, fill=f) for f in FILLS]
    for state in STATES:
        for _ in range(6):
            out.append(mk_state(r, state=state))
    for state in (1, 2, 4):
        for var_same in (0, 1):
            for queued in (0, 1, 0x27FF):
                for person_word in (0xE, 0xD, 0):
                    for row0 in (-1, 0):
                        for centre in (0, 1):
                            c = mk_state(r, state=state, centre=centre)
                            var = queued if var_same else queued + 1
                            c["fields"][3] = queued
                            c["q"]["var"] = [var]
                            c["person"] = [person_word] * 3
                            c["things"] = {0: row0, queued: 1} if queued else {0: row0}
                            if queued == 0 and not var_same:
                                c["things"] = {0: -1}
                                c["q"]["var"] = [queued]   # person 0 would be a null pointer: only the equal path
                            out.append(c)
        for nulls in ([0], [1, 0], [1, 1, 0], [1, 1, 1]):
            for clears in (0, 1):
                c = mk_state(r, state=state, centre=0)
                c["q"]["newnull"] = nulls
                c["q"]["clear_msg"] = [clears]
                out.append(c)
    for idx in range(4):
        for slot in (0, 1, 2, 3, 0xFFFF, 0x80000000, 0x7FFFFFFF, MASK):
            c = mk_state(r, state=r.choice([1, 2, 4]))
            c["fields"][1] = idx
            c["fields"][2] = slot
            out.append(c)
    return out


TESTS = [ObjectTest(NAMES[0], ppc_case, host_case, edge=ctor_edges, rnd=lambda r: mk_ctor(r)),
         ObjectTest(NAMES[1], ppc_case, host_case, edge=state_edges, rnd=lambda r: mk_state(r))]
