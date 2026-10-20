// TPSettings.cpp (main app): the game's resources, the sources game files are opened and
// searched from (TPSettings.h has the file's player settings). The ride database's
// resources: item lists and the individual items' names, paths and file storage.
#pragma once
#include <cstdint>

#include "FS_System.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "bfl_FileSystem.h"

class CTPResourceManager;
class CTPItemResource;

// A source of game files: a named folder and the storages it is searched in (the folder on
// disc and as a .wad archive, locally and on the remote path). Abstract (the destructor is
// pure). Layout on the Mac (0x40 bytes): the vtable is at 0x28, after the two strings.
class CTPBaseResource {
public:
    // Inline: the name, the folder with a separator, no storages yet.
    CTPBaseResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path) {
        mName = name;
        mPath = path;
        FS_AddDirectorySeparator(mPath);
        mManager = nullptr;
        mCurrentSearchStorage = nullptr;
        mLastFoundFileStorage = nullptr;
        mSearchPattern = nullptr;
        mStorages = nullptr;
        mStorages = new TbPtrList<bfl::FileSystem::CFileStorage>;
    }
    virtual ~CTPBaseResource() = 0;   // 0x1015f464: frees the search pattern and the storages
    // Opens `name` (relative to the folder) from the first storage that has it; 1 on success
    // (vtable slot 0xc, 0x10160660; `mode` is not used).
    virtual int32_t OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode);
    // Opens `name` from whichever storage has it (vtable slot 0x10, 0x101604c0; `storage` and
    // `mode` are not used).
    virtual int32_t OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                             bfl::FileSystem::CFileStorage& storage, uint32_t mode);

    void AddDiscStorage(const TbDynamicStringTemplate<char>& path);     // 0x1015f648: the folder
    void AddWADStorage(const TbDynamicStringTemplate<char>& path);      // 0x1015f780: the archive
    void AddAllStorages(const TbDynamicStringTemplate<char>& path);     // 0x1015f948: local, then remote
    void AddLocalStorage(const TbDynamicStringTemplate<char>& path);    // 0x1015f9e8: folder and path.wad
    // 0x1015fbd4: the same under the remote path (gTPFileInfo).
    void AddRemoteStorage(const TbDynamicStringTemplate<char>& path);
    bfl::FileSystem::CFileStorage* pGetCurrentSearchStorage() const;    // 0x1015fdf4
    // 0x1015fe40: the storage the last file was found in (NULL for none).
    bfl::FileSystem::CFileStorage* pGetLastFoundFileStorage() const;
    // The first entry matching `pattern` in any storage (0x10160830), then the next ones
    // (0x10160a2c, going on through the storages); 1 while found.
    int32_t FindFirst(const TbDynamicStringTemplate<char>& pattern, bfl::FileSystem::CFileInfo& info);
    int32_t FindNext(bfl::FileSystem::CFileInfo& info);
    // 0x10162fe0: opens the qualified `name` in `storage`; 1 on success.
    int32_t OpenQualifiedFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                              bfl::FileSystem::CFileStorage& storage) const;
    // 0x101630d8: opens the qualified `name` from the first storage that has it; 1 on success.
    int32_t OpenQualifiedFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name);

    TbDynamicStringTemplate<char> mName;    // 0x00  what FindResource looks for
    TbDynamicStringTemplate<char> mPath;    // 0x14  the folder, with a separator
    // (vtable at 0x28 on the Mac)
    TbPtrList<bfl::FileSystem::CFileStorage>* mStorages;    // 0x2c  searched in order
    CTPResourceManager* mManager;                           // 0x30  set by RegisterResource
    bfl::FileSystem::CFileStorage* mCurrentSearchStorage;   // 0x34  FindFirst/FindNext's
    bfl::FileSystem::CFileStorage* mLastFoundFileStorage;   // 0x38
    TbDynamicStringTemplate<char>* mSearchPattern;          // 0x3c  FindFirst's, for FindNext
};

// The resource subclasses (constructors and destructors in TPSettings.cpp; the base
// constructor is inline in each). Vtables on the Mac in the comments. The members the
// texture resources add are library file storages (constructed and destroyed only).
class CTPTextureResource : public CTPBaseResource {   // 0x102a7050
public:
    CTPTextureResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPTextureResource() override;
    bfl::FileSystem::CFileStorage mF40;   // 0x40
};
class CTPIntermediate : public CTPTextureResource {   // 0x102a708c; abstract
public:
    CTPIntermediate(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
        : CTPTextureResource(name, path) {}
    ~CTPIntermediate() override = 0;   // 0x10161bf4
    // Opens the resource's name plus ".sam" (0x10162d1c), or the online one (0x10162e24),
    // from `storage`; 1 on success.
    int32_t GetSamFile(bfl::FileSystem::CFileStorage& storage, bfl::FileSystem::CFile& file) const;
    int32_t GetOnlineSamFile(bfl::FileSystem::CFileStorage& storage, bfl::FileSystem::CFile& file) const;
};
class CTPAdvisorResource : public CTPIntermediate {   // 0x102a7014
public:
    CTPAdvisorResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPAdvisorResource() override;
    bfl::FileSystem::CFileStorage mF48;   // 0x48
};
// The ride database's list of one kind of item (rides, features, sideshows, upgrades or shops): 0x50 bytes on the Mac.
// The destructor 0x101621f4 and the two OpenFile overrides 0x101639f0 and 0x10163b90 remain
// untranslated. The current destructor leaves the registered items and their list allocated.
class CTPItemListResource : public CTPIntermediate {   // 0x102a6fdc
public:
    CTPItemListResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);   // 0x10161148
    ~CTPItemListResource() override;   // 0x101621f4 (untranslated, 424 bytes)
    int32_t RegisterItem(CTPItemResource& item);   // 0x10163494 (164): adds at the head, then sets the owner
    int32_t FindItem(const TbDynamicStringTemplate<char>& name) const;   // 0x10163924 (120): case ignored, 0 or 1
    // 0x10163b90 (396): the first item folder whose storage opens `name` becomes mLastFoundFileStorage; 1 on success, else clears 0x48 and 0x38.
    int32_t OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode) override;
    // 0x101639f0 (272): qualifies `name` against the resource folder, then opens it from the first storage; 1 on success.
    int32_t OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                     bfl::FileSystem::CFileStorage& storage, uint32_t mode) override;
    void* mF48;      // 0x48  cleared by the constructor
    TbPtrList<CTPItemResource>* mItems;    // 0x4c  allocated by RegisterItem on first use
};

// One ride, feature, shop or other entry in an item list's folder. Mac size 0x38, vtable
// 0x102a6f68 at +0x30, with only the destructor at slot 8. Member names follow CTPBaseResource.
class CTPItemResource {
public:
    CTPItemResource(const bfl::FileSystem::CFileInfo& info, const TbDynamicStringTemplate<char>& folder); // 0x101612cc (136)
    virtual ~CTPItemResource();   // 0x10162470 (136)
    void InitItem(const bfl::FileSystem::CFileInfo& info, const TbDynamicStringTemplate<char>& folder); // 0x10163584 (820)
    int32_t GetSamFile(bfl::FileSystem::CFile& file);          // 0x10163d88 (196)
    int32_t GetEasySamFile(bfl::FileSystem::CFile& file);      // 0x10163e98 (244)
    int32_t GetOnlineSamFile(bfl::FileSystem::CFile& file);    // 0x10163fdc (244)
    int32_t GetCoasterSamFile(bfl::FileSystem::CFile& file);   // 0x10164120 (196)
    int32_t GetTextFile(bfl::FileSystem::CFile& file);         // 0x10164238 (240)
    int32_t HasSounds();                                    // 0x10164374 (460)
    int32_t HasParticles();                                 // 0x10164574 (104)

    TbDynamicStringTemplate<char> mName;       // 0x00
    TbDynamicStringTemplate<char> mPath;       // 0x14
    bfl::FileSystem::CFileStorage mStorage;    // 0x28
    // The item's vtable word is at 0x30 on the Mac.
    CTPItemListResource* mList;                // 0x34, untouched by construction
};
class CTPUIResource : public CTPTextureResource {   // 0x102a6efc
public:
    CTPUIResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPUIResource() override;
    bfl::FileSystem::CFileStorage mF48;   // 0x48
};
class CTPTerrainResource : public CTPTextureResource {   // 0x102a6ecc
public:
    CTPTerrainResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPTerrainResource() override;
};
class CTPSharedTextureResource : public CTPBaseResource {   // 0x102a7078
public:
    CTPSharedTextureResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPSharedTextureResource() override;
};
class CTPSoundResource : public CTPBaseResource {   // 0x102a703c
public:
    CTPSoundResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPSoundResource() override;
    // 0x101632ac: `path` = the full path of the storage with a file matching `name` + "*"
    // (the first one); 1 if one has.
    int32_t FindSoundFile(const TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& path) const;
};
class CTPFontResource : public CTPBaseResource {   // 0x102a6f4c
public:
    CTPFontResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPFontResource() override;
};
class CTPSpriteResource : public CTPBaseResource {   // 0x102a6f24
public:
    CTPSpriteResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPSpriteResource() override;
};
class CTPLanguageResource : public CTPBaseResource {   // 0x102a6e88
public:
    CTPLanguageResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPLanguageResource() override;
};
class CTPFMVResource : public CTPBaseResource {   // 0x102a6e60
public:
    CTPFMVResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path);
    ~CTPFMVResource() override;
};

// The game's resources (gGame+0x34; 12 bytes on the Mac).
class CTPResourceManager {
public:
    CTPResourceManager();    // 0x10164e84
    ~CTPResourceManager();   // 0x10164ec8: deletes the resources
    // Adds `resource` at the front (0x10165054); the shared texture one is also kept
    // (0x10165144). 1 on success.
    int32_t RegisterResource(CTPBaseResource* resource);
    int32_t RegisterResource(CTPSharedTextureResource* resource);
    // 0x10165244: removes and deletes `resource`; 1 if it was registered.
    int32_t UnregisterResource(CTPBaseResource* resource);
    // 0x10165344: opens `name` from the first resource that has it; 1 on success.
    int32_t OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name);
    // 0x10165448: `*resource` = the resource named `name` (ignoring case), which becomes the
    // current one; 1 if there is one (else there is no current resource).
    int32_t FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** resource);
    CTPBaseResource* pGetCurrentResource() const;            // 0x10165538
    void pSetCurrentResource(CTPBaseResource* resource);     // 0x10165580

    TbPtrList<CTPBaseResource>* mResources;      // 0x00
    CTPBaseResource* mCurrentResource;           // 0x04  the last one found or opened from
    CTPSharedTextureResource* mSharedTextures;   // 0x08
};

// 0x1015fe8c: removes a leading "." (the current folder) and the separators and dots after it.
TbDynamicStringTemplate<char>& strRemoveRootDirectory(TbDynamicStringTemplate<char>& path);
// 0x10162f44: appends `extension` to `name`.
void MakeFullFilename(const TbDynamicStringTemplate<char>& extension, TbDynamicStringTemplate<char>& name);
// 0x10160078: `out` = `name` made relative to the resource folder `base`; 1 on success.
int32_t QualifyFilename(TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& base,
                        TbDynamicStringTemplate<char>& out);

// Unit v5375: calls the item list resource's code makes through __ptr_glue (virtual calls), faked by the test library.
namespace itemlist_callee {
// Slot 0x14 of a file storage's vtable: opens `name` (the main app passes mode 0x80000001, data NULL); returns the TbError word.
int32_t StorageOpen(bfl::FileSystem::CFileStorage* storage, bfl::FileSystem::CFile& file, const char* name, uint32_t mode, void* data);
// Slot 8 of an item's vtable (at 0x30): the deleting destructor.
void ItemDelete(CTPItemResource* item, int16_t flag);
// Slot 8 of the item list's vtable: the deleting destructor.
void ItemListDelete(TbPtrList<CTPItemResource>* list, int16_t flag);
}
