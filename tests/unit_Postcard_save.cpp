// Test library for Postcard::save, SetWorldBoundingArea and SetHeightfieldTexture (unit v5906; libtpw_unit_Postcard_save.dylib,
// tests/test_Postcard_save.py), main app:
//   src/game/MeshLoad.cpp, src/game/ui/Postcard.cpp
//
// The callees outside the unit are recording fakes: SamsUtilities::UPixels (constructors, Convert, GetData, destructor), the libjpeg
// compressor calls, fopen and fclose. The fopen fake always opens (the failing path calls exit, which cannot be compared). The globals
// of other files (gHeightfieldCopy, gGameOptions) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Postcard.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/upixels.h"
#include "HFieldTextures.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "lobby.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
sHField* gHeightfieldCopy;
uint32_t gGameOptions;

namespace {

uint32_t gCardPix32;   // the card's pixels (32-bit address), set by the test
uint32_t gPix32;       // the buffer GetData answers
const void* gUp1;
const void* gUp2;

int64_t Bits(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s != 0; ++s) h = h * 31u + static_cast<uint32_t>(static_cast<unsigned char>(*s));
    return h;
}

int64_t WhichUp(const void* p) { return p == gUp2 ? 2 : (p == gUp1 ? 1 : 0); }

}  // namespace

// ---- the fakes
namespace SamsUtilities {

UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    gUp1 = this;
    ev("UPixels6", {static_cast<int64_t>(format), static_cast<int64_t>(Bits(data) == gCardPix32), width, height, stride, palette != nullptr});
}

UPixels::UPixels(intptr_t format, int32_t width, int32_t height, UPixel* palette) {
    gUp2 = this;
    ev("UPixels4", {static_cast<int64_t>(format), width, height, palette != nullptr});
}

void UPixels::Convert(UPixels& source, bool dither) {
    ev("Convert", {static_cast<int64_t>(this == gUp2), static_cast<int64_t>(&source == gUp1), static_cast<int64_t>(dither)});
}

void* UPixels::GetData() {
    ev("GetData", {static_cast<int64_t>(this == gUp2)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(gPix32));
}

UPixels::~UPixels() { ev("dtor", {WhichUp(this), -1}); }

}  // namespace SamsUtilities

jpeg_error_mgr* jpeg_std_error(jpeg_error_mgr*) {
    ev("StdError", {});
    return reinterpret_cast<jpeg_error_mgr*>(static_cast<uintptr_t>(0x6000));
}

void jpeg_CreateCompress(jpeg_compress_struct* cinfo, int32_t version, uint32_t size) {
    ev("Create", {static_cast<int64_t>(cinfo->mErr), version, static_cast<int64_t>(size)});
}

void jpeg_stdio_dest(jpeg_compress_struct*, void* file) { ev("stdio_dest", {Bits(file)}); }

void jpeg_set_defaults(jpeg_compress_struct* c) {
    ev("set_defaults", {static_cast<int64_t>(c->mImageWidth), static_cast<int64_t>(c->mImageHeight), c->mInputComponents, c->mInColorSpace});
}

void jpeg_set_quality(jpeg_compress_struct*, int32_t quality, int32_t force) { ev("set_quality", {quality, force}); }

void jpeg_start_compress(jpeg_compress_struct* c, int32_t all) {
    ev("start", {all, static_cast<int64_t>(c->mImageWidth), static_cast<int64_t>(c->mImageHeight), c->mInputComponents, c->mInColorSpace});
    c->mNextScanline = 0;
}

uint32_t jpeg_write_scanlines(jpeg_compress_struct* c, uint8_t** rows, uint32_t count) {
    ev("write", {static_cast<int64_t>(static_cast<uint32_t>(Bits(*rows) - gPix32)), static_cast<int64_t>(count)});
    c->mNextScanline += 1;
    return 1;
}

void jpeg_finish_compress(jpeg_compress_struct*) { ev("finish", {}); }

void jpeg_destroy_compress(jpeg_compress_struct*) { ev("destroy", {}); }

extern "C" {

std::FILE* fopen(const char* path, const char* mode) {
    ev("fopen", {Hash(path), Hash(mode)});
    return reinterpret_cast<std::FILE*>(static_cast<uintptr_t>(0x7777));
}

int fclose(std::FILE* file) {
    ev("fclose", {Bits(file)});
    return 0;
}

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_pc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"copy", &gHeightfieldCopy},
        {"options", &gGameOptions},
        {"cardpix", &gCardPix32},
        {"pix", &gPix32},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_pc_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The host's offsets and sizes (HOST_LAYOUT): the card's width, height and pixels in Postcard; the table entry's stride, count and values;
// the table's entries; the texture's stride and index; the header's fields the module writes as Mac offsets.
int32_t tpw_pc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Postcard, mCard) + offsetof(PostcardBitmap, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(Postcard, mCard) + offsetof(PostcardBitmap, mHeight));
    out[n++] = static_cast<int64_t>(offsetof(Postcard, mCard) + offsetof(PostcardBitmap, mPixels));
    out[n++] = static_cast<int64_t>(sizeof(sExtraTextureEntry));
    out[n++] = static_cast<int64_t>(offsetof(sExtraTextureEntry, mCount));
    out[n++] = static_cast<int64_t>(offsetof(sExtraTextureEntry, mValues));
    out[n++] = static_cast<int64_t>(offsetof(sExtraTextureTable, mEntries));
    out[n++] = static_cast<int64_t>(sizeof(sExtraTexture));
    out[n++] = static_cast<int64_t>(offsetof(sExtraTexture, mTextureIndex));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mBox));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mF18));
    out[n++] = static_cast<int64_t>(offsetof(sHField, mQuads));
    out[n++] = static_cast<int64_t>(sizeof(sHFieldQuad));
    return n;
}

// ---- the functions
int32_t tpw_pc_save(uint64_t card, uint64_t path) {
    return reinterpret_cast<Postcard*>(static_cast<uintptr_t>(card))->save(reinterpret_cast<char*>(static_cast<uintptr_t>(path)));
}

void tpw_pc_bound(uint64_t a, uint64_t b) {
    SetWorldBoundingArea(reinterpret_cast<tag_sM3D2Header*>(static_cast<uintptr_t>(a)), reinterpret_cast<tag_sM3D2Header*>(static_cast<uintptr_t>(b)));
}

void tpw_pc_tex(uint64_t header, uint64_t table, int32_t x, int32_t y, int32_t category, int32_t texture, int32_t rotation) {
    SetHeightfieldTexture(reinterpret_cast<tag_sM3D2Header*>(static_cast<uintptr_t>(header)),
                          reinterpret_cast<sExtraTextureTable*>(static_cast<uintptr_t>(table)), x, y, category, texture, rotation);
}

}  // extern "C"
