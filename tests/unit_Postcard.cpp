// Test library for the postcard composer (libtpw_unit_Postcard.dylib; see tests/test_Postcard.py): src/Postcard.cpp, the 14
// functions of PostcardBitmap and Postcard.
//
// Everything the unit calls is a fake that records through calllog: the TGA loader and its render area, FindFilePath, the
// platform's pixel converter and temporary allocator, the OpenGL reads and UDebug. The library includes calllog.cpp itself to
// watch the heap: every array allocation and release is an event ("new[]" with its number in the call, its size and whether the
// case made it fail; "delete[]" with the number of the allocation it releases, -1 for a block the case's set-up made, and its size), and
// every new block is filled with 0xa5 first, so a byte nobody writes compares as untouched. A failing allocation throws
// bad_alloc, which the translation's nothrow new turns into NULL.
//
// The Python module drives the library through one entry, tpw_pc_run: a case is a blob of numbers and bytes (the seven bitmaps of
// a Postcard, the arguments, an output buffer, the display size, the temporary allocator's state, the allocations that fail, the
// pictures the loader serves and the paths FindFilePath answers). The set-up runs with logging off; the call runs with it on.
// The answers and the state afterwards (the bitmaps' words and pixel checksums, the output buffer) come back as numbers.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
bool ObserveRelease(void* block, bool array);
}  // namespace
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_RELEASE_OBSERVER ObserveRelease
#include "calllog.cpp"

#include "Postcard.h"
#include "TPConfig.h"
#include "TbGraphicFile.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "tb_resource_display.h"

using namespace calllog;
using namespace SamsUtilities;

// The nothrow forms of operator new are the system library's, which calls the library's own operator new and not the one
// replaced above: these two route them through the replaced ones, so that the translation's `new (std::nothrow)` is watched.
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

namespace {

// ---- the blob reader (the case, from Python)
struct Reader {
    const uint8_t* cursor;
    uint32_t u32() {
        uint32_t value;
        std::memcpy(&value, cursor, 4);
        cursor += 4;
        return value;
    }
    int32_t s32() { return static_cast<int32_t>(u32()); }
    const uint8_t* bytes(uint32_t count) {
        const uint8_t* start = cursor;
        cursor += (count + 3) & ~3u;
        return start;
    }
};

// ---- crc32 (zlib's, so that Python's zlib.crc32 gives the same numbers)
uint32_t gCrcTable[256];
bool gCrcReady;
uint32_t Crc32(const void* data, std::size_t size) {
    if (!gCrcReady) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) != 0 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            gCrcTable[i] = c;
        }
        gCrcReady = true;
    }
    uint32_t crc = 0xffffffffu;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) crc = gCrcTable[(crc ^ bytes[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

// ---- the heap: every array block with its size and its number in the call (-1: made by the set-up)
struct Block {
    const void* address;
    std::size_t size;
    int32_t number;
};
constexpr int kBlocks = 2048;
Block gBlocks[kBlocks];
int32_t gAllocations;          // array allocations since the call began
constexpr int kFailMax = 16;
uint32_t gFail[kFailMax];
int32_t gFailCount;

Block* FindBlock(const void* address) {
    for (Block& block : gBlocks)
        if (block.address == address) return &block;
    return nullptr;
}

bool MustFail(int32_t number) {
    for (int32_t i = 0; i < gFailCount; ++i)
        if (gFail[i] == static_cast<uint32_t>(number)) return true;
    return false;
}

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!array) {
        if (on() && size == sizeof(UAllocatorTemporary)) ev("new", {8});
        return;
    }
    std::memset(block, 0xa5, size);
    int32_t number = -1;
    bool fail = false;
    if (on()) {
        number = ++gAllocations;
        fail = MustFail(number);
        ev("new[]", {number, static_cast<int64_t>(size), fail});
    }
    if (fail) {
        std::free(block);
        throw std::bad_alloc();
    }
    Block* slot = FindBlock(nullptr);
    if (slot != nullptr) *slot = Block{block, size, number};
}

bool ObserveRelease(void* block, bool array) {
    if (!array || block == nullptr) return false;
    Block* found = FindBlock(block);
    if (on()) ev("delete[]", {found != nullptr ? found->number : -2, found != nullptr ? static_cast<int64_t>(found->size) : 0});
    if (found != nullptr) *found = Block{nullptr, 0, 0};
    return false;
}

// ---- the world
constexpr int kBitmaps = 7;
alignas(Postcard) uint8_t gPostcardStore[sizeof(Postcard)];
Postcard* gPostcard;
constexpr std::size_t kOutCapacity = 0x180000;
uint8_t gOut[kOutCapacity + 16];

constexpr std::size_t kTempSize = 0x4b000;
uint8_t gTempBuffer[kTempSize + 16];
bool gAllocatorNull;                   // Allocate answers NULL

// The pixels the loader's area gives (AllocMemory's store).
constexpr std::size_t kStoreSize = 1 << 19;
uint8_t gPixelStore[kStoreSize];

// The pictures the loader serves, by the path it is asked for.
struct Picture {
    char path[256];
    int32_t depth, palettised, width, height, headerCode, loadCode;
    uint32_t size;
    const uint8_t* pixels;
};
constexpr int kPictures = 8;
Picture gPictures[kPictures];
int gPictureCount;

constexpr int kFinds = 8;
char gFinds[kFinds][256];
int gFindCount;
int gFindNext;

const uint8_t* gPattern;   // the bytes glReadPixels hands back, repeated
uint32_t gPatternSize;

Picture* PictureFor(const char* path) {
    for (int i = 0; i < gPictureCount; ++i)
        if (std::strcmp(gPictures[i].path, path) == 0) return &gPictures[i];
    return nullptr;
}

// ---- the pixel converter's objects
struct PixelsInfo {
    const void* object;
    uint8_t* data;
    int32_t width, height;
    bool flipped;
};
constexpr int kPixelsInfos = 8;
PixelsInfo gPixelsInfo[kPixelsInfos];
int gPixelsInfoCount;
PixelsInfo& Info(const void* object) {
    for (int i = 0; i < gPixelsInfoCount; ++i)
        if (gPixelsInfo[i].object == object) return gPixelsInfo[i];
    PixelsInfo& added = gPixelsInfo[gPixelsInfoCount++ % kPixelsInfos];
    added = PixelsInfo{object, nullptr, 0, 0, false};
    return added;
}

// The bitmap under test (the grab's pixels, or the first bitmap's): the pixels OpenGL and the converter's objects may point at.
PostcardBitmap* gSubject;
const uint8_t* SubjectPixels() { return gSubject != nullptr ? gSubject->mPixels : nullptr; }
}  // namespace

// ---- the loader
TbRenderArea::TbRenderArea() {
    ev("RenderArea.ctor", {newlab(this)});
    mDepth = 0;
    mPalettised = 0;
    mData = nullptr;
    mWidth = 0;
    mPictureHeight = 0;
    mStride = 0;
}
TbResult TbRenderArea::AllocMemory() {
    mStride = mWidth * ((mDepth + 7) >> 3);
    ev("AllocMemory", {lab(this), mStride, mPalettised, mPictureHeight});
    std::memset(gPixelStore, 0, sizeof gPixelStore);
    mData = gPixelStore;
    return TbResult{0};
}
void TbRenderArea::FreeMemory() { ev("FreeMemory", {lab(this)}); }
TbTGALoader::TbTGALoader() {
    ev("TGALoader.ctor", {newlab(this)});
    mHasPalette = 0;
}
TbTGALoader::~TbTGALoader() { ev("TGALoader.dtor", {lab(this)}); }
TbResult TbGraphicFileLoader::ReadHeader(const char* path, const char* second, TbRenderArea* area) {
    ev("ReadHeader", {lab(this), lab(area), second == nullptr}, {path});
    const Picture* picture = PictureFor(path);
    if (picture == nullptr) return TbResult{-1};
    area->mDepth = picture->depth;
    area->mPalettised = picture->palettised;
    area->mWidth = picture->width;
    area->mPictureHeight = picture->height;
    return TbResult{picture->headerCode};
}
TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, const char* path, const char* second, TbRenderArea* other) {
    ev("Load", {lab(this), lab(&area), second == nullptr, other == nullptr}, {path});
    const Picture* picture = PictureFor(path);
    if (picture == nullptr) return TbResult{-1};
    std::size_t bytes = static_cast<std::size_t>(area.mStride) * static_cast<std::size_t>(area.mPictureHeight);
    if (bytes > picture->size) bytes = picture->size;
    if (bytes > kStoreSize) bytes = kStoreSize;
    if (area.mData != nullptr) std::memcpy(area.mData, picture->pixels, bytes);
    return TbResult{picture->loadCode};
}

// ---- the file finder
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    if (gFindCount == 0) return nullptr;
    const int index = gFindNext < gFindCount ? gFindNext++ : gFindCount - 1;
    return gFinds[index];
}

// ---- UDebug
void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

// ---- the platform's pixels: the converter copies every byte of the source (rows reversed when it is flipped) xor 0x5a
UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    const int64_t which = data == gTempBuffer ? 1 : data == SubjectPixels() ? 2 : 0;
    const int64_t crc = data == gTempBuffer ? static_cast<int64_t>(Crc32(data, static_cast<std::size_t>(width) * height * 4)) : -1;
    ev("UPixels.wrap", {newlab(this), format, which, crc, width, height, stride, palette == nullptr});
    PixelsInfo& info = Info(this);
    info = PixelsInfo{this, static_cast<uint8_t*>(data), width, height, false};
}
void UPixels::SetFlipped(uint8_t flipped) {
    ev("UPixels.SetFlipped", {lab(this), flipped});
    Info(this).flipped = flipped != 0;
}
void UPixels::Convert(UPixels& source, bool dither) {
    ev("UPixels.Convert", {lab(this), lab(&source), dither, Info(&source).flipped});
    PixelsInfo& to = Info(this);
    const PixelsInfo& from = Info(&source);
    const int32_t rowBytes = to.width * 4;
    for (int32_t row = 0; row < to.height; ++row) {
        const int32_t sourceRow = from.flipped ? from.height - 1 - row : row;
        for (int32_t i = 0; i < rowBytes; ++i) to.data[row * rowBytes + i] = from.data[sourceRow * rowBytes + i] ^ 0x5a;
    }
}
UPixels::~UPixels() { ev("UPixels.dtor", {lab(this)}); }

// ---- the temporary allocator (its two table entries are virtual members here)
SamsUtilities::UAllocatorTemporary* SamsUtilities::gMemoryTemporary = nullptr;
UAllocatorTemporary::UAllocatorTemporary() { ev("UAllocatorTemporary.ctor", {newlab(this)}); }
UAllocatorTemporary::~UAllocatorTemporary() {}
uint8_t* UAllocatorTemporary::Allocate(uint32_t size) {
    ev("Allocate", {lab(this), size});
    if (gAllocatorNull) return nullptr;
    std::memset(gTempBuffer, 0xa5, sizeof gTempBuffer);
    return gTempBuffer;
}
void UAllocatorTemporary::Free(uint8_t*& block) {
    ev("Free", {lab(this), block == gTempBuffer, block == gTempBuffer ? static_cast<int64_t>(Crc32(block, kTempSize)) : -1});
    block = nullptr;
}

// ---- the display and OpenGL
CDisplay gei_Display;
CDisplay::CDisplay() {}
void glReadBuffer(GLenum mode) { ev("glReadBuffer", {mode}); }
void glDrawBuffer(GLenum mode) { ev("glDrawBuffer", {mode}); }
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    ev("glReadPixels", {x, y, width, height, format, type, pixels != nullptr, pixels == SubjectPixels()});
    if (pixels == nullptr || width <= 0 || height <= 0) return;
    const std::size_t bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    auto* out = static_cast<uint8_t*>(pixels);
    for (std::size_t i = 0; i < bytes; ++i) out[i] = gPattern[i % gPatternSize];
}

// ======================================================================= the entry

namespace {
// The operations, in the order of the Python module's table.
enum Op {
    kBitmapCtor, kBitmapDtor, kBitmapClear, kLoadFromTga, kCopyFromOpenGL, kAlphablt, kResize, kPostcardCtor, kPostcardDtor,
    kPostcardClear, kLoadSources, kGrab, kGenerate, kPreview
};

PostcardBitmap* BitmapAt(int index) {
    PostcardBitmap* bitmaps[kBitmaps] = {&gPostcard->mLogo, &gPostcard->mBullfrog, &gPostcard->mStamp, &gPostcard->mFrank,
                                         &gPostcard->mLegal, &gPostcard->mCard, &gPostcard->mGrab};
    return bitmaps[index];
}

void CopyText(char* to, const uint8_t* from, uint32_t size) {
    std::memcpy(to, from, size);
    to[size] = 0;
}

char gPrefix[256];
char gName[256];
}  // namespace

extern "C" {

// Runs one case. out receives the answer and the state; returns how many numbers it wrote.
int32_t tpw_pc_run(const uint8_t* blob, int64_t* out) {
    Reader in{blob};
    const int32_t fn = in.s32();
    int32_t arg[8];
    for (int32_t& value : arg) value = in.s32();
    struct Spec {
        uint32_t width, height, bytes;
        int32_t size;
        const uint8_t* data;
    } specs[kBitmaps];
    const uint32_t bitmapCount = in.u32();
    for (uint32_t i = 0; i < bitmapCount; ++i) {
        Spec& spec = specs[i];
        spec.width = in.u32();
        spec.height = in.u32();
        spec.bytes = in.u32();
        spec.size = in.s32();
        spec.data = spec.size > 0 ? in.bytes(static_cast<uint32_t>(spec.size)) : nullptr;
    }
    const uint32_t outSize = in.u32();
    gei_Display.mWidth = in.s32();
    gei_Display.mHeight = in.s32();
    const uint32_t allocatorState = in.u32();
    gAllocatorNull = in.u32() != 0;
    gFailCount = static_cast<int32_t>(in.u32());
    for (int32_t i = 0; i < gFailCount; ++i) gFail[i] = in.u32();
    gPatternSize = in.u32();
    gPattern = in.bytes(gPatternSize);
    gPictureCount = static_cast<int>(in.u32());
    for (int i = 0; i < gPictureCount; ++i) {
        Picture& picture = gPictures[i];
        const uint32_t pathSize = in.u32();
        CopyText(picture.path, in.bytes(pathSize), pathSize);
        picture.depth = in.s32();
        picture.palettised = in.s32();
        picture.width = in.s32();
        picture.height = in.s32();
        picture.headerCode = in.s32();
        picture.loadCode = in.s32();
        picture.size = in.u32();
        picture.pixels = in.bytes(picture.size);
    }
    gFindCount = static_cast<int>(in.u32());
    for (int i = 0; i < gFindCount; ++i) {
        const uint32_t size = in.u32();
        CopyText(gFinds[i], in.bytes(size), size);
    }
    gFindNext = 0;
    const bool hasPrefix = in.s32() != 0;
    const uint32_t prefixSize = in.u32();
    CopyText(gPrefix, in.bytes(prefixSize), prefixSize);
    const bool hasName = in.s32() != 0;
    const uint32_t nameSize = in.u32();
    CopyText(gName, in.bytes(nameSize), nameSize);

    // ---- the set-up, with logging off
    const int32_t wasOn = tpw_cl_on;
    tpw_cl_on = 0;
    for (Block& block : gBlocks) block = Block{nullptr, 0, 0};
    gPixelsInfoCount = 0;
    std::memset(gPostcardStore, 0xa5, sizeof gPostcardStore);
    gPostcard = fn == kPostcardCtor ? reinterpret_cast<Postcard*>(gPostcardStore) : new (gPostcardStore) Postcard;
    if (fn != kPostcardCtor) {
        for (uint32_t i = 0; i < bitmapCount; ++i) {
            PostcardBitmap* bitmap = BitmapAt(static_cast<int>(i));
            bitmap->mWidth = specs[i].width;
            bitmap->mHeight = specs[i].height;
            bitmap->mBytesPerPixel = specs[i].bytes;
            if (specs[i].size >= 0) {
                bitmap->mPixels = new uint8_t[static_cast<std::size_t>(specs[i].size)];
                if (specs[i].data != nullptr) std::memcpy(bitmap->mPixels, specs[i].data, static_cast<std::size_t>(specs[i].size));
            }
        }
    }
    std::memset(gOut, 0xa5, sizeof gOut);
    gMemoryTemporary = nullptr;
    if (allocatorState != 0) {
        // The allocator a case starts with: the constructor gives it the next label, as the Python side does.
        alignas(UAllocatorTemporary) static uint8_t block[sizeof(UAllocatorTemporary)];
        gMemoryTemporary = new (block) UAllocatorTemporary;
    }
    tpw_cl_on = wasOn;
    gAllocations = 0;

    // ---- the call
    int64_t answer = 0;
    PostcardBitmap* subject = fn == kGrab ? &gPostcard->mGrab : &gPostcard->mLogo;
    gSubject = subject;
    switch (fn) {
    case kBitmapCtor: {
        std::memset(static_cast<void*>(subject), 0xa5, sizeof *subject);
        new (subject) PostcardBitmap(static_cast<uint32_t>(arg[0]), static_cast<uint32_t>(arg[1]), static_cast<uint32_t>(arg[2]));
        break;
    }
    case kBitmapDtor: subject->~PostcardBitmap(); break;
    case kBitmapClear: subject->clear(); break;
    case kLoadFromTga: answer = subject->load_from_tga(hasPrefix ? gPrefix : nullptr, hasName ? gName : nullptr); break;
    case kCopyFromOpenGL: answer = subject->copy_from_opengl(); break;
    case kAlphablt: subject->alphablt(gPostcard->mBullfrog, static_cast<uint32_t>(arg[0]), static_cast<uint32_t>(arg[1])); break;
    case kResize: answer = subject->resize_into_buffer(gOut, arg[0], arg[1], arg[2]); break;
    case kPostcardCtor: new (gPostcard) Postcard; break;
    case kPostcardDtor: gPostcard->~Postcard(); break;
    case kPostcardClear: gPostcard->clear(); break;
    case kLoadSources: answer = gPostcard->load_sources(); break;
    case kGrab: {
        alignas(8) static uint8_t surface[8];   // the parameter is ignored
        answer = gPostcard->grab(reinterpret_cast<_DDSURFACEDESC2&>(surface));
        break;
    }
    case kGenerate: gPostcard->generate(); break;
    case kPreview: gPostcard->generate_preview_into_buffer(gOut, arg[0], arg[1], arg[2]); break;
    default: break;
    }

    // ---- the state afterwards, with logging off
    tpw_cl_on = 0;
    int64_t* put = out;
    *put++ = answer;
    for (int i = 0; i < kBitmaps; ++i) {
        const PostcardBitmap* bitmap = BitmapAt(i);
        *put++ = bitmap->mWidth;
        *put++ = bitmap->mHeight;
        *put++ = bitmap->mBytesPerPixel;
        if (bitmap->mPixels == nullptr) {
            *put++ = 0;
            *put++ = 0;
            *put++ = 0;
            *put++ = 0;
            continue;
        }
        const Block* block = FindBlock(bitmap->mPixels);
        const std::size_t size = block != nullptr ? block->size : 0;
        uint64_t head = 0;
        for (std::size_t k = 0; k < 8 && k < size; ++k) head = (head << 8) | bitmap->mPixels[k];
        *put++ = 1;
        *put++ = static_cast<int64_t>(size);
        *put++ = Crc32(bitmap->mPixels, size);
        *put++ = static_cast<int64_t>(head);
    }
    *put++ = Crc32(gOut, outSize + 16);
    *put++ = gMemoryTemporary != nullptr;
    return static_cast<int32_t>(put - out);
}

}  // extern "C"
