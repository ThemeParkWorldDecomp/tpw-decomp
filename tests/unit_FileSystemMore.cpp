// Unit w222: file-system leftovers and UTF-16 conversions. The platform fakes below
// the real disc layer are the template's existing disc fakes.
#define TPW_REAL_FILE_ACCESS
#define LbFile_ToProjectFilename TemplateToProjectFilename
#include "unit_bflDiscAccess.cpp"
#undef LbFile_ToProjectFilename
#include "bfl_WadFile.h"
#include "bfl_Strings.h"
#include "bfl_ZLib.h"
#include <new>
#include <cstddef>

namespace {
void FsmAllocation(std::size_t, bool, void*);
}
#define TPW_ALLOCATION_OBSERVER FsmAllocation
#include "calllog.cpp"
using namespace calllog;

namespace {
int gKind;
int gNameOffset;
uint32_t gSeed;
const char* gEntry;
const char* gFull;
void* gOutput;
void* gInput;
_LbSemaphore gSemaphore;
bool gTraceCopy;
bool gTraceBusy;
_TbMemFileAccess* gMemory;

struct Span {
    int64_t label;
    uint32_t offset;
    std::size_t room;
};

// Copy pointers are buffer identities plus byte displacements, including block data.
Span CopySpan(const void* p) {
    uintptr_t value = reinterpret_cast<uintptr_t>(p);
    for (void* base : {gInput, gOutput}) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(base);
        if (value >= begin && value <= begin + 2048) return {lab(base), static_cast<uint32_t>(value - begin), begin + 2048 - value};
    }
    for (auto* block = gMemory->mFirst; block != nullptr; block = block->mNext) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(block->mData);
        if (value >= begin && value <= begin + gMemory->mBlockSize)
            return {lab(block), static_cast<uint32_t>(value - begin), begin + gMemory->mBlockSize - value};
    }
    return {lab(p), 0, 0};
}

// Every allocation is named, and native object and block headers are normalized.
void FsmAllocation(std::size_t size, bool array, void* p) {
    if (!on()) return;
    if (array && gKind == 0) size -= sizeof(_TbMemFileAccess::Block) - 20;
    if (!array && size == sizeof(_TbMemFileAccess)) size = 44;
    if (!array && size == sizeof(_TbFileStorageAccess)) size = 40;
    if (!array && size == sizeof(_TbWadStorageAccess)) size = 224;
    ev(array ? "new[]" : "new", {static_cast<int64_t>(size), newlab(p)});
}

// The char storage slots write every entry word and retain an interior name pointer.
class Storage : public CFileStorage {
public:
    void Fill(CFileInfo& info) {
        info.mF00 = gSeed;
        info.mAttributes = gSeed + 1;
        for (int i = 0; i < 24; ++i) info.mF08[i] = (gSeed + i * 7) & 255;
        std::strcpy(info.mNameBuffer, gEntry);
        info.mName = info.mNameBuffer + gNameOffset;
    }
    TbError FindFirst(const char* name, CFileInfo& info) override {
        ev("FindFirst", {lab(this), info.mName == info.mNameBuffer, info.mNameBuffer[0]}, {name});
        Fill(info);
        return TbError(ret("storage"));
    }
    TbError FindNext(CFileInfo& info) override {
        ev("FindNext", {lab(this), info.mName == info.mNameBuffer, info.mNameBuffer[0]});
        Fill(info);
        return TbError(ret("storage"));
    }
    TbError OpenFile(CFile& file, const char* name, uint32_t mode, void* data) override {
        ev("OpenFile", {lab(this), lab(&file), mode, lab(data)}, {name});
        return TbError(ret("storage"));
    }
    TbError FullPath(char* out, const char* name, uint32_t size) override {
        ev("FullPath", {lab(this), size}, {name});
        std::strcpy(out, gFull);
        return TbError(ret("storage"));
    }
    TbError SetAttribs(const char* name, uint32_t flags) override {
        ev("SetAttribs", {lab(this), flags}, {name});
        return TbError(ret("storage"));
    }
    TbError CreateDir(const char* name) override {
        ev("CreateDir", {lab(this)}, {name});
        return TbError(ret("storage"));
    }
    TbError Delete(const char* name) override {
        ev("Delete", {lab(this)}, {name});
        return TbError(ret("storage"));
    }
    TbError Move(const char* a, const char* b) override {
        ev("Move", {lab(this)}, {a, b});
        return TbError(ret("storage"));
    }
};

// ResizeTo is virtual, including when called by the real string buffer helpers.
template<class Char> class String : public TbDynamicStringTemplate<Char> {
public:
    void ResizeTo(uint32_t n) override {
        ev("ResizeTo", {lab(this), n});
        this->mLength = n;
    }
    TbDynamicStringTemplate<Char>& operator=(const Char* p) override {
        ev("Assign", {lab(this)}, {""});
        this->mLength = 0;
        this->mData[0] = 0;
        return *this;
    }
};

// The next access layer records all arguments, including the callback's destination.
class Backend : public CAccessItem {
public:
    int32_t Read(void* p, uint32_t n) override {
        ev("Read", {lab(this), lab(p), n});
        int32_t count = ret("read", 3);
        uint32_t done = count < 0 ? 0 : std::min(n, static_cast<uint32_t>(count));
        for (uint32_t i = 0; i < done; ++i) static_cast<uint8_t*>(p)[i] = (gSeed + i) & 255;
        return count;
    }
    void AttachAccessClass(CAccessItem* p) override {
        ev("Attach", {lab(this), lab(p)});
        mNext = p;
    }
    const uint8_t* MapFile(int32_t a, int32_t b) override {
        ev("MapFile", {lab(this), a, b});
        return static_cast<uint8_t*>(gOutput);
    }
    void UnmapFile(const uint8_t* p) override { ev("UnmapFile", {lab(this), lab(p)}); }
    int32_t GetMacFileReference() override {
        ev("MacRef", {lab(this)});
        return ret("read", 3);
    }
    _LbSemaphore* GetSemaphore() override {
        ev("Semaphore", {lab(this)});
        return &gSemaphore;
    }
};
}

// Variable-size copies in the translated memory access resolve here. Huge signed
// counts are recorded in full, while the fake only touches mapped buffer bytes.
extern "C" void* memcpy(void* dest, const void* source, std::size_t size) {
    if (!gTraceCopy || gTraceBusy) return std::memmove(dest, source, size);
    Span out = CopySpan(dest);
    Span in = CopySpan(source);
    gTraceBusy = true;
    ev("memcpy", {out.label, out.offset, in.label, in.offset, static_cast<int64_t>(size)});
    gTraceBusy = false;
    return std::memmove(dest, source, std::min(size, std::min(out.room, in.room)));
}

// The template's project-path fake adds a prefix without bounds. Use the same
// bounded model on both sides so a 259-byte converted name remains testable.
TbError LbFile_ToProjectFilename(char* out, uint32_t size, const char* source) {
    ev("LbFile_ToProjectFilename", {size}, {source});
    if (size != 0) std::snprintf(out, size, "P:%s", source);
    return TbError(0);
}

// Failure queues are consumed by nothrow allocation only, as in the original imports.
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    void* p = on() && ret("allocation failure") ? nullptr : std::malloc(n ? n : 1);
    if (p) FsmAllocation(n, false, p);
    else if (on()) ev("new", {gKind == 9 ? 44 : gKind == 3 ? 40 : 224, 0});
    return p;
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
    void* p = on() && ret("array failure") ? nullptr : std::malloc(n ? n : 1);
    if (p) {
        std::memset(p, 0x5a, n);
        FsmAllocation(n, true, p);
    } else if (on()) {
        ev("new[]", {static_cast<int64_t>(gKind == 0 ? n - sizeof(_TbMemFileAccess::Block) + 20 : n), 0});
    }
    return p;
}

namespace NS_MacDoze {
// Both models count the NUL and write a truncated result before answering zero.
int32_t MultiByteToWideChar(uint32_t cp, uint32_t flags, const char* text, int32_t length, char16_t* out, int32_t size) {
    ev("MB", {cp, flags, length, out != nullptr, size}, {text});
    int32_t n = std::strlen(text) + 1;
    if (out && size) for (int i = 0; i < std::min(n, size); ++i) out[i] = static_cast<uint8_t>(text[i]);
    if (out && ret("mb_fail")) return 0;
    if (!out) return n + ret("query_extra");
    return size && size < n ? 0 : n;
}
int32_t WideCharToMultiByte(uint32_t cp, uint32_t flags, const char16_t* text, int32_t length, char* out, int32_t size,
                            const char* def, int32_t* used) {
    ev("WC", {cp, flags, length, out != nullptr, size, lab(def), lab(used)}, {narrow(text).c_str()});
    int32_t n = 0;
    while (text[n]) ++n;
    ++n;
    if (out && size) for (int i = 0; i < std::min(n, size); ++i) out[i] = text[i] > 255 ? '?' : text[i];
    if (!out) return n + ret("query_extra");
    return size && size < n ? 0 : n;
}
void CopyMemory(void* dest, const void* source, uint32_t size) {
    ev("CopyMemory", {lab(dest), static_cast<int64_t>(static_cast<const uint8_t*>(source) - static_cast<const uint8_t*>(gInput)), size});
    std::memcpy(dest, source, size);
}
}

uint32_t LbZLib_BufferedDecompress(void* dest, void* buffer, uint32_t size,
                                  bfl::compress::ZLibBufferFunction fn, void* context, void* workspace) {
    ev("Decompress", {lab(dest), lab(buffer), size, fn == _GetMoreData, lab(context), lab(workspace)});
    int32_t answer = fn(buffer, 7, context);
    ev("CallbackResult", {answer});
    return 123;
}

// The wad storage is outside this unit; fake the entire class and its owned members.
namespace bfl::FileSystem {
template<> NodeAllocator<DirNode>::NodeAllocator() {}
template<> NodeAllocator<DirNode>::~NodeAllocator() {}
template<> NodeAllocator<FileNode>::NodeAllocator() {}
template<> NodeAllocator<FileNode>::~NodeAllocator() {}
_TbWadStorageAccess::_TbWadStorageAccess() {
    ev("WadCtor", {lab(this)});
}
_TbWadStorageAccess::~_TbWadStorageAccess() { ev("WadDtor", {lab(this)}); }
TbError _TbWadStorageAccess::Open(const char* name, uint32_t flags) {
    ev("WadOpen", {lab(this), flags}, {name});
    return TbError(ret("storage"));
}
TbError _TbWadStorageAccess::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(-1); }
TbError _TbWadStorageAccess::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError _TbWadStorageAccess::FindNext(CFileInfo&) { return TbError(-1); }
TbError _TbWadStorageAccess::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError _TbWadStorageAccess::CreateDir(const char*) { return TbError(-1); }
TbError _TbWadStorageAccess::Delete(const char*) { return TbError(-1); }
TbError _TbWadStorageAccess::Move(const char*, const char*) { return TbError(-1); }
void _TbWadStorageAccess::Close() {}
WadNameTree::WadNameTree() {}
WadNameTree::~WadNameTree() {}
}
TbStringMatch::~TbStringMatch() {}
TbCharStringList::~TbCharStringList() {}
int32_t _LbSemaLock::Lock(int32_t) { return 1; }
_LbSemaLock::~_LbSemaLock() {}

extern "C" {
// Native layouts accompany each Mac layout in Python; no native object uses Mac offsets.
uint64_t tpw_fsm_make(int kind) {
    gKind = kind;
    void* p = std::calloc(1, 2048);
    switch (kind) {
        case 0: ::new (p) _TbMemFileAccess(4); break;
        case 1: ::new (p) _TbZLibFileAccess; break;
        case 2: ::new (p) Storage; break;
        case 3: ::new (p) CDiscFileStorage; break;
        case 4: ::new (p) CWadFileStorage; break;
        case 5: ::new (p) CFileInfo; break;
        case 6: ::new (p) String<char16_t>; break;
        case 7: ::new (p) String<char>; break;
        case 8: ::new (p) _TbDiscAccess; break;
        case 9: ::new (p) CMemFile; break;
        case 10: ::new (p) Backend; break;
        case 12: ::new (p) CUniFileInfo; break;
    }
    return reinterpret_cast<uintptr_t>(p);
}
void tpw_fsm_config(int kind, int offset, uint32_t seed, const char* entry, const char* full, void* input, void* output) {
    gKind = kind;
    gNameOffset = offset;
    gSeed = seed;
    gEntry = entry;
    gFull = full;
    gInput = input;
    gOutput = output;
}
int tpw_fsm_layout(uint32_t* out) {
    const uint32_t fields[] = {
        offsetof(CAccessItem, mNext), offsetof(CAccessItem, mReferences),
        offsetof(_TbMemFileAccess, mFirst), offsetof(_TbMemFileAccess, mLast), offsetof(_TbMemFileAccess, mCurrent),
        offsetof(_TbMemFileAccess, mCursor), offsetof(_TbMemFileAccess, mPosition), offsetof(_TbMemFileAccess, mSize),
        offsetof(_TbMemFileAccess, mBlockSize), offsetof(_TbMemFileAccess, mSemaphore),
        offsetof(_TbMemFileAccess::Block, mPrev), offsetof(_TbMemFileAccess::Block, mNext),
        offsetof(_TbMemFileAccess::Block, mData), offsetof(_TbMemFileAccess::Block, mStart), offsetof(_TbMemFileAccess::Block, mEnd),
        sizeof(_TbMemFileAccess::Block), offsetof(_TbZLibFileAccess, mSize), offsetof(_TbZLibFileAccess, mBuffer),
        offsetof(_TbZLibFileAccess, mCursor), offsetof(CFileStorage, mAccess), offsetof(CFile, mImplementation),
        offsetof(CUniFileInfo, mName), offsetof(CUniFileInfo, mNameBuffer), offsetof(TbStringBase<char16_t>, mLength),
        offsetof(TbStringBase<char16_t>, mData), offsetof(TbStringBase<char16_t>, mCapacity), offsetof(_TbDiscAccess, mSemaphore)
    };
    std::memcpy(out, fields, sizeof fields);
    return sizeof fields / sizeof fields[0];
}
uint64_t tpw_fsm_sema() { return reinterpret_cast<uintptr_t>(&gSemaphore); }
uint64_t tpw_fsm_call(int op, void* obj, void* a, void* b, uint32_t c, uint64_t d) {
    auto* mem = static_cast<_TbMemFileAccess*>(obj);
    auto* zip = static_cast<_TbZLibFileAccess*>(obj);
    auto* storage = static_cast<CFileStorage*>(obj);
    auto* access = static_cast<CAccessItem*>(obj);
    gMemory = mem;
    gTraceCopy = op == 16 || op == 20 || op == 50;
    struct CopyScope {
        ~CopyScope() { gTraceCopy = false; }
    } scope;
    switch (op) {
        case 0: return storage->FindFirst(static_cast<char16_t*>(a), *static_cast<CUniFileInfo*>(b));
        case 1: return storage->FindNext(*static_cast<CUniFileInfo*>(b));
        case 2: return storage->OpenFile(*static_cast<CFile*>(b), static_cast<char16_t*>(a), c, reinterpret_cast<void*>(static_cast<uintptr_t>(d)));
        case 3: return storage->FullPath(static_cast<char16_t*>(b), static_cast<char16_t*>(a), c);
        case 4: return storage->FullPath(*static_cast<TbDynamicStringTemplate<char16_t>*>(b), *static_cast<TbDynamicStringTemplate<char16_t>*>(a));
        case 5: return storage->SetAttribs(static_cast<char16_t*>(a), c);
        case 6: return storage->CreateDir(static_cast<char16_t*>(a));
        case 7: return storage->Delete(static_cast<char16_t*>(a));
        case 8: return storage->Move(static_cast<char16_t*>(a), static_cast<char16_t*>(b));
        case 9: return static_cast<CDiscFileStorage*>(obj)->Open(static_cast<char16_t*>(a));
        case 12: return static_cast<CWadFileStorage*>(obj)->Open(static_cast<char16_t*>(a), c);
        case 14: ::new (obj) _TbMemFileAccess(c); return reinterpret_cast<uintptr_t>(obj);
        case 15: mem->Close(); return 0;
        case 16: return mem->Read(a, c);
        case 17: return mem->Seek(c, d);
        case 18: return mem->GetSize();
        case 19: return mem->GetPos();
        case 20: return mem->AddBlock();
        case 21: mem->RemoveAllBlocks(); return 0;
        case 22: return reinterpret_cast<uintptr_t>(mem->GetSemaphore());
        case 25: ::new (obj) _TbZLibFileAccess; return reinterpret_cast<uintptr_t>(obj);
        case 26: return zip->Open(c);
        case 27: return zip->Write(a, c);
        case 28: return zip->Read(a, c);
        case 29: return zip->Seek(c, d);
        case 30: return zip->GetPos();
        case 31: return zip->GetSize();
        case 32: zip->Close(); return 0;
        case 33: zip->AttachAccessClass(static_cast<CAccessItem*>(a)); return 0;
        case 34: return _GetMoreData(a, c, obj);
        case 35: access->CAccessItem::Close(); return 0;
        case 36: access->CAccessItem::AttachAccessClass(static_cast<CAccessItem*>(a)); return 0;
        case 37: return reinterpret_cast<uintptr_t>(access->CAccessItem::MapFile(c, d));
        case 38: access->CAccessItem::UnmapFile(static_cast<uint8_t*>(a)); return 0;
        case 39: return reinterpret_cast<uintptr_t>(access->CAccessItem::GetSemaphore());
        case 40: return access->CAccessItem::Flush();
        case 41: return access->CAccessItem::GetMacFileReference();
        case 42: return reinterpret_cast<uintptr_t>(static_cast<_TbDiscAccess*>(obj)->GetSemaphore());
        case 43: static_cast<_TbDiscAccess*>(obj)->AttachAccessClass(static_cast<CAccessItem*>(a)); return 0;
        case 44: return bfl::Strings::Convert(static_cast<char16_t*>(b), c, static_cast<char*>(a));
        case 45: return bfl::Strings::Convert(*static_cast<TbDynamicStringTemplate<char16_t>*>(obj), static_cast<char*>(a));
        case 46: return bfl::Strings::Convert(static_cast<char*>(b), c, static_cast<char16_t*>(a));
        case 47: return bfl::Strings::Convert(*static_cast<TbDynamicStringTemplate<char>*>(obj), static_cast<char16_t*>(a));
        case 48: return reinterpret_cast<uintptr_t>(static_cast<TbStringBase<char16_t>*>(obj)->LockBuffer(c));
        case 49: static_cast<TbStringBase<char16_t>*>(obj)->ReleaseBuffer(c); return 0;
        case 50: return mem->Write(a, c);
        case 51: return static_cast<CMemFile*>(obj)->Open(c);
    }
    if (!obj) return 0;
    bool deleting = static_cast<int16_t>(c) > 0;
    if (op == 10) { if (deleting) delete static_cast<CDiscFileStorage*>(obj); else static_cast<CDiscFileStorage*>(obj)->~CDiscFileStorage(); }
    if (op == 11) { if (deleting) delete static_cast<CWadFileStorage*>(obj); else static_cast<CWadFileStorage*>(obj)->~CWadFileStorage(); }
    if (op == 13) { if (deleting) delete static_cast<CFileInfo*>(obj); else static_cast<CFileInfo*>(obj)->~CFileInfo(); }
    if (op == 23) { if (deleting) delete mem; else mem->~_TbMemFileAccess(); }
    if (op == 24) { if (deleting) delete zip; else zip->~_TbZLibFileAccess(); }
    return reinterpret_cast<uintptr_t>(obj);
}
}
