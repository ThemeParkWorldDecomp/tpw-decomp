"""Unit w222: 52 bullfrog file-system and string-conversion functions.

The same world is built in native and PPC memory using member layouts. Compare
answers, fields, full buffers, block links and ordered calls. Platform conversion
uses a shared terminator-inclusive byte/UTF-16 model. Physical disc operations
below the real storage are template fakes. No floating point operations.
Excluded: dereferenced NULL disc access after allocation failure plus successful
Open, wide dynamic conversion returning zero on the second call (writes index
0xffffffff), and FullPath sizes above its original 260-byte local buffer.
"""
import ctypes as C
import random
import struct
from pathlib import Path
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_FileSystemMore"
MASK = 0xffffffff
# The manifest order is also the C wrapper dispatch order.
SPECS = [
    (0x1000c2b4, "CFileStorage::FindFirst"), (0x1000c448, "CFileStorage::FindNext"),
    (0x1000c5b0, "CFileStorage::OpenFile"), (0x1000c69c, "CFileStorage::FullPath wide"),
    (0x1000c7a0, "CFileStorage::FullPath string"), (0x1000c8ec, "CFileStorage::SetAttribs"),
    (0x1000c9c4, "CFileStorage::CreateDir"), (0x1000ca88, "CFileStorage::Delete"),
    (0x1000cb48, "CFileStorage::Move"), (0x1000b060, "CDiscFileStorage::Open"),
    (0x1000d12c, "CDiscFileStorage::~CDiscFileStorage"), (0x1000d1d4, "CWadFileStorage::~CWadFileStorage"),
    (0x1000d3b8, "CWadFileStorage::Open"), (0x10008fbc, "CFileInfo::~CFileInfo"),
    (0x10005f20, "_TbMemFileAccess::_TbMemFileAccess"), (0x10005fdc, "_TbMemFileAccess::Close"),
    (0x10006040, "_TbMemFileAccess::Read"), (0x10006360, "_TbMemFileAccess::Seek"),
    (0x10006478, "_TbMemFileAccess::GetSize"), (0x100064c4, "_TbMemFileAccess::GetPos"),
    (0x10006510, "_TbMemFileAccess::AddBlock"), (0x10006628, "_TbMemFileAccess::RemoveAllBlocks"),
    (0x100066e0, "_TbMemFileAccess::GetSemaphore"), (0x10006730, "_TbMemFileAccess::~_TbMemFileAccess"),
    (0x10009c3c, "_TbZLibFileAccess::~_TbZLibFileAccess"), (0x1000a224, "_TbZLibFileAccess::_TbZLibFileAccess"),
    (0x1000a2b0, "_TbZLibFileAccess::Open"), (0x1000a34c, "_TbZLibFileAccess::Write"),
    (0x1000a39c, "_TbZLibFileAccess::Read"), (0x1000a46c, "_TbZLibFileAccess::Seek"),
    (0x1000a58c, "_TbZLibFileAccess::GetPos"), (0x1000a5e0, "_TbZLibFileAccess::GetSize"),
    (0x1000a62c, "_TbZLibFileAccess::Close"), (0x1000a6b8, "_TbZLibFileAccess::AttachAccessClass"),
    (0x1000a79c, "_GetMoreData"), (0x1000a958, "CAccessItem::Close"),
    (0x1000ac38, "CAccessItem::AttachAccessClass"), (0x1000acf8, "CAccessItem::MapFile"),
    (0x1000ad68, "CAccessItem::UnmapFile"), (0x1000addc, "CAccessItem::GetSemaphore"),
    (0x1000ae50, "CAccessItem::Flush"), (0x1000ae98, "CAccessItem::GetMacFileReference"),
    (0x100050dc, "_TbDiscAccess::GetSemaphore"), (0x1000523c, "_TbDiscAccess::AttachAccessClass"),
    (0x100662c8, "Strings::Convert wide buffer"), (0x10066360, "Strings::Convert wide string"),
    (0x100664ac, "Strings::Convert byte buffer"), (0x1006654c, "Strings::Convert byte string"),
    (0x10011e58, "TbStringBase::LockBuffer wide"), (0x10012e2c, "TbStringBase::ReleaseBuffer wide"),
    (0x100061a8, "_TbMemFileAccess::Write"), (0x1002951c, "CMemFile::Open")]
MAC = [4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 0, 4, 8, 12, 16, 20,
       12, 16, 20, 4, 4, 32, 36, 4, 8, 12, 28]
DTORS = {10, 11, 13, 23, 24}
VOID = {15, 21, 32, 33, 35, 36, 38, 43, 49}
ERROR = set(range(10)) | {12, 20, 26, 40, 44, 45, 46, 47, 51}
KIND = {**dict.fromkeys(range(9), 2), 9: 3, 10: 3, 11: 4, 12: 4, 13: 5,
        **dict.fromkeys(range(14, 24), 0), **dict.fromkeys(range(24, 34), 1),
        34: 10, **dict.fromkeys(range(35, 42), 0), 42: 8, 43: 8,
        44: 6, 45: 6, 46: 7, 47: 7, 48: 6, 49: 6, 50: 0, 51: 9}
VT = {0: 0x10071a60, 1: 0x10071d60, 2: 0x10071e88, 3: 0x10071e88,
      4: 0x10071ed4, 6: 0x10072380, 7: 0x10072580, 8: 0x100719a4, 9: 0x10071dec, 10: 0x10071da4}


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "fsm_bound", False):
        return
    def f(name, result, args):
        fn = getattr(lib, "tpw_fsm_" + name)
        fn.restype, fn.argtypes = result, args
    P, U = C.c_void_p, C.c_uint32
    f("make", C.c_uint64, [C.c_int])
    f("layout", C.c_int, [C.POINTER(U)])
    f("call", C.c_uint64, [C.c_int, P, P, P, U, C.c_uint64])
    f("config", None, [C.c_int, C.c_int, U, C.c_char_p, C.c_char_p, P, P])
    f("sema", C.c_uint64, [])
    layout = (U * 32)()
    n = lib.tpw_fsm_layout(layout)
    assert n == 27
    lib.fsm_layout = list(layout)[:n]
    lib.fsm_bound = True


class Side:
    """Layouts differ only at the point a named member or pointer is accessed."""
    def word(self, p, value=None):
        if value is None:
            return struct.unpack(self.end + "I", self.read(p, 4))[0]
        self.write(p, struct.pack(self.end + "I", value & MASK))

    def ptr(self, p, value=None):
        fmt = self.end + ("I" if self.ps == 4 else "Q")
        if value is None:
            return struct.unpack(fmt, self.read(p, self.ps))[0]
        self.write(p, struct.pack(fmt, value))

    def field(self, p, index, value=None, pointer=False):
        return (self.ptr if pointer else self.word)(p + self.off[index], value)

    def text(self, p, wide=False):
        units = []
        while True:
            n = int.from_bytes(self.read(p, 2 if wide else 1), "big" if self.end == ">" else "little")
            if n == 0:
                return units
            units.append(n)
            p += 2 if wide else 1

    def string(self, text, wide=False):
        data = b"".join(struct.pack(self.end + "H", x) for x in text + [0]) if wide else bytes(text + [0])
        p = self.alloc(max(len(data), 1024))
        self.write(p, data)
        return p

    def label(self, p, label):
        self.labels[p] = label

    def lab(self, p):
        return 0 if not p else self.labels.get(p, -1)


class Host(Side):
    end, ps = "<", 8
    def __init__(self, lib, case):
        bind(lib)
        self.lib = lib
        self.off = lib.fsm_layout
        self.labels = {}
        self.bufs = []
        self.case = case
    def alloc(self, n):
        assert n <= 2048
        return self.lib.tpw_fsm_make(11)
    def make(self, kind):
        return self.lib.tpw_fsm_make(kind)
    def read(self, p, n):
        return C.string_at(p, n)
    def write(self, p, data):
        C.memmove(p, data, len(data))
    def begin(self):
        c = self.case
        self.lib.tpw_fsm_config(KIND[c["op"]], c["nameoff"], c["seed"], c["entry"], c["full"], self.zipbuf, self.out)
        CL.host_begin(self.lib, {label: p for p, label in self.labels.items()}, c["queues"])
    def done(self):
        pass
    def log(self):
        return CL.host_end(self.lib)
    def call(self, op, obj, a=0, b=0, c=0, d=0):
        return self.lib.tpw_fsm_call(op, obj, a, b, c & MASK, d if op == 2 else d & MASK)


class Ppc(Side):
    end, ps = ">", 4
    def __init__(self, e, case):
        self.e, self.case = e, case
        self.off = MAC
        self.labels = {}
        self.stubs = CL.Stubs()
        self.stubs.real.update(a for a, _ in SPECS)
        # These callees execute the same real code as the host link set.
        self.stubs.real.update([0x1000a818, 0x1000a86c, 0x1000b6c0, 0x1000bf88,
                                0x1000b998, 0x100052a8, 0x10005a18, 0x10011634, 0x100136d0])
        self.fakevts = {}
        self.install()
    def alloc(self, n):
        return self.e.alloc(n, b"\0" * n)
    def read(self, p, n):
        return self.e.read(p, n)
    def write(self, p, data):
        self.e.write(p, data)
    def make(self, kind):
        p = self.alloc(2048)
        if kind in VT:
            self.word(p, self.fakevts.get(kind, VT[kind]))
        return p
    def begin(self):
        self.L = self.e.cl = CL.PpcLog(self.labels, self.case["queues"])
        record = self.L.rec
        self.L.rec = lambda name, ints=(), texts=(): record(name, ints, tuple(t[:159] for t in texts))
        self.saved = self.stubs.activate(self.e)
    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
    def log(self):
        self.labels = self.L.labels
        return self.L.log
    def call(self, op, obj, a=0, b=0, c=0, d=0):
        args = {
            0: [obj, a, b], 1: [obj, b], 2: [obj, b, a, c, d], 3: [obj, b, a, c],
            4: [obj, b, a], 5: [obj, a, c], 6: [obj, a], 7: [obj, a], 8: [obj, a, b],
            9: [obj, a], 12: [obj, a, c], 14: [obj, c], 16: [obj, a, c], 17: [obj, c, d],
            26: [obj, c], 27: [obj, a, c], 28: [obj, a, c], 29: [obj, c, d], 33: [obj, a],
            34: [a, c, obj], 36: [obj, a], 37: [obj, c, d], 38: [obj, a], 43: [obj, a],
            44: [b, c, a], 45: [obj, a], 46: [b, c, a], 47: [obj, a],
            48: [obj, c], 49: [obj, c], 50: [obj, a, c], 51: [obj, c]}.get(op, [obj])
        if op in DTORS:
            args.append(c)
        out = self.alloc(4)
        if op in ERROR:
            args.insert(0, out)
        result = T.call_ppc(self.e, SPECS[op][0], [x & MASK for x in args])[0]
        return self.word(out) if op in ERROR else result
    def virtual(self, kind, slot, fn):
        if kind not in self.fakevts:
            self.fakevts[kind] = self.alloc(64)
            if kind in VT:
                self.write(self.fakevts[kind], self.read(VT[kind], 60))
        entry = self.e.alloc(4, b"\x4e\x80\x00\x20")
        vector = self.e.alloc(8, struct.pack(">II", entry, self.e.toc))
        self.stubs.internal(entry, fn)
        self.word(self.fakevts[kind] + slot, vector)
    def install(self):
        s = self.stubs
        def glue(address, fn):
            def wrapped(e):
                # A replaced import glue must still save the caller's TOC.
                self.word(e.r(1) + 20, e.r(2))
                fn(e)
            s.internal(address, wrapped)
        s.internal(0x10024a24, lambda e: None)
        s.internal(0x10024a54, lambda e: None)
        def conv(wide):
            def fn(e):
                cp, flags, src, length, dest, size = [e.r(i) for i in range(3, 9)]
                units = self.text(src, not wide)
                text = bytes((x if wide or x < 128 else ord('?')) for x in units).decode("latin1")
                ints = [cp, flags, T.s32(length), int(bool(dest)), T.s32(size)]
                if not wide:
                    ints += [e.cl.lab(e.r(9)), e.cl.lab(e.r(10))]
                e.cl.rec("MB" if wide else "WC", tuple(ints), (text,))
                data = units + [0]
                if dest and size:
                    data = data[:size]
                    if wide:
                        self.write(dest, b"".join(struct.pack(">H", x) for x in data))
                    else:
                        self.write(dest, bytes(x if x <= 255 else ord('?') for x in data))
                answer = 0 if dest and size and size < len(units) + 1 else len(units) + 1
                if not dest:
                    answer += e.cl.ret("query_extra")
                if wide and dest and e.cl.ret("mb_fail"):
                    answer = 0
                e.set_r(3, answer)
            return fn
        glue(0x1006b70c, conv(True))
        glue(0x1006b6f4, conv(False))
        def allocate(array):
            def fn(e):
                n = e.r(3)
                failed = e.cl.ret("array failure" if array else "allocation failure")
                p = 0 if failed else e.alloc(max(n, 1), b"\x5a" * max(n, 1))
                label = 0 if failed else e.cl.newlab(p)
                e.cl.rec("new[]" if array else "new", (n, label))
                e.set_r(3, p)
            return fn
        glue(0x1006b2bc, allocate(False))
        glue(0x1006b334, allocate(True))
        def delete(e):
            e.cl.rec("delete", (e.cl.lab(e.r(3)),))
            e.cl.forget(e.r(3))
        glue(0x1006b2d4, delete)
        glue(0x1006b364, delete)
        def increment(e):
            p = e.r(3)
            before = self.word(p)
            e.cl.rec("InterlockedIncrement", (e.cl.lab(p), T.s32(before)))
            self.word(p, before + 1)
            e.set_r(3, (before + 1) & MASK)
        s.imp("InterlockedIncrement__10NS_MacDozeFPl", increment)
        s.imp("wcslen", lambda e: e.set_r(3, len(self.text(e.r(3), True))))
        def project(e):
            out, dest, size, name = [e.r(i) for i in range(3, 7)]
            e.cl.rec("LbFile_ToProjectFilename", (size,), (e.cstr(name),))
            self.write(dest, (b"P:" + bytes(self.text(name)))[:size - 1] + b"\0")
            self.word(out, 0)
        s.internal(0x10001590, project)
        def attributes(e):
            e.cl.rec("GetFileAttributes", (), (e.cstr(e.r(3)),))
            e.set_r(3, e.cl.ret("GetFileAttributes", 0x20) & MASK)
        s.imp("GetFileAttributes__10NS_MacDozeFPCc", attributes)
        def span(p):
            for base in (self.input, self.out):
                if base <= p <= base + 2048:
                    return self.L.lab(base), p - base, base + 2048 - p
            b = self.field(self.obj, 2, pointer=True)
            while b:
                data = self.field(b, 12, pointer=True)
                if data <= p <= data + self.case["block"]:
                    return self.L.lab(b), p - data, data + self.case["block"] - p
                b = self.field(b, 11, pointer=True)
            return self.L.lab(p), 0, 0
        def memcopy(e):
            dest, src, size = [e.r(i) for i in (3, 4, 5)]
            if self.case["op"] in (16, 20, 50):
                dl, do, dr = span(dest)
                sl, so, sr = span(src)
                e.cl.rec("memcpy", (dl, do, sl, so, size))
                count = min(size, dr, sr)
            else:
                count = size
            self.write(dest, self.read(src, count))
            e.set_r(3, dest)
        s.imp("memcpy", memcopy)
        def copy(e):
            e.cl.rec("CopyMemory", (e.cl.lab(e.r(3)), (e.r(4) - self.zipbuf) & MASK, e.r(5)))
            self.write(e.r(3), self.read(e.r(4), e.r(5)))
        glue(0x1006b784, copy)
        def read(e):
            p, dest, n = e.r(3), e.r(4), e.r(5)
            e.cl.rec("Read", (e.cl.lab(p), e.cl.lab(dest), n))
            count = T.s32(e.cl.ret("read", 3))
            done = max(0, min(n, count))
            self.write(dest, bytes((self.case["seed"] + i) & 255 for i in range(done)))
            e.set_r(3, count & MASK)
        def attach(e):
            e.cl.rec("Attach", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
            self.field(e.r(3), 0, e.r(4), True)
        def mapfile(e):
            e.cl.rec("MapFile", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))
            e.set_r(3, self.out)
        def unmap(e):
            e.cl.rec("UnmapFile", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        def macref(e):
            e.cl.rec("MacRef", (e.cl.lab(e.r(3)),))
            e.set_r(3, e.cl.ret("read", 3))
        def semaphore(e):
            e.cl.rec("Semaphore", (e.cl.lab(e.r(3)),))
            e.set_r(3, self.sema)
        for slot, fn in [(12, read), (0x34, attach), (0x20, mapfile), (0x24, unmap), (0x2c, macref), (0x38, semaphore)]:
            self.virtual(10, slot, fn)
        def resize(e):
            e.cl.rec("ResizeTo", (e.cl.lab(e.r(3)), e.r(4)))
            self.field(e.r(3), 23, e.r(4))
        def assign(e):
            e.cl.rec("Assign", (e.cl.lab(e.r(3)),), ("",))
            self.field(e.r(3), 23, 0)
            self.write(self.field(e.r(3), 24, pointer=True), b"\0\0" if self.case["op"] != 47 else b"\0")
        for kind in (6, 7):
            self.virtual(kind, 12, resize)
            self.virtual(kind, 32, assign)
        def storage(method):
            def fn(e):
                out, p = e.r(3), e.r(4)
                ints, texts = [e.cl.lab(p)], []
                if method in ("FindFirst", "FindNext"):
                    info = e.r(6) if method == "FindFirst" else e.r(5)
                    ints += [int(self.ptr(info + 32) == info + 36), self.read(info + 36, 1)[0]]
                    if method == "FindFirst":
                        texts = [e.cstr(e.r(5))]
                    self.word(info, self.case["seed"])
                    self.word(info + 4, self.case["seed"] + 1)
                    self.write(info + 8, bytes((self.case["seed"] + i * 7) & 255 for i in range(24)))
                    self.ptr(info + 32, info + 36 + self.case["nameoff"])
                    self.write(info + 36, self.case["entry"] + b"\0")
                elif method == "OpenFile":
                    ints += [e.cl.lab(e.r(5)), e.r(7), e.cl.lab(e.r(8))]
                    texts = [e.cstr(e.r(6))]
                elif method == "FullPath":
                    ints += [e.r(7)]
                    texts = [e.cstr(e.r(6))]
                    self.write(e.r(5), self.case["full"] + b"\0")
                else:
                    texts = [e.cstr(e.r(5))]
                    if method == "SetAttribs":
                        ints += [e.r(6)]
                    if method == "Move":
                        texts += [e.cstr(e.r(6))]
                e.cl.rec(method, tuple(ints), tuple(texts))
                self.word(out, e.cl.ret("storage"))
            return fn
        for slot, name in [(12, "FindFirst"), (16, "FindNext"), (20, "OpenFile"), (24, "FullPath"),
                           (36, "SetAttribs"), (40, "CreateDir"), (44, "Delete"), (48, "Move")]:
            self.virtual(2, slot, storage(name))
        def decompress(e):
            dest, buffer, size, callback, context, workspace = [e.r(i) for i in range(3, 9)]
            e.cl.rec("Decompress", (e.cl.lab(dest), e.cl.lab(buffer), size, int(e.u32(callback) == 0x1000a79c), e.cl.lab(context), e.cl.lab(workspace)))
            # Unicorn cannot recursively start emulation inside an import hook.
            # Dispatch the fake backend here; _GetMoreData itself has its own real test.
            e.set_r(3, context)
            e.set_r(4, buffer)
            e.set_r(5, 7)
            read(e)
            e.cl.rec("CallbackResult", (0,))
            e.set_r(3, 123)
        s.internal(0x10025d14, decompress)
        def wadctor(e):
            p = e.r(3)
            self.wad_buffers = []
            for _ in range(3):
                e.set_r(3, 9)
                allocate(True)(e)
                self.wad_buffers.append(e.r(3))
            e.cl.rec("WadCtor", (e.cl.lab(p),))
            self.word(p, self.fakevts[13])
            self.field(p, 0, 0, True)
            self.word(p + 12, 0)
            e.set_r(3, p)
        def wadopen(e):
            e.cl.rec("WadOpen", (e.cl.lab(e.r(4)), e.r(6)), (e.cstr(e.r(5)),))
            self.word(e.r(3), e.cl.ret("storage"))
        def waddtor(e):
            p = e.r(3)
            e.cl.rec("WadDtor", (e.cl.lab(p),))
            for b in reversed(self.wad_buffers):
                e.set_r(3, b)
                delete(e)
            e.set_r(3, p)
            delete(e)
        self.virtual(13, 8, waddtor)
        s.internal(0x100086c8, wadctor)
        s.internal(0x10008af4, wadopen)


def base(r, op):
    block = r.choice([1, 4, 16])
    count = r.randrange(4)
    size = r.randrange((count - 1) * block + 1, count * block + 1) if count else 0
    return dict(op=op, block=block, count=count, size=size, position=r.randrange(size + 1),
                n=r.randrange(36), offset=r.choice([-100, -1, 0, 1, size, size + 1, 0x7fffffff, -0x80000000]),
                whence=r.choice([0, 1, 2, 3, MASK]), old=r.randrange(2), seed=r.getrandbits(32),
                nameoff=r.randrange(4), name=r.choice([[], [65], [128, 255], [0x100, 66], [65] * 260]),
                other=r.choice([[], [66, 67], [66] * 260]), entry=r.choice([b"abc/file", b"\x80\xff", b""]),
                full=r.choice([b"", b"Root:file", b"\x80/file"]), flag=r.choice([0, 1, -1, 0x10000, 0x18000]),
                null=r.randrange(4) == 0, sentinel=r.randrange(2),
                queues={"storage": [r.choice([-7, -1, 0, 1, 17])], "read": [r.randrange(8)],
                        "allocation failure": [0], "array failure": [r.randrange(4) == 0 if op in (20, 26, 50) else 0]})


def edges(op):
    r = random.Random(222 + op)
    for whence in [0, 1, 2, 3, MASK]:
        for offset in [-17, -1, 0, 1, 16, 17, -0x80000000, 0x7fffffff]:
            c = base(r, op)
            c.update(block=4, count=3, size=10, position=4, whence=whence, offset=offset)
            yield c
    for block in [1, 4, 16]:
        for n in [0, 1, block - 1, block, block + 1, 3 * block]:
            c = base(r, op)
            c.update(block=block, count=0, size=0, position=0, n=n)
            yield c
    for name in [[], [65], [128, 255], [0x100, 66], [65] * 259, [65] * 260]:
        for result in [-1, 0, 1]:
            c = base(r, op)
            c.update(name=name)
            c["queues"]["storage"] = [result]
            yield c
    if op == 51:
        for fail in (0, 1):
            c = base(r, op)
            c.update(old=0)
            c["queues"]["allocation failure"] = [fail]
            yield c
    if op in (16, 50):
        for n in [0x7fffffff, 0x80000000, MASK]:
            c = base(r, op)
            c.update(block=4, count=1, size=4, position=1, n=n)
            c["queues"]["array failure"] = [0, 1]
            yield c
    if op in (9, 12):
        for fail in (0, 1):
            c = base(r, op)
            c.update(name=[65], old=0)
            c["queues"].update({"allocation failure": [fail], "GetFileAttributes": [-1], "storage": [-1]})
            yield c
    if op in (0, 1):
        c = base(r, op)
        c.update(name=[65], entry=b"abc/file", nameoff=4)
        c["queues"].update({"storage": [1], "mb_fail": [1]})
        yield c
    if op in (4, 45, 47):
        c = base(r, op)
        c.update(name=[65], full=b"Root:file")
        c["queues"].update({"storage": [0], "query_extra": [1]})
        yield c
    if op in (33, 34):
        c = base(r, op)
        c["queues"]["read"] = [-1]
        yield c
    if op in (14, 20, 51):
        c = base(r, op)
        c.update(block=0, count=0, size=0, position=0, old=0)
        c["queues"]["array failure"] = [0]
        yield c
    if op in (17, 18, 19):
        for word in (0x7fffffff, 0x80000000, MASK):
            c = base(r, op)
            c.update(count=0, size=word, position=word, offset=1, whence=1)
            yield c
    if op == 49:
        for capacity in (0x80000000, MASK):
            c = base(r, op)
            c.update(capacity=capacity, sentinel=1)
            yield c


def build(s, c):
    op, kind = c["op"], KIND[c["op"]]
    s.obj = s.make(kind)
    s.label(s.obj, 1)
    s.backend = s.make(10)
    s.label(s.backend, 2)
    s.prior = s.make(10)
    s.label(s.prior, 3)
    s.out = s.alloc(2048)
    s.write(s.out, b"\x69" * 2048)
    s.label(s.out, 4)
    s.input = s.alloc(2048)
    s.write(s.input, bytes((i * 11 + 19) & 255 for i in range(2048)))
    s.label(s.input, 5)
    s.zipbuf = s.input
    s.sema = s.alloc(4) if s.ps == 4 else s.lib.tpw_fsm_sema()
    s.label(s.sema, 6)
    s.name = s.string(c["name"], True)
    s.other = s.string(c["other"], True)
    s.bytes = s.string([x & 255 or 1 for x in c["name"]])
    s.info = s.make(12)
    s.write(s.info, b"\x37" * 32)
    s.field(s.info, 21, s.info + s.off[22] + 6, True)
    s.write(s.info + s.off[22], b"\x71\x71" * 260)
    s.label(s.info, 7)
    s.file = s.make(9)
    s.label(s.file, 8)
    s.blocks = []
    if kind == 0:
        for i in range(c["count"]):
            b = s.alloc(s.off[15] + c["block"])
            s.label(b, 20 + i)
            s.blocks.append(b)
        for i, b in enumerate(s.blocks):
            s.field(b, 10, s.blocks[i - 1] if i else 0, True)
            s.field(b, 11, s.blocks[i + 1] if i + 1 < len(s.blocks) else 0, True)
            s.field(b, 12, b + s.off[15], True)
            s.field(b, 13, i * c["block"])
            s.field(b, 14, (i + 1) * c["block"])
            s.write(b + s.off[15], bytes((i * c["block"] + j + 7) & 255 for j in range(c["block"])))
        s.field(s.obj, 0, s.backend if 37 <= op <= 41 else s.prior if c["old"] else 0, True)
        s.field(s.obj, 1, c["seed"])
        s.field(s.obj, 2, s.blocks[0] if s.blocks else 0, True)
        s.field(s.obj, 3, s.blocks[-1] if s.blocks else 0, True)
        current = c["position"] // c["block"] if c["block"] else 0
        b = s.blocks[current] if current < len(s.blocks) else 0
        s.field(s.obj, 4, b, True)
        s.field(s.obj, 5, b + s.off[15] + c["position"] % c["block"] if b else 0, True)
        s.field(s.obj, 6, c["position"])
        s.field(s.obj, 7, c["size"])
        s.field(s.obj, 8, c["block"])
        s.field(s.obj, 9, c["seed"])
    if kind == 1:
        s.field(s.obj, 0, s.prior if c["old"] else 0, True)
        s.field(s.obj, 1, c["seed"])
        s.field(s.obj, 16, c["size"])
        s.field(s.obj, 17, s.zipbuf, True)
        s.field(s.obj, 18, s.zipbuf + c["position"], True)
    if kind in (6, 7):
        s.strbuf = s.alloc(2048)
        s.write(s.strbuf, b"\x77" * 2048)
        s.write(s.strbuf, struct.pack(s.end + "4H", 97, 98, 99, 0) if kind == 6 else b"abc\0")
        s.label(s.strbuf, 9)
        s.field(s.obj, 23, 3)
        s.field(s.obj, 24, s.strbuf, True)
        s.field(s.obj, 25, c.get("capacity", c["n"]))
    if kind in (2, 3, 4):
        s.field(s.obj, 19, 0, True)
    if kind == 9:
        s.field(s.obj, 20, s.backend if c["old"] else 0, True)
    if op == 4:
        s.dynamic_source = s.make(6)
        s.field(s.dynamic_source, 24, s.name, True)
        s.dynamic = s.make(6)
        s.field(s.dynamic, 24, s.out, True)
        s.label(s.dynamic, 10)
    s.begin()


def execute(s, c):
    op = c["op"]
    obj, a, b, n, d = s.obj, s.name, 0, c["n"], 0
    if op in (0, 1):
        b = s.info
    if op == 2:
        b, n, d = s.file, c["seed"], s.input
    if op == 3:
        b, n = s.out, min(c["n"], 260)
    if op == 4:
        a, b = s.dynamic_source, s.dynamic
    if op == 5 or op == 12:
        n = c["seed"]
    if op == 8:
        b = s.other
    if op in DTORS:
        obj, n = (0 if c["null"] else obj), c["flag"]
    if op in (14, 51):
        n = c["block"]
    if op in (16, 28, 34):
        a = s.out
    if op in (17, 29, 37):
        n, d = c["offset"], c["whence"]
    if op in (33, 36, 43):
        a = s.backend
    if op == 38:
        a = s.out
    if op in (44, 46):
        a = s.bytes if op == 44 else s.name
        b = s.out
        n = c["n"]
    if op in (45, 47):
        a = s.bytes if op == 45 else s.name
    if op == 49:
        n = 0x7fffffff if c["sentinel"] else c["n"]
    if op == 50:
        a = s.input
    result = s.call(op, obj, a, b, n, d)
    if op == 32:
        s.closed_position = (s.call(30, obj) + (s.zipbuf & MASK)) & MASK
    if op in VOID:
        return 0
    if op in DTORS or op in (14, 25, 22, 37, 39, 42, 48):
        if op == 22:
            return int(result == s.obj + s.off[9])
        if op == 42:
            return int(result == s.obj + s.off[26])
        return s.lab(result)
    return result & MASK


def observe(s, c, answer):
    op, kind = c["op"], KIND[c["op"]]
    log = s.log()
    # New labels are exposed by calllog on the PPC side; host labels are discovered
    # from real object members and their allocation event order below.
    result = dict(answer=answer, log=log)
    if op in DTORS and (c["null"] or T.s16(c["flag"]) > 0):
        return result
    if kind == 0:
        chain = []
        b = s.field(s.obj, 2, pointer=True)
        newlabels = [int(event[1][1]) for event in log if event[0] == "new[]" and event[1][1]]
        newindex = 0
        while b:
            if b not in s.labels:
                s.label(b, newlabels[newindex])
                newindex += 1
            prev, nxt, data = [s.field(b, i, pointer=True) for i in (10, 11, 12)]
            chain.append((b, prev, nxt, data, s.field(b, 13), s.field(b, 14), s.read(data, c["block"])))
            b = nxt
        current, cursor = [s.field(s.obj, i, pointer=True) for i in (4, 5)]
        result["blocks"] = [(s.lab(b), s.lab(prev), s.lab(nxt), data - b - s.off[15], start, end, contents)
                            for b, prev, nxt, data, start, end, contents in chain]
        result["memory"] = [s.lab(s.field(s.obj, 0, pointer=True)), s.field(s.obj, 1),
                            s.lab(s.field(s.obj, 2, pointer=True)), s.lab(s.field(s.obj, 3, pointer=True)), s.lab(current),
                            (cursor - current - s.off[15]) & MASK if cursor else -1,
                            s.field(s.obj, 6), s.field(s.obj, 7), s.field(s.obj, 8), s.field(s.obj, 9)]
        result["output"] = s.read(s.out, 64)
    if kind == 1:
        buf, cur = [s.field(s.obj, i, pointer=True) for i in (17, 18)]
        result["zlib"] = [s.lab(s.field(s.obj, 0, pointer=True)), s.field(s.obj, 1), s.field(s.obj, 16), bool(buf),
                          cur - buf if cur else None, buf == s.zipbuf]
        result["output"] = s.read(s.out, 64)
        if op == 32:
            result["closed_position"] = s.closed_position
    if op in (0, 1):
        result["entry"] = ([s.word(s.info), s.word(s.info + 4), s.read(s.info + 8, 24)], (s.field(s.info, 21, pointer=True) - s.info - s.off[22]) // 2,
                           list(struct.unpack(s.end + "260H", s.read(s.info + s.off[22], 520))))
    if op in (3, 44, 46):
        result["output"] = list(struct.unpack(s.end + "300H", s.read(s.out, 600))) if op != 46 else s.read(s.out, 600)
    if kind in (6, 7) and op not in (44, 46):
        result["string"] = [s.field(s.obj, 23), s.field(s.obj, 25),
                            list(struct.unpack(s.end + "512H", s.read(s.strbuf, 1024))) if kind == 6 else s.read(s.strbuf, 1024)]
    if op == 4:
        result["dynamic"] = (s.field(s.dynamic, 23), list(struct.unpack(s.end + "300H", s.read(s.out, 600))))
    if op in (9, 12):
        p = s.field(s.obj, 19, pointer=True)
        result["storage_access"] = bool(p)
        if p:
            result["refs"] = s.word(p + (12 if s.ps == 4 else 20))
    if op == 51:
        p = s.field(s.obj, 20, pointer=True)
        result["file"] = 2 if p == s.backend else int(bool(p))
        if p and p != s.backend:
            result["mem"] = [s.field(p, i) for i in (1, 6, 7, 8)]
    if op in (33, 36):
        result["backend_next"] = s.lab(s.field(s.backend, 0, pointer=True))
    return result


def run(s, c):
    build(s, c)
    try:
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


TESTS = [FnTest(name, ppc, host, edge=lambda op=op: edges(op), rnd=lambda r, op=op: base(r, op))
         for op, (_, name) in enumerate(SPECS)]
