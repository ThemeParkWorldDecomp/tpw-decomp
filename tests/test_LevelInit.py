"""The park's first initialisation (unit w106, main app): CGame::InitThemeResources and the item list resource's constructor,
RSSE_Init, SetAnimationDetail, FLY_Init, LevelInit1 with LoadLevelAdvisors, SetupLevelLights, InitCamera and
Load1stPersonMesh.

Every callee outside the ten functions is a fake on both sides that records the same event (the resource constructors,
AddLocalStorage and the manager's RegisterResource, GetCurrentThemeName, the advisor loader, the height field loader, the
engine's matrices and gamut, the clock, rand, the light maker, the file storage constructor). The ten functions run as the
original on the PowerPC side and as src/ on the host. LevelInit1 runs its own callees (SetAnimationDetail, InitCamera,
LoadLevelAdvisors, Load1stPersonMesh, SetupLevelLights) for real on both sides; the state of every global they touch is
compared, bit for bit (a NaN is one value). The strings are the PowerPC side's fakes holding the texts of the host's
defaults (include/TPGlobals.h).
"""
import ctypes as C
import math
import pathlib
import re
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LevelInit"

GLOBALS, GAME, FILEINFO, CLOCK = 0x103B74C8, 0x1037F134, 0x102B3458, 0x1037F048
MANAGER = 0x7C0FD400            # the resource manager of the game object under test (label 3)
ITR, ITEMLIST, RSSE, ANIM, FLY, LEVEL, ADVISORS, LIGHTS, CAMERA, MESH = (
    0x10148C3C, 0x10161148, 0x100D32E8, 0x100C7C68, 0x100752D4, 0x100638B0, 0x1006375C, 0x10064374, 0x100334D4, 0x10032558)
OWN = {ITR, ITEMLIST, RSSE, ANIM, FLY, LEVEL, ADVISORS, LIGHTS, CAMERA, MESH}
ITEMLIST_VTABLE = 0x102A6FDC
SENTINEL = 0x6A5A0010           # a pointer value the functions must keep or clear

# Global -> (PowerPC address, words, kind). kind "w": 32-bit words; "p": pointers (4 bytes there, 8 on the host).
STATE = {
    "engineFlags": (GLOBALS + 0x400C, 1, "w"), "sinTableLast": (GLOBALS + 0x4008, 1, "w"),
    "mF4010": (GLOBALS + 0x4010, 1, "w"),
    "rsse0": (0x102F7BEC, 1, "w"), "rsse4": (0x102F7BF0, 1, "w"), "rsse8": (0x102F7BF4, 1, "w"),
    "rssec": (0x102F7BF8, 1, "w"), "rsse10": (0x102F7BFC, 1, "p"), "checksum": (0x102F7BE8, 1, "w"),
    "f7be4": (0x102F7BE4, 1, "w"),
    "flyActive": (0x102DAAF4, 1, "w"), "flyInit": (0x102DAAF8, 1, "w"), "flyTable": (0x102DAAFC, 1, "p"),
    "flyBases": (0x102DA964, 100, "p"),
    "cameraF68": (0x1029CC68, 1, "w"), "cameraStart": (0x1029CC6C, 3, "w"), "cameraPosition": (0x1029CC78, 3, "w"),
    "cameraFov": (0x1029CC84, 1, "w"), "cameraStamp": (0x102B5A90, 1, "w"), "modeA": (0x102B5AA4, 1, "w"),
    "modeB": (0x102B5AA8, 1, "w"), "cameraF5AAC": (0x102B5AAC, 1, "w"), "matrixA": (0x102B5AF4, 16, "w"),
    "matrixB": (0x102B5B34, 16, "w"), "cameraMatrix": (0x102B5B74, 16, "w"), "cameraF5BB4": (0x102B5BB4, 3, "w"),
    "cameraF5BF4": (0x102B5BF4, 1, "w"), "firstPerson": (0x102B5A40, 1, "w"),
    "lightHandle": (0x102C6C84, 1, "p"), "lightDirection": (0x102C6C88, 3, "w"), "lightColour": (0x102C6C94, 1, "w"),
    "levelRandom": (0x102F7BD0, 1, "w"), "dialogOptions": (0x103BC664, 1, "w"),
}
CAMERA_NAMES = ["cameraF68", "cameraStart", "cameraPosition", "cameraFov", "cameraStamp", "modeA", "modeB", "cameraF5AAC",
                "matrixA", "matrixB", "cameraMatrix", "cameraF5BB4", "cameraF5BF4", "firstPerson"]
LIGHT_NAMES = ["lightHandle", "lightDirection", "lightColour"]
LEVEL_NAMES = ["engineFlags", "levelRandom", "dialogOptions"] + CAMERA_NAMES + LIGHT_NAMES
FLY_NAMES = ["flyActive", "flyInit", "flyTable", "flyBases"]
RSSE_NAMES = ["rsse0", "rsse4", "rsse8", "rssec", "rsse10", "checksum", "f7be4"]
ANIM_NAMES = ["engineFlags", "sinTableLast", "mF4010"]
FOLDER = 0x103963CC             # gThemeTextureFolder

# The default names: offset -> text, from the header's comments (char strings only).
NAMES = {int(m.group(1), 16): m.group(2) for m in re.finditer(
    r'TbDynamicStringTemplate<char> \w+;\s*// (0x[0-9a-f]+) "([^"]*)"',
    (pathlib.Path(__file__).resolve().parent.parent / "include" / "TPGlobals.h").read_text())}


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "game_string_vtable"):
        T.persist_alloc(e, 16)                           # makes sure the persistent region is mapped
        # The fake strings' vtable: the item list resource assigns its two strings through it.
        e.write(CL.FAKE_VT + 0x14, struct.pack(">I", T.fake_function(e, lambda emu: None)))
        e.write(CL.FAKE_VT + 0x1c, struct.pack(">I", T.fake_function(
            e, lambda emu: CL.tds_set(emu, emu.r(3), CL.tds_text(emu, emu.r(4))))))
        e.game_string_vtable = True


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def new_string(e, text):
    obj = e.alloc(20)
    CL.tds_set(e, obj, text)
    return obj


def f32_from_bits(bits):
    return struct.unpack(">f", struct.pack(">I", bits & 0xFFFFFFFF))[0]


# ------------------------------------------------------------------ state on both sides


def ppc_get(e, names):
    out = {}
    for name in names:
        addr, n, kind = STATE[name]
        words = struct.unpack(">%dI" % n, e.read(addr, 4 * n))
        out[name] = list(words)
    return out


def ppc_set(e, values):
    for name, words in values.items():
        addr, n, kind = STATE[name]
        e.write(addr, struct.pack(">%dI" % n, *[w & 0xFFFFFFFF for w in words]))


def host_address(lib, name):
    fn = lib.tpw_li_addr
    fn.restype, fn.argtypes = C.c_void_p, [C.c_char_p]
    return fn(name.encode())


def host_get(lib, names):
    out = {}
    for name in names:
        _, n, kind = STATE[name]
        addr = host_address(lib, name)
        out[name] = list((C.c_uint32 * n).from_address(addr) if kind == "w" else (C.c_uint64 * n).from_address(addr))
    return out


def host_set(lib, values):
    for name, words in values.items():
        _, n, kind = STATE[name]
        addr = host_address(lib, name)
        array = (C.c_uint32 * n if kind == "w" else C.c_uint64 * n).from_address(addr)
        for i, w in enumerate(words):
            array[i] = w & (0xFFFFFFFF if kind == "w" else 0xFFFFFFFFFFFFFFFF)


def normalise(state, labeller):
    """Compare-ready: pointers become 0 (cleared), 1 (kept the sentinel), a label, or 2; words stay."""
    out = {}
    for name, words in state.items():
        kind = STATE[name][2]
        if kind == "p" and name == "lightHandle":
            out[name] = [labeller(words[0])]
        elif kind == "p":
            out[name] = [0 if w == 0 else 1 if w == SENTINEL else 2 for w in words]
        elif name in ("lightDirection", "cameraStart", "cameraPosition", "cameraFov", "cameraF68", "cameraF5BF4"):
            out[name] = [f32_from_bits(w) for w in words]       # floats: a NaN is one value
        else:
            out[name] = words
    return out


def lab_host(lib):
    fn = lib.tpw_cl_lab
    fn.restype, fn.argtypes = C.c_int32, [C.c_void_p]
    return lambda value: fn(value or None) if value else 0


# ------------------------------------------------------------------ the PowerPC side's fakes


def strncat(e):
    """MSL strncat: appends at most n characters and terminates."""
    dst, src, n = e.r(3), e.r(4), e.r(5)
    old = e.cstr(dst, limit=1 << 16).encode("latin-1")
    e.write(dst + len(old), e.cstr(src, limit=1 << 16).encode("latin-1")[:n] + b"\0")
    e.set_r(3, dst)


def identity(e):
    matrix = e.r(3)
    e.cl.rec("SetMatrixIdentity", [e.cl.lab(matrix)])
    words = [0] * 16
    for i in range(4):
        words[i * 5] = 0x3F800000
    e.write(matrix, struct.pack(">16I", *words))


def add_separator(e):
    value = CL.tds_text(e, e.r(3))
    if not value.endswith(":"):
        CL.tds_set(e, e.r(3), value + ":")


def install(e, L, st, op, objects):
    """The fakes for operation `op`; `objects` is the case's PowerPC objects (see ppc)."""
    st.imports.update(CL.string_stubs())
    st.internal(0x10004BC0, add_separator)                                      # FS_AddDirectorySeparator
    st.imp("strncat", strncat)

    def rand(e):
        value = L.ret("Rand", 4242)
        L.rec("rand", [T.s32(value)])
        e.set_r(3, T.u32(value))

    st.imp("rand", rand)
    st.internal(0x1014A058, lambda e: (L.rec("GetCurrentThemeName", [L.lab(e.r(3))]), None)[1])   # answers its this
    # --- resources
    L.kinds = {}

    def resource(name, kind, spec="NTT"):
        def stub(e):
            obj = e.r(3)
            L.rec(name, [L.newlab(obj)], [CL.tds_text(e, e.r(4)), CL.tds_text(e, e.r(5))])
            L.kinds[obj] = kind
        return stub

    if op == "itr":
        st.internal(0x10160CE8, resource("Texture", 0))
        st.internal(0x10160B90, resource("Shared", 4))
        st.internal(0x10160FB8, resource("Advisor", 2))
        st.internal(0x101617E0, resource("Terrain", 3))
        # The host's real constructor runs the texture resource's fake under it.
        st.internal(ITEMLIST, resource("Texture", 1))
        st.internal(0x1015F9E8, CL.recorder("AddLocal", "LT"))

        def register(name):
            def stub(e):
                L.rec(name, [L.lab(e.r(3)), L.lab(e.r(4)), L.kinds.get(e.r(4), 0)])
                e.set_r(3, 1)
            return stub

        st.internal(0x10165054, register("Register"))
        st.internal(0x10165144, register("RegisterShared"))
    if op == "itemlist":
        st.internal(0x101648B4, lambda e: None)                                  # TbTList<CFileStorage*>::TbTList
        st.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", CL.recorder("CFileStorage", "L"))
    # --- level
    st.internal(0x10004EC4, lambda e: (L.rec("GetDataPath", [L.lab(e.r(3))]), e.set_r(3, FILEINFO + 0x28))[1])
    st.internal(0x10031BC0, lambda e: L.rec("LoadAdvisor", [e.r(6)], [e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159],
                                                                      e.cstr(e.r(5))[:159] if e.r(5) else "(null)"]))
    st.internal(0x10007E5C, CL.recorder("ADV_Init"))
    st.internal(0x1015139C, lambda e: (L.rec("get_theme_path"), e.set_r(3, objects["themePath"]))[1])
    st.internal(0x10068D14, CL.recorder("CreateScratchGeomBuffer", "i"))
    st.internal(0x1005BC44, CL.recorder("LoadLevelHeightfield", "ii", ret=lambda e, L: 0))

    def create_light(e):
        direction = e.r(6)
        bits = struct.unpack(">3I", e.read(direction, 12)) if direction else (0, 0, 0)
        L.rec("CreateGlobalLight", [T.s32(e.r(3)), e.r(4), L.lab(e.r(5)), L.lab(direction), *bits])
        e.set_r(3, objects["record"])

    st.internal(0x10063D24, create_light)
    # --- camera
    st.internal(0x100C0E28, identity)

    def read_fixed(e):
        value = L.ret("ReadFixed", 1234)
        L.rec("ReadFixed", [L.lab(e.r(3)), T.s32(value)])
        e.set_r(3, T.u32(value))

    st.internal(0x1014733C, read_fixed)

    def load(e):
        words = struct.unpack(">16I", e.read(e.r(4), 64))
        L.rec("CMatrix::Load", [L.lab(e.r(3)), L.lab(e.r(4)), *words[:6]])
        L.rec("CMatrix::Load.", words[6:14])
        L.rec("CMatrix::Load..", words[14:])

    st.imp("Load__7CMatrixFR7sMatrix", load)
    st.imp("Projection__7CMatrixFfff", CL.recorder("CMatrix::Projection", "Lfff"))
    st.imp("Init__6CGamutFf", CL.recorder("CGamut::Init", "Lf"))
    st.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "is"))
    st.imp("LbMem_Alloc__FUi", lambda e: (L.rec("LbMem_Alloc", [e.r(3)]), e.set_r(3, objects["flyBlock"]))[1])


# ------------------------------------------------------------------ one case on each side

def ppc(e, c):
    op = c["op"]
    L = None
    objects = {}
    flags_addr = 0
    # PowerPC objects of the case
    e.reset_scratch()
    objects["themePath"] = CL.new_text(e, c.get("themePath", ""))
    light = e.alloc(0x80, b"\xa5" * 0x80)
    holder = e.alloc(0x74, b"\xa5" * 0x74)
    record = e.alloc(0x20, b"\xa5" * 0x20)
    w32(e, record, 2)
    w32(e, record + 4, holder)
    w32(e, holder, light)
    objects["record"] = record
    objects["flyBlock"] = e.alloc(0x4000 + 16, b"\xa5" * (0x4000 + 16))
    flags_addr = light + 0x50
    this = e.alloc(0x40, b"\xa5" * 0x40)
    item = e.alloc(0x50, b"\xa5" * 0x50)
    view, gamut = e.alloc(0x40), e.alloc(0x40)            # the engine's view matrix and gamut (gei_* are pointers to them)
    projection = e.data_import("gei_ProjectionMatrix")   # an object, not a pointer
    fixed = {GAME: 1, this: 2, MANAGER: 3, FILEINFO: 4, view: 5, projection: 6, gamut: 7, 0x102B5B74: 8, 0x102B5B34: 9,
             0x102B5AF4: 10, record: 11, 0x102C6C88: 12, item + 0x40: 13, CLOCK: 14}
    L = CL.PpcLog(fixed, c.get("q"))
    e.cl = L
    st = CL.Stubs()
    st.real = set(OWN) - ({ITEMLIST} if op == "itr" else set())      # the constructor is a fake under InitThemeResources
    install(e, L, st, op, objects)
    saved = st.activate(e)
    out = {}
    try:
        for off, value in NAMES.items():
            CL.tds_set(e, GLOBALS + off, value)
        CL.tds_set(e, GAME, c.get("theme", ""))
        CL.tds_set(e, FILEINFO + 0x28, c.get("dataPath", ""))
        e.write(FOLDER, c.get("folder", "").encode("latin-1") + b"\0")
        w32(e, e.data_import("gei_ViewMatrix"), view)
        w32(e, e.data_import("gei_Gamut"), gamut)
        ppc_set(e, c.get("init", {}))
        w32(e, flags_addr, c.get("lightFlags", 0))
        names = c.get("names", [])
        if op == "itr":
            w32(e, this + 0x34, MANAGER)
            T.call_ppc(e, ITR, [this, new_string(e, c["path"])])
        elif op == "itemlist":
            T.call_ppc(e, ITEMLIST, [item, new_string(e, c["name"]), new_string(e, c["path"])])
            u = e.u32
            out["text"] = CL.tds_text(e, item) + "\n" + CL.tds_text(e, item + 0x14)
            out["fields"] = [int(u(item + 0x2C) != 0), int(u(item + 0x30) != 0), int(u(item + 0x34) != 0),
                             int(u(item + 0x38) != 0), int(u(item + 0x3C) != 0), int(u(item + 0x48) != 0),
                             int(u(item + 0x4C) != 0)]
            out["vtable"] = int(u(item + 0x28) == ITEMLIST_VTABLE)
        else:
            entry = {"rsse": RSSE, "anim": ANIM, "fly": FLY, "level": LEVEL, "advisors": ADVISORS, "lights": LIGHTS,
                     "camera": CAMERA, "mesh": MESH}[op]
            r3 = T.call_ppc(e, entry, [c.get("arg", 0)])[0]
            if op == "level":
                out["result"] = T.s32(r3)
            if op == "fly":
                block = e.read(objects["flyBlock"], 0x4000 + 16)
                out["table"] = struct.unpack(">4096I", block[:0x4000])
                out["guard"] = block[0x4000:]
        if names:
            state = ppc_get(e, names)
            out["state"] = normalise(state, lambda value: L.lab(value))
        if op == "lights":
            out["flags"] = e.u32(flags_addr)
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


def host(lib, c):
    op = c["op"]
    names = c.get("names", [])
    lib.tpw_li_prepare.restype, lib.tpw_li_prepare.argtypes = None, [C.c_char_p] * 4
    lib.tpw_li_call.restype, lib.tpw_li_call.argtypes = C.c_int32, [C.c_int32, C.c_uint32]
    lib.tpw_li_itr.restype, lib.tpw_li_itr.argtypes = None, [C.c_char_p]
    lib.tpw_li_itemlist.restype = None
    lib.tpw_li_itemlist.argtypes = [C.c_char_p, C.c_char_p, C.c_char_p, C.POINTER(C.c_int64)]
    lib.tpw_li_fly_block.restype = C.c_void_p
    CL.host_begin(lib, {}, c.get("q"))
    lib.tpw_li_prepare(c.get("theme", "").encode("latin-1"), c.get("themePath", "").encode("latin-1"),
                       c.get("dataPath", "").encode("latin-1"), c.get("folder", "").encode("latin-1"))
    host_set(lib, c.get("init", {}))
    C.c_uint32.from_address(host_address(lib, "lightFlags")).value = c.get("lightFlags", 0) & 0xFFFFFFFF
    out = {}
    try:
        if op == "itr":
            lib.tpw_li_itr(c["path"].encode("latin-1"))
        elif op == "itemlist":
            text, fields = C.create_string_buffer(512), (C.c_int64 * 8)()
            lib.tpw_li_itemlist(c["name"].encode("latin-1"), c["path"].encode("latin-1"), text, fields)
            out["text"] = text.value.decode("latin-1")
            out["fields"] = list(fields)[:7]
            out["vtable"] = 1
        else:
            code = {"rsse": 0, "anim": 1, "fly": 2, "level": 3, "advisors": 4, "lights": 5, "camera": 6, "mesh": 7}[op]
            result = lib.tpw_li_call(code, c.get("arg", 0) & 0xFFFFFFFF)
            if op == "level":
                out["result"] = result
            if op == "fly":
                block = C.string_at(lib.tpw_li_fly_block(), 0x4000 + 16)
                out["table"] = struct.unpack("<4096I", block[:0x4000])
                out["guard"] = block[0x4000:]
        if names:
            state = host_get(lib, names)
            out["state"] = normalise(state, lab_host(lib))
        if op == "lights":
            out["flags"] = C.c_uint32.from_address(host_address(lib, "lightFlags")).value
    finally:
        out["log"] = CL.host_end(lib)
    return out


# ------------------------------------------------------------------ cases

EDGE_WORDS = [0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x200, 0x80, 0x100]
FLOAT_BITS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x42DC0000, 0x43ED8000, 0x432F0000, 0x3FC90FDB,
              0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF, 0x00800000, 0x1E3CE508, 0x60AD78EC, 0xC2C80000]


def rnd_float_bits(r):
    """A random single float's bits: normal numbers and edges (no signalling NaN, no denormal)."""
    k = r.random()
    if k < 0.4:
        return r.choice(FLOAT_BITS)
    if k < 0.7:
        return T.f32_bits(T.f32(r.uniform(-1000.0, 1000.0)))
    while True:
        bits = r.getrandbits(32)
        exponent = (bits >> 23) & 0xFF
        if exponent != 0 and exponent != 0xFF:
            return bits


def rnd_pointer(r):
    return r.choice([0, SENTINEL, SENTINEL])


def rnd_word(r):
    return r.choice(EDGE_WORDS) if r.random() < 0.5 else r.getrandbits(32)


TEXTS = ["", "a", "ab", "abc", "abcd", "fantasy", "hallow", "jungle", "space", "Theme Park", "data:levels:fantasy",
         "data:levels:fantasy:", ":", "x:y:", "a longer text of some length"]
THEMES = ["", "a", "ab", "abc", "abcd", "fantasy", "hallow", "jungle", "space", "12345678", "Fantasy Land"]
FOLDERS = ["", "textures", "stexture", "sharetex", "ssharete", "a"]
CHARS = "abcXYZ019 _:."


def rnd_text(r, limit=24):
    return "".join(r.choice(CHARS) for _ in range(r.randrange(0, limit)))


def rnd_mix(r):
    return r.choice(TEXTS) if r.random() < 0.5 else rnd_text(r)


def case(op, names=(), **kw):
    c = dict(op=op, names=list(names))
    c.update(kw)
    return c


# --- InitThemeResources
def itr_edges():
    for path in TEXTS:
        yield case("itr", path=path, theme="fantasy")
    for theme in THEMES:
        yield case("itr", path="data:levels:" + theme, theme=theme)


def itr_random(r):
    return case("itr", path=rnd_mix(r), theme=r.choice(THEMES) if r.random() < 0.5 else rnd_text(r))


# --- The item list resource's constructor
def itemlist_edges():
    for name, path in (("", ""), ("Rides", "data:levels:fantasy:Rides"), ("Rides", "data:levels:fantasy:Rides:"),
                       ("a", ":"), ("x" * 100, "y" * 100), ("Shops", "z:"), ("Upgrades", "")):
        yield case("itemlist", name=name, path=path)


def itemlist_random(r):
    return case("itemlist", name=rnd_text(r, 40), path=rnd_text(r, 60))


# --- RSSE_Init
def rsse_case(r, first=None):
    init = {"rsse0": [first if first is not None else rnd_word(r)], "rsse4": [rnd_word(r)], "rsse8": [rnd_word(r)],
            "rssec": [rnd_word(r)], "rsse10": [rnd_pointer(r)], "checksum": [rnd_word(r)], "f7be4": [rnd_word(r)]}
    return case("rsse", RSSE_NAMES, init=init)


def rsse_edges():
    r = __import__("random").Random(5)
    for first in (0, 1, 2, 0xFFFFFFFF, 0x80000000, 0x100):
        yield rsse_case(r, first)
        yield rsse_case(r, first)


def rsse_random(r):
    return rsse_case(r, r.choice([0, 0, 1, rnd_word(r)]))


# --- SetAnimationDetail
def anim_case(r, arg):
    return case("anim", ANIM_NAMES, arg=arg,
                init={"engineFlags": [rnd_word(r)], "sinTableLast": [rnd_word(r)], "mF4010": [rnd_word(r)]})


def anim_edges():
    r = __import__("random").Random(6)
    for arg in (0, 1, 2, 3, 4, 8, 0x10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        yield anim_case(r, arg)


def anim_random(r):
    return anim_case(r, rnd_word(r))


# --- FLY_Init
def fly_case(r, first=None):
    init = {"flyInit": [first if first is not None else rnd_word(r)], "flyActive": [rnd_word(r)],
            "flyTable": [rnd_pointer(r)],
            "flyBases": [r.choice([0, SENTINEL, r.getrandbits(31) | 1]) for _ in range(100)]}
    return case("fly", FLY_NAMES, init=init)


def fly_edges():
    r = __import__("random").Random(7)
    for first in (0, 0, 1, 2, 0xFFFFFFFF, 0x80000000):
        yield fly_case(r, first)


def fly_random(r):
    return fly_case(r, r.choice([0, 0, 1, rnd_word(r)]))


# --- SetupLevelLights
DIRECTIONS = [(1.0, 0.0, 0.0), (0.0, 0.0, 0.0), (3.0, 4.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (1.0, 1.0, 1.0),
              (-1.0, 2.0, -3.0), (1e20, 1e20, 1e20), (1e-30, 1e-30, 1e-30), (math.inf, 0.0, 0.0), (-0.0, 0.0, -0.0),
              (0.5, -0.25, 0.125), (1e19, 1.0, 1.0), (3e19, 4e19, 0.0), (0.3, 0.4, 0.5), (-5.5, 0.0, 12.0)]


def lights_case(r, direction, flags=None):
    bits = [T.f32_bits(T.f32(x)) for x in direction]
    init = {"lightDirection": bits, "lightColour": [rnd_word(r)], "lightHandle": [rnd_pointer(r)]}
    return case("lights", LIGHT_NAMES, init=init, lightFlags=flags if flags is not None else rnd_word(r))


def lights_edges():
    r = __import__("random").Random(8)
    for direction in DIRECTIONS:
        yield lights_case(r, direction)
    for flags in (0, 0x20, 0xFFFFFFFF, 0x40):
        yield lights_case(r, (1.0, 2.0, 2.0), flags)


def lights_random(r):
    if r.random() < 0.3:
        direction = r.choice(DIRECTIONS)
    else:
        direction = tuple(T.f32(r.uniform(-100.0, 100.0)) for _ in range(3))
    case_ = lights_case(r, direction)
    if r.random() < 0.3:
        case_["init"]["lightDirection"] = [rnd_float_bits(r) for _ in range(3)]
    return case_


# --- LoadLevelAdvisors
def advisors_case(theme, themePath, dataPath, folder):
    return case("advisors", [], theme=theme, themePath=themePath, dataPath=dataPath, folder=folder)


def advisors_edges():
    for theme in THEMES:
        yield advisors_case(theme, "data:levels:" + theme, "data", "textures")
    yield advisors_case("fantasy", "", "", "")
    yield advisors_case("space", "a:b:c", "Theme Park:Theme Park Data:data", "stexture")


def advisors_random(r):
    return advisors_case(r.choice(THEMES) if r.random() < 0.5 else rnd_text(r, 10), rnd_mix(r), rnd_mix(r),
                         r.choice(FOLDERS))


# --- InitCamera
def camera_init(r):
    init = {"cameraF68": [rnd_float_bits(r)], "cameraStart": [rnd_float_bits(r) for _ in range(3)],
            "cameraPosition": [rnd_float_bits(r) for _ in range(3)], "cameraFov": [rnd_float_bits(r)],
            "cameraStamp": [rnd_word(r)], "modeA": [rnd_word(r)], "modeB": [rnd_word(r)], "cameraF5AAC": [rnd_word(r)],
            "matrixA": [rnd_word(r) for _ in range(16)], "matrixB": [rnd_word(r) for _ in range(16)],
            "cameraMatrix": [rnd_float_bits(r) for _ in range(16)], "cameraF5BB4": [rnd_word(r) for _ in range(3)],
            "cameraF5BF4": [rnd_float_bits(r)], "firstPerson": [rnd_word(r)]}
    return init


def camera_case(r, mode=None, fov=None, start=None):
    init = camera_init(r)
    if mode is not None:
        init["modeA"] = [mode]
    if fov is not None:
        init["cameraFov"] = [T.f32_bits(fov)]
    if start is not None:
        init["cameraStart"] = [T.f32_bits(x) for x in start]
    return case("camera", CAMERA_NAMES, init=init, q={"ReadFixed": [r.choice([1234, 0, -1, 0x7FFFFFFF, r.randint(-5000, 5000)])]})


def camera_edges():
    r = __import__("random").Random(9)
    for mode in (0, 0x200, 0x80, 0x280, 0xFFFFFFFF, 0xFFFFFDFF, 0x100, 0x7FFFFDFF):
        yield camera_case(r, mode)
    for fov in (1.5707963705062866, 0.0, 1.0, -1.0, 3.0, 1e30, math.inf):
        yield camera_case(r, fov=fov)
    yield camera_case(r, start=(475.0, 0.0, 175.0))
    yield camera_case(r, start=(-0.0, 0.0, -0.0))
    yield camera_case(r, start=(1e30, -1e30, math.inf))


def camera_random(r):
    return camera_case(r, mode=rnd_word(r))


# --- Load1stPersonMesh
def mesh_random(r):
    init = camera_init(r)
    init.update({"engineFlags": [rnd_word(r)], "sinTableLast": [rnd_word(r)], "mF4010": [rnd_word(r)],
                 "lightDirection": [rnd_float_bits(r) for _ in range(3)], "lightColour": [rnd_word(r)],
                 "lightHandle": [rnd_pointer(r)]})
    return case("mesh", CAMERA_NAMES + LIGHT_NAMES + ANIM_NAMES, init=init)


# --- LevelInit1
def level_case(r, options=None):
    init = camera_init(r)
    init.update({"engineFlags": [rnd_word(r)], "levelRandom": [rnd_word(r)],
                 "dialogOptions": [options if options is not None else rnd_word(r)],
                 "lightDirection": [T.f32_bits(T.f32(r.uniform(-10.0, 10.0))) for _ in range(3)],
                 "lightColour": [rnd_word(r)], "lightHandle": [rnd_pointer(r)]})
    theme = r.choice(THEMES)
    return case("level", LEVEL_NAMES, init=init, lightFlags=rnd_word(r), theme=theme, themePath="data:levels:" + theme,
                dataPath=r.choice(["data", "Theme Park:Theme Park Data:data"]), folder=r.choice(FOLDERS),
                q={"Rand": [r.choice([0, 1, 0x7FFF, r.randrange(0x8000), 0x12345678, 0x7FFFFFFF, -1, 0x8000])],
                  "ReadFixed": [r.randint(-100, 100)]})


def level_edges():
    r = __import__("random").Random(10)
    for options in list(range(16)) + [0x80000000, 0xFFFFFFFF, 0x10, 0x18, 0xFFFFFFF8]:
        yield level_case(r, options)


def level_random(r):
    return level_case(r)


TESTS = [
    FnTest("CGame::InitThemeResources", ppc, host, edge=itr_edges, rnd=itr_random),
    FnTest("CTPItemListResource::CTPItemListResource", ppc, host, edge=itemlist_edges, rnd=itemlist_random),
    FnTest("RSSE_Init", ppc, host, edge=rsse_edges, rnd=rsse_random),
    FnTest("SetAnimationDetail", ppc, host, edge=anim_edges, rnd=anim_random),
    FnTest("FLY_Init", ppc, host, edge=fly_edges, rnd=fly_random),
    FnTest("LoadLevelAdvisors", ppc, host, edge=advisors_edges, rnd=advisors_random),
    FnTest("SetupLevelLights", ppc, host, edge=lights_edges, rnd=lights_random),
    FnTest("InitCamera", ppc, host, edge=camera_edges, rnd=camera_random),
    FnTest("Load1stPersonMesh", ppc, host, edge=lambda: [], rnd=mesh_random),
    FnTest("LevelInit1", ppc, host, edge=level_edges, rnd=level_random),
]
