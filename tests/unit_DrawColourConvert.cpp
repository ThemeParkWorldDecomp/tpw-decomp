#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "DrawRenderClasses.h"
#include "TbRaConvert.h"

struct TbPalette { uint8_t bytes[0x400]; };

CConverterBase::~CConverterBase() = default;
CInverseMap::CInverseMap() = default;
CInverseMap::~CInverseMap() = default;
void CInverseMap::InvCMap(int32_t, int32_t, const TbPalette&, int32_t,
                         uint32_t*, uint8_t*) {}

using bfl::draw::CColour;
using bfl::draw::TbMaskRotates;

namespace {
void CopyPalState(const CPalToMaskConverter& c, uint32_t* out) {
    std::memcpy(out, c.mSourceMask, 3 * sizeof(uint32_t));
    std::memcpy(out + 3, c.mShiftRight, 3 * sizeof(uint32_t));
    std::memcpy(out + 6, c.mShiftLeft, 3 * sizeof(uint32_t));
    out[9] = c.mAlphaMask;
    out[10] = c.mDestinationBytes;
}

void SetPalState(CPalToMaskConverter& c, const uint32_t* state,
                 const uint8_t* palette) {
    std::memcpy(c.mSourceMask, state, 3 * sizeof(uint32_t));
    std::memcpy(c.mShiftRight, state + 3, 3 * sizeof(uint32_t));
    std::memcpy(c.mShiftLeft, state + 6, 3 * sizeof(uint32_t));
    c.mAlphaMask = state[9];
    c.mDestinationBytes = state[10];
    std::memcpy(c.mPalette, palette, 0x400);
}

void CopyMaskState(const CMaskToPalConverter& c, uint32_t* out) {
    out[0] = c.mBits;
    out[1] = c.mSide;
    std::memcpy(out + 2, c.mSourceMask, 3 * sizeof(uint32_t));
    std::memcpy(out + 5, c.mShiftRight, 3 * sizeof(uint32_t));
    std::memcpy(out + 8, c.mShiftLeft, 3 * sizeof(uint32_t));
    out[11] = c.mSourceBytes;
}

void SetMaskState(CMaskToPalConverter& c, const uint32_t* state,
                  const uint8_t* palette, uint8_t* inverse) {
    std::memcpy(c.mPalette, palette, 0x400);
    c.mPaletteSource = nullptr;
    c.mInverseMap = inverse;
    c.mBits = state[0];
    c.mSide = state[1];
    std::memcpy(c.mSourceMask, state + 2, 3 * sizeof(uint32_t));
    std::memcpy(c.mShiftRight, state + 5, 3 * sizeof(uint32_t));
    std::memcpy(c.mShiftLeft, state + 8, 3 * sizeof(uint32_t));
    c.mSourceBytes = state[11];
}
}

extern "C" {
uint32_t tpw_dcc_colour_format(const uint8_t* colour, const TbPixFormat* format,
                               uint8_t* cache) {
    CColour c;
    std::memcpy(&c, colour, sizeof(c));
    const uint32_t result = c.GetFormattedValue(*format);
    std::memcpy(cache, c.mFormatCache, sizeof(c.mFormatCache));
    return result;
}

uint32_t tpw_dcc_colour_rotates(const uint8_t* colour, const uint32_t* words) {
    CColour c;
    TbMaskRotates rotates;
    std::memcpy(&c, colour, sizeof(c));
    std::memcpy(&rotates, words, sizeof(rotates));
    return c.GetFormattedValue(rotates);
}

uint32_t tpw_dcc_rotate_get(uint32_t top, uint32_t mask) {
    TbMaskRotates rotates{};
    return rotates.GetRightRotate2(top, mask);
}

void tpw_dcc_rotate_init(const TbPixFormat* source, const TbPixFormat* destination,
                         uint32_t* out) {
    TbMaskRotates rotates;
    rotates.Init(*source, *destination);
    std::memcpy(out, &rotates, sizeof(rotates));
}

void tpw_dcc_pal_shift(uint32_t source, uint32_t destination, uint32_t* out) {
    CPalToMaskConverter c;
    c.GetShiftBits(source, destination, out[0], out[1]);
}

void tpw_dcc_pal_init(const uint8_t* palette, const TbPixFormat* format,
                      uint32_t* state, uint8_t* copied) {
    CPalToMaskConverter c;
    c.Init(*reinterpret_cast<const TbPalette*>(palette), *format);
    CopyPalState(c, state);
    std::memcpy(copied, c.mPalette, 0x400);
}

void tpw_dcc_pal_line(uint32_t down, const uint32_t* state,
                      const uint8_t* palette, uint8_t* destination,
                      uint8_t* source, int32_t count) {
    CPalToMaskConverter c;
    SetPalState(c, state, palette);
    if (down) c.ConvertLineDown(destination, source, count);
    else c.ConvertLineUp(destination, source, count);
}

void tpw_dcc_pal_destruct() {
    alignas(CPalToMaskConverter) uint8_t storage[sizeof(CPalToMaskConverter)]{};
    reinterpret_cast<CPalToMaskConverter*>(storage)->CPalToMaskConverter::~CPalToMaskConverter();
}

void tpw_dcc_mask_shift(uint32_t source, uint32_t destination, uint32_t* out) {
    alignas(CMaskToPalConverter) uint8_t storage[sizeof(CMaskToPalConverter)]{};
    auto* c = reinterpret_cast<CMaskToPalConverter*>(storage);
    c->GetShiftBits(source, destination, out[0], out[1]);
}

uint8_t tpw_dcc_mask_brute(const uint8_t* palette, uint8_t red,
                           uint8_t green, uint8_t blue) {
    alignas(CMaskToPalConverter) uint8_t storage[sizeof(CMaskToPalConverter)]{};
    auto* c = reinterpret_cast<CMaskToPalConverter*>(storage);
    std::memcpy(c->mPalette, palette, 0x400);
    return c->BruteFindColour(red, green, blue);
}

void tpw_dcc_mask_line(uint32_t down, const uint32_t* state,
                       const uint8_t* palette, uint8_t* inverse,
                       uint8_t* destination, uint8_t* source, int32_t count) {
    alignas(CMaskToPalConverter) uint8_t storage[sizeof(CMaskToPalConverter)]{};
    auto* c = reinterpret_cast<CMaskToPalConverter*>(storage);
    SetMaskState(*c, state, palette, inverse);
    if (down) c->CMaskToPalConverter::ConvertLineDown(destination, source, count);
    else c->CMaskToPalConverter::ConvertLineUp(destination, source, count);
}

void tpw_dcc_mask_init(const uint8_t* palette, const TbPixFormat* format,
                       uint32_t* state) {
    CMaskToPalConverter c(*format, *reinterpret_cast<const TbPalette*>(palette));
    CopyMaskState(c, state);
}

void tpw_dcc_mask_release() {
    alignas(CMaskToPalConverter) uint8_t storage[sizeof(CMaskToPalConverter)]{};
    auto* c = reinterpret_cast<CMaskToPalConverter*>(storage);
    c->mInverseMap = static_cast<uint8_t*>(std::malloc(32));
    c->Release();
}

void tpw_dcc_mask_destruct() {
    alignas(CMaskToPalConverter) uint8_t storage[sizeof(CMaskToPalConverter)]{};
    auto* c = reinterpret_cast<CMaskToPalConverter*>(storage);
    c->mInverseMap = static_cast<uint8_t*>(std::malloc(32));
    c->CMaskToPalConverter::~CMaskToPalConverter();
}
}
