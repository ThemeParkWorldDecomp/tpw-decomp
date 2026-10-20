"""Differential tests for the save file header (unit w114), against libtpw_unit_SaveHeader.dylib (tests/unit_SaveHeader.cpp):

  src/SaveLoadHeader.cpp  CSaveLoad::LoadHeader 0x10159e50, CSaveLoadHeader::Serialise 0x1015b8b0 and the static initialisation of
                          SaveLoad.cpp (__sinit_SaveLoad_cpp 0x1015b484: the seven legal texts and four words of gSaveLoad)

LoadHeader and Serialise run as the original on the PowerPC side. What they call is a fake that records the same event on both sides:
LbFile_Read (a fake file read in order; the queues "LbFile_Read" and "LbFile_Read:xfer" change one read's result or count), GAME_ASSERTMSG,
SamsUtilities::_USwapBlock16 (on the PowerPC side it swaps the bytes of each unit in memory as the platform's does, so the text the compare
sees is the same units as the host's), the Bullfrog string compare TbStringBase<wchar_t>::operator!= (the table entry it was asked about
and a hash of the 640 units, answers from the queue "operator!="), CWorld::DoIHaveTheseRides (the receiver's label, previews, the ride
record's hash and its first 0x20 bytes; the queue "DoIHaveTheseRides" answers, 1 if empty) and the six LOADVAR/SAVEVAR instances Serialise
calls (the value's offset from the first value of the case, the name, noLog; call number `fail` of the case answers 1, counting from 0).
memcpy is the stub of tpwtest. Compared after each call: the answer, the error word of gSaveLoad (it starts as a random word), the 32 bytes
of the park's required rides (they start random), the header the caller gave (it must stay untouched: the fakes write nothing), how many
bytes the file handed out, and the log.

The static initialisation runs on the PowerPC side with the string constructor and __register_global_object recorded; its texts, read from
the original's image, are compared unit for unit with the host's table, and the four words of gSaveLoad it sets with the host's.

The language index is not range checked by the original; the cases use it up to 255 and the compare is a fake, so no table entry beyond the
seventh is read. The text always has a 0 among its 640 units (the original does not terminate it). A flag word other than 0 and 1 is only
used with a header (the original calls Serialise on NULL otherwise).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_SaveHeader"

MASK = 0xFFFFFFFF
LOAD_HEADER, SERIALISE, SINIT = 0x10159E50, 0x1015B8B0, 0x1015B484
GAME_ASSERT, DO_I_HAVE = 0x1000DDE8, 0x10140FAC
VARS = {0x10111844: "LOADVAR<wchar_t>", 0x10111A98: "SAVEVAR<wchar_t>", 0x100F5074: "LOADVAR<unsigned short>",
        0x100F5530: "SAVEVAR<unsigned short>", 0x100F16B4: "LOADVAR<float>", 0x100F18D8: "SAVEVAR<float>"}
GSAVELOAD = 0x1037FEFC
SAVELOAD_WORDS = [GSAVELOAD, GSAVELOAD + 4, 0x1037FF10, 0x1037FF14]   # mF00, mError, mF14, mF18
GAME_WORLD_VARIABLE = 0x1037EBFC
PARK_RIDES = 0x1034CBB8
LEGAL_TABLE = 0x1037FE1C
FILE_HANDLE = 0x6E100000      # the PowerPC side's file (never dereferenced)
LAB_HEADER, LAB_FILE, LAB_WORLD = 7, 8, 9
HEADER_SIZE = 0x3D4
CANARY = b"\xa5" * HEADER_SIZE
KEY_BE = bytes([0x01, 0x22, 0x19, 0x85])     # the Mac's file holds the key in the Mac's byte order
LEGAL_ADDRESSES = [0x10249F40 + off for off in (0, 0x33A, 0x776, 0xAD0, 0xE00, 0x11F6, 0x15AA)]
VP, I32, U32 = C.c_void_p, C.c_int32, C.c_uint32

# the file's segments: the language byte, the text, the ride record, the key, the flag word
SEGMENTS = [1, 0x500, 0x100, 4, 4]


def s32(v):
    return T.s32(v & MASK)


def fnv(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h & MASK


def units_bytes(units):
    return b"".join(struct.pack("<H", u) for u in units)


# ======================================================================= PowerPC side fakes


class PsState:
    pass


def lb_read(e):
    # r3 the result record, r4 the file, r5 the buffer, r6 the length, r7 the count
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length, e.ps.pos))
    data = e.ps.file
    available = max(len(data) - e.ps.pos, 0)
    count = min(available, length)
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = L.ret("LbFile_Read")
    if count == length and result >= 0:
        e.write(buf, data[e.ps.pos:e.ps.pos + length].ljust(length, b"\0"))
        e.ps.pos += length
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def swap16(e):
    buf, count = e.r(3), e.r(4)
    e.cl.rec("_USwapBlock16", (T.s32(count),))
    words = e.read(buf, 2 * count)
    e.write(buf, b"".join(words[i + 1:i + 2] + words[i:i + 1] for i in range(0, len(words), 2)))


def ne_stub(e):
    L = e.cl
    this, text = e.r(3), e.r(4)
    units = struct.unpack(">%dH" % 0x280, e.read(text, 0x500))
    L.rec("operator!=", ((this - LEGAL_TABLE) // 0x14, fnv(units_bytes(units))), (CL.wide_text(e, text),))
    e.set_r(3, 1 if L.ret("operator!=") else 0)


def do_i_have(e):
    L = e.cl
    world, required, previews = e.r(3), e.r(4), e.r(5)
    data = e.read(required, 0x100)
    L.rec("DoIHaveTheseRides", (L.lab(world), T.s32(previews), fnv(data)), (data[:0x20].hex(),))
    e.set_r(3, T.u32(L.ret("DoIHaveTheseRides", 1)))


def var_stub(name):
    def stub(e):
        L = e.cl
        file, value, text, no_log = e.r(3), e.r(4), e.r(5), e.r(6)
        if e.ps.base is None:
            e.ps.base = value
        L.rec(name, (L.lab(file), value - e.ps.base, T.s32(no_log), L.lab(e.ps.base)), (e.cstr(text)[:159],))
        n = e.ps.calls
        e.ps.calls += 1
        e.set_r(3, 1 if n == e.ps.fail else 0)
    return stub


def build_stubs():
    S = CL.Stubs()
    S.real.update({LOAD_HEADER, SERIALISE})
    S.internal(GAME_ASSERT, lambda e: e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
    S.internal(DO_I_HAVE, do_i_have)
    for addr, name in VARS.items():
        S.internal(addr, var_stub(name))
    S.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    S.imp("_USwapBlock16__13SamsUtilitiesFPUsl", swap16)
    S.imp("__ne__15TbStringBase<w>CFPCw", ne_stub)
    return S


def build_sinit_stubs():
    S = CL.Stubs()
    S.real.add(SINIT)

    def construct(e):
        this, text = e.r(3), e.r(4)
        units = []
        while len(units) < 2000:
            unit = struct.unpack(">H", e.read(text + 2 * len(units), 2))[0]
            if unit == 0:
                break
            units.append(unit)
        e.ps.constructed.append(((this - LEGAL_TABLE) // 0x14, (this - LEGAL_TABLE) % 0x14, text, units))
        e.set_r(3, this)

    S.imp("__ct__26TbDynamicStringTemplate<w>FPCw", construct)
    S.imp("__register_global_object", lambda e: e.ps.registered.append((e.r(3) - LEGAL_TABLE) // 0x14))
    return S


STUBS = None
SINIT_STUBS = None


def setup(runner):
    global STUBS, SINIT_STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()
    SINIT_STUBS = build_sinit_stubs()


def lib_setup(lib):
    lib.tpw_sh_addr.restype = VP
    lib.tpw_sh_addr.argtypes = [C.c_char_p]
    lib.tpw_sh_begin.argtypes = [C.c_char_p, I32, I32]
    lib.tpw_sh_position.restype = I32
    lib.tpw_sh_load_header.restype = I32
    lib.tpw_sh_load_header.argtypes = [I32, VP, I32, VP, VP]
    lib.tpw_sh_serialise.restype = I32
    lib.tpw_sh_serialise.argtypes = [VP, I32, I32, U32, I32]
    lib.tpw_sh_legal.restype = I32
    lib.tpw_sh_legal.argtypes = [I32, VP, I32]
    lib.tpw_sh_saveload.argtypes = [VP]


# ======================================================================= LoadHeader

SENTINELS = [0, 1, 0x7FFFFFFF, -1, 0x12345678, -0x80000000, 5]
DEFAULT_TEXT = [ord(ch) for ch in "THE SAVE GAME DATA CREATED AND SAVED IN THIS PRODUCT"] + [0] * 3


def text_units(units):
    """640 units: the given ones then zeros."""
    return (list(units) + [0] * 0x280)[:0x280]


def lh_case(type=2, header=True, lang=0, text=None, rides=None, key=KEY_BE, flag=(1, 0, 0, 0), q=None, error=0x5A5A5A5A,
            init_rides=None, truncate=None, fail=-1):
    return dict(fail=fail, type=type, header=header, lang=lang, text=text_units(DEFAULT_TEXT if text is None else text),
                rides=bytes(range(0x100)) if rides is None else bytes(rides), key=bytes(key), flag=bytes(flag), q=q or {},
                error=error, init_rides=bytes(range(100, 132)) if init_rides is None else bytes(init_rides), truncate=truncate)


def lh_file(c):
    data = bytes([c["lang"]]) + units_bytes(c["text"]) + c["rides"] + c["key"] + c["flag"] + bytes(32)
    return data if c["truncate"] is None else data[:c["truncate"]]


def lh_flag_value(c):
    return int.from_bytes(c["flag"], "little")


def lh_runnable(c):
    """The original calls Serialise on NULL (and crashes) for a flag other than 0 and 1 with no header, once it gets that far."""
    return c["header"] or lh_flag_value(c) in (0, 1)


def ppc_load_header(e, c):
    saved = STUBS.activate(e)
    try:
        header = e.alloc(HEADER_SIZE, CANARY, alignment=4) if c["header"] else 0
        world = e.alloc(16)
        fixed = {FILE_HANDLE: LAB_FILE, world: LAB_WORLD}
        if header:
            fixed[header] = LAB_HEADER
        e.cl = CL.PpcLog(fixed, c["q"])
        e.ps = PsState()
        e.ps.file, e.ps.pos, e.ps.base, e.ps.fail, e.ps.calls = lh_file(c), 0, None, c["fail"], 0
        e.write(GAME_WORLD_VARIABLE, struct.pack(">I", world))
        e.write(GSAVELOAD + 4, struct.pack(">I", c["error"] & MASK))
        e.write(PARK_RIDES, c["init_rides"])
        r3 = T.call_ppc(e, LOAD_HEADER, [GSAVELOAD, FILE_HANDLE, T.u32(c["type"]), header])[0]
        out = dict(result=T.s32(r3), error=T.s32(e.u32(GSAVELOAD + 4)), rides=e.read(PARK_RIDES, 0x20),
                   header=e.read(header, HEADER_SIZE) if header else b"", position=e.ps.pos, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    return out


def host_load_header(lib, c):
    lib_setup(lib)
    data = lh_file(c)
    lib.tpw_sh_begin(data, len(data), c["fail"])
    header = C.create_string_buffer(CANARY, HEADER_SIZE) if c["header"] else None
    fixed = {LAB_FILE: lib.tpw_sh_addr(b"file"), LAB_WORLD: lib.tpw_sh_addr(b"world")}
    if header:
        fixed[LAB_HEADER] = C.addressof(header)
    CL.host_begin(lib, fixed, c["q"])
    rides = C.create_string_buffer(c["init_rides"], 0x20)
    error = I32()
    result = lib.tpw_sh_load_header(c["type"], C.addressof(header) if header else None, c["error"], rides, C.byref(error))
    log = CL.host_end(lib)
    return dict(result=result, error=error.value, rides=rides.raw[:0x20], header=header.raw[:HEADER_SIZE] if header else b"",
                position=lib.tpw_sh_position(), log=log)


def read_fault(j, kind, length):
    """The queues that make the j-th read (1 to 5) fail: kind 'neg' a negative result, 'short' a count one short, 'zero' a count of 0,
    'long' a count one over, 'huge' a count of 0xffffffff, 'minus2' a result of -2."""
    ok = [0] * (j - 1)
    keep = [-1] * (j - 1)
    if kind == "neg":
        return {"LbFile_Read": ok + [-1]}
    if kind == "minus2":
        return {"LbFile_Read": ok + [-2], "LbFile_Read:xfer": keep + [length]}
    count = {"short": length - 1, "zero": 0, "long": length + 1, "huge": 0xFFFFFFFF}[kind]
    return {"LbFile_Read:xfer": keep + [count]}


def lh_edges():
    cases = []
    # the plain path: every type, with and without a header, flag 0 and 1
    for type in (0, 1, 2, 3, -1, 0x7FFFFFFF, -0x80000000):
        for header in (True, False):
            for flag in ((0, 0, 0, 0), (1, 0, 0, 0)):
                cases.append(lh_case(type=type, header=header, flag=flag))
    # the language byte
    for lang in (0, 1, 2, 3, 4, 5, 6, 7, 0x80, 0xFF, 0x7F):
        cases.append(lh_case(lang=lang))
        cases.append(lh_case(lang=lang, q={"operator!=": [1]}))
    # a text that differs, with the text bytes in different byte orders
    wide = [0x0141, 0x4101, 0x00FF, 0xFF00, 0x1234, 0x3412, 0x8000, 0x0001, 0xFFFF, 0x0100]
    cases.append(lh_case(text=wide))
    cases.append(lh_case(text=wide, q={"operator!=": [1]}))
    cases.append(lh_case(text=[0]))
    cases.append(lh_case(text=[0x41] * 0x27F))
    cases.append(lh_case(text=[0x41] * 0x27F + [0], q={"operator!=": [1]}))
    cases.append(lh_case(text=[0x1000 + i for i in range(0x27F)]))
    # the ride check: no ride, every type; the record (all 0x100 bytes seen, 0x20 kept)
    for type in (0, 1, 2, 3, -1):
        cases.append(lh_case(type=type, q={"DoIHaveTheseRides": [0]}))
        cases.append(lh_case(type=type, q={"DoIHaveTheseRides": [5]}))
        cases.append(lh_case(type=type, q={"DoIHaveTheseRides": [-1]}))
    cases.append(lh_case(rides=bytes(0x100)))
    cases.append(lh_case(rides=bytes([0xFF] * 0x100)))
    cases.append(lh_case(rides=bytes([0] * 0x20 + [0xFF] * 0xE0)))
    cases.append(lh_case(rides=bytes([0xFF] * 0x20 + [0] * 0xE0)))
    cases.append(lh_case(rides=bytes([1] * 0x1F + [2] + [3] * 0xE0)))
    cases.append(lh_case(rides=bytes([0xAA] * 0x1F + [0x55] * 0xE1)))
    cases.append(lh_case(rides=bytes([1] * 0xE0 + [7] * 0x20)))
    # the key: the Mac's, then each bit of it flipped, the PC's order, 0, ffffffff
    cases.append(lh_case(key=KEY_BE))
    for bit in range(32):
        word = int.from_bytes(KEY_BE, "big") ^ (1 << bit)
        cases.append(lh_case(key=word.to_bytes(4, "big")))
    cases.append(lh_case(key=KEY_BE[::-1]))
    cases.append(lh_case(key=bytes(4)))
    cases.append(lh_case(key=bytes([0xFF] * 4)))
    cases.append(lh_case(key=bytes([0x01, 0x22, 0x19, 0x85]), error=-7))
    # the flag word, against a header and none, and what Serialise answers
    flags = [(0, 0, 0, 0), (1, 0, 0, 0), (0, 0, 0, 1), (2, 0, 0, 0), (0xFF, 0xFF, 0xFF, 0xFF), (0, 0, 0, 0x80), (0, 1, 0, 0), (0, 0, 1, 0),
             (1, 1, 0, 0), (3, 0, 0, 0), (0x80, 0, 0, 0), (0xFF, 0, 0, 0)]
    for flag in flags:
        for header in (True, False):
            for fail in (-1, 0, 1, 281):
                c = lh_case(header=header, flag=flag, fail=fail)
                if lh_runnable(c):
                    cases.append(c)
    # a failed read at each of the five reads
    for j in range(1, 6):
        for kind in ("neg", "minus2", "short", "zero", "long", "huge"):
            length = SEGMENTS[j - 1]
            cases.append(lh_case(q=read_fault(j, kind, length), error=0x77))
    # a file that ends early: at each boundary and one byte either side
    bounds = [sum(SEGMENTS[:k]) for k in range(1, 6)]
    for b in bounds:
        for cut in (b - 1, b, b + 1):
            cases.append(lh_case(truncate=cut))
    cases.append(lh_case(truncate=0))
    # a read that fails after the key (not at the end): the failing key read leaves the rides copied
    cases.append(lh_case(q=read_fault(4, "neg", 4)))
    cases.append(lh_case(q=read_fault(5, "short", 4), header=False))
    # the sentinel error word survives a success and every failure but 6 and 8
    for error in SENTINELS:
        cases.append(lh_case(error=error))
        cases.append(lh_case(error=error, q={"DoIHaveTheseRides": [0]}))
        cases.append(lh_case(error=error, key=bytes(4)))
        cases.append(lh_case(error=error, q={"operator!=": [1]}))
    return cases


def lh_rnd(r):
    for _ in range(100):
        header = r.random() < 0.6
        flag = r.choice([(1, 0, 0, 0), (0, 0, 0, 0), (1, 0, 0, 0), (r.randrange(256), r.randrange(256), r.randrange(256), r.randrange(256))])
        q = {}
        if r.random() < 0.3:
            q["operator!="] = [r.choice([0, 1, 1, 2])]
        if r.random() < 0.3:
            q["DoIHaveTheseRides"] = [r.choice([0, 1, 7, -1])]
        fail = r.randrange(282) if r.random() < 0.3 else -1
        if r.random() < 0.3:
            j = r.randrange(1, 6)
            q.update(read_fault(j, r.choice(["neg", "minus2", "short", "zero", "long", "huge"]), SEGMENTS[j - 1]))
        if r.random() < 0.25:
            key = bytes(r.getrandbits(8) for _ in range(4))
        elif r.random() < 0.4:
            key = (int.from_bytes(KEY_BE, "big") ^ (1 << r.randrange(32))).to_bytes(4, "big")
        else:
            key = KEY_BE
        text = [r.choice([r.getrandbits(16), r.randrange(0x20, 0x7F)]) for _ in range(r.randrange(0, 0x280))] + [0]
        c = lh_case(type=r.choice([0, 1, 2, 3, -1, r.getrandbits(32) - 0x80000000]), header=header, lang=r.choice([0, 1, 6, 7, 0x80, 0xFF, r.getrandbits(8)]),
                    text=text, rides=bytes(r.getrandbits(8) for _ in range(0x100)), key=key, flag=flag, q=q, fail=fail,
                    error=r.getrandbits(32) - 0x80000000, init_rides=bytes(r.getrandbits(8) for _ in range(32)),
                    truncate=r.choice([None, None, None, r.randrange(0x610)]))
        if lh_runnable(c):
            return c
    return lh_case()


# ======================================================================= Serialise

SAVES = [0, 1, -1, 2, -0x80000000, 0x7FFFFFFF]
VERSIONS = [-0x80000000, -1, 0, 1, 2, 3, 0x7FFFFFFF]


def se_case(save=0, version=1, unused=0, fail=-1):
    return dict(save=save, version=version, unused=unused, fail=fail, q={})


def ppc_serialise(e, c):
    saved = STUBS.activate(e)
    try:
        header = e.alloc(HEADER_SIZE, CANARY, alignment=4)
        e.cl = CL.PpcLog({FILE_HANDLE: LAB_FILE, header: LAB_HEADER}, c["q"])
        e.ps = PsState()
        e.ps.base, e.ps.fail, e.ps.calls = None, c["fail"], 0
        r3 = T.call_ppc(e, SERIALISE, [header, FILE_HANDLE, T.u32(c["save"]), T.u32(c["version"]), c["unused"]])[0]
        out = dict(result=T.s32(r3), header=e.read(header, HEADER_SIZE), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    return out


def host_serialise(lib, c):
    lib_setup(lib)
    header = C.create_string_buffer(CANARY, HEADER_SIZE)
    CL.host_begin(lib, {LAB_FILE: lib.tpw_sh_addr(b"file"), LAB_HEADER: C.addressof(header)}, c["q"])
    result = lib.tpw_sh_serialise(C.addressof(header), c["save"], c["version"], c["unused"], c["fail"])
    log = CL.host_end(lib)
    return dict(result=result, header=header.raw[:HEADER_SIZE], log=log)


def se_edges():
    cases = []
    for save in SAVES:
        for version in VERSIONS:
            cases.append(se_case(save, version))
    # the call that fails, for each of the 282, both directions (version 0 and 1)
    for save in (0, 1):
        for version in (1, 0):
            for k in range(282):
                cases.append(se_case(save, version, fail=k))
    for save in (-1, 5):
        cases.append(se_case(save, 1, 0xFFFFFFFF, fail=16))
    cases.append(se_case(0, 2, 7, fail=0))
    cases.append(se_case(1, -1, 7, fail=0))
    return cases


def se_rnd(r):
    return se_case(r.choice(SAVES + [r.getrandbits(32) - 0x80000000]), r.choice(VERSIONS + [r.getrandbits(32) - 0x80000000]),
                   r.getrandbits(32), fail=r.choice([-1, r.randrange(300), r.randrange(300)]))


# ======================================================================= the static initialisation


def ppc_sinit(e, c):
    saved = SINIT_STUBS.activate(e)
    try:
        e.ps = PsState()
        e.ps.constructed, e.ps.registered = [], []
        r = random.Random(c["seed"])
        for address in SAVELOAD_WORDS:
            e.write(address, struct.pack(">I", r.getrandbits(32)))
        T.call_ppc(e, SINIT, [])
        out = dict(words=[e.u32(address) for address in SAVELOAD_WORDS], registered=list(e.ps.registered),
                   order=[k for k, _, _, _ in e.ps.constructed], sources=[text for _, _, text, _ in e.ps.constructed],
                   aligned=[rest for _, rest, _, _ in e.ps.constructed], texts=[units for _, _, _, units in e.ps.constructed])
    finally:
        CL.Stubs.deactivate(e, saved)
    return out


def host_sinit(lib, c):
    lib_setup(lib)
    words = (U32 * 4)()
    lib.tpw_sh_saveload(words)
    texts = []
    for k in range(7):
        buffer = (C.c_uint16 * 700)()
        n = lib.tpw_sh_legal(k, buffer, 700)
        texts.append(list(buffer[:n]))
    # the construction order and the image addresses are not observable on the host: the original's (the listing's) are given
    return dict(words=list(words), registered=list(range(7)), order=list(range(7)), sources=LEGAL_ADDRESSES, aligned=[0] * 7, texts=texts)


def sinit_cases(r=None):
    return dict(seed=1 if r is None else r.getrandbits(32))


# (the static initialisation is first: the other two leave their error word in gSaveLoad)
TESTS = [
    FnTest("__sinit_SaveLoad_cpp", ppc_sinit, host_sinit, edge=lambda: [sinit_cases()], rnd=sinit_cases),
    FnTest("CSaveLoad::LoadHeader", ppc_load_header, host_load_header, edge=lh_edges, rnd=lh_rnd),
    FnTest("CSaveLoadHeader::Serialise", ppc_serialise, host_serialise, edge=se_edges, rnd=se_rnd),
]
