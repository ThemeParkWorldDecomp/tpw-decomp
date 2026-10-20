"""Unit w79e, the sound manager, against libtpw_unit_SoundManager.dylib (tests/unit_SoundManager.cpp), in the program "sound symbols
shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/TbSoundManager.cpp  TbSoundManager: constructor 0x10026e30, destructor 0x10027f20, Init 0x10026fa4, GetSoundPath 0x10016a48,
                          SetListenerPosition 0x1001670c, GetListenerPosition 0x10017cf4, SetAudioSystem 0x100167b4,
                          SetDistanceMapping 0x10027274, GetDistanceMultiplier 0x1001966c, IsQSoundOn 0x1001954c,
                          GetRolloffFactorToMinMaxDistance 0x1001960c, GetVolumeControl 0x10019758, GetVolume 0x1002a080,
                          VolumeControlIsOn 0x10016e60, UpdateVolumeControl 0x100279c4, ToggleVolumeControl 0x10027adc,
                          SetQSoundInfo 0x10027160, SetListenerOrientation 0x10027b60, GetMaxNumberOfChannels 0x10027020,
                          GetDirectSoundObject 0x100278c0, GetBankData 0x10027334, IsBankDroppable 0x100272b8, SetDroppable
                          0x10027408, GetBank 0x1002748c, ConstructSample 0x10027c90, PlaySample 0x10027d94

Each function runs as the original in the emulator and as the host's translation on a manager built from the case: the real
constructor, then the case's presets written into the members (options, distance, control bytes, volumes, droppable byte, the
listener, the audio component, the starting sound path, a sample store) and, for the bank functions, a bank deque of up to three
containers built over 0xA5 (flags, name, a storage whose vtable records, a cached bank or none). The manager runs over the real
core (w79b) with the commands' Execute bodies faked (w79d is not landed): SetSystemInfo, UpdateAllVolumes and Start record the
core they were given and the words they carry, so the log shows the command objects the manager handed to SystemCommand. Mode 1
replaces the manager's GetSystemInfo slot (+0x08) with a recording fake (the real core never answers the DirectSound bit). Every
callee outside the unit is a fake that records the same event on both sides: the audio component (NewControl and NewSample, with
the sample info's words), the storage (OpenFile, FullPath), the file (constructor, destructor), CFileOperation::GetBankPath,
CBankFactory::GenerateBank, TbMapStreamer::SetDroppable and the new of the global copier. The unit's own functions, the core, the
w77 info records, the w79a handle code and the strings run as the original. TbError is returned through a hidden r3 buffer (this
in r4) and read from the buffer.

Pointers compare as labels (0 NULL, the label of the object, -1 for anything else); floats compare as bit patterns with every NaN
one value. UpdateVolumeControl's out-of-range indexes (4 and up) compare the written word only: the host's members after the volumes
sit at other offsets than the Mac's.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundManager"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5
ONE = 0x3F800000

S_COPY = 0x1003F34C       # CMemoryCopy::_mpCopy
S_SPEAKER = 0x1003F348    # TbSpeaker::AutoDelete::_mpSpeaker
S_COUNT = 0x1003F350      # CChannelState::mChannelState
MGR_VT, MGR_SUB_VT = 0x1003D668, 0x1003D68C
MGR_SLOTS = {0x08: 0x1000BC8C, 0x0C: 0x1000C058, 0x10: 0x1000C220, 0x14: 0x1000C404, 0x18: 0x1000C470, 0x1C: 0x1000C500,
             0x20: 0x1000C53C, 0x34: 0x10027FCC, 0x38: 0x1000B85C, 0x3C: 0x10027F20, 0x40: 0x10026FA4, 0x44: 0x1001960C}
STD_COPY_VT = 0x1003C900

A = dict(
    ctor=0x10026E30, dtor=0x10027F20, init=0x10026FA4, getpath=0x10016A48, setlp=0x1001670C, getlp=0x10017CF4,
    setaudio=0x100167B4, setdist=0x10027274, getdist=0x1001966C, isq=0x1001954C, rolloff=0x1001960C, getvc=0x10019758,
    getvol=0x1002A080, volon=0x10016E60, updvol=0x100279C4, toggle=0x10027ADC, setq=0x10027160, orient=0x10027B60,
    getmax=0x10027020, getdirect=0x100278C0, getbankdata=0x10027334, isdrop=0x100272B8, setdrop=0x10027408,
    getbank=0x1002748C, construct=0x10027C90, play=0x10027D94,
)
# the callees outside the unit
INSTANCE, STREAMER_SETDROP = 0x1001F6EC, 0x100231FC
GETBANKPATH, GENERATEBANK = 0x1001E720, 0x100141C0
EXEC_SETINFO, EXEC_UPDATE, EXEC_START = 0x10015B8C, 0x10016050, 0x100154E4
STORE_MAX, STORE_USED = 0x1000BEB4, 0x1000BE68
COMP_SUB_VT = 0x10035C48

# the host's operation numbers (enum Op in the unit file)
(K_CTOR, K_DTOR, K_INIT, K_GETPATH, K_SETLP, K_GETLP, K_SETAUDIO, K_SETDIST, K_GETDIST, K_ISQ, K_ROLLOFF, K_GETVC, K_GETVOL,
 K_VOLON, K_UPDVOL, K_TOGGLE, K_SETQ, K_ORIENT, K_GETMAX, K_GETDIRECT, K_GETBANKDATA, K_ISDROP, K_SETDROP, K_GETBANK,
 K_CONSTRUCT, K_PLAY) = range(26)
OP_ADDRESS = {
    K_CTOR: "ctor", K_DTOR: "dtor", K_INIT: "init", K_GETPATH: "getpath", K_SETLP: "setlp", K_GETLP: "getlp",
    K_SETAUDIO: "setaudio", K_SETDIST: "setdist", K_GETDIST: "getdist", K_ISQ: "isq", K_ROLLOFF: "rolloff", K_GETVC: "getvc",
    K_GETVOL: "getvol", K_VOLON: "volon", K_UPDVOL: "updvol", K_TOGGLE: "toggle", K_SETQ: "setq", K_ORIENT: "orient",
    K_GETMAX: "getmax", K_GETDIRECT: "getdirect", K_GETBANKDATA: "getbankdata", K_ISDROP: "isdrop", K_SETDROP: "setdrop",
    K_GETBANK: "getbank", K_CONSTRUCT: "construct", K_PLAY: "play",
}
# the operations that return a TbError through the hidden buffer (this moves to r4)
HIDDEN = {K_INIT, K_UPDVOL, K_TOGGLE, K_SETQ, K_ORIENT, K_GETMAX, K_GETDIRECT}
QUEUE_ERRORS = [0, -1, 1, 5, -2, 0x7FFFFFFF, -0x80000000]
DTOR_FLAGS = [0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, -0x8000]

POS_VALID = [0x3F800000, 0x40000000, 0xC0400000]       # 1.0, 2.0, -3.0
BAD_WORDS = [0x7FC00000, 0x7F800000, 0xFF800000]       # a quiet NaN, +inf, -inf
VT = {}
HOOK = {}


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def lbl(L, address):
    return L.lab(address) & MASK


def words_at(e, address, count):
    return list(struct.unpack(">%dI" % count, e.read(address, 4 * count)))


def canon(bits):
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x007FFFFF) else bits


def f1_bits(e):
    return canon(struct.unpack(">I", struct.pack(">f", e.f(1)))[0])


def bits_to_float(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


# ---------------------------------------------------------------- the PowerPC side: strings and records

def string(e, obj):
    ptr, count = e.u32(obj + 8), e.u32(obj + 4)
    return e.read(ptr, count).decode("latin-1") if ptr else ""


def set_string(e, obj, value):
    raw = value.encode("latin-1")
    ptr = e.alloc(len(raw) + 1, raw + b"\0")
    e.write(obj, pack([e.string_vt, len(raw), ptr, len(raw), 0]))
    e.set_r(3, obj)


def make_vtable(e, slots):
    table = T.persist_alloc(e, 0x50)
    for slot, callback in slots.items():
        e.write(table + slot, pack([T.fake_function(e, callback)]))
    return table


def rec_sys(L, e, name, first, address):
    """The words of a system info the constructor and the setters define (the unit file's RecInfo)."""
    w = words_at(e, address, 19)
    mask = w[6]
    L.rec(name, [first & MASK, w[0], w[1], w[2], mask, w[7], w[8], w[16],
                 lbl(L, w[11]) if mask & 4 else 0, lbl(L, w[12]) if mask & 8 else 0,
                 lbl(L, w[13]) if mask & 8 else 0, lbl(L, w[14]) if mask & 0x10 else 0])


def stub_new(e):
    L, size = e.cl, e.r(3)
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    if L.track:
        L.rec("new", (size, L.newlab(block)))
    e.set_r(3, block)


def stub_delete(e):
    L, block = e.cl, e.r(3)
    if block != 0 and L.lab(block) > 0:
        L.rec("delete", (L.lab(block),))
        L.forget(block)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    assert lib.tpw_sm_sizes(0) >= 0x68, "the host's manager is at least the Mac's"
    lib.tpw_sm_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_sm_path.restype = C.c_char_p

    # ---- the functional strings (unrecorded), as in w70's module
    def assign_text(e):
        set_string(e, e.r(3), e.cstr(e.r(4)) if e.r(4) else "")

    def assign_base(e):
        set_string(e, e.r(3), string(e, e.r(4)))

    def append_text(e):
        set_string(e, e.r(3), string(e, e.r(3)) + e.cstr(e.r(4)))

    def append_base(e):
        set_string(e, e.r(3), string(e, e.r(3)) + string(e, e.r(4)))

    def append_char(e):
        ch = e.read(e.r(4), 1).decode("latin-1")
        set_string(e, e.r(3), string(e, e.r(3)) + (ch if ch != "\0" else ""))

    e.string_vt = make_vtable(e, {0x18: assign_base, 0x1c: assign_base, 0x20: assign_text,
                                  0x28: append_base, 0x2c: append_char, 0x30: append_text})
    e.stub("__ct__26TbDynamicStringTemplate<c>Fv", lambda e: set_string(e, e.r(3), ""))
    e.stub("__ct__26TbDynamicStringTemplate<c>FPCc", assign_text)
    for signature in ["__ct__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>",
                      "__as__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>",
                      "__as__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>"]:
        e.stub(signature, assign_base)
    e.stub("__as__26TbDynamicStringTemplate<c>FPCc", assign_text)
    e.stub("__apl__26TbDynamicStringTemplate<c>FPCc", append_text)
    e.stub("__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", append_base)
    e.stub("__apl__26TbDynamicStringTemplate<c>FRCc", append_char)
    e.stub("__dt__26TbDynamicStringTemplate<c>Fv", lambda e: None)

    # ---- the MathLib imports Valid() makes (as in w77's module)
    def classify(e):
        v = e.f(1)
        bits = struct.unpack(">Q", struct.pack(">d", v))[0]
        if math.isnan(v):
            cls = 1 if bits & (1 << 51) else 0
        elif math.isinf(v):
            cls = 2
        elif v == 0.0:
            cls = 3
        elif abs(v) < 2.0 ** -126:
            cls = 5
        else:
            cls = 4
        e.set_r(3, cls)

    e.stub("__fpclassifyf", classify)
    e.stub("__isfinitef", lambda e: e.set_r(3, 0 if math.isnan(e.f(1)) or math.isinf(e.f(1)) else 1))
    e.stub("memcpy", lambda e: e.write(e.r(3), e.read(e.r(4), e.r(5))) if e.r(5) else None)
    e.stub("__nw__FUl", stub_new)
    e.stub("__dl__FPv", stub_delete)

    # ---- the TbRefRelease imports of the bullfrog library (src/TbRefRelease.cpp): the count is the word after the vptr
    def ref_ctor(e):
        e.write(e.r(3) + 4, pack([1]))

    def ref_addref(e):
        sub = e.r(3)
        count = (e.u32(sub + 4) + 1) & MASK
        e.write(sub + 4, pack([count]))
        e.set_r(3, count)

    def ref_release(e, call):
        sub = e.r(3)
        count = (e.u32(sub + 4) - 1) & MASK
        e.write(sub + 4, pack([count]))
        if count == 0:
            call(e.u32(e.u32(sub) + 0x10), sub, 1)
            return 0
        return count

    e.stub("__ct__12TbRefReleaseFv", ref_ctor)
    e.stub("__dt__12TbRefReleaseFv", lambda e: None)
    e.stub("AddRef__12TbRefReleaseFv", ref_addref)
    T.stub_cb(e, "Release__12TbRefReleaseFv", ref_release)

    # ---- the file (constructed and destroyed on GetBank's stack)
    def file_ctor(e):
        L, this = e.cl, e.r(3)
        L.rec("File.ctor", (L.newlab(this),))
        e.write(this, pack([0, 0]))
        e.set_r(3, this)

    def file_dtor(e):
        L, this = e.cl, e.r(3)
        L.rec("File.dtor", (L.lab(this),))
        L.forget(this)

    e.stub("__ct__Q33bfl10FileSystem5CFileFv", file_ctor)
    e.stub("__dt__Q33bfl10FileSystem5CFileFv", file_dtor)

    # ---- the stand-ins in the binary: the streamer, the commands' Execute bodies, the store, the bank pieces
    def instance(e):
        e.set_r(3, e.w.streamer)

    def set_droppable(e):
        L = e.cl
        L.rec("Streamer.SetDroppable", (L.lab(e.r(3)), e.r(4) & 0xFF))

    def exec_info(e):
        L = e.cl
        rec_sys(L, e, "SetSystemInfo.Execute", L.lab(e.r(5)), e.u32(e.r(4) + 4))
        e.write(e.r(3), pack([L.ret("Exec.SetSystemInfo", 0)]))

    def exec_update(e):
        L = e.cl
        L.rec("UpdateAllVolumes.Execute", (L.lab(e.r(5)),))
        e.write(e.r(3), pack([L.ret("Exec.UpdateAllVolumes", 0)]))

    def exec_start(e):
        L = e.cl
        L.rec("Start.Execute", (L.lab(e.r(5)), e.u32(e.r(4) + 4)))
        e.write(e.r(3), pack([L.ret("Exec.Start", 0)]))

    def store_max(e):
        L = e.cl
        L.rec("Store.GetMaxNumberOfChannels", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("Store.Max", 0)))

    def get_bank_path(e):
        # r3 = the empty CFileOperation (a stack word), r4 = the storage, r5 = the string that gets "HD.sdt", r6 = the other string
        L, path, other = e.cl, e.r(5), e.r(6)
        L.rec("GetBankPath", (L.lab(e.r(4)), e.r(7) & MASK), (string(e, path), string(e, other)))
        answer = L.ret("GetBankPath", 1)
        if answer != 0:
            set_string(e, path, string(e, path) + "HD.sdt")
        e.set_r(3, T.u32(answer))

    def generate_bank(e):
        L, info, component = e.cl, e.r(4), e.r(5)
        file = e.u32(info + 0xC)
        path = e.u32(info + 8)
        L.rec("Factory.GenerateBank", (e.u32(info), L.lab(file), L.lab(component)), (e.cstr(path) if path else "(null)",))
        if L.ret("GenBank", 0) == 0:
            e.set_r(3, 0)
            return
        block = e.alloc(16, b"\xa5" * 16)
        L.newlab(block)
        e.set_r(3, block)

    for address, fn in {INSTANCE: instance, STREAMER_SETDROP: set_droppable, EXEC_SETINFO: exec_info,
                        EXEC_UPDATE: exec_update, EXEC_START: exec_start, STORE_MAX: store_max,
                        GETBANKPATH: get_bank_path, GENERATEBANK: generate_bank}.items():
        T.stub_internal(e, address, fn)

    # ---- the storage vtable: OpenFile (+0x14) and FullPath (+0x1c) record
    def storage_open(e):
        L = e.cl
        L.rec("Storage.OpenFile", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(7) & MASK, e.r(8) & MASK), (e.cstr(e.r(6)),))
        e.write(e.r(3), pack([L.ret("Storage.Open", 0)]))

    def storage_fullpath(e):
        L, buf = e.cl, e.r(3)
        L.rec("Storage.FullPath", (L.lab(e.r(4)),), (string(e, e.r(6)),))
        set_string(e, e.r(5), "R:")
        e.write(buf, pack([L.ret("Storage.FullPath", 0)]))

    VT["storage"] = make_vtable(e, {0x14: storage_open, 0x1c: storage_fullpath})

    # ---- the audio component the manager makes samples through: NewControl (+0x14) and NewSample (+0x10) record the sample
    # info's words; every other slot records its name
    def comp_new(name):
        def stub(e):
            L, info = e.cl, e.r(5)
            L.rec(name, (L.lab(e.r(3)), e.r(4) & MASK, L.lab(info), e.u32(info + 0xC), e.u32(info + 0x14)))
            e.set_r(3, T.u32(L.ret(name, 0)))
        return stub

    def comp_buf(name):
        def stub(e):
            e.cl.rec(name, (e.cl.lab(e.r(4)),))
            e.write(e.r(3), pack([-77]))
        return stub

    def comp_ptr(name):
        def stub(e):
            e.cl.rec(name, (e.cl.lab(e.r(3)),))
            e.set_r(3, 0)
        return stub

    words = [0] * 17
    words[2] = T.fake_function(e, comp_buf("Fake.GetSystemInfo"))
    words[3] = T.fake_function(e, comp_buf("Fake.GetSampleInfo"))
    words[4] = T.fake_function(e, comp_new("Fake.NewSample"))
    words[5] = T.fake_function(e, comp_new("Fake.NewControl"))
    words[6] = T.fake_function(e, comp_buf("Fake.SystemCommand"))
    words[7] = T.fake_function(e, comp_ptr("Fake.GetSystemModule"))
    words[8] = T.fake_function(e, comp_ptr("Fake.GetSampleStore"))
    VT["comp"] = T.persist_alloc(e, 4 * len(words), pack(words))
    VT["comp_sub"] = T.persist_alloc(e, 20, pack([0, 0, e.u32(COMP_SUB_VT + 8), e.u32(COMP_SUB_VT + 0xC), 0]))

    # ---- the hooked manager: the real vtable with slot +0x08 (GetSystemInfo) replaced by a recording fake
    def hook_get_system_info(e):
        L, buf, this, info = e.cl, e.r(3), e.r(4), e.r(5)
        rec_sys(L, e, "Hook.GetSystemInfo", L.lab(this), info)
        words = words_at(e, info, 19)
        words[6] |= T.u32(L.ret("Hook.mask", 0))
        words[5] = T.u32(L.ret("Hook.max", 0))
        words[0] = T.u32(L.ret("Hook.f0", 0))
        e.write(info, pack(words))
        e.write(buf, pack([L.ret("Hook.err", 0)]))

    table = bytearray(e.read(MGR_VT, 0x48))
    table[8:12] = pack([T.fake_function(e, hook_get_system_info)])
    HOOK["vt"] = T.persist_alloc(e, 0x48, bytes(table))


# ---------------------------------------------------------------- the case

TEXT_SLOTS = 5   # three bank names, the Init argument, the path the manager starts with


def pos_words(kind, r=None):
    """The three words of a position: a valid one, or one with a NaN or an infinity in the given place."""
    if kind == "v":
        return list(POS_VALID) if r is None else [r.choice([0, ONE, 0x40490FDB, 0xC1200000, r.getrandbits(32) & 0x7F7FFFFF])
                                                   for _ in range(3)]
    place, bad = kind
    words = list(POS_VALID)
    words[place] = bad
    return words


DEFAULT_POS = pos_words("v") * 4


def case(op, **kw):
    c = dict(op=op, mode=0, opts=8, dist=ONE, on=0x01010101, vol=[ONE] * 4, drop=0, audio=0, listener=0, nbanks=0,
             flags=[0, 0, 0], cached=[0, 0, 0], store=0, a=[0, 0, 0, 0], info0c=FILL, infoflags=FILL, control=FILL,
             pos=list(DEFAULT_POS), names=["bank0", "bank1", "bank2", "", ""], q={})
    c.update(kw)
    c["names"] = list(c["names"]) + [""] * (TEXT_SLOTS - len(c["names"]))
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def params(c):
    p = [0] * 64
    p[0], p[1], p[2], p[3] = c["mode"], c["opts"], c["dist"], c["on"]
    p[4:8] = c["vol"]
    p[8], p[9], p[10], p[12] = c["drop"], c["audio"], c["listener"], c["nbanks"]
    p[13:16] = c["flags"]
    p[16:19] = c["cached"]
    p[19] = c["store"]
    p[20:24] = c["a"]
    p[28], p[29], p[30] = c["info0c"], c["infoflags"], c["control"]
    p[32:44] = c["pos"]
    return [v & MASK for v in p]


# ---------------------------------------------------------------- the PowerPC side of a case

def comp_object(e, L, label):
    block = e.alloc(0x18, pack([VT["comp"], VT["comp_sub"], 1, 0, 0, 0]))
    L.labels[block] = label
    return block


def manager_ppc(e, c):
    op = c["op"]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    w = e.w = type("W", (), {})()
    e.write(S_SPEAKER, pack([0]))
    e.write(S_COUNT, pack([0]))
    samp = e.alloc(0x90, b"\xa5" * 0x90)
    e.write(samp + 0x0C, pack([c["info0c"]]))
    e.write(samp + 0x14, pack([c["infoflags"]]))
    L.labels[samp] = 5
    positions = []
    for i in range(4):
        block = e.alloc(12, pack(c["pos"][3 * i:3 * i + 3]))
        L.labels[block] = 11 + i
        positions.append(block)
    out_word = e.alloc(4, pack([FILL]))
    control = e.alloc(4, pack([c["control"]]))
    obj = e.alloc(0x70, b"\xa5" * 0x70)
    L.labels[obj] = 1
    if op == K_CTOR:
        e.write(S_COPY, pack([0]))
    else:
        T.call_ppc(e, A["ctor"], [obj])
        L.labels[e.u32(S_COPY)] = 8
        e.write(obj + 0x24, pack([c["opts"], c["dist"]]))
        e.write(obj + 0x48, bytes((c["on"] >> (8 * i)) & 0xFF for i in range(4)))
        e.write(obj + 0x4C, pack(c["vol"]))
        e.write(obj + 0x64, bytes([c["drop"] & 0xFF]))
        if c["listener"]:
            e.write(obj + 0x40, pack([positions[c["listener"] - 1]]))
        comps = {}
        if c["audio"] or op == K_SETAUDIO:
            comps = {1: comp_object(e, L, 10), 2: comp_object(e, L, 15)}
        if c["audio"]:
            e.write(obj + 0x44, pack([comps[c["audio"]]]))
        if c["store"]:
            store = e.alloc(0x10, b"\xa5" * 0x10)
            L.labels[store] = 3
            e.write(obj + 0x1C, pack([store]))
        set_string(e, obj + 0x2C, c["names"][4])
        if c["mode"] == 1:
            e.write(obj, pack([HOOK["vt"]]))
        w.comps = comps
    # the bank deque
    w.streamer = e.alloc(0x40, b"\xa5" * 0x40)
    L.labels[w.streamer] = 9
    count = c["nbanks"]
    array = e.alloc(max(4 * count, 4), b"\0" * max(4 * count, 4))
    e.write(w.streamer + 0x24, pack([array if count else 0, count]))
    containers = []
    for i in range(min(count, 3)):
        container = e.alloc(0x2C, b"\xa5" * 0x2C)
        L.labels[container] = 31 + i
        cached = 0
        if c["cached"][i]:
            cached = e.alloc(16, b"\xa5" * 16)
            L.labels[cached] = 41 + i
        e.write(container + 8, pack([cached]))
        set_string(e, container + 0x0C, c["names"][i])
        e.write(container + 0x20, pack([c["flags"][i]]))
        e.write(container + 0x24, pack([VT["storage"], 0]))
        L.labels[container + 0x24] = 21 + i
        e.write(array + 4 * i, pack([container]))
        containers.append(container)
    buf = e.alloc(4, pack([FILL]))
    a = c["a"]
    f = lambda bits: T.F(bits_to_float(bits))
    del L.log[:]
    L.track = True
    result = 0
    x = [0] * 8
    tail = True
    if op == K_CTOR:
        T.call_ppc(e, A["ctor"], [obj])
        copier = e.u32(S_COPY)
        ok = e.u32(obj) == MGR_VT and e.u32(obj + 4) == MGR_SUB_VT
        for slot, address in MGR_SLOTS.items():
            ok = ok and e.u32(e.u32(MGR_VT + slot)) == address
        ok = ok and e.read(obj + 0x65, 3) == b"\xa5\xa5\xa5" and e.read(obj + 0x68, 8) == b"\xa5" * 8
        ok = ok and e.u32(copier) == STD_COPY_VT
        x[0], x[1] = lbl(L, copier), int(ok)
    elif op == K_DTOR:
        T.call_ppc(e, A["dtor"], [obj, a[0] & MASK])
        x[0] = lbl(L, e.u32(S_COPY))
    elif op == K_INIT:
        text = e.alloc(len(c["names"][3]) + 1, c["names"][3].encode("latin-1") + b"\0")
        T.call_ppc(e, A["init"], [buf, obj, text])
        result = T.s32(e.u32(buf))
    elif op == K_GETPATH:
        result = int(T.call_ppc(e, A["getpath"], [obj])[0] == obj + 0x2C)
    elif op == K_SETLP:
        T.call_ppc(e, A["setlp"], [obj, positions[a[0] & 3]])
    elif op == K_GETLP:
        result = lbl(L, T.call_ppc(e, A["getlp"], [obj])[0])
    elif op == K_SETAUDIO:
        T.call_ppc(e, A["setaudio"], [obj, w.comps[a[0]] if a[0] else 0])
    elif op == K_SETDIST:
        T.call_ppc(e, A["setdist"], [obj, f(a[0])])
    elif op == K_GETDIST:
        T.call_ppc(e, A["getdist"], [obj])
        result = f1_bits(e)
    elif op == K_ISQ:
        result = T.call_ppc(e, A["isq"], [obj])[0] & MASK
    elif op == K_ROLLOFF:
        T.call_ppc(e, A["rolloff"], [obj, f(a[0]), f(a[1])])
        result = f1_bits(e)
    elif op == K_GETVC:
        result = int(T.call_ppc(e, A["getvc"], [obj])[0] == obj + 0x5C)
    elif op == K_GETVOL:
        if (a[0] & 0xFF) >= 4:
            # an index past the four volumes reads the member after them: a[1] is put there first (on both sides)
            e.write(obj + 0x4C + 4 * (a[0] & 0xFF), pack([a[1]]))
            tail = False
        T.call_ppc(e, A["getvol"], [obj, a[0]])
        result = f1_bits(e)
    elif op == K_VOLON:
        result = T.call_ppc(e, A["volon"], [obj, a[0]])[0] & MASK
    elif op == K_UPDVOL:
        T.call_ppc(e, A["updvol"], [buf, obj, a[0], f(a[1])])
        result = T.s32(e.u32(buf))
        if (a[0] & 0xFF) >= 4:
            tail = False
            x[0] = canon(e.u32(obj + 0x4C + 4 * (a[0] & 0xFF)))
    elif op == K_TOGGLE:
        T.call_ppc(e, A["toggle"], [buf, obj, a[0], a[1]])
        result = T.s32(e.u32(buf))
    elif op == K_SETQ:
        T.call_ppc(e, A["setq"], [buf, obj, a[0], a[1], a[2]])
        result = T.s32(e.u32(buf))
    elif op == K_ORIENT:
        T.call_ppc(e, A["orient"], [buf, obj] + positions)
        result = T.s32(e.u32(buf))
    elif op in (K_GETMAX, K_GETDIRECT):
        e.write(out_word, pack([a[0]]))
        T.call_ppc(e, A["getmax" if op == K_GETMAX else "getdirect"], [buf, obj, out_word])
        result = T.s32(e.u32(buf))
    elif op == K_GETBANKDATA:
        result = lbl(L, T.call_ppc(e, A["getbankdata"], [obj, a[0]])[0])
    elif op == K_ISDROP:
        result = T.call_ppc(e, A["isdrop"], [obj, a[0]])[0] & MASK
    elif op == K_SETDROP:
        T.call_ppc(e, A["setdrop"], [obj, 1 if a[0] else 0])
    elif op == K_GETBANK:
        result = lbl(L, T.call_ppc(e, A["getbank"], [obj, a[0]])[0])
    elif op == K_CONSTRUCT:
        result = T.call_ppc(e, A["construct"], [obj, a[0], control, samp])[0] & MASK
    elif op == K_PLAY:
        result = T.call_ppc(e, A["play"], [obj, samp, control])[0] & MASK
    L.track = False
    state = [0] * 12
    if op != K_DTOR:
        state = [e.u32(obj + 0x24), canon(e.u32(obj + 0x28)), lbl(L, e.u32(obj + 0x40)), lbl(L, e.u32(obj + 0x44)),
                 int.from_bytes(e.read(obj + 0x48, 4), "little")]
        state += [canon(e.u32(obj + 0x4C + 4 * i)) for i in range(4)]
        if tail:
            state += [int(e.u32(obj + 0x5C) == obj + 0x4C), e.u32(obj + 0x60), e.read(obj + 0x64, 1)[0]]
        else:
            state += [0, 0, 0]
    banks = [lbl(L, e.u32(containers[i] + 8)) if i < len(containers) else 0 for i in range(3)]
    return dict(r=result & MASK, st=tuple(state), x=tuple(x), info=(e.u32(samp + 0x0C), e.u32(samp + 0x14)),
                ctl=e.u32(control), banks=tuple(banks), out=e.u32(out_word),
                path=string(e, obj + 0x2C) if op not in (K_CTOR, K_DTOR) else "", log=list(L.log))


def manager_host(lib, c):
    p = (C.c_uint32 * 64)(*params(c))
    names = bytearray(64 * TEXT_SLOTS)
    for i, text in enumerate(c["names"]):
        raw = text.encode("latin-1")
        names[64 * i:64 * i + len(raw)] = raw
    name_buffer = (C.c_char * len(names)).from_buffer(names)
    out = (C.c_uint32 * 40)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_sm_run(c["op"], p, name_buffer, out)
    log = CL.host_end(lib)
    op = c["op"]
    result = out[0]
    if op in HIDDEN:
        result &= MASK
    return dict(r=result, st=tuple(out[1:13]), x=tuple(out[14:22]), info=(out[22], out[23]), ctl=out[24],
                banks=tuple(out[25:28]), out=out[28],
                path=lib.tpw_sm_path().decode("latin-1") if op not in (K_CTOR, K_DTOR) else "", log=log)


# ---------------------------------------------------------------- the cases

def rbits(r):
    """A random float: a normal number, an edge value or any bit pattern."""
    k = r.random()
    if k < 0.4:
        return T.f32_bits(T.f32(r.uniform(-1000.0, 1000.0)))
    if k < 0.7:
        return r.choice([0, 0x80000000, ONE, 0xBF800000, 0x3F000000, 0x00000001, 0x007FFFFF, 0x00800000, 0x7F7FFFFF,
                         0xFF7FFFFF, 0x7F800000, 0xFF800000, 0x7FC00000, 0x3C800000, 0x42800000])
    return r.getrandbits(32)


FLOAT_EDGES = [0, 0x80000000, ONE, 0xBF800000, 0x3F000000, 0x40000000, 0x3C800000, 0x42800000, 0x00000001, 0x007FFFFF,
               0x00800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x7F800000, 0xFF800000, 0x7FC00000, 0x41200000, 0xC1200000]


def base_random(r, op):
    """The presets every case takes: options, distance, control bytes, volumes, droppable byte, listener, audio, path."""
    return dict(
        op=op,
        opts=r.choice([0, 8, 0x100, 0x108, 0xFFFFFFF7, 0xFFFFFFFF, 7, 0x1F8, r.getrandbits(32)]),
        dist=rbits(r),
        on=r.choice([0x01010101, 0, 0x00010001, 0x01000100, 0x02020202, 0xFFFFFFFF, 0x00000100, r.getrandbits(32)]),
        vol=[rbits(r) for _ in range(4)], drop=r.choice([0, 1, 2]), audio=r.choice([0, 1, 2]), listener=r.choice([0, 1, 2]),
        names=[r.choice(["bank0", "", "abc", "x" * 40]) for _ in range(3)] + [r.choice(["", "ROOT:", "Sound:Banks:"]),
                                                                                  r.choice(["", "Old:", "p" * 30])],
        info0c=r.choice([0, 2, 0x2000, 0x2002, FILL, r.getrandbits(32)]),
        infoflags=r.choice([0, 0x2000, 0xFFFFDFFF, 0xFFFFFFFF, 0x1FFF, 0x4000, r.getrandbits(32)]),
        control=r.choice([0, 0x1234, FILL, 0xFFFFFFFF, r.getrandbits(32)]),
    )


def random_queue_errors(r, *keys):
    return {key: [r.choice(QUEUE_ERRORS)] for key in keys}


def make_random(op):
    def rnd(r):
        c = case(op)
        c.update(base_random(r, op))
        c["a"] = [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]
        c["q"] = {k: [v] if not isinstance(v, list) else v for k, v in random_queue(r, op).items()}
        specialise(r, c, op)
        return c
    return rnd


def random_queue(r, op):
    q = random_queue_errors(r, "Exec.SetSystemInfo", "Exec.UpdateAllVolumes", "Exec.Start", "Hook.err", "Storage.Open",
                            "Storage.FullPath")
    q["Hook.mask"] = [r.choice([0, 0x80, 1, 0x81, 0xFFFFFFFF, 0x7F, 0x100, r.getrandbits(32)])]
    q["Hook.max"] = [r.getrandbits(32)]
    q["Hook.f0"] = [r.getrandbits(32)]
    q["Store.Max"] = [r.getrandbits(32)]
    q["Fake.NewControl"] = [r.choice([0, 0x1111, r.getrandbits(32)])]
    q["Fake.NewSample"] = [r.choice([0, 0x2222, 0xFFFFFFFF, r.getrandbits(32)])]
    q["GetBankPath"] = [r.choice([0, 1, 1, 1])]
    q["GenBank"] = [r.choice([0, 1, 1])]
    return q


def specialise(r, c, op):
    """Per-operation ranges for the argument words and the world."""
    a = c["a"]
    if op == K_DTOR:
        a[0] = r.choice(DTOR_FLAGS) & MASK
    elif op in (K_SETLP,):
        a[0] = r.randrange(4)
    elif op == K_SETAUDIO:
        a[0] = r.randrange(3)
    elif op in (K_SETDIST, K_GETDIST):
        a[0] = rbits(r)
    elif op == K_ROLLOFF:
        a[0], a[1] = rbits(r), rbits(r)
    elif op in (K_GETVOL, K_VOLON, K_TOGGLE):
        a[0] = r.choice([r.randrange(4), r.randrange(256), r.getrandbits(32)]) if op != K_GETVOL else r.randrange(4)
        a[1] = r.choice([0, 1, 2, 0x100, 0xFFFFFFFF, r.getrandbits(32)])
    elif op == K_UPDVOL:
        a[0] = r.randrange(8)
        a[1] = rbits(r)
    elif op == K_SETQ:
        a[:3] = [r.choice([0, 1, 0x100, r.getrandbits(32)]) for _ in range(3)]
    elif op == K_ORIENT:
        kinds = ["v", "v", "v", (0, BAD_WORDS[0]), (1, BAD_WORDS[1]), (2, BAD_WORDS[2])]
        c["pos"] = sum([pos_words(r.choice(kinds), r) for _ in range(4)], [])
    elif op in (K_GETMAX, K_GETDIRECT):
        c["mode"] = r.randrange(2)
        c["store"] = r.randrange(2)
        a[0] = r.getrandbits(32)
    elif op in (K_GETBANKDATA, K_ISDROP, K_GETBANK):
        c["nbanks"] = r.randrange(4)
        c["flags"] = [r.choice([0, 1, 2, 4, 3, 5, 6, 7, 0x40, 0x41, 0x44, 0x22, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(3)]
        c["cached"] = [r.choice([0, 0, 1]) for _ in range(3)]
        a[0] = r.choice([0, 1, 2, 3, 4, 0xFFFFFFFF, r.randrange(5)])
    elif op == K_SETDROP:
        a[0] = r.randrange(2)
    elif op in (K_CONSTRUCT, K_PLAY):
        c["audio"] = r.choice([1, 2])
    if op == K_PLAY or op == K_CONSTRUCT:
        a[0] = r.choice([0, 5, 0xFFFFFFFF, r.getrandbits(32)])


def edge_cases(op):
    def edges():
        cs = []
        if op == K_CTOR:
            cs.append(case(op))
        if op == K_DTOR:
            for flag in DTOR_FLAGS:
                for audio in (0, 1):
                    cs.append(case(op, a=[flag & MASK, 0, 0, 0], audio=audio, listener=audio + 1, opts=0x108))
        if op == K_INIT:
            for text in ("", "a", "ROOT:Sound:", "q" * 60, "Sound Banks:"):
                for start in ("", "Old:", "z" * 40):
                    cs.append(case(op, names=["b", "b", "b", text, start]))
        if op == K_GETPATH:
            cs.append(case(op))
            cs.append(case(op, names=["b", "b", "b", "", "ROOT:"]))
        if op == K_SETLP:
            for i in range(4):
                for listener in (0, 1, 2):
                    cs.append(case(op, a=[i, 0, 0, 0], listener=listener))
        if op == K_GETLP:
            for listener in (0, 1, 2, 3, 4):
                cs.append(case(op, listener=listener))
        if op == K_SETAUDIO:
            for i in range(3):
                for audio in (0, 1, 2):
                    cs.append(case(op, a=[i, 0, 0, 0], audio=audio))
        if op in (K_SETDIST, K_GETDIST):
            for bits in FLOAT_EDGES:
                cs.append(case(op, a=[bits, 0, 0, 0], dist=bits ^ 0x3F800000 if bits != ONE else 0x40000000))
        if op == K_ISQ:
            for opts in (0, 8, 0x100, 0x108, 7, 0x10, 0xFFFFFFF7, 0xFFFFFFFF, 0x80000008, 0x1000, 0x18, 0x28):
                cs.append(case(op, opts=opts))
        if op == K_ROLLOFF:
            for lo in FLOAT_EDGES:
                for hi in (0, ONE, 0x41200000, 0x42C80000, 0xC1200000, 0x7F800000, 0x7FC00000, lo):
                    cs.append(case(op, a=[lo, hi, 0, 0]))
        if op == K_GETVC:
            cs.append(case(op))
        if op == K_GETVOL:
            for i in range(4):
                for bits in (0, ONE, 0x3F000000, 0x7FC00000, 0xBF800000):
                    vol = [0x11111111, 0x22222222, 0x33333333, 0x44444444]
                    vol[i] = bits
                    cs.append(case(op, a=[i, 0, 0, 0], vol=vol))
            cs.append(case(op, a=[0x103, 0, 0, 0]))
            cs.append(case(op, a=[0xFFFFFF02, 0, 0, 0]))
            for i in range(4, 8):
                for bits in (0, ONE, 0x7FC00000, 0xBF800000, 0x12345678):
                    cs.append(case(op, a=[i, bits, 0, 0]))
            cs.append(case(op, a=[0x105, 0x40000000, 0, 0]))
        if op == K_VOLON:
            for pattern in range(16):
                on = sum(((pattern >> i) & 1) << (8 * i) for i in range(4))
                for mask in list(range(0, 18)) + [0x1F, 0x20, 0x55, 0xAA, 0xF0, 0xFF, 0x80, 0x100 & 0xFF]:
                    cs.append(case(op, a=[mask, 0, 0, 0], on=on))
            for on in (0x02020202, 0xFF00FF00, 0x00FF00FF, 0x80808080, 0x01010100, 0x00000100):
                for mask in (0, 1, 2, 4, 8, 0xF, 0xFF):
                    cs.append(case(op, a=[mask, 0, 0, 0], on=on))
            cs.append(case(op, a=[0x1F3, 0, 0, 0]))
        if op == K_UPDVOL:
            for i in range(8):
                for bits in (0, ONE, 0x7FC00000, 0xBF800000, 0x12345678):
                    for err in (0, -1):
                        cs.append(case(op, a=[i, bits, 0, 0], q={"Exec.UpdateAllVolumes": [err]}))
            cs.append(case(op, a=[0x101, ONE, 0, 0]))
            for err in QUEUE_ERRORS:
                cs.append(case(op, a=[2, 0x40000000, 0, 0], q={"Exec.UpdateAllVolumes": [err]}, vol=[1, 2, 3, 4]))
        if op == K_TOGGLE:
            for i in list(range(7)) + [0xFF, 0x100, 0x104, 0x1FF]:
                for on in (0, 1, 2, 0x100, 0xFFFFFFFF, 0x1234):
                    cs.append(case(op, a=[i, on, 0, 0], on=0x01020304))
        if op == K_SETQ:
            for combo in range(8):
                a = [combo & 1, (combo >> 1) & 1, (combo >> 2) & 1, 0]
                for opts in (0, 8, 0x100, 0x108, 0x1F7):
                    for err in (0, -1):
                        cs.append(case(op, a=a, opts=opts, q={"Exec.SetSystemInfo": [err]}))
            for value in (2, 0x100, 0xFFFFFFFF, 0x80000000):
                cs.append(case(op, a=[value, value, value, 0], opts=0x100))
        if op == K_ORIENT:
            kinds = [None] + [(i, bad) for i in range(3) for bad in BAD_WORDS]
            valid = pos_words("v")
            for k1 in kinds:
                for k2 in kinds[:4]:
                    pos = list(valid)
                    pos += pos_words(k1) if k1 else valid
                    pos += pos_words(k2) if k2 else valid
                    pos += valid
                    cs.append(case(op, pos=pos, listener=2))
            cs.append(case(op, pos=valid * 3 + pos_words((1, BAD_WORDS[0]))))
            cs.append(case(op, pos=pos_words((0, BAD_WORDS[1])) + valid * 3))
            for err in QUEUE_ERRORS:
                cs.append(case(op, q={"Exec.SetSystemInfo": [err]}))
        if op in (K_GETMAX, K_GETDIRECT):
            for mode in (0, 1):
                for store in (0, 1):
                    for err in (0, -1, 5, -0x80000000):
                        for mask in (0, 0x80, 1, 0x81, 0xFFFFFFFF, 0x7E, 0x100):
                            cs.append(case(op, mode=mode, store=store, a=[0xCAFE, 0, 0, 0], q={
                                "Hook.err": [err], "Hook.mask": [mask], "Hook.max": [0x1357], "Hook.f0": [0x2468],
                                "Store.Max": [0xBEEF]}))
        if op in (K_GETBANKDATA, K_ISDROP):
            for nbanks in range(4):
                for index in (0, 1, 2, 3, 4, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                    for flags in ([0, 0, 0], [0x40, 0x40, 0x40], [0x3F, 0x7F, 0xFFFFFFBF], [0xFFFFFFFF, 0x41, 0x80]):
                        cs.append(case(op, nbanks=nbanks, a=[index, 0, 0, 0], flags=flags))
        if op == K_SETDROP:
            for drop in (0, 1, 2, 0xFF):
                for on in (0, 1):
                    cs.append(case(op, drop=drop, a=[on, 0, 0, 0]))
        if op == K_GETBANK:
            for nbanks in (0, 1, 3):
                for index in (0, 1, 2, 3, 0xFFFFFFFF):
                    cs.append(case(op, nbanks=nbanks, a=[index, 0, 0, 0], flags=[1, 2, 4]))
            names = ["bank0", "", "long" * 12]
            for flags in (0, 1, 2, 4, 3, 5, 6, 7, 8, 0x40, 0x41, 0x42, 0x44, 0x20, 0xFFFFFFFF, 0xFFFFFFF8):
                for cached in (0, 1):
                    for opened in (0, -1, 1):
                        for path_ok in (0, 1):
                            for generated in (0, 1):
                                cs.append(case(op, nbanks=1, a=[0, 0, 0, 0], flags=[flags, 0, 0], cached=[cached, 0, 0],
                                               names=names, q={"Storage.Open": [opened], "GetBankPath": [path_ok],
                                                               "GenBank": [generated]}))
            for name in names:
                cs.append(case(op, nbanks=3, a=[2, 0, 0, 0], flags=[1, 2, 4], names=[name, name, name, "", "Sound:"]))
            cs.append(case(op, nbanks=2, a=[1, 0, 0, 0], flags=[4, 2, 0], cached=[1, 0, 0], q={"Storage.FullPath": [-1]}))
        if op == K_CONSTRUCT:
            for audio in (1, 2):
                for flags in (0, 0x2000, 0x1FFF, 0xFFFFDFFF, 0xFFFFFFFF, 0x4000):
                    for control in (0, 0x1234):
                        for handle in (0, 5, 0xFFFFFFFF):
                            for new_control in (0, 0xC0DE):
                                for new_sample in (0, 0x5A5A, 0xFFFFFFFF):
                                    cs.append(case(op, audio=audio, infoflags=flags, control=control, a=[handle, 0, 0, 0],
                                                   info0c=0x7, q={"Fake.NewControl": [new_control],
                                                                  "Fake.NewSample": [new_sample]}))
        if op == K_PLAY:
            for opts in (0, 8, 0x100, 0x108, 0x10, 0x118, 0xFFFFFFFF):
                for flags in (0, 0x2000):
                    for control in (0, 0x1234):
                        for new_control in (0, 0xC0DE):
                            for new_sample in (0, 0x5A5A):
                                for start in (0, -1, 3):
                                    cs.append(case(op, audio=1, opts=opts, infoflags=flags, control=control, info0c=0x10,
                                                   q={"Fake.NewControl": [new_control], "Fake.NewSample": [new_sample],
                                                      "Exec.Start": [start]}))
            for info0c in (0, 2, 0x2000, 0x2002, 0xFFFFFFFF):
                cs.append(case(op, audio=2, opts=0x108, info0c=info0c, infoflags=0x2000,
                               q={"Fake.NewSample": [0x1], "Fake.NewControl": [0x2]}))
        return cs
    return edges


# ---------------------------------------------------------------- the tests

def manager_tests():
    table = [
        (K_CTOR, "TbSoundManager::TbSoundManager 0x10026e30"),
        (K_DTOR, "TbSoundManager::~TbSoundManager 0x10027f20"),
        (K_INIT, "TbSoundManager::Init 0x10026fa4"),
        (K_GETPATH, "TbSoundManager::GetSoundPath 0x10016a48"),
        (K_SETLP, "TbSoundManager::SetListenerPosition 0x1001670c"),
        (K_GETLP, "TbSoundManager::GetListenerPosition 0x10017cf4"),
        (K_SETAUDIO, "TbSoundManager::SetAudioSystem 0x100167b4"),
        (K_SETDIST, "TbSoundManager::SetDistanceMapping 0x10027274"),
        (K_GETDIST, "TbSoundManager::GetDistanceMultiplier 0x1001966c"),
        (K_ISQ, "TbSoundManager::IsQSoundOn 0x1001954c"),
        (K_ROLLOFF, "TbSoundManager::GetRolloffFactorToMinMaxDistance 0x1001960c"),
        (K_GETVC, "TbSoundManager::GetVolumeControl 0x10019758"),
        (K_GETVOL, "TbSoundManager::GetVolume 0x1002a080"),
        (K_VOLON, "TbSoundManager::VolumeControlIsOn 0x10016e60"),
        (K_UPDVOL, "TbSoundManager::UpdateVolumeControl 0x100279c4"),
        (K_TOGGLE, "TbSoundManager::ToggleVolumeControl 0x10027adc"),
        (K_SETQ, "TbSoundManager::SetQSoundInfo 0x10027160"),
        (K_ORIENT, "TbSoundManager::SetListenerOrientation 0x10027b60"),
        (K_GETMAX, "TbSoundManager::GetMaxNumberOfChannels 0x10027020"),
        (K_GETDIRECT, "TbSoundManager::GetDirectSoundObject 0x100278c0"),
        (K_GETBANKDATA, "TbSoundManager::GetBankData 0x10027334"),
        (K_ISDROP, "TbSoundManager::IsBankDroppable 0x100272b8"),
        (K_SETDROP, "TbSoundManager::SetDroppable 0x10027408"),
        (K_GETBANK, "TbSoundManager::GetBank 0x1002748c"),
        (K_CONSTRUCT, "TbSoundManager::ConstructSample 0x10027c90"),
        (K_PLAY, "TbSoundManager::PlaySample 0x10027d94"),
    ]
    return [FnTest(name, manager_ppc, manager_host, edge=edge_cases(op), rnd=make_random(op)) for op, name in table]


TESTS = manager_tests()
