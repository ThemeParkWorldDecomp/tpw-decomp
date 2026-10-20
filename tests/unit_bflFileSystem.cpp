#include "bfl_FileSystem.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "calllog.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <algorithm>

namespace {
void observe_allocation(std::size_t size, bool array, void* p);
void snapshot_access(bfl::FileSystem::CAccessItem* p);
void snapshot_storage(bfl::FileSystem::_TbStorageAccess* p);
}
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"
#define BFL_EXTERN_LOG(...) calllog::ev(__VA_ARGS__)
#define BFL_LAB(p) calllog::lab(p)
#define BFL_RETURN(name) static_cast<int32_t>(calllog::ret(name))
#define BFL_ACCESS_DESTROY(p) snapshot_access(p)
#define BFL_STORAGE_DESTROY(p) snapshot_storage(p)
#include "bfl_external_fakes.h"
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text);
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char& ch);
#include "fake_strings.h"
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, text != nullptr ? text : "", text != nullptr ? std::strlen(text) : 0);
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char& ch) {
    calllog::ev("String::appendChar", {calllog::lab(this), static_cast<uint8_t>(ch)});
    const char text[2] = {ch, 0};
    return operator+=(text);
}

using namespace bfl::FileSystem;
using calllog::ev;
using calllog::lab;
using calllog::ret;

namespace {
int32_t gTypes[512];
bool gDestroyed[512];
uint32_t gSaved[512][12];
char gExpanded[260] = "Root:";
int32_t gAttrs;
int32_t gOperation;

void label_members(int32_t label, void* p, int32_t type) {
    if (type <= 4) {
        auto* access = static_cast<CAccessItem*>(p);
        tpw_cl_label(200 + label, &access->mReferences);
    } else if (type == 5 || type == 6) {
        auto* access = static_cast<_TbStorageAccess*>(p);
        tpw_cl_label(200 + label, &access->mReferences);
        tpw_cl_label(300 + label, &access->mSemaphore);
        tpw_cl_label(400 + label, &access->mRoot);
    }
}

void observe_allocation(std::size_t size, bool array, void* p) {
    if (!calllog::on()) return;
    int32_t type = -1;
    if (!array && gOperation == 30 && size == sizeof(_TbBufferAccess)) type = 1;
    if (!array && (gOperation == 11 || gOperation == 29) &&
        size == sizeof(_TbFileStorageAccess)) type = 6;
    if (!array && type == -1) return; // implementation details inside functional string fakes
    const int32_t label = p != nullptr ? calllog::newlab(p) : 0;
    ev(array ? "new[]" : "new", {array ? static_cast<int64_t>(size) : 40, label});
    if (!array && p != nullptr) {
        gTypes[label] = type;
        label_members(label, p, type);
    }
}

}

void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return -1; }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    void* p = calllog::on() && ret("allocation failure") ? nullptr : std::malloc(size != 0 ? size : 1);
    if (p != nullptr) std::memset(p, 0x5a, size);
    observe_allocation(size, false, p);
    return p;
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    void* p = calllog::on() && ret("array failure") ? nullptr : std::malloc(size != 0 ? size : 1);
    observe_allocation(size, true, p);
    return p;
}

namespace {

void state(void* p, int32_t type, uint32_t* out) {
    std::fill(out, out + 12, 0);
    if (type <= 4) {
        auto* access = static_cast<CAccessItem*>(p);
        out[0] = lab(access->mNext);
        out[1] = static_cast<uint32_t>(access->mReferences);
        if (type == 1) {
            auto* buffer = static_cast<_TbBufferAccess*>(p);
            out[2] = lab(buffer->mBuffer);
            out[3] = lab(buffer->mCacheBuffer);
            out[4] = buffer->mPosition;
            out[5] = buffer->mCacheStart;
            out[6] = buffer->mCacheEnd;
            out[7] = buffer->mBaseOffset;
            out[8] = buffer->mBufferSize;
        } else if (type == 3 || type == 4)
            out[2] = static_cast<_TbQuickPosAccess*>(p)->mPosition;
    } else if (type == 5 || type == 6) {
        auto* access = static_cast<_TbStorageAccess*>(p);
        out[0] = lab(access->mNext);
        out[1] = static_cast<uint32_t>(access->mReferences);
        out[2] = access->mSemaphore.mState;
        out[3] = access->mRoot.mLength;
        out[4] = access->mRoot.mCapacity;
        out[5] = access->mRoot.mGrowBy;
        if (type == 6) out[6] = lab(static_cast<_TbFileStorageAccess*>(p)->mFindHandle);
    } else if (type == 7 || type == 9 || type == 10)
        out[0] = lab(static_cast<CFile*>(p)->mImplementation);
    else
        out[0] = lab(static_cast<CFileStorage*>(p)->mAccess);
}

void snapshot_access(CAccessItem* p) {
    const int32_t label = lab(p);
    if (label > 0 && label < 512) {
        state(p, gTypes[label], gSaved[label]);
        gDestroyed[label] = true;
    }
}
void snapshot_storage(_TbStorageAccess* p) {
    const int32_t label = lab(p);
    if (label > 0 && label < 512) {
        state(p, gTypes[label], gSaved[label]);
        gDestroyed[label] = true;
    }
}
}

namespace NS_MacDoze {
int32_t InterlockedIncrement(int32_t* value) {
    ev("InterlockedIncrement", {lab(value), *value});
    *value = static_cast<int32_t>(static_cast<uint32_t>(*value) + 1);
    return *value;
}
int32_t InterlockedDecrement(int32_t* value) {
    ev("InterlockedDecrement", {lab(value), *value});
    *value = static_cast<int32_t>(static_cast<uint32_t>(*value) - 1);
    return *value;
}
uint32_t GetFileAttributes(const char* path) {
    ev("GetFileAttributes", {}, {path});
    return static_cast<uint32_t>(gAttrs);
}
int32_t IsDBCSLeadByte(uint8_t ch) {
    ev("IsDBCSLeadByte", {ch});
    return static_cast<int32_t>(ret("IsDBCSLeadByte"));
}
}
TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* name) {
    ev("LbFile_ToProjectFilename", {size}, {name});
    std::strcpy(out, gExpanded);
    return TbError(static_cast<int32_t>(ret("LbFile_ToProjectFilename")));
}

// The functional strings use code units and borrowed views, not Mac field offsets.
template<> int32_t TbStringBase<char>::FindReverseOneOf(const char* mask, uint32_t start) const {
    ev("String::FindReverseOneOf", {start}, {mData, mask});
    if (mLength == 0) return -1;
    int32_t position = static_cast<int32_t>(std::min(start, static_cast<uint32_t>(mLength - 1)));
    for (; position >= 0; --position)
        if (std::strchr(mask, mData[position]) != nullptr) return position;
    return -1;
}

extern "C" {
void tpw_bfl_config(const char* expanded, int32_t attrs) {
    std::strcpy(gExpanded, expanded);
    gAttrs = attrs;
    std::fill(gDestroyed, gDestroyed + 512, false);
    std::fill(gTypes, gTypes + 512, -1);
}
void* tpw_bfl_new(int32_t type) {
    switch (type) {
    case 0: return new CAccessItem();
    case 1: return new _TbBufferAccess();
    case 3: return new _TbQuickPosAccess();
    case 4: return new _TbSharedAccess();
    case 5: return new _TbStorageAccess();
    case 6: return new _TbFileStorageAccess();
    case 7: return new CFile();
    case 8: return new CFileStorage();
    case 9: return new CDiscFile();
    case 10: return new _TbBufferedFile();
    case 11: return new CDiscFileStorage();
    }
    return nullptr;
}
void tpw_bfl_seed(void* p, int32_t type, int32_t label, void* next, uint32_t references,
                  uint32_t seed, void* data) {
    tpw_cl_label(label, p);
    gTypes[label] = type;
    label_members(label, p, type);
    if (type <= 4) {
        auto* access = static_cast<CAccessItem*>(p);
        access->mNext = static_cast<CAccessItem*>(next);
        access->mReferences = static_cast<int32_t>(references);
        if (type == 1) {
            auto* buffer = static_cast<_TbBufferAccess*>(p);
            buffer->mBuffer = buffer->mCacheBuffer = static_cast<uint8_t*>(data);
            buffer->mPosition = buffer->mCacheStart = buffer->mCacheEnd = seed;
            buffer->mBaseOffset = buffer->mBufferSize = seed;
        } else if (type == 3 || type == 4)
            static_cast<_TbQuickPosAccess*>(p)->mPosition = seed;
    } else if (type == 5 || type == 6) {
        auto* access = static_cast<_TbStorageAccess*>(p);
        access->mNext = static_cast<_TbStorageAccess*>(next);
        access->mReferences = static_cast<int32_t>(references);
        access->mSemaphore.mState = seed;
        access->mRoot = "old";
        if (type == 6) static_cast<_TbFileStorageAccess*>(p)->mFindHandle = data;
    } else if (type == 7 || type == 9 || type == 10)
        static_cast<CFile*>(p)->mImplementation = static_cast<CAccessItem*>(next);
    else
        static_cast<CFileStorage*>(p)->mAccess = static_cast<_TbStorageAccess*>(next);
}
void tpw_bfl_state(void* p, int32_t type, int32_t label, uint32_t* out, char* text) {
    if (gDestroyed[label]) std::copy(gSaved[label], gSaved[label] + 12, out);
    else state(p, type, out);
    text[0] = 0;
    if (type == 5 || type == 6) std::strcpy(text, static_cast<_TbStorageAccess*>(p)->mRoot.mData);
}
// Allocation labels remain inspectable after fake deleting destructors snapshot them.
int32_t tpw_bfl_born(int32_t label, uint32_t* out, int32_t* type, char* text) {
    *type = gTypes[label];
    if (gDestroyed[label]) {
        std::copy(gSaved[label], gSaved[label] + 12, out);
        text[0] = 0;
        return 1;
    }
    if (gObjects[label] == nullptr || (*type != 1 && *type != 6)) return 0;
    tpw_bfl_state(const_cast<void*>(gObjects[label]), *type, label, out, text);
    return 1;
}

int32_t tpw_bfl_call(int32_t op, void* p, void* other, void* file, CFileInfo* info,
                     const char* name, uint32_t x, uint32_t y, void* data, int32_t flag) {
    gOperation = op;
    auto* access = static_cast<CAccessItem*>(p);
    auto* storage = static_cast<CFileStorage*>(p);
    auto* openfile = static_cast<CFile*>(p);
    switch (op) {
    case 0: new (p) _TbBufferAccess(); break;
    case 1: return static_cast<_TbBufferAccess*>(p)->Open(x);
    case 2: new (p) _TbFileStorageAccess(); break;
    case 3: return static_cast<_TbFileStorageAccess*>(p)->Open(name);
    case 4: new (p) _TbQuickPosAccess(); break;
    case 5: new (p) _TbSharedAccess(); break;
    case 6: new (p) _TbStorageAccess(); break;
    case 7: static_cast<_TbStorageAccess*>(p)->AddRef(); break;
    case 8: new (p) CAccessItem(); break;
    case 9: access->AddRef(); break;
    case 10: return access->Release();
    case 11: return static_cast<CDiscFileStorage*>(p)->Open(name);
    case 12: new (p) CFile(); break;
    case 13: openfile->CFile::~CFile(); break;
    case 14: openfile->Close(); break;
    case 15: return openfile->Seek(x, y);
    case 16: return openfile->GetSize();
    case 17: return openfile->GetPos();
    case 18: return lab(openfile->GetAccessClass());
    case 19: openfile->AttachAccessClass(static_cast<CAccessItem*>(other)); break;
    case 20: new (p) CFileStorage(); break;
    case 21: storage->CFileStorage::~CFileStorage(); break;
    case 22: *storage = *static_cast<CFileStorage*>(other); return lab(storage);
    case 23: return storage->FindFirst(name, *info);
    case 24: return storage->FindNext(*info);
    case 25: return storage->OpenFile(*static_cast<CFile*>(file), name, x, data);
    case 26: storage->AttachAccessClass(static_cast<_TbStorageAccess*>(other)); break;
    case 27: new (p) CDiscFile(); break;
    case 28: static_cast<CDiscFile*>(p)->CDiscFile::~CDiscFile(); break;
    case 29: return static_cast<CDiscFile*>(p)->Open(name, x);
    case 30: return static_cast<_TbBufferedFile*>(p)->Open(*static_cast<CFile*>(file), x);
    }
    // Cache named fields before the deleting-destructor wrapper releases storage.
    if ((op == 13 || op == 21 || op == 28) && static_cast<int16_t>(flag) > 0) {
        const int32_t label = lab(p);
        state(p, gTypes[label], gSaved[label]);
        gDestroyed[label] = true;
        ::operator delete(p);
    }
    return 0;
}
}
