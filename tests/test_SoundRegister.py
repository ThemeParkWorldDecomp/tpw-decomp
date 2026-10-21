"""Unit w65, the sound registration layer, against libtpw_unit_SoundRegister.dylib (tests/unit_SoundRegister.cpp), in the main app
("SimTheme Park"):
  src/game/Sound.cpp, src/game/park/ParkSound.cpp  SOUND_Malloc 0x100dc5e4, SOUND_Free 0x100dc64c, SOUND_RegisterCatagory (plain) 0x100dc6ac and (resource)
                         0x100dc74c, SOUND_FreeCatagory 0x100dc89c, SND_RegisterGlobalLobbySounds 0x100def68,
                         SND_RegisterLocalLobbySounds 0x100df02c, SND_FreeGlobalLobbySounds 0x100df1bc, SND_FreeLocalLobbySounds
                         0x100df234, SND_RegisterGlobalSounds 0x100df310, SND_RegisterLocalParkSounds 0x100df4d4,
                         SND_FreeGlobalSounds 0x100df67c, SND_FreeLocalParkSounds 0x100df778
  src/game/Sound.cpp, src/game/park/ParkSound.cpp        the state they work on

The functions run as the original, one script of operations per case, with the state block (the flags, the allocation counter, the
sixteen words of the banks and the island table, and the table's rows) read after every operation. Every callee outside the unit is a
fake that records the same event on both sides: the audio object (a fake object in persistent PowerPC memory whose vtable has the
two slots the game calls, +0x10 RegisterCatagory and +0x20 SystemCommand; the host's is a TbAudioSystem), the resource manager's
FindResource (the queue "Find": 0 nothing, 1 to 4 a resource, 5 resource 1 with the answer 2, 6 nothing with the answer -1),
CTPSoundResource::FindSoundFile (the queue "Sound" is its answer; a nonzero answer writes the path "P_<name>"), GAME_ASSERTMSG, and
the library heap LbMem_Alloc and LbMem_Free (zeroed blocks, labelled). The bullfrog strings are functional stubs on the PowerPC side
(real on the host). The command object SystemCommand receives is compared by its category and by its vtable word, which has to be the
imported vtable's address the original loads (the word at 0x102611d0): the host records whether the object is a FlushCatagory.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_SoundRegister"
MASK = 0xFFFFFFFF

# the functions
MALLOC, FREE, REG_PLAIN, REG_RES, FREE_CAT = 0x100DC5E4, 0x100DC64C, 0x100DC6AC, 0x100DC74C, 0x100DC89C
REG_GLOBAL, REG_GLOBAL_LOBBY, REG_LOCAL_LOBBY, REG_LOCAL_PARK = 0x100DF310, 0x100DEF68, 0x100DF02C, 0x100DF4D4
FREE_GLOBAL, FREE_GLOBAL_LOBBY, FREE_LOCAL_LOBBY, FREE_LOCAL_PARK = 0x100DF67C, 0x100DF1BC, 0x100DF234, 0x100DF778
# the callees and the data
GAME_ASSERTMSG, FIND_RESOURCE, FIND_SOUND_FILE = 0x1000DDE8, 0x10165448, 0x101632AC
G_GAME, G_TPGLOBALS = 0x1037F134, 0x103B74C8
ALLOCS, AUDIO, WANTED, ON, BLOCK = 0x102F7C30, 0x102F7C34, 0x102F7C38, 0x102F7C3C, 0x102F7C94
FLUSH_VTABLE_WORD = 0x102611D0      # the TOC word the original stores as the command's vtable
NAME_OFFSETS = (0x4768, 0x477C, 0x4790, 0x47A4, 0x47B8)
DEFAULT_NAMES = ["GlobalSounds", "GlobalSpeech", "LocalSounds_", "LocalSpeech_", "LocalMusic_"]
LABEL_MANAGER, LABEL_AUDIO, LABEL_RESOURCE = 1, 2, 3

# the operations of a script: (operation, a, b, s1, s2)
(OP_MALLOC, OP_MALLOC_FAIL, OP_FREE, OP_REG_PLAIN, OP_REG_RES, OP_FREE_CAT, OP_GLOBAL, OP_GLOBAL_LOBBY, OP_LOCAL_LOBBY,
 OP_FREE_GLOBAL_LOBBY, OP_FREE_LOCAL_LOBBY, OP_LOCAL_PARK, OP_FREE_GLOBAL, OP_FREE_LOCAL_PARK) = range(14)
STATE_WORDS = 17     # allocations, audio object, wanted, on, banks 0 to 7, advisor, music, thunder, island table, island count
BANK_WORDS = list(range(4, 15))


def s32(v):
    return T.s32(v)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


STUBS = None
AUDIO_OBJECT = None


def make_audio(e):
    """The fake audio object in persistent memory: a vtable with the two slots the game calls."""
    def register(e):
        # r3 the hidden result buffer, r4 this, r5 the path, r6 the name, r7 the handle
        L = e.cl
        L.rec("Audio.RegisterCatagory", (L.lab(e.r(4)),), (e.cstr(e.r(5))[:159], e.cstr(e.r(6))[:159]))
        ws(e, e.r(7), [L.ret("Handle", 0)])

    def command(e):
        # r3 the hidden result buffer, r4 this, r5 the command: its vtable word and its category
        L = e.cl
        cmd = e.r(5)
        L.rec("Audio.SystemCommand", (L.lab(e.r(4)), int(e.u32(cmd) == e.u32(FLUSH_VTABLE_WORD)), e.u32(cmd + 4)))

    vt = T.persist_alloc(e, 0x40)
    ws(e, vt + 0x10, [T.fake_function(e, register)])
    ws(e, vt + 0x20, [T.fake_function(e, command)])
    return T.persist_alloc(e, 16, struct.pack(">I", vt) + b"\0" * 12)


def build_stubs():
    S = CL.Stubs()

    def assert_(e):
        e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def alloc(e):
        L, size = e.cl, e.r(3)
        if L.alloc_fail:
            L.rec("LbMem_Alloc", (size, 0))
            e.set_r(3, 0)
            return
        L.rec("LbMem_Alloc", (size, 1))
        block = e.alloc(max(size, 4), b"\0" * max(size, 4))
        L.newlab(block)
        e.set_r(3, block)

    def free(e):
        L, block = e.cl, e.r(3)
        L.rec("LbMem_Free", (L.lab(block),))
        if L.lab(block) > 0:
            L.forget(block)

    def find_resource(e):
        L = e.cl
        L.rec("FindResource", (L.lab(e.r(3)),), (CL.tds_text(e, e.r(4)),))
        code = L.ret("Find", 0)
        if 1 <= code <= 4:
            ws(e, e.r(5), [L.resources[code - 1]])
            e.set_r(3, 1)
        elif code == 5:
            ws(e, e.r(5), [L.resources[0]])
            e.set_r(3, 2)
        else:
            e.set_r(3, T.u32(-1) if code == 6 else 0)

    def find_sound(e):
        L = e.cl
        name = CL.tds_text(e, e.r(4))
        L.rec("FindSoundFile", (L.lab(e.r(3)),), (name,))
        answer = L.ret("Sound", 1)
        if answer != 0:
            CL.tds_set(e, e.r(5), "P_" + name)
        e.set_r(3, answer & MASK)

    def ctor0(e):
        CL.tds_set(e, e.r(3), "")

    def ctor_text(e):
        CL.tds_set(e, e.r(3), e.cstr(e.r(4)) if e.r(4) else "")

    def copy(e):
        CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(4)))

    def append_text(e):
        CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + e.cstr(e.r(4)))

    def append(e):
        CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + CL.tds_text(e, e.r(4)))

    S.internal(GAME_ASSERTMSG, assert_)
    S.internal(FIND_RESOURCE, find_resource)
    S.internal(FIND_SOUND_FILE, find_sound)
    S.imp("LbMem_Alloc__FUi", alloc)
    S.imp("LbMem_Free__FPv", free)
    S.imp("__ct__26TbDynamicStringTemplate<c>Fv", ctor0)
    S.imp("__ct__26TbDynamicStringTemplate<c>FPCc", ctor_text)
    S.imp("__ct__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>", copy)
    S.imp("__dt__26TbDynamicStringTemplate<c>Fv", lambda e: None)
    S.imp("__as__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>", copy)
    S.imp("__apl__26TbDynamicStringTemplate<c>FPCc", append_text)
    S.imp("__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", append)
    return S


def setup(runner):
    global STUBS, AUDIO_OBJECT
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()
    AUDIO_OBJECT = make_audio(runner.emu)


# ---------------------------------------------------------------- the two sides

def cstr(e, text):
    return e.alloc(len(text) + 1, text.encode("latin-1") + b"\0")


def state_ppc(e, L):
    """The state words, in the order of tpw_sr_get, then the island table's rows."""
    words = [s32(e.u32(ALLOCS)), int(e.u32(AUDIO) != 0), e.u32(WANTED), e.u32(ON)]
    words += [s32(e.u32(BLOCK + 4 * i)) for i in range(8)]
    words += [s32(e.u32(BLOCK + 0x20)), s32(e.u32(BLOCK + 0x24)), s32(e.u32(BLOCK + 0x28))]
    table, count = e.u32(BLOCK + 0x2C), s32(e.u32(BLOCK + 0x30))
    words += [L.lab(table), count]
    rows = [s32(e.u32(table + 4 * i)) for i in range(2 * count)] if table and 0 < count <= 40 else []
    return words, rows


def ppc_case(e, case):
    e.heap_in_scratch = True
    resources = [e.alloc(64, b"\xa5" * 64) for _ in range(4)]
    manager, stranger = e.alloc(16, b"\xa5" * 16), e.alloc(16, b"\xa5" * 16)
    fixed = {manager: LABEL_MANAGER, AUDIO_OBJECT: LABEL_AUDIO}
    fixed.update({a: LABEL_RESOURCE + i for i, a in enumerate(resources)})
    e.cl = L = CL.PpcLog(fixed, case.get("q"))
    L.alloc_fail = False
    L.resources = resources
    saved = STUBS.activate(e)
    try:
        init = case["init"]
        ws(e, G_GAME + 0x34, [manager])
        for offset, name in zip(NAME_OFFSETS, case["names"]):
            CL.tds_set(e, G_TPGLOBALS + offset, name)
        e.write(BLOCK, b"\0" * 0x34)
        ws(e, ALLOCS, [init.get("allocs", 0)])
        ws(e, AUDIO, [AUDIO_OBJECT if init.get("audio", 1) else 0])
        ws(e, WANTED, [init.get("wanted", 0)])
        ws(e, ON, [init.get("on", 0)])
        for index, value in init.get("words", {}).items():
            ws(e, BLOCK + 4 * (index - 4), [value])
        blocks = [0] * 16
        steps = []
        for op, a, b, s1, s2 in case["ops"]:
            result = 0
            if op in (OP_MALLOC, OP_MALLOC_FAIL):
                L.alloc_fail = op == OP_MALLOC_FAIL
                blocks[b] = T.call_ppc(e, MALLOC, [a & MASK])[0]
                L.alloc_fail = False
                result = L.lab(blocks[b])
            elif op == OP_FREE:
                T.call_ppc(e, FREE, [stranger if a == 99 else 0 if a < 0 else blocks[a]])
                if 0 <= a < 16:
                    blocks[a] = 0
            elif op == OP_REG_PLAIN:
                result = s32(T.call_ppc(e, REG_PLAIN, [cstr(e, s1), cstr(e, s2)])[0])
            elif op == OP_REG_RES:
                result = s32(T.call_ppc(e, REG_RES, [cstr(e, s1), resources[a]])[0])
            elif op == OP_FREE_CAT:
                T.call_ppc(e, FREE_CAT, [a & MASK])
            elif op == OP_LOCAL_LOBBY:
                result = s32(T.call_ppc(e, REG_LOCAL_LOBBY, [cstr(e, s1)])[0])
            elif op == OP_LOCAL_PARK:
                theme = e.alloc(0x14)
                CL.tds_set(e, theme, s1)
                T.call_ppc(e, REG_LOCAL_PARK, [theme])
            else:
                address = {OP_GLOBAL: REG_GLOBAL, OP_GLOBAL_LOBBY: REG_GLOBAL_LOBBY, OP_FREE_GLOBAL_LOBBY: FREE_GLOBAL_LOBBY,
                           OP_FREE_LOCAL_LOBBY: FREE_LOCAL_LOBBY, OP_FREE_GLOBAL: FREE_GLOBAL, OP_FREE_LOCAL_PARK: FREE_LOCAL_PARK}[op]
                T.call_ppc(e, address, [])
            words, rows = state_ppc(e, L)
            steps.append(dict(result=result, state=words, rows=rows))
        return dict(steps=steps, log=L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_case(lib, case):
    init = case["init"]
    names = (C.c_char_p * 5)(*[n.encode("latin-1") for n in case["names"]])
    lib.tpw_sr_begin.argtypes = [C.c_void_p]
    lib.tpw_sr_begin(names)
    lib.tpw_sr_put.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sr_put(0, init.get("allocs", 0))
    lib.tpw_sr_put(1, init.get("audio", 1))
    lib.tpw_sr_put(2, s32(init.get("wanted", 0)))
    lib.tpw_sr_put(3, s32(init.get("on", 0)))
    for index, value in init.get("words", {}).items():
        lib.tpw_sr_put(index, s32(value))
    lib.tpw_sr_object.restype = C.c_void_p
    lib.tpw_sr_object.argtypes = [C.c_int32]
    fixed = {label: lib.tpw_sr_object(label) for label in range(1, 7)}   # (the stranger, 7, has no label)
    CL.host_begin(lib, fixed, case.get("q"))
    lib.tpw_sr_op.restype = C.c_int64
    lib.tpw_sr_op.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_char_p, C.c_char_p]
    lib.tpw_sr_get.restype = C.c_int32
    lib.tpw_sr_get.argtypes = [C.c_int32]
    lib.tpw_sr_table.restype = C.c_int32
    lib.tpw_sr_table.argtypes = [C.c_int32]
    steps = []
    for op, a, b, s1, s2 in case["ops"]:
        result = lib.tpw_sr_op(op, a, b, s1.encode("latin-1") if s1 is not None else None,
                               s2.encode("latin-1") if s2 is not None else None)
        words = [lib.tpw_sr_get(i) for i in range(STATE_WORDS)]
        words[2] &= MASK
        words[3] &= MASK
        count = words[16]
        rows = [lib.tpw_sr_table(i) for i in range(2 * count)] if words[15] > 0 and 0 < count <= 40 else []
        steps.append(dict(result=s32(result & MASK) if op in (OP_REG_PLAIN, OP_REG_RES, OP_LOCAL_LOBBY) else result, state=words,
                          rows=rows))
    log = CL.host_end(lib)
    lib.tpw_sr_end()
    return dict(steps=steps, log=log)


def run_ppc(emu, case):
    return ppc_case(emu, case)


# ---------------------------------------------------------------- the cases

def case(ops, wanted=1, on=1, audio=1, words=None, allocs=0, find=None, sound=None, handle=None, names=None):
    q = {}
    for key, values in (("Find", find), ("Sound", sound), ("Handle", handle)):
        if values is not None:
            q[key] = list(values)
    init = dict(wanted=wanted, on=on, audio=audio, words=words or {}, allocs=allocs)
    return dict(ops=ops, init=init, q=q, names=names or DEFAULT_NAMES)


def op(kind, a=0, b=0, s1=None, s2=None):
    return (kind, a, b, s1, s2)


FLAGS = [(1, 1), (0, 0), (1, 0), (0, 1), (2, 3), (MASK, MASK), (0x100, 0x80000000), (0, MASK)]
HANDLES = [[0], [1], [MASK], [0x12345678], [3, 0, MASK, 7, 0x80000000, 9], [5, 6, 7, 8, 9, 10, 11, 12]]
SOUNDS = [[1], [0], [2], [0xFF], [0x100], [0x101], [0x1FF], [MASK - 254], [1, 0, 1, 1, 0, 1], [0, 1, 1, 1, 1, 1], [1, 1, 0, 1, 1, 1],
          [1, 1, 1, 0, 1, 1], [1, 1, 1, 1, 0, 1], [1, 1, 1, 1, 1, 0]]
FINDS = [[1, 1], [0, 1], [1, 0], [0, 0], [2, 3], [5, 1], [1, 5], [6, 1], [1, 6], [4, 4], [3, 0], [0, 2]]


def run(*ops):
    return list(ops)


def banks(r, zero=0.3):
    return {i: (0 if r.random() < zero else r.choice([1, 2, MASK, 0x80000000, r.getrandbits(32)])) for i in BANK_WORDS}


def random_queue(r, values, n=60):
    return [r.choice(values) if not callable(values) else values(r) for _ in range(r.randrange(1, n))]


def random_find(r, n=24):
    return [r.choice([0, 1, 1, 1, 2, 3, 4, 5, 6]) for _ in range(r.randrange(1, n))]


def random_handles(r, n=40):
    return [r.choice([0, 1, MASK, r.getrandbits(32), r.randrange(1, 100)]) for _ in range(r.randrange(1, n))]


def random_sounds(r, n=40):
    return [r.choice([1, 1, 1, 1, 0, 2, 0x101, 0xFF, 0x100]) for _ in range(r.randrange(1, n))]


def random_flags(r):
    return r.choice([(1, 1), (1, 1), (1, 1), (0, 0), (1, 0), (0, 1), (r.getrandbits(32), r.getrandbits(32))])


def random_names(r):
    if r.random() < 0.7:
        return DEFAULT_NAMES
    return [r.choice(["GlobalSounds", "", "x", "Name_" + str(r.randrange(100)), "A" * r.randrange(1, 60)]) for _ in range(5)]


def random_text(r):
    return r.choice(["", "a", "Island", "fantasy", "hallow", "jungle", "Theme_" + str(r.randrange(1000)), "x" * r.randrange(1, 70)])


def random_case(r, ops_maker):
    wanted, on = random_flags(r)
    words = banks(r) if r.random() < 0.5 else {}
    return case(ops_maker(r), wanted=wanted, on=on, audio=r.choice([1, 1, 1, 0]) if (wanted == 0 or on == 0) else 1, words=words,
                find=random_find(r), sound=random_sounds(r), handle=random_handles(r), names=random_names(r),
                allocs=r.choice([0, 0, 0, 5, -3, 0x7FFFFFF0]))


def with_system(ops, **kw):
    """Edge cases of one script under the flag combinations that matter: on, wanted only, on only, off."""
    for wanted, on in FLAGS:
        yield case(ops, wanted=wanted, on=on, audio=1 if wanted and on else 1, **kw)


# SOUND_Malloc, SOUND_Free

def malloc_edges():
    for size in (0, 1, 4, 8, 0x400, 0x7FF):
        yield case(run(op(OP_MALLOC, size, 0)))
        yield case(run(op(OP_MALLOC_FAIL, size, 0)))
    yield case(run(op(OP_MALLOC, 8, 0), op(OP_MALLOC, 16, 1), op(OP_MALLOC, 0, 2), op(OP_MALLOC_FAIL, 24, 3)))
    yield case(run(op(OP_MALLOC, 8, 0)), allocs=-3)
    yield case(run(op(OP_MALLOC, 8, 0)), allocs=0x7FFFFFF0)


def malloc_random(r):
    ops = []
    for _ in range(r.randrange(1, 8)):
        kind = r.choice([OP_MALLOC, OP_MALLOC, OP_MALLOC_FAIL, OP_FREE])
        ops.append(op(kind, r.choice([0, 1, 7, 8, 64, r.randrange(1, 0x300)]), r.randrange(4)) if kind != OP_FREE
                   else op(OP_FREE, r.choice([-1, 0, 1, 2, 3, 99])))
    return case(ops, allocs=r.choice([0, 0, 3, -5, 100]))


def free_edges():
    yield case(run(op(OP_FREE, -1)))
    yield case(run(op(OP_FREE, 99)))
    yield case(run(op(OP_FREE, 99)), allocs=-4)
    yield case(run(op(OP_MALLOC, 8, 0), op(OP_FREE, 0)))
    yield case(run(op(OP_MALLOC, 8, 0), op(OP_MALLOC, 8, 1), op(OP_FREE, 1), op(OP_FREE, 0), op(OP_FREE, -1)))
    yield case(run(op(OP_MALLOC, 0, 0), op(OP_FREE, 0)))
    yield case(run(op(OP_MALLOC_FAIL, 8, 0), op(OP_FREE, 0)))


def free_random(r):
    ops = [op(OP_MALLOC, r.randrange(1, 100), i) for i in range(r.randrange(0, 4))]
    ops += [op(OP_FREE, r.choice([-1, 0, 1, 2, 3, 99, 5])) for _ in range(r.randrange(1, 6))]
    return case(ops)


# SOUND_RegisterCatagory, the plain form

def plain_edges():
    for words in ((None, None),):
        for handle in HANDLES:
            for cases_ in with_system(run(op(OP_REG_PLAIN, 0, 0, "Data/bank.snd", "cat_x")), handle=handle):
                yield cases_
    yield case(run(op(OP_REG_PLAIN, 0, 0, "", "")), handle=[7])
    yield case(run(op(OP_REG_PLAIN, 0, 0, "a", "b"), op(OP_REG_PLAIN, 0, 0, "c", "d")), handle=[1, MASK])
    for wanted, on in ((0, 0), (0, 1), (1, 0)):
        yield case(run(op(OP_REG_PLAIN, 0, 0, "p", "n")), wanted=wanted, on=on, audio=0)


def plain_random(r):
    def ops(r):
        return [op(OP_REG_PLAIN, 0, 0, random_text(r), random_text(r)) for _ in range(r.randrange(1, 5))]
    return random_case(r, ops)


# SOUND_RegisterCatagory, the resource form

def res_edges():
    for sound in SOUNDS:
        for wanted, on in ((1, 1), (0, 1), (1, 0)):
            yield case(run(op(OP_REG_RES, 2, 0, "cat_music")), wanted=wanted, on=on, sound=sound, handle=[5])
    for handle in HANDLES:
        yield case(run(op(OP_REG_RES, 0, 0, "cat_ambient"), op(OP_REG_RES, 3, 0, "cat_ui")), handle=handle)
    for index in range(4):
        yield case(run(op(OP_REG_RES, index, 0, "")), handle=[9])
    yield case(run(op(OP_REG_RES, 1, 0, "n" * 70)), handle=[9])
    yield case(run(op(OP_REG_RES, 0, 0, "cat_x")), wanted=0, on=0, audio=0)


def res_random(r):
    def ops(r):
        return [op(OP_REG_RES, r.randrange(4), 0, random_text(r)) for _ in range(r.randrange(1, 6))]
    return random_case(r, ops)


# SOUND_FreeCatagory

def freecat_edges():
    for category in (0, 1, 7, MASK, 0x7FFFFFFF, 0x80000000, 0x12345678):
        for cases_ in with_system(run(op(OP_FREE_CAT, category))):
            yield cases_
    yield case(run(op(OP_FREE_CAT, 3), op(OP_FREE_CAT, 4)))
    yield case(run(op(OP_FREE_CAT, 3)), wanted=0, on=0, audio=0)
    yield case(run(op(OP_FREE_CAT, 3)), wanted=0, on=1, audio=0)
    yield case(run(op(OP_FREE_CAT, 3)), wanted=1, on=0, audio=0)


def freecat_random(r):
    def ops(r):
        return [op(OP_FREE_CAT, r.choice([0, 1, MASK, r.getrandbits(32)])) for _ in range(r.randrange(1, 5))]
    return random_case(r, ops)


# SND_RegisterGlobalSounds

def global_edges():
    for find in FINDS:
        yield case(run(op(OP_GLOBAL)), find=find, handle=[11, 12, 13, 14, 15, 16])
    for sound in SOUNDS:
        yield case(run(op(OP_GLOBAL)), find=[1, 1], sound=sound, handle=[11, 12, 13, 14, 15, 16])
    for wanted, on in FLAGS:
        yield case(run(op(OP_GLOBAL)), wanted=wanted, on=on, find=[1, 1], handle=[11, 12, 13, 14, 15, 16])
    for index in (4, 5, 6, 7, 8, 9):   # banks 0 to 5: each one set trips its own assertion
        yield case(run(op(OP_GLOBAL)), words={index: 3}, find=[1, 1], handle=[11, 12, 13, 14, 15, 16])
    yield case(run(op(OP_GLOBAL)), words={i: MASK for i in range(4, 10)}, find=[1, 1], handle=[1])
    yield case(run(op(OP_GLOBAL)), words={10: 5, 11: 6, 12: 7, 13: 8, 14: 9}, find=[1, 1], handle=[1, 2, 3, 4, 5, 6])
    yield case(run(op(OP_GLOBAL), op(OP_GLOBAL)), find=[1, 1, 1, 1], handle=[11, 12, 13, 14, 15, 16])
    yield case(run(op(OP_GLOBAL)), find=[1, 1], names=["a", "b", "c", "d", "e"])


def global_random(r):
    return random_case(r, lambda r: [op(OP_GLOBAL) for _ in range(r.randrange(1, 3))])


# SND_RegisterGlobalLobbySounds

def global_lobby_edges():
    for find in ([1], [0], [2], [5], [6], [4]):
        yield case(run(op(OP_GLOBAL_LOBBY)), find=find, handle=[21])
    for wanted, on in FLAGS:
        yield case(run(op(OP_GLOBAL_LOBBY)), wanted=wanted, on=on, find=[1], handle=[MASK])
    for sound in SOUNDS[:6]:
        yield case(run(op(OP_GLOBAL_LOBBY)), find=[1], sound=sound, handle=[21])
    yield case(run(op(OP_GLOBAL_LOBBY)), words={14: 1}, find=[1], handle=[21])
    yield case(run(op(OP_GLOBAL_LOBBY)), words={14: MASK}, find=[0], handle=[21])
    yield case(run(op(OP_GLOBAL_LOBBY), op(OP_GLOBAL_LOBBY)), find=[1, 1], handle=[21, 22])
    yield case(run(op(OP_GLOBAL_LOBBY)), find=[1], names=["x", "y", "z", "w", "v"])


def global_lobby_random(r):
    return random_case(r, lambda r: [op(OP_GLOBAL_LOBBY) for _ in range(r.randrange(1, 3))])


# SND_RegisterLocalLobbySounds, SND_FreeLocalLobbySounds and SND_FreeGlobalLobbySounds

def local_lobby_edges():
    yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "Island")), find=[1], handle=[31, 32])
    yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "Island")), find=[0], handle=[31, 32])
    yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "Island")), find=[6], handle=[31, 32])
    yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "Island")), find=[5], handle=[31, 32])
    yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "")), find=[2], handle=[31, 32])
    for wanted, on in FLAGS:
        yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "A")), wanted=wanted, on=on, find=[1], handle=[41, 42])
    for sound in SOUNDS[:8]:
        yield case(run(op(OP_LOCAL_LOBBY, 0, 0, "A")), find=[1], sound=sound, handle=[41, 42])
    # the table grows from 0 to 5 rows: the old rows are copied, the old block freed; one island fails in the middle
    names = ["Fantasy", "Jungle", "Hallow", "Space", "Island5"]
    yield case([op(OP_LOCAL_LOBBY, 0, 0, n) for n in names], find=[1] * 5, handle=list(range(101, 111)))
    yield case([op(OP_LOCAL_LOBBY, 0, 0, n) for n in names], find=[1, 1, 0, 1, 6], handle=list(range(101, 111)), sound=[1, 1, 0, 1, 1, 1])
    yield case([op(OP_LOCAL_LOBBY, 0, 0, n) for n in names] + [op(OP_FREE_LOCAL_LOBBY)], find=[1] * 5, handle=list(range(101, 111)))
    yield case([op(OP_LOCAL_LOBBY, 0, 0, n) for n in names] + [op(OP_FREE_LOCAL_LOBBY), op(OP_LOCAL_LOBBY, 0, 0, "again")],
               find=[1] * 6, handle=list(range(101, 115)))
    yield case([op(OP_LOCAL_LOBBY, 0, 0, "n" * 70)], find=[1], handle=[1, 2])
    yield case([op(OP_LOCAL_LOBBY, 0, 0, "a"), op(OP_LOCAL_LOBBY, 0, 0, "b")], find=[1, 1], handle=[0, 0, MASK, 0])


def local_lobby_random(r):
    def ops(r):
        out = []
        for _ in range(r.randrange(1, 9)):
            out.append(op(r.choice([OP_LOCAL_LOBBY] * 4 + [OP_FREE_LOCAL_LOBBY]), 0, 0, random_text(r)))
        return out
    return random_case(r, ops)


def free_local_lobby_edges():
    for wanted, on in FLAGS:
        yield case(run(op(OP_FREE_LOCAL_LOBBY)), wanted=wanted, on=on)
    for n in range(1, 6):
        for wanted, on in ((1, 1), (0, 0), (1, 0)):
            yield case([op(OP_LOCAL_LOBBY, 0, 0, "I%d" % i) for i in range(n)] + [op(OP_FREE_LOCAL_LOBBY)], wanted=wanted, on=on,
                       find=[1] * n, handle=list(range(51, 51 + 2 * n)))
    yield case(run(op(OP_FREE_LOCAL_LOBBY), op(OP_FREE_LOCAL_LOBBY)))
    yield case([op(OP_LOCAL_LOBBY, 0, 0, "I"), op(OP_FREE_LOCAL_LOBBY), op(OP_FREE_LOCAL_LOBBY)], find=[1], handle=[0, MASK])
    yield case([op(OP_LOCAL_LOBBY, 0, 0, "I"), op(OP_LOCAL_LOBBY, 0, 0, "J"), op(OP_FREE_LOCAL_LOBBY)], find=[1, 1],
               handle=[1, 2, 3, 4], sound=[1, 0, 1, 1])


def free_local_lobby_random(r):
    def ops(r):
        out = [op(OP_LOCAL_LOBBY, 0, 0, random_text(r)) for _ in range(r.randrange(0, 6))]
        out.append(op(OP_FREE_LOCAL_LOBBY))
        if r.random() < 0.3:
            out.append(op(OP_LOCAL_LOBBY, 0, 0, random_text(r)))
        return out
    return random_case(r, ops)


def free_global_lobby_edges():
    for wanted, on in FLAGS:
        for bank in (0, 1, 5, MASK):
            yield case(run(op(OP_FREE_GLOBAL_LOBBY)), wanted=wanted, on=on, words={14: bank})
    yield case(run(op(OP_GLOBAL_LOBBY), op(OP_FREE_GLOBAL_LOBBY)), find=[1], handle=[77])
    yield case(run(op(OP_GLOBAL_LOBBY), op(OP_FREE_GLOBAL_LOBBY), op(OP_FREE_GLOBAL_LOBBY)), find=[1], handle=[77])
    yield case(run(op(OP_FREE_GLOBAL_LOBBY)), wanted=0, on=0, audio=0, words={14: 3})


def free_global_lobby_random(r):
    def ops(r):
        return [op(r.choice([OP_FREE_GLOBAL_LOBBY, OP_GLOBAL_LOBBY])) for _ in range(r.randrange(1, 5))]
    return random_case(r, ops)


# SND_RegisterLocalParkSounds

def local_park_edges():
    for find in ([1, 1, 1], [0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], [1, 0, 1], [0, 1, 1], [1, 1, 0], [2, 3, 4], [5, 5, 5], [6, 6, 6],
                 [1, 6, 1], [4, 1, 1]):
        yield case(run(op(OP_LOCAL_PARK, 0, 0, "fantasy")), find=find, handle=[61, 62, 63, 64])
    for wanted, on in FLAGS:
        yield case(run(op(OP_LOCAL_PARK, 0, 0, "jungle")), wanted=wanted, on=on, find=[1, 1, 1], handle=[MASK, 0, 5, 6])
    for sound in SOUNDS[:10]:
        yield case(run(op(OP_LOCAL_PARK, 0, 0, "hallow")), find=[1, 1, 1], sound=sound, handle=[61, 62, 63, 64])
    yield case(run(op(OP_LOCAL_PARK, 0, 0, "")), find=[1, 1, 1], handle=[1, 2, 3, 4])
    yield case(run(op(OP_LOCAL_PARK, 0, 0, "t" * 70)), find=[1, 1, 1], handle=[1, 2, 3, 4])
    yield case(run(op(OP_LOCAL_PARK, 0, 0, "a"), op(OP_LOCAL_PARK, 0, 0, "b")), find=[1] * 6, handle=list(range(1, 9)))
    yield case(run(op(OP_LOCAL_PARK, 0, 0, "a")), find=[1, 1, 1], names=["g", "h", "p1", "p2", "p3"], handle=[1, 2, 3, 4])
    yield case(run(op(OP_LOCAL_PARK, 0, 0, "a")), find=[1, 1, 1], words={10: 9, 11: 9, 12: 9, 13: 9}, handle=[1, 2, 3, 4])


def local_park_random(r):
    return random_case(r, lambda r: [op(OP_LOCAL_PARK, 0, 0, random_text(r)) for _ in range(r.randrange(1, 3))])


# SND_FreeGlobalSounds and SND_FreeLocalParkSounds

def subsets(words, n):
    """Every subset of the n bank words nonzero (the others zero)."""
    for mask in range(1 << n):
        yield {words[i]: (i + 1) * 0x11 for i in range(n) if mask >> i & 1}


def free_global_edges():
    for words in subsets([4, 5, 6, 7, 8, 9], 6):
        yield case(run(op(OP_FREE_GLOBAL)), words=words)
    for wanted, on in FLAGS:
        yield case(run(op(OP_FREE_GLOBAL)), wanted=wanted, on=on, words={i: i for i in range(4, 10)})
    yield case(run(op(OP_FREE_GLOBAL)), words={i: MASK for i in range(4, 10)})
    yield case(run(op(OP_FREE_GLOBAL)), words={i: 7 for i in range(4, 15)})
    yield case(run(op(OP_FREE_GLOBAL), op(OP_FREE_GLOBAL)), words={i: i for i in range(4, 10)})
    yield case(run(op(OP_GLOBAL), op(OP_FREE_GLOBAL), op(OP_GLOBAL)), find=[1, 1, 1, 1], handle=[11, 12, 13, 14, 15, 16])


def free_global_random(r):
    return random_case(r, lambda r: [op(r.choice([OP_FREE_GLOBAL, OP_GLOBAL])) for _ in range(r.randrange(1, 4))])


def free_local_park_edges():
    for words in subsets([10, 11, 12, 13], 4):
        yield case(run(op(OP_FREE_LOCAL_PARK)), words=words)
    for wanted, on in FLAGS:
        yield case(run(op(OP_FREE_LOCAL_PARK)), wanted=wanted, on=on, words={10: 3, 11: 4, 12: 5, 13: 6})
    yield case(run(op(OP_FREE_LOCAL_PARK)), words={i: MASK for i in range(10, 14)})
    yield case(run(op(OP_FREE_LOCAL_PARK)), words={i: 7 for i in range(4, 15)})
    yield case(run(op(OP_FREE_LOCAL_PARK), op(OP_FREE_LOCAL_PARK)), words={10: 3, 11: 4, 12: 5, 13: 6})
    yield case(run(op(OP_LOCAL_PARK, 0, 0, "fantasy"), op(OP_FREE_LOCAL_PARK), op(OP_LOCAL_PARK, 0, 0, "jungle")), find=[1] * 6,
               handle=list(range(61, 70)))


def free_local_park_random(r):
    return random_case(r, lambda r: [op(r.choice([OP_FREE_LOCAL_PARK, OP_LOCAL_PARK]), 0, 0, random_text(r)) for _ in range(r.randrange(1, 4))])


# every function in one script

ALL_OPS = [OP_MALLOC, OP_FREE, OP_REG_PLAIN, OP_REG_RES, OP_FREE_CAT, OP_GLOBAL, OP_GLOBAL_LOBBY, OP_LOCAL_LOBBY, OP_FREE_GLOBAL_LOBBY,
           OP_FREE_LOCAL_LOBBY, OP_LOCAL_PARK, OP_FREE_GLOBAL, OP_FREE_LOCAL_PARK]


def mixed_random(r):
    def ops(r):
        out = []
        slots = 0
        for _ in range(r.randrange(2, 12)):
            kind = r.choice(ALL_OPS)
            if kind == OP_MALLOC:
                out.append(op(kind, r.randrange(1, 64), slots % 16))
                slots += 1
            elif kind == OP_FREE:
                out.append(op(kind, r.choice([-1, 99] + list(range(max(slots, 1))))))
            elif kind == OP_REG_PLAIN:
                out.append(op(kind, 0, 0, random_text(r), random_text(r)))
            elif kind == OP_REG_RES:
                out.append(op(kind, r.randrange(4), 0, random_text(r)))
            elif kind == OP_FREE_CAT:
                out.append(op(kind, r.choice([0, 1, MASK, r.getrandbits(32)])))
            elif kind in (OP_LOCAL_LOBBY, OP_LOCAL_PARK):
                out.append(op(kind, 0, 0, random_text(r)))
            else:
                out.append(op(kind))
        return out
    return random_case(r, ops)


def mixed_edges():
    yield case(run(op(OP_GLOBAL), op(OP_GLOBAL_LOBBY), op(OP_LOCAL_LOBBY, 0, 0, "I"), op(OP_LOCAL_PARK, 0, 0, "fantasy"),
                   op(OP_FREE_LOCAL_PARK), op(OP_FREE_LOCAL_LOBBY), op(OP_FREE_GLOBAL_LOBBY), op(OP_FREE_GLOBAL)),
               find=[1] * 12, handle=list(range(201, 230)))


TESTS = [
    FnTest("SOUND_Malloc 0x100dc5e4", run_ppc, host_case, edge=malloc_edges, rnd=malloc_random),
    FnTest("SOUND_Free 0x100dc64c", run_ppc, host_case, edge=free_edges, rnd=free_random),
    FnTest("SOUND_RegisterCatagory(char*, char*) 0x100dc6ac", run_ppc, host_case, edge=plain_edges, rnd=plain_random),
    FnTest("SOUND_RegisterCatagory(char*, const CTPSoundResource&) 0x100dc74c", run_ppc, host_case, edge=res_edges, rnd=res_random),
    FnTest("SOUND_FreeCatagory 0x100dc89c", run_ppc, host_case, edge=freecat_edges, rnd=freecat_random),
    FnTest("SND_RegisterGlobalSounds 0x100df310", run_ppc, host_case, edge=global_edges, rnd=global_random),
    FnTest("SND_RegisterGlobalLobbySounds 0x100def68", run_ppc, host_case, edge=global_lobby_edges, rnd=global_lobby_random),
    FnTest("SND_RegisterLocalLobbySounds 0x100df02c", run_ppc, host_case, edge=local_lobby_edges, rnd=local_lobby_random),
    FnTest("SND_FreeLocalLobbySounds 0x100df234", run_ppc, host_case, edge=free_local_lobby_edges, rnd=free_local_lobby_random),
    FnTest("SND_FreeGlobalLobbySounds 0x100df1bc", run_ppc, host_case, edge=free_global_lobby_edges, rnd=free_global_lobby_random),
    FnTest("SND_RegisterLocalParkSounds 0x100df4d4", run_ppc, host_case, edge=local_park_edges, rnd=local_park_random),
    FnTest("SND_FreeGlobalSounds 0x100df67c", run_ppc, host_case, edge=free_global_edges, rnd=free_global_random),
    FnTest("SND_FreeLocalParkSounds 0x100df778", run_ppc, host_case, edge=free_local_park_edges, rnd=free_local_park_random),
    FnTest("all thirteen in mixed scripts", run_ppc, host_case, edge=mixed_edges, rnd=mixed_random),
]
