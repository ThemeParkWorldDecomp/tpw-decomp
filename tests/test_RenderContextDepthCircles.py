"""w234-w238: drawing primitives, built-in text and sprites compared with the Mac binary.

The tests compare complete padded framebuffers and their guards. They also
check that the render area, palette, fade and ghost tables, pixel capabilities,
drawing globals and inputs remain unchanged. Valid sprite RLE uses nonzero
palette indices for painted runs; zero terminates a row.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest


PROGRAM = 'bullfrog symbols shared'
SHIM_LIBRARY = 'unit_RenderContextDepthCircles'
DEPTHS = {
    8: (0x1002c7dc, 1),
    16: (0x1003c890, 2),
    24: (0x10030a50, 3),
    32: (0x10036f7c, 4),
}
FILLED = {8: 0x1002e310, 16: 0x1003fd20, 24: 0x100331b4, 32: 0x10038a1c}
HLINES = {8: 0x1002c2dc, 16: 0x1003c0cc, 24: 0x10030448, 32: 0x100369d4}
VLINES = {8: 0x1002c508, 16: 0x1003c480, 24: 0x10030740, 32: 0x10036c78}
OPERATIONS = {
    2: {8: 0x1002f108, 16: 0x100411d4, 24: 0x1003434c, 32: 0x10039930},
    3: {8: 0x1002b898, 16: 0x1003b2a8, 24: 0x1002f894, 32: 0x10035ee0},
    4: HLINES,
    5: VLINES,
    6: {8: 0x1002bd04, 16: 0x1003b8b0, 24: 0x1002fde0, 32: 0x1003639c},
    7: {8: 0x1002c734, 16: 0x1003c7e4, 24: 0x100309a4, 32: 0x10036ed0},
    8: {8: 0x1002f1fc, 16: 0x1004139c, 24: 0x100344f0, 32: 0x10039aa8},
    9: {8: 0x1002ed98, 16: 0x10040e60, 24: 0x10033fd8, 32: 0x100395bc},
    10: {8: 0x1002bae8, 16: 0x1003b690, 24: 0x1002fbc0, 32: 0x1003617c},
    11: {8: 0x1002bb98, 16: 0x1003b744, 24: 0x1002fc74, 32: 0x10036230},
    12: {8: 0x1002f460, 16: 0x1004178c, 24: 0x10034844, 32: 0x10039db8},
    13: {8: 0x1002f678, 16: 0x10041bc8, 24: 0x10034ae0, 32: 0x10039ff8},
    14: {8: 0x10046a9c, 16: 0x10042008, 24: 0x10034d80, 32: 0x1003a23c},
    15: {8: 0x10046b8c, 16: 0x10042e64, 24: 0x10035690, 32: 0x1003aaf0},
}
PPC_GLOBALS = [0x10074880, 0x10074860, 0x1007485c, 0x10074834,
               0x10074830, 0x10074828, 0x10074838, 0x10074070, 0x10074884]
STUBS = CL.Stubs()
STUBS.imp('abs', lambda emulator: emulator.set_r(
    3, abs(T.s32(emulator.r(3))) & MASK))
STUBS.imp('memset', lambda emulator: emulator.write(
    emulator.r(3), bytes([emulator.r(4) & 0xff]) * emulator.r(5))
    if emulator.r(5) else None)
STUBS.real.update(entry for entry, _ in DEPTHS.values())
STUBS.real.update(FILLED.values())
STUBS.real.update(HLINES.values())
STUBS.real.update(VLINES.values())
for entries in OPERATIONS.values():
    STUBS.real.update(entries.values())
STUBS.real.update([0x10029f8c, 0x1002998c, 0x100299c8])
MASK = 0xffffffff


def bind(lib):
    if getattr(lib, 'rdepthcircle_bound', False):
        return
    for name, ret, args in [
        ('global', C.c_uint64, [C.c_uint]),
        ('layout', C.c_uint32, [C.c_uint]),
        ('run', None, [C.c_uint32, C.c_uint32, C.c_int32, C.c_int32,
                       C.c_int32, C.c_int32, C.c_int32, C.c_int32,
                       C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p,
                       C.c_uint16, C.c_uint16, C.c_void_p]),
    ]:
        function = getattr(lib, 'tpw_rdepthcircle_' + name)
        function.restype, function.argtypes = ret, args
    lib.rdepthcircle_layout = [lib.tpw_rdepthcircle_layout(i) for i in range(5)]
    assert lib.rdepthcircle_layout == [48, 24, 32, 36, 40], lib.rdepthcircle_layout
    lib.rdepthcircle_bound = True


def run(emulator, lib, case):
    if lib:
        bind(lib)
    emulated = emulator is not None
    pointer_size, endian = (4, 'big') if emulated else (8, 'little')
    layout = [40, 24, 28, 32, 36] if emulated else lib.rdepthcircle_layout
    globals_ = PPC_GLOBALS if emulated else [lib.tpw_rdepthcircle_global(i) for i in range(9)]
    keep = []

    def read(address, size):
        return emulator.read(address, size) if emulated else C.string_at(address, size)

    def write(address, data):
        if emulated:
            emulator.write(address, data)
        else:
            C.memmove(address, data, len(data))

    def put(address, value, size=4):
        write(address, (value & ((1 << (size * 8)) - 1)).to_bytes(size, endian))

    def allocate(data):
        if emulated:
            return emulator.alloc(len(data), data)
        buffer = C.create_string_buffer(data, len(data))
        keep.append(buffer)
        return C.addressof(buffer)

    random_ = random.Random(case['seed'])
    depth = case['depth']
    outline_entry, bytes_per_pixel = DEPTHS[depth]
    operation = case['operation']
    entry = outline_entry if operation == 0 else FILLED[depth] if operation == 1 else OPERATIONS[operation][depth]
    width, height, padding = case['size']
    stride = width * bytes_per_pixel + padding
    raw = random_.randbytes(stride * height + 128)
    pixels = allocate(raw)
    row_zero = pixels + 64 + case['origin_x'] * bytes_per_pixel
    origin = row_zero + (height - 1) * stride if case['negative_stride'] else row_zero
    ghost = allocate(b'\xb6' * 32 + random_.randbytes(65536) + b'\xd9' * 32)
    fade_bytes = random_.randbytes(256)
    fade = allocate(b'\xa4' * 32 + fade_bytes + b'\xc5' * 32)
    area = allocate(b'\xa5' * layout[0])
    colour = allocate(case['colour'])
    descriptors = allocate(b'\xd3' * 32)
    vtable = allocate(b'\xe7' * 64)
    object_ = allocate(b'\xf1' * 16)
    rect = allocate(b''.join((value & MASK).to_bytes(4, endian) for value in case.get('rect', [0, 0, 0, 0])))
    text = allocate(case.get('text', b'\0'))
    unicode_bytes = b''.join(value.to_bytes(2, endian) for value in case.get('unicode', [0]))
    unicode = allocate(unicode_bytes)
    sprite_bytes = case.get('sprite_data', b'\0')
    sprite_data = allocate(sprite_bytes)
    sprite_width, sprite_height = case.get('sprite_size', (1, 1))
    sprite = allocate(sprite_data.to_bytes(pointer_size, endian) +
                      sprite_width.to_bytes(2, endian) + sprite_height.to_bytes(2, endian))
    palette = random_.randbytes(1024)
    state_seed = random_.randbytes(40)

    for i in range(6):
        put(area + i * 4, random_.getrandbits(32))
    put(area + layout[1], pixels + 64, pointer_size)
    put(area + layout[2], width)
    put(area + layout[3], height)
    put(area + layout[4], -stride if case['negative_stride'] else stride)
    for i, pointer in [(0, area), (2, origin), (4, ghost + 32), (5, fade + 32)]:
        put(globals_[i], pointer, pointer_size)
    if emulated and operation in (1, 6, 7, 9, 10, 11):
        slots = {}
        if operation in (1, 6, 9, 11):
            slots[0x28] = HLINES[depth]
        if operation in (6, 9, 11):
            slots[0x2c] = VLINES[depth]
        if operation == 7:
            slots[0x1c] = outline_entry
            slots[0x20] = FILLED[depth]
            slots[0x28] = HLINES[depth]
        if operation == 9:
            slots[0x24] = OPERATIONS[6][depth]
        if operation == 10:
            slots[0x10] = OPERATIONS[11][depth]
            slots[0x14] = OPERATIONS[8][depth]
            slots[0x28] = HLINES[depth]
            slots[0x2c] = VLINES[depth]
        for index, (slot, code) in enumerate(slots.items()):
            descriptor = descriptors + index * 8
            put(descriptor, code)
            put(descriptor + 4, emulator.toc)
            put(vtable + slot, descriptor, pointer_size)
        put(object_, vtable, pointer_size)
    for i, value in enumerate(case['clip']):
        put(globals_[1] + i * 4, value)
    put(globals_[3], case['flags'])
    write(globals_[7], palette)
    write(globals_[8], state_seed)

    # TbPixelCaps contains red, green, blue and alpha mask records. Each
    # record is a uint32 mask followed by two int16 shifts.
    for i, (mask, shift_right, shift_left) in enumerate(case['caps']):
        base = globals_[6] + i * 8
        put(base, mask)
        put(base + 4, shift_right, 2)
        put(base + 6, shift_left, 2)

    protected = [(area, layout[0]), (ghost, 65536 + 64), (fade, 256 + 64),
                 (colour, 5), (rect, 16), (text, len(case.get('text', b'\0'))),
                 (unicode, len(unicode_bytes)), (sprite_data, len(sprite_bytes)),
                 (sprite, pointer_size + 4),
                 (object_, 16), (vtable, 64), (descriptors, 32)]
    sizes = [pointer_size, 16, pointer_size, 4, pointer_size, pointer_size, 32, 1024]
    protected += list(zip(globals_[:8], sizes))
    before = [read(address, size) for address, size in protected]
    saved = STUBS.activate(emulator) if emulated else None
    try:
        if emulated:
            colour_words = [int.from_bytes(case['colour'][:4], 'big'),
                            case['colour'][4] << 24]
            if operation == 2:
                T.call_ppc(emulator, entry, [object_] + colour_words)
            elif operation == 3:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK] + colour_words)
            elif operation in (4, 5):
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, case['a'] & MASK] + colour_words)
            elif operation == 6:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, case['a'] & MASK, case['b'] & MASK] + colour_words)
            elif operation == 8:
                T.call_ppc(emulator, entry, [object_, rect] + colour_words)
            elif operation == 9:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, case['a'] & MASK, case['b'] & MASK,
                    case['c'] & MASK, case['d'] & MASK] + colour_words)
            elif operation in (10, 11):
                T.call_ppc(emulator, entry, [object_, rect] + colour_words)
            elif operation == 12:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, text] + colour_words)
            elif operation == 13:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, unicode] + colour_words)
            elif operation == 14:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, sprite])
            elif operation == 15:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK,
                    case['y'] & MASK, sprite] + colour_words)
            else:
                T.call_ppc(emulator, entry, [object_, case['x'] & MASK, case['y'] & MASK,
                    case.get('a', case['radius'])] + colour_words)
        else:
            lib.tpw_rdepthcircle_run(operation, depth, case['x'], case['y'],
                                     case.get('a', case['radius']), case.get('b', 0),
                                     case.get('c', 0), case.get('d', 0), rect, text, unicode,
                                     sprite_data, sprite_width, sprite_height, colour)
        state_words = [int.from_bytes(read(globals_[8] + i * 4, 4), endian) for i in range(10)]
        if operation == 14 and depth == 8:
            state_result = state_words[:4] + [state_words[4] == (origin & MASK),
                state_words[5] == ((ghost + 32) & MASK), read(globals_[8] + 24, 8)] + state_words[8:]
        else:
            state_result = read(globals_[8], 40)
        return {
            'pixels': read(pixels, len(raw)),
            'sprite_state': state_result,
            'unchanged': [read(address, size) == old
                          for (address, size), old in zip(protected, before)],
        }
    finally:
        if emulated:
            STUBS.deactivate(emulator, saved)


def mask_info(mask, top_bit):
    top = mask.bit_length() - 1 if mask else 31
    if top >= top_bit:
        return mask, top - top_bit, 0
    return mask, 0, top_bit - top


def random_caps(random_):
    masks = [random_.getrandbits(16) for _ in range(4)]
    return [mask_info(mask, top) for mask, top in zip(masks, [23, 15, 7, 31])]


def base(random_, depth):
    _, bytes_per_pixel = DEPTHS[depth]
    width, height = random_.randrange(1, 65), random_.randrange(1, 49)
    origin_x = random_.randrange(min(width, 5))
    left = random_.randrange(-origin_x, width - origin_x)
    right = random_.randrange(left, width - origin_x + 1)
    top = random_.randrange(height)
    bottom = random_.randrange(top, height + 1)
    padding = random_.randrange(8)
    return {
        'seed': random_.getrandbits(32),
        'operation': 0,
        'depth': depth,
        'size': (width, height, padding),
        'origin_x': origin_x,
        'clip': [left, top, right, bottom],
        'negative_stride': bool(random_.randrange(2)),
        'flags': random_.getrandbits(32),
        'x': random_.randrange(-96, width + 96),
        'y': random_.randrange(-96, height + 96),
        'radius': random_.randrange(97),
        'colour': random_.randbytes(5),
        'caps': random_caps(random_),
    }


def edges(depth):
    random_ = random.Random(234 + depth)
    output = []
    standard_caps = [mask_info(mask, top) for mask, top in
                     zip([0x7c00, 0x03e0, 0x001f, 0], [23, 15, 7, 31])]
    for flags in range(64):
        for radius in [0, 1, 2, 3, 7, 16, 31]:
            for x, y in [(0, 0), (20, 16), (39, 31), (-1, 16), (40, 16)]:
                case = base(random_, depth)
                case.update(size=(40, 32, 7), origin_x=0, clip=[0, 0, 40, 32],
                            flags=flags, x=x, y=y, radius=radius, caps=standard_caps)
                output.append(case)
        for clip in [[5, 5, 5, 10], [5, 5, 10, 5], [10, 10, 5, 5], [-3, 0, 37, 32]]:
            case = base(random_, depth)
            case.update(size=(40, 32, 7), origin_x=3, clip=clip, flags=flags,
                        x=5, y=5, radius=8)
            output.append(case)
        for radius in [0x80000000, 0x80000001, 0xffffffff]:
            case = base(random_, depth)
            case.update(flags=flags, radius=radius)
            output.append(case)
        for x, y in [(-0x80000000, 0), (0x7fffffff, 1),
                     (0, -0x80000000), (1, 0x7fffffff)]:
            case = base(random_, depth)
            case.update(flags=flags, x=x, y=y, radius=3)
            output.append(case)
    return output


def make_test(depth):
    return FnTest(f'RenderContext{depth}::LbDraw_CircleOutline',
                  lambda emulator, case: run(emulator, None, case),
                  lambda lib, case: run(None, lib, case),
                  edge=lambda: edges(depth),
                  rnd=lambda random_: base(random_, depth))


def filled_edges(depth):
    output = []
    for case in edges(depth):
        if case['radius'] < 0x80000000:
            case['operation'] = 1
            output.append(case)
    return output


def filled_random(random_, depth):
    case = base(random_, depth)
    case['operation'] = 1
    return case


def make_filled_test(depth):
    return FnTest(f'RenderContext{depth}::LbDraw_CircleFilled',
                  lambda emulator, case: run(emulator, None, case),
                  lambda lib, case: run(None, lib, case),
                  edge=lambda: filled_edges(depth),
                  rnd=lambda random_: filled_random(random_, depth))


def clear_edges(depth=16):
    random_ = random.Random(235 + depth)
    output = []
    for flags in range(64):
        for clip in [[0, 0, 1, 1], [0, 0, 40, 32], [3, 5, 29, 21],
                     [7, 9, 7, 19], [7, 9, 19, 9]]:
            case = base(random_, depth)
            case.update(operation=2, size=(40, 32, 7), origin_x=0,
                        clip=clip, flags=flags)
            output.append(case)
    return output


def clear_random(random_, depth=16):
    case = base(random_, depth)
    case['operation'] = 2
    return case


def make_clear_test(depth):
    return FnTest(f'RenderContext{depth}::LbDraw_ClearClipWindow',
                  lambda emulator, case: run(emulator, None, case),
                  lambda lib, case: run(None, lib, case),
                  edge=lambda: clear_edges(depth),
                  rnd=lambda random_: clear_random(random_, depth))


OP_NAMES = {3: 'LbDraw_Pixel', 4: 'LbDraw_HorizontalLine',
            5: 'LbDraw_VerticalLine', 6: 'LbDraw_Line',
            7: 'LbDraw_Circle', 8: 'LbDraw_RectangleFilled',
            9: 'LbDraw_Triangle', 10: 'LbDraw_Rectangle',
            11: 'LbDraw_RectangleOutline', 12: 'LbDraw_PropText',
            13: 'LbDraw_UnicodePropText', 14: 'LbDraw_Sprite',
            15: 'LbDraw_OneColourSprite'}


def sprite_data(random_, width, height):
    output = bytearray()
    for _ in range(height):
        column = 0
        while column < width:
            count = min(random_.randrange(1, 7), width - column)
            if random_.randrange(3) == 0:
                output.append((-count) & 0xff)
            else:
                output.append(count)
                output.extend(random_.randrange(1, 256) for _ in range(count))
            column += count
        output.append(0)
    return bytes(output)


def primitive_edges(operation, depth):
    random_ = random.Random(23600 + operation * 10 + depth)
    output = []
    coordinates = [(-12, -9, 17, 13), (0, 0, 39, 31), (20, 16, 20, 30),
                   (20, 16, 35, 16), (39, 31, 0, 0), (-5, 12, 45, 19),
                   (8, 40, 31, -7), (5, 5, 6, 6)]
    rectangles = [[0, 0, 40, 32], [-8, -6, 48, 39], [5, 7, 29, 25],
                  [8, 9, 8, 20], [20, 18, 7, 3]]
    for flags in range(64):
        if operation in (8, 10, 11):
            for rectangle in rectangles:
                case = base(random_, depth)
                case.update(operation=operation, size=(40, 32, 7), origin_x=0,
                            clip=[0, 0, 40, 32], flags=flags, rect=rectangle)
                output.append(case)
            continue
        for x0, y0, x1, y1 in coordinates:
            case = base(random_, depth)
            case.update(operation=operation, size=(40, 32, 7), origin_x=0,
                        clip=[0, 0, 40, 32], flags=flags, x=x0, y=y0)
            if operation in (4, 5):
                case['a'] = x1 - x0 if operation == 4 else y1 - y0
            elif operation == 6:
                case.update(a=x1, b=y1)
            elif operation == 7:
                case['a'] = [0, 1, 2, 7, 16][len(output) % 5]
            elif operation == 9:
                case.update(a=x1, b=y1, c=coordinates[(len(output) + 3) % len(coordinates)][2],
                            d=coordinates[(len(output) + 3) % len(coordinates)][3])
            elif operation == 12:
                samples = [b'A\0', b'Hello\0', b'A B\0', b'A\tB\0',
                           b'A\nB\0', b'\x01\x7f\x80\xff\0']
                case['text'] = samples[len(output) % len(samples)]
            elif operation == 13:
                samples = [[0x41, 0], [0x48, 0x69, 0], [0x41, 9, 0x42, 0],
                           [0x41, 10, 0x42, 0], [1, 0x7f, 0x80, 0x200, 0]]
                case['unicode'] = samples[len(output) % len(samples)]
            elif operation in (14, 15):
                sw = [1, 2, 7, 16, 31][len(output) % 5]
                sh = [1, 3, 8, 15][len(output) % 4]
                case.update(sprite_size=(sw, sh),
                            sprite_data=sprite_data(random_, sw, sh))
            output.append(case)
    return output


def primitive_random(random_, operation, depth):
    case = base(random_, depth)
    width, height, _ = case['size']
    case.update(operation=operation, clip=[0, 0, width, height])
    if operation in (4, 5):
        case['a'] = random_.randrange(-100, 101)
    elif operation == 6:
        case.update(a=random_.randrange(-96, width + 96),
                    b=random_.randrange(-96, height + 96))
    elif operation == 7:
        case['a'] = random_.randrange(49)
    elif operation == 8:
        case['rect'] = [random_.randrange(-32, width + 32),
                        random_.randrange(-32, height + 32),
                        random_.randrange(-32, width + 32),
                        random_.randrange(-32, height + 32)]
    elif operation == 9:
        case.update(a=random_.randrange(-32, width + 32),
                    b=random_.randrange(-32, height + 32),
                    c=random_.randrange(-32, width + 32),
                    d=random_.randrange(-32, height + 32))
    elif operation in (10, 11):
        case['rect'] = [random_.randrange(-32, width + 32),
                        random_.randrange(-32, height + 32),
                        random_.randrange(-32, width + 32),
                        random_.randrange(-32, height + 32)]
    elif operation == 12:
        alphabet = b' ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\t\n'
        case['text'] = bytes(random_.choice(alphabet) for _ in range(random_.randrange(1, 17))) + b'\0'
    elif operation == 13:
        alphabet = [9, 10, 32, 33, 48, 65, 90, 97, 122, 0x7f, 0x80, 0x100, 0x200]
        case['unicode'] = [random_.choice(alphabet) for _ in range(random_.randrange(1, 17))] + [0]
    elif operation in (14, 15):
        sw, sh = random_.randrange(1, 33), random_.randrange(1, 21)
        case.update(sprite_size=(sw, sh), sprite_data=sprite_data(random_, sw, sh))
    return case


def make_primitive_test(operation, depth):
    return FnTest(f'RenderContext{depth}::{OP_NAMES[operation]}',
                  lambda emulator, case: run(emulator, None, case),
                  lambda lib, case: run(None, lib, case),
                  edge=lambda: primitive_edges(operation, depth),
                  rnd=lambda random_: primitive_random(random_, operation, depth))


TESTS = ([make_test(depth) for depth in DEPTHS] +
         [make_filled_test(depth) for depth in DEPTHS] +
         [make_clear_test(depth) for depth in DEPTHS] +
         [make_primitive_test(operation, depth)
          for operation in range(3, 16) for depth in DEPTHS])
