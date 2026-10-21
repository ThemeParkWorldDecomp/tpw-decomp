// Test library for unit w23 (libtpw_unit_ParticlesInit.dylib; see tests/test_ParticlesInit.py): the bullfrog library's
// zlib unpacker and semaphore lock, the particle system's start-up, the lobby start's small leaves (texture folders, the
// authorised user, the debug font, the string heap) and the ltms font render parameters.
//
// Real: src/bullfrog/LbZLib.cpp (over the host's zlib), src/game/Particles.cpp with the particle system's globals
// (src/game/rides/rsse.cpp): PAR_*, the SamsUtilities swap helpers, DummyHeightCallBack; src/game/rides/RidePlacement.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/InterfaceStringHeap.cpp,
// src/ltms/TbFontRenderParams.cpp and src/bullfrog/LbSemaphore.cpp. The unit library is built with TPW_SWAP_FILE_WORDS, so that the
// swap helpers really swap (the loaded library's bytes are then the original's).
// Fakes that record on both sides (name, then every argument): operator new[] (nothrow), LbMem_Alloc, the file calls
// of the library loader (TPFile_Open, LbFile_Read, LbFile_Seek, LbFile_Close, served from a file the case sets), the
// platform's _USwapBlock32, GAME_ASSERTMSG (it landed with unit w22, src/game/advisor/Advisor.cpp, and is not part of this unit),
// the ltms TbRenderArea constructor and a font whose AddRef records.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "FS_System.h"
#include "InterfaceFont.h"
#include "LbFile.h"
#include "LbSemaphore.h"
#include "TbFontRenderParams.h"
#include "TbResource.h"
#include "UI_Base.h"
#include "bfl_ZLib.h"
#include "calllog.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "tb_particle.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;

// ---- operator new[] without exceptions: the block is filled with 0xa5 and labelled; the case's queue "array failure"
// refuses one (a block of 1 GB or more is refused too, as the PowerPC side's stub does). A case may ask for a margin of
// bytes behind each block (also 0xa5), for the originals that write past the end of an array.
namespace {
std::size_t gNewMargin = 0;
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    const bool refuse = on() && (ret("array failure") != 0 || size >= 0x40000000u);
    void* p = refuse ? nullptr : std::malloc(size + gNewMargin != 0 ? size + gNewMargin : 1);
    if (p != nullptr) std::memset(p, 0xa5, size + gNewMargin);
    if (on()) ev("new[]", {static_cast<int64_t>(size), p != nullptr ? newlab(p) : 0});
    return p;
}
extern "C" void tpw_pi_set_margin(int32_t margin) { gNewMargin = static_cast<std::size_t>(margin); }

// ---- the zlib unpacker
extern "C" {
uint32_t tpw_pi_lbzlib(void* source, void* dest, void* workspace) { return LbZLib_MemDecompress(source, dest, workspace); }
int32_t tpw_pi_czlib(uint8_t* dest, uint8_t* source, uint8_t* workspace) {
    bfl::compress::CZLib decompressor;
    return decompressor.MemDecompress(dest, source, workspace);
}
int32_t tpw_pi_czlib_size() {
    bfl::compress::CZLib decompressor;
    return decompressor.CompressWorkspaceSize();
}
void tpw_pi_swapheader(ZLibCompHeader* header) { SwapHeader(header); }
}

// ---- the callees of the particle code

// operator new[] may be refused, and a block is labelled when it is made (see above); LbMem_Alloc is the library's heap.
// A block of 2 MB or more is not filled (the PowerPC side does not make one either: only its label matters).
void* LbMem_Alloc(uint32_t size) {
    const bool refuse = on() && (ret("LbMem_Alloc fails") != 0 || size >= 0x40000000u);
    void* p = refuse ? nullptr : std::malloc(size != 0 ? size : 1);
    if (p != nullptr && size < 0x200000u) std::memset(p, 0xa5, size != 0 ? size : 1);
    // The drawing list's block holds 100 records, 0xaf0 bytes of 0x1c on the Mac and more on the host (a native pointer in PTCL).
    const int64_t shown = size == kParDrawingListRecords * sizeof(PTCL) ? 0xaf0 : static_cast<int64_t>(size);
    ev("LbMem_Alloc", {shown, p != nullptr ? newlab(p) : 0});
    return p;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

namespace {
// The library file the case sets: a byte buffer and a position; the handle is one static object.
std::uint8_t gFileData[0x20000];
int32_t gFileLength;
int32_t gFilePosition;
char gFileObject[16];
constexpr int kLabelFile = 5;

bool InLibrary(const void* p) {
    const auto* byte = static_cast<const std::uint8_t*>(p);
    const auto* start = reinterpret_cast<const std::uint8_t*>(&gParLibrary);
    return byte >= start && byte < start + sizeof(gParLibrary);
}
}  // namespace

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    if (ret("TPFile_Open", 1) == 0) return nullptr;
    gFilePosition = 0;
    return reinterpret_cast<bfl::FileSystem::CFile*>(gFileObject);
}

// A short read hands over what there is and zeroes the rest of the buffer (the loader's locals are not set when the file is
// too short; the PowerPC side's fake does the same, so that the two have the same words).
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const uint32_t remaining = static_cast<uint32_t>(gFileLength - gFilePosition);
    const uint32_t count = std::min(length, remaining);
    std::memcpy(buffer, gFileData + gFilePosition, count);
    std::memset(static_cast<std::uint8_t*>(buffer) + count, 0, length - count);
    gFilePosition += static_cast<int32_t>(count);
    if (transferred != nullptr) *transferred = count;
    return TbError(0);
}

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    ev("LbFile_Seek", {lab(file), offset, whence});
    const int64_t position = static_cast<int64_t>(gFilePosition) + offset;
    gFilePosition = static_cast<int32_t>(std::max<int64_t>(0, std::min<int64_t>(gFileLength, position)));
    return 0;
}

TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {lab(file)});
    return TbError(0);
}

// The platform's byte swap does nothing on a little-endian host, and the particle loader's counts and sizes must stay
// as they are; the library's records, though, are swapped here (as the original does) so that their bytes can be
// compared. The case's queue "swap everywhere" makes it swap any buffer.
void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) {
    ev("SwapBlock32", {count, InLibrary(words) ? 1 : 0});
    if (!InLibrary(words) && ret("swap everywhere") == 0) return;
    for (int32_t i = 0; i < count; ++i) words[i] = __builtin_bswap32(words[i]);
}

// ---- the particle system

extern "C" {
void tpw_pi_set_file(const std::uint8_t* data, int32_t length) {
    std::memcpy(gFileData, data, static_cast<std::size_t>(length));
    gFileLength = length;
    gFilePosition = 0;
}
void tpw_pi_file_label() { tpw_cl_label(kLabelFile, gFileObject); }
void tpw_pi_init(void* owner, int32_t density, int32_t maxParticles) { PAR_InitParticleSystem(owner, density, maxParticles); }
void tpw_pi_reset() { PAR_ResetParticleSystem(); }
int32_t tpw_pi_load(char* path) { return PAR_LoadLibraryResult(path); }
void tpw_pi_load_void(char* path) { PAR_LoadLibrary(path); }
void tpw_pi_lowspec(int32_t on) { PAR_SetLowSpec(on); }
void* tpw_pi_malloc(int32_t size) { return PAR_Malloc(size); }
int32_t* tpw_pi_sin() { return PAR_SetupSin(); }
int32_t tpw_pi_height(int32_t x, int32_t z) { return DummyHeightCallBack(x, z); }
int32_t tpw_pi_sin_is_table(const int32_t* p) { return p == gParSinTable; }
// The helpers on a buffer.
void tpw_pi_swap16s(int16_t* p) { SamsUtilities::USwapInplace16(*p); }
void tpw_pi_swap16u(uint16_t* p) { SamsUtilities::USwapInplace16(*p); }
void tpw_pi_swap32(int32_t* p) { SamsUtilities::USwapInplace32(*p); }
void tpw_pi_swapblock(int32_t* p, int32_t count) { SamsUtilities::USwapBlock32(p, count); }

// The state: the header's words as a flat list (pointers as labels or kinds), and the regions as bytes.
// header[]: 0 mF00, 1 mTicks, 2 mLowSpec, 3 mMaxParticles, 4 mF10, 5 mF14, 6 mF18, 7 mF1A, 8 sin table is gParSinTable,
// 9 mDensity, 10 height callback (0 none, 1 the label 7, 2 DummyHeightCallBack, 3 other), 11 the pool's label.
void tpw_pi_header(int64_t* out) {
    const PARState& s = gParState;
    out[0] = s.mF00;
    out[1] = s.mTicks;
    out[2] = s.mLowSpec;
    out[3] = s.mMaxParticles;
    out[4] = s.mF10;
    out[5] = s.mF14;
    out[6] = s.mF18;
    out[7] = s.mF1A;
    out[8] = s.mSinTable == gParSinTable ? 1 : (s.mSinTable == nullptr ? 0 : 2);
    out[9] = s.mDensity;
    out[10] = s.mHeightCallback == nullptr ? 0
              : lab(s.mHeightCallback) == 7 ? 1
              : s.mHeightCallback == reinterpret_cast<const void*>(&DummyHeightCallBack) ? 2 : 3;
    out[11] = lab(s.mParticles);
}
// globals[]: 0 running, 1 the drawing list's busy flag, 2 the drawing list's label, 3 word A, 4 word B.
void tpw_pi_globals(int64_t* out) {
    out[0] = gParSystemRunning;
    out[1] = gParDrawingListBusy;
    out[2] = lab(gParDrawingList);
    out[3] = gParLibraryWordA;
    out[4] = gParLibraryWordB;
}
// Regions: 0 emitters, 1 effecters, 2 the chain heads (mEmittersUsed to the end), 3 the library, 4 the pool, 5 the sines.
void* tpw_pi_region(int32_t which, int32_t* size) {
    switch (which) {
    case 0: *size = sizeof(gParState.mEmitters); return gParState.mEmitters;
    case 1: *size = sizeof(gParState.mEffecters); return gParState.mEffecters;
    case 2: *size = 0x1c; return &gParState.mEmittersUsed;
    case 3: *size = sizeof(gParLibrary); return &gParLibrary;
    case 4: *size = gParState.mMaxParticles > 0 ? gParState.mMaxParticles * static_cast<int32_t>(sizeof(tagPARTICLE)) : 0;
            return gParState.mParticles;
    default: *size = sizeof(gParSinTable); return gParSinTable;
    }
}
// Puts the state, the library, the sines and the words into a known condition: every byte of them `fill`, then the
// running flag, the maximum and the pool as given.
void tpw_pi_prepare(int32_t fill, int32_t running, int32_t maximum, void* pool, uint32_t wordA, uint32_t wordB) {
    std::memset(&gParState, fill, sizeof(gParState));
    std::memset(&gParLibrary, fill, sizeof(gParLibrary));
    std::memset(gParSinTable, fill, sizeof(gParSinTable));
    gParSystemRunning = running;
    gParDrawingListBusy = fill;
    gParDrawingList = nullptr;
    gParLibraryWordA = wordA;
    gParLibraryWordB = wordB;
    gParState.mMaxParticles = maximum;
    gParState.mParticles = static_cast<tagPARTICLE*>(pool);
}
// A word of the state as the case wants it (its Mac offset in the header).
void tpw_pi_poke_header(int32_t index, int32_t value) {
    PARState& s = gParState;
    switch (index) {
    case 0: s.mF00 = value; break;
    case 1: s.mTicks = value; break;
    case 2: s.mLowSpec = value; break;
    case 4: s.mF10 = value; break;
    case 5: s.mF14 = value; break;
    case 6: s.mF18 = static_cast<int16_t>(value); break;
    case 7: s.mF1A = static_cast<int16_t>(value); break;
    case 9: s.mDensity = value; break;
    }
}
}

// ---- the lobby start's leaves

// Their globals (src/game/rides/RidePlacement.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/InterfaceStringHeap.cpp; the two texture folders are declared in include/ui_ErrorDisplay.h).
extern char gSecurityKey[];
extern char gSecurityText[];
extern char gSecurityMask[];
extern char gAuthorizationStamp[];
extern char gAuthorizedUser[];
extern uint8_t* gStringHeapTableB;
extern uint8_t* gStringHeapTableA;
extern int32_t gStringHeapRowsB;
extern int32_t gStringHeapWidthB;
extern int32_t gStringHeapRowsA;
extern int32_t gStringHeapWidthA;

namespace {
// The font object the case builds in static storage: filled with a byte, no constructor (Gen8x8DebugFont does not use one).
alignas(16) std::uint8_t gFontStorage[64];
}

extern "C" {
// The two folder names after SetThemeTextureDirectories, from arrays filled with `fill` before.
void tpw_pi_texdirs(int32_t small, int32_t fill, char* theme, char* shared) {
    std::memset(gThemeTextureFolder, fill, 16);
    std::memset(gSharedTextureFolder, fill, 16);
    SetThemeTextureDirectories(small);
    std::memcpy(theme, gThemeTextureFolder, 16);
    std::memcpy(shared, gSharedTextureFolder, 16);
}
// SEC_AuthorizedUser: the buffer is filled first; answers which array the text is in (0 the user buffer, 1 the stamp, 2 any
// other) and the first 160 bytes of what it points to (plus the buffer after the call).
int32_t tpw_pi_sec(int32_t fill, char* text, char* user) {
    std::memset(gAuthorizedUser, fill, 80);
    const char* answer = SEC_AuthorizedUser();
    std::strncpy(text, answer, 159);
    text[159] = '\0';
    std::memcpy(user, gAuthorizedUser, 80);
    return answer == gAuthorizedUser ? 0 : answer == gAuthorizationStamp ? 1 : 2;
}
// The arrays the check reads, for the case to patch: 0 the key, 1 its text, 2 the mask, 3 the stamp, 4 the user buffer.
void* tpw_pi_sec_area(int32_t which, int32_t* size) {
    switch (which) {
    case 0: *size = 77; return gSecurityKey;
    case 1: *size = 64; return gSecurityText;
    case 2: *size = 68; return gSecurityMask;
    case 3: *size = 23; return gAuthorizationStamp;
    default: *size = 80; return gAuthorizedUser;
    }
}
// The debug font: storage filled with `fill`; the offsets of its fields (vptr aside) for the case to read it.
void* tpw_pi_font_prepare(int32_t fill) {
    std::memset(gFontStorage, fill, sizeof(gFontStorage));
    return gFontStorage;
}
void tpw_pi_font_offsets(int32_t* out) {
    const InterfaceFont* font = reinterpret_cast<const InterfaceFont*>(gFontStorage);
    const auto at = [font](const void* field) {
        return static_cast<int32_t>(reinterpret_cast<const std::uint8_t*>(field) - reinterpret_cast<const std::uint8_t*>(font));
    };
    out[0] = at(&font->mF00);
    out[1] = at(&font->mF02);
    out[2] = at(&font->mF04);
    out[3] = at(&font->mHeight);
    out[4] = at(&font->mBitmap);
    out[5] = at(&font->mID);
    out[6] = static_cast<int32_t>(sizeof(InterfaceFont));
}
void tpw_pi_font_gen(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e) {
    reinterpret_cast<InterfaceFont*>(gFontStorage)->Gen8x8DebugFont(a, b, c, d, e);
}
// The string heap: its globals filled with a pattern, then the call (the result or the void form).
void tpw_pi_heap_prepare(int32_t fill) {
    const std::uint64_t pattern = 0x0101010101010101ull * static_cast<std::uint8_t>(fill);
    gStringHeapTableA = reinterpret_cast<uint8_t*>(pattern);
    gStringHeapTableB = reinterpret_cast<uint8_t*>(pattern);
    const int32_t word = static_cast<int32_t>(0x01010101u * static_cast<std::uint8_t>(fill));
    gStringHeapRowsA = gStringHeapRowsB = gStringHeapWidthA = gStringHeapWidthB = word;
}
int32_t tpw_pi_heap_alloc(int32_t a, int32_t b, int32_t c, int32_t d) { return InterfaceStringHeap::AllocateResult(a, b, c, d); }
void tpw_pi_heap_alloc_void(int32_t a, int32_t b, int32_t c, int32_t d) { InterfaceStringHeap::Allocate(a, b, c, d); }
// words[]: 0 width A, 1 rows A, 2 width B, 3 rows B, 4 table A's label, 5 table B's label.
void tpw_pi_heap_globals(int64_t* out) {
    out[0] = gStringHeapWidthA;
    out[1] = gStringHeapRowsA;
    out[2] = gStringHeapWidthB;
    out[3] = gStringHeapRowsB;
    out[4] = lab(gStringHeapTableA);
    out[5] = lab(gStringHeapTableB);
}
void* tpw_pi_heap_table(int32_t which) { return which == 0 ? gStringHeapTableA : gStringHeapTableB; }
}

// ---- the ltms font render parameters

// The render area's constructor is an import of the ltms library: it records and puts a recognisable word in each of
// the Mac's ten words at +0x14 (written as bytes: the host TbRenderArea, TbGraphicFile.h, is unit w21's struct, whose
// fields are not these words; the test compares the ten words by position).
TbRenderArea::TbRenderArea() {
    ev("TbRenderArea::TbRenderArea", {newlab(this)});
    static_assert(sizeof(TbRenderArea) >= 40, "the ten Mac words must fit");
    for (uint32_t i = 0; i < 10; ++i) {
        const uint32_t word = 0x7a000000u + i;
        std::memcpy(reinterpret_cast<uint8_t*>(this) + 4 * i, &word, 4);
    }
}
// The resource base's members are faked as in the other units (the font below overrides Release and AddRef).
int32_t TbResource::Release() { return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}
TbIFont::~TbIFont() {}

namespace {
// A font whose AddRef and Release record (the copy constructor takes a reference on the font, the destructor lets go).
class FakeFont : public TbIFont {
public:
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t Release() override {
        ev("Release", {lab(this)});
        return 0;
    }
    uint32_t GetCharIndex(const unsigned char*&) override { return 0; }
    uint16_t GetCharCount() override { return 0; }
    TbFontCharSize GetCharSize(uint16_t) override { return {0, 0}; }
    uint8_t GetVerticalSpacing() override { return 0; }
    uint32_t GetCaps() override { return 0; }
    uint32_t GetType() override { return 0; }
    TbIMBToUnicode* GetMBToUnicode() override { return nullptr; }
};
FakeFont gFakeFont;
}  // namespace

extern "C" {
int32_t tpw_pi_frp_size() { return static_cast<int32_t>(sizeof(TbFontRenderParams)); }
void* tpw_pi_frp_font() { return &gFakeFont; }
void tpw_pi_frp_default(void* storage) { new (storage) TbFontRenderParams; }
void tpw_pi_frp_copy(void* dest, const void* source) {
    new (dest) TbFontRenderParams(*static_cast<const TbFontRenderParams*>(source));
}
int32_t tpw_pi_frp_alpha(void* object) { return static_cast<TbFontRenderParams*>(object)->UseAlphaWriteMode(); }
// A parameters object made by the constructor (the case labels it), then given the slots of `raw` (the object's bytes after
// the vtable pointer); the destructor runs as the deleting form when `flag` is positive.
void* tpw_pi_frp_new(const void* slots, int32_t size) {
    TbFontRenderParams* object = new TbFontRenderParams;
    std::memcpy(reinterpret_cast<std::uint8_t*>(object) + sizeof(void*), slots, static_cast<std::size_t>(size));
    return object;
}
void tpw_pi_frp_destroy(void* object, int32_t flag) {
    TbFontRenderParams* params = static_cast<TbFontRenderParams*>(object);
    if (flag > 0) delete params;
    else params->~TbFontRenderParams();
}
void tpw_pi_frp_free(void* object) { std::free(object); }
void tpw_pi_frp_slots(void* object, void* out, int32_t size) {
    std::memcpy(out, reinterpret_cast<const std::uint8_t*>(object) + sizeof(void*), static_cast<std::size_t>(size));
}
}

// ---- the bullfrog semaphore lock

extern "C" {
// A lock over a semaphore word; the case labels it. The destructor is called as the deleting form when `flag` is positive.
void* tpw_pi_sema_new(uint32_t word) {
    _LbSemaphore* semaphore = reinterpret_cast<_LbSemaphore*>(&word);
    return new _LbSemaLock(*semaphore);
}
void tpw_pi_sema_dtor(void* lock, int32_t flag) {
    _LbSemaLock* object = static_cast<_LbSemaLock*>(lock);
    if (flag > 0) delete object;
    else if (object != nullptr) object->~_LbSemaLock();
}
int32_t tpw_pi_sema_lock(void* lock, int32_t wait) { return static_cast<_LbSemaLock*>(lock)->Lock(wait); }
void tpw_pi_sema_free(void* lock) { std::free(lock); }
}
