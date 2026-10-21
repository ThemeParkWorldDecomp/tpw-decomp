// Test library for the archive name tree and the wildcard masks (src/bullfrog/TbWadStorageAccess.cpp, src/bullfrog/WadNameTree.cpp,
// src/bullfrog/FileSystem.cpp, src/bullfrog/TbWadStorageAccess.cpp; libtpw_unit_bflWadTree.dylib; see tests/test_bflWadTree.py).
//
// The tree, its iterator, the node allocators and lists, the masks and the bullfrog strings
// are all real; the archive is a file object over the test's bytes whose Seek and Read
// record what they are asked and can be told to fail.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

#include "bfl_WadFile.h"
#include "calllog.h"
#include "platform/samsutils.h"

using namespace bfl::FileSystem;
using calllog::ev;
using calllog::ret;

namespace {
constexpr int32_t kAsAsked = -9999;   // a queued result that means "what was asked for"
uint8_t gBytes[0x10000];
uint32_t gLength, gPosition;
}  // namespace

#define TPW_RECORDED_ACCESS_IO
#include "fake_file_slots.h"
namespace bfl::FileSystem {
CAccessItem::~CAccessItem() {}
// Copies what the file has at the current position and returns how much that was, unless
// the "Read" queue says otherwise.
int32_t CAccessItem::Read(void* out, uint32_t size) {
    ev("File.Read", {size, gPosition});
    const uint32_t available = gPosition <= gLength ? gLength - gPosition : 0;
    const uint32_t count = std::min(size, available);
    int32_t result = static_cast<int32_t>(ret("Read", kAsAsked));
    if (result == kAsAsked) result = static_cast<int32_t>(count);
    std::memset(out, 0xee, size);
    std::memcpy(out, gBytes + std::min(gPosition, gLength), count);
    gPosition += size;
    return result;
}
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::GetSize() { return static_cast<int32_t>(gLength); }
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    ev("File.Seek", {offset, whence});
    gPosition = offset;
    const int32_t result = static_cast<int32_t>(ret("Seek", kAsAsked));
    return result == kAsAsked ? static_cast<int32_t>(offset) : result;
}
CDiscFile::CDiscFile() {}
CDiscFile::~CDiscFile() {}
TbError CDiscFile::Open(const char*, uint32_t) { return TbError(-1); }
}  // namespace bfl::FileSystem

// The tree asks for its root and its iterator with the form of new that returns null: through
// the counted operator like every other block.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return operator new(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return operator new[](size); }

namespace SamsUtilities {
void _USwapBlock32(uint32_t*, int32_t) {}   // a little-endian host has the file's words already
}
// The strings' formatting members are another unit's; nothing here calls them.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

namespace {
CAccessItem gAccess;
CDiscFile* File() {
    static CDiscFile file;
    file.mImplementation = &gAccess;
    return &file;
}
WadNameTree* gTree;

void Describe(char* out, int32_t result, const CFileInfo& info) {
    if (result < 0) std::snprintf(out, 512, "%d", result);
    else std::snprintf(out, 512, "%d|%u|%u|%s|%d", result, info.mF00, info.mAttributes, info.mNameBuffer,
                       static_cast<int>(info.mName - info.mNameBuffer));
}
}  // namespace

extern "C" {
// SetMask then MatchString: the two results, and the mask's parts joined by '|'.
int32_t tpw_wt_mask(const char* mask, const char* text, char* parts) {
    TbStringMatch match;
    const int32_t set = match.SetMask(mask);
    parts[0] = 0;
    for (auto* node = match.mHead; node != nullptr; node = node->mNext) {
        std::strcat(parts, node->mData.mData);
        std::strcat(parts, "|");
    }
    return set * 2 + (match.MatchString(text) ? 1 : 0);
}

void tpw_wt_begin(const uint8_t* bytes, uint32_t length) {
    length = std::min<uint32_t>(length, sizeof gBytes);
    std::memcpy(gBytes, bytes, length);
    gLength = length;
    gPosition = 0;
    gTree = new WadNameTree;
}
// 0 Initialise(borrow names: a), 1 FindFirst(text), 2 FindNext, 3 the entry `text` exactly
// (through an iterator of its own: "done|offset"), 4 Close, 5 destroy and make anew.
// `heap`: the blocks operator new, new[], delete and delete[] handled during the operation
// itself (not the pattern string this function makes for it).
void tpw_wt_op(int32_t op, int32_t a, const char* text, char* out, int32_t* heap) {
    out[0] = 0;
    CFileInfo info;   // blank, as the PowerPC side's: a search that finds nothing to describe leaves it alone
    info.mF00 = info.mAttributes = 0;
    TbDynamicStringTemplate<char> pattern(text);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
    switch (op) {
    case 0: std::snprintf(out, 512, "%d", static_cast<int32_t>(gTree->Initialise(*File(), a != 0))); break;
    case 1: Describe(out, gTree->FindFirst(info, pattern), info); break;
    case 2: Describe(out, gTree->FindNext(info), info); break;
    case 3: {
        uint32_t offset = 0x5a5a5a5a;
        TreeIterator iterator(*gTree, pattern, &offset);
        std::snprintf(out, 512, "%d|%u", iterator.done() ? 1 : 0, offset);
        break;
    }
    case 4: gTree->Close(); break;
    case 5:   // (in place: the tree's own block is not part of the traffic)
        gTree->~WadNameTree();
        new (gTree) WadNameTree;
        break;
    }
    std::memcpy(heap, tpw_cl_heap, sizeof tpw_cl_heap);
}
void tpw_wt_end() {
    delete gTree;
    gTree = nullptr;
}
}
