"""Unit w79d, the SystemCommand Execute bodies of the sound library, against libtpw_unit_SoundCommands.dylib
(tests/unit_SoundCommands.cpp), in the program "sound symbols shared" (Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/sound/SystemCommand.cpp  Pause::Execute 0x10014e00, PauseAll::Execute 0x10014f08, Restart::Execute 0x10014fac,
                        RestartAll::Execute 0x100150b4, SetSampleInfo::Execute 0x1001515c, SetSampleInfo::SetChannelInfo
                        0x10015268, Start::Execute 0x100154e4, Stop::Execute 0x10015684, StopLoop::Execute 0x1001578c,
                        StopAll::Execute 0x10015980, StopAllVolumeControl::Execute 0x10015a24, Process::Execute 0x10015ae4,
                        SetSystemInfo::Execute 0x10015b8c, UpdateAllVolumes::Execute 0x10016050

Each function runs as the original in the emulator (called at its address with the hidden TbError buffer in r3, the command in
r4 and the access object in r5) and as the host's translation on a world built from the case: an access object (the core)
whose two virtuals (+0x1c module, +0x20 store) record, a device module whose every slot records (a slot nobody should call
records its number), the sample store's functions the commands call, four channels (the functions and the virtual +0x2c
SetInfo), TbSoundCore::StopAndReleasePipe and SamsUtilities::UDebug. The call logs, the answer, the command's sample info and
the system info (and the positions it points to) after the call, and Start's two statics are compared. The unit's own
functions and the w77 records (info constructors and setters, the system info getters, TbSoundPosition) run as the original;
CUniqueHandle's constructor and TbError's constructor too. NaN floats are recorded as one value.

Labels: 1 the access object, 2 the module, 3 the store, 4 to 7 the channels, 8 the command's sample info, 9 the system info,
10 the listener position, 11 the front direction, 12 the top direction, 13 the listener velocity, 14 the speaker config.
A channel selector is 0 (NULL) or 1 to 4.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundCommands"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

S_INIT, S_INPROG = 0x1003F354, 0x1003F355     # Start::Execute's two static bytes
UDEBUG = 0x10003950
STORE_VALID, STORE_AT, STORE_AT_HANDLE = 0x100102AC, 0x10015478, 0x100100E8
STORE_SETN, STORE_STOP_ALL, STORE_STOP_VC = 0x10010568, 0x1001038C, 0x1001043C
IT_CTOR, IT_NEXT, IT_CURRENT = 0x1000FFA4, 0x1000FED8, 0x1000FF44
CH_SET_PRIORITY, CH_SET_VOLUME, CH_GET_VOLUME, CH_GET_INDEX, CH_RELEASE = 0x10009B68, 0x1000A238, 0x1000A9B4, 0x1000A19C, 0x1000A7E0
CORE_STOP_RELEASE = 0x1000BF50

(K_PAUSE, K_RESTART, K_PAUSE_ALL, K_RESTART_ALL, K_SET_SAMPLE, K_SET_CHANNEL, K_START, K_STOP, K_STOP_LOOP, K_STOP_ALL,
 K_STOP_VC, K_PROCESS, K_SET_SYSTEM, K_UPDATE) = range(14)
ADDRESS = {
    K_PAUSE: 0x10014E00, K_RESTART: 0x10014FAC, K_PAUSE_ALL: 0x10014F08, K_RESTART_ALL: 0x100150B4, K_SET_SAMPLE: 0x1001515C,
    K_SET_CHANNEL: 0x10015268, K_START: 0x100154E4, K_STOP: 0x10015684, K_STOP_LOOP: 0x1001578C, K_STOP_ALL: 0x10015980,
    K_STOP_VC: 0x10015A24, K_PROCESS: 0x10015AE4, K_SET_SYSTEM: 0x10015B8C, K_UPDATE: 0x10016050,
}
NAME = {
    K_PAUSE: "SystemCommand::Pause::Execute", K_RESTART: "SystemCommand::Restart::Execute",
    K_PAUSE_ALL: "SystemCommand::PauseAll::Execute", K_RESTART_ALL: "SystemCommand::RestartAll::Execute",
    K_SET_SAMPLE: "SystemCommand::SetSampleInfo::Execute", K_SET_CHANNEL: "SystemCommand::SetSampleInfo::SetChannelInfo",
    K_START: "SystemCommand::Start::Execute", K_STOP: "SystemCommand::Stop::Execute",
    K_STOP_LOOP: "SystemCommand::StopLoop::Execute", K_STOP_ALL: "SystemCommand::StopAll::Execute",
    K_STOP_VC: "SystemCommand::StopAllVolumeControl::Execute", K_PROCESS: "SystemCommand::Process::Execute",
    K_SET_SYSTEM: "SystemCommand::SetSystemInfo::Execute", K_UPDATE: "SystemCommand::UpdateAllVolumes::Execute",
}

# the parameter words of tpw_sx_run (the unit file's enum)
P_HANDLE, P_SINIT, P_SINPROG, P_MASK, P_INDEX, P_REENTER, P_SAMP, P_SYS, P_POS = 0, 1, 2, 3, 4, 5, 8, 16, 40
O_SAMP, O_SYS, O_POS, O_STATIC = 1, 6, 25, 37

SYS = dict(f0=0, f4=1, f8=2, fc=3, f10=4, max=5, mask=6, use=7, hwnd=8, guid=9, cls=10, pos=11, front=12, top=13, vel=14,
           spk=15, f40=16, playing=17, hw=18)
QUEUE_ERRORS = [0, -1, 1, 5, -2, 0x7FFFFFFF, -0x80000000]


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def words_at(e, address, count):
    return list(struct.unpack(">%dI" % count, e.read(address, 4 * count)))


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bits_float(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def norm(word):
    """A float word as recorded: every NaN as one value."""
    word &= MASK
    if (word & 0x7F800000) == 0x7F800000 and (word & 0x7FFFFF) != 0:
        return 0x7FC00000
    return word


# ---------------------------------------------------------------- the PowerPC side: fakes

def rec_info(e, name, address):
    """The sample info as the unit file's RecInfo records it."""
    L, w = e.cl, (lambda off: e.u32(address + off))
    flags = w(0x14)
    if e.w.fresh:
        L.rec(name, [L.lab(address), w(0), w(0x0C), w(0x10), flags, w(0x28), w(0x40) if flags & 0x40 else 0,
                     w(0x1C) if flags & 2 else 0])
    else:
        L.rec(name, [L.lab(address), w(0), w(8), w(0x0C), w(0x10), flags, w(0x18), w(0x1C), w(0x28), w(0x40)])


def rec_pos(e, name, address):
    L = e.cl
    L.rec(name, [L.lab(address)] + [norm(e.u32(address + o)) for o in (0, 4, 8)])


def channel_addr(e, selector):
    return e.w.channels[selector - 1] if 1 <= selector <= 4 else 0


def build_fakes(e):
    def error(key):
        """Writes the queue's TbError into the hidden buffer in r3."""
        def put(e):
            e.write(e.r(3), pack([e.cl.ret(key, 0)]))
        return put

    def plain(name, fields, answer=None):
        """Records `name` with the registers r3.. read as the field kinds say (l label, u value, b byte); answers r3."""
        def stub(e):
            L = e.cl
            ints = []
            for i, kind in enumerate(fields):
                v = e.r(3 + i)
                ints.append(L.lab(v) if kind == "l" else v & 0xFF if kind == "b" else v & MASK)
            L.rec(name, ints)
            if answer is not None:
                e.set_r(3, answer(e))
        return stub

    # the store
    def store_valid(e):
        L = e.cl
        L.rec("Store.GetValidChannelIndexFromHandle", (L.lab(e.r(3)), e.u32(e.r(4))))
        e.write(e.r(5), pack([L.ret("Store.index", 0)]))
        e.set_r(3, 1 if L.ret("Store.valid", 0) != 0 else 0)

    def store_at(e):
        L = e.cl
        L.rec("Store.at", (L.lab(e.r(3)), e.r(4) & MASK))
        e.set_r(3, channel_addr(e, L.ret("Store.at", 0)))

    def store_at_handle(e):
        L = e.cl
        L.rec("Store.atHandle", (L.lab(e.r(3)), e.u32(e.r(4))))
        e.set_r(3, channel_addr(e, L.ret("Store.atHandle", 0)))

    def it_ctor(e):
        L = e.cl
        L.rec("It.ctor", (L.lab(e.r(4)),))
        e.write(e.r(3), pack([channel_addr(e, L.ret("It.first", 0)), 0]))

    def it_next(e):
        L = e.cl
        L.rec("It.next", ())
        current = channel_addr(e, L.ret("It.next", 0))
        e.write(e.r(3), pack([current]))
        e.set_r(3, current)

    def it_current(e):
        e.cl.rec("It.current", ())
        e.set_r(3, e.u32(e.r(3)))

    def core_stop(e):
        L = e.cl
        L.rec("Core.StopAndReleasePipe", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(6) & 0xFF))
        error("Core.StopAndReleasePipe")(e)

    def udebug(e):
        e.cl.rec("UDebug", (), (e.cstr(e.r(3)),))

    def queue(key):
        return lambda e: T.u32(e.cl.ret(key, 0))

    internals = {
        STORE_VALID: store_valid, STORE_AT: store_at, STORE_AT_HANDLE: store_at_handle,
        STORE_SETN: plain("Store.SetNumberOfChannelsToUse", "ll"),
        STORE_STOP_ALL: plain("Store.StopAllChannels", "lb"),
        STORE_STOP_VC: plain("Store.StopAllChannelsWithVolumeControl", "lu"),
        IT_CTOR: it_ctor, IT_NEXT: it_next, IT_CURRENT: it_current,
        CH_SET_PRIORITY: plain("Chan.SetPriority", "lu"), CH_SET_VOLUME: plain("Chan.SetVolume", "lu"),
        CH_GET_VOLUME: plain("Chan.GetVolume", "l", queue("Chan.GetVolume")),
        CH_GET_INDEX: plain("Chan.GetIndex", "l", queue("Chan.GetIndex")),
        CH_RELEASE: plain("Chan.ReleasePipe", "lb", lambda e: 1 if e.cl.ret("Chan.ReleasePipe", 0) != 0 else 0),
        CORE_STOP_RELEASE: core_stop, UDEBUG: udebug,
    }
    for address, fn in internals.items():
        T.stub_internal(e, address, fn)


def build_tables(e):
    fake = lambda fn: T.fake_function(e, fn)

    # the access object: +0x1c the module, +0x20 the store
    def a_module(e):
        e.cl.rec("Access.GetSystemModule", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.w.module)

    def a_store(e):
        e.cl.rec("Access.GetSampleStore", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.w.store)

    words = [0] * 12
    words[0x1C // 4] = fake(a_module)
    words[0x20 // 4] = fake(a_store)
    e.vt_access = T.persist_alloc(e, 4 * len(words), pack(words))

    # the module: the slots the commands call record; every other slot records its number
    def err(key):
        return lambda e: e.write(e.r(3), pack([e.cl.ret(key, 0)]))

    def m_process(e):
        e.cl.rec("Mod.Process", (e.cl.lab(e.r(3)),))

    def m_one(name, kind):
        def stub(e):
            L = e.cl
            if kind == "u":
                L.rec(name, (L.lab(e.r(4)), e.r(5) & MASK))
            elif kind == "c":
                L.rec(name, (L.lab(e.r(4)), L.lab(e.r(5))))
            elif kind == "uu":
                L.rec(name, (L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))
            else:
                L.rec(name, (L.lab(e.r(4)),))
            err(name)(e)
        return stub

    def m_pos(name):
        def stub(e):
            e.cl.rec(name, (e.cl.lab(e.r(4)),))
            rec_pos(e, name + ".pos", e.r(5))
            err(name)(e)
        return stub

    def m_direction(e):
        e.cl.rec("Mod.SetListenerDirection", (e.cl.lab(e.r(4)),))
        rec_pos(e, "Mod.SetListenerDirection.front", e.r(5))
        rec_pos(e, "Mod.SetListenerDirection.top", e.r(6))
        err("Mod.SetListenerDirection")(e)

    def m_channel_info(e):
        e.cl.rec("Mod.SetChannelInfo", (e.cl.lab(e.r(4)), e.r(5) & MASK, e.cl.lab(e.r(6))))
        rec_info(e, "Mod.SetChannelInfo.info", e.r(7))
        err("Mod.SetChannelInfo")(e)

    def m_channel_volume(e):
        e.cl.rec("Mod.SetChannelVolume", (e.cl.lab(e.r(4)), e.r(5) & MASK))
        rec_info(e, "Mod.SetChannelVolume.info", e.r(6))
        err("Mod.SetChannelVolume")(e)

    def other(slot):
        def stub(e):
            e.cl.rec("Mod.slot%02x" % slot, (e.cl.lab(e.r(3)),))
        return stub

    # StreamData answers the queue; when the case asks, it first starts the command again (the re-entry guard) from a nested
    # PowerPC call, as the host's fake does, and records that call's answer.
    def m_stream(e):
        L, buf = e.cl, e.r(3)
        L.rec("Mod.StreamData", (L.lab(e.r(4)), L.lab(e.r(5))))
        if not e.w.reenter:
            e.write(buf, pack([L.ret("Mod.StreamData", 0)]))
            return
        e.w.reenter = False

        def nested(e, call):
            call(("code", ADDRESS[K_START]), e.w.inner_buf, e.w.inner_cmd, e.w.access)
            L.rec("Inner.Start", (T.s32(e.u32(e.w.inner_buf)),))
            e.write(buf, pack([L.ret("Mod.StreamData", 0)]))
        e.pending_cb = nested
        e.uc.reg_write(T.P.UC_PPC_REG_PC, T.ppcemu.RETURN_ADDR)

    words = [0] * 40
    for slot in range(0x08, 0x98, 4):
        words[slot // 4] = fake(other(slot))
    words[0x0C // 4] = fake(m_process)
    words[0x1C // 4] = fake(m_one("Mod.SetOptions", "u"))
    words[0x28 // 4] = fake(m_stream)
    words[0x34 // 4] = fake(m_one("Mod.PauseChannel", "u"))
    words[0x38 // 4] = fake(m_one("Mod.RestartChannel", "u"))
    words[0x3C // 4] = fake(m_one("Mod.PauseAll", ""))
    words[0x40 // 4] = fake(m_one("Mod.RestartAll", ""))
    words[0x54 // 4] = fake(m_one("Mod.SetSpeakerConfig", "c"))
    words[0x58 // 4] = fake(m_one("Mod.Set3DDSPFunctionality", "uu"))
    words[0x74 // 4] = fake(m_pos("Mod.SetListenerVelocity"))
    words[0x78 // 4] = fake(m_pos("Mod.SetListenerPosition"))
    words[0x7C // 4] = fake(m_direction)
    words[0x88 // 4] = fake(m_channel_info)
    words[0x8C // 4] = fake(m_channel_volume)
    e.vt_module = T.persist_alloc(e, 4 * len(words), pack(words))

    # the channel: +0x2c SetInfo(const TbSoundSampleInfo&)
    def c_set_info(e):
        e.cl.rec("Chan.SetInfo", (e.cl.lab(e.r(4)),))
        rec_info(e, "Chan.SetInfo.info", e.r(5))
        err("Chan.SetInfo")(e)

    words = [0] * 20
    for slot in range(0x08, 0x50, 4):
        words[slot // 4] = fake(other(slot))
    words[0x2C // 4] = fake(c_set_info)
    e.vt_channel = T.persist_alloc(e, 4 * len(words), pack(words))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    build_fakes(e)
    build_tables(e)
    lib = runner.lib
    lib.tpw_sx_run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p]


# ---------------------------------------------------------------- the cases

def zero_sys():
    return [0] * 19


def make_case(op, **kw):
    c = dict(op=op, handle=0x1234, sinit=0, sinprog=0, mask=0, index=0, reenter=0, samp=[0, 0, 0, 0, 0], sys=zero_sys(),
             pos=[0x3F800000, 0, 0] * 4, q={})
    c.update(kw)
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def sys_words(**kw):
    w = zero_sys()
    for name, value in kw.items():
        w[SYS[name]] = value
    return w


def ppc_run(e, c):
    op = c["op"]
    L = e.cl = CL.PpcLog({}, c["q"])
    w = e.w = type("W", (), {})()
    w.fresh = op in (K_STOP_LOOP, K_UPDATE)
    access = e.alloc(8, pack([e.vt_access, FILL]))
    w.module = e.alloc(8, pack([e.vt_module, FILL]))
    w.store = e.alloc(16, b"\xa5" * 16)
    w.channels = [e.alloc(0x60, pack([e.vt_channel]) + b"\xa5" * 0x5C) for _ in range(4)]
    samp = e.alloc(0x90, b"\xa5" * 0x90)
    for offset, value in zip((0x0C, 0x14, 0x18, 0x1C, 0x28), c["samp"]):
        e.write(samp + offset, pack([value]))
    positions = [e.alloc(12, pack(c["pos"][3 * i:3 * i + 3])) for i in range(4)]
    config = e.alloc(16, b"\xa5" * 16)
    sw = list(c["sys"])
    sw[SYS["pos"]], sw[SYS["front"]], sw[SYS["top"]], sw[SYS["vel"]] = positions
    sw[SYS["spk"]] = config
    info = e.alloc(0x4C, pack(sw))
    L.labels.update({access: 1, w.module: 2, w.store: 3, samp: 8, info: 9, config: 14})
    for i, address in enumerate(w.channels):
        L.labels[address] = 4 + i
    for i, address in enumerate(positions):
        L.labels[address] = 10 + i
    e.write(S_INIT, bytes([1 if c["sinit"] else 0]))
    e.write(S_INPROG, bytes([1 if c["sinprog"] else 0]))
    buf = e.alloc(4, pack([FILL]))
    handle = c["handle"] & MASK
    w.reenter, w.access = bool(c["reenter"]), access
    w.inner_buf, w.inner_cmd = e.alloc(4, pack([FILL])), e.alloc(8, pack([0, handle]))
    if op in (K_PAUSE, K_RESTART, K_START, K_STOP, K_STOP_LOOP):
        cmd = e.alloc(8, pack([0, handle]))
    elif op in (K_SET_SAMPLE, K_SET_CHANNEL):
        cmd = e.alloc(12, pack([0, handle, samp]))
    elif op == K_SET_SYSTEM:
        cmd = e.alloc(8, pack([0, info]))
    elif op == K_STOP_VC:
        cmd = e.alloc(8, pack([0, c["mask"]]))
    else:
        cmd = e.alloc(8, pack([0, FILL]))
    args = [buf, cmd, access] + ([c["index"] & MASK] if op == K_SET_CHANNEL else [])
    T.call_ppc(e, ADDRESS[op], args)
    result = e.u32(buf)
    state = words_at(e, info, 19)
    for i in (SYS["pos"], SYS["front"], SYS["top"], SYS["vel"], SYS["spk"]):
        state[i] = L.lab(state[i]) & MASK
    return dict(r=result, samp=tuple(e.u32(samp + o) for o in (0x0C, 0x14, 0x18, 0x1C, 0x28)), sys=tuple(state),
                pos=tuple(norm(e.u32(a + o)) for a in positions for o in (0, 4, 8)),
                st=(e.read(S_INIT, 1)[0], e.read(S_INPROG, 1)[0]), log=L.log)


def host_run(lib, c):
    p = [0] * 64
    p[P_HANDLE], p[P_SINIT], p[P_SINPROG], p[P_MASK], p[P_INDEX] = c["handle"], int(c["sinit"]), int(c["sinprog"]), c["mask"], c["index"]
    p[P_REENTER] = int(c["reenter"])
    p[P_SAMP:P_SAMP + 5] = c["samp"]
    p[P_SYS:P_SYS + 19] = c["sys"]
    p[P_POS:P_POS + 12] = c["pos"]
    params = (C.c_uint32 * 64)(*[v & MASK for v in p])
    out = (C.c_uint32 * 64)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_sx_run(c["op"], params, out)
    log = CL.host_end(lib)
    return dict(r=out[0], samp=tuple(out[O_SAMP:O_SAMP + 5]), sys=tuple(out[O_SYS:O_SYS + 19]), pos=tuple(out[O_POS:O_POS + 12]),
                st=tuple(out[O_STATIC:O_STATIC + 2]), log=log)


# ---------------------------------------------------------------- randomisation

SPECIAL_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 3.0, 4.0, 2.0, 1e20, -1e20, 1e-30, 1e-45, 3.4e38, -3.4e38, math.inf, -math.inf, math.nan]


def rfloat(r):
    k = r.random()
    if k < 0.3:
        return fbits(r.choice(SPECIAL_FLOATS))
    if k < 0.8:
        return fbits(r.uniform(-100.0, 100.0))
    return r.getrandbits(32)


def rpos(r):
    return [rfloat(r) for _ in range(3)]


def rhandle(r):
    return r.choice([0, 1, 0x1234, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.getrandbits(32)])


def rindex(r):
    return r.choice([0, 1, 2, 0x3F, 0xFFFFFFFF, r.getrandbits(32)])


def rerr(r):
    return r.choice(QUEUE_ERRORS)


def rsel(r):
    return r.choice([0, 1, 2, 3, 4])


def random_case(op):
    def make(r):
        q = {}
        for key in ("Mod.PauseChannel", "Mod.RestartChannel", "Mod.PauseAll", "Mod.RestartAll", "Mod.StreamData",
                    "Mod.SetOptions", "Mod.Set3DDSPFunctionality", "Mod.SetChannelInfo", "Mod.SetListenerPosition",
                    "Mod.SetListenerDirection", "Mod.SetListenerVelocity", "Mod.SetSpeakerConfig", "Mod.SetChannelVolume",
                    "Chan.SetInfo", "Core.StopAndReleasePipe"):
            q[key] = [rerr(r) for _ in range(2)]
        q["Store.valid"] = [r.randrange(2)]
        q["Store.index"] = [rindex(r)]
        q["Store.at"] = [rsel(r)]
        q["Store.atHandle"] = [rsel(r)]
        q["Chan.GetVolume"] = [r.getrandbits(32) for _ in range(5)]
        q["Chan.GetIndex"] = [rindex(r) for _ in range(5)]
        q["Chan.ReleasePipe"] = [r.randrange(2)]
        q["It.first"] = [rsel(r)]
        q["It.next"] = [rsel(r) for _ in range(r.randrange(1, 6))] + [0]
        c = make_case(op, handle=rhandle(r), sinit=r.randrange(2), sinprog=r.randrange(2), reenter=r.randrange(2), mask=r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]),
                      index=rindex(r), q=q)
        if op in (K_SET_SAMPLE, K_SET_CHANNEL):
            f0c = r.choice([0, 2, 8, 0x10, 0x4000, 0x401A, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32) & ~0x401A])
            flags = r.choice([0, 0x400, 2, 0x402, 0x800, 0xFFFFFFFF, 0xFFFFFBFF, r.getrandbits(32), r.getrandbits(32) & ~0x402])
            c["samp"] = [f0c, flags, r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]
        if op == K_SET_SYSTEM:
            mask = r.choice([0, 0x40, 4, 8, 0x10, 0x20, 0x7C, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32) & ~0x7C])
            c["sys"] = sys_words(f4=r.choice([0, 1, 0x100, r.getrandbits(32)]), mask=mask, use=r.getrandbits(32),
                                 f0=r.getrandbits(32), f8=r.getrandbits(32), max=r.getrandbits(32), hwnd=r.getrandbits(32),
                                 f40=r.getrandbits(32), playing=r.getrandbits(32))
            c["pos"] = [x for _ in range(4) for x in rpos(r)]
            if r.random() < 0.5:
                c["sys"][SYS["mask"]] |= 8
                c["pos"][3:6] = [fbits(r.uniform(-50.0, 50.0)) for _ in range(3)]
        return c
    return make


# ---------------------------------------------------------------- the edges

def handle_edges(op, key):
    """Pause and Restart: a valid or an invalid handle, the module's answers."""
    cases = []
    for handle in (0, 1, 0x1234, 0xFFFFFFFF, 0x80000000):
        for valid in (0, 1):
            for index in (0, 1, 0x3F, 0xFFFFFFFF):
                for answer in QUEUE_ERRORS:
                    cases.append(make_case(op, handle=handle, q={"Store.valid": valid, "Store.index": index, key: answer}))
    return cases


def all_edges(op, key):
    return [make_case(op, q={key: answer}) for answer in QUEUE_ERRORS]


def set_sample_edges():
    cases = []
    for handle in (0, 1, 0x1234, 0xFFFFFFFF):
        for valid in (0, 1):
            for index in (0, 5, 0xFFFFFFFF):
                for answer in (0, -1):
                    cases.append(make_case(K_SET_SAMPLE, handle=handle, samp=[0x4000, 0x402, 7, 0x55, 0x66],
                                           q={"Store.valid": valid, "Store.index": index, "Store.at": 1, "Mod.SetChannelInfo": answer}))
    return cases


F0C = [0, 1, 2, 4, 8, 0x10, 0x20, 0x4000, 0x4002, 0x4008, 0x4010, 0x1A, 0x401A, 0x8000, 0xFFFFFFFF, 0xFFFFBFFF, 0x7FFF0000]
FLAGS = [0, 1, 2, 0x400, 0x402, 4, 0x800, 0x802, 0xFFFFFFFF, 0xFFFFFBFF, 0xFFFFFFFD, 0x80000000, 0x40000402]


def set_channel_edges():
    cases = []
    for f0c in F0C:
        for flags in FLAGS:
            for sel in (0, 1, 3):
                cases.append(make_case(K_SET_CHANNEL, index=7, samp=[f0c, flags, 0x1818, 0x2020, 0x4040],
                                       q={"Store.at": sel, "Chan.GetVolume": 0xCAFE, "Mod.Set3DDSPFunctionality": -1,
                                          "Chan.SetInfo": 3, "Mod.SetChannelInfo": -4}))
    for index in (0, 1, 0xFFFFFFFF, 0x80000000):
        for volume in (0, 1, 0xFFFFFFFF):
            cases.append(make_case(K_SET_CHANNEL, index=index, samp=[0x401A, 0x402, 9, volume, 0x77],
                                   q={"Store.at": 2, "Chan.GetVolume": [volume ^ 0x55, 5]}))
    return cases


def start_edges():
    cases = []
    for sinit in (0, 1):
        for sinprog in (0, 1):
            for handle in (0, 1, 0xFFFFFFFF):
                for sel in (0, 1):
                    for answer in QUEUE_ERRORS:
                        cases.append(make_case(K_START, sinit=sinit, sinprog=sinprog, handle=handle,
                                               q={"Store.atHandle": sel, "Mod.StreamData": answer, "Chan.ReleasePipe": 1}))
    # StreamData starts the command again before it answers: the guard must make the inner one log and answer -1
    for sinit in (0, 1):
        for answer in (0, -1, 3):
            cases.append(make_case(K_START, sinit=sinit, sinprog=0, handle=5, reenter=1,
                                   q={"Store.atHandle": 1, "Mod.StreamData": answer, "Chan.ReleasePipe": 1}))
    return cases


def stop_edges():
    cases = []
    for handle in (0, 1, 0xFFFFFFFF):
        for valid in (0, 1):
            for sel in (0, 1, 4):
                for answer in QUEUE_ERRORS:
                    cases.append(make_case(K_STOP, handle=handle, q={"Store.valid": valid, "Store.index": 3, "Store.at": sel,
                                                                    "Core.StopAndReleasePipe": answer}))
    return cases


def stop_loop_edges():
    cases = []
    for handle in (0, 1, 0xFFFFFFFF):
        for valid in (0, 1):
            for sel in (0, 1, 2):
                for answer in QUEUE_ERRORS:
                    cases.append(make_case(K_STOP_LOOP, handle=handle, q={"Store.valid": valid, "Store.index": 2, "Store.at": sel,
                                                                         "Chan.SetInfo": answer}))
    return cases


def stop_vc_edges():
    return [make_case(K_STOP_VC, mask=m) for m in (0, 1, 2, 0x80000000, 0xFFFFFFFF, 0x12345678)]


FRONTS = [(1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (0.0, 0.0, 0.0), (3.0, 4.0, 0.0), (1.0, 2.0, 2.0), (-2.0, 3.0, 6.0),
          (1e20, 1e20, 1e20), (1e-30, 1e-30, 1e-30), (1e-45, 0.0, 0.0), (3.4e38, 3.4e38, 0.0), (math.inf, 0.0, 0.0),
          (0.0, math.nan, 1.0), (-0.0, -0.0, -0.0), (0.1, 0.2, 0.3), (123.456, -78.9, 0.001), (5.0, 12.0, 0.0), (1.0, 1.0, 1.0)]


def system_edges():
    cases = []
    top = [fbits(0.25), fbits(0.5), fbits(0.75)]
    for f4 in (0, 1, 0xFFFFFFFF):
        for mask in (0, 0x40, 4, 8, 0x10, 0x20, 0x7C, 0x80, 0xFFFFFFFF, 0xFFFFFF83, 0x44, 0x48, 0x50, 0x60, 0x28, 0x18, 0x30):
            cases.append(make_case(K_SET_SYSTEM, sys=sys_words(f4=f4, mask=mask, use=3, max=9),
                                   pos=[fbits(1.5), fbits(2.5), fbits(3.5), fbits(1.0), fbits(2.0), fbits(2.0)] + top + [fbits(9.0)] * 3,
                                   q={"Mod.SetOptions": -1, "Mod.SetListenerPosition": 3}))
    for front in FRONTS:
        cases.append(make_case(K_SET_SYSTEM, sys=sys_words(mask=8), pos=[0, 0, 0] + [fbits(v) for v in front] + top + [0, 0, 0]))
    # many directions whose squares need all 48 bits, so that the order and the precision of the length show: the length of
    # a unit-ish vector, big and small vectors, and vectors near the float range
    import random
    r = random.Random(7900)
    for scale in (1.0, 10.0, 100.0, 1e-3, 1e5, 1e15, 1e-15, 1e19, 3e19, 1e-20):
        for _ in range(40):
            front = [r.uniform(-scale, scale) for _ in range(3)]
            cases.append(make_case(K_SET_SYSTEM, sys=sys_words(mask=8), pos=[0, 0, 0] + [fbits(v) for v in front] + top + [0, 0, 0]))
    return cases


def update_edges():
    cases = []
    for first in (0, 1, 2):
        for nxt in ([0], [2, 0], [2, 3, 0], [2, 3, 4, 0], [1, 1, 0]):
            cases.append(make_case(K_UPDATE, q={"It.first": first, "It.next": nxt, "Chan.GetVolume": [0x10, 0x20, 0x30, 0x40, 0x50],
                                                "Chan.GetIndex": [5, 6, 7, 8, 9], "Mod.SetChannelVolume": [0, -1, 3]}))
    for volume in (0, 1, 0xFFFFFFFF, 0x80000000):
        cases.append(make_case(K_UPDATE, q={"It.first": 1, "It.next": [2, 0], "Chan.GetVolume": [volume, 0], "Chan.GetIndex": 0xFFFFFFFF}))
    return cases


EDGES = {
    K_PAUSE: lambda: handle_edges(K_PAUSE, "Mod.PauseChannel"),
    K_RESTART: lambda: handle_edges(K_RESTART, "Mod.RestartChannel"),
    K_PAUSE_ALL: lambda: all_edges(K_PAUSE_ALL, "Mod.PauseAll"),
    K_RESTART_ALL: lambda: all_edges(K_RESTART_ALL, "Mod.RestartAll"),
    K_SET_SAMPLE: set_sample_edges,
    K_SET_CHANNEL: set_channel_edges,
    K_START: start_edges,
    K_STOP: stop_edges,
    K_STOP_LOOP: stop_loop_edges,
    K_STOP_ALL: lambda: [make_case(K_STOP_ALL)],
    K_STOP_VC: stop_vc_edges,
    K_PROCESS: lambda: [make_case(K_PROCESS)],
    K_SET_SYSTEM: system_edges,
    K_UPDATE: update_edges,
}

TESTS = [FnTest("%s 0x%08x" % (NAME[op], ADDRESS[op]), ppc_run, host_run, edge=EDGES[op], rnd=random_case(op))
         for op in range(14)]
