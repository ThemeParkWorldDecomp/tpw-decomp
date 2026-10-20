"""The bullfrog file system's disc side as one unit: scripts of calls on a storage and a
file (open a folder, list it, open a file, read, seek, map, close, paths, deleting and
moving), with macdoze's Win32 file calls and the LbFile_* wrappers as recorded, queued
fakes on both sides. Covers _TbDiscAccess, _TbFileStorageAccess's and _TbStorageAccess's
members, CAccessItem's slots and the CFile and CFileStorage members that forward to them."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_bflDiscAccess"

# In-binary functions the script calls directly (results in a hidden buffer where marked).
STORAGE_OPEN = 0x1000af14        # CDiscFileStorage::Open(const char*), hidden result
FIND_FIRST, FIND_NEXT, OPEN_FILE = 0x1000bcec, 0x1000bda0, 0x1000be50
FILE_READ, FILE_WRITE, FILE_SEEK = 0x10000f28, 0x10001044, 0x1000b504
FILE_SIZE, FILE_POS, FILE_FLUSH, FILE_MAP = 0x1000b588, 0x1000b600, 0x1000b808, 0x1000b788
FILE_MACREF, FILE_CLOSE = 0x1000b358, 0x1000b48c
FULL_PATH_BUFFER, FULL_PATH_STRING, ROOT_PATH = 0x1000bc50, 0x1000bb80, 0x1000bf04
SET_ATTRIBS, CREATE_DIR, DELETE, MOVE, STORAGE_CLOSE = 0x1000c058, 0x1000c0f4, 0x1000c18c, 0x1000c220, 0x1000bb00
FILE_CTOR, FILE_COPY, FILE_ASSIGN, FILE_DTOR = 0x1000b1dc, 0x1000b224, 0x1000b3dc, 0x1000b2b8
STORAGE_COPY, STORAGE_DTOR = 0x1000b8e0, 0x1000b998
DISC_MAP, DISC_UNMAP, DISC_OPEN = 0x10004ef8, 0x1000505c, 0x10004a90
STRING_CTOR = 0x10010328         # TbDynamicStringTemplate<char>(const char*)
DISC_STORAGE_VT, FILE_VT = 0x10071e88, 0x10071dec
LBFILE = {"LbFile_ToProjectFilename": 0x10001590, "LbFile_CreateDirectory": 0x100001d0,
          "LbFile_DeleteDirectory": 0x10000274, "LbFile_SetAttributes": 0x10000798,
          "LbFile_IsDirectory": 0x1000099c, "LbFile_Delete": 0x10000a20, "LbFile_Rename": 0x10000d5c}


def F(lib, name, restype, *args):
    f = lib[name]
    f.restype, f.argtypes = restype, args
    return f


def w32(e, a, v):
    e.write(a, struct.pack(">I", T.u32(v)))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    s = e.da = CL.Stubs()
    rec = lambda *a: e.cl.rec(*a)
    ret = lambda *a: e.cl.ret(*a)

    def fill_find(e, data):
        raw = bytearray(0x140)
        struct.pack_into(">I", raw, 0, T.u32(ret("Find.attributes", 0x80)))
        raw[4:28] = bytes((i + 1 + e.da_find) & 0xff for i in range(24))
        struct.pack_into(">II", raw, 28, 0x11, T.u32(ret("Find.size", 1234)))
        name = b"found%d.dat\0" % e.da_find
        raw[44:44 + len(name)] = name
        e.write(data, bytes(raw))
        e.da_find += 1

    def result(fn):
        """A fake whose return value goes back in r3."""
        return lambda e: e.set_r(3, T.u32(fn(e)))

    def tb(fn):
        """A fake returning a TbError through the hidden buffer in r3."""
        return lambda e: w32(e, e.r(3), fn(e))

    def create_file(e):
        rec("CreateFile", (e.r(4), e.r(5), e.r(6), e.r(7), e.r(8), e.r(9)), (e.cstr(e.r(3)),))
        return ret("CreateFile", 0x5001)

    def read_file(e):
        handle, buffer, count, done = e.r(3), e.r(4), e.r(5), e.r(6)
        rec("ReadFile", (handle, count, e.r(7)))
        n = min(T.u32(ret("ReadFile.done", count)), count)
        if n:
            e.write(buffer, bytes((i * 7 + 3) & 0xff for i in range(n)))
        w32(e, done, n)
        return ret("ReadFile", 1)

    def write_file(e):
        handle, buffer, count, done = e.r(3), e.r(4), e.r(5), e.r(6)
        total = 0
        for byte in e.read(buffer, count):
            total = (total * 31 + byte) & 0xffffffff
        rec("WriteFile", (handle, count, total, e.r(7)))
        w32(e, done, ret("WriteFile.done", count))
        return ret("WriteFile", 1)

    def find_first(e):
        rec("FindFirstFile", (), (e.cstr(e.r(3)),))
        fill_find(e, e.r(4))
        return ret("FindFirstFile", 0x6001)

    def find_next(e):
        rec("FindNextFile", (e.r(3),))
        fill_find(e, e.r(4))
        return ret("FindNextFile", 1)

    def system_info(e):
        raw = bytearray(36)
        struct.pack_into(">I", raw, 28, T.u32(ret("Granularity", 0x10000)))
        e.write(e.r(3), bytes(raw))

    def interlocked(step):
        def fake(e):
            value = T.u32(T.s32(e.u32(e.r(3))) + step)
            w32(e, e.r(3), value)
            e.set_r(3, value)
        return fake

    imports = {
        "GetFileAttributes": result(lambda e: (rec("GetFileAttributes", (), (e.cstr(e.r(3)),)),
                                               ret("GetFileAttributes", 0x10))[1]),
        "CreateFile": result(create_file),
        "CloseHandle": result(lambda e: (rec("CloseHandle", (e.r(3),)), 1)[1]),
        "ReadFile": result(read_file),
        "WriteFile": result(write_file),
        "SetFilePointer": result(lambda e: (rec("SetFilePointer", (e.r(3), T.s32(e.r(4)), e.r(5), e.r(6))),
                                            ret("SetFilePointer", e.r(4)))[1]),
        "GetFileSize": result(lambda e: (rec("GetFileSize", (e.r(3), e.r(4))), ret("GetFileSize", 1000))[1]),
        "FlushFileBuffers": result(lambda e: (rec("FlushFileBuffers", (e.r(3),)), ret("FlushFileBuffers", 1))[1]),
        "GetMacFileReference": result(lambda e: (rec("GetMacFileReference", (e.r(3),)),
                                                 ret("GetMacFileReference", 7))[1]),
        "CreateFileMapping": result(lambda e: (rec("CreateFileMapping", (e.r(3), e.r(4), e.r(5), e.r(6), e.r(7), e.r(8))),
                                               ret("CreateFileMapping", 0x7001))[1]),
        "MapViewOfFileEx": result(lambda e: (rec("MapViewOfFileEx", (e.r(3), e.r(4), e.r(5), e.r(6), e.r(7), e.r(8))),
                                             ret("MapViewOfFileEx", 0x200000))[1]),
        "UnmapViewOfFile": result(lambda e: (rec("UnmapViewOfFile", (e.r(3),)), 1)[1]),
        "GetSystemInfo": system_info,
        "FindFirstFile": result(find_first),
        "FindNextFile": result(find_next),
        "FindClose": result(lambda e: (rec("FindClose", (e.r(3),)), 1)[1]),
        "InterlockedIncrement": interlocked(1),
        "InterlockedDecrement": interlocked(-1),
        "IsDBCSLeadByte": result(lambda e: 0),
    }
    found = set()
    for info in e.imports.values():
        base = info["name"].split("__")[0]
        if base in imports and "NS_MacDoze" in info["name"]:
            s.imp(info["name"], imports[base])
            found.add(base)
    assert found == set(imports), sorted(set(imports) - found)

    def to_project(e):
        name = e.cstr(e.r(6))
        rec("LbFile_ToProjectFilename", (e.r(5),), (name,))
        e.write(e.r(4), b"P:" + name.encode("latin-1") + b"\0")
        return 0

    s.internal(LBFILE["LbFile_ToProjectFilename"], tb(to_project))
    for name in ("LbFile_CreateDirectory", "LbFile_DeleteDirectory", "LbFile_Delete"):
        s.internal(LBFILE[name], tb(lambda e, name=name: (rec(name, (), (e.cstr(e.r(4)),)), ret(name, 0))[1]))
    s.internal(LBFILE["LbFile_SetAttributes"],
               tb(lambda e: (rec("LbFile_SetAttributes", (e.r(5),), (e.cstr(e.r(4)),)), ret("LbFile_SetAttributes", 0))[1]))
    s.internal(LBFILE["LbFile_Rename"],
               tb(lambda e: (rec("LbFile_Rename", (), (e.cstr(e.r(4)), e.cstr(e.r(5)))), ret("LbFile_Rename", 0))[1]))
    s.internal(LBFILE["LbFile_IsDirectory"],
               result(lambda e: (rec("LbFile_IsDirectory", (), (e.cstr(e.r(3)),)), ret("LbFile_IsDirectory", 0))[1]))
    def strncpy(e):
        """MSL strncpy: at most n characters, padded with NULs."""
        dst, src, n = e.r(3), e.r(4), e.r(5)
        text = e.cstr(src, limit=1 << 16).encode("latin-1")[:n]
        if n:
            e.write(dst, text + b"\0" * (n - len(text)))

    def strncat(e):
        """MSL strncat: appends at most n characters and terminates."""
        dst, src, n = e.r(3), e.r(4), e.r(5)
        old = e.cstr(dst, limit=1 << 16).encode("latin-1")
        e.write(dst + len(old), e.cstr(src, limit=1 << 16).encode("latin-1")[:n] + b"\0")

    def strcat(e):
        dst, src = e.r(3), e.r(4)
        old = e.cstr(dst, limit=1 << 16).encode("latin-1")
        e.write(dst + len(old), e.cstr(src, limit=1 << 16).encode("latin-1") + b"\0")

    s.imp("strncpy", strncpy)
    s.imp("strncat", strncat)
    s.imp("strcat", strcat)
    # Everything else in the library runs as the original, whatever other modules faked.
    s.real.update(f["entry"] for f in e.functions if f["entry"] not in set(LBFILE.values()))


def text_arg(e, text):
    return e.alloc(len(text) + 1, text.encode("latin-1") + b"\0")


def hidden(e, entry, args):
    """Calls a function that returns a TbError through a hidden buffer."""
    out = e.alloc(4, b"\xee" * 4)
    T.call_ppc(e, entry, [out] + args)
    return T.s32(e.u32(out))


def ppc(e, c):
    e.da_find = 0
    e.cl = CL.PpcLog({}, c["queues"])
    saved = e.da.activate(e)
    try:
        storage = e.alloc(8, struct.pack(">II", DISC_STORAGE_VT, 0))
        file = e.alloc(8, b"\0" * 8)
        T.call_ppc(e, FILE_CTOR, [file])
        info = e.alloc(0x128, b"\0" * 0x128)
        w32(e, info + 0x20, info + 0x24)
        steps = []
        for op, a, b, s1, s2, data in c["steps"]:
            text, io = "", b""
            if op == 0:
                value = hidden(e, STORAGE_OPEN, [storage, text_arg(e, s1)])
            elif op in (1, 2):
                if op == 1:
                    value = hidden(e, FIND_FIRST, [storage, text_arg(e, s1), info])
                else:
                    value = hidden(e, FIND_NEXT, [storage, info])
                size, attributes = struct.unpack(">II", e.read(info, 8))
                io = struct.pack("<II", size, attributes) + e.read(info + 8, 24)
                text = e.cstr(e.u32(info + 0x20)) + "|" + e.cstr(info + 0x24)
            elif op == 3:
                value = hidden(e, OPEN_FILE, [storage, file, text_arg(e, s1), T.u32(a), 0])
            elif op == 4:
                buffer = e.alloc(a + 8, b"\x5a" * (a + 8))
                value = T.s32(T.call_ppc(e, FILE_READ, [file, buffer, a])[0])
                io = e.read(buffer, a + 8)
            elif op == 5:
                buffer = e.alloc(len(data) + 1, bytes(data) + b"\0")
                value = T.s32(T.call_ppc(e, FILE_WRITE, [file, buffer, a])[0])
            elif op == 6:
                value = T.s32(T.call_ppc(e, FILE_SEEK, [file, T.u32(a), b])[0])
            elif op in (7, 8, 11):
                value = T.s32(T.call_ppc(e, {7: FILE_SIZE, 8: FILE_POS, 11: FILE_MACREF}[op], [file])[0])
            elif op == 9:
                value = hidden(e, FILE_FLUSH, [file])
            elif op == 10:
                value = T.call_ppc(e, FILE_MAP, [file])[0]
            elif op == 12:
                T.call_ppc(e, FILE_CLOSE, [file])
                value = 0
            elif op == 13:
                buffer = e.alloc(400, b"z" * 399 + b"\0")
                value = hidden(e, FULL_PATH_BUFFER, [storage, buffer, text_arg(e, s1), T.u32(a)])
                text = e.cstr(buffer)
            elif op == 14:
                out, name = e.alloc(20, b"\0" * 20), e.alloc(20, b"\0" * 20)
                T.call_ppc(e, STRING_CTOR, [out, text_arg(e, "previous")])
                T.call_ppc(e, STRING_CTOR, [name, text_arg(e, s1)])
                value = hidden(e, FULL_PATH_STRING, [storage, out, name])
                text = e.cstr(e.u32(out + 8))
            elif op == 15:
                root = T.call_ppc(e, ROOT_PATH, [storage])[0]
                value, text = 0, e.cstr(root) if root else "(null)"
            elif op == 16:
                value = hidden(e, SET_ATTRIBS, [storage, text_arg(e, s1), T.u32(a)])
            elif op in (17, 18):
                value = hidden(e, CREATE_DIR if op == 17 else DELETE, [storage, text_arg(e, s1)])
            elif op == 19:
                value = hidden(e, MOVE, [storage, text_arg(e, s1), text_arg(e, s2)])
            elif op == 20:
                T.call_ppc(e, STORAGE_CLOSE, [storage])
                value = 0
            elif op == 21:
                copy = e.alloc(8, b"\0" * 8)
                T.call_ppc(e, FILE_COPY, [copy, file])
                value = T.s32(T.call_ppc(e, FILE_POS, [copy])[0])
                T.call_ppc(e, FILE_DTOR, [copy, 0])
            elif op == 22:
                other = e.alloc(8, b"\0" * 8)
                T.call_ppc(e, FILE_CTOR, [other])
                T.call_ppc(e, FILE_ASSIGN, [other, file])
                value = T.s32(T.call_ppc(e, FILE_POS, [other])[0])
                T.call_ppc(e, FILE_DTOR, [other, 0])
            elif op == 23:
                copy = e.alloc(8, b"\0" * 8)
                T.call_ppc(e, STORAGE_COPY, [copy, storage])
                root = T.call_ppc(e, ROOT_PATH, [copy])[0]
                value, text = 0, e.cstr(root) if root else "(null)"
                T.call_ppc(e, STORAGE_DTOR, [copy, 0])
            elif op == 24:
                access = e.u32(file + 4)
                value = T.call_ppc(e, DISC_MAP, [access, T.u32(a), T.u32(b)])[0]
                T.call_ppc(e, DISC_UNMAP, [access, value])
            elif op == 25:
                value = hidden(e, DISC_OPEN, [e.u32(file + 4), text_arg(e, s1), T.u32(a)])
            else:
                other = e.alloc(8, b"\0" * 8)
                T.call_ppc(e, FILE_CTOR, [other])
                hidden(e, OPEN_FILE, [storage, other, text_arg(e, s1), T.u32(a), 0])
                T.call_ppc(e, FILE_ASSIGN, [other, file])
                value = T.s32(T.call_ppc(e, FILE_POS, [other])[0])
                T.call_ppc(e, FILE_DTOR, [other, 0])
            steps.append((value, text, io))
        T.call_ppc(e, FILE_DTOR, [file, 0])
        T.call_ppc(e, STORAGE_DTOR, [storage, 0])
        return dict(steps=steps, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, c):
    step = F(lib, "tpw_da_step", C.c_int64, C.c_int32, C.c_int32, C.c_int32, C.c_char_p, C.c_char_p, C.c_void_p,
             C.c_void_p)
    CL.host_begin(lib, {}, c["queues"])
    F(lib, "tpw_da_begin", None)()
    steps = []
    for op, a, b, s1, s2, data in c["steps"]:
        size = 32 if op in (1, 2) else a + 8 if op == 4 else len(data) + 1
        io = C.create_string_buffer(bytes(data) + b"\0" if op == 5 else b"\x5a" * size, size)
        text = C.create_string_buffer(600)
        value = step(op, a, b, s1.encode("latin-1"), s2.encode("latin-1"), io, text)
        if op in (10, 24):
            value &= 0xffffffff
        steps.append((value, text.value.decode("latin-1"), io.raw if op in (1, 2, 4) else b""))
    F(lib, "tpw_da_end", None)()
    return dict(steps=steps, log=CL.host_end(lib))


NAMES = ["data.bin", "ui:menu.wad", "sub/file.txt", "a", "save:users:1Tess:gms.dat"]
PATTERNS = ["*", "*.sam", "levels:*", "levels/jungle/*.wad", "x"]
ROOTS = ["Disk:Theme Park Data:data:", "Disk:folder", "root/", "r"]


def case(r):
    """A random script that keeps to what the original survives: no copy of a file that
    is not open (it dereferences the missing access object), no partial map or re-open
    without one. Most scripts start with an open storage and an open file, since that is
    where nearly all the code is."""
    steps, queues = [], {}
    opened = file_open = False

    def add(op, a=0, b=0, s1="", s2="", data=b""):
        steps.append((op, a, b, s1, s2, data))

    def open_storage(ok):
        nonlocal opened
        queues.setdefault("GetFileAttributes", []).append(0x10 if ok else 0xffffffff)
        add(0, s1=r.choice(ROOTS))
        opened = ok    # opening again closes the old storage first, successful or not

    def open_file(ok):
        nonlocal file_open
        mode = r.choice([0x80000001, 0x80000001, 0xc0000010, 0xc0000030, 0x40000050, 0xc0000003])
        if opened and not file_open:
            handle = r.choice([0x5001, 0x5002, 0x5003]) if ok else -1
            queues.setdefault("CreateFile", []).append(handle)
            file_open = handle != -1
        add(3, a=mode, s1=r.choice(NAMES))

    if r.random() < 0.8:
        open_storage(True)
        open_file(True)
    file_ops = [4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11, 21, 22, 24, 24, 25, 26]
    for _ in range(r.randint(4, 16)):
        if file_open and r.random() < 0.6:
            op = r.choice(file_ops)
        else:
            op = r.choice([0, 1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23])
        if op == 0:
            open_storage(r.random() < 0.85)
        elif op == 1:
            pattern = r.choice(PATTERNS + [""])
            if opened and pattern:
                queues.setdefault("FindFirstFile", []).append(r.choice([0x6001, 0x6002, -1]))
            add(1, s1=pattern)
        elif op == 3:
            open_file(r.random() < 0.8)
        elif op == 4:
            size = r.choice([0, 1, 7, 16, 40])
            if file_open and size:
                queues.setdefault("ReadFile.done", []).append(r.choice([size, size, size // 2, 0]))
            add(4, a=size)
        elif op == 5:
            data = r.randbytes(r.choice([0, 1, 5, 24]))
            if file_open and data:
                queues.setdefault("WriteFile.done", []).append(r.choice([len(data), len(data), len(data) // 2]))
            add(5, a=len(data), data=data)
        elif op == 6:
            add(6, a=r.choice([0, 5, 100, -3, 0x7fffffff]), b=r.randrange(3))
        elif op == 9:
            if file_open:
                queues.setdefault("FlushFileBuffers", []).append(r.choice([1, 1, 0]))
            add(9)
        elif op in (10, 24):
            a, b = r.choice([(0, 0), (0x8000, 16), (0x10000, 4), (0x10001, 4), (0x34567, 0x100), (5, 0)]) if op == 24 else (0, 0)
            if file_open:
                queues.setdefault("CreateFileMapping", []).append(r.choice([0x7001, 0x7001, 0x7001, 0]))
                queues.setdefault("MapViewOfFileEx", []).append(r.choice([0x200000, 0x300000, 0x300000, 0]))
            add(op, a=a, b=b)
        elif op == 12:
            file_open = False
            add(12)
        elif op == 13:
            add(13, a=r.choice([400, 40, 12, 3, 1]), s1=r.choice(NAMES))
        elif op in (14, 16, 17, 18):
            if op == 18 and opened:
                queues.setdefault("LbFile_IsDirectory", []).append(r.choice([0, 0, 0x10]))
            add(op, a=r.getrandbits(8), s1=r.choice(NAMES))
        elif op == 19:
            add(19, s1=r.choice(NAMES), s2=r.choice(NAMES))
        elif op == 20:
            opened = False
            add(20)
        elif op == 25:
            # The access object opened again: it closes its old file first, and a failure
            # leaves it without a handle (the file object still holds the access object).
            queues.setdefault("CreateFile", []).append(r.choice([0x5004, 0x5005, -1]))
            add(25, a=r.choice([0x80000001, 0xc0000010]), s1=r.choice(NAMES))
        elif op == 26:
            # A second file opened in the same storage, then assigned the first file's access.
            if opened:
                queues.setdefault("CreateFile", []).append(r.choice([0x5006, 0x5007, -1]))
            add(26, a=0x80000001, s1=r.choice(NAMES))
        else:
            add(op)
    for name in ("LbFile_Delete", "LbFile_DeleteDirectory", "LbFile_CreateDirectory", "LbFile_SetAttributes",
                 "LbFile_Rename"):
        queues[name] = [r.choice([0, 0, -1]) for _ in range(4)]
    queues["FindNextFile"] = [r.choice([1, 1, 0]) for _ in range(8)]
    queues["Find.size"] = [r.getrandbits(32) for _ in range(8)]
    queues["Find.attributes"] = [r.choice([0x80, 0x10, 0x21]) for _ in range(8)]
    queues["Granularity"] = [r.choice([0x10000, 0x10000, 0x1000])]
    queues["GetFileSize"] = [r.choice([1000, 0, 0xffffffff])]
    queues["SetFilePointer"] = [r.choice([0, 5, 100, 0xffffffff]) for _ in range(6)]
    return dict(steps=steps, queues=queues)


def edges():
    """The game's own sequences."""
    listing = [(0, 0, 0, "Disk:Theme Park Data:data:language", "", b""), (15, 0, 0, "", "", b""),
               (1, 0, 0, "*", "", b""), (2, 0, 0, "", "", b""), (2, 0, 0, "", "", b""), (2, 0, 0, "", "", b""),
               (13, 400, 0, "American", "", b""), (14, 0, 0, "ResIdx.dat", "", b""), (23, 0, 0, "", "", b"")]
    reading = [(0, 0, 0, "Disk:Theme Park Data:data:", "", b""), (3, 0x80000001, 0, "sound.sam", "", b""),
               (7, 0, 0, "", "", b""), (4, 16, 0, "", "", b""), (8, 0, 0, "", "", b""), (6, 4, 0, "", "", b""),
               (4, 8, 0, "", "", b""), (21, 0, 0, "", "", b""), (22, 0, 0, "", "", b""), (10, 0, 0, "", "", b""),
               (10, 0, 0, "", "", b""), (24, 0x34567, 0x100, "", "", b""), (11, 0, 0, "", "", b""),
               (9, 0, 0, "", "", b""), (3, 0x80000001, 0, "again.sam", "", b""), (12, 0, 0, "", "", b""),
               (4, 4, 0, "", "", b""), (20, 0, 0, "", "", b""), (3, 0x80000001, 0, "closed.sam", "", b"")]
    saving = [(0, 0, 0, "Disk:save", "", b""), (17, 0, 0, "users", "", b""), (3, 0xc0000010, 0, "config.tcf", "", b""),
              (5, 5, 0, "", "", b"hello"), (8, 0, 0, "", "", b""), (12, 0, 0, "", "", b""),
              (16, 0x80, 0, "config.tcf", "", b""), (19, 0, 0, "config.tcf", "old.tcf", b""),
              (18, 0, 0, "old.tcf", "", b""), (18, 0, 0, "users", "", b"")]
    closed = [(1, 0, 0, "*", "", b""), (2, 0, 0, "", "", b""), (3, 1, 0, "x", "", b""), (13, 40, 0, "x", "", b""),
              (14, 0, 0, "x", "", b""), (15, 0, 0, "", "", b""), (16, 0, 0, "x", "", b""), (17, 0, 0, "x", "", b""),
              (18, 0, 0, "x", "", b""), (19, 0, 0, "x", "y", b""), (4, 4, 0, "", "", b""), (5, 1, 0, "", "", b"q"),
              (6, 0, 0, "", "", b""), (7, 0, 0, "", "", b""), (8, 0, 0, "", "", b""), (9, 0, 0, "", "", b""),
              (10, 0, 0, "", "", b""), (11, 0, 0, "", "", b""), (22, 0, 0, "", "", b""), (23, 0, 0, "", "", b"")]
    return [dict(steps=listing, queues={"FindNextFile": [1, 1, 0]}),
            dict(steps=reading, queues={}),
            dict(steps=saving, queues={"LbFile_IsDirectory": [0, 0x10]}),
            dict(steps=closed, queues={}),
            dict(steps=[(0, 0, 0, "nowhere", "", b""), (15, 0, 0, "", "", b"")],
                 queues={"GetFileAttributes": [0xffffffff]}),
            dict(steps=reading[:3], queues={"CreateFile": [-1]})]


# The scripts stand for some forty functions, so they run five times over.
TESTS = [FnTest("bullfrog disc access scripts (%d)" % (i + 1), ppc, host, edge=edges if i == 0 else None, rnd=case)
         for i in range(5)]
