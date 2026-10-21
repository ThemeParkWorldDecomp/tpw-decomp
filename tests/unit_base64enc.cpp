// Test library for base64enc, Email::Attachment::Attachment, Connection::Connection, Connection::SendCommandLine and CUIGrapher::DrawAxes
// (unit v51015; libtpw_unit_base64enc.dylib, tests/test_base64enc.py), main app:
//   src/game/online/Connection.cpp, src/game/online/EmailMessage.cpp, src/game/ui/UIGrapher.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: the string
// constructor and assign, the C library file calls, new[] and delete[], CSock::CSock, CSock::SendData and the two LbDraw lines.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "email.h"
#include "FS_Online.h"
#include "FinanceGraphPanelMsg.h"
#include "LbDrawPrimitives.h"

using namespace calllog;

namespace {

uint8_t* gObj = nullptr;     // the object under test: string fields are logged as offsets from it
void* gBlocks[16];
int gBlockCount = 0;

#define I(x) static_cast<int64_t>(x)

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (uint32_t i = 0; s[i] != 0; i++) h += (i + 1) * static_cast<uint8_t>(s[i]);
    return h;
}

int64_t BlockId(void* p) {
    for (int i = 0; i < gBlockCount; i++)
        if (gBlocks[i] == p) return i + 1;
    return 0;
}

int64_t ColourWord(TbColour c) {
    return (static_cast<uint32_t>(c.mAlpha) << 24) | (static_cast<uint32_t>(c.mRed) << 16) | (static_cast<uint32_t>(c.mGreen) << 8) | c.mBlue;
}

}  // namespace

// ---- the fakes
namespace emailcb_callee {
void StringInit(void* str) { ev("StringInit", {I(static_cast<uint8_t*>(str) - gObj)}); }
}  // namespace emailcb_callee

namespace email_callee {
void StringAssign(void* str, const char* text, uint32_t length) {
    ev("StringAssign", {I(static_cast<uint8_t*>(str) - gObj), I(length), Hash(text)});
}
}  // namespace email_callee

namespace attach_callee {
void* Fopen(const char* path, const char* mode) {
    ev("Fopen", {Hash(path), Hash(mode)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("Fopen")));
}
int32_t Fseek(void* file, int32_t offset, int32_t whence) {
    ev("Fseek", {I(static_cast<int32_t>(reinterpret_cast<uintptr_t>(file))), I(offset), I(whence)});
    return static_cast<int32_t>(ret("Fseek"));
}
int32_t Ftell(void* file) {
    ev("Ftell", {I(static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)))});
    return static_cast<int32_t>(ret("Ftell"));
}
void* NewArray(uint32_t size) {
    ev("NewArray", {I(size)});
    if (ret("NewArray") != 0) return nullptr;
    void* p = tpw::ArenaAllocZeroed(size != 0 ? size : 1);
    if (gBlockCount < 16) gBlocks[gBlockCount++] = p;
    return p;
}
uint32_t Fread(void* buffer, uint32_t size, uint32_t count, void* file) {
    ev("Fread", {I(size), I(count), I(static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)))});
    uint8_t* bytes = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < size * count; i++) bytes[i] = static_cast<uint8_t>(i * 37 + 11);
    return count;
}
void Fclose(void* file) { ev("Fclose", {I(static_cast<int32_t>(reinterpret_cast<uintptr_t>(file)))}); }
void DeleteArray(void* p) { ev("DeleteArray", {BlockId(p)}); }
}  // namespace attach_callee

CSock::CSock() { ev("CSockCtor", {lab(this)}); }
int32_t CSock::SendData(const char* data, int32_t count) {
    ev("SendData", {lab(this), I(count)}, {data != nullptr ? data : ""});
    return static_cast<int32_t>(ret("SendData"));
}

void LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    ev("HLine", {I(x), I(y), I(length), ColourWord(colour), I(colour.mIndex)});
}
void LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    ev("VLine", {I(x), I(y), I(length), ColourWord(colour), I(colour.mIndex)});
}

extern "C" {

uint64_t tpw_b64_addr(const char*) { return 0; }

uint64_t tpw_b64_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host address of an arena offset (Ptr32).
uint64_t tpw_b64_deref(uint32_t offset) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase + offset)); }

void tpw_b64_reset() {
    gBlockCount = 0;
    gObj = nullptr;
}

int32_t tpw_b64_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Email::Attachment, mData));
    out[n++] = static_cast<int64_t>(offsetof(Connection, mBuf));
    out[n++] = static_cast<int64_t>(offsetof(Connection, mLen));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF04));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF08));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF10));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF14));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mXAxis));
    out[n++] = static_cast<int64_t>(offsetof(CUIGrapher, mYAxis));
    return n;
}

void tpw_b64_base64enc(uint64_t out, uint64_t in, int32_t count) {
    base64enc(reinterpret_cast<unsigned char*>(out), reinterpret_cast<unsigned char*>(in), count);
}

void tpw_b64_attach(uint64_t object, uint64_t path) {
    gObj = reinterpret_cast<uint8_t*>(object);
    Email::Attachment::Construct(reinterpret_cast<Email::Attachment*>(object), reinterpret_cast<const char*>(path));
}

void tpw_b64_conn(uint64_t object) { new (reinterpret_cast<void*>(object)) Connection(); }

void tpw_b64_send(uint64_t object, uint64_t line) {
    reinterpret_cast<Connection*>(object)->SendCommandLine(reinterpret_cast<char*>(line));
}

void tpw_b64_axes(uint64_t object) { reinterpret_cast<CUIGrapher*>(object)->DrawAxes(); }

}  // extern "C"
