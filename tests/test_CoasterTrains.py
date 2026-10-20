"""Unit w192, the coaster trains, against libtpw_unit_CoasterTrains.dylib (tests/unit_CoasterTrains.cpp), main app ("SimTheme Park"),
src/TPCoaster_trains.cpp:
  InstanceCoasterTrains 0x10042084, DestroyCoasterTrains 0x10042344, GetCoaster1stPersonCar 0x100433F4, SaveTrainsAndCars 0x10043A18,
  LoadTrainsAndCars 0x10043E8C
(the four swap instances 0x10043DB8, 0x10043E00, 0x10043E44 and 0x10021BB4 are tests/test_CoasterTrainsSwap.py's).

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian words,
32-bit pointers; trains 0x80 bytes, cars 0x60) for the original, and in the host library's memory (little-endian, 64-bit pointers, the host
structs' own offsets from tpw_ctr_layout) for the translation. A train's type word is the coaster type PLUS 0x2f0 on the Mac (the embedded
train type) and the type itself on the host. Two coaster types (T0 the instance's, T1 another), four car types (ride type 0 or a ride, head
counts) and five ride anim sets (gRideAnimSets[0..4]) stand behind them. Every callee outside the unit is a fake on both sides that records
its arguments (objects as labels, the allocator's Mac sizes told from the host's) and answers or changes the world from the case's queues:
InstanceRide (the ride answered, a new train and car count of the type), DisableMeshDraw (clears the ride's set), DeactivateCoasterTrain
(another train type, another train count, another instance type), DisableCoasterCarDrawSafe (another ride number for the car), LbFile_Read
and LbFile_Write (the transferred counts "xfer" and the TbErrors "err"; Read serves the case's stream, zero past its end).

Compared: the answer, the call log, the written stream byte for byte (Save), and a snapshot of the world after: the instance's, every train's
and every car's fields (a pointer as a label, or as an index into the trains, the cars or the heads), the types' counts, the ride set array and
the contents of every head block. The file is little-endian: the Mac swaps every word, the plain host library does not swap, so the bytes are
equal. Float fields are compared as bits. Not tested: a ride number past gRideAnimSets (the original indexes it), a NULL set at the first
read (the original crashes), a train or car count past the blocks the case builds (4 trains, 5 cars each), counts of head words above 300.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
import test_ParticleSave as PS

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CoasterTrains"
MASK = 0xFFFFFFFF

INSTANCE, DESTROY, FIRST, SAVE, LOAD = 0x10042084, 0x10042344, 0x100433F4, 0x10043A18, 0x10043E8C
SIZES = {INSTANCE: 620, DESTROY: 240, FIRST: 92, SAVE: 864, LOAD: 948}
OWN = set(SIZES) | {0x10043DB8, 0x10043E00, 0x10043E44, 0x10021BB4}

UDEBUG, ALLOC, FREE = 0x10000E40, 0x10064BD4, 0x10064C54
INSTANCE_RIDE, DMD1, DMD3, OVERRIDE = 0x10069B24, 0x100653A4, 0x1006548C, 0x100C609C
RESTORE, DESTROY_RIDE, DEACTIVATE, DISABLE_DRAW = 0x1006A140, 0x1006A3EC, 0x1004AA70, 0x1004A648
RIDEANIM, INSTANCES = 0x102C79A4, 0x102B68B4
FILE = 0x4321
TYPE_OFF = 0x2F0          # a Mac train's type word: the coaster type plus this

# struct -> ([(field, Mac offset, kind)], Mac size). Kinds: w word, p pointer, t the train's type word, h2 two halfwords. The host's offsets come from
# tpw_ctr_layout in the same order, each struct's size last.
FIELDS = {
    "car": ([("mFlags", 0x0, "w"), ("mType", 0x4, "p"), ("mIndex", 0x8, "w"), ("mRide", 0xc, "w"), ("mF10[0]", 0x10, "w"), ("mF10[1]", 0x14, "w"), ("mF10[2]", 0x18, "w"), ("mF1C", 0x1c, "w"), ("mHeadsA", 0x20, "p"), ("mHeadsB", 0x24, "p"), ("mCurrentA", 0x28, "p"), ("mCurrentB", 0x2c, "p"), ("mCountA", 0x30, "w"), ("mCountB", 0x34, "w"), ("mF38", 0x38, "w"), ("mF3C", 0x3c, "w"), ("mF40", 0x40, "w"), ("mF44", 0x44, "w"), ("mF48[0]", 0x48, "w"), ("mF48[1]", 0x4c, "w"), ("mF48[2]", 0x50, "w"), ("mF54[0]", 0x54, "w"), ("mF54[1]", 0x58, "w"), ("mF54[2]", 0x5c, "w")], 0x60),
    "train": ([("mFlags", 0x0, "w"), ("mType", 0x4, "t"), ("mF08", 0x8, "w"), ("mCarsInUse", 0xc, "w"), ("mF10", 0x10, "w"), ("mF14", 0x14, "w"), ("mF18", 0x18, "w"), ("mF1C", 0x1c, "w"), ("mFrontSample", 0x20, "w"), ("mRearSample", 0x24, "w"), ("mCars", 0x28, "p"), ("mCentreCar", 0x2c, "w"), ("mF30", 0x30, "w"), ("mFirstPersonCar", 0x34, "p"), ("mF38", 0x38, "w"), ("mF3C[0]", 0x3c, "w"), ("mF3C[1]", 0x40, "w"), ("mF3C[2]", 0x44, "w"), ("mF48[0]", 0x48, "w"), ("mF48[1]", 0x4c, "w"), ("mF48[2]", 0x50, "w"), ("mF54[0]", 0x54, "w"), ("mF54[1]", 0x58, "w"), ("mF54[2]", 0x5c, "w"), ("mF54[3]", 0x60, "w"), ("mF64", 0x64, "w"), ("mF68", 0x68, "w"), ("mF6C", 0x6c, "w"), ("mF70", 0x70, "w"), ("mPosition", 0x74, "w"), ("mF78", 0x78, "w"), ("mF7C", 0x7c, "w")], 0x80),
    "inst": ([("mFlags", 0x0, "w"), ("mType", 0x4, "p"), ("mEntranceID", 0x10, "w"), ("mNext", 0x18, "p"), ("mLeadTrain", 0x124, "p"), ("mTrains", 0x128, "p"), ("mTrainWords", 0x12c, "p"), ("mCars", 0x130, "p"), ("mHeads", 0x148, "p")], 0x150),
    "type": ([("mF2E8", 0x2e8, "w"), ("mF2F4", 0x2f4, "w"), ("mCarTypeA", 0x2fc, "p"), ("mCarTypeB", 0x300, "p"), ("mCarTypeC", 0x304, "p"), ("mF318", 0x318, "w")], 0x31c),
    "ctype": ([("mRideType", 0xc, "w"), ("mHeadCount", 0x30, "w")], 0x34),
    "set": ([("mFlags", 0x4, "w"), ("mHeads", 0x8, "p"), ("mFC0", 0xc0, "h2"), ("mFC4", 0xc4, "h2")], 0xd0),
}
ORDER = ["car", "train", "inst", "type", "ctype", "set"]
HOST = {}          # struct -> {field: offset}, filled by bind
HOST_SIZE = {}
HOST_LAYOUT = [0, 8, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 0, 8, 16, 20, 24, 28, 32, 36, 40, 44, 48, 56, 60, 64, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0, 8, 24, 32, 352, 360, 368, 376, 408, 416, 816, 828, 184, 192, 200, 848, 944, 40, 56, 64, 8, 16, 288, 292, 384]
# The host's offsets and sizes (native pointers: a car is 128 bytes and a train 144 where the Mac's are 0x60 and 0x80; the instance's pointers
# mLeadTrain..mHeads sit at 352..408); the Mac's are the tables above.


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stk(e, k):
    """The k-th word of the stack arguments (the ninth argument is k = 0)."""
    return e.u32(e.r(1) + 0x38 + 4 * k)


def be(v):
    return struct.pack(">I", v & MASK)


def stub_alloc(e):
    L = e.cl
    size, flags = e.r(3), e.r(4)
    a = e.alloc(max(size, 4), bytes(max(size, 4)))
    L.rec("MReleaseAllocF", (size, flags, L.newlab(a)))
    e.set_r(3, a)


def stub_free(e):
    e.cl.rec("MReleaseFreeF", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_instance_ride(e):
    L = e.cl
    L.rec("InstanceRide", [e.r(i) for i in range(3, 11)] + [stk(e, 0), stk(e, 1)])
    L.lastride = L.ret("ride", 0) & MASK
    n = L.ret("n", -1)
    if n >= 0:
        e.write(L.type + 0x2E8, be(n))
    c = L.ret("c", -1)
    if c >= 0:
        e.write(L.type + 0x2F4, be(c))
    e.set_r(3, L.lastride)


def stub_restore(e):
    L = e.cl
    L.rec("RestoreRideInstance", [e.r(i) for i in range(3, 11)] + [stk(e, 0)])
    L.lastride = e.r(3)
    e.set_r(3, L.ret("restore", 0) & MASK)


def stub_destroy_ride(e):
    e.cl.rec("DestroyRideInstance", (e.r(3),))


def clear_set(e):
    if e.cl.ret("clear", 0) != 0:
        e.write(RIDEANIM + 4 * e.cl.lastride, bytes(4))


def stub_dmd1(e):
    e.cl.rec("DisableMeshDraw1", (e.cl.lab(e.r(3)),))
    clear_set(e)


def stub_dmd3(e):
    e.cl.rec("DisableMeshDraw3", (e.cl.lab(e.r(3)), e.r(4), e.r(5)))
    clear_set(e)


def stub_override(e):
    e.cl.rec("OverrideActiveChannels", (e.cl.lab(e.r(3)),))


def stub_deactivate(e):
    L = e.cl
    instance, train = e.r(3), e.r(4)
    L.rec("DeactivateCoasterTrain", (L.lab(instance), L.lab(train)))
    k = L.ret("retype", -1)
    if k >= 0:
        e.write(train + 4, be(L.types[k] + TYPE_OFF))
    k = L.ret("ncount", -1)
    if k >= 0:
        e.write(L.types[0] + 0x2E8, be(k))
    k = L.ret("itype", -1)
    if k >= 0:
        e.write(instance + 4, be(L.types[k]))


def stub_disable_draw(e):
    L = e.cl
    ride = e.r(3)
    L.rec("DisableCoasterCarDrawSafe", (ride,))
    mod = L.ret("rmod", -1)
    if mod >= 0:
        base, count = L.cars
        for i in range(count):
            if e.u32(base + 0x60 * i + 0xC) == ride:
                e.write(base + 0x60 * i + 0xC, be(mod))
                break


def stub_udebug(e):
    e.cl.rec("UDebug", (), (e.cstr(e.r(3)),))


def stub_read(e):
    L, buffer, length, transferred = e.cl, e.r(5), e.r(6), e.r(7)
    L.rec("LbFile_Read", (e.r(4), length, 1 if transferred else 0))
    chunk = L.file_in[L.file_pos:L.file_pos + length]
    L.file_pos += length
    if length:
        e.write(buffer, chunk + bytes(length - len(chunk)))
    if transferred:
        e.write(transferred, be(L.ret("xfer", length)))
    e.write(e.r(3), struct.pack(">i", L.ret("err", 0)))


def stub_write(e):
    L, buffer, length, transferred = e.cl, e.r(5), e.r(6), e.r(7)
    L.rec("LbFile_Write", (e.r(4), length, 1 if transferred else 0))
    L.file_out.append(e.read(buffer, length) if length else b"")
    if transferred:
        e.write(transferred, be(L.ret("xfer", length)))
    e.write(e.r(3), struct.pack(">i", L.ret("err", 0)))


STUBS.internal(UDEBUG, stub_udebug)
STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(FREE, stub_free)
STUBS.internal(INSTANCE_RIDE, stub_instance_ride)
STUBS.internal(RESTORE, stub_restore)
STUBS.internal(DESTROY_RIDE, stub_destroy_ride)
STUBS.internal(DMD1, stub_dmd1)
STUBS.internal(DMD3, stub_dmd3)
STUBS.internal(OVERRIDE, stub_override)
STUBS.internal(DEACTIVATE, stub_deactivate)
STUBS.internal(DISABLE_DRAW, stub_disable_draw)
STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", PS.stub_swap_block)
STUBS.imp("_USwapCopy32__13SamsUtilitiesFPUll", PS.stub_swap_copy)
STUBS.imp("_USwapDelete__13SamsUtilitiesFPv", PS.stub_swap_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)       # memset


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def kind(self, struct, name):
        for n, o, k in FIELDS[struct][0]:
            if n == name:
                return k
        raise KeyError(name)

    def put(self, struct, addr, name, v):
        k, off = self.kind(struct, name), self.off(struct, name)
        if k == "t" and self.mac and v:
            v += TYPE_OFF
        if k == "h2":
            self.write(addr + off, struct_h2(self, v))
        elif k in "pt":
            self.write(addr + off, self.pack_ptr(v))
        else:
            self.write(addr + off, self.pack_word(v))

    def get(self, struct, addr, name):
        k, off = self.kind(struct, name), self.off(struct, name)
        if k == "h2":
            raw = self.read(addr + off, 4)
            a, b = struct_unh2(self, raw)
            return (a << 16) | b
        if k in "pt":
            v = self.unpack_ptr(self.read(addr + off, self.psize))
            if k == "t" and self.mac and v:
                v -= TYPE_OFF
            return v
        return self.unpack_word(self.read(addr + off, 4))

    def stride(self, struct):
        return self.size(struct)

    def new(self, struct, label=None, count=1):
        a = self.alloc(self.size(struct) * count)
        if label is not None:
            self.label(label, a)
        return a

    def raw(self, n, label=None):
        a = self.alloc(n)
        if label is not None:
            self.label(label, a)
        return a

    def wordlist(self, a, n):
        return [self.unpack_word(self.read(a + 4 * i, 4)) for i in range(n)]

    def set_words(self, a, ws):
        self.write(a, b"".join(self.pack_word(w) for w in ws))


def struct_h2(side, v):
    return struct.pack(">I", v & MASK) if side.mac else struct.pack("<HH", (v >> 16) & 0xFFFF, v & 0xFFFF)


def struct_unh2(side, raw):
    if side.mac:
        v = struct.unpack(">I", raw)[0]
        return v >> 16, v & 0xFFFF
    return struct.unpack("<HH", raw)


class Ppc(Side):
    mac, psize = True, 4

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("queues"))
        self.L.file_in, self.L.file_pos, self.L.file_out = case.get("stream", b""), 0, []
        self.L.lastride, self.L.type, self.L.types, self.L.cars = 0, 0, [0, 0], (0, 0)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, struct, name):
        for n, o, k in FIELDS[struct][0]:
            if n == name:
                return o
        raise KeyError(name)

    def size(self, struct):
        return FIELDS[struct][1]

    def alloc(self, n):
        return self.e.alloc(max(n, 4), bytes(max(n, 4)))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, l, a):
        self.L.labels[a] = l

    def lab(self, a):
        return self.L.lab(a)

    def pack_ptr(self, v):
        return struct.pack(">I", v & MASK)

    def unpack_ptr(self, raw):
        return struct.unpack(">I", raw)[0]

    def pack_word(self, v):
        return struct.pack(">I", v & MASK)

    def unpack_word(self, raw):
        return struct.unpack(">I", raw)[0]

    def set_ride_set(self, i, a):
        self.write(RIDEANIM + 4 * i, self.pack_ptr(a))

    def get_ride_set(self, i):
        return self.unpack_ptr(self.read(RIDEANIM + 4 * i, 4))

    def set_instances(self, head):
        self.write(INSTANCES, self.pack_ptr(head))

    def set_fake_world(self, types, cars, count):
        self.L.type, self.L.types, self.L.cars = types[0], types, (cars, count)

    def written(self):
        return b"".join(self.L.file_out)

    def log(self):
        return self.L.log

    def call(self, case, w):
        e = self.e
        op = case["op"]
        if op == "instance":
            T.call_ppc(e, INSTANCE, [w.ty[0], w.inst, case["flag"]])
            return 0
        if op == "destroy":
            T.call_ppc(e, DESTROY, [w.inst])
            return 0
        if op == "first":
            r3, _, _ = T.call_ppc(e, FIRST, [case["entrance"]])
            return r3 & MASK
        entry = SAVE if op == "save" else LOAD
        r3, _, _ = T.call_ppc(e, entry, [FILE, w.inst])
        return T.s32(r3)


class Host(Side):
    mac, psize = False, 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case.get("queues"))
        data = case.get("stream", b"")
        lib.tpw_ctr_begin(data, len(data))

    def done(self):
        pass

    def off(self, struct, name):
        return HOST[struct][name]

    def size(self, struct):
        return HOST_SIZE[struct]

    def alloc(self, n):
        return self.lib.tpw_ctr_alloc(max(n, 16))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, l, a):
        self.lib.tpw_cl_label(l, a)

    def lab(self, a):
        return self.lib.tpw_cl_lab(a) if a else 0

    def pack_ptr(self, v):
        return struct.pack("<Q", v)

    def unpack_ptr(self, raw):
        return struct.unpack("<Q", raw)[0]

    def pack_word(self, v):
        return struct.pack("<I", v & MASK)

    def unpack_word(self, raw):
        return struct.unpack("<I", raw)[0]

    def set_ride_set(self, i, a):
        self.write(self.lib.tpw_ctr_addr(b"rideanim") + 8 * i, self.pack_ptr(a))

    def get_ride_set(self, i):
        return self.unpack_ptr(self.read(self.lib.tpw_ctr_addr(b"rideanim") + 8 * i, 8))

    def set_instances(self, head):
        self.write(self.lib.tpw_ctr_addr(b"instances"), self.pack_ptr(head))

    def set_fake_world(self, types, cars, count):
        self.lib.tpw_ctr_set_world(types[0], types[0], types[1], cars, count)

    def written(self):
        n = self.lib.tpw_ctr_out_length()
        out = (C.c_uint8 * max(n, 1))()
        self.lib.tpw_ctr_out_data(out)
        return bytes(out)[:n]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case, w):
        lib, op = self.lib, case["op"]
        if op == "instance":
            lib.tpw_ctr_Instance(w.ty[0], w.inst, case["flag"])
            return 0
        if op == "destroy":
            lib.tpw_ctr_Destroy(w.inst)
            return 0
        if op == "first":
            return lib.tpw_ctr_First(case["entrance"])
        if op == "save":
            return lib.tpw_ctr_Save(FILE, w.inst)
        return lib.tpw_ctr_Load(FILE, w.inst)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ctr_addr.restype, lib.tpw_ctr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ctr_alloc.restype, lib.tpw_ctr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ctr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ctr_begin.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_ctr_set_world.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_ctr_out_length.restype = C.c_int32
    lib.tpw_ctr_out_data.argtypes = [C.POINTER(C.c_uint8)]
    lib.tpw_ctr_Instance.argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_ctr_Destroy.argtypes = [C.c_uint64]
    lib.tpw_ctr_First.restype, lib.tpw_ctr_First.argtypes = C.c_uint32, [C.c_uint32]
    for n in ("Save", "Load"):
        f = getattr(lib, "tpw_ctr_" + n)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    for n in ("swap16", "swap32i", "swap32u", "swapf"):
        getattr(lib, "tpw_ctr_" + n).argtypes = [C.c_uint64]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    out = (C.c_int64 * 512)()
    n = lib.tpw_ctr_layout(out)
    got, i = list(out)[:n], 0
    for s in ORDER:
        HOST[s] = {}
        for name, _, _ in FIELDS[s][0]:
            HOST[s][name] = got[i]
            i += 1
        HOST_SIZE[s] = got[i]
        i += 1
    assert i == n and got == HOST_LAYOUT, got
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

L_INST, L_T0, L_T1, L_CT, L_SET, L_MESH = 1, 2, 3, 4, 10, 20
L_HEADS, L_CARS, L_WORDS, L_TRAIN = 30, 31, 33, 40      # the allocator fakes' labels start at 100 (the head blocks have no label: regions)
SLOTS = 5          # cars per train in the case's car block
TRAINS = 4         # trains in the case's train block
BLOCK_WORDS = 300  # words of a car's head block


class W:
    pass


def build_common(side, spec):
    w = W()
    w.regions, w.blocks = [], []
    w.ct = []
    for i, c in enumerate(spec["ctypes"]):
        a = side.new("ctype", L_CT + i)
        side.put("ctype", a, "mRideType", c["ride"])
        side.put("ctype", a, "mHeadCount", c["heads"])
        w.ct.append(a)
    w.ty = []
    for i, t in enumerate(spec["types"]):
        a = side.new("type", L_T0 + i)
        side.put("type", a, "mF2E8", t["n"])
        side.put("type", a, "mF2F4", t["c"])
        side.put("type", a, "mF318", t["f"])
        for k in "ABC":
            side.put("type", a, "mCarType" + k, w.ct[t[k]])
        w.ty.append(a)
    w.sets = []
    for i, s in enumerate(spec["sets"]):
        mesh = side.raw(16, L_MESH + i)
        a = side.new("set", L_SET + i)
        side.put("set", a, "mFlags", s["flag"])
        side.put("set", a, "mHeads", mesh)
        side.put("set", a, "mFC0", s["fc0"])
        side.put("set", a, "mFC4", s["fc4"])
        side.set_ride_set(i, a)
        w.sets.append(a)
    w.inst = side.new("inst", L_INST)
    return w


def put_fields(side, struct, addr, values):
    for name, v in values.items():
        side.put(struct, addr, name, v)


def build_trains(side, w, case):
    """The destroy, save and load worlds: the instance with its four trains, 5 car slots each, head words and blocks."""
    sp = case["spec"]
    ts, cs = side.stride("train"), side.stride("car")
    w.trains = side.new("train", None, TRAINS)       # train 0 carries the block's label (L_TRAIN)
    w.cars = side.new("car", L_CARS, TRAINS * SLOTS)
    w.heads = side.raw(4 * 64, L_HEADS)
    w.words = side.raw(4 * TRAINS, L_WORDS)
    side.put("inst", w.inst, "mFlags", case["inst"]["flags"])
    side.put("inst", w.inst, "mType", w.ty[case["inst"].get("type", 0)])
    side.put("inst", w.inst, "mTrains", w.trains if case["inst"].get("trains", True) else 0)
    side.put("inst", w.inst, "mCars", w.cars)
    side.put("inst", w.inst, "mHeads", w.heads)
    side.put("inst", w.inst, "mTrainWords", w.words)
    for t in range(TRAINS):
        a = w.trains + t * ts
        side.label(L_TRAIN + t, a)
        tr = case["trains"][t]
        put_fields(side, "train", a, tr["fields"])
        side.put("train", a, "mType", w.ty[tr["type"]])
        side.put("train", a, "mCars", w.cars + t * SLOTS * cs)
        side.put("train", a, "mFirstPersonCar", w.cars + t * SLOTS * cs)
        for j in range(SLOTS):
            ca = w.cars + (t * SLOTS + j) * cs
            car = case["cars"][t][j]
            put_fields(side, "car", ca, car["fields"])
            side.put("car", ca, "mType", w.ct[car["ct"]])
            blocks = []
            for k in range(4):
                b = side.raw(4 * BLOCK_WORDS)
                side.set_words(b, car["blocks"][k])
                blocks.append(b)
                w.blocks.append((b, BLOCK_WORDS))
                w.regions.append(("b%d" % len(w.blocks), b, 4, BLOCK_WORDS))
            side.put("car", ca, "mHeadsA", blocks[2])
            side.put("car", ca, "mHeadsB", blocks[3])
            side.put("car", ca, "mCurrentA", blocks[0] if car["hasA"] else 0)
            side.put("car", ca, "mCurrentB", blocks[1] if car["hasB"] else 0)
    w.ntr, w.ncar = TRAINS, TRAINS * SLOTS
    w.regions += [("train", w.trains, ts, TRAINS), ("car", w.cars, cs, TRAINS * SLOTS + 1)]
    side.set_fake_world(w.ty, w.cars, TRAINS * SLOTS)


def desc(side, w, ptr):
    if ptr == 0:
        return 0
    if side.lab(ptr) > 0 and not getattr(w, "force_heads", False):
        return ("L", side.lab(ptr))
    for name, base, stride, count in w.regions:
        if base <= ptr < base + stride * count:
            return (name, (ptr - base) // stride, (ptr - base) % stride)
    if side.lab(ptr) > 0:
        return ("L", side.lab(ptr))
    return ("L", side.lab(ptr))


def fields_of(side, w, struct, addr):
    out = {}
    for name, _, kind in FIELDS[struct][0]:
        v = side.get(struct, addr, name)
        if kind in "pt" and v and getattr(w, "force_heads", False) and name in ("mHeadsA", "mHeadsB", "mCurrentA", "mCurrentB", "mHeads"):
            out[name] = ("head", (v - w.heads) // 4, (v - w.heads) % 4)      # the instance world: the head words may run past the block
        else:
            out[name] = desc(side, w, v) if kind in "pt" else v
    return out


def snapshot(side, w):
    ts, cs = side.stride("train"), side.stride("car")
    out = {"inst": fields_of(side, w, "inst", w.inst)}
    out["trains"] = [fields_of(side, w, "train", w.trains + i * ts) for i in range(w.ntr)]
    out["cars"] = [fields_of(side, w, "car", w.cars + i * cs) for i in range(w.ncar)]
    out["types"] = [(side.get("type", a, "mF2E8"), side.get("type", a, "mF2F4")) for a in w.ty]
    out["rides"] = [desc(side, w, side.get_ride_set(i)) for i in range(5)]
    out["blocks"] = [side.wordlist(a, n) for a, n in w.blocks]
    return out


def run(side, case, builder, after=None):
    """Builds the world, calls, snapshots. The side's done() runs last."""
    try:
        w = build_common(side, case["spec"])
        builder(side, w, case)
        answer = side.call(case, w)
        if after:
            after(side, w, case)
        return {"answer": answer, "log": side.log(), "written": side.written(), "world": snapshot(side, w) if case["op"] != "first" else 0}
    finally:
        side.done()


# ---- the instance world
def build_instance(side, w, case):
    t0 = case["spec"]["types"][0]
    side.put("inst", w.inst, "mFlags", case["inst_flags"])
    side.put("inst", w.inst, "mType", w.ty[0])
    side.set_fake_world(w.ty, 0, 0)
    w.ntr = w.ncar = 0


def after_instance(side, w, case):
    t0 = case["spec"]["types"][0]
    ts, cs = side.stride("train"), side.stride("car")
    w.trains = side.get("inst", w.inst, "mTrains")
    w.cars = side.get("inst", w.inst, "mCars")
    w.heads = side.get("inst", w.inst, "mHeads")
    w.words = side.get("inst", w.inst, "mTrainWords")
    w.ntr, w.ncar = t0["n"], t0["n"] * t0["c"]
    w.force_heads = True
    w.regions = [("words", w.words, 4, w.ntr), ("train", w.trains, ts, w.ntr), ("car", w.cars, cs, w.ncar + 1)]


# ---- the first-person car world: a list of instances, each with two trains and three cars
def build_first(side, w, case):
    ts, cs = side.stride("train"), side.stride("car")
    insts = []
    for i, ic in enumerate(case["insts"]):
        a = w.inst if i == 0 else side.new("inst", L_INST + i)
        trains = side.new("train", None, 2)
        cars = side.new("car", None, 3)
        put_fields(side, "inst", a, {"mFlags": ic["flags"], "mEntranceID": ic["entrance"]})
        side.put("inst", a, "mLeadTrain", trains + ic["lead"] * ts if ic["lead"] >= 0 else 0)
        for t in range(2):
            side.put("train", trains + t * ts, "mFirstPersonCar", cars + ic["fpc"][t] * cs)
        for j in range(3):
            side.put("car", cars + j * cs, "mRide", ic["rides"][j])
        insts.append(a)
    for i, a in enumerate(insts):
        side.put("inst", a, "mNext", insts[i + 1] if i + 1 < len(insts) else 0)
    side.set_instances(insts[0] if insts else 0)
    w.ntr = w.ncar = 0
    w.regions = []


# ---------------------------------------------------------------------------------------------------------------- the cases

FLOAT_BITS = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7FC00001, 0x00000001, 0x007FFFFF, 0x7F7FFFFF,
              0x437F0000, 0xC2C80000]
WORD_EDGES = [0, 1, 2, 4, 0x7F, 0x80, 0xFF, 0x100, 0x101, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]


def rword(r):
    k = r.random()
    if k < 0.2:
        return r.choice(WORD_EDGES)
    if k < 0.4:
        return r.randrange(16)
    return r.getrandbits(32)


def rfloat(r):
    if r.random() < 0.35:
        return r.choice(FLOAT_BITS)
    return struct.unpack(">I", struct.pack(">f", r.uniform(-1000, 1000)))[0]


FLOAT_FIELDS = {"mPosition", "mF78", "mF30", "mFrontSample", "mRearSample", "mF7C"}


def rand_fields(r, struct, skip=()):
    out = {}
    for name, _, kind in FIELDS[struct][0]:
        if kind == "w" and name not in skip:
            out[name] = rfloat(r) if name in FLOAT_FIELDS else rword(r)
    return out


def rand_spec(r, n=None, c=None, ride_choice=None, **kw):
    """The shared world: two coaster types, four car types, five ride sets."""
    cts = []
    for i in range(4):
        cts.append({"ride": ride_choice[i] if ride_choice else r.choice([0, 0, 1, 2, 3]), "heads": r.choice([0, 1, 1, 2, 7])})
    types = []
    for i in range(2):
        types.append({"n": n if (n is not None and i == 0) else r.randrange(0, 4), "c": c if (c is not None and i == 0) else r.randrange(1, 6),
                      "f": r.randrange(0, 40), "A": r.randrange(4), "B": r.randrange(4), "C": r.randrange(4)})
        if r.random() < 0.3:
            types[i]["B"] = types[i]["A"]
        if r.random() < 0.2:
            types[i]["C"] = types[i]["A"]
    sets = [{"flag": r.choice([0, 0x4000, 0x4000, r.getrandbits(32), 0x3000]), "fc0": r.getrandbits(32), "fc4": r.getrandbits(32)} for _ in range(5)]
    spec = {"ctypes": cts, "types": types, "sets": sets}
    spec.update(kw)
    return spec


# ---- instance
def instance_case(r, name, **kw):
    flag = kw.pop("flag", r.choice([0, 0, 1]))
    n, c = kw.pop("n", None), kw.pop("c", None)
    spec = rand_spec(r, n=n, c=c, **kw.pop("spec", {}))
    t0 = spec["types"][0]
    q = {}
    rides = kw.pop("rides", None)
    q["ride"] = rides if rides is not None else [r.randrange(0, 5) for _ in range(r.randrange(1, 6))]
    if r.random() < 0.15:
        q["n"] = [r.randrange(0, t0["n"] + 1)]
    if r.random() < 0.15:
        q["c"] = [r.randrange(0, 8)]
    if r.random() < 0.3 and t0["n"] * t0["c"] <= 5:
        # a cleared set must not be handed out again (the original would dereference NULL): every ride answered is different
        q["ride"] = r.sample(range(5), 5)
        q["clear"] = [r.choice([0, 1]) for _ in range(5)]
        q.pop("n", None)
        q.pop("c", None)
    return {"name": name, "op": "instance", "flag": flag, "spec": spec, "inst_flags": rword(r), "queues": q}


def instance_edges():
    r = random.Random(1921)
    for flag in (0, 1):
        for n in (0, 1, 3):
            for c in (1, 2, 3, 5):
                yield instance_case(r, "flag%d n%d c%d" % (flag, n, c), flag=flag, n=n, c=c)
    yield instance_case(r, "cars 0", n=2, c=0)
    # every car type a ride, A == B, and A != B, with the answers 0 and non-0, and a drawn range set
    for spec_ct in ([1, 1, 1, 1], [0, 0, 0, 0], [1, 2, 3, 0]):
        yield instance_case(r, "rides %s" % spec_ct, flag=0, n=2, c=3, spec={"ride_choice": spec_ct}, rides=[1, 2, 3, 4, 0, 4])
    yield instance_case(r, "answer 0", flag=0, n=1, c=2, spec={"ride_choice": [1, 1, 1, 1]}, rides=[0])
    c = instance_case(r, "set cleared by the mesh call", flag=0, n=2, c=2, spec={"ride_choice": [1, 1, 1, 1]}, rides=[2, 3, 1, 4, 0])
    c["queues"].pop("n", None)
    c["queues"].pop("c", None)
    c["queues"]["clear"] = [1, 0, 1, 0]
    yield c
    c = instance_case(r, "counts changed by InstanceRide", flag=0, n=3, c=3, spec={"ride_choice": [1, 1, 1, 1]}, rides=[1, 2, 3, 4])
    c["queues"]["n"] = [1]
    c["queues"]["c"] = [5]
    yield c
    for _ in range(8):
        yield instance_case(r, "heads and types")


def instance_random(r):
    return instance_case(r, "random")


# ---- destroy
def trains_case(r, op, name, **kw):
    spec = rand_spec(r, **kw.pop("spec", {}))
    cars = []
    trains = []
    for t in range(TRAINS):
        trains.append({"fields": rand_fields(r, "train", skip=()), "type": r.choice([0, 0, 1])})
        cars.append([])
        for j in range(SLOTS):
            cars[t].append({"fields": rand_fields(r, "car"), "ct": r.randrange(4), "hasA": True, "hasB": True,
                            "blocks": [[rword(r) for _ in range(BLOCK_WORDS)] for _ in range(4)]})
    return {"name": name, "op": op, "spec": spec, "trains": trains, "cars": cars, "inst": {"flags": rword(r)}, "queues": {}}


def destroy_case(r, name, flags=None, rides=None, **kw):
    c = trains_case(r, "destroy", name, **kw)
    c["spec"]["types"][0]["n"] = kw.get("n", r.randrange(0, TRAINS + 1))
    for t in range(TRAINS):
        f = c["trains"][t]["fields"]
        f["mFlags"] = (f["mFlags"] & ~2) | (2 if (flags[t] if flags else r.random() < 0.5) else 0)
        for j in range(SLOTS):
            on = rides[t][j] if rides else r.random() < 0.5
            c["cars"][t][j]["fields"]["mRide"] = (10 + t * SLOTS + j) if on else 0
    q = {}
    if r.random() < 0.2:
        q["retype"] = [r.choice([0, 1])]
    if r.random() < 0.2:
        q["ncount"] = [r.randrange(0, TRAINS + 1)]
    if r.random() < 0.15:
        q["itype"] = [1]
    if r.random() < 0.25:
        q["rmod"] = [r.choice([0, 77, 10 + r.randrange(20)])]
    c["queues"] = q
    c["inst"]["trains"] = kw.get("trains", True)
    return c


def destroy_edges():
    r = random.Random(1922)
    yield destroy_case(r, "no trains block", trains=False, n=2)
    for n in (0, 1, 2, 4):
        yield destroy_case(r, "n%d all active, all rides" % n, n=n, flags=[1] * 4, rides=[[1] * SLOTS] * 4)
        yield destroy_case(r, "n%d none active, no rides" % n, n=n, flags=[0] * 4, rides=[[0] * SLOTS] * 4)
        yield destroy_case(r, "n%d random" % n, n=n)
    c = destroy_case(r, "train type changed by Deactivate", n=3, flags=[1] * 4)
    c["queues"] = {"retype": [1, 0, 1]}
    yield c
    c = destroy_case(r, "train count changed by Deactivate", n=4, flags=[1] * 4)
    c["queues"] = {"ncount": [1]}
    yield c
    c = destroy_case(r, "instance type changed by Deactivate", n=4, flags=[1] * 4)
    c["queues"] = {"itype": [1]}
    yield c
    c = destroy_case(r, "ride changed by DisableCoasterCarDrawSafe", n=2, flags=[0] * 4, rides=[[1] * SLOTS] * 4, spec={"c": 3})
    c["queues"] = {"rmod": [0, 55, 10]}
    yield c
    for _ in range(6):
        yield destroy_case(r, "random edge")


def destroy_random(r):
    return destroy_case(r, "random")


# ---- first-person car
def first_case(r, name, m=None, entrance=None):
    m = r.randrange(0, 4) if m is None else m
    insts = []
    for i in range(m):
        insts.append({"flags": rword(r) & ~0x800 | (0x800 if r.random() < 0.6 else 0), "entrance": r.choice([5, 6, 7, 5]),
                      "lead": r.randrange(2), "fpc": [r.randrange(3), r.randrange(3)], "rides": [rword(r) for _ in range(3)]})
        if not insts[i]["flags"] & 0x800:
            insts[i]["lead"] = r.choice([-1, 0, 1])
    return {"name": name, "op": "first", "spec": rand_spec(r), "insts": insts, "entrance": entrance if entrance is not None else r.choice([4, 5, 6, 7, 8]),
            "queues": {}}


def first_edges():
    r = random.Random(1923)
    yield first_case(r, "empty list", m=0, entrance=5)
    for m in (1, 2, 3):
        for e in (5, 6, 9):
            yield first_case(r, "m%d entrance %d" % (m, e), m=m, entrance=e)
    c = first_case(r, "first match wins", m=2, entrance=5)
    for i in c["insts"]:
        i["entrance"], i["flags"], i["lead"] = 5, 0x800, 0
    c["insts"][0]["rides"], c["insts"][1]["rides"] = [1, 2, 3], [4, 5, 6]
    c["insts"][0]["fpc"] = [1, 1]
    yield c
    c = first_case(r, "first has no flag, second has", m=2, entrance=5)
    c["insts"][0].update({"entrance": 5, "flags": 0, "lead": -1})
    c["insts"][1].update({"entrance": 5, "flags": 0x800, "lead": 1})
    yield c
    c = first_case(r, "flag 0x800 with other bits", m=1, entrance=5)
    c["insts"][0].update({"entrance": 5, "flags": 0xFFFFF7FF, "lead": 0})
    yield c


def first_random(r):
    return first_case(r, "random")


# ---- save and load
def stream_sizes(case, op):
    """The calls' sequence the function makes when nothing fails: ('rec', size) and ('arr', array) entries."""
    t0 = case["spec"]["types"][case["inst"].get("type", 0)]
    seq = []
    for t in range(t0["n"]):
        seq.append(("rec", 0x1B))
        tr = case["trains"][t]
        for j in range(case["spec"]["types"][tr["type"]]["c"]):
            seq.append(("rec", 0xB))
            car = case["cars"][t][j]
            f = car["fields"]
            if op == "save":
                if f["mCountA"] != 0:
                    seq.append(("arr", "A"))
                if f["mCountB"] != 0:
                    seq.append(("arr", "B"))
            elif car["hasA"] and car["hasB"]:
                seq.append(("arr", "A"))
                seq.append(("arr", "B"))
    return seq


def save_load_case(r, op, name, n=None, fail=None, **kw):
    c = trains_case(r, op, name, spec={"n": n} if n is not None else {})
    t0 = c["spec"]["types"][0]
    t0["n"] = n if n is not None else r.randrange(0, 4)
    for t in range(TRAINS):
        f = c["trains"][t]["fields"]
        f["mCarsInUse"] = r.choice([0, 1, 5, 0xFF, 0x100, 0x1234, rword(r)])
        for j in range(SLOTS):
            car = c["cars"][t][j]
            cf = car["fields"]
            cf["mCountA"] = r.choice([0, 0, 1, 2, 5, 17, 255, 256, 261, 299]) if op == "save" else rword(r)
            cf["mCountB"] = r.choice([0, 0, 1, 3, 9, 255, 258, 299]) if op == "save" else rword(r)
            cf["mRide"] = rword(r)
            if op == "load":
                car["hasA"] = r.random() < 0.8
                car["hasB"] = r.random() < 0.85
    if "cars" in kw:
        for t in range(TRAINS):
            c["spec"]["types"][c["trains"][t]["type"]]["c"] = kw["cars"]
    c["fail"] = fail
    finish_queues(r, c, op)
    return c


def finish_queues(r, c, op):
    """The stream (load) and the queues of transferred counts and errors, with an optional failure at call number c['fail'] = (index, kind)."""
    seq = stream_sizes(c, op)
    fail = c["fail"]
    xfer, err = [], []
    for i, (kind, what) in enumerate(seq):
        bad = fail is not None and fail[0] == i
        if kind == "rec":
            xfer.append(fail[1] if bad else what)
            err.append(-1 if r.random() < 0.1 else 0)
        else:
            err.append(-1 if bad else 0)
    c["queues"] = {"xfer": xfer, "err": err}
    if not xfer:
        c["queues"].pop("xfer")
    if not err:
        c["queues"].pop("err")
    if op == "load":
        c["stream"] = load_stream(r, c)
    c["rides_ok"] = True


def load_stream(r, c):
    """The file the original wrote, little-endian: per train its record, per car its record and its two arrays when both current pointers are set."""
    out = bytearray()
    t0 = c["spec"]["types"][c["inst"].get("type", 0)]
    for t in range(t0["n"]):
        tr = c["trains"][t]
        out += struct.pack("<I", rword(r)) + bytes([r.choice([0, 1, 7, 255])]) + struct.pack("<I", rfloat(r)) * 2
        out += struct.pack("<I", rword(r)) + struct.pack("<I", rword(r)) + struct.pack("<I", rword(r)) + struct.pack("<H", r.getrandbits(16))
        assert len(out) % 27 == 0 or True
        for j in range(c["spec"]["types"][tr["type"]]["c"]):
            car = c["cars"][t][j]
            ct = c["spec"]["ctypes"][car["ct"]]
            flags = r.choice([0, 1, 0xFFFFFFFE, 0xFFFFFFFF, rword(r)])
            ride = r.randrange(0, 5) if ct["ride"] else rword(r)
            na, nb = r.choice([0, 0, 1, 2, 5, 17, 255]), r.choice([0, 1, 3, 9, 255])
            out += struct.pack("<I", flags) + bytes([na, nb, r.choice([0, 1, 255])]) + struct.pack("<I", ride)
            if car["hasA"] and car["hasB"]:
                words = [rword(r) for _ in range(na + nb)]
                if c.get("fail") is not None:     # a failed call can leave an array unswapped: its words are byte-palindromes, the same in either byte order
                    words = [(w & 0xFFFF) | ((((w & 0xFF) << 8) | ((w >> 8) & 0xFF)) << 16) for w in words]
                out += b"".join(struct.pack("<I", w) for w in words)
    return bytes(out)


def pick_fail(r, c, op):
    seq = stream_sizes(c, op)
    if not seq:
        return None
    i = r.randrange(len(seq))
    kind = seq[i][0]
    if kind == "rec":
        size = seq[i][1]
        return (i, r.choice([0, size - 1, size + size - 1 + 0, 2 * size - 0, 2 * size, size + 1, 3 * size]) if r.random() < 0.8 else 0)
    return (i, -1)


def with_fail(r, op, name, **kw):
    c = save_load_case(r, op, name, **kw)
    c["fail"] = pick_fail(r, c, op)
    finish_queues(r, c, op)
    return c


def sl_edges(op):
    r = random.Random(1924 if op == "save" else 1925)
    for n in (0, 1, 2, 3):
        yield save_load_case(r, op, "n%d" % n, n=n)
    for cars in (1, 2, 5):
        yield save_load_case(r, op, "cars %d" % cars, n=2, cars=cars)
    for i in range(6):
        yield with_fail(r, op, "failure %d" % i, n=r.randrange(1, 4))
    # the exact record counts
    for size, label in ((0x1B, "train"), (0xB, "car")):
        for k in (0, size - 1, size, 2 * size - 1, 2 * size, 1):
            c = save_load_case(r, op, "%s record transferred %d" % (label, k), n=2)
            idx = 0 if label == "train" else 1
            seq = stream_sizes(c, op)
            c["fail"] = (idx, k)
            finish_queues(r, c, op)
            yield c
    for i in range(4):
        yield save_load_case(r, op, "random edge", n=r.randrange(0, 4))


def save_edges():
    return sl_edges("save")


def load_edges():
    return sl_edges("load")


def save_random(r):
    if r.random() < 0.3:
        return with_fail(r, "save", "random failure")
    return save_load_case(r, "save", "random")


def load_random(r):
    if r.random() < 0.3:
        return with_fail(r, "load", "random failure")
    return save_load_case(r, "load", "random")


# ---------------------------------------------------------------------------------------------------------------- the runs


def ppc(e, case):
    side = Ppc(e, case)
    op = case["op"]
    if op == "instance":
        return run(side, case, build_instance, after_instance)
    if op == "first":
        return run(side, case, build_first)
    return run(side, case, build_trains)


def host(lib, case):
    side = Host(lib, case)
    op = case["op"]
    if op == "instance":
        return run(side, case, build_instance, after_instance)
    if op == "first":
        return run(side, case, build_first)
    return run(side, case, build_trains)


TESTS = [
    FnTest("InstanceCoasterTrains", ppc, host, edge=instance_edges, rnd=instance_random),
    FnTest("DestroyCoasterTrains", ppc, host, edge=destroy_edges, rnd=destroy_random),
    FnTest("GetCoaster1stPersonCar", ppc, host, edge=first_edges, rnd=first_random),
    FnTest("SaveTrainsAndCars", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("LoadTrainsAndCars", ppc, host, edge=load_edges, rnd=load_random),
]
