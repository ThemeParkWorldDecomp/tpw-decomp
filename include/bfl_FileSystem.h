// bfl::FileSystem (bullfrog library): file storages, files and directory entries.
// PARTIAL: the main-app interfaces and the u11 access/disc methods. Members outside
// that inventory remain external declarations.
#pragma once
#include <cstdint>
#include <cstring>

#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "LbSemaphore.h"

namespace NS_MacDoze {
struct InternalHandle;
struct _WIN32_FIND_DATAA;
}

namespace bfl {
namespace FileSystem {

class CFile;
class CAccessItem;
class _TbStorageAccess;

// A directory entry, as FindFirst and FindNext fill it (0x128 bytes on the Mac). The
// constructor is inline: the name points at the entry's own buffer.
class CFileInfo {
public:
    CFileInfo() : mName(mNameBuffer) { mNameBuffer[0] = '\0'; }
    // A copy has its name pointer at the same place in its OWN buffer (the main app passes
    // entries by value: LoadAllUIMeshes, inline at each call).
    CFileInfo(const CFileInfo& other) { *this = other; }
    ~CFileInfo() {}             // main 0x10004994; bullfrog 0x10008fbc
    // Inline (TPSettings.cpp's searches reset an entry from a blank one before each storage):
    // the header words, the name, and the name pointer at the same place in the own buffer.
    CFileInfo& operator=(const CFileInfo& other) {
        mF00 = other.mF00;
        mAttributes = other.mAttributes;
        std::memcpy(mF08, other.mF08, sizeof mF08);
        std::strcpy(mNameBuffer, other.mNameBuffer);
        mName = mNameBuffer + (other.mName - other.mNameBuffer);
        return *this;
    }

    uint32_t mF00;              // 0x00
    uint32_t mAttributes;       // 0x04  0x10: a directory
    uint8_t mF08[0x20 - 0x08];
    char* mName;                // 0x20
    char mNameBuffer[0x104];    // 0x24
};

// Unit w222 (src/bfl_FileSystem_more.cpp): UTF-16 directory entry, Mac size 0x22c.
class CUniFileInfo {
public:
    CUniFileInfo() : mName(mNameBuffer) { mNameBuffer[0] = 0; }
    uint32_t mF00;                    // +0x00
    uint32_t mAttributes;             // +0x04
    uint8_t mF08[0x18];               // +0x08
    char16_t* mName;                  // +0x20
    char16_t mNameBuffer[0x104];      // +0x24
};

// A place files are opened from (the bullfrog library keeps the ones that exist). All
// the remaining members are bullfrog imports. Vtable slots on the Mac: 8 Close, 0xc FindFirst, 0x10
// FindNext, 0x14 OpenFile, 0x18 and 0x1c FullPath, 0x20 GetRootPath, 0x24 SetAttribs, 0x28
// CreateDir, 0x2c Delete, 0x30 Move, 0x34 the destructor. Close's and GetRootPath's return
// types are not in their names (the main app does not call them).
class CFileStorage {
public:
    CFileStorage();
    CFileStorage(const CFileStorage& other);   // 0x1000b8e0: shares the other's access chain
    CFileStorage& operator=(const CFileStorage& other);
    virtual void Close();
    virtual TbError FindFirst(const char* pattern, CFileInfo& info);
    virtual TbError FindNext(CFileInfo& info);
    // Opens `name` in this storage into `file` (the main app passes mode 0x80000001, data NULL).
    virtual TbError OpenFile(CFile& file, const char* name, uint32_t mode, void* data);
    virtual TbError FullPath(char* path, const char* name, uint32_t size);
    // The full name of `name` in this storage.
    virtual TbError FullPath(TbDynamicStringTemplate<char>& path, const TbDynamicStringTemplate<char>& name);
    virtual const char* GetRootPath();
    virtual TbError SetAttribs(const char* name, uint32_t attributes);
    virtual TbError CreateDir(const char* name);
    virtual TbError Delete(const char* name);
    virtual TbError Move(const char* from, const char* to);
    virtual ~CFileStorage();
    // Unit w222 (src/bfl_FileSystem_more.cpp): non-virtual wide wrappers.
    TbError FindFirst(const char16_t*, CUniFileInfo&); // 0x1000c2b4
    TbError FindNext(CUniFileInfo&);                 // 0x1000c448
    TbError OpenFile(CFile&, const char16_t*, uint32_t, void*); // 0x1000c5b0
    TbError FullPath(char16_t*, const char16_t*, uint32_t); // 0x1000c69c
    TbError FullPath(TbDynamicStringTemplate<char16_t>&, const TbDynamicStringTemplate<char16_t>&); // 0x1000c7a0
    TbError SetAttribs(const char16_t*, uint32_t);    // 0x1000c8ec
    TbError CreateDir(const char16_t*);              // 0x1000c9c4
    TbError Delete(const char16_t*);                 // 0x1000ca88
    TbError Move(const char16_t*, const char16_t*);   // 0x1000cb48
    void AttachAccessClass(_TbStorageAccess* access);
    _TbStorageAccess* mAccess;   // Mac +4
};

// A storage on disc. Its constructor and destructor are inline in the main app (they
// only set the vtable, 0x10278bf0, whose other slots are the library's); the deleting
// destructor's out-of-line copy is 0x10004a10 (FS_System.cpp range).
class CDiscFileStorage : public CFileStorage {
public:
    CDiscFileStorage() {}
    ~CDiscFileStorage() override {} // Unit w222: bullfrog 0x1000d12c
    TbError Open(const char* path);   // bullfrog import: the storage for a directory
    // Unit w222 (src/bfl_FileSystem_more.cpp).
    TbError Open(const char16_t* path); // 0x1000b060
    // Unit v5110 (src/TbIResourceIndex_bfl_FileSystem_CDiscFileStorage_Dtor_CDiscFileSt.cpp): the ltms
    // library's out-of-line destructor 0x100034c0 (this in r3, the hidden "delete storage" short in r4).
    static CDiscFileStorage* DestroyLtms(CDiscFileStorage* self, int16_t deleteFlag);
    // Unit v51112 (src/AudioPlaceHolder_LbMaths_Random_LbMaths_RangedRandomSlow.cpp): the sound program's out-of-line
    // destructor 0x10020c50 (same body as DestroyLtms).
    static CDiscFileStorage* DestroySound(CDiscFileStorage* self, int16_t deleteFlag);
};

// A storage in a .wad archive. Its constructor and destructor are inline in the main app too
// (vtable 0x102a70d0); the deleting destructor's out-of-line copy is 0x1015f8a0
// (TPSettings.cpp range).
class CWadFileStorage : public CFileStorage {
public:
    CWadFileStorage() {}
    ~CWadFileStorage() override {} // Unit w222: bullfrog 0x1000d1d4
    TbError Open(const char* path, uint32_t flags);   // bullfrog import (the main app passes 0)
    // Unit w222 (src/bfl_FileSystem_more.cpp).
    TbError Open(const char16_t* path, uint32_t flags); // 0x1000d3b8
};

// The access layer behind a CFile, Mac {vptr, next +4, references +8}.
// Read/Write/Seek/GetSize/GetPos are pure slots in the Mac's base vtable. Their
// declarations also permit the older test libraries' concrete import fakes.
class CAccessItem {
public:
    CAccessItem();
    virtual ~CAccessItem();                             // 8
    virtual int32_t Read(void* dest, uint32_t size);
    virtual int32_t Write(const void* source, uint32_t size);
    virtual int32_t Seek(int32_t offset, uint32_t whence); // 0x14
    virtual int32_t GetSize();                           // 0x18
    virtual int32_t GetPos();                            // 0x1c
    virtual const uint8_t* MapFile(int32_t offset, int32_t length); // 0x20
    virtual void UnmapFile(const uint8_t* data);          // 0x24
    virtual TbError Flush();                            // 0x28
    virtual int32_t GetMacFileReference();               // 0x2c
    virtual void Close();                               // 0x30
    virtual void AttachAccessClass(CAccessItem* access); // 0x34
    virtual _LbSemaphore* GetSemaphore();               // 0x38
    void AddRef();
    int32_t Release();
    CAccessItem* mNext;                                  // +4
    int32_t mReferences;                                // +8
};
using CFileImplementation = CAccessItem; // compatibility with the earlier partial header

class _TbBufferAccess : public CAccessItem {
public:
    _TbBufferAccess();
    ~_TbBufferAccess() override;
    TbError Open(uint32_t size);
    int32_t Read(void*, uint32_t) override;
    int32_t Write(const void*, uint32_t) override;
    int32_t Seek(int32_t, uint32_t) override;
    int32_t GetSize() override;
    int32_t GetPos() override;
    void Close() override;
    void AttachAccessClass(CAccessItem*) override;
    int32_t ReadDirectToOutputBuffer(void*, uint32_t);
    uint32_t CacheBufferToOutputBuffer(void*, uint32_t);
    void RefillCacheBuffer();
    int32_t BasicRead(void*, uint32_t);
    uint8_t* mBuffer;        // +0xc
    uint8_t* mCacheBuffer;   // +0x10
    uint32_t mPosition;      // +0x14
    uint32_t mCacheStart;    // +0x18
    uint32_t mCacheEnd;      // +0x1c, untouched by constructor
    uint32_t mBaseOffset;    // +0x20
    uint32_t mBufferSize;    // +0x24
};

class _TbQuickPosAccess : public CAccessItem {
public:
    _TbQuickPosAccess();
    ~_TbQuickPosAccess() override;
    int32_t Read(void*, uint32_t) override;
    int32_t Write(const void*, uint32_t) override;
    int32_t Seek(int32_t, uint32_t) override;
    int32_t GetSize() override;
    int32_t GetPos() override;
    void Close() override;
    uint32_t mPosition;      // +0xc
};

class _TbSharedAccess : public _TbQuickPosAccess {
public:
    _TbSharedAccess();
    ~_TbSharedAccess() override;
    int32_t Read(void*, uint32_t) override;
    int32_t Write(const void*, uint32_t) override;
    int32_t Seek(int32_t, uint32_t) override;
    int32_t GetSize() override;
    int32_t GetPos() override;
    void AttachAccessClass(CAccessItem*) override;
    void ValidateFilePosition();
};

// A separate storage access hierarchy, Mac 0x24 bytes, not a CAccessItem subclass.
class _TbStorageAccess {
public:
    _TbStorageAccess();
    virtual ~_TbStorageAccess();                         // 8
    virtual TbError OpenFile(CFile&, const char*, uint32_t, void*); // 0xc
    virtual TbError FindFirst(const char*, CFileInfo&);   // 0x10
    virtual TbError FindNext(CFileInfo&);                // 0x14
    virtual TbError FullPath(char*, const char*, uint32_t); // 0x18
    virtual TbError FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&); // 0x1c
    virtual const char* GetRootPath();                  // 0x20
    virtual TbError SetAttribs(const char*, uint32_t);   // 0x24
    virtual TbError CreateDir(const char*);             // 0x28
    virtual TbError Delete(const char*);                // 0x2c
    virtual TbError Move(const char*, const char*);      // 0x30
    virtual void AttachAccessClass(_TbStorageAccess*);  // 0x34
    virtual void Close();                              // 0x38, pure on the Mac
    void AddRef();
    int32_t Release();                                  // 0x10007bc0
    _TbStorageAccess* mNext;                            // +4
    _LbSemaphore mSemaphore;                           // +8
    int32_t mReferences;                              // +0xc
    TbDynamicStringTemplate<char> mRoot;               // +0x10
};

class _TbFileStorageAccess : public _TbStorageAccess {
public:
    _TbFileStorageAccess();
    ~_TbFileStorageAccess() override;
    TbError Open(const char* path);
    TbError OpenFile(CFile&, const char*, uint32_t, void*) override;
    TbError FindFirst(const char*, CFileInfo&) override;
    TbError FindNext(CFileInfo&) override;
    TbError SetAttribs(const char*, uint32_t) override;
    TbError CreateDir(const char*) override;
    TbError Delete(const char*) override;
    TbError Move(const char*, const char*) override;
    void Close() override;
    virtual void _FindClose();                          // 0x3c
    void ConvertFindData(CFileInfo& info, const NS_MacDoze::_WIN32_FIND_DATAA& data);   // 0x10005780
    void* mFindHandle;                                  // +0x24, initially -1
};

// The access object behind a file on disc: a Win32 file handle from macdoze. Mac 0x20
// bytes, vtable 0x100719a4. Created by _TbFileStorageAccess::OpenFile.
class _TbDiscAccess : public CAccessItem {
public:
    _TbDiscAccess();                                     // 0x10004938
    ~_TbDiscAccess() override;                           // 0x100049dc
    TbError Open(const char* name, uint32_t mode);       // 0x10004a90
    int32_t Read(void* dest, uint32_t size) override;    // 0x10004c70
    int32_t Write(const void* source, uint32_t size) override;   // 0x10004d10
    int32_t Seek(int32_t offset, uint32_t whence) override;       // 0x10004db4
    int32_t GetSize() override;                          // 0x10004e44
    int32_t GetPos() override;                           // 0x10004eb0
    const uint8_t* MapFile(int32_t offset, int32_t length) override;   // 0x10004ef8
    void UnmapFile(const uint8_t* data) override;        // 0x1000505c
    TbError Flush() override;                            // 0x10005128
    int32_t GetMacFileReference() override;              // 0x100051c8
    void Close() override;                               // 0x10004bb0
    void AttachAccessClass(CAccessItem* access) override;   // 0x1000523c: nothing can follow a disc file
    _LbSemaphore* GetSemaphore() override;               // 0x100050dc
    NS_MacDoze::InternalHandle* mHandle;     // +0xc, -1 when closed
    uint32_t mPosition;                      // +0x10
    NS_MacDoze::InternalHandle* mMapping;    // +0x14, a file mapping made on demand
    const uint8_t* mView;                    // +0x18, the whole-file view of it
    _LbSemaphore mSemaphore;                 // +0x1c
};

// Unit w222 (src/bfl_FileSystem_more.cpp): Mac size 0x2c, vtable 0x10071a60.
class _TbMemFileAccess : public CAccessItem {
public:
    struct Block {
        Block* mPrev;       // Mac +0x00
        Block* mNext;       // +0x04
        uint8_t* mData;     // +0x08, follows the native header
        uint32_t mStart;    // +0x0c
        uint32_t mEnd;      // +0x10
    };
    explicit _TbMemFileAccess(uint32_t blockSize); // 0x10005f20
    ~_TbMemFileAccess() override;                 // 0x10006730
    void Close() override;                       // 0x10005fdc
    int32_t Read(void*, uint32_t) override;       // 0x10006040
    int32_t Write(const void*, uint32_t) override; // 0x100061a8
    int32_t Seek(int32_t, uint32_t) override;     // 0x10006360
    int32_t GetSize() override;                  // 0x10006478
    int32_t GetPos() override;                   // 0x100064c4
    TbError AddBlock();                          // 0x10006510
    void RemoveAllBlocks();                     // 0x10006628
    _LbSemaphore* GetSemaphore() override;       // 0x100066e0
    Block* mFirst;                              // +0x0c
    Block* mLast;                               // +0x10
    Block* mCurrent;                            // +0x14
    uint8_t* mCursor;                           // +0x18
    uint32_t mPosition;                         // +0x1c
    uint32_t mSize;                             // +0x20
    uint32_t mBlockSize;                        // +0x24
    _LbSemaphore mSemaphore;                    // +0x28
};

// An open file (8 bytes on the Mac: vtable, implementation). Read and Write are inline
// (out-of-line copies 0x100042dc and 0x10004550 in the FS_System.cpp range); the other
// members are the library's. Vtable slots on the Mac: 8 destructor, 0xc Close, 0x10 Read,
// 0x14 Write, 0x18 Seek, 0x1c GetSize, 0x20 GetPos, 0x24 MapFile, 0x28 Flush,
// 0x2c GetMacFileReference (verified at 0x10071dec in the bullfrog dump).
class CFile {
public:
    CFile();                    // bullfrog import
    CFile(const CFile& other);              // 0x1000b224: shares the other's access object
    CFile& operator=(const CFile& other);   // 0x1000b3dc
    virtual ~CFile();
    virtual void Close();
    virtual int32_t Read(void* dest, uint32_t size) {
        if (mImplementation == nullptr) return -1;
        return mImplementation->Read(dest, size);
    }
    virtual int32_t Write(const void* source, uint32_t size) {
        if (mImplementation == nullptr) return -1;
        return mImplementation->Write(source, size);
    }
    virtual int32_t Seek(uint32_t offset, uint32_t whence);   // whence 0 start, 1 current, 2 end; < 0 failed
    virtual int32_t GetSize();
    int32_t Size() { return GetSize(); }                // earlier main-app spelling
    virtual int32_t GetPos();                         // 0x20
    virtual void* MapFile();                         // 0x24
    virtual TbError Flush();                         // 0x28
    virtual int32_t GetMacFileReference();            // 0x2c

    CAccessItem* GetAccessClass();
    void AttachAccessClass(CAccessItem* access);
    CAccessItem* mImplementation;   // 0x04
};

class CDiscFile : public CFile {
public:
    CDiscFile();                // bullfrog import
    ~CDiscFile() override;      // bullfrog import
    virtual TbError Open(const char* name, uint32_t mode);   // 0x30, bullfrog import
};

// A file in memory. The main app makes one on its stack: its constructor and destructor are inline (the CFile's, with the vtable
// word set to 0x102a4d0c, the main app's CMemFile vtable). Open is a bullfrog import (0x1002951c, 156 bytes, hidden result in r3,
// the file in r4, the size in r5): a file that has no access object takes a new memory access of `size` bytes and answers 0, one
// that has one answers -1 and is left as it is.
class CMemFile : public CFile {
public:
    CMemFile() {}
    ~CMemFile() override {}
    TbError Open(uint32_t size);
};

class _TbBufferedFile : public CFile {
public:
    ~_TbBufferedFile() override; // 0x10023024, src/bfl_graphicfile.cpp
    TbError Open(CFile& file, uint32_t size);
};

}  // namespace FileSystem
}  // namespace bfl
