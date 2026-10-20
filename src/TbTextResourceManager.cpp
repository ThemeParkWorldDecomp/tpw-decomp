#include "TbTextResourceManager.h"
#include "TbIResourceIndex.h"
#include "TbITable.h"
#include "bfl_FileSystem.h"
#include "platform/ltms.h"
#include <new>

TbTextResourceManager::TbTextResourceManager() : mIndex(nullptr), mOpened(0) {
    new (&mStorage) bfl::FileSystem::CDiscFileStorage;
}
TbTextResourceManager::~TbTextResourceManager() {
    Close();
    mStorage.~CDiscFileStorage();
}
TbError TbTextResourceManager::Open(const char* path) {
    if (mOpened != 0) return TbError(-1);
    if (!mStorage.Open(path).Succeeded()) return TbError(-1);
    mOpened = 1;
    return TbError(0);
}
TbError TbTextResourceManager::Close() {
    mStorage.Close();
    if (mIndex) {
        mIndex->Release();
        mIndex = nullptr;
    }
    return TbError(0);
}
TbError TbTextResourceManager::SetLanguage(const char* name) {
    char path[0x200];
    if (!mStorage.FullPath(path, name, sizeof(path)).Succeeded()) return TbError(-1);
    return LoadResourceIndex(path);
}
TbError TbTextResourceManager::LoadResourceIndex(const char* path) {
    if (mIndex) {
        mIndex->Release();
        mIndex = nullptr;
    }
    bfl::FileSystem::CDiscFileStorage storage;
    if (!storage.Open(path).Succeeded()) return TbError(-1);
    bfl::FileSystem::CFile file;
    if (!storage.CFileStorage::OpenFile(file, "ResIdx.dat", 0x80000001u, nullptr).Succeeded()) return TbError(-1);
    uint32_t header[2];
    if (file.Read(header, 8) != 8) return TbError(-1);
    SamsUtilities::USwapInplace32(header[0]);
    SamsUtilities::USwapInplace32(header[1]);
    if (header[0] != 0x49524642u) return TbError(-1);
    const uint32_t size = uint32_t(file.GetSize()) - uint32_t(file.GetPos());
    if (header[1] == 0) {
        mStorage.Close();
        if (mIndex) {
            mIndex->Release();
            mIndex = nullptr;
        }
        return TbError(-1);
    }
    auto* block = new uint8_t[sizeof(TbIResourceIndex) + size];
    auto* data = block + sizeof(TbIResourceIndex);
    if (file.Read(data, size) != static_cast<int32_t>(size)) {
        delete[] block;
        return TbError(-1);
    }
    auto* index = new (block) TbIResourceIndex;
    if (!index->Initialise(path, data, header[1]).Succeeded()) {
        delete[] block;
        return TbError(-1);
    }
    mIndex = index;
    return TbError(0);
}

namespace {
template<class T> TbError Get(TbIResourceIndex* index, T** out, uint32_t id, uint16_t type) {
    auto* handle = index->GetResourceItemHandle(id);
    if (index->GetResourceType(handle) != type) {
        *out = nullptr;
        return TbError(-1);
    }
    return index->GetResource(handle, reinterpret_cast<void**>(out));
}
}
TbError TbTextResourceManager::GetResource(TbIMBToUnicode** out, uint32_t id) {
    return Get(mIndex, out, id, 0);
}
TbError TbTextResourceManager::GetResource(TbIUnicodeToMB** out, uint32_t id) {
    return Get(mIndex, out, id, 1);
}
TbError TbTextResourceManager::GetResource(TbITable** out, uint32_t id) {
    return Get(mIndex, out, id, 3);
}
TbError TbTextResourceManager::GetResource(TbIFont** out, uint32_t id) {
    return Get(mIndex, out, id, 4);
}
