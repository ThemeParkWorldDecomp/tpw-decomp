// TbFontRenderParams.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbFontRenderParams code, named after it.

#include "bfl_FileSystem.h"
#include "CCharacterHandler.h"
#include "CStaticBuffer.h"
#include "helpers/TbFontRenderParamsHelpers.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"
#include "TbFontRender.h"
#include "TbFontRenderParams.h"
#include "TbIResourceIndex.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include <cstring>
#include <new>

// ltms TbFontRenderParams: the constructors, the destructor and UseAlphaWriteMode (ltms 0x10002350, 0x10002400, 0x100024ac,
// 0x1000296c). The class's other members are untranslated (include/TbFontRenderParams.h). The destructor is the class's key
// function: the constructors write the vtable pointer, so the vtable has to be emitted where the destructor is defined.

namespace {

// The five flag bytes at +0x44..+0x48 (the first five bytes of slot 16 on the host).
uint8_t* Flags(TbFontRenderParams& params) { return reinterpret_cast<uint8_t*>(&params.mState[16]); }

}  // namespace

// 0x10002350
TbFontRenderParams::TbFontRenderParams() {
    mState[0] = 0;
    mState[2] = 3;
    mState[3] = 1;
    new (&mState[4]) TbRenderArea;
    mState[14] = 0;
    mState[15] = 0;
    uint8_t* flags = Flags(*this);
    flags[0] = 0;
    flags[1] = 0;
    flags[2] = 0;
    flags[3] = 0;
    flags[4] = 0;
    mState[18] = 0;
}

// 0x10002400: lets go of the font (its virtual Release, slot 8 on the Mac) and forgets it. The original's deleting form
// (a positive halfword flag in r4 calls operator delete on `this`) is the compiler's handling of `delete`.
TbFontRenderParams::~TbFontRenderParams() {
    TbIFont* font = reinterpret_cast<TbIFont*>(mState[0]);
    if (font != nullptr) {
        font->Release();
        mState[0] = 0;
    }
}

// 0x100024ac
TbFontRenderParams::TbFontRenderParams(const TbFontRenderParams& other) {
    for (int32_t i = 0; i < 16; ++i) mState[i] = other.mState[i];
    const uint8_t* source = reinterpret_cast<const uint8_t*>(&other.mState[16]);
    uint8_t* flags = Flags(*this);
    for (int32_t i = 0; i < 5; ++i) flags[i] = source[i];
    mState[18] = other.mState[18];
    reinterpret_cast<TbIFont*>(mState[0])->AddRef();
}

// ltms TbFontRender: the constructor, destructor, Initialise and DrawChar (0x10001fdc, 0x10002050, 0x100020e4, 0x100021fc),
// and three TbFontRenderParams members (UseReColour 0x10002800, UsePreColoured 0x10002888, SetFont 0x100029d0). The class's
// other members are the render methods' (untranslated) or in src/tb_fontrenderparams.cpp.

// ---- unit w45a: the rest of the parameters' members (ltms 0x100025e8, 0x10002718, 0x100028f8, 0x10002ab8, 0x10002b70)

// 0x100025e8: nothing for a copy of itself. Otherwise: the blend word (+0x10), the five flag bytes (+0x44 as a word, +0x48 as
// a byte), the font (+4) with a reference taken on it WITHOUT a NULL test and without letting go of the old one, then the
// caps (+8), the style (+0xc), the render area and the point after it (+0x14..+0x40) and the word at +0x4c.
TbFontRenderParams& TbFontRenderParams::operator=(const TbFontRenderParams& other) {
    if (&other == this) return *this;
    mState[3] = other.mState[3];
    const uint8_t* source = reinterpret_cast<const uint8_t*>(&other.mState[16]);
    uint8_t* flags = Flags(*this);
    for (int32_t i = 0; i < 5; ++i) flags[i] = source[i];
    mState[0] = other.mState[0];
    reinterpret_cast<TbIFont*>(mState[0])->AddRef();
    mState[1] = other.mState[1];
    mState[2] = other.mState[2];
    for (int32_t i = 4; i < 16; ++i) mState[i] = other.mState[i];
    mState[18] = other.mState[18];
    return *this;
}

// 0x10002718: with a font whose caps include 4, the render area and the point become the parameters' (the area's ten words
// to +0x14..+0x38, the point's two to +0x3c and +0x40) and the style is 2 (textured); else -1 and nothing changes.
TbError TbFontRenderParams::UseTexture(const TbRenderArea& area, const TbPoint& point) {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 4) == 0) return TbError(-1);
    *reinterpret_cast<TbRenderArea*>(&mState[4]) = area;
    mState[14] = static_cast<uint32_t>(point.x);
    mState[15] = static_cast<uint32_t>(point.y);
    mState[2] = 2;
    return TbError(0);
}

// 0x10002800: with a font whose caps include 2, the colour (four bytes and the index byte) and style 1 (one colour).
TbError TbFontRenderParams::UseReColour(const TbColour& colour) {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 2) == 0) return TbError(-1);
    const uint8_t* source = reinterpret_cast<const uint8_t*>(&colour);
    uint8_t* flags = Flags(*this);
    flags[0] = source[0];
    flags[1] = source[1];
    flags[2] = source[2];
    flags[3] = source[3];
    flags[4] = source[4];
    mState[2] = 1;
    return TbError(0);
}

// 0x10002888: with a font whose caps include 1, style 0 (the font's own colours).
TbError TbFontRenderParams::UsePreColoured() {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 1) == 0) return TbError(-1);
    mState[2] = 0;
    return TbError(0);
}

// 0x100028f8: with a font whose caps include 8, the blend word is 1 (blended writing); else -1.
TbError TbFontRenderParams::UseBlendedMode() {
    if (mState[0] == 0 || (static_cast<uint32_t>(mState[1]) & 8) == 0) return TbError(-1);
    mState[3] = 1;
    return TbError(0);
}

// 0x1000296c: with a font, the slot at +0x10 is cleared and the answer is 0; without, -1.
TbError TbFontRenderParams::UseAlphaWriteMode() {
    if (mState[0] != 0) {
        mState[3] = 0;
        return TbError(0);
    }
    return TbError(-1);
}

// 0x100029d0: style 3 first, then the old font is released (before the new one is held, so the same font twice is let go
// first) and forgotten, then the new one is held and its caps taken. A NULL font leaves the caps as they were.
TbError TbFontRenderParams::SetFont(TbIFont* font) {
    mState[2] = 3;
    if (mState[0] != 0) {
        reinterpret_cast<TbIFont*>(mState[0])->Release();
        mState[0] = 0;
    }
    mState[0] = reinterpret_cast<uintptr_t>(font);
    if (mState[0] != 0) {
        font->AddRef();
        mState[1] = font->GetCaps();
    }
    return TbError(0);
}

// 0x10002ab8
int32_t TbFontRenderParams::GetStyle() const { return static_cast<int32_t>(static_cast<uint32_t>(mState[2])); }

// 0x10002af4: the font (slot 0, +4), after a reference has been taken on it (the font's virtual AddRef, slot 3) when there is one.
// Nobody lets the reference go (CreateInfoList leaks one per call).
TbIFont* TbFontRenderParams::GetFont() const {
    TbIFont* font = reinterpret_cast<TbIFont*>(mState[0]);
    if (font != nullptr) font->AddRef();
    return reinterpret_cast<TbIFont*>(mState[0]);
}

// 0x10002b70: the font's virtual GetType (slot 0x30), 5 without a font.
int32_t TbFontRenderParams::GetType() const {
    TbIFont* font = reinterpret_cast<TbIFont*>(mState[0]);
    if (font == nullptr) return 5;
    return static_cast<int32_t>(font->GetType());
}

// ltms: the 4-bit run-length font resource (ltms shared, 0x10002cb8 and
// 0x100040d8..0x100048ec). Drawing it is the font render methods' job.

TbIFont::~TbIFont() {}

// ltms: the resource factories. TbIResourceIndex::OpenResource opens a resource's file and
// hands it to the factory of the resource's type (gLtmsResourceFactories), which reads the
// file and builds the object, usually at the start of one allocation with the file's data
// behind it. File data is little-endian; the Mac swaps it after reading.

namespace {
// The header both character-set files start with (little-endian in the file).
struct MBHeader {
    uint32_t mMagic;      // 'BFMB' for the plain description, 'BFMU' or 'BFMH' with a table
    uint16_t mLead;       // low byte: 0xff minus the first lead byte
    uint16_t mCounts;     // see the factories
};

void SwapHeader(MBHeader& header) {
    SamsUtilities::USwapInplace32(header.mMagic);
    SamsUtilities::USwapInplace16(header.mLead);
    SamsUtilities::USwapInplace16(header.mCounts);
}
}  // namespace

// 0x10002d34: the 8-byte header alone. A bad file is closed here (the other factories
// leave that to their caller).
TbError MBInfoFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t, TbIResourceIndex*) const {
    MBHeader header;
    if (file.Read(&header, 8) != 8) {
        file.Close();
        return TbError(-1);
    }
    SwapHeader(header);
    if (header.mMagic != 0x424d4642u) {   // "BFMB"
        file.Close();
        return TbError(-1);
    }
    *out = new TbIMBInfo(static_cast<uint8_t>(-1 - static_cast<int8_t>(header.mLead & 0xff)), header.mCounts);
    return TbError(0);
}

// ltms: multibyte character sets and the multibyte to Unicode converter (ltms shared,
// 0x1000063c, 0x10002f3c, 0x10003204..0x10003338). Their loaders are in
// TbResourceFactories.cpp.

TbIMBInfo::~TbIMBInfo() {}

// 0x10002fbc: the header, then the table of 16-bit values to the end of the file. The
// 'BFMH' variant builds the same object and still reports failure.
TbError MBToUnicodeFactory::CreateInterface(bfl::FileSystem::CFile& file, void** out, uint32_t,
                                            TbIResourceIndex*) const {
    MBHeader header;
    if (file.Read(&header, 8) != 8) return TbError(-1);
    SwapHeader(header);
    if (header.mMagic != 0x554d4642u && header.mMagic != 0x484d4642u) return TbError(-1);   // "BFMU", "BFMH"
    const uint16_t firstLead = static_cast<uint16_t>(0xff - (header.mLead & 0xff));
    const uint32_t lastLead = header.mCounts & 0xff;
    const uint32_t lastTrail = (header.mCounts >> 8) & 0xff;
    const uint32_t size = Remaining(file);
    auto* block = new uint8_t[sizeof(TbIMBToUnicode) + size];
    auto* table = block + sizeof(TbIMBToUnicode);
    if (static_cast<uint32_t>(file.Read(table, size)) != size) {
        delete[] block;
        return TbError(-1);
    }
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(table), static_cast<int32_t>(size >> 1));
    const uint32_t count = (((lastLead - 1) & 0xff) * 0xff + (firstLead & 0xff) * static_cast<uint32_t>(-0xfe) + lastTrail) - 1;
    *out = new (block) TbIMBToUnicode(static_cast<uint8_t>(firstLead), static_cast<uint16_t>(count), table);
    return TbError(header.mMagic == 0x484d4642u ? -1 : 0);
}

// ltms 0x100034c0: bfl::FileSystem::CDiscFileStorage::~CDiscFileStorage (out-of-line copy in the ltms library).

namespace bfl {
namespace FileSystem {

// If this is non-NULL: store the CDiscFileStorage vtable (not observable on the host), run the base destructor,
// and delete the storage when the hidden short flag is positive. Returns this.
CDiscFileStorage* CDiscFileStorage::DestroyLtms(CDiscFileStorage* self, int16_t deleteFlag) {
    if (self) {
        self->CFileStorage::~CFileStorage();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(self));
    }
    return self;
}

}  // namespace FileSystem
}  // namespace bfl
