// Unit v51015: base64enc 0x10202114, Email::Attachment::Attachment 0x10202670, Connection::Connection 0x10201a84,
// Connection::SendCommandLine 0x10201b14, CUIGrapher::DrawAxes 0x101fda4c.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "email.h"
#include "FS_Online.h"
#include "FinanceGraphPanelMsg.h"
#include "LbDrawPrimitives.h"

namespace {

// The alphabet at 0x102b16c4 (TOC slot -0x3afc).
const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// The texts at the TOC base 0x1024faeb: +0x14 "rb", +0x17 the file name, +0x24 the type.
const char kModeRead[] = "rb";
const char kFileName[] = "postcard.jpg";
const char kFileType[] = "image/jpeg";

}  // namespace

void base64enc(unsigned char* out, unsigned char* in, int count) {
    if (out == nullptr) return;
    if (in == nullptr) return;
    int column = 0;
    while (count > 0) {
        uint32_t word = static_cast<uint32_t>(*in++) << 8;
        if (count > 1) word |= *in++;
        word <<= 8;
        if (count > 2) word |= *in++;
        *out++ = static_cast<unsigned char>(kBase64Alphabet[word >> 18]);
        word &= 0x3ffff;
        *out++ = static_cast<unsigned char>(kBase64Alphabet[word >> 12]);
        word &= 0xfff;
        if (count > 1) {
            *out++ = static_cast<unsigned char>(kBase64Alphabet[word >> 6]);
            word &= 0x3f;
            if (count > 2)
                *out++ = static_cast<unsigned char>(kBase64Alphabet[word]);
            else
                *out++ = '=';
        } else {
            *out++ = '=';
            *out++ = '=';
        }
        column += 4;
        count -= 3;
        if (column == 0x4c) {
            *out++ = '\r';
            *out++ = '\n';
            column = 0;
        }
    }
    *out = 0;
}

namespace attach_callee {
__attribute__((weak)) void* Fopen(const char* path, const char* mode) { return std::fopen(path, mode); }
__attribute__((weak)) int32_t Fseek(void* file, int32_t offset, int32_t whence) {
    return std::fseek(static_cast<std::FILE*>(file), offset, whence);
}
__attribute__((weak)) int32_t Ftell(void* file) { return static_cast<int32_t>(std::ftell(static_cast<std::FILE*>(file))); }
__attribute__((weak)) void* NewArray(uint32_t size) { return ::operator new[](size); }
__attribute__((weak)) uint32_t Fread(void* buffer, uint32_t size, uint32_t count, void* file) {
    return static_cast<uint32_t>(std::fread(buffer, size, count, static_cast<std::FILE*>(file)));
}
__attribute__((weak)) void Fclose(void* file) { std::fclose(static_cast<std::FILE*>(file)); }
__attribute__((weak)) void DeleteArray(void* p) { ::operator delete[](p); }
}  // namespace attach_callee

Email::Attachment* Email::Attachment::Construct(Attachment* self, const char* path) {
    emailcb_callee::StringInit(&self->mString00);
    emailcb_callee::StringInit(&self->mString04);
    void* file = attach_callee::Fopen(path, kModeRead);
    if (file == nullptr) {
        self->mData.mOffset = 0;
        return self;
    }
    attach_callee::Fseek(file, 0, 2);
    int32_t size = attach_callee::Ftell(file);
    attach_callee::Fseek(file, 0, 0);
    uint8_t* block = static_cast<uint8_t*>(attach_callee::NewArray(static_cast<uint32_t>(size) + 1));
    if (block != nullptr) {
        attach_callee::Fread(block, 1, static_cast<uint32_t>(size), file);
        block[size] = 0;
    }
    attach_callee::Fclose(file);
    email_callee::StringAssign(&self->mString00, kFileName, static_cast<uint32_t>(std::strlen(kFileName)));
    email_callee::StringAssign(&self->mString04, kFileType, static_cast<uint32_t>(std::strlen(kFileType)));
    int32_t padded = size;
    while (padded % 3 != 0) padded++;
    int32_t chars = static_cast<int32_t>(static_cast<uint32_t>(padded) << 2) / 3;
    chars += (chars / 0x4c) * 2;
    uint8_t* text = static_cast<uint8_t*>(attach_callee::NewArray(static_cast<uint32_t>(chars) + 1));
    self->mData.set(text);
    if (self->mData.mOffset == 0) {
        attach_callee::DeleteArray(block);
        return self;
    }
    base64enc(self->mData.get(), block, size);
    if (block != nullptr) attach_callee::DeleteArray(block);
    return self;
}

Connection::Connection() {
    new (this) CSock();
    mF00 = 0x102b1610;
    char* bytes = reinterpret_cast<char*>(this);
    std::memset(bytes + 0x18, 0, 8);
    mLen = 0;
    std::memset(mBuf, 0, 0x400);
}

void Connection::SendCommandLine(char* line) {
    int32_t* total = reinterpret_cast<int32_t*>(reinterpret_cast<char*>(this) + 0x18);
    *total += static_cast<int32_t>(std::strlen(line));
    reinterpret_cast<CSock*>(this)->SendData(line, static_cast<int32_t>(std::strlen(line)));
}

void CUIGrapher::DrawAxes() {
    int32_t y = mF14 + 1;
    if (mF1C < 0) {
        int32_t span = mF24 - mF1C;
        if (span > 0) y = mF14 - (-mF1C * mF04) / span;
    }
    // The bytes are copied into a TbColour without running its constructor (not linked in every library).
    alignas(TbColour) uint8_t xb[sizeof(TbColour)];
    std::memcpy(xb, mXAxis, 5);
    LbDraw_HorizontalLine(mF08 - 1, y, mF10 + 1, *reinterpret_cast<TbColour*>(xb));
    alignas(TbColour) uint8_t yb[sizeof(TbColour)];
    std::memcpy(yb, mYAxis, 5);
    LbDraw_VerticalLine(mF08 - 1, mF0C - 1, mF14 + 1, *reinterpret_cast<TbColour*>(yb));
}
