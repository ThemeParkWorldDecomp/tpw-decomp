// ltms resource index, Mac size 0x18, vtable 0x10011250.
#pragma once
#include "TbResource.h"
#include "TbError.h"
#include "bfl_FileSystem.h"
#include <vector>

class TbIResourceIndex : public TbResource {
public:
    // File records are little endian, 12 bytes on Mac. Host pointers widen records.
    struct resourceIndexItem {
        uint32_t mId;
        uint16_t mType;
        uint16_t mFilenameOffset;
        TbResource* mResource;
    };
    TbIResourceIndex();
    ~TbIResourceIndex() override;
    TbError GetResource(void* handle, void** out);
    TbError OpenResource(resourceIndexItem* item, void** out);
    TbError OpenResourceFile(bfl::FileSystem::CFile& file, resourceIndexItem* item);
    TbError Initialise(const char* path, void* data, uint32_t count);
    uint16_t GetResourceType(void* handle);
    void* GetResourceItemHandle(uint32_t id);
    resourceIndexItem* GetItem(uint32_t id);   // 0x1000383c (unit v5L21): the item with the id, NULL if none
    // 0x100039ac: the resource an item already holds, with a reference added; -1 if none.
    TbError GetPreOpenedResource(void* handle, void** out);
    void Close();
    resourceIndexItem* mFirst; // Mac +8
    resourceIndexItem* mLast;  // Mac +0xc, inclusive
    bfl::FileSystem::CDiscFileStorage mStorage; // Mac +0x10
    uint8_t* mData;
    std::vector<resourceIndexItem> mItems;
    // Unit v5260
    void ReleaseResource(void* handle); // 0x100036d8: the item's resource gets ManagerRelease (vtable +0x14), the slot is cleared
    void ReleaseItem(resourceIndexItem* item); // 0x1000362c: the same
};

// Factories are separate ltms functions, retained as calls.
class TbResourceFactory {
public:
    virtual TbError CreateInterface(bfl::FileSystem::CFile&, void**, uint32_t,
                                    TbIResourceIndex*) const = 0;
};
#define LTMS_FACTORY(Name) \
class Name : public TbResourceFactory { \
public: \
    TbError CreateInterface(bfl::FileSystem::CFile&, void**, uint32_t, \
                            TbIResourceIndex*) const override; \
};
LTMS_FACTORY(MBToUnicodeFactory)
LTMS_FACTORY(UnicodeToMBFactory)
LTMS_FACTORY(MBInfoFactory)
LTMS_FACTORY(TableFactory)
LTMS_FACTORY(RLE4bitFontFactory)
#undef LTMS_FACTORY
extern TbResourceFactory* gLtmsResourceFactories[6]; // Mac TOC +0xd7c
