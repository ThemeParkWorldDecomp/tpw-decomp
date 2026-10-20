// Unit v5906 (main app, file rsse.cpp): Postcard::save 0x100c0c20 (the card as a JPEG), SetWorldBoundingArea 0x100b9180 (a header's box
// copied to another and carried by the second's node matrix) and SetHeightfieldTexture 0x100b8080 (sets the texture and flags of one height
// field quad).
#include "Postcard.h"
#include <cstdint>
#include <cstring>
#include <new>
#include "TPConfig.h"
#include "TbGraphicFile.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "tb_resource_display.h"

#include <cstdio>
#include <cstdlib>

#include "HFieldTextures.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "lobby.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {
const char kModeWrite[] = "wb";                    // 0x1023ee55
const char kCantOpen[] = "can't open %s\n";        // 0x1023ee58
}  // namespace

// 0x100c0c20. The card (format 9, 4 bytes a pixel) is converted to 24 bit RGB (format 7) and written as a JPEG of quality 75. A file
// that does not open is reported on stderr and the program exits (the original's stderr is entry 2 of the C library's file table, +0xa8).
int32_t Postcard::save(char* path) {
    SamsUtilities::UPixels source(SamsUtilities::kPixels32ARGB, mCard.mPixels, mCard.mWidth, mCard.mHeight, -1, nullptr);
    SamsUtilities::UPixels dest(SamsUtilities::kPixels24RGB, mCard.mWidth, mCard.mHeight, nullptr);
    dest.Convert(source, false);
    uint8_t* data = static_cast<uint8_t*>(dest.GetData());

    jpeg_error_mgr errorManager;
    jpeg_compress_struct cinfo;
    cinfo.mErr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(jpeg_std_error(&errorManager)));
    jpeg_CreateCompress(&cinfo, 0x3e, 0x174);
    std::FILE* file = std::fopen(path, kModeWrite);
    if (file == nullptr) {
        std::fprintf(stderr, kCantOpen, path);
        std::exit(1);
    }
    jpeg_stdio_dest(&cinfo, file);
    cinfo.mImageWidth = mCard.mWidth;
    cinfo.mImageHeight = mCard.mHeight;
    cinfo.mInputComponents = 3;
    cinfo.mInColorSpace = 2;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 0x4b, 1);
    jpeg_start_compress(&cinfo, 1);
    uint32_t stride = mCard.mWidth * 3;
    while (cinfo.mNextScanline < cinfo.mImageHeight) {
        uint8_t* row = data + static_cast<uint32_t>(cinfo.mNextScanline * stride);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    std::fclose(file);
    jpeg_destroy_compress(&cinfo);
    return 1;
}

// 0x100b9180: b's box takes a's; then each of its two corners (3 floats at 0x80 and 0x8c) is carried by the matrix at +0x10 of b's node
// (mF78): row 0 at +0, row 1 at +0x10, row 2 at +0x20, translation at +0x30.
static void CarryCorner(const float* m, float* p) {
    float x = p[0], y = p[1], z = p[2];
    float a = y * m[4];
    float b = y * m[5];
    float c = y * m[6];
    a = ppc_fmadds(x, m[0], a);
    b = ppc_fmadds(x, m[1], b);
    c = ppc_fmadds(x, m[2], c);
    a = ppc_fmadds(z, m[8], a);
    b = ppc_fmadds(z, m[9], b);
    c = ppc_fmadds(z, m[10], c);
    a = m[12] + a;
    b = m[13] + b;
    c = m[14] + c;
    p[0] = a;
    p[1] = b;
    p[2] = c;
}

void SetWorldBoundingArea(tag_sM3D2Header* a, tag_sM3D2Header* b) {
    uint8_t* pa = reinterpret_cast<uint8_t*>(a);
    uint8_t* pb = reinterpret_cast<uint8_t*>(b);
    std::memmove(pb + 0x80, pa + 0x80, 0x18);   // (a and b may be the same header)
    const float* m = reinterpret_cast<const float*>(reinterpret_cast<uint8_t*>(b->mF78.get()) + 0x10);
    CarryCorner(m, reinterpret_cast<float*>(pb + 0x80));
    m = reinterpret_cast<const float*>(reinterpret_cast<uint8_t*>(b->mF78.get()) + 0x10);
    CarryCorner(m, reinterpret_cast<float*>(pb + 0x8c));
}

// 0x100b8080: the quad (x, y) of the header's height field. With category 0 and texture 8 the quad is made plain (bits 0x101, not 0x80);
// with category 0 and texture 0x37 it takes the saved copy's words (nothing when there is none); otherwise the texture is looked up in
// the table (category and texture wrapped into range) and the rotation `rotation` (0x5a, 0xb4, 0x10e) sets bit 0x20, 0x10 or 8.
// The category wrap divides by the table's count and the texture wrap by the entry's: a count of 0 leaves the texture as it is (the
// emulator's divw); a negative category reads before the table (kept).
void SetHeightfieldTexture(tag_sM3D2Header* header, sExtraTextureTable* table, int32_t x, int32_t y, int32_t category, int32_t texture,
                           int32_t rotation) {
    bool plain = category == 0 && texture == 8;
    bool copy = category == 0 && texture == 0x37;
    if (!plain && !copy) {
        if (category == 0) category = 1;
        int32_t count = table->mCount;
        if (count <= 0) return;
        if (table->mEntries[category - 1].mCount <= 0) return;
        int32_t wrapped = category - 1;
        category = (wrapped - (wrapped / count) * count) + 1;
        int32_t perEntry = table->mEntries[category - 1].mCount;
        texture = texture - (perEntry != 0 ? (texture / perEntry) * perEntry : 0);
    }
    if (x < 0) return;
    sHField* field = header->mHField.get();
    int32_t width = static_cast<int32_t>(field->mF18[0]);
    if (x >= width) return;
    if (y < 0) return;
    if (y >= static_cast<int32_t>(field->mF18[1])) return;
    int32_t index = x + y * width;
    sHFieldQuad* quad = field->mQuads.get() + index;
    if (plain) {
        quad->mF00 = static_cast<uint16_t>(quad->mF00 | 0x101);
        quad = header->mHField->mQuads.get() + index;
        quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff7f);
    } else if (copy) {
        if (gHeightfieldCopy == nullptr) return;
        const sHFieldQuad* saved = gHeightfieldCopy->mQuads.get() + index;
        quad->mF02 = saved->mF02;
        quad = header->mHField->mQuads.get() + index;
        quad->mF00 = saved->mF00;
        quad->mF00 = static_cast<uint16_t>(quad->mF00 | 0x100);
        if ((gGameOptions & 0x4000) != 0) quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff87);
    } else {
        quad->mF02 = table->mEntries[category - 1].mValues[texture].mTextureIndex;
        uint16_t* word = &(header->mHField->mQuads.get() + index)->mF00;
        *word = static_cast<uint16_t>(*word & 0xff87);
        *word = static_cast<uint16_t>(*word & 0xfffe);
        *word = static_cast<uint16_t>(*word | 0x180);
        if (rotation == 0xb4) *word = static_cast<uint16_t>(*word | 0x10);
        else if (rotation == 0x5a) *word = static_cast<uint16_t>(*word | 0x20);
        else if (rotation == 0x10e) *word = static_cast<uint16_t>(*word | 8);
    }
}
