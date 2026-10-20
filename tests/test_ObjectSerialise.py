"""Object.cpp saved ride state (w229), against unit_ObjectSerialise.

The original CObject::Serialise, both ObjectState serializers and static initializer
run on PPC. All outside calls are recording fakes on both sides. One Side builder
randomizes all 0x448 object bytes plus guards and normalizes each primitive's endian
order. Every call failure keeps its deterministic destination mutation. Enum I/O is
real, including hidden TbError buffers, raw partial failure bytes, assertions and log.
The host runtime's static definitions are compared structurally with PPC construction
and registration; no host registration function is invented. HardwareRounding corrects
Unicorn's fctiwz positive saturation. GetTime always fills all outputs, even on error:
the original's uninitialized outputs after an untouched failed call are not tested.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_ObjectSerialise"
SER, LOAD, SAVE, INIT = 0x10106570, 0x10111958, 0x10111BBC, 0x10111E94
HANDLE = 0x6E100000
GLOBALS = [0x1034C384, 0x1034C380, 0x1034C360, 0x1034C374]
MASK = 0xFFFFFFFF
VARS = {
    "long": (0x1000EE04, 0x1000F384, 4),
    "ulong": (0x1000F02C, 0x1000F5DC, 4),
    "int": (0x1000EF18, 0x1000F4B0, 4),
    "uint": (0x100F44A4, 0x100F45BC, 4),
    "ushort": (0x100F5074, 0x100F5530, 2),
    "uchar": (0x100F17C8, 0x100F1A04, 1),
    "wide": (0x10111844, 0x10111A98, 2),
    "float": (0x100F16B4, 0x100F18D8, 4),
    "ObjectId": (0x100F1300, 0x100F1188, 2),
    "MapId": (0x100F53CC, 0x100F58C0, 2),
    "ThingId": (0x1000EB4C, 0x1000E9D4, 2),
}
HISTORY = [0x6C, 0xF4, 0x1A4, 0x22C, 0x2B4, 0x33C]
WORDS = [0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x3C, 0x40, 0x44, 0x48,
         0x50, 0x54, 0x5C, 0x60, 0x64, 0x17C, 0x180, 0x184, 0x188, 0x18C,
         0x190, 0x194, 0x198, 0x19C, 0x1A0]
HALVES = [0xC, 0xE, 0x2C, 0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x5A, 0x68]
SWAPS = ([(a, 4) for a in WORDS] + [(a, 2) for a in HALVES]
         + [(h + i * 4, 4) for h in HISTORY for i in range(33)]
         + [(a, 2) for a in range(0x3C4, 0x448, 2)])


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


class Rounding(HardwareRounding):
    def __enter__(self):
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=SER, end=SER + 4367))


STUBS = CL.Stubs()
STUBS.real.update((SER, LOAD, SAVE, INIT))


def result(e):
    s = e.os
    n = s.calls
    s.calls += 1
    return s.case["fail_value"] if n == s.case["fail_at"] else 0


def destination(s, address, name):
    if s.obj <= address < s.obj + 0x448:
        return address - s.obj
    if name == "tv[t]":
        if not s.calendar_base:
            s.calendar_base = address
        return -1000 + address - s.calendar_base
    if name == "pv":
        if not s.float_base:
            s.float_base = address
        return -2000 if address == s.float_base else -2001
    return -3000


def scalar(kind, size, save):
    def stub(e):
        s = e.os
        name = e.cstr(e.r(5))
        p = e.r(4)
        data = e.read(p, size)[::-1] if save else b""
        e.cl.rec(("S:" if save else "L:") + kind,
                 (e.cl.lab(e.r(3)), destination(s, p, name), T.s32(e.r(6))), (name, data.hex()))
        if save:
            s.file.extend(data)
        else:
            n = min(s.case["partial"], size) if s.calls == s.case["fail_at"] else size
            old = bytearray(e.read(p, size)[::-1])
            old[:n] = s.file[s.pos:s.pos + n]
            e.write(p, bytes(old[::-1]))
        s.pos += size
        e.set_r(3, result(e) & MASK)
    return stub


for _kind, (_load, _save, _size) in VARS.items():
    STUBS.internal(_load, scalar(_kind, _size, False))
    STUBS.internal(_save, scalar(_kind, _size, True))


def base(e):
    e.cl.rec("Base", (e.r(3) - e.os.obj, e.cl.lab(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), e.r(7)))
    e.write(e.r(3), bytes(x ^ 0xA5 for x in e.read(e.r(3), 12)))
    e.set_r(3, e.os.case["base"] & MASK)


def history(kind):
    def stub(e):
        e.cl.rec(kind, (e.r(3) - e.os.obj, e.cl.lab(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), e.r(7)))
        e.write(e.r(3), bytes(x ^ 0x5A for x in e.read(e.r(3), 0x88)))
        e.set_r(3, result(e) & MASK)
    return stub


def log(e):
    e.cl.rec("Log", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(6))), (e.cstr(e.r(5)),))


def assertion(e):
    e.cl.rec("Assert", (T.s32(e.r(3)),), (e.cstr(e.r(4)),))


def file_io(save):
    def stub(e):
        s = e.os
        fail = result(e)
        count = 3 if fail else s.case["count"]
        n = min(count, e.r(6))
        if save:
            data = e.read(e.r(5), e.r(6))
            e.cl.rec("Write", (e.cl.lab(e.r(4)), e.r(6), e.u32(e.r(7)), count), (data.hex(),))
            s.file.extend(data[:n])
        else:
            data = bytes(s.file[s.pos:s.pos + n])
            e.cl.rec("Read", (e.cl.lab(e.r(4)), destination(s, e.r(5), "mState"), e.r(6), e.u32(e.r(7)), count),
                     (data.hex(),))
            e.write(e.r(5), data)
            s.enum_n = n
            s.enum_failure = bool(fail or s.case["error"] < 0 or count != 4)
        s.pos += n
        w32(e, e.r(7), count)
        w32(e, e.r(3), -1 if fail else s.case["error"])
    return stub


def get_time(e):
    s = e.os
    pointers = [e.r(i) for i in range(5, 11)] + [e.u32(e.r(1) + 0x38), e.u32(e.r(1) + 0x3C)]
    s.calendar_base = pointers[0]
    e.cl.rec("GetTime", (e.r(4) - s.obj,) + tuple((p - pointers[0]) // 4 for p in pointers[1:]))
    e.cl.rec("Calendar", tuple(s.case["calendar"]))
    for p, value in zip(pointers, s.case["calendar"]):
        w32(e, p, value)
    e.write(e.r(4), bytes(x ^ 0x3C for x in e.read(e.r(4), 8)))
    w32(e, e.r(3), s.case["time_error"])


def set_time(e):
    values = [T.s32(e.r(i)) for i in range(5, 11)] + [T.s32(e.u32(e.r(1) + 0x38))]
    e.cl.rec("SetTime", (e.r(4) - e.os.obj,) + tuple(values))
    y, mo, d, h, mi, sec, ms = values
    w32(e, e.r(4), y ^ h ^ ms)
    w32(e, e.r(4) + 4, mo ^ d ^ mi ^ sec)
    w32(e, e.r(3), e.os.case["time_error"])


STUBS.internal(0x1012E010, base)
STUBS.internal(0x10111560, history("History<uint>"))
STUBS.internal(0x10110FEC, history("History<int>"))
STUBS.internal(0x1015B3DC, log)
STUBS.internal(0x1000DDE8, assertion)
STUBS.imp("LbFile_Read__FPvPvUlPUl", file_io(False))
STUBS.imp("LbFile_Write__FPvPCvUlPUl", file_io(True))
STUBS.imp("GetTime__11TbTimeStampCFPiPiPiPiPiPiPiPi", get_time)
STUBS.imp("SetTime__11TbTimeStampFiiiiiii", set_time)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def build(self, case):
        self.case = case
        self.blocks = []
        self.block = self.alloc(0x468)
        self.obj = self.block + 16
        self.calls, self.pos = 0, 0
        self.file = bytearray(case["file"])
        self.calendar_base = self.float_base = 0
        self.enum_n, self.enum_failure = -1, False
        raw = bytearray(case["object"])
        self.initial = bytes(raw[16:16 + 0x448])
        if self.big:
            for offset, size in SWAPS:
                a = 16 + offset
                raw[a:a + size] = raw[a:a + size][::-1]
        self.initial_raw = bytes(raw[16:16 + 0x448])
        self.write(self.block, bytes(raw))

    def observe(self, answer):
        c = self.case
        if c["op"] == "serialise":
            if c["base"] != 0:
                assert answer == 1 and self.calls == 0
            elif c["version"] not in (0, 1):
                assert answer == 0 and self.calls == 0
            else:
                failure = c["fail_at"]
                if c["count"] != 4 or c["error"] < 0:
                    failure = min(80, failure) if failure >= 0 else 80
                assert self.calls == (failure + 1 if failure >= 0 else FIELD_CALLS)
                assert answer == int(failure >= 0)
        raw = self.read(self.block, 0x468)
        body = bytearray(raw[16:16 + 0x448])
        enum = None
        if self.enum_failure:
            a, n = 0x198, self.enum_n
            enum = (bytes(body[a:a + n]), body[a + n:a + 4] == self.initial_raw[a + n:a + 4])
            body[a:a + 4] = self.initial_raw[a:a + 4]
        if self.big:
            for a, size in SWAPS:
                body[a:a + size] = body[a:a + size][::-1]
        if "price" in self.case and self.case["fail_at"] in (-1, 107):
            price = min(500, max(0, self.case["price"]))
            assert int.from_bytes(body[0x190:0x194], "little") == price
        return dict(answer=answer, object=bytes(body), guards=raw[:16] + raw[-16:],
                    file=bytes(self.file), position=self.pos, enum=enum, calls=self.calls, log=self.log())


class Ppc(Side):
    big = True

    def __init__(self, e, case):
        self.e = e
        self.build(case)
        e.os = self
        e.cl = CL.PpcLog({HANDLE: 1, 0x1037FEFC: 2, self.obj: 3})
        self.saved = STUBS.activate(e)

    def alloc(self, size):
        return self.e.alloc(size)

    def write(self, a, data):
        self.e.write(a, data)

    def read(self, a, size):
        return self.e.read(a, size)

    def log(self):
        return self.e.cl.log

    def execute(self):
        c = self.case
        if c["op"] == "enum":
            name = self.e.alloc(16, b"state-name\0")
            return T.call_ppc(self.e, SAVE if c["save"] else LOAD,
                              [HANDLE, self.obj + 0x198, name, c["no_log"] & MASK])[0]
        with Rounding(self.e):
            return T.call_ppc(self.e, SER, [self.obj, HANDLE, c["save"] & MASK, c["version"] & MASK, c["unused"]])[0]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None


def bind(lib):
    if getattr(lib, "osr_bound", False):
        return
    lib.tpw_osr_addr.argtypes = [C.c_int32]
    lib.tpw_osr_addr.restype = C.c_uint64
    lib.tpw_osr_call.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_osr_call.restype = C.c_uint32
    lib.tpw_osr_enum.argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_osr_enum.restype = C.c_uint32
    lib.tpw_osr_reset.argtypes = [C.c_uint64, C.c_char_p, C.c_uint32, C.POINTER(C.c_int32), C.POINTER(C.c_int32)]
    lib.tpw_osr_file.argtypes = [C.c_void_p, C.POINTER(C.c_uint32)]
    lib.tpw_osr_file.restype = C.c_uint32
    lib.tpw_osr_enum_read.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_osr_enum_read.restype = C.c_int32
    lib.tpw_osr_calls.restype = C.c_int32
    lib.tpw_osr_init.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_uint16)]
    lib.tpw_osr_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 8)()
    assert lib.tpw_osr_layout(out) == 8
    assert list(out) == [0x448, 0x10, 0x14, 0xF4, 0x198, 0x33C, 0x3C4, 0x406]
    lib.osr_bound = True


class Host(Side):
    big = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.build(case)
        config = [case[k] for k in ("fail_at", "fail_value", "base", "error", "count", "partial", "time_error")]
        lib.tpw_osr_reset(self.obj, bytes(self.file), len(self.file), (C.c_int32 * 7)(*config),
                          (C.c_int32 * 8)(*case["calendar"]))
        CL.host_begin(lib, {1: lib.tpw_osr_addr(0), 2: lib.tpw_osr_addr(1), 3: self.obj})

    def alloc(self, size):
        block = C.create_string_buffer(size)
        self.blocks.append(block)
        return C.addressof(block)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def read(self, a, size):
        return C.string_at(a, size)

    def log(self):
        return CL.host_end(self.lib)

    def execute(self):
        c = self.case
        if c["op"] == "enum":
            answer = self.lib.tpw_osr_enum(self.obj, c["save"], c["no_log"])
        else:
            answer = self.lib.tpw_osr_call(self.obj, c["save"], c["version"], c["unused"])
        data = C.create_string_buffer(8192)
        position = C.c_uint32()
        n = self.lib.tpw_osr_file(data, C.byref(position))
        self.file, self.pos = bytearray(data.raw[:n]), position.value
        failed = C.c_int32()
        self.enum_n = self.lib.tpw_osr_enum_read(C.byref(failed))
        self.enum_failure = bool(failed.value)
        self.calls = self.lib.tpw_osr_calls()
        return answer

    def done(self):
        pass


def run(s):
    try:
        return s.observe(s.execute())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case))


def host(lib, case):
    return run(Host(lib, case))


def case(r, save, **changes):
    raw = bytes(r.getrandbits(8) for _ in range(0x468))
    c = dict(op="serialise", object=raw, file=b"" if save else bytes(r.getrandbits(8) for _ in range(2048)),
             save=save, version=1, unused=r.getrandbits(32), calendar=[T.s32(r.getrandbits(32)) for _ in range(8)],
             fail_at=-1, fail_value=1, base=0, error=0, count=4, partial=4, time_error=0, no_log=0)
    c.update(changes)
    return c


# 118 scalar/enum/history calls, in addition to the always-first base call.
FIELD_CALLS = 118


def edges(save):
    r = random.Random(229 + save)
    yield case(r, save)
    for version in (-0x80000000, -1, 0, 1, 2, 0x7FFFFFFF):
        for base_answer in (0, 1, -1, 7):
            yield case(r, save, version=version, base=base_answer)
    for save_flag in ((1, -1, 2, -0x80000000) if save else (0,)):
        yield case(r, save_flag)
    for fail_at in range(FIELD_CALLS):
        for value in (1, -1, 7):
            yield case(r, save, fail_at=fail_at, fail_value=value, partial=fail_at % 5)
    for time_error in (-0x80000000, -1, 0, 1, 0x7FFFFFFF):
        yield case(r, save, time_error=time_error)
    for count in (0, 1, 3, 4, 5):
        for error in (-0x80000000, -1, 0, 1, 0x7FFFFFFF):
            yield case(r, save, count=count, error=error)
    if save:
        floats = [-0.0, 0.0, -0.99, -1.99, 0.99, 1.99, 255.99, 256.0, 257.0, -1.0, -256.0,
                  float("nan"), float("inf"), -float("inf"), 2147483648.0, -2147483904.0, 2147483520.0]
        for i, f in enumerate(floats):
            c = case(r, save)
            raw = bytearray(c["object"])
            for j, a in enumerate((0x48, 0x44, 0x40)):
                raw[16 + a:20 + a] = struct.pack("<f", floats[(i + j) % len(floats)])
            c["object"] = bytes(raw)
            yield c
    else:
        # Price is call index 106, after the six history calls and owned enum I/O.
        for price in (-1, 0, 499, 500, 501, -0x80000000, 0x7FFFFFFF):
            for failure in (-1, 105, 106, 107):
                c = case(r, save, fail_at=failure)
                c["price"] = price
                yield with_price(c, price)
        for bits in (0, 0x80000000, 0x7F800000, 0xFF800000, 0x7FC12345, 0x7F812345, 1, 0xFFFFFFFF):
            for failure in (-1, 109, 110, 111):
                c = case(r, save, fail_at=failure)
                data = bytearray(c["file"])
                data[252:264] = struct.pack("<3I", bits, bits ^ 0x80000000, bits)
                c["file"] = bytes(data)
                yield c


def with_price(c, price):
    # The fake histories consume no file bytes. The price is at byte offset 240.
    data = bytearray(c["file"])
    data[240:244] = struct.pack("<i", price)
    c["file"] = bytes(data)
    return c


def random_case(r, save):
    return case(r, r.choice([1, -1, 2, -0x80000000]) if save else 0,
                version=r.choice([0, 1, 1, 1, -1, 2]), fail_at=r.choice([-1, -1, r.randrange(FIELD_CALLS)]),
                fail_value=r.choice([1, -1, 7]), partial=r.randrange(5), time_error=r.choice([-1, 0, 1]))


def enum_edges(save):
    r = random.Random(230 + save)
    for no_log in (0, 1, -1):
        for count in (0, 1, 3, 4, 5):
            for error in (-1, -0x80000000, 0, 1, 0x7FFFFFFF):
                yield case(r, save, op="enum", no_log=no_log, count=count, error=error)
    for pattern in (0, 1, 4, 5, 0xFFFFFFFF, 0x80000000, 0x12345678):
        c = case(r, save, op="enum")
        raw = bytearray(c["object"])
        raw[16 + 0x198:20 + 0x198] = struct.pack("<I", pattern)
        c["object"] = bytes(raw)
        if not save:
            c["file"] = struct.pack("<I", pattern) + c["file"][4:]
        yield c


def enum_random(r, save):
    return case(r, save, op="enum", no_log=r.choice([0, 1, -1]), count=r.randrange(6),
                error=r.choice([-0x80000000, -1, 0, 1, 0x7FFFFFFF]))


INIT_STUBS = CL.Stubs()
INIT_STUBS.real.add(INIT)


def init_ctor(e):
    words = [e.u32(GLOBALS[0]), e.u32(GLOBALS[1])]
    text = e.read(e.r(4), 16).decode("utf-16-be").rstrip("\0")
    e.osi.append(("Ctor", words, text, e.r(3) == GLOBALS[2]))
    e.write(e.r(3), struct.pack(">5I", 0, 7, 0, 8, 16))
    e.set_r(3, e.r(3) + e.os_return)


def init_register(e):
    e.osi.append(("Register", e.r(3) == GLOBALS[2] + e.os_return, e.r(4) == 0x103BCBB8, e.r(5) == GLOBALS[3]))


INIT_STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", init_ctor)
INIT_STUBS.imp("__register_global_object", init_register)


def init_ppc(e, c):
    e.osi = []
    e.os_return = c["return"]
    e.write(GLOBALS[2] - 4, b"\xA5" * 40)
    e.write(GLOBALS[2] + 40, b"\x5A" * 4)
    w32(e, GLOBALS[0], c["dirty"][0])
    w32(e, GLOBALS[1], c["dirty"][1])
    before = e.read(GLOBALS[2] - 4, 4) + e.read(GLOBALS[2] + 20, 12) + e.read(GLOBALS[2] + 40, 4)
    saved = INIT_STUBS.activate(e)
    try:
        T.call_ppc(e, INIT, [])
    finally:
        CL.Stubs.deactivate(e, saved)
    guards = e.read(GLOBALS[2] - 4, 4) + e.read(GLOBALS[2] + 20, 12) + e.read(GLOBALS[2] + 40, 4)
    return dict(words=[e.u32(GLOBALS[0]), e.u32(GLOBALS[1])], text="gms.dat", length=e.u32(GLOBALS[2] + 4),
                log=e.osi, guards=guards == before)


def init_host(lib, c):
    bind(lib)
    words = (C.c_int32 * 5)()
    text = (C.c_uint16 * 16)()
    lib.tpw_osr_init(words, text)
    s = bytes(bytearray(b"".join(struct.pack("<H", x) for x in text[:8]))).decode("utf-16-le").rstrip("\0")
    constructed = bytes(bytearray(b"".join(struct.pack("<H", x) for x in text[8:]))).decode("utf-16-le").rstrip("\0")
    return dict(words=list(words[2:4]), text=s, length=words[4],
                log=[("Ctor", list(words[:2]), constructed, True), ("Register", True, True, True)], guards=True)


def init_random(r):
    return {"dirty": [r.getrandbits(32), r.getrandbits(32)], "return": r.choice([0, 4, 12])}


TESTS = [
    FnTest("CObject::Serialise save", ppc, host, edge=lambda: edges(1), rnd=lambda r: random_case(r, 1)),
    FnTest("CObject::Serialise load", ppc, host, edge=lambda: edges(0), rnd=lambda r: random_case(r, 0)),
    FnTest("LOADVAR<ObjectState>", ppc, host, edge=lambda: enum_edges(0), rnd=lambda r: enum_random(r, 0)),
    FnTest("SAVEVAR<ObjectState>", ppc, host, edge=lambda: enum_edges(1), rnd=lambda r: enum_random(r, 1)),
    FnTest("__sinit_Object_cpp", init_ppc, init_host,
           edge=lambda: [{"dirty": [0, 0], "return": 0}, {"dirty": [MASK, MASK], "return": 4}], rnd=init_random),
]
