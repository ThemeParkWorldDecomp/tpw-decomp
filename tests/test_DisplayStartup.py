"""u8b: original engine display start-up versus the C++ translation.

Platform/other engine callees are queued fakes on both sides. Descriptor sizes are
compared in Mac units (124 versus sizeof on the host); pointer results use labels.
The enumeration fake owns its callback and supplies four GUIDs, including two
equal through NUL but different afterwards, to exercise C-string ordering. Uninitialised allocation
bytes are excluded only for objects just created by a start-up wrapper.
"""
import ctypes as C
import random
import struct
import calllog as L
import tpwtest as T

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_DisplayStartup"
U = C.c_uint32
I = C.c_int32
VP = C.c_void_p
DD = 0x10052AD0
RESULT = 0x10052B40
SYSTEM = 0x10165F80
DISPLAY = 0x10165634
DRAW = 0x102B363C
OPENGL = 0x102B3640
MEMORY = 0x102B3644
DATA = 0x102B3648
CLIPPING = 0x102B3638
BLEND = 0x100520FC

FUNCTIONS = [
    (0x100036b0, "DDraw_Init"), (0x1000380c, "DDraw_Close"),
    (0x10003890, "CDirectDraw::CDirectDraw"), (0x100038f0, "CDirectDraw::~CDirectDraw"),
    (0x10003a48, "CDirectDraw::Init"), (0x1000498c, "CDirectDraw::SetupCaps"),
    (0x10004cc0, "CDirectDraw::Close"), (0x10004dfc, "CDirectDraw::TempShutdown"),
    (0x10004e7c, "CDirectDraw::ReInitialize"), (0x10004f8c, "CDirectDraw::LockBackbuffer"),
    (0x10005098, "CDirectDraw::UnlockBackbuffer"), (0x100451a0, "OpenGL_Init"),
    (0x100452e0, "OpenGL_Close"), (0x10045370, "COpenGL::COpenGL"),
    (0x100453dc, "COpenGL::Init"), (0x100455f4, "COpenGL::Close"),
    (0x10045648, "COpenGL::TempShutdown"), (0x100456a4, "COpenGL::ReInitialize"),
    (0x100458e4, "COpenGL::InitBackground"), (0x1004595c, "COpenGL::InitRenderStates"),
    (0x10045a90, "COpenGL::MaxTextures"), (0x10045bf0, "COpenGLTexture::Initialise"),
    (0x10045cec, "COpenGLTexture::Cleanup"), (0x10046cec, "COpenGLPrintf::printf_xy"),
    (0x10042c70, "DrawPrimitiveOpenGL_Init"), (0x10042d60, "DrawPrimitiveOpenGL_Close"),
    (0x10042df8, "COpenGLDrawPrimitive::COpenGLDrawPrimitive"),
    (0x1003f3bc, "Clipping_Init"), (0x1003f45c, "Clipping_Close"),
    (0x100456f4, "COpenGL::ClearViewport"),
    (0x10003974, "CDirectDraw::FindDeviceCallback"),
]
VOID = {2, 3, 9, 13, 21, 22, 23, 26}


def put(e, a, *v):
    e.write(a, struct.pack(">" + "I" * len(v), *(T.u32(x) for x in v)))


def words(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


def fn(lib, name, res, args):
    f = lib[name]
    f.restype, f.argtypes = res, args
    return f


STUBS = L.Stubs()
STUBS.real = {a for a, _ in FUNCTIONS}


def install_fakes(e):
    # Install only the runtime's fixed memory helpers, allocations and deletions.
    T.install_std_stubs(e)
    for name in ("memset", "memcpy", "memmove"):
        STUBS.imp(name, e.stubs[name])

    def new(x):
        x.set_r(3, x.alloc(max(x.r(3), 4), b"\xcd" * max(x.r(3), 4)))

    def delete(x):
        if x.cl.lab(x.r(3)) > 0:
            x.cl.rec("delete", (x.cl.lab(x.r(3)),))
            x.cl.forget(x.r(3))
    STUBS.imp("__nw__FUl", new)
    STUBS.imp("__dl__FPv", delete)
    def strncmp(x):
        a, b, n = x.r(3), x.r(4), x.r(5)
        result = 0
        for i in range(n):
            av, bv = x.read(a + i, 1)[0], x.read(b + i, 1)[0]
            if av != bv:
                result = av - bv; break
            if not av: break
        x.set_r(3, result)
    STUBS.imp("strncmp", strncmp)

    def imp(prefix, event, spec="", result="i", then=None):
        names = [v["name"] for v in e.imports.values() if v["name"].startswith(prefix)]
        assert len(names) == 1, (prefix, names)
        STUBS.imp(names[0], L.recorder(event, spec, result, then))

    STUBS.internal(0x10008904, L.recorder("Push", "xii"))
    STUBS.internal(0x10043b20, lambda x: x.cl.rec("UDebug", (), (T.c_format(x, x.cstr(x.r(3)), 4),)))
    STUBS.internal(0x10027f2c, L.recorder("SetDisplay", "xiii", then=lambda x, _: put(x, DISPLAY + 8, x.r(4), x.r(5))))
    # SetDisplay's depth word is separated from the width and height.
    original_set = STUBS.internals[0x10027f2c]
    def set_display(x):
        depth = x.r(6)
        original_set(x)
        put(x, DISPLAY + 0x24, depth)
    STUBS.internal(0x10027f2c, set_display)
    STUBS.internal(0x10028664, L.recorder("SetMaxBits", "x"))
    def bitpattern(x):
        values = [x.r(i) for i in range(4, 11)] + words(x, x.r(1) + 56, 5)
        x.cl.rec("SetBitPattern", tuple(v if i % 3 == 0 else T.s32(v) for i, v in enumerate(values)))
    STUBS.internal(0x1002827c, bitpattern)
    STUBS.internal(0x10038ac0, L.recorder("ShiftFactor", "u", "i"))
    STUBS.internal(0x10038a78, L.recorder("BitsSet", "u", "i"))

    def query(x):
        version = 2 if x.r(4) == x.data_import("IID_IDirectDraw2") else 4
        key = "Query2" if version == 2 else "Query4"
        x.cl.rec(key, (x.cl.lab(x.r(3)),))
        result = x.cl.ret(key)
        if not result:
            put(x, x.r(5), x.objects[12 if version == 2 else 13])
        x.set_r(3, result)
    e.query_tv = T.fake_function(e, query)
    imp("Release__", "Release", "L")
    def create(x, log):
        guid, dest, unk = x.r(3), x.r(4), x.r(5)
        log.rec("DirectDrawCreate", (log.lab(unk),), (x.read(guid, 16).hex() if guid else "(null)",))
        result = log.ret("DirectDrawCreate")
        if not result:
            put(x, dest, x.objects[11])
        return result
    imp("DirectDrawCreate__", "unused", result=create)
    # recorder is not used for functions whose arguments include output blocks.
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("DirectDrawCreate__"))
    STUBS.imp(name, lambda x: x.set_r(3, create(x, x.cl)))
    def enumerate_dd(x):
        dest = x.r(4)
        x.cl.rec("Enumerate", (x.cl.lab(dest),))
        put(x, dest + 0x784, x.cl.ret("devices", 4))
        guids = ([9, 1], [3, 0, 2], [3, 0, 8], [1, 5])
        if x.cl.ret("device_order"): guids = guids[::-1]
        x.write(dest + 0x444, b"".join(bytes(v) + b"\0" * (16 - len(v)) for v in guids))
        x.set_r(3, x.cl.ret("Enumerate"))
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("DirectDrawEnumerateA__"))
    STUBS.imp(name, enumerate_dd)

    def caps(x):
        a, b = x.r(4), x.r(5)
        dirty = 0
        for v in words(x, a + 4, 94) + words(x, b + 4, 94):
            dirty |= v
        x.cl.rec("GetCaps", (x.cl.lab(x.r(3)), x.u32(a), x.u32(b), dirty))
        result = x.cl.ret("GetCaps")
        if not result:
            put(x, a + 4, *(i * 37 for i in range(1, 95)))
            put(x, b + 4, *(i * 53 for i in range(1, 95)))
        x.set_r(3, result)
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("GetCaps__"))
    STUBS.imp(name, caps)
    def device(x):
        a = x.r(4)
        x.cl.rec("GetDeviceIdentifier", (x.cl.lab(x.r(3)), x.r(5)))
        result = x.cl.ret("GetDeviceIdentifier")
        if not result:
            x.write(a, b"\0" * 0x428)
            x.write(a, b"fake-driver\0")
            x.write(a + 512, b"fake-description\0")
            put(x, a + 0x400, 0x12345678, 0x98765432, 0x1002, 0x4742)
        x.set_r(3, result)
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("GetDeviceIdentifier__"))
    STUBS.imp(name, device)
    def mode(event):
        def run(x):
            a = x.r(4)
            x.cl.rec(event, (x.cl.lab(x.r(3)), x.u32(a)))
            result = x.cl.ret(event)
            if not result:
                d = list(x.case["desc"])
                d[9] = x.objects.get(d[9], 0)
                put(x, a, *d)
            x.set_r(3, result)
        return run
    for prefix, event in (("GetDisplayMode__", "GetDisplayMode"), ("GetSurfaceDesc__", "GetSurfaceDesc")):
        name = next(v["name"] for v in e.imports.values() if v["name"].startswith(prefix))
        STUBS.imp(name, mode(event))
    def surface(x):
        d = words(x, x.r(4), 31)
        x.cl.rec("CreateSurface", (x.cl.lab(x.r(3)), d[0], d[1], d[2], d[3], d[5], d[26], x.cl.lab(x.r(6))))
        result = x.cl.ret("CreateSurface")
        if not result:
            put(x, x.r(5), x.objects[16 if d[26] == 0x6040 else 15])
        x.set_r(3, result)
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("CreateSurface__"))
    STUBS.imp(name, surface)
    imp("CreateClipper__", "CreateClipper", "LuxL", then=lambda x, log: put(x, x.r(5), x.objects[14]) if not T.s32(x.r(3)) else None)
    # Output helpers must preserve argument registers before the recorder sets r3.
    def output(prefix, event, spec, output_reg, label):
        name = next(v["name"] for v in e.imports.values() if v["name"].startswith(prefix))
        record = L.recorder(event, spec)
        def run(x):
            out = x.r(output_reg)
            record(x)
            result = x.cl.ret(event)
            if not result:
                put(x, out, x.objects[label])
            x.set_r(3, result)
        STUBS.imp(name, run)
    output("CreateClipper__", "CreateClipper", "LuxL", 5, 14)
    def attached(x):
        out = x.r(5)
        x.cl.rec("GetAttachedSurface", (x.cl.lab(x.r(3)), x.u32(x.r(4))))
        result = x.cl.ret("GetAttachedSurface")
        if not result:
            put(x, out, x.objects[16])
        x.set_r(3, result)
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("GetAttachedSurface__"))
    STUBS.imp(name, attached)
    imp("SetCooperativeLevel__", "SetCooperativeLevel", "LLu")
    imp("SetDisplayMode__", "SetDisplayMode", "Luuuuu")
    imp("RestoreDisplayMode__", "RestoreDisplayMode", "L")
    imp("SetHWnd__", "SetHWnd", "LuL")
    imp("SetClipper__", "SetClipper", "LL")
    imp("Restore__", "Restore", "L")
    imp("Unlock__", "Unlock", "LL")
    def blt(x):
        a = x.r(8)
        dirty = 0
        for v in words(x, a + 4, 24):
            dirty |= v
        x.cl.rec("Blt", (x.cl.lab(x.r(3)), x.cl.lab(x.r(4)), x.cl.lab(x.r(5)), x.cl.lab(x.r(6)), x.r(7), x.u32(a), dirty))
        x.set_r(3, x.cl.ret("Blt"))
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("Blt__"))
    STUBS.imp(name, blt)
    def lock(x):
        a = x.r(5)
        x.cl.rec("Lock", (x.cl.lab(x.r(3)), x.cl.lab(x.r(4)), x.u32(a), x.r(6), x.cl.lab(x.r(7))))
        result = x.cl.ret("Lock")
        if not result:
            d = list(x.case["desc"]); d[9] = x.objects.get(d[9], 0); put(x, a, *d)
        x.set_r(3, result)
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("Lock__"))
    STUBS.imp(name, lock)
    imp("ShowWindow__", "ShowWindow", "Li")
    imp("UpdateWindow__", "UpdateWindow", "L")
    def rect(event, values):
        def run(x):
            x.cl.rec(event, (x.cl.lab(x.r(3)),))
            put(x, x.r(4), *values)
            x.set_r(3, x.cl.ret(event))
        return run
    for prefix, event, values in (("GetClientRect__", "GetClientRect", (3, 7, 303, 207)),
                                  ("GetWindowRect__", "GetWindowRect", (21, 42, 341, 292))):
        name = next(v["name"] for v in e.imports.values() if v["name"].startswith(prefix))
        STUBS.imp(name, rect(event, values))
    imp("MoveWindow__", "MoveWindow", "Liiiii")
    def zero(x):
        x.cl.rec("ZeroMemory", (x.r(4),))
        x.write(x.r(3), b"\0" * x.r(4))
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("ZeroMemory__"))
    STUBS.imp(name, zero)
    imp("__ct__Q213SamsUtilities11ULogManager", "LoggerCtor", "N", None)
    imp("SetContext__", "SetContext", "Lssiu", None)
    def log(x):
        x.cl.rec("Log", (x.cl.lab(x.r(3)),), (T.c_format(x, x.cstr(x.r(4)), 5),))
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("Log__"))
    STUBS.imp(name, log)
    imp("__ct__Q213SamsUtilities10UAllocator", "AllocatorCtor", "N", None)
    def aligned(x):
        a = x.r(3)
        x.cl.rec("AlignedCtor", (x.cl.newlab(a), x.r(4), x.r(5), x.r(6), x.cl.lab(x.r(7))))
        put(x, a + 4, x.objects.get(x.cl.ret("data", 19), 0))
    name = next(v["name"] for v in e.imports.values() if v["name"].startswith("__ct__Q213SamsUtilities18UMemoryAlignedBase"))
    STUBS.imp(name, aligned)
    imp("__dt__Q213SamsUtilities18UMemoryAlignedBase", "AlignedDtor", "L", None)
    imp("GetMaxTextures__", "GetMaxTextures", "Liii")
    def context(x):
        x.cl.rec("DisplayGetContext")
        x.set_r(3, x.objects[22])
    e.context_tv = T.fake_function(e, context)
    def primitive_ctor(x):
        a = x.r(3)
        x.cl.rec("PrimitiveCtor", (x.cl.newlab(a),))
        put(x, a, *(0x55aa0000 + i for i in range(11)))
    STUBS.internal(0x10006954, primitive_ctor)
    def primitive_init(x):
        x.cl.rec("PrimitiveInit", (x.cl.lab(x.u32(x.r(3))),))
        x.set_r(3, x.cl.ret("PrimitiveInit"))
    STUBS.internal(0x10042f0c, primitive_init)
    def primitive_dtor(x):
        base = x.u32(x.r(3))
        label = x.cl.lab(base)
        x.cl.rec("PrimitiveDtor", (label,))
        if T.s16(x.r(4)) > 0 and label > 0:
            x.cl.rec("delete", (label,)); x.cl.forget(base)
    STUBS.internal(0x10044ee8, primitive_dtor)
    for name, spec in (("glGetError", ""), ("glPushAttrib", "u"), ("glPopAttrib", ""),
                       ("glDepthMask", "b"), ("glClear", "u"), ("glEnable", "u"), ("glDisable", "u"),
                       ("glDepthFunc", "u"), ("glEnableClientState", "u"), ("glViewport", "iiii"),
                       ("glMatrixMode", "u"), ("glLoadIdentity", ""), ("glFrontFace", "u"),
                       ("glClearColor", "ffff")):
        STUBS.imp(name, L.recorder(name, spec, "i" if name == "glGetError" else None))


def base_case():
    dd = [0xcdcdcdcd] * (0x78c // 4)
    dd[:7] = [11, 12, 13, 14, 15, 16, 17]
    dd[0x77c // 4:] = [0, 1, 4, 0]
    dd[0x41c // 4:0x434 // 4] = [0x11, 0x22, 0x33, 0x44, 0x55, 0x66]
    state = [0, 0, 0, 0, 0, 19, 3, 1, 0, 42, 640, 480, 16, 0, 1, 1, 0x12340000]
    desc = [0] * 31
    desc[0], desc[9] = 124, 19
    desc[20:26] = [32, 0x00ff0000, 0x0000ff00, 0xff, 0xff000000, 0]
    return dict(dd=dd, state=state, desc=desc, args=[640, 480, 16, 0, 1, 1], queues={})


def rcase(r):
    c = base_case()
    s, d = c["state"], c["dd"]
    for i in range(7):
        d[i] = (11 + i) if r.randrange(3) else 0
    # Keep the queried DD4 available when an existing DD1 is selected.
    d[2] = 13
    for i in range(7, len(d)):
        d[i] = r.getrandbits(32)
    d[0x784 // 4], d[0x788 // 4] = r.randrange(5), r.randrange(4)
    for i in range(5):
        s[i] = r.randrange(2)
    s[5] = r.choice([0, 19]); s[6] = r.choice([-3, 0, 1, 2, 3, 4, 7, 0x7fffffff])
    s[7] = r.choice([0, 1, -1, 3]); s[8] = r.getrandbits(32)
    s[9] = r.getrandbits(32)
    s[10:14] = [r.randrange(1, 2049), r.randrange(1, 1537), r.choice([16, 24, 32]), r.getrandbits(32)]
    s[14:16] = [r.randrange(2), r.randrange(2)]
    c["args"] = [r.randrange(1, 2049), r.randrange(1, 1537), r.choice([16, 24, 32]), r.randrange(4), r.choice([0, 1, 2, -1]), r.choice([0, 1, -1])]
    c["desc"] = [r.getrandbits(32) for _ in range(31)]
    c["desc"][0], c["desc"][9] = 124, r.choice([0, 19])
    c["queues"] = {"glGetError": [r.choice([0, 0, 0, 0x502]) for _ in range(4)],
                   "devices": [r.randrange(5)], "device_order": [r.randrange(2)], "GetMaxTextures": [T.rint(r)],
                   "PrimitiveInit": [T.rint(r)], "ShiftFactor": [T.rint(r) for _ in range(4)],
                   "BitsSet": [T.rint(r) for _ in range(4)]}
    keys = ["DirectDrawCreate", "Query2", "Query4", "SetCooperativeLevel", "SetDisplayMode",
            "CreateSurface", "GetAttachedSurface", "GetDisplayMode", "CreateClipper", "SetHWnd",
            "SetClipper", "GetDeviceIdentifier", "GetCaps", "Restore", "Unlock", "Lock"]
    if r.randrange(3):
        key = r.choice(keys)
        c["queues"][key] = [0] * r.randrange(2) + [r.choice([-1, 17, -2005532222])]
    return c


def ppc_case(e, c):
    e.case = c
    e.objects = {n: e.alloc(64) for n in list(range(11, 18)) + [19, 20, 21, 22, 30]}
    vt = e.alloc(16)
    put(e, vt + 12, e.query_tv)
    for n in range(11, 18):
        put(e, e.objects[n], vt)
    d = list(c["dd"])
    for i in range(7):
        d[i] = e.objects.get(d[i], 0)
    dd = e.alloc(0x78c, struct.pack(">483I", *d))
    gl = e.alloc(20); put(e, gl, gl + 12, 0x1005235c, c["state"][7], 0x10052380, 0)
    draw = e.alloc(0x4c)
    put(e, draw, draw + 24, 0x100522a0, -1, 0, 0, 0xcdcdcdcd)
    put(e, draw + 24, *(0x55aa0000 + i for i in range(11)), 0x100522f0, 0)
    mem = e.alloc(16); put(e, mem + 4, e.objects[19])
    clip = e.alloc(8)
    e.objects.update({1: dd, 2: gl, 3: draw + 24, 4: mem, 5: clip})
    e.cl = L.PpcLog({a: label for label, a in e.objects.items()}, c["queues"])
    s = c["state"]
    for a, v in ((DD, dd if s[0] else 0), (OPENGL, gl if s[1] else 0), (DRAW, draw + 24 if s[2] else 0),
                 (CLIPPING, clip if s[3] else 0), (MEMORY, mem if s[4] else 0), (DATA, e.objects.get(s[5], 0)),
                 (RESULT, s[9]), (SYSTEM, s[8]), (SYSTEM + 0x10, e.objects[30]),
                 (0x10052b3c, e.objects[19]),
                 (SYSTEM + 0x1c, s[6]), (SYSTEM + 0x24, gl + 12), (DISPLAY, s[13]),
                 (DISPLAY + 8, s[10]), (DISPLAY + 12, s[11]), (DISPLAY + 0x24, s[12]),
                 (DISPLAY + 0x904, 101), (DISPLAY + 0x90c, 102), (DISPLAY + 0x914, 103), (DISPLAY + 0x91c, 104),
                 (e.data_import("gLogger__13SamsUtilities"), e.objects[20] if s[14] else 0),
                 (e.data_import("gMemory__13SamsUtilities"), e.objects[21] if s[15] else 0)):
        put(e, a, v)
    put(e, BLEND, *(s[16] + i for i in range(12)))
    implementation = e.alloc(4); ivt = e.alloc(44); put(e, ivt + 40, e.context_tv); put(e, implementation, ivt)
    put(e, e.data_import("mImplementation__13IdxDirectDraw"), implementation)
    return dd, gl, draw, mem, clip


def snapshot(e, dd, gl):
    out = [int(e.u32(a) != 0) for a in (DD, OPENGL, DRAW, CLIPPING, MEMORY)]
    out += [T.u32(e.cl.lab(e.u32(DATA))), e.u32(SYSTEM + 0x1c), e.u32(gl + 8), e.u32(SYSTEM), e.u32(RESULT),
            e.u32(DISPLAY + 8), e.u32(DISPLAY + 12), e.u32(DISPLAY + 0x24), e.u32(DISPLAY),
            int(e.u32(e.data_import("gLogger__13SamsUtilities")) != 0),
            int(e.u32(e.data_import("gMemory__13SamsUtilities")) != 0)]
    out += [e.u32(DISPLAY + a) for a in (0x904, 0x90c, 0x914, 0x91c)]
    out += [int(e.u32(SYSTEM + 0x24) != 0), int(e.u32(SYSTEM + 0x10) == e.objects[30])]
    out += [e.u32(e.u32(OPENGL) + 8) if e.u32(OPENGL) else 0]
    out += [T.u32(e.cl.lab(e.u32(0x10052b3c)))]
    return out


def dd_snapshot(e, dd):
    d = words(e, dd, 483)
    d[:7] = [T.u32(e.cl.lab(x)) for x in d[:7]]
    return d


class DisplayTest:
    def __init__(self, index):
        self.index = index
        self.address, self.name = FUNCTIONS[index]

    def random_case(self, r):
        c = rcase(r)
        if self.index in (3, 13, 26):
            c["args"][0] = r.choice([0, 1, -1, 0x10000, 0x7fff, -0x8000])
        if self.index == 30: c["args"][0] = r.randrange(2)
        if self.index == 20:
            c["args"][:2] = [T.rint(r), r.choice([3, 4, 5, 6, -1])]
        if self.index == 23:
            c["args"][:3] = [T.rint(r), T.rint(r), r.choice([0, 19])]
        if self.index in (14, 11):
            c["args"][0] = r.choice([0, 1, 2, -1, 0x10000])
        return c

    def edge_cases(self):
        out = []
        for renderer in (-1, 0, 1, 2, 3, 4, 5):
            for present in (0, 1):
                c = base_case(); c["state"][:5] = [present] * 5; c["state"][6] = renderer
                out.append(c)
        if self.index in (0, 4, 8):
            for key in ("DirectDrawCreate", "Query2", "Query4", "SetCooperativeLevel", "SetDisplayMode",
                        "CreateSurface", "GetAttachedSurface", "GetDisplayMode", "CreateClipper", "SetHWnd",
                        "SetClipper", "GetDeviceIdentifier", "GetCaps"):
                for full in (0, 1):
                    for which in (0, 1):
                        c = base_case(); c["dd"][:3] = [0, 0, 0]; c["args"][4] = full
                        c["queues"][key] = [0] * which + [-1]; out.append(c)
            for device in range(4):
                for count in range(5):
                    for flags in (0, 0x10):
                        c = base_case(); c["dd"][:3] = [0, 0, 0]; c["args"][3] = device
                        c["state"][8] = flags; c["queues"]["devices"] = [count]; out.append(c)
        if self.index in (11, 14, 17):
            for mode in (0, 1, 2, -1):
                for at in range(4):
                    c = base_case(); c["args"][0] = mode; c["queues"]["glGetError"] = [0] * at + [0x502]
                    out.append(c)
        if self.index == 8:
            for renderer in (0, 2, 4):
                for results in ([-1], [0, -1], [0, 0]):
                    c = base_case(); c["state"][6] = renderer
                    c["queues"]["Restore"] = results; out.append(c)
        if self.index in (3, 13, 26):
            for flag in (0, 1, -1, 2, 0x7fff, -0x8000, 0x10000):
                c = base_case(); c["args"][0] = flag; out.append(c)
        if self.index == 20:
            for format in (-0x80000000, -1, 0, 3, 4, 5, 6, 0x7fffffff):
                c = base_case(); c["args"][1] = format; out.append(c)
        return out

    def run_case(self, runner, c):
        e, lib, index = runner.emu, runner.lib, self.index
        e.reset_scratch()
        saved = STUBS.activate(e)
        try:
            dd, gl, draw, mem, clip = ppc_case(e, c)
            a = c["args"]
            if index == 0: args = [e.objects[30]] + a[:5]
            elif index == 30: args = [dd + 0x444 if a[0] else 0, 0, 0, dd]
            elif index in (1, 12, 24, 25, 27, 28, 21, 22): args = []
            elif index <= 10: args = [dd] + (a[:6] if index == 4 else a[:1] if index == 3 else [])
            elif index == 11: args = a[:1]
            elif index in (13, 14): args = [gl] + a[:1]
            elif index == 20: args = [gl] + a[:2]
            elif index == 26: args = [draw, a[0]]
            elif index == 23:
                printf = e.alloc(28, b"\xcd" * 28)
                args = [printf, a[0], a[1], e.objects.get(a[2], 0)]
            else: args = [gl]
            output = e.alloc(124)
            if index == 9: args = [output, dd]
            result = e.call(self.address, args)
            p = {"state": snapshot(e, dd, gl)}
            if index not in VOID: p["ret"] = T.s32(result)
            if index in set(range(2, 11)) | {30}:
                p["dd"] = dd_snapshot(e, dd)
            if index == 0 and e.u32(DD) and not c["state"][0]:
                d = dd_snapshot(e, e.u32(DD)); p["new_dd"] = d[:7] + d[0x77c // 4:]
            if index == 9:
                d = words(e, output, 31); d[9] = T.u32(e.cl.lab(d[9])); p["output"] = d
            if index == 23:
                p["output"] = words(e, printf, 7)
                p["output"][4] = T.u32(e.cl.lab(p["output"][4]))
            if index == 26:
                p["draw"] = words(e, draw + 8, 3) + words(e, draw + 24, 11) + words(e, BLEND, 12)
            # The primitive ctor writes the global table in nested start-up too.
            p["blend"] = words(e, BLEND, 12)
            p["log"] = e.cl.log
            # Retain labels until snapshots are complete; deletion itself is still recorded.
        finally:
            STUBS.deactivate(e, saved)

        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        fn(lib, "tpw_ds_reset", None, [C.POINTER(U)] * 3)((U * 483)(*c["dd"]), (U * 17)(*(T.u32(v) for v in c["state"])), (U * 31)(*c["desc"]))
        fixed = {n: fn(lib, "tpw_ds_pointer", VP, [I])(n) for n in range(11, 18)}
        fixed.update({n: fn(lib, "tpw_ds_object", VP, [I])(n) for n in (1, 2, 3, 4, 5, 20, 21, 22)})
        # The draw base has a host-specific virtual-base offset.
        fixed[3] = fn(lib, "tpw_ds_draw_base", VP, [])()
        fixed[19], fixed[30] = 0x1234, 0x4567
        L.host_begin(lib, fixed, c["queues"])
        out = (U * 31)()
        result = fn(lib, "tpw_ds_call", I, [I, C.POINTER(I), C.POINTER(U)])(index, (I * 6)(*(T.s32(v) for v in a)), out)
        state = (U * 24)(); fn(lib, "tpw_ds_state", None, [C.POINTER(U)])(state)
        h = {"state": list(state)}
        if index not in VOID: h["ret"] = result
        if index in set(range(2, 11)) | {30}:
            d = (U * 483)(); fn(lib, "tpw_ds_dd_snapshot", None, [C.POINTER(U), I])(d, 0); h["dd"] = list(d)
        if "new_dd" in p:
            d = (U * 483)(); fn(lib, "tpw_ds_dd_snapshot", None, [C.POINTER(U), I])(d, 1)
            h["new_dd"] = list(d[:7]) + list(d[0x77c // 4:])
        if index in (9, 23): h["output"] = list(out[:31 if index == 9 else 7])
        if index == 26:
            d = (U * 26)(); fn(lib, "tpw_ds_draw_snapshot", None, [C.POINTER(U)])(d); h["draw"] = list(d)
        h["blend"] = list((U * 12).in_dll(lib, "gOpenGLBlendModes"))
        h["log"] = L.host_end(lib)
        mismatch = T.same_values({k: v for k, v in p.items() if k != "log"}, {k: v for k, v in h.items() if k != "log"})
        diff = L.first_difference(p["log"], h["log"])
        if diff: mismatch.append(diff)
        return mismatch


def setup(runner):
    install_fakes(runner.emu)


TESTS = [DisplayTest(i) for i in range(len(FUNCTIONS))]
