// Unused import slots needed by the older main-app unit libraries.
#pragma once
#include "bfl_FileSystem.h"
#ifndef TPW_ACCESS_ONLY
#ifndef TPW_REAL_CFILE_MEMBERS
int32_t bfl::FileSystem::CFile::GetPos() { return 0; }
#endif
void* bfl::FileSystem::CFile::MapFile() { return nullptr; }
TbError bfl::FileSystem::CFile::Flush() { return TbError(0); }
int32_t bfl::FileSystem::CFile::GetMacFileReference() { return -1; }
#endif
#ifndef TPW_REAL_ACCESS_CTORS
bfl::FileSystem::CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
#endif
#ifndef TPW_RECORDED_ACCESS_IO
bfl::FileSystem::CAccessItem::~CAccessItem() {}
int32_t bfl::FileSystem::CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t bfl::FileSystem::CAccessItem::Write(const void*, uint32_t) { return -1; }
#endif
int32_t bfl::FileSystem::CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t bfl::FileSystem::CAccessItem::GetSize() { return -1; }
int32_t bfl::FileSystem::CAccessItem::GetPos() { return -1; }
const uint8_t* bfl::FileSystem::CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void bfl::FileSystem::CAccessItem::UnmapFile(const uint8_t*) {}
TbError bfl::FileSystem::CAccessItem::Flush() { return TbError(-1); }
int32_t bfl::FileSystem::CAccessItem::GetMacFileReference() { return -1; }
void bfl::FileSystem::CAccessItem::Close() {}
void bfl::FileSystem::CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* bfl::FileSystem::CAccessItem::GetSemaphore() { return nullptr; }
