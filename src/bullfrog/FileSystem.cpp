// FileSystem.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of FileSystem code, named after it.

#include "bfl_FileSystem.h"
#include "bfl_Refpack.h"
#include "bfl_Strings.h"
#include "bfl_WadFile.h"
#include "bfl_ZLib.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "TbDynamicStringTemplate.h"
#include "TbFPUPrecision.h"
#include "TbStringMatch.h"
#include "TbTList.h"
#include "WinScrn.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>

// The bullfrog access chains and disc path.

namespace bfl::FileSystem {

CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
void CAccessItem::AddRef() { NS_MacDoze::InterlockedIncrement(&mReferences); }

int32_t CAccessItem::Release() {
    if (NS_MacDoze::InterlockedDecrement(&mReferences) > 0) return mReferences;
    Close();
    if (mNext != nullptr) {
        mNext->CAccessItem::Release();
        mNext = nullptr;
    }
    delete this;
    return 0;
}

TbError CDiscFileStorage::Open(const char* path) {
    Close();
    auto* access = new (std::nothrow) _TbFileStorageAccess();
    if (!access->Open(path).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}

CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() { Close(); }
void CFile::Close() {
    if (mImplementation != nullptr) {
        mImplementation->CAccessItem::Release();
        mImplementation = nullptr;
    }
}
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    return mImplementation == nullptr ? -1 : mImplementation->Seek(static_cast<int32_t>(offset), whence);
}
int32_t CFile::GetSize() { return mImplementation == nullptr ? -1 : mImplementation->GetSize(); }
int32_t CFile::GetPos() { return mImplementation == nullptr ? -1 : mImplementation->GetPos(); }
CAccessItem* CFile::GetAccessClass() { return mImplementation; }
void CFile::AttachAccessClass(CAccessItem* access) {
    if (access == nullptr) return;
    access->CAccessItem::AddRef();
    if (mImplementation != nullptr) access->AttachAccessClass(mImplementation);
    mImplementation = access;
}

CFileStorage::CFileStorage() : mAccess(nullptr) {}
CFileStorage::~CFileStorage() { Close(); }
CFileStorage& CFileStorage::operator=(const CFileStorage& other) {
    if (&other != this) {
        Close();
        AttachAccessClass(other.mAccess);
    }
    return *this;
}
TbError CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    return mAccess == nullptr ? TbError(-1) : mAccess->FindFirst(pattern, info);
}
TbError CFileStorage::FindNext(CFileInfo& info) {
    return mAccess == nullptr ? TbError(-1) : mAccess->FindNext(info);
}
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    return mAccess == nullptr ? TbError(-1) : mAccess->OpenFile(file, name, mode, data);
}
void CFileStorage::AttachAccessClass(_TbStorageAccess* access) {
    access->_TbStorageAccess::AddRef();
    if (mAccess != nullptr) access->AttachAccessClass(mAccess);
    mAccess = access;
}
}

// The bullfrog file system's disc side, the part reached through vtables: the access
// object behind a disc file (_TbDiscAccess), the disc storage's members
// (_TbFileStorageAccess: opening, listing, deleting, moving), the storage access base
// (_TbStorageAccess), CAccessItem's own slots, and the CFile and CFileStorage members that
// forward to them (bullfrog shared, 0x10004938..0x10005dc8, 0x10007a9c..0x10008024,
// 0x1000a86c..0x1000ae98, 0x1000b224..0x1000c220). The disk itself is macdoze's Win32
// file API (include/platform/macdoze.h).

namespace bfl::FileSystem {

// ---- CAccessItem: the slots of the base class ---------------------------------------
// Read, Write, Seek, GetSize and GetPos are pure in the original's base vtable; a call
// that lands there is a bug in the access chain.
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { std::abort(); }
int32_t CAccessItem::Write(const void*, uint32_t) { std::abort(); }
int32_t CAccessItem::Seek(int32_t, uint32_t) { std::abort(); }
int32_t CAccessItem::GetSize() { std::abort(); }
int32_t CAccessItem::GetPos() { std::abort(); }
void CAccessItem::Close() {}   // 0x1000a958

// 0x1000ac38: `access` goes behind this item; what was there is handed on to it first.
void CAccessItem::AttachAccessClass(CAccessItem* access) {
    if (mNext != nullptr) access->AttachAccessClass(mNext);
    mNext = access;
}

// 0x1000acf8..0x1000ae98: an item that does not answer these passes them down the chain.
const uint8_t* CAccessItem::MapFile(int32_t offset, int32_t length) { return mNext->MapFile(offset, length); } // 0x1000acf8
void CAccessItem::UnmapFile(const uint8_t* data) { mNext->UnmapFile(data); } // 0x1000ad68
_LbSemaphore* CAccessItem::GetSemaphore() { return mNext->GetSemaphore(); } // 0x1000addc
TbError CAccessItem::Flush() { return TbError(-1); } // 0x1000ae50
int32_t CAccessItem::GetMacFileReference() { return mNext->GetMacFileReference(); } // 0x1000ae98

// Pure in the original's base vtable (slots 0xc..0x14, 0x24..0x30 and 0x38).
TbError _TbStorageAccess::OpenFile(CFile&, const char*, uint32_t, void*) { std::abort(); }
TbError _TbStorageAccess::FindFirst(const char*, CFileInfo&) { std::abort(); }
TbError _TbStorageAccess::FindNext(CFileInfo&) { std::abort(); }
TbError _TbStorageAccess::SetAttribs(const char*, uint32_t) { std::abort(); }
TbError _TbStorageAccess::CreateDir(const char*) { std::abort(); }
TbError _TbStorageAccess::Delete(const char*) { std::abort(); }
TbError _TbStorageAccess::Move(const char*, const char*) { std::abort(); }
void _TbStorageAccess::Close() { std::abort(); }

// ---- CFile and CFileStorage: the members that forward ----------------------------------
CFile::CFile(const CFile& other) : mImplementation(other.mImplementation) {   // 0x1000b224
    mImplementation->CAccessItem::AddRef();
}

int32_t CFile::GetMacFileReference() {   // 0x1000b358
    return mImplementation == nullptr ? -1 : mImplementation->GetMacFileReference();
}

CFile& CFile::operator=(const CFile& other) {   // 0x1000b3dc
    if (&other != this) {
        Close();
        AttachAccessClass(other.mImplementation);
    }
    return *this;
}

void* CFile::MapFile() {   // 0x1000b788
    return mImplementation == nullptr ? nullptr : const_cast<uint8_t*>(mImplementation->MapFile(0, 0));
}
TbError CFile::Flush() { return mImplementation == nullptr ? TbError(-1) : mImplementation->Flush(); }   // 0x1000b808
CFileStorage::CFileStorage(const CFileStorage& other) : mAccess(nullptr) {   // 0x1000b8e0
    if (&other != this && other.mAccess != nullptr) AttachAccessClass(other.mAccess);
}

void CFileStorage::Close() {   // 0x1000bb00
    if (mAccess != nullptr) {
        mAccess->Release();
        mAccess = nullptr;
    }
}

TbError CFileStorage::FullPath(char* path, const char* name, uint32_t size) {   // 0x1000bc50
    return mAccess == nullptr ? TbError(-1) : mAccess->FullPath(path, name, size);
}
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>& path, const TbDynamicStringTemplate<char>& name) {   // 0x1000bb80
    return mAccess == nullptr ? TbError(-1) : mAccess->FullPath(path, name);
}
const char* CFileStorage::GetRootPath() { return mAccess == nullptr ? nullptr : mAccess->GetRootPath(); }   // 0x1000bf04
TbError CFileStorage::SetAttribs(const char* name, uint32_t attributes) {   // 0x1000c058
    return mAccess == nullptr ? TbError(-1) : mAccess->SetAttribs(name, attributes);
}
TbError CFileStorage::CreateDir(const char* name) {   // 0x1000c0f4
    return mAccess == nullptr ? TbError(-1) : mAccess->CreateDir(name);
}
TbError CFileStorage::Delete(const char* name) {   // 0x1000c18c
    return mAccess == nullptr ? TbError(-1) : mAccess->Delete(name);
}
TbError CFileStorage::Move(const char* from, const char* to) {   // 0x1000c220
    return mAccess == nullptr ? TbError(-1) : mAccess->Move(from, to);
}

}  // namespace bfl::FileSystem

// Bullfrog LbSCBack.cpp file access and wide storage wrappers, and LbMaths.cpp CMemFile::Open.

namespace bfl::FileSystem {

TbError CDiscFileStorage::Open(const char16_t* path) { // 0x1000b060
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    Close();
    auto* access = new (std::nothrow) _TbFileStorageAccess;
    if (!access->Open(name).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}

TbError CFileStorage::FindFirst(const char16_t* pattern, CUniFileInfo& info) { // 0x1000c2b4
    CFileInfo entry;
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, pattern);
    if (static_cast<int32_t>(error) < 0) return error;
    error = FindFirst(name, entry);
    if (static_cast<int32_t>(error) >= 0) {
        info.mF00 = entry.mF00;
        info.mAttributes = entry.mAttributes;
        std::memcpy(info.mF08, entry.mF08, sizeof info.mF08);
        error = bfl::Strings::Convert(info.mNameBuffer, 0x104, entry.mNameBuffer);
        info.mName = info.mNameBuffer;
        if (static_cast<int32_t>(error) >= 0) info.mName += entry.mName - entry.mNameBuffer;
    }
    return error;
}

TbError CFileStorage::FindNext(CUniFileInfo& info) { // 0x1000c448
    CFileInfo entry;
    TbError error = FindNext(entry);
    if (static_cast<int32_t>(error) >= 0) {
        info.mF00 = entry.mF00;
        info.mAttributes = entry.mAttributes;
        std::memcpy(info.mF08, entry.mF08, sizeof info.mF08);
        error = bfl::Strings::Convert(info.mNameBuffer, 0x104, entry.mNameBuffer);
        info.mName = info.mNameBuffer;
        if (static_cast<int32_t>(error) >= 0) info.mName += entry.mName - entry.mNameBuffer;
    }
    return error;
}

TbError CFileStorage::OpenFile(CFile& file, const char16_t* path, uint32_t mode, void* data) { // 0x1000c5b0
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return OpenFile(file, name, mode, data);
}

TbError CFileStorage::FullPath(char16_t* path, const char16_t* source, uint32_t size) { // 0x1000c69c
    char name[0x104];
    char full[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, source);
    if (static_cast<int32_t>(error) < 0) return error;
    error = FullPath(full, name, size);
    if (static_cast<int32_t>(error) < 0) return error;
    return bfl::Strings::Convert(path, size, full);
}

TbError CFileStorage::FullPath(TbDynamicStringTemplate<char16_t>& path,
                              const TbDynamicStringTemplate<char16_t>& source) { // 0x1000c7a0
    char name[0x104];
    char full[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, source.mData);
    if (static_cast<int32_t>(error) < 0) return error;
    error = FullPath(full, name, 0x104);
    if (static_cast<int32_t>(error) < 0) return error;
    return bfl::Strings::Convert(path, full);
}

TbError CFileStorage::SetAttribs(const char16_t* path, uint32_t attributes) { // 0x1000c8ec
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return SetAttribs(name, attributes);
}

TbError CFileStorage::CreateDir(const char16_t* path) { // 0x1000c9c4
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return CreateDir(name);
}

TbError CFileStorage::Delete(const char16_t* path) { // 0x1000ca88
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    return Delete(name);
}

TbError CFileStorage::Move(const char16_t* from, const char16_t* to) { // 0x1000cb48
    char first[0x104];
    char second[0x104];
    TbError error = bfl::Strings::Convert(first, 0x104, from);
    if (static_cast<int32_t>(error) < 0) return error;
    error = bfl::Strings::Convert(second, 0x104, to);
    if (static_cast<int32_t>(error) < 0) return error;
    return Move(first, second);
}

TbError CWadFileStorage::Open(const char16_t* path, uint32_t flags) { // 0x1000d3b8
    char name[0x104];
    TbError error = bfl::Strings::Convert(name, 0x104, path);
    if (static_cast<int32_t>(error) < 0) return error;
    Close();
    auto* access = new (std::nothrow) _TbWadStorageAccess;
    if (!access->Open(name, flags).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}

}

namespace bfl::FileSystem {

TbError CWadFile::Open(const char* name, const char* storagePath) {
    if (mImplementation != nullptr) return TbError(-1);
    CWadFileStorage wad;
    CDiscFileStorage disc;
    char full[260];
    if (storagePath != nullptr && *storagePath != 0) {
        LbFile_ToProjectFilename(full, 260, storagePath);
        const uint32_t attributes = NS_MacDoze::GetFileAttributes(full);
        if (attributes == 0xffffffff) return TbError(-1);
        CFileStorage* storage;
        if ((attributes & 0x10) != 0) {
            if (!disc.Open(full).Succeeded()) return TbError(-1);
            storage = &disc;
        } else {
            if (!wad.Open(storagePath, 0).Succeeded()) return TbError(-1);
            storage = &wad;
        }
        return storage->OpenFile(*this, name, 0x80000001, nullptr);
    }
    LbFile_ToProjectFilename(full, 260, name);
    TbDynamicStringTemplate<char> path(full);
    int32_t split = path.FindReverse(':', 0xffffffff);
    split = std::max(split, path.FindReverse('/', 0xffffffff));
    split = std::max(split, path.FindReverse('\\', 0xffffffff));
    if (split < 1) return TbError(-1);
    int32_t result;
    {
        TbDynamicStringTemplate<char> directory(path.Left(static_cast<uint32_t>(split)));
        result = disc.Open(directory.mData);
    }
    if (result < 0) return TbError(-1);
    TbDynamicStringTemplate<char> basename(path.Mid(static_cast<uint32_t>(split + 1)));
    return disc.OpenFile(*this, basename.mData, 0x80000001, nullptr);
}

TbError CWadFileStorage::Open(const char* path, uint32_t flags) {
    Close();
    auto* access = new (std::nothrow) _TbWadStorageAccess;
    if (!access->Open(path, flags).Succeeded()) {
        delete access;
        return TbError(-1);
    }
    AttachAccessClass(access);
    return TbError(0);
}
} // namespace bfl::FileSystem

// ---- LbSCBack.cpp: bfl::compress::CRefpack::MemCompress 0x1000d53c

__attribute__((weak)) uint8_t* Refpack_NewArray(uint32_t size) { return new uint8_t[size]; }
__attribute__((weak)) void Refpack_DeleteArray(void* block) { delete[] static_cast<uint8_t*>(block); }

namespace {
// The 256 word table at 0x10071f10: the standard reflected CRC-16 table (polynomial 0xA001), generated here.
struct RefpackTable {
    uint32_t v[256];
    constexpr RefpackTable() : v() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xA001u : c >> 1;
            v[i] = c;
        }
    }
};
constexpr RefpackTable kRefpackTableGen;
const uint32_t* const kRefpackTable = kRefpackTableGen.v;

inline uint32_t RefpackHash(const uint8_t* p) {
    const uint32_t a = kRefpackTable[p[0]];
    const uint32_t b = (a >> 8) ^ kRefpackTable[(a ^ p[1]) & 0xff];
    return (b >> 8) ^ kRefpackTable[(b ^ p[2]) & 0xff];
}

// Writes the pending literals in blocks of up to 0x70 bytes (a multiple of 4); fewer than 4 stay pending.
inline void FlushLiterals(uint8_t*& out, const uint8_t*& lit, uint32_t& n) {
    while (n > 3) {
        uint32_t count = n & 0xfffffffcu;
        if (count > 0x70) count = 0x70;
        *out++ = static_cast<uint8_t>((count >> 2) + 0xdf);
        std::memcpy(out, lit, count);
        n -= count;
        lit += count;
        out += count;
    }
}
}  // namespace

int32_t bfl::compress::CRefpack::MemCompress(uint8_t* dst, uint8_t* src, uint32_t length, uint8_t* work) const {
    uint8_t* out = dst + 5;
    const uint8_t* lit = src;
    uint8_t* pos = src;
    uint8_t* allocated = nullptr;
    uint32_t n = 0;
    uint32_t remaining = length;
    dst[0] = 0x10;
    dst[1] = 0xfb;
    dst[2] = static_cast<uint8_t>(length >> 16);
    dst[3] = static_cast<uint8_t>(length >> 8);
    dst[4] = static_cast<uint8_t>(length);
    if (work == nullptr) {
        const uint32_t size = static_cast<uint32_t>(CompressWorkspaceSize());
        allocated = Refpack_NewArray(size);
        if (allocated == nullptr) return 0;
        work = allocated;
    }
    std::memset(work, 0xff, 0x40000);
    auto head = [&](uint32_t h) -> int32_t& { return *reinterpret_cast<int32_t*>(work + static_cast<size_t>(h) * 4); };
    auto link = [&](int32_t i) -> int32_t& {
        return *reinterpret_cast<int32_t*>(work + 0x40000 + ((static_cast<uint32_t>(i) * 4) & 0x7fffc));
    };
    while (remaining != 0) {
        uint32_t bestOffset = 0, bestType = 2, bestLength = 2;
        const uint32_t maxLength = remaining < 0x404 ? remaining : 0x404;
        const int32_t index = static_cast<int32_t>(pos - src);
        const uint32_t h = RefpackHash(pos);
        int32_t candidate = head(h);
        const int32_t limit = (index - 0x1ffff > 0) ? index - 0x1ffff : 0;
        while (candidate >= limit) {
            const uint8_t* cp = src + candidate;
            if (pos[bestLength] == cp[bestLength]) {
                const uint8_t* a = pos;
                const uint8_t* b = cp;
                uint32_t l = 0;
                while (l < maxLength) {
                    const uint8_t x = *a++;
                    const uint8_t y = *b++;
                    if (x != y) break;
                    ++l;
                }
                if (l > bestLength) {
                    const uint32_t offset = static_cast<uint32_t>((pos - 1) - cp);
                    uint32_t type;
                    if (offset < 0x400 && l <= 10) type = 2;
                    else if (offset < 0x4000 && l <= 0x43) type = 3;
                    else type = 4;
                    if (l - type + 4 > bestLength - bestType + 4) {
                        bestLength = l;
                        bestType = type;
                        bestOffset = offset;
                        if (l >= 0x404) break;
                    }
                }
            }
            candidate = link(candidate);
        }
        if (bestType >= bestLength) {
            // No match worth coding: the byte becomes a pending literal.
            ++n;
            --remaining;
            link(index) = head(h);
            head(h) = index;
            ++pos;
            continue;
        }
        FlushLiterals(out, lit, n);
        if (bestType == 2) {
            *out = static_cast<uint8_t>(((bestOffset >> 3) & 0x1fffffe0u) + (bestLength - 3) * 4 + n);
            out[1] = static_cast<uint8_t>(bestOffset);
            out += 2;
        } else if (bestType == 3) {
            *out = static_cast<uint8_t>(bestLength + 0x7c);
            out[1] = static_cast<uint8_t>((n << 6) + (bestOffset >> 8));
            out[2] = static_cast<uint8_t>(bestOffset);
            out += 3;
        } else {
            const uint32_t extra = bestLength - 5;
            *out = static_cast<uint8_t>(((bestOffset >> 12) & 0xffff0u) + ((extra >> 6) & 0x3fffffcu) + n + 0xc0);
            out[1] = static_cast<uint8_t>(bestOffset >> 8);
            out[2] = static_cast<uint8_t>(bestOffset);
            out[3] = static_cast<uint8_t>(extra);
            out += 4;
        }
        if (n != 0) {
            std::memcpy(out, lit, n);
            out += n;
            n = 0;
        }
        if (static_cast<int32_t>(bestLength) > 0) {
            // Every position the match covers goes into the hash chains.
            for (uint32_t k = 0; k < bestLength; ++k) {
                const int32_t i = static_cast<int32_t>(pos - src);
                const uint32_t hh = RefpackHash(pos);
                link(i) = head(hh);
                head(hh) = i;
                ++pos;
            }
        }
        lit = pos;
        remaining -= bestLength;
    }
    FlushLiterals(out, lit, n);
    *out++ = static_cast<uint8_t>(n + 0xfc);
    if (n != 0) {
        std::memcpy(out, lit, n);
        out += n;
    }
    if (allocated != nullptr) Refpack_DeleteArray(allocated);
    // The original answers the written size plus 5 again (the 5 header bytes are already in `out`): kept.
    return static_cast<int32_t>(out - dst) + 5;
}

namespace bfl::compress {
int32_t CRefpack::MemDecompress(uint8_t* dest, uint8_t* source, uint8_t*) const {
    if (source == nullptr) return 0;
    const uint8_t* input = source + ((source[0] & 1) != 0 ? 5 : 2);
    const int32_t decodedSize = (input[0] << 16) | (input[1] << 8) | input[2];
    input += 3;
    for (;;) {
        const uint32_t command = *input++;
        uint32_t literal, length, distance;
        if ((command & 0x80) == 0) {
            const uint32_t second = *input++;
            literal = command & 3;
            length = ((command >> 2) & 7) + 3;
            distance = ((command & 0x60) << 3) + second + 1;
        } else if ((command & 0x40) == 0) {
            const uint32_t second = *input++;
            const uint32_t third = *input++;
            literal = second >> 6;
            length = (command & 0x3f) + 4;
            distance = ((second & 0x3f) << 8) + third + 1;
        } else if ((command & 0x20) == 0) {
            const uint32_t second = *input++;
            const uint32_t third = *input++;
            const uint32_t fourth = *input++;
            literal = command & 3;
            length = ((command & 0xc) << 6) + fourth + 5;
            distance = ((command & 0x10) << 12) + (second << 8) + third + 1;
        } else {
            literal = ((command & 0x1f) << 2) + 4;
            const bool final = literal > 0x70;
            if (final) literal = command & 3;
            for (uint32_t i = 0; i < literal; ++i) *dest++ = *input++;
            if (final) return decodedSize;
            continue;
        }
        for (uint32_t i = 0; i < literal; ++i) *dest++ = *input++;
        const uint8_t* match = dest - distance;
        for (uint32_t i = 0; i < length; ++i) *dest++ = *match++;
    }
}
int32_t CRefpack::CompressWorkspaceSize() const { return 0xc0000; }
int32_t CRefpack::DecompressWorkspaceSize() const { return 0; }
int32_t CRefpack::DecompressedSize(uint8_t* source) const {
    const uint8_t* length = source + (source[0] == 0x10 && source[1] == 0xfb ? 2 : 5);
    return (length[0] << 16) | (length[1] << 8) | length[2];
}

}

// Bullfrog strings: how the dynamic and static strings size their buffers (bullfrog
// shared: TbDynamicStringTemplate<char> AllocBuffer 0x1000e150 and ResizeTo 0x1000e200, the
// wchar_t instances 0x1000e8b4 and 0x1000e96c, TbStaticStringTemplate<char>::ResizeTo
// 0x10010558). Every constructor, assignment and append of TbString.cpp goes through
// these by the vtable.

// A fresh buffer for `length` characters: the capacity is the next multiple of 8 above the
// length, plus the terminator. The old buffer is the caller's business.
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t length) {
    mGrowBy = 8;
    mLength = static_cast<int32_t>(length);
    mCapacity = static_cast<int32_t>(static_cast<uint32_t>(mGrowBy) *
                                     (static_cast<uint32_t>(mLength) / static_cast<uint32_t>(mGrowBy) + 1));
    mData = new Char[static_cast<uint32_t>(mCapacity) + 1];
    mData[mLength] = 0;
}

// Within the capacity only the length moves. Beyond it a new buffer is allocated and the
// old text copied over (up to its terminator, so what lies between the old text's end and
// the new length is not initialised: the caller writes it).
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t length) {
    if (static_cast<uint32_t>(mCapacity) < length) {
        Char* old = mData;
        AllocBuffer(length);
        this->SBCstrcpy(mData, old);
        delete[] old;
    } else {
        mLength = static_cast<int32_t>(length);
        mData[mLength] = 0;
    }
}

// A static string cannot grow: a length that does not fit is cut to capacity - 1 and the
// truncation remembered.
template<class Char> void TbStaticStringTemplate<Char>::ResizeTo(uint32_t length) {
    if (length < static_cast<uint32_t>(mCapacity)) {
        mLength = static_cast<int32_t>(length);
        mTruncated = 0;
    } else {
        mLength = mCapacity - 1;
        mTruncated = 1;
    }
    mData[mLength] = 0;
}

// The static string's assignments and appends (0x10010658..0x10010a40): size first (which
// may truncate), then copy as much as the new length allows.
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const TbStringBase<Char>& other) {
    ResizeTo(static_cast<uint32_t>(other.Length()));
    this->SBCstrncpy(mData, other.mData, static_cast<uint32_t>(mLength));
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const Char* text) {
    if (text == nullptr) {
        ResizeTo(0);
    } else {
        ResizeTo(this->SBCstrlen(text));
        this->SBCstrncpy(mData, text, static_cast<uint32_t>(mLength));
    }
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const Char& ch) {
    if (ch == 0) {
        ResizeTo(0);
    } else {
        ResizeTo(1);
        if (mTruncated == 0) mData[0] = ch;
    }
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    const int32_t old = mLength;
    ResizeTo(static_cast<uint32_t>(mLength + text.Length()));
    this->SBCstrncat(mData, text.mData, static_cast<uint32_t>(mLength - old));
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const Char& ch) {
    if (ch != 0) {
        ResizeTo(static_cast<uint32_t>(mLength + 1));
        if (mTruncated == 0) mData[mLength - 1] = ch;
    }
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const Char* text) {
    if (text != nullptr) {
        const int32_t old = mLength;
        ResizeTo(static_cast<uint32_t>(mLength) + this->SBCstrlen(text));
        this->SBCstrncat(mData, text, static_cast<uint32_t>(mLength - old));
    }
    return *this;
}

template void TbStaticStringTemplate<char>::ResizeTo(uint32_t);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const TbStringBase<char>&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char*);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const TbStringBase<char>&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char*);

// Bullfrog string functions, 0x1000e2c8..0x10013774. Substrings borrow their data.

template<class Char> uint32_t TbStringBase<Char>::SBCstrlen(const Char* text) const {
    if constexpr (std::is_same_v<Char, char>) return std::strlen(text);
    else return wcslen(text);
}
template<class Char> Char* TbStringBase<Char>::SBCstrcpy(Char* dst, const Char* src) {
    if constexpr (std::is_same_v<Char, char>) return std::strcpy(dst, src);
    else return wcscpy(dst, src);
}
template<class Char> Char* TbStringBase<Char>::SBCstrncpy(Char* dst, const Char* src, uint32_t count) {
    if constexpr (std::is_same_v<Char, char>) return std::strncpy(dst, src, count);
    else return wcsncpy(dst, src, count);
}

// Bullfrog string members, unit w225. Mac wchar_t is a 16-bit code unit.

namespace {
// Host support for the MSL imports that have no char16_t entry point.
int32_t wideCompare(const char16_t* a, const char16_t* b) {
    while (*a != 0 && *a == *b) { ++a; ++b; }
    return int32_t(*a) - int32_t(*b);
}
int32_t wideToInt(const char16_t* p) {
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) ++p;
    bool negative = *p == '-';
    if (*p == '+' || *p == '-') ++p;
    uint32_t value = 0;
    while (*p >= '0' && *p <= '9') value = value * 10 + (*p++ - '0');
    return static_cast<int32_t>(negative ? 0u - value : value);
}

// Parse directives here so 'long' remains 32 bits and wide pointers remain 16 bits.
// Like platform/msl.cpp's swprintf, wide %s widens narrow bytes and %ls reads UTF-16.
// The narrow entry has vsnprintf's would-have-written result; the wide entry returns -1 on truncation.
template<class Char> int32_t macVsnprintf(Char* dst, uint32_t size, const Char* format, va_list source) {
    va_list args;
    va_copy(args, source);
    uint32_t total = 0;
    auto put = [&](Char c) {
        if (size != 0 && total < size - 1) dst[total] = c;
        ++total;
    };
    for (const Char* p = format; *p != 0;) {
        if (*p != '%') { put(*p++); continue; }
        ++p;
        if (*p == '%') { put('%'); ++p; continue; }
        std::string flags;
        while (*p && std::strchr("-+ #0", int(*p))) flags += char(*p++);
        bool left = flags.find('-') != std::string::npos;
        int width = 0;
        if (*p == '*') {
            width = va_arg(args, int);
            ++p;
            if (width < 0) { left = true; flags += '-'; width = -width; }
        } else while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
        int precision = -1;
        if (*p == '.') {
            ++p;
            precision = 0;
            if (*p == '*') { precision = va_arg(args, int); ++p; }
            else while (*p >= '0' && *p <= '9') precision = precision * 10 + (*p++ - '0');
        }
        Char modifier = 0;
        if (*p == 'l' || *p == 'h' || *p == 'N' || *p == 'L' || *p == 'F') modifier = *p++;
        const char conversion = char(*p++);
        if (conversion == 's' || conversion == 'S' || conversion == 'c' || conversion == 'C') {
            const char* narrow = nullptr;
            const char16_t* wide = nullptr;
            uint32_t length;
            uint32_t character = 0;
            if (conversion == 'c' || conversion == 'C') {
                character = va_arg(args, int);
                length = 1;
            } else if (modifier == 'l' || (conversion == 'S' && modifier != 'h')) {
                wide = va_arg(args, const char16_t*);
                if (!wide) wide = u"(null)";
                length = wcslen(wide);
            } else {
                narrow = va_arg(args, const char*);
                if (!narrow) narrow = "(null)";
                length = std::strlen(narrow);
            }
            if (conversion == 's' || conversion == 'S') {
                if (precision >= 0) length = std::min(length, uint32_t(precision));
            }
            uint32_t padding = width > 0 && uint32_t(width) > length ? uint32_t(width) - length : 0;
            if (!left) for (uint32_t i = 0; i < padding; ++i) put(' ');
            for (uint32_t i = 0; i < length; ++i)
                put(Char(wide ? wide[i] : narrow ? uint8_t(narrow[i]) : character));
            if (left) for (uint32_t i = 0; i < padding; ++i) put(' ');
        } else {
            std::string spec = "%" + flags;
            if (width > 0) spec += std::to_string(width);
            if (precision >= 0) spec += "." + std::to_string(precision);
            spec += conversion;
            auto number = [&](auto value) {
                char local[256];
                const int n = std::snprintf(local, sizeof local, spec.c_str(), value);
                char* text = local;
                if (n >= int(sizeof local)) {
                    text = static_cast<char*>(std::malloc(size_t(n) + 1));
                    std::snprintf(text, size_t(n) + 1, spec.c_str(), value);
                }
                for (int i = 0; i < n; ++i) put(Char(uint8_t(text[i])));
                if (text != local) std::free(text);
            };
            if (std::strchr("eEfFgG", conversion)) {
                number(va_arg(args, double));
            } else if (conversion == 'd' || conversion == 'i') {
                int value = va_arg(args, int);
                if (modifier == 'h') value = int16_t(value);
                number(value);
            } else if (std::strchr("ouxX", conversion)) {
                unsigned value = va_arg(args, unsigned);
                if (modifier == 'h') value = uint16_t(value);
                number(value);
            } else if (conversion == 'p') {
                number(va_arg(args, void*));
            } else if (conversion == 'n') {
                if (modifier == 'h') *va_arg(args, int16_t*) = total;
                else *va_arg(args, int32_t*) = total;
            } else {
                put('%');
                put(Char(conversion));
            }
        }
    }
    va_end(args);
    if (size) dst[std::min(total, size - 1)] = 0;
    if constexpr (std::is_same_v<Char, char16_t>) {
        if (size == 0 || total >= size) return -1;
    }
    return static_cast<int32_t>(total);
}
} // namespace

template<class Char> int32_t TbStringBase<Char>::SBCstrcmp(const Char* a, const Char* b) const {
    if constexpr (std::is_same_v<Char, char>) return std::strcmp(a, b);
    else return wideCompare(a, b);
}
template<class Char> int32_t TbStringBase<Char>::SBCstrncmp(const Char* a, const Char* b, uint32_t count) const {
    if constexpr (std::is_same_v<Char, char>) return std::strncmp(a, b, count);
    else return wcsncmp(a, b, count);
}
template<class Char> const Char* TbStringBase<Char>::SBCstrstr(const Char* text, const Char* needle) const {
    if constexpr (std::is_same_v<Char, char>) return std::strstr(text, needle);
    else return wcsstr(text, needle);
}

// w225: wide SBCstrcat 0x100113d4, 44 B.
template<class Char> Char* TbStringBase<Char>::SBCstrcat(Char* dst, const Char* src) {
    if constexpr (std::is_same_v<Char, char>) return std::strcat(dst, src);
    else return wcscat(dst, src);
}
template<class Char> Char* TbStringBase<Char>::SBCstrncat(Char* dst, const Char* src, uint32_t count) {
    if constexpr (std::is_same_v<Char, char>) return std::strncat(dst, src, count);
    else return wcsncat(dst, src, count);
}

template<class Char> int32_t TbStringBase<Char>::SBCstrtoint(Char* text) {
    if constexpr (std::is_same_v<Char, char>) return std::atoi(text);
    else return wideToInt(text);
}
template<class Char> int32_t TbStringBase<Char>::SBCisdigit(Char ch) {
    const uint32_t c = std::is_same_v<Char, char> ? uint8_t(ch) : uint16_t(ch);
    return c >= '0' && c <= '9' ? 0x10 : 0;
}
template<class Char> int32_t TbStringBase<Char>::SBCvsprintf(Char* dst, const Char* fmt, va_list args) {
    return macVsnprintf(dst, 0x7fffffff, fmt, args);
}
template<class Char> int32_t TbStringBase<Char>::SBCvsnprintf(Char* dst, uint32_t count, const Char* fmt, va_list args) {
    return macVsnprintf(dst, count, fmt, args);
}

// w225: wide text equality 0x10011f28, 156 B.
template<class Char> bool TbStringBase<Char>::operator==(const Char* text) const {
    if (text == nullptr) return mLength == 0;
    if (static_cast<uint32_t>(mLength) != SBCstrlen(text)) return false;
    TbStringBase other(const_cast<Char*>(text), mLength);
    return *this == other;
}

template<class Char> bool TbStringBase<Char>::operator==(const Char& ch) const {
    return uint32_t(mLength) < 2 && mData[0] == ch;
}

// w225: wide base equality 0x10011ff4, 76 B.
template<class Char> bool TbStringBase<Char>::operator==(const TbStringBase& other) const {
    if (mLength != other.mLength) return false;
    if constexpr (std::is_same_v<Char, char>) return std::strncmp(mData, other.mData, mLength) == 0;
    else return wcsncmp(mData, other.mData, mLength) == 0;
}
// w225: wide text inequality 0x100120ec, 168 B.
template<class Char> bool TbStringBase<Char>::operator!=(const Char* text) const { return !(*this == text); }

template<class Char> bool TbStringBase<Char>::operator!=(const Char& ch) const { return !(*this == ch); }
template<class Char> bool TbStringBase<Char>::operator!=(const TbStringBase& other) const {
    return mLength != other.mLength || SBCstrncmp(mData, other.mData, mLength) != 0;
}
template<class Char> bool TbStringBase<Char>::operator<(const Char& ch) const {
    if constexpr (std::is_same_v<Char, char>) return int8_t(mData[0]) < int8_t(ch);
    else return mData[0] < ch;
}
template<class Char> bool TbStringBase<Char>::operator>(const Char& ch) const {
    if constexpr (std::is_same_v<Char, char>)
        return uint32_t(mLength) <= 1 ? int8_t(mData[0]) > int8_t(ch) : int8_t(mData[0]) >= int8_t(ch);
    else return uint32_t(mLength) <= 1 ? mData[0] > ch : mData[0] >= ch;
}
template<class Char> bool TbStringBase<Char>::operator<(const Char* text) const {
    if (!text) return false;
    int32_t result = SBCstrncmp(mData, text, mLength);
    return result == 0 ? uint32_t(mLength) < SBCstrlen(text) : result < 0;
}
template<class Char> bool TbStringBase<Char>::operator>(const Char* text) const {
    if (!text) return true;
    int32_t result = SBCstrncmp(mData, text, mLength);
    return result == 0 ? uint32_t(mLength) > SBCstrlen(text) : result > 0;
}
template<class Char> bool TbStringBase<Char>::operator>(const TbStringBase& other) const {
    uint32_t n = std::min(uint32_t(mLength), uint32_t(other.mLength));
    int32_t result = SBCstrncmp(mData, other.mData, n);
    return result == 0 ? uint32_t(mLength) > uint32_t(other.mLength) : result > 0;
}
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char* format, ...) {
    if (!format) { ResizeTo(0); return; }
    va_list args;
    va_start(args, format);
    FormatV(format, args);
    va_end(args);
}
template<class Char> void TbDynamicStringTemplate<Char>::FormatV(const Char* format, va_list args) {
    va_list scan;
    va_copy(scan, args);
    uint32_t total = 0;
    for (const Char* p = format; *p != 0; ++p) {
        if (*p != '%' || *++p == '%') { ++total; continue; }
        uint32_t width = 0;
        uint32_t precision = 0;
        while (*p != 0) {
            if (*p == '#') total += 2;
            else if (*p == '*') width = va_arg(scan, int);
            else if (*p != '-' && *p != '+' && *p != '0' && *p != ' ') break;
            ++p;
        }
        if (width == 0) {
            width = this->SBCstrtoint(const_cast<Char*>(p));
            while (*p && this->SBCisdigit(*p)) ++p;
        }
        if (*p == '.') {
            ++p;
            if (*p == '*') { precision = va_arg(scan, int); ++p; }
            else {
                precision = this->SBCstrtoint(const_cast<Char*>(p));
                while (*p && this->SBCisdigit(*p)) ++p;
            }
        }
        uint32_t flag = 0;
        if (*p == 'h') { flag = 0x10000; ++p; }
        else if (*p == 'l') { flag = 0x20000; ++p; }
        else if (*p == 'N' || *p == 'L' || *p == 'F') ++p;
        const int32_t ch = std::is_same_v<Char, char> ? int8_t(*p) : uint16_t(*p);
        const uint32_t key = ch | flag;
        uint32_t length = 0;
        if (key == ('c' | flag) || key == ('C' | flag)) {
            (void)va_arg(scan, int);
            length = 2;
        } else if (key == ('s' | flag) || key == ('S' | flag)) {
            const void* value = va_arg(scan, const void*);
            if (!value) length = 6;
            else {
                if (flag == 0x10000) length = std::strlen(static_cast<const char*>(value));
                else if (flag == 0x20000) length = wcslen(static_cast<const char16_t*>(value));
                else length = this->SBCstrlen(static_cast<const Char*>(value));
                // All six image words 0x1007234c..0x10072360 are 1.
                length = std::max(1u, length);
            }
        }
        if (length != 0) {
            length = std::max(length, width);
            if (precision != 0) length = std::min(length, precision);
        } else {
            switch (ch) {
            case 'G': case 'e': case 'f': case 'g':
                (void)va_arg(scan, double);
                length = std::max(0x80u, width + precision);
                break;
            case 'X': case 'd': case 'i': case 'o': case 'u': case 'x':
                (void)va_arg(scan, int);
                length = std::max(0x20u, width + precision);
                break;
            case 'p':
                (void)va_arg(scan, const void*);
                length = std::max(0x20u, width + precision);
                break;
            case 'n': (void)va_arg(scan, void*); break;
            }
        }
        total += length;
    }
    va_end(scan);
    Char* buffer = new Char[total + 1];
    int32_t length = this->SBCvsnprintf(buffer, total + 1, format, args);
    buffer[length] = 0;
    ResizeTo(length);
    this->SBCstrcpy(mData, buffer);
    delete[] buffer;
    mData[length] = 0;
    mLength = length;
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char& ch) {
    AllocBuffer(1);
    mData[0] = ch;
}
template<class Char> void TbStaticStringTemplate<Char>::Format(const Char* format, ...) {
    if (!format) { ResizeTo(0); return; }
    va_list args;
    va_start(args, format);
    mLength = this->SBCvsnprintf(mData, mCapacity, format, args);
    va_end(args);
}

template<class Char> int32_t TbStringBase<Char>::Length() const { return mLength; }

template<class Char> int32_t TbStringBase<Char>::FindReverse(const Char* text, uint32_t start) const {
    if (!text || mLength == 0) return -1;
    uint32_t n = SBCstrlen(text);
    if (start == 0xffffffff || start > uint32_t(mLength)) start = uint32_t(mLength) - n;
    if (n >= uint32_t(mLength) || start > uint32_t(mLength)) return -1;
    do {
        if (mData[start] == text[0] && SBCstrncmp(mData + start, text, n) == 0) return start;
    } while (start-- != 0);
    return -1;
}
template void TbStaticStringTemplate<char>::Format(const char*, ...);

// w225: wide Mid 0x1001263c, 84 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Mid(uint32_t start) const {
    start = std::min(start, static_cast<uint32_t>(mLength));
    return TbStringBase(mData + start, static_cast<uint32_t>(mLength) - start);
}
// w225: wide Mid 0x100126c0, 124 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Mid(uint32_t start, uint32_t count) const {
    start = std::min(start, static_cast<uint32_t>(mLength));
    return TbStringBase(mData + start, std::min(count, static_cast<uint32_t>(mLength) - start));
}
// w225: wide Right 0x1001276c, 72 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Right(uint32_t count) const {
    count = std::min(count, static_cast<uint32_t>(mLength));
    return TbStringBase(mData + static_cast<uint32_t>(mLength) - count, count);
}
// w225: wide Left 0x100127e4, 60 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Left(uint32_t count) const {
    return TbStringBase(mData, std::min(count, static_cast<uint32_t>(mLength)));
}
// w225: wide base comparison 0x10012d48, 164 B.
template<class Char> bool TbStringBase<Char>::operator<(const TbStringBase& other) const {
    const uint32_t n = std::min(static_cast<uint32_t>(mLength), static_cast<uint32_t>(other.mLength));
    int32_t result;
    if constexpr (std::is_same_v<Char, char>) result = std::strncmp(mData, other.mData, n);
    else result = wcsncmp(mData, other.mData, n);
    return result == 0 ? static_cast<uint32_t>(mLength) < static_cast<uint32_t>(other.mLength) : result < 0;
}
template<class Char> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() { delete[] mData; }
// w225: wide base assignment 0x1000ead4, 156 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>& other) {
    if (static_cast<const TbStringBase<Char>*>(this) != &other) {
        if (static_cast<uint32_t>(mLength) < static_cast<uint32_t>(other.mLength)) ResizeTo(other.mLength);
        this->SBCstrncpy(mData, other.mData, other.mLength);
        ResizeTo(other.mLength);
    }
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char* text) {
    if (text == nullptr) {
        mLength = 0;
        mData[0] = 0;
    } else {
        ResizeTo(this->SBCstrlen(text));
        this->SBCstrcpy(mData, text);
    }
    return *this;
}
// w225: wide character assignment 0x1000ec88, 132 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char& ch) {
    if constexpr (std::is_same_v<Char, char16_t>) {
        // The wide listing branches before ResizeTo, then reloads the aliased unit.
        if (ch == 0) ResizeTo(0);
        else {
            ResizeTo(1);
            mData[0] = ch;
        }
    } else {
        ResizeTo(ch == 0 ? 0 : 1);
        if (ch != 0) mData[0] = ch;
    }
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    ResizeTo(static_cast<uint32_t>(mLength) + text.Length());
    this->SBCstrncat(mData, text.mData, text.Length());
    return *this;
}
// w225: wide character append 0x1000ee18, 120 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char& ch) {
    if (ch != 0) {
        ResizeTo(static_cast<uint32_t>(mLength) + 1);
        mData[mLength - 1] = ch;
    }
    return *this;
}
// w225: wide text append 0x1000eecc, 124 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char* text) {
    if (text != nullptr) {
        ResizeTo(static_cast<uint32_t>(mLength) + this->SBCstrlen(text));
        this->SBCstrcat(mData, text);
    }
    return *this;
}
// w225: wide dynamic assignment 0x1000f614, 84 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate& other) {
    TbStringBase<Char> view(other);
    return operator=(view);
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() { AllocBuffer(0); }
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char* text) {
    AllocBuffer(text == nullptr ? 0 : this->SBCstrlen(text));
    if (text != nullptr) this->SBCstrcpy(mData, text);
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other)
    : TbDynamicStringTemplate(static_cast<const TbStringBase<Char>&>(other)) {}
// w225: wide base constructor 0x1000f7a8, 152 B.
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbStringBase<Char>& other) {
    AllocBuffer(other.mLength);
    this->SBCstrncpy(mData, other.mData, other.mLength);
    mData[other.mLength] = 0;
}
template<class Char> TbStaticStringTemplate<Char>::~TbStaticStringTemplate() {}

template<class Char> TbStaticStringTemplate<Char>::TbStaticStringTemplate(Char* buffer, uint32_t capacity, int32_t clear) {
    mCapacity = capacity;
    mData = buffer;
    ResizeTo(clear == 0 ? this->SBCstrlen(buffer) : 0);
}
template<class Char> Char* TbStringBase<Char>::LockBuffer(uint32_t length) {
    ResizeTo(length);
    return mData;
}
// w225: wide character FindReverse 0x10012a0c, 152 B.
template<class Char> int32_t TbStringBase<Char>::FindReverse(const Char& ch, uint32_t start) const {
    if (ch == 0 || mLength == 0) return -1;
    uint32_t i = std::min(start, static_cast<uint32_t>(mLength) - 1);
    do {
        if (mData[i] == ch) return i;
    } while (i-- != 0);
    return -1;
}
// w225: wide FindReverseOneOf 0x10012850, 168 B.
template<class Char> int32_t TbStringBase<Char>::FindReverseOneOf(const Char* set, uint32_t start) const {
    if (set == nullptr || mLength == 0) return -1;
    uint32_t i = std::min(start, static_cast<uint32_t>(mLength) - 1);
    do {
        for (const Char* p = set; *p != 0; ++p)
            if (mData[i] == *p) return i;
    } while (i-- != 0);
    return -1;
}
// w225: wide FindOneOf 0x10012938, 156 B.
template<class Char> int32_t TbStringBase<Char>::FindOneOf(const Char* set, uint32_t start) const {
    if (set == nullptr) return -1;
    for (uint32_t i = std::min(start, static_cast<uint32_t>(mLength)); i < static_cast<uint32_t>(mLength); ++i)
        for (const Char* p = set; *p != 0; ++p)
            if (mData[i] == *p) return i;
    return -1;
}
// w225: wide character Find 0x10012adc, 136 B.
template<class Char> int32_t TbStringBase<Char>::Find(const Char& ch, uint32_t start) const {
    if (ch == 0) return -1;
    for (uint32_t i = std::min(start, static_cast<uint32_t>(mLength)); i < static_cast<uint32_t>(mLength); ++i)
        if (mData[i] == ch) return i;
    return -1;
}
// w225: wide text Find 0x10012b98, 152 B.
template<class Char> int32_t TbStringBase<Char>::Find(const Char* text, uint32_t start) const {
    if (text == nullptr) return -1;
    start = std::min(start, static_cast<uint32_t>(mLength));
    const Char* result;
    if constexpr (std::is_same_v<Char, char>) result = std::strstr(mData + start, text);
    else result = wcsstr(mData + start, text);
    return result == nullptr ? -1 : static_cast<int32_t>(result - mData);
}
template<class Char> void TbStringBase<Char>::ReleaseBuffer(uint32_t length) {
    if (length == 0x7fffffff) {
        ResizeTo(std::min(SBCstrlen(mData), static_cast<uint32_t>(mCapacity)));
    } else {
        ResizeTo(length);
        mData[length] = 0;
    }
}
template class TbStringBase<char>;
template class TbStringBase<char16_t>;
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;
template TbStaticStringTemplate<char>::TbStaticStringTemplate(char*, uint32_t, int32_t);
template TbStaticStringTemplate<char>::~TbStaticStringTemplate();

// TbStringMatch (bullfrog library): a wildcard mask kept as a list of parts. A part is a run
// of plain text, or a '*' or '?' followed by the plain text after it; a run of wildcards
// after a '*' is one '*'. MatchString walks the parts: '?' takes exactly one character, '*'
// goes to the first place its text occurs (it never tries a later one), and a last part of
// plain text or '?' must end the string, while anything may follow a last '*' part.

TbError TbStringMatch::SetMask(const char* mask) {
    while (mCount != 0) {
        TbDynamicStringTemplate<char> removed = RemoveHead();
    }
    TbDynamicStringTemplate<char> source(mask);
    TbDynamicStringTemplate<char> part;
    const char* cursor = source.mData;
    while (*cursor != '*' && *cursor != '?' && *cursor != 0) ++cursor;
    if (cursor > source.mData) {
        part = source.Left(static_cast<uint32_t>(cursor - source.mData));
        AddTail(part);
    }
    if (*cursor != 0) {
        part = *cursor++;
        while (*cursor != 0) {
            if (*cursor == '*' || *cursor == '?') {
                AddTail(part);
                part = *cursor;
                if (*cursor == '*') {
                    while (cursor[1] == '?' || cursor[1] == '*') ++cursor;
                }
                ++cursor;
            } else part += *cursor++;
        }
        AddTail(part);
    }
    return TbError(0);
}

bool TbStringMatch::MatchString(const char* text) { return MatchSubString(text, mHead); }
bool TbStringMatch::MatchSubString(const char* text, void* position) {
    if (position == nullptr) return true;
    auto* node = static_cast<CNode*>(position);
    const char* pattern = node->mData.mData;
    if (*pattern == '?') {
        if (*text == 0) return false;
        if (std::strncmp(text + 1, pattern + 1, node->mData.Length() - 1) != 0) return false;
    } else if (*pattern == '*') {
        const char* found = std::strstr(text, pattern + 1);
        if (found == nullptr) return false;
        return MatchSubString(found + node->mData.Length() - 1, node->mNext);
    } else if (std::strncmp(text, pattern, node->mData.Length()) != 0) return false;
    const int32_t length = node->mData.Length();
    return node->mNext == nullptr ? text[length] == 0 : MatchSubString(text + length, node->mNext);
}

template<> TbDynamicStringTemplate<char>
TbTList<TbDynamicStringTemplate<char>, TbDynamicStringTemplate<char>&>::RemoveHead() {
    if (mHead == nullptr) return TbDynamicStringTemplate<char>();
    CNode* node = mHead;
    mHead = node->mNext;
    if (mHead == nullptr) mTail = nullptr;
    else mHead->mPrev = nullptr;
    TbDynamicStringTemplate<char> temporary(node->mData);
    --mCount;
    delete node;
    TbDynamicStringTemplate<char> result(temporary);
    return result;
}

// w225: char-string list destructor 0x1000a074, 100 B; RemoveAll 0x1000a138, 132 B.
// The generic bodies in TbTList.h preserve tail/head clearing and next-before-delete order.
template class TbTList<TbDynamicStringTemplate<char>, TbDynamicStringTemplate<char>&>;

// 0x10014738
CritLock* CritLock_DtorBody(CritLock* self, int16_t flag) {
    if (self != nullptr) {
        if (self->mSection != nullptr) NS_MacDoze::LeaveCriticalSection(self->mSection);
        self->mSection = nullptr;
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}
