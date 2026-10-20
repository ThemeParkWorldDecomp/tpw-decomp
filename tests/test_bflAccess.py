"""Chained file access: physical calls, logical positions, cache bytes and decoder output."""
import ctypes as C
import itertools
import random
import struct
import tpwtest as T
import calllog as CL

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_bflAccess"
VT = [0x10071da4, 0x1007194c, 0x10071ab8, 0x10071bac, 0x10071c48, 0x10071b38]
SIZES = [12, 40, 16, 16, 28, 24]
DECODER_INITIAL = bytes((i * 13 + 29) & 255 for i in range(131072 + 4096))
# Type, function address, operation, original name.
SPECS = []
for kind, entries in enumerate([
    [(0xa818, 19, "constructor"), (0xa86c, 12, "destructor"), (0xa958, 5, "Close"),
     (0xac38, 6, "AttachAccessClass"), (0xacf8, 7, "MapFile"), (0xad68, 8, "UnmapFile"),
     (0xaddc, 11, "GetSemaphore"), (0xae50, 9, "Flush"), (0xae98, 10, "GetMacFileReference")],
    [(0x3e60, 19, "constructor"), (0x3efc, 13, "Open"), (0x3fc4, 5, "Close"),
     (0x402c, 0, "Read"), (0x412c, 2, "Seek"), (0x4250, 1, "Write"), (0x43c0, 4, "GetPos"),
     (0x441c, 3, "GetSize"), (0x44a0, 6, "AttachAccessClass"), (0x455c, 16, "ReadDirectToOutputBuffer"),
     (0x45f0, 17, "CacheBufferToOutputBuffer"), (0x46f8, 18, "RefillCacheBuffer"),
     (0x4798, 15, "BasicRead"), (0x4890, 12, "destructor")],
    [(0x67e8, 19, "constructor"), (0x686c, 5, "Close"), (0x68bc, 0, "Read"),
     (0x6948, 1, "Write"), (0x69d8, 2, "Seek"), (0x6a6c, 3, "GetSize"),
     (0x6ae0, 4, "GetPos"), (0x6b2c, 12, "destructor")],
    [(0x7360, 19, "constructor"), (0x73e4, 0, "Read"), (0x74d8, 1, "Write"),
     (0x75cc, 2, "Seek"), (0x76cc, 3, "GetSize"), (0x77a0, 4, "GetPos"),
     (0x77e8, 14, "ValidateFilePosition"), (0x7898, 6, "AttachAccessClass"), (0x794c, 12, "destructor")],
    [(0x8114, 19, "constructor"), (0x81a4, 13, "Open"), (0x8204, 5, "Close"),
     (0x827c, 1, "Write"), (0x82c8, 0, "Read"), (0x839c, 2, "Seek"),
     (0x84ec, 4, "GetPos"), (0x8538, 3, "GetSize"), (0x8584, 7, "MapFile"), (0x8620, 12, "destructor")],
    [(0x6bd4, 19, "constructor"), (0x6c64, 13, "Open"), (0x6d00, 1, "Write"),
     (0x6d50, 0, "Read"), (0x6e24, 2, "Seek"), (0x6f48, 4, "GetPos"),
     (0x6fa0, 3, "GetSize"), (0x6ff0, 5, "Close"), (0x707c, 6, "AttachAccessClass"), (0x72b4, 12, "destructor")],
]):
    SPECS.extend((kind, 0x10000000 + address, op, name) for address, op, name in entries)


def w32(e, p, value):
    e.write(p, struct.pack(">I", T.u32(value)))


def F(lib, name, result, *args):
    f = lib[name]
    f.restype, f.argtypes = result, args
    return f


def packet(data):
    """Literal-only RefPack stream, including every legal final literal count."""
    result = bytearray(b"\x10\xfb" + len(data).to_bytes(3, "big"))
    while len(data) >= 4:
        n = min(112, len(data) & ~3)
        result.append(0xe0 + n // 4 - 1)
        result.extend(data[:n])
        data = data[n:]
    result.append(0xfc + len(data))
    result.extend(data)
    return bytes(result)


def setup(runner):
    T.install_std_stubs(runner.emu)


def stubs(e, c, kind, op):
    s = CL.Stubs()
    s.real.update(address for _, address, _, _ in SPECS)
    s.real.update((0x1000a8f4, 0x1000a998, 0x1000dbb4, 0x100071a0, 0x1000722c))
    rec, ret, lab = e.cl.rec, e.cl.ret, e.cl.lab

    def span(p):
        if e.ba_io <= p < e.ba_io + 1024:
            return (4, p - e.ba_io)
        buffer = e.u32(e.ba_p + (12 if kind == 1 else 16)) if kind in (1, 5) else e.ba_buffer
        if buffer and buffer <= p < buffer + 1024:
            return (lab(buffer), p - buffer)
        return (lab(p), 0)

    def scalar(fn):
        return lambda e: e.set_r(3, T.u32(fn(e)))

    def read(e):
        p, dest, size = e.r(3), e.r(4), e.r(5)
        rec("Backend.Read", (lab(p), *span(dest), size))
        count = ret("Backend.Read", size)
        if kind == 5:
            e.write(dest, c["packed"][:size])
        else:
            done = min(size, max(0, count))
            e.write(dest, bytes((i * 7 + 3 + e.u32(p + 12)) & 255 for i in range(done)))
        w32(e, p + 12, e.u32(p + 12) + count)
        return count

    def write(e):
        p, src, size = e.r(3), e.r(4), e.r(5)
        checksum = 0
        for byte in e.read(src, size):
            checksum = T.u32(checksum * 31 + byte)
        rec("Backend.Write", (lab(p), *span(src), size, checksum))
        count = ret("Backend.Write", size)
        w32(e, p + 12, e.u32(p + 12) + count)
        return count

    def seek(e):
        rec("Backend.Seek", (lab(e.r(3)), T.s32(e.r(4)), e.r(5)))
        value = ret("Backend.Seek", e.r(4))
        w32(e, e.r(3) + 12, value)
        return value

    def mapped(e):
        rec("Backend.MapFile", (lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))
        return e.ba_io if ret("Backend.MapFile", 1) else 0

    def attach(e):
        rec("Backend.AttachAccessClass", (lab(e.r(3)), lab(e.r(4))))
        w32(e, e.r(3) + 4, e.r(4))

    callbacks = {
        12: scalar(read), 16: scalar(write), 20: scalar(seek),
        24: scalar(lambda e: (rec("Backend.GetSize", (lab(e.r(3)),)),
                             ret("Backend.GetSize", len(c["packed"]) if kind == 5 else 128))[1]),
        28: scalar(lambda e: (rec("Backend.GetPos", (lab(e.r(3)),)),
                             ret("Backend.GetPos", e.u32(e.r(3) + 12)))[1]),
        32: scalar(mapped),
        36: lambda e: rec("Backend.UnmapFile", (lab(e.r(3)), lab(e.r(4)))),
        44: scalar(lambda e: (rec("Backend.GetMacFileReference", (lab(e.r(3)),)),
                             ret("Backend.GetMacFileReference", 17))[1]),
        52: attach,
        56: scalar(lambda e: (rec("Backend.GetSemaphore", (lab(e.r(3)),)), e.ba_sem)[1]),
    }
    vt = e.alloc(60, b"\0" * 60)
    for offset, fn in callbacks.items():
        entry = e.alloc(4, b"\x4e\x80\x00\x20")
        vector = e.alloc(8, struct.pack(">II", entry, e.toc))
        w32(e, vt + offset, vector)
        s.internal(entry, fn)
    w32(e, e.ba_backend, vt)

    def allocated(e):
        size = e.r(3)
        failed = ret("array failure") if op == 13 else 0
        p = 0 if failed else e.alloc(max(1, size), b"\x5a" * max(1, size))
        rec("new[]", (size, e.cl.newlab(p) if p else 0))
        e.set_r(3, p)

    def deleted(e):
        p = e.r(3)
        if lab(p) > 0:
            rec("delete", (lab(p),))
        e.cl.forget(p)

    def atomic(step):
        def fn(e):
            p, old = e.r(3), T.s32(e.u32(e.r(3)))
            rec("InterlockedIncrement" if step == 1 else "InterlockedDecrement", (lab(p), old))
            w32(e, p, old + step)
            e.set_r(3, T.u32(old + step))
        return fn

    def copy(e):
        dest, src, size = e.r(3), e.r(4), e.r(5)
        rec("CopyMemory", (*span(dest), *span(src), size))
        e.write(dest, e.read(src, size))

    s.imp("__nwa__FUl", allocated)
    s.imp("__dla__FPv", deleted)
    s.imp("__dl__FPv", deleted)
    for info in e.imports.values():
        name = info["name"]
        base = name.split("__")[0]
        if "NS_MacDoze" in name and base in ("InterlockedIncrement", "InterlockedDecrement", "CopyMemory"):
            s.imp(name, copy if base == "CopyMemory" else atomic(1 if base == "InterlockedIncrement" else -1))
    s.internal(0x10024b6c, scalar(lambda e: (rec("SemaLock.Lock", (*struct.unpack(">3I", e.read(e.r(3), 12)), T.s32(e.r(4)))),
                                           ret("SemaLock.Lock", 1))[1]))
    s.internal(0x10024b00, lambda e: rec("SemaLock.dtor", struct.unpack(">3I", e.read(e.r(3), 12))))
    return s


class AccessTest:
    def __init__(self, spec):
        self.kind, self.addr, self.op, name = spec
        self.exact_nan = False
        self.name = ["CAccessItem", "_TbBufferAccess", "_TbQuickPosAccess", "_TbSharedAccess",
                     "_TbWadFileAccess", "_TbRefpackFileAccess"][self.kind] + "::" + name

    def edge_cases(self):
        return list(itertools.islice(self.cases(random.Random(118)), 145))

    def random_case(self, rng):
        return next(itertools.islice(self.cases(rng), 145, None))

    def cases(self, rng):
        base = dict(fields=[16, 64, 16, 8, 40, 0, 0, 0, 16, 1, 7], offset=0,
                    amount=16, whence=0, queues={}, packed=packet(bytes(range(64))))
        yield base
        for field in range(11):
            for value in (0, 1, 32, 128):
                fields = list(base["fields"])
                fields[field] = value
                yield dict(base, fields=fields)
        for offset in (-2147483648, -129, -64, -17, -1, 0, 1, 64, 128, 2147483647):
            for whence in (0, 1, 2, 3, 0xffffffff):
                yield dict(base, offset=offset, whence=whence)
        for key in ("Backend.Read", "Backend.Write", "Backend.Seek", "Backend.GetPos", "Backend.GetSize",
                    "Backend.MapFile", "Backend.GetMacFileReference", "SemaLock.Lock", "array failure"):
            for value in (-1, 0, 1, 16, 128):
                yield dict(base, queues={key: [value]})
        for amount in (0, 1, 64, 128):
            yield dict(base, amount=amount)
        fields = list(base["fields"])
        fields[0] = fields[4]
        fields[2] = fields[0]
        yield dict(base, fields=fields, amount=fields[1])
        for _ in range(200):
            start = rng.randrange(65)
            fields = [rng.randrange(129), rng.choice([0, 4, 8, 16, 32, 64, 128]), rng.randrange(129),
                      start, start + rng.randrange(65), rng.randrange(2), rng.randrange(17),
                      rng.randrange(2), rng.randrange(129), rng.randrange(1, 4), rng.getrandbits(32)]
            queues = {"Backend.Seek": [rng.choice([-1, 0, 1, fields[0], fields[2], 128])],
                      "Backend.Read": [rng.choice([-1, 0, 1, 8, 16, 64])],
                      "Backend.Write": [rng.choice([-1, 0, 1, 8, 16, 64])],
                      "Backend.MapFile": [rng.randrange(2)]}
            yield dict(base, fields=fields, queues=queues, offset=rng.randrange(-140, 141),
                       amount=rng.randrange(129), whence=rng.choice([0, 1, 2, 3, 0xffffffff]))

    def run_case(self, runner, c):
        kind, op = self.kind, self.op
        c = dict(c, fields=list(c["fields"]))
        f = c["fields"]
        # Only methods with an explicit next-pointer guard accept detached objects.
        detached = op in (6, 12, 19) or (kind == 0 and op in (5, 9)) or (kind == 1 and op in (0, 2, 3, 4, 5, 13, 17))
        detached |= kind == 2 and op in (4, 5)
        detached |= kind == 3 and op == 4
        detached |= kind == 4 and op in (1, 3, 4, 13)
        detached |= kind == 5 and op != 6
        if not detached:
            f[7] = 0
        if kind == 5 and op == 6:
            f[1] = 64
            c["queues"] = dict(c["queues"], **{"Backend.GetSize": [len(c["packed"])]})
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        p = e.ba_p = e.alloc(SIZES[kind], b"\x5a" * SIZES[kind])
        backend = e.ba_backend = e.alloc(16, b"\0" * 16)
        io = e.ba_io = e.alloc(1024, b"\x69" * 1024)
        buffer = e.ba_buffer = e.alloc(1024, bytes((i * 11 + 19) & 255 for i in range(1024)))
        sem = e.ba_sem = e.alloc(4, struct.pack(">I", f[10]))
        e.cl = CL.PpcLog({p: 1, backend: 2, io: 4, buffer: 5, sem: 7, p + 8: 103, backend + 8: 104}, c["queues"])
        e.write(p, struct.pack(">3I", VT[kind], 0 if f[7] else backend, 3))
        e.write(backend + 8, struct.pack(">II", f[9], f[8]))
        if kind == 1:
            e.write(p + 12, struct.pack(">7I", buffer, buffer + f[6], f[0], f[3], f[4], f[2], f[1]))
        elif kind in (2, 3):
            w32(e, p + 12, f[0])
        elif kind == 4:
            e.write(p + 12, struct.pack(">4I", f[1], f[0], f[2], io if f[5] else 0))
        elif kind == 5:
            e.write(p + 12, struct.pack(">3I", f[1], buffer, buffer + f[0]))
        out = e.alloc(4, b"\xee" * 4)
        args = [p]
        if op in (0, 1, 15, 16, 17):
            args += [io, c["amount"]]
        elif op == 2:
            args += [T.u32(c["offset"]), c["whence"]]
        elif op == 6:
            args += [backend]
        elif op == 7:
            args += [T.u32(c["offset"]), c["amount"]]
        elif op == 8:
            args += [io]
        elif op == 12:
            args += [T.S16(c["offset"])]
        elif op == 13:
            args += [c["amount"]]
            if kind == 4:
                args += [T.u32(c["offset"])]
        if op in (9, 13):
            args.insert(0, out)
        s = stubs(e, c, kind, op)
        saved = s.activate(e)
        try:
            result = T.call_ppc(e, self.addr, args)[0]
        finally:
            s.deactivate(e, saved)
        if op in (9, 13):
            result = T.s32(e.u32(out))
        elif op in (7, 11):
            result = e.cl.lab(result)
        elif op in (0, 1, 2, 3, 4, 10, 15, 16, 17):
            result = T.s32(result)
        else:
            result = 0
        state = [T.u32(e.cl.lab(e.u32(p + 4))), e.u32(p + 8), e.u32(backend + 12), e.u32(backend + 8)] + [0] * 8
        state[11] = T.u32(e.cl.lab(e.u32(backend + 4)))
        data = buffer
        if kind == 1:
            data = e.u32(p + 12)
            cache = e.u32(p + 16)
            state[4:11] = [e.u32(p + a) for a in (20, 36, 32, 24, 28)] + [T.u32(e.cl.lab(data)), T.u32(cache - data if data else e.cl.lab(cache))]
        elif kind in (2, 3):
            state[4] = e.u32(p + 12)
        elif kind == 4:
            state[4:7] = [e.u32(p + a) for a in (16, 12, 20)]
            state[9] = T.u32(e.cl.lab(e.u32(p + 24)))
        elif kind == 5:
            data, cursor = e.u32(p + 16), e.u32(p + 20)
            state[4:6] = [T.u32(cursor - data) if cursor else 0xffffffff, e.u32(p + 12)]
            state[9] = T.u32(e.cl.lab(data))
        closed = op == 5 and kind in (1, 5)
        bytes_buffer = e.read(data, min(256, state[5])) + bytes(256 - min(256, state[5])) if data and not closed else bytes(256)
        if op == 12 and T.s16(c["offset"]) > 0:
            state = [0, 0, e.u32(backend + 12), e.u32(backend + 8)] + [0] * 7 + [T.u32(e.cl.lab(e.u32(backend + 4)))]
            bytes_buffer = bytes(256)
        expected = (result, state, e.read(io, 256), bytes_buffer, e.cl.log)

        CL.host_begin(lib, {}, c["queues"])
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        fields = (C.c_uint32 * 11)(*f)
        packed = C.create_string_buffer(c["packed"])
        F(lib, "tpw_ba_begin", None, C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32)(kind, fields, packed, len(c["packed"]))
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
        actual_result = F(lib, "tpw_ba_call", C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32)(
            op, c["offset"], c["amount"], c["whence"])
        log = CL.host_end(lib)
        hstate, hout, hbuf = (C.c_uint32 * 12)(), (C.c_uint8 * 256)(), (C.c_uint8 * 256)()
        F(lib, "tpw_ba_state", None, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32)(hstate, hout, hbuf, closed)
        actual = (actual_result, list(hstate), bytes(hout), bytes(hbuf), log)
        return T.same_values(expected, actual, "", False)


TESTS = [AccessTest(spec) for spec in SPECS]


def decoder_case(rng):
    """Back references may start in the initialized prefix and may overlap output."""
    extended = rng.randrange(2)
    header = bytes([0x10 | extended, rng.randrange(256)])
    if extended:
        header += bytes(rng.randrange(256) for _ in range(3))
    header += rng.randrange(1 << 24).to_bytes(3, "big")
    commands = bytearray()
    for form in rng.sample(range(4), 4):
        if form == 0:
            first, second = rng.randrange(128), rng.randrange(256)
            commands.extend((first, second))
            literals = first & 3
        elif form == 1:
            first, second = 0x80 | rng.randrange(64), rng.randrange(256)
            commands.extend((first, second, rng.randrange(256)))
            literals = second >> 6
        elif form == 2:
            first = 0xc0 | rng.randrange(32)
            commands.extend((first, rng.randrange(256), rng.randrange(256), rng.randrange(256)))
            literals = first & 3
        else:
            first = rng.randrange(0xe0, 0xfc)
            commands.append(first)
            literals = ((first & 31) + 1) * 4
        commands.extend(rng.randrange(256) for _ in range(literals))
    last = rng.randrange(4)
    commands.append(0xfc + last)
    commands.extend(rng.randrange(256) for _ in range(last))
    return dict(packed=header + commands, null=False, flag=rng.choice([-1, 0, 1, 0x7fff, -0x8000]))


class DecoderTest:
    def __init__(self, op, address, name):
        self.op, self.addr, self.name = op, address, "RefPack " + name
        self.exact_nan = False

    def edge_cases(self):
        rng = random.Random(119)
        out = [decoder_case(rng) for _ in range(100)]
        out.append(dict(packed=packet(b""), null=True, flag=0))
        for count in (0, 1, 2, 3, 4, 7, 112, 113, 255):
            out.append(dict(packed=packet(bytes(i & 255 for i in range(count))), null=False, flag=0))
        # Each form's literal counts, length endpoints and overlapping distance one.
        for command in (0, 3, 0x1c, 0x1f, 0x60, 0x7f, 0x80, 0xbf, 0xc0, 0xcf, 0xdf):
            for second in (0, 0x40, 0x80, 0xc0, 255):
                if command < 128:
                    body, literals = bytes([command, second]), command & 3
                elif command < 192:
                    body, literals = bytes([command, second, 0]), second >> 6
                else:
                    body, literals = bytes([command, second, 0, 255]), command & 3
                packed = b"\x10\xfb\x00\x01\x00" + body + b"abc"[:literals] + b"\xfc"
                out.append(dict(packed=packed, null=False, flag=0))
        return out

    def random_case(self, rng):
        return decoder_case(rng)

    def run_case(self, runner, c):
        e, lib, op = runner.emu, runner.lib, self.op
        e.reset_scratch()
        obj = e.alloc(4, struct.pack(">I", 0x1007232c))
        # All possible command distances land in this prefix; writes stay in the suffix.
        initial = DECODER_INITIAL
        memory = e.alloc(len(initial), initial)
        output = memory + 131072
        source = e.alloc(len(c["packed"]) + 8, c["packed"] + bytes(8))
        e.cl = CL.PpcLog({obj: 1})
        s = CL.Stubs()
        s.real.update((0x1000dbb4, 0x1000dff0, 0x1000e040, 0x1000e090, 0x100071a0, 0x1000722c))
        def deleted(e):
            e.cl.rec("delete", (e.cl.lab(e.r(3)),))
            e.cl.forget(e.r(3))
        s.imp("__dl__FPv", deleted)
        args = [obj]
        if op == 0:
            args += [output, 0 if c["null"] else source, 0]
        elif op == 1:
            args += [source]
        elif op >= 4:
            args += [T.S16(c["flag"])]
        saved = s.activate(e)
        try:
            value = T.s32(T.call_ppc(e, self.addr, args)[0])
        finally:
            s.deactivate(e, saved)
        expected = (value if op < 4 else 0, e.read(memory, len(initial)), e.read(source, len(c["packed"]) + 8), e.cl.log)
        CL.host_begin(lib, {})
        hostmem, hostsrc = C.create_string_buffer(initial, len(initial)), C.create_string_buffer(c["packed"] + bytes(8))
        result = F(lib, "tpw_refpack_call", C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32)(
            op, C.addressof(hostmem) + 131072, None if op == 0 and c["null"] else hostsrc, None, c["flag"])
        log = CL.host_end(lib)
        actual = (result, hostmem.raw, hostsrc.raw[:-1], log)
        return T.same_values(expected, actual, "", False)


TESTS += [DecoderTest(op, address, name) for op, address, name in [
    (0, 0x1000dbb4, "MemDecompress"), (1, 0x1000e090, "DecompressedSize"),
    (2, 0x1000dff0, "CompressWorkspaceSize"), (3, 0x1000e040, "DecompressWorkspaceSize"),
    (4, 0x100071a0, "destructor"), (5, 0x1000722c, "base destructor")]]
