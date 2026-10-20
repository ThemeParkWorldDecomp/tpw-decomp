"""ltms character-set converters and their factories: MBInfoFactory, MBToUnicodeFactory,
UnicodeToMBFactory, TbIMBInfo::GetIndex, TbIMBToUnicode::ConvertString,
TbIUnicodeToMB::GetMBValue. The file is a queued fake on both sides (reads may come back
short); objects are compared field by field, their tables as values."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL
import ltms_support as L
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbConverters"
FACTORIES = ["CreateInterface__18MBToUnicodeFactoryCFRQ33bfl10FileSystem5CFilePPvUlP16TbIResourceIndex",
             "CreateInterface__18UnicodeToMBFactoryCFRQ33bfl10FileSystem5CFilePPvUlP16TbIResourceIndex",
             "CreateInterface__13MBInfoFactoryCFRQ33bfl10FileSystem5CFilePPvUlP16TbIResourceIndex"]
MAGICS = [0x554d4642, 0x4d554642, 0x424d4642]   # BFMU, BFUM, BFMB
KINDS = {0x1001114c: 1, 0x100111c8: 2, 0x1001142c: 3}   # TbIMBInfo, TbIMBToUnicode, TbIUnicodeToMB vtables
CONVERT_STRING = 0x10003338   # TbIMBToUnicode::ConvertString (no label in the dump)
UNTOUCHED = 0x77777777


def F(lib, name, restype, *args):
    f = lib[name]
    f.restype, f.argtypes = restype, args
    return f


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.cv = CL.Stubs()
    e.cv.imp("_USwapBlock16__13SamsUtilitiesFPUsl", swap16)
    e.cv.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap32)

    def decrement(e):
        value = T.u32(T.s32(e.u32(e.r(3))) - 1)
        L.word(e, e.r(3), value)
        e.set_r(3, value)

    e.cv.imp("InterlockedDecrement__10NS_MacDozeFPl", decrement)
    # Other ltms modules replace index functions with fakes in the emulator; here every
    # function of the library runs as the original.
    e.cv.real.update(f["entry"] for f in e.functions)

    def read(e):
        size = e.r(5)
        e.cl.rec("Read", (size,))
        n = T.s32(T.u32(e.cl.ret("Read", size)))
        if n > 0:
            e.write(e.r(4), e.cv_bytes[e.cv_pos:e.cv_pos + n])
            e.cv_pos += n
        e.set_r(3, T.u32(n))

    def query(name, value):
        def fake(e):
            e.cl.rec(name, (e.cl.lab(e.r(3)),))
            e.set_r(3, T.u32(e.cl.ret(name, value(e))))
        return fake

    # A CFile as the factories use it: Close 0xc, Read 0x10, GetSize 0x1c, GetPos 0x20.
    e.cv_file_vt = T.persist_alloc(e, 0x30)
    L.word(e, e.cv_file_vt + 0x0c, T.fake_function(e, lambda e: e.cl.rec("FileClose", (e.cl.lab(e.r(3)),))))
    L.word(e, e.cv_file_vt + 0x10, T.fake_function(e, read))
    L.word(e, e.cv_file_vt + 0x1c, T.fake_function(e, query("GetSize", lambda e: len(e.cv_bytes))))
    L.word(e, e.cv_file_vt + 0x20, T.fake_function(e, query("GetPos", lambda e: e.cv_pos)))


def swap16(e):
    """SamsUtilities::_USwapBlock16(words, count): the Mac's real swap, recorded."""
    p, n = e.r(3), T.s32(e.r(4))
    e.cl.rec("Swap16", (n,))
    for i in range(max(n, 0)):
        b = e.read(p + 2 * i, 2)
        e.write(p + 2 * i, bytes((b[1], b[0])))


def factory_ppc(e, c):
    e.cv_bytes, e.cv_pos = bytes(c["bytes"]), 0
    file = e.alloc(8, struct.pack(">II", e.cv_file_vt, 0))
    result = e.alloc(4, b"\xee" * 4)
    out = e.alloc(4, struct.pack(">I", UNTOUCHED))
    this = e.alloc(4, b"\0" * 4)
    e.cl = CL.PpcLog({}, c["queues"])
    saved = e.cv.activate(e)
    try:
        e.cl.rec("File", (e.cl.newlab(file),))
        T.call_ppc(e, e.func(FACTORIES[c["which"]]), [result, this, file, out, c["id"], 0])
        e.cl.rec("~File", (e.cl.lab(file),))
        made = e.u32(out)
        fields, words = [0, 0, 0], ()
        kind = -1 if made == UNTOUCHED else 0
        if made not in (UNTOUCHED, 0):
            kind = KINDS[e.u32(made)]
            n = c["words"]
            if kind == 2:
                fields = [T.s32(e.u32(made + 4)), e.read(made + 8, 1)[0], struct.unpack(">H", e.read(made + 10, 2))[0]]
                words = struct.unpack(">%dH" % n, e.read(e.u32(made + 12), 2 * n))
            elif kind == 1:
                fields = [T.s32(e.u32(made + 4)), e.read(made + 8, 1)[0], struct.unpack(">H", e.read(made + 10, 2))[0]]
            else:
                fields = [T.s32(e.u32(made + 4)), 0, 0]
                words = struct.unpack(">%dH" % n, e.read(e.u32(made + 8), 2 * n))
        return dict(result=T.s32(e.u32(result)), kind=kind, fields=tuple(fields), words=tuple(words), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def factory_host(lib, c):
    data = bytes(c["bytes"])
    out = (C.c_int32 * 6)()
    words = (C.c_uint16 * max(c["words"], 1))()
    CL.host_begin(lib, {}, c["queues"])
    F(lib, "tpw_conv_factory", None, C.c_int32, C.c_char_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_void_p,
      C.c_uint32)(c["which"], data, len(data), c["id"], out, words, c["words"])
    log = CL.host_end(lib)
    copied = out[5]
    return dict(result=out[0], kind=out[1], fields=(out[2], out[3], out[4]), words=tuple(words[:copied]), log=log)


def factory_case(r, which=None):
    which = r.randrange(3) if which is None else which
    magic = r.choice([MAGICS[which]] * 6 + [0x484d4642, MAGICS[(which + 1) % 3], r.getrandbits(32)])
    body = r.randbytes(r.choice([0, 2, 6, 7, 16, 33, 64]))
    if which == 1:
        data = struct.pack("<I", magic) + body
        header = 4
    else:
        data = struct.pack("<IHH", magic, r.getrandbits(16), r.getrandbits(16)) + body
        header = 8
    queues = {}
    fault = r.randrange(10)
    if fault == 0:      # the header comes back short
        queues["Read"] = [r.choice([0, header - 1, -1])]
    elif fault == 1:    # the body comes back short
        queues["Read"] = [header, max(len(body) - 1, 0) if body else -1]
    elif fault == 2 and len(body) >= 4:   # the file reports a smaller size (never below the header)
        queues["GetSize"] = [len(data) - r.randrange(3)]
    # The table words compared: what the factory read, whole words only.
    words = 0 if which == 2 else (len(data) - header) // 2
    if "GetSize" in queues:
        words = (queues["GetSize"][0] - header) // 2
    return dict(which=which, bytes=list(data), id=r.getrandbits(8), queues=queues, words=words)


def factory_edges():
    r = random.Random(12)
    cases = []
    for which in range(3):
        c = factory_case(r, which)   # a plain good file
        header = 4 if which == 1 else 8
        good = (struct.pack("<I", MAGICS[which]) if which == 1 else
                struct.pack("<IHH", MAGICS[which], 0x1280, 0x7efc)) + bytes(range(1, 21))
        c.update(bytes=list(good), queues={}, words=0 if which == 2 else 10)
        cases.append(c)
        c = dict(c, queues={"Read": [header - 1]}, words=0)   # short header
        cases.append(c)
        c = dict(c, queues={}, bytes=list(struct.pack("<I", 0x12345678) + good[4:]), words=0)   # wrong magic
        cases.append(c)
        c = dict(c, queues={}, bytes=list(good[:header]), words=0)   # nothing after the header
        cases.append(c)
    # The 'BFMH' variant of the multibyte table: the object is built, the result is failure.
    cases.append(dict(which=0, bytes=list(struct.pack("<IHH", 0x484d4642, 0x0080, 0xfc9f) + bytes(range(40))),
                      id=3, queues={}, words=20))
    return cases


def index_ppc(e, c):
    info = e.alloc(12, struct.pack(">IIBBH", 0x1001114c, 1, c["first"], 0, c["count"]))
    text = e.alloc(4, bytes(c["text"]))
    pointer = e.alloc(4, struct.pack(">I", text))
    value = T.call_ppc(e, e.func("GetIndex__9TbIMBInfoFRPCUc"), [info, pointer])[0]
    return dict(index=T.u32(value), advance=e.u32(pointer) - text)


def index_host(lib, c):
    advance = C.c_int32()
    value = F(lib, "tpw_conv_index", C.c_uint32, C.c_uint32, C.c_uint32, C.c_char_p, C.c_void_p)(
        c["first"], c["count"], bytes(c["text"]), C.byref(advance))
    return dict(index=value, advance=advance.value)


def index_case(r):
    first = r.choice([0, 1, 0x80, 0x81, 0xe0, 0xff, r.randrange(256)])
    lead = r.choice([0, 1, first, (first + 1) & 0xff, (first - 1) & 0xff, 0xff, r.randrange(256)])
    return dict(first=first, count=r.getrandbits(16), text=[lead, r.randrange(256), 0, 0])


def convert_ppc(e, c):
    table = e.alloc(2 * len(c["table"]) + 2, struct.pack(">%dH" % len(c["table"]), *c["table"]))
    converter = e.alloc(16, struct.pack(">IIBBHI", 0x100111c8, 1, c["first"], 0, c["count"], table))
    source = e.alloc(len(c["text"]) + 1, bytes(c["text"]) + b"\0")
    out = e.alloc(2 * 40, b"\x99" * 80)
    result = e.alloc(4, b"\xee" * 4)
    T.call_ppc(e, CONVERT_STRING, [result, converter, out + 2, source, c["size"]])
    return dict(result=T.s32(e.u32(result)), out=struct.unpack(">40H", e.read(out, 80)))


def convert_host(lib, c):
    table = (C.c_uint16 * (len(c["table"]) + 1))(*c["table"])
    out = (C.c_uint16 * 40)(*([0x9999] * 40))
    result = F(lib, "tpw_conv_to_unicode", C.c_int32, C.c_uint32, C.c_uint32, C.c_void_p, C.c_void_p, C.c_char_p,
               C.c_uint32)(c["first"], c["count"], table, C.byref(out, 2), bytes(c["text"]) + b"\0", c["size"])
    return dict(result=result, out=tuple(out))


def convert_case(r):
    first = r.choice([0x81, 0x81, 0xe0, 0x20, 0xff])
    table = [r.randrange(1, 0x10000) for _ in range(600)]
    text = []
    for _ in range(r.randint(0, 12)):
        lead = r.choice([r.randrange(1, max(first, 2)), first, (first + 1) & 0xff or 1])
        text.append(lead or 1)
        if lead - 1 >= first:
            text.append(r.randrange(1, 256))
    # No embedded NUL, at most what the 40-word output can take.
    text = [b or 1 for b in text][:30]
    return dict(first=first, count=r.choice([600, 600, 0, 40, 255]), table=table, text=text,
                size=r.choice([len(text) + 5, len(text), max(len(text) - 1, 0), 1, 0, 2, 38]))


def convert_edges():
    r = random.Random(5)
    cases = []
    for size in (0, 1, 2, 3, 4, 5):   # three single-byte characters against every buffer size
        c = convert_case(r)
        c.update(first=0x81, count=600, text=[0x41, 0x42, 0x43], size=size)
        cases.append(c)
    c = convert_case(r)
    c.update(text=[], size=0)
    cases.append(c)
    c = convert_case(r)
    c.update(text=[], size=3)
    cases.append(c)
    return cases


def value_ppc(e, c):
    tree = e.alloc(2 * len(c["tree"]), struct.pack(">%dH" % len(c["tree"]), *c["tree"]))
    converter = e.alloc(12, struct.pack(">III", 0x1001142c, 1, tree))
    return dict(value=T.call_ppc(e, e.func("GetMBValue__14TbIUnicodeToMBFw"), [converter, c["char"]])[0] & 0xffff)


def value_host(lib, c):
    tree = (C.c_uint16 * len(c["tree"]))(*c["tree"])
    return dict(value=F(lib, "tpw_conv_mb_value", C.c_uint32, C.c_void_p, C.c_uint32)(tree, c["char"]) & 0xffff)


def value_case(r):
    # Two ranges: a root with a lower child (offsets are in bytes from the node).
    low, high = sorted(r.sample(range(0x40, 0x60), 2))
    root = [low, 0, high, 0] + [r.randrange(1, 0x10000) for _ in range(high - low + 1)]
    child = [0x20, 0, 0x2f, 0] + [r.randrange(1, 0x10000) for _ in range(16)]
    root[1] = 2 * len(root)
    return dict(tree=root + child, char=r.choice([low, high, low - 1, high + 1, 0x20, 0x2f, 0x1f, 0x30, r.randrange(0x80)]))


# ---- the string table and font loaders ------------------------------------------------------
TABLE_FACTORY = "CreateInterface__12TableFactoryCFRQ33bfl10FileSystem5CFilePPvUlP16TbIResourceIndex"
FONT_FACTORY = "CreateInterface__18RLE4bitFontFactoryCFRQ33bfl10FileSystem5CFilePPvUlP16TbIResourceIndex"
TABLE_VT, FONT_VT, INDEX_VT = 0x100113cc, 0x10011334, 0x10011250
TABLE_RELEASE = 0x10005370
FONT = dict(count=0x100044b4, size=0x10004508, spacing=0x10004590, type=0x100045ec, caps=0x10004638,
            data=0x10004684, header=0x100046f8, mb=0x100048ec, index=0x1000442c)


def held_objects(e, first_lead):
    """Two converters an index item may already hold: their Release (slot 8) and AddRef
    (slot 0xc) only record; the first has a first lead byte for the font's GetCharIndex."""
    vt = e.alloc(0x20, b"\0" * 0x20)
    L.word(e, vt + 8, T.fake_function(e, lambda e: e.cl.rec("Release", (e.cl.lab(e.r(3)),))))
    L.word(e, vt + 12, T.fake_function(e, lambda e: e.cl.rec("AddRef", (e.cl.lab(e.r(3)),))))
    return [e.alloc(16, struct.pack(">IIBBHI", vt, 1, first_lead, 0, 0, 0)) for _ in range(2)]


def make_index(e, c, held):
    """A TbIResourceIndex in emulator memory with the case's items (12 bytes each)."""
    items = c["items"]
    raw = b"".join(struct.pack(">IHHI", ident, kind, 0, held[which] if which >= 0 else 0)
                   for ident, kind, which in items) or b"\0" * 12
    first = e.alloc(len(raw), raw)
    return e.alloc(0x18, struct.pack(">IIIIII", INDEX_VT, 1, first, first + 12 * (len(items) - 1), 0, 0))


def run_factory(e, c, label, held):
    e.cv_bytes, e.cv_pos = bytes(c["bytes"]), 0
    file = e.alloc(8, struct.pack(">II", e.cv_file_vt, 0))
    result = e.alloc(4, b"\xee" * 4)
    out = e.alloc(4, struct.pack(">I", UNTOUCHED))
    this = e.alloc(4, b"\0" * 4)
    index = make_index(e, c, held)
    e.cl.rec("File", (e.cl.newlab(file),))
    T.call_ppc(e, e.func(label), [result, this, file, out, c["id"], index])
    e.cl.rec("~File", (e.cl.lab(file),))
    return T.s32(e.u32(result)), e.u32(out)


def held_label(e, held, pointer):
    return 0 if pointer == 0 else held.index(pointer) + 1 if pointer in held else -1


def table_ppc(e, c):
    held = held_objects(e, 0x81)
    e.cl = CL.PpcLog({held[0]: 1, held[1]: 2}, c["queues"])
    saved = e.cv.activate(e)
    try:
        result, made = run_factory(e, c, TABLE_FACTORY, held)
        fields, offsets, firsts = (0, 0, 0, 0), (), ()
        kind = -1 if made == UNTOUCHED else 0
        if made not in (UNTOUCHED, 0):
            assert e.u32(made) == TABLE_VT
            kind = 4
            count, strings = e.u32(made + 8), e.u32(made + 12)
            fields = (T.s32(e.u32(made + 4)), count, held_label(e, held, e.u32(made + 16)),
                      held_label(e, held, e.u32(made + 20)))
            pointers = [e.u32(strings + 4 * i) for i in range(c["strings"])]
            offsets = tuple(p - strings for p in pointers)
            firsts = tuple(e.u32(p) for p in pointers)
            T.call_ppc(e, TABLE_RELEASE, [made])
        return dict(result=result, kind=kind, fields=fields, offsets=offsets, firsts=firsts, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def items_array(c):
    flat = [x for item in c["items"] for x in item]
    return (C.c_int32 * max(len(flat), 1))(*flat)


def table_host(lib, c):
    data = bytes(c["bytes"])
    out = (C.c_int32 * 6)()
    n = c["strings"]
    words = (C.c_uint32 * max(2 * n, 1))()
    held = [F(lib, "tpw_conv_held", C.c_void_p, C.c_int32)(i) for i in range(2)]
    CL.host_begin(lib, {1: held[0], 2: held[1]}, c["queues"])
    F(lib, "tpw_conv_table", None, C.c_char_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p,
      C.c_uint32)(data, len(data), c["id"], items_array(c), len(c["items"]), out, words, n)
    log = CL.host_end(lib)
    made = out[1] == 4
    return dict(result=out[0], kind=out[1], fields=(out[2], out[3], out[4], out[5]),
                offsets=tuple(words[:n]) if made else (), firsts=tuple(words[n:2 * n]) if made else (), log=log)


def random_items(r):
    """Index items in rising id order; ids 1 and 2 are the ones the loaders ask for."""
    items = []
    if r.random() < 0.85:
        items.append((1, r.choice([0, 0, 0, 1, 3]), r.choice([0, 0, -1])))
    if r.random() < 0.85:
        items.append((2, r.choice([1, 1, 1, 0, 4]), r.choice([1, 1, -1])))
    for ident in sorted(r.sample(range(3, 40), r.randint(0, 4))):
        items.append((ident, r.randrange(5), -1))
    return items or [(7, 3, -1)]


def table_string(r):
    """One string's words: literals, variables, formats and parameters, then the end word."""
    out = b""
    for _ in range(r.randint(0, 5)):
        op = r.randrange(1, 7)
        if op == 1:
            raw = r.randbytes(r.randint(0, 9))
            out += struct.pack("<I", (len(raw) << 8) | 1) + raw + bytes((-len(raw)) % 4)
        elif op == 3:
            n = r.randint(0, 2)
            out += struct.pack("<I", (n << 8) | 3) + r.randbytes(4 * n)
        else:
            out += struct.pack("<I", (r.getrandbits(16) << 8) | op)
    return out + struct.pack("<I", r.choice([0, 0, 0, 9]))


def table_case(r):
    ident = r.getrandbits(8)
    strings = [table_string(r) for _ in range(r.randint(0, 5))]
    offsets, position = [], 4 * len(strings)
    for text in strings:
        offsets.append(position)
        position += len(text)
    body = struct.pack("<%dI" % len(strings), *offsets) + b"".join(strings)
    magic = r.choice([0x54534642] * 8 + [0x54534643, r.getrandbits(32)])
    file_id = r.choice([ident] * 8 + [ident + 1])
    data = struct.pack("<III", magic, file_id, len(strings)) + body
    queues = {}
    fault = r.randrange(12)
    if fault == 0:
        queues["Read"] = [r.choice([11, 0, -1])]
    elif fault == 1 and body:
        queues["Read"] = [12, len(body) - 1]
    return dict(bytes=list(data), id=ident, items=random_items(r), queues=queues, strings=len(strings))


def table_edges():
    r = random.Random(77)
    cases = []
    c = table_case(r)     # the game's shape: both converters open in the index
    good = struct.pack("<III", 0x54534642, 5, 2) + struct.pack("<II", 8, 20) + \
        struct.pack("<I", (5 << 8) | 1) + b"Hello\0\0\0" + struct.pack("<I", 0) + struct.pack("<II", (3 << 8) | 2, 0)
    c.update(bytes=list(good), id=5, items=[(1, 0, 0), (2, 1, 1), (9, 3, -1)], queues={}, strings=2)
    cases.append(c)
    cases.append(dict(c, items=[(1, 0, -1), (2, 1, -1)]))          # the items exist but hold nothing
    cases.append(dict(c, items=[(1, 1, 0), (2, 0, 1)]))            # wrong types: neither is taken
    cases.append(dict(c, items=[(9, 3, -1)]))                       # no such items
    cases.append(dict(c, id=6, strings=0))                          # the file is another table's
    cases.append(dict(c, bytes=list(good[:12]) + [0] * 0, strings=0,
                      queues={}, id=5) | dict(bytes=list(struct.pack("<III", 0x54534642, 5, 0))))   # no strings
    return cases


def font_record(r):
    header = struct.pack("<HHIIIHH", r.getrandbits(16), r.getrandbits(16), r.getrandbits(32), r.getrandbits(32),
                         r.getrandbits(32), r.getrandbits(16), r.getrandbits(16)) + r.randbytes(2) + \
        struct.pack("<H", r.getrandbits(16))
    data = r.randbytes(4 * r.randint(0, 3))
    return header + data


def font_case(r):
    records = [font_record(r) for _ in range(r.randint(1, 6))]
    count = len(records)
    offsets, position = [], 8 + 4 * count
    for record in records:
        offsets.append(position)
        position += len(record)
    magic = r.choice([0x42463446] * 8 + [0x42463447, r.getrandbits(32)])
    data = struct.pack("<IBBH", magic, r.getrandbits(8), r.getrandbits(8), count) + \
        struct.pack("<%dI" % count, *offsets) + b"".join(records)
    queues = {}
    if r.randrange(10) == 0:
        queues["Read"] = [len(data) - r.choice([1, 4])]
    lead = r.choice([0x41, 0x80, 0x81, 0x82, 0xff])
    return dict(bytes=list(data), id=0, items=random_items(r), queues=queues, chars=count,
                text=[lead, r.randrange(1, 256), 0])


def font_ppc(e, c):
    held = held_objects(e, 0x81)
    e.cl = CL.PpcLog({held[0]: 1, held[1]: 2}, c["queues"])
    saved = e.cv.activate(e)
    try:
        result, made = run_factory(e, c, FONT_FACTORY, held)
        fields, chars = (0,) * 8, ()
        kind = -1 if made == UNTOUCHED else 0
        if made not in (UNTOUCHED, 0):
            assert e.u32(made) == FONT_VT
            kind = 5
            call = lambda name, *args: T.call_ppc(e, FONT[name], [made] + list(args))[0]
            data = e.u32(made + 8)
            mb = call("mb")
            index = advance = 0
            if mb:
                text = e.alloc(4, bytes(c["text"]) + b"\0")
                pointer = e.alloc(4, struct.pack(">I", text))
                index = T.s32(T.call_ppc(e, FONT["index"], [made, pointer])[0])
                advance = e.u32(pointer) - text
            fields = (T.s32(e.u32(made + 4)), call("count") & 0xffff, call("spacing") & 0xff, call("type"), call("caps"),
                      held_label(e, held, mb), index, advance)
            rows = []
            for i in range(c["chars"]):
                header = call("header", i)
                h = struct.unpack(">HHIIIHHHH", e.read(header, 24))
                size = e.alloc(8, b"\xee" * 8)
                T.call_ppc(e, FONT["size"], [size, made, i])
                rows.append(h + struct.unpack(">II", e.read(size, 8)) + (call("data", i) - data, header - data))
            chars = tuple(rows)
        return dict(result=result, kind=kind, fields=fields, chars=chars, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def font_host(lib, c):
    data = bytes(c["bytes"])
    out = (C.c_int32 * 10)()
    n = c["chars"]
    words = (C.c_uint32 * max(13 * n, 1))()
    held = [F(lib, "tpw_conv_held", C.c_void_p, C.c_int32)(i) for i in range(2)]
    CL.host_begin(lib, {1: held[0], 2: held[1]}, c["queues"])
    F(lib, "tpw_conv_font", None, C.c_char_p, C.c_uint32, C.c_void_p, C.c_int32, C.c_char_p, C.c_void_p, C.c_void_p,
      C.c_uint32)(data, len(data), items_array(c), len(c["items"]), bytes(c["text"]) + b"\0", out, words, n)
    log = CL.host_end(lib)
    made = out[1] == 5
    chars = tuple(tuple(words[13 * i:13 * i + 13]) for i in range(n)) if made else ()
    return dict(result=out[0], kind=out[1], fields=tuple(out[2:10]) if made else (0,) * 8, chars=chars, log=log)


def preopened_ppc(e, c):
    held = held_objects(e, 0x81)
    e.cl = CL.PpcLog({held[0]: 1, held[1]: 2}, {})
    saved = e.cv.activate(e)
    try:
        index = make_index(e, c, held)
        handle = e.u32(index + 8) + 12 * c["which"] if c["which"] >= 0 else 0
        result = e.alloc(4, b"\xee" * 4)
        out = e.alloc(4, struct.pack(">I", UNTOUCHED))
        T.call_ppc(e, 0x100039ac, [result, index, handle, out])
        got = e.u32(out)
        return dict(result=T.s32(e.u32(result)), resource=-1 if got == UNTOUCHED else held_label(e, held, got),
                    log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def preopened_host(lib, c):
    out = (C.c_int32 * 2)()
    held = [F(lib, "tpw_conv_held", C.c_void_p, C.c_int32)(i) for i in range(2)]
    CL.host_begin(lib, {1: held[0], 2: held[1]}, {})
    F(lib, "tpw_conv_preopened", None, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p)(
        items_array(c), len(c["items"]), c["which"], out)
    return dict(result=out[0], resource=out[1], log=CL.host_end(lib))


def preopened_case(r):
    items = random_items(r)
    return dict(items=items, which=r.choice([-1] + list(range(len(items))) * 3))


def swap32(e):
    """SamsUtilities::_USwapBlock32(words, count): the Mac's real swap, recorded."""
    p, n = e.r(3), T.s32(e.r(4))
    e.cl.rec("Swap32", (n,))
    for i in range(max(n, 0)):
        b = e.read(p + 4 * i, 4)
        e.write(p + 4 * i, b[::-1])


TESTS = [
    FnTest("MBToUnicodeFactory::CreateInterface", factory_ppc, factory_host, edge=factory_edges,
           rnd=lambda r: factory_case(r, 0)),
    FnTest("UnicodeToMBFactory::CreateInterface", factory_ppc, factory_host, rnd=lambda r: factory_case(r, 1)),
    FnTest("MBInfoFactory::CreateInterface", factory_ppc, factory_host, rnd=lambda r: factory_case(r, 2)),
    FnTest("TbIMBInfo::GetIndex", index_ppc, index_host, rnd=index_case),
    FnTest("TbIMBToUnicode::ConvertString", convert_ppc, convert_host, edge=convert_edges, rnd=convert_case),
    FnTest("TbIUnicodeToMB::GetMBValue", value_ppc, value_host, rnd=value_case),
    FnTest("TableFactory::CreateInterface and TbITable", table_ppc, table_host, edge=table_edges, rnd=table_case),
    FnTest("RLE4bitFontFactory::CreateInterface and TbIRLE4bitFont", font_ppc, font_host, rnd=font_case),
    FnTest("TbIResourceIndex::GetPreOpenedResource", preopened_ppc, preopened_host, rnd=preopened_case),
]
