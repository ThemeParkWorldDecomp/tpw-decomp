// Unit w223: recording file, heap and converter fakes for IFF, GIF and TIM.
// The file and heap fakes follow CursorClasses; freed blocks remain readable until the next case.
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "LbFile.h"
#include "calllog.h"
#include "platform/samsutils.h"
using namespace calllog;
using namespace SamsUtilities;
using bfl::FileSystem::CAccessItem;
using bfl::FileSystem::CFile;
using bfl::FileSystem::CWadFile;
using bfl::FileSystem::_TbBufferedFile;
namespace {
constexpr int kMargin = 0x400;
constexpr int kBlockSize = 0x4000;
alignas(16) unsigned char gLoaderStore[0x600];
alignas(16) unsigned char gGllStore[0x300];
unsigned char gBlockStore[2][kBlockSize + kMargin];
TbRenderArea gArea;
int gKind;
TbGraphicFileLoader* Loader() { return reinterpret_cast<TbIFFLoader*>(gLoaderStore); }
_AndyGLL* Reader() { return reinterpret_cast<_AndyIFFGLL*>(gGllStore); }
uint8_t* Header(bool loader) {
    if (gKind == 0) return loader ? reinterpret_cast<TbIFFLoader*>(gLoaderStore)->mHeader : reinterpret_cast<_AndyIFFGLL*>(gGllStore)->mHeader;
    if (gKind == 1) return loader ? reinterpret_cast<TbGIFLoader*>(gLoaderStore)->mHeader : reinterpret_cast<_AndyGIFGLL*>(gGllStore)->mHeader;
    return loader ? reinterpret_cast<TbTIMLoader*>(gLoaderStore)->mHeader : reinterpret_cast<_TimGLL*>(gGllStore)->mHeader;
}
struct Block {
    const unsigned char* start;
    std::size_t extent;   // the block and the margin after it a run may write into
    int32_t label;
    bool freed;           // a heap block the library has let go of: it stays, so that a pointer left in the world still describes
};
constexpr int kBlockMax = 64;
Block gRegistry[kBlockMax];
int gRegistryCount;

void Register(const void* start, std::size_t extent, int32_t label) {
    if (gRegistryCount < kBlockMax) gRegistry[gRegistryCount++] = {static_cast<const unsigned char*>(start), extent, label, false};
}
// Marks a live heap block (a label of 100 or more) as let go of; false for anything else. The memory is kept until the next
// case (a block freed and made again would take the label of a pointer left behind).
bool Release(const void* start) {
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].start == start && gRegistry[i].label >= 100 && !gRegistry[i].freed) {
            gRegistry[i].freed = true;
            return true;
        }
    }
    return false;
}

// A pointer as the dumps write it, two numbers: the label of the block it lies in and the offset, (0, 0) for NULL, (-2, 0)
// for the untouched fill, (-1, raw) for anything else. A pointer a little before one of the world's blocks (a bottom-up
// copy of an empty picture starts a row before its area) counts as in that block, with a negative offset.
void Describe(const void* pointer, int64_t* out) {
    const auto* p = static_cast<const unsigned char*>(pointer);
    if (p == nullptr) {
        out[0] = out[1] = 0;
        return;
    }
    if (gKind == 1) {
        const auto* b = reinterpret_cast<_AndyGIFGLL*>(gGllStore)->mByteBuff;
        if (p >= b && p <= b + 257) { out[0] = 17; out[1] = p - b; return; }
    }
    for (int i = 0; i < gRegistryCount; ++i) {
        if (p >= gRegistry[i].start && p <= gRegistry[i].start + gRegistry[i].extent) {
            out[0] = gRegistry[i].label;
            out[1] = p - gRegistry[i].start;
            return;
        }
    }
    if (reinterpret_cast<uintptr_t>(p) == 0xa5a5a5a5a5a5a5a5ull) {
        out[0] = -2;
        out[1] = 0;
        return;
    }
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].label < 100 && p < gRegistry[i].start && gRegistry[i].start - p <= 0x1000) {
            out[0] = gRegistry[i].label;
            out[1] = p - gRegistry[i].start;
            return;
        }
    }
    out[0] = -1;
    out[1] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu);
}

// ---- the file stream every fake file serves
constexpr int kStreamMax = 1 << 16;
unsigned char gStream[kStreamMax];
int64_t gStreamSize;
int64_t gStreamPos;

// The access object behind each file: Read is inline in CFile and goes through it. One per file object, taken from a
// pool; it knows its file for the log.
class FakeAccess : public CAccessItem {
public:
    int32_t Read(void* dest, uint32_t size) override {
        int64_t target[2];
        Describe(dest, target);
        if (dest == _AndyIFFGLL::sChunk) { target[0] = 12; target[1] = 0; }
        else if (dest == Header(true)) { target[0] = 13; target[1] = 0; }
        else if (dest == _AndyGIFGLL::sPalette) { target[0] = 16; target[1] = 0; }
        else if (gKind == 1 && dest == reinterpret_cast<_AndyGIFGLL*>(gGllStore)->mByteBuff) { target[0] = 17; target[1] = 0; }
        else if (target[0] == -1) { target[0] = -3; target[1] = 0; }
        ev("CFile.Read", {lab(mOwner), size, target[0], target[1]});
        int64_t remaining = gStreamSize - gStreamPos;
        if (remaining < 0) remaining = 0;
        const int64_t count = size < remaining ? size : remaining;
        if (dest != nullptr && count > 0) std::memcpy(dest, gStream + gStreamPos, static_cast<std::size_t>(count));
        gStreamPos += count;
        return static_cast<int32_t>(ret("CFile.Read", count));
    }
    const CFile* mOwner = nullptr;
};
constexpr int kAccessPool = 16;
FakeAccess gAccess[kAccessPool];
int gAccessUsed;

CWadFile gFile0;   // the file the script hands to the functions that take one


}  // namespace

// ---- the file layer
namespace bfl {
namespace FileSystem {

CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(-1); }
int32_t CAccessItem::GetMacFileReference() { return -1; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

CFile::CFile() : mImplementation(nullptr) {
    ev("CFile.ctor", {newlab(this)});
    if (gAccessUsed < kAccessPool) {
        gAccess[gAccessUsed].mOwner = this;
        mImplementation = &gAccess[gAccessUsed++];
    }
}
CFile::~CFile() { ev("CFile.dtor", {lab(this)}); }
void CFile::Close() {}
// Positions within the stream; a position outside it fails and leaves the old one.
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    const int32_t signedOffset = static_cast<int32_t>(offset);
    ev("CFile.Seek", {lab(this), signedOffset, whence});
    int64_t target;
    if (whence == 0) target = signedOffset;
    else if (whence == 1) target = gStreamPos + signedOffset;
    else if (whence == 2) target = gStreamSize + signedOffset;
    else target = -1;
    int64_t answer = -1;
    if (target >= 0 && target <= gStreamSize) {
        gStreamPos = target;
        answer = target;
    }
    return static_cast<int32_t>(ret("CFile.Seek", answer));
}
int32_t CFile::GetSize() {
    ev("CFile.GetSize", {lab(this)});
    return static_cast<int32_t>(ret("CFile.GetSize", gStreamSize));
}
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(-1); }
int32_t CFile::GetMacFileReference() { return -1; }

CWadFile::~CWadFile() {}
// A file that opens is at its start.
TbError CWadFile::Open(const char* name, const char* storagePath) {
    ev("CWadFile.Open", {lab(this)}, {name, storagePath});
    const int32_t answer = static_cast<int32_t>(ret("CWadFile.Open", 0));
    if (answer >= 0) gStreamPos = 0;
    return TbError(answer);
}

TbError _TbBufferedFile::Open(CFile& file, uint32_t size) {
    ev("Buffered.Open", {lab(this), lab(&file), size});
    return TbError(static_cast<int32_t>(ret("Buffered.Open", 0)));
}

}  // namespace FileSystem
}  // namespace bfl

// ---- the library's heap: blocks filled with 0xa5, a margin after each (the run-length reader can write past a line)
void* LbMem_Alloc(uint32_t size) {
    if (ret("LbMem_Alloc", 1) == 0) {
        ev("LbMem_Alloc", {size, 0});
        return nullptr;
    }
    auto* block = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(size) + kMargin + 1));
    std::memset(block, 0xa5, static_cast<std::size_t>(size) + kMargin);
    const int32_t label = newlab(block);
    Register(block, static_cast<std::size_t>(size) + kMargin, label);
    ev("LbMem_Alloc", {size, label});
    return block;
}
// (A pointer that is not one of the fake heap's blocks is only logged: the script may free the world's pixel blocks.)
void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    if (Release(block)) forget(block);
}

// ---- the rectangle helpers (the library's, from their listings), the surface and the pixel converter
uint8_t LbRect_Intersection(TbRect* rect, const TbRect* other) {
    ev("LbRect_Intersection", {rect->left, rect->top, rect->right, rect->bottom, other->left, other->top, other->right,
                               other->bottom});
    rect->left = other->left < rect->left ? rect->left : other->left;
    rect->top = other->top < rect->top ? rect->top : other->top;
    rect->right = rect->right < other->right ? rect->right : other->right;
    rect->bottom = rect->bottom < other->bottom ? rect->bottom : other->bottom;
    // (the answer can be set by the test: the clipped rectangle then keeps what the real function made of it)
    return static_cast<uint8_t>(ret("LbRect_Intersection", rect->left < rect->right && rect->top < rect->bottom ? 1 : 0));
}
void LbRect_Empty(TbRect* rect) {
    ev("LbRect_Empty", {rect->left, rect->top, rect->right, rect->bottom});
    rect->right = rect->left;
    rect->bottom = rect->top;
}
const TbRenderArea& TbSurface::GetRenderArea() const {
    ev("TbSurface.GetRenderArea", {lab(this)});
    return gArea;
}

CConverter::CConverter() { ev("CConverter.ctor", {newlab(this)}); }
void CConverter::Init(const TbPixFormat* destination, const TbPixFormat* source, const TbPalette* palette, uint32_t flags) {
    ev("CConverter.Init", {lab(this), lab(destination), lab(source), lab(palette), flags});
}
// Every byte of the line is changed so that the converted pixels show in the picture: exclusive-or with 0x5a.
void CConverter::ConvBuf(uint8_t* source, uint8_t* dest, int32_t count) {
    ev("CConverter.ConvBuf", {lab(this), lab(source), lab(dest), count});
    for (int32_t i = 0; i < count; ++i) dest[i] = static_cast<uint8_t>(source[i] ^ 0x5a);
}
void CConverter::Release() { ev("CConverter.Release", {lab(this)}); }


// calllog owns the replacement operators; observers retain arrays for byte comparison.
namespace {
void ObserveAllocation(std::size_t size, bool array, void* p) {
    if (!array || !calllog::on()) return;
    std::memset(p, 0xa5, size);
    const int label = newlab(p);
    Register(p, size, label);
    ev("new[]", {static_cast<int64_t>(size), label});
}
bool ObserveRelease(void* p, bool array) { return array && Release(p); }
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_RELEASE_OBSERVER ObserveRelease
#include "calllog.cpp"

extern "C" {
void tpw_igt_begin(int32_t kind, const uint8_t* stream, int32_t count, const uint8_t* fill, int32_t fillCount) {
    for (int i = 0; i < gRegistryCount; ++i) if (gRegistry[i].label >= 100) std::free(const_cast<unsigned char*>(gRegistry[i].start));
    gRegistryCount = 0;
    gKind = kind;
    std::memset(gLoaderStore, 0xa5, sizeof gLoaderStore);
    std::memset(gGllStore, 0xa5, sizeof gGllStore);
    std::memset(&gArea, 0xa5, sizeof gArea);
    std::memset(gBlockStore, 0xa5, sizeof gBlockStore);
    std::memset(_AndyIFFGLL::sChunk, 0, 8);
    std::memset(_AndyGIFGLL::sPalette, 0, 768);
    _AndyGIFGLL::sB1 = 0;
    gStreamSize = std::min(count, kStreamMax);
    gStreamPos = 0;
    if (count) std::memcpy(gStream, stream, gStreamSize);
    if (fillCount) std::memcpy(gBlockStore[1], fill, std::min(fillCount, kBlockSize));
    gAccessUsed = 1;
    gAccess[0].mOwner = &gFile0;
    gFile0.mImplementation = &gAccess[0];
    Register(gBlockStore[0], kBlockSize + kMargin, 8);
    Register(gBlockStore[1], kBlockSize + kMargin, 9);
    Register(gLoaderStore, sizeof gLoaderStore, 1);
    Register(gGllStore, sizeof gGllStore, 6);
    Register(&gArea, sizeof gArea, 2);
    if (kind == 0) { new (gLoaderStore) TbIFFLoader; new (gGllStore) _AndyIFFGLL; }
    else if (kind == 1) { new (gLoaderStore) TbGIFLoader; new (gGllStore) _AndyGIFGLL; }
    else if (kind == 2) { new (gLoaderStore) TbTIMLoader; new (gGllStore) _TimGLL; }
}
void tpw_igt_labels() {
    tpw_cl_label(2, &gArea);
    tpw_cl_label(5, &Loader()->mArea);
    tpw_cl_label(11, Loader()->mPalette);
    tpw_cl_label(6, gGllStore);
    tpw_cl_label(7, &gFile0);
    tpw_cl_label(8, gBlockStore[0]);
    tpw_cl_label(9, gBlockStore[1]);
}
void* tpw_igt_addr(int field) {
    if (field == 100) return &gArea;
    if (field == 101) return &Loader()->mArea;
    if (field == 102) return Loader()->mPalette;
    if (field == 103) return &Loader()->mHasPalette;
    if (field == 104) return Header(true);
    if (field == 105) return Header(false);
    if (field == 106) return _AndyIFFGLL::sChunk;
    if (field == 107) return _AndyGIFGLL::sPalette;
    if (field == 108) return &_AndyGIFGLL::sB1;
    if (field == 109) return reinterpret_cast<_AndyGIFGLL*>(gGllStore)->mByteBuff;
    if (field == 110) return gBlockStore[0];
    if (field == 111) return gBlockStore[1];
    if (field == 112) return gLoaderStore;
    if (field == 113) return gGllStore;
    if (field == 114) return &gStreamPos;
    if (field == 115) return const_cast<TbPixFormat*>(&LbgfsaveTim16);
    if (field == 116) return const_cast<TbPixFormat*>(&LbgfsaveTim24);
    if (gKind == 0) { auto* r = reinterpret_cast<_AndyIFFGLL*>(gGllStore); switch (field) {
    case 0: return &r->mLine;
    case 1: return &r->mLineSize;
    case 2: return &r->mDestBytes;
    case 3: return &r->mSourceBytes;
    case 4: return &r->mBottomUp;
    case 5: return &r->mDest;
    case 6: return &r->mDestStep;
    case 7: return &r->mRead;
    case 8: return &r->mPlanes;
    case 9: return &r->mPlaneBytes;
    case 10: return &r->mLinesRead;
    case 11: return &r->mLineBytes;
    case 12: return &r->mHeight;
    } }
    if (gKind == 1) { auto* r = reinterpret_cast<_AndyGIFGLL*>(gGllStore); switch (field) {
    case 0: return &r->mLine;
    case 1: return &r->mLineSize;
    case 2: return &r->mDestBytes;
    case 3: return &r->mSourceBytes;
    case 4: return &r->mBottomUp;
    case 5: return &r->mDest;
    case 6: return &r->mDestStep;
    case 7: return &r->mInterlaced;
    case 8: return &r->mNewCodes;
    case 9: return &r->mSize;
    case 10: return &r->mFc;
    case 11: return &r->mOc;
    case 12: return &r->mPalette;
    case 13: return &r->mPass;
    case 14: return &r->mRow;
    case 15: return &r->mFirstDest;
    case 16: return &r->mAvailBytes;
    case 17: return &r->mBitsLeft;
    case 18: return &r->mCurrSize;
    case 19: return &r->mClear;
    case 20: return &r->mTopSlot;
    case 21: return &r->mEnding;
    case 22: return &r->mSlot;
    case 23: return &r->mBytes;
    case 24: return &r->mStack;
    case 25: return &r->mSp;
    case 26: return &r->mSuffix;
    case 27: return &r->mPrefix;
    } }
    if (gKind == 2) { auto* r = reinterpret_cast<_TimGLL*>(gGllStore); switch (field) {
    case 0: return &r->mLine;
    case 1: return &r->mLineSize;
    case 2: return &r->mDestBytes;
    case 3: return &r->mSourceBytes;
    case 4: return &r->mBottomUp;
    case 5: return &r->mDest;
    case 6: return &r->mDestStep;
    case 7: return &r->mLinesLeft;
    case 8: return &r->mLineBytes;
    case 9: return &r->mHeight;
    case 10: return &r->mWidth;
    case 11: return &r->mWordWidth;
    } }
    return nullptr;
}
void* tpw_igt_area_field(int which, int field) {
    TbRenderArea* a = which ? &Loader()->mArea : &gArea;
    if (field < 4) return a->mF00 + 4 * field;
    switch (field) {
    case 4: return &a->mDepth;
    case 5: return &a->mPalettised;
    case 6: return &a->mData;
    case 7: return &a->mWidth;
    case 8: return &a->mPictureHeight;
    default: return &a->mStride;
    }
}
int64_t tpw_igt_op(int code) {
    switch (gKind) {
    case 0: {
        auto* r = reinterpret_cast<_AndyIFFGLL*>(gGllStore);
        auto* l = reinterpret_cast<TbIFFLoader*>(gLoaderStore);
        switch (code) {
        case 0: return r->ReadHeaderInfo(gFile0, l).mCode;
        case 1: return r->ReadData(gArea, gFile0, l).mCode;
        case 2: return r->ReadFirstLine(gFile0).mCode;
        case 3: return r->ReadNextLine(gFile0).mCode;
        case 4: return r->GetSourceLineLength();
        case 6: return l->ReadHeaderInfo(gFile0, gArea).mCode;
        case 7: return l->ReadData(gArea, gFile0, gArea).mCode;
        case 8: return l->ValidImage(gFile0);
        } break; }
    case 1: {
        auto* r = reinterpret_cast<_AndyGIFGLL*>(gGllStore);
        auto* l = reinterpret_cast<TbGIFLoader*>(gLoaderStore);
        switch (code) {
        case 0: return r->ReadHeaderInfo(gFile0, l).mCode;
        case 1: return r->ReadData(gArea, gFile0, l).mCode;
        case 2: return r->ReadFirstLine(gFile0).mCode;
        case 3: return r->ReadNextLine(gFile0).mCode;
        case 4: return r->GetSourceLineLength();
        case 6: return l->ReadHeaderInfo(gFile0, gArea).mCode;
        case 7: return l->ReadData(gArea, gFile0, gArea).mCode;
        case 8: return l->ValidImage(gFile0);
        case 9: return r->_GifGetNextCode(gFile0);
        case 10: return r->_GifReadPalette(gFile0, reinterpret_cast<TbPalette*>(l->mPalette)).mCode;
        case 11: r->WriteLine(); return 0;
        } break; }
    case 2: {
        auto* r = reinterpret_cast<_TimGLL*>(gGllStore);
        auto* l = reinterpret_cast<TbTIMLoader*>(gLoaderStore);
        switch (code) {
        case 0: return r->ReadHeaderInfo(gFile0, l).mCode;
        case 1: return r->ReadData(gArea, gFile0, l).mCode;
        case 2: return r->ReadFirstLine(gFile0).mCode;
        case 3: return r->ReadNextLine(gFile0).mCode;
        case 4: return r->GetSourceLineLength();
        case 6: return l->ReadHeaderInfo(gFile0, gArea).mCode;
        case 7: return l->ReadData(gArea, gFile0, gArea).mCode;
        case 8: return l->ValidImage(gFile0);
        } break; }
    } return 0;
}
// Every byte, including the margin, of both world blocks and every heap allocation (freed or live).
int32_t tpw_igt_memory(int32_t index, uint8_t* out, int64_t* meta) {
    int n = 0;
    for (int i = 0; i < gRegistryCount; ++i) {
        const Block& b = gRegistry[i];
        if (b.label != 8 && b.label != 9 && b.label < 100) continue;
        if (n++ != index) continue;
        meta[0] = b.label;
        meta[1] = b.extent;
        meta[2] = b.freed;
        std::memcpy(out, b.start, b.extent);
        return 1;
    }
    return 0;
}

void* tpw_igt_heap_addr(int label) {
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].label == label) return const_cast<unsigned char*>(gRegistry[i].start);
    }
    return nullptr;
}

}
