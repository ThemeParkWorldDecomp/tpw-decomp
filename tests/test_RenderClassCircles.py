"""w239: Draw.cpp CRenderClass circle rasterizers against the Mac binary."""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest


PROGRAM = 'bullfrog symbols shared'
SHIM_LIBRARY = 'unit_RenderClassCircles'
OUTLINE = {8: 0x10049590, 16: 0x1004ffe4, 24: 0x10056bcc, 32: 0x1005ed18}
FILLED = {8: 0x1004b4e8, 16: 0x10051ffc, 24: 0x100591f4, 32: 0x10060d14}
FORMATTED = 0
FORMAT_CALLS = 0
STUBS = CL.Stubs()
STUBS.real.update(OUTLINE.values())
STUBS.real.update(FILLED.values())
STUBS.real.update([0x1002a06c, 0x100591c4])


def colour_format(emulator):
    global FORMAT_CALLS
    FORMAT_CALLS += 1
    emulator.set_r(3, FORMATTED)


STUBS.internal(0x100193dc, colour_format)


FORMATS = {
    8: [0xe0, 0x1c, 0x03, 0],
    16: [0x7c00, 0x03e0, 0x001f, 0],
    24: [0xff0000, 0x00ff00, 0x0000ff, 0],
    32: [0xffffff00, 0, 0, 0xff],
}


def bind(lib):
    if getattr(lib, 'rclasscircle_bound', False):
        return
    lib.tpw_rclasscircle_layout.argtypes = [C.c_uint]
    lib.tpw_rclasscircle_layout.restype = C.c_uint32
    lib.tpw_rclasscircle_calls.restype = C.c_uint32
    lib.tpw_rclasscircle_run.argtypes = [
        C.c_uint32, C.c_uint32, C.c_void_p, C.c_int32, C.c_int32, C.c_int32,
        C.c_void_p, C.c_void_p, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32,
        C.c_void_p, C.c_uint32,
    ]
    layout = [lib.tpw_rclasscircle_layout(i) for i in range(4)]
    assert layout == [48, 24, 40, 36], layout
    lib.rclasscircle_bound = True


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
            return emulator.alloc(len(data), data)
        block = C.create_string_buffer(data, len(data))
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
    clip = allocate(b''.join((v & 0xffffffff).to_bytes(4, endian) for v in case['clip']))
    colour = allocate(case['colour'])
    format_values = FORMATS[depth]
    for i, value in enumerate(format_values):
        put(area + i * 4, value)
    put(area + 16, depth)
    put(area + 20, int(depth == 8))
    put(area + data_offset, origin, pointer_size)
    put(area + data_offset + pointer_size, width)
    put(area + data_offset + pointer_size + 4, height)
    put(area + stride_offset, stride)
    protected = [(area, area_size), (clip, 16), (colour, 36)]
    before = [read(address, size) for address, size in protected]
    formatted = case['formatted']

    saved = STUBS.activate(emulator) if emulated else None
    try:
        if emulated:
            FORMATTED = formatted
            FORMAT_CALLS = 0
            entry = OUTLINE[depth] if case['operation'] == 0 else FILLED[depth]
            this = allocate(b'\xd7' * 32)
            T.call_ppc(emulator, entry, [this, area, clip, case['flags'],
                                         case['x'] & 0xffffffff, case['y'] & 0xffffffff,
                                         case['radius'], colour])
            calls = FORMAT_CALLS
        else:
            format_array = (C.c_uint32 * 4)(*format_values)
            lib.tpw_rclasscircle_run(case['operation'], depth, origin, width, height,
                                     stride, format_array, clip, case['flags'],
                                     case['x'], case['y'], case['radius'], colour, formatted)
            calls = lib.tpw_rclasscircle_calls()
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
    width, height = r.randrange(8, 58), r.randrange(8, 46)
    # The 24-bit renderer consumes the high three bytes of a 32-bit formatted
    # value, so retain all four source bytes in those cases.
    bits = 8 if depth == 8 else 16 if depth == 16 else 32
    formatted = r.getrandbits(bits)
    return {
        'seed': r.getrandbits(32),
        'operation': operation,
        'depth': depth,
        'size': (width, height),
        'padding': r.randrange(4, 13),
        'clip': [0, 0, width, height],
        'flags': r.choice([0, 8, 0x10, 0x18, 0x20, 0x28]),
        'x': r.randrange(-32, width + 32),
        'y': r.randrange(-32, height + 32),
        'radius': r.randrange(24),
        'colour': r.randbytes(36),
        'formatted': formatted,
    }


def edges(depth, operation):
    r = random.Random(239000 + depth * 10 + operation)
    out = []
    for flags in [0, 1, 8, 0x10, 0x18, 0x20, 0x28, 0xffffffff]:
        for radius in [0, 1, 2, 3, 4, 7, 12, 19]:
            for x, y in [(0, 0), (20, 16), (39, 31), (-2, 16), (41, 16)]:
                case = base(r, depth, operation)
                case.update(size=(40, 32), padding=11, clip=[0, 0, 40, 32],
                            flags=flags, radius=radius, x=x, y=y)
                out.append(case)
        for clip in [[4, 3, 35, 29], [5, 5, 5, 20], [5, 5, 20, 5],
                     [-3, -2, 37, 30], [12, 10, 7, 4]]:
            case = base(r, depth, operation)
            case.update(size=(40, 32), padding=11, clip=clip, flags=flags,
                        x=20, y=16, radius=12)
            out.append(case)
    return out


def make_test(depth, operation):
    kind = 'CircleOutline' if operation == 0 else 'CircleFilled'
    return FnTest(f'bfl::draw::CRenderClass{depth}::{kind}',
                  lambda emulator, case: run(emulator, None, case),
                  lambda lib, case: run(None, lib, case),
                  edge=lambda: edges(depth, operation),
                  rnd=lambda r: base(r, depth, operation))


TESTS = [make_test(depth, operation) for depth in [8, 16, 24, 32]
         for operation in [0, 1]]
