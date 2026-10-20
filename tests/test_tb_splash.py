"""Differential tests for engine tb_splash.cpp. See difftest.py and tpwtest.py.

Callees, the same on both sides: the OpenGL calls, SystemTask and UTimer::GetAbsolute are
logged (the clock answers base + n * step microseconds for its n-th call, so busy-wait
loops end); TGALoadAndConvert (tb_loaders.cpp) answers its n-th call from the case's image
list; gei_DrawPrimitive is a fake whose BeginDraw, EndDraw and DrawFilledBox are logged
(DrawFilledBox answers from a list; on the PowerPC side through the original's vtable
call at +0x2c); the engine allocator's free records and clears the caller's pointer.
RenderSystem_FlipScreen and RenderSystem_ClearViewport run the translated and the
original code, with the renderer and FrontWindow fakes of test_tb_resource_system.
snprintf on the PowerPC side is tpwtest.c_format.
"""
import ctypes
import random
import struct

import test_tb_resource_system as RS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"

DISPLAY, SYSTEM, ERRORS = 0x10165634, 0x10165f80, 0x10052b44
SPLASH, FIRST, START, DRAWPRIM_PTR = 0x10169110, 0x10051dd8, 0x1016910c, 0x10052b3c
FREE_FN, TGA = 0x10051f74, 0x100156d8
GL_IDS = {"glPushAttrib": 1, "glPopAttrib": 2, "glDisable": 3, "glPixelStorei": 4, "glRasterPos3f": 5,
          "glPixelZoom": 6, "glDrawPixels": 7, "glGetError": 8}
VP, U32, I32 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int32


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def fbits(x):
    return T.f32_bits(T.f32(x))

# ----------------------------------------------------------------- PowerPC fakes


def setup(runner):
    RS.setup(runner)
    emu = runner.emu
    for name, ident in GL_IDS.items():
        def gl(e, ident=ident):
            ints = [e.r(3 + i) for i in range(4)] if ident in (1, 3, 4, 7) else [0] * 4
            floats = [fbits(e.f(1 + i)) for i in range(3)] if ident in (5, 6) else [0, 0, 0]
            if ident == 6:
                floats[2] = 0
            if ident in (1, 3):
                ints[1:] = [0, 0, 0]
            if ident == 4:
                ints[2:] = [0, 0]
            row = [ident] + [T.u32(x) for x in ints] + floats
            if ident == 7:
                row[4] = e.r(6)
                row[5] = e.sp_label(e.r(7))
                row[6] = row[7] = 0
            e.sp_gl.append(tuple(row))
            if ident == 8:
                e.set_r(3, 0)
        emu.stub(name, gl)
    emu.stub("SystemTask", lambda e: setattr(e, "sp_tasks", e.sp_tasks + 1))

    def now(e):
        base, step = e.sp_time
        v = (base + e.sp_time_n * step) & 0xFFFFFFFFFFFFFFFF
        e.sp_time_n += 1
        e.write(e.r(3), struct.pack(">Q", v))

    emu.stub("GetAbsolute__Q213SamsUtilities6UTimerFv", now)

    def tga(e):
        i = e.sp_img_n
        e.sp_img_n += 1
        e.sp_tga.append((e.cstr(e.r(3)), T.s32(e.r(8)), e.r(6), e.r(7)))
        imgs = e.sp_images
        if i >= len(imgs) or not imgs[i][0]:
            e.set_r(3, 0)
            return
        ok, w, h, data = imgs[i]
        a = e.alloc(max(len(data), 4), data)
        e.sp_labels[a] = "img%d" % i
        e.sp_sizes[a] = len(data)
        e.write(e.r(4), struct.pack(">i", w))
        e.write(e.r(5), struct.pack(">i", h))
        e.set_r(3, a)

    T.stub_internal(emu, TGA, tga)

    def free(e):
        ref = e.r(3)
        e.sp_frees.append(e.sp_label(e.u32(ref)))
        if e.sp_free_clears:
            e.write(ref, b"\0\0\0\0")

    emu.sp_free_tv = T.fake_function(emu, free)

    def begin(e):
        e.sp_dp.append((1,))
        e.set_r(3, 1)

    def end(e):
        e.sp_dp.append((2,))
        e.set_r(3, 1)

    def box(e):
        k = sum(1 for x in e.sp_dp if x[0] == 3)
        e.sp_dp.append((3, fbits(e.f(1)), fbits(e.f(2)), fbits(e.f(3)), fbits(e.f(4)), T.s32(e.r(8)), e.r(9)))
        res = e.sp_dp_results
        e.set_r(3, T.u32(res[min(k, len(res) - 1)] if res else 1))

    vt = T.persist_alloc(emu, 0x40)
    for off, fn in ((0x24, begin), (0x28, end), (0x30, box)):
        emu.write(vt + off, struct.pack(">I", T.fake_function(emu, fn)))
    emu.sp_dp_obj = T.persist_alloc(emu, 0x40)
    emu.write(emu.sp_dp_obj + 0x2c, struct.pack(">I", vt))

# ----------------------------------------------------------------- case model
# c: disp (flags, width, height, frame, f84), sys (flags, window, rs), front, flips,
# errors (count, entries), first, start, time (base, step), images [(ok, w, h, bytes)],
# dp (present, results), splash (ints[7], scale, back) where back = None or
# (data bytes or None, w, h, bpp).


def ppc_side(emu, c):
    emu.sp_labels, emu.sp_sizes, emu.sp_gl, emu.sp_tasks, emu.sp_time_n = {}, {}, [], 0, 0
    emu.sp_tga, emu.sp_img_n, emu.sp_frees, emu.sp_dp = [], 0, [], []
    emu.sp_images, emu.sp_time, emu.sp_dp_results = c["images"], c["time"], c["dp"][1]
    emu.sp_free_clears = c.get("free_clears", True)
    emu.alloc_log, emu.asserts, emu.rs_log = [], [], []
    emu.sp_label = lambda a: None if a == 0 else emu.sp_labels.get(a, "?%x" % a)
    emu.rs_flips, emu.front = c["flips"], c["front"]
    flags, w, h, frame, f84 = c["disp"]
    emu.write(DISPLAY, struct.pack(">I", flags))
    emu.write(DISPLAY + 8, struct.pack(">ii", w, h))
    emu.write(DISPLAY + 0x80, struct.pack(">ii", frame, f84))
    sflags, window, rs = c["sys"]
    emu.write(SYSTEM, struct.pack(">I", sflags))
    emu.write(SYSTEM + 0x10, struct.pack(">I", window))
    emu.write(SYSTEM + 0x24, struct.pack(">I", RS.ppc_render_system(emu, rs)))
    emu.write(ERRORS, RS.pack_errors(">", c["errors"]))
    emu.write(FIRST, struct.pack(">i", c["first"]))
    emu.write(START, struct.pack(">I", c["start"]))
    emu.write(FREE_FN, struct.pack(">I", emu.sp_free_tv))
    emu.write(DRAWPRIM_PTR, struct.pack(">I", emu.sp_dp_obj if c["dp"][0] else 0))


def ppc_back(emu, back):
    if back is None:
        return 0
    data, w, h, bpp = back
    obj = emu.alloc(0x10)
    px = 0
    if data is not None:
        px = emu.alloc(max(len(data), 4), data)
        emu.sp_labels[px] = "backpx"
        emu.sp_sizes[px] = len(data)
    emu.write(obj, struct.pack(">Iiii", px, w, h, bpp))
    emu.sp_labels[obj] = "back"
    return obj


def ppc_splash(emu, sp, at=None):
    ints, scale, back = sp
    obj = at if at is not None else emu.alloc(0x38, b"\x5a" * 0x38)
    b = ppc_back(emu, back)
    emu.write(obj, struct.pack(">iiiiIi", ints[0], ints[1], ints[2], ints[3], b, ints[4]))
    emu.write(obj + 0x18, struct.pack(">f", scale))
    emu.write(obj + 0x28, struct.pack(">Ii", T.u32(ints[5]), ints[6]))
    emu.sp_labels[obj] = "self"
    return obj


def ppc_back_view(emu, b):
    if b == 0:
        return None
    px, w, h, bpp = struct.unpack(">Iiii", emu.read(b, 16))
    data = emu.read(px, emu.sp_sizes[px]) if px in emu.sp_sizes else None
    return (emu.sp_label(px), w, h, bpp, data)


def ppc_observe(emu, obj, ret, back_obj=None):
    out = dict(ret=ret)
    news = {}
    for kind, a, size in emu.alloc_log:
        if kind == "new":
            emu.sp_labels.setdefault(a, "new%d" % len(news))
            news[a] = size
    out["allocs"] = [("new", emu.sp_label(a)) if kind == "new" else ("delete", emu.sp_label(a))
                     for kind, a, size in emu.alloc_log]
    if obj is not None:
        v = struct.unpack(">iiiiIi", emu.read(obj, 24))
        scale = struct.unpack(">I", emu.read(obj + 0x18, 4))[0]
        start, draws = struct.unpack(">Ii", emu.read(obj + 0x28, 8))
        out["splash"] = dict(ints=[v[0], v[1], v[2], v[3], v[5], T.s32(start), draws], scale=scale,
                             back=emu.sp_label(v[4]))
        out["back_view"] = ppc_back_view(emu, v[4]) if v[4] and v[4] not in [a for k, a, s in emu.alloc_log
                                                                               if k == "delete"] else None
    if back_obj is not None:
        out["back"] = ppc_back_view(emu, back_obj)
    out["globals"] = (emu.s32(FIRST), emu.u32(START), emu.u32(SYSTEM), emu.u32(DISPLAY),
                      struct.unpack(">ii", emu.read(DISPLAY + 0x80, 8)))
    out["gl"] = list(emu.sp_gl)
    out["tasks"], out["clock"] = emu.sp_tasks, emu.sp_time_n
    out["tga"] = [(n, b) for n, b, f, p in emu.sp_tga]
    out["dp"] = list(emu.sp_dp)
    out["rs"] = list(emu.rs_log)
    out["frees"] = list(emu.sp_frees)
    out["asserts"] = list(emu.asserts)
    out["errors"] = RS.unpack_errors(">", emu.read(ERRORS, 0x204))[0]
    return out


class Host:
    def __init__(self, lib, c):
        self.lib, self.keep, self.labels, self.sizes = lib, [], {}, {}
        F(lib, "tpw_splash_reset", None)()
        F(lib, "tpw_dp_reset", None)()
        F(lib, "tpw_rs_reset", None)()
        F(lib, "tpw_cap_install", None)()
        F(lib, "tpw_cap_reset", None)()
        I32.in_dll(lib, "tpw_cap_free_clears").value = 1 if c.get("free_clears", True) else 0
        F(lib, "tpw_alloc_reset", None)()
        T.host_asserts(lib, reset=True)
        flags, w, h, frame, f84 = c["disp"]
        d = ctypes.addressof(ctypes.c_char.in_dll(lib, "gei_Display"))
        ctypes.memmove(d, struct.pack("<I", flags), 4)
        ctypes.memmove(d + 8, struct.pack("<ii", w, h), 8)
        ctypes.memmove(d + 0x80, struct.pack("<ii", frame, f84), 8)
        self.d = d
        sflags, window, rs = c["sys"]
        sysp = F(lib, "tpw_system_global", VP)()
        arr = (U32 * 8)()
        F(lib, "tpw_system_scalars", None, VP, ctypes.POINTER(U32), I32)(sysp, arr, 0)
        arr[0] = sflags
        F(lib, "tpw_system_scalars", None, VP, ctypes.POINTER(U32), I32)(sysp, arr, 1)
        F(lib, "tpw_system_window", VP, VP, VP, I32)(sysp, window or None, 1)
        F(lib, "tpw_system_renderer", I32, VP, I32, I32)(sysp, rs, 1)
        self.sysp = sysp
        VP.in_dll(lib, "tpw_front_window").value = c["front"] or None
        flips = c["flips"]
        (I32 * 8).in_dll(lib, "tpw_rs_flips")[:len(flips)] = flips
        I32.in_dll(lib, "tpw_rs_nflips").value = len(flips)
        self.errs = ctypes.addressof(ctypes.c_char.in_dll(lib, "gei_ErrorHandler"))
        ctypes.memmove(self.errs, RS.pack_errors("<", c["errors"]), 0x204)
        F(lib, "tpw_splash_first", ctypes.POINTER(I32))()[0] = c["first"]
        F(lib, "tpw_splash_start", ctypes.POINTER(U32))()[0] = c["start"]
        ctypes.c_uint64.in_dll(lib, "tpw_time_base").value = c["time"][0]
        ctypes.c_uint64.in_dll(lib, "tpw_time_step").value = c["time"][1]
        imgs = c["images"]
        blob, rows = b"", (I32 * 4 * 16).in_dll(lib, "tpw_images")
        for i, (ok, iw, ih, data) in enumerate(imgs[:16]):
            rows[i][0], rows[i][1], rows[i][2], rows[i][3] = iw, ih, 1 if ok else 0, len(blob)
            blob += data
        ctypes.memmove(ctypes.addressof(ctypes.c_char.in_dll(lib, "tpw_image_bytes")), blob, len(blob))
        I32.in_dll(lib, "tpw_image_count").value = len(imgs)
        self.images = imgs
        res = c["dp"][1]
        (I32 * 8).in_dll(lib, "tpw_dp_results")[:len(res)] = res
        I32.in_dll(lib, "tpw_dp_nresults").value = len(res)
        F(lib, "tpw_dp_install", None, I32)(1 if c["dp"][0] else 0)

    def lab(self, a):
        return None if not a else self.labels.get(a, "?%x" % a)

    def back(self, back):
        if back is None:
            return None
        data, w, h, bpp = back
        obj = F(self.lib, "tpw_new_back", VP)()
        px = None
        if data is not None:
            px = F(self.lib, "tpw_block", VP, U32, ctypes.c_char_p)(len(data), data)
            self.labels[px], self.sizes[px] = "backpx", len(data)
        F(self.lib, "tpw_back_set", None, VP, VP, I32, I32, I32)(obj, px, w, h, bpp)
        self.labels[obj] = "back"
        return obj

    def splash(self, sp, obj=None):
        ints, scale, back = sp
        obj = obj if obj is not None else F(self.lib, "tpw_splash_store", VP)()
        b = self.back(back)
        F(self.lib, "tpw_splash_set", None, VP, ctypes.POINTER(I32), ctypes.c_float, VP)(
            obj, (I32 * 7)(*ints), scale, b)
        self.labels[obj] = "self"
        return obj

    def start(self):
        F(self.lib, "tpw_alloc_reset", None)()
        I32.in_dll(self.lib, "tpw_alloc_log_on").value = 1

    def back_view(self, b):
        if not b:
            return None
        px, whb = VP(), (I32 * 3)()
        F(self.lib, "tpw_back_get", None, VP, ctypes.POINTER(VP), ctypes.POINTER(I32))(b, ctypes.byref(px), whb)
        data = ctypes.string_at(px.value, self.sizes[px.value]) if px.value in self.sizes else None
        return (self.lab(px.value), whb[0], whb[1], whb[2], data)

    def observe(self, obj, ret, back_obj=None):
        lib = self.lib
        I32.in_dll(lib, "tpw_alloc_log_on").value = 0
        # images first (their labels), then allocations
        ptrs = (VP * 16).in_dll(lib, "tpw_image_ptr")
        n_img = I32.in_dll(lib, "tpw_image_n").value
        for i in range(min(n_img, 16)):
            if ptrs[i]:
                self.labels[ptrs[i]] = "img%d" % i
                self.sizes[ptrs[i]] = len(self.images[i][3])
        n = I32.in_dll(lib, "tpw_alloc_n").value
        kinds = (I32 * 1024).in_dll(lib, "tpw_alloc_kind")
        aptr = (ctypes.c_uint64 * 1024).in_dll(lib, "tpw_alloc_ptr")
        allocs, k = [], 0
        for i in range(n):
            if kinds[i] in (1, 2):
                self.labels.setdefault(aptr[i], "new%d" % k)
                k += 1
                allocs.append(("new", self.lab(aptr[i])))
            else:
                allocs.append(("delete", self.lab(aptr[i])))
        out = dict(ret=ret, allocs=allocs)
        deleted = {a for kd, a in zip(kinds[:n], aptr[:n]) if kd in (3, 4)}
        if obj is not None:
            v, scale, b = (I32 * 7)(), ctypes.c_float(), VP()
            F(lib, "tpw_splash_get", None, VP, ctypes.POINTER(I32), ctypes.POINTER(ctypes.c_float),
              ctypes.POINTER(VP))(obj, v, ctypes.byref(scale), ctypes.byref(b))
            out["splash"] = dict(ints=list(v), scale=T.f32_bits(scale.value), back=self.lab(b.value))
            out["back_view"] = self.back_view(b.value) if b.value and b.value not in deleted else None
        if back_obj is not None:
            out["back"] = self.back_view(back_obj)
        d = self.d
        out["globals"] = (F(lib, "tpw_splash_first", ctypes.POINTER(I32))()[0],
                          F(lib, "tpw_splash_start", ctypes.POINTER(U32))()[0],
                          RS.host_get_system(lib, self.sysp)["scalars"][0],
                          struct.unpack("<I", ctypes.string_at(d, 4))[0],
                          struct.unpack("<ii", ctypes.string_at(d + 0x80, 8)))
        gl, rows = [], ((U32 * 8) * 128).in_dll(lib, "tpw_gl_log")
        gptr = (ctypes.c_uint64 * 128).in_dll(lib, "tpw_gl_ptr")
        for i in range(I32.in_dll(lib, "tpw_gl_n").value):
            row = list(rows[i])
            if row[0] == 7:
                row[5] = self.lab(gptr[i])
                row[6] = row[7] = 0
            gl.append(tuple(row))
        out["gl"] = gl
        out["tasks"] = I32.in_dll(lib, "tpw_systemtask_calls").value
        out["clock"] = I32.in_dll(lib, "tpw_time_calls").value
        names = ((ctypes.c_char * 128) * 16).in_dll(lib, "tpw_image_name")
        bpps = (I32 * 16).in_dll(lib, "tpw_image_bpp")
        out["tga"] = [(names[i].value.decode("latin-1"), bpps[i]) for i in range(n_img)]
        dp, kinds_dp = [], (I32 * 32).in_dll(lib, "tpw_dp_kind")
        boxes = ((ctypes.c_float * 4) * 32).in_dll(lib, "tpw_dp_box")
        cols = (I32 * 32).in_dll(lib, "tpw_dp_colour")
        texs = (ctypes.c_uint64 * 32).in_dll(lib, "tpw_dp_texture")
        for i in range(I32.in_dll(lib, "tpw_dp_n").value):
            if kinds_dp[i] == 3:
                dp.append((3,) + tuple(T.f32_bits(x) for x in boxes[i]) + (cols[i], texs[i]))
            else:
                dp.append((kinds_dp[i],))
        out["dp"] = dp
        nlog = I32.in_dll(lib, "tpw_rs_log_n").value
        out["rs"] = [(int(a), bool(b)) for a, b in zip((I32 * 32).in_dll(lib, "tpw_rs_log")[:nlog],
                                                       (I32 * 32).in_dll(lib, "tpw_rs_log_ok")[:nlog])]
        cn = I32.in_dll(lib, "tpw_cap_log_n").value
        ck = (I32 * 64).in_dll(lib, "tpw_cap_log_kind")
        cp = (VP * 64).in_dll(lib, "tpw_cap_log_ptr")
        out["frees"] = [self.lab(cp[i]) for i in range(cn) if ck[i] == 2]
        out["asserts"] = T.host_asserts(lib, reset=True)
        out["errors"] = RS.unpack_errors("<", ctypes.string_at(self.errs, 0x204))[0]
        # leave nothing for gei_Splash's destructor at exit
        F(lib, "tpw_splash_first", ctypes.POINTER(I32))()[0] = 0
        return out

# ----------------------------------------------------------------- cases


def rimage(r, w, h, bpp=4):
    if r.random() < 0.12:
        return (False, w, h, b"")
    return (True, w, h, r.randbytes(w * h * bpp))


def rcase(r, back=True):
    width = r.choice([640, 800, 1024, 640, 512, 1280, 700])
    height = r.choice([480, 600, 768, 400])
    imgw = r.choice([2, 3, 4, 8])
    images = [rimage(r, imgw, r.choice([1, 3, 8, 230, 300, 420, 700])) for _ in range(4)]
    # steps of 10 ms and more keep the busy waits (2.5 s, 3 s) short in the emulator; some
    # bases sit just below the point where the 32-bit millisecond count wraps
    step = r.choice([10 * 1000, 333333, 400 * 1000, 900 * 1000, 1500 * 1000, 2600 * 1000, 4000 * 1000])
    k = r.random()
    if k < 0.4:
        base = r.randint(10 ** 6, 10 ** 12)
    elif k < 0.6:
        base = (2 ** 32 - r.randint(0, 3000)) * 1000 + r.randint(0, 999)
    else:
        base = r.randint(0, 2 ** 52)
    time = (base, step)
    bw = imgw
    bh = r.choice([4, 16, 300, 500])
    b = (r.randbytes(bw * bh * 4) if r.random() < 0.85 else None, bw, bh, 4) if back and r.random() < 0.8 else None
    sp = ([r.choice([0, 0, 1, 5, 499, 500, 600, T.s32(T.rint(r))]), T.s32(T.rint(r)),
           r.choice([500, 1, 7, 1000]), T.s32(T.rint(r)), T.s32(T.rint(r)), T.s32(r.getrandbits(32)),
           r.choice([0, 1, 2, 3, 4, -1])],
          r.choice([0.2, 1.0, 0.0, 100.0, -1.0, 1e-6, T.f32(r.uniform(-5, 5))]), b)
    # half the cases start the clock just before "now", so Close's 3-second wait runs
    start = ((time[0] // 1000) - r.randint(0, 4000)) & 0xFFFFFFFF if r.random() < 0.5 else r.getrandbits(32)
    return dict(disp=(r.getrandbits(32), width, height, T.s32(T.rint(r)), T.s32(T.rint(r))),
                sys=(r.getrandbits(32), 0x7C0E0000, 1), front=r.choice([0x7C0E0000, 0x7C0E0000, 0x7C0E1000]),
                flips=[r.choice([1, 1, 0]) for _ in range(4)], errors=RS.rerrors(r), first=r.choice([0, 1, 1]),
                start=start, time=time, images=images, free_clears=r.random() < 0.8,
                dp=(True, [r.choice([1, 1, 1, 0]) for _ in range(3)]), splash=sp,
                language=r.choice([None, "american", "french", "x"]), path=r.choice([None, "data:init:", ""]),
                frames=r.choice([0, 0, 500, 1, -1, -500, 7, -0x80000000, T.s32(T.rint(r))]),
                overlay=(r.choice(["over.tga", "data:a.tga"]), T.s32(T.rint(r)), r.randint(0, 520)), flag=r.choice(
                    [-1, 0, 1, 2, 0x7fff]))


def splash_test(name, label, host_fn, args_ppc=lambda e, c: [], args_host=lambda h, c: [], argtypes=(),
                rnd=rcase, edge=None, back_only=False):
    def ppc(emu, c):
        ppc_side(emu, c)
        if back_only:
            b = ppc_back(emu, c["splash"][2])
            r3 = T.call_ppc(emu, emu.func(label), [b] + args_ppc(emu, c))[0]
            return ppc_observe(emu, None, T.s32(r3), back_obj=b)
        obj = ppc_splash(emu, c["splash"])
        r3 = T.call_ppc(emu, emu.func(label), [obj] + args_ppc(emu, c))[0]
        return ppc_observe(emu, obj, T.s32(r3))

    def host(lib, c):
        h = Host(lib, c)
        if back_only:
            b = h.back(c["splash"][2])
            h.start()
            ret = F(lib, host_fn, I32, VP, *argtypes)(b, *args_host(h, c))
            return h.observe(None, ret, back_obj=b)
        obj = h.splash(c["splash"])
        h.start()
        ret = F(lib, host_fn, I32, VP, *argtypes)(obj, *args_host(h, c))
        return h.observe(obj, ret)
    return FnTest(name, ppc, host, rnd=rnd, edge=edge)


def str_ppc(emu, s):
    return 0 if s is None else emu.alloc(len(s) + 1, s.encode() + b"\0")


def str_host(h, s):
    if s is None:
        return None
    b = ctypes.create_string_buffer(s.encode())
    h.keep.append(b)
    return b


def with_back(r):
    c = rcase(r)
    sp = c["splash"]
    bw = c["images"][0][1]
    c["splash"] = (sp[0], sp[1], (r.randbytes(bw * 300 * 4) if r.random() < 0.9 else None, bw, 300, 4))
    return c


def init_edges():
    r = random.Random(3)
    out = []
    for width in (640, 800, 1024, 600, 2048):
        for first in (0, 1):
            for frames in (0, -1, 500):
                c = rcase(r)
                c["disp"] = (0, width, 480, 0, 0)
                c["first"], c["frames"] = first, frames
                c["images"] = [rimage(r, 4, 700) for _ in range(4)]
                out.append(c)
    return out


def close_edges():
    """The first clock read in Close lands exactly 2998, 2999 or 3000 ms after the start."""
    r = random.Random(4)
    out = []
    for gap in (2998, 2999, 3000, 0, 2999 + 2 ** 32):
        c = rcase(r)
        base = 5 * 10 ** 9
        c["time"] = (base, 1000)
        c["start"] = (base // 1000 - gap) & 0xFFFFFFFF
        c["first"] = 1
        out.append(c)
    return out


def init_clock_edges():
    """Init's 2.5-second wait with clock steps on either side of 2500 ms."""
    r = random.Random(5)
    out = []
    for step in (2499 * 1000, 2500 * 1000, 2501 * 1000, 1250 * 1000, 2499999, 2500000 - 1):
        c = rcase(r)
        c["disp"] = (0, 640, 480, 0, 0)
        c["first"], c["frames"], c["time"] = 1, 0, (7 * 10 ** 9, step)
        c["images"] = [rimage(r, 4, 8) for _ in range(4)]
        c["images"][0] = (True, 4, 8, bytes(4 * 8 * 4))
        out.append(c)
    return out


TESTS = [
    splash_test("CDrawToBackBuffer::Close", "Close__17CDrawToBackBufferFv", "tpw_CDrawToBackBuffer_Close",
                back_only=True, rnd=with_back),
    splash_test("CDrawToBackBuffer::Overlay", "Overlay__17CDrawToBackBufferFPcii", "tpw_CDrawToBackBuffer_Overlay",
                args_ppc=lambda e, c: [str_ppc(e, c["overlay"][0]), T.u32(c["overlay"][1]), c["overlay"][2]],
                args_host=lambda h, c: [str_host(h, c["overlay"][0]), c["overlay"][1], c["overlay"][2]],
                argtypes=[ctypes.c_char_p, I32, I32], back_only=True, rnd=with_back),
    splash_test("CSplash::Init", "Init__7CSplashFPcPci", "tpw_CSplash_Init",
                args_ppc=lambda e, c: [str_ppc(e, c["language"]), str_ppc(e, c["path"]), T.u32(c["frames"])],
                args_host=lambda h, c: [str_host(h, c["language"]), str_host(h, c["path"]), c["frames"]],
                argtypes=[ctypes.c_char_p, ctypes.c_char_p, I32], rnd=lambda r: rcase(r, back=False),
                edge=lambda: init_edges() + init_clock_edges()),
    splash_test("CSplash::Close", "Close__7CSplashFv", "tpw_CSplash_Close", edge=close_edges),
    splash_test("CSplash::WinProgressBar", "WinProgressBar__7CSplashFv", "tpw_CSplash_WinProgressBar"),
    splash_test("CSplash::InterpolateSplash", "InterpolateSplash__7CSplashFv", "tpw_CSplash_InterpolateSplash"),
]


def dtor_test(name, label, host_fn, back_only):
    def ppc(emu, c):
        ppc_side(emu, c)
        if back_only:
            obj = ppc_back(emu, c["splash"][2])
            r3 = T.call_ppc(emu, emu.func(label), [obj, T.u32(T.s16(c["flag"]))])[0]
            out = ppc_observe(emu, None, r3 == obj, back_obj=obj)
        else:
            obj = ppc_splash(emu, c["splash"])
            r3 = T.call_ppc(emu, emu.func(label), [obj, T.u32(T.s16(c["flag"]))])[0]
            out = ppc_observe(emu, obj, r3 == obj)
        return out

    def host(lib, c):
        h = Host(lib, c)
        obj = h.back(c["splash"][2]) if back_only else h.splash(c["splash"])
        h.start()
        freed = F(lib, host_fn, I32, VP, ctypes.c_int16)(obj, T.s16(c["flag"]))
        out = h.observe(None, True, back_obj=obj) if back_only else h.observe(obj, True)
        if freed:
            out["allocs"] = out["allocs"] + [("delete", "back" if back_only else "self")]
        return out
    return FnTest(name, ppc, host, rnd=with_back if back_only else rcase)


def sinit_ppc(emu, c):
    ppc_side(emu, c)
    emu.write(SPLASH, c["junk"])
    emu.registered = []
    T.call_ppc(emu, emu.func("__sinit_tb_splash_cpp"), [])
    dt = emu.func("__dt__7CSplashFv")
    reg = [(obj == SPLASH, emu.u32(fn) == dt, rec - obj) for obj, fn, rec in emu.registered]
    v = struct.unpack(">iiiiIi", emu.read(SPLASH, 24))
    return dict(ints=list(v[:4]) + [v[5]], back=v[4], reg=reg)


def sinit_host(lib, c):
    g = F(lib, "tpw_splash_global", VP)()
    j = struct.unpack(">iiiiIi", c["junk"][:24])
    F(lib, "tpw_splash_set", None, VP, ctypes.POINTER(I32), ctypes.c_float, VP)(
        g, (I32 * 7)(j[0], j[1], j[2], j[3], j[5], 0, 0), 0.0, None)
    F(lib, "tpw_sinit_tb_splash", None)()
    v, scale, b = (I32 * 7)(), ctypes.c_float(), VP()
    F(lib, "tpw_splash_get", None, VP, ctypes.POINTER(I32), ctypes.POINTER(ctypes.c_float),
      ctypes.POINTER(VP))(g, v, ctypes.byref(scale), ctypes.byref(b))
    # C++ registers the destructor itself: the record follows the 0x38-byte object
    return dict(ints=[v[0], v[1], v[2], v[3], v[4]], back=b.value or 0, reg=[(True, True, 0x38)])


TESTS += [
    dtor_test("CDrawToBackBuffer::~CDrawToBackBuffer", "__dt__17CDrawToBackBufferFv", "tpw_CDrawToBackBuffer_dtor", True),
    dtor_test("CSplash::~CSplash", "__dt__7CSplashFv", "tpw_CSplash_dtor", False),
    FnTest("__sinit_tb_splash_cpp", sinit_ppc, sinit_host,
           rnd=lambda r: dict(rcase(r), junk=r.randbytes(0x38))),
]
