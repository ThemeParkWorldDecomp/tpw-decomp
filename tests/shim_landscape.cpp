// extern "C" wrappers for tests/test_landscape.py (ctypes): engine landscape.cpp, CPU, the
// Bitmap methods landscape uses (clear, realloc, load, the destructor) and
// SetFileCallbacks.
//
// Objects live in static storage here (one Landscape, one Tileset, two Bitmaps, one CPU);
// the test reads and writes their fields through accessors, because the host layout
// differs from the PowerPC one wherever a pointer is involved. Pixel buffers the code may
// delete[] are allocated with new[] (tpw_new_bytes).
#include <cstddef>
#include <cstring>
#include <new>

#include "Bitmap.h"
#include "CPU.h"
#include "Wavelet.h"
#include "landscape.h"

// The scalar heads have the PowerPC offsets on the host too; the test relies on that.
static_assert(offsetof(Landscape, mRampC) == 0x02 && offsetof(Landscape, mRampCover1) == 0x0a, "head");
static_assert(offsetof(Landscape, mWidthTiles) == 0x0c && offsetof(Landscape, mHeightTiles) == 0x10, "head");
static_assert(offsetof(Tileset, mTileSize) == 0 && offsetof(Tileset, mLevel) == 4, "tileset head");

namespace {

alignas(Landscape) unsigned char gLandscapeStore[sizeof(Landscape)];
alignas(Tileset) unsigned char gTilesetStore[sizeof(Tileset)];
alignas(Bitmap) unsigned char gBitmapStore[2][sizeof(Bitmap)];
alignas(CPU) unsigned char gCPUStore[sizeof(CPU)];

// Fake file for the engine's file callbacks (same semantics as tpwtest.FakeFile).
struct FakeFile {
    const uint8_t* data;
    uint32_t size;
    uint32_t pos;
};
FakeFile gFake;

}  // namespace

extern "C" {

int32_t tpw_file_fail_open = 0;
// Call log: kind (1 open, 2 close, 3 read, 4 seek), a, b, handle ok.
int32_t tpw_file_log_n = 0;
uint32_t tpw_file_log[2048][4];
char tpw_file_open_name[256];
char tpw_file_open_mode[16];

}  // extern "C"

namespace {

void file_log(uint32_t kind, uint32_t a, uint32_t b, bool ok) {
    if (tpw_file_log_n >= 2048) return;
    uint32_t* e = tpw_file_log[tpw_file_log_n++];
    e[0] = kind;
    e[1] = a;
    e[2] = b;
    e[3] = ok ? 1 : 0;
}

void* fake_open(const char* name, const char* mode) {
    std::strncpy(tpw_file_open_name, name, sizeof(tpw_file_open_name) - 1);
    std::strncpy(tpw_file_open_mode, mode, sizeof(tpw_file_open_mode) - 1);
    file_log(1, 0, 0, true);
    return tpw_file_fail_open != 0 ? nullptr : &gFake;
}

uint32_t fake_close(void* file) {
    file_log(2, 0, 0, file == &gFake);
    return 0;
}

uint32_t fake_read(void* dest, uint32_t size, uint32_t count, void* file) {
    file_log(3, size, count, file == &gFake);
    uint32_t n = size * count;
    uint32_t avail = gFake.pos < gFake.size ? gFake.size - gFake.pos : 0;
    uint32_t k = n < avail ? n : avail;
    uint8_t* d = static_cast<uint8_t*>(dest);
    if (k != 0) std::memcpy(d, gFake.data + gFake.pos, k);
    if (n > k) std::memset(d + k, 0, n - k);
    gFake.pos += n;
    return count;
}

uint32_t fake_seek(void* file, uint32_t offset, uint32_t whence) {
    file_log(4, offset, whence, file == &gFake);
    if (whence == 1) gFake.pos += offset;
    else if (whence == 0) gFake.pos = offset;
    return 0;
}

}  // namespace

extern "C" {

// ---- fake file
void tpw_file_set(const uint8_t* data, uint32_t size) {
    gFake.data = data;
    gFake.size = size;
    gFake.pos = 0;
    tpw_file_log_n = 0;
    tpw_file_open_name[0] = 0;
    tpw_file_open_mode[0] = 0;
}
void* tpw_file_handle() { return &gFake; }
void tpw_file_install() { SetFileCallbacks(fake_open, fake_close, fake_read, fake_seek, nullptr, nullptr); }

// ---- buffers and objects
uint8_t* tpw_new_bytes(uint32_t n, const uint8_t* init) {
    uint8_t* p = new uint8_t[n != 0 ? n : 1];
    if (init != nullptr) std::memcpy(p, init, n);
    return p;
}
void tpw_free_bytes(uint8_t* p) { delete[] p; }

void* tpw_landscape_store() { return gLandscapeStore; }
void* tpw_tileset_store() { return gTilesetStore; }
void* tpw_bitmap_store(int32_t i) { return gBitmapStore[i]; }
void* tpw_cpu_store() { return gCPUStore; }

Bitmap* tpw_landscape_control(Landscape* l) { return &l->mControl; }
Tileset* tpw_landscape_tileset(Landscape* l, int32_t i) { return &l->mTilesets[i]; }
Bitmap* tpw_tileset_bitmap(Tileset* t, int32_t k) { return &t->mBitmaps[k]; }

void tpw_bitmap_set(Bitmap* b, uint32_t w, uint32_t h, uint32_t bpp, uint8_t* data) {
    b->mWidth = w;
    b->mHeight = h;
    b->mBytesPerPixel = bpp;
    b->mData = data;
}
void tpw_bitmap_get(const Bitmap* b, uint32_t* whb, uint8_t** data) {
    whb[0] = b->mWidth;
    whb[1] = b->mHeight;
    whb[2] = b->mBytesPerPixel;
    *data = b->mData;
}

// ---- CPU
void tpw_CPU_detect(CPU* c) { c->detect(); }
int32_t tpw_CPU_clocks_per_millisec(CPU* c, int32_t unused) { return c->clocks_per_millisec(unused); }

// ---- free functions
int32_t tpw_resample_control(Bitmap* src, uint32_t tx, uint32_t ty, uint32_t channel, Bitmap* dst) {
    return resample_control(*src, tx, ty, channel, *dst);
}
void tpw_blend_3(uint8_t* a1, uint8_t* b1, uint8_t* m1, uint8_t* t1, uint8_t* a2, uint8_t* b2, uint8_t* m2,
                 uint8_t* t2, uint8_t* a3, uint8_t* b3, uint8_t* m3, uint8_t* t3, uint8_t* c, uint8_t* w12,
                 uint8_t* w23, uint8_t* out) {
    blend_3(a1, b1, m1, t1, a2, b2, m2, t2, a3, b3, m3, t3, c, w12, w23, out);
}
void tpw_blend_2(uint8_t* a1, uint8_t* b1, uint8_t* m1, uint8_t* t1, uint8_t* a2, uint8_t* b2, uint8_t* m2,
                 uint8_t* t2, uint8_t* c, uint8_t* w, uint8_t* out) {
    blend_2(a1, b1, m1, t1, a2, b2, m2, t2, c, w, out);
}
void tpw_single_layer(uint8_t* a, uint8_t* b, uint8_t* m, uint8_t* t, uint8_t* c, uint8_t* out) {
    single_layer(a, b, m, t, c, out);
}
uint8_t tpw_calc_cover(uint8_t* mapA, uint8_t* mapB, uint8_t* ramp, uint8_t* control, uint8_t* out) {
    return calc_cover(mapA, mapB, ramp, control, out);
}

// ---- Bitmap
void tpw_Bitmap_clear(Bitmap* b) { b->clear(); }
int32_t tpw_Bitmap_realloc(Bitmap* b, uint32_t w, uint32_t h, uint32_t bpp) { return b->realloc(w, h, bpp); }
int32_t tpw_Bitmap_load(Bitmap* b, void* file) { return b->load(file); }
// Destructor convention (as the pilot's Vector2d test): storage is freed when flag > 0;
// the return value says whether it would be.
int32_t tpw_Bitmap_dtor(Bitmap* b, int16_t flag) {
    b->~Bitmap();
    return flag > 0 ? 1 : 0;
}

// ---- Tileset
void tpw_Tileset_ctor(void* storage) { new (storage) Tileset(); }
int32_t tpw_Tileset_dtor(Tileset* t, int16_t flag) {
    t->~Tileset();
    return flag > 0 ? 1 : 0;
}
void tpw_Tileset_clear(Tileset* t) { t->clear(); }
int32_t tpw_Tileset_load(Tileset* t, void* file) { return t->load(file); }

// ---- Landscape
void tpw_Landscape_ctor(void* storage) { new (storage) Landscape(); }
int32_t tpw_Landscape_dtor(Landscape* l, int16_t flag) {
    l->~Landscape();
    return flag > 0 ? 1 : 0;
}
void tpw_Landscape_clear(Landscape* l) { l->clear(); }
void tpw_Landscape_set_tilesize(Landscape* l, int32_t size) { l->set_tilesize(size); }
void tpw_Landscape_precalc_tables(Landscape* l) { l->precalc_tables(); }
// overlay: has_overlay = 0 passes NULL; otherwise an sTileOverlay built from the fields.
void tpw_Landscape_generate_tile(Landscape* l, uint8_t* out, uint32_t tx, uint32_t ty, int32_t has_overlay,
                                 void* data, int32_t w, int32_t h, int32_t format, uint32_t flags) {
    sTileOverlay ov{data, w, h, format, flags};
    l->generate_tile(out, tx, ty, has_overlay != 0 ? &ov : nullptr);
}
int32_t tpw_Landscape_load_landscape(Landscape* l, char* name) { return l->load_landscape(name); }
void tpw_sinit_landscape() {
    new (&gControl1) Bitmap();
    new (&gControl3) Bitmap();
    new (&gControl2) Bitmap();
}

// ---- Wavelet.cpp
void tpw_SetFileCallbacks(uintptr_t open, uintptr_t close, uintptr_t read, uintptr_t seek, uintptr_t tell,
                          uintptr_t name) {
    SetFileCallbacks(reinterpret_cast<TbFileOpenFn>(open), reinterpret_cast<TbFileCloseFn>(close),
                     reinterpret_cast<TbFileReadFn>(read), reinterpret_cast<TbFileSeekFn>(seek),
                     reinterpret_cast<TbFileTellFn>(tell), reinterpret_cast<TbFileNameFn>(name));
}
void tpw_get_file_callbacks(uint64_t* out) {
    out[0] = reinterpret_cast<uintptr_t>(gFileOpen);
    out[1] = reinterpret_cast<uintptr_t>(gFileClose);
    out[2] = reinterpret_cast<uintptr_t>(gFileRead);
    out[3] = reinterpret_cast<uintptr_t>(gFileSeek);
    out[4] = reinterpret_cast<uintptr_t>(gFileTell);
    out[5] = reinterpret_cast<uintptr_t>(gFileName);
}

}  // extern "C"
