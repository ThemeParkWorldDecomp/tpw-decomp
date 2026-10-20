// Bullfrog file calls are queued and recorded on both sides of ltms tests.
#pragma once
#include "bfl_FileSystem.h"
#include "calllog.h"
#include <cstring>
using namespace bfl::FileSystem;
using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

namespace {
const uint8_t* fileBytes;
uint32_t fileLength;
uint32_t filePosition;
// The access object behind every fake file: reads come from fileBytes, with queued results.
class FileImpl : public CAccessItem {
public:
    int32_t Read(void* out, uint32_t size) override {
        ev("Read", {size});
        const auto n = static_cast<int32_t>(ret("Read", size));
        if (n > 0) std::memcpy(out, fileBytes + filePosition, static_cast<uint32_t>(n));
        filePosition += n > 0 ? n : 0;
        return n;
    }
    int32_t Write(const void*, uint32_t) override { return -1; }
} fileImpl;
}
// The base access class's slots are pure on the Mac; nothing here reaches them.
CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(0); }
int32_t CAccessItem::GetMacFileReference() { return 0; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }
CFile::CFile() : mImplementation(nullptr) { ev("File", {newlab(this)}); }
CFile::~CFile() { ev("~File", {lab(this)}); }
void CFile::Close() { ev("FileClose", {lab(this)}); }
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() {
    ev("GetSize", {lab(this)});
    return static_cast<int32_t>(ret("GetSize", fileLength));
}
int32_t CFile::GetPos() {
    ev("GetPos", {lab(this)});
    return static_cast<int32_t>(ret("GetPos", filePosition));
}
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
CFileStorage::CFileStorage() : mAccess(nullptr) { ev("Storage", {newlab(this)}); }
CFileStorage::~CFileStorage() { ev("~Storage", {lab(this)}); }
void CFileStorage::Close() { ev("StorageClose", {lab(this)}); }
TbError CDiscFileStorage::Open(const char* path) {
    ev("StorageOpen", {lab(this)}, {path});
    return TbError(static_cast<int32_t>(ret("StorageOpen")));
}
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    ev("OpenFile", {lab(this), lab(&file), mode, lab(data)}, {name});
    if (ret("Implementation", 1)) file.mImplementation = &fileImpl;
    filePosition = 0;
    return TbError(static_cast<int32_t>(ret("OpenFile")));
}
TbError CFileStorage::FullPath(char* path, const char* name, uint32_t size) {
    ev("FullPath", {lab(this), size}, {name});
    std::strcpy(path, "root:language:");
    return TbError(static_cast<int32_t>(ret("FullPath")));
}
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(0); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(0); }
const char* CFileStorage::GetRootPath() { return ""; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }
