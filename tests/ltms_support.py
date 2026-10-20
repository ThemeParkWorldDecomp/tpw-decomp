"""Queued file and virtual resource fakes shared by ltms tests."""
import struct
import tpwtest as T
import calllog as CL


def word(e, a, v):
    e.write(a, struct.pack(">I", T.u32(v)))


def files(e):
    """Record file lifetime and operations, with per-case return queues."""
    def storage(e):
        e.cl.rec("Storage", (e.cl.newlab(e.r(3)),))

    def file(e):
        e.cl.rec("File", (e.cl.newlab(e.r(3)),))
        word(e, e.r(3) + 4, 0)

    def opened(e):
        e.cl.rec("StorageOpen", (e.cl.lab(e.r(4)),), (e.cstr(e.r(5)),))
        word(e, e.r(3), e.cl.ret("StorageOpen"))

    def openfile(e):
        e.cl.rec("OpenFile", (e.cl.lab(e.r(4)), e.cl.lab(e.r(5)), e.r(7), e.cl.lab(e.r(8))),
                 (e.cstr(e.r(6)),))
        word(e, e.r(5) + 4, e.lt_impl if e.cl.ret("Implementation", 1) else 0)
        e.lt_pos = 0
        word(e, e.r(3), e.cl.ret("OpenFile"))

    def read(e):
        size = e.r(5)
        e.cl.rec("Read", (size,))
        n = e.cl.ret("Read", size)
        if n > 0:
            e.write(e.r(4), e.lt_bytes[e.lt_pos:e.lt_pos + n])
            e.lt_pos += n
        e.set_r(3, T.u32(n))

    def path(e):
        e.cl.rec("FullPath", (e.cl.lab(e.r(4)), e.r(7)), (e.cstr(e.r(6)),))
        e.write(e.r(5), b"root:language:\0")
        word(e, e.r(3), e.cl.ret("FullPath"))

    def query(name, value):
        def fake(e):
            e.cl.rec(name, (e.cl.lab(e.r(3)),))
            e.set_r(3, T.u32(e.cl.ret(name, value(e))))
        return fake

    s = CL.Stubs()
    def delete(e):
        label = e.cl.lab(e.r(3))
        if label > 0:
            e.cl.rec("delete", (label,))
        e.cl.forget(e.r(3))
    s.imp("__dla__FPv", delete)
    s.imp("__dl__FPv", delete)
    s.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", storage)
    # Destructor flags are ABI details, not events.
    s.imp("__dt__Q33bfl10FileSystem12CFileStorageFv",
          lambda e: e.cl.rec("~Storage", (e.cl.lab(e.r(3)),)))
    s.imp("__ct__Q33bfl10FileSystem5CFileFv", file)
    s.imp("__dt__Q33bfl10FileSystem5CFileFv", lambda e: e.cl.rec("~File", (e.cl.lab(e.r(3)),)))
    s.imp("Open__Q33bfl10FileSystem16CDiscFileStorageFPCc", opened)
    s.imp("OpenFile__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem5CFilePCcUlPv", openfile)
    s.imp("FullPath__Q33bfl10FileSystem12CFileStorageFPcPCcUi", path)
    s.imp("Close__Q33bfl10FileSystem12CFileStorageFv", lambda e: e.cl.rec("StorageClose", (e.cl.lab(e.r(3)),)))
    s.imp("GetSize__Q33bfl10FileSystem5CFileFv", query("GetSize", lambda e: len(e.lt_bytes)))
    s.imp("GetPos__Q33bfl10FileSystem5CFileFv", query("GetPos", lambda e: e.lt_pos))
    e.lt_impl_vt = T.persist_alloc(e, 20)
    word(e, e.lt_impl_vt + 12, T.fake_function(e, read))
    e.lt_impl = T.persist_alloc(e, 4, struct.pack(">I", e.lt_impl_vt))
    return s


def resource(e):
    """A resource whose virtual slots only record calls."""
    vt = T.persist_alloc(e, 28)
    for off, name in ((8, "Release"), (12, "AddRef"), (20, "ManagerRelease"), (24, "ManagerAddRef")):
        word(e, vt + off, T.fake_function(e, lambda e, n=name: e.cl.rec(n, (e.cl.lab(e.r(3)),))))
    return T.persist_alloc(e, 8, struct.pack(">Ii", vt, 1))


def install(e, s, fixed, queues):
    e.cl = CL.PpcLog({a: label for label, a in fixed.items()}, queues)
    saved = s.activate(e)
    return saved
