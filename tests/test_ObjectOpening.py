"""Unit w200, a ride opens, closes, runs and asks for an upgrade, and its messages, against libtpw_unit_ObjectOpening.dylib
(tests/unit_ObjectOpening.cpp), main app ("SimTheme Park"):
  src/game/Object.cpp   CObject::IsOpenable 0x1010aaac, IsCloseable 0x1010ab98, CloseObject 0x1010abcc, Reopen 0x1010acdc, IsRunning 0x1010cb28,
                           IsAvailableToChoose 0x101083e0, RequestUpgrade 0x1010c214, SetUpgradeParent 0x1010d9f8, GetUpgradeParent 0x1010da64,
                           NotifyExitBlocked 0x1010a93c, GetDBInfo 0x10107cd4, SetOperatingSpeed 0x10108068, SetOperatingDuration 0x10108104,
                           SetOperatingCapacity 0x10108248, ReceiveMessage 0x10107ba0
  src/game/Object.cpp, src/game/ui/MessageCentre.cpp   the constructors, GetType functions and deleting destructors of CMsgNewRide, CMsgRideBrokenDown, CMsgRideCondemned,
                           CMsgRideWantsUpgrade, CMsgRideDeleted (it has no constructor) and CMsgBlockedExit
  src/game/rides/RideDB.cpp    CRideDatabase::GetRideInfo 0x10154868

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation; the same code reads both back. CRideDatabase::GetRideInfo runs for
real on both sides (the test builds the six ride records it walks, its cache, and the Mac layout of the database in the emulator). Every other callee is
a fake on both sides that records its arguments and answers from the case's queues: the ride questions (IsBrokenDown, IsClosed, IsCondemned,
IsFlaggedForMaintenance, RequiresUpgrade, IsBOQConnected, IsCoaster, IsCoasterUseable, GetBackOfQueue, HasFreeQueueSpace), SetState,
UnassignStaffMember, GetThingId, the hoardings, the script calls, CTRACE, PostMessage, the message centre's operator new and its array constructor,
and the two SetFromTemp. (A first plan ran the real predicates, GetThingId, SetFromTemp and SetState; faking them reaches every answer
combination and links no other source, and the real ones have their own units.)

The world of a case: the CObject block (0x448 bytes between guard bytes, filled with one byte per case, the compared fields set), the script block, the
ride anim set table (8 entries, some NULL), gMessageCentre (present or NULL), the ride database (six records, count, cached id and cached record, a
decoy record for the cache) and, for ReceiveMessage, a message whose GetType the case answers. What is compared: the answer, every compared field of
the object by value, the rest of the block (fill and guards untouched), the database count, cache and slots as labels, gMessageCentre as a label, and
the event log. The message tests build a message in a guarded block and compare the constructor's fields, the type each GetType answers and what the
deleting destructor leaves and deletes.

No fused operations. Not tested: a NULL this for the deleting destructors (the Mac tests it; the host would crash), a record count above the six the
test fills, a NULL record from the database in the functions that read it (they read a field of it without testing it, as the Mac does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ObjectOpening"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
IS_OPENABLE, IS_CLOSEABLE, CLOSE_OBJECT, REOPEN = 0x1010AAAC, 0x1010AB98, 0x1010ABCC, 0x1010ACDC
IS_RUNNING, IS_AVAILABLE, REQUEST_UPGRADE = 0x1010CB28, 0x101083E0, 0x1010C214
SET_PARENT, GET_PARENT, NOTIFY_BLOCKED, GET_DB_INFO = 0x1010D9F8, 0x1010DA64, 0x1010A93C, 0x10107CD4
SET_SPEED, SET_DURATION, SET_CAPACITY, RECEIVE = 0x10108068, 0x10108104, 0x10108248, 0x10107BA0
GET_RIDE_INFO = 0x10154868
SIZES = {
    IS_OPENABLE: 192, IS_CLOSEABLE: 8, CLOSE_OBJECT: 228, REOPEN: 344, IS_RUNNING: 76, IS_AVAILABLE: 396, REQUEST_UPGRADE: 656,
    SET_PARENT: 16, GET_PARENT: 12, NOTIFY_BLOCKED: 200, GET_DB_INFO: 56, SET_SPEED: 104, SET_DURATION: 272, SET_CAPACITY: 356,
    RECEIVE: 256, GET_RIDE_INFO: 104,
}
# name, own vtable, constructor (None: CMsgRideDeleted has none), GetType, deleting destructor, the type GetType answers
MESSAGES = [
    ("CMsgNewRide", 0x102A0CC0, 0x10151998, 0x10110F6C, 0x101064FC, 6),
    ("CMsgRideBrokenDown", 0x102A0C30, 0x10151A14, 0x10110E84, 0x1010E524, 7),
    ("CMsgRideCondemned", 0x102A0C54, 0x10151A98, 0x10110EC0, 0x1010E5A0, 8),
    ("CMsgRideWantsUpgrade", 0x102A0C0C, 0x10151B1C, 0x10110E44, 0x1010E4A4, 9),
    ("CMsgRideDeleted", 0x102A0C9C, None, 0x10110F34, 0x10107B28, 10),
    ("CMsgBlockedExit", 0x102A0C78, 0x10151E48, 0x10110EFC, 0x1010AA34, 0x14),
]
for _name, _vtable, _ctor, _gettype, _dtor, _type in MESSAGES:
    SIZES[_gettype] = 8
    SIZES[_dtor] = 76
    if _ctor:
        SIZES[_ctor] = 40
OWN = set(SIZES)
BASE_VTABLE = 0x102A10EC        # CMsg's vtable, which every constructor stores first and every destructor leaves behind

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
GET_THING_ID, CTRACE = 0x1012E1F8, 0x1006FCBC
GET_SCRIPT_POINTER, SET_VAR, GET_VAR, SET_RIDE_SPEED = 0x100D668C, 0x100D6734, 0x100D6C58, 0x100D6AC0
OPERATOR_DELETE, OPERATOR_NEW, CONSTRUCT_ARRAY = 0x10238218, 0x102384B8, 0x10238758
SET_FROM_TEMP_U, SET_FROM_TEMP_S = 0x101117E8, 0x10111464
IS_BROKEN_DOWN, IS_CONDEMNED, IS_FLAGGED, REQUIRES_UPGRADE = 0x1010C878, 0x1010C8BC, 0x1010C980, 0x1010C9C0
IS_CLOSED, IS_BOQ_CONNECTED, IS_COASTER, IS_COASTER_USEABLE = 0x1010CAE8, 0x10109764, 0x10110CD8, 0x10052954
GET_BACK_OF_QUEUE, HAS_FREE_QUEUE_SPACE = 0x101092F4, 0x101085A0
SET_STATE, UNASSIGN_STAFF = 0x1010DC94, 0x1010C554
ACTIVATE_HOARDINGS, DEACTIVATE_HOARDINGS = 0x100614A0, 0x100617CC
POST_MESSAGE = 0x1015256C
MSG_DEATH_GET_TYPE = 0x1012EC14
CENTRE_CTOR, CENTRE_DTOR = 0x102657E8, 0x10267288
BLOCKED_EXIT_VTABLE = 0x102A0C78

# ---- the Mac's addresses of the globals; the host library's come from tpw_oop_addr(name)
ADDR = dict(centre=0x1037F6C4, db=0x1037F6C8, anims=0x102C79A4)
DB_COUNT, DB_CACHED_ID, DB_CACHED_RIDE = 0x528, 0x730, 0x734       # the Mac's CRideDatabase members, from the database's start
# ---- the host's offsets and sizes the module relies on, checked once against tpw_oop_layout (catches a header that moved)
HOST_LAYOUT = [0x448, 0, 0x0E, 0x1C, 0x20, 0x28, 0x2C, 0x2E, 0x54, 0x58, 0x59, 0x5A, 0x64, 0x68, 0x6C, 0xF4, 0x1A4, 0x22C, 0x2B4, 0x33C,
               0, 0, 8, 0xC, 0x10]

# ---- the block sizes and labels of the world
OBJ_SIZE, GUARD, GUARD_BYTE = 0x448, 0x10, 0xC3
CENTRE_SIZE, RIDE_SIZE = 0x1D0, 0x140
RIDES, ANIMS = 6, 8
L_OBJ, L_SCRIPT, L_CENTRE, L_DECOY, L_MSG = 1, 2, 3, 4, 5
L_ANIM, L_RIDE = 10, 20                                            # plus the index
SLOTS = (36, 70, 71, 72, 73)       # WhichTrackType, MinCapacity, MaxCapacity, MinDuration, MaxDuration (the Mac record keeps slot s at 0x0c + 4 s)
FILLS = (0x00, 0xA5, 0xFF, 0x5A)

# the compared fields of the object: name, offset, width (everything else in the block must stay as the case filled it)
FIELDS = [
    ("thing", 0x00, 2), ("type", 0x0E, 2), ("f1c", 0x1C, 4), ("f20", 0x20, 4), ("f28", 0x28, 4), ("f2c", 0x2C, 2), ("flags", 0x2E, 2),
    ("f54", 0x54, 4), ("f58", 0x58, 1), ("f59", 0x59, 1), ("f5a", 0x5A, 2), ("f64", 0x64, 4), ("loaded", 0x68, 2),
    ("h6c", 0x6C, 4), ("hf4", 0xF4, 4), ("h1a4", 0x1A4, 4), ("h22c", 0x22C, 4), ("h2b4", 0x2B4, 4), ("h33c", 0x33C, 4),
]
# the number of ints the Mac passes after each trace text (not the format's: see the unit library)
TRACE_ARGS = {
    "Object %d: Closing...": 1,
    "Object %d: person being loaded was %d (about to zeroise)\n": 2,
    "!!!!!!!!!! Opening non-openable ride! (non-fatal, can ignore) !!!!!!!!!! \n": 0,
    "Object %d: wants maintenance\n": 1,
    "Cannot upgrade this object\n": 1,
    "Object %d: Cancelling request for upgrade\n": 1,
    "Object %d: Never requested upgrading in the first place\n": 1,
    "Object %d: staff member assigned to service me no longer exists\n": 0,
    "SPEED = %d\n": 1,
    "DUR = %d\n": 1,
    "CAPACITY = %d\n": 1,
}
WIDTH_CODE = {1: "B", 2: "H", 4: "I"}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u16(e, address):
    """The big-endian halfword at an address of the emulator."""
    return struct.unpack(">H", e.read(address, 2))[0]


def stub_get_thing_id(e):
    """CThing::GetThingId: the halfword at the thing's start goes through the hidden pointer (r3), the thing is r4."""
    L = e.cl
    L.rec("GetThingId", (L.lab(e.r(4)),))
    e.write(e.r(3), e.read(e.r(4), 2))


def stub_ctrace(e):
    """CTRACE(level, text, ints): the ints are the registers the original fills for that text; an unknown text is logged under its own name."""
    L = e.cl
    text = e.cstr(e.r(4))
    count = TRACE_ARGS.get(text)
    if count is None:
        L.rec("CTRACE unknown", (), (text,))
        return
    L.rec("CTRACE", [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(count)], (text,))


def stub_get_script_pointer(e):
    """RSSE_GetScriptPointer(handle): the handle as an unsigned word; answers the case's script block."""
    L = e.cl
    L.rec("RSSE_GetScriptPointer", (e.r(3) & MASK,))
    e.set_r(3, e.oop.script_block)


def stub_getvar(e):
    L = e.cl
    L.rec("RSSE_Getvar", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("var", 0) & MASK)


def stub_setvar(e):
    L = e.cl
    L.rec("RSSE_Setvar", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def stub_set_ride_speed(e):
    L = e.cl
    L.rec("RSSE_SetRideSpeed", (L.lab(e.r(3)), s32(e.r(4))))


def stub_activate(e):
    L = e.cl
    L.rec("ActivateHoardings", (L.lab(e.r(3)), e.r(4) & MASK))


def stub_deactivate(e):
    L = e.cl
    L.rec("DeActivateHoardings", (L.lab(e.r(3)),))


def stub_set_state(e):
    L = e.cl
    L.rec("SetState", (L.lab(e.r(3)), s32(e.r(4))))


def stub_unassign(e):
    L = e.cl
    L.rec("UnassignStaffMember", (L.lab(e.r(3)),))


def predicate(name, key):
    """A ride question: records the object (r3) and answers from the queue `key`."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, L.ret(key, 0) & MASK)
    return stub


def stub_get_back_of_queue(e):
    """GetBackOfQueue(): the hidden pointer is r3, the object r4; the answer is a halfword."""
    L = e.cl
    L.rec("GetBackOfQueue", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("GetBackOfQueue", 0) & 0xFFFF))


def set_from_temp(signed):
    """CHistory::SetFromTemp: records where the history is in the object and its temp word (the signed history as signed)."""
    def stub(e):
        L = e.cl
        history = e.r(3)
        temp = e.u32(history)
        L.rec("SetFromTempS" if signed else "SetFromTemp", (history - e.oop.obj, s32(temp) if signed else temp))
    return stub


def stub_post_message(e):
    """CMessageCentre::PostMessage(message): the centre (r3, may be NULL), the message's kind from its vtable, its flags word and thing id."""
    L = e.cl
    message = e.r(4)
    kind = 0x14 if e.u32(message) == BLOCKED_EXIT_VTABLE else -1
    L.rec("PostMessage", (L.lab(e.r(3)), kind, e.u32(message + 4), u16(e, message + 8)))
    e.set_r(3, 1)


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def keep_toc(e):
    """The Mac reaches operator new and the array constructor through a glue that saves r2 at 0x14(r1), and the caller reloads r2 from there."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_new(e):
    """operator new for the message centre (0x1d0 bytes of 0xa5); the queue "newnull" makes it fail."""
    keep_toc(e)
    L = e.cl
    size = e.r(3)
    fail = bool(L.ret("newnull", 0))
    L.rec("new", (size, int(fail)))
    e.set_r(3, 0 if fail else e.alloc(size, b"\xA5" * size))


def stub_construct_array(e):
    """__construct_array(p, ctor, dtor, size, count): the centre's 29 headers; recorded once with the new block's label."""
    keep_toc(e)
    p, ctor, dtor, size, count = (e.r(i) for i in range(3, 8))
    e.cl.rec("CMessageCentre", [e.cl.newlab(p), int(ctor == CENTRE_CTOR), int(dtor == CENTRE_DTOR), size, count])


def stub_msg_get_type(e):
    """CMsgDeath::GetType, reached through the test message's vtable: the type is the case's."""
    L = e.cl
    L.rec("GetType", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("type", 0) & MASK)


STUBS.internal(GET_THING_ID, stub_get_thing_id)
STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(GET_SCRIPT_POINTER, stub_get_script_pointer)
STUBS.internal(GET_VAR, stub_getvar)
STUBS.internal(SET_VAR, stub_setvar)
STUBS.internal(SET_RIDE_SPEED, stub_set_ride_speed)
STUBS.internal(ACTIVATE_HOARDINGS, stub_activate)
STUBS.internal(DEACTIVATE_HOARDINGS, stub_deactivate)
STUBS.internal(SET_STATE, stub_set_state)
STUBS.internal(UNASSIGN_STAFF, stub_unassign)
STUBS.internal(IS_BROKEN_DOWN, predicate("IsBrokenDown", "IsBrokenDown"))
STUBS.internal(IS_CLOSED, predicate("IsClosed", "IsClosed"))
STUBS.internal(IS_CONDEMNED, predicate("IsCondemned", "IsCondemned"))
STUBS.internal(IS_FLAGGED, predicate("IsFlaggedForMaintenance", "IsFlaggedForMaintenance"))
STUBS.internal(REQUIRES_UPGRADE, predicate("RequiresUpgrade", "RequiresUpgrade"))
STUBS.internal(IS_BOQ_CONNECTED, predicate("IsBOQConnected", "IsBOQConnected"))
STUBS.internal(IS_COASTER, predicate("IsCoaster", "IsCoaster"))
STUBS.internal(IS_COASTER_USEABLE, predicate("IsCoasterUseable", "IsCoasterUseable"))
STUBS.internal(HAS_FREE_QUEUE_SPACE, predicate("HasFreeQueueSpace", "HasFreeQueueSpace"))
STUBS.internal(GET_BACK_OF_QUEUE, stub_get_back_of_queue)
STUBS.internal(SET_FROM_TEMP_U, set_from_temp(False))
STUBS.internal(SET_FROM_TEMP_S, set_from_temp(True))
STUBS.internal(POST_MESSAGE, stub_post_message)
STUBS.internal(OPERATOR_DELETE, stub_delete)
STUBS.internal(OPERATOR_NEW, stub_new)
STUBS.internal(CONSTRUCT_ARRAY, stub_construct_array)
STUBS.internal(MSG_DEATH_GET_TYPE, stub_msg_get_type)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, strlen ... (tpwtest.py 491 on)


# ---------------------------------------------------------------------------------------------------------------- the two sides

# op name -> (PowerPC entry, host wrapper name); pointer answers are compared as labels, void functions answer None
CALLS = {
    "is_openable": (IS_OPENABLE, "is_openable"),
    "is_closeable": (IS_CLOSEABLE, "is_closeable"),
    "close_object": (CLOSE_OBJECT, "close_object"),
    "reopen": (REOPEN, "reopen"),
    "is_running": (IS_RUNNING, "is_running"),
    "is_available": (IS_AVAILABLE, "is_available"),
    "request_upgrade": (REQUEST_UPGRADE, "request_upgrade"),
    "set_parent": (SET_PARENT, "set_parent"),
    "notify_blocked": (NOTIFY_BLOCKED, "notify_blocked"),
    "get_db_info": (GET_DB_INFO, "get_db_info"),
    "set_speed": (SET_SPEED, "set_speed"),
    "set_duration": (SET_DURATION, "set_duration"),
    "set_capacity": (SET_CAPACITY, "set_capacity"),
    "receive": (RECEIVE, "receive"),
}
VOID = {"close_object", "reopen", "set_parent", "notify_blocked", "set_speed", "set_duration", "set_capacity", "receive"}
POINTER_OPS = {"get_db_info", "ride_info"}

# host wrapper -> (return type, argument types)
HOST_FUNCS = {
    "addr": (C.c_uint64, [C.c_char_p]),
    "alloc": (C.c_uint64, [C.c_uint32]),
    "layout": (C.c_int32, [C.POINTER(C.c_int64)]),
    "lab": (C.c_int64, [C.c_uint64]),
    "set_centre": (None, [C.c_uint64]),
    "centre": (C.c_uint64, []),
    "set_anim": (None, [C.c_int32, C.c_uint64]),
    "ride_new": (C.c_uint64, [C.c_uint32]),
    "ride_slot": (None, [C.c_uint64, C.c_int32, C.c_int32]),
    "db_set": (None, [C.c_int32, C.c_uint32, C.c_uint64, C.POINTER(C.c_uint64)]),
    "db_read": (None, [C.POINTER(C.c_int64)]),
    "msg_make": (C.c_uint64, [C.c_uint32, C.c_uint64, C.c_uint32, C.c_uint32]),
    "msg_type": (C.c_int32, [C.c_uint64]),
    "msg_ctor": (C.c_uint64, [C.c_uint32, C.c_uint64, C.c_uint32]),
    "msg_destroy": (C.c_uint64, [C.c_uint32, C.c_uint64, C.c_int32]),
    "test_message": (C.c_uint64, [C.c_uint64, C.c_uint32, C.c_uint32]),
    "is_openable": (C.c_int32, [C.c_uint64]),
    "is_closeable": (C.c_int32, [C.c_uint64]),
    "close_object": (None, [C.c_uint64]),
    "reopen": (None, [C.c_uint64]),
    "is_running": (C.c_int32, [C.c_uint64]),
    "is_available": (C.c_int32, [C.c_uint64]),
    "request_upgrade": (C.c_int32, [C.c_uint64, C.c_int32]),
    "set_parent": (None, [C.c_uint64, C.c_uint32]),
    "get_parent": (C.c_uint32, [C.c_uint64]),
    "notify_blocked": (None, [C.c_uint64]),
    "get_db_info": (C.c_uint64, [C.c_uint64]),
    "set_speed": (None, [C.c_uint64, C.c_uint32]),
    "set_duration": (None, [C.c_uint64, C.c_uint8]),
    "set_capacity": (None, [C.c_uint64, C.c_uint8]),
    "receive": (None, [C.c_uint64, C.c_uint64]),
    "ride_info": (C.c_uint64, [C.c_uint32]),
}


class Side:
    """What the shared code needs from either memory: words, typed fields and the world's globals."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def put(self, a, width, value):
        self.write(a, struct.pack(self.fmt[0] + WIDTH_CODE[width], value & ((1 << (8 * width)) - 1)))

    def get(self, a, width):
        return struct.unpack(self.fmt[0] + WIDTH_CODE[width], self.read(a, width))[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log (once the blocks exist), memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = STUBS.activate(e)
        e.oop = self                    # the stubs reach the case's blocks through the emulator

    def begin(self, labels):
        """The log, with a label for every block the log may name (labels: label -> address)."""
        fixed = {address: label for label, address in labels.items() if address}
        self.L = self.e.cl = CL.PpcLog(fixed, {key: list(values) for key, values in self.case["queues"].items()})

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def label(self, address):
        return self.L.lab(address & MASK)

    def script(self):
        self.script_block = self.alloc(16)
        return self.script_block

    def set_anim(self, index, address):
        self.words(ADDR["anims"] + 4 * index, [address])

    def set_centre(self, address):
        self.words(ADDR["centre"], [address])

    def get_centre(self):
        return self.rd_word(ADDR["centre"])

    def ride_new(self, ident):
        record = self.alloc(RIDE_SIZE)
        self.write(record, bytes([0xA5]) * RIDE_SIZE)
        self.put(record, 2, ident)
        return record

    def ride_slot(self, record, slot, value):
        self.put(record + 0x0C + 4 * slot, 4, value)

    def db_set(self, count, cached_id, cached, rides):
        base = ADDR["db"]
        self.words(base, rides)
        self.words(base + DB_COUNT, [count])
        self.put(base + DB_CACHED_ID, 2, cached_id)
        self.words(base + DB_CACHED_RIDE, [cached])

    def db_get(self):
        """[count, cached id, cached record, the six record pointers]."""
        base = ADDR["db"]
        return ([s32(self.rd_word(base + DB_COUNT)), self.get(base + DB_CACHED_ID, 2), self.rd_word(base + DB_CACHED_RIDE)] +
                self.rd_words(base, RIDES))

    def test_message(self, flags, ident):
        """A message whose vtable slot 2 holds a TVector for CMsgDeath::GetType (the stub answers the case's type)."""
        tvector = self.alloc(8)
        self.words(tvector, [MSG_DEATH_GET_TYPE, 0])
        vtable = self.alloc(16)
        self.words(vtable, [0, 0, tvector, 0])
        message = self.alloc(16)
        self.write(message, struct.pack(">IIHH", vtable, flags & MASK, ident, 0xA5A5) + bytes([0xA5]) * 4)
        return message

    def call(self, op, args):
        entry = CALLS[op][0]
        r3 = T.call_ppc(self.e, entry, [a & MASK for a in args])[0]
        return None if op in VOID else r3 & MASK

    def ride_info(self, id_word):
        return T.call_ppc(self.e, GET_RIDE_INFO, [ADDR["db"], id_word & MASK])[0] & MASK

    def get_parent(self, obj):
        """GetUpgradeParent writes a halfword through the hidden pointer (r3); the rest of the slot must stay."""
        out = self.alloc(16)
        self.write(out, b"\xEE" * 16)
        T.call_ppc(self.e, GET_PARENT, [out, obj])
        return [self.get(out, 2), self.read(out + 2, 14) == b"\xEE" * 14]


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def begin(self, labels):
        fixed = {label: address for label, address in labels.items() if address}
        CL.host_begin(self.lib, fixed, {key: list(values) for key, values in self.case["queues"].items()})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_oop_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_oop_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, address):
        return self.lib.tpw_oop_lab(address)

    def script(self):
        return self.g("script")

    def set_anim(self, index, address):
        self.lib.tpw_oop_set_anim(index, address)

    def set_centre(self, address):
        self.lib.tpw_oop_set_centre(address)

    def get_centre(self):
        return self.lib.tpw_oop_centre()

    def ride_new(self, ident):
        return self.lib.tpw_oop_ride_new(ident)

    def ride_slot(self, record, slot, value):
        self.lib.tpw_oop_ride_slot(record, slot, s32(value))

    def db_set(self, count, cached_id, cached, rides):
        self.lib.tpw_oop_db_set(count, cached_id, cached, (C.c_uint64 * RIDES)(*rides))

    def db_get(self):
        out = (C.c_int64 * (3 + RIDES))()
        self.lib.tpw_oop_db_read(out)
        return list(out)

    def test_message(self, flags, ident):
        return self.lib.tpw_oop_test_message(self.alloc(32), flags & MASK, ident)

    def call(self, op, args):
        """ctypes converts each argument to the wrapper's type (pointers stay 64-bit, words and bytes wrap)."""
        function = getattr(self.lib, "tpw_oop_" + CALLS[op][1])
        r = function(*args)
        if op in VOID:
            return None
        return r if op in POINTER_OPS else r & MASK

    def ride_info(self, id_word):
        return self.lib.tpw_oop_ride_info(id_word & MASK)

    def get_parent(self, obj):
        return [self.lib.tpw_oop_get_parent(obj) & 0xFFFF, True]


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    for name, (restype, argtypes) in HOST_FUNCS.items():
        function = getattr(lib, "tpw_oop_" + name)
        function.restype = restype
        function.argtypes = argtypes
    out = (C.c_int64 * 32)()
    n = lib.tpw_oop_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def write_slots(s, record, values):
    """The five value slots the unit reads, of one ride record (the other slots keep the 0xa5 fill)."""
    for slot, value in zip(SLOTS, values):
        s.ride_slot(record, slot, value)


def build(s, case):
    """Writes the case's object, script, anim sets, message centre, ride database and message through Side: the same code for both memories."""
    w = {}
    w["block"] = s.alloc(OBJ_SIZE + 2 * GUARD)
    w["obj"] = w["block"] + GUARD
    s.obj = w["obj"]
    s.write(w["block"], bytes([GUARD_BYTE]) * GUARD + bytes([case["fill"]]) * OBJ_SIZE + bytes([GUARD_BYTE]) * GUARD)
    for name, offset, width in FIELDS:
        s.put(w["obj"] + offset, width, case["obj"][name])
    w["script"] = s.script()
    w["anims"] = []
    for i in range(ANIMS):
        address = s.alloc(16) if case["anims"][i] else 0
        s.set_anim(i, address)
        w["anims"].append(address)
    w["centre"] = 0
    if case["centre"]:
        w["centre"] = s.alloc(CENTRE_SIZE)
        s.write(w["centre"], bytes([0xA5]) * CENTRE_SIZE)
    s.set_centre(w["centre"])
    db = case["db"]
    w["rides"] = []
    for i in range(RIDES):
        record = s.ride_new(db["ids"][i])
        write_slots(s, record, db["vals"][i])
        w["rides"].append(record)
    w["decoy"] = s.ride_new(db["decoy_id"])
    write_slots(s, w["decoy"], db["decoy_vals"])
    cached = db["cached"]
    cached_address = 0 if cached is None else w["decoy"] if cached == "decoy" else w["rides"][cached]
    s.db_set(db["count"], db["cached_id"], cached_address, w["rides"])
    w["msg"] = s.test_message(case["msg_flags"], case["msg_id"]) if case["op"] == "receive" else 0
    labels = {L_OBJ: w["obj"], L_SCRIPT: w["script"], L_CENTRE: w["centre"], L_DECOY: w["decoy"], L_MSG: w["msg"]}
    for i in range(ANIMS):
        labels[L_ANIM + i] = w["anims"][i]
    for i in range(RIDES):
        labels[L_RIDE + i] = w["rides"][i]
    s.begin(labels)
    return w


def execute(s, case, w):
    """The call."""
    op = case["op"]
    if op == "get_parent":
        return s.get_parent(w["obj"])
    if op == "ride_info":
        return s.ride_info(case["args"][0])
    if op == "receive":
        return s.call(op, [w["obj"], w["msg"]])
    return s.call(op, [w["obj"]] + list(case["args"]))


def masked(data):
    """The object block with the compared fields zeroed: what is left must be the fill and the guards, untouched."""
    out = bytearray(data)
    for _name, offset, width in FIELDS:
        out[GUARD + offset:GUARD + offset + width] = bytes(width)
    return bytes(out)


def observe(s, case, w, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels. The log is read last."""
    out = {}
    out["fields"] = {name: s.get(w["obj"] + offset, width) for name, offset, width in FIELDS}
    out["bytes"] = masked(s.read(w["block"], OBJ_SIZE + 2 * GUARD))
    db = s.db_get()
    out["db"] = [db[0], db[1] & 0xFFFF] + [s.label(address) for address in db[2:]]
    out["centre"] = s.label(s.get_centre())
    out["answer"] = s.label(answer) if case["op"] in POINTER_OPS else answer
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, case, w, execute(s, case, w))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the messages


def msg_block(s, case):
    """A guarded block for one message: the message starts 0x10 bytes in; everything is the case's fill byte."""
    block = s.alloc(0x50)
    s.write(block, bytes([case["fill"]]) * 0x50)
    return block, block + 0x10


def msg_result(s, case, block, msg, size, extra):
    """What a message test compares: the head and tail around the message untouched, plus the test's own values."""
    fill = bytes([case["fill"]])
    out = dict(extra)
    out["head_ok"] = s.read(block, 0x10) == fill * 0x10
    out["tail_ok"] = s.read(msg + size, 0x40 - size) == fill * (0x40 - size)
    out["log"] = s.log()
    return out


def ctor_ppc(e, case):
    s = Ppc(e, case)
    try:
        block, msg = msg_block(s, case)
        s.begin({L_MSG: msg})
        name, vtable, ctor, gettype, dtor, mtype = MESSAGES[case["kind"]]
        r3 = T.call_ppc(e, ctor, [msg, case["word"] & MASK])[0] & MASK
        stored = s.rd_word(msg)
        code = s.rd_word(s.rd_word(stored + 8))                 # GetType, found through the vtable's slot 2 (a TVector)
        extra = {
            "ret_this": r3 == msg,
            "vtable_ok": stored == vtable,
            "type": T.call_ppc(e, code, [msg])[0] & MASK,
            "flags": s.rd_word(msg + 4),
            "id": s.get(msg + 8, 2),
        }
        return msg_result(s, case, block, msg, 0xC, extra)
    finally:
        s.done()


def ctor_host(lib, case):
    s = Host(lib, case)
    try:
        block, msg = msg_block(s, case)
        s.begin({L_MSG: msg})
        r = lib.tpw_oop_msg_ctor(case["kind"], msg, case["word"] & MASK)
        extra = {
            "ret_this": r == msg,
            "vtable_ok": True,
            "type": lib.tpw_oop_msg_type(msg) & MASK,
            "flags": s.rd_word(msg + 8),
            "id": s.get(msg + 0xC, 2),
        }
        return msg_result(s, case, block, msg, 0x10, extra)
    finally:
        s.done()


def dtor_ppc(e, case):
    s = Ppc(e, case)
    try:
        block, msg = msg_block(s, case)
        s.write(msg, struct.pack(">IIH", BASE_VTABLE, case["flags"] & MASK, case["id"]))
        s.begin({L_MSG: msg})
        name, vtable, ctor, gettype, dtor, mtype = MESSAGES[case["kind"]]
        r3 = T.call_ppc(e, dtor, [msg, T.S16(case["flag"])])[0] & MASK
        extra = {
            "ret_this": r3 == msg,
            "vtable_ok": s.rd_word(msg) == vtable,
            "flags": s.rd_word(msg + 4),
            "id": s.get(msg + 8, 2),
        }
        return msg_result(s, case, block, msg, 0xC, extra)
    finally:
        s.done()


def dtor_host(lib, case):
    s = Host(lib, case)
    try:
        block, msg = msg_block(s, case)
        lib.tpw_oop_msg_make(case["kind"], msg, case["flags"] & MASK, case["id"])
        s.begin({L_MSG: msg})
        r = lib.tpw_oop_msg_destroy(case["kind"], msg, T.s16(case["flag"]))
        extra = {
            "ret_this": r == msg,
            "vtable_ok": True,
            "flags": s.rd_word(msg + 8),
            "id": s.get(msg + 0xC, 2),
        }
        return msg_result(s, case, block, msg, 0x10, extra)
    finally:
        s.done()


def gettype_ppc(e, case):
    s = Ppc(e, case)
    try:
        block, msg = msg_block(s, case)
        s.begin({L_MSG: msg})
        name, vtable, ctor, gettype, dtor, mtype = MESSAGES[case["kind"]]
        return {"type": T.call_ppc(e, gettype, [msg])[0] & MASK, "log": s.log()}
    finally:
        s.done()


def gettype_host(lib, case):
    s = Host(lib, case)
    try:
        block, msg = msg_block(s, case)
        lib.tpw_oop_msg_make(case["kind"], msg, case["flags"] & MASK, case["id"])
        s.begin({L_MSG: msg})
        return {"type": lib.tpw_oop_msg_type(msg) & MASK, "log": s.log()}
    finally:
        s.done()


def msg_case(r, kind, **over):
    c = {"kind": kind, "fill": r.choice(FILLS), "flags": rw(r), "id": r.randrange(0x10000), "flag": 1, "word": r.randrange(1 << 32),
         "queues": {}}
    c.update(over)
    return c


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100, 0x10000, 0xFFFF, 0x8000]
ID_POOL = [0, 1, 2, 3, 0x7FFF, 0x8000, 0xFFFF, 0x1234]
NONZERO = [1, 1, 1, 2, 0x100, -0x80000000, -1, 0x7FFFFFFF, 0x10000]
TRACKS = [0, 1, 1, 2, 2, 3, 4, -1, 0x7FFFFFFF, -0x80000000]
WRAPS = [(0x7FFFFFFF, 1), (0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, -1), (0x40000000, 0x40000000), (-5, -0x80000000), (-0x80000000, -0x80000000),
         (3, -3), (0, 0)]
# the answers of the fakes when the case does not say otherwise: a ride that is fine
PASS = dict(IsBrokenDown=[0], IsClosed=[0], IsCondemned=[0], IsFlaggedForMaintenance=[0], RequiresUpgrade=[0], IsBOQConnected=[1], IsCoaster=[0],
            IsCoasterUseable=[1], HasFreeQueueSpace=[1], GetBackOfQueue=[1], var=[0], type=[0], newnull=[0])


def rw(r):
    """A 32-bit word: now and then a special value."""
    return r.choice(SPECIAL + [r.randrange(1 << 32), r.randrange(1 << 32), r.randrange(1 << 32)])


def pick_id(r):
    return r.choice(ID_POOL) if r.random() < 0.6 else r.randrange(0x10000)


def queues(**answers):
    """The pass answers with the given ones replaced (a list is the queue, a number answers every call)."""
    q = {key: list(values) for key, values in PASS.items()}
    for key, value in answers.items():
        q[key] = list(value) if isinstance(value, (list, tuple)) else [value]
    return q


def qlist(r, p_zero, nonzero):
    """A queue of one answer, or of two (the second call answers differently): zero with the given probability."""
    return [0 if r.random() < p_zero else r.choice(nonzero) for _ in range(1 if r.random() < 0.75 else 2)]


def random_queues(r):
    return {
        "IsBrokenDown": qlist(r, 0.75, [1]),
        "IsClosed": qlist(r, 0.7, [1]),
        "IsCondemned": qlist(r, 0.8, NONZERO),
        "IsFlaggedForMaintenance": qlist(r, 0.8, NONZERO),
        "RequiresUpgrade": qlist(r, 0.6, NONZERO),
        "IsBOQConnected": qlist(r, 0.15, NONZERO),
        "IsCoaster": qlist(r, 0.6, NONZERO),
        "IsCoasterUseable": qlist(r, 0.4, NONZERO),
        "HasFreeQueueSpace": qlist(r, 0.15, NONZERO),
        "GetBackOfQueue": qlist(r, 0.15, [1, 0xFFFF, 0x8000, 0x1234]),
        "var": [r.choice([-1, 0, 1, 2, 5, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000)])],
        "type": [0],
        "newnull": qlist(r, 0.8, [1]),
    }


def range_pair(r):
    """A (minimum, maximum) pair of record values: ordinary, reversed, with a sum that is not positive, wrapping, or random."""
    k = r.random()
    if k < 0.5:
        low = r.randrange(0, 20)
        return (low, low + r.choice([r.randrange(0, 40), r.randrange(0, 40), 300, 1000]))
    if k < 0.65:
        low = r.randrange(5, 300)
        return (low, r.randrange(0, low))
    if k < 0.8:
        return (-r.randrange(0, 40), r.randrange(0, 40))
    if k < 0.9:
        return r.choice(WRAPS)
    return (s32(r.randrange(1 << 32)), s32(r.randrange(1 << 32)))


def ride_values(r):
    """The five slots the unit reads: WhichTrackType, the capacity pair, the duration pair."""
    return (r.choice(TRACKS),) + range_pair(r) + range_pair(r)


def make_db(r, mode):
    """A ride database and the object type to look up. mode "hit": the lookup lands on a record (through the table or the cache);
    "any": the type may be unknown (the callers do not read the record)."""
    ids = [pick_id(r) for _ in range(RIDES)]
    db = {
        "ids": ids,
        "vals": [ride_values(r) for _ in range(RIDES)],
        "decoy_id": pick_id(r),
        "decoy_vals": ride_values(r),
        "count": r.choice([-3, -1, 0, 1, 2, 3, 4, 5, 6, 6, 6]),
        "cached_id": r.choice([pick_id(r), r.choice(ids), r.choice(ids)]),
        "cached": r.choice([None, "decoy", r.randrange(RIDES), r.randrange(RIDES)]),
    }
    typ = pick_id(r)
    if mode == "hit":
        if r.random() < 0.75:
            if db["count"] < 1:
                db["count"] = r.randrange(1, RIDES + 1)
            typ = ids[r.randrange(db["count"])]
            if db["cached_id"] == typ and db["cached"] is None:
                db["cached"] = r.choice(["decoy", r.randrange(RIDES)])
        else:
            typ = db["cached_id"]
            if db["cached"] is None:
                db["cached"] = r.choice(["decoy", r.randrange(RIDES)])
    else:
        typ = r.choice([typ, db["cached_id"], r.choice(ids)])
    return db, typ


def effective_values(case):
    """The five slot values of the record the lookup of the object's type lands on."""
    db = case["db"]
    typ = case["obj"]["type"]
    if typ == db["cached_id"]:
        return db["decoy_vals"] if db["cached"] == "decoy" else db["vals"][db["cached"]]
    for i in range(max(db["count"], 0)):
        if db["ids"][i] == typ:
            return db["vals"][i]
    return None


def set_track(case, track):
    """WhichTrackType of every record (and the decoy)."""
    db = case["db"]
    db["vals"] = [(track,) + v[1:] for v in db["vals"]]
    db["decoy_vals"] = (track,) + db["decoy_vals"][1:]


def rand_fields(r):
    flags = r.randrange(0x10000)
    flags = (flags | 4) if r.random() < 0.7 else (flags & ~4)
    return {
        "thing": r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0x10000)]),
        "type": 0,
        "f1c": r.randrange(ANIMS),
        "f20": rw(r),
        "f28": r.choice([0, 0, 1, 2, 0x100, 0x80000000, 0xFFFFFFFF, r.randrange(1 << 32)]),
        "f2c": r.randrange(0x10000),
        "flags": flags,
        "f54": rw(r),
        "f58": r.randrange(256),
        "f59": r.randrange(256),
        "f5a": r.choice([0, 0xFFFF, r.randrange(0x10000)]),
        "f64": r.choice([0, 1, 2, 0x80000000, 0xFFFFFFFF, r.randrange(1 << 32)]),
        "loaded": r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0x10000)]),
        "h6c": rw(r), "hf4": rw(r), "h1a4": rw(r), "h22c": rw(r), "h2b4": rw(r), "h33c": rw(r),
    }


def new_case(r, op, mode="any", args=()):
    """The world every case starts from: random object, database, anim table, centre and fake answers."""
    db, typ = make_db(r, mode)
    obj = rand_fields(r)
    obj["type"] = typ
    return {
        "op": op,
        "fill": r.choice(FILLS),
        "obj": obj,
        "db": db,
        "centre": r.choice([0, 1]),
        "anims": [r.random() < 0.85 for _ in range(ANIMS)],
        "queues": random_queues(r),
        "args": list(args),
        "msg_flags": rw(r),
        "msg_id": r.randrange(0x10000),
    }


def simple(op, seed, mode="any", args=None, edge_args=()):
    """Edge and random generators of a function of the object and at most one argument: the edges run the argument edges over random worlds."""
    def edges():
        r = random.Random(seed)
        for value in edge_args:
            yield new_case(r, op, mode, [] if value is None else [value])
        for _ in range(8):
            yield new_case(r, op, mode, [] if args is None else [args(r)])

    def rnd(r):
        return new_case(r, op, mode, [] if args is None else [args(r)])
    return edges, rnd


# ---- IsOpenable and Reopen: every combination of the chain's five bad answers and the three coaster states, the wide values in turn
def chain_edges(op, seed):
    def edges():
        r = random.Random(seed)
        i = 0
        for bits in range(32):
            for coaster, usable in ((0, 1), (1, 1), (1, 0)):
                bad = NONZERO[i % len(NONZERO)]
                i += 1
                c = new_case(r, op)
                c["queues"] = queues(
                    IsBrokenDown=bits & 1,
                    IsCondemned=bad if bits & 2 else 0,
                    IsFlaggedForMaintenance=bad if bits & 4 else 0,
                    RequiresUpgrade=bad if bits & 8 else 0,
                    IsBOQConnected=0 if bits & 16 else bad,
                    IsCoaster=bad if coaster else 0,
                    IsCoasterUseable=bad if usable else 0,
                )
                yield c
    return edges


def chain_random(op):
    def rnd(r):
        return new_case(r, op)
    return rnd


# ---- CloseObject: the loaded person, the thing id, the anim set (present or NULL) and the script handle
def close_edges():
    r = random.Random(203)
    for loaded in (0, 1, 0x8000, 0xFFFF):
        for f1c in (0, 3, 7):
            c = new_case(r, "close_object")
            c["obj"]["loaded"] = loaded
            c["obj"]["f1c"] = f1c
            c["anims"][f1c] = bool(f1c % 2)
            yield c


# ---- IsRunning: the script's variable 9
def running_edges():
    r = random.Random(205)
    for value in (-0x80000000, -2, -1, 0, 1, 2, 0x7FFFFFFF):
        c = new_case(r, "is_running")
        c["queues"]["var"] = [value]
        yield c


# ---- IsAvailableToChoose: every combination of the first six exits, then the record's track type against mF28 and the coaster states
def available_edges():
    r = random.Random(206)
    for bits in range(64):
        c = new_case(r, "is_available", "hit")
        c["obj"]["flags"] = (c["obj"]["flags"] & ~4) | (0 if bits & 1 else 4)
        c["queues"] = queues(
            IsBrokenDown=(bits >> 1) & 1,
            IsClosed=(bits >> 2) & 1,
            IsCondemned=NONZERO[bits % len(NONZERO)] if bits & 8 else 0,
            GetBackOfQueue=0 if bits & 16 else [1, 0xFFFF, 0x8000][bits % 3],
            HasFreeQueueSpace=0 if bits & 32 else NONZERO[bits % len(NONZERO)],
        )
        yield c
    for track in (0, 1, 2, 3, -1):
        for f28 in (0, 1, 0x100, 0x80000000):
            for coaster, usable in ((0, 1), (1, 1), (1, 0)):
                c = new_case(r, "is_available", "hit")
                c["obj"]["flags"] |= 4
                c["obj"]["f28"] = f28
                set_track(c, track)
                c["queues"] = queues(IsCoaster=coaster, IsCoasterUseable=usable)
                yield c


# ---- RequestUpgrade: asking and withdrawing against the ride's flag bit and the answers of RequiresUpgrade (and of the open test inside Reopen)
def upgrade_edges():
    r = random.Random(207)
    for on in (1, 0, -1, 2, 0x100, 0x10000, -0x80000000):
        for flag4 in (0, 4):
            for requires in ([0], [1], [0x100], [1, 0], [1, 1], [-0x80000000], [1, 1, 0]):
                c = new_case(r, "request_upgrade", "any", [on])
                c["obj"]["flags"] = (c["obj"]["flags"] & ~4) | flag4
                c["queues"]["RequiresUpgrade"] = requires
                yield c


def upgrade_random(r):
    return new_case(r, "request_upgrade", "any", [r.choice([0, 0, 1, 1, -1, 2, rw(r) & MASK])])


# ---- SetUpgradeParent and GetUpgradeParent: the id is the high halfword of the register
def parent_word(r):
    return (r.choice(ID_POOL + [r.randrange(0x10000)]) << 16) | r.choice([0, 0xFFFF, 0x1234, r.randrange(0x10000)])


# ---- NotifyExitBlocked: closed or not, a centre or none, the allocation failing or not
def notify_edges():
    r = random.Random(210)
    for closed in (0, 1):
        for centre in (0, 1):
            for newnull in (0, 1):
                for thing in (0, 0x1234, 0xFFFF):
                    c = new_case(r, "notify_blocked")
                    c["centre"] = centre
                    c["obj"]["thing"] = thing
                    c["queues"]["IsClosed"] = [closed]
                    c["queues"]["newnull"] = [newnull]
                    yield c


# ---- GetDBInfo: a type the table has, the cache has, or neither
def db_info_edges():
    r = random.Random(211)
    for mode in ("hit", "any", "any"):
        for _ in range(8):
            yield new_case(r, "get_db_info", mode)


# ---- SetOperatingSpeed
def speed_edges():
    r = random.Random(212)
    for value in SPECIAL + [3, 5, 0x7F, 0x80, 0xFF, 0x100]:
        yield new_case(r, "set_speed", "any", [value])


def speed_random(r):
    return new_case(r, "set_speed", "any", [rw(r)])


# ---- SetOperatingDuration and SetOperatingCapacity: the byte at, around and beyond the record's two bounds
def clamp_case(r, op, low_slot):
    c = new_case(r, op, "hit")
    return c, effective_values(c)[low_slot:low_slot + 2]


def clamp_edges(op, seed, low_slot):
    def edges():
        r = random.Random(seed)
        for _ in range(40):
            c, (low, high) = clamp_case(r, op, low_slot)
            for value in (0, 1, 255, low - 1, low, low + 1, high - 1, high, high + 1):
                d = dict(c)
                d["args"] = [value & 0xFF]
                yield d
    return edges


def clamp_random(op, low_slot):
    def rnd(r):
        c, (low, high) = clamp_case(r, op, low_slot)
        c["args"] = [r.choice([r.randrange(256), low & 0xFF, high & 0xFF, (low - 1) & 0xFF, (high + 1) & 0xFF, 0, 255])]
        return c
    return rnd


# ---- ReceiveMessage: the death of the assigned staff member, of another, the month end and every other type
TYPES = [0x1B, 0x1B, 0x1B, 0x0B, 0x0B, 0, 1, 0x0A, 0x0C, 0x1A, 0x1C, 0x7FFFFFFF, -1, -0x80000000, 0x10B, 0x1B00]


def receive_case(r, typ, same=None):
    c = new_case(r, "receive")
    c["queues"]["type"] = [typ]
    if same is None:
        same = r.random() < 0.5
    c["msg_id"] = c["obj"]["f5a"] if same else (c["obj"]["f5a"] + r.choice([1, 0x8000, 0xFFFF, r.randrange(1, 0x10000)])) & 0xFFFF
    return c


def receive_edges():
    r = random.Random(214)
    for typ in TYPES:
        for same in (True, False):
            yield receive_case(r, typ, same)


def receive_random(r):
    return receive_case(r, r.choice(TYPES))


# ---- CRideDatabase::GetRideInfo: found first, in the middle, last, duplicated or not at all; the cache hit by record, decoy and NULL; any count
def lookup_case(r, count, position, cache):
    """position: the record index the id asks for, or None for an id the table does not have; cache: how the cache starts."""
    c = new_case(r, "ride_info")
    db = c["db"]
    db["ids"] = [0x100 + i * 3 for i in range(RIDES)]
    db["count"] = count
    ident = 0x7777 if position is None else db["ids"][position]
    db["cached_id"], db["cached"] = {
        "cold": (0x6666, None),
        "hit_record": (ident, (position or 0)),
        "hit_decoy": (ident, "decoy"),
        "hit_null": (ident, None),
        "other_record": (0x6666, 2),
    }[cache]
    c["args"] = [(ident << 16) | r.choice([0, 0, 0xFFFF, 0x1234])]
    return c


def lookup_edges():
    r = random.Random(215)
    for count in (-1, 0, 1, 3, 6):
        for position in (0, 2, 5, None):
            for cache in ("cold", "hit_record", "hit_decoy", "hit_null", "other_record"):
                yield lookup_case(r, count, position, cache)
    for _ in range(12):
        c = new_case(r, "ride_info")
        db = c["db"]
        db["ids"][3] = db["ids"][0]
        db["count"] = 6
        c["args"] = [db["ids"][0] << 16]
        yield c


def lookup_random(r):
    c = new_case(r, "ride_info")
    db = c["db"]
    ident = r.choice([c["obj"]["type"], db["cached_id"], r.choice(db["ids"]), pick_id(r)])
    c["args"] = [(ident << 16) | r.choice([0, 0, 0xFFFF, r.randrange(0x10000)])]
    return c


# ---- the messages
ID_WORDS = [0, 0xFFFF0000, 0x00010000, 0x12345678, 0x0000FFFF, 0x80000000, 0xFFFFFFFF, 0x7FFF8000, 0x00008000, 0x1234FFFF]
FLAGS = (-0x80000000, -0x8000, -1, 0, 1, 2, 0x7FFF, 0x10000, 0x18000, 0x1FFFF)


def ctor_random(kind):
    def rnd(r):
        return msg_case(r, kind, word=r.choice([r.randrange(1 << 32)] * 3 + ID_WORDS))
    return rnd


def dtor_random(kind):
    def rnd(r):
        return msg_case(r, kind, flag=r.choice([s32(r.randrange(1 << 32)), -1, 0, 1, 2, 0x10000, 0x1FFFF]))
    return rnd


def gettype_random(kind):
    def rnd(r):
        return msg_case(r, kind)
    return rnd


def ctor_edges(kind):
    def edges():
        r = random.Random(500 + kind)
        for i in range(20):
            yield msg_case(r, kind, word=ID_WORDS[i % len(ID_WORDS)], fill=FILLS[(i // len(ID_WORDS)) % len(FILLS)])
    return edges


def dtor_edges(kind):
    def edges():
        r = random.Random(600 + kind)
        for i in range(20):
            yield msg_case(r, kind, flag=FLAGS[i % len(FLAGS)], fill=FILLS[(i // len(FLAGS)) % len(FILLS)])
    return edges


def gettype_edges(kind):
    def edges():
        r = random.Random(700 + kind)
        for _ in range(20):
            yield msg_case(r, kind)
    return edges


def parent_edges(name, seed):
    def edges():
        r = random.Random(seed)
        for _ in range(20):
            yield new_case(r, name)
    return edges


def parent_args_edges():
    r = random.Random(208)
    for ident in ID_POOL:
        for junk in (0, 0xFFFF, 0x1234):
            yield new_case(r, "set_parent", "any", [(ident << 16) | junk])


def parent_args_random(r):
    return new_case(r, "set_parent", "any", [parent_word(r)])


def object_only(op, seed, count=20):
    """A function of the object alone: a fixed number of random worlds, no random cases."""
    def edges():
        r = random.Random(seed)
        for _ in range(count):
            yield new_case(r, op)
    return edges


def rnd_of(op, mode="any"):
    def rnd(r):
        return new_case(r, op, mode)
    return rnd


TESTS = [
    FnTest("CObject::IsOpenable", ppc, host, edge=chain_edges("is_openable", 201), rnd=chain_random("is_openable")),
    FnTest("CObject::IsCloseable", ppc, host, edge=object_only("is_closeable", 202), rnd=rnd_of("is_closeable")),
    FnTest("CObject::CloseObject", ppc, host, edge=close_edges, rnd=rnd_of("close_object")),
    FnTest("CObject::Reopen", ppc, host, edge=chain_edges("reopen", 204), rnd=chain_random("reopen")),
    FnTest("CObject::IsRunning", ppc, host, edge=running_edges, rnd=rnd_of("is_running")),
    FnTest("CObject::IsAvailableToChoose", ppc, host, edge=available_edges, rnd=rnd_of("is_available", "hit")),
    FnTest("CObject::RequestUpgrade", ppc, host, edge=upgrade_edges, rnd=upgrade_random),
    FnTest("CObject::SetUpgradeParent", ppc, host, edge=parent_args_edges, rnd=parent_args_random),
    FnTest("CObject::GetUpgradeParent", ppc, host, edge=parent_edges("get_parent", 209), rnd=rnd_of("get_parent")),
    FnTest("CObject::NotifyExitBlocked", ppc, host, edge=notify_edges, rnd=rnd_of("notify_blocked")),
    FnTest("CObject::GetDBInfo", ppc, host, edge=db_info_edges, rnd=rnd_of("get_db_info")),
    FnTest("CObject::SetOperatingSpeed", ppc, host, edge=speed_edges, rnd=speed_random),
    FnTest("CObject::SetOperatingDuration", ppc, host, edge=clamp_edges("set_duration", 213, 3), rnd=clamp_random("set_duration", 3)),
    FnTest("CObject::SetOperatingCapacity", ppc, host, edge=clamp_edges("set_capacity", 216, 1), rnd=clamp_random("set_capacity", 1)),
    FnTest("CObject::ReceiveMessage", ppc, host, edge=receive_edges, rnd=receive_random),
    FnTest("CRideDatabase::GetRideInfo", ppc, host, edge=lookup_edges, rnd=lookup_random),
]
for _kind, (_name, _vtable, _ctor, _gettype, _dtor, _type) in enumerate(MESSAGES):
    if _ctor:
        TESTS.append(FnTest(_name + "::" + _name, ctor_ppc, ctor_host, edge=ctor_edges(_kind), rnd=ctor_random(_kind)))
    TESTS.append(FnTest(_name + "::GetType", gettype_ppc, gettype_host, edge=gettype_edges(_kind), rnd=gettype_random(_kind)))
    TESTS.append(FnTest(_name + "::~" + _name, dtor_ppc, dtor_host, edge=dtor_edges(_kind), rnd=dtor_random(_kind)))
