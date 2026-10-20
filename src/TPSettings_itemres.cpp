// TPSettings.cpp (main app): item resources used by the ride database.
// SimTheme Park 2.04b3, 0x101612cc..0x101645dc. Mac layouts are in TPResource.h.
#include "TPResource.h"

#include <new>
#include "TPGlobals.h"
#include "TPString.h"
#include "Text.h"

using namespace bfl::FileSystem;

namespace {
constexpr uint32_t kOpenForReading = 0x80000001;
}

CTPItemResource::CTPItemResource(const CFileInfo& info, const TbDynamicStringTemplate<char>& folder)
    : mName(), mPath(), mStorage() {
    InitItem(info, folder);
}

CTPItemResource::~CTPItemResource() {}

void CTPItemResource::InitItem(const CFileInfo& info, const TbDynamicStringTemplate<char>& folder) {
    TbDynamicStringTemplate<char> storagePath;
    mName = info.mName;
    mName = RemoveExtension(gTPGlobals.mWadExtension, mName);
    mPath = folder;
    mPath = RemoveExtension(gTPGlobals.mWadExtension, mPath);
    storagePath = folder;

    const int32_t first = mName.Find("_", 0);
    // The substring overload rejects a needle as long as the entire name. For this
    // fixed one-character needle, guard the existing character overload the same way.
    const int32_t last = mName.mLength > 1 ? mName.FindReverse('_', 0xffffffff) : -1;
    if (first != last && mName.Find("_", static_cast<uint32_t>(first + 1)) == last) {
        mName = mName.Left(static_cast<uint32_t>(last));
        if (mName.Left(1) == "p")
            mName = mName.Right(static_cast<uint32_t>(mName.Length() - 1));
    }

    if (FS_IsWadFile(info)) {
        CWadFileStorage storage;
        if (storage.Open(storagePath.mData, 0).Succeeded())
            mStorage = static_cast<CFileStorage>(storage);
    } else {
        CDiscFileStorage storage;
        if (storage.Open(storagePath.mData).Succeeded())
            mStorage = static_cast<CFileStorage>(storage);
    }
}

int32_t CTPItemListResource::RegisterItem(CTPItemResource& item) {
    if (mItems == nullptr)
        mItems = new (std::nothrow) TbPtrList<CTPItemResource>;
    if (mItems == nullptr) return 0;
    mItems->AddHead(&item);
    item.mList = this;
    return 1;
}

int32_t CTPItemListResource::FindItem(const TbDynamicStringTemplate<char>& name) const {
    if (mItems == nullptr) return 0;
    auto* node = mItems->mHead;
    while (node != nullptr) {
        CTPItemResource* item = node->mData;
        node = node->mNext;
        if (TPStrEq(item->mName, name)) return 1;
    }
    return 0;
}

int32_t CTPItemResource::GetSamFile(CFile& file) {
    TbDynamicStringTemplate<char> name;
    name = mName;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetEasySamFile(CFile& file) {
    TbDynamicStringTemplate<char> prefix(gTPGlobals.mF427C);
    prefix += mName;
    TbDynamicStringTemplate<char> name;
    name = prefix;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetOnlineSamFile(CFile& file) {
    TbDynamicStringTemplate<char> prefix(gTPGlobals.mOnlinePrefix);
    prefix += mName;
    TbDynamicStringTemplate<char> name;
    name = prefix;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetCoasterSamFile(CFile& file) {
    TbDynamicStringTemplate<char> name;
    name = gTPGlobals.mF42A4;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetTextFile(CFile& file) {
    TbDynamicStringTemplate<char> language(gText.GetLanguageName());
    TbDynamicStringTemplate<char> name;
    name = language;
    name += gTPGlobals.mF42B8;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::HasSounds() {
    CFileInfo info;
    CFileInfo blank;
    if (!mStorage.FindFirst(gTPGlobals.mF4164.mData, info).Succeeded()) return 0;
    info = blank;
    if (!mStorage.FindFirst(gTPGlobals.mF4178.mData, info).Succeeded()) return 0;
    info = blank;
    if (!mStorage.FindFirst(gTPGlobals.mF418C.mData, info).Succeeded()) return 0;
    return 1;
}

int32_t CTPItemResource::HasParticles() {
    CFileInfo info;
    return mStorage.FindFirst(gTPGlobals.mF45B0.mData, info).Succeeded();
}
