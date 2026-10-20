"""Unit v5722, AddressBook::Add and AddressBook::Remove, against libtpw_unit_AddressBook_Add.dylib (tests/unit_AddressBook_Add.cpp), main app
("SimTheme Park"), src/email_load_AddressBook_Save_AddressBook_Add.cpp:
  AddressBook::Add 0x102065f8, AddressBook::Remove 0x10206728

A case is a list of strings and a call. The PowerPC side builds the Mac list (count, sentinel at +4, nodes prev/next/string word, the string
word pointing to a representation {length, capacity, refs, data}) and the original runs; its callees (list insert 0x101b25e0, sort
0x1020811c, unique 0x1020833c, erase 0x10208838, string operator== 0x1020702c) are modelled in Python on the list's content. The host side
runs the translation on a real std::list<std::string> (the same strings in, the list written back as text).

What is compared: the answer and the whole list afterwards (the strings in order; on the Mac side also the count word and the links).
No fused operations. Not tested: nothing outside the strings being plain ASCII without a newline (the model compares bytes the same way
as std::string).
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AddressBook_Add"
MASK = 0xFFFFFFFF

FN_ADD, FN_REMOVE = 0x102065F8, 0x10206728
CALLEE_INSERT, CALLEE_SORT, CALLEE_UNIQUE, CALLEE_ERASE, CALLEE_EQ = 0x101B25E0, 0x1020811C, 0x1020833C, 0x10208838, 0x1020702C


# ---------------------------------------------------------------------------------------------------------------- the Mac side

def mac_string(e, text):
    """Allocates a std::string representation; answers the representation's address (the string word)."""
    data = text.encode()
    d = e.alloc(len(data) + 1, data + b"\0")
    rep = e.alloc(16, struct.pack(">IIII", len(data), len(data), 0, d))
    return rep


def mac_text(e, rep):
    n = e.u32(rep)
    d = e.u32(rep + 12)
    return e.read(d, n).decode()


def nodes(e, lst):
    out = []
    n = e.u32(lst + 8)
    while n != lst + 4 and len(out) < 1000:
        out.append(n)
        n = e.u32(n + 4)
    return out


def relink(e, lst, ns):
    prev = lst + 4
    for n in ns:
        e.write(prev + 4 if prev == lst + 4 else prev + 4, struct.pack(">I", n))
        e.write(n, struct.pack(">I", prev))
        prev = n
    e.write(prev + 4, struct.pack(">I", lst + 4))
    e.write(lst + 4, struct.pack(">I", prev))
    e.write(lst, struct.pack(">I", len(ns)))


# The sentinel's prev is at lst+4 and its next at lst+8 (a node: prev +0, next +4); relink keeps that: for the sentinel, "prev + 4" is lst+8.


def node_text(e, n):
    return mac_text(e, e.u32(n + 8))


def stub_insert(e):
    res, lst, pos, strp = e.r(3), e.r(4), e.r(5), e.r(6)
    node = e.alloc(12, struct.pack(">III", 0, 0, e.u32(strp)))
    ns = nodes(e, lst)
    i = ns.index(pos) if pos in ns else len(ns)
    ns.insert(i, node)
    relink(e, lst, ns)
    e.write(res, struct.pack(">I", node))
    e.set_r(3, node)


def stub_sort(e):
    lst = e.r(3)
    ns = sorted(nodes(e, lst), key=lambda n: node_text(e, n).encode())
    relink(e, lst, ns)


def stub_unique(e):
    lst = e.r(3)
    keep = []
    for n in nodes(e, lst):
        if keep and node_text(e, keep[-1]) == node_text(e, n):
            continue
        keep.append(n)
    relink(e, lst, keep)


def stub_erase(e):
    res, lst, pos = e.r(3), e.r(4), e.r(5)
    ns = nodes(e, lst)
    i = ns.index(pos)
    nxt = ns[i + 1] if i + 1 < len(ns) else lst + 4
    del ns[i]
    relink(e, lst, ns)
    e.write(res, struct.pack(">I", nxt))
    e.set_r(3, nxt)


def stub_eq(e):
    a, b = mac_text(e, e.u32(e.r(3))), mac_text(e, e.u32(e.r(4)))
    e.set_r(3, 1 if a == b else 0)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for addr, fn in ((CALLEE_INSERT, stub_insert), (CALLEE_SORT, stub_sort), (CALLEE_UNIQUE, stub_unique),
                     (CALLEE_ERASE, stub_erase), (CALLEE_EQ, stub_eq)):
        T.stub_internal(runner.emu, addr, fn)


def ppc_run(e, case):
    lst = e.alloc(12, b"\0" * 12)
    ns = []
    for t in case["items"]:
        ns.append(e.alloc(12, struct.pack(">III", 0, 0, mac_string(e, t))))
    relink(e, lst, ns)
    if case["op"] == "add":
        arg = e.alloc(4, struct.pack(">I", mac_string(e, case["text"])))
        r3, _, _ = T.call_ppc(e, FN_ADD, [lst, arg])
        answer = T.s32(r3)
    else:
        r3, _, _ = T.call_ppc(e, FN_REMOVE, [lst, case["pos"] & MASK])
        answer = T.s32(r3)
    cur = nodes(e, lst)
    links = [(e.u32(n), e.u32(n + 4)) for n in cur]
    ok = []
    prev = lst + 4
    for n, (p, nx) in zip(cur, links):
        ok.append(p == prev)
        prev = n
    ok.append(e.u32(lst + 4) == prev)
    return {"answer": answer, "items": [node_text(e, n) for n in cur], "count": e.u32(lst), "linked": all(ok)}


# ---------------------------------------------------------------------------------------------------------------- the host side

def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_abadd_add.restype = C.c_int32
    lib.tpw_abadd_add.argtypes = [C.c_char_p, C.c_int32, C.c_char_p, C.c_char_p, C.POINTER(C.c_int32)]
    lib.tpw_abadd_remove.restype = C.c_int32
    lib.tpw_abadd_remove.argtypes = [C.c_char_p, C.c_int32, C.c_int32, C.c_char_p, C.POINTER(C.c_int32)]
    lib.bound = True


def host_run(lib, case):
    bind(lib)
    joined = "\n".join(case["items"]).encode()
    n = len(case["items"])
    out = C.create_string_buffer(16384)
    outn = C.c_int32(0)
    if case["op"] == "add":
        r = lib.tpw_abadd_add(joined, n, case["text"].encode(), out, C.byref(outn))
    else:
        r = lib.tpw_abadd_remove(joined, n, T.s32(case["pos"]), out, C.byref(outn))
    items = out.value.decode().split("\n") if outn.value else []
    return {"answer": r, "items": items, "count": outn.value, "linked": True}


def ppc(e, case):
    return ppc_run(e, case)


def host(lib, case):
    return host_run(lib, case)


# ---------------------------------------------------------------------------------------------------------------- cases

ALPHA = "abc"


def rstr(r):
    return "".join(r.choice(ALPHA) for _ in range(r.randrange(0, 4)))


def items(r):
    return [rstr(r) for _ in range(r.choice([0, 0, 1, 2, 3, 5, 8]))]


def add_case(r, its=None, text=None):
    its = items(r) if its is None else its
    if text is None:
        text = r.choice(its) if its and r.random() < 0.5 else rstr(r)
    return {"op": "add", "items": its, "text": text}


def rem_case(r, its=None, pos=None):
    its = items(r) if its is None else its
    if pos is None:
        pos = r.choice([-1, 0, 1, len(its) - 1, len(its), len(its) + 1, r.randrange(0, 10), 0x80000000, 0x7FFFFFFF])
    return {"op": "remove", "items": its, "pos": pos & MASK}


def add_edges():
    r = random.Random(722)
    yield add_case(r, [], "")
    yield add_case(r, [], "x")
    yield add_case(r, ["b"], "b")
    yield add_case(r, ["b"], "a")
    yield add_case(r, ["b"], "c")
    yield add_case(r, ["a", "b", "c"], "bb")
    yield add_case(r, ["c", "a", "a", "b", "b"], "a")
    yield add_case(r, ["c", "a", "a", "b", "b"], "c")
    yield add_case(r, ["", "a"], "")
    yield add_case(r, ["a", "ab", "abc"], "ab")


def rem_edges():
    r = random.Random(723)
    yield rem_case(r, [], 0)
    yield rem_case(r, [], -1)
    yield rem_case(r, ["a"], 0)
    yield rem_case(r, ["a"], 1)
    yield rem_case(r, ["a"], -1)
    yield rem_case(r, ["a", "b", "c"], 0)
    yield rem_case(r, ["a", "b", "c"], 1)
    yield rem_case(r, ["a", "b", "c"], 2)
    yield rem_case(r, ["a", "b", "c"], 3)
    yield rem_case(r, ["a", "b", "c"], 0x80000000)
    yield rem_case(r, ["a", "b", "c"], 0x7FFFFFFF)


TESTS = [
    FnTest("AddressBook::Add", ppc, host, edge=add_edges, rnd=add_case),
    FnTest("AddressBook::Remove", ppc, host, edge=rem_edges, rnd=rem_case),
]
