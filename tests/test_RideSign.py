"""Ride signs in main and engine. Each program has an isolated host library.

The image and set builders follow test_Mesh and test_RideTypes. Every external
call records all arguments; text rendering records every UTF-16 code unit.
Freed blocks stay readable in both fakes for the post-call memory comparison.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
import test_Mesh as MS
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideSign"
ENGINE = "engine symbols shared"
GENERATE, DESTROY, REGENERATE = 0x101EE290, 0x101EEA14, 0x101EED5C
MESH_GENERATE, GET_POINTER = 0x1001ED9C, 0x1001F120
MASK = 0xFFFFFFFF


def F(lib, name, result, *args):
    fn = lib[name]
    fn.restype = result
    fn.argtypes = list(args)
    return fn


def w(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def wide(e, text):
    return 0 if text is None else CL.new_wide(e, text)


def setup(runner):
    T.install_std_stubs(runner.emu)


class World:
    """The same small file image on either side, with Ptr32/native address adapters."""

    def __init__(self, side, c, host=False):
        self.side, self.c, self.host = side, c, host
        self.order = "<" if host else ">"
        if host:
            self.base = F(side, "tpw_rs_base", C.c_void_p)() if c["op"] < 3 else F(side, "tpw_ms_arena_base", C.c_void_p)()
            name = "tpw_rs_alloc" if c["op"] < 3 else "tpw_ms_alloc"
            self.allocate = F(side, name, C.c_void_p, C.c_uint32)
            self.read = lambda a, n: C.string_at(a, n)
            self.write = lambda a, b: C.memmove(a, b, len(b))
            self.ptr = lambda a: a - self.base if a else 0
            self.native = lambda a: self.base + a if a else 0
            self.label = lambda a, label: side.tpw_cl_label(label, C.c_void_p(a))
            self.lab = lambda a: F(side, "tpw_cl_lab", C.c_int32, C.c_void_p)(a)
        else:
            self.allocate = side.alloc
            self.read, self.write = side.read, side.write
            self.ptr = lambda a: a
            self.native = lambda a: a
            self.label = lambda a, label: side.cl.labels.update({a: label})
            self.lab = side.cl.lab
        self.records = []
        spec = dict(textures=[(t[0], t[2]) for t in c["textures"]],
                    sets=[x for node in c["nodes"] for x in node],
                    nodes=[dict(flags=0x12340000 + k, sets=len(node), words=0, block=False)
                           for k, node in enumerate(c["nodes"])],
                    set_bytes=[0xA5 for node in c["nodes"] for _ in node],
                    name="RideTest", flags=c["header_flags"], uv=0, hfield=False)
        size = len(MS.image(spec, self.order, lambda x: 0)[0])
        self.header = self.allocate(size)
        data, offsets = MS.image(spec, self.order, lambda x: self.ptr(self.header + x))
        self.write(self.header, data)
        self.t, self.i, self.f, self.n = [self.header + x for x in offsets]
        self.put(self.header + 0x42, "H", len(c["nodes"]))
        for a, label in ((self.header, 30), (self.t, 31), (self.i, 32), (self.f, 33)):
            self.label(a, label)
        for k, node in enumerate(c["nodes"]):
            self.label(self.n + 0xA0 * k, 80 + k)
        for k, (flags, info_flags, frames, name, block_flags) in enumerate(c["textures"]):
            if info_flags & 0x18:
                record = self.allocate(22)
                self.write(record, bytes(22))
                self.put(record, "I", block_flags)
                self.write(record + 6, b"\x00\x80\x00\x80")
                self.records.append(record)
            else:
                text = name.encode("latin-1") + b"\0"
                record = self.allocate(len(text))
                self.write(record, text)
            self.label(record, 40 + k)
            self.put(self.i + 16 * k, "I", info_flags)
            self.put(self.i + 16 * k + 12, "I", self.ptr(record))
        self.newblock = 0

    def put(self, a, fmt, value):
        self.write(a, struct.pack(self.order + fmt, value))

    def get(self, a, fmt="I"):
        return struct.unpack(self.order + fmt, self.read(a, struct.calcsize(fmt)))[0]

    def record_state(self, record):
        return [self.get(record), *self.read(record + 4, 14), self.lab(self.native(self.get(record + 18)))]

    def state(self, flags):
        out = [flags, self.get(self.header + 0x30), self.get(self.header + 0x36, "H"), self.get(self.header + 0x42, "H")]
        for k in range(len(self.c["textures"])):
            out += [self.get(self.t + 8 * k, "H"), self.get(self.t + 8 * k + 2, "H"),
                    self.lab(self.native(self.get(self.t + 8 * k + 4)))]
            out += [self.get(self.i + 16 * k), self.lab(self.native(self.get(self.i + 16 * k + 4))),
                    self.get(self.i + 16 * k + 8, "H"), self.get(self.i + 16 * k + 10, "H"),
                    self.lab(self.native(self.get(self.i + 16 * k + 12)))]
        for record in self.records:
            out += self.record_state(record)
        out.append(bool(self.newblock))
        if self.newblock:
            out += self.record_state(self.newblock)
            out += self.record_state(self.newblock + 22)
        for k, node in enumerate(self.c["nodes"]):
            a = self.n + 0xA0 * k
            out += [self.get(a), self.get(a + 0x5A, "H")]
            faces = self.native(self.get(a + 0x6C))
            out.append((faces - self.f) // 16)
            for j in range(len(node)):
                p = self.native(self.get(faces + 16 * j))
                out += [-1 if not p else (p - self.t) // 8, *self.read(faces + 16 * j + 4, 12)]
        return out


def main_stubs(e, world, c, set_addr, pixels):
    S = CL.Stubs()
    S.real = {GENERATE, DESTROY, REGENERATE}
    R = CL.recorder

    def ctor(e):
        e.cl.labels[e.r(3)] = 90
        e.cl.rec("Renderer.ctor", [90])

    def alloc(e):
        e.cl.rec("Alloc", [e.r(3), e.r(4)])
        block = e.alloc(e.r(3), bytes(e.r(3)))
        e.cl.newlab(block)
        e.cl.newlab(block + 22)
        world.newblock = block
        e.set_r(3, block)

    def compare(e):
        a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
        e.cl.rec("Compare", [], [a, b])
        e.set_r(3, T.u32((a.lower() > b.lower()) - (a.lower() < b.lower())))

    def mesh(e):
        indices = e.r(4)
        e.cl.rec("MeshSign", [e.cl.lab(e.r(3)), e.r(5), e.u32(indices), e.u32(indices + 4)])
        w(e, indices, pixels[0])
        w(e, indices + 4, pixels[1])
        e.set_r(3, e.cl.ret("MeshSign", 1))

    def render(e):
        o = e.r(5)
        e.cl.rec("Render", [e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.u32(o + 8)),
                            e.cl.lab(e.u32(o + 12)), e.u32(o + 16), e.u32(o + 20), e.u32(o + 24)])
        for a in (e.u32(o), e.u32(o + 4)):
            text = []
            while world.get(a, "H"):
                text.append(world.get(a, "H"))
                a += 2
            e.cl.rec("TextLength", [len(text)])
            for i in range(0, len(text), 12):
                e.cl.rec("Text", text[i:i + 12] + [0] * (12 - len(text[i:i + 12])))

    def language(e):
        e.cl.rec("Language", [e.cl.lab(e.r(3))])
        e.set_r(3, CL.new_text(e, c["language"]))

    def local_path(e):
        e.cl.rec("LocalPath", [e.cl.lab(e.r(3))])
        obj = e.alloc(20, bytes(20))
        if c["path"] is None:
            text = 0
        elif c.get("before") is not None:
            # An empty path with the byte `before` just ahead of it (the original reads path[-1] of an empty path).
            text = e.alloc(8, bytes([ord(c["before"]), 0]) + bytes(6), alignment=4) + 1
        else:
            text = CL.new_text(e, c["path"])
        w(e, obj + 8, text)
        e.set_r(3, obj)

    S.internal(0x100CB594, ctor)
    S.internal(0x100CB82C, R("Renderer.dtor", "L"))
    S.internal(0x100CB934, R("Settings", "Ls"))
    S.internal(0x100CBD68, R("Fonts", "Ls"))
    S.internal(0x100CBEE4, render)
    S.internal(0x101EE150, R("Strip", "LLfLL"))
    S.internal(0x1006FD3C, R("ERRORBox", "ss"))
    S.internal(0x100B98A4, R("Exists", "s", lambda e, L: L.ret("Exists", 1)))
    S.internal(0x1015E93C, language)
    S.internal(0x10004D4C, local_path)
    S.internal(0x10064BD4, alloc)
    S.internal(0x10064C54, R("Free", "L", lambda e, L: 0))
    S.imp("_stricmp__10NS_MacDozeFPCcPCc", compare)
    S.imp("GenerateRideSign__5CMeshFPUli", mesh)
    S.imp("GetDC__10NS_MacDozeFP8GrafPort", R("GetDC", "L", lambda e, L: 0x7777))
    S.imp("ReleaseDC__10NS_MacDozeFP8GrafPortPQ210NS_MacDoze14InternalHandle", R("ReleaseDC", "LL", lambda e, L: 1))
    # Standard stubs do not implement these two routines.
    def strncpy(e):
        dst, src, count = e.r(3), e.r(4), e.r(5)
        text = []
        stopped = False
        for k in range(count):
            ch = 0 if stopped else world.get(src + 2 * k, "H")
            stopped |= ch == 0
            text.append(ch)
        e.write(dst, struct.pack(">" + "H" * count, *text))
        e.set_r(3, dst)

    S.imp("wcsncpy", strncpy)
    def length(e):
        n = 0
        while world.get(e.r(3) + 2 * n, "H"):
            n += 1
        e.set_r(3, n)

    def copy(e):
        dst, src = e.r(3), e.r(4)
        n = 0
        while world.get(src + 2 * n, "H"):
            n += 1
        e.write(dst, e.read(src, 2 * (n + 1)))
        e.set_r(3, dst)

    S.imp("wcslen", length)
    S.imp("wcscpy", copy)
    S.imp("strrchr", lambda e: e.set_r(3, e.r(3) + e.cstr(e.r(3)).rfind(chr(e.r(4)))
                                     if chr(e.r(4)) in e.cstr(e.r(3)) else 0))
    return S


def ppc_main(e, c):
    e.cl = CL.PpcLog({0x1037FF18: 4, 0x102B3458: 5, 0x8888: 6, 0x7777: 7}, c["q"])
    world = World(e, c)
    set_addr = e.alloc(0xFC, b"\xa5" * 0xFC)
    mesh_addr = e.alloc(0x40, b"\xa5" * 0x40)
    engine = [e.alloc(0x15C) for _ in range(2)]
    for a, label in [(set_addr, 10), (mesh_addr, 20), (engine[0], 60), (engine[1], 61)]:
        world.label(a, label)
    w(e, set_addr + 4, c["flags"])
    w(e, set_addr + 8, mesh_addr)
    w(e, mesh_addr + 4, world.header)
    w(e, mesh_addr + 0x1C, engine[0] if c["engine"] else 0)
    w(e, mesh_addr + 0x20, engine[1])
    pixels = [e.alloc(4) for _ in range(2)]
    for k, pixel in enumerate(pixels):
        world.label(pixel, 70 + k)
    for k, record in enumerate(world.records):
        w(e, record + 18, pixels[k % 2])
    settings = e.data_import("gei_TextureSettings")
    globals_ = [(settings + 4, c["format"]), (0x102B2BB4, 0x8888), (0x1038071C, c["renderer"]),
                (0x102C79A8, set_addr if c["present"] else 0)]
    kept = [(a, e.read(a, 4)) for a, _ in globals_]
    for a, value in globals_:
        w(e, a, value)
    saved = main_stubs(e, world, c, set_addr, pixels).activate(e)
    try:
        folder = CL.new_text(e, c["folder"])
        name = 0 if c["name"] is None else CL.new_text(e, c["name"])
        args = [set_addr, folder, name, CL.new_text(e, c["font"]), wide(e, c["text1"]), wide(e, c["text2"])]
        if c["op"] == 1:
            T.call_ppc(e, DESTROY, [set_addr])
            result = 0
        elif c["op"] == 2:
            result = T.call_ppc(e, REGENERATE, [c["instance"], folder, name, args[4], args[5]])[0]
        else:
            result = T.call_ppc(e, GENERATE, args)[0]
        return dict(result=result, state=world.state(e.u32(set_addr + 4)), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, data in kept:
            e.write(a, data)


def host_main(lib, c):
    CL.host_begin(lib, {}, c["q"])
    world = World(lib, c, True)
    path = None if c["path"] is None else c["path"].encode()
    if c.get("before") is not None:
        # The same: the empty path is the second byte of a buffer whose first byte is `before`.
        keep = C.create_string_buffer(c["before"].encode("latin-1") + b"\0\0")
        path = C.cast(C.addressof(keep) + 1, C.c_char_p)
    F(lib, "tpw_rs_reset", None, C.c_void_p, C.c_uint32, C.c_int32, C.c_int32, C.c_char_p,
      C.c_char_p, C.c_int32, C.c_int32)(world.header, c["flags"], c["engine"], c["format"],
                                     c["language"].encode(), path, c["renderer"], c["present"])
    for k, record in enumerate(world.records):
        pixel = F(lib, "tpw_rs_pixel", C.c_uint32, C.c_int32)(k % 2)
        world.put(record + 18, "I", pixel)
    texts = [None if t is None else (C.c_uint16 * (len(t) + 1))(*map(ord, t), 0) for t in (c["text1"], c["text2"])]
    result = F(lib, "tpw_rs_call", C.c_int32, C.c_int32, C.c_uint32, C.c_char_p, C.c_char_p,
               C.c_char_p, C.c_void_p, C.c_void_p)(c["op"], c["instance"], c["folder"].encode(),
               None if c["name"] is None else c["name"].encode(), c["font"].encode(), *texts)
    world.newblock = F(lib, "tpw_rs_newblock", C.c_void_p)() or 0
    state = world.state(F(lib, "tpw_rs_flags", C.c_uint32)())
    return dict(result=result, state=state, log=CL.host_end(lib))


def tex(name="sign1.tga", flags=0, info=0, frames=1, made=0):
    return (flags, info, frames, name, made)


def main_case(op=0, **given):
    c = dict(op=op, textures=[tex(), tex("sign2.tga")], nodes=[[0], [1]], flags=0,
             header_flags=0, text1="A ride name", text2=None, folder="Data:Rides:Ride:", name=None,
             font="Data:Fonts", language="English", format=4, engine=1, path="disk:game:",
             renderer=1, present=1, instance=1, q={})
    c.update(given)
    if op in (0, 2):
        roles = []
        for texture in c["textures"]:
            _, info, frames, name, _ = texture
            if info & 8:
                roles.append(0)
            elif info & 16:
                roles.append(1)
            elif not info & 2 and frames == 1:
                role = {"sign1.tga": 0, "sign2.tga": 1}.get(name.lower())
                if role is not None and role not in roles:
                    roles.append(role)
        if len(roles) == 2 and len(set(roles)) == 1:
            # Force the original's found!=2 cleanup before its uninitialized write.
            c["textures"] = list(c["textures"]) + [tex(info=18 if roles[0] == 0 else 10, made=2)]
    return c


def main_edges():
    yield main_case(text1=None)
    for text in ("", "A", "AB", " ", " A", "A ", "AB C", "A BC", "AB CD", "A B C", " ABCD",
                 "ABCD ", "AB CD EF", "A" * 255, "A" * 127 + " " + "B" * 127, "\u00e9 \u03b2"):
        yield main_case(text1=text)
    yield main_case(text1="First", text2="Second")
    yield main_case(text1="", text2="")
    for n in range(7):
        yield main_case(textures=[tex("sign3.tga")] * n)
    for names in (("sign1.tga",), ("sign2.tga",), ("SIGN1.TGA", "SIGN2.TGA"),
                  ("sign1.tga", "sign1.tga", "sign2.tga"), ("sign2.tga", "sign1.tga", "sign2.tga")):
        yield main_case(textures=[tex(name) for name in names])
    for frames in (1, 2):
        for flag in (0, 2, 8, 10, 16, 18, 24, 26):
            for made in (0, 2, 0xFFFFFFFF):
                yield main_case(textures=[tex(info=flag, frames=frames, made=made), tex("sign2.tga", info=18, made=2)])
    for q in ({}, {"MeshSign": [0]}, {"Exists": [0]}, {"Exists": [2]}):
        for engine in (0, 1):
            for flags in (0, 1, 0x8000000, 0xFFFFFFFF):
                yield main_case(q=q, engine=engine, flags=flags)
    for nodes in ([], [[]], [[None]], [[0, 1]], [[None, 0, None], [None, 1]], [[None], [0, 1], [1, 0]], [[1], [0]]):
        for folder in ("Ride", "Data:Rides:Ride", "Data:Rides:Ride:", ""):
            for name in (None, "", "Named"):
                yield main_case(nodes=nodes, folder=folder, name=name)
    for format_ in list(range(8)) + [-1, -0x80000000, 0x7FFFFFFF]:
        yield main_case(format=format_)


def main_random(r, op=0):
    textures = []
    for _ in range(r.randrange(7)):
        textures.append(tex(r.choice(["sign1.tga", "SIGN1.TGA", "sign2.tga", "sign3.tga"]),
                            r.getrandbits(16), r.choice([0, 0, 0, 2, 8, 10, 16, 18, 24, 26]),
                            r.choice([1, 1, 2]), r.getrandbits(32)))
    # Exactly two sign-1 records without sign-2 dereference an uninitialized register
    # in the original; omit that malformed input from Generate/Regenerate.
    roles = [0 if t[1] & 8 else 1 for t in textures if t[1] & 0x18]
    if op != 1 and len(roles) == 2 and len(set(roles)) == 1:
        textures.append(tex(info=18 if roles[0] == 0 else 10, made=2))
    nodes = [[r.randrange(len(textures)) if textures and r.random() < 0.8 else None
              for _ in range(r.randrange(5))] for _ in range(r.randrange(4))]
    text = "".join(r.choice("ABCD  ") for _ in range(r.randrange(256)))
    return main_case(op, textures=textures, nodes=nodes, flags=r.getrandbits(32), header_flags=r.getrandbits(32),
                     text1=r.choice([None, text, "A ride name"]), text2=r.choice([None, None, text]),
                     folder=r.choice(["Ride", "Data:Ride", "Data:Ride:", ""]), name=r.choice([None, "X", ""]),
                     engine=r.randrange(2), language=r.choice(["English", "French", ""]), format=r.randrange(-1, 8),
                     q={"MeshSign": [r.randrange(2)], "Exists": [r.randrange(2)]})


def destroy_edges():
    for flags in (0, 0x8000, 0xFFFFFFFF):
        for infos in ([], [0], [8], [16], [24], [8, 8], [0, 16, 8, 8]):
            yield main_case(1, header_flags=flags, textures=[tex(info=f, made=2) for f in infos])
    # Triage 2026-10-13b: texture flags that hold bit 0 (or other low bits) but not bit 3 are not the sign's name (the
    # survivor `& 9` for `& 8`), alone and before the one that is.
    for flags in (0x8000, 0xFFFFFFFF):
        for infos in ([1], [3], [5], [7], [17], [0xF7], [1, 8], [1, 3, 24], [16, 1, 8], [0xFFFFFFF7, 8], [9], [0xFFFFFFFF]):
            yield main_case(1, header_flags=flags, textures=[tex(info=f, made=2) for f in infos])


def regenerate_edges():
    for instance in (0, 1):
        for path in (None, "disk:game:", "disk:game"):
            for renderer in (0, 1, 2):
                for present in (0, 1):
                    for flags in (0, 1, 0x8000000, 0xFFFFFFFF):
                        # NULL set is safe through Generate only with NULL text1.
                        yield main_case(2, instance=instance, path=path, renderer=renderer, present=present,
                                        flags=flags, text1=None if not present else "Ride sign")
    yield from regenerate_short_paths()
    yield from regenerate_empty_path()


def regenerate_short_paths():
    """Triage 2026-10-13b (the survivor `length != 1` for `length != 0` in the separator test): local paths of one and two
    characters, with and without the closing colon, with the font folder text in the middle of a case."""
    for path in (":", "x", "::", "a:", ":a", "ab", "disk:game::"):
        for renderer in (0, 1):
            yield main_case(2, path=path, renderer=renderer, text1="Ride sign")


def regenerate_empty_path():
    """Triage 2026-10-13b: an EMPTY local path. The original tests path[-1] (the byte before the text), so it takes a
    colon there for the closing one: the font folder is "Data:Fonts" and not ":Data:Fonts". The translation never looks
    before the text (it takes an empty path as having no separator). One case per byte before the text. The
    choice: the guard stays, because the original's read is before the buffer (undefined; on the Mac it is
    whatever the heap holds), so the colon byte, the one case where the two differ, is not run."""
    for before in ("x", "\x00", "/", "\xff"):
        yield main_case(2, path="", before=before, renderer=1, text1="Ride sign")


def regenerate_random(r):
    c = main_random(r, 2)
    c.update(instance=r.randrange(2), path=r.choice([None, "disk:game:", "disk:game"]),
             renderer=r.randrange(3), present=r.randrange(2))
    if not c["present"]:
        c["text1"] = None
    return c


class EngineTest(FnTest):
    """Switch both the emulator and library to the engine for one case."""

    def run_case(self, runner, case):
        program, lib = runner.program, runner.lib
        runner.use(ENGINE)
        runner.lib = runner.shim("unit_RideSignMesh")
        T.install_std_stubs(runner.emu)
        try:
            return super().run_case(runner, case)
        finally:
            runner.use(program)
            runner.lib = lib


def engine_case(op=3, **given):
    c = main_case(op, textures=[tex(flags=0x80), tex(flags=0x80), tex(flags=0x80)], nodes=[])
    c.update(count=2, indices=[0, 1], mesh_flags=0, planted=0, null_header=0,
             null_texture=-1, null_stexture=-1, initial_entry=1, index=0, q={"Data": [1, 2, 3]})
    c.update(given)
    return c


def ppc_engine(e, c):
    e.cl = CL.PpcLog({}, c["q"])
    world = World(e, c)
    mesh = e.alloc(0x15C, bytes(0x15C))
    w(e, mesh + 4, c["mesh_flags"])
    w(e, mesh + 8, 0 if c["null_header"] else world.header)
    w(e, mesh + 0x130, 0x1234 if c["planted"] & 1 else 0)
    w(e, mesh + 0x134, 0x5678 if c["planted"] & 2 else 0)
    w(e, mesh + 0x138, 9)
    for k in range(len(c["textures"])):
        texture = e.alloc(16, bytes(16))
        entries = e.alloc(4, bytes(4))
        entry = e.alloc(0x5C) if c["initial_entry"] else 0
        w(e, texture, 0 if c["null_stexture"] == k else world.t + 8 * k)    # null_stexture: no sTexture behind it
        w(e, texture + 4, world.i + 16 * k)
        w(e, texture + 8, entries)
        w(e, entries, entry)
        world.label(texture, 50 + k)
        if entry:
            world.label(entry, 65 + k)
        w(e, world.t + 8 * k + 4, texture if c["null_texture"] != k else 0)
    S = CL.Stubs()
    S.real = {MESH_GENERATE, GET_POINTER, 0x1001F020, 0x10017168, 0x100171EC}
    arrays = {}
    allocations = 0

    def op_new(e):
        nonlocal allocations
        texture = allocations == 1
        allocations += 1
        e.cl.rec("ArrayTex" if texture else "ArrayOld", [c["count"]])
        if not e.cl.ret("NewTex" if texture else "NewOld", 1):
            e.set_r(3, 0)
            return
        block = e.alloc(e.r(3), bytes(e.r(3)))
        label = e.cl.newlab(block)
        if not texture:
            e.cl.labels[block + 8] = label
        e.set_r(3, block)

    def construct(e):
        raw, size, count = e.r(3), e.r(6), e.r(7)
        if not raw:
            return
        first = raw + 8
        w(e, raw, size)
        w(e, raw + 4, count)
        arrays[first] = count
        e.write(first, bytes(size * count))
        if size == 16:
            for i in range(count):
                e.cl.rec("Tex.ctor", [e.cl.newlab(first + size * i)])
        e.set_r(3, first)

    def reload(e):
        this, other = e.r(3), e.r(4)
        e.cl.rec("ReLoad", [e.cl.lab(this), e.cl.lab(other)])
        w(e, this, e.u32(other))
        w(e, this + 4, e.u32(other + 4))
        entries = e.alloc(4, bytes(4))
        entry = e.alloc(0x5C) if e.cl.ret("Entry", 1) else 0
        if entry:
            e.cl.newlab(entry)
        w(e, entries, entry)
        w(e, this + 8, entries)
        e.write(this + 12, struct.pack(">HH", 1, 0))
        e.set_r(3, e.cl.ret("ReLoad", 1))

    def data(e):
        e.cl.rec("Data", [e.cl.lab(e.r(3))])
        value = e.cl.ret("Data", 0)
        # The native and emulated pointer domains are normalized separately.
        e.set_r(3, value)

    def free(e):
        pointer = e.r(3)
        e.cl.rec("Free", [e.cl.lab(e.u32(pointer))])
        w(e, pointer, 0)

    def destroy_array(e):
        first = e.r(3)
        if not first:
            return
        if e.r(4):
            for k in reversed(range(arrays[first])):
                e.cl.rec("Tex.Close", [e.cl.lab(first + 16 * k)])
                e.cl.ret("Tex.Close", 1)
        raw = first - 8
        e.cl.rec("delete", [e.cl.lab(raw)])
        e.cl.forget(raw)


    class Assert(Exception):
        pass

    def assertion(e):
        e.cl.rec("Assert", [e.r(5)], [e.cstr(e.r(3)), e.cstr(e.r(4))])
        raise Assert()

    S.imp("__nwa__FUl", op_new)
    S.imp("__construct_new_array", construct)
    S.imp("__destroy_new_array", destroy_array)
    S.imp("__assertion_failed", assertion)
    S.internal(0x10016F60, reload)
    S.internal(0x10041274, data)
    oldfree = e.read(MS.P_FREE, 4)
    w(e, MS.P_FREE, T.fake_function(e, free))
    saved = S.activate(e)
    indices = e.alloc(4 * max(c["count"], 1))
    for k, index in enumerate(c["indices"]):
        w(e, indices + 4 * k, index)
    try:
        try:
            result = T.call_ppc(e, MESH_GENERATE, [mesh, indices, c["count"]])[0] if c["op"] == 3 else \
                T.call_ppc(e, GET_POINTER, [mesh, c["index"]])[0]
            result = T.s32(result) if c["op"] == 3 else result
        except Assert:
            result = -99
        old, textures = e.u32(mesh + 0x130), e.u32(mesh + 0x134)
        state = [e.u32(mesh + 4), bool(old), bool(textures), e.u32(mesh + 0x138)]
        state += [e.cl.lab(e.u32(world.t + 8 * k + 4)) for k in range(len(c["textures"]))]
        if old and old != 0x1234:
            for k in range(c["count"]):
                where = e.u32(old + 8 * k + 4)
                state += [e.cl.lab(e.u32(old + 8 * k)), -1 if not where else (where - world.t - 4) // 8]
        if textures and textures != 0x5678:
            for k in range(c["count"]):
                t = textures + 16 * k
                record, info, entries = e.u32(t), e.u32(t + 4), e.u32(t + 8)
                state += [e.cl.lab(t), -1 if not record else (record - world.t) // 8,
                          -1 if not info else (info - world.i) // 16, world.get(t + 12, "H"), world.get(t + 14, "H"),
                          e.cl.lab(e.u32(entries)) if entries else 0]
        out_indices = [e.u32(indices + 4 * k) for k in range(c["count"])]
        return dict(result=result, state=state, indices=out_indices, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.write(MS.P_FREE, oldfree)


def host_engine(lib, c):
    # Data is an opaque address for GetTexturePointer, and an arena pointer for
    # GenerateRideSign (whose output fields are arena offsets).
    CL.host_begin(lib, {}, c["q"])
    world = World(lib, c, True)
    for k in range(len(c["textures"])):
        texture = F(lib, "tpw_rsm_texture", C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32)(
            0 if c["null_stexture"] == k else world.t + 8 * k, world.i + 16 * k, c["initial_entry"])
        world.label(texture, 50 + k)
        entry = F(lib, "tpw_rsm_entry", C.c_void_p, C.c_void_p)(texture)
        if entry:
            world.label(entry, 65 + k)
        world.put(world.t + 8 * k + 4, "I", world.ptr(texture) if c["null_texture"] != k else 0)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    F(lib, "tpw_rsm_reset", None, C.c_void_p, C.c_uint32, C.c_int32, C.c_int32)(
        world.header, c["mesh_flags"], c["count"], c["planted"])
    if c["null_header"]:
        F(lib, "tpw_rsm_null_header", None)()
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    indices = (C.c_uint32 * max(c["count"], 1))(*c["indices"])
    if c["op"] == 3:
        result = F(lib, "tpw_rsm_generate", C.c_int32, C.POINTER(C.c_uint32))(indices)
    else:
        result = F(lib, "tpw_rsm_get", C.c_size_t, C.c_int32)(c["index"])
        if result == C.c_size_t(-99).value:
            result = -99
    out = (C.c_int64 * 128)()
    n = F(lib, "tpw_rsm_state", C.c_int32, C.POINTER(C.c_int64))(out)
    return dict(result=result, state=list(out[:n]), indices=list(indices[:c["count"]]), log=CL.host_end(lib))


def engine_edges():
    for count in (0, 1, 2, 3):
        for flags in (0, 0x40000000, 0x12345678):
            for q in ({"Data": [0]}, {"Data": [1, 2, 3]}, {"NewOld": [0]}, {"NewTex": [0]},
                      {"NewOld": [0], "NewTex": [0]}, {"Entry": [0]}):
                yield engine_case(count=count, indices=list(range(count)), mesh_flags=flags, q=q)
        for fail in range(count):
            yield engine_case(count=count, indices=list(range(count)), q={"ReLoad": [1] * fail + [0], "Data": [0]})
            yield engine_case(count=count, indices=list(range(count)), mesh_flags=0x40000000,
                              q={"ReLoad": [1] * fail + [0], "Data": [0]})
    for planted in (1, 2, 3):
        yield engine_case(planted=planted)
    yield engine_case(textures=[tex(flags=0)] * 3)
    # Triage 2026-10-13b (0x1001ef10): a source texture object with no sTexture behind it (GetsTexture answers NULL) halts
    # on the same assertion as one that is not a generated texture, at the first, a later and an unused entry.
    for count, indices in ((1, [0]), (2, [0, 1]), (2, [1, 0]), (3, [0, 1, 2]), (1, [2])):
        for null in range(3):
            for flags in (0, 0x40000000):
                yield engine_case(null_stexture=null, count=count, indices=indices, mesh_flags=flags)


def engine_random(r):
    count = r.randrange(1, 4)
    indices = r.sample(range(3), count)
    return engine_case(count=count, indices=indices, mesh_flags=r.getrandbits(32) & ~0x40000000,
                       q={"NewOld": [r.choice([1, 1, 1, 0])], "NewTex": [r.choice([1, 1, 1, 0])],
                          "ReLoad": [r.choice([1, 1, 0]) for _ in range(count)],
                          "Entry": [r.randrange(2) for _ in range(count)], "Data": [0]})


def pointer_edges():
    yield engine_case(4, null_header=1)
    yield engine_case(4, index=3)
    yield engine_case(4, null_texture=0)
    yield engine_case(4, initial_entry=0)
    yield engine_case(4, index=-1)
    for index in range(3):
        for data in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
            yield engine_case(4, index=index, q={"Data": [data]})


def pointer_random(r):
    return engine_case(4, index=r.randrange(3), null_texture=r.choice([-1, -1, 0, 1, 2]),
                       initial_entry=r.randrange(2), q={"Data": [r.getrandbits(32)]})


TESTS = [
    FnTest("GenerateRideSign", ppc_main, host_main, edge=main_edges, rnd=main_random),
    FnTest("DestroyRideSign", ppc_main, host_main, edge=destroy_edges, rnd=lambda r: main_random(r, 1)),
    FnTest("RegenerateRideSign", ppc_main, host_main, edge=regenerate_edges, rnd=regenerate_random),
    EngineTest("CMesh::GenerateRideSign", ppc_engine, host_engine, edge=engine_edges, rnd=engine_random),
    EngineTest("CMesh::GetTexturePointer", ppc_engine, host_engine, edge=pointer_edges, rnd=pointer_random),
]
