#!/usr/bin/env python3
"""The differential tests' slicer: a copy of a source file with only the external definitions one unit library builds.

usage:
  python3 -I devtools/slicer.py index --src FILE --out INDEX.json [--depfile DEP] -- COMPILER FLAGS...
  python3 -I devtools/slicer.py slice --src FILE --index INDEX.json (--keep KEEP.txt | --drop DROP.txt) --out COPY
  python3 -I devtools/slicer.py defs --src FILE -- COMPILER FLAGS...

A file of src/ holds every function of one original source file, but a unit library (tests/unit_<Name>.cpp,
cmake/units/<Name>.cmake) builds only the functions under test for real and fakes their neighbours. CMake does
not compile such a file into the library as it is: it compiles a copy in the build folder, made by this script,
in which every external definition outside the unit's keep list (TPW_UNIT_<Name>_KEEP) becomes a declaration:

  a free function            its body becomes ";" (the declaration stays, with its default arguments)
  a member function          the out-of-line definition goes (the class declares it); an explicit
                             specialization keeps its declaration
  external data              "extern", without the initializer
  a static data member       the out-of-line definition goes
  a static object            the definition goes: an internal variable of namespace scope that the program
                             constructs or destroys at run time (a class that is not plain data, or an initializer
                             that calls a function); a keep list names it like an external definition, so a library
                             runs the constructors and destructors of the static objects it keeps and no others
                             (one that the rest of the copy still names stays, or the copy would not compile)
  an explicit instantiation  the "template ...;" statement goes, so the copy no longer emits what it instantiates;
                             a keep list names it by the instantiated function's name, or for a whole class by the
                             class's type name (__ZTS and the type, "template class X<char>;" is __ZTS1XIcE); one
                             written by a function-like macro goes with each invocation of the macro
  a template definition      a function template's body becomes ";", an out-of-line member of a class template goes
                             (with its template heads): a merged file holds the templates of several old files, and a
                             unit library that sees one an old file of it never had would instantiate it in place of
                             its test's fakes; a keep list names it "tmpl_" and the first 12 hex digits of the SHA-1 of
                             its clang USR (the cmake comment says which)
  a guard macro              the line goes: an object-like "#define NAME" of the file that a header it includes tests
                             (#ifdef, #ifndef, defined) and does not define, such as a TPW_..._STRONG that keeps a
                             header's weak inline defaults out of the file with the real definitions; a keep list names
                             it by NAME, so a copy without it gets the header's defaults back

Everything else stays as it is: the includes, types, macros, internal (static, unnamed namespace) functions and
other data, inline functions, and templates defined inside their class or of internal linkage. The compiler drops the internal functions nothing kept calls, as it always
does. Preprocessor lines inside a removed stretch stay in place and every removed line stays as an empty line,
so the copy keeps the original's line numbers, and a "#line" names the original for the compiler's messages.

"index" finds the definitions with Apple clang's own parser: libclang.dylib of the compiler's toolchain
(found through its -print-resource-dir), the same flags as the compile, so the definitions are those the compiler
sees (the port fix macro on or off). A definition written by a macro (one NAME(...) invocation that makes the whole
definition) goes as a whole invocation, which needs a declaration earlier in the file; the definitions one
invocation makes go together, as do the names of one declaration. A weak definition is stripped of its weak
attribute (or of a macro of the file that spells it) when it becomes a declaration. Inline functions and internal
definitions stay. The index stops, naming the definition, on anything it cannot cut
exactly: a macro definition without an earlier declaration, a name the macro builds, an unusual initializer.
Explicit instantiations are found in the text (outside comments and skipped preprocessor stretches) and named by
a second parse of the file with a probe after each one, a declaration that names what it instantiates.
"slice" applies the index to the file and stops when a keep list keeps only some of a group; "--drop" names the
definitions to leave out instead (every other one is kept), for a library that builds a file but for a few of its
functions. An entry NAME@PATH of either list applies only to the file src/PATH (a name two files define, or a guard
macro several files have). "defs" prints the external definitions of a file, and with "--static" its static objects, guard macros and
template definitions too (the names a keep list holds).
"""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys


# ---- libclang, the parts the slicer uses (clang-c/Index.h)

class CXCursor(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_int), ("xdata", ctypes.c_int), ("data", ctypes.c_void_p * 3)]


class CXSourceLocation(ctypes.Structure):
    _fields_ = [("ptr_data", ctypes.c_void_p * 2), ("int_data", ctypes.c_uint)]


class CXSourceRange(ctypes.Structure):
    _fields_ = [("ptr_data", ctypes.c_void_p * 2), ("begin_int_data", ctypes.c_uint), ("end_int_data", ctypes.c_uint)]


class CXString(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("private_flags", ctypes.c_uint)]


class CXType(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_int), ("data", ctypes.c_void_p * 2)]


class CXStringSet(ctypes.Structure):
    _fields_ = [("Strings", ctypes.POINTER(CXString)), ("Count", ctypes.c_uint)]


class CXSourceRangeList(ctypes.Structure):
    _fields_ = [("count", ctypes.c_uint), ("ranges", ctypes.POINTER(CXSourceRange))]


class CXUnsavedFile(ctypes.Structure):
    _fields_ = [("Filename", ctypes.c_char_p), ("Contents", ctypes.c_char_p), ("Length", ctypes.c_ulong)]


VISITOR = ctypes.CFUNCTYPE(ctypes.c_int, CXCursor, CXCursor, ctypes.py_object)
INCLUSION_VISITOR = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(CXSourceLocation), ctypes.c_uint,
                                     ctypes.py_object)

# CXCursorKind
K_STRUCT, K_UNION, K_CLASS = 2, 3, 4
K_FUNCTION, K_VAR = 8, 9
K_METHOD, K_NAMESPACE, K_LINKAGE_SPEC, K_CTOR, K_DTOR, K_CONVERSION = 21, 22, 23, 24, 25, 26
K_FUNCTION_TEMPLATE, K_CLASS_TEMPLATE, K_PARTIAL_SPEC = 30, 31, 32
K_CALL_EXPR, K_NEW_EXPR = 103, 134
K_COMPOUND_STMT, K_TRY_STMT = 202, 224
K_TRANSLATION_UNIT = 300
FUNCTION_KINDS = {K_FUNCTION, K_METHOD, K_CTOR, K_DTOR, K_CONVERSION}
RECORD_KINDS = {K_STRUCT, K_UNION, K_CLASS}
TEMPLATE_KINDS = {K_CLASS_TEMPLATE, K_PARTIAL_SPEC}
LINKAGE_EXTERNAL = 4
CXTYPE_RECORD, CXTYPE_CONSTANT_ARRAY = 105, 112
SC_EXTERN = 2
DIAG_ERROR = 3
IDENT = re.compile(r"[A-Za-z_]\w*")


def load_libclang(compiler):
    """The libclang of the compiler's own toolchain, and the resource directory to parse with."""
    res = subprocess.run([compiler, "-print-resource-dir"], capture_output=True, text=True, check=True).stdout.strip()
    lib = pathlib.Path(res).parent.parent / ("libclang.dylib" if sys.platform == "darwin" else "libclang.so")
    if not lib.exists():
        found = ctypes.util.find_library("clang")
        if not found:
            sys.exit(f"slicer: no libclang next to {compiler} ({lib})")
        lib = found
    cl = ctypes.CDLL(str(lib))
    sig = {
        "clang_createIndex": (ctypes.c_void_p, [ctypes.c_int, ctypes.c_int]),
        "clang_parseTranslationUnit2": (ctypes.c_int, [ctypes.c_void_p, ctypes.c_char_p,
                                                       ctypes.POINTER(ctypes.c_char_p), ctypes.c_int, ctypes.c_void_p,
                                                       ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]),
        "clang_getTranslationUnitCursor": (CXCursor, [ctypes.c_void_p]),
        "clang_visitChildren": (ctypes.c_uint, [CXCursor, VISITOR, ctypes.py_object]),
        "clang_getCursorExtent": (CXSourceRange, [CXCursor]),
        "clang_getCursorLocation": (CXSourceLocation, [CXCursor]),
        "clang_getRangeStart": (CXSourceLocation, [CXSourceRange]),
        "clang_getRangeEnd": (CXSourceLocation, [CXSourceRange]),
        "clang_getExpansionLocation": (None, [CXSourceLocation, ctypes.POINTER(ctypes.c_void_p),
                                              ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
                                              ctypes.POINTER(ctypes.c_uint)]),
        "clang_getSpellingLocation": (None, [CXSourceLocation, ctypes.POINTER(ctypes.c_void_p),
                                             ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint),
                                             ctypes.POINTER(ctypes.c_uint)]),
        "clang_getFile": (ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_char_p]),
        "clang_File_isEqual": (ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p]),
        "clang_getCursorLinkage": (ctypes.c_int, [CXCursor]),
        "clang_isCursorDefinition": (ctypes.c_uint, [CXCursor]),
        "clang_Cursor_getMangling": (CXString, [CXCursor]),
        "clang_Cursor_getCXXManglings": (ctypes.POINTER(CXStringSet), [CXCursor]),
        "clang_disposeStringSet": (None, [ctypes.POINTER(CXStringSet)]),
        "clang_Cursor_isFunctionInlined": (ctypes.c_uint, [CXCursor]),
        "clang_Cursor_getStorageClass": (ctypes.c_int, [CXCursor]),
        "clang_getCursorSemanticParent": (CXCursor, [CXCursor]),
        "clang_getSpecializedCursorTemplate": (CXCursor, [CXCursor]),
        "clang_Cursor_isNull": (ctypes.c_int, [CXCursor]),
        "clang_getCanonicalCursor": (CXCursor, [CXCursor]),
        "clang_getCursorType": (CXType, [CXCursor]),
        "clang_getArraySize": (ctypes.c_longlong, [CXType]),
        "clang_getCanonicalType": (CXType, [CXType]),
        "clang_getArrayElementType": (CXType, [CXType]),
        "clang_isPODType": (ctypes.c_uint, [CXType]),
        "clang_equalCursors": (ctypes.c_uint, [CXCursor, CXCursor]),
        "clang_getCursorSpelling": (CXString, [CXCursor]),
        "clang_getCursorUSR": (CXString, [CXCursor]),
        "clang_getCString": (ctypes.c_char_p, [CXString]),
        "clang_disposeString": (None, [CXString]),
        "clang_getNumDiagnostics": (ctypes.c_uint, [ctypes.c_void_p]),
        "clang_getDiagnostic": (ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_uint]),
        "clang_getDiagnosticSeverity": (ctypes.c_int, [ctypes.c_void_p]),
        "clang_formatDiagnostic": (CXString, [ctypes.c_void_p, ctypes.c_uint]),
        "clang_disposeDiagnostic": (None, [ctypes.c_void_p]),
        "clang_getInclusions": (None, [ctypes.c_void_p, INCLUSION_VISITOR, ctypes.py_object]),
        "clang_getFileName": (CXString, [ctypes.c_void_p]),
        "clang_getCursorReferenced": (CXCursor, [CXCursor]),
        "clang_getTypeSpelling": (CXString, [CXType]),
        "clang_getDiagnosticLocation": (CXSourceLocation, [ctypes.c_void_p]),
        "clang_getAllSkippedRanges": (ctypes.POINTER(CXSourceRangeList), [ctypes.c_void_p]),
        "clang_disposeSourceRangeList": (None, [ctypes.POINTER(CXSourceRangeList)]),
        "clang_disposeTranslationUnit": (None, [ctypes.c_void_p]),
    }
    for name, (res_t, args) in sig.items():
        f = getattr(cl, name)
        f.restype, f.argtypes = res_t, args
    return cl, res


class Parser:
    def __init__(self, compiler):
        self.cl, self.resource_dir = load_libclang(compiler)
        self.index = self.cl.clang_createIndex(0, 0)

    def text(self, cxstring):
        s = self.cl.clang_getCString(cxstring)
        self.cl.clang_disposeString(cxstring)
        return s.decode() if s else ""

    def children(self, cursor):
        out = []

        def visit(child, parent, data):
            data.append(CXCursor.from_buffer_copy(child))
            return 1                    # CXChildVisit_Continue

        self.cl.clang_visitChildren(cursor, VISITOR(visit), out)
        return out

    def file_of(self, loc):
        """The file a location is in after macro expansion (a macro's output belongs to its invocation's file)."""
        f = ctypes.c_void_p()
        self.cl.clang_getExpansionLocation(loc, ctypes.byref(f), None, None, None)
        return f

    def offset(self, loc, spelling=False):
        f, line, col, off = ctypes.c_void_p(), ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
        get = self.cl.clang_getSpellingLocation if spelling else self.cl.clang_getExpansionLocation
        get(loc, ctypes.byref(f), ctypes.byref(line), ctypes.byref(col), ctypes.byref(off))
        return off.value, line.value

    def extent(self, cursor):
        r = self.cl.clang_getCursorExtent(cursor)
        b, e = self.cl.clang_getRangeStart(r), self.cl.clang_getRangeEnd(r)
        return self.offset(b), self.offset(e), self.offset(b, True), self.offset(e, True)

    def parse(self, src, flags, contents=None):
        """The translation unit of src; with contents (the text to parse in place of the file's), the lines of its
        errors too, which do not stop it."""
        extra = ["-x", "c++", "-resource-dir", self.resource_dir]
        if sys.platform == "darwin" and "-isysroot" not in flags:
            # The compiler driver finds the SDK by itself (xcrun); libclang does not.
            sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()
            if sdk:
                extra += ["-isysroot", sdk]
        if contents is not None:
            extra.append("-ferror-limit=0")
        args = [a.encode() for a in extra + flags]
        argv = (ctypes.c_char_p * len(args))(*args)
        tu = ctypes.c_void_p()
        unsaved, n = None, 0
        if contents is not None:
            data = contents.encode("utf-8", errors="surrogateescape")
            unsaved, n = CXUnsavedFile(str(src).encode(), data, len(data)), 1
        err = self.cl.clang_parseTranslationUnit2(self.index, str(src).encode(), argv, len(args),
                                                  ctypes.byref(unsaved) if unsaved else None, n, 0, ctypes.byref(tu))
        if err:
            sys.exit(f"slicer: libclang could not parse {src} (error {err})")
        errors, lines = [], set()
        main = self.cl.clang_getFile(tu, str(src).encode())
        for i in range(self.cl.clang_getNumDiagnostics(tu)):
            d = self.cl.clang_getDiagnostic(tu, i)
            if self.cl.clang_getDiagnosticSeverity(d) >= DIAG_ERROR:
                errors.append(self.text(self.cl.clang_formatDiagnostic(d, 0x1 | 0x2)))   # source location, column
                loc = self.cl.clang_getDiagnosticLocation(d)
                if self.cl.clang_File_isEqual(self.file_of(loc), main):
                    lines.add(self.offset(loc)[1])
            self.cl.clang_disposeDiagnostic(d)
        if contents is not None:
            return tu, lines
        if errors:
            sys.exit("slicer: " + str(src) + " does not compile:\n  " + "\n  ".join(errors[:20]))
        return tu

    def includes(self, tu):
        files = []

        def visit(f, stack, n, data):
            data.append(self.text(self.cl.clang_getFileName(f)))

        self.cl.clang_getInclusions(tu, INCLUSION_VISITOR(visit), files)
        return files


# ---- the definitions of a file

class SliceError(Exception):
    pass


def skip_space(text, i):
    """The first offset at or after i that is not white space or a comment."""
    n = len(text)
    while i < n:
        if text[i].isspace():
            i += 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            break
    return i


def skip_balanced(text, i):
    """Past the bracket at i and its partner (string and character literals skipped)."""
    pairs = {"(": ")", "[": "]", "{": "}"}
    stack = [pairs[text[i]]]
    i += 1
    while stack:
        if i >= len(text):
            raise SliceError("unbalanced brackets")
        c = text[i]
        if c in "\"'":
            i = skip_literal(text, i)
            continue
        if text.startswith("//", i) or text.startswith("/*", i):
            i = skip_space(text, i)
            continue
        if c in pairs:
            stack.append(pairs[c])
        elif c in ")]}":
            if c != stack.pop():
                raise SliceError("mismatched brackets")
        i += 1
    return i


def skip_literal(text, i):
    q = text[i]
    i += 1
    while i < len(text) and text[i] != q:
        if text[i] == "\\":
            i += 1
        elif text[i] == "\n":
            return i                    # an unterminated quote (in a skipped #if branch) ends with its line
        i += 1
    return i + 1


def describe(text, off):
    line = text.count("\n", 0, off) + 1
    return f"line {line}"


def index_file(parser, src, flags):
    """The external definitions of src: each with its name and the edits that turn it into a declaration."""
    text = pathlib.Path(src).read_text(encoding="utf-8", errors="surrogateescape")
    data = text.encode("utf-8", errors="surrogateescape")
    if len(data) != len(text):
        raise SliceError("non-ASCII text: offsets would not match")   # libclang counts bytes
    tu = parser.parse(src, flags)
    cl = parser.cl
    items, problems = [], []

    main = cl.clang_getFile(tu, str(src).encode())

    def top_level(cursor):
        for c in parser.children(cursor):
            if not cl.clang_File_isEqual(parser.file_of(cl.clang_getCursorLocation(c)), main):
                continue
            if c.kind in (K_NAMESPACE, K_LINKAGE_SPEC):
                if c.kind == K_NAMESPACE and not parser.text(cl.clang_getCursorSpelling(c)):
                    continue            # the unnamed namespace: internal throughout
                yield from top_level(c)
            else:
                yield c

    weak_macros = find_weak_macros(text)
    invocations = {}
    for c in top_level(cl.clang_getTranslationUnitCursor(tu)):
        tmpl = template_definition(parser, cl, c, text, problems)
        if tmpl:
            items.append(tmpl)
            continue
        if c.kind not in FUNCTION_KINDS and c.kind != K_VAR:
            continue
        if not cl.clang_isCursorDefinition(c) or cl.clang_getCursorLinkage(c) != LINKAGE_EXTERNAL:
            continue
        parent = cl.clang_getCursorSemanticParent(c)
        if in_template(cl, parent):
            continue                    # a member of a class template (or of a class inside one): emitted only where used
        if c.kind in FUNCTION_KINDS and cl.clang_Cursor_isFunctionInlined(c):
            continue
        name = parser.text(cl.clang_Cursor_getMangling(c))
        spelled = parser.text(cl.clang_getCursorSpelling(c))
        (b, bl), (e, el), (sb, _), (se, _) = parser.extent(c)
        where = f"{name} ({spelled}, {describe(text, b)})"
        try:
            if not name:
                raise SliceError("clang gives it no symbol name")
            if (b, e) != (sb, se):
                raise SliceError("written by a macro")
            loc, _ = parser.offset(cl.clang_getCursorLocation(c))
            sloc, _ = parser.offset(cl.clang_getCursorLocation(c), True)
            variants = []
            if c.kind in (K_CTOR, K_DTOR):
                ss = cl.clang_Cursor_getCXXManglings(c)
                variants = [cl.clang_getCString(ss.contents.Strings[i]).decode() for i in range(ss.contents.Count)]
                cl.clang_disposeStringSet(ss)
            specialization = not cl.clang_Cursor_isNull(cl.clang_getSpecializedCursorTemplate(c))
            member = parent.kind in RECORD_KINDS
            head_edits = []
            if c.kind in FUNCTION_KINDS:
                body = [k for k in parser.children(c) if k.kind in (K_COMPOUND_STMT, K_TRY_STMT)]
                bext = parser.extent(body[-1]) if body else None
                if bext and (bext[0][0], bext[1][0]) == (b, e) and macro_invocation(text, b, e):
                    # A function-like macro writes the whole definition: the invocation goes, which is exact when
                    # the function is declared before it (by its class or a header). The functions one invocation
                    # defines go together (a keep list keeps all of them or none).
                    if cl.clang_equalCursors(cl.clang_getCanonicalCursor(c), c):
                        raise SliceError("a macro defines it and nothing declares it before")
                    edits = [] if (b, e) in invocations else [(b, end_with_semicolon(text, e), "")]
                    invocations.setdefault((b, e), []).append(name)
                    kind = "delete"
                elif loc != sloc:
                    raise SliceError("its name comes from a macro")
                elif (member or c.kind != K_FUNCTION) and not specialization:
                    edits = [(b, end_with_semicolon(text, e), "")]
                    kind = "delete"
                    head_edits = weak_edits(text, b, bext[0][0] if bext else e, weak_macros)   # only marks it weak
                elif body:
                    (bb, _), (be, _), (sbb, _), (sbe, _) = bext
                    if (bb, be) != (sbb, sbe):
                        raise SliceError("its body comes from a macro")
                    if not (text.startswith("{", bb) or text.startswith("try", bb)) or text[be - 1] != "}":
                        raise SliceError(f"unexpected body text {text[bb:bb + 10]!r}")
                    head_edits = weak_edits(text, b, bb, weak_macros)
                    start = ctor_initializer(text, loc, spelled, bb) if c.kind == K_CTOR else bb
                    edits = head_edits + [(start, be, ";")]
                    kind = "body"
                else:
                    raise SliceError("a free function defined without a body (= default or = delete)")
            else:
                if member:
                    edits = [(b, end_with_semicolon(text, e), "")]
                    kind = "delete"
                    head_edits = weak_edits(text, b, loc, weak_macros)   # only marks it weak
                else:
                    size = cl.clang_getArraySize(cl.clang_getCursorType(c))
                    extern_at, edits = extern_edits(text, b, e, loc, spelled, size)
                    head_edits = weak_edits(text, b, loc, weak_macros)
                    kind = "extern"
            for x, y, _ in edits:
                check_directives(text, x, y)
            item = {"name": name, "spelled": spelled, "line": bl, "kind": kind, "edits": edits}
            if variants:
                item["variants"] = variants
            if head_edits:
                item["weak"] = True
            if (b, e) in invocations:
                item["group"] = b
            if kind == "extern":
                # The names of one declaration (int a = 1, b;) share its start, its "extern" and its attributes.
                item["decl"], item["extern_at"], item["decl_edits"] = b, extern_at, head_edits
            items.append(item)
        except SliceError as err:
            problems.append(f"{where}: {err}")
    # libclang also shows a static data member that the file instantiates from a class template's definition, under
    # the instantiation's name and inside the definition's text: the template's item owns that text.
    spans = [ed[0] for it in items if it["kind"] == "template" for ed in [it["edits"]]]
    items = [it for it in items if it["kind"] == "template"
             or not any(x <= ex and ey <= y for ex, ey, _ in it["edits"] for x, y, _ in spans)]
    items.extend(static_objects(parser, cl, tu, main, text, problems))
    items.extend(explicit_instantiations(parser, cl, src, flags, text, tu, main, problems))
    items.extend(guard_defines(parser, cl, src, text, tu, main))
    return text, tu, items, problems


def template_definition(parser, cl, c, text, problems):
    """The item of a template definition of external linkage (a function template, or an out-of-line member of a class
    template), or None for anything else. Its name is "tmpl_" and the start of the SHA-1 of its clang USR (a USR holds
    "#" and "@"). A member goes as a whole, with every template head before it (a member template of a class template
    has two); a free function template keeps its declaration."""
    if c.kind == K_FUNCTION_TEMPLATE:
        pass
    elif c.kind not in FUNCTION_KINDS and c.kind != K_VAR:
        return None
    elif not in_template(cl, cl.clang_getCursorSemanticParent(c)):
        return None
    if not cl.clang_isCursorDefinition(c) or cl.clang_getCursorLinkage(c) != LINKAGE_EXTERNAL:
        return None
    usr = parser.text(cl.clang_getCursorUSR(c))
    spelled = parser.text(cl.clang_getCursorSpelling(c))
    (b, bl), (e, el), (sb, _), (se, _) = parser.extent(c)
    try:
        if not usr:
            raise SliceError("clang gives it no USR")
        if (b, e) != (sb, se):
            raise SliceError("written by a macro")
        parent = cl.clang_getCursorSemanticParent(c)
        if parent.kind in RECORD_KINDS or parent.kind in TEMPLATE_KINDS:
            edits = [(template_head_start(text, b), end_with_semicolon(text, e), "")]
        else:
            body = [k for k in parser.children(c) if k.kind in (K_COMPOUND_STMT, K_TRY_STMT)]
            if not body:
                raise SliceError("a function template defined without a body")
            (bb, _), (be, _), (sbb, _), (sbe, _) = parser.extent(body[-1])
            if (bb, be) != (sbb, sbe):
                raise SliceError("its body comes from a macro")
            if not (text.startswith("{", bb) or text.startswith("try", bb)) or text[be - 1] != "}":
                raise SliceError(f"unexpected body text {text[bb:bb + 10]!r}")
            edits = [(bb, be, ";")]
        for x, y, _ in edits:
            check_directives(text, x, y)
    except SliceError as err:
        problems.append(f"template {spelled} ({describe(text, b)}): {err}")
        return None
    return {"name": "tmpl_" + sha1(usr)[:12], "spelled": spelled, "line": bl, "kind": "template", "usr": usr,
            "edits": edits}


def template_head_start(text, b):
    """b, or the start of the template heads ("template <...>") right before it."""
    while True:
        i = b
        while i > 0 and text[i - 1].isspace():
            i -= 1
        if i == 0 or text[i - 1] != ">":
            return b
        depth, j = 0, i - 1
        while j >= 0:
            if text[j] == ">":
                depth += 1
            elif text[j] == "<":
                depth -= 1
                if depth == 0:
                    break
            j -= 1
        if j < 0:
            return b
        k = j
        while k > 0 and text[k - 1].isspace():
            k -= 1
        if not text.endswith("template", 0, k) or (k > 8 and (text[k - 9].isalnum() or text[k - 9] == "_")):
            return b
        b = k - 8


HEADER_MACROS = {}


def header_macros(path):
    """The macro names a header tests (#ifdef, #ifndef, defined) and those it defines."""
    if path not in HEADER_MACROS:
        code = CODE_NOISE.sub(blank, pathlib.Path(path).read_text(encoding="utf-8", errors="surrogateescape"))
        tested = set(re.findall(r"^[ \t]*#[ \t]*if(?:n?def)?\b[^\n]*?\b(?:defined\s*\(?\s*)?(\w+)", code, re.M))
        tested |= set(re.findall(r"\bdefined\s*\(?\s*(\w+)", code))
        defined = set(re.findall(r"^[ \t]*#[ \t]*define[ \t]+(\w+)", code, re.M))
        HEADER_MACROS[path] = (tested, defined)
    return HEADER_MACROS[path]


def guard_defines(parser, cl, src, text, tu, main):
    """The file's guard macros: an object-like #define of the file (outside skipped preprocessor stretches) whose name
    a header tests and does not define itself, such as a TPW_..._STRONG that takes a header's weak inline defaults out
    of the file that defines the functions for real. A keep list names one like a definition (a copy that does not
    keep it loses the line), so a copy that keeps none of the real definitions gets the header's defaults back."""
    code = CODE_NOISE.sub(blank, text)
    skipped = []
    ranges = cl.clang_getAllSkippedRanges(tu)
    if ranges:
        for i in range(ranges.contents.count):
            r = ranges.contents.ranges[i]
            b, e = cl.clang_getRangeStart(r), cl.clang_getRangeEnd(r)
            if cl.clang_File_isEqual(parser.file_of(b), main):
                skipped.append((parser.offset(b)[0], parser.offset(e)[0]))
        cl.clang_disposeSourceRangeList(ranges)
    tested, defined = set(), set()
    for h in parser.includes(tu):
        if pathlib.Path(h).resolve() != pathlib.Path(src).resolve():
            t, d = header_macros(h)
            tested |= t
            defined |= d
    items = []
    for m in re.finditer(r"^[ \t]*(#[ \t]*define[ \t]+(\w+))(?![\w(])[^\n]*", code, re.M):
        name = m.group(2)
        if name not in tested or name in defined or m.group(0).endswith("\\"):
            continue
        if any(b <= m.start(1) < e for b, e in skipped):
            continue
        items.append({"kind": "guard", "name": name, "edits": [(m.start(1), m.end(), "")]})
    return items


def static_objects(parser, cl, tu, main, text, problems):
    """The internal variables of namespace scope that the program constructs or destroys at run time: of a class type
    that is not plain data, or with an initializer that calls a function or news (constexpr ones are constants). They are definitions of the file
    too (each one's constructor and destructor run in whatever library builds the copy), so a keep list keeps them by
    name like the external ones; the names of one declaration go together."""
    found = []

    def walk(cursor):
        for c in parser.children(cursor):
            if not cl.clang_File_isEqual(parser.file_of(cl.clang_getCursorLocation(c)), main):
                continue
            if c.kind in (K_NAMESPACE, K_LINKAGE_SPEC):
                walk(c)
            elif (c.kind == K_VAR and cl.clang_isCursorDefinition(c)
                  and cl.clang_getCursorLinkage(c) != LINKAGE_EXTERNAL and run_time(c)):
                found.append(c)

    def run_time(c):
        (b, _), _, _, _ = parser.extent(c)
        loc, _ = parser.offset(cl.clang_getCursorLocation(c))
        if re.search(r"\b(constexpr|constinit)\b", text[b:loc]):
            return False
        t = cl.clang_getCanonicalType(cl.clang_getCursorType(c))
        while t.kind == CXTYPE_CONSTANT_ARRAY:
            t = cl.clang_getCanonicalType(cl.clang_getArrayElementType(t))
        if t.kind == CXTYPE_RECORD and not cl.clang_isPODType(t):
            return True
        stack = parser.children(c)
        while stack:
            k = stack.pop()
            if k.kind in (K_CALL_EXPR, K_NEW_EXPR):
                return True
            stack.extend(parser.children(k))
        return False

    walk(cl.clang_getTranslationUnitCursor(tu))
    items, decls = [], {}
    for c in found:
        name = parser.text(cl.clang_Cursor_getMangling(c))
        spelled = parser.text(cl.clang_getCursorSpelling(c))
        (b, bl), (e, _), (sb, _), (se, _) = parser.extent(c)
        try:
            if not name:
                raise SliceError("clang gives it no symbol name")
            if (b, e) != (sb, se):
                raise SliceError("written by a macro")
            decls.setdefault(b, []).append({"name": name, "spelled": spelled, "line": bl, "kind": "static",
                                            "edits": [], "group": b, "end": e})
        except SliceError as err:
            problems.append(f"{name} ({spelled}, {describe(text, b)}): {err}")
    for b, names in decls.items():
        try:
            j = skip_space(text, names[-1]["end"])
            for it in names:
                del it["end"]
            if j >= len(text) or text[j] != ";":
                raise SliceError(f"the declaration does not end at {describe(text, j)}")
            check_directives(text, b, j + 1)
            names[0]["edits"] = [(b, j + 1, "")]
            items.extend(names)
        except SliceError as err:
            problems.append(f"{names[0]['name']} ({names[0]['spelled']}, {describe(text, b)}): {err}")
    return items


def blank(m):
    return "".join(ch if ch == "\n" else " " for ch in m.group(0))


def explicit_instantiations(parser, cl, src, flags, text, tu, main, problems):
    """The explicit instantiation definitions of the file (template void f<int>(int); template class X<int>;, or one
    invocation of a macro of the file that writes some): each makes external definitions of its own, so a keep list
    keeps it by name like a definition, and a copy that does not keep it loses the statement. clang does not show
    them in its tree (it shows what they instantiate, not the statement), so the names come from a second parse with
    a probe after each statement that names what the statement instantiates: the function, or the constructor or
    destructor; a class's whole instantiation is named __ZTS<the class's mangled type> (the typeinfo name)."""
    code = CODE_NOISE.sub(blank, text)
    skipped = []
    ranges = cl.clang_getAllSkippedRanges(tu)
    if ranges:
        for i in range(ranges.contents.count):
            r = ranges.contents.ranges[i]
            b, e = cl.clang_getRangeStart(r), cl.clang_getRangeEnd(r)
            if cl.clang_File_isEqual(parser.file_of(b), main):
                skipped.append((parser.offset(b)[0], parser.offset(e)[0]))
        cl.clang_disposeSourceRangeList(ranges)
    directives = [(m.start(), m.end()) for m in re.finditer(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", code, re.M)]
    bodies, scopes = [], {}

    def walk(cursor, path):
        for c in parser.children(cursor):
            if not cl.clang_File_isEqual(parser.file_of(cl.clang_getCursorLocation(c)), main):
                continue
            (b, _), (e, _), _, _ = parser.extent(c)
            if c.kind in (K_NAMESPACE, K_LINKAGE_SPEC):
                name = parser.text(cl.clang_getCursorSpelling(c)) if c.kind == K_NAMESPACE else ""
                scopes[(b, e)] = path + ([name] if name else [])
                walk(c, scopes[(b, e)])
            else:
                bodies.append((b, e))

    walk(cl.clang_getTranslationUnitCursor(tu), [])

    def inside(p, spans, strict=False):
        return any((b < p if strict else b <= p) and p < e for b, e in spans)

    def namespace_of(p):
        best = max(((b, e) for b, e in scopes if b < p < e), key=lambda s: s[0], default=None)
        return scopes[best] if best else []

    def statement_end(i):
        while i < len(code):
            if code[i] == ";":
                return i + 1
            i = skip_balanced(code, i) if code[i] in "([{" else i + 1
        raise SliceError("no ';' ends it")

    def is_instantiation(i):
        if re.match(r"template\s*<", code[i:]):
            return False
        j = i
        while j > 0 and code[j - 1].isspace():
            j -= 1
        return not re.search(r"(\bextern|\.|->|::)$", code[max(0, j - 8):j])

    found = []                          # (start, end, statements, what)
    macros = {}
    for m in re.finditer(r"\btemplate\b", code):
        i = m.start()
        if inside(i, skipped) or not is_instantiation(i):
            continue
        d = [s for s in directives if s[0] <= i < s[1]]
        if d:
            dm = re.match(r"[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)\(([^)]*)\)", code[d[0][0]:d[0][1]])
            if dm:
                macros[dm.group(1)] = (d[0], [a.strip() for a in dm.group(2).split(",")], dm.end() + d[0][0])
            continue
        if inside(i, bodies, strict=True):
            continue
        e = statement_end(i)
        found.append((i, e, [code[i:e]], None))
    for name, ((db, de), params, bb) in macros.items():
        undef = re.compile(r"^[ \t]*#[ \t]*undef[ \t]+%s\b" % name, re.M).search(code, de)
        stop = undef.start() if undef else len(code)
        body = re.sub(r"\\\n", " ", code[bb:de])
        for m in re.finditer(r"\b%s\s*\(" % name, code[:stop]):
            i = m.start()
            if i < de or inside(i, skipped) or inside(i, directives) or inside(i, bodies, strict=True):
                continue
            e = skip_balanced(code, m.end() - 1)
            args = [a.strip() for a in split_commas(code[m.end():e - 1])]
            if len(args) != len(params):
                raise SliceError(f"{name} at {describe(text, i)}: {len(args)} arguments for {len(params)} parameters")
            expanded = body
            for p, a in zip(params, args):
                expanded = re.sub(r"\b%s\b" % re.escape(p), a, expanded)
            stmts = [s.strip() + ";" for s in split_semicolons(expanded) if s.strip()]
            if not all(re.match(r"template\b(?!\s*<)", s) for s in stmts):
                problems.append(f"{name} ({describe(text, i)}): a macro that writes explicit instantiations and more")
                continue
            j = skip_space(text, e)
            found.append((i, j + 1 if text.startswith(";", j) else e, stmts, name))
    if not found:
        return []

    # The probe parse: each statement's probes go on lines of their own after it, in its namespace; a class's go at
    # the end, at global scope, through decltype of a pointer declared next to the statement.
    pieces, pos, line = [], 0, 1
    probes = []                         # (item index, statement index, kind, probe name, line)
    tail = []

    def emit(s):
        nonlocal line
        pieces.append(s)
        line += s.count("\n")

    for n, (b, e, stmts, _) in enumerate(sorted(found)):
        emit(text[pos:e])
        pos = e
        for k, s in enumerate(stmts):
            for kind, decl in probe_decls(" ".join(s.split())[len("template"):].strip().rstrip(";").strip(),
                                          f"__slicer_probe_{n}_{k}"):
                if kind == "class":
                    qual = "".join(p + "::" for p in namespace_of(b))
                    tail.append((n, k, f"void __slicer_probe_{n}_{k}_c(decltype({qual}__slicer_probe_{n}_{k}_p));"))
                    kind = "pointer"
                emit("\n" + decl)
                probes.append((n, k, kind, decl.split("__slicer_probe_", 1)[1].split("(")[0].split(" ")[0], line))
    emit(text[pos:])
    for n, k, decl in tail:
        emit("\n" + decl)
        probes.append((n, k, "class", f"{n}_{k}_c", line))
    ptu, error_lines = parser.parse(src, flags, "".join(pieces))
    pmain = cl.clang_getFile(ptu, str(src).encode())
    decls = {}

    def collect(cursor):
        for c in parser.children(cursor):
            if c.kind in (K_NAMESPACE, K_LINKAGE_SPEC):
                collect(c)
            elif cl.clang_File_isEqual(parser.file_of(cl.clang_getCursorLocation(c)), pmain):
                spelled = parser.text(cl.clang_getCursorSpelling(c))
                if spelled.startswith("__slicer_probe_"):
                    decls[spelled[len("__slicer_probe_"):]] = c

    collect(cl.clang_getTranslationUnitCursor(ptu))
    want = {"function": FUNCTION_KINDS, "member": FUNCTION_KINDS, "ctor": {K_CTOR}, "dtor": {K_DTOR}}

    def referenced(c, kinds):
        stack = list(reversed(parser.children(c)))
        while stack:
            k = stack.pop()
            r = cl.clang_getCursorReferenced(k)
            if not cl.clang_Cursor_isNull(r) and r.kind in kinds:
                return r
            stack.extend(reversed(parser.children(k)))
        return None

    names = {}                          # (item, statement) -> set of (name, variants)
    for n, k, kind, pname, pline in probes:
        if kind == "pointer" or pline in error_lines or pname not in decls:
            continue
        c = decls[pname]
        if kind == "class":
            mangled = parser.text(cl.clang_Cursor_getMangling(c))
            head = f"__Z{len('__slicer_probe_' + pname)}__slicer_probe_{pname}P"
            if mangled.startswith(head):
                names.setdefault((n, k), set()).add(("__ZTS" + mangled[len(head):], ()))
            continue
        r = referenced(c, want[kind])
        if r is None:
            continue
        variants = ()
        if r.kind in (K_CTOR, K_DTOR):
            ss = cl.clang_Cursor_getCXXManglings(r)
            variants = tuple(cl.clang_getCString(ss.contents.Strings[i]).decode() for i in range(ss.contents.Count))
            cl.clang_disposeStringSet(ss)
        names.setdefault((n, k), set()).add((parser.text(cl.clang_Cursor_getMangling(r)), variants))
    cl.clang_disposeTranslationUnit(ptu)

    items = []
    for n, (b, e, stmts, macro) in enumerate(sorted(found)):
        group = []
        bl = text.count("\n", 0, b) + 1
        for k, s in enumerate(stmts):
            got = names.get((n, k), set())
            where = f"{' '.join(s.split())} ({describe(text, b)}{', from ' + macro if macro else ''})"
            if len(got) != 1 or not next(iter(got))[0]:
                problems.append(f"{where}: an explicit instantiation the probes do not name"
                                + (f" ({len(got)} names)" if got else ""))
                continue
            name, variants = next(iter(got))
            item = {"name": name, "spelled": " ".join(s.split()), "line": bl, "kind": "instantiation", "edits": []}
            if variants:
                item["variants"] = list(variants)
            group.append(item)
        if not group:
            continue
        check_directives(text, b, e)
        group[0]["edits"] = [(b, e, "")]
        if len(group) > 1:
            for it in group:
                it["group"] = b
        items.extend(group)
    return items


def split_semicolons(s):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == ";" and depth == 0:
            parts.append(cur)
            cur = ""
            continue
        depth += ch in "([{"
        depth -= ch in ")]}"
        cur += ch
    parts.append(cur)
    return parts


def probe_decls(s, probe):
    """The probes for one explicit instantiation (s: the statement without "template" and ";"), as (kind, text): a
    declaration of a static variable or function named probe..., whose initializer or body refers to what s
    instantiates."""
    m = re.match(r"(class|struct|union)\s+(.+)$", s)
    if m:
        return [("class", f"static {m.group(2)}* {probe}_p = nullptr;")]
    depth, i = 0, 0
    while i < len(s):
        if s.startswith("operator", i) and not (s[i + 8:i + 9].isalnum() or s[i + 8:i + 9] == "_"):
            j = i + 8
            while j < len(s) and s[j] in " <>=+-*/%&|^!~[]," and not s.startswith("()", j):
                j += 1
            i = j if j > i + 8 else i + 8
            continue
        if s[i] == "<":
            depth += 1
        elif s[i] == ">":
            depth -= 1
        elif s[i] == "(" and depth == 0:
            break
        i += 1
    if i >= len(s):
        return []
    j = skip_balanced(s, i)
    head, params, after = s[:i].strip(), s[i + 1:j - 1].strip(), s[j:].strip()
    depth, cut_at = 0, -1
    for x, ch in enumerate(head):
        depth += ch == "<"
        depth -= ch == ">"
        if ch == " " and depth == 0 and not head[x + 1:].startswith(("::", "<", ">")) and not head[:x].endswith("::"):
            cut_at = x
    ret, name = (head[:cut_at].strip(), head[cut_at + 1:].strip()) if cut_at >= 0 else ("", head)
    parts, depth, cur, x = [], 0, "", 0
    while x < len(name):
        if depth == 0 and name.startswith("::", x):
            parts.append(cur)
            cur, x = "", x + 2
            continue
        depth += name[x] == "<"
        depth -= name[x] == ">"
        cur += name[x]
        x += 1
    parts.append(cur)
    qual, last = "::".join(parts[:-1]), parts[-1]
    if qual and "<" in parts[-2]:
        # In the parameters, the class template's own name without arguments is its injected name: the probes,
        # outside the class, spell the class in full.
        bare = parts[-2][:parts[-2].index("<")].strip()
        params = re.sub(r"\b%s\b(?!\s*<)" % re.escape(bare), lambda m: qual, params)
    if not ret:
        if not qual:
            return []
        if last.startswith("~"):
            return [("dtor", f"static void {probe}({qual}* p) {{ p->{last}(); }}")]
        types = [p.strip() for p in split_commas(params)] if params and params != "void" else []
        if any(t == "..." for t in types):
            return []
        decl = ", ".join(f"{t} a{x}" for x, t in enumerate(types))
        use = f"{qual} x({', '.join(f'a{x}' for x in range(len(types)))});" if types else f"{qual} x;"
        return [("ctor", f"static void {probe}({decl}) {{ {use} }}")]
    out = [("function", f"static auto {probe}_f = static_cast<{ret} (*)({params})>(&{name});")]
    if qual:
        out.append(("member", f"static auto {probe}_m = static_cast<{ret} ({qual}::*)({params}) {after}>(&{name});"))
    return out


def in_template(cl, cursor):
    """Is cursor a class template, or inside one?"""
    while not cl.clang_Cursor_isNull(cursor) and cursor.kind != K_TRANSLATION_UNIT:
        if cursor.kind in TEMPLATE_KINDS:
            return True
        cursor = cl.clang_getCursorSemanticParent(cursor)
    return False


def macro_invocation(text, b, e):
    """Is text[b:e] one invocation of a function-like macro (NAME(...))?"""
    m = IDENT.match(text, b)
    if not m:
        return False
    i = skip_space(text, m.end())
    return i < e and text[i] == "(" and skip_balanced(text, i) == e


def find_weak_macros(text):
    """The object-like macros of the file that stand for a weak attribute: name -> the text without "weak"."""
    found = {}
    for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+(__attribute__[ \t]*\(\(.*\)\))[ \t]*$",
                         text, re.M):
        rest = drop_weak(m.group(2))
        if rest is not None:
            found[m.group(1)] = rest
    return found


def drop_weak(attr):
    """An __attribute__((...)) without its "weak" ("" when nothing is left); None when it has no "weak"."""
    inner = attr[attr.index("((") + 2:attr.rindex("))")]
    parts = [p.strip() for p in split_commas(inner)]
    rest = [p for p in parts if p not in ("weak", "__weak__")]
    if len(rest) == len(parts):
        return None
    return f"__attribute__(({', '.join(rest)}))" if rest else ""


def split_commas(s):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
            continue
        depth += ch in "([{"
        depth -= ch in ")]}"
        cur += ch
    parts.append(cur)
    return parts


def weak_edits(text, x, y, weak_macros):
    """The edits that take "weak" out of the attributes in text[x:y]: a declaration that stays weak would make
    every reference a weak import."""
    edits, i = [], x
    while i < y:
        m = IDENT.search(text, i, y)
        if not m:
            break
        word = m.group(0)
        if word == "__attribute__":
            j = skip_space(text, m.end())
            k = skip_balanced(text, j)
            rest = drop_weak(text[m.start():k])
            if rest is not None:
                edits.append((m.start(), k, rest))
            i = k
        elif word in weak_macros:
            edits.append((m.start(), m.end(), weak_macros[word]))
            i = m.end()
        else:
            i = m.end()
    return edits


def ctor_initializer(text, loc, spelled, bb):
    """Where a constructor's body starts, its member initializers included (the ':' before them, else bb)."""
    m = IDENT.match(text, loc)
    if not m or m.group(0) != spelled:
        raise SliceError("the constructor's name is not where clang puts it")
    i = skip_space(text, m.end())
    if text.startswith("<", i):
        raise SliceError("a constructor name with template arguments")
    if not text.startswith("(", i):
        raise SliceError("no parameter list after the constructor's name")
    i = skip_balanced(text, i)
    while i < bb:
        i = skip_space(text, i)
        if text[i] == ":" and not text.startswith("::", i):
            return i
        if text[i] in "([":
            i = skip_balanced(text, i)
        elif text[i] in "\"'":
            i = skip_literal(text, i)
        else:
            i += 1
    return bb


def end_with_semicolon(text, e):
    """e, or past the ';' that ends the declaration there (= default; and data definitions)."""
    j = skip_space(text, e)
    m = re.compile(r"=\s*(default|delete)\b").match(text, j)
    if m:
        j = skip_space(text, m.end())
    if j < len(text) and text[j] == ";":
        return j + 1
    if j < len(text) and text[j] == ",":
        raise SliceError("one of several names in a declaration")
    return e


def skip_attributes(text, i):
    """Past white space and attribute specifiers (__attribute__((...)), [[...]], alignas(...)) at i."""
    while True:
        i = skip_space(text, i)
        for word in ("__attribute__", "alignas"):
            if text.startswith(word, i) and text[skip_space(text, i + len(word))] == "(":
                i = skip_balanced(text, skip_space(text, i + len(word)))
                break
        else:
            if text.startswith("[[", i):
                i = skip_balanced(text, i)
            else:
                return i


def extern_edits(text, b, e, loc, spelled, size):
    """External data: where "extern" goes in front (None: it is there), and the edits that remove the initializer and
    give an array of unwritten bound its size (sizeof of the declaration must still work)."""
    m = IDENT.match(text, loc)
    if not m:
        raise SliceError("the name is not where clang puts it")
    if m.group(0) != spelled and not re.search(rf"^[ \t]*#[ \t]*define[ \t]+{m.group(0)}[ \t]+{spelled}[ \t]*$", text, re.M):
        raise SliceError(f"the name is spelled {m.group(0)}, which is not a macro for {spelled}")
    i = m.end()
    closed = False
    edits = []
    first_bound = True
    while True:                         # the rest of the declarator: array bounds, a pointer to function's parameters
        i = skip_attributes(text, i)
        if text.startswith("[", i) and not text.startswith("[[", i):
            if first_bound and not closed and text[skip_space(text, i + 1)] == "]":
                if size < 0:
                    raise SliceError("an array of unwritten bound whose size clang does not give")
                edits.append((i + 1, i + 1, str(size)))
            first_bound = False
            i = skip_balanced(text, i)
        elif text.startswith(")", i):
            i += 1
            closed = True
        elif text.startswith("(", i) and closed:
            i = skip_balanced(text, i)
        else:
            break
    if text[i] in "=({":
        if i >= e:
            raise SliceError("the initializer is not inside the declaration")
        edits.append((i, e, ""))
        j = skip_attributes(text, e)
    else:
        j = i
    if j >= len(text) or text[j] not in ";,":
        raise SliceError(f"the declaration does not end at {describe(text, e)}: {text[j:j + 12]!r}")
    at = skip_attributes(text, b)
    if text.startswith("extern", at) and not (text[at + 6].isalnum() or text[at + 6] == "_"):
        at = None
    return at, edits


def check_directives(text, x, y):
    """A removed stretch may hold preprocessor lines (they stay); it may not start or end inside one."""
    ls = text.rfind("\n", 0, x) + 1
    if text[ls:x].strip().startswith("#"):
        raise SliceError("the cut starts on a preprocessor line")


def removed_lines(text, x, y):
    """What replaces text[x:y]: its preprocessor lines, every other line empty (so the line count stays)."""
    lines, i = [], x
    line_start = False                  # x is a token, never the start of a line
    in_comment = False
    while i < y:
        if line_start and not in_comment:
            j = i
            while j < y and text[j] in " \t":
                j += 1
            if j < y and text[j] == "#":
                while True:             # the directive, with its continuation lines
                    nl = text.find("\n", i)
                    if nl < 0 or nl >= y:
                        raise SliceError("a preprocessor line ends the removed text")
                    lines.append(text[i:nl])
                    i = nl + 1
                    if text[nl - 1] != "\\":
                        break
                continue
        line_start = False
        c = text[i]
        if c == "\n":
            lines.append("")
            line_start = True
            i += 1
        elif in_comment:
            if text.startswith("*/", i):
                in_comment = False
                i += 2
            else:
                i += 1
        elif text.startswith("/*", i):
            in_comment = True
            i += 2
        elif text.startswith("//", i):
            nl = text.find("\n", i)
            i = y if nl < 0 or nl > y else nl
        elif c in "\"'":
            i = min(skip_literal(text, i), y)
        else:
            i += 1
    lines.append("")
    return "\n".join(lines)


CODE_NOISE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def apply(text, items, keep, src_name):
    """The copy for a keep list. A static object the list leaves out stays when what remains of the copy still names it
    (an internal function or variable of the file that uses it), since the copy would not compile without it."""
    keep = set(keep)
    while True:
        result = cut(text, items, keep, src_name)
        words = set(re.findall(r"[A-Za-z_]\w*", CODE_NOISE.sub(" ", result)))
        named = {it["group"] for it in items if it["kind"] == "static" and it["name"] not in keep
                 and it["spelled"] in words}
        if not named:
            return result
        keep |= {it["name"] for it in items if it.get("group") in named and it["kind"] == "static"}


def cut(text, items, keep, src_name):
    edits, decls = [], {}
    for it in items:
        if it["name"] not in keep:
            edits.extend(it["edits"])
        if it["kind"] == "extern":
            decls.setdefault(it["decl"], []).append(it)
    groups = {}
    for it in items:
        if "group" in it:
            groups.setdefault(it["group"], []).append(it)
    for start, names in groups.items():
        dropped = [it for it in names if it["name"] not in keep]
        if dropped and len(dropped) != len(names):
            what = {"static": "one declaration", "instantiation": "one explicit instantiation"}.get(
                names[0]["kind"], "one macro invocation")
            raise SliceError(f"{what} ({describe(text, start)}) defines {len(names)} names,"
                             " but the keep list keeps only some of them")
    for start, names in decls.items():
        dropped = [it for it in names if it["name"] not in keep]
        if dropped and len(dropped) != len(names):
            raise SliceError(f"{', '.join(it['name'] for it in names)} share one declaration ({describe(text, start)}),"
                             " but the keep list keeps only some of them")
        if dropped:
            edits.extend(names[0].get("decl_edits", []))
            if names[0]["extern_at"] is not None:
                edits.append((names[0]["extern_at"], names[0]["extern_at"], "extern "))
    edits = sorted(tuple(x) for x in edits)
    out, pos = [], 0
    for x, y, insert in edits:
        if x < pos:
            raise SliceError(f"overlapping edits at {describe(text, x)}")
        out.append(text[pos:x])
        out.append(insert)
        out.append(removed_lines(text, x, y))
        pos = y
    out.append(text[pos:])
    return f'#line 1 "{src_name}"\n' + "".join(out)


def sha1(text):
    return hashlib.sha1(text.encode("utf-8", errors="surrogateescape")).hexdigest()


def cmd_index(args):
    parser = Parser(args.compiler[0])
    try:
        text, tu, items, problems = index_file(parser, args.src, args.compiler[1:])
    except SliceError as err:
        sys.exit(f"slicer: {args.src}: {err}")
    if problems:
        sys.exit(f"slicer: {args.src}: definitions the slicer cannot cut exactly:\n  " + "\n  ".join(problems))
    out = {"src": str(args.src), "sha1": sha1(text), "flags": args.compiler[1:], "items": items}
    pathlib.Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    tmp = pathlib.Path(str(args.out) + ".tmp")
    tmp.write_text(json.dumps(out, indent=0) + "\n")
    os.replace(tmp, args.out)
    if args.depfile:
        deps = [args.src] + parser.includes(tu)
        esc = lambda p: p.replace(" ", "\\ ")
        pathlib.Path(args.depfile).write_text(esc(str(args.out)) + ": " + " \\\n  ".join(esc(d) for d in deps) + "\n")


def for_file(names, src):
    """The names of a list that apply to src: NAME, or NAME@PATH when src is src/PATH."""
    out = set()
    for n in names:
        name, _, path = n.partition("@")
        if not path or pathlib.Path(src).resolve().as_posix().endswith("/src/" + path):
            out.add(name)
    return out


def cmd_slice(args):
    idx = json.loads(pathlib.Path(args.index).read_text())
    text = pathlib.Path(args.src).read_text(encoding="utf-8", errors="surrogateescape")
    if sha1(text) != idx["sha1"]:
        sys.exit(f"slicer: the index {args.index} is not of the current {args.src}")
    if args.keep:
        keep = for_file(pathlib.Path(args.keep).read_text().split(), args.src)
    else:
        keep = {it["name"] for it in idx["items"]} - for_file(pathlib.Path(args.drop).read_text().split(), args.src)
    try:
        result = apply(text, idx["items"], keep, str(pathlib.Path(args.src).resolve()))
    except SliceError as err:
        sys.exit(f"slicer: {args.src}: {err}")
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text(encoding="utf-8", errors="surrogateescape") != result:
        tmp = pathlib.Path(str(out) + ".tmp")
        tmp.write_text(result, encoding="utf-8", errors="surrogateescape")
        os.replace(tmp, out)


def cmd_defs(args):
    parser = Parser(args.compiler[0])
    text, tu, items, problems = index_file(parser, args.src, args.compiler[1:])
    for it in items:
        if it["kind"] not in ("static", "guard", "template"):
            print(it["name"])
        elif args.static:
            print(it["name"], it["kind"], it.get("spelled", ""))
    for p in problems:
        print("PROBLEM " + p, file=sys.stderr)
    if problems:
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("index")
    p.add_argument("--src", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--depfile")
    p.add_argument("compiler", nargs="+")
    p.set_defaults(fn=cmd_index)
    p = sub.add_parser("slice")
    p.add_argument("--src", required=True)
    p.add_argument("--index", required=True)
    which = p.add_mutually_exclusive_group(required=True)
    which.add_argument("--keep")
    which.add_argument("--drop")
    p.add_argument("--out", required=True)
    p.set_defaults(fn=cmd_slice)
    p = sub.add_parser("defs")
    p.add_argument("--src", required=True)
    p.add_argument("--static", action="store_true")
    p.add_argument("compiler", nargs="+")
    p.set_defaults(fn=cmd_defs)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
