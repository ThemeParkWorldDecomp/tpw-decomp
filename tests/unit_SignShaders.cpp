// Test library for the ride sign's image filters and shaders (unit w84; libtpw_unit_SignShaders.dylib, tests/test_SignShaders.py):
//   src/game/rides/RideSign.cpp   erodebuffer_erode 0x100c8e40, make_gaussian_filter_2d 0x100c90d8, copy_offset_alpha 0x100c922c,
//                               dilate 0x100c9404, erode 0x100c9650, blur 0x100c98e4
//   src/game/rides/RideSign.cpp   PlasticShader::clear 0x100ca6a0, ~PlasticShader 0x100ca558, load_settings 0x100ca724, bevel 0x100ca95c,
//                               blur_with_mask 0x100cab6c, plastic_shade 0x100caf40, ShadowGlow::shadow_glow 0x100cb468,
//                               ~ShadowGlow 0x100cb7c0
//
// Real: the two sources above, memcpy and the C library (sin, cos, exp, pow, sqrt: the PowerPC side calls the same host functions
// through Python's math module). Fakes that record the same event on both sides: Bitmap::clear and realloc (with the real ones'
// effects: the engine's own tests cover them), Bitmap::descimate (fills the destination from the case's "Texel" queue), TPFile_Read (the
// words come from the "Word" queue, little-endian as in the file) and _USwapBlock32 (a no-op on the host, where the words are already
// right). Operator new[] and delete[] are the call log's, with the allocation observer recording every new[] (label and size) so the
// two sides' heap traffic is compared in order; a block's size is also kept to dump the bitmaps' buffers.
//
// Each operation has one entry point (tpw_w84_*): a description of numbers in, a dump of numbers out, the events in the call log.
// A bitmap in a description is: width, height, bytes per pixel, the buffer's length (-1: no buffer), then the buffer's bytes (the
// length may be more than width * height * bytes per pixel: spare bytes the functions read past the end).
#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <new>

#include "Bitmap.h"
#include "PlasticShader.h"
#include "rsse.h"

namespace {
struct BlockSize {
    const void* block;
    int64_t size;
};
BlockSize gBlocks[4096];
int32_t gBlockCount = 0;
void ObserveAllocation(std::size_t size, bool array, void* block);
}  // namespace

#define TPW_CALLLOG_QUEUE_VALUES 1024
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "FS_System.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {

// Every block handed out is remembered with its size (the bitmaps' buffers are dumped with their lengths); a new[] during a recorded
// call is an event (label from the next free one, size).
void ObserveAllocation(std::size_t size, bool array, void* block) {
    // Fresh memory is 0xa5 on both sides, so a read of a byte the function never wrote shows (the PowerPC side fills its blocks too).
    std::memset(block, 0xa5, size);
    int32_t slot = gBlockCount;
    for (int32_t i = 0; i < gBlockCount; ++i)
        if (gBlocks[i].block == block) slot = i;
    if (slot == gBlockCount && gBlockCount < 4096) ++gBlockCount;
    if (slot < 4096) gBlocks[slot] = {block, static_cast<int64_t>(size)};
    if (array && on()) {
        const int32_t label = newlab(block);
        ev("new[]", {label, static_cast<int64_t>(size)});
    }
}

int64_t SizeOf(const void* block) {
    for (int32_t i = 0; i < gBlockCount; ++i)
        if (gBlocks[i].block == block) return gBlocks[i].size;
    return -1;
}

const void* gSettingsBase;

}  // namespace

// ---- the fakes
void Bitmap::clear() {
    ev("Bitmap.clear", {lab(this)});
    mWidth = 0;
    mHeight = 0;
    mBytesPerPixel = 0;
    if (mData != nullptr) delete[] mData;
    mData = nullptr;
}

int32_t Bitmap::realloc(uint32_t width, uint32_t height, uint32_t bytesPerPixel) {
    ev("Bitmap.realloc", {lab(this), width, height, bytesPerPixel});
    if (width != mWidth || height != mHeight || bytesPerPixel != mBytesPerPixel || mData == nullptr) {
        if (mData != nullptr) delete[] mData;
        mData = new uint8_t[bytesPerPixel * width * height];
        mWidth = width;
        mHeight = height;
        mBytesPerPixel = bytesPerPixel;
    }
    std::memset(mData, 0, bytesPerPixel * width * height);
    return 1;
}

// The box filter is not in this unit (unit w83): the destination takes bytes from the case's queue.
int32_t Bitmap::descimate(Bitmap& dest) {
    ev("Bitmap.descimate", {lab(this), lab(&dest), mWidth, mHeight, mBytesPerPixel, dest.mWidth, dest.mHeight, dest.mBytesPerPixel});
    const uint32_t count = dest.mWidth * dest.mHeight * dest.mBytesPerPixel;
    for (uint32_t i = 0; i < count; ++i) dest.mData[i] = static_cast<uint8_t>(ret("Texel", 0));
    return 1;
}

uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {static_cast<const char*>(dest) - static_cast<const char*>(gSettingsBase), size, count, lab(file)});
    const uint32_t word = static_cast<uint32_t>(ret("Word", 0));
    std::memcpy(dest, &word, 4);
    return 4;
}

namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) {
    ev("USwapBlock32", {reinterpret_cast<const char*>(words) - static_cast<const char*>(gSettingsBase), count});
}
}  // namespace SamsUtilities

namespace {
int64_t DoubleBits(double value);
}

// pow of the C library, recorded: the arguments are what plastic_shade's single-precision arithmetic (several fused multiply-adds)
// delivers, so a different rounding upstream shows here. The unit library's own definition is the one its code binds to; the result
// is the host's libm's.
extern "C" double pow(double x, double y) {
    ev("pow", {DoubleBits(x), DoubleBits(y)});
    using PowFn = double (*)(double, double);
    static const PowFn real = reinterpret_cast<PowFn>(dlsym(RTLD_NEXT, "pow"));
    return real(x, y);
}

namespace {

// ---------------------------------------------------------------------------------------------------- description and dump

struct Reader {
    const int64_t* p;
    int64_t next() { return *p++; }
};

// The labels: a bitmap k is k, its buffer 10 + k; the shader is 20, the glow 21, the settings file 22.
constexpr int32_t kShaderLabel = 20, kGlowLabel = 21, kFileLabel = 22;

void ReadBitmap(Reader& in, Bitmap& bitmap, int32_t label) {
    bitmap.mWidth = static_cast<uint32_t>(in.next());
    bitmap.mHeight = static_cast<uint32_t>(in.next());
    bitmap.mBytesPerPixel = static_cast<uint32_t>(in.next());
    const int64_t length = in.next();
    bitmap.mData = nullptr;
    if (length >= 0) {
        bitmap.mData = new uint8_t[static_cast<std::size_t>(length)];
        for (int64_t i = 0; i < length; ++i) bitmap.mData[i] = static_cast<uint8_t>(in.next());
        tpw_cl_label(10 + label, bitmap.mData);
    }
    tpw_cl_label(label, &bitmap);
}

void DumpBitmap(int64_t* out, int32_t& n, const Bitmap& bitmap) {
    out[n++] = bitmap.mWidth;
    out[n++] = bitmap.mHeight;
    out[n++] = bitmap.mBytesPerPixel;
    if (bitmap.mData == nullptr) {
        out[n++] = -1;
        return;
    }
    const int64_t length = SizeOf(bitmap.mData);
    out[n++] = length;
    for (int64_t i = 0; i < length; ++i) out[n++] = bitmap.mData[i];
}

// A double as its bits, a NaN as one canonical value (the sign and payload of a NaN are not reproduced by the emulator).
int64_t DoubleBits(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0x000fffffffffffffull) != 0) bits = 0x7ff8000000000000ull;
    return static_cast<int64_t>(bits);
}

double DoubleFromBits(int64_t bits) {
    double value;
    std::memcpy(&value, &bits, 8);
    return value;
}

float FloatFromBits(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

// The shader's eleven words (+0 .. +0x28), the Mac's offsets, then its four bitmaps.
void ReadShader(Reader& in, PlasticShader& shader) {
    uint32_t words[11];
    for (auto& word : words) word = static_cast<uint32_t>(in.next());
    static_assert(__builtin_offsetof(PlasticShader, mOffsetY) == 0x28, "the eleven words are the first 0x2c bytes");
    std::memcpy(static_cast<void*>(&shader), words, sizeof words);
    ReadBitmap(in, shader.mMask, 1);
    ReadBitmap(in, shader.mFile, 2);
    ReadBitmap(in, shader.mBevel, 3);
    ReadBitmap(in, shader.mShaded, 4);
    tpw_cl_label(kShaderLabel, &shader);
}

void DumpShaderWords(int64_t* out, int32_t& n, const PlasticShader& shader) {
    uint32_t words[11];
    std::memcpy(words, &shader, sizeof words);
    for (uint32_t word : words) out[n++] = word;
}

void DumpShader(int64_t* out, int32_t& n, const PlasticShader& shader) {
    DumpShaderWords(out, n, shader);
    DumpBitmap(out, n, shader.mMask);
    DumpBitmap(out, n, shader.mFile);
    DumpBitmap(out, n, shader.mBevel);
    DumpBitmap(out, n, shader.mShaded);
}

// ---------------------------------------------------------------------------------------------------- the filters

// description: value, work, mask, out. dump: work, mask, out.
int32_t RunErodeBuffer(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    const uint8_t value = static_cast<uint8_t>(in.next());
    Bitmap work, mask, result;
    ReadBitmap(in, work, 1);
    ReadBitmap(in, mask, 2);
    ReadBitmap(in, result, 3);
    tpw_cl_on = 1;
    erodebuffer_erode(work, mask, value, result);
    tpw_cl_on = 0;
    int32_t n = 0;
    DumpBitmap(out, n, work);
    DumpBitmap(out, n, mask);
    DumpBitmap(out, n, result);
    return n;
}

// description: size, sigma bits, spare entries. dump: the size * size + spare entries' bits (the spare ones start as -12345.0).
int32_t RunGaussian(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    const int32_t size = static_cast<int32_t>(d[0]);
    const double sigma = DoubleFromBits(d[1]);
    const int32_t spare = static_cast<int32_t>(d[2]);
    const int32_t count = (size > 0 ? size * size : 0) + spare;
    double* filter = new double[static_cast<std::size_t>(count) + 1];
    for (int32_t i = 0; i < count; ++i) filter[i] = -12345.0;
    tpw_cl_on = 1;
    make_gaussian_filter_2d(filter, size, sigma);
    tpw_cl_on = 0;
    for (int32_t i = 0; i < count; ++i) out[i] = DoubleBits(filter[i]);
    delete[] filter;
    return count;
}

// description: dx, dy, dest, src. dump: the answer, dest, src.
int32_t RunCopyOffset(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    const int32_t dx = static_cast<int32_t>(in.next());
    const int32_t dy = static_cast<int32_t>(in.next());
    Bitmap dest, src;
    ReadBitmap(in, dest, 1);
    ReadBitmap(in, src, 2);
    tpw_cl_on = 1;
    const int32_t answer = copy_offset_alpha(dest, src, dx, dy);
    tpw_cl_on = 0;
    int32_t n = 0;
    out[n++] = answer;
    DumpBitmap(out, n, dest);
    DumpBitmap(out, n, src);
    return n;
}

// description: radius, bitmap. dump: the answer, the bitmap.
int32_t RunMorph(const int64_t* d, int64_t* out, bool dilateNotErode) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    const int32_t radius = static_cast<int32_t>(in.next());
    Bitmap bitmap;
    ReadBitmap(in, bitmap, 1);
    tpw_cl_on = 1;
    const int32_t answer = dilateNotErode ? dilate(bitmap, radius) : erode(bitmap, radius);
    tpw_cl_on = 0;
    int32_t n = 0;
    out[n++] = answer;
    DumpBitmap(out, n, bitmap);
    return n;
}

// description: sigma bits, bitmap. dump: the answer, the bitmap.
int32_t RunBlur(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    const double sigma = DoubleFromBits(in.next());
    Bitmap bitmap;
    ReadBitmap(in, bitmap, 1);
    tpw_cl_on = 1;
    const int32_t answer = blur(bitmap, sigma);
    tpw_cl_on = 0;
    int32_t n = 0;
    out[n++] = answer;
    DumpBitmap(out, n, bitmap);
    return n;
}

// ---------------------------------------------------------------------------------------------------- PlasticShader

// description: the shader (eleven words, four bitmaps). dump: the shader.
int32_t RunShaderClear(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    PlasticShader* shader = new PlasticShader;
    ReadShader(in, *shader);
    tpw_cl_on = 1;
    shader->clear();
    tpw_cl_on = 0;
    int32_t n = 0;
    DumpShader(out, n, *shader);
    delete shader;
    return n;
}

// description: the flag (a 16-bit short on the Mac), the shader. dump: whether the object was freed, then (if not) the shader.
int32_t RunShaderDtor(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    const int16_t flag = static_cast<int16_t>(in.next());
    void* storage = ::operator new(sizeof(PlasticShader));
    PlasticShader* shader = new (storage) PlasticShader;
    ReadShader(in, *shader);
    int32_t n = 0;
    tpw_cl_on = 1;
    if (flag > 0) {
        delete shader;
        tpw_cl_on = 0;
        out[n++] = 1;
    } else {
        shader->~PlasticShader();
        tpw_cl_on = 0;
        out[n++] = 0;
        DumpShader(out, n, *shader);
        ::operator delete(storage);
    }
    return n;
}

// description: the shader. The words come from the "Word" queue. dump: the answer, the shader's eleven words, the bitmaps.
int32_t RunLoadSettings(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    PlasticShader* shader = new PlasticShader;
    ReadShader(in, *shader);
    static char fileObject;
    tpw_cl_label(kFileLabel, &fileObject);
    gSettingsBase = shader;
    tpw_cl_on = 1;
    const int32_t answer = shader->load_settings(reinterpret_cast<bfl::FileSystem::CFile*>(&fileObject));
    tpw_cl_on = 0;
    int32_t n = 0;
    out[n++] = answer;
    DumpShader(out, n, *shader);
    delete shader;
    return n;
}

// description: the shader, width, mask, out. dump: the shader, mask, out.
int32_t RunBevel(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    PlasticShader* shader = new PlasticShader;
    ReadShader(in, *shader);
    const uint32_t width = static_cast<uint32_t>(in.next());
    Bitmap mask, result;
    ReadBitmap(in, mask, 5);
    ReadBitmap(in, result, 6);
    tpw_cl_on = 1;
    shader->bevel(mask, width, result);
    tpw_cl_on = 0;
    int32_t n = 0;
    DumpShader(out, n, *shader);
    DumpBitmap(out, n, mask);
    DumpBitmap(out, n, result);
    delete shader;
    return n;
}

// description: the shader, sigma bits (a float), bitmap, mask. dump: the shader, bitmap, mask.
int32_t RunBlurWithMask(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    PlasticShader* shader = new PlasticShader;
    ReadShader(in, *shader);
    const float sigma = FloatFromBits(in.next());
    Bitmap bitmap, mask;
    ReadBitmap(in, bitmap, 5);
    ReadBitmap(in, mask, 6);
    tpw_cl_on = 1;
    shader->blur_with_mask(bitmap, mask, sigma);
    tpw_cl_on = 0;
    int32_t n = 0;
    DumpShader(out, n, *shader);
    DumpBitmap(out, n, bitmap);
    DumpBitmap(out, n, mask);
    delete shader;
    return n;
}

// description: the shader, the bevel argument. The tile bytes come from the "Texel" queue. dump: the shader, the bevel argument.
int32_t RunPlasticShade(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    PlasticShader* shader = new PlasticShader;
    ReadShader(in, *shader);
    Bitmap bevel;
    ReadBitmap(in, bevel, 5);
    tpw_cl_on = 1;
    shader->plastic_shade(bevel);
    tpw_cl_on = 0;
    int32_t n = 0;
    DumpShader(out, n, *shader);
    DumpBitmap(out, n, bevel);
    delete shader;
    return n;
}

// ---------------------------------------------------------------------------------------------------- ShadowGlow

// description: red, green, blue, alpha, grow, blur bits (a float), offset x, offset y, src, out. dump: the answer, the glow's five
// words, src, out.
int32_t RunShadowGlow(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    Reader in{d};
    ShadowGlow* glow = new ShadowGlow;
    glow->mRed = static_cast<uint8_t>(in.next());
    glow->mGreen = static_cast<uint8_t>(in.next());
    glow->mBlue = static_cast<uint8_t>(in.next());
    glow->mAlpha = static_cast<uint8_t>(in.next());
    glow->mGrow = static_cast<int32_t>(in.next());
    glow->mBlur = FloatFromBits(in.next());
    glow->mOffsetX = static_cast<int32_t>(in.next());
    glow->mOffsetY = static_cast<int32_t>(in.next());
    Bitmap src, result;
    ReadBitmap(in, src, 1);
    ReadBitmap(in, result, 2);
    tpw_cl_label(kGlowLabel, glow);
    tpw_cl_on = 1;
    const uint32_t answer = glow->shadow_glow(src, result);
    tpw_cl_on = 0;
    int32_t n = 0;
    out[n++] = answer;
    out[n++] = (static_cast<uint32_t>(glow->mRed) << 24) | (static_cast<uint32_t>(glow->mGreen) << 16) |
               (static_cast<uint32_t>(glow->mBlue) << 8) | glow->mAlpha;
    out[n++] = glow->mGrow;
    uint32_t blurBits;
    std::memcpy(&blurBits, &glow->mBlur, 4);
    out[n++] = blurBits;
    out[n++] = glow->mOffsetX;
    out[n++] = glow->mOffsetY;
    DumpBitmap(out, n, src);
    DumpBitmap(out, n, result);
    delete glow;
    return n;
}

// description: the flag, nothing else. dump: whether the object was freed.
int32_t RunGlowDtor(const int64_t* d, int64_t* out) {
    gBlockCount = 0;
    tpw_cl_on = 0;
    const int16_t flag = static_cast<int16_t>(d[0]);
    void* storage = ::operator new(sizeof(ShadowGlow));
    ShadowGlow* glow = new (storage) ShadowGlow;
    tpw_cl_label(kGlowLabel, glow);
    tpw_cl_on = 1;
    if (flag > 0) {
        delete glow;
        tpw_cl_on = 0;
        out[0] = 1;
    } else {
        glow->~ShadowGlow();
        tpw_cl_on = 0;
        out[0] = 0;
        ::operator delete(storage);
    }
    return 1;
}

}  // namespace

extern "C" {

int32_t tpw_w84_erode_buffer(const int64_t* d, int64_t* out) { return RunErodeBuffer(d, out); }
int32_t tpw_w84_gaussian(const int64_t* d, int64_t* out) { return RunGaussian(d, out); }
int32_t tpw_w84_copy_offset(const int64_t* d, int64_t* out) { return RunCopyOffset(d, out); }
int32_t tpw_w84_dilate(const int64_t* d, int64_t* out) { return RunMorph(d, out, true); }
int32_t tpw_w84_erode(const int64_t* d, int64_t* out) { return RunMorph(d, out, false); }
int32_t tpw_w84_blur(const int64_t* d, int64_t* out) { return RunBlur(d, out); }
int32_t tpw_w84_shader_clear(const int64_t* d, int64_t* out) { return RunShaderClear(d, out); }
int32_t tpw_w84_shader_dtor(const int64_t* d, int64_t* out) { return RunShaderDtor(d, out); }
int32_t tpw_w84_load_settings(const int64_t* d, int64_t* out) { return RunLoadSettings(d, out); }
int32_t tpw_w84_bevel(const int64_t* d, int64_t* out) { return RunBevel(d, out); }
int32_t tpw_w84_blur_with_mask(const int64_t* d, int64_t* out) { return RunBlurWithMask(d, out); }
int32_t tpw_w84_plastic_shade(const int64_t* d, int64_t* out) { return RunPlasticShade(d, out); }
int32_t tpw_w84_shadow_glow(const int64_t* d, int64_t* out) { return RunShadowGlow(d, out); }
int32_t tpw_w84_glow_dtor(const int64_t* d, int64_t* out) { return RunGlowDtor(d, out); }

}  // extern "C"
