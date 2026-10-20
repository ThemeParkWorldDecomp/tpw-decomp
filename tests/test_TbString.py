"""Bullfrog strings: original views, full buffers, fields and outside-callee order."""
import ctypes
import struct
import tpwtest as T

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbString"

# The vtables were read from the relocated dump, including RTTI and adjustment words.
VT = [0x10072580, 0x10072380]
BASE_VT = [0x1007254c, 0x1007255c]
STATIC_VT = 0x100725d4
SPECS = [
    (0x1000e2c8, "char destructor", 0, 4),
    (0x1000e364, "char assign base", 0, 5),
    (0x1000e44c, "char assign C string", 0, 6),
    (0x1000e518, "char assign character", 0, 7),
    (0x1000e5d8, "char append base", 0, 8),
    (0x1000e6a8, "char append character", 0, 9),
    (0x1000e758, "char append C string", 0, 10),
    (0x1000ea38, "UTF16 destructor", 1, 4),
    (0x1000ebbc, "UTF16 assign C string", 1, 6),
    (0x1000ed48, "UTF16 append base", 1, 8),
    (0x1000f6bc, "UTF16 copy constructor", 1, 2),
    (0x1000f88c, "UTF16 C string constructor", 1, 1),
    (0x1000fa28, "UTF16 default constructor", 1, 0),
    (0x100100b8, "char assign dynamic", 0, 11),
    (0x10010160, "char copy constructor", 0, 2),
    (0x10010248, "char base constructor", 0, 3),
    (0x10010328, "char C string constructor", 0, 1),
    (0x100104c4, "char default constructor", 0, 0),
    (0x100105d4, "char static destructor", 0, 34),
    (0x10010bd4, "char static constructor", 0, 33),
    (0x10010cd0, "char SBCstrlen", 0, 12),
    (0x10010d2c, "char SBCstrcpy", 0, 13),
    (0x10010d90, "char SBCstrncpy", 0, 14),
    (0x10010f2c, "char SBCstrcat", 0, 15),
    (0x10010f90, "char SBCstrncat", 0, 16),
    (0x10011178, "UTF16 SBCstrlen", 1, 12),
    (0x100111d4, "UTF16 SBCstrcpy", 1, 13),
    (0x10011238, "UTF16 SBCstrncpy", 1, 14),
    (0x10011438, "UTF16 SBCstrncat", 1, 16),
    (0x10011634, "char LockBuffer", 0, 31),
    (0x1001170c, "char equal C string", 0, 22),
    (0x100117d8, "char equal base", 0, 23),
    (0x100118d8, "char unequal C string", 0, 24),
    (0x10011ccc, "char Length", 0, 17),
    (0x100124ac, "UTF16 Length", 1, 17),
    (0x10012f10, "char Mid", 0, 18),
    (0x10012f90, "char Mid count", 0, 19),
    (0x10013038, "char Right", 0, 20),
    (0x100130ac, "char Left", 0, 21),
    (0x10013118, "char FindReverseOneOf", 0, 26),
    (0x100131f8, "char FindOneOf", 0, 27),
    (0x100132c8, "char FindReverse character", 0, 28),
    (0x10013390, "char Find character", 0, 29),
    (0x10013448, "char Find string", 0, 30),
    (0x100135ec, "char less base", 0, 25),
    (0x100136d0, "char ReleaseBuffer", 0, 32),
]


def unpack_text(e, addr, width):
    # C runtime fakes read until the first NUL, including UTF-16 code units.
    out = []
    while True:
        value = int.from_bytes(e.read(addr, width), "big")
        if value == 0:
            return out
        out.append(value)
        addr += width


def pack_text(values, width, endian):
    return b"".join(v.to_bytes(width, endian) for v in values)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.u11_events = []

    def alloc(width):
        def fake(em):
            obj, length = em.r(3), em.r(4)
            em.u11_events.append((1, length, width))
            data = em.alloc(512 * width, pack_text([0x7e] * 512, width, "big"))
            capacity = (length // 8 + 1) * 8
            em.write(obj + 4, struct.pack(">4I", length, data, capacity, 8))
            em.write(data + length * width, b"\0" * width)
        return fake

    def resize(width, static=False):
        def fake(em):
            obj, length = em.r(3), em.r(4)
            em.u11_events.append((3 if static else 2, length, width))
            if static:
                capacity = em.u32(obj + 12)
                truncated = int(length >= capacity)
                length = capacity - 1 if truncated else length
                em.write(obj + 16, struct.pack(">I", truncated))
            em.write(obj + 4, struct.pack(">I", T.u32(length)))
            em.write(em.u32(obj + 8) + length * width, b"\0" * width)
        return fake

    for address, width in [(0x1000e150, 1), (0x1000e8b4, 2)]:
        T.stub_internal(e, address, alloc(width))
    for address, width in [(0x1000e200, 1), (0x1000e96c, 2)]:
        T.stub_internal(e, address, resize(width))
    T.stub_internal(e, 0x10010558, resize(1, True))

    def runtime(width, operation):
        def fake(em):
            dst, src, n = em.r(3), em.r(4), em.r(5)
            if operation == "len":
                em.set_r(3, len(unpack_text(em, dst, width)))
                return
            a = unpack_text(em, dst, width)
            b = unpack_text(em, src, width)
            if operation == "cmp":
                aa, bb = a[:n], b[:n]
                em.set_r(3, T.u32((aa > bb) - (aa < bb)))
                return
            if operation == "find":
                pos = next((i for i in range(len(a) + 1) if a[i:i + len(b)] == b), -1)
                em.set_r(3, dst + pos * width if pos >= 0 else 0)
                return
            if operation == "copy":
                values = b + [0]
                target = dst
            elif operation == "ncpy":
                values = b[:n] + [0] * max(0, n - len(b))
                target = dst
            elif operation == "cat":
                values = b + [0]
                target = dst + len(a) * width
            else:
                values = b[:n] + [0]
                target = dst + len(a) * width
            em.write(target, pack_text(values, width, "big"))
            em.set_r(3, dst)
        return fake

    for prefix, width in [("str", 1), ("wcs", 2)]:
        for suffix, operation in [("len", "len"), ("cpy", "copy"), ("ncpy", "ncpy"),
                                  ("cat", "cat"), ("ncat", "ncat"), ("ncmp", "cmp"),
                                  ("str", "find")]:
            e.stub(prefix + suffix, runtime(width, operation))

    def deleted(em):
        em.u11_events.append((4, int(em.r(3) != 0), 0))
    e.stub("__dla__FPv", deleted)
    e.stub("__dl__FPv", lambda em: em.u11_events.append((5, int(em.r(3) != 0), 0)))


def seed(e, values, wide):
    width = wide + 1
    raw = values + [0] + [0x7e] * (511 - len(values))
    data = e.alloc(513 * width, pack_text([0x7e] + raw, width, "big")) + width
    obj = e.alloc(20, struct.pack(">5I", VT[wide], len(values), data, 400, 19))
    return obj, data


class StringTest:
    def __init__(self, spec):
        self.address, self.name, self.wide, self.op = spec

    def edge_cases(self):
        for a, b in [([], []), ([97], [97]), ([97, 98, 97], [98]),
                     ([97, 98], [97, 98, 99]), ([97, 98, 99], [97, 98]),
                     ([0, 98, 99], [0, 98, 100]), ([255, 97], [128, 97]),
                     ([97, 98, 97], []), ([97], [98])]:
            for n in [0, 1, 2, 3, 7, 24, 0x7fffffff, 0xffffffff]:
                for null in [False, True]:
                    yield dict(a=a, b=b, n=n, count=n, null=null, self=False)
        yield dict(a=[97, 98], b=[97, 98], n=1, count=0, null=False, self=True)

    def random_case(self, r):
        alphabet = [0, 65, 97, 98, 99, 128, 255]
        if self.wide:
            alphabet += [0x1234, 0x20ac, 0xd800, 0xffff]
        a = [r.choice(alphabet) for _ in range(r.randrange(25))]
        b = [r.choice(alphabet) for _ in range(r.randrange(25))]
        if r.random() < 0.25:
            b = a[:]
        return dict(a=a, b=b, n=r.choice([r.randrange(40), 0x7fffffff, 0xffffffff]),
                    count=r.choice([r.randrange(40), 0xffffffff]), null=r.random() < 0.15,
                    self=r.random() < 0.15)

    def run_case(self, runner, case):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        c = dict(case)
        op = self.op
        # Resizes and C copies stay within the allocated buffer; unsigned extremes
        # belong to searches, views and ReleaseBuffer's special sentinel.
        if op in (14, 16, 31, 32, 33):
            c["n"] = c["n"] % 64 if c["n"] != 0x7fffffff or op != 32 else c["n"]
        if op in (0, 1, 2, 3):
            c["self"] = False
        if op in (8, 10):
            c["self"] = False  # Overlapping strcat is undefined in the C runtime.
        a, adata = seed(e, c["a"], self.wide)
        b, bdata = seed(e, c["b"], self.wide)
        other = a if c["self"] else b
        text = 0 if c["null"] else bdata
        n, count = c["n"], c["count"]
        result, offset = 0, 0
        out = e.alloc(20, b"\x55" * 20)
        e.u11_events = []
        if op in (31, 32) and c["self"]:
            e.write(a, struct.pack(">I", BASE_VT[self.wide]))
        if op == 32:
            e.write(a + 12, struct.pack(">I", count % 32))
        if op == 0:
            args = [a]
        elif op in (1, 6, 10, 22, 24):
            args = [a, text]
        elif op in (2, 3, 5, 8, 11, 23, 25):
            args = [a, other]
        elif op in (7, 9, 28, 29):
            args = [a, bdata] + ([n] if op in (28, 29) else [])
        elif op in (4, 34):
            args = [a, 1 if count == 1 else 0xffffffff]
            if op == 34:
                e.write(a, struct.pack(">I", STATIC_VT))
        elif op == 12:
            args = [a, bdata]
        elif op in (13, 15):
            args = [a, adata, bdata]
        elif op in (14, 16):
            args = [a, adata, bdata, n]
        elif op == 17:
            args = [a]
        elif op in (18, 20, 21):
            args = [out, a, n]
        elif op == 19:
            args = [out, a, n, count]
        elif op in (26, 27, 30):
            args = [a, text, n]
        elif op in (31, 32):
            args = [a, n]
        elif op == 33:
            args = [a, adata, n, count]
        ret = e.call(self.address, args)
        if op in (5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 31):
            result = int(ret == (adata if op in (13, 14, 15, 16, 31) else a))
        elif op in (12, 17, 22, 23, 24, 25, 26, 27, 28, 29, 30):
            result = T.s32(ret)
        elif op in (18, 19, 20, 21):
            result = T.s32(e.u32(out + 4))
            offset = e.u32(out + 8) - adata
        pfields = [result, T.s32(e.u32(a + 4)), T.s32(e.u32(a + 12)), T.s32(e.u32(a + 16)), offset, T.s32(e.u32(b + 4)), T.s32(e.u32(b + 12)), T.s32(e.u32(b + 16))]
        width = self.wide + 1
        pbytes = [] if op == 4 else list(struct.unpack(">" + ("512H" if self.wide else "512B"), e.read(e.u32(a + 8), 512 * width)))
        pbytes += list(struct.unpack(">" + ("512H" if self.wide else "512B"), e.read(bdata, 512 * width)))
        pevents = e.u11_events[:]
        first = ctypes.create_string_buffer(pack_text(c["a"] + [0], width, "little"))
        second = ctypes.create_string_buffer(pack_text(c["b"] + [0], width, "little"))
        fields = (ctypes.c_int32 * 8)()
        raw = ctypes.create_string_buffer(1024 * width)
        fn = lib.u11_string
        fn.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32,
                       ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                       ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p]
        fn(self.wide, op, first, len(c["a"]), second, len(c["b"]), n, count,
           c["null"], c["self"], fields, raw)
        events = (ctypes.c_int32 * 1024)()
        lib.u11_events.argtypes = [ctypes.c_void_p]
        total = lib.u11_events(events)
        hevents = [tuple(events[i:i + 3]) for i in range(0, total, 3)]
        hbytes = list(struct.unpack("<" + ("1024H" if self.wide else "1024B"), raw.raw))
        if op == 4:
            hbytes = hbytes[512:]
        mismatches = []
        if pfields != list(fields):
            mismatches.append(f"fields PPC={pfields}, host={list(fields)}")
        if pbytes != hbytes:
            at = next(i for i, (p, h) in enumerate(zip(pbytes, hbytes)) if p != h)
            mismatches.append(f"buffer[{at}] PPC={pbytes[at]}, host={hbytes[at]}")
        if pevents != hevents:
            mismatches.append(f"calls PPC={pevents}, host={hevents}")
        return mismatches


TESTS = [StringTest(spec) for spec in SPECS]


class ListTest:
    def __init__(self, address, name, op):
        self.address = address
        self.name = name
        self.op = op

    def edge_cases(self):
        return [dict(count=n, a=a, b=b) for n in range(6)
                for a, b in [("", ""), ("old", "new"), ("same", "same")]]

    def random_case(self, r):
        return dict(count=r.randrange(6),
                    a="".join(r.choice("abZ:") for _ in range(r.randrange(32))),
                    b="".join(r.choice("abZ:") for _ in range(r.randrange(32))))

    def run_case(self, runner, c):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        nodes = []
        for _ in range(c["count"]):
            obj, data = seed(e, list(c["a"].encode()), 0)
            node = e.alloc(28, e.read(obj, 20) + b"\0" * 8)
            nodes.append(node)
        for i, node in enumerate(nodes):
            e.write(node + 20, struct.pack(">2I", nodes[i + 1] if i + 1 < len(nodes) else 0,
                                          nodes[i - 1] if i else 0))
        obj = e.alloc(16, struct.pack(">4I", 0x10071d38, nodes[0] if nodes else 0,
                                      nodes[-1] if nodes else 0, len(nodes)))
        value, _ = seed(e, list(c["b"].encode()), 0)
        out = e.alloc(28, b"\x55" * 28)
        e.u11_events = []
        if self.op == 0:
            e.call(self.address, [obj])
        elif self.op == 1:
            e.call(self.address, [out, value])
        elif self.op == 2:
            e.call(self.address, [obj, value])
        else:
            e.call(self.address, [out, obj])
        events = e.u11_events[:]
        head, tail, count = [e.u32(obj + i) for i in [4, 8, 12]]
        fields = [count, int(head == 0), int(tail == 0),
                  int(not head or e.u32(head + 24) == 0),
                  int(not tail or e.u32(tail + 20) == 0), 0, 0,
                  e.u32(out + 4) if self.op == 3 else 0,
                  e.u32(out + 4) if self.op == 1 else 0,
                  int(e.u32(out + 20) == e.u32(out + 24) == 0) if self.op == 1 else 1]
        contents = bytearray(1024)
        if self.op in (1, 3):
            offset = 128 if self.op == 1 else 0
            raw = e.read(e.u32(out + 8), e.u32(out + 4))
            contents[offset:offset + len(raw)] = raw
        node = head
        while node:
            raw = e.read(e.u32(node + 8), e.u32(node + 4))
            offset = 256 + 64 * fields[5]
            contents[offset:offset + len(raw)] = raw
            fields[5] += 1
            nxt = e.u32(node + 20)
            if nxt and e.u32(nxt + 24) != node:
                fields[3] = 0
            node = nxt
        node = tail
        while node:
            raw = e.read(e.u32(node + 8), e.u32(node + 4))
            offset = 640 + 64 * fields[6]
            contents[offset:offset + len(raw)] = raw
            fields[6] += 1
            prev = e.u32(node + 24)
            if prev and e.u32(prev + 20) != node:
                fields[4] = 0
            node = prev
        hostfields = (ctypes.c_int32 * 10)()
        hostbytes = ctypes.create_string_buffer(1024)
        lib.u11_list.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p,
                                ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]
        lib.u11_list(self.op, c["count"], c["a"].encode(), c["b"].encode(), hostfields, hostbytes)
        hostevents = (ctypes.c_int32 * 1024)()
        lib.u11_events.argtypes = [ctypes.c_void_p]
        n = lib.u11_events(hostevents)
        hostlog = [tuple(hostevents[i:i + 3]) for i in range(0, n, 3)]
        mismatches = []
        if fields != list(hostfields):
            mismatches.append(f"list fields PPC={fields}, host={list(hostfields)}")
        if bytes(contents) != hostbytes.raw:
            mismatches.append("list contents differ")
        if events != hostlog:
            mismatches.append(f"list calls PPC={events}, host={hostlog}")
        return mismatches


TESTS += [ListTest(0x10009ff8, "string list constructor", 0),
          ListTest(0x10003d94, "string list node constructor", 1),
          ListTest(0x10003c78, "string list AddTail", 2),
          ListTest(0x10013ca0, "string list RemoveHead", 3)]
