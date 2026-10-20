"""Bullfrog string buffers: TbDynamicStringTemplate's AllocBuffer and ResizeTo (char and
the 16-bit wchar_t) and TbStaticStringTemplate<char>::ResizeTo. Buffers are compared only
where the functions define them: AllocBuffer and a growing ResizeTo leave the rest of the
new buffer uninitialised."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbStringBuffer"
VT = [0x10072580, 0x10072380]        # TbDynamicStringTemplate<char>, <wchar_t> (as in test_TbString)
STATIC_VT = 0x100725d4               # TbStaticStringTemplate<char>
ALLOC = ["AllocBuffer__26TbDynamicStringTemplate<c>FUi", "AllocBuffer__26TbDynamicStringTemplate<w>FUi"]
RESIZE = ["ResizeTo__26TbDynamicStringTemplate<c>FUi", "ResizeTo__26TbDynamicStringTemplate<w>FUi"]
STATIC_RESIZE = "ResizeTo__25TbStaticStringTemplate<c>FUi"


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def wcscpy(e):
        """MSL's wcscpy on the Mac's 16-bit wchar_t (the wide SBCstrcpy calls it)."""
        dst, src = e.r(3), e.r(4)
        i = 0
        while True:
            word = e.read(src + 2 * i, 2)
            e.write(dst + 2 * i, word)
            i += 1
            if word == b"\0\0":
                break

    e.stub("wcscpy", wcscpy)
    # Other modules of this program (test_TbString) replace these functions with fakes
    # inside the emulator; here they must run as the original.
    e.sb = CL.Stubs()
    e.sb.real.update(e.func(label) for label in ALLOC + RESIZE + STATIC_OPS + [
        STATIC_RESIZE, "SBCstrcpy__15TbStringBase<c>FPcPCc", "SBCstrcpy__15TbStringBase<w>FPwPCw",
        "SBCstrlen__15TbStringBase<c>CFPCc", "SBCstrncpy__15TbStringBase<c>FPcPCcUi",
        "SBCstrncat__15TbStringBase<c>FPcPCcUi", "Length__15TbStringBase<c>CFv"])

    def strncpy(e):
        """MSL strncpy: at most n characters, padded with NULs."""
        dst, src, n = e.r(3), e.r(4), e.r(5)
        text = e.cstr(src, limit=1 << 16).encode("latin-1")[:n]
        e.write(dst, text + b"\0" * (n - len(text))) if n else None

    def strncat(e):
        """MSL strncat: appends at most n characters and terminates."""
        dst, src, n = e.r(3), e.r(4), e.r(5)
        old = e.cstr(dst, limit=1 << 16).encode("latin-1")
        e.write(dst + len(old), e.cstr(src, limit=1 << 16).encode("latin-1")[:n] + b"\0")

    e.stub("strncpy", strncpy)
    e.stub("strncat", strncat)


def original(fn):
    """Runs a PowerPC-side case with the unit's functions un-faked."""
    def run(e, c):
        saved = e.sb.activate(e)
        try:
            return fn(e, c)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


def defined(c):
    """How many leading characters of the result the function defines, and whether the
    terminator at the new length is defined too."""
    if c["op"] == 0:
        return 0
    if c["length"] > c["capacity"]:          # grown: the old text was copied with its terminator
        return len(c["text"]) + 1
    return c["length"] + 1                   # in place: the old characters and the new terminator


def dynamic_ppc(e, c):
    wide, width = c["wide"], 2 if c["wide"] else 1
    fmt = ">%dH" if wide else ">%dB"
    cells = c["capacity"] + 1
    raw = list(c["text"]) + [0] + [0x5a] * cells
    raw = raw[:cells] if len(c["text"]) < cells else list(c["text"])[:cells]
    buffer = e.alloc(cells * width + 4, struct.pack(fmt % cells, *raw))
    obj = e.alloc(20, struct.pack(">IiIii", VT[wide], c["length0"], buffer, c["capacity"], c["grow0"]))
    T.call_ppc(e, e.func((ALLOC if c["op"] == 0 else RESIZE)[wide]), [obj, c["length"]])
    length, data, capacity, grow = struct.unpack(">iIii", e.read(obj + 4, 16))
    n = defined(c)
    contents = struct.unpack(fmt % n, e.read(data, n * width)) if n else ()
    terminator = struct.unpack(fmt % 1, e.read(data + length * width, width))[0]
    return dict(fields=(length, capacity, grow), replaced=int(data != buffer), contents=tuple(contents),
                terminator=terminator)


def dynamic_host(lib, c):
    f = lib["tpw_sb_dynamic"]
    f.restype = None
    f.argtypes = [C.c_int32] * 5 + [C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_void_p, C.c_uint32]
    text = (C.c_uint16 * (len(c["text"]) + 1))(*c["text"], 0)
    out = (C.c_int32 * 4)()
    # The contents asked for: the defined prefix, then everything up to the new length so
    # the terminator can be picked out (the undefined part in between is not compared).
    count = max(defined(c), c["length"] + 1 if c["op"] == 0 or c["length"] > c["capacity"] else 0, 1)
    contents = (C.c_uint16 * count)()
    f(c["op"], c["wide"], c["length0"], c["capacity"], c["grow0"], text, len(c["text"]) + 1, c["length"], out,
      contents, count)
    n = defined(c)
    return dict(fields=(out[0], out[1], out[2]), replaced=out[3], contents=tuple(contents[:n]),
                terminator=contents[out[0]] if out[0] < count else 0)


def dynamic_case(r, op=None, wide=None):
    op = r.randrange(2) if op is None else op
    wide = r.randrange(2) if wide is None else wide
    capacity = r.choice([8, 8, 16, 24, 40])
    text = [r.randrange(1, 0x100 if not wide else 0x10000) for _ in range(r.randint(0, capacity))]
    length = r.choice([0, 1, 7, 8, 9, capacity - 1, capacity, capacity + 1, capacity + 9, 63, 64, r.randrange(120)])
    return dict(op=op, wide=wide, capacity=capacity, text=text, length0=len(text), grow0=r.choice([8, 8, 19, 0]),
                length=length)


def dynamic_edges(op):
    r = random.Random(40 + op)
    cases = []
    for wide in (0, 1):
        for length in (0, 7, 8, 15, 16, 17, 100):
            c = dynamic_case(r, op, wide)
            c.update(capacity=16, text=[0x41, 0x42, 0x43], length0=3, length=length)
            cases.append(c)
    return cases


def static_ppc(e, c):
    buffer = e.alloc(c["capacity"] + 8, bytes([0x5a]) * (c["capacity"] + 8))
    obj = e.alloc(20, struct.pack(">IiIii", STATIC_VT, c["length0"], buffer, c["capacity"], c["truncated0"]))
    T.call_ppc(e, e.func(STATIC_RESIZE), [obj, c["length"]])
    length, data, capacity, truncated = struct.unpack(">iIii", e.read(obj + 4, 16))
    return dict(fields=(length, capacity, truncated), buffer=e.read(buffer, c["capacity"] + 8))


def static_host(lib, c):
    f = lib["tpw_sb_static"]
    f.restype = None
    f.argtypes = [C.c_void_p, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32, C.c_void_p]
    buffer = C.create_string_buffer(bytes([0x5a]) * (c["capacity"] + 8), c["capacity"] + 8)
    out = (C.c_int32 * 3)()
    f(buffer, c["capacity"], c["length0"], c["truncated0"], c["length"], out)
    return dict(fields=(out[0], out[1], out[2]), buffer=buffer.raw)


def static_case(r):
    capacity = r.choice([1, 2, 16, 64, 260])
    return dict(capacity=capacity, length0=r.randrange(capacity), truncated0=r.randrange(2),
                length=r.choice([0, 1, capacity - 1, capacity, capacity + 1, capacity * 3, r.randrange(capacity + 4)]))


STATIC_OPS = ["__as__25TbStaticStringTemplate<c>FRC15TbStringBase<c>", "__as__25TbStaticStringTemplate<c>FPCc",
              "__as__25TbStaticStringTemplate<c>FRCc", "__apl__25TbStaticStringTemplate<c>FRC15TbStringBase<c>",
              "__apl__25TbStaticStringTemplate<c>FRCc", "__apl__25TbStaticStringTemplate<c>FPCc"]
BASE_VT = 0x1007254c                 # TbStringBase<char>, a borrowed view


def static_op_ppc(e, c):
    size = c["capacity"] + 8
    initial = bytes(c["initial"]) + b"\0"
    buffer = e.alloc(size, initial + bytes([0x5a]) * (size - len(initial)))
    obj = e.alloc(20, struct.pack(">IiIii", STATIC_VT, len(c["initial"]), buffer, c["capacity"], 0))
    text = e.alloc(len(c["text"]) + 1, bytes(c["text"]) + b"\0")
    if c["op"] in (0, 3):
        argument = e.alloc(16, struct.pack(">IiIi", BASE_VT, len(c["text"]), text, 0))
    elif c["null"]:
        argument = 0
    else:
        argument = text
    T.call_ppc(e, e.func(STATIC_OPS[c["op"]]), [obj, argument])
    length, data, capacity, truncated = struct.unpack(">iIii", e.read(obj + 4, 16))
    return dict(fields=(length, capacity, truncated), buffer=e.read(buffer, size))


def static_op_host(lib, c):
    f = lib["tpw_sb_static_op"]
    f.restype = None
    f.argtypes = [C.c_void_p, C.c_uint32, C.c_int32, C.c_char_p, C.c_int32, C.c_void_p]
    size = c["capacity"] + 8
    initial = bytes(c["initial"]) + b"\0"
    buffer = C.create_string_buffer(initial + bytes([0x5a]) * (size - len(initial)), size)
    out = (C.c_int32 * 3)()
    f(buffer, c["capacity"], c["op"], bytes(c["text"]), c["null"], out)
    return dict(fields=(out[0], out[1], out[2]), buffer=buffer.raw)


def static_op_case(r, op=None):
    op = r.randrange(6) if op is None else op
    capacity = r.choice([4, 8, 16, 32])
    initial = [r.randrange(0x21, 0x7f) for _ in range(r.randrange(capacity))]
    # Texts around what is left of the buffer: shorter, exact, one more, much more, empty.
    room = capacity - 1 - (len(initial) if op >= 3 else 0)
    n = r.choice([0, 1, max(room - 1, 0), room, room + 1, room + 7, r.randrange(40)])
    text = [r.randrange(0x21, 0x7f) for _ in range(max(n, 0))]
    if op in (2, 4) and r.random() < 0.2:
        text = []          # the NUL character: nothing assigned or appended
    return dict(op=op, capacity=capacity, initial=initial, text=text, null=int(op in (1, 5) and r.random() < 0.15))


TESTS = [
    FnTest("TbDynamicStringTemplate<char>::AllocBuffer", original(dynamic_ppc), dynamic_host, edge=lambda: dynamic_edges(0),
           rnd=lambda r: dynamic_case(r, 0, 0)),
    FnTest("TbDynamicStringTemplate<wchar_t>::AllocBuffer", original(dynamic_ppc), dynamic_host,
           rnd=lambda r: dynamic_case(r, 0, 1)),
    FnTest("TbDynamicStringTemplate<char>::ResizeTo", original(dynamic_ppc), dynamic_host, edge=lambda: dynamic_edges(1),
           rnd=lambda r: dynamic_case(r, 1, 0)),
    FnTest("TbDynamicStringTemplate<wchar_t>::ResizeTo", original(dynamic_ppc), dynamic_host,
           rnd=lambda r: dynamic_case(r, 1, 1)),
    FnTest("TbStaticStringTemplate<char>::ResizeTo", original(static_ppc), static_host, rnd=static_case),
] + [FnTest("TbStaticStringTemplate<char>::" + name, original(static_op_ppc), static_op_host,
            rnd=lambda r, op=op: static_op_case(r, op))
     for op, name in enumerate(["operator=(view)", "operator=(text)", "operator=(char)", "operator+=(view)",
                                "operator+=(char)", "operator+=(text)"])]
