"""Unit w66, the sound runtime, against libtpw_unit_SoundRuntime.dylib (tests/unit_SoundRuntime.cpp), in the main app
("SimTheme Park"):
  src/SoundRuntime.cpp  SOUND_Init 0x100dc940, SOUND_Close 0x100dccd4, SOUND_SetChannels 0x100dcd80, SOUND_SetSoundQuality
                        0x100dcf0c, the volume setters 0x100dd1b4 to 0x100dd278, SOUND_SetParamValue 0x100dd380, StopAllSfx
                        0x100dd41c, FadeOutAllSfx 0x100dd4b0, Pause 0x100dd548, UnPause 0x100dd5d8, SOUND_Tick 0x100dd668,
                        SOUND_SendEvent 0x100dd9fc, PauseSound 0x100ddb18, UnpauseSound 0x100ddbb8, SetVolumeOnSound 0x100ddc5c,
                        SendEventExt 0x100ddd08, SetPauseOn3DSoundsOnly 0x100dde38, SetEarPosition 0x100dde7c, MoveSoundEffect
                        0x100ddf60, StopSoundEffect 0x100de058, FadeSoundEffect 0x100de0fc,
                        GetCurrentSampleLengthInMilliseconds 0x100de758, AddObject 0x100de8e4, DelObject 0x100de918,
                        SetOcclusionProcessing 0x100de944, SetReverbLevel 0x100de97c, SetDroppableLevel 0x100de9ac
  include/TbAudioSystem.h  the TbAudioEvent getters 0x100de9e0, 0x100dea18, 0x100dea54, 0x100dea8c
  src/TPSound.cpp       the state (the status setters and SND_DuckVolume are called by the scripts but are not part of the unit)

The functions run as the original, one script of operations per case, with the state (the three flags, the allocation counter,
the bank block, the settings block and the keyboard) read after every operation. Every callee outside the unit is a fake that
records the same event on both sides: the audio object (a fake object in persistent PowerPC memory whose vtable has the slots the
game calls: +0x08 Release, +0x0c SetListenerOrientation, +0x14 SendEvent, +0x18 UpdateHandle, +0x1c UpdateParameter, +0x20
SystemCommand; the host's is a TbAudioSystem), CreateAudioObject (the queue "Create"), GAME_ASSERTMSG, the sams keyboard (a
persistent object whose vtable has +0x10 IsOptionDown and +0x18 IsCommandDown) and USoundChannel::SetSafeMode. A command object
SystemCommand receives is compared by its kind, which the PowerPC side finds from the vtable word the original stores (the sound
library's import vtable of that command class), and by its payload words. The event SendEvent receives is compared by its words
(the vtable word must be main's TbAudioEvent vtable 0x102a0158). gSoundBalance's value slots are set per case.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_SoundRuntime"
MASK = 0xFFFFFFFF

# the functions
ADDR = {
    "Init": 0x100DC940, "Close": 0x100DCCD4, "SetChannels": 0x100DCD80, "SetSoundQuality": 0x100DCF0C,
    "SetGlobalVolume": 0x100DD1B4, "SetMusicVolume": 0x100DD1F8, "SetSfxVolume": 0x100DD238, "SetSpeechVolume": 0x100DD278,
    "SetMusicStatus": 0x100DD2BC, "SetSfxStatus": 0x100DD2FC, "SetSpeechStatus": 0x100DD33C, "SetParamValue": 0x100DD380,
    "StopAllSfx": 0x100DD41C, "FadeOutAllSfx": 0x100DD4B0, "Pause": 0x100DD548, "UnPause": 0x100DD5D8, "Tick": 0x100DD668,
    "SendEvent": 0x100DD9FC, "PauseSound": 0x100DDB18, "UnpauseSound": 0x100DDBB8, "SetVolumeOnSound": 0x100DDC5C,
    "SendEventExt": 0x100DDD08, "SetPauseOn3DSoundsOnly": 0x100DDE38, "SetEarPosition": 0x100DDE7C,
    "MoveSoundEffect": 0x100DDF60, "StopSoundEffect": 0x100DE058, "FadeSoundEffect": 0x100DE0FC,
    "GetCurrentSampleLength": 0x100DE758, "AddObject": 0x100DE8E4, "DelObject": 0x100DE918,
    "SetOcclusionProcessing": 0x100DE944, "SetReverbLevel": 0x100DE97C, "SetDroppableLevel": 0x100DE9AC,
    "DuckVolume": 0x100DEAC8, "GetType": 0x100DE9E0, "GetCategory": 0x100DEA18, "GetEvent": 0x100DEA54,
    "GetPosition": 0x100DEA8C,
}
# the operations of a script: (operation, args, text); the numbers are those of tests/unit_SoundRuntime.cpp
(OP_INIT, OP_CLOSE, OP_SETCHANNELS, OP_SETQUALITY, OP_GLOBALVOL, OP_MUSICVOL, OP_SFXVOL, OP_SPEECHVOL, OP_PARAM, OP_STOPALL,
 OP_FADEALL, OP_PAUSE, OP_UNPAUSE, OP_TICK, OP_SENDEVENT, OP_PAUSESOUND, OP_UNPAUSESOUND, OP_VOLONSOUND, OP_SENDEVENTEXT,
 OP_PAUSE3D, OP_EAR, OP_MOVE, OP_STOPSOUND, OP_FADESOUND, OP_SAMPLELEN, OP_OCCLUSION, OP_REVERB, OP_DROPPABLE, OP_ADDOBJECT,
 OP_DELOBJECT, OP_MUSICSTATUS, OP_SFXSTATUS, OP_SPEECHSTATUS, OP_DUCK) = range(34)
OP_GETTYPE, OP_GETCATEGORY, OP_GETEVENT, OP_GETPOSITION = 60, 61, 62, 63
OP_ADDRESS = {
    OP_INIT: "Init", OP_CLOSE: "Close", OP_SETCHANNELS: "SetChannels", OP_SETQUALITY: "SetSoundQuality",
    OP_GLOBALVOL: "SetGlobalVolume", OP_MUSICVOL: "SetMusicVolume", OP_SFXVOL: "SetSfxVolume", OP_SPEECHVOL: "SetSpeechVolume",
    OP_PARAM: "SetParamValue", OP_STOPALL: "StopAllSfx", OP_FADEALL: "FadeOutAllSfx", OP_PAUSE: "Pause", OP_UNPAUSE: "UnPause",
    OP_TICK: "Tick", OP_SENDEVENT: "SendEvent", OP_PAUSESOUND: "PauseSound", OP_UNPAUSESOUND: "UnpauseSound",
    OP_VOLONSOUND: "SetVolumeOnSound", OP_SENDEVENTEXT: "SendEventExt", OP_PAUSE3D: "SetPauseOn3DSoundsOnly",
    OP_EAR: "SetEarPosition", OP_MOVE: "MoveSoundEffect", OP_STOPSOUND: "StopSoundEffect", OP_FADESOUND: "FadeSoundEffect",
    OP_SAMPLELEN: "GetCurrentSampleLength", OP_OCCLUSION: "SetOcclusionProcessing", OP_REVERB: "SetReverbLevel",
    OP_DROPPABLE: "SetDroppableLevel", OP_ADDOBJECT: "AddObject", OP_DELOBJECT: "DelObject", OP_MUSICSTATUS: "SetMusicStatus",
    OP_SFXSTATUS: "SetSfxStatus", OP_SPEECHSTATUS: "SetSpeechStatus", OP_DUCK: "DuckVolume", OP_GETTYPE: "GetType",
    OP_GETCATEGORY: "GetCategory", OP_GETEVENT: "GetEvent", OP_GETPOSITION: "GetPosition",
}
FLOAT_OPS = {OP_EAR: 9, OP_ADDOBJECT: 5}      # the operations whose arguments are all floats (as bits): how many
RESULT_OPS = {OP_INIT, OP_PARAM, OP_SENDEVENT, OP_SENDEVENTEXT, OP_MOVE, OP_SAMPLELEN, OP_ADDOBJECT, OP_GETTYPE, OP_GETCATEGORY,
              OP_GETEVENT}

# the callees and the data
GAME_ASSERTMSG = 0x1000DDE8
CREATE_AUDIO_OBJECT = "CreateAudioObject__FPCcPP13TbAudioSystemRC12CAudioConfig"
KEYBOARD_CTOR = "__ct__Q213SamsUtilities9UKeyboardFv"
SET_SAFE_MODE = "SetSafeMode__Q213SamsUtilities13USoundChannelFUc"
ALLOCS, AUDIO, WANTED, ON, SETTINGS, BLOCK = 0x102F7C30, 0x102F7C34, 0x102F7C38, 0x102F7C3C, 0x102F7C40, 0x102F7C94
BALANCE = 0x102B4E2C                 # gSoundBalance: value slot n is the word at +0x08 + 4 n
KEYBOARD_VARIABLE = "gKeyboard__13SamsUtilities"   # SamsUtilities::gKeyboard, an import variable (the emulator gives it a buffer)
EVENT_VTABLE_WORD = 0x102A0158
LABEL_AUDIO, LABEL_KEYBOARD = 2, 3

# the command classes: kind number -> (name, payload spec). The sound library's import vtable of the class is the first word of a
# command object (the data import "__vt__Q212TbSysCommand<length><name>", whose buffer the emulator gives the TOC word).
# payload spec: i = a word, p = a pointer to a word the library writes (recorded as set or not)
COMMANDS = {
    1: ("Tick", ""), 2: ("Process", ""), 3: ("SetMasterVolume", "i"), 4: ("SetMusicVolume", "i"), 5: ("SetUser2Volume", "i"),
    6: ("SetSpeechVolume", "i"), 7: ("ToggleSFX", "i"), 8: ("ToggleMusic", "i"), 9: ("ToggleSpeech", "i"),
    10: ("SetDistanceMapping", "i"), 11: ("SetQSoundInfo", "iii"), 12: ("SetEAX", "i"), 13: ("SetHalfRate", "i"),
    14: ("SetFades", "i"), 15: ("SetNumberOfChannels", "i"), 16: ("GetMaxNumberOfChannels", "p"), 17: ("PauseAll", ""),
    18: ("RestartAll", ""), 19: ("StopAll", ""), 20: ("PauseHandle", "i"), 21: ("RestartHandle", "i"), 22: ("StopHandle", "i"),
    23: ("SetHandleVolume", "ii"), 24: ("GetSampleLength", "pi"),
}
KIND_OF_WORD = {}      # filled by setup: the vtable word of each command class -> its kind

# the value slots of gSoundBalance the unit reads (see src/SoundRuntime.cpp)
S_QSOUND, S_REAR, S_CROSSOVER, S_EAX, S_MPEG, S_DUCK, S_DROPPABLE, S_FADE = 1, 2, 3, 4, 6, 7, 8, 10
S_CH0, S_SW0, S_CH1, S_SW1, S_CH2, S_SW2 = 16, 17, 18, 19, 20, 21
BALANCE_SLOTS = [S_QSOUND, S_REAR, S_CROSSOVER, S_EAX, S_MPEG, S_DUCK, S_DROPPABLE, S_FADE, S_CH0, S_SW0, S_CH1, S_SW1, S_CH2, S_SW2]
DEFAULT_BALANCE = {S_QSOUND: 20, S_REAR: 30, S_CROSSOVER: 40, S_EAX: 50, S_MPEG: 60, S_DUCK: 50, S_DROPPABLE: 10, S_FADE: 70,
                   S_CH0: 8, S_SW0: 30, S_CH1: 12, S_SW1: 60, S_CH2: 16, S_SW2: 80}

# the state: 0 allocations, 1 audio object, 2 wanted, 3 on, 4 to 11 banks, 12 advisor bank, 13 music bank, 14 thunder bank,
# 15 island table (0 or 1), 16 island count, 17 to 38 the settings (entries 0 to 11: the words at 0x00 to 0x2c, 12 and 13: the
# shorts at 0x30 and 0x32, 14 to 21: the words at 0x34 to 0x50), 39 the keyboard
STATE_WORDS = 40
(E_GLOBAL, E_SFX, E_MUSIC, E_SPEECH, E_CHANGED, E_DUCK, E_MUTE, E_PAUSE3D) = (6, 7, 8, 9, 10, 11, 12, 13)   # settings entries
(E_ACTIVE, E_F14, E_SFXON, E_MUSICON, E_SPEECHON) = (2, 5, 15, 16, 17)
SETTINGS_ENTRIES = 22


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


STUBS = None
AUDIO_OBJECT = None
KEYBOARD_VTABLE = None
KEYBOARD_OBJECT = None


def make_audio(e):
    """The fake audio object in persistent memory: a vtable with the six slots the game calls."""
    def release(e):
        # r3 the hidden result buffer, r4 this
        e.cl.rec("Audio.Release", (e.cl.lab(e.r(4)),))

    def listener(e):
        # r3 hidden, r4 this, r5 position, r6 forward, r7 up, r8 velocity
        L = e.cl
        v = [[e.u32(e.r(r) + 4 * i) for i in range(3)] for r in (5, 6, 7, 8)]
        L.rec("Audio.SetListener", (L.lab(e.r(4)),) + tuple(v[0]) + tuple(v[1]))
        L.rec("Audio.SetListener2", tuple(v[2]) + tuple(v[3]))

    def send_event(e):
        # r3 this, r4 the entity, r5 the event
        L = e.cl
        ev = e.r(5)
        L.rec("Audio.SendEvent", (L.lab(e.r(3)), s32(e.r(4)), int(e.u32(ev) == EVENT_VTABLE_WORD), s32(e.u32(ev + 4)),
                                  s32(e.u32(ev + 8)), e.u32(ev + 0xC), e.u32(ev + 0x10), e.u32(ev + 0x14), e.u32(ev + 0x18)))
        e.set_r(3, L.ret("Handle", 0) & MASK)

    def update_handle(e):
        # r3 this, r4 the handle, r5 position, r6 velocity
        L = e.cl
        pos, vel = e.r(5), e.r(6)
        L.rec("Audio.UpdateHandle", (L.lab(e.r(3)), s32(e.r(4))) + tuple(e.u32(pos + 4 * i) for i in range(3)) +
              tuple(e.u32(vel + 4 * i) for i in range(3)))
        e.set_r(3, L.ret("Word", 0) & MASK)

    def update_parameter(e):
        # r3 this, r4 the handle, r5 the parameter, r6 the value
        L = e.cl
        L.rec("Audio.UpdateParameter", (L.lab(e.r(3)), s32(e.r(4)), e.r(5), e.r(6)))
        e.set_r(3, L.ret("Word", 0) & MASK)

    def command(e):
        # r3 hidden, r4 this, r5 the command: its vtable word and its payload words
        L = e.cl
        cmd = e.r(5)
        kind = KIND_OF_WORD.get(e.u32(cmd), 99)
        payload = []
        if kind != 99:
            for i, c in enumerate(COMMANDS[kind][1]):
                word = e.u32(cmd + 4 + 4 * i)
                if c == "i":
                    payload.append(s32(word))
                else:
                    payload.append(1 if word != 0 else 0)
                    if word != 0 and L.ret("Skip", 0) == 0:
                        value = L.ret("Max", 0x20) if kind == 16 else L.ret("Length", 1234)
                        ws(e, word, [value])
        payload += [0] * (3 - len(payload))
        L.rec("Audio.SystemCommand", (L.lab(e.r(4)), kind) + tuple(payload))

    vt = T.persist_alloc(e, 0x40)
    for slot, fn in ((0x08, release), (0x0C, listener), (0x14, send_event), (0x18, update_handle), (0x1C, update_parameter),
                     (0x20, command)):
        ws(e, vt + slot, [T.fake_function(e, fn)])
    return T.persist_alloc(e, 16, struct.pack(">I", vt) + b"\0" * 12)


def make_keyboard(e):
    """The keyboard's vtable (+0x10 IsOptionDown, +0x18 IsCommandDown) and the object that exists before a case."""
    def option(e):
        L = e.cl
        L.rec("Keyboard.IsOptionDown", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("Option", 0) & MASK)

    def command_down(e):
        L = e.cl
        L.rec("Keyboard.IsCommandDown", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("Command", 0) & MASK)

    vt = T.persist_alloc(e, 0x40)
    ws(e, vt + 0x10, [T.fake_function(e, option)])
    ws(e, vt + 0x18, [T.fake_function(e, command_down)])
    return vt, T.persist_alloc(e, 16, struct.pack(">I", vt) + b"\0" * 12)


def build_stubs():
    S = CL.Stubs()

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def create(e):
        # r3 the hidden result, r4 the name, r5 where the object goes, r6 the config
        L = e.cl
        config = e.r(6)
        quality = struct.unpack(">q", e.read(config, 8))[0]
        L.rec("CreateAudioObject", (s32(e.u32(config + 8)), quality), (e.cstr(e.r(4))[:159],))
        answer = s32(L.ret("Create", 0))
        if answer >= 0:
            ws(e, e.r(5), [AUDIO_OBJECT])
        ws(e, e.r(3), [answer])

    def keyboard_ctor(e):
        L = e.cl
        this = e.r(3)
        L.rec("UKeyboard.ctor", (L.newlab(this),))
        ws(e, this, [KEYBOARD_VTABLE])

    def safe_mode(e):
        e.cl.rec("SetSafeMode", (e.r(3) & 0xFF,))

    S.internal(GAME_ASSERTMSG, assert_)
    S.imp(CREATE_AUDIO_OBJECT, create)
    S.imp(KEYBOARD_CTOR, keyboard_ctor)
    S.imp(SET_SAFE_MODE, safe_mode)
    return S


def setup(runner):
    global STUBS, AUDIO_OBJECT, KEYBOARD_VTABLE, KEYBOARD_OBJECT
    T.install_std_stubs(runner.emu)
    KIND_OF_WORD.clear()
    for kind, (name, _) in COMMANDS.items():
        KIND_OF_WORD[runner.emu.data_import("__vt__Q212TbSysCommand%d%s" % (len(name), name))] = kind
    STUBS = build_stubs()
    AUDIO_OBJECT = make_audio(runner.emu)
    KEYBOARD_VTABLE, KEYBOARD_OBJECT = make_keyboard(runner.emu)


# ---------------------------------------------------------------- the two sides

def cstr(e, text):
    return e.alloc(len(text) + 1, text.encode("latin-1") + b"\0")


def settings_address(entry):
    return SETTINGS + (4 * entry if entry < 12 else 0x30 + 2 * (entry - 12) if entry < 14 else 0x34 + 4 * (entry - 14))


def state_ppc(e, L):
    words = [s32(e.u32(ALLOCS)), int(e.u32(AUDIO) != 0), s32(e.u32(WANTED)), s32(e.u32(ON))]
    words += [s32(e.u32(BLOCK + 4 * i)) for i in range(8)]
    words += [s32(e.u32(BLOCK + 0x20)), s32(e.u32(BLOCK + 0x24)), s32(e.u32(BLOCK + 0x28)), int(e.u32(BLOCK + 0x2C) != 0),
              s32(e.u32(BLOCK + 0x30))]
    for entry in range(SETTINGS_ENTRIES):
        a = settings_address(entry)
        words.append(T.s16(struct.unpack(">H", e.read(a, 2))[0]) if entry in (12, 13) else s32(e.u32(a)))
    words.append(L.lab(e.u32(e.data_import(KEYBOARD_VARIABLE))))
    return words


def write_state(e, init):
    ws(e, ALLOCS, [init["allocs"]])
    ws(e, AUDIO, [AUDIO_OBJECT if init["audio"] else 0])
    ws(e, WANTED, [init["wanted"]])
    ws(e, ON, [init["on"]])
    e.write(BLOCK, b"\0" * 0x34)
    for index, value in init["banks"].items():
        ws(e, BLOCK + (4 * (index - 4)), [value])
    ws(e, BLOCK + 0x2C, [0x12345678 if init["island"] else 0])
    ws(e, BLOCK + 0x30, [init["count"]])
    e.write(SETTINGS, b"\0" * 0x54)
    for entry, value in init["settings"].items():
        a = settings_address(entry)
        if entry in (12, 13):
            e.write(a, struct.pack(">H", value & 0xFFFF))
        else:
            ws(e, a, [value])
    for slot in BALANCE_SLOTS:
        ws(e, BALANCE + 8 + 4 * slot, [init["balance"].get(slot, 0)])
    ws(e, e.data_import(KEYBOARD_VARIABLE), [KEYBOARD_OBJECT if init["keyboard"] else 0])


def ppc_case(e, case):
    e.heap_in_scratch = True
    fixed = {AUDIO_OBJECT: LABEL_AUDIO, KEYBOARD_OBJECT: LABEL_KEYBOARD}
    e.cl = L = CL.PpcLog(fixed, case.get("q"))
    saved = STUBS.activate(e)
    try:
        write_state(e, case["init"])
        news0 = len(e.news)
        steps = []
        for op, args, text in case["ops"]:
            result = 0
            address = ADDR[OP_ADDRESS[op]]
            if op in (OP_GETTYPE, OP_GETCATEGORY, OP_GETEVENT, OP_GETPOSITION):
                words = [EVENT_VTABLE_WORD, args[0], args[1], args[2], args[3], args[4], args[5]]
                obj = e.alloc(0x1C, struct.pack(">" + "I" * 7, *[w & MASK for w in words]))
                r3 = T.call_ppc(e, address, [obj])[0]
                if op == OP_GETPOSITION:
                    L.rec("Position", tuple(e.u32(r3 + 4 * i) for i in range(3)))   # (the address itself is a layout fact)
                else:
                    result = s32(r3)
            elif op in FLOAT_OPS:
                r3 = T.call_ppc(e, address, [T.F(T.bits_f32(a)) for a in args[:FLOAT_OPS[op]]])[0]
                result = s32(r3) if op in RESULT_OPS else 0
            elif op == OP_INIT:
                result = s32(T.call_ppc(e, address, [cstr(e, text), args[0] & MASK, args[1] & MASK])[0])
            else:
                count = ARG_COUNT[op]
                r3 = T.call_ppc(e, address, [a & MASK for a in args[:count]])[0]
                result = s32(r3) if op in RESULT_OPS else 0
            steps.append(dict(result=result, state=state_ppc(e, L)))
        return dict(steps=steps, log=L.log, news=[size for size, _ in e.news[news0:]])
    finally:
        CL.Stubs.deactivate(e, saved)


ARG_COUNT = {OP_CLOSE: 0, OP_SETCHANNELS: 1, OP_SETQUALITY: 1, OP_GLOBALVOL: 1, OP_MUSICVOL: 1, OP_SFXVOL: 1, OP_SPEECHVOL: 1,
             OP_PARAM: 3, OP_STOPALL: 0, OP_FADEALL: 0, OP_PAUSE: 0, OP_UNPAUSE: 0, OP_TICK: 0, OP_SENDEVENT: 6, OP_PAUSESOUND: 1,
             OP_UNPAUSESOUND: 1, OP_VOLONSOUND: 2, OP_SENDEVENTEXT: 7, OP_PAUSE3D: 1, OP_MOVE: 4, OP_STOPSOUND: 1, OP_FADESOUND: 1,
             OP_SAMPLELEN: 1, OP_OCCLUSION: 1, OP_REVERB: 1, OP_DROPPABLE: 1, OP_DELOBJECT: 1, OP_MUSICSTATUS: 1,
             OP_SFXSTATUS: 1, OP_SPEECHSTATUS: 1, OP_DUCK: 1}


def host_case(lib, case):
    init = case["init"]
    lib.tpw_sx_begin.argtypes = [C.c_int32]
    lib.tpw_sx_begin(init["keyboard"])
    lib.tpw_sx_put.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sx_put_balance.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sx_put(0, s32(init["allocs"]))
    lib.tpw_sx_put(1, init["audio"])
    lib.tpw_sx_put(2, s32(init["wanted"]))
    lib.tpw_sx_put(3, s32(init["on"]))
    for index, value in init["banks"].items():
        lib.tpw_sx_put(index, s32(value))
    lib.tpw_sx_put(15, init["island"])
    lib.tpw_sx_put(16, s32(init["count"]))
    for entry, value in init["settings"].items():
        lib.tpw_sx_put(17 + entry, s32(value))
    for slot in BALANCE_SLOTS:
        lib.tpw_sx_put_balance(slot, s32(init["balance"].get(slot, 0)))
    lib.tpw_sx_object.restype = C.c_void_p
    lib.tpw_sx_object.argtypes = [C.c_int32]
    fixed = {LABEL_AUDIO: lib.tpw_sx_object(LABEL_AUDIO), LABEL_KEYBOARD: lib.tpw_sx_object(LABEL_KEYBOARD)}
    CL.host_begin(lib, fixed, case.get("q"))
    lib.tpw_sx_op.restype = C.c_int64
    lib.tpw_sx_op.argtypes = [C.c_int32, C.c_void_p, C.c_char_p]
    lib.tpw_sx_get.restype = C.c_int32
    lib.tpw_sx_get.argtypes = [C.c_int32]
    steps = []
    for op, args, text in case["ops"]:
        padded = (C.c_int64 * 12)(*[a if a < (1 << 63) else a - (1 << 64) for a in (list(args) + [0] * 12)[:12]])
        result = lib.tpw_sx_op(op, C.addressof(padded), text.encode("latin-1") if text is not None else None)
        result = s32(result) if op in RESULT_OPS else 0
        steps.append(dict(result=result, state=[lib.tpw_sx_get(i) for i in range(STATE_WORDS)]))
    log = CL.host_end(lib)
    lib.tpw_sx_new_keyboards.restype = C.c_int32
    news = [0xC] * lib.tpw_sx_new_keyboards()
    lib.tpw_sx_end()
    return dict(steps=steps, log=log, news=news)


def run_ppc(emu, case):
    return ppc_case(emu, case)


# ---------------------------------------------------------------- the cases

def make_init(wanted=1, on=1, audio=1, allocs=0, settings=None, banks=None, island=0, count=0, balance=None, keyboard=0):
    return dict(wanted=wanted, on=on, audio=audio, allocs=allocs, settings=settings or {}, banks=banks or {}, island=island,
                count=count, balance=dict(DEFAULT_BALANCE if balance is None else balance), keyboard=keyboard)


def case(ops, queues=None, **kw):
    q = {key: list(values) for key, values in (queues or {}).items() if values is not None}
    return dict(ops=ops, init=make_init(**kw), q=q)


def op(kind, *args, text=None):
    return (kind, list(args) + [0] * (12 - len(args)), text)


def fb(x):
    """The bits of the single float x."""
    return T.f32_bits(x)


FLAGS = [(1, 1), (0, 0), (1, 0), (0, 1), (2, 3), (MASK, MASK), (0x100, 0x80000000), (0, MASK)]
QUALITIES = [0, 1, 2, 29, 30, 31, 49, 50, 51, 59, 60, 61, 79, 80, 81, 99, 100, 101, 1000, -1, -100, 0x7FFFFFFF, 0x80000000, MASK]
COORDS = [0, 1, -1, 2, 100, -100, 0x7FFFFFFF, 0x80000000, 16777216, 16777217, -16777217, 0x40000001, 0x7FFFFF80, 0x7FFFFFC0,
          12345678, 0x01000001, 0x03000003]
HANDLES = [0, 1, -1, 7, 0x7FFFFFFF, 0x80000000, 0x12345678, MASK]
FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 100.5, 1e30, -1e30, 1e-40, float("inf"), float("-inf"), 10000.0, 3.14159, 123456.789]

# a few settings of the balance table
def bal(overrides):
    """The default balance table with some slots changed."""
    return {**DEFAULT_BALANCE, **overrides}


BALANCES = {
    "default": DEFAULT_BALANCE,
    "equal01": bal({S_SW1: 30}),               # SWITCH 1 equals SWITCH 0: a divisor of 0
    "equal12": bal({S_SW2: 60}),               # SWITCH 2 equals SWITCH 1
    "hundred": bal({S_SW2: 100}),              # 100 - SWITCH 2 is 0
    "over": bal({S_SW2: 120}),                 # a negative divisor
    "big": bal({S_CH0: 20, S_CH1: 30, S_CH2: 40}),
    "negative": bal({S_CH0: -4, S_CH1: -2, S_CH2: -1}),
    "zero": {slot: 0 for slot in BALANCE_SLOTS},
    "low": bal({S_SW0: 0, S_SW1: 1, S_SW2: 2}),
    "high": bal({S_SW0: 98, S_SW1: 99, S_SW2: 100}),
    "reverse": bal({S_SW0: 70, S_SW1: 40, S_SW2: 10}),
}


def random_balance(r):
    pool = [0, 1, 2, 8, 12, 16, 30, 50, 60, 80, 99, 100, 101, -1, -50, MASK, 0x7FFFFFFF, 0x80000000]
    return {slot: s32(r.choice(pool) if r.random() < 0.6 else r.randrange(-5, 130)) for slot in BALANCE_SLOTS}


def random_word(r):
    return s32(r.choice([0, 1, 2, -1, 100, 0x7FFFFFFF, 0x80000000, 0x100, r.getrandbits(32), r.randrange(0, 200)]))


# ---- SOUND_Init

def init_overflows(balance, quality):
    """True when SOUND_Init's channel interpolation would convert a value above INT32_MAX (an infinite or huge quotient): the
    emulator's fctiwz answers 0x80000000 there where the processor saturates to 0x7fffffff (NOTES.md, 2026-10-10), so such cases
    cannot be compared; the host saturates (include/ppc_math.h)."""
    q = s32(quality)
    sw0, sw1, sw2 = (s32(balance.get(slot, 0)) for slot in (S_SW0, S_SW1, S_SW2))
    ch0, ch1, ch2 = (s32(balance.get(slot, 0)) for slot in (S_CH0, S_CH1, S_CH2))
    if sw2 < q:
        low, high, num, den = ch2, 16, s32(q - sw2), s32(100 - sw2)
    elif sw1 < q:
        low, high, num, den = ch1, ch2, s32(q - sw1), s32(sw2 - sw1)
    else:
        low, high, num, den = ch0, ch1, s32(q - sw0), s32(sw1 - sw0)
    if den == 0:
        fraction = float("nan") if num == 0 else float("inf") if num > 0 else float("-inf")
    else:
        fraction = num / den
    value = fraction * s32(high - low) + low
    return value >= 2147483000.0


def bad_init(case):
    """True for a script whose Init would hit init_overflows."""
    return any(op == OP_INIT and args[0] != 0 and init_overflows(case["init"]["balance"], args[1]) for op, args, _ in case["ops"])


def init_edges():
    for c in init_edges_all():
        if not bad_init(c):
            yield c


def init_edges_all():
    for wanted, on in FLAGS:
        yield case([op(OP_INIT, wanted, 50, text="snd")], on=on, queues={"Create": [0]})
        yield case([op(OP_INIT, wanted, 50, text="snd")], on=on, queues={"Create": [-1]}, audio=0)
    for create in ([0], [-1], [1], [0x7FFFFFFF], [-2147483648], [MASK]):
        yield case([op(OP_INIT, 1, 50, text="name")], on=0, queues={"Create": create}, audio=0)
        yield case([op(OP_INIT, 1, 50, text="name")], on=0, queues={"Create": create}, audio=1)
    for name, balance in BALANCES.items():
        for quality in QUALITIES:
            yield case([op(OP_INIT, 1, quality, text="q_" + name)], on=0, balance=balance, queues={"Create": [0]})
    # the fused multiply-add: operands where the product rounded alone and then added
    # truncates one higher than the fused result; branch 3 (quality at most SWITCH 1): (quality - SWITCH 0) / (SWITCH 1 - SWITCH 0)
    # * (CHANNELS 1 - CHANNELS 0) + CHANNELS 0
    for num, den, d, lo in ((-18, 27, -12, -5), (-63, 69, -23, -6), (-4, 24, -18, -1), (-50, 75, -6, -2), (-20, 21, -21, -6),
                            (-24, 80, -30, -5), (-88, -77, 7, -6), (20, 35, 7, -2), (6, 18, 15, -2), (-88, -66, 15, -4)):
        balance = bal({S_SW0: 0, S_SW1: den, S_SW2: 100, S_CH0: lo, S_CH1: lo + d})
        yield case([op(OP_INIT, 1, num, text="fma")], on=0, balance=balance, queues={"Create": [0]})
    # everything is cleared by the second Init, the first leaves the system on
    yield case([op(OP_INIT, 1, 50, text="a"), op(OP_INIT, 1, 60, text="b")], on=0, queues={"Create": [0, 0]})
    yield case([op(OP_INIT, 1, 50, text="a"), op(OP_CLOSE), op(OP_INIT, 1, 70, text="c")], on=0, queues={"Create": [0, 0]})
    yield case([op(OP_INIT, 1, 50, text="a")], on=1, queues={"Create": [0]},
               banks={4: 5, 5: 6, 6: 7, 7: 8, 8: 9, 9: 10, 10: 11, 11: 12, 12: 13, 13: 14, 14: 15}, island=1, count=3)
    # the clearing of the banks and the settings (Init after a failed first Init)
    junk = {i: s32(0x01010101 * (i + 1)) for i in range(SETTINGS_ENTRIES)}
    yield case([op(OP_INIT, 1, 50, text="a")], on=0, queues={"Create": [-1]}, settings=junk, allocs=9,
               banks={4: 5, 5: 6, 6: 7, 7: 8, 8: 9, 9: 10, 10: 11, 11: 12, 12: 13, 13: 14, 14: 15}, island=1, count=3, audio=0)
    yield case([op(OP_INIT, 1, 50, text="a")], on=0, queues={"Create": [0]}, settings=junk, allocs=-9,
               banks={4: 5, 5: 6, 6: 7, 7: 8, 8: 9, 9: 10, 10: 11, 11: 12, 12: 13, 13: 14, 14: 15}, island=1, count=3, audio=0)
    # the keyboard: absent or there, the answers of its two virtuals
    answers = [0, 1, 2, 0xFF, 0x100, 0x101, MASK, 0x80000000, 0x1FF00]
    for keyboard in (0, 1):
        for option in answers:
            for command in (0, 1, 0x100, 0xFF, MASK):
                yield case([op(OP_INIT, 1, 50, text="kb")], on=0, keyboard=keyboard,
                           queues={"Create": [0], "Option": [option], "Command": [command]})
    yield case([op(OP_INIT, 1, 50, text="kb"), op(OP_CLOSE), op(OP_INIT, 1, 50, text="kb2")], on=0, keyboard=0,
               queues={"Create": [0, 0], "Option": [1, 1], "Command": [1, 1]})
    yield case([op(OP_INIT, 1, 50, text="kb")], on=0, keyboard=0, queues={"Create": [-1], "Option": [1], "Command": [1]})
    yield case([op(OP_INIT, 1, 50, text="x" * 70)], on=0, queues={"Create": [0]})
    yield case([op(OP_INIT, 1, 50, text="")], on=0, queues={"Create": [0]})


def random_init_case(r):
    while True:
        c = random_init_case_once(r)
        if not bad_init(c):
            return c


def random_init_case_once(r):
    quality = r.choice([r.choice(QUALITIES), r.randrange(-10, 120)])
    balance = r.choice(list(BALANCES.values())) if r.random() < 0.5 else random_balance(r)
    return case([op(OP_INIT, r.choice([1, 1, 1, 0, 0x400, MASK]), quality, text=r.choice(["", "snd", "a" * r.randrange(1, 40)]))],
                on=r.choice([0, 0, 0, 1]), audio=r.choice([0, 1]), balance=balance, keyboard=r.choice([0, 1]),
                allocs=r.choice([0, 0, 3, -2]),
                settings={i: random_word(r) for i in r.sample(range(SETTINGS_ENTRIES), r.randrange(0, 8))},
                banks={i: random_word(r) for i in r.sample(range(4, 15), r.randrange(0, 6))},
                island=r.choice([0, 1]), count=r.choice([0, 1, 5]),
                queues={"Create": [r.choice([0, 0, -1, 1, MASK])], "Option": [r.choice([0, 1, 0x100, 0xFF])],
                        "Command": [r.choice([0, 1, 0x100, 0xFF])]})


# ---- SOUND_Close

def close_edges():
    for wanted, on in FLAGS:
        for allocs in (0, 1, -1, 5, 0x7FFFFFFF):
            yield case([op(OP_CLOSE)], wanted=wanted, on=on, allocs=allocs)
    yield case([op(OP_CLOSE), op(OP_CLOSE)], allocs=3)
    yield case([op(OP_CLOSE)], wanted=0, on=0, audio=0, allocs=2)
    yield case([op(OP_CLOSE)], wanted=1, on=0, audio=0)


def close_random(r):
    wanted, on = r.choice(FLAGS)
    return case([op(OP_CLOSE) for _ in range(r.randrange(1, 3))], wanted=wanted, on=on, allocs=r.choice([0, 0, 1, -1, 99]))


# ---- SOUND_SetChannels and SOUND_SetSoundQuality

def channels_edges():
    for name, balance in BALANCES.items():
        for quality in QUALITIES:
            for maximum in (0x20, 0, 5, 17, MASK, 0x7FFFFFFF, 0x80000000):
                if quality in (-1, 0x80000000, MASK, 1000) and maximum not in (0x20, MASK):
                    continue
                yield case([op(OP_SETCHANNELS, quality)], balance=balance, queues={"Max": [maximum]})
    for wanted, on in FLAGS:
        yield case([op(OP_SETCHANNELS, 90)], wanted=wanted, on=on, queues={"Max": [24]})


def channels_random(r):
    balance = r.choice(list(BALANCES.values())) if r.random() < 0.4 else random_balance(r)
    return case([op(OP_SETCHANNELS, r.choice([r.choice(QUALITIES), r.randrange(-10, 120)])) for _ in range(r.randrange(1, 3))],
                balance=balance, wanted=r.choice(FLAGS)[0], on=r.choice(FLAGS)[1],
                queues={"Max": [r.choice([0x20, 0, 5, MASK, r.randrange(0, 64), r.getrandbits(32)])]})


def quality_edges():
    for wanted, on in FLAGS:
        for quality in (0, 25, 50, 75, 100):
            yield case([op(OP_SETQUALITY, quality)], wanted=wanted, on=on, queues={"Max": [24]})
    for name, balance in BALANCES.items():
        for quality in QUALITIES:
            yield case([op(OP_SETQUALITY, quality)], balance=balance, queues={"Max": [24]})
    # each threshold on its own: quality at, one below and one above the value
    for slot in (S_QSOUND, S_REAR, S_CROSSOVER, S_EAX, S_MPEG, S_DROPPABLE, S_FADE):
        for quality in (49, 50, 51):
            balance = bal({S_QSOUND: 5, S_REAR: 5, S_CROSSOVER: 5, S_EAX: 5, S_MPEG: 5, S_DROPPABLE: 5,
                                               S_FADE: 5, slot: 50})
            yield case([op(OP_SETQUALITY, quality)], balance=balance, queues={"Max": [24]})
            balance = bal({S_QSOUND: 95, S_REAR: 95, S_CROSSOVER: 95, S_EAX: 95, S_MPEG: 95, S_DROPPABLE: 95,
                                               S_FADE: 95, slot: 50})
            yield case([op(OP_SETQUALITY, quality)], balance=balance, queues={"Max": [24]})


def quality_random(r):
    balance = r.choice(list(BALANCES.values())) if r.random() < 0.4 else random_balance(r)
    wanted, on = r.choice(FLAGS)
    return case([op(OP_SETQUALITY, r.choice([r.choice(QUALITIES), r.randrange(-10, 120)])) for _ in range(r.randrange(1, 3))],
                balance=balance, wanted=wanted, on=on,
                queues={"Max": [r.choice([0x20, 0, 5, MASK, r.randrange(0, 64)])]})


# ---- the volume setters

VOLUMES = [0, 1, 99, 100, 101, -1, 0x7FFFFFFF, 0x80000000, MASK, 12345678]


def volume_edges(kind):
    for value in VOLUMES:
        for changed in (0, 1, 5, -1):
            yield case([op(kind, value)], settings={E_CHANGED: changed})
    yield case([op(kind, 3), op(kind, 4)], settings={E_CHANGED: 0})
    yield case([op(kind, 3)], wanted=0, on=0, settings={i: i + 1 for i in range(SETTINGS_ENTRIES)})


def volume_random(kind):
    def maker(r):
        return case([op(kind, r.choice(VOLUMES + [r.getrandbits(32)])) for _ in range(r.randrange(1, 4))],
                    settings={i: random_word(r) for i in r.sample(range(SETTINGS_ENTRIES), r.randrange(0, 6))},
                    wanted=r.choice(FLAGS)[0], on=r.choice(FLAGS)[1])
    return maker


# ---- SOUND_SetParamValue

def param_edges():
    for wanted, on in FLAGS:
        for word in ([0], [5], [MASK], [0x80000000]):
            yield case([op(OP_PARAM, 7, 3, 99)], wanted=wanted, on=on, queues={"Word": word})
    for handle in HANDLES:
        for param in (0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x1FF, 0x12345678, MASK):
            yield case([op(OP_PARAM, handle, param, 0xAB)], queues={"Word": [77]})
    for value in (0, 1, MASK, 0x80000000, 0x7FFFFFFF, 0x12345678):
        yield case([op(OP_PARAM, 3, 4, value)], queues={"Word": [value]})
    yield case([op(OP_PARAM, 1, 2, 3), op(OP_PARAM, 4, 5, 6)], queues={"Word": [10, 11]})


def param_random(r):
    wanted, on = r.choice(FLAGS)
    return case([op(OP_PARAM, r.choice(HANDLES), r.choice([0, 1, 0xFF, 0x100, r.getrandbits(32)]),
                    r.choice([0, 1, MASK, r.getrandbits(32)])) for _ in range(r.randrange(1, 4))],
                wanted=wanted, on=on, queues={"Word": [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)]})


# ---- the no-payload commands

def simple_edges(kind):
    def edges():
        for wanted, on in FLAGS:
            yield case([op(kind)], wanted=wanted, on=on)
        yield case([op(kind), op(kind)])
    return edges


def simple_random(kind):
    def maker(r):
        wanted, on = r.choice(FLAGS)
        return case([op(kind) for _ in range(r.randrange(1, 3))], wanted=wanted, on=on)
    return maker


# ---- SOUND_Tick

def tick_edges():
    for wanted, on in FLAGS:
        yield case([op(OP_TICK)], wanted=wanted, on=on, settings={E_CHANGED: 1, E_MUSIC: 11, E_SFX: 22, E_GLOBAL: 33, E_SPEECH: 44})
    for changed in (0, 1, -1, 0x100):
        for duck in (0, 1, -5):
            for speech_on in (0, 1, 2):
                for speech_volume in (-1, 0, 1, 0x7FFFFFFF, 0x80000000):
                    for mute in (0, 1):
                        for music_on in (0, 1):
                            yield case([op(OP_TICK)], settings={E_CHANGED: changed, E_DUCK: duck, E_SPEECHON: speech_on,
                                                                E_SPEECH: speech_volume, E_MUTE: mute, E_MUSICON: music_on,
                                                                E_SFXON: 1, E_MUSIC: 1000, E_SFX: 2000, E_GLOBAL: 3000})
    for mute in (0, 1, -1, 0x100, -32768, 0x7FFF, 0x4000):
        for duck in (0, 1):
            yield case([op(OP_TICK)], settings={E_CHANGED: 1, E_DUCK: duck, E_SPEECHON: 1, E_SPEECH: 9, E_MUTE: mute, E_MUSIC: 500,
                                                E_SFX: 600})
    # the ducked volumes: the product wraps, the division by 100 truncates toward zero
    for music in VOLUMES + [-1000, 150, 199, -199, 1 << 24, -(1 << 24), 0x01000000 + 99]:
        for level in (0, 1, 50, 99, 100, 101, -50, -1, 0x7FFFFFFF, 0x80000000, 0x01000000, 255, 1000):
            yield case([op(OP_TICK)], balance=bal({S_DUCK: level}),
                       settings={E_CHANGED: 1, E_DUCK: 1, E_SPEECHON: 1, E_SPEECH: 5, E_MUSIC: music, E_SFX: s32(music * 3 + 1),
                                 E_GLOBAL: 7})
    # a volume that does not duck is sent as it is
    for volume in VOLUMES:
        yield case([op(OP_TICK)], settings={E_CHANGED: 1, E_DUCK: 0, E_MUSIC: volume, E_SFX: volume, E_GLOBAL: volume,
                                            E_SPEECH: volume, E_MUSICON: 1, E_SFXON: 0, E_SPEECHON: 1})
    yield case([op(OP_TICK), op(OP_TICK)], settings={E_CHANGED: 1, E_MUSIC: 1})
    yield case([op(OP_TICK), op(OP_MUSICVOL, 77), op(OP_TICK), op(OP_TICK)], settings={E_CHANGED: 0})
    yield case([op(OP_TICK)], settings={E_CHANGED: 1}, wanted=0, on=0, audio=0)
    yield case([op(OP_TICK)], settings={E_CHANGED: 1}, wanted=1, on=0, audio=0)
    yield case([op(OP_TICK)], settings={E_CHANGED: 1}, wanted=0, on=1, audio=0)


def tick_random(r):
    wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 3 + FLAGS[2:4])
    settings = {i: random_word(r) for i in range(SETTINGS_ENTRIES)}
    settings[E_CHANGED] = r.choice([0, 1, 1, 1, random_word(r)])
    settings[E_DUCK] = r.choice([0, 1, 1, random_word(r)])
    settings[E_SPEECHON] = r.choice([0, 1, 1, random_word(r)])
    settings[E_SPEECH] = r.choice([0, 1, -1, 50, random_word(r)])
    settings[E_MUTE] = r.choice([0, 0, 1, random_word(r) & 0x7FFF, -1])
    balance = bal({S_DUCK: r.choice([50, 100, 0, -10, random_word(r)])})
    return case([op(OP_TICK) for _ in range(r.randrange(1, 4))], wanted=wanted, on=on, settings=settings, balance=balance)


# ---- SOUND_SendEvent and SOUND_SendEventExt

def event_edges(kind):
    def edges():
        count = 7 if kind == OP_SENDEVENTEXT else 6
        for wanted, on in FLAGS:
            for event in (0, 1, -1, 0x7FFFFFFF, 0x80000000):
                yield case([op(kind, 5, 6, event, 10, 20, 30, 40)], wanted=wanted, on=on, queues={"Handle": [0x77]})
        for coordinate in COORDS:
            yield case([op(kind, 0, 4, 5, coordinate, coordinate, coordinate, coordinate)], queues={"Handle": [1]})
            yield case([op(kind, 0, 4, 5, coordinate, 0, 0, 0)], queues={"Handle": [1]})
            yield case([op(kind, 0, 4, 5, 0, coordinate, 0, coordinate)], queues={"Handle": [1]})
            yield case([op(kind, 0, 4, 5, 0, 0, coordinate, 0)], queues={"Handle": [1]})
            yield case([op(kind, 0, 4, 5, 3, coordinate, 7, 11)], queues={"Handle": [1]})
        for entity in (0, 1, -1, 0x7FFFFFFF, 0x80000000, MASK):
            for bank in (0, 1, -1, 0x7FFFFFFF, 0x80000000, 0x12345678):
                yield case([op(kind, entity, bank, 9, 1, 2, 3, 4)], queues={"Handle": [entity & MASK]})
        for handle in (0, 1, MASK, 0x80000000, 0x7FFFFFFF):
            yield case([op(kind, 1, 2, 3, 4, 5, 6, 7)], queues={"Handle": [handle]})
        yield case([op(kind, 1, 2, 3, 4, 5, 6, 7), op(kind, 7, 8, 9, 10, 11, 12, 13)], queues={"Handle": [5, 6]})
        yield case([op(kind, 1, 2, 3, 4, 5, 6, 7)], wanted=0, on=0, audio=0)
    return edges


def event_random(kind):
    def maker(r):
        wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 4 + FLAGS[2:4])
        ops = []
        for _ in range(r.randrange(1, 4)):
            c = [r.choice(COORDS + [r.getrandbits(32)]) for _ in range(4)]
            ops.append(op(kind, r.choice([0, 1, -1, r.getrandbits(32)]), r.choice([0, 1, -1, r.getrandbits(32)]),
                          r.choice([0, 1, -1, 5, r.getrandbits(32)]), *c))
        return case(ops, wanted=wanted, on=on, queues={"Handle": [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)]})
    return maker


# ---- the commands that take a handle

def handle_edges(kind, count):
    def edges():
        for wanted, on in FLAGS:
            yield case([op(kind, 5, 6)], wanted=wanted, on=on)
        for handle in HANDLES:
            for second in ((0,) if count == 1 else (0, 1, -1, 0x80000000, 0x7FFFFFFF, 100)):
                yield case([op(kind, handle, second)])
        yield case([op(kind, 1, 2), op(kind, 3, 4)])
        yield case([op(kind, 1, 2)], wanted=0, on=0, audio=0)
    return edges


def handle_random(kind, count):
    def maker(r):
        wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 4 + FLAGS[2:4])
        return case([op(kind, r.choice(HANDLES + [r.getrandbits(32)]), r.choice(HANDLES + [r.getrandbits(32)]))
                     for _ in range(r.randrange(1, 4))], wanted=wanted, on=on)
    return maker


# ---- SOUND_MoveSoundEffect

def move_edges():
    for wanted, on in FLAGS:
        yield case([op(OP_MOVE, 5, 1, 2, 3)], wanted=wanted, on=on, queues={"Word": [0x66]})
    for handle in HANDLES:
        yield case([op(OP_MOVE, handle, 0, 0, 0)], queues={"Word": [handle & MASK]})
    for coordinate in COORDS:
        yield case([op(OP_MOVE, 3, coordinate, 0, 0)], queues={"Word": [1]})
        yield case([op(OP_MOVE, 3, 0, coordinate, 0)], queues={"Word": [1]})
        yield case([op(OP_MOVE, 3, 0, 0, coordinate)], queues={"Word": [1]})
        yield case([op(OP_MOVE, 3, coordinate, coordinate + 1, coordinate - 1)], queues={"Word": [1]})
    for word in (0, 1, MASK, 0x80000000, 0x7FFFFFFF):
        yield case([op(OP_MOVE, 3, 1, 2, 3)], queues={"Word": [word]})
    yield case([op(OP_MOVE, 3, 1, 2, 3)], wanted=0, on=0, audio=0)


def move_random(r):
    wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 4 + FLAGS[2:4])
    return case([op(OP_MOVE, r.choice(HANDLES + [r.getrandbits(32)]), *[r.choice(COORDS + [r.getrandbits(32)]) for _ in range(3)])
                 for _ in range(r.randrange(1, 4))], wanted=wanted, on=on,
                queues={"Word": [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)]})


# ---- SOUND_GetCurrentSampleLengthInMilliseconds

def length_edges():
    for wanted, on in FLAGS:
        yield case([op(OP_SAMPLELEN, 5)], wanted=wanted, on=on, queues={"Length": [999]})
        yield case([op(OP_SAMPLELEN, 5)], wanted=wanted, on=on, queues={"Skip": [1]})
    for handle in HANDLES:
        yield case([op(OP_SAMPLELEN, handle)], queues={"Length": [321]})
    for length in (0, 1, -1, 5000, 0x7FFFFFFF, 0x80000000, MASK):
        yield case([op(OP_SAMPLELEN, 2)], queues={"Length": [length]})
    yield case([op(OP_SAMPLELEN, 2), op(OP_SAMPLELEN, 3)], queues={"Length": [10, 20]})
    yield case([op(OP_SAMPLELEN, 2), op(OP_SAMPLELEN, 3)], queues={"Skip": [0, 1], "Length": [10, 20]})
    yield case([op(OP_SAMPLELEN, 2)], wanted=0, on=0, audio=0)
    yield case([op(OP_SAMPLELEN, 2)], wanted=1, on=0, audio=0)


def length_random(r):
    wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 4 + FLAGS[2:4])
    return case([op(OP_SAMPLELEN, r.choice(HANDLES)) for _ in range(r.randrange(1, 4))], wanted=wanted, on=on,
                queues={"Length": [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)], "Skip": [r.choice([0, 0, 1])]})


# ---- SOUND_SetPauseOn3DSoundsOnly and SOUND_SetEarPosition

def pause3d_edges():
    for value in (0, 1, -1, 0x10000, 0x10001, 0x7FFFFFFF, 0x80000000, 0xFFFF, 0x8000, 0x7FFF, 0xFFFF0000, 0x12345678):
        yield case([op(OP_PAUSE3D, value)])
    for wanted, on in FLAGS:
        yield case([op(OP_PAUSE3D, 1)], wanted=wanted, on=on, settings={E_PAUSE3D: 7, E_MUTE: 5})
    yield case([op(OP_PAUSE3D, 1), op(OP_PAUSE3D, 0)], settings={E_PAUSE3D: -7})


def pause3d_random(r):
    return case([op(OP_PAUSE3D, r.choice([0, 1, -1, 0x10000, 0xFFFF, r.getrandbits(32)])) for _ in range(r.randrange(1, 4))],
                settings={i: random_word(r) for i in r.sample(range(SETTINGS_ENTRIES), r.randrange(0, 6))},
                wanted=r.choice(FLAGS)[0], on=r.choice(FLAGS)[1])


def ear_ops(values):
    return op(OP_EAR, *[fb(v) for v in values])


def ear_edges():
    base = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]
    for wanted, on in FLAGS:
        yield case([ear_ops(base)], wanted=wanted, on=on)
    for value in (0, 1, -1, 0x100, 0x7FFF, -32768):
        yield case([ear_ops(base)], settings={E_PAUSE3D: value})
        yield case([ear_ops([0.0] * 9)], settings={E_PAUSE3D: value})
    for index in range(9):
        for value in FLOATS:
            floats = list(base)
            floats[index] = value
            yield case([ear_ops(floats)])
            yield case([ear_ops(floats)], settings={E_PAUSE3D: 1})
    # Triage w66 (doubtful 6): a NaN in each place, with the y replacement on and off (the short at +0x30 decides it).
    for index in range(9):
        floats = list(base)
        floats[index] = float("nan")
        for value in (0, 1):
            yield case([ear_ops(floats)], settings={E_PAUSE3D: value})
    yield case([ear_ops(base), ear_ops(list(reversed(base)))])
    yield case([ear_ops(base)], wanted=0, on=0, audio=0)
    yield case([ear_ops(base), op(OP_PAUSE3D, 1), ear_ops(base), op(OP_PAUSE3D, 0), ear_ops(base)])


def ear_random(r):
    wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 4 + FLAGS[2:4])
    pool = FLOATS + [r.uniform(-1000, 1000)]
    return case([ear_ops([r.choice(pool) for _ in range(9)]) for _ in range(r.randrange(1, 3))], wanted=wanted, on=on,
                settings={E_PAUSE3D: r.choice([0, 1, 0, -1])})


# ---- the empty wrappers

def empty_edges(kind, count):
    def edges():
        for value in (0, 1, -1, 0x7FFFFFFF, 0x80000000):
            for wanted, on in FLAGS[:4]:
                if kind == OP_ADDOBJECT:
                    yield case([op(kind, *[fb(float(value) + i) for i in range(5)])], wanted=wanted, on=on,
                               settings={i: i + 3 for i in range(SETTINGS_ENTRIES)})
                else:
                    yield case([op(kind, value)], wanted=wanted, on=on, settings={i: i + 3 for i in range(SETTINGS_ENTRIES)})
        yield case([op(kind, 1), op(kind, 2)])
    return edges


def empty_random(kind, count):
    def maker(r):
        if kind == OP_ADDOBJECT:
            ops = [op(kind, *[fb(r.choice(FLOATS)) for _ in range(5)]) for _ in range(r.randrange(1, 3))]
        else:
            ops = [op(kind, r.choice([0, 1, -1, r.getrandbits(32)])) for _ in range(r.randrange(1, 3))]
        return case(ops, wanted=r.choice(FLAGS)[0], on=r.choice(FLAGS)[1],
                    settings={i: random_word(r) for i in r.sample(range(SETTINGS_ENTRIES), r.randrange(0, 6))})
    return maker


# ---- the TbAudioEvent getters

def getter_edges(kind):
    def edges():
        for category in (0, 1, -1, 0x7FFFFFFF, 0x80000000, 0x12345678):
            for event in (0, 1, -1, 0x7FFFFFFF, 0x80000000):
                yield case([op(kind, category, event, fb(1.5), fb(-2.5), fb(3.5), fb(0.0))])
        for value in FLOATS:
            yield case([op(kind, 7, 8, fb(value), fb(value), fb(value), fb(value))])
        yield case([op(kind, 1, 2, fb(1.0), fb(2.0), fb(3.0), fb(4.0)), op(kind, 3, 4, fb(5.0), fb(6.0), fb(7.0), fb(8.0))])
    return edges


def getter_random(kind):
    def maker(r):
        return case([op(kind, r.choice([0, 1, -1, r.getrandbits(32)]), r.choice([0, 1, -1, r.getrandbits(32)]),
                        *[fb(r.choice(FLOATS + [r.uniform(-1000, 1000)])) for _ in range(4)]) for _ in range(r.randrange(1, 3))])
    return maker


# ---- every function in one script

MIXED_OPS = [OP_INIT, OP_CLOSE, OP_SETCHANNELS, OP_SETQUALITY, OP_GLOBALVOL, OP_MUSICVOL, OP_SFXVOL, OP_SPEECHVOL, OP_PARAM,
             OP_STOPALL, OP_FADEALL, OP_PAUSE, OP_UNPAUSE, OP_TICK, OP_TICK, OP_TICK, OP_SENDEVENT, OP_PAUSESOUND, OP_UNPAUSESOUND,
             OP_VOLONSOUND, OP_SENDEVENTEXT, OP_PAUSE3D, OP_EAR, OP_MOVE, OP_STOPSOUND, OP_FADESOUND, OP_SAMPLELEN,
             OP_OCCLUSION, OP_REVERB, OP_DROPPABLE, OP_ADDOBJECT, OP_DELOBJECT, OP_MUSICSTATUS, OP_SFXSTATUS, OP_SPEECHSTATUS,
             OP_DUCK]


def random_op(r, kind):
    word = lambda: r.choice([0, 1, -1, 5, 100, r.getrandbits(32)])
    if kind == OP_INIT:
        return op(kind, r.choice([1, 1, 0, 0x400]), r.choice(QUALITIES + [r.randrange(0, 101)]), text=r.choice(["", "snd", "x" * 20]))
    if kind in FLOAT_OPS:
        return op(kind, *[fb(r.choice(FLOATS)) for _ in range(FLOAT_OPS[kind])])
    if kind in (OP_SETCHANNELS, OP_SETQUALITY):
        return op(kind, r.choice(QUALITIES + [r.randrange(0, 101)]))
    return op(kind, *[word() for _ in range(7)])


def mixed_random(r):
    while True:
        c = mixed_random_once(r)
        if not bad_init(c):
            return c


def mixed_random_once(r):
    wanted, on = r.choice(FLAGS[:2] + [(1, 1)] * 3 + FLAGS[2:4])
    settings = {i: random_word(r) for i in r.sample(range(SETTINGS_ENTRIES), r.randrange(0, 8))}
    settings[E_CHANGED] = r.choice([0, 1, 1])
    settings[E_MUTE] = r.choice([0, 0, 1, 0x100])
    balance = r.choice(list(BALANCES.values())) if r.random() < 0.5 else random_balance(r)
    ops = [random_op(r, r.choice(MIXED_OPS)) for _ in range(r.randrange(2, 12))]
    return case(ops, wanted=wanted, on=on, settings=settings, balance=balance, keyboard=r.choice([0, 1]), allocs=r.choice([0, 0, 2]),
                queues={"Create": [r.choice([0, 0, -1])], "Handle": [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)],
                        "Word": [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)],
                        "Max": [r.choice([0x20, 0, 5, r.randrange(0, 64)])],
                        "Length": [r.choice([0, 1, 5000, MASK])],
                        "Option": [r.choice([0, 1, 0x100])], "Command": [r.choice([0, 1, 0x100])]})


def mixed_edges():
    script = [op(OP_INIT, 1, 80, text="game"), op(OP_GLOBALVOL, 90), op(OP_MUSICVOL, 70),
              op(OP_SFXVOL, 60), op(OP_SPEECHVOL, 80), op(OP_TICK), op(OP_SENDEVENT, 0, 5, 7, 1, 2, 3), op(OP_DUCK, 1), op(OP_TICK),
              op(OP_PAUSE), op(OP_UNPAUSE), op(OP_SETQUALITY, 40), op(OP_EAR, *[fb(float(i)) for i in range(9)]),
              op(OP_STOPALL), op(OP_CLOSE)]
    yield case(script, on=0, queues={"Create": [0], "Handle": [42], "Max": [24], "Option": [1], "Command": [1]})
    yield case(script, on=0, queues={"Create": [-1], "Handle": [42], "Max": [24]})


TESTS = [
    FnTest("SOUND_Init 0x100dc940", run_ppc, host_case, edge=init_edges, rnd=random_init_case),
    FnTest("SOUND_Close 0x100dccd4", run_ppc, host_case, edge=close_edges, rnd=close_random),
    FnTest("SOUND_SetChannels 0x100dcd80", run_ppc, host_case, edge=channels_edges, rnd=channels_random),
    FnTest("SOUND_SetSoundQuality 0x100dcf0c", run_ppc, host_case, edge=quality_edges, rnd=quality_random),
    FnTest("SOUND_SetGlobalVolume 0x100dd1b4", run_ppc, host_case, edge=lambda: volume_edges(OP_GLOBALVOL),
           rnd=volume_random(OP_GLOBALVOL)),
    FnTest("SOUND_SetMusicVolume 0x100dd1f8", run_ppc, host_case, edge=lambda: volume_edges(OP_MUSICVOL),
           rnd=volume_random(OP_MUSICVOL)),
    FnTest("SOUND_SetSfxVolume 0x100dd238", run_ppc, host_case, edge=lambda: volume_edges(OP_SFXVOL),
           rnd=volume_random(OP_SFXVOL)),
    FnTest("SOUND_SetSpeechVolume 0x100dd278", run_ppc, host_case, edge=lambda: volume_edges(OP_SPEECHVOL),
           rnd=volume_random(OP_SPEECHVOL)),
    FnTest("SOUND_SetParamValue 0x100dd380", run_ppc, host_case, edge=param_edges, rnd=param_random),
    FnTest("SOUND_StopAllSfx 0x100dd41c", run_ppc, host_case, edge=simple_edges(OP_STOPALL), rnd=simple_random(OP_STOPALL)),
    FnTest("SOUND_FadeOutAllSfx 0x100dd4b0", run_ppc, host_case, edge=simple_edges(OP_FADEALL), rnd=simple_random(OP_FADEALL)),
    FnTest("SOUND_Pause 0x100dd548", run_ppc, host_case, edge=simple_edges(OP_PAUSE), rnd=simple_random(OP_PAUSE)),
    FnTest("SOUND_UnPause 0x100dd5d8", run_ppc, host_case, edge=simple_edges(OP_UNPAUSE), rnd=simple_random(OP_UNPAUSE)),
    FnTest("SOUND_Tick 0x100dd668", run_ppc, host_case, edge=tick_edges, rnd=tick_random),
    FnTest("SOUND_SendEvent 0x100dd9fc", run_ppc, host_case, edge=event_edges(OP_SENDEVENT), rnd=event_random(OP_SENDEVENT)),
    FnTest("SOUND_PauseSound 0x100ddb18", run_ppc, host_case, edge=handle_edges(OP_PAUSESOUND, 1),
           rnd=handle_random(OP_PAUSESOUND, 1)),
    FnTest("SOUND_UnpauseSound 0x100ddbb8", run_ppc, host_case, edge=handle_edges(OP_UNPAUSESOUND, 1),
           rnd=handle_random(OP_UNPAUSESOUND, 1)),
    FnTest("SOUND_SetVolumeOnSound 0x100ddc5c", run_ppc, host_case, edge=handle_edges(OP_VOLONSOUND, 2),
           rnd=handle_random(OP_VOLONSOUND, 2)),
    FnTest("SOUND_SendEventExt 0x100ddd08", run_ppc, host_case, edge=event_edges(OP_SENDEVENTEXT),
           rnd=event_random(OP_SENDEVENTEXT)),
    FnTest("SOUND_SetPauseOn3DSoundsOnly 0x100dde38", run_ppc, host_case, edge=pause3d_edges, rnd=pause3d_random),
    FnTest("SOUND_SetEarPosition 0x100dde7c", run_ppc, host_case, edge=ear_edges, rnd=ear_random),
    FnTest("SOUND_MoveSoundEffect 0x100ddf60", run_ppc, host_case, edge=move_edges, rnd=move_random),
    FnTest("SOUND_StopSoundEffect 0x100de058", run_ppc, host_case, edge=handle_edges(OP_STOPSOUND, 1),
           rnd=handle_random(OP_STOPSOUND, 1)),
    FnTest("SOUND_FadeSoundEffect 0x100de0fc", run_ppc, host_case, edge=handle_edges(OP_FADESOUND, 1),
           rnd=handle_random(OP_FADESOUND, 1)),
    FnTest("SOUND_GetCurrentSampleLengthInMilliseconds 0x100de758", run_ppc, host_case, edge=length_edges, rnd=length_random),
    FnTest("SOUND_AddObject 0x100de8e4", run_ppc, host_case, edge=empty_edges(OP_ADDOBJECT, 5), rnd=empty_random(OP_ADDOBJECT, 5)),
    FnTest("SOUND_DelObject 0x100de918", run_ppc, host_case, edge=empty_edges(OP_DELOBJECT, 1), rnd=empty_random(OP_DELOBJECT, 1)),
    FnTest("SOUND_SetOcclusionProcessing 0x100de944", run_ppc, host_case, edge=empty_edges(OP_OCCLUSION, 1),
           rnd=empty_random(OP_OCCLUSION, 1)),
    FnTest("SOUND_SetReverbLevel 0x100de97c", run_ppc, host_case, edge=empty_edges(OP_REVERB, 1), rnd=empty_random(OP_REVERB, 1)),
    FnTest("SOUND_SetDroppableLevel 0x100de9ac", run_ppc, host_case, edge=empty_edges(OP_DROPPABLE, 1),
           rnd=empty_random(OP_DROPPABLE, 1)),
    FnTest("TbAudioEvent::GetType 0x100de9e0", run_ppc, host_case, edge=getter_edges(OP_GETTYPE), rnd=getter_random(OP_GETTYPE)),
    FnTest("TbAudioEvent::GetCategory 0x100dea18", run_ppc, host_case, edge=getter_edges(OP_GETCATEGORY),
           rnd=getter_random(OP_GETCATEGORY)),
    FnTest("TbAudioEvent::GetEvent 0x100dea54", run_ppc, host_case, edge=getter_edges(OP_GETEVENT), rnd=getter_random(OP_GETEVENT)),
    FnTest("TbAudioEvent::GetPosition 0x100dea8c", run_ppc, host_case, edge=getter_edges(OP_GETPOSITION),
           rnd=getter_random(OP_GETPOSITION)),
    FnTest("every function in mixed scripts", run_ppc, host_case, edge=mixed_edges, rnd=mixed_random),
]
