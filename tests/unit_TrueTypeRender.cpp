// Unit w85: TrueTypeRender (src/game/rides/RideSign.cpp). Every callee outside the unit is a fake that appends a line to a text
// log (tpw_ttr_log); tests/test_TrueTypeRender.py builds the same lines on the PowerPC side and compares the two logs.
//
// What is faked: the file read (TPFile_Read, over a byte blob the test hands in) and _USwapBlock32; the eleven GDI calls
// (handles are small integers that the fakes count out, so both sides print the same numbers; SelectObject remembers the
// bitmap and the font that were selected last and answers them as the previous object; GetTextExtentPoint answers a
// scripted list of extents; CreateDIBSection hands back a buffer the test filled); Bitmap::realloc; wcslen; and operator
// new[] and delete[] (blocks are filled with 0xc3, the Nth new[] can answer NULL, and a delete[] is logged with a hash of
// the block's bytes at that moment, which shows the narrow text and the bitmap header the unit built).
// The unit is built with -fcheck-new so that the NULL checks after new[] are live here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

#include "Bitmap.h"
#include "FS_System.h"
#include "TrueTypeRender.h"
#include "platform/msl.h"
#include "platform/samsutils.h"

namespace {

// ---- the log ---------------------------------------------------------------------------------------------------------
constexpr std::size_t kLogSize = 1 << 21;
char gLog[kLogSize];
std::size_t gLogLength = 0;
bool gRecording = false;
int32_t gEvents = 0;

// Appends one line "name n n n |text" (the texts are optional).
void event(const char* name, std::initializer_list<int64_t> ints = {}, const char* text = nullptr) {
    if (!gRecording) return;
    if (++gEvents > 20000 || gLogLength + 600 > kLogSize) {
        std::fprintf(stderr, "unit_TrueTypeRender: too many events (last: %s), probably an endless loop\n", name);
        std::abort();
    }
    gLogLength += std::snprintf(gLog + gLogLength, kLogSize - gLogLength, "%s", name);
    for (int64_t value : ints) gLogLength += std::snprintf(gLog + gLogLength, kLogSize - gLogLength, " %lld", static_cast<long long>(value));
    if (text != nullptr) gLogLength += std::snprintf(gLog + gLogLength, kLogSize - gLogLength, " |%s", text);
    gLog[gLogLength++] = '\n';
    gLog[gLogLength] = '\0';
}

uint32_t hash_bytes(const uint8_t* p, std::size_t n) {
    uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

// ---- the allocator ------------------------------------------------------------------------------------------------------
constexpr int kMaxBlocks = 16;
struct Block {
    void* p;
    std::size_t size;
};
Block gBlocks[kMaxBlocks];
int gBlockCount = 0;
int gNewCalls = 0;
int gFailAt = 0;
bool gLogDelete = false;

// ---- the case's inputs ---------------------------------------------------------------------------------------------------
const uint8_t* gBlob = nullptr;      // the file's bytes
int32_t gBlobLength = 0;
int32_t gBlobPosition = 0;
uint8_t* gObject = nullptr;          // the object under test (offsets of the reads are logged relative to it)
const int32_t* gExtents = nullptr;   // GetTextExtentPoint answers: cx, cy pairs, the last repeats
int32_t gExtentCount = 0;
int32_t gExtentNext = 0;
uint8_t* gBits = nullptr;            // what CreateDIBSection hands out
int32_t gDibNull = 0;
int32_t gNextDib = 0;
int32_t gNextFont = 0;
int32_t gNextDc = 0;
int32_t gCurrentBitmap = 0;
int32_t gCurrentFont = 0;
constexpr int32_t kStockBitmap = 0x4000;
constexpr int32_t kStockFont = 0x5000;

int64_t id(const void* handle) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(handle)); }
NS_MacDoze::InternalHandle* handle(int64_t value) { return reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<intptr_t>(value)); }

}  // namespace

#pragma clang diagnostic ignored "-Wnew-returns-null"
void* operator new[](std::size_t size) {
    ++gNewCalls;
    if (gRecording) event("new[]", {static_cast<int64_t>(size)});
    if (gRecording && gNewCalls == gFailAt) return nullptr;
    void* p = std::malloc(size != 0 ? size : 1);
    std::memset(p, 0xc3, size);
    if (gRecording && gBlockCount < kMaxBlocks) gBlocks[gBlockCount++] = Block{p, size};
    return p;
}

void operator delete[](void* p) noexcept {
    if (p == nullptr) return;
    for (int i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].p != p) continue;
        // the bitmap header's words are big-endian on the PowerPC side, so that block is hashed from the palette on
        const std::size_t skip = gBlocks[i].size == 0x42c ? 0x28 : 0;
        event("delete[]", {i + 1, static_cast<int64_t>(hash_bytes(static_cast<uint8_t*>(p) + skip, gBlocks[i].size - skip))});
        gBlocks[i].p = nullptr;
        break;
    }
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

void* operator new(std::size_t size) { return std::malloc(size != 0 ? size : 1); }
void operator delete(void* p) noexcept {
    if (gLogDelete && p != nullptr) event("delete");
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }

// ---- fakes: the file and the byte swap -----------------------------------------------------------------------------------
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile*) {
    const int32_t position = gBlobPosition;
    event("Read", {static_cast<int64_t>(static_cast<uint8_t*>(dest) - gObject), size, count, position});
    const int64_t want = static_cast<int64_t>(size) * count;
    int64_t got = gBlobLength - position;
    if (got > want) got = want;
    if (got < 0) got = 0;
    std::memcpy(dest, gBlob + position, static_cast<std::size_t>(got));
    gBlobPosition += static_cast<int32_t>(got);
    return size != 0 ? static_cast<uint32_t>(got / size) : 0;
}

void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) {
    event("Swap", {static_cast<int64_t>(reinterpret_cast<uint8_t*>(words) - gObject), count});
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t v = words[i];
        words[i] = (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
    }
}

// ---- fakes: wcslen and Bitmap::realloc ------------------------------------------------------------------------------------
uint32_t wcslen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}

// The real one frees and reallocates when the shape changes, then zeroes the buffer.
int32_t Bitmap::realloc(uint32_t width, uint32_t height, uint32_t bytesPerPixel) {
    event("realloc", {width, height, bytesPerPixel, mWidth, mHeight, mBytesPerPixel});
    const uint32_t size = bytesPerPixel * width * height;
    if (width != mWidth || height != mHeight || bytesPerPixel != mBytesPerPixel || mData == nullptr) {
        std::free(mData);
        mData = static_cast<uint8_t*>(std::malloc(size != 0 ? size : 1));
        mWidth = width;
        mHeight = height;
        mBytesPerPixel = bytesPerPixel;
    }
    std::memset(mData, 0, size);
    return 1;
}

// ---- fakes: GDI -----------------------------------------------------------------------------------------------------------
namespace NS_MacDoze {

InternalHandle* CreateDIBSection(InternalHandle* dc, const tagBITMAPINFO* info, uint32_t usage, void** bits,
                                 InternalHandle* section, uint32_t offset) {
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(info);
    const tagBITMAPINFOHEADER& h = info->bmiHeader;
    // header fields by name (the PowerPC side decodes its big-endian words the same way), then hashes of the 256 palette
    // entries (all four bytes: the reserved byte stays the allocator's 0xc3) and of the four bytes after them
    event("CreateDIBSection", {id(dc), usage, id(section), offset, h.biSize, h.biWidth, h.biHeight, h.biPlanes, h.biBitCount,
                               h.biCompression, h.biSizeImage, h.biXPelsPerMeter, h.biYPelsPerMeter, h.biClrUsed,
                               h.biClrImportant, static_cast<int64_t>(hash_bytes(raw + 0x28, 0x400)),
                               static_cast<int64_t>(hash_bytes(raw + 0x28 + 0x400, 4))});
    if (gDibNull) return nullptr;
    *bits = gBits;
    return handle(0x4001 + gNextDib++);
}

InternalHandle* CreateCompatibleDC(InternalHandle* dc) {
    const int64_t result = 0x6001 + gNextDc++;
    event("CreateCompatibleDC", {id(dc), result});
    return handle(result);
}

InternalHandle* SelectObject(InternalHandle* dc, InternalHandle* object) {
    const int64_t value = id(object);
    int32_t previous = 0;
    if ((value >> 12) == 4) {
        previous = gCurrentBitmap;
        gCurrentBitmap = static_cast<int32_t>(value);
    } else if ((value >> 12) == 5) {
        previous = gCurrentFont;
        gCurrentFont = static_cast<int32_t>(value);
    }
    event("SelectObject", {id(dc), value, previous});
    return handle(previous);
}

int32_t SetMapMode(InternalHandle* dc, int32_t mode) {
    event("SetMapMode", {id(dc), mode});
    return 1;
}

int32_t DeleteObject(InternalHandle* object) {
    event("DeleteObject", {id(object)});
    return 1;
}

int32_t DeleteDC(InternalHandle* dc) {
    event("DeleteDC", {id(dc)});
    return 1;
}

uint32_t SetBkColor(InternalHandle* dc, uint32_t colour) {
    event("SetBkColor", {id(dc), colour});
    return 0;
}

uint32_t SetTextColor(InternalHandle* dc, uint32_t colour) {
    event("SetTextColor", {id(dc), colour});
    return 0;
}

InternalHandle* CreateFontIndirect(const tagLOGFONTA* font) {
    const int64_t result = 0x5001 + gNextFont++;
    const uint8_t* flags = &font->lfItalic;
    event("CreateFontIndirect",
          {font->lfHeight, font->lfWidth, font->lfEscapement, font->lfOrientation, font->lfWeight, flags[0], flags[1], flags[2],
           flags[3], flags[4], flags[5], flags[6], flags[7], result},
          font->lfFaceName);
    return handle(result);
}

int32_t GetTextExtentPoint(InternalHandle* dc, const char* text, int32_t length, tagSIZE* size) {
    event("GetTextExtentPoint", {id(dc), length}, text);
    const int32_t i = gExtentNext < gExtentCount ? gExtentNext++ : gExtentCount - 1;
    size->cx = gExtents[2 * i];
    size->cy = gExtents[2 * i + 1];
    return 1;
}

int32_t TextOut(InternalHandle* dc, int32_t x, int32_t y, const char* text, int32_t length) {
    event("TextOut", {id(dc), x, y, length}, text);
    return 1;
}

}  // namespace NS_MacDoze

// ---- entry points ---------------------------------------------------------------------------------------------------------
namespace {

void begin(uint8_t* object) {
    gLogLength = 0;
    gLog[0] = '\0';
    gEvents = 0;
    gBlockCount = 0;
    gNewCalls = 0;
    gFailAt = 0;
    gObject = object;
    gBlob = nullptr;
    gBlobLength = 0;
    gBlobPosition = 0;
    gNextDib = gNextFont = gNextDc = 0;
    gCurrentBitmap = kStockBitmap;
    gCurrentFont = kStockFont;
    gDibNull = 0;
    gLogDelete = false;
}

}  // namespace

extern "C" {
const char* tpw_ttr_log() { return gLog; }
int32_t tpw_ttr_position() { return gBlobPosition; }

// The constructor over `object` (0x190 bytes the test filled).
void tpw_ttr_ctor(uint8_t* object) {
    begin(object);
    new (object) TrueTypeRender();
}

// The deleting destructor: `flag` is the hidden argument (a 16-bit value on the Mac; a positive one deletes), nullThis 1 passes
// a NULL object. The object is a fresh `new` block so that a delete is a real delete.
int32_t tpw_ttr_dtor(int32_t flag, int32_t nullThis) {
    begin(nullptr);
    gRecording = true;
    gLogDelete = true;
    TrueTypeRender* object = nullThis ? nullptr : new TrueTypeRender();
    // the shell: delete when the object exists and the (16-bit signed) flag is positive
    if (object != nullptr && static_cast<int16_t>(flag) > 0) {
        delete object;
        object = nullptr;
    }
    if (object != nullptr) {
        object->~TrueTypeRender();
        std::free(object);   // allocated by the operator new above (malloc)
    }
    gRecording = false;
    gLogDelete = false;
    return 0;
}

// load_settings over `object` and a file of `blob`.
int32_t tpw_ttr_load(uint8_t* object, const uint8_t* blob, int32_t blobLength) {
    begin(object);
    gBlob = blob;
    gBlobLength = blobLength;
    gRecording = true;
    const int32_t result = reinterpret_cast<TrueTypeRender*>(object)->load_settings(nullptr);
    gRecording = false;
    return result;
}

// render_font_unicode over `object`. text: UTF-16 with its NUL. bitmap: width, height, bytes per pixel in/out; the data is
// filled with 0x7e first and copied to dataOut after. extents: cx, cy pairs. bits: the DIB's bytes. failNew: the 1-based
// new[] that answers NULL; dibNull: CreateDIBSection answers NULL.
int32_t tpw_ttr_render(uint8_t* object, const char16_t* text, uint32_t* bitmap, uint8_t* dataOut, const int32_t* extents,
                       int32_t extentCount, uint8_t* bits, int32_t failNew, int32_t dibNull) {
    begin(object);
    gExtents = extents;
    gExtentCount = extentCount;
    gExtentNext = 0;
    gBits = bits;
    gDibNull = dibNull;
    const uint32_t size = bitmap[2] * bitmap[0] * bitmap[1];
    Bitmap out;
    out.mWidth = bitmap[0];
    out.mHeight = bitmap[1];
    out.mBytesPerPixel = bitmap[2];
    out.mData = static_cast<uint8_t*>(std::malloc(size != 0 ? size : 1));
    std::memset(out.mData, 0x7e, size);
    gFailAt = failNew;
    gRecording = true;
    const int32_t result = reinterpret_cast<TrueTypeRender*>(object)->render_font_unicode(handle(0x500), text, out);
    gRecording = false;
    bitmap[0] = out.mWidth;
    bitmap[1] = out.mHeight;
    bitmap[2] = out.mBytesPerPixel;
    const uint32_t after = out.mBytesPerPixel * out.mWidth * out.mHeight;
    std::memcpy(dataOut, out.mData, after);
    std::free(out.mData);
    out.mData = nullptr;
    return result;
}
}
