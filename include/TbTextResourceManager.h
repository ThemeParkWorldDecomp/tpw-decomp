// ltms manager: Mac {index +0, disc storage +4, opened +0xc}, size 0x10.
// The vptr belongs to the storage, not the manager.
#pragma once
#include <cstdint>
#include "TbError.h"
#include "bfl_FileSystem.h"
class TbIResourceIndex;
class TbIFont;
class TbITable;
class TbIMBToUnicode;
class TbIUnicodeToMB;
class TbTextResourceManager {
public:
    TbTextResourceManager();
    ~TbTextResourceManager();
    TbError Open(const char* path);
    TbError Close();
    TbError SetLanguage(const char* name);
    TbError LoadResourceIndex(const char* path);
    TbError GetResource(TbIFont** out, uint32_t id);
    TbError GetResource(TbITable** out, uint32_t id);
    TbError GetResource(TbIMBToUnicode** out, uint32_t id);
    TbError GetResource(TbIUnicodeToMB** out, uint32_t id);
    TbIResourceIndex* mIndex;
    // Manual lifetime permits main-app test fakes to retain opaque state.
    union {
        bfl::FileSystem::CDiscFileStorage mStorage;
        uintptr_t mStorageWords[2];
    };
    int32_t mOpened;
    uintptr_t State(int i) const {
        return i == 0 ? reinterpret_cast<uintptr_t>(mIndex) :
               i == 1 ? mStorageWords[1] : static_cast<uint32_t>(mOpened);
    }
    void SetState(int i, uintptr_t value) {
        if (i == 0) mIndex = reinterpret_cast<TbIResourceIndex*>(value);
        else if (i == 1) mStorageWords[1] = value;
        else mOpened = static_cast<int32_t>(value);
    }
};
