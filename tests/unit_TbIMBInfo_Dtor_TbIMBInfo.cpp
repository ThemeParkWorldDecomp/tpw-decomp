// Test library for the ltms destructors, CCharacterHandler and CFile::Read (unit v51128; tests/test_TbIMBInfo_Dtor_TbIMBInfo.py).
// Fakes: TbResource's virtuals (Release records), CStaticMBBuffer::Close and the file access object's Read record events.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbResource.h"
#include "CStaticBuffer.h"
#include "TbITable.h"
#include "CCharacterHandler.h"
#include "bfl_FileSystem.h"

using namespace calllog;

int32_t TbResource::Release() {
    ev("Release", {static_cast<int64_t>(lab(this))});
    return 0;
}
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}

void CStaticMBBuffer::Close() { ev("Close", {static_cast<int64_t>(lab(this))}); }
void CStaticMBBuffer::AppendMB(const unsigned char*, uint32_t) {}
void CStaticMBBuffer::AppendMB(uint16_t) {}
void CStaticMBBuffer::AppendUnicode(const char16_t*, uint32_t) {}
void CStaticMBBuffer::AppendUnicode(char16_t) {}

namespace bfl {
namespace FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace FileSystem
}  // namespace bfl

namespace {

struct TestFont : TbIFont {
    uint32_t GetCharIndex(const unsigned char*&) override { return 0; }
    uint16_t GetCharCount() override { return 0; }
    TbFontCharSize GetCharSize(uint16_t) override { return {0, 0}; }
    uint8_t GetVerticalSpacing() override { return 0; }
    uint32_t GetCaps() override { return 0; }
    uint32_t GetType() override { return 0; }
    TbIMBToUnicode* GetMBToUnicode() override { return nullptr; }
};

struct TestBuffer : CStaticBuffer {
    void Close() override {}
    void AppendMB(const unsigned char*, uint32_t) override {}
    void AppendMB(uint16_t) override {}
    void AppendUnicode(const char16_t*, uint32_t) override {}
    void AppendUnicode(char16_t) override {}
};

// Same vtable shape as bfl's CAccessItem for the slots used: destructor (two slots), then Read.
struct FakeAccess {
    virtual ~FakeAccess() {}
    virtual int32_t Read(void* dest, uint32_t size) {
        ev("Read", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(dest)), static_cast<int64_t>(size)});
        return static_cast<int32_t>(ret("Read"));
    }
};

}  // namespace

extern "C" {

uint64_t tpw_ch_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// kind: 0 TbIMBInfo, 1 TbIFont, 2 TbResource, 3 CStaticMBBuffer, 4 CStaticBuffer, 5 CCharacterHandler.
// The destructor runs as the compiler's non-deleting form; the delete storage flag (a short) decides the delete event.
uint64_t tpw_ch_dtor(uint32_t kind, uint64_t objAddr, int32_t flag, uint64_t convAddr) {
    void* obj = reinterpret_cast<void*>(static_cast<uintptr_t>(objAddr));
    if (obj == nullptr) return 0;
    switch (kind) {
        case 0: { auto* p = new (obj) TbIMBInfo(); p->TbIMBInfo::~TbIMBInfo(); break; }
        case 1: { auto* p = new (obj) TestFont(); p->~TestFont(); break; }
        case 2: { auto* p = new (obj) TbResource(); p->TbResource::~TbResource(); break; }
        case 3: {
            auto* p = new (obj) CStaticMBBuffer;
            p->mConverter = nullptr;
            if (convAddr != 0)
                p->mConverter = reinterpret_cast<TbIUnicodeToMB*>(
                    new (reinterpret_cast<void*>(static_cast<uintptr_t>(convAddr))) TbResource());
            p->CStaticMBBuffer::~CStaticMBBuffer();
            break;
        }
        case 4: { auto* p = new (obj) TestBuffer(); p->~TestBuffer(); break; }
        default: { auto* p = new (obj) CCharacterHandler(); p->CCharacterHandler::~CCharacterHandler(); break; }
    }
    if (static_cast<int16_t>(flag) > 0) ev("delete", {static_cast<int64_t>(lab(obj))});
    return objAddr;
}

void tpw_ch_ctor(uint64_t objAddr) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(objAddr))) CCharacterHandler(); }

int32_t tpw_ch_read(uint64_t fileAddr, uint64_t implAddr, uint64_t dest, uint32_t size) {
    auto* file = new (reinterpret_cast<void*>(static_cast<uintptr_t>(fileAddr))) bfl::FileSystem::CFile();
    if (implAddr != 0)
        file->mImplementation = reinterpret_cast<bfl::FileSystem::CAccessItem*>(
            new (reinterpret_cast<void*>(static_cast<uintptr_t>(implAddr))) FakeAccess());
    return file->Read(reinterpret_cast<void*>(static_cast<uintptr_t>(dest)), size);
}

}  // extern "C"
