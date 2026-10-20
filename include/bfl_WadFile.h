// Bullfrog WAD storage, name tree and stored/RefPack access.
#pragma once
#include "bfl_FileSystem.h"
#include "TbStringMatch.h"

namespace bfl::FileSystem {

struct FileNode {
    FileNode();  // 0x10046824: initializes only ownership
    ~FileNode(); // 0x10046784
    char* mName;              // Mac +0
    bool mOwnsName;           // +4
    uint32_t mSize;           // +8
    uint32_t mEntryOffset;    // +0xc
    uint32_t mHash;           // +0x10
};

template<class T> class NodeAllocator {
public:
    struct Block {
        Block* mNext;        // Mac +0
        T* mNodes;           // +4
    };
    NodeAllocator();
    ~NodeAllocator(); // 0x10045c80 (DirNode), 0x10045f64 (FileNode)
    T* Alloc();
    Block* mBlock;            // +0
    int32_t mUsed;            // +4
};

struct DirNode {
    DirNode(); // 0x10046868: initializes ownership and lists
    ~DirNode();
    void AddItem(char* name, uint32_t size, uint32_t offset, uint32_t hash,
                 NodeAllocator<FileNode>& allocator, bool borrow);
    DirNode* FindOrAddSubdir(char* path, NodeAllocator<DirNode>& allocator, bool borrow);
    DirNode* FindSubDir(const TbDynamicStringTemplate<char>& path, TbDynamicStringTemplate<char>& prefix);
    char* mName;                // Mac +0
    bool mOwnsName;             // +4
    TbPtrList<DirNode> mDirs;   // +8
    TbPtrList<FileNode> mFiles; // +0x18
};

class TreeIterator;
class WadNameTree {
public:
    WadNameTree();
    ~WadNameTree(); // 0x10043e0c
    void Close();
    TbError Initialise(CDiscFile& file, bool borrowNames);
    TbError FindFirst(CFileInfo& info, const TbDynamicStringTemplate<char>& pattern);
    TbError FindNext(CFileInfo& info);
    DirNode* mRoot;                 // Mac +0
    char* mNames;                   // +4
    TreeIterator* mIterator;        // +8
    NodeAllocator<DirNode> mDirs;   // +0xc
    NodeAllocator<FileNode> mFiles; // +0x14
};

// No independent vtable: the first member is a dynamic string, Mac total size 0x60.
class TreeIterator {
public:
    TreeIterator(); // inline default state used by Initialise
    TreeIterator(WadNameTree&, const TbDynamicStringTemplate<char>&, uint32_t*);
    ~TreeIterator(); // 0x10009d8c
    void init(WadNameTree& tree, const TbDynamicStringTemplate<char>& pattern, uint32_t* offset);
    void next();
    bool done();
    void info(CFileInfo& info);
    TbDynamicStringTemplate<char> mPattern; // Mac +0
    DirNode* mDir;                         // +0x14
    POSITION mNext;                        // +0x18
    POSITION mCurrent;                     // +0x1c
    TbDynamicStringTemplate<char> mPrefix; // +0x20
    bool mDirectory;                       // +0x34
    bool mExact;                           // +0x35
    uint32_t mHash;                        // +0x38
    TbStringMatch mMask;                   // +0x3c
    TbStringMatch mExtensionMask;          // +0x4c
    bool mMatchWithoutExtension;           // +0x5c
};

struct WadHeader {
    uint32_t mSignature;  // +0, little-endian 0x42465744
    uint32_t mVersion;    // +4
    uint8_t mReserved[64];
    uint32_t mCount;      // +0x48
    uint32_t mNameOffset; // +0x4c
    uint32_t mNameSize;   // +0x50
    uint32_t mUnknown;    // +0x54
};

// A window onto an entry in the archive. Mac 0x1c, vtable 0x10071c48.
class _TbWadFileAccess : public CAccessItem {
public:
    _TbWadFileAccess();
    ~_TbWadFileAccess() override;
    TbError Open(uint32_t size, uint32_t baseOffset);
    int32_t Read(void*, uint32_t) override;
    int32_t Write(const void*, uint32_t) override;
    int32_t Seek(int32_t, uint32_t) override;
    int32_t GetSize() override;
    int32_t GetPos() override;
    const uint8_t* MapFile(int32_t, int32_t) override;
    void Close() override;
    uint32_t mSize;           // +0xc
    uint32_t mPosition;       // +0x10
    uint32_t mBaseOffset;     // +0x14
    const uint8_t* mView;     // +0x18
};

// RefPack entries are expanded when attached. Mac 0x18, vtable 0x10071b38.
class _TbRefpackFileAccess : public CAccessItem {
public:
    _TbRefpackFileAccess();
    ~_TbRefpackFileAccess() override;
    TbError Open(uint32_t size);
    int32_t Read(void*, uint32_t) override;
    int32_t Write(const void*, uint32_t) override;
    int32_t Seek(int32_t, uint32_t) override;
    int32_t GetSize() override;
    int32_t GetPos() override;
    void Close() override;
    void AttachAccessClass(CAccessItem*) override;
    uint32_t mSize;       // +0xc
    uint8_t* mBuffer;     // +0x10
    uint8_t* mCursor;     // +0x14
};

// Unit w222 (src/bfl_FileSystem_more.cpp): Mac 0x18, vtable 0x10071d60.
class _TbZLibFileAccess : public CAccessItem {
public:
    _TbZLibFileAccess();
    ~_TbZLibFileAccess() override;
    TbError Open(uint32_t size);
    int32_t Read(void*, uint32_t) override;
    int32_t Write(const void*, uint32_t) override;
    int32_t Seek(int32_t, uint32_t) override;
    int32_t GetSize() override;
    int32_t GetPos() override;
    void Close() override;
    void AttachAccessClass(CAccessItem*) override;
    uint32_t mSize;       // +0x0c
    uint8_t* mBuffer;     // +0x10
    uint8_t* mCursor;     // +0x14
};

// Unit w222 (src/bfl_FileSystem_more.cpp), 0x1000a79c.
int32_t _GetMoreData(void* dest, uint32_t size, void* context);

class _TbWadStorageAccess : public _TbStorageAccess {
public:
    _TbWadStorageAccess();
    ~_TbWadStorageAccess() override;
    TbError Open(const char* path, uint32_t flags);
    TbError OpenFile(CFile&, const char*, uint32_t, void*) override;
    TbError FindFirst(const char*, CFileInfo&) override;
    TbError FindNext(CFileInfo&) override;
    TbError SetAttribs(const char*, uint32_t) override;
    TbError CreateDir(const char*) override;
    TbError Delete(const char*) override;
    TbError Move(const char*, const char*) override;
    void Close() override;
    void StripSubPath(CFileInfo&);
    TbError OpenWadItem(CFile&, const char*);
    TbDynamicStringTemplate<char> mSubpath; // Mac +0x24
    int32_t mFindIndex;                    // +0x38
    TbStringMatch mMask;                   // +0x3c
    TbDynamicStringTemplate<char> mPattern; // +0x4c
    WadHeader mHeader;                     // +0x60
    CDiscFile mFile;                       // +0xb8
    CAccessItem* mOriginalAccess;          // +0xc0
    WadNameTree mTree;                     // +0xc4, total Mac 0xe0
};

class CWadFile : public CFile {
public:
    ~CWadFile() override; // inherits all CFile slots
    TbError Open(const char* name, const char* storagePath);
};

} // namespace bfl::FileSystem
