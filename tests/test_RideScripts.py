"""Unit w132: the ride scripts. RSSE_Tick (0x100d33dc), RSSE_RenderTick (0x100d3674), RSSE_Getvar (0x100d6c58), RSSE_GetScriptPointer (0x100d668c),
RSSE_SetReverbOnEax (0x100d6e7c), SCR_ScriptTick (0x100d907c), SCR_CloseScriptSystem (0x100d8e4c), SCR_BuildScriptSpriteList (0x100d9184) and the ride
object's CObject::UpdateTurn (0x1010d634), ModelState (0x1010daa0), CreateRideScript (0x101076b0) and UpdateEngine (0x10107d38), against the original
PowerPC code.

Every callee outside the unit is a recording fake on both sides (the interpreter and the ticks of the script's things, the kill, the fly, coaster and
animation setters, the script table's interpreter, the sprite list, the clock, the object's wear, breakdown and queue members, the thing id, the ride
record and the ride instancing, the file lookup, the trace and the assertions). The twelve functions run as the original, so UpdateTurn and ModelState reach
the real RSSE_GetScriptPointer and RSSE_Getvar. The script records and the script table are built on both sides from the same images: on the Mac with
real addresses, on the host in the arena (their links are 32-bit offsets). A script's written fields compare by value; every other byte of a record (its
three links aside), of a table entry and of the object must keep its fill.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideScripts"
NAMES = ["RSSE_Tick", "RSSE_RenderTick", "RSSE_Getvar", "RSSE_GetScriptPointer", "RSSE_SetReverbOnEax", "SCR_ScriptTick", "SCR_CloseScriptSystem",
         "SCR_BuildScriptSpriteList", "CObject::UpdateTurn", "CObject::ModelState", "CObject::CreateRideScript", "CObject::UpdateEngine"]
ADDR = [0x100D33DC, 0x100D3674, 0x100D6C58, 0x100D668C, 0x100D6E7C, 0x100D907C, 0x100D8E4C, 0x100D9184, 0x1010D634, 0x1010DAA0, 0x101076B0, 0x10107D38]
OWN = set(ADDR)
MASK = 0xFFFFFFFF
KEEP = 0x5A5A5A5A
STATE, F04, F7BE0, F7BE4 = 0x102F7BEC, 0x102F7BF0, 0x102F7BE0, 0x102F7BE4
G_TICKTOTAL, G_TICKTIME, G_TABSIZE, G_TABLE, G_SCRINIT = 0x102F7C08, 0x102F7C0C, 0x102F7C14, 0x102F7C18, 0x102F7C28
G_WORLD, G_MAP, G_THINGS, G_ANIM, G_RIDEDB = 0x1037EC0C, 0x1037EC08, 0x1034CBFC, 0x102C79A4, 0x1037F6C8
G_RECORDER, G_BALANCE = 0x1037EC44 + 0x24, 0x102B4568
WORLD_TAIL = 0x1DA6F0
OBJ_SIZE = 0x448 + 0x40
SCRIPT_SIZE, ENTRY_SIZE, BLOCK = 0xF4, 0x118, 0x280
MISC = 24
SLOT_REVERB, SLOT_OFFX, SLOT_OFFZ = 465, 339, 340
S_SHAPE, S_OVX, S_OVZ, S_OVW, S_OVH, S_DONT = 3, 5, 6, 7, 8, 11
LAB_BASE, LAB_OBJ, LAB_RIDE, LAB_SCRIPT, LAB_MISC, LAB_TABLE, LAB_ENTRY = 1, 2, 4, 20, 40, 70, 80
ROW_ZERO = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"

# The object's fields a case sets (offset, width, key) and the ones the functions write; the hash covers the written ones and the quality (0x40).
OBJ_IN = [(0x04, 1, "cx"), (0x06, 1, "cy"), (0x0E, 2, "type"), (0x10, 4, "rot"), (0x1C, 4, "f1c"), (0x20, 4, "f20"), (0x2E, 2, "flags"), (0x30, 2, "f30"),
          (0x38, 2, "queued"), (0x40, 4, "f40"), (0x44, 4, "f44"), (0x64, 4, "f64"), (0x198, 4, "state")]
OBJ_OUT = [(0x1C, 4), (0x20, 4), (0x38, 2), (0x44, 4)]
OBJ_SNAP = [(0x1C, 4), (0x20, 4), (0x38, 2), (0x40, 4), (0x44, 4)]
# The script record's written fields (state, countdown, speed) and the pointer words that differ between the sides.
SCRIPT_OUT = [(0x3C, 4), (0x98, 4), (0xC0, 2)]
SCRIPT_SKIP = set(range(0, 8)) | set(range(0x1C, 0x20))
ENTRY_OUT = [(0x7C, 4)]


def s64(v):
    v &= 0xFFFFFFFFFFFFFFFF
    return v - (1 << 64) if v & (1 << 63) else v


def pk(value, width, little):
    return (value & ((1 << (8 * width)) - 1)).to_bytes(width, "little" if little else "big")


def hash_values(values):
    h = 2166136261
    for v in values:
        h = ((h ^ v) * 16777619) & MASK
    return h


def filler(seed, size):
    return bytearray(random.Random(seed).randbytes(size))


# ---------------------------------------------------------------- the images both sides are built from

def script_image(s, little):
    buf = filler(s["seed"], SCRIPT_SIZE)
    for o in (0, 4, 0x1C):
        buf[o:o + 4] = b"\0" * 4
    for o, w, k in [(0x08, 4, "handle"), (0x0C, 4, "linked"), (0x10, 4, "f10"), (0x18, 4, "code"), (0x3C, 4, "state"), (0x8C, 4, "vcount"),
                    (0x94, 4, "instr"), (0x98, 4, "countdown"), (0x9C, 4, "fly"), (0xAC, 2, "thing"), (0xB8, 1, "every"), (0xC0, 2, "speed"),
                    (0xC8, 4, "anim"), (0xD8, 4, "reverb"), (0xE0, 4, "coaster"), (0xE4, 2, "factor")]:
        buf[o:o + w] = pk(s[k], w, little)
    return bytes(buf)


def entry_image(e, little):
    buf = filler(e["seed"], ENTRY_SIZE)
    buf[0x18:0x1C] = pk(e["state"], 4, little)
    buf[0x7C:0x80] = pk(e["next"], 4, little)
    buf[0x80:0x84] = pk(e["period"], 4, little)
    return bytes(buf)


def block_image(c, k, little):
    buf = filler(c["seed"] + 1000 + k, BLOCK)
    buf[0x24:0x28] = pk(c["w24"].get(k, 0), 4, little)
    buf[0x220:0x224] = pk(c["w220"].get(k, 0), 4, little)
    return bytes(buf)


def obj_image(c, little):
    buf = filler(c["seed"] + 7, OBJ_SIZE)
    for o, w, k in OBJ_IN:
        buf[o:o + w] = pk(c["obj"][k], w, little)
    return bytes(buf)


def script_report(buf, img):
    values = [int.from_bytes(buf[o:o + w], BYTEORDER[0]) for o, w in SCRIPT_OUT]
    covered = SCRIPT_SKIP | {i for o, w in SCRIPT_OUT for i in range(o, o + w)}
    return {"fields": values, "untouched": [(i, buf[i]) for i in range(SCRIPT_SIZE) if i not in covered and buf[i] != img[i]]}


def entry_report(buf, img):
    covered = {i for o, w in ENTRY_OUT for i in range(o, o + w)}
    return {"fields": [int.from_bytes(buf[0x7C:0x80], BYTEORDER[0])],
            "untouched": [(i, buf[i]) for i in range(ENTRY_SIZE) if i not in covered and buf[i] != img[i]]}


def obj_report(buf, img):
    values = [int.from_bytes(buf[o:o + w], BYTEORDER[0]) for o, w in OBJ_OUT]
    covered = {i for o, w in OBJ_OUT for i in range(o, o + w)}
    return {"fields": values, "untouched": [(i, buf[i]) for i in range(OBJ_SIZE) if i not in covered and buf[i] != img[i]]}


BYTEORDER = ["big"]   # the reports decode with the side's order (set by each side before it reports)


def used_ids(c):
    ids = {0, c["obj"]["queued"]} | {s["thing"] for s in c["scripts"] if s["thing"]}
    return ids


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------- the PowerPC side

def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def ppc_case(e, c, _unused=None):
    BYTEORDER[0] = "big"
    op = c["op"]
    L = CL.PpcLog({}, c["q"])
    e.cl = L
    img = obj_image(c, False)
    obj = e.alloc(OBJ_SIZE, img)
    tail = e.alloc(0x100, b"\xa5" * 0x100)
    base = tail - WORLD_TAIL
    if base <= 0:
        raise RuntimeError("the world base would be below zero")
    ride = e.alloc(0x800, b"\0" * 0x800)
    misc = [e.alloc(BLOCK, block_image(c, k, False)) for k in range(MISC)]
    name = b"".join(struct.pack(">H", ord(ch)) for ch in c["fname"]) + b"\0\0"
    fname = e.alloc(0x200, name)
    scripts, images = [], []
    for s in c["scripts"]:
        image = script_image(s, False)
        addr = e.alloc(SCRIPT_SIZE, image)
        vals = [v & MASK for v in s["vars"][:max(s["vcount"], 0)]]
        vaddr = e.alloc(4 * max(len(vals), 1), struct.pack(">%dI" % len(vals), *vals) if vals else b"\0\0\0\0")
        scripts.append((addr, vaddr))
        images.append(image)
    for i, (addr, vaddr) in enumerate(scripts):
        w32(e, addr, scripts[i + 1][0] if i + 1 < len(scripts) else 0)
        w32(e, addr + 4, scripts[i - 1][0] if i else 0)
        w32(e, addr + 0x1C, vaddr)
    w32(e, STATE + 0x10, scripts[0][0] if scripts else 0)
    tab = c["tab"]
    table = 0 if tab["null"] else e.alloc(4 * max(tab["size"], 1), b"\0" * (4 * max(tab["size"], 1)))
    entries, entry_images = {}, {}
    for i, ent in sorted(tab["entries"].items()):
        entry_images[i] = entry_image(ent, False)
        entries[i] = e.alloc(ENTRY_SIZE, entry_images[i])
        w32(e, table + 4 * i, entries[i])
    L.labels.update({base: 1, obj: LAB_OBJ, ride: LAB_RIDE})
    if table:
        L.labels[table] = LAB_TABLE
    for i, (addr, _v) in enumerate(scripts):
        L.labels[addr] = LAB_SCRIPT + i
    for k, a in enumerate(misc):
        L.labels[a] = LAB_MISC + k
    for i, a in entries.items():
        L.labels[a] = LAB_ENTRY + i
    gi = c["g"]
    w32(e, STATE, gi[0])
    w32(e, F04, gi[1])
    w32(e, F7BE4, gi[2])
    w32(e, G_TICKTOTAL, gi[3])
    w32(e, G_TICKTIME, gi[4])
    w32(e, G_TABSIZE, tab["size"])
    w32(e, G_TABLE, table)
    w32(e, G_SCRINIT, gi[7])
    w32(e, F7BE0, gi[8])
    for slot, v in c["bal"].items():
        w32(e, G_BALANCE + 8 + 4 * slot, v)
    w32(e, G_RECORDER, c["rec"])
    w32(e, G_WORLD, base)
    w32(e, G_MAP, misc[0])
    w32(e, tail + 0x1C, c["turn"])
    for i in range(16):
        w32(e, G_ANIM + 4 * i, misc[c["anim"][i]] if c["anim"][i] >= 0 else 0)
    for index, block in c["things"].items():
        w32(e, G_THINGS + 0x14 * index, misc[block] if block >= 0 else 0)
    for slot, v in c["ride"].items():
        w32(e, ride + 0xC + 4 * slot, v)
    w32(e, ride + 0xC + 4 * S_SHAPE, misc[c["shape"]] if c["shape"] >= 0 else 0)
    e.write(ride + 0x28C, c["folder"].encode("latin-1") + b"\0")
    w32(e, ride + 0x78C, c["number"])
    pi_calls = [0]

    def snap():
        return hash_values(int.from_bytes(e.read(obj + o, w), "big") for o, w in OBJ_SNAP)

    def s32at(a):
        return T.s32(e.u32(a))

    def trace(emu):
        fmt = emu.cstr(emu.r(4))
        ints, texts, g = [], [], 5
        i = 0
        while i < len(fmt):
            if fmt[i] == "%" and i + 1 < len(fmt):
                kind = fmt[i + 1]
                if kind == "d":
                    ints.append(T.s32(emu.r(g)))
                    g += 1
                elif kind == "s":
                    p = emu.r(g)
                    g += 1
                    texts.append(emu.cstr(p)[:159] if p else "(null)")
                i += 1
            i += 1
        ints, texts = (ints + [0, 0])[:2], (texts + ["", ""])[:2]
        L.rec("CTRACE", [emu.r(3), ints[0], ints[1]], [fmt[:159], texts[0], texts[1]])

    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [T.s32(emu.r(3))], [emu.cstr(emu.r(4))[:159]])

    def process_instruction(emu):
        s = emu.r(3)
        countdown, state = s32at(s + 0x98), s32at(s + 0x3C)
        pi_calls[0] += 1
        flag = L.ret("pi_flag", 0) & MASK
        w32(e, F7BE4, flag)
        nxt = L.ret("pi_state", KEEP)
        if nxt != KEEP:
            w32(e, s + 0x3C, nxt)
        if pi_calls[0] > 40:
            w32(e, s + 0x3C, -1)
        L.rec("ProcessInstruction", [L.lab(s), countdown, state, flag, s32at(s + 0x3C)])

    def process_objects(emu):
        s = emu.r(3)
        state = s32at(s + 0x3C)
        nxt = L.ret("po_state", KEEP)
        if nxt != KEEP:
            w32(e, s + 0x3C, nxt)
        L.rec("ProcessObjects", [L.lab(s), state, s32at(s + 0x98), s32at(s + 0x3C)])

    def plain(name, spec="", result=None):
        return CL.recorder(name, spec, ret=result)

    def init_script(emu):
        L.rec("RSSE_InitScript", [emu.r(4) >> 16, T.s32(emu.r(5))], [emu.cstr(emu.r(3))[:159]])
        emu.set_r(3, T.u32(L.ret("init", 0)))

    def anim_speed(emu):
        L.rec("UpdateRideAnimSpeedAllChannels", [L.lab(emu.r(3)), CL.fbits(emu.f(1))])

    def first_person(emu):
        L.rec("GetFirstPersonRideID")
        emu.set_r(3, T.u32(L.ret("fpr", 0)))

    def do_script(emu):
        s = emu.r(3)
        L.rec("DoScript", [L.lab(s), s32at(s + 0x18), s32at(s + 0x7C), s32at(s + 0x80)])
        emu.set_r(3, T.u32(L.ret("ds", 0)))

    def clock(emu):
        L.rec("Clock")
        emu.set_r(3, T.u32(L.ret("clock", 0)))

    def free(emu):
        L.rec("LbMem_Free", [L.lab(emu.r(3))])

    def thing_id(emu):
        ident = L.ret("id", 0x1234)
        L.rec("GetThingId", [L.lab(emu.r(4)), snap()])
        emu.write(emu.r(3), struct.pack(">H", ident & 0xFFFF))

    def in_queueing(emu):
        L.rec("InQueueingState", [L.lab(emu.r(3)), snap()])
        emu.set_r(3, T.u32(L.ret("inq", 0)))

    def member(name, shape=""):
        def stub(emu):
            ints = [L.lab(emu.r(3))]
            if shape == "w":
                ints.append(emu.r(4) >> 16)
            L.rec(name, ints + [snap()])
        return stub

    def file_name(emu):
        L.rec("GetFileName", [L.lab(emu.r(3)), snap()])
        emu.set_r(3, fname)

    def db_info(emu):
        L.rec("GetDBInfo", [L.lab(emu.r(3)), snap()])
        emu.set_r(3, ride)

    def effector(emu):
        L.rec("RemoveRegionalEffector", [L.lab(emu.r(3)), T.s32(emu.r(4)), emu.r(5) >> 16, snap()])

    def ride_info(emu):
        L.rec("GetRideInfo", [int(emu.r(3) == G_RIDEDB), emu.r(4) >> 16, snap()])
        emu.set_r(3, ride if L.ret("ri", 1) != 0 else 0)

    def find_file(emu):
        L.rec("FindFilePath", [emu.r(4), int(emu.r(5) != 0)], [emu.cstr(emu.r(3))[:159]])
        if L.ret("ffp", 1) == 0:
            emu.set_r(3, 0)
            return
        text = ("found:" + emu.cstr(emu.r(3))).encode("latin-1") + b"\0"
        emu.set_r(3, emu.alloc(len(text), text))

    def instance(emu):
        p = emu.r(5)
        sp = emu.r(1)
        L.rec("InstanceRide", [emu.r(3), emu.r(4), s32at(p), s32at(p + 4), s32at(p + 8), L.lab(e.u32(p + 12)), T.s32(emu.r(7)), L.lab(emu.r(8)),
                               T.s32(emu.r(9)), T.s32(emu.r(10)), e.u32(sp + 0x38), s32at(sp + 0x3C)])
        emu.set_r(3, T.u32(L.ret("inst", 0)))

    def restore(emu):
        p = emu.r(6)
        sp = emu.r(1)
        L.rec("RestoreRideInstance", [emu.r(3), emu.r(4), emu.r(5), s32at(p), s32at(p + 4), s32at(p + 8), L.lab(e.u32(p + 12)), emu.r(7),
                                      L.lab(emu.r(8)), T.s32(emu.r(9)), T.s32(emu.r(10)), s32at(sp + 0x38)])
        emu.set_r(3, T.u32(L.ret("rest", 0)))

    def sprintf(emu):
        fmt = emu.cstr(emu.r(4))
        if fmt != "%s:%ls%s":
            L.rec("sprintf_format", [], [fmt[:159]])
        text = emu.cstr(emu.r(5)) + ":" + CL.wide_text(emu, emu.r(6)) + emu.cstr(emu.r(7))
        emu.write(emu.r(3), text.encode("latin-1") + b"\0")
        emu.set_r(3, emu.r(3))

    S = CL.Stubs()
    S.real = OWN
    for addr, fn in [
        (0x100CFDF4, process_instruction), (0x100CF884, process_objects),
        (0x100CE740, plain("BOUNCE_Tick", "L")), (0x100CD928, plain("WALKNODE_Tick", "L")), (0x100D42D0, plain("RSSE_KillScript", "ii")),
        (0x100C7AE4, anim_speed), (0x10078958, plain("FLY_SetRunTime", "ii")), (0x100789EC, plain("FLY_SetRunSpeed", "ii")),
        (0x10049630, plain("CoasterSetSpeed", "uu")), (0x10026C98, plain("BMP_SetRidePerformance", "ii")), (0x10032B64, first_person),
        (0x100DE97C, plain("SOUND_SetReverbLevel", "i")), (0x1000DDE8, assertion), (0x1006FCBC, trace),
        (0x100D6734, plain("RSSE_Setvar", "Lii")), (0x100D37B4, init_script), (0x100D88B4, do_script), (0x100D9478, plain("SCR_KillScript", "i")),
        (0x100D900C, plain("SCR_CloseCheckTable")), (0x1007AF24, plain("ResetHLSprites")), (0x1007B02C, plain("DrawHLSprites")),
        (0x1007CA9C, plain("ESP_AddHLSpriteFromScript", "L")), (0x101472F4, clock),
        (0x1010176C, effector), (0x1010B244, member("AddWearAndTear")), (0x1010D90C, member("RequestBreakdown")),
        (0x1010D994, member("SetWornFlag")), (0x1010EB44, member("ProcessStateFunctioning")), (0x1010EA00, member("LetPeopleOff")),
        (0x1011062C, file_name), (0x10107CD4, db_info), (0x10108C20, member("RemovePersonFromQueue", "w")),
        (0x1011DF70, member("ForceRideAdmittance")), (0x1011F1B8, member("DismissFromQueue")), (0x10120B2C, in_queueing),
        (0x1012E1F8, thing_id), (0x10154868, ride_info), (0x10069B24, instance), (0x1006A140, restore),
        (0x101EEFD0, plain("SetRideScriptID", "uu")), (0x10044B10, plain("HACKRestoreCoasterQueues", "u")), (0x100597F8, find_file)]:
        S.internal(addr, fn)
    S.imp("LbMem_Free__FPv", free)
    S.imp("sprintf", sprintf)
    saved = S.activate(e)
    try:
        if op == 2:
            args = [scripts[c["a"]][0] if c["a"] >= 0 else 0, c["b"] & MASK]
        elif op == 3:
            args = [c["a"] & MASK]
        elif op == 11:
            args = [obj, c["a"] & MASK]
        elif op >= 8:
            args = [obj]
        else:
            args = []
        r3 = T.call_ppc(e, ADDR[op], args, max_insns=50_000_000, timeout_us=60_000_000)[0]
        if op in (0, 2):
            answer = T.s32(r3)
        elif op == 3:
            answer = L.lab(r3 & MASK)
        else:
            answer = 0
        out = {"answer": answer,
               "scripts": [script_report(e.read(a, SCRIPT_SIZE), images[i]) for i, (a, _v) in enumerate(scripts)],
               "entries": {i: entry_report(e.read(a, ENTRY_SIZE), entry_images[i]) for i, a in entries.items()},
               "obj": obj_report(e.read(obj, OBJ_SIZE), img),
               "globals": [e.u32(STATE), e.u32(F04), e.u32(F7BE4), e.u32(G_TICKTOTAL), e.u32(G_TICKTIME), e.u32(G_TABSIZE), L.lab(e.u32(G_TABLE)) & MASK,
                           e.u32(G_SCRINIT), e.u32(F7BE0)]}
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


# ---------------------------------------------------------------- the host side

def host_case(lib, c, _unused=None):
    BYTEORDER[0] = "little"
    op = c["op"]
    lib.tpw_rs_ptr.restype = C.c_void_p
    lib.tpw_rs_ptr.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rs_call.restype = C.c_int64
    lib.tpw_rs_call.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_rs_script.argtypes = [C.c_char_p, C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_rs_entry.argtypes = [C.c_int32, C.c_char_p]
    lib.tpw_rs_table.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rs_set_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rs_get_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rs_fields.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    for f in ("tpw_rs_thing", "tpw_rs_anim", "tpw_rs_ride_value", "tpw_rs_balance"):
        getattr(lib, f).argtypes = [C.c_int32, C.c_int32]
    for f in ("tpw_rs_ride_shape", "tpw_rs_ride_number", "tpw_rs_recorder"):
        getattr(lib, f).argtypes = [C.c_int32]
    for f in ("tpw_rs_ride_folder", "tpw_rs_file_name"):
        getattr(lib, f).argtypes = [C.c_char_p]
    lib.tpw_rs_reset()

    def ptr(kind, i=0):
        return lib.tpw_rs_ptr(kind, i)

    img = obj_image(c, True)
    C.memmove(ptr(2), img, OBJ_SIZE)
    table = (C.c_int32 * (2 * len(OBJ_SNAP)))(*[v for pair in OBJ_SNAP for v in pair])
    lib.tpw_rs_fields(table, len(OBJ_SNAP))
    C.memmove(ptr(3), b"\xa5" * 0x100, 0x100)
    C.memmove(ptr(3) + 0x1C, struct.pack("<I", c["turn"] & MASK), 4)
    for k in range(MISC):
        C.memmove(ptr(1, k), block_image(c, k, True), BLOCK)
    images = []
    for s in c["scripts"]:
        image = script_image(s, True)
        vals = [v - (1 << 32) if v & (1 << 31) else v for v in (v & MASK for v in s["vars"][:max(s["vcount"], 0)])]
        arr = (C.c_int32 * max(len(vals), 1))(*vals)
        lib.tpw_rs_script(image, arr, len(vals))
        images.append(image)
    tab = c["tab"]
    lib.tpw_rs_table(tab["size"], 1 if tab["null"] else 0)
    entry_images = {}
    for i, ent in sorted(tab["entries"].items()):
        entry_images[i] = entry_image(ent, True)
        lib.tpw_rs_entry(i, entry_images[i])
    for index, block in c["things"].items():
        lib.tpw_rs_thing(index, block)
    for i in range(16):
        lib.tpw_rs_anim(i, c["anim"][i])
    for slot, v in c["ride"].items():
        lib.tpw_rs_ride_value(slot, T.s32(v))
    lib.tpw_rs_ride_shape(c["shape"])
    lib.tpw_rs_ride_folder(c["folder"].encode("latin-1"))
    lib.tpw_rs_ride_number(T.s32(c["number"]))
    lib.tpw_rs_file_name(c["fname"].encode("latin-1"))
    for slot, v in c["bal"].items():
        lib.tpw_rs_balance(slot, T.s32(v))
    lib.tpw_rs_recorder(T.s32(c["rec"]))
    gi = (C.c_int64 * 9)(*[T.s32(v) for v in c["g"]])
    lib.tpw_rs_set_globals(gi)
    labels = {1: ptr(7), LAB_OBJ: ptr(2), LAB_RIDE: ptr(4), LAB_TABLE: ptr(5)}
    for i in range(len(c["scripts"])):
        labels[LAB_SCRIPT + i] = ptr(0, i)
    for k in range(MISC):
        labels[LAB_MISC + k] = ptr(1, k)
    for i in entry_images:
        labels[LAB_ENTRY + i] = ptr(6, i)
    CL.host_begin(lib, labels, c["q"])
    try:
        answer = lib.tpw_rs_call(op, c["a"], c["b"])
    finally:
        log = CL.host_end(lib)
    out_globals = (C.c_int64 * 9)()
    lib.tpw_rs_get_globals(out_globals)
    out = {"answer": T.s32(answer) if op in (0, 2) else answer,
           "scripts": [script_report(C.string_at(ptr(0, i), SCRIPT_SIZE), images[i]) for i in range(len(images))],
           "entries": {i: entry_report(C.string_at(ptr(6, i), ENTRY_SIZE), entry_images[i]) for i in entry_images},
           "obj": obj_report(C.string_at(ptr(2), OBJ_SIZE), img),
           "globals": [v & MASK for v in out_globals], "log": log}
    return out


def has_untouched(x):
    if isinstance(x, dict):
        return bool(x.get("untouched")) or any(has_untouched(v) for k, v in x.items() if k != "untouched")
    if isinstance(x, list):
        return any(has_untouched(v) for v in x)
    return False


class RSTest(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except T.ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if has_untouched(p) or has_untouched(h):
            errors.append("a byte outside the written fields of a record, an entry or the object changed")
        if difference:
            errors.append(difference)
        return errors


# ---------------------------------------------------------------- cases

def fl(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


NAN = float("nan")
FLOATS = [256.0, -1.0, -1e10, NAN, 0.0, 100.0, 105.0, 106.0, 104.0, 5.0, 4.0, 3.5, 300.0, -300.0, 1.0, 255.0, 2.5, 512.0, -256.0, 24.0, 25.0, 24.9,
          30.0, 0.5, -0.0, 99.9, 1e-3, 65536.0, 7.0]
# Positive overflow (1e10, infinity) is left out: the emulator's fctiwz answers 0x80000000 there, the hardware 0x7fffffff (see the doubtful list).
KINDS = [0x32F, 0x361, 5, 0, 0xFFFF]
SCRIPT_OPS = {0, 1, 2, 3, 4, 8, 9}
THING_BLOCKS = list(range(1, 9))
ANIM_BLOCKS = list(range(9, 17))
SHAPE_BLOCKS = list(range(17, 24))
WORDS = [0, 1, -1, 2, 3, 5, 0x7FFFFFFF, -0x80000000, 0x12345678, 100, 0xE]


def S(r, **kw):
    s = {"seed": r.randrange(1 << 30), "handle": r.randrange(0, 12), "linked": 0, "f10": r.choice([0, 0, 0, 1, 5]),
         "code": r.choice([0, 0xC0DE, 1]), "state": r.choice([0, 0, 0, 1, 2, 5, -1, -9]), "vcount": r.choice([-1, 0, 1, 2, 3, 4, 5, 6]),
         "instr": r.choice([0, 1, 2, 3, 6]), "countdown": r.choice([0, 1, -1, 9]), "fly": r.choice([0, 0, 0, 7, -3, 0x7FFF]),
         "thing": r.choice([0, 0, 0, 1, 2, 3, 5, 0x27FF]), "every": r.choice([0, 0, 1, 0xFF]),
         "speed": r.choice([0, 1, 50, 100, -100, 32767, -32768, r.randrange(-300, 300)]), "anim": r.choice([0, 0, 0, 1, 2, 3, 15]),
         "reverb": r.choice([0, 0, 1, 2, -1, 5, -7]), "coaster": r.choice([0, 0, 0, 9, 0x80000000, MASK]),
         "factor": r.choice([0, 1, 100, -100, 1000, -1000, 32767, -32768, r.randrange(-3000, 3000)])}
    s.update(kw)
    if "vars" not in kw:
        s["vars"] = [r.choice([0, 1, 2, 3, 4, -1, 7, 0x7FFFFFFF, 0x80000000, 0x400000, r.randrange(1 << 32)]) for _ in range(max(s["vcount"], 0))]
    return s


def table(r, op):
    size = r.choice([0, 1, 2, 3, 5, 8])
    null = size <= 1 and r.random() < 0.3
    if op == 6 and r.random() < 0.15:
        null = True
    entries = {}
    if not null:
        clock = r.choice([0, 1, 6, 100, -1, 1000, 5])
        for i in range(size):
            if r.random() < 0.75:
                entries[i] = {"seed": r.randrange(1 << 30), "state": r.choice([0, 1, 4, 4, 2, -1, 3]),
                              "next": r.choice([clock - 1, clock, clock + 1, -5, 0, 5, 0x7FFFFFFF, -0x80000000, r.randrange(1 << 32)]),
                              "period": r.choice([0, 1, 10, -10, 0x7FFFFFFF, r.randrange(1 << 32)])}
    return {"size": size, "null": null, "entries": entries}


def mk_obj(r):
    return {"cx": r.randrange(256), "cy": r.randrange(256), "type": r.choice([0x32F, 0x361, 5, r.randrange(65536)]),
            "rot": r.choice([0, 1, 90, -1, r.randrange(1 << 32)]), "f1c": r.choice([0, 7, r.randrange(1 << 32)]), "f20": r.choice([0, 3, 99, MASK]),
            "f30": r.choice([0, 1, 2, 129, 130, 0x8000, r.randrange(65536)]), "flags": r.choice([0, 1, 0x80, 0x81, 0x100, 0x181, r.randrange(65536)]),
            "queued": r.choice([0, 0, 1, 2, 3, 5]), "f40": fl(r.choice(FLOATS)), "f44": fl(r.choice(FLOATS)),
            "f64": r.choice([0, 1, 0x12345, MASK]), "state": r.choice([0, 1, 2, 3, 4, 5, -1, 100])}


def used_ids(c):
    return {0, c["obj"]["queued"]} | {s["thing"] for s in c["scripts"] if s["thing"]}


def var0(c):
    """Script variable 0 of the script the object's handle names, as ModelState reads it."""
    if c["g"][0] == 0:
        return 0
    f20 = c["obj"]["f20"]
    for s in c["scripts"]:
        if s["handle"] == T.s32(f20):
            return s["vars"][0] & MASK if s["vcount"] > 0 else 0
    return 0


def fix_things(c, r):
    things = {}
    for i in sorted(used_ids(c)):
        things[i] = c["things"].get(i, r.choice([-1, 1, 2, 3, 4, 5, 6, 7, 8]))
    if c["op"] == 9 and c["obj"]["state"] in (1, 2, 4):
        q = c["obj"]["queued"]
        if var0(c) != q and things.get(q, -1) < 0:
            things[q] = r.choice(THING_BLOCKS)
    c["things"] = things


def mk(r, op, **over):
    c = {"op": op, "seed": r.randrange(1 << 30), "a": 0, "b": 0, "things": {}, "q": {}, "ride": {}, "scripts": []}
    init = 0 if r.random() < 0.1 else 1
    c["g"] = [init, r.choice([0, 1, 6, 7, 8, 9, 15, 16, -1, -8, 0x7FFFFFFE, -0x80000000, 100, r.randrange(-50, 200)]),
              r.choice([0, 1, MASK, r.randrange(1 << 32)]), r.choice([0, 5, -7, r.randrange(1 << 32)]), r.choice([0, 5, -7, r.randrange(1 << 32)]), 0, 0,
              0 if r.random() < 0.15 else 1, r.choice([0, 1, -1, 0x7FFFFFFF, r.randrange(1 << 32)])]
    c["bal"] = {SLOT_REVERB: r.choice([0, 1, 7, -3, 465]), SLOT_OFFX: r.choice([0, 3, -4, 100]), SLOT_OFFZ: r.choice([0, 6, -9, 200])}
    c["rec"] = r.choice([0, 0, 1, 2, -1])
    c["turn"] = r.choice([0, 8, 16, 7, 9, 15, 1000, 1001, r.randrange(1 << 32), (r.randrange(1 << 28) << 3)])
    c["anim"] = [r.choice([-1] + ANIM_BLOCKS) for _ in range(16)]
    c["w24"] = {k: r.choice([0, 0, 5, 0x1234, 0x80000000]) for k in range(MISC)}
    c["w220"] = {k: r.choice([0, 0xE, 0xE, 3, 0x10E]) for k in range(MISC)}
    c["shape"] = r.choice([-1] + SHAPE_BLOCKS)
    c["folder"] = r.choice(["", "Data:Rides", "folder name", "x" * 60])
    c["fname"] = r.choice(["", "ride", "Big Wheel 2", "abc.def", "y" * 40])
    c["number"] = r.choice([0, 1, 7, 0x7FFFFFFF, -1])
    c["tab"] = table(r, op) if op in (5, 6, 7) else {"size": 0, "null": True, "entries": {}}
    c["obj"] = mk_obj(r)
    if op in SCRIPT_OPS:
        n = r.choice([0, 1, 1, 2, 3, 4])
        pool = r.sample(range(12), n) if r.random() < 0.9 else [r.randrange(12) for _ in range(n)]
        c["scripts"] = [S(r, handle=h) for h in pool]
        for s in c["scripts"]:
            if c["scripts"] and r.random() < 0.3:
                s["linked"] = r.choice(c["scripts"])["handle"]
        if c["scripts"] and r.random() < 0.8:
            c["obj"]["f20"] = r.choice(c["scripts"])["handle"] & MASK
        for s in c["scripts"]:
            s["anim"] = s["anim"] if op != 4 else s["anim"]
    q = c["q"]
    if op == 0:
        q["pi_flag"] = r.choice([[0], [0], [0], [1], [0, 1, 0, 0], [2, 0], [MASK]])
        if r.random() < 0.4:
            q["pi_state"] = r.choice([[-1], [1, -1], [0, 2, -3], [4]])
        if r.random() < 0.4:
            q["po_state"] = r.choice([[-1], [0, -1], [3], [-2, 1]])
    elif op == 4:
        ids = [s["anim"] for s in c["scripts"]]
        q["fpr"] = [r.choice(ids) if ids and r.random() < 0.6 else r.choice([0, 1, 2, 3, 15])]
    elif op == 5:
        q["clock"] = [r.choice([0, 1, 6, 100, -1, 1000, 5, 0x7FFFFFFF])]
        q["ds"] = r.choice([[0], [5], [-3], [0x7FFFFFFF], [1, 2, 3]])
    elif op in (8, 9):
        q["inq"] = r.choice([[0], [1], [0, 1], [5]])
        q["id"] = r.choice([[0x1234], [1], [7, 9], [0], [0xFFFF]])
    elif op == 10:
        q["id"] = r.choice([[0x1234], [1], [0], [0xFFFF], [0x8001]])
        q["ffp"] = r.choice([[1], [0, 1], [1, 0], [0, 1]])
        q["init"] = r.choice([[0], [5], [-1], [0x7FFFFFFF]])
    elif op == 11:
        q["ri"] = r.choice([[1], [1], [1], [0]])
        q["inst"] = r.choice([[0], [7], [-1], [0x80000000]])
        q["rest"] = r.choice([[0], [3]])
        c["a"] = r.choice([0, 1, -1, 5, 0x80000000])
        for slot in (S_OVX, S_OVZ, S_OVW, S_OVH, S_DONT):
            c["ride"][slot] = r.choice([0, 0, 1, -3, 12, MASK])
    if op == 2:
        c["a"] = r.randrange(-1, len(c["scripts"]) + 1) if c["scripts"] else -1
        c["a"] = min(c["a"], len(c["scripts"]) - 1)
        s = c["scripts"][c["a"]] if c["a"] >= 0 else None
        c["b"] = r.randrange(-2, (s["vcount"] if s else 2) + 3)
    elif op == 3:
        hs = [s["handle"] for s in c["scripts"]]
        c["a"] = r.choice(hs) if hs and r.random() < 0.7 else r.choice([0, 3, -1, 99, 11])
    for k, v in over.items():
        if k in ("obj", "q", "ride", "bal", "w24", "w220") and isinstance(v, dict):
            c[k].update(v)
        else:
            c[k] = v
    for s in c["scripts"]:
        if s["linked"] and s["linked"] not in {t["handle"] for t in c["scripts"]}:
            s["linked"] = 0
    fix_things(c, r)
    return c


R = random.Random(0x132)


def tick_edges():
    out = [mk(R, 0, g=[0, 3, 0, 0, 0, 0, 0, 1, 77]), mk(R, 0, scripts=[]), mk(R, 0, g=[1, 0x7FFFFFFE, 0, 0, 0, 0, 0, 1, -5], scripts=[S(R, handle=0, every=0)])]
    for pf in ([0], [1], [0, 1, 0], [2, 0], [MASK]):
        for ps in (None, [-1], [3, -2]):
            for po in (None, [-1]):
                q = {"pi_flag": pf}
                if ps:
                    q["pi_state"] = ps
                if po:
                    q["po_state"] = po
                out.append(mk(R, 0, q=q, scripts=[S(R, code=1, instr=3, state=0, every=1, handle=2), S(R, code=0, every=0, handle=5, state=0)]))
    for f04 in (0, 4, 6, 7, 8, -1, -8, 0x7FFFFFFE):
        out.append(mk(R, 0, g=[1, f04, 0, 0, 0, 0, 0, 1, 9], scripts=[S(R, handle=h, every=0, code=1, instr=2, state=0) for h in (0, 1, 5, 7, 8, 13, -3)]))
    for anim in (0, 1, 15):
        for speed, factor in ((50, 1000), (-300, 7), (32767, -32768), (0, 0)):
            out.append(mk(R, 0, scripts=[S(R, every=1, code=0, anim=anim, speed=speed, factor=factor, state=0, fly=0, coaster=0, thing=0)]))
    for vc in (-1, 0, 3, 4, 5):
        for var3 in (0, 1, 5, 0x400000, 0x80000000, MASK):
            out.append(mk(R, 0, scripts=[S(R, every=1, code=0, fly=7, vcount=vc, vars=[1, 2, 3, var3, 5][:max(vc, 0)], state=0, anim=0, coaster=0, thing=0)]))
    for coaster in (9, 0x80000000, MASK):
        for speed in (0, -1, 32767, -32768):
            out.append(mk(R, 0, scripts=[S(R, every=1, code=0, coaster=coaster, speed=speed, state=0, anim=0, fly=0, thing=0)]))
    for thing in (1, 2, 3):
        for row0 in (-1, 4):
            for blk in (-1, 1, 2):
                for w24 in (0, 5):
                    out.append(mk(R, 0, things={0: row0, thing: blk}, w24={1: w24, 2: w24},
                                  scripts=[S(R, every=1, code=0, thing=thing, speed=-7, state=0, anim=0, fly=0, coaster=0)]))
    for linked in (1, 2):
        out.append(mk(R, 0, scripts=[S(R, handle=1, every=1, code=0, linked=2, speed=-42, state=0, anim=0, fly=0, coaster=0, thing=0),
                                     S(R, handle=2, every=0, code=0, speed=5, state=0, anim=0, fly=0, coaster=0, thing=0)]))
        out.append(mk(R, 0, scripts=[S(R, handle=1, every=1, code=0, linked=1, speed=-42, state=0, anim=0, fly=0, coaster=0, thing=0)]))
    return out


def render_edges():
    return [mk(R, 1, g=[0, 0, 0, 0, 0, 0, 0, 0, 0]), mk(R, 1, scripts=[]), mk(R, 1, scripts=[S(R, handle=1), S(R, handle=2), S(R, handle=3)])]


def getvar_edges():
    out = [mk(R, 2, g=[0, 0, 0, 0, 0, 0, 0, 1, 0], scripts=[S(R, handle=1)], a=0, b=0), mk(R, 2, scripts=[], a=-1, b=0)]
    for vc in (-1, 0, 1, 3):
        for b in (-1, 0, 1, 2, 3, 4, 0x7FFFFFFF, -0x80000000):
            s = S(R, handle=1, vcount=vc, vars=[0x80000000, 5, MASK][:max(vc, 0)])
            out.append(mk(R, 2, scripts=[s], a=0, b=b))
    return out


def pointer_edges():
    out = [mk(R, 3, g=[0, 0, 0, 0, 0, 0, 0, 1, 0], scripts=[S(R, handle=1)], a=1)]
    for a in (0, 1, 2, 3, -1, 99):
        out.append(mk(R, 3, scripts=[S(R, handle=1), S(R, handle=2), S(R, handle=2), S(R, handle=0)], a=a))
    out.append(mk(R, 3, scripts=[], a=1))
    return out


def reverb_edges():
    out = []
    for fpr in (0, 1, 2, 9):
        for reverb in (-1, 0, 5):
            for f10 in (0, 1):
                out.append(mk(R, 4, q={"fpr": [fpr]}, scripts=[S(R, handle=1, anim=3, reverb=-4), S(R, handle=2, anim=fpr, reverb=reverb, f10=f10),
                                                              S(R, handle=3, anim=fpr, reverb=9, f10=0)]))
    for default in (0, 5, -1, 0x7FFFFFFF):
        out.append(mk(R, 4, q={"fpr": [0]}, bal={SLOT_REVERB: default}, scripts=[]))
        out.append(mk(R, 4, q={"fpr": [4]}, bal={SLOT_REVERB: default}, scripts=[S(R, anim=2)]))
    return out


def script_tick_edges():
    out = [mk(R, 5, g=[1, 0, 0, 9, 9, 0, 0, 0, 0]), mk(R, 5, g=[1, 0, 0, 9, 9, 0, 0, 1, 0], tab={"size": 0, "null": True, "entries": {}}),
           mk(R, 5, tab={"size": 1, "null": True, "entries": {}})]
    for clock in (0, 5, -3, 0x7FFFFFFF):
        for state in (0, 4, 2):
            for nxt in (clock - 1, clock, clock + 1):
                ents = {1: {"seed": 1, "state": state, "next": nxt, "period": 7}, 2: {"seed": 2, "state": 4, "next": clock - 5, "period": 3},
                        4: {"seed": 3, "state": 0, "next": 0x7FFFFFFF, "period": 0x7FFFFFFF}}
                out.append(mk(R, 5, g=[1, 0, 0, 3, 3, 0, 0, 1, 0], q={"clock": [clock], "ds": [4, 0x7FFFFFFF, 9]},
                              tab={"size": 5, "null": False, "entries": ents}))
    return out


def close_edges():
    out = [mk(R, 6, g=[1, 0, 0, 0, 0, 0, 0, 0, 0]), mk(R, 6, tab={"size": 4, "null": False, "entries": {0: {"seed": 1, "state": 0, "next": 0, "period": 0}}}),
           mk(R, 6, tab={"size": 0, "null": True, "entries": {}}, g=[1, 0, 0, 0, 0, 0, 0, 1, 0]),
           mk(R, 6, tab={"size": 3, "null": True, "entries": {}}, g=[1, 0, 0, 0, 0, 0, 0, 1, 0])]
    return out


def sprite_edges():
    ents = {1: {"seed": 1, "state": 0, "next": 0, "period": 0}, 3: {"seed": 2, "state": 4, "next": 0, "period": 0}}
    return [mk(R, 7, g=[1, 0, 0, 0, 0, 0, 0, 0, 0], tab={"size": 4, "null": False, "entries": ents}),
            mk(R, 7, g=[1, 0, 0, 0, 0, 0, 0, 1, 0], tab={"size": 4, "null": False, "entries": ents}),
            mk(R, 7, g=[1, 0, 0, 0, 0, 0, 0, 1, 0], tab={"size": 0, "null": True, "entries": {}}),
            mk(R, 7, g=[1, 0, 0, 0, 0, 0, 0, 1, 0], tab={"size": 1, "null": True, "entries": {}}),
            mk(R, 7, g=[1, 0, 0, 0, 0, 0, 0, 1, 0], tab={"size": 2, "null": False, "entries": {0: ents[1], 1: ents[1]}})]


def turn_edges():
    out = []
    handle = 4
    scr = lambda **kw: S(R, handle=handle, every=0, **kw)
    for flags in (0x80, 0x81, 0x01, 0):
        for v0 in (1, 0, 2):
            for state in (0, 1, 3, 4):
                out.append(mk(R, 8, scripts=[scr(vcount=2, vars=[v0, 9])], obj={"flags": flags, "f20": handle, "state": state, "queued": 0}))
    out.append(mk(R, 8, g=[0, 0, 0, 0, 0, 0, 0, 1, 0], scripts=[scr(vcount=2, vars=[1, 9])], obj={"flags": 0x80, "f20": handle, "state": 0}))
    out.append(mk(R, 8, scripts=[scr(vcount=0, vars=[])], obj={"flags": 0x80, "f20": handle, "state": 0}))
    out.append(mk(R, 8, scripts=[], obj={"flags": 0x80, "f20": 77, "state": 0}))
    for turn in (0, 8, 7, 9, 16, 1 << 31, MASK):
        for state in (0, 1, 2, 3, 4, 5):
            for f64 in (0, 1):
                out.append(mk(R, 8, turn=turn, scripts=[scr(vcount=5, vars=[0, 0, 0, 0, 0])],
                              obj={"state": state, "f64": f64, "f20": handle, "flags": 0, "f40": fl(256.0), "f44": fl(100.0), "queued": 0}))
    for f44 in (256.0, -1.0, -1e10, NAN, 0.0, 100.0, 105.0, 106.0, 104.0, 5.0, 4.0, 3.5, 300.0, 1.0, 255.0, 2.5, 512.0, 1e-3):
        for f40 in (256.0, 0.0, NAN, 100.0, -1e10, 512.0):
            for flags in (0, 1, 0x100):
                for v4 in (0, 1):
                    out.append(mk(R, 8, turn=16, scripts=[scr(vcount=5, vars=[0, 0, 0, 0, v4])],
                                  obj={"state": 0, "f64": 1, "f20": handle, "flags": flags, "f40": fl(f40), "f44": fl(f44), "queued": 0}))
    for f40 in (0.0, 1.0, 24.0, 24.9, 25.0, 100.0, 255.0, 256.0, NAN, -1.0, -1e10):
        for flags in (1, 0, 0x81):
            out.append(mk(R, 8, turn=3, scripts=[scr(vcount=5, vars=[0, 0, 0, 0, 0])],
                          obj={"state": 0, "f64": 1, "f20": handle, "flags": flags, "f40": fl(f40), "f44": fl(100.0), "queued": 0}))
    for queued in (0, 3):
        for blk in (-1, 1):
            for inq in (0, 1, 7):
                for row0 in (-1, 5):
                    out.append(mk(R, 8, turn=3, q={"inq": [inq]}, things={0: row0, 3: blk}, scripts=[scr(vcount=5, vars=[0, 0, 0, 0, 0])],
                                  obj={"state": r_choice(0, 3), "queued": queued, "f64": 1, "f20": handle, "flags": 0}))
    return out


def r_choice(a, b):
    return R.choice([a, b])


def model_edges():
    out = []
    handle = 4
    for state in (0, 1, 2, 3, 4, 5, -1, 100):
        out.append(mk(R, 9, obj={"state": state}))
    for state in (1, 2, 4):
        for v0 in (0, 3, 5, 0x10003, MASK):
            for queued in (0, 3, 5):
                for w220 in (0xE, 0x10E, 3, 0):
                    for row0 in (-1, 5):
                        out.append(mk(R, 9, things={0: row0, 3: 1, 5: 2}, w220={1: w220, 2: w220, 5: w220},
                                      scripts=[S(R, handle=handle, vcount=2, vars=[v0, 1])], q={"id": R.choice([[0x1234], [7, 9]])},
                                      obj={"state": state, "queued": queued, "f20": handle}))
    out.append(mk(R, 9, g=[0, 0, 0, 0, 0, 0, 0, 1, 0], things={0: 5, 3: 1}, w220={1: 0xE}, scripts=[S(R, handle=handle, vcount=2, vars=[3, 1])],
                  obj={"state": 1, "queued": 3, "f20": handle}))
    out.append(mk(R, 9, things={0: 5, 3: 1}, w220={1: 0xE}, scripts=[], obj={"state": 2, "queued": 3, "f20": 99}))
    return out


def create_edges():
    out = []
    for fname in ("", "ride", "a b"):
        for ffp in ([1], [0, 1], [1, 0]):
            for rec in (0, 1, 2, -1):
                for idq in (0x1234, 1, 0, 0xFFFF):
                    out.append(mk(R, 10, fname=fname, q={"ffp": ffp, "id": [idq], "init": [R.choice([0, 5, -1])]}, rec=rec,
                                  obj={"f1c": R.choice([0, 7, MASK])}))
    out.append(mk(R, 10, fname="z" * 200, folder="f" * 200, q={"ffp": [1]}))
    return out


def engine_edges():
    out = []
    for flag in (0, 1, -1):
        for ri in (0, 1):
            for dont in (0, 1, -1):
                for ox in (0, 12):
                    for oz in (0, -3):
                        for shape in (-1, 17):
                            for f30 in (0, 1, 2, 128, 129, 130, 0x8000, 0xFFFF):
                                out.append(mk(R, 11, a=flag, q={"ri": [ri], "inst": [R.choice([0, 7, -1])], "rest": [3]}, shape=shape,
                                              ride={S_DONT: dont, S_OVX: ox, S_OVZ: oz, S_OVW: R.choice([0, 4]), S_OVH: R.choice([0, -9])},
                                              obj={"f30": f30, "f20": R.choice([0, 3, MASK]), "f1c": R.choice([0, 9, MASK]), "rot": R.choice([0, -1, 90])}))
    return out


EDGES = [tick_edges, render_edges, getvar_edges, pointer_edges, reverb_edges, script_tick_edges, close_edges, sprite_edges, turn_edges, model_edges,
         create_edges, engine_edges]
TESTS = [RSTest(NAMES[i], ppc_case, host_case, edge=EDGES[i], rnd=(lambda op: (lambda r: mk(r, op)))(i)) for i in range(12)]
