"""Differential tests for the twenty basic serialisation instances (src/Serialise_basic.cpp and, since unit w114,
src/Serialise_float.cpp): LOADVAR and SAVEVAR of int, bool, unsigned int, unsigned short, char, wchar_t and float (unit w146 adds unsigned char,
short and the thing id struct's LOADVARSTRUCT and SAVEVARSTRUCT), against
libtpw_unit_Serialise.dylib (tests/unit_Serialise.cpp's fakes; see calllog.py).

A case is one call: the file's bytes to read (loads), the value's starting content, the name,
the noLog flag and the queues of the file fakes (result codes and transferred counts). Both
sides record the serialise log, the file calls (a write as hex) and every GAME_ASSERTMSG
with its condition and text. The case compares the answer, the value after the call (a
saver must leave it alone, a failing loader too), the bytes around it (a wrong width would
touch them) and the log. The PowerPC side's value is big-endian memory and the host's is
little-endian; both are compared as integers.
"""
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Serialise"

# (name, address, size in bytes, loads?)
INSTANCES = [
    ("LOADVAR<int>", 0x1000EF18, 4, True), ("LOADVAR<bool>", 0x1000F144, 1, True),
    ("LOADVAR<unsigned int>", 0x100F44A4, 4, True), ("LOADVAR<unsigned short>", 0x100F5074, 2, True),
    ("LOADVAR<char>", 0x1016D2C8, 1, True), ("LOADVAR<wchar_t>", 0x10111844, 2, True),
    ("SAVEVAR<int>", 0x1000F4B0, 4, False), ("SAVEVAR<bool>", 0x1000F70C, 1, False),
    ("SAVEVAR<unsigned int>", 0x100F45BC, 4, False), ("SAVEVAR<unsigned short>", 0x100F5530, 2, False),
    ("SAVEVAR<char>", 0x1016D3D4, 1, False), ("SAVEVAR<wchar_t>", 0x10111A98, 2, False),
    ("LOADVAR<float>", 0x100F16B4, 4, True), ("SAVEVAR<float>", 0x100F18D8, 4, False),
    # unit w146: unsigned char, short and the thing id struct (tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, a halfword swapped on the way)
    ("LOADVAR<unsigned char>", 0x100F17C8, 1, True), ("LOADVAR<short>", 0x100F518C, 2, True),
    ("SAVEVAR<unsigned char>", 0x100F1A04, 1, False), ("SAVEVAR<short>", 0x100F5658, 2, False),
    ("LOADVARSTRUCT<ThingId>", 0x1000EB4C, 2, True), ("SAVEVARSTRUCT<ThingId>", 0x1000E9D4, 2, False),
    # unit w160, in tpw_sv_call's order (cases 20 to 29): the Mac's long (4 bytes) and unsigned long, the object id and map id structs, TbTimeStamp (8 bytes)
    ("LOADVAR<long>", 0x1000EE04, 4, True), ("LOADVAR<unsigned long>", 0x1000F02C, 4, True),
    ("SAVEVAR<long>", 0x1000F384, 4, False), ("SAVEVAR<unsigned long>", 0x1000F5DC, 4, False),
    ("LOADVARSTRUCT<ObjectId>", 0x100F1300, 2, True), ("SAVEVARSTRUCT<ObjectId>", 0x100F1188, 2, False),
    ("LOADVARSTRUCT<MapId>", 0x100F53CC, 2, True), ("SAVEVARSTRUCT<MapId>", 0x100F58C0, 2, False),
    ("LOADVARSTRUCT<TbTimeStamp>", 0x100F1468, 8, True), ("SAVEVARSTRUCT<TbTimeStamp>", 0x100F1B1C, 8, False),
]
# unit w160: the Byteswap of the object id and the thing id (a halfword exchanged in place)
BYTESWAPS = [("tag_IDX_TYPE<ObjectId>::Byteswap", 0x100FA688), ("tag_IDX_TYPE<ThingId>::Byteswap", 0x10079A0C)]
GSAVELOAD = 0x1037FEFC
FILE_HANDLE = 0x6E100000      # the PowerPC side's file (never dereferenced)
LABEL_SAVELOAD, LABEL_FILE = 6, 8
CANARY = bytes([0xA5, 0x5A, 0xC3, 0x3C, 0x96, 0x69, 0xF0, 0x0F])
CANARY16 = CANARY + bytes(reversed(CANARY))   # the value buffer is 16 bytes (w160: an 8-byte value), the canary after the value
VP, I32 = C.c_void_p, C.c_int32

EDGE_VALUES = {
    # (the last six are float patterns for the float instances: a quiet NaN, a signalling NaN, the smallest denormal, both infinities and 1.0)
    4: [0, 1, 0xFFFFFFFF, 0x12345678, 0x80000000, 0x7FFFFFFF, 0xFF, 0xFF00, 0x00010203, 0xA1B2C3D4,
        0x7FC00000, 0x7FA00000, 0x00000001, 0x7F800000, 0xFF800000, 0x3F800000],
    8: [0, 1, 0xFFFFFFFFFFFFFFFF, 0x0123456789ABCDEF, 0x8000000000000000, 0x7FFFFFFFFFFFFFFF, 0xFF, 0xFF00000000000000,
        0x00000000FFFFFFFF, 0xFFFFFFFF00000000],
    2: [0x0000, 0x00FF, 0xFF00, 0x1234, 0xFFFF, 0x8000, 0x0001, 0x7FFF, 0xABCD],
    1: [0, 1, 2, 0x7F, 0x80, 0xFF],
}
# (the last two are longer than the log's 159-character cut, which both sides apply to the name)
NAMES = ["", "x", "mCount", "mAward[3]", "A name with spaces 123", "n" * 120, "L" * 159, "M" * 200]
NO_LOGS = [0, 1, -1, 2, 0x100, 0x80000000, -0x80000000]
RESULTS = [0, 1, 5, 0x7FFFFFFF, -1, -2, -0x80000000]


def le(value, size):
    return (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


def make(which, value, init, file, name="x", no_log=0, q=None):
    return dict(which=which, value=value, init=init, file=bytes(file), name=name, no_log=no_log, q=q or {})


# ======================================================================= PowerPC side


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(addr for _, addr, _, _ in INSTANCES)
    S.real.update(addr for _, addr in BYTESWAPS)
    S.internal(0x1000DDE8, lambda e: e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
    S.internal(0x1015B3DC, R("CSaveLoad::WriteSerialiseLog", "Lisi"))

    def lb_read(e):
        # r3 the result record, r4 the file, r5 the buffer, r6 the length, r7 the count
        L, buf, length = e.cl, e.r(5), e.r(6)
        L.rec("LbFile_Read", (L.lab(e.r(4)), length))
        data = e.ps.file
        count = min(len(data), length)
        forced = L.ret("LbFile_Read:xfer", -1)
        if forced >= 0:
            count = forced
        e.write(e.r(7), (count & 0xFFFFFFFF).to_bytes(4, "big"))
        result = L.ret("LbFile_Read")
        if count == length and result >= 0:
            e.write(buf, (data + bytes(length))[:length])
        e.write(e.r(3), (result & 0xFFFFFFFF).to_bytes(4, "big"))

    def lb_write(e):
        L, length = e.cl, e.r(6)
        L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
        forced = L.ret("LbFile_Write:xfer", -1)
        e.write(e.r(7), ((forced if forced >= 0 else length) & 0xFFFFFFFF).to_bytes(4, "big"))
        e.write(e.r(3), (L.ret("LbFile_Write") & 0xFFFFFFFF).to_bytes(4, "big"))

    S.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    S.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
    return S


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()


class PsState:
    pass


def observe(value_bytes, size, order):
    return int.from_bytes(value_bytes[:size], order), value_bytes[size:].hex()


# ======================================================================= the test of one instance


def fn_test(index, edge, rnd):
    name, addr, size, loads = INSTANCES[index]

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            e.cl = CL.PpcLog({GSAVELOAD: LABEL_SAVELOAD, FILE_HANDLE: LABEL_FILE}, c["q"])
            e.ps = PsState()
            e.ps.file = c["file"]
            buf = e.alloc(16, c["init"].to_bytes(size, "big") + CANARY16[size:], alignment=8)
            text = CL.new_text(e, c["name"])
            r3 = T.call_ppc(e, addr, [FILE_HANDLE, buf, text, T.u32(c["no_log"])])[0]
            value, rest = observe(e.read(buf, 16), size, "big")
            out = dict(result=T.s32(r3), value=value, rest=rest, log=e.cl.log)
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        return out

    def host(lib, c):
        lib.tpw_sv_addr.restype = VP
        lib.tpw_sv_addr.argtypes = [C.c_char_p]
        lib.tpw_sv_set_file.argtypes = [C.c_char_p, I32]
        lib.tpw_sv_call.restype = I32
        lib.tpw_sv_call.argtypes = [I32, VP, C.c_char_p, I32]
        lib.tpw_sv_set_file(c["file"], len(c["file"]))
        CL.host_begin(lib, {LABEL_SAVELOAD: lib.tpw_sv_addr(b"gSaveLoad"), LABEL_FILE: lib.tpw_sv_addr(b"file")}, c["q"])
        buf = C.create_string_buffer(le(c["init"], size) + CANARY16[size:], 16)
        result = lib.tpw_sv_call(index, C.addressof(buf), c["name"].encode("latin-1"), c["no_log"])
        log = CL.host_end(lib)
        value, rest = observe(buf.raw, size, "little")
        return dict(result=result, value=value, rest=rest, log=log)

    return FnTest(name, ppc, host, edge=edge, rnd=rnd)


# ======================================================================= case generators


def edge_cases(index):
    _, _, size, loads = INSTANCES[index]
    values = EDGE_VALUES[size]

    def edge():
        cases = []
        for k, value in enumerate(values):
            other = values[(k + 3) % len(values)]
            data = le(value, size) if loads else b""
            cases.append(make(index, value, other, data))
            cases.append(make(index, value, value, data, no_log=1))
            cases.append(make(index, value, other, data, name=NAMES[k % len(NAMES)], no_log=0))
        for no_log in NO_LOGS:
            cases.append(make(index, values[3 % len(values)], values[1], le(values[3 % len(values)], size) if loads else b"",
                              name="log%d" % no_log, no_log=no_log))
        for name in NAMES:
            cases.append(make(index, values[2], values[0], le(values[2], size) if loads else b"", name=name))
        data = le(values[3 % len(values)], size) if loads else b""
        for result in RESULTS:
            call = "LbFile_Read" if loads else "LbFile_Write"
            cases.append(make(index, values[3 % len(values)], values[2], data, q={call: [result]}))
        counts = {0, 1, size - 1, size + 1, size * 2, 0xFFFFFFFF, 0x100000000 - size}
        for count in sorted(counts):
            call = "LbFile_Read" if loads else "LbFile_Write"
            cases.append(make(index, values[2], values[1], data, q={call + ":xfer": [count]}))
            cases.append(make(index, values[2], values[1], data, q={call + ":xfer": [count], call: [-1]}))
        # both calls fail at once, short file, empty file, a long file
        cases.append(make(index, values[1], values[2], data, q={"LbFile_Read": [-1], "LbFile_Write": [-1],
                                                             "LbFile_Read:xfer": [0], "LbFile_Write:xfer": [0]}))
        if loads:
            cases.append(make(index, values[2], values[1], b""))
            cases.append(make(index, values[2], values[1], le(values[2], size)[:size - 1]))
            cases.append(make(index, values[2], values[1], le(values[2], size) + b"\x01\x02\x03\x04"))
            cases.append(make(index, values[2], values[1], b"", q={"LbFile_Read:xfer": [size]}))
        return cases
    return edge


def rnd_cases(index):
    _, _, size, loads = INSTANCES[index]
    values = EDGE_VALUES[size]
    call = "LbFile_Read" if loads else "LbFile_Write"

    def pick(r):
        return r.choice(values) if r.random() < 0.4 else r.getrandbits(8 * size)

    def rnd(r):
        value, init = pick(r), pick(r)
        data = le(value, size) if loads else b""
        if loads and r.random() < 0.1:
            data = data[:r.randint(0, size)]
        q = {}
        if r.random() < 0.2:
            q[call] = [r.choice(RESULTS)]
        if r.random() < 0.2:
            q[call + ":xfer"] = [r.choice([0, 1, size - 1, size, size + 1, 0xFFFFFFFF, r.randint(0, 9)])]
        name = r.choice(NAMES) if r.random() < 0.5 else "".join(r.choice("abcXYZ_[]019 ") for _ in range(r.randint(0, 30)))
        return make(index, value, init, data, name=name, no_log=r.choice(NO_LOGS + [0, 0, 1, 1]), q=q)
    return rnd


TESTS = [fn_test(i, edge_cases(i), rnd_cases(i)) for i in range(len(INSTANCES))]


# ======================================================================= the Byteswaps (unit w160)

BS_VALUES = [0, 1, 0x00FF, 0xFF00, 0x1234, 0xFFFF, 0x8000]
BS_CANARY = bytes([0xA5, 0x5A, 0xC3, 0x3C])


def byteswap_test(which):
    name, addr = BYTESWAPS[which]

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            e.cl = CL.PpcLog({}, {})
            # a 4-byte canary each side of the halfword
            buf = e.alloc(12, BS_CANARY + c["value"].to_bytes(2, "big") + BS_CANARY, alignment=4)
            T.call_ppc(e, addr, [buf + 4])
            raw = e.read(buf, 12)
            out = dict(before=raw[:4].hex(), value=int.from_bytes(raw[4:6], "big"), after=raw[6:10].hex())
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        return out

    def host(lib, c):
        lib.tpw_sv_byteswap.argtypes = [I32, VP]
        buf = C.create_string_buffer(BS_CANARY + le(c["value"], 2) + BS_CANARY, 12)
        lib.tpw_sv_byteswap(which, C.addressof(buf) + 4)
        raw = buf.raw
        return dict(before=raw[:4].hex(), value=int.from_bytes(raw[4:6], "little"), after=raw[6:10].hex())

    def edge():
        return [dict(value=v) for v in BS_VALUES]

    def rnd(r):
        return dict(value=r.getrandbits(16))

    return FnTest(name, ppc, host, edge=edge, rnd=rnd)


TESTS += [byteswap_test(i) for i in range(len(BYTESWAPS))]
