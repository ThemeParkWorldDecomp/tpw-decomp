// Test library for CStaticMBBuffer AppendMB x2, AppendUnicode x2, Close (unit v5312; libtpw_unit_CStaticMBBuffer_AppendMB.dylib), main app:
//   src/TbITable_TbITable_FormatString.cpp
//
// The virtual calls go through fake vtables in raw memory (Itanium slots: 0 Release for the converter; 4 AppendMB(uint16_t) for the buffer), each
// recording an event like the PowerPC glue stub. TbIUnicodeToMB::LocateValue (behind the inline GetMBValue) is a recording fake.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbITable.h"
#include "TbResource.h"

using namespace calllog;

uint16_t TbIUnicodeToMB::LocateValue(char16_t value) {
    ev("GetMBValue", {lab(this), static_cast<int64_t>(value)});
    return static_cast<uint16_t>(ret("GetMBValue"));
}

namespace {

int32_t FakeRelease(void* self) {
    ev("Release", {lab(self)});
    return 0;
}

void FakeAppendMB16(void* self, uint16_t value) { ev("AppendMB16", {lab(self), static_cast<int64_t>(value)}); }

void FakeOther(void* self) { ev("UNEXPECTED", {lab(self)}); }

void* gBufferVtable[8] = {nullptr, nullptr, (void*)FakeOther, (void*)FakeOther, (void*)FakeAppendMB16, (void*)FakeOther, (void*)FakeOther, (void*)FakeOther};
void* gConverterVtable[8] = {(void*)FakeRelease, (void*)FakeOther, (void*)FakeOther, (void*)FakeOther, nullptr, nullptr, nullptr, nullptr};

CStaticMBBuffer* Buf(uint64_t p) { return reinterpret_cast<CStaticMBBuffer*>(static_cast<uintptr_t>(p)); }

}  // namespace

extern "C" {

uint64_t tpw_cmb_addr(const char* name) {
    if (std::strcmp(name, "bufvt") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gBufferVtable[0]));
    if (std::strcmp(name, "convvt") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gConverterVtable[0]));
    return 0;
}

uint64_t tpw_cmb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cmb_layout(int64_t* out) {
    alignas(16) char mem[64] = {};
    CStaticMBBuffer* o = reinterpret_cast<CStaticMBBuffer*>(mem);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&o->mCursor) - mem;
    out[n++] = reinterpret_cast<char*>(&o->mEnd) - mem;
    out[n++] = reinterpret_cast<char*>(&o->mConverter) - mem;
    out[n++] = reinterpret_cast<char*>(&o->mOverflow) - mem;
    return n;
}

void tpw_cmb_appendmb_p(uint64_t obj, uint64_t data, uint32_t size) {
    Buf(obj)->CStaticMBBuffer::AppendMB(reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(data)), size);
}
void tpw_cmb_appendmb_c(uint64_t obj, uint32_t v) { Buf(obj)->CStaticMBBuffer::AppendMB(static_cast<uint16_t>(v)); }
void tpw_cmb_appendu_p(uint64_t obj, uint64_t data, uint32_t size) {
    Buf(obj)->CStaticMBBuffer::AppendUnicode(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(data)), size);
}
void tpw_cmb_appendu_c(uint64_t obj, uint32_t v) { Buf(obj)->CStaticMBBuffer::AppendUnicode(static_cast<char16_t>(v)); }
void tpw_cmb_close(uint64_t obj) { Buf(obj)->CStaticMBBuffer::Close(); }

}  // extern "C"
