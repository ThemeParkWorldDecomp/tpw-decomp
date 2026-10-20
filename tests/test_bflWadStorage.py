"""WAD storage paths, search state, attachment order and entry compression selection."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_bflWadStorage"
SPECS = [
    (0x100086c8, "constructor"), (0x10008a24, "Close"), (0x10008af4, "Open"),
    (0x10009038, "FindFirst"), (0x100092d8, "FindNext"), (0x10009220, "StripSubPath"),
    (0x100093f0, "OpenFile"), (0x10009520, "OpenWadItem"), (0x10009ea0, "SetAttribs"),
    (0x10009ef8, "CreateDir"), (0x10009f50, "Delete"), (0x10009fa4, "Move"),
    (0x1000d27c, "CWadFileStorage Open"), (0x1000cc48, "CWadFile Open"),
    (0x100088f4, "destructor"),
    (0x10022a94, "CWadFile destructor"),
]
ERROR = set(range(2, 5)) | set(range(6, 14))


def w32(e, address, value):
    e.write(address, struct.pack(">I", T.u32(value)))


def string_set(e, p, text):
    e.write(p, struct.pack(">5I", 0x10072580, len(text), CL.new_text(e, text), 0, 0))


def string_text(e, p):
    return e.cstr(e.u32(p + 8))


def F(lib, name, result, *args):
    fn = lib[name]
    fn.restype, fn.argtypes = result, args
    return fn


def setup(runner):
    T.install_std_stubs(runner.emu)


def stubs(e):
    s = CL.Stubs()
    s.real.update(a for a, _ in SPECS)
    rec, ret, lab = e.cl.rec, e.cl.ret, e.cl.lab

    def scalar(fn):
        return lambda e: e.set_r(3, T.u32(fn(e)))

    def error(fn):
        return lambda e: w32(e, e.r(3), fn(e))

    def ctor_text(e):
        string_set(e, e.r(3), e.cstr(e.r(4)) if e.r(4) else "")

    def ctor_base(e):
        string_set(e, e.r(3), e.read(e.u32(e.r(4) + 8), e.u32(e.r(4) + 4)).decode("latin-1"))

    def append(e, base):
        text = string_text(e, e.r(4)) if base else e.cstr(e.r(4))
        string_set(e, e.r(3), string_text(e, e.r(3)) + text)

    def view(e, kind):
        text, count = string_text(e, e.r(4)), e.r(5)
        value = text[count:] if kind == "Mid" else text[:count] if kind == "Left" else text[-count:] if count else ""
        string_set(e, e.r(3), value)

    def find_one(e):
        text, chars, start = string_text(e, e.r(3)), e.cstr(e.r(4)), e.r(5)
        return next((i for i in range(start, len(text)) if text[i] in chars), -1)

    def find_reverse(e):
        text, ch, start = string_text(e, e.r(3)), e.read(e.r(4), 1).decode("latin-1"), e.r(5)
        return text.rfind(ch, 0, min(start + 1, len(text)))

    for a in (0x100104c4,):
        s.internal(a, lambda e: string_set(e, e.r(3), ""))
    for a in (0x10010328, 0x1000e44c):
        s.internal(a, ctor_text)
    for a in (0x10010160, 0x10010248, 0x1000e364, 0x100100b8):
        s.internal(a, ctor_base)
    s.internal(0x1000e5d8, lambda e: append(e, True))
    s.internal(0x1000e758, lambda e: append(e, False))
    s.internal(0x1000e2c8, lambda e: None)
    s.internal(0x10011ccc, scalar(lambda e: e.u32(e.r(3) + 4)))
    s.internal(0x10011634, scalar(lambda e: e.u32(e.r(3) + 8)))
    s.internal(0x100136d0, lambda e: w32(e, e.r(3) + 4, len(string_text(e, e.r(3)))))
    s.internal(0x100118d8, scalar(lambda e: int(string_text(e, e.r(3)) != e.cstr(e.r(4)))))
    s.internal(0x10013448, scalar(lambda e: string_text(e, e.r(3)).find(e.cstr(e.r(4)), e.r(5))))
    s.internal(0x100131f8, scalar(find_one))
    s.internal(0x100132c8, scalar(find_reverse))
    for a, kind in ((0x10012f10, "Mid"), (0x10013038, "Right"), (0x100130ac, "Left")):
        s.internal(a, lambda e, kind=kind: view(e, kind))

    def file_ctor(e):
        w32(e, e.r(3), 0x10071dec)
        w32(e, e.r(3) + 4, 0)

    def disc_ctor(e):
        file_ctor(e)
        w32(e, e.r(3), 0x100731ac)

    def file_close(e):
        rec("File.Close", (lab(e.r(3)),))
        w32(e, e.r(3) + 4, 0)

    def disc_open(e):
        rec("DiscFile.Open", (lab(e.r(4)), e.r(6)), (e.cstr(e.r(5)),))
        value = ret("DiscFile.Open", 0)
        if value >= 0:
            w32(e, e.r(4) + 4, e.ws_original)
        return value

    def file_read(e):
        rec("File.Read", (lab(e.r(3)), e.r(5)))
        words = e.ws_case["header"] if e.r(5) == 88 else e.ws_case["entry"]
        e.write(e.r(4), struct.pack("<%dI" % len(words), *words))
        return ret("File.Read", e.r(5))

    def file_attach(e):
        rec("File.Attach", (lab(e.r(3)), lab(e.r(4))))
        if e.r(4):
            w32(e, e.r(3) + 4, e.r(4))

    def disc_dtor(e):
        rec("DiscFile.dtor", (lab(e.r(3)),))
        rec("File.dtor", (lab(e.r(3)),))

    s.internal(0x1000b1dc, file_ctor)
    s.internal(0x100291a0, disc_ctor)
    s.internal(0x10029214, disc_dtor)
    s.internal(0x1000b2b8, CL.recorder("File.dtor", "L"))
    s.internal(0x1000b48c, file_close)
    s.internal(0x100292b4, error(disc_open))
    s.internal(0x10000f28, scalar(file_read))
    s.internal(0x1000b504, scalar(lambda e: (rec("File.Seek", (lab(e.r(3)), T.s32(e.r(4)), e.r(5))),
                                          ret("File.Seek", e.r(4)))[1]))
    s.internal(0x1000b678, scalar(lambda e: (rec("File.GetAccess", (lab(e.r(3)),)), e.u32(e.r(3) + 4))[1]))
    s.internal(0x1000b6c0, file_attach)

    def storage_ctor(e):
        w32(e, e.r(3), 0x10071e24)
        w32(e, e.r(3) + 4, 0)

    def storage_close(e):
        rec("Storage.Close", (lab(e.r(3)),))
        w32(e, e.r(3) + 4, 0)

    def storage_attach(e):
        rec("Storage.Attach", (lab(e.r(3)), lab(e.r(4))))
        w32(e, e.r(3) + 4, e.r(4))

    def storage_open_file(e):
        rec("Storage.OpenFile", (lab(e.r(4)), lab(e.r(5)), e.r(7), lab(e.r(8))), (e.cstr(e.r(6)),))
        return ret("Storage.OpenFile", 0)

    s.internal(0x1000b890, storage_ctor)
    s.internal(0x1000bb00, storage_close)
    s.internal(0x1000b998, CL.recorder("Storage.dtor", "L"))
    s.internal(0x1000bf88, storage_attach)
    s.internal(0x1000be50, error(storage_open_file))
    s.internal(0x1000af14, error(lambda e: (rec("DiscStorage.Open", (lab(e.r(4)),), (e.cstr(e.r(5)),)),
                                         ret("DiscStorage.Open", 0))[1]))

    def base_ctor(e):
        p = e.r(3)
        w32(e, p, 0x10071bf0)
        w32(e, p + 4, 0)
        w32(e, p + 12, 1)
        string_set(e, p + 16, "")

    s.internal(0x10007a00, base_ctor)
    s.internal(0x10007a9c, CL.recorder("StorageAccess.dtor", "L"))

    def tree_ctor(e):
        e.write(e.r(3), b"\0" * 28)

    def tree_close(e):
        rec("Tree.Close", (lab(e.r(3)),))
        w32(e, e.r(3), 0)

    def tree_initialise(e):
        rec("Tree.Initialise", (lab(e.r(4)), lab(e.r(5)), e.r(6)))
        w32(e, e.r(4), e.ws_original)
        return ret("Tree.Initialise", 0)

    def fill_info(e, info):
        w32(e, info, ret("Info.size", 91))
        w32(e, info + 4, ret("Info.attributes", 0x10))
        e.write(info + 8, b"\x07" * 24)
        e.write(info + 36, b"scope:found.dat\0")
        w32(e, info + 32, info + 42)

    def tree_first(e):
        rec("Tree.FindFirst", (lab(e.r(4)),), (string_text(e, e.r(6)), e.cstr(e.r(5) + 36)))
        fill_info(e, e.r(5))
        return ret("Tree.FindFirst", 0)

    def tree_next(e):
        rec("Tree.FindNext", (lab(e.r(4)),), (e.cstr(e.r(5) + 36),))
        fill_info(e, e.r(5))
        return ret("Tree.FindNext", 0)

    def iterator_ctor(e):
        rec("Iterator.ctor", (lab(e.r(4)),), (string_text(e, e.r(5)),))
        w32(e, e.r(6), ret("Iterator.offset", 88))
        p = e.r(3)
        string_set(e, p, "")
        string_set(e, p + 32, "")
        e.write(p + 60, struct.pack(">4I", 0x10071d20, 0, 0, 0))
        e.write(p + 76, struct.pack(">4I", 0x10071d20, 0, 0, 0))

    s.internal(0x10043d78, tree_ctor)
    s.internal(0x10043e0c, CL.recorder("Tree.dtor", "L"))
    s.internal(0x10043ec4, tree_close)
    s.internal(0x10044010, error(tree_initialise))
    s.internal(0x10044790, error(tree_first))
    s.internal(0x10044898, error(tree_next))
    s.internal(0x10044f94, iterator_ctor)
    s.internal(0x10045948, scalar(lambda e: (rec("Iterator.done"), ret("Iterator.done", 0))[1]))
    # Empty masks have no nodes; their inlined destructors make no edge calls.
    s.internal(0x1000a074, lambda e: None)

    def born(e, name):
        w32(e, e.r(3), {"Shared": 0x10071bac, "WadFile": 0x10071c48,
                       "Refpack": 0x10071b38, "Zlib": 0x10071d60}[name])
        rec(name + ".ctor", (lab(e.r(3)),))

    s.internal(0x10007360, lambda e: born(e, "Shared"))
    for ctor, opening, dtor, name in ((0x10008114, 0x100081a4, 0x10008620, "WadFile"),
                                     (0x10006bd4, 0x10006c64, 0x100072b4, "Refpack"),
                                     (0x1000a224, 0x1000a2b0, 0x10009c3c, "Zlib")):
        s.internal(ctor, lambda e, name=name: born(e, name))
        s.internal(opening, error(lambda e, name=name: (rec(name + ".Open", (lab(e.r(4)), e.r(5), e.r(6))
                                                              if name == "WadFile" else (lab(e.r(4)), e.r(5))),
                                                       ret(name + ".Open", 0))[1]))
        def compression_dtor(e, name=name):
            rec(name + ".dtor", (lab(e.r(3)),))
            if T.s16(e.r(4)) > 0:
                rec("delete", (lab(e.r(3)),))
                e.cl.forget(e.r(3))
        s.internal(dtor, compression_dtor)

    def expand(e):
        rec("LbFile_ToProjectFilename", (e.r(5),), (e.cstr(e.r(6)),))
        prefix = b"P:" if ret("Project.prefix", 1) else b""
        e.write(e.r(4), prefix + e.cstr(e.r(6)).encode("latin-1") + b"\0")
        return ret("LbFile_ToProjectFilename", 0)

    def swap(e):
        rec("SwapBlock32", (T.s32(e.r(4)),))
        for i in range(e.r(4)):
            p = e.r(3) + 4 * i
            e.write(p, e.read(p, 4)[::-1])

    s.internal(0x10001590, error(expand))
    s.imp("GetFileAttributes__10NS_MacDozeFPCc", scalar(lambda e: (rec("GetFileAttributes", (), (e.cstr(e.r(3)),)),
                                                               ret("GetFileAttributes", 0x80))[1]))
    for item in e.imports.values():
        if "_USwapBlock32" in item["name"]:
            s.imp(item["name"], swap)
    s.imp("strcat", lambda e: e.write(e.r(3), (e.cstr(e.r(3)) + e.cstr(e.r(4))).encode("latin-1") + b"\0"))
    def delete(e):
        if lab(e.r(3)) > 0:
            rec("delete", (lab(e.r(3)),))
            e.cl.forget(e.r(3))
    s.imp("__dl__FPv", delete)
    def allocate(e):
        size = e.r(3)
        p = 0 if ret("allocation failure", 0) else e.alloc(size, b"\x5a" * size)
        label = e.cl.newlab(p) if p else 0
        rec("new", (size, label))
        if p and size == 224:
            e.cl.labels[p + 196] = label + 200
            e.cl.labels[p + 184] = label + 300
        e.set_r(3, p)
    s.imp("__nw__FUl", allocate)
    return s


def case(r):
    header = [0] * 22
    header[0:2] = [r.choice([0x42465744, 0, 0x44465742]), r.randrange(5)]
    header[18:22] = [r.randrange(6), 400, 128, r.getrandbits(32)]
    entry = [r.getrandbits(32), 500, 12, r.randrange(200, 1000), r.randrange(128), r.randrange(8), r.randrange(128), 0, 0, 0]
    queues = {key: [r.choice([-7, -1, 0, 1, 37])] for key in
              ["Tree.Initialise", "Tree.FindFirst", "Tree.FindNext", "DiscFile.Open", "DiscStorage.Open",
               "WadFile.Open", "Refpack.Open", "Zlib.Open", "Storage.OpenFile", "LbFile_ToProjectFilename"]}
    queues.update({"GetFileAttributes": [r.choice([-1, 0x10, 0x80])],
                   "File.Read": [r.choice([0, 87, 88])], "Iterator.done": [r.randrange(2)],
                   "Info.attributes": [r.choice([0x10, 0x81, 0])]})
    return dict(header=header, entry=entry, queues=queues, original=r.randrange(2), file_open=r.randrange(2),
                tree_open=r.randrange(2), subpath=r.choice(["", ":", "scope:", ":sub:"]),
                info="prefix:previous.dat", name=r.choice(["data.wad", "dir/data.wad", "dir\\data.wad",
                "Disk:Data.wad*:scope", "Data.wad*:scope:", "folder/*.dat", "a:b", "a\\b", ""]),
                path=r.choice([None, "", "Wad:Data.wad", "folder"]),
                flags=r.choice([0, 1, 2, 0x8000, 0x80000001, 0x40000000, 0x10, 0x20]))


def edges():
    out = []
    r = random.Random(114)
    for signature, version in ((0, 0), (0x42465744, 0), (0x42465744, 1), (0x42465744, 2), (0x42465744, 3)):
        for name in ("data.wad", "dir/data.wad", "dir\\data.wad", "Data.wad*:scope", "Data.wad*:scope:"):
            for attributes in (0x10, 0x81):
                c = case(r)
                c.update(name=name, original=1, file_open=0, tree_open=0, flags=0x80000001)
                c["header"][:2] = [signature, version]
                c["queues"] = {"GetFileAttributes": [0x80], "Info.attributes": [attributes], "File.Read": [88],
                               "Tree.Initialise": [-1], "Iterator.done": [0]}
                out.append(c)
    for compression in range(8):
        for key in (None, "WadFile.Open", "Refpack.Open", "Zlib.Open", "Tree.FindFirst", "Tree.FindNext", "DiscFile.Open"):
            c = case(r)
            c.update(original=1, file_open=0, tree_open=1, flags=0x80000001)
            c["header"][:2] = [0x42465744, 2]
            c["entry"][5] = compression
            c["queues"] = {"File.Read": [88], "GetFileAttributes": [0x80], "Iterator.done": [0]}
            if key:
                c["queues"][key] = [-1]
            out.append(c)
    c = case(r)
    c.update(name=None, original=1)
    out.append(c)
    for name in ("file.dat", "/file.dat", ""):
        c = case(r)
        c.update(name=name, path=None, file_open=0)
        c["queues"]["Project.prefix"] = [0]
        out.append(c)
    c = case(r)
    c.update(name="Data.wad", original=1, file_open=0, tree_open=1, flags=0x80000001)
    c["header"][:2] = [0x42465744, 2]
    c["queues"] = {"allocation failure": [1, 0], "File.Read": [88],
                   "GetFileAttributes": [0x80], "Iterator.done": [0]}
    out.append(c)
    return out


class WadStorageTest:
    def __init__(self, op, spec):
        self.op, (self.addr, name) = op, spec
        self.name = "WAD storage " + name
        self.exact_nan = False

    def edge_cases(self):
        return edges()

    def random_case(self, r):
        return case(r)

    def run_case(self, runner, c):
        if c["name"] is None and self.op != 3:
            c = dict(c, name="data.wad")
        if "allocation failure" in c["queues"]:
            c = dict(c, queues=dict(c["queues"]))
            if self.op not in (2, 6, 7):
                del c["queues"]["allocation failure"]
            elif self.op == 7:
                c["file_open"] = 1
        e, lib, op = runner.emu, runner.lib, self.op
        e.reset_scratch()
        e.ws_case = c
        e.cl = CL.PpcLog({}, c["queues"])
        p = e.alloc(224, b"\x5a" * 224)
        file = e.alloc(8, struct.pack(">II", 0x10073038, 0))
        storage = e.alloc(8, struct.pack(">II", 0x10071ed4, 0))
        original = e.alloc(12, b"\0" * 12)
        e.ws_original = original
        info = e.alloc(296, b"\x5a" * 296)
        e.write(info + 36, c["info"].encode() + b"\0")
        w32(e, info + 32, info + 36)
        e.cl.labels.update({p: 1, file: 2, storage: 3, original: 4, p + 196: 5, p + 184: 6})
        w32(e, p, 0x10071cac)
        string_set(e, p + 16, "root")
        string_set(e, p + 36, c["subpath"])
        string_set(e, p + 76, "old-pattern")
        w32(e, p + 56, -1)
        e.write(p + 60, struct.pack(">4I", 0x10071d20, 0, 0, 0))
        e.write(p + 96, struct.pack(">22I", *c["header"]))
        w32(e, p + 184, 0x100731ac)
        w32(e, p + 188, original)
        w32(e, p + 192, original if c["original"] else 0)
        w32(e, p + 196, original if c["tree_open"] else 0)
        w32(e, file + 4, original if c["file_open"] else 0)
        out = e.alloc(4)
        name = e.alloc(len(c["name"]) + 2, b"\0" + c["name"].encode() + b"\0") + 1 if c["name"] is not None else 0
        path = CL.new_text(e, c["path"]) if c["path"] is not None else 0
        args = [out, p]
        if op == 3:
            args += [name, info]
        elif op == 4:
            args += [info]
        elif op in (6, 7):
            args += [file, name]
            if op == 6:
                args += [c["flags"], 0]
        elif op == 12:
            args = [out, storage, name, c["flags"]]
        elif op == 13:
            args = [out, file, name, path]
        elif op in (2, 8):
            args += [name, c["flags"]]
        elif op in (9, 10, 11):
            args += [name]
            if op == 11:
                args += [path]
        else:
            args = [p, info] if op == 5 else [p, T.S16(c["flags"])] if op == 14 else [file, T.S16(c["flags"])] if op == 15 else [p]
        s = stubs(e)
        saved = s.activate(e)
        try:
            T.call_ppc(e, self.addr, args)
        finally:
            s.deactivate(e, saved)
        result = T.s32(e.u32(out)) if op in ERROR else 0
        values = [T.u32(e.cl.lab(e.u32(p + 192))), T.u32(e.cl.lab(e.u32(p + 188))),
                  T.u32(e.cl.lab(e.u32(file + 4))), e.u32(p + 56), e.u32(info + 32) - info - 36,
                  e.u32(info), e.u32(info + 4)]
        # On short/invalid reads the words that have not been swapped are file bytes.
        header = list(struct.unpack(">22I", e.read(p + 96, 88)))
        if any(event[0] == "File.Read" and event[1] == (6, 88) for event in e.cl.log):
            swapped = any(event[0] == "SwapBlock32" and event[1] == (3,) for event in e.cl.log)
            for i in range(22):
                if not (swapped and (i in (0, 1, 18, 19, 20, 21))):
                    header[i] = int.from_bytes(header[i].to_bytes(4, "big"), "little")
        values += header
        values += list(e.read(info + 8, 24))
        values += [T.u32(e.cl.lab(e.u32(p + 196))), T.u32(e.cl.lab(e.u32(storage + 4)))]
        text = "|".join([string_text(e, p + a) for a in (16, 36, 76)] + [e.cstr(info + 36)])
        expected = (result, values, text, e.cl.log)

        CL.host_begin(lib, {}, c["queues"])
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        hheader, hentry = (C.c_uint32 * 22)(*c["header"]), (C.c_uint32 * 10)(*c["entry"])
        F(lib, "tpw_ws_begin", None, C.c_void_p, C.c_void_p, C.c_char_p, C.c_int32, C.c_int32,
          C.c_int32, C.c_char_p)(hheader, hentry, c["subpath"].encode(), c["original"], c["file_open"],
                                c["tree_open"], c["info"].encode())
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
        actual_result = F(lib, "tpw_ws_call", C.c_int32, C.c_int32, C.c_char_p, C.c_char_p, C.c_uint32)(
            op, c["name"].encode() if c["name"] is not None else None,
            c["path"].encode() if c["path"] is not None else None, c["flags"])
        log = CL.host_end(lib)
        hvalues, htext = (C.c_uint32 * 55)(), C.create_string_buffer(1200)
        F(lib, "tpw_ws_state", None, C.c_void_p, C.c_void_p)(hvalues, htext)
        actual = (actual_result, list(hvalues), htext.value.decode(), log)
        return T.same_values(expected, actual, "", False)


TESTS = [WadStorageTest(op, spec) for op, spec in enumerate(SPECS)]
