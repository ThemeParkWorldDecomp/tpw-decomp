// Test library for TbIntegerVTag::Format (unit v5280; libtpw_unit_TbIntegerVTag_Format.dylib), main app:
//   src/TbUniStringVTag_TbIntegerVTag_Format.cpp
//
// swprintf is a recording fake formatting "%d"; the buffer's AppendUnicode records its arguments.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "TbVTag.h"

using namespace calllog;

namespace {

class FakeBuffer : public CStaticBuffer {
public:
    void Close() override {}
    void AppendMB(const unsigned char*, uint32_t) override {}
    void AppendMB(uint16_t) override {}
    void AppendUnicode(const char16_t* text, uint32_t n) override {
        ev("AppendUnicode", {static_cast<int64_t>(n)});
        for (const char16_t* p = text; *p; p++) ev("Char", {static_cast<int64_t>(*p)});
    }
    void AppendUnicode(char16_t) override {}
};

}  // namespace

int swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t value = va_arg(ap, int32_t);
    va_end(ap);
    std::string s = std::to_string(value);
    size_t n = s.size() < size - 1 ? s.size() : size - 1;
    for (size_t i = 0; i < n; i++) buffer[i] = static_cast<char16_t>(s[i]);
    buffer[n] = 0;
    ev("swprintf", {static_cast<int64_t>(size), value});
    return static_cast<int>(n);
}

extern "C" {

uint64_t tpw_tivf_addr(const char*) { return 0; }

uint64_t tpw_tivf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tivf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbIntegerVTag, mValue));
    return n;
}

uint32_t tpw_tivf_format(int32_t value) {
    TbIntegerVTag tag(5, value);
    FakeBuffer buf;
    TbVarFormat fmt{};
    return static_cast<uint32_t>(tag.Format(buf, fmt));
}

}  // extern "C"
