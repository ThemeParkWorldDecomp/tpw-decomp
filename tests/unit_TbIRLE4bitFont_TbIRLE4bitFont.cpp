// Test library for the TbIRLE4bitFont default constructor (libtpw_unit_TbIRLE4bitFont_TbIRLE4bitFont.dylib).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbIRLE4bitFont.h"

// The other virtuals live in src/TbIRLE4bitFont.cpp, which this library does not compile: empty bodies so the vtable links.
TbIRLE4bitFont::~TbIRLE4bitFont() {}
uint32_t TbIRLE4bitFont::GetCharIndex(const unsigned char*&) { return 0; }
uint16_t TbIRLE4bitFont::GetCharCount() { return 0; }
TbFontCharSize TbIRLE4bitFont::GetCharSize(uint16_t) { return TbFontCharSize(); }
uint8_t TbIRLE4bitFont::GetVerticalSpacing() { return 0; }
uint32_t TbIRLE4bitFont::GetCaps() { return 0; }
uint32_t TbIRLE4bitFont::GetType() { return 0; }
TbIMBToUnicode* TbIRLE4bitFont::GetMBToUnicode() { return nullptr; }

int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}
TbIFont::~TbIFont() {}

extern "C" {

// Builds the object over a block filled with 0xAA, returns [references, mData, mHeader, mOffsets, mMBToUnicode untouched?].
void tpw_rle_ctor(uint32_t* out) {
    void* mem = tpw::ArenaAllocZeroed(sizeof(TbIRLE4bitFont) + 16);
    std::memset(mem, 0xAA, sizeof(TbIRLE4bitFont) + 16);
    TbIRLE4bitFont* f = ::new (mem) TbIRLE4bitFont();
    out[0] = static_cast<uint32_t>(f->mReferences);
    out[1] = f->mData != nullptr;
    out[2] = f->mHeader != nullptr;
    out[3] = f->mOffsets != nullptr;
    out[4] = reinterpret_cast<uintptr_t>(f->mMBToUnicode) == 0xAAAAAAAAAAAAAAAAull;
}

}  // extern "C"
