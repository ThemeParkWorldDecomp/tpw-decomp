"""w240: the remaining Draw.cpp CRenderClass raster methods."""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest


PROGRAM = 'bullfrog symbols shared'
SHIM_LIBRARY = 'unit_RenderClassPrimitives'
NAMES = ['Pixel', 'HorizontalLine', 'VerticalLine', 'Line',
         'RectangleOutline', 'RectangleFilled', 'Triangle',
         'OneColourSprite', 'Sprite', 'PropText']
ENTRIES = {
    8:  [0x1004d3b0, 0x1004d564, 0x1004d89c, 0x1004dbec, 0x1004e184,
         0x1004e368, 0x1004e6dc, 0x1004ebbc, 0x1004f2cc, 0x1004fa08],
    16: [0x10053ef0, 0x100540ac, 0x100543e8, 0x1005473c, 0x10054cec,
         0x10054ed0, 0x1005524c, 0x10055738, 0x10055e80, 0x100565e8],
    24: [0x1005baa0, 0x1005bc90, 0x1005c0c4, 0x1005c518, 0x1005cb24,
         0x1005cd08, 0x1005d178, 0x1005d6b4, 0x1005dec4, 0x1005e6f4],
    32: [0x10062698, 0x10062844, 0x10062ad0, 0x10062e0c, 0x100633ac,
         0x10063590, 0x1006385c, 0x10063d48, 0x1006448c, 0x10064bf4],
}
FORMATS = {
    8: [0xe0, 0x1c, 0x03, 0],
    16: [0x7c00, 0x03e0, 0x001f, 0],
    24: [0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff],
    32: [0xffffff00, 0, 0, 0xff],
}
FORMATTED = 0
FORMAT_CALLS = 0
STUBS = CL.Stubs()
for values in ENTRIES.values():
    STUBS.real.update(values)
STUBS.real.update([0x10029f8c, 0x1002a06c, 0x1002a1d4, 0x1006c12c])


def colour_format(emulator):
    global FORMAT_CALLS
    FORMAT_CALLS += 1
    emulator.set_r(3, FORMATTED)


STUBS.internal(0x100193dc, colour_format)


def bind(lib):
    if getattr(lib, 'rclassprim_bound', False):
        return
    lib.tpw_rclassprim_layout.argtypes = [C.c_uint]
    lib.tpw_rclassprim_layout.restype = C.c_uint32
    lib.tpw_rclassprim_calls.restype = C.c_uint32
    lib.tpw_rclassprim_run.argtypes = [
        C.c_uint32, C.c_uint32, C.c_void_p, C.c_int32, C.c_int32, C.c_int32,
        C.c_void_p, C.c_void_p, C.c_uint32, C.c_void_p, C.c_void_p,
        C.c_void_p, C.c_uint16, C.c_uint16, C.c_void_p, C.c_void_p, C.c_uint32,
    ]
    layout = [lib.tpw_rclassprim_layout(i) for i in range(5)]
    assert layout == [48, 24, 40, 36, 16], layout
    lib.rclassprim_bound = True


def sprite_data(r, width, height):
    output = bytearray()
    for _ in range(height):
        column = 0
        while column < width:
            count = min(r.randrange(1, 7), width - column)
            if r.randrange(3) == 0:
                output.append((-count) & 0xff)
            else:
                output.append(count)
                output.extend(r.randrange(1, 256) for _ in range(count))
            column += count
        output.append(0)
    return bytes(output)


def one_colour_data(r, width, height):
    # The shipped un-clipped X-flip fast path advances two bytes for a
    # negative run and can walk off an otherwise valid palette-sprite stream.
    # Positive runs exercise the one-colour rasterizer without invoking that
    # original out-of-bounds bug.
    output = bytearray()
    for _ in range(height):
        output.append(width)
        output.extend(r.randrange(1, 256) for _ in range(width))
        output.append(0)
    return bytes(output)


def run(emulator, lib, case):
    global FORMATTED, FORMAT_CALLS
    emulated = emulator is not None
    if lib:
        bind(lib)
    endian = 'big' if emulated else 'little'
    pointer_size = 4 if emulated else 8
    area_size = 40 if emulated else 48
    data_offset = 24
    stride_offset = 36 if emulated else 40
    keep = []

    def allocate(data):
        if emulated:
            return emulator.alloc(max(len(data), 4), data)
        block = C.create_string_buffer(data, max(len(data), 4))
        keep.append(block)
        return C.addressof(block)

    def read(address, size):
        return emulator.read(address, size) if emulated else C.string_at(address, size)

    def write(address, data):
        if emulated:
            emulator.write(address, data)
        else:
            C.memmove(address, data, len(data))

    def put(address, value, size=4):
        write(address, (value & ((1 << (size * 8)) - 1)).to_bytes(size, endian))

    r = random.Random(case['seed'])
    depth = case['depth']
    bpp = depth // 8
    width, height = case['size']
    stride = width * bpp + case['padding']
    raw = r.randbytes(stride * height + 132)
    pixels = allocate(raw)
    origin = pixels + 64
    area = allocate(b'\xa5' * area_size)
    clip_data = b''.join((v & 0xffffffff).to_bytes(4, endian) for v in case['clip'])
    clip = allocate(clip_data)
    colour = allocate(case['colour'])
    params_data = b''.join((v & 0xffffffff).to_bytes(4, endian) for v in case['params'])
    params = allocate(params_data)
    palette_data = b''.join((v & 0xffffffff).to_bytes(4, endian) for v in case['palette'])
    palette = allocate(palette_data)
    sprite_bytes = case['sprite_data']
    sprite_data_address = allocate(sprite_bytes)
    sw, sh = case['sprite_size']
    sprite_blob = (sprite_data_address.to_bytes(pointer_size, endian) +
                   sw.to_bytes(2, endian) + sh.to_bytes(2, endian))
    sprite = allocate(sprite_blob)
    text_data = case['text']
    text_address = allocate(text_data)
    format_values = FORMATS[depth]
    for i, value in enumerate(format_values):
        put(area + i * 4, value)
    put(area + 16, depth)
    put(area + 20, int(depth == 8))
    put(area + data_offset, origin, pointer_size)
    put(area + data_offset + pointer_size, width)
    put(area + data_offset + pointer_size + 4, height)
    put(area + stride_offset, stride)

    this = 0
    if emulated:
        vtable = allocate(b'\xe7' * 64)
        descriptors = allocate(b'\xd3' * 24)
        for i, (slot, target) in enumerate(zip([0x10, 0x14, 0x18], ENTRIES[depth][1:4])):
            descriptor = descriptors + i * 8
            put(descriptor, target)
            put(descriptor + 4, emulator.toc)
            put(vtable + slot, descriptor)
        this = allocate(vtable.to_bytes(4, 'big') + b'\xf1' * 12)

    protected = [(area, area_size), (clip, 16), (colour, 36),
                 (params, 24), (palette, 1024),
                 (sprite_data_address, len(sprite_bytes)),
                 (sprite, pointer_size + 4), (text_address, len(text_data))]
    before = [read(address, size) for address, size in protected]
    operation = case['operation']
    saved = STUBS.activate(emulator) if emulated else None
    try:
        if emulated:
            FORMATTED = case['formatted']
            FORMAT_CALLS = 0
            # PropText's newline stop test uses the drawing library's global
            # screen height. Keep it equivalent to the native clip used here.
            put(0x1007486c, case['clip'][3])
            p = [value & 0xffffffff for value in case['params']]
            common = [this, area, clip, case['flags']]
            if operation == 0:
                args = common + p[:2] + [colour]
            elif operation in (1, 2):
                args = common + p[:3] + [colour]
            elif operation == 3:
                args = common + p[:4] + [colour]
            elif operation in (4, 5):
                args = common + [params, colour]
            elif operation == 6:
                args = common + p[:6] + [colour]
            elif operation == 7:
                args = common + p[:2] + [sprite, colour]
            elif operation == 8:
                args = common + [palette] + p[:2] + [sprite]
            else:
                args = common + p[:2] + [text_address, colour]
            T.call_ppc(emulator, ENTRIES[depth][operation], args)
            calls = FORMAT_CALLS
        else:
            format_array = (C.c_uint32 * 4)(*format_values)
            params_array = (C.c_int32 * 6)(*case['params'])
            palette_array = (C.c_uint32 * 256)(*case['palette'])
            lib.tpw_rclassprim_run(operation, depth, origin, width, height, stride,
                                   format_array, clip, case['flags'], params_array,
                                   palette_array, sprite_data_address, sw, sh,
                                   text_address, colour, case['formatted'])
            calls = lib.tpw_rclassprim_calls()
        return {
            'pixels': read(pixels, len(raw)),
            'format_calls': calls,
            'unchanged': [read(address, size) == old
                          for (address, size), old in zip(protected, before)],
        }
    finally:
        if emulated:
            STUBS.deactivate(emulator, saved)


def base(r, depth, operation):
    width, height = r.randrange(12, 58), r.randrange(12, 46)
    sw, sh = r.randrange(1, 24), r.randrange(1, 17)
    bits = 8 if depth == 8 else 16 if depth == 16 else 32
    return {
        'seed': r.getrandbits(32), 'depth': depth, 'operation': operation,
        'size': (width, height), 'padding': r.randrange(4, 13),
        'clip': [0, 0, width, height],
        'flags': r.choice([0, 1, 2, 3, 4, 5, 7, 8, 0x10, 0x18, 0x20, 0x28, 0xffffffff]),
        'params': [r.randrange(-32, width + 32), r.randrange(-32, height + 32),
                   r.randrange(-32, width + 32), r.randrange(-32, height + 32),
                   r.randrange(-32, width + 32), r.randrange(-32, height + 32)],
        'colour': r.randbytes(36), 'formatted': r.getrandbits(bits),
        'palette': [r.getrandbits(32) for _ in range(256)],
        'sprite_size': (sw, sh),
        'sprite_data': (one_colour_data(r, sw, sh) if operation == 7
                        else sprite_data(r, sw, sh)),
        'text': bytes(r.choice(b' ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\t\n')
                      for _ in range(r.randrange(1, 18))) + b'\0',
    }


def random_case(r, depth, operation):
    case = base(r, depth, operation)
    width, height = case['size']
    if operation in (1, 2):
        case['params'][2] = r.randrange(-80, 81)
    elif operation in (4, 5):
        case['params'][:4] = [r.randrange(-20, width + 20), r.randrange(-20, height + 20),
                              r.randrange(-20, width + 20), r.randrange(-20, height + 20)]
    return case


def edges(depth, operation):
    r = random.Random(240000 + depth * 100 + operation)
    output = []
    geometry = [
        [0, 0, 39, 31, 20, 0], [20, 16, 20, 30, 3, 4],
        [20, 16, 35, 16, 5, 5], [-8, -6, 48, 39, 9, 25],
        [39, 31, 0, 0, 7, 3], [5, 5, 6, 6, 5, 12],
    ]
    for flags in [0, 1, 2, 3, 4, 7, 8, 0x10, 0x18, 0x20, 0x28, 0xffffffff]:
        for values in geometry:
            case = base(r, depth, operation)
            case.update(size=(40, 32), padding=11, clip=[0, 0, 40, 32], flags=flags)
            case['params'] = values[:]
            output.append(case)
        case = base(r, depth, operation)
        case.update(size=(40, 32), padding=11, clip=[5, 4, 34, 27], flags=flags)
        case['params'] = [2, 3, 37, 29, 20, 1]
        output.append(case)
    return output


def make_test(depth, operation):
    return FnTest(f'bfl::draw::CRenderClass{depth}::{NAMES[operation]}',
                  lambda emulator, case: run(emulator, None, case),
                  lambda lib, case: run(None, lib, case),
                  edge=lambda: edges(depth, operation),
                  rnd=lambda r: random_case(r, depth, operation))


TESTS = [make_test(depth, operation) for depth in [8, 16, 24, 32]
         for operation in range(10)]
