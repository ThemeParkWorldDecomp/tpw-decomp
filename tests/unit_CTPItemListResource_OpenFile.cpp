// Test library for the item list resource's OpenFile (two overloads) and destructor (unit v5375;
// libtpw_unit_CTPItemListResource_OpenFile.dylib, tests/test_CTPItemListResource_OpenFile.py), main app:
//   src/TPSettings_itemres_CTPItemListResource_OpenFile.cpp
//
// Strings are tagged: the fakes keep a tag in mData (a new one for every string built), so events name their strings by tag.
// The base destructors (inlined in the original's destructor) are written here as fakes that make the events the original's code
// makes. The virtual calls the original makes through __ptr_glue are the itemlist_callee functions.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TPGlobals.h"
#include "TPResource.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;
using bfl::FileSystem::_TbStorageAccess;

TPGlobals::TPGlobals() {}   // the real one fills in the whole table; the test sets the one string it reads
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;

namespace {
using Str = TbDynamicStringTemplate<char>;
using ItemList = TbPtrList<CTPItemResource>;
using StorageList = TbPtrList<bfl::FileSystem::CFileStorage>;
uint32_t gCounter = 0x10000;

int64_t TagOf(const Str& s) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mData)); }
int64_t StorageTag(const bfl::FileSystem::CFileStorage* s) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s->mAccess)); }
void SetTag(Str& s, uint32_t tag) { s.mData = reinterpret_cast<char*>(static_cast<uintptr_t>(tag)); }
}  // namespace

// ---- the string fakes
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() {
    uint32_t tag = gCounter++;
    this->mData = reinterpret_cast<Char*>(static_cast<uintptr_t>(tag));
    ev("StrNew", {tag});
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate<Char>& other) {
    uint32_t tag = gCounter++;
    this->mData = reinterpret_cast<Char*>(static_cast<uintptr_t>(tag));
    ev("StrCopy", {tag, static_cast<int64_t>(reinterpret_cast<uintptr_t>(other.mData))});
}
template<class Char> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {
    ev("StrDtor", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this->mData)), -1});
}
template<class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate<Char>& other) {
    ev("StrAssign", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this->mData)),
                     static_cast<int64_t>(reinterpret_cast<uintptr_t>(other.mData))});
    this->mData = other.mData;
    return *this;
}

// The rest of the string's virtuals: in its vtable, never called.
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char*) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) { return *this; }
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) { return *this; }

template class TbDynamicStringTemplate<char>;   // the src file's calls need these out of line

// The file storage's other virtuals (the library's; Close, the key function, makes the vtable): never called.
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo&) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return nullptr; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }

bfl::FileSystem::CFileStorage::~CFileStorage() { ev("StorageDtor", {StorageTag(this), -1}); }

int32_t QualifyFilename(TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& base, TbDynamicStringTemplate<char>& out) {
    uint32_t tag = gCounter++;
    ev("Qualify", {TagOf(name), TagOf(base), tag});
    SetTag(out, tag);
    return static_cast<int32_t>(ret("Qualify"));
}

int32_t CTPBaseResource::OpenQualifiedFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name) {
    ev("OpenQualified", {lab(this), lab(&file), TagOf(name)});
    return static_cast<int32_t>(ret("oq"));
}

// The base class's virtuals (the vtables of the classes defined below name them).
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&, bfl::FileSystem::CFileStorage&, uint32_t) {
    return 0;
}

// The inlined base destructors of the original: the file storage member, then the search pattern and the storages.
CTPIntermediate::~CTPIntermediate() {}
CTPTextureResource::~CTPTextureResource() {}
CTPBaseResource::~CTPBaseResource() {
    if (mSearchPattern != nullptr) ev("StrDelete", {TagOf(*mSearchPattern), 1});
    mSearchPattern = nullptr;
    if (mStorages != nullptr) {
        while (mStorages->mCount != 0) {
            bfl::FileSystem::CFileStorage* storage = mStorages->RemoveHead();
            if (storage != nullptr) ev("StorageDelete", {StorageTag(storage), 1});
        }
        if (mStorages != nullptr) ev("ListDelete", {2, mStorages->mCount, 1});
        mStorages = nullptr;
    }
}

namespace itemlist_callee {
int32_t StorageOpen(bfl::FileSystem::CFileStorage* storage, bfl::FileSystem::CFile& file, const char* name, uint32_t mode, void* data) {
    ev("StorageOpen", {StorageTag(storage), lab(&file), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name)), mode,
                       static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data))});
    return static_cast<int32_t>(ret("open"));
}
void ItemDelete(CTPItemResource* item, int16_t flag) { ev("ItemDelete", {TagOf(item->mName), static_cast<uint16_t>(flag)}); }
void ItemListDelete(TbPtrList<CTPItemResource>* list, int16_t flag) { ev("ListDelete", {1, list->mCount, static_cast<uint16_t>(flag)}); }
}  // namespace itemlist_callee

extern "C" {

uint64_t tpw_il_addr(const char* name) {
    if (std::strcmp(name, "searchpath") == 0) return reinterpret_cast<uintptr_t>(&gTPGlobals.mSearchPath);
    return 0;
}
uint64_t tpw_il_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
int32_t tpw_il_layout(int64_t*) { return 0; }
void tpw_il_reset() { gCounter = 0x10000; }

void tpw_il_set_tag(uint64_t s, uint32_t tag) { SetTag(*reinterpret_cast<Str*>(s), tag); }
uint64_t tpw_il_str_new(uint32_t tag) {
    Str* s = reinterpret_cast<Str*>(tpw::ArenaAllocZeroed(sizeof(Str)));
    SetTag(*s, tag);
    return reinterpret_cast<uintptr_t>(s);
}
uint64_t tpw_il_storage_new(uint32_t tag) {
    auto* s = reinterpret_cast<bfl::FileSystem::CFileStorage*>(tpw::ArenaAllocZeroed(sizeof(bfl::FileSystem::CFileStorage)));
    s->mAccess = reinterpret_cast<_TbStorageAccess*>(static_cast<uintptr_t>(tag));
    return reinterpret_cast<uintptr_t>(s);
}
uint64_t tpw_il_item_new(uint32_t tag) {
    auto* item = reinterpret_cast<CTPItemResource*>(tpw::ArenaAllocZeroed(sizeof(CTPItemResource)));
    SetTag(item->mName, tag);
    SetTag(item->mPath, tag + 1);
    item->mStorage.mAccess = reinterpret_cast<_TbStorageAccess*>(static_cast<uintptr_t>(tag + 2));
    return reinterpret_cast<uintptr_t>(item);
}
uint64_t tpw_il_list_new(int32_t kind) {
    if (kind == 1) return reinterpret_cast<uintptr_t>(new ItemList);
    return reinterpret_cast<uintptr_t>(new StorageList);
}
void tpw_il_list_add(int32_t kind, uint64_t list, uint64_t obj) {
    if (kind == 1) reinterpret_cast<ItemList*>(list)->AddTail(reinterpret_cast<CTPItemResource*>(obj));
    else reinterpret_cast<StorageList*>(list)->AddTail(reinterpret_cast<bfl::FileSystem::CFileStorage*>(obj));
}
int32_t tpw_il_list_dump(int32_t kind, uint64_t list, int64_t* out, int32_t max) {
    auto* l = reinterpret_cast<TbTList<void*, void*>*>(list);   // the same layout for both element types
    int32_t n = 0;
    out[n++] = l->mCount;
    for (auto* node = l->mHead; node != nullptr && n < max; node = node->mNext) {
        if (node->mData == nullptr) out[n++] = 0;
        else if (kind == 1) out[n++] = TagOf(static_cast<CTPItemResource*>(node->mData)->mName);
        else out[n++] = StorageTag(static_cast<bfl::FileSystem::CFileStorage*>(node->mData));
    }
    return n;
}

uint64_t tpw_il_res_new() { return tpw_il_alloc(sizeof(CTPItemListResource)); }
void tpw_il_res_set(uint64_t r, int32_t id, uint64_t v) {
    auto* res = reinterpret_cast<CTPItemListResource*>(r);
    switch (id) {
        case 0: res->mItems = reinterpret_cast<ItemList*>(v); break;
        case 1: res->mF48 = reinterpret_cast<void*>(v); break;
        case 2: res->mLastFoundFileStorage = reinterpret_cast<bfl::FileSystem::CFileStorage*>(v); break;
        case 3: res->mSearchPattern = reinterpret_cast<Str*>(v); break;
        case 4: res->mStorages = reinterpret_cast<StorageList*>(v); break;
        case 5: res->mF40.mAccess = reinterpret_cast<_TbStorageAccess*>(static_cast<uintptr_t>(v)); break;
        case 6: SetTag(res->mPath, static_cast<uint32_t>(v)); break;
        case 7: SetTag(res->mName, static_cast<uint32_t>(v)); break;
    }
}
uint64_t tpw_il_res_get(uint64_t r, int32_t id) {
    auto* res = reinterpret_cast<CTPItemListResource*>(r);
    switch (id) {
        case 0: return res->mItems != nullptr;
        case 1: return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res->mF48));
        case 2: return res->mLastFoundFileStorage ? static_cast<uint64_t>(StorageTag(res->mLastFoundFileStorage)) : 0;
        case 3: return res->mSearchPattern != nullptr;
        case 4: return res->mStorages != nullptr;
    }
    return 0;
}

uint32_t tpw_il_open1(uint64_t res, uint64_t file, uint64_t name, uint32_t mode) {
    return static_cast<uint32_t>(reinterpret_cast<CTPItemListResource*>(res)->CTPItemListResource::OpenFile(
        *reinterpret_cast<bfl::FileSystem::CFile*>(file), *reinterpret_cast<Str*>(name), mode));
}
uint32_t tpw_il_open2(uint64_t res, uint64_t file, uint64_t name, uint64_t storage, uint32_t mode) {
    return static_cast<uint32_t>(reinterpret_cast<CTPItemListResource*>(res)->CTPItemListResource::OpenFile(
        *reinterpret_cast<bfl::FileSystem::CFile*>(file), *reinterpret_cast<Str*>(name),
        *reinterpret_cast<bfl::FileSystem::CFileStorage*>(storage), mode));
}
// The deleting destructor: the destructor, then operator delete for a flag above 0; answers this.
uint64_t tpw_il_dtor(uint64_t res, int16_t flag) {
    auto* r = reinterpret_cast<CTPItemListResource*>(res);
    if (r != nullptr) {
        r->CTPItemListResource::~CTPItemListResource();
        if (flag > 0) ev("OperatorDelete", {lab(r)});
    }
    return res;
}

}  // extern "C"
