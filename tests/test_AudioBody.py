"""Unit w79f, the audio object the game holds, against libtpw_unit_AudioBody.dylib (tests/unit_AudioBody.cpp), in the program
"sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/AudioComponent.cpp, src/sound/TbAudioBody.cpp, src/sound/TbSysCommand.cpp  TbAudioBody: constructor 0x100161a0, destructor 0x100163bc, CreateSystemChain 0x10016480, Init 0x1001653c,
                       Release 0x10016804, DeregisterCatagoryBanks 0x10016884, QualifyPath 0x10016928, RegisterCatagory
                       0x10016ae4, SendEvent 0x10016c1c, CanBeDroppedDueToVolControl 0x10016f68, UpdateHandle 0x10017314,
                       UpdateParameter 0x100173d4, StopHandle 0x10017490, SystemCommand 0x1001753c, SetListenerOrientation
                       0x10017610, UpdateDistortion 0x1001781c, UpdateLowPass 0x1001785c, GetSoundManager 0x10028110,
                       GetPlaceHolderList 0x1002814c; TbAudioSystem constructor 0x10016384; CAutoPtr (CAudioPlaceHolderList):
                       constructor 0x10016324, destructor 0x10016270, operator-> 0x10016e18, operator CAudioPlaceHolderList*
                       0x100172b4; _IsSlash 0x10016a84; CAudioComponentFactory: CreateCore 0x10001b54, CreateComponent
                       0x10001bd8; CreateAudioObject 0x1002a18c

Each function runs as the original in the emulator and as the host's translation on a world built from the case: a body made by
the constructor, then the case's presets (the manager, the component, the list, the four positions, the reference counts, a chain
between the component and the manager) and, for the streamer functions, a catagory deque of up to three containers over 0xA5. The
manager (w79e) and the core under it (w79b) are fakes in this unit, so that every branch of the body is reachable: every member
the body calls records its arguments and answers from a queue (the PowerPC side stubs the manager's constructor and members at
their addresses and gives the object a table whose slots record). The component chain is real (CAudioComponent, w79b): Chain and
Init run as the original, the chain's tail being the fake manager. The placeholder list and the placeholders (w79g1, w79g2), the
streamer (TbMapStreamer::Instance, SetSoundManager, RegisterCatagory, RemoveCatagory) and TbEventMapper::MapEvent are fakes that
record the same event on both sides. Mode 1 of the Init cases replaces CreateSystemChain (slot +0x3c of the body's table) by a
recording function that installs nothing, with a hooked component (its Init, slot +0x38, records and answers a queue).

TbError is returned through a hidden r3 buffer (this in r4) and read from the buffer. UpdateDistortion and UpdateLowPass answer the
handle in r3 on the Mac (no hidden result). Pointers compare as labels (0 NULL, the label of the object, -1 for anything else);
position words compare as the words (no float arithmetic touches them).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_AudioBody"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

BODY_VT, SYS_VT = 0x1003CB70, 0x1003CBB0
COMP_VT, COMP_SUB_VT = 0x10035C24, 0x10035C48
BODY_SLOTS = {0x08: 0x10016804, 0x0C: 0x10017610, 0x10: 0x10016AE4, 0x14: 0x10016C1C, 0x18: 0x10017314, 0x1C: 0x100173D4,
              0x20: 0x1001753C, 0x24: 0x1001653C, 0x28: 0x1001781C, 0x2C: 0x1001785C, 0x30: 0x100163BC, 0x34: 0x10017490,
              0x38: 0x10016884, 0x3C: 0x10016480}

A = dict(
    sysctor=0x10016384, ctor=0x100161A0, dtor=0x100163BC, capctor=0x10016324, capdtor=0x10016270, caparrow=0x10016E18,
    capconv=0x100172B4, chain=0x10016480, init=0x1001653C, release=0x10016804, dereg=0x10016884, qualify=0x10016928,
    register=0x10016AE4, send=0x10016C1C, canbe=0x10016F68, updhandle=0x10017314, updparam=0x100173D4, stop=0x10017490,
    syscmd=0x1001753C, orient=0x10017610, upddist=0x1001781C, updlow=0x1001785C, getsm=0x10028110, getph=0x1002814C,
    core=0x10001B54, comp=0x10001BD8, create=0x1002A18C, isslash=0x10016A84,
)
# the callees outside the unit
INSTANCE, STREAMER_SETMGR, STREAMER_REGISTER, STREAMER_REMOVE, MAPEVENT = 0x1001F6EC, 0x10016760, 0x1002071C, 0x10022E78, 0x10026D24
LIST_CTOR, LIST_FIND, LIST_CREATE, LIST_UPDATE, LIST_UPDPARAM = 0x1001BB40, 0x1001C8D8, 0x1001C678, 0x1001C4C0, 0x1001C5B4
HOLDER_SETMGR = 0x100166B4
MGR_CTOR, MGR_GETPATH, MGR_SETLISTENER, MGR_SETAUDIO, MGR_ORIENT, MGR_VOLON = (0x10026E30, 0x10016A48, 0x1001670C, 0x100167B4,
                                                                            0x10027B60, 0x10016E60)
CORE_CTOR, COMP_CTOR, COMP_CHAIN = 0x1000B618, 0x10001364, 0x100015E4

# the host's operation numbers (enum Op in the unit file)
(K_SYSCTOR, K_CTOR, K_DTOR, K_CAPCTOR, K_CAPDTOR, K_CAPARROW, K_CAPCONV, K_CHAIN, K_INIT, K_RELEASE, K_DEREG, K_QUALIFY,
 K_REGISTER, K_SEND, K_CANBE, K_UPDHANDLE, K_UPDPARAM, K_STOP, K_SYSCMD, K_ORIENT, K_UPDDIST, K_UPDLOW, K_GETSM, K_GETPH,
 K_CORE, K_COMP, K_CREATE, K_ISSLASH) = range(28)
NO_BODY = {K_SYSCTOR, K_CTOR, K_CAPCTOR, K_CAPDTOR, K_CAPARROW, K_CAPCONV, K_CORE, K_COMP, K_CREATE, K_ISSLASH}
QUEUE_ERRORS = [0, -1, 1, 5, -2, 0x7FFFFFFF, -0x80000000]
GOOD_ERRORS = [0, 1, 5, 0x7FFFFFFF]
DTOR_FLAGS = [0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, -0x8000]

# the out vector of the unit (enum Out)
O_RESULT, O_BODYPOS = 0, 1
O_MGR, O_COMP, O_LIST, O_AUDIO, O_LISTENER, O_MREF, O_CREF, O_HFLAGS, O_NCATS, O_HANDLE, O_NEXT, O_PREV, O_OUTPTR, O_X = range(13, 27)
VT = {}


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def lbl(L, address):
    return L.lab(address) & MASK


def words_at(e, address, count):
    return list(struct.unpack(">%dI" % count, e.read(address, 4 * count)))


def bits_to_float(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def string(e, obj):
    ptr, count = e.u32(obj + 8), e.u32(obj + 4)
    return e.read(ptr, count).decode("latin-1") if ptr else ""


def set_string(e, obj, value):
    raw = value.encode("latin-1")
    ptr = e.alloc(len(raw) + 4, raw + b"\0\0\0\0")
    e.write(obj, pack([e.string_vt, len(raw), ptr, len(raw), 0]))
    e.set_r(3, obj)


def make_vtable(e, slots, size=0x50):
    table = T.persist_alloc(e, size)
    for slot, callback in slots.items():
        e.write(table + slot, pack([T.fake_function(e, callback)]))
    return table


def delete_block(e, block):
    L = e.cl
    if block != 0 and L.lab(block) > 0:
        L.rec("delete", (L.lab(block),))
        L.forget(block)


def stub_new(e):
    L, size = e.cl, e.r(3)
    block = e.alloc(max(size, 16), b"\xa5" * max(size, 16))
    if L.track:
        L.rec("new", (L.newlab(block),))
    e.set_r(3, block)


def stub_delete(e):
    delete_block(e, e.r(3))


def stub_new_array(e):
    size = e.r(3)
    block = e.alloc(max(size, 16), b"\0" * max(size, 16))
    e.news_arr.append((size, block))
    e.set_r(3, block)


def rec_info(L, e, name, first, address):
    """The words of a system info the body defines (the host's RecInfo): the three of the constructor, the quality double's two
    halves, the mask, the buffers to use, the window handle and +0x40."""
    w = words_at(e, address, 19)
    L.rec(name, [first & MASK, w[0], w[1], w[2], w[3], w[4], w[6], w[7], w[8], w[16]])


def rec_positions(L, e, name, first, a, b):
    L.rec(name, [first & MASK] + words_at(e, a, 3) + words_at(e, b, 3))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    assert lib.tpw_ab_sizes(0) >= 0x40 and lib.tpw_ab_sizes(1) >= 0x68, "the host's objects are at least the Mac's"
    lib.tpw_ab_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_ab_text.restype = C.c_char_p

    # ---- the functional strings (unrecorded), as in w70's and w79e's modules
    def assign_text(e):
        set_string(e, e.r(3), e.cstr(e.r(4)) if e.r(4) else "")

    def assign_base(e):
        set_string(e, e.r(3), string(e, e.r(4)))

    def append_text(e):
        set_string(e, e.r(3), string(e, e.r(3)) + e.cstr(e.r(4)))

    e.string_vt = make_vtable(e, {0x18: assign_base, 0x1c: assign_base, 0x20: assign_text, 0x30: append_text})
    e.stub("__ct__26TbDynamicStringTemplate<c>Fv", lambda e: set_string(e, e.r(3), ""))
    e.stub("__dt__26TbDynamicStringTemplate<c>Fv", lambda e: None)
    e.stub("__nw__FUl", stub_new)
    e.stub("__dl__FPv", stub_delete)
    e.stub("__nwa__FUl", stub_new_array)

    def memcmp(e):
        a, b, n = e.r(3), e.r(4), e.r(5)
        x, y = e.read(a, n), e.read(b, n)
        e.set_r(3, 0 if x == y else (T.u32(-1) if x < y else 1))

    e.stub("memcmp", memcmp)

    # ---- the TbRefRelease imports of the bullfrog library (src/bullfrog/TbRefRelease.cpp): the count is the word after the vptr
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

    # ---- the streamer and the mapper
    def instance(e):
        e.set_r(3, e.w.streamer)

    def set_manager(e):
        L = e.cl
        L.rec("Streamer.SetSoundManager", (L.lab(e.r(3)), L.lab(e.r(4))))

    def register(e):
        # r3 = the buffer, r4 = the streamer, r5 = the qualified path, r6 = the name, r7 = the handle
        L = e.cl
        handle = e.r(7)
        L.rec("Streamer.Register", (L.lab(e.r(4)), e.u32(handle)),
              (e.cstr(e.r(5)) if e.r(5) else "", e.cstr(e.r(6)) if e.r(6) else ""))
        e.write(handle, pack([L.ret("Streamer.handle", 0)]))
        e.write(e.r(3), pack([L.ret("Streamer.Register", 0)]))

    def remove(e):
        L = e.cl
        L.rec("Streamer.Remove", (L.lab(e.r(4)), e.r(5) & MASK))
        e.write(e.r(3), pack([L.ret("Streamer.Remove", 0)]))

    def map_event(e):
        L, data, event = e.cl, e.r(4), e.r(5)
        L.rec("MapEvent", (L.lab(data), L.lab(event), e.u32(event + 4), e.u32(event + 8)))
        e.set_r(3, e.w.sound if L.ret("Map", 0) != 0 else 0)

    # ---- the placeholder list and the placeholders
    def list_ctor(e):
        L, this = e.cl, e.r(3)
        L.rec("List.ctor", (L.lab(this), e.r(4) & MASK))
        e.write(this, pack([VT["list"], FILL]))
        e.set_r(3, this)

    def list_find(e):
        L = e.cl
        L.rec("List.Find", (L.lab(e.r(3)), e.r(4) & MASK))
        e.set_r(3, e.w.holder if L.ret("List.Find", 0) != 0 else 0)

    def list_create(e):
        L = e.cl
        L.rec("List.Create", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6))))
        e.set_r(3, T.u32(L.ret("List.Create", 0)))

    def list_update(e):
        L = e.cl
        rec_positions(L, e, "List.Update", e.r(4), e.r(5), e.r(6))
        e.set_r(3, T.u32(L.ret("List.Update", 0)))

    def list_updparam(e):
        L = e.cl
        L.rec("List.UpdateParameter", (L.lab(e.r(3)), e.r(4) & MASK, e.r(5) & 0xFF, e.r(6) & MASK))
        e.set_r(3, T.u32(L.ret("List.UpdateParameter", 0)))

    def holder_setmgr(e):
        L = e.cl
        L.rec("Holder.SetSoundManager", (L.lab(e.r(3)),))

    # ---- the fake manager and core (w79e, w79b): the constructor and the members the body calls
    def mgr_ctor(e):
        L, this = e.cl, e.r(3)
        e.write(this, pack([VT["mgr"], VT["mgr"] + 0x24, 1, 0, 0, 0]))
        e.write(this + 0x18, pack([0, 0]))
        e.write(this + 0x40, pack([0, 0]))
        L.rec("Manager.ctor", (L.lab(this),))
        set_string(e, this + 0x2C, "")
        e.set_r(3, this)

    def mgr_getpath(e):
        L, this = e.cl, e.r(3)
        L.rec("Manager.GetSoundPath", (L.lab(this),))
        e.set_r(3, this + 0x2C)

    def mgr_setlistener(e):
        L, this, position = e.cl, e.r(3), e.r(4)
        L.rec("Manager.SetListenerPosition", (L.lab(this), L.lab(position)))
        e.write(this + 0x40, pack([position]))

    def mgr_setaudio(e):
        L, this, component = e.cl, e.r(3), e.r(4)
        L.rec("Manager.SetAudioSystem", (L.lab(this), L.lab(component)))
        e.write(this + 0x44, pack([component]))

    def mgr_volon(e):
        L = e.cl
        L.rec("Manager.VolumeControlIsOn", (L.lab(e.r(3)), e.r(4) & 0xFF))
        e.set_r(3, 1 if L.ret("Manager.VolumeOn", 0) != 0 else 0)

    def mgr_orient(e):
        # r3 = the buffer, r4 = the manager, r5..r8 = position, front, top, velocity
        L = e.cl
        rec_positions(L, e, "Manager.SetListenerOrientation.a", lbl(L, e.r(4)), e.r(5), e.r(6))
        rec_positions(L, e, "Manager.SetListenerOrientation.b", lbl(L, e.r(4)), e.r(7), e.r(8))
        L.rec("Manager.SetListenerOrientation", (L.lab(e.r(5)), L.lab(e.r(6)), L.lab(e.r(7)), L.lab(e.r(8))))
        e.write(e.r(3), pack([L.ret("Manager.Orient", 0)]))

    def core_ctor(e):
        this = e.r(3)
        e.write(this, pack([e.u32(0x1003C120), 0x1003C144, 1, 0, 0, 0, 0, 0, 0]))
        e.set_r(3, this)

    for address, fn in {
        INSTANCE: instance, STREAMER_SETMGR: set_manager, STREAMER_REGISTER: register, STREAMER_REMOVE: remove,
        MAPEVENT: map_event, LIST_CTOR: list_ctor, LIST_FIND: list_find, LIST_CREATE: list_create, LIST_UPDATE: list_update,
        LIST_UPDPARAM: list_updparam, HOLDER_SETMGR: holder_setmgr, MGR_CTOR: mgr_ctor, MGR_GETPATH: mgr_getpath,
        MGR_SETLISTENER: mgr_setlistener, MGR_SETAUDIO: mgr_setaudio, MGR_VOLON: mgr_volon, MGR_ORIENT: mgr_orient,
        CORE_CTOR: core_ctor,
    }.items():
        T.stub_internal(e, address, fn)

    # ---- the tables
    fake = lambda fn: T.fake_function(e, fn)

    def list_dtor(e):
        L, this = e.cl, e.r(3)
        L.rec("List.~", (L.lab(this),))
        if T.s16(e.r(4)) > 0:
            delete_block(e, this)

    VT["list"] = T.persist_alloc(e, 16, pack([0, 0, fake(list_dtor), 0]))

    def holder_stop(e):
        L = e.cl
        L.rec("Holder.Stop", (L.lab(e.r(4)),))
        e.write(e.r(3), pack([L.ret("Holder.Stop", 0)]))

    VT["holder"] = T.persist_alloc(e, 0x30, pack([0] * 8 + [fake(holder_stop)] + [0] * 3))

    def cmd_exec(e):
        L = e.cl
        L.rec("Cmd.Execute", (L.lab(e.r(4)), L.lab(e.r(5))))
        e.write(e.r(3), pack([L.ret("Cmd.Execute", 0)]))

    VT["cmd"] = T.persist_alloc(e, 16, pack([0, 0, fake(cmd_exec), 0]))

    def event_category(e):
        L, this = e.cl, e.r(3)
        L.rec("Event.GetCategory", (L.lab(this),))
        e.set_r(3, e.u32(this + 4))

    VT["event"] = T.persist_alloc(e, 0x20, pack([0, 0, fake(lambda e: e.set_r(3, 0)), fake(event_category),
                                                  fake(lambda e: e.set_r(3, e.u32(e.r(3) + 8))), fake(lambda e: None), 0, 0]))

    # the manager's table: the access slots (never called), the TbRefRelease sub-table (descriptors copied from the component's),
    # the dtor thunk, TbSoundCore::Init (slot +0x38), the deleting destructor and the manager's Init (slot +0x40)
    def mgr_dtor(sub):
        def stub(e):
            L = e.cl
            this = e.r(3) - (4 if sub else 0)
            L.rec("Manager.~", (L.lab(this),))
            if T.s16(e.r(4)) > 0:
                delete_block(e, this)
        return stub

    def core_init(e):
        L = e.cl
        rec_info(L, e, "Core.Init", lbl(L, e.r(4)), e.r(5))
        e.write(e.r(3), pack([L.ret("Core.Init", 0)]))

    def mgr_init(e):
        L = e.cl
        L.rec("Manager.Init", (L.lab(e.r(4)),), (e.cstr(e.r(5)) if e.r(5) else "(null)",))
        e.write(e.r(3), pack([L.ret("Manager.Init", 0)]))

    words = [0] * 18
    for slot in range(0x08, 0x24, 4):
        words[slot // 4] = fake(lambda e: e.set_r(3, 0))
    words[0x28 // 4] = 0
    words[0x2C // 4] = e.u32(COMP_SUB_VT + 8)
    words[0x30 // 4] = e.u32(COMP_SUB_VT + 0xC)
    words[0x34 // 4] = fake(mgr_dtor(True))
    words[0x38 // 4] = fake(core_init)
    words[0x3C // 4] = fake(mgr_dtor(False))
    words[0x40 // 4] = fake(mgr_init)
    words[0x44 // 4] = fake(lambda e: None)
    VT["mgr"] = T.persist_alloc(e, 4 * len(words), pack(words))

    # the hooked component: the real component's table with Init (slot +0x38) replaced by a recording function
    def comp_init(e):
        L = e.cl
        rec_info(L, e, "Comp.Init", lbl(L, e.r(4)), e.r(5))
        e.write(e.r(3), pack([L.ret("Comp.Init", 0)]))

    table = bytearray(e.read(COMP_VT, 0x40))
    table[0x38:0x3C] = pack([fake(comp_init)])
    VT["comp_hook"] = T.persist_alloc(e, 0x40, bytes(table))

    # the hooked body: the real table with CreateSystemChain (slot +0x3c) replaced by a recording function that installs nothing
    def chain_hook(e):
        L, cfg = e.cl, e.r(4)
        L.rec("Body.CreateSystemChain", (L.lab(e.r(3)), L.lab(cfg), e.u32(cfg), e.u32(cfg + 4), e.u32(cfg + 8)))
        e.set_r(3, 0)

    table = bytearray(e.read(BODY_VT, 0x40))
    table[0x3C:0x40] = pack([fake(chain_hook)])
    VT["body_hook"] = T.persist_alloc(e, 0x40, bytes(table))


# ---------------------------------------------------------------- the case

def case(op, **kw):
    c = dict(op=op, mode=0, mgr=0, comp=0, lst=1, mref=1, cref=1, chain=0, bpos=[0] * 12, apos=[0] * 12, a=[0] * 8, ncat=0,
             kinds=[0, 0, 0], f08=[0, 0, 0], sflags=0, sprio=0, evcat=0, evev=0, hflags=0, hprio=0, qhi=0, qlo=0,
             names=["", "", "", "", "", ""], q={})
    c.update(kw)
    c["names"] = list(c["names"]) + [""] * (6 - len(c["names"]))
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def params(c):
    p = [0] * 64
    p[0], p[1], p[2], p[3], p[4], p[5], p[6] = c["mode"], c["mgr"], c["comp"], c["lst"], c["mref"], c["cref"], c["chain"]
    p[8:20] = c["bpos"]
    p[20:32] = c["apos"]
    p[32:40] = c["a"]
    p[40] = c["ncat"]
    p[41:44] = c["kinds"]
    p[44:47] = c["f08"]
    p[47], p[48], p[49], p[50] = c["sflags"], c["sprio"], c["evcat"], c["evev"]
    p[52], p[53], p[54], p[55] = c["hflags"], c["hprio"], c["qhi"], c["qlo"]
    return [v & MASK for v in p]


# ---------------------------------------------------------------- the PowerPC side of a case

def world_ppc(e, c, L):
    """The streamer, the sound, the placeholder, the arguments and (for the body operations) the body with its presets."""
    op = c["op"]
    w = e.w = type("W", (), {})()
    streamer = e.alloc(0x40, b"\xa5" * 0x40)
    L.labels[streamer] = 6
    w.streamer = streamer
    ncat = c["ncat"]
    array = e.alloc(max(4 * ncat, 4), b"\0" * max(4 * ncat, 4))
    for i in range(min(ncat, 3)):
        kind = c["kinds"][i]
        if kind == 0:
            continue
        container = e.alloc(0x30, b"\xa5" * 0x30)
        L.labels[container] = 31 + i
        data = 0
        if kind == 2:
            data = e.alloc(0x20, b"\xa5" * 0x20)
            L.labels[data] = 21 + i
            e.write(data + 8, struct.pack(">H", c["f08"][i] & 0xFFFF))
        e.write(container + 8, pack([data]))
        e.write(array + 4 * i, pack([container]))
    e.write(streamer + 0x1C, pack([array if ncat else 0, ncat]))
    sound = e.alloc(0x20, b"\xa5" * 0x20)
    L.labels[sound] = 7
    e.write(sound + 0x0C, pack([c["sprio"]]))
    e.write(sound + 0x10, struct.pack(">H", c["sflags"] & 0xFFFF))
    w.sound = sound
    holder = e.alloc(0x60, b"\xa5" * 0x60)
    L.labels[holder] = 5
    e.write(holder, pack([VT["holder"]]))
    e.write(holder + 0x2C, pack([c["hprio"]]))
    e.write(holder + 0x3C, pack([c["hflags"]]))
    w.holder = holder
    cfg = e.alloc(16, pack([c["qhi"], c["qlo"], c["a"][7], FILL]))
    L.labels[cfg] = 10
    w.cfg = cfg
    event = e.alloc(0x20, pack([VT["event"], c["evcat"], c["evev"], 0, 0, 0, 0, 0]))
    L.labels[event] = 8
    w.event = event
    command = e.alloc(8, pack([VT["cmd"], 0]))
    L.labels[command] = 9
    w.command = command
    w.args = []
    for i in range(4):
        block = e.alloc(12, pack(c["apos"][3 * i:3 * i + 3]))
        L.labels[block] = 41 + i
        w.args.append(block)
    w.body = w.mgr = w.comp = 0
    w.cap = w.caplist = 0
    if op in (K_CAPDTOR, K_CAPARROW, K_CAPCONV):
        if c["a"][1]:
            w.caplist = e.alloc(0x2C, pack([VT["list"], FILL]) + b"\xa5" * 0x24)
            L.labels[w.caplist] = 4
        w.cap = e.alloc(16, pack([w.caplist, FILL, FILL, FILL]))
        L.labels[w.cap] = 13
    if op in NO_BODY:
        return
    body = e.alloc(0x50, b"\xa5" * 0x50)
    w.body = body
    L.labels[body] = 1
    T.call_ppc(e, A["ctor"], [body])
    list_ptr = e.u32(body + 0x28)
    L.labels[list_ptr] = 4
    if not c["lst"]:
        e.write(body + 0x28, pack([0]))
    for offset, label in ((0x34, 12), (4, 13), (0x10, 14), (0x1C, 15)):
        L.labels[body + offset] = label
    e.write(body + 0x34, pack(c["bpos"][0:3]))
    e.write(body + 0x04, pack(c["bpos"][3:6]))
    e.write(body + 0x10, pack(c["bpos"][6:9]))
    e.write(body + 0x1C, pack(c["bpos"][9:12]))
    if c["mode"] == 1 and op == K_INIT:
        e.write(body, pack([VT["body_hook"]]))
    if c["mgr"]:
        mgr = e.alloc(0x70, b"\xa5" * 0x70)
        L.labels[mgr] = 2
        T.call_ppc(e, MGR_CTOR, [mgr])
        e.write(mgr + 8, pack([c["mref"]]))
        set_string(e, mgr + 0x2C, c["names"][4])
        e.write(body + 0x2C, pack([mgr]))
        w.mgr = mgr
    if c["comp"]:
        comp = e.alloc(0x18, b"\xa5" * 0x18)
        L.labels[comp] = 3
        T.call_ppc(e, COMP_CTOR, [comp])
        if c["comp"] == 2:
            e.write(comp, pack([VT["comp_hook"]]))
        e.write(comp + 8, pack([c["cref"]]))
        e.write(body + 0x30, pack([comp]))
        w.comp = comp
    if c["chain"] and w.mgr and w.comp:
        scratch = e.alloc(4, pack([FILL]))
        T.call_ppc(e, COMP_CHAIN, [scratch, w.comp, w.mgr])
        e.write(w.mgr + 8, pack([c["mref"]]))
        e.write(w.comp + 8, pack([c["cref"]]))


def ppc_run(e, c):
    op = c["op"]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    world_ppc(e, c, L)
    w = e.w
    body, mgr, comp = w.body, w.mgr, w.comp
    names = c["names"]
    a = c["a"]
    buf = e.alloc(4, pack([FILL]))
    texts = [e.alloc(64, names[i].encode("latin-1").ljust(64, b"\0")) for i in range(6)]
    handle_out = e.alloc(4, pack([a[0]]))
    out_word = e.alloc(4, pack([FILL]))
    outstr = e.alloc(0x14, b"\0" * 0x14)
    set_string(e, outstr, names[1])
    del L.log[:]
    L.track = True
    result = 0
    x = [0] * 8
    state, counts = True, True
    if op == K_SYSCTOR:
        raw = e.alloc(0x20, b"\xa5" * 0x20)
        T.call_ppc(e, A["sysctor"], [raw])
        x[0] = int(e.u32(raw) == SYS_VT and e.read(raw + 4, 0x1C) == b"\xa5" * 0x1C)
        state = counts = False
    elif op == K_CTOR:
        raw = e.alloc(0x50, b"\xa5" * 0x50)
        L.labels[raw] = 1
        for offset, label in ((0x34, 12), (4, 13), (0x10, 14), (0x1C, 15)):
            L.labels[raw + offset] = label
        T.call_ppc(e, A["ctor"], [raw])
        ok = e.u32(raw) == BODY_VT
        for slot, address in BODY_SLOTS.items():
            ok = ok and e.u32(e.u32(BODY_VT + slot)) == address
        ok = ok and e.read(raw + 0x40, 0x10) == b"\xa5" * 0x10
        x[0] = int(ok)
        body = raw
        counts = False
    elif op == K_DTOR:
        T.call_ppc(e, A["dtor"], [body, a[0] & MASK])
        state = False
    elif op == K_CAPCTOR:
        raw = e.alloc(16, b"\xa5" * 16)
        T.call_ppc(e, A["capctor"], [raw, a[0]])
        x[0] = e.u32(raw)
        state = counts = False
    elif op == K_CAPDTOR:
        T.call_ppc(e, A["capdtor"], [w.cap, a[0] & MASK])
        state = counts = False
    elif op in (K_CAPARROW, K_CAPCONV):
        result = lbl(L, T.call_ppc(e, A["caparrow" if op == K_CAPARROW else "capconv"], [w.cap])[0])
        state = counts = False
    elif op == K_CHAIN:
        T.call_ppc(e, A["chain"], [body, w.cfg])
        mgr, comp = e.u32(body + 0x2C), e.u32(body + 0x30)
    elif op == K_INIT:
        T.call_ppc(e, A["init"], [buf, body, texts[3], w.cfg])
        result = T.s32(e.u32(buf))
    elif op == K_RELEASE:
        T.call_ppc(e, A["release"], [buf, 0 if c["a"][1] else body])
        result = T.s32(e.u32(buf))
        state = False
    elif op == K_DEREG:
        T.call_ppc(e, A["dereg"], [buf, body, a[0] & MASK])
        result = T.s32(e.u32(buf))
    elif op == K_QUALIFY:
        T.call_ppc(e, A["qualify"], [body, texts[0], outstr])
    elif op == K_REGISTER:
        T.call_ppc(e, A["register"], [buf, body, texts[0], texts[1], handle_out])
        result = T.s32(e.u32(buf))
    elif op == K_SEND:
        result = T.call_ppc(e, A["send"], [body, a[0] & MASK, w.event])[0] & MASK
    elif op == K_CANBE:
        result = T.call_ppc(e, A["canbe"], [body, a[0] & MASK])[0] & MASK
    elif op == K_UPDHANDLE:
        result = T.call_ppc(e, A["updhandle"], [body, a[0] & MASK, w.args[0], w.args[1]])[0] & MASK
    elif op == K_UPDPARAM:
        result = T.call_ppc(e, A["updparam"], [body, a[0] & MASK, a[1] & 0xFF, a[2] & MASK])[0] & MASK
    elif op == K_STOP:
        T.call_ppc(e, A["stop"], [buf, body, a[0] & MASK])
        result = T.s32(e.u32(buf))
    elif op == K_SYSCMD:
        T.call_ppc(e, A["syscmd"], [buf, body, w.command])
        result = T.s32(e.u32(buf))
    elif op == K_ORIENT:
        T.call_ppc(e, A["orient"], [buf, body] + w.args)
        result = T.s32(e.u32(buf))
    elif op in (K_UPDDIST, K_UPDLOW):
        result = T.call_ppc(e, A["upddist" if op == K_UPDDIST else "updlow"], [body, a[0] & MASK, T.F(bits_to_float(a[1]))])[0] & MASK
    elif op in (K_GETSM, K_GETPH):
        result = lbl(L, T.call_ppc(e, A["getsm" if op == K_GETSM else "getph"], [body])[0])
    elif op in (K_CORE, K_COMP):
        core = T.call_ppc(e, A["core" if op == K_CORE else "comp"], [])[0]
        result = lbl(L, core)
        x[0] = lbl(L, e.u32(core + 0x18)) if op == K_CORE else 0
        x[1] = lbl(L, e.u32(core + 0x1C)) if op == K_CORE else 0
        x[2], x[3], x[4], x[5] = lbl(L, e.u32(core + 0xC)), lbl(L, e.u32(core + 0x10)), e.u32(core + 0x14), e.u32(core + 8)
        state = counts = False
    elif op == K_CREATE:
        T.call_ppc(e, A["create"], [buf, texts[3], out_word, w.cfg])
        result = T.s32(e.u32(buf))
        body = e.u32(out_word)
        mgr = e.u32(body + 0x2C) if body else 0
        comp = e.u32(body + 0x30) if body else 0
        counts = False
        state = bool(body)
    elif op == K_ISSLASH:
        result = T.call_ppc(e, A["isslash"], [a[0] & 0xFF])[0] & MASK
        state = counts = False
    L.track = False
    res = [0] * 34
    res[O_RESULT] = result & MASK
    if op == K_CREATE:
        res[O_OUTPTR] = lbl(L, e.u32(out_word))
    if (state and body and op not in (K_DTOR, K_RELEASE)) or op == K_CTOR:
        res[O_BODYPOS:O_BODYPOS + 3] = words_at(e, body + 0x34, 3)
        res[O_BODYPOS + 3:O_BODYPOS + 6] = words_at(e, body + 0x04, 3)
        res[O_BODYPOS + 6:O_BODYPOS + 9] = words_at(e, body + 0x10, 3)
        res[O_BODYPOS + 9:O_BODYPOS + 12] = words_at(e, body + 0x1C, 3)
        res[O_MGR] = lbl(L, e.u32(body + 0x2C))
        res[O_COMP] = lbl(L, e.u32(body + 0x30))
        res[O_LIST] = lbl(L, e.u32(body + 0x28))
        if op != K_CTOR:
            if mgr:
                res[O_AUDIO] = lbl(L, e.u32(mgr + 0x44))
                res[O_LISTENER] = lbl(L, e.u32(mgr + 0x40))
                res[O_PREV] = lbl(L, e.u32(mgr + 0x10))
            if comp:
                res[O_NEXT] = lbl(L, e.u32(comp + 0xC))
    if counts:
        if mgr:
            res[O_MREF] = e.u32(mgr + 8)
        if comp:
            res[O_CREF] = e.u32(comp + 8)
    res[O_HFLAGS] = e.u32(w.holder + 0x3C)
    res[O_NCATS] = e.u32(w.streamer + 0x20)
    if op == K_REGISTER:
        res[O_HANDLE] = e.u32(handle_out)
    res[O_X:O_X + 8] = x
    text = string(e, outstr) if op == K_QUALIFY else ""
    return dict(out=tuple(res), text=text, log=list(L.log))


def host_run(lib, c):
    p = (C.c_uint32 * 64)(*params(c))
    names = bytearray(64 * 6)
    for i, text in enumerate(c["names"]):
        raw = text.encode("latin-1")
        names[64 * i:64 * i + len(raw)] = raw
    name_buffer = (C.c_char * len(names)).from_buffer(names)
    out = (C.c_uint32 * 64)()
    CL.host_begin(lib, {}, c["q"])
    if not (c["op"] == K_RELEASE and c["a"][1]):
        lib.tpw_ab_run(c["op"], p, name_buffer, out)
    log = CL.host_end(lib)
    text = lib.tpw_ab_text().decode("latin-1") if c["op"] == K_QUALIFY else ""
    return dict(out=tuple(out[:34]), text=text, log=log)


# ---------------------------------------------------------------- the cases

POOL = [0, 0x80000000, 0x3F800000, 0x40000000, 0xC0400000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800001]
POOL_Q = [(0, 0), (0x3FF00000, 0), (0x7FF80000, 0), (0x7FF00000, 0), (0xFFF00000, 1), (0x40590000, 0), (0x80000000, 0),
          (0x7FF00000, 1), (0x3FE00000, 0x12345678)]
PATH_POOL = ["C:\\x", "C:x", "C:/x", "\\\\server\\x", "//srv/y", "/x", "\\x", "x", "", ":", "::", "a::", "a:\\", "::x", "C::x",
             "a", "ab", "\\\\", "//", "/\\", "\\/", "c:", "c:/", "Sound\\a.sfx", "x:y\\z", ":\\", "ab:\\q"]
SOUND_PATHS = ["", "Sound:", "ROOT:Sound:Banks:", "X" * 30]


def rpos(r):
    return r.choice(POOL) if r.random() < 0.75 else r.getrandbits(32)


def rtext(r):
    alphabet = "abC:/\\x. "
    return "".join(r.choice(alphabet) for _ in range(r.randrange(0, 7)))


def rconfig(r):
    qhi, qlo = r.choice(POOL_Q) if r.random() < 0.7 else (r.getrandbits(32), r.getrandbits(32))
    return dict(qhi=qhi, qlo=qlo, a=None, channels=r.choice([0, 1, 2, 8, 0x10, 0x1F, 0xFFFFFFFF, r.getrandbits(32)]))


def with_channels(c, channels):
    c["a"][7] = channels
    return c


def rq(r, *keys, pool=None):
    return {key: [r.choice(pool or QUEUE_ERRORS)] for key in keys}


DTOR_COMBOS = [(0, 0, 0, 1, 1, 1), (0, 0, 0, 1, 1, 0), (1, 0, 0, 1, 1, 1), (1, 0, 0, 2, 1, 1), (1, 0, 0, 3, 1, 0),
               (0, 1, 0, 1, 1, 1), (0, 1, 0, 1, 2, 1), (1, 1, 0, 1, 1, 1), (1, 1, 0, 2, 2, 1), (1, 1, 1, 2, 1, 1),
               (1, 1, 1, 2, 2, 1), (1, 1, 1, 3, 3, 0), (1, 1, 1, 2, 1, 0), (1, 1, 1, 3, 1, 1)]


def world_random(r, op):
    """Presets common to the body operations."""
    mgr, comp, chain = r.choice([(1, 1, 0), (1, 1, 1), (1, 0, 0), (0, 1, 0), (0, 0, 0), (1, 1, 0)])
    c = case(op, mgr=mgr, comp=comp, chain=chain, lst=r.choice([0, 1, 1]), mref=2 if chain else r.choice([1, 2, 3]),
             cref=r.choice([1, 2]), bpos=[rpos(r) for _ in range(12)], apos=[rpos(r) for _ in range(12)])
    c["a"] = [r.getrandbits(32) for _ in range(8)]
    return c


def make_random(op):
    def rnd(r):
        c = world_random(r, op)
        specialise(r, c, op)
        return c
    return rnd


def specialise(r, c, op):
    a = c["a"]
    if op in (K_SYSCTOR, K_CTOR, K_CORE, K_COMP):
        c["mgr"] = c["comp"] = c["chain"] = 0
    elif op in (K_DTOR, K_RELEASE):
        combo = r.choice(DTOR_COMBOS)
        c["mgr"], c["comp"], c["chain"], c["mref"], c["cref"], c["lst"] = combo
        a[0] = r.choice(DTOR_FLAGS) & MASK
        a[1] = 1 if op == K_RELEASE and r.random() < 0.1 else 0
        if a[1]:
            c["mgr"] = c["comp"] = c["chain"] = 0
    elif op in (K_CAPCTOR,):
        a[0] = r.choice([0, 0x1234, 0xFFFFFFFF, r.getrandbits(32)])
    elif op in (K_CAPDTOR, K_CAPARROW, K_CAPCONV):
        a[0] = r.choice(DTOR_FLAGS) & MASK
        a[1] = r.randrange(2)
    elif op == K_CHAIN:
        c["mgr"] = c["comp"] = c["chain"] = 0
        q, cfg = rconfig(r)[:2] if False else (None, None)
        c["qhi"], c["qlo"] = r.choice(POOL_Q)
    elif op == K_INIT:
        cfg = rconfig(r)
        c["qhi"], c["qlo"] = cfg["qhi"], cfg["qlo"]
        a[7] = cfg["channels"]
        c["names"][3] = r.choice(["", "Sound:", "x" * 40, rtext(r)])
        c["mode"] = r.randrange(2)
        if c["mode"] == 0:
            c["mgr"] = c["comp"] = c["chain"] = 0
            c["q"] = {"Core.Init": [r.choice(GOOD_ERRORS)], "Manager.Init": [r.choice(QUEUE_ERRORS)]}
        else:
            c["mgr"] = 1
            c["comp"] = r.choice([1, 2, 2])
            c["chain"] = r.choice([0, 1]) if c["comp"] == 1 else 0
            c["mref"], c["cref"] = 2 if c["chain"] else 1, 1
            c["q"] = {"Comp.Init": [r.choice(QUEUE_ERRORS)], "Core.Init": [r.choice(GOOD_ERRORS)],
                      "Manager.Init": [r.choice(QUEUE_ERRORS)]}
    elif op == K_DEREG:
        a[0] = r.choice([0, 1, 2, 0xFFFFFFFF, r.getrandbits(32)])
        c["q"] = {"Streamer.Remove": [r.choice(QUEUE_ERRORS)]}
    elif op == K_QUALIFY:
        c["mgr"] = 1
        c["names"][0] = r.choice(PATH_POOL) if r.random() < 0.7 else rtext(r)
        c["names"][1] = r.choice(["", "junk", "J" * 20])
        c["names"][4] = r.choice(SOUND_PATHS)
    elif op == K_REGISTER:
        c["mgr"] = r.choice([0, 1, 1])
        c["names"][0] = r.choice(PATH_POOL) if r.random() < 0.7 else rtext(r)
        c["names"][1] = r.choice(["music", "", "Sfx", rtext(r)])
        c["names"][4] = r.choice(SOUND_PATHS)
        c["q"] = {"Streamer.Register": [r.choice(QUEUE_ERRORS)], "Streamer.handle": [r.choice([0, 5, 0xFFFFFFFF, r.getrandbits(32)])]}
    elif op == K_SEND:
        c["mgr"] = r.choice([1, 1, 1, 0])
        c["lst"] = r.choice([1, 1, 1, 0])
        c["ncat"] = r.randrange(0, 4)
        c["kinds"] = [r.choice([0, 1, 2, 2]) for _ in range(3)]
        c["f08"] = [r.choice([0, 1, 0x1234, 0xFF, 0x100, 0xFFFF, r.getrandbits(16)]) for _ in range(3)]
        c["evcat"] = r.choice([0, 1, 2, 3, 5, 0xFFFF, r.randrange(4)])
        c["evev"] = r.getrandbits(32)
        c["sflags"] = r.choice([0, 4, 8, 0xC, 1, 0x10, 0xFFFF, 5, 0xA, 0xFFFB, 0xFFF7, 0x3, r.getrandbits(16)])
        c["sprio"] = r.choice([0, 1, 2, 3, 4, 0xFFFF, 0x10000, 0x80000000, 0xFFFFFFFF, r.getrandbits(32)])
        c["hflags"] = r.choice([0, 0x10, 0x100, 0xFFFFFFFF, 0x1F, 0xFFFFFFEF, 0x20, r.getrandbits(32)])
        c["hprio"] = r.choice([0, 3, 0x00050003, 0xFFFF0002, 0x0001FFFF, 0xABCD1234, r.getrandbits(32)])
        a[0] = r.choice([0, 1, 0x12345, 0xFFFFFFFF, r.getrandbits(32)])
        c["q"] = {"Map": [r.choice([0, 1, 1, 1])], "Manager.VolumeOn": [r.choice([0, 1])], "List.Find": [r.choice([0, 1])],
                  "List.Create": [r.choice([0, 0x777, r.getrandbits(32)])]}
    elif op == K_CANBE:
        a[0] = r.choice([0, 4, 8, 0xC, 1, 2, 0x10, 0xFFFFFFF3, 0xFFFFFFFB, 0xFFFFFFF7, r.getrandbits(32)])
    elif op == K_UPDHANDLE:
        c["lst"] = 1
        a[0] = r.choice([0, 1, 0x12345, 0xFFFFFFFF, r.getrandbits(32)])
        c["q"] = {"List.Update": [r.choice([0, 1, 0x7777, r.getrandbits(32)])]}
    elif op == K_UPDPARAM:
        a[0] = r.choice([0, 1, 0x12345, 0xFFFFFFFF, r.getrandbits(32)])
        a[1] = r.choice([0, 1, 0x7F, 0xFF, r.randrange(256)])
        c["q"] = {"List.UpdateParameter": [r.choice([0, 1, 0x7777, r.getrandbits(32)])]}
    elif op == K_STOP:
        a[0] = r.choice([0, 1, 0x12345, 0xFFFFFFFF, r.getrandbits(32)])
        if a[0] != 0:
            c["lst"] = 1
        c["q"] = {"List.Find": [r.choice([0, 1])], "Holder.Stop": [r.choice(QUEUE_ERRORS)]}
    elif op == K_SYSCMD:
        c["q"] = {"Cmd.Execute": [r.choice(QUEUE_ERRORS)]}
    elif op == K_ORIENT:
        c["mgr"] = r.choice([0, 1, 1, 1])
        # each argument equals the stored one (a copy of the preset) or differs
        for i in range(4):
            if r.random() < 0.5:
                c["apos"][3 * i:3 * i + 3] = c["bpos"][3 * ((i + 1) % 4 if i == 0 else i - 0 if False else i):3 * ((i + 1) % 4 if i == 0 else i) + 3] if False else c["apos"][3 * i:3 * i + 3]
        order = [0, 1, 2, 3]   # argument i is compared with the member at the same place (position, forward, up, velocity)
        stored = {0: c["bpos"][0:3], 1: c["bpos"][3:6], 2: c["bpos"][6:9], 3: c["bpos"][9:12]}
        for i in order:
            if r.random() < 0.5:
                c["apos"][3 * i:3 * i + 3] = list(stored[i])
        c["q"] = {"Manager.Orient": [r.choice(QUEUE_ERRORS)]}
    elif op in (K_UPDDIST, K_UPDLOW):
        a[0] = r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])
        a[1] = r.choice(POOL + [r.getrandbits(32)])
    elif op in (K_GETSM, K_GETPH):
        pass
    elif op == K_CREATE:
        cfg = rconfig(r)
        c["qhi"], c["qlo"] = cfg["qhi"], cfg["qlo"]
        a[7] = cfg["channels"]
        c["names"][3] = r.choice(["", "Sound:", "x" * 40, rtext(r)])
        c["q"] = {"Core.Init": [r.choice(GOOD_ERRORS)], "Manager.Init": [r.choice(QUEUE_ERRORS)]}
    elif op == K_ISSLASH:
        a[0] = r.choice([0x5C, 0x2F, 0x3A, r.randrange(256), 0x5B, 0x5D, 0x2E, 0x2D, 0x3B, 0x80, 0xFF, 0xDC])


def edge_cases(op):
    def edges():
        cs = []
        if op in (K_SYSCTOR, K_CTOR, K_CORE, K_COMP):
            cs.append(case(op))
        if op in (K_DTOR, K_RELEASE):
            for flag in (DTOR_FLAGS if op == K_DTOR else [1]):
                for combo in DTOR_COMBOS:
                    mgr, comp, chain, mref, cref, lst = combo
                    cs.append(case(op, mgr=mgr, comp=comp, chain=chain, mref=mref, cref=cref, lst=lst, a=[flag & MASK, 0, 0, 0, 0, 0, 0, 0]))
            if op == K_RELEASE:
                cs.append(case(op, a=[0, 1, 0, 0, 0, 0, 0, 0]))
                for lst in (0, 1):
                    cs.append(case(op, lst=lst))
        if op == K_CAPCTOR:
            for ptr in (0, 1, 0x1234, 0xFFFFFFFF, 0x80000000):
                cs.append(case(op, a=[ptr, 0, 0, 0, 0, 0, 0, 0]))
        if op == K_CAPDTOR:
            for flag in DTOR_FLAGS:
                for has in (0, 1):
                    cs.append(case(op, a=[flag & MASK, has, 0, 0, 0, 0, 0, 0]))
        if op in (K_CAPARROW, K_CAPCONV):
            for has in (0, 1):
                cs.append(case(op, a=[0, has, 0, 0, 0, 0, 0, 0]))
        if op == K_CHAIN:
            for qhi, qlo in POOL_Q[:3]:
                cs.append(case(op, qhi=qhi, qlo=qlo))
        if op == K_INIT:
            for mode in (0, 1):
                for ce in (QUEUE_ERRORS if mode == 1 else [0]):
                    for me in QUEUE_ERRORS:
                        for core in ([0, 5] if mode == 0 else [0]):
                            comp = 2 if mode == 1 else 0
                            cs.append(case(op, mode=mode, mgr=mode, comp=comp, mref=1, cref=1, a=[0, 0, 0, 0, 0, 0, 0, 0x10],
                                           names=["", "", "", "Sound:", ""], qhi=0x3FF00000, qlo=0,
                                           q={"Comp.Init": ce, "Manager.Init": me, "Core.Init": core}))
            for chain in (0, 1):
                for comp_ in (1, 2):
                    for me in (0, -1):
                        cs.append(case(op, mode=1, mgr=1, comp=comp_, chain=chain, mref=2 if chain else 1, cref=1,
                                       a=[0, 0, 0, 0, 0, 0, 0, 3], names=["", "", "", "Sound:", ""],
                                       q={"Comp.Init": 0, "Manager.Init": me, "Core.Init": 1}))
            for qhi, qlo in POOL_Q:
                for channels in (0, 1, 0x10, 0xFFFFFFFF, 0x80000000):
                    cs.append(case(op, mode=1, mgr=1, comp=2, a=[0, 0, 0, 0, 0, 0, 0, channels], qhi=qhi, qlo=qlo,
                                   names=["", "", "", "n", ""], q={"Comp.Init": 0, "Manager.Init": 0}))
        if op == K_DEREG:
            for cat in (0, 1, 2, 7, 0x80000000, 0xFFFFFFFF):
                for err in QUEUE_ERRORS:
                    cs.append(case(op, a=[cat, 0, 0, 0, 0, 0, 0, 0], q={"Streamer.Remove": err}))
        if op == K_QUALIFY:
            for path in PATH_POOL:
                for sound in SOUND_PATHS:
                    cs.append(case(op, mgr=1, names=[path, "junk", "", "", sound]))
        if op == K_REGISTER:
            for mgr in (0, 1):
                for path in PATH_POOL[:8] + [""]:
                    for err in (0, -1):
                        cs.append(case(op, mgr=mgr, a=[0x1234, 0, 0, 0, 0, 0, 0, 0], names=[path, "music", "", "", "Sound:"],
                                       q={"Streamer.Register": err, "Streamer.handle": 7}))
        if op == K_SEND:
            flags = [0, 4, 8, 0xC, 1, 0x10, 0xFFFF, 5, 0xA, 0xFFFB, 0xFFF7, 3]
            for sflags in flags:
                for vol in (0, 1):
                    for holder in (0, 1):
                        cs.append(case(op, mgr=1, ncat=2, kinds=[0, 2, 0], f08=[0, 0x1234, 0], evcat=1, sflags=sflags, sprio=3,
                                       hflags=0, hprio=0x00050003 if holder else 0, a=[0x55, 0, 0, 0, 0, 0, 0, 0],
                                       q={"Map": 1, "Manager.VolumeOn": vol, "List.Find": holder, "List.Create": 0x777}))
            for sprio in (0, 2, 3, 4, 0xFFFF, 0x10000, 0x10003, 0x80000000, 0xFFFFFFFF):
                for hprio in (0x00000003, 0xFFFF0003, 0x0000FFFF, 0x00010000 | 3, 0, 0xABCD0002):
                    for hflags in (0, 0x10, 0x100, 0xFFFFFFEF):
                        cs.append(case(op, mgr=1, ncat=1, kinds=[2, 0, 0], f08=[0xFF, 0, 0], evcat=0, sflags=0, sprio=sprio,
                                       hflags=hflags, hprio=hprio, a=[0x77, 0, 0, 0, 0, 0, 0, 0],
                                       q={"Map": 1, "Manager.VolumeOn": 1, "List.Find": 1, "List.Create": 0x888}))
            for lst in (0, 1):
                for mgr in (0, 1):
                    cs.append(case(op, mgr=mgr, lst=lst, ncat=1, kinds=[2, 0, 0], evcat=0, sprio=3,
                                   a=[0x42, 0, 0, 0, 0, 0, 0, 0], q={"Map": 1, "Manager.VolumeOn": 1, "List.Find": 0, "List.Create": 9}))
            for ncat in (0, 1, 2, 3):
                for evcat in (0, 1, 2, 3, 4, 9):
                    for kinds in ([0, 0, 0], [1, 1, 1], [2, 2, 2], [2, 1, 0]):
                        cs.append(case(op, mgr=1, ncat=ncat, kinds=kinds, f08=[0x11, 0x22, 0x33], evcat=evcat, sprio=1,
                                       a=[0x99, 0, 0, 0, 0, 0, 0, 0], q={"Map": 1, "Manager.VolumeOn": 1, "List.Find": 0, "List.Create": 5}))
            for f08 in (0, 1, 0x1234, 0xFF, 0x100, 0xFFFF, 0xAB):
                cs.append(case(op, mgr=1, ncat=1, kinds=[2, 0, 0], f08=[f08, 0, 0], sflags=0, sprio=1, a=[7, 0, 0, 0, 0, 0, 0, 0],
                               q={"Map": 1, "Manager.VolumeOn": 0, "List.Find": 0, "List.Create": 5}))
            cs.append(case(op, mgr=1, ncat=1, kinds=[2, 0, 0], sflags=0, sprio=1, a=[7, 0, 0, 0, 0, 0, 0, 0],
                           q={"Map": 0, "Manager.VolumeOn": 1, "List.Find": 1, "List.Create": 5}))
        if op == K_CANBE:
            for flags in list(range(0, 18)) + [0xFFFFFFF3, 0xFFFFFFFF, 0x80000000, 0x100, 0xF0, 0xFFFFFFFB, 0xFFFFFFF7]:
                cs.append(case(op, a=[flags, 0, 0, 0, 0, 0, 0, 0]))
        if op == K_UPDHANDLE:
            for handle in (0, 1, 0x12345, 0xFFFFFFFF):
                for answer in (0, 1, 0x7777):
                    cs.append(case(op, a=[handle, 0, 0, 0, 0, 0, 0, 0], apos=POOL[:6] + POOL[3:9], q={"List.Update": answer}))
        if op == K_UPDPARAM:
            for handle in (0, 1, 0xFFFFFFFF):
                for lst in (0, 1):
                    for param in (0, 1, 0xFF):
                        cs.append(case(op, lst=lst, a=[handle, param, 0xDEADBEEF, 0, 0, 0, 0, 0], q={"List.UpdateParameter": 0x55}))
        if op == K_STOP:
            for handle in (0, 5, 0xFFFFFFFF):
                for lst in (0, 1):
                    for find in (0, 1):
                        for err in (0, -1, 5):
                            if handle != 0 or True:
                                cs.append(case(op, lst=1 if handle else lst, a=[handle, 0, 0, 0, 0, 0, 0, 0],
                                               q={"List.Find": find, "Holder.Stop": err}))
        if op == K_SYSCMD:
            for mgr in (0, 1):
                for lst in (0, 1):
                    for err in QUEUE_ERRORS:
                        cs.append(case(op, mgr=mgr, lst=lst, q={"Cmd.Execute": err}))
        if op == K_ORIENT:
            same = [0, 1, 2, 3]
            stored = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
            for mgr in (0, 1):
                for mask in range(16):
                    apos = list(range(101, 113))
                    for i in range(4):
                        if mask >> i & 1:
                            apos[3 * i:3 * i + 3] = stored[3 * i:3 * i + 3]
                    cs.append(case(op, mgr=mgr, bpos=stored, apos=apos, q={"Manager.Orient": 0}))
            for err in QUEUE_ERRORS:
                cs.append(case(op, mgr=1, bpos=stored, apos=[9] * 12, q={"Manager.Orient": err}))
            # a single word different in each of the twelve places
            for place in range(12):
                apos = list(stored)
                apos[place] ^= 0x80000000
                cs.append(case(op, mgr=1, bpos=stored, apos=apos, q={"Manager.Orient": 0}))
            cs.append(case(op, mgr=1, bpos=[0x7FC00000] * 12, apos=[0x7FC00000] * 12))
            cs.append(case(op, mgr=1, bpos=[0] * 12, apos=[0x80000000] * 3 + [0] * 9))
        if op in (K_UPDDIST, K_UPDLOW):
            for handle in (0, 1, 0xFFFFFFFF, 0x12345678):
                for bits in POOL:
                    cs.append(case(op, a=[handle, bits, 0, 0, 0, 0, 0, 0]))
        if op in (K_GETSM, K_GETPH):
            for mgr in (0, 1):
                for lst in (0, 1):
                    cs.append(case(op, mgr=mgr, lst=lst))
        if op == K_CREATE:
            for me in QUEUE_ERRORS:
                for core in (0, 1):
                    cs.append(case(op, a=[0, 0, 0, 0, 0, 0, 0, 0x10], names=["", "", "", "Sound:", ""], qhi=0x3FE00000, qlo=0,
                                   q={"Manager.Init": me, "Core.Init": core}))
            for qhi, qlo in POOL_Q:
                cs.append(case(op, a=[0, 0, 0, 0, 0, 0, 0, 4], qhi=qhi, qlo=qlo, names=["", "", "", "abc", ""], q={"Manager.Init": 0}))
        if op == K_ISSLASH:
            for ch in range(256):
                cs.append(case(op, a=[ch, 0, 0, 0, 0, 0, 0, 0]))
        return cs
    return edges


# ---------------------------------------------------------------- the tests

def audio_tests():
    table = [
        (K_SYSCTOR, "TbAudioSystem::TbAudioSystem 0x10016384"),
        (K_CTOR, "TbAudioBody::TbAudioBody 0x100161a0"),
        (K_DTOR, "TbAudioBody::~TbAudioBody 0x100163bc"),
        (K_CAPCTOR, "CAutoPtr (CAudioPlaceHolderList)::CAutoPtr 0x10016324"),
        (K_CAPDTOR, "CAutoPtr (CAudioPlaceHolderList)::~CAutoPtr 0x10016270"),
        (K_CAPARROW, "CAutoPtr (CAudioPlaceHolderList)::operator-> 0x10016e18"),
        (K_CAPCONV, "CAutoPtr (CAudioPlaceHolderList)::operator CAudioPlaceHolderList* 0x100172b4"),
        (K_CHAIN, "TbAudioBody::CreateSystemChain 0x10016480"),
        (K_INIT, "TbAudioBody::Init 0x1001653c"),
        (K_RELEASE, "TbAudioBody::Release 0x10016804"),
        (K_DEREG, "TbAudioBody::DeregisterCatagoryBanks 0x10016884"),
        (K_QUALIFY, "TbAudioBody::QualifyPath 0x10016928"),
        (K_REGISTER, "TbAudioBody::RegisterCatagory 0x10016ae4"),
        (K_SEND, "TbAudioBody::SendEvent 0x10016c1c"),
        (K_CANBE, "TbAudioBody::CanBeDroppedDueToVolControl 0x10016f68"),
        (K_UPDHANDLE, "TbAudioBody::UpdateHandle 0x10017314"),
        (K_UPDPARAM, "TbAudioBody::UpdateParameter 0x100173d4"),
        (K_STOP, "TbAudioBody::StopHandle 0x10017490"),
        (K_SYSCMD, "TbAudioBody::SystemCommand 0x1001753c"),
        (K_ORIENT, "TbAudioBody::SetListenerOrientation 0x10017610"),
        (K_UPDDIST, "TbAudioBody::UpdateDistortion 0x1001781c"),
        (K_UPDLOW, "TbAudioBody::UpdateLowPass 0x1001785c"),
        (K_GETSM, "TbAudioBody::GetSoundManager 0x10028110"),
        (K_GETPH, "TbAudioBody::GetPlaceHolderList 0x1002814c"),
        (K_CORE, "CAudioComponentFactory::CreateCore 0x10001b54"),
        (K_COMP, "CAudioComponentFactory::CreateComponent 0x10001bd8"),
        (K_CREATE, "CreateAudioObject 0x1002a18c"),
        (K_ISSLASH, "_IsSlash 0x10016a84"),
    ]
    return [FnTest(name, ppc_run, host_run, edge=edge_cases(op), rnd=make_random(op)) for op, name in table]


TESTS = audio_tests()
