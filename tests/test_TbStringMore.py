"""Bullfrog strings: original views, full buffers, fields and outside-callee order."""
import ctypes
import struct
import tpwtest as T

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_TbStringMore"

# The vtables were read from the relocated dump, including RTTI and adjustment words.
VT = [0x10072580, 0x10072380]
BASE_VT = [0x1007254c, 0x1007255c]
STATIC_VT = 0x100725d4
SPECS = [(268504568, 'TbStringBase<char>::SBCstrcmp 0x10010df8', 0, 35), (268504668, 'TbStringBase<char>::SBCstrncmp 0x10010e5c', 0, 36), (268504776, 'TbStringBase<char>::SBCstrstr 0x10010ec8', 0, 37), (268505080, 'TbStringBase<char>::SBCstrtoint 0x10010ff8', 0, 38), (268505172, 'TbStringBase<char>::SBCisdigit 0x10011054', 0, 39), (268505244, 'TbStringBase<char>::SBCvsprintf 0x1001109c', 0, 52), (268505352, 'TbStringBase<char>::SBCvsnprintf 0x10011108', 0, 53), (268505760, 'TbStringBase<wchar_t>::SBCstrcmp 0x100112a0', 1, 35), (268505860, 'TbStringBase<wchar_t>::SBCstrncmp 0x10011304', 1, 36), (268505968, 'TbStringBase<wchar_t>::SBCstrstr 0x10011370', 1, 37), (268506068, 'TbStringBase<wchar_t>::SBCstrcat 0x100113d4', 1, 15), (268506272, 'TbStringBase<wchar_t>::SBCstrtoint 0x100114a0', 1, 38), (268506364, 'TbStringBase<wchar_t>::SBCisdigit 0x100114fc', 1, 39), (268506456, 'TbStringBase<wchar_t>::SBCvsprintf 0x10011558', 1, 52), (268506564, 'TbStringBase<wchar_t>::SBCvsnprintf 0x100115c4', 1, 53), (268506788, 'TbStringBase<char>::operator== 0x100116a4', 0, 40), (268507236, 'TbStringBase<char>::operator!= 0x10011864', 0, 41), (268507568, 'TbStringBase<char>::operator!= 0x100119b0', 0, 42), (268507720, 'TbStringBase<char>::operator< 0x10011a48', 0, 43), (268507812, 'TbStringBase<char>::operator< 0x10011aa4', 0, 44), (268508016, 'TbStringBase<char>::operator> 0x10011b70', 0, 45), (268508160, 'TbStringBase<char>::operator> 0x10011c00', 0, 46), (268514568, 'TbStringBase<char>::operator> 0x10013508', 0, 47), (268508872, 'TbStringBase<wchar_t>::operator== 0x10011ec8', 1, 40), (268508968, 'TbStringBase<wchar_t>::operator== 0x10011f28', 1, 22), (268509172, 'TbStringBase<wchar_t>::operator== 0x10011ff4', 1, 23), (268509312, 'TbStringBase<wchar_t>::operator!= 0x10012080', 1, 41), (268509420, 'TbStringBase<wchar_t>::operator!= 0x100120ec', 1, 24), (268509636, 'TbStringBase<wchar_t>::operator!= 0x100121c4', 1, 42), (268509788, 'TbStringBase<wchar_t>::operator< 0x1001225c', 1, 43), (268509860, 'TbStringBase<wchar_t>::operator< 0x100122a4', 1, 44), (268510064, 'TbStringBase<wchar_t>::operator> 0x10012370', 1, 45), (268510176, 'TbStringBase<wchar_t>::operator> 0x100123e0', 1, 46), (268512356, 'TbStringBase<wchar_t>::operator> 0x10012c64', 1, 47), (268512584, 'TbStringBase<wchar_t>::operator< 0x10012d48', 1, 25), (268508420, 'TbStringBase<char>::FindReverse 0x10011d04', 0, 48), (268508704, 'TbStringBase<char>::ResizeTo 0x10011e20', 0, 49), (268510436, 'TbStringBase<wchar_t>::FindReverse 0x100124e4', 1, 48), (268510724, 'TbStringBase<wchar_t>::ResizeTo 0x10012604', 1, 49), (268510780, 'TbStringBase<wchar_t>::Mid 0x1001263c', 1, 18), (268510912, 'TbStringBase<wchar_t>::Mid 0x100126c0', 1, 19), (268511084, 'TbStringBase<wchar_t>::Right 0x1001276c', 1, 20), (268511204, 'TbStringBase<wchar_t>::Left 0x100127e4', 1, 21), (268511312, 'TbStringBase<wchar_t>::FindReverseOneOf 0x10012850', 1, 26), (268511544, 'TbStringBase<wchar_t>::FindOneOf 0x10012938', 1, 27), (268511756, 'TbStringBase<wchar_t>::FindReverse 0x10012a0c', 1, 28), (268511964, 'TbStringBase<wchar_t>::Find 0x10012adc', 1, 29), (268512152, 'TbStringBase<wchar_t>::Find 0x10012b98', 1, 30), (268515248, 'TbStringBase<char>::Format 0x100137b0', 0, 50), (268515328, 'TbStringBase<wchar_t>::Format 0x10013800', 1, 50), (268494864, 'TbDynamicStringTemplate<char>::Format 0x1000e810', 0, 54), (268496772, 'TbDynamicStringTemplate<wchar_t>::Format 0x1000ef84', 1, 54), (268503824, 'TbStaticStringTemplate<char>::Format 0x10010b10', 0, 56), (268496936, 'TbDynamicStringTemplate<wchar_t>::FormatV 0x1000f028', 1, 55), (268499644, 'TbDynamicStringTemplate<char>::FormatV 0x1000fabc', 0, 55), (268495572, 'TbDynamicStringTemplate<wchar_t>::operator= 0x1000ead4', 1, 5), (268496008, 'TbDynamicStringTemplate<wchar_t>::operator= 0x1000ec88', 1, 7), (268496408, 'TbDynamicStringTemplate<wchar_t>::operator+= 0x1000ee18', 1, 9), (268496588, 'TbDynamicStringTemplate<wchar_t>::operator+= 0x1000eecc', 1, 10), (268498452, 'TbDynamicStringTemplate<wchar_t>::operator= 0x1000f614', 1, 11), (268498856, 'TbDynamicStringTemplate<wchar_t>::TbDynamicStringTemplate 0x1000f7a8', 1, 3), (268499320, 'TbDynamicStringTemplate<wchar_t>::TbDynamicStringTemplate 0x1000f978', 1, 51), (268502036, 'TbDynamicStringTemplate<char>::TbDynamicStringTemplate 0x10010414', 0, 51)]


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
    install_more(e)
    e.tpw_tsm_events = []

    def alloc(width):
        def fake(em):
            obj, length = em.r(3), em.r(4)
            em.tpw_tsm_events.append((1, length, width))
            data = em.alloc(512 * width, pack_text([0x7e] * 512, width, "big"))
            capacity = (length // 8 + 1) * 8
            em.write(obj + 4, struct.pack(">4I", length, data, capacity, 8))
            em.write(data + length * width, b"\0" * width)
        return fake

    def resize(width, static=False):
        def fake(em):
            obj, length = em.r(3), em.r(4)
            em.tpw_tsm_events.append((3 if static else 2, length, width))
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
                result = LIBC.strncmp(bytes(a) + b'\0', bytes(b) + b'\0', n) if width == 1 else (aa > bb) - (aa < bb)
                em.set_r(3, T.u32(result))
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
        em.tpw_tsm_events.append((4, int(em.r(3) != 0), 0))
    e.stub("__dla__FPv", deleted)
    e.stub("__dl__FPv", lambda em: em.tpw_tsm_events.append((5, int(em.r(3) != 0), 0)))


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
        if self.op in (7, 9):
            for count in range(6):
                yield dict(a=[97, 98, 99], b=[120], n=0, count=count, null=False, self=True)
        if self.op == 38:
            for text in ['', '  +123tail', '\t-42', '-0', '2147483647', '-2147483648', '4294967295', '4294967296', 'x23', '+', '  - 12', '00127']:
                yield dict(a=[], b=list(text.encode()), n=0, count=0, null=False, self=False)
        if self.op == 39:
            for n in list(range(256)) + [256, 0x1234, 0xffff, 0x10030, 0xffffffff]:
                yield dict(a=[], b=[], n=n, count=0, null=False, self=False)
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
        if op in (0, 1, 2, 3, 51):
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
        e.tpw_tsm_events = []
        if op in (31, 32) and c["self"]:
            e.write(a, struct.pack(">I", BASE_VT[self.wide]))
        if op == 32:
            e.write(a + 12, struct.pack(">I", count % 32))
        if op == 51:
            args = [a, bdata]
        elif op == 0:
            args = [a]
        elif op in (1, 6, 10, 22, 24):
            args = [a, text]
        elif op in (2, 3, 5, 8, 11, 23, 25):
            args = [a, other]
        elif op in (7, 9, 28, 29):
            character = adata + (count % 32) * (self.wide + 1) if c["self"] and op in (7, 9) else bdata
            args = [a, character] + ([n] if op in (28, 29) else [])
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
        elif op in (35, 37):
            args = [a, adata, bdata]
        elif op == 36:
            args = [a, adata, bdata, n]
        elif op == 38:
            args = [a, bdata]
        elif op == 39:
            args = [a, (n & 0xffff) if self.wide else T.u32(T.s8(n))]
        elif op in (40, 41, 43, 45):
            args = [a, bdata]
        elif op in (42, 47):
            args = [a, other]
        elif op in (44, 46):
            args = [a, text]
        elif op == 48:
            args = [a, text, n]
        elif op == 49:
            args = [a, n]
        elif op == 50:
            args = [a, text, n, count]
        elif op in (31, 32):
            args = [a, n]
        elif op == 33:
            args = [a, adata, n, count]
        ret = e.call(self.address, args)
        if op in (5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 31):
            result = int(ret == (adata if op in (13, 14, 15, 16, 31) else a))
        elif op == 37:
            result = (ret - adata) // (self.wide + 1) if ret else -1
        elif op in (12, 17, 22, 23, 24, 25, 26, 27, 28, 29, 30, 35, 36, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48):
            result = T.s32(ret)
        elif op in (18, 19, 20, 21):
            result = T.s32(e.u32(out + 4))
            offset = (e.u32(out + 8) - adata) // (self.wide + 1)
        pfields = [result, T.s32(e.u32(a + 4)), T.s32(e.u32(a + 12)), T.s32(e.u32(a + 16)), offset, T.s32(e.u32(b + 4)), T.s32(e.u32(b + 12)), T.s32(e.u32(b + 16))]
        width = self.wide + 1
        pbytes = [] if op == 4 else list(struct.unpack(">" + ("512H" if self.wide else "512B"), e.read(e.u32(a + 8), 512 * width)))
        pbytes += list(struct.unpack(">" + ("512H" if self.wide else "512B"), e.read(bdata, 512 * width)))
        pevents = e.tpw_tsm_events[:]
        first = ctypes.create_string_buffer(pack_text(c["a"] + [0], width, "little"))
        second = ctypes.create_string_buffer(pack_text(c["b"] + [0], width, "little"))
        fields = (ctypes.c_int32 * 8)()
        raw = ctypes.create_string_buffer(1024 * width)
        fn = lib.tpw_tsm_string
        fn.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p, ctypes.c_uint32,
                       ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                       ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p]
        fn(self.wide, op, first, len(c["a"]), second, len(c["b"]), n, count,
           c["null"], c["self"], fields, raw)
        events = (ctypes.c_int32 * 1024)()
        lib.tpw_tsm_events.argtypes = [ctypes.c_void_p]
        total = lib.tpw_tsm_events(events)
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


TESTS = [StringTest(spec) for spec in SPECS if spec[3] < 52]


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
        e.tpw_tsm_events = []
        if self.op in (4, 5):
            e.call(self.address, [obj] + ([0xffffffff] if self.op == 5 else []))
        elif self.op == 0:
            e.call(self.address, [obj])
        elif self.op == 1:
            e.call(self.address, [out, value])
        elif self.op == 2:
            e.call(self.address, [obj, value])
        else:
            e.call(self.address, [out, obj])
        events = e.tpw_tsm_events[:]
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
        lib.tpw_tsm_list.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_char_p,
                                ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]
        lib.tpw_tsm_list(self.op, c["count"], c["a"].encode(), c["b"].encode(), hostfields, hostbytes)
        hostevents = (ctypes.c_int32 * 1024)()
        lib.tpw_tsm_events.argtypes = [ctypes.c_void_p]
        n = lib.tpw_tsm_events(hostevents)
        hostlog = [tuple(hostevents[i:i + 3]) for i in range(0, n, 3)]
        mismatches = []
        if fields != list(hostfields):
            mismatches.append(f"list fields PPC={fields}, host={list(hostfields)}")
        if bytes(contents) != hostbytes.raw:
            mismatches.append("list contents differ")
        if events != hostlog:
            mismatches.append(f"list calls PPC={events}, host={hostlog}")
        return mismatches


TESTS += [ListTest(0x1000a074, "string list destructor", 5),
          ListTest(0x1000a138, "string list RemoveAll", 4)]

# C library imports are modelled here, not replaced inside the string functions.
# Exact compare magnitudes use this host's libc; MSL itself is outside the loaded image.
LIBC = ctypes.CDLL(None)
LIBC.snprintf.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p]
LIBC.strcmp.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
LIBC.strncmp.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
LIBC.atoi.argtypes = [ctypes.c_char_p]


def c_number(spec, value, floating=False, unsigned=False):
    out = ctypes.create_string_buffer(4096)
    arg = ctypes.c_double(value) if floating else ctypes.c_uint32(value) if unsigned else ctypes.c_int32(value)
    LIBC.snprintf(out, ctypes.c_size_t(len(out)), spec.encode(), arg)
    return out.value.decode('latin1')


def atoi_units(values):
    import re
    s = ''.join(chr(x) if x < 128 else '?' for x in values)
    m = re.match(r'\s*([+-]?\d+)', s)
    return T.s32(int(m[1])) if m else 0


def formatted(e, fmt, va, width):
    # CodeWarrior's va_list is a pointer to consecutive big-endian argument words.
    import re
    text = ''.join(chr(x) for x in unpack_text(e, fmt, width))
    cursor = va

    def word():
        nonlocal cursor
        value = e.u32(cursor)
        cursor += 4
        return value

    out = ''
    at = 0
    pattern = re.compile(r'%([-+ #0]*)(\*|\d*)(?:\.(\*|\d*))?([hlNLF]?)([A-Za-z%])')
    while at < len(text):
        if text[at] != '%':
            out += text[at]
            at += 1
            continue
        if text[at:at + 2] == '%%':
            out += '%'
            at += 2
            continue
        m = pattern.match(text, at)
        if not m:
            raise ValueError(('bad test format', text, at))
        flags, w, precision, modifier, conv = m.groups()
        w = T.s32(word()) if w == '*' else int(w or 0)
        prec = T.s32(word()) if precision == '*' else int(precision or 0) if precision is not None else -1
        if w < 0:
            w = -w
            flags += '-'
        if conv in 'sScC':
            value = word()
            if conv in 'cC':
                body = chr(value & (0xffff if width == 2 else 0xff))
            elif value == 0:
                body = '(null)'
            else:
                units = unpack_text(e, value, 2 if modifier == 'l' or (conv == 'S' and modifier != 'h') else 1)
                body = ''.join(chr(x) for x in units)
            if conv in 'sS' and prec >= 0:
                body = body[:prec]
            padding = ' ' * max(0, w - len(body))
            out += body + padding if '-' in flags else padding + body
        else:
            floating = conv in 'eEfFgG'
            if floating:
                value = struct.unpack('>d', e.read(cursor, 8))[0]
                cursor += 8
            else:
                value = word()
                value = T.s32(value) if conv in 'di' else value
                if modifier == 'h':
                    value = T.s16(value) if conv in 'di' else T.u16(value)
            spec = '%' + flags + (str(w) if w else '') + ('.' + str(prec) if prec >= 0 else '') + conv
            out += c_number(spec, value, floating, conv in 'ouxX')
        at = m.end()
    return [ord(x) for x in out]


def install_more(e):
    # Import slots for the two classification tables must point to distinct arrays.
    ctype = T.persist_alloc(e, 256, bytes(0x10 if 48 <= i <= 57 else 0 for i in range(256)))
    wctype = T.persist_alloc(e, 512, pack_text([0x10 if 48 <= i <= 57 else 0 for i in range(256)], 2, 'big'))
    e.write(0x1006f0d0, struct.pack('>I', ctype))
    e.write(0x1006f0cc, struct.pack('>I', wctype))
    e.stub('strcmp', lambda em: em.set_r(3, T.u32(LIBC.strcmp(bytes(unpack_text(em, em.r(3), 1)), bytes(unpack_text(em, em.r(4), 1))))))

    def widecmp(em):
        a = unpack_text(em, em.r(3), 2) + [0]
        b = unpack_text(em, em.r(4), 2) + [0]
        result = next((x - y for x, y in zip(a, b) if x != y), 0)
        em.set_r(3, T.u32(result))
    e.stub('wcscmp', widecmp)
    e.stub('atoi', lambda em: em.set_r(3, T.u32(LIBC.atoi(bytes(unpack_text(em, em.r(3), 1))))))
    e.stub('_wtoi__10NS_MacDozeFPCw', lambda em: em.set_r(3, T.u32(atoi_units(unpack_text(em, em.r(3), 2)))))

    def formatter(width, unlimited=False):
        def fake(em):
            dst = em.r(3)
            size = 0x7fffffff if unlimited else em.r(4)
            fmt, va = (em.r(4), em.r(5)) if unlimited else (em.r(5), em.r(6))
            units = formatted(em, fmt, va, width)
            if size:
                em.write(dst, pack_text(units[:size - 1] + [0], width, 'big'))
            result = -1 if width == 2 and (size == 0 or len(units) >= size) else len(units)
            em.set_r(3, T.u32(result))
        return fake
    e.stub('vsprintf', formatter(1, True))
    e.stub('vsnprintf', formatter(1))
    e.stub('vswprintf', formatter(2))

    def newarray(em):
        size = em.r(3)
        em.tpw_tsm_events.append((6, size, 0))
        em.set_r(3, em.alloc(size))
    e.stub('__nwa__FUl', newarray)


class FormatTest:
    def __init__(self, spec):
        self.address, self.name, self.wide, self.op = spec

    def edge_cases(self):
        samples = [('', 0), ('ordinary %% text', 0), ('%d/%u', 0), ('%ld/%lx', 0),
                   ('%hd/%hu', 0), ('%#08x/%o', 0), ('%+12d/% 10i', 0),
                   ('%*d', 0), ('%*.*f', 1), ('%*.*e', 1), ('%*.*g', 1),
                   ('%*.*G', 1), ('%ls/%d', 3), ('%hs/%d', 3),
                   ('%*.*ls', 2), ('%*.*hs', 2), ('%c/%d', 0), ('%lc/%d', 0),
                   ('%hc/%d', 0), ('%d:%.3f:%ls:%d', 4)]
        for fmt, kind in samples:
            for x, y, word in [(0, 0, ''), (1, 5, 'abc'), (3, 9, 'longer text'), (7, 12, 'abcdefghi')]:
                # Character precision cannot limit an output width larger than it.
                if kind in (1, 2) and x > y:
                    continue
                yield dict(fmt=fmt, kind=kind, x=x, y=y, number=-12.375, text=word, size=64, null=False, textnull=False)
        for fmt, word in [('%s/%d', ''), ('%s/%d', 'a'), ('%.0s/%d', 'a'), ('%S/%d', ''), ('%lS/%d', 'abc')]:
            # No-modifier wide %s is deliberately one narrow byte, with enough NUL padding
            # for the estimator's 16-bit read and an output that fits its estimate.
            if '%S' in fmt and not self.wide:
                continue
            yield dict(fmt=fmt, kind=3, x=-17, y=0, number=0., text=word, size=64, null=False, textnull=False)
        yield dict(fmt='%300d', kind=0, x=-17, y=0, number=0., text='', size=64, null=False, textnull=False)
        for size in [0, 1, 2, 4, 8, 32]:
            yield dict(fmt='%ld/%u', kind=0, x=-2147483648, y=-1, number=0., text='', size=size, null=False, textnull=False)
        for x in [-2147483648, -32769, -1, 0, 127, 128, 32768, 2147483647]:
            yield dict(fmt='%ld:%hd', kind=0, x=x, y=x, number=0., text='', size=64, null=False, textnull=False)
        yield dict(fmt='%ls/%d', kind=3, x=42, y=0, number=0., text='', size=64, null=False, textnull=True)
        if self.op in (54, 56):
            yield dict(fmt='', kind=0, x=1, y=2, number=0., text='', size=64, null=True, textnull=False)

    def random_case(self, r):
        fmt, kind = r.choice([('%ld,%u', 0), ('%#x,%o', 0), ('%*.*f', 1), ('%*.*g', 1),
                              ('%*.*ls', 2), ('%ls:%d', 3), ('%d %.2f %ls %d', 4)])
        return dict(fmt=fmt, kind=kind, x=r.randrange(0, 12) if kind in (1, 2) else T.s32(r.getrandbits(32)),
                    y=r.randrange(12, 20), number=r.uniform(-1000, 1000),
                    text=''.join(r.choice('abcXYZ') for _ in range(r.randrange(20))), size=r.randrange(1, 90),
                    null=False, textnull=r.random() < .15)

    def run_case(self, runner, c):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        width = self.wide + 1
        a, data = seed(e, [], self.wide)
        if self.op == 56:
            e.write(a, struct.pack('>I', STATIC_VT))
            e.write(a + 12, struct.pack('>I', c['size']))
        fmt = 0 if c['null'] else e.alloc((len(c['fmt']) + 1) * width, pack_text([ord(x) for x in c['fmt']] + [0], width, 'big'))
        textwide = 'ls' in c['fmt'] or 'S' in c['fmt']
        textwidth = 2 if textwide else 1
        # For a wide plain %s, estimator and MSL disagree on pointer interpretation.
        # This suite uses explicit %hs and %ls, which agree about the pointed-to units.
        text = 0 if c['textnull'] else e.alloc((len(c['text']) + 4) * textwidth, pack_text([ord(x) for x in c['text']] + [0, 0, 0, 0], textwidth, 'big'))
        x, y = T.u32(c['x']), T.u32(c['y'])
        doublewords = list(struct.unpack('>2I', struct.pack('>d', c['number'])))
        words = [[x, y], [x, y] + doublewords, [x, y, text], [text, x], [x] + doublewords + [text, y]][c['kind']]
        va = e.alloc(len(words) * 4, struct.pack('>' + 'I' * len(words), *words))
        e.tpw_tsm_events = []
        if self.op in (54, 56):
            args = [a, fmt] + words
        elif self.op == 55:
            args = [a, fmt, va]
        elif self.op == 52:
            args = [a, data, fmt, va]
        else:
            args = [a, data, c['size'], fmt, va]
        result = e.call(self.address, args)
        pfields = [T.s32(result) if self.op in (52, 53) else 0] + [T.s32(e.u32(a + k)) for k in (4, 12, 16)]
        pbytes = list(struct.unpack('>' + ('512H' if self.wide else '512B'), e.read(e.u32(a + 8), 512 * width)))
        pevents = e.tpw_tsm_events[:]
        hfmt = None if c['null'] else ctypes.create_string_buffer(pack_text([ord(x) for x in c['fmt']] + [0], width, 'little'))
        htext = None if c['textnull'] else ctypes.create_string_buffer(pack_text([ord(x) for x in c['text']] + [0, 0, 0, 0], textwidth, 'little'))
        fields = (ctypes.c_int32 * 4)()
        raw = ctypes.create_string_buffer(512 * width)
        lib.tpw_tsm_format.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32,
                                      ctypes.c_int32, ctypes.c_int32, ctypes.c_double, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
        lib.tpw_tsm_format(self.wide, self.op, hfmt, c['kind'], c['size'], c['x'], c['y'], c['number'], htext, fields, raw)
        events = (ctypes.c_int32 * 1024)()
        lib.tpw_tsm_events.argtypes = [ctypes.c_void_p]
        n = lib.tpw_tsm_events(events)
        hevents = [tuple(events[i:i + 3]) for i in range(0, n, 3)]
        hbytes = list(struct.unpack('<' + ('512H' if self.wide else '512B'), raw.raw))
        mismatches = []
        if pfields != list(fields):
            mismatches.append(f'format fields PPC={pfields}, host={list(fields)}')
        if pbytes != hbytes:
            pos = next(i for i in range(512) if pbytes[i] != hbytes[i])
            mismatches.append(f'format buffer[{pos}] PPC={pbytes[pos]}, host={hbytes[pos]}')
        if pevents != hevents:
            mismatches.append(f'format calls PPC={pevents}, host={hevents}')
        return mismatches


TESTS += [FormatTest(spec) for spec in SPECS if spec[3] >= 52]
