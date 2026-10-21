// landscape.cpp: name proven by __sinit_landscape_cpp in the engine library.

#include "CPU.h"
#include "landscape.h"
#include "platform/interfacelib.h"
#include "platform/samsutils.h"
#include "Wavelet.h"
#include <cstring>

// CPU.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x1000006c..0x1000014c; original file name unknown).

int32_t gCPUClockMHz;

void CPU::detect() {
    if (gCPUClockMHz == 0) {
        int32_t hz;
        if (Gestalt(0x70636c6b /* 'pclk' */, &hz) == 0) gCPUClockMHz = hz / 1000000;
    }
    BlockMoveData("FunkyPowerPC", mName, 13);
    mType = 6;
    mF08 = 1;
    mF04 = 0;
    mF0C = 0;
}

int32_t CPU::clocks_per_millisec(int32_t unused) {
    return gCPUClockMHz * 1000;
}

// landscape.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x1000017c..0x10002db4, plus Bitmap::~Bitmap's out-of-line copy, which is inline in
// Bitmap.h).
//
// Pixels are 4 bytes in memory order A, R, G, B; the original writes them byte by byte
// except the "no coverage" fill, a 32-bit store of 0x00ffffff that is the bytes 00 ff ff
// ff on the Mac, written here as bytes so the result does not depend on the host's byte
// order. Shifts of negative values are arithmetic (srawi), as in the original.

// Declaration order follows __sinit_landscape_cpp.
Bitmap gControl1;
Bitmap gControl3;
Bitmap gControl2;

uint8_t* gTileBuffer;
uint8_t gRampB[256];
uint8_t gRampA[256];
uint8_t gRampC[256];
uint8_t gRampCover2[256];
uint8_t gRampCover1[256];

int32_t resample_control(const Bitmap& src, uint32_t tx, uint32_t ty, uint32_t channel, Bitmap& dst) {
    int32_t n = static_cast<int32_t>(dst.mWidth) >> 2;
    int32_t shift;
    switch (n) {
    case 2: shift = 1; break;
    case 4: shift = 2; break;
    case 8: shift = 3; break;
    case 16: shift = 4; break;
    case 32: shift = 5; break;
    case 64: shift = 6; break;
    case 128: shift = 7; break;
    case 256: shift = 8; break;
    default: shift = 0; break;
    }
    uint32_t up = 16 - shift;
    for (uint32_t row = 0; row < 4; row++) {
        for (uint32_t col = 0; col < 4; col++) {
            const uint8_t* p = src.mData + ((row + ty * 4) * 4 * src.mWidth + (col + tx * 4) * 4 + channel);
            const uint8_t* q = p + src.mWidth * 4;   // the next control row
            uint32_t v00 = p[0], v10 = q[0], v01 = p[4], v11 = q[4];
            int32_t start = static_cast<int32_t>(v00 << 16);
            int32_t dx = static_cast<int32_t>((v01 - v00) << up);
            int32_t dy = static_cast<int32_t>((v10 - v00) << up);
            int32_t ddx = (static_cast<int32_t>((v11 - v10) << up) - dx) >> shift;
            uint8_t* rowp = dst.mData + (static_cast<uint32_t>(n) * row * dst.mWidth + col * static_cast<uint32_t>(n));
            for (int32_t y = n; --y >= 0;) {
                int32_t v = start;
                uint8_t* o = rowp;
                for (int32_t x = n; --x >= 0;) {
                    *o++ = static_cast<uint8_t>(v >> 16);
                    v += dx;
                }
                start += dy;
                dx += ddx;
                rowp += dst.mWidth;
            }
        }
    }
    return 1;
}

namespace {
// b + (k * (a - b)) >> 8, the per-channel lerp every kernel uses (arithmetic shift).
inline int32_t lerp8(int32_t b, int32_t a, int32_t k) {
    return b + ((k * (a - b)) >> 8);
}
}  // namespace

void blend_3(uint8_t* a1, uint8_t* b1, uint8_t* m1, uint8_t* t1, uint8_t* a2, uint8_t* b2, uint8_t* m2, uint8_t* t2,
             uint8_t* a3, uint8_t* b3, uint8_t* m3, uint8_t* t3, uint8_t* c, uint8_t* w12, uint8_t* w23, uint8_t* out) {
    for (int32_t i = 0; i < 16; i++) {
        int32_t ctl = *c;   // not advanced: one control byte for the whole run
        int32_t k1 = t1[(*m1 * ctl) >> 8];
        int32_t k2 = t2[(*m2 * ctl) >> 8];
        int32_t k3 = t3[(*m3 * ctl) >> 8];
        int32_t l1[4], m23[4];
        for (int32_t ch = 1; ch <= 3; ch++) {
            l1[ch] = lerp8(b1[ch], a1[ch], k1) & 0xff;
            int32_t l2 = lerp8(b2[ch], a2[ch], k2) & 0xff;
            int32_t l3 = lerp8(b3[ch], a3[ch], k3) & 0xff;
            m23[ch] = lerp8(l3, l2, *w23) & 0xff;
        }
        out[0] = 0;
        out[1] = static_cast<uint8_t>(lerp8(m23[1], l1[1], *w12));
        out[2] = static_cast<uint8_t>(lerp8(m23[2], l1[2], *w12));
        out[3] = static_cast<uint8_t>(lerp8(m23[3], l1[3], *w12));
        a1 += 4; b1 += 4; a2 += 4; b2 += 4; a3 += 4; b3 += 4;
        m1++; m2++; m3++; w12++; w23++;
        out += 4;
    }
}

void blend_2(uint8_t* a1, uint8_t* b1, uint8_t* m1, uint8_t* t1, uint8_t* a2, uint8_t* b2, uint8_t* m2, uint8_t* t2,
             uint8_t* c, uint8_t* w, uint8_t* out) {
    for (int32_t i = 0; i < 16; i++) {
        int32_t ctl = *c++;
        int32_t k1 = t1[(*m1++ * ctl) >> 8];
        int32_t k2 = t2[(*m2++ * ctl) >> 8];
        int32_t l1[4], l2[4];
        for (int32_t ch = 1; ch <= 3; ch++) {
            l1[ch] = lerp8(b1[ch], a1[ch], k1) & 0xff;
            l2[ch] = lerp8(b2[ch], a2[ch], k2) & 0xff;
        }
        a1 += 4; b1 += 4; a2 += 4; b2 += 4;
        out[0] = 0;
        out[1] = static_cast<uint8_t>(lerp8(l2[1], l1[1], *w));
        out[2] = static_cast<uint8_t>(lerp8(l2[2], l1[2], *w));
        out[3] = static_cast<uint8_t>(lerp8(l2[3], l1[3], *w));
        w++;
        out += 4;
    }
}

void single_layer(uint8_t* a, uint8_t* b, uint8_t* m, uint8_t* t, uint8_t* c, uint8_t* out) {
    for (int32_t i = 0; i < 16; i++) {
        int32_t k = t[(m[i] * c[i]) >> 8];
        out[0] = 0;
        out[1] = static_cast<uint8_t>(lerp8(b[1], a[1], k));
        out[2] = static_cast<uint8_t>(lerp8(b[2], a[2], k));
        out[3] = static_cast<uint8_t>(lerp8(b[3], a[3], k));
        a += 4;
        b += 4;
        out += 4;
    }
}

uint8_t calc_cover(uint8_t* mapA, uint8_t* mapB, uint8_t* ramp, uint8_t* control, uint8_t* out) {
    uint32_t all = 0xff, any = 0;
    for (int32_t i = 0; i < 16; i++) {
        uint32_t ta = ramp[(mapA[i] * control[i]) >> 8];
        uint32_t tb = ramp[(mapB[i] * control[i]) >> 8];
        out[i] = static_cast<uint8_t>(ta > tb ? ta : tb);
        all &= out[i];
        any |= out[i];
    }
    if ((all & 0xff) == 0xff) return 0xff;
    if ((any & 0xff) == 0) return 0;
    return 0xaa;
}

Tileset::Tileset() {
    clear();
}

Tileset::~Tileset() {
    clear();
}

void Tileset::clear() {
    mLevel = 0;
    mTileSize = 0;
    for (Bitmap& b : mBitmaps) b.clear();
}

int32_t Tileset::load(void* file) {
    gFileRead(&mTileSize, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&mTileSize), 1);
    gFileRead(&mLevel, 1, 1, file);
    // File order: the 4-byte textures first, then the 1-byte masks and cover maps.
    static const int kFileOrder[kTilesetBitmaps] = {0, 1, 3, 4, 8, 9, 2, 5, 10, 6, 7, 11, 12};
    for (int i : kFileOrder) mBitmaps[i].load(file);
    return 1;
}

Landscape::Landscape() {
    clear();
}

Landscape::~Landscape() {
    clear();
    if (gTileBuffer != nullptr) {
        delete[] gTileBuffer;
        gTileBuffer = nullptr;
    }
}

void Landscape::clear() {
    mRampC[0] = mRampC[1] = 0;
    mRampB[0] = mRampB[1] = 0;
    mRampCover2[0] = mRampCover2[1] = 0;
    mRampA[0] = mRampA[1] = 0;
    mRampCover1[0] = mRampCover1[1] = 0;
    mWidthTiles = 0;
    mHeightTiles = 0;
    mControl.clear();
    for (uint32_t i = 0; i < 3; i++) {
        mTilesets[i].clear();
        mTilesets[i].mTileSize = 0x40 >> i;
        mTilesets[i].mLevel = static_cast<uint8_t>(i);
    }
}

void Landscape::set_tilesize(int32_t size) {
    mTileSize = static_cast<uint8_t>(size);
    if (size == 0x40) {
        mTileset = 0;
    } else if (size == 0x20) {
        mTileset = 1;
    } else if (size == 0x10) {
        mTileset = 2;
    } else {
        mTileSize = 0x20;
        mTileset = 1;
    }
    if (gTileBuffer != nullptr) delete[] gTileBuffer;
    gTileBuffer = new uint8_t[mTileSize * mTileSize * 4];
}

namespace {
// 0 below centre - width/2, 255 above centre + width/2, a linear ramp between.
void make_ramp(uint8_t* table, uint8_t centre, uint8_t width) {
    int32_t span = static_cast<int16_t>(width) + 1;
    int16_t half = static_cast<int16_t>(span >> 1);
    int32_t lo = static_cast<int16_t>(centre - half);
    int32_t hi = static_cast<int16_t>(centre + half);
    for (int32_t i = 0; i < 256; i++) {
        if (i < lo) table[i] = 0;
        else if (i > hi) table[i] = 0xff;
        else table[i] = static_cast<uint8_t>(((i - lo) * 0xff) / span);
    }
}
}  // namespace

void Landscape::precalc_tables() {
    make_ramp(gRampCover1, mRampCover1[0], mRampCover1[1]);
    make_ramp(gRampCover2, mRampCover2[0], mRampCover2[1]);
    make_ramp(gRampA, mRampA[0], mRampA[1]);
    make_ramp(gRampB, mRampB[0], mRampB[1]);
    make_ramp(gRampC, mRampC[0], mRampC[1]);
}

void Landscape::generate_tile(uint8_t* out, uint32_t tileX, uint32_t tileY, sTileOverlay* overlay) {
    if (gTileBuffer == nullptr) return;
    uint32_t size = mTileSize;
    uint32_t tileYFlip = (static_cast<uint32_t>(mHeightTiles) - tileY) - 1;
    uint8_t* rowStart = gTileBuffer + (size - 1) * size * 4;   // rows are built bottom-up
    if (gControl1.mWidth != mTileSize) gControl1.realloc(mTileSize, mTileSize, 1);
    if (gControl3.mWidth != mTileSize) gControl3.realloc(mTileSize, mTileSize, 1);
    if (gControl2.mWidth != mTileSize) gControl2.realloc(mTileSize, mTileSize, 1);

    // The tile is only built when the control texel's four corners are fully covered.
    uint32_t stride = mControl.mWidth * 4;
    const uint8_t* corner = mControl.mData + ((tileYFlip * 4 + 1) * 4 * mControl.mWidth + (tileX * 4 + 1) * 4);
    if ((corner[0] & corner[4] & corner[stride] & corner[stride + 4] & 0xff) != 0xff) {
        uint8_t* o = out;
        for (int32_t i = 0; i < static_cast<int32_t>(mTileSize * mTileSize); i++) {
            o[0] = 0x00;
            o[1] = 0xff;
            o[2] = 0xff;
            o[3] = 0xff;
            o += 4;
        }
        return;
    }
    resample_control(mControl, tileX, tileYFlip, 1, gControl1);
    resample_control(mControl, tileX, tileYFlip, 2, gControl2);
    resample_control(mControl, tileX, tileYFlip, 3, gControl3);

    // Each tileset bitmap is sampled from (x0, y0), wrapped into (width - size, height - size).
    Tileset& ts = mTilesets[mTileset];
    uint32_t y0 = tileYFlip * size;
    uint32_t x0 = tileX * size;
    auto texel4 = [&](int k) {
        Bitmap& b = ts.mBitmaps[k];
        return b.mData + ((x0 % (b.mWidth - size)) + (y0 % (b.mHeight - size)) * b.mWidth) * 4;
    };
    auto texel1 = [&](int k) {
        Bitmap& b = ts.mBitmaps[k];
        return b.mData + ((y0 % (b.mHeight - size)) * b.mWidth + (x0 % (b.mWidth - size)));
    };
    uint8_t* aA = texel4(kLayerA_TextureA);
    uint8_t* bA = texel4(kLayerA_TextureB);
    uint8_t* aB = texel4(kLayerB_TextureA);
    uint8_t* bB = texel4(kLayerB_TextureB);
    uint8_t* aC = texel4(kLayerC_TextureA);
    uint8_t* bC = texel4(kLayerC_TextureB);
    uint8_t* mA = texel1(kLayerA_Mask);
    uint8_t* mB = texel1(kLayerB_Mask);
    uint8_t* mC = texel1(kLayerC_Mask);
    uint8_t* cover1a = texel1(kCover1_MapA);
    uint8_t* cover1b = texel1(kCover1_MapB);
    uint8_t* cover2a = texel1(kCover2_MapA);
    uint8_t* cover2b = texel1(kCover2_MapB);
    uint8_t* ctl3 = gControl3.mData;
    uint8_t* ctl2 = gControl2.mData;
    uint8_t* ctl1 = gControl1.mData;
    uint8_t w1[16], w2[16];

    uint8_t* dst = rowStart;
    for (int32_t row = 0; row < static_cast<int32_t>(mTileSize); row++) {
        for (int32_t run = 0; run < static_cast<int32_t>(mTileSize) / 16; run++) {
            uint32_t cover1 = calc_cover(cover1a, cover1b, gRampCover1, ctl3, w1);
            if (cover1 == 0xff) {
                single_layer(aA, bA, mA, gRampA, ctl1, dst);
            } else {
                uint32_t cover2 = calc_cover(cover2a, cover2b, gRampCover2, ctl2, w2);
                if (cover1 == 0) {
                    if (cover2 == 0xff) single_layer(aB, bB, mB, gRampB, ctl1, dst);
                    else if (cover2 == 0) single_layer(aC, bC, mC, gRampC, ctl1, dst);
                    else blend_2(aB, bB, mB, gRampB, aC, bC, mC, gRampC, ctl1, w2, dst);
                } else if (cover2 == 0xff) {
                    blend_2(aA, bA, mA, gRampA, aB, bB, mB, gRampB, ctl1, w1, dst);
                } else if (cover2 == 0) {
                    blend_2(aA, bA, mA, gRampA, aC, bC, mC, gRampC, ctl1, w1, dst);
                } else {
                    blend_3(aA, bA, mA, gRampA, aB, bB, mB, gRampB, aC, bC, mC, gRampC, ctl1, w1, w2, dst);
                }
            }
            dst += 0x40;
            aA += 0x40; bA += 0x40; aB += 0x40; bB += 0x40; aC += 0x40; bC += 0x40;
            cover1a += 0x10; ctl3 += 0x10; mA += 0x10; mB += 0x10; mC += 0x10;
            cover1b += 0x10; cover2a += 0x10; cover2b += 0x10; ctl2 += 0x10; ctl1 += 0x10;
        }
        uint32_t s = mTileSize;
        rowStart -= s * 4;
        dst = rowStart;
        Bitmap* b = mTilesets[mTileset].mBitmaps;
        // Step each source to its next row (32-bit wrapping offsets, as the original).
        aA += static_cast<int32_t>((b[kLayerA_TextureA].mWidth - s) * 4);
        bA += static_cast<int32_t>((b[kLayerA_TextureB].mWidth - s) * 4);
        aC += static_cast<int32_t>((b[kLayerC_TextureA].mWidth - s) * 4);
        bC += static_cast<int32_t>((b[kLayerC_TextureB].mWidth - s) * 4);
        aB += static_cast<int32_t>((b[kLayerB_TextureA].mWidth - s) * 4);
        bB += static_cast<int32_t>((b[kLayerB_TextureB].mWidth - s) * 4);
        mA += static_cast<int32_t>(b[kLayerA_Mask].mWidth - s);
        cover2b += static_cast<int32_t>(b[kCover2_MapB].mWidth - s);
        cover1b += static_cast<int32_t>(b[kCover1_MapB].mWidth - s);
        mB += static_cast<int32_t>(b[kLayerB_Mask].mWidth - s);
        mC += static_cast<int32_t>(b[kLayerC_Mask].mWidth - s);
        cover2a += static_cast<int32_t>(b[kCover2_MapA].mWidth - s);
        cover1a += static_cast<int32_t>(b[kCover1_MapA].mWidth - s);
    }

    uint32_t s = mTileSize;
    if (overlay == nullptr || overlay->mData == nullptr) {
        std::memcpy(out, gTileBuffer, s * s * 4);
        return;
    }

    // Overlay: walk the tile buffer in the orientation the flags select.
    int32_t rowBytes = static_cast<int32_t>(s * 4);
    int32_t total = static_cast<int32_t>(s * s * 4);
    uint32_t flags = overlay->mFlags;
    uint8_t* base;
    int32_t pixStep, rowStep;
    if (flags & sTileOverlay::kRotate) {
        if (flags & sTileOverlay::kFlip8) {
            base = gTileBuffer + total - rowBytes; rowStep = -rowBytes; pixStep = 4;
        } else if (flags & sTileOverlay::kFlip10) {
            base = gTileBuffer + total - 4; pixStep = -rowBytes; rowStep = -4;
        } else if (flags & sTileOverlay::kFlip20) {
            base = gTileBuffer + rowBytes - 4; rowStep = rowBytes; pixStep = -4;
        } else {
            base = gTileBuffer; pixStep = rowBytes; rowStep = 4;
        }
    } else {
        if (flags & sTileOverlay::kFlip8) {
            base = gTileBuffer + total - rowBytes; pixStep = -rowBytes; rowStep = 4;
        } else if (flags & sTileOverlay::kFlip10) {
            base = gTileBuffer + total - 4; rowStep = -rowBytes; pixStep = -4;
        } else if (flags & sTileOverlay::kFlip20) {
            base = gTileBuffer + rowBytes - 4; pixStep = rowBytes; rowStep = -4;
        } else {
            base = gTileBuffer; rowStep = rowBytes; pixStep = 4;
        }
    }

    const uint8_t* srcRow = static_cast<const uint8_t*>(overlay->mData);
    uint8_t* o = out;
    uint8_t* tileRow = base;
    if (overlay->mFormat == 2) {
        int32_t xStep = overlay->mWidth / static_cast<int32_t>(s);
        int32_t yStep = overlay->mHeight / static_cast<int32_t>(s);
        for (int32_t row = 0; row < static_cast<int32_t>(mTileSize); row++) {
            const uint8_t* sp = srcRow;
            uint8_t* tp = tileRow;
            for (int32_t col = 0; col < static_cast<int32_t>(mTileSize); col++) {
                uint16_t v16;
                std::memcpy(&v16, sp, 2);
                uint32_t v = v16;
                sp += xStep * 2;
                o[0] = 0;
                int32_t alpha = static_cast<int32_t>((v & 0xf) << 4);
                o[1] = static_cast<uint8_t>(lerp8(tp[1], static_cast<int32_t>((v >> 8) & 0xf0), alpha));
                o[2] = static_cast<uint8_t>(lerp8(tp[2], static_cast<int32_t>((v >> 4) & 0xf0), alpha));
                o[3] = static_cast<uint8_t>(lerp8(tp[3], static_cast<int32_t>(v & 0xf0), alpha));
                tp += pixStep;
                o += 4;
            }
            tileRow += rowStep;
            srcRow += overlay->mWidth * yStep * 2;
        }
    } else if (overlay->mFormat == 4) {
        int32_t xStep = overlay->mWidth / static_cast<int32_t>(s);
        int32_t yStep = overlay->mHeight / static_cast<int32_t>(s);
        for (int32_t row = 0; row < static_cast<int32_t>(mTileSize); row++) {
            const uint8_t* sp = srcRow;
            uint8_t* tp = tileRow;
            for (int32_t col = 0; col < static_cast<int32_t>(mTileSize); col++) {
                uint32_t v;
                std::memcpy(&v, sp, 4);
                sp += xStep * 4;
                o[0] = 0;
                int32_t alpha = static_cast<int32_t>(v & 0xff);
                o[1] = static_cast<uint8_t>(lerp8(tp[1], static_cast<int32_t>(v >> 24), alpha));
                o[2] = static_cast<uint8_t>(lerp8(tp[2], static_cast<int32_t>((v >> 16) & 0xff), alpha));
                o[3] = static_cast<uint8_t>(lerp8(tp[3], static_cast<int32_t>((v >> 8) & 0xff), alpha));
                tp += pixStep;
                o += 4;
            }
            tileRow += rowStep;
            srcRow += overlay->mWidth * yStep * 4;
        }
    }
}

int32_t Landscape::load_landscape(char* name) {
    clear();
    void* file = gFileOpen(name, "rb");
    if (file == nullptr) return 0;
    uint8_t version;
    gFileRead(&version, 1, 1, file);
    if (version != 3) {
        gFileClose(file);
        return 0;
    }
    gFileRead(&mRampC[0], 1, 1, file);
    gFileRead(&mRampC[1], 1, 1, file);
    gFileRead(&mRampB[0], 1, 1, file);
    gFileRead(&mRampB[1], 1, 1, file);
    gFileRead(&mRampCover2[0], 1, 1, file);
    gFileRead(&mRampCover2[1], 1, 1, file);
    gFileRead(&mRampA[0], 1, 1, file);
    gFileRead(&mRampA[1], 1, 1, file);
    gFileRead(&mRampCover1[0], 1, 1, file);
    gFileRead(&mRampCover1[1], 1, 1, file);
    precalc_tables();
    mControl.load(file);
    mWidthTiles = static_cast<int32_t>(mControl.mWidth - 1) >> 2;
    mHeightTiles = static_cast<int32_t>(mControl.mHeight - 1) >> 2;
    for (uint32_t i = 0; i < 3; i++) {
        if (mTileset == i) {
            mTilesets[i].load(file);
        } else {
            // Skip the other tile sizes: header, then 13 bitmaps' sizes and pixels.
            uint32_t tileSize;
            uint8_t level;
            gFileRead(&tileSize, 4, 1, file);
            gFileRead(&level, 1, 1, file);
            for (int32_t k = 0; k < kTilesetBitmaps; k++) {
                uint32_t width, height, bytesPerPixel;
                gFileRead(&width, 4, 1, file);
                SamsUtilities::_USwapBlock32(&width, 1);
                gFileRead(&height, 4, 1, file);
                SamsUtilities::_USwapBlock32(&height, 1);
                gFileRead(&bytesPerPixel, 4, 1, file);
                SamsUtilities::_USwapBlock32(&bytesPerPixel, 1);
                gFileSeek(file, bytesPerPixel * width * height, 1);
            }
        }
    }
    gFileClose(file);
    return 1;
}
