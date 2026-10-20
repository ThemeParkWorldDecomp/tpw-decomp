"""Unit w217: all 11 bullfrog date formatter functions, both Mac character widths.

Calendar imports and append virtuals are recording fakes on both sides. Compare return
values, every guarded buffer unit, string lengths/truncation, cursor steps, room, and all
callee arguments. String doubles form an array for the original DBCS pointer bug. No
floating-point operations. Failing APIs still fill outputs; uninitialised failure output
and pointer steps outside valid object arrays are not meaningful deterministic inputs.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_StampFormat"
TAG = [0x10027F88, 0x10027164]
STATIC = [0x10028DA8, 0x10028FC0]
DYNAMIC = [0x10026F7C, 0x10026DC0]
BUFFER = 0x100264A8
WRAP_DYNAMIC = [0x10026BC0, 0x10026C40]
WRAP_STATIC = [0x10026CC0, 0x10026D40]
OWN = set(TAG + STATIC + DYNAMIC + [BUFFER] + WRAP_DYNAMIC + WRAP_STATIC)
FT = "FileTimeToSystemTime__10NS_MacDozeFPCQ210NS_MacDoze11tagFILETIMEPQ210NS_MacDoze11_SYSTEMTIME"
DATE = "GetDateFormatA__10NS_MacDozeFUlUlPCQ210NS_MacDoze11_SYSTEMTIMEPCcPci"
TIME = "GetTimeFormatA__10NS_MacDozeFUlUlPCQ210NS_MacDoze11_SYSTEMTIMEPCcPci"
STUBS = CL.Stubs()
STUBS.real.update(OWN)
N = 512
TAGS = ["%", "d", "dd", "w", "ww", "www", "wwww", "m", "mm", "mmm", "mmmm",
        "y", "yy", "yyy", "yyyy", "h", "hh", "ha", "hha", "n", "nn", "s", "ss",
        "l", "ll", "lll", "a", "am", "q", ""]
DEFAULTS = {"dddd": b"Wednesday", "ddd": b"Wed", "MMMM": b"September", "MMM": b"Sep", "tt": b"PM", "t": b"P"}
EDGES = [[0, 9, 99, 999, 1999, 2026, 9999, 10000, 65535], [0, 1, 9, 10, 12, 65535],
         [0, 6, 9, 65535], [0, 1, 9, 10, 31, 99, 100, 2080, 65535],
         [0, 1, 11, 12, 13, 23, 24, 65535], [0, 5, 59, 60, 100, 65535],
         [0, 5, 59, 60, 100, 65535], [0, 5, 50, 999, 1000, 65535]]


def pack(units, width, endian):
    return bytes(units) if width == 1 else struct.pack(endian + str(len(units)) + "H", *units)


def unpack(raw, width, endian):
    return list(raw) if width == 1 else list(struct.unpack(endian + str(len(raw) // 2) + "H", raw))


def read_text(e, address, width):
    out = []
    while True:
        value = int.from_bytes(e.read(address, width), "big")
        if value == 0:
            return out
        out.append(value)
        address += width


def hex_text(units):
    return "".join(f"{unit:04x}" for unit in units)


# Calendar fakes fill outputs even when returning zero, matching the host doubles.
def filetime(e):
    ft, st = e.r(3), e.r(4)
    e.cl.rec("FileTimeToSystemTime", [e.u32(ft), e.u32(ft + 4)])
    e.write(st, struct.pack(">8H", *e.sfm_case["fields"]))
    e.set_r(3, T.u32(e.cl.ret("ftres", 1)))


def calendar_api(event):
    def fake(e):
        picture = e.cstr(e.r(6))
        fields = list(struct.unpack(">8H", e.read(e.r(5), 16)))
        size = T.s32(e.r(8))
        e.cl.rec(event, [e.r(3), e.r(4)] + fields + [size], [picture])
        answer = e.sfm_case["name"]
        if answer is None:
            answer = DEFAULTS[picture]
        e.write(e.r(7), answer[:size - 1] + b"\0")
        e.set_r(3, T.u32(e.cl.ret("apires", 1)))
    return fake


STUBS.imp(FT, filetime)
STUBS.imp(DATE, calendar_api("GetDateFormatA"))
STUBS.imp(TIME, calendar_api("GetTimeFormatA"))


def append_stub(width, fixed, slot, text):
    """Fake a virtual append, recording object index, slot, and every argument unit."""
    def fake(e):
        obj, arg = e.r(3), e.r(4)
        index = (obj - e.sfm_objects) // 20
        values = read_text(e, arg, width) if text else [int.from_bytes(e.read(arg, width), "big")]
        if text:
            e.cl.rec("appendText", [index, slot], [hex_text(values)])
        else:
            e.cl.rec("appendChar", [index, slot, values[0]])
        length, data, capacity, flag = struct.unpack(">iIii", e.read(obj + 4, 16))
        for value in values:
            if value == 0:
                continue
            if fixed and length + 1 >= capacity:
                flag = 1
                break
            e.write(data + length * width, pack([value], width, ">"))
            length += 1
            e.write(data + length * width, b"\0" * width)
        e.write(obj + 4, struct.pack(">i", length))
        if fixed:
            e.write(obj + 16, struct.pack(">i", flag))
        e.set_r(3, obj)
    return fake


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # strlen in the original follows the actual bytes supplied by the API doubles.
    e.stub("strlen", lambda em: em.set_r(3, len(em.cstr(em.r(3)).encode("latin-1"))))
    e.sfm_vtables = {}
    for width in (1, 2):
        for fixed in (False, True):
            vt = T.persist_alloc(e, 0x40)
            slots = {0x24 if fixed else 0x2c: False, 0x28 if fixed else 0x30: True}
            for off in range(0, 0x40, 4):
                fake = append_stub(width, fixed, off, slots[off]) if off in slots else lambda em: em.cl.rec("unexpected")
                tv = T.fake_function(e, fake)
                e.write(vt + off, struct.pack(">I", tv))
            e.sfm_vtables[width, fixed] = vt


def bind(lib):
    if getattr(lib, "sfm_bound", False):
        return
    lib.tpw_sfm_config.argtypes = [C.c_void_p, C.c_char_p, C.c_void_p]
    lib.tpw_sfm_config.restype = None
    lib.tpw_sfm_buffer.argtypes = [C.c_int64, C.c_void_p, C.c_int32, C.c_void_p]
    lib.tpw_sfm_buffer.restype = C.c_int32
    for width in (8, 16):
        f = getattr(lib, f"tpw_sfm_tag{width}")
        f.argtypes = [C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p]
        f.restype = None
        for kind in ("static", "dynamic"):
            f = getattr(lib, f"tpw_sfm_{kind}{width}")
            f.argtypes = [C.c_int, C.c_int64, C.c_void_p, C.c_int, C.c_void_p, C.c_void_p]
            f.restype = C.c_int32
    lib.sfm_bound = True


def ppc_run(e, case, kind, width, wrapper):
    e.sfm_case = case
    log = e.cl = CL.PpcLog({}, case["queues"])
    saved = STUBS.activate(e)
    try:
        fmt = e.alloc((len(case["format"]) + 4) * width, pack(case["format"] + [0] * 4, width, ">"))
        stamp = e.alloc(8, struct.pack(">Q", case["ticks"] & ((1 << 64) - 1)))
        if kind in ("tag", "buffer"):
            size = case["size"] + 64 if kind == "buffer" else 96
            raw = e.alloc((size + 8) * width, pack([0x7e] * (size + 8), width, ">"))
            buf = raw + 8 * width
            if kind == "tag":
                cells = e.alloc(12, struct.pack(">IiI", buf, case["room"], fmt))
                args = [cells, cells + 4, cells + 8] + list(struct.unpack(">4I", struct.pack(">8H", *case["fields"])))
                T.call_ppc(e, TAG[width - 1], args)
                answer = None
                state = [(e.u32(cells) - buf) // width, e.s32(cells + 4), (e.u32(cells + 8) - fmt) // width]
            else:
                answer = T.s32(T.call_ppc(e, BUFFER, [stamp, buf, case["size"], fmt])[0])
                state = []
            return dict(answer=answer, state=state, buffer=unpack(e.read(raw, (size + 8) * width), width, ">"), log=log.log)
        fixed = kind == "static"
        obj = e.alloc(60)
        e.sfm_objects = obj
        buffers = []
        for i in range(3):
            data = e.alloc(N * width, pack([65 + i, 0] + [0x7e] * (N - 2), width, ">"))
            buffers.append(data)
            e.write(obj + i * 20, struct.pack(">IiIii", e.sfm_vtables[width, fixed], 1, data, case["capacity"], case["flags"][i] if fixed else 0))
        address = (WRAP_STATIC if fixed else WRAP_DYNAMIC)[width - 1] if wrapper else (STATIC if fixed else DYNAMIC)[width - 1]
        args = [stamp, obj + 40, fmt] if wrapper else [obj + 40, fmt, stamp]
        answer = T.s32(T.call_ppc(e, address, args)[0])
        state = []
        for i in range(3):
            state.extend([e.s32(obj + i * 20 + 4), e.s32(obj + i * 20 + 16) if fixed else 0])
        return dict(answer=answer, state=state, buffer=[unpack(e.read(data, N * width), width, ">") for data in buffers], log=log.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, case, kind, width, wrapper):
    bind(lib)
    fields = (C.c_uint16 * 8)(*case["fields"])
    flags = (C.c_int32 * 3)(*case["flags"])
    lib.tpw_sfm_config(fields, case["name"], flags)
    CL.host_begin(lib, {}, case["queues"])
    fmt = C.create_string_buffer(pack(case["format"] + [0] * 4, width, "<"))
    if kind in ("tag", "buffer"):
        size = case["size"] + 64 if kind == "buffer" else 96
        raw = C.create_string_buffer(pack([0x7e] * (size + 8), width, "<"), (size + 8) * width)
        buf = C.byref(raw, 8 * width)
        if kind == "tag":
            state = (C.c_int32 * 3)()
            getattr(lib, f"tpw_sfm_tag{8 * width}")(buf, case["room"], fmt, state)
            answer = None
            state = list(state)
        else:
            answer = lib.tpw_sfm_buffer(case["ticks"], buf, case["size"], fmt)
            state = []
        return dict(answer=answer, state=state, buffer=unpack(raw.raw, width, "<"), log=CL.host_end(lib))
    out = (C.c_uint16 * (3 * N))()
    state = (C.c_int32 * 6)()
    answer = getattr(lib, f"tpw_sfm_{kind}{8 * width}")(wrapper, case["ticks"], fmt, case["capacity"], out, state)
    return dict(answer=answer, state=list(state), buffer=[list(out)[i * N:(i + 1) * N] for i in range(3)], log=CL.host_end(lib))


def make_case(fmt, **changes):
    c = dict(format=list(fmt.encode("latin-1")) if isinstance(fmt, str) else list(fmt),
             fields=[2026, 9, 3, 15, 13, 5, 59, 50], ticks=0x0123456789abcdef,
             room=30, size=64, capacity=256, name=None, flags=[0, 0, 0],
             queues={"ftres": [1], "apires": [1]})
    c.update(changes)
    return c


def edges(kind, width):
    rooms = [-2, -1, 0, 1, 2, 3, 5, 30] if kind == "tag" else [30]
    sizes = [0, 1, 2, 3, 8, 64] if kind == "buffer" else [64]
    for tag in TAGS:
        for room in rooms:
            for size in sizes:
                yield make_case(tag if kind == "tag" else "%" + tag, room=room, size=size)
    # Every numeric edge, including halfwords whose tens wrap in narrow output.
    for index, values in enumerate(EDGES):
        for value in values:
            fields = [2026, 9, 3, 15, 13, 5, 59, 50]
            fields[index] = value
            for tag in TAGS:
                yield make_case(tag if kind == "tag" else "%" + tag, fields=fields)
    for name in (b"", b"x", b"\xe9t\xe9", b"\x80\xffA", b"01234567890123456789012345678"):
        for tag in ("www", "wwww", "mmm", "mmmm", "a", "am"):
            for room in ([0, 1, 2, 3, 5, 9, 29, 30] if kind == "tag" else [30]):
                yield make_case(tag if kind == "tag" else "%" + tag, name=name, room=room)
    if kind != "tag":
        for ticks in (0, 0x0123456789abcdef, -1, -(1 << 63)):
            for result in (0, -1, 1):
                yield make_case("[%yyyy/%mm/%dd] %hha:%nn:%ss.%lll %amp!", ticks=ticks,
                                queues={"ftres": [result], "apires": [result]})
        for fmt in ("", "literal", "abc%", "%q!%y!%ww!%yyy!%ll!", "%nn%ss%nn%ss", "%a!Z", "%am!Z"):
            for capacity in (1, 2, 3, 5, 8, 16, 256):
                for size in sizes:
                    yield make_case(fmt, capacity=capacity, size=size)
        if width == 1:
            for fmt in (b"\xffZ", b"\xfeZ%dd", b"A\xffB\xffC", b"\xff", b"\xfe%dd"):
                for flag in (0, 1):
                    for size in sizes:
                        yield make_case(fmt, flags=[flag, flag, 0], size=size)
        else:
            yield make_case([0x80, 0xff, 0xe9, 0xffff, 0x100, 37, 100])
    else:
        for unit in ([0x80, 0xff] if width == 1 else [0x80, 0xff, 0xffff, 0x100]):
            yield make_case([unit])


def random_case(r, kind, width):
    fields = [r.choice(values) if r.random() < 0.6 else r.randrange(65536) for values in EDGES]
    if kind == "tag":
        fmt = list(r.choice(TAGS).encode("ascii"))
    else:
        fmt = list("".join(r.choice(["abc", "Z", "%" + r.choice(TAGS), ":"]) for _ in range(r.randrange(1, 7))).encode("ascii"))
        if width == 1 and r.random() < 0.2:
            fmt = [r.choice([0xfe, 0xff]), 65] + fmt
        elif width == 2 and r.random() < 0.2:
            fmt = [r.choice([0x80, 0xffff, 0x100])] + fmt
    return make_case(fmt, fields=fields, room=r.choice([-1, 0, 1, 2, 3, 5, 30]),
                     size=r.choice([0, 1, 2, 3, 8, 64]), capacity=r.choice([1, 2, 3, 5, 8, 16, 256]),
                     ticks=r.randrange(-(1 << 63), 1 << 63),
                     name=r.choice([None, b"", b"\xe9t\xe9", b"01234567890123456789012345678"]),
                     flags=[r.randrange(2), r.randrange(2), 0],
                     queues={"ftres": [r.choice([0, 1, -1])], "apires": [r.choice([0, 1, -1])]})


def fn(name, kind, width=1, wrapper=False):
    return FnTest(name, lambda e, c: ppc_run(e, c, kind, width, wrapper),
                  lambda l, c: host_run(l, c, kind, width, wrapper),
                  edge=lambda: edges(kind, width), rnd=lambda r: random_case(r, kind, width))


TESTS = [fn("ProcessFormatTag<char>", "tag"), fn("ProcessFormatTag<wchar_t>", "tag", 2),
         fn("TbTimeStamp::Format buffer", "buffer"),
         fn("TemplatedStaticFormat char", "static"), fn("TemplatedStaticFormat wchar_t", "static", 2),
         fn("TemplatedDynamicFormat char", "dynamic"), fn("TemplatedDynamicFormat wchar_t", "dynamic", 2),
         fn("TbTimeStamp::Format dynamic char", "dynamic", wrapper=True),
         fn("TbTimeStamp::Format dynamic wchar_t", "dynamic", 2, True),
         fn("TbTimeStamp::Format static char", "static", wrapper=True),
         fn("TbTimeStamp::Format static wchar_t", "static", 2, True)]
