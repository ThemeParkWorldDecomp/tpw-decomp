"""Unit w79b, the sound library's core, against libtpw_unit_SoundCore.dylib (tests/unit_SoundCore.cpp), in the program
"sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/TbSoundCore.cpp   TbSoundCore: constructor 0x1000b618, destructor 0x1000b738, ModifyMaxBuffers 0x1000b7e8, Init 0x1000b85c,
                        Close 0x1000bb50, GetSystemInfo 0x1000bc8c, StopAndReleasePipe 0x1000bf50, GetSampleInfo 0x1000c058,
                        NewSample 0x1000c220, NewControl 0x1000c404, SystemCommand 0x1000c470, GetSystemModule 0x1000c500,
                        GetSampleStore 0x1000c53c; CModuleFactory: GenerateModule 0x10006ff8, CloseModule 0x10007120;
                        CMemoryCopyInit: constructor 0x10014b78, destructor 0x1000b6ac; CMemoryCopy::CStandardCopy constructor
                        0x10014bfc and Copy 0x10014cac, CMemoryCopy::CCopy constructor 0x10014c70
  src/CAudioComponent.cpp  CAudioComponentAccess constructor 0x100013f4; CAudioComponent: constructor 0x10001364, destructor
                        0x10001434, Init 0x100014ec, Chain 0x100015e4, UnchainAndRelease 0x100016d0, GetComponent 0x10001778,
                        and the forwarders GetSystemInfo 0x100017f4, GetSampleInfo 0x10001878, NewSample 0x10001900, NewControl
                        0x10001978, GetSystemModule 0x100019f0, GetSampleStore 0x10001a58, SystemCommand 0x10001ac0
  src/snd_commands.cpp  the SystemCommand constructors: Command 0x1000ba38, HandleCommand 0x1000c380, SetSystemInfo 0x1000b9a0,
                        SetSampleInfo 0x1000c2e8, Stop 0x10019c34, Pause 0x1001a60c, Restart 0x1001a748, Start 0x10027eb4,
                        UpdateAllVolumes 0x10027a64, Process 0x100280a4, StopAll 0x10028270, PauseAll 0x10028404,
                        RestartAll 0x1002852c

Each function runs as the original in the emulator and as the host's translation on a world built from the case: the core
(with a module and a store or without), a sample info, a system info, a channel, a test command; or, for the component chain,
in scripts of operations on six node slots (real CAudioComponent objects and FakeComponents that record every slot) with the
state of every node after each operation. Every callee outside the unit is a fake that records the same event on both sides:
the runtime's new (the Mac's size and a label; blocks are filled with 0xA5) and delete (a labelled block), the sample store
(placeholder for w79c), the channel getters (w79c), CSprocketsModule (p-sound; the Mac side's module fake has a recording
function in each slot the core uses and a recorder in every other slot, so that a wrong slot number shows as another name), the
command Execute bodies (w79d), the TbRefRelease imports (a count at +8, as src/TbRefRelease.cpp). The unit's own functions, the
w77 info records and the w79a handle and module code run as the original. TbError is returned through a hidden r3 buffer (this
in r4) and read from the buffer.

Pointers compare as labels (0 NULL, the label of the object, -1 for anything else). The PowerPC side starts the code under test
with the log on and the objects of the world made with it off (the host does the same).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundCore"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

# statics and vtables
S_COPY = 0x1003F34C       # CMemoryCopy::_mpCopy
S_COUNT = 0x1003F350      # CChannelState::mChannelState (the count of channels in use)
S_SPEAKER = 0x1003F348    # TbSpeaker::AutoDelete::_mpSpeaker
CORE_VT, CORE_SUB_VT = 0x1003C120, 0x1003C144
COMP_VT, COMP_SUB_VT = 0x10035C24, 0x10035C48
ACCESS_VT, STD_COPY_VT, COPY_VT = 0x10035C64, 0x1003C900, 0x1003C8E0
CORE_SLOTS = {0x08: 0x1000BC8C, 0x0C: 0x1000C058, 0x10: 0x1000C220, 0x14: 0x1000C404, 0x18: 0x1000C470, 0x1C: 0x1000C500,
              0x20: 0x1000C53C, 0x38: 0x1000B85C, 0x3C: 0x1000B738}
COMP_SLOTS = {0x08: 0x100017F4, 0x0C: 0x10001878, 0x10: 0x10001900, 0x14: 0x10001978, 0x18: 0x10001AC0, 0x1C: 0x100019F0,
              0x20: 0x10001A58, 0x38: 0x100014EC, 0x3C: 0x10001434}

# the functions
A = dict(
    core_ctor=0x1000B618, core_dtor=0x1000B738, modify=0x1000B7E8, init=0x1000B85C, close=0x1000BB50, getsys=0x1000BC8C,
    stoprel=0x1000BF50, getsamp=0x1000C058, newsamp=0x1000C220, newctl=0x1000C404, syscmd=0x1000C470, getmod=0x1000C500,
    getstore=0x1000C53C, generate=0x10006FF8, closemod=0x10007120, ci_ctor=0x10014B78, ci_dtor=0x1000B6AC,
    std_ctor=0x10014BFC, std_copy=0x10014CAC, copy_ctor=0x10014C70, access_ctor=0x100013F4, comp_ctor=0x10001364,
    comp_dtor=0x10001434, comp_init=0x100014EC, chain=0x100015E4, unchain=0x100016D0, getcomp=0x10001778,
    module_dtor=0x100071D4,
)
# the callees outside the unit
STORE_CTOR, STORE_INIT, STORE_RELEASE_ALL, STORE_DTOR = 0x1000FA64, 0x1000FC60, 0x1000FE34, 0x10010054
STORE_USED, STORE_MAX, STORE_INDEX, STORE_NEW_SAMPLE, STORE_NEW_CONTROL = 0x1000BE68, 0x1000BEB4, 0x100100E8, 0x1001093C, 0x10010B6C
CH_GET_INDEX, CH_RELEASE_PIPE, CH_VOLUME, CH_PRIORITY = 0x1000A19C, 0x1000A7E0, 0x1000C1DC, 0x1000C19C
SPROCKETS_CTOR = 0x1002B234

# the command constructors: name, constructor, vtable, Execute, kind of arguments (h handle, i system info, s sample info)
COMMANDS = [
    ("Command", 0x1000BA38, 0x1003C160, None, ""),
    ("HandleCommand", 0x1000C380, 0x1003C0C4, None, "h"),
    ("SetSystemInfo", 0x1000B9A0, 0x1003CB28, 0x10015B8C, "i"),
    ("SetSampleInfo", 0x1000C2E8, 0x1003C9F0, 0x1001515C, "hs"),
    ("Stop", 0x10019C34, 0x1003CAE0, 0x10015684, "h"),
    ("Pause", 0x1001A60C, 0x1003C9AC, 0x10014E00, "h"),
    ("Restart", 0x1001A748, 0x1003C964, 0x10014FAC, "h"),
    ("Start", 0x10027EB4, 0x1003CA34, 0x100154E4, "h"),
    ("UpdateAllVolumes", 0x10027A64, 0x1003CB08, 0x10016050, ""),
    ("Process", 0x100280A4, 0x1003CB48, 0x10015AE4, ""),
    ("StopAll", 0x10028270, 0x1003CA7C, 0x10015980, ""),
    ("PauseAll", 0x10028404, 0x1003C984, 0x10014F08, ""),
    ("RestartAll", 0x1002852C, 0x1003C928, 0x100150B4, ""),
]

# the host's operation numbers (enum Op in the unit file)
(K_CTOR, K_DTOR, K_MODIFY, K_INIT, K_CLOSE, K_GETSYS, K_STOPREL, K_GETSAMP, K_NEWSAMP, K_NEWCTL, K_SYSCMD, K_GETMOD,
 K_GETSTORE) = range(13)
K_GENERATE, K_CLOSEMOD, K_CIINIT, K_STDCTOR, K_COPYCTOR, K_STDCOPY, K_ACCESSCTOR, K_COMPCTOR = range(20, 28)
NODES = 6
VT = {}


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def lbl(L, address):
    return L.lab(address) & MASK


def words_at(e, address, count):
    return list(struct.unpack(">%dI" % count, e.read(address, 4 * count)))


# ---------------------------------------------------------------- the PowerPC side: fakes

def rec_info(L, e, name, address):
    w = words_at(e, address, 19)
    L.rec(name + ".a", w[:10])
    L.rec(name + ".b", w[10:])


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


def build_fakes(e):
    """The recording callees outside the unit."""
    def rec(name, *fields):
        def stub(e):
            L = e.cl
            ints = []
            for i, kind in enumerate(fields):
                v = e.r(3 + i)
                ints.append(L.lab(v) if kind == "l" else v & MASK if kind == "u" else v & 0xFF)
            L.rec(name, ints)
        return stub

    def answer(name, key, *fields):
        """A callee that answers the queue `key` in r3."""
        base = rec(name, *fields)

        def stub(e):
            base(e)
            e.set_r(3, T.u32(e.cl.ret(key, 0)))
        return stub

    def error(name, key, *fields):
        """A callee returning TbError through the buffer in r3 (this in r4)."""
        def stub(e):
            L = e.cl
            ints = []
            for i, kind in enumerate(fields):
                v = e.r(4 + i)
                ints.append(L.lab(v) if kind == "l" else v & MASK)
            L.rec(name, ints)
            e.write(e.r(3), pack([L.ret(key, 0)]))
        return stub

    # the store
    def store_ctor(e):
        L = e.cl
        L.rec("Store.ctor", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK))

    def store_dtor(e):
        L, this = e.cl, e.r(3)
        L.rec("Store.~", (L.lab(this),))
        if T.s16(e.r(4)) > 0:
            stub_delete(e)

    def store_index(e):
        L = e.cl
        L.rec("Store.index", (L.lab(e.r(3)), e.u32(e.r(4))))
        e.set_r(3, e.w.channel if L.ret("Store.index", 0) != 0 else 0)

    def new_sample(e):
        L = e.cl
        L.rec("Store.CreateNewSample", (L.lab(e.r(3)), e.r(4) & MASK, L.lab(e.r(5))))
        e.write(e.r(6), pack([L.ret("Store.out", 0)]))
        e.set_r(3, T.u32(L.ret("Store.sample", 0)))

    def new_control(e):
        L = e.cl
        L.rec("Store.CreateNewControl", (L.lab(e.r(3)), e.r(4) & MASK, L.lab(e.r(5))))
        e.set_r(3, T.u32(L.ret("Store.control", 0)))

    def sprockets_ctor(e):
        L, this = e.cl, e.r(3)
        e.write(this, pack([VT["module"]]))
        L.rec("CSprocketsModule", (L.lab(this),))

    # the commands' Execute bodies
    def exec_info(e):
        L = e.cl
        L.rec("SetSystemInfo.Execute", (L.lab(e.r(5)),))
        rec_info(L, e, "SetSystemInfo.info", e.u32(e.r(4) + 4))
        e.write(e.r(3), pack([L.ret("Exec.SetSystemInfo", 0)]))

    def exec_sample(e):
        L, cmd = e.cl, e.r(4)
        L.rec("SetSampleInfo.Execute", (L.lab(e.r(5)), e.u32(cmd + 4), L.lab(e.u32(cmd + 8))))
        e.write(e.r(3), pack([L.ret("Exec.SetSampleInfo", 0)]))

    def exec_handle(name):
        def stub(e):
            L = e.cl
            L.rec(name + ".Execute", (L.lab(e.r(5)), e.u32(e.r(4) + 4)))
            e.write(e.r(3), pack([0]))
        return stub

    def exec_plain(name):
        def stub(e):
            e.cl.rec(name + ".Execute", (e.cl.lab(e.r(5)), 0))
            e.write(e.r(3), pack([0]))
        return stub

    internals = {
        STORE_CTOR: store_ctor,
        STORE_INIT: error("Store.Init", "Store.Init", "l", "u"),
        STORE_RELEASE_ALL: rec("Store.ReleaseAllPipes", "l"),
        STORE_DTOR: store_dtor,
        STORE_USED: answer("Store.GetNumberOfUsedChannels", "Store.Used", "l"),
        STORE_MAX: answer("Store.GetMaxNumberOfChannels", "Store.Max", "l"),
        STORE_INDEX: store_index,
        STORE_NEW_SAMPLE: new_sample,
        STORE_NEW_CONTROL: new_control,
        CH_GET_INDEX: answer("Channel.GetIndex", "Channel.index", "l"),
        CH_RELEASE_PIPE: answer("Channel.ReleasePipe", "Channel.release", "l", "b"),
        CH_VOLUME: answer("Channel.GetActualVolume", "Channel.volume", "l"),
        CH_PRIORITY: answer("Channel.GetPriority", "Channel.priority", "l"),
        SPROCKETS_CTOR: sprockets_ctor,
        0x10015B8C: exec_info,
        0x1001515C: exec_sample,
    }
    for name, ctor, vt, execute, kinds in COMMANDS:
        if execute is None or name in ("SetSystemInfo", "SetSampleInfo"):
            continue
        internals[execute] = exec_handle(name) if "h" in kinds else exec_plain(name)
    for address, fn in internals.items():
        T.stub_internal(e, address, fn)

    # the TbRefRelease imports of the bullfrog library (src/TbRefRelease.cpp): the count is the word after the vptr
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
    e.stub("__nw__FUl", stub_new)
    e.stub("__dl__FPv", stub_delete)

    def memcpy(e):
        d, s, n = e.r(3), e.r(4), e.r(5)
        if n:
            e.write(d, e.read(s, n))
    e.stub("memcpy", memcpy)


def code_with_hook(e, hook, prefix, target):
    """A fake slot function: the hook runs on entry (it records), then `prefix` instructions and a jump to `target` (a real
    function) with r3 and r4 as they were. Returns the address of a TVector for a vtable word."""
    words = [0x60000000] + list(prefix) + [0x3D800000 | (target >> 16), 0x618C0000 | (target & 0xFFFF), 0x7D8903A6, 0x4E800420]
    code = T.persist_alloc(e, 4 * len(words), pack(words), alignment=8)
    tvec = T.persist_alloc(e, 8, struct.pack(">II", code, e.toc), alignment=8)
    e.uc.hook_add(T.UC_HOOK_CODE, lambda uc, address, size, user: hook(e), begin=code, end=code)
    return tvec


def build_tables(e):
    fake = lambda fn: T.fake_function(e, fn)

    def rec_this(name, fields=()):
        def stub(e):
            L = e.cl
            L.rec(name, [L.lab(e.r(3))] + [e.r(4 + i) & MASK for i in range(len(fields))])
        return stub

    # the module: the slots the core uses record; every other slot records its number
    def m_init(e):
        L = e.cl
        L.rec("Module.Init", (L.lab(e.r(4)),))
        rec_info(L, e, "Module.Init.info", e.r(5))
        e.write(e.r(3), pack([L.ret("Module.Init", 0)]))

    def m_stop_channel(e):
        L = e.cl
        L.rec("Module.StopChannel", (L.lab(e.r(4)), e.r(5) & MASK))
        e.write(e.r(3), pack([L.ret("Module.StopChannel", 0)]))

    def m_stop_all(e):
        L = e.cl
        L.rec("Module.StopAll", (L.lab(e.r(4)),))
        e.write(e.r(3), pack([L.ret("Module.StopAll", 0)]))

    def m_supports(e):
        L = e.cl
        L.rec("Module.Supports", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("Module.Supports", 1))

    def m_close(e):
        L = e.cl
        L.rec("Module.Close", (L.lab(e.r(3)),))
        if e.w.owner and L.ret("Module.Close.clear", 0):
            e.write(e.w.owner, pack([0]))

    def m_dtor_hook(e):
        L = e.cl
        L.rec("Module.~", (L.lab(e.r(3)),))

    words = [0, 0] * 1 + [0] * 36
    for slot in range(0x0C, 0x98, 4):
        words[slot // 4] = fake(rec_this("Module.slot%02x" % slot))
    words[0x08 // 4] = code_with_hook(e, m_dtor_hook, [], A["module_dtor"])
    words[0x20 // 4] = fake(m_init)
    words[0x24 // 4] = fake(m_close)
    words[0x2C // 4] = fake(m_stop_channel)
    words[0x30 // 4] = fake(m_stop_all)
    words[0x48 // 4] = fake(m_supports)
    VT["module"] = T.persist_alloc(e, 4 * len(words), pack(words))

    # the test command
    def c_exec(e):
        L = e.cl
        L.rec("Command.Execute", (L.lab(e.r(4)), L.lab(e.r(5))))
        e.write(e.r(3), pack([L.ret("Exec", 0)]))
    VT["cmd"] = T.persist_alloc(e, 12, pack([0, 0, fake(c_exec)]))

    # the chain component that records every slot (its destructors record, then run the real ~CAudioComponent)
    def f_buf(name, nargs, handle=False):
        def stub(e):
            L = e.cl
            args = [L.lab(e.r(4))]
            if handle:
                args.append(e.r(5) & MASK)
                args.append(L.lab(e.r(6)))
            else:
                args.append(L.lab(e.r(5)))
            L.rec(name, args[:nargs])
            e.write(e.r(3), pack([L.ret(name, 0)]))
        return stub

    def f_ptr(name, handle=True):
        def stub(e):
            L = e.cl
            L.rec(name, [L.lab(e.r(3))] + ([e.r(4) & MASK, L.lab(e.r(5))] if handle else []))
            e.set_r(3, T.u32(L.ret(name, 0)))
        return stub

    def f_init(e):
        L = e.cl
        L.rec("Fake.Init", (L.lab(e.r(4)),))
        rec_info(L, e, "Fake.Init.info", e.r(5))
        e.write(e.r(3), pack([L.ret("Fake.Init", 0)]))

    def f_dtor(e):
        e.cl.rec("Fake.~", (e.cl.lab(e.r(3)),))

    def f_dtor_sub(e):
        e.cl.rec("Fake.~", (e.cl.lab(e.r(3) - 4),))

    words = [0] * 17
    words[2] = fake(f_buf("Fake.GetSystemInfo", 2))
    words[3] = fake(f_buf("Fake.GetSampleInfo", 3, True))
    words[4] = fake(f_ptr("Fake.NewSample"))
    words[5] = fake(f_ptr("Fake.NewControl"))
    words[6] = fake(f_buf("Fake.SystemCommand", 2))
    words[7] = fake(f_ptr("Fake.GetSystemModule", False))
    words[8] = fake(f_ptr("Fake.GetSampleStore", False))
    words[14] = fake(f_init)
    words[15] = code_with_hook(e, f_dtor, [], A["comp_dtor"])
    VT["fake"] = T.persist_alloc(e, 4 * len(words), pack(words))
    words = [0, 0, e.u32(COMP_SUB_VT + 8), e.u32(COMP_SUB_VT + 0xC), 0]
    words[4] = code_with_hook(e, f_dtor_sub, [0x3863FFFC], A["comp_dtor"])
    VT["fake_sub"] = T.persist_alloc(e, 4 * len(words), pack(words))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    build_fakes(e)
    build_tables(e)
    lib = runner.lib
    assert lib.tpw_sc_sizes(0) == 0x18, "the host's size must be the Mac's"
    # The real store (w79c's header) is 0x10 on a 32-bit target (its static_assert) and larger on the 64-bit host.
    assert lib.tpw_sc_sizes(1) >= 0x10, "the host's store is at least the Mac's"
    lib.tpw_sc_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p]
    lib.tpw_sc_copyinit_dtor.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_sc_command.argtypes = [C.c_int32, C.c_uint32, C.c_void_p]
    lib.tpw_sc_chain_op.argtypes = [C.c_int32] * 5 + [C.c_void_p]
    lib.tpw_sc_chain_slot.argtypes = [C.c_int32, C.c_void_p]


# ---------------------------------------------------------------- the core, the factory and the copiers

def sys_words(r=None, **kw):
    """The 19 words of a system info (Mac offsets 0..0x48)."""
    w = [0xA5A5A5A5] * 19 if r is None else [r.getrandbits(32) for _ in range(19)]
    for name, value in kw.items():
        w[SYS[name]] = value
    return w


SYS = dict(f0=0, f4=1, f8=2, fc=3, f10=4, max=5, mask=6, use=7, hwnd=8, guid=9, cls=10, pos=11, front=12, top=13, vel=14,
           spk=15, f40=16, playing=17, hw=18)


def core_case(op, r=None, **kw):
    c = dict(op=op, hm=False, hs=False, handle=0, flag=0, chan=0, count=0, si=sys_words(), ii=[FILL] * 5, q={})
    if r is not None:
        c.update(randomise(r, op))
    c.update(kw)
    return c


QUEUE_ERRORS = [0, -1, 1, 5, -2, 0x7FFFFFFF, -0x80000000]
CORE_QUEUES = ("Module.Init", "Module.StopAll", "Module.StopChannel", "Store.Init", "Exec", "Exec.SetSystemInfo",
               "Exec.SetSampleInfo")


def randomise(r, op):
    handle = r.choice([0, 1, 0x1234, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32)])
    bits4 = r.choice([0, 0x100, 0xFFFFFEFF, 0xFFFFFFFF, r.getrandbits(32)])
    bits8 = r.choice([0, 0x80, 0x40, 0x200, 0xC0, 0x2C0, 0xFFFFFD3F, 0xFFFFFFFF, r.getrandbits(32)])
    si = sys_words(r)
    si[SYS["f4"]] = bits4
    si[SYS["f8"]] = bits8
    si[SYS["mask"]] = r.choice([0, 0x80, 0x40, 0x200, r.getrandbits(32)])
    si[SYS["use"]] = r.choice([0, 1, 10, 0x7FFFFFFF, 0xFFFFFFFF, r.getrandbits(32)])
    ii = [r.getrandbits(32) for _ in range(5)]
    ii[0] = r.choice([0, 0x100, r.getrandbits(32), r.getrandbits(32) & ~0x100])
    ii[2] = r.choice([0, 2, 0x400, 0x402, 0xFFFFFFFD, 0xFFFFFBFF, r.getrandbits(32)])
    q = {}
    for key in CORE_QUEUES:
        q[key] = [r.choice(QUEUE_ERRORS)]
    q["Module.Supports"] = [r.randrange(2)]
    q["Store.Used"] = [r.getrandbits(32)]
    q["Store.Max"] = [r.getrandbits(32)]
    q["Store.index"] = [r.randrange(2)]
    q["Store.sample"] = [r.choice([0, r.getrandbits(32), 0xFFFFFFFF])]
    q["Store.control"] = [r.choice([0, r.getrandbits(32)])]
    q["Store.out"] = [r.getrandbits(32)]
    q["Channel.index"] = [r.choice([0, 1, 0x3F, r.getrandbits(32)])]
    q["Channel.release"] = [r.randrange(2)]
    q["Channel.volume"] = [r.getrandbits(32)]
    q["Channel.priority"] = [r.getrandbits(32)]
    c = dict(hm=bool(r.getrandbits(1)), hs=bool(r.getrandbits(1)), handle=handle, flag=r.choice([0, 1]),
             chan=r.randrange(2), count=r.getrandbits(32), si=si, ii=ii, q=q)
    if op == K_DTOR:
        c["flag"] = r.choice(DTOR_FLAGS)
    return sanitize(c, op)


DTOR_FLAGS = [0, 1, 2, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, -0x8000]


def sanitize(c, op):
    """Cases the Mac code cannot run (it calls through a NULL store)."""
    c["op"] = op
    if op in (K_NEWSAMP, K_NEWCTL) or (op == K_GETSAMP and c["handle"] != 0):
        c["hs"] = True
    return c


def core_ppc(e, c):
    op = c["op"]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    w = e.w = type("W", (), {})()
    w.owner = 0   # the word Module.Close clears when the case's queue "Module.Close.clear" says so (Close and CloseModule only)
    e.write(S_SPEAKER, pack([0]))
    e.write(S_COUNT, pack([c["count"]]))
    info = e.alloc(0x4C, pack(c["si"]))
    samp = e.alloc(0x90, b"\xa5" * 0x90)
    for offset, value in zip((0x0C, 0x14, 0x18, 0x1C, 0x28), c["ii"]):
        e.write(samp + offset, pack([value]))
    w.channel = e.alloc(0x60, b"\xa5" * 0x60)
    cmd = e.alloc(8, pack([VT["cmd"], FILL]))
    access = e.alloc(16, b"\xa5" * 16)
    L.labels.update({w.channel: 4, samp: 5, info: 6, cmd: 7, access: 9})
    handle = c["handle"] & MASK
    flag = c["flag"] & MASK
    obj = 0
    x = [0] * 8
    result = 0
    core_ops = op <= K_GETSTORE
    if core_ops:
        obj = e.alloc(0x24, b"\xa5" * 0x24)
        L.labels[obj] = 1
        if op == K_CTOR:
            e.write(S_COPY, pack([0]))
        else:
            T.call_ppc(e, A["core_ctor"], [obj])
            L.labels[e.u32(S_COPY)] = 8
            if c["hm"]:
                module = e.alloc(0x18, pack([VT["module"]]) + b"\xa5" * 0x14)
                L.labels[module] = 2
                e.write(obj + 0x18, pack([module]))
            if c["hs"]:
                store = e.alloc(0x10, b"\xa5" * 0x10)
                L.labels[store] = 3
                e.write(obj + 0x1C, pack([store]))
    buf = e.alloc(4, pack([FILL]))
    modref = e.alloc(4, pack([0]))
    store_arg = 0
    if op in (K_GENERATE, K_CLOSEMOD):
        if c["hm"]:
            module = e.alloc(0x18, pack([VT["module"]]) + b"\xa5" * 0x14)
            L.labels[module] = 2
            e.write(modref, pack([module]))
        store_arg = e.alloc(0x10, b"\xa5" * 0x10)
        L.labels[store_arg] = 3
    if op == K_CIINIT:
        copier = 0
        if c["hm"]:
            copier = e.alloc(8, b"\xa5" * 8)
            T.call_ppc(e, A["std_ctor"], [copier])
            L.labels[copier] = 8
        e.write(S_COPY, pack([copier]))
    L.track = True
    if op == K_CTOR:
        T.call_ppc(e, A["core_ctor"], [obj])
        copier = e.u32(S_COPY)
        ok = e.u32(obj) == CORE_VT and e.u32(obj + 4) == CORE_SUB_VT
        for slot, address in CORE_SLOTS.items():
            ok = ok and e.u32(e.u32(CORE_VT + slot)) == address
        ok = ok and e.u32(e.u32(CORE_VT + 0x34)) == 0x1000C578
        x = [lbl(L, copier), int(e.u32(copier) == STD_COPY_VT), e.u32(obj + 0x20), e.u32(obj + 0x14), e.u32(obj + 8),
             lbl(L, e.u32(obj + 0x0C)), lbl(L, e.u32(obj + 0x10)), int(ok)]
    elif op == K_DTOR:
        T.call_ppc(e, A["core_dtor"], [obj, flag])
        x[0] = lbl(L, e.u32(S_COPY))
    elif op == K_MODIFY:
        T.call_ppc(e, A["modify"], [obj, info])
    elif op in (K_INIT, K_GETSYS):
        T.call_ppc(e, A["init" if op == K_INIT else "getsys"], [buf, obj, info])
        result = e.u32(buf)
    elif op == K_CLOSE:
        w.owner = obj + 0x18
        T.call_ppc(e, A["close"], [obj])
    elif op == K_STOPREL:
        T.call_ppc(e, A["stoprel"], [buf, obj, w.channel if c["chan"] else 0, flag])
        result = e.u32(buf)
    elif op == K_GETSAMP:
        T.call_ppc(e, A["getsamp"], [buf, obj, handle, samp])
        result = e.u32(buf)
    elif op in (K_NEWSAMP, K_NEWCTL):
        result = T.call_ppc(e, A["newsamp" if op == K_NEWSAMP else "newctl"], [obj, handle, samp])[0]
    elif op == K_SYSCMD:
        T.call_ppc(e, A["syscmd"], [buf, obj, cmd])
        result = e.u32(buf)
    elif op in (K_GETMOD, K_GETSTORE):
        result = lbl(L, T.call_ppc(e, A["getmod" if op == K_GETMOD else "getstore"], [obj])[0])
    elif op in (K_GENERATE, K_CLOSEMOD):
        factory = e.alloc(4, b"\xa5" * 4)
        if op == K_GENERATE:
            T.call_ppc(e, A["generate"], [buf, factory, modref, info, store_arg])
            result = e.u32(buf)
        else:
            w.owner = modref
            T.call_ppc(e, A["closemod"], [factory, modref])
        x[0] = lbl(L, e.u32(modref))
    elif op == K_CIINIT:
        block = e.alloc(4, b"\xa5" * 4)
        T.call_ppc(e, A["ci_ctor"], [block])
        copier = e.u32(S_COPY)
        x = [lbl(L, copier), int(e.u32(copier) == STD_COPY_VT), e.u32(block), 0, 0, 0, 0, 0]
    elif op == K_STDCTOR:
        block = e.alloc(8, b"\xa5" * 8)
        T.call_ppc(e, A["std_ctor"], [block])
        x[0:2] = [int(e.u32(block) == STD_COPY_VT), int(e.u32(block + 4) == FILL)]
    elif op == K_COPYCTOR:
        block = e.alloc(8, b"\xa5" * 8)
        T.call_ppc(e, A["copy_ctor"], [block])
        x[0] = int(e.u32(block) == COPY_VT and e.u32(block + 4) == FILL)
    elif op == K_ACCESSCTOR:
        block = e.alloc(8, b"\xa5" * 8)
        T.call_ppc(e, A["access_ctor"], [block])
        x[0] = int(e.u32(block) == ACCESS_VT and e.u32(block + 4) == FILL)
    elif op == K_COMPCTOR:
        block = e.alloc(0x20, b"\xa5" * 0x20)
        T.call_ppc(e, A["comp_ctor"], [block])
        ok = e.u32(block) == COMP_VT and e.u32(block + 4) == COMP_SUB_VT and e.u32(block + 0x18) == FILL
        for slot, address in COMP_SLOTS.items():
            ok = ok and e.u32(e.u32(COMP_VT + slot)) == address
        x = [lbl(L, e.u32(block + 0x0C)), lbl(L, e.u32(block + 0x10)), e.u32(block + 0x14), e.u32(block + 8), int(ok), 0, 0, 0]
    elif op == K_STDCOPY:
        copier = e.alloc(8, b"\xa5" * 8)
        T.call_ppc(e, A["std_ctor"], [copier])
        dest = e.alloc(48, b"\xee" * 48)
        seed, source = handle, bytearray()
        for _ in range(48):
            seed = (seed * 1664525 + 1013904223) & MASK
            source.append(seed >> 24)
        src = e.alloc(48, bytes(source))
        code = e.u32(e.u32(e.u32(copier) + 8))
        assert code == A["std_copy"]
        T.call_ppc(e, code, [copier, dest + c["count"], src, flag])
        x = e.read(dest, 48)
    L.track = False

    out = dict(r=result & MASK, si=tuple(words_at(e, info, 19)), ii=tuple(e.u32(samp + o) for o in (0x0C, 0x14, 0x18, 0x1C, 0x28)),
               mod=0, sto=0, x=tuple(x) if op != K_STDCOPY else x, log=L.log)
    if core_ops and op != K_DTOR:
        out["mod"] = lbl(L, e.u32(obj + 0x18))
        out["sto"] = lbl(L, e.u32(obj + 0x1C))
    return out


def core_host(lib, c):
    op = c["op"]
    flag = T.s16(c["flag"]) if op == K_DTOR else c["flag"]
    p = [int(c["hm"]), int(c["hs"]), c["handle"], flag, c["chan"], c["count"], 0, 0] + list(c["si"]) + list(c["ii"])
    p += [0] * (40 - len(p))
    params = (C.c_uint32 * 40)(*[v & MASK for v in p])
    out = (C.c_uint32 * 64)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_sc_run(op, params, out)
    log = CL.host_end(lib)
    x = tuple(out[27:35])
    if op == K_STDCOPY:
        x = struct.pack("<12I", *out[27:39])
    return dict(r=out[0], si=tuple(out[1:20]), ii=tuple(out[20:25]), mod=out[25], sto=out[26], x=x, log=log)


def si_variants(**kw):
    return sys_words(**kw)


def core_edges(op):
    def edges():
        cases = []
        if op == K_CTOR:
            cases.append(core_case(op))
        if op == K_DTOR:
            for hm in (False, True):
                for hs in (False, True):
                    for flag in DTOR_FLAGS:
                        cases.append(core_case(op, hm=hm, hs=hs, flag=flag, q={"Module.StopAll": [0]}))
            cases.append(core_case(op, hm=True, hs=True, flag=1, q={"Module.StopAll": [-1]}))
        if op in (K_CLOSE, K_GETMOD, K_GETSTORE):
            for hm in (False, True):
                for hs in (False, True):
                    cases.append(core_case(op, hm=hm, hs=hs))
        if op == K_CLOSE:
            # the module clears the core's module pointer from inside Close: the second NULL test (0x1000bbd4) skips the delete
            for hs in (False, True):
                cases.append(core_case(op, hm=True, hs=hs, q={"Module.Close.clear": [1]}))
        if op == K_MODIFY:
            for use in (0, 1, 10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                for mask in (0, 0x80, 0x40, 0xFFFFFFFF):
                    cases.append(core_case(op, si=sys_words(use=use, mask=mask, max=0x1234)))
            cases.append(core_case(op))
        if op == K_INIT:
            for hm in (False, True):
                for hs in (False, True):
                    for store_err in (0, -1, 3, -0x80000000):
                        for module_err in (0, -1, 4, 0x7FFFFFFF):
                            for exec_err in (0, -1, 6):
                                cases.append(core_case(
                                    op, hm=hm, hs=hs, si=sys_words(use=10, max=5, mask=0x40, f4=0x100, f8=0x80),
                                    q={"Store.Init": [store_err], "Module.Init": [module_err], "Exec.SetSystemInfo": [exec_err]}))
            for use in (0, 1, 0x7FFFFFFF, 0xFFFFFFFF):
                cases.append(core_case(op, si=sys_words(use=use, max=0x77, mask=0), q={"Store.Init": [0]}))
        if op == K_GETSYS:
            for hm in (False, True):
                for hs in (False, True):
                    for f4 in (0x100, 0, 0xFFFFFEFF, 0xFFFFFFFF, 0x1FF):
                        for f8 in range(0, 0x400, 1):
                            if f8 & ~0x2C0 and f8 not in (0x3FF, 0x100, 0x1):
                                continue
                            for supports in (0, 1):
                                cases.append(core_case(
                                    op, hm=hm, hs=hs, count=0x1234 + f8,
                                    si=sys_words(f4=f4, f8=f8, max=0x11111111, mask=0x20, use=0x22222222, playing=0x33333333),
                                    q={"Module.Supports": [supports], "Store.Max": [0xA1A1], "Store.Used": [0xB2B2]}))
            for count in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                cases.append(core_case(op, count=count, si=sys_words(f8=0x200)))
        if op == K_STOPREL:
            for hm in (False, True):
                for chan in (0, 1):
                    for release in (0, 1):
                        for stop in (0, -1, 1, 0x7FFFFFFF, -0x80000000):
                            cases.append(core_case(op, hm=hm, chan=chan, flag=release, q={
                                "Module.StopChannel": [stop], "Channel.index": [0x2A], "Channel.release": [release ^ 1]}))
            cases.append(core_case(op, hm=True, chan=1, flag=1, q={"Channel.index": [0xFFFFFFFF]}))
        if op == K_GETSAMP:
            for handle in (0, 1, 0x1234, 0xFFFFFFFF, 0x80000000):
                for hs in (False, True):
                    if handle != 0 and not hs:
                        continue
                    for index in (0, 1):
                        for f0c in (0, 0x100, 0xFFFFFEFF, 0xFFFFFFFF):
                            for f18 in (0, 2, 0x400, 0x402, 0xFFFFFFFD, 0xFFFFFBFF, 0xFFFFFFFF):
                                cases.append(core_case(
                                    op, hs=hs, handle=handle, ii=[f0c, 0x10, f18, 0x77, 0x88],
                                    q={"Store.index": [index], "Channel.volume": [0xCAFE], "Channel.priority": [0xBEEF]}))
        if op in (K_NEWSAMP, K_NEWCTL):
            for handle in (0, 5, 0xFFFFFFFF):
                for sample in (0, 1, 0x12345678, 0xFFFFFFFF):
                    for exec_err in (0, -1):
                        cases.append(core_case(op, hs=True, handle=handle, q={
                            "Store.sample": [sample], "Store.control": [sample], "Store.out": [0x99], "Exec.SetSampleInfo": [exec_err]}))
        if op == K_SYSCMD:
            for hm in (False, True):
                for exec_err in (0, -1, 7, -0x80000000):
                    cases.append(core_case(op, hm=hm, q={"Exec": [exec_err]}))
        if op == K_GENERATE:
            for hm in (False, True):
                for err in (0, -1, 1, 0x7FFFFFFF, -0x80000000):
                    cases.append(core_case(op, hm=hm, hs=True, q={"Module.Init": [err]}))
        if op == K_CLOSEMOD:
            for hm in (False, True):
                cases.append(core_case(op, hm=hm))
            # the module clears the caller's pointer from inside Close: the second NULL test (0x10007158) skips the delete
            cases.append(core_case(op, hm=True, q={"Module.Close.clear": [1]}))
        if op == K_CIINIT:
            for hm in (False, True):
                cases.append(core_case(op, hm=hm))
        if op in (K_STDCTOR, K_COPYCTOR, K_ACCESSCTOR, K_COMPCTOR):
            cases.append(core_case(op))
        if op == K_STDCOPY:
            for count in (0, 1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 40):
                for offset in (0, 1, 3, 8):
                    if offset + count <= 48:
                        cases.append(core_case(op, handle=count * 7 + offset, count=offset, flag=count))
        return cases
    return edges


def core_random(op):
    def rnd(r):
        c = core_case(op, r)
        if op in (K_GENERATE, K_CLOSEMOD, K_CIINIT):
            c["hs"] = True
        if op == K_STDCOPY:
            c.update(handle=r.getrandbits(32), count=r.randrange(9), flag=r.randrange(41))
        return c
    return rnd


def copyinit_dtor_ppc(e, c):
    L = e.cl = CL.PpcLog({}, {})
    L.track = False
    block = e.alloc(4, b"\xa5" * 4)
    T.call_ppc(e, A["ci_ctor"], [block])
    L.labels[block] = 1
    L.labels[e.u32(S_COPY)] = 8
    T.call_ppc(e, A["ci_dtor"], [block, c["flag"] & MASK])
    return dict(x=(lbl(L, e.u32(S_COPY)), int(e.u32(S_COPY) != 0)), log=L.log)


def copyinit_dtor_host(lib, c):
    out = (C.c_uint32 * 8)()
    CL.host_begin(lib, {}, {})
    lib.tpw_sc_copyinit_dtor(T.s16(c["flag"]), out)
    return dict(x=tuple(out[:2]), log=CL.host_end(lib))


# ---------------------------------------------------------------- the command constructors

def command_ppc(e, c):
    name, ctor, vt, execute, kinds = COMMANDS[c["kind"]]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    info = e.alloc(0x4C, pack([0x100 + i for i in range(19)]))
    samp = e.alloc(0x90, b"\xa5" * 0x90)
    access = e.alloc(16, b"\xa5" * 16)
    L.labels.update({samp: 5, info: 6, access: 9})
    obj = e.alloc(64, b"\xa5" * 64)
    args = [obj]
    if "h" in kinds:
        args.append(c["handle"])
    if "i" in kinds:
        args.append(info)
    if "s" in kinds:
        args.append(samp)
    T.call_ppc(e, ctor, args)
    ok = int(e.u32(obj) == vt)
    out = [0, 0, 0, 1]
    if "h" in kinds:
        out[0] = e.u32(obj + 4)
    if "i" in kinds:
        out[1] = lbl(L, e.u32(obj + 4))
    if "s" in kinds:
        out[1] = lbl(L, e.u32(obj + 8))
    if execute is not None:
        code = e.u32(e.u32(vt + 8))
        ok = ok and int(code == execute)
        buf = e.alloc(4, pack([FILL]))
        T.call_ppc(e, code, [buf, obj, access])
        out[2] = e.u32(buf)
    out[3] = ok
    return dict(x=tuple(out), log=L.log)


def command_host(lib, c):
    out = (C.c_uint32 * 8)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_sc_command(c["kind"], c["handle"], out)
    return dict(x=tuple(out[:4]), log=CL.host_end(lib))


def command_case(r=None, kind=0, handle=0, **q):
    if r is not None:
        kind = r.randrange(len(COMMANDS))
        handle = r.choice([0, 1, 0x1234, 0xFFFFFFFF, r.getrandbits(32)])
        q = {"Exec.SetSystemInfo": [r.choice(QUEUE_ERRORS)], "Exec.SetSampleInfo": [r.choice(QUEUE_ERRORS)]}
    return dict(kind=kind, handle=handle, q={k: v if isinstance(v, list) else [v] for k, v in q.items()})


def command_edges():
    return [command_case(kind=k, handle=h) for k in range(len(COMMANDS)) for h in (0, 1, 0x7FFFFFFF, 0xFFFFFFFF)] + [
        command_case(kind=2, **{"Exec.SetSystemInfo": err}) for err in QUEUE_ERRORS] + [
        command_case(kind=3, handle=9, **{"Exec.SetSampleInfo": err}) for err in QUEUE_ERRORS]


# ---------------------------------------------------------------- the component chain

class Chain:
    """What the PowerPC side knows of the nodes."""


def slot_state(e, w, s):
    out = [0] * 6
    node = w.nodes[s]
    if node == 0:
        return out
    L = e.cl
    if w.destroyed[s] or L.lab(node) <= 0:
        out[0] = 1
        return out
    out[0], out[1] = 2, w.kinds[s]
    out[2], out[3] = L.lab(e.u32(node + 0x0C)), L.lab(e.u32(node + 0x10))
    out[4], out[5] = e.u32(node + 0x14), e.u32(node + 8)
    return out


def slot_of(w, address):
    for s in range(NODES):
        if w.nodes[s] == address and address != 0:
            return s
    return None


def alive(e, w, s):
    return s is not None and w.nodes[s] != 0 and e.cl.lab(w.nodes[s]) > 0 and not w.destroyed[s]


def walk(e, w, s, stop_at_fake, null_ok=False):
    """The slots from s along mNext, or None when a node is dead, unknown or repeated (or the walk is too long). With
    stop_at_fake the walk ends at a fake node, and a real node with no next is an error unless null_ok."""
    seen = []
    while s is not None:
        if not alive(e, w, s) or s in seen or len(seen) > 8:
            return None
        seen.append(s)
        if stop_at_fake and w.kinds[s] == 2:
            return seen
        nxt = e.u32(w.nodes[s] + 0x0C)
        if nxt == 0:
            return seen if (null_ok or not stop_at_fake) else None
        s = slot_of(w, nxt)
        if s is None:
            return None
    return seen


def neighbours_ok(e, w, s):
    for offset in (0x0C, 0x10):
        other = e.u32(w.nodes[s] + offset)
        if other != 0 and not alive(e, w, slot_of(w, other)):
            return False
    return True


def safe(e, w, op, a, b):
    if op == 0:
        return 0 <= a < NODES and w.nodes[a] == 0
    if not (0 <= a < NODES) or not alive(e, w, a):
        return False
    if op == 1:
        if b >= NODES or a == b or (b >= 0 and not alive(e, w, b)):
            return False
        first = walk(e, w, a, False)
        second = walk(e, w, b, False) if b >= 0 else []
        return first is not None and second is not None and not set(first) & set(second)
    if op == 2:
        path = walk(e, w, a, True, True)
        return path is not None and all(neighbours_ok(e, w, s) for s in path)
    if op == 3:
        return neighbours_ok(e, w, a)
    if op == 4:
        return walk(e, w, a, False) is not None
    if op == 5 or op == 8:
        return True
    if op == 6:
        return walk(e, w, a, True) is not None
    if op in (7, 9):
        if op == 7 and e.u32(w.nodes[a] + 8) > 1:
            return True
        return walk(e, w, a, False) is not None
    return False


def sub_slot(e, node, slot):
    """The code of a slot of the TbRefRelease sub-table of a node."""
    return e.u32(e.u32(e.u32(node + 4) + slot))


def prim_slot(e, node, slot):
    return e.u32(e.u32(e.u32(node) + slot))


def chain_op(e, w, op, a, b, c):
    L = e.cl
    node = w.nodes[a] if 0 <= a < NODES else 0
    other = w.nodes[b] if 0 <= b < NODES else 0
    buf = e.alloc(4, pack([FILL]))
    if op == 0:
        block = e.alloc(0x18, b"\xa5" * 0x18)
        T.call_ppc(e, A["comp_ctor"], [block])
        if b == 2:
            e.write(block, pack([VT["fake"]]))
            e.write(block + 4, pack([VT["fake_sub"]]))
        w.nodes[a], w.kinds[a], w.destroyed[a] = block, b, False
        L.labels[block] = 11 + a
        return L.lab(block), 0
    if op == 1:
        T.call_ppc(e, A["chain"], [buf, node, other])
        return T.s32(e.u32(buf)), 0
    if op == 2:
        T.call_ppc(e, prim_slot(e, node, 0x38), [buf, node, w.info])
        return T.s32(e.u32(buf)), 0
    if op == 3:
        return T.call_ppc(e, A["unchain"], [node])[0], 0
    if op == 4:
        return L.lab(T.call_ppc(e, A["getcomp"], [node, b & MASK])[0]), 0
    if op == 5:
        e.write(node + 0x14, pack([b]))
        return 0, 0
    if op == 6:
        handle = c & MASK
        calls = {
            0: (0x08, [buf, node, w.info], True), 1: (0x0C, [buf, node, handle, w.samp], True),
            2: (0x10, [node, handle, w.samp], False), 3: (0x14, [node, handle, w.samp], False),
            4: (0x18, [buf, node, w.cmd], True), 5: (0x1C, [node], False), 6: (0x20, [node], False),
        }
        slot, args, hidden = calls[b]
        r3 = T.call_ppc(e, prim_slot(e, node, slot), args)[0]
        return (T.s32(e.u32(buf)) if hidden else r3), 0
    if op == 7:
        return T.call_ppc(e, sub_slot(e, node, 8), [node + 4])[0], 0
    if op == 8:
        return T.call_ppc(e, sub_slot(e, node, 0x0C), [node + 4])[0], 0
    if op == 9:
        T.call_ppc(e, prim_slot(e, node, 0x3C), [node, b & MASK])
        if T.s16(b) <= 0:
            w.destroyed[a] = True
        return 0, 0
    raise ValueError(op)


def chain_ppc(e, c):
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    w = e.w = type("W", (), {})()
    w.nodes, w.kinds, w.destroyed = [0] * NODES, [0] * NODES, [False] * NODES
    w.info = e.alloc(0x4C, pack([0x100 + i for i in range(19)]))
    w.samp = e.alloc(0x90, b"\xa5" * 0x90)
    w.cmd = e.alloc(8, pack([VT["cmd"], FILL]))
    L.labels.update({w.samp: 5, w.info: 6, w.cmd: 7})
    c["_skip"] = [0] * len(c["ops"])
    steps = []
    for index, (op, a, b, cc) in enumerate(c["ops"]):
        before = len(L.log)
        if not safe(e, w, op, a, b):
            c["_skip"][index] = 1
            result = -99
        else:
            try:
                result, _ = chain_op(e, w, op, a, b, cc)
            except RuntimeError as ex:
                for later in range(index, len(c["ops"])):
                    c["_skip"][later] = 1
                steps.append(dict(crash=str(ex)[:100], op=(op, a, b, cc)))
                break
        steps.append(dict(result=result, slots=[slot_state(e, w, s) for s in range(NODES)], log=list(L.log[before:])))
    return dict(steps=steps)


def chain_host(lib, c):
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_sc_chain_begin()
    steps = []
    for index, (op, a, b, cc) in enumerate(c["ops"]):
        before = C.c_int32.in_dll(lib, "tpw_cl_n").value
        out = (C.c_int64 * 4)()
        lib.tpw_sc_chain_op(op, a, T.s32(b & MASK), T.s32(cc & MASK), c["_skip"][index], out)
        after = C.c_int32.in_dll(lib, "tpw_cl_n").value
        log = (CL.TpwCallEvent * 4096).in_dll(lib, "tpw_cl_log")
        events = [(log[i].name.decode("latin-1"), tuple(log[i].ints[k] for k in range(log[i].nints)), ())
                  for i in range(before, min(after, 4096))]
        slots = []
        for s in range(NODES):
            state = (C.c_int64 * 6)()
            lib.tpw_sc_chain_slot(s, state)
            slots.append([int(v) for v in state])
        result = int(out[0])
        if op in (1, 2) or (op == 6 and c["ops"][index][2] in (0, 1, 4)):
            result = T.s32(result)
        steps.append(dict(result=result, slots=slots, log=events))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    return dict(steps=steps)


def chain_ppc_run(e, c):
    result = chain_ppc(e, c)
    for step in result["steps"]:
        step["log"] = [(name, tuple(ints), ()) for name, ints, texts in step["log"]]
    return result


FWD = ("GetSystemInfo", "GetSampleInfo", "NewSample", "NewControl", "SystemCommand", "GetSystemModule", "GetSampleStore")


def chain_queues(r=None):
    if r is None:
        return {"Fake.Init": [0], "Fake.GetSystemInfo": [0], "Fake.GetSampleInfo": [0], "Fake.SystemCommand": [0]}
    q = {"Fake.Init": [r.choice(QUEUE_ERRORS)]}
    for name in FWD:
        q["Fake." + name] = [r.choice(QUEUE_ERRORS) if name in ("GetSystemInfo", "GetSampleInfo", "SystemCommand")
                             else r.choice([0, 0x12345, r.getrandbits(32)])]
    return q


def script(ops, **q):
    queues = chain_queues()
    queues.update({k: v if isinstance(v, list) else [v] for k, v in q.items()})
    return dict(ops=ops, q=queues)


def new(slot, kind=1):
    return (0, slot, kind, 0)


def chain_edges():
    S = []
    # two nodes, chained both ways
    S.append(script([new(0), new(1, 2), (1, 0, 1, 0)]))
    S.append(script([new(0), new(1), (1, 0, 1, 0), (1, 1, 0, 0)]))
    S.append(script([new(0), (1, 0, -1, 0)]))
    S.append(script([new(0), new(1), (1, 0, 0, 0)]))
    # three, built tail first and head first, and an insertion in the middle
    S.append(script([new(0), new(1), new(2, 2), (1, 1, 2, 0), (1, 0, 1, 0)]))
    S.append(script([new(0), new(1), new(2, 2), (1, 0, 1, 0), (1, 1, 2, 0)]))
    S.append(script([new(0), new(1), new(2, 2), (1, 0, 2, 0), (1, 0, 1, 0)]))
    S.append(script([new(0), new(1), new(2, 2), (1, 0, 2, 0), (1, 0, 1, 0), (7, 1, 0, 0), (7, 0, 0, 0)]))
    S.append(script([new(0), new(1), new(2), new(3, 2), (1, 2, 3, 0), (1, 0, 1, 0), (1, 0, 2, 0)]))
    S.append(script([new(0), new(1), new(2), new(3, 2), (1, 0, 1, 0), (1, 2, 3, 0), (1, 0, 2, 0), (2, 0, 0, 0)]))
    # Init through the chain with the tail answering
    for err in (0, -1, 1, -0x80000000):
        S.append(script([new(0), new(1), new(2, 2), (1, 0, 1, 0), (1, 1, 2, 0), (2, 0, 0, 0), (2, 1, 0, 0)], **{"Fake.Init": err}))
        S.append(script([new(0), new(1, 2), (1, 0, 1, 0), (2, 0, 0, 0), (4, 0, 0, 0)], **{"Fake.Init": err}))
        S.append(script([new(0), new(1), new(2, 2), new(3, 2), (1, 0, 1, 0), (1, 1, 2, 0), (1, 1, 3, 0), (2, 0, 0, 0)],
                        **{"Fake.Init": err}))
    S.append(script([new(0), (2, 0, 0, 0)]))
    S.append(script([new(0, 2), (2, 0, 0, 0)]))
    # UnchainAndRelease from the head, the middle and the tail
    for victim in range(4):
        S.append(script([new(0), new(1), new(2), new(3, 2), (1, 2, 3, 0), (1, 1, 2, 0), (1, 0, 1, 0), (3, victim, 0, 0)]))
    S.append(script([new(0), new(1, 2), (1, 0, 1, 0), (3, 1, 0, 0), (3, 0, 0, 0)]))
    S.append(script([new(0), (3, 0, 0, 0)]))
    # GetComponent by id through the chain, with equal ids
    S.append(script([new(0), new(1), new(2, 2), (1, 1, 2, 0), (1, 0, 1, 0), (5, 0, 7, 0), (5, 1, 8, 0), (5, 2, 8, 0),
                     (4, 0, 0, 0), (4, 0, 7, 0), (4, 0, 8, 0), (4, 0, 9, 0), (4, 1, 8, 0), (4, 2, 7, 0), (4, 2, 0xFFFFFFFF, 0)]))
    # the seven forwarders through one and two real nodes
    for which in range(7):
        S.append(script([new(0), new(1), new(2, 2), (1, 1, 2, 0), (1, 0, 1, 0), (6, 0, which, 0x1234), (6, 1, which, 0),
                         (6, 2, which, 0xFFFFFFFF)], **{"Fake." + FWD[which]: 0x55}))
    S.append(script([new(0), (6, 0, 0, 0)]))
    # reference counts: AddRef and Release, with the count reaching zero and the destructors cascading
    S.append(script([new(0), new(1), new(2, 2), (1, 1, 2, 0), (1, 0, 1, 0), (8, 1, 0, 0), (7, 1, 0, 0), (7, 1, 0, 0),
                     (7, 0, 0, 0)]))
    S.append(script([new(0), new(1), (1, 0, 1, 0), (7, 0, 0, 0)]))
    S.append(script([new(0), new(1, 2), (1, 0, 1, 0), (7, 0, 0, 0)]))
    S.append(script([new(0), new(1), new(2, 2), (1, 1, 2, 0), (1, 0, 1, 0), (7, 0, 0, 0), (7, 0, 0, 0)]))
    # destructors with each flag
    for flag in (0, 1, -1, 2, 0x8000, 0x10000):
        S.append(script([new(0), new(1), new(2, 2), (1, 1, 2, 0), (1, 0, 1, 0), (9, 0, flag, 0)]))
        S.append(script([new(0), new(1, 2), (1, 0, 1, 0), (9, 1, flag, 0), (9, 0, flag, 0)]))
    return S


def chain_random(r):
    count = r.randrange(2, 6)
    ops = [new(i, 2 if (i == count - 1 and r.random() < 0.6) or r.random() < 0.15 else 1) for i in range(count)]
    for _ in range(r.randrange(2, 16)):
        kind = r.choices([1, 2, 3, 4, 5, 6, 7, 8, 9], [6, 3, 2, 3, 2, 4, 2, 2, 1])[0]
        a = r.randrange(count)
        if kind == 1:
            ops.append((1, a, r.randrange(-1, count), 0))
        elif kind == 4:
            ops.append((4, a, r.choice([0, 1, 7, 8, 0xFFFFFFFF]), 0))
        elif kind == 5:
            ops.append((5, a, r.choice([0, 1, 7, 8, 0xFFFFFFFF]), 0))
        elif kind == 6:
            ops.append((6, a, r.randrange(7), r.getrandbits(32)))
        elif kind == 9:
            ops.append((9, a, r.choice([0, 1, -1, 2]), 0))
        else:
            ops.append((kind, a, 0, 0))
    return dict(ops=ops, q=chain_queues(r))


# ---------------------------------------------------------------- the tests

def core_tests():
    out = []
    table = [
        (K_CTOR, "TbSoundCore::TbSoundCore 0x1000b618"),
        (K_DTOR, "TbSoundCore::~TbSoundCore 0x1000b738"),
        (K_MODIFY, "TbSoundCore::ModifyMaxBuffers 0x1000b7e8"),
        (K_INIT, "TbSoundCore::Init 0x1000b85c"),
        (K_CLOSE, "TbSoundCore::Close 0x1000bb50"),
        (K_GETSYS, "TbSoundCore::GetSystemInfo 0x1000bc8c"),
        (K_STOPREL, "TbSoundCore::StopAndReleasePipe 0x1000bf50"),
        (K_GETSAMP, "TbSoundCore::GetSampleInfo 0x1000c058"),
        (K_NEWSAMP, "TbSoundCore::NewSample 0x1000c220"),
        (K_NEWCTL, "TbSoundCore::NewControl 0x1000c404"),
        (K_SYSCMD, "TbSoundCore::SystemCommand 0x1000c470"),
        (K_GETMOD, "TbSoundCore::GetSystemModule 0x1000c500"),
        (K_GETSTORE, "TbSoundCore::GetSampleStore 0x1000c53c"),
        (K_GENERATE, "CModuleFactory::GenerateModule 0x10006ff8"),
        (K_CLOSEMOD, "CModuleFactory::CloseModule 0x10007120"),
        (K_CIINIT, "CMemoryCopyInit::CMemoryCopyInit 0x10014b78"),
        (K_STDCTOR, "CMemoryCopy::CStandardCopy::CStandardCopy 0x10014bfc"),
        (K_COPYCTOR, "CMemoryCopy::CCopy::CCopy 0x10014c70"),
        (K_STDCOPY, "CMemoryCopy::CStandardCopy::Copy 0x10014cac"),
        (K_ACCESSCTOR, "CAudioComponentAccess::CAudioComponentAccess 0x100013f4"),
        (K_COMPCTOR, "CAudioComponent::CAudioComponent 0x10001364"),
    ]
    for op, name in table:
        out.append(FnTest(name, core_ppc, core_host, edge=core_edges(op), rnd=core_random(op)))
    out.append(FnTest("CMemoryCopyInit::~CMemoryCopyInit 0x1000b6ac", copyinit_dtor_ppc, copyinit_dtor_host,
                      edge=lambda: [dict(flag=f) for f in DTOR_FLAGS], rnd=lambda r: dict(flag=r.choice(DTOR_FLAGS))))
    out.append(FnTest("SystemCommand constructors: Command 0x1000ba38, HandleCommand 0x1000c380, SetSystemInfo 0x1000b9a0, "
                      "SetSampleInfo 0x1000c2e8, Stop 0x10019c34, Pause 0x1001a60c, Restart 0x1001a748, Start 0x10027eb4, "
                      "UpdateAllVolumes 0x10027a64, Process 0x100280a4, StopAll 0x10028270, PauseAll 0x10028404, "
                      "RestartAll 0x1002852c", command_ppc, command_host, edge=command_edges, rnd=command_case))
    return out


def chain_tests():
    names = ("CAudioComponent::Chain 0x100015e4, ~CAudioComponent 0x10001434 and UnchainAndRelease 0x100016d0",
             "CAudioComponent::Init 0x100014ec, GetComponent 0x10001778 and the forwarders GetSystemInfo 0x100017f4, "
             "GetSampleInfo 0x10001878, NewSample 0x10001900, NewControl 0x10001978, GetSystemModule 0x100019f0, "
             "GetSampleStore 0x10001a58, SystemCommand 0x10001ac0 (scripts over six nodes)")
    return [FnTest(names[0], chain_ppc_run, chain_host, edge=chain_edges, rnd=chain_random),
            FnTest(names[1], chain_ppc_run, chain_host, edge=lambda: [], rnd=chain_random)]


TESTS = core_tests() + chain_tests()
