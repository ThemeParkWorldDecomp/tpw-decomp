"""The archive name tree and the wildcard masks (bullfrog library: WadNameTree,
TreeIterator, DirNode, FileNode, the two node allocators and node lists, 0x10043d78..
0x10046868 and 0x10009d8c; TbStringMatch and TbCharStringList, 0x100087bc, 0x1000885c,
0x10013850..0x10013c68; src/bfl_WadFile.cpp, src/TbStringMatch.cpp).

Both sides run their real code all the way down (the tree, its iterator, the allocators,
the lists, the masks, the bullfrog strings): only the archive is a fake, a file object over
the case's bytes whose Seek and Read record what they are asked and can be told to fail.

A mask case sets a mask and matches a text: the two results and the mask's parts compare.
A tree case is an archive directory the test builds (versions 0 to 2, folders that stick
to the entries after them, hashes right or wrong) and a script: Initialise, FindFirst and
FindNext with patterns, exact lookups through an iterator, Close, destruction. Every
answer compares as text (result, size, attributes, the whole name buffer, where the name
starts in it), and so does the sequence of seeks and reads.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_bflWadTree"

TREE_CTOR, TREE_DTOR, TREE_CLOSE, TREE_INIT, FIND_FIRST, FIND_NEXT = (
    0x10043d78, 0x10043e0c, 0x10043ec4, 0x10044010, 0x10044790, 0x10044898)
ITERATOR_CTOR, ITERATOR_DONE, ITERATOR_DTOR = 0x10044f94, 0x10045948, 0x10009d8c
SET_MASK, MATCH_STRING, MASK_DTOR, MASK_VTABLE, STRING_VTABLE = 0x10013850, 0x10013a5c, 0x100087bc, 0x10071d20, 0x10072580
MINUS = 0xFFFFFFFF
AS_ASKED = -9999          # a queued result that means "what was asked for"
SKIPPED = "(left out)"


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.heap_in_scratch = True          # every case builds and frees trees: the heap is the case's own
    if not hasattr(e, "wt_fns"):
        e.wt_fns = {name: T.fake_function(e, (lambda n: lambda emu: getattr(emu.wt_side, n)())(name))
                    for name in ("file_read", "file_seek", "file_other")}
        # The file object's vtable: Read at 0x10 and Seek at 0x18, as in CFile's.
        e.wt_vtable = T.persist_alloc(e, 0x40)
        for slot in range(8, 0x40, 4):
            e.write(e.wt_vtable + slot, struct.pack(">I", e.wt_fns["file_other"]))
        e.write(e.wt_vtable + 0x10, struct.pack(">I", e.wt_fns["file_read"]))
        e.write(e.wt_vtable + 0x18, struct.pack(">I", e.wt_fns["file_seek"]))

        # Arrays of nodes: the constructor or destructor of each element runs as original code.
        def construct(emu, call):
            block, ctor, size, count = emu.r(3), emu.r(4), emu.r(6), emu.r(7)
            if not block:
                return 0
            emu.write(block, struct.pack(">II", size, count))
            for i in range(count):
                call(ctor, block + 8 + i * size, 1)
            return block + 8

        # (The runtime frees the block itself: that counts as a delete[] in the heap traffic.)
        def destroy(emu, call):
            first, dtor = emu.r(3), emu.r(4)
            if first and dtor:
                size, count = struct.unpack(">II", emu.read(first - 8, 8))
                for i in reversed(range(count)):
                    call(dtor, first + i * size, MINUS)
            if first:
                emu.alloc_log.append(("delete[]", first - 8, 0))
            return None

        # The C library's string functions the tree and the masks import.
        text = lambda emu, a: emu.read(a, 600).split(b"\0")[0]

        def strncpy(emu):
            dest, source, count = emu.r(3), text(emu, emu.r(4)), emu.r(5)
            emu.write(dest, (source[:count] + bytes(count))[:count])

        def strncmp(emu):
            a, b = text(emu, emu.r(3))[:emu.r(5)], text(emu, emu.r(4))[:emu.r(5)]
            emu.set_r(3, T.u32((a > b) - (a < b)))

        def strcmp(emu):
            a, b = text(emu, emu.r(3)), text(emu, emu.r(4))
            emu.set_r(3, T.u32((a > b) - (a < b)))

        def strstr(emu):
            at = text(emu, emu.r(3)).find(text(emu, emu.r(4)))
            emu.set_r(3, emu.r(3) + at if at >= 0 else 0)

        def strncat(emu):
            dest, source = emu.r(3), text(emu, emu.r(4))[:emu.r(5)]
            emu.write(dest + len(text(emu, dest)), source + b"\0")

        for name, fn in (("strncpy", strncpy), ("strncmp", strncmp), ("strcmp", strcmp), ("strstr", strstr),
                         ("strncat", strncat)):
            e.stub(name, fn)
        T.stub_cb(e, "__construct_new_array", construct)
        T.stub_cb(e, "__destroy_new_array", destroy)

        # The C library's case maps (data imports): the tree folds names through them.
        e.write(e.data_import("__lower_map"), bytes(c + 32 if 0x41 <= c <= 0x5a else c for c in range(256)))
        e.write(e.data_import("__upper_map"), bytes(c - 32 if 0x61 <= c <= 0x7a else c for c in range(256)))


def original_code(fn):
    """The PowerPC side of a case with every function of the library as the original has it:
    earlier modules of the suite leave replacements of their own in the shared emulator (the
    strings' buffer functions, which then do not go through operator new[])."""
    def run(e, case):
        stubs = getattr(e, "internal_stubs", {})
        saved = dict(stubs)
        stubs.clear()
        try:
            return fn(e, case)
        finally:
            stubs.update(saved)
    return run


def string(e, text):
    """A dynamic string object holding `text`, as the library's own constructor leaves one."""
    data = text.encode("latin-1") + b"\0"
    place = e.alloc(0x14)
    e.write(place, struct.pack(">5I", STRING_VTABLE, len(data) - 1, e.alloc(len(data) + 8, data), len(data), 0))
    return place


# ------------------------------------------------------------------ masks


@original_code
def mask_ppc(e, case):
    mask = e.alloc(0x10, struct.pack(">4I", MASK_VTABLE, 0, 0, 0))
    result = e.alloc(4)
    T.call_ppc(e, SET_MASK, [result, mask, e.alloc(64, case["mask"].encode("latin-1") + b"\0")])
    parts, node = "", e.u32(mask + 4)
    while node:                                      # a node: the string (text at +8), then the links
        parts += e.cstr(e.u32(node + 8)) + "|"
        node = e.u32(node + 0x14)
    matched = T.call_ppc(e, MATCH_STRING, [mask, e.alloc(64, case["text"].encode("latin-1") + b"\0")])[0] & 0xff
    return [T.s32(e.u32(result)) * 2 + (1 if matched else 0), parts]


def mask_host(lib, case):
    fn = lib["tpw_wt_mask"]
    fn.restype, fn.argtypes = C.c_int32, [C.c_char_p, C.c_char_p, C.c_char_p]
    parts = C.create_string_buffer(1024)
    result = fn(case["mask"].encode("latin-1"), case["text"].encode("latin-1"), parts)
    return [result, parts.value.decode("latin-1")]


MASKS = ["*", "?", "abc", "a*", "*c", "a*c", "a?c", "*.*", "*.md2", "a**c", "a*?c", "?*", "*?", "??", "a?", "?b?", "",
         "a*b*c", "*a*", "ab", "a.b", "*.", ".*", "a?*", "**", "a*b?c*", "x*y", "abc*", "*abc", "a?c.*", "***a"]
TEXTS = ["", "a", "ab", "abc", "ac", "abcd", "a.b", "abc.md2", ".md2", "md2", "aXc", "aXXc", "c", "ca", "abcabc", "a.",
         "x", "xy", "xzy", "abbc", "abc.", ".", "aab", "ba"]


def mask_edge():
    for mask in MASKS:
        for text in TEXTS:
            yield dict(mask=mask, text=text)


def mask_rnd(r):
    letters = "ab.*?"
    return dict(mask="".join(r.choice(letters) for _ in range(r.randrange(0, 7))),
                text="".join(r.choice("ab.") for _ in range(r.randrange(0, 8))))


# ------------------------------------------------------------------ the archive


def name_hash(path):
    """The directory's hash of an entry: over its whole path in upper case with '\\' between
    the folders, which is the form a lookup hashes (all 14,099 entries of the disc agree)."""
    data = path.replace("/", "\\").replace(":", "\\").upper().encode("latin-1")
    total, i = 0, 0
    while len(data) - i >= 4:
        for k in range(4):
            total += (MINUS if data[i + k] == 0xff else data[i + k]) << k
        i += 4
    for byte in data[i:]:
        total += 255 if byte == 0xff else byte
    return total & MINUS


def archive(r, version, entries):
    """entries: (stored name, full path, size, compressed size or None, good hash)."""
    count = len(entries)
    names = b""
    records = []
    name_offset = 0x58 + 40 * count
    for stored, path, size, packed, good in entries:
        records.append((name_hash(path) if good else r.getrandbits(32), name_offset + len(names), len(stored),
                        r.randrange(0x1000), size if packed is None else packed, 0 if packed is None else 4,
                        r.getrandbits(16) if packed is None else size))
        names += stored.encode("latin-1") + b"\0"
    header = b"DWFB" + struct.pack("<I", version) + bytes(64) + struct.pack("<4I", count, name_offset, len(names), 0)
    body = b"".join(struct.pack("<7I", *record) + bytes(12) for record in records)
    return header + body + names


class PpcSide:
    def __init__(self, e, case):
        self.e, self.data, self.position = e, case["file"], 0
        e.wt_side = self
        self.L = CL.PpcLog({}, case.get("q"))
        e.cl = self.L
        self.file = e.alloc(8, struct.pack(">II", e.wt_vtable, 0))
        self.tree = e.alloc(0x1c)
        self.result = e.alloc(4)
        self.info = e.alloc(0x128)
        self.call(TREE_CTOR, self.tree)
        self.searching = False

    def file_read(self):
        e, L = self.e, self.L
        dest, size = e.r(4), e.r(5)
        L.rec("File.Read", (size, self.position))
        chunk = self.data[self.position:self.position + size]
        result = L.ret("Read", AS_ASKED)
        result = len(chunk) if result == AS_ASKED else result
        e.write(dest, b"\xee" * size)
        if chunk:
            e.write(dest, chunk)
        self.position += size
        e.set_r(3, T.u32(result))

    def file_seek(self):
        e, L = self.e, self.L
        L.rec("File.Seek", (e.r(4), e.r(5)))
        self.position = e.r(4)
        result = L.ret("Seek", AS_ASKED)
        e.set_r(3, e.r(4) if result == AS_ASKED else T.u32(result))

    def file_other(self):
        self.L.rec("File.other")
        self.e.set_r(3, 0)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]

    def describe(self):
        e, info = self.e, self.info
        result = T.s32(e.u32(self.result))
        if result < 0:
            return str(result)
        return f"{result}|{e.u32(info)}|{e.u32(info + 4)}|{e.cstr(info + 0x24)}|{e.u32(info + 0x20) - (info + 0x24)}"

    def fresh_info(self):
        e = self.e
        e.write(self.info, bytes(0x128))
        e.write(self.info + 0x20, struct.pack(">I", self.info + 0x24))

    def op(self, op, a, text):
        e = self.e
        if op == 0:
            self.call(TREE_INIT, self.result, self.tree, self.file, a)
            self.searching = True
            return str(T.s32(e.u32(self.result)))
        if op in (1, 2):
            if not self.searching:          # the tree's own iterator is made by Initialise
                return None
            self.fresh_info()
            if op == 1:
                self.call(FIND_FIRST, self.result, self.tree, self.info, string(e, text))
            else:
                self.call(FIND_NEXT, self.result, self.tree, self.info)
            return self.describe()
        if op == 3:
            iterator = e.alloc(0x60, bytes(0x60))
            offset = e.alloc(4, struct.pack(">I", 0x5a5a5a5a))
            self.call(ITERATOR_CTOR, iterator, self.tree, string(e, text), offset)
            done = self.call(ITERATOR_DONE, iterator) & 0xff
            answer = f"{1 if done else 0}|{e.u32(offset)}"
            self.call(ITERATOR_DTOR, iterator, MINUS)
            return answer
        if op == 4:
            self.call(TREE_CLOSE, self.tree)
            self.searching = False
            return ""
        if op == 5:
            self.call(TREE_DTOR, self.tree, MINUS)
            self.call(TREE_CTOR, self.tree)
            self.searching = False
            return ""
        return "?"


def heap_traffic(log, freed):
    """How many blocks each of operator new, new[], delete and delete[] handled. A block
    counts once: after a names read that fails the original keeps the pointer to the block it
    has just freed and Close frees it again, which the translation does not do (`freed`: the
    blocks of the case freed so far; the emulator's heap never gives an address twice)."""
    counts = dict.fromkeys(("new", "new[]", "delete", "delete[]"), 0)
    for kind, address, _ in log:
        if not address or (kind.startswith("delete") and address in freed):
            continue
        if kind.startswith("delete"):
            freed.add(address)
        counts[kind] += 1
    return list(counts.values())


@original_code
def tree_ppc(e, case):
    side = PpcSide(e, case)
    answers, marks, skipped, heap, freed = [], [], [], [], set()
    for op, a, text in case["script"]:
        del e.alloc_log[:]
        answer = side.op(op, a, text)
        skipped.append(answer is None)
        answers.append(SKIPPED if answer is None else answer)
        marks.append(len(side.L.log))
        heap.append(heap_traffic(e.alloc_log, freed))
    side.call(TREE_DTOR, side.tree, MINUS)
    case["_skipped"] = skipped
    return dict(answers=answers, marks=marks, log=side.L.log, heap=heap)


def tree_host(lib, case):
    begin = lib["tpw_wt_begin"]
    begin.restype, begin.argtypes = None, [C.c_char_p, C.c_uint32]
    op_fn = lib["tpw_wt_op"]
    op_fn.restype, op_fn.argtypes = None, [C.c_int32, C.c_int32, C.c_char_p, C.c_char_p, C.POINTER(C.c_int32)]
    CL.host_begin(lib, {}, case.get("q"))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    begin(case["file"], len(case["file"]))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    answers, marks, heap = [], [], []
    out = C.create_string_buffer(1024)
    traffic = (C.c_int32 * 4)()
    try:
        for (op, a, text), skip in zip(case["script"], case["_skipped"]):
            traffic[:] = [0, 0, 0, 0]
            if skip:
                answers.append(SKIPPED)
            else:
                op_fn(op, a, text.encode("latin-1"), out, traffic)
                answers.append(out.value.decode("latin-1"))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
            heap.append(list(traffic))
        lib["tpw_wt_end"]()
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, marks=marks, log=log, heap=heap)


FOLDERS = ["", "textures", "Textures\\Big", "a", "a\\b"]
STEMS = ["wood", "Metal", "a", "ab", "abc", "sky_1", "x.y", "qickload", "b_fire"]
EXTENSIONS = [".wct", ".MD2", ".txt", "", ".tga"]
PATTERNS = ["*", "*.*", "*.wct", "*.md2", "a*", "?b*", "textures:*", "textures:*.wct", "textures:big:*", "a:*", "a:b:*",
            "a:b", "textures", "missing:*", "wood.wct", "WOOD.WCT", "textures:wood.wct", "textures/wood.wct",
            "textures\\wood.wct", ":wood.wct", "wood.wct:", "", ":", "a", "ab.*", "a?c.*", "*.", "x.y.*", "sky_?.*",
            "textures:big:metal.md2", "*:*", "a:*:*", "a?c", "?b", "sky_?.tga", "textures:sky_?.tga", "ab?"]


def random_archive(r):
    entries, folder = [], ""
    for _ in range(r.randrange(0, 12)):
        name = r.choice(STEMS) + r.choice(EXTENSIONS)
        stored = name
        if r.random() < 0.4:
            folder = r.choice(FOLDERS)
            stored = folder + "\\" + name if folder else name
            if r.random() < 0.3 and folder:
                stored = stored.replace("\\", "/")
        path = folder + "\\" + name if folder else name
        entries.append((stored, path, r.randrange(1, 5000), r.choice([None, None, r.randrange(1, 4000)]), r.random() < 0.85))
    return archive(r, r.choice([2, 2, 2, 1, 0]), entries), entries


def tree_case(r, fail=False, patterns=None):
    data, entries = random_archive(r)
    paths = [path.lower().replace("\\", ":") for _, path, _, _, _ in entries]
    script = [(0, r.choice([1, 1, 0]) if data[4] != 2 else r.choice([1, 1, 1, 0]), "")]
    for _ in range(r.randrange(4, 14)):
        kind = r.choice([1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 0, 4, 5])
        if kind == 1:
            script.append((1, 0, r.choice(patterns or PATTERNS)))
        elif kind == 3:
            script.append((3, 0, r.choice(paths + PATTERNS[14:22] + ["missing.txt"]) if paths else "missing.txt"))
        elif kind == 0:
            script.append((0, r.randrange(2), ""))
        else:
            script.append((kind, 0, ""))
    q = None
    if fail:
        q = {"Read": [r.choice([-1, 0, 0x58, 1]) if r.random() < 0.3 else AS_ASKED for _ in range(6)] + [AS_ASKED],
             "Seek": [r.choice([-1, 1]) if r.random() < 0.3 else AS_ASKED for _ in range(4)] + [AS_ASKED]}
    return dict(file=data, script=script, q=q)


def tree_edge():
    r = random.Random(141)
    one = lambda name, path=None: (name, path or name, 100, None, True)
    # An empty archive; one file; folders that stick; each version of the directory.
    for version in (0, 1, 2):
        for entries in ([], [one("wood.wct")],
                        [one("textures\\wood.wct"), one("metal.wct", "textures\\metal.wct"), one("a\\b\\x.md2"),
                         one("y.md2", "a\\b\\y.md2"), one("top.txt")]):
            data = archive(r, version, entries)
            for borrow in (0, 1):
                script = [(0, borrow, "")] + [(1, 0, p) for p in ("*", "textures:*", "a:b:*", "wood.wct", "missing")]
                script += [(2, 0, "")] * 3 + [(3, 0, "textures:wood.wct"), (3, 0, "a:b:y.md2"), (3, 0, "top.txt"),
                                               (3, 0, "nothere.txt"), (1, 0, "*"), (2, 0, ""), (2, 0, ""), (2, 0, ""),
                                               (2, 0, ""), (2, 0, ""), (2, 0, ""), (4, 0, ""), (3, 0, "top.txt"), (5, 0, "")]
                yield dict(file=data, script=script)
    # Every pattern on one archive, each followed by all its answers.
    entries = [one("wood.wct"), one("Metal.MD2"), one("ab"), one("abc.txt"), one("textures\\wood.wct"),
               one("sky_1.tga", "textures\\sky_1.tga"), one("Textures\\Big\\metal.md2"), one("a\\x.y.wct"),
               one("a\\b\\qickload.txt"), one("b_fire.MD2", "a\\b\\b_fire.MD2")]
    data = archive(r, 2, entries)
    for pattern in PATTERNS:
        yield dict(file=data, script=[(0, 1, ""), (1, 0, pattern)] + [(2, 0, "")] * 12 + [(3, 0, pattern)])
    # Names with the same hash (the characters after the last full group of four are summed
    # plainly, so their order does not count), at the top and in a folder.
    same = [one(name) for name in ("ab", "ba", "abc", "cab", "bca")]
    same += [one("f\\abcd")] + [one(name, "f\\" + name) for name in ("abdc", "a.bc", "a.cb")]
    assert name_hash("f\\abcd") == name_hash("f\\abdc") and name_hash("cab") == name_hash("bca")
    data2 = archive(r, 2, same)
    for borrow in (0, 1):
        yield dict(file=data2, script=[(0, borrow, "")] + [(3, 0, name) for name in
                                                          ("ba", "ab", "cab", "bca", "abc", "acb", "f:abdc", "f:abcd", "f:a.cb",
                                                           "f:a.bc", "f/abdc", "f\\a.cb", "f:abxx", "F:ABDC")])
    # A directory that is not one, or cut short; reads and seeks that fail.
    yield dict(file=b"XXXX" + data[4:], script=[(0, 1, ""), (1, 0, "*")])
    for cut in (0, 0x30, 0x58, 0x60, len(data) - 5):
        yield dict(file=data[:cut], script=[(0, 1, ""), (1, 0, "*"), (2, 0, ""), (3, 0, "wood.wct")])
    for version in (0, 2):
        data = archive(r, version, entries)
        for step in range(5):
            for value in (-1, 0, 1):
                yield dict(file=data, q={"Read": [AS_ASKED] * step + [value, AS_ASKED]}, script=[(0, 1, ""), (1, 0, "*"), (2, 0, "")])
                yield dict(file=data, q={"Seek": [AS_ASKED] * step + [value, AS_ASKED]}, script=[(0, 1, ""), (1, 0, "*"), (2, 0, "")])
    # More nodes than one block of the node allocators holds (100 of each kind).
    many = [one("d%03d\\f%03d.dat" % (i // 2, i)) for i in range(230)] + [one("top%03d.txt" % i) for i in range(120)]
    data = archive(r, 2, many)
    for borrow in (0, 1):
        script = [(0, borrow, ""), (1, 0, "*")] + [(2, 0, "")] * 4 + [(1, 0, "d1*")] + [(2, 0, "")] * 20
        script += [(3, 0, name) for name in ("d000:f000.dat", "d049:f099.dat", "d050:f100.dat", "d050:f101.dat",
                                             "d114:f229.dat", "top000.txt", "top099.txt", "top100.txt", "top119.txt",
                                             "d114:f228.dat", "d115:f230.dat")]
        script += [(1, 0, "d099:*"), (2, 0, ""), (2, 0, ""), (0, 1 - borrow, ""), (3, 0, "d100:f201.dat"), (4, 0, "")]
        yield dict(file=data, script=script)
    for _ in range(40):
        yield tree_case(r)
    for _ in range(15):
        yield tree_case(r, fail=True)


TESTS = [
    FnTest("TbStringMatch: SetMask and MatchString", mask_ppc, mask_host, edge=mask_edge, rnd=mask_rnd),
    FnTest("WadNameTree and TreeIterator", tree_ppc, tree_host, edge=tree_edge, rnd=lambda r: tree_case(r, fail=r.random() < 0.2)),
]
