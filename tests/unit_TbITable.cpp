#include "TbITable.h"
#include "calllog.h"
#include <cstring>
#include <string>
#include <array>
using calllog::ev;
using calllog::ret;

namespace {
TbIMBToUnicode converter;
TbITable table;
char16_t* start;
class Tag : public TbVTag {
public:
    int32_t Format(CStaticBuffer&, TbVarFormat& p) override {
        ev("Variable", {mId, p.mWidth, p.mPrecision, p.mFlags},
           {p.mFormat ? reinterpret_cast<const char*>(p.mFormat) : ""});
        p = {nullptr, 123, 456, 789};
        return static_cast<int32_t>(ret("Variable"));
    }
};
std::array<Tag, 8> tags;
std::array<TbVTag*, 9> arguments;
std::string Hex(const unsigned char* data, uint32_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (uint32_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}
void Write(CStaticUnicodeBuffer* p, char16_t value) {
    if (!p->mOverflow && p->mCursor == p->mEnd) p->mOverflow = true;
    if (!p->mOverflow) *p->mCursor++ = value;
}
}
// Resource lifetime calls are fakes; table and buffer callees outside this batch.
int32_t TbResource::Release() { ev("Release"); return 0; }
void TbResource::AddRef() { ev("AddRef"); }
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}
int32_t TbITable::Release() { return 0; }
void TbITable::AddRef() {}
TbITable::~TbITable() {}
// The destructor (unit w40 translated the real one, which Closes: FormatString already did, so a fake that records nothing keeps the
// log the original's inlined destructor makes).
CStaticUnicodeBuffer::~CStaticUnicodeBuffer() {}
void CStaticUnicodeBuffer::Close() {
    ev("Close", {mCursor ? mCursor - start : -1, mEnd - start, mOverflow, mConverter != nullptr});
    if (mCursor) {
        if (!mOverflow && mCursor == mEnd) mOverflow = true;
        if (mOverflow) mCursor[-1] = 0;
        else *mCursor = 0;
        mCursor = nullptr;
    }
    if (mConverter) {
        auto* old = mConverter;
        mConverter = nullptr;
        old->Release();
    }
}
void CStaticUnicodeBuffer::AppendMB(const unsigned char* data, uint32_t size) {
    const auto text = Hex(data, size);
    ev("Append", {size}, {text.c_str()});
    for (uint32_t i = 0; i < size; ++i) Write(this, data[i]);
}
void CStaticUnicodeBuffer::AppendMB(uint16_t value) { Write(this, value); }
void CStaticUnicodeBuffer::AppendUnicode(const char16_t* p, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) Write(this, p[i]);
}
void CStaticUnicodeBuffer::AppendUnicode(char16_t value) { Write(this, value); }

extern "C" {
// Words describe magic/id pairs. A zero magic denotes a NULL pointer.
int32_t tpw_table_call(int32_t op, const unsigned char* code, uint32_t count, uint32_t id,
                       const uint32_t* words, uint32_t n, char16_t* out, uint32_t size,
                       int32_t hasConverter, int32_t* extra) {
    for (uint32_t i = 0; i < n; ++i) {
        tags[i].mMagic = words[i * 2];
        tags[i].mId = words[i * 2 + 1];
        arguments[i] = words[i * 2] ? &tags[i] : nullptr;
    }
    for (uint32_t i = n; i < arguments.size(); ++i) arguments[i] = nullptr;
    const unsigned char* strings[] = {code, code, code, code};
    table.mCount = count;
    table.mStrings = strings;
    table.mMBToUnicode = hasConverter ? &converter : nullptr;
    table.mUnicodeToMB = nullptr;
    start = out;
    *extra = 0;
    if (op == 0) {
        CStaticUnicodeBuffer buffer;
        buffer.mCursor = out;
        buffer.mEnd = out + size;
        buffer.mConverter = nullptr;
        buffer.mOverflow = false;
        return table.DoFormat(buffer, 0, reinterpret_cast<char*>(arguments.data()));
    }
    if (op == 1) {
        return table.FormatString(out, size, 0, arguments[0], arguments[1], arguments[2],
                                  arguments[3], arguments[4], arguments[5], arguments[6],
                                  arguments[7], static_cast<TbVTag*>(nullptr));
    }
    if (op == 2) {
        auto* scan = reinterpret_cast<char*>(arguments.data());
        auto* found = table.FindVariableTag(id, scan);
        *extra = static_cast<int32_t>((scan - reinterpret_cast<char*>(arguments.data())) / sizeof(void*));
        return found ? static_cast<int32_t>(static_cast<Tag*>(found) - tags.data()) : -1;
    }
    auto* found = table.GetConstString(id);
    return found ? static_cast<int32_t>(found - code) : -1;
}
// Converter inputs and outputs are independently guarded buffers.
int32_t tpw_unicode_call(int32_t op, uint16_t* data, char16_t value, const char16_t* source,
                         unsigned char* out, uint16_t size) {
    TbIUnicodeToMB unicode;
    unicode.mData = data;
    if (op == 0) return unicode.LocateValue(value);
    return unicode.ConvertString(out, source, size);
}
}
