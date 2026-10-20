"""Unit w221: 47 bullfrog registry/file entries, original versus recording host fakes.

The outside registry is a configurable value, subkey and enumeration tree. Each API
has forced-answer queues. Functional C runtime fakes and strings are unrecorded.
We compare guards, handles, outputs, file state, allocation bytes and call logs.
No floats. Excluded: null strlen/path inputs, overflowing fixed paths, roots that
never exist, and GUID text that leaves sscanf's stack-only temporaries uninitialized.
The malformed GUID printf suffix is rendered by the host libc on both sides.
"""
import ctypes as C
import random
import re
import struct
import calllog as CL
import tpwtest as T

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_RegistryFiles"
SPECS = [
    (0x10001e60, "TbRegistryKey::copy", 52), (0x10001ed0, "TbRegistryKey::operator=", 52),
    (0x10001f40, "TbRegistryKey::OpenBullfrogGameKey", 256), (0x1000207c, "TbRegistryKey::OpenEAGameKey", 256),
    (0x100021b4, "TbRegistryKey::OpenKey", 284), (0x10002300, "TbRegistryKey::CreateKey", 288),
    (0x10002454, "TbRegistryKey::~TbRegistryKey", 96), (0x100024e0, "TbRegistryKey::CloseKey", 104),
    (0x10002578, "TbRegistryKey::Initialise", 12), (0x100025b4, "TbRegistryKey::_OpenSubKey", 80),
    (0x1000263c, "TbRegistryKey::_CreateSubKey", 116), (0x100026e8, "TbRegistryKey::SetValue", 144),
    (0x100027c0, "TbRegistryKey::GetValue", 148), (0x1000288c, "TbRegistryKey::GetValueSize", 100),
    (0x10002928, "TbRegistryKey::SetValueGUID", 184), (0x10002a2c, "TbRegistryKey::GetValueGUID", 416),
    (0x10002c18, "TbRegistryKey::GetValueGUID_default", 176), (0x10002d28, "TbRegistryKey::SetValueString", 180),
    (0x10002e18, "TbRegistryKey::GetValueString", 440), (0x10003010, "TbRegistryKey::SetValueULONG", 152),
    (0x100030e0, "TbRegistryKey::GetValueULONG", 332), (0x10003268, "TbRegistryKey::GetValueULONG_default", 424),
    (0x1000344c, "TbRegistryKey::OpenSubKey", 288), (0x100035b0, "TbRegistryKey::CreateSubKey", 304),
    (0x10003724, "TbRegistryKey::GetItemType", 148), (0x100037ec, "TbRegistryKey::default", 12),
    (0x10003824, "TbRegistryKey::ListItems", 668), (0x10003b04, "TbRegistryKey::DeleteItem", 320),
    (0x10000048, "LbFile_GetCurrentDrive", 104), (0x100000e0, "LbFile_GetFreeDriveSpace", 176),
    (0x1000046c, "LbFile_Flush", 56), (0x100004c8, "LbFile_GetPosition", 44),
    (0x100005c8, "LbFile_GetFileStamp", 12), (0x1000062c, "LbFile_SetFileStamp", 12),
    (0x10000ab0, "LbFile_FindFirst", 308), (0x10000c1c, "LbFile_FindNext", 156),
    (0x10000cec, "LbFile_FindEnd", 60), (0x10001228, "LbFile_MakePath", 616),
    (0x10001758, "LbFile_GetProjectFilename", 128), (0x10001994, "LbFile_AllocLoad", 436),
    (0x10001b78, "LbFile_Save", 280), (0x10001e04, "LbFile_Open_ptrptr", 48),
    (0x10013dec, "LbMem_ReAlloc", 140), (0x10001520, "LB_MAX", 24),
    (0x100014b8, "TbError::copy", 12), (0x100014f0, "TbError::operator_long", 8),
    (0x10001564, "TbError::value", 8),
]
MASK = 0xffffffff
ROOTS = ["HKEY_CLASSES_ROOT", "HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE", "HKEY_USERS",
         "HKEY_PERFORMANCE_DATA", "HKEY_CURRENT_CONFIG", "HKEY_DYN_DATA"]


def w32(e, a, x):
    e.write(a, struct.pack(">I", x & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "rgf_bound", False):
        return
    lib.tpw_rgf_addr.argtypes = [C.c_int32]
    lib.tpw_rgf_addr.restype = C.c_uint64
    lib.tpw_rgf_layout.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_rgf_call.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p,
                                C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p]
    lib.tpw_rgf_call.restype = C.c_int64
    lib.tpw_rgf_label.argtypes = [C.c_void_p, C.c_int64]
    lib.tpw_rgf_queue.argtypes = [C.c_char_p, C.POINTER(C.c_int64), C.c_uint32]
    lib.tpw_rgf_events.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_rgf_events.restype = C.c_void_p
    lib.rgf_bound = True


def stubs(s):
    """The Mac fakes record all arguments, including the register/stack tail."""
    e, c = s.e, s.c
    st = CL.Stubs()
    st.real.update(a for a, _, _ in SPECS)
    cfg = c["cfg"]
    def rec(name, ints=(), texts=()):
        s.events.append((name, tuple(ints), tuple(texts)))
    def txt(p):
        return e.cstr(p) if p else "(null)"
    def regarg(n):
        return e.r(n + 2) if n <= 8 else e.u32(e.r(1) + 0x38 + 4 * (n - 9))
    def close(em):
        rec("Close", [em.r(3)])
        em.set_r(3, s.L.ret("Close") & MASK)
    def opened(kind):
        def fake(em):
            if kind == "Create":
                a = [regarg(i) for i in range(1, 10)]
                rec(kind, [a[0], a[2], bool(a[3]), a[4], a[5], bool(a[6]), bool(a[7]), bool(a[8])], [txt(a[1])])
                out = a[7]
            elif kind == "OpenEx":
                rec(kind, [em.r(3), em.r(5), em.r(6), bool(em.r(7))], [txt(em.r(4))])
                out = em.r(7)
            else:
                rec(kind, [em.r(3), bool(em.r(5))], [txt(em.r(4))])
                out = em.r(5)
            r = s.L.ret(kind)
            if r == 0:
                w32(em, out, cfg[0])
            em.set_r(3, r & MASK)
        return fake
    def setvalue(em):
        h, n, reserved, ty, data, size = [em.r(i) for i in range(3, 9)]
        rec("Set", [h, reserved, ty, size, bool(data)], [txt(n), em.read(data, size).hex()])
        em.set_r(3, s.L.ret("Set") & MASK)
    def query(em):
        h, n, reserved, ty, data, size = [em.r(i) for i in range(3, 9)]
        available = em.u32(size) if data and size else 0
        rec("Query", [h, bool(reserved), bool(ty), bool(data), bool(size), available], [txt(n)])
        r = s.L.ret("Query")
        if r == 0:
            if ty:
                w32(em, ty, cfg[1])
            if size:
                w32(em, size, cfg[2])
            if data and available < cfg[2]:
                r = 234
            elif data:
                payload = struct.pack(">I", cfg[3]) if 4 <= cfg[1] <= 6 and cfg[2] == 4 else c["payload"]
                em.write(data, payload[:cfg[2]])
        em.set_r(3, r & MASK)
    def info(em):
        a = [regarg(i) for i in range(1, 13)]
        rec("Info", [a[0]] + [bool(v) for v in a[1:]])
        r = s.L.ret("Info")
        if r == 0:
            for p, v in zip([a[4], a[5], a[7], a[8]], cfg[4:8]):
                w32(em, p, v)
        em.set_r(3, r & MASK)
    def enum(kind):
        def fake(em):
            a = [em.r(i) for i in range(3, 11)]
            rec(kind, [a[0], a[1], em.u32(a[3])] + [bool(v) for v in a[4:]])
            r = s.L.ret(kind)
            if r == 0:
                text = ("child" if a[1] == 0 else "long-key") if kind == "EnumKey" else "value%d" % a[1]
                em.write(a[2], text.encode() + b"\0")
                w32(em, a[3], len(text))
                if kind == "EnumValue":
                    w32(em, a[5], a[1] if a[1] < 8 else MASK)
            em.set_r(3, r & MASK)
        return fake
    def deleted(kind):
        def fake(em):
            rec(kind, [em.r(3)], [txt(em.r(4))])
            em.set_r(3, s.L.ret(kind) & MASK)
        return fake
    for n, fn in [("RegCloseKey", close), ("RegOpenKeyExA", opened("OpenEx")),
                  ("RegOpenKeyA", opened("Open")), ("RegCreateKeyExA", opened("Create")),
                  ("RegSetValueExA", setvalue), ("RegQueryValueExA", query),
                  ("RegQueryInfoKeyA", info), ("RegEnumKeyExA", enum("EnumKey")),
                  ("RegEnumValueA", enum("EnumValue")), ("RegDeleteKeyA", deleted("DeleteKey")),
                  ("RegDeleteValueA", deleted("DeleteValue"))]:
        st.imp(n, fn)
    def compare(em):
        a, b, n = txt(em.r(3)), txt(em.r(4)), em.r(5)
        rec("Compare", [n], [a, b])
        em.set_r(3, (0 if a[:n].lower() == b[:n].lower() else 1))
    def disk(em):
        rec("Disk", [bool(em.r(i)) for i in range(4, 8)], [txt(em.r(3))])
        for reg, val in zip(range(4, 8), cfg[8:12]):
            w32(em, em.r(reg), val)
        em.set_r(3, s.L.ret("Disk") & MASK)
    def volume(em):
        rec("Volume", [bool(em.r(i)) for i in range(3, 6)])
        em.write(em.r(3), bytes([cfg[14]]))
        em.set_r(3, s.L.ret("Volume") & MASK)
    def upper(em):
        rec("Upper", [T.s32(em.r(3)), em.r(4)])
    def pascal(em):
        rec("Pascal", [T.s32(em.r(3))])
        em.set_r(3, cfg[15])
    def findfirst(em):
        rec("FindFirst", [bool(em.r(4))], [txt(em.r(3))])
        w32(em, em.r(4), cfg[12])
        em.write(em.r(4) + 0x2c, b"first.dat\0")
        em.set_r(3, s.L.ret("FindFirst") & MASK)
    def findnext(em):
        rec("FindNext", [em.r(3), bool(em.r(4))])
        r = s.L.ret("FindNext")
        if r:
            w32(em, em.r(4), cfg[13])
            em.write(em.r(4) + 0x2c, b"next.bin\0")
        em.set_r(3, r & MASK)
    def findclose(em):
        rec("FindClose", [em.r(3)])
        em.set_r(3, s.L.ret("FindClose") & MASK)
    for n, fn in [("_strnicmp__10NS_MacDozeFPCcPCci", compare),
                  ("GetDiskFreeSpace__10NS_MacDozeFPCcPUlPUlPUlPUl", disk),
                  ("HGetVol", volume), ("UpperString", upper), ("p2cstr", pascal),
                  ("FindFirstFile__10NS_MacDozeFPCcPQ210NS_MacDoze17_WIN32_FIND_DATAA", findfirst),
                  ("FindNextFile__10NS_MacDozeFPQ210NS_MacDoze14InternalHandlePQ210NS_MacDoze17_WIN32_FIND_DATAA", findnext),
                  ("FindClose__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle", findclose)]:
        st.imp(n, fn)
    def allocate(em):
        rec("Alloc", [em.r(3)])
        em.set_r(3, s.globals[3] if s.L.ret("Alloc") else 0)
    def free(em):
        rec("Free", [s.L.lab(em.r(3))])
    def project(em):
        rec("Project", [s.L.lab(em.r(4)), em.r(5)], [txt(em.r(6))])
        em.write(em.r(4), ("App:" + (txt(em.r(6)) if em.r(6) else "default")).encode() + b"\0")
        w32(em, em.r(3), s.L.ret("Project"))
    def exists(em):
        n = txt(em.r(3))
        rec("Exists", [], [n])
        r = int(n in ("Disk", "Disk:", "Disk:/")) if cfg[16] else s.L.ret("Exists")
        em.set_r(3, r & 255)
    def length(em):
        rec("Length", [], [txt(em.r(3))])
        em.set_r(3, s.L.ret("Length") & MASK)
    def directory(em):
        rec("Directory", [], [txt(em.r(4))])
        w32(em, em.r(3), s.L.ret("Directory"))
    def fileopen(em):
        rec("FileOpen", [em.r(6)], [txt(em.r(5))])
        r = s.L.ret("FileOpen")
        if r >= 0:
            w32(em, em.r(4), 0x6000)
        w32(em, em.r(3), r)
    def io(kind):
        def fake(em):
            ints = [em.r(4), s.L.lab(em.r(5)), em.r(6), bool(em.r(7))]
            rec(kind, ints, [em.read(em.r(5), em.r(6)).hex()] if kind == "Write" else [])
            if kind == "Read" and em.r(6):
                em.write(em.r(5), b"\x7b")
            w32(em, em.r(3), s.L.ret(kind))
        return fake
    def fileclose(em):
        rec("FileClose", [em.r(4)])
        w32(em, em.r(3), s.L.ret("FileClose"))
    def ctor(em):
        w32(em, em.r(3) + 8, em.r(4))
        w32(em, em.r(3) + 4, len(txt(em.r(4))))
    def add(em):
        s.items.append(txt(em.u32(em.r(4) + 8)))
    def position(em):
        rec("Position", [s.L.lab(em.r(3))])
        em.set_r(3, s.L.ret("Position") & MASK)
    def flush(em):
        rec("Flush", [s.L.lab(em.r(4))])
        w32(em, em.r(3), s.L.ret("Flush"))
    for a, fn in [(0x10013ea4, allocate), (0x10013efc, free), (0x10001590, project),
                  (0x10000924, exists), (0x10000894, length), (0x100001d0, directory),
                  (0x10000318, fileopen), (0x10000e84, io("Read")), (0x10000fa0, io("Write")),
                  (0x100003d8, fileclose), (0x10010328, ctor), (0x10003c78, add),
                  (0x1000e2c8, lambda em: None), (0x1000b608, position), (0x1000b694, flush)]:
        st.internal(a, fn)
    st.imp("__dl__FPv", lambda em: rec("Delete", [s.L.lab(em.r(3))]))
    # Match the host libc's rendering of the malformed percent suffix, while checking its exact format.
    def sprintf(em):
        fmt = txt(em.r(4))
        assert fmt == "{%.8X-%.4X-%.4X-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X%}"
        args = [em.r(i) for i in range(5, 11)] + [em.u32(em.r(1) + 0x38 + 4 * i) for i in range(5)]
        out = C.create_string_buffer(128)
        render = C.CDLL(None).snprintf
        render.argtypes = [C.c_void_p, C.c_size_t, C.c_char_p]
        render.restype = C.c_int
        render(out, 128, fmt.encode(), *[C.c_uint32(v) for v in args])
        em.write(em.r(3), out.value + b"\0")
        em.set_r(3, len(out.value))
    def sscanf(em):
        assert txt(em.r(4)) == "{%lx-%hx-%hx-%hx-%4hx%8lx}"
        m = re.match(r"\{([0-9A-Fa-f]+)-([0-9A-Fa-f]+)-([0-9A-Fa-f]+)-([0-9A-Fa-f]+)-([0-9A-Fa-f]{4})([0-9A-Fa-f]{8})", txt(em.r(3)))
        assert m, txt(em.r(3))
        for p, token, width in zip([em.r(i) for i in range(5, 11)], m.groups(), [4, 2, 2, 2, 2, 4]):
            em.write(p, int(token, 16).to_bytes(width, "big"))
        em.set_r(3, 6)
    st.imp("sprintf", sprintf)
    st.imp("sscanf", sscanf)
    # strchr and strncpy are not guaranteed by the common C runtime set.
    def strchr(em):
        raw = txt(em.r(3)).encode("latin-1") + b"\0"
        i = raw.find(bytes([em.r(4) & 255]))
        em.set_r(3, 0 if i < 0 else em.r(3) + i)
    def strncpy(em):
        data = txt(em.r(4)).encode("latin-1")
        n = em.r(5)
        em.write(em.r(3), (data + b"\0" * n)[:n])
    st.imp("strchr", strchr)
    st.imp("strncpy", strncpy)
    return st


class Side:
    """Both memories share block setup and observation; only pointer widths differ."""
    def words(self, a, xs):
        self.write(a, struct.pack(self.endian + "%dI" % len(xs), *[x & MASK for x in xs]))
    def word(self, a):
        return struct.unpack(self.endian + "I", self.read(a, 4))[0]
    def ptr(self, a, x):
        self.write(a, int(x).to_bytes(self.psize, "big" if self.endian == ">" else "little"))
    def rdptr(self, a):
        return int.from_bytes(self.read(a, self.psize), "big" if self.endian == ">" else "little")
    def string(self, text):
        if text is None:
            return 0
        b = text.encode("latin-1") + b"\0"
        a = self.alloc(len(b))
        self.write(a, b)
        return a
    def block(self, n):
        a = self.alloc(n + 16)
        self.write(a, b"\xa5" * (n + 16))
        self.guards.append((a, n))
        return a + 8
    def label(self, p):
        return self.labels.get(p, 0 if not p else -1)


class Ppc(Side):
    endian, psize = ">", 4
    def __init__(self, e, c):
        self.e, self.c = e, c
        self.guards, self.labels, self.items = [], {}, []
        self.globals = [e.alloc(128), e.alloc(128), 0x10073ba0, e.alloc(512), e.alloc(8), e.alloc(512)]
        self.layout = [0x250, 0x144, 0x148, 0x24c]
    def alloc(self, n):
        return self.e.alloc(n)
    def write(self, a, data):
        self.e.write(a, data)
    def read(self, a, n):
        return self.e.read(a, n)
    def start(self):
        self.L = self.e.cl = CL.PpcLog({a: label for a, label in self.labels.items()}, self.c["queues"])
        self.events = []
        upper = self.e.alloc(256, bytes(i - 32 if 97 <= i <= 122 else i for i in range(256)))
        w32(self.e, 0x1006f05c, upper)
        self.st = stubs(self)
        self.saved = self.st.activate(self.e)
        # Original CFile TVectors, replaced above: read the precise descriptors from the vtable.
        vt = self.alloc(0x34)
        for slot, addr in [(0x20, 0x1000b608), (0x28, 0x1000b694)]:
            desc = self.alloc(8)
            self.words(desc, [addr, self.e.r(2)])
            self.words(vt + slot, [desc])
        self.words(self.globals[4], [vt, 0])
    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
    def log(self):
        return self.events


class Host(Side):
    endian, psize = "<", 8
    def __init__(self, lib, c):
        bind(lib)
        self.lib, self.c = lib, c
        self.buffers, self.guards, self.labels, self.items = [], [], {}, []
        self.globals = [lib.tpw_rgf_addr(i) for i in range(6)]
        out = (C.c_uint32 * 4)()
        lib.tpw_rgf_layout(out)
        self.layout = list(out)
        assert self.layout == [600, 328, 332, 592], self.layout
    def alloc(self, n):
        b = C.create_string_buffer(n)
        self.buffers.append(b)
        return C.addressof(b)
    def write(self, a, data):
        C.memmove(a, data, len(data))
    def read(self, a, n):
        return C.string_at(a, n)
    def start(self):
        self.lib.tpw_rgf_begin()
        for a, label in self.labels.items():
            self.lib.tpw_rgf_label(a, label)
        for n, q in self.c["queues"].items():
            values = (C.c_int64 * len(q))(*q)
            self.lib.tpw_rgf_queue(n.encode(), values, len(q))
    def done(self):
        pass
    def log(self):
        class Event(C.Structure):
            _fields_ = [("name", C.c_char * 24), ("ni", C.c_uint32), ("nt", C.c_uint32),
                        ("ints", C.c_int64 * 12), ("texts", (C.c_char * 160) * 3)]
        n = C.c_uint32()
        p = self.lib.tpw_rgf_events(C.byref(n))
        events = C.cast(p, C.POINTER(Event))
        return [(events[i].name.decode(), tuple(events[i].ints[:events[i].ni]),
                 tuple(bytes(events[i].texts[j]).split(b"\0")[0].decode("latin-1") for j in range(events[i].nt)))
                for i in range(n.value)]


def build(s):
    c, op = s.c, s.c["op"]
    s.words(s.globals[0], c["cfg"])
    s.write(s.globals[1], c["payload"].ljust(128, b"\0"))
    s.write(s.globals[2], b"prior\0" + b"\xa5" * 255)
    s.write(s.globals[3], b"\xcc" * 512)
    s.write(s.globals[5], b"\0" * 512)
    s.key, s.sub = s.block(4), s.block(4)
    s.words(s.key, [c["handle"]])
    s.words(s.sub, [c["sub"]])
    s.data, s.aux = s.block(768), s.block(128)
    s.write(s.data, c["data"])
    s.write(s.aux, c["fallback"])
    if op in (20, 21, 44, 45, 46) or (op == 12 and 4 <= c["cfg"][1] <= 6 and c["cfg"][2] == 4) or (op == 11 and 4 <= c["x"] <= 6 and c["y"] == 4):
        s.words(s.data, [c["word"]])
        s.words(s.aux, [c["fallback_word"]])
    if op in (39, 41):
        s.ptr(s.data, 0x7777)
        s.words(s.aux, [0x11223344])
    if op == 29:
        s.words(s.data, [c["word"], c["fallback_word"], c["cfg"][8], c["cfg"][9]])
    if op in (34, 35, 36):
        s.write(s.data, b"\0" * s.layout[0])
        s.words(s.data, [c["cfg"][12]])
        s.write(s.data + 0x2c, b"original.dat\0")
        s.ptr(s.data + 0x140, 0x5555)
        s.words(s.data + s.layout[1], [c["x"]])
        s.write(s.data + s.layout[2], b"prior/name\0")
        s.ptr(s.data + s.layout[3], s.data + s.layout[2] + 6)
    s.name = s.string(c["name"])
    for a, label in [(s.key, 1), (s.sub, 2), (s.data, 3), (s.aux, 4),
                     (s.globals[2], 5), (s.globals[3], 10), (s.globals[4], 6)]:
        s.labels[a] = label
    s.start()


def execute(s):
    c, op = s.c, s.c["op"]
    k = 0 if c["nullkey"] and op == 6 else s.key
    data = 0 if c["nulldata"] and op in (11, 12, 29, 32, 33) else s.data
    aux = 0 if c["nullaux"] and op in (18, 32, 33, 39) else s.aux
    x, y = c["x"], c["y"]
    if isinstance(s, Host):
        return s.lib.tpw_rgf_call(op, k, s.sub, s.name, data, x, y, aux) & MASK
    e = s.e
    result = s.block(4)
    a = SPECS[op][0]
    args = {
        0: [k, s.sub], 1: [k, s.sub], 2: [result, k, s.name, x], 3: [result, k, s.name, x],
        4: [result, k, s.name], 5: [result, k, s.name], 6: [k, x], 7: [result, k], 8: [k],
        9: [s.name, x], 10: [s.name, x, y], 11: [result, k, s.name, x, data, y],
        12: [result, k, s.name, data, x], 13: [k, s.name], 14: [result, k, s.name, data],
        15: [result, k, s.name, data], 16: [result, k, s.name, data, aux],
        17: [result, k, s.name, data], 18: [result, k, s.name, data, x, aux],
        19: [result, k, s.name, x], 20: [result, k, s.name, data], 21: [result, k, s.name, data, x],
        22: [result, k, s.name, s.sub], 23: [result, k, s.name, s.sub], 24: [k, s.name], 25: [k],
        26: [result, k, s.aux, x], 27: [result, k, s.name], 28: [], 29: [x, data],
        30: [result, s.globals[4]], 31: [s.globals[4]], 32: [result, k, data, aux, 0],
        33: [result, k, data, aux, 0], 34: [result, s.name, data, x], 35: [result, data],
        36: [result, data], 37: [result, s.name], 38: [s.name], 39: [result, s.name, data, aux],
        40: [result, s.name, data, x], 41: [result, data, s.name, x], 42: [data, x, y],
        44: [data, aux], 45: [data], 46: [data, x],
    }
    if op == 43:
        slots = s.alloc(8)
        s.words(slots, [x, y])
        r = T.call_ppc(e, a, [slots, slots + 4])[0]
        return 0 if r == slots else 1
    r = T.call_ppc(e, a, args[op])[0] & MASK
    if op in (0, 1, 6, 25, 38, 42, 44, 46):
        return s.label(r) & MASK
    if op == 8:
        return 0
    if op in (9, 10, 13, 24, 28, 29, 31, 45):
        return r
    return s.word(result)


def observe(s, answer):
    op = s.c["op"]
    data = s.read(s.data, 768)
    aux = s.read(s.aux, 128)
    if op in (20, 21, 44, 45, 46) or (op == 12 and 4 <= s.c["cfg"][1] <= 6 and s.c["cfg"][2] == 4) or (op == 11 and 4 <= s.c["x"] <= 6 and s.c["y"] == 4):
        data = [s.word(s.data), data[4:]]
        aux = [s.word(s.aux), aux[4:]]
    if op in (39, 41):
        data = [s.label(s.rdptr(s.data)) if op == 39 and s.rdptr(s.data) != 0x7777 else s.rdptr(s.data), data[s.psize:]]
        # Normalize the native pointer slot width; the rest is untouched guard fill.
        data[1] = data[1][-760:]
        aux = [s.word(s.aux), aux[4:]]
    if op == 29:
        data = [[s.word(s.data + i * 4) for i in range(4)], data[16:]]
    if op in (34, 35, 36):
        base = s.data + s.layout[2]
        data = dict(attrs=s.word(s.data), prefix=s.read(s.data + 4, 40),
                    filename=s.read(s.data + 0x2c, 276), handle=s.rdptr(s.data + 0x140) & MASK,
                    flags=s.word(s.data + s.layout[1]), path=s.read(base, 260),
                    offset=s.rdptr(s.data + s.layout[3]) - base)
    if op == 26 and isinstance(s, Host):
        s.items = C.string_at(s.globals[5]).decode().splitlines()
    guards = [s.read(a, 8) + s.read(a + n + 8, 8) for a, n in s.guards[:4]]
    return dict(answer=answer, keys=[s.read(s.key - 8, 8), s.word(s.key), s.read(s.key + 4, 8),
                                  s.read(s.sub - 8, 8), s.word(s.sub), s.read(s.sub + 4, 8)],
                data=data, aux=aux, process=s.read(s.globals[2], 260), alloc=s.read(s.globals[3], 512),
                items=s.items, guards=guards, log=s.log())


def run(s):
    build(s)
    try:
        return observe(s, execute(s))
    finally:
        s.done()


def case(r, op):
    ty = r.randrange(8)
    cfg = [0x5000 + r.randrange(4), ty, 4 if 4 <= ty <= 6 else r.choice([0, 3, 8]), r.getrandbits(32),
           r.randrange(3), r.choice([8, 12]), r.randrange(10), r.choice([8, 16]),
           r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32),
           r.choice([0, 1, 16, 32, 0x80000000]), r.choice([0, 1, 16, 32]), r.randrange(256), r.getrandbits(32),
           int(op == 37)] + [0] * 15
    queues = {n: [r.choice([0, 0, 0, 5])] for n in ("Close", "OpenEx", "Open", "Create", "Set", "Query", "Info", "EnumKey", "EnumValue", "DeleteKey", "DeleteValue")}
    queues.update({n: [r.choice([-1, 0, 1, 7])] for n in ("Project", "FileOpen", "Read", "Write", "FileClose", "Directory", "Flush", "Position")})
    queues.update(Alloc=[r.randrange(2)], Disk=[r.choice([0, 1, -1])], Volume=[r.choice([0, 0, 1, -1, 0x10000])],
                  FindFirst=[r.choice([-1, 0x5555])], FindNext=[1, 1, 0], FindClose=[-1],
                  Exists=[r.randrange(2)], Length=[r.choice([-1, 0, 4, 17, 64])])
    name = r.choice([None, "value", "child", "", "mixed Name"])
    x, y = r.getrandbits(32), r.getrandbits(32)
    if op in (2, 3, 6):
        x = r.choice([0, 1, 0xffff, 0x10000, 0x8000])
    if op in (4, 5):
        name = r.choice(ROOTS + [v.lower() for v in ROOTS] + ["unknown"])
        name += r.choice(["", "\\child", "/value", "Xtail"])
    if op == 11:
        x, y = ty, r.choice([0, 1, 4, 16, 31])
    if op in (12, 18):
        x = r.choice([0, 1, 4, 8, 39, 40, 64])
    if op == 26:
        x = r.choice([0, 1, 2, 4, 8, 16, 31, 0xffffffff, 32])
    if op == 29:
        x = r.randrange(256)
    if op in (34, 35):
        x = r.choice([0, 1, 16, 32, MASK])
        name = r.choice(["*.dat", "Disk:*.dat", "Disk/a/*", "Disk\\a\\*", "", "Disk/a/"])
    if op == 37:
        name = r.choice(["Disk/a/b", "Disk:a:b", "Disk\\a\\b", "Disk:/a\\b:c", "Disk", "bare", "Disk:/a/b"])
    if op in (39, 40):
        x = r.choice([0, 1, 4, 16, 32])
    if op == 42:
        x, y = r.randrange(129), r.randrange(129)
    payload = b"abcdefg\0" * 16
    if op in (15, 16):
        cfg[1] = r.choice([1, 1, 1, 2, 4])
        cfg[2] = r.choice([39, 39, 38, 40])
        payload = ("{%08X-%04X-%04X-%04X-%04X%08X%%}" %
                   (r.getrandbits(32), r.getrandbits(16), r.getrandbits(16),
                    r.getrandbits(16), r.getrandbits(16), r.getrandbits(32))).encode() + b"\0"
    return dict(op=op, cfg=cfg, queues=queues, payload=payload,
                name=name, x=x, y=y, handle=r.choice([0, 0x5000, 0x80000001]), sub=r.choice([0, 0x5001]),
                data=(b"sample text\0" + bytes(r.randrange(256) for _ in range(116))),
                fallback=b"fallback-value\0" + b"\x67" * 113, word=r.getrandbits(32), fallback_word=r.getrandbits(32),
                nullkey=bool(r.randrange(2)), nulldata=bool(r.randrange(4) == 0), nullaux=bool(r.randrange(2)))


def edges(op):
    # Each control bit, handle guard, null name, and independent outside failure.
    for i in range(40):
        c = case(random.Random(1000 + i), op)
        c["queues"].update({n: [0] for n in ("Close", "OpenEx", "Open", "Create", "Set", "Query", "Info", "EnumKey", "EnumValue")})
        c["queues"].update(Alloc=[1], Project=[0], Exists=[1], FileOpen=[0], Read=[0], Write=[0], Disk=[1], Volume=[0])
        c["handle"] = 0x5000
        c["sub"] = 0x5001
        c["nulldata"] = False
        if op == 26:
            c["x"] = i if i < 31 else MASK
            c["cfg"][4:8] = [2, 8, 9, 16]
        if op in (15, 16):
            c["name"] = "value"
            c["cfg"][1:3] = [1, 39]
        if op in (20, 21):
            c["cfg"][1:3] = [i % 8, 4]
        if op in (4, 5):
            c["name"] = (ROOTS[i % 7].lower() if i & 1 else ROOTS[i % 7]) + ["", "xchild", "\\child", "/child"][i % 4]
        if op == 43:
            c["x"], c["y"] = [(0, 0), (1, 1), (0, MASK), (MASK, 0), (0x80000000, 0x7fffffff)][i % 5]
        if op == 42:
            c["x"], c["y"] = [(0, 0), (0, 16), (16, 0), (16, 16), (64, 17), (17, 64)][i % 6]
        if op == 29:
            c["x"] = [0, 255, 97, 122, 65, 128, 254][i % 7]
        if op == 28:
            c["cfg"][14] = [0, 127, 128, 255][i % 4]
            c["cfg"][15] = [0, 127, 128, 255, 0xdeadbeef][i % 5]
            c["queues"]["Volume"] = [[0], [0x10000], [-0x10000], [0xffff]][i % 4]
        if 16 <= i < 32:
            n = ["Close", "OpenEx", "Open", "Create", "Set", "Query", "Info", "EnumKey", "EnumValue",
                 "Project", "FileOpen", "Read", "Write", "Alloc", "Disk", "Volume"][i - 16]
            c["queues"][n] = [5 if n not in ("Alloc", "Disk") else 0]
        if i >= 32:
            n, q = [("Query", [0, 5]), ("Query", [0, 0, 5]), ("Query", [0, 0, 0, 5]),
                    ("Create", [0, 5]), ("Close", [5, 0]), ("EnumKey", [0, 5]),
                    ("EnumValue", [0, 5]), ("OpenEx", [0])][i - 32]
            c["queues"][n] = q
        yield c


TESTS = [T.FnTest(name, lambda e, c: run(Ppc(e, c)), lambda lib, c: run(Host(lib, c)),
                 edge=lambda op=op: edges(op), rnd=lambda r, op=op: case(r, op))
         for op, (_, name, _) in enumerate(SPECS)]
