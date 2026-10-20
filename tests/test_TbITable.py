"""Formatting bytecode, VARM scanning, and UTF-16 range conversion."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL
import ltms_support as L
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbITable"
VP, I32, U32, U16 = C.c_void_p, C.c_int32, C.c_uint32, C.c_uint16


def F(lib, name, restype, *args):
    f = lib[name]
    f.restype, f.argtypes = restype, args
    return f


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.b2 = CL.Stubs()
    def write(e, p, value):
        cursor, end = e.u32(p + 4), e.u32(p + 8)
        overflow = bool(e.read(p + 16, 1)[0])
        if not overflow and cursor == end:
            overflow = True
            e.write(p + 16, b"\1")
        if not overflow:
            e.write(cursor, struct.pack(">H", value))
            L.word(e, p + 4, cursor + 2)

    def append(e):
        data, size = e.r(4), e.r(5)
        raw = e.read(data, size)
        e.cl.rec("Append", (size,), (raw.hex(),))
        for ch in raw:
            write(e, e.r(3), ch)

    def close(e):
        p = e.r(3)
        cursor, end, conv = e.u32(p + 4), e.u32(p + 8), e.u32(p + 12)
        overflow = bool(e.read(p + 16, 1)[0])
        e.cl.rec("Close", ((cursor - e.b2_start) // 2 if cursor else -1,
                           (end - e.b2_start) // 2, overflow, bool(conv)))
        if cursor:
            if not overflow and cursor == end:
                overflow = True
                e.write(p + 16, b"\1")
            e.write(cursor - 2 if overflow else cursor, b"\0\0")
            L.word(e, p + 4, 0)
        if conv:
            L.word(e, p + 12, 0)
            e.cl.rec("Release")

    def variable(e):
        p = e.r(5)
        fmt = e.u32(p)
        width, precision, flags = struct.unpack(">hHI", e.read(p + 4, 8))
        e.cl.rec("Variable", (e.u32(e.r(3) + 8), width, precision, flags),
                 (e.cstr(fmt) if fmt else "",))
        e.write(p, struct.pack(">IhHI", 0, 123, 456, 789))
        e.set_r(3, T.u32(e.cl.ret("Variable")))

    e.b2.internal(0x10000904, close)
    e.b2.internal(0x100004f0, append)
    e.b2_buffer_vt = T.persist_alloc(e, 32)
    L.word(e, e.b2_buffer_vt + 12, T.fake_function(e, close))
    L.word(e, e.b2_buffer_vt + 16, T.fake_function(e, append))
    e.b2_tag_vt = T.persist_alloc(e, 12)
    L.word(e, e.b2_tag_vt + 8, T.fake_function(e, variable))
    e.b2_conv_vt = T.persist_alloc(e, 28)
    L.word(e, e.b2_conv_vt + 8, T.fake_function(e, lambda e: e.cl.rec("Release")))
    L.word(e, e.b2_conv_vt + 12, T.fake_function(e, lambda e: e.cl.rec("AddRef")))


def case(r):
    tags = [(r.choice([0x5641524d, 0x5641524d, 0x5641524d, 0, 13]), r.randrange(5))
            for _ in range(r.randint(1, 7))]
    tags += [(0, 0)]
    code = []
    for _ in range(r.randint(0, 14)):
        op = r.randrange(1, 7)
        if op == 1:
            raw = r.randbytes(r.randint(0, 15))
            code.append(((len(raw) << 8) | 1, raw + bytes((-len(raw)) % 4)))
        elif op == 2:
            code.append(((r.randrange(5) << 8) | 2, b""))
        elif op == 3:
            raw = r.choice([b"%d\0\0", b"%08d\0\0\0\0", b"%s\0\0"])
            code.append(((len(raw) // 4 << 8) | 3, raw))
        elif op == 4:
            code.append(((r.getrandbits(24) << 8) | 4, b""))
        else:
            code.append(((r.getrandbits(16) << 16) | op, b""))
    code.append((r.choice([0, 0, 0, 7, 255]), b""))
    return dict(tags=tags, code=code, id=r.choice([0, 1, 2, 3, 0xffffffff]), count=r.randrange(4),
                size=r.randint(1, 80), conv=r.randrange(2),
                queues={"Variable": [r.choice([-1, 0, 9]) for _ in range(16)]})


def encode(c, order):
    return b"".join(struct.pack(order + "I", word) + raw for word, raw in c["code"])


def ppc(e, c, op):
    code = e.alloc(len(encode(c, ">")), encode(c, ">"))
    tags = [e.alloc(12, struct.pack(">III", magic, e.b2_tag_vt, id)) if magic else 0
            for magic, id in c["tags"]]
    arguments = e.alloc(len(tags) * 4, struct.pack(">" + "I" * len(tags), *tags))
    array = e.alloc(16, struct.pack(">4I", *([code] * 4)))
    converter = e.alloc(8, struct.pack(">II", e.b2_conv_vt, 1)) if c["conv"] else 0
    table = e.alloc(24, struct.pack(">6I", e.toc + 0xf3c, 1, c["count"], array, converter, 0))
    output = e.alloc(1028, b"\x99\x99" * 514)
    e.b2_start = output + 4
    buffer = e.alloc(20, struct.pack(">4I", e.b2_buffer_vt, e.b2_start, e.b2_start + c["size"] * 2, 0) + b"\0" * 4)
    saved = L.install(e, e.b2, {}, c["queues"])
    try:
        if op == 0:
            value = T.call_ppc(e, e.func("DoFormat__8TbITableCFR13CStaticBufferUlPc"),
                               [table, buffer, 0, arguments])[0]
            extra = 0
        elif op == 1:
            value = T.call_ppc(e, e.func("FormatString__8TbITableCFPwUiUle"),
                               [table, e.b2_start, c["size"], 0] + tags)[0]
            extra = 0
        elif op == 2:
            ptr = e.alloc(4, struct.pack(">I", arguments))
            value = T.call_ppc(e, e.func("FindVariableTag__8TbITableCFUlRPc"), [table, c["id"], ptr])[0]
            value = tags.index(value) if value else -1
            extra = (e.u32(ptr) - arguments) // 4
        else:
            value = T.call_ppc(e, e.func("GetConstString__8TbITableCFUl"), [table, c["id"]])[0]
            value = value - code if value else -1
            extra = 0
        return dict(result=T.s32(value), extra=extra,
                    output=struct.unpack(">514H", e.read(output, 1028)), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, c, op):
    words = [x for tag in c["tags"] for x in tag]
    code = C.create_string_buffer(encode(c, "<"))
    output = (U16 * 514)(*([0x9999] * 514))
    extra = I32()
    CL.host_begin(lib, {}, c["queues"])
    value = F(lib, "tpw_table_call", I32, I32, VP, U32, U32, C.POINTER(U32), U32, VP, U32, I32, C.POINTER(I32))(
        op, code, c["count"], c["id"], (U32 * len(words))(*words), len(c["tags"]),
        C.byref(output, 4), c["size"], c["conv"], C.byref(extra))
    return dict(result=value, extra=extra.value, output=tuple(output), log=CL.host_end(lib))


def edges():
    r = random.Random(17)
    cases = []
    for size in (1, 2, 5, 16):
        for terminator in (0, 7):
            c = case(r)
            c.update(size=size, code=[(0x401, b"abc\0"), (terminator, b"")])
            cases.append(c)
    # Two variables in a row, the first with every parameter set: the second must see the
    # parameters reset (no format, width -1, precision 0, flags 0), whatever the first
    # variable's Format left in the block.
    for results in ([0, 0], [9, 9], [0, -1]):
        c = case(r)
        c.update(tags=[(0x5641524d, 3), (0x5641524d, 1), (0, 0)], size=64,
                 code=[(0x103, b"%d\0\0"), (0x123404, b""), (0x00070005, b""), (0x00090006, b""),
                       (0x302, b""), (0x102, b""), (0, b"")],
                 queues={"Variable": results + [0] * 14})
        cases.append(c)
    return cases


TESTS = [FnTest("TbITable::" + name, lambda e, c, op=i: ppc(e, c, op),
                lambda l, c, op=i: host(l, c, op), edge=edges, rnd=case)
         for i, name in enumerate(["DoFormat", "FormatString", "FindVariableTag", "GetConstString"])]


def unicode_case(r):
    ranges = [(100, 108, 1, 2), (20, 28, 3, 4), (300, 308, 5, 6),
              (1, 9, 0, 0), (50, 58, 0, 0), (200, 208, 0, 0), (600, 608, 0, 0)]
    words = []
    offsets = [i * 26 for i in range(7)]
    for i, (low, high, left, right) in enumerate(ranges):
        words += [low, offsets[left] - offsets[i] if left else 0, high,
                  offsets[right] - offsets[i] if right else 0]
        words += [r.getrandbits(16) for _ in range(9)]
    chars = [r.choice([0, 1, 9, 19, 20, 28, 29, 49, 50, 58, 59, 99, 100, 108, 109,
                       199, 200, 208, 209, 299, 300, 308, 309, 599, 600, 608, 609, 65535])
             for _ in range(r.randint(0, 60))] + [0]
    return dict(words=words, chars=chars, value=r.choice(chars + [r.getrandbits(16)]),
                size=r.choice([0, 1, 2, 3, 4, 5, 10, 20, 65535]))


def unicode_ppc(e, c, op):
    data = e.alloc(len(c["words"]) * 2, struct.pack(">" + "H" * len(c["words"]), *c["words"]))
    source = e.alloc(len(c["chars"]) * 2, struct.pack(">" + "H" * len(c["chars"]), *c["chars"]))
    obj = e.alloc(12, struct.pack(">3I", e.toc + 0xf9c, 1, data))
    out = e.alloc(512, b"\xa5" * 512)
    if op == 0:
        value = T.call_ppc(e, e.func("LocateValue__14TbIUnicodeToMBFw"), [obj, c["value"]])[0]
    else:
        result = e.alloc(4)
        T.call_ppc(e, e.func("ConvertString__14TbIUnicodeToMBFPUcPCwUs"), [result, obj, out + 4, source, c["size"]])
        value = T.s32(e.u32(result))
    return dict(result=value, out=e.read(out, 512))


def unicode_host(lib, c, op):
    data = (U16 * len(c["words"]))(*c["words"])
    chars = (U16 * len(c["chars"]))(*c["chars"])
    out = C.create_string_buffer(b"\xa5" * 512, 512)
    value = F(lib, "tpw_unicode_call", I32, I32, VP, U16, VP, VP, U16)(
        op, data, c["value"], chars, C.byref(out, 4), c["size"])
    return dict(result=value, out=out.raw)


def unicode_edges():
    r = random.Random(18)
    cases = []
    for size in (0, 1, 2, 3, 4, 5, 6):
        c = unicode_case(r)
        c.update(chars=[100, 101, 102, 103, 0], size=size, value=100)
        cases.append(c)
    return cases


TESTS += [FnTest("TbIUnicodeToMB::" + name, lambda e, c, op=i: unicode_ppc(e, c, op),
                 lambda l, c, op=i: unicode_host(l, c, op), edge=unicode_edges, rnd=unicode_case)
          for i, name in enumerate(["LocateValue", "ConvertString"])]
