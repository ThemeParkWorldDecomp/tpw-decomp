"""Unit w79h, the Execute bodies of the sound library's system commands, against libtpw_unit_SysCommands.dylib
(tests/unit_SysCommands.cpp), in the program "sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 =
0x1003abf0):
  src/TbSysCommand.cpp  the 47 TbSysCommand::<Class>::Execute bodies (0x10027fd4 to 0x1002e348) and the five payload accessors of the
                        command bases: SetValue::GetSysValue 0x1002899c, SetValue::GetFloatSysValue 0x10028658,
                        SetValue::GetTbHandleSysValue 0x100287d8, GetValue::GetSysValue 0x10028aa8, Serialise::GetHandle 0x1002df40

Each function runs as the original in the emulator and as the host's translation on a world built from the case: a body (the real
TbAudioBody of w79f on the host, its words laid out as the Mac's on the PowerPC side: the list at +0x28, the manager at +0x2c, a table
whose slots +0x34 StopHandle and +0x38 DeregisterCatagoryBanks record), a sound manager, a placeholder list, a placeholder (with a
sample), and a command object laid out as the game and the library build it. GetSoundManager and GetPlaceHolderList run as the original
(w79f); the command constructors (SystemCommand::Process and the others, TbSoundSystemInfo, TbSoundSampleInfo) run as the original too.
Every callee outside the unit is a fake that records the same event on both sides: the manager (GetSystemInfo and SystemCommand, the
virtual slots +0x08 and +0x18, and the members SetDistanceMapping, GetMaxNumberOfChannels, SetQSoundInfo, SetDroppable,
UpdateVolumeControl, ToggleVolumeControl, GetDirectSoundObject, GetVolume at their addresses), the list (ProcessPlaceHolders,
StopAllPlaceHolders, GetNumberOfChannels, SetNumberOfChannels, SetBoundingRectangle, GetPlaceholderAndSoundHandle, FindPlaceHolder),
the placeholder (SetVolume, SetVolumeAndPitch, the virtuals Pause +0x24 and Restart +0x28), TpBody::Tick, Save and Load (untranslated)
and CvtDblUl. The command the manager receives is compared by its class (found on the PowerPC side from the vtable word the original's
constructor stores) and by the words it defines; the manager answers from queues.

TbError is returned through a hidden r3 buffer (the command in r4, the body in r5) and read from the buffer. Pointers compare as labels
(0 NULL, the label of the object, -1 for anything else); handles and words compare as words.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SysCommands"
MASK = 0xFFFFFFFF
FILL = 0xA5A5A5A5

BODY_VT = 0x1003CB70

# the callees outside the unit, by address
LIST_PROCESS, LIST_STOPALL, LIST_GETCH, LIST_SETCH, LIST_SETRECT, LIST_GETPH, LIST_FIND = (
    0x1001BF78, 0x1001C418, 0x10028950, 0x1001BD68, 0x1001C23C, 0x1001C36C, 0x1001C8D8)
HOLDER_SETVOLUME, HOLDER_SETVOLUMEPITCH = 0x10019EA0, 0x1001A1BC
MGR_SETDISTANCE, MGR_GETMAX, MGR_QSOUND, MGR_DROPPABLE, MGR_UPDATE, MGR_TOGGLE, MGR_DS, MGR_GETVOLUME = (
    0x10027274, 0x10027020, 0x10027160, 0x10027408, 0x100279C4, 0x10027ADC, 0x100278C0, 0x1002A080)
TPBODY_TICK, TPBODY_SAVE, TPBODY_LOAD = 0x1002E404, 0x1002E57C, 0x1002E6EC
CVTDBLUL = 0x1002E988

# the vtable words the library's command constructors store (kind numbers as in the host's fake manager)
COMMAND_KINDS = {0x1003CB48: 1, 0x1003CA7C: 2, 0x1003C984: 3, 0x1003C928: 4, 0x1003CB28: 5, 0x1003C9F0: 6}

# the operations, in the order of enum Op in the unit file: (name, address, command layout, kind of the system info)
#   layout: "" no payload, "w1".."w5" that many integer words, "out" the out pointer, "outh" the out pointer and a handle word,
#   "cfg" the speaker config pointer, "file" the save file, "load" the file and a flag byte at +8, "acc:..." an accessor
OPS = [
    ("Process", 0x10027FD4, ""),
    ("StopAll", 0x100281A8, ""),
    ("StopAllWithFade", 0x100282DC, ""),
    ("PauseAll", 0x10028350, ""),
    ("RestartAll", 0x10028474, ""),
    ("SetDistanceMapping", 0x1002859C, "w1"),
    ("SetBoundingRectangle", 0x100286A4, "w5"),
    ("SetNumberOfChannels", 0x10028824, "w1"),
    ("GetMaxNumberOfChannels", 0x100289E0, "out"),
    ("GetNumberOfPlayingChannels", 0x10028AEC, "out"),
    ("FlushCatagory", 0x10028C4C, "w1"),
    ("SetSpeakerConfig", 0x10028CF0, "cfg"),
    ("SetFades", 0x10028DC0, "w1"),
    ("SetHalfRate", 0x10028E2C, "w1"),
    ("SetQSoundInfo", 0x10028E9C, "w3"),
    ("SetEAX", 0x10028F60, "w1"),
    ("SetDroppable", 0x10028FC8, "w1"),
    ("SetGeometryInfo", 0x1002908C, "w1"),
    ("StopHandle", 0x10029100, "w1"),
    ("SetHandleVolume", 0x100291B8, "w2"),
    ("SetHandleVolumeAndPitch", 0x1002930C, "w3"),
    ("StopHandleWithFade", 0x1002947C, "w1"),
    ("SetMasterVolume", 0x100294F4, "w1"),
    ("ToggleMaster", 0x100295C8, "w1"),
    ("SetSpeechVolume", 0x1002967C, "w1"),
    ("ToggleSpeech", 0x10029750, "w1"),
    ("SetUser1Volume", 0x10029804, "w1"),
    ("ToggleUser1", 0x100298D8, "w1"),
    ("SetUser2Volume", 0x1002998C, "w1"),
    ("ToggleUser2", 0x10029A60, "w1"),
    ("SetSFXVolume", 0x10029B14, "w1"),
    ("ToggleSFX", 0x10029BE4, "w1"),
    ("SetMusicVolume", 0x10029C94, "w1"),
    ("ToggleMusic", 0x10029D68, "w1"),
    ("GetDirectSoundObject", 0x10029E1C, "out"),
    ("GetSampleLength", 0x10029ED4, "outh"),
    ("GetSFXVolume", 0x10029FBC, "out"),
    ("GetMusicVolume", 0x1002A0C4, "out"),
    ("Tick", 0x1002DE28, ""),
    ("Save", 0x1002DEAC, "file"),
    ("Load", 0x1002DF84, "load"),
    ("PauseHandle", 0x1002E028, "w1"),
    ("RestartHandle", 0x1002E10C, "w1"),
    ("SetPresetReverb", 0x1002E1F0, "w1"),
    ("RegisterObject", 0x1002E264, "w1"),
    ("DeleteObject", 0x1002E2D8, "w1"),
    ("DeleteAllObjects", 0x1002E348, "w1"),
    ("SetValue::GetSysValue", 0x1002899C, "acc:w"),
    ("SetValue::GetFloatSysValue", 0x10028658, "acc:f"),
    ("SetValue::GetTbHandleSysValue", 0x100287D8, "acc:w"),
    ("GetValue::GetSysValue", 0x10028AA8, "acc:out"),
    ("Serialise::GetHandle", 0x1002DF40, "acc:w"),
]
NAMES = [o[0] for o in OPS]
IDX = {name: i for i, name in enumerate(NAMES)}

# the parameter words of tpw_sc_run (enum Param of the unit file)
P_OP, P_W, P_LIST, P_SAMPLE, P_LENGTH = 0, 1, 8, 9, 10

QUEUE_ERRORS = [0, -1, 1, 5, -2, 0x7FFFFFFF, -0x80000000]
GOOD_ERRORS = [0, 1, 5]
POOL32 = [0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0x100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678, 0x01000000, 0x00FFFFFF, 0x10000]
VOLUMES = [0, 1, 2, 49, 50, 99, 100, 101, 127, 255, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x1000000, 0x1000001, 0x1000003,
           0x2000003, 0xFFFFFF01, 0x7FFFFF81]
HANDLES = [0, 1, 2, 0x10001, 0x12345678, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
FLOATS = [0x00000000, 0x80000000, 0x3F000000, 0x3F7EB852, 0x3F800000, 0x40000000, 0xBF800000, 0x3C23D70A, 0x00000001, 0x007FFFFF,
          0x7F7FFFFF, 0x4E6E6B28, 0x7FC00000, 0x7F800000, 0xFF800000, 0xFF7FFFFF, 0x42C80000, 0x41200000, 0x3F7FFFFF, 0x3F800001,
          0x4B800000, 0x4F800000, 0x4F7FFFFF, 0xC2C80000]
MASKS = [0, 0x200, 0x1FF, 0x201, 0x400, 0xFFFFFFFF, 0x100, 0x80000000, 0x3FF, 0xFFFFFDFF]
CHANNELS = [0, 1, 2, 8, 0x10, 0x1E, 0x1F, 0x20, 0xFFFFFFFF, 0x80000000]
SAMPLE_COOKIES = [0, 1, 0x1234, 0x80000000, 0xFFFFFFFF]
PITCHES = [0, 1, -1, 0x7FFFFFFF, -0x80000000, 0xFFFF, 0x10000, 100, -100]

# the queue keys of the fakes and their pools (every case may set any of them; a key that is not read is harmless)
Q_POOLS = {
    "Mgr.Command": QUEUE_ERRORS, "Mgr.Info": QUEUE_ERRORS, "Mgr.Info.mask": MASKS, "Mgr.Info.playing": POOL32,
    "Mgr.Max": QUEUE_ERRORS, "Mgr.Max.value": POOL32, "Mgr.QSound": QUEUE_ERRORS, "Mgr.Update": QUEUE_ERRORS,
    "Mgr.Toggle": QUEUE_ERRORS, "Mgr.DS": QUEUE_ERRORS, "Mgr.DS.value": POOL32, "Mgr.Volume": FLOATS,
    "CvtDblUl": POOL32, "List.GetCh": CHANNELS, "List.SetCh": CHANNELS, "List.SetRect": QUEUE_ERRORS,
    "List.GetPH.holder": [0, 1], "List.GetPH.sample": SAMPLE_COOKIES, "List.Find": [0, 1], "Holder.Pause": QUEUE_ERRORS,
    "Holder.Restart": QUEUE_ERRORS, "Body.StopHandle": QUEUE_ERRORS, "Body.Dereg": QUEUE_ERRORS,
    "TpBody.Save": QUEUE_ERRORS, "TpBody.Load": QUEUE_ERRORS,
}

VT = {}


def pack(words):
    return struct.pack(">%dI" % len(words), *[w & MASK for w in words])


def f32_of(x):
    """The single-precision bits of a double that holds a float."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bits_to_float(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def f64_halves(x):
    bits = struct.unpack(">Q", struct.pack(">d", x))[0]
    return bits >> 32, bits & MASK


def rec_info(L, e, name, first, address):
    """The words a system info defines for the kind of constructor the running command used (the host's RecInfo)."""
    word = lambda offset: e.u32(address + offset)
    if e.info_kind == 0:
        L.rec(name, [first & MASK, word(0), word(4), word(8), word(0x18), word(0x1C), word(0x20), word(0x40)])
    else:
        L.rec(name, [first & MASK, word(4), word(8), word(0x18), L.lab(word(0x3C))])


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_sc_run.argtypes = [C.c_void_p, C.c_void_p]
    fake = lambda fn: T.fake_function(e, fn)

    # ---- the list
    def list_process(e):
        e.cl.rec("List.ProcessPlaceHolders", (e.cl.lab(e.r(3)),))

    def list_stopall(e):
        e.cl.rec("List.StopAllPlaceHolders", (e.cl.lab(e.r(3)), e.r(4) & MASK))

    def list_getch(e):
        L = e.cl
        L.rec("List.GetNumberOfChannels", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("List.GetCh", 0)))

    def list_setch(e):
        L = e.cl
        L.rec("List.SetNumberOfChannels", (L.lab(e.r(3)), e.r(4) & MASK))
        e.set_r(3, T.u32(L.ret("List.SetCh", 0)))

    def list_setrect(e):
        # r3 = the buffer, r4 = the list, r5 = the handle, r6 = the rectangle
        L = e.cl
        rect = [e.u32(e.r(6) + 4 * i) for i in range(4)]
        L.rec("List.SetBoundingRectangle", [L.lab(e.r(4)), e.r(5) & MASK] + rect)
        e.write(e.r(3), pack([L.ret("List.SetRect", 0)]))

    def list_getph(e):
        # r3 = the list, r4 = the handle, r5 = where the placeholder goes
        L = e.cl
        L.rec("List.GetPlaceholderAndSoundHandle", (L.lab(e.r(3)), e.r(4) & MASK))
        e.write(e.r(5), pack([e.w.holder if L.ret("List.GetPH.holder", 0) != 0 else 0]))
        e.set_r(3, T.u32(L.ret("List.GetPH.sample", 0)))

    def list_find(e):
        L = e.cl
        L.rec("List.FindPlaceHolder", (L.lab(e.r(3)), e.r(4) & MASK))
        e.set_r(3, e.w.holder if L.ret("List.Find", 0) != 0 else 0)

    def holder_setvolume(e):
        e.cl.rec("Holder.SetVolume", (e.cl.lab(e.r(3)), e.r(4) & MASK))

    def holder_setvolumepitch(e):
        e.cl.rec("Holder.SetVolumeAndPitch", (e.cl.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))

    # ---- the manager's members
    def mgr_setdistance(e):
        e.cl.rec("Mgr.SetDistanceMapping", (e.cl.lab(e.r(3)), f32_of(e.f(1))))

    def mgr_getmax(e):
        # r3 = the buffer, r4 = the manager, r5 = the word the answer goes to
        L = e.cl
        out = e.r(5)
        L.rec("Mgr.GetMaxNumberOfChannels", (L.lab(e.r(4)), L.lab(out), e.u32(out)))
        e.write(out, pack([L.ret("Mgr.Max.value", 0)]))
        e.write(e.r(3), pack([L.ret("Mgr.Max", 0)]))

    def mgr_qsound(e):
        L = e.cl
        L.rec("Mgr.SetQSoundInfo", (L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK))
        e.write(e.r(3), pack([L.ret("Mgr.QSound", 0)]))

    def mgr_droppable(e):
        e.cl.rec("Mgr.SetDroppable", (e.cl.lab(e.r(3)), e.r(4) & 0xFF))

    def mgr_update(e):
        L = e.cl
        L.rec("Mgr.UpdateVolumeControl", (L.lab(e.r(4)), e.r(5) & 0xFF, f32_of(e.f(1))))
        e.write(e.r(3), pack([L.ret("Mgr.Update", 0)]))

    def mgr_toggle(e):
        L = e.cl
        L.rec("Mgr.ToggleVolumeControl", (L.lab(e.r(4)), e.r(5) & 0xFF, e.r(6) & MASK))
        e.write(e.r(3), pack([L.ret("Mgr.Toggle", 0)]))

    def mgr_ds(e):
        L = e.cl
        out = e.r(5)
        L.rec("Mgr.GetDirectSoundObject", (L.lab(e.r(4)), L.lab(out), e.u32(out)))
        e.write(out, pack([L.ret("Mgr.DS.value", 0)]))
        e.write(e.r(3), pack([L.ret("Mgr.DS", 0)]))

    def mgr_getvolume(e):
        L = e.cl
        L.rec("Mgr.GetVolume", (L.lab(e.r(3)), e.r(4) & 0xFF))
        e.set_f(1, bits_to_float(L.ret("Mgr.Volume", 0)))

    def cvt(e):
        L = e.cl
        hi, lo = f64_halves(e.f(1))
        L.rec("CvtDblUl", (hi, lo))
        e.set_r(3, T.u32(L.ret("CvtDblUl", 0)))

    def tpbody_tick(e):
        e.cl.rec("TpBody.Tick", (e.cl.lab(e.r(3)),))

    def tpbody_save(e):
        L = e.cl
        L.rec("TpBody.Save", (L.lab(e.r(4)), e.r(5) & MASK))
        e.write(e.r(3), pack([L.ret("TpBody.Save", 0)]))

    def tpbody_load(e):
        L = e.cl
        L.rec("TpBody.Load", (L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & 0xFF))
        e.write(e.r(3), pack([L.ret("TpBody.Load", 0)]))

    for address, fn in {
        LIST_PROCESS: list_process, LIST_STOPALL: list_stopall, LIST_GETCH: list_getch, LIST_SETCH: list_setch,
        LIST_SETRECT: list_setrect, LIST_GETPH: list_getph, LIST_FIND: list_find, HOLDER_SETVOLUME: holder_setvolume,
        HOLDER_SETVOLUMEPITCH: holder_setvolumepitch, MGR_SETDISTANCE: mgr_setdistance, MGR_GETMAX: mgr_getmax,
        MGR_QSOUND: mgr_qsound, MGR_DROPPABLE: mgr_droppable, MGR_UPDATE: mgr_update, MGR_TOGGLE: mgr_toggle, MGR_DS: mgr_ds,
        MGR_GETVOLUME: mgr_getvolume, CVTDBLUL: cvt, TPBODY_TICK: tpbody_tick, TPBODY_SAVE: tpbody_save, TPBODY_LOAD: tpbody_load,
    }.items():
        T.stub_internal(e, address, fn)

    # ---- the tables
    def mgr_getsysteminfo(e):
        # r3 = the buffer, r4 = the manager, r5 = the info
        L = e.cl
        info = e.r(5)
        L.rec("Mgr.GetSystemInfo", (L.lab(e.r(4)), e.u32(info + 8)))
        e.write(info + 0x18, pack([L.ret("Mgr.Info.mask", 0)]))
        e.write(info + 0x44, pack([L.ret("Mgr.Info.playing", 0)]))
        e.write(e.r(3), pack([L.ret("Mgr.Info", 0)]))

    def mgr_command(e):
        # r3 = the buffer, r4 = the manager, r5 = the command
        L = e.cl
        cmd = e.r(5)
        kind = COMMAND_KINDS.get(e.u32(cmd), -1)
        if kind == 5:
            rec_info(L, e, "Mgr.SetSystemInfo", L.lab(e.r(4)), e.u32(cmd + 4))
        elif kind == 6:
            info = e.u32(cmd + 8)
            flags = e.u32(info + 0x14)
            L.rec("Mgr.SetSampleInfo", (L.lab(e.r(4)), e.u32(cmd + 4), flags, e.u32(info + 0x1C) if flags & 2 else 0,
                                        e.u32(info + 0x24) if flags & 0x20 else 0))
        L.rec("Mgr.Command", (L.lab(e.r(4)), kind))
        e.write(e.r(3), pack([L.ret("Mgr.Command", 0)]))

    VT["mgr"] = T.persist_alloc(e, 0x20, pack([0, 0, fake(mgr_getsysteminfo), 0, 0, 0, fake(mgr_command), 0]))

    def holder_pause(e):
        L = e.cl
        L.rec("Holder.Pause", (L.lab(e.r(4)),))
        e.write(e.r(3), pack([L.ret("Holder.Pause", 0)]))

    def holder_restart(e):
        L = e.cl
        L.rec("Holder.Restart", (L.lab(e.r(4)),))
        e.write(e.r(3), pack([L.ret("Holder.Restart", 0)]))

    VT["holder"] = T.persist_alloc(e, 0x30, pack([0] * 9 + [fake(holder_pause), fake(holder_restart), 0]))

    def body_stop(e):
        L = e.cl
        L.rec("Body.StopHandle", (L.lab(e.r(4)), e.r(5) & MASK))
        e.write(e.r(3), pack([L.ret("Body.StopHandle", 0)]))

    def body_dereg(e):
        L = e.cl
        L.rec("Body.DeregisterCatagoryBanks", (L.lab(e.r(4)), e.r(5) & MASK))
        e.write(e.r(3), pack([L.ret("Body.Dereg", 0)]))

    # the body's table: the real one with slots +0x34 and +0x38 replaced by recording functions
    table = bytearray(e.read(BODY_VT, 0x40))
    table[0x34:0x38] = pack([fake(body_stop)])
    table[0x38:0x3C] = pack([fake(body_dereg)])
    VT["body"] = T.persist_alloc(e, 0x40, bytes(table))


# ---------------------------------------------------------------- the case

def case(op, **kw):
    c = dict(op=op, w=[0, 0, 0, 0, 0], lst=1, sample=1, length=0, q={})
    c.update(kw)
    c["w"] = list(c["w"]) + [0] * (5 - len(c["w"]))
    c["q"] = {k: v if isinstance(v, list) else [v] for k, v in c["q"].items()}
    return c


def params(c):
    p = [0] * 16
    p[P_OP] = IDX[c["op"]]
    p[P_W:P_W + 5] = c["w"]
    p[P_LIST] = c["lst"]
    p[P_SAMPLE] = c["sample"]
    p[P_LENGTH] = c["length"]
    return [v & MASK for v in p]


def ppc_run(e, c):
    name = c["op"]
    op = IDX[name]
    address, layout = OPS[op][1], OPS[op][2]
    L = e.cl = CL.PpcLog({}, c["q"])
    L.track = False
    e.info_kind = 1 if name == "SetSpeakerConfig" else 0
    # the world
    w = e.w = type("W", (), {})()
    lst = e.alloc(0x30, b"\xa5" * 0x30)
    L.labels[lst] = 4
    sample = e.alloc(0x10, b"\xa5" * 0x10)
    e.write(sample + 8, pack([c["length"]]))
    L.labels[sample] = 7
    w.holder = e.alloc(0x60, b"\xa5" * 0x60)
    e.write(w.holder, pack([VT["holder"]]))
    e.write(w.holder + 0xC, pack([sample if c["sample"] else 0]))
    L.labels[w.holder] = 5
    mgr = e.alloc(0x70, b"\xa5" * 0x70)
    e.write(mgr, pack([VT["mgr"]]))
    L.labels[mgr] = 2
    body = e.alloc(0x50, b"\xa5" * 0x50)
    e.write(body, pack([VT["body"]]))
    e.write(body + 0x28, pack([lst if c["lst"] else 0]))
    e.write(body + 0x2C, pack([mgr]))
    L.labels[body] = 1
    cfg = e.alloc(16, b"\xa5" * 16)
    L.labels[cfg] = 10
    out_word = e.alloc(4, pack([FILL]))
    L.labels[out_word] = 11
    wd = c["w"]
    # the command object, as the game or the library lays it out
    words = {"": [], "w1": wd[:1], "w2": wd[:2], "w3": wd[:3], "w5": wd[:5], "out": [out_word], "outh": [out_word, wd[1]],
             "cfg": [cfg], "file": wd[:1], "load": [wd[0], ((wd[1] & 0xFF) << 24) | 0xA5A5A5],
             "acc:w": wd[:1], "acc:f": wd[:1], "acc:out": [out_word]}[layout]
    cmd = e.alloc(0x20, b"\xa5" * 0x20)
    e.write(cmd, pack([0xC0C0C0C0] + words))
    L.labels[cmd] = 9
    buf = e.alloc(4, pack([FILL]))
    L.track = True
    result, high = 0, 0
    if layout.startswith("acc:"):
        r3, _, f1 = T.call_ppc(e, address, [cmd])
        if layout == "acc:f":
            result = f32_of(f1)
        elif layout == "acc:out":
            result = L.lab(r3) & MASK
        else:
            result = r3 & MASK
        if name == "SetValue::GetTbHandleSysValue":
            high = 0
    else:
        T.call_ppc(e, address, [buf, cmd, body])
        result = T.s32(e.u32(buf)) & MASK
    L.track = False
    out = [result & MASK, e.u32(out_word), high]
    return dict(out=tuple(out), log=list(L.log))


def host_run(lib, c):
    p = (C.c_uint32 * 16)(*params(c))
    out = (C.c_uint32 * 16)()
    CL.host_begin(lib, {}, c["q"])
    lib.tpw_sc_run(p, out)
    log = CL.host_end(lib)
    return dict(out=tuple(out[:3]), log=log)


# ---------------------------------------------------------------- the cases

def rq(r, *keys):
    return {key: [r.choice(Q_POOLS[key])] for key in keys}


def make_random(name):
    def rnd(r):
        w = [r.choice(POOL32) if r.random() < 0.8 else r.getrandbits(32) for _ in range(5)]
        if name in VOLUME_SETTERS or name in TOGGLES:
            w[0] = r.choice(VOLUMES) if r.random() < 0.8 else r.getrandbits(32)
        if name in ("SetDistanceMapping", "SetValue::GetFloatSysValue"):
            w[0] = r.choice(FLOATS) if r.random() < 0.8 else r.getrandbits(32) & 0xFF7FFFFF
        if name in ("SetHandleVolume", "SetHandleVolumeAndPitch", "StopHandle", "PauseHandle", "RestartHandle",
                    "SetBoundingRectangle", "SetValue::GetTbHandleSysValue"):
            w[0] = r.choice(HANDLES) if r.random() < 0.8 else r.getrandbits(32)
        if name in ("SetHandleVolumeAndPitch",):
            w[2] = r.choice(PITCHES) & MASK
        if name == "GetSampleLength":
            w[1] = r.choice(HANDLES)
        q = {key: [r.choice(pool)] for key, pool in Q_POOLS.items()}
        return case(name, w=w, lst=r.choice([0, 1, 1]), sample=r.randrange(2), length=r.choice(POOL32), q=q)
    return rnd


VOLUME_SETTERS = ["SetMasterVolume", "SetSpeechVolume", "SetUser1Volume", "SetUser2Volume", "SetSFXVolume", "SetMusicVolume"]
TOGGLES = ["ToggleMaster", "ToggleSpeech", "ToggleUser1", "ToggleUser2", "ToggleSFX", "ToggleMusic"]
NOTHING = ["StopAllWithFade", "SetFades", "SetHalfRate", "SetEAX", "SetGeometryInfo", "StopHandleWithFade", "SetPresetReverb",
           "RegisterObject", "DeleteObject", "DeleteAllObjects"]


def edge_cases(name):
    def edges():
        cs = []
        if name in ("Process", "StopAll", "PauseAll", "RestartAll", "SetSpeakerConfig"):
            for err in QUEUE_ERRORS:
                cs.append(case(name, q={"Mgr.Command": err}))
        if name in NOTHING:
            for v in POOL32:
                cs.append(case(name, w=[v]))
        if name == "SetDistanceMapping":
            for bits in FLOATS:
                cs.append(case(name, w=[bits]))
        if name == "SetBoundingRectangle":
            for handle in HANDLES:
                for err in (0, -1, 7):
                    cs.append(case(name, w=[handle, 1, 2, 3, 4], q={"List.SetRect": err}))
            cs.append(case(name, w=[5, 0x80000000, 0x7FFFFFFF, 0xFFFFFFFF, 0]))
            cs.append(case(name, w=[5, 4, 3, 2, 1]))
        if name == "SetNumberOfChannels":
            for got in CHANNELS:
                for want in CHANNELS:
                    cs.append(case(name, w=[want], q={"List.GetCh": got, "List.SetCh": 0x1E, "Mgr.Command": 0}))
            for used in CHANNELS:
                cs.append(case(name, w=[3], q={"List.GetCh": 2, "List.SetCh": used, "Mgr.Command": 0}))
            for err in QUEUE_ERRORS:
                cs.append(case(name, w=[3], q={"List.GetCh": 2, "List.SetCh": 3, "Mgr.Command": err}))
        if name == "GetMaxNumberOfChannels":
            for value in POOL32:
                for err in QUEUE_ERRORS:
                    cs.append(case(name, q={"Mgr.Max.value": value, "Mgr.Max": err}))
        if name == "GetNumberOfPlayingChannels":
            for mask in MASKS:
                for playing in (0, 7, 0xFFFFFFFF):
                    for err in (0, -1):
                        cs.append(case(name, q={"Mgr.Info.mask": mask, "Mgr.Info.playing": playing, "Mgr.Info": err}))
        if name == "FlushCatagory":
            for cat in POOL32:
                for err in QUEUE_ERRORS:
                    cs.append(case(name, w=[cat], q={"Body.Dereg": err}))
        if name == "SetQSoundInfo":
            for a in (0, 1, 2, 0xFFFFFFFF):
                for b in (0, 1, 0x80000000):
                    for c3 in (0, 1, 0x100):
                        cs.append(case(name, w=[a, b, c3], q={"Mgr.QSound": 0}))
            for err in QUEUE_ERRORS:
                cs.append(case(name, w=[1, 0, 1], q={"Mgr.QSound": err}))
        if name == "SetDroppable":
            for v in POOL32:
                cs.append(case(name, w=[v]))
        if name == "StopHandle":
            for handle in HANDLES:
                for err in QUEUE_ERRORS:
                    cs.append(case(name, w=[handle], q={"Body.StopHandle": err}))
        if name in ("SetHandleVolume", "SetHandleVolumeAndPitch"):
            for holder in (0, 1):
                for sample in SAMPLE_COOKIES:
                    for vol in (0, 1, 50, 100, 101, 0xFFFFFFFF, 0x80000000):
                        pitch = PITCHES[(vol + sample) % len(PITCHES)] & MASK
                        cs.append(case(name, w=[0x10001, vol, pitch], q={"List.GetPH.holder": holder, "List.GetPH.sample": sample}))
            for handle in HANDLES:
                cs.append(case(name, w=[handle, 7, 9], q={"List.GetPH.holder": 1, "List.GetPH.sample": 0x1234}))
            for err in QUEUE_ERRORS:
                cs.append(case(name, w=[5, 70, 3], q={"List.GetPH.holder": 1, "List.GetPH.sample": 0x1234, "Mgr.Command": err}))
        if name in VOLUME_SETTERS:
            for v in VOLUMES:
                cs.append(case(name, w=[v]))
            for err in QUEUE_ERRORS:
                cs.append(case(name, w=[50], q={"Mgr.Update": err}))
        if name in TOGGLES:
            for v in POOL32:
                cs.append(case(name, w=[v]))
            for err in QUEUE_ERRORS:
                cs.append(case(name, w=[1], q={"Mgr.Toggle": err}))
        if name == "GetDirectSoundObject":
            for value in POOL32:
                for err in QUEUE_ERRORS:
                    cs.append(case(name, q={"Mgr.DS.value": value, "Mgr.DS": err}))
        if name == "GetSampleLength":
            for lst in (0, 1):
                for find in (0, 1):
                    for sample in (0, 1):
                        for length in (0, 1000, 0x7FFFFFFF, 0xFFFFFFFF):
                            cs.append(case(name, w=[0, 0x10001], lst=lst, sample=sample, length=length, q={"List.Find": find}))
            for handle in HANDLES:
                cs.append(case(name, w=[0, handle], length=123, q={"List.Find": 1}))
        if name in ("GetSFXVolume", "GetMusicVolume"):
            for bits in FLOATS:
                for result in (0, 100, 0xFFFFFFFF):
                    cs.append(case(name, q={"Mgr.Volume": bits, "CvtDblUl": result}))
        if name == "Save":
            for file in POOL32:
                for err in QUEUE_ERRORS:
                    cs.append(case(name, w=[file], q={"TpBody.Save": err}))
        if name == "Load":
            for file in POOL32[:6]:
                for flag in (0, 1, 2, 0x80, 0xFF, 0x100, 0xFFFFFF01):
                    for err in (0, -1):
                        cs.append(case(name, w=[file, flag], q={"TpBody.Load": err}))
        if name in ("PauseHandle", "RestartHandle"):
            key = "Holder.Pause" if name == "PauseHandle" else "Holder.Restart"
            for handle in HANDLES:
                for find in (0, 1):
                    for err in QUEUE_ERRORS:
                        cs.append(case(name, w=[handle], q={"List.Find": find, key: err}))
        if name in ("SetValue::GetSysValue", "SetValue::GetTbHandleSysValue"):
            for v in POOL32 + HANDLES:
                cs.append(case(name, w=[v]))
        if name == "SetValue::GetFloatSysValue":
            for bits in FLOATS:
                cs.append(case(name, w=[bits]))
        if name in ("GetValue::GetSysValue",):
            cs.append(case(name))
        if name == "Serialise::GetHandle":
            for v in POOL32 + HANDLES:
                cs.append(case(name, w=[v]))
        if name in ("Tick", "Process", "StopAll", "PauseAll", "RestartAll"):
            cs.append(case(name))
        return cs
    return edges


def sys_tests():
    tests = []
    for name, address, _layout in OPS:
        tests.append(FnTest("%s 0x%x" % (name if "::" in name else name + "::Execute", address), ppc_run, host_run,
                            edge=edge_cases(name), rnd=make_random(name)))
    return tests


TESTS = sys_tests()
