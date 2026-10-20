// Unit w82. Real SignRenderer, Bitmap clear/realloc/load and bullfrog strings.
// Files are scripted; rendering callees record every argument and bitmap word.
#include "SignRenderer.h"
#include "FS_System.h"
#include "TPGlobals.h"
#include "calllog.h"
#include <algorithm>
#include <cstring>
#include <new>

using namespace calllog;
namespace {
bool gObserve;
void Observe(std::size_t size, bool array, void* block) {
    if (gObserve && array && on()) ev("new[]", {static_cast<int64_t>(size), newlab(block)});
}
}
#define TPW_ALLOCATION_OBSERVER Observe
#include "calllog.cpp"

template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;

namespace {
class WaveletFake : public CWavelet {
public:
    int32_t Decompress(uint32_t, uint32_t, bool, const uint8_t*, uint8_t*, float, float, float) override { return 0; }
    int32_t Decompress(int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) override { return 0; }
    int32_t Decompress(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) override { return 0; }
    int32_t AddRef() override { return 0; }
    int32_t GetRefCount() override { return 0; }
    int32_t Release() override { ev("Release", {lab(this)}); return 0; }
} gWavelet;
SignRenderer* gRenderer;
const uint8_t* gBytes;
uint32_t gSize, gPosition;
int32_t gOpen;
uint8_t gFile;
uint8_t gDC;

// Local bitmaps receive labels at their first use, distinct from allocation labels.
int32_t BitmapLabel(const Bitmap* bitmap) {
    if (lab(bitmap) == -1) newlab(bitmap);
    return lab(bitmap);
}
void RecordBitmap(const char* name, const Bitmap& bitmap) {
    ev(name, {BitmapLabel(&bitmap), bitmap.mWidth, bitmap.mHeight, bitmap.mBytesPerPixel, lab(bitmap.mData)});
}
uint32_t ReadBytes(void* dest, uint32_t size, uint32_t count, void* file) {
    ev("Read", {size, count, lab(file), gPosition});
    const uint32_t n = std::min(size * count, gSize - gPosition);
    if (n) std::memcpy(dest, gBytes + gPosition, n);
    gPosition += n;
    return size ? n / size : 0;
}
}
CWavelet::~CWavelet() {}
CWavelet* CWavelet::Instance() { ev("Instance"); return &gWavelet; }
TbFileReadFn gFileRead = ReadBytes;
bfl::FileSystem::CFile* TPFile_Open(const char* path, const char* mode) {
    ev("Open", {}, {path, mode});
    return gOpen ? reinterpret_cast<bfl::FileSystem::CFile*>(&gFile) : nullptr;
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    return ReadBytes(dest, size, count, file);
}
namespace SamsUtilities {
void _USwapBlock32(uint32_t*, int32_t) {}
}
TrueTypeRender::TrueTypeRender() {
    ev("Font.ctor", {lab(this)});
    RecordBitmap("Ctor.background", gRenderer->mBackground);
    RecordBitmap("Ctor.output", gRenderer->mOutput);
    std::memset(static_cast<void*>(this), 0x7b, sizeof *this);
}
int32_t TrueTypeRender::load_settings(bfl::FileSystem::CFile* file) {
    ev("Font.load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("Font.load", 0));
}
int32_t TrueTypeRender::render_font_unicode(NS_MacDoze::InternalHandle* dc, const char16_t* text, Bitmap& bitmap) {
    ev("Font.render", {lab(this), lab(dc), BitmapLabel(&bitmap)}, {narrow(text).c_str()});
    for (int i = 0; text[i]; ++i) ev("Font.text", {text[i]});
    RecordBitmap("Font.bitmap", bitmap);
    return static_cast<int32_t>(ret("Font.render", 0));
}
PlasticShader::~PlasticShader() { ev("Shader.dtor", {lab(this), 1}); }
int32_t PlasticShader::load_settings(bfl::FileSystem::CFile* file) {
    ev("Shader.load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("Shader.load", 0));
}
void PlasticShader::bevel(Bitmap& mask, uint32_t width, Bitmap& bevelBitmap) {
    ev("Bevel", {lab(this), BitmapLabel(&mask), width, BitmapLabel(&bevelBitmap)});
    RecordBitmap("Bevel.mask", mask);
    RecordBitmap("Bevel.output", bevelBitmap);
}
void PlasticShader::blur_with_mask(Bitmap& bevelBitmap, Bitmap& mask, float blur) {
    ev("Blur", {lab(this), BitmapLabel(&bevelBitmap), BitmapLabel(&mask), fbits(blur)});
    RecordBitmap("Blur.bevel", bevelBitmap);
    RecordBitmap("Blur.mask", mask);
}
void PlasticShader::plastic_shade(Bitmap& bitmap) {
    ev("Shade", {lab(this), BitmapLabel(&bitmap)});
    RecordBitmap("Shade.bitmap", bitmap);
}
uint32_t ShadowGlow::shadow_glow(const Bitmap& mask, Bitmap& dest) const {
    ev("Glow", {lab(this), BitmapLabel(&mask), BitmapLabel(&dest)});
    RecordBitmap("Glow.mask", mask);
    RecordBitmap("Glow.output", dest);
    return 0;
}
ShadowGlow::~ShadowGlow() {}
TrueTypeRender::~TrueTypeRender() {}
void Bitmap::colour_fill(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    ev("Fill", {BitmapLabel(this), a, r, g, b});
    RecordBitmap("Fill.bitmap", *this);
    // A deterministic output pattern makes both copy loops observable.
    if (this == &gRenderer->mOutput)
        for (uint32_t i = 0; i < mWidth * mHeight * mBytesPerPixel; ++i) mData[i] = static_cast<uint8_t>(i * 37 + ret("PixelSeed", 0));
}
void Bitmap::copy_to(Bitmap& dest) {
    ev("Copy", {BitmapLabel(this), BitmapLabel(&dest)});
    RecordBitmap("Copy.source", *this);
    RecordBitmap("Copy.dest", dest);
}
void Bitmap::shadowblend(Bitmap& dest) {
    ev("Blend", {BitmapLabel(this), BitmapLabel(&dest)});
    RecordBitmap("Blend.source", *this);
    RecordBitmap("Blend.dest", dest);
}
void Bitmap::colourblt(Bitmap& dest, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    ev("Colour", {BitmapLabel(this), BitmapLabel(&dest), a, r, g, b});
    RecordBitmap("Colour.source", *this);
    RecordBitmap("Colour.dest", dest);
}
void Bitmap::alphablt(Bitmap& dest, uint32_t x, uint32_t y) {
    ev("Alpha", {BitmapLabel(this), BitmapLabel(&dest), x, y});
    RecordBitmap("Alpha.source", *this);
    RecordBitmap("Alpha.dest", dest);
}
void Bitmap::swizzle_for_gimex() {
    ev("Swizzle", {BitmapLabel(this)});
    RecordBitmap("Swizzle.bitmap", *this);
}
int32_t Bitmap::load_wavelet(void* file, CWavelet* wavelet) {
    ev("Wavelet.load", {BitmapLabel(this), lab(file), lab(wavelet)});
    RecordBitmap("Wavelet.bitmap", *this);
    return static_cast<int32_t>(ret("Wavelet.load", 0));
}
namespace NS_MacDoze {
int32_t AddFontResource(const char* path) {
    ev("AddFont", {}, {path});
    return static_cast<int32_t>(ret("AddFont", 1));
}
}

namespace {
Bitmap* Bitmaps(SignRenderer* renderer, int index) {
    Bitmap* bitmaps[] = {&renderer->mBackground, &renderer->mOutput,
        &renderer->mShader1.mMask, &renderer->mShader1.mFile, &renderer->mShader1.mBevel, &renderer->mShader1.mShaded,
        &renderer->mShader2.mMask, &renderer->mShader2.mFile, &renderer->mShader2.mBevel, &renderer->mShader2.mShaded};
    return bitmaps[index];
}
void Label(SignRenderer* renderer) {
    tpw_cl_label(1, renderer);
    tpw_cl_label(2, &renderer->mFont1);
    tpw_cl_label(3, &renderer->mFont2);
    tpw_cl_label(4, &renderer->mShader1);
    tpw_cl_label(5, &renderer->mShader2);
    tpw_cl_label(6, &renderer->mGlow1);
    tpw_cl_label(7, &renderer->mGlow2);
    tpw_cl_label(8, &gWavelet);
    tpw_cl_label(9, &gFile);
    tpw_cl_label(10, &gDC);
    for (int i = 0; i < 10; ++i) tpw_cl_label(20 + i, Bitmaps(renderer, i));
}
// Dump every Mac field in a canonical order, skipping only host alignment padding.
int32_t Dump(SignRenderer* r, int64_t* out) {
    int n = 0;
    auto value = [&](uint32_t v) { out[n++] = v; };
    value(r->mVersion);
    value(r->mF004);
    value(r->mHasBackground);
    for (uint8_t v : r->mPadding009) value(v);
    value(r->mGlowMode1);
    value(r->mGlowMode2);
    value(lab(r->mWavelet));
    for (auto* font : {&r->mFont1, &r->mFont2}) {
        value(font->mLeft);
        value(font->mRight);
        for (int i = 8; i < 0x190; ++i) value(reinterpret_cast<uint8_t*>(font)[i]);
    }
    for (auto* shader : {&r->mShader1, &r->mShader2})
        for (int i = 0; i < 11; ++i) {
            uint32_t v;
            std::memcpy(&v, reinterpret_cast<uint8_t*>(shader) + i * 4, 4);
            value(v);
        }
    for (auto* glow : {&r->mGlow1, &r->mGlow2}) {
        value(glow->mRed);
        value(glow->mGreen);
        value(glow->mBlue);
        value(glow->mAlpha);
        for (int i = 1; i < 5; ++i) {
            uint32_t v;
            std::memcpy(&v, reinterpret_cast<uint8_t*>(glow) + i * 4, 4);
            value(v);
        }
    }
    for (int i = 0; i < 10; ++i) {
        Bitmap& bm = *Bitmaps(r, i);
        value(bm.mWidth);
        value(bm.mHeight);
        value(bm.mBytesPerPixel);
        value(lab(bm.mData));
        if (bm.mData)
            for (uint32_t j = 0; j < bm.mWidth * bm.mHeight * bm.mBytesPerPixel; ++j) value(bm.mData[j]);
    }
    return n;
}
}

// d: fill, order, background, modes 1/2, w/h/quality, NULL bits, file flags 1/2,
// blur bits 1/2, font extents, 8 glow colour bytes, dtor flag, open, version, reuse bits.
extern "C" int32_t tpw_srr_run(int32_t op, const int64_t* d, const char* path, const char* name1,
    const char* name2, const char* separator, const uint8_t* bytes, uint32_t size, int64_t* out) {
    tpw_cl_on = 0;
    alignas(SignRenderer) uint8_t storage[sizeof(SignRenderer)];
    std::memset(storage, static_cast<int>(d[0]), sizeof storage);
    auto* r = reinterpret_cast<SignRenderer*>(storage);
    gRenderer = r;
    Label(r);
    gBytes = bytes;
    gSize = size;
    gPosition = 0;
    gOpen = static_cast<int32_t>(d[26]);
    gTPGlobals.mSeparator = separator;
    if (op != 0) {
        for (int i = 0; i < 10; ++i) new (Bitmaps(r, i)) Bitmap;
        r->mVersion = static_cast<int32_t>(d[27]);
        r->mF004 = static_cast<int32_t>(d[1]);
        r->mHasBackground = static_cast<uint8_t>(d[2]);
        r->mGlowMode1 = static_cast<int32_t>(d[3]);
        r->mGlowMode2 = static_cast<int32_t>(d[4]);
        r->mWavelet = &gWavelet;
        uint32_t blur1 = static_cast<uint32_t>(d[11]), blur2 = static_cast<uint32_t>(d[12]);
        std::memcpy(&r->mShader1.mBlur, &blur1, 4);
        std::memcpy(&r->mShader2.mBlur, &blur2, 4);
        r->mFont1.mLeft = static_cast<int32_t>(d[13]);
        r->mFont1.mRight = static_cast<int32_t>(d[14]);
        r->mFont2.mLeft = static_cast<int32_t>(d[15]);
        r->mFont2.mRight = static_cast<int32_t>(d[16]);
        for (int i = 0; i < 4; ++i) {
            reinterpret_cast<uint8_t*>(&r->mGlow1)[i] = static_cast<uint8_t>(d[17 + i]);
            reinterpret_cast<uint8_t*>(&r->mGlow2)[i] = static_cast<uint8_t>(d[21 + i]);
        }
        if (op == 3) {
            std::strcpy(r->mFont1.mName, name1);
            std::strcpy(r->mFont2.mName, name2);
        }
        for (int i = 0; i < 10; ++i) {
            Bitmap& bm = *Bitmaps(r, i);
            bool file = (i == 3 && d[9]) || (i == 7 && d[10]);
            if (file || ((d[28] >> i) & 1)) {
                bm.mWidth = static_cast<uint32_t>(d[5]) * 2;
                bm.mHeight = static_cast<uint32_t>(d[6]) * 2;
                bm.mBytesPerPixel = (i == 1 || i == 5 || i == 9) ? 4 : 1;
                bm.mData = new uint8_t[bm.mWidth * bm.mHeight * bm.mBytesPerPixel];
                std::memset(bm.mData, 0x5a, bm.mWidth * bm.mHeight * bm.mBytesPerPixel);
                tpw_cl_label(50 + i, bm.mData);
            }
        }
    }
    uint8_t texture1[1024], texture2[1024];
    std::memset(texture1, 0xa5, sizeof texture1);
    std::memset(texture2, 0xa5, sizeof texture2);
    char16_t text1[] = u"First \u00e9", text2[] = u"Second \u03b1";
    sign_renderer_options options{text1, text2, texture1, texture2,
        static_cast<int32_t>(d[5]), static_cast<int32_t>(d[6]), static_cast<int32_t>(d[7])};
    if (d[8] & 1) options.mText1 = nullptr;
    if (d[8] & 2) options.mText2 = nullptr;
    if (d[8] & 4) options.mTexture1 = nullptr;
    if (d[8] & 8) options.mTexture2 = nullptr;
    gObserve = op != 3;
    tpw_cl_on = 1;
    int32_t result = 1;
    if (op == 0) new (r) SignRenderer;
    if (op == 1) {
        r->~SignRenderer();
        // The deleting form is checked as a signed short without freeing stack storage.
        if (static_cast<int16_t>(d[25]) > 0) ev("delete", {lab(r)});
    }
    if (op == 2) result = r->sign_load_settings(path);
    if (op == 3) result = r->sign_install_fonts(path);
    if (op == 4) result = r->sign_render(reinterpret_cast<NS_MacDoze::InternalHandle*>(&gDC), options);
    tpw_cl_on = 0;
    gObserve = false;
    int n = 0;
    out[n++] = result;
    out[n++] = gPosition;
    n += Dump(r, out + n);
    for (int i = 0; i < 1024; ++i) out[n++] = texture1[i];
    for (int i = 0; i < 1024; ++i) out[n++] = texture2[i];
    // Buffers not consumed by the operation are cleaned with recording disabled.
    for (int i = 0; i < 10; ++i) {
        Bitmap* bm = Bitmaps(r, i);
        if (bm->mData) delete[] bm->mData;
    }
    return n;
}
