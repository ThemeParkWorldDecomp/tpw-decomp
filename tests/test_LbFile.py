"""LbFile, heap and clock: path state, outputs and all outside-callee arguments."""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbFile"
SPECS = [
    (0x100001d0, "LbFile_CreateDirectory"), (0x10000318, "LbFile_Open"),
    (0x100003d8, "LbFile_Close"), (0x10000690, "LbFile_Seek"),
    (0x10000894, "LbFile_GetLength"), (0x10000924, "LbFile_Exists"),
    (0x10000e84, "LbFile_Read"), (0x10000fa0, "LbFile_Write"),
    (0x100010bc, "LbFile_SetProjectDir"), (0x100011bc, "LbFile_GetProjectDir"),
    (0x10001590, "LbFile_ToProjectFilename"), (0x1000180c, "LbFile_Load"),
    (0x10001cb8, "LbFile_GetProjectPath"), (0x10013ea4, "LbMem_Alloc"),
    (0x10013efc, "LbMem_Free"), (0x100469ec, "LbTime_GetClock"),
    (0x100006f0, "LbFile_GetAttributes"), (0x10000798, "LbFile_SetAttributes"),
    (0x1000099c, "LbFile_IsDirectory"), (0x10000a20, "LbFile_Delete"),
    (0x10000274, "LbFile_DeleteDirectory"), (0x10000d5c, "LbFile_Rename"),
    (0x10000840, "LbFile_GetSize"), (0x10000ddc, "LbFile_Copy"),
]
PROJECT = 0x10071702
VT = 0x100731ac


def w32(e, a, v):
    e.write(a, struct.pack(">I", T.u32(v)))


def setup(runner):
    T.install_std_stubs(runner.emu)


def build_stubs(e):
    s = CL.Stubs()
    s.real.update(a for a, _ in SPECS)

    def rec(kind, a=0, b=0, text=""):
        e.lbf_events.append((kind, T.u32(a), T.u32(b), text))

    def lab(p):
        return 0 if p == 0 else 1 if p == e.lbf_file else 4 if p == e.lbf_buffer else 99

    def ctor(em):
        em.lbf_file = em.r(3)
        w32(em, em.r(3), VT)
        rec(2, 1)

    def opened(em):
        rec(3, lab(em.r(4)), em.r(6), em.cstr(em.r(5)) if em.r(5) else "(null)")
        w32(em, em.r(3), em.lbf_config[0])

    def close(em):
        rec(4, lab(em.r(3)))

    def dtor(em):
        rec(5, lab(em.r(3)))
        if T.s16(em.r(4)) > 0:
            rec(6, lab(em.r(3)))

    def io(kind):
        def fake(em):
            rec(kind, em.r(5), lab(em.r(4)))
            if kind == 18 and em.r(5) != 0:
                em.write(em.r(4), b"\xa5")
            em.set_r(3, em.lbf_config[1])
        return fake

    def seek(em):
        rec(20, em.r(4), em.r(5))
        em.set_r(3, em.lbf_config[5])

    def getsize(em):
        # CFile::GetSize, the file's vtable slot 0x1c: records which object and answers a configured word
        rec(26, lab(em.r(3)))
        em.set_r(3, em.lbf_config[5])

    for addr, fn in [(0x100291a0, ctor), (0x100292b4, opened), (0x10029214, dtor),
                     (0x1000b48c, close), (0x10000f28, io(18)), (0x10001044, io(19)),
                     (0x1000b504, seek), (0x1000b588, getsize)]:
        s.internal(addr, fn)

    def allocate(array):
        def fake(em):
            rec(7 if array else 1, em.r(3))
            em.set_r(3, em.alloc(em.r(3) or 1))
        return fake

    def free(array):
        def fake(em):
            rec(8 if array else 6, lab(em.r(3)))
        return fake

    def create(em):
        rec(14, lab(em.r(4)), 0, em.cstr(em.r(3)))
        em.set_r(3, em.lbf_config[8])

    def find(em):
        rec(15, 0, 0, em.cstr(em.r(3)))
        w32(em, em.r(4) + 0x20, em.lbf_config[2])
        w32(em, em.r(4) + 0x1c, 0x76543210)
        em.set_r(3, 3 if em.lbf_config[3] else 0xffffffff)

    def findclose(em):
        rec(16, em.r(3))
        em.set_r(3, T.u32(-10))

    def attrs(em):
        rec(17, 0, 0, em.cstr(em.r(3)))
        em.set_r(3, em.lbf_config[4])

    def ustring(em):
        rec(10, 5)
        w32(em, em.r(3) + 4, 0)
        w32(em, em.r(3) + 8, em.alloc(4, b"\0" * 4))

    def folder(em):
        rec(11)
        em.set_r(3, em.lbf_folder_object)

    def path(em):
        rec(12, 5)
        text = em.lbf_folder_text
        w32(em, em.r(4) + 4, len(text))
        w32(em, em.r(4) + 8, CL.new_text(em, text))

    def udtor(em):
        rec(13, 5)

    def timer(em):
        rec(9)
        w32(em, em.r(3), em.lbf_config[6])
        w32(em, em.r(3) + 4, em.lbf_config[7])

    # Compiler C runtime fakes are functional. Game and platform callees record calls.
    s.imp("__nw__FUl", allocate(False))
    s.imp("__nwa__FUl", allocate(True))
    s.imp("__dl__FPv", free(False))
    s.imp("__dla__FPv", free(True))
    s.imp("CreateDirectory__10NS_MacDozeFPCcPv", create)
    s.imp("FindFirstFile__10NS_MacDozeFPCcPQ210NS_MacDoze17_WIN32_FIND_DATAA", find)
    s.imp("FindClose__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle", findclose)
    s.imp("GetFileAttributes__10NS_MacDozeFPCc", attrs)
    def platform(kind):
        def fake(em):
            rec(kind, em.r(4) if kind == 21 else 0, 0, em.cstr(em.r(3)) if em.r(3) else "(null)")
            if kind == 24:
                rec(25, 0, 0, em.cstr(em.r(4)))
            em.set_r(3, em.lbf_config[8])
        return fake
    def copy_file(em):
        # NS_MacDoze::CopyFile(from, to, failIfExists): events 27 (with the flag) and 28 (unit w90)
        rec(27, 0, em.r(5), em.cstr(em.r(3)))
        rec(28, 0, 0, em.cstr(em.r(4)))
        em.set_r(3, em.lbf_config[8])

    for info in e.imports.values():
        name = info["name"]
        base = name.split("__")[0]
        if "NS_MacDoze" in name and base == "CopyFile":
            s.imp(name, copy_file)
        if "NS_MacDoze" in name and base in ("SetFileAttributes", "DeleteFile", "RemoveDirectory", "MoveFile"):
            s.imp(name, platform({"SetFileAttributes": 21, "DeleteFile": 22, "RemoveDirectory": 23, "MoveFile": 24}[base]))
    s.imp("__ct__Q213SamsUtilities7UStringFv", ustring)
    s.imp("__dt__Q213SamsUtilities7UStringFv", udtor)
    s.imp("GetFolder__Q213SamsUtilities8UProcessFv", folder)
    s.imp("GetAbsolute__Q213SamsUtilities6UTimerFv", timer)
    def strstr(em):
        pos = em.cstr(em.r(3)).find(em.cstr(em.r(4)))
        em.set_r(3, 0 if pos < 0 else em.r(3) + pos)
    s.imp("strstr", strstr)
    def strcat(em):
        text = (em.cstr(em.r(3)) + em.cstr(em.r(4))).encode("latin-1") + b"\0"
        em.write(em.r(3), text)
    s.imp("strcat", strcat)
    s.internal(e.lbf_path_entry, path)
    return s


def copy_target(c):
    """The second name of LbFile_Copy: a text that depends on the case (so that a swapped pair of names shows)."""
    return ("to" + c["guard"] + (c["name"] or "x")).encode("latin-1")[:200] + b"\0"


def case(r):
    project = r.choice(["", "Root:", "App/", "C:\\", "Theme Park:"])
    name = r.choice([None, "", "a.dat", "/a", "\\a", ":a", "::a", "Theme Park:file",
                     project + "file", "x" + project + "file"])
    x = r.choice([0, 1, 3, 16, 64, 128, 0x80000000, 0xffffffff])
    cfg = [T.u32(r.choice([-3, -1, 0, 1, 2])), r.choice([0, 1, 16, x, 0xffffffff]),
           r.choice([0, 1, 64, 128, 0x80000000, 0xfffffffe, 0xffffffff]), r.randrange(2),
           r.choice([0, 1, 0x10, 0x80000000, 0xffffffff]), r.getrandbits(32),
           r.getrandbits(32), r.getrandbits(32), T.u32(r.choice([0, 1, -1])), 0, 0, 0]
    return dict(project=project, name=name, folder=r.choice(["", "App", "App:", "App/", "App\\"]),
                guard=r.choice(["x", "/", "\\", ":"]), x=x, y=r.getrandbits(32),
                optional=r.randrange(2), cfg=cfg, buffer=r.randbytes(512))


def edges():
    r = random.Random(11)
    out = []
    for project in ("", "Root:", "Theme Park:"):
        for name in (None, "", "a", "/a", "\\a", ":a", "::a", "Root:a", "Theme Park:a"):
            for optional in (0, 1):
                c = case(r)
                c.update(project=project, name=name, optional=optional)
                out.append(c)
    for n in (258, 259, 260, 261):
        for end in ("a", "/", "\\", ":"):
            c = case(r)
            c.update(long="a" * (n - 1) + end, folder="a" * (min(n, 260) - 1) + end)
            out.append(c)
    for t in (0, 999, 1000, 1001, 0xffffffff, 0x100000000, 0xffffffffffffffff):
        c = case(r)
        c["cfg"][6:8] = [t >> 32, t & 0xffffffff]
        out.append(c)
    # Load's signed capacity check and success/error paths.
    for size in (0, 16, 64, 0x80000000, 0xfffffffe, 0xffffffff):
        for capacity in (0, 16, 64, 0x80000000, 0xffffffff):
            c = case(r)
            c.update(x=capacity, optional=1)
            c["cfg"][0:4] = [0, size, size, 1]
            out.append(c)
    return out


class FileTest:
    def __init__(self, op, addr, name):
        self.op, self.addr, self.name = op, addr, name
        self.exact_nan = False

    def edge_cases(self):
        return edges()

    def random_case(self, r):
        return case(r)

    def run_case(self, runner, c):
        e, lib = runner.emu, runner.lib
        op = self.op
        e.reset_scratch()
        e.lbf_events = []
        e.lbf_config = c["cfg"]
        e.lbf_file = 0
        folder_text = c["folder"] if op in (8, 9, 12) else "App:" if len(c["folder"]) > 200 else c["folder"]
        e.lbf_folder_text = folder_text
        # Empty setters intentionally receive an accessible preceding byte.
        name = c.get("long", c["name"]) if op == 8 else c["name"]
        if op == 8 and name is None:
            name = ""
        n = CL.new_text(e, c["guard"] + name) + 1 if name is not None else 0
        b = e.alloc(512, c["buffer"])
        if op == 21:
            e.write(b, b"other:path.dat\0")
        if op == 23:
            e.write(b, copy_target(c))
        e.lbf_buffer = b
        out = e.alloc(12, struct.pack(">3I", 0x11223344, 0x11223344, 99))
        result = e.alloc(4, b"\x55\xaa\x55\xaa" if op == 23 else None)
        e.write(PROJECT, c["project"].encode() + b"\0")
        e.write(0x10073b9f, b"\0")
        e.write(0x1007180c, b"\0" * 8)
        fvt = e.alloc(48, b"\0" * 48)
        e.lbf_path_entry = e.alloc(4, b"\x4e\x80\x00\x20")
        vec = e.alloc(8, struct.pack(">II", e.lbf_path_entry, e.toc))
        w32(e, fvt + 0x28, vec)
        e.lbf_folder_object = e.alloc(4, struct.pack(">I", fvt))
        file = e.alloc(8, struct.pack(">II", VT, 0))
        if op in (2, 3, 6, 7, 14, 22):
            e.lbf_file = file if op != 14 or c["optional"] else 0
        x = min(c["x"], 1024) if op == 13 else c["x"]
        args = {
            0: [result, n], 1: [result, out + 8, n, x], 2: [result, file],
            3: [file, x, c["y"]], 4: [n], 5: [n],
            6: [result, file, b, x, out if c["optional"] else 0],
            7: [result, file, b, x, out if c["optional"] else 0], 8: [n], 9: [],
            10: [result, b, x, n], 11: [result, n, b, x, out + 4 if c["optional"] else 0],
            12: [], 13: [x], 14: [e.lbf_file], 15: [],
            16: [result, n, out], 17: [result, n, x], 18: [n], 19: [result, n],
            20: [result, n], 21: [result, n, b], 22: [file], 23: [result, n, b],
        }[op]
        s = build_stubs(e)
        saved = s.activate(e)
        try:
            value = T.call_ppc(e, self.addr, args)[0]
        finally:
            s.deactivate(e, saved)
        pret = e.u32(result) if op in (0, 1, 2, 6, 7, 10, 11, 16, 17, 19, 20, 21, 23) else value
        if op in (8, 9, 12, 14):
            pret = 0
        if op == 13:
            pret = int(value != 0)
        pout = [e.u32(out), e.u32(out + 4), 1 if e.u32(out + 8) == e.lbf_file and e.lbf_file else 99]
        if op == 13:
            pout[2] = 99 if value else 0
        p = dict(ret=T.s32(pret), outputs=pout, project=e.cstr(PROJECT),
                 buffer=e.read(b, 512), events=e.lbf_events)
        # Native named fields and fixed buffers avoid assuming the host's object size.
        lib.tpw_lbf_project.restype = C.c_void_p
        project = lib.tpw_lbf_project()
        C.memmove(project, c["project"].encode() + b"\0", len(c["project"]) + 1)
        cfg = (C.c_uint32 * 12).in_dll(lib, "tpw_lbf_config")
        cfg[:] = c["cfg"]
        folder = (C.c_char * 512).in_dll(lib, "tpw_lbf_folder")
        folder.value = folder_text.encode()
        hname = C.create_string_buffer((c["guard"] + name).encode()) if name is not None else None
        hbuf = C.create_string_buffer(c["buffer"], 512)
        if op == 21:
            C.memmove(hbuf, b"other:path.dat\0", 15)
        if op == 23:
            C.memmove(hbuf, copy_target(c), len(copy_target(c)))
        hout = (C.c_uint32 * 3)()
        lib.tpw_lbf_call.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32,
                                     C.c_int32, C.POINTER(C.c_uint32)]
        lib.tpw_lbf_call.restype = C.c_int32
        hret = lib.tpw_lbf_call(op, C.addressof(hname) + 1 if hname is not None else None,
                               hbuf, x, c["y"], c["optional"], hout)
        class Event(C.Structure):
            _fields_ = [("kind", C.c_int32), ("a", C.c_uint32), ("b", C.c_uint32), ("text", C.c_char * 512)]
        events = (Event * 128).in_dll(lib, "tpw_lbf_events")
        count = C.c_int32.in_dll(lib, "tpw_lbf_n").value
        h = dict(ret=hret, outputs=list(hout), project=C.string_at(project).decode(), buffer=hbuf.raw,
                 events=[(v.kind, v.a, v.b, v.text.decode()) for v in events[:count]])
        return T.same_values(p, h, "", False)


TESTS = [FileTest(op, addr, name) for op, (addr, name) in enumerate(SPECS)]
