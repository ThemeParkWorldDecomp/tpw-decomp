// WadNameTree.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of WadNameTree code, named after it.

#include "bfl_WadFile.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include <algorithm>
#include <cstring>
#include <new>

// The name tree of a WAD archive (bullfrog library): the archive's directory read into a tree
// of folders and files, and the iterator that answers a search pattern or an exact name.
//
// Initialise reads the directory once: every name is folded to lower case, an entry whose
// name has a folder part moves the "current folder", and an entry without one goes into the
// folder of the entry before it. A lookup of a name without wildcards compares the hash the
// archive stores for each entry before it compares names; a pattern goes through the masks
// of TbStringMatch. Nodes come from allocators that hand out blocks of 100.

namespace bfl::FileSystem {

template<class T> NodeAllocator<T>::NodeAllocator() : mBlock(new Block), mUsed(0) {
    mBlock->mNext = nullptr;
    mBlock->mNodes = new T[100];
}
template<class T> NodeAllocator<T>::~NodeAllocator() {
    while (mBlock != nullptr) {
        Block* next = mBlock->mNext;
        delete[] mBlock->mNodes;
        delete mBlock;
        mBlock = next;
    }
}
template<class T> T* NodeAllocator<T>::Alloc() {
    if (mUsed > 99) {
        auto* block = new Block;
        block->mNext = mBlock;
        block->mNodes = new T[100];
        mBlock = block;
        mUsed = 0;
    }
    return mBlock->mNodes + mUsed++;
}
template NodeAllocator<DirNode>::NodeAllocator();
template NodeAllocator<DirNode>::~NodeAllocator();
template DirNode* NodeAllocator<DirNode>::Alloc();
template NodeAllocator<FileNode>::NodeAllocator();
template NodeAllocator<FileNode>::~NodeAllocator();
template FileNode* NodeAllocator<FileNode>::Alloc();

WadNameTree::WadNameTree() : mRoot(nullptr), mNames(nullptr), mIterator(nullptr) {}
WadNameTree::~WadNameTree() { Close(); }
void WadNameTree::Close() {
    delete mRoot;
    if (mNames != nullptr) ::operator delete[](mNames);
    delete mIterator;
    mRoot = nullptr;
    mNames = nullptr;
    mIterator = nullptr;
}

TbError WadNameTree::Initialise(CDiscFile& file, bool borrowNames) {
    if (mRoot != nullptr) delete mRoot;
    mRoot = new (std::nothrow) DirNode;
    mIterator = new (std::nothrow) TreeIterator;
    if (file.Seek(0, 0) != 0) return TbError(-1);
    WadHeader header;
    if (file.Read(&header, 0x58) != 0x58) return TbError(-1);
    SamsUtilities::USwapInplace32(header.mSignature);
    SamsUtilities::USwapInplace32(header.mVersion);
    SamsUtilities::USwapInplace32(header.mCount);
    SamsUtilities::_USwapBlock32(&header.mNameOffset, 3);
    const uint32_t bytes = header.mCount * 0x28;
    auto* entries = static_cast<uint32_t*>(::operator new[](bytes));
    if (static_cast<uint32_t>(file.Read(entries, bytes)) != bytes) {
        ::operator delete[](entries);
        return TbError(-1);
    }
    if (borrowNames) {
        mNames = static_cast<char*>(::operator new[](header.mNameSize));
        if (static_cast<uint32_t>(file.Seek(header.mNameOffset, 0)) != header.mNameOffset ||
            static_cast<uint32_t>(file.Read(mNames, header.mNameSize)) != header.mNameSize) {
            ::operator delete[](entries);
            ::operator delete[](mNames);
            // The original leaves the pointer, and Close then frees the block a second
            // time; here the tree forgets it.
            mNames = nullptr;
            return TbError(-1);
        }
    }
    DirNode* directory = mRoot;
    uint32_t offset = 0x58;
    for (uint32_t index = 0; index < header.mCount; ++index, offset += 0x28) {
        uint32_t* entry = entries + index * 10;
        for (int i = 0; i != 6; ++i) SamsUtilities::USwapInplace32(entry[i]);
        SamsUtilities::USwapBlock32(entry + 6, 4);
        char* name;
        if (borrowNames) name = mNames + entry[1] - header.mNameOffset;
        else {
            name = static_cast<char*>(::operator new[](entry[2] + 1));
            if (static_cast<uint32_t>(file.Seek(entry[1], 0)) != entry[1] ||
                static_cast<uint32_t>(file.Read(name, entry[2] + 1)) != entry[2] + 1) {
                ::operator delete[](name);
                ::operator delete[](entries);
                return TbError(-1);
            }
        }
        TbStaticStringTemplate<char> view(name, static_cast<uint32_t>(std::strlen(name) + 1), 0);
        for (int32_t i = 0; i < view.Length(); ++i) {
            const uint8_t ch = static_cast<uint8_t>(name[i]);
            name[i] = ch == '/' ? ':' : ch == 0xff ? static_cast<char>(0xff) : static_cast<char>(msl_lower_map[ch]);
        }
        const int32_t split = view.FindReverseOneOf("\\/:", 0xffffffff);
        if (split != -1) {
            name[split] = 0;
            directory = mRoot->FindOrAddSubdir(name, mDirs, borrowNames);
        }
        directory->AddItem(name + split + 1, entry[5] == 0 ? entry[4] : entry[6], offset, entry[0], mFiles, borrowNames);
    }
    ::operator delete[](entries);
    return TbError(0);
}

TbError WadNameTree::FindFirst(CFileInfo& info, const TbDynamicStringTemplate<char>& pattern) {
    mIterator->init(*this, pattern, nullptr);
    if (mIterator->done()) return TbError(-1);
    mIterator->info(info);
    return TbError(0);
}
TbError WadNameTree::FindNext(CFileInfo& info) {
    mIterator->next();
    if (mIterator->done()) return TbError(-1);
    mIterator->info(info);
    return TbError(0);
}

DirNode::~DirNode() {
    if (mOwnsName) ::operator delete[](mName);
}
void DirNode::AddItem(char* name, uint32_t size, uint32_t offset, uint32_t hash,
                      NodeAllocator<FileNode>& allocator, bool borrow) {
    FileNode* node = allocator.Alloc();
    node->mOwnsName = !borrow;
    if (borrow) node->mName = name;
    else {
        node->mName = static_cast<char*>(::operator new[](std::strlen(name) + 1));
        std::strcpy(node->mName, name);
    }
    node->mHash = hash;
    node->mSize = size;
    node->mEntryOffset = offset;
    mFiles.AddTail(node);
}
DirNode* DirNode::FindOrAddSubdir(char* path, NodeAllocator<DirNode>& allocator, bool borrow) {
    TbStaticStringTemplate<char> view(path, static_cast<uint32_t>(std::strlen(path) + 1), 0);
    int32_t split = view.FindOneOf("\\/:", 0);
    DirNode* found = nullptr;
    for (auto* item = mDirs.mHead; item != nullptr; item = item->mNext) {
        if (view.Left(static_cast<uint32_t>(split)) == item->mData->mName) {
            found = item->mData;
            break;
        }
    }
    if (found == nullptr) {
        if (split != -1) path[split] = 0;
        found = allocator.Alloc();
        found->mOwnsName = !borrow;
        if (borrow) found->mName = path;
        else {
            found->mName = static_cast<char*>(::operator new[](std::strlen(path) + 1));
            std::strcpy(found->mName, path);
        }
        mDirs.AddHead(found);
    }
    return split == -1 ? found : found->FindOrAddSubdir(path + split + 1, allocator, borrow);
}
DirNode* DirNode::FindSubDir(const TbDynamicStringTemplate<char>& path, TbDynamicStringTemplate<char>& prefix) {
    const int32_t split = path.FindOneOf("\\/:", 0);
    for (auto* item = mDirs.mHead; item != nullptr; item = item->mNext) {
        DirNode* node = item->mData;
        if (!(path.Left(static_cast<uint32_t>(split)) == node->mName)) continue;
        if (split == -1) {
            prefix += static_cast<const TbStringBase<char>&>(path);
            prefix += ':';   // (the folders above it keep the separator of the pattern)
            return node;
        }
        TbDynamicStringTemplate<char> first(path.Left(static_cast<uint32_t>(split + 1)));
        prefix += static_cast<const TbStringBase<char>&>(first);
        TbDynamicStringTemplate<char> rest(path.Mid(static_cast<uint32_t>(split + 1)));
        return node->FindSubDir(rest, prefix);
    }
    return nullptr;
}

TreeIterator::TreeIterator()
    : mDir(nullptr), mNext(nullptr), mCurrent(nullptr), mDirectory(true) {}
TreeIterator::TreeIterator(WadNameTree& tree, const TbDynamicStringTemplate<char>& pattern, uint32_t* offset)
    : TreeIterator() {
    init(tree, pattern, offset);
}
void TreeIterator::init(WadNameTree& tree, const TbDynamicStringTemplate<char>& pattern, uint32_t* offset) {
    TbDynamicStringTemplate<char> source(pattern);
    char* data = source.LockBuffer(source.Length());
    for (char* p = data; *p != 0; ++p) if (*p == ':' || *p == '/') *p = '\\';
    source.ReleaseBuffer(0x7fffffff);
    mDir = tree.mRoot;
    mNext = mCurrent = nullptr;
    mDirectory = false;
    if (mDir == nullptr) return;
    mPrefix = "";
    uint32_t start = 0;
    uint32_t length = source.Length();
    if (source.Right(1) == "\\" || source.Right(1) == "/" || source.Right(1) == ":") --length;
    if (source.Left(1) == "\\" || source.Left(1) == "/" || source.Left(1) == ":") {
        start = 1;
        --length;
    }
    TbDynamicStringTemplate<char> normalized(source.Mid(start, length));
    for (int32_t i = 0; i < normalized.Length(); ++i) {
        const uint8_t ch = static_cast<uint8_t>(normalized.mData[i]);
        normalized.mData[i] = ch == '/' ? ':' : ch == 0xff ? static_cast<char>(0xff) : static_cast<char>(msl_lower_map[ch]);
    }
    mExact = normalized.Find('*', 0) == -1 && normalized.Find('?', 0) == -1;
    mHash = 0;
    if (mExact) {
        const auto* p = reinterpret_cast<const uint8_t*>(normalized.mData);
        uint32_t count = static_cast<uint32_t>(std::strlen(normalized.mData));
        for (; count >= 4; count -= 4, p += 4) {
            for (int i = 0; i < 4; ++i) mHash += (p[i] == 0xff ? 0xffffffff : msl_upper_map[p[i]]) << i;
        }
        for (uint32_t i = 0; i < count; ++i) mHash += p[i] == 0xff ? 255 : msl_upper_map[p[i]];
    }
    const int32_t split = normalized.FindReverseOneOf("\\/:", 0xffffffff);
    if (split != -1) {
        TbDynamicStringTemplate<char> path(normalized.Left(static_cast<uint32_t>(split)));
        mDir = mDir->FindSubDir(path, mPrefix);
    }
    if (mDir == nullptr) return;
    mPattern = normalized.Mid(static_cast<uint32_t>(split + 1));
    if (!mMask.SetMask(mPattern.mData).Succeeded()) return;
    mMatchWithoutExtension = false;
    if (mPattern.Right(2) == ".*") {
        mMatchWithoutExtension = true;
        TbDynamicStringTemplate<char> shorter(mPattern.Left(mPattern.Length() - 2));
        if (!mExtensionMask.SetMask(shorter.mData).Succeeded()) return;
    }
    mNext = mDir->mDirs.mHead;
    mDirectory = true;
    next();
    if (offset != nullptr) {
        *offset = mCurrent == nullptr || mDirectory ? 0 : static_cast<TbTList<FileNode*, FileNode*>::CNode*>(mCurrent)->mData->mEntryOffset;
    }
}

void TreeIterator::next() {
    if (mDir == nullptr) return;
    if (mDirectory) {
        while (mNext != nullptr) {
            auto* item = static_cast<TbTList<DirNode*, DirNode*>::CNode*>(mNext);
            mNext = item->mNext;
            if (mMask.MatchString(item->mData->mName) ||
                (mMatchWithoutExtension && mExtensionMask.MatchString(item->mData->mName))) {
                mCurrent = item;
                return;
            }
        }
        mDirectory = false;
        mCurrent = nullptr;
        mNext = mDir->mFiles.mHead;
    }
    while (mNext != nullptr) {
        auto* item = static_cast<TbTList<FileNode*, FileNode*>::CNode*>(mNext);
        mNext = item->mNext;
        const bool match = mExact ? item->mData->mHash == mHash && mPattern == item->mData->mName :
            mMask.MatchString(item->mData->mName) ||
            (mMatchWithoutExtension && mExtensionMask.MatchString(item->mData->mName));
        if (match) {
            mCurrent = item;
            return;
        }
    }
    mCurrent = nullptr;
}
bool TreeIterator::done() { return mCurrent == nullptr && !mDirectory; }
void TreeIterator::info(CFileInfo& info) {
    if (mCurrent == nullptr) return;
    const char* name;
    if (mDirectory) {
        name = static_cast<TbTList<DirNode*, DirNode*>::CNode*>(mCurrent)->mData->mName;
        info.mF00 = 0;
        info.mAttributes = 0x10;
    } else {
        auto* node = static_cast<TbTList<FileNode*, FileNode*>::CNode*>(mCurrent)->mData;
        name = node->mName;
        info.mF00 = node->mSize;
        info.mAttributes = 0x81;
    }
    std::memset(info.mF08, 0, sizeof(info.mF08));
    info.mName = info.mNameBuffer + mPrefix.Length();
    std::strncpy(info.mNameBuffer, mPrefix.mData, mPrefix.Length());
    std::strncpy(info.mName, name, std::strlen(name) + 1);
}
FileNode::~FileNode() {
    if (mOwnsName) ::operator delete[](mName);
}
FileNode::FileNode() : mOwnsName(false) {}
DirNode::DirNode() : mOwnsName(false) {}
} // namespace bfl::FileSystem

// The eight node-list methods are the existing generic definitions, now emitted here.
template class TbTList<bfl::FileSystem::DirNode*, bfl::FileSystem::DirNode*>;
template class TbTList<bfl::FileSystem::FileNode*, bfl::FileSystem::FileNode*>;
