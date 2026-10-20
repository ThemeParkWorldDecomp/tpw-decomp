"""Bullfrog access layers: named fields, call arguments, chain ownership and paths."""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_bflFileSystem"
SPECS = [
    (0x10003e60, "Buffer constructor", 1), (0x10003efc, "Buffer Open", 1),
    (0x100052a8, "FileStorageAccess constructor", 6), (0x10005a18, "FileStorageAccess Open", 6),
    (0x100067e8, "QuickPos constructor", 3), (0x10007360, "Shared constructor", 4),
    (0x10007a00, "StorageAccess constructor", 5), (0x10007b54, "StorageAccess AddRef", 5),
    (0x1000a818, "Access constructor", 0), (0x1000a8f4, "Access AddRef", 0),
    (0x1000a998, "Access Release", 0), (0x1000af14, "DiscStorage Open", 11),
    (0x1000b1dc, "File constructor", 7), (0x1000b2b8, "File destructor", 7),
    (0x1000b48c, "File Close", 7), (0x1000b504, "File Seek", 7),
    (0x1000b588, "File GetSize", 7), (0x1000b600, "File GetPos", 7),
    (0x1000b678, "File GetAccessClass", 7), (0x1000b6c0, "File AttachAccessClass", 7),
    (0x1000b890, "Storage constructor", 8), (0x1000b998, "Storage destructor", 8),
    (0x1000ba40, "Storage assignment", 8), (0x1000bcec, "Storage FindFirst", 8),
    (0x1000bda0, "Storage FindNext", 8), (0x1000be50, "Storage OpenFile", 8),
    (0x1000bf88, "Storage AttachAccessClass", 8), (0x100291a0, "DiscFile constructor", 9),
    (0x10029214, "DiscFile destructor", 9), (0x100292b4, "DiscFile Open", 9),
    (0x10049430, "BufferedFile Open", 10),
]
VT = {0: 0x10071da4, 1: 0x1007194c, 3: 0x10071ab8, 4: 0x10071bac,
      5: 0x10071bf0, 6: 0x10071a00, 7: 0x10071dec, 8: 0x10071e24,
      9: 0x100731ac, 10: 0x10072ff4, 11: 0x10071e88}
SIZE = {0: 12, 1: 40, 3: 16, 4: 16, 5: 36, 6: 40, 7: 8, 8: 8, 9: 8, 10: 8, 11: 8}
CTOR = {0, 2, 4, 5, 6, 8, 12, 20, 27}
ERROR = {1, 3, 11, 23, 24, 25, 29, 30}


def w32(e, a, v):
    e.write(a, struct.pack(">I", T.u32(v)))


def string_set(e, a, text):
    e.write(a, struct.pack(">5I", 0x10072580, len(text), CL.new_text(e, text), 0, 0))


def string_text(e, a):
    return e.cstr(e.u32(a + 8))


def setup(runner):
    T.install_std_stubs(runner.emu)


def label_members(e, p, kind, label):
    if kind <= 4:
        e.cl.labels[p + 8] = 200 + label
    elif kind in (5, 6):
        e.cl.labels[p + 12] = 200 + label
        e.cl.labels[p + 8] = 300 + label
        e.cl.labels[p + 16] = 400 + label


def read_state(e, p, kind):
    out = [0] * 12
    text = ""
    if kind <= 4:
        out[:2] = [T.u32(e.cl.lab(e.u32(p + 4))), e.u32(p + 8)]
        if kind == 1:
            out[2:4] = [T.u32(e.cl.lab(e.u32(p + 12))), T.u32(e.cl.lab(e.u32(p + 16)))]
            out[4:9] = [e.u32(p + offset) for offset in range(20, 40, 4)]
        elif kind in (3, 4):
            out[2] = e.u32(p + 12)
    elif kind in (5, 6):
        out[:6] = [T.u32(e.cl.lab(e.u32(p + 4))), e.u32(p + 12), e.u32(p + 8),
                   e.u32(p + 20), e.u32(p + 28), e.u32(p + 32)]
        text = string_text(e, p + 16)
        if kind == 6:
            out[6] = T.u32(e.cl.lab(e.u32(p + 36)))
    else:
        out[0] = T.u32(e.cl.lab(e.u32(p + 4)))
    return out, text


def build_stubs(e):
    s = CL.Stubs()
    s.real.update(addr for addr, _, _ in SPECS)
    rec = e.cl.rec
    lab = e.cl.lab

    def fake_entry(fn):
        entry = e.alloc(4, b"\x4e\x80\x00\x20")
        vec = e.alloc(8, struct.pack(">II", entry, e.toc))
        s.internal(entry, fn)
        return vec

    def virtual_scalar(name, args="L"):
        return CL.recorder(name, args, "i")

    def access_dtor(em):
        p = em.r(3)
        rec("Access::dtor", (lab(p), lab(em.u32(p + 4)), T.s32(em.u32(p + 8))))
        if T.s16(em.r(4)) > 0:
            rec("delete", (lab(p),))
            e.cl.forget(p)

    s.internal(0x1000a86c, access_dtor)
    s.internal(0x1000a958, CL.recorder("Access::Close", "L"))
    s.internal(0x1000ac38, CL.recorder("Access::Attach", "LL"))
    # Pure base slots receive fakes only on the prepared fake objects.
    avt = e.alloc(60, e.read(VT[0], 60))
    for slot, name, args in [(12, "Read", "LLu"), (16, "Write", "LLu"),
                             (20, "Seek", "Liu"), (24, "GetSize", "L"), (28, "GetPos", "L")]:
        w32(e, avt + slot, fake_entry(virtual_scalar("Access::" + name, args)))
    e.bfl_fake_access_vt = avt

    def error_fake(name, operation):
        def fake(em):
            out, p = em.r(3), em.r(4)
            if operation == "OpenFile":
                rec(name, (lab(p), lab(em.r(5)), em.r(7), lab(em.r(8))),
                    (em.cstr(em.r(6)) if em.r(6) else "(null)",))
            elif operation == "FindFirst":
                rec(name, (lab(p), lab(em.r(6))), (em.cstr(em.r(5)) if em.r(5) else "(null)",))
            else:
                rec(name, (lab(p), lab(em.r(5))))
            w32(em, out, e.cl.ret(name))
        return fake

    svt = e.alloc(60, e.read(VT[5], 60))
    for slot, method in [(12, "OpenFile"), (16, "FindFirst"), (20, "FindNext")]:
        w32(e, svt + slot, fake_entry(error_fake("_TbStorageAccess::" + method, method)))
    e.bfl_fake_storage_vt = svt
    for addr, method in [(0x100058a0, "OpenFile"), (0x10005438, "FindFirst"), (0x100055f0, "FindNext")]:
        s.internal(addr, error_fake("_TbFileStorageAccess::" + method, method))
    s.internal(0x10007e64, CL.recorder("StorageAccess::Attach", "LL"))
    s.internal(0x1000bb00, CL.recorder("Storage::Close", "L"))
    # Native fake destructors automatically run their base/member destructors.
    def buffer_dtor(em):
        rec("Buffer::dtor", (lab(em.r(3)),))
        access_dtor(em)
    s.internal(0x10004890, buffer_dtor)
    s.internal(0x10003fc4, CL.recorder("Buffer::Close", "L"))
    s.internal(0x100044a0, CL.recorder("Buffer::Attach", "LL"))

    def storage_dtor(em, derived):
        p, label = em.r(3), lab(em.r(3))
        if derived:
            rec("DiscAccess::dtor", (label,))
        rec("StorageAccess::dtor", (label,))
        rec("Semaphore::dtor", (lab(p + 8),))
        if T.s16(em.r(4)) > 0:
            rec("delete", (label,))
            e.cl.forget(p)
    s.internal(0x10005330, lambda em: storage_dtor(em, True))
    s.internal(0x10007a9c, lambda em: storage_dtor(em, False))
    s.internal(e.func("__ct__12_LbSemaphoreFv"), CL.recorder("Semaphore::ctor", "L"))

    def interlocked(delta):
        def fake(em):
            p = em.r(3)
            old = em.u32(p)
            rec("InterlockedIncrement" if delta == 1 else "InterlockedDecrement", (lab(p), T.s32(old)))
            w32(em, p, old + delta)
            em.set_r(3, T.u32(old + delta))
        return fake
    s.imp("InterlockedIncrement__10NS_MacDozeFPl", interlocked(1))
    s.imp("InterlockedDecrement__10NS_MacDozeFPl", interlocked(-1))
    def attributes(em):
        rec("GetFileAttributes", (), (em.cstr(em.r(3)),))
        em.set_r(3, T.u32(e.bfl_case["attrs"]))
    s.imp("GetFileAttributes__10NS_MacDozeFPCc", attributes)
    s.imp("IsDBCSLeadByte__10NS_MacDozeFUc", CL.recorder("IsDBCSLeadByte", "b", "i"))
    def expand(em):
        rec("LbFile_ToProjectFilename", (em.r(5),), (em.cstr(em.r(6)) if em.r(6) else "(null)",))
        em.write(em.r(4), e.bfl_case["expanded"].encode("latin-1") + b"\0")
        w32(em, em.r(3), e.cl.ret("LbFile_ToProjectFilename"))
    s.internal(0x10001590, expand)

    def allocate(array):
        def fake(em):
            size = em.r(3)
            fail = e.cl.ret("array failure" if array else "allocation failure")
            p = 0 if fail else em.alloc(size or 1, b"\x5a" * (size or 1))
            label = e.cl.newlab(p) if p else 0
            rec("new[]" if array else "new", (size, label))
            if p and not array:
                kind = 1 if e.bfl_op == 30 else 6
                e.bfl_born[label] = (p, kind)
                label_members(e, p, kind, label)
            em.set_r(3, p)
        return fake
    s.imp("__nw__FUl", allocate(False))
    s.imp("__nwa__FUl", allocate(True))
    def delete(em):
        rec("delete", (lab(em.r(3)),))
        e.cl.forget(em.r(3))
    s.imp("__dl__FPv", delete)

    def ctor0(em):
        string_set(em, em.r(3), "")
    def ctor_text(em):
        string_set(em, em.r(3), em.cstr(em.r(4)) if em.r(4) else "")
    def ctor_base(em):
        p = em.r(4)
        string_set(em, em.r(3), em.read(em.u32(p + 8), em.u32(p + 4)).decode("latin-1"))
    def assign_base(em):
        ctor_base(em)
    def append_text(em):
        string_set(em, em.r(3), string_text(em, em.r(3)) + em.cstr(em.r(4)))
    def append_char(em):
        ch = em.read(em.r(4), 1)[0]
        rec("String::appendChar", (lab(em.r(3)), ch))
        string_set(em, em.r(3), string_text(em, em.r(3)) + chr(ch))
    def view(mid):
        def fake(em):
            p, count = em.r(4), em.r(5)
            text = string_text(em, p)
            string_set(em, em.r(3), text[count:] if mid else text[:count])
        return fake
    def reverse(em):
        text, mask, start = string_text(em, em.r(3)), em.cstr(em.r(4)), em.r(5)
        rec("String::FindReverseOneOf", (start,), (text, mask))
        found = -1
        for i in range(min(start, len(text) - 1), -1, -1):
            if text[i] in mask:
                found = i
                break
        em.set_r(3, T.u32(found))
    for addr, fn in [(0x100104c4, ctor0), (0x10010328, ctor_text), (0x10010248, ctor_base),
                     (0x1000e364, assign_base), (0x1000e44c, ctor_text), (0x1000e758, append_text),
                     (0x1000e6a8, append_char),
                     (0x1000e2c8, lambda em: None), (0x10011ccc, lambda em: em.set_r(3, em.u32(em.r(3) + 4))),
                     (0x10012f10, view(True)), (0x100130ac, view(False)), (0x10013118, reverse)]:
        s.internal(addr, fn)
    return s


def case(r):
    names = [None, "", "file.dat", "/root/file", "\\root\\file", "Disk:file", "a/b:c/d",
             "/leading", "trailing:", "a\\b/c:d", "abc\x81/file", "abc:\x81/file"]
    keys = ["Access::Seek", "Access::GetSize", "Access::GetPos", "_TbStorageAccess::OpenFile",
            "_TbStorageAccess::FindFirst", "_TbStorageAccess::FindNext", "_TbFileStorageAccess::OpenFile"]
    queues = {key: [r.choice([-7, -1, 0, 1, 17, 0x7fffffff])] for key in keys}
    queues["IsDBCSLeadByte"] = r.choice([[0], [1], [1, 0], [0, 1]])
    queues["LbFile_ToProjectFilename"] = [r.choice([-1, 0, 1])]
    return dict(name=r.choice(names), expanded=r.choice(["Root", "Root:", "Root/", "Root\\", "App:Data"]),
                attrs=r.choice([-1, 0, 0x10, 0x20]), depth=r.randrange(11), old=r.randrange(2),
                self=r.randrange(2), other_null=r.randrange(2), source_null=r.randrange(2),
                refs=[T.u32(r.choice([-1, 0, 1, 2, 3, -0x80000000, 0x7fffffff])) for _ in range(12)],
                seed=r.getrandbits(32), x=r.choice([0, 1, 8, 32, 128]), y=r.getrandbits(32),
                flag=r.choice([-0x8000, -1, 0, 1, 0x7fff, 0x10000, 0x18000]),
                queues=queues, array_fail=r.randrange(2), alloc_fail=r.randrange(2))


def edges():
    r = random.Random(31)
    out = []
    for depth in range(11):
        for stop in range(depth + 1):
            c = case(r)
            c.update(depth=depth, old=1)
            c["refs"] = [1] * 12
            if stop < depth:
                c["refs"][stop + 1] = 2
            out.append(c)
    for name in [None, "", "file", "/file", "\\file", ":file", "a/b:c/d", "a/b/", "abc\x81/file"]:
        for attrs in (-1, 0, 0x10):
            for lead in ([0], [1], [1, 0]):
                c = case(r)
                c.update(name=name, attrs=attrs, expanded="App:")
                c["queues"]["IsDBCSLeadByte"] = lead
                out.append(c)
    return out


def F(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = result, list(args)
    return fn


class FileSystemTest:
    def __init__(self, op, spec):
        self.op, (self.addr, name, self.kind) = op, spec
        self.name = "bfl::" + name
        self.exact_nan = False

    def edge_cases(self):
        return edges()

    def random_case(self, r):
        return case(r)

    def run_case(self, runner, c):
        e, lib, op = runner.emu, runner.lib, self.op
        e.reset_scratch()
        e.bfl_op, e.bfl_case, e.bfl_born = op, c, {}
        queues = dict(c["queues"])
        queues["allocation failure"] = [c["alloc_fail"] if op == 30 else 0]
        queues["array failure"] = [c["array_fail"] if op in (1, 30) else 0]
        e.cl = CL.PpcLog({}, queues)
        s = build_stubs(e)
        objects = {}
        data = e.alloc(128, b"D" * 128)
        info = e.alloc(512, b"I" * 512)
        e.cl.labels[data], e.cl.labels[info] = 5, 4
        def make(label, kind, nextp, refs):
            p = e.alloc(SIZE[kind], b"\0" * SIZE[kind])
            w32(e, p, e.bfl_fake_access_vt if kind == 0 else e.bfl_fake_storage_vt if kind == 5 else VT[kind])
            if kind <= 4:
                w32(e, p + 4, nextp)
                w32(e, p + 8, refs)
                if kind == 1:
                    w32(e, p + 12, data)
                    w32(e, p + 16, data)
                    for offset in range(20, 40, 4):
                        w32(e, p + offset, c["seed"])
                elif kind in (3, 4):
                    w32(e, p + 12, c["seed"])
            elif kind in (5, 6):
                w32(e, p + 4, nextp)
                w32(e, p + 8, c["seed"])
                w32(e, p + 12, refs)
                string_set(e, p + 16, "old")
                if kind == 6:
                    w32(e, p + 36, data)
            else:
                w32(e, p + 4, nextp)
            e.cl.labels[p] = label
            label_members(e, p, kind, label)
            objects[label] = (p, kind, nextp, refs)
            return p
        chain = 0
        for i in reversed(range(c["depth"])):
            chain = make(20 + i, 0, chain, c["refs"][i + 1])
        old_storage = make(40, 5, 0, 2)
        source = make(3, 7, 0 if c["source_null"] else chain, 0)
        storage_op = self.kind in (5, 6, 8, 11)
        current = old_storage if storage_op else chain
        p = make(1, self.kind, current if c["old"] else 0, c["refs"][0])
        other_kind = 8 if op == 22 else 5 if op == 26 else 0
        other = make(2, other_kind, old_storage if op == 22 else 0, c["refs"][-1])
        if op == 22 and c["self"]:
            other = p
        if op == 19 and c["other_null"]:
            other = 0
        name = CL.new_text(e, c["name"]) if c["name"] is not None else 0
        result = e.alloc(4)
        if op in CTOR or op in (7, 9, 10, 12, 14, 16, 17, 18, 20, 27):
            args = [p]
        elif op in (1, 3, 11, 29, 30):
            args = [result, p, source if op == 30 else c["x"] if op == 1 else name]
            if op in (29, 30):
                args.append(c["x"])
        elif op in (13, 21, 28):
            args = [p, T.S16(c["flag"])]
        elif op == 15:
            args = [p, c["x"], c["y"]]
        elif op in (19, 22, 26):
            args = [p, other]
        elif op == 23:
            args = [result, p, name, info]
        elif op == 24:
            args = [result, p, info]
        else:
            args = [result, p, source, name, c["x"], data]
        saved = s.activate(e)
        try:
            value = T.call_ppc(e, self.addr, args)[0]
        finally:
            s.deactivate(e, saved)
        ret = T.s32(e.u32(result)) if op in ERROR else T.s32(value) if op in (10, 15, 16, 17) else e.cl.lab(value) if op in (18, 22) else 0
        abi_ok = value == p if op in CTOR or op in (13, 21, 28) else True
        states = {label: read_state(e, address, kind) for label, (address, kind, _, _) in objects.items()}
        born = {label: (kind, read_state(e, address, kind)) for label, (address, kind) in e.bfl_born.items()}
        expected = dict(ret=ret, states=states, born=born, log=e.cl.log, abi_ok=abi_ok)

        # Create/seed native objects before the observed call; native fields use accessors.
        F(lib, "tpw_bfl_config", None, C.c_char_p, C.c_int32)(c["expanded"].encode("latin-1"), c["attrs"])
        hdata, hinfo = C.create_string_buffer(b"D" * 128, 128), C.create_string_buffer(b"I" * 512, 512)
        host_objects = {label: F(lib, "tpw_bfl_new", C.c_void_p, C.c_int32)(kind)
                        for label, (_, kind, _, _) in objects.items()}
        addresses = {address: host_objects[label] for label, (address, _, _, _) in objects.items()}
        CL.host_begin(lib, {5: C.addressof(hdata), 4: C.addressof(hinfo)}, queues)
        for label, (_, kind, nextp, refs) in objects.items():
            F(lib, "tpw_bfl_seed", None, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p,
              C.c_uint32, C.c_uint32, C.c_void_p)(host_objects[label], kind, label,
                                                addresses.get(nextp), refs, c["seed"], hdata)
        hname = c["name"].encode("latin-1") if c["name"] is not None else None
        hret = F(lib, "tpw_bfl_call", C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p,
                 C.c_void_p, C.c_char_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_int32)(
                     op, host_objects[1], addresses.get(other), host_objects[3], hinfo,
                     hname, c["x"], c["y"], hdata, c["flag"])
        log = CL.host_end(lib)
        hstates, hborn = {}, {}
        for label, (_, kind, _, _) in objects.items():
            words, text = (C.c_uint32 * 12)(), C.create_string_buffer(512)
            F(lib, "tpw_bfl_state", None, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p)(
                host_objects[label], kind, label, words, text)
            hstates[label] = (list(words), text.value.decode("latin-1"))
        for label in range(100, 110):
            words, text, kind = (C.c_uint32 * 12)(), C.create_string_buffer(512), C.c_int32()
            exists = F(lib, "tpw_bfl_born", C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p)(
                label, words, C.byref(kind), text)
            if exists:
                hborn[label] = (kind.value, (list(words), text.value.decode("latin-1")))
        actual = dict(ret=hret, states=hstates, born=hborn, log=log, abi_ok=True)
        return T.same_values(expected, actual, "", False)


TESTS = [FileSystemTest(op, spec) for op, spec in enumerate(SPECS)]
