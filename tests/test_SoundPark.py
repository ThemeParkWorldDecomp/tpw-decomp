"""Unit w74, the park sound wrappers, against libtpw_unit_SoundPark.dylib (tests/unit_SoundPark.cpp), in the main app
("SimTheme Park"):
  src/game/Sound.cpp, src/game/park/ParkSound.cpp  SND_DipMusic 0x100deb04, SND_StartParkMusic 0x100deb40, SND_StopParkMusic 0x100debd4, SND_SetParkMusicLevel
                     0x100dec30, SND_SetChatterLevel 0x100dec9c, SND_SetChatterOnOff 0x100deda4, SND_SetRainLevel 0x100dee84,
                     SND_SendIndirectedEvent 0x100df858, SOUND_SaveStatus 0x100de1a0, SOUND_LoadStatus 0x100de48c
  src/game/Sound.cpp, src/game/park/ParkSound.cpp    the state they work on

The functions run as the original, one script of operations per case, with the state (the two flags, the audio object, the
settings block and the file position) read after every operation. Every callee outside the unit is a fake that records the same event
on both sides: the runtime wrappers SOUND_SendEvent 0x100dd9fc, SOUND_SetParamValue 0x100dd380 and SOUND_FadeSoundEffect 0x100de0fc
(the queues "Event" and "Param" are the answers of the first two), RSSE_GetSoundEventCode 0x100d6d04 (the queue "Code"),
LbFile_Read/Write/Seek (one file the case sets up: bytes and a position; the writes are logged with their bytes and kept for
a read-back by the Rewind operation), the two SamsUtilities swap instances 0x100de404 and 0x100de448 (they swap and are
recorded with the offset of their word, so that the set of swapped fields is compared too) and two audio objects (fake objects in
persistent PowerPC memory whose vtable has the one slot the game calls, +0x20 SystemCommand; the host's are TbAudioSystems). A command
object SystemCommand receives is compared by its kind, which the PowerPC side finds from the vtable word the original stores (the sound
library's import vtable of that command class), and by its payload words. The queues "PokeRead", "PokeWrite" and "PokeCmd" change one
word of the state after a file read, write or command (the game reads some of its state again later: the condition of the
save, the audio object between the two commands of the load, the reverb preset).

The file's words: "w" is a little-endian word of the file format (the same bytes on both sides); "n" is a word the original reads
raw, in the machine's own byte order (the load's skip word, and the audio object's block): the PowerPC side stores it big-endian
and the host little-endian, so that both read the same number. The settings entries the game never swaps (18 to 21) are compared
as the bytes read in little-endian order; the rain handle (entry 3), which the save copies raw in the machine's own order, is
given equal bytes whenever a save or a load handles it.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_SoundPark"
MASK = 0xFFFFFFFF

# the functions
ADDR = {
    "DipMusic": 0x100DEB04, "StartParkMusic": 0x100DEB40, "StopParkMusic": 0x100DEBD4, "SetParkMusicLevel": 0x100DEC30,
    "SetChatterLevel": 0x100DEC9C, "SetChatterOnOff": 0x100DEDA4, "SetRainLevel": 0x100DEE84, "SendIndirectedEvent": 0x100DF858,
    "SaveStatus": 0x100DE1A0, "LoadStatus": 0x100DE48C,
}
# the operations of a script; the numbers are those of tests/unit_SoundPark.cpp
(OP_DIP, OP_START, OP_STOP, OP_MUSICLEVEL, OP_CHATTERLEVEL, OP_CHATTERONOFF, OP_RAIN, OP_INDIRECT, OP_SAVE, OP_LOAD,
 OP_REWIND, OP_RESETFILE) = range(12)
OP_ADDRESS = {OP_DIP: "DipMusic", OP_START: "StartParkMusic", OP_STOP: "StopParkMusic", OP_MUSICLEVEL: "SetParkMusicLevel",
              OP_CHATTERLEVEL: "SetChatterLevel", OP_CHATTERONOFF: "SetChatterOnOff", OP_RAIN: "SetRainLevel",
              OP_INDIRECT: "SendIndirectedEvent", OP_SAVE: "SaveStatus", OP_LOAD: "LoadStatus"}
ARG_COUNT = {OP_DIP: 1, OP_START: 0, OP_STOP: 0, OP_MUSICLEVEL: 1, OP_CHATTERLEVEL: 1, OP_CHATTERONOFF: 1, OP_RAIN: 1, OP_INDIRECT: 7}
RESULT_OPS = {OP_INDIRECT}

# the callees (in-binary functions, hooked by address)
SEND_EVENT, SET_PARAM, FADE_SOUND, GET_EVENT_CODE = 0x100DD9FC, 0x100DD380, 0x100DE0FC, 0x100D6D04
SWAP16, SWAP32 = 0x100DE404, 0x100DE448
# the imports
LB_READ = "LbFile_Read__FPvPvUlPUl"
LB_WRITE = "LbFile_Write__FPvPCvUlPUl"
LB_SEEK = "LbFile_Seek__FPvl14TbFileSeekMode"
# the data
WANTED, ON, AUDIO, SETTINGS, BLOCK = 0x102F7C38, 0x102F7C3C, 0x102F7C34, 0x102F7C40, 0x102F7C94
LABEL_AUDIO, LABEL_AUDIO2, LABEL_FILE = 2, 4, 5

# the command classes: kind number -> import vtable name
COMMANDS = {1: "Save", 2: "Load", 3: "SetPresetReverb"}
KIND_OF_WORD = {}      # filled by setup: the vtable word of each command class -> its kind

# the state: 0 wanted, 1 on, 2 the audio object's label, 3 to 24 the settings (entries 0 to 11: the words at 0x00 to 0x2c, 12 and
# 13: the shorts at 0x30 and 0x32, 14 to 21: the words at 0x34 to 0x50), 25 the file position
STATE_WORDS = 26
SETTINGS_ENTRIES = 22
RAW_ENTRIES = (18, 19, 20, 21)         # the bytes the game never writes or swaps (compared as bytes)
SYMMETRIC_ENTRIES = (3,)               # the rain handle: the game copies it raw but also writes it as a word, so its four bytes are equal
(E_PARK, E_CHATTER_HANDLE, E_ON, E_RAIN, E_LEVEL, E_F14, E_GLOBAL, E_SFX, E_MUSIC, E_SPEECH, E_CHANGED, E_DUCK, E_MUTE,
 E_PAUSE3D, E_F34, E_SFXON, E_MUSICON, E_SPEECHON) = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)
# the put indexes beyond the state
P_WANTED, P_ON, P_AUDIO, P_ENTRY0, P_BANK0, P_BANK1, P_MUSICBANK = 0, 1, 2, 3, 25, 26, 27
SAVED_ENTRIES = [e for e in range(SETTINGS_ENTRIES) if e not in RAW_ENTRIES]


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


STUBS = None
AUDIO_OBJECTS = None
FILE_OBJECT = None


def settings_address(entry):
    return SETTINGS + (4 * entry if entry < 12 else 0x30 + 2 * (entry - 12) if entry < 14 else 0x34 + 4 * (entry - 14))


def put_state_ppc(e, index, value):
    if index == P_WANTED:
        ws(e, WANTED, [value])
    elif index == P_ON:
        ws(e, ON, [value])
    elif index == P_AUDIO:
        ws(e, AUDIO, [0 if value == 0 else AUDIO_OBJECTS[0] if value == 1 else AUDIO_OBJECTS[1]])
    elif index == P_BANK0:
        ws(e, BLOCK, [value])
    elif index == P_BANK1:
        ws(e, BLOCK + 4, [value])
    elif index == P_MUSICBANK:
        ws(e, BLOCK + 0x24, [value])
    else:
        entry = index - P_ENTRY0
        a = settings_address(entry)
        if entry in (12, 13):
            e.write(a, struct.pack(">H", value & 0xFFFF))
        elif entry in RAW_ENTRIES:
            e.write(a, struct.pack("<I", value & MASK))
        else:
            ws(e, a, [value])


def hexs(data):
    return data.hex()


def poke(e, key):
    packed = e.cl.ret(key, 0)
    if packed:
        put_state_ppc(e, (packed >> 32) - 1, s32(packed & MASK))


def file_bytes(segments, big):
    """The bytes of a file given as segments: ("w", v) a little-endian word, ("n", v) a word in the machine's order (big on the
    PowerPC side, little on the host), ("b", bytes) raw bytes."""
    out = b""
    for kind, value in segments:
        if kind == "w":
            out += struct.pack("<I", value & MASK)
        elif kind == "n":
            out += struct.pack(">I" if big else "<I", value & MASK)
        else:
            out += value
    return out


def build_stubs():
    S = CL.Stubs()

    def send_event(e):
        L = e.cl
        L.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
        e.set_r(3, L.ret("Event", 0) & MASK)

    def set_param(e):
        L = e.cl
        L.rec("SOUND_SetParamValue", (s32(e.r(3)), e.r(4), e.r(5)))
        e.set_r(3, L.ret("Param", 0) & MASK)

    def fade(e):
        e.cl.rec("SOUND_FadeSoundEffect", (s32(e.r(3)),))

    def event_code(e):
        L = e.cl
        L.rec("RSSE_GetSoundEventCode", (s32(e.r(3)), s32(e.r(4))))
        e.set_r(3, L.ret("Code", 0) & MASK)

    # the swaps: they swap the word and are recorded with its offset (in the settings block, or from the first word outside it)
    def swap(size):
        def stub(e):
            L = e.cl
            a = e.r(3)
            if SETTINGS <= a < SETTINGS + 0x54:
                offset = a - SETTINGS
            else:
                if L.swap_base is None:
                    L.swap_base = a
                offset = a - L.swap_base
            L.rec("USwapInplace%d" % (8 * size), (offset,))
            e.write(a, e.read(a, size)[::-1])
        return stub

    # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done (NULL in the game)
    def lb_read(e):
        L, buffer, length = e.cl, e.r(5), e.r(6)
        L.swap_base = None
        in_settings, in_wanted = buffer == SETTINGS, buffer == WANTED
        count = min(length, len(L.file) - L.position)
        if in_settings:
            count = min(count, 0x54)
        if count:
            e.write(buffer, L.file[L.position:L.position + count])
        L.position += count
        if e.r(7):
            ws(e, e.r(7), [count])
        L.rec("LbFile_Read", (L.lab(e.r(4)), length, 1 if in_settings else 2 if in_wanted else 0, count, 1 if e.r(7) else 0))
        ws(e, e.r(3), [0])
        poke(e, "PokeRead")

    def lb_write(e):
        L, length = e.cl, e.r(6)
        L.swap_base = None
        data = e.read(e.r(5), length) if length else b""
        third = (length + 2) // 3
        cut1, cut2 = min(third, length), min(2 * third, length)
        L.rec("LbFile_Write", (L.lab(e.r(4)), length, 1 if e.r(7) else 0),
              (hexs(data[:cut1]), hexs(data[cut1:cut2]), hexs(data[cut2:])))
        L.written += data
        ws(e, e.r(3), [0])
        poke(e, "PokeWrite")

    def lb_seek(e):
        L, offset = e.cl, s32(e.r(4))
        L.rec("LbFile_Seek", (L.lab(e.r(3)), offset, e.r(5)))
        L.position = max(0, min(len(L.file), L.position + offset))
        e.set_r(3, 0)

    S.internal(SEND_EVENT, send_event)
    S.internal(SET_PARAM, set_param)
    S.internal(FADE_SOUND, fade)
    S.internal(GET_EVENT_CODE, event_code)
    S.internal(SWAP16, swap(2))
    S.internal(SWAP32, swap(4))
    S.imp(LB_READ, lb_read)
    S.imp(LB_WRITE, lb_write)
    S.imp(LB_SEEK, lb_seek)
    return S


def make_audio(e):
    """Two fake audio objects in persistent memory sharing a vtable with the one slot the game calls (+0x20 SystemCommand)."""
    def command(e):
        # r3 hidden, r4 this, r5 the command: its vtable word and its payload words
        L = e.cl
        cmd = e.r(5)
        kind = KIND_OF_WORD.get(e.u32(cmd), 99)
        p = [0, 0]
        if kind == 1:
            p = [L.lab(e.u32(cmd + 4)), len(L.objbytes)]
            L.written += L.objbytes
        elif kind == 2:
            p = [L.lab(e.u32(cmd + 4)), e.read(cmd + 8, 1)[0]]
        elif kind == 3:
            p = [s32(e.u32(cmd + 4)), 0]
        L.rec("Audio.SystemCommand", (L.lab(e.r(4)), kind, p[0], p[1]))
        poke(e, "PokeCmd")

    vt = T.persist_alloc(e, 0x40)
    ws(e, vt + 0x20, [T.fake_function(e, command)])
    return [T.persist_alloc(e, 16, struct.pack(">I", vt) + b"\0" * 12) for _ in range(2)]


def setup(runner):
    global STUBS, AUDIO_OBJECTS, FILE_OBJECT
    T.install_std_stubs(runner.emu)
    KIND_OF_WORD.clear()
    for kind, name in COMMANDS.items():
        KIND_OF_WORD[runner.emu.data_import("__vt__Q212TbSysCommand%d%s" % (len(name), name))] = kind
    STUBS = build_stubs()
    AUDIO_OBJECTS = make_audio(runner.emu)
    FILE_OBJECT = T.persist_alloc(runner.emu, 16, b"\0" * 16)


# ---------------------------------------------------------------- the two sides

def state_ppc(e, L):
    words = [s32(e.u32(WANTED)), s32(e.u32(ON)), L.lab(e.u32(AUDIO))]
    for entry in range(SETTINGS_ENTRIES):
        a = settings_address(entry)
        if entry in (12, 13):
            words.append(T.s16(struct.unpack(">H", e.read(a, 2))[0]))
        elif entry in RAW_ENTRIES:
            words.append(struct.unpack("<i", e.read(a, 4))[0])
        else:
            words.append(s32(e.u32(a)))
    words.append(L.position)
    return words


def write_state(e, init):
    put_state_ppc(e, P_WANTED, init["wanted"])
    put_state_ppc(e, P_ON, init["on"])
    put_state_ppc(e, P_AUDIO, init["audio"])
    e.write(SETTINGS, b"\0" * 0x54)
    for entry, value in init["settings"].items():
        put_state_ppc(e, P_ENTRY0 + entry, value)
    put_state_ppc(e, P_BANK0, init["bank0"])
    put_state_ppc(e, P_BANK1, init["bank1"])
    put_state_ppc(e, P_MUSICBANK, init["music"])


def ppc_case(e, case):
    e.heap_in_scratch = True
    fixed = {AUDIO_OBJECTS[0]: LABEL_AUDIO, AUDIO_OBJECTS[1]: LABEL_AUDIO2, FILE_OBJECT: LABEL_FILE}
    e.cl = L = CL.PpcLog(fixed, case.get("q"))
    init = case["init"]
    L.file = file_bytes(init["file"], True)
    L.position = init["position"]
    L.file0, L.position0 = L.file, L.position
    L.written = b""
    L.objbytes = file_bytes(init["obj"], True)
    L.swap_base = None
    saved = STUBS.activate(e)
    try:
        write_state(e, init)
        steps = []
        for op, args in case["ops"]:
            result = 0
            if op == OP_REWIND:
                L.file, L.written, L.position = L.written, b"", 0
            elif op == OP_RESETFILE:
                L.file, L.position = L.file0, L.position0
            elif op in (OP_SAVE, OP_LOAD):
                T.call_ppc(e, ADDR[OP_ADDRESS[op]], [FILE_OBJECT if args[0] else 0])
            else:
                r3 = T.call_ppc(e, ADDR[OP_ADDRESS[op]], [a & MASK for a in args[:ARG_COUNT[op]]])[0]
                result = s32(r3) if op in RESULT_OPS else 0
            steps.append(dict(result=result, state=state_ppc(e, L)))
        return dict(steps=steps, log=L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_case(lib, case):
    init = case["init"]
    lib.tpw_sx_begin()
    lib.tpw_sx_put.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sx_put(P_WANTED, s32(init["wanted"]))
    lib.tpw_sx_put(P_ON, s32(init["on"]))
    lib.tpw_sx_put(P_AUDIO, init["audio"])
    for entry, value in init["settings"].items():
        lib.tpw_sx_put(P_ENTRY0 + entry, s32(value))
    lib.tpw_sx_put(P_BANK0, s32(init["bank0"]))
    lib.tpw_sx_put(P_BANK1, s32(init["bank1"]))
    lib.tpw_sx_put(P_MUSICBANK, s32(init["music"]))
    data = file_bytes(init["file"], False)
    lib.tpw_sx_set_file.argtypes = [C.c_char_p, C.c_int32, C.c_int32]
    lib.tpw_sx_set_file(data, len(data), init["position"])
    obj = file_bytes(init["obj"], False)
    lib.tpw_sx_set_object_bytes.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_sx_set_object_bytes(obj, len(obj))
    lib.tpw_sx_object.restype = C.c_void_p
    lib.tpw_sx_object.argtypes = [C.c_int32]
    fixed = {LABEL_AUDIO: lib.tpw_sx_object(LABEL_AUDIO), LABEL_AUDIO2: lib.tpw_sx_object(LABEL_AUDIO2),
             LABEL_FILE: lib.tpw_sx_object(LABEL_FILE)}
    CL.host_begin(lib, fixed, case.get("q"))
    lib.tpw_sx_op.restype = C.c_int64
    lib.tpw_sx_op.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_sx_get.restype = C.c_int32
    lib.tpw_sx_get.argtypes = [C.c_int32]
    steps = []
    for op, args in case["ops"]:
        padded = (C.c_int64 * 8)(*[a if a < (1 << 63) else a - (1 << 64) for a in (list(args) + [0] * 8)[:8]])
        result = lib.tpw_sx_op(op, C.addressof(padded))
        result = s32(result) if op in RESULT_OPS else 0
        steps.append(dict(result=result, state=[lib.tpw_sx_get(i) for i in range(STATE_WORDS)]))
    log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def run_ppc(emu, case):
    return ppc_case(emu, case)


# ---------------------------------------------------------------- the cases

def make_init(wanted=0, on=0, audio=1, settings=None, bank0=0x1000, bank1=0x2000, music=0x3000, file=None, position=0, obj=None):
    return dict(wanted=wanted, on=on, audio=audio, settings=settings or {}, bank0=bank0, bank1=bank1, music=music,
                file=file or [], position=position, obj=obj or [])


def case(ops, queues=None, **kw):
    q = {key: list(values) for key, values in (queues or {}).items() if values is not None}
    return dict(ops=ops, init=make_init(**kw), q=q)


def op(kind, *args):
    return (kind, list(args) + [0] * (8 - len(args)))


LEVELS = [0, 1, 2, 7, 49, 50, 99, 100, 101, 102, 150, 255, 256, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x12345678, 0x7FFFFFFF,
          0x80000000, MASK, MASK - 1, -2, -50, -99, -100, -101, -0x8000, -0x10000]
HANDLES = [0, 1, -1, 7, 0x100, 0x7FFFFFFF, 0x80000000, 0x12345678, MASK]
ANSWERS = [0, 1, -1, 5, 0x7FFFFFFF, 0x80000000, 0x12345678, MASK]
FLAGS = [0, 1, 2, 0x100, 0x10000, 0x1000000, -1, 0x80000000, MASK]


def pat(seed, n):
    """n bytes of a pattern with no repeated neighbours (so that a swap or a shift shows)."""
    return bytes(((seed + 37 * i) * 5 + 1 + i // 3) & 0xFF for i in range(n))


def junk(seed=1):
    """Every settings entry set to an asymmetric word (the shorts to an asymmetric short)."""
    out = {}
    for entry in range(SETTINGS_ENTRIES):
        if entry in (12, 13):
            out[entry] = s32(0x1234 + 0x0101 * (entry + seed))
        elif entry in SYMMETRIC_ENTRIES:
            out[entry] = s32(((0x31 + 0x11 * seed) & 0xFF) * 0x01010101)
        else:
            out[entry] = s32((0x01020304 + 0x04040404 * (entry + seed)) & MASK) ^ (entry << 20)
    return out


def symmetric(value):
    """A word whose four bytes are equal (the same in either byte order)."""
    return s32((value & 0xFF) * 0x01010101)


def payload_bytes(seed, n):
    """The settings block's bytes as a save holds them, with equal bytes where the rain handle is (0x0c)."""
    data = bytearray(pat(seed, n))
    if n >= 0x10:
        data[0x0D:0x10] = bytes([data[0x0C]] * 3)
    return bytes(data)


def word(r):
    return s32(r.choice(LEVELS + [r.getrandbits(32), r.randrange(0, 200), r.randrange(-200, 0)]))


def random_junk(r, entries=None):
    """A random selection of settings entries with random words (the shorts as shorts)."""
    out = {}
    for entry in (entries if entries is not None else r.sample(range(SETTINGS_ENTRIES), r.randrange(0, 10))):
        out[entry] = s32(r.choice([0, 1, -1, 100, 150, r.getrandbits(32)])) if entry not in (12, 13) else s32(r.getrandbits(16))
        if entry in SYMMETRIC_ENTRIES:
            out[entry] = symmetric(out[entry])
    return out


def random_queues(r):
    return {"Event": [r.choice(ANSWERS) for _ in range(3)], "Param": [r.choice(ANSWERS) for _ in range(3)],
            "Code": [r.choice([0, 0, 1, -1, 0x5B, MASK, r.getrandbits(32)]) for _ in range(2)]}


def random_banks(r):
    return dict(bank0=s32(r.choice([0, 1, r.getrandbits(32)])), bank1=s32(r.choice([0, 2, r.getrandbits(32)])),
                music=s32(r.choice([0, 3, r.getrandbits(32)])))


# ---- SND_DipMusic

def dip_edges():
    for level in LEVELS:
        for changed in (0, 1, 0x55):
            yield case([op(OP_DIP, level)], settings={**junk(), E_CHANGED: changed})
    yield case([op(OP_DIP, 5), op(OP_DIP, 0), op(OP_DIP, 0x10001)], settings={E_CHANGED: 7})


def dip_random(r):
    return case([op(OP_DIP, word(r)) for _ in range(r.randrange(1, 4))], settings=random_junk(r))


# ---- SND_StartParkMusic

def start_edges():
    for handle in HANDLES:
        for event in ANSWERS[:5]:
            yield case([op(OP_START)], settings={**junk(), E_PARK: handle}, queues={"Event": [event], "Param": [event ^ 5]},
                       music=0x123456)
    for event, param in ((0, 0), (0, 9), (3, 0), (-1, 4)):
        yield case([op(OP_START), op(OP_START)], settings={E_PARK: 0}, queues={"Event": [event, 6], "Param": [param, 8]})
    for music in (0, 1, -1, 0x7FFFFFFF, 0x80000000):
        yield case([op(OP_START)], music=music, queues={"Event": [1], "Param": [2]})


def start_random(r):
    return case([op(OP_START) for _ in range(r.randrange(1, 3))], settings=random_junk(r), queues=random_queues(r),
                **{k: v for k, v in random_banks(r).items() if k == "music"})


# ---- SND_StopParkMusic

def stop_edges():
    for handle in HANDLES:
        yield case([op(OP_STOP)], settings={**junk(), E_PARK: handle})
        yield case([op(OP_STOP), op(OP_STOP)], settings={E_PARK: handle})


def stop_random(r):
    return case([op(OP_STOP) for _ in range(r.randrange(1, 3))], settings=random_junk(r))


# ---- SND_SetParkMusicLevel

def musiclevel_edges():
    for handle in HANDLES:
        for level in LEVELS:
            yield case([op(OP_MUSICLEVEL, level)], settings={E_PARK: handle}, queues={"Param": [handle ^ 0x55]})
    yield case([op(OP_MUSICLEVEL, 5)], settings=junk(), queues={"Param": [9]})


def musiclevel_random(r):
    return case([op(OP_MUSICLEVEL, word(r)) for _ in range(r.randrange(1, 3))], settings=random_junk(r), queues=random_queues(r))


# ---- SND_SetChatterLevel

def chatterlevel_edges():
    for on in (0, 1, 2, -1, 0x100):
        for handle in (0, 5, -1):
            for level in LEVELS:
                yield case([op(OP_CHATTERLEVEL, level)], settings={E_ON: on, E_CHATTER_HANDLE: handle, E_LEVEL: 33},
                           queues={"Event": [0x44], "Param": [0x55]}, bank1=0x777)
    # the fields around the level stay as they were
    for level in (0, 50, 150):
        yield case([op(OP_CHATTERLEVEL, level)], settings={**junk(), E_ON: 1, E_CHATTER_HANDLE: 0}, queues={"Event": [3], "Param": [4]})
        yield case([op(OP_CHATTERLEVEL, level)], settings={**junk(), E_ON: 0, E_CHATTER_HANDLE: 9}, queues={"Event": [3], "Param": [4]})
    # a start whose event answers 0 (the parameter call then gets the handle 0)
    yield case([op(OP_CHATTERLEVEL, 40)], settings={E_ON: 1}, queues={"Event": [0], "Param": [0]})
    yield case([op(OP_CHATTERLEVEL, 40), op(OP_CHATTERLEVEL, 0), op(OP_CHATTERLEVEL, 60)], settings={E_ON: 1},
               queues={"Event": [8, 9], "Param": [8, 9]})


def chatterlevel_random(r):
    return case([op(OP_CHATTERLEVEL, word(r)) for _ in range(r.randrange(1, 4))],
                settings={**random_junk(r), E_ON: r.choice([0, 1, 1, 2, -1]), E_CHATTER_HANDLE: r.choice([0, 0, 1, -1, 77])},
                queues=random_queues(r), **{k: v for k, v in random_banks(r).items() if k == "bank1"})


# ---- SND_SetChatterOnOff

def chatteronoff_edges():
    for on in LEVELS[:12] + [-1, 0x80000000, MASK, 0x10000]:
        for handle in (0, 5, -1):
            for level in (0, 1, 100, 150, -5, 0x80000000, MASK):
                yield case([op(OP_CHATTERONOFF, on)], settings={E_CHATTER_HANDLE: handle, E_LEVEL: level, E_ON: 3 - on % 4},
                           queues={"Event": [0x44], "Param": [0x55]}, bank1=0x777)
    yield case([op(OP_CHATTERONOFF, 1), op(OP_CHATTERONOFF, 1), op(OP_CHATTERONOFF, 0), op(OP_CHATTERONOFF, 1)],
               settings={**junk(), E_CHATTER_HANDLE: 0}, queues={"Event": [6, 7], "Param": [8, 9]})
    yield case([op(OP_CHATTERONOFF, 1)], settings={E_LEVEL: 12}, queues={"Event": [0], "Param": [0]})


def chatteronoff_random(r):
    return case([op(OP_CHATTERONOFF, word(r)) for _ in range(r.randrange(1, 4))],
                settings={**random_junk(r), E_CHATTER_HANDLE: r.choice([0, 0, 1, -1, 77]), E_LEVEL: word(r)},
                queues=random_queues(r), **{k: v for k, v in random_banks(r).items() if k == "bank1"})


# ---- SND_SetRainLevel

def rain_edges():
    for handle in (0, 5, -1):
        for level in LEVELS:
            yield case([op(OP_RAIN, level)], settings={E_RAIN: handle}, queues={"Event": [0x44], "Param": [0x55]}, bank0=0x888)
    yield case([op(OP_RAIN, 60), op(OP_RAIN, 0), op(OP_RAIN, 70)], settings={**junk(), E_RAIN: 0},
               queues={"Event": [6, 7], "Param": [8, 9]})
    yield case([op(OP_RAIN, 60)], settings=junk(), queues={"Event": [0], "Param": [0]})


def rain_random(r):
    return case([op(OP_RAIN, word(r)) for _ in range(r.randrange(1, 4))],
                settings={**random_junk(r), E_RAIN: r.choice([0, 0, 1, -1, 77])}, queues=random_queues(r),
                **{k: v for k, v in random_banks(r).items() if k == "bank0"})


# ---- SND_SendIndirectedEvent

INDIRECT_ARGS = [11, 22, 33, 44, 55, 66, 77]


def indirect_edges():
    for code in (0, 1, -1, 0x5B, MASK, 0x80000000, 0x7FFFFFFF):
        yield case([op(OP_INDIRECT, *INDIRECT_ARGS)], settings=junk(), queues={"Code": [code], "Event": [0x4242]})
    for code in (0, 9):
        for event in ANSWERS:
            yield case([op(OP_INDIRECT, *INDIRECT_ARGS)], queues={"Code": [code], "Event": [event]})
    for args in ([0] * 7, [-1] * 7, [MASK, 0x80000000, 0x7FFFFFFF, 1, 2, 3, 4], [1, 2, 3, 4, 5, 6, 7]):
        yield case([op(OP_INDIRECT, *args)], queues={"Code": [5], "Event": [1]})
    yield case([op(OP_INDIRECT, *INDIRECT_ARGS), op(OP_INDIRECT, 7, 6, 5, 4, 3, 2, 1)], queues={"Code": [0, 3], "Event": [9, 8]})


def indirect_random(r):
    return case([op(OP_INDIRECT, *[word(r) & MASK for _ in range(7)]) for _ in range(r.randrange(1, 3))],
                settings=random_junk(r), queues=random_queues(r))


# ---- SOUND_SaveStatus

def save_expect(case_):
    return case_


def save_edges():
    # the flags: the audio object is called only when both are nonzero
    for wanted in FLAGS:
        for on in FLAGS:
            yield case([op(OP_SAVE, 1)], wanted=wanted, on=on, audio=1, settings=junk())
    for wanted, on in ((0, 0), (0, 1), (1, 0)):
        yield case([op(OP_SAVE, 1)], wanted=wanted, on=on, audio=0, settings=junk())
    yield case([op(OP_SAVE, 0)], wanted=1, on=1, audio=1, settings=junk())
    yield case([op(OP_SAVE, 0)], wanted=0, on=0, audio=0, settings=junk(3))
    # the second audio object, and the raw bytes
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=2, settings=junk(5))
    yield case([op(OP_SAVE, 1)], wanted=0x01000000, on=0x00010000, audio=1, settings={entry: 0 for entry in range(22)})
    yield case([op(OP_SAVE, 1)], wanted=MASK, on=MASK, audio=1, settings={entry: (-1 if entry not in (12, 13) else -1)
                                                                          for entry in range(22)})
    # every field alone: a single set word shows where it lands in the file and whether it is swapped
    for entry in range(SETTINGS_ENTRIES):
        value = 0x7BCDEF01 if entry not in (12, 13) else -0x1234
        if entry in SYMMETRIC_ENTRIES:
            value = 0x7B7B7B7B
        yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, settings={entry: value})
    # a block of 0x54 bytes with a different byte in each place
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, settings=junk(9))
    # the save's copy is taken after the three words are written: a write can change the block, or the flags the end tests
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, settings=junk(), queues={"PokeWrite": [0, 0, (P_ENTRY0 + E_PARK + 1) << 32 | 0x11223344, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, settings=junk(), queues={"PokeWrite": [0, 0, 0, (P_ENTRY0 + E_LEVEL + 1) << 32 | 0x11223344, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, settings=junk(), queues={"PokeWrite": [(P_ENTRY0 + E_MUTE + 1) << 32 | 0x1234, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, queues={"PokeWrite": [(P_WANTED + 1) << 32 | 0x0, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, queues={"PokeWrite": [0, (P_WANTED + 1) << 32 | 0x0, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, queues={"PokeWrite": [0, 0, (P_ON + 1) << 32 | 0x0, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, queues={"PokeWrite": [0, 0, 0, (P_ON + 1) << 32 | 0x0, 0]})
    yield case([op(OP_SAVE, 1)], wanted=0, on=1, audio=1, queues={"PokeWrite": [0, 0, 0, (P_WANTED + 1) << 32 | 0x5, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=0, audio=1, queues={"PokeWrite": [0, 0, 0, (P_ON + 1) << 32 | 0x5, 0]})
    yield case([op(OP_SAVE, 1)], wanted=1, on=0, audio=1, queues={"PokeWrite": [(P_ON + 1) << 32 | 0x5, 0]})
    yield case([op(OP_SAVE, 1)], wanted=0, on=1, audio=1, queues={"PokeWrite": [(P_WANTED + 1) << 32 | 0x5, 0]})
    # the audio object: the command goes to the one the pointer holds at the end
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, queues={"PokeWrite": [0, 0, 0, (P_AUDIO + 1) << 32 | 2, 0]})
    # the audio object's own data follows the settings; a save to a file and a load back
    obj = [("n", 0), ("n", 0), ("n", 0), ("n", 12), ("b", pat(3, 8))]
    yield case([op(OP_SAVE, 1)], wanted=1, on=1, audio=1, settings=junk(2), obj=obj)


def save_random(r):
    wanted = r.choice([0, 1, 1, 2, MASK, 0x100])
    on = r.choice([0, 1, 1, 2, MASK, 0x10000])
    pokes = None
    if r.random() < 0.4:
        index = r.choice([P_WANTED, P_ON, P_AUDIO, P_ENTRY0 + r.choice(SAVED_ENTRIES), P_ENTRY0 + r.choice(RAW_ENTRIES)])
        value = r.choice([0, 1, 2]) if index in (P_WANTED, P_ON) else r.choice([1, 2]) if index == P_AUDIO else word(r)
        if index in (P_ENTRY0 + 12, P_ENTRY0 + 13):
            value = s32(r.getrandbits(16))
        if index - P_ENTRY0 in SYMMETRIC_ENTRIES:
            value = symmetric(value)
        queue = [0] * r.randrange(0, 4) + [(index + 1) << 32 | (value & MASK), 0]
        pokes = queue
    return case([op(OP_SAVE, r.choice([0, 1, 1]))], wanted=wanted, on=on, audio=r.choice([1, 2]),
                settings=random_junk(r, range(SETTINGS_ENTRIES) if r.random() < 0.7 else None),
                obj=[("b", pat(r.randrange(0, 50), r.randrange(0, 20)))], queues={"PokeWrite": pokes})


# ---- SOUND_LoadStatus

def load_file(saved_on, wanted, size, payload, tail):
    """The file a save made: the system-on word, the wanted word, the size, the settings (payload), then the audio object's block."""
    return [("w", saved_on), ("w", wanted), ("w", size), ("b", payload)] + tail


def object_tail(skip=8, extra=16, seed=4):
    """The audio object's block when there is no audio object to read it: three words dropped, the skip word (native order), the rest."""
    return [("n", 0x11111111), ("n", 0x22222222), ("n", 0x33333333), ("n", skip), ("b", pat(seed, extra))]


SIZES = [0x54, 0x54, 0x54, 0, 4, 0x10, 0x50, 0x53, 0x55, 0x58, 0x60, 0x100, 0x7FFFFFFF, MASK, 0x54000000, 0x1000]


def zero_uncovered(c):
    """The game swaps the settings block in place whatever the read brought in: a swapped field the read did not cover would be
    swapped from its old value (garbage on the Mac; the host's swap does nothing), so such a field starts at 0 in a case that loads."""
    init = c["init"]
    if c["ops"][0][0] != OP_LOAD:
        return c
    data = file_bytes(init["file"], True)
    size = struct.unpack("<I", data[init["position"] + 8:init["position"] + 12])[0]
    covered = min(size, 0x54, max(len(data) - init["position"] - 12, 0))
    for entry in range(SETTINGS_ENTRIES):
        end = settings_address(entry) - SETTINGS + (2 if entry in (12, 13) else 4)
        if entry not in RAW_ENTRIES and entry not in SYMMETRIC_ENTRIES and end > covered:
            init["settings"].pop(entry, None)
    return c


def load_edges():
    for c in load_edges_all():
        yield zero_uncovered(c)


def load_edges_all():
    payload = payload_bytes(1, 0x54)
    # savedOn: zero or not (any nonzero byte), the old wanted value: zero or not, with and without the audio object
    for saved_on in (0, 1, 2, 0x100, 0x10000, 0x1000000, MASK, 0x80000000):
        for old in (0, 1, 0x5555):
            for wanted in (0, 1, 0x01020304):
                for audio in (0, 1):
                    yield case([op(OP_LOAD, 1)], wanted=old, audio=audio, settings=junk(),
                               file=load_file(saved_on, wanted, 0x54, payload, object_tail()))
    # the sizes
    for size in SIZES:
        for audio in (0, 1):
            for saved_on in (0, 1):
                yield case([op(OP_LOAD, 1)], wanted=3, on=1, audio=audio, settings=junk(2),
                           file=load_file(saved_on, 0x77, size, payload[:min(size, 0x54)], object_tail()))
    # the skip word: the seek is relative to the position by skip - 4
    for skip in (4, 0, 100, -1, -4, -100, 8, 3, 5, 12, 0x7FFFFFFF, 0x80000000, 16, 28, 20):
        for extra in (0, 4, 40, 200):
            yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload, object_tail(skip, extra)))
        yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload, object_tail(skip, 30)), position=0)
    # a file that ends early: the skip word is not there (stays 0), or the block is missing
    yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload, []))
    yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload, [("n", 1), ("n", 2), ("n", 3)]))
    yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload, [("n", 1), ("n", 2)]))
    yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload[:0x20], []))
    yield case([op(OP_LOAD, 1)], wanted=1, audio=1, file=load_file(1, 1, 0x54, payload[:0x20], []))
    # the file pointer NULL
    yield case([op(OP_LOAD, 0)], wanted=1, audio=1, file=load_file(1, 1, 0x54, payload, []))
    yield case([op(OP_LOAD, 0)], wanted=1, audio=0, file=load_file(0, 1, 0x54, payload, []))
    # the audio object gets Load (the file, the flag 1) and then SetPresetReverb with the block's reverb word, read after the Load
    for reverb in (0, 1, -1, 0x01020304, 0x80000000, MASK):
        words = bytearray(payload_bytes(7, 0x54))
        words[0x14:0x18] = struct.pack("<I", reverb & MASK)
        yield case([op(OP_LOAD, 1)], wanted=1, audio=1, settings=junk(), file=load_file(1, 1, 0x54, bytes(words), []))
        yield case([op(OP_LOAD, 1)], wanted=1, audio=1, file=load_file(1, 1, 0x54, bytes(words), []),
                   queues={"PokeCmd": [(P_ENTRY0 + E_F14 + 1) << 32 | 0x1357, 0]})
    # the reverb word is read from the block after the swap, and the audio object after the first command is read again
    yield case([op(OP_LOAD, 1)], wanted=1, audio=1, file=load_file(1, 1, 0x54, payload, []),
               queues={"PokeCmd": [(P_AUDIO + 1) << 32 | 2, 0]})
    yield case([op(OP_LOAD, 1)], wanted=1, audio=2, file=load_file(1, 1, 0x54, payload, []),
               queues={"PokeCmd": [(P_AUDIO + 1) << 32 | 1, 0]})
    # the audio object pointer can change during the reads: the branch follows what is there after the swaps
    yield case([op(OP_LOAD, 1)], wanted=1, audio=1, file=load_file(1, 1, 0x54, payload, object_tail()),
               queues={"PokeRead": [0, 0, 0, (P_AUDIO + 1) << 32 | 0, 0]})
    yield case([op(OP_LOAD, 1)], wanted=1, audio=0, file=load_file(1, 1, 0x54, payload, object_tail()),
               queues={"PokeRead": [0, 0, 0, (P_AUDIO + 1) << 32 | 1, 0]})
    # a block of sizes with the swap of each field
    for entry in range(SETTINGS_ENTRIES):
        data = bytearray(0x54)
        a = settings_address(entry) - SETTINGS
        data[a:a + (2 if entry in (12, 13) else 4)] = bytes([0x05] * 4 if entry in SYMMETRIC_ENTRIES else [0x01, 0x02, 0x03, 0x04][:2 if entry in (12, 13) else 4])
        yield case([op(OP_LOAD, 1)], wanted=1, audio=1, settings=junk(), file=load_file(1, 5, 0x54, bytes(data), []))
    # a save and a load of it (the PowerPC and the host write and read the same bytes)
    for wanted, on, audio in ((1, 1, 1), (0, 0, 1), (1, 0, 1), (1, 1, 2), (0, 1, 0), (1, 0, 0)):
        obj = object_tail(8, 4)
        yield case([op(OP_SAVE, 1), op(OP_REWIND), op(OP_LOAD, 1)], wanted=wanted, on=on, audio=audio, settings=junk(4), obj=obj)
        yield case([op(OP_SAVE, 1), op(OP_REWIND), op(OP_SAVE, 1), op(OP_REWIND), op(OP_LOAD, 1)],
                   wanted=wanted, on=on, audio=audio, settings=junk(6), obj=obj)


def random_load_file(r):
    size = r.choice(SIZES)
    payload_length = min(size, 0x54) if r.random() < 0.9 else r.randrange(0, 0x54)
    tail = []
    if payload_length == min(size, 0x54) and r.random() < 0.85:
        tail = object_tail(r.choice([4, 0, 8, 100, -1, -4, 12, 3, 28, 0x7FFFFFFF]), r.choice([0, 4, 16, 64]), r.randrange(0, 20))
        if r.random() < 0.15:
            tail = tail[:r.randrange(0, 5)]
    return load_file(r.choice([0, 1, 2, 0x100, MASK, 0x01000000]), r.choice([0, 1, 0x01020304, MASK, r.getrandbits(32)]), size,
                     payload_bytes(r.randrange(0, 50), payload_length), tail)


def load_random(r):
    return zero_uncovered(load_random_once(r))


def load_random_once(r):
    pokes = {}
    if r.random() < 0.25:
        pokes["PokeCmd"] = [r.choice([(P_ENTRY0 + E_F14 + 1) << 32 | r.getrandbits(32), (P_AUDIO + 1) << 32 | 2]), 0]
    if r.random() < 0.15:
        pokes["PokeRead"] = [0, 0, 0, (P_AUDIO + 1) << 32 | r.choice([0, 1, 2]), 0]
    return case([op(OP_LOAD, r.choice([1, 1, 1, 0]))], wanted=r.choice([0, 1, 5, MASK]), on=r.choice([0, 1]),
                audio=r.choice([0, 1, 1, 2]), settings=random_junk(r), file=random_load_file(r), queues=pokes)


# ---- every function in mixed scripts

MIXED_OPS = [OP_DIP, OP_START, OP_STOP, OP_MUSICLEVEL, OP_CHATTERLEVEL, OP_CHATTERONOFF, OP_RAIN, OP_INDIRECT, OP_SAVE, OP_LOAD]


SYMMETRIC_ANSWERS = [0, -1, 0x01010101, 0x7F7F7F7F, symmetric(0x80), symmetric(0xFF), symmetric(0x2A)]


def mixed_op(r, kind):
    if kind == OP_SAVE:
        return [op(kind, r.choice([1, 1, 0]))]
    if kind == OP_LOAD:
        # a load needs a file it can read whole (the block, then the settings in place of whatever the earlier operations left)
        return [op(OP_RESETFILE), op(kind, r.choice([1, 1, 0]))]
    if kind == OP_INDIRECT:
        return [op(kind, *[word(r) & MASK for _ in range(7)])]
    if kind in (OP_START, OP_STOP):
        return [op(kind)]
    return [op(kind, word(r) & MASK)]


def mixed_random(r):
    audio = r.choice([0, 1, 1, 2])
    kinds = [k for k in MIXED_OPS if not (audio == 0 and k == OP_SAVE)]
    ops = []
    for _ in range(r.randrange(2, 12)):
        kind = r.choice(kinds)
        ops += mixed_op(r, kind)
        if kind == OP_SAVE and r.random() < 0.5:
            ops.append(op(OP_REWIND))
    obj = object_tail(r.choice([4, 8, 12, 0]), r.choice([0, 8, 20]))
    # the rain handle is saved raw (in the machine's byte order): its answers have equal bytes
    q = {"Event": [r.choice(SYMMETRIC_ANSWERS) for _ in range(3)], "Param": [r.choice(SYMMETRIC_ANSWERS) for _ in range(3)],
         "Code": [r.choice([0, 1, 0x5B, MASK, 0, 7]) for _ in range(3)]}
    file = load_file(r.choice([0, 1, 1, 2]), r.choice([0, 1, 0x01020304]), 0x54, payload_bytes(r.randrange(0, 50), 0x54),
                     object_tail(r.choice([4, 8, 12, 0]), r.choice([0, 8, 20])) if r.random() < 0.8 else [])
    return case(ops, wanted=r.choice([0, 1, 1, 5]), on=r.choice([0, 1, 1]), audio=audio, settings=random_junk(r),
                file=file, obj=obj, queues=q, **random_banks(r))


def mixed_edges():
    script = [op(OP_START), op(OP_MUSICLEVEL, 50), op(OP_CHATTERONOFF, 1), op(OP_CHATTERLEVEL, 80), op(OP_RAIN, 30),
              op(OP_INDIRECT, 1, 2, 3, 4, 5, 6, 7), op(OP_DIP, 1), op(OP_SAVE, 1), op(OP_REWIND), op(OP_RESETFILE), op(OP_LOAD, 1),
              op(OP_STOP), op(OP_RAIN, 0), op(OP_CHATTERLEVEL, 0), op(OP_CHATTERONOFF, 0)]
    queues = {"Event": [10, 11, 12, 13], "Param": [0x14141414, 0x15151515, 0x16161616, 0x17171717, 0x18181818], "Code": [9]}
    full = load_file(1, 1, 0x54, payload_bytes(1, 0x54), object_tail())
    yield case(script, wanted=1, on=1, audio=1, settings={**junk(), E_PARK: 0, E_CHATTER_HANDLE: 0, E_RAIN: 0}, file=full,
               obj=object_tail(8, 4), queues=queues)
    yield case(script[:7] + script[9:], wanted=1, on=1, audio=0, file=full, queues=queues)


TESTS = [
    FnTest("SND_DipMusic 0x100deb04", run_ppc, host_case, edge=dip_edges, rnd=dip_random),
    FnTest("SND_StartParkMusic 0x100deb40", run_ppc, host_case, edge=start_edges, rnd=start_random),
    FnTest("SND_StopParkMusic 0x100debd4", run_ppc, host_case, edge=stop_edges, rnd=stop_random),
    FnTest("SND_SetParkMusicLevel 0x100dec30", run_ppc, host_case, edge=musiclevel_edges, rnd=musiclevel_random),
    FnTest("SND_SetChatterLevel 0x100dec9c", run_ppc, host_case, edge=chatterlevel_edges, rnd=chatterlevel_random),
    FnTest("SND_SetChatterOnOff 0x100deda4", run_ppc, host_case, edge=chatteronoff_edges, rnd=chatteronoff_random),
    FnTest("SND_SetRainLevel 0x100dee84", run_ppc, host_case, edge=rain_edges, rnd=rain_random),
    FnTest("SND_SendIndirectedEvent 0x100df858", run_ppc, host_case, edge=indirect_edges, rnd=indirect_random),
    FnTest("SOUND_SaveStatus 0x100de1a0", run_ppc, host_case, edge=save_edges, rnd=save_random),
    FnTest("SOUND_LoadStatus 0x100de48c", run_ppc, host_case, edge=load_edges, rnd=load_random),
    FnTest("every function in mixed scripts", run_ppc, host_case, edge=mixed_edges, rnd=mixed_random),
]
