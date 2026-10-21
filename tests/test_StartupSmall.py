"""Small main-app functions the game's start-up runs: the time classes' constructors, the
command line, the sound and movie status setters, UI_SetRealScreenRez, CheckFileExists,
InterfaceFont and its wrapper, UIMain_ConvertMulti, CExitError::SetError.

One operation per case, run on both sides; the results are a list of numbers (and a text)
read in the same order as tests/unit_StartupSmall.cpp writes the host's, plus the events
of the fakes (_strdup, CTRACE, the TPFile calls, the text converter, deleted bitmaps).

A last test (unit v5L39), "the movie volume": the host's gMovieVolume as the library was loaded against the program image's word
(tests/imagedata.py). Before v5L39 the host started it at 0 (silent) where the Mac starts it at 0x100.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StartupSmall"

FIXED_CTOR, VARIABLE_CTOR = 0x10147bdc, 0x1016886c
LINE_CTOR, LINE_DTOR, GET_ARGC, GET_ARGV = 0x10147778, 0x101478d0, 0x10147958, 0x1014798c
SET_STATUS = [0x100dd2bc, 0x100dd2fc, 0x100dd33c]          # music, sound effects, speech
SOUND_STATE, MOVIE_SET_VOLUME, MOVIE_VOLUME = 0x102f7c40, 0x100b9cbc, 0x1029eb88
SET_REAL_REZ, REAL_WIDTH, REAL_HEIGHT = 0x101c9ed8, 0x10395bbc, 0x10395bba
CHECK_FILE, TPFILE_OPEN, TPFILE_CLOSE, CTRACE = 0x100b98a4, 0x10003fd4, 0x10004354, 0x1006fcbc
FONT_CTOR, FONT_DTOR, FONT_HEIGHT, FONT_RELEASE = 0x10168ed8, 0x10168f28, 0x10168fc0, 0x10168ff8
WRAPPER_CTOR, WRAPPER_DTOR = 0x10168ae0, 0x10168b68
CONVERT_CONST, CONVERT, TEXT_OBJECT, CONVERT_BUFFER = 0x101803cc, 0x10180334, 0x1037ff18, 0x10380c48
SET_ERROR = 0x1014b62c
OWN = [FIXED_CTOR, VARIABLE_CTOR, LINE_CTOR, LINE_DTOR, GET_ARGC, GET_ARGV, MOVIE_SET_VOLUME, SET_REAL_REZ, CHECK_FILE,
       FONT_CTOR, FONT_DTOR, FONT_HEIGHT, FONT_RELEASE, WRAPPER_CTOR, WRAPPER_DTOR, CONVERT_CONST, CONVERT,
       SET_ERROR] + SET_STATUS
FILE, FONT, CONVERTERS = 0x7c0fd300, 0x7c0fd310, [0x7c0fd320, 0x7c0fd330]      # labels 7, 8, 10 and 11
WINDOW, MOVIE, DEVICE = 0x7c0fd340, 0x7c0fd350, 0x7c0fd360                     # labels 12, 13 and 14
MOVIE_PLAY, MOVIE_TICK, MOVIE_STOP, MOVIE_INIT, MOVIE_CLOSE = 0x100b9914, 0x100b9bcc, 0x100b9c48, 0x100b9d10, 0x100b9d58
MOVIE_GLOBAL, SCREEN_WIDTH, SCREEN_HEIGHT, UDEBUG = 0x102e49ec, 0x102b2b54, 0x102b2b50, 0x10000e40
OWN += [MOVIE_PLAY, MOVIE_TICK, MOVIE_STOP, MOVIE_INIT, MOVIE_CLOSE]
PATTERN = b"\xa5"


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def text_at(e, text):
    return e.alloc(len(text) + 1, text.encode("latin-1") + b"\0", alignment=4)


def movie_stubs(e, L, st):
    """The movie player's QuickTime and Toolbox calls: fakes that record."""
    s16, s32 = T.s16, T.s32
    h = lambda a: s16(struct.unpack(">H", e.read(a, 2))[0])   # noqa: E731
    spec = [0]

    def rect_at(a):                     # a QuickDraw Rect: top, left, bottom, right
        return struct.unpack(">4h", e.read(a, 8))

    def make_spec(e):
        length = e.read(e.r(5), 1)[0]
        L.rec("FSMakeFSSpec", (s16(e.r(3)), s32(e.r(4))), (e.read(e.r(5) + 1, length).decode("latin-1"),))
        spec[0] = e.r(6)
        e.set_r(3, T.u32(L.ret("FSMakeFSSpec", 0)))

    def resolve(e):
        L.rec("ResolveAliasFile", (int(e.r(3) == spec[0]), e.r(4) & 0xFF, int(e.r(5) != 0), int(e.r(6) != 0)))
        e.set_r(3, T.u32(L.ret("ResolveAliasFile", 0)))

    def open_movie(e):
        L.rec("OpenMovieFile", (int(e.r(3) == spec[0]), T.s8(e.r(5))))
        e.write(e.r(4), struct.pack(">h", 77))
        e.set_r(3, T.u32(L.ret("OpenMovieFile", 0)))

    def new_movie(e):
        L.rec("NewMovieFromFile", (int(e.r(3) == MOVIE_GLOBAL), s16(e.r(4)), h(e.r(5)), int(e.r(6) == 0), s16(e.r(7)),
                                   int(e.r(8) == 0)))
        error = L.ret("NewMovieFromFile", 0)
        if s16(error) == 0:
            w32(e, e.r(3), MOVIE)
        e.set_r(3, T.u32(error))

    def movie_box(e):
        L.rec("GetMovieBox", (L.lab(e.r(3)),))
        e.write(e.r(4), struct.pack(">4h", 1, 2, 3, 4))

    def rectangle(e):                   # the four edges, ordered (the shim's Rationalize)
        left, top, right, bottom = (s32(e.r(k)) for k in range(4, 8))
        L.rec("URectangle", (left, top, right, bottom))
        if right < left:
            left, right = right, left
        if bottom < top:
            top, bottom = bottom, top
        e.write(e.r(3) + 4, struct.pack(">4i", left, top, right, bottom))

    def to_local(e):                    # the window is at (10, 20)
        left, top, right, bottom = struct.unpack(">4i", e.read(e.r(3) + 4, 16))
        L.rec("ToLocal", (L.lab(e.r(4)), left, top, right, bottom))
        e.write(e.r(3) + 4, struct.pack(">4I", *[v & 0xFFFFFFFF for v in (left - 10, top - 20, right - 10, bottom - 20)]))

    def as_rect(e):
        left, top, right, bottom = struct.unpack(">4I", e.read(e.r(3) + 4, 16))
        e.set_r(3, e.alloc(8, struct.pack(">4h", s16(top & 0xFFFF), s16(left & 0xFFFF), s16(bottom & 0xFFFF), s16(right & 0xFFFF))))

    def c2pcpy(e):
        text = e.read(e.r(4), 300).split(b"\0")[0][:255]
        e.write(e.r(3), bytes([len(text)]) + text)

    R = CL.recorder
    st.imp("c2pcpy__Q213SamsUtilities7UStringFPUcPCc", c2pcpy)
    st.imp("FSMakeFSSpec", make_spec)
    st.imp("ResolveAliasFile", resolve)
    st.imp("OpenMovieFile", open_movie)
    st.imp("NewMovieFromFile", new_movie)
    st.imp("GetMovieBox", movie_box)
    st.imp("SetPort", R("SetPort", "L"))
    st.imp("ShowWindow", R("ShowWindow", "L"))
    st.imp("ValidRect", lambda e: L.rec("ValidRect", rect_at(e.r(3))))
    st.imp("GetGWorldDevice", R("GetGWorldDevice", "L", lambda e, L: DEVICE))
    st.imp("SetMovieGWorld", R("SetMovieGWorld", "LLL"))
    st.imp("__ct__Q213SamsUtilities10URectangleFllll", rectangle)
    st.imp("ToLocal__Q213SamsUtilities13URectangleMacFP8GrafPort", to_local)
    st.imp("__opP4Rect__Q213SamsUtilities13URectangleMacFv", as_rect)
    st.imp("SetMovieBox", lambda e: L.rec("SetMovieBox", (L.lab(e.r(3)),) + rect_at(e.r(4))))
    st.imp("SetMovieVolume", R("SetMovieVolume", "Lh"))
    st.imp("MoviesTask", R("MoviesTask", "Li"))
    st.imp("GetMovieTime", lambda e: (L.rec("GetMovieTime", (L.lab(e.r(3)), int(e.r(4) == 0))), e.set_r(3, T.u32(L.ret("GetMovieTime", 5)))))
    st.imp("GetMoviePreferredRate", R("GetMoviePreferredRate", "L", lambda e, L: L.ret("GetMoviePreferredRate", 0x10000)))
    st.imp("PrerollMovie", R("PrerollMovie", "Lii"))
    st.imp("SetMovieRate", R("SetMovieRate", "Li"))
    st.imp("IsMovieDone", R("IsMovieDone", "L", lambda e, L: 1 if L.ret("IsMovieDone", 0) else 0))
    st.imp("StopMovie", R("StopMovie", "L"))
    st.imp("DisposeMovie", R("DisposeMovie", "L"))
    st.imp("EnterMovies", R("EnterMovies", ""))
    st.imp("ExitMovies", R("ExitMovies", ""))
    st.internal(UDEBUG, lambda e: L.rec("UDebug", (s32(e.r(4)),), (e.cstr(e.r(3)),)))


def ppc(e, c):
    op, a, text = c["op"], c.get("a", [0] * 8), c.get("text", "")
    L = CL.PpcLog({FILE: 7, FONT: 8, CONVERT_BUFFER: 9, CONVERTERS[0]: 10, CONVERTERS[1]: 11, WINDOW: 12, MOVIE: 13,
                   DEVICE: 14}, c.get("q"))
    e.cl = L
    u = e.u32
    s = lambda addr: T.s32(u(addr))
    st = CL.Stubs()
    st.real = set(OWN)

    def strdup(e):
        L.rec("_strdup", (), (e.cstr(e.r(3)),))
        e.set_r(3, text_at(e, text))

    def ctrace(e):
        fmt = e.cstr(e.r(4))
        L.rec("CTRACE", (e.r(3),), ((fmt % (T.s32(e.r(5)), e.cstr(e.r(6))))[:159],))

    def open_file(e):
        L.rec("TPFile_Open", (), (e.cstr(e.r(3)), e.cstr(e.r(4))))
        e.set_r(3, FILE if L.ret("TPFile_Open", 0) else 0)

    def convert(e):                     # hidden result in r3, then this, out, source, size
        L.rec("ConvertString", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(7) & 0xFFFF), (e.cstr(e.r(6)),))

    def strncpy(e):
        dest, source, count = e.r(3), e.r(4), e.r(5)
        data = b""
        while len(data) < count:
            byte = e.read(source + len(data), 1)
            if byte == b"\0":
                break
            data += byte
        e.write(dest, data + bytes(count - len(data)))

    def delete(e):
        block = e.r(3)
        if L.lab(block) > 0:
            L.rec("delete", (L.lab(block),))
        L.forget(block)

    st.imp("_strdup__10NS_MacDozeFPCc", strdup)
    st.internal(CTRACE, ctrace)
    st.internal(TPFILE_OPEN, open_file)
    st.internal(TPFILE_CLOSE, CL.recorder("TPFile_Close", "L", ret=lambda e, L: 0))
    st.imp("ConvertString__14TbIMBToUnicodeFPwPCUcUs", convert)
    st.imp("strncpy", strncpy)
    st.imp("free", lambda e: None)
    st.imp("__dla__FPv", delete)
    st.imp("__dl__FPv", delete)
    movie_stubs(e, L, st)

    def call(addr, *args):
        return T.call_ppc(e, addr, list(args))[0]

    def bitmap():
        block = e.alloc(16)
        L.newlab(block)
        return block

    out, joined = [], ""
    saved = st.activate(e)
    try:
        block = e.alloc(0x220, PATTERN * 0x220)
        if op == 1:
            call(FIXED_CTOR, block)
            out = [s(block), struct.unpack(">q", e.read(block + 4, 8))[0], u(block + 0xc)]
        elif op == 2:
            call(VARIABLE_CTOR, block)
            out = [s(block), struct.unpack(">q", e.read(block + 4, 8))[0], u(block + 0xc), u(block + 0x10),
                   u(block + 0x14), struct.unpack(">q", e.read(block + 0x18, 8))[0], u(block + 0x20)]
        elif op == 3:
            call(LINE_CTOR, block)
            argc = s(block)
            out = [argc, T.s32(call(GET_ARGC, block)), int(call(GET_ARGV, block) == block + 4)]
            for i in range(min(max(argc, 0), 0x80)):
                joined += e.cstr(u(block + 4 + 4 * i)) + "|"
            call(LINE_DTOR, block, 0)
        elif op == 4:
            saved_state = e.read(SOUND_STATE + 0x28, 0x1c)
            for offset, value in ((0x28, 0x11111111), (0x38, 0x22222222), (0x3c, 0x33333333), (0x40, 0x44444444)):
                w32(e, SOUND_STATE + offset, value)
            call(SET_STATUS[a[0]], a[1])
            out = [s(SOUND_STATE + 0x28), u(SOUND_STATE + 0x38), u(SOUND_STATE + 0x3c), u(SOUND_STATE + 0x40)]
            e.write(SOUND_STATE + 0x28, bytes(saved_state))
        elif op == 5:
            saved_volume = u(MOVIE_VOLUME)
            call(MOVIE_SET_VOLUME, a[0])
            out = [u(MOVIE_VOLUME)]
            w32(e, MOVIE_VOLUME, saved_volume)
        elif op == 6:
            saved_rez = e.read(REAL_HEIGHT, 4)
            call(SET_REAL_REZ, T.u32(a[0]), T.u32(a[1]))
            out = list(struct.unpack(">hh", e.read(REAL_WIDTH, 2) + e.read(REAL_HEIGHT, 2)))
            e.write(REAL_HEIGHT, bytes(saved_rez))
        elif op == 7:
            out = [T.s32(call(CHECK_FILE, text_at(e, text)))]
        elif op == 8:
            call(FONT_CTOR, block)
            out = list(struct.unpack(">HHHh", e.read(block, 8))) + [int(u(block + 8) != 0), s(block + 0xc)]
            e.write(block + 6, struct.pack(">h", T.s16(a[0])))
            out.append(T.s32(call(FONT_HEIGHT, block)))
            if a[1]:
                w32(e, block + 8, bitmap())
            call(FONT_RELEASE, block)
            out.append(int(u(block + 8) != 0))
            if a[2]:
                w32(e, block + 8, bitmap())
            call(FONT_DTOR, block, 0)
        elif op == 9:
            call(WRAPPER_CTOR, block, FONT if a[0] else 0)
            out = list(struct.unpack(">HHH", e.read(block, 6))) + [int(u(block + 8) != 0), L.lab(u(block + 0x14))]
            if a[1]:
                w32(e, block + 8, bitmap())
            call(WRAPPER_DTOR, block, 0)
        elif op == 10:
            saved_pointers = e.read(TEXT_OBJECT + 0x54c, 8)
            w32(e, TEXT_OBJECT + 0x54c, CONVERTERS[0] if a[1] else 0)
            w32(e, TEXT_OBJECT + 0x550, CONVERTERS[1] if a[2] else 0)
            out = [L.lab(call(CONVERT if a[3] else CONVERT_CONST, text_at(e, text) if a[0] else 0))]
            e.write(TEXT_OBJECT + 0x54c, bytes(saved_pointers))
        elif op == 11:
            call(SET_ERROR, block, a[0], a[1], text_at(e, text) if a[2] else 0)
            out = [s(block), s(block + 4), u(block + 8)] + list(e.read(block + 0xc, 0x54))
        elif op == 12:
            saved_globals = [(addr, u(addr)) for addr in (MOVIE_GLOBAL, SCREEN_WIDTH, SCREEN_HEIGHT, MOVIE_VOLUME)]
            w32(e, SCREEN_WIDTH, a[2])
            w32(e, SCREEN_HEIGHT, a[3])
            w32(e, MOVIE_VOLUME, a[4])
            w32(e, MOVIE_GLOBAL, MOVIE if a[5] else 0)
            for step in text:
                if step == "I":
                    call(MOVIE_INIT)
                    out.append(0)
                elif step == "P":
                    out.append(T.s32(call(MOVIE_PLAY, text_at(e, "Data:Movies:bf.mov"), WINDOW, a[0] & 0xFFFFFFFF,
                                          a[1] & 0xFFFFFFFF, 0)))
                elif step == "T":
                    out.append(T.s32(call(MOVIE_TICK)))
                elif step == "S":
                    call(MOVIE_STOP)
                    out.append(0)
                elif step == "C":
                    call(MOVIE_CLOSE)
                    out.append(0)
                out.append(L.lab(u(MOVIE_GLOBAL)))
            for addr, value in saved_globals:
                w32(e, addr, value)
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(out=out, text=joined, log=L.log)


def host(lib, c):
    op, a, text = c["op"], c.get("a", [0] * 8), c.get("text", "")
    fn = lib["tpw_ss_op"]
    fn.restype = C.c_int32
    fn.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.c_char_p, C.POINTER(C.c_int64), C.c_char_p]
    address = lib["tpw_ss_address"]
    address.restype, address.argtypes = C.c_void_p, [C.c_int32]
    CL.host_begin(lib, {label: address(label) for label in (7, 8, 9, 10, 11, 12, 13, 14)}, c.get("q"))
    out = (C.c_int64 * 128)()
    joined = C.create_string_buffer(8192)
    try:
        count = fn(op, (C.c_int32 * 8)(*[T.s32(v) for v in (list(a) + [0] * 8)[:8]]), text.encode("latin-1"), out, joined)
    finally:
        log = CL.host_end(lib)
    return dict(out=list(out[:count]), text=joined.value.decode("latin-1"), log=log)


# ----------------------------------------------------------------- cases

WORDS = ["", "x", "/w", "/windowed", "game.exe", "a b", " /x", "/a /b", "one /two three /four", "tail ", " ",
         "  /", "a/b /c", "/1 /2 /3 /4 /5", "no slash here", "x /"]


def rnd_text(r, limit=60):
    alphabet = "ab /-_.0Z:"
    return "".join(r.choice(alphabet) for _ in range(r.randrange(limit)))


def one(op, **more):
    return lambda: [dict(op=op, **more)]


def line_edges():
    for words in WORDS:
        yield dict(op=3, text=words)
    yield dict(op=3, text=" /".join("a%d" % i for i in range(0x7f)))       # 127 arguments
    yield dict(op=3, text=" /".join("a%d" % i for i in range(0x80)))       # exactly the 128 it keeps
    yield dict(op=3, text=" /".join("a%d" % i for i in range(0x90)))       # more than it keeps


def status_edges():
    for which in range(3):
        for value in (0, 1, 2, 0xFFFFFFFF):
            yield dict(op=4, a=[which, value])


def volume_edges():
    for value in (0, 1, 3, 4, 0x1ff, 0x200, 0x3fe, 0x3ff, 0x400, 0xFFFFFF, 0x1000000, 0xFFFFFFFF):
        yield dict(op=5, a=[value])


def rez_edges():
    for width, height in ((640, 480), (800, 600), (0, 0), (-1, -2), (0x7fff, -0x8000), (0x12345, 0x54321)):
        yield dict(op=6, a=[width, height])


def file_edges():
    for name in ("data:ui:splash.tga", "", "x"):
        for found in (0, 1):
            yield dict(op=7, text=name, q={"TPFile_Open": [found]})


def font_edges():
    for first in (0, 1):
        for second in (0, 1):
            yield dict(op=8, a=[17, first, second])
    yield dict(op=8, a=[-3, 0, 0])
    for font in (0, 1):
        for held in (0, 1):
            yield dict(op=9, a=[font, held])


def convert_edges():
    for has_text in (0, 1):
        for first in (0, 1):
            for second in (0, 1):
                for overload in (0, 1):
                    yield dict(op=10, a=[has_text, first, second, overload], text="Mot\xf6rhead")


def error_edges():
    for length in (0, 1, 0x4e, 0x4f, 0x50, 0x51, 0x70):
        yield dict(op=11, a=[3, 77, 1], text="e" * length)
    yield dict(op=11, a=[12, 0, 0])
    yield dict(op=11, a=[-1, 0xFFFFFFFF, 1], text="Unable to load string table")


def movie_case(r, script=None, q=None, playing=None):
    if q is None:
        q = {}
        for key in ("FSMakeFSSpec", "ResolveAliasFile", "OpenMovieFile", "NewMovieFromFile"):
            if r.random() < 0.15:
                q[key] = [r.choice([-43, 1, -2010, 0x10000, -1])] + [0]
        q["IsMovieDone"] = [r.randrange(2) for _ in range(6)]
        q["GetMovieTime"] = [r.getrandbits(16)]
        q["GetMoviePreferredRate"] = [r.choice([0x10000, 0x8000, 0, 0x20000])]
    if script is None:
        script = "".join(r.choice("IPPTTTSC") for _ in range(r.randrange(1, 9)))
    return dict(op=12, text=script, q=q,
                a=[r.choice([0, 0, -8, 100, 10]), r.choice([0, 64, -22, 20]), r.choice([800, 640, 1024, 0, 0x7FFFFFFF]),
                   r.choice([600, 480, 768, 0, -1, 0x1000000]), r.choice([0x100, 0, 0x80, 0x8000, 0x12345]),
                   r.randrange(2) if playing is None else playing])


def movie_edges():
    r = random.Random(3)
    yield movie_case(r, "IPTTTSC", {}, 0)
    yield movie_case(r, "PTSTC", {"IsMovieDone": [0, 1]}, 0)
    yield movie_case(r, "PP", {}, 0)                       # a second movie closes the first
    yield movie_case(r, "TSC", {}, 0)                      # nothing playing
    yield movie_case(r, "TSC", {"IsMovieDone": [1]}, 1)
    for key in ("FSMakeFSSpec", "ResolveAliasFile", "OpenMovieFile", "NewMovieFromFile"):
        for error in (-43, 1, 0x10000):                    # (an error of 0x10000 is no error: it is a 16-bit result)
            for playing in (0, 1):
                yield movie_case(r, "PT", {key: [error, 0]}, playing)
    for _ in range(40):
        yield movie_case(r)


TESTS = [
    FnTest("CFixedTime::CFixedTime", ppc, host, edge=one(1), rnd=lambda r: dict(op=1)),
    FnTest("CVariableTime::CVariableTime", ppc, host, edge=one(2), rnd=lambda r: dict(op=2)),
    FnTest("CCommandLine", ppc, host, edge=line_edges, rnd=lambda r: dict(op=3, text=rnd_text(r, 100))),
    FnTest("SOUND_SetMusicStatus, SfxStatus, SpeechStatus", ppc, host, edge=status_edges,
           rnd=lambda r: dict(op=4, a=[r.randrange(3), r.getrandbits(32)])),
    FnTest("MOVIE_SetVolume", ppc, host, edge=volume_edges, rnd=lambda r: dict(op=5, a=[r.getrandbits(r.choice([10, 24, 32]))])),
    FnTest("MOVIE_Play, Tick, Stop, Init, Close", ppc, host, edge=movie_edges, rnd=movie_case),
    FnTest("UI_SetRealScreenRez", ppc, host, edge=rez_edges,
           rnd=lambda r: dict(op=6, a=[r.randrange(-0x8000, 0x8000), r.randrange(-0x8000, 0x8000)])),
    FnTest("CheckFileExists", ppc, host, edge=file_edges,
           rnd=lambda r: dict(op=7, text=rnd_text(r), q={"TPFile_Open": [r.randrange(2)]})),
    FnTest("InterfaceFont and InterfaceFontWrapper", ppc, host, edge=font_edges,
           rnd=lambda r: dict(op=r.choice([8, 9]), a=[r.randrange(-200, 200), r.randrange(2), r.randrange(2)])),
    FnTest("UIMain_ConvertMulti", ppc, host, edge=convert_edges,
           rnd=lambda r: dict(op=10, a=[r.randrange(2) for _ in range(4)], text=rnd_text(r))),
    FnTest("CExitError::SetError", ppc, host, edge=error_edges,
           rnd=lambda r: dict(op=11, a=[r.randrange(-2, 20), r.getrandbits(32), r.randrange(2)], text=rnd_text(r, 140))),
    imagedata.table_test("the movie volume", [("movievol", MOVIE_VOLUME, "I", 1)]),
]
