// Test library for CBankContainer::~CBankContainer (unit v5573; libtpw_unit_CBankContainer_Dtor_CBankContainer.dylib), main app:
//   src/sound/CSprocketsModule.cpp
//
// The member destructors (CFileStorage, TbDynamicStringTemplate<char>) and the base TbRefRelease destructor are recording fakes.
// Each records whether `this` is the expected subobject of the object under test. The deleting tail is done by the wrapper.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbMapStreamer.h"

using namespace calllog;

namespace {
CBankContainer* gObj = nullptr;
}

// ---- fakes
TbRefRelease::TbRefRelease() {}
TbRefRelease::~TbRefRelease() { ev("BaseDtor", {static_cast<const void*>(this) == static_cast<const void*>(gObj) ? 1 : 0}); }
int32_t TbRefRelease::Release() { return 0; }
int32_t TbRefRelease::AddRef() { return 0; }

namespace bfl {
namespace FileSystem {
CFileStorage::~CFileStorage() { ev("StorageDtor", {this == &gObj->mStorage ? 1 : 0}); }
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(0); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(0); }
TbError CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(0); }
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
const char* CFileStorage::GetRootPath() { return nullptr; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }
}  // namespace FileSystem
}  // namespace bfl

template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { ev("StringDtor", {this == &gObj->mName ? 1 : 0}); }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

extern "C" {

uint64_t tpw_cbd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_cbd_size() { return static_cast<uint32_t>(sizeof(CBankContainer)); }

// The delete flag is the hidden short after this; the wrapper does what the compiler's tail does. A NULL this does nothing.
void tpw_cbd_dtor(uint64_t object, int32_t flag) {
    CBankContainer* p = reinterpret_cast<CBankContainer*>(static_cast<uintptr_t>(object));
    if (p == nullptr) return;
    gObj = p;
    p->CBankContainer::~CBankContainer();   // qualified: the zeroed object has no vptr
    if (static_cast<int16_t>(flag) > 0) ev("delete", {1});
}

}  // extern "C"
