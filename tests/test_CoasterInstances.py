"""Unit w186, the coaster instances, against libtpw_unit_CoasterInstances.dylib (tests/unit_CoasterInstances.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp, src/game/rides/CoasterType.cpp, src/game/rides/TPCoaster.cpp:
  SetCurrentEditCoaster 0x1003aca0, GetIntervalForHeight 0x10040e10, GetIntervalForHeightDelta 0x10040e64, GetHeightForInterval 0x10040eb8,
  FlagGenerateTrack 0x10040f08, FlagCollisionUpdateTrackAroundBrace 0x10040fa4, XZAngleBetweenPylons 0x10041018,
  GetPotentialPylonPositionArea 0x10041170, DestroyEndPylons 0x10042728, EndTrack 0x100427d0, SetCoasterState 0x10042bb8,
  IsCoasterType 0x100432dc, IsCoaster 0x10043330, FindCoasterInstanceFromEntranceID 0x10043380, SetCoasterCObject 0x10043480,
  InstanceCoaster 0x100434ec, SetCoasterScriptID 0x1004373c, ui_GetNumCoasterTracks 0x100458ac, CCoasterBalance::~CCoasterBalance 0x10039b30

A case is a world and a call. The world is built twice by ONE piece of Python code from a description (a list of objects with named fields and the
globals): in emulator memory at the Mac's offsets (big-endian words, 4-byte pointers) for the original, and in the host library's memory for the
translation (little-endian, 8-byte pointers, the offsets from tpw_ci_layout); the same code reads both back (pointers as labels, floats with the
NaNs made one). Every callee outside the unit is a recording fake on both sides (names and arguments identical); the fakes that run inside the
unit's loops can also change the world on the case's queues (the edit coaster switches to the instance's next, the trains' base and count
change, a deleted brace loses its next) to show which words the original reads again.

What is compared: the answer, the log, every object's fields, the globals the functions write (the edit coaster, the list head, the count,
the four pylon words) and the new instance's fields. Fused operations: the single-precision fmadds are corrected by ROUNDING (GetHeightForInterval,
XZAngleBetweenPylons); the double fmadd and sqrt/acos are the C library's on both sides.
Not tested: InstanceCoaster with a ride type number no coaster type has (the original reads a NULL type: a crash) and its answer when the flag is
set (the Mac answers its register 25, the host 0); DestroyEndPylons with no plain brace after the end pylons (a NULL read); a train count above
4 (the blocks hold four records).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CoasterInstances"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN = dict(
    SetCurrentEditCoaster=0x1003ACA0, GetIntervalForHeight=0x10040E10, GetIntervalForHeightDelta=0x10040E64, GetHeightForInterval=0x10040EB8,
    FlagGenerateTrack=0x10040F08, FlagCollisionUpdateTrackAroundBrace=0x10040FA4, XZAngleBetweenPylons=0x10041018,
    GetPotentialPylonPositionArea=0x10041170, DestroyEndPylons=0x10042728, EndTrack=0x100427D0, SetCoasterState=0x10042BB8,
    IsCoasterType=0x100432DC, IsCoaster=0x10043330, FindCoasterInstanceFromEntranceID=0x10043380, SetCoasterCObject=0x10043480,
    InstanceCoaster=0x100434EC, SetCoasterScriptID=0x1004373C, ui_GetNumCoasterTracks=0x100458AC, Dtor=0x10039B30)
SIZES = {0x1003ACA0: 12, 0x10040E10: 24, 0x10040E64: 20, 0x10040EB8: 20, 0x10040F08: 92, 0x10040FA4: 40, 0x10041018: 288, 0x10041170: 52,
         0x10042728: 104, 0x100427D0: 228, 0x10042BB8: 1072, 0x100432DC: 44, 0x10043330: 44, 0x10043380: 56, 0x10043480: 64,
         0x100434EC: 544, 0x1004373C: 100, 0x100458AC: 12, 0x10039B30: 100}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
RECALC, DELETEB, EDITSTATE, PANEL, PLACEEND = 0x10050F88, 0x10056660, 0x10053DE8, 0x1019C158, 0x100424F0
KICK, UNLOAD, ACTIVATE, DEACTIVATE, FLASH = 0x10049858, 0x100499E4, 0x1004A8A0, 0x1004AA70, 0x10052158
CAPACITY, SPEED, PLACEENT, TRAINS, RESTORE = 0x1004A328, 0x1004B94C, 0x10040ACC, 0x10042084, 0x100472C4
ALLOCOBJ, ALLOCQ, MALLOC, MFREE, UNINIT = 0x101EF1F4, 0x10049180, 0x10064BD4, 0x10064C54, 0x1001ED40

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_ci_addr(name)
ADDR = dict(instances=0x102B68B4, count=0x102B68AC, edit=0x102B68B0, typecount=0x102B68B8, types=0x102B68BC, rideanim=0x102C79A4,
            ridetypes=0x102D19B0, w689C=0x102B689C, w6898=0x102B6898, w6894=0x102B6894, w6890=0x102B6890, recording=0x1037EC44 + 0x24,
            otcount=0x102B730C, otfree=0x102B7308, otlowest=0x102B7304, otable=0x102B6F04)
WORDS = ("count", "typecount", "w689C", "w6898", "w6894", "w6890", "recording", "otcount", "otfree", "otlowest")
POINTERS = ("instances", "edit")
ARRAYS = {"types": 8, "rideanim": 13, "ridetypes": 13}

# ---- the structs: (field, Mac offset, kind) and the Mac size; kinds: q 32-bit pointer (Ptr32 in the arena, address zero), w word, f float (NaNs made one), p pointer (4 bytes on the Mac, 8 on the host)
MAC = {
    "inst": ([("mFlags", 0, "w"), ("mType", 4, "p"), ("mState", 8, "w"), ("mTypeID", 0xC, "w"), ("mEntranceID", 0x10, "w"),
              ("mObjectInstanceID", 0x14, "w"), ("mNext", 0x18, "p"), ("mX", 0x1C, "w"), ("mZ", 0x20, "w"), ("mScriptID", 0x24, "w"),
              ("mCObject", 0x28, "p"), ("mF2C", 0x2C, "f"), ("mBlock3C", 0x3C, "w"), ("mBraces", 0x58, "p"), ("mFAC", 0xAC, "f"),
              ("mTrainCount", 0xF4, "w"), ("mF118", 0x118, "f"), ("mF11C", 0x11C, "f"), ("mTrains", 0x128, "p"), ("mUVOffsets", 0x13C, "p")],
             0x14C),
    "brace": ([("mFlags", 8, "w"), ("mNext", 0x68, "p"), ("mData", 0x100, "p")], 0x104),
    "bdata": ([("mFlags", 4, "w")], 8),
    "type": ([("mFlags", 0, "w"), ("mRideTypeNumber", 4, "w"), ("mTextureCount", 0x20, "w"), ("mFA4", 0xA4, "f"), ("mF2F4", 0x2F4, "w"),
              ("mF2F8", 0x2F8, "w"), ("mF318", 0x318, "w")], 0x35C),
    "set": ([("mFlags", 4, "w"), ("mHeads", 8, "p")], 0x100),
    "mesh": ([("mHeader", 4, "p")], 0x40),
    "hdr": ([("mF78", 0x78, "q")], 0x100),
    "node": ([("m31", 0x44, "f")], 0x58),
    "pylon": ([("mFBC", 0xBC, "f"), ("mFC0", 0xC0, "f")], 0x100),
    "train": ([("mFlags", 0, "w")], 0x80),     # the train block's record: native pointers make the host's records longer than the Mac's 0x80
}
ORDER = ["inst", "brace", "bdata", "type", "set", "mesh", "hdr", "node", "pylon", "train"]
# ---- the host's offsets and sizes the module relies on, checked once against tpw_ci_layout (catches a header that moved); the last is
# sizeof(CCoasterBalance)
HOST_LAYOUT = [0, 8, 16, 20, 24, 28, 32, 40, 44, 48, 56, 64, 88, 120, 212, 300, 336, 340, 360, 392, 496, 8, 112, 264, 496, 4, 72, 0, 916, 40, 180, 828, 832, 848, 944, 8, 16, 392, 8, 112, 120, 184, 68, 160, 188, 192, 296, 0, 144, 30464]
HOST = {}
HOSTSIZE = {}
BALANCE_MAC = 0x3B80


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only (Unicorn rounds fused operations twice)."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def lab_edit(e):
    return e.cl.lab(e.u32(ADDR["edit"]))


def switch_edit(e, key):
    """The case's queue moves the edit coaster to its mNext."""
    if e.cl.ret(key) != 0 and e.u32(ADDR["edit"]) != 0:
        e.write(ADDR["edit"], e.u32(e.u32(ADDR["edit"]) + 0x18).to_bytes(4, "big"))


def stub_recalc(e):
    L, inst = e.cl, e.r(3)
    L.rec("RecalcTrackParams", (L.lab(inst), e.u32(inst), lab_edit(e)))
    switch_edit(e, "Edit:Recalc")


def stub_deleteb(e):
    L, inst, brace = e.cl, e.r(3), e.r(4)
    L.rec("DeleteTrackBrace", (L.lab(inst), L.lab(brace), e.u32(inst + 0x3C)))
    if L.ret("Poison") != 0:
        e.write(brace + 0x68, e.u32(inst + 0x28).to_bytes(4, "big"))


def stub_editstate(e):
    L = e.cl
    L.rec("SetCoasterEditState", (e.r(3) & MASK, lab_edit(e)))
    switch_edit(e, "Edit:State")


def stub_panel(e):
    e.cl.rec("CoasterPanelEndTrack", (lab_edit(e),))
    switch_edit(e, "Edit:Panel")


def stub_placeend(e):
    L = e.cl
    L.rec("PlaceEndPylons", (L.lab(e.r(3)), canon_f(fb(e.f(1))), lab_edit(e)))
    switch_edit(e, "Edit:Place")


def train_fake(name, swap, count, extra=lambda e: 0):
    def stub(e):
        L, inst, train = e.cl, e.r(3), e.r(4)
        L.rec(name, (L.lab(inst), L.lab(train), extra(e), e.u32(inst + 8), canon_f(e.u32(inst + 0x2C)), e.u32(train), e.u32(inst)))
        if L.ret(swap) != 0:
            e.write(inst + 0x128, e.u32(inst + 0x13C).to_bytes(4, "big"))
        n = L.ret(count, -1)
        if n >= 0:
            e.write(inst + 0xF4, (n & MASK).to_bytes(4, "big"))
    return stub


def stub_kick(e):
    L, inst = e.cl, e.r(3)
    L.rec("KickPeepsFromEntranceQueue", (L.lab(inst), e.u32(inst + 8)))


def stub_flash(e):
    L, inst = e.cl, e.r(3)
    L.rec("FlashEndPylons", (L.lab(inst), e.r(4) & MASK, e.u32(inst)))


def stub_capacity(e):
    L = e.cl
    L.rec("SetTrainCapacity", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK))


def stub_speed(e):
    L, inst = e.cl, e.r(3)
    L.rec("CoasterSetSpeed", (L.lab(inst), e.r(4) & MASK, s32(e.r(5)), e.u32(inst + 8), lab_edit(e)))


def stub_placeent(e):
    L = e.cl
    L.rec("PlaceEntrancePylons", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, canon_f(fb(e.f(1))), s32(e.r(7))))


def stub_trains(e):
    L = e.cl
    L.rec("InstanceCoasterTrains", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))


def stub_restore(e):
    L = e.cl
    L.rec("RestoreCoasterSounds", (L.lab(e.r(3)),))


def stub_allocobj(e):
    L = e.cl
    places = ((1 if e.r(3) == ADDR["otcount"] else 0) | (2 if e.r(4) == ADDR["otfree"] else 0) | (4 if e.r(5) == ADDR["otlowest"] else 0)
              | (8 if e.r(7) == ADDR["otable"] else 0))
    L.rec("AllocateObjectInstanceToTable", (places, e.r(6) & MASK, L.lab(e.r(8)), e.r(9) & MASK))
    e.set_r(3, L.ret("Alloc", 7) & MASK)


def stub_allocq(e):
    L, q = e.cl, e.r(3)
    head = e.u32(ADDR["instances"])
    which = 0 if q == head + 0xB0 else 1 if q == head + 0xD0 else -1
    L.rec("AllocateQueue", (L.lab(head), which, e.r(4) & MASK))


def stub_malloc(e):
    L, size = e.cl, e.r(3) & MASK
    L.rec("MReleaseAllocF", (size, e.r(4) & MASK))
    block = e.alloc(size, bytes(size))
    L.newlab(block)
    e.set_r(3, block)


def stub_mfree(e):
    L = e.cl
    L.rec("MReleaseFreeF", (L.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_delete(e):
    L = e.cl
    if L.lab(e.r(3)) > 0:
        L.rec("delete", (L.lab(e.r(3)),))
    L.forget(e.r(3))


def stub_uninit(e):
    L = e.cl
    L.rec("CBalanceTable::UnInit", (L.lab(e.r(3)),))


STUBS.internal(RECALC, stub_recalc)
STUBS.internal(DELETEB, stub_deleteb)
STUBS.internal(EDITSTATE, stub_editstate)
STUBS.internal(PANEL, stub_panel)
STUBS.internal(PLACEEND, stub_placeend)
STUBS.internal(KICK, stub_kick)
STUBS.internal(UNLOAD, train_fake("UnloadTrain", "Swap:Unload", "Count:Unload", lambda e: s32(e.r(5))))
STUBS.internal(ACTIVATE, train_fake("ActivateCoasterTrain", "Swap:Activate", "Count:Activate"))
STUBS.internal(DEACTIVATE, train_fake("DeactivateCoasterTrain", "Swap:Deactivate", "Count:Deactivate"))
STUBS.internal(FLASH, stub_flash)
STUBS.internal(CAPACITY, stub_capacity)
STUBS.internal(SPEED, stub_speed)
STUBS.internal(PLACEENT, stub_placeent)
STUBS.internal(TRAINS, stub_trains)
STUBS.internal(RESTORE, stub_restore)
STUBS.internal(ALLOCOBJ, stub_allocobj)
STUBS.internal(ALLOCQ, stub_allocq)
STUBS.internal(MALLOC, stub_malloc)
STUBS.internal(MFREE, stub_mfree)
STUBS.internal(UNINIT, stub_uninit)
STUBS.imp("__dl__FPv", stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's sqrt and acos (C semantics), operator delete


# ---------------------------------------------------------------------------------------------------------------- the two sides

FIRST_NEW = 500


class Side:
    """What the shared code needs from either memory."""

    def fld(self, st, name):
        return self.table[st][name]

    def size(self, st):
        return self.sizes[st]

    def put(self, a, st, name, v):
        off, kind = self.fld(st, name)
        if kind == "p":
            self.write(a + off, struct.pack(self.pfmt, v))
        elif kind == "q":
            self.write(a + off, struct.pack(self.wfmt, self.enc32(v) & MASK))
        else:
            self.write(a + off, struct.pack(self.wfmt, v & MASK))

    def get(self, a, st, name):
        off, kind = self.fld(st, name)
        if kind == "p":
            return self.lab(struct.unpack(self.pfmt, self.read(a + off, struct.calcsize(self.pfmt)))[0])
        v = struct.unpack(self.wfmt, self.read(a + off, 4))[0]
        if kind == "q":
            return self.lab(self.dec32(v))
        return canon_f(v) if kind == "f" else v

    def enc32(self, a):
        return a

    def dec32(self, v):
        return v

    def rd_word(self, a):
        return struct.unpack(self.wfmt, self.read(a, 4))[0]

    def wr_word(self, a, v):
        self.write(a, struct.pack(self.wfmt, v & MASK))

    def put_ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def get_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    wfmt, pfmt = ">I", ">I"

    def __init__(self, e, case):
        self.e = e
        self.table = {st: {n: (o, k) for n, o, k in MAC[st][0]} for st in MAC}
        self.sizes = {st: MAC[st][1] for st in MAC}
        self.L = e.cl = CL.PpcLog({}, case["queues"], first_new=FIRST_NEW)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def psize(self):
        return 4

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, a, n):
        self.L.labels[a] = n

    def lab(self, a):
        return self.L.lab(a)

    def log(self):
        return self.L.log

    def call(self, name, ints=(), floats=()):
        """ints (words) in r3 up, floats (bit patterns) in f1 up; answers (r3, float bits)."""
        args = [T.F(bitsf(f)) for f in floats] + [i & MASK for i in ints]
        with ROUNDING(self.e):
            r3, _, f1 = T.call_ppc(self.e, FN[name], args)
        return r3 & MASK, canon_f(fb(f1))


class Host(Side):
    """The same on the host library."""
    wfmt, pfmt = "<I", "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.table = {st: {n: (HOST[st][n][0], HOST[st][n][1]) for n in HOST[st]} for st in HOST}
        self.sizes = HOSTSIZE
        CL.host_begin(lib, {}, case["queues"], first_new=FIRST_NEW)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ci_addr(name.encode())

    def enc32(self, a):
        return a - self.lib.tpw_ci_addr(b"arenabase") if a else 0

    def dec32(self, v):
        return v + self.lib.tpw_ci_addr(b"arenabase") if v else 0

    def psize(self):
        return 8

    def alloc(self, n):
        return self.lib.tpw_ci_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, a, n):
        self.lib.tpw_cl_label(n, a)

    def lab(self, a):
        return self.lib.tpw_cl_lab(a) if a else 0

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, ints=(), floats=()):
        lib = self.lib
        if name == "Interval":
            which, pylon = ints
            return lib.tpw_ci_Interval(which, bitsf(floats[0]), pylon) & MASK, 0
        raise AssertionError(name)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_ci_"
    u64, u32, i32, f32 = C.c_uint64, C.c_uint32, C.c_int32, C.c_float
    sig = {"addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "SetCurrentEditCoaster": (None, [u64]),
           "Interval": (u32, [i32, f32, u64]), "FlagGenerateTrack": (None, [u64, i32]), "FlagCollision": (None, [u64]),
           "XZAngle": (u32, [u32] * 6), "Potential": (None, [u64]), "DestroyEndPylons": (None, [u64]), "EndTrack": (None, [i32]),
           "SetCoasterState": (None, [u64, u32]), "IsCoasterType": (i32, [u32]), "IsCoaster": (i32, [u32]), "Find": (u64, [u32]),
           "SetCObject": (None, [u32, u64]), "SetScriptID": (None, [u32, i32]), "NumTracks": (i32, []),
           "InstanceCoaster": (u32, [u32, u32, u32, u32, i32, u64]), "balance_new": (u64, []), "balance_dtor": (i32, [u64, i32])}
    for name, (res, args) in sig.items():
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    out = (C.c_int64 * 80)()
    n = getattr(lib, p + "layout")(out)
    layout = list(out)[:n]
    if HOST_LAYOUT is not None:
        assert layout == HOST_LAYOUT, layout
    i = 0
    for st in ORDER:
        fields = MAC[st][0]
        HOST[st] = {name: (layout[i + k], kind) for k, (name, _, kind) in enumerate(fields)}
        HOSTSIZE[st] = layout[i + len(fields)]
        i += len(fields) + 1
    HOSTSIZE["balance"] = layout[i]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def O(n, st, **f):
    return {"n": n, "s": st, "f": f}


def rec_off(s, o, off):
    """A train block's word at the Mac's offset, on this side: records are 0x80 bytes on the Mac and sizeof(sTrainInstance) on the host."""
    return (off // 0x80) * s.size("train") + off % 0x80 if o["rec"] else off


def BLK(n, size=0x200, w=None, rec=False):
    return {"n": n, "s": "blk", "size": size, "w": w or {}, "rec": rec}


def build(s, case):
    """Writes the case's globals and objects through Side: the same code for both memories (offsets are the Mac's)."""
    objs = {}
    for k, o in enumerate(case["objs"]):
        st = o["s"]
        objs[o["n"]] = s.alloc((max(o["size"], 4 * s.size("train")) if o["rec"] else o["size"]) if st == "blk" else s.size(st))
        s.label(objs[o["n"]], k + 1)
        if st == "blk" and o["rec"]:
            for j in range(1, 4):
                s.label(objs[o["n"]] + s.size("train") * j, 200 + 10 * k + j)

    def val(v):
        return 0 if v is None else objs[v] if isinstance(v, str) else v

    for o in case["objs"]:
        a = objs[o["n"]]
        if o["s"] == "blk":
            for off, v in o["w"].items():
                s.wr_word(a + rec_off(s, o, off), v)
        else:
            for name, v in o["f"].items():
                s.put(a, o["s"], name, val(v))
    g = case["g"]
    for name in WORDS:
        s.wr_word(s.g(name), g.get(name, 0))
    for name in POINTERS:
        s.put_ptr(s.g(name), val(g.get(name)))
    for name, n in ARRAYS.items():
        entries = g.get(name, {})
        for i in range(n):
            s.put_ptr(s.g(name) + i * s.psize(), val(entries.get(i)))
    case["_objs"] = objs
    return objs


def dump_instance(s, a):
    return tuple(s.get(a, "inst", name) for name, _, _ in MAC["inst"][0])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels, floats canon_f'd."""
    objs = case["_objs"]
    out = {"answer": answer, "log": s.log()}
    for o in case["objs"]:
        a = objs[o["n"]]
        if o["s"] == "blk":
            out[o["n"]] = [s.rd_word(a + rec_off(s, o, off)) for off in sorted(o["w"])]
        else:
            out[o["n"]] = [s.get(a, o["s"], name) for name, _, _ in MAC[o["s"]][0]]
    out["edit"] = s.lab(s.get_ptr(s.g("edit")))
    head = s.get_ptr(s.g("instances"))
    out["instances"] = s.lab(head)
    out["count"] = s.rd_word(s.g("count"))
    out["words"] = [s.rd_word(s.g(n)) for n in ("w689C", "w6898", "w6894", "w6890")]
    chain, a = [], head
    while a and len(chain) < 6:
        chain.append(dump_instance(s, a))
        a = s.get_ptr(a + s.fld("inst", "mNext")[0])
    out["chain"] = chain
    return out


def execute(s, case):
    """The call."""
    op, objs = case["op"], case["_objs"]
    arg = lambda v: 0 if v is None else objs[v] if isinstance(v, str) else v
    a = case["args"]
    if op == "SetCurrentEditCoaster":
        return call(s, op, [arg(a[0])])
    if op in ("GetIntervalForHeight", "GetIntervalForHeightDelta", "GetHeightForInterval"):
        return call(s, op, [arg(a[1])], [a[0]], which=("GetIntervalForHeight", "GetIntervalForHeightDelta", "GetHeightForInterval").index(op))
    if op == "FlagGenerateTrack":
        return call(s, op, [arg(a[0]), a[1]])
    if op == "FlagCollisionUpdateTrackAroundBrace":
        return call(s, op, [arg(a[0])])
    if op == "XZAngleBetweenPylons":
        return call(s, op, list(a))
    if op == "GetPotentialPylonPositionArea":
        b = objs["buf"]
        return call(s, op, [b, b + 4, b + 8, b + 12], host=[b])
    if op in ("DestroyEndPylons", "SetCoasterState", "EndTrack", "IsCoasterType", "IsCoaster", "FindCoasterInstanceFromEntranceID",
              "SetCoasterCObject", "SetCoasterScriptID", "ui_GetNumCoasterTracks", "InstanceCoaster"):
        vals = [arg(v) if isinstance(v, str) or v is None and op in ("SetCoasterCObject", "InstanceCoaster") else v for v in a]
        return call(s, op, vals)
    raise AssertionError(op)


HOSTFN = dict(SetCurrentEditCoaster="SetCurrentEditCoaster", FlagGenerateTrack="FlagGenerateTrack",
              FlagCollisionUpdateTrackAroundBrace="FlagCollision", XZAngleBetweenPylons="XZAngle", GetPotentialPylonPositionArea="Potential",
              DestroyEndPylons="DestroyEndPylons", EndTrack="EndTrack", SetCoasterState="SetCoasterState", IsCoasterType="IsCoasterType",
              IsCoaster="IsCoaster", FindCoasterInstanceFromEntranceID="Find", SetCoasterCObject="SetCObject",
              SetCoasterScriptID="SetScriptID", ui_GetNumCoasterTracks="NumTracks", InstanceCoaster="InstanceCoaster")
SIGNED = {"EndTrack", "FlagGenerateTrack", "SetCoasterScriptID", "InstanceCoaster"}
FLOATOPS = ("GetIntervalForHeight", "GetIntervalForHeightDelta", "GetHeightForInterval", "XZAngleBetweenPylons")


VOIDOPS = {"SetCurrentEditCoaster", "FlagGenerateTrack", "FlagCollisionUpdateTrackAroundBrace", "GetPotentialPylonPositionArea",
           "DestroyEndPylons", "EndTrack", "SetCoasterState", "SetCoasterCObject", "SetCoasterScriptID"}


def call(s, op, ints, floats=(), which=0, host=None):
    """Both sides' call of one function; the answer is a word (floats as bits)."""
    if isinstance(s, Ppc):
        r3, f1 = s.call(op, ints, floats)
        if op in VOIDOPS:
            return 0
        if op == "FindCoasterInstanceFromEntranceID":
            return s.lab(r3)
        if op in ("GetIntervalForHeight", "GetIntervalForHeightDelta", "GetHeightForInterval"):
            return f1
        if op == "XZAngleBetweenPylons":
            return f1
        return r3
    lib = s.lib
    fn = getattr(lib, "tpw_ci_" + HOSTFN.get(op, op), None)
    if op in ("GetIntervalForHeight", "GetIntervalForHeightDelta", "GetHeightForInterval"):
        return canon_f(lib.tpw_ci_Interval(which, bitsf(floats[0]), ints[0]))
    if op == "XZAngleBetweenPylons":
        return canon_f(fn(*[i & MASK for i in ints]))
    if op == "GetPotentialPylonPositionArea":
        fn(host[0])
        return 0
    if op in ("SetCoasterState",):
        return fn(ints[0], ints[1] & MASK) or 0
    if op == "InstanceCoaster":
        return fn(ints[0] & MASK, ints[1] & MASK, ints[2] & MASK, ints[3] & MASK, s32(ints[4]), ints[5]) & MASK
    if op == "FindCoasterInstanceFromEntranceID":
        return s.lab(fn(*ints))
    res = fn(*[s32(i) if op in SIGNED and k == 1 or op == "EndTrack" else i for k, i in enumerate(ints)])
    return (res or 0) & MASK


def run(s, case):
    try:
        build(s, case)
        answer = execute(s, case)
        if case["op"] == "InstanceCoaster" and case["args"][4] != 0:
            answer = 0                           # the Mac answers an unset register (not compared)
        return observe(s, case, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
WORDS_EDGE = [0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 100, 255, 0x1234]


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rw(r):
    return r.choice([0, MASK, 0x40, 0x80, 0xC0, 2, 0x42, r.getrandbits(32), r.getrandbits(32)])


def rflt(r):
    return r.choice([0, fb(1.0), fb(0.5), fb(-1.0), fb(3.0)])


def base(r, op, args, objs=None, g=None, queues=None):
    return {"op": op, "args": args, "objs": objs or [], "g": g or {}, "queues": queues or {}}


def inst_objs(r, name, ntr=None, **f):
    """An instance with a block of four trains (word 0 random) and another block (mUVOffsets) the fakes can switch the base to."""
    ntr = r.randrange(0, 4) if ntr is None else ntr
    return [BLK(name + "T", 0x200, {i * 0x80: rw(r) for i in range(4)}, True), BLK(name + "A", 0x200, {i * 0x80: rw(r) for i in range(4)}, True),
            O(name, "inst", mTrains=name + "T", mTrainCount=ntr, mUVOffsets=name + "A", **f)]


def chain_objs(r, name, entrance, deep):
    """The ride anim set of an entrance down to the node whose y the pylons are placed at."""
    return [O(name + "n", "node", m31=rf(r)), O(name + "h", "hdr", mF78=name + "n"), O(name + "m", "mesh", mHeader=name + "h"),
            O(name + "s", "set", mFlags=r.choice([0, 0x40000, 0x40001, 0xFFFBFFFF, MASK]), mHeads=name + "m")]


# ---- the pylon interval three and the small ones
def pylon_case(op, r, lo=None, hi=None, h=None):
    c = base(r, op, [r.choice(SPECIAL) if h is None else h, "p"], [O("p", "pylon", mFBC=rf(r) if lo is None else lo, mFC0=rf(r) if hi is None else hi)])
    return c


def pylon_edges(op, seed):
    def gen():
        r = random.Random(seed)
        one, two, zero, neg = fb(1.0), fb(2.0), 0, fb(-1.5)
        for lo, hi in ((one, two), (two, two), (zero, zero), (neg, one), (0x7F800000, one), (one, 0x7F800000), (0x7FC00000, one)):
            for h in (zero, one, two, neg, fb(0.1), 0x80000000, 0x7F800000, 0x7FC00000, fb(1e30), fb(1e-30)):
                yield pylon_case(op, r, lo, hi, h)
    return gen


def flag_case(r, flag=None, edit=None):
    objs = inst_objs(r, "i", flags=0) if False else inst_objs(r, "i", mFlags=r.choice([0, 1, 0x1000, 0x2000, MASK, rw(r)]))
    return base(r, "FlagGenerateTrack", ["i", r.choice([0, 1, -1, 2, 0x100]) if flag is None else flag], objs,
                {"edit": r.choice([None, "i"]) if edit is None else edit})


def flag_edges():
    r = random.Random(186003)
    for flag in (0, 1, -1, 2, 0x100, 0x80000000):
        for flags in (0, 1, 0x1000, 0x1001, MASK):
            c = flag_case(r, flag)
            c["objs"][-1]["f"]["mFlags"] = flags
            yield c


def coll_case(r, nxt=None):
    has = r.random() < 0.6 if nxt is None else nxt
    objs = [O("b1", "brace", mFlags=rw(r), mNext="b2" if has else None), O("b2", "brace", mFlags=rw(r))]
    return base(r, "FlagCollisionUpdateTrackAroundBrace", ["b1"], objs)


def coll_edges():
    r = random.Random(186004)
    for nxt in (False, True, False, True):
        yield coll_case(r, nxt)


def xz_case(r, words=None):
    if words is None:
        pick = lambda: r.choice(WORDS_EDGE + [r.randrange(0, 2000), r.getrandbits(32), r.randrange(0, 50)])
        words = [pick() for _ in range(6)]
    return base(r, "XZAngleBetweenPylons", list(words))


def xz_edges():
    r = random.Random(186005)
    for w in ((0, 0, 0, 0, 0, 0), (10, 10, 10, 10, 10, 10), (0, 0, 10, 0, 20, 0), (0, 0, 10, 0, 0, 10), (0, 10, 0, 0, 10, 0),
              (5, 0, 0, 0, 0, 5), (0, 0, 5, 5, 10, 10), (10, 10, 5, 5, 0, 0), (1, 0, 0, 0, 0, 1), (0, 0, 1, 1, 2, 3), (2, 3, 1, 1, 0, 0),
              (0xFFFFFFFF, 0, 0, 0, 1, 0), (0x80000000, 0x80000000, 0, 0, 0xFFFFFFFF, 1), (1, 2, 3, 4, 3, 4), (3, 4, 3, 4, 1, 2),
              (100, 0, 0, 0, 100, 1), (100, 0, 0, 0, 100, 0), (100, 1, 0, 0, 100, 0), (1000000, 3, 0, 0, 1000000, 2)):
        yield xz_case(r, w)
    for _ in range(40):
        yield xz_case(r)


def pot_case(r):
    g = {n: r.choice(WORDS_EDGE + [r.getrandbits(32)]) for n in ("w689C", "w6898", "w6894", "w6890")}
    return base(r, "GetPotentialPylonPositionArea", [], [BLK("buf", 0x10, {0: 0xDEADBEEF, 4: 0xDEADBEEF, 8: 0xDEADBEEF, 12: 0xDEADBEEF})], g)


def edit_case(r, n=None):
    objs = []
    for k in range(3):
        objs += inst_objs(r, "i%d" % k, mFlags=rw(r))
    arg = r.choice([None, "i0", "i1", "i2"]) if n is None else n
    return base(r, "SetCurrentEditCoaster", [arg], objs, {"edit": r.choice([None, "i1"])})


# ---- DestroyEndPylons
def destroy_case(r, ends=None, poison=None, bdflags=None):
    ends = r.randrange(0, 5) if ends is None else ends
    objs = [O("spare", "brace", mFlags=rw(r), mData="sd"), O("sd", "bdata", mFlags=0)]
    names = ["e%d" % i for i in range(ends)] + ["plain", "tail"]
    for i, n in enumerate(names):
        end = i < ends
        flags = (r.choice([1, 3, 0x11, MASK, 0x81]) if bdflags is None else bdflags) if end else r.choice([0, 2, 0x10, 0xFFFFFFFE, 0x80])
        objs.append(O(n, "brace", mFlags=rw(r), mNext=names[i + 1] if i + 1 < len(names) else None, mData=n + "d"))
        objs.append(O(n + "d", "bdata", mFlags=flags))
    objs.append(O("i", "inst", mBraces=names[0], mBlock3C=r.choice([0, 1, 3, MASK, rw(r)]), mCObject="spare", mFlags=rw(r)))
    q = {"Poison": [r.choice([0, 1])] if poison is None else [poison]}
    return base(r, "DestroyEndPylons", ["i"], objs, {}, q)


def destroy_edges():
    r = random.Random(186006)
    for ends in range(5):
        for poison in (0, 1):
            yield destroy_case(r, ends, poison)
    yield destroy_case(r, 2, 0, 0x101)
    yield destroy_case(r, 1, 1, 0xFFFFFFFF)


# ---- SetCoasterState
CURS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)
HIGHS = (0, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70)


def state_case(r, cur=None, new=None, ntr=None, queues=None):
    cur = r.choice([r.choice(CURS) | r.choice(HIGHS) | r.choice([0, 0x80, 0x100, 0x8000]), r.choice((1, 2, 4, 8)) | r.choice((0, 0x40))]) if cur is None else cur
    new = r.choice([r.choice(CURS) | r.choice(HIGHS) | r.choice([0, 0x80, 0xF00]), r.choice((1, 2, 4, 8)) | r.choice((0, 0x10, 0x20, 0x40))]) if new is None else new
    objs = inst_objs(r, "i", ntr, mState=cur, mFlags=r.choice([0, 0x800, MASK, rw(r)]), mF2C=rflt(r))
    if queues is None:
        queues = {}
        if r.random() < 0.4:
            for k in ("Deactivate", "Activate", "Unload"):
                if r.random() < 0.5:
                    queues["Swap:" + k] = [1]
                if r.random() < 0.5:
                    queues["Count:" + k] = [r.choice([0, 1, 2, 3, 4])]
    return base(r, "SetCoasterState", ["i", new], objs, {"edit": "i"}, queues)


def state_edges():
    r = random.Random(186007)
    for cur in CURS:
        for new in CURS:
            yield state_case(r, cur, new, 2, {})
    for cur in HIGHS:
        for new in HIGHS:
            yield state_case(r, cur | 3, new, r.choice([0, 1, 3]), {})
            yield state_case(r, cur | 4, new | 1, 2, {})
    for cur, new in ((1, 2), (2, 1), (4, 2), (8, 2), (2, 8), (1, 8), (4, 8), (8, 8)):
        for ntr in (0, 1, 3):
            yield state_case(r, cur, new, ntr, {})
        for swap in (0, 1):
            for count in (-1, 0, 1, 3, 4):
                q = {}
                for k in ("Deactivate", "Activate", "Unload"):
                    q["Swap:" + k] = [swap]
                    if count >= 0:
                        q["Count:" + k] = [count]
                yield state_case(r, cur, new, 2, q)


# ---- EndTrack
def end_case(r, flag=None, recording=None, edit=None):
    objs = []
    for k in range(3):
        objs += inst_objs(r, "i%d" % k, mFlags=rw(r), mState=r.choice([1, 2, 4, 8, 3, 0x41, 0x42, 0]), mBlock3C=rw(r),
                          mEntranceID=k + 1, mNext="i%d" % ((k + 1) % 3), mF2C=rflt(r))
        objs += chain_objs(r, "c%d" % k, k + 1, 0)
    queues = {k: [r.choice([0, 1])] for k in ("Edit:Recalc", "Edit:State", "Edit:Panel", "Edit:Place")}
    g = {"edit": r.choice(["i0", "i1"]) if edit is None else edit, "recording": r.choice([0, 1, 0x100, MASK]) if recording is None else recording,
         "rideanim": {1: "c0s", 2: "c1s", 3: "c2s"}}
    return base(r, "EndTrack", [r.choice([0, 1, -1, 2]) if flag is None else flag], objs, g, queues)


def end_edges():
    r = random.Random(186008)
    for flag in (0, 1, -1):
        for recording in (0, 1):
            for qs in ((0, 0, 0, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1), (1, 1, 1, 1), (0, 0, 1, 1)):
                c = end_case(r, flag, recording, "i0")
                for key, v in zip(("Edit:Recalc", "Edit:State", "Edit:Panel", "Edit:Place"), qs):
                    c["queues"][key] = [v]
                yield c


# ---- the finders and setters
def list_case(r, op, args=None):
    n = r.randrange(0, 5)
    ids = [r.choice([1, 2, 3, 2, 0, MASK]) for _ in range(n)]
    objs = []
    for k in range(n):
        objs += inst_objs(r, "i%d" % k, 0, mEntranceID=ids[k], mNext="i%d" % (k + 1) if k + 1 < n else None, mCObject=r.choice([None, "cb"]),
                          mScriptID=r.choice([0, 5, MASK]))
    objs.append(BLK("cb", 0x10, {}))
    objs.append(BLK("cb2", 0x10, {}))
    if args is None:
        args = [r.choice([1, 2, 3, 0, MASK, 7])]
    return base(r, op, args, objs, {"instances": "i0" if n else None})


def find_edges(op, extra):
    def gen():
        r = random.Random(186009 + len(op))
        for n in range(5):
            for entrance in (1, 2, 3, 7, 0, MASK):
                c = list_case(r, op, [entrance] + extra(r))
                yield c
    return gen


def type_case(r, op):
    c = base(r, op, [r.choice([1, 2, 3, 4])], [], {})
    sets = []
    for k in range(1, 5):
        sets.append(O("s%d" % k, "set", mFlags=r.choice([0, 0x40000, 0x40001, 0xFFFBFFFF, MASK, 0x3FFFF, 0x80000, 0x20000])))
    c["objs"] = sets
    c["g"] = {"ridetypes": {k: "s%d" % (5 - k) for k in range(1, 5)}, "rideanim": {k: "s%d" % k for k in range(1, 5)}}
    return c


def type_edges(op):
    def gen():
        r = random.Random(186010 + len(op))
        for idx in (1, 2, 3, 4):
            for bits in (0, 0x40000, 0x40001, 0xFFFBFFFF, MASK, 0x3FFFF):
                c = type_case(r, op)
                c["args"] = [idx]
                for o in c["objs"]:
                    o["f"]["mFlags"] = bits
                c["objs"][idx - 1]["f"]["mFlags"] = bits ^ 0x40000
                yield c
    return gen


def count_case(r):
    return base(r, "ui_GetNumCoasterTracks", [], [], {"count": r.choice([0, 1, 5, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)])})


# ---- InstanceCoaster
def instance_case(r, flag=None, first=None, alloc=None, nexist=None, last=None):
    ntypes = r.randrange(1, 5)
    objs = []
    numbers = [r.choice([5, 6, 7, 9, 5]) for _ in range(ntypes)]
    for k in range(ntypes):
        objs.append(O("t%d" % k, "type", mFlags=r.choice([0, 4, 8, 0xC, 0xFFFFFFF3, MASK, rw(r)]), mRideTypeNumber=numbers[k], mTextureCount=r.choice([0, 1, 5, 40]),
                      mFA4=rf(r), mF2F4=r.choice([0, 3, 5, MASK]), mF2F8=r.choice([0, 4, 10, 0x1000]), mF318=r.choice([0, 3, 100, 0x40000000])))
    nexist = r.randrange(0, 3) if nexist is None else nexist
    for k in range(nexist):
        objs += inst_objs(r, "x%d" % k, 0, mFlags=rw(r), mNext="x%d" % (k + 1) if k + 1 < nexist else None, mEntranceID=k + 1)
    for k in range(1, 4):
        objs += chain_objs(r, "c%d" % k, k, 0)
    objs.append(BLK("cb", 0x10, {}))
    wanted = r.choice(numbers) if first is None else first
    if last:
        wanted = numbers[-1]
    g = {"types": {k: "t%d" % k for k in range(ntypes)}, "typecount": ntypes, "instances": "x0" if nexist else None,
         "count": r.choice([0, 3, MASK]), "edit": r.choice([None, "cb"]), "rideanim": {1: "c1s", 2: "c2s", 3: "c3s"}}
    flag = r.choice([0, 0, 1, -1, 5]) if flag is None else flag
    args = [wanted, r.choice([1, 2, 3]), r.choice(WORDS_EDGE + [r.getrandbits(32)]), r.choice(WORDS_EDGE + [r.getrandbits(32)]), flag, r.choice([None, "cb"])]
    queues = {"Alloc": [r.choice([0, 7, 255, MASK, 1])] if alloc is None else [alloc]}
    return base(r, "InstanceCoaster", args, objs, g, queues)


def instance_edges():
    r = random.Random(186011)
    for flag in (0, 1, -1):
        for alloc in (0, 5, MASK):
            for nexist in (0, 2):
                yield instance_case(r, flag, None, alloc, nexist)
    for last in (False, True):
        for _ in range(6):
            yield instance_case(r, 0, None, 9, None, last)
    for tflags in (0, 4, 8, 0xC):
        c = instance_case(r, 0, None, 9, 1)
        for o in c["objs"]:
            if o["s"] == "type":
                o["f"]["mFlags"] = tflags
        yield c


# ---- the destructor
L_BAL = 7


def dtor_case(r, null=None, flag=None):
    c = base(r, "Dtor", [])
    c["null"] = r.random() < 0.15 if null is None else null
    c["arg"] = r.choice([0, 1, -1, 0x10000, 0x18000, 0x8000, 0x7FFF, 0xFFFF, 2, 0x10001, MASK, r.getrandbits(32)]) if flag is None else flag
    return c


def dtor_edges():
    r = random.Random(186012)
    for null in (True, False):
        for flag in (0, 1, -1, 0x10000, 0x18000, 0x8000, 0x7FFF, 0xFFFF, 2, 0x10001, 0x7FFF0000, MASK):
            yield dtor_case(r, null, flag)


def dtor_ppc(e, case):
    s = Ppc(e, case)
    try:
        blk = e.alloc(BALANCE_MAC, bytes(rr % 251 for rr in range(BALANCE_MAC)))
        s.L.labels[blk] = L_BAL
        r3 = T.call_ppc(e, FN["Dtor"], [0 if case["null"] else blk, case["arg"] & MASK])[0] & MASK
        return {"result": 0 if r3 == 0 else L_BAL if r3 == blk else r3, "log": s.L.log}
    finally:
        s.done()


def dtor_host(lib, case):
    s = Host(lib, case)
    p = lib.tpw_ci_balance_new()
    res = lib.tpw_ci_balance_dtor(0 if case["null"] else p, s32(case["arg"]))
    return {"result": res, "log": CL.host_end(lib)}


# ---- the FnTest list
def rnd(maker):
    return lambda r: maker(r)


def fn(name, maker, edges):
    return FnTest(name, ppc, host, edge=edges, rnd=maker)


def finder_args(op):
    return {"SetCoasterCObject": lambda r: [r.choice([None, "cb", "cb2"])], "SetCoasterScriptID": lambda r: [r.choice([0, 1, -1, 99, MASK])],
            "FindCoasterInstanceFromEntranceID": lambda r: []}[op]


def finder_case(op):
    def maker(r):
        c = list_case(r, op)
        c["args"] += finder_args(op)(r)
        return c
    return maker


TESTS = [
    fn("SetCurrentEditCoaster", lambda r: edit_case(r), lambda: (edit_case(random.Random(186001), n) for n in (None, "i0", "i1", "i2"))),
    fn("GetIntervalForHeight", lambda r: pylon_case("GetIntervalForHeight", r, h=rf(r)), pylon_edges("GetIntervalForHeight", 186101)),
    fn("GetIntervalForHeightDelta", lambda r: pylon_case("GetIntervalForHeightDelta", r, h=rf(r)), pylon_edges("GetIntervalForHeightDelta", 186102)),
    fn("GetHeightForInterval", lambda r: pylon_case("GetHeightForInterval", r, h=rf(r)), pylon_edges("GetHeightForInterval", 186103)),
    fn("FlagGenerateTrack", flag_case, flag_edges),
    fn("FlagCollisionUpdateTrackAroundBrace", coll_case, coll_edges),
    fn("XZAngleBetweenPylons", xz_case, xz_edges),
    fn("GetPotentialPylonPositionArea", pot_case, lambda: (pot_case(random.Random(i)) for i in range(4))),
    fn("DestroyEndPylons", destroy_case, destroy_edges),
    fn("EndTrack", end_case, end_edges),
    fn("SetCoasterState", state_case, state_edges),
    fn("IsCoasterType", lambda r: type_case(r, "IsCoasterType"), type_edges("IsCoasterType")),
    fn("IsCoaster", lambda r: type_case(r, "IsCoaster"), type_edges("IsCoaster")),
    fn("FindCoasterInstanceFromEntranceID", finder_case("FindCoasterInstanceFromEntranceID"), find_edges("FindCoasterInstanceFromEntranceID", lambda r: [])),
    fn("SetCoasterCObject", finder_case("SetCoasterCObject"), find_edges("SetCoasterCObject", lambda r: [r.choice([None, "cb", "cb2"])])),
    fn("SetCoasterScriptID", finder_case("SetCoasterScriptID"), find_edges("SetCoasterScriptID", lambda r: [r.choice([0, 1, -1, 99, MASK])])),
    fn("ui_GetNumCoasterTracks", count_case, lambda: (count_case(random.Random(i)) for i in range(6))),
    fn("InstanceCoaster", instance_case, instance_edges),
    FnTest("CCoasterBalance::~CCoasterBalance", dtor_ppc, dtor_host, edge=dtor_edges, rnd=dtor_case),
]
