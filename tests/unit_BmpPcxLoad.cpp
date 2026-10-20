// Unit w220: recording file, heap and converter fakes for the BMP and PCX loaders.
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
alignas(16) unsigned char gGllStore[0x100];
unsigned char gBlockStore[2][kBlockSize + kMargin];
TbRenderArea gArea;
int gKind;
TbGraphicFileLoader* Loader() {
    if (gKind == 0) return reinterpret_cast<TbBMPLoader*>(gLoaderStore);
    return reinterpret_cast<TbPCXLoader*>(gLoaderStore);
}
_AndyGLL* Reader() {
    if (gKind == 0) return reinterpret_cast<_AndyBMPGLL*>(gGllStore);
    return reinterpret_cast<_AndyPCXGLL*>(gGllStore);
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
        const void* header = gKind == 0 ? static_cast<void*>(reinterpret_cast<TbBMPLoader*>(gLoaderStore)->mInfo)
                                      : static_cast<void*>(reinterpret_cast<TbPCXLoader*>(gLoaderStore)->mHeader);
        if (dest == _AndyBMPGLL::sFileHeader) {
            target[0] = 12;
            target[1] = 0;
        } else if (dest == header) {
            target[0] = 13;
            target[1] = 0;
        } else if (gKind == 1 && dest == &reinterpret_cast<_AndyPCXGLL*>(gGllStore)->mRunCount) {
            target[0] = 14;
            target[1] = 0;
        } else if (gKind == 1 && dest == &reinterpret_cast<_AndyPCXGLL*>(gGllStore)->mRunValue) {
            target[0] = 15;
            target[1] = 0;
        } else if (dest == Loader()->mPalette) {
            target[0] = 16;
            target[1] = 0;
        } else if (target[0] == -1) {
            target[0] = -3;  // local bytes of ValidImage
            target[1] = 0;
        }
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


extern "C" {

// Fresh objects over the same 0xa5 fill as the original's world.
void tpw_bpl_begin(int32_t kind, const uint8_t* stream, int32_t count, const uint8_t* fill, int32_t fillCount) {
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].label >= 100) std::free(const_cast<unsigned char*>(gRegistry[i].start));
    }
    gRegistryCount = 0;
    gKind = kind;
    std::memset(gLoaderStore, 0xa5, sizeof gLoaderStore);
    std::memset(gGllStore, 0xa5, sizeof gGllStore);
    std::memset(&gArea, 0xa5, sizeof gArea);
    std::memset(gBlockStore, 0xa5, sizeof gBlockStore);
    std::memset(_AndyBMPGLL::sFileHeader, 0, 0xe);
    gStreamSize = std::min(count, kStreamMax);
    gStreamPos = 0;
    if (count > 0) std::memcpy(gStream, stream, gStreamSize);
    if (fillCount > 0) std::memcpy(gBlockStore[1], fill, std::min(fillCount, kBlockSize));
    gAccessUsed = 1;
    gAccess[0].mOwner = &gFile0;
    gFile0.mImplementation = &gAccess[0];
    Register(gBlockStore[0], kBlockSize + kMargin, 8);
    Register(gBlockStore[1], kBlockSize + kMargin, 9);
    Register(gLoaderStore, sizeof gLoaderStore, 1);
    Register(gGllStore, sizeof gGllStore, 6);
    Register(&gArea, sizeof gArea, 2);
    if (kind == 0) {
        new (gLoaderStore) TbBMPLoader;
        new (gGllStore) _AndyBMPGLL;
    } else {
        new (gLoaderStore) TbPCXLoader;
        new (gGllStore) _AndyPCXGLL;
    }
}

// Install labels after calllog's reset; loader area and loader share their native address.
void tpw_bpl_labels() {
    tpw_cl_label(2, &gArea);
    tpw_cl_label(5, &Loader()->mArea);
    tpw_cl_label(11, Loader()->mPalette);
    tpw_cl_label(6, gGllStore);
    tpw_cl_label(7, &gFile0);
    tpw_cl_label(8, gBlockStore[0]);
    tpw_cl_label(9, gBlockStore[1]);
}

// Operation codes 0..8 are the unit functions; 40..44 set initial state.
int64_t tpw_bpl_op(int32_t code, const int64_t* a) {
    auto* bmp = reinterpret_cast<_AndyBMPGLL*>(gGllStore);
    auto* pcx = reinterpret_cast<_AndyPCXGLL*>(gGllStore);
    auto* bl = reinterpret_cast<TbBMPLoader*>(gLoaderStore);
    auto* pl = reinterpret_cast<TbPCXLoader*>(gLoaderStore);
    switch (code) {
    case 0: return gKind == 0 ? bmp->ReadHeaderInfo(gFile0, bl).mCode : pcx->ReadHeaderInfo(gFile0, pl).mCode;
    case 1: return gKind == 0 ? bmp->ReadData(gArea, gFile0, bl).mCode : pcx->ReadData(gArea, gFile0, pl).mCode;
    case 2: return Reader()->ReadFirstLine(gFile0).mCode;
    case 3: return Reader()->ReadNextLine(gFile0).mCode;
    case 4: return Reader()->GetSourceLineLength();
    case 5: return bmp->BMPLoadRLE(gFile0).mCode;
    case 6: return gKind == 0 ? bl->ReadHeaderInfo(gFile0, gArea).mCode : pl->ReadHeaderInfo(gFile0, gArea).mCode;
    case 7: return gKind == 0 ? bl->ReadData(gArea, gFile0, gArea).mCode : pl->ReadData(gArea, gFile0, gArea).mCode;
    case 8: return gKind == 0 ? bl->ValidImage(gFile0) : pl->ValidImage(gFile0);
    case 40: {  // area (0 destination, 1 loader), field, value, offset
        TbRenderArea& area = a[0] == 0 ? gArea : Loader()->mArea;
        const uint32_t value = static_cast<uint32_t>(a[2]);
        if (a[1] < 4) std::memcpy(area.mF00 + 4 * a[1], &value, 4);
        else if (a[1] == 4) area.mDepth = value;
        else if (a[1] == 5) area.mPalettised = value;
        else if (a[1] == 6) area.mData = a[2] ? gBlockStore[a[2] - 1] + a[3] : nullptr;
        else if (a[1] == 7) area.mWidth = value;
        else if (a[1] == 8) area.mPictureHeight = value;
        else area.mStride = value;
        return 0;
    }
    case 41: {  // reader field, value, offset; pointers name world blocks 1 or 2
        _AndyGLL* g = Reader();
        uint8_t* pointer = a[1] >= 1 && a[1] <= 2 ? gBlockStore[a[1] - 1] + a[2] : nullptr;
        // Only form a pointer for pointer fields; scalar values may be arbitrary words.
        switch (a[0]) {
        case 0: g->mLine = pointer; break;
        case 1: g->mLineSize = a[1]; break;
        case 2: g->mDestBytes = a[1]; break;
        case 3: g->mSourceBytes = a[1]; break;
        case 4: g->mBottomUp = a[1]; break;
        case 5: g->mDest = pointer; break;
        case 6: g->mDestStep = a[1]; break;
        default:
            if (gKind == 0) {
                switch (a[0]) {
                case 7: bmp->mLinesLeft = a[1]; break;
                case 8: bmp->mLineBytes = a[1]; break;
                case 9: bmp->mRleData = pointer; break;
                case 10: bmp->mRleCursor = pointer; break;
                case 11: bmp->mSkipLines = a[1]; break;
                case 12: bmp->mNextX = a[1]; break;
                case 13: bmp->mRleEnd = a[1]; break;
                }
            } else {
                switch (a[0]) {
                case 7: pcx->mDecodeBytes = a[1]; break;
                case 8: pcx->mLinesLeft = a[1]; break;
                case 9: pcx->mRunCount = a[1]; break;
                case 10: pcx->mRunValue = a[1]; break;
                }
            }
            break;
        }
        return 0;
    }
    case 42: Loader()->mPalette[a[0]] = a[1]; return 0;
    case 43: Loader()->mHasPalette = a[0]; return 0;
    case 44: gStreamPos = a[0]; return 0;
    case 45: {  // raw header byte: 0 reader, 1 loader
        uint8_t* p;
        if (gKind == 0) p = a[0] == 0 ? bmp->mInfo : bl->mInfo;
        else p = a[0] == 0 ? pcx->mHeader : pl->mHeader;
        p[a[1]] = a[2];
        return 0;
    }
    }
    return 0;
}

// Scalars and raw headers, with native pointers represented by block labels and offsets.
int32_t tpw_bpl_dump(int64_t* out) {
    int64_t* start = out;
    auto put = [&](uint32_t v) { *out++ = v; };
    auto pointer = [&](const void* p) { Describe(p, out); out += 2; };
    for (const TbRenderArea* a : {&gArea, &Loader()->mArea}) {
        for (int i = 0; i < 4; ++i) {
            uint32_t word;
            std::memcpy(&word, a->mF00 + 4 * i, 4);
            put(word);
        }
        put(a->mDepth);
        put(a->mPalettised);
        pointer(a->mData);
        put(a->mWidth);
        put(a->mPictureHeight);
        put(a->mStride);
    }
    put(Loader()->mHasPalette);
    for (uint8_t b : Loader()->mPalette) put(b);
    const uint8_t* lh = gKind == 0 ? reinterpret_cast<TbBMPLoader*>(gLoaderStore)->mInfo : reinterpret_cast<TbPCXLoader*>(gLoaderStore)->mHeader;
    const int headerSize = gKind == 0 ? 0x28 : 0x80;
    for (int i = 0; i < headerSize; ++i) put(lh[i]);
    const _AndyGLL* g = Reader();
    pointer(g->mLine);
    put(g->mLineSize);
    put(g->mDestBytes);
    put(g->mSourceBytes);
    put(g->mBottomUp);
    pointer(g->mDest);
    put(g->mDestStep);
    const uint8_t* rh;
    if (gKind == 0) {
        const auto* b = reinterpret_cast<_AndyBMPGLL*>(gGllStore);
        put(b->mLinesLeft);
        put(b->mLineBytes);
        pointer(b->mRleData);
        pointer(b->mRleCursor);
        put(b->mSkipLines);
        put(b->mNextX);
        put(b->mRleEnd);
        rh = b->mInfo;
    } else {
        const auto* p = reinterpret_cast<_AndyPCXGLL*>(gGllStore);
        put(p->mDecodeBytes);
        put(p->mLinesLeft);
        put(p->mRunCount);
        put(p->mRunValue);
        rh = p->mHeader;
    }
    for (int i = 0; i < headerSize; ++i) put(rh[i]);
    for (uint8_t b : _AndyBMPGLL::sFileHeader) put(b);
    put(gStreamPos);
    return static_cast<int32_t>(out - start);
}

// Every byte, including the margin, of both world blocks and every heap allocation (freed or live).
int32_t tpw_bpl_memory(int32_t index, uint8_t* out, int64_t* meta) {
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

}  // extern "C"
