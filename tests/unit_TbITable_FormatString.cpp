// Test library for TbITable::FormatString, multibyte overload (unit v5032; libtpw_unit_TbITable_FormatString.dylib), main app:
//   src/TbITable_TbITable_FormatString.cpp
//
// Fakes: TbITable::DoFormat, CStaticMBBuffer::Close (and the unused appends), and a converter whose AddRef/Release record.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbITable.h"

using namespace calllog;

namespace {

constexpr uint64_t kMask = 0xffffffffu;

// A converter: vtable order Release, AddRef like TbResource.
struct FakeConv {
    virtual int32_t Release() {
        ev("Release", {lab(this)});
        return 0;
    }
    virtual void AddRef() { ev("AddRef", {lab(this)}); }
    virtual ~FakeConv() {}
};

}  // namespace

void CStaticMBBuffer::Close() {
    ev("Close", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(mCursor) & kMask),
                 static_cast<int64_t>(reinterpret_cast<uintptr_t>(mEnd) & kMask), lab(mConverter), mOverflow ? 1 : 0});
}
// The destructor as translated in src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp (unit v51128): Close, then the Release if still held.
CStaticMBBuffer::~CStaticMBBuffer() {
    Close();
    if (mConverter != nullptr) mConverter->Release();
}
void CStaticMBBuffer::AppendMB(const unsigned char*, uint32_t) {}
void CStaticMBBuffer::AppendMB(uint16_t) {}
void CStaticMBBuffer::AppendUnicode(const char16_t*, uint32_t) {}
void CStaticMBBuffer::AppendUnicode(char16_t) {}

int32_t TbITable::DoFormat(CStaticBuffer& out, uint32_t id, char* arguments) const {
    auto& b = static_cast<CStaticMBBuffer&>(out);
    const uint64_t* a = reinterpret_cast<const uint64_t*>(arguments);
    ev("DoFormat", {lab(const_cast<TbITable*>(this)), static_cast<int64_t>(reinterpret_cast<uintptr_t>(b.mCursor) & kMask),
                    static_cast<int64_t>(reinterpret_cast<uintptr_t>(b.mEnd) & kMask), lab(b.mConverter), b.mOverflow ? 1 : 0,
                    static_cast<int64_t>(id), static_cast<int64_t>(a[0] & kMask), static_cast<int64_t>(a[1] & kMask),
                    static_cast<int64_t>(a[2] & kMask), static_cast<int64_t>(a[3] & kMask)});
    return static_cast<int32_t>(ret("DoFormat"));
}

extern "C" {

uint64_t tpw_ft_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ft_newconv() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new FakeConv())); }

int32_t tpw_ft_layout(int64_t*) { return 0; }

int32_t tpw_ft_format(uint64_t table, uint64_t conv, uint32_t out, uint32_t size, uint32_t id, uint32_t a0, uint32_t a1, uint32_t a2,
                      uint32_t a3) {
    auto* t = reinterpret_cast<TbITable*>(static_cast<uintptr_t>(table));
    t->mUnicodeToMB = reinterpret_cast<TbIUnicodeToMB*>(static_cast<uintptr_t>(conv));
    return t->FormatString(reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(out)), size, id, static_cast<uint64_t>(a0),
                           static_cast<uint64_t>(a1), static_cast<uint64_t>(a2), static_cast<uint64_t>(a3));
}

}  // extern "C"
