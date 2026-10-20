"""The game object's construction, destruction, resources and theme names (CGame, main
app Game.cpp range) and FS_ScanThemes.

One operation per case. The resource classes, the resource manager and the disc storage
are fakes that record what they are given, so InitResources compares as the list of
resources made, the places each is given and the order they are registered in. The
original's side keeps the default names (gTPGlobals) as fake strings holding the texts of
include/TPGlobals.h; the host's are the real object.
"""
import ctypes as C
import pathlib
import re
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Game"

SINIT, DTOR, INIT_LOCAL, INIT_RESOURCES, ADD_THEME, CLEAR_THEMES, SCAN = (
    0x1014b118, 0x10147d5c, 0x10148468, 0x10148730, 0x10149e54, 0x10149fcc, 0x10169310)
LIST_MEMBERS = [0x1014aa0c, 0x1014aad4, 0x1014ab7c, 0x1014ac48, 0x1014ad34, 0x1014af08, 0x1014b010, 0x10147cb0]
IS_SUBDIRECTORY, ADD_SEPARATOR, STR_GTR = 0x10004764, 0x10004bc0, 0x10005354
GAME, GLOBALS, LIST_VTABLE_SLOT = 0x1037f134, 0x103b74c8, 0x102614e0
RESOURCES = {"Texture": 0x10160ce8, "Sound": 0x10160e68, "Advisor": 0x10160fb8, "Font": 0x101613bc,
             "Sprite": 0x1016150c, "UI": 0x10161660, "Language": 0x10161950, "FMV": 0x10161aa4}
ADDERS = {"AddDisc": 0x1015f648, "AddWAD": 0x1015f780, "AddAll": 0x1015f948, "AddLocal": 0x1015f9e8}
REGISTER, MANAGER_CTOR, MANAGER_DTOR, SKY_DTOR = 0x10165054, 0x10164e84, 0x10164ec8, 0x1015b61c
MANAGER = 0x7c0fd400            # the game's resource manager for ops 3 to 9 (label 3)
PAUSE, UNPAUSE = 0x10149904, 0x101499d0
CLOCK, UI = 0x1037f048, 0x10396440

# The default names: offset -> text, from the header's comments (char strings only).
NAMES = {int(m.group(1), 16): m.group(2) for m in re.finditer(
    r'TbDynamicStringTemplate<char> \w+;\s*// (0x[0-9a-f]+) "([^"]*)"',
    (pathlib.Path(__file__).resolve().parent.parent / "include" / "TPGlobals.h").read_text())}


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "game_string_vtable"):
        T.persist_alloc(e, 16)                           # makes sure the persistent region is mapped
        # The fake strings' vtable: the game assigns and destroys theme names through it.
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


def set_themes(e, names):
    """The game object's theme list, built by hand: nodes of {string, next, previous}."""
    nodes = []
    for name in names:
        node = e.alloc(12)
        w32(e, node, new_string(e, name))
        nodes.append(node)
    for i, node in enumerate(nodes):
        w32(e, node + 4, nodes[i + 1] if i + 1 < len(nodes) else 0)
        w32(e, node + 8, nodes[i - 1] if i else 0)
    e.write(GAME + 0x24, struct.pack(">IIII", e.u32(LIST_VTABLE_SLOT), nodes[0] if nodes else 0,
                                     nodes[-1] if nodes else 0, len(nodes)))


def describe(e, L):
    u = e.u32
    s = lambda a: T.s32(u(a))
    themes, node, walked = "", u(GAME + 0x28), 0
    while node and walked < 64:
        themes += CL.tds_text(e, u(node)) + "\n"
        node, walked = u(node + 4), walked + 1
    tail = u(GAME + 0x2c)
    out = [s(GAME + 0x14), s(GAME + 0x18), s(GAME + 0x1c), e.read(GAME + 0x20, 1)[0], s(GAME + 0x30), walked,
           L.lab(u(GAME + 0x34)), int(u(GAME + 0x38) != 0), s(GAME + 0x3c),
           0 if tail == 0 else (1 if u(tail + 4) == 0 else -1)]
    return out, themes


def ppc(e, c):
    op, text = c["op"], c.get("text", "")
    L = CL.PpcLog({MANAGER: 3, CLOCK: 4, UI: 5}, c.get("q"))
    e.cl = L
    st = CL.Stubs()
    st.real = {SINIT, DTOR, INIT_LOCAL, INIT_RESOURCES, ADD_THEME, CLEAR_THEMES, SCAN, IS_SUBDIRECTORY,
               PAUSE, UNPAUSE} | set(LIST_MEMBERS)
    st.imports.update(CL.string_stubs())
    for name, addr in RESOURCES.items():
        st.internal(addr, CL.recorder(name, "NTT"))
    for name, addr in ADDERS.items():
        st.internal(addr, CL.recorder(name, "LT"))
    st.internal(REGISTER, CL.recorder("Register", "LL", ret=lambda e, L: 1))
    st.internal(MANAGER_CTOR, CL.recorder("Manager", "N"))
    st.internal(MANAGER_DTOR, CL.recorder("~Manager", "D"))
    sky_dtor = CL.recorder("~Sky", "D")
    st.internal(SKY_DTOR, lambda e: sky_dtor(e) if e.r(3) else None)       # the original's own NULL check

    def add_separator(e):
        value = CL.tds_text(e, e.r(3))
        if not value.endswith(":"):
            CL.tds_set(e, e.r(3), value + ":")

    def greater(e):
        a = CL.tds_text(e, e.r(3)).encode("latin-1").lower()
        b = CL.tds_text(e, e.r(4)).encode("latin-1").lower()
        e.set_r(3, int(a > b))

    st.internal(ADD_SEPARATOR, add_separator)
    st.internal(STR_GTR, greater)

    def pause_event(name, member=False, value=False):
        def record(e):
            args = [L.lab(e.r(3))] if member else ([T.s32(e.r(3))] if value else [])
            L.rec(name, args + [T.s32(e.u32(GAME + 0x1c)), e.read(GAME + 0x20, 1)[0]])
        return record

    for address, name, member, value in (
            (0x1014738c, "ClockPause", True, False), (0x101473ec, "ClockUnPause", True, False),
            (0x101ef8a0, "UIPause", True, False), (0x101ef8f0, "UIUnPause", True, False),
            (0x100dd548, "SoundPause", False, False), (0x100dd5d8, "SoundUnPause", False, False),
            (0x10007d54, "AdvisorPause", False, False), (0x10007dd4, "AdvisorUnPause", False, False),
            (0x100dde38, "Sounds3DOnly", False, True)):
        st.internal(address, pause_event(name, member, value))

    listing = [line for line in c.get("listing", "").split("\n") if line]
    position = [0]

    def entry(e, info):
        if position[0] >= len(listing):
            return -1
        name = listing[position[0]]
        position[0] += 1
        folder = name.endswith("/")
        name = name.rstrip("/") if folder else name
        w32(e, info + 4, 0x10 if folder else 0x20)
        e.write(info + 0x24, name.encode("latin-1") + b"\0")
        w32(e, info + 0x20, info + 0x24)
        return 0

    def open_storage(e):                 # hidden result, this, path
        L.rec("Open", (), (e.cstr(e.r(5)),))
        w32(e, e.r(3), L.ret("Open", 0))

    def find_first(e):                   # hidden result, this, pattern, info
        L.rec("FindFirst", (), (e.cstr(e.r(5)),))
        position[0] = 0
        w32(e, e.r(3), entry(e, e.r(6)))

    def find_next(e):                    # hidden result, this, info
        L.rec("FindNext")
        w32(e, e.r(3), entry(e, e.r(5)))

    st.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", lambda e: None)
    st.imp("__dt__Q33bfl10FileSystem12CFileStorageFv", lambda e: None)
    st.imp("Open__Q33bfl10FileSystem16CDiscFileStorageFPCc", open_storage)
    st.imp("FindFirst__Q33bfl10FileSystem12CFileStorageFPCcRQ33bfl10FileSystem9CFileInfo", find_first)
    st.imp("FindNext__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem9CFileInfo", find_next)

    def call(addr, *args):
        return T.call_ppc(e, addr, list(args))[0]

    saved_game = e.read(GAME, 0x40)
    saved_names = {off: e.read(GLOBALS + off, 20) for off in NAMES}
    saved = st.activate(e)
    result, name = 0, ""
    try:
        for off, value in NAMES.items():
            CL.tds_set(e, GLOBALS + off, value)
        if op in (1, 2):
            CL.tds_set(e, GLOBALS + 0x4060, text)
            e.write(GAME, b"\xa5" * 0x40)
            call(SINIT)
            if op == 1:
                name = CL.tds_text(e, GAME)
                out, themes = describe(e, L)
            else:
                call(ADD_THEME, GAME, new_string(e, "one"))
                call(ADD_THEME, GAME, new_string(e, "two"))
                if c["a"][0]:
                    sky = e.alloc(8)
                    L.newlab(sky)
                    w32(e, GAME + 0x38, sky)
                call(DTOR, GAME, 0)
                out, themes = [0] * 10, ""
        else:
            set_themes(e, c.get("themes", []))
            w32(e, GAME + 0x34, MANAGER)
            w32(e, GAME + 0x38, 0)
            if op == 3:
                call(INIT_RESOURCES, GAME)
            elif op == 4:
                call(INIT_LOCAL, GAME)
            elif op == 5:
                call(ADD_THEME, GAME, new_string(e, text))
            elif op == 6:
                call(CLEAR_THEMES, GAME)
            elif op == 7:
                where = e.alloc(4, b"\x55\x55\x55\x55")
                call(SCAN, where, new_string(e, text))
                result = T.s32(e.u32(where))
            elif op in (8, 9):
                a = c["a"]
                w32(e, GAME + 0x3c, a[0])
                w32(e, GAME + 0x1c, a[1])
                e.write(GAME + 0x20, bytes([a[2] & 0xff]))
                for _ in range(a[5]):
                    if op == 8:
                        call(PAUSE, GAME, a[3], int(bool(a[4])))
                    else:
                        call(UNPAUSE, GAME)
            out, themes = describe(e, L)
            out[0:2] = [0, 0]
            if op not in (8, 9):
                out[2:4] = [0, 0]          # fields the test does not set for these operations
                out[8] = 0
    finally:
        CL.Stubs.deactivate(e, saved)
        e.write(GAME, bytes(saved_game))
        for off, data in saved_names.items():
            e.write(GLOBALS + off, bytes(data))
    return dict(result=result, out=out, themes=themes, name=name, log=L.log)


def host(lib, c):
    op, text = c["op"], c.get("text", "")
    set_themes_fn = lib["tpw_game_set_themes"]
    set_themes_fn.restype, set_themes_fn.argtypes = None, [C.c_char_p]
    set_listing = lib["tpw_game_set_listing"]
    set_listing.restype, set_listing.argtypes = None, [C.c_char_p]
    fn = lib["tpw_game_op"]
    fn.restype = C.c_int32
    fn.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.c_char_p, C.POINTER(C.c_int64), C.c_char_p, C.c_char_p]
    set_themes_fn("\n".join(c.get("themes", [])).encode("latin-1"))
    set_listing(c.get("listing", "").encode("latin-1"))
    CL.host_begin(lib, {}, c.get("q"))
    out = (C.c_int64 * 10)()
    themes, name = C.create_string_buffer(8192), C.create_string_buffer(256)
    try:
        result = fn(op, (C.c_int32 * 8)(*(list(c.get("a", [])) + [0] * 8)[:8]), text.encode("latin-1"), out, themes, name)
    finally:
        log = CL.host_end(lib)
    values = list(out)
    if op not in (1, 2):
        values[0:2] = [0, 0]
        if op not in (8, 9):
            values[2:4] = [0, 0]
            values[8] = 0
    return dict(result=result, out=values, themes=themes.value.decode("latin-1"), name=name.value.decode("latin-1"),
                log=log)


# ----------------------------------------------------------------- cases

THEMES = ["fantasy", "hallow", "jungle", "space"]
POOL = ["fantasy", "hallow", "jungle", "space", "Arctic", "zoo", "HALLOW", "Jungle2", "a", "M", "m", "", "space station"]


def rnd_themes(r):
    """A preset theme list (no empty name: the host's list is handed over as lines)."""
    return sorted((name for name in r.sample(POOL, r.randrange(0, 6)) if name), key=lambda s: s.lower())


def construct_edges():
    for text in ("", "data", "a longer default name"):
        yield dict(op=1, text=text)
    for sky in (0, 1):
        yield dict(op=2, a=[sky])


def resource_edges():
    for themes in ([], ["fantasy"], THEMES, ["b", "a"]):
        yield dict(op=3, themes=themes)
        yield dict(op=4, themes=themes)


def theme_edges():
    for themes in ([], ["jungle"], THEMES):
        for name in ("", "a", "fantasy", "FANTASY", "gala", "jungle", "Jungle", "space", "zzz", "hallow2"):
            yield dict(op=5, themes=themes, text=name)
        yield dict(op=6, themes=themes)


def scan_edges():
    yield dict(op=7, text="TPW:", listing="fantasy/\nhallow/\nreadme.txt\n.hidden/\njungle/\nspace/")
    yield dict(op=7, text="TPW", listing="space/\njungle/\nHallow/\nfantasy/", themes=["old"])
    yield dict(op=7, text="", listing="")
    yield dict(op=7, text="x:", listing="only.txt")
    yield dict(op=7, text="x:", listing="a/\nb/", q={"Open": [-1]})
    yield dict(op=7, text="x:", listing="./\n../\nA/")


def rnd_listing(r):
    names = []
    for _ in range(r.randrange(0, 8)):
        name = r.choice(POOL + [".", "..", ".x", "file.sam"]) or "e"
        names.append(name + ("/" if r.random() < 0.7 else ""))
    return "\n".join(names)


def pause_edges():
    for op in (8, 9):
        for mode in (-1, 0, 1, 2):
            for paused in (0, 1, -7):
                for sound in (0, 1, 255):
                    for reason in (0, 1, 2):
                        for all_sounds in (0, 1):
                            yield dict(op=op, a=[mode, paused, sound, reason, all_sounds, 2])


def random_pause(r):
    return dict(op=r.choice([8, 9]), a=[r.choice([-1, 0, 1, 1, 2]), r.randint(-20, 20),
                                      r.randrange(256), r.choice([-1, 0, 1, 2]), r.randrange(2), r.randint(1, 4)])


TESTS = [
    FnTest("CGame::Pause, Unpause", ppc, host, edge=pause_edges, rnd=random_pause),
    FnTest("CGame construction and destruction", ppc, host, edge=construct_edges,
           rnd=lambda r: r.choice([dict(op=1, text=r.choice(POOL)), dict(op=2, a=[r.randrange(2)])])),
    FnTest("CGame::InitResources, InitLocalSoundResources", ppc, host, edge=resource_edges,
           rnd=lambda r: dict(op=r.choice([3, 4]), themes=rnd_themes(r))),
    FnTest("CGame::AddThemeName, ClearThemeNames", ppc, host, edge=theme_edges,
           rnd=lambda r: dict(op=r.choice([5, 5, 5, 6]), themes=rnd_themes(r), text=r.choice(POOL + ["Q", "h", "sp"]))),
    FnTest("FS_ScanThemes", ppc, host, edge=scan_edges,
           rnd=lambda r: dict(op=7, text=r.choice(["TPW:", "TPW", "", "a:b"]), listing=rnd_listing(r), themes=rnd_themes(r),
                              q={"Open": [r.choice([0, 0, 0, -1, 1])]})),
]
