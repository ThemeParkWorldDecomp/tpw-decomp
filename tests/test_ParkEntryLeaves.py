"""Differential tests for the park entry leaves (unit w114), against libtpw_unit_ParkEntryLeaves.dylib (tests/unit_ParkEntryLeaves.cpp):

  src/World_rides.cpp       CWorld::DoIHaveTheseRides 0x10140fac
  src/RideDB_walk.cpp       CRideDatabase::GetFirstRide 0x101547b4, GetNextRide 0x10154800
  src/hlsprite_flybase.cpp  FLY_RemoveBase 0x100781f4
  src/TPEmbed_destroy.cpp   DisableRideHead 0x1020039c, TPDestroyEmbeddedData 0x101ffa58
  src/analyser_kill.cpp     SCR_KillScript 0x100d9478
  src/TPRide_proctex.cpp    LoadProceduralTextureSet 0x1005b8fc
  src/rsse_proctex.cpp      CorrectOrientationFlagsForDiffProceduralMode 0x100b7fa0

The nine functions run as the original on the PowerPC side and as src/ on the host; where one calls another (the walk under DoIHaveTheseRides,
DisableRideHead and SCR_KillScript under FLY_RemoveBase, the corrector under LoadProceduralTextureSet) the callee is real on both sides.
Everything else they call is a fake that records the same event on both sides: FindDataNodeIndexByID (the ride set's label, the mask and
the id; the queue answers, 0xffffffff when empty), DestroyRideInstance, PAR_KillEmitter, SOUND_StopSoundEffect, LbMem_Free and MReleaseFreeF
(they free nothing, they record the block's label; a script block's free also records the script table's entry at that moment, which the
original has cleared already), GetMeshPath (the case's folder or NULL), PT_Load (the mode, the length and a hash of the
path, the path) and PT_Disable. memset, sprintf and labs are the C library (stubs of tpwtest on the PowerPC side).

The scene of the head functions: the script system (on word, size word, a table of 128 words whose entries are 0 or 0x40000000 + 16 * handle,
labelled 100 + handle), ride instances 1 and 5 of gRideAnimSets (labelled 20 and 21) each with a mesh whose embedded set has eight records,
and for FLY_RemoveBase a table of 100 fly bases (the block being removed is label 30, another block 31). What is compared after a call is
the answer, the log, and every word the function can have written: the scene (set flags, counter, the records' flags and script handles,
the script table), the fly block's 1163 words and the table of bases, the masks, the height field quads and the copy.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ParkEntryLeaves"

MASK = 0xFFFFFFFF
DO_I_HAVE, GET_FIRST, GET_NEXT = 0x10140FAC, 0x101547B4, 0x10154800
FLY_REMOVE_BASE, DISABLE_HEAD, KILL_SCRIPT = 0x100781F4, 0x1020039C, 0x100D9478
PROCTEX_SET, CORRECT, DESTROY_EMBEDDED = 0x1005B8FC, 0x100B7FA0, 0x101FFA58
FIND_NODE, DESTROY_RIDE, KILL_EMITTER, STOP_SOUND, RELEASE_FREE, GET_MESH_PATH = (0x101FFDC8, 0x1006A3EC, 0x100BE1E8, 0x100DE058, 0x10064C54,
                                                                                0x100B88A0)
RIDE_DB, OWNED = 0x1037F6C8, 0x1034CB98
REQUIRED_MASK = 0x1034CBB8   # gParkRequiredRides, the 32 bytes after the owned mask
FLY_BASES, RIDE_ANIM_SETS = 0x102DA964, 0x102C79A4
SCRIPT_ON, SCRIPT_SIZE, SCRIPT_TABLE = 0x102F7C28, 0x102F7C14, 0x102F7C18
OPTIONS, HF_HEADER, HF_COPY = 0x102B2B48, 0x102C6C7C, 0x102C6C80

POOL, RECORDS, TABLE_WORDS = 8, 8, 128
SET_INTS = 2 + 2 * RECORDS
SCRIPT_BASE = 0x40000000
SET_LABEL, SCRIPT_LABEL, FLY_LABEL = 20, 100, 30
BLOCK_WORDS = 0x122C // 4
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
VP, I32, U32, U16, U8 = C.c_void_p, C.c_int32, C.c_uint32, C.c_uint16, C.c_uint8


def s32(v):
    return T.s32(v & MASK)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def script_word(h):
    return SCRIPT_BASE + 16 * h


def fnv(text):
    h = 0xCBF29CE484222325
    for b in text:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h & MASK


# ======================================================================= PowerPC side fakes


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real.update({DO_I_HAVE, GET_FIRST, GET_NEXT, FLY_REMOVE_BASE, DISABLE_HEAD, KILL_SCRIPT, PROCTEX_SET, CORRECT, DESTROY_EMBEDDED})
    S.internal(FIND_NODE, R("FindDataNodeIndexByID", "Luu", ret=lambda e, L: L.ret("FindDataNodeIndexByID", -1)))
    S.internal(DESTROY_RIDE, R("DestroyRideInstance", "u"))
    S.internal(KILL_EMITTER, R("PAR_KillEmitter", "i"))
    S.internal(STOP_SOUND, R("SOUND_StopSoundEffect", "i"))
    S.internal(RELEASE_FREE, R("MReleaseFreeF", "L"))

    def lb_free(e):
        block = e.r(3)
        entry = -1
        if SCRIPT_BASE <= block < SCRIPT_BASE + 16 * TABLE_WORDS and (block - SCRIPT_BASE) % 16 == 0:
            entry = e.u32(e.ps.table + 4 * ((block - SCRIPT_BASE) // 16))
        e.cl.rec("LbMem_Free", (e.cl.lab(block), entry))

    S.imp("LbMem_Free__FPv", lb_free)

    def get_mesh_path(e):
        e.cl.rec("GetMeshPath")
        e.set_r(3, e.ps.path)

    def pt_load(e):
        path = e.cstr(e.r(3), limit=1 << 16)
        e.cl.rec("PT_Load", (T.s32(e.r(4)), len(path), fnv(path.encode("latin-1"))), (path[:159],))
        e.set_r(3, 0)

    S.internal(GET_MESH_PATH, get_mesh_path)
    S.imp("PT_Load__FPci", pt_load)
    S.imp("PT_Disable__Fv", lambda e: e.cl.rec("PT_Disable"))
    return S


STUBS = None


class PsState:
    pass


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    runner.emu.stub("labs", lambda e: e.set_r(3, T.u32(abs(T.s32(e.r(3))))))
    STUBS = build_stubs()


def lib_setup(lib):
    lib.tpw_pe_ride.argtypes = [I32, I32, I32]
    lib.tpw_pe_db.argtypes = [I32, I32, VP, I32]
    lib.tpw_pe_do_i_have.restype = I32
    lib.tpw_pe_do_i_have.argtypes = [VP, I32, VP, VP]
    lib.tpw_pe_required_after.argtypes = [VP]
    lib.tpw_pe_walk.restype = I32
    lib.tpw_pe_walk.argtypes = [I32, VP]
    lib.tpw_pe_scene.argtypes = [I32, I32, VP, VP]
    lib.tpw_pe_scene_get.argtypes = [VP, VP]
    lib.tpw_pe_disable_head.argtypes = [I32, U32]
    lib.tpw_pe_kill_script.argtypes = [I32]
    lib.tpw_pe_fly.argtypes = [I32, VP, VP]
    lib.tpw_pe_fly_table.argtypes = [VP]
    lib.tpw_pe_destroy.argtypes = [I32, I32, I32, VP]
    lib.tpw_pe_hfield.argtypes = [I32, I32, VP, VP]
    lib.tpw_pe_hfield_get.argtypes = [I32, I32, VP, VP]
    lib.tpw_pe_correct.argtypes = [I32]
    lib.tpw_pe_proctex.argtypes = [I32, U32, C.c_char_p]


def arr(ctype, values):
    return (ctype * max(len(values), 1))(*values)


class Run:
    """The state a PowerPC case runs in: the log with its fixed labels, the fakes active, the case's scratch."""

    def __init__(self, e, queues, labels=None):
        self.e = e
        self.saved = STUBS.activate(e)
        self.labels = dict(labels or {})
        self.queues = queues
        self.log = None

    def start(self):
        self.e.cl = self.log = CL.PpcLog(self.labels, self.queues)
        self.e.ps = PsState()
        self.e.ps.path = 0

    def close(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None


# ======================================================================= the scene (script system, ride sets)


def new_scene(on=1, size=100, table=None, sets=None):
    table = list(table) if table is not None else [1 if h % 3 == 0 else 0 for h in range(TABLE_WORDS)]
    if sets is None:
        sets = []
        for s in range(2):
            sets += [6, 2] + [x for r in range(RECORDS) for x in (0x2 | (r & 1) * 4 | (r & 4) * 8, (r * 7 + s) % 100)]
    return dict(on=on, size=size, table=table, sets=list(sets))


def random_scene(r):
    sets = []
    for s in range(2):
        sets += [r.choice([0, 1, 4, 5, 6, 7, 0xC, 0xFFFFFFFF, r.getrandbits(32)]),
                 r.choice([0, 1, 2, 3, 5, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)])]
        for _ in range(RECORDS):
            sets += [r.choice([0, 2, 4, 6, 0x12, 0x22, 0xFFFFFFFF, r.getrandbits(32)]),
                     r.choice([0, 1, 5, 50, 99, 100, 101, 127, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.randrange(130)])]
    return dict(on=r.choice([1, 1, 1, 0, 2, 0xFFFFFFFF]), size=r.choice([100, 100, 100, 0, 1, 99, 101, -1, r.randrange(-3, 130)]),
                table=[r.randrange(2) for _ in range(TABLE_WORDS)], sets=[s & MASK for s in sets])


def script_labels():
    return {script_word(h): SCRIPT_LABEL + h for h in range(TABLE_WORDS)}


def ppc_scene(e, sc):
    """Writes the scene to the emulator; answers (the addresses of the ride sets, the meshes, the embedded sets, the record arrays, the table)."""
    table = e.alloc(4 * TABLE_WORDS, b"".join(struct.pack(">I", script_word(h) if sc["table"][h] else 0) for h in range(TABLE_WORDS)))
    w32(e, SCRIPT_ON, sc["on"])
    w32(e, SCRIPT_SIZE, sc["size"])
    w32(e, SCRIPT_TABLE, table)
    e.write(RIDE_ANIM_SETS, bytes(4 * 16))
    parts = []
    for s in range(2):
        records, embedded, header, mesh, ride = e.alloc(0x14 * RECORDS), e.alloc(0x14), e.alloc(0x80), e.alloc(0x40), e.alloc(0x100)
        w32(e, mesh + 4, header)
        w32(e, mesh + 0x28, embedded)
        w32(e, ride + 8, mesh)
        ints = sc["sets"][s * SET_INTS:(s + 1) * SET_INTS]
        e.write(embedded, struct.pack(">IIIII", ints[0], records, ints[1], 0, 0))
        for r in range(RECORDS):
            e.write(records + 0x14 * r, struct.pack(">IIIII", ints[2 + 2 * r], 0x0B0B0B0B, 0x0C0C0C0C, ints[3 + 2 * r], 0x0D0D0D0D))
        parts.append((ride, mesh, embedded, records))
    w32(e, RIDE_ANIM_SETS + 4, parts[0][0])
    w32(e, RIDE_ANIM_SETS + 20, parts[1][0])
    return dict(table=table, parts=parts)


def ppc_scene_labels(addrs):
    labels = script_labels()
    labels[addrs["parts"][0][0]] = SET_LABEL
    labels[addrs["parts"][1][0]] = SET_LABEL + 1
    return labels


def ppc_scene_read(e, addrs):
    words = [s32(e.u32(addrs["table"] + 4 * h)) for h in range(TABLE_WORDS)]
    sets = []
    for ride, mesh, embedded, records in addrs["parts"]:
        sets += [s32(e.u32(embedded)), s32(e.u32(embedded + 8))]
        for r in range(RECORDS):
            sets += [s32(e.u32(records + 0x14 * r)), s32(e.u32(records + 0x14 * r + 12))]
            # (the words the functions never write: they must be as they were)
            assert e.u32(records + 0x14 * r + 4) == 0x0B0B0B0B and e.u32(records + 0x14 * r + 16) == 0x0D0D0D0D
    return dict(table=words, sets=sets)


def host_scene(lib, sc):
    lib.tpw_pe_scene(s32(sc["on"]), sc["size"], arr(I32, [x if x < 0x80000000 else x - (1 << 32) for x in sc["table"]]),
                     arr(I32, [x if x < 0x80000000 else x - (1 << 32) for x in sc["sets"]]))


def host_scene_read(lib):
    table, sets = (I32 * TABLE_WORDS)(), (I32 * (2 * SET_INTS))()
    lib.tpw_pe_scene_get(table, sets)
    return dict(table=list(table), sets=list(sets))


def signed_list(values):
    return [s32(v) for v in values]


# ======================================================================= DoIHaveTheseRides, GetFirstRide, GetNextRide


def ride_pool(r=None):
    """(bit, preview) of the 8 pool records."""
    if r is None:
        return [(8 * k + 1, 0) for k in range(POOL)]
    return [(r.choice([0, 1, 2, 8, 9, 17, 256, 33, r.randrange(1, 257)]), r.choice([0, 0, 1, 5])) for _ in range(POOL)]


def db_case(pool=None, slots=None, count=None, cursor=0, required=bytes(32), previews=0, owned=bytes(32)):
    pool = pool or ride_pool()
    slots = [0, 1, 2] if slots is None else slots
    return dict(pool=pool, slots=slots, count=len(slots) if count is None else count, cursor=cursor, required=bytes(required), previews=previews,
                owned=bytes(owned))


def write_ppc_db(e, c):
    pool = []
    for bit, preview in c["pool"]:
        rec = e.alloc(0x100, b"\x00" * 0x100)
        w32(e, rec + 0x94, bit)
        w32(e, rec + 0x98, preview)
        pool.append(rec)
    e.write(RIDE_DB, bytes(0x738))
    for i, k in enumerate(c["slots"][:0x14A]):
        w32(e, RIDE_DB + 4 * i, 0 if k < 0 else pool[k])
    w32(e, RIDE_DB + 0x528, c["count"])
    w32(e, RIDE_DB + 0x72C, c["cursor"])
    return pool


def host_db(lib, c):
    for k, (bit, preview) in enumerate(c["pool"]):
        lib.tpw_pe_ride(k, bit, preview)
    slots = c["slots"][:0x14A]
    lib.tpw_pe_db(c["count"] if c["count"] < 0x80000000 else c["count"] - (1 << 32), c["cursor"], arr(I32, slots), len(slots))


def ppc_do_i_have(e, c):
    run = Run(e, {})
    try:
        run.start()
        pool = write_ppc_db(e, c)
        e.write(OWNED, c["owned"])
        e.write(REQUIRED_MASK, b"\xc3" * 32)
        required = e.alloc(32, c["required"])
        r3 = T.call_ppc(e, DO_I_HAVE, [0, required, T.u32(c["previews"])])[0]
        out = dict(result=s32(r3), owned=e.read(OWNED, 32), required_mask=e.read(REQUIRED_MASK, 32),
                   cursor=s32(e.u32(RIDE_DB + 0x72C)), log=run.log.log)
    finally:
        run.close()
    return out


def host_do_i_have(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {})
    host_db(lib, c)
    owned = C.create_string_buffer(c["owned"], 32)
    cursor = I32()
    required = C.create_string_buffer(c["required"], 32)
    result = lib.tpw_pe_do_i_have(required, c["previews"], owned, C.byref(cursor))
    required_mask = C.create_string_buffer(32)
    lib.tpw_pe_required_after(required_mask)
    return dict(result=result, owned=owned.raw[:32], required_mask=required_mask.raw[:32], cursor=cursor.value, log=CL.host_end(lib))


def walk_visit(c):
    """The slots DoIHaveTheseRides' walk visits (the model the generator uses to keep the masks within their 32 bytes)."""
    seen, cursor = [], 0
    k = c["slots"][0] if c["slots"] else -1
    while k >= 0 and len(seen) < 400:
        seen.append(k)
        if cursor < s32(c["count"] - 1):
            cursor += 1
            k = c["slots"][cursor] if cursor < len(c["slots"]) else -1
        else:
            break
    return seen


def owned_model(c):
    mask = bytearray(40)
    for k in walk_visit(c):
        bit, preview = c["pool"][k]
        counts = not (preview != 0 and c["previews"] != 0)
        if bit != 0 and counts:
            n = (bit - 1) & MASK
            if n >> 3 < 32:
                mask[n >> 3] |= 1 << (n & 7)
    return bytes(mask[:32])


def do_i_have_edges():
    cases = []
    base_pool = ride_pool()
    # one ride per owned bit (bit 8i + 1 is bit 0 of byte i): required bit 0 of byte i, other bits of it, with the ride there or not
    for i in range(32):
        for present in (True, False):
            for byte in (0x01, 0x02, 0x80, 0x81, 0xFF, 0x00, 0x7E):
                required = bytearray(32)
                required[i] = byte
                pool = list(base_pool)
                pool[0] = (8 * i + 1, 0)
                cases.append(db_case(pool, slots=[0] if present else [-1], required=required, owned=bytes([0xA5] * 32)))
    # the quirk: every bit but bit 0 alone, nothing owned, answers 1
    for i in (0, 1, 15, 31):
        for bit in range(1, 8):
            required = bytearray(32)
            required[i] = 1 << bit
            cases.append(db_case(slots=[-1], required=required))
    cases.append(db_case(slots=[-1], required=bytes([0xFF] * 32)))
    cases.append(db_case(slots=[-1], required=bytes([0xFE] * 32)))
    cases.append(db_case(slots=[-1], required=bytes([0x01] * 32)))
    cases.append(db_case(slots=[-1], required=bytes([0x81] * 32)))
    cases.append(db_case(slots=[-1], required=bytes([0x80] * 32)))
    # databases of 0, 1, 2 and 5 rides, the cursor left over, the ride words
    for count, slots in ((0, [-1]), (1, [0]), (2, [0, 1]), (5, [0, 1, 2, 3, 4]), (0, [0, 1]), (1, [0, 1]), (3, [0, -1, 2]), (2, [-1, 1])):
        for cursor in (0, 3, -1, 0x7FFFFFFF):
            cases.append(db_case(slots=slots, count=count, cursor=cursor, required=bytes([1] * 32), owned=bytes([0x5A] * 32)))
    for bit in (0, 1, 2, 8, 9, 17, 256, 255, 128, 129):
        for preview in (0, 1, 5):
            for previews in (0, 1, 2, -1, INT_MIN):
                pool = [(bit, preview)] + base_pool[1:]
                required = bytearray(32)
                n = (bit - 1) & MASK
                if bit != 0:
                    required[n >> 3] = 1 if n & 7 == 0 else 0x81
                cases.append(db_case(pool, slots=[0], required=required, previews=previews))
    # two rides sharing a byte; a ride with the preview word and previews acceptable or not, others plain
    pool = [(1, 0), (2, 1), (3, 5), (4, 0), (5, 0), (9, 1), (10, 0), (256, 0)]
    for previews in (0, 1, 2, -1):
        for required0 in (0x01, 0x03, 0x05, 0xFF, 0x00):
            cases.append(db_case(pool, slots=[0, 1, 2, 3, 4, 5, 6, 7], count=8, required=bytes([required0, 0x01] + [0] * 29 + [0x01]),
                                 previews=previews))
    return cases


def do_i_have_rnd(r):
    pool = ride_pool(r)
    n = r.randrange(0, 9)
    slots = [r.randrange(POOL) for _ in range(n)] + [-1] * 2
    if r.random() < 0.15 and n > 2:
        slots[r.randrange(n)] = -1
    count = r.choice([n, n, n, r.randrange(0, 10), INT_MIN, -1, 330])
    c = db_case(pool, slots=slots, count=count, cursor=r.choice([0, 1, 5, -1, 0x7FFFFFFF, r.getrandbits(31)]),
                previews=r.choice([0, 1, 2, -1, r.getrandbits(32) - (1 << 31)]), owned=bytes(r.getrandbits(8) for _ in range(32)))
    owned = owned_model(c)
    roll = r.random()
    if roll < 0.4:
        # required inside the owned bits (answer 1), with other bits mixed in
        required = bytes((owned[i] if r.random() < 0.5 else 0) | (r.getrandbits(8) & 0xFE) for i in range(32))
    elif roll < 0.7:
        required = bytes(owned[i] | (1 if r.random() < 0.1 else 0) for i in range(32))
    else:
        required = bytes(r.getrandbits(8) for _ in range(32))
    c["required"] = required
    return c


def ppc_walk(which):
    def run(e, c):
        r = Run(e, {})
        try:
            r.start()
            pool = write_ppc_db(e, c)
            r3 = T.call_ppc(e, GET_FIRST if which == 0 else GET_NEXT, [RIDE_DB])[0]
            index = -1 if r3 == 0 else (pool.index(r3) if r3 in pool else -2)
            out = dict(result=index, cursor=s32(e.u32(RIDE_DB + 0x72C)))
        finally:
            r.close()
        return out
    return run


def host_walk(which):
    def run(lib, c):
        lib_setup(lib)
        host_db(lib, c)
        cursor = I32()
        result = lib.tpw_pe_walk(which, C.byref(cursor))
        return dict(result=result, cursor=cursor.value)
    return run


def wrapped(count):
    return s32((count - 1) & MASK)


def walk_ok(which, c):
    """The original reads slot cursor + 1 in GetNextRide when the cursor is below the (wrapped) count less one: keep it inside the 330 slots."""
    if which == 0:
        return True
    if c["cursor"] < wrapped(c["count"]):
        return 0 <= c["cursor"] + 1 < 0x14A
    return True


def walk_slots(r=None):
    if r is None:
        return [k % POOL if k % 7 != 3 else -1 for k in range(0x14A)]
    return [r.choice([-1, 0, 1, 2, 3, 4, 5, 6, 7]) for _ in range(0x14A)]


def walk_edges(which):
    def edges():
        cases = []
        for count in (0, 1, 2, 3, 330, 329, 331, INT_MIN, INT_MIN + 1, -1, -2, INT_MAX, 100):
            for cursor in (0, 1, count - 2, count - 1, count, count + 1, -1, -2, INT_MAX, 328, 329):
                if not (INT_MIN <= cursor <= INT_MAX):
                    continue
                c = db_case(slots=walk_slots(), count=count, cursor=cursor)
                if walk_ok(which, c):
                    cases.append(c)
        # empty slots and short slot lists
        for slots in ([], [-1], [0], [-1, 1], [0, -1, 2]):
            for count in (0, 1, 2, 3):
                for cursor in (0, 1, 2, -1):
                    c = db_case(slots=slots, count=count, cursor=cursor)
                    if walk_ok(which, c):
                        cases.append(c)
        return cases
    return edges


def walk_rnd(which):
    def rnd(r):
        for _ in range(50):
            count = r.choice([r.randrange(0, 12), r.randrange(0, 331), INT_MIN, -1, INT_MAX, r.getrandbits(32) - (1 << 31)])
            cursor = r.choice([0, 1, -1, count - 1, count - 2, count, r.randrange(0, 331), INT_MAX, r.getrandbits(32) - (1 << 31)])
            if not (INT_MIN <= cursor <= INT_MAX):
                continue
            c = db_case(slots=walk_slots(r), count=count, cursor=cursor)
            if walk_ok(which, c):
                return c
        return db_case(slots=[0, 1], count=2, cursor=0)
    return rnd


# ======================================================================= FLY_RemoveBase

COUNTS = [0, 1, 2, 7, -1, -5, 100]
DIVISORS = [0, 1, 2, 3, -1, -3, 7]
OFFSETS = [0, 1, -4, 100]


def slot_spec(active=1, count=0, ride=0, emitter=0, offset=0, divisor=1, sound=0):
    return dict(active=active, count=count, ride=ride, emitter=emitter, offset=offset, divisor=divisor, sound=sound)


def block_words(spec):
    """The 1163 words of a fly base: filler everywhere, the fields set from the spec."""
    words = [0x11000000 + i for i in range(BLOCK_WORDS)]
    words[3] = spec["live"] & MASK
    for k, s in enumerate(spec["slots"]):
        base = (0xDC + k * 0xE4) // 4
        words[base + 0] = s["active"] & MASK
        words[base + 3] = s["count"] & MASK
        words[base + 0x10] = s["ride"] & MASK
        words[base + 0x11] = s["emitter"] & MASK
        words[base + 0x16] = s["offset"] & MASK
        words[base + 0x17] = s["divisor"] & MASK
        words[base + 0x18] = s["sound"] & MASK
    return words


def fly_case(index=1, block=None, kinds=None, scene=None, find=None, live=20, slots=None):
    if block is None:
        block = dict(live=live, slots=slots if slots is not None else [slot_spec() for _ in range(20)])
    if kinds is None:
        kinds = [0] * 100
        if 0 < index < 100:
            kinds[index] = 1
        for i in (2, 50, 99):
            if i != index:
                kinds[i] = 2
    return dict(index=index, block=block, kinds=list(kinds), scene=scene or new_scene(), find=find)


def ppc_fly(e, c):
    sc_addrs = None
    run = Run(e, {})
    try:
        block0 = e.alloc(0x122C, struct.pack(">%dI" % BLOCK_WORDS, *block_words(c["block"])))
        block1 = e.alloc(0x122C, b"\x5a" * 0x122C)
        sc_addrs = ppc_scene(e, c["scene"])
        labels = ppc_scene_labels(sc_addrs)
        labels[block0], labels[block1] = FLY_LABEL, FLY_LABEL + 1
        run.labels = labels
        run.queues = {"FindDataNodeIndexByID": c["find"]} if c["find"] else {}
        run.start()
        e.ps.table = sc_addrs["table"]
        e.write(FLY_BASES, struct.pack(">100I", *[0 if k == 0 else (block0 if k == 1 else block1) for k in c["kinds"]]))
        T.call_ppc(e, FLY_REMOVE_BASE, [T.u32(c["index"])])
        table = struct.unpack(">100I", e.read(FLY_BASES, 400))
        kinds = [0 if a == 0 else (1 if a == block0 else (2 if a == block1 else 3)) for a in table]
        out = dict(words=list(struct.unpack(">%dI" % BLOCK_WORDS, e.read(block0, 0x122C))), kinds=kinds, scene=ppc_scene_read(e, sc_addrs),
                   other=e.read(block1, 0x122C), log=run.log.log)
    finally:
        run.close()
    return out


def host_fly(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {}, {"FindDataNodeIndexByID": c["find"]} if c["find"] else {})
    host_scene(lib, c["scene"])
    words = (U32 * BLOCK_WORDS)(*block_words(c["block"]))
    lib.tpw_pe_fly(c["index"], arr(I32, c["kinds"]), words)
    kinds = (I32 * 100)()
    lib.tpw_pe_fly_table(kinds)
    return dict(words=list(words), kinds=list(kinds), scene=host_scene_read(lib), other=b"\x5a" * 0x122C, log=CL.host_end(lib))


def fly_edges():
    cases = []
    # the index
    for index in (-1, 0, 1, 50, 99, 100, 101, INT_MIN, INT_MAX, 2, 98):
        cases.append(fly_case(index, slots=[slot_spec(count=2, ride=1, divisor=3, emitter=7, sound=9) for _ in range(20)]))
    # an empty table entry at a valid index
    for index in (1, 50, 99):
        kinds = [0] * 100
        cases.append(fly_case(index, kinds=kinds))
    # slots active in different subsets
    for pattern in (0, 1, 0x80000, 0xAAAAA, 0x55555, 0xFFFFF, 0x1, 0xFFFFE, 0x3C0F0):
        slots = [slot_spec(active=(pattern >> k) & 1, count=1, ride=1 if k % 2 else 5, divisor=2, offset=k, emitter=k, sound=k * 3) for k in range(20)]
        cases.append(fly_case(7, slots=slots, find=[0, 3, 0xFFFFFFFF, 1, 2]))
    # the fields of one active slot, over the listing's values
    for count in COUNTS:
        for divisor in DIVISORS:
            for offset in OFFSETS:
                for ride in (0, 1, 5):
                    slots = [slot_spec(active=1 if k == 3 else 0, count=count, ride=ride, divisor=divisor, offset=offset, emitter=0, sound=0)
                             for k in range(20)]
                    cases.append(fly_case(1, slots=slots, find=[0xFFFFFFFF, 0, 3, 1, 2, 5, 7]))
    # find answers: not found, found at 0, at 3
    for find in ([0xFFFFFFFF], [0], [3], [0, 3, 0xFFFFFFFF], [7, 1], [-1, 2, 2]):
        slots = [slot_spec(count=5, ride=1, divisor=4, offset=1) if k in (0, 19) else slot_spec(active=0) for k in range(20)]
        cases.append(fly_case(5, slots=slots, find=find))
    # the emitter and the sound words, zero and nonzero, and the live count
    for emitter in (0, 5, -1):
        for sound in (0, 9, -1):
            for live in (0, 1, 20, INT_MIN, -1):
                slots = [slot_spec(count=0, emitter=emitter, sound=sound, ride=5) if k in (2, 3, 4) else slot_spec(active=0) for k in range(20)]
                cases.append(fly_case(9, slots=slots, live=live))
    # script handles (the heads' scripts) killed in the loop: a scene with every handle present, and the system off
    for scene in (new_scene(), new_scene(on=0), new_scene(size=0), new_scene(size=1), new_scene(table=[1] * TABLE_WORDS)):
        slots = [slot_spec(count=-6, ride=1 if k % 2 else 5, divisor=5, offset=3, emitter=2, sound=4) for k in range(3)] + \
                [slot_spec(active=0) for _ in range(17)]
        cases.append(fly_case(1, slots=slots, scene=scene, find=[0, 1, 2, 3, 4, 5, 6, 7]))
    return cases


def fly_rnd(r):
    slots = []
    for k in range(20):
        active = 1 if r.random() < 0.5 else r.choice([0, 0, 2, 0xFFFFFFFF])
        slots.append(slot_spec(active, r.choice(COUNTS[:6] + [r.randrange(-12, 12)]), r.choice([0, 1, 5, 1, 5]),
                               r.choice([0, 0, 3, 77, r.getrandbits(32)]), r.choice(OFFSETS + [r.randrange(-50, 50)]),
                               r.choice(DIVISORS + [r.randrange(1, 12)]), r.choice([0, 0, 8, r.getrandbits(32)])))
    index = r.choice([1, 2, 50, 99, r.randrange(1, 100), r.randrange(-3, 105)])
    find = [r.choice([0xFFFFFFFF, 0, 1, 2, 3, 4, 5, 6, 7]) for _ in range(r.randrange(1, 40))]
    return fly_case(index, slots=slots, live=r.choice([20, 0, 5, r.getrandbits(32) - (1 << 31)]), scene=random_scene(r), find=find)


# ======================================================================= DisableRideHead and SCR_KillScript


def disable_case(set=0, index=0, scene=None):
    return dict(set=set, index=index, scene=scene or new_scene())


def ppc_disable(e, c):
    run = Run(e, {})
    try:
        addrs = ppc_scene(e, c["scene"])
        run.labels = ppc_scene_labels(addrs)
        run.start()
        e.ps.table = addrs["table"]
        T.call_ppc(e, DISABLE_HEAD, [addrs["parts"][c["set"]][1], c["index"]])
        out = dict(scene=ppc_scene_read(e, addrs), log=run.log.log)
    finally:
        run.close()
    return out


def host_disable(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {})
    host_scene(lib, c["scene"])
    lib.tpw_pe_disable_head(c["set"], c["index"])
    return dict(scene=host_scene_read(lib), log=CL.host_end(lib))


def disable_edges():
    cases = []
    for f08 in (0, 1, 2, 0xFFFFFFFF, 5, 0x80000000):
        for flags in (0, 4, 6, 0xC, 0xFFFFFFFF, 0xFFFFFFFB):
            for index in (0, 1, 7):
                for rflags in (0, 2, 4, 6, 0xFFFFFFFF, 0x12):
                    # (the handles 3, 6, 9 .. are the ones the default table holds a script for)
                    sets = [flags, f08] + [x for r in range(RECORDS) for x in (rflags if r == index else 0x6, 3 * (1 + r))] + \
                           [flags, f08] + [x for r in range(RECORDS) for x in (rflags if r == index else 0x6, 3 * (1 + r))]
                    cases.append(disable_case(0, index, new_scene(sets=sets)))
    for handle in (0, 1, 3, 5, 50, 51, 99, 100, 101, 127, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0xFFFFFFF0):
        for on in (0, 1):
            for size in (100, 0, 1, 99, 101, -1):
                sets = [6, 2] + [x for r in range(RECORDS) for x in (0x2, handle)] + [6, 2] + [x for r in range(RECORDS) for x in (0x2, handle)]
                cases.append(disable_case(1, 2, new_scene(on=on, size=size, table=[1] * TABLE_WORDS, sets=sets)))
    return cases


def disable_rnd(r):
    return disable_case(r.randrange(2), r.randrange(RECORDS), random_scene(r))


def kill_case(handle, scene=None):
    return dict(handle=handle, scene=scene or new_scene())


def ppc_kill(e, c):
    run = Run(e, {})
    try:
        addrs = ppc_scene(e, c["scene"])
        run.labels = ppc_scene_labels(addrs)
        run.start()
        e.ps.table = addrs["table"]
        T.call_ppc(e, KILL_SCRIPT, [T.u32(c["handle"])])
        out = dict(scene=ppc_scene_read(e, addrs), log=run.log.log)
    finally:
        run.close()
    return out


def host_kill(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {})
    host_scene(lib, c["scene"])
    lib.tpw_pe_kill_script(c["handle"])
    return dict(scene=host_scene_read(lib), log=CL.host_end(lib))


def kill_edges():
    cases = []
    for handle in (INT_MIN, -1, 0, 1, 50, 99, 100, 101, 127, INT_MAX, 2, 3):
        for on in (0, 1, 2, 0xFFFFFFFF):
            for size in (100, 0, 1, 99, 101, -1, 128, 3, 4):
                for entry in (0, 1):
                    table = [entry] * TABLE_WORDS
                    cases.append(kill_case(handle, new_scene(on=on, size=size, table=table)))
    return cases


def kill_rnd(r):
    return kill_case(r.choice([INT_MIN, -1, 0, 1, 50, 99, 100, 101, INT_MAX, r.randrange(-3, 130)]), random_scene(r))


# ======================================================================= TPDestroyEmbeddedData


def destroy_case(has_set, has_records, has_blocks):
    return dict(has_set=has_set, has_records=has_records, has_blocks=has_blocks)


def ppc_destroy(e, c):
    run = Run(e, {})
    try:
        mesh, set_, records, blocks = e.alloc(0x40), e.alloc(0x14), e.alloc(16), e.alloc(16)
        run.labels = {mesh: 60, set_: 61, records: 62, blocks: 63}
        run.start()
        e.write(set_, struct.pack(">IIIII", 0x1234, records if c["has_records"] else 0, 7, 3, blocks if c["has_blocks"] else 0))
        w32(e, mesh + 0x28, set_ if c["has_set"] else 0)
        T.call_ppc(e, DESTROY_EMBEDDED, [mesh])
        lab = run.log.lab
        out = dict(out=[lab(e.u32(mesh + 0x28)), lab(e.u32(set_ + 4)), lab(e.u32(set_ + 0x10)), s32(e.u32(set_)), s32(e.u32(set_ + 8)),
                        s32(e.u32(set_ + 0xC))], log=run.log.log)
    finally:
        run.close()
    return out


def host_destroy(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {})
    result = (I32 * 6)()
    lib.tpw_pe_destroy(c["has_set"], c["has_records"], c["has_blocks"], result)
    return dict(out=list(result), log=CL.host_end(lib))


def destroy_edges():
    return [destroy_case(a, b, c) for a in (0, 1) for b in (0, 1) for c in (0, 1)]


def destroy_rnd(r):
    return destroy_case(r.randrange(2), r.randrange(2), r.randrange(2))


# ======================================================================= the corrector and LoadProceduralTextureSet

QUAD_FLAGS = [0x0000, 0x0001, 0x0080, 0x0081, 0x0078, 0x00F8, 0x0007, 0x8000, 0xFFFF, 0xFF7E, 0x0179, 0x00FE, 0x0006, 0x8001, 0x007F, 0x0100]


def hf_case(mode=0, quads=None, copy=None, copy_count=None, **extra):
    quads = quads if quads is not None else [(f, 0x1234 + i) for i, f in enumerate(QUAD_FLAGS)]
    copy = copy if copy is not None else [(0xA5F8 ^ (i * 0x35), 0x4321 + i) for i in range(len(quads))]
    c = dict(mode=mode, quads=list(quads), copy=list(copy))
    c.update(extra)
    return c


def flat(pairs):
    return [x for p in pairs for x in p]


def ppc_chain(e, c):
    header, field, copy_field = e.alloc(0x80), e.alloc(0x30), e.alloc(0x30)
    quads = e.alloc(4 * max(len(c["quads"]), 1), struct.pack(">%dH" % (2 * len(c["quads"])), *flat(c["quads"])))
    copy = e.alloc(4 * max(len(c["copy"]), 1), struct.pack(">%dH" % (2 * len(c["copy"])), *flat(c["copy"])))
    e.write(field + 6, struct.pack(">H", len(c["quads"])))
    w32(e, field + 0x2C, quads)
    e.write(copy_field + 6, struct.pack(">H", len(c["copy"])))
    w32(e, copy_field + 0x2C, copy)
    w32(e, header + 0x6C, field)
    w32(e, HF_HEADER, header)
    w32(e, HF_COPY, copy_field)
    return quads, copy


def ppc_chain_read(e, c, quads, copy):
    """The quads and the copy as halfwords in this machine's order (bytes), so that a big count compares fast."""
    q = struct.unpack(">%dH" % (2 * len(c["quads"])), e.read(quads, 4 * len(c["quads"])))
    k = struct.unpack(">%dH" % (2 * len(c["copy"])), e.read(copy, 4 * len(c["copy"])))
    return struct.pack("<%dH" % len(q), *q), struct.pack("<%dH" % len(k), *k)


def host_chain(lib, c):
    lib.tpw_pe_hfield(len(c["quads"]), len(c["copy"]), arr(U16, flat(c["quads"])), arr(U16, flat(c["copy"])))


def host_chain_read(lib, c):
    q, k = (U16 * max(2 * len(c["quads"]), 1))(), (U16 * max(2 * len(c["copy"]), 1))()
    lib.tpw_pe_hfield_get(len(c["quads"]), len(c["copy"]), q, k)
    return bytes(q)[:4 * len(c["quads"])], bytes(k)[:4 * len(c["copy"])]


def ppc_correct(e, c):
    run = Run(e, {})
    try:
        run.start()
        quads, copy = ppc_chain(e, c)
        T.call_ppc(e, CORRECT, [T.u32(c["mode"])])
        q, k = ppc_chain_read(e, c, quads, copy)
        out = dict(quads=q, copy=k, log=run.log.log)
    finally:
        run.close()
    return out


def host_correct(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {})
    host_chain(lib, c)
    lib.tpw_pe_correct(c["mode"])
    q, k = host_chain_read(lib, c)
    return dict(quads=q, copy=k, log=CL.host_end(lib))


def random_quads(r, n):
    return [(r.choice(QUAD_FLAGS + [r.getrandbits(16)]), r.getrandbits(16)) for _ in range(n)]


def correct_edges():
    cases = []
    for mode in (0, 1, 2, -1, INT_MIN, INT_MAX, 0x100):
        cases.append(hf_case(mode))
    for mode in (0, 1):
        for count in (0, 1, 2, 17):
            for seed in range(3):
                r = random.Random(count * 10 + seed)
                cases.append(hf_case(mode, random_quads(r, count), random_quads(r, count)))
        # every flag value against a copy value with every bit
        for f in QUAD_FLAGS:
            cases.append(hf_case(mode, [(f, 0xBEEF)], [(0xFFFF, 0x0001)]))
            cases.append(hf_case(mode, [(f, 0xBEEF)], [(0x0000, 0x0001)]))
        # a short copy: mode 1 never reads past it
        cases.append(hf_case(mode, [(0, 1)] * 5, [(0xF8, 2)]) if mode == 1 else hf_case(mode, [(0x1, 1)] * 5, [(0xF8, 2)] * 5))
        # the biggest count
        r = random.Random(mode + 77)
        cases.append(hf_case(mode, random_quads(r, 0xFFFF), random_quads(r, 0xFFFF)))
    # every one-bit flag and its pair
    for bit in range(16):
        cases.append(hf_case(0, [(1 << bit, 9)], [(0xFFFF, 8)]))
        cases.append(hf_case(1, [(1 << bit, 9)], [(0xFFFF, 8)]))
    return cases


def correct_rnd(r):
    n = r.choice([0, 1, 2, 3, 5, 17, r.randrange(0, 40)])
    return hf_case(r.choice([0, 1, 2, -1, 0, 1]), random_quads(r, n), random_quads(r, n))


def proc_case(mode, options, path, quads=None, copy=None):
    c = hf_case(mode, quads if quads is not None else [(0x00, 1), (0x88, 2), (0x7E, 3)], copy)
    c.update(options=options, path=path)
    return c


def ppc_proctex(e, c):
    run = Run(e, {})
    try:
        run.start()
        quads, copy = ppc_chain(e, c)
        w32(e, OPTIONS, c["options"])
        e.ps.path = 0 if c["path"] is None else e.alloc(len(c["path"]) + 1, c["path"].encode("latin-1") + b"\0", alignment=4)
        T.call_ppc(e, PROCTEX_SET, [T.u32(c["mode"])])
        q, k = ppc_chain_read(e, c, quads, copy)
        out = dict(quads=q, copy=k, log=run.log.log)
    finally:
        run.close()
    return out


def host_proctex(lib, c):
    lib_setup(lib)
    CL.host_begin(lib, {})
    host_chain(lib, c)
    lib.tpw_pe_proctex(c["mode"], c["options"], None if c["path"] is None else c["path"].encode("latin-1"))
    q, k = host_chain_read(lib, c)
    return dict(quads=q, copy=k, log=CL.host_end(lib))


PATHS = [None, "", "data:levels:", "a", "%s", "100%d%", "x" * 150, "x" * 1000, ":Folder With Spaces:", "p%sq%n"]


def proc_edges():
    cases = []
    for path in PATHS:
        for options in (0x4000, 0, 0x2000, 0x6000, 0xFFFFFFFF, 0xFFFFBFFF, 0x4001):
            cases.append(proc_case(1, options, path))
    for mode in (0, 1, -1, 2, INT_MIN, INT_MAX):
        for options in (0x4000, 0):
            cases.append(proc_case(mode, options, "level:"))
    cases.append(proc_case(1, 0x4000, "z", quads=[], copy=[]))
    return cases


def proc_rnd(r):
    n = r.randrange(0, 10)
    path = r.choice(PATHS + ["".join(r.choice("abc:%sd ") for _ in range(r.randrange(0, 60)))])
    return proc_case(r.choice([0, 1, -1, 2, r.getrandbits(32) - (1 << 31)]), r.getrandbits(32) & (0xFFFFBFFF if r.random() < 0.5 else MASK) |
                     (0x4000 if r.random() < 0.5 else 0), path, random_quads(r, n), random_quads(r, n))


TESTS = [
    FnTest("CWorld::DoIHaveTheseRides", ppc_do_i_have, host_do_i_have, edge=do_i_have_edges, rnd=do_i_have_rnd),
    FnTest("CRideDatabase::GetFirstRide", ppc_walk(0), host_walk(0), edge=walk_edges(0), rnd=walk_rnd(0)),
    FnTest("CRideDatabase::GetNextRide", ppc_walk(1), host_walk(1), edge=walk_edges(1), rnd=walk_rnd(1)),
    FnTest("FLY_RemoveBase", ppc_fly, host_fly, edge=fly_edges, rnd=fly_rnd),
    FnTest("DisableRideHead", ppc_disable, host_disable, edge=disable_edges, rnd=disable_rnd),
    FnTest("SCR_KillScript", ppc_kill, host_kill, edge=kill_edges, rnd=kill_rnd),
    FnTest("TPDestroyEmbeddedData", ppc_destroy, host_destroy, edge=destroy_edges, rnd=destroy_rnd),
    FnTest("CorrectOrientationFlagsForDiffProceduralMode", ppc_correct, host_correct, edge=correct_edges, rnd=correct_rnd),
    FnTest("LoadProceduralTextureSet", ppc_proctex, host_proctex, edge=proc_edges, rnd=proc_rnd),
]
