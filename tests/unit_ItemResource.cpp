// Item resources: functional strings and recorded file-system and string-helper fakes.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "TPResource.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "Text.h"
#include "calllog.h"

// The extra borrowed string operations used by InitItem, unrecorded like fake_strings.h.
template<class Char> int32_t TbStringBase<Char>::Find(const Char* text, uint32_t start) const {
    const uint32_t length = static_cast<uint32_t>(mLength);
    if (start > length) return -1;
    for (uint32_t i = start; i <= length; ++i) {
        uint32_t j = 0;
        while (text[j] != 0 && i + j < length && mData[i + j] == text[j]) ++j;
        if (text[j] == 0) return static_cast<int32_t>(i);
    }
    return -1;
}
template<class Char> int32_t TbStringBase<Char>::FindReverse(const Char& ch, uint32_t start) const {
    const uint32_t length = static_cast<uint32_t>(mLength);
    uint32_t i = start < length ? start : length;
    for (;;) {
        if (i < length && mData[i] == ch) return static_cast<int32_t>(i);
        if (i == 0) return -1;
        --i;
    }
}
template<class Char> bool TbStringBase<Char>::operator==(const Char* text) const {
    uint32_t i = 0;
    while (text[i] != 0 && i < static_cast<uint32_t>(mLength) && mData[i] == text[i]) ++i;
    return i == static_cast<uint32_t>(mLength) && text[i] == 0;
}
#define TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW
#include "fake_strings.h"
#include "fake_file_slots.h"

using namespace bfl::FileSystem;
using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
CText gText;

namespace {
char gLanguage[256];
bool gFailNew;
char gAccess;

// ASCII case ignoring, with high bytes kept, matching the paired fake.
bool Equal(const char* a, const char* b) {
    for (;; ++a, ++b) {
        const unsigned char x = *a >= 'A' && *a <= 'Z' ? *a + 32 : static_cast<unsigned char>(*a);
        const unsigned char y = *b >= 'A' && *b <= 'Z' ? *b + 32 : static_cast<unsigned char>(*b);
        if (x != y) return false;
        if (x == 0) return true;
    }
}
}

// Allocation failure is enabled only around RegisterItem's first list allocation.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (gFailNew) {
        gFailNew = false;
        return nullptr;
    }
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    ev("TPStrEq", {}, {a.mData, b.mData});
    return Equal(a.mData, b.mData);
}
TbDynamicStringTemplate<char>& RemoveExtension(const TbDynamicStringTemplate<char>& extension,
                                               TbDynamicStringTemplate<char>& name) {
    ev("RemoveExtension", {}, {extension.mData, name.mData});
    const uint32_t count = static_cast<uint32_t>(extension.mLength);
    if (count <= static_cast<uint32_t>(name.mLength) && Equal(name.mData + name.mLength - count, extension.mData))
        name = name.Left(static_cast<uint32_t>(name.mLength) - count);
    return name;
}
bool FS_IsWadFile(const CFileInfo& info) {
    ev("FS_IsWadFile", {lab(&info)}, {info.mName});
    return ret("FS_IsWadFile", 0) != 0;
}
char* CText::GetLanguageName() {
    ev("CText::GetLanguageName", {lab(this)});
    return gLanguage;
}

// Storage copies share an access marker, so successful initialization is observable.
CFileStorage::CFileStorage() : mAccess(nullptr) { ev("CFileStorage", {newlab(this)}); }
CFileStorage::CFileStorage(const CFileStorage& other) : mAccess(other.mAccess) {
    ev("CFileStorage::copy", {newlab(this), lab(&other), lab(other.mAccess)});
}
CFileStorage& CFileStorage::operator=(const CFileStorage& other) {
    ev("CFileStorage::assign", {lab(this), lab(&other), lab(other.mAccess)});
    mAccess = other.mAccess;
    return *this;
}
CFileStorage::~CFileStorage() { ev("~CFileStorage", {lab(this), lab(mAccess)}); }
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    ev("CFileStorage::FindFirst", {lab(this), info.mName - info.mNameBuffer}, {pattern, info.mName});
    const int32_t result = static_cast<int32_t>(ret("CFileStorage::FindFirst", lab(this), -1));
    info.mF00 = 0x1000 + lab(this);
    info.mAttributes = lab(this) * 3;
    for (uint32_t i = 0; i < sizeof(info.mF08); ++i) info.mF08[i] = static_cast<uint8_t>(lab(this) + i);
    if (result >= 0) {
        std::snprintf(info.mNameBuffer, sizeof(info.mNameBuffer), "first%d", lab(this));
        info.mName = info.mNameBuffer + lab(this) % 3;
    }
    return TbError(result);
}
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    // Three chunks preserve the whole filename, even when its extension is beyond byte 159.
    char chunks[3][160] = {};
    const std::size_t length = std::strlen(name);
    for (std::size_t k = 0; k < 3 && k * 159 < length; ++k)
        std::memcpy(chunks[k], name + k * 159, std::min<std::size_t>(159, length - k * 159));
    ev("CFileStorage::OpenFile", {lab(this), lab(&file), mode, lab(data)}, {chunks[0], chunks[1], chunks[2]});
    file.mImplementation = reinterpret_cast<CAccessItem*>(&gAccess);
    return TbError(static_cast<int32_t>(ret("CFileStorage::OpenFile", lab(this), -1)));
}
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
const char* CFileStorage::GetRootPath() { return ""; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }
TbError CDiscFileStorage::Open(const char* path) {
    ev("CDiscFileStorage::Open", {lab(this)}, {path});
    mAccess = reinterpret_cast<_TbStorageAccess*>(&gAccess);
    return TbError(static_cast<int32_t>(ret("Storage::Open", -1)));
}
TbError CWadFileStorage::Open(const char* path, uint32_t flags) {
    ev("CWadFileStorage::Open", {lab(this), flags}, {path});
    mAccess = reinterpret_cast<_TbStorageAccess*>(&gAccess);
    return TbError(static_cast<int32_t>(ret("Storage::Open", -1)));
}
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }

extern "C" {
void* tpw_ir_global(int32_t which) {
    if (which == 0) return &gText;
    return &gAccess;
}
void tpw_ir_globals(int32_t off, const char* text) {
    TbDynamicStringTemplate<char>* s = nullptr;
    switch (off) {
    case 0x4164: s = &gTPGlobals.mF4164; break;
    case 0x4178: s = &gTPGlobals.mF4178; break;
    case 0x418c: s = &gTPGlobals.mF418C; break;
    case 0x4240: s = &gTPGlobals.mSamExtension; break;
    case 0x4268: s = &gTPGlobals.mOnlineSamExtension; break;
    case 0x427c: s = &gTPGlobals.mF427C; break;
    case 0x4290: s = &gTPGlobals.mOnlinePrefix; break;
    case 0x42a4: s = &gTPGlobals.mF42A4; break;
    case 0x42b8: s = &gTPGlobals.mF42B8; break;
    case 0x4394: s = &gTPGlobals.mWadExtension; break;
    case 0x45b0: s = &gTPGlobals.mF45B0; break;
    }
    if (s != nullptr) *s = text;
}
void tpw_ir_language(const char* text) { std::strcpy(gLanguage, text); }
void* tpw_ir_string(const char* text) { return new TbDynamicStringTemplate<char>(text); }
void* tpw_ir_info(const char* name, int32_t offset) {
    auto* info = new CFileInfo;
    std::memset(info->mNameBuffer, 'x', sizeof info->mNameBuffer);
    std::strcpy(info->mNameBuffer + offset, name);
    info->mName = info->mNameBuffer + offset;
    return info;
}
void* tpw_ir_block() {
    void* block = std::malloc(sizeof(CTPItemResource));
    std::memset(block, 0xa5, sizeof(CTPItemResource));
    return block;
}
void* tpw_ir_construct(void* block, const CFileInfo* info, const TbDynamicStringTemplate<char>* folder) {
    return new (block) CTPItemResource(*info, *folder);
}
void tpw_ir_set(CTPItemResource* item, const char* name, const char* path, CTPItemListResource* owner) {
    item->mName = name;
    item->mPath = path;
    item->mStorage.mAccess = nullptr;
    item->mList = owner;
}
void tpw_ir_access(CTPItemResource* item, int32_t present) {
    item->mStorage.mAccess = present ? reinterpret_cast<_TbStorageAccess*>(&gAccess) : nullptr;
}
int32_t tpw_ir_owner_untouched(CTPItemResource* item) {
    unsigned char bytes[sizeof(item->mList)];
    std::memcpy(bytes, &item->mList, sizeof bytes);
    for (const unsigned char byte : bytes)
        if (byte != 0xa5) return 0;
    return 1;
}
void* tpw_ir_member(CTPItemResource* item, int32_t which) {
    if (which == 0) return &item->mName;
    if (which == 1) return &item->mPath;
    if (which == 2) return &item->mStorage;
    if (which == 3) return item->mList;
    return item->mStorage.mAccess;
}
const char* tpw_ir_text(const TbDynamicStringTemplate<char>* text) { return text->mData; }
void* tpw_ir_file() { return new CFile; }
void* tpw_ir_file_access(CFile* file) { return file->mImplementation; }
void* tpw_ir_resource(TbPtrList<CTPItemResource>* list) {
    void* block = std::malloc(sizeof(CTPItemListResource));
    std::memset(block, 0xa5, sizeof(CTPItemListResource));
    auto* resource = static_cast<CTPItemListResource*>(block);
    resource->mItems = list;
    return resource;
}
void* tpw_ir_list() { return new TbPtrList<CTPItemResource>; }
void tpw_ir_append(TbPtrList<CTPItemResource>* list, CTPItemResource* item) { list->AddTail(item); }
int32_t tpw_ir_walk(CTPItemListResource* resource, int64_t* out) {
    auto* list = resource->mItems;
    if (list == nullptr) return 0;
    out[0] = list->mCount;
    int32_t n = 1;
    for (auto* node = list->mHead; node != nullptr; node = node->mNext) out[n++] = lab(node->mData);
    for (auto* node = list->mTail; node != nullptr; node = node->mPrev) out[n++] = lab(node->mData);
    return n;
}
int32_t tpw_ir_call(int32_t op, CTPItemResource* item, void* arg, int32_t flag) {
    switch (op) {
    case 1:
        if (flag > 0) delete item;
        else item->~CTPItemResource();
        return 0;
    case 5: return item->GetSamFile(*static_cast<CFile*>(arg));
    case 6: return item->GetEasySamFile(*static_cast<CFile*>(arg));
    case 7: return item->GetOnlineSamFile(*static_cast<CFile*>(arg));
    case 8: return item->GetCoasterSamFile(*static_cast<CFile*>(arg));
    case 9: return item->GetTextFile(*static_cast<CFile*>(arg));
    case 10: return item->HasSounds();
    case 11: return item->HasParticles();
    }
    return 0;
}
void tpw_ir_init(CTPItemResource* item, const CFileInfo* info, const TbDynamicStringTemplate<char>* folder) {
    item->InitItem(*info, *folder);
}
int32_t tpw_ir_register(CTPItemListResource* resource, CTPItemResource* item, int32_t fail) {
    gFailNew = fail != 0;
    const int32_t result = resource->RegisterItem(*item);
    gFailNew = false;
    return result;
}
int32_t tpw_ir_find(CTPItemListResource* resource, const TbDynamicStringTemplate<char>* name) {
    return resource->FindItem(*name);
}
}
