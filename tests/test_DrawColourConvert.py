"""w241: Draw.cpp colour formatting and palette converters."""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_DrawColourConvert"

ADDR = {
    "colour.format": 0x100193DC, "rotate.get": 0x10019F84,
    "colour.rotates": 0x1001A008, "pal.init": 0x10047740,
    "pal.up": 0x1004786C, "pal.down": 0x10047FD0,
    "pal.shift": 0x1004874C, "pal.dtor": 0x1004889C,
    "mask.ctor": 0x10048924, "mask.dtor": 0x100489C8,
    "mask.init": 0x10048A6C, "mask.release": 0x10048C98,
    "mask.up": 0x10048CFC, "mask.down": 0x10048F88,
    "mask.shift": 0x10049228, "mask.brute": 0x10049378,
    "rotate.init": 0x10065E7C,
}

FORMATS = [
    [0xF000, 0x0F00, 0x00F0, 0x000F, 16, 0],
    [0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF, 32, 0],
    [0x7C00, 0x03E0, 0x001F, 0x8000, 16, 0],
    [0x7C00, 0x03E0, 0x001F, 0, 16, 0],
    [0xF800, 0x07E0, 0x001F, 0, 16, 0],
    [0x0F00, 0x00F0, 0x000F, 0xF000, 16, 0],
    [0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0],
    [0xFF, 0xFF00, 0xFF0000, 0xFF000000, 32, 0],
    [0xFF0000, 0xFF00, 0xFF, 0, 24, 0],
    [0xFF, 0xFF00, 0xFF0000, 0, 32, 0],
    [0x000003E0, 0x00007C00, 0x0000001F, 0x8000, 16, 0],
]


def bind(lib):
    if getattr(lib, "dcc_bound", False):
        return
    VP, U32, I32, U8 = C.c_void_p, C.c_uint32, C.c_int32, C.c_uint8
    specs = {
        "colour_format": (U32, [VP, VP, VP]), "colour_rotates": (U32, [VP, VP]),
        "rotate_get": (U32, [U32, U32]), "rotate_init": (None, [VP, VP, VP]),
        "pal_shift": (None, [U32, U32, VP]), "pal_init": (None, [VP, VP, VP, VP]),
        "pal_line": (None, [U32, VP, VP, VP, VP, I32]), "pal_destruct": (None, []),
        "mask_shift": (None, [U32, U32, VP]),
        "mask_brute": (U8, [VP, U8, U8, U8]),
        "mask_line": (None, [U32, VP, VP, VP, VP, VP, I32]),
        "mask_init": (None, [VP, VP, VP]), "mask_release": (None, []),
        "mask_destruct": (None, []),
    }
    for name, (restype, args) in specs.items():
        fn = getattr(lib, "tpw_dcc_" + name)
        fn.restype, fn.argtypes = restype, args
    lib.dcc_bound = True


def alloc(e, data, alignment=4):
    return e.alloc(max(len(data), 4), data, alignment=alignment)


def pformat(values):
    return struct.pack(">4I2i", *values)


def hformat(values):
    return (C.c_uint32 * 6)(*(v & 0xFFFFFFFF for v in values))


def words_ppc(e, address, count):
    return list(struct.unpack(">" + "I" * count, e.read(address, 4 * count)))


def ppc_colour(e, c, rotates=False):
    colour = alloc(e, c["channels"] + struct.pack(">8I", *c["cache"]))
    arg = alloc(e, struct.pack(">8I", *c["words"]) if rotates else pformat(c["format"]))
    r3, _, _ = T.call_ppc(e, ADDR["colour.rotates" if rotates else "colour.format"], [colour, arg])
    return {"result": r3, "cache": words_ppc(e, colour + 4, 8)}


def host_colour(lib, c, rotates=False):
    bind(lib)
    raw = c["channels"] + struct.pack("<8I", *c["cache"])
    colour = C.create_string_buffer(raw, 36)
    cache = (C.c_uint32 * 8)()
    if rotates:
        words = (C.c_uint32 * 8)(*c["words"])
        result = lib.tpw_dcc_colour_rotates(colour, words)
        return {"result": result, "cache": c["cache"]}
    fmt = hformat(c["format"])
    result = lib.tpw_dcc_colour_format(colour, fmt, cache)
    return {"result": result, "cache": list(cache)}


def colour_case(r, rotates=False):
    channels, cache = r.randbytes(4), [r.getrandbits(32) for _ in range(8)]
    if rotates:
        return {"channels": channels, "cache": cache,
                "words": [r.randrange(32) for _ in range(4)] + [r.getrandbits(32) for _ in range(4)]}
    return {"channels": channels, "cache": cache,
            "format": r.choice(FORMATS) if r.random() < .8 else
            [r.getrandbits(32) for _ in range(4)] + [r.choice([8, 16, 24, 32]), 0]}


def ppc_rotate(e, c, init=False):
    obj = alloc(e, b"\xA5" * 32)
    if not init:
        r3, _, _ = T.call_ppc(e, ADDR["rotate.get"], [obj, c["top"], c["mask"]])
        return {"result": r3}
    source, destination = alloc(e, pformat(c["source"])), alloc(e, pformat(c["destination"]))
    T.call_ppc(e, ADDR["rotate.init"], [obj, source, destination])
    return {"words": words_ppc(e, obj, 8)}


def host_rotate(lib, c, init=False):
    bind(lib)
    if not init:
        return {"result": lib.tpw_dcc_rotate_get(c["top"], c["mask"])}
    out = (C.c_uint32 * 8)()
    lib.tpw_dcc_rotate_init(hformat(c["source"]), hformat(c["destination"]), out)
    return {"words": list(out)}


def mask_value(r, allow_zero=True):
    if allow_zero and r.random() < .15:
        return 0
    lo = r.randrange(32)
    return ((1 << r.randrange(1, 33 - lo)) - 1) << lo


def shift_case(r):
    return {"source": mask_value(r), "destination": mask_value(r)}


def ppc_shift(e, c, mask=False):
    obj = alloc(e, bytes(0x43C if mask else 0x430))
    right, left = alloc(e, bytes(4)), alloc(e, bytes(4))
    T.call_ppc(e, ADDR["mask.shift" if mask else "pal.shift"],
               [obj, c["source"], c["destination"], right, left])
    return {"values": [e.u32(right), e.u32(left)]}


def host_shift(lib, c, mask=False):
    bind(lib)
    out = (C.c_uint32 * 2)()
    getattr(lib, "tpw_dcc_mask_shift" if mask else "tpw_dcc_pal_shift")(
        c["source"], c["destination"], out)
    return {"values": list(out)}


def ppc_pal_init(e, c):
    obj, pal, fmt = alloc(e, b"\xA5" * 0x430), alloc(e, c["palette"]), alloc(e, pformat(c["format"]))
    T.call_ppc(e, ADDR["pal.init"], [obj, pal, fmt])
    return {"state": words_ppc(e, obj + 4, 11), "palette": e.read(obj + 0x30, 0x400)}


def host_pal_init(lib, c):
    bind(lib)
    pal = C.create_string_buffer(c["palette"], 0x400)
    state, copied = (C.c_uint32 * 11)(), C.create_string_buffer(0x400)
    lib.tpw_dcc_pal_init(pal, hformat(c["format"]), state, copied)
    return {"state": list(state), "palette": copied.raw}


def pal_init_case(r):
    return {"palette": r.randbytes(0x400), "format": r.choice(FORMATS[:10])}


def pack_pal_obj(e, state, palette):
    raw = bytearray(0x430)
    raw[4:0x30] = struct.pack(">11I", *state)
    raw[0x30:] = palette
    return alloc(e, bytes(raw))


def ppc_pal_line(e, c, down=False):
    obj = pack_pal_obj(e, c["state"], c["palette"])
    bpp, count = c["state"][10], c["count"]
    dest = alloc(e, b"\xCD" * (count * bpp + 16))
    srcbase = alloc(e, b"\x11" + c["source"] + b"\x22")
    T.call_ppc(e, ADDR["pal.down" if down else "pal.up"], [obj, dest + 8, srcbase + 1, count])
    raw = e.read(dest + 8, count * bpp)
    vals = [int.from_bytes(raw[i*bpp:(i+1)*bpp], "big") for i in range(count)]
    return {"values": vals}


def host_pal_line(lib, c, down=False):
    bind(lib)
    state = (C.c_uint32 * 11)(*c["state"])
    pal = C.create_string_buffer(c["palette"], 0x400)
    source = C.create_string_buffer(b"\x11" + c["source"] + b"\x22")
    dest = C.create_string_buffer(b"\xCD" * (c["count"] * c["state"][10] + 16))
    lib.tpw_dcc_pal_line(int(down), state, pal, C.addressof(dest) + 8,
                         C.addressof(source) + 1, c["count"])
    bpp = c["state"][10]
    raw = dest.raw[8:8 + c["count"] * bpp]
    vals = [int.from_bytes(raw[i*bpp:(i+1)*bpp], "little" if bpp == 2 else "big") for i in range(c["count"])]
    return {"values": vals}


def pal_line_case(r):
    bpp = r.choice([1, 2, 3, 4])
    return {"state": [0xFF, 0xFF00, 0xFF0000, 0, 8, 16, 0, 0, 0,
                      r.getrandbits(32), bpp], "palette": r.randbytes(0x400),
            "source": r.randbytes(18), "count": 16}


def ppc_brute(e, c):
    raw = bytearray(0x43C); raw[4:0x404] = c["palette"]
    obj = alloc(e, bytes(raw))
    r3, _, _ = T.call_ppc(e, ADDR["mask.brute"], [obj, c["red"], c["green"], c["blue"]])
    return {"index": r3 & 0xFF}


def host_brute(lib, c):
    bind(lib)
    pal = C.create_string_buffer(c["palette"], 0x400)
    return {"index": lib.tpw_dcc_mask_brute(pal, c["red"], c["green"], c["blue"])}


def brute_case(r):
    return {"palette": r.randbytes(0x400), "red": r.randrange(256),
            "green": r.randrange(256), "blue": r.randrange(256)}


def pack_mask_obj(e, state, palette, inverse):
    raw = bytearray(0x43C); raw[4:0x404] = palette
    inv = alloc(e, inverse) if inverse is not None else 0
    struct.pack_into(">II", raw, 0x404, 0, inv)
    struct.pack_into(">12I", raw, 0x40C, *state)
    return alloc(e, bytes(raw)), inv


def ppc_mask_line(e, c, down=False):
    obj, _ = pack_mask_obj(e, c["state"], c["palette"], c["inverse"])
    count = len(c["values"])
    source = b"".join(struct.pack(">I", v) for v in c["values"])
    src, dest = alloc(e, source + b"\0\0\0\0"), alloc(e, b"\xCD" * (count + 8))
    T.call_ppc(e, ADDR["mask.down" if down else "mask.up"], [obj, dest + 4, src, count])
    return {"output": e.read(dest + 4, count)}


def host_mask_line(lib, c, down=False):
    bind(lib)
    state = (C.c_uint32 * 12)(*c["state"])
    pal = C.create_string_buffer(c["palette"], 0x400)
    inv = C.create_string_buffer(c["inverse"], len(c["inverse"])) if c["inverse"] is not None else None
    source = C.create_string_buffer(b"".join(struct.pack("<I", v) for v in c["values"]) + b"\0\0\0\0")
    dest = C.create_string_buffer(b"\xCD" * (len(c["values"]) + 8))
    lib.tpw_dcc_mask_line(int(down), state, pal, inv, C.addressof(dest) + 4,
                          source, len(c["values"]))
    return {"output": dest.raw[4:4 + len(c["values"])]}


def mask_line_case(r):
    inverse = r.randbytes(0x8000) if r.random() < .75 else None
    return {"state": [5, 32, 0x00F80000, 0x0000F800, 0x000000F8,
                      19, 11, 3, 10, 5, 0, 4], "palette": r.randbytes(0x400),
            "inverse": inverse, "values": [r.getrandbits(32) for _ in range(r.randrange(1, 18))]}


def ppc_mask_init(e, c, ctor=False):
    obj, pal, fmt = alloc(e, bytes(0x43C)), alloc(e, c["palette"]), alloc(e, pformat(c["format"]))
    entry = ADDR["mask.ctor" if ctor else "mask.init"]
    T.call_ppc(e, entry, [obj, fmt, pal])
    return {"state": words_ppc(e, obj + 0x40C, 12), "palette": e.read(obj + 4, 0x400)}


def host_mask_init(lib, c):
    bind(lib)
    state = (C.c_uint32 * 12)()
    lib.tpw_dcc_mask_init(C.create_string_buffer(c["palette"], 0x400), hformat(c["format"]), state)
    return {"state": list(state), "palette": c["palette"]}


def ppc_void(e, c, kind):
    obj = alloc(e, bytes(0x43C if kind != "pal.dtor" else 0x430))
    if kind in ("mask.release", "mask.dtor"):
        heap = alloc(e, bytes(32)); e.write(obj + 0x408, struct.pack(">I", heap))
    T.call_ppc(e, ADDR[kind], [obj, 0] if kind.endswith("dtor") else [obj])
    return {"completed": True}


def host_void(lib, c, kind):
    bind(lib)
    getattr(lib, {"pal.dtor": "tpw_dcc_pal_destruct", "mask.release": "tpw_dcc_mask_release",
                  "mask.dtor": "tpw_dcc_mask_destruct"}[kind])()
    return {"completed": True}


def setup(runner):
    T.install_std_stubs(runner.emu)
    runner.emu.stub("malloc", lambda e: e.set_r(3, e.alloc(max(e.r(3), 4))))
    runner.emu.stub("free", lambda e: None)
    for a in (0x1001707C, 0x100170B0, 0x10017124):
        T.stub_internal(runner.emu, a, lambda e: None)
    T.call_ppc(runner.emu, 0x1001A104, [])


rotate_case = lambda r: {"top": r.getrandbits(32), "mask": mask_value(r)}
rotate_init_case = lambda r: {"source": [mask_value(r) for _ in range(4)] + [32, 0],
                              "destination": [mask_value(r) for _ in range(4)] + [32, 0]}

TESTS = [
    FnTest("CColour::GetFormattedValue(TbPixFormat)", lambda e,c: ppc_colour(e,c), lambda l,c: host_colour(l,c), rnd=colour_case),
    FnTest("TbMaskRotates::GetRightRotate2", lambda e,c: ppc_rotate(e,c), lambda l,c: host_rotate(l,c), rnd=rotate_case),
    FnTest("CColour::GetFormattedValue(TbMaskRotates)", lambda e,c: ppc_colour(e,c,True), lambda l,c: host_colour(l,c,True), rnd=lambda r: colour_case(r,True)),
    FnTest("TbMaskRotates::Init", lambda e,c: ppc_rotate(e,c,True), lambda l,c: host_rotate(l,c,True), rnd=rotate_init_case),
    FnTest("CPalToMaskConverter::Init", ppc_pal_init, host_pal_init, rnd=pal_init_case),
    FnTest("CPalToMaskConverter::ConvertLineUp", lambda e,c: ppc_pal_line(e,c), lambda l,c: host_pal_line(l,c), rnd=pal_line_case),
    FnTest("CPalToMaskConverter::ConvertLineDown", lambda e,c: ppc_pal_line(e,c,True), lambda l,c: host_pal_line(l,c,True), rnd=pal_line_case),
    FnTest("CPalToMaskConverter::GetShiftBits", lambda e,c: ppc_shift(e,c), lambda l,c: host_shift(l,c), rnd=shift_case),
    FnTest("CPalToMaskConverter::~CPalToMaskConverter", lambda e,c: ppc_void(e,c,"pal.dtor"), lambda l,c: host_void(l,c,"pal.dtor"), rnd=lambda r: {}),
    FnTest("CMaskToPalConverter::CMaskToPalConverter", lambda e,c: ppc_mask_init(e,c,True), host_mask_init, rnd=pal_init_case),
    FnTest("CMaskToPalConverter::~CMaskToPalConverter", lambda e,c: ppc_void(e,c,"mask.dtor"), lambda l,c: host_void(l,c,"mask.dtor"), rnd=lambda r: {}),
    FnTest("CMaskToPalConverter::Init", ppc_mask_init, host_mask_init, rnd=pal_init_case),
    FnTest("CMaskToPalConverter::Release", lambda e,c: ppc_void(e,c,"mask.release"), lambda l,c: host_void(l,c,"mask.release"), rnd=lambda r: {}),
    FnTest("CMaskToPalConverter::ConvertLineUp", lambda e,c: ppc_mask_line(e,c), lambda l,c: host_mask_line(l,c), rnd=mask_line_case),
    FnTest("CMaskToPalConverter::ConvertLineDown", lambda e,c: ppc_mask_line(e,c,True), lambda l,c: host_mask_line(l,c,True), rnd=mask_line_case),
    FnTest("CMaskToPalConverter::GetShiftBits", lambda e,c: ppc_shift(e,c,True), lambda l,c: host_shift(l,c,True), rnd=shift_case),
    FnTest("CMaskToPalConverter::BruteFindColour", ppc_brute, host_brute, rnd=brute_case),
]
